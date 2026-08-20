// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
===========================================================================
nav_offmesh.c -- Off-mesh connection generation from BSP entity string.

Pure C port of nav_omc.cpp.  Changes from the original:
  - calloc/free replaced with Z_Malloc/Z_Free (engine memory accounting).
  - All C++ headers removed.
  - Included via nav_local.h.

Entity defs buffer: Z_Malloc(ENT_MAX_ENTITIES * sizeof(entDef_t)).
The buffer can reach ~1.5 MB at ENT_MAX_ENTITIES=4096 — too large for
Hunk_AllocateTempMemory (stack-based), so Z_Malloc is the correct choice.
===========================================================================
*/

#include "../q_shared.h"
#include "../q_feats.h"
LOG_DECLARE_CHANNEL( ch_nav_build, "nav.build" );

#if FEAT_RECAST_NAVMESH

#include "../qcommon.h"
#include "../maps/map_format_registry.h"
#include "nav_local.h"

#include <string.h>
#include <stdio.h>
#include <math.h>

/* -------------------------------------------------------------------------
   Entity string parser
   ------------------------------------------------------------------------- */

#define ENT_MAX_ENTITIES 4096
#define ENT_MAX_PAIRS    32
#define ENT_MAX_KEY      64
#define ENT_MAX_VALUE    256

typedef struct {
    char key[ENT_MAX_KEY];
    char value[ENT_MAX_VALUE];
} entPair_t;

typedef struct {
    entPair_t pairs[ENT_MAX_PAIRS];
    int       numPairs;
} entDef_t;

static const char *Ent_Get( const entDef_t *e, const char *key )
{
    int i;
    for ( i = 0; i < e->numPairs; i++ ) {
        if ( strcmp( e->pairs[i].key, key ) == 0 )
            return e->pairs[i].value;
    }
    return NULL;
}

static void parseVec3( const char *s, float v[3] )
{
    v[0] = v[1] = v[2] = 0.0f;
    if ( s ) sscanf( s, "%f %f %f", &v[0], &v[1], &v[2] );
}

/*
 * Parse all entities from the BSP entity string.
 * Returns Z_Malloc'd array; caller must Z_Free.
 */
static entDef_t *parseEntities( const char *es, int *countOut )
{
    entDef_t *defs = (entDef_t *)Z_Malloc( ENT_MAX_ENTITIES * (int)sizeof(entDef_t) );
    int n = 0;

    if ( !defs ) { *countOut = 0; return NULL; }
    memset( defs, 0, ENT_MAX_ENTITIES * sizeof(entDef_t) );

    while ( *es && n < ENT_MAX_ENTITIES ) {
        while ( *es && *es != '{' ) es++;
        if ( !*es ) break;
        es++;

        entDef_t *e = &defs[n];
        e->numPairs = 0;

        while ( *es && *es != '}' ) {
            while ( *es == ' ' || *es == '\t' || *es == '\n' || *es == '\r' )
                es++;
            if ( *es == '}' ) break;
            if ( *es != '"' ) { es++; continue; }

            es++;
            char key[ENT_MAX_KEY];
            int ki = 0;
            while ( *es && *es != '"' && ki < ENT_MAX_KEY - 1 )
                key[ki++] = *es++;
            key[ki] = '\0';
            if ( *es == '"' ) es++;

            while ( *es == ' ' || *es == '\t' || *es == '\n' || *es == '\r' )
                es++;
            if ( *es != '"' ) continue;

            es++;
            char value[ENT_MAX_VALUE];
            int vi = 0;
            while ( *es && *es != '"' && vi < ENT_MAX_VALUE - 1 )
                value[vi++] = *es++;
            value[vi] = '\0';
            if ( *es == '"' ) es++;

            if ( e->numPairs < ENT_MAX_PAIRS ) {
                strncpy( e->pairs[e->numPairs].key,   key,   ENT_MAX_KEY   - 1 );
                strncpy( e->pairs[e->numPairs].value, value, ENT_MAX_VALUE - 1 );
                e->pairs[e->numPairs].key[ENT_MAX_KEY-1]     = '\0';
                e->pairs[e->numPairs].value[ENT_MAX_VALUE-1] = '\0';
                e->numPairs++;
            }
        }
        if ( *es == '}' ) es++;
        n++;
    }

    *countOut = n;
    return defs;
}

/* -------------------------------------------------------------------------
   Entity-centre helpers
   ------------------------------------------------------------------------- */

static void entCentre( const entDef_t *e, const mapFile_t *bsp, float out[3] )
{
    out[0] = out[1] = out[2] = 0.0f;
    parseVec3( Ent_Get(e, "origin"), out );

    const char *model = Ent_Get(e, "model");
    if ( model && model[0] == '*' ) {
        int idx = atoi( model + 1 );
        if ( idx > 0 && idx < bsp->numSubModels ) {
            const dmodel_t *dm = &bsp->subModels[idx];
            float cx = (dm->mins[0] + dm->maxs[0]) * 0.5f;
            float cy = (dm->mins[1] + dm->maxs[1]) * 0.5f;
            float cz = (dm->mins[2] + dm->maxs[2]) * 0.5f;
            out[0] += cx;
            out[1] += cy;
            out[2] += cz;
        }
    }
}

/* Format-agnostic classname: strip an optional "q1_" prefix (the Q1 loader
 * prefixes classnames) so mechanism recognizers match both Q3 "trigger_teleport"
 * and Q1 "q1_trigger_teleport".  Mirrors buildDoorGapOmcs' and NavReg_Bare's
 * rule; hoisted here so the teleporter / jump-pad / target_push / activating-
 * trigger recognizers below share it (they previously used a raw strcmp that
 * silently skipped every Q1-format entity — e.g. e1m1's sole teleporter). */
static const char *omcBareClass( const char *cn )
{
    if ( !cn ) return "";
    if ( Q_stricmpn( cn, "q1_", 3 ) == 0 ) return cn + 3;
    return cn;
}

static const entDef_t *findByTargetname( const entDef_t *defs, int numDefs,
                                          const char *tname )
{
    int i;
    for ( i = 0; i < numDefs; i++ ) {
        const char *tn = Ent_Get( &defs[i], "targetname" );
        if ( tn && strcmp( tn, tname ) == 0 )
            return &defs[i];
    }
    return NULL;
}

/* Find a trigger entity (trigger_multiple, trigger_once, trigger_push) whose
 * "target" field matches 'targetname'.  Used to resolve the activating trigger
 * for target_push entities (FIX-4). */
static const entDef_t *findActivatingTrigger( const entDef_t *defs, int numDefs,
                                               const char *targetname )
{
    static const char *kTriggerClasses[] = {
        "trigger_multiple", "trigger_once", "trigger_push", "trigger_hurt", NULL
    };
    int i;
    for ( i = 0; i < numDefs; i++ ) {
        const char *cn = Ent_Get( &defs[i], "classname" );
        if ( !cn ) continue;
        int j;
        for ( j = 0; kTriggerClasses[j]; j++ ) {
            if ( strcmp( omcBareClass( cn ), kTriggerClasses[j] ) == 0 ) {
                const char *t = Ent_Get( &defs[i], "target" );
                if ( t && strcmp( t, targetname ) == 0 )
                    return &defs[i];
                break;
            }
        }
    }
    return NULL;
}

/* -------------------------------------------------------------------------
   Jump pad (trigger_push) — parabolic arc approximation
   landing.xy = 2 * apex.xy - trigger.xy; landing.z = trigger.z
   ------------------------------------------------------------------------- */

static void jumpPadLanding( const float trigger[3], const float apex[3],
                             float landing[3] )
{
    landing[0] = 2.0f * apex[0] - trigger[0];
    landing[1] = 2.0f * apex[1] - trigger[1];
    landing[2] = trigger[2];
}

static void buildJumpPads( const entDef_t *defs, int numDefs,
                            const mapFile_t *bsp, navOmcInput_t *out )
{
    int i;
    for ( i = 0; i < numDefs && out->count < NAV_MAX_OMC; i++ ) {
        const entDef_t *e = &defs[i];
        const char *cn = Ent_Get( e, "classname" );
        if ( strcmp( omcBareClass( cn ), "trigger_push" ) != 0 ) continue;

        const char *target = Ent_Get( e, "target" );
        if ( !target ) continue;

        const entDef_t *apexEnt = findByTargetname( defs, numDefs, target );
        if ( !apexEnt ) continue;

        float trigCentre[3], apex[3], landing[3];
        entCentre( e,       bsp, trigCentre );
        entCentre( apexEnt, bsp, apex );
        jumpPadLanding( trigCentre, apex, landing );

        navOmcEntry_t *omc = &out->entries[out->count];
        memcpy( omc->start, trigCentre, sizeof(trigCentre) );
        memcpy( omc->end,   landing,    sizeof(landing) );
        omc->radius = 32.0f;
        omc->area   = (unsigned char)NAVAREA_JUMP_LINK;
        omc->flags  = (unsigned short)(NAVPOLY_WALKABLE | NAVPOLY_OFFMESH);
        omc->bidir  = 0;
        omc->traversalMode = (unsigned char)NAV_TM_BALLISTIC;   /* jump-pad: pad velocity carries */
        out->count++;
        Com_Log( SEV_DEBUG, LOG_CH(ch_nav_build), "OMC pad %d: trigger=(%.0f,%.0f,%.0f) apex=(%.0f,%.0f,%.0f) landing=(%.0f,%.0f,%.0f)\n",
                     out->count - 1,
                     trigCentre[0], trigCentre[1], trigCentre[2],
                     apex[0], apex[1], apex[2],
                     landing[0], landing[1], landing[2] );
    }
}

