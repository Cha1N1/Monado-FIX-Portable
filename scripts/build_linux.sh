#!/usr/bin/env bash
# [XCDE] T-LINUX: native Linux x86-64 build of the same runtime and game code as the Windows build.
# Windows and Linux objects never mix (build/linux/). Needs clang, pkg-config and the development packages of
# SDL3, Vulkan, shaderc, opus, FFmpeg and zlib, plus the Vulkan Memory Allocator header (VMA_INCLUDE_DIR if it
# is not installed system-wide) and .venv with capstone unicorn lz4 (see docs/LINUX.md).
#   scripts/build_linux.sh                # dump in targets/ -> generated/ -> build/linux/monado
#   scripts/build_linux.sh --runtime-only # compile the runtime only (no game files needed)
#   scripts/build_linux.sh --check        # check the build dependencies
set -euo pipefail
cd "$(dirname "$0")/.."
[[ $(uname -s) == Linux && $(uname -m) == x86_64 ]] || { echo 'Linux x86-64 is required' >&2; exit 1; }
mode=${1:-build}
case "$mode" in build|--runtime-only|--check) ;; *) echo "Usage: $0 [--check|--runtime-only]" >&2; exit 2;; esac
CC=${CC:-clang}
CXX=${CXX:-clang++}
PYTHON=${PYTHON:-.venv/bin/python}
command -v "$CC" >/dev/null || { echo "missing $CC" >&2; exit 1; }
command -v "$CXX" >/dev/null || { echo "missing $CXX" >&2; exit 1; }
packages=(sdl3 vulkan shaderc opus libavcodec libavformat libavutil libswscale libswresample zlib)
pkg-config --print-errors --exists "${packages[@]}"
read -r -a includes <<< "$(pkg-config --cflags "${packages[@]}")"
# Same optimization, CPU baseline and FP/aliasing policy as scripts/build_rt_gfx.sh.
flags=(-O1 -march=x86-64-v2 -ffp-contract=off -fno-strict-aliasing -fvisibility=hidden -D_GNU_SOURCE
       -Iruntime -Iruntime/nvn -Ithird_party -Ibuild/inc "${includes[@]}" -DGAMELIB_ID='"dev"' -w)
[[ -z ${VMA_INCLUDE_DIR:-} ]] || flags+=("-I$VMA_INCLUDE_DIR")
printf '#if __has_include(<vma/vk_mem_alloc.h>)\n#include <vma/vk_mem_alloc.h>\n#else\n#include <vk_mem_alloc.h>\n#endif\n' \
  | "$CXX" "${flags[@]}" -x c++ -E - >/dev/null || { echo 'missing vk_mem_alloc.h (set VMA_INCLUDE_DIR)' >&2; exit 1; }
[[ $mode != --check ]] || { echo 'Linux build dependencies OK'; exit 0; }
if [[ $mode == build ]]; then
  [[ -x $PYTHON ]] || { echo "missing $PYTHON (python3 -m venv .venv && .venv/bin/pip install capstone unicorn lz4)" >&2; exit 1; }
  "$PYTHON" tools/extract/check_dump.py targets
fi
rtdir=build/linux/rt
# [XCDE] T-LINUX-FSR3 / T-FSR4-LITE: the optional native temporal upscalers, loaded at run time from build/linux/.
# XCDE_NATIVE_FSR=0 skips them; a failed component build only leaves that upscaler unavailable.
if [[ ${XCDE_NATIVE_FSR:-1} != 0 ]]; then
  scripts/build_fsr3_linux.sh || echo 'warning: native FSR 3 not built (needs git, glslangValidator; see third_party/fsr3_native/README.md)' >&2
  scripts/build_fsr4_linux.sh || echo 'warning: native FSR 4 Lite not built (see third_party/fsr4_native/README.md)' >&2
  # FSR 4 Lite model assets (tools/prepare_fsr4_lite.py, once) are looked up beside the executable
  [[ ! -d build/fsr4/assets/lite || -e build/linux/fsr4 ]] || ln -s ../fsr4/assets build/linux/fsr4
