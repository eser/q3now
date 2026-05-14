// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
===========================================================================
ai_dodge.c — missile avoidance for bot AI improvements

Bots detect incoming projectiles and evaluate 9 dodge directions
(8 cardinal + maintain heading). For each direction, predict closest
approach of all tracked missiles over a 0.7s lookahead. Pick randomly
from the 3 safest directions.

Skill scaling:
  Skill 1-2: evaluate 3 directions only
  Skill 3+:  evaluate all 9 directions
  Lower skill = smaller detection FOV, delayed dodge start
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
#include "ai_dodge.h"
#include "wired/bots/g_bot_scripts.h"

#define DODGE_LOOKAHEAD		0.7f	// seconds to predict ahead
#define DODGE_NUM_DIRS		9		// 8 cardinal + maintain heading
#define DODGE_BEST_DIRS		3		// pick from N safest directions

/*
==================
TrajectoryClosestDistSq

Predict the minimum squared distance between two linearly moving points
over a time window [0, lookahead]. Used for missile-vs-bot prediction.
==================
*/
float TrajectoryClosestDistSq( vec3_t p1, vec3_t v1,
                               vec3_t p2, vec3_t v2,
                               float lookahead )
{
	vec3_t dp, dv;
	float a, b, t, dist;

	VectorSubtract( p1, p2, dp );
	VectorSubtract( v1, v2, dv );

	a = DotProduct( dv, dv );
	if ( a < 0.001f ) {
		// parallel or stationary — distance is constant
		return DotProduct( dp, dp );
	}

	b = DotProduct( dp, dv );
	t = -b / a;

	// clamp to [0, lookahead]
	if ( t < 0.0f ) t = 0.0f;
	if ( t > lookahead ) t = lookahead;

	// compute position difference at time t
	dp[0] += dv[0] * t;
	dp[1] += dv[1] * t;
	dp[2] += dv[2] * t;

	dist = DotProduct( dp, dp );
	return dist;
}

/*
==================
BotScanMissiles

Scan g_entities[] for ET_MISSILE entities aimed near this bot.
Populate bs->missile_dodge[] with up to MAX_MISSILE_DODGE entries.
==================
*/
void BotScanMissiles( struct bot_state_s *bs )
{
	gentity_t	*ent;
	int			i;
	float		skill;
	float		detectFov;
	vec3_t		toMissile, forward;

	bs->num_missiles = 0;
	skill = WiredBots_EffectiveSkill( bs );

	// skill 1-2: narrow detection FOV
	detectFov = ( skill >= 3 ) ? 0.0f : 0.5f; // 0 = 180° fov, 0.5 = ~120° fov

	AngleVectors( bs->viewangles, forward, NULL, NULL );

	for ( i = MAX_CLIENTS, ent = &g_entities[i]; i < level.num_entities; i++, ent++ ) {
		if ( !ent->inuse ) continue;
		if ( ent->s.eType != ET_MISSILE ) continue;
		if ( ent->r.ownerNum == bs->entitynum ) continue; // own missile
		if ( ent->s.pos.trType != TR_LINEAR ) continue;   // only straight trajectories

		// check if missile is roughly aimed toward us
		VectorSubtract( ent->r.currentOrigin, bs->origin, toMissile );
		VectorNormalize( toMissile );
		if ( DotProduct( forward, toMissile ) < -detectFov ) continue; // behind us

		if ( bs->num_missiles >= MAX_MISSILE_DODGE ) break;

		bs->missile_dodge[bs->num_missiles].entnum = i;
		bs->missile_dodge[bs->num_missiles].eType = ET_MISSILE;
		bs->num_missiles++;
	}
}