/* -------------------------------------------------------------------------
   Teleporter (trigger_teleport)
   ------------------------------------------------------------------------- */

static void buildTeleporters( const entDef_t *defs, int numDefs,
                               const mapFile_t *bsp, navOmcInput_t *out )
{
    int i;
    for ( i = 0; i < numDefs && out->count < NAV_MAX_OMC; i++ ) {
        const entDef_t *e = &defs[i];
        const char *cn = Ent_Get( e, "classname" );
        if ( strcmp( omcBareClass( cn ), "trigger_teleport" ) != 0 ) continue;

        const char *target = Ent_Get( e, "target" );
        if ( !target ) continue;

        const entDef_t *destEnt = findByTargetname( defs, numDefs, target );
        if ( !destEnt ) continue;

        float trigCentre[3], dest[3];
        entCentre( e,       bsp, trigCentre );
        entCentre( destEnt, bsp, dest );

        navOmcEntry_t *omc = &out->entries[out->count];
        memcpy( omc->start, trigCentre, sizeof(trigCentre) );
        memcpy( omc->end,   dest,       sizeof(dest) );
        omc->radius = 32.0f;
        omc->area   = (unsigned char)NAVAREA_TELEPORT;
        omc->flags  = (unsigned short)(NAVPOLY_WALKABLE | NAVPOLY_OFFMESH);
        omc->bidir  = 0;
        omc->traversalMode = (unsigned char)NAV_TM_BALLISTIC;   /* teleporter: instant, no walk */
        out->count++;
    }
}

/* -------------------------------------------------------------------------
   target_push — one-way impulse entity.
   OMC source = the activating trigger's AABB centroid (trigger_multiple /
   trigger_once / trigger_push entity whose "target" matches this entity's
   "targetname").  OMC destination = entity targeted by target_push->target.
   If no activating trigger is found, skip this entry entirely — falling back
   to the target_push origin gives a misleading OMC and is worse than nothing.

   DEFERRED: review with a map that actually has target_push entities.
   Q3DM6 and Q3DM13 have none, so test gates do not exercise this path.
   "target_push semantics require review on a map that actually uses the
   entity — current implementation uses the activating trigger's volume
   centroid as source, which may not match all configurations."
   ------------------------------------------------------------------------- */

static void buildTargetPushOmcs( const entDef_t *defs, int numDefs,
                                  const mapFile_t *bsp, navOmcInput_t *out )
{
    int i;
    for ( i = 0; i < numDefs && out->count < NAV_MAX_OMC; i++ ) {
        const entDef_t *e = &defs[i];
        const char *cn = Ent_Get( e, "classname" );
        if ( strcmp( omcBareClass( cn ), "target_push" ) != 0 ) continue;

        const char *target = Ent_Get( e, "target" );
        if ( !target ) continue; /* no destination — skip */

        const char *tname = Ent_Get( e, "targetname" );
        if ( !tname ) continue; /* no targetname — can't find activating trigger */

        /* Find the trigger that activates this target_push. */
        const entDef_t *activator = findActivatingTrigger( defs, numDefs, tname );
        if ( !activator ) continue; /* no activating trigger — skip entirely */

        /* Find the destination entity. */
        const entDef_t *destEnt = findByTargetname( defs, numDefs, target );
        if ( !destEnt ) continue;

        float src[3], dst[3];
        entCentre( activator, bsp, src );
        entCentre( destEnt,   bsp, dst );

        navOmcEntry_t *omc = &out->entries[out->count];
        memcpy( omc->start, src, sizeof(src) );
        memcpy( omc->end,   dst, sizeof(dst) );
        omc->radius = 32.0f;
        omc->area   = (unsigned char)NAVAREA_JUMP_LINK;
        omc->flags  = (unsigned short)(NAVPOLY_WALKABLE | NAVPOLY_OFFMESH);
        omc->bidir  = 0;
        omc->traversalMode = (unsigned char)NAV_TM_BALLISTIC;   /* target_push: push velocity carries */
        out->count++;
    }
}

/* -------------------------------------------------------------------------
   Vertical movers (elevators/platforms) — bidirectional off-mesh connections.

   Format-neutral: this business-logic layer NEVER names a mover classname and
   NEVER reads an entity origin key. It asks the map-format adapter
   (bsp->format->extractMovers) for a list of format-neutral navMoverDesc_t
   descriptors — each already carries world-space topLedge (upper stop) and
   shaftFloor (lower stop) — and emits one bidirectional OMC per descriptor:
     start = shaftFloor (boarding position at the bottom)
     end   = topLedge   (delivery position at the top)
   All format divergence (Q1's origin=(0,0,0)+AABB math, Q3's origin-key math,
   tomorrow's Doom 3) lives in the adapters (BSP_Q1_ExtractMovers /
   BSP_Q3_ExtractMovers). Adding a new format's movers never touches this file.
   ------------------------------------------------------------------------- */

static void buildPlatforms( const mapFile_t *bsp, navOmcInput_t *out )
{
    navMoverDesc_t movers[NAV_MAX_OMC];
    int n, i, maxOut;

    if ( !bsp->format || !bsp->format->extractMovers )
        return;

    maxOut = NAV_MAX_OMC - out->count;
    if ( maxOut <= 0 )
        return;
    if ( maxOut > NAV_MAX_OMC )
        maxOut = NAV_MAX_OMC;

    n = bsp->format->extractMovers( bsp, movers, maxOut );

    for ( i = 0; i < n && out->count < NAV_MAX_OMC; i++ ) {
        navOmcEntry_t *omc = &out->entries[out->count];
        memcpy( omc->start, movers[i].shaftFloor, sizeof(omc->start) );
        memcpy( omc->end,   movers[i].topLedge,   sizeof(omc->end) );
        omc->radius = movers[i].radius;
        omc->area   = (unsigned char)NAVAREA_JUMP_LINK;
        omc->flags  = (unsigned short)(NAVPOLY_WALKABLE | NAVPOLY_OFFMESH);
        omc->bidir  = 1; /* bidirectional: agents can ride up or down */
        /* Plat WALK vs RIDE: a plat whose two endpoints differ vertically by more
         * than a step is an ELEVATOR the rider must board and ride (RIDE); a level
         * plat link is a flat crossing the bot walks (WALK).  The producer knows the
         * mover's own endpoint geometry — |topLedge.z - shaftFloor.z| decides, gated
         * on the existing step-height constant (NAV_WATEREDGE_STEP = 18u STEPSIZE). */
        {
            float platRise = movers[i].topLedge[2] - movers[i].shaftFloor[2];
            if ( platRise < 0.0f ) platRise = -platRise;
            omc->traversalMode = ( platRise > NAV_WATEREDGE_STEP )
                               ? (unsigned char)NAV_TM_RIDE
                               : (unsigned char)NAV_TM_WALK;
        }
        out->count++;
    }
}

/* -------------------------------------------------------------------------
   Door floor-gap off-mesh connection.

   A solid door brush (func_door / func_door_secret) can straddle a real hole
   in the walkable collision floor: the two floor components sit on either side
   of a gap the closed door blocks and the open door lets the bot cross.  This
   emits ONE off-mesh connection per such door, gated on an ACTUAL floor gap so
   a door flush on continuous floor emits nothing.

   Format-agnostic framing: the placement uses the mover's submodel AABB + the
   extracted walkable-floor geometry, not a Q1-only classname branch in the
   span logic.  It lives in the door-recognition path (Q1 doors are the case)
   and reuses the same "q1_"-strip rule the door/mover recognizers use.
   ------------------------------------------------------------------------- */

/* Sample the extracted walkable floor at Quake XY (qx,qy): return the z of the
 * highest walkable triangle whose XY projection contains the point within
 * [zLo, zHi], or return qfalse if no floor is present there.  The geom verts
 * are Recast (Y-up): Recast (X, Y, Z) = Quake (X, Z, -Y), so a Quake point
 * (qx,qy) maps to Recast (rx=qx, rz=-qy) in the XZ plane and Quake floor z is
 * Recast Y. */
static qboolean floorZAt( const navGeom_t *geom, float qx, float qy,
                          float zLo, float zHi, float *outZ )
{
    const float rx = qx;
    const float rz = -qy;
    qboolean found = qfalse;
    float bestZ = -1e30f;
    int t;

    if ( !geom || !geom->verts || !geom->tris || geom->numTris <= 0 )
        return qfalse;

    for ( t = 0; t < geom->numTris; t++ ) {
        const int i0 = geom->tris[t*3+0];
        const int i1 = geom->tris[t*3+1];
        const int i2 = geom->tris[t*3+2];
        const float *a = &geom->verts[i0*3];
        const float *b = &geom->verts[i1*3];
        const float *c = &geom->verts[i2*3];

        /* Point-in-triangle in the Recast XZ plane (barycentric). */
        const float ax = a[0], az = a[2];
        const float bx = b[0], bz = b[2];
        const float cx = c[0], cz = c[2];
        const float d = (bz - cz)*(ax - cx) + (cx - bx)*(az - cz);
        if ( fabsf( d ) < 1e-6f ) continue;   /* degenerate / vertical */
        const float w0 = ( (bz - cz)*(rx - cx) + (cx - bx)*(rz - cz) ) / d;
        const float w1 = ( (cz - az)*(rx - cx) + (ax - cx)*(rz - cz) ) / d;
        const float w2 = 1.0f - w0 - w1;
        if ( w0 < -0.001f || w1 < -0.001f || w2 < -0.001f ) continue;

        /* Interpolated Recast Y (= Quake floor z) at the sample point. */
        const float fz = w0*a[1] + w1*b[1] + w2*c[1];
        if ( fz < zLo || fz > zHi ) continue;
        if ( fz > bestZ ) { bestZ = fz; found = qtrue; }
    }

    if ( found && outZ ) *outZ = bestZ;
    return found;
}

