/*
===========================================================================
nav_geom.c -- Geometry orchestration layer.

Responsibilities:
  - Nav_Geom_GetChecksum: fast BSP checksum read (no geometry extraction).
  - Nav_Geom_Extract: full extraction via BSP abstraction callback +
    off-mesh connection assembly.  Called only on cache miss.
  - Nav_Geom_Free: LIFO Hunk_FreeTempMemory (areas → tris → verts).
  - Nav_OMC_Free: zeroes the fixed-size navOmcInput_t struct.

Two-pass allocation pattern (mirrors tr_bsp.c):
  Pass 1: iterate surfaces, count output verts and tris.
  Pass 2: Hunk_AllocateTempMemory at exact sizes (verts → tris → areas),
          then iterate surfaces again and fill.
Free order MUST be LIFO: areas → tris → verts.
===========================================================================
*/

#include "../q_shared.h"
#include "../q_feats.h"

#if FEAT_RECAST_NAVMESH

#include "../qcommon.h"
#include "../maps/bsp.h"
#include "nav_local.h"

/*
 * Nav_Geom_GetChecksum
 * Fast path: load BSP, read checksum, free BSP.
 * Does NOT extract geometry — safe to call every map load.
 * Returns qfalse if BSP_Load fails (out is unchanged).
 */
qboolean Nav_Geom_GetChecksum( const char *mapname, int *out )
{
    bspFile_t *bsp = NULL;
    char bspPath[MAX_QPATH];
    Com_sprintf( bspPath, sizeof(bspPath), "maps/%s.bsp", mapname );

    if ( !BSP_Load( bspPath, &bsp ) || !bsp )
        return qfalse;

    *out = bsp->checksum;
    BSP_Free( bsp );
    return qtrue;
}

/*
 * Nav_Geom_Extract
 * Slow path: BSP_Load → extractNavGeometry callback → Nav_OMC_Build.
 * Allocates geomOut arrays with Hunk_AllocateTempMemory (two-pass inside
 * the format callback).
 * Returns qfalse if BSP_Load fails or the format does not support nav.
 */
qboolean Nav_Geom_Extract( const char *mapname,
                            navGeom_t     *geomOut,
                            navOmcInput_t *omcOut )
{
    bspFile_t *bsp = NULL;
    char bspPath[MAX_QPATH];
    Com_sprintf( bspPath, sizeof(bspPath), "maps/%s.bsp", mapname );

    if ( !BSP_Load( bspPath, &bsp ) || !bsp ) {
        Com_Printf( "[NAV] Nav_Geom_Extract: BSP_Load failed for '%s'\n", mapname );
        return qfalse;
    }

    if ( !bsp->format || !bsp->format->extractNavGeometry ) {
        Com_Printf( "[NAV] Nav_Geom_Extract: format '%s' does not support nav geometry\n",
                    bsp->format ? bsp->format->name : "<null>" );
        BSP_Free( bsp );
        return qfalse;
    }

    memset( geomOut, 0, sizeof(*geomOut) );
    if ( !bsp->format->extractNavGeometry( bsp, geomOut ) ) {
        Com_Printf( "[NAV] Nav_Geom_Extract: extractNavGeometry failed for '%s'\n", mapname );
        BSP_Free( bsp );
        return qfalse;
    }

    memset( omcOut, 0, sizeof(*omcOut) );
    Nav_OMC_Build( bsp, omcOut );

    BSP_Free( bsp );
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

int Nav_Get_DoorBoxes( const char *mapname, navDoorBox_t **outBoxes )
{
    bspFile_t *bsp = NULL;
    char bspPath[MAX_QPATH];
    Com_sprintf( bspPath, sizeof(bspPath), "maps/%s.bsp", mapname );

    *outBoxes = NULL;

    if ( !BSP_Load( bspPath, &bsp ) || !bsp )
        return 0;

    if ( !bsp->entityString ) {
        BSP_Free( bsp );
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

        if ( strcmp( classname, "func_door" ) == 0 && modelIdx > 0
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
        BSP_Free( bsp );
        return 0;
    }

    navDoorBox_t *boxes = (navDoorBox_t *)Z_Malloc( numDoors * (int)sizeof(navDoorBox_t) );
    for ( int i = 0; i < numDoors; i++ ) {
        const dmodel_t *dm = &bsp->subModels[ doorRecords[i].modelIdx ];
        boxes[i].mins[0] = dm->mins[0]; boxes[i].mins[1] = dm->mins[1]; boxes[i].mins[2] = dm->mins[2];
        boxes[i].maxs[0] = dm->maxs[0]; boxes[i].maxs[1] = dm->maxs[1]; boxes[i].maxs[2] = dm->maxs[2];
        Q_strncpyz( boxes[i].targetname, doorRecords[i].targetname, sizeof(boxes[i].targetname) );
    }

    BSP_Free( bsp );
    *outBoxes = boxes;
    return numDoors;
}

#endif /* FEAT_RECAST_NAVMESH */
