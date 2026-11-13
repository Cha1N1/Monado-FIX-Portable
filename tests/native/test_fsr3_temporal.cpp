// [XCDE] Native FSR3 stationary-detail regression. No game data or window.
// Build: clang++ -std=c++17 -Ithird_party tests/native/test_fsr3_temporal.cpp \
//   -Lbuild/linux -Wl,-rpath,"$PWD/build/linux" -lmonado_fsr3 -lvulkan -o
//   build/fsr3/test-temporal
// Run on the target Vulkan device after scripts/build_fsr3_linux.sh.
// Thirty-two jittered Native AA frames of a 4-pixel-period sine pattern must
// retain detail in persistent history. A constant-colour one-frame smoke test
// cannot detect the LUT history-filter failure this reproduces.
#include <cassert>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ffx_api/ffx_api.h>
#include <ffx_api/ffx_upscale.h>
#include <string>
#include <vector>
#include <vulkan/vulkan.h>
static PFN_vkVoidFunction proc(VkDevice dev, const char *name) {
  auto f = vkGetDeviceProcAddr(dev, name);
  if (f)
    return f;
  std::string core = name;
  if (core.size() > 3 && core.substr(core.size() - 3) == "KHR") {
    core.resize(core.size() - 3);
    return vkGetDeviceProcAddr(dev, core.c_str());
  }
  return nullptr;
}
#define VK(x)                                                                  \
  do {                                                                         \
    auto r = (x);                                                              \
    if (r != VK_SUCCESS) {                                                     \
      fprintf(stderr, "Vulkan %d line %d\n", r, __LINE__);                     \
      abort();                                                                 \
    }                                                                          \
  } while (0)