/*
==================
BotDodgeMovement

Evaluate dodge directions and set bs->dodge_dir / bs->dodge_active.

Algorithm:
  1. Build 9 candidate directions (8 cardinal + maintain heading)
  2. For each direction, simulate bot moving at 320 ups for DODGE_LOOKAHEAD
  3. Compute minimum distance to ALL tracked missiles
  4. Score each direction by worst-case splash exposure
  5. Sort by safety (highest min-distance first)
  6. Pick randomly from the DODGE_BEST_DIRS safest directions
==================
*/
void BotDodgeMovement( struct bot_state_s *bs )
{
	gentity_t	*missile;
	vec3_t		forward, right;
	vec3_t		dodgeDirs[DODGE_NUM_DIRS];
	float		dodgeSafety[DODGE_NUM_DIRS];
	int			numDirs, i, j, k, picked;
	float		skill;
	float		botSpeed = 320.0f;	// approximate run speed
	vec3_t		botVel;
	float		bestSafety;
	int			bestIndices[DODGE_BEST_DIRS];
	int			numBest;
	float		worstDist, distSq, splashSq, safety;
	int			entnum, bestIdx, chosen;

	VectorClear( bs->dodge_dir );
	bs->dodge_active = qfalse;

	if ( bs->num_missiles <= 0 ) {
		return;
	}

	skill = WiredBots_EffectiveSkill( bs );

	// build candidate directions in world space
	AngleVectors( bs->viewangles, forward, right, NULL );
	forward[2] = 0; VectorNormalize( forward );
	right[2] = 0;   VectorNormalize( right );

	// direction 0: maintain current heading (no dodge)
	VectorClear( dodgeDirs[0] );

	// directions 1-8: 8 cardinal directions
	//   1=forward, 2=back, 3=right, 4=left
	//   5=fwd-right, 6=fwd-left, 7=back-right, 8=back-left
	VectorCopy( forward, dodgeDirs[1] );
	VectorScale( forward, -1, dodgeDirs[2] );
	VectorCopy( right, dodgeDirs[3] );
	VectorScale( right, -1, dodgeDirs[4] );

	VectorAdd( forward, right, dodgeDirs[5] );   VectorNormalize( dodgeDirs[5] );
	VectorSubtract( forward, right, dodgeDirs[6] ); VectorNormalize( dodgeDirs[6] );
	VectorAdd( dodgeDirs[2], right, dodgeDirs[7] );  VectorNormalize( dodgeDirs[7] );
	VectorSubtract( dodgeDirs[2], right, dodgeDirs[8] ); VectorNormalize( dodgeDirs[8] );

	// skill 1-2: only evaluate 3 directions (forward, left, right)
	numDirs = ( skill >= 3 ) ? DODGE_NUM_DIRS : 3;
	if ( numDirs == 3 ) {
		// remap: 0=maintain, 1=right, 2=left
		VectorCopy( right, dodgeDirs[1] );
		VectorScale( right, -1, dodgeDirs[2] );
	}

	// evaluate each direction
	for ( i = 0; i < numDirs; i++ ) {
		worstDist = 999999.0f;

		// compute simulated bot velocity for this dodge direction
		VectorScale( dodgeDirs[i], botSpeed, botVel );

		// check against every tracked missile
		for ( j = 0; j < bs->num_missiles; j++ ) {
			entnum = bs->missile_dodge[j].entnum;
			missile = &g_entities[entnum];

			// validate entity is still a missile
			if ( !missile->inuse || missile->s.eType != ET_MISSILE ) continue;

			distSq = TrajectoryClosestDistSq(
				bs->origin, botVel,
				missile->r.currentOrigin, missile->s.pos.trDelta,
				DODGE_LOOKAHEAD
			);

			// factor in splash radius — closer to splash edge = more dangerous
			splashSq = (float)missile->splashRadius * (float)missile->splashRadius;
			safety = distSq - splashSq; // negative = inside splash zone

			if ( safety < worstDist ) {
				worstDist = safety;
			}
		}

		dodgeSafety[i] = worstDist;
	}

	// find the DODGE_BEST_DIRS safest directions
	numBest = 0;
	for ( i = 0; i < DODGE_BEST_DIRS && i < numDirs; i++ ) {
		bestIdx = -1;
		bestSafety = -999999.0f;

		for ( j = 0; j < numDirs; j++ ) {
			// skip already picked
			picked = 0;
			for ( k = 0; k < numBest; k++ ) {
				if ( bestIndices[k] == j ) { picked = 1; break; }
			}
			if ( picked ) continue;

			if ( dodgeSafety[j] > bestSafety ) {
				bestSafety = dodgeSafety[j];
				bestIdx = j;
			}
		}

		if ( bestIdx >= 0 ) {
			bestIndices[numBest++] = bestIdx;
		}
	}

	if ( numBest <= 0 ) {
		return; // all directions equally bad, don't dodge
	}

	// pick randomly from the best directions (human-like unpredictability)
	chosen = bestIndices[ rand() % numBest ];

	if ( chosen == 0 ) {
		return; // "maintain heading" was chosen — no dodge
	}

	VectorCopy( dodgeDirs[chosen], bs->dodge_dir );
	bs->dodge_active = qtrue;

#if FEAT_WIREDNET_OBSERVER
	// emit dodge event for QUIC observer
	{
		int wp = 0;
		if ( bs->num_missiles > 0 ) {
			gentity_t *m = &g_entities[bs->missile_dodge[0].entnum];
			if ( m->inuse ) wp = m->s.weapon;
		}
		trap_WiredNet_EmitBotEvent( bs->entitynum, "dodge", chosen, wp, bs->origin );
	}
#endif
}
