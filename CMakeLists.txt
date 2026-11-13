# Monado: CMake + Ninja build of the same objects and executables as the shell scripts
# (scripts/build_rt_gfx.sh + build_boot.sh + link_gfx.sh + tools/launcher/build.sh on Windows,
#  scripts/build_linux.sh + tools/launcher/build_linux.sh on Linux), with the same compilers and flags.
#
#   tools/gen_all.py --out generated               # game code: first, and again after a new dump or patch change
#   cmake --preset windows    (or linux, linux-zig)
#   cmake --build --preset windows
#
# Ninja tracks header dependencies, compiles in parallel and rebuilds only what changed. The executables land where
# the launchers expect them: build/xcde.exe + build/xcde_launcher.exe (Windows), build/linux/monado +
# build/linux/monado-launcher (Linux). See docs/BUILD-CMAKE.md.
cmake_minimum_required(VERSION 3.21)
project(Monado LANGUAGES C CXX)

set(ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
option(MONADO_RUNTIME_ONLY "Compile the runtime only (no generated game code, no link)" OFF)
# Release packages (docs/BUILD-CMAKE.md): the executable holds no game code. It loads lasgame.so / lasgame.dll, which
# the launcher builds once from the player's own dump (tools/gamebuild.py); nothing derived from the game ships.
option(MONADO_RELEASE "Runtime without game code: loads the game library the launcher builds on first start" OFF)
if(CMAKE_C_COMPILER MATCHES "zig")
  set(MONADO_ZIG ON)
  set(MARCH -march=x86_64_v2)   # (zig's CPU model name)
else()
  set(MONADO_ZIG OFF)
  set(MARCH -march=x86-64-v2)
endif()
find_package(Python3 REQUIRED COMPONENTS Interpreter)

# ---- unit lists: the one place a new runtime file is added ----
set(RT_UNITS loader dispatch kernel ipc fnprof services/fs services/romfs_virtual services/am services/acc services/time
    services/hid_set services/kbicons services/misc services/audio services/hwopus hle/heaps hle/pl hle/vi hle/nvn_boot
    hle/fibers hle/input hle/party hle/bindings hle/movie hle/framepace hle/foliage_distance hle/foliage_pool hle/graphics
    host/stubs host/settings_ini host/stubs_gfx host/main_boot diag/diag diag/sampler diag/watchdog
    diag/crashdump paths)
set(NVN_UNITS astc cmdbuf formats maxwell/translate objects output_scale smaa_data tegra texpatch vulkan/device
    vulkan/exec vulkan/precompile vulkan/shaders vulkan/textures vulkan/temporal vulkan/vkq vulkan/fsr4)
set(IMGUI third_party/imgui)
set(UI_SOURCES ${IMGUI}/imgui.cpp ${IMGUI}/imgui_draw.cpp ${IMGUI}/imgui_tables.cpp ${IMGUI}/imgui_widgets.cpp
    ${IMGUI}/backends/imgui_impl_vulkan.cpp runtime/nvn/vulkan/imgui_glue.cpp runtime/ui/menu_vk.cpp)
set(LAUNCHER_SOURCES ${IMGUI}/imgui.cpp ${IMGUI}/imgui_draw.cpp ${IMGUI}/imgui_tables.cpp ${IMGUI}/imgui_widgets.cpp
    ${IMGUI}/backends/imgui_impl_sdl3.cpp ${IMGUI}/backends/imgui_impl_sdlrenderer3.cpp tools/launcher/launcher.cpp)
if(WIN32)
  list(APPEND RT_UNITS platform/windows)
else()
  list(APPEND RT_UNITS platform/linux)
endif()
set(RT_SOURCES)
if(MONADO_RELEASE)
  list(APPEND RT_UNITS gamelib)
  # GAMELIB_ID: the fingerprint of the generator, guest headers and guest flags (tools/game_id.py). The first-start
  # builder stamps the library with the same one; the runtime loads no other.
  execute_process(COMMAND "${Python3_EXECUTABLE}" tools/game_id.py WORKING_DIRECTORY "${ROOT}"
                  OUTPUT_VARIABLE GAMELIB_ID OUTPUT_STRIP_TRAILING_WHITESPACE RESULT_VARIABLE _gid)
  if(NOT _gid EQUAL 0 OR NOT GAMELIB_ID)
    message(FATAL_ERROR "tools/game_id.py failed")
  endif()
  message(STATUS "release runtime, GAMELIB_ID ${GAMELIB_ID}")
else()
  list(APPEND RT_UNITS host/gamelib_static)
  set(GAMELIB_ID dev)
endif()
foreach(u IN LISTS RT_UNITS)
  list(APPEND RT_SOURCES runtime/${u}.c)
endforeach()
foreach(u IN LISTS NVN_UNITS)
  list(APPEND RT_SOURCES runtime/nvn/${u}.c)
endforeach()

# ---- generated sources ----
set(GEN_INC "${CMAKE_BINARY_DIR}/inc")
add_custom_command(OUTPUT "${GEN_INC}/smaa_hlsl.inc"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${GEN_INC}"
  COMMAND "${Python3_EXECUTABLE}" tools/gen_smaa_inc.py third_party/smaa/SMAA.hlsl "${GEN_INC}/smaa_hlsl.inc"
  DEPENDS tools/gen_smaa_inc.py third_party/smaa/SMAA.hlsl WORKING_DIRECTORY "${ROOT}" VERBATIM)
set_source_files_properties(runtime/nvn/smaa_data.c PROPERTIES OBJECT_DEPENDS "${GEN_INC}/smaa_hlsl.inc")

if(NOT MONADO_RUNTIME_ONLY AND NOT MONADO_RELEASE)
  file(GLOB GAME_SOURCES CONFIGURE_DEPENDS "${ROOT}/generated/main/*.c" "${ROOT}/generated/sdk/*.c")
  if(NOT GAME_SOURCES OR NOT EXISTS "${ROOT}/generated/main/main_id.h")
    message(FATAL_ERROR "generated/ is empty: run tools/gen_all.py --out generated first (it needs your dump in targets/), "
                        "or configure with -DMONADO_RUNTIME_ONLY=ON")
  endif()
endif()

if(WIN32)
  # =========================== Windows (MSYS2 UCRT64 libraries, zig cc) ===========================
  set(MSYS2_PREFIX "C:/msys64/ucrt64" CACHE PATH "MSYS2 UCRT64 prefix with SDL3, Vulkan, shaderc, FFmpeg, Opus, zlib")
  set(OUT "${ROOT}/build")
  # scripts/build_rt_gfx.sh CF, and the C++ flags of its ImGui / DLSS / VMA units
  set(RT_C_FLAGS -O1 ${MARCH} -fno-strict-aliasing -fvisibility=hidden -w)
  set(RT_C_INCLUDES runtime runtime/nvn third_party "${MSYS2_PREFIX}/include")
  set(CXX_FLAGS -std=c++17 -O1 ${MARCH} -fvisibility=hidden -w "SHELL:-idirafter \"${MSYS2_PREFIX}/include\"")
  # scripts/build_boot.sh CF (the game code)
  set(GAME_C_FLAGS -O1 ${MARCH} -ffp-contract=off -fno-strict-aliasing -fvisibility=hidden -w)

  add_library(monado_rt OBJECT ${RT_SOURCES})
  target_compile_options(monado_rt PRIVATE ${RT_C_FLAGS})
  target_include_directories(monado_rt PRIVATE ${RT_C_INCLUDES})
  target_compile_definitions(monado_rt PRIVATE "GAMELIB_ID=\"${GAMELIB_ID}\"")
  set_source_files_properties(runtime/host/main_boot.c PROPERTIES COMPILE_DEFINITIONS XCDE_GFX)
  set_source_files_properties(runtime/nvn/smaa_data.c PROPERTIES INCLUDE_DIRECTORIES "${GEN_INC};${ROOT}/third_party/smaa/Textures")

  add_library(monado_ui OBJECT ${UI_SOURCES})
  target_compile_options(monado_ui PRIVATE ${CXX_FLAGS})
  target_include_directories(monado_ui PRIVATE runtime runtime/nvn runtime/nvn/vulkan ${IMGUI} ${IMGUI}/backends)
  add_library(monado_dlss OBJECT runtime/nvn/vulkan/dlss.cpp)
  target_compile_options(monado_dlss PRIVATE ${CXX_FLAGS})
  target_include_directories(monado_dlss PRIVATE runtime runtime/nvn third_party/streamline/include)
  add_library(monado_vma OBJECT runtime/nvn/vulkan/vma.cpp)
  target_compile_options(monado_vma PRIVATE ${CXX_FLAGS})

  set(GAME_LINK_LIBS)
  foreach(l SDL3.dll shaderc_shared.dll vulkan-1.dll opus.dll avcodec.dll avformat.dll avutil.dll swscale.dll
            swresample.dll z)
    list(APPEND GAME_LINK_LIBS "${MSYS2_PREFIX}/lib/lib${l}.a")
  endforeach()
  list(APPEND GAME_LINK_LIBS winmm dbghelp)
  set(GAME_LINK_OPTIONS ${MARCH} -Wl,--stack,268435456)
  if(MONADO_RELEASE)
    # The game library links against xcde.exe's exports (tools/gamebuild_stage.py --link-file xcde_imports.a). zig's
    # linker has no --export-all-symbols, so the runtime objects' globals are exported through a generated .drectve
    # (tools/coff_exports.py) and the import library is written from the linked executable (tools/pe_implib.py).
    set(RELEASE_EXPORTS "${CMAKE_BINARY_DIR}/release_exports.c")
    add_custom_command(OUTPUT "${RELEASE_EXPORTS}"
      COMMAND "${Python3_EXECUTABLE}" "${ROOT}/tools/coff_exports.py" --output "${RELEASE_EXPORTS}" --objects
              "$<TARGET_OBJECTS:monado_rt>" "$<TARGET_OBJECTS:monado_ui>" "$<TARGET_OBJECTS:monado_dlss>"
              "$<TARGET_OBJECTS:monado_vma>"
      DEPENDS monado_rt monado_ui monado_dlss monado_vma "${ROOT}/tools/coff_exports.py"
              "$<TARGET_OBJECTS:monado_rt>" "$<TARGET_OBJECTS:monado_ui>"
      COMMAND_EXPAND_LISTS VERBATIM)
  endif()
  set(GAME_NAME xcde)
  set(GAME_OBJECTS $<TARGET_OBJECTS:monado_rt> $<TARGET_OBJECTS:monado_ui> $<TARGET_OBJECTS:monado_dlss>
                   $<TARGET_OBJECTS:monado_vma>)
  if(MONADO_RELEASE)
    list(APPEND GAME_OBJECTS "${RELEASE_EXPORTS}")
  endif()

  # tools/launcher/build.sh
  set(LAUNCHER_NAME xcde_launcher)
  set(LAUNCHER_FLAGS -O2 ${MARCH} -std=c++17 -w "SHELL:-idirafter \"${MSYS2_PREFIX}/include\"")
  set(LAUNCHER_LIBS "${MSYS2_PREFIX}/lib/libSDL3.dll.a" "${MSYS2_PREFIX}/lib/libvulkan-1.dll.a")
  set(LAUNCHER_LINK_OPTIONS ${MARCH} -Wl,--subsystem,windows -static-libstdc++)

  # scripts/build_fsr4_windows.sh: the optional native FSR 4 Lite component, loaded at run time beside xcde.exe
  add_library(monado_fsr4 SHARED third_party/fsr4_native/fsr411.cpp third_party/fsr4_native/monado_adapter.cpp)
  set_target_properties(monado_fsr4 PROPERTIES OUTPUT_NAME monado_fsr4 PREFIX "" RUNTIME_OUTPUT_DIRECTORY "${OUT}"
                        LIBRARY_OUTPUT_DIRECTORY "${OUT}")
  target_compile_options(monado_fsr4 PRIVATE -O2 ${MARCH} -std=c++20 -fvisibility=hidden -w
                         "SHELL:-idirafter \"${MSYS2_PREFIX}/include\"")
  target_include_directories(monado_fsr4 PRIVATE runtime/nvn/vulkan)
  target_link_libraries(monado_fsr4 PRIVATE "${MSYS2_PREFIX}/lib/libvulkan-1.dll.a")
  # the prepared model files (tools/prepare_fsr4_lite.py) go where the runtime looks: build/fsr4/lite
  if(EXISTS "${ROOT}/build/fsr4/assets/lite")
    add_custom_command(TARGET monado_fsr4 POST_BUILD
      COMMAND "${CMAKE_COMMAND}" -E copy_directory "${ROOT}/build/fsr4/assets/lite" "${ROOT}/build/fsr4/lite" VERBATIM)
  endif()
else()
  # =========================== Linux (pkg-config libraries; system clang, or zig cc) ===========================
  set(OUT "${ROOT}/build/linux")
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(DEPS REQUIRED IMPORTED_TARGET sdl3 vulkan shaderc opus libavcodec libavformat libavutil libswscale
                    libswresample zlib)
  pkg_check_modules(LAUNCHER_DEPS REQUIRED IMPORTED_TARGET sdl3 vulkan)
  set(VMA_INCLUDE_DIR "$ENV{VMA_INCLUDE_DIR}" CACHE PATH "Directory with vk_mem_alloc.h, if it is not installed system-wide")
  # scripts/build_linux.sh flags (runtime and game code alike)
  set(LINUX_FLAGS -O1 ${MARCH} -ffp-contract=off -fno-strict-aliasing -D_GNU_SOURCE -w)
  if(NOT MONADO_RELEASE)  # a release runtime keeps default visibility: the game library resolves its symbols (-rdynamic)
    list(APPEND LINUX_FLAGS -fvisibility=hidden)
  endif()
  set(LINUX_INCLUDES runtime runtime/nvn third_party "${GEN_INC}")
  if(VMA_INCLUDE_DIR)
    list(APPEND LINUX_INCLUDES "${VMA_INCLUDE_DIR}")
  endif()
  set(GAME_C_FLAGS ${LINUX_FLAGS})

  add_library(monado_rt OBJECT ${RT_SOURCES})
  target_compile_options(monado_rt PRIVATE ${LINUX_FLAGS})
  target_include_directories(monado_rt PRIVATE ${LINUX_INCLUDES} third_party/smaa/Textures)
  target_compile_definitions(monado_rt PRIVATE "GAMELIB_ID=\"${GAMELIB_ID}\"" XCDE_GFX)
  target_link_libraries(monado_rt PRIVATE PkgConfig::DEPS)
  # [XCDE] T-RUNTIME-O2: the two Vulkan units that dominate the render thread's CPU time
  set_source_files_properties(runtime/nvn/vulkan/exec.c runtime/nvn/vulkan/textures.c PROPERTIES COMPILE_OPTIONS -O2)

  add_library(monado_ui OBJECT ${UI_SOURCES} runtime/nvn/vulkan/dlss.cpp runtime/nvn/vulkan/vma.cpp)
  target_compile_options(monado_ui PRIVATE ${LINUX_FLAGS} -std=c++17)
  target_include_directories(monado_ui PRIVATE ${LINUX_INCLUDES} ${IMGUI} ${IMGUI}/backends third_party/streamline/include
                             runtime/nvn/vulkan)
  target_compile_definitions(monado_ui PRIVATE "GAMELIB_ID=\"${GAMELIB_ID}\"")
  target_link_libraries(monado_ui PRIVATE PkgConfig::DEPS)

  set(GAME_LINK_LIBS PkgConfig::DEPS Threads::Threads ${CMAKE_DL_LIBS} m)
  find_package(Threads REQUIRED)
  set(GAME_LINK_OPTIONS -rdynamic)
  set(GAME_NAME monado)
  set(GAME_OBJECTS $<TARGET_OBJECTS:monado_rt> $<TARGET_OBJECTS:monado_ui>)

  # tools/launcher/build_linux.sh
  set(LAUNCHER_NAME monado-launcher)
  set(LAUNCHER_FLAGS -O2 ${MARCH} -std=c++17 -D_GNU_SOURCE -w)
  set(LAUNCHER_LIBS PkgConfig::LAUNCHER_DEPS ${CMAKE_DL_LIBS})
  set(LAUNCHER_LINK_OPTIONS)

  # [XCDE] T-LINUX-FSR3 / T-FSR4-LITE: the optional native upscaler components (their own scripts; a failure only
  # leaves that upscaler unavailable)
  option(MONADO_NATIVE_FSR "Build the optional native FSR 3 / FSR 4 Lite components" ON)
  if(MONADO_NATIVE_FSR)
    add_custom_target(native_fsr ALL
      COMMAND bash -c "scripts/build_fsr3_linux.sh || echo 'warning: native FSR 3 not built (see third_party/fsr3_native/README.md)' >&2"
      COMMAND bash -c "scripts/build_fsr4_linux.sh || echo 'warning: native FSR 4 Lite not built (see third_party/fsr4_native/README.md)' >&2"
      COMMAND bash -c "[[ ! -d build/fsr4/assets/lite || -e build/linux/fsr4 ]] || ln -s ../fsr4/assets build/linux/fsr4"
      WORKING_DIRECTORY "${ROOT}" USES_TERMINAL VERBATIM)
  endif()
endif()

if(MONADO_RUNTIME_ONLY)
  return()
endif()

# ---- the game code (scripts/build_boot.sh on Windows) and the game executable ----
if(MONADO_RELEASE)
  set(GAME_CODE)
else()
add_library(monado_game OBJECT ${GAME_SOURCES})
target_compile_options(monado_game PRIVATE ${GAME_C_FLAGS})
target_include_directories(monado_game PRIVATE runtime)
if(NOT WIN32)
  target_include_directories(monado_game PRIVATE ${LINUX_INCLUDES})
  target_compile_definitions(monado_game PRIVATE "GAMELIB_ID=\"${GAMELIB_ID}\"")
  target_link_libraries(monado_game PRIVATE PkgConfig::DEPS)
endif()

set(GAME_CODE $<TARGET_OBJECTS:monado_game>)
endif()
add_executable(monado_game_exe ${GAME_OBJECTS} ${GAME_CODE})
set_target_properties(monado_game_exe PROPERTIES OUTPUT_NAME ${GAME_NAME} LINKER_LANGUAGE CXX
                      RUNTIME_OUTPUT_DIRECTORY "${OUT}")
target_link_libraries(monado_game_exe PRIVATE ${GAME_LINK_LIBS})
target_link_options(monado_game_exe PRIVATE ${GAME_LINK_OPTIONS})
if(WIN32 AND MONADO_RELEASE)
  # zig's linker writes no import library: make one from xcde.exe's export table (tools/pe_implib.py)
  add_custom_command(TARGET monado_game_exe POST_BUILD
    COMMAND "${Python3_EXECUTABLE}" "${ROOT}/tools/pe_implib.py" "$<TARGET_FILE:monado_game_exe>"
            "${ROOT}/build/gamebuild/xcde_imports.a" --zig "${CMAKE_C_COMPILER}" VERBATIM)
endif()
if(WIN32)
  # scripts/copy_upscaler_dlls.sh: the AMD / NVIDIA runtime DLLs beside build/xcde.exe, if they were downloaded
  set(SDK_DIR "${ROOT}/sdk" CACHE PATH "Downloaded FidelityFX / Streamline SDKs (scripts/copy_upscaler_dlls.sh)")
  add_custom_command(TARGET monado_game_exe POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -DSDK_DIR=${SDK_DIR} -DOUT=${OUT} -P "${ROOT}/cmake/copy_upscaler_dlls.cmake" VERBATIM)
endif()

# ---- the launcher ----
add_executable(monado_launcher ${LAUNCHER_SOURCES})
set_target_properties(monado_launcher PROPERTIES OUTPUT_NAME ${LAUNCHER_NAME} RUNTIME_OUTPUT_DIRECTORY "${OUT}")
target_compile_options(monado_launcher PRIVATE ${LAUNCHER_FLAGS})
target_include_directories(monado_launcher PRIVATE ${IMGUI} ${IMGUI}/backends)
target_link_libraries(monado_launcher PRIVATE ${LAUNCHER_LIBS})
target_link_options(monado_launcher PRIVATE ${LAUNCHER_LINK_OPTIONS})
