"""XCDE instruction patches (contract: docs/contracts/patch-format.md)."""
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "tools", "lifter"))
from patchfmt import JumpTable, Patch  # noqa: E402,F401


def _hook_then_original(fn):
    """Call a runtime hook, then the original instruction (which must lift to plain C)."""
    def emit(gen, addr):
        ins = gen.lift(addr)
        if ins.kind != 'op':
            raise ValueError(f'{addr:#x}: hook patch needs a plain instruction, got {ins.kind}')
        return f'{{ extern void {fn}(void); {fn}(); }} {ins.code}'
    return emit


# T157: the game's frame limiter (nnMain+0x12150, f_70005bcfe0) busy-waits for the end of the 33.3 ms frame: a loop
# that reads the system tick through a vtable until enough time passed, no sleep (a dedicated core on the Switch). On
# PC that spin was ~0.3 of a core. Hooks: the loop head (sleep while the frame end is still far, runtime/hle/framepace.c)
# and the function's exit (a frame boundary: learns the frame interval).
PATCHES = {
    0x70005bd068: Patch(0x320003e0, _hook_then_original('xcde_limiter_spin'), 'T157', 'mov w0, #1 - limiter loop head'),
    0x70005bd168: Patch(0x6d4123e9, _hook_then_original('xcde_limiter_done'), 'T157', 'ldp d9, d8, [sp, #0x10] - limiter exit'),
}


# T210 (party mod session): frame-count thresholds at the game's own 30 fps above 30 fps (runtime/hle/party.c
# xcde_frames_gt: "more than n frames at 30 fps" -> the same time at the current frame rate). A field follower restarts
# its "follow while running" move once its movement component (+0x200) counted more than 9 frames of a condition; at
# 60 fps that happened every ~11 frames and the party fell behind walking (2 -> 17.5 m in 200 frames; 30 fps: a steady
# 6.5 m). Both checks of that counter (f_7000443760, f_70004439f0) are `cmp w8, #9`.
def _frames_gt(n):
    def emit(gen, addr):
        return ('{ extern uint32_t xcde_frames_gt(uint32_t n); '
                f'(void)(subs32(c, (uint32_t)c->x[8], xcde_frames_gt({n}))); }}')
    return emit


PATCHES[0x700044394c] = Patch(0x7100251f, _frames_gt(9), 'T210', 'cmp w8, #9 - follower move counter (f_7000443760)')
PATCHES[0x7000443b9c] = Patch(0x7100251f, _frames_gt(9), 'T210', 'cmp w8, #9 - follower move counter (f_70004439f0)')


# T222 (party mod session): save / load list thumbnails. f_700057da00 copies a slot's loaded .tmb picture (320x180 RGB)
# into the buffer it maps (bl memcpy at 0x700057dab4: x0 destination, x1 source, x2 size) - only one of the slot's two
# alternating picture buffers. The hook records which buffer holds the game's own picture (runtime/nvn/vulkan/exec.c
# slot_thumb_fix copies it into the twin): the twin is not always empty - in-game its memory held leftovers, shown
# every other frame as pink / green garbage.
def _thumb_loaded(gen, addr):
    return ('c->x[30] = 0x700057dab8ull; { extern void xcde_thumb_loaded(uint64_t d, uint64_t n); '
            'uint64_t d_ = c->x[0], n_ = c->x[2]; f_70008b6b48(c); xcde_thumb_loaded(d_, n_); }')


PATCHES[0x700057dab4] = Patch(0x940ce425, _thumb_loaded, 'T222', 'bl memcpy - slot thumbnail picture loaded (f_700057da00)')

# T262: Colony 9 affinity chart. The byte-offset switch crosses lifter partitions.
PATCHES[0x7000183f48] = JumpTable(
    '7e1df8e08d60544bbdca1e333c153c97', 0xd61f0140, 0x9d5324, 0x7000183f4c,
    (0x7000183f4c, 0x7000183f94, 0x7000183fb8, 0x7000183fdc, 0x7000183f70), 'T262',
)

# T265: the main perspective camera stores width / height before calculating both projection and culling.
def _scene_aspect(gen, addr):
    return ('{ extern uint32_t xcde_scene_aspect(uint32_t, uint32_t); '
            'ST32(c->x[19] + 0x4c0, xcde_scene_aspect((uint32_t)c->v[4], (uint32_t)c->x[20])); }')


PATCHES[0x70006f7ea0] = Patch(0xbd04c264, _scene_aspect, 'T265', 'store scene projection aspect')


# [XCDE] Follower footsteps above 30 FPS: keep the normal marker eligibility
# check, then reject repeated follower foot contacts across locomotion
# animation restarts (runtime/hle/party.c xcde_footstep_marker).
def _footstep_marker(gen, addr):
    return ('{ extern _Bool xcde_footstep_marker(Cpu *c, uint64_t action, uint64_t marker); '
            'if (!(c->x[0] & 1) || !xcde_footstep_marker(c, c->x[20], c->x[19])) '
            'goto L_70003f4c5c; }')


PATCHES[0x70003f4c0c] = Patch(0x36000280, _footstep_marker, 'T-FOOTSTEP-TIMING',
                              'tbz w0, #0 - follower foot marker replay guard')


# [XCDE] Menu hold/repeat timers above 30 FPS. f_7000578b50 advanced every pad
# hold/repeat timer by the fixed simulation step at UpdateInfo+4 (1/30 s), once
# per presented frame, so at 120 FPS a short tap repeated and held directions
# scrolled 4x too fast. UpdateInfo+8 already holds the elapsed seconds of the
# frame (+12 is elapsed authored frames). Use it only for these 27 timer loads;
# button edge detection, input polling and layout rendering stay intact.
def _menu_input_elapsed(gen, addr):
    return '{ uint64_t ea = c->x[20] + 8; c->v[1] = LD32(ea); }'


_MENU_HOLD_TIME_LOADS = (
    0x7000578ebc, 0x7000578eec, 0x7000578f18, 0x7000578f44,
    0x7000578f70, 0x7000578f9c, 0x7000578fc8, 0x7000578ff4,
    0x7000579020, 0x700057904c, 0x7000579078, 0x70005790a4,
    0x70005790d0, 0x70005790fc, 0x7000579128, 0x7000579154,
    0x7000579180, 0x70005791ac, 0x70005791d8, 0x7000579204,
    0x7000579230, 0x700057925c, 0x7000579288, 0x70005792b4,
    0x70005792e0, 0x7000579308, 0x70005793b4,
)
for _site in _MENU_HOLD_TIME_LOADS:
    PATCHES[_site] = Patch(0xbd400681, _menu_input_elapsed, 'T-MENU-INPUT',
                           'ldr s1, [x20, #4] -> elapsed seconds at +8 for menu hold timers')