// Bounded MPF CPU scratch/patch storage with native map lifetime.
#include "../hle.h"
#include "foliage_distance.h"

// Unused reserved guest window between the HLE heap and alias mappings.
#define FOLIAGE_ARENA_BASE (HLE_HEAP + HLE_HEAP_SIZE)
#define FOLIAGE_ARENA_STRIDE 0x02000000ull
#define FOLIAGE_ARENA_SLOTS 8
_Static_assert(FOLIAGE_ARENA_BASE + FOLIAGE_ARENA_STRIDE * FOLIAGE_ARENA_SLOTS <= ALIAS_BASE,
               "foliage arena overlaps alias memory");
_Static_assert(XCDE_FOLIAGE_BITMAP_BYTES + XCDE_FOLIAGE_POOL_BYTES + XCDE_FOLIAGE_METADATA_BYTES + 0x3a20u <= FOLIAGE_ARENA_STRIDE,
               "foliage allocation exceeds its slot");
static uint64_t owners[FOLIAGE_ARENA_SLOTS];
static pthread_mutex_t arena_lock = PTHREAD_MUTEX_INITIALIZER;
RT_ON_RESET(foliage_arena_reset) { memset(owners, 0, sizeof owners); }

uint64_t xcde_foliage_pool_alloc(uint64_t owner) {
    if (!owner) FATAL("foliage allocation without an owner\n");
    pthread_mutex_lock(&arena_lock);
    unsigned slot;
    for (slot=0; slot<FOLIAGE_ARENA_SLOTS; ++slot) if (owners[slot]==owner) break;
    if (slot==FOLIAGE_ARENA_SLOTS)
        for (slot=0; slot<FOLIAGE_ARENA_SLOTS && owners[slot]; ++slot) {}
    if (slot==FOLIAGE_ARENA_SLOTS) {
        pthread_mutex_unlock(&arena_lock);
        FATAL("too many simultaneous foliage maps\n");
    }
    uint64_t base=FOLIAGE_ARENA_BASE + slot*FOLIAGE_ARENA_STRIDE;
    if (!owners[slot]) {
        plat_commit(base,xcde_foliage_pool_layout(0));
        // Fresh/decommitted pages are zero; the native constructor initializes its tables.
        owners[slot]=owner;
    }
    pthread_mutex_unlock(&arena_lock);
    if (getenv("XCDE_GRASS_TRACE"))
        LOG("[foliage-pool] owner=%llx base=%llx bytes=%u\n",(unsigned long long)owner,
            (unsigned long long)base,xcde_foliage_pool_layout(0));
    return base;
}

void xcde_foliage_pool_free(uint64_t address) {
    if (address<FOLIAGE_ARENA_BASE || (address-FOLIAGE_ARENA_BASE)%FOLIAGE_ARENA_STRIDE) return;
    uint64_t slot=(address-FOLIAGE_ARENA_BASE)/FOLIAGE_ARENA_STRIDE;
    if (slot>=FOLIAGE_ARENA_SLOTS) return;
    pthread_mutex_lock(&arena_lock);
    if (owners[slot]) {
        if (!plat_reset_guest(address,address+FOLIAGE_ARENA_STRIDE)) {
            pthread_mutex_unlock(&arena_lock);
            FATAL("could not release foliage storage\n");
        }
        owners[slot]=0;
    }
    pthread_mutex_unlock(&arena_lock);
}

