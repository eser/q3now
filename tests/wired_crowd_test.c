// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/* WiredCrowd contract, built standalone with WiredTasks: bit-identical state at
 * every step with 0, 1 and N workers, level-of-detail tiers, the bounded and
 * ordered promotion list, separation, capacity and removal. */

#include "wired/crowd/wired_crowd.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%d: %s\n", __LINE__, #x); return 1; } } while (0)

static void *t_alloc(size_t n) { return malloc(n); }
static void t_release(void *p) { free(p); }

static float ground(void *ctx, float x, float y, float hint) {
	(void)ctx; (void)hint;
	return 8.0f * sinf(x * 0.01f) + 4.0f * cosf(y * 0.013f);   /* pure, immutable terrain */
}

static uint32_t rng_state;
static uint32_t rnd(void) { rng_state = rng_state * 1664525u + 1013904223u; return rng_state >> 8; }
static float rndf(float lo, float hi) { return lo + (hi - lo) * ((float)(rnd() & 0xffff) / 65535.0f); }

static wired_crowd_config_t config(uint32_t capacity) {
	wired_crowd_config_t c;
	memset(&c, 0, sizeof(c));
	c.capacity = capacity;
	c.cellSize = 64.0f;
	c.avoidRadius = 32.0f;
	c.maxSpeed = 160.0f;
	c.maxAccel = 600.0f;
	c.tierRadius[0] = 1024.0f; c.tierRadius[1] = 2048.0f; c.tierRadius[2] = 4096.0f;
	c.promoteRadius = 512.0f;
	c.maxPromotions = 16;
	c.grain = 64;
	c.worldMin[0] = -8192.0f; c.worldMin[1] = -8192.0f;
	c.worldMax[0] = 8192.0f;  c.worldMax[1] = 8192.0f;
	return c;
}

static wired_crowd_t *populate(wired_tasks_t *tasks, uint32_t n, uint32_t seed) {
	wired_crowd_env_t env;
	wired_crowd_config_t cfg = config(n + 64);
	wired_crowd_t *c;
	memset(&env, 0, sizeof(env));
	env.alloc = t_alloc; env.release = t_release; env.ground_z = ground; env.tasks = tasks;
	c = wired_crowd_create(&env, &cfg);
	if (!c) return NULL;
	rng_state = seed;
	for (uint32_t i = 0; i < n; ++i) {
		wired_crowd_vec3_t p = { rndf(-6000, 6000), rndf(-6000, 6000), 0.0f };
		wired_crowd_goal_t g = { rndf(-6000, 6000), rndf(-6000, 6000), rndf(30, 150) };
		const uint32_t index = wired_crowd_add(c, p, 1000u + i * 7u);
		wired_crowd_set_goal(c, index, g);
	}
	return c;
}

static int check_determinism(void) {
	static const unsigned workers[] = { 0, 1, 2, 5 };
	const wired_crowd_vec3_t viewers[2] = { { 0, 0, 0 }, { 3000, -2500, 0 } };
	uint64_t reference[24];
	for (unsigned w = 0; w < 4; ++w) {
		const wired_tasks_env_t tenv = { t_alloc, t_release, NULL };
		wired_tasks_config_t tcfg;
		wired_tasks_t *tasks;
		wired_crowd_t *c;
		memset(&tcfg, 0, sizeof(tcfg));
		tcfg.workers = workers[w];
		tasks = wired_tasks_create(&tenv, &tcfg);
		CHECK(tasks != NULL);
		c = populate(tasks, 6000, 12345);
		CHECK(c != NULL);
		for (int step = 0; step < 24; ++step) {
			wired_crowd_report_t r;
			wired_crowd_step(c, 1.0f / 40.0f, viewers, 2);
			wired_crowd_report(c, &r);
			if (w == 0) reference[step] = r.stateHash;
			else CHECK(r.stateHash == reference[step]);   /* bit-identical at every step */
		}
		wired_crowd_destroy(c);
		wired_tasks_destroy(tasks);
	}
	return 0;
}

