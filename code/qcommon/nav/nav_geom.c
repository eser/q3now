// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
===========================================================================
nav_geom.c -- Geometry orchestration layer.

Responsibilities:
  - Nav_Geom_GetChecksum: fast BSP checksum read (no geometry extraction).
  - Nav_Geom_Extract: full extraction via BSP abstraction callback +
    off-mesh connection assembly.  Called only on cache miss.
  - Nav_Geom_Free: LIFO Hunk_FreeTempMemory (areas → tris → verts).
  - Nav_OMC_Free: zeroes the fixed-size navOmcInput_t struct.

Two-pass allocation pattern (mirrors tr_map.c):
  Pass 1: iterate surfaces, count output verts and tris.
  Pass 2: Hunk_AllocateTempMemory at exact sizes (verts → tris → areas),
          then iterate surfaces again and fill.
Free order MUST be LIFO: areas → tris → verts.
===========================================================================
*/

#include "../q_shared.h"
#include "../q_feats.h"
LOG_DECLARE_CHANNEL( ch_nav_build, "nav.build" );

#if FEAT_RECAST_NAVMESH

#include "../qcommon.h"
#include "../maps/map_format_registry.h"
#include "nav_local.h"

/*
 * Nav_Geom_GetChecksum
 * Fast path: load BSP, read checksum, free BSP.
 * Does NOT extract geometry — safe to call every map load.
 * Returns qfalse if Map_Load fails (out is unchanged).
 */
qboolean Nav_Geom_GetChecksum( const char *mapname, int *out )
{
    mapFile_t *bsp = NULL;
    char bspPath[MAX_QPATH];
    Com_sprintf( bspPath, sizeof(bspPath), "maps/%s.bsp", mapname );

    if ( !Map_Load( bspPath, &bsp, MAP_LOAD_FLAGS_NONE ) || !bsp )
        return qfalse;

    *out = bsp->checksum;
    Map_Free( bsp );
    return qtrue;
}

/*
 * Nav_Geom_Extract
 * Slow path: Map_Load → extractNavGeometry callback → Nav_OMC_Build.
 * Allocates geomOut arrays with Hunk_AllocateTempMemory (two-pass inside
 * the format callback).
 * Returns qfalse if Map_Load fails or the format does not support nav.
 */
qboolean Nav_Geom_Extract( const char *mapname,
                            navGeom_t     *geomOut,
                            navOmcInput_t *omcOut )
{
    mapFile_t *bsp = NULL;
    char bspPath[MAX_QPATH];
    Com_sprintf( bspPath, sizeof(bspPath), "maps/%s.bsp", mapname );

    if ( !Map_Load( bspPath, &bsp, MAP_LOAD_FLAGS_NONE ) || !bsp ) {
        Com_Log( SEV_WARN, LOG_CH(ch_nav_build), "Nav_Geom_Extract: Map_Load failed for '%s'\n", mapname );
        return qfalse;
    }

    memset( geomOut, 0, sizeof(*geomOut) );

    /* Origin-space bake input: the walkable geometry comes from the LIVE
     * canonical collision brushes (the same set the runtime tracer uses). The
     * fresh `bsp` above stays loaded for the OMC producers (entities/movers), but
     * the mesh geometry no longer comes from its visual surfaces — mesh and trace
     * share one collision source. Extraction runs on the main thread while the
     * map's cm is loaded, so reading cm/cmCanon here is safe. */
    {
        float         *qverts = NULL;
        int            *tris   = NULL;
        unsigned char  *areas  = NULL;
        int             nv = 0, nt = 0;
        int _t = Sys_Milliseconds();

        if ( !CM_BuildNavGeometry( &qverts, &nv, &tris, &areas, &nt ) || nt == 0 ) {
            Com_Log( SEV_WARN, LOG_CH(ch_nav_build),
                     "Nav_Geom_Extract: canonical nav geometry empty for '%s'\n", mapname );
            if ( qverts || tris || areas ) CM_FreeNavGeometry( qverts, tris, areas );
            Map_Free( bsp );
            return qfalse;
        }
        Com_Log( SEV_INFO, LOG_CH(ch_nav_build), "[NAVGEOM-T] CM_BuildNavGeometry %d ms\n", Sys_Milliseconds() - _t );

        /* Copy into Hunk temp arrays (LIFO: verts, tris, areas — matching
         * Nav_Geom_Free), converting verts Quake->Recast. */
        geomOut->verts = (float *)        Hunk_AllocateTempMemory( nv * 3 * (int)sizeof(float) );
        geomOut->tris  = (int *)          Hunk_AllocateTempMemory( nt * 3 * (int)sizeof(int) );
        geomOut->areas = (unsigned char *)Hunk_AllocateTempMemory( nt      * (int)sizeof(unsigned char) );
        for ( int i = 0; i < nv; i++ ) {
            float r[3];
            Nav_QuakeToRecast( &qverts[i*3], r );
            geomOut->verts[i*3+0] = r[0];
            geomOut->verts[i*3+1] = r[1];
            geomOut->verts[i*3+2] = r[2];
        }
        memcpy( geomOut->tris,  tris,  nt * 3 * sizeof(int) );
        memcpy( geomOut->areas, areas, nt      * sizeof(unsigned char) );
        geomOut->numVerts = nv;
        geomOut->numTris  = nt;

        CM_FreeNavGeometry( qverts, tris, areas );
    }

    memset( omcOut, 0, sizeof(*omcOut) );
    Nav_OMC_Build( bsp, geomOut, omcOut );

    Map_Free( bsp );
    return qtrue;
}

