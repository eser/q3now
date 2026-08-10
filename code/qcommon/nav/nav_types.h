// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
===========================================================================
nav_types.h -- public nav layer type definitions

These types are shared between the engine nav layer (code/qcommon/nav/)
and WASM game code (code/game/) via the trap interface.

Memory lifetime: all navMesh state is level-scoped.  Nav_LoadMap()
allocates it; Nav_UnloadMap() frees it.  Pointers to nav data are
invalid after map unload.

This header is pure C so it can be included by both C and C++ code.
===========================================================================
*/
#ifndef NAV_TYPES_H
#define NAV_TYPES_H

/* Opaque Detour polygon reference (matches dtPolyRef = unsigned int). */
typedef unsigned int navPolyRef_t;

/* Agent type — determines which set of walkable parameters to use when
 * querying the navmesh.  Add entries here as new monster sizes are
 * defined in the AI framework. */
typedef enum {
	NAV_AGENT_PLAYER = 0,   /* Standard Q3 player capsule (15 Q3u radius, 56 Q3u height) */
	NAV_AGENT_SMALL,        /* Reserved: small creatures (radius ~10 Q3u) */
	NAV_AGENT_LARGE,        /* Reserved: large creatures (radius ~24 Q3u) */
	NAV_AGENT_COUNT
} navAgentType_t;

/* How many agent sizes are actually baked at map load. Only PLAYER is used by any
 * production consumer (bots + the behavior FSM + map-placed monsters all path on agent
 * 0); SMALL/LARGE are reserved and consumed by nothing, and an unbuilt size cleanly
 * falls back to the player mesh (Nav_QueryForAgent). Baking all NAV_AGENT_COUNT sizes at
 * load tripled the (multi-second) bake for zero benefit, so only the live sizes are baked
 * — the reserved sizes stay defined and buildable on demand, just not at every load. */
#define NAV_AGENT_LIVE_COUNT 1

/* Per-agent movement parameters used when querying path corridors. */
typedef struct {
	float radius;           /* capsule half-width in Q3 units */
	float height;           /* capsule full height in Q3 units */
	float maxClimb;         /* max step height in Q3 units */
	float maxSlope;         /* max walkable slope in degrees */
	float maxJumpDown;      /* max safe drop distance in Q3 units */
} navAgentParams_t;

/* Default agent params matching standard Q3 player physics.
 * Numeric derivation in docs/nav/01-recast-study.md §11. */
#define NAV_DEFAULT_AGENT { 15.0f, 56.0f, 18.0f, 45.0f, 64.0f }

/* Maximum waypoints returned in a single path query. */
#define NAV_MAX_PATH_POINTS 256

/* Waypoint flag bits written into navPath_t.flags[].
 * These match Detour DT_STRAIGHTPATH_* values so engine-side code can copy
 * the bits directly.  Gamecode reads these through the trap interface without
 * including DetourNavMeshQuery.h. */
#define NAV_PATHFLAG_START          0x01   /* DT_STRAIGHTPATH_START */
#define NAV_PATHFLAG_END            0x02   /* DT_STRAIGHTPATH_END */
#define NAV_PATHFLAG_OFFMESH_CON    0x04   /* DT_STRAIGHTPATH_OFFMESH_CONNECTION */

/* Encoding of the Nav_FindPath / trap_Nav_FindPath scalar return value.
 *
 * The return is the waypoint count in its low bits (0..NAV_MAX_PATH_POINTS, so it
 * never exceeds 256), OR-ed with NAV_FINDPATH_PARTIAL in a high bit when Detour
 * reported the corridor did NOT reach the goal (DT_PARTIAL_RESULT: the goal poly is
 * unreachable, or the corridor was truncated at NAV_MAX_PATH_POINTS). A return of 0
 * still means "no corridor at all". This packs the reach-status into the existing
 * scalar return, keeping navPath_t layout and the trap signature unchanged (the
 * struct crosses the WASM/native boundary, so no field may be added). Consumers that
 * only care about "any corridor" keep testing `ret <= 0`; the partial bit sits well
 * above the count so a partial result is still a positive value that those consumers
 * follow exactly as before. NAV_FINDPATH_COUNT() recovers the waypoint count. */
#define NAV_FINDPATH_PARTIAL        (1 << 16)
#define NAV_FINDPATH_COUNT(ret)     ((ret) & (NAV_FINDPATH_PARTIAL - 1))

/* Off-mesh-connection TRAVERSAL MODE — how the FOLLOWER must physically cross a
 * link.  Mode is NOT derivable from geometry (jump-pad, door-gap and flat-plat are
 * byte-identical in {area, Δz, length}) and OmcClassify only distinguishes the
 * mechanism KIND, not the follower actuation.  So each PRODUCER stamps this byte at
 * emission (it knows its own class), and it rides the waypoint to the follower.
 *   BALLISTIC — jump-pad / target-push / teleporter: the pad's own velocity or an
 *               instant teleport carries the bot; the follower suppresses its move
 *               for the whole transit (the one class the pre-mode code got right).
 *   FALL      — hatch-descent / physics-gen DROP: a gravity fall off a lip; the
 *               follower drives off the edge (phase A) then suppresses while airborne
 *               (phase B) so gravity lands it.
 *   WALK      — door-gap / water-edge wade / FLAT func_plat: a level crossing no
 *               launch carries; the follower WALKS to the destination.
 *   RIDE      — func_plat that MOVES the rider vertically: board, then hold while the
 *               plat mover translates the standing rider, step off at the top.
 *   CLIMB     — physics-gen up-jump (dest above start): a run-jump the follower
 *               times (heading alignment, run-momentum gate, one-shot jump at the
 *               step base).  The actuation IS implemented — g_bot_nav.c's CLIMB case.
 * 0 (NONE) is a non-OMC waypoint.
 *
 * These modes are stamped ONCE per link, from the baked start->end rise, but a mesh
 * off-mesh connection is BIDIRECTIONAL.  The gravity-relative pair therefore depends
 * on which way the link is being walked: CLIMB one way is FALL the other.  The path
 * query resolves that per traversal (see the direction-aware lookup in Nav_FindPath);
 * WALK, RIDE and BALLISTIC are direction-symmetric and need no flip. */
