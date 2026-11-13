#!/usr/bin/env bash
# [XCDE] T-LINUX-APPIMAGE: the whole Linux build (scripts/build_linux.sh: game code from your dump in targets/, runtime,
# launcher, optional native FSR 3 / FSR 4 Lite) and its AppImage, build/Monado-x86_64.AppImage. For your own use only:
# it contains the game code recompiled from your dump. Arguments go to tools/make_appimage.py.
set -euo pipefail
cd "$(dirname "$0")/.."
scripts/build_linux.sh
python3 tools/make_appimage.py "$@"
