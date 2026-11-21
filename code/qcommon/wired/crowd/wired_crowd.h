// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
===========================================================================
WiredCrowd -- storage and stepping for large populations of simple agents
(capacity roadmap doc-1, Phase 3).

A standalone engine module: it knows nothing of gentities, the game module or
the renderer. The engine owns the population (positions, velocities, tiers); the
game decides what an agent wants (a goal point, a speed) and which of them become
real entities near the player.

Design rules
  * Structure of arrays. The step touches one field at a time over all agents, so
    it streams through memory instead of chasing an object per agent.
  * Double-buffered state. A step reads the previous state and writes the next one;
    an agent's result depends only on the previous state, never on the order in
    which agents are processed, so it is bit-identical with 0, 1 or N WiredTasks
    workers. Neighbour queries walk a spatial hash whose cell contents are in
    ascending agent-index order, so even the order of floating-point additions is
    fixed.
  * Simulation level of detail. Agents near a viewer step every tick; farther
    ones step every 2nd / 8th tick with a proportionally larger dt; the farthest
    are dormant (they keep their slot and are counted, not simulated). Dormant
    agents are the despawn candidates of a world that holds more people than it
    can afford to simulate.
  * Promotion. The step reports the nearest agents inside the promotion radius,
    in a fixed order and at most `maxPromotions`, so the game can turn exactly
    those into full gentities (full Q3 mechanics and prediction) and hand the rest
    to the cheap crowd channel.
  * No allocation after create; no engine includes. Allocator, ground query and
    scheduler are injected.
===========================================================================
*/

#ifndef WIRED_CROWD_H
#define WIRED_CROWD_H

#include <stddef.h>
#include <stdint.h>

#include "../tasks/wired_tasks.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct wired_crowd wired_crowd_t;

typedef enum {
	WIRED_CROWD_TIER_FULL = 0,   /* every tick                          */
	WIRED_CROWD_TIER_HALF,       /* every 2nd tick, dt x2               */
	WIRED_CROWD_TIER_FAR,        /* every 8th tick, dt x8               */
	WIRED_CROWD_TIER_DORMANT,    /* not simulated                       */
	WIRED_CROWD_TIER_COUNT
} wired_crowd_tier_t;

typedef struct {
	void *( *alloc )( size_t bytes );
	void  ( *release )( void *memory );
	/* Height of the walkable ground at (x, y), starting from a hint. Called from
	 * worker threads: it must be a pure read of immutable data. NULL keeps agents
	 * at their current height. NaN means "an agent cannot stand there" (a wall, a
	 * ledge too high to step up, the void): the step then keeps the agent where it
	 * was and stops it, so it slides along the obstruction instead of walking
	 * through it. */
	float ( *ground_z )( void *ctx, float x, float y, float hintZ );
	void   *ground_ctx;
	wired_tasks_t *tasks;   /* optional; NULL steps on the calling thread */
} wired_crowd_env_t;

typedef struct {
	uint32_t capacity;        /* maximum number of agents                          */
	float    cellSize;        /* spatial hash cell edge; at least the avoid radius */
	float    avoidRadius;     /* separation distance                               */
	float    maxSpeed;        /* units per second                                  */
	float    maxAccel;        /* units per second squared                          */
	float    tierRadius[3];   /* viewer distance bounds of FULL, HALF, FAR         */
	float    promoteRadius;   /* viewer distance inside which an agent is promotable */
	uint32_t maxPromotions;   /* cap on promotion candidates per step              */
	uint32_t grain;           /* agents per task chunk (0 = 256)                   */
	uint32_t maxNeighbors;    /* candidates one agent examines for separation (0 = 48): bounds the
	                           * cost in a dense crowd to O(agents x this) however packed it is */
	float    worldMin[2], worldMax[2];   /* the hash covers this rectangle         */
} wired_crowd_config_t;

typedef struct { float x, y, z; } wired_crowd_vec3_t;

/* What the game asked for. Goals are optional per agent. */
typedef struct {
	float x, y;
	float speed;     /* desired speed; 0 = stand */
} wired_crowd_goal_t;

wired_crowd_t *wired_crowd_create( const wired_crowd_env_t *env, const wired_crowd_config_t *config );
void           wired_crowd_destroy( wired_crowd_t *crowd );

/* Add an agent; returns its stable index, or UINT32_MAX when full. */
uint32_t wired_crowd_add( wired_crowd_t *crowd, wired_crowd_vec3_t position, uint32_t userId );

/* Remove an agent. The last agent moves into its slot, so indices are only
 * stable between removals; userId is the stable identity. */
void     wired_crowd_remove( wired_crowd_t *crowd, uint32_t index );

uint32_t wired_crowd_count( const wired_crowd_t *crowd );

/* One agent's complete state: what leaves the crowd when the game promotes it to a
 * real entity and what comes back when it is demoted. */
typedef struct {
	uint32_t           userId;
	wired_crowd_vec3_t position;
	float              vx, vy;
	wired_crowd_goal_t goal;
} wired_crowd_agent_t;

/* Current index of an agent by its stable id, or UINT32_MAX. */
uint32_t wired_crowd_index_of( const wired_crowd_t *crowd, uint32_t userId );

/* Hand-off. take removes the agent and returns its state (false when the id is not
 * in the crowd, so a double promotion cannot duplicate it); put adds it back with
 * that state, keeping its id. Both are main-thread calls, between steps. */
int      wired_crowd_take( wired_crowd_t *crowd, uint32_t userId, wired_crowd_agent_t *out );
uint32_t wired_crowd_put( wired_crowd_t *crowd, const wired_crowd_agent_t *agent );
void     wired_crowd_set_goal( wired_crowd_t *crowd, uint32_t index, wired_crowd_goal_t goal );

/* Advance the simulation by dtSeconds. `viewers` are the points that drive the
 * level of detail and the promotion list (the players). */
void wired_crowd_step( wired_crowd_t *crowd, float dtSeconds,
		const wired_crowd_vec3_t *viewers, uint32_t viewerCount );

/* Read access to the state after the last step. */
const float    *wired_crowd_pos_x( const wired_crowd_t *crowd );
const float    *wired_crowd_pos_y( const wired_crowd_t *crowd );
const float    *wired_crowd_pos_z( const wired_crowd_t *crowd );
const float    *wired_crowd_vel_x( const wired_crowd_t *crowd );
const float    *wired_crowd_vel_y( const wired_crowd_t *crowd );
const uint8_t  *wired_crowd_tier( const wired_crowd_t *crowd );
const uint32_t *wired_crowd_user_id( const wired_crowd_t *crowd );

/* Result of the last step. */
typedef struct {
	uint32_t tierCount[WIRED_CROWD_TIER_COUNT];
	uint32_t simulated;        /* agents actually stepped this tick               */
	uint32_t promotionCount;   /* entries in the list below                       */
	uint64_t stateHash;        /* hash of the post-step state, for determinism tests */
} wired_crowd_report_t;

void wired_crowd_report( const wired_crowd_t *crowd, wired_crowd_report_t *out );

/* Indices of promotion candidates from the last step: nearest viewer distance
 * ascending, ties broken by index. Valid until the next step. */
const uint32_t *wired_crowd_promotions( const wired_crowd_t *crowd, uint32_t *count );

#ifdef __cplusplus
}
#endif

#endif
