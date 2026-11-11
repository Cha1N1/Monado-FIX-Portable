// hid (shared memory, input state filled in later) and settings (set, set:sys).
#include "../diag/diag.h"
#include <string.h>

#include "../hle.h"
#include "../ipc.h"
#include "../settings.h"

static uint32_t new_event_handle(void) {
    uint32_t w, r;
    kernel_create_event_pair(&w, &r);
    return r;
}

// ------------------------------------------------------------------ hid
KSharedMem *g_hid_shmem;
static uint32_t hid_shmem_handle;
RT_ON_RESET(hid_reset) { g_hid_shmem = NULL; hid_shmem_handle = 0; }

static void applet_resource(Service *s, IpcReq *req, IpcResp *resp) {
    (void)s;
    if (req->cmd == 0) {  // GetSharedMemoryHandle
        if (!g_hid_shmem) g_hid_shmem = kernel_create_shared_memory(0x40000, NULL, &hid_shmem_handle);
        resp->copy_handles[resp->ncopy++] = hid_shmem_handle;
    } else {  // [XCDE][RUMBLE] first runs: 201 may land here instead of on the hid service
        static int nlog;
        if (nlog++ < 12)
            LOG("[applet_resource] cmd %u | args %#x %#x | buffers x:%u a:%u b:%u\n", req->cmd,
                IN(uint32_t, 0), IN(uint32_t, 8), req->nx ? (uint32_t)req->x[0].size : 0,
                req->na ? (uint32_t)req->a[0].size : 0, req->nb ? (uint32_t)req->b[0].size : 0);
    }
}

// [XCDE][RUMBLE] GitHub: controller vibration. Measured on the game's hid service (the diagnostics run):
// SendVibrationValue is hid cmd 206 at ~2/frame: X buffer #0 = the 4-byte VibrationDeviceHandle, X buffer #1 =
// the 16-byte VibrationValue {f32 amplitudeLow, frequencyLow, amplitudeHigh, frequencyHigh}. Each Joy-Con half
// has one linear actuator abstracted as low/high bands: the LEFT handle's value drives SDL's low motor, the
// RIGHT handle's the high one (that matches the ~2 calls per frame). Other cmd numbers in the accepted family
// stay telemetry (202..209 minus, 405..410); 201/404/409 keep a generic-multi-quad path.
static void hid_vibration(IpcReq *req) {
    static int nlog;
    extern void input_rumble_offer(float low, float high);
    extern void input_rumble_offer_channel(int right, float amplitude);
    // the 4-byte device handle, the 16-byte value, or a multi-quad array, in whatever buffers arrived
    uint32_t handle = 0;
    int have_handle = 0;
    const float *f = NULL;
    int quads = 0;
    for (int i = 0; i < 8; i++) {
        if (i < req->nx && req->x[i].size == 4) { memcpy(&handle, (void *)(uintptr_t)req->x[i].addr, 4); have_handle = 1; }
        if (i < req->na && req->a[i].size == 4) { memcpy(&handle, (void *)(uintptr_t)req->a[i].addr, 4); have_handle = 1; }
        if (i < req->nx && req->x[i].size >= 16) { f = (const float *)(uintptr_t)req->x[i].addr; quads = (int)(req->x[i].size / 16); }
        if (!f && i < req->na && req->a[i].size >= 16) { f = (const float *)(uintptr_t)req->a[i].addr; quads = (int)(req->a[i].size / 16); }
    }
    if (!f) return;
    if (have_handle && quads == 1) {
        // per-device send (the measured cmd 206 shape): the LEFT handle carries the low band, the RIGHT one the high band.
        // [XCDE][RUMBLE] measured session: the game keeps using a single handle (the right one) whose value carries BOTH
        // bands (its stop pattern is {0, 160, 0, 320}), and some effects only fill amplitudeLow - which the plain
        // handle->band mapping dropped, so those actions never reached the low motor. Carry the other band too whenever
        // the value has content in it (a device's own stop still zeroes only its own band).
        int right = (int)(handle & 1);
        input_rumble_offer_channel(right, right ? f[2] : f[0]);
        if (right) { if (f[0] > 0.0001f) input_rumble_offer_channel(false, f[0]); }
        else       { if (f[2] > 0.0001f) input_rumble_offer_channel(true, f[2]); }
        // [XCDE][RUMBLE] full-session trace (was capped at 12, which only ever caught the idling zero sends)
        if (nlog++ < 1200)
            LOG("[rumble] cmd %u: handle %#x (%s) lo %.3f hi %.3f f %.2f %.2f\n", req->cmd, handle,
                right ? "right" : "left", f[0], f[2], f[1], f[3]);
        return;
    }
    // generic: VibrationValue quads for the single player we serve
    if (nlog++ < 1200) LOG("[rumble] cmd %u: %d values | lo %.3f hi %.3f f %.2f %.2f\n", req->cmd, quads, f[0], f[2], f[1], f[3]);
    input_rumble_offer(f[0], f[2]);
}

