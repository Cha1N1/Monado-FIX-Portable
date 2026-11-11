// [XCDE] User settings: xcde.ini in the working directory, written by the launcher (tools/launcher) and read when the
// window opens. "key = value" lines; unknown keys are ignored, missing ones keep their defaults. Key bindings use the
// LAS format (bind_key_<action> / bind_pad_<action>, runtime/hle/bindings.c).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../input_bind.h"
#include "../settings.h"
#include "../quality_policy.h"
#include "ini_writer.h"

const char *const g_language_codes[N_LANGUAGES] = {"", "ja", "en-US", "fr", "de", "it", "es", "zh-CN", "ko", "nl", "pt", "ru", "zh-Hans", "zh-Hant"};
const char *const g_language_names[N_LANGUAGES] = {"Auto", "Japanese", "English", "French", "German", "Italian", "Spanish",
                                                   "Chinese", "Korean", "Dutch", "Portuguese", "Russian",
                                                   "Chinese (Simplified)", "Chinese (Traditional)"};
Settings g_settings = {.fps = 30, .fps_overlay = true, .upscaler_auto = true, .volume = 1.0f, .movie_audio = true, .res_mode = RES_MATCH, .upscaler = UPSCALE_FSR1, .upscale_quality = UQ_QUALITY, .game_blur = true, .vsync = true, .ui_lang = 1, .gpu_index = -1, .gpu_profile = GPU_PROFILE_AUTO, .gpu_gpl = GPU_GPL_AUTO, .occlusion = OCCL_AUTO, .gpu_memory_budget = true, .input_rumble = true};
int g_window_w = 1280, g_window_h = 720;  // windowed size (settings.h)

// [XCDE] Mods (runtime/services/romfs_virtual.c): xcde.ini `mod_<folder> = 0/1`; a folder not listed is on.
bool g_party_switch;  // [XCDE] party mod: xcde.ini party_switch = 0/1 (off by default; runtime/hle/party.c)
static struct { char name[96]; bool on; } g_mods[128];
static int g_nmods;
static void mod_set(const char *name, bool on) {
    for (int i = 0; i < g_nmods; i++) if (!strcmp(g_mods[i].name, name)) { g_mods[i].on = on; return; }
    if (g_nmods == 128) return;
    snprintf(g_mods[g_nmods].name, sizeof g_mods[g_nmods].name, "%s", name);
    g_mods[g_nmods++].on = on;
}
bool settings_mod_enabled(const char *name) {
    for (int i = 0; i < g_nmods; i++) if (!strcmp(g_mods[i].name, name)) return g_mods[i].on;
    return strncmp(name, "icons_", 6) != 0;  // [XCDE] button prompt packs (mods/icons_xbox...): only the one the launcher turns on
}
static char g_button_icons[16] = "auto";  // [XCDE] the launcher's choice (button_icons), kept when the game saves

static const char *ini_path(void) {
    const char *e = getenv("XCDE_INI");
    return e ? e : "xcde.ini";
}

static char *trim(char *s) {
    while (*s == ' ' || *s == '\t') s++;
    char *e = s + strlen(s);
    while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r' || e[-1] == '\n')) *--e = 0;
    return s;
}

static int lang_index(const char *v) {
    for (int i = 0; i < N_LANGUAGES; i++)
        if (!strcmp(v, i ? g_language_codes[i] : "auto")) return i;
    return LANG_AUTO;
}