/*
 * Nav_Geom_Free
 * Release geometry arrays allocated by the two-pass extractor.
 * LIFO order: areas → tris → verts (Hunk_FreeTempMemory is stack-based).
 */
void Nav_Geom_Free( navGeom_t *geom )
{
    if ( !geom ) return;
    if ( geom->areas ) { Hunk_FreeTempMemory( geom->areas ); geom->areas  = NULL; }
    if ( geom->tris  ) { Hunk_FreeTempMemory( geom->tris  ); geom->tris   = NULL; }
    if ( geom->verts ) { Hunk_FreeTempMemory( geom->verts ); geom->verts  = NULL; }
    geom->numVerts = 0;
    geom->numTris  = 0;
}

/*
 * Nav_Geom_HeapCopy
 * Deep-copy an extracted geom (whose arrays live on the shared main-thread Hunk
 * TEMP stack) into a self-contained heap (Z_Malloc) copy. The bake reads only this
 * copy, so it never touches Hunk temp — which the main thread resets on the very
 * next spawn phase (FS_ReadFile → Hunk_ClearTempMemory). Returns qtrue on success;
 * on failure *out is left zeroed and nothing is allocated. Free with
 * Nav_Geom_HeapFree. numVerts/numTris and the OMC (a value struct, no pointers)
 * are copied by the caller alongside.
 */
qboolean Nav_Geom_HeapCopy( const navGeom_t *src, navGeom_t *out )
{
    size_t vBytes, tBytes, aBytes;

    memset( out, 0, sizeof( *out ) );
    if ( !src || src->numTris <= 0 || src->numVerts <= 0 ) {
        return qfalse;
    }

    vBytes = (size_t)src->numVerts * 3 * sizeof( float );
    tBytes = (size_t)src->numTris  * 3 * sizeof( int );
    aBytes = (size_t)src->numTris  * sizeof( unsigned char );

    out->verts = (float *)Z_Malloc( vBytes );
    out->tris  = (int   *)Z_Malloc( tBytes );
    out->areas = (unsigned char *)Z_Malloc( aBytes );

    memcpy( out->verts, src->verts, vBytes );
    memcpy( out->tris,  src->tris,  tBytes );
    memcpy( out->areas, src->areas, aBytes );
    out->numVerts = src->numVerts;
    out->numTris  = src->numTris;
    return qtrue;
}

/*
 * Nav_Geom_HeapFree
 * Free a heap geom made by Nav_Geom_HeapCopy (Z_Free, NOT the Hunk path).
 */
