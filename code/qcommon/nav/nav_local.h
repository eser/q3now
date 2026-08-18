// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
===========================================================================
nav_local.h -- internal nav layer types (engine-side only, not exposed to WASM)

Include only from code/qcommon/nav/ implementation files.
Never include from game code or shared headers.
===========================================================================
*/
#ifndef NAV_LOCAL_H
#define NAV_LOCAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../q_shared.h"
#include "../q_feats.h"
#include "nav_types.h"
#include "nav_coord.h"

/* --------------------------------------------------------------------------
   Geometry types (used by nav layer and BSP format extractors via bspFormat_t)
   -------------------------------------------------------------------------- */

/*
 * Triangle soup in Recast (Y-up) coordinates, ready for rcRasterizeTriangles.
 * Each array is an independent Hunk_AllocateTempMemory block.
 * Allocation order: verts (1st), tris (2nd), areas (3rd / top of stack).
 * Free in reverse LIFO order: areas → tris → verts.
 */
typedef struct navGeom_s {
	float         *verts;   /* 3 floats/vertex, Recast Y-up coords        */
	int           *tris;    /* 3 ints/triangle, CCW winding (Recast conv) */
	unsigned char *areas;   /* 1 byte/triangle: navAreaId_t               */
	int            numVerts;
	int            numTris;
} navGeom_t;

/* --------------------------------------------------------------------------
   Off-mesh connection catalog
   -------------------------------------------------------------------------- */

/*
 * Stock Q3 pak0 entity survey (trigger_push + trigger_teleport + target_push):
 *   Phase-4 test maps: Q3DM6=3, Q3DM13=3.  Highest stock map: q3ctf4=24.
 *   64 gives 2.6× headroom over stock max; 64 × ~40 B ≈ 2.5 KB stack.
 */
#define NAV_MAX_OMC 64

typedef struct {
	float          start[3];   /* Quake-space origin of connection start */
	float          end[3];     /* Quake-space destination                */
	float          radius;     /* agent radius at endpoints              */
	unsigned char  area;       /* navAreaId_t                            */
	unsigned short flags;      /* navPolyFlags_t                         */
	unsigned char  bidir;      /* 0 = one-way, 1 = bidirectional         */
	unsigned char  traversalMode; /* navTraversalMode_t: follower actuation */
} navOmcEntry_t;

typedef struct {
	navOmcEntry_t  entries[NAV_MAX_OMC];
	int            count;
} navOmcInput_t;

/* --------------------------------------------------------------------------
   Format-neutral mover descriptor

   Produced by each map format's extractMovers adapter (BSP_Q1_ExtractMovers,
   BSP_Q3_ExtractMovers) and consumed by nav_offmesh.c buildPlatforms, which
   emits one bidirectional off-mesh connection per mover (shaftFloor <-> topLedge).
   All format divergence lives in the adapter; this layer never names a mover
   classname. Coordinates are Quake world-space units.

   Forward-declared as `struct navMoverDesc_s` in map_format_registry.h (the
   registry carries it only by pointer); this is the full definition.
   -------------------------------------------------------------------------- */

typedef enum {
	NAV_MOVER_PLATFORM = 0    /* func_plat-style elevator (upper/lower stop) */
} navMoverKind_t;

typedef struct navMoverDesc_s {
	float topLedge[3];        /* upper stop / boarding ledge (world-space)  */
	float shaftFloor[3];      /* lower stop                                 */
	float radius;             /* agent radius at endpoints                  */
	int   kind;               /* navMoverKind_t                             */
} navMoverDesc_t;

/*
 * AABB for a func_door entity, used by Nav_TagDoorAreas.
 * All values in Quake world-space units.
 */
typedef struct {
	float mins[3];
	float maxs[3];
	/* D-19: targetname key for runtime poly-flag updates (door open/close).
	 * Synthesised as "door_%d" if the entity has no targetname field.
	 * BSP entity parse order matches G_Spawn() spawn order in Q3 — both
	 * iterate the entity string sequentially — so the synthesis index is
	 * identical on both sides without any explicit coordination. */
	char  targetname[64];
} navDoorBox_t;

#if FEAT_RECAST_NAVMESH

