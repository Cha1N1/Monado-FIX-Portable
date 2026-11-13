# Native FSR 4.1.2 Lite component

[XCDE] T-FSR4-LITE. `fsr411.cpp` and `fsr411.h` are from
https://github.com/deadinside28/bloodborne_pc/tree/0fc2921c45f8ff3900f6bfa43a299e9a4f532af4/gpu/upscale/fsr411
(GPL-2.0-or-later). Local adaptation replaces the engine environment helpers with
standard `getenv`; `monado_adapter.cpp` is the Monado C interface (same licence).
The upstream namespace/file names are historical: this revision implements
4.1.2's real 12-dispatch Lite INT8 graph and 27,392-byte model weights.

Build in the repository: `scripts/build_fsr4_linux.sh` (Linux, `build/linux/libmonado_fsr4.so`) or
`scripts/build_fsr4_windows.sh` (Windows, zig c++ from `.venv`, `build/monado_fsr4.dll`; `scripts/build_all.sh` and
the CMake `windows` preset build it too).

The model files are prepared once with `tools/prepare_fsr4_lite.py`, which needs Linux and Proton. The Windows build
reads them from `build/fsr4/lite/` beside `xcde.exe`: copy the prepared `build/fsr4/assets/lite` folder there (the
Windows build scripts do this when `build/fsr4/assets/lite` exists). They come from AMD's SDK under the terms in
`LICENSE-AMD-SDK.md`, so `tools/make_portable.py` only packages them with `--with-fsr4-assets`.
Build from the flat source folder included in the AppImage (Vulkan headers,
loader and C++20 compiler required):

```
clang++ -O2 -std=c++20 -fPIC -shared -fvisibility=hidden -I. \
  fsr411.cpp monado_adapter.cpp -lvulkan -o libmonado_fsr4.so
```

The versioned interface header `fsr4_api.h` is included in the package alongside
this source. No game source, shader or asset is needed to build this component.
AMD shader/model assets are separate; their full SDK notice is preserved in
`LICENSE-AMD-SDK.md`. See ADR 0012 (`docs/adr/0012-native-fsr4-lite.md`) and `tools/prepare_fsr4_lite.py` for provenance
and asset preparation. Upstream benchmark logging still says "FSR 4.1.1";
Monado's `[fsr4]` log identifies the explicitly selected Lite 4.1.2 model.

Windows prefers the native mixed-dot feature when available. Otherwise it uses
`portable_dot.h` to expand the captured prepass
`OpFDot2MixAcc32VALVE` into FP32 conversion, dot and accumulation before module
creation. This removes the VALVE mixed-dot extension requirement; Lite does not
use compute derivatives. The INT8 graph/model assets are unchanged on disk.
Linux keeps the native shader path. Structural SPIR-V validation passes for all
48 Lite modules; Windows driver execution and floating-point image differences
remain subject to gameplay validation.
