// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/* WiredCrowdNet contract: round trip within the quantisation step, the delta
 * chain and its resync, interest management, the byte budget, rate bands,
 * malformed input and byte-identical output for identical input. */

#include "wired/crowd/wired_crowd_net.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%d: %s\n", __LINE__, #x); return 1; } } while (0)

static void *t_alloc(size_t n) { return malloc(n); }
static void t_release(void *p) { free(p); }
static const wired_crowd_net_env_t ENV = { t_alloc, t_release };

enum { N = 400 };
typedef struct { uint32_t id[N]; float x[N], y[N], z[N], vx[N], vy[N]; } world_t;

static wired_crowd_net_config_t cfg(void) {
	wired_crowd_net_config_t c;
	memset(&c, 0, sizeof(c));
	c.capacity = 1024;
	c.worldMin[0] = -8192; c.worldMin[1] = -8192; c.worldMin[2] = -512;
	c.worldMax[0] = 8192;  c.worldMax[1] = 8192;  c.worldMax[2] = 512;
	c.maxSpeed = 160.0f;
	c.tickMsec = 25.0f;
	c.rateEvery[0] = 1; c.rateEvery[1] = 2; c.rateEvery[2] = 8;
	c.bandRadius[0] = 800.0f; c.bandRadius[1] = 1600.0f; c.bandRadius[2] = 3200.0f;
	return c;
}

static uint32_t seed = 99;
static uint32_t rnd(void) { seed = seed * 1664525u + 1013904223u; return seed >> 8; }
static float rf(float lo, float hi) { return lo + (hi - lo) * ((float)(rnd() & 0xffff) / 65535.0f); }

static void fill(world_t *w) {
	seed = 99;
	for (int i = 0; i < N; ++i) {
		w->id[i] = 10u + (uint32_t)i * 3u;
		w->x[i] = rf(-3000, 3000); w->y[i] = rf(-3000, 3000); w->z[i] = rf(-20, 20);
		w->vx[i] = rf(-100, 100); w->vy[i] = rf(-100, 100);
	}
}

static void feed(wired_crowd_net_server_t *s, uint32_t tick, const world_t *w, int n) {
	wired_crowd_net_server_tick(s, tick, (uint32_t)n, w->id, w->x, w->y, w->z, w->vx, w->vy);
}

static int by_id(const void *a, const void *b) {
	const wired_crowd_net_sample_t *x = a, *y = b;
	return x->userId < y->userId ? -1 : x->userId > y->userId;
}

static wired_crowd_net_config_t wrong_box(void) {
	wired_crowd_net_config_t c = cfg();
	for (int a = 0; a < 3; ++a) { c.worldMin[a] = -1.0f; c.worldMax[a] = 1.0f; }
	c.maxSpeed = 1.0f;
	c.tickMsec = 1.0f;
	return c;
}