struct Backend {
  ffxApiHeader header;
  VkDevice dev;
  VkPhysicalDevice phys;
  PFN_vkGetDeviceProcAddr proc;
};
// Optional first argument: an ffx_api library to load at run time instead of the linked
// component (e.g. AMD's prebuilt amd_fidelityfx_vk.dll in a Windows build under Wine).
#ifdef _WIN32
#include <windows.h>
static void *lib_open(const char *p) { return (void *)LoadLibraryA(p); }
static void *lib_sym(void *l, const char *n) { return (void *)GetProcAddress((HMODULE)l, n); }
#else
#include <dlfcn.h>
static void *lib_open(const char *p) { return dlopen(p, RTLD_NOW | RTLD_LOCAL); }
static void *lib_sym(void *l, const char *n) { return dlsym(l, n); }
#endif
static PfnFfxCreateContext pCreate = ffxCreateContext;
static PfnFfxDispatch pDispatch = ffxDispatch;
static PfnFfxDestroyContext pDestroy = ffxDestroyContext;
#define ffxCreateContext pCreate
#define ffxDispatch pDispatch
#define ffxDestroyContext pDestroy
int main(int argc, char **argv) {
  if (argc > 1) {
    void *lib = lib_open(argv[1]);
    if (!lib) { fprintf(stderr, "cannot load %s\n", argv[1]); return 1; }
    pCreate = (PfnFfxCreateContext)lib_sym(lib, "ffxCreateContext");
    pDispatch = (PfnFfxDispatch)lib_sym(lib, "ffxDispatch");
    pDestroy = (PfnFfxDestroyContext)lib_sym(lib, "ffxDestroyContext");
    if (!pCreate || !pDispatch || !pDestroy) { fprintf(stderr, "%s lacks ffx_api entry points\n", argv[1]); return 1; }
    printf("library %s\n", argv[1]);
  }
  const float sign = 1;
  VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
  app.apiVersion = VK_API_VERSION_1_3;
  VkInstanceCreateInfo ic{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
  ic.pApplicationInfo = &app;
  VkInstance instance;
  VK(vkCreateInstance(&ic, nullptr, &instance));
  uint32_t n = 0;
  VK(vkEnumeratePhysicalDevices(instance, &n, nullptr));
  assert(n);
  std::vector<VkPhysicalDevice> devices(n);
  VK(vkEnumeratePhysicalDevices(instance, &n, devices.data()));
  auto phys = devices[0];
  uint32_t qn = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(phys, &qn, nullptr);
  std::vector<VkQueueFamilyProperties> qs(qn);
  vkGetPhysicalDeviceQueueFamilyProperties(phys, &qn, qs.data());
  uint32_t family = 0;
  while (!(qs[family].queueFlags & VK_QUEUE_COMPUTE_BIT))
    family++;
  float priority = 1;
  VkDeviceQueueCreateInfo qi{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
  qi.queueFamilyIndex = family;
  qi.queueCount = 1;
  qi.pQueuePriorities = &priority;
  VkPhysicalDeviceFeatures features;
  vkGetPhysicalDeviceFeatures(phys, &features);
  VkPhysicalDeviceVulkan13Features f13{
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
  VkPhysicalDeviceVulkan12Features f12{
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
  f12.pNext = &f13;
  VkPhysicalDeviceFeatures2 f2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
  f2.pNext = &f12;
  vkGetPhysicalDeviceFeatures2(phys, &f2);
  VkDeviceCreateInfo dc{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
  dc.pNext = &f12;
  dc.pEnabledFeatures = &features;
  dc.queueCreateInfoCount = 1;
  dc.pQueueCreateInfos = &qi;
  VkDevice dev;
  VK(vkCreateDevice(phys, &dc, nullptr, &dev));
  VkQueue queue;
  vkGetDeviceQueue(dev, family, 0, &queue);
  VkCommandPoolCreateInfo pc{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
  pc.queueFamilyIndex = family;
  pc.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  VkCommandPool pool;
  VK(vkCreateCommandPool(dev, &pc, nullptr, &pool));
  VkCommandBufferAllocateInfo ai{
      VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
  ai.commandPool = pool;
  ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  ai.commandBufferCount = 1;
  VkCommandBuffer cb;
  VK(vkAllocateCommandBuffers(dev, &ai, &cb));
  VkPhysicalDeviceMemoryProperties mp;
  vkGetPhysicalDeviceMemoryProperties(phys, &mp);
  std::vector<VkDeviceMemory> mems;
  std::vector<VkImage> images;
  auto image = [&](VkFormat fmt, uint32_t ff, uint32_t w, uint32_t h) {
    VkImageCreateInfo ci{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    ci.imageType = VK_IMAGE_TYPE_2D;
    ci.format = fmt;
    ci.extent = {w, h, 1};
    ci.mipLevels = ci.arrayLayers = 1;
    ci.samples = VK_SAMPLE_COUNT_1_BIT;
    ci.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
               VK_IMAGE_USAGE_TRANSFER_DST_BIT |
               VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    VkImage im;
    VK(vkCreateImage(dev, &ci, nullptr, &im));
    images.push_back(im);
    VkMemoryRequirements mr;
    vkGetImageMemoryRequirements(dev, im, &mr);
    uint32_t type = 0;
    while (!(mr.memoryTypeBits & (1u << type)))
      type++;
    VkMemoryAllocateInfo ma{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    ma.allocationSize = mr.size;
    ma.memoryTypeIndex = type;
    VkDeviceMemory mem;
    VK(vkAllocateMemory(dev, &ma, nullptr, &mem));
    mems.push_back(mem);
    VK(vkBindImageMemory(dev, im, mem, 0));
    FfxApiResource r{};
    r.resource = (void *)im;
    r.description.type = FFX_API_RESOURCE_TYPE_TEXTURE2D;
    r.description.format = ff;
    r.description.width = w;
    r.description.height = h;
    r.description.depth = r.description.mipCount = 1;
    r.description.usage = FFX_API_RESOURCE_USAGE_UAV;
    r.state = FFX_API_RESOURCE_STATE_UNORDERED_ACCESS;
    return r;
  };
  ffxDispatchDescUpscale d{};
  d.header.type = FFX_API_DISPATCH_DESC_TYPE_UPSCALE;
  d.commandList = cb;
  d.color = image(VK_FORMAT_R16G16B16A16_SFLOAT,
                  FFX_API_SURFACE_FORMAT_R16G16B16A16_FLOAT, 128, 128);
  d.motionVectors = image(VK_FORMAT_R16G16_SFLOAT,
                          FFX_API_SURFACE_FORMAT_R16G16_FLOAT, 128, 128);
  d.depth =
      image(VK_FORMAT_R32_SFLOAT, FFX_API_SURFACE_FORMAT_R32_FLOAT, 128, 128);
  d.output = image(VK_FORMAT_R16G16B16A16_SFLOAT,
                   FFX_API_SURFACE_FORMAT_R16G16B16A16_FLOAT, 128, 128);
  // FFX_BACKEND_TYPE: 3 for FSR 3.1.4-era libraries, 0x01000003 for AMD's FidelityFX SDK 2.4 loader.
  uint64_t backend_type = getenv("FFX_BACKEND_TYPE") ? strtoull(getenv("FFX_BACKEND_TYPE"), nullptr, 0) : 3;
  Backend be{{backend_type, nullptr}, dev, phys, proc};
  ffxCreateContextDescUpscale create{};
  create.header = {FFX_API_CREATE_CONTEXT_DESC_TYPE_UPSCALE, &be.header};
  create.maxRenderSize = {128, 128};
  create.maxUpscaleSize = {128, 128};
  create.flags =
      FFX_UPSCALE_ENABLE_DEPTH_INFINITE | FFX_UPSCALE_ENABLE_DYNAMIC_RESOLUTION;
  ffxContext context = nullptr;
  auto result = ffxCreateContext(&context, &create.header, nullptr);
  printf("create=%u\n", result);
  fflush(stdout);
  assert(result == 0);
  VkBufferCreateInfo bc{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
  bc.size = 128 * 128 * 8;
  bc.usage =
      VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
  bc.size *= 2;
  VkBuffer rb;
  VK(vkCreateBuffer(dev, &bc, nullptr, &rb));
  VkMemoryRequirements br;
  vkGetBufferMemoryRequirements(dev, rb, &br);
  uint32_t host = 0;
  while (!(br.memoryTypeBits & (1u << host)) ||
         (mp.memoryTypes[host].propertyFlags &
          (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
           VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) !=
             (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
              VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
    host++;
  VkMemoryAllocateInfo ba{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
  ba.allocationSize = br.size;
  ba.memoryTypeIndex = host;
  VkDeviceMemory bm;
  VK(vkAllocateMemory(dev, &ba, nullptr, &bm));
  VK(vkBindBufferMemory(dev, rb, bm, 0));
  void *mapped;
  VK(vkMapMemory(dev, bm, 0, VK_WHOLE_SIZE, 0, &mapped));
  auto halton = [](int i, int b) {
    float f = 1, r = 0;
    for (; i; i /= b) {
      f /= b;
      r += f * (i % b);
    }
    return r;
  };
  for (int frame = 0; frame < 32; frame++) {
    VK(vkResetCommandBuffer(cb, 0));
    float jx = halton(frame % 8 + 1, 2) - 0.5f,
          jy = halton(frame % 8 + 1, 3) - 0.5f;
    auto input = static_cast<_Float16 *>(mapped);
    for (int y = 0; y < 128; y++)
      for (int x = 0; x < 128; x++) {
        float v = 0.5f + 0.4f * cosf((x + 0.5f - jx) * 1.570796327f);
        for (int c = 0; c < 4; c++)
          input[(y * 128 + x) * 4 + c] = c == 3 ? 1 : v;
      }
    VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    VK(vkBeginCommandBuffer(cb, &begin));
    VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    for (auto im : images) {
      VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
      b.oldLayout =
          frame == 0 ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_GENERAL;
      b.newLayout = VK_IMAGE_LAYOUT_GENERAL;
      b.srcQueueFamilyIndex = b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      b.image = im;
      b.subresourceRange = range;
      b.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
      vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                           VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                           nullptr, 1, &b);
      VkClearColorValue clear{{0.25f, 0.25f, 0.25f, 1}};
      if (im == images[1] || im == images[3])
        clear = {};
      if (im == images[2])
        clear.float32[0] = 0.5f;
      vkCmdClearColorImage(cb, im, VK_IMAGE_LAYOUT_GENERAL, &clear, 1, &range);
    }
    VkBufferImageCopy upload{};
    upload.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    upload.imageExtent = {128, 128, 1};
    vkCmdCopyBufferToImage(cb, rb, images[0], VK_IMAGE_LAYOUT_GENERAL, 1,
                           &upload);
    VkMemoryBarrier mb{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    mb.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    mb.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &mb, 0,
                         nullptr, 0, nullptr);
    d.jitterOffset = {jx * sign, jy * sign};
    d.motionVectorScale = {128, 128};
    d.renderSize = {128, 128};
    d.upscaleSize = {128, 128};
    d.frameTimeDelta = 16.67f;
    d.preExposure = 1;
    d.reset = frame == 0;
    d.cameraNear = 0.1f;
    d.cameraFar = FLT_MAX;
    d.cameraFovAngleVertical = 1;
    d.viewSpaceToMetersFactor = 1;
    d.enableSharpening = false;
    d.sharpness = 0.3f;
    result = ffxDispatch(&context, &d.header);
    assert(result == 0);
    mb.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    mb.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 1, &mb, 0, nullptr,
                         0, nullptr);
    VkBufferImageCopy copy{};
    copy.bufferOffset = 128 * 128 * 8;
    copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    copy.imageExtent = {128, 128, 1};
    vkCmdCopyImageToBuffer(cb, images[3], VK_IMAGE_LAYOUT_GENERAL, rb, 1,
                           &copy);
    mb.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    mb.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
    vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &mb, 0, nullptr, 0,
                         nullptr);
    VK(vkEndCommandBuffer(cb));
    VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cb;
    VK(vkQueueSubmit(queue, 1, &submit, VK_NULL_HANDLE));
    VK(vkQueueWaitIdle(queue));
    auto output = reinterpret_cast<_Float16 *>(static_cast<char *>(mapped) +
                                               128 * 128 * 8);
    double err = 0, amp = 0;
    for (int y = 16; y < 112; y++)
      for (int x = 16; x < 112; x++) {
        double expected = 0.5 + 0.4 * cos((x + 0.5) * 1.570796327);
        double got = output[(y * 128 + x) * 4];
        err += (got - expected) * (got - expected);
        amp += (got - 0.5) * cos((x + 0.5) * 1.570796327);
      }
    if (frame == 31) {
      assert(sqrt(err / (96 * 96)) < 0.05);
      assert(amp * 2 / (96 * 96) > 0.30);
    }
    if (frame == 0 || frame == 7 || frame == 31)
      printf("sign %.0f frame %d rms %.4f amplitude %.4f\n", sign, frame,
             sqrt(err / (96 * 96)), amp * 2 / (96 * 96));
  }
  vkUnmapMemory(dev, bm);
  vkDestroyBuffer(dev, rb, nullptr);
  vkFreeMemory(dev, bm, nullptr);
  assert(ffxDestroyContext(&context, nullptr) == 0);
  for (auto im : images)
    vkDestroyImage(dev, im, nullptr);
  for (auto mem : mems)
    vkFreeMemory(dev, mem, nullptr);
  vkDestroyCommandPool(dev, pool, nullptr);
  vkDestroyDevice(dev, nullptr);
  vkDestroyInstance(instance, nullptr);
  puts("native FSR3 synthetic dispatch OK");
}
