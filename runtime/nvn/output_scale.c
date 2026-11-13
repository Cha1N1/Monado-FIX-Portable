// Output resolution, shared by the Metal and Vulkan backends (each calls output_scale() once per presented frame).
// Render targets the game sizes for its own frame (output_scale_set_base) are allocated at (sx, sy) times their size;
// the image is then drawn into (vx, vy, vw, vh) of the window. Video tab "Internal resolution" (g_settings.res_mode):
//   Match display  render at the window's own pixel size (default)
//   1x .. 8x       N times the game's frame (720 lines x N for XCDE; wide windows keep their shape: 32:9 at 2x =
//                  5120 x 1440); capped to the GPU's 16384-pixel texture limit
//   1.5x           1.5 times the game's frame: 1920 x 1080 for XCDE
//   .75x / .50x / .25x  of Match display (scaled up to the window - with MetalFX on macOS when chosen)
//   Dynamic        Match display, lowered in steps while the GPU can't hold 60 fps and raised again when it can
//                  (each change reallocates the render targets: a short hitch, so it changes rarely, with hysteresis)
// Test overrides: LARECOMP_SCALE=<f> (render height = the game's frame height x f), LARECOMP_WIDE_TEST=<f> (forced shape).
#include <math.h>
#include <stdlib.h>

#include "../hle.h"
#include "../settings.h"
#include "output_scale.h"

#define MAX_TEX 16384.0

// Dynamic resolution: a factor on the Match display scale, 0.5 .. 1 in 0.05 steps.
static double g_dyn = 1.0;
static double g_dyn_gpu_sum, g_dyn_dt_sum, g_last_t;
static int g_dyn_n, g_dyn_gpu_n, g_dyn_good;
static double g_dyn_cool_until, g_dyn_probe_wait = 5.0, g_dyn_probe_at;
static double g_dyn_last_raise = -100;

static void dyn_update(double now, double gpu_ms, double floor) {
    double dt = g_last_t > 0 ? (now - g_last_t) * 1000.0 : 16.7;
    g_last_t = now;
    if (dt > 250 || gpu_ms < -1.5) {
        g_dyn_dt_sum=g_dyn_gpu_sum=0; g_dyn_n=g_dyn_gpu_n=g_dyn_good=0;
        return; // loading/compilation is not GPU steady state
    }
    g_dyn_dt_sum += dt;
    g_dyn_n++;
    if (gpu_ms >= 0) { g_dyn_gpu_sum += gpu_ms; g_dyn_gpu_n++; }
    if (g_dyn_dt_sum < 500.0) return;  // T251: half a second at any render frequency
    double avg_dt = g_dyn_dt_sum / g_dyn_n, avg_gpu = g_dyn_gpu_n ? g_dyn_gpu_sum / g_dyn_gpu_n : -1;
    g_dyn_dt_sum = g_dyn_gpu_sum = 0;
    g_dyn_n = g_dyn_gpu_n = 0;
    if (now < g_dyn_cool_until) return;
    double prev = g_dyn;
    int target_fps = g_settings.fps;
#ifdef _WIN32
    extern int xcde_present_fps(void);
    target_fps = xcde_present_fps(); // T251: a 30 Hz cinematic must not be treated as a missed 144 Hz target
#endif
    double target_ms = 1000.0 / (target_fps > 0 ? target_fps : 60);
    // T260: don't lower image quality for a CPU/present/compile bottleneck when GPU time is known.
    bool slow = avg_gpu >= 0 ? avg_gpu > target_ms * 0.87 : avg_dt > target_ms * 1.044;
    if (slow) {
        g_dyn = fmax(floor, g_dyn - ((avg_gpu >= 0 ? avg_gpu : avg_dt) > target_ms * 1.32 ? 0.1 : 0.05));
        g_dyn_good = 0;
        if (now - g_dyn_last_raise < 3.0) g_dyn_probe_wait = fmin(g_dyn_probe_wait * 2, 60.0);  // raised too eagerly
    } else if (g_dyn < 1.0) {
        g_dyn_good++;
        // with GPU timing: raise when there is clear headroom; without (Vulkan): probe upwards now and then
        bool room = avg_gpu >= 0 ? avg_gpu < target_ms * 0.63 && g_dyn_good >= 4 : now >= g_dyn_probe_at && g_dyn_good >= 2 * (int)g_dyn_probe_wait;
        if (room) { g_dyn = fmin(1.0, g_dyn + 0.05); g_dyn_good = 0; g_dyn_last_raise = now; g_dyn_probe_at = now + g_dyn_probe_wait; }
    }
    g_dyn = fmax(floor, round(g_dyn * 20) / 20);
    if (g_dyn != prev) {
        g_dyn_cool_until = now + 1.5;
        LOG("[dynres] %.2f -> %.2f (frame %.1f ms, gpu %.1f ms)\n", prev, g_dyn, avg_dt, avg_gpu);
    }
}

double output_scale_dynamic_factor(void) { return g_dyn; }

// [XCDE] T146: the size the game renders its frame for. LASRecomp's game drew for the Switch's 1920x1080; XCDE's window
// textures are 1280x720, so with 1080 as the base a 720p window rendered everything (scene and HUD) at 853x480 and
// scaled it up. The NVN window's texture size sets it (nvnWindowInitialize); 1920x1080 until then.
static double g_base_w = 1920.0, g_base_h = 1080.0;
void output_scale_set_base(int w, int h) {
    if (w <= 0 || h <= 0 || (g_base_w == w && g_base_h == h)) return;
    g_base_w = w, g_base_h = h;
    LOG("[scale] the game renders for %dx%d\n", w, h);
}

