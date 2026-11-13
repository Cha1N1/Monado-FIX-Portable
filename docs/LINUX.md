# Native Linux build

The same runtime and recompiled game code as the Windows build, compiled natively for Linux x86-64
(no Wine). Windows and Linux objects never mix: everything Linux goes to `build/linux/`.

## Requirements

- x86-64-v2 CPU (SSE4.2, POPCNT, CMPXCHG16B), a Vulkan 1.2+ driver (Mesa RADV/ANV, NVIDIA proprietary).
- `clang`, `pkg-config` and the development packages of SDL3, Vulkan (headers + loader), shaderc, opus,
  FFmpeg (libavcodec, libavformat, libavutil, libswscale, libswresample) and zlib.
- The Vulkan Memory Allocator header `vk_mem_alloc.h` (system package, or `VMA_INCLUDE_DIR=<dir>`).
- Python 3 with `capstone unicorn lz4` for the recompiler:
  `python3 -m venv .venv && .venv/bin/pip install capstone unicorn lz4`.
- Your own dump in `targets/` (`exefs/`, `romfs/`), the same as for the Windows build (version 2.0.0).

Arch / CachyOS: `pacman -S clang sdl3 vulkan-headers vulkan-icd-loader shaderc opus ffmpeg zlib vulkan-memory-allocator`.

## Build and play

```bash
scripts/build_linux.sh --check        # dependencies only
scripts/build_linux.sh                # targets/ -> generated/ -> build/linux/monado
build/linux/monado targets            # from the repository root (xcde.ini, cache/, saves are relative to it)
```

`scripts/build_linux.sh --runtime-only` compiles the runtime without game files (CI / quick checks).
`JOBS=<n>` limits parallel game-code compilation; unchanged chunks are not recompiled.

## Differences from Windows

- DLSS (NVIDIA Streamline) and the FSR 3 DLL are Windows-only: the temporal upscalers fall back to the
  spatial ones. Everything else (FSR 1, SMAA, the F1 menu, mods, controls) is the same code.
- Logs go to the terminal or wherever stderr is redirected (the launcher: `build/last_game.log`); started from a
  desktop without either, to `~/.local/share/lasrecomp/lasrecomp.log`.
- The watchdog (`XCDE_STALL`, `XCDE_WATCHDOG`) samples one PC per guest thread instead of a full host stack.