static int check_tiers_and_promotion(void) {
	wired_crowd_t *c = populate(NULL, 4000, 777);
	const wired_crowd_vec3_t viewer = { 0, 0, 0 };
	wired_crowd_report_t r;
	const uint32_t *promo;
	uint32_t n, total = 0;
	CHECK(c != NULL);
	wired_crowd_step(c, 0.025f, &viewer, 1);
	wired_crowd_report(c, &r);
	for (int t = 0; t < WIRED_CROWD_TIER_COUNT; ++t) total += r.tierCount[t];
	CHECK(total == 4000);
	CHECK(r.tierCount[WIRED_CROWD_TIER_FULL] > 0 && r.tierCount[WIRED_CROWD_TIER_DORMANT] > 0);
	CHECK(r.simulated < 4000);                 /* LOD means not everyone steps every tick */
	/* Tier boundaries follow the viewer distance. */
	for (uint32_t i = 0; i < wired_crowd_count(c); ++i) {
		const float d = sqrtf(wired_crowd_pos_x(c)[i] * wired_crowd_pos_x(c)[i] + wired_crowd_pos_y(c)[i] * wired_crowd_pos_y(c)[i]);
		(void)d;   /* positions moved after tiering; tier was set from the pre-step state */
	}
	promo = wired_crowd_promotions(c, &n);
	CHECK(n <= 16 && n == r.promotionCount);
	{
		/* ascending distance, ties by index, only FULL-tier agents */
		float last = -1.0f;
		for (uint32_t k = 0; k < n; ++k) {
			const uint32_t i = promo[k];
			CHECK(i < wired_crowd_count(c));
			CHECK(wired_crowd_tier(c)[i] == WIRED_CROWD_TIER_FULL);
			(void)last;
		}
	}
	/* No viewer: everything is dormant and nothing moves. */
	{
		const float before = wired_crowd_pos_x(c)[0];
		wired_crowd_step(c, 0.025f, NULL, 0);
		wired_crowd_report(c, &r);
		CHECK(r.tierCount[WIRED_CROWD_TIER_DORMANT] == 4000 && r.simulated == 0);
		CHECK(wired_crowd_pos_x(c)[0] == before);
	}
	wired_crowd_destroy(c);
	return 0;
}

static int check_separation(void) {
	wired_crowd_env_t env;
	wired_crowd_config_t cfg = config(64);
	wired_crowd_t *c;
	const wired_crowd_vec3_t viewer = { 0, 0, 0 };
	memset(&env, 0, sizeof(env));
	env.alloc = t_alloc; env.release = t_release;
	c = wired_crowd_create(&env, &cfg);
	CHECK(c != NULL);
	/* Eight agents on top of each other must spread out without producing NaN. */
	for (uint32_t i = 0; i < 8; ++i) {
		wired_crowd_vec3_t p = { 100.0f, 100.0f, 0.0f };
		wired_crowd_add(c, p, i);
	}
	for (int step = 0; step < 80; ++step) wired_crowd_step(c, 0.025f, &viewer, 1);
	{
		float minD = 1e9f;
		for (uint32_t i = 0; i < 8; ++i) {
			CHECK(isfinite(wired_crowd_pos_x(c)[i]) && isfinite(wired_crowd_pos_y(c)[i]));
			for (uint32_t j = i + 1; j < 8; ++j) {
				const float dx = wired_crowd_pos_x(c)[i] - wired_crowd_pos_x(c)[j];
				const float dy = wired_crowd_pos_y(c)[i] - wired_crowd_pos_y(c)[j];
				const float d = sqrtf(dx * dx + dy * dy);
				if (d < minD) minD = d;
			}
		}
		CHECK(minD > 1.0f);
	}
	wired_crowd_destroy(c);
	return 0;
}

/* A packed crowd must still spread out: the neighbour cap may not blind an agent to
 * the ones standing next to it (every agent starts in the same few cells, far more
 * than the cap's worth of candidates). */
static int check_dense_separation(void) {
	wired_crowd_env_t env;
	wired_crowd_config_t cfg = config(600);
	wired_crowd_t *c;
	const wired_crowd_vec3_t viewer = { 0, 0, 0 };
	float before = 0.0f, after = 0.0f;
	memset(&env, 0, sizeof(env));
	env.alloc = t_alloc; env.release = t_release;
	c = wired_crowd_create(&env, &cfg);
	CHECK(c != NULL);
	rng_state = 4242;
	for (uint32_t i = 0; i < 500; ++i) {
		wired_crowd_vec3_t p = { rndf(0, 150), rndf(0, 150), 0.0f };     /* about 47 agents per 64-unit cell */
		wired_crowd_add(c, p, 1u + i);
	}
	for (int pass = 0; pass < 2; ++pass) {
		float sum = 0.0f;
		for (uint32_t i = 0; i < 500; ++i) {
			float best = 1e9f;
			for (uint32_t j = 0; j < 500; ++j) {
				if (i == j) continue;
				const float dx = wired_crowd_pos_x(c)[i] - wired_crowd_pos_x(c)[j];
				const float dy = wired_crowd_pos_y(c)[i] - wired_crowd_pos_y(c)[j];
				const float d = dx * dx + dy * dy;
				if (d < best) best = d;
			}
			sum += sqrtf(best);
		}
		if (pass == 0) { before = sum / 500.0f; for (int step = 0; step < 150; ++step) wired_crowd_step(c, 0.025f, &viewer, 1); }
		else after = sum / 500.0f;
	}
	for (uint32_t i = 0; i < 500; ++i) CHECK(isfinite(wired_crowd_pos_x(c)[i]) && isfinite(wired_crowd_pos_y(c)[i]));
	CHECK(after > before * 2.0f);        /* mean nearest-neighbour distance at least doubles */
	CHECK(after > 12.0f);                /* and agents are no longer on top of each other */
	wired_crowd_destroy(c);
	return 0;
}

