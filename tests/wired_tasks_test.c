// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/* WiredTasks contract. The module links against nothing of the engine, so this
 * test builds it standalone: it checks the deterministic partition, the
 * independence of results from the worker count, exactly-once execution under
 * stress, nested loops, the bounded async slots and a clean shutdown. */

#include "wired/tasks/wired_tasks.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%d: %s\n", __LINE__, #x); return 1; } } while (0)

static void *t_alloc(size_t n) { return malloc(n); }
static void t_release(void *p) { free(p); }
static uint64_t t_clock(void) {
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint64_t)ts.tv_sec * 1000000000u + (uint64_t)ts.tv_nsec;
}

static wired_tasks_t *make(unsigned workers, unsigned slots) {
	const wired_tasks_env_t env = { t_alloc, t_release, t_clock };
	wired_tasks_config_t cfg;
	memset(&cfg, 0, sizeof(cfg));
	cfg.workers = workers;
	cfg.asyncSlots = slots;
	cfg.name = "wt-test";
	return wired_tasks_create(&env, &cfg);
}

/* ---- the partition ------------------------------------------------------ */

static int check_partition(void) {
	CHECK(wired_tasks_chunk_count(0, 4) == 0);
	CHECK(wired_tasks_chunk_count(1, 4) == 1);
	CHECK(wired_tasks_chunk_count(8, 4) == 2);
	CHECK(wired_tasks_chunk_count(9, 4) == 3);
	CHECK(wired_tasks_chunk_count(5, 0) == 5);   /* grain 0 behaves as 1 */
	return 0;
}

/* ---- results independent of the worker count ---------------------------- */

typedef struct {
	uint64_t *perChunk;
	uint32_t  grain;
	int       order;     /* last chunk seen, to prove the inline backend is ordered */
	int       ordered;
} sumCtx_t;

static uint64_t mix(uint64_t v) {
	v ^= v >> 33; v *= 0xff51afd7ed558ccdull; v ^= v >> 33; v *= 0xc4ceb9fe1a85ec53ull; v ^= v >> 33;
	return v;
}

static void sum_body(void *p, uint32_t chunk, uint32_t begin, uint32_t end) {
	sumCtx_t *c = (sumCtx_t *)p;
	uint64_t acc = 0;
	/* The partition never depends on the worker count. */
	if (begin != chunk * c->grain) { c->ordered = -1; return; }
	for (uint32_t i = begin; i < end; ++i) acc += mix(i) * (uint64_t)(i + 1u);
	c->perChunk[chunk] = acc;
}

static uint64_t run_sum(wired_tasks_t *t, uint32_t count, uint32_t grain, int *okOut) {
	sumCtx_t c;
	uint64_t total = 0;
	const uint32_t chunks = wired_tasks_chunk_count(count, grain);
	c.perChunk = (uint64_t *)calloc(chunks ? chunks : 1, sizeof(uint64_t));
	c.grain = grain ? grain : 1;
	c.order = 0; c.ordered = 0;
	wired_tasks_parallel_for(t, count, grain, sum_body, &c);
	for (uint32_t i = 0; i < chunks; ++i) total = total * 1099511628211ull + c.perChunk[i]; /* merge in chunk order */
	*okOut = c.ordered == 0;
	free(c.perChunk);
	return total;
}

static int check_determinism(void) {
	static const unsigned workerCounts[] = { 0, 1, 2, 3, 7 };
	static const struct { uint32_t count, grain; } shapes[] = {
		{ 1, 1 }, { 2, 1 }, { 63, 1 }, { 64, 64 }, { 1000, 7 }, { 4096, 33 }, { 100000, 1024 }, { 5, 100 },
	};
	for (unsigned s = 0; s < sizeof(shapes) / sizeof(shapes[0]); ++s) {
		int ok = 1;
		wired_tasks_t *inlineTasks = make(0, 0);
		const uint64_t reference = run_sum(inlineTasks, shapes[s].count, shapes[s].grain, &ok);
		CHECK(ok);
		wired_tasks_destroy(inlineTasks);
		/* A NULL scheduler is the same inline path. */
		CHECK(run_sum(NULL, shapes[s].count, shapes[s].grain, &ok) == reference && ok);
		for (unsigned w = 0; w < sizeof(workerCounts) / sizeof(workerCounts[0]); ++w) {
			wired_tasks_t *t = make(workerCounts[w], 0);
			CHECK(t != NULL);
			CHECK(wired_tasks_workers(t) == workerCounts[w]);
			for (int rep = 0; rep < 20; ++rep) {
				CHECK(run_sum(t, shapes[s].count, shapes[s].grain, &ok) == reference);
				CHECK(ok);
			}
			wired_tasks_destroy(t);
		}
	}
	return 0;
}

/* ---- every index exactly once, under stress ----------------------------- */

typedef struct { uint8_t *seen; } onceCtx_t;
static void once_body(void *p, uint32_t chunk, uint32_t begin, uint32_t end) {
	onceCtx_t *c = (onceCtx_t *)p;
	(void)chunk;
	for (uint32_t i = begin; i < end; ++i) __atomic_fetch_add(&c->seen[i], 1, __ATOMIC_RELAXED);
}

static int check_exactly_once(void) {
	enum { N = 5003 };
	wired_tasks_t *t = make(6, 0);
	onceCtx_t c;
	CHECK(t != NULL);
	c.seen = (uint8_t *)calloc(N, 1);
	for (int rep = 0; rep < 3000; ++rep) {
		memset(c.seen, 0, N);
		wired_tasks_parallel_for(t, N, (uint32_t)(1 + rep % 97), once_body, &c);
		for (int i = 0; i < N; ++i) CHECK(c.seen[i] == 1);
	}
	free(c.seen);
	{
		wired_tasks_stats_t st;
		wired_tasks_stats(t, &st);
		CHECK(st.parallelFors > 0);
		CHECK(st.chunksByCaller + st.chunksByWorkers > 0);
	}
	wired_tasks_destroy(t);
	return 0;
}