SERVICE("hid", hid) {
    (void)s;
    switch (req->cmd) {
    case 0: ipc_return_object(resp, service_new("IAppletResource", applet_resource, NULL)); break;
    case 40: case 106: resp->copy_handles[resp->ncopy++] = new_event_handle(); break;
    case 101: OUT(uint32_t, 0) = 0x1F; resp_size(resp, 4); break;  // GetSupportedNpadStyleSet
    case 121: OUT(int64_t, 0) = 0; resp_size(resp, 8); break;      // GetNpadJoyHoldType
    case 129: OUT(int64_t, 0) = 0; resp_size(resp, 8); break;      // GetNpadHandheldActivationMode
    case 201: case 206: case 404: case 409: hid_vibration(req); break;  // [XCDE][RUMBLE] nn::hid send (206: measured)

    case 202: case 203: case 207: case 208: case 209: case 405: case 406: case 407: case 408: case 410: break;  // accepted
    case 200: memset(resp->data, 0, 0x10); OUT(uint32_t, 0) = 1; resp_size(resp, 0x10); break;
    default: {
        // [XCDE][RUMBLE] first runs: which cmds the vibration path really takes (the 201 guess may be wrong)
        static int nlog;
        if (nlog++ < 30) {
            LOG("[hid] cmd %u | args %#x %#x %#x %#x | buffers x:%u a:%u b:%u\n", req->cmd,
                IN(uint32_t, 0), IN(uint32_t, 8), IN(uint32_t, 16), IN(uint32_t, 24),
                req->nx ? (uint32_t)req->x[0].size : 0, req->na ? (uint32_t)req->a[0].size : 0,
                req->nb ? (uint32_t)req->b[0].size : 0);
        }
        break;  // Activate*/Set*: accept
    }
    }
}

SERVICE("hid:sys", hid_sys) { (void)s; (void)req; resp_size(resp, 0x10); }

// ------------------------------------------------------------------ settings
static const char *const languages[] = {"ja", "en-US", "fr", "de", "it", "es", "zh-CN", "ko", "nl",
                                         "pt", "ru", "zh-TW", "en-GB", "fr-CA", "es-419", "zh-Hans",
                                         "zh-Hant", "pt-BR"};
#define NLANG 18

static uint64_t lang_code(const char *s) {
    uint64_t v = 0;
    memcpy(&v, s, strlen(s));
    return v;
}

static void language_list(IpcReq *req, IpcResp *resp, int max) {
    IpcBuf *b = out_buf(req, 0);
    int n = 0;
    if (b) {
        uint64_t *out = (uint64_t *)(uintptr_t)b->addr;
        for (; n < max && n < NLANG && (uint64_t)(n + 1) * 8 <= b->size; n++) out[n] = lang_code(languages[n]);
    }
    OUT(int32_t, 0) = n;
    resp_size(resp, 4);
}

SERVICE("set", set) {
    (void)s;
    switch (req->cmd) {
    case 0: OUT(uint64_t, 0) = lang_code(settings_language()); resp_size(resp, 8); break;  // GetLanguageCode
    case 1: language_list(req, resp, NLANG); break;                                   // GetAvailableLanguageCodes
    case 2: {                                                                      // MakeLanguageCode
        uint32_t i = IN(uint32_t, 0);
        OUT(uint64_t, 0) = i < NLANG ? lang_code(languages[i]) : 0;
        resp_size(resp, 8);
        break;
    }
    case 3: OUT(int32_t, 0) = NLANG; resp_size(resp, 4); break;
    case 4: OUT(uint32_t, 0) = 1; resp_size(resp, 4); break;                       // GetRegionCode: USA
    case 5: language_list(req, resp, NLANG); break;
    case 6: OUT(int32_t, 0) = NLANG; resp_size(resp, 4); break;
    case 8: OUT(uint8_t, 0) = 0; resp_size(resp, 1); break;                        // GetQuestFlag
    default: diag_unimplemented("ipc", "[set] cmd %u", req->cmd); resp_size(resp, 0x10);
    }
}

SERVICE("set:sys", set_sys) {
    (void)s;
    switch (req->cmd) {
    case 3: case 4: {  // GetFirmwareVersion(2) -> B buffer 0x100
        IpcBuf *b = out_buf(req, 0);
        if (b) {
            uint8_t *v = (uint8_t *)(uintptr_t)b->addr;
            memset(v, 0, 0x100);
            v[0] = 8; v[1] = 1; v[2] = 0; v[4] = 8; v[5] = 1; v[6] = 0;  // 8.1.0
            strcpy((char *)v + 0x68, "8.1.0");
            strcpy((char *)v + 0x80, "8.1.0-1.0");
        }
        break;
    }
    case 23: OUT(uint32_t, 0) = 1; resp_size(resp, 4); break;  // GetColorSetId: dark
    default: diag_unimplemented("ipc", "[set:sys] cmd %u", req->cmd); resp_size(resp, 0x10);
    }
}
