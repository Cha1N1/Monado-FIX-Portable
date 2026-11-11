# GPU compatibility test matrix (Windows / Vulkan)

This matrix is a test plan, not a compatibility claim. A row becomes **verified** only after the executable has launched, rendered gameplay and cinematics, completed save/load, and passed the soak test on the named GPU + driver.

2026-11-04: an AMD integrated adapter passed procedural core-1.3/KHR-1.2 clear/readback smoke tests. No row below is promoted to gameplay-verified. See `docs/evidence/port_2026-11-04.md` and `docs/PORT_VALIDATION.md` for results, execution restrictions, the named GPU selector and open heap-windowing work.

## Required matrix

| Vendor / family | Representative target | Required checks | Status before hardware run |
|---|---|---|---|
| AMD Vega iGPU | Ryzen 5 Pro 5650U / Vega 7, 16 GB RAM | shared-memory reporting, 720p/50% scale, shader/pipeline cache, 60-minute soak | Pending |
| AMD Polaris | RX 580 / RX 570 | texture formats, BC compression, pipeline creation, frame pacing | Pending |
| AMD RDNA 1 | RX 5700 XT | compute, timeline semaphores, swapchain recreation | Pending |
| AMD RDNA 2 | RX 6600 / 6800 | FSR path, memory budget, device-loss diagnostics | Pending |
| AMD RDNA 3 | RX 7600 / 7800 XT + Ryzen 5 8600G iGPU (Phoenix1, 0x15BF) | GPL auto/off comparison (GPL **off = device lost**, do not ship), pipeline cache, 1-2 px colour-dot glitch: `amd_fix` feedback-snapshot path **fixes it** (tester-confirmed on RX 7800 XT and 8600G) | Fix confirmed via `amd_fix = auto` (RDNA 3 only); performance soak still pending |
| AMD RDNA 4 | RX 9070 series | driver feature discovery, regression suite | Pending |
| NVIDIA Maxwell | GTX 970 / 980 | robustness2 fallback, texture and shader correctness | Pending |
| NVIDIA Pascal | GTX 1060 / 1080 | memory pressure, cache persistence | Pending |
| NVIDIA Turing | RTX 2060 | DLSS optional path, fallback path | Pending |
| NVIDIA Ampere | RTX 3060 / 3080 | pipeline cache, frame pacing | Pending |
| NVIDIA Ada | RTX 4060 / 4080 | DLSS and swapchain proxy path | Pending |
| NVIDIA Blackwell | RTX 50 series | current driver and Vulkan feature discovery | Pending |
| Intel integrated | UHD 630 / newer UHD | host-visible memory classification, shader correctness | Pending |
| Intel Iris Xe | Xe-LP | integrated memory, pipeline cache, long session | Pending |
| Intel Arc | A380 / A750 | async compute, descriptor/pipeline stability | Pending |

## Per-device acceptance checklist

1. Save the complete `vulkaninfo --summary` output and Windows display-driver version.
2. Start with automatic adapter selection and the automatic GPU profile.
3. Confirm log lines for vendor/device IDs, Vulkan API, driver, queue family, memory heaps, optional features and enabled extensions.
4. Run once with `LARECOMP_VK_VALIDATION=1`; investigate every validation error (warnings must be triaged, not blindly ignored).
5. Reach gameplay; traverse a demanding area; open/close menus; play a cinematic with audio.
6. Save, exit normally, relaunch, and load the save.
7. Repeat with the compatibility profile and compare crashes, artifacts, frame pacing and memory use.
8. Run a 60-minute soak test and inspect logs for device loss, allocation failures, repeated shader compilation or unbounded resource growth.
9. Mark the row verified only with GPU model, driver version, OS build, test date and attached logs.

## Safe fallback controls

The in-game **GPU / Vulkan** tab persists these restart-scoped controls in `xcde.ini`:

- `gpu_index = -1`: automatic device selection; set a Vulkan enumeration index to request a specific adapter.
- `gpu_profile = auto|compatibility`: compatibility disables the optional graphics-pipeline-library fast path.
- `gpu_gpl = auto|off|on`: explicitly select graphics pipeline libraries (still gated by extension, feature and fast-link support).
- `gpu_memory_budget = 0|1`: use `VK_EXT_memory_budget` when exposed.
- `amd_fix = auto|on|off`: issue #7 RDNA 3 1-2 px colour dots — automatic engages the feedback-snapshot path only on AMD RDNA 3 (Navi 31/32/33 + Phoenix iGPUs); `on`/`off` force it anywhere. `LARECOMP_FEEDBACK_DIRECT` still overrides for diagnosis.

Environment overrides remain available for diagnosis: `LARECOMP_GPU`, `LARECOMP_VK_GPL`, `XCDE_VK_GPL`, `XCDE_VK_BUDGET`, and `LARECOMP_VK_VALIDATION`.
