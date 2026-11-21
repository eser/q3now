// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/* Contract for the pure window arithmetic in wired/workload_stats.h and for the
 * slot registry table (names, sample masks). The registry, the cvar and the log
 * rows live in workload_stats.c and are exercised by the evidence test. */

#include "wired/workload_stats.h"
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%d: %s\n", __LINE__, #x); return 1; } } while (0)

static int check_bins(void) {
	/* Below 128 ns everything is bin 0; bin edges are exact and monotonic. */
	CHECK(wired_wl_bin_for(0) == 0 && wired_wl_bin_for(127) == 0);
	CHECK(wired_wl_bin_for(128) == 1 && wired_wl_bin_for(159) == 1);
	CHECK(wired_wl_bin_for(160) == 2 && wired_wl_bin_for(255) == 4);
	CHECK(wired_wl_bin_for(256) == 5);
	unsigned previous = 0;
	for (uint64_t ns = 1; ns < (UINT64_C(1) << 40); ns += 1 + ns / 7) {
		const unsigned bin = wired_wl_bin_for(ns);
		CHECK(bin >= previous);           /* monotonic */
		CHECK(bin < WIRED_WL_BINS);
		/* the bin's lower edge is at or below the value, the next edge above it */
		CHECK(wired_wl_bin_lower_ns(bin) <= ns);
		if (bin + 1u < WIRED_WL_BINS) CHECK(ns < wired_wl_bin_lower_ns(bin + 1u));
		/* and a bin is at most a quarter octave wide (relative error bound) */
		if (bin >= 1u && bin + 1u < WIRED_WL_BINS) {
			const uint64_t lo = wired_wl_bin_lower_ns(bin), hi = wired_wl_bin_lower_ns(bin + 1u);
			CHECK((hi - lo) * 4u <= lo);
		}
		previous = bin;
	}
	/* Overflow: everything from 128 ns * 2^24 up lands in the last bin. */
	CHECK(wired_wl_bin_for(UINT64_C(1) << 31) == WIRED_WL_BINS - 1u);
	CHECK(wired_wl_bin_for(UINT64_MAX) == WIRED_WL_BINS - 1u);
	return 0;
}

static int check_window(void) {
	wired_workload_window_t w;
	wired_workload_clear(&w);
	CHECK(wired_workload_quantile_ns(&w, 99, 100) == 0);   /* nothing timed */

	/* 100 timed calls at 1 us, one at 10 ms, with a 4 ms budget. */
	for (int i = 0; i < 100; ++i) {
		wired_workload_add_call(&w, 3);
		wired_workload_add_sample(&w, 1000, 3, 4000000);
	}
	wired_workload_add_call(&w, 5);
	wired_workload_add_sample(&w, 10000000, 5, 4000000);
	CHECK(w.calls == 101 && w.items == 305);
	CHECK(w.samples == 101 && w.sampleNs == 100u * 1000u + 10000000u);
	CHECK(w.sampleItems == 305 && w.maxNs == 10000000 && w.overBudget == 1);

	/* p50 and p95 sit with the bulk, p99.5 sees the outlier; and a reported
	 * quantile never understates the truth (it is the bin's upper edge). */
	const uint64_t p50 = wired_workload_quantile_ns(&w, 50, 100);
	const uint64_t p95 = wired_workload_quantile_ns(&w, 95, 100);
	const uint64_t p99 = wired_workload_quantile_ns(&w, 99, 100);
	const uint64_t pmax = wired_workload_quantile_ns(&w, 1000, 1000);
	CHECK(p50 >= 1000 && p50 <= 1250);
	CHECK(p95 >= 1000 && p95 <= 1250);
	CHECK(p99 >= 1000 && p99 <= 1250);          /* 99% of 101 = 100 calls: still the bulk */
	CHECK(pmax >= 10000000 && pmax <= 12500000); /* the full range reaches the outlier */
	CHECK(p50 <= p95 && p95 <= p99 && p99 <= pmax);

	/* A call exactly at the budget is not over it. */
	wired_workload_add_sample(&w, 4000000, 0, 4000000);
	CHECK(w.overBudget == 1);
	/* No budget: never over. */
	wired_workload_add_sample(&w, 90000000, 0, 0);
	CHECK(w.overBudget == 1);

	/* The overflow bin reports the observed maximum, not an invented edge. */
	wired_workload_clear(&w);
	wired_workload_add_call(&w, 0);
	wired_workload_add_sample(&w, UINT64_C(5) << 31, 0, 0);
	CHECK(wired_workload_quantile_ns(&w, 99, 100) == (UINT64_C(5) << 31));

	/* Untimed calls keep exact counts and add no samples. */
	wired_workload_clear(&w);
	for (int i = 0; i < 16; ++i) wired_workload_add_call(&w, 2);
	CHECK(w.calls == 16 && w.items == 32 && !w.samples && !w.sampleNs);
	CHECK(wired_workload_quantile_ns(&w, 50, 100) == 0);

	wired_workload_clear(&w);
	CHECK(!w.calls && !w.items && !w.samples && !w.maxNs && !w.bins[0] && !w.overBudget);
	return 0;
}

static int check_registry(void) {
	static const char *const names[] = {
#define WL_NAME(id, name, mask, items) name,
		WIRED_WORKLOAD_SLOTS(WL_NAME)
#undef WL_NAME
	};
	static const unsigned masks[] = {
#define WL_MASK(id, name, mask, items) mask,
		WIRED_WORKLOAD_SLOTS(WL_MASK)
#undef WL_MASK
	};
	const unsigned count = (unsigned)(sizeof(names) / sizeof(names[0]));
	CHECK(count == (unsigned)WIRED_WL_SLOT_COUNT);
	for (unsigned i = 0; i < count; ++i) {
		CHECK(names[i][0] != '\0');
		/* a sample mask is 2^k - 1 so "calls & mask" is a modulus */
		CHECK((masks[i] & (masks[i] + 1u)) == 0u);
		for (unsigned j = i + 1u; j < count; ++j) CHECK(strcmp(names[i], names[j]) != 0);
	}
	return 0;
}

int main(void) {
	if (check_bins()) return 1;
	if (check_window()) return 1;
	if (check_registry()) return 1;
	return 0;
}
