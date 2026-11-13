// clang -Iruntime tests/native/test_foliage_distance.c runtime/hle/foliage_distance.c -lm
#include "../../runtime/hle/foliage_distance.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    assert(xcde_foliage_far_scaled(0, 50, .5f) == 25);
    assert(xcde_foliage_far_scaled(0, 80, .5f) == 40);
    assert(xcde_foliage_far_scaled(0, 30, .5f) == 15);
    assert(xcde_foliage_far_scaled(0, 20, .25f) == 20);
    assert(xcde_foliage_far_scaled(2, 20, 1.5f) == 29);
    assert(xcde_foliage_far_scaled(2, 80, 1.5f) == 119);
    assert(xcde_foliage_far_scaled(0, 80, 1.5f) == 120);
    assert(xcde_foliage_far_scaled(0, 80, XCDE_FOLIAGE_DEFAULT_SCALE) == 136);
    assert(xcde_foliage_far_scaled(0, 50, XCDE_FOLIAGE_DEFAULT_SCALE) == 85);
    assert(xcde_foliage_far_scaled(0, 30, XCDE_FOLIAGE_DEFAULT_SCALE) == 51);
    assert(fabsf(xcde_foliage_far_scaled(2, 80, XCDE_FOLIAGE_DEFAULT_SCALE) - 134.6f) < .001f);
    assert(xcde_foliage_far_scaled(0,500,3)==1500);
    assert(xcde_foliage_far_scaled(0,500,6)==500);
    assert(xcde_foliage_far_scaled(7000, 8000, 1.5f) == 8000);
    assert(xcde_foliage_far_scaled(0, 0, 1.5f) == 0);
    assert(xcde_foliage_far_scaled(80, 20, 1.5f) == 20);
    assert(xcde_foliage_far_scaled(-1, 20, 1.5f) == 20);
    assert(xcde_foliage_far_scaled(0, 20, NAN) == 20);
    assert(xcde_foliage_far_scaled(0, 20, 7) == 20);
    assert(isnan(xcde_foliage_far_scaled(0, NAN, 1.5f)));
    assert(isinf(xcde_foliage_far_scaled(0, INFINITY, 1.5f)));
    for (int i = 1; i <= 500; ++i) {
        float far = i;
        assert(xcde_foliage_far_scaled(0, far, .5f) == far * .5f);
        assert(xcde_foliage_far_scaled(0, far, 1) == far);
        float expected = far * 1.5f;
        assert(xcde_foliage_far_scaled(0, far, 1.5f) == expected);
        for (int scale = 1; scale <= 3; ++scale) {
            float radius = xcde_foliage_far_scaled(0, far, scale);
            assert(radius >= far && radius <= XCDE_FOLIAGE_MAX_RADIUS);
            /* Actual guest clear rounds bits down to bytes, then allocates
             * one more 32-byte block; include its five-layer worst case. */
            unsigned side = 2 * (unsigned)(radius / 5) + 1;
            unsigned clear_bytes = ((side * side * 5) / 256 + 1) * 32;
            assert(clear_bytes <= XCDE_FOLIAGE_BITMAP_BYTES);
            side = 2 * (unsigned)(radius / 5) + 1;
            assert(((side * side) / 256 + 1) * 32 <= XCDE_FOLIAGE_BITMAP_BYTES);
        }
    }
    assert(xcde_foliage_pool_layout(1) == XCDE_FOLIAGE_BITMAP_BYTES);
    assert(xcde_foliage_pool_layout(5) == XCDE_FOLIAGE_POOL_BYTES);
    assert(xcde_foliage_pool_layout(4) == xcde_foliage_pool_layout(1) + xcde_foliage_pool_layout(5));
    assert(xcde_foliage_pool_layout(2) == xcde_foliage_pool_layout(4) + XCDE_FOLIAGE_METADATA_BYTES);
    assert(xcde_foliage_pool_layout(3) == xcde_foliage_pool_layout(2) + 0x1e00);
    assert(xcde_foliage_pool_layout(0) == xcde_foliage_pool_layout(3) + 0x1c20);
    puts("foliage distance: range, fallback and reference checks passed");
}