/* Emit door floor-gap OMCs.  Reuses the parsed entity list for classname +
 * model, reads the door submodel AABB, samples the floor across the door
 * footprint along the passage (thin-horizontal) axis, and emits one bidir OMC
 * per door that straddles a real floor gap. */
static void buildDoorGapOmcs( const entDef_t *defs, int numDefs,
                              const mapFile_t *bsp, const navGeom_t *geom,
                              navOmcInput_t *out )
{
    int i;

    if ( !geom || geom->numTris <= 0 )
        return;   /* no floor geometry to gate/place against */

    for ( i = 0; i < numDefs && out->count < NAV_MAX_OMC; i++ ) {
        const entDef_t *e = &defs[i];
        const char *cn = Ent_Get( e, "classname" );
        if ( !cn ) continue;
        /* Strip an optional "q1_" prefix (the Q1 loader prefixes classnames),
         * then match the bare door classnames. */
        const char *bare = cn;
        if ( Q_stricmpn( bare, "q1_", 3 ) == 0 ) bare += 3;
        if ( Q_stricmp( bare, "func_door" ) != 0 &&
             Q_stricmp( bare, "func_door_secret" ) != 0 )
            continue;

        const char *model = Ent_Get( e, "model" );
        if ( !model || model[0] != '*' ) continue;
        int idx = atoi( model + 1 );
        if ( idx <= 0 || idx >= bsp->numSubModels ) continue;

        const dmodel_t *dm = &bsp->subModels[idx];
        const float dmnx = dm->mins[0], dmxx = dm->maxs[0];
        const float dmny = dm->mins[1], dmxy = dm->maxs[1];
        const float dbaseZ = dm->mins[2];   /* door leaf base — near the floor */
        const float spanX = dmxx - dmnx;
        const float spanY = dmxy - dmny;

        /* Passage axis = the door's thin horizontal axis (a bot walks THROUGH
         * the doorway across the thin slab; the long axis runs along the wall).
         * axis 0 = X is the passage direction; axis 1 = Y is the passage. */
        int passAxis = ( spanX <= spanY ) ? 0 : 1;

        /* Sweep along the passage axis across the door footprint (grown a little
         * so the endpoints land just past the gap lip on real floor), sampling
         * the floor at the door's cross-axis centre.  Build a present/absent
         * floor profile and its z either side of the first real gap. */
        const float crossCentre = ( passAxis == 0 )
            ? 0.5f*(dmny + dmxy)   /* Y centre when passage is X */
            : 0.5f*(dmnx + dmxx);  /* X centre when passage is Y */
        const float grow = NAV_DOORGAP_MAX_WIDTH;   /* reach onto solid floor either side */
        const float lo = ( passAxis == 0 ) ? (dmnx - grow) : (dmny - grow);
        const float hi = ( passAxis == 0 ) ? (dmxx + grow) : (dmxy + grow);
        const float zLo = dbaseZ - NAV_DOORGAP_Z_WINDOW;
        const float zHi = dbaseZ + NAV_DOORGAP_Z_WINDOW;

        /* Walk the profile: find the last solid sample before the gap (near
         * side) and the first solid sample after it (far side). */
        float p = lo;
        float nearEdge = 0.0f, farEdge = 0.0f, nearZ = 0.0f, farZ = 0.0f;
        qboolean haveNear = qfalse, inGap = qfalse, gapFound = qfalse;
        float gapStart = 0.0f;

        for ( ; p <= hi + 0.5f; p += NAV_DOORGAP_SAMPLE_STEP ) {
            float qx = ( passAxis == 0 ) ? p : crossCentre;
            float qy = ( passAxis == 0 ) ? crossCentre : p;
            float fz;
            qboolean solid = floorZAt( geom, qx, qy, zLo, zHi, &fz );

            if ( solid ) {
                if ( inGap ) {
                    /* We just crossed the gap: measure it. */
                    float gapW = p - gapStart;
                    if ( gapW >= NAV_DOORGAP_MIN_WIDTH && gapW <= NAV_DOORGAP_MAX_WIDTH ) {
                        farEdge = p; farZ = fz; gapFound = qtrue;
                        break;   /* first real gap under the door is enough */
                    }
                    inGap = qfalse;   /* too-narrow/too-wide run — not our gap */
                }
                nearEdge = p; nearZ = fz; haveNear = qtrue;
            } else {
                if ( haveNear && !inGap ) { inGap = qtrue; gapStart = p; }
            }
        }

        if ( !gapFound ) {
            Com_Log( SEV_DEBUG, LOG_CH(ch_nav_build),
                "door-gap: door model=*%d AABB x[%.0f,%.0f] y[%.0f,%.0f] z%.0f passAxis=%d -> no real floor gap (skip)\n",
                idx, dmnx, dmxx, dmny, dmxy, dbaseZ, passAxis );
            continue;
        }

        /* Place endpoints on real floor just inside each solid side, past the
         * gap lip, at the sampled floor z. */
        float startP = nearEdge - NAV_DOORGAP_ENDPOINT_INSET;
        float endP   = farEdge  + NAV_DOORGAP_ENDPOINT_INSET;
        /* Re-sample floor z at the inset points so the endpoint sits on floor. */
        float sQx = ( passAxis == 0 ) ? startP : crossCentre;
        float sQy = ( passAxis == 0 ) ? crossCentre : startP;
        float eQx = ( passAxis == 0 ) ? endP : crossCentre;
        float eQy = ( passAxis == 0 ) ? crossCentre : endP;
        float sZ = nearZ, eZ = farZ;
        floorZAt( geom, sQx, sQy, zLo, zHi, &sZ );
        floorZAt( geom, eQx, eQy, zLo, zHi, &eZ );

        /* Dedup: two overlapping door leaves (a func_door + a func_door_secret
         * at the same threshold) both straddle the SAME floor gap and would each
         * emit an OMC.  A gap is a property of the floor, not of any one door, so
         * skip a second OMC whose span coincides with one already emitted this
         * pass — one connection per real gap, not per door leaf. */
        {
            qboolean dup = qfalse;
            int j;
            for ( j = 0; j < out->count; j++ ) {
                const navOmcEntry_t *o = &out->entries[j];
                if ( fabsf( o->start[0] - sQx ) <= NAV_DOORGAP_SAMPLE_STEP &&
                     fabsf( o->start[1] - sQy ) <= NAV_DOORGAP_SAMPLE_STEP &&
                     fabsf( o->end[0]   - eQx ) <= NAV_DOORGAP_SAMPLE_STEP &&
                     fabsf( o->end[1]   - eQy ) <= NAV_DOORGAP_SAMPLE_STEP ) {
                    dup = qtrue; break;
                }
            }
            if ( dup ) {
                Com_Log( SEV_DEBUG, LOG_CH(ch_nav_build),
                    "door-gap: door model=*%d straddles an already-linked gap start=(%.0f,%.0f) — skip duplicate\n",
                    idx, sQx, sQy );
                continue;
            }
        }

        navOmcEntry_t *omc = &out->entries[out->count];
        omc->start[0] = sQx; omc->start[1] = sQy; omc->start[2] = sZ;
        omc->end[0]   = eQx; omc->end[1]   = eQy; omc->end[2]   = eZ;
        omc->radius = 32.0f;
        omc->area   = (unsigned char)NAVAREA_JUMP_LINK;
        omc->flags  = (unsigned short)(NAVPOLY_WALKABLE | NAVPOLY_OFFMESH);
        omc->bidir  = 1;   /* a door is crossed both ways */
        omc->traversalMode = (unsigned char)NAV_TM_WALK;   /* door gap: a level crossing, walked */
        out->count++;

        Com_Log( SEV_INFO, LOG_CH(ch_nav_build),
            "door-gap OMC %d: door model=*%d passAxis=%d gap[%.0f,%.0f] (%.0fu) "
            "start=(%.0f,%.0f,%.0f) end=(%.0f,%.0f,%.0f)\n",
            out->count - 1, idx, passAxis, nearEdge, farEdge, farEdge - nearEdge,
            omc->start[0], omc->start[1], omc->start[2],
            omc->end[0], omc->end[1], omc->end[2] );
    }
}

/* -------------------------------------------------------------------------
   Water-edge off-mesh connection.

   Reconnects two walkable floor components split by the bake at a shallow
   liquid seam (a wade the bake fragmented into a riser staircase above the
   walk-climb).  Runs on the MAIN THREAD (Nav_OMC_Build, before the bake spawns)
   so the loaded collision model can be queried; the worker only consumes the
   emitted navOmcEntry_t values in voxel space.  All collision goes through the
   ONE format-agnostic tracer: CM_BoxTrace resolves Q1 hull-1 (CONTENTS_PLAYERCLIP
   selects the hull) and Q3 brushes with no Q1/Q3 branch; CM_PointContents reads
   the liquid contents both formats expose.  The 4 gates (real-floor, clear
   column, foot-in-water, gap-cap) bound it to genuine liquid crossings.
   ------------------------------------------------------------------------- */

/* Per-cell floor field entry.  hasFloor==0 means the column has no walkable
 * floor triangle in the geom (most of the grid).  floorZ is first the geom
 * coverage anchor, then the collision pass replaces it with the hull-1 foot. */
typedef struct {
    float         floorZ;    /* Quake z of the resting foot (hull-1 collision)  */
    int           label;     /* connected-component id (climb rule); 0 = none  */
    unsigned char hasFloor;
    unsigned char water;     /* foot-in-liquid (CONTENTS_WATER/SLIME/LAVA)      */
} waterEdgeCell_t;

