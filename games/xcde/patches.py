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


# [XCDE] Grass draw distance presets (runtime/hle/foliage_distance.c, foliage_pool.c).
# T-FOLIAGE-DISTANCE: apply the preset's separate MPF visibility and shading
# ranges. Original retains High shading at 1x visibility; Low halves visibility.
# Only bounded authored local ranges change; the broad
# 7000/8000 fallback, terrain/tree/cliff LODs and instance placement stay intact.
def _foliage_fade(gen, addr):
    ins = gen.lift(addr)
    if ins.kind != 'op':
        raise ValueError('foliage fade patch needs a plain instruction')
    return ('{ extern float xcde_foliage_shading_distance(float, float); '
            'float near_, far_; uint32_t bits_; '
            'memcpy(&near_, &c->v[0], 4); memcpy(&far_, &c->v[1], 4); '
            'far_ = xcde_foliage_shading_distance(near_, far_); '
            'memcpy(&bits_, &far_, 4); c->v[1] = bits_; } ' + ins.code)


def _foliage_radius(gen, addr):
    ins = gen.lift(addr)
    if ins.kind != 'op':
        raise ValueError('foliage radius patch needs a plain instruction')
    return (ins.code + ' { extern float xcde_foliage_distance(float, float); '
            'float radius_; uint32_t bits_; memcpy(&radius_, &c->v[8], 4); '
            'radius_ = xcde_foliage_distance(0.f, radius_); '
            'memcpy(&bits_, &radius_, 4); c->v[8] = bits_; }')


PATCHES[0x7000655ab0] = Patch(0xbd0023e0, _foliage_fade, 'T-FOLIAGE-DISTANCE',
                          'str s0, [sp, #0x20] - extend MPF material far fade')
for _site in (0x7000655e80, 0x7000655f8c):
    PATCHES[_site] = Patch(0xbd400908, _foliage_radius, 'T-FOLIAGE-DISTANCE',
                          'ldr s8, [x8, #8] - extend MPF CPU patch-selection radius')

# The type-3 default selector discards that input and loads a fixed 50-unit
# radius for its 5-unit cells. Scale AFTER this replacement as well; otherwise
# increasing the upstream radius leaves this grass edge unchanged. The original
# load still executes and is scaled once, rather than multiplying its caller's
# already-expanded radius again.
PATCHES[0x7000659ca8] = Patch(0xbd4a71a8, _foliage_radius, 'T-FOLIAGE-DISTANCE',
                          'ldr s8, [x13, #0xa70] - extend fixed 50-unit foliage radius')


# Commit a live request once for the whole serial foliage pass. Reuse the
# game's all-groups rebuild and material-upload paths; never clear live pools.
PATCHES[0x700067ea80] = Patch(0xa9bc5ff8, _hook_then_original('xcde_foliage_begin'),
                          'T-FOLIAGE-LIVE', 'stp x24, x23, [sp, #-0x40]! - foliage pass boundary')


def _foliage_live_selection(gen, addr):
    ins = gen.lift(addr)
    if ins.kind != 'op':
        raise ValueError('foliage refresh patch needs a plain instruction')
    return ('{ extern void xcde_foliage_refresh_selection(Cpu *c); '
            'xcde_foliage_refresh_selection(c); } ' + ins.code)


def _foliage_live_materials(gen, addr):
    return ('{ extern void f_70006d8fd0(Cpu *c); '
            'extern _Bool xcde_foliage_refreshing(void); '
            'c->x[30] = 0x700067eae8ull; f_70006d8fd0(c); '
            'if (xcde_foliage_refreshing()) c->x[0] |= 1; }')


PATCHES[0x7000655d30] = Patch(0xd10243ff, _foliage_live_selection, 'T-FOLIAGE-LIVE',
                          'sub sp, sp, #0x90 - refresh cached stationary foliage selection')
PATCHES[0x700067eae4] = Patch(0x9401693b, _foliage_live_materials, 'T-FOLIAGE-LIVE',
                          'bl f_70006d8fd0 - request normal material upload after live change')


# T-FOLIAGE-STREAMING: high presets exhausted the authored 3.5 MiB patch pool.
# Provision the complete contiguous native allocation for supported 3x live coverage.
# Keep the bitmap, free-list pool, metadata and scratch/table offsets coherent;
# no live pool is resized, reset or freed outside the guest's normal lifetime.
def _foliage_pool(reg, part, base=False):
    def emit(gen, addr):
        ins = gen.lift(addr)
        if ins.kind != 'op': raise ValueError('foliage layout requires a plain instruction')
        return (ins.code + ' { extern uint32_t xcde_foliage_pool_layout(int part); '
                f'c->x[{reg}] = ' + ('c->x[0] + ' if base else '') +
                f'xcde_foliage_pool_layout({part}); }}')
    return emit
for _site, _word, _reg, _part, _base in (
    (0x700068751c, 0xb81a03a8, 1, 0, False),
    (0x7000687524, 0x91400808, 8, 1, True),
    (0x700068752c, 0x914e8808, 8, 2, True),
    (0x7000687538, 0x72a00748, 8, 3, False),
    (0x7000687544, 0x914e0809, 9, 4, True),
    (0x7000687580, 0x320d0bea, 10, 5, False),
):
    PATCHES[_site] = Patch(_word, _foliage_pool(_reg, _part, _base), 'T-FOLIAGE-STREAMING',
                           'coherent MPF bitmap/pool allocation for all live grass presets')


# The game's fixed map heap cannot hold 3x coverage. Own a bounded private
# guest-address arena and release it at the original MPF destructor boundary.
def _foliage_allocate(gen, addr):
    return ('{ extern uint64_t xcde_foliage_pool_alloc(uint64_t owner); '
            'c->x[30]=0x7000687524ull; c->x[0]=xcde_foliage_pool_alloc(c->x[19]); }')
def _foliage_release(gen, addr):
    return ('{ extern void xcde_foliage_pool_free(uint64_t address); '
            'c->x[30]=0x7000687718ull; xcde_foliage_pool_free(c->x[0]); }')
PATCHES[0x7000687520] = Patch(0x97fdbb78, _foliage_allocate, 'T-FOLIAGE-STREAMING',
                           'allocate MPF scratch/pool outside the fixed map heap')
PATCHES[0x7000687714] = Patch(0x97fdbb2b, _foliage_release, 'T-FOLIAGE-STREAMING',
                           'release MPF private storage on native destruction')