static int check_roundtrip_and_delta(void) {
	const wired_crowd_net_config_t c = cfg();
	const wired_crowd_net_config_t clientCfg = wrong_box();   /* only the capacity matters; the packet carries the box */
	wired_crowd_net_server_t *s = wired_crowd_net_server_create(&ENV, &c);
	wired_crowd_net_peer_t *p = wired_crowd_net_peer_create(&ENV, &c);
	wired_crowd_net_view_t *v = wired_crowd_net_view_create(&ENV, &clientCfg);
	static world_t w;
	static uint8_t pkt[65536];
	static wired_crowd_net_sample_t samples[1024];
	wired_crowd_net_encode_stats_t st;
	uint32_t tickOut = 0;
	CHECK(s && p && v);
	fill(&w);
	feed(s, 100, &w, N);
	{
		size_t n = wired_crowd_net_encode(s, p, 0, 0, 5000, 60000, pkt, sizeof(pkt), &st);
		CHECK(n > 0 && n != (size_t)-1);
		CHECK(st.agentsSent == N && st.absoluteRecords == N && st.deltaRecords == 0);   /* first packet: all absolute */
		CHECK(wired_crowd_net_view_tick_msec(v) == 1.0f);       /* before the first packet: the creation default */
		CHECK(wired_crowd_net_decode(v, pkt, n, &tickOut) == WIRED_CROWD_NET_OK && tickOut == 100);
		CHECK(wired_crowd_net_view_tick_msec(v) == 25.0f);       /* announced by the resync packet */
	}
	{
		size_t got = wired_crowd_net_view_sample(v, 100.0f, samples, 1024);
		CHECK(got == N);
		qsort(samples, got, sizeof(samples[0]), by_id);
		for (int i = 0; i < N; ++i) {
			CHECK(samples[i].userId == w.id[i]);
			CHECK(fabsf(samples[i].x - w.x[i]) < 0.2f && fabsf(samples[i].y - w.y[i]) < 0.2f);   /* 16384/65535 / 2 */
			CHECK(fabsf(samples[i].z - w.z[i]) < 0.03f);
		}
	}
	/* Unchanged world: nothing is due, nothing is sent. */
	feed(s, 101, &w, N);
	CHECK(wired_crowd_net_encode(s, p, 0, 0, 5000, 60000, pkt, sizeof(pkt), &st) == 0);
	/* Everyone moves a little: the next packet is deltas and small. */
	for (int i = 0; i < N; ++i) { w.x[i] += 5.0f; w.y[i] -= 3.0f; }
	feed(s, 102, &w, N);
	{
		size_t n = wired_crowd_net_encode(s, p, 0, 0, 5000, 60000, pkt, sizeof(pkt), &st);
		CHECK(n > 0);
		CHECK(st.deltaRecords > 0 && st.deltaRecords >= st.agentsSent / 2);
		CHECK(st.bytes < N * 9u);                     /* well under the absolute 12 bytes per agent */
		CHECK(wired_crowd_net_decode(v, pkt, n, &tickOut) == WIRED_CROWD_NET_OK);
	}
	/* Interpolation between the last two states is monotonic and bounded. */
	{
		wired_crowd_net_sample_t a, b;
		size_t ga, gb;
		ga = wired_crowd_net_view_sample(v, 100.0f, samples, 1024); (void)ga;
		a = samples[0];
		gb = wired_crowd_net_view_sample(v, 102.0f, samples, 1024); (void)gb;
		b = samples[0];
		CHECK(a.userId == b.userId);
	}
	wired_crowd_net_server_destroy(s); wired_crowd_net_peer_destroy(p); wired_crowd_net_view_destroy(v);
	return 0;
}

static int check_interest_management(void) {
	const wired_crowd_net_config_t c = cfg();
	wired_crowd_net_server_t *s = wired_crowd_net_server_create(&ENV, &c);
	wired_crowd_net_peer_t *p = wired_crowd_net_peer_create(&ENV, &c);
	wired_crowd_net_view_t *v = wired_crowd_net_view_create(&ENV, &c);
	static world_t w;
	static uint8_t pkt[65536];
	static wired_crowd_net_sample_t samples[1024];
	wired_crowd_net_encode_stats_t st;
	size_t n;
	CHECK(s && p && v);
	fill(&w);
	feed(s, 1, &w, N);
	n = wired_crowd_net_encode(s, p, 0, 0, 1500, 60000, pkt, sizeof(pkt), &st);
	CHECK(n > 0 && wired_crowd_net_decode(v, pkt, n, NULL) == WIRED_CROWD_NET_OK);
	{
		const size_t seen = wired_crowd_net_view_sample(v, 1.0f, samples, 1024);
		CHECK(seen == st.agentsSent && seen < N);
		for (size_t i = 0; i < seen; ++i) CHECK(sqrtf(samples[i].x * samples[i].x + samples[i].y * samples[i].y) <= 1500.5f);
	}
	/* Move the viewer far away: the agents that left the radius are removed. */
	feed(s, 2, &w, N);
	n = wired_crowd_net_encode(s, p, 2600, 2600, 1500, 60000, pkt, sizeof(pkt), &st);
	CHECK(n > 0 && st.removals > 0);
	CHECK(wired_crowd_net_decode(v, pkt, n, NULL) == WIRED_CROWD_NET_OK);
	{
		const size_t seen = wired_crowd_net_view_sample(v, 2.0f, samples, 1024);
		for (size_t i = 0; i < seen; ++i) {
			const float dx = samples[i].x - 2600, dy = samples[i].y - 2600;
			CHECK(sqrtf(dx * dx + dy * dy) <= 1500.5f);
		}
	}
	wired_crowd_net_server_destroy(s); wired_crowd_net_peer_destroy(p); wired_crowd_net_view_destroy(v);
	return 0;
}