/* Hull-1 point-solid test: a zero-length MASK_PLAYERSOLID trace.  The
 * CONTENTS_PLAYERCLIP bit selects the Q1 player hull (hull-1) and matches Q3
 * player-solid brushes — one call, no Q1/Q3 branch. */
static qboolean waterEdgePointSolid( float qx, float qy, float qz )
{
    trace_t tr;
    vec3_t  p = { qx, qy, qz };
    CM_BoxTrace( &tr, p, p, vec3_origin, vec3_origin, 0, MASK_PLAYERSOLID, qfalse );
    return ( tr.startsolid || tr.allsolid ) ? qtrue : qfalse;
}

/* Column scan under (qx,qy): find the resting foot on the LOWEST hull-1 floor
 * that has full standing headroom (the ground, not a ledge/overhang top), and
 * report whether the column touches liquid near that floor.  Mirrors the
 * read-only over-fire measurement's build_floor_field: one bottom→top pass
 * recording solidity, then take the first (lowest) solid→non-solid transition
 * that has NAV_WATEREDGE_HEADROOM of clear space above.  Collision (hull-1) is
 * the sole floor authority — the geom only says WHERE to look (its z can be a
 * wall/water surface, so it is not trusted for the resting height).  Returns
 * qfalse if the column has no standable floor. */
/* Traces spent by the current water-edge scan, against NAV_WATEREDGE_TRACE_BUDGET.
 * File-scope rather than threaded through every helper: the scan is single-
 * threaded main-thread work (collision is not thread-safe, which is the whole
 * reason it runs here and not on the bake worker), so there is no second scan to
 * confuse it with. Reset at the start of each scan. */
static long s_waterEdgeTraces;

static qboolean waterEdgeColumnFloor( float qx, float qy, float zLo, float zHi,
                                      float *outFootZ, qboolean *outWater )
{
    #define NAV_WATEREDGE_ZSAMPLES 544
    const float step = 4.0f;
    int   nz = (int)( ( zHi - zLo ) / step ) + 1;
    unsigned char solid[NAV_WATEREDGE_ZSAMPLES];
    int   i;
    if ( nz > NAV_WATEREDGE_ZSAMPLES ) nz = NAV_WATEREDGE_ZSAMPLES;

    /* Walk UP the column, sampling only as far as the answer requires.
     *
     * What is wanted is the LOWEST solid→non-solid transition carrying a full
     * standing run of clear space above it — the ground, not the top of a ledge
     * or overhang. That condition is local: once a transition is found, only the
     * headSteps samples above it decide the outcome, and nothing higher in the
     * column can change it. So the scan can stop there.
     *
     * The previous shape sampled the ENTIRE column first and searched
     * afterwards, which made every cell cost the full Z extent regardless of
     * where its floor was. Measured on arena7 that was ~445 traces per cell
     * against a 544 ceiling — i.e. the early exit was doing almost nothing,
     * because there was no early exit. Floors sit near the bottom of a column
     * far more often than not, so stopping at foot+headroom is the difference
     * between "scan the map's whole Z range per cell" and "scan until you find
     * the ground".
     *
     * Identical result, not an approximation: same predicate, same first match,
     * evaluated in the same bottom-up order. */
    const int headSteps = (int)( NAV_WATEREDGE_HEADROOM / step );
    float footZ = 0.0f;
    qboolean haveFloor = qfalse;
    int sampled = 0;

    for ( i = 0; i < nz; i++ ) {
        solid[i] = waterEdgePointSolid( qx, qy, zLo + i*step ) ? 1 : 0;
        sampled++;

        if ( i == 0 || !( solid[i-1] && !solid[i] ) )
            continue;

        /* A transition at i: does clear space run headSteps above it? Sample
         * only that far — and if the run is broken by solid before reaching
         * headSteps, this is a ledge top, so resume the outer walk from there
         * rather than rescanning. */
        int j, run = 0;
        for ( j = i; j < nz && run < headSteps; j++ ) {
            if ( j > i ) {
                solid[j] = waterEdgePointSolid( qx, qy, zLo + j*step ) ? 1 : 0;
                sampled++;
            }
            if ( solid[j] ) break;
            run++;
        }
        if ( run >= headSteps ) {
            footZ = zLo + i*step;
            haveFloor = qtrue;
            break;
        }
        i = j - 1;   /* continue above the rejected run; loop's i++ steps past it */
    }
    s_waterEdgeTraces += sampled;

    if ( !haveFloor )
        return qfalse;

    /* Liquid near the foot (world contents — the pm->pointcontents path). */
    qboolean water = qfalse;
    {
        static const float band[] = { 1.0f, NAV_WATEREDGE_WATER_BAND * 0.5f,
                                      NAV_WATEREDGE_WATER_BAND, -2.0f };
        int bi;
        for ( bi = 0; bi < (int)(sizeof(band)/sizeof(band[0])); bi++ ) {
            vec3_t p = { qx, qy, footZ + band[bi] };
            if ( CM_PointContents( p, 0 ) & MASK_WATER ) { water = qtrue; break; }
        }
    }

    if ( outFootZ ) *outFootZ = footZ;
    if ( outWater ) *outWater = water;
    return qtrue;
    #undef NAV_WATEREDGE_ZSAMPLES
}

/* Real-floor gate: solid within a step below the foot. */
static qboolean waterEdgeRealFloor( float qx, float qy, float footZ )
{
    trace_t tr;
    vec3_t  start = { qx, qy, footZ + 1.0f };
    vec3_t  end   = { qx, qy, footZ - NAV_WATEREDGE_STEP - 4.0f };
    CM_BoxTrace( &tr, start, end, vec3_origin, vec3_origin, 0,
                 MASK_PLAYERSOLID, qfalse );
    return ( tr.fraction < 1.0f && !tr.startsolid ) ? qtrue : qfalse;
}

/* Clear-column gate: the straight segment between two edge points at resting
 * height is all non-solid (no submerged wall between them). */
static qboolean waterEdgeColumnClear( float x0, float y0, float x1, float y1,
                                      float restZ )
{
    float dx = x1 - x0, dy = y1 - y0;
    float len = sqrtf( dx*dx + dy*dy );
    int   n   = (int)( len / 2.0f );
    int   i;
    if ( n < 2 ) n = 2;
    for ( i = 0; i <= n; i++ ) {
        float t = (float)i / (float)n;
        vec3_t p = { x0 + dx*t, y0 + dy*t, restZ };
        if ( CM_PointContents( p, 0 ) & CONTENTS_SOLID )
            return qfalse;
    }
    return qtrue;
}

/* PM_SetWaterLevel model at the deepest column point (link-type decider).
 * footZ = resting foot z; origin = footZ + |MINS_Z|.  Samples the same three
 * heights PM_SetWaterLevel uses.  Returns WATERLEVEL_* (0..3). */
static int waterEdgeWaterLevel( float qx, float qy, float footZ )
{
    const float originZ  = footZ + NAV_WATEREDGE_MINS_Z;    /* origin 24u above foot */
    const float minsZ    = -NAV_WATEREDGE_MINS_Z;
    const float sample2  = NAV_WATEREDGE_VIEWH - minsZ;     /* viewheight - MINS_Z    */
    const float sample1  = sample2 * 0.5f;
    vec3_t p;
    int    c;

    p[0] = qx; p[1] = qy;
    p[2] = originZ + minsZ + 1.0f;
    c = CM_PointContents( p, 0 );
    if ( !( c & MASK_WATER ) )
        return 0;                                            /* WATERLEVEL_NONE */
    p[2] = originZ + minsZ + sample1;
    if ( !( CM_PointContents( p, 0 ) & MASK_WATER ) )
        return 1;                                            /* WATERLEVEL_FEET */
    p[2] = originZ + minsZ + sample2;
    if ( !( CM_PointContents( p, 0 ) & MASK_WATER ) )
        return 2;                                            /* WATERLEVEL_HALFWAY (wade) */
    return 3;                                                /* WATERLEVEL_SUBMERGED (swim) */
}

/* Rasterize the extracted nav floor geom into an 8u XY grid, marking which cells
 * a walkable floor triangle covers and keeping the lowest covering z as a
 * where-to-look anchor (the exact resting height comes from the collision scan).
 * The geom verts are Recast (Y-up): Recast (X, Y, Z) = Quake (X, Z, -Y), so a
 * triangle's Recast Y is the Quake floor z and Recast (x,z) map to Quake (x, -z).
 * Mirrors floorZAt's mapping. */