fi
mkdir -p "$rtdir" build/linux/game build/inc
python3 tools/gen_smaa_inc.py third_party/smaa/SMAA.hlsl build/inc/smaa_hlsl.inc
objects=()
# The units of scripts/build_rt_gfx.sh, with the Linux platform layer.
rt=(loader dispatch kernel ipc fnprof platform/linux services/fs services/romfs_virtual services/am services/acc services/time
    services/hid_set services/kbicons services/misc services/audio services/hwopus hle/heaps hle/pl hle/vi hle/nvn_boot
    hle/fibers hle/input hle/party hle/bindings hle/movie hle/framepace host/stubs host/settings_ini host/stubs_gfx
    host/gamelib_static host/main_boot diag/diag diag/sampler diag/watchdog diag/crashdump paths)
nvn=(astc cmdbuf formats maxwell/translate objects output_scale smaa_data tegra texpatch vulkan/device vulkan/exec
     vulkan/precompile vulkan/shaders vulkan/textures vulkan/temporal vulkan/fsr4)
for f in "${nvn[@]}"; do rt+=("nvn/$f"); done
for f in "${rt[@]}"; do
  obj="$rtdir/${f//\//_}.o"
  echo "CC $f"
  # [XCDE] T-RUNTIME-O2: the two Vulkan units that dominate the render thread's CPU time get -O2 (measured +5-9 %
  # FPS on RADV); everything else, the game code and the FP / aliasing policy stay as they are.
  unit_flags=()
  case "$f" in nvn/vulkan/exec|nvn/vulkan/textures) unit_flags=(-O2);; esac
  "$CC" "${flags[@]}" "${unit_flags[@]}" -DXCDE_GFX -Ithird_party/smaa/Textures -c "runtime/$f.c" -o "$obj"
  objects+=("$obj")
done
cpp=(third_party/imgui/imgui.cpp third_party/imgui/imgui_draw.cpp third_party/imgui/imgui_tables.cpp
     third_party/imgui/imgui_widgets.cpp third_party/imgui/backends/imgui_impl_vulkan.cpp runtime/nvn/vulkan/imgui_glue.cpp
     runtime/ui/menu_vk.cpp runtime/nvn/vulkan/dlss.cpp runtime/nvn/vulkan/vma.cpp)
for f in "${cpp[@]}"; do
  obj="$rtdir/${f//\//_}.o"
  echo "CXX $f"
  "$CXX" "${flags[@]}" -std=c++17 -Ithird_party/imgui -Ithird_party/imgui/backends -Ithird_party/streamline/include \
    -Iruntime/nvn/vulkan -c "$f" -o "$obj"
  objects+=("$obj")
done
[[ $mode != --runtime-only ]] || { echo 'Linux runtime compiled (game files not needed)'; exit 0; }
"$PYTHON" tools/gen_all.py --out generated
jobs=${JOBS:-$(nproc)}
[[ $jobs =~ ^[1-9][0-9]*$ ]] || { echo 'JOBS must be a positive integer' >&2; exit 2; }
pending=()
flush_jobs() {
  local failed=0 pid
  for pid in "${pending[@]}"; do wait "$pid" || failed=1; done
  pending=()
  [[ $failed == 0 ]] || { echo 'game code compilation failed' >&2; exit 1; }
}
trap 'for pid in "${pending[@]}"; do kill "$pid" 2>/dev/null || true; done; exit 130' INT TERM
for module in main sdk; do
  mkdir -p "build/linux/game/$module"
  shopt -s nullglob
  sources=(generated/"$module"/*.c)
  ((${#sources[@]})) || { echo "no generated $module sources" >&2; exit 1; }
  for f in "${sources[@]}"; do
    obj="build/linux/game/$module/$(basename "${f%.c}").o"
    if [[ -f $obj && $obj -nt $f ]]; then objects+=("$obj"); continue; fi  # (unchanged chunk)
    echo "GAME $f"
    ( tmp="$obj.$BASHPID.tmp"; trap 'rm -f "$tmp"' EXIT; "$CC" "${flags[@]}" -c "$f" -o "$tmp" && mv -f "$tmp" "$obj" ) &
    pending+=("$!")
    objects+=("$obj")
    ((${#pending[@]} < jobs)) || flush_jobs
  done
done
flush_jobs
trap - INT TERM
read -r -a libs <<< "$(pkg-config --libs "${packages[@]}")"
"$CXX" -o build/linux/monado "${objects[@]}" "${libs[@]}" -pthread -ldl -lm -rdynamic
tools/launcher/build_linux.sh
echo 'Built build/linux/monado and build/linux/monado-launcher'
