# ADR 0012 — Optional native FSR 4.1.2 Lite INT8 (Linux)

[XCDE] T-FSR4-LITE, 2026-11-04. Proposed for the native Linux build.

AMD FidelityFX SDK 2.4.0 (2026-10-28) supplies FSR Upscale 4.1.2 and a Vulkan
backend, but ships Windows DLLs. Its public API selects a model by hardware;
it does not offer a public Lite override. The user's RX 6900 XT normally selects
full INT8. The requested implementation explicitly uses the actual smaller
`ffxProvider_FSR4Lite_Int8` model, not FSR 1/3 renamed as FSR 4.

Use bbport's standalone native Vulkan implementation, pinned to
`0fc2921c45f8ff3900f6bfa43a299e9a4f532af4`, as an optional shared component.
Its two source files are GPL-2.0-or-later, with original copyright notices.
The adapter uses the same licence. Preserve LASRecomp's and Monado's original
MIT source notices and top-level licence. Dynamic loading is an implementation
boundary, not a claim that GPL obligations disappear. Distribution of a combined
FSR-enabled work requires satisfying applicable GPL terms. Packages that include the
component include its corresponding source (`third_party/fsr4_native/`) and licence.
No GPL-3.0 engine code is copied.

Use SDK tag v2.4.0, commit `7539ae8b6568ad29ed78a753404351daf3b5cca7`.
`Kits/FidelityFX/docs/license.md` explicitly lists the upscaler DLLs among its
MIT-licensed exceptions. Preserve the complete AMD notice. Prepare a local
copy with only Lite's provider eligible, capture its 12-dispatch graph using
Proton, and translate DXIL through pinned dxil-spirv. This is an unofficial
native port; AMD does not supply or support this Linux component. No Windows
DLL or Proton is required at runtime.

The preparation tool validates all captured dimensions against the native
replay's rules, writes both output tiers and both standard/ultra models, and
records hashes. Model assets and intermediate captures remain ignored in
`build/fsr4/`; they are not Nintendo data. Packaging explicitly opts in via
`--fsr4-assets`, verifies the manifest, and includes only prepared assets.

Core integration reuses camera jitter, scene depth, normalized motion vectors,
object velocity extraction and the existing HUD separation. It converts sRGB
input to linear floating point for the captured HDR graph and encodes output
back before HUD composition. Drain the recording worker before external Vulkan
commands. Protect the eight UBO slots with submission timeline values, defer
old contexts until their GPU submissions complete, and reset history after
camera cuts, missing frames, quality/size/mode changes. Missing features/assets
produce a diagnostic and an explicitly reported spatial fallback.

Limits: Vulkan features include mixed FP16 dot product, integer dot product,
16/8-bit operations, push descriptors and compute derivatives. Model output is
limited to 3840x2160, including ultrawide dimensions within that rectangle.
Upscaling only; this component does not implement FSR frame generation.
