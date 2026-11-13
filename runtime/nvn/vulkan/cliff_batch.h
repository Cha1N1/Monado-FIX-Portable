// [XCDE] T-CLIFF-BATCH: preserve command order while coalescing contiguous instance records.
#pragma once
#include <string.h>
#include "../nvn.h"

typedef struct { uint64_t addr, size; } VkCliffRange;
typedef struct {
    const CmdRec *next;
    VkCliffRange last;
    uint32_t draws, vertex_binds, stream_binds;
} VkCliffBatch;

// The caller validates shader semantics, fixed state, and stream 1's divisor/attribute bounds.
// Only instance-buffer binds and identical stream-state binds may occur between these draws.
// Uncommitted commands before an incompatible draw are left for ordinary execution.
static inline VkCliffBatch vk_cliff_batch_scan(const CmdRec *first, VkCliffRange initial,
                                               const uint32_t streams[2][2], uint32_t limit) {
    VkCliffBatch out = {.next = (const CmdRec *)((const uint8_t *)first + first->size),
                       .last = initial, .draws = 1};
    uint32_t stride = streams[1][0], vb = 0, vs = 0;
    if (first->op != CMD_DRAW_ELEMENTS || first->size != sizeof *first ||
        first->x[5] || first->x[6] > 1 || !stride || streams[1][1] != 1 || limit < 2) return out;
    VkCliffRange pending = initial;
    const CmdRec *q = out.next;
    for (uint32_t scanned = 0; out.draws < limit && scanned < limit * 8; scanned++) {
        if (q->size < sizeof *q || (q->size & 7) || q->size > 65536) break;
        if (q->op == CMD_BIND_VERTEX_BUFFERS) {
            if (q->x[0] != 1 || q->x[1] != 1 || q->size < sizeof *q + sizeof pending) break;
            memcpy(&pending, q->extra, sizeof pending);
            vb++;
        } else if (q->op == CMD_BIND_VERTEX_STREAM) {
            if (q->x[0] != 2 || q->size < sizeof *q + 16 || memcmp(q->extra, streams, 16)) break;
            vs++;
        } else if (q->op == CMD_DRAW_ELEMENTS) {
            uint64_t offset = (uint64_t)out.draws * stride;
            if (q->size != sizeof *q || q->x[6] > 1 || memcmp(q->x, first->x, 6 * sizeof(uint64_t)) ||
                initial.addr > UINT64_MAX - offset || pending.addr != initial.addr + offset ||
                pending.size < stride || initial.size / stride <= out.draws) break;
            out.draws++;
            out.last = pending;
            out.vertex_binds += vb;
            out.stream_binds += vs;
            vb = vs = 0;
            out.next = (const CmdRec *)((const uint8_t *)q + q->size);
        } else {
            break;  // Includes barriers, queries, inline writes, state changes and list boundaries.
        }
        q = (const CmdRec *)((const uint8_t *)q + q->size);
    }
    return out;
}

