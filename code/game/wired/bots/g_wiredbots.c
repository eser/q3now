// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
===========================================================================
g_wiredbots.c — WiredBots unified bot directive system (Phase 1)

All bot orders flow through one channel: chat messages parsed into
botDirective_t values stored in bs->directives.

BotDirective_FrameUpdate() is called before BotDeathmatchAI() each frame
and translates the active directive into bs->ltgtype / bs->teamgoal so the
legacy AINode_Seek_LTG navigation code keeps driving movement unchanged.
===========================================================================
*/

#include "g_local.h"
#include "../botlib/botlib.h"
#include "../botlib/be_aas.h"
#include "../botlib/be_ea.h"
#include "../botlib/be_ai_char.h"
#include "../botlib/be_ai_chat.h"
#include "../botlib/be_ai_gen.h"
#include "../botlib/be_ai_goal.h"
#include "../botlib/be_ai_move.h"
#include "../botlib/be_ai_weap.h"
#include "ai_main.h"
#include "ai_dmnet.h"
#include "ai_dmq3.h"
#include "g_wiredbots.h"
/* Phase 5: log channels */
LOG_DECLARE_CHANNEL( ch_game, "game" );

/* ── botstates array (defined in ai_main.c) ─────────────────────────── */
extern bot_state_t *botstates[MAX_CLIENTS];

/* ── forward declarations ────────────────────────────────────────────── */
static int  ResolveClientByName( const char *name );

/* =========================================================================
   LIFECYCLE
   ========================================================================= */

/*
==================
BotDirective_Init
==================
Zeroes the directive state and sets sentinel values.
Called once during BotAISetupClient().
*/
void BotDirective_Init( botDirectiveState_t *ds ) {
    memset( ds, 0, sizeof( *ds ) );
    ds->tactical.type          = DIR_NONE;
    ds->tactical.target_client = -1;
    ds->tactical.source_client = -1;
    ds->tactic_target          = -1;
    ds->directiveLocked        = qfalse;
    ds->armorAtStart           = 0;
    ds->healthAtStart          = 0;
}

/*
==================
BotDirective_ClearAll
==================
Resets all directives to idle.  Keeps the bot alive and controllable by AI
but removes any current orders.
*/
void BotDirective_ClearAll( bot_state_t *bs ) {
    BotDirective_Init( &bs->directives );
    bs->ordered        = qfalse;
    bs->ltgtype        = 0;
    bs->teamgoal_time  = 0;
}

/*
==================
BotDirective_OnItemPickup
==================
If the bot just picked up the item it was sent to fetch, clear the
seek-item directive so it resumes normal AI.
*/
void BotDirective_OnItemPickup( bot_state_t *bs, int item_entity ) {
    botDirective_t *d = &bs->directives.tactical;

    if ( d->type != DIR_SEEK_ITEM ) {
        return;
    }

    /* We only know the entity number here.  If the directive had a specific
       entity target set (target_client repurposed for entity num), compare;
       otherwise clear unconditionally after any pickup. */
    if ( d->target_client < 0 || d->target_client == item_entity ) {
        d->type = DIR_NONE;
        bs->directives.directiveLocked = qfalse;
        bs->ordered       = qfalse;
        bs->ltgtype       = 0;
        bs->teamgoal_time = 0;
        BotDirective_UpdateConfigstring( bs );
    }
}

/*
==================
BotDirective_OnDeath
==================
Directive persists across death — bot resumes pursuing the same goal on
respawn without needing a new coach order.  Only three events clear a
directive: completion, a replacement order, or TTL expiry.

On death we reset the TTL clock so the bot gets a fresh window after
respawn, and zero the stat baselines so pickup detection works correctly
with the new post-respawn health/armor values.

Tactics (HUNT/AVOID personality modifiers) persist across deaths unchanged.
*/
void BotDirective_OnDeath( bot_state_t *bs ) {
    botDirective_t *d = &bs->directives.tactical;

    if ( bs->directives.directiveLocked && d->type != DIR_NONE ) {
        /* Restart the TTL from now so bot gets a full window after respawn */
        float ttl = d->expire_time - d->issue_time;
        if ( ttl > 0.0f ) {
            d->issue_time  = FloatTime();
            d->expire_time = d->issue_time + ttl;
        }
        /* Defer baseline re-snapshot until the first alive frame post-respawn.
           Zeroing here causes false "Got it!" because respawn health (100) > 0. */
        bs->directives.needsBaselineReset = qtrue;
        /* Reset raw movement — re-evaluate AAS reachability on respawn */
        bs->directives.useRawPosition = qfalse;
    }

    /* Reset movement state so the nav system picks up the goal cleanly */
    bs->ordered       = qfalse;
    bs->ltgtype       = 0;
    bs->teamgoal_time = 0;
    BotDirective_UpdateConfigstring( bs );
}

/*
==================
BotDirective_UpdateConfigstring
==================
Pushes the current directive to the CS_BOTDIRECTIVES+clientNum configstring
so cgame can render it as 3D text above the bot's head.
Format: "type\displayname"  — type is the directiveType_t integer value.
Called after every directive state change; engine deduplicates identical writes.
*/
void BotDirective_UpdateConfigstring( bot_state_t *bs ) {
    char            cs[128];
    botDirective_t *d = &bs->directives.tactical;
    const char     *displayName;

    if ( !bs->directives.directiveLocked || d->type == DIR_NONE ) {
        trap_SetConfigstring( CS_BOTDIRECTIVES + bs->client, "" );
        return;
    }

    switch ( d->type ) {
        case DIR_FOLLOW:
        case DIR_KILL_TARGET:
            displayName = d->target_name;
            break;
        case DIR_SEEK_ITEM:
            displayName = d->item_classname;
            break;
        default:
            displayName = "";
            break;
    }

    Com_sprintf( cs, sizeof( cs ), "%d\\%s", (int)d->type, displayName );
    trap_SetConfigstring( CS_BOTDIRECTIVES + bs->client, cs );
}

/* =========================================================================
   THINK-LOOP INTEGRATION
   ========================================================================= */

/*
==================
BotResolveItemName (local)
==================
Resolves a user-supplied string to the pickup_name that botlib's
trap_BotGetLevelItemGoal expects.  Accepts either:
  • entity classname  (e.g. "item_armor_body")
  • pickup_name       (e.g. "Heavy Armor")
Returns a pointer into bg_itemlist on success, NULL if unknown.
*/

/*
==================
BotPickupToClassname
==================
Maps a user-visible pickup_name (e.g. "Heavy Armor") to the primary entity
classname (e.g. "item_armor_body") that trap_BotGetLevelItemGoal indexes by.
Also accepts classnames directly — returns them unchanged.
*/
static const char *BotPickupToClassname( const char *pickup_name ) {
    int i;
    for ( i = 1; i < bg_numItems; i++ ) {
        if ( bg_itemlist[i].pickup_name &&
            Q_stricmp( bg_itemlist[i].pickup_name, pickup_name ) == 0 )
            return bg_itemlist[i].classname;
    }
    return pickup_name; /* fallback: may already be a classname */
}

static const char *BotResolveItemName( const char *input )
{
    gitem_t *it;
    int      j;

    if ( !input || !input[0] )
        return NULL;

    /* Special alias used by coach commands */
    if ( Q_stricmp( input, "flag" ) == 0 )
        return "flag";

    for ( it = bg_itemlist + 1; it->classname; it++ ) {
        /* Match by pickup_name first (exact, case-insensitive) */
        if ( it->pickup_name && Q_stricmp( it->pickup_name, input ) == 0 )
            return it->pickup_name;
        /* Match by any classname alias */
        if ( Q_stricmp( it->classname, input ) == 0 )
            return it->pickup_name;
    }
    return NULL;
}

/*
==================
BotDirective_Abort
==================
Full reset after a directive fails for any reason other than success.
Clears all directive state and returns the bot to normal free-roam AI
immediately — the bot will never stand idle after a failed directive.
*/
static void BotDirective_Abort( bot_state_t *bs ) {
    botDirectiveState_t *ds = &bs->directives;
    ds->tactical.type     = DIR_NONE;
    ds->directiveLocked   = qfalse;
    ds->useRawPosition    = qfalse;
    ds->lastProgressCheck = 0;
    ds->lastProgressDist  = 0.0f;
    bs->ltgtype           = 0;
    bs->nbg_time          = 0;
    trap_BotEmptyGoalStack( bs->gs );
    AIEnter_Seek_LTG( bs, "directive aborted" );
}