static int check_budget(void) {
	const wired_crowd_net_config_t c = cfg();
	wired_crowd_net_server_t *s = wired_crowd_net_server_create(&ENV, &c);
	wired_crowd_net_peer_t *p = wired_crowd_net_peer_create(&ENV, &c);
	wired_crowd_net_view_t *v = wired_crowd_net_view_create(&ENV, &c);
	static world_t w;
	static uint8_t pkt[65536];
	static wired_crowd_net_sample_t samples[1024];
	wired_crowd_net_encode_stats_t st;
	size_t n;
	uint32_t total = 0;
	CHECK(s && p && v);
	fill(&w);
	feed(s, 1, &w, N);
	n = wired_crowd_net_encode(s, p, 0, 0, 5000, 600, pkt, sizeof(pkt), &st);
	CHECK(n > 0 && n <= 600);                           /* the budget is a hard cap */
	CHECK(st.agentsDeferred > 0 && st.agentsSent + st.agentsDeferred == st.agentsConsidered);
	CHECK(wired_crowd_net_decode(v, pkt, n, NULL) == WIRED_CROWD_NET_OK);
	{
		/* Nearest first: the 20 nearest agents are all in the first packet. (A quarter of
		 * the budget goes to the far end of the queue to keep it from starving, so only
		 * the head of the ranking is guaranteed, not a strict prefix.) */
		const size_t seen = wired_crowd_net_view_sample(v, 1.0f, samples, 1024);
		float nearD2[N];
		total = (uint32_t)seen;
		for (int i = 0; i < N; ++i) nearD2[i] = w.x[i] * w.x[i] + w.y[i] * w.y[i];
		for (int rank = 0; rank < 20; ++rank) {
			int best = -1;
			for (int i = 0; i < N; ++i) if (nearD2[i] >= 0.0f && (best < 0 || nearD2[i] < nearD2[best])) best = i;
			{
				int sent = 0;
				for (size_t k = 0; k < seen; ++k) if (samples[k].userId == w.id[best]) sent = 1;
				CHECK(sent);
			}
			nearD2[best] = -1.0f;
		}
	}
	/* Nothing is lost: repeated packets deliver the rest. */
	for (uint32_t tick = 2; tick < 200 && total < N; ++tick) {
		feed(s, tick, &w, N);
		n = wired_crowd_net_encode(s, p, 0, 0, 5000, 600, pkt, sizeof(pkt), &st);
		if (!n) break;
		CHECK(wired_crowd_net_decode(v, pkt, n, NULL) == WIRED_CROWD_NET_OK);
		total = (uint32_t)wired_crowd_net_view_sample(v, 1.0f, samples, 1024);
	}
	CHECK(total == N);
	wired_crowd_net_server_destroy(s); wired_crowd_net_peer_destroy(p); wired_crowd_net_view_destroy(v);
	return 0;
}