static void waterEdgeRasterFloor( const navGeom_t *geom, waterEdgeCell_t *cells,
                                  float ox, float oy, int nx, int ny )
{
    int t;
    for ( t = 0; t < geom->numTris; t++ ) {
        const int i0 = geom->tris[t*3+0];
        const int i1 = geom->tris[t*3+1];
        const int i2 = geom->tris[t*3+2];
        const float *a = &geom->verts[i0*3];
        const float *b = &geom->verts[i1*3];
        const float *c = &geom->verts[i2*3];
        /* Quake XY of each vertex (qx = rx, qy = -rz); Quake floor z = ry. */
        const float qx0 = a[0], qy0 = -a[2], qz0 = a[1];
        const float qx1 = b[0], qy1 = -b[2], qz1 = b[1];
        const float qx2 = c[0], qy2 = -c[2], qz2 = c[1];
        float minx = qx0, maxx = qx0, miny = qy0, maxy = qy0;
        if ( qx1 < minx ) minx = qx1; if ( qx1 > maxx ) maxx = qx1;
        if ( qx2 < minx ) minx = qx2; if ( qx2 > maxx ) maxx = qx2;
        if ( qy1 < miny ) miny = qy1; if ( qy1 > maxy ) maxy = qy1;
        if ( qy2 < miny ) miny = qy2; if ( qy2 > maxy ) maxy = qy2;

        int ixlo = (int)floorf( (minx - ox) / NAV_WATEREDGE_CELL );
        int ixhi = (int)floorf( (maxx - ox) / NAV_WATEREDGE_CELL );
        int iylo = (int)floorf( (miny - oy) / NAV_WATEREDGE_CELL );
        int iyhi = (int)floorf( (maxy - oy) / NAV_WATEREDGE_CELL );
        if ( ixlo < 0 ) ixlo = 0; if ( ixhi >= nx ) ixhi = nx - 1;
        if ( iylo < 0 ) iylo = 0; if ( iyhi >= ny ) iyhi = ny - 1;

        int ix, iy;
        for ( iy = iylo; iy <= iyhi; iy++ ) {
            for ( ix = ixlo; ix <= ixhi; ix++ ) {
                const float px = ox + ix*NAV_WATEREDGE_CELL + NAV_WATEREDGE_CELL*0.5f;
                const float py = oy + iy*NAV_WATEREDGE_CELL + NAV_WATEREDGE_CELL*0.5f;
                /* Point-in-triangle (Quake XY, barycentric). */
                const float d = (qy1 - qy2)*(qx0 - qx2) + (qx2 - qx1)*(qy0 - qy2);
                if ( fabsf( d ) < 1e-6f ) continue;
                const float w0 = ( (qy1 - qy2)*(px - qx2) + (qx2 - qx1)*(py - qy2) ) / d;
                const float w1 = ( (qy2 - qy0)*(px - qx2) + (qx0 - qx2)*(py - qy2) ) / d;
                const float w2 = 1.0f - w0 - w1;
                if ( w0 < -0.001f || w1 < -0.001f || w2 < -0.001f ) continue;
                const float fz = w0*qz0 + w1*qz1 + w2*qz2;
                waterEdgeCell_t *cell = &cells[iy*nx + ix];
                /* Keep the LOWEST covering triangle z (the ground, not an
                 * overhang/wall face) as the anchor the collision scan searches
                 * a tight window around — the exact resting foot comes from the
                 * hull-1 scan, this is only a where-to-look seed. */
                if ( !cell->hasFloor || fz < cell->floorZ ) {
                    cell->floorZ   = fz;
                    cell->hasFloor = 1;
                }
            }
        }
    }
}