/* Maximum polys that can be tagged per func_door entity.  This bounds BOTH the
 * queryPolygons result and the per-door stored list Nav_SetPolyFlagsForDoor walks at
 * runtime, so a door that exceeds it keeps polys that are solid but never gated —
 * a silently-untagged wall.
 *
 * 64 was sized when a door contributed only its THRESHOLD floor.  A door now also
 * contributes polys born on its OWN at-rest volume, and the tag query grows by a
 * player diameter to reach the floor that volume seals: measured on e1m1 the worst
 * doors (t1, t4) went 50/53 -> 58, i.e. 6 of headroom left at 64.  128 restores a
 * comfortable ~2x margin over the measured worst case rather than shipping on the
 * edge of a cap whose overflow mode is an invisible wall. */
#define NAV_MAX_DOOR_POLYS   128
/* Maximum distinct func_door entities per map. */
#define NAV_MAX_DOORS       128

/* Per-door poly list built during Nav_TagDoorAreas.
 * Used by Nav_SetPolyFlagsForDoor to apply NAVPOLY_BLOCKED at runtime.
 *
 * origArea[] records each poly's area BEFORE Nav_TagDoorAreas promoted it to
 * NAVAREA_DOOR, so a door OPEN can revert the 10x door-cost back to what the
 * poly actually is (a threshold floor originates NAVAREA_GROUND; a door-gap /
 * hatch crossing OMC originates NAVAREA_JUMP_LINK).  Reverting a crossing OMC to
 * GROUND would corrupt its cost key / traversal class, so the per-poly original
 * must be preserved rather than blanket-set.  Engine-internal (never serialised,
 * not in NAV_CACHE_VERSION/paramHash — runtime areas/flags already are not).
 *
 * probeSavedFlags[]/probeDepth are the FLAG half of that same idea, and they exist
 * because the AREA half shipped with a saved original while the flag half never did.
 * A caller that neutralises a flag to run a hypothetical query (WiredIntel_DoorCutsRoute
 * clears NAVPOLY_BLOCKED, probes, then re-sets it) was ASSERTING the post-state rather
 * than restoring the observed pre-state.  That assumption is wrong for doors:
 * G_Nav_ApplyDoorFlags deliberately keeps a CLOSED door BLOCKED-clear (a closed door is
 * openable, not a permanent obstacle), so the probe's "restore" SET a bit that had never
 * been set, and the door stuck at BLOCKED — which GetFilter excludes, deleting those
 * polys from every later search.  Measured: 8 of 14 doors ended a run at 0x215 instead
 * of 0x205, and the per-door set/clear counts were PERFECTLY PAIRED throughout, which is
 * why every pairing audit passed.  A balanced toggle around a wrong baseline still
 * corrupts.  Recording the observed flags at neutralise-time and putting exactly those
 * back removes the assumption entirely. */
typedef struct {
	char           targetname[64];
	navPolyRef_t   polyrefs[NAV_MAX_DOOR_POLYS];
	unsigned char  origArea[NAV_MAX_DOOR_POLYS];   /* pre-tag area per polyref (for open-revert) */
	unsigned short probeSavedFlags[NAV_MAX_DOOR_POLYS]; /* per-poly flags observed at neutralise */
	int            probeDepth;                     /* >0 while a probe neutralisation is open */
	int            numPolyrefs;
} navDoorEntry_t;

/* Maximum simultaneous crowd agents (bots + monsters). */
#define NAV_MAX_CROWD_AGENTS  128
/* Maximum Detour nav mesh query nodes (open/closed set size). */
#define NAV_MAX_QUERY_NODES   2048

/* Increment when serialisation format or area-type enum changes.
 * STANDALONE value in cache file header — NOT folded into paramHash. */
#define NAV_CACHE_VERSION     2

/* --------------------------------------------------------------------------
   Off-mesh-connection endpoint reconnect + target-poly preference.

   Some OMC endpoints (e.g. a func_plat boarding point that rests offset from
   the surrounding walkable floor) fail Detour's standard bind because the
   nearest walkable poly lies just beyond the connect radius.  A second-chance
   nearest-poly search with a wider, per-axis extent snaps such an endpoint onto
   the real floor.  These extents are folded into Nav_Cache_ParamHash so a value
   change self-invalidates the disk cache.
   -------------------------------------------------------------------------- */

