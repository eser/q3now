/*
===========================================================================
nav_traps.c -- Trap dispatch for G_NAV_* syscalls.  Pure C.

Nav_HandleTrap() is the single entry point called from sv_game.c for all
G_NAV_* cases.  It resolves VM pointers internally using a file-local
Nav_VMA() helper, keeping VM internals out of the rest of the nav module.

VM pointer resolution:
  WAMR / native build:  vmBase == NULL; val is already a host pointer.
  QVM bytecode build:   vmBase == gvm->dataBase; val is a byte offset.
  No dataMask is applied here; nav functions are trusted engine code.

sv_game.c should pass:
  byte *vmBase = (gvm && !gvm->entryPoint) ? (byte *)gvm->dataBase : NULL;
  return Nav_HandleTrap( args[0], args, vmBase );
===========================================================================
*/

#include "../q_shared.h"
#include "../q_feats.h"

#if FEAT_RECAST_NAVMESH

#include "../qcommon.h"
#include "nav_public.h"
#include "nav_local.h"
#include "../../game/g_public.h"

/* -------------------------------------------------------------------------
   VM pointer resolution — local to this file
   ------------------------------------------------------------------------- */

static void *Nav_VMA( intptr_t val, byte *vmBase )
{
    if ( !val ) return NULL;
    if ( vmBase ) return (void *)(vmBase + (int)val);
    return (void *)val;
}

/* -------------------------------------------------------------------------
   Nav_HandleTrap
   Dispatches all G_NAV_* cases.  DetourCrowd stubs return -1/0 (Phase 6).
   ------------------------------------------------------------------------- */

intptr_t Nav_HandleTrap( int trap, const intptr_t *args, byte *vmBase )
{
    switch ( trap ) {

    case G_NAV_IS_READY:
        return Nav_IsReady();

    case G_NAV_FIND_PATH:
        /* args[1]=origin args[2]=goal args[3]=agentType args[4]=pathOut */
        return (intptr_t)Nav_FindPath(
            (const float *)Nav_VMA( args[1], vmBase ),
            (const float *)Nav_VMA( args[2], vmBase ),
            (int)args[3],
            (navPath_t *)Nav_VMA( args[4], vmBase ) );

    case G_NAV_RAYCAST:
        /* args[1]=start args[2]=end args[3]=hitPosOut */
        return (intptr_t)Nav_Raycast(
            (const float *)Nav_VMA( args[1], vmBase ),
            (const float *)Nav_VMA( args[2], vmBase ),
            (float *)       Nav_VMA( args[3], vmBase ) );

    case G_NAV_FIND_NEAREST_POLY:
        /* args[1]=origin args[2]=searchExtents */
        return (intptr_t)Nav_FindNearestPoly(
            (const float *)Nav_VMA( args[1], vmBase ),
            (const float *)Nav_VMA( args[2], vmBase ) );

    case G_NAV_GET_POLY_AREA_FLAGS:
        /* args[1]=polyRef */
        return (intptr_t)Nav_GetPolyAreaFlags( (navPolyRef_t)args[1] );

    case G_NAV_TRIGGER_OFF_MESH_LINK:
        /* Stub — deferred to Phase 4 */
        return 0;

    case G_NAV_GET_RANDOM_POINT:
        /* args[1]=areaFilter args[2]=posOut */
        return (intptr_t)Nav_GetRandomPoint(
            (int)args[1],
            (float *)Nav_VMA( args[2], vmBase ) );

    /* DetourCrowd — Phase 6 stubs */
    case G_NAV_ADD_CROWD_AGENT:
        return -1;

    case G_NAV_UPDATE_CROWD_AGENT:
    case G_NAV_REMOVE_CROWD_AGENT:
    case G_NAV_UPDATE_CROWD:
        return 0;

    case G_NAV_SET_POLY_FLAGS_FOR_DOOR:
        /* args[1]=targetname args[2]=setFlags args[3]=clearFlags */
        Nav_SetPolyFlagsForDoor(
            (const char *)Nav_VMA( args[1], vmBase ),
            (int)args[2],
            (int)args[3] );
        return 0;

    case G_NAV_PREDICT_ENEMY_POSITION:
        /* args[1]=origin args[2]=velocity args[3]=predictTime args[4]=outPos */
        Nav_PredictEnemyPosition(
            (const float *)Nav_VMA( args[1], vmBase ),
            (const float *)Nav_VMA( args[2], vmBase ),
            *(const float *)&args[3],
            (float *)       Nav_VMA( args[4], vmBase ) );
        return 0;

    default:
        Com_Log( SEV_DEBUG, LOG_CAT_NAV, "NAV: Nav_HandleTrap: unknown trap %d\n", trap );
        return 0;
    }
}

#endif /* FEAT_RECAST_NAVMESH */
