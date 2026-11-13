#!/usr/bin/env bash
# Optional native FSR 3.1.4, no game data or Windows tools required.
set -euo pipefail
cd "$(dirname "$0")/.."
exec python3 tools/build_fsr3_linux.py "$@"