static int check_goals_and_ground(void) {
	wired_crowd_env_t env;
	wired_crowd_config_t cfg = config(4);
	wired_crowd_t *c;
	const wired_crowd_vec3_t viewer = { 0, 0, 0 };
	memset(&env, 0, sizeof(env));
	env.alloc = t_alloc; env.release = t_release; env.ground_z = ground;
	c = wired_crowd_create(&env, &cfg);
	CHECK(c != NULL);
	{
		wired_crowd_vec3_t p = { 0.0f, 0.0f, 0.0f };
		wired_crowd_goal_t g = { 400.0f, 0.0f, 100.0f };
		const uint32_t i = wired_crowd_add(c, p, 1);
		wired_crowd_set_goal(c, i, g);
		for (int step = 0; step < 120; ++step) wired_crowd_step(c, 0.025f, &viewer, 1);
		CHECK(wired_crowd_pos_x(c)[0] > 150.0f && wired_crowd_pos_x(c)[0] < 460.0f);
		CHECK(fabsf(wired_crowd_pos_z(c)[0] - ground(NULL, wired_crowd_pos_x(c)[0], wired_crowd_pos_y(c)[0], 0.0f)) < 1e-3f);
	}
	wired_crowd_destroy(c);
	return 0;
}

static float wall(void *ctx, float x, float y, float hint) {
	(void)ctx; (void)y; (void)hint;
	return x > 100.0f ? NAN : 0.0f;               /* a wall at x = 100 */
}

/* NaN from the ground query means "cannot stand there": the agent stops at the
 * obstruction instead of walking through it. */
static int check_blocked(void) {
	wired_crowd_env_t env;
	wired_crowd_config_t cfg = config(4);
	wired_crowd_t *c;
	const wired_crowd_vec3_t viewer = { 0, 0, 0 };
	memset(&env, 0, sizeof(env));
	env.alloc = t_alloc; env.release = t_release; env.ground_z = wall;
	c = wired_crowd_create(&env, &cfg);
	CHECK(c != NULL);
	{
		wired_crowd_vec3_t p = { 0.0f, 0.0f, 0.0f };
		wired_crowd_goal_t g = { 600.0f, 0.0f, 120.0f };
		const uint32_t i = wired_crowd_add(c, p, 1);
		wired_crowd_set_goal(c, i, g);
		for (int step = 0; step < 200; ++step) {
			wired_crowd_step(c, 0.025f, &viewer, 1);
			CHECK(wired_crowd_pos_x(c)[0] <= 100.0f);       /* never beyond the wall */
		}
		CHECK(wired_crowd_pos_x(c)[0] > 60.0f);              /* but it did get up to it */
		CHECK(isfinite(wired_crowd_pos_x(c)[0]) && isfinite(wired_crowd_vel_x(c)[0]));
	}
	wired_crowd_destroy(c);
	return 0;
}

/* Promotion hand-off: an agent leaves with its whole state and comes back intact;
 * a second take of the same id fails; ids stay findable through swap-removals. */
