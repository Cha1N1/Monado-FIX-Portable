#!/usr/bin/env bash
# [XCDE] T-LINUX: build the launcher natively: build/linux/monado-launcher (SDL3 + Dear ImGui, SDL renderer;
# Vulkan for the GPU list).
# It starts build/linux/monado (scripts/build_linux.sh) from the repository root, like the Windows launcher.
set -euo pipefail
cd "$(dirname "$0")/../.."
CXX=${CXX:-clang++}
I=third_party/imgui
read -r -a sdl <<< "$(pkg-config --cflags --libs sdl3 vulkan)"
mkdir -p build/linux/launcher
objs=()
for f in $I/imgui.cpp $I/imgui_draw.cpp $I/imgui_tables.cpp $I/imgui_widgets.cpp $I/backends/imgui_impl_sdl3.cpp \
         $I/backends/imgui_impl_sdlrenderer3.cpp tools/launcher/launcher.cpp; do
  o=build/linux/launcher/$(basename "${f%.cpp}").o
  # shared runtime headers also affect the launcher: always rebuild its translation unit
  if [[ $f == tools/launcher/launcher.cpp || ! -f $o || $f -nt $o ]]; then
    "$CXX" -c "$f" -o "$o" -O2 -march=x86-64-v2 -std=c++17 -D_GNU_SOURCE -I$I -I$I/backends "${sdl[@]}" -w
  fi
  objs+=("$o")
done
"$CXX" -o build/linux/monado-launcher "${objs[@]}" "${sdl[@]}" -ldl
echo 'Built build/linux/monado-launcher'
