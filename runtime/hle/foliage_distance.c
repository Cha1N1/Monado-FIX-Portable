#include "foliage_distance.h"
#include "../grass_lod.h"
#include "../rt.h"
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>

/* The render-thread UI publishes a request; only the serial guest foliage pass
 * commits it. A whole pass uses one scale, including selection and materials. */
static _Atomic float requested_scale = XCDE_FOLIAGE_DEFAULT_SCALE;
static float active_scale = XCDE_FOLIAGE_DEFAULT_SCALE;
static bool refresh_pass;

float xcde_foliage_requested_scale(void) {
    return atomic_load_explicit(&requested_scale, memory_order_relaxed);
}

void xcde_foliage_set_preset(int preset) {
    atomic_store_explicit(&requested_scale, grass_lod_scale(preset), memory_order_relaxed);
}

void xcde_foliage_configure(int preset) {
    float scale = grass_lod_scale(preset);
    const char *e = getenv("XCDE_GRASS_DISTANCE");
    if (e) {
        char *end;
        float requested = strtof(e, &end);
        if (end != e && !*end && isfinite(requested)) {
            scale = requested < XCDE_FOLIAGE_MIN_SCALE ? XCDE_FOLIAGE_MIN_SCALE : requested > XCDE_FOLIAGE_MAX_SCALE
                ? XCDE_FOLIAGE_MAX_SCALE : requested;
        }
    }
    atomic_store_explicit(&requested_scale, scale, memory_order_relaxed);
}

void xcde_foliage_begin(void) {
    float next = xcde_foliage_requested_scale();
    refresh_pass = next != active_scale;
    if (refresh_pass) {
        if (getenv("XCDE_GRASS_TRACE"))
            fprintf(stderr, "[foliage-live] %g -> %g; refreshing selection and materials\n", active_scale, next);
        active_scale = next;
    }
}

bool xcde_foliage_refreshing(void) { return refresh_pass; }

float xcde_foliage_shading_distance(float near_r, float far_r) {
    float detail_scale = grass_lod_shading_scale(active_scale);
    float result = xcde_foliage_far_scaled(near_r, far_r, detail_scale);
    static int trace = -1, printed;
    if (trace < 0) trace = getenv("XCDE_GRASS_TRACE") != NULL;
    if (trace && printed < 12) {
        ++printed;
        fprintf(stderr, "[foliage-shading] near=%g far=%g -> %g visibility=%g detail=%g\n",
                near_r, far_r, result, active_scale, detail_scale);
    }
    return result;
}

static bool heap_range(uint64_t addr, uint64_t size) {
    return size <= HEAP_MAX && addr >= HEAP_BASE && addr - HEAP_BASE <= HEAP_MAX - size;
}

void xcde_foliage_refresh_selection(Cpu *c) {
    if (!refresh_pass || !heap_range(c->x[0], 0xc8)) return;
    uint64_t collection = c->x[0];
    uint64_t header = LD64(collection), bounds = LD64(collection + 0xc0);
    if (!heap_range(header, 4)) return;
    uint32_t count = LD32(header);
    if (!count || count > 64 || !heap_range(bounds, (uint64_t)count * 16)) return;
    /* These are the old camera X/Z values read by all three selectors. Force
     * their ordinary rebuild even with a stationary camera. Never clear pools
     * or touch instance/GPU lifetimes. The guest retires old patches normally. */
    float invalid = -1000000.f;
    uint32_t bits;
    memcpy(&bits, &invalid, 4);
    for (uint32_t i = 0; i < count; ++i) {
        ST32(bounds + i * 16, bits);
        ST32(bounds + i * 16 + 8, bits);
    }
    c->x[1] |= 1; // use the game's existing all-groups update path for this pass
}

/* Called before CPU selection and uniform upload; never changes game assets
 * or accumulates a multiplier in the material records. */
float xcde_foliage_distance(float near_r, float far_r) {
    float scale = active_scale;
    static int trace, printed_radius, printed_fade, printed_fixed;
    static bool traced;
    if (!traced) { trace = getenv("XCDE_GRASS_TRACE") != NULL; traced = true; }
    float result = xcde_foliage_far_scaled(near_r, far_r, scale);
    if (trace) {
        if (near_r == 0.f && far_r == 50.f && !printed_fixed) {
            printed_fixed = 1;
            fprintf(stderr, "[foliage-fixed-radius] %g -> %g scale=%g\n", far_r, result, scale);
        }
        int *printed = near_r == 0.f ? &printed_radius : &printed_fade;
        if (*printed < 12) {
            ++*printed;
            fprintf(stderr, "[foliage-distance] near=%g far=%g -> %g scale=%g\n",
                    near_r, far_r, result, scale);
        }
    }
    return result;
}

uint32_t xcde_foliage_pool_layout(int part) {
    const uint32_t metadata = XCDE_FOLIAGE_BITMAP_BYTES + XCDE_FOLIAGE_POOL_BYTES;
    const uint32_t scratch = metadata + XCDE_FOLIAGE_METADATA_BYTES;
    switch (part) {
    case 0: return scratch + 0x3a20u; // entire allocation
    case 1: return XCDE_FOLIAGE_BITMAP_BYTES;
    case 2: return scratch;
    case 3: return scratch + 0x1e00u; // distribution tables
    case 4: return metadata;
    case 5: return XCDE_FOLIAGE_POOL_BYTES;
    default: return 0;
    }
}

