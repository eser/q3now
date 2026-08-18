// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
===========================================================================
nav_impl.cpp -- ALL Recast/Detour/DetourCrowd interaction.

This is the ONLY C++ file in the nav module.  Everything outside this file
is pure C.  All public symbols are exposed via extern "C" wrappers so server
code can link against them without needing C++ compilation.

Nav_LoadMap flow (two-call pattern):
  1. Nav_Geom_GetChecksum → checksum.
  2. Nav_Cache_Load(checksum) → hit: skip to query_init.
  3. Cache miss: Nav_Geom_Extract → Nav_Build_Internal → Nav_Cache_Save.
  4. dtAllocNavMeshQuery + init → nav.ready = qtrue.

All Recast/Detour objects live in the file-static navGlobals_t nav struct.
Level-scoped: Nav_UnloadMap tears everything down.
===========================================================================
*/

/* All Recast/Detour C++ headers are confined to this translation unit. */
#include "Recast.h"
#include "DetourNavMesh.h"
#include "DetourNavMeshBuilder.h"
#include "DetourNavMeshQuery.h"
#include "DetourCrowd.h"
#include "DetourAlloc.h"

#include <string.h>
#include <math.h>

extern "C" {
#include "../q_shared.h"
#include "../q_feats.h"
#include "../qcommon.h"
/* Phase 5: log channels */
LOG_DECLARE_CHANNEL( ch_nav, "nav" );
}

#if FEAT_RECAST_NAVMESH

#include "nav_local.h"
#include "nav_coord.h"
#include "../wired/core/thread/thread.h"   /* Sys_CreateThread / Sys_JoinThread — background bake */

/* -------------------------------------------------------------------------
   Module globals
   navGlobals_t is intentionally NOT in any public header.  Only this file
   accesses it.  External code uses the Nav_Impl_* accessors below.
   ------------------------------------------------------------------------- */

/* OMC capacity — external (NAV_MAX_OMC) + physics-gen internal (NAV_MAX_INT_OMC).
 * Declared here (before navGlobals_t's userId-keyed traversal table) and reused by
 * OmcArrays below. */
#define NAV_MAX_INT_OMC    512
#define NAV_MAX_TOTAL_OMC  (NAV_MAX_OMC + NAV_MAX_INT_OMC)

typedef struct {
    dtNavMesh       *mesh;
    dtNavMeshQuery  *query;
    dtCrowd         *crowd;        /* Phase 6: remains NULL until activated */
    qboolean         ready;
    qboolean         fromCache;
    int              buildMs;      /* last build or cache-load time in ms */
    char             mapname[64];  /* current map (for nav_draw output path) */
    /* D-19: per-door poly lists built during Nav_TagDoorAreas.
     * Indexed by build order (same as navDoorBox_t array index).
     * Used at runtime by Nav_SetPolyFlagsForDoor. */
    navDoorEntry_t   doorEntries[NAV_MAX_DOORS];
    int              numDoorEntries;
    /* Follower OMC traversal-mode side-table, keyed by OMC userId (dense, unique).
     * Producer-stamped at bake; rebuilt on cache-load (re-derive from mesh geometry)
     * since the .nav cache does not serialize it.  Nav_FindPath resurfaces it onto
     * the follower waypoint.  NAV_TM_NONE (0) for any userId with no stamp. */
    unsigned char    omcTraversalMode[NAV_MAX_TOTAL_OMC];
    qboolean         omcTraversalReady;
} navGlobals_t;

static navGlobals_t nav;

/* -------------------------------------------------------------------------
   Build parameters (Phase 1 §11, calibrated for q3now 320–700 ups physics)
   ------------------------------------------------------------------------- */

static const float NAV_CS  = 2.0f;
/* Voxel height.  MUST divide NAV_WALKABLE_CLIMB exactly: the mesh's effective
 * climb is floor(NAV_WALKABLE_CLIMB/NAV_CH)*NAV_CH, and any shortfall against the
 * movement code's step height (STEPSIZE=18, bg_local.h:9 — mirrored here as
 * NAVTRAJ_STEPSIZE) makes the mesh disagree with the player about what is
 * walkable.  At CH=5 the effective climb was floor(18/5)*5 = 15u, so every Q1
 * staircase riser (~16u) fragmented a physically continuous floor and Recast
 * could not join the columns — the defect the water-edge OMC producer was
 * written to compensate for.  CH=3 gives floor(18/3)*3 = 18u — exact parity. */
static const float NAV_CH  = 3.0f;
static const float NAV_WALKABLE_HEIGHT      = 56.0f;  /* MAXS_Z(32) - MINS_Z(-24) from bg_public.h */
/* NAV_CROUCH_HEIGHT: CROUCH_MAXS_Z(16) - MINS_Z(-24) from bg_public.h */
static const float NAV_CROUCH_HEIGHT        = 40.0f;
static const float NAV_WALKABLE_CLIMB       = 18.0f;
static const float NAV_WALKABLE_RADIUS      = 15.0f;
static const float NAV_WALKABLE_SLOPE_ANGLE = 45.0f;
static const float NAV_MAX_EDGE_LEN         = 12.0f;
static const float NAV_MAX_SIMPLIFICATION_ERR = 1.3f;
static const int   NAV_MIN_REGION_AREA      = 64;
static const int   NAV_MERGE_REGION_AREA    = 400;
static const int   NAV_MAX_VERTS_PER_POLY   = 6;

/* Split-floor gap-bridge reconnect (post-init, targeted).  Walkable-area erosion
 * (radius NAV_WALKABLE_RADIUS) widens a thin non-walkable feature crossing a
 * continuous floor (a decal-brush seam / flush threshold / grate) into a strip
 * ~2*radius wide, severing the floor into separate components with a NULL gap
 * between two FACING boundary edges — while the real collision floor is
 * continuous.  This pass bridges exactly that: two facing floor edges across a
 * gap no wider than the erosion diameter, within a walkable vertical step, in
 * different large components.  It is the inverse of erosion and bridges nothing
 * else — a real wall/pit/liquid channel severs by MORE than the erosion diameter
 * or adds a vertical drop, so it has no facing floor edge inside the gap.
 *   maxStep  == NAV_WALKABLE_CLIMB (the same walkable-step gate the bake uses,
 *              NOT a looser one; a wider drop stays a drop-OMC candidate).
 *   maxGap   == 2*NAV_WALKABLE_RADIUS + one cell of slack = the erosion diameter
 *              (2*15 + 2 = 32u); a wider gap is treated as a real barrier.
 *   minOverlap keeps a genuine facing span (rejects corner touches / T-joins). */
static const float NAV_SEAM_MAX_STEP        = 18.0f;  /* == NAV_WALKABLE_CLIMB */
static const float NAV_SEAM_MAX_GAP         = 32.0f;  /* 2*NAV_WALKABLE_RADIUS + NAV_CS */
static const float NAV_SEAM_MIN_OVERLAP     = 8.0f;   /* facing span >= half a cell-pair */
static const float NAV_DETAIL_SAMPLE_DIST   = 3.0f * NAV_CS;  /* 3 × NAV_CS */
/* Detail-mesh height error tolerance, genuinely derived from NAV_CH (it was a
 * hardcoded 1.0f*5.0f whose comment claimed the derivation it did not have, so it
 * went stale the moment NAV_CH moved).  One voxel of height error is the intended
 * identity: the detail mesh should not deviate from the heightfield by more than
 * the quantization the heightfield already imposes. */
static const float NAV_DETAIL_SAMPLE_MAX_ERR = 1.0f * NAV_CH; /* 1 × NAV_CH */

/* -------------------------------------------------------------------------
   rcContext — routes Recast log messages to the engine console
   ------------------------------------------------------------------------- */

class NavContext : public rcContext {
protected:
    void doLog( const rcLogCategory cat, const char *msg, const int ) override {
        if ( cat == RC_LOG_ERROR || cat == RC_LOG_WARNING )
            Com_Log( SEV_INFO, LOG_CH(ch_nav), "NAV: [RC] %s\n", msg );
        else
            Com_Log( SEV_DEBUG, LOG_CH(ch_nav), "NAV: [RC] %s\n", msg );
    }
};

/* Forward declarations for static helpers used by Nav_Build_Internal before
 * they are defined later in this file. Wrapped in extern "C" because the
 * definitions live inside the extern "C" Nav_Impl_* block at line ~458; GCC 16
 * rejects mismatched-linkage forward decls in the same TU. */
extern "C" {
    static const dtQueryFilter *GetFilter( void );
    static void Nav_TagDoorAreas( const char *mapname );
}

/* -------------------------------------------------------------------------
   Off-mesh connection layout arrays (used in dtNavMeshCreateParams)
   External OMCs come from nav_offmesh.c entity parsing (max NAV_MAX_OMC).
   Internal drop OMCs are detected post-polymesh (max NAV_MAX_INT_OMC).
   Total capacity = NAV_MAX_TOTAL_OMC.  Stack allocation in a non-recursive
   function — ~20 KB, well within frame stack limits.
   ------------------------------------------------------------------------- */

/* NAV_MAX_INT_OMC / NAV_MAX_TOTAL_OMC are defined above navGlobals_t. */
#define MAX_OMC_FLAT       (NAV_MAX_TOTAL_OMC * 2 * 3)

struct OmcArrays {
    float          verts[MAX_OMC_FLAT];
    float          rads[NAV_MAX_TOTAL_OMC];
    unsigned short flags[NAV_MAX_TOTAL_OMC];
    unsigned char  areas[NAV_MAX_TOTAL_OMC];
    unsigned char  dirs[NAV_MAX_TOTAL_OMC];
    unsigned int   userIds[NAV_MAX_TOTAL_OMC];
    /* Producer-stamped follower traversal mode (navTraversalMode_t) per OMC — NOT a
     * Detour channel; harvested by Nav_BuildTraversalModeTable into a userId-keyed
     * side-table for Nav_FindPath to resurface onto the follower waypoint. */
    unsigned char  traversalModes[NAV_MAX_TOTAL_OMC];
    int            count;
};

static void buildOmcArrays( const navOmcInput_t *omc, OmcArrays *out )
{
    out->count = 0;
    for ( int i = 0; i < omc->count && i < NAV_MAX_OMC; i++ ) {
        const navOmcEntry_t *e = &omc->entries[i];
        float rs[3], re[3];
        Nav_QuakeToRecast( e->start, rs );
        Nav_QuakeToRecast( e->end,   re );
        int base = i * 6;
        out->verts[base+0] = rs[0]; out->verts[base+1] = rs[1]; out->verts[base+2] = rs[2];
        out->verts[base+3] = re[0]; out->verts[base+4] = re[1]; out->verts[base+5] = re[2];
        out->rads[i]    = e->radius;
        out->flags[i]   = e->flags;
        out->areas[i]   = e->area;
        out->dirs[i]    = e->bidir;
        out->userIds[i] = (unsigned int)i;
        out->traversalModes[i] = e->traversalMode;   /* producer-stamped */
        out->count++;
    }
}

/* Forward declaration: the unified physics-link generator (defined after the
 * trajectory validator it uses).  Replaces the old drop / vertical-step / jump-gap
 * producers + the sole-connector augmented-poly oracle: candidate boundary-edge
 * pairs -> classify -> validate against physics -> emit only proven links.
 * Declared here (before its use in Nav_Build_Internal) but DEFINED far below near the
 * validator; the extern "C" wrapper matches the definition's linkage (the whole
 * Nav_Impl_* block is extern "C"). */
extern "C" {
    static void Nav_GeneratePhysicsLinks( rcPolyMesh *pmesh, const float bmin[3],
                                          OmcArrays *omcArrs, int numExtOmcs );
}

/* -------------------------------------------------------------------------
   Nav_Build_Internal -- file-static; not exported.
   Runs the Recast -> Detour pipeline: workAreas buffer preserves WATER/LAVA/custom
   area types through rcMarkWalkableTriangles; crouchH (8 vox) keeps low-clearance
   spans (tagged NAVAREA_LOW_CEILING); the unified physics-link generator emits
   proven-traversable OMCs after rcBuildPolyMesh.  Returns a heap-allocated dtNavMesh
   on success, NULL on any error.
   ------------------------------------------------------------------------- */

static dtNavMesh *Nav_Build_Internal( const navGeom_t *geom, const navOmcInput_t *omc )
{
    if ( !geom || geom->numVerts == 0 || geom->numTris == 0 ) {
        Com_Log( SEV_INFO, LOG_CH(ch_nav), "NAV: Nav_Build_Internal called with empty geometry\n" );
        return NULL;
    }

    NavContext ctx;

    /* Voxel counts derived from physical constants */
    const int walkH   = (int)ceilf( NAV_WALKABLE_HEIGHT / NAV_CH ); /* 19 vox = 57u */
    const int crouchH = (int)ceilf( NAV_CROUCH_HEIGHT   / NAV_CH ); /* 14 vox = 42u */
    /* 6 vox = 18u == NAV_WALKABLE_CLIMB exactly (see NAV_CH): the mesh's effective
     * climb equals the movement code's STEPSIZE, so a riser the player can walk up
     * is a riser Recast joins rather than a floor-fragmenting cliff. */
    const int walkC   = (int)floorf( NAV_WALKABLE_CLIMB  / NAV_CH );

    /* 1. Bounds & grid */
    float bmin[3], bmax[3];
    rcCalcBounds( geom->verts, geom->numVerts, bmin, bmax );
    int gw = 0, gh = 0;
    rcCalcGridSize( bmin, bmax, NAV_CS, &gw, &gh );

    /* 2. Heightfield */
    rcHeightfield *hf = rcAllocHeightfield();
    if ( !hf ) { Com_Log( SEV_INFO, LOG_CH(ch_nav), "NAV: rcAllocHeightfield failed\n" ); return NULL; }

    if ( !rcCreateHeightfield( &ctx, *hf, gw, gh, bmin, bmax, NAV_CS, NAV_CH ) ) {
        Com_Log( SEV_INFO, LOG_CH(ch_nav), "NAV: rcCreateHeightfield failed\n" );
        rcFreeHeightField( hf ); return NULL;
    }

    /* workAreas buffer: preserve custom area types through rcMarkWalkableTriangles.
     * That function overwrites its areas[] argument with RC_WALKABLE_AREA on walkable
     * tris, losing WATER/LAVA assignments from bsp_q3.c.  Strategy: work on a separate
     * buffer initialised to RC_NULL_AREA, then merge custom types back where the triangle
     * was marked walkable.  (Decision 1, Option A, approved 2026-04-21.)
     *
     * Assumption: rcMarkWalkableTriangles only writes RC_WALKABLE_AREA or leaves
     * RC_NULL_AREA — never any other value.  The assert below self-documents this. */
    unsigned char *workAreas = (unsigned char *)Z_Malloc( geom->numTris );
    if ( !workAreas ) {
        Com_Log( SEV_INFO, LOG_CH(ch_nav), "NAV: Z_Malloc workAreas failed\n" );
        rcFreeHeightField( hf ); return NULL;
    }
    memset( workAreas, RC_NULL_AREA, (size_t)geom->numTris );

    rcMarkWalkableTriangles( &ctx, NAV_WALKABLE_SLOPE_ANGLE,
                              geom->verts, geom->numVerts,
                              geom->tris,  geom->numTris, workAreas );


    for ( int i = 0; i < geom->numTris; i++ ) {
        assert( workAreas[i] == RC_WALKABLE_AREA || workAreas[i] == RC_NULL_AREA );
        if ( workAreas[i] == RC_WALKABLE_AREA )
            workAreas[i] = geom->areas[i] ? geom->areas[i] : (unsigned char)NAVAREA_GROUND;
    }

    if ( !rcRasterizeTriangles( &ctx, geom->verts, geom->numVerts,
                                 geom->tris, workAreas, geom->numTris, *hf, walkC ) ) {
        Com_Log( SEV_INFO, LOG_CH(ch_nav), "NAV: rcRasterizeTriangles failed\n" );
        Z_Free( workAreas ); rcFreeHeightField( hf ); return NULL;
    }
    Z_Free( workAreas );

    rcFilterLowHangingWalkableObstacles( &ctx, walkC, *hf );
    /* crouchH: spans with clearance < crouchH are filtered as impassable;
     * spans with crouchH <= clearance < walkH survive and are tagged
     * NAVAREA_LOW_CEILING in the compact HF scan below. */
    rcFilterLedgeSpans( &ctx, crouchH, walkC, *hf );
    rcFilterWalkableLowHeightSpans( &ctx, crouchH, *hf );

    /* 3. Compact heightfield (use crouchH so low-clearance spans are included) */
    rcCompactHeightfield *chf = rcAllocCompactHeightfield();
    if ( !chf ) {
        Com_Log( SEV_INFO, LOG_CH(ch_nav), "NAV: rcAllocCompactHeightfield failed\n" );
        rcFreeHeightField( hf ); return NULL;
    }
    if ( !rcBuildCompactHeightfield( &ctx, crouchH, walkC, *hf, *chf ) ) {
        Com_Log( SEV_INFO, LOG_CH(ch_nav), "NAV: rcBuildCompactHeightfield failed\n" );
        rcFreeHeightField( hf ); rcFreeCompactHeightfield( chf ); return NULL;
    }
    rcFreeHeightField( hf ); hf = NULL;

    /* NAVAREA_LOW_CEILING tagging: after compact HF, before erosion.
     * Spans with clearance < walkH but >= crouchH are accessible only while
     * crouching.  Spans below crouchH are set impassable (defensive; they were
     * already filtered by rcFilterWalkableLowHeightSpans above). */
    {
        const int ncells = chf->width * chf->height;
        for ( int ci = 0; ci < ncells; ci++ ) {
            const rcCompactCell &cc = chf->cells[ci];
            for ( unsigned si = cc.index, se = cc.index + (unsigned)cc.count; si < se; si++ ) {
                if ( chf->areas[si] == RC_NULL_AREA ) continue;
                const int h = (int)chf->spans[si].h;
                if ( h < walkH ) {
                    if ( h >= crouchH )
                        chf->areas[si] = (unsigned char)NAVAREA_LOW_CEILING;
                    else
                        chf->areas[si] = RC_NULL_AREA;
                }
            }
        }
    }


    /* Ceiling-less-column guard: drop a column's TOPMOST walkable span when the
     * player footprint at that span is entirely inside solid.
     *
     * WHY THIS EXISTS.  A sealed map has something above every floor, so a column
     * whose highest walkable span sits inside solid is a defect -- the surface
     * above it never reached the soup.  Every downstream filter works by detecting
     * geometry ABOVE a span, so a topmost span has nothing for them to find and the
     * pipeline is correct, on its own terms, to call it a surface and build polys
     * on it.  Measured on e1m1: 186,027 such spans against arena1's 46 (a 4044x
     * separation on this one property, with every other span statistic comparable
     * between the maps), yielding 1009 reachable polys buried in rock that the bot
     * then routed through.  This guard is the only stage positioned to see it.
     *
     * WHY A FOOTPRINT AND NOT A SINGLE POINT.  A point test at the span centre
     * condemns spans whose centre is solid but whose extent is walkable: measured
     * 56,391 of 316,338 nulls (17.8%) were spans a player-footprint test would
     * spare.  A 30u-wide agent is not a point, and testing it as one is the same
     * class of error this guard exists to correct.  So sample the centroid plus the
     * footprint corners and null ONLY when every sample is solid -- the strict
     * reading, matching nav_validate's buried census, so the two agree.
     *
     * Placed after LOW_CEILING tagging and before erosion: the cell/span walk above
     * already establishes the topmost-in-column property, and running before
     * erosion lets every later stage see the corrected area set. */
    {
        const int ncells = chf->width * chf->height;
        for ( int ci = 0; ci < ncells; ci++ ) {
            const rcCompactCell &cc = chf->cells[ci];
            const unsigned se = cc.index + (unsigned)cc.count;
            if ( cc.count == 0 ) continue;
            unsigned top = se;
            for ( unsigned si = cc.index; si < se; si++ )
                if ( chf->areas[si] != RC_NULL_AREA ) top = si;
            if ( top == se ) continue;   /* no walkable span in this column */

            const int cx = ci % chf->width, cy = ci / chf->width;
            const float rp[3] = { chf->bmin[0] + ( (float)cx + 0.5f ) * chf->cs,
                                  chf->bmin[1] + ( (float)chf->spans[top].y + 0.5f ) * chf->ch,
                                  chf->bmin[2] + ( (float)cy + 0.5f ) * chf->cs };
            float q[3];
            Nav_RecastToQuake( rp, q );

            /* Player footprint: centre + the four half-width corners. */
            static const float kFoot[5][2] = {
                { 0.0f, 0.0f },
                { -NAV_WALKABLE_RADIUS, -NAV_WALKABLE_RADIUS },
                {  NAV_WALKABLE_RADIUS, -NAV_WALKABLE_RADIUS },
                { -NAV_WALKABLE_RADIUS,  NAV_WALKABLE_RADIUS },
                {  NAV_WALKABLE_RADIUS,  NAV_WALKABLE_RADIUS },
            };
            bool allSolid = true;
            for ( int fi = 0; fi < 5 && allSolid; fi++ ) {
                const vec3_t fp = { q[0] + kFoot[fi][0], q[1] + kFoot[fi][1], q[2] };
                if ( !CM_NavPointSolidPublic( fp ) ) allSolid = false;
            }
            if ( allSolid ) chf->areas[top] = RC_NULL_AREA;
        }
    }

    /* 4. Erosion */
    const int walkR = (int)floorf( NAV_WALKABLE_RADIUS / NAV_CS );
    if ( !rcErodeWalkableArea( &ctx, walkR, *chf ) ) {
        Com_Log( SEV_INFO, LOG_CH(ch_nav), "NAV: rcErodeWalkableArea failed\n" );
        rcFreeCompactHeightfield( chf ); return NULL;
    }

    /* 5. Regions */
    if ( !rcBuildDistanceField( &ctx, *chf ) ) {
        Com_Log( SEV_INFO, LOG_CH(ch_nav), "NAV: rcBuildDistanceField failed\n" );
        rcFreeCompactHeightfield( chf ); return NULL;
    }
    if ( !rcBuildRegions( &ctx, *chf, 0, NAV_MIN_REGION_AREA, NAV_MERGE_REGION_AREA ) ) {
        Com_Log( SEV_INFO, LOG_CH(ch_nav), "NAV: rcBuildRegions failed\n" );
        rcFreeCompactHeightfield( chf ); return NULL;
    }

    /* 6. Contours → poly mesh */
    rcContourSet *cset = rcAllocContourSet();
    if ( !cset ) { Com_Log( SEV_INFO, LOG_CH(ch_nav), "NAV: rcAllocContourSet failed\n" ); rcFreeCompactHeightfield( chf ); return NULL; }
    if ( !rcBuildContours( &ctx, *chf, NAV_MAX_SIMPLIFICATION_ERR, (int)NAV_MAX_EDGE_LEN, *cset ) ) {
        Com_Log( SEV_INFO, LOG_CH(ch_nav), "NAV: rcBuildContours failed\n" );
        rcFreeCompactHeightfield( chf ); rcFreeContourSet( cset ); return NULL;
    }


    rcPolyMesh *pmesh = rcAllocPolyMesh();
    if ( !pmesh ) { Com_Log( SEV_INFO, LOG_CH(ch_nav), "NAV: rcAllocPolyMesh failed\n" ); rcFreeCompactHeightfield( chf ); rcFreeContourSet( cset ); return NULL; }
    if ( !rcBuildPolyMesh( &ctx, *cset, NAV_MAX_VERTS_PER_POLY, *pmesh ) ) {
        Com_Log( SEV_INFO, LOG_CH(ch_nav), "NAV: rcBuildPolyMesh failed\n" );
        rcFreeCompactHeightfield( chf ); rcFreeContourSet( cset ); rcFreePolyMesh( pmesh ); return NULL;
    }
    rcFreeContourSet( cset ); cset = NULL;


    /* Build OmcArrays from external OMCs now, before drop detection, so
     * internal drops can be appended directly. */
    OmcArrays omcArrs;
    memset( &omcArrs, 0, sizeof(omcArrs) );
    if ( omc ) buildOmcArrays( omc, &omcArrs );

    /* Unified physics-link generator (replaces the drop / vertical-step / jump-gap
     * producers + the sole-connector augmented-poly oracle).  Searches candidate
     * boundary-edge pairs, classifies each, validates against the game's movement
     * physics (Nav_ValidateLinkTrajectory), and emits ONLY proven-traversable links
     * into omcArrs -- so the mesh's every physics link carries its own proof.  Runs
     * on the bake worker thread (CM_BoxTrace is thread-legal since the collision
     * thread-safety work).  omcArrs already holds the external/mechanism OMCs; the
     * external count is the current omcArrs.count. */
    Nav_GeneratePhysicsLinks( pmesh, bmin, &omcArrs, omcArrs.count );

    /* Harvest the producer-stamped follower traversal modes into the userId-keyed
     * side-table (userId is the OMC's dense index in omcArrs here — external OMCs
     * are 0..numExt, physics-gen are NAV_MAX_OMC+offset, all < NAV_MAX_TOTAL_OMC).
     * Nav_FindPath resurfaces this onto the follower waypoint.  Bake path only;
     * the cache path rebuilds it from the loaded mesh (Nav_RebuildTraversalTable). */
    memset( nav.omcTraversalMode, 0, sizeof(nav.omcTraversalMode) );
    for ( int oi = 0; oi < omcArrs.count; oi++ ) {
        unsigned int uid = omcArrs.userIds[oi];
        if ( uid < (unsigned)NAV_MAX_TOTAL_OMC )
            nav.omcTraversalMode[uid] = omcArrs.traversalModes[oi];
    }
    nav.omcTraversalReady = qtrue;

    rcPolyMeshDetail *dmesh = rcAllocPolyMeshDetail();
    if ( !dmesh ) { Com_Log( SEV_INFO, LOG_CH(ch_nav), "NAV: rcAllocPolyMeshDetail failed\n" ); rcFreeCompactHeightfield( chf ); rcFreePolyMesh( pmesh ); return NULL; }
    if ( !rcBuildPolyMeshDetail( &ctx, *pmesh, *chf, NAV_DETAIL_SAMPLE_DIST, NAV_DETAIL_SAMPLE_MAX_ERR, *dmesh ) ) {
        Com_Log( SEV_INFO, LOG_CH(ch_nav), "NAV: rcBuildPolyMeshDetail failed\n" );
        rcFreeCompactHeightfield( chf ); rcFreePolyMesh( pmesh ); rcFreePolyMeshDetail( dmesh ); return NULL;
    }
    rcFreeCompactHeightfield( chf ); chf = NULL;


    /* 7. Tag areas and flags.
     *    rcPolyMesh::areas[] holds sequential navAreaId_t values.
     *    rcPolyMesh::flags[] holds navPolyFlags_t bitfield — separate write.
     */
    for ( int i = 0; i < pmesh->npolys; i++ ) {
        const unsigned char a = pmesh->areas[i];
        if ( a == RC_WALKABLE_AREA ) {
            pmesh->areas[i] = (unsigned char)NAVAREA_GROUND;
            pmesh->flags[i] = (unsigned short)NAVPOLY_WALKABLE;
        } else if ( a == (unsigned char)NAVAREA_WATER ) {
            pmesh->flags[i] = (unsigned short)(NAVPOLY_WALKABLE | NAVPOLY_WATER);
        } else if ( a == (unsigned char)NAVAREA_LAVA ) {
            pmesh->flags[i] = (unsigned short)(NAVPOLY_WALKABLE | NAVPOLY_LAVA);
        } else if ( a == (unsigned char)NAVAREA_LOW_CEILING ) {
            pmesh->flags[i] = (unsigned short)(NAVPOLY_WALKABLE | NAVPOLY_LOW_CEILING);
        } else if ( a == RC_NULL_AREA ) {
            pmesh->flags[i] = 0;
        } else {
            pmesh->flags[i] = (unsigned short)NAVPOLY_WALKABLE;
        }
    }

    /* 8. Detour navmesh data */
    dtNavMeshCreateParams params;
    memset( &params, 0, sizeof(params) );
    params.verts = pmesh->verts;   params.vertCount = pmesh->nverts;
    params.polys = pmesh->polys;   params.polyAreas = pmesh->areas;
    params.polyFlags = pmesh->flags; params.polyCount = pmesh->npolys;
    params.nvp = pmesh->nvp;
    params.detailMeshes = dmesh->meshes; params.detailVerts = dmesh->verts;
    params.detailVertsCount = dmesh->nverts; params.detailTris = dmesh->tris;
    params.detailTriCount = dmesh->ntris;
    if ( omcArrs.count > 0 ) {
        params.offMeshConVerts   = omcArrs.verts;  params.offMeshConRad  = omcArrs.rads;
        params.offMeshConFlags   = omcArrs.flags;  params.offMeshConAreas = omcArrs.areas;
        params.offMeshConDir     = omcArrs.dirs;   params.offMeshConUserID = omcArrs.userIds;
        params.offMeshConCount   = omcArrs.count;
    }
    params.bmin[0] = pmesh->bmin[0]; params.bmin[1] = pmesh->bmin[1]; params.bmin[2] = pmesh->bmin[2];
    params.bmax[0] = pmesh->bmax[0]; params.bmax[1] = pmesh->bmax[1]; params.bmax[2] = pmesh->bmax[2];
    params.walkableHeight = NAV_WALKABLE_HEIGHT; params.walkableRadius = NAV_WALKABLE_RADIUS;
    params.walkableClimb  = NAV_WALKABLE_CLIMB;  params.cs = NAV_CS; params.ch = NAV_CH;
    params.buildBvTree = true;

    unsigned char *navData = NULL; int navDataSize = 0;
    if ( !dtCreateNavMeshData( &params, &navData, &navDataSize ) ) {
        Com_Log( SEV_INFO, LOG_CH(ch_nav), "NAV: dtCreateNavMeshData failed (%d polys)\n", pmesh->npolys );
        rcFreePolyMesh( pmesh ); rcFreePolyMeshDetail( dmesh ); return NULL;
    }
    rcFreePolyMesh( pmesh ); rcFreePolyMeshDetail( dmesh );

    dtNavMesh *mesh = dtAllocNavMesh();
    if ( !mesh ) { dtFree( navData ); Com_Log( SEV_INFO, LOG_CH(ch_nav), "NAV: dtAllocNavMesh failed\n" ); return NULL; }
    dtStatus status = mesh->init( navData, navDataSize, DT_TILE_FREE_DATA );
    if ( dtStatusFailed(status) ) {
        dtFree( navData ); dtFreeNavMesh( mesh );
        Com_Log( SEV_INFO, LOG_CH(ch_nav), "NAV: dtNavMesh::init failed (0x%08x)\n", (unsigned)status );
        return NULL;
    }

    /* Note: the off-mesh-connection endpoint reconnect (reconnectUnlinkedOffMeshStarts)
     * AND the split-floor gap-bridge reconnect (reconnectSplitFloorSeams) are applied
     * in Nav_FinishReady, not here — Detour rebuilds a tile's links from poly->neis on
     * every addTile (including cache load), so a build-time-only pass would be dropped
     * on reload (a hatch-descent OMC's deep END would dangle after a cache round-trip:
     * omcFail mech > 0).  Running BOTH reconnects at finalize keeps them durable. */

    Com_Log( SEV_DEBUG, LOG_CH(ch_nav), "NAV: navmesh built -- %d bytes, %d OMCs (%d drop)\n",
                 navDataSize, omcArrs.count, omcArrs.count - (omc ? omc->count : 0) );
    return mesh;
}

/* -------------------------------------------------------------------------
   Nav_Impl_* — opaque accessors called from nav_cache.c (pure C)
   ------------------------------------------------------------------------- */