typedef enum {
	NAV_TM_NONE      = 0,
	NAV_TM_BALLISTIC = 1,
	NAV_TM_FALL      = 2,
	NAV_TM_WALK      = 3,
	NAV_TM_RIDE      = 4,
	NAV_TM_CLIMB     = 5,
} navTraversalMode_t;

/* Result of a path query.  positions[] are in Quake world-space. */
typedef struct {
	float         positions[NAV_MAX_PATH_POINTS][3];  /* waypoints in Q3 units */
	unsigned char flags[NAV_MAX_PATH_POINTS];          /* NAV_PATHFLAG_* bits per waypoint */
	int           count;                               /* number of valid waypoints */
#if FEAT_RECAST_NAVMESH
	/* Detour poly refs for each waypoint — used for area-flag queries
	 * (crouch detection, door cost) without a second mesh lookup.
	 * Populated by Nav_FindPath from findStraightPath straightRefs[].
	 * Guarded to avoid 1 KB overhead per slot in AAS builds (64 KB total
	 * at MAX_CLIENTS=64 avoided when FEAT_RECAST_NAVMESH=0). */
	navPolyRef_t  polyrefs[NAV_MAX_PATH_POINTS];
	/* Per-waypoint OMC traversal mode (navTraversalMode_t); NAV_TM_NONE on a
	 * non-OMC waypoint.  Producer-stamped at bake, resurfaced by Nav_FindPath from
	 * the OMC userId side-table.  Mirrors polyrefs[] (same WASM-boundary rebuild). */
	unsigned char traversalMode[NAV_MAX_PATH_POINTS];
#endif
} navPath_t;

/* ── Area type IDs ─────────────────────────────────────────────────────
 * Stored in rcPolyMesh::areas[] (unsigned char, 0–63 for Recast) and
 * in dtNavMesh per-polygon area field.  Area IDs are MUTUALLY EXCLUSIVE
 * — one polygon has exactly one area type at a time.
 *
 * Values 0–11 are defined here.  Values 12–63 are reserved for future
 * use.  Changing any ID value requires bumping NAV_CACHE_VERSION.
 *
 * See docs/nav/01-recast-study.md §8 for the canonical reference table.
 */
typedef enum {
	NAVAREA_NULL          = 0,   /* impassable / blocked (maps to Recast RC_NULL_AREA) */
	NAVAREA_GROUND        = 1,   /* standard walkable floor */
	NAVAREA_WATER         = 2,   /* swimming volume (path cost penalty) */
	NAVAREA_DOOR          = 3,   /* near func_door — may be blocked at runtime */
	NAVAREA_JUMP_LINK     = 4,   /* off-mesh connection: jump pad endpoint */
	NAVAREA_TELEPORT      = 5,   /* off-mesh connection: teleporter endpoint */
	NAVAREA_LAVA          = 6,   /* lava or slime (walkable with damage cost) */
	NAVAREA_LOW_CEILING   = 7,   /* crouch required (clearance < standing height) */
	/* Reserved for monster AI framework: */
	NAVAREA_PATROL_NODE   = 8,
	NAVAREA_COVER_NODE    = 9,
	NAVAREA_FLEE_NODE     = 10,
	NAVAREA_SPAWN_VOLUME  = 11,
} navAreaId_t;

/* ── Poly flags ────────────────────────────────────────────────────────
 * Stored in rcPolyMesh::flags[] (unsigned short) and in Detour's
 * per-polygon flags field.  Flags are a BITFIELD — multiple may be set
 * simultaneously (e.g., NAVPOLY_WALKABLE | NAVPOLY_DOOR).
 *
 * Two flags are runtime-mutable (set/cleared via trap_Nav_SetPolyFlagsForDoor
 * when a mover changes state): NAVPOLY_BLOCKED and NAVPOLY_OPENABLE_CLOSED.
 * BLOCKED means "not traversable" (the query filter excludes it) — used for
 * permanent obstacles (finalize-disabled lying polys) and, historically, closed
 * doors. OPENABLE_CLOSED means "closed now but the bot can open it" — a door the
 * path should plan THROUGH at high cost rather than route around; the query
 * filter includes it. Both are runtime-only: neither participates in the bake,
 * the cache serialization, or the param hash.
 */
typedef enum {
	NAVPOLY_WALKABLE        = (1 << 0),  /* standard walkable (default for all passable polys) */
	NAVPOLY_WATER           = (1 << 1),  /* water volume */
	NAVPOLY_DOOR            = (1 << 2),  /* near door entity */
	NAVPOLY_OFFMESH         = (1 << 3),  /* off-mesh connection endpoint */
	NAVPOLY_BLOCKED         = (1 << 4),  /* runtime obstacle: not traversable (filter excludes) */
	NAVPOLY_LAVA            = (1 << 5),  /* lava or slime */
	NAVPOLY_COVER           = (1 << 6),  /* cover node (monster AI) */
	NAVPOLY_PATROL          = (1 << 7),  /* patrol waypoint (monster AI) */
	NAVPOLY_LOW_CEILING     = (1 << 8),  /* crouch required */
	NAVPOLY_OPENABLE_CLOSED = (1 << 9),  /* closed door the bot can open: plan through at high cost */
} navPolyFlags_t;

#endif /* NAV_TYPES_H */