/* Second-chance OMC-endpoint snap: independent horizontal (x/y Quake) and
 * vertical (z Quake) search half-extents, sized to the measured worst-case
 * plat-well offset (≈47u lateral / ≈44u vertical) plus margin. */
#define NAV_OMC_SNAP_HORIZ    52.0f
#define NAV_OMC_SNAP_VERT     50.0f

/* Wider horizontal box for the OMC *end* boarding endpoint, so it can reach
 * the adjacent walkable floor island rather than binding to the mover's own
 * (small) clip-surface island. */
#define NAV_OMC_APPROACH_HORIZ 80.0f

/* --------------------------------------------------------------------------
   Target-poly preference thresholds (isPreferredWalkInFloor).

   A nearest-poly lookup can bind to the *nearest* poly when the correct target
   is a slightly-farther poly on the reachable walk-in floor (the nearest being
   a tiny isolated ledge or a mover clip pad).  The preference prefers a
   candidate over the plain nearest only when the nearest is a tiny isolated
   island and the candidate is a large connected floor flush-to-one-step-below.
   Island size is measured by getReachablePolyCount (bounded flood, OMC-skipped).
   -------------------------------------------------------------------------- */

/* Bounded-flood cap for getReachablePolyCount (island-size measure). */
#define NAV_PREF_ISLAND_CAP   24
/* Nearest poly counts as a tiny isolated island at or below this size. */
#define NAV_PREF_ISLAND_SMALL 5
/* Candidate counts as a large walk-in floor at or above this size. */
#define NAV_PREF_ISLAND_LARGE 12
/* Candidate must be flush-to-one-step-below: never snap up more than a lip,
 * never drop a whole level.  (Vertical accept band is [-DROP_MAX, +4].) */
#define NAV_PREF_DROP_MAX     28.0f

/* Runtime goal-snap (Nav_SnapGoalToFloor) horizontal reach cap: the goal may
 * be relocated to a reachable exit-floor edge up to this far horizontally,
 * just past the measured worst-case (≈67u). */
#define NAV_GOAL_PREF_HORIZ   72.0f

/* --------------------------------------------------------------------------
   Door floor-gap off-mesh connection (nav_offmesh.c buildDoorGapOmcs).

   A solid door brush (func_door / func_door_secret) can span a real gap in the
   walkable collision floor: at the threshold the two floor components sit on
   either side of a hole the closed door blocks and the open door lets the bot
   cross.  Recast rasterizes the two sides as different components with a NULL
   band between them (a real floor gap wider than the walk-climb, not the
   erosion-diameter seam the split-floor bridge handles).  Where such a gap
   exists under a door, one off-mesh connection is emitted linking the two
   floor sides so the bot traverses the doorway as the map intends.

   The gate is on an ACTUAL floor gap: a door flush on continuous floor emits
   nothing (no gap → no OMC), so this does not over-emit one connection per
   door.  Endpoints are placed on the real floor just outside each edge of the
   gap so Detour's baseOffMeshLinks / reconnectUnlinkedOffMeshStarts bind each
   end into the correct (different) floor component.
   -------------------------------------------------------------------------- */

/* A no-floor run along the passage axis counts as a real gap only when it is
 * wider than the walk-climb (a step the mesh already joins) and no wider than
 * this cap (beyond it the door fronts a genuine chasm, not a crossable
 * threshold).  MIN is set just above NAV_WALKABLE_CLIMB (18u) so a run the mesh
 * would already join is never bridged; MAX is sized past the measured e1m1
 * doorway hole (~32u no-floor) with headroom for wider-slab doors. */
#define NAV_DOORGAP_MIN_WIDTH   20.0f   /* > NAV_WALKABLE_CLIMB (18u): a real hole, not a step */
#define NAV_DOORGAP_MAX_WIDTH   96.0f   /* beyond this the door fronts a chasm, skip */

/* Floor sampling: step size along the passage axis and the vertical window a
 * sample searches for a walkable floor triangle around the door's base z. */
#define NAV_DOORGAP_SAMPLE_STEP 8.0f
#define NAV_DOORGAP_Z_WINDOW    64.0f

/* The floor either side of the gap must be a real standable component; place
 * the endpoint this far onto the solid floor, past the gap lip. */