static int check_gap_resync_and_malformed(void) {
	const wired_crowd_net_config_t c = cfg();
	wired_crowd_net_server_t *s = wired_crowd_net_server_create(&ENV, &c);
	wired_crowd_net_peer_t *p = wired_crowd_net_peer_create(&ENV, &c);
	wired_crowd_net_view_t *v = wired_crowd_net_view_create(&ENV, &c);
	static world_t w;
	static uint8_t pkt[3][65536];
	static wired_crowd_net_sample_t before[1024], after[1024];
	size_t len[3];
	wired_crowd_net_encode_stats_t st;
	CHECK(s && p && v);
	fill(&w);
	feed(s, 1, &w, N);
	len[0] = wired_crowd_net_encode(s, p, 0, 0, 5000, 60000, pkt[0], sizeof(pkt[0]), &st);
	CHECK(wired_crowd_net_decode(v, pkt[0], len[0], NULL) == WIRED_CROWD_NET_OK);
	for (int i = 0; i < N; ++i) w.x[i] += 4.0f;
	feed(s, 2, &w, N);
	len[1] = wired_crowd_net_encode(s, p, 0, 0, 5000, 60000, pkt[1], sizeof(pkt[1]), &st);      /* "lost" */
	for (int i = 0; i < N; ++i) w.x[i] += 4.0f;
	feed(s, 3, &w, N);
	len[2] = wired_crowd_net_encode(s, p, 0, 0, 5000, 60000, pkt[2], sizeof(pkt[2]), &st);
	{
		const size_t k = wired_crowd_net_view_sample(v, 1.0f, before, 1024);
		/* a packet that skips one is rejected as a GAP and leaves the view alone */
		CHECK(wired_crowd_net_decode(v, pkt[2], len[2], NULL) == WIRED_CROWD_NET_GAP);
		CHECK(wired_crowd_net_view_sample(v, 1.0f, after, 1024) == k);
		CHECK(memcmp(before, after, k * sizeof(before[0])) == 0);
		/* truncated and garbage input is malformed and also changes nothing */
		CHECK(wired_crowd_net_decode(v, pkt[1], len[1] / 2, NULL) != WIRED_CROWD_NET_OK);
		CHECK(wired_crowd_net_decode(v, (const uint8_t *)"\x00\x01\x02\x03\x04", 5, NULL) == WIRED_CROWD_NET_MALFORMED);
		CHECK(wired_crowd_net_decode(v, pkt[1], 0, NULL) == WIRED_CROWD_NET_MALFORMED);
		CHECK(wired_crowd_net_view_sample(v, 1.0f, after, 1024) == k);
		CHECK(memcmp(before, after, k * sizeof(before[0])) == 0);
	}
	/* The client asks for a resync; the server answers with an absolute refresh. */
	wired_crowd_net_peer_resync(p);
	feed(s, 4, &w, N);
	len[0] = wired_crowd_net_encode(s, p, 0, 0, 5000, 60000, pkt[0], sizeof(pkt[0]), &st);
	CHECK(st.absoluteRecords == st.agentsSent && st.deltaRecords == 0);
	CHECK(wired_crowd_net_decode(v, pkt[0], len[0], NULL) == WIRED_CROWD_NET_OK);
	{
		const size_t k = wired_crowd_net_view_sample(v, 4.0f, after, 1024);
		qsort(after, k, sizeof(after[0]), by_id);
		CHECK(k == N);
		for (int i = 0; i < N; ++i) CHECK(fabsf(after[i].x - w.x[i]) < 0.2f);
	}
	/* and the chain continues normally afterwards */
	for (int i = 0; i < N; ++i) w.x[i] += 4.0f;
	feed(s, 5, &w, N);
	len[0] = wired_crowd_net_encode(s, p, 0, 0, 5000, 60000, pkt[0], sizeof(pkt[0]), &st);
	CHECK(wired_crowd_net_decode(v, pkt[0], len[0], NULL) == WIRED_CROWD_NET_OK);
	wired_crowd_net_server_destroy(s); wired_crowd_net_peer_destroy(p); wired_crowd_net_view_destroy(v);
	return 0;
}

static int check_rate_bands_and_determinism(void) {
	const wired_crowd_net_config_t c = cfg();
	wired_crowd_net_server_t *s1 = wired_crowd_net_server_create(&ENV, &c);
	wired_crowd_net_server_t *s2 = wired_crowd_net_server_create(&ENV, &c);
	wired_crowd_net_peer_t *p1 = wired_crowd_net_peer_create(&ENV, &c);
	wired_crowd_net_peer_t *p2 = wired_crowd_net_peer_create(&ENV, &c);
	static world_t w;
	static uint8_t a[65536], b[65536];
	wired_crowd_net_encode_stats_t st;
	uint32_t sentFar = 0, ticks = 64;
	CHECK(s1 && s2 && p1 && p2);
	fill(&w);
	/* far agents only: put everyone beyond the second band */
	for (int i = 0; i < N; ++i) { w.x[i] = 2000.0f + (float)(i % 40) * 20.0f; w.y[i] = 0.0f; }
	for (uint32_t t = 1; t <= ticks; ++t) {
		size_t na, nb;
		for (int i = 0; i < N; ++i) w.y[i] += 1.0f;                 /* everyone changes every tick */
		feed(s1, t, &w, N); feed(s2, t, &w, N);
		na = wired_crowd_net_encode(s1, p1, 0, 0, 5000, 60000, a, sizeof(a), &st);
		nb = wired_crowd_net_encode(s2, p2, 0, 0, 5000, 60000, b, sizeof(b), NULL);
		CHECK(na == nb && memcmp(a, b, na) == 0);                  /* identical input, identical bytes */
		if (t > 1) sentFar += st.agentsSent;
	}
	/* far band: one eighth of the agents per tick, on average */
	CHECK(sentFar > 0 && sentFar < (ticks - 1u) * N / 4u);
	wired_crowd_net_server_destroy(s1); wired_crowd_net_server_destroy(s2);
	wired_crowd_net_peer_destroy(p1); wired_crowd_net_peer_destroy(p2);
	return 0;
}