static void buildWaterEdgeOmcs( const navGeom_t *geom, navOmcInput_t *out )
{
    if ( !geom || geom->numTris <= 0 || geom->numVerts <= 0 )
        return;

    /* No liquid anywhere on this map => no water seam to reconnect, so every cell
     * scanned below would be traced only to be discarded. That scan is expensive:
     * it rasterises the whole map XY extent and column-traces each covered cell
     * with up to NAV_WATEREDGE_ZSAMPLES CM_BoxTrace calls — and it runs on the MAIN
     * THREAD, before the bake worker spawns, because collision is not thread-safe
     * and cannot move off it. On a large dry arena that is minutes of frozen
     * process during map spawn: the engine looks hung with the connect screen up,
     * since the server never reaches the point of sending a gamestate. One
     * whole-map contents query removes the whole cost. */
    if ( !CM_HasLiquid() ) {
        Com_Log( SEV_DEBUG, LOG_CH(ch_nav_build),
            "water-edge OMC: map has no liquid, skipping the column scan\n" );
        return;
    }

    /* Grid bounds from the geom XY extent (Quake). */
    float minx = 1e30f, maxx = -1e30f, miny = 1e30f, maxy = -1e30f;
    float minz = 1e30f, maxz = -1e30f;
    int   v;
    for ( v = 0; v < geom->numVerts; v++ ) {
        const float qx =  geom->verts[v*3+0];
        const float qy = -geom->verts[v*3+2];
        const float qz =  geom->verts[v*3+1];
        if ( qx < minx ) minx = qx; if ( qx > maxx ) maxx = qx;
        if ( qy < miny ) miny = qy; if ( qy > maxy ) maxy = qy;
        if ( qz < minz ) minz = qz; if ( qz > maxz ) maxz = qz;
    }
    if ( maxx <= minx || maxy <= miny )
        return;

    const float ox = floorf( minx / NAV_WATEREDGE_CELL ) * NAV_WATEREDGE_CELL;
    const float oy = floorf( miny / NAV_WATEREDGE_CELL ) * NAV_WATEREDGE_CELL;
    const int   nx = (int)( (maxx - ox) / NAV_WATEREDGE_CELL ) + 1;
    const int   ny = (int)( (maxy - oy) / NAV_WATEREDGE_CELL ) + 1;
    if ( nx <= 0 || ny <= 0 || (long)nx * ny > 4000000L )
        return;   /* implausible extent — bail rather than over-allocate */

    waterEdgeCell_t *cells =
        (waterEdgeCell_t *)Z_Malloc( nx * ny * (int)sizeof(waterEdgeCell_t) );
    if ( !cells )
        return;
    memset( cells, 0, nx * ny * sizeof(waterEdgeCell_t) );

    waterEdgeRasterFloor( geom, cells, ox, oy, nx, ny );

    /* Column-scan z-window from the geom z-extent (the collision floor lives in
     * this band). */
    const float scanLo = minz - 32.0f;
    const float scanHi = maxz + 32.0f;

    /* Resolve each geom-covered cell to its real hull-1 resting foot (collision is
     * authoritative; the rasterized geom z is only a coverage marker — it can be a
     * wall face or the flat water surface, so it is discarded here).  Keep every
     * standable cell (dry too) so the island graph matches the nav components; mark
     * only liquid-adjacent cells as water.  This is the main-thread collision pass;
     * the worker never runs it. */
    int ix, iy;
    int scanned = 0;
    qboolean overBudget = qfalse;
    s_waterEdgeTraces = 0;
    for ( iy = 0; iy < ny && !overBudget; iy++ ) {
        for ( ix = 0; ix < nx; ix++ ) {
            waterEdgeCell_t *cell = &cells[iy*nx + ix];
            if ( !cell->hasFloor ) continue;
            /* Checked per cell, before the column rather than after: a column is
             * up to 544 traces, so testing afterwards would overshoot by that
             * much every time. */
            if ( s_waterEdgeTraces >= NAV_WATEREDGE_TRACE_BUDGET ) {
                overBudget = qtrue;
                break;
            }
            const float qx = ox + ix*NAV_WATEREDGE_CELL + NAV_WATEREDGE_CELL*0.5f;
            const float qy = oy + iy*NAV_WATEREDGE_CELL + NAV_WATEREDGE_CELL*0.5f;
            float footZ; qboolean water;
            scanned++;
            if ( waterEdgeColumnFloor( qx, qy, scanLo, scanHi, &footZ, &water ) ) {
                cell->floorZ = footZ;
                cell->water  = water ? 1 : 0;
            } else {
                cell->hasFloor = 0;   /* no clean standable collision floor here */
            }
        }
    }

    if ( overBudget ) {
        /* Abandon the pass rather than emit links from a partial scan — see the
         * budget's rationale in nav_local.h. Logged at WARN with the numbers
         * because the alternative, quietly producing fewer links, is
         * indistinguishable from a map that simply has fewer water edges. */
        Com_Log( SEV_WARN, LOG_CH(ch_nav_build),
            "water-edge OMC: trace budget exhausted (%ld traces, %d/%d cells scanned) "
            "— skipping the water-edge pass for this map\n",
            s_waterEdgeTraces, scanned, nx * ny );
        Z_Free( cells );
        return;
    }

    Com_Log( SEV_DEBUG, LOG_CH(ch_nav_build),
        "water-edge OMC: column scan %d cells, %ld traces (budget %ld)\n",
        scanned, s_waterEdgeTraces, (long)NAV_WATEREDGE_TRACE_BUDGET );

    /* Connected components under the climb rule (4-neighbour, |Δz| ≤ climb).
     * Iterative flood fill using the label array as its own work queue is awkward;
     * use a small explicit stack of cell indices. */
    int *stack = (int *)Z_Malloc( nx * ny * (int)sizeof(int) );
    if ( !stack ) { Z_Free( cells ); return; }
    int nextLabel = 0;
    for ( iy = 0; iy < ny; iy++ ) {
        for ( ix = 0; ix < nx; ix++ ) {
            waterEdgeCell_t *seed = &cells[iy*nx + ix];
            if ( !seed->hasFloor || seed->label ) continue;
            nextLabel++;
            int sp = 0;
            stack[sp++] = iy*nx + ix;
            seed->label = nextLabel;
            while ( sp > 0 ) {
                int ci = stack[--sp];
                int cx = ci % nx, cy = ci / nx;
                float fz = cells[ci].floorZ;
                static const int dxs[4] = { 1, -1, 0, 0 };
                static const int dys[4] = { 0, 0, 1, -1 };
                int k;
                for ( k = 0; k < 4; k++ ) {
                    int nxc = cx + dxs[k], nyc = cy + dys[k];
                    if ( nxc < 0 || nxc >= nx || nyc < 0 || nyc >= ny ) continue;
                    waterEdgeCell_t *nb = &cells[nyc*nx + nxc];
                    if ( !nb->hasFloor || nb->label ) continue;
                    if ( fabsf( nb->floorZ - fz ) > NAV_WATEREDGE_CLIMB ) continue;
                    nb->label = nextLabel;
                    stack[sp++] = nyc*nx + nxc;
                }
            }
        }
    }
    Z_Free( stack );

    /* Enumerate cross-component near-water cell pairs, apply the 4 gates, and
     * collapse survivors by the unordered island pair they reconnect.  One link
     * per island pair reconnects the fragmented components (a wade fragmented
     * into a riser staircase spans several island pairs — one link each merges
     * the chain), while the foot-in-water gate keeps it to genuine liquid seams.
     * Each island pair keeps its DEEPEST survivor as the representative endpoint
     * (the point whose PM_SetWaterLevel class decides the link type). */
    #define NAV_WATEREDGE_MAX_LINKS 64
    struct {
        int   la, lb;                 /* island labels (la < lb)             */
        float ax, ay, az, bx, by, bz; /* representative endpoints (deepest)  */
        float deepFoot;               /* deepest foot z seen for this pair   */
        int   waterLevel;             /* PM_SetWaterLevel class at the deep pt */
    } pairs[NAV_WATEREDGE_MAX_LINKS];
    int  numPairs = 0;
    int  surviving = 0;               /* survivor cell-pairs, for diagnostics  */
    int  gPair = 0, gWater = 0, gGap = 0, gFloor = 0, gCol = 0;

    for ( iy = 0; iy < ny; iy++ ) {
        for ( ix = 0; ix < nx; ix++ ) {
            waterEdgeCell_t *a = &cells[iy*nx + ix];
            if ( !a->hasFloor || !a->water ) continue;   /* candidate must be liquid-adjacent */
            const float axq = ox + ix*NAV_WATEREDGE_CELL + NAV_WATEREDGE_CELL*0.5f;
            const float ayq = oy + iy*NAV_WATEREDGE_CELL + NAV_WATEREDGE_CELL*0.5f;

            int dx, dy;
            for ( dy = -NAV_WATEREDGE_PAIR_CELLS; dy <= NAV_WATEREDGE_PAIR_CELLS; dy++ ) {
                int by = iy + dy;
                if ( by < 0 || by >= ny ) continue;
                for ( dx = -NAV_WATEREDGE_PAIR_CELLS; dx <= NAV_WATEREDGE_PAIR_CELLS; dx++ ) {
                    if ( dx == 0 && dy == 0 ) continue;
                    int bx = ix + dx;
                    if ( bx < 0 || bx >= nx ) continue;
                    waterEdgeCell_t *b = &cells[by*nx + bx];
                    if ( !b->hasFloor ) continue;
                    if ( b->label == a->label ) continue;   /* same component: not a reconnection */
                    /* Process each pair once (order the two cell indices). */
                    if ( (by*nx + bx) < (iy*nx + ix) ) continue;
                    gPair++;
                    if ( !b->water ) continue;               /* gate 3: both foot-in-water */
                    gWater++;

                    const float bxq = ox + bx*NAV_WATEREDGE_CELL + NAV_WATEREDGE_CELL*0.5f;
                    const float byq = oy + by*NAV_WATEREDGE_CELL + NAV_WATEREDGE_CELL*0.5f;

                    /* Gate 4: horizontal gap cap. */
                    float gdx = bxq - axq, gdy = byq - ayq;
                    if ( sqrtf( gdx*gdx + gdy*gdy ) > NAV_WATEREDGE_GAP_MAX ) continue;
                    gGap++;
                    /* Gate 1: both stand on real solid floor. */
                    if ( !waterEdgeRealFloor( axq, ayq, a->floorZ ) ) continue;
                    if ( !waterEdgeRealFloor( bxq, byq, b->floorZ ) ) continue;
                    gFloor++;
                    /* Gate 2: clear (non-solid) column between them at rest height. */
                    float restZ = ( a->floorZ > b->floorZ ? a->floorZ : b->floorZ ) + 4.0f;
                    if ( !waterEdgeColumnClear( axq, ayq, bxq, byq, restZ ) ) continue;
                    gCol++;

                    surviving++;

                    /* Fold into the island-pair record (deepest survivor wins). */
                    int la = a->label, lb = b->label;
                    if ( la > lb ) { int tt = la; la = lb; lb = tt; }
                    float deepFoot = ( a->floorZ < b->floorZ ) ? a->floorZ : b->floorZ;

                    int pi, found = -1;
                    for ( pi = 0; pi < numPairs; pi++ ) {
                        if ( pairs[pi].la == la && pairs[pi].lb == lb ) { found = pi; break; }
                    }
                    if ( found < 0 ) {
                        if ( numPairs >= NAV_WATEREDGE_MAX_LINKS ) continue;
                        found = numPairs++;
                        pairs[found].la = la; pairs[found].lb = lb;
                        pairs[found].deepFoot = 1e30f;
                    }
                    if ( deepFoot < pairs[found].deepFoot ) {
                        pairs[found].deepFoot = deepFoot;
                        pairs[found].ax = axq; pairs[found].ay = ayq; pairs[found].az = a->floorZ;
                        pairs[found].bx = bxq; pairs[found].by = byq; pairs[found].bz = b->floorZ;
                    }
                }
            }
        }
    }

    /* Classify each island pair by depth (PM_SetWaterLevel at its deep point) and
     * emit one bidirectional wade link per pair.  This landing is wade-only: a
     * SUBMERGED (swim) survivor is a distinct deep-water body, not the shallow
     * crossing being reconnected — skip it (no swim link this landing). */
    int nWade = 0, nSwimSkipped = 0;
    int pi;
    for ( pi = 0; pi < numPairs && out->count < NAV_MAX_OMC; pi++ ) {
        float dmx = 0.5f*(pairs[pi].ax + pairs[pi].bx);
        float dmy = 0.5f*(pairs[pi].ay + pairs[pi].by);
        int   wl  = waterEdgeWaterLevel( dmx, dmy, pairs[pi].deepFoot );
        pairs[pi].waterLevel = wl;
        if ( wl >= 3 ) { nSwimSkipped++; continue; }   /* swim body — not this crossing */

        navOmcEntry_t *omc = &out->entries[out->count];
        /* Endpoint z = resting origin (foot + |MINS_Z|) so Detour binds it to the
         * standable floor poly, matching the door-gap / plat placement. */
        omc->start[0] = pairs[pi].ax; omc->start[1] = pairs[pi].ay;
        omc->start[2] = pairs[pi].az + NAV_WATEREDGE_MINS_Z;
        omc->end[0]   = pairs[pi].bx; omc->end[1]   = pairs[pi].by;
        omc->end[2]   = pairs[pi].bz + NAV_WATEREDGE_MINS_Z;
        omc->radius = 32.0f;
        omc->area   = (unsigned char)NAVAREA_WATER;
        omc->flags  = (unsigned short)(NAVPOLY_WALKABLE | NAVPOLY_WATER | NAVPOLY_OFFMESH);
        omc->bidir  = 1;   /* a wade crossing is traversed both ways */
        omc->traversalMode = (unsigned char)NAV_TM_WALK;   /* wade: the bot walks through shallow water */
        out->count++;
        nWade++;

        Com_Log( SEV_INFO, LOG_CH(ch_nav_build),
            "water-edge OMC %d: wade islands(%d,%d) start=(%.0f,%.0f,%.0f) end=(%.0f,%.0f,%.0f) waterlevel=%d\n",
            out->count - 1, pairs[pi].la, pairs[pi].lb,
            omc->start[0], omc->start[1], omc->start[2],
            omc->end[0], omc->end[1], omc->end[2], wl );
    }

    Com_Log( SEV_INFO, LOG_CH(ch_nav_build),
        "water-edge: %d island-pairs survived the gates -> %d wade link(s) (%d deep-water skipped)\n",
        numPairs, nWade, nSwimSkipped );
    Com_Log( SEV_DEBUG, LOG_CH(ch_nav_build),
        "water-edge gates: cross-island-pairs=%d ->foot-in-water=%d ->gap-cap=%d ->real-floor=%d ->clear-column=%d (survivor cell-pairs=%d)\n",
        gPair, gWater, gGap, gFloor, gCol, surviving );

    #undef NAV_WATEREDGE_MAX_LINKS
    Z_Free( cells );
}

/* -------------------------------------------------------------------------
   Hatch-descent off-mesh connection — the door-gap producer's VERTICAL sibling.

   Detects a door that gates a VERTICAL floor discontinuity (upper floor beside
   the door footprint, a standable landing a genuine shaft-depth below through it)
   and emits ONE one-way DROP OMC launched at the upper lip EDGE — the near-
   vertical fall the trajectory validator proved PASSes only from the edge, not
   the poly centroid.  Door-keyed automatically by Nav_TagDoorAreas (the OMC
   midpoint sits under the door centre → tagged NAVAREA_DOOR + added to the
   door's poly set → closed door sets OPENABLE_CLOSED so the bot plans through it
   and opens it).  Runs on the main thread (CM_BoxTrace legal), reusing the
   water-edge collision helpers (waterEdgePointSolid / waterEdgeColumnFloor).
   ------------------------------------------------------------------------- */

/* Find a standable landing directly below (qx,qy), scanning the column from
 * (upperZ - MIN_DROP) down to (upperZ - MAX_DROP).  Reuses the water-edge column
 * scan (lowest solid→clear transition with full standing headroom).  Returns the
 * foot z of the deepest standable landing in that band, or qfalse if none. */
static qboolean hatchLandingBelow( float qx, float qy, float upperZ, float *outFootZ )
{
    const float zHi = upperZ - NAV_HATCH_MIN_DROP;   /* at least a shaft, not a step */
    const float zLo = upperZ - NAV_HATCH_MAX_DROP;   /* no deeper than a designed drop */
    if ( zHi <= zLo )
        return qfalse;
    /* waterEdgeColumnFloor returns the LOWEST standable foot with headroom in
     * [zLo,zHi]; the liquid flag is unused here (a landing in water still lands). */
    qboolean water = qfalse;
    return waterEdgeColumnFloor( qx, qy, zLo, zHi, outFootZ, &water );
}