#define NAV_DOORGAP_ENDPOINT_INSET 12.0f

/* --------------------------------------------------------------------------
   Hatch-descent off-mesh connection (nav_offmesh.c buildHatchDescentOmcs).

   The VERTICAL sibling of the door-gap producer.  A door-gap bridges two floors
   at the SAME level separated by a horizontal hole; a hatch bridges two floors
   at DIFFERENT levels — an upper floor on one side of a door and a standable
   landing far below on/through the other (e1m1's floor-hatch t1/*3: upper start
   room z27 over a 240u shaft to the door-t1 room z-213).  Detection is a VERTICAL
   floor discontinuity at the door (upper floor beside the door footprint; a
   standable landing a genuine shaft-depth below it), NOT the door brush shape.
   Emission is ONE one-way DROP OMC per hatch: launch at the upper lip EDGE next
   to the footprint (a near-vertical fall — the measured PASS zone is within ~one
   agent radius horizontally), each candidate validated by a clear vertical column
   + a standable landing.  Door-keyed by Nav_TagDoorAreas' OMC-midpoint pass
   (closed ⇒ OPENABLE_CLOSED so the bot plans through it and opens the door).

   Runs on the MAIN THREAD in Nav_OMC_Build (CM_BoxTrace legal), like water-edge;
   the worker consumes the entry in voxel space. */

/* A hatch's two floors must differ by MORE than a walkable climb for the drop to
 * be a genuine shaft the mesh could not join by walking (same >CLIMB rule the
 * door-gap MIN_WIDTH uses for holes).  Sized to the walk-climb; no map constant. */
#define NAV_HATCH_MIN_DROP        48.0f   /* > NAV_WALKABLE_CLIMB(18): a real shaft, not a step */

/* Deepest shaft we will link (a fall past this is a chasm / a death pit, not an
 * intended descent).  Matches the generator's drop bound so both agree on "too
 * deep to be a designed drop". */
#define NAV_HATCH_MAX_DROP        320.0f  /* == |NAV_GEN_MIN_DROP|: past this is a chasm */

/* Vertical sampling step for the shaft column scan and the floor searches. */
#define NAV_HATCH_SAMPLE_STEP     4.0f

/* Standing headroom the landing needs (a low void under the shaft is not a floor). */
#define NAV_HATCH_HEADROOM        56.0f   /* NAV_WALKABLE_HEIGHT */

/* Launch lip inset: how far EITHER SIDE of the hatch footprint edge the lip
 * (outside, on upper solid floor) and the landing probe (inside, in the shaft
 * mouth) sit.  The lip→landing horizontal separation is 2*this, which must stay
 * inside the validator's measured PASS zone for a near-vertical fall (arcs with a
 * lip→landing offset beyond ~20u FAILed wall-clip).  8u ⇒ 16u separation. */
#define NAV_HATCH_LIP_INSET       8.0f

/* Endpoint resting-origin height above the collision foot (|MINS_Z|), matching
 * the water-edge endpoint convention so Detour binds the endpoint on the floor. */
#define NAV_HATCH_MINS_Z          24.0f   /* |MINS_Z| (bg_public.h MINS_Z = -24) */

/* The upper floor beside the door is searched within this vertical window of the
 * door leaf's TOP (the lip is at the upper storey, near the leaf top). */
#define NAV_HATCH_LIP_Z_WINDOW    64.0f

/* Near-vertical cap: the lip→landing horizontal offset must stay within this for
 * the fall to clear the shaft (the validator PASSed arcs up to ~20u and FAILed
 * wall-clip beyond).  2*LIP_INSET is the natural separation across the edge; this
 * cap (a touch above it) rejects any wider, wall-clipping candidate. */
#define NAV_HATCH_MAX_HOFFSET     20.0f

