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


# [XCDE] Graphics options (runtime/hle/graphics.c, graphics_options.h, prop_lod.h).
# T-GRAPHICS-SCALABILITY: f_70006b24e0 has just copied the complete 0x1220
# render configuration. Invalidate any previous per-view overlay backup.
# The final camera-setup boundary applies settings without changing the source.
def _graphics_copy(gen, addr):
    ins = gen.lift(addr)
    if ins.kind != 'op': raise ValueError('graphics copy requires a plain epilogue instruction')
    return ('{ extern void xcde_graphics_copy(uint64_t destination); '
            'xcde_graphics_copy(c->x[19]); } ' + ins.code)
PATCHES[0x70006b24f8] = Patch(0xa9417bfd, _graphics_copy, 'T-GRAPHICS-SCALABILITY',
                          'ldp x29,x30,[sp,#0x10] - apply live render-config overrides after copy')

# Camera-mode setup can restore bloom/AO/blur from the authored singleton AFTER
# the copy. Apply the request at the final per-view setup boundary as well.
def _graphics_finish(gen, addr):
    return _graphics_copy(gen, addr).replace('xcde_graphics_copy', 'xcde_graphics_finish')
PATCHES[0x70006b2918] = Patch(0xa9437bfd, _graphics_finish, 'T-GRAPHICS-SCALABILITY',
                          'ldp x29,x30,[sp,#0x30] - apply overrides after camera-mode setup')


# T-SHADOW-MAPS: size both native pool-layout calculations and all three
# cascade defaults. The independent 256px target remains authored.
def _shadow_pool_dimension(reg):
    def emit(gen, addr):
        return ('{ extern uint32_t xcde_shadow_map_size(void); '
                f'c->x[{reg}] = xcde_shadow_map_size(); }}')
    return emit
for _site,_word,_reg in ((0x70006a2234,0x321603e2,2),(0x70006a2238,0x321603e3,3)):
    PATCHES[_site]=Patch(_word,_shadow_pool_dimension(_reg),'T-SHADOW-MAPS','native cascade pool dimensions')
def _shadow_default(reg):
    def emit(gen,addr):
        ins=gen.lift(addr)
        if ins.kind!='op':raise ValueError('shadow defaults require a plain load')
        return (ins.code+' { extern uint32_t xcde_shadow_map_size(void); '
                f'c->v[{reg}]=(c->v[{reg}] & ~((__uint128_t)0xffffffffu<<32)) | ((__uint128_t)xcde_shadow_map_size()<<32); }}')
    return emit
for _site,_word,_reg in ((0x7000696a60,0xfd440100,0),(0x7000696a80,0xfd440901,1),(0x7000696aa0,0xfd440d01,1)):
    PATCHES[_site]=Patch(_word,_shadow_default(_reg),'T-SHADOW-MAPS','native cascade resolution default; preserve near/far floats')

# T-PROP-LOD: decoded prop buffer and its size from the selected 64-byte map
# stream record are available before the native prop constructor consumes it.
def _prop_lod_prepare(gen,addr):
    ins=gen.lift(addr)
    if ins.kind!='op':raise ValueError('prop LOD hook requires a plain load')
    return ('{ extern void xcde_prop_lod(uint64_t data,uint32_t size); '
            'xcde_prop_lod(c->x[22],LD32(c->x[8]+0x38)); } '+ins.code)
PATCHES[0x700067b77c]=Patch(0xb9403908,_prop_lod_prepare,'T-PROP-LOD','ldr w8,[x8,#0x38] - guarded prop stream preparation')


# Undo the prior frame's overlay even when the working configuration is reused.
def _graphics_begin(gen, addr):
    ins=gen.lift(addr)
    if ins.kind!='op': raise ValueError('graphics begin requires a plain prologue')
    return ('{ extern void xcde_graphics_begin(uint64_t destination); '
            'xcde_graphics_begin(c->x[0]); } '+ins.code)
PATCHES[0x70006b2720] = Patch(0xa9bc5ff8, _graphics_begin, 'T-GRAPHICS-SCALABILITY',
                           'restore previous overrides before native per-view camera setup')


# Reused/freed native configuration objects must not retain a prior view backup.
def _graphics_construct(gen, addr):
    ins=gen.lift(addr)
    if ins.kind!='op': raise ValueError('graphics constructor requires a plain prologue')
    return ('{ extern void xcde_graphics_forget(uint64_t destination); '
            'xcde_graphics_forget(c->x[0]); } '+ins.code)
def _graphics_destroy(gen, addr):
    return '{ extern void xcde_graphics_forget(uint64_t destination); xcde_graphics_forget(c->x[0]); } return;'
PATCHES[0x70006b1b00] = Patch(0xf81e0ff3, _graphics_construct, 'T-GRAPHICS-SCALABILITY',
                           'forget prior overlays on native configuration construction')
PATCHES[0x70006b24d0] = Patch(0xd65f03c0, _graphics_destroy, 'T-GRAPHICS-SCALABILITY',
                           'forget prior overlays on native configuration destruction')
