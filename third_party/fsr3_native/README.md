# Native Linux FSR 3.1.4

`adapter.cpp` implements the small FidelityFX API subset used by Monado's
`temporal.c`, using AMD's FSR 3.1.4 upscaler and Vulkan backend. It does not
implement frame generation, swapchain replacement or a general FidelityFX API.
The adapter is covered by the repository MIT licence. AMD SDK source, shaders
and the resulting component are subject to the preserved `LICENSE-AMD.txt`.

Build from the repository root:

```bash
scripts/build_fsr3_linux.sh
# Or supply an existing official SDK 1.1.4 checkout:
scripts/build_fsr3_linux.sh --sdk /absolute/path/to/sdk-checkout
```

Requires Python 3, Git, Clang++, Vulkan development headers/library and
`glslangValidator` with SPIR-V optimization support. The build tool downloads
AMD's official SDK at commit `c6efa6bf7f2027b3ec94f28578bb5965eabb9e55` when
needed. Derived SDK files and shader binaries stay under ignored `build/`.
No game content, Windows DLL, Proton, DXC or model weights are used.

The build prepares only Monado's linear LDR, normal-depth, unjittered
render-resolution motion-vector shader permutations, in FP32. The SDK's analytical Lanczos history filter is used on all GPUs; sharpening
variants are included. The LUT history permutation was removed after a synthetic
Native AA sequence reproduced severe stationary contrast loss. See
`tests/native/test_fsr3_temporal.cpp` and `docs/evidence/linux-fsr3/stationary-history.md`. Shader reflection is read directly from
glslang's SPIR-V descriptor names/types/bindings. `-Os` removes unused bindings
before reflection; these must match the SDK's resource tables.

Linux preparation adds bounded MSVC string helper equivalents, includes missing
standard headers, enlarges the opaque FSR context for 32-bit `wchar_t`, and aligns
the SDK's `alignas(32)` effect-context scratch array with sufficient padding.
These changes are generated without altering the pinned checkout. The unused
frame-generation configuration callback returns an unsupported error.

The runtime requires extended storage-image formats, formatless storage-image
writes and compute subgroup quad operations. It reuses the existing depth,
camera/game velocity, jitter and HUD separation. Input is converted from sRGB to
linear and output back to sRGB before HUD composition. Queue recording is drained
before external SDK calls; four SDK transient slots are protected by submission
completion. Size changes rebuild the context after GPU idle; camera cuts,
render-size changes and backend switches reset history.

`build/linux/libmonado_fsr3.so` loads beside the native executable or from the
library search path. `MONADO_FSR3_LIBRARY` overrides its path. Missing components,
unsupported features and SDK failures fall back to spatial FSR with a diagnostic.
The runtime/full build builds this component automatically. Standard AppImage
packaging always includes it together with FSR4 and the launcher.
`scripts/build_appimage_linux.sh` builds and packages the complete deliverable.

Gameplay quality and performance remain for user validation; a successful
synthetic dispatch is not evidence of full-game compatibility.