/*
==================
BotDirective_FrameUpdate
==================
Translates the active botDirective_t into bs->ltgtype / bs->teamgoal every
frame.  Called before BotDeathmatchAI(bs, thinktime) in ai_main.c.

Directives are authoritative; ltgtype is a derived cache used by the
existing AINode_Seek_LTG navigation engine in ai_dmnet.c.
*/
void BotDirective_FrameUpdate( bot_state_t *bs ) {
    botDirective_t *d  = &bs->directives.tactical;
    float           now = FloatTime();

    if ( trap_Cvar_VariableIntegerValue( "bot_debug" ) >= 1 ) {
        static float s_dirLogTime[MAX_CLIENTS];
        if ( FloatTime() - s_dirLogTime[bs->client] > 2.0f ) {
            s_dirLogTime[bs->client] = FloatTime();
            Com_Log( SEV_INFO, LOG_CH(ch_game), "^3[DirFrame] cl=%d type=%d target_cl=%d expire=%.1f\n",
                bs->client, d->type, d->target_client,
                d->expire_time > 0.0f ? d->expire_time - now : -1.0f );
        }
    }

    /* ── post-respawn baseline re-snapshot ──────────────────────────────── */
    if ( bs->directives.needsBaselineReset && bs->cur_ps.pm_type != PM_DEAD ) {
        bs->directives.armorAtStart   = bs->cur_ps.stats[STAT_ARMOR];
        bs->directives.healthAtStart  = bs->cur_ps.stats[STAT_HEALTH];
        bs->directives.needsBaselineReset = qfalse;
    }

    /* ── expiry check ─────────────────────────────────────────────────── */
    if ( d->type != DIR_NONE && d->expire_time > 0.0f && now > d->expire_time ) {
        d->type = DIR_NONE;
        bs->directives.directiveLocked = qfalse;
        BotDirective_UpdateConfigstring( bs );
    }

    if ( d->type == DIR_NONE ) {
        /* No directive active — let existing AI choose its own goals.
           Do not touch ltgtype or teamgoal_time here; the bot may have
           set them via normal BotMatchMessage handling. */
        return;
    }

    /* ── ltgtype translation bridge ───────────────────────────────────── */
    bs->ordered        = qtrue;
    bs->decisionmaker  = d->source_client >= 0 ? d->source_client : bs->client;
    bs->order_time     = d->issue_time;

    switch ( d->type ) {
        case DIR_FOLLOW:
            /* "follow carrier": re-resolve the flag carrier every frame so
               the directive automatically re-targets if the carrier changes. */
            if ( Q_stricmp( d->target_name, "flag carrier" ) == 0 ) {
                d->target_client = BotTeamFlagCarrier( bs );
            }
            if ( d->target_client >= 0 &&
                 d->target_client < MAX_CLIENTS &&
                 level.clients[d->target_client].pers.connected == CON_CONNECTED ) {
                bs->ltgtype        = LTG_TEAMACCOMPANY;
                bs->teammate       = d->target_client;
                bs->teamgoal_time  = now + 1.0f;   /* refresh each frame */
                bs->formation_dist = 3.5f * 32.0f;
            }
            break;

        case DIR_DEFEND_AREA:
            bs->ltgtype = LTG_DEFENDKEYAREA;
            VectorCopy( d->area_origin, bs->teamgoal.origin );
            bs->teamgoal.areanum  = BotPointAreaNum( d->area_origin );
            bs->teamgoal.entitynum = -1;
            bs->teamgoal.flags    = 0;
            bs->teamgoal_time     = now + 1.0f;
            bs->defendaway_time   = 0;
            bs->defendaway_range  = d->area_radius > 0 ? d->area_radius : 200.0f;
            break;

        case DIR_CAMP_SPOT:
            /* LTG_CAMPORDER is defined but not handled in BotGetLongTermGoal.
               LTG_DEFENDKEYAREA implements the same navigate-then-hold pattern.
               A very tight defendaway_range keeps the bot pinned to the spot. */
            bs->ltgtype           = LTG_DEFENDKEYAREA;
            VectorCopy( d->area_origin, bs->teamgoal.origin );
            bs->teamgoal.areanum  = BotPointAreaNum( d->area_origin );
            bs->teamgoal.entitynum = -1;
            bs->teamgoal.flags    = 0;
            bs->teamgoal_time     = now + 1.0f;
            bs->defendaway_time   = 0;
            bs->defendaway_range  = 80.0f;   /* tight — barely roams */
            bs->ordered           = qtrue;
            break;

        case DIR_PATROL:
            if ( bs->patrolpoints ) {
                bs->ltgtype       = LTG_PATROL;
                bs->teamgoal_time = now + 1.0f;
            }
            /* If no patrol waypoints, fall through to normal AI */
            break;

        case DIR_SEEK_ITEM: {
            const char *resolved = BotResolveItemName( d->item_classname );
            if ( !resolved ) {
                char rej[MAX_SAY_TEXT];
                Com_sprintf( rej, sizeof(rej), "Unknown item: %s", d->item_classname );
                trap_EA_Say( bs->client, rej );
                BotDirective_Abort( bs );
                break;
            }

            /* Pickup detection (Approach A): poll player state each frame.
               If the stat corresponding to this item type increased since the
               directive was issued, the bot acquired the item — clear and announce. */
            if ( bs->directives.directiveLocked && Q_stricmp( resolved, "flag" ) != 0 ) {
                qboolean picked_up = qfalse;
                const gitem_t *it;
                for ( it = bg_itemlist + 1; it->classname; it++ ) {
                    if ( it->pickup_name && Q_stricmp( it->pickup_name, resolved ) == 0 ) {
                        if ( it->giType == IT_ARMOR &&
                             bs->cur_ps.stats[STAT_ARMOR] > bs->directives.armorAtStart ) {
                            picked_up = qtrue;
                        } else if ( it->giType == IT_HEALTH &&
                                     bs->cur_ps.stats[STAT_HEALTH] > bs->directives.healthAtStart ) {
                            picked_up = qtrue;
                        } else if ( it->giType == IT_WEAPON ) {
                            int wn = it->giTag;
                            if ( wn > 0 && wn < MAX_WEAPONS &&
                                 ( bs->cur_ps.stats[STAT_WEAPONS] & ( 1 << wn ) ) ) {
                                picked_up = qtrue;
                            }
                        }
                        break;
                    }
                }
                /* For armor/health, also require proximity to the target item
                   to reject false positives from picking up a different item of
                   the same type (e.g. "get Heavy Armor" while grabbing YA nearby)
                   or from the baseline being taken before the respawn stat reset. */
                if ( picked_up &&
                     ( it->giType == IT_ARMOR || it->giType == IT_HEALTH ) ) {
                    if ( bs->directives.seekPosition[0] != 0.0f ||
                         bs->directives.seekPosition[1] != 0.0f ||
                         bs->directives.seekPosition[2] != 0.0f ) {
                        vec3_t diff;
                        VectorSubtract( bs->origin, bs->directives.seekPosition, diff );
                        if ( VectorLength( diff ) > 200.0f ) {
                            picked_up = qfalse;   /* too far — different item */
                        }
                    }
                }
                if ( picked_up ) {
                    if ( bs->wiredBotsActive )
                        trap_EA_SayTeam( bs->client, "Got it!" );
                    d->type = DIR_NONE;
                    bs->directives.directiveLocked = qfalse;
                    bs->ordered = qfalse;
                    bs->ltgtype = 0;
                    bs->teamgoal_time = 0;
                    break;
                }
            }

            /* ── raw-position movement path (AAS entirely unavailable) ──── */
            if ( bs->directives.useRawPosition ) {
                /* 10-second timeout before giving up */
                if ( FloatTime() - bs->directives.rawMoveStartTime > 10.0f ) {
                    char rej[MAX_SAY_TEXT];
                    Com_sprintf( rej, sizeof(rej), "Can't reach: %s", resolved );
                    trap_EA_Say( bs->client, rej );
                    BotDirective_Abort( bs );
                    break;
                }
                /* Walk toward the item's world coordinates via EA movement */
                {
                    vec3_t dir, angles;
                    VectorSubtract( bs->directives.seekPosition, bs->origin, dir );
                    dir[2] = 0.0f;
                    if ( VectorNormalize( dir ) > 32.0f ) {
                        vectoangles( dir, angles );
                        bs->ideal_viewangles[YAW]   = angles[YAW];
                        bs->ideal_viewangles[PITCH] = 0.0f;
                        if ( trap_Cvar_VariableIntegerValue( "bot_debug" ) >= 2 )
                            BotAI_Print( PRT_MESSAGE, "[AimSet/wb-seek] client %d pitch=%.1f yaw=%.1f\n",
                                         bs->client, bs->ideal_viewangles[PITCH], bs->ideal_viewangles[YAW] );
                        trap_EA_MoveForward( bs->client );
                    }
                }
                bs->teamgoal_time = now + 1.0f;
                break;
            }

            /* ── Goal setting: navigate toward the item via AAS ─────────── */
            if ( Q_stricmp( resolved, "flag" ) == 0 ) {
                bs->ltgtype = LTG_GETFLAG;
            } else {
                /* Resolve to AAS level goal.
                   trap_BotGetLevelItemGoal indexes items by entity classname
                   (e.g. "item_armor_body"), not pickup_name ("Heavy Armor").
                   BotPickupToClassname converts pickup_name to classname. */
                const char *item_cn = BotPickupToClassname( resolved );
                bot_goal_t item_goal;
                int item_result;
                memset( &item_goal, 0, sizeof( item_goal ) );
                item_result = trap_BotGetLevelItemGoal( -1, (char *)item_cn, &item_goal );
                if ( item_result >= 0 && item_goal.areanum > 0 ) {
                    /* Store world position for pickup proximity detection */
                    if ( VectorLength( bs->directives.seekPosition ) < 1.0f ) {
                        VectorCopy( item_goal.origin, bs->directives.seekPosition );
                    }
                    bs->teamgoal  = item_goal;
                    bs->ltgtype   = LTG_GETITEM;
                } else {
                    /* AAS goal list failed — search world entities for the item
                       and synthesize a goal from its world position + AAS area. */
                    gentity_t  *ent = NULL;
                    int         ei;
                    for ( ei = 0; ei < level.num_entities; ei++ ) {
                        gentity_t *e = &g_entities[ei];
                        if ( !e->inuse || !e->item || !e->item->pickup_name ) continue;
                        if ( Q_stricmp( e->item->pickup_name, resolved ) == 0 ) {
                            ent = e;
                            break;
                        }
                    }
                    if ( ent ) {
                        int area = trap_AAS_PointAreaNum( ent->r.currentOrigin );
                        if ( area <= 0 ) {
                            /* Items at exact floor/boundary positions return 0.
                               Probe slightly above the origin to find the area. */
                            vec3_t probe;
                            VectorCopy( ent->r.currentOrigin, probe );
                            probe[2] += 24.0f;
                            area = trap_AAS_PointAreaNum( probe );
                            if ( area <= 0 ) {
                                probe[2] = ent->r.currentOrigin[2] + 48.0f;
                                area = trap_AAS_PointAreaNum( probe );
                            }
                        }
                        if ( area > 0 ) {
                            /* Build a synthetic goal from the entity's world position.
                               Set GFL_ITEM so BotReachedGoal uses item-proximity
                               logic, and provide a reasonable bounding box. */
                            qboolean first_seek = ( VectorLength( bs->directives.seekPosition ) < 1.0f );
                            memset( &item_goal, 0, sizeof( item_goal ) );
                            VectorCopy( ent->r.currentOrigin, item_goal.origin );
                            item_goal.areanum   = area;
                            item_goal.entitynum = (int)( ent - g_entities );
                            item_goal.flags     = GFL_ITEM;
                            VectorSet( item_goal.mins, -15, -15, -15 );
                            VectorSet( item_goal.maxs,  15,  15,  15 );
                            VectorCopy( ent->r.currentOrigin, bs->directives.seekPosition );
                            bs->teamgoal = item_goal;
                            bs->ltgtype  = LTG_GETITEM;
                            if ( first_seek )
                                trap_EA_SayTeam( bs->client, "On my way!" );
                        } else {
                            /* No AAS coverage — fall back to raw direction movement */
                            qboolean first_seek = ( VectorLength( bs->directives.seekPosition ) < 1.0f );
                            VectorCopy( ent->r.currentOrigin, bs->directives.seekPosition );
                            bs->directives.useRawPosition  = qtrue;
                            bs->directives.rawMoveStartTime = FloatTime();
                            if ( first_seek )
                                trap_EA_SayTeam( bs->client, "On my way!" );
                            bs->teamgoal_time = now + 1.0f;
                            break;
                        }
                    } else {
                        /* Item doesn't exist on this map at all */
                        char rej[MAX_SAY_TEXT];
                        Com_sprintf( rej, sizeof(rej), "No such item: %s", resolved );
                        trap_EA_Say( bs->client, rej );
                        BotDirective_Abort( bs );
                        break;
                    }
                }
            }

            /* ── diagnostic ─────────────────────────────────────────────── */
            if ( trap_Cvar_VariableIntegerValue( "bot_debug" ) >= 1 ) {
                static float s_dirMoveLog[MAX_CLIENTS];
                if ( FloatTime() - s_dirMoveLog[bs->client] > 1.0f ) {
                    vec3_t diff;
                    s_dirMoveLog[bs->client] = FloatTime();
                    VectorSubtract( bs->origin, bs->teamgoal.origin, diff );
                    Com_Log( SEV_INFO, LOG_CH(ch_game), "DIRECTIVE MOVE: cl=%d item=%s dist=%.0f goal=[%.0f,%.0f,%.0f] area=%d ltg=%d\n",
                        bs->client, d->item_classname, VectorLength( diff ),
                        bs->teamgoal.origin[0], bs->teamgoal.origin[1], bs->teamgoal.origin[2],
                        bs->teamgoal.areanum, bs->ltgtype );
                }
            }

            /* ── near-spawn patrol (item not yet respawned) ───────────────
               When the bot has arrived at the item location but the entity
               hasn't respawned yet, patrol defensively rather than standing
               idle.  We break early so stuck detection doesn't misfire. */
            if ( bs->teamgoal.entitynum >= 0
                 && bs->teamgoal.entitynum < MAX_GENTITIES
                 && Q_stricmp( resolved, "flag" ) != 0 ) {
                float distToGoal = Distance( bs->origin, bs->teamgoal.origin );
                if ( distToGoal < 128.0f ) {
                    gentity_t *eg = &g_entities[bs->teamgoal.entitynum];
                    qboolean   itemPresent = (eg->r.contents & CONTENTS_TRIGGER) != 0;
                    if ( !itemPresent ) {
                        WiredBots_DefensiveCombat( bs );
                        if ( level.time > bs->directives.nextPatrolTime ) {
                            vec3_t patrolPoint, pdir;
                            patrolPoint[0] = bs->teamgoal.origin[0] + crandom() * 150.0f;
                            patrolPoint[1] = bs->teamgoal.origin[1] + crandom() * 150.0f;
                            patrolPoint[2] = bs->teamgoal.origin[2];
                            VectorSubtract( patrolPoint, bs->origin, pdir );
                            pdir[2] = 0.0f;
                            if ( VectorNormalize( pdir ) > 16.0f ) {
                                vec3_t pangles;
                                vectoangles( pdir, pangles );
                                bs->ideal_viewangles[YAW]   = pangles[YAW];
                                bs->ideal_viewangles[PITCH] = 0.0f;
                                if ( trap_Cvar_VariableIntegerValue( "bot_debug" ) >= 2 )
                                    BotAI_Print( PRT_MESSAGE, "[AimSet/wb-patrol] client %d pitch=%.1f yaw=%.1f\n",
                                                 bs->client, bs->ideal_viewangles[PITCH], bs->ideal_viewangles[YAW] );
                                trap_EA_Move( bs->client, pdir, 200 );
                            }
                            bs->directives.nextPatrolTime =
                                level.time + 1500 + ( rand() % 2000 );
                        }
                        /* Not stuck — reset baseline so check restarts when item appears */
                        bs->directives.lastProgressCheck = 0;
                        bs->teamgoal_time = now + 1.0f;
                        break;
                    }
                }
            }

            /* ── stuck detection ──────────────────────────────────────────
               Sample distance to goal every 5 s. If the bot hasn't closed
               the gap by at least 32 units it is stuck — abort the directive
               so the bot returns to normal AI rather than standing idle. */
            {
                float dist = Distance( bs->origin, bs->teamgoal.origin );
                if ( bs->directives.lastProgressCheck == 0 ) {
                    bs->directives.lastProgressDist  = dist;
                    bs->directives.lastProgressCheck = level.time;
                } else if ( level.time - bs->directives.lastProgressCheck > 5000 ) {
                    if ( dist >= bs->directives.lastProgressDist - 32.0f ) {
                        trap_EA_SayTeam( bs->client, "Can't get there." );
                        BotDirective_Abort( bs );
                        break;
                    }
                    bs->directives.lastProgressDist  = dist;
                    bs->directives.lastProgressCheck = level.time;
                }
            }

            bs->teamgoal_time = now + 1.0f;
            break;
        }

        case DIR_RUSH_BASE:
            bs->ltgtype         = LTG_RUSHBASE;
            bs->teamgoal_time   = now + 1.0f;
            bs->rushbaseaway_time = 0;
            break;

        case DIR_RETURN_FLAG:
            bs->ltgtype         = LTG_RETURNFLAG;
            bs->teamgoal_time   = now + 1.0f;
            break;

        case DIR_ATTACK_BASE:
            bs->ltgtype         = LTG_ATTACKENEMYBASE;
            bs->teamgoal_time   = now + 1.0f;
            bs->attackaway_time = 0;
            break;

        case DIR_KILL_TARGET:
            if ( d->target_client >= 0 &&
                 d->target_client < MAX_CLIENTS &&
                 level.clients[d->target_client].pers.connected == CON_CONNECTED &&
                 level.clients[d->target_client].ps.stats[STAT_HEALTH] > 0 ) {
                bs->ltgtype              = LTG_KILL;
                bs->teamgoal.entitynum   = d->target_client;
                bs->teamgoal_time        = now + 1.0f;
                bs->ordered              = qtrue;
            } else {
                /* Target dead or disconnected — announce if we were actively hunting */
                if ( bs->directives.directiveLocked && bs->wiredBotsActive )
                    trap_EA_SayTeam( bs->client, "Target down!" );
                d->type = DIR_NONE;
                bs->directives.directiveLocked = qfalse;
            }
            break;

        case DIR_HARVEST:
            bs->ltgtype        = LTG_HARVEST;
            bs->teamgoal_time  = now + 1.0f;
            bs->harvestaway_time = 0;
            break;

        case DIR_ROAM:
            /* Cancel any current LTG goal and let the bot wander */
            bs->ltgtype        = 0;
            bs->ordered        = qfalse;
            bs->teamgoal_time  = 0;
            break;

        case DIR_SET_SUBTEAM:
            /* One-shot: write subteam name (or clear it) and self-expire */
            Q_strncpyz( bs->directives.subteam, d->target_name,
                        sizeof( bs->directives.subteam ) );
            d->type = DIR_NONE;
            break;

        default:
            break;
    }
}

