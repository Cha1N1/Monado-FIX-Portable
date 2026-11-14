#!/usr/bin/env bash
# [XCDE] T-FSR4-LITE: the optional native FSR 4.1.2 Lite component for Windows, build/monado_fsr4.dll (GPL-2.0-or-later,
# third_party/fsr4_native; no game dependencies). xcde.exe loads it at run time (LoadLibrary) with the prepared model
# files from build/fsr4/lite/. Without either, FSR 4 Lite is just unavailable and the other upscalers still work.
# The same zig c++ as the runtime; the MSYS2 headers come after zig's own C/C++ headers (-idirafter, as in
# build_rt_gfx.sh), and the Vulkan loader import library from MSYS2.
cd "$(dirname "$0")/.."
Z=".venv/Scripts/python -m ziglang c++"
M=${MSYS2_PREFIX:-/c/msys64/ucrt64}
mkdir -p build
$Z -O2 -std=c++20 -target x86_64-windows-gnu -mcpu=x86_64_v2 -shared -fvisibility=hidden -Iruntime/nvn/vulkan -idirafter "$M/include" -w \
  third_party/fsr4_native/fsr411.cpp third_party/fsr4_native/monado_adapter.cpp \
  -o build/monado_fsr4.dll "$M/lib/libvulkan-1.dll.a" || { echo "FAIL fsr4 (FSR 4 Lite will be unavailable)"; exit 1; }
# The model files: tools/prepare_fsr4_lite.py writes build/fsr4/assets/lite; the runtime reads <exe folder>/fsr4/lite.
if [ -d build/fsr4/assets/lite ]; then
  rm -rf build/fsr4/lite && cp -r build/fsr4/assets/lite build/fsr4/lite && echo "FSR 4 Lite model files ready in build/fsr4/lite"
elif [ ! -d build/fsr4/lite ]; then
  echo "note: no FSR 4 Lite model files in build/fsr4/lite (see third_party/fsr4_native/README.md); FSR 4 Lite stays unavailable"
fi
