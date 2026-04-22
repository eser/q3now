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

#if FEAT_RECAST_NAVMESH

#include "../qcommon.h"
#include "../maps/bsp.h"
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

static void entCentre( const entDef_t *e, const bspFile_t *bsp, float out[3] )
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
            if ( strcmp( cn, kTriggerClasses[j] ) == 0 ) {
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
                            const bspFile_t *bsp, navOmcInput_t *out )
{
    int i;
    for ( i = 0; i < numDefs && out->count < NAV_MAX_OMC; i++ ) {
        const entDef_t *e = &defs[i];
        const char *cn = Ent_Get( e, "classname" );
        if ( !cn || strcmp( cn, "trigger_push" ) != 0 ) continue;

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
        out->count++;
    }
}

/* -------------------------------------------------------------------------
   Teleporter (trigger_teleport)
   ------------------------------------------------------------------------- */

static void buildTeleporters( const entDef_t *defs, int numDefs,
                               const bspFile_t *bsp, navOmcInput_t *out )
{
    int i;
    for ( i = 0; i < numDefs && out->count < NAV_MAX_OMC; i++ ) {
        const entDef_t *e = &defs[i];
        const char *cn = Ent_Get( e, "classname" );
        if ( !cn || strcmp( cn, "trigger_teleport" ) != 0 ) continue;

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
                                  const bspFile_t *bsp, navOmcInput_t *out )
{
    int i;
    for ( i = 0; i < numDefs && out->count < NAV_MAX_OMC; i++ ) {
        const entDef_t *e = &defs[i];
        const char *cn = Ent_Get( e, "classname" );
        if ( !cn || strcmp( cn, "target_push" ) != 0 ) continue;

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
        out->count++;
    }
}

/* -------------------------------------------------------------------------
   func_plat — bidirectional elevator.
   Endpoints verified against g_mover.c SP_func_plat lines 1054-1057:
     pos2 = s.origin  (rest/upper position — where the brush is placed in BSP)
     pos1 = pos2.z - height  (lower position — where plat descends to board)
   The BSP model is compiled at the rest/upper position (pos2), so:
     entity "origin" key = pos2.z (upper exit position)
     lower boarding position = pos2.z - height
   Travel height: from "height" entity key, or (model_Z_span - lip) where
   lip defaults to 8 Q3u per Q3 func_plat convention.

   DEFERRED: func_plat OMC endpoint verification — assumed start = lower
   position, end = entity origin.  Verify actual bot traversal behaviour
   during Phase 4 elevator testing.
   ------------------------------------------------------------------------- */

static void buildPlatforms( const entDef_t *defs, int numDefs,
                             const bspFile_t *bsp, navOmcInput_t *out )
{
    int i;
    for ( i = 0; i < numDefs && out->count < NAV_MAX_OMC; i++ ) {
        const entDef_t *e = &defs[i];
        const char *cn = Ent_Get( e, "classname" );
        if ( !cn || strcmp( cn, "func_plat" ) != 0 ) continue;

        const char *model = Ent_Get( e, "model" );
        if ( !model || model[0] != '*' ) continue;
        int idx = atoi( model + 1 );
        if ( idx <= 0 || idx >= bsp->numSubModels ) continue;

        const dmodel_t *dm = &bsp->subModels[idx];
        float cx = (dm->mins[0] + dm->maxs[0]) * 0.5f;
        float cy = (dm->mins[1] + dm->maxs[1]) * 0.5f;

        /* pos2.z (upper/rest) = entity origin Z (g_mover.c: pos2 = s.origin). */
        float org[3] = { 0.0f, 0.0f, 0.0f };
        parseVec3( Ent_Get( e, "origin" ), org );
        float czHigh = org[2]; /* pos2.z — upper exit position */

        /* Travel height: from "height" key, or (model Z span - lip). */
        float heightVal;
        const char *hkey = Ent_Get( e, "height" );
        if ( hkey && hkey[0] ) {
            heightVal = (float)atof( hkey );
        } else {
            float lip = 8.0f; /* Q3 func_plat default lip */
            const char *lkey = Ent_Get( e, "lip" );
            if ( lkey ) lip = (float)atof( lkey );
            heightVal = (dm->maxs[2] - dm->mins[2]) - lip;
        }
        if ( heightVal <= 0.0f ) continue; /* degenerate — skip */

        /* pos1.z (lower/boarding) = pos2.z - height (g_mover.c: pos1[2] -= height). */
        float czLow = czHigh - heightVal;

        float start[3], end[3];
        start[0] = cx;    start[1] = cy;    start[2] = czLow;
        end[0]   = cx;    end[1]   = cy;    end[2]   = czHigh;

        navOmcEntry_t *omc = &out->entries[out->count];
        memcpy( omc->start, start, sizeof(start) );
        memcpy( omc->end,   end,   sizeof(end) );
        omc->radius = 32.0f;
        omc->area   = (unsigned char)NAVAREA_JUMP_LINK;
        omc->flags  = (unsigned short)(NAVPOLY_WALKABLE | NAVPOLY_OFFMESH);
        omc->bidir  = 1; /* bidirectional: agents can ride up or down */
        out->count++;
    }
}

/* -------------------------------------------------------------------------
   Public API
   ------------------------------------------------------------------------- */

void Nav_OMC_Build( const struct bspFile_s *bsp, navOmcInput_t *out )
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
    buildPlatforms(      defs, numDefs, bsp, out );
    int nPlatforms   = out->count - c0;

    Com_Printf( "[NAV] OMC: %d trigger_push, %d trigger_teleport, "
                "%d target_push, %d func_plat  (total %d)\n",
                nJumpPads, nTeleporters, nTargetPush, nPlatforms, out->count );

    Z_Free( defs );
}

#endif /* FEAT_RECAST_NAVMESH */