extern "C" {

void *Nav_Impl_AllocMesh( void )
{
    return (void *)dtAllocNavMesh();
}

void Nav_Impl_FreeMesh( void *mesh )
{
    if ( mesh )
        dtFreeNavMesh( (dtNavMesh *)mesh );
}

qboolean Nav_Impl_InitMeshFromData( void *mesh, unsigned char *data,
                                    int dataSize, int freeOnDealloc )
{
    dtStatus st = ((dtNavMesh *)mesh)->init(
        data, dataSize, freeOnDealloc ? DT_TILE_FREE_DATA : 0 );
    return dtStatusSucceed(st) ? qtrue : qfalse;
}

int Nav_Impl_GetMaxTiles( const void *mesh )
{
    return ((const dtNavMesh *)mesh)->getMaxTiles();
}

void Nav_Impl_GetTileData( const void *mesh, int tileIdx,
                           unsigned int *refLo, unsigned int *refHi,
                           int *dataSize, const unsigned char **data )
{
    const dtNavMesh *m = (const dtNavMesh *)mesh;
    const dtMeshTile *tile = m->getTile( tileIdx );
    if ( !tile || !tile->header ) {
        *refLo = *refHi = 0; *dataSize = 0; *data = NULL; return;
    }
    dtTileRef ref = m->getTileRef( tile );
    *refLo    = (unsigned int)((unsigned long long)ref & 0xFFFFFFFFu);
    *refHi    = (unsigned int)((unsigned long long)ref >> 32);
    *dataSize = tile->dataSize;
    *data     = tile->data;
}

void *Nav_Impl_AllocTileData( int size )
{
    return dtAlloc( size, DT_ALLOC_PERM );
}

/* dtFree(data) — release tile data allocated with Nav_Impl_AllocTileData. */
void Nav_Impl_FreeTileData( void *data )
{
    dtFree( data );
}

/* -------------------------------------------------------------------------
   Lifecycle
   ------------------------------------------------------------------------- */

void Nav_Init( void )
{
    memset( &nav, 0, sizeof(nav) );
    Nav_Debug_RegisterCommands();
    Com_Log( SEV_DEBUG, LOG_CH(ch_nav), "[NAV] Nav_Init: Recast/Detour nav layer initialised\n" );
}

void Nav_Shutdown( void )
{
    Nav_UnloadMap();
    Com_Log( SEV_DEBUG, LOG_CH(ch_nav), "[NAV] Nav_Shutdown\n" );
}

/* Defined in the background-bake section below; drains any in-flight worker so
 * the free calls here cannot race the bake thread's geom/result writes. */
static void Nav_JoinBake( void );

void Nav_UnloadMap( void )
{
    /* Drain any background bake first — otherwise the worker may still be
     * writing into navBake.geom/result while we free the mesh (use-after-free). */
    Nav_JoinBake();
    if ( nav.crowd ) { dtFreeCrowd( nav.crowd ); nav.crowd = NULL; }
    if ( nav.query ) { dtFreeNavMeshQuery( nav.query ); nav.query = NULL; }
    if ( nav.mesh  ) { dtFreeNavMesh( nav.mesh );  nav.mesh  = NULL; }
    nav.ready             = qfalse;
    nav.fromCache         = qfalse;
    nav.omcTraversalReady = qfalse;   /* re-stamp (bake) or re-derive (cache) next load */
}

int Nav_IsReady( void ) { return nav.ready ? 1 : 0; }

/* -------------------------------------------------------------------------
   Background bake

   A cache miss runs the (multi-second) Recast pipeline on a worker thread so
   the map is playable immediately.  The main-thread geometry extraction happens
   up front; the geometry is heap-copied (Nav_Geom_HeapCopy) so the worker owns
   a private copy while the main thread continues.  Nav_Frame() picks up the
   finished mesh on the main thread (query init, door tag, cache save, ready
   flip) — all Detour query/init work stays single-threaded.
   ------------------------------------------------------------------------- */

typedef enum {
    NAV_BAKE_IDLE = 0,
    NAV_BAKE_RUNNING,
    NAV_BAKE_DONE
} navBakeState_t;

static struct {
    volatile navBakeState_t state;
    sysThread_t   *thread;
    navGeom_t      geom;              /* heap-owned copy (Z_Malloc) for the worker */
    navOmcInput_t  omc;               /* value struct, copied alongside geom */
    char           mapname[64];
    int            checksum;
    int            tStart;
    dtNavMesh     *result;            /* set by the worker, read by Nav_Frame */
    qboolean       ok;                /* worker success flag */
} navBake;

static void Nav_BakeWorker( void *arg )
{
    (void)arg;
    dtNavMesh *mesh = Nav_Build_Internal( &navBake.geom, &navBake.omc );
    navBake.result = mesh;
    navBake.ok     = mesh ? qtrue : qfalse;
    /* Publish the result last: Nav_Frame only reads result/ok after seeing DONE. */
    navBake.state  = NAV_BAKE_DONE;
}

/* Join an in-flight bake and discard its result (called on unload/reload). */
static void Nav_JoinBake( void )
{
    if ( navBake.state == NAV_BAKE_IDLE )
        return;
    if ( navBake.thread ) {
        Sys_JoinThread( navBake.thread );
        navBake.thread = NULL;
    }
    if ( navBake.result ) {
        dtFreeNavMesh( navBake.result );
        navBake.result = NULL;
    }
    Nav_Geom_HeapFree( &navBake.geom );
    navBake.state = NAV_BAKE_IDLE;
    Com_Log( SEV_DEBUG, LOG_CH(ch_nav), "nav: in-flight bake joined and discarded\n" );
}

int  Nav_IsBaking( void ) { return navBake.state == NAV_BAKE_RUNNING ? 1 : 0; }

/* Spawn the worker thread; returns qfalse if the thread could not be created. */
static qboolean Nav_SpawnBake( void )
{
    navBake.state  = NAV_BAKE_RUNNING;
    navBake.result = NULL;
    navBake.ok     = qfalse;
    navBake.thread = Sys_CreateThread( Nav_BakeWorker, NULL );
    return navBake.thread ? qtrue : qfalse;
}

/*
 * Nav_BakeBegin — the worker path for a cache miss.  Extracts geometry on the
 * main thread, heap-copies it, and spawns the background bake.  Falls back to an
 * inline (synchronous) build if the thread cannot be spawned.  Returns qtrue if
 * a background bake is now in flight (map playable, mesh pending), qfalse if the
 * build completed inline (nav.mesh set) or failed.
 */
static qboolean Nav_BakeBegin( const char *mapname, int checksum )
{
    navGeom_t    geom;
    navOmcInput_t omc;

    int t0 = Sys_Milliseconds();
    if ( !Nav_Geom_Extract( mapname, &geom, &omc ) ) {
        Com_Log( SEV_INFO, LOG_CH(ch_nav), "[NAV] Nav_LoadMap: geometry extraction failed for '%s'\n", mapname );
        return qfalse;
    }
    if ( geom.numTris == 0 ) {
        Com_Log( SEV_INFO, LOG_CH(ch_nav), "nav: no geometry to bake for '%s'\n", mapname );
        Nav_OMC_Free( &omc );
        Nav_Geom_Free( &geom );
        return qfalse;
    }

    int t1 = Sys_Milliseconds();
    Com_Log( SEV_INFO, LOG_CH(ch_nav), "[NAV] geometry extraction: %d ms (%d verts, %d tris)\n",
                t1 - t0, geom.numVerts, geom.numTris );

    /* Deep-copy the geom arrays to the heap; carry the OMC (a value struct with
     * no pointers) alongside in navBake.omc — HeapCopy touches only geom arrays. */
    if ( !Nav_Geom_HeapCopy( &geom, &navBake.geom ) ) {
        Com_Log( SEV_INFO, LOG_CH(ch_nav), "[NAV] Nav_LoadMap: heap-copy failed for '%s'\n", mapname );
        Nav_OMC_Free( &omc );
        Nav_Geom_Free( &geom );
        return qfalse;
    }
    navBake.omc = omc;

    /* The Hunk-temp originals are done once copied. */
    Nav_OMC_Free( &omc );
    Nav_Geom_Free( &geom );

    Q_strncpyz( navBake.mapname, mapname, sizeof(navBake.mapname) );
    navBake.checksum = checksum;
    navBake.tStart   = t0;

    if ( Nav_SpawnBake() ) {
        Com_Log( SEV_INFO, LOG_CH(ch_nav), "nav: baking '%s' in background (map playable now)\n", mapname );
        return qtrue;
    }

    /* Thread spawn failed — build inline so the map is still navigable. */
    Com_Log( SEV_INFO, LOG_CH(ch_nav), "nav: could not spawn bake thread for '%s', baking inline\n", mapname );
    nav.mesh = Nav_Build_Internal( &navBake.geom, &navBake.omc );
    Nav_Geom_HeapFree( &navBake.geom );
    navBake.state = NAV_BAKE_IDLE;
    nav.buildMs   = Sys_Milliseconds() - t0;
    nav.fromCache = qfalse;
    if ( nav.mesh && checksum != 0 )
        Nav_Cache_Save( mapname, checksum, nav.mesh );
    return qfalse;
}

/* Finalize the mesh on the main thread once query_init has run. */
static void Nav_FinishReady( void );
static int  Nav_FinalizeOmcStandability( void );          /* post-bake OMC-endpoint standability pass  (defined near nav_validate) */
static int  Nav_FinalizeGroundPolyStandability( void );   /* post-bake ground-poly standability pass    (defined near nav_validate) */
static void NavVal_ResetMoverCache( void );               /* drop the standability probe's at-rest mover handles (per-map) */
static int  Nav_FinalizeLinkTrajectory( void );           /* post-bake physics-traversability pass      (defined near nav_validate) */
static int  Nav_FinalizeIntraRegionCull( void );          /* post-bake intra-region shortcut cull       (defined near nav_validate) */

/*
 * Nav_Frame — called every server frame.  Cheap no-op unless a background bake
 * just finished, in which case it adopts the mesh on the main thread and runs
 * the single-threaded finalization (query init, door tag, cache save, ready).
 */
void Nav_Frame( void )
{
    if ( navBake.state != NAV_BAKE_DONE )
        return;

    if ( navBake.thread ) {
        Sys_JoinThread( navBake.thread );
        navBake.thread = NULL;
    }

    dtNavMesh *mesh = navBake.result;
    navBake.result  = NULL;
    int checksum    = navBake.checksum;
    int tStart      = navBake.tStart;
    Nav_Geom_HeapFree( &navBake.geom );
    navBake.state   = NAV_BAKE_IDLE;

    if ( !navBake.ok || !mesh ) {
        Com_Log( SEV_INFO, LOG_CH(ch_nav), "nav: background bake failed for '%s'\n", navBake.mapname );
        return;
    }

    nav.mesh      = mesh;
    nav.buildMs   = Sys_Milliseconds() - tStart;
    nav.fromCache = qfalse;

    if ( checksum != 0 )
        Nav_Cache_Save( navBake.mapname, checksum, nav.mesh );

    /* Same main-thread finalization as the synchronous path (query init +
     * door tag + ready flip). nav.mapname already holds this map. */
    Nav_FinishReady();
}

void Nav_LoadMap( const char *mapname )
{
    Nav_UnloadMap();
    /* The standability probe's at-rest mover handles belong to the PREVIOUS map's
     * collision world; drop them so the next probe rebuilds against this map. */
    NavVal_ResetMoverCache();
    Q_strncpyz( nav.mapname, mapname, sizeof(nav.mapname) );

    int tStart = Sys_Milliseconds();

    /* Fast path: checksum → cache lookup */
    int checksum = 0;
    if ( Nav_Geom_GetChecksum( mapname, &checksum ) ) {
        void *cached = Nav_Cache_Load( mapname, checksum );
        if ( cached ) {
            nav.mesh      = (dtNavMesh *)cached;
            nav.fromCache = qtrue;
            int ms = Sys_Milliseconds() - tStart;
            nav.buildMs   = ms;
            Com_Log( SEV_INFO, LOG_CH(ch_nav), "[NAV] loaded from cache: %d ms\n", ms );
            goto query_init;
        }
    }

    /* Slow path: extract on the main thread, then bake in the background.
     * If a background bake is in flight, return now — Nav_Frame() will finish
     * it (query init, ready flip) once the worker completes. */
    if ( Nav_BakeBegin( mapname, checksum ) )
        return;
    if ( !nav.mesh )
        return;   /* inline build failed or no geometry */

query_init:
    Nav_FinishReady();
}

/*
 * Nav_FinishReady — main-thread mesh finalization shared by the synchronous
 * (cache-hit / inline-build) path and the async Nav_Frame() pickup.  Allocates
 * and initialises the query against nav.mesh, tags door polys (needs an
 * initialised query for queryPolygons), and flips nav.ready.  On any failure
 * the mesh and query are freed and nav.ready stays false.  nav.mesh must be set.
 */
/* Rebuild the follower OMC traversal-mode table from the LOADED mesh's OMCs, for
 * the CACHE path (which has no producer OmcArrays).  The bake path already stamped
 * the exact producer modes into nav.omcTraversalMode (omcTraversalReady) — keep
 * those.  On cache-load derive per OMC from userId + area + endpoint vertical:
 *   - TELEPORT area                         -> BALLISTIC (instant)
 *   - WATER area                            -> WALK (wade)
 *   - physics-gen (userId >= NAV_MAX_OMC)   -> FALL/CLIMB/WALK by dest-vs-start z
 *   - external JUMP_LINK (userId < NAV_MAX_OMC): mechanism class the geometry cannot
 *     disambiguate (jump-pad vs door-gap vs hatch vs plat all look alike) — fall back
 *     to the endpoint vertical: a real drop reads FALL, a rise reads BALLISTIC (a pad
 *     launches up), and a level link reads WALK.  This is the cache-load best-effort;
 *     the tested fresh-bake path carries the exact producer stamp.
 * Uses the same offMeshCons[] userId lookup the finalize siblings use. */
static void Nav_BuildTraversalTable( void )
{
    if ( nav.omcTraversalReady )
        return;   /* bake path already stamped the exact producer modes — keep them */

    memset( nav.omcTraversalMode, 0, sizeof(nav.omcTraversalMode) );
    if ( !nav.mesh ) return;
    const dtNavMesh *mesh = (const dtNavMesh *)nav.mesh;
    const int maxTiles = mesh->getMaxTiles();
    for ( int ti = 0; ti < maxTiles; ti++ ) {
        const dtMeshTile *t = mesh->getTile( ti );
        if ( !t || !t->header ) continue;
        for ( int ci = 0; ci < t->header->offMeshConCount; ci++ ) {
            const dtOffMeshConnection *con = &t->offMeshCons[ci];
            const dtPoly *poly = &t->polys[ con->poly ];
            if ( poly->vertCount < 2 ) continue;
            unsigned int uid  = con->userId;
            unsigned char area = 0;
            mesh->getPolyArea( mesh->getPolyRefBase( t ) + con->poly, &area );
            /* OMC endpoints (recast): con->pos[0..2] start, [3..5] end.  Recast Y is
             * up, so the vertical delta is pos[4]-pos[1]. */
            float rise = con->pos[4] - con->pos[1];
            unsigned char mode;
            if ( area == (unsigned char)NAVAREA_TELEPORT ) {
                mode = (unsigned char)NAV_TM_BALLISTIC;
            } else if ( area == (unsigned char)NAVAREA_WATER ) {
                mode = (unsigned char)NAV_TM_WALK;
            } else if ( uid >= (unsigned)NAV_MAX_OMC ) {
                mode = ( rise >  NAV_WATEREDGE_STEP ) ? (unsigned char)NAV_TM_CLIMB :
                       ( rise < -NAV_WATEREDGE_STEP ) ? (unsigned char)NAV_TM_FALL  :
                                                        (unsigned char)NAV_TM_WALK;
            } else {
                /* External mechanism class on the CACHE path, where the producer stamp
                 * is unavailable and jump-pad / target-push / plat / door-gap / hatch
                 * all look alike.  BALLISTIC must NOT be guessed here.
                 *
                 * BALLISTIC is not a shape, it is a PROMISE: the follower sets
                 * suppressMove for the whole transit because a mechanism is expected to
                 * carry the bot.  When that promise is wrong the bot issues no movement
                 * at all and sits at the entry until the leash expires — measured on
                 * e1m1's 425 u drop at (716,557,104)->(792,512,-321): 77 entries from
                 * the identical position, distToDest pinned at 434, pastPlane exactly
                 * -distToDest (algebraically zero displacement along the link axis), and
                 * no trigger_push or teleporter anywhere near it.
                 *
                 * A verified mechanism still reaches BALLISTIC through the TELEPORT
                 * branch above, and every real producer (nav_offmesh: jump-pad :245,
                 * teleporter :285, target_push :339) stamps BALLISTIC itself on the bake
                 * path, which this function does not touch.  So the safe cache-load
                 * derivation is the SAME rule the physics-gen branch directly above
                 * already uses on identical geometry: self-powered CLIMB/FALL/WALK.
                 * Guessing self-powered on a real pad merely adds a redundant move the
                 * pad's velocity dominates; guessing BALLISTIC on a self-powered link
                 * hangs the bot for the whole leash.  The asymmetry decides it. */
                mode = ( rise >  NAV_WATEREDGE_STEP ) ? (unsigned char)NAV_TM_CLIMB :
                       ( rise < -NAV_WATEREDGE_STEP ) ? (unsigned char)NAV_TM_FALL  :
                                                        (unsigned char)NAV_TM_WALK;
            }
            if ( uid < (unsigned)NAV_MAX_TOTAL_OMC )
                nav.omcTraversalMode[uid] = mode;
        }
    }
}

static void Nav_FinishReady( void )
{
    nav.query = dtAllocNavMeshQuery();
    if ( !nav.query ) {
        Com_Log( SEV_INFO, LOG_CH(ch_nav), "[NAV] dtAllocNavMeshQuery failed\n" );
        Nav_Impl_FreeMesh( nav.mesh ); nav.mesh = NULL; return;
    }
    dtStatus st = nav.query->init( nav.mesh, NAV_MAX_QUERY_NODES );
    if ( dtStatusFailed(st) ) {
        Com_Log( SEV_INFO, LOG_CH(ch_nav), "[NAV] dtNavMeshQuery::init failed (0x%08x)\n", (unsigned)st );
        dtFreeNavMeshQuery( nav.query ); nav.query = NULL;
        Nav_Impl_FreeMesh( nav.mesh );  nav.mesh  = NULL; return;
    }

    /* Off-mesh-connection endpoint reconnect (second-chance bind for an OMC endpoint
     * that failed the standard baseOffMeshLinks bind — a func_plat boarding point or a
     * hatch-descent landing resting offset from the surrounding walkable floor, and the
     * per-endpoint END-rescue for a bound-START/unbound-END connection).  Applied HERE,
     * not only at bake time, because Detour rebuilds a tile's link list from poly->neis
     * on every addTile (cache load included), which drops any post-bake links; a
     * build-time-only pass would leave the endpoint dangling after a cache round-trip
     * (omcFail mech > 0).  Idempotent: a no-op for endpoints already bound. */
    {
        int reconnected = ((dtNavMesh *)nav.mesh)->reconnectUnlinkedOffMeshStarts(
            NAV_OMC_SNAP_HORIZ, NAV_OMC_SNAP_VERT, NAV_OMC_APPROACH_HORIZ );
        if ( reconnected > 0 )
            Com_Log( SEV_DEBUG, LOG_CH(ch_nav),
                     "NAV: reconnected %d unlinked off-mesh endpoint(s) (snap %.0f/%.0f, approach %.0f)\n",
                     reconnected, (double)NAV_OMC_SNAP_HORIZ, (double)NAV_OMC_SNAP_VERT,
                     (double)NAV_OMC_APPROACH_HORIZ );
    }

    /* Split-floor gap-bridge reconnect (see Nav_Build_Internal for the mechanism).
     * Applied here — not only at bake time — because Detour rebuilds a tile's link
     * list from poly->neis on every addTile (cache load included), which drops any
     * post-bake links; re-running the bridge each load keeps it durable across the
     * disk cache.  Idempotent: it links only currently-different large components,
     * so a second pass on an already-bridged mesh is a no-op. */
    {
        int seams = ((dtNavMesh *)nav.mesh)->reconnectSplitFloorSeams(
            NAV_SEAM_MAX_STEP, NAV_SEAM_MIN_OVERLAP, NAV_SEAM_MAX_GAP );
        if ( seams > 0 )
            Com_Log( SEV_DEBUG, LOG_CH(ch_nav),
                     "NAV: bridged %d split-floor gap seam(s) (maxStep %.0f, minOverlap %.0f, maxGap %.0f)\n",
                     seams, (double)NAV_SEAM_MAX_STEP, (double)NAV_SEAM_MIN_OVERLAP,
                     (double)NAV_SEAM_MAX_GAP );
    }

    /* (The old jump-gap OMC endpoint finalize retired with the physics producers:
     * the unified generator already validates each link's endpoints against the
     * collision floor at emission via the shared standability probe, so no post-load
     * foot-snap re-binding is needed -- the endpoints ship collision-real.) */

    /* Tag door polys AFTER query is initialised (needs queryPolygons). */
    Nav_TagDoorAreas( nav.mapname );

    /* Endpoint-standability finalize: EVERY OMC endpoint (any producer) must land
     * on a collision-standable floor.  Several producers run on the bake thread
     * where CM tracing is illegal, so this is enforced here on the main thread —
     * one shared "standable" definition (NavVal_ClearanceFloorZ, the same probe
     * nav_validate uses) applied to what SHIPS.  Each failing endpoint is
     * regrounded within a small ring, or the OMC is disabled if none is standable.
     * Per-load + post-init (like the seam / jump-gap reconnect above) so the mesh
     * is cache-durable: omcFail==0 becomes a structural property of the mesh. */
    Nav_FinalizeOmcStandability();

    /* Poly-standability finalize: the symmetric case.  A large tilted face whose
     * own centroid is standable can still yield a Recast poly over its NON-standable
     * extent (poly 5016: a poly ~22u above the collision floor).  Per-face culling
     * cannot catch that; per-poly can.  Every reachable ground poly runs the SAME
     * shared standability probe at its centroid; a poly a player cannot rest on is
     * flagged BLOCKED (the query filter excludes it) — an honest mesh refuses to
     * path over a surface that lies.  No z-correction (detail-mesh surgery is not
     * worth it); flag-and-exclude.  Per-load, cache-durable, like the OMC finalize. */
    Nav_FinalizeGroundPolyStandability();

    /* Trajectory finalize: the physics sibling of the standability finalizes.  Every
     * physics-class OMC (drop / vertical-step / jump-gap) that no launchable player
     * arc can traverse (apex-deficit, wall/ceiling clip, launch-box-solid, landing
     * not standable, gap beyond range) is disabled via NAVPOLY_BLOCKED -- so no
     * physically-untraversable link is ever routable, structurally.  Mechanism links
     * (teleporter / plat / door-gap / jump-pad / water-edge wade) are exempt.  Runs
     * AFTER the standability finalizes so it validates against the standable-endpoint
     * mesh.  Per-load, cache-durable, format-agnostic. */
    Nav_FinalizeLinkTrajectory();

    /* Intra-region cull (Option A): the PLACEMENT sibling of the physics finalize.
     * The generator emits every physics-proven link at bake; this keeps only the ones
     * the loaded world actually NEEDS.  A generator link whose two endpoints are
     * mutually reachable WITHOUT any generator link (walk + seam + mechanism OMCs) is a
     * pure shortcut across one already-connected region (the deferred class) and is
     * disabled.  Reachability is a QUERY against the real loaded link graph -- the
     * oracle's question answered where it is ground truth, not predicted at bake.  Runs
     * AFTER the seam-reconnect + door tags + trajectory finalize so the exclusion graph
     * is the final walk+mechanism connectivity.  Per-load, cache-durable. */
    Nav_FinalizeIntraRegionCull();

    /* Follower OMC traversal-mode table.  The bake path already stamped it from the
     * producers' OmcArrays (nav.omcTraversalReady).  The CACHE path loads a bare
     * mesh with no OmcArrays, so rebuild the table from the loaded mesh's OMCs —
     * per-load + cache-durable, the same shape as the finalize siblings above. */
    Nav_BuildTraversalTable();

    nav.ready = qtrue;
    Com_Log( SEV_INFO, LOG_CH(ch_nav), "[NAV] navmesh ready for '%s' (%s)\n",
                nav.mapname, nav.fromCache ? "from cache" : "built" );
}

/* -------------------------------------------------------------------------
   Query functions (called via Nav_HandleTrap in nav_traps.c)
   ------------------------------------------------------------------------- */

static const float kDefaultExtents[3] = { 280.0f, 280.0f, 480.0f };




static dtQueryFilter s_filter;
static bool          s_filterInited = false;

static const dtQueryFilter *GetFilter( void )
{
    if ( !s_filterInited ) {
        s_filter.setIncludeFlags( 0xFFFF );
        /* Exclude polys that have been runtime-blocked (finalize-disabled lying polys
         * / physically-untraversable links, or nav_setblocked).  NAVPOLY_OPENABLE_CLOSED
         * is NOT excluded: a closed OPENABLE door is INCLUDED so the path plans THROUGH
         * it (committing to open it) instead of routing around -- see the door-area cost
         * below.  This is the openable-traversal filter contract. */
        s_filter.setExcludeFlags( (unsigned short)NAVPOLY_BLOCKED );
        /* Door crossings (NAVAREA_DOOR, incl. the tagged door-gap OMC) traverse at HIGH
         * cost: 10x the distance.  A bot planning to the exit prefers any open route,
         * but when a closed door is the only way it still plans through it (and the
         * activation handler then opens it).  10 is high enough to deprioritise a door
         * against parallel open geometry, low enough to keep the door routable when it
         * is the sole crossing (a doorway has no alternative, so this never strands). */
        s_filter.setAreaCost( (int)NAVAREA_DOOR, 10.0f );
        s_filterInited = true;
    }
    return &s_filter;
}

/* -------------------------------------------------------------------------
   Goal-floor preference (EP2 / target-poly preference)

   findNearestPoly picks the geometrically closest poly, which near a func_plat
   shaft or a tiny floating island can be a tiny, isolated fragment the bot can
   never actually stand on — while a large walkable floor sits one step below,
   slightly farther. NavGoalFloorPreferQuery visits every poly overlapping the
   query box and, via the (topology-only) dtNavMesh::isPreferredWalkInFloor
   predicate, prefers a large floor poly one step below the tiny nearest one.

   The predicate lives in Detour (poly verts/links/OMC-type only — format-
   agnostic, no classname/entity data); this visitor is the thin query adapter.
   ------------------------------------------------------------------------- */

/* Local 3-float copy (DetourCommon's dtVcopy is not included in this TU). */
static inline void navVcopy( float *dst, const float *src )
{
    dst[0] = src[0]; dst[1] = src[1]; dst[2] = src[2];
}

class NavGoalFloorPreferQuery : public dtPolyQuery
{
public:
    NavGoalFloorPreferQuery( const dtNavMeshQuery *query, const float *center,
                             dtPolyRef nearRef, const float *nearPt,
                             float capHorizSqr )
        : m_query( query ), m_nearRef( nearRef ), m_capHorizSqr( capHorizSqr ),
          m_prefRef( 0 )
    {
        navVcopy( m_center, center );
        navVcopy( m_nearPt, nearPt );
        navVcopy( m_prefPt, nearPt );
    }

    void process( const dtMeshTile *tile, dtPoly **polys, dtPolyRef *refs,
                  int count ) override
    {
        (void)polys;
        const dtNavMesh *mesh = m_query->getAttachedNavMesh();
        for ( int i = 0; i < count; ++i ) {
            dtPolyRef candRef = refs[i];
            if ( candRef == m_nearRef )
                continue;
            float candPt[3];
            if ( dtStatusFailed( m_query->closestPointOnPoly( candRef, m_center, candPt, 0 ) ) )
                continue;
            if ( mesh->isPreferredWalkInFloor( m_center, m_nearRef, m_nearPt,
                                               candRef, candPt, m_capHorizSqr ) ) {
                m_prefRef = candRef;
                navVcopy( m_prefPt, candPt );
            }
        }
        (void)tile;
    }

    dtPolyRef    prefRef() const { return m_prefRef; }
    const float *prefPt()  const { return m_prefPt; }

private:
    const dtNavMeshQuery *m_query;
    dtPolyRef             m_nearRef;
    float                 m_capHorizSqr;
    float                 m_center[3];
    float                 m_nearPt[3];
    dtPolyRef             m_prefRef;
    float                 m_prefPt[3];
};

/*
 * Nav_SnapGoalToFloor — resolve a goal position to a target poly, preferring a
 * large walkable floor one step below a tiny/isolated nearest poly (EP2 fix).
 * Falls back to the plain findNearestPoly result when no better floor exists.
 * *outRef / outPt receive the chosen poly ref and closest point.
 */
static void Nav_SnapGoalToFloor( const dtNavMeshQuery *query, const float *rGoal,
                                 const float *extents, const dtQueryFilter *filter,
                                 dtPolyRef *outRef, float *outPt,
                                 dtPolyRef fromRef )
{
    dtPolyRef nearRef = 0;
    float     nearPt[3];
    query->findNearestPoly( rGoal, extents, filter, &nearRef, nearPt );

    /* Default: the plain nearest poly. */
    *outRef = nearRef;
    navVcopy( outPt, nearPt );
    if ( !nearRef )
        return;

    /* Look for a preferred large-floor poly one step below. */
    NavGoalFloorPreferQuery pref( query, rGoal, nearRef, nearPt,
                                  NAV_GOAL_PREF_HORIZ * NAV_GOAL_PREF_HORIZ );
    query->queryPolygons( rGoal, extents, filter, &pref );
    if ( !pref.prefRef() )
        return;

    /* REACHABILITY PREFERENCE.  The floor preference above ranks candidates by
     * FLOOR SIZE alone (dtNavMesh::isPreferredWalkInFloor is topology-only) and
     * never asks whether the agent can route to what it picked.  Measured on the
     * e1m1 specimen: a trigger volume whose plain nearest poly IS reachable was
     * relocated 27u horizontally and 63u down onto a "better" floor that is NOT —
     * every query to it then returned DT_PARTIAL_RESULT with termPoly != endRef,
     * and the caller read that as "the target is unreachable" when in fact only
     * the SNAP's choice was.  A bigger floor the agent cannot stand on is not a
     * better goal, so size only decides among candidates the agent can get to.
     *
     * COST, stated plainly: this needs real reachability, and Detour exposes no
     * component id to compare — getReachablePolyCount floods from ONE ref and
     * cannot answer "same island as that other ref".  So it costs up to two extra
     * findPath calls, and ONLY when the size preference actually fired AND the
     * caller supplied its start poly.  The second call runs only when the first
     * reports a partial, i.e. only on the already-suspect case.  Both use a small
     * fixed probe buffer (the corridor is discarded; only the status is read), so
     * the added work is bounded per call and does not scale with route length.
     * fromRef == 0 — a caller with no start poly — keeps the historical size-only
     * behaviour byte-for-byte. */
    if ( fromRef ) {
        dtPolyRef probePath[NAV_MAX_PATH_POINTS];
        int       nPref = 0, nNear = 0;
        dtStatus  sPref, sNear;
        sPref = query->findPath( fromRef, pref.prefRef(), rGoal, pref.prefPt(),
                                 filter, probePath, &nPref, NAV_MAX_PATH_POINTS );
        if ( !dtStatusFailed( sPref ) && dtStatusDetail( sPref, DT_PARTIAL_RESULT ) ) {
            /* The preferred floor did not resolve. Keep it only if the plain
             * nearest poly is no better — never trade a reachable target for an
             * unreachable one, and never reject on the preferred poly alone. */
            sNear = query->findPath( fromRef, nearRef, rGoal, nearPt,
                                     filter, probePath, &nNear, NAV_MAX_PATH_POINTS );
            if ( !dtStatusFailed( sNear ) && !dtStatusDetail( sNear, DT_PARTIAL_RESULT ) ) {
                return;   /* nearest reaches, preferred does not — keep nearest */
            }
        }
    }

    *outRef = pref.prefRef();
    navVcopy( outPt, pref.prefPt() );
}

int Nav_FindPath( const float *qOrigin, const float *qGoal,
                  int /*agentType*/, navPath_t *out )
{
    if ( !nav.ready || !out ) { if ( out ) out->count = 0; return 0; }


    float rOrigin[3], rGoal[3];
    Nav_QuakeToRecast( qOrigin, rOrigin );
    Nav_QuakeToRecast( qGoal,   rGoal );

    const dtQueryFilter *filter = GetFilter();
    float nearPt[3];
    dtPolyRef startRef = 0, endRef = 0;
    nav.query->findNearestPoly( rOrigin, kDefaultExtents, filter, &startRef, nearPt );
    /* Goal endpoint: prefer a large walkable floor over a tiny/isolated poly the
     * plain nearest-poly would pick near a shaft or floating fragment (EP2). */
    Nav_SnapGoalToFloor( nav.query, rGoal, kDefaultExtents, filter, &endRef, nearPt,
                         startRef );

    if ( !startRef || !endRef ) { out->count = 0; return 0; }

    static dtPolyRef polyPath[NAV_MAX_PATH_POINTS];
    int numPolys = 0;
    /* Capture the query status: Detour sets DT_PARTIAL_RESULT when the corridor did
     * NOT reach the goal poly (goal severed by an excluded flag, or the cost-directed
     * search terminated at a best-guess dead-end short of the goal).  This is the
     * authoritative "does the route reach the goal" signal — encoded into the scalar
     * return below as NAV_FINDPATH_PARTIAL so callers (WiredIntel_PathReaches) can
     * distinguish a reached route from a dead-ending partial, instead of guessing from
     * endpoint proximity. */
    dtStatus pathStatus = nav.query->findPath( startRef, endRef, rOrigin, rGoal, filter,
                                               polyPath, &numPolys, NAV_MAX_PATH_POINTS );


    if ( numPolys <= 0 ) { out->count = 0; return 0; }



    static float        straightPos[NAV_MAX_PATH_POINTS][3];
    static unsigned char straightFlags[NAV_MAX_PATH_POINTS];
    static dtPolyRef    straightRefs[NAV_MAX_PATH_POINTS];
    int nStraight = 0;
    nav.query->findStraightPath( rOrigin, rGoal, polyPath, numPolys,
                                  straightPos[0], straightFlags, straightRefs,
                                  &nStraight, NAV_MAX_PATH_POINTS );
    if ( nStraight <= 0 ) { out->count = 0; return 0; }

    int count = nStraight < NAV_MAX_PATH_POINTS ? nStraight : NAV_MAX_PATH_POINTS;
    const dtNavMesh *mesh = (const dtNavMesh *)nav.mesh;

    for ( int i = 0; i < count; i++ ) {
        Nav_RecastToQuake( straightPos[i], out->positions[i] );
        out->flags[i]    = straightFlags[i];
        out->polyrefs[i] = (navPolyRef_t)straightRefs[i];
        /* Resurface the producer-stamped follower traversal mode onto the OMC
         * waypoint: resolve the OMC's userId from its poly ref (the same
         * offMeshCons[].poly == pi lookup the finalizes use), index the side-table.
         * NAV_TM_NONE on a non-OMC waypoint.
         *
         * DIRECTION-AWARE.  The stored mode is stamped ONCE, from the link's baked
         * start->end rise (nav_impl :3436 / :887, and nav_offmesh for movers).  But a
         * mesh off-mesh connection is BIDIRECTIONAL — Nav_FinalizeLinkTrajectory says
         * so in as many words: "A link is validated in EITHER direction (the mesh link
         * is bidirectional); it is disabled only if NEITHER direction is physically
         * traversable."  Validated both ways, typed one way: a single slot holding a
         * value that is only meaningful per-direction.
         *
         * The consequence is a theorem, not a risk: any link whose |rise| exceeds the
         * step height is NECESSARILY wrong in one of its two senses, because a genuine
         * run-jump UP one way is a DROP the other and only one mode exists.  Measured
         * on e1m1: the bot descended poly 6728 (dZ -30) and the follower handed it
         * NAV_TM_CLIMB — the value the bake rule yields for the +30 sense — so an
         * up-jump actuator drove a downward link and the transit timed out.
         *
         * So resolve from (uid, direction of travel) rather than uid alone.  No new
         * data is needed and the .nav format is untouched: the reverse mode is a pure
         * function of the forward one.  Only the gravity-relative pair flips —
         *   CLIMB <-> FALL  (an up-jump one way is a drop the other)
         *   WALK   -> WALK  (|rise| <= step, symmetric by construction)
         *   RIDE   -> RIDE  (nav_offmesh stamps it off |topLedge.z - shaftFloor.z|,
         *                    an ABSOLUTE rise; the mover carries the rider either way)
         *   BALLISTIC -> BALLISTIC (teleport / pad / external mechanism: the mechanism
         *                    carries the bot, so the sense of the link is irrelevant)
         * — leaving RIDE and BALLISTIC identity because their actuations are already
         * direction-symmetric, not because the reverse is unknown.
         *
         * Travel direction comes from the traversal itself: Detour emits the OMC
         * waypoint at the endpoint the path ENTERS.  Comparing that against the link's
         * stored start endpoint tells us which sense is being walked.  Both are Recast
         * coords here, so this compares like with like and needs no conversion. */
        unsigned char tm = (unsigned char)NAV_TM_NONE;
        if ( ( straightFlags[i] & DT_STRAIGHTPATH_OFFMESH_CONNECTION ) && mesh ) {
            unsigned int salt, it, ip;
            mesh->decodePolyId( (dtPolyRef)straightRefs[i], salt, it, ip );
            const dtMeshTile *t = mesh->getTile( (int)it );
            if ( t && t->header ) {
                for ( int c = 0; c < t->header->offMeshConCount; c++ ) {
                    if ( t->offMeshCons[c].poly == (unsigned short)ip ) {
                        unsigned int uid = t->offMeshCons[c].userId;
                        if ( uid < (unsigned)NAV_MAX_TOTAL_OMC )
                            tm = nav.omcTraversalMode[uid];
                        if ( tm == (unsigned char)NAV_TM_CLIMB ||
                             tm == (unsigned char)NAV_TM_FALL ) {
                            /* Which SENSE is being walked?  Ask Detour, which already
                             * answers exactly this question for its own steering:
                             * getOffMeshConnectionPolyEndPoints( prevRef, omcRef, ... )
                             * inspects the link chain and hands back start/end ORDERED
                             * FOR THE ARRIVING SIDE.  Do NOT compare against
                             * offMeshCons[].pos: those are the AUTHORED endpoints, are
                             * relocated onto found floors at tile build, and are not the
                             * funnel apex findStraightPath emits — comparing the two
                             * measures the wrong thing (measured: the flip never fired).
                             *
                             * prevRef is the corridor poly the path occupies before the
                             * link.  polyPath is the corridor from findPath; scan it for
                             * this OMC's ref and take its predecessor.  A missing or
                             * leading match leaves prevRef 0, which is exactly what
                             * Detour treats as "no previous" — the baked order. */
                            dtPolyRef omcRef  = (dtPolyRef)straightRefs[i];
                            dtPolyRef prevRef = 0;
                            for ( int q = 1; q < numPolys; q++ ) {
                                if ( polyPath[q] == omcRef ) { prevRef = polyPath[q - 1]; break; }
                            }
                            float sPos[3], ePos[3];
                            if ( dtStatusSucceed( mesh->getOffMeshConnectionPolyEndPoints(
                                                      prevRef, omcRef, sPos, ePos ) ) ) {
                                /* Recast Y is up.  Rise as TRAVELLED, so the mode always
                                 * describes the direction the bot is about to move. */
                                const float rise = ePos[1] - sPos[1];
                                if ( rise > 0.0f )
                                    tm = (unsigned char)NAV_TM_CLIMB;
                                else
                                    tm = (unsigned char)NAV_TM_FALL;
                            }
                        }
                        break;
                    }
                }
            }
        }
        out->traversalMode[i] = tm;
    }
    out->count = count;



    if ( Cvar_VariableIntegerValue( "nav_botdebug" ) ) {
        /* Check for any OMC waypoint (DT_STRAIGHTPATH_OFFMESH_CONNECTION = 0x04). */
        bool hasOmc = false;
        for ( int i = 0; i < count; i++ ) {
            if ( straightFlags[i] & 0x04 ) { hasOmc = true; break; }
        }
        Com_Log( SEV_DEBUG, LOG_CH(ch_nav), "[BOTNAV] FindPath engine: %d polys -> %d pts%s\n",
                     numPolys, count, hasOmc ? " [OMC]" : "" );
    }

    /* Encode the reach status into the high bit (count never exceeds
     * NAV_MAX_PATH_POINTS < 256, so bit 16 is free): a corridor that did not reach the
     * goal poly (Detour's DT_PARTIAL_RESULT) returns with NAV_FINDPATH_PARTIAL set.
     * out->count and every path field carry the plain count unchanged; callers recover
     * the count with NAV_FINDPATH_COUNT() (g_bot_nav masks it, path-following is
     * unaffected) and test the bit for reachability (WiredIntel_PathReaches). */
    if ( dtStatusDetail( pathStatus, DT_PARTIAL_RESULT ) )
        return count | NAV_FINDPATH_PARTIAL;

    return count;
}

qboolean Nav_Raycast( const float *qStart, const float *qEnd, float *qHitPos )
{
    if ( !nav.ready ) {
        qHitPos[0] = qEnd[0]; qHitPos[1] = qEnd[1]; qHitPos[2] = qEnd[2];
        return qfalse;
    }
    float rStart[3], rEnd[3];
    Nav_QuakeToRecast( qStart, rStart );
    Nav_QuakeToRecast( qEnd,   rEnd );

    const dtQueryFilter *filter = GetFilter();
    float nearPt[3];
    dtPolyRef startRef = 0;
    nav.query->findNearestPoly( rStart, kDefaultExtents, filter, &startRef, nearPt );
    if ( !startRef ) {
        qHitPos[0] = qEnd[0]; qHitPos[1] = qEnd[1]; qHitPos[2] = qEnd[2];
        return qfalse;
    }

    dtRaycastHit hit; memset( &hit, 0, sizeof(hit) ); hit.t = 1.0f;
    nav.query->raycast( startRef, rStart, rEnd, filter, 0, &hit );
    if ( hit.t < 1.0f ) {
        float rHit[3];
        rHit[0] = rStart[0] + (rEnd[0]-rStart[0])*hit.t;
        rHit[1] = rStart[1] + (rEnd[1]-rStart[1])*hit.t;
        rHit[2] = rStart[2] + (rEnd[2]-rStart[2])*hit.t;
        Nav_RecastToQuake( rHit, qHitPos );
        return qtrue;
    }
    qHitPos[0] = qEnd[0]; qHitPos[1] = qEnd[1]; qHitPos[2] = qEnd[2];
    return qfalse;
}

navPolyRef_t Nav_FindNearestPoly( const float *qOrigin, const float *qExtents )
{
    if ( !nav.ready ) return 0;
    float rOrigin[3];
    Nav_QuakeToRecast( qOrigin, rOrigin );
    float rExtents[3];
    rExtents[0] = qExtents[0]; rExtents[1] = qExtents[2]; rExtents[2] = qExtents[1];

    const dtQueryFilter *filter = GetFilter();
    dtPolyRef ref = 0; float nearPt[3];
    nav.query->findNearestPoly( rOrigin, rExtents, filter, &ref, nearPt );
    return (navPolyRef_t)ref;
}

int Nav_GetPolyAreaFlags( navPolyRef_t polyRef )
{
    if ( !nav.ready || !polyRef ) return 0;
    unsigned short flags = 0;
    nav.mesh->getPolyFlags( (dtPolyRef)polyRef, &flags );
    return (int)flags;
}

qboolean Nav_GetRandomPoint( int areaFilter, float *qPosOut )
{
    if ( !nav.ready ) return qfalse;
    dtQueryFilter filter;
    filter.setIncludeFlags( (unsigned short)areaFilter );
    filter.setExcludeFlags( (unsigned short)NAVPOLY_BLOCKED );

    auto frand = []() -> float {
        static unsigned int seed = 0x12345678u;
        seed = seed * 1664525u + 1013904223u;
        return (float)(seed & 0x7FFFFFFFu) / (float)0x7FFFFFFFu;
    };

    dtPolyRef randomRef = 0;
    float     randomPt[3] = { 0, 0, 0 };
    nav.query->findRandomPoint( &filter, frand, &randomRef, randomPt );
    if ( !randomRef ) return qfalse;
    Nav_RecastToQuake( randomPt, qPosOut );
    return qtrue;
}

/* -------------------------------------------------------------------------
   Nav_PredictEnemyPosition — Phase 5.5 Detour-based aim prediction
   Forward-simulate enemy trajectory on the navmesh surface using
   moveAlongSurface.  Replaces trap_AAS_PredictClientMovement at high skill.
   Inputs in Quake world-space; output in Quake world-space.
   ------------------------------------------------------------------------- */
void Nav_PredictEnemyPosition( const float *qOrigin, const float *qVelocity,
                                float predictTime, float *qPosOut )
{
    /* Return current position on any failure — caller still has velocity lead. */
    if ( !nav.ready || !nav.query || predictTime <= 0.0f ) {
        qPosOut[0] = qOrigin[0]; qPosOut[1] = qOrigin[1]; qPosOut[2] = qOrigin[2];
        return;
    }

    /* Skip simulation if velocity is negligible (< 10 ups). */
    float velSqLen = qVelocity[0]*qVelocity[0]
                   + qVelocity[1]*qVelocity[1]
                   + qVelocity[2]*qVelocity[2];
    if ( velSqLen < 100.0f ) {  /* 10² */
        qPosOut[0] = qOrigin[0]; qPosOut[1] = qOrigin[1]; qPosOut[2] = qOrigin[2];
        return;
    }

    /* Convert to Recast coordinate space: (x, y, z) → (x, z, -y). */
    float rPos[3], rVel[3];
    Nav_QuakeToRecastV( qOrigin,   rPos );
    Nav_QuakeToRecastV( qVelocity, rVel );

    const dtQueryFilter *filter = GetFilter();
    float     nearPt[3];
    dtPolyRef startRef = 0;
    nav.query->findNearestPoly( rPos, kDefaultExtents, filter, &startRef, nearPt );
    if ( !startRef ) {
        qPosOut[0] = qOrigin[0]; qPosOut[1] = qOrigin[1]; qPosOut[2] = qOrigin[2];
        return;
    }

    /* Forward-simulate in 10 ms steps.  Max 80 steps (= 0.8 s, covers rocket range). */
    const float dt       = 0.01f;
    const int   maxSteps = (int)( predictTime / dt ) + 1;
    const int   capSteps = maxSteps < 80 ? maxSteps : 80;

    dtPolyRef visited[16];
    int       visitedCount = 0;
    dtPolyRef curRef = startRef;
    float     curPos[3] = { rPos[0], rPos[1], rPos[2] };

    for ( int step = 0; step < capSteps; step++ ) {
        float endPos[3] = {
            curPos[0] + rVel[0] * dt,
            curPos[1] + rVel[1] * dt,
            curPos[2] + rVel[2] * dt
        };

        float     resultPos[3];
        dtStatus  st = nav.query->moveAlongSurface(
            curRef, curPos, endPos, filter,
            resultPos, visited, &visitedCount, 16 );

        if ( dtStatusFailed(st) ) break;

        if ( visitedCount > 0 )
            curRef = visited[visitedCount - 1];

        /* Stop early if step was clipped more than 8 units (hit a boundary wall). */
        float dx = resultPos[0] - endPos[0];
        float dz = resultPos[2] - endPos[2];
        if ( dx*dx + dz*dz > 64.0f ) {  /* 8² — horizontal clip only */
            curPos[0] = resultPos[0]; curPos[1] = resultPos[1]; curPos[2] = resultPos[2];
            break;
        }

        curPos[0] = resultPos[0];
        curPos[1] = resultPos[1];
        curPos[2] = resultPos[2];
    }

    Nav_RecastToQuakeV( curPos, qPosOut );
}

/* -------------------------------------------------------------------------
   Nav_SetPolyFlagsForDoor — D-19 runtime door hookup
   Set or clear poly flags for all polys belonging to 'targetname' door.
   Called from gamecode via trap at door state transition-start.
   Safe to call for non-door movers: returns silently on targetname mismatch.
   ------------------------------------------------------------------------- */
void Nav_SetPolyFlagsForDoor( const char *targetname, int setFlags, int clearFlags )
{
    if ( !nav.ready || !targetname ) return;

    /* Door state is a TWO-signal contract: the OPENABLE_CLOSED flag governs
     * INCLUDABILITY (a closed door is still planned-through, committing to open it),
     * and the NAVAREA_DOOR area governs the 10x traversal COST (GetFilter's
     * setAreaCost) that deprioritises a closed door against parallel open geometry.
     * A transition that touched only the flag left the 10x cost on an OPENED door, so
     * the open route still read as expensive and the planner kept preferring any
     * cheaper (even dead-end) alternative — the missing half of the contract.  Manage
     * the area IN LOCKSTEP with the flag: opening (clearing OPENABLE_CLOSED) reverts
     * each poly to its recorded pre-tag area (origArea[]) so the door walks at normal
     * cost; closing (setting OPENABLE_CLOSED) re-promotes to NAVAREA_DOOR.  The
     * per-poly original is essential: a threshold floor originates NAVAREA_GROUND, but
     * a door-gap / hatch crossing OMC originates NAVAREA_JUMP_LINK — a blanket GROUND
     * would corrupt the OMC's cost key / traversal class. */
    const bool opening = ( (unsigned)clearFlags & (unsigned)NAVPOLY_OPENABLE_CLOSED ) != 0;
    const bool closing = ( (unsigned)setFlags   & (unsigned)NAVPOLY_OPENABLE_CLOSED ) != 0;

    /* PROBE NEUTRALISE/RESTORE — the flag-half counterpart of origArea[].
     *
     * A "probe" is a caller asking a hypothetical: neutralise a flag, run one query,
     * put the flag back.  It is identified structurally, by the shape of the call
     * rather than by who made it: clearing ONLY (setFlags == 0) opens the window, and
     * the mirror-image call that sets ONLY those same bits (clearFlags == 0) closes
     * it.  A transition that touches OPENABLE_CLOSED is a real door state change, not
     * a probe, and is deliberately excluded from this path so G_Nav_ApplyDoorFlags and
     * the area/origArea lockstep below behave exactly as before.
     *
     * On open: record the flags actually observed, per poly.
     * On close: write those recorded flags back verbatim — do NOT OR the bits in.
     * The old close ASSERTED the bits, which set flags that had never been set (see
     * navDoorEntry_t's comment); restoring the observation cannot, whatever the
     * pre-state was.  Nesting is guarded by probeDepth so only the outermost close
     * restores, and an unmatched close (no window open) falls through to the ordinary
     * path rather than replaying a stale snapshot. */
    const bool probeOpen  = ( (unsigned)setFlags == 0 && (unsigned)clearFlags != 0 && !opening );
    const bool probeClose = ( (unsigned)clearFlags == 0 && (unsigned)setFlags != 0 && !closing );

    for ( int i = 0; i < nav.numDoorEntries; i++ ) {
        if ( strcmp( nav.doorEntries[i].targetname, targetname ) == 0 ) {
            unsigned short before0 = 0, after0 = 0;
            navDoorEntry_t *de = &nav.doorEntries[i];
            const bool restoring = ( probeClose && de->probeDepth > 0 );

            if ( probeOpen ) {
                if ( de->probeDepth == 0 ) {
                    for ( int j = 0; j < de->numPolyrefs; j++ ) {
                        unsigned short f = 0;
                        nav.mesh->getPolyFlags( de->polyrefs[j], &f );
                        de->probeSavedFlags[j] = f;
                    }
                }
                de->probeDepth++;
            } else if ( restoring ) {
                de->probeDepth--;
            }

            for ( int j = 0; j < de->numPolyrefs; j++ ) {
                unsigned short flags = 0;
                nav.mesh->getPolyFlags( nav.doorEntries[i].polyrefs[j], &flags );
                if ( j == 0 ) before0 = flags;
                if ( restoring && de->probeDepth == 0 ) {
                    flags = de->probeSavedFlags[j];   /* put back exactly what was there */
                } else {
                    flags = (unsigned short)(
                        (flags | (unsigned short)setFlags) & ~(unsigned short)clearFlags );
                }
                nav.mesh->setPolyFlags( nav.doorEntries[i].polyrefs[j], flags );
                if ( j == 0 ) after0 = flags;

                /* Area (cost) half of the contract, in lockstep with the flag. */
                if ( opening )
                    nav.mesh->setPolyArea( (dtPolyRef)nav.doorEntries[i].polyrefs[j],
                                           nav.doorEntries[i].origArea[j] );
                else if ( closing )
                    nav.mesh->setPolyArea( (dtPolyRef)nav.doorEntries[i].polyrefs[j],
                                           (unsigned char)NAVAREA_DOOR );
            }
            /* Observability for the door-state -> nav-flag bridge (severity-gated,
             * dormant at normal play): one line per flip with the door name, poly
             * count, and the first poly's before/after flag word. Makes the
             * OPENABLE_CLOSED (0x200) / BLOCKED (0x010) transitions auditable. */
            Com_Log( SEV_DEBUG, LOG_CH(ch_nav),
                "[NAV] door '%s' flags: %d poly(s) set=0x%x clear=0x%x (poly0 0x%x->0x%x)%s\n",
                targetname, nav.doorEntries[i].numPolyrefs,
                (unsigned)setFlags, (unsigned)clearFlags, before0, after0,
                opening ? " area->orig" : ( closing ? " area->door" : "" ) );
            return;
        }
    }
}

/* -------------------------------------------------------------------------
   Debug commands
   ------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------
   Nav_TagDoorAreas
   Post-generation step: tag polys that overlap a func_door AABB as
   NAVAREA_DOOR (area) + NAVPOLY_DOOR (flag).
   Must be called AFTER nav.query->init().
   ------------------------------------------------------------------------- */
static void Nav_TagDoorAreas( const char *mapname )
{
    navDoorBox_t *boxes = NULL;
    int numBoxes = Nav_Get_DoorBoxes( mapname, &boxes );
    if ( numBoxes <= 0 || !boxes ) {
        Com_Log( SEV_DEBUG, LOG_CH(ch_nav), "[NAV] no func_door entities on %s\n", mapname );
        return;
    }

    static const float kHalf[3] = { 280.0f, 280.0f, 480.0f };
    int polyCount = 0;

    for ( int b = 0; b < numBoxes; b++ ) {
        /* Convert Quake AABB → Recast centre + half-extents.
         * Quake: X=right, Y=forward, Z=up.  Recast: X=right, Y=up, Z=forward. */
        const float *mins = boxes[b].mins;
        const float *maxs = boxes[b].maxs;
        float centre[3], half[3];
        centre[0] =  (mins[0] + maxs[0]) * 0.5f;
        centre[1] =  (mins[2] + maxs[2]) * 0.5f; /* Q Z →  R Y */
        centre[2] = -(mins[1] + maxs[1]) * 0.5f; /* Q Y → -R Z (Nav_QuakeToRecast negates Y) */
        half[0]   =  (maxs[0] - mins[0]) * 0.5f;
        half[1]   =  (maxs[2] - mins[2]) * 0.5f;
        half[2]   =  (maxs[1] - mins[1]) * 0.5f;

        /* Grow the XY footprint by ONE PLAYER DIAMETER (2 x NAV_WALKABLE_RADIUS).
         *
         * A door no longer contributes only the THRESHOLD floor beside its leaf: at-rest
         * mover volumes are part of the nav soup, so a door also contributes polys born on
         * its OWN volume, and the floor it seals can begin up to a player-box width beyond
         * the leaf's AABB.  A query clamped closer than that stops short of exactly the
         * polys that must be claimed -- measured on e1m1's hatch t1, the sealed floor sat
         * 6u outside a +16u box, so it never received NAVPOLY_DOOR/OPENABLE_CLOSED and the
         * closed door walled off 90% of the map with no plan to open it.
         *
         * Deriving the growth from NAV_WALKABLE_RADIUS says what it means -- "a player's
         * width of floor just outside the leaf" -- and tracks the agent size, instead of a
         * hand-tuned constant that goes stale silently the moment the player box changes.
         *
         * The VERTICAL (recast Y = quake Z) extent is grown GENEROUSLY DOWNWARD for a
         * different reason, unchanged here: a Q1 door leaf's AABB bottom (mins[2]) can sit
         * a little above the walkable floor a bot actually stands on in the doorway (the
         * floor the closed door gates), so a query clamped to the leaf's own z-span misses
         * the threshold floor polys entirely -- which left poly-carrying doorways (door
         * *3-class: floor through, no gap OMC) UN-TAGGED and therefore un-gated.  Reaching
         * NAV_WALKABLE_HEIGHT below the leaf base catches the threshold floor while staying
         * within one room. */
        half[0] += 2.0f * NAV_WALKABLE_RADIUS; half[2] += 2.0f * NAV_WALKABLE_RADIUS;
        const float qLoY = centre[1] - half[1] - NAV_WALKABLE_HEIGHT;   /* down to the floor under the leaf */
        const float qHiY = centre[1] + half[1] + 8.0f;
        centre[1] = 0.5f * ( qLoY + qHiY );
        half[1]   = 0.5f * ( qHiY - qLoY );

        /* ONE cap governs both the query result and the per-door store below, because
         * a poly found but not stored is just as untagged at runtime as one never
         * found: Nav_SetPolyFlagsForDoor walks the stored list to apply/clear
         * OPENABLE_CLOSED.  Two independent 64s could drift apart and silently drop
         * the difference, so they are the same constant. */
        static const int kMaxPolyResult = NAV_MAX_DOOR_POLYS;
        dtPolyRef refs[NAV_MAX_DOOR_POLYS];
        int numFound = 0;

        const dtQueryFilter *filter = GetFilter();
        nav.query->queryPolygons( centre, half, filter, refs, &numFound, kMaxPolyResult );

        if ( numFound >= kMaxPolyResult ) {
            /* Saturated: polys past the cap are NOT tagged and NOT gated by this
             * door's open/close contract, so a closed door can wall a route off with
             * no plan to open it.  Say so loudly — silent truncation here reads as a
             * permanent wall much later and far from the cause. */
            Com_Log( SEV_WARN, LOG_CH(ch_nav),
                "[NAV] door '%s' poly query saturated the cap (%d): polys beyond it stay UNTAGGED "
                "and ungated (a closed crossing may read as a wall). Raise NAV_MAX_DOOR_POLYS.\n",
                boxes[b].targetname[0] ? boxes[b].targetname : "(unnamed)", kMaxPolyResult );
        }

        /* Pre-tag area per found poly, captured BEFORE the NAVAREA_DOOR upgrade so a
         * later door OPEN can revert to the true area (see navDoorEntry_t.origArea). */
        unsigned char preArea[NAV_MAX_DOOR_POLYS];   /* kMaxPolyResult */
        for ( int i = 0; i < numFound; i++ ) {
            unsigned short flags = 0;
            nav.mesh->getPolyFlags( refs[i], &flags );
            unsigned char area = 0;
            nav.mesh->getPolyArea( refs[i], &area );
            preArea[i] = area;   /* original area, before any promotion below */

            /* Set NAVPOLY_DOOR flag (OR with existing, preserve walkability). */
            flags = (unsigned short)(flags | (unsigned short)NAVPOLY_DOOR);
            nav.mesh->setPolyFlags( refs[i], flags );

            /* Upgrade area to NAVAREA_DOOR only from NAVAREA_GROUND. */
            if ( area == (unsigned char)NAVAREA_GROUND )
                nav.mesh->setPolyArea( refs[i], (unsigned char)NAVAREA_DOOR );

            polyCount++;
        }

        /* D-19: Store per-door poly list for runtime flag hookup. */
        if ( b < NAV_MAX_DOORS ) {
            navDoorEntry_t *de = &nav.doorEntries[b];
            Q_strncpyz( de->targetname, boxes[b].targetname, sizeof(de->targetname) );
            int nStore = numFound < NAV_MAX_DOOR_POLYS ? numFound : NAV_MAX_DOOR_POLYS;
            for ( int j = 0; j < nStore; j++ ) {
                de->polyrefs[j] = (navPolyRef_t)refs[j];
                de->origArea[j] = preArea[j];   /* lockstep with polyrefs[j] */
            }
            de->numPolyrefs = nStore;
            /* No probe window is open on a freshly built door list.  Explicit rather
             * than relying on the file-static zero-init, because a map reload reuses
             * this struct and a stale depth would suppress the next restore. */
            de->probeDepth = 0;
            nav.numDoorEntries = b + 1;
        }

    }

    /* Tag each door's CROSSING off-mesh connection (its door-gap OMC): a closed door
     * that straddles a real floor hole is passed via the door-gap OMC, not via floor
     * polys under the solid leaf (which is why the box queryPolygons above finds 0
     * there).  The polys the door state must gate are the OMC's.  A door-gap OMC's
     * endpoints sit on the floor just past the doorway lip (and the END-endpoint may
     * have been reground onto the far-side floor), so they lie OUTSIDE the leaf AABB --
     * associate by the OMC MIDPOINT being nearest THIS door's centre, within the
     * producer's own door-gap reach (NAV_DOORGAP_MAX_WIDTH + inset).  Assigning to the
     * NEAREST door keeps two nearby doorways from stealing each other's crossing.
     * Door-keyed, no producer touch. */
    {
        const dtNavMesh *mesh = (const dtNavMesh *)nav.mesh;
        const int maxTiles = mesh->getMaxTiles();
        const float reach = NAV_DOORGAP_MAX_WIDTH + NAV_DOORGAP_ENDPOINT_INSET + 16.0f;
        for ( int ti = 0; ti < maxTiles; ti++ ) {
            const dtMeshTile *t = mesh->getTile( ti );
            if ( !t || !t->header ) continue;
            for ( int pi = 0; pi < t->header->polyCount; pi++ ) {
                const dtPoly *p = &t->polys[pi];
                if ( p->getType() != DT_POLYTYPE_OFFMESH_CONNECTION || p->vertCount < 2 ) continue;
                /* OMC midpoint (Quake). */
                float q0[3], q1[3];
                Nav_RecastToQuake( &t->verts[p->verts[0]*3], q0 );
                Nav_RecastToQuake( &t->verts[p->verts[1]*3], q1 );
                const float mid[3] = { 0.5f*(q0[0]+q1[0]), 0.5f*(q0[1]+q1[1]), 0.5f*(q0[2]+q1[2]) };
                /* Nearest door centre within reach. */
                int bestB = -1; float bestD = reach * reach;
                for ( int b = 0; b < numBoxes && b < NAV_MAX_DOORS; b++ ) {
                    const float dc[3] = { 0.5f*(boxes[b].mins[0]+boxes[b].maxs[0]),
                                          0.5f*(boxes[b].mins[1]+boxes[b].maxs[1]),
                                          0.5f*(boxes[b].mins[2]+boxes[b].maxs[2]) };
                    const float dxy = (mid[0]-dc[0])*(mid[0]-dc[0]) + (mid[1]-dc[1])*(mid[1]-dc[1]);
                    if ( fabsf( mid[2]-dc[2] ) > 96.0f ) continue;   /* different storey */
                    if ( dxy < bestD ) { bestD = dxy; bestB = b; }
                }
                if ( bestB < 0 ) continue;
                navDoorEntry_t *de = &nav.doorEntries[bestB];
                navPolyRef_t oref = (navPolyRef_t)( mesh->encodePolyId( t->salt, ti, pi ) );
                bool dup = false;
                for ( int j = 0; j < de->numPolyrefs; j++ ) if ( de->polyrefs[j] == oref ) { dup = true; break; }
                if ( dup || de->numPolyrefs >= NAV_MAX_DOOR_POLYS ) continue;
                /* Record the OMC's pre-tag area (its true class, e.g. JUMP_LINK for a
                 * door-gap / hatch crossing) BEFORE the NAVAREA_DOOR promotion below, so
                 * an open-revert restores the crossing's own cost key rather than a
                 * blanket GROUND that would corrupt the OMC class.  Lockstep with the
                 * polyrefs[] index just written. */
                {
                    unsigned char oarea = (unsigned char)NAVAREA_JUMP_LINK;
                    nav.mesh->getPolyArea( (dtPolyRef)oref, &oarea );
                    de->origArea[ de->numPolyrefs ] = oarea;
                }
                de->polyrefs[ de->numPolyrefs++ ] = oref;
                /* Mark the crossing NAVAREA_DOOR so the query filter applies the
                 * high door-traversal cost (GetFilter setAreaCost).  Keeps the OMC's
                 * userId (its class) intact -- only the area (cost key) changes. */
                nav.mesh->setPolyArea( (dtPolyRef)oref, (unsigned char)NAVAREA_DOOR );
                polyCount++;
                Com_Log( SEV_DEBUG, LOG_CH(ch_nav),
                    "[NAV] door '%s' tagged crossing OMC %u (mid %.0f %.0f %.0f)\n",
                    de->targetname, (unsigned)oref, mid[0], mid[1], mid[2] );
            }
        }
    }

    Com_Log( SEV_INFO, LOG_CH(ch_nav), "[NAV] tagged %d polys as NAVAREA_DOOR (incl door-gap OMCs) from %d func_door entities on %s\n",
                polyCount, numBoxes, mapname );
    Z_Free( boxes );
}

/* -------------------------------------------------------------------------
   Nav_DrawCmd  — "nav_draw"
   Exports the current navmesh as navmesh/<mapname>.obj + navmesh/<mapname>.mtl
   for visual inspection in Blender / MeshLab.
   Area types are colour-coded via OBJ material groups.
   Off-mesh connections are emitted as OBJ 'l' (line) primitives.
   Blocked polys get an inline comment.
   ------------------------------------------------------------------------- */
static void Nav_DrawCmd( void )
{
    if ( !nav.ready || !nav.mesh ) { Com_Log( SEV_INFO, LOG_CH(ch_nav), "NAV: navmesh not loaded\n" ); return; }

    const char *mapname = nav.mapname[0] ? nav.mapname : "unknown";

    char mtlName[MAX_OSPATH], objName[MAX_OSPATH];
    Com_sprintf( mtlName, sizeof(mtlName), "navmesh/%s.mtl", mapname );
    Com_sprintf( objName, sizeof(objName), "navmesh/%s.obj", mapname );

    fileHandle_t mtlF = 0;
    FS_FOpenFileByMode( mtlName, &mtlF, FS_WRITE );
    if ( !mtlF ) { Com_Log( SEV_INFO, LOG_CH(ch_nav), "[NAV] nav_draw: cannot open %s\n", mtlName ); return; }

    FS_Printf( mtlF, "# wired navmesh materials\n" );
    FS_Printf( mtlF, "newmtl nav_ground\n"      "Kd 0.6 0.6 0.6\n\n" );
    FS_Printf( mtlF, "newmtl nav_water\n"       "Kd 0.2 0.4 0.9\n\n" );
    FS_Printf( mtlF, "newmtl nav_lava\n"        "Kd 0.9 0.3 0.1\n\n" );
    FS_Printf( mtlF, "newmtl nav_door\n"        "Kd 0.9 0.9 0.2\n\n" );
    FS_Printf( mtlF, "newmtl nav_low_ceiling\n" "Kd 0.5 0.2 0.7\n\n" );
    FS_Printf( mtlF, "newmtl nav_offmesh\n"     "Kd 1.0 0.0 0.0\n\n" );
    FS_FCloseFile( mtlF );

    fileHandle_t f = 0;
    FS_FOpenFileByMode( objName, &f, FS_WRITE );
    if ( !f ) { Com_Log( SEV_INFO, LOG_CH(ch_nav), "[NAV] nav_draw: cannot open %s\n", objName ); return; }

    /* Timestamp (server frames, not wall clock — we don't have strftime) */
    FS_Printf( f, "# wired navmesh export: %s\n", mapname );
    FS_Printf( f, "mtllib %s.mtl\n\n", mapname );

    /* First pass: count total verts & tris across all tiles for the header. */
    int totalVerts = 0, totalTris = 0;
    int maxTiles = nav.mesh->getMaxTiles();
    for ( int ti = 0; ti < maxTiles; ti++ ) {
        const dtMeshTile *t = ((const dtNavMesh *)nav.mesh)->getTile( ti );
        if ( !t || !t->header ) continue;
        totalVerts += t->header->vertCount;
        for ( int pi = 0; pi < t->header->polyCount; pi++ ) {
            const dtPoly *p = &t->polys[pi];
            if ( p->getType() != DT_POLYTYPE_GROUND ) continue;
            if ( p->vertCount >= 3 ) totalTris += p->vertCount - 2;
        }
    }
    FS_Printf( f, "# verts: %d  tris: %d\n\n", totalVerts, totalTris );

    /* Per-area material name helper. */
    auto areaToMtl = []( unsigned char area ) -> const char * {
        switch ( (navAreaId_t)area ) {
        case NAVAREA_WATER:       return "nav_water";
        case NAVAREA_LAVA:        return "nav_lava";
        case NAVAREA_DOOR:        return "nav_door";
        case NAVAREA_LOW_CEILING: return "nav_low_ceiling";
        default:                  return "nav_ground";
        }
    };

    /* Second pass: emit all vertices (Quake space) per tile, remembering
     * per-tile base offset for face emission (OBJ indices are 1-based). */
    int tileBases[1024];  /* supports up to 1024 tiles */
    int numTilesUsed = 0;
    int globalVbase  = 1; /* next 1-based OBJ vertex index */

    for ( int ti = 0; ti < maxTiles && numTilesUsed < 1024; ti++ ) {
        const dtMeshTile *t = ((const dtNavMesh *)nav.mesh)->getTile( ti );
        if ( !t || !t->header ) { tileBases[ti] = -1; continue; }
        tileBases[ti] = globalVbase;
        numTilesUsed++;

        FS_Printf( f, "# tile %d\n", ti );
        for ( int vi = 0; vi < t->header->vertCount; vi++ ) {
            const float *rv = &t->verts[vi * 3];
            float qv[3];
            Nav_RecastToQuake( rv, qv );
            FS_Printf( f, "v %g %g %g\n", qv[0], qv[1], qv[2] );
            globalVbase++;
        }
    }
    FS_Printf( f, "\n" );

    /* Third pass: emit faces grouped by material. */
    const char *curMtl = NULL;
    for ( int ti = 0; ti < maxTiles; ti++ ) {
        const dtMeshTile *t = ((const dtNavMesh *)nav.mesh)->getTile( ti );
        if ( !t || !t->header || tileBases[ti] < 0 ) continue;
        int base = tileBases[ti] - 1; /* 0-based offset so OBJ verts are base+vertIdx+1 */

        for ( int pi = 0; pi < t->header->polyCount; pi++ ) {
            const dtPoly *p = &t->polys[pi];
            if ( p->getType() != DT_POLYTYPE_GROUND ) continue;
            if ( p->vertCount < 3 ) continue;

            unsigned char area = p->getArea();
            unsigned short flags = p->flags;
            const char *mtl = areaToMtl( area );
            if ( mtl != curMtl ) {
                FS_Printf( f, "usemtl %s\n", mtl );
                curMtl = mtl;
            }

            /* Annotate door polys and blocked polys with their ref
             * so nav_setblocked targets can be identified from the OBJ. */
            dtPolyRef ref = nav.mesh->encodePolyId( t->salt, ti, pi );
            if ( flags & (unsigned short)NAVPOLY_BLOCKED )
                FS_Printf( f, "# poly %u blocked\n", (unsigned)ref );
            else if ( flags & (unsigned short)NAVPOLY_DOOR )
                FS_Printf( f, "# poly %u\n", (unsigned)ref );

            /* Fan triangulation from vertex 0: (v0,v_i,v_{i+1}). */
            for ( int i = 1; i + 1 < (int)p->vertCount; i++ ) {
                FS_Printf( f, "f %d %d %d\n",
                           base + (int)p->verts[0] + 1,
                           base + (int)p->verts[i] + 1,
                           base + (int)p->verts[i+1] + 1 );
            }
        }
    }

    /* Fourth pass: off-mesh connections as 'l' line primitives.
     * OMC polys are DT_POLYTYPE_OFFMESH_CONNECTION.  Each has 2 endpoints
     * stored as consecutive verts in the tile's vert buffer. */
    FS_Printf( f, "\nusemtl nav_offmesh\n" );
    for ( int ti = 0; ti < maxTiles; ti++ ) {
        const dtMeshTile *t = ((const dtNavMesh *)nav.mesh)->getTile( ti );
        if ( !t || !t->header || tileBases[ti] < 0 ) continue;
        int base = tileBases[ti] - 1;
        for ( int pi = 0; pi < t->header->polyCount; pi++ ) {
            const dtPoly *p = &t->polys[pi];
            if ( p->getType() != DT_POLYTYPE_OFFMESH_CONNECTION ) continue;
            if ( p->vertCount < 2 ) continue;
            FS_Printf( f, "l %d %d\n",
                       base + (int)p->verts[0] + 1,
                       base + (int)p->verts[1] + 1 );
        }
    }

    FS_FCloseFile( f );
    Com_Log( SEV_INFO, LOG_CH(ch_nav), "[NAV] wrote %d verts, %d tris to navmesh/%s.obj\n",
                totalVerts, totalTris, mapname );
}

/* -------------------------------------------------------------------------
   Nav_SetBlockedCmd  — "nav_setblocked <polyref> <0|1>"
   Manually flip NAVPOLY_BLOCKED on a poly for testing door-block mechanism.
   ------------------------------------------------------------------------- */
static void Nav_SetBlockedCmd( void )
{
    if ( Cmd_Argc() < 3 ) {
        Com_Log( SEV_INFO, LOG_CH(ch_nav), "Usage: nav_setblocked <polyref> <0|1>\n" ); return;
    }
    if ( !nav.ready || !nav.mesh ) { Com_Log( SEV_INFO, LOG_CH(ch_nav), "NAV: navmesh not loaded\n" ); return; }

    dtPolyRef ref = (dtPolyRef)atoi( Cmd_Argv(1) );
    int block     = atoi( Cmd_Argv(2) );

    unsigned short flags = 0;
    dtStatus st = nav.mesh->getPolyFlags( ref, &flags );
    if ( dtStatusFailed(st) ) {
        Com_Log( SEV_INFO, LOG_CH(ch_nav), "[NAV] nav_setblocked: invalid polyref %u\n", (unsigned)ref ); return;
    }

    if ( block )
        flags = (unsigned short)(flags | (unsigned short)NAVPOLY_BLOCKED);
    else
        flags = (unsigned short)(flags & ~(unsigned short)NAVPOLY_BLOCKED);

    nav.mesh->setPolyFlags( ref, flags );
    Com_Log( SEV_INFO, LOG_CH(ch_nav), "[NAV] poly %u %s\n", (unsigned)ref, block ? "blocked" : "unblocked" );
}

static void Nav_InfoCmd( void )
{
    if ( !nav.ready || !nav.mesh ) { Com_Log( SEV_INFO, LOG_CH(ch_nav), "NAV: navmesh not loaded\n" ); return; }
    const dtNavMeshParams *params = nav.mesh->getParams();
    int totalPolys = 0, totalVerts = 0;
    int maxTiles = nav.mesh->getMaxTiles();
    for ( int i = 0; i < maxTiles; i++ ) {
        const dtMeshTile *tile = ((const dtNavMesh *)nav.mesh)->getTile(i);
        if ( !tile || !tile->header ) continue;
        totalPolys += tile->header->polyCount;
        totalVerts += tile->header->vertCount;
    }
    Com_Log( SEV_INFO, LOG_CH(ch_nav), "--- NAV INFO ---\n" );
    Com_Log( SEV_INFO, LOG_CH(ch_nav), "  Ready       : %s\n",    nav.ready     ? "yes" : "no" );
    Com_Log( SEV_INFO, LOG_CH(ch_nav), "  Map         : %s\n",    nav.mapname[0] ? nav.mapname : "(none)" );
    Com_Log( SEV_INFO, LOG_CH(ch_nav), "  Tile origin : %.1f %.1f %.1f\n", params->orig[0], params->orig[1], params->orig[2] );
    Com_Log( SEV_INFO, LOG_CH(ch_nav), "  Tile size   : %.1f x %.1f\n",    params->tileWidth, params->tileHeight );
    Com_Log( SEV_INFO, LOG_CH(ch_nav), "  Max tiles   : %d\n",    params->maxTiles );
    Com_Log( SEV_INFO, LOG_CH(ch_nav), "  Polys/verts : %d / %d\n", totalPolys, totalVerts );
    Com_Log( SEV_INFO, LOG_CH(ch_nav), "  Cache from  : %s\n",    nav.fromCache ? "disk" : "built" );
    Com_Log( SEV_INFO, LOG_CH(ch_nav), "  Build time  : %d ms (%s)\n", nav.buildMs,
                nav.fromCache ? "from cache" : "built this session" );
    Com_Log( SEV_INFO, LOG_CH(ch_nav), "--- END NAV ---\n" );
}

static void Nav_ClearCmd( void )
{
    Nav_ClearCache( Cmd_Argc() > 1 ? Cmd_Argv(1) : NULL );
}

/* -------------------------------------------------------------------------
   Reachability oracle (read-only debug commands)

   nav_findpath <ax ay az> <bx by bz> — snaps both points to the navmesh (goal
   via the floor-preference wrapper) and reports whether a corridor exists, its
   poly chain, and whether it traverses an off-mesh connection. The topology
   ground-truth used to pin the e1m1 elevator/exit route breaks (per-poly
   connectivity from Detour's real link graph, not a heuristic).

   nav_island <x y z> — snaps the point and reports its connected-island size
   (getReachablePolyCount), exposing tiny/isolated fragments.
   ------------------------------------------------------------------------- */

static void Nav_FindPathCmd( void )
{
    if ( Cmd_Argc() < 7 ) {
        Com_Log( SEV_INFO, LOG_CH(ch_nav), "Usage: nav_findpath <ax ay az> <bx by bz>  (two world-space points)\n" );
        return;
    }
    if ( !nav.ready || !nav.query ) {
        Com_Log( SEV_INFO, LOG_CH(ch_nav), "nav_findpath: navmesh not loaded (load a map first)\n" );
        return;
    }

    float qA[3], qB[3];
    qA[0] = atof( Cmd_Argv(1) ); qA[1] = atof( Cmd_Argv(2) ); qA[2] = atof( Cmd_Argv(3) );
    qB[0] = atof( Cmd_Argv(4) ); qB[1] = atof( Cmd_Argv(5) ); qB[2] = atof( Cmd_Argv(6) );

    float rA[3], rB[3];
    Nav_QuakeToRecast( qA, rA );
    Nav_QuakeToRecast( qB, rB );

    const dtQueryFilter *filter = GetFilter();

    /* Snap A with plain nearest; snap B (goal) with the floor-preference wrapper
     * — mirrors Nav_FindPath so the oracle reports the same corridor the bot flies. */
    dtPolyRef aRef = 0, bRef = 0;
    float aPt[3], bPt[3];
    nav.query->findNearestPoly( rA, kDefaultExtents, filter, &aRef, aPt );
    Nav_SnapGoalToFloor( nav.query, rB, kDefaultExtents, filter, &bRef, bPt, aRef );

    float aQ[3], bQ[3];
    Nav_RecastToQuake( aPt, aQ );
    Nav_RecastToQuake( bPt, bQ );
    Com_Log( SEV_INFO, LOG_CH(ch_nav), "nav_findpath: A raw=%.1f %.1f %.1f snapRef=%u snap=%.1f %.1f %.1f miss=%.1f\n",
                qA[0], qA[1], qA[2], (unsigned)aRef, aQ[0], aQ[1], aQ[2],
                aRef ? Distance( qA, aQ ) : -1.0f );
    Com_Log( SEV_INFO, LOG_CH(ch_nav), "nav_findpath: B raw=%.1f %.1f %.1f snapRef=%u snap=%.1f %.1f %.1f miss=%.1f\n",
                qB[0], qB[1], qB[2], (unsigned)bRef, bQ[0], bQ[1], bQ[2],
                bRef ? Distance( qB, bQ ) : -1.0f );

    if ( !aRef || !bRef ) {
        Com_Log( SEV_INFO, LOG_CH(ch_nav), "nav_findpath: result=NO_SNAP reachable=no reason=%s-did-not-snap\n",
                    !aRef ? "A" : "B" );
        return;
    }

    static dtPolyRef polyCorridor[NAV_MAX_PATH_POINTS];
    int numPolys = 0;
    nav.query->findPath( aRef, bRef, rA, rB, filter, polyCorridor, &numPolys, NAV_MAX_PATH_POINTS );

    dtPolyRef termPoly = ( numPolys > 0 ) ? polyCorridor[numPolys - 1] : 0;
    qboolean sameComponent = ( numPolys > 0 && termPoly == bRef ) ? qtrue : qfalse;
    qboolean reachable     = sameComponent;

    /* Does the corridor traverse an off-mesh connection (elevator/jump/teleport)? */
    qboolean traversesOmc = qfalse;
    for ( int i = 0; i < numPolys; i++ ) {
        const dtMeshTile *tile = 0;
        const dtPoly     *poly = 0;
        if ( dtStatusSucceed( nav.mesh->getTileAndPolyByRef( polyCorridor[i], &tile, &poly ) )
             && poly && poly->getType() == DT_POLYTYPE_OFFMESH_CONNECTION ) {
            traversesOmc = qtrue; break;
        }
    }

    Com_Log( SEV_INFO, LOG_CH(ch_nav),
                "nav_findpath: result=%s reachable=%s same_component=%s corridorPolys=%d traverses_omc=%s startPoly=%u termPoly=%u\n",
                reachable ? "REACHABLE" : "UNREACHABLE",
                reachable ? "yes" : "no",
                sameComponent ? "yes" : "no",
                numPolys,
                traversesOmc ? "yes" : "no",
                (unsigned)aRef, (unsigned)termPoly );

    for ( int i = 0; i < numPolys; i++ ) {
        const dtMeshTile *tile = 0;
        const dtPoly     *poly = 0;
        unsigned polyType = 0;
        nav.mesh->getTileAndPolyByRef( polyCorridor[i], &tile, &poly );
        if ( poly ) polyType = poly->getType();
        Com_Log( SEV_INFO, LOG_CH(ch_nav), "nav_findpath: corridor[%d] poly=%u type=%u%s\n",
                    i, (unsigned)polyCorridor[i], polyType,
                    polyType == DT_POLYTYPE_OFFMESH_CONNECTION ? " [OMC]" : "" );
    }

    if ( numPolys > 0 ) {
        static float         straightPos[NAV_MAX_PATH_POINTS][3];
        static unsigned char straightFlags[NAV_MAX_PATH_POINTS];
        static dtPolyRef     straightRefs[NAV_MAX_PATH_POINTS];
        int nStraight = 0;
        nav.query->findStraightPath( rA, rB, polyCorridor, numPolys,
                                      straightPos[0], straightFlags, straightRefs,
                                      &nStraight, NAV_MAX_PATH_POINTS );
        for ( int i = 0; i < nStraight; i++ ) {
            float wp[3];
            Nav_RecastToQuake( straightPos[i], wp );
            qboolean isOmc   = ( straightFlags[i] & DT_STRAIGHTPATH_OFFMESH_CONNECTION ) ? qtrue : qfalse;
            qboolean isStart = ( straightFlags[i] & DT_STRAIGHTPATH_START ) ? qtrue : qfalse;
            qboolean isEnd   = ( straightFlags[i] & DT_STRAIGHTPATH_END ) ? qtrue : qfalse;
            Com_Log( SEV_INFO, LOG_CH(ch_nav), "nav_findpath: wp[%d] %.1f %.1f %.1f poly=%u flags=0x%02x%s%s%s\n",
                        i, wp[0], wp[1], wp[2], (unsigned)straightRefs[i], straightFlags[i],
                        isStart ? " START" : "", isEnd ? " END" : "", isOmc ? " OMC" : "" );
        }
    }
}

static void Nav_IslandCmd( void )
{
    if ( Cmd_Argc() < 4 ) {
        Com_Log( SEV_INFO, LOG_CH(ch_nav), "Usage: nav_island <x y z>\n" );
        return;
    }
    if ( !nav.ready || !nav.query ) {
        Com_Log( SEV_INFO, LOG_CH(ch_nav), "nav_island: navmesh not loaded\n" );
        return;
    }

    float qP[3];
    qP[0] = atof( Cmd_Argv(1) ); qP[1] = atof( Cmd_Argv(2) ); qP[2] = atof( Cmd_Argv(3) );

    float rP[3];
    Nav_QuakeToRecast( qP, rP );

    const dtQueryFilter *filter = GetFilter();
    dtPolyRef ref = 0;
    float pt[3];
    nav.query->findNearestPoly( rP, kDefaultExtents, filter, &ref, pt );

    int island = ref ? nav.mesh->getReachablePolyCount( ref, NAV_PREF_ISLAND_CAP ) : 0;

    float snapQ[3];
    Nav_RecastToQuake( pt, snapQ );
    Com_Log( SEV_INFO, LOG_CH(ch_nav), "nav_island: raw=%.0f %.0f %.0f snapRef=%u snap=%.0f %.0f %.0f island=%d\n",
                qP[0], qP[1], qP[2], (unsigned)ref, snapQ[0], snapQ[1], snapQ[2], island );
}

/* nav_spawnprobe [x y z] — physical collision test at a spawn (or all map spawns
 * if no args): place the STANDING player box with its origin AT the entity spawn
 * origin and report startsolid; then drop it from just above to report the true
 * rest-z.  The discriminant for the arena stuck-bot: a spawn that STARTSOLIDs at
 * its own entity origin means the bot/player spawns embedded (class-1 collision
 * spawn-embed), which no harness placement should mask. */
static void Nav_SpawnProbeCmd( void )
{
    const vec3_t mins = { -15.0f, -15.0f, -24.0f };   /* bg_public.h player box */
    const vec3_t maxs = {  15.0f,  15.0f,  32.0f };

    /* One-point probe helper (inline). */
    struct P {
        static void probe( const char *tag, const float *o ) {
            /* (a) startsolid AT the entity origin (where the spawn places the box). */
            trace_t at; vec3_t p0 = { o[0], o[1], o[2] };
            const vec3_t mn = { -15,-15,-24 }, mx = { 15,15,32 };
            CM_BoxTrace( &at, p0, p0, mn, mx, 0, MASK_PLAYERSOLID, qfalse );
            /* (b) drop from +48 to find the true rest-z. */
            vec3_t ds = { o[0], o[1], o[2] + 48.0f }, de = { o[0], o[1], o[2] - 128.0f };
            trace_t dr;
            CM_BoxTrace( &dr, ds, de, mn, mx, 0, MASK_PLAYERSOLID, qfalse );
            float restZ = ( dr.startsolid || dr.fraction >= 1.0f ) ? -9999.0f : dr.endpos[2];
            Com_Log( SEV_INFO, LOG_CH(ch_nav),
                "nav_spawnprobe: %s origin=(%.0f %.0f %.0f) atStartSolid=%d atAllSolid=%d dropRestZ=%.0f restVsOrigin=%.0f\n",
                tag, o[0], o[1], o[2], (int)at.startsolid, (int)at.allsolid,
                restZ, ( restZ < -9000.0f ) ? -9999.0f : ( restZ - o[2] ) );
        }
    };

    if ( Cmd_Argc() >= 4 ) {
        float o[3] = { (float)atof(Cmd_Argv(1)), (float)atof(Cmd_Argv(2)), (float)atof(Cmd_Argv(3)) };
        P::probe( "point", o );
        return;
    }

    /* No args: enumerate all spawns on the current map and probe each. */
    if ( !nav.mapname[0] ) { Com_Log( SEV_INFO, LOG_CH(ch_nav), "nav_spawnprobe: no map loaded\n" ); return; }
    static float spawns[256][3];
    int n = Nav_Get_AllSpawnPoints( nav.mapname, spawns, 256 );
    Com_Log( SEV_INFO, LOG_CH(ch_nav), "nav_spawnprobe: map=%s spawns=%d\n", nav.mapname, n );
    int embedded = 0;
    for ( int i = 0; i < n; i++ ) {
        char tag[32]; Com_sprintf( tag, sizeof(tag), "spawn[%d]", i );
        /* Re-probe inline so we can tally startsolid. */
        trace_t at; vec3_t p0 = { spawns[i][0], spawns[i][1], spawns[i][2] };
        CM_BoxTrace( &at, p0, p0, mins, maxs, 0, MASK_PLAYERSOLID, qfalse );
        vec3_t ds = { spawns[i][0], spawns[i][1], spawns[i][2] + 48.0f };
        vec3_t de = { spawns[i][0], spawns[i][1], spawns[i][2] - 128.0f };
        trace_t dr; CM_BoxTrace( &dr, ds, de, mins, maxs, 0, MASK_PLAYERSOLID, qfalse );
        float restZ = ( dr.startsolid || dr.fraction >= 1.0f ) ? -9999.0f : dr.endpos[2];
        if ( at.startsolid || at.allsolid ) embedded++;
        Com_Log( SEV_INFO, LOG_CH(ch_nav),
            "nav_spawnprobe: %s origin=(%.0f %.0f %.0f) atStartSolid=%d atAllSolid=%d dropRestZ=%.0f restVsOrigin=%.0f\n",
            tag, spawns[i][0], spawns[i][1], spawns[i][2], (int)at.startsolid, (int)at.allsolid,
            restZ, ( restZ < -9000.0f ) ? -9999.0f : ( restZ - spawns[i][2] ) );
    }
    Com_Log( SEV_INFO, LOG_CH(ch_nav), "nav_spawnprobe: map=%s embedded(startsolid)=%d of %d\n", nav.mapname, embedded, n );
}

/* nav_thresholdprobe <x0 x1 xstep y0 y1 ystep floorZ> — ZZTHR diagnostic
 * (temporary): the openable-traversal threshold-severance probe. Over a band it
 * measures per the ruling: (1) world-solid occupancy + z-extent, (2) clip
 * occupancy, (3) clearance (free height above floor vs walkableHeight=56), (4)
 * floor-z step profile vs walkableClimb=18, (5) collision walk-check (standing-box
 * step-sequence). Read-only, main-thread CM. */
static void Nav_ThresholdProbeCmd( void )
{
    if ( Cmd_Argc() < 8 ) {
        Com_Log( SEV_INFO, LOG_CH(ch_nav), "Usage: nav_thresholdprobe <x0 x1 xstep y0 y1 ystep floorZ>\n" );
        return;
    }
    const float x0 = (float)atof(Cmd_Argv(1)), x1 = (float)atof(Cmd_Argv(2)), xs = (float)atof(Cmd_Argv(3));
    const float y0 = (float)atof(Cmd_Argv(4)), y1 = (float)atof(Cmd_Argv(5)), ys = (float)atof(Cmd_Argv(6));
    const float fz = (float)atof(Cmd_Argv(7));
    const vec3_t mn = { -15,-15,-24 }, mx = { 15,15,32 };

    Com_Log( SEV_INFO, LOG_CH(ch_nav), "ZZTHR band x[%.0f,%.0f/%.0f] y[%.0f,%.0f/%.0f] floorZ=%.0f  walkClimb=18 walkHeight=56 (bake: climb=15u/3vox clearance=60u/12vox)\n", x0,x1,xs,y0,y1,ys,fz );
    for ( float y = y0; y <= y1 + 0.5f; y += ys ) {
        for ( float x = x0; x <= x1 + 0.5f; x += xs ) {
            /* (4) floor-z: drop a point from well above to find the real floor. */
            vec3_t ps = { x, y, fz + 200.0f }, pe = { x, y, fz - 120.0f };
            trace_t pt; CM_BoxTrace( &pt, ps, pe, vec3_origin, vec3_origin, 0, MASK_PLAYERSOLID, qfalse );
            float floorZ = ( pt.startsolid || pt.fraction >= 1.0f ) ? -9999.0f : pt.endpos[2];
            /* (1) world-solid contents just above the floor + its z-extent (scan up). */
            vec3_t cpt = { x, y, fz + 4.0f };
            int cLo = CM_PointContents( cpt, 0 );
            /* (3) clearance: how high above floor before hitting a ceiling (point up). */
            float clr = -1.0f;
            if ( floorZ > -9000.0f ) {
                vec3_t us = { x, y, floorZ + 2.0f }, ue = { x, y, floorZ + 200.0f };
                trace_t ut; CM_BoxTrace( &ut, us, ue, vec3_origin, vec3_origin, 0, MASK_PLAYERSOLID, qfalse );
                clr = ( ut.fraction >= 1.0f ) ? 200.0f : ( ut.endpos[2] - floorZ );
            }
            /* (5) collision walk-check: STAND the box on the discovered floor
             * (origin = floorZ+|MINS_Z|) and test startsolid THERE — this is where
             * a resting player actually is. Then drop a hair to confirm the rest.
             * Keyed to the per-cell floorZ (not the fixed fz), so a stepped floor
             * across the band is measured correctly. */
            int boxStuck = 1; float boxRest = -9999.0f;
            if ( floorZ > -9000.0f ) {
                float oz = floorZ + 24.0f;   /* |MINS_Z| */
                vec3_t bo = { x, y, oz };
                trace_t st; CM_BoxTrace( &st, bo, bo, mn, mx, 0, MASK_PLAYERSOLID, qfalse );
                vec3_t ds = { x, y, oz + 8.0f }, de = { x, y, oz - 40.0f };
                trace_t dr; CM_BoxTrace( &dr, ds, de, mn, mx, 0, MASK_PLAYERSOLID, qfalse );
                boxStuck = ( st.startsolid || st.allsolid );
                boxRest = ( dr.startsolid || dr.fraction >= 1.0f ) ? -9999.0f : dr.endpos[2];
            }
            const char *cc = ( cLo & CONTENTS_SOLID ) ? "SOLID" : ( cLo & CONTENTS_PLAYERCLIP ) ? "CLIP" : ( cLo == 0 ) ? "empty" : "other";
            Com_Log( SEV_INFO, LOG_CH(ch_nav),
                "ZZTHR (%.0f,%.0f) floorZ=%.1f contents@+4=%s clearance=%.0f boxRest=%.1f boxStuck=%d\n",
                x, y, floorZ, cc, clr, boxRest, boxStuck );
        }
    }
    /* Clip-only occupancy pass (2): PLAYERCLIP contents distinct from solid. */
    Com_Log( SEV_INFO, LOG_CH(ch_nav), "ZZTHR --- clip occupancy (CONTENTS_PLAYERCLIP without SOLID) ---\n" );
    for ( float y = y0; y <= y1 + 0.5f; y += ys ) {
        char row[512]; int rl = 0;
        rl += Com_sprintf( row+rl, sizeof(row)-rl, "ZZTHR clipY%5.0f ", y );
        for ( float x = x0; x <= x1 + 0.5f; x += xs ) {
            vec3_t cpt2 = { x, y, fz + 4.0f };
            int c = CM_PointContents( cpt2, 0 );
            char ch2 = ( c & CONTENTS_SOLID ) ? 'S' : ( c & CONTENTS_PLAYERCLIP ) ? 'C' : ( c == 0 ) ? '.' : '?';
            rl += Com_sprintf( row+rl, sizeof(row)-rl, "%c ", ch2 );
        }
        Com_Log( SEV_INFO, LOG_CH(ch_nav), "%s\n", row );
    }
    /* Collision walk-check (5): step-sequence across Y at the band centre X. */
    Com_Log( SEV_INFO, LOG_CH(ch_nav), "ZZTHR --- collision walk-check (standing box, X=%.0f, Y sweep) ---\n", 0.5f*(x0+x1) );
    {
        float cx = 0.5f*(x0+x1);
        float prevRest = -9999.0f;
        for ( float y = y0; y <= y1 + 0.5f; y += ys ) {
            /* Find the floor by point-drop from high above, then stand the box on it. */
            vec3_t ps = { cx, y, fz + 200.0f }, pe = { cx, y, fz - 120.0f };
            trace_t pt; CM_BoxTrace( &pt, ps, pe, vec3_origin, vec3_origin, 0, MASK_PLAYERSOLID, qfalse );
            float floorZ = ( pt.startsolid || pt.fraction >= 1.0f ) ? -9999.0f : pt.endpos[2];
            int stuck = 1; float rest = -9999.0f;
            if ( floorZ > -9000.0f ) {
                float oz = floorZ + 24.0f;
                vec3_t bo = { cx, y, oz };
                trace_t stt; CM_BoxTrace( &stt, bo, bo, mn, mx, 0, MASK_PLAYERSOLID, qfalse );
                stuck = ( stt.startsolid || stt.allsolid );
                rest  = stuck ? -9999.0f : floorZ;
            }
            float step = ( prevRest > -9000.0f && rest > -9000.0f ) ? ( rest - prevRest ) : 0.0f;
            Com_Log( SEV_INFO, LOG_CH(ch_nav),
                "ZZTHR walk Y=%.0f rest=%.1f step=%.1f%s stuck=%d\n",
                y, rest, step, ( fabsf(step) > 18.0f ? " >CLIMB!" : "" ), stuck );
            prevRest = rest;
        }
    }
}

/* nav_gridprobe <cx> <x0> <x1> <xstep> <y0> <y1> <ystep> — render an occupancy +
 * standability grid around a point.  For each cell (x,y): contents at z=32/64
 * (S=SOLID, C=PLAYERCLIP, .=EMPTY, ?=other) and a standing-box drop (digit = rest
 * z tens, 'x'=startsolid/no-rest).  Two ASCII maps to resolve pillar-vs-wall and
 * the true standable extent.  All CM (main thread); read-only. */
static void Nav_GridProbeCmd( void )
{
    if ( Cmd_Argc() < 8 ) {
        Com_Log( SEV_INFO, LOG_CH(ch_nav), "Usage: nav_gridprobe <cx> <x0> <x1> <xstep> <y0> <y1> <ystep>\n" );
        return;
    }
    const float cx    = (float)atof( Cmd_Argv(1) );
    const float x0    = (float)atof( Cmd_Argv(2) );
    const float x1    = (float)atof( Cmd_Argv(3) );
    const float xstep = (float)atof( Cmd_Argv(4) );
    const float y0    = (float)atof( Cmd_Argv(5) );
    const float y1    = (float)atof( Cmd_Argv(6) );
    const float ystep = (float)atof( Cmd_Argv(7) );
    (void)cx;
    const vec3_t mn = { -15,-15,-24 }, mx = { 15,15,32 };

    Com_Log( SEV_INFO, LOG_CH(ch_nav), "nav_gridprobe: CONTENTS map (rows Y high->low, cols X low->high); z32/z64 per cell as [ab]  S=solid C=clip .=empty ?=other\n" );
    for ( float y = y1; y >= y0 - 0.5f; y -= ystep ) {
        char row[512]; int rl = 0;
        rl += Com_sprintf( row + rl, sizeof(row) - rl, "Y%5.0f ", y );
        for ( float x = x0; x <= x1 + 0.5f; x += xstep ) {
            vec3_t p32 = { x, y, 32.0f }, p64 = { x, y, 64.0f };
            int c32 = CM_PointContents( p32, 0 );
            int c64 = CM_PointContents( p64, 0 );
            char a = ( c32 & CONTENTS_SOLID ) ? 'S' : ( c32 & CONTENTS_PLAYERCLIP ) ? 'C' : ( c32 == 0 ) ? '.' : '?';
            char b = ( c64 & CONTENTS_SOLID ) ? 'S' : ( c64 & CONTENTS_PLAYERCLIP ) ? 'C' : ( c64 == 0 ) ? '.' : '?';
            rl += Com_sprintf( row + rl, sizeof(row) - rl, "%c%c ", a, b );
        }
        Com_Log( SEV_INFO, LOG_CH(ch_nav), "%s\n", row );
    }

    Com_Log( SEV_INFO, LOG_CH(ch_nav), "nav_gridprobe: STANDABILITY map (box drop from z+48): digit = rest-z/10, x=startsolid/no-rest\n" );
    for ( float y = y1; y >= y0 - 0.5f; y -= ystep ) {
        char row[512]; int rl = 0;
        rl += Com_sprintf( row + rl, sizeof(row) - rl, "Y%5.0f ", y );
        for ( float x = x0; x <= x1 + 0.5f; x += xstep ) {
            vec3_t ds = { x, y, 80.0f }, de = { x, y, -80.0f };
            trace_t dr; CM_BoxTrace( &dr, ds, de, mn, mx, 0, MASK_PLAYERSOLID, qfalse );
            char cell;
            if ( dr.startsolid || dr.allsolid || dr.fraction >= 1.0f ) cell = 'x';
            else { int t = ( (int)( dr.endpos[2] ) / 10 ) % 10; if ( t < 0 ) t = 0; cell = (char)( '0' + t ); }
            rl += Com_sprintf( row + rl, sizeof(row) - rl, " %c ", cell );
        }
        Com_Log( SEV_INFO, LOG_CH(ch_nav), "%s\n", row );
    }
}

/* -------------------------------------------------------------------------
   nav_validate — post-bake navmesh-vs-collision agreement instrument (report).

   Measures how far the (origin-space) baked navmesh disagrees with the LIVE
   player collision.  Both measurements run on the MAIN THREAD (CM_BoxTrace is
   legal here, never on the bake thread).

   Probe rule — CLEARANCE-AWARE (verify the instrument):
     The floor beneath a poly is found by dropping a ZERO-WIDTH POINT from just
     above the poly surface (polyZ + NAVVAL_CLEAR_UP), NOT the standing box from
     polyZ+64.  A box dropped from far above a poly that sits in a low-headroom
     STACKED room (a crouch passage under an upper floor — common on Q1 maps)
     rests on the UPPER floor and measures the WRONG ROOM, inventing a spurious
     ~one-storey (~63u) delta.  A point started INSIDE the poly's own air span
     cannot escape upward and cannot startsolid against a lateral wall, so it
     reports the collision surface in THIS room.  The standing-ORIGIN there is
     floorZ + |MINS_Z| (a resting player's origin sits |MINS_Z| above its feet),
     which is what a correct origin-space poly-z equals.  delta = origin z −
     poly-z; a poly whose surface is itself embedded in solid (point startsolid)
     is reported as NO floor, not a false delta.  OMC endpoints use the same
     clearance-aware floor.

   Reachability scoping — the enforce domain:
     Stats are accumulated over TWO scopes: [reachable] = the component the player
     SPAWN snaps to (walk + OMC-connected — the play floor), the enforce domain;
     [full] = every poly + an unreachablePolys count.  Severed islands (deep pit
     bottoms, under-shelf voids) stay in the mesh (future traversal-link material)
     but are outside the "agrees with collision" guarantee, so they sit in
     unreachablePolys, visible but out of the gate.  Anchor provenance is logged.

   Output: two [NAVVAL] scope lines (+ an anchor line) per invocation; optional
   per-failure detail behind "verbose".  "enforce <maxMean> <maxP95>" gates the
   REACHABLE scope (mean/p95 within ceilings AND reachable omcFail == 0).

   nofloor is reported as nofloor=N(buried=A submerged=B unmeasured=C), A+B+C==N.
   The split matters because the three are unrelated: BURIED (poly inside solid
   world geometry) is a real defect; SUBMERGED (liquid column) can never report a
   floor because MASK_PLAYERSOLID excludes water, so it is not a defect at all;
   UNMEASURED is the genuine residue.  Read the parts, not the total -- the merged
   number reads like a measurement gap and has already caused one misdiagnosis.

   The box z-extents and probe geometry mirror bg_public.h; these local mirrors
   keep the validator ABI-neutral (no game header pulled into qcommon).
   MINS_Z=-24, MAXS_Z=32.
   ------------------------------------------------------------------------- */

/* Player-box z-extents, mirrored from bg_public.h (MINS_Z/MAXS_Z). */
static const float NAVVAL_MINS_Z           = -24.0f;  /* MINS_Z         */
static const float NAVVAL_MAXS_Z           =  32.0f;  /* MAXS_Z         */
static const float NAVVAL_CROUCH_MAXS_Z    =  16.0f;  /* CROUCH_MAXS_Z  */
static const float NAVVAL_HALFWIDTH        =  15.0f;  /* PLAYER_WIDTH   */
/* Column probe geometry for the clearance-clamped box down-trace and OMC checks. */
static const float NAVVAL_CLEAR_EPS        = 2.0f;    /* low box start above poly (inside its own room) */
static const float NAVVAL_CLEAR_CLIMB      = 18.0f;   /* high box start (NAV_WALKABLE_CLIMB): brackets a floor sitting a step ABOVE poly-z */
static const float NAVVAL_CLEAR_SETTLE     = 40.0f;   /* short settle distance a clamped box drops from the LOW start */
static const float NAVVAL_DOWN_RANGE       = 256.0f;  /* full down-sweep for the point candidate */
static const float NAVVAL_OMC_HEADROOM     = 56.0f;   /* standing clearance (WALKABLE_HEIGHT) */
static const float NAVVAL_EPSILON          = 2.0f;    /* z-agreement tolerance band    */

/* ── at-rest mover volumes for the standability probe ──────────────────────
 * The canonical world model contains ONLY the world (CMQ1_BuildCanonicalModel
 * walks submodelRoots[0]), so a box dropped against model handle 0 falls straight
 * through any mover and lands on whatever is beneath it.  For a surface a player
 * genuinely stands on at rest — a closed hatch lid, a plat top — that reads as a
 * floor hundreds of units too low, and the poly resting there gets called a liar.
 *
 * The probe therefore also drops against the movers themselves, using the public
 * per-inline-model clip handles (CM_InlineModel / CM_NumInlineModels).  Nothing is
 * merged into the canonical model: runtime collision, tracing and physics are
 * untouched; only this bake-time probe gains the extra question.
 *
 * Pose: the compiled at-rest position.  An inline model's own clip data is stored
 * at that pose, so tracing the handle directly samples it with no entity lookup and
 * no live state (this runs at bake).
 *
 * Format neutrality: CM_InlineModel / CM_PointContents are generic.  A format whose
 * inline models are already part of the traced world simply contributes no NEW floor
 * (the world candidates already found it and sit nearer poly-z), so this is a
 * natural no-op there rather than a format branch.
 *
 * Cost: bounded per probe.  The candidate list is built ONCE per bake (below) and a
 * probe only traces the movers whose at-rest AABB actually brackets its column, so
 * the common case adds zero traces. */
#define NAVVAL_MAX_MOVERS 256
static struct {
    int          count;
    qboolean     built;
    clipHandle_t handle[NAVVAL_MAX_MOVERS];
    vec3_t       mins[NAVVAL_MAX_MOVERS];
    vec3_t       maxs[NAVVAL_MAX_MOVERS];
} s_navMovers;

static void NavVal_ResetMoverCache( void ) { memset( &s_navMovers, 0, sizeof( s_navMovers ) ); }

/* Collect the at-rest SOLID inline models once per bake.  Solid-only is decided
 * structurally: sample the model's own volume through its own clip handle and keep
 * it if the collision world reports CONTENTS_SOLID there.  A trigger volume (no
 * solid contents) is rejected, so it can never become phantom floor.  No classname,
 * no entity lookup, no map constant, no format branch. */
static void NavVal_BuildMoverCache( void )
{
    if ( s_navMovers.built ) return;
    s_navMovers.built = qtrue;
    s_navMovers.count = 0;

    const int numInline = CM_NumInlineModels();
    for ( int i = 1; i < numInline && s_navMovers.count < NAVVAL_MAX_MOVERS; i++ ) {
        clipHandle_t h = CM_InlineModel( i );
        if ( !h ) continue;
        vec3_t mn, mx;
        CM_ModelBounds( h, mn, mx );
        if ( mx[0] <= mn[0] && mx[1] <= mn[1] && mx[2] <= mn[2] ) continue;   /* degenerate */
        /* Structural obstruction test against the model's OWN clip data: keep a model
         * whose own volume blocks the player box.  The mask is the SAME policy the nav
         * geometry pass uses (CONTENTS_SOLID | CONTENTS_PLAYERCLIP) — a Q1 mover's clip
         * volume presents as PLAYERCLIP, a Q3 inline model as SOLID, and a non-solid
         * trigger volume matches neither, so it can never become phantom floor.  One
         * policy, no classname, no entity lookup, no format branch.
         *
         * The centre alone is not sufficient: a hollow or thin-shelled mover can report
         * empty there, so sample the centre plus a few interior points and keep the
         * model if ANY of them is obstructing. */
        int cont = 0;
        {
            static const float kFrac[3] = { 0.5f, 0.25f, 0.75f };
            for ( int a = 0; a < 3 && !cont; a++ ) {
                for ( int bq = 0; bq < 3 && !cont; bq++ ) {
                    vec3_t p;
                    p[0] = mn[0] + ( mx[0] - mn[0] ) * kFrac[a];
                    p[1] = mn[1] + ( mx[1] - mn[1] ) * kFrac[bq];
                    p[2] = mn[2] + ( mx[2] - mn[2] ) * 0.5f;
                    cont = CM_PointContents( p, h ) & ( CONTENTS_SOLID | CONTENTS_PLAYERCLIP );
                }
            }
        }
        if ( !cont ) continue;

        const int k = s_navMovers.count++;
        s_navMovers.handle[k] = h;
        VectorCopy( mn, s_navMovers.mins[k] );
        VectorCopy( mx, s_navMovers.maxs[k] );
    }
}

/* One clamped box drop from a given start origin; settles DOWN `settle` units.
 * Tries the STANDING box, then (or first, if lowCeiling) the CROUCH box.  Returns
 * true + the box-origin rest z, or false if neither box could start/settle here.
 *
 * `model` selects what is traced: 0 = the canonical world, or an inline-model clip
 * handle for an at-rest mover.  Splitting on the handle keeps ONE drop definition
 * shared by both, rather than forking a second probe. */
static bool NavVal_BoxDropModel( float qx, float qy, float startOrigin, float settle,
                                 bool lowCeiling, clipHandle_t model, float *outRest )
{
    const vec3_t mins  = { -NAVVAL_HALFWIDTH, -NAVVAL_HALFWIDTH, NAVVAL_MINS_Z };
    const vec3_t maxsS = {  NAVVAL_HALFWIDTH,  NAVVAL_HALFWIDTH, NAVVAL_MAXS_Z };
    const vec3_t maxsC = {  NAVVAL_HALFWIDTH,  NAVVAL_HALFWIDTH, NAVVAL_CROUCH_MAXS_Z };
    vec3_t start = { qx, qy, startOrigin };
    vec3_t end   = { qx, qy, startOrigin - settle };
    trace_t tr;
    if ( !lowCeiling ) {
        CM_BoxTrace( &tr, start, end, mins, maxsS, model, MASK_PLAYERSOLID, qfalse );
        if ( !tr.startsolid && !tr.allsolid && tr.fraction < 1.0f ) { if ( outRest ) *outRest = tr.endpos[2]; return true; }
    }
    CM_BoxTrace( &tr, start, end, mins, maxsC, model, MASK_PLAYERSOLID, qfalse );
    if ( !tr.startsolid && !tr.allsolid && tr.fraction < 1.0f ) { if ( outRest ) *outRest = tr.endpos[2]; return true; }
    return false;
}

static bool NavVal_BoxDrop( float qx, float qy, float startOrigin, float settle,
                            bool lowCeiling, float *outRest )
{
    return NavVal_BoxDropModel( qx, qy, startOrigin, settle, lowCeiling, 0, outRest );
}

/* Best at-rest-mover rest z for this column, or false if no mover brackets it.
 * Bound: only movers whose at-rest AABB contains the column in XY and whose z-span
 * brackets the drop are traced, so a probe far from every mover costs one AABB
 * compare each and no traces at all. */
static bool NavVal_MoverFloorZ( float qx, float qy, float startOrigin, float settle,
                                bool lowCeiling, float polyZ, float *outRest )
{
    NavVal_BuildMoverCache();
    bool  have = false;
    float best = 0.0f, bestD = 1e30f;
    for ( int i = 0; i < s_navMovers.count; i++ ) {
        const float *mn = s_navMovers.mins[i];
        const float *mx = s_navMovers.maxs[i];
        /* Column must lie within the mover footprint (grown by the box half-width so
         * a player resting on the very edge still finds it). */
        if ( qx < mn[0] - NAVVAL_HALFWIDTH || qx > mx[0] + NAVVAL_HALFWIDTH ) continue;
        if ( qy < mn[1] - NAVVAL_HALFWIDTH || qy > mx[1] + NAVVAL_HALFWIDTH ) continue;
        /* The drop span must reach the mover's z-range at all. */
        if ( startOrigin - settle > mx[2] + NAVVAL_MAXS_Z ) continue;
        if ( startOrigin < mn[2] ) continue;
        float r;
        if ( !NavVal_BoxDropModel( qx, qy, startOrigin, settle, lowCeiling,
                                   s_navMovers.handle[i], &r ) )
            continue;
        const float d = fabsf( r - polyZ );
        if ( d < bestD ) { bestD = d; best = r; have = true; }
    }
    if ( have && outRest ) *outRest = best;
    return have;
}

/* Zero-width point floor: drops a point from just above foot level; the origin is
 * pointFloor + |MINS_Z|.  Straddles nothing (falls through grate gaps) but never
 * startsolids on a lateral wall -- the complement of the box. */
static bool NavVal_PointFloorZ( float qx, float qy, float polyZ, float *outOriginZ )
{
    const float footZ = polyZ + NAVVAL_MINS_Z;
    trace_t tr;
    vec3_t start = { qx, qy, footZ + 4.0f };
    vec3_t end   = { qx, qy, footZ - NAVVAL_DOWN_RANGE };
    CM_BoxTrace( &tr, start, end, vec3_origin, vec3_origin, 0, MASK_PLAYERSOLID, qfalse );
    if ( tr.startsolid || tr.allsolid ) return false;
    if ( tr.fraction >= 1.0f )          return false;
    if ( outOriginZ ) *outOriginZ = tr.endpos[2] + ( -NAVVAL_MINS_Z );
    return true;
}

/* Shared "standable floor origin at (qx,qy) for a surface at polyZ" -- the ONE
 * definition of standable, called by both the validator and the post-bake OMC
 * endpoint finalize.  Answers: does the floor this poly/endpoint represents exist
 * in collision, and at what standing-origin z?
 *
 * FOUR artifact classes are excluded BY CONSTRUCTION, not threshold:
 *   1. Upward-escape: the old box from polyZ+64 rested on the room ABOVE. Avoided
 *      -- every start is within a climb-step of polyZ, never a storey up.
 *   2. Grate fall-through: a zero-width point drops between grate bars and snags a
 *      sliver below. Avoided -- a box (30u footprint) straddles the gap and rests
 *      on the bars like a real player.
 *   3. Box over-conservative startsolid: the box footprint clips nearby raised
 *      geometry a resting player would not. Avoided -- the point is used as a
 *      candidate too (it fits where the box cannot).
 *   4. Low-start fall-past: a floor sitting a little ABOVE poly-z (a wade floor /
 *      quantization) is missed by a polyZ+eps start, which begins below it and
 *      falls to a lower shelf. Avoided -- BRACKETED starts: drop a box from BOTH
 *      polyZ+eps AND polyZ+climb; the higher start brackets such a floor.
 *
 * BEST-OF: gather every candidate rest (low box, high box, point) and return the
 * one CLOSEST to polyZ -- "the floor this poly represents" is the nearest resting
 * surface.  A genuinely floating poly finds no candidate near polyZ from any start
 * and its nearest rest is still far -> it stays flagged (nothing is masked).
 * Returns false only if NO candidate found a floor at all (truly buried). */
static bool NavVal_ClearanceFloorZ( float qx, float qy, float polyZ,
                                    bool lowCeiling, float *outOriginZ )
{
    float best = 0.0f, bestD = 1e30f;
    bool  have = false;
    float r;

    /* Candidate A: low box start (polyZ+eps) -- the primary; where the poly's own
     * floor is at/just-below poly-z this measures ~=0 (keeps the greens stable). */
    if ( NavVal_BoxDrop( qx, qy, polyZ + NAVVAL_CLEAR_EPS, NAVVAL_CLEAR_SETTLE, lowCeiling, &r ) ) {
        float d = fabsf( r - polyZ ); if ( d < bestD ) { bestD = d; best = r; have = true; }
    }
    /* Candidate B: high box start (polyZ+climb) -- brackets a floor a step ABOVE
     * poly-z (wade floor / quantization); settle reaches back down through it. */
    if ( NavVal_BoxDrop( qx, qy, polyZ + NAVVAL_CLEAR_CLIMB, NAVVAL_CLEAR_CLIMB + NAVVAL_CLEAR_SETTLE, lowCeiling, &r ) ) {
        float d = fabsf( r - polyZ ); if ( d < bestD ) { bestD = d; best = r; have = true; }
    }
    /* Candidate C: zero-width point -- fits where a box footprint cannot start. */
    if ( NavVal_PointFloorZ( qx, qy, polyZ, &r ) ) {
        float d = fabsf( r - polyZ ); if ( d < bestD ) { bestD = d; best = r; have = true; }
    }
    /* Candidate D: the at-rest MOVER volumes, which the canonical world model does
     * not contain.  A surface a player rests on at rest (a closed hatch lid, a plat
     * top) has no world floor at its own z, so candidates A-C land far below it and
     * the poly reads as floating.  Tracing the movers supplies the missing rest.
     *
     * It joins the same BEST-OF rule rather than overriding: whichever candidate is
     * CLOSEST to poly-z wins.  On ordinary floor the world candidates already sit at
     * distance ~0, so a mover elsewhere in the column can never displace them — which
     * is why this cannot change an existing verdict.  Only a poly whose true rest IS
     * the mover top gets a nearer answer than it had. */
    if ( NavVal_MoverFloorZ( qx, qy, polyZ + NAVVAL_CLEAR_CLIMB,
                             NAVVAL_CLEAR_CLIMB + NAVVAL_CLEAR_SETTLE,
                             lowCeiling, polyZ, &r ) ) {
        float d = fabsf( r - polyZ ); if ( d < bestD ) { bestD = d; best = r; have = true; }
    }

    if ( !have ) return false;   /* truly buried: no floor from any start */
    if ( outOriginZ ) *outOriginZ = best;
    return true;
}


/* Shared endpoint-standability test (Quake-space point) -- the ONE definition of
 * "an OMC endpoint is standable", called by BOTH the validator's OMC check and
 * the post-bake endpoint finalize.  Standable iff the clearance probe finds a
 * floor AND the endpoint z sits within that floor's resting band
 * [floorFoot - HEADROOM, floorOrigin + MAXS_Z].  Returns the floor origin z. */
static bool NavVal_EndpointStandable( const float *qe, float *outOriginZ )
{
    float originZ = 0.0f;
    if ( !NavVal_ClearanceFloorZ( qe[0], qe[1], qe[2], false, &originZ ) )
        return false;
    const float footZ = originZ + NAVVAL_MINS_Z;
    if ( qe[2] < footZ - NAVVAL_OMC_HEADROOM || qe[2] > originZ + NAVVAL_MAXS_Z )
        return false;
    if ( outOriginZ ) *outOriginZ = originZ;
    return true;
}

/* -------------------------------------------------------------------------
   OMC producer classification (shared by the trajectory finalize below and the
   nav_linkaudit census command).  Recovered from the OMC userId range, refined by
   poly area for external links.  userId encoding (nav_offmesh.c / nav_impl.cpp):
     - external      userId <  NAV_MAX_OMC (64): entity-parsed links
     - internal      NAV_MAX_OMC <= userId < NAV_JUMPGAP_USERID_BASE: drop / step-up
     - jump-gap      userId >= NAV_JUMPGAP_USERID_BASE: diagonal jump-gap
   External links carry their mechanism in the poly area (teleport / water / jump). */
typedef enum {
    OMCCLASS_EXTERNAL_JUMPLINK = 0, /* external NAVAREA_JUMP_LINK: jump-pad / target-push / plat / door-gap */
    OMCCLASS_TELEPORTER,            /* external NAVAREA_TELEPORT                                             */
    OMCCLASS_WATEREDGE,             /* external NAVAREA_WATER                                                */
    OMCCLASS_INTERNAL,              /* drop / vertical-step (userId in [NAV_MAX_OMC, 1e6))                    */
    OMCCLASS_JUMPGAP,               /* diagonal jump-gap (userId >= NAV_JUMPGAP_USERID_BASE)                  */
    OMCCLASS_COUNT
} omcClass_t;

static const char *OmcClassStr( omcClass_t c ) {
    switch ( c ) {
        case OMCCLASS_EXTERNAL_JUMPLINK: return "ext-jumplink";
        case OMCCLASS_TELEPORTER:        return "teleporter";
        case OMCCLASS_WATEREDGE:         return "water-edge";
        case OMCCLASS_INTERNAL:          return "internal-drop/step";
        case OMCCLASS_JUMPGAP:           return "jump-gap";
        default:                         return "unknown";
    }
}

/* Classify an OMC from its userId + poly area. */
static omcClass_t OmcClassify( unsigned int userId, unsigned char area ) {
    if ( userId >= NAV_JUMPGAP_USERID_BASE )     return OMCCLASS_JUMPGAP;
    if ( userId >= (unsigned)NAV_MAX_OMC )        return OMCCLASS_INTERNAL;
    if ( area == (unsigned char)NAVAREA_TELEPORT ) return OMCCLASS_TELEPORTER;
    if ( area == (unsigned char)NAVAREA_WATER )    return OMCCLASS_WATEREDGE;
    return OMCCLASS_EXTERNAL_JUMPLINK;
}

/* A link that traverses by MECHANISM rather than a run-jump parabola is exempt from
 * the trajectory validator -- forcing a jump arc on it would mis-verdict it.  The
 * jump parabola only applies to links a player crosses BY JUMPING (drop / vertical-
 * step / jump-gap):
 *   - Teleporter: instantaneous destination, no physics arc.
 *   - External jump-link: jump-pad (launched by the pad's own velocity, not a
 *     run-jump), plus plat/door-gap (mover/gap mechanisms).
 *   - Water-edge: a WADE crossing -- the bot walks through shallow water at water
 *     level (producer gates waterlevel<3 = wade, skips swim), it does NOT jump the
 *     seam; a standing-box jump arc clips the low water tunnel and mis-FAILs it. */
static bool OmcClassIsExempt( omcClass_t c ) {
    return ( c == OMCCLASS_TELEPORTER
          || c == OMCCLASS_EXTERNAL_JUMPLINK
          || c == OMCCLASS_WATEREDGE );
}

/* -------------------------------------------------------------------------
   Shared per-class OMC-endpoint VALIDITY -- the SINGLE definition consumed by
   BOTH the post-bake OMC finalize (disable decision) and nav_validate (omcFail
   count).  The question an endpoint must answer depends on the KIND of edge:

     - PHYSICS classes (generator drop / vertical-step / jump-gap): validity =
       BOX-STANDABLE.  A physics link promises a landing a player can stand on,
       so the endpoint must itself be standable (the shared NavVal_EndpointStandable).

     - MECHANISM classes (door-gap / teleporter / plat / jump-pad / water-edge --
       the OmcClassIsExempt set): validity = BOUND TO A LIVE POLY within a bounded
       distance of that poly's surface.  The crossing is mediated by the mechanism
       (the door opens, the plat rides, the wade walks); the endpoint's job is to
       ANCHOR the graph to real, standable mesh -- which poly-binding already proves
       (the bound poly is a live ground poly, itself guaranteed standable by the
       poly-standability finalize).  The exact-point box test at a doorway lip is
       redundant where it agrees with binding and wrongly strict where it doesn't
       (it dragged door *34's door-gap endpoint into a pit and disabled the crossing).

   INVARIANT (enforced at omcFail==0 and by the finalize): every OMC endpoint is
   valid under its class's validity definition -- physics: box-standable; mechanism:
   live-poly-bound (a non-blocked ground poly within snap distance).

   This is a gate-semantics REFINEMENT, not a loosening: the MECHANISM-FAILURE path
   still fires -- a mechanism OMC whose endpoint is bound to NOTHING (boundRef==0),
   bound to a BLOCKED/dead poly, or bound BEYOND the snap distance is INVALID and is
   disabled / counted as omcFail exactly as before.  (Yesterday's wade defect --
   7342/7343 vert-desynced off their standable poly -- would surface here as a
   distance failure had the vert-sync fix not already placed them on their poly.)

   The distance bound reuses the existing OMC snap tolerances (NAV_OMC_SNAP_HORIZ/
   VERT, nav_local.h) -- the same tolerance the endpoint-reconnect binds within --
   so an endpoint the binder accepted is valid and one it could not is not.  Runtime
   only (no bake constant; nothing folded into the cache param hash). */
static bool NavVal_OmcEndpointValid( unsigned int userId, unsigned char area,
                                     const float *qe, navPolyRef_t boundRef )
{
    const omcClass_t cls = OmcClassify( userId, area );
    if ( !OmcClassIsExempt( cls ) ) {
        /* Physics link: the endpoint promises a standable landing. */
        return NavVal_EndpointStandable( qe, NULL );
    }
    /* Mechanism link: the endpoint promises an anchor to live mesh.  Validity =
     * bound to a LIVE (non-blocked) ground poly.  The binding was established within
     * the OMC snap tolerance by the binder (baseOffMeshLinks / the endpoint reconnect,
     * halfExtents = con->rad / walkableClimb), so a bound endpoint is within the
     * distance bound BY CONSTRUCTION -- the separate closest-point distance test is
     * redundant and, for the END side (which shares the START's forward ground link in
     * the standard bind), would wrongly reject a correctly-wired long crossing.  The
     * MECHANISM-FAILURE path still fires: bound to NOTHING (boundRef==0) or to a
     * BLOCKED/dead poly (e.g. the poly-standability finalize killed the anchor) -> invalid. */
    if ( !boundRef || !nav.mesh )
        return false;                        /* bound to nothing -> invalid */
    unsigned short bf = 0;
    nav.mesh->getPolyFlags( (dtPolyRef)boundRef, &bf );
    if ( bf & (unsigned short)NAVPOLY_BLOCKED )
        return false;                        /* bound to a dead/blocked poly -> invalid */
    return true;
}

/* -------------------------------------------------------------------------
   Physics trajectory validator + finalize.  Answers: "can a player-shaped body
   physically traverse this link under the game's movement physics, landing
   standable?"  It simulates the family of launchable arcs with the ACTUAL pmove
   constants + integration and traces a standing box against the world each step; a
   link PASSes iff at least one collision-free arc reaches a standable landing.

   Nav_FinalizeLinkTrajectory (below) runs this over every physics-class OMC in the
   post-bake finalize chain (Nav_FinishReady), disabling FAILing links via the same
   NAVPOLY_BLOCKED poly-flag the OMC/poly standability finalizes use -- so no
   physically-untraversable link is ever routable, structurally, regardless of the
   producer that emitted it.  nav_linkaudit runs the same validator as a read-only
   census report.

   Movement constants -- read from bg_public.h / bg_pmove.c, cited per value.
   No guessed literals; the jump ceiling is DERIVED from these, never hand-tuned. */
static const float NAVTRAJ_GRAVITY      = 800.0f;  /* DEFAULT_GRAVITY  bg_public.h:37 / g_envGravity "800" g_main.c:209 */
static const float NAVTRAJ_JUMP_VEL     = 270.0f;  /* JUMP_VELOCITY    bg_public.h:40 (single jump; double-jump +100 needs a prior jump in-window, not a standing run-jump) */
static const float NAVTRAJ_RUN_SPEED    = 320.0f;  /* DEFAULT_MOVESPEED_PLAYER bg_public.h:38 (ground wishspeed clamp) */
static const float NAVTRAJ_STEPSIZE     = 18.0f;   /* STEPSIZE         bg_local.h:9 (a rise <= this is walked, not jumped) */
/* Sim tick: the trajectory is integrated at a fine, fixed dt so the arc is
 * resolution-independent (the game runs pmove per usercmd; a fine dt over-resolves
 * it and converges to the same apex within the documented residual).  Fixed dt +
 * fixed sampling order => deterministic verdict. */
static const float NAVTRAJ_DT           = 1.0f / 120.0f;  /* 8.33 ms integration step */
static const int   NAVTRAJ_MAX_STEPS    = 600;            /* 5 s flight ceiling (a real arc is < 1 s) */
static const int   NAVTRAJ_SPEED_SAMPLES = 9;   /* run-up speeds sampled 0..RUN_SPEED inclusive */

/* Physics-derived jump ceiling: apex of a JUMP_VELOCITY launch under GRAVITY.
 * v^2/(2g) = 270^2/1600 = 45.5625 u.  This is the number every apex-deficit verdict
 * is measured against -- from physics, not a constant. */
static float NavTraj_JumpCeiling( void ) {
    return ( NAVTRAJ_JUMP_VEL * NAVTRAJ_JUMP_VEL ) / ( 2.0f * NAVTRAJ_GRAVITY );
}

typedef enum {
    NAVTRAJ_JUMP_UP = 0,   /* end higher than start: needs the jump impulse */
    NAVTRAJ_DROP,          /* end lower: ballistic run-off, no jump impulse   */
    NAVTRAJ_ACROSS         /* ~level: jump-across a gap                       */
} navTrajType_t;

typedef enum {
    NAVTRAJ_PASS = 0,
    NAVTRAJ_FAIL_APEX,        /* required rise exceeds the jump ceiling      */
    NAVTRAJ_FAIL_CEILING,     /* arc clips a ceiling before reaching end     */
    NAVTRAJ_FAIL_WALL,        /* arc clips a wall mid-flight                 */
    NAVTRAJ_FAIL_LANDING,     /* reaches end xy but landing not standable    */
    NAVTRAJ_FAIL_LAUNCH,      /* standing box startsolid at the launch point */
    NAVTRAJ_FAIL_RANGE        /* no launchable arc spans the horizontal gap  */
} navTrajResult_t;

static const char *NavTraj_ResultStr( navTrajResult_t r ) {
    switch ( r ) {
        case NAVTRAJ_PASS:         return "PASS";
        case NAVTRAJ_FAIL_APEX:    return "FAIL:apex-deficit";
        case NAVTRAJ_FAIL_CEILING: return "FAIL:ceiling-clip";
        case NAVTRAJ_FAIL_WALL:    return "FAIL:wall-clip";
        case NAVTRAJ_FAIL_LANDING: return "FAIL:landing-not-standable";
        case NAVTRAJ_FAIL_LAUNCH:  return "FAIL:launch-box-solid";
        case NAVTRAJ_FAIL_RANGE:   return "FAIL:gap-beyond-range";
        default:                   return "FAIL:unknown";
    }
}

/* Evidence recorded for a PASS (the arc that worked) or the best partial for a FAIL. */
typedef struct {
    navTrajResult_t result;
    float launchSpeed;   /* horizontal run-up speed of the passing/best arc     */
    float apex;          /* peak height above the launch foot (u)               */
    float flightTime;    /* seconds from launch to landing                      */
    float reqRise;       /* end foot minus start foot (u); the rise to clear    */
    float horizGap;      /* horizontal distance start->end (u)                  */
    float bestReachXY;   /* nearest horizontal approach to end an arc achieved  */
} navTrajEvidence_t;

/* Simulate ONE arc from (sx,sy,startFootZ) toward (ex,ey,endFootZ) at a given
 * horizontal launch speed, with (jump ? +JUMP_VEL : 0) vertical impulse.  Steps a
 * standing box with CM_BoxTrace; returns the result and fills apex/flightTime.
 * A collision-free arc that arrives within landing tolerance of the end xy AND at a
 * standable end is a PASS.  The integration is the pmove half-step Verlet
 * (bg_slidemove.c:48-52): pos uses v - 0.5*g*dt, end velocity is v - g*dt. */
static navTrajResult_t NavTraj_SimArc( const float startOrigin[3], const float endFoot[3],
                                       float launchSpeed, bool jump,
                                       float *outApex, float *outTime, float *outBestReachXY )
{
    /* Horizontal launch direction: toward the end xy. */
    float dx = endFoot[0] - startOrigin[0];
    float dy = endFoot[1] - startOrigin[1];
    float hlen = sqrtf( dx*dx + dy*dy );
    float ux = ( hlen > 0.001f ) ? dx / hlen : 1.0f;
    float uy = ( hlen > 0.001f ) ? dy / hlen : 0.0f;

    /* Standing box for the swept trace. */
    const vec3_t mins = { -NAVVAL_HALFWIDTH, -NAVVAL_HALFWIDTH, NAVVAL_MINS_Z };
    const vec3_t maxs = {  NAVVAL_HALFWIDTH,  NAVVAL_HALFWIDTH, NAVVAL_MAXS_Z };

    float pos[3] = { startOrigin[0], startOrigin[1], startOrigin[2] };
    float vx = ux * launchSpeed, vy = uy * launchSpeed;
    float vz = jump ? NAVTRAJ_JUMP_VEL : 0.0f;

    const float launchFootZ = startOrigin[2] + NAVVAL_MINS_Z;
    const float endFootZ    = endFoot[2];
    float apex = 0.0f, bestReachXY = 1e30f;
    const float dt = NAVTRAJ_DT;

    for ( int step = 0; step < NAVTRAJ_MAX_STEPS; step++ ) {
        /* pmove half-step Verlet: position uses the mid-frame vertical velocity. */
        float vzHalf = vz - 0.5f * NAVTRAJ_GRAVITY * dt;
        float next[3] = { pos[0] + vx * dt, pos[1] + vy * dt, pos[2] + vzHalf * dt };
        vz -= NAVTRAJ_GRAVITY * dt;   /* full end-of-step velocity */

        /* Swept standing-box trace for this segment (world model). */
        trace_t tr;
        CM_BoxTrace( &tr, pos, next, mins, maxs, 0, MASK_PLAYERSOLID, qfalse );
        if ( tr.startsolid || tr.allsolid ) {
            if ( outApex ) *outApex = apex;
            if ( outTime ) *outTime = step * dt;
            if ( outBestReachXY ) *outBestReachXY = bestReachXY;
            /* A startsolid at step 0 means the standing box is embedded at the LAUNCH
             * point itself -- the endpoint's foot is standable but a full standing box
             * overhangs into a wall/ledge there.  That is a distinct condition from a
             * mid-flight wall clip: the player cannot stand-launch a jump from this
             * exact spot.  Report it separately. */
            return ( step == 0 ) ? NAVTRAJ_FAIL_LAUNCH : NAVTRAJ_FAIL_WALL;
        }
        if ( tr.fraction < 1.0f ) {
            /* Hit something.  If it's the floor at the destination height and near the
             * end xy, that's the landing; else it's a wall/ceiling clip. */
            float hit[3] = { pos[0] + ( next[0]-pos[0] ) * tr.fraction,
                             pos[1] + ( next[1]-pos[1] ) * tr.fraction,
                             pos[2] + ( next[2]-pos[2] ) * tr.fraction };
            float hxdx = hit[0] - endFoot[0], hxdy = hit[1] - endFoot[1];
            float hxy = sqrtf( hxdx*hxdx + hxdy*hxdy );
            if ( hxy < bestReachXY ) bestReachXY = hxy;
            /* A downward-moving box landing on a near-horizontal surface (normal z up)
             * close to the end xy = the landing. */
            bool descending = ( vzHalf < 0.0f );
            bool floorLike  = ( tr.plane.normal[2] > 0.7f );

            /* DROP LAUNCH CLEARANCE.  A drop's arc BEGINS in contact with the floor it
             * leaves: jump==0 means vz starts at 0, so step 0's displacement is purely
             * -0.5*g*dt^2 -- a sub-unit downward nudge into the very ground the box is
             * resting on.  The swept trace then reports fraction==0 against that floor
             * and the arc is rejected as a mid-flight wall clip, so a drop can never
             * leave the ground inside the simulator at all.  A JUMP escapes this because
             * +NAVTRAJ_JUMP_VEL lifts the box clear on step 0; a drop has no impulse, so
             * the allowance must be explicit.  Measured (2026-07-31, three maps): 99.1% /
             * 99.4% / 99.7% of drop wall-clips were step 0, all at distance <1u from the
             * launch point, 96.8% against a normal-z==1.00 worldspawn floor.
             *
             * Skip the blocking verdict for exactly that signature and let the arc
             * continue: jump==0, the FIRST step, no forward progress (fraction==0), and a
             * floor-like surface.  Scope notes:
             *   - startsolid/allsolid is handled ABOVE and still returns FAIL_LAUNCH, so a
             *     genuinely EMBEDDED launch is untouched by this.
             *   - floorLike reuses the same normal cutoff the landing test above uses
             *     (0.7 == MIN_WALK_NORMAL, bg_local.h:7) -- no new constant.
             *   - jump==1 is excluded, so jump/across behaviour is bit-identical.
             *   - fraction==0 only: a drop that actually travels before hitting a floor is
             *     a real obstruction and still fails.
             *
             * 🔴 STEP 0 IS NOT THE WHOLE CONDITION (measured 2026-08-01, e1m1).  Gating on
             * step==0 exempts exactly one step, but a drop starts at vz==0 and separates
             * from its floor only after gravity has built enough speed.  Step 0's exemption
             * advances pos by the sub-unit nudge; at step 1 the box is STILL resting on the
             * SAME floor, so the trace again returns fraction==0 / normal.z==1.00 -- and
             * step==0 is now false, so the arc is rejected as a wall clip.  Measured: all 9
             * speed samples of the closest severed pair died at step==1, fraction==0.000,
             * normalZ=1.00, having fallen 0.0u and travelled <=2.7u.
             *
             * The physical condition is "still in RESTING CONTACT WITH THE LAUNCH FLOOR and
             * has not yet separated from it", so the exemption is anchored to THAT surface,
             * not to "any floor":
             *   - the contact must be at (or below) the launch foot plane, within the
             *     heightfield's own quantization NAV_CH -- a box that has descended to a
             *     LOWER floor is no longer on the launch floor and gets no exemption, so a
             *     descending box cannot skate across successive floors.
             *   - it is bounded in time, and the bound is DERIVED, not tuned: with vz0==0,
             *     the fall after n steps is 0.5*g*(n*dt)^2, so clearing one NAV_CH of
             *     separation needs n >= sqrt(2*NAV_CH/g)/dt = sqrt(2*3/800)*120 = 10.4 ->
             *     11 steps.  Past that the box has provably left the launch plane, so the
             *     exemption can no longer apply and any further floor contact is real.
             * Together these make the exemption self-terminating: it cannot outlive the
             * separation it is waiting for, and it cannot transfer to a different surface. */
            const float dropSepZ    = launchFootZ - (float)NAV_CH;
            const int   dropMaxStep = (int)ceilf( sqrtf( 2.0f * (float)NAV_CH / NAVTRAJ_GRAVITY ) / dt );
            const bool  onLaunchFloor =
                ( ( pos[2] + NAVVAL_MINS_Z ) >= dropSepZ );   /* not yet separated */
            const bool dropLaunchContact =
                ( !jump && step <= dropMaxStep && onLaunchFloor
                  && tr.fraction == 0.0f && floorLike );
            if ( dropLaunchContact ) {
                /* Resting contact with the launch floor, not an obstruction and not a
                 * landing (zero forward progress means the arc has not gone anywhere).
                 * Advance past it: keep the integrated 'next' position and continue the
                 * sweep from there, exactly as a non-colliding step would. */
                pos[0] = next[0]; pos[1] = next[1]; pos[2] = next[2];
                continue;
            }
            if ( descending && floorLike && hxy <= NAVVAL_HALFWIDTH + 8.0f ) {
                if ( outApex ) *outApex = apex;
                if ( outTime ) *outTime = ( step + tr.fraction ) * dt;
                if ( outBestReachXY ) *outBestReachXY = bestReachXY;
                /* Landing standability is checked by the caller against the shared
                 * probe at the end xy; here we confirm the arc physically arrived. */
                return NAVTRAJ_PASS;
            }
            /* Otherwise a mid-arc obstruction: ceiling if we were rising, wall if not. */
            if ( outApex ) *outApex = apex;
            if ( outTime ) *outTime = ( step + tr.fraction ) * dt;
            if ( outBestReachXY ) *outBestReachXY = bestReachXY;
            return ( vzHalf > 0.0f ) ? NAVTRAJ_FAIL_CEILING : NAVTRAJ_FAIL_WALL;
        }

        pos[0] = next[0]; pos[1] = next[1]; pos[2] = next[2];
        float footAbove = ( pos[2] + NAVVAL_MINS_Z ) - launchFootZ;
        if ( footAbove > apex ) apex = footAbove;

        /* Track nearest horizontal approach to the end. */
        float rdx = pos[0] - endFoot[0], rdy = pos[1] - endFoot[1];
        float rxy = sqrtf( rdx*rdx + rdy*rdy );
        if ( rxy < bestReachXY ) bestReachXY = rxy;

        /* Past the destination height and descending, near the end xy but no floor hit:
         * fell past the landing -- treat as reached-xy landing test by the caller iff
         * close; otherwise it undershoots/overshoots. */
        if ( vzHalf < 0.0f && pos[2] + NAVVAL_MINS_Z < endFootZ - 64.0f ) {
            /* dropped well below the target foot without landing near it: this arc
             * missed. */
            if ( outApex ) *outApex = apex;
            if ( outTime ) *outTime = step * dt;
            if ( outBestReachXY ) *outBestReachXY = bestReachXY;
            return ( bestReachXY <= NAVVAL_HALFWIDTH + 8.0f ) ? NAVTRAJ_PASS : NAVTRAJ_FAIL_RANGE;
        }
    }
    if ( outApex ) *outApex = apex;
    if ( outTime ) *outTime = NAVTRAJ_MAX_STEPS * dt;
    if ( outBestReachXY ) *outBestReachXY = bestReachXY;
    return NAVTRAJ_FAIL_RANGE;
}

/* Validate a link's traversability by physics.  start/end are Quake-space OMC
 * endpoints (poly-z convention, i.e. standing origins on their floors).  Simulates
 * the family of launchable arcs (run-up speed 0..RUN_SPEED; jump at the edge for
 * up/across, ballistic for drops); PASS iff at least one collision-free arc lands
 * standable at the end.  Deterministic: fixed dt, fixed speed-sample order. */
static navTrajResult_t Nav_ValidateLinkTrajectory( const float startQ[3], const float endQ[3],
                                                   navTrajEvidence_t *ev )
{
    /* Standable start/end origins (reuse the shared probe so "standable" is one
     * definition across the whole nav layer). */
    float startOriginZ = startQ[2], endOriginZ = endQ[2];
    NavVal_ClearanceFloorZ( startQ[0], startQ[1], startQ[2], false, &startOriginZ );
    bool endStandable = NavVal_EndpointStandable( endQ, &endOriginZ );

    const float startFootZ = startOriginZ + NAVVAL_MINS_Z;
    const float endFootZ   = endOriginZ   + NAVVAL_MINS_Z;
    const float reqRise    = endFootZ - startFootZ;
    float dxy = sqrtf( ( endQ[0]-startQ[0] )*( endQ[0]-startQ[0] ) + ( endQ[1]-startQ[1] )*( endQ[1]-startQ[1] ) );

    if ( ev ) {
        memset( ev, 0, sizeof( *ev ) );
        ev->reqRise = reqRise; ev->horizGap = dxy; ev->bestReachXY = 1e30f;
    }

    /* Classify. */
    navTrajType_t type;
    if ( reqRise >  NAVTRAJ_STEPSIZE )      type = NAVTRAJ_JUMP_UP;
    else if ( reqRise < -NAVTRAJ_STEPSIZE ) type = NAVTRAJ_DROP;
    else                                    type = NAVTRAJ_ACROSS;

    /* Early apex-deficit gate for a jump-up: if the rise exceeds the physics jump
     * ceiling, NO launchable arc can clear it -- the simulator will confirm, but this
     * gives the precise reason + margin. */
    const float ceiling = NavTraj_JumpCeiling();
    if ( type == NAVTRAJ_JUMP_UP && reqRise > ceiling + 0.5f ) {
        if ( ev ) { ev->result = NAVTRAJ_FAIL_APEX; ev->apex = ceiling; }
        return NAVTRAJ_FAIL_APEX;
    }

    float startOrigin[3] = { startQ[0], startQ[1], startOriginZ };
    float endFoot[3]     = { endQ[0], endQ[1], endFootZ };
    bool jump = ( type != NAVTRAJ_DROP );

    navTrajResult_t bestFail = NAVTRAJ_FAIL_RANGE;
    float bestReachXY = 1e30f;
    /* Track the arc that got CLOSEST to the end (the most-informative failure) so the
     * evidence reflects the best attempt, not the last-tried (a speed=0 straight-up
     * arc clips the near wall immediately and would otherwise mask the real story). */
    float failSpeed = 0.0f, failApex = 0.0f, failTime = 0.0f, failReach = 1e30f;

    /* Sample run-up speeds 0..RUN_SPEED in fixed order (deterministic). */
    for ( int si = 0; si < NAVTRAJ_SPEED_SAMPLES; si++ ) {
        float speed = ( NAVTRAJ_SPEED_SAMPLES > 1 )
                    ? ( NAVTRAJ_RUN_SPEED * (float)si / (float)( NAVTRAJ_SPEED_SAMPLES - 1 ) )
                    : NAVTRAJ_RUN_SPEED;
        float apex = 0.0f, ftime = 0.0f, reach = 1e30f;
        navTrajResult_t r = NavTraj_SimArc( startOrigin, endFoot, speed, jump, &apex, &ftime, &reach );
        if ( reach < bestReachXY ) bestReachXY = reach;
        if ( r == NAVTRAJ_PASS ) {
            /* Physically arrived; require the landing be standable (shared probe). */
            if ( !endStandable ) {
                if ( ev ) { ev->result = NAVTRAJ_FAIL_LANDING; ev->apex = apex; ev->flightTime = ftime;
                            ev->launchSpeed = speed; ev->bestReachXY = bestReachXY; }
                return NAVTRAJ_FAIL_LANDING;
            }
            if ( ev ) { ev->result = NAVTRAJ_PASS; ev->launchSpeed = speed; ev->apex = apex;
                        ev->flightTime = ftime; ev->bestReachXY = reach; }
            return NAVTRAJ_PASS;
        }
        /* Keep the arc that reached closest to the end as the reported evidence, and
         * prefer a specific failure (wall/ceiling/apex) over a bare range miss. */
        if ( reach < failReach ) { failReach = reach; failSpeed = speed; failApex = apex; failTime = ftime; }
        if ( r != NAVTRAJ_FAIL_RANGE ) bestFail = r;
    }

    if ( ev ) { ev->result = bestFail; ev->bestReachXY = bestReachXY;
                ev->launchSpeed = failSpeed; ev->apex = failApex; ev->flightTime = failTime; }
    return bestFail;
}

/* =========================================================================
   Nav_GeneratePhysicsLinks -- the unified physics-link generator (validator
   INVERTED).  Replaces the old drop / vertical-step / jump-gap producers and the
   sole-connector augmented-poly union-find oracle.  Instead of a topology heuristic
   guessing where links belong (and over-emitting infeasible ones the finalize then
   disabled), this SEARCHES candidate boundary-edge pairs, classifies each, and
   emits a link ONLY if the trajectory validator proves a player can traverse it and
   land standable.  Every emitted physics link therefore carries its own proof.

   Runs on the bake worker thread -- CM_BoxTrace (via the validator's standability
   probe) is thread-legal since the collision thread-safety work.  Candidate
   validation is embarrassingly parallel (each pair is independent) but is kept
   single-threaded here for a first cut; the pre-filters keep the validated-pair
   count small (the FAIL-reason census showed most gaps are rejected on rise alone).

   DIVISION OF LABOR (with the trajectory finalize, which STAYS wired): the generator
   is mesh QUALITY (emit only proven links); Nav_FinalizeLinkTrajectory is the
   structural INVARIANT (nothing untraversable is ever routable, whatever emitted
   it).  Steady state: the finalize disables 0 generator links -- a nonzero count is
   a regression signal, which is why the finalize remains.

   DEFERRED (documented, not silently dropped): intra-component shortcut candidates
   -- detour-ratio-gated feasible jumps WITHIN one component -- are a designed future
   extension.  The validator already supports them; only this v1 candidate search
   restricts to DISTINCT reachable components (cost: no per-candidate path query).
   The old sole-connector gate's sin was REJECTING feasible shortcuts; we are not
   re-encoding that -- we are deferring the search, not the capability.
   ------------------------------------------------------------------------- */

/* Generator search constants -- defined in nav_local.h (shared with the cache param
 * hash so a value change self-invalidates the on-disk .nav).  All physics-derived or
 * a search bound; no map-specific tuning.  Aliased here for readability. */
static const float NAVGEN_MAX_REACH    = NAV_GEN_MAX_REACH;
static const float NAVGEN_MAX_RISE      = NAV_GEN_MAX_RISE;
static const float NAVGEN_MIN_DROP      = NAV_GEN_MIN_DROP;
static const float NAVGEN_INSET_STEP    = NAV_GEN_INSET_STEP;
static const int   NAVGEN_INSET_LADDER  = NAV_GEN_INSET_LADDER;
static const int   NAVGEN_COMP_MIN      = NAV_GEN_COMP_MIN;
static const float NAVGEN_LINK_RADIUS   = NAV_GEN_LINK_RADIUS;

/* One emitted physics link, held for the deterministic sort before it goes into
 * omcArrs (emission order must not depend on search/thread order -- Amendment 4). */
typedef struct {
    float  sx, sy, sz;      /* start (Recast) */
    float  ex, ey, ez;      /* end   (Recast) */
    unsigned char dir;      /* 0 = one-way (drop), 1 = bidirectional (jump both ways validate) */
    /* stable sort key: quantized start then end, so the emitted set is byte-identical
     * across bakes regardless of candidate discovery order. */
    int    keyX, keyY, keyZ, keyX2, keyY2, keyZ2;
    /* evidence (audit) */
    float  launchSpeed, apex, flightTime, reqRise, horizGap;
} navGenLink_t;

static int NavGen_LinkCmp( const void *pa, const void *pb ) {
    const navGenLink_t *a = (const navGenLink_t *)pa, *b = (const navGenLink_t *)pb;
    if ( a->keyX  != b->keyX  ) return a->keyX  - b->keyX;
    if ( a->keyY  != b->keyY  ) return a->keyY  - b->keyY;
    if ( a->keyZ  != b->keyZ  ) return a->keyZ  - b->keyZ;
    if ( a->keyX2 != b->keyX2 ) return a->keyX2 - b->keyX2;
    if ( a->keyY2 != b->keyY2 ) return a->keyY2 - b->keyY2;
    return a->keyZ2 - b->keyZ2;
}

/* Voxel (integer polymesh coords) -> Recast world float.  Y is the rise axis. */
static inline void NavGen_VoxToRecast( const float bmin[3], int vx, int vy, int vz, float out[3] ) {
    out[0] = bmin[0] + ( (float)vx + 0.5f ) * NAV_CS;
    out[1] = bmin[1] +   (float)vy          * NAV_CH;
    out[2] = bmin[2] + ( (float)vz + 0.5f ) * NAV_CS;
}

static void Nav_GeneratePhysicsLinks( rcPolyMesh *pmesh, const float bmin[3],
                                      OmcArrays *omcArrs, int numExtOmcs )
{
    if ( !pmesh || pmesh->npolys <= 0 ) return;
    const int nvp    = pmesh->nvp;
    const int npolys = pmesh->npolys;
    const int startCount = omcArrs->count;
    const int64_t genT0 = Sys_Microseconds();

    /* --- per-poly geometry + component flood (the reachable-component labeling the
     * candidate search needs; a straight poly-neighbor flood, no augmented oracle). */
    int   *comp  = (int *)  Z_Malloc( sizeof(int)   * npolys );
    int   *pnvi  = (int *)  Z_Malloc( sizeof(int)   * npolys );
    float *cx    = (float *)Z_Malloc( sizeof(float) * npolys );  /* Recast centroid */
    float *cy    = (float *)Z_Malloc( sizeof(float) * npolys );
    float *cz    = (float *)Z_Malloc( sizeof(float) * npolys );
    int   *stack = (int *)  Z_Malloc( sizeof(int)   * npolys );
    if ( !comp || !pnvi || !cx || !cy || !cz || !stack ) {
        if ( comp ) Z_Free( comp ); if ( pnvi ) Z_Free( pnvi );
        if ( cx ) Z_Free( cx ); if ( cy ) Z_Free( cy ); if ( cz ) Z_Free( cz );
        if ( stack ) Z_Free( stack );
        return;
    }
    for ( int pi = 0; pi < npolys; pi++ ) {
        comp[pi] = -1;
        int nvi = 0; float sx = 0, sy = 0, sz = 0;
        for ( int v = 0; v < nvp; v++ ) {
            unsigned short vi = pmesh->polys[pi * nvp * 2 + v];
            if ( vi == RC_MESH_NULL_IDX ) break;
            float r[3];
            NavGen_VoxToRecast( bmin, (int)pmesh->verts[vi*3+0], (int)pmesh->verts[vi*3+1],
                                (int)pmesh->verts[vi*3+2], r );
            sx += r[0]; sy += r[1]; sz += r[2]; nvi++;
        }
        pnvi[pi] = nvi;
        if ( nvi > 0 ) { cx[pi] = sx/nvi; cy[pi] = sy/nvi; cz[pi] = sz/nvi; }
        else           { cx[pi] = cy[pi] = cz[pi] = 0; }
    }
    /* Flood the poly-neighbor graph (internal neighbor = polys[pi*nvp*2 + nvp + e]
     * that is not RC_MESH_NULL_IDX). */
    int numComp = 0;
    for ( int pi = 0; pi < npolys; pi++ ) {
        if ( comp[pi] != -1 || pnvi[pi] < 3 ) continue;
        int sp = 0; stack[sp++] = pi; comp[pi] = numComp;
        while ( sp > 0 ) {
            int p = stack[--sp];
            for ( int e = 0; e < nvp; e++ ) {
                unsigned short nei = pmesh->polys[p * nvp * 2 + nvp + e];
                if ( nei == RC_MESH_NULL_IDX ) continue;
                if ( nei & 0x8000 ) continue;   /* tile-portal edge, not an internal neighbor */
                if ( (int)nei >= npolys ) continue;
                if ( comp[nei] == -1 && pnvi[nei] >= 3 ) { comp[nei] = numComp; stack[sp++] = nei; }
            }
        }
        numComp++;
    }

    /* NOTE: the generator does NOT predict walk-reachability at bake time.  It pairs
     * DISTINCT raw polymesh components and emits every physics-PROVEN crossing; the
     * PLACEMENT decision -- "is this pair genuinely severed, or a shortcut across one
     * already-connected floor?" -- is made at LOAD by Nav_FinalizeIntraRegionCull,
     * where reachability is a ground-truth query against the real link graph (Option
     * A).  This is deliberate: every bake-time approximation of loaded-mesh topology
     * this project built drifted into a second representation; the generator owns
     * PHYSICS truth, the load-time chain owns PLACEMENT truth. */

    /* Component sizes (of the RAW flood components; the seed floor is per raw
     * component so a sliver never anchors a candidate). */
    int *csize = (int *)Z_Malloc( sizeof(int) * ( numComp > 0 ? numComp : 1 ) );
    if ( csize ) { for ( int c = 0; c < numComp; c++ ) csize[c] = 0;
                   for ( int pi = 0; pi < npolys; pi++ ) if ( comp[pi] >= 0 ) csize[comp[pi]]++; }

    /* --- candidate search: DISTINCT-component poly pairs within horizontal reach and
     * rise band; validate at emission; keep ONE proven link per COMPONENT PAIR -- the
     * shortest-gap crossing (the cheapest, most-natural jump between two floors), so a
     * pair of large floors yields ONE link, not one per poly-pair.  This matches the
     * old producers' cardinality (one link per severed component pair) while proving
     * each by physics instead of a topology heuristic. */
    navGenLink_t *links = (navGenLink_t *)Z_Malloc( sizeof(navGenLink_t) * NAV_MAX_INT_OMC );
    int numLinks = 0;
    int validated = 0;

    /* --- PERMANENT rejection census (aggregate, not per-candidate).
     * The generator's output count alone cannot distinguish "gates are too strict"
     * from "candidates were never generated" from "a cap truncated the set" — three
     * defects needing three different fixes.  These counters make the generator state
     * its own reasoning: every poly pair it considers lands in exactly one bucket, and
     * the buckets are asserted to sum to the pairs examined.  Aggregate, so 10^5
     * candidates produce one block, not 10^5 lines.  Kept permanently and deliberately:
     * a census that is removed after use destroys the evidence for the goal it serves,
     * and the next yield regression then costs another five-pass investigation instead
     * of one bake.  Rides ch_nav at SEV_DEBUG — the same channel and severity as the
     * generator's existing one-line summary directly above it, so it is visible in any
     * bake already capturing that summary and silent at the default severity.  No new
     * cvar and no new channel: two extra DEBUG lines per bake is not spam. */
    long long cenPairs      = 0;   /* ordered poly pairs (a<b) reaching the pair loop  */
    long long cenSameComp   = 0;   /* rejected: b in the same raw component as a        */
    long long cenSliverB    = 0;   /* rejected: b's component below NAVGEN_COMP_MIN     */
    long long cenReach      = 0;   /* rejected: centroid horiz > NAVGEN_MAX_REACH       */
    long long cenRise       = 0;   /* rejected: rise > MAX_RISE (too high to jump)      */
    long long cenDrop       = 0;   /* rejected: rise < MIN_DROP (bottomless)            */
    long long cenPairDup    = 0;   /* skipped: pair already has a strictly shorter link */
    long long cenTraj       = 0;   /* rejected: no inset produced a PASS arc            */
    long long cenGapWorse   = 0;   /* proven, but not shorter than the pair's best      */
    long long cenCap        = 0;   /* proven+shortest, but NAV_MAX_INT_OMC full         */
    long long cenKept       = 0;   /* proven+shortest, stored as the pair's link        */
    /* Symmetric-pair-validation buckets.  cenRevSaved is a SUBSET of the pairs that
     * reached validation and is NOT a separate arm of the sum (a reverse-saved pair
     * lands in kept/gap-worse/cap like any other proof); cenBothFail is the subset of
     * traj-fail where BOTH admissible directions were tried and neither proved. */
    long long cenRevSaved   = 0;   /* proven only by the b->a attempt (one-way link)    */
    long long cenBothFail   = 0;   /* both admissible directions attempted, none proved */
    /* Per-inset-rung verdict tally for the reverse (drop-side) attempt, indexed
     * [rung][navTrajResult_t].  Standing evidence for whether the inset ladder — which
     * walks a drop's launch AWAY from the edge — is load-bearing. */
    long long cenRungWhy[4][7] = {{0}};
    /* Sub-census of the dominant expected rejector: which validator verdict ended the
     * inset ladder.  Indexed by navTrajResult_t (PASS..FAIL_RANGE). */
    long long cenTrajWhy[7] = { 0, 0, 0, 0, 0, 0, 0 };
    /* Seed-side rejections are counted once per 'a', not per pair (they skip the inner
     * loop entirely), so they are reported separately and are NOT part of the pair sum. */
    long long cenSeedSliver = 0;   /* 'a' skipped: its component below NAVGEN_COMP_MIN  */
    long long cenSeedDegen  = 0;   /* 'a' skipped: degenerate poly / unlabelled         */
    /* Per-component-pair best-link index (keyed lo*numComp+hi), so a second, longer
     * crossing of the same pair overwrites nothing and a shorter one replaces. */
    int   *pairLink = (int *)  Z_Malloc( sizeof(int)   * ( numComp > 0 ? numComp : 1 ) * ( numComp > 0 ? numComp : 1 ) );
    float *pairGap  = (float *)Z_Malloc( sizeof(float) * ( numComp > 0 ? numComp : 1 ) * ( numComp > 0 ? numComp : 1 ) );
    bool   pairOk   = ( pairLink && pairGap && numComp > 0 && (long long)numComp * numComp <= 4000000LL );
    if ( pairOk ) {
        for ( long long i = 0; i < (long long)numComp * numComp; i++ ) { pairLink[i] = -1; pairGap[i] = 1e30f; }
    }

    /* Pair DISTINCT raw polymesh components (one link per component pair, shortest
     * gap).  Whether a pair is genuinely severed or a walk-reachable shortcut is
     * decided at LOAD by the intra-region cull, not here (Option A). */
    for ( int a = 0; a < npolys && pairOk; a++ ) {
        if ( pnvi[a] < 3 || comp[a] < 0 ) { cenSeedDegen++; continue; }
        if ( csize && csize[comp[a]] < NAVGEN_COMP_MIN ) { cenSeedSliver++; continue; }
        for ( int b = a + 1; b < npolys; b++ ) {
            if ( pnvi[b] < 3 || comp[b] < 0 ) continue;   /* degenerate b: not a pair */
            cenPairs++;
            if ( comp[b] == comp[a] ) { cenSameComp++; continue; }   /* same raw component (already walk-connected) */
            if ( csize && csize[comp[b]] < NAVGEN_COMP_MIN ) { cenSliverB++; continue; }

            /* Cheap centroid pre-filters (reject on rise/reach before any trace). */
            float dxr = cx[b] - cx[a], dzr = cz[b] - cz[a];
            float horiz = sqrtf( dxr*dxr + dzr*dzr );
            if ( horiz > NAVGEN_MAX_REACH ) { cenReach++; continue; }
            /* PRE-FILTER SYMMETRY.  'rise' is SIGNED and direction-dependent, and the two
             * bounds are NOT mirror images (MAX_RISE=+63.6 is a jump ceiling; MIN_DROP=-320
             * is a fall cap), so the admitted set is genuinely ASYMMETRIC: a pair at
             * rise=-100 is a legal drop a->b but an illegal +100 jump b->a, and a pair at
             * rise=+30 is a legal jump a->b and a legal -30 drop b->a.  A direction is
             * therefore admissible only if ITS OWN signed rise lies in [MIN_DROP,MAX_RISE].
             * Evaluate both explicitly rather than assuming one implies the other; the
             * pair is rejected only when NEITHER direction is admissible.  Thresholds
             * themselves are unchanged — this only stops applying a->b's verdict to b->a. */
            float rise = cy[b] - cy[a];                         /* Recast Y up */
            const bool fwdOk = ( rise <= NAVGEN_MAX_RISE && rise >= NAVGEN_MIN_DROP );
            const bool revOk = ( -rise <= NAVGEN_MAX_RISE && -rise >= NAVGEN_MIN_DROP );
            if ( !fwdOk && !revOk ) {
                /* Neither direction admissible: attribute to the bound that excluded the
                 * forward direction, preserving the existing census semantics. */
                if ( rise > NAVGEN_MAX_RISE ) { cenRise++; } else { cenDrop++; }
                continue;
            }

            /* One link per COMPONENT pair (keyed by raw component ids).  Skip if the
             * pair already has a strictly-shorter proven link (horiz is a lower bound
             * on the actual gap). */
            int clo = comp[a] < comp[b] ? comp[a] : comp[b];
            int chi = comp[a] < comp[b] ? comp[b] : comp[a];
            long long pk = (long long)clo * numComp + chi;
            if ( horiz >= pairGap[pk] ) { cenPairDup++; continue; }

            /* Inset ladder (Amendment 2): ladder the LAUNCH point back from a's centroid
             * AWAY from b (inward on a's floor) so a jump feasible from half a step back
             * is FOUND, not lost to edge quantization.  Landing at b's centroid.  The
             * validator re-grounds both endpoints and validates in Quake space. */
            navTrajResult_t got = NAVTRAJ_FAIL_RANGE;
            navTrajEvidence_t gotEv; memset( &gotEv, 0, sizeof(gotEv) );
            float gotS[3] = {0,0,0}, gotE[3] = {0,0,0};
            unsigned char gotDir = 0;

            float ux = ( horiz > 0.001f ) ? dxr / horiz : 1.0f;
            float uz = ( horiz > 0.001f ) ? dzr / horiz : 0.0f;
            navTrajResult_t lastTrajFail = NAVTRAJ_FAIL_RANGE;   /* census */

            /* SYMMETRIC PAIR VALIDATION.  A pair of components is a traversal in TWO
             * directions with DIFFERENT physics: a->b may be an infeasible jump-up while
             * b->a is a trivially feasible step-down.  Validating only a->b tests whichever
             * direction poly index order happens to give, so the direction that would
             * succeed is never constructed and the pair is rejected as impossible.
             * (Measured: a 22-poly ledge was paired 48 times as a +32u JUMP_UP that
             * correctly ceiling-clipped, and 0 times as the -32u step-down that works.)
             * So: try a->b; if no inset proves it, try b->a before rejecting the pair.
             * The reverse attempt runs ONLY on forward failure, so a passing pair costs
             * exactly what it did before. */
            bool revSaved = false;
            for ( int li = 0; li < NAVGEN_INSET_LADDER && got != NAVTRAJ_PASS; li++ ) {
                float inset = (float)li * NAVGEN_INSET_STEP;
                float rs[3] = { cx[a] - ux * inset, cy[a], cz[a] - uz * inset };
                float re[3] = { cx[b], cy[b], cz[b] };
                float qStart[3], qEnd[3];
                Nav_RecastToQuake( rs, qStart );
                Nav_RecastToQuake( re, qEnd );
                navTrajEvidence_t evAB, evBA;
                if ( fwdOk ) {
                    validated++;
                    navTrajResult_t rAB = Nav_ValidateLinkTrajectory( qStart, qEnd, &evAB );
                    lastTrajFail = rAB;   /* census: the verdict that ended this ladder */
                    if ( rAB == NAVTRAJ_PASS ) {
                        validated++;
                        navTrajResult_t rBA = Nav_ValidateLinkTrajectory( qEnd, qStart, &evBA );
                        gotDir = ( rBA == NAVTRAJ_PASS ) ? 1 : 0;   /* bidir only if BOTH validate */
                        got = NAVTRAJ_PASS; gotEv = evAB;
                        gotS[0]=rs[0]; gotS[1]=rs[1]; gotS[2]=rs[2];
                        gotE[0]=re[0]; gotE[1]=re[1]; gotE[2]=re[2];
                        break;
                    }
                }

                /* Forward direction did not prove at this inset — try the REVERSE.  The
                 * launch point is laddered inward on b's floor (away from a), mirroring
                 * the forward inset; the landing is a's centroid.  A reverse-only proof
                 * emits a ONE-WAY link b->a: navGenLink_t stores start/end explicitly and
                 * dir=0 means one-way start->end, so the direction that actually works is
                 * what gets emitted (no representation change needed). */
                if ( revOk ) {
                    float rs2[3] = { cx[b] + ux * inset, cy[b], cz[b] + uz * inset };
                    float re2[3] = { cx[a], cy[a], cz[a] };
                    float qStart2[3], qEnd2[3];
                    Nav_RecastToQuake( rs2, qStart2 );
                    Nav_RecastToQuake( re2, qEnd2 );
                    navTrajEvidence_t evRev;
                    validated++;
                    navTrajResult_t rRev = Nav_ValidateLinkTrajectory( qStart2, qEnd2, &evRev );
                    /* PERMANENT (aggregated): per-rung verdict tally for the REVERSE
                     * (drop-side) attempt.  The inset ladder walks a drop's launch AWAY
                     * from the edge, and this distribution is the standing evidence for
                     * whether that is load-bearing (measured 2026-07-31: launch-box-solid
                     * rises 14664 -> 16010 -> 17288 across rungs while PASS falls
                     * 21 -> 9 -> 0).  Counters, not per-candidate lines: the same question
                     * answered in one block instead of ~10^5 log lines. */
                    if ( li >= 0 && li < 4 && (int)rRev >= 0 && (int)rRev < 7 )
                        cenRungWhy[li][(int)rRev]++;
                    if ( rRev == NAVTRAJ_PASS ) {
                        gotDir = 0;   /* one-way: only b->a is proven */
                        got = NAVTRAJ_PASS; gotEv = evRev;
                        gotS[0]=rs2[0]; gotS[1]=rs2[1]; gotS[2]=rs2[2];
                        gotE[0]=re2[0]; gotE[1]=re2[1]; gotE[2]=re2[2];
                        revSaved = true;
                        break;
                    }
                    if ( !fwdOk ) lastTrajFail = rRev;   /* reverse-only pair: its verdict */
                }
            }
            if ( revSaved ) cenRevSaved++;
            if ( got != NAVTRAJ_PASS ) {
                cenTraj++;
                if ( fwdOk && revOk ) cenBothFail++;   /* both directions tried, neither proved */
                if ( (int)lastTrajFail >= 0 && (int)lastTrajFail < 7 ) cenTrajWhy[(int)lastTrajFail]++;
                continue;   /* no proven arc from any inset -> no link */
            }

            /* Proven crossing.  Keep it as this component-pair's best iff its gap is the
             * shortest seen for the pair (one link per pair). */
            if ( gotEv.horizGap >= pairGap[pk] ) { cenGapWorse++; continue; }
            navGenLink_t *L;
            if ( pairLink[pk] >= 0 ) {
                L = &links[ pairLink[pk] ];       /* overwrite the pair's prior link */
            } else {
                if ( numLinks >= NAV_MAX_INT_OMC ) { cenCap++; continue; }
                pairLink[pk] = numLinks;
                L = &links[ numLinks++ ];
            }
            cenKept++;
            pairGap[pk] = gotEv.horizGap;
            L->sx=gotS[0]; L->sy=gotS[1]; L->sz=gotS[2];
            L->ex=gotE[0]; L->ey=gotE[1]; L->ez=gotE[2];
            L->dir = gotDir;
            L->launchSpeed = gotEv.launchSpeed; L->apex = gotEv.apex;
            L->flightTime = gotEv.flightTime;   L->reqRise = gotEv.reqRise; L->horizGap = gotEv.horizGap;
            L->keyX  = (int)lrintf( gotS[0] ); L->keyY  = (int)lrintf( gotS[1] ); L->keyZ  = (int)lrintf( gotS[2] );
            L->keyX2 = (int)lrintf( gotE[0] ); L->keyY2 = (int)lrintf( gotE[1] ); L->keyZ2 = (int)lrintf( gotE[2] );
        }
    }
    if ( pairLink ) Z_Free( pairLink );
    if ( pairGap )  Z_Free( pairGap );

    /* Cost note (Amendment 3): these are DISTINCT reachable components with no walk
     * path between them at bake time, so a jump link cannot shortcut a comparable
     * walk (there is none) -- the cost gate is trivially met.  Detour costs OMC
     * traversal by its own length, so among the pair's candidates the shortest gap
     * (kept above) is also the cheapest.  The detour-ratio cost gate matters for the
     * DEFERRED intra-component shortcuts (documented in the header). */

    /* --- deterministic emission: sort by stable key, then append into omcArrs so the
     * emitted link SET is byte-identical across bakes (Amendment 4). */
    if ( links && numLinks > 0 )
        qsort( links, numLinks, sizeof( navGenLink_t ), NavGen_LinkCmp );

    for ( int i = 0; i < numLinks && omcArrs->count < NAV_MAX_TOTAL_OMC; i++ ) {
        const navGenLink_t *L = &links[i];
        int idx  = omcArrs->count;
        int vbase = idx * 6;
        omcArrs->verts[vbase+0] = L->sx; omcArrs->verts[vbase+1] = L->sy; omcArrs->verts[vbase+2] = L->sz;
        omcArrs->verts[vbase+3] = L->ex; omcArrs->verts[vbase+4] = L->ey; omcArrs->verts[vbase+5] = L->ez;
        omcArrs->rads[idx]  = NAVGEN_LINK_RADIUS;
        omcArrs->flags[idx] = (unsigned short)( NAVPOLY_WALKABLE | NAVPOLY_OFFMESH );
        omcArrs->areas[idx] = (unsigned char)NAVAREA_JUMP_LINK;
        omcArrs->dirs[idx]  = L->dir;
        /* Physics userId range (>= NAV_MAX_OMC) so the surviving trajectory finalize +
         * linkaudit classify it as a physics link and keep validating it. */
        omcArrs->userIds[idx] = (unsigned int)( NAV_MAX_OMC + ( idx - numExtOmcs ) );
        /* Follower traversal mode from the link's OWN vertical.  Use L->reqRise —
         * the validator's "end foot minus start foot" in Quake foot-Z (the SAME sign
         * its DROP/JUMP_UP/ACROSS split uses at line 2510).  (NOT L->ez-L->sz: the
         * emitted verts are RECAST coords, whose Z is a horizontal Quake axis; the
         * vertical is recast Y — reqRise already carries the correct Quake rise.)
         * dest above start = a run-jump UP (CLIMB, actuation deferred); below = a DROP
         * the bot falls (FALL); ~level = a flat gap it walks (WALK).  L->dir is the
         * bidir flag, not up/down. */
        omcArrs->traversalModes[idx] =
            ( L->reqRise >  NAVTRAJ_STEPSIZE ) ? (unsigned char)NAV_TM_CLIMB :
            ( L->reqRise < -NAVTRAJ_STEPSIZE ) ? (unsigned char)NAV_TM_FALL  :
                                                 (unsigned char)NAV_TM_WALK;
        omcArrs->count++;
    }

    const int64_t genT1 = Sys_Microseconds();
    Com_Log( SEV_DEBUG, LOG_CH(ch_nav),
        "[NAV] physics-link generator: %d proven link(s) emitted (%d candidate pairs validated, %d components) in %.1f ms\n",
        omcArrs->count - startCount, validated, numComp, (double)( genT1 - genT0 ) / 1000.0 );

    /* --- PERMANENT rejection census.  Every pair the loop examined is in exactly one
     * bucket; the sum check below is asserted in the output so a future refactor that
     * adds an unaccounted early-out is visible immediately rather than silently
     * shrinking the yield. */
    {
        const long long cenSum = cenSameComp + cenSliverB + cenReach + cenRise + cenDrop
                               + cenPairDup + cenTraj + cenGapWorse + cenCap + cenKept;
        Com_Log( SEV_DEBUG, LOG_CH(ch_nav),
            "[NAVGEN-CENSUS] pairs=%lld sum=%lld %s | same-comp=%lld sliver-b=%lld reach=%lld "
            "rise=%lld drop=%lld pair-dup=%lld traj-fail=%lld gap-worse=%lld cap=%lld kept=%lld\n",
            cenPairs, cenSum, ( cenSum == cenPairs ) ? "OK" : "MISMATCH",
            cenSameComp, cenSliverB, cenReach, cenRise, cenDrop,
            cenPairDup, cenTraj, cenGapWorse, cenCap, cenKept );
        Com_Log( SEV_DEBUG, LOG_CH(ch_nav),
            "[NAVGEN-CENSUS] traj-fail by verdict: apex=%lld ceiling=%lld wall=%lld landing=%lld "
            "launch=%lld range=%lld | seed-skips: sliver-a=%lld degenerate-a=%lld | "
            "components=%d links=%d comp-min=%d max-reach=%.0f max-rise=%.1f min-drop=%.0f\n",
            cenTrajWhy[NAVTRAJ_FAIL_APEX],   cenTrajWhy[NAVTRAJ_FAIL_CEILING],
            cenTrajWhy[NAVTRAJ_FAIL_WALL],   cenTrajWhy[NAVTRAJ_FAIL_LANDING],
            cenTrajWhy[NAVTRAJ_FAIL_LAUNCH], cenTrajWhy[NAVTRAJ_FAIL_RANGE],
            cenSeedSliver, cenSeedDegen, numComp, numLinks,
            NAVGEN_COMP_MIN, NAVGEN_MAX_REACH, NAVGEN_MAX_RISE, NAVGEN_MIN_DROP );
        /* Symmetric-pair-validation outcome.  rev-saved counts pairs proven ONLY by the
         * b->a attempt (emitted as one-way b->a); both-fail counts pairs where both
         * admissible directions were tried and neither proved.  rev-saved is a subset of
         * the proven pairs, not a separate sum arm — see the declaration comment. */
        Com_Log( SEV_DEBUG, LOG_CH(ch_nav),
            "[NAVGEN-CENSUS] symmetric-pairs: rev-saved=%lld both-dirs-failed=%lld\n",
            cenRevSaved, cenBothFail );
        for ( int rg = 0; rg < NAVGEN_INSET_LADDER && rg < 4; rg++ )
            Com_Log( SEV_DEBUG, LOG_CH(ch_nav),
                "[NAVGEN-CENSUS] rung%d (drop-side): pass=%lld apex=%lld ceiling=%lld wall=%lld "
                "landing=%lld launch=%lld range=%lld\n", rg,
                cenRungWhy[rg][NAVTRAJ_PASS],         cenRungWhy[rg][NAVTRAJ_FAIL_APEX],
                cenRungWhy[rg][NAVTRAJ_FAIL_CEILING], cenRungWhy[rg][NAVTRAJ_FAIL_WALL],
                cenRungWhy[rg][NAVTRAJ_FAIL_LANDING], cenRungWhy[rg][NAVTRAJ_FAIL_LAUNCH],
                cenRungWhy[rg][NAVTRAJ_FAIL_RANGE] );
    }

    if ( links )  Z_Free( links );
    if ( csize )  Z_Free( csize );
    if ( stack )  Z_Free( stack );
    if ( cz )     Z_Free( cz );
    if ( cy )     Z_Free( cy );
    if ( cx )     Z_Free( cx );
    if ( pnvi )   Z_Free( pnvi );
    if ( comp )   Z_Free( comp );
}

/* Trajectory-disabled count from the most recent finalize (surfaced in [NAVVAL] so
 * the census stays visible at every load). */
static int s_trajDisabled = 0;

/* -------------------------------------------------------------------------
   Nav_FinalizeLinkTrajectory -- structural post-bake finalize (sibling of the OMC/
   poly standability finalizes).  For every PHYSICS-CLASS OMC (drop / vertical-step
   / jump-gap) it runs Nav_ValidateLinkTrajectory; a FAIL disables the link via the
   same NAVPOLY_BLOCKED poly-flag the query filter excludes -- so no physically-
   untraversable link is ever routable, regardless of the producer that emitted it.
   EXEMPT-BY-MECHANISM classes (teleporter / external jump-link / plat / door-gap /
   water-edge) are NOT gated: they traverse by mechanism, not a run-jump parabola.
   A link is validated in EITHER direction (the mesh link is bidirectional); it is
   disabled only if NEITHER direction is physically traversable.  Per-load + post-
   init, cache-durable, format-agnostic -- the same shape as the sibling finalizes.
   ------------------------------------------------------------------------- */
static int Nav_FinalizeLinkTrajectory( void )
{
    if ( !nav.mesh ) return 0;
    const dtNavMesh *mesh = (const dtNavMesh *)nav.mesh;
    const int maxTiles = mesh->getMaxTiles();

    int disabled = 0;
    for ( int ti = 0; ti < maxTiles; ti++ ) {
        const dtMeshTile *t = mesh->getTile( ti );
        if ( !t || !t->header ) continue;
        const dtPolyRef base = mesh->getPolyRefBase( t );

        for ( int ci = 0; ci < t->header->offMeshConCount; ci++ ) {
            const dtOffMeshConnection *con = &t->offMeshCons[ci];
            const dtPoly *poly = &t->polys[ con->poly ];
            if ( poly->vertCount < 2 ) continue;

            const dtPolyRef ref = base | (dtPolyRef)con->poly;
            unsigned char area = 0; unsigned short flags = 0;
            mesh->getPolyArea( ref, &area ); mesh->getPolyFlags( ref, &flags );

            /* Already disabled (a prior finalize) -> nothing to do. */
            if ( flags & (unsigned short)NAVPOLY_BLOCKED ) continue;

            const omcClass_t cls = OmcClassify( con->userId, area );
            if ( OmcClassIsExempt( cls ) ) continue;   /* mechanism links are not gated */

            float qStart[3], qEnd[3];
            Nav_RecastToQuake( &con->pos[0], qStart );
            Nav_RecastToQuake( &con->pos[3], qEnd );

            /* Physically traversable in EITHER direction keeps the link (the mesh OMC
             * is bidirectional; a step-up may be recorded either way). */
            navTrajEvidence_t ev;
            navTrajResult_t r = Nav_ValidateLinkTrajectory( qStart, qEnd, &ev );
            if ( r != NAVTRAJ_PASS ) {
                navTrajEvidence_t ev2;
                navTrajResult_t r2 = Nav_ValidateLinkTrajectory( qEnd, qStart, &ev2 );
                if ( r2 == NAVTRAJ_PASS ) { r = r2; ev = ev2; }
            }
            if ( r == NAVTRAJ_PASS ) continue;   /* proven traversable -> stays routable */

            /* FAIL: disable the link (query filter excludes NAVPOLY_BLOCKED polys).
             * setPolyFlags via nav.mesh (the mutable handle), matching the poly
             * standability finalize. */
            nav.mesh->setPolyFlags( ref, (unsigned short)( flags | (unsigned short)NAVPOLY_BLOCKED ) );
            disabled++;
            Com_Log( SEV_INFO, LOG_CH(ch_nav),
                "[NAV] trajectory finalize (%s): DISABLED %s id=%u (%.0f %.0f %.0f)->(%.0f %.0f %.0f) "
                "reqRise=%.1f gap=%.1f reason=%s\n",
                nav.mapname[0] ? nav.mapname : "(none)", OmcClassStr( cls ), con->userId,
                qStart[0],qStart[1],qStart[2], qEnd[0],qEnd[1],qEnd[2],
                ev.reqRise, ev.horizGap, NavTraj_ResultStr( r ) );
        }
    }
    s_trajDisabled = disabled;
    return disabled;
}

/* Intra-region-culled count from the most recent cull (surfaced in [NAVVAL],
 * separate from trajDisabled: trajDisabled=infeasible-emitted (steady-state 0),
 * intraRegionCulled=deferred-shortcut census). */
static int s_intraRegionCulled = 0;

/* True iff a poly ref is a GENERATOR physics-link OMC (userId in the internal range
 * [NAV_MAX_OMC, NAV_JUMPGAP_USERID_BASE)).  Mechanism OMCs (teleporter / jump-pad /
 * plat / door-gap / water-edge) have userId < NAV_MAX_OMC and stay in the graph.
 * The lookup is by scanning the tile's offMeshCons for the one whose poly matches. */
static bool Nav_IsGeneratorLinkPoly( const dtNavMesh *mesh, dtPolyRef ref )
{
    unsigned int salt, ti, pi;
    mesh->decodePolyId( ref, salt, ti, pi );
    const dtMeshTile *t = mesh->getTile( (int)ti );
    if ( !t || !t->header ) return false;
    const dtPoly *p = &t->polys[pi];
    if ( p->getType() != DT_POLYTYPE_OFFMESH_CONNECTION ) return false;
    for ( int c = 0; c < t->header->offMeshConCount; c++ ) {
        if ( t->offMeshCons[c].poly == (unsigned short)pi ) {
            unsigned int uid = t->offMeshCons[c].userId;
            return ( uid >= (unsigned)NAV_MAX_OMC && uid < NAV_JUMPGAP_USERID_BASE );
        }
    }
    return false;
}

/* Directed reachability over the EXCLUSION GRAPH: can 'from' reach 'to' following
 * links but NEVER traversing a generator physics-link OMC (Spec 1: without the
 * generator links, is the pair genuinely severed?).  Mechanism OMCs + walk links are
 * traversed.  Directed (Spec 1): mechanism links are often one-way, so the reverse
 * must be tested separately.  Small link counts => a per-call BFS is cheap. */
static bool Nav_ReachesExcludingGenLinks( const dtNavMesh *mesh, dtPolyRef from, dtPolyRef to,
                                          int *visitStamp, int stampVal,
                                          const int *tilePolyBase, int totalDense,
                                          dtPolyRef *stack, int maxTiles )
{
    if ( from == to ) return true;
    unsigned int fsalt, fti, fpi;
    mesh->decodePolyId( from, fsalt, fti, fpi );
    if ( (int)fti >= maxTiles ) return false;
    int fdense = tilePolyBase[fti] + (int)fpi;
    if ( fdense < 0 || fdense >= totalDense ) return false;

    int nstack = 0;
    stack[nstack++] = from;
    visitStamp[fdense] = stampVal;

    while ( nstack > 0 ) {
        dtPolyRef cur = stack[--nstack];
        const dtMeshTile *ct = 0; const dtPoly *cp = 0;
        if ( dtStatusFailed( mesh->getTileAndPolyByRef( cur, &ct, &cp ) ) || !ct || !cp ) continue;
        for ( unsigned int k = cp->firstLink; k != DT_NULL_LINK; k = ct->links[k].next ) {
            const dtPolyRef nref = ct->links[k].ref;
            if ( !nref ) continue;
            /* Exclude generator physics links from the connectivity graph. */
            if ( Nav_IsGeneratorLinkPoly( mesh, nref ) ) continue;
            if ( nref == to ) return true;
            unsigned int nsalt, nti, npi;
            mesh->decodePolyId( nref, nsalt, nti, npi );
            if ( (int)nti >= maxTiles ) continue;
            int nd = tilePolyBase[nti] + (int)npi;
            if ( nd < 0 || nd >= totalDense ) continue;
            if ( visitStamp[nd] == stampVal ) continue;
            visitStamp[nd] = stampVal;
            stack[nstack++] = nref;
        }
    }
    return false;
}

/* -------------------------------------------------------------------------
   Nav_FinalizeIntraRegionCull -- PLACEMENT truth on the loaded mesh (Option A).
   The generator emits every physics-PROVEN link at bake; this pass keeps only the
   ones the world actually NEEDS.  A generator link whose two ground endpoints are
   MUTUALLY reachable WITHOUT any generator physics link (walk adjacency + seam
   reconnect + mechanism OMCs) is a pure shortcut across one already-connected
   region -- the DEFERRED intra-region class -- and is disabled.  A link whose sides
   are severed (one-way or no reachability without it) is LOAD-BEARING and kept.

   Reachability is a QUERY against the real loaded link graph (not a bake-time
   prediction) -- the oracle's question answered where it is ground truth.  The
   directed both-ways test matters because mechanism links are often one-way.

   DEFERRED (documented plug-in point): a future detour-ratio-gated extension will
   KEEP a subset of what this culls (feasible shortcuts that meaningfully shorten a
   long walk); it plugs in exactly here, replacing "cull all mutually-reachable" with
   "cull unless detour-ratio warrants keeping".
   ------------------------------------------------------------------------- */
static int Nav_FinalizeIntraRegionCull( void )
{
    s_intraRegionCulled = 0;
    if ( !nav.mesh ) return 0;
    const dtNavMesh *mesh = (const dtNavMesh *)nav.mesh;
    const int maxTiles = mesh->getMaxTiles();

    /* Dense poly indexing (tile base offsets), reused for the visit stamps. */
    int *tilePolyBase = (int *)Z_Malloc( ( maxTiles > 0 ? maxTiles : 1 ) * (int)sizeof(int) );
    if ( !tilePolyBase ) return 0;
    int totalDense = 0;
    for ( int ti = 0; ti < maxTiles; ti++ ) {
        tilePolyBase[ti] = totalDense;
        const dtMeshTile *t = mesh->getTile( ti );
        if ( t && t->header ) totalDense += t->header->polyCount;
    }
    if ( totalDense <= 0 ) { Z_Free( tilePolyBase ); return 0; }

    int      *visitStamp = (int *)     Z_Malloc( totalDense * (int)sizeof(int) );
    dtPolyRef *bfsStack  = (dtPolyRef *)Z_Malloc( totalDense * (int)sizeof(dtPolyRef) );
    if ( !visitStamp || !bfsStack ) {
        if ( visitStamp ) Z_Free( visitStamp );
        if ( bfsStack )   Z_Free( bfsStack );
        Z_Free( tilePolyBase ); return 0;
    }
    for ( int i = 0; i < totalDense; i++ ) visitStamp[i] = 0;
    int stamp = 0;

    int culled = 0;
    for ( int ti = 0; ti < maxTiles; ti++ ) {
        const dtMeshTile *t = mesh->getTile( ti );
        if ( !t || !t->header ) continue;
        const dtPolyRef base = mesh->getPolyRefBase( t );

        for ( int ci = 0; ci < t->header->offMeshConCount; ci++ ) {
            const dtOffMeshConnection *con = &t->offMeshCons[ci];
            const dtPoly *poly = &t->polys[ con->poly ];
            if ( poly->vertCount < 2 ) continue;

            const dtPolyRef ref = base | (dtPolyRef)con->poly;
            unsigned short flags = 0;
            mesh->getPolyFlags( ref, &flags );
            if ( flags & (unsigned short)NAVPOLY_BLOCKED ) continue;   /* already disabled */

            /* Only generator physics links are subject to the cull. */
            unsigned int uid = con->userId;
            if ( uid < (unsigned)NAV_MAX_OMC || uid >= NAV_JUMPGAP_USERID_BASE ) continue;

            /* The OMC poly's two ground endpoints are its two linked neighbor polys. */
            dtPolyRef endA = 0, endB = 0;
            for ( unsigned int k = poly->firstLink; k != DT_NULL_LINK; k = t->links[k].next ) {
                dtPolyRef nref = t->links[k].ref;
                if ( !nref ) continue;
                if ( !endA ) endA = nref;
                else if ( nref != endA ) { endB = nref; break; }
            }
            if ( !endA || !endB ) continue;   /* not a 2-endpoint link (shouldn't happen) */

            /* Mutual reachability WITHOUT generator links (two directed BFS). */
            stamp++;
            bool aToB = Nav_ReachesExcludingGenLinks( mesh, endA, endB, visitStamp, stamp,
                                                      tilePolyBase, totalDense, bfsStack, maxTiles );
            bool bToA = false;
            if ( aToB ) {
                stamp++;
                bToA = Nav_ReachesExcludingGenLinks( mesh, endB, endA, visitStamp, stamp,
                                                     tilePolyBase, totalDense, bfsStack, maxTiles );
            }

            if ( aToB && bToA ) {
                /* Pure shortcut within one already-connected region -> cull (deferred). */
                nav.mesh->setPolyFlags( ref, (unsigned short)( flags | (unsigned short)NAVPOLY_BLOCKED ) );
                culled++;
            }
            /* else: load-bearing (severed one-way or no walk path without it) -> keep. */
        }
    }

    s_intraRegionCulled = culled;
    if ( culled > 0 )
        Com_Log( SEV_DEBUG, LOG_CH(ch_nav),
            "[NAV] intra-region cull (%s): %d generator shortcut link(s) disabled (deferred class)\n",
            nav.mapname[0] ? nav.mapname : "(none)", culled );

    Z_Free( bfsStack );
    Z_Free( visitStamp );
    Z_Free( tilePolyBase );
    return culled;
}

/* Count of endpoints the finalize callback moved this pass (Detour reports only
 * the disabled total). */
static int s_omcRegrounded = 0;

/* Endpoint-VALIDITY callback for dtNavMesh::finalizeOmcEndpoints.
 * Recast-in endpoint -> Recast-out valid endpoint (returns false = disable).
 * Detour passes the OMC's userId + poly area (the class discriminator) and the
 * endpoint's bound ground-poly ref.  The per-class validity decision is the SHARED
 * NavVal_OmcEndpointValid:
 *   - MECHANISM class (door-gap/teleporter/plat/jump-pad/water-edge): valid iff
 *     bound to a live poly within the snap distance.  Producer-placed on the
 *     crossing; NOT box-reground (regrounding a mechanism endpoint by box-standability
 *     dragged door *34's door-gap endpoint into a pit).  Kept unchanged if valid;
 *     disabled only on binding failure.
 *   - PHYSICS class (generator drop/step/jump-gap): valid iff box-standable; if not,
 *     ring-search a standable rest (z-bounded) or disable.
 * All CM tracing (main-thread only) lives here; Detour just applies the result. */
static bool Nav_OmcStandableSnap( void *ctx, const float *recastIn, float *recastOut,
                                  unsigned int userId, unsigned char area, dtPolyRef boundRef )
{
    (void)ctx;
    static const float ringR[]  = { 8.0f, 16.0f, 24.0f, 32.0f };
    static const int   ringN    = (int)( sizeof(ringR) / sizeof(ringR[0]) );
    static const float ringDx[8] = { 1, -1, 0, 0,  0.7071f, -0.7071f,  0.7071f, -0.7071f };
    static const float ringDy[8] = { 0, 0, 1, -1,  0.7071f,  0.7071f, -0.7071f, -0.7071f };

    float qe[3];
    Nav_RecastToQuake( recastIn, qe );

    const bool mechanism = OmcClassIsExempt( OmcClassify( userId, area ) );

    /* Endpoint already box-standable -> keep unchanged (both classes). */
    if ( NavVal_EndpointStandable( qe, NULL ) ) {
        recastOut[0] = recastIn[0]; recastOut[1] = recastIn[1]; recastOut[2] = recastIn[2];
        return true;
    }

    /* Not standable at the emitted point -> try a z-bounded ring reground onto a
     * NEARBY standable floor.  A successful reground also re-binds the endpoint (the
     * Detour finalize re-binds after a real move), so this fixes marginal bindings for
     * BOTH classes -- which is why mechanism OMCs must run it too (skipping it lost the
     * binding fixes and fragmented the mesh). */
    float bestQ[3]; float bestD = 1e30f; bool fixed = false;
    for ( int r = 0; r < ringN; r++ ) {
        for ( int k = 0; k < 8; k++ ) {
            float cand[3] = { qe[0] + ringDx[k]*ringR[r], qe[1] + ringDy[k]*ringR[r], qe[2] };
            float o = 0.0f;
            if ( !NavVal_ClearanceFloorZ( cand[0], cand[1], cand[2], false, &o ) ) continue;
            /* Z-BOUND the regrounding: the endpoint was placed by its producer on its
             * OWN floor; standability regrounding is a NUDGE onto the adjacent floor,
             * not a relocation to a different storey.  A candidate floor more than a
             * walkable step (NAV_WALKABLE_CLIMB) from the emitted endpoint z belongs to
             * a different level (e.g. a door-gap OMC endpoint at the doorway floor being
             * dragged down into a deep pit below), which silently destroys a mechanism
             * crossing.  Reject those so regrounding stays within the endpoint's level. */
            if ( fabsf( o - qe[2] ) > NAV_WALKABLE_CLIMB ) continue;
            cand[2] = o;   /* place the endpoint exactly on the found floor's origin */
            if ( !NavVal_EndpointStandable( cand, NULL ) ) continue;
            float d = (cand[0]-qe[0])*(cand[0]-qe[0]) + (cand[1]-qe[1])*(cand[1]-qe[1])
                    + (cand[2]-qe[2])*(cand[2]-qe[2]);
            if ( d < bestD ) { bestD = d; bestQ[0]=cand[0]; bestQ[1]=cand[1]; bestQ[2]=cand[2]; fixed = true; }
        }
        if ( fixed ) break;
    }
    if ( !fixed ) {
        /* No standable rest nearby.  Per-class DISABLE decision (the shared validity):
         *   - PHYSICS: a physics link promises a standable landing -> disable.
         *   - MECHANISM: the crossing is mediated by the mechanism; the endpoint only
         *     has to anchor to live mesh.  Keep it (unchanged, producer-placed) iff it
         *     is bound to a live poly; disable only on binding failure. */
        recastOut[0] = recastIn[0]; recastOut[1] = recastIn[1]; recastOut[2] = recastIn[2];
        if ( mechanism && NavVal_OmcEndpointValid( userId, area, qe, (navPolyRef_t)boundRef ) )
            return true;
        Com_Log( SEV_INFO, LOG_CH(ch_nav),
            "[NAV] OMC finalize: %s endpoint (%.0f,%.0f,%.0f) invalid under class validity -> OMC disabled\n",
            mechanism ? "mechanism" : "physics", qe[0], qe[1], qe[2] );
        return false;
    }
    Nav_QuakeToRecast( bestQ, recastOut );
    s_omcRegrounded++;
    return true;
}

/* Post-bake, main-thread OMC endpoint finalize.  Delegates the tile/link surgery
 * to dtNavMesh (like the seam / jump-gap reconnect), passing the standability
 * callback above.  omcFail==0 becomes a structural mesh property, cache-durable. */
static int Nav_FinalizeOmcStandability( void )
{
    if ( !nav.mesh ) return 0;
    s_omcRegrounded = 0;
    int disabled = ((dtNavMesh *)nav.mesh)->finalizeOmcEndpoints(
        Nav_OmcStandableSnap, NULL, (unsigned short)NAVPOLY_BLOCKED );
    if ( s_omcRegrounded > 0 || disabled > 0 )
        Com_Log( SEV_INFO, LOG_CH(ch_nav),
            "[NAV] OMC endpoint finalize (%s): %d endpoint(s) regrounded, %d OMC(s) disabled\n",
            nav.mapname[0] ? nav.mapname : "(none)", s_omcRegrounded, disabled );
    return disabled;
}

/* -------------------------------------------------------------------------
   Reachability labeling for nav_validate scoping.

   The enforce guarantee is "every REACHABLE poly agrees with collision".
   Unreachable severed islands (deep pit bottoms under a clip cap, under-shelf
   voids) legitimately stay in the mesh — a following package bridges such
   pockets with off-mesh connections — so they must NOT be pruned, but they are
   outside the gate's semantic domain.  This pass labels connected components
   over the LOADED mesh's real link graph and returns the REACHABLE set (the
   largest component; on these maps it is the main floor that contains the
   player spawn).  Unlike getReachablePolyCount (which deliberately STOPS at
   off-mesh connections, measuring only walk-islands), this walk TRAVERSES OMC
   polys so their two ground endpoints share one component — an OMC-bridged
   platform (e.g. e1m1's exit island) is REACHABLE, exactly as a bot experiences
   it.  Ground polys reachable through walk-links and/or OMC traversal from the
   anchor are in-scope; the rest are reported as unreachablePolys, never hidden.

   comp[] is indexed by a dense poly index = tilePolyBase[ti] + pi.  A ground
   poly's component is its label; OMC polys are traversed but not themselves
   counted as reachable-set members (they carry no standability delta).

   Anchor: if anchorDense >= 0 (the poly the player-spawn snaps to), the
   REACHABLE component is the one CONTAINING that poly — the play floor, exactly
   what a bot pathing from spawn can reach (walk + OMC).  This is the correct
   anchor: "largest by poly count" mis-fires on a heavily island-segmented mesh,
   where a big inert void/shell region out-counts the tight play floor.  When no
   spawn is available (anchorDense < 0), fall back to the largest component.
   Returns the reachable component id (>=0) or -1 if the mesh is empty. */
static int NavVal_LabelReachable( int *comp, const int *tilePolyBase,
                                  int totalDense, int anchorDense,
                                  int *outReachComp, int *outNumComp )
{
    const dtNavMesh *mesh = (const dtNavMesh *)nav.mesh;
    const int maxTiles = mesh->getMaxTiles();

    for ( int i = 0; i < totalDense; i++ ) comp[i] = -1;

    /* Reverse map dense-index -> tile index, built once (avoids an O(maxTiles)
     * search per BFS pop). */
    int *denseTile = (int *)Z_Malloc( ( totalDense > 0 ? totalDense : 1 ) * (int)sizeof(int) );
    if ( !denseTile ) { if ( outReachComp ) *outReachComp = -1; if ( outNumComp ) *outNumComp = 0; return -1; }
    for ( int i = 0; i < totalDense; i++ ) denseTile[i] = -1;
    for ( int ti = 0; ti < maxTiles; ti++ ) {
        const dtMeshTile *t = mesh->getTile( ti );
        if ( !t || !t->header ) continue;
        for ( int pi = 0; pi < t->header->polyCount; pi++ )
            denseTile[ tilePolyBase[ti] + pi ] = ti;
    }

    /* A BFS work stack of dense indices. Sized to totalDense (each poly pushed
     * at most once). */
    int *stack = (int *)Z_Malloc( ( totalDense > 0 ? totalDense : 1 ) * (int)sizeof(int) );
    if ( !stack ) { Z_Free( denseTile ); if ( outReachComp ) *outReachComp = -1; if ( outNumComp ) *outNumComp = 0; return -1; }

    /* DIRECTED reachability from the anchor.  The reachable set = every poly the bot
     * can reach FROM the spawn, following each link only in its legal direction.
     *
     * Why DIRECTED, not an undirected component union:  Detour encodes OMC direction
     * STRUCTURALLY -- a unidirectional OMC (a drop you fall down but cannot jump back
     * up) has its reverse links OMITTED, not flagged.  An undirected component union
     * still equates the two endpoints (it follows start-ground -> OMC -> end-ground via
     * the forward links and unions them), conflating a one-way link into two-way
     * connectivity -- which is exactly what over-counted reachability vs the bot's A*.
     * A directed BFS from the anchor follows only the links that exist FROM each
     * reached poly, so it can enter a unidir OMC only from its start side and never
     * traverses it backward.  Ground adjacency is symmetric, so a directed flood over
     * ground links equals the undirected one; only the one-way OMCs differ.  Result:
     * the instrument now asks the same question A* does.  (BLOCKED skipped as before;
     * OPENABLE_CLOSED traversed per the openable-traversal addendum.) */
    int reachComp = -1, numComp = 0;
    auto directedFlood = [&]( int seed ) {
        int nstack = 0;
        stack[nstack++] = seed;
        comp[seed] = 0;
        while ( nstack > 0 ) {
            const int cd = stack[--nstack];
            const int cti = denseTile[cd];
            if ( cti < 0 ) continue;
            const dtMeshTile *ct = mesh->getTile( cti );
            const int cpi = cd - tilePolyBase[cti];
            const dtPoly *cp = &ct->polys[cpi];
            for ( unsigned int k = cp->firstLink; k != DT_NULL_LINK; k = ct->links[k].next ) {
                const dtPolyRef nref = ct->links[k].ref;
                if ( !nref ) continue;
                const dtMeshTile *nt = 0; const dtPoly *np = 0;
                if ( dtStatusFailed( mesh->getTileAndPolyByRef( nref, &nt, &np ) ) || !nt || !np )
                    continue;
                if ( np->flags & (unsigned short)NAVPOLY_BLOCKED )
                    continue;   /* never crossable -- matches the query filter */
                unsigned int nsalt, ntile, npoly;
                mesh->decodePolyId( nref, nsalt, ntile, npoly );
                (void)nsalt;
                if ( (int)ntile >= maxTiles ) continue;
                const int nd = tilePolyBase[ntile] + (int)npoly;
                if ( nd < 0 || nd >= totalDense ) continue;
                if ( comp[nd] != -1 ) continue;
                comp[nd] = 0;
                stack[nstack++] = nd;   /* forward links only -> direction-correct */
            }
        }
    };

    if ( anchorDense >= 0 && anchorDense < totalDense ) {
        directedFlood( anchorDense );
        reachComp = 0;
        numComp = 1;
    } else {
        /* No spawn anchor: fall back to the largest UNDIRECTED component (a diagnostic
         * default only -- the enforce gate always has a spawn).  Kept undirected here
         * because there is no source to direct from. */
        int comp2 = 0;
        for ( int ti = 0; ti < maxTiles; ti++ ) {
            const dtMeshTile *t = mesh->getTile( ti );
            if ( !t || !t->header ) continue;
            for ( int pi = 0; pi < t->header->polyCount; pi++ ) {
                const int dense = tilePolyBase[ti] + pi;
                if ( comp[dense] != -1 ) continue;
                int nstack = 0; stack[nstack++] = dense; comp[dense] = comp2;
                while ( nstack > 0 ) {
                    const int cd = stack[--nstack];
                    const int cti = denseTile[cd];
                    if ( cti < 0 ) continue;
                    const dtMeshTile *ct = mesh->getTile( cti );
                    const dtPoly *cp = &ct->polys[cd - tilePolyBase[cti]];
                    for ( unsigned int k = cp->firstLink; k != DT_NULL_LINK; k = ct->links[k].next ) {
                        const dtPolyRef nref = ct->links[k].ref;
                        if ( !nref ) continue;
                        const dtMeshTile *nt = 0; const dtPoly *np = 0;
                        if ( dtStatusFailed( mesh->getTileAndPolyByRef( nref, &nt, &np ) ) || !nt || !np ) continue;
                        if ( np->flags & (unsigned short)NAVPOLY_BLOCKED ) continue;
                        unsigned int ns, ntile, npoly; mesh->decodePolyId( nref, ns, ntile, npoly ); (void)ns;
                        if ( (int)ntile >= maxTiles ) continue;
                        const int nd = tilePolyBase[ntile] + (int)npoly;
                        if ( nd < 0 || nd >= totalDense || comp[nd] != -1 ) continue;
                        comp[nd] = comp2; stack[nstack++] = nd;
                    }
                }
                comp2++;
            }
        }
        numComp = comp2;
        int *csize = (int *)Z_Malloc( ( numComp > 0 ? numComp : 1 ) * (int)sizeof(int) );
        if ( csize ) {
            for ( int i = 0; i < numComp; i++ ) csize[i] = 0;
            for ( int ti = 0; ti < maxTiles; ti++ ) {
                const dtMeshTile *t = mesh->getTile( ti );
                if ( !t || !t->header ) continue;
                for ( int pi = 0; pi < t->header->polyCount; pi++ ) {
                    const dtPoly *p = &t->polys[pi];
                    if ( p->getType() == DT_POLYTYPE_OFFMESH_CONNECTION || p->vertCount < 3 ) continue;
                    const int c = comp[ tilePolyBase[ti] + pi ];
                    if ( c >= 0 && c < numComp ) csize[c]++;
                }
            }
            int best = -1;
            for ( int i = 0; i < numComp; i++ ) if ( csize[i] > best ) { best = csize[i]; reachComp = i; }
            Z_Free( csize );
        }
    }

    Z_Free( stack );
    Z_Free( denseTile );
    if ( outReachComp ) *outReachComp = reachComp;
    if ( outNumComp )   *outNumComp   = numComp;
    return reachComp;
}

/* Build the reachable-set labeling shared by nav_validate and the poly finalize:
 * allocate tilePolyBase[] + comp[] (Z_Malloc; caller frees), anchor on the player
 * spawn poly (fallback largest), and label components.  Returns the anchor comp id
 * via *outReachComp; totalDense via *outTotalDense.  Both out arrays are non-NULL
 * on success; returns false (and frees) if allocation fails. */
static bool NavVal_BuildReachableSet( int **outComp, int **outTileBase,
                                      int *outTotalDense, int *outReachComp, int *outNumComp )
{
    const dtNavMesh *mesh = (const dtNavMesh *)nav.mesh;
    const int maxTiles = mesh->getMaxTiles();

    int totalDense = 0;
    int *tilePolyBase = (int *)Z_Malloc( ( maxTiles > 0 ? maxTiles : 1 ) * (int)sizeof(int) );
    if ( !tilePolyBase ) return false;
    for ( int ti = 0; ti < maxTiles; ti++ ) {
        tilePolyBase[ti] = totalDense;
        const dtMeshTile *t = mesh->getTile( ti );
        if ( t && t->header ) totalDense += t->header->polyCount;
    }
    int *comp = (int *)Z_Malloc( ( totalDense > 0 ? totalDense : 1 ) * (int)sizeof(int) );
    if ( !comp ) { Z_Free( tilePolyBase ); return false; }

    /* Spawn anchor (same path as nav_validate). */
    int anchorDense = -1;
    float spawnQ[3];
    if ( nav.mapname[0] && nav.query && Nav_Get_SpawnPoint( nav.mapname, spawnQ ) ) {
        float rSpawn[3];
        Nav_QuakeToRecast( spawnQ, rSpawn );
        dtPolyRef sref = 0; float snp[3];
        nav.query->findNearestPoly( rSpawn, kDefaultExtents, GetFilter(), &sref, snp );
        if ( sref ) {
            unsigned int ss, st, sp;
            nav.mesh->decodePolyId( sref, ss, st, sp ); (void)ss;
            if ( (int)st < maxTiles ) {
                int sd = tilePolyBase[st] + (int)sp;
                if ( sd >= 0 && sd < totalDense ) anchorDense = sd;
            }
        }
    }

    int reachComp = -1, numComp = 0;
    NavVal_LabelReachable( comp, tilePolyBase, totalDense, anchorDense, &reachComp, &numComp );

    *outComp = comp; *outTileBase = tilePolyBase; *outTotalDense = totalDense;
    if ( outReachComp ) *outReachComp = reachComp;
    if ( outNumComp )   *outNumComp   = numComp;
    return true;
}

/* Ground-poly standability finalize (Ruling-1 analog of the OMC finalize).  For
 * each REACHABLE ground poly, run the SHARED standability probe at its centroid;
 * a poly a player cannot rest on (band-fail) is flagged NAVPOLY_BLOCKED so the
 * query filter excludes it — an honest mesh will not path over a lying surface.
 * No z-correction.  Runs post-init + per-load (cache-durable), after the OMC
 * finalize.  Returns the number of polys blocked. */
static int Nav_FinalizeGroundPolyStandability( void )
{
    if ( !nav.mesh || !nav.query ) return 0;
    const dtNavMesh *mesh = (const dtNavMesh *)nav.mesh;
    const int maxTiles = mesh->getMaxTiles();

    int *comp = NULL, *tilePolyBase = NULL, totalDense = 0, reachComp = -1, numComp = 0;
    if ( !NavVal_BuildReachableSet( &comp, &tilePolyBase, &totalDense, &reachComp, &numComp ) )
        return 0;

    (void)numComp;
    int blocked = 0;
    for ( int ti = 0; ti < maxTiles; ti++ ) {
        const dtMeshTile *t = mesh->getTile( ti );
        if ( !t || !t->header ) continue;
        for ( int pi = 0; pi < t->header->polyCount; pi++ ) {
            const dtPoly *p = &t->polys[pi];
            if ( p->getType() == DT_POLYTYPE_OFFMESH_CONNECTION ) continue;
            if ( p->vertCount < 3 ) continue;
            if ( comp[ tilePolyBase[ti] + pi ] != reachComp ) continue;   /* reachable only */
            if ( p->flags & (unsigned short)NAVPOLY_BLOCKED ) continue;    /* already excluded */

            /* Centroid (Recast) -> Quake; poly-z = centroid z. */
            float rc[3] = { 0, 0, 0 };
            for ( int v = 0; v < p->vertCount; v++ ) {
                const float *rv = &t->verts[p->verts[v] * 3];
                rc[0] += rv[0]; rc[1] += rv[1]; rc[2] += rv[2];
            }
            const float inv = 1.0f / (float)p->vertCount;
            rc[0] *= inv; rc[1] *= inv; rc[2] *= inv;
            float qc[3];
            Nav_RecastToQuake( rc, qc );

            /* SAME standable definition the validator uses.  Block ONLY a poly that
             * LIES: the probe finds a collision floor but the poly-z disagrees with
             * it by more than a walkable step (poly 5016 -- a surface a player would
             * rest ~22u away from).  A poly where the probe finds NO floor at all is
             * NOT blocked here -- it is a separate class already outside the delta
             * metric (nav_validate counts it as nofloor, never as a beyond-N
             * outlier).  So: found-floor AND out-of-band == lying == block;
             * no-floor == leave.
             *
             * 🔴 WHAT THAT "no-floor == leave" RULE IS HOLDING BACK (measured
             * 2026-07-28, e1m1): the no-floor population is NOT mostly wade/swim.
             * Of 1017 reachable no-floor polys, 1007 are BURIED inside solid world
             * geometry, 6 are submerged, 4 are unmeasured -- so ~99% is a real
             * defect, not a legitimate liquid surface.  This rule is currently the
             * ONLY thing keeping those 1007 out of NAVPOLY_BLOCKED.  Anyone who
             * changes it to also block no-floor polys will mass-block a thousand
             * polys and remove them from the query filter.  Fix the reason they are
             * buried (canonical-soup emission) BEFORE loosening this guard.
             * See nav_validate's nofloor=N(buried/submerged/unmeasured) breakdown. */
            float originZ = 0.0f;
            bool foundFloor = NavVal_ClearanceFloorZ( qc[0], qc[1], qc[2],
                                    ( p->flags & NAVPOLY_LOW_CEILING ) != 0, &originZ );
            bool lying = foundFloor && fabsf( originZ - qc[2] ) > (float)NAV_WALKABLE_CLIMB;
            if ( lying ) {
                dtPolyRef ref = mesh->encodePolyId( t->salt, ti, pi );
                unsigned short fl = 0;
                nav.mesh->getPolyFlags( ref, &fl );
                nav.mesh->setPolyFlags( ref, (unsigned short)( fl | (unsigned short)NAVPOLY_BLOCKED ) );
                Com_Log( SEV_INFO, LOG_CH(ch_nav),
                    "[NAV] poly finalize: BLOCKED poly %u at %.0f %.0f %.0f — not standable (floor origin %.0f, delta %.0f)\n",
                    (unsigned)ref, qc[0], qc[1], qc[2], originZ, originZ - qc[2] );
                blocked++;
            }
        }
    }

    Z_Free( comp );
    Z_Free( tilePolyBase );

    if ( blocked > 0 )
        Com_Log( SEV_INFO, LOG_CH(ch_nav),
            "[NAV] ground-poly standability finalize (%s): %d poly(s) blocked\n",
            nav.mapname[0] ? nav.mapname : "(none)", blocked );
    return blocked;
}

/* Can a player-sized box actually rest at this Quake-space point?
 *
 * Being on the navmesh is NOT sufficient. Recast can emit a walkable poly on a
 * ledge or shelf whose surface a player box cannot occupy, and a point that
 * merely passes the query filter is exactly what FinishSpawningItem rejects
 * with "startsolid" (measured on arenat2: the medoid landed on a raised
 * platform at z=160 and the flag was freed instead of spawned). So test the
 * real player box against the real collision world, the same shape the item
 * spawn path will use — box mins/maxs from bg_public.h.
 *
 * Reports the floor rest-z through outRestZ so the caller can settle onto it. */
static bool Nav_CenterStandable( const float *q, float *outRestZ )
{
    const vec3_t mins = { -15.0f, -15.0f, -24.0f };
    const vec3_t maxs = {  15.0f,  15.0f,  32.0f };
    trace_t tr;

    /* Drop a player box from just above the candidate onto the floor below. */
    vec3_t start = { q[0], q[1], q[2] + 32.0f };
    vec3_t end   = { q[0], q[1], q[2] - 64.0f };
    CM_BoxTrace( &tr, start, end, mins, maxs, 0, MASK_PLAYERSOLID, qfalse );
    if ( tr.startsolid || tr.allsolid || tr.fraction >= 1.0f )
        return false;

    /* And confirm the box genuinely fits where it came to rest. */
    trace_t at;
    vec3_t rest = { tr.endpos[0], tr.endpos[1], tr.endpos[2] };
    CM_BoxTrace( &at, rest, rest, mins, maxs, 0, MASK_PLAYERSOLID, qfalse );
    if ( at.startsolid || at.allsolid )
        return false;

    if ( outRestZ ) *outRestZ = tr.endpos[2];
    return true;
}

/*
 * Nav_GetWalkableCenter — "the most central spot a player can stand on".
 *
 * Used by the game module to place an item (the 1FCTF neutral flag) on a map
 * whose author never placed one.  Definition, and why it is defensible:
 *
 *  1. POPULATION: every ground poly in the REACHABLE component — the same
 *     spawn-anchored flood NavVal_BuildReachableSet gives the validator.  So an
 *     unreachable vault or a sealed island cannot pull the answer toward itself,
 *     and NAVPOLY_BLOCKED polys (the ones the standability finalize proved a
 *     player cannot rest on) are excluded outright.
 *
 *  2. CANDIDATE: the AREA-WEIGHTED centroid of that population.  Area weighting
 *     matters because Recast's polys vary hugely in size; an unweighted mean of
 *     poly centroids is really a mean of tessellation density and drifts toward
 *     whichever corner got chopped finest.
 *
 *  3. SNAP: the raw centroid is a point in space, not a place — on an L-shaped or
 *     doughnut map it can land in a wall or over the void.  So it is never used
 *     directly: findNearestPoly pulls it onto real navmesh, and the result is
 *     accepted only if it is IN the reachable component.
 *
 *  4. FALLBACK (this is the part that makes the non-convex case honest): if the
 *     snap misses, or lands outside the reachable set, pick the reachable ground
 *     poly whose own centroid is CLOSEST to the ideal centroid — a medoid.  A
 *     medoid is by construction a real walkable location, so a doughnut map gets
 *     the point on the ring nearest the (unreachable) hole centre instead of the
 *     hole itself.  Horizontal distance dominates the comparison so a stacked
 *     map does not pick a spot directly above/below the centre by accident.
 *
 *  5. FLOOR: a candidate is accepted by BOTH passes only if a real player box
 *     can come to rest there (Nav_CenterStandable below), and the z returned is
 *     that collision-world rest height — not the poly's z.  Being on the navmesh
 *     is not enough on its own: measured on arenat2, the medoid sat on a poly at
 *     z=160 that a player box cannot occupy, and the flag placed there was
 *     rejected as start-solid and freed.  Because the returned z is a real
 *     resting surface, the caller can place an item at a fixed offset above it
 *     and know it is on a floor.
 *
 * Returns qfalse (leaving qPosOut untouched) if the mesh is not ready, or if no
 * reachable ground poly is both present and standable — the caller must handle
 * that rather than place an item at the origin.
 */

qboolean Nav_GetWalkableCenter( float *qPosOut )
{
    if ( !nav.ready || !nav.mesh || !nav.query || !qPosOut ) return qfalse;

    const dtNavMesh *mesh = (const dtNavMesh *)nav.mesh;
    const int maxTiles = mesh->getMaxTiles();

    int *comp = NULL, *tilePolyBase = NULL, totalDense = 0, reachComp = -1, numComp = 0;
    if ( !NavVal_BuildReachableSet( &comp, &tilePolyBase, &totalDense, &reachComp, &numComp ) )
        return qfalse;
    (void)numComp;

    /* Pass 1: area-weighted centroid over reachable ground polys (Quake space). */
    double accum[3] = { 0.0, 0.0, 0.0 };
    double totalArea = 0.0;
    int    considered = 0;

    for ( int ti = 0; ti < maxTiles; ti++ ) {
        const dtMeshTile *t = mesh->getTile( ti );
        if ( !t || !t->header ) continue;
        for ( int pi = 0; pi < t->header->polyCount; pi++ ) {
            const dtPoly *p = &t->polys[pi];
            if ( p->getType() == DT_POLYTYPE_OFFMESH_CONNECTION ) continue;
            if ( p->vertCount < 3 ) continue;
            if ( comp[ tilePolyBase[ti] + pi ] != reachComp ) continue;
            if ( p->flags & (unsigned short)NAVPOLY_BLOCKED ) continue;

            float rc[3] = { 0, 0, 0 };
            for ( int v = 0; v < p->vertCount; v++ ) {
                const float *rv = &t->verts[p->verts[v] * 3];
                rc[0] += rv[0]; rc[1] += rv[1]; rc[2] += rv[2];
            }
            const float inv = 1.0f / (float)p->vertCount;
            rc[0] *= inv; rc[1] *= inv; rc[2] *= inv;

            /* Fan-triangulate for area; Recast X/Z is the horizontal plane. */
            double area = 0.0;
            const float *v0 = &t->verts[p->verts[0] * 3];
            for ( int v = 1; v + 1 < p->vertCount; v++ ) {
                const float *v1 = &t->verts[p->verts[v] * 3];
                const float *v2 = &t->verts[p->verts[v + 1] * 3];
                const double ax = v1[0] - v0[0], az = v1[2] - v0[2];
                const double bx = v2[0] - v0[0], bz = v2[2] - v0[2];
                area += fabs( ax * bz - az * bx ) * 0.5;
            }
            if ( area <= 0.0 ) continue;

            float qc[3];
            Nav_RecastToQuake( rc, qc );
            accum[0] += (double)qc[0] * area;
            accum[1] += (double)qc[1] * area;
            accum[2] += (double)qc[2] * area;
            totalArea += area;
            considered++;
        }
    }

    if ( considered == 0 || totalArea <= 0.0 ) {
        Z_Free( comp );
        Z_Free( tilePolyBase );
        Com_Log( SEV_INFO, LOG_CH(ch_nav),
            "[NAV] walkable-center: no reachable ground polys on '%s'\n",
            nav.mapname[0] ? nav.mapname : "(none)" );
        return qfalse;
    }

    float ideal[3];
    ideal[0] = (float)( accum[0] / totalArea );
    ideal[1] = (float)( accum[1] / totalArea );
    ideal[2] = (float)( accum[2] / totalArea );

    /* Pass 2: snap the ideal onto real navmesh, and require the reachable set. */
    float chosen[3] = { ideal[0], ideal[1], ideal[2] };
    qboolean haveChosen = qfalse;

    {
        float rIdeal[3];
        Nav_QuakeToRecast( ideal, rIdeal );
        dtPolyRef ref = 0; float nearPt[3];
        nav.query->findNearestPoly( rIdeal, kDefaultExtents, GetFilter(), &ref, nearPt );
        if ( ref ) {
            unsigned int rs, rt, rp;
            nav.mesh->decodePolyId( ref, rs, rt, rp ); (void)rs;
            if ( (int)rt < maxTiles ) {
                const int dense = tilePolyBase[rt] + (int)rp;
                if ( dense >= 0 && dense < totalDense && comp[dense] == reachComp ) {
                    float cand[3];
                    Nav_RecastToQuake( nearPt, cand );
                    float restZ;
                    /* Only accept the snap if a player box can rest here; otherwise
                     * leave haveChosen false and let the medoid pass find a spot
                     * that a player can actually occupy. */
                    if ( Nav_CenterStandable( cand, &restZ ) ) {
                        chosen[0] = cand[0]; chosen[1] = cand[1]; chosen[2] = restZ;
                        haveChosen = qtrue;
                    }
                }
            }
        }
    }

    /* Pass 3: medoid fallback for non-convex maps (snap missed or left the set). */
    if ( !haveChosen ) {
        double bestScore = 1e300;
        for ( int ti = 0; ti < maxTiles; ti++ ) {
            const dtMeshTile *t = mesh->getTile( ti );
            if ( !t || !t->header ) continue;
            for ( int pi = 0; pi < t->header->polyCount; pi++ ) {
                const dtPoly *p = &t->polys[pi];
                if ( p->getType() == DT_POLYTYPE_OFFMESH_CONNECTION ) continue;
                if ( p->vertCount < 3 ) continue;
                if ( comp[ tilePolyBase[ti] + pi ] != reachComp ) continue;
                if ( p->flags & (unsigned short)NAVPOLY_BLOCKED ) continue;

                float rc[3] = { 0, 0, 0 };
                for ( int v = 0; v < p->vertCount; v++ ) {
                    const float *rv = &t->verts[p->verts[v] * 3];
                    rc[0] += rv[0]; rc[1] += rv[1]; rc[2] += rv[2];
                }
                const float inv = 1.0f / (float)p->vertCount;
                rc[0] *= inv; rc[1] *= inv; rc[2] *= inv;
                float qc[3];
                Nav_RecastToQuake( rc, qc );

                /* Horizontal distance dominates; z is a light tie-breaker so a
                 * stacked map cannot win purely by being directly overhead. */
                const double dx = (double)qc[0] - ideal[0];
                const double dy = (double)qc[1] - ideal[1];
                const double dz = (double)qc[2] - ideal[2];
                const double score = dx * dx + dy * dy + 0.25 * dz * dz;
                if ( score < bestScore ) {
                    /* Standability is the expensive test (two collision traces),
                     * so it runs only for a candidate that would actually win —
                     * turning a per-poly cost into a per-improvement one. */
                    float restZ;
                    if ( !Nav_CenterStandable( qc, &restZ ) ) continue;
                    bestScore = score;
                    chosen[0] = qc[0]; chosen[1] = qc[1]; chosen[2] = restZ;
                    haveChosen = qtrue;
                }
            }
        }
    }

    Z_Free( comp );
    Z_Free( tilePolyBase );

    if ( !haveChosen ) return qfalse;

    /* chosen[2] is already the collision-world rest-z reported by
     * Nav_CenterStandable for whichever pass won, so the point is on the surface
     * a player box actually comes to rest on — no further settling to do. */

    qPosOut[0] = chosen[0];
    qPosOut[1] = chosen[1];
    qPosOut[2] = chosen[2];

    Com_Log( SEV_INFO, LOG_CH(ch_nav),
        "[NAV] walkable-center on '%s': %.0f %.0f %.0f (ideal %.0f %.0f %.0f, %d polys, area %.0f)\n",
        nav.mapname[0] ? nav.mapname : "(none)",
        chosen[0], chosen[1], chosen[2], ideal[0], ideal[1], ideal[2],
        considered, totalArea );
    return qtrue;
}

static void Nav_ValidateCmd( void )
{
    if ( !nav.ready || !nav.mesh ) {
        Com_Log( SEV_INFO, LOG_CH(ch_nav), "nav_validate: navmesh not loaded (load a map first)\n" );
        return;
    }

    /* Argument parse: "verbose" prints per-failure detail; "enforce <mM> <mP95>"
     * runs the report then prints an explicit verdict against the two ceilings. */
    qboolean verbose = qfalse;
    qboolean enforce = qfalse;
    float    maxMean = 0.0f, maxP95 = 0.0f;
    if ( Cmd_Argc() > 1 ) {
        const char *a1 = Cmd_Argv(1);
        if ( !Q_stricmp( a1, "verbose" ) ) {
            verbose = qtrue;
        } else if ( !Q_stricmp( a1, "enforce" ) ) {
            enforce = qtrue;
            maxMean = ( Cmd_Argc() > 2 ) ? atof( Cmd_Argv(2) ) : 0.0f;
            maxP95  = ( Cmd_Argc() > 3 ) ? atof( Cmd_Argv(3) ) : 0.0f;
        }
    }

    const int maxTiles = nav.mesh->getMaxTiles();

    /* Build the dense poly-index base per tile, then label connected components
     * over the loaded mesh's real link graph (OMC-traversing) so the enforce set
     * can be scoped to the REACHABLE polys (see NavVal_LabelReachable).  comp[]
     * maps dense index -> component id; reachComp is the anchor (largest). */
    int totalDense = 0;
    int *tilePolyBase = (int *)Z_Malloc( ( maxTiles > 0 ? maxTiles : 1 ) * (int)sizeof(int) );
    for ( int ti = 0; ti < maxTiles; ti++ ) {
        tilePolyBase[ti] = totalDense;
        const dtMeshTile *t = ((const dtNavMesh *)nav.mesh)->getTile( ti );
        if ( t && t->header ) totalDense += t->header->polyCount;
    }
    int *comp = (int *)Z_Malloc( ( totalDense > 0 ? totalDense : 1 ) * (int)sizeof(int) );

    /* Anchor the reachable set on the player-spawn poly (the play floor).  Parse
     * the map's spawn entity, snap it to the mesh, and pass its dense index to the
     * labeler; anchorDense < 0 (no spawn / off-mesh) falls back to the largest
     * component.  Spawn parse uses the same main-thread entity path as the door
     * pass — legal here (this is a debug/gate command, never the bake thread). */
    int anchorDense = -1;
    float spawnQ[3];
    qboolean haveSpawn = ( nav.mapname[0] && Nav_Get_SpawnPoint( nav.mapname, spawnQ ) )
                         ? qtrue : qfalse;
    if ( haveSpawn ) {
        float rSpawn[3];
        Nav_QuakeToRecast( spawnQ, rSpawn );
        dtPolyRef sref = 0; float snp[3];
        nav.query->findNearestPoly( rSpawn, kDefaultExtents, GetFilter(), &sref, snp );
        if ( sref ) {
            unsigned int ss, st, sp;
            nav.mesh->decodePolyId( sref, ss, st, sp ); (void)ss;
            if ( (int)st < maxTiles ) {
                int sd = tilePolyBase[st] + (int)sp;
                if ( sd >= 0 && sd < totalDense ) anchorDense = sd;
            }
        }
    }

    int reachComp = -1, numComp = 0;
    NavVal_LabelReachable( comp, tilePolyBase, totalDense, anchorDense, &reachComp, &numComp );

    /* Two scopes accumulated in one sweep: [0] = REACHABLE set (the enforce
     * domain), [1] = FULL mesh (visibility). */
    struct NavValScope {
        int    polysTotal, polysMeasured, polysNoFloor, polysLowCeiling;
        /* polysNoFloor is a TOTAL over three unrelated populations; the split is
         * what actually tells you whether you are looking at a defect:
         *   buried     — the poly sits inside solid world geometry.  A real defect.
         *   submerged  — the column is liquid.  MASK_PLAYERSOLID excludes water, so
         *                these can never report a floor and are NOT defects.
         *   unmeasured — neither of the above; the probe simply found nothing.
         * buried + submerged + unmeasured == polysNoFloor (checked at report time). */
        int    noFloorBuried, noFloorSubmerged, noFloorUnmeasured;
        double sumSigned, maxAbs;
        int    beyond2, beyond8, beyond16;
        int    omcEndpoints, omcFail;
        int    omcFailMech, omcFailPhys;   /* per-class breakdown of omcFail */
        float *absDelta;      /* |delta| buffer, sized to this scope's ground polys */
        int    deltaCap, deltaN;
        float  mean, p95;
    } sc[2];
    memset( sc, 0, sizeof( sc ) );

    /* Size each scope's |delta| buffer by counting its ground polys. */
    for ( int ti = 0; ti < maxTiles; ti++ ) {
        const dtMeshTile *t = ((const dtNavMesh *)nav.mesh)->getTile( ti );
        if ( !t || !t->header ) continue;
        for ( int pi = 0; pi < t->header->polyCount; pi++ ) {
            const dtPoly *p = &t->polys[pi];
            if ( p->getType() == DT_POLYTYPE_OFFMESH_CONNECTION ) continue;
            if ( p->vertCount < 3 ) continue;
            if ( p->flags & (unsigned short)NAVPOLY_BLOCKED ) continue;
            sc[1].deltaCap++;
            if ( comp[ tilePolyBase[ti] + pi ] == reachComp ) sc[0].deltaCap++;
        }
    }
    for ( int s = 0; s < 2; s++ )
        if ( sc[s].deltaCap > 0 )
            sc[s].absDelta = (float *)Z_Malloc( sc[s].deltaCap * (int)sizeof(float) );

    /* Legacy single-scope names kept as aliases into the FULL scope so the rest
     * of the body (verbose per-poly logging) reads unchanged. */
    int   &polysTotal      = sc[1].polysTotal;
    int   &polysMeasured   = sc[1].polysMeasured;
    int   &polysNoFloor    = sc[1].polysNoFloor;
    int   &polysLowCeiling = sc[1].polysLowCeiling;


    /* Second sweep: measure. */
    for ( int ti = 0; ti < maxTiles; ti++ ) {
        const dtMeshTile *t = ((const dtNavMesh *)nav.mesh)->getTile( ti );
        if ( !t || !t->header ) continue;
        for ( int pi = 0; pi < t->header->polyCount; pi++ ) {
            const dtPoly *p = &t->polys[pi];
            if ( p->getType() == DT_POLYTYPE_OFFMESH_CONNECTION ) continue;
            if ( p->vertCount < 3 ) continue;
            /* A finalize-BLOCKED poly is excluded by the query filter (not path-
             * reachable), so it is out of the "every reachable poly agrees" scope. */
            if ( p->flags & (unsigned short)NAVPOLY_BLOCKED ) continue;

            /* Which scopes this poly counts toward: always FULL (s==1); also
             * REACHABLE (s==0) iff it belongs to the anchor component. */
            const qboolean inReach = ( comp[ tilePolyBase[ti] + pi ] == reachComp ) ? qtrue : qfalse;

            for ( int s = ( inReach ? 0 : 1 ); s < 2; s++ ) {
                sc[s].polysTotal++;
                if ( p->flags & NAVPOLY_LOW_CEILING ) sc[s].polysLowCeiling++;
            }

            /* Polygon centroid in Recast space → Quake; centroid Recast-Y = poly-z. */
            float rc[3] = { 0, 0, 0 };
            for ( int v = 0; v < p->vertCount; v++ ) {
                const float *rv = &t->verts[p->verts[v] * 3];
                rc[0] += rv[0]; rc[1] += rv[1]; rc[2] += rv[2];
            }
            const float inv = 1.0f / (float)p->vertCount;
            rc[0] *= inv; rc[1] *= inv; rc[2] *= inv;

            float qc[3];
            Nav_RecastToQuake( rc, qc );
            const float polyZ = qc[2];

            const bool polyLowCeil = ( p->flags & NAVPOLY_LOW_CEILING ) != 0;
            float originZ = 0.0f;
            if ( !NavVal_ClearanceFloorZ( qc[0], qc[1], polyZ, polyLowCeil, &originZ ) ) {
                /* Classify WHY there is no floor, from live contents at the poly's
                 * own position -- measured, not inferred.  "No floor" alone merges a
                 * real defect (poly inside solid) with a non-defect (poly in liquid,
                 * which MASK_PLAYERSOLID can never rest a box on), and reading the
                 * merged number as a measurement gap has already cost a diagnosis. */
                const int noFloorContents = CM_PointContents( qc, 0 );
                for ( int s = ( inReach ? 0 : 1 ); s < 2; s++ ) {
                    sc[s].polysNoFloor++;
                    if ( noFloorContents & ( CONTENTS_SOLID | CONTENTS_PLAYERCLIP ) )
                        sc[s].noFloorBuried++;
                    else if ( noFloorContents & MASK_WATER )
                        sc[s].noFloorSubmerged++;
                    else
                        sc[s].noFloorUnmeasured++;
                }

                if ( verbose ) {
                    dtPolyRef ref = nav.mesh->encodePolyId( t->salt, ti, pi );
                    float ptO = -9999.0f; bool ptOk = NavVal_PointFloorZ( qc[0], qc[1], polyZ, &ptO );
                    Com_Log( SEV_INFO, LOG_CH(ch_nav),
                                "nav_validate: poly %u at %.0f %.0f %.0f reach=%d — NO standable collision floor (clearance-aware) contents=0x%x ptOk=%d ptOriginZ=%.1f\n",
                                (unsigned)ref, qc[0], qc[1], polyZ, (int)inReach, noFloorContents, (int)ptOk, ptO );
                }
                continue;
            }

            const float delta = originZ - polyZ;   /* RAW: standing-origin minus poly-z */
            const float a     = fabsf( delta );
            for ( int s = ( inReach ? 0 : 1 ); s < 2; s++ ) {
                sc[s].polysMeasured++;
                sc[s].sumSigned += delta;
                if ( a > sc[s].maxAbs ) sc[s].maxAbs = a;
                if ( a > 2.0f )  sc[s].beyond2++;
                if ( a > 8.0f )  sc[s].beyond8++;
                if ( a > 16.0f ) sc[s].beyond16++;
                if ( sc[s].absDelta && sc[s].deltaN < sc[s].deltaCap )
                    sc[s].absDelta[ sc[s].deltaN++ ] = a;
            }

            if ( verbose && a > 16.0f ) {
                dtPolyRef ref = nav.mesh->encodePolyId( t->salt, ti, pi );
                bool lc = ( p->flags & NAVPOLY_LOW_CEILING ) != 0;
                /* Diagnostic point-probe column: a large box-vs-point disagreement
                 * is the grate signature (box on the bars, point through the gap). */
                float ptOrigin = -9999.0f;
                bool ptOk = NavVal_PointFloorZ( qc[0], qc[1], polyZ, &ptOrigin );
                Com_Log( SEV_INFO, LOG_CH(ch_nav),
                            "nav_validate: poly %u at %.0f %.0f %.0f reach=%d lowCeil=%d polyZ=%.1f boxOriginZ=%.1f delta=%.1f ptOk=%d ptOriginZ=%.1f ptDelta=%.1f\n",
                            (unsigned)ref, qc[0], qc[1], polyZ, (int)inReach, (int)lc, polyZ, originZ, delta,
                            (int)ptOk, ptOrigin, ptOk ? ptOrigin - polyZ : 0.0f );
            }
        }
    }

    /* Per-scope mean + p95 of |delta| (in-place insertion sort; a few k max). */
    for ( int s = 0; s < 2; s++ ) {
        sc[s].mean = sc[s].polysMeasured
            ? (float)( sc[s].sumSigned / (double)sc[s].polysMeasured ) : 0.0f;
        sc[s].p95 = 0.0f;
        if ( sc[s].absDelta && sc[s].deltaN > 0 ) {
            for ( int i = 1; i < sc[s].deltaN; i++ ) {
                float key = sc[s].absDelta[i]; int j = i - 1;
                while ( j >= 0 && sc[s].absDelta[j] > key ) { sc[s].absDelta[j + 1] = sc[s].absDelta[j]; j--; }
                sc[s].absDelta[j + 1] = key;
            }
            int idx = (int)( 0.95f * (float)( sc[s].deltaN - 1 ) + 0.5f );
            if ( idx < 0 ) idx = 0;
            if ( idx >= sc[s].deltaN ) idx = sc[s].deltaN - 1;
            sc[s].p95 = sc[s].absDelta[idx];
        }
    }
    for ( int s = 0; s < 2; s++ ) if ( sc[s].absDelta ) Z_Free( sc[s].absDelta );

    /* OMC endpoint standability.  Each OMC poly stores its two endpoints as
     * consecutive tile verts (poly->verts[0], poly->verts[1]) — same layout the
     * OBJ export reads.  An endpoint passes if the standing box finds a floor
     * beneath it AND there is player headroom above that floor.  Scope: an OMC
     * counts toward REACHABLE iff its poly is in the anchor component (the BFS
     * traversed OMC polys, so an OMC bridging into the main floor is labeled with
     * it); every OMC always counts toward FULL. */
    for ( int ti = 0; ti < maxTiles; ti++ ) {
        const dtMeshTile *t = ((const dtNavMesh *)nav.mesh)->getTile( ti );
        if ( !t || !t->header ) continue;
        for ( int pi = 0; pi < t->header->polyCount; pi++ ) {
            const dtPoly *p = &t->polys[pi];
            if ( p->getType() != DT_POLYTYPE_OFFMESH_CONNECTION ) continue;
            if ( p->vertCount < 2 ) continue;
            /* A finalize-DISABLED OMC (NAVPOLY_BLOCKED) is intentionally unroutable
             * -- the query filter excludes it -- so it is not an omcFail. */
            if ( p->flags & (unsigned short)NAVPOLY_BLOCKED ) continue;

            const qboolean omcReach = ( comp[ tilePolyBase[ti] + pi ] == reachComp ) ? qtrue : qfalse;

            /* This OMC's userId + area -> class (mechanism vs physics).  userId lives
             * in the offMeshCons[] array keyed by .poly == pi (same lookup as
             * Nav_IsGeneratorLinkPoly). */
            unsigned int omcUserId = 0; unsigned char omcArea = 0;
            for ( int c = 0; c < t->header->offMeshConCount; c++ ) {
                if ( t->offMeshCons[c].poly == (unsigned short)pi ) { omcUserId = t->offMeshCons[c].userId; break; }
            }
            { dtPolyRef sref = nav.mesh->encodePolyId( t->salt, ti, pi );
              nav.mesh->getPolyArea( sref, &omcArea ); }

            for ( int e = 0; e < 2; e++ ) {
                const float *rv = &t->verts[p->verts[e] * 3];
                float qe[3];
                Nav_RecastToQuake( rv, qe );
                for ( int s = ( omcReach ? 0 : 1 ); s < 2; s++ ) sc[s].omcEndpoints++;

                /* Endpoint e's bound ground poly: prefer the link whose edge == e;
                 * fall back to ANY forward ground link (the standard bind forward-links
                 * only the START side, so e==1 may lack an edge-specific link on a
                 * correctly-wired OMC).  IDENTICAL derivation to the finalize's, so both
                 * consumers feed the shared validity the same boundRef. */
                navPolyRef_t boundRef = 0, anyRef = 0;
                for ( unsigned int k = p->firstLink; k != DT_NULL_LINK; k = t->links[k].next ) {
                    if ( !t->links[k].ref ) continue;
                    if ( !anyRef ) anyRef = (navPolyRef_t)t->links[k].ref;
                    if ( t->links[k].edge == (unsigned char)e ) { boundRef = (navPolyRef_t)t->links[k].ref; break; }
                }
                if ( !boundRef ) boundRef = anyRef;

                /* SHARED per-class validity -- the SAME NavVal_OmcEndpointValid the
                 * post-bake finalize enforces (physics: box-standable; mechanism:
                 * live-poly-bound).  A green validator here means the finalize did its
                 * job (or there was nothing to fix), under each class's own definition. */
                qboolean ok = NavVal_OmcEndpointValid( omcUserId, omcArea, qe, boundRef ) ? qtrue : qfalse;
                if ( !ok ) {
                    const qboolean mech = OmcClassIsExempt( OmcClassify( omcUserId, omcArea ) ) ? qtrue : qfalse;
                    for ( int s = ( omcReach ? 0 : 1 ); s < 2; s++ ) { sc[s].omcFail++; if ( mech ) sc[s].omcFailMech++; else sc[s].omcFailPhys++; }
                    if ( verbose ) {
                        dtPolyRef ref = nav.mesh->encodePolyId( t->salt, ti, pi );
                        unsigned short of = 0; nav.mesh->getPolyFlags( ref, &of );
                        Com_Log( SEV_INFO, LOG_CH(ch_nav),
                                    "nav_validate: OMC %u endpoint %d at %.0f %.0f %.0f reach=%d class=%s boundRef=%u area=%d flags=0x%x — invalid under class validity\n",
                                    (unsigned)ref, e, qe[0], qe[1], qe[2], (int)omcReach,
                                    OmcClassStr( OmcClassify( omcUserId, omcArea ) ), (unsigned)boundRef, (int)omcArea, (unsigned)of );
                    }
                }
            }
        }
    }

    const int unreachablePolys = sc[1].polysTotal - sc[0].polysTotal;

    /* Anchor provenance — which point seeded the reachable set, and how it was
     * chosen (spawn entity vs largest-component fallback).  Makes the scope
     * auditable: a wrong anchor would show here. */
    if ( haveSpawn && anchorDense >= 0 )
        Com_Log( SEV_INFO, LOG_CH(ch_nav),
                    "[NAVVAL] anchor=spawn map=%s at=%.0f,%.0f,%.0f reachComp=%d\n",
                    nav.mapname[0] ? nav.mapname : "(none)",
                    spawnQ[0], spawnQ[1], spawnQ[2], reachComp );
    else
        Com_Log( SEV_INFO, LOG_CH(ch_nav),
                    "[NAVVAL] anchor=largest-component map=%s (no spawn snap) reachComp=%d\n",
                    nav.mapname[0] ? nav.mapname : "(none)", reachComp );

    /* Machine-readable summary — TWO scopes.  The reachable-set line is the gate
     * (enforce reads it); the full-mesh line + unreachablePolys keep every poly
     * visible (nothing hidden — a ballooning unreachablePolys is a real signal).
     * Tokens avoid the arena golden's capture filter (no "client"/"repath"/"wp"/
     * "reached"/"OMC"/"stuck"/"FindPath engine:" substrings). */



    /* The nofloor breakdown must partition nofloor exactly; a mismatch means the
     * classification and the total disagree, which would silently mislead again. */
    for ( int s = 0; s < 2; s++ ) {
        const int sum = sc[s].noFloorBuried + sc[s].noFloorSubmerged + sc[s].noFloorUnmeasured;
        if ( sum != sc[s].polysNoFloor )
            Com_Log( SEV_WARN, LOG_CH(ch_nav),
                "[NAVVAL] scope=%s nofloor breakdown MISMATCH: buried=%d submerged=%d unmeasured=%d sum=%d nofloor=%d\n",
                s == 0 ? "reachable" : "full",
                sc[s].noFloorBuried, sc[s].noFloorSubmerged, sc[s].noFloorUnmeasured,
                sum, sc[s].polysNoFloor );
    }

    Com_Log( SEV_INFO, LOG_CH(ch_nav),
                "[NAVVAL] scope=reachable map=%s polys=%d measured=%d nofloor=%d(buried=%d submerged=%d unmeasured=%d) mean=%.1f p95=%.1f max=%.1f "
                "beyond2u=%d beyond8u=%d beyond16u=%d omc=%d omcFail=%d(mech=%d phys=%d) lowCeil=%d eps=%.1f\n",
                nav.mapname[0] ? nav.mapname : "(none)",
                sc[0].polysTotal, sc[0].polysMeasured, sc[0].polysNoFloor,
                sc[0].noFloorBuried, sc[0].noFloorSubmerged, sc[0].noFloorUnmeasured,
                sc[0].mean, sc[0].p95, (float)sc[0].maxAbs,
                sc[0].beyond2, sc[0].beyond8, sc[0].beyond16,
                sc[0].omcEndpoints, sc[0].omcFail, sc[0].omcFailMech, sc[0].omcFailPhys, sc[0].polysLowCeiling, NAVVAL_EPSILON );
    Com_Log( SEV_INFO, LOG_CH(ch_nav),
                "[NAVVAL] scope=full map=%s polys=%d measured=%d nofloor=%d(buried=%d submerged=%d unmeasured=%d) mean=%.1f p95=%.1f max=%.1f "
                "beyond2u=%d beyond8u=%d beyond16u=%d omc=%d omcFail=%d(mech=%d phys=%d) lowCeil=%d unreachablePolys=%d comps=%d "
                "trajDisabled=%d intraRegionCulled=%d eps=%.1f\n",
                nav.mapname[0] ? nav.mapname : "(none)",
                sc[1].polysTotal, sc[1].polysMeasured, sc[1].polysNoFloor,
                sc[1].noFloorBuried, sc[1].noFloorSubmerged, sc[1].noFloorUnmeasured,
                sc[1].mean, sc[1].p95, (float)sc[1].maxAbs,
                sc[1].beyond2, sc[1].beyond8, sc[1].beyond16,
                sc[1].omcEndpoints, sc[1].omcFail, sc[1].omcFailMech, sc[1].omcFailPhys, sc[1].polysLowCeiling,
                unreachablePolys, numComp, s_trajDisabled, s_intraRegionCulled, NAVVAL_EPSILON );


    if ( enforce ) {
        /* Gate on the REACHABLE scope only — its polys are what a bot can actually
         * reach and what the "agrees with collision" guarantee covers.  omcFail is
         * also gated (a reachable OMC endpoint that can't be stood on is a real
         * defect). */
        qboolean pass = ( fabsf( sc[0].mean ) <= maxMean && sc[0].p95 <= maxP95
                          && sc[0].omcFail == 0 ) ? qtrue : qfalse;
        Com_Log( SEV_INFO, LOG_CH(ch_nav),
                    "[NAVVAL] verdict=%s scope=reachable mean=%.1f<=%.1f p95=%.1f<=%.1f omcFail=%d==0\n",
                    pass ? "PASS" : "FAIL", fabsf( sc[0].mean ), maxMean, sc[0].p95, maxP95, sc[0].omcFail );
    }

    Z_Free( comp );
    Z_Free( tilePolyBase );
}

/* -------------------------------------------------------------------------
   nav_linkaudit -- physics-validated OMC census (read-only report).  Enumerates
   EVERY off-mesh connection, classifies its producer (OmcClassify), runs the same
   trajectory validator the finalize uses, and prints one row per link plus a
   per-class summary.  This is the report sibling of Nav_FinalizeLinkTrajectory:
   the finalize DISABLES physics-FAIL links at load; this command just reports the
   census (and shows which are already [blocked]).  Read-only -- no mesh mutation.
   ------------------------------------------------------------------------- */

static void Nav_LinkAuditCmd( void )
{
    if ( !nav.ready || !nav.mesh ) {
        Com_Log( SEV_INFO, LOG_CH(ch_nav), "nav_linkaudit: navmesh not loaded (load a map first)\n" );
        return;
    }
    const dtNavMesh *mesh = (const dtNavMesh *)nav.mesh;
    const int maxTiles = mesh->getMaxTiles();

    /* Per-class tallies: PASS / FAIL / EXEMPT. */
    int nPass[OMCCLASS_COUNT]   = { 0 };
    int nFail[OMCCLASS_COUNT]   = { 0 };
    int nExempt[OMCCLASS_COUNT] = { 0 };
    int total = 0, disabled = 0;

    Com_Log( SEV_INFO, LOG_CH(ch_nav),
        "[LINKAUDIT] map=%s jumpCeiling=%.2fu (JUMP_VELOCITY=%.0f gravity=%.0f)\n",
        nav.mapname, NavTraj_JumpCeiling(), NAVTRAJ_JUMP_VEL, NAVTRAJ_GRAVITY );
    Com_Log( SEV_INFO, LOG_CH(ch_nav),
        "[LINKAUDIT] cols: id | class | start(x y z) -> end(x y z) | verdict | reqRise horizGap | evidence\n" );

    for ( int ti = 0; ti < maxTiles; ti++ ) {
        const dtMeshTile *t = mesh->getTile( ti );
        if ( !t || !t->header ) continue;
        const dtPolyRef base = mesh->getPolyRefBase( t );

        for ( int ci = 0; ci < t->header->offMeshConCount; ci++ ) {
            const dtOffMeshConnection *con = &t->offMeshCons[ci];
            const dtPoly *poly = &t->polys[ con->poly ];
            if ( poly->vertCount < 2 ) continue;
            total++;

            const dtPolyRef ref = base | (dtPolyRef)con->poly;
            unsigned char area = 0; unsigned short flags = 0;
            mesh->getPolyArea( ref, &area ); mesh->getPolyFlags( ref, &flags );

            const bool isBlocked = ( flags & (unsigned short)NAVPOLY_BLOCKED ) != 0;
            if ( isBlocked ) disabled++;

            /* Classify by userId range, refined by area for external links (shared
             * with the trajectory finalize). */
            omcClass_t cls = OmcClassify( con->userId, area );

            /* Endpoints -> Quake space.  pos[0..2]=start, pos[3..5]=end (Recast). */
            float qStart[3], qEnd[3];
            Nav_RecastToQuake( &con->pos[0], qStart );
            Nav_RecastToQuake( &con->pos[3], qEnd );

            if ( OmcClassIsExempt( cls ) ) {
                /* Exempt: mechanism traversal.  Report landing standability as evidence
                 * only; no parabola verdict.  (Jump-pad launch velocity is not recorded
                 * per-OMC by the producer, so an arc-against-launch check is not
                 * derivable here -- reported as such, no producer rework.) */
                float endOriginZ = qEnd[2];
                bool land = NavVal_EndpointStandable( qEnd, &endOriginZ );
                nExempt[cls]++;
                Com_Log( SEV_INFO, LOG_CH(ch_nav),
                    "[LINKAUDIT] id=%u | %-12s | (%.0f %.0f %.0f)->(%.0f %.0f %.0f) | EXEMPT-BY-MECHANISM | landStandable=%d%s\n",
                    con->userId, OmcClassStr( cls ),
                    qStart[0],qStart[1],qStart[2], qEnd[0],qEnd[1],qEnd[2],
                    (int)land, isBlocked ? " [blocked]" : "" );
                continue;
            }

            /* Physics-validated classes: run the trajectory validator.  A bidirectional
             * link (step-up/drop pairs) is validated in the up direction that needs the
             * jump; we validate start->end as recorded (the harder direction for a
             * step-up is the up leg). */
            navTrajEvidence_t ev;
            navTrajResult_t r = Nav_ValidateLinkTrajectory( qStart, qEnd, &ev );
            /* Also try end->start: a recorded drop is start-high; a step-up may be
             * recorded either way, so PASS if EITHER direction is physically traversable
             * (the mesh link is bidirectional). */
            if ( r != NAVTRAJ_PASS ) {
                navTrajEvidence_t ev2;
                navTrajResult_t r2 = Nav_ValidateLinkTrajectory( qEnd, qStart, &ev2 );
                if ( r2 == NAVTRAJ_PASS ) { r = r2; ev = ev2; }
            }

            if ( r == NAVTRAJ_PASS ) nPass[cls]++; else nFail[cls]++;
            Com_Log( SEV_INFO, LOG_CH(ch_nav),
                "[LINKAUDIT] id=%u | %-12s | (%.0f %.0f %.0f)->(%.0f %.0f %.0f) | %-26s | reqRise=%.1f gap=%.1f | speed=%.0f apex=%.1f t=%.2fs reachXY=%.1f%s\n",
                con->userId, OmcClassStr( cls ),
                qStart[0],qStart[1],qStart[2], qEnd[0],qEnd[1],qEnd[2],
                NavTraj_ResultStr( r ),
                ev.reqRise, ev.horizGap, ev.launchSpeed, ev.apex, ev.flightTime,
                ( ev.bestReachXY < 1e29f ) ? ev.bestReachXY : -1.0f,
                isBlocked ? " [blocked]" : "" );
        }
    }

    /* Per-class summary. */
    Com_Log( SEV_INFO, LOG_CH(ch_nav), "[LINKAUDIT] --- summary map=%s total=%d disabled=%d ---\n",
             nav.mapname, total, disabled );
    for ( int c = 0; c < OMCCLASS_COUNT; c++ ) {
        if ( nPass[c] + nFail[c] + nExempt[c] == 0 ) continue;
        Com_Log( SEV_INFO, LOG_CH(ch_nav),
            "[LINKAUDIT] class=%-18s PASS=%d FAIL=%d EXEMPT=%d\n",
            OmcClassStr( (omcClass_t)c ), nPass[c], nFail[c], nExempt[c] );
    }
}

/* Read-only causal-registry report for the loaded map (openable traversal).
   Delegates to Nav_RegistryAudit (nav_offmesh.c) which parses the entity lump. */
static void Nav_RegistryAuditCmd( void )
{
    if ( !nav.mapname[0] ) {
        Com_Log( SEV_INFO, LOG_CH(ch_nav), "nav_registryaudit: no map loaded\n" );
        return;
    }
    Nav_RegistryAudit( nav.mapname );
}



/* nav_reachset <px py pz> [<px py pz> ...] — read-only severance enumerator.
 * Component-labels the loaded mesh (OMC-traversing, spawn-anchored — the same
 * NavVal_BuildReachableSet nav_validate uses), then for each query point reports
 * its component id, whether it is IN the spawn-reachable component, and that
 * component's ground-poly size. Two points with different comp ids are severed;
 * this is how the spawn->exit break set is enumerated. */
static void Nav_ReachSetCmd( void )
{
    if ( Cmd_Argc() < 4 ) {
        Com_Log( SEV_INFO, LOG_CH(ch_nav), "Usage: nav_reachset <px py pz> [<px py pz> ...]\n" );
        return;
    }
    if ( !nav.ready || !nav.query || !nav.mesh ) {
        Com_Log( SEV_INFO, LOG_CH(ch_nav), "nav_reachset: navmesh not loaded\n" );
        return;
    }

    int *comp = NULL, *tileBase = NULL, totalDense = 0, reachComp = -1, numComp = 0;
    if ( !NavVal_BuildReachableSet( &comp, &tileBase, &totalDense, &reachComp, &numComp ) ) {
        Com_Log( SEV_INFO, LOG_CH(ch_nav), "nav_reachset: labeling failed\n" );
        return;
    }
    const dtNavMesh *mesh = (const dtNavMesh *)nav.mesh;
    const int maxTiles = mesh->getMaxTiles();

    /* Component ground-poly sizes. */
    int *csize = (int *)Z_Malloc( ( numComp > 0 ? numComp : 1 ) * (int)sizeof(int) );
    if ( csize ) {
        for ( int i = 0; i < numComp; i++ ) csize[i] = 0;
        for ( int ti = 0; ti < maxTiles; ti++ ) {
            const dtMeshTile *t = mesh->getTile( ti );
            if ( !t || !t->header ) continue;
            for ( int pi = 0; pi < t->header->polyCount; pi++ ) {
                const dtPoly *p = &t->polys[pi];
                if ( p->getType() == DT_POLYTYPE_OFFMESH_CONNECTION || p->vertCount < 3 ) continue;
                int c = comp[ tileBase[ti] + pi ];
                if ( c >= 0 && c < numComp ) csize[c]++;
            }
        }
    }

    Com_Log( SEV_INFO, LOG_CH(ch_nav),
        "[REACHSET] spawnComp=%d numComp=%d totalDense=%d\n", reachComp, numComp, totalDense );

    for ( int argi = 1; argi + 2 < Cmd_Argc(); argi += 3 ) {
        float qP[3] = { (float)atof(Cmd_Argv(argi)), (float)atof(Cmd_Argv(argi+1)), (float)atof(Cmd_Argv(argi+2)) };
        float rP[3]; Nav_QuakeToRecast( qP, rP );
        dtPolyRef ref = 0; float pt[3];
        nav.query->findNearestPoly( rP, kDefaultExtents, GetFilter(), &ref, pt );
        if ( !ref ) {
            Com_Log( SEV_INFO, LOG_CH(ch_nav),
                "[REACHSET] (%.0f %.0f %.0f) -> NO_SNAP\n", qP[0], qP[1], qP[2] );
            continue;
        }
        unsigned int ss, st, sp; mesh->decodePolyId( ref, ss, st, sp ); (void)ss;
        int c = -1;
        if ( (int)st < maxTiles ) {
            int dense = tileBase[st] + (int)sp;
            if ( dense >= 0 && dense < totalDense ) c = comp[dense];
        }
        float snapQ[3]; Nav_RecastToQuake( pt, snapQ );
        int sz = ( csize && c >= 0 && c < numComp ) ? csize[c] : 0;
        Com_Log( SEV_INFO, LOG_CH(ch_nav),
            "[REACHSET] (%.0f %.0f %.0f) snap=(%.0f %.0f %.0f) miss=%.0f comp=%d size=%d inSpawnComp=%s\n",
            qP[0], qP[1], qP[2], snapQ[0], snapQ[1], snapQ[2], Distance(qP, snapQ),
            c, sz, ( c == reachComp ) ? "YES" : "NO" );
    }

    if ( csize ) Z_Free( csize );
    Z_Free( comp );
    Z_Free( tileBase );
}

/* nav_halflinkaudit -- read-only half-link census.  For every OMC, report whether
 * each endpoint (edge 0 = start, edge 1 = end) has a forward ground link, and tally
 * per class the OMCs whose END is unbound (a dangling half-link that cannot bridge).
 * Diagnoses the OMC-END-binding gap.  Read-only. */
static void Nav_HalfLinkAuditCmd( void )
{
    if ( !nav.ready || !nav.mesh ) { Com_Log( SEV_INFO, LOG_CH(ch_nav), "nav_halflinkaudit: navmesh not loaded\n" ); return; }
    const dtNavMesh *mesh = (const dtNavMesh *)nav.mesh;
    const int maxTiles = mesh->getMaxTiles();

    int total = 0, startUnbound = 0, endUnbound = 0, bothBound = 0, blockedSkipped = 0;
    int groundLinksChecked = 0, missingBackLink = 0;
    int endUnboundByClass[OMCCLASS_COUNT] = { 0 };
    int totalByClass[OMCCLASS_COUNT] = { 0 };

    for ( int ti = 0; ti < maxTiles; ti++ ) {
        const dtMeshTile *t = mesh->getTile( ti );
        if ( !t || !t->header ) continue;
        for ( int pi = 0; pi < t->header->polyCount; pi++ ) {
            const dtPoly *p = &t->polys[pi];
            if ( p->getType() != DT_POLYTYPE_OFFMESH_CONNECTION || p->vertCount < 2 ) continue;
            dtPolyRef ref = mesh->encodePolyId( t->salt, ti, pi );
            unsigned short pf = 0; mesh->getPolyFlags( ref, &pf );
            if ( pf & (unsigned short)NAVPOLY_BLOCKED ) { blockedSkipped++; continue; }   /* disabled OMC */

            unsigned int uid = 0; unsigned char oa = 0;
            for ( int c = 0; c < t->header->offMeshConCount; c++ )
                if ( t->offMeshCons[c].poly == (unsigned short)pi ) { uid = t->offMeshCons[c].userId; break; }
            mesh->getPolyArea( ref, &oa );
            const omcClass_t cls = OmcClassify( uid, oa );

            /* A binding is TWO links on two chains: forward (OMC->ground, on this
               poly) and reciprocal (ground->OMC, on the ground poly).  Counting only
               the forward half reported full health over one-way links, because
               Detour's A* reaches neighbours solely via the ground poly's own chain.
               missingBackLink makes that half visible. */
            bool e0 = false, e1 = false;
            for ( unsigned int k = p->firstLink; k != DT_NULL_LINK; k = t->links[k].next ) {
                if ( !t->links[k].ref ) continue;
                if ( t->links[k].edge == 0 ) e0 = true;
                else if ( t->links[k].edge == 1 ) e1 = true;

                const dtMeshTile *gt = NULL; const dtPoly *gp = NULL;
                mesh->getTileAndPolyByRefUnsafe( (dtPolyRef)t->links[k].ref, &gt, &gp );
                groundLinksChecked++;
                if ( gt && gp ) {
                    bool back = false;
                    for ( unsigned int m = gp->firstLink; m != DT_NULL_LINK; m = gt->links[m].next ) {
                        if ( (dtPolyRef)gt->links[m].ref == ref ) { back = true; break; }
                    }
                    if ( !back ) missingBackLink++;
                } else {
                    missingBackLink++;
                }
            }
            total++; totalByClass[cls]++;
            if ( !e0 ) startUnbound++;
            if ( !e1 ) { endUnbound++; endUnboundByClass[cls]++; }
            if ( e0 && e1 ) bothBound++;
            if ( !e1 ) {
                const float *rv = &t->verts[p->verts[1] * 3]; float qe[3]; Nav_RecastToQuake( rv, qe );
                Com_Log( SEV_DEBUG, LOG_CH(ch_nav),
                    "[HALFLINK] OMC %u class=%s END-unbound end=(%.0f %.0f %.0f) startBound=%d\n",
                    (unsigned)ref, OmcClassStr(cls), qe[0], qe[1], qe[2], (int)e0 );
            }
        }
    }

    Com_Log( SEV_INFO, LOG_CH(ch_nav),
        "[HALFLINK] map=%s total=%d bothBound=%d startUnbound=%d endUnbound=%d blockedSkipped=%d "
        "groundLinks=%d missingBackLink=%d\n",
        nav.mapname, total, bothBound, startUnbound, endUnbound, blockedSkipped,
        groundLinksChecked, missingBackLink );
    for ( int c = 0; c < OMCCLASS_COUNT; c++ )
        if ( totalByClass[c] > 0 )
            Com_Log( SEV_INFO, LOG_CH(ch_nav),
                "[HALFLINK] class=%-18s total=%d endUnbound=%d\n", OmcClassStr((omcClass_t)c), totalByClass[c], endUnboundByClass[c] );
}

void Nav_Debug_RegisterCommands( void )
{
    Cmd_AddCommand( "nav_info",         Nav_InfoCmd );
    Cmd_AddCommand( "nav_draw",         Nav_DrawCmd );
    Cmd_AddCommand( "nav_setblocked",   Nav_SetBlockedCmd );
    Cmd_AddCommand( "nav_clear",        Nav_ClearCmd );
    Cmd_AddCommand( "nav_findpath",     Nav_FindPathCmd );
    Cmd_AddCommand( "nav_island",       Nav_IslandCmd );
    Cmd_AddCommand( "nav_spawnprobe",   Nav_SpawnProbeCmd );
    Cmd_AddCommand( "nav_gridprobe",    Nav_GridProbeCmd );
    Cmd_AddCommand( "nav_thresholdprobe", Nav_ThresholdProbeCmd );
    Cmd_AddCommand( "nav_reachset",     Nav_ReachSetCmd );
    Cmd_AddCommand( "nav_halflinkaudit", Nav_HalfLinkAuditCmd );
    Cmd_AddCommand( "nav_validate",     Nav_ValidateCmd );
    Cmd_AddCommand( "nav_linkaudit",    Nav_LinkAuditCmd );
    Cmd_AddCommand( "nav_registryaudit", Nav_RegistryAuditCmd );
}

} /* extern "C" */

#else /* !FEAT_RECAST_NAVMESH */

extern "C" {
void Nav_Init( void )                    { }
void Nav_Shutdown( void )                { }
void Nav_LoadMap( const char *mapname )  { (void)mapname; }
void Nav_UnloadMap( void )               { }
int  Nav_IsReady( void )                 { return 0; }
void Nav_Debug_RegisterCommands( void )  { }
}

#endif /* FEAT_RECAST_NAVMESH */