/* Emit hatch-descent OMCs.  One one-way drop per door whose two sides show a
 * vertical floor discontinuity (upper floor beside the footprint; a standable
 * landing >CLIMB below through it), launched at the upper lip edge and validated
 * as a clear near-vertical fall to a standable landing. */
static void buildHatchDescentOmcs( const entDef_t *defs, int numDefs,
                                   const mapFile_t *bsp, const navGeom_t *geom,
                                   navOmcInput_t *out )
{
    int i;
    if ( !geom || geom->numTris <= 0 )
        return;   /* need floor geom to find the upper lip */

    for ( i = 0; i < numDefs && out->count < NAV_MAX_OMC; i++ ) {
        const entDef_t *e = &defs[i];
        const char *cn = Ent_Get( e, "classname" );
        const char *bare = omcBareClass( cn );
        if ( Q_stricmp( bare, "func_door" ) != 0 &&
             Q_stricmp( bare, "func_door_secret" ) != 0 )
            continue;

        const char *model = Ent_Get( e, "model" );
        if ( !model || model[0] != '*' ) continue;
        int idx = atoi( model + 1 );
        if ( idx <= 0 || idx >= bsp->numSubModels ) continue;

        const dmodel_t *dm = &bsp->subModels[idx];
        const float dmnx = dm->mins[0], dmxx = dm->maxs[0];
        const float dmny = dm->mins[1], dmxy = dm->maxs[1];
        const float dtopZ = dm->maxs[2];   /* door leaf top — near the upper storey */
        const float cX = 0.5f*(dmnx+dmxx);
        const float cY = 0.5f*(dmny+dmxy);

        /* Candidate LIP points: just OUTSIDE the door footprint on each of the four
         * sides, at the upper storey (a window of the leaf top).  For each, the
         * DROP is NEAR-VERTICAL: the landing is probed directly below the lip (the
         * measured PASS zone is a near-vertical fall — a landing offset far from the
         * lip clips the shaft wall).  Deterministic side order (+X,-X,+Y,-Y); the
         * first side that yields a genuine, clear, standable shaft wins.  This picks
         * the side the bot actually steps off (real collision floor at the lip),
         * not merely the highest geom sample. */
        const float uzLo = dtopZ - NAV_HATCH_LIP_Z_WINDOW;
        const float uzHi = dtopZ + NAV_HATCH_LIP_Z_WINDOW;
        const float halfX = 0.5f*(dmxx-dmnx), halfY = 0.5f*(dmxy-dmny);
        /* Lip just OUTSIDE the footprint edge (on the upper solid floor); landing just
         * INSIDE the footprint edge (in the shaft mouth).  Both hug the SAME edge so
         * the fall is NEAR-VERTICAL (their horizontal separation is one small edge
         * inset either way — inside the measured PASS zone; a landing across the
         * footprint would clip the shaft wall). */
        const float edgeIn = NAV_HATCH_LIP_INSET;   /* small step either side of the edge */
        const float outX = halfX + edgeIn, edgeX = halfX - edgeIn;
        const float outY = halfY + edgeIn, edgeY = halfY - edgeIn;
        struct { float lx, ly, ex, ey; } cand[4] = {
            { cX + outX, cY, cX + (edgeX > 0 ? edgeX : 0), cY },   /* +X lip, land at near shaft edge */
            { cX - outX, cY, cX - (edgeX > 0 ? edgeX : 0), cY },   /* -X */
            { cX, cY + outY, cX, cY + (edgeY > 0 ? edgeY : 0) },   /* +Y */
            { cX, cY - outY, cX, cY - (edgeY > 0 ? edgeY : 0) } }; /* -Y */

        float lipX=0, lipY=0, lipZ=0, endX=0, endY=0, landZ=0, drop=0;
        int   chosen = -1;
        for ( int s = 0; s < 4; s++ ) {
            /* Lip: the geom marks WHERE an upper floor is; the collision column scan
             * gives the standable FOOT there (same authority as the landing, so lip
             * and landing z are in one coordinate system — the resting foot).  Search
             * a window around the geom surface for the collision foot. */
            float geomZ;
            if ( !floorZAt( geom, cand[s].lx, cand[s].ly, uzLo, uzHi, &geomZ ) ) continue;
            float upZ; qboolean lipWater = qfalse;
            if ( !waterEdgeColumnFloor( cand[s].lx, cand[s].ly,
                                        geomZ - NAV_HATCH_LIP_Z_WINDOW,
                                        geomZ + NAV_HATCH_HEADROOM, &upZ, &lipWater ) )
                continue;
            /* Landing: collision foot directly below the near-edge point in the shaft. */
            float lz;
            if ( !hatchLandingBelow( cand[s].ex, cand[s].ey, upZ, &lz ) ) continue;
            const float d = upZ - lz;
            if ( d < NAV_HATCH_MIN_DROP || d > NAV_HATCH_MAX_DROP ) continue;
            /* Near-vertical gate: the lip→landing horizontal offset must be within the
             * measured PASS zone (~one agent radius).  A larger offset would clip the
             * shaft wall on the way down (the validator FAILed those arcs).  This is
             * the geometric form of "the fall clears the shaft"; we do NOT collision-
             * probe the shaft column because the shaft IS the doorway — the door brush
             * (excluded from the Q1 nav soup but SOLID in the closed collision model)
             * would false-block it, and this OMC is traversed only when the door is
             * OPEN (the door-keyed gating guarantees that).  Same trust the door-gap
             * producer places in the open doorway. */
            const float hoff = sqrtf( (cand[s].ex-cand[s].lx)*(cand[s].ex-cand[s].lx)
                                    + (cand[s].ey-cand[s].ly)*(cand[s].ey-cand[s].ly) );
            if ( hoff > NAV_HATCH_MAX_HOFFSET ) continue;   /* not near-vertical — would clip the shaft wall */
            /* COVER vs LEAF discriminator.  A hatch-descent candidate is invalid when
             * the mover's OWN TOP surface lies at or below the lip's standing foot:
             * such a mover is a floor COVER — closed, its top IS the ground the agent
             * stands on at the lip, so no gravity drop can begin there regardless of
             * what lies beneath it.  A mover whose top is ABOVE the lip foot is a LEAF
             * beside an opening (the agent stands on real floor next to it) and stays
             * eligible.  This compares two heights we already hold — the door submodel
             * top (dtopZ) and the collision lip foot (upZ) — and never traces the
             * shaft, so the "don't collision-probe the shaft, the closed door would
             * false-block it" rule above stays intact.  Height relation only: no
             * footprint-size threshold, no map constant, no tolerance term. */
            if ( dtopZ <= upZ ) continue;   /* floor cover, not a leaf beside an opening */
            lipX=cand[s].lx; lipY=cand[s].ly; lipZ=upZ;
            endX=cand[s].ex; endY=cand[s].ey; landZ=lz; drop=d; chosen=s;
            break;
        }
        if ( chosen < 0 )
            continue;   /* no side yields a clear near-vertical shaft — not a hatch */

        navOmcEntry_t *omc = &out->entries[out->count];
        omc->start[0] = lipX; omc->start[1] = lipY; omc->start[2] = lipZ  + NAV_HATCH_MINS_Z;
        omc->end[0]   = endX; omc->end[1]   = endY; omc->end[2]   = landZ + NAV_HATCH_MINS_Z;
        omc->radius = 32.0f;
        omc->area   = (unsigned char)NAVAREA_JUMP_LINK;
        omc->flags  = (unsigned short)(NAVPOLY_WALKABLE | NAVPOLY_OFFMESH);
        omc->bidir  = 0;   /* one-way: drop down the shaft, no climb back up */
        omc->traversalMode = (unsigned char)NAV_TM_FALL;   /* hatch: a gravity drop off the lip */
        out->count++;

        Com_Log( SEV_INFO, LOG_CH(ch_nav_build),
            "hatch-descent OMC %d: door model=*%d drop=%.0fu lip=(%.0f,%.0f,%.0f) -> land=(%.0f,%.0f,%.0f)\n",
            out->count - 1, idx, drop,
            omc->start[0], omc->start[1], omc->start[2],
            omc->end[0], omc->end[1], omc->end[2] );
    }
}


/* -------------------------------------------------------------------------
   Public API
   ------------------------------------------------------------------------- */

void Nav_OMC_Build( const struct mapFile_s *bsp, const navGeom_t *geom,
                    navOmcInput_t *out )
{
    memset( out, 0, sizeof(*out) );

    if ( !bsp || !bsp->entityString )
        return;

    int numDefs = 0;
    entDef_t *defs = parseEntities( bsp->entityString, &numDefs );
    if ( !defs )
        return;

    int c0 = out->count;
    buildJumpPads(       defs, numDefs, bsp, out );
    int nJumpPads    = out->count - c0; c0 = out->count;
    buildTeleporters(    defs, numDefs, bsp, out );
    int nTeleporters = out->count - c0; c0 = out->count;
    buildTargetPushOmcs( defs, numDefs, bsp, out );
    int nTargetPush  = out->count - c0; c0 = out->count;
    buildPlatforms(      bsp, out );
    int nPlatforms   = out->count - c0; c0 = out->count;
    buildDoorGapOmcs(    defs, numDefs, bsp, geom, out );
    int nDoorGaps    = out->count - c0; c0 = out->count;
    buildHatchDescentOmcs( defs, numDefs, bsp, geom, out );
    int nHatches     = out->count - c0; c0 = out->count;
    buildWaterEdgeOmcs(  geom, out );
    int nWaterEdge   = out->count - c0;

    Com_Log( SEV_INFO, LOG_CH(ch_nav_build), "OMC: %d trigger_push, %d trigger_teleport, "
                "%d target_push, %d func_plat, %d door_gap, %d hatch_descent, %d water_edge  (total %d)\n",
                nJumpPads, nTeleporters, nTargetPush, nPlatforms, nDoorGaps, nHatches, nWaterEdge, out->count );

    Z_Free( defs );
}

