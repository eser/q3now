// g_railgun.c -- Railgun weapon implementation
#include "../g_local.h"

extern float s_quadFactor;
extern vec3_t forward, right, up;
extern vec3_t muzzle;
extern int G_DamageFalloff( int damage, vec3_t start, vec3_t end, float maxDamageDistance );
extern void SnapVectorTowards( vec3_t v, vec3_t to );

/*
=================
Attack_Railgun_Primary
=================
*/
#define	MAX_RAIL_HITS	4
void Attack_Railgun_Primary (gentity_t *ent) {
	vec3_t		end;
	vec3_t impactpoint, bouncedir;
	trace_t		trace;
	gentity_t	*tent;
	gentity_t	*traceEnt;
	int			damage;
	int			i;
	int			hits;
	int			unlinked;
	int			passent;
	gentity_t	*unlinkedEntities[MAX_RAIL_HITS];

	damage = 100 * s_quadFactor;

	if (ent->client) {
		ent->client->accuracy_shots++;
		ent->client->attackStats[ATT_RAILGUN_PRIMARY].shots++;
	}

	VectorMA (muzzle, 8192, forward, end);

	// trace only against the solids, so the railgun will go through people
	unlinked = 0;
	hits = 0;
	passent = ent->s.number;
	do {
		trap_Trace (&trace, muzzle, NULL, NULL, end, passent, MASK_SHOT );
		if ( trace.entityNum >= ENTITYNUM_MAX_NORMAL ) {
			break;
		}
		traceEnt = &g_entities[ trace.entityNum ];
		if ( traceEnt->takedamage ) {
			if ( traceEnt->client && traceEnt->client->deflectorTime > level.time ) {
				if ( G_DeflectorEffect( traceEnt, forward, trace.endpos, impactpoint, bouncedir ) ) {
					G_BounceProjectile( muzzle, impactpoint, bouncedir, end );
					// snap the endpos to integers to save net bandwidth, but nudged towards the line
					SnapVectorTowards( trace.endpos, muzzle );
					// send railgun beam effect
					tent = G_TempEntity( trace.endpos, EV_RAILTRAIL );
					tent->r.svFlags |= SVF_BROADCAST;
					// set player number for custom colors on the railtrail
					tent->s.clientNum = ent->s.clientNum;
					VectorCopy( muzzle, tent->s.origin2 );
					// move origin a bit to come closer to the drawn gun muzzle
					VectorMA( tent->s.origin2, 4, right, tent->s.origin2 );
					VectorMA( tent->s.origin2, -1, up, tent->s.origin2 );
					tent->s.eventParm = 255;	// don't make the explosion at the end
					//
					VectorCopy( impactpoint, muzzle );
					// the player can hit him/herself with the bounced rail
					passent = ENTITYNUM_NONE;
				}
			}

			{
				int	sDamage = damage;

				if( LogAccuracyHit( traceEnt, ent ) ) {
					hits++;
				}

				// eser - damage falloff
				sDamage = G_DamageFalloff( sDamage, muzzle, trace.endpos, bg_attacklist[ATT_RAILGUN_PRIMARY].maxDamageDistance );
				// eser - damage falloff

				G_Damage( traceEnt, ent, ent, forward, trace.endpos, sDamage, 0, MOD_RAILGUN);
			}
		}
		if ( trace.contents & CONTENTS_SOLID ) {
			break;		// we hit something solid enough to stop the beam
		}
		// unlink this entity, so the next trace will go past it
		trap_UnlinkEntity( traceEnt );
		unlinkedEntities[unlinked] = traceEnt;
		unlinked++;
	} while ( unlinked < MAX_RAIL_HITS );

	// link back in any entities we unlinked
	for ( i = 0 ; i < unlinked ; i++ ) {
		trap_LinkEntity( unlinkedEntities[i] );
	}

	// the final trace endpos will be the terminal point of the rail trail

	// snap the endpos to integers to save net bandwidth, but nudged towards the line
	SnapVectorTowards( trace.endpos, muzzle );

	// send railgun beam effect
	tent = G_TempEntity( trace.endpos, EV_RAILTRAIL );
	tent->r.svFlags |= SVF_BROADCAST;

	// set player number for custom colors on the railtrail
	tent->s.clientNum = ent->s.clientNum;

	VectorCopy( muzzle, tent->s.origin2 );
	// move origin a bit to come closer to the drawn gun muzzle
	VectorMA( tent->s.origin2, 4, right, tent->s.origin2 );
	VectorMA( tent->s.origin2, -1, up, tent->s.origin2 );

	// no explosion at end if SURF_NOIMPACT, but still make the trail
	if ( trace.surfaceFlags & SURF_NOIMPACT ) {
		tent->s.eventParm = 255;	// don't make the explosion at the end
	} else {
		tent->s.eventParm = DirToByte( trace.plane.normal );
	}
	tent->s.clientNum = ent->s.clientNum;

	// give the shooter a reward sound if they have made two railgun hits in a row
	if ( ent->client ) {
		if ( hits == 0 ) {
			// complete miss
			ent->client->accurateCount = 0;
		} else {
			// check for "impressive" reward sound
			ent->client->accurateCount += hits;
			if ( ent->client->accurateCount >= 2 ) {
				ent->client->accurateCount -= 1;
				ent->client->ps.persistant[PERS_IMPRESSIVE_COUNT]++;
				// add the sprite over the player's head
				ent->client->ps.eFlags &= ~(EF_AWARD_IMPRESSIVE | EF_AWARD_EXCELLENT | EF_AWARD_GAUNTLET | EF_AWARD_ASSIST | EF_AWARD_DEFEND | EF_AWARD_CAP );
				ent->client->ps.eFlags |= EF_AWARD_IMPRESSIVE;
				ent->client->rewardTime = level.time + REWARD_SPRITE_TIME;
			}

			ent->client->accuracy_hits++;
			ent->client->attackStats[ATT_RAILGUN_PRIMARY].hits++;
		}
	}
}