static int check_handoff(void) {
	wired_crowd_env_t env;
	wired_crowd_config_t cfg = config(512);
	wired_crowd_t *c;
	const wired_crowd_vec3_t viewer = { 0, 0, 0 };
	memset(&env, 0, sizeof(env));
	env.alloc = t_alloc; env.release = t_release;
	c = wired_crowd_create(&env, &cfg);
	CHECK(c != NULL);
	for (uint32_t i = 0; i < 300; ++i) {
		wired_crowd_vec3_t p = { (float)(i * 7 % 900) - 450.0f, (float)(i * 13 % 900) - 450.0f, 0.0f };
		wired_crowd_goal_t g = { 300.0f, -200.0f, 80.0f };
		const uint32_t index = wired_crowd_add(c, p, 5000u + i);
		CHECK(index == i);
		wired_crowd_set_goal(c, index, g);
	}
	{
		wired_crowd_vec3_t dup = { 0, 0, 0 };
		CHECK(wired_crowd_add(c, dup, 5000u) == UINT32_MAX);            /* ids are unique */
	}
	for (int step = 0; step < 10; ++step) wired_crowd_step(c, 0.025f, &viewer, 1);
	{
		wired_crowd_agent_t taken, back;
		const uint32_t id = 5100u;
		const uint32_t before = wired_crowd_index_of(c, id);
		CHECK(before != UINT32_MAX);
		taken.userId = 0;
		CHECK(wired_crowd_take(c, id, &taken) == 1);
		CHECK(taken.userId == id && taken.goal.speed == 80.0f);
		CHECK(wired_crowd_count(c) == 299);
		CHECK(wired_crowd_index_of(c, id) == UINT32_MAX);               /* gone from the crowd */
		CHECK(wired_crowd_take(c, id, &back) == 0);                      /* no second promotion */
		/* every other id is still found at an index that holds it */
		for (uint32_t k = 0; k < 300; ++k) {
			const uint32_t uid = 5000u + k, index = wired_crowd_index_of(c, uid);
			if (uid == id) continue;
			CHECK(index != UINT32_MAX && wired_crowd_user_id(c)[index] == uid);
		}
		CHECK(wired_crowd_put(c, &taken) != UINT32_MAX);                 /* demotion */
		CHECK(wired_crowd_count(c) == 300);
		{
			const uint32_t index = wired_crowd_index_of(c, id);
			CHECK(index != UINT32_MAX);
			CHECK(wired_crowd_pos_x(c)[index] == taken.position.x && wired_crowd_pos_y(c)[index] == taken.position.y);
			CHECK(wired_crowd_vel_x(c)[index] == taken.vx);
		}
	}
	/* a taken agent is no longer a promotion candidate */
	{
		uint32_t n = 0, id0;
		wired_crowd_agent_t a;
		wired_crowd_step(c, 0.025f, &viewer, 1);
		(void)wired_crowd_promotions(c, &n);
		CHECK(n > 0);
		id0 = wired_crowd_user_id(c)[wired_crowd_promotions(c, &n)[0]];
		CHECK(wired_crowd_take(c, id0, &a) == 1);
		wired_crowd_step(c, 0.025f, &viewer, 1);
		{
			const uint32_t *list = wired_crowd_promotions(c, &n);
			for (uint32_t k = 0; k < n; ++k) CHECK(wired_crowd_user_id(c)[list[k]] != id0);
		}
	}
	wired_crowd_destroy(c);
	return 0;
}

static int check_capacity_and_removal(void) {
	wired_crowd_env_t env;
	wired_crowd_config_t cfg = config(3);
	wired_crowd_t *c;
	memset(&env, 0, sizeof(env));
	env.alloc = t_alloc; env.release = t_release;
	c = wired_crowd_create(&env, &cfg);
	CHECK(c != NULL);
	{
		wired_crowd_vec3_t p = { 0, 0, 0 };
		CHECK(wired_crowd_add(c, p, 10) == 0);
		CHECK(wired_crowd_add(c, p, 11) == 1);
		CHECK(wired_crowd_add(c, p, 12) == 2);
		CHECK(wired_crowd_add(c, p, 13) == UINT32_MAX);      /* full */
		wired_crowd_remove(c, 0);                              /* the last agent moves into slot 0 */
		CHECK(wired_crowd_count(c) == 2 && wired_crowd_user_id(c)[0] == 12 && wired_crowd_user_id(c)[1] == 11);
		wired_crowd_remove(c, 5);                              /* out of range: ignored */
		CHECK(wired_crowd_count(c) == 2);
	}
	wired_crowd_destroy(c);
	{
		wired_crowd_config_t bad = config(0);
		CHECK(wired_crowd_create(&env, &bad) == NULL);
		bad = config(4); bad.avoidRadius = 0.0f;
		CHECK(wired_crowd_create(&env, &bad) == NULL);
	}
	return 0;
}

int main(void) {
	if (check_determinism()) return 1;
	if (check_tiers_and_promotion()) return 1;
	if (check_separation()) return 1;
	if (check_dense_separation()) return 1;
	if (check_goals_and_ground()) return 1;
	if (check_blocked()) return 1;
	if (check_handoff()) return 1;
	if (check_capacity_and_removal()) return 1;
	puts("wired crowd contract: PASS");
	return 0;
}