/* -------------------------------------------------------------------------
   Nav_RegistryAudit — read-only causal-registry report (openable traversal)

   For each activator (func_button + q1_ prefix, and target-carrying triggers)
   resolve its target -> the mover(s) with that targetname, report the mover
   class + submodel AABB, and classify the causality. NO nav state touched; a
   pure entity-lump report built from the same resolvers the OMC builders use.
   ------------------------------------------------------------------------- */

/* Format-agnostic classname compare: strip an optional "q1_" prefix, then match
   the bare name (mirrors buildDoorGapOmcs' rule). */
static const char *NavReg_Bare( const char *cn )
{
    if ( !cn ) return "";
    if ( Q_stricmpn( cn, "q1_", 3 ) == 0 ) return cn + 3;
    return cn;
}

static qboolean NavReg_IsMover( const char *bare )
{
    return ( Q_stricmp( bare, "func_door" ) == 0 ||
             Q_stricmp( bare, "func_door_secret" ) == 0 ||
             Q_stricmp( bare, "func_plat" ) == 0 ||
             Q_stricmp( bare, "func_train" ) == 0 ) ? qtrue : qfalse;
}

static qboolean NavReg_IsActivator( const char *bare )
{
    return ( Q_stricmp( bare, "func_button" ) == 0 ||
             Q_stricmp( bare, "trigger_multiple" ) == 0 ||
             Q_stricmp( bare, "trigger_once" ) == 0 ||
             Q_stricmp( bare, "trigger_counter" ) == 0 ) ? qtrue : qfalse;
}

/* Count how many activators target a given targetname (multi-button detection). */
static int NavReg_CountActivatorsTargeting( const entDef_t *defs, int numDefs,
                                            const char *tname )
{
    int i, n = 0;
    if ( !tname || !tname[0] ) return 0;
    for ( i = 0; i < numDefs; i++ ) {
        const char *cn = NavReg_Bare( Ent_Get( &defs[i], "classname" ) );
        if ( !NavReg_IsActivator( cn ) ) continue;
        const char *t = Ent_Get( &defs[i], "target" );
        if ( t && strcmp( t, tname ) == 0 ) n++;
    }
    return n;
}

void Nav_RegistryAudit( const char *mapname )
{
    mapFile_t *bsp = NULL;
    char bspPath[MAX_QPATH];
    Com_sprintf( bspPath, sizeof(bspPath), "maps/%s.bsp", mapname );

    if ( !Map_Load( bspPath, &bsp, MAP_LOAD_FLAGS_NONE ) || !bsp ) {
        Com_Log( SEV_INFO, LOG_CH(ch_nav_build), "[REGISTRY] map=%s: load failed\n", mapname );
        return;
    }
    if ( !bsp->entityString ) {
        Com_Log( SEV_INFO, LOG_CH(ch_nav_build), "[REGISTRY] map=%s: no entity string\n", mapname );
        Map_Free( bsp );
        return;
    }

    int numDefs = 0;
    entDef_t *defs = parseEntities( bsp->entityString, &numDefs );
    if ( !defs ) { Map_Free( bsp ); return; }

    Com_Log( SEV_INFO, LOG_CH(ch_nav_build),
        "[REGISTRY] map=%s entities=%d\n", mapname, numDefs );
    Com_Log( SEV_INFO, LOG_CH(ch_nav_build),
        "[REGISTRY] cols: activator(class) target -> mover(class) modelIdx aabb | class\n" );

    int nMovers = 0, nButtons = 0, nChains = 0, nAmbiguous = 0, nOrphanMover = 0;

    /* Pass 1: activator -> mover chains. */
    for ( int i = 0; i < numDefs; i++ ) {
        const char *cnRaw = Ent_Get( &defs[i], "classname" );
        const char *cn    = NavReg_Bare( cnRaw );
        if ( !NavReg_IsActivator( cn ) ) continue;
        if ( Q_stricmp( cn, "func_button" ) == 0 ) nButtons++;

        /* Activator world centre (button brush or trigger volume centre) — the
         * approach point the bot must reach to press it. */
        float actC[3]; entCentre( &defs[i], bsp, actC );

        const char *target = Ent_Get( &defs[i], "target" );
        if ( !target || !target[0] ) {
            Com_Log( SEV_INFO, LOG_CH(ch_nav_build),
                "[REGISTRY] %-16s at(%.0f %.0f %.0f) (no target) | NO_TARGET\n",
                cn, actC[0], actC[1], actC[2] );
            continue;
        }

        /* Resolve every mover this target names (a target may fan out). */
        int resolved = 0;
        for ( int j = 0; j < numDefs; j++ ) {
            const char *mtn = Ent_Get( &defs[j], "targetname" );
            if ( !mtn || strcmp( mtn, target ) != 0 ) continue;
            const char *mcnRaw = Ent_Get( &defs[j], "classname" );
            const char *mcn    = NavReg_Bare( mcnRaw );
            if ( !NavReg_IsMover( mcn ) ) continue;   /* target is a non-mover (relay/counter) */

            const char *model = Ent_Get( &defs[j], "model" );
            int idx = ( model && model[0] == '*' ) ? atoi( model + 1 ) : -1;
            float mn[3] = {0,0,0}, mx[3] = {0,0,0};
            if ( idx > 0 && idx < bsp->numSubModels ) {
                const dmodel_t *dm = &bsp->subModels[idx];
                mn[0]=dm->mins[0]; mn[1]=dm->mins[1]; mn[2]=dm->mins[2];
                mx[0]=dm->maxs[0]; mx[1]=dm->maxs[1]; mx[2]=dm->maxs[2];
            }

            /* Causality class. */
            int nAct = NavReg_CountActivatorsTargeting( defs, numDefs, target );
            const char *cls = ( nAct > 1 ) ? "MULTI_BUTTON" : "SIMPLE";
            if ( nAct > 1 ) nAmbiguous++;

            Com_Log( SEV_INFO, LOG_CH(ch_nav_build),
                "[REGISTRY] %-16s at(%.0f %.0f %.0f) '%s' -> %-16s *%d aabb(%.0f %.0f %.0f)-(%.0f %.0f %.0f) | %s\n",
                cn, actC[0], actC[1], actC[2], target, mcn, idx,
                mn[0],mn[1],mn[2], mx[0],mx[1],mx[2], cls );
            resolved++;
            nChains++;
        }

        if ( resolved == 0 ) {
            /* Target names a non-mover (relay/counter chain) or a dangling name. */
            const entDef_t *tgt = findByTargetname( defs, numDefs, target );
            const char *tcn = tgt ? NavReg_Bare( Ent_Get( tgt, "classname" ) ) : NULL;
            const char *cls = !tgt ? "UNRESOLVED"
                            : ( tcn && Q_stricmp( tcn, "trigger_counter" ) == 0 ) ? "COUNTER"
                            : ( tcn && Q_stricmp( tcn, "target_delay" ) == 0 )    ? "RELAY"
                            : "INDIRECT";
            Com_Log( SEV_INFO, LOG_CH(ch_nav_build),
                "[REGISTRY] %-16s '%s' -> %s | %s\n",
                cn, target, tcn ? tcn : "(none)", cls );
            nAmbiguous++;
        }
    }

    /* Pass 2: movers with NO activator targeting them (always-open / auto / orphan). */
    for ( int j = 0; j < numDefs; j++ ) {
        const char *mcn = NavReg_Bare( Ent_Get( &defs[j], "classname" ) );
        if ( !NavReg_IsMover( mcn ) ) continue;
        nMovers++;
        const char *mtn = Ent_Get( &defs[j], "targetname" );
        int nAct = ( mtn && mtn[0] ) ? NavReg_CountActivatorsTargeting( defs, numDefs, mtn ) : 0;
        if ( nAct == 0 ) {
            const char *model = Ent_Get( &defs[j], "model" );
            int idx = ( model && model[0] == '*' ) ? atoi( model + 1 ) : -1;
            /* A plat is rider-triggered (touch), a door may be trigger-touch or
               shootable; an unnamed/untargeted mover is auto/proximity, not orphan. */
            const char *cls = ( Q_stricmp( mcn, "func_plat" ) == 0 ) ? "PLAT_TOUCH"
                            : ( mtn && mtn[0] ) ? "ORPHAN_NAMED" : "AUTO_TOUCH";
            Com_Log( SEV_INFO, LOG_CH(ch_nav_build),
                "[REGISTRY] mover %-16s *%d targetname='%s' activators=0 | %s\n",
                mcn, idx, mtn ? mtn : "", cls );
            if ( mtn && mtn[0] ) nOrphanMover++;
        }
    }

    Com_Log( SEV_INFO, LOG_CH(ch_nav_build),
        "[REGISTRY] --- summary map=%s movers=%d buttons=%d chains=%d ambiguous=%d orphan-named-movers=%d ---\n",
        mapname, nMovers, nButtons, nChains, nAmbiguous, nOrphanMover );

    Z_Free( defs );
    Map_Free( bsp );
}

#endif /* FEAT_RECAST_NAVMESH */