void Nav_Geom_HeapFree( navGeom_t *geom )
{
    if ( !geom ) return;
    if ( geom->areas ) { Z_Free( geom->areas ); geom->areas = NULL; }
    if ( geom->tris  ) { Z_Free( geom->tris  ); geom->tris  = NULL; }
    if ( geom->verts ) { Z_Free( geom->verts ); geom->verts = NULL; }
    geom->numVerts = 0;
    geom->numTris  = 0;
}

/*
 * Nav_OMC_Free
 * Zero the navOmcInput_t.  Currently a memset; forward-compatible for
 * future heap-allocated OMC entries (the caller must free before zeroing).
 */
void Nav_OMC_Free( navOmcInput_t *omc )
{
    if ( omc )
        memset( omc, 0, sizeof(*omc) );
}

/* -------------------------------------------------------------------------
   Nav_Get_DoorBoxes
   Parse the BSP entity string for func_door entities and return their
   world-space AABBs.  Q3 BSP compiles brush entity geometry into world
   space, so subModels[N].mins/maxs are already in world units.
   Returns Z_Malloc'd array (caller must Z_Free(*outBoxes)); count on return.
   Returns 0 if no func_door entities or on BSP load failure.
   ------------------------------------------------------------------------- */

/* Minimal entity key/value parser — local to this function, no deps. */
static const char *DoorEnt_ReadKey( const char **src, char *buf, int bufLen )
{
    const char *s = *src;
    while ( *s && *s != '"' ) s++;
    if ( !*s ) { *src = s; return NULL; }
    s++; /* skip opening " */
    int i = 0;
    while ( *s && *s != '"' && i < bufLen - 1 )
        buf[i++] = *s++;
    buf[i] = '\0';
    if ( *s == '"' ) s++;
    *src = s;
    return buf;
}

/* Format-agnostic classname match on a raw parsed entity value.
 * Mirrors the game-side G_ClassnameIs (g_utils.c): the Q1 BSP loader
 * (BSP_Q1_PrefixClassnames) rewrites a Q1 map's classnames with a "q1_"
 * prefix, so a door parsed from a Q1 map arrives here as "q1_func_door"
 * while a Q3 map's is bare "func_door".  Strip an optional "q1_" prefix,
 * then compare against the bare name — "func_door" and "q1_func_door" are
 * equivalent.  This qcommon parser has no gentity_t, so it can't call
 * G_ClassnameIs; the same strip rule is applied here on the raw string. */
static qboolean DoorEnt_ClassnameIs( const char *classname, const char *bareName )
{
    const char *cn = classname;
    if ( !cn || !bareName ) return qfalse;
    if ( Q_stricmpn( cn, "q1_", 3 ) == 0 )
        cn += 3; /* strip the Q1 format prefix; compare the bare classname */
    return ( Q_stricmp( cn, bareName ) == 0 ) ? qtrue : qfalse;
}