/* =========================================================================
   DIRECTIVE LOCK — DEFENSIVE COMBAT
   ========================================================================= */

/*
==================
WiredBots_DefensiveCombat
==================
Called from AINode_Seek_LTG / AINode_Seek_NBG when directiveLocked is set.

The bot keeps navigating toward its directive objective while returning fire
at any visible enemy.  It does NOT switch into a Battle node — movement
continues on the LTG/NBG path, but aiming and firing happen here.
*/
void WiredBots_DefensiveCombat( bot_state_t *bs ) {
    /* BotFindEnemy sets bs->enemy to the most threatening visible foe.
       BotAimAtEnemy adjusts bs->ideal_viewangles toward that enemy.
       BotCheckAttack fires if range and conditions allow.
       None of these calls switch the AI node — the bot stays in Seek_LTG/NBG
       and continues moving toward its objective. */
    if ( BotFindEnemy( bs, -1 ) ) {
        BotAimAtEnemy( bs );
        BotCheckAttack( bs );
    }
}

/* =========================================================================
   AUTHORIZATION
   ========================================================================= */

/*
==================
BotAuthorizeOrder
==================
Returns qtrue if issuer_client may give orders to this bot.

Rules:
  - issuer_client < 0 → server console: always authorized
  - Same team: authorized
  - Different team or spectator: denied
*/
qboolean BotAuthorizeOrder( bot_state_t *bs, int issuer_client ) {
    if ( issuer_client < 0 ) {
        return qtrue;   /* server console */
    }
    if ( issuer_client >= MAX_CLIENTS ) {
        return qfalse;
    }
    if ( level.clients[issuer_client].pers.connected != CON_CONNECTED ) {
        return qfalse;
    }
    /* stateless clients are authorized for all bots on all teams */
    if ( level.clients[issuer_client].sess.isStatelessClient ) {
        return qtrue;
    }
    return BotSameTeam( bs, issuer_client );
}

/* =========================================================================
   DIRECTIVE DELIVERY
   ========================================================================= */