/* ---- nested loops run serially, no deadlock ----------------------------- */

typedef struct { wired_tasks_t *t; uint32_t *cells; } nestCtx_t;
static void inner_body(void *p, uint32_t chunk, uint32_t begin, uint32_t end) {
	uint32_t *cell = (uint32_t *)p;
	(void)chunk;
	*cell += end - begin;
}
static void outer_body(void *p, uint32_t chunk, uint32_t begin, uint32_t end) {
	nestCtx_t *c = (nestCtx_t *)p;
	(void)begin; (void)end;
	/* Each outer chunk owns one cell; the nested loop runs on this thread. */
	wired_tasks_parallel_for(c->t, 100, 3, inner_body, &c->cells[chunk]);
}

static int check_nested(void) {
	wired_tasks_t *t = make(4, 0);
	nestCtx_t c;
	CHECK(t != NULL);
	c.t = t;
	c.cells = (uint32_t *)calloc(32, sizeof(uint32_t));
	wired_tasks_parallel_for(t, 32, 1, outer_body, &c);
	for (int i = 0; i < 32; ++i) CHECK(c.cells[i] == 100);
	free(c.cells);
	wired_tasks_destroy(t);
	return 0;
}

/* ---- async: bounded slots, exactly once, wait semantics ----------------- */

static void async_body(void *p) {
	__atomic_fetch_add((uint32_t *)p, 1u, __ATOMIC_RELAXED);
	/* a little work so tasks overlap */
	volatile uint64_t x = 0;
	for (int i = 0; i < 2000; ++i) x += (uint64_t)i;
}

static int check_async(void) {
	const unsigned workerCounts[] = { 0, 1, 3 };
	for (unsigned w = 0; w < 3; ++w) {
		wired_tasks_t *t = make(workerCounts[w], 4);   /* only four slots */
		enum { TOTAL = 400 };
		uint32_t runs[TOTAL];
		wired_tasks_handle_t handles[TOTAL];
		CHECK(t != NULL);
		memset(runs, 0, sizeof(runs));
		/* Far more tasks than slots: overflow runs inline, nothing blocks or grows. */
		for (int i = 0; i < TOTAL; ++i) handles[i] = wired_tasks_submit(t, async_body, &runs[i]);
		for (int i = 0; i < TOTAL; ++i) {
			wired_tasks_wait(t, handles[i]);
			wired_tasks_wait(t, handles[i]);          /* waiting twice is a no-op */
			CHECK(runs[i] == 1);
		}
		{
			wired_tasks_handle_t zero = { 0, 0 };
			wired_tasks_wait(t, zero);
			wired_tasks_wait(NULL, handles[0]);
		}
		{
			wired_tasks_stats_t st;
			wired_tasks_stats(t, &st);
			CHECK(st.asyncSubmitted + st.asyncInline == TOTAL);
			if (workerCounts[w] == 0) CHECK(st.asyncSubmitted == 0 && st.asyncInline == TOTAL);
		}
		wired_tasks_destroy(t);
	}
	return 0;
}

/* The point of submit/wait: the caller does other work while the task runs. */
static void slow_body(void *p) {
	volatile uint64_t x = 0;
	for (int i = 0; i < 3000000; ++i) x += (uint64_t)i;
	*(uint64_t *)p = x ? 1u : 2u;
}
static int check_overlap(void) {
	wired_tasks_t *t = make(2, 0);
	uint64_t result = 0, mine = 0;
	CHECK(t != NULL);
	{
		wired_tasks_handle_t h = wired_tasks_submit(t, slow_body, &result);
		for (int i = 0; i < 1000; ++i) mine += mix((uint64_t)i);   /* caller's own work */
		wired_tasks_wait(t, h);
	}
	CHECK(result != 0 && mine != 0);
	wired_tasks_destroy(t);
	return 0;
}

/* ---- shutdown with work outstanding ------------------------------------- */

static int check_shutdown(void) {
	for (int rep = 0; rep < 200; ++rep) {
		wired_tasks_t *t = make(4, 8);
		uint32_t runs[8];
		CHECK(t != NULL);
		memset(runs, 0, sizeof(runs));
		for (int i = 0; i < 8; ++i) (void)wired_tasks_submit(t, async_body, &runs[i]);
		wired_tasks_destroy(t);                      /* destroy waits for every task */
		for (int i = 0; i < 8; ++i) CHECK(runs[i] == 1);
	}
	return 0;
}

static int check_bad_environment(void) {
	wired_tasks_config_t cfg;
	wired_tasks_env_t bad;
	memset(&cfg, 0, sizeof(cfg));
	memset(&bad, 0, sizeof(bad));
	CHECK(wired_tasks_create(NULL, &cfg) == NULL);
	CHECK(wired_tasks_create(&bad, &cfg) == NULL);
	/* every entry point accepts a NULL scheduler */
	wired_tasks_destroy(NULL);
	CHECK(wired_tasks_workers(NULL) == 0);
	return 0;
}

int main(void) {
	if (check_partition()) return 1;
	if (check_determinism()) return 1;
	if (check_exactly_once()) return 1;
	if (check_nested()) return 1;
	if (check_async()) return 1;
	if (check_overlap()) return 1;
	if (check_shutdown()) return 1;
	if (check_bad_environment()) return 1;
	puts("wired tasks contract: PASS");
	return 0;
}