/* --------------------------------------------------------------------------
   Water-edge off-mesh connection (nav_offmesh.c buildWaterEdgeOmcs).

   A shallow liquid crossing between two walkable floor components.  Where the
   player would simply wade across, one off-mesh connection reconnects the two
   floor components across the liquid seam.

   HISTORICAL NOTE: this producer was originally written to compensate for a
   voxel-height defect — at NAV_CH=5 the mesh's effective climb was
   floor(NAV_WALKABLE_CLIMB=18 / 5) = 3 vox = 15u, so a staircase of ~16u Q1
   risers exceeded it and Recast fragmented a floor that was physically
   continuous.  NAV_CH is now 3 (floor(18/3) = 6 vox = 18u, exact parity with
   STEPSIZE), so riser fragmentation is no longer a source of split floor and any
   seam this producer still emits reflects a genuine liquid crossing rather than a
   quantization artefact.

   The producer runs on the MAIN THREAD in Nav_OMC_Build (before the bake
   spawns), where the collision model is loaded and CM_BoxTrace / CM_PointContents
   are legal; the worker consumes the resulting entries purely in voxel space.
   Collision is format-agnostic: the ONE cm.tracer->Trace resolves Q1 hull-1
   (via CONTENTS_PLAYERCLIP in MASK_PLAYERSOLID) AND Q3 brushes with no
   classname/format branch, and CM_PointContents(p,0) reads the liquid contents
   both formats already expose to PM_SetWaterLevel.

   A candidate reconnection (A,B) — a pair of near-boundary cells on different
   floor components — is emitted only if all four gates hold:
     1. real-floor    — both cells stand on real solid floor within a step below.
     2. clear column  — the segment between them at resting height is all
                        non-solid (no submerged wall).
     3. foot-in-water — both cells are foot-in-liquid (the dominant gate: it
                        drops every dry near-water seam, so only genuine liquid
                        crossings survive; no map-name special-casing needed).
     4. gap cap       — horizontal separation within the cap.
   The link type is HALFWAY(wade) or SUBMERGED(swim) per the PM_SetWaterLevel
   model at the deepest column point.
   -------------------------------------------------------------------------- */

/* XY grid cell for the walkable-floor field the water-edge producer rasterizes
 * from the extracted nav geom (matches the read-only over-fire measurement). */
#define NAV_WATEREDGE_CELL        8.0f

/* Recast joins cross-column floor within this climb; a riser above it fragments
 * the component (floor(NAV_WALKABLE_CLIMB/NAV_CH)*NAV_CH = 6*3).  Two cells are
 * on the same floor component only if their floor z differ by no more than this.
 * A LITERAL, not a derived expression: NAV_CH is file-static in nav_impl.cpp and
 * not visible here.  It must be re-derived by hand whenever NAV_CH changes — the
 * identity above is the recipe.  With NAV_CH=3 the mesh's effective climb equals
 * NAV_WALKABLE_CLIMB (18u) exactly, so this now matches the walk-climb itself. */
#define NAV_WATEREDGE_CLIMB       18.0f

/* Standing headroom a floor cell needs (a low-clearance void is not a floor). */
#define NAV_WATEREDGE_HEADROOM    56.0f   /* NAV_WALKABLE_HEIGHT */

/* Real-floor gate: a down-trace must hit solid within this far below the foot
 * (STEPSIZE, bg_local.h) — auto-excludes floating / fabricated-solid nav cells. */
#define NAV_WATEREDGE_STEP        18.0f

/* Foot-in-water probe offsets around the resting foot (Quake z): the cell is
 * liquid-adjacent if any is in CONTENTS_WATER/SLIME/LAVA. */
#define NAV_WATEREDGE_WATER_BAND  24.0f

/* A reconnection pair's two cells sit within this many grid cells of each other
 * (a boundary pair, not an arbitrary long link). */
#define NAV_WATEREDGE_PAIR_CELLS  4

/* Horizontal gap cap between the two endpoints (cannot link far-apart islands). */
#define NAV_WATEREDGE_GAP_MAX     256.0f

/* Endpoint placement: resting-origin height above the collision foot
 * (|MINS_Z| from bg_public.h) and the eye height above the origin
 * (DEFAULT_VIEWHEIGHT), used by the PM_SetWaterLevel link-type model. */
#define NAV_WATEREDGE_MINS_Z      24.0f   /* |MINS_Z| (bg_public.h MINS_Z = -24) */
#define NAV_WATEREDGE_VIEWH       26.0f   /* DEFAULT_VIEWHEIGHT (bg_public.h)     */