void settings_load(void) {
    // test runs (hidden window, deterministic, scripted input) ignore the player's settings unless XCDE_INI names a file
    if (!getenv("XCDE_INI") && (getenv("LARECOMP_HIDDEN") || getenv("XCDE_DETERMINISTIC") || getenv("XCDE_INPUT_REPLAY"))) return;
    FILE *f = fopen(ini_path(), "r");
    if (!f) return;
    char line[512];
    while (fgets(line, sizeof line, f)) {
        char *s = line;
        if (strlen(s) >= 3 && !memcmp(s, "\xEF\xBB\xBF", 3)) s += 3;  // [XCDE] a UTF-8 BOM (Notepad) before the first key
        s = trim(s);
        if (!*s || *s == '#' || *s == ';' || *s == '[') continue;
        char *eq = strchr(s, '=');
        if (!eq) continue;
        *eq = 0;
        char *k = trim(s), *v = trim(eq + 1);
        if (!strcmp(k, "fullscreen")) g_settings.fullscreen = atoi(v) != 0;
        else if (!strcmp(k, "aspect")) g_settings.aspect = !strcmp(v, "original") ? ASPECT_ORIGINAL : ASPECT_UNLOCKED;
        else if (!strcmp(k, "window")) sscanf(v, "%dx%d", &g_window_w, &g_window_h);
        else if (!strcmp(k, "resolution")) {  // match | 1x..4x | 75% | 50% | 25% | dynamic
            if (!strcmp(v, "match")) g_settings.res_mode = RES_MATCH;
            else if (!strcmp(v, "dynamic")) g_settings.res_mode = RES_DYNAMIC;
            else if (!strcmp(v, "75%")) g_settings.res_mode = RES_Q75;
            else if (!strcmp(v, "50%")) g_settings.res_mode = RES_Q50;
            else if (!strcmp(v, "25%")) g_settings.res_mode = RES_Q25;
            else if (atoi(v) >= 1 && atoi(v) <= 8) g_settings.res_mode = atoi(v);
        } else if (!strcmp(k, "upscaler")) {
            g_settings.upscaler_auto = !strcmp(v, "auto");
            g_settings.upscaler = !strcmp(v, "fsr") ? UPSCALE_FSR1 : !strcmp(v, "fsr3") ? UPSCALE_FSR3
                                : !strcmp(v, "dlss") ? UPSCALE_DLSS : g_settings.upscaler_auto ? UPSCALE_FSR1 : UPSCALE_BILINEAR;
        } else if (!strcmp(k, "upscale_quality")) {
            static const char *q[] = {"native", "quality", "balanced", "performance", "ultra"};
            for (int i = 0; i < 5; i++) if (!strcmp(v, q[i])) g_settings.upscale_quality = i;
        } else if (!strcmp(k, "frame_gen")) g_settings.frame_gen = atoi(v) != 0;
        else if (!strcmp(k, "cutscenes")) g_settings.cutscene_mode = !strcmp(v, "30") ? 1 : !strcmp(v, "30fg") ? 2 : 0;
        else if (!strcmp(k, "gpu_index")) { int n = atoi(v); g_settings.gpu_index = n < -1 ? -1 : n > 255 ? 255 : n; }
        else if (!strcmp(k, "gpu_uuid")) snprintf(g_settings.gpu_uuid, sizeof g_settings.gpu_uuid, "%s", v);
        else if (!strcmp(k, "vulkan_api")) g_settings.vulkan_api = !strcmp(v, "1.2") ? 12 : !strcmp(v, "1.3") ? 13 : !strcmp(v, "1.4") ? 14 : 0;
        else if (!strcmp(k, "fps_overlay")) g_settings.fps_overlay = atoi(v) != 0;
        else if (!strcmp(k, "gpu_preset")) { g_settings.quality_preset = xcde_quality_parse(v); g_settings.gpu_performance = g_settings.quality_preset == QUALITY_PERFORMANCE || g_settings.quality_preset == QUALITY_POTATO; }
        else if (!strcmp(k, "low_memory")) g_settings.gpu_performance = atoi(v) != 0;
        else if (!strcmp(k, "dynamic_min_height")) g_settings.dynamic_min_height = atoi(v);
        else if (!strcmp(k, "dynamic_max_height")) g_settings.dynamic_max_height = atoi(v);
        else if (!strcmp(k, "effects_off")) g_settings.effects_off = (unsigned)strtoul(v, NULL, 10) & 31u;
        else if (!strcmp(k, "gpu_keep_heavy")) g_settings.gpu_keep_heavy = atoi(v) != 0;
        else if (!strcmp(k, "gpu_profile")) g_settings.gpu_profile = !strcmp(v, "compatibility") ? GPU_PROFILE_COMPATIBILITY : GPU_PROFILE_AUTO;
        else if (!strcmp(k, "gpu_gpl")) g_settings.gpu_gpl = !strcmp(v, "on") ? GPU_GPL_ON : !strcmp(v, "off") ? GPU_GPL_OFF : GPU_GPL_AUTO;
        else if (!strcmp(k, "occlusion")) g_settings.occlusion = !strcmp(v, "on") ? OCCL_ON : !strcmp(v, "off") ? OCCL_OFF : OCCL_AUTO;
        else if (!strcmp(k, "gpu_memory_budget")) g_settings.gpu_memory_budget = atoi(v) != 0;
        else if (!strcmp(k, "rumble")) g_settings.input_rumble = atoi(v) != 0;
        else if (!strcmp(k, "antialias")) g_settings.antialias = !strcmp(v, "smaa") ? AA_SMAA : AA_OFF;
        else if (!strcmp(k, "depth_of_field")) g_settings.dof_off = atoi(v) == 0;
        else if (!strcmp(k, "game_blur")) g_settings.game_blur = atoi(v) != 0;
        else if (!strcmp(k, "taa")) g_settings.taa_off = atoi(v) == 0;  // [XCDE] taa = 0 removes the game's temporal AA only
        else if (!strcmp(k, "vsync")) g_settings.vsync = atoi(v) != 0;
        else if (!strcmp(k, "launcher_lang")) g_settings.ui_lang = !strcmp(v, "es") ? 0 : 1;
        else if (!strcmp(k, "stick_deadzone")) { int d = atoi(v); g_settings.stick_deadzone = d < 0 ? 0 : d > 30 ? 30 : d; }
        else if (!strcmp(k, "party_switch")) g_party_switch = atoi(v) != 0;  // [XCDE] party mod (runtime/hle/party.c)
        else if (!strcmp(k, "volume")) g_settings.volume = (float)atof(v) / 100.0f;
        else if (!strcmp(k, "movie_audio")) g_settings.movie_audio = atoi(v) != 0;
        else if (!strcmp(k, "show_fps")) g_settings.show_fps = atoi(v) != 0;
        else if (!strcmp(k, "fps")) {
            static const char *names[] = {"30", "60", "120", "144", "165", "unlimited", "0", "unl\xc3\xadmited", "ilimitado"};
            static const int rates[] = {30, 60, 120, 144, 165, 0, 0, 0, 0};
            g_settings.fps = 30;
            for (unsigned i = 0; i < sizeof rates / sizeof rates[0]; ++i)
                if (!strcmp(v, names[i])) { g_settings.fps = rates[i]; break; }
        }
        else if (!strcmp(k, "language")) g_settings.language = lang_index(v);
        else if (!strncmp(k, "mod_", 4)) mod_set(k + 4, atoi(v) != 0);
        else if (!strcmp(k, "button_icons")) snprintf(g_button_icons, sizeof g_button_icons, "%s", v);
        else bind_settings_load(k, v);
    }
    fclose(f);
    if (g_window_w < 320) g_window_w = 320;
    if (g_window_h < 180) g_window_h = 180;
    if (g_settings.volume < 0) g_settings.volume = 0;
    if (g_settings.volume > 2) g_settings.volume = 2;
    if (g_settings.dynamic_min_height < 0) g_settings.dynamic_min_height = 0;
    if (g_settings.dynamic_max_height < 0) g_settings.dynamic_max_height = 0;
    if (g_settings.dynamic_min_height > 4320) g_settings.dynamic_min_height = 4320;
    if (g_settings.dynamic_max_height > 4320) g_settings.dynamic_max_height = 4320;
    if (g_settings.dynamic_max_height && g_settings.dynamic_min_height > g_settings.dynamic_max_height)
        g_settings.dynamic_min_height = g_settings.dynamic_max_height;
}