/*
==================
ResolveClientByName  (internal)
==================
Finds a connected client whose name matches (case-insensitive prefix match).
Returns client num or -1.
*/
static int ResolveClientByName( const char *name ) {
    int   i;
    char  clean[MAX_NETNAME];

    if ( !name || !name[0] ) {
        return -1;
    }

    for ( i = 0; i < MAX_CLIENTS; i++ ) {
        if ( level.clients[i].pers.connected != CON_CONNECTED ) {
            continue;
        }
        Q_strncpyz( clean, level.clients[i].pers.netname, sizeof( clean ) );
        Q_CleanStr( clean );
        if ( Q_stricmpn( clean, name, strlen( name ) ) == 0 ) {
            return i;
        }
    }
    return -1;
}

/*
==================
BotReceiveDirective
==================
Parse an order string and store it as the bot's active directive.
Called from BotDirective_ConsoleOrder and (Phase 3) BotMatchMessage.

Order syntax examples:
  "follow Sarge"
  "defend"  / "defend base"
  "camp"
  "patrol"
  "get flag"
  "rush base"
  "return flag"
  "attack base"
  "kill Sarge"
  "harvest"
  "roam"
  "hunt Sarge"     ← tactic, not a goal directive
  "avoid Sarge"
  "retreat"
  "rush"           ← tactic RUSH (do not stop for items)
  "ambush"
  "join <subteam>" ← join a named subteam
  "leave"          ← leave current subteam
*/
void BotReceiveDirective( bot_state_t *bs, int issuer_client, const char *order ) {
    char            tok[64];
    char            rest[128];
    const char     *space;
    int             toklen;
    botDirective_t *d = &bs->directives.tactical;
    float           now = FloatTime();

    if ( !BotAuthorizeOrder( bs, issuer_client ) ) {
        return;
    }

    /* tokenize: first word into tok, remainder into rest */
    space = strchr( order, ' ' );
    if ( space ) {
        toklen = (int)( space - order );
        if ( toklen >= (int)sizeof( tok ) ) {
            toklen = (int)sizeof( tok ) - 1;
        }
        memcpy( tok, order, toklen );
        tok[toklen] = '\0';
        /* skip spaces between tok and rest */
        while ( *space == ' ' || *space == '\t' ) {
            space++;
        }
        Q_strncpyz( rest, space, sizeof( rest ) );
        /* strip trailing whitespace from rest */
        {
            int len = (int)strlen( rest ) - 1;
            while ( len >= 0 && ( rest[len] == ' ' || rest[len] == '\t' ||
                                   rest[len] == '\r' || rest[len] == '\n' ) ) {
                rest[len--] = '\0';
            }
        }
    } else {
        Q_strncpyz( tok, order, sizeof( tok ) );
        rest[0] = '\0';
    }

    /* ── tactic modifiers (do not replace the tactical directive) ──── */
    if ( Q_stricmp( tok, "hunt" ) == 0 ) {
        bs->directives.tactic        = TACTIC_HUNT;
        bs->directives.tactic_target = ResolveClientByName( rest );
        bs->directives.tactic_active = qtrue;
        if ( !TeamPlayIsOn() )
            trap_SendServerCommand( -1, va( "print \"^5[Bot %s]^7 Tactic: hunt %s\n\"",
                level.clients[bs->client].pers.netname, rest ) );
        return;
    }
    if ( Q_stricmp( tok, "avoid" ) == 0 ) {
        bs->directives.tactic        = TACTIC_AVOID;
        bs->directives.tactic_target = ResolveClientByName( rest );
        bs->directives.tactic_active = qtrue;
        if ( !TeamPlayIsOn() )
            trap_SendServerCommand( -1, va( "print \"^5[Bot %s]^7 Tactic: avoid %s\n\"",
                level.clients[bs->client].pers.netname, rest ) );
        return;
    }
    if ( Q_stricmp( tok, "retreat" ) == 0 ) {
        bs->directives.tactic        = TACTIC_RETREAT;
        bs->directives.tactic_target = -1;
        bs->directives.tactic_active = qtrue;
        if ( !TeamPlayIsOn() )
            trap_SendServerCommand( -1, va( "print \"^5[Bot %s]^7 Tactic: retreat\n\"",
                level.clients[bs->client].pers.netname ) );
        return;
    }
    if ( Q_stricmp( tok, "ambush" ) == 0 ) {
        bs->directives.tactic        = TACTIC_AMBUSH;
        bs->directives.tactic_target = -1;
        bs->directives.tactic_active = qtrue;
        if ( !TeamPlayIsOn() )
            trap_SendServerCommand( -1, va( "print \"^5[Bot %s]^7 Tactic: ambush\n\"",
                level.clients[bs->client].pers.netname ) );
        return;
    }
    /* "rush" alone is a TACTIC_RUSH; "rush base" is a directive handled below */
    if ( Q_stricmp( tok, "rush" ) == 0 && ( !rest[0] || Q_stricmp( rest, "base" ) != 0 ) ) {
        bs->directives.tactic        = TACTIC_RUSH;
        bs->directives.tactic_target = -1;
        bs->directives.tactic_active = qtrue;
        if ( !TeamPlayIsOn() )
            trap_SendServerCommand( -1, va( "print \"^5[Bot %s]^7 Tactic: rush\n\"",
                level.clients[bs->client].pers.netname ) );
        return;
    }

    /* ── tactical directives ──────────────────────────────────────── */
    memset( d, 0, sizeof( *d ) );
    d->source_client = issuer_client;
    d->issue_time    = now;
    d->expire_time   = 0;
    d->target_client = -1;

    if ( Q_stricmp( tok, "follow" ) == 0 ||
         Q_stricmp( tok, "escort" ) == 0 ||
         Q_stricmp( tok, "protect" ) == 0 ) {
        /* "follow/escort/protect carrier|the carrier|flag carrier" → follow
           whoever on the team currently holds the enemy flag.  The target is
           re-resolved every frame in FrameUpdate so the directive stays live
           if the carrier dies and a teammate picks up the flag. */
        qboolean carrierTarget =
            Q_stricmp( rest, "carrier" )          == 0 ||
            Q_stricmp( rest, "the carrier" )       == 0 ||
            Q_stricmp( rest, "flag carrier" )      == 0 ||
            Q_stricmp( rest, "the flag carrier" )  == 0;
        d->type        = DIR_FOLLOW;
        d->expire_time = now + TEAM_ACCOMPANY_TIME;
        if ( carrierTarget ) {
            Q_strncpyz( d->target_name, "flag carrier", sizeof( d->target_name ) );
            /* Resolve now; -1 means nobody has the flag yet — FrameUpdate retries. */
            d->target_client = BotTeamFlagCarrier( bs );
        } else if ( Q_stricmp( rest, "me" ) == 0 || Q_stricmp( rest, "myself" ) == 0 ) {
            /* "follow me" — target is the issuer themselves */
            d->target_client = issuer_client;
            if ( issuer_client >= 0 && issuer_client < MAX_CLIENTS ) {
                ClientName( issuer_client, d->target_name, sizeof( d->target_name ) );
                Q_CleanStr( d->target_name );
            } else {
                Q_strncpyz( d->target_name, "me", sizeof( d->target_name ) );
            }
        } else {
            Q_strncpyz( d->target_name, rest, sizeof( d->target_name ) );
            d->target_client = ResolveClientByName( rest );
            if ( d->target_client < 0 ) {
                d->type = DIR_NONE;
                trap_Cvar_Set( "wiredbot_ack", va( "rejected: unknown target '%s'", rest ) );
                WiredBots_Announce( bs, WB_ACK_UNKNOWN_TARGET, rest );
                return;
            }
        }
    }
    else if ( Q_stricmp( tok, "defend" ) == 0 ||
              Q_stricmp( tok, "guard" )  == 0 ) {
        /* "defend/guard flag|the flag|our flag" in a CTF game → defend the
           team's own flag spawn using the pre-computed ctf_redflag/blueflag
           goal (same globals BotVoiceChat_Defend uses). */
        qboolean flagTarget =
            Q_stricmp( rest, "flag" )      == 0 ||
            Q_stricmp( rest, "the flag" )  == 0 ||
            Q_stricmp( rest, "our flag" )  == 0;
        d->type = DIR_DEFEND_AREA;
        d->expire_time = now + TEAM_DEFENDKEYAREA_TIME;
        if ( flagTarget && ( gametype == GT_CTF || gametype == GT_1FCTF ) ) {
            bot_goal_t *flagGoal = NULL;
            switch ( BotTeam( bs ) ) {
                case TEAM_RED:  flagGoal = &ctf_redflag;  break;
                case TEAM_BLUE: flagGoal = &ctf_blueflag; break;
                default: break;
            }
            if ( flagGoal && flagGoal->areanum ) {
                VectorCopy( flagGoal->origin, d->area_origin );
                d->area_radius = 300.0f;
            } else {
                /* Fallback: defend current position if goal not yet initialised */
                VectorCopy( bs->origin, d->area_origin );
                d->area_radius = 200.0f;
            }
        } else {
            /* Generic "defend/guard" with no flag target → defend current pos */
            VectorCopy( bs->origin, d->area_origin );
            d->area_radius = 200.0f;
        }
    }
    else if ( Q_stricmp( tok, "camp" ) == 0 ) {
        d->type = DIR_CAMP_SPOT;
        VectorCopy( bs->origin, d->area_origin );
        d->expire_time = now + TEAM_CAMP_TIME;
    }
    else if ( Q_stricmp( tok, "patrol" ) == 0 ) {
        d->type        = DIR_PATROL;
        d->expire_time = now + TEAM_PATROL_TIME;
    }
    else if ( Q_stricmp( tok, "get" ) == 0 ) {
        d->type = DIR_SEEK_ITEM;
        Q_strncpyz( d->item_classname, rest, sizeof( d->item_classname ) );
        d->expire_time = now + TEAM_GETITEM_TIME;
    }
    else if ( Q_stricmp( tok, "rush" ) == 0 && Q_stricmp( rest, "base" ) == 0 ) {
        d->type        = DIR_RUSH_BASE;
        d->expire_time = now + CTF_RUSHBASE_TIME;
    }
    else if ( Q_stricmp( tok, "return" ) == 0 ) {
        d->type        = DIR_RETURN_FLAG;
        d->expire_time = now + CTF_RETURNFLAG_TIME;
    }
    else if ( Q_stricmp( tok, "attack" ) == 0 ) {
        d->type        = DIR_ATTACK_BASE;
        d->expire_time = now + TEAM_ATTACKENEMYBASE_TIME;
    }
    else if ( Q_stricmp( tok, "kill" ) == 0 ) {
        d->target_client = ResolveClientByName( rest );
        if ( d->target_client < 0 ) {
            trap_Cvar_Set( "wiredbot_ack", va( "rejected: unknown target '%s'", rest ) );
            WiredBots_Announce( bs, WB_ACK_UNKNOWN_TARGET, rest );
            return;
        }
        d->type          = DIR_KILL_TARGET;
        Q_strncpyz( d->target_name, rest, sizeof( d->target_name ) );
        d->expire_time   = now + TEAM_KILL_SOMEONE;
    }
    else if ( Q_stricmp( tok, "harvest" ) == 0 ) {
        d->type        = DIR_HARVEST;
        d->expire_time = now + TEAM_HARVEST_TIME;
    }
    else if ( Q_stricmp( tok, "roam" ) == 0 ) {
        d->type        = DIR_ROAM;
        d->expire_time = now + 30.0f;   /* auto-clear roam after 30 s */
    }
    else if ( Q_stricmp( tok, "join" ) == 0 && rest[0] ) {
        /* "join <subteam>" — one-shot; FrameUpdate applies it immediately */
        d->type = DIR_SET_SUBTEAM;
        Q_strncpyz( d->target_name, rest, sizeof( d->target_name ) );
        d->expire_time = 0;
    }
    else if ( Q_stricmp( tok, "leave" ) == 0 && !rest[0] ) {
        /* "leave" with no argument — clears subteam membership */
        d->type = DIR_SET_SUBTEAM;
        d->target_name[0] = '\0';
        d->expire_time = 0;
    }
    else {
        /* Unknown order — clear directive, do not disrupt existing goal */
        d->type = DIR_NONE;
    }

    /* Lock directive and snapshot player state for pickup detection.
       Done before the ack so the baseline is accurate at issue time. */
    if ( d->type != DIR_NONE ) {
        bs->directives.directiveLocked   = qtrue;
        bs->directives.armorAtStart      = bs->cur_ps.stats[STAT_ARMOR];
        bs->directives.healthAtStart     = bs->cur_ps.stats[STAT_HEALTH];
        bs->directives.lastProgressCheck = 0;   /* FrameUpdate takes snapshot on first frame */
        bs->directives.lastProgressDist  = 0.0f;
    }

    /* Acknowledge recognised tactical directives with a team chat message.
       Tactic modifiers (hunt/avoid/retreat/ambush/rush) return early above
       and do not reach this point. */
    if ( d->type != DIR_NONE ) {
        trap_Cvar_Set( "wiredbot_ack",
                       va( "accepted: %s (directiveLocked=1)",
                           level.clients[bs->client].pers.netname ) );
        WiredBots_Announce( bs, WB_ACK_YES, NULL );
        /* In non-team modes WB_ACK_YES is team_only and never fires.
           Print the directive explicitly so the player can see it. */
        if ( !TeamPlayIsOn() ) {
            trap_SendServerCommand( -1, va( "print \"^5[Bot %s]^7 Directive: %s\n\"",
                level.clients[bs->client].pers.netname, order ) );
        }
    }

    BotDirective_UpdateConfigstring( bs );
}

