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
    hle/fibers hle/input hle/party hle/bindings hle/movie hle/framepace hle/foliage_distance hle/foliage_pool
    host/stubs host/settings_ini host/stubs_gfx host/gamelib_static host/main_boot diag/diag diag/sampler diag/watchdog
    diag/crashdump paths)
set(NVN_UNITS astc cmdbuf formats maxwell/translate objects output_scale smaa_data tegra texpatch vulkan/device
    vulkan/exec vulkan/precompile vulkan/shaders vulkan/textures vulkan/temporal)
set(IMGUI third_party/imgui)
set(UI_SOURCES ${IMGUI}/imgui.cpp ${IMGUI}/imgui_draw.cpp ${IMGUI}/imgui_tables.cpp ${IMGUI}/imgui_widgets.cpp
    ${IMGUI}/backends/imgui_impl_vulkan.cpp runtime/nvn/vulkan/imgui_glue.cpp runtime/ui/menu_vk.cpp)
set(LAUNCHER_SOURCES ${IMGUI}/imgui.cpp ${IMGUI}/imgui_draw.cpp ${IMGUI}/imgui_tables.cpp ${IMGUI}/imgui_widgets.cpp
    ${IMGUI}/backends/imgui_impl_sdl3.cpp ${IMGUI}/backends/imgui_impl_sdlrenderer3.cpp tools/launcher/launcher.cpp)
if(WIN32)
  list(APPEND RT_UNITS platform/windows)
else()
  list(APPEND RT_UNITS platform/linux)
  list(APPEND NVN_UNITS vulkan/fsr4)
endif()
set(RT_SOURCES)
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

if(NOT MONADO_RUNTIME_ONLY)
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
  target_compile_definitions(monado_rt PRIVATE "GAMELIB_ID=\"dev\"")
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
  set(GAME_NAME xcde)
  set(GAME_OBJECTS $<TARGET_OBJECTS:monado_rt> $<TARGET_OBJECTS:monado_ui> $<TARGET_OBJECTS:monado_dlss>
                   $<TARGET_OBJECTS:monado_vma>)

  # tools/launcher/build.sh
  set(LAUNCHER_NAME xcde_launcher)
  set(LAUNCHER_FLAGS -O2 ${MARCH} -std=c++17 -w "SHELL:-idirafter \"${MSYS2_PREFIX}/include\"")
  set(LAUNCHER_LIBS "${MSYS2_PREFIX}/lib/libSDL3.dll.a" "${MSYS2_PREFIX}/lib/libvulkan-1.dll.a")
  set(LAUNCHER_LINK_OPTIONS ${MARCH} -Wl,--subsystem,windows -static-libstdc++)
else()
  # =========================== Linux (pkg-config libraries; system clang, or zig cc) ===========================
  set(OUT "${ROOT}/build/linux")
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(DEPS REQUIRED IMPORTED_TARGET sdl3 vulkan shaderc opus libavcodec libavformat libavutil libswscale
                    libswresample zlib)
  pkg_check_modules(LAUNCHER_DEPS REQUIRED IMPORTED_TARGET sdl3 vulkan)
  set(VMA_INCLUDE_DIR "$ENV{VMA_INCLUDE_DIR}" CACHE PATH "Directory with vk_mem_alloc.h, if it is not installed system-wide")
  # scripts/build_linux.sh flags (runtime and game code alike)
  set(LINUX_FLAGS -O1 ${MARCH} -ffp-contract=off -fno-strict-aliasing -fvisibility=hidden -D_GNU_SOURCE -w)
  set(LINUX_INCLUDES runtime runtime/nvn third_party "${GEN_INC}")
  if(VMA_INCLUDE_DIR)
    list(APPEND LINUX_INCLUDES "${VMA_INCLUDE_DIR}")
  endif()
  set(GAME_C_FLAGS ${LINUX_FLAGS})

  add_library(monado_rt OBJECT ${RT_SOURCES})
  target_compile_options(monado_rt PRIVATE ${LINUX_FLAGS})
  target_include_directories(monado_rt PRIVATE ${LINUX_INCLUDES} third_party/smaa/Textures)
  target_compile_definitions(monado_rt PRIVATE "GAMELIB_ID=\"dev\"" XCDE_GFX)
  target_link_libraries(monado_rt PRIVATE PkgConfig::DEPS)
  # [XCDE] T-RUNTIME-O2: the two Vulkan units that dominate the render thread's CPU time
  set_source_files_properties(runtime/nvn/vulkan/exec.c runtime/nvn/vulkan/textures.c PROPERTIES COMPILE_OPTIONS -O2)

  add_library(monado_ui OBJECT ${UI_SOURCES} runtime/nvn/vulkan/dlss.cpp runtime/nvn/vulkan/vma.cpp)
  target_compile_options(monado_ui PRIVATE ${LINUX_FLAGS} -std=c++17)
  target_include_directories(monado_ui PRIVATE ${LINUX_INCLUDES} ${IMGUI} ${IMGUI}/backends third_party/streamline/include
                             runtime/nvn/vulkan)
  target_compile_definitions(monado_ui PRIVATE "GAMELIB_ID=\"dev\"")
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
add_library(monado_game OBJECT ${GAME_SOURCES})
target_compile_options(monado_game PRIVATE ${GAME_C_FLAGS})
target_include_directories(monado_game PRIVATE runtime)
if(NOT WIN32)
  target_include_directories(monado_game PRIVATE ${LINUX_INCLUDES})
  target_compile_definitions(monado_game PRIVATE "GAMELIB_ID=\"dev\"")
  target_link_libraries(monado_game PRIVATE PkgConfig::DEPS)
endif()

add_executable(monado_game_exe ${GAME_OBJECTS} $<TARGET_OBJECTS:monado_game>)
set_target_properties(monado_game_exe PROPERTIES OUTPUT_NAME ${GAME_NAME} LINKER_LANGUAGE CXX
                      RUNTIME_OUTPUT_DIRECTORY "${OUT}")
target_link_libraries(monado_game_exe PRIVATE ${GAME_LINK_LIBS})
target_link_options(monado_game_exe PRIVATE ${GAME_LINK_OPTIONS})
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