void settings_save(void) {
    char tmp[1200];
    FILE *f = xcde_ini_begin(ini_path(), tmp, sizeof tmp);
    if (!f) return;
    static const char *res[] = {"match", "1", "2", "3", "4", "5", "6", "7", "8", "dynamic", "75%", "25%", "50%"};
    fprintf(f, "# Monado settings (the launcher writes this file)\n");
    fprintf(f, "fullscreen = %d\nwindow = %dx%d\n", g_settings.fullscreen, g_window_w, g_window_h);
    fprintf(f, "aspect = %s\n", g_settings.aspect == ASPECT_ORIGINAL ? "original" : "auto");
    fprintf(f, "resolution = %s\n", g_settings.res_mode >= 0 && g_settings.res_mode <= 12 ? res[g_settings.res_mode] : "match");
    static const char *up[] = {"bilinear", "bilinear", "fsr", "fsr3", "dlss"};
    static const char *uq[] = {"native", "quality", "balanced", "performance", "ultra"};
    fprintf(f, "upscaler = %s\nantialias = %s\n", g_settings.upscaler_auto ? "auto" : up[g_settings.upscaler >= 0 && g_settings.upscaler <= 4 ? g_settings.upscaler : 0],
            g_settings.antialias == AA_SMAA ? "smaa" : "off");
    fprintf(f, "upscale_quality = %s\nframe_gen = %d\n",
            uq[g_settings.upscale_quality >= 0 && g_settings.upscale_quality <= 4 ? g_settings.upscale_quality : 1], g_settings.frame_gen);
    fprintf(f, "depth_of_field = %d\n", !g_settings.dof_off);
    fprintf(f, "game_blur = %d\n", g_settings.game_blur);
    fprintf(f, "taa = %d\n", !g_settings.taa_off);
    fprintf(f, "vsync = %d\n", g_settings.vsync);
    fprintf(f, "stick_deadzone = %d\n", g_settings.stick_deadzone);
    fprintf(f, "launcher_lang = %s\n", g_settings.ui_lang ? "en" : "es");  // (the launcher's key: kept when the game saves)
    fprintf(f, "cutscenes = %s\n", g_settings.cutscene_mode == 1 ? "30" : g_settings.cutscene_mode == 2 ? "30fg" : "same");
    fprintf(f, "gpu_index = %d\ngpu_profile = %s\ngpu_gpl = %s\nocclusion = %s\ngpu_memory_budget = %d\n",
            g_settings.gpu_index, g_settings.gpu_profile == GPU_PROFILE_COMPATIBILITY ? "compatibility" : "auto",
            g_settings.gpu_gpl == GPU_GPL_ON ? "on" : g_settings.gpu_gpl == GPU_GPL_OFF ? "off" : "auto",
            g_settings.occlusion == OCCL_ON ? "on" : g_settings.occlusion == OCCL_OFF ? "off" : "auto",
            g_settings.gpu_memory_budget);
    fprintf(f, "rumble = %d\n", g_settings.input_rumble);
    fprintf(f, "party_switch = %d\n", g_party_switch);
    fprintf(f, "vulkan_api = %s\nfps_overlay = %d\n", g_settings.vulkan_api == 12 ? "1.2" : g_settings.vulkan_api == 13 ? "1.3" : g_settings.vulkan_api == 14 ? "1.4" : "auto", g_settings.fps_overlay);
    fprintf(f, "gpu_uuid = %s\ngpu_preset = %s\ngpu_keep_heavy = %d\n", g_settings.gpu_uuid,
            xcde_quality_name(g_settings.quality_preset), g_settings.gpu_keep_heavy);
    fprintf(f, "low_memory = %d\ndynamic_min_height = %d\ndynamic_max_height = %d\neffects_off = %u\n",
            g_settings.gpu_performance, g_settings.dynamic_min_height, g_settings.dynamic_max_height, g_settings.effects_off);
    fprintf(f, "volume = %d\nmovie_audio = %d\nshow_fps = %d\n", (int)(g_settings.volume * 100 + 0.5f), g_settings.movie_audio,
            g_settings.show_fps);
    if (g_settings.fps) fprintf(f, "fps = %d\n", g_settings.fps); else fprintf(f, "fps = unlimited\n");
    fprintf(f, "language = %s\n", g_settings.language > 0 && g_settings.language < N_LANGUAGES ? g_language_codes[g_settings.language] : "auto");
    for (int i = 0; i < g_nmods; i++) fprintf(f, "mod_%s = %d\n", g_mods[i].name, g_mods[i].on);
    fprintf(f, "button_icons = %s\n", g_button_icons);
    bind_settings_save(f);
    if (!xcde_ini_commit(f, tmp, ini_path())) fprintf(stderr, "[settings] save failed; previous configuration kept\n");
}

// The language served to the game: the setting; "auto" stays English (what every test route was recorded with).
const char *settings_language(void) {
    if (g_settings.language > 0 && g_settings.language < N_LANGUAGES) return g_language_codes[g_settings.language];
    return "en-US";
}

const char *settings_language_name(void) {
    const char *c = settings_language();
    for (int i = 1; i < N_LANGUAGES; i++)
        if (!strcmp(c, g_language_codes[i])) return g_language_names[i];
    return "English";
}