/*
==================
BotDirective_IssueToDirect
==================
Direct (non-chat) delivery used by BotCTFOrders (Phase 4).
Bypasses string parsing; caller supplies already-resolved values.
*/
void BotDirective_IssueToDirect( bot_state_t *bs,
                                  int issuer_client,
                                  directiveType_t type,
                                  int target_client,
                                  vec3_t area_origin ) {
    botDirective_t *d   = &bs->directives.tactical;
    float           now = FloatTime();

    if ( !BotAuthorizeOrder( bs, issuer_client ) ) {
        return;
    }

    memset( d, 0, sizeof( *d ) );
    d->type          = type;
    d->source_client = issuer_client;
    d->issue_time    = now;
    d->target_client = target_client;

    if ( area_origin ) {
        VectorCopy( area_origin, d->area_origin );
    }

    /* set expiry per directive type */
    switch ( type ) {
        case DIR_FOLLOW:         d->expire_time = now + TEAM_ACCOMPANY_TIME;        break;
        case DIR_DEFEND_AREA:    d->expire_time = now + TEAM_DEFENDKEYAREA_TIME;    break;
        case DIR_CAMP_SPOT:      d->expire_time = now + TEAM_CAMP_TIME;             break;
        case DIR_PATROL:         d->expire_time = now + TEAM_PATROL_TIME;           break;
        case DIR_SEEK_ITEM:      d->expire_time = now + TEAM_GETITEM_TIME;          break;
        case DIR_RUSH_BASE:      d->expire_time = now + CTF_RUSHBASE_TIME;          break;
        case DIR_RETURN_FLAG:    d->expire_time = now + CTF_RETURNFLAG_TIME;        break;
        case DIR_ATTACK_BASE:    d->expire_time = now + TEAM_ATTACKENEMYBASE_TIME;  break;
        case DIR_KILL_TARGET:    d->expire_time = now + TEAM_KILL_SOMEONE;          break;
        case DIR_HARVEST:        d->expire_time = now + TEAM_HARVEST_TIME;          break;
        case DIR_ROAM:           d->expire_time = now + 30.0f;                      break;
        default:                 d->expire_time = 0;                                break;
    }

    /* Lock directive and snapshot state for pickup detection */
    if ( type != DIR_NONE && type != DIR_ROAM ) {
        bs->directives.directiveLocked   = qtrue;
        bs->directives.armorAtStart      = bs->cur_ps.stats[STAT_ARMOR];
        bs->directives.healthAtStart     = bs->cur_ps.stats[STAT_HEALTH];
        bs->directives.needsBaselineReset = qfalse;
        /* Reset seek state — will be populated by FrameUpdate on first pursue */
        VectorClear( bs->directives.seekPosition );
        bs->directives.useRawPosition    = qfalse;
        bs->directives.rawMoveStartTime  = 0.0f;
    }

    BotDirective_UpdateConfigstring( bs );
}

/* =========================================================================
   CHAT PARSING
   ========================================================================= */

/*
==================
BotDirective_ParseChatOrder
==================
Called at the end of BotMatchMessage() for messages not matched by existing
MSG_* patterns (Phase 3 wires this in).  Parses free-form team-chat and
routes to BotReceiveDirective().

Only responds to messages that start with the bot's own name or "all".
*/
void BotDirective_ParseChatOrder( bot_state_t *bs, int talker, const char *msg ) {
    char name[MAX_NETNAME];
    char botname[MAX_NETNAME];
    char msgcopy[MAX_MESSAGE_SIZE];
    char *colon;
    char *body;

    if ( !msg || !msg[0] ) {
        return;
    }

    if ( !BotAuthorizeOrder( bs, talker ) ) {
        return;
    }

    Q_strncpyz( msgcopy, msg, sizeof( msgcopy ) );

    /* Expected format: "<name>: <order>" or just "<order>" */
    colon = strchr( msgcopy, ':' );
    if ( colon ) {
        *colon = '\0';
        Q_strncpyz( name, msgcopy, sizeof( name ) );
        Q_CleanStr( name );
        body = colon + 1;
        while ( *body == ' ' || *body == '\t' ) body++;

        /* Check if addressed to this bot or to "all" */
        trap_BotLibVarGet( "name", botname, sizeof( botname ) );
        /* Fall through: if name is "all" or matches bot's name, process it */
        if ( Q_stricmp( name, "all" ) != 0 ) {
            char cleanbot[MAX_NETNAME];
            Q_strncpyz( cleanbot, botname, sizeof( cleanbot ) );
            Q_CleanStr( cleanbot );
            if ( Q_stricmpn( cleanbot, name, strlen( name ) ) != 0 ) {
                return;   /* addressed to a different bot */
            }
        }
    } else {
        body = msgcopy;
    }

    BotReceiveDirective( bs, talker, body );
}

/* =========================================================================
   CHAT RESPONSE
   ========================================================================= */

/*
==================
BotDirective_RespondTeamChat
==================
Send a team-chat acknowledgement to the directive issuer.
Uses the bot's configured chat system.  Falls back gracefully if the
"order_ack" or "cmd_accompany" chat keys don't exist in this bot's file.
*/
void BotDirective_RespondTeamChat( bot_state_t *bs, int talker, const char *msg ) {
    (void)msg;   /* not inspected for the response */

    if ( trap_BotNumInitialChats( bs->cs, "order_ack" ) > 0 ) {
        BotAI_BotInitialChat( bs, "order_ack", NULL );
        trap_BotEnterChat( bs->cs, talker, CHAT_TELL );
    } else if ( trap_BotNumInitialChats( bs->cs, "cmd_accompany" ) > 0 ) {
        BotAI_BotInitialChat( bs, "cmd_accompany", NULL );
        trap_BotEnterChat( bs->cs, talker, CHAT_TELL );
    }
    /* If neither key exists, the bot stays silent (acceptable) */
}

/* =========================================================================
   CONSOLE COMMAND
   ========================================================================= */

/*
==================
BotDirective_ConsoleOrder
==================
Handler for the "bot_order <botname> <order>" server console command.
Searches botstates[] for a matching bot and delivers the directive.

Called from G_BotOrder() in g_bot.c.
*/
void BotDirective_ConsoleOrder( const char *bot_name, const char *order ) {
    int   i;
    char  clean[MAX_NETNAME];

    if ( !bot_name || !bot_name[0] || !order || !order[0] ) {
        Com_Log( SEV_INFO, LOG_CH(ch_game), "Usage: bot_order <botname> <order>\n" );
        return;
    }

    for ( i = 0; i < MAX_CLIENTS; i++ ) {
        if ( !botstates[i] || !botstates[i]->inuse ) {
            continue;
        }
        Q_strncpyz( clean, level.clients[i].pers.netname, sizeof( clean ) );
        Q_CleanStr( clean );
        if ( Q_stricmpn( clean, bot_name, strlen( bot_name ) ) == 0 ) {
            BotReceiveDirective( botstates[i], -1 /* console */, order );
            Com_Log( SEV_INFO, LOG_CH(ch_game), "Directive sent to %s: %s\n",
                        level.clients[i].pers.netname, order );
            return;
        }
    }
    Com_Log( SEV_INFO, LOG_CH(ch_game), "bot_order: no active bot named '%s'\n", bot_name );
}

