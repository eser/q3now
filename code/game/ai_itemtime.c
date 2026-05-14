// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
===========================================================================
ai_itemtime.c — item respawn timing for bot AI improvements

Bots track when major items (mega health, red armor, powerups) will
respawn and time their routes to arrive 1-2s before the item appears.

Respawn constants mirror g_items.c defines.

Skill scaling: only skill 4+ actively times items.
===========================================================================
*/

#include "g_local.h"
#include "../qcommon/q_shared.h"
#include "../botlib/botlib.h"
#include "../botlib/be_aas.h"
#include "../botlib/be_ea.h"
#include "../botlib/be_ai_char.h"
#include "../botlib/be_ai_chat.h"
#include "../botlib/be_ai_gen.h"
#include "../botlib/be_ai_goal.h"
#include "../botlib/be_ai_move.h"
#include "../botlib/be_ai_weap.h"
//
#include "ai_main.h"
#include "ai_itemtime.h"
#include "wired/bots/g_bot_scripts.h"

// mirror g_items.c respawn values (seconds)
#define RESPAWN_ARMOR		30
#define RESPAWN_HEALTH		30
#define RESPAWN_POWERUP		90
#define RESPAWN_HOLDABLE	60

/*
==================
BotItemIsHighValue

Returns qtrue if an item entity is worth timing (mega health, red/yellow armor, powerups).
==================
*/
static qboolean BotItemIsHighValue( gentity_t *ent )
{
	gitem_t *item;

	if ( !ent->item ) return qfalse;
	item = ent->item;

	switch ( item->giType ) {
		case IT_ARMOR:
			// only time red and yellow armor (quantity >= 50)
			return ( item->quantity >= 50 ) ? qtrue : qfalse;
		case IT_HEALTH:
			// only time mega health (quantity >= 100)
			return ( item->quantity >= 100 ) ? qtrue : qfalse;
		case IT_POWERUP:
			return qtrue;
		case IT_HOLDABLE:
			return qtrue;
		default:
			return qfalse;
	}
}

/*
==================
BotItemRespawnTime

Returns respawn time in milliseconds for the given item type.
==================
*/
static int BotItemRespawnTime( gitem_t *item )
{
	switch ( item->giType ) {
		case IT_ARMOR:		return RESPAWN_ARMOR * 1000;
		case IT_HEALTH:		return RESPAWN_HEALTH * 1000;
		case IT_POWERUP:	return RESPAWN_POWERUP * 1000;
		case IT_HOLDABLE:	return RESPAWN_HOLDABLE * 1000;
		default:			return 30 * 1000;
	}
}

/*
==================
BotItemTimeInit

Scan g_entities[] at level start, register items worth timing.
==================
*/
void BotItemTimeInit( struct bot_state_s *bs )
{
	int i;
	gentity_t *ent;

	bs->num_timed_items = 0;

	for ( i = MAX_CLIENTS, ent = &g_entities[i]; i < level.num_entities; i++, ent++ ) {
		if ( !ent->inuse ) continue;
		if ( ent->s.eType != ET_ITEM ) continue;
		if ( !BotItemIsHighValue( ent ) ) continue;
		if ( bs->num_timed_items >= MAX_TIMED_ITEMS ) break;

		bs->timed_items[bs->num_timed_items].ent.entnum = i;
		bs->timed_items[bs->num_timed_items].ent.eType = ET_ITEM;
		bs->timed_items[bs->num_timed_items].itemType = ent->item->giType;
		VectorCopy( ent->r.currentOrigin, bs->timed_items[bs->num_timed_items].origin );
		bs->timed_items[bs->num_timed_items].areaNum = trap_AAS_PointAreaNum( ent->r.currentOrigin );
		bs->timed_items[bs->num_timed_items].respawnTime = BotItemRespawnTime( ent->item );
		bs->timed_items[bs->num_timed_items].pickupTime = 0; // available
		bs->num_timed_items++;
	}

	bs->items_initialized = qtrue;
}

/*
==================
BotItemTimeUpdate

Each frame, check if timed items have been picked up (entity not visible/linked)
or have respawned (entity visible again). Update pickup timestamps.
==================
*/
void BotItemTimeUpdate( struct bot_state_s *bs )
{
	gentity_t *ent;
	bot_itemtime_t *ti;

	if ( !bs->items_initialized ) {
		BotItemTimeInit( bs );
		return;
	}

	for ( int i = 0; i < bs->num_timed_items; i++ ) {
		ti = &bs->timed_items[i];
		ent = &g_entities[ti->ent.entnum];

		// validate entity is still an item
		if ( !ent->inuse || ent->s.eType != ET_ITEM ) continue;

		if ( ent->r.linked && ent->r.contents ) {
			// item is present (linked and has contents = available for pickup)
			ti->pickupTime = 0; // mark as available
		} else if ( ti->pickupTime == 0 ) {
			// item just disappeared — record pickup time
			ti->pickupTime = level.time;
		}
		// else: already tracking pickup time, keep it
	}
}

/*
==================
BotItemTimingGoal

Check if a high-value item is about to respawn soon.
Returns qtrue if the bot should move toward a timed item.
This is a lightweight check — it doesn't override goals directly,
just signals the caller.
==================
*/
qboolean BotItemTimingGoal( struct bot_state_s *bs )
{
	int i, bestItem;
	float bestValue, value;
	int timeUntilRespawn;
	bot_itemtime_t *ti;

	if ( WiredBots_EffectiveSkill( bs ) < 4.0f ) return qfalse; // only skill 4+ times items

	bestItem = -1;
	bestValue = 0;

	for ( i = 0; i < bs->num_timed_items; i++ ) {
		ti = &bs->timed_items[i];

		if ( ti->pickupTime == 0 ) continue; // item is available right now — no timing needed

		timeUntilRespawn = (ti->pickupTime + ti->respawnTime) - level.time;

		// only consider items respawning within 10 seconds
		if ( timeUntilRespawn > 10000 || timeUntilRespawn < -5000 ) continue;

		// value based on item type and how soon it respawns
		value = 1.0f;
		if ( ti->itemType == IT_POWERUP ) value = 3.0f;
		else if ( ti->itemType == IT_ARMOR ) value = 2.0f;
		else if ( ti->itemType == IT_HEALTH ) value = 2.0f;

		// prefer items respawning sooner
		if ( timeUntilRespawn > 0 ) {
			value *= (10000.0f - timeUntilRespawn) / 10000.0f;
		} else {
			value *= 1.5f; // already respawned but not picked up yet — high priority
		}

		if ( value > bestValue ) {
			bestValue = value;
			bestItem = i;
		}
	}

	return ( bestItem >= 0 ) ? qtrue : qfalse;
}
