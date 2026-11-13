# NOTICE

Monado is an independent project, not affiliated with or endorsed by Nintendo or Monolith Soft. *Xenoblade
Chronicles* is a trademark of Nintendo. This repository contains no game code, assets, keys, firmware or system
fonts (see `CONTRIBUTING.md`, Article I). Players must supply files extracted from their own copy.

## Derived work

| Component | Origin | License | Where |
|---|---|---|---|
| AArch64→C generator (`gen.py`, `lifter.py`, `simd_c.py`, `analyze.py`, `nso.py`, `sym.py`) | [LASRecomp](https://github.com/peiikai/LASRecomp) by Peiikai, commit `136f679` | MIT | `tools/lifter/` |
| Runtime (loader, dispatch, kernel, IPC, services, NVN→Vulkan, Maxwell shader translator) | LASRecomp, commit `136f679` | MIT | `runtime/` |

Every edit to a copied file is marked `[XCDE]` (the project's working name) and listed in `docs/UPSTREAM.md`.

## Reference only (no code copied)

| Project | License | Use |
|---|---|---|
| ssbu-recomp (Denkishi), commit `938e66b` | GPL-3.0 | design reference; copying code requires an ADR accepting GPL-3.0 for the whole project |
| B2recomp (K0NL) | GPL-3.0 | design reference |
| Other Switch recomps and PC ports reviewed for ideas (commits and issues) | various | listed in `docs/RECOMP_KNOWLEDGE.md`; no code copied |

## Third-party components in this repository (`third_party/`)

| Component | License |
|---|---|
| Dear ImGui | MIT (`third_party/imgui/LICENSE.txt`) |
| SMAA (reference shader and lookup textures) | MIT-style (`third_party/smaa/LICENSE.txt`) |
| AMD FidelityFX API headers (FSR 3.1 / frame generation) | MIT |
| NVIDIA Streamline headers (DLSS) | MIT (`third_party/streamline/license.txt`) |
| weeping-of-the-empire (daniel-madrid-07, fe2345e8) | MIT (`third_party/weeping-of-the-empire/LICENSE`); the ordered Vulkan recording queue is adapted in `runtime/nvn/vulkan/vkq.c` / `vkq.h` |
| bcdec (BC texture decoding) | MIT or public domain (Unlicense), at the user's choice |

## Linked at build time (not in this repository)

| Component | License |
|---|---|
| SDL3 | zlib |
| Vulkan headers / loader, Vulkan Memory Allocator | Apache-2.0 / MIT |
| shaderc (glslang, SPIRV-Tools) | Apache-2.0 |
| FFmpeg (LGPL build from MSYS2) | LGPL-2.1+ |
| libopus | BSD-3-Clause |
| Capstone, Unicorn, lz4 (build-time Python tools, not linked into the game) | BSD-3-Clause / GPL-2.0 / BSD-2-Clause |

## Optional vendor binaries (not redistributed; each user downloads them)

| Component | Use |
|---|---|
| AMD FidelityFX SDK 1.1.4 signed DLL (`amd_fidelityfx_vk.dll`) | FSR 3.1 upscaling and frame generation |
| NVIDIA Streamline 2.14.16 + DLSS DLLs | DLSS |
| Button icon packs from GameBanana (CompC: "Xbox control layout", "PS5 DualSense control layout", CC BY-NC-ND) | controller prompts; installed by the user into `mods/` |

`tools/install_dlss.py` retrieves Streamline 2.14.16 from NVIDIA-RTX's official GitHub release, verifies the pinned archive SHA-256 and installs production x64 binaries into the ignored `build/` directory. The release's licence files are retained in `sdk/sl/licenses/` and `build/dlss-licenses/`; source control contains no vendor binaries. The existing local packager preserves these files and the source/hash manifest when present.