int Nav_Get_DoorBoxes( const char *mapname, navDoorBox_t **outBoxes )
{
    mapFile_t *bsp = NULL;
    char bspPath[MAX_QPATH];
    Com_sprintf( bspPath, sizeof(bspPath), "maps/%s.bsp", mapname );

    *outBoxes = NULL;

    if ( !Map_Load( bspPath, &bsp, MAP_LOAD_FLAGS_NONE ) || !bsp )
        return 0;

    if ( !bsp->entityString ) {
        Map_Free( bsp );
        return 0;
    }

    /* D-19: BSP entity string is parsed in declaration order, which
     * matches the order Q3's G_Spawn() processes entities at map load.
     * For doors WITH a targetname this is irrelevant — we use the string directly.
     * For unnamed doors we synthesise "door_model_N" using the BSP submodel index N,
     * which is deterministic and available at both parse time (*N in model key) and
     * runtime (atoi(ent->model+1) in g_mover.c), so no ordering contract is needed. */

    /* Two-pass: count then fill. */
    #define MAX_DOOR_MODEL_IDX 128
    typedef struct { int modelIdx; char targetname[64]; } doorRecord_t;
    doorRecord_t doorRecords[MAX_DOOR_MODEL_IDX];
    int numDoors = 0;

    const char *es = bsp->entityString;
    while ( *es ) {
        /* Advance to next '{' */
        while ( *es && *es != '{' ) es++;
        if ( !*es ) break;
        es++;

        /* Scan key/value pairs for this entity */
        char classname[64]   = "";
        char targetname[64]  = "";
        int  modelIdx        = -1;

        while ( *es && *es != '}' ) {
            while ( *es == ' ' || *es == '\t' || *es == '\n' || *es == '\r' ) es++;
            if ( *es == '}' ) break;
            if ( *es != '"' ) { es++; continue; }

            char key[64], value[256];
            if ( !DoorEnt_ReadKey( &es, key, sizeof(key) ) ) break;
            while ( *es == ' ' || *es == '\t' || *es == '\n' || *es == '\r' ) es++;
            if ( *es != '"' ) continue;
            if ( !DoorEnt_ReadKey( &es, value, sizeof(value) ) ) break;

            if ( strcmp( key, "classname" ) == 0 )
                Q_strncpyz( classname, value, sizeof(classname) );
            else if ( strcmp( key, "model" ) == 0 && value[0] == '*' )
                modelIdx = atoi( value + 1 );
            else if ( strcmp( key, "targetname" ) == 0 )
                Q_strncpyz( targetname, value, sizeof(targetname) );
        }
        if ( *es == '}' ) es++;

        if ( DoorEnt_ClassnameIs( classname, "func_door" ) && modelIdx > 0
             && modelIdx < bsp->numSubModels
             && numDoors < MAX_DOOR_MODEL_IDX ) {
            doorRecords[numDoors].modelIdx = modelIdx;
            if ( targetname[0] ) {
                Q_strncpyz( doorRecords[numDoors].targetname,
                            targetname, sizeof(doorRecords[numDoors].targetname) );
            } else {
                /* Synthesize deterministic id for unnamed doors using submodel index;
                 * see comment above — reproduced in g_mover.c as "door_model_%d". */
                Com_sprintf( doorRecords[numDoors].targetname,
                             sizeof(doorRecords[numDoors].targetname),
                             "door_model_%d", modelIdx );
            }
            numDoors++;
        }
    }
    #undef MAX_DOOR_MODEL_IDX

    if ( numDoors == 0 ) {
        Map_Free( bsp );
        return 0;
    }

    navDoorBox_t *boxes = (navDoorBox_t *)Z_Malloc( numDoors * (int)sizeof(navDoorBox_t) );
    for ( int i = 0; i < numDoors; i++ ) {
        const dmodel_t *dm = &bsp->subModels[ doorRecords[i].modelIdx ];
        boxes[i].mins[0] = dm->mins[0]; boxes[i].mins[1] = dm->mins[1]; boxes[i].mins[2] = dm->mins[2];
        boxes[i].maxs[0] = dm->maxs[0]; boxes[i].maxs[1] = dm->maxs[1]; boxes[i].maxs[2] = dm->maxs[2];
        Q_strncpyz( boxes[i].targetname, doorRecords[i].targetname, sizeof(boxes[i].targetname) );
    }

    Map_Free( bsp );
    *outBoxes = boxes;
    return numDoors;
}

/* -------------------------------------------------------------------------
   Nav_Get_SpawnPoint
   Parse the BSP entity string for a player-spawn entity and return its
   world-space origin.  Used to anchor nav_validate's REACHABLE set on the
   component a player actually starts in (the play floor), rather than the
   largest-by-poly-count component — which on a heavily OMC-bridged, island-
   segmented mesh is often an inert void/shell region, not where a bot plays.
   Prefers info_player_start; falls back to info_player_deathmatch / _coop /
   testplayerstart (Q1 + Q3 names; the Q1 loader's "q1_" prefix is stripped by
   DoorEnt_ClassnameIs).  Returns qtrue and fills out[3] (Quake space) on the
   first match; qfalse if none / on BSP load failure.
   ------------------------------------------------------------------------- */
