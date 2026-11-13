// T-CLIFF-BATCH: command boundaries, original fetch order, and final binding state.
// clang -std=c11 -Iruntime -Iruntime/nvn tests/native/test_cliff_batch.c
#include <assert.h>
#include <stdalign.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "nvn/vulkan/cliff_batch.h"

static alignas(16) uint8_t commands[200000];
static size_t used;
static const uint32_t streams[2][2] = {{32, 0}, {52, 1}};
static const uint64_t base = 0x7200000000ull;

static CmdRec *append(uint32_t op, size_t extra) {
    CmdRec *r = (CmdRec *)(commands + used);
    assert(used + sizeof *r + extra <= sizeof commands);
    memset(r, 0, sizeof *r + extra);
    r->op = op; r->size = (uint32_t)(sizeof *r + extra);
    used += r->size;
    return r;
}
static CmdRec *draw(uint64_t indices) {
    CmdRec *r = append(CMD_DRAW_ELEMENTS, 0);
    r->x[0] = 4; r->x[1] = 1; r->x[2] = 36; r->x[3] = indices; r->x[6] = 1;
    return r;
}
static CmdRec *bind(uint64_t addr, uint64_t size) {
    CmdRec *r = append(CMD_BIND_VERTEX_BUFFERS, sizeof(VkCliffRange));
    r->x[0] = r->x[1] = 1;
    VkCliffRange range = {addr, size}; memcpy(r->extra, &range, sizeof range);
    return r;
}
static CmdRec *stream(void) {
    CmdRec *r = append(CMD_BIND_VERTEX_STREAM, sizeof streams);
    r->x[0] = 2; memcpy(r->extra, streams, sizeof streams); return r;
}

int main(void) {
    uint64_t checked = 0;
    // Vary group caps and list length; verify every source instance address and
    // final binding, not merely the scan's reported number of records.
    for (unsigned n = 2; n <= 345; n++) {
        used = 0; draw(0x7250000000ull);
        for (unsigned i = 1; i < n; i++) { bind(base + 52 * i, 40000); stream(); draw(0x7250000000ull); }
        CmdRec *end = append(CMD_END, 0);
        for (unsigned cap = 2; cap <= 64; cap += 7) {
            const CmdRec *p = (const CmdRec *)commands;
            uint64_t addr = base; unsigned consumed = 0;
            while (p != end) {
                if (p->op == CMD_BIND_VERTEX_BUFFERS) {
                    VkCliffRange v; memcpy(&v, p->extra, sizeof v); addr = v.addr;
                    p = (const CmdRec *)((const uint8_t *)p + p->size); continue;
                }
                if (p->op == CMD_BIND_VERTEX_STREAM) { p = (const CmdRec *)((const uint8_t *)p + p->size); continue; }
                assert(p->op == CMD_DRAW_ELEMENTS);
                VkCliffBatch b = vk_cliff_batch_scan(p, (VkCliffRange){addr, 40000}, streams, cap);
                for (unsigned j = 0; j < b.draws; j++) { assert(addr + j * 52 == base + consumed * 52); consumed++; checked++; }
                assert(b.last.addr == base + (consumed - 1) * 52);
                assert(b.vertex_binds == b.draws - 1 && b.stream_binds == b.draws - 1);
                addr = b.last.addr; p = b.next;
            }
            assert(consumed == n);
        }
    }
    // Every state/visibility/synchronization command is a boundary.
    for (unsigned op = 0; op < CMD_COUNT; op++) {
        if (op == CMD_BIND_VERTEX_BUFFERS || op == CMD_BIND_VERTEX_STREAM || op == CMD_DRAW_ELEMENTS) continue;
        used = 0; CmdRec *first = draw(100); CmdRec *boundary = append(op, 0); bind(base + 52, 1000); stream(); draw(100); append(CMD_END, 0);
        VkCliffBatch b = vk_cliff_batch_scan(first, (VkCliffRange){base, 1000}, streams, 64);
        assert(b.draws == 1 && b.next == boundary && b.last.addr == base);
    }
    for (unsigned fault = 0; fault < 12; fault++) {
        used = 0; CmdRec *first = draw(100); CmdRec *vb = bind(base + 52, 1000); CmdRec *vs = stream(); CmdRec *second = draw(100); append(CMD_END, 0);
        VkCliffRange initial = {base, 1000};
        if (fault == 0) second->x[3]++;
        if (fault == 1) second->x[2]++;
        if (fault == 2) second->x[4]++;
        if (fault == 3) second->x[5]++;
        if (fault == 4) second->x[6] = 2;
        if (fault == 5) { VkCliffRange v = {base + 104, 1000}; memcpy(vb->extra, &v, sizeof v); }
        if (fault == 6) { VkCliffRange v = {base + 52, 51}; memcpy(vb->extra, &v, sizeof v); }
        if (fault == 7) initial.size = 103;
        if (fault == 8) vb->x[0] = 0;
        if (fault == 9) vb->x[1] = 2;
        if (fault == 10) ((uint32_t *)vs->extra)[3] = 2;
        if (fault == 11) vb->size = sizeof *vb;
        VkCliffBatch b = vk_cliff_batch_scan(first, initial, streams, 64);
        assert(b.draws == 1 && b.next == vb && b.last.addr == base);
    }
    // A valid batch followed by an incompatible range commits only earlier binds.
    used = 0; CmdRec *first = draw(100); bind(base + 52, 1000); stream(); draw(100);
    CmdRec *uncommitted = bind(base + 156, 1000); stream(); draw(100); append(CMD_END, 0);
    VkCliffBatch b = vk_cliff_batch_scan(first, (VkCliffRange){base, 1000}, streams, 64);
    assert(b.draws == 2 && b.next == uncommitted && b.last.addr == base + 52);
    printf("PASS: %llu source instance fetches, state/sync boundaries, index changes, bounds and final state\n", (unsigned long long)checked);
}
