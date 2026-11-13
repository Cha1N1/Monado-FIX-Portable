// Monado-only FidelityFX API subset backed by AMD FSR 3.1.4 and native Vulkan.
// This library implements only the descriptors used by temporal.c, without FG.
#include <FidelityFX/host/ffx_fsr3upscaler.h>
#include <FidelityFX/host/backends/vk/ffx_vk.h>
#include <ffx_api/ffx_api.h>
#include <ffx_api/ffx_upscale.h>
#include <cstdlib>
#include <cstring>

#if defined(_WIN32)
#define EXPORT extern "C" __declspec(dllexport)
#else
#define EXPORT extern "C" __attribute__((visibility("default")))
#endif
struct BackendDesc {
    ffxCreateContextDescHeader header;
    VkDevice device;
    VkPhysicalDevice physical;
    PFN_vkGetDeviceProcAddr proc;
};
struct Context {
    FfxFsr3UpscalerContext fsr{};
    FfxInterface backend{};
    FfxResourceInternal shared[3]{};
    unsigned sharedCount = 0;
    bool created = false;
    VkDeviceContext device{};
};
static ffxReturnCode_t result(FfxErrorCode code) {
    if (code != FFX_OK) std::fprintf(stderr,"[fsr3] SDK error 0x%x\n", unsigned(code));
    return code == FFX_OK ? FFX_API_RETURN_OK : FFX_API_RETURN_ERROR_RUNTIME_ERROR;
}
static void release(Context *c) {
    for (unsigned i=0;i<c->sharedCount;i++) c->backend.fpDestroyResource(&c->backend,c->shared[i],0);
    if (c->created) ffxFsr3UpscalerContextDestroy(&c->fsr);
    std::free(c->backend.scratchBuffer);
    delete c;
}
EXPORT ffxReturnCode_t ffxCreateContext(ffxContext *out, ffxCreateContextDescHeader *header, const ffxAllocationCallbacks *alloc) {
    if (!out || !header || alloc) return FFX_API_RETURN_ERROR_PARAMETER;
    *out=nullptr;
    if (header->type != FFX_API_CREATE_CONTEXT_DESC_TYPE_UPSCALE) return FFX_API_RETURN_ERROR_UNKNOWN_DESCTYPE;
    auto *d=reinterpret_cast<ffxCreateContextDescUpscale*>(header);
    auto *be=header->pNext;
    while (be && be->type != 3) be=be->pNext;
    if (!be) return FFX_API_RETURN_ERROR_PARAMETER;
    auto *vk=reinterpret_cast<BackendDesc*>(be);
    // Shader set is intentionally restricted to Monado's input conventions.
    if (d->flags & (FFX_UPSCALE_ENABLE_HIGH_DYNAMIC_RANGE | FFX_UPSCALE_ENABLE_DISPLAY_RESOLUTION_MOTION_VECTORS |
        FFX_UPSCALE_ENABLE_MOTION_VECTORS_JITTER_CANCELLATION | FFX_UPSCALE_ENABLE_DEPTH_INVERTED))
        return FFX_API_RETURN_ERROR_PARAMETER;
    auto *c=new(std::nothrow) Context;
    if (!c) return FFX_API_RETURN_ERROR_MEMORY;
    c->device={vk->device,vk->physical,vk->proc};
    size_t size=ffxGetScratchMemorySizeVK(vk->physical,1);
    void *scratch=std::calloc(1,size);
    if (!scratch) { delete c; return FFX_API_RETURN_ERROR_MEMORY; }
    FfxErrorCode err=ffxGetInterfaceVK(&c->backend,ffxGetDeviceVK(&c->device),scratch,size,1);
    if (err != FFX_OK) { std::free(scratch); delete c; return result(err); }
    FfxFsr3UpscalerContextDescription desc{};
    desc.backendInterface=c->backend;
    desc.maxRenderSize={d->maxRenderSize.width,d->maxRenderSize.height};
    desc.maxUpscaleSize={d->maxUpscaleSize.width,d->maxUpscaleSize.height};
    desc.flags=FFX_FSR3UPSCALER_ENABLE_DEPTH_INFINITE | FFX_FSR3UPSCALER_ENABLE_DYNAMIC_RESOLUTION;
    if (d->flags & FFX_UPSCALE_ENABLE_DEBUG_CHECKING) desc.flags |= FFX_FSR3UPSCALER_ENABLE_DEBUG_CHECKING;
    desc.fpMessage=reinterpret_cast<FfxFsr3UpscalerMessage>(d->fpMessage);
    err=ffxFsr3UpscalerContextCreate(&c->fsr,&desc);
    if (err != FFX_OK) { release(c); return result(err); }
    c->created=true;
    FfxFsr3UpscalerSharedResourceDescriptions shared{};
    err=ffxFsr3UpscalerGetSharedResourceDescriptions(&c->fsr,&shared);
    FfxCreateResourceDescription descriptions[]={shared.dilatedDepth,shared.dilatedMotionVectors,shared.reconstructedPrevNearestDepth};
    for (unsigned i=0;err==FFX_OK && i<3;i++) {
        err=c->backend.fpCreateResource(&c->backend,&descriptions[i],0,&c->shared[i]);
        if (err==FFX_OK) c->sharedCount++;
    }
    if (err != FFX_OK) { release(c); return result(err); }
    *out=c; return FFX_API_RETURN_OK;
}
EXPORT ffxReturnCode_t ffxDestroyContext(ffxContext *context, const ffxAllocationCallbacks *alloc) {
    if (!context || !*context || alloc) return FFX_API_RETURN_ERROR_PARAMETER;
    release(static_cast<Context*>(*context)); *context=nullptr; return FFX_API_RETURN_OK;
}
EXPORT ffxReturnCode_t ffxQuery(ffxContext *context, ffxQueryDescHeader *header) {
    if (!context || !*context || !header) return FFX_API_RETURN_ERROR_PARAMETER;
    if (header->type != FFX_API_QUERY_DESC_TYPE_GET_PROVIDER_VERSION) return FFX_API_RETURN_ERROR_UNKNOWN_DESCTYPE;
    auto *d=reinterpret_cast<ffxQueryGetProviderVersion*>(header);
    d->versionId=0; d->versionName="FSR 3.1.4 native Vulkan (FP32)"; return FFX_API_RETURN_OK;
}
EXPORT ffxReturnCode_t ffxConfigure(ffxContext*, const ffxConfigureDescHeader*) {
    return FFX_API_RETURN_ERROR_UNKNOWN_DESCTYPE;
}
static FfxResource resource(const FfxApiResource &r) {
    FfxResource out{};
    out.resource=r.resource;
    if (!r.resource) return out;
    out.state=FFX_RESOURCE_STATE_UNORDERED_ACCESS;
    out.description.type=FFX_RESOURCE_TYPE_TEXTURE2D;
    out.description.width=r.description.width; out.description.height=r.description.height;
    out.description.depth=1; out.description.mipCount=1;
    out.description.usage=r.description.usage & FFX_API_RESOURCE_USAGE_UAV ? FFX_RESOURCE_USAGE_UAV : FFX_RESOURCE_USAGE_READ_ONLY;
    switch (r.description.format) {
    case FFX_API_SURFACE_FORMAT_R16G16B16A16_FLOAT: out.description.format=FFX_SURFACE_FORMAT_R16G16B16A16_FLOAT; break;
    case FFX_API_SURFACE_FORMAT_R16G16_FLOAT: out.description.format=FFX_SURFACE_FORMAT_R16G16_FLOAT; break;
    case FFX_API_SURFACE_FORMAT_R32_FLOAT: out.description.format=FFX_SURFACE_FORMAT_R32_FLOAT; break;
    case FFX_API_SURFACE_FORMAT_R8_UNORM: out.description.format=FFX_SURFACE_FORMAT_R8_UNORM; break;
    default: out.description.format=FFX_SURFACE_FORMAT_UNKNOWN; break;
    }
    return out;
}
EXPORT ffxReturnCode_t ffxDispatch(ffxContext *context, const ffxDispatchDescHeader *header) {
    if (!context || !*context || !header) return FFX_API_RETURN_ERROR_PARAMETER;
    if (header->type != FFX_API_DISPATCH_DESC_TYPE_UPSCALE) return FFX_API_RETURN_ERROR_UNKNOWN_DESCTYPE;
    auto *c=static_cast<Context*>(*context);
    auto *d=reinterpret_cast<const ffxDispatchDescUpscale*>(header);
    FfxFsr3UpscalerDispatchDescription p{};
    p.commandList=d->commandList;
    p.color=resource(d->color); p.depth=resource(d->depth); p.motionVectors=resource(d->motionVectors);
    p.output=resource(d->output); p.reactive=resource(d->reactive);
    p.transparencyAndComposition=resource(d->transparencyAndComposition);
    p.dilatedDepth=c->backend.fpGetResource(&c->backend,c->shared[0]);
    p.dilatedMotionVectors=c->backend.fpGetResource(&c->backend,c->shared[1]);
    p.reconstructedPrevNearestDepth=c->backend.fpGetResource(&c->backend,c->shared[2]);
    p.jitterOffset={d->jitterOffset.x,d->jitterOffset.y};
    p.motionVectorScale={d->motionVectorScale.x,d->motionVectorScale.y};
    p.renderSize={d->renderSize.width,d->renderSize.height}; p.upscaleSize={d->upscaleSize.width,d->upscaleSize.height};
    p.enableSharpening=d->enableSharpening; p.sharpness=d->sharpness;
    p.frameTimeDelta=d->frameTimeDelta; p.preExposure=d->preExposure; p.reset=d->reset;
    p.cameraNear=d->cameraNear; p.cameraFar=d->cameraFar; p.cameraFovAngleVertical=d->cameraFovAngleVertical;
    p.viewSpaceToMetersFactor=d->viewSpaceToMetersFactor;
    p.flags=d->flags & FFX_UPSCALE_FLAG_DRAW_DEBUG_VIEW ? FFX_FSR3UPSCALER_DISPATCH_DRAW_DEBUG_VIEW : 0;
    return result(ffxFsr3UpscalerContextDispatch(&c->fsr,&p));
}

// Upscale-only component: no swapchain replacement or frame generation.
extern "C" FfxErrorCode ffxSetFrameGenerationConfigToSwapchainVK(const FfxFrameGenerationConfig*) {
    return FFX_ERROR_INVALID_ARGUMENT;
}