/* The nearest-first sampler returns exactly the nearest agents, in order. */
static int check_nearest_sampler(void) {
	const wired_crowd_net_config_t c = cfg();
	wired_crowd_net_server_t *s = wired_crowd_net_server_create(&ENV, &c);
	wired_crowd_net_peer_t *p = wired_crowd_net_peer_create(&ENV, &c);
	wired_crowd_net_view_t *v = wired_crowd_net_view_create(&ENV, &c);
	static world_t w;
	static uint8_t pkt[65536];
	static wired_crowd_net_sample_t near[64];
	size_t n;
	CHECK(s && p && v);
	fill(&w);
	feed(s, 1, &w, N);
	n = wired_crowd_net_encode(s, p, 0, 0, 6000, 60000, pkt, sizeof(pkt), NULL);
	CHECK(wired_crowd_net_decode(v, pkt, n, NULL) == WIRED_CROWD_NET_OK);
	{
		const float cx = 100.0f, cy = -200.0f, cz = 0.0f;
		float last = -1.0f;
		const size_t got = wired_crowd_net_view_sample_nearest(v, 1.0f, cx, cy, cz, near, 64);
		CHECK(got == 64);
		for (size_t i = 0; i < got; ++i) {
			const float d = sqrtf((near[i].x - cx) * (near[i].x - cx) + (near[i].y - cy) * (near[i].y - cy) + (near[i].z - cz) * (near[i].z - cz));
			CHECK(d + 8.5f >= last);             /* non-decreasing at the 8-unit bucket resolution */
			if (d > last) last = d;
		}
		/* every agent nearer than the farthest chosen one (minus the resolution) is chosen */
		for (int i = 0; i < N; ++i) {
			const float d = sqrtf((w.x[i] - cx) * (w.x[i] - cx) + (w.y[i] - cy) * (w.y[i] - cy) + (w.z[i] - cz) * (w.z[i] - cz));
			int chosen = 0;
			for (size_t k = 0; k < got; ++k) if (near[k].userId == w.id[i]) chosen = 1;
			if (d < last - 9.0f) CHECK(chosen);
		}
		CHECK(wired_crowd_net_view_sample_nearest(v, 1.0f, cx, cy, cz, near, 0) == 0);
	}
	wired_crowd_net_server_destroy(s); wired_crowd_net_peer_destroy(p); wired_crowd_net_view_destroy(v);
	return 0;
}

/* More agents in range than the client can hold: the nearest ones are kept, the
 * rest wait, and no packet is ever rejected for capacity. */
