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
}

#if FEAT_RECAST_NAVMESH

#include "nav_local.h"
#include "nav_coord.h"

/* -------------------------------------------------------------------------
   Module globals
   navGlobals_t is intentionally NOT in any public header.  Only this file
   accesses it.  External code uses the Nav_Impl_* accessors below.
   ------------------------------------------------------------------------- */

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
} navGlobals_t;

static navGlobals_t nav;

/* -------------------------------------------------------------------------
   Build parameters (Phase 1 §11, calibrated for q3now 320–700 ups physics)
   ------------------------------------------------------------------------- */

static const float NAV_CS  = 2.0f;
static const float NAV_CH  = 5.0f;
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
static const float NAV_DETAIL_SAMPLE_DIST   = 3.0f * 2.0f;  /* 3 × NAV_CS */
static const float NAV_DETAIL_SAMPLE_MAX_ERR = 1.0f * 5.0f; /* 1 × NAV_CH */

/* -------------------------------------------------------------------------
   rcContext — routes Recast log messages to the engine console
   ------------------------------------------------------------------------- */

class NavContext : public rcContext {
protected:
    void doLog( const rcLogCategory cat, const char *msg, const int ) override {
        if ( cat == RC_LOG_ERROR || cat == RC_LOG_WARNING )
            Com_Log( SEV_INFO, LOG_CAT_NAV, "NAV: [RC] %s\n", msg );
        else
            Com_Log( SEV_DEBUG, LOG_CAT_NAV, "NAV: [RC] %s\n", msg );
    }
};

/* Forward declarations for static helpers used by Nav_Build_Internal before
 * they are defined later in this file. */
static const dtQueryFilter *GetFilter( void );
static void Nav_TagDoorAreas( const char *mapname );

/* -------------------------------------------------------------------------
   Off-mesh connection layout arrays (used in dtNavMeshCreateParams)
   External OMCs come from nav_offmesh.c entity parsing (max NAV_MAX_OMC).
   Internal drop OMCs are detected post-polymesh (max NAV_MAX_INT_OMC).
   Total capacity = NAV_MAX_TOTAL_OMC.  Stack allocation in a non-recursive
   function — ~20 KB, well within frame stack limits.
   ------------------------------------------------------------------------- */

#define NAV_MAX_INT_OMC    512
#define NAV_MAX_TOTAL_OMC  (NAV_MAX_OMC + NAV_MAX_INT_OMC)
#define MAX_OMC_FLAT       (NAV_MAX_TOTAL_OMC * 2 * 3)

struct OmcArrays {
    float          verts[MAX_OMC_FLAT];
    float          rads[NAV_MAX_TOTAL_OMC];
    unsigned short flags[NAV_MAX_TOTAL_OMC];
    unsigned char  areas[NAV_MAX_TOTAL_OMC];
    unsigned char  dirs[NAV_MAX_TOTAL_OMC];
    unsigned int   userIds[NAV_MAX_TOTAL_OMC];
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
        out->count++;
    }
}

/* -------------------------------------------------------------------------
   Nav_Build_Internal — file-static; not exported
   Runs the Recast → Detour pipeline including:
     - workAreas buffer to preserve WATER/LAVA/custom area types through
       rcMarkWalkableTriangles (Decision 1, Option A, approved 2026-04-21).
     - crouchH (8 vox) used for filters + compact HF so low-clearance spans
       survive; tagged NAVAREA_LOW_CEILING in the compact HF scan.
     - Drop OMC detection (Option C edge-based): adjacent poly pairs compared
       by center Y in voxel coords; threshold 8 vox = 36 Q3u = 2×STEPSIZE.
   Returns a heap-allocated dtNavMesh on success, NULL on any error.
   ------------------------------------------------------------------------- */

