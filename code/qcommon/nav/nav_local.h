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
} navOmcEntry_t;

typedef struct {
	navOmcEntry_t  entries[NAV_MAX_OMC];
	int            count;
} navOmcInput_t;

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

/* Maximum polys that can be tagged per func_door entity.
 * 64 covers even wide sliding door brushes with margin. */
#define NAV_MAX_DOOR_POLYS   64
/* Maximum distinct func_door entities per map. */
#define NAV_MAX_DOORS       128

/* Per-door poly list built during Nav_TagDoorAreas.
 * Used by Nav_SetPolyFlagsForDoor to apply NAVPOLY_BLOCKED at runtime. */
typedef struct {
	char         targetname[64];
	navPolyRef_t polyrefs[NAV_MAX_DOOR_POLYS];
	int          numPolyrefs;
} navDoorEntry_t;

/* Maximum simultaneous crowd agents (bots + monsters). */
#define NAV_MAX_CROWD_AGENTS  128
/* Maximum Detour nav mesh query nodes (open/closed set size). */
#define NAV_MAX_QUERY_NODES   2048

/* Increment when serialisation format or area-type enum changes.
 * STANDALONE value in cache file header — NOT folded into paramHash. */
#define NAV_CACHE_VERSION     2

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

/* --------------------------------------------------------------------------
   Disk cache (nav_cache.c)
   -------------------------------------------------------------------------- */

/* Returns opaque mesh handle (dtNavMesh*) on hit, NULL on miss/error. */
void    *Nav_Cache_Load( const char *mapName, int bspChecksum );

/* Serialise mesh to disk.  Logs S_COLOR_YELLOW warning on write failure. */
void     Nav_Cache_Save( const char *mapName, int bspChecksum, const void *mesh );

/* 32-bit FNV-1a hash of build parameters used in the cache key. */
unsigned int Nav_Cache_ParamHash( void );

/* --------------------------------------------------------------------------
   Detour query functions (all implemented in nav_impl.cpp, called from nav_traps.c)
   -------------------------------------------------------------------------- */

int          Nav_FindPath( const float *qOrigin, const float *qGoal,
                           int agentType, navPath_t *out );
qboolean     Nav_Raycast( const float *qStart, const float *qEnd, float *qHitPos );
navPolyRef_t Nav_FindNearestPoly( const float *qOrigin, const float *qExtents );
int          Nav_GetPolyAreaFlags( navPolyRef_t polyRef );
qboolean     Nav_GetRandomPoint( int areaFilter, float *qPosOut );

/* Set or clear poly flags for all polys belonging to the named door entity.
 * Called from g_mover.c via trap at each door state transition.
 * Returns silently if targetname is NULL or not found in doorEntries. */
void         Nav_SetPolyFlagsForDoor( const char *targetname,
                                      int setFlags, int clearFlags );

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

struct bspFile_s;
void Nav_OMC_Build( const struct bspFile_s *bsp, navOmcInput_t *out );

/* --------------------------------------------------------------------------
   Trap dispatch (nav_traps.c)
   -------------------------------------------------------------------------- */

intptr_t Nav_HandleTrap( int trap, const intptr_t *args, byte *vmBase );

#ifdef __cplusplus
}
#endif

#endif /* NAV_LOCAL_H */