static int check_view_capacity(void) {
	wired_crowd_net_config_t c = cfg();
	wired_crowd_net_config_t clientCfg;
	wired_crowd_net_server_t *s;
	wired_crowd_net_peer_t *p;
	wired_crowd_net_view_t *v;
	static world_t w;
	static uint8_t pkt[65536];
	static wired_crowd_net_sample_t samples[1024];
	c.viewCapacity = 100;
	clientCfg = c; clientCfg.capacity = 100;          /* the client really holds 100 */
	s = wired_crowd_net_server_create(&ENV, &c);
	p = wired_crowd_net_peer_create(&ENV, &c);
	v = wired_crowd_net_view_create(&ENV, &clientCfg);
	CHECK(s && p && v);
	fill(&w);
	for (uint32_t tick = 1; tick <= 80; ++tick) {
		for (int i = 0; i < N; ++i) { w.x[i] += (tick & 1u) ? 2.0f : -2.0f; }
		feed(s, tick, &w, N);
		{
			const size_t n = wired_crowd_net_encode(s, p, 0, 0, 6000, 4000, pkt, sizeof(pkt), NULL);
			if (n) CHECK(wired_crowd_net_decode(v, pkt, n, NULL) == WIRED_CROWD_NET_OK);   /* never rejected */
		}
		CHECK(wired_crowd_net_view_count(v) <= 100);
	}
	CHECK(wired_crowd_net_view_count(v) == 100);       /* full, with the nearest first */
	{
		const size_t got = wired_crowd_net_view_sample(v, 80.0f, samples, 1024);
		float farthest = 0.0f;
		int nearerOutside = 0;
		for (size_t i = 0; i < got; ++i) {
			const float d = sqrtf(samples[i].x * samples[i].x + samples[i].y * samples[i].y);
			if (d > farthest) farthest = d;
		}
		for (int i = 0; i < N; ++i) {
			const float d = sqrtf(w.x[i] * w.x[i] + w.y[i] * w.y[i]);
			int held = 0;
			for (size_t k = 0; k < got; ++k) if (samples[k].userId == w.id[i]) held = 1;
			if (!held && d < farthest * 0.4f) ++nearerOutside;
		}
		CHECK(nearerOutside == 0);                       /* nobody much nearer than what is held waits outside */
	}
	wired_crowd_net_server_destroy(s); wired_crowd_net_peer_destroy(p); wired_crowd_net_view_destroy(v);
	return 0;
}

/* Near agents that change every tick must not starve far agents of the budget:
 * everything in range is delivered within a bounded number of ticks. */
static int check_no_starvation(void) {
	const wired_crowd_net_config_t c = cfg();
	wired_crowd_net_server_t *s = wired_crowd_net_server_create(&ENV, &c);
	wired_crowd_net_peer_t *p = wired_crowd_net_peer_create(&ENV, &c);
	wired_crowd_net_view_t *v = wired_crowd_net_view_create(&ENV, &c);
	static world_t w;
	static uint8_t pkt[65536];
	static wired_crowd_net_sample_t samples[1024];
	uint32_t farDelivered = 0, tickAllDelivered = 0;
	CHECK(s && p && v);
	seed = 5;
	for (int i = 0; i < N; ++i) {
		const int near = i < 100;
		const float r = near ? rf(20, 300) : rf(2200, 3200);
		const float a = rf(0, 6.2831853f);
		w.id[i] = 1u + (uint32_t)i; w.x[i] = r * cosf(a); w.y[i] = r * sinf(a); w.z[i] = 0;
		w.vx[i] = w.vy[i] = 0;
	}
	for (uint32_t tick = 1; tick <= 150; ++tick) {
		size_t n;
		for (int i = 0; i < 100; ++i) w.x[i] += (tick & 1u) ? 3.0f : -3.0f;       /* every near agent changes every tick */
		feed(s, tick, &w, N);
		n = wired_crowd_net_encode(s, p, 0, 0, 4000, 500, pkt, sizeof(pkt), NULL);   /* ~35 agents per packet */
		if (n) CHECK(wired_crowd_net_decode(v, pkt, n, NULL) == WIRED_CROWD_NET_OK);
		{
			const size_t got = wired_crowd_net_view_sample(v, (float)tick, samples, 1024);
			farDelivered = 0;
			for (size_t k = 0; k < got; ++k) if (samples[k].userId > 100u) ++farDelivered;
			if (farDelivered == (uint32_t)(N - 100) && !tickAllDelivered) tickAllDelivered = tick;
		}
	}
	CHECK(farDelivered == (uint32_t)(N - 100));
	CHECK(tickAllDelivered > 0 && tickAllDelivered <= 100);
	wired_crowd_net_server_destroy(s); wired_crowd_net_peer_destroy(p); wired_crowd_net_view_destroy(v);
	return 0;
}