qboolean Nav_Get_SpawnPoint( const char *mapname, float out[3] )
{
    mapFile_t *bsp = NULL;
    char bspPath[MAX_QPATH];
    Com_sprintf( bspPath, sizeof(bspPath), "maps/%s.bsp", mapname );

    if ( !Map_Load( bspPath, &bsp, MAP_LOAD_FLAGS_NONE ) || !bsp )
        return qfalse;
    if ( !bsp->entityString ) { Map_Free( bsp ); return qfalse; }

    /* Spawn classnames in preference order.  A deathmatch start is the anchor a
     * bot actually uses; single-player info_player_start is the campaign anchor.
     * Either lands the anchor on the main play floor. */
    static const char *kSpawnClasses[] = {
        "info_player_start", "info_player_deathmatch",
        "info_player_coop", "info_player_intermission"
    };
    const int kNumSpawnClasses = (int)( sizeof(kSpawnClasses) / sizeof(kSpawnClasses[0]) );

    float best[3] = { 0, 0, 0 };
    int   bestRank = kNumSpawnClasses;   /* lower rank = higher preference */
    qboolean found = qfalse;

    /* VALID candidate (preferred): a spawn the player can actually occupy, held
     * separately from the raw best-by-rank so an embedded higher-ranked spawn
     * cannot displace a usable lower-ranked one. */
    float bestValid[3] = { 0, 0, 0 };
    int   bestValidRank = kNumSpawnClasses;
    qboolean foundValid = qfalse;

    const char *es = bsp->entityString;
    while ( *es ) {
        while ( *es && *es != '{' ) es++;
        if ( !*es ) break;
        es++;

        char classname[64] = "";
        float origin[3] = { 0, 0, 0 };
        qboolean haveOrigin = qfalse;

        while ( *es && *es != '}' ) {
            while ( *es == ' ' || *es == '\t' || *es == '\n' || *es == '\r' ) es++;
            if ( *es == '}' ) break;
            if ( *es != '"' ) { es++; continue; }

            char key[64], value[256];
            if ( !DoorEnt_ReadKey( &es, key, sizeof(key) ) ) break;
            while ( *es == ' ' || *es == '\t' || *es == '\n' || *es == '\r' ) es++;
            if ( *es != '"' ) continue;
            if ( !DoorEnt_ReadKey( &es, value, sizeof(value) ) ) break;

            if ( strcmp( key, "classname" ) == 0 )
                Q_strncpyz( classname, value, sizeof(classname) );
            else if ( strcmp( key, "origin" ) == 0 ) {
                if ( sscanf( value, "%f %f %f", &origin[0], &origin[1], &origin[2] ) == 3 )
                    haveOrigin = qtrue;
            }
        }
        if ( *es == '}' ) es++;

        if ( !haveOrigin ) continue;
        for ( int r = 0; r < kNumSpawnClasses; r++ ) {
            if ( !DoorEnt_ClassnameIs( classname, kSpawnClasses[r] ) )
                continue;

            /* Raw best-by-rank — the pre-existing behaviour, retained as the
             * fallback for maps where no spawn passes the validity test. */
            if ( r < bestRank ) {
                best[0] = origin[0]; best[1] = origin[1]; best[2] = origin[2];
                bestRank = r; found = qtrue;
            }

            /* Validity + rest position.  The contract in the header is "the
             * component a player actually starts in (the play floor)", and an
             * origin embedded in solid cannot be that: it snaps to whatever
             * fragment happens to be nearest, anchoring the reachable set on a
             * region the player never occupies.  Same two-trace predicate the
             * nav_spawnprobe diagnostic already uses — the shipped player box
             * and MASK_PLAYERSOLID, no new mechanism and no new tuning value:
             *   (a) startsolid at the origin  -> embedded, reject;
             *   (b) drop for the true rest-z  -> no rest, reject.
             * The rest position is what is returned, not the raw origin: the
             * entity origin sits at the player's standing centre, so the raw
             * value can be far above the floor the mesh was built on. */
            if ( r < bestValidRank ) {
                const vec3_t pmins = { -15, -15, -24 }, pmaxs = { 15, 15, 32 };
                trace_t at, dr;
                vec3_t  p0, ds, de;

                VectorCopy( origin, p0 );
                CM_BoxTrace( &at, p0, p0, pmins, pmaxs, 0, MASK_PLAYERSOLID, qfalse );
                if ( at.startsolid || at.allsolid )
                    break;   /* embedded: not where a player starts */

                VectorSet( ds, origin[0], origin[1], origin[2] + 48.0f );
                VectorSet( de, origin[0], origin[1], origin[2] - 128.0f );
                CM_BoxTrace( &dr, ds, de, pmins, pmaxs, 0, MASK_PLAYERSOLID, qfalse );
                if ( dr.startsolid || dr.fraction >= 1.0f )
                    break;   /* no floor beneath: cannot rest here */

                bestValid[0] = origin[0];
                bestValid[1] = origin[1];
                bestValid[2] = dr.endpos[2];
                bestValidRank = r;
                foundValid = qtrue;
            }
            break;
        }
    }

    Map_Free( bsp );
    /* A valid spawn wins even when it ranks lower by classname: the contract is
     * "where a player actually starts", not "whichever class ranks first".  When
     * NO spawn passes the validity test the old behaviour stands unchanged
     * (first by class rank, raw origin), so a map with only embedded spawns
     * keeps its anchor rather than losing it entirely. */
    if ( foundValid ) {
        out[0] = bestValid[0]; out[1] = bestValid[1]; out[2] = bestValid[2];
        return qtrue;
    }
    if ( found ) { out[0] = best[0]; out[1] = best[1]; out[2] = best[2]; }
    return found;
}