/* --------------------------------------------------------------------------
   Vertical-step jump-link off-mesh connection (Nav_Build_Internal, bake time).

   The complement of the drop-OMC detector.  The drop detector fires on ADJACENT
   poly pairs (sharing a mesh edge) whose centers differ by a large DOWNWARD step
   — a floor that drops onto another floor already reachable by walking to the
   edge.  A 90-degree vertical wall is different: erosion + the walk-climb gate
   leave the wall base and the top ledge as TWO SEPARATE floor components that
   share NO facing mesh edge (a vertical wall presents no walkable floor between
   its foot and its top), so the drop detector never sees the pair and the mesh
   never joins them.  The bot dead-ends at the wall.

   This detector bridges exactly that severed vertical step, and ONLY that:
     - flood-fill the poly-mesh neighbor graph into connected components (the
       bake-time reachability oracle — the same graph the drop detector walks);
     - find NON-adjacent poly pairs close in the horizontal (Quake XY / Recast
       XZ) plane and separated by an UPWARD vertical in the reach band;
     - accept a pair only when its two polys sit in DIFFERENT LARGE components
       (mirrors NAV_PREF_ISLAND_LARGE): the reachability gate that makes this
       emit ZERO on an arena (every arena ledge is same-component, reachable by
       walking around) and exactly one on a genuine sole-connector wall.
   One NAVAREA_JUMP_LINK OMC is emitted per severed component pair (verts placed
   LOW-side first so Detour's start endpoint binds at the foot and the follower
   rides UP to the top), bidirectional so the agent can also step back down.
   -------------------------------------------------------------------------- */

/* --------------------------------------------------------------------------
   Unified physics-link generator constants (nav_impl.cpp Nav_GeneratePhysicsLinks).
   Replaced the old vertical-step / jump-gap / drop producer heuristics + the
   sole-connector augmented-poly oracle.  The generator searches candidate boundary-
   edge pairs of DISTINCT reachable components and emits ONLY links the trajectory
   validator proves traversable -- so every constant here is physics-derived or a
   search bound, never a map-tuned magic number.  All fold into Nav_Cache_ParamHash.
   -------------------------------------------------------------------------- */

/* Max horizontal jump reach: ~2 * runSpeed(320) * t_apex(0.3375s) = 216u, the range
 * a run-jump covers from launch to landing at the same height.  A centroid pair
 * farther apart than this cannot be a run-jump and is pre-rejected before any trace. */
#define NAV_GEN_MAX_REACH        216.0f

/* Max upward rise a jump may bridge: jump ceiling (270^2/(2*800)=45.56u) + step
 * (STEPSIZE 18u) = 63.6u.  A rise beyond this exceeds the physics jump apex, so no
 * arc clears it -- pre-reject.  (Drops fall arbitrarily far; bounded by MIN_DROP.) */
#define NAV_GEN_MAX_RISE         63.6f
#define NAV_GEN_MIN_DROP        -320.0f  /* lower bound on a drop's fall, so a bottomless void is not paired */

/* Inward inset ladder from the candidate launch point (Amendment 2): a jump feasible
 * from half a step back off the ledge edge is FOUND, not lost to edge quantization.
 * Sampled at 0, 1*step, 2*step inward. */
#define NAV_GEN_INSET_STEP        12.0f
#define NAV_GEN_INSET_LADDER      3

/* Minimum component poly count to seed candidates (skip sliver fragments). */
#define NAV_GEN_COMP_MIN          4

/* OMC endpoint bind radius (matches the retired producers). */
#define NAV_GEN_LINK_RADIUS       32.0f

/* Physics-link userId classification base (retained for OmcClassify / the census /
 * the trajectory finalize).  The generator emits into the internal range
 * [NAV_MAX_OMC, NAV_JUMPGAP_USERID_BASE); this base still delimits the JUMPGAP class
 * in the classifier so the census histogram and the userId->class map stay valid. */
#define NAV_JUMPGAP_USERID_BASE   1000000u

/* --------------------------------------------------------------------------
   Nav_Impl_* opaque Detour accessors — implemented as extern "C" in nav_impl.cpp.
   nav_cache.c uses void* mesh handles throughout.
   -------------------------------------------------------------------------- */

/* dtAllocNavMesh() — returns opaque mesh handle. */
void    *Nav_Impl_AllocMesh( void );

/* dtFreeNavMesh() */
void     Nav_Impl_FreeMesh( void *mesh );