/* The shared grid must select exactly what a full scan would. */
static int check_interest_matches_brute_force(void) {
	const wired_crowd_net_config_t c = cfg();
	wired_crowd_net_server_t *s = wired_crowd_net_server_create(&ENV, &c);
	static world_t w;
	static uint8_t pkt[65536];
	CHECK(s);
	fill(&w);
	for (int trial = 0; trial < 20; ++trial) {
		wired_crowd_net_peer_t *p = wired_crowd_net_peer_create(&ENV, &c);
		wired_crowd_net_encode_stats_t st;
		const float vx = rf(-4000, 4000), vy = rf(-4000, 4000), radius = rf(100, 3500);
		uint32_t inside = 0;
		for (int i = 0; i < N; ++i) {
			const float dx = w.x[i] - vx, dy = w.y[i] - vy;
			if (dx * dx + dy * dy <= radius * radius) ++inside;
		}
		feed(s, (uint32_t)(trial + 1), &w, N);
		(void)wired_crowd_net_encode(s, p, vx, vy, radius, 60000, pkt, sizeof(pkt), &st);
		CHECK(st.agentsConsidered == inside);
		wired_crowd_net_peer_destroy(p);
	}
	wired_crowd_net_server_destroy(s);
	return 0;
}

/* Several clients encoded at once from one server: nothing is shared mutably, so
 * the packets are byte-identical to the serial ones (run under ThreadSanitizer). */
typedef struct {
	wired_crowd_net_server_t *server;
	wired_crowd_net_peer_t *peer;
	float x, y;
	uint8_t packet[65536];
	size_t length;
} job_t;

static void *encode_job(void *p) {
	job_t *j = (job_t *)p;
	j->length = wired_crowd_net_encode(j->server, j->peer, j->x, j->y, 2500, 4000, j->packet, sizeof(j->packet), NULL);
	return NULL;
}

static int check_concurrent_encode(void) {
	enum { CLIENTS = 6 };
	const wired_crowd_net_config_t c = cfg();
	wired_crowd_net_server_t *s = wired_crowd_net_server_create(&ENV, &c);
	static world_t w;
	static job_t serial[CLIENTS], parallel[CLIENTS];
	pthread_t threads[CLIENTS];
	CHECK(s);
	fill(&w);
	for (int i = 0; i < CLIENTS; ++i) {
		serial[i].server = parallel[i].server = s;
		serial[i].peer = wired_crowd_net_peer_create(&ENV, &c);
		parallel[i].peer = wired_crowd_net_peer_create(&ENV, &c);
		serial[i].x = parallel[i].x = -2000.0f + (float)i * 700.0f;
		serial[i].y = parallel[i].y = 500.0f - (float)i * 300.0f;
	}
	for (uint32_t tick = 1; tick <= 12; ++tick) {
		for (int i = 0; i < N; ++i) { w.x[i] += w.vx[i] * 0.025f; w.y[i] += w.vy[i] * 0.025f; }
		feed(s, tick, &w, N);
		for (int i = 0; i < CLIENTS; ++i) encode_job(&serial[i]);
		for (int i = 0; i < CLIENTS; ++i) CHECK(pthread_create(&threads[i], NULL, encode_job, &parallel[i]) == 0);
		for (int i = 0; i < CLIENTS; ++i) pthread_join(threads[i], NULL);
		for (int i = 0; i < CLIENTS; ++i) {
			CHECK(serial[i].length == parallel[i].length);
			CHECK(memcmp(serial[i].packet, parallel[i].packet, serial[i].length) == 0);
		}
	}
	for (int i = 0; i < CLIENTS; ++i) { wired_crowd_net_peer_destroy(serial[i].peer); wired_crowd_net_peer_destroy(parallel[i].peer); }
	wired_crowd_net_server_destroy(s);
	return 0;
}

static int check_bad_config(void) {
	wired_crowd_net_config_t c = cfg();
	c.capacity = 0;
	CHECK(wired_crowd_net_server_create(&ENV, &c) == NULL);
	c = cfg(); c.maxSpeed = 0;
	CHECK(wired_crowd_net_server_create(&ENV, &c) == NULL);
	c = cfg(); c.rateEvery[1] = 0;
	CHECK(wired_crowd_net_server_create(&ENV, &c) == NULL);
	return 0;
}

int main(void) {
	if (check_roundtrip_and_delta()) return 1;
	if (check_interest_management()) return 1;
	if (check_budget()) return 1;
	if (check_gap_resync_and_malformed()) return 1;
	if (check_rate_bands_and_determinism()) return 1;
	if (check_interest_matches_brute_force()) return 1;
	if (check_concurrent_encode()) return 1;
	if (check_no_starvation()) return 1;
	if (check_nearest_sampler()) return 1;
	if (check_view_capacity()) return 1;
	if (check_bad_config()) return 1;
	puts("wired crowd net contract: PASS");
	return 0;
}
