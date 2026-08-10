// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
===========================================================================
g_belief.c — per-brain belief store implementation.

Interactables, threats, and explored-areas live on bs->beliefStore and are
produced/queried here. The item-belief view is a read-only accessor over the
shared item-goal DB and is implemented in g_bot_scripts.c (where that DB
lives); only the interactables/threats/explored code is here.

The store never crosses the botlib/engine ABI boundary — no syscall, no enum,
no wire/savegame field. It is the addressable seam the goal tree and the future
coroutine layer query; the existing subsystems became its first clients.
===========================================================================
*/

#include "g_local.h"
#include "../../../botlib/botlib.h"
#include "../../../botlib/be_aas.h"       /* aas_entityinfo_t, bsp_trace_t (ai_main.h protos) */
#include "../../../botlib/be_ai_goal.h"   /* bot_goal_t (ai_main.h bot_state_t/protos) */
#include "ai_main.h"                      /* bot_state_t */
#include "g_belief.h"

/*
==================
Belief_Init
Zero the store and set sentinels. Called from BotDirective_Init (which runs
once per bot in BotAISetupClient) so a fresh brain starts with an empty belief.
BotResetState memsets bot_state_t to zero, which already yields an empty store;
this makes the sentinels explicit and keeps init a single call.
==================
*/
void Belief_Init( beliefStore_t *b ) {
    if ( !b ) return;
    memset( b, 0, sizeof( *b ) );
    b->lastThreat.entitynum   = -1;
    b->explored.lastRecorded  = -1;
}

/* =========================================================================
   INTERACTABLES — discovered buttons/gates.
   Backing store for the directive activation queue: the queue cursor
   (count/index/door) lives in botDirectiveState_t and indexes these records.
   ========================================================================= */

void Belief_InteractablesClear( bot_state_t *bs ) {
    if ( !bs ) return;
    bs->beliefStore.interactableCount = 0;
}

int Belief_InteractableAdd( bot_state_t *bs, const vec3_t origin,
                            int entitynum, int associatedDoor, int kind ) {
    beliefInteractable_t *rec;
    int idx;

    if ( !bs || bs->beliefStore.interactableCount >= BELIEF_MAX_INTERACTABLES ) {
        return -1;
    }
    idx = bs->beliefStore.interactableCount;
    rec = &bs->beliefStore.interactables[idx];
    VectorCopy( origin, rec->origin );
    rec->entitynum      = entitynum;
    rec->associatedDoor = associatedDoor;
    rec->kind           = kind;
    bs->beliefStore.interactableCount = idx + 1;
    return idx;
}

int Belief_InteractableCount( bot_state_t *bs ) {
    return bs ? bs->beliefStore.interactableCount : 0;
}

const beliefInteractable_t *Belief_Interactable( bot_state_t *bs, int index ) {
    if ( !bs || index < 0 || index >= bs->beliefStore.interactableCount ) {
        return NULL;
    }
    return &bs->beliefStore.interactables[index];
}

void Belief_InteractablesSetDoor( bot_state_t *bs, int doorEntity ) {
    int i;
    if ( !bs ) return;
    for ( i = 0; i < bs->beliefStore.interactableCount; i++ ) {
        bs->beliefStore.interactables[i].associatedDoor = doorEntity;
    }
}

void Belief_InteractableSetOrigin( bot_state_t *bs, int index, const vec3_t origin ) {
    if ( !bs || index < 0 || index >= bs->beliefStore.interactableCount ) return;
    VectorCopy( origin, bs->beliefStore.interactables[index].origin );
}

qboolean Belief_KnownInteractableNear( bot_state_t *bs, const vec3_t origin,
                                       float radius ) {
    int i;
    float r2;
    if ( !bs ) return qfalse;
    r2 = radius * radius;
    for ( i = 0; i < bs->beliefStore.interactableCount; i++ ) {
        if ( DistanceSquared( origin, bs->beliefStore.interactables[i].origin ) <= r2 ) {
            return qtrue;
        }
    }
    return qfalse;
}

/* =========================================================================
   THREATS — last-seen enemy position.
   Recorded where the battle nodes already update bs->enemy / lastenemyorigin /
   lastenemyareanum / enemyvisible_time, so the store mirrors the live last-seen
   enemy without altering tracking.
   ========================================================================= */

void Belief_RecordThreat( bot_state_t *bs, int entitynum,
                          const vec3_t origin, int areanum, float seenTime ) {
    if ( !bs ) return;
    bs->beliefStore.lastThreat.entitynum    = entitynum;
    VectorCopy( origin, bs->beliefStore.lastThreat.origin );
    bs->beliefStore.lastThreat.areanum      = areanum;
    bs->beliefStore.lastThreat.lastSeenTime = seenTime;
}

const beliefThreat_t *Belief_LastThreat( bot_state_t *bs ) {
    return bs ? &bs->beliefStore.lastThreat : NULL;
}

/* =========================================================================
   EXPLORED AREAS — bounded visited-area ring. The new producer this landing.
   Recorded as the bot transitions areas; INERT w.r.t. current decisions (no
   existing code reads it — the query helpers exist for the self-test and the
   future consumer).
   ========================================================================= */

void Belief_RecordVisitedArea( bot_state_t *bs, int areanum ) {
    beliefExplored_t *ex;

    if ( !bs || areanum <= 0 ) {
        return;   /* 0 = no valid nav area (BotPointAreaNum miss) — nothing to record */
    }
    ex = &bs->beliefStore.explored;
    if ( areanum == ex->lastRecorded ) {
        return;   /* same area as last frame — only record on transition */
    }
    ex->lastRecorded = areanum;
    ex->areas[ex->head] = areanum;
    ex->head = ( ex->head + 1 ) % BELIEF_MAX_EXPLORED;
    if ( ex->count < BELIEF_MAX_EXPLORED ) {
        ex->count++;
    }
}

qboolean Belief_HasVisited( bot_state_t *bs, int areanum ) {
    int i;
    const beliefExplored_t *ex;
    if ( !bs || areanum <= 0 ) return qfalse;
    ex = &bs->beliefStore.explored;
    for ( i = 0; i < ex->count; i++ ) {
        if ( ex->areas[i] == areanum ) {
            return qtrue;
        }
    }
    return qfalse;
}

int Belief_VisitedCount( bot_state_t *bs ) {
    return bs ? bs->beliefStore.explored.count : 0;
}