/* =========================================================================
   GOAL OVERRIDE
   ========================================================================= */

/*
==================
BotDirective_OverrideGoal
==================
If the active directive specifies a fixed position (DEFEND/CAMP), replaces
the passed goal with one pointing at the directive's area_origin.

Called from AINode_Seek_LTG (Phase 2 wires this in) when ltgtype matches.
*/
void BotDirective_OverrideGoal( bot_state_t *bs, bot_goal_t *goal ) {
    botDirective_t *d = &bs->directives.tactical;

    /* For seek-item directives, enforce the goal computed in FrameUpdate.
       BotLongTermGoal calls trap_BotGetLevelItemGoal independently each frame
       and may return a stale/empty goal when the item isn't in the AAS goal
       list.  Overriding here ensures our synthetic goal (including fallbacks
       built from entity world position) is what the movement engine uses. */
    if ( d->type == DIR_SEEK_ITEM && bs->directives.directiveLocked
         && bs->teamgoal.areanum > 0 ) {
        *goal = bs->teamgoal;
        return;
    }

    if ( d->type != DIR_DEFEND_AREA && d->type != DIR_CAMP_SPOT ) {
        return;
    }

    memset( goal, 0, sizeof( *goal ) );
    VectorCopy( d->area_origin, goal->origin );
    goal->areanum  = BotPointAreaNum( d->area_origin );
    goal->entitynum = -1;
    goal->flags    = 0;
}

/* =========================================================================
   ITEM SCORING
   ========================================================================= */

/*
==================
BotDirective_ScoreItem
==================
Adjusts the item desirability score based on the active directive and tactic.

Returns qtrue if the score was modified so that the caller can decide
whether to apply it.
*/
/* =========================================================================
   ANNOUNCEMENT SYSTEM
   ========================================================================= */

typedef struct {
    const char *text;       /* team-chat text; "" = voice-only              */
    qboolean    team_only;  /* qtrue = use trap_EA_SayTeam, else EA_Say     */
    float       cooldown;   /* per-type minimum seconds between firings     */
    const char *voice_id;   /* vsay_team / vsay command id; NULL = no voice */
} wbAnnounceEntry_t;

static const wbAnnounceEntry_t s_announcements[WB_ANNOUNCE_NUM_TYPES] = {
    /* WB_ACK_YES         */ { "Yes!",                qfalse,  2.0f, "yes"         },
    /* WB_STATUS_DEFENSE  */ { "On defense.",          qtrue,  15.0f, "imondefense" },
    /* WB_STATUS_INPOS    */ { "In position.",         qtrue,  10.0f, "inposition"  },
    /* WB_STATUS_GETFLAG  */ { "Going for the flag!",  qtrue,  15.0f, "ongetflag"   },
    /* WB_STATUS_RTNFLAG  */ { "Returning the flag.",  qtrue,  15.0f, "onreturnflag"},
    /* WB_STATUS_OFFENSE  */ { "On offense.",          qtrue,  15.0f, "imonoffense" },
    /* WB_STATUS_LEADER   */ { "I'm leading.",         qtrue,  30.0f, "startleader" },
    /* WB_TAUNT_GENERIC   */ { "",                     qtrue,  20.0f, "taunt"       },
    /* WB_TAUNT_PRAISE    */ { "Good game!",           qtrue,  30.0f, "taunt"       },
    /* WB_TAUNT_DEATH     */ { "",                     qtrue,  10.0f, "taunt"       },
    /* WB_TAUNT_KILL      */ { "",                     qtrue,  10.0f, "taunt"       },
    /* WB_ACK_UNKNOWN_TARGET */ { "",                 qtrue,   3.0f, NULL          },
};

static qboolean WiredBots_ShouldAnnounce( wbAnnounceType_t type ) {
    /* Status / ack / rejection types always fire — functional information. */
    if ( type < WB_TAUNT_GENERIC || type == WB_ACK_UNKNOWN_TARGET ) {
        return qtrue;
    }
    /* Taunts are flavor — fire ~50 % of the time to avoid repetition. */
    return ( random() < 0.5f ) ? qtrue : qfalse;
}

/*
==================
WiredBots_Announce
==================
Send a team-chat status message and (if FEAT_TA_VOICECHAT is enabled) a
matching voice command.  Cooldown tracking prevents spam.

context is reserved for future use (e.g., mentioning a player name).
*/
void WiredBots_Announce( bot_state_t *bs, wbAnnounceType_t type, const char *context ) {
    const wbAnnounceEntry_t *entry;
    float now;

    if ( !bs || !bs->wiredBotsActive ) {
        return;
    }
    if ( type < 0 || type >= WB_ANNOUNCE_NUM_TYPES ) {
        return;
    }

    entry = &s_announcements[type];
    now   = FloatTime();

    /* per-type cooldown */
    if ( now - bs->directives.lastAnnounceTime[type] < entry->cooldown ) {
        return;
    }
    /* global cooldown: 2 s minimum gap between any two announcements */
    if ( now - bs->directives.lastAnnounceAnyTime < 2.0f ) {
        return;
    }

    if ( !WiredBots_ShouldAnnounce( type ) ) {
        return;
    }

    /* stamp cooldowns before sending (prevents re-entry stacking) */
    bs->directives.lastAnnounceTime[type] = now;
    bs->directives.lastAnnounceAnyTime    = now;

    /* dynamic rejection: "Who is X?" */
    if ( type == WB_ACK_UNKNOWN_TARGET ) {
        char msg[MAX_SAY_TEXT];
        if ( context && context[0] ) {
            Com_sprintf( msg, sizeof(msg), "Who is %s?", context );
        } else {
            Q_strncpyz( msg, "Who?", sizeof(msg) );
        }
        if ( entry->team_only && TeamPlayIsOn() ) {
            trap_EA_SayTeam( bs->client, msg );
        } else {
            trap_EA_Say( bs->client, msg );
        }
        return;
    }

    /* text message */
    if ( entry->text && entry->text[0] ) {
        if ( entry->team_only && TeamPlayIsOn() ) {
            trap_EA_SayTeam( bs->client, (char *)entry->text );
        } else if ( !entry->team_only ) {
            trap_EA_Say( bs->client, (char *)entry->text );
        }
    }

    /* voice command */
    if ( entry->voice_id ) {
#if FEAT_TA_VOICECHAT
        if ( entry->team_only && TeamPlayIsOn() ) {
            trap_EA_Command( bs->client, va( "vsay_team %s", entry->voice_id ) );
        } else {
            trap_EA_Command( bs->client, va( "vsay %s", entry->voice_id ) );
        }
#endif
    }
}

/* =========================================================================
   ITEM SCORING
   ========================================================================= */

qboolean BotDirective_ScoreItem( bot_state_t *bs, bot_goal_t *goal, float *score_multiplier ) {
    botDirective_t *d = &bs->directives.tactical;

    if ( !goal || !score_multiplier ) {
        return qfalse;
    }

    /* RUSH_BASE / ATTACK_BASE: deprioritise all item pickups so the bot
       keeps moving toward the objective instead of detouring. */
    if ( d->type == DIR_RUSH_BASE || d->type == DIR_ATTACK_BASE ) {
        *score_multiplier *= 0.4f;
        return qtrue;
    }

    /* TACTIC_RUSH: same deprioritisation for item pickups */
    if ( bs->directives.tactic == TACTIC_RUSH && bs->directives.tactic_active ) {
        *score_multiplier *= 0.5f;
        return qtrue;
    }

    /* SEEK_ITEM: boost the item we're after; the classname check is a
       best-effort string match against the goal's entity classname. */
    if ( d->type == DIR_SEEK_ITEM && d->item_classname[0] ) {
        if ( goal->entitynum >= 0 && goal->entitynum < MAX_GENTITIES ) {
            const gentity_t *ent = &g_entities[goal->entitynum];
            if ( ent->classname &&
                 Q_stricmp( ent->classname, d->item_classname ) == 0 ) {
                *score_multiplier *= 3.0f;
                return qtrue;
            }
        }
    }

    return qfalse;
}

/* =========================================================================
   @ MENTION ADDRESSING SYSTEM
   ========================================================================= */

/* ── Private helpers ─────────────────────────────────────────────────── */

/* Case-insensitive prefix match against the bot's cleaned netname. */
static qboolean WiredBots_NameMatches( bot_state_t *bs, const char *name ) {
    char clean[MAX_NETNAME];
    Q_strncpyz( clean, level.clients[bs->client].pers.netname, sizeof( clean ) );
    Q_CleanStr( clean );
    return ( Q_stricmpn( clean, name, strlen( name ) ) == 0 ) ? qtrue : qfalse;
}

/* Returns the client number of the enemy carrying OUR flag, or -1. */
static int BotEnemyFlagCarrier( bot_state_t *bs ) {
    int      myTeam, i;
    gentity_t *ent;

    if ( bs->client < 0 || bs->client >= MAX_CLIENTS ) {
        return -1;
    }
    if ( !g_entities[bs->client].client ) {
        return -1;
    }
    myTeam = g_entities[bs->client].client->sess.sessionTeam;

    for ( i = 0; i < level.maxclients; i++ ) {
        ent = &g_entities[i];
        if ( !ent->inuse || !ent->client ) {
            continue;
        }
        if ( level.clients[i].pers.connected != CON_CONNECTED ) {
            continue;
        }
        if ( ent->client->sess.sessionTeam == myTeam ) {
            continue;   /* skip own team */
        }
        /* Enemy carrying OUR flag */
        if ( myTeam == TEAM_RED ) {
            if ( ent->client->ps.powerups[PW_REDFLAG] ) {
                return i;
            }
        } else {
            if ( ent->client->ps.powerups[PW_BLUEFLAG] ) {
                return i;
            }
        }
    }
    return -1;
}

