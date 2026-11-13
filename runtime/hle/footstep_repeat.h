#pragma once
#include <stdbool.h>
#include <stdint.h>

// One physical foot cannot land twice within the same authored stride. Keep
// this history across animation blends/restarts, independently for each foot.
typedef struct {
    uint64_t last_ns, period_ns;
    bool valid;
} FootstepRepeat;

static inline bool footstep_repeat_accept(FootstepRepeat *s, uint64_t now, uint64_t period) {
    uint64_t minimum = period;
    if (s->valid && s->period_ns < minimum) minimum = s->period_ns;
    if (s->valid && now >= s->last_ns && now - s->last_ns < minimum) return false;
    s->last_ns = now;
    s->period_ns = period;
    s->valid = true;
    return true;
}
