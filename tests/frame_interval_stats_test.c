// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "wired/frame_interval_stats.h"
#include <stdio.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%d: %s\n", __LINE__, #x); return 1; } } while (0)

int main(void) {
    wired_frame_interval_stats_t s = {0};
    uint64_t now = 100;
    wired_frame_interval_record(&s, now);
    CHECK(!s.count && !s.invalid);
    wired_frame_interval_record(&s, now += 3999999);
    wired_frame_interval_record(&s, now += 4000000);
    wired_frame_interval_record(&s, now += 4000001);
    CHECK(s.count == 3 && s.overBudget == 1 && s.totalNs == 12000000);
    CHECK(s.maxNs == 4000001 && s.bins[15] == 1 && s.bins[16] == 2);
    wired_frame_interval_clear_window(&s);
    CHECK(s.previousNs == now && !s.count && !s.totalNs && !s.bins[16]);
    wired_frame_interval_record(&s, now += 12000000);
    CHECK(s.count == 1 && s.overBudget == 1 && s.bins[32] == 1);
    /* Bucket resets retain the cross-bucket interval. Epoch resets do not. */
    memset(&s, 0, sizeof(s));
    for (uint32_t i = 0; i < 200; ++i)
        wired_frame_interval_record(&s, now += 1000000);
    CHECK(s.count == 199 && s.bins[4] == 199);
    wired_frame_interval_clear_window(&s);
    for (uint32_t i = 0; i < 200; ++i)
        wired_frame_interval_record(&s, now += 1000000);
    CHECK(s.count == 200 && s.bins[4] == 200);
    wired_frame_interval_clear_window(&s);
    for (uint32_t i = 0; i < 37; ++i)
        wired_frame_interval_record(&s, now += i == 36 ? 5000000 : 1000000);
    CHECK(s.count == 37 && s.bins[4] == 36 && s.bins[20] == 1);
    CHECK(s.totalNs == 41000000 && s.maxNs == 5000000 && s.overBudget == 1);
    wired_frame_interval_record(&s, now); /* Duplicate/non-monotonic clock. */
    wired_frame_interval_record(&s, now - 1);
    CHECK(s.invalid == 2 && s.count == 37);
    memset(&s, 0, sizeof(s));
    wired_frame_interval_record(&s, 0);
    CHECK(s.invalid == 1 && !s.count);
    wired_frame_interval_record(&s, 1);
    s.totalNs = UINT64_MAX;
    wired_frame_interval_record(&s, 2);
    CHECK(s.invalid == 2 && !s.count && s.totalNs == UINT64_MAX);
    memset(&s, 0, sizeof(s));
    wired_frame_interval_record(&s, 1);
    s.count = UINT32_MAX;
    wired_frame_interval_record(&s, 2);
    CHECK(s.invalid == 1 && s.count == UINT32_MAX && !s.totalNs);
    puts("PASS: full-frame interval boundaries, windows and invalid clocks");
    return 0;
}