/* Returns the client number of the bot on sender's team closest to sender. */
static int WiredBots_FindClosestBot( int senderClient ) {
    vec3_t senderOrigin, diff;
    int    senderTeam, i, best;
    float  bestDistSq, distSq;

    if ( senderClient < 0 || senderClient >= MAX_CLIENTS ) {
        return -1;
    }
    if ( !g_entities[senderClient].client ) {
        return -1;
    }
    VectorCopy( g_entities[senderClient].client->ps.origin, senderOrigin );
    senderTeam = g_entities[senderClient].client->sess.sessionTeam;

    best = -1;
    bestDistSq = 1e30f;

    for ( i = 0; i < MAX_CLIENTS; i++ ) {
        if ( !botstates[i] || !botstates[i]->inuse ) {
            continue;
        }
        if ( !g_entities[i].client ) {
            continue;
        }
        if ( g_entities[i].client->sess.sessionTeam != senderTeam ) {
            continue;
        }
        if ( level.clients[i].pers.connected != CON_CONNECTED ) {
            continue;
        }
        VectorSubtract( senderOrigin, g_entities[i].client->ps.origin, diff );
        distSq = DotProduct( diff, diff );
        if ( distSq < bestDistSq ) {
            bestDistSq = distSq;
            best = i;
        }
    }
    return best;
}

/* Returns the client number of the bot on sender's team farthest from sender. */
static int WiredBots_FindFarthestBot( int senderClient ) {
    vec3_t senderOrigin, diff;
    int    senderTeam, i, best;
    float  bestDistSq, distSq;

    if ( senderClient < 0 || senderClient >= MAX_CLIENTS ) {
        return -1;
    }
    if ( !g_entities[senderClient].client ) {
        return -1;
    }
    VectorCopy( g_entities[senderClient].client->ps.origin, senderOrigin );
    senderTeam = g_entities[senderClient].client->sess.sessionTeam;

    best = -1;
    bestDistSq = -1.0f;

    for ( i = 0; i < MAX_CLIENTS; i++ ) {
        if ( !botstates[i] || !botstates[i]->inuse ) {
            continue;
        }
        if ( !g_entities[i].client ) {
            continue;
        }
        if ( g_entities[i].client->sess.sessionTeam != senderTeam ) {
            continue;
        }
        if ( level.clients[i].pers.connected != CON_CONNECTED ) {
            continue;
        }
        VectorSubtract( senderOrigin, g_entities[i].client->ps.origin, diff );
        distSq = DotProduct( diff, diff );
        if ( distSq > bestDistSq ) {
            bestDistSq = distSq;
            best = i;
        }
    }
    return best;
}

/*
==================
WiredBots_ResolveMention  (internal)
==================
Resolves a recipient @mention string to a list of matching bot client numbers.
Returns a pointer to a static array; *count is set to the number of entries.
*/
static int *WiredBots_ResolveMention( const char *mention, int senderClient, int *count ) {
    static int results[MAX_CLIENTS];
    const char *name;
    int         senderTeam, i;
    int         resolveTeam;   /* -1 = all bots; >=0 = filter by that team_t value */
    int         candidates[MAX_CLIENTS];
    int         numCandidates;

    *count = 0;

    if ( !mention || mention[0] != '@' ) {
        return results;
    }
    name = mention + 1;   /* skip '@' */

    senderTeam = TEAM_FREE;
    if ( senderClient >= 0 && senderClient < MAX_CLIENTS &&
         g_entities[senderClient].client ) {
        senderTeam = g_entities[senderClient].client->sess.sessionTeam;
    }

    /* ── team-prefix: @red:selector, @blue:selector ─────────────────────
       Parse an optional "team:" prefix before the selector keyword/name.
       Stateless clients and regular players can both use this to target
       a specific team.  When no prefix is present, stateless clients
       address all bots; regular players address only their own team. */
    resolveTeam = -2;   /* sentinel: not yet determined */
    {
        const char *colon = strchr( name, ':' );
        if ( colon && colon > name ) {
            int  prefixLen = (int)( colon - name );
            char prefix[16];
            if ( prefixLen < (int)sizeof( prefix ) ) {
                Q_strncpyz( prefix, name, prefixLen + 1 );
                if      ( Q_stricmp( prefix, "red"  ) == 0 ) resolveTeam = TEAM_RED;
                else if ( Q_stricmp( prefix, "blue" ) == 0 ) resolveTeam = TEAM_BLUE;
                else if ( Q_stricmp( prefix, "free" ) == 0 ) resolveTeam = TEAM_FREE;
            }
            if ( resolveTeam >= 0 ) {
                name = colon + 1;   /* advance past the "team:" prefix */
            }
        }
    }
    if ( resolveTeam == -2 ) {
        /* no valid team prefix found — fall back to per-sender rules */
        qboolean isStateless = ( senderClient >= 0 && senderClient < MAX_CLIENTS &&
                                  g_entities[senderClient].client &&
                                  g_entities[senderClient].client->sess.isStatelessClient );
        resolveTeam = isStateless ? -1 : (int)senderTeam;
    }

/* Iterates all bots, optionally filtered to resolveTeam.
   resolveTeam == -1 → no team filter (all bots). */
#define WB_TEAM_BOT_ITER( body ) \
    for ( i = 0; i < MAX_CLIENTS; i++ ) { \
        if ( !botstates[i] || !botstates[i]->inuse ) continue; \
        if ( !g_entities[i].client ) continue; \
        if ( resolveTeam >= 0 && \
             g_entities[i].client->sess.sessionTeam != (team_t)resolveTeam ) continue; \
        if ( level.clients[i].pers.connected != CON_CONNECTED ) continue; \
        body \
    }

    /* ── group keywords ──────────────────────────────────────────────── */
    if ( Q_stricmp( name, "everyone" ) == 0 ||
         Q_stricmp( name, "all" )      == 0 ||
         Q_stricmp( name, "team" )     == 0 ) {
        WB_TEAM_BOT_ITER( results[(*count)++] = i; )
        return results;
    }

    if ( Q_stricmp( name, "closest" ) == 0 ) {
        int best = WiredBots_FindClosestBot( senderClient );
        if ( best >= 0 ) {
            results[(*count)++] = best;
        }
        return results;
    }

    if ( Q_stricmp( name, "farthest" ) == 0 ) {
        int best = WiredBots_FindFarthestBot( senderClient );
        if ( best >= 0 ) {
            results[(*count)++] = best;
        }
        return results;
    }

    if ( Q_stricmp( name, "random" )    == 0 ||
         Q_stricmp( name, "somebody" )  == 0 ||
         Q_stricmp( name, "anyone" )    == 0 ) {
        numCandidates = 0;
        WB_TEAM_BOT_ITER( candidates[numCandidates++] = i; )
        if ( numCandidates > 0 ) {
            results[(*count)++] = candidates[ (int)( random() * numCandidates ) % numCandidates ];
        }
        return results;
    }

    if ( Q_stricmp( name, "defenders" ) == 0 ) {
        WB_TEAM_BOT_ITER(
            if ( botstates[i]->directives.preference & TEAMTP_DEFENDER ) {
                results[(*count)++] = i;
            }
        )
        return results;
    }

    if ( Q_stricmp( name, "attackers" ) == 0 ) {
        WB_TEAM_BOT_ITER(
            if ( botstates[i]->directives.preference & TEAMTP_ATTACKER ) {
                results[(*count)++] = i;
            }
        )
        return results;
    }

    if ( Q_stricmp( name, "idle" ) == 0 ) {
        WB_TEAM_BOT_ITER(
            if ( botstates[i]->directives.tactical.type == DIR_NONE &&
                 botstates[i]->enemy < 0 ) {
                results[(*count)++] = i;
            }
        )
        return results;
    }

#undef WB_TEAM_BOT_ITER

    /* ── named bot — prefix match against cleaned netname ────────────── */
    for ( i = 0; i < MAX_CLIENTS; i++ ) {
        if ( !botstates[i] || !botstates[i]->inuse ) {
            continue;
        }
        if ( level.clients[i].pers.connected != CON_CONNECTED ) {
            continue;
        }
        if ( resolveTeam >= 0 && g_entities[i].client &&
             g_entities[i].client->sess.sessionTeam != (team_t)resolveTeam ) {
            continue;
        }
        if ( WiredBots_NameMatches( botstates[i], name ) ) {
            results[(*count)++] = i;
            return results;   /* name match is always a single bot */
        }
    }

    return results;
}

/*
==================
WiredBots_ResolveTargetMention  (internal)
==================
Resolves a target @mention within an order body to a single client number.
Returns -1 if unresolvable.
*/
static int WiredBots_ResolveTargetMention( bot_state_t *bs, const char *mention ) {
    const char *name;

    if ( !mention || mention[0] != '@' ) {
        return -1;
    }
    name = mention + 1;

    if ( Q_stricmp( name, "carrier" ) == 0 ) {
        return BotTeamFlagCarrier( bs );
    }
    if ( Q_stricmp( name, "enemycarrier" ) == 0 ) {
        return BotEnemyFlagCarrier( bs );
    }
    if ( Q_stricmp( name, "leader" ) == 0 ) {
        if ( bs->directives.teamleader[0] ) {
            return ResolveClientByName( bs->directives.teamleader );
        }
        return -1;
    }

    /* Any connected player — any team */
    return ResolveClientByName( name );
}

/*
==================
WiredBots_ExtractMention  (internal)
==================
Extracts the @word at the start of text into mention[].
Returns pointer to the remainder of the string (the order body).
Returns NULL if text does not start with '@'.
*/
static const char *WiredBots_ExtractMention( const char *text, char *mention, int mentionSize ) {
    int i;

    if ( !text || text[0] != '@' ) {
        return NULL;
    }

    i = 0;
    while ( text[i] && text[i] != ' ' && i < mentionSize - 1 ) {
        mention[i] = text[i];
        i++;
    }
    mention[i] = '\0';

    /* skip whitespace between mention and order */
    while ( text[i] == ' ' ) {
        i++;
    }
    return &text[i];
}

