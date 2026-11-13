// [XCDE] T-LINUX-GTT: driver-owned AMD guest memory, adapted from
// weeping-of-the-empire engine/runtime/linux/plat_posix.cpp, commit fe2345e8.
// Copyright (c) 2026 daniel-madrid-07. MIT; see third_party/weeping-of-the-empire/LICENSE.
// Included only by platform/linux.c. Mapping is permitted only before guest writes.
#pragma once
#include <sys/ioctl.h>
struct MonadoDrmVersion { int major, minor, patch; size_t name_len; char *name; size_t date_len; char *date; size_t desc_len; char *desc; };
struct MonadoGemCreate { uint64_t size, alignment, domains, flags; };
struct MonadoGemMmap { uint64_t value; };
struct MonadoPrime { uint32_t handle, flags; int32_t fd; };
struct MonadoGemClose { uint32_t handle, pad; };
static int gpu_heap_fd = -1;
static uintptr_t gpu_heap_base;
static size_t gpu_heap_size;

static int linux_gpu_heap_map(void *address, size_t size) {
    const char *setting = getenv("XCDE_GPU_BO");
    if (!size || ((uintptr_t)address & 4095) || (size & 4095) || gpu_heap_fd >= 0 || (setting && *setting == '0')) return -1;
    int amd = -1, nodes = 0;
    for (int i = 128; i < 192; i++) {
        char path[40]; snprintf(path, sizeof path, "/dev/dri/renderD%d", i);
        int fd = open(path, O_RDWR | O_CLOEXEC);
        if (fd < 0) continue;
        nodes++;
        char name[32] = {0};
        struct MonadoDrmVersion version = {.name_len = sizeof name - 1, .name = name};
        if (!ioctl(fd, _IOWR('d', 0x00, struct MonadoDrmVersion), &version) && !strcmp(name, "amdgpu") && amd < 0) amd = fd;
        else close(fd);
    }
    if (amd < 0) return -1;
    if (nodes != 1 && !(setting && *setting == '1')) { close(amd); return -1; }
    struct MonadoGemCreate create = {size, 2u << 20, 2, 1u << 7}; // GTT, EXPLICIT_SYNC
    if (ioctl(amd, _IOWR('d', 0x40, struct MonadoGemCreate), &create)) { close(amd); return -1; }
    uint32_t handle = (uint32_t)create.size;
    struct MonadoGemMmap map = {handle};
    struct MonadoPrime prime = {handle, O_CLOEXEC | O_RDWR, -1};
    void *p = MAP_FAILED;
    if (!ioctl(amd, _IOWR('d', 0x41, struct MonadoGemMmap), &map) &&
        !ioctl(amd, _IOWR('d', 0x2d, struct MonadoPrime), &prime))
        p = mmap(address, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, amd, (off_t)map.value);
    struct MonadoGemClose release = {handle, 0};
    ioctl(amd, _IOW('d', 0x09, struct MonadoGemClose), &release);
    close(amd);
    if (p != address) {
        if (prime.fd >= 0) close(prime.fd);
        if (p != MAP_FAILED) munmap(p, size);
        // The original reservation has no live data yet. Restore it if MAP_FIXED failed.
        if (mmap(address, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE | MAP_FIXED, -1, 0) != address) {
            fprintf(stderr, "FATAL: could not restore guest heap reservation\n");
            _exit(1);
        }
        return -1;
    }
    // Preserve the anonymous-memory zero-initialization contract explicitly.
    memset(address, 0, size);
    gpu_heap_fd = prime.fd;
    gpu_heap_base = (uintptr_t)address;
    gpu_heap_size = size;
    fprintf(stderr, "[gpu] guest heap backed by driver-owned GTT (%zu MiB), dma-buf %d\n", size >> 20, prime.fd);
    return prime.fd;
}
