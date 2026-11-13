// [XCDE] T247/T248: exercise the runtime INI reader/writer together, not source text.
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../runtime/settings.h"
#include "../../runtime/quality_policy.h"
bool bind_settings_load(const char *key, const char *value) { (void)key; (void)value; return false; }
void bind_settings_save(FILE *f) { fputs("mouse_look = 1\n", f); }
int main(int argc, char **argv) {
    assert(argc == 2);
#ifdef _WIN32
    assert(!_putenv_s("XCDE_INI", argv[1]));
#else
    assert(!setenv("XCDE_INI", argv[1], 1));
#endif
    FILE *f = fopen(argv[1], "w"); assert(f);
    fputs("gpu_uuid = 0123456789abcdef0123456789abcdef\nresolution = 1.5\ngpu_index = 1\ngpu_profile = compatibility\n"
          "gpu_gpl = off\ngpu_memory_budget = 0\ngpu_preset = performance\ngpu_keep_heavy = 1\n"
          "occlusion = on\namd_fix = off\nsession_fix = off\n"
          "vulkan_api = 1.2\nfps_overlay = 0\nupscaler = auto\n"
          "stick_deadzone = 20\nfuture_plugin = preserve this\n", f); fclose(f);
    settings_load();
    assert(g_settings.res_mode == RES_1_5);
    assert(!strcmp(g_settings.gpu_uuid, "0123456789abcdef0123456789abcdef"));
    assert(g_settings.gpu_index == 1 && g_settings.gpu_profile == GPU_PROFILE_COMPATIBILITY);
    assert(g_settings.gpu_gpl == GPU_GPL_OFF && !g_settings.gpu_memory_budget);
    assert(g_settings.occlusion == OCCL_ON && g_settings.amd_fix == AMD_FIX_OFF && g_settings.session_fix == SESSION_FIX_OFF);
    assert(g_settings.gpu_performance && g_settings.gpu_keep_heavy);
    assert(g_settings.vulkan_api == 12 && !g_settings.fps_overlay && g_settings.upscaler_auto);
    xcde_quality_apply(&g_settings,QUALITY_POTATO); g_settings.effects_off=EFFECT_SSR;
    g_settings.stick_deadzone = 0; settings_save();
    g_settings.gpu_uuid[0] = 0; g_settings.gpu_index = -1; settings_load();
    assert(g_settings.gpu_uuid[0] && g_settings.gpu_index == 1 && g_settings.stick_deadzone == 0);
    assert(g_settings.vulkan_api == 12 && !g_settings.fps_overlay && !g_settings.upscaler_auto);
    assert(g_settings.quality_preset==QUALITY_POTATO && g_settings.gpu_performance);
    assert(g_settings.dynamic_min_height==360 && g_settings.dynamic_max_height==540 && g_settings.effects_off==EFFECT_SSR);
    char data[8192] = {0}; f = fopen(argv[1], "r"); assert(f);
    assert(fread(data, 1, sizeof data - 1, f)); fclose(f);
    assert(strstr(data, "future_plugin = preserve this"));
    g_settings.res_mode = RES_1_5; settings_save();
    g_settings.res_mode = RES_MATCH; settings_load();
    assert(g_settings.res_mode == RES_1_5);
    // [XCDE] RCAS strength: unset stays "default" (FSR 1 0.9 / FSR 3 0.3); an explicit value round-trips, 0 = off
    assert(strstr(data, "upscale_sharpness = default") && g_settings.upscale_sharpness < 0.0f);
    g_settings.upscaler = UPSCALE_FSR1; assert(xcde_sharpness(&g_settings) == 0.9f);
    g_settings.upscaler = UPSCALE_FSR3; assert(xcde_sharpness(&g_settings) == 0.3f);
    g_settings.upscale_sharpness = 0.0f; settings_save(); g_settings.upscale_sharpness = -1.0f; settings_load();
    assert(g_settings.upscale_sharpness == 0.0f && xcde_sharpness(&g_settings) == 0.0f);
    assert(!remove(argv[1])); puts("settings round-trip: stable GPU ID, policies, 1.5x, unknown keys and default reset OK"); return 0;
}
