# CMake + Ninja build

`CMakeLists.txt` builds the same objects and executables as the shell scripts:
- Windows: `build_rt_gfx.sh`, `build_boot.sh`, `link_gfx.sh` and `tools/launcher/build.sh`
- Linux: `scripts/build_linux.sh` and `tools/launcher/build_linux.sh`

It uses the same compilers and flags. The scripts still work. This is an alternative while it proves itself.

What it adds:
- **Header tracking:** Ninja reads the compiler's dependency files. A change to `runtime/cpu.h`, `simd.h` or `simd_fp.h` recompiles every game chunk that includes it, which the scripts' timestamp check misses.
- **Faster rebuilds:** everything compiles in parallel, and only what changed is rebuilt.
- **Fails early:** the build stops at the first compiler error and shows it.
- **One list per platform:** each runtime unit is listed once, in `RT_UNITS` / `NVN_UNITS`.
- **Editor support:** the presets write `compile_commands.json` for clangd and IDEs.

## Steps

The game code is generated first, as before:

```bash
python tools/gen_all.py --out generated     # Windows: .venv/Scripts/python; Linux: .venv/bin/python
```

CMake picks up the generated chunks by themselves, including chunks that appear or disappear after a generator change. Re-run `gen_all.py` after a new dump or patch change, then build.

### Windows (Git Bash or PowerShell)

`tools/setup_windows.ps1` installs `cmake` and `ninja` into `.venv` next to `ziglang`.

```bash
export PATH="$PWD/.venv/Scripts:/c/msys64/ucrt64/bin:$PATH"
cmake --preset windows
cmake --build --preset windows
```

- **Output:** `build/xcde.exe` and `build/xcde_launcher.exe`. The FSR 3 / DLSS runtime DLLs are copied from `sdk/` when they are there, like `copy_upscaler_dlls.sh`.
- **Compiler:** zig cc / zig c++ from the `ziglang` package in `.venv` (`cmake/zig.cmake`). `-DZIG_EXECUTABLE=<zig>` uses another one.
- **Libraries:** taken from `C:/msys64/ucrt64`. Change that with `-DMSYS2_PREFIX=<prefix>`.
- **Close the game before building.** Unlike `build_rt_gfx.sh`, this build does not stop a running `xcde.exe`, so linking over it fails while it runs.

### Linux

The dependencies are listed in `docs/LINUX.md`. Run with `VMA_INCLUDE_DIR` set if the Vulkan Memory Allocator header isn't installed system-wide.

```bash
cmake --preset linux        # system clang, like scripts/build_linux.sh
cmake --build --preset linux
```

- **Output:** `build/linux/monado` and `build/linux/monado-launcher`.
- **Upscaler components:** the optional native FSR 3 / FSR 4 Lite components are built through their own scripts. Turn that off with `-DMONADO_NATIVE_FSR=OFF`.
- **zig instead of clang:** `cmake --preset linux-zig` uses the same zig as Windows, from `.venv/bin/python -m pip install ziglang`.

### Runtime only

`-DMONADO_RUNTIME_ONLY=ON` compiles the runtime without generated game code, like `build_linux.sh --runtime-only`. It's meant for CI and quick checks.

## Adding a source file

Add the unit to `RT_UNITS` (under `runtime/`) or `NVN_UNITS` (under `runtime/nvn/`) in `CMakeLists.txt`. While the scripts still exist, add it to them as well.

## Build directories

Configure and build files live in `build/cmake/<preset>/`. Deleting that folder forces a clean configure; the executables in `build/` and `build/linux/` stay where they are.
