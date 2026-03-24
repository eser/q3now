/*
===========================================================================
ai_aware.c — entity/sound awareness for bot AI improvements

Bots become aware of enemies through non-visual cues:
  - Missile detection → track missile's owner
  - EV_PLAYER_TELEPORT_IN → enemy teleported nearby
  - EV_OBITUARY → track kills/deaths for threat assessment

Skill-weighted event_radius using CHARACTERISTIC_ALERTNESS.
react_time delay before bot can act on awareness.
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
#include "ai_aware.h"

#if FEAT_BOT_IMPROVEMENTS

#define AWARE_BASE_RADIUS		1500.0f	// base detection radius (units)
#define AWARE_REACT_BASE		0.8f	// base reaction time (seconds)
#define AWARE_EXPIRE_TIME		5.0f	// seconds before awareness expires

/*
==================
BotAwareUpdate

Process entity events and expire old awareness entries.
==================
*/
void BotAwareUpdate( struct bot_state_s *bs )
{
	int i;
	float now = floattime;
	float skill, awareRadius;

	// expire old entries
	for ( i = 0; i < bs->num_aware; i++ ) {
		if ( now - bs->aware[i].first_noted > AWARE_EXPIRE_TIME ) {
			// shift remaining entries down
			if ( i < bs->num_aware - 1 ) {
				memmove( &bs->aware[i], &bs->aware[i + 1],
				         sizeof(bot_aware_t) * (bs->num_aware - 1 - i) );
			}
			bs->num_aware--;
			i--; // re-check this index
		}
	}

	skill = bs->autoskill > 0 ? bs->autoskill : bs->settings.skill;
	awareRadius = AWARE_BASE_RADIUS * (skill / 5.0f); // skill 1 = 300u, skill 5 = 1500u

	// --- trigger 1: missile owners ---
	// if we detected missiles (from BotScanMissiles), become aware of their owners
	for ( i = 0; i < bs->num_missiles; i++ ) {
		int mnum = bs->missile_dodge[i].entnum;
		gentity_t *missile = &g_entities[mnum];
		if ( !missile->inuse || missile->s.eType != ET_MISSILE ) continue;
		// track the player who fired this missile
		BotAwareTrackEntity( bs, missile->r.ownerNum, awareRadius * 2.0f ); // double radius for missile source
	}

	// --- trigger 2: entity events (teleport, sounds) ---
	for ( i = 0; i < MAX_CLIENTS; i++ ) {
		gentity_t *ent = &g_entities[i];
		if ( !ent->inuse || !ent->client ) continue;
		if ( i == bs->entitynum ) continue; // skip self

		int event = ent->s.event & ~EV_EVENT_BITS;
		if ( event == 0 ) continue;

		if ( event == EV_PLAYER_TELEPORT_IN || event == EV_PLAYER_TELEPORT_OUT ) {
			BotAwareTrackEntity( bs, i, awareRadius );
		}
	}
}

/*
==================
BotAwareTrackEntity

Add or refresh an entity in the awareness list.
==================
*/
void BotAwareTrackEntity( struct bot_state_s *bs, int entnum, float radius )
{
	int i;
	gentity_t *ent;
	float dist, skill, react;
	vec3_t delta;

	if ( entnum < 0 || entnum >= MAX_CLIENTS ) return;

	ent = &g_entities[entnum];
	if ( !ent->inuse || !ent->client ) return;

	// must be enemy
	if ( OnSameTeam( ent, &g_entities[bs->entitynum] ) ) return;

	// check distance
	VectorSubtract( ent->r.currentOrigin, bs->origin, delta );
	dist = VectorLength( delta );
	if ( dist > radius ) return;

	// check if already tracked
	for ( i = 0; i < bs->num_aware; i++ ) {
		if ( bs->aware[i].ent.entnum == entnum ) {
			// refresh — don't reset react_time
			return;
		}
	}

	// add new entry
	if ( bs->num_aware >= MAX_AWARE_ENTITIES ) {
		// evict oldest (LRU — index 0 is oldest)
		memmove( &bs->aware[0], &bs->aware[1],
		         sizeof(bot_aware_t) * (MAX_AWARE_ENTITIES - 1) );
		bs->num_aware = MAX_AWARE_ENTITIES - 1;
	}

	skill = bs->autoskill > 0 ? bs->autoskill : bs->settings.skill;
	react = AWARE_REACT_BASE * (6.0f - skill) / 5.0f; // skill 1 = 0.8s, skill 5 = 0.16s

	bs->aware[bs->num_aware].ent.entnum = entnum;
	bs->aware[bs->num_aware].ent.eType = ET_PLAYER;
	bs->aware[bs->num_aware].first_noted = floattime;
	bs->aware[bs->num_aware].react_time = floattime + react;
	bs->aware[bs->num_aware].visual = qfalse;
	bs->num_aware++;

#if FEAT_QUIC_OBSERVE
	trap_QUIC_EmitBotEvent( bs->entitynum, "aware", entnum, (int)dist, bs->origin );
#endif
}

/*
==================
BotBestAwareEnemy

Return entity number of the highest-rated aware enemy that the bot
can now act on (react_time has passed), or -1 if none.
==================
*/
int BotBestAwareEnemy( struct bot_state_s *bs )
{
	int i, best = -1;
	float bestDist = 99999.0f;
	float now = floattime;
	float dist;
	gentity_t *ent;
	vec3_t delta;

	for ( i = 0; i < bs->num_aware; i++ ) {
		if ( now < bs->aware[i].react_time ) continue; // still reacting

		// validate entity is still valid
		ent = &g_entities[bs->aware[i].ent.entnum];
		if ( !ent->inuse || !ent->client ) continue;
		if ( ent->client->ps.pm_type == PM_DEAD ) continue;

		VectorSubtract( ent->r.currentOrigin, bs->origin, delta );
		dist = VectorLength( delta );
		if ( dist < bestDist ) {
			bestDist = dist;
			best = bs->aware[i].ent.entnum;
		}
	}

	return best;
}

#endif // FEAT_BOT_IMPROVEMENTS