/*
==================
WiredBots_ExtractTargetMention  (internal)
==================
Scans order text for a @word that is at a word boundary (preceded by space
or at string start).  Extracts it into targetMention[] and writes the
cleaned order text (without the @word) to cleanOrder[].
*/
static void WiredBots_ExtractTargetMention( const char *order,
                                             char *cleanOrder, int cleanSize,
                                             char *targetMention, int targetSize ) {
    const char *at, *p, *s;
    int         ci, ti;

    targetMention[0] = '\0';

    /* Find first @ that is at a word boundary */
    at = NULL;
    for ( p = order; *p; p++ ) {
        if ( *p == '@' && ( p == order || *(p - 1) == ' ' ) ) {
            at = p;
            break;
        }
    }

    if ( !at ) {
        Q_strncpyz( cleanOrder, order, cleanSize );
        return;
    }

    /* Extract @word */
    p = at;
    ti = 0;
    while ( *p && *p != ' ' && ti < targetSize - 1 ) {
        targetMention[ti++] = *p++;
    }
    targetMention[ti] = '\0';

    /* Build clean order: text before @ (strip trailing space) + text after @word */
    ci = 0;
    for ( s = order; s < at && ci < cleanSize - 1; s++ ) {
        cleanOrder[ci++] = *s;
    }
    while ( ci > 0 && cleanOrder[ci - 1] == ' ' ) {
        ci--;
    }
    /* skip spaces after @word, then append remainder */
    while ( *p == ' ' ) {
        p++;
    }
    if ( *p && ci < cleanSize - 1 ) {
        cleanOrder[ci++] = ' ';
        while ( *p && ci < cleanSize - 1 ) {
            cleanOrder[ci++] = *p++;
        }
    }
    cleanOrder[ci] = '\0';
}

/*
==================
WiredBots_BuildFinalOrder  (internal)
==================
Combines a clean verb with a resolved target to produce the final order
string for BotReceiveDirective.

Special cases:
  "attack <player>"   → "kill <player>"   (bare "attack" = DIR_ATTACK_BASE,
                                           we want DIR_KILL_TARGET)
  any verb + "flag carrier" keyword
                      → "<verb> flag carrier"
*/
static void WiredBots_BuildFinalOrder( const char *cleanOrder,
                                        const char *targetMention,
                                        bot_state_t *bs,
                                        char *finalOrder, int finalSize ) {
    char  verb[64];
    const char *space;
    int   verbLen;
    int   targetClient;
    char  targetStr[MAX_NETNAME];
    qboolean isFollowVerb;

    if ( !targetMention || !targetMention[0] ) {
        Q_strncpyz( finalOrder, cleanOrder, finalSize );
        return;
    }

    /* extract verb (first word of cleanOrder) */
    space = strchr( cleanOrder, ' ' );
    if ( space ) {
        verbLen = (int)( space - cleanOrder );
        if ( verbLen >= (int)sizeof( verb ) ) {
            verbLen = (int)sizeof( verb ) - 1;
        }
        memcpy( verb, cleanOrder, verbLen );
        verb[verbLen] = '\0';
    } else {
        Q_strncpyz( verb, cleanOrder, sizeof( verb ) );
    }

    isFollowVerb = ( Q_stricmp( verb, "follow" )  == 0 ||
                     Q_stricmp( verb, "escort" )  == 0 ||
                     Q_stricmp( verb, "protect" ) == 0 ) ? qtrue : qfalse;

    /* @carrier + follow-like verb → use the "flag carrier" sentinel so
       BotReceiveDirective sets dynamic re-targeting in FrameUpdate */
    if ( isFollowVerb && Q_stricmp( targetMention + 1, "carrier" ) == 0 ) {
        Com_sprintf( finalOrder, finalSize, "%s flag carrier", verb );
        return;
    }

    /* resolve target to a client number */
    targetClient = WiredBots_ResolveTargetMention( bs, targetMention );
    if ( targetClient < 0 ) {
        /* target not found — issue order without target rather than silently dropping */
        Q_strncpyz( finalOrder, cleanOrder, finalSize );
        return;
    }

    /* get target's clean display name */
    ClientName( targetClient, targetStr, sizeof( targetStr ) );
    Q_CleanStr( targetStr );
    if ( !targetStr[0] ) {
        Q_strncpyz( finalOrder, cleanOrder, finalSize );
        return;
    }

    /* verb → order string mapping */
    if ( Q_stricmp( verb, "attack" ) == 0 ) {
        /* "attack @X" has no player-target syntax in BotReceiveDirective,
           but "kill <name>" does (DIR_KILL_TARGET) */
        Com_sprintf( finalOrder, finalSize, "kill %s", targetStr );
    } else {
        Com_sprintf( finalOrder, finalSize, "%s %s", verb, targetStr );
    }
}

/* ── Public API ──────────────────────────────────────────────────────── */

/*
==================
WiredBots_ProcessChat
==================
Central entry point for @ mention directive dispatch.  Called once per
chat message from G_Say BEFORE the text is relayed to clients.

  1.  If the message starts with '@', extracts the recipient mention and
      the order body.
  2.  Resolves the recipient to a list of bot client numbers.
  3.  Optionally resolves a target '@' mention within the order body.
  4.  Issues the directive to each recipient via BotReceiveDirective.
  5.  Populates *result for the caller (G_Say uses it to colorize the
      displayed text).

Messages without a leading '@' are left for the per-bot legacy path in
BotDirective_ParseChatOrder; result->hasMentions is set to qfalse.
*/
void WiredBots_ProcessChat( int senderClient, const char *message,
                             wbParseResult_t *result ) {
    char        recipientMention[MAX_NETNAME];
    char        targetMention[MAX_NETNAME];
    char        cleanOrder[256];
    char        finalOrder[256];
    const char *orderStart;
    int        *recipients;
    int         count, i;

    memset( result, 0, sizeof( *result ) );

    if ( !message || message[0] != '@' ) {
        return;
    }

    /* extract recipient @mention from the start of the message */
    orderStart = WiredBots_ExtractMention( message, recipientMention,
                                           sizeof( recipientMention ) );
    if ( !orderStart || !orderStart[0] ) {
        /* bare '@mention' with no order — still colorize but no directive */
        if ( recipientMention[0] ) {
            result->hasMentions = qtrue;
            Q_strncpyz( result->recipientMention, recipientMention,
                        sizeof( result->recipientMention ) );
        }
        return;
    }

    /* resolve recipients */
    recipients = WiredBots_ResolveMention( recipientMention, senderClient, &count );
    if ( count == 0 ) {
        return;
    }

    /* fill result */
    result->hasMentions  = qtrue;
    result->numRecipients = count;
    Q_strncpyz( result->recipientMention, recipientMention,
                sizeof( result->recipientMention ) );
    for ( i = 0; i < count && i < MAX_CLIENTS; i++ ) {
        result->recipientClients[i] = recipients[i];
    }

    /* extract target @mention from order body (if any) */
    WiredBots_ExtractTargetMention( orderStart, cleanOrder, sizeof( cleanOrder ),
                                    targetMention, sizeof( targetMention ) );
    if ( targetMention[0] ) {
        Q_strncpyz( result->targetMention, targetMention,
                    sizeof( result->targetMention ) );
    }

    /* deliver directive to each recipient */
    for ( i = 0; i < count; i++ ) {
        bot_state_t *bs = botstates[recipients[i]];
        if ( !bs || !bs->inuse ) {
            continue;
        }

        if ( targetMention[0] ) {
            WiredBots_BuildFinalOrder( cleanOrder, targetMention, bs,
                                       finalOrder, sizeof( finalOrder ) );
        } else {
            Q_strncpyz( finalOrder, cleanOrder, sizeof( finalOrder ) );
        }

        if ( finalOrder[0] ) {
            BotReceiveDirective( bs, senderClient, finalOrder );
        }
    }
}

/*
==================
WiredBots_ColorizeMentions
==================
Colorizes @mentions in chat text for display to human clients.

  Recipient @mentions → ^5 (cyan)
  Target @mentions    → ^3 (yellow)

Only @words at word boundaries are colorized (same rule as extraction).
Output is guaranteed NUL-terminated and at most outputSize-1 characters.
*/
void WiredBots_ColorizeMentions( const char *input, char *output, int outputSize,
                                  const char *recipientMention,
                                  const char *targetMention ) {
    const char *p;
    int         o;
    char        mention[MAX_NETNAME];
    int         mi;
    const char *color;

    if ( !input || !output || outputSize <= 0 ) {
        if ( output && outputSize > 0 ) {
            output[0] = '\0';
        }
        return;
    }

    o = 0;
    p = input;

    while ( *p && o < outputSize - 12 ) {
        /* detect @ at a word boundary */
        if ( *p == '@' && ( p == input || *(p - 1) == ' ' ) ) {
            /* extract the @word */
            mi = 0;
            while ( *p && *p != ' ' && mi < (int)sizeof( mention ) - 1 ) {
                mention[mi++] = *p++;
            }
            mention[mi] = '\0';

            /* pick color: recipient (cyan) or target (yellow) */
            if ( recipientMention && Q_stricmp( mention, recipientMention ) == 0 ) {
                color = "^5";
            } else if ( targetMention && targetMention[0] &&
                        Q_stricmp( mention, targetMention ) == 0 ) {
                color = "^3";
            } else {
                color = "^5";   /* unknown @ defaults to cyan */
            }

            /* write: ^color @word ^7 */
            output[o++] = color[0];
            output[o++] = color[1];
            {
                const char *s;
                for ( s = mention; *s && o < outputSize - 4; s++ ) {
                    output[o++] = *s;
                }
            }
            output[o++] = '^';
            output[o++] = '7';
        } else {
            output[o++] = *p++;
        }
    }

    output[o] = '\0';
}
