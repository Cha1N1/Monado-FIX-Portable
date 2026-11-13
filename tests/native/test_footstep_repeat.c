// Focused marker/ABI regression. No game, audio device or player data required.
// clang -D_GNU_SOURCE -ffunction-sections -fdata-sections -Wl,--gc-sections \
//   tests/native/test_footstep_repeat.c -pthread -lm -o /tmp/test-footsteps
#include <assert.h>
#include "../../runtime/hle/party.c"

Settings g_settings;
static uint64_t clock_ns = 1000000000;
static uint64_t holder[16];
uint64_t plat_now_ns(void) { return clock_ns; }
bool diag_deterministic(void) { return false; }
uint64_t diag_now_ns(void) { return clock_ns; }
void guest_call(Cpu *c, uint64_t fn) {
    assert(fn == 0x7000593120ull);
    c->x[0] = (uint64_t)(uintptr_t)holder;
}
static void q(void *p, size_t off, uint64_t v) { memcpy((char *)p + off, &v, 8); }
static void w(void *p, size_t off, uint32_t v) { memcpy((char *)p + off, &v, 4); }
static void f(void *p, size_t off, float v) { memcpy((char *)p + off, &v, 4); }

int main(void) {
    _Alignas(16) uint8_t action[0x600] = {0}, actor[0x100] = {0}, owner[16] = {0}, context[64] = {0};
    _Alignas(16) uint8_t group[0x800] = {0}, bindings[32] = {0}, node[0xa0] = {0}, clip[32] = {0}, header[48] = {0};
    _Alignas(16) uint8_t resource[32] = {0}, state[32] = {0}, states[0xa0] = {0}, keys[192] = {0}, stack[4096];
    uint8_t *left = keys + 12, *right = keys + 56, *restarted = keys + 100;
    Cpu c = {0}; c.sp = ((uint64_t)(uintptr_t)(stack + sizeof stack)) & ~15ull;
    c.x[19] = 0x1234; c.x[20] = 0x5678; c.x[30] = 0x9abc;
    Cpu saved = c;
    q(action, 0x38, (uintptr_t)actor); w(actor, 0x34, 64);
    q(action, 8, (uintptr_t)owner); q(owner, 8, (uintptr_t)context); q(context, 0x38, 1);
    q(holder, 0x40, (uintptr_t)group + 8);
    q(group, 0x730, 3); w(group, 0x6a0, 62); w(group, 0x6d0, 64); w(group, 0x700, 66);
    q(action, 0x78, (uintptr_t)bindings); q(action, 0x80, 1); q(bindings, 8, (uintptr_t)node);
    q(node, 0x18, (uintptr_t)clip); q(clip, 0x18, (uintptr_t)header);
    node[0x94] = 3; f(node, 0x88, 1); f(header, 0x28, 1.0f / 30); w(header, 0x2c, 31);
    q(action, 0x58, (uintptr_t)resource); q(action, 0x1d8, (uintptr_t)state);
    q(resource, 0x10, (uintptr_t)states); q(resource, 0x18, 1); q(states, 8, (uintptr_t)keys);
    keys[0] = 4; keys[4] = 8; keys[6] = 0x28; // Skip an authored control record.
    left[0] = right[0] = 44; left[2] = right[2] = 0xf;
    left[8] = 2; right[8] = 17; restarted[2] = 1; // Native list terminator.
    memcpy(left + 0x1c, "JLft_L", 7); memcpy(right + 0x1c, "JLft_R", 7);
    g_settings.fps = 120;
#define ACCEPT(key) xcde_footstep_marker(&c, (uintptr_t)action, (uintptr_t)(key))
    assert(ACCEPT(left));
    assert(!ACCEPT(left));
    clock_ns += 10000000;
    assert(!ACCEPT(left)); // Restart of the same animation marker.
    assert(ACCEPT(right));
    assert(!memcmp(&c, &saved, sizeof c)); // Read-only guest lookup preserves all CPU state.
    clock_ns = 2000000000;
    assert(ACCEPT(left)); // A complete authored stride re-arms the foot.
    f(node, 0x88, 2); clock_ns += 500000000;
    assert(ACCEPT(left)); // Faster authored playback changes the stride immediately.
    memcpy(restarted, left, 44); restarted[8] = 17; keys[146] = 1;
    clock_ns += 250000000;
    assert(ACCEPT(left)); // Multiple authored contacts for one foot in a clip remain valid.
    clock_ns += 10000000;
    assert(!ACCEPT(restarted)); // Cross-marker restart still shares that foot's history.
    w(actor, 0x34, 62);
    assert(ACCEPT(left) && ACCEPT(left)); // Leader is untouched.
    w(actor, 0x34, 99);
    assert(ACCEPT(left) && ACCEPT(left)); // Actors outside the deployed party are untouched.
    w(actor, 0x34, 64); g_settings.fps = 30;
    assert(ACCEPT(left) && ACCEPT(left)); // Preserve the original 30 FPS path.
    g_settings.fps = 120; left[0xa] = 2;
    assert(ACCEPT(left)); // Non-contact effect markers pass through.
    left[0xa] = 0; node[0x94] = 0;
    assert(ACCEPT(left)); // Non-looping actions pass through.
    node[0x94] = 2; f(node, 0x88, NAN);
    assert(ACCEPT(left)); // Unrecognized/invalid clip metadata fails open.
    f(node, 0x88, 2); left[0] = 0;
    assert(ACCEPT(left)); // Malformed marker lengths fail open without looping.
    puts("Footstep marker repeat guard: OK");
}