static dtNavMesh *Nav_Build_Internal( const navGeom_t *geom, const navOmcInput_t *omc )
{
    if ( !geom || geom->numVerts == 0 || geom->numTris == 0 ) {
        Com_Log( SEV_INFO, LOG_CAT_NAV, "NAV: Nav_Build_Internal called with empty geometry\n" );
        return NULL;
    }

    NavContext ctx;

    /* Voxel counts derived from physical constants */
    const int walkH   = (int)ceilf( NAV_WALKABLE_HEIGHT / NAV_CH ); /* 12 vox */
    const int crouchH = (int)ceilf( NAV_CROUCH_HEIGHT   / NAV_CH ); /* 8 vox  */
    const int walkC   = (int)floorf( NAV_WALKABLE_CLIMB  / NAV_CH );

    /* 1. Bounds & grid */
    float bmin[3], bmax[3];
    rcCalcBounds( geom->verts, geom->numVerts, bmin, bmax );
    int gw = 0, gh = 0;
    rcCalcGridSize( bmin, bmax, NAV_CS, &gw, &gh );

    /* 2. Heightfield */
    rcHeightfield *hf = rcAllocHeightfield();
    if ( !hf ) { Com_Log( SEV_INFO, LOG_CAT_NAV, "NAV: rcAllocHeightfield failed\n" ); return NULL; }

    if ( !rcCreateHeightfield( &ctx, *hf, gw, gh, bmin, bmax, NAV_CS, NAV_CH ) ) {
        Com_Log( SEV_INFO, LOG_CAT_NAV, "NAV: rcCreateHeightfield failed\n" );
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
        Com_Log( SEV_INFO, LOG_CAT_NAV, "NAV: Z_Malloc workAreas failed\n" );
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
        Com_Log( SEV_INFO, LOG_CAT_NAV, "NAV: rcRasterizeTriangles failed\n" );
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
        Com_Log( SEV_INFO, LOG_CAT_NAV, "NAV: rcAllocCompactHeightfield failed\n" );
        rcFreeHeightField( hf ); return NULL;
    }
    if ( !rcBuildCompactHeightfield( &ctx, crouchH, walkC, *hf, *chf ) ) {
        Com_Log( SEV_INFO, LOG_CAT_NAV, "NAV: rcBuildCompactHeightfield failed\n" );
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

    /* 4. Erosion */
    const int walkR = (int)floorf( NAV_WALKABLE_RADIUS / NAV_CS );
    if ( !rcErodeWalkableArea( &ctx, walkR, *chf ) ) {
        Com_Log( SEV_INFO, LOG_CAT_NAV, "NAV: rcErodeWalkableArea failed\n" );
        rcFreeCompactHeightfield( chf ); return NULL;
    }

    /* 5. Regions */
    if ( !rcBuildDistanceField( &ctx, *chf ) ) {
        Com_Log( SEV_INFO, LOG_CAT_NAV, "NAV: rcBuildDistanceField failed\n" );
        rcFreeCompactHeightfield( chf ); return NULL;
    }
    if ( !rcBuildRegions( &ctx, *chf, 0, NAV_MIN_REGION_AREA, NAV_MERGE_REGION_AREA ) ) {
        Com_Log( SEV_INFO, LOG_CAT_NAV, "NAV: rcBuildRegions failed\n" );
        rcFreeCompactHeightfield( chf ); return NULL;
    }

    /* 6. Contours → poly mesh */
    rcContourSet *cset = rcAllocContourSet();
    if ( !cset ) { Com_Log( SEV_INFO, LOG_CAT_NAV, "NAV: rcAllocContourSet failed\n" ); rcFreeCompactHeightfield( chf ); return NULL; }
    if ( !rcBuildContours( &ctx, *chf, NAV_MAX_SIMPLIFICATION_ERR, (int)NAV_MAX_EDGE_LEN, *cset ) ) {
        Com_Log( SEV_INFO, LOG_CAT_NAV, "NAV: rcBuildContours failed\n" );
        rcFreeCompactHeightfield( chf ); rcFreeContourSet( cset ); return NULL;
    }

    rcPolyMesh *pmesh = rcAllocPolyMesh();
    if ( !pmesh ) { Com_Log( SEV_INFO, LOG_CAT_NAV, "NAV: rcAllocPolyMesh failed\n" ); rcFreeCompactHeightfield( chf ); rcFreeContourSet( cset ); return NULL; }
    if ( !rcBuildPolyMesh( &ctx, *cset, NAV_MAX_VERTS_PER_POLY, *pmesh ) ) {
        Com_Log( SEV_INFO, LOG_CAT_NAV, "NAV: rcBuildPolyMesh failed\n" );
        rcFreeCompactHeightfield( chf ); rcFreeContourSet( cset ); rcFreePolyMesh( pmesh ); return NULL;
    }
    rcFreeContourSet( cset ); cset = NULL;

    /* Build OmcArrays from external OMCs now, before drop detection, so
     * internal drops can be appended directly. */
    OmcArrays omcArrs;
    memset( &omcArrs, 0, sizeof(omcArrs) );
    if ( omc ) buildOmcArrays( omc, &omcArrs );

    /* Drop OMC detection (Part F3, Option C — edge-based adjacency).
     * Only adjacent (connected) poly pairs are compared.  Height difference
     * is the average Y of all poly vertices in integer voxel coordinates.
     * This gives correct results even for non-convex polygons.
     * Threshold: 8 vox = 36 Q3u = 2 × STEPSIZE.  One-way drop from higher
     * poly center to lower poly center, both converted to Recast float space. */
    {
        static const int DROP_THRESH_VOX = 8;
        const int nvp = pmesh->nvp;
        const int numExtOmcs = omcArrs.count;

        for ( int pi = 0; pi < pmesh->npolys && omcArrs.count < NAV_MAX_TOTAL_OMC; pi++ ) {
            int nvi = 0;
            for ( int v = 0; v < nvp; v++ ) {
                if ( pmesh->polys[pi * nvp * 2 + v] == RC_MESH_NULL_IDX ) break;
                nvi++;
            }
            if ( nvi < 3 ) continue;

            int sumX_i = 0, sumY_i = 0, sumZ_i = 0;
            for ( int v = 0; v < nvi; v++ ) {
                unsigned short vi = pmesh->polys[pi * nvp * 2 + v];
                sumX_i += (int)pmesh->verts[vi * 3 + 0];
                sumY_i += (int)pmesh->verts[vi * 3 + 1];
                sumZ_i += (int)pmesh->verts[vi * 3 + 2];
            }

            for ( int e = 0; e < nvi && omcArrs.count < NAV_MAX_TOTAL_OMC; e++ ) {
                unsigned short nei = pmesh->polys[pi * nvp * 2 + nvp + e];
                if ( nei == RC_MESH_NULL_IDX ) continue;
                if ( (int)nei < pi ) continue; /* process each pair once */

                int nvn = 0;
                for ( int v = 0; v < nvp; v++ ) {
                    if ( pmesh->polys[(int)nei * nvp * 2 + v] == RC_MESH_NULL_IDX ) break;
                    nvn++;
                }
                if ( nvn < 3 ) continue;

                int sumX_n = 0, sumY_n = 0, sumZ_n = 0;
                for ( int v = 0; v < nvn; v++ ) {
                    unsigned short vn = pmesh->polys[(int)nei * nvp * 2 + v];
                    sumX_n += (int)pmesh->verts[vn * 3 + 0];
                    sumY_n += (int)pmesh->verts[vn * 3 + 1];
                    sumZ_n += (int)pmesh->verts[vn * 3 + 2];
                }

                int diff = sumY_i / nvi - sumY_n / nvn;
                if ( diff <= DROP_THRESH_VOX && -diff <= DROP_THRESH_VOX ) continue;

                float hiCx, hiCy, hiCz, loCx, loCy, loCz;
                if ( diff > 0 ) {
                    /* pi is higher, drop from pi to nei */
                    hiCx = bmin[0] + ((float)sumX_i / nvi + 0.5f) * NAV_CS;
                    hiCy = bmin[1] + ((float)sumY_i / nvi) * NAV_CH;
                    hiCz = bmin[2] + ((float)sumZ_i / nvi + 0.5f) * NAV_CS;
                    loCx = bmin[0] + ((float)sumX_n / nvn + 0.5f) * NAV_CS;
                    loCy = bmin[1] + ((float)sumY_n / nvn) * NAV_CH;
                    loCz = bmin[2] + ((float)sumZ_n / nvn + 0.5f) * NAV_CS;
                } else {
                    /* nei is higher, drop from nei to pi */
                    hiCx = bmin[0] + ((float)sumX_n / nvn + 0.5f) * NAV_CS;
                    hiCy = bmin[1] + ((float)sumY_n / nvn) * NAV_CH;
                    hiCz = bmin[2] + ((float)sumZ_n / nvn + 0.5f) * NAV_CS;
                    loCx = bmin[0] + ((float)sumX_i / nvi + 0.5f) * NAV_CS;
                    loCy = bmin[1] + ((float)sumY_i / nvi) * NAV_CH;
                    loCz = bmin[2] + ((float)sumZ_i / nvi + 0.5f) * NAV_CS;
                }

                int idx  = omcArrs.count;
                int base = idx * 6;
                omcArrs.verts[base+0] = hiCx; omcArrs.verts[base+1] = hiCy; omcArrs.verts[base+2] = hiCz;
                omcArrs.verts[base+3] = loCx; omcArrs.verts[base+4] = loCy; omcArrs.verts[base+5] = loCz;
                omcArrs.rads[idx]    = 32.0f;
                omcArrs.flags[idx]   = (unsigned short)(NAVPOLY_WALKABLE | NAVPOLY_OFFMESH);
                omcArrs.areas[idx]   = (unsigned char)NAVAREA_JUMP_LINK;
                omcArrs.dirs[idx]    = 0;
                omcArrs.userIds[idx] = (unsigned int)(NAV_MAX_OMC + (idx - numExtOmcs));
                omcArrs.count++;
            }
        }
        Com_Log( SEV_DEBUG, LOG_CAT_NAV, "[NAV] drop OMCs detected: %d\n", omcArrs.count - numExtOmcs );
    }

    rcPolyMeshDetail *dmesh = rcAllocPolyMeshDetail();
    if ( !dmesh ) { Com_Log( SEV_INFO, LOG_CAT_NAV, "NAV: rcAllocPolyMeshDetail failed\n" ); rcFreeCompactHeightfield( chf ); rcFreePolyMesh( pmesh ); return NULL; }
    if ( !rcBuildPolyMeshDetail( &ctx, *pmesh, *chf, NAV_DETAIL_SAMPLE_DIST, NAV_DETAIL_SAMPLE_MAX_ERR, *dmesh ) ) {
        Com_Log( SEV_INFO, LOG_CAT_NAV, "NAV: rcBuildPolyMeshDetail failed\n" );
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
        Com_Log( SEV_INFO, LOG_CAT_NAV, "NAV: dtCreateNavMeshData failed (%d polys)\n", pmesh->npolys );
        rcFreePolyMesh( pmesh ); rcFreePolyMeshDetail( dmesh ); return NULL;
    }
    rcFreePolyMesh( pmesh ); rcFreePolyMeshDetail( dmesh );

    dtNavMesh *mesh = dtAllocNavMesh();
    if ( !mesh ) { dtFree( navData ); Com_Log( SEV_INFO, LOG_CAT_NAV, "NAV: dtAllocNavMesh failed\n" ); return NULL; }
    dtStatus status = mesh->init( navData, navDataSize, DT_TILE_FREE_DATA );
    if ( dtStatusFailed(status) ) {
        dtFree( navData ); dtFreeNavMesh( mesh );
        Com_Log( SEV_INFO, LOG_CAT_NAV, "NAV: dtNavMesh::init failed (0x%08x)\n", (unsigned)status );
        return NULL;
    }
    Com_Log( SEV_DEBUG, LOG_CAT_NAV, "NAV: navmesh built -- %d bytes, %d OMCs (%d drop)\n",
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

/* -------------------------------------------------------------------------
   Lifecycle
   ------------------------------------------------------------------------- */

void Nav_Init( void )
{
    memset( &nav, 0, sizeof(nav) );
    Nav_Debug_RegisterCommands();
    Com_Log( SEV_DEBUG, LOG_CAT_NAV, "[NAV] Nav_Init: Recast/Detour nav layer initialised\n" );
}

void Nav_Shutdown( void )
{
    Nav_UnloadMap();
    Com_Log( SEV_DEBUG, LOG_CAT_NAV, "[NAV] Nav_Shutdown\n" );
}

void Nav_UnloadMap( void )
{
    if ( nav.crowd ) { dtFreeCrowd( nav.crowd ); nav.crowd = NULL; }
    if ( nav.query ) { dtFreeNavMeshQuery( nav.query ); nav.query = NULL; }
    if ( nav.mesh  ) { dtFreeNavMesh( nav.mesh );  nav.mesh  = NULL; }
    nav.ready     = qfalse;
    nav.fromCache = qfalse;
}

int Nav_IsReady( void ) { return nav.ready ? 1 : 0; }

void Nav_LoadMap( const char *mapname )
{
    Nav_UnloadMap();
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
            Com_Log( SEV_INFO, LOG_CAT_NAV, "[NAV] loaded from cache: %d ms\n", ms );
            goto query_init;
        }
    }

    /* Slow path: full extraction + build */
    {
        navGeom_t    geom;
        navOmcInput_t omc;

        int t0 = Sys_Milliseconds();
        if ( !Nav_Geom_Extract( mapname, &geom, &omc ) ) {
            Com_Log( SEV_INFO, LOG_CAT_NAV, "[NAV] Nav_LoadMap: geometry extraction failed for '%s'\n", mapname );
            return;
        }

        if ( geom.numTris == 0 ) {
            Com_Log( SEV_INFO, LOG_CAT_NAV, "[NAV] Nav_LoadMap: no geometry in '%s'\n", mapname );
            Nav_OMC_Free( &omc );
            Nav_Geom_Free( &geom );
            return;
        }

        int t1 = Sys_Milliseconds();
        Com_Log( SEV_INFO, LOG_CAT_NAV, "[NAV] geometry extraction: %d ms (%d verts, %d tris)\n",
                    t1 - t0, geom.numVerts, geom.numTris );

        nav.mesh = Nav_Build_Internal( &geom, &omc );

        int t2 = Sys_Milliseconds();
        Com_Log( SEV_INFO, LOG_CAT_NAV, "[NAV] Recast pipeline:     %d ms\n", t2 - t1 );
        Com_Log( SEV_INFO, LOG_CAT_NAV, "[NAV] total build:         %d ms\n", t2 - t0 );

        nav.buildMs   = t2 - t0;
        nav.fromCache = qfalse;

        Nav_OMC_Free( &omc );
        Nav_Geom_Free( &geom );

        if ( !nav.mesh ) {
            Com_Log( SEV_INFO, LOG_CAT_NAV, "[NAV] Nav_LoadMap: build failed for '%s'\n", mapname );
            return;
        }

        /* Cache for next load; warn on write failure */
        if ( checksum != 0 )
            Nav_Cache_Save( mapname, checksum, nav.mesh );
    }

query_init:
    nav.query = dtAllocNavMeshQuery();
    if ( !nav.query ) {
        Com_Log( SEV_INFO, LOG_CAT_NAV, "[NAV] dtAllocNavMeshQuery failed\n" );
        Nav_Impl_FreeMesh( nav.mesh ); nav.mesh = NULL; return;
    }
    dtStatus st = nav.query->init( nav.mesh, NAV_MAX_QUERY_NODES );
    if ( dtStatusFailed(st) ) {
        Com_Log( SEV_INFO, LOG_CAT_NAV, "[NAV] dtNavMeshQuery::init failed (0x%08x)\n", (unsigned)st );
        dtFreeNavMeshQuery( nav.query ); nav.query = NULL;
        Nav_Impl_FreeMesh( nav.mesh );  nav.mesh  = NULL; return;
    }

    /* Tag door polys AFTER query is initialised (needs queryPolygons). */
    Nav_TagDoorAreas( mapname );

    nav.ready = qtrue;
    Com_Log( SEV_INFO, LOG_CAT_NAV, "[NAV] navmesh ready for '%s' (%s)\n",
                mapname, nav.fromCache ? "from cache" : "built" );
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
        /* Exclude polys that have been runtime-blocked (door closed, path sealed).
         * Without this, nav_setblocked has no effect on route computation. */
        s_filter.setExcludeFlags( (unsigned short)NAVPOLY_BLOCKED );
        s_filterInited = true;
    }
    return &s_filter;
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
    nav.query->findNearestPoly( rGoal,   kDefaultExtents, filter, &endRef,   nearPt );

    if ( !startRef || !endRef ) { out->count = 0; return 0; }

    static dtPolyRef polyPath[NAV_MAX_PATH_POINTS];
    int numPolys = 0;
    nav.query->findPath( startRef, endRef, rOrigin, rGoal, filter,
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
    for ( int i = 0; i < count; i++ ) {
        Nav_RecastToQuake( straightPos[i], out->positions[i] );
        out->flags[i]    = straightFlags[i];
        out->polyrefs[i] = (navPolyRef_t)straightRefs[i];
    }
    out->count = count;

    if ( Cvar_VariableIntegerValue( "nav_botdebug" ) ) {
        /* Check for any OMC waypoint (DT_STRAIGHTPATH_OFFMESH_CONNECTION = 0x04). */
        bool hasOmc = false;
        for ( int i = 0; i < count; i++ ) {
            if ( straightFlags[i] & 0x04 ) { hasOmc = true; break; }
        }
        Com_Log( SEV_DEBUG, LOG_CAT_NAV, "[BOTNAV] FindPath engine: %d polys -> %d pts%s\n",
                     numPolys, count, hasOmc ? " [OMC]" : "" );
    }

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
    for ( int i = 0; i < nav.numDoorEntries; i++ ) {
        if ( strcmp( nav.doorEntries[i].targetname, targetname ) == 0 ) {
            for ( int j = 0; j < nav.doorEntries[i].numPolyrefs; j++ ) {
                unsigned short flags = 0;
                nav.mesh->getPolyFlags( nav.doorEntries[i].polyrefs[j], &flags );
                flags = (unsigned short)(
                    (flags | (unsigned short)setFlags) & ~(unsigned short)clearFlags );
                nav.mesh->setPolyFlags( nav.doorEntries[i].polyrefs[j], flags );
            }
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
        Com_Log( SEV_DEBUG, LOG_CAT_NAV, "[NAV] no func_door entities on %s\n", mapname );
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
        centre[0] = (mins[0] + maxs[0]) * 0.5f;
        centre[1] = (mins[2] + maxs[2]) * 0.5f; /* Q Z → R Y */
        centre[2] = (mins[1] + maxs[1]) * 0.5f; /* Q Y → R Z */
        half[0]   = (maxs[0] - mins[0]) * 0.5f;
        half[1]   = (maxs[2] - mins[2]) * 0.5f;
        half[2]   = (maxs[1] - mins[1]) * 0.5f;

        /* Grow half-extents a little to catch polys straddling the boundary. */
        half[0] += 16.0f; half[1] += 8.0f; half[2] += 16.0f;

        static const int kMaxPolyResult = 64;
        dtPolyRef refs[64];
        int numFound = 0;

        const dtQueryFilter *filter = GetFilter();
        nav.query->queryPolygons( centre, half, filter, refs, &numFound, kMaxPolyResult );

        for ( int i = 0; i < numFound; i++ ) {
            unsigned short flags = 0;
            nav.mesh->getPolyFlags( refs[i], &flags );
            unsigned char area = 0;
            nav.mesh->getPolyArea( refs[i], &area );

            /* Set NAVPOLY_DOOR flag (OR with existing, preserve walkability). */
            flags = (unsigned short)(flags | (unsigned short)NAVPOLY_DOOR);
            nav.mesh->setPolyFlags( refs[i], flags );

            /* Upgrade area to NAVAREA_DOOR only from NAVAREA_GROUND. */
            if ( area == (unsigned char)NAVAREA_GROUND )
                nav.mesh->setPolyArea( refs[i], (unsigned char)NAVAREA_DOOR );

            polyCount++;
        }

        /* D-19: Store per-door poly list for runtime NAVPOLY_BLOCKED hookup. */
        if ( b < NAV_MAX_DOORS ) {
            navDoorEntry_t *de = &nav.doorEntries[b];
            Q_strncpyz( de->targetname, boxes[b].targetname, sizeof(de->targetname) );
            int nStore = numFound < NAV_MAX_DOOR_POLYS ? numFound : NAV_MAX_DOOR_POLYS;
            for ( int j = 0; j < nStore; j++ )
                de->polyrefs[j] = (navPolyRef_t)refs[j];
            de->numPolyrefs = nStore;
            nav.numDoorEntries = b + 1;
        }
    }

    Com_Log( SEV_INFO, LOG_CAT_NAV, "[NAV] tagged %d polys as NAVAREA_DOOR from %d func_door entities on %s\n",
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
    if ( !nav.ready || !nav.mesh ) { Com_Log( SEV_INFO, LOG_CAT_NAV, "NAV: navmesh not loaded\n" ); return; }

    const char *mapname = nav.mapname[0] ? nav.mapname : "unknown";

    char mtlName[MAX_OSPATH], objName[MAX_OSPATH];
    Com_sprintf( mtlName, sizeof(mtlName), "navmesh/%s.mtl", mapname );
    Com_sprintf( objName, sizeof(objName), "navmesh/%s.obj", mapname );

    fileHandle_t mtlF = 0;
    FS_FOpenFileByMode( mtlName, &mtlF, FS_WRITE );
    if ( !mtlF ) { Com_Log( SEV_INFO, LOG_CAT_NAV, "[NAV] nav_draw: cannot open %s\n", mtlName ); return; }

    FS_Printf( mtlF, "# q3now navmesh materials\n" );
    FS_Printf( mtlF, "newmtl nav_ground\n"      "Kd 0.6 0.6 0.6\n\n" );
    FS_Printf( mtlF, "newmtl nav_water\n"       "Kd 0.2 0.4 0.9\n\n" );
    FS_Printf( mtlF, "newmtl nav_lava\n"        "Kd 0.9 0.3 0.1\n\n" );
    FS_Printf( mtlF, "newmtl nav_door\n"        "Kd 0.9 0.9 0.2\n\n" );
    FS_Printf( mtlF, "newmtl nav_low_ceiling\n" "Kd 0.5 0.2 0.7\n\n" );
    FS_Printf( mtlF, "newmtl nav_offmesh\n"     "Kd 1.0 0.0 0.0\n\n" );
    FS_FCloseFile( mtlF );

    fileHandle_t f = 0;
    FS_FOpenFileByMode( objName, &f, FS_WRITE );
    if ( !f ) { Com_Log( SEV_INFO, LOG_CAT_NAV, "[NAV] nav_draw: cannot open %s\n", objName ); return; }

    /* Timestamp (server frames, not wall clock — we don't have strftime) */
    FS_Printf( f, "# q3now navmesh export: %s\n", mapname );
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
    Com_Log( SEV_INFO, LOG_CAT_NAV, "[NAV] wrote %d verts, %d tris to navmesh/%s.obj\n",
                totalVerts, totalTris, mapname );
}

/* -------------------------------------------------------------------------
   Nav_SetBlockedCmd  — "nav_setblocked <polyref> <0|1>"
   Manually flip NAVPOLY_BLOCKED on a poly for testing door-block mechanism.
   ------------------------------------------------------------------------- */
static void Nav_SetBlockedCmd( void )
{
    if ( Cmd_Argc() < 3 ) {
        Com_Log( SEV_INFO, LOG_CAT_NAV, "Usage: nav_setblocked <polyref> <0|1>\n" ); return;
    }
    if ( !nav.ready || !nav.mesh ) { Com_Log( SEV_INFO, LOG_CAT_NAV, "NAV: navmesh not loaded\n" ); return; }

    dtPolyRef ref = (dtPolyRef)atoi( Cmd_Argv(1) );
    int block     = atoi( Cmd_Argv(2) );

    unsigned short flags = 0;
    dtStatus st = nav.mesh->getPolyFlags( ref, &flags );
    if ( dtStatusFailed(st) ) {
        Com_Log( SEV_INFO, LOG_CAT_NAV, "[NAV] nav_setblocked: invalid polyref %u\n", (unsigned)ref ); return;
    }

    if ( block )
        flags = (unsigned short)(flags | (unsigned short)NAVPOLY_BLOCKED);
    else
        flags = (unsigned short)(flags & ~(unsigned short)NAVPOLY_BLOCKED);

    nav.mesh->setPolyFlags( ref, flags );
    Com_Log( SEV_INFO, LOG_CAT_NAV, "[NAV] poly %u %s\n", (unsigned)ref, block ? "blocked" : "unblocked" );
}

static void Nav_InfoCmd( void )
{
    if ( !nav.ready || !nav.mesh ) { Com_Log( SEV_INFO, LOG_CAT_NAV, "NAV: navmesh not loaded\n" ); return; }
    const dtNavMeshParams *params = nav.mesh->getParams();
    int totalPolys = 0, totalVerts = 0;
    int maxTiles = nav.mesh->getMaxTiles();
    for ( int i = 0; i < maxTiles; i++ ) {
        const dtMeshTile *tile = ((const dtNavMesh *)nav.mesh)->getTile(i);
        if ( !tile || !tile->header ) continue;
        totalPolys += tile->header->polyCount;
        totalVerts += tile->header->vertCount;
    }
    Com_Log( SEV_INFO, LOG_CAT_NAV, "--- NAV INFO ---\n" );
    Com_Log( SEV_INFO, LOG_CAT_NAV, "  Ready       : %s\n",    nav.ready     ? "yes" : "no" );
    Com_Log( SEV_INFO, LOG_CAT_NAV, "  Map         : %s\n",    nav.mapname[0] ? nav.mapname : "(none)" );
    Com_Log( SEV_INFO, LOG_CAT_NAV, "  Tile origin : %.1f %.1f %.1f\n", params->orig[0], params->orig[1], params->orig[2] );
    Com_Log( SEV_INFO, LOG_CAT_NAV, "  Tile size   : %.1f x %.1f\n",    params->tileWidth, params->tileHeight );
    Com_Log( SEV_INFO, LOG_CAT_NAV, "  Max tiles   : %d\n",    params->maxTiles );
    Com_Log( SEV_INFO, LOG_CAT_NAV, "  Polys/verts : %d / %d\n", totalPolys, totalVerts );
    Com_Log( SEV_INFO, LOG_CAT_NAV, "  Cache from  : %s\n",    nav.fromCache ? "disk" : "built" );
    Com_Log( SEV_INFO, LOG_CAT_NAV, "  Build time  : %d ms (%s)\n", nav.buildMs,
                nav.fromCache ? "from cache" : "built this session" );
    Com_Log( SEV_INFO, LOG_CAT_NAV, "--- END NAV ---\n" );
}

static void Nav_ClearCmd( void )
{
    Nav_ClearCache( Cmd_Argc() > 1 ? Cmd_Argv(1) : NULL );
}

void Nav_Debug_RegisterCommands( void )
{
    Cmd_AddCommand( "nav_info",       Nav_InfoCmd );
    Cmd_AddCommand( "nav_draw",       Nav_DrawCmd );
    Cmd_AddCommand( "nav_setblocked", Nav_SetBlockedCmd );
    Cmd_AddCommand( "nav_clear",      Nav_ClearCmd );
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
