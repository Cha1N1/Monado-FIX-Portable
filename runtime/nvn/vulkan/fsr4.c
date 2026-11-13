// [XCDE] T-FSR4-LITE. The optional component and AMD model assets are loaded separately.
#include "fsr4.h"
#include "vk.h"
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#define FSR4_LIBRARY_NAME "monado_fsr4.dll"
static void *fsr4_open(const char *name) { return (void *)LoadLibraryA(name); }
static void *fsr4_sym(void *m, const char *n) { return (void *)GetProcAddress((HMODULE)m, n); }
static void fsr4_close(void *m) { FreeLibrary((HMODULE)m); }
static const char *fsr4_error(void) {
    static char msg[64];
    snprintf(msg, sizeof msg, "LoadLibrary error %lu", (unsigned long)GetLastError());
    return msg;
}
#else
#include <dlfcn.h>
#include <unistd.h>
#define FSR4_LIBRARY_NAME "libmonado_fsr4.so"
static void *fsr4_open(const char *name) { return dlopen(name, RTLD_NOW | RTLD_LOCAL); }
static void *fsr4_sym(void *m, const char *n) { return dlsym(m, n); }
static void fsr4_close(void *m) { dlclose(m); }
static const char *fsr4_error(void) { return dlerror(); }
#endif
// Resolve <executable folder>/<leaf> (the component is deployed beside the game).
static bool fsr4_beside_exe(char *path, size_t n, const char *leaf) {
#ifdef _WIN32
    DWORD len = GetModuleFileNameA(NULL, path, (DWORD)n);
    if (!len || (size_t)len >= n) return false;
    char *slash = strrchr(path, '\\');
#else
    ssize_t got = readlink("/proc/self/exe", path, n - 1);
    if (got <= 0) return false;
    path[got] = 0;
    char *slash = strrchr(path, '/');
#endif
    if (!slash || slash - path + 1 + (ptrdiff_t)strlen(leaf) >= (ptrdiff_t)n) return false;
    strcpy(slash + 1, leaf);
    return true;
}
#ifdef __linux__
bool fsr4_device_ok;
static const MonadoFsr4Api *api;
static char assets[4096];
static int load_state;
bool fsr4_available(void) {
    if (!fsr4_device_ok) return false;
    // Reject oversized output before the game starts jittering its scene draws.
    if (g_vk.sc_extent.width > 3840 || g_vk.sc_extent.height > 2160) {
        static uint32_t warned_w, warned_h;
        if (warned_w != g_vk.sc_extent.width || warned_h != g_vk.sc_extent.height) {
            warned_w = g_vk.sc_extent.width; warned_h = g_vk.sc_extent.height;
            LOG("[fsr4] %ux%u output exceeds native model limits; using spatial FSR\n", warned_w, warned_h);
        }
        return false;
    }
    if (load_state) return load_state > 0;
    load_state = -1;
    const char *lib = getenv("MONADO_FSR4_LIBRARY");
    void *module = fsr4_open(lib ? lib : FSR4_LIBRARY_NAME);
    if (!module && !lib) {
        char path[4096];
        if (fsr4_beside_exe(path, sizeof path, FSR4_LIBRARY_NAME)) module = fsr4_open(path);
    }
    if (!module) { LOG("[fsr4] native component unavailable: %s\n", fsr4_error()); return false; }
    MonadoFsr4GetApi get = (MonadoFsr4GetApi)fsr4_sym(module,"monado_fsr4_get_api");
    api = get ? get(MONADO_FSR4_ABI) : NULL;
    if (!api || api->abi != MONADO_FSR4_ABI || !api->create || !api->build || !api->record || !api->destroy || !api->error) {
        LOG("[fsr4] incompatible native component\n"); fsr4_close(module); api = NULL; return false;
    }
    const char *dir = getenv("MONADO_FSR4_ASSET_DIR");
    static char beside[4096];
    if (!dir && fsr4_beside_exe(beside, sizeof beside, "fsr4")) dir = beside;  // <executable folder>/fsr4/lite/...
    if (!dir || strlen(dir) >= sizeof(assets)) { LOG("[fsr4] set MONADO_FSR4_ASSET_DIR to the prepared model directory\n"); return false; }
    strcpy(assets,dir);
    static const char *sets[] = {"t1080_m0","t1080_m1","t2160_m0","t2160_m1"};
    for (unsigned i=0;i<4;i++) { char path[4200]; snprintf(path,sizeof(path),"%s/lite/%s/initializer.bin",assets,sets[i]);
        FILE *f=fopen(path,"rb"); if (!f) { LOG("[fsr4] missing Lite weights: %s\n",path); return false; }
        fseek(f,0,SEEK_END); long size=ftell(f); fclose(f);
        if(size!=27392) { LOG("[fsr4] invalid Lite weights: %s\n",path); return false; }
    }
    load_state=1;
    LOG("[fsr4] native Vulkan FSR 4.1.2 Lite INT8 ready\n"); return true;
}
typedef struct Retired { void *ctx; uint64_t seq; struct Retired *next; } Retired;
static void *ctx;
static Retired *retired;
static uint32_t width,height,ultra,slot;
static uint64_t ring[8],last_use;
bool fsr4_record(const MonadoFsr4Frame *f) {
    if (!fsr4_available() || !f->output.width || !f->output.height || f->output.width>3840 || f->output.height>2160) return false;
    uint64_t complete=vk_completed_seq();
    Retired **at=&retired;
    while (*at) { Retired *r=*at; if(r->seq<=complete) { *at=r->next;api->destroy(r->ctx);free(r); } else at=&r->next; }
    bool changed=!ctx || width!=f->output.width || height!=f->output.height || ultra!=f->ultra;
    if (changed) {
        LOG("[fsr4-reset] context rebuild: initial=%d output-size=%d model=%d; %ux%u -> %ux%u\n",
            !ctx, ctx && (width!=f->output.width || height!=f->output.height), ctx && ultra!=f->ultra,
            width,height,f->output.width,f->output.height);
        void *fresh=api->create(g_vk.phys,g_vk.dev,assets);
        if(!fresh) return false;
        if(!api->build(fresh,f->output.width,f->output.height,f->ultra)) {
            LOG("[fsr4] pipeline build failed: %s\n",api->error(fresh)); api->destroy(fresh); load_state=-1; return false;
        }
        if(ctx) { Retired *r=malloc(sizeof(*r)); if(!r) { api->destroy(fresh);return false; }
            *r=(Retired){ctx,last_use,retired};retired=r; }
        ctx=fresh;width=f->output.width;height=f->output.height;ultra=f->ultra;slot=0;memset(ring,0,sizeof(ring));
        LOG("[fsr4] Lite INT8 %ux%u, %s model\n",width,height,ultra?"ultra":"standard");
    }
    // Protect the library's eight host-mapped constant-buffer slots from GPU reuse.
    if(ring[slot]>complete) { if(ring[slot]>=vk_pending_seq()) return false; vk_wait_seq(ring[slot]); }
    MonadoFsr4Frame frame=*f;frame.reset|=changed;
    if(!api->record(ctx,&frame)) { LOG("[fsr4] dispatch failed: %s\n",api->error(ctx)); load_state=-1; return false; }
    ring[slot]=last_use=vk_pending_seq();slot=(slot+1)%8;
    return true;
}
#endif