/* -------------------------------------------------------------------------
   Nav_Get_AllSpawnPoints — enumerate EVERY player-spawn entity origin on the map
   (all deathmatch/start/coop spawns), up to maxOut.  Diagnostic use: a systemic
   spawn-standability table.  Returns the count written into out[][3].
   ------------------------------------------------------------------------- */
int Nav_Get_AllSpawnPoints( const char *mapname, float (*out)[3], int maxOut )
{
    mapFile_t *bsp = NULL;
    char bspPath[MAX_QPATH];
    Com_sprintf( bspPath, sizeof(bspPath), "maps/%s.bsp", mapname );

    if ( !out || maxOut <= 0 ) return 0;
    if ( !Map_Load( bspPath, &bsp, MAP_LOAD_FLAGS_NONE ) || !bsp )
        return 0;
    if ( !bsp->entityString ) { Map_Free( bsp ); return 0; }

    static const char *kSpawnClasses[] = {
        "info_player_start", "info_player_deathmatch", "info_player_coop"
    };
    const int kNum = (int)( sizeof(kSpawnClasses) / sizeof(kSpawnClasses[0]) );

    int n = 0;
    const char *es = bsp->entityString;
    while ( *es && n < maxOut ) {
        while ( *es && *es != '{' ) es++;
        if ( !*es ) break;
        es++;

        char classname[64] = "";
        float origin[3] = { 0, 0, 0 };
        qboolean haveOrigin = qfalse;

        while ( *es && *es != '}' ) {
            while ( *es == ' ' || *es == '\t' || *es == '\n' || *es == '\r' ) es++;
            if ( *es == '}' ) break;
            if ( *es != '"' ) { es++; continue; }
            char key[64], value[256];
            if ( !DoorEnt_ReadKey( &es, key, sizeof(key) ) ) break;
            while ( *es == ' ' || *es == '\t' || *es == '\n' || *es == '\r' ) es++;
            if ( *es != '"' ) continue;
            if ( !DoorEnt_ReadKey( &es, value, sizeof(value) ) ) break;
            if ( strcmp( key, "classname" ) == 0 )
                Q_strncpyz( classname, value, sizeof(classname) );
            else if ( strcmp( key, "origin" ) == 0 ) {
                if ( sscanf( value, "%f %f %f", &origin[0], &origin[1], &origin[2] ) == 3 )
                    haveOrigin = qtrue;
            }
        }
        if ( *es == '}' ) es++;
        if ( !haveOrigin ) continue;
        for ( int r = 0; r < kNum; r++ ) {
            if ( DoorEnt_ClassnameIs( classname, kSpawnClasses[r] ) ) {
                out[n][0] = origin[0]; out[n][1] = origin[1]; out[n][2] = origin[2];
                n++;
                break;
            }
        }
    }

    Map_Free( bsp );
    return n;
}

#endif /* FEAT_RECAST_NAVMESH */
