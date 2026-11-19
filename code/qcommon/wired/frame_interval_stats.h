// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef WIRED_FRAME_INTERVAL_STATS_H
#define WIRED_FRAME_INTERVAL_STATS_H

#include <stdint.h>
#include <string.h>

/* Opt-in diagnostics only: 250us buckets through 8ms, then overflow.
 * Consecutive end-of-client timestamps include outer-loop work and pacing.
 * These are application intervals, not physical display presentation times. */
#define WIRED_FRAME_INTERVAL_BINS 33u
#define WIRED_FRAME_INTERVAL_BIN_NS UINT64_C(250000)
#define WIRED_FRAME_INTERVAL_BUDGET_NS UINT64_C(4000000)
typedef struct {
    uint64_t previousNs;
    uint64_t totalNs;
    uint64_t maxNs;
    uint32_t count;
    uint32_t invalid;
    uint32_t overBudget;
    uint32_t bins[WIRED_FRAME_INTERVAL_BINS];
} wired_frame_interval_stats_t;

static inline void wired_frame_interval_clear_window(wired_frame_interval_stats_t *s) {
    const uint64_t previous = s->previousNs;
    memset(s, 0, sizeof(*s));
    s->previousNs = previous;
}

static inline void wired_frame_interval_record(wired_frame_interval_stats_t *s,
        uint64_t now) {
    const uint64_t previous = s->previousNs;
    s->previousNs = now;
    if (!now || (previous && now <= previous)) { ++s->invalid; return; }
    if (!previous) return; /* First endpoint anchors an epoch, not a sample. */
    const uint64_t elapsed = now - previous;
    if (UINT64_MAX - s->totalNs < elapsed || s->count == UINT32_MAX) {
        ++s->invalid; return;
    }
    uint64_t bin = elapsed / WIRED_FRAME_INTERVAL_BIN_NS;
    if (bin >= WIRED_FRAME_INTERVAL_BINS) bin = WIRED_FRAME_INTERVAL_BINS - 1u;
    ++s->bins[bin]; ++s->count;
    s->totalNs += elapsed;
    if (elapsed > s->maxNs) s->maxNs = elapsed;
    if (elapsed > WIRED_FRAME_INTERVAL_BUDGET_NS) ++s->overBudget;
}

#endif