void output_scale(double dw, double dh, bool reshape_allowed, double now, double gpu_ms, OutScale *o) {
    double fit = fmin(dw / g_base_w, dh / g_base_h);
    static double forced = -1, wide_test = -1;
    if (forced < 0) forced = getenv("LARECOMP_SCALE") ? atof(getenv("LARECOMP_SCALE")) : 0;
    if (wide_test < 0) wide_test = getenv("LARECOMP_WIDE_TEST") ? atof(getenv("LARECOMP_WIDE_TEST")) : 0;
    double shape = wide_test > 0 ? wide_test : (dw / dh) / (16.0 / 9.0);
    bool reshape = reshape_allowed && fabs(shape - 1.0) > 0.005;
    if (!reshape) shape = 1.0;
    // render height in units of the game's frame height
    double h;
    int mode = g_settings.res_mode;
    if (mode!=RES_DYNAMIC) { g_dyn=1; g_last_t=0; g_dyn_n=g_dyn_gpu_n=g_dyn_good=0; g_dyn_dt_sum=g_dyn_gpu_sum=0; }
    if (forced > 0) h = forced;
    else if (mode == RES_1_5) h = 1.5;
    else if (mode >= 1 && mode <= 8) h = mode;
    else {
        h = reshape ? dh / g_base_h : fit;  // Match display
        if (mode == RES_DYNAMIC) {
            // T259: absolute bounds prevent a 4K desktop from making a low-end preset render at 1080p minimum.
            if (g_settings.dynamic_max_height > 0) h = fmin(h, g_settings.dynamic_max_height / g_base_h);
            double floor = g_settings.dynamic_min_height > 0 ? fmin(1.0, g_settings.dynamic_min_height / (g_base_h*h)) : 0.5;
            static double previous_h, previous_floor;
            static int previous_target=-1;
            if (previous_h != h || previous_floor != floor || previous_target != g_settings.fps) {
                g_dyn=1; g_last_t=0; g_dyn_n=g_dyn_gpu_n=g_dyn_good=0;
                g_dyn_dt_sum=g_dyn_gpu_sum=0; g_dyn_cool_until=0; g_dyn_probe_wait=5;
                previous_h=h; previous_floor=floor; previous_target=g_settings.fps;
            }
            dyn_update(now, gpu_ms, floor); h *= g_dyn;
        }
        else if (mode == RES_Q75) h *= 0.75;
        else if (mode == RES_Q25) h *= 0.25;
        else if (mode == RES_Q50) h *= 0.5;
        // [XCDE] temporal upscalers (FSR 3 / DLSS): Match display at the quality mode's render size
        else if (mode == RES_MATCH && (g_settings.upscaler == UPSCALE_FSR3 || g_settings.upscaler == UPSCALE_DLSS ||
                                       g_settings.upscaler == UPSCALE_FSR4_LITE)) {
            static const double ratio[] = {1.0, 1.5, 1.7, 2.0, 3.0};
            int q = g_settings.upscale_quality;
            h /= ratio[q >= 0 && q <= 4 ? q : 1];
        }
    }
    h = fmax(h, 0.1);  // (.25x of a small window)
    double sx = h * shape, sy = h;
    double cap = fmin(MAX_TEX / (g_base_w * sx), MAX_TEX / (g_base_h * sy));
    if (cap < 1.0) { sx *= cap; sy *= cap; }
    // [XCDE] Balanced (1.7x) is the one temporal-upscaler ratio that does not divide the frame into whole pixels: 4K
    // gives 2259x1271, 1440p 1506x847, 1080p 1129x635. The game's half / quarter-resolution targets are then sized and
    // sampled off a fractional grid, and with the upscaler's sub-pixel jitter the image shakes. Snap the render size to a
    // multiple of eight (4K: 2256x1272), as the other presets already land on; the output and aspect stay the same.
    // XCDE_BALANCED_GRID=0: the exact 1.7x size.
    static int snap = -1;
    if (snap < 0) { const char *e = getenv("XCDE_BALANCED_GRID"); snap = !(e && *e == '0'); }
    if (snap && mode == RES_MATCH && forced <= 0 && g_settings.upscale_quality == UQ_BALANCED &&
        (g_settings.upscaler == UPSCALE_FSR3 || g_settings.upscaler == UPSCALE_DLSS || g_settings.upscaler == UPSCALE_FSR4_LITE)) {
        double rw = fmax(8.0, round(g_base_w * sx / 8.0) * 8.0), rh = fmax(8.0, round(g_base_h * sy / 8.0) * 8.0);
        sx = rw / g_base_w; sy = rh / g_base_h;
    }
    o->sx = sx; o->sy = sy; o->shape = shape;
    // where the image goes in the window: fills it when reshaped, else a fitted 16:9 box
    if (reshape) { o->vw = dw; o->vh = dh; }
    else { o->vw = g_base_w * fit; o->vh = g_base_h * fit; }
    if (wide_test > 0) {
        double f2 = fmin(dw / (g_base_w * wide_test), dh / g_base_h);
        o->vw = g_base_w * wide_test * f2; o->vh = g_base_h * f2;
    }
    o->vx = (dw - o->vw) / 2; o->vy = (dh - o->vh) / 2;
}
