#pragma once
#include <math.h>
#include <string.h>

enum { GRASS_LOW, GRASS_ORIGINAL, GRASS_HIGH, GRASS_VERY_HIGH, GRASS_ULTRA, GRASS_PRESET_COUNT };
#define GRASS_DEFAULT GRASS_HIGH

static const char *const grass_lod_labels[GRASS_PRESET_COUNT] = {
    "Low (0.5x)", "Original (1x)", "High (1.7x)", "Very High (2.2x)", "Ultra (3x)"
};
static const char *const grass_lod_labels_es[GRASS_PRESET_COUNT] = {
    "Baja (0.5x)", "Original (1x)", "Alta (1.7x)", "Muy alta (2.2x)", "Ultra (3x)"
};
static const char *const grass_lod_ini[GRASS_PRESET_COUNT] = {
    "low", "original", "high", "very_high", "ultra"
};
static inline int grass_lod_valid(int preset) {
    if (preset == 5) return GRASS_ULTRA; // retired 6x persisted index
    return preset >= 0 && preset < GRASS_PRESET_COUNT ? preset : GRASS_DEFAULT;
}
static inline int grass_lod_parse(const char *value) {
    if (!strcmp(value,"extreme")) return GRASS_ULTRA;
    for (int i = 0; i < GRASS_PRESET_COUNT; ++i)
        if (!strcmp(value, grass_lod_ini[i])) return i;
    return GRASS_DEFAULT;
}
static inline float grass_lod_scale(int preset) {
    static const float values[GRASS_PRESET_COUNT] = {.5f, 1.f, 1.7f, 2.2f, 3.f};
    return values[grass_lod_valid(preset)];
}
/* A legacy numeric diagnostic override can be outside the named presets. */
static inline int grass_lod_from_scale(float scale) {
    for (int i = 0; i < GRASS_PRESET_COUNT; ++i)
        if (fabsf(scale - grass_lod_scale(i)) < .00001f) return i;
    return -1;
}
/* Original keeps High's nearby shading without selecting more geometry.
 * Low reduces visibility while retaining the game's authored shading range. */
static inline float grass_lod_shading_scale(float visibility_scale) {
    int preset = grass_lod_from_scale(visibility_scale);
    if (preset == GRASS_ORIGINAL) return grass_lod_scale(GRASS_HIGH);
    if (preset == GRASS_LOW) return 1.f;
    return visibility_scale;
}