/*
 * mesh->init(data, dataSize, DT_TILE_FREE_DATA) when freeOnDealloc != 0.
 * Returns qtrue on success.
 */
qboolean Nav_Impl_InitMeshFromData( void *mesh, unsigned char *data,
                                    int dataSize, int freeOnDealloc );

/* mesh->getMaxTiles() */
int      Nav_Impl_GetMaxTiles( const void *mesh );

/*
 * Return tile data for tile index tileIdx.
 * *refLo, *refHi: lower and upper 32 bits of dtTileRef.
 * *dataSize: tile payload size (0 if tile is empty).
 * *data: pointer to tile payload (NULL if empty); owned by Detour.
 */
void     Nav_Impl_GetTileData( const void *mesh, int tileIdx,
                               unsigned int *refLo, unsigned int *refHi,
                               int *dataSize, const unsigned char **data );

/* dtAlloc(size, DT_ALLOC_PERM) — allocate tile data that Detour will own. */
void    *Nav_Impl_AllocTileData( int size );

/* dtFree(data) — release tile data from Nav_Impl_AllocTileData. */
void     Nav_Impl_FreeTileData( void *data );

/* --------------------------------------------------------------------------
   Geometry orchestration (nav_geom.c)
   -------------------------------------------------------------------------- */

/*
 * Nav_Geom_GetChecksum — fast path: BSP_Load → read checksum → BSP_Free.
 * Returns qtrue and sets *out on success.  No geometry extraction.
 */
qboolean Nav_Geom_GetChecksum( const char *mapname, int *out );

/*
 * Nav_Geom_Extract — slow path (cache miss): BSP_Load → extractNavGeometry
 * callback → Nav_OMC_Build → BSP_Free.  Allocates geomOut arrays with
 * Hunk_AllocateTempMemory (LIFO: areas → tris → verts when freeing).
 * Returns qtrue on success.
 */
qboolean Nav_Geom_Extract( const char *mapname,
                            navGeom_t *geomOut, navOmcInput_t *omcOut );

/*
 * Nav_Geom_Free — free geom arrays in LIFO order: areas → tris → verts.
 */
void     Nav_Geom_Free( navGeom_t *geom );

/*
 * Nav_Geom_HeapCopy — deep-copy an extracted geom (whose arrays live on the
 * shared main-thread Hunk TEMP stack) into a self-contained Z_Malloc heap copy
 * so a background bake thread can own it while the main thread continues.
 * Copies verts/tris/areas + numVerts/numTris; the OMC (a value struct) is copied
 * alongside by the caller. Returns qtrue on success (*out zeroed on failure).
 */
qboolean Nav_Geom_HeapCopy( const navGeom_t *src, navGeom_t *out );

/*
 * Nav_Geom_HeapFree — free a heap geom made by Nav_Geom_HeapCopy (Z_Free path,
 * not the Hunk path).
 */
void     Nav_Geom_HeapFree( navGeom_t *geom );

/*
 * Nav_OMC_Free — reset omc (currently a memset; forward-compatible with
 * future heap-allocated entries).
 */
void     Nav_OMC_Free( navOmcInput_t *omc );

/*
 * Nav_Get_DoorBoxes — parse the BSP entity string and extract world-space
 * AABBs for all func_door entities.  Returns a Z_Malloc'd array of count
 * navDoorBox_t entries; caller must Z_Free(*outBoxes).
 * Returns 0 on failure or if no func_door entities exist.
 */
int      Nav_Get_DoorBoxes( const char *mapname, navDoorBox_t **outBoxes );

/*
 * Nav_Get_SpawnPoint — parse the BSP entity string for a player-spawn entity
 * and return its world-space (Quake) origin in out[3].  Anchors nav_validate's
 * REACHABLE set on the play floor.  Returns qtrue on the first spawn found.
 */
qboolean Nav_Get_SpawnPoint( const char *mapname, float out[3] );

/* Nav_Get_AllSpawnPoints — every player-spawn origin (diagnostic); returns count. */
int      Nav_Get_AllSpawnPoints( const char *mapname, float (*out)[3], int maxOut );

