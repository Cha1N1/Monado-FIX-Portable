#!/usr/bin/env bash
# [XCDE] T-FSR4-LITE: optional GPL-2.0-or-later native component, no game dependencies.
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/linux
${CXX:-clang++} -O2 -std=c++20 -march=x86-64-v2 -fPIC -shared -fvisibility=hidden \
  -Iruntime/nvn/vulkan third_party/fsr4_native/fsr411.cpp third_party/fsr4_native/monado_adapter.cpp \
  -o build/linux/libmonado_fsr4.so $(pkg-config --cflags --libs vulkan)
