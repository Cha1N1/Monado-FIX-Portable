// Headless boot executable: fixed default settings (the window build reads xcde.ini: settings_ini.c).
#include "../settings.h"

int g_window_w = 1280, g_window_h = 720;
const char *const g_language_codes[N_LANGUAGES] = {"", "ja", "en-US", "fr", "de", "it", "es", "zh-CN", "ko", "nl", "pt", "ru", "zh-Hans", "zh-Hant"};
const char *const g_language_names[N_LANGUAGES] = {"Auto", "Japanese", "English", "French", "German", "Italian", "Spanish",
                                                   "Chinese", "Korean", "Dutch", "Portuguese", "Russian",
                                                   "Chinese (Simplified)", "Chinese (Traditional)"};
Settings g_settings = {.fps_overlay = true, .upscaler_auto = true, .volume = 1.0f, .movie_audio = true, .gpu_index = -1, .gpu_profile = GPU_PROFILE_AUTO,
                       .gpu_gpl = GPU_GPL_AUTO, .occlusion = OCCL_AUTO, .amd_fix = AMD_FIX_AUTO, .session_fix = SESSION_FIX_AUTO, .gpu_memory_budget = true, .input_rumble = true};

void settings_load(void) {}
void settings_save(void) {}
const char *settings_language(void) { return "en-US"; }
const char *settings_language_name(void) { return "English"; }