/*
 * Nav_RegistryAudit — read-only report of the openable-traversal causal registry
 * for one map: every button/trigger -> targeted mover (door/plat) chain, the
 * mover's submodel AABB, and a causality class for each row (SIMPLE, MULTI_BUTTON,
 * COUNTER, SHOOTER, NO_TARGET, UNRESOLVED). Parsed from the entity lump via the
 * existing resolvers; touches no nav state. Emits [REGISTRY] log lines.
 */
void     Nav_RegistryAudit( const char *mapname );

/* --------------------------------------------------------------------------
   Disk cache (nav_cache.c)
   -------------------------------------------------------------------------- */

/* Returns opaque mesh handle (dtNavMesh*) on hit, NULL on miss/error. */
void    *Nav_Cache_Load( const char *mapName, int bspChecksum );

/* Serialise mesh to disk.  Logs S_COLOR_YELLOW warning on write failure. */
void     Nav_Cache_Save( const char *mapName, int bspChecksum, const void *mesh );

/* 32-bit FNV-1a hash of build parameters used in the cache key. */
unsigned int Nav_Cache_ParamHash( void );

/* Delete cached navmesh files.
 * mapFilter = NULL     — delete all .nav files in the navmesh directory.
 * mapFilter non-NULL   — delete the single file for that map (bare name,
 *   with or without .nav extension or directory prefix; all normalized). */
void Nav_ClearCache( const char *mapFilter );

/* --------------------------------------------------------------------------
   Detour query functions (all implemented in nav_impl.cpp, called from nav_traps.c)
   -------------------------------------------------------------------------- */

int          Nav_FindPath( const float *qOrigin, const float *qGoal,
                           int agentType, navPath_t *out );
qboolean     Nav_Raycast( const float *qStart, const float *qEnd, float *qHitPos );
navPolyRef_t Nav_FindNearestPoly( const float *qOrigin, const float *qExtents );
int          Nav_GetPolyAreaFlags( navPolyRef_t polyRef );
qboolean     Nav_GetRandomPoint( int areaFilter, float *qPosOut );

/* Most central spot a player can stand on: area-weighted centroid of the
 * reachable walkable surface, snapped to real navmesh (medoid fallback when the
 * centroid falls outside it, e.g. a doughnut map) and settled onto the floor.
 * qfalse if the mesh is not ready or has no reachable ground polys. */
qboolean     Nav_GetWalkableCenter( float *qPosOut );

/* Set or clear poly flags for all polys belonging to the named door entity.
 * Called from g_mover.c via trap at each door state transition.
 * Returns silently if targetname is NULL or not found in doorEntries. */
void         Nav_SetPolyFlagsForDoor( const char *targetname,
                                      int setFlags, int clearFlags );

/* Predict enemy position by forward-simulating velocity on the navmesh surface.
 * qOrigin/qVelocity/qPosOut in Quake world-space coordinates.
 * Simulates up to predictTime seconds in ~10ms steps using moveAlongSurface.
 * Falls through to qOrigin unchanged if navmesh is not ready. */
void         Nav_PredictEnemyPosition( const float *qOrigin, const float *qVelocity,
                                       float predictTime, float *qPosOut );

#endif /* FEAT_RECAST_NAVMESH */

/* --------------------------------------------------------------------------
   Engine-side lifecycle (called from sv_init.c via nav_public.h)
   -------------------------------------------------------------------------- */

void Nav_Init( void );
void Nav_Shutdown( void );
void Nav_LoadMap( const char *mapname );
void Nav_UnloadMap( void );
int  Nav_IsReady( void );
void Nav_Debug_RegisterCommands( void );

/* --------------------------------------------------------------------------
   Off-mesh connection builder (nav_offmesh.c)
   -------------------------------------------------------------------------- */

struct mapFile_s;
/* geom (extracted nav floor, Recast space) may be NULL; when present it is used
 * to gate + place the door floor-gap off-mesh connections on the real floor. */
void Nav_OMC_Build( const struct mapFile_s *bsp, const navGeom_t *geom,
                    navOmcInput_t *out );

/* --------------------------------------------------------------------------
   Trap dispatch (nav_traps.c)
   -------------------------------------------------------------------------- */

intptr_t Nav_HandleTrap( int trap, const intptr_t *args, byte *vmBase );

#ifdef __cplusplus
}
#endif

#endif /* NAV_LOCAL_H */
