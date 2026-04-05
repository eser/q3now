// g_shotgun.c -- Shotgun weapon implementation
#include "../g_local.h"
#include "../bg_promode.h"

extern float s_quadFactor;
extern vec3_t forward, right, up;
extern vec3_t muzzle;
extern int G_DamageFalloff( int damage, vec3_t start, vec3_t end, float maxDamageDistance );
extern void SnapVectorTowards( vec3_t v, vec3_t to );

/*
  Why the dwell phase is everything: In Doom, the shotgun pump doesn't start until 7 tics (200ms) after firing. During that gap, the muzzle flash
  is fading and the recoil is settling — your brain registers the shot as complete. Then the pump starts as a separate event. Without the dwell,
  recoil and pump blur into one motion and it feels like generic weapon bob.

  The new 5-phase timeline (900ms total, fits in 1000ms reload):
  0ms        100ms       240ms       490ms       740ms       900ms
   |  RECOIL  |   DWELL   | PUMP BACK | PUMP FWD  |  SETTLE  |
   |  kick↑   | flash fade|  ch-      |  -chk     |  ready   |
   | pitch -6°|  ease out | pitch -10°| pitch→0   |  idle    |
   |          |           | pull -3.5 | push out  |          |

  Easing curves matter: Phase 3 uses t*(2-t) (ease-out) so the pump decelerates into the back position — like a hand gripping and pulling. Phase 4
   uses 1-t² (ease-in) so the pump accelerates forward — like releasing a spring. This asymmetry is what makes pump-actions feel mechanical rather
   than robotic.

  The rhythm is BOOM → pause → ch-chk → ready.
*/

// DEFAULT_SHOTGUN_SPREAD and DEFAULT_SHOTGUN_COUNT	are in bg_public.h, because
// client predicts same spreads
#define	DEFAULT_SHOTGUN_DAMAGE	8

qboolean ShotgunPellet( vec3_t start, vec3_t end, gentity_t *ent, int mod ) {
	trace_t		tr;
	int			damage, i, passent;
	gentity_t	*traceEnt;
#if FEAT_PW_INVULNERABILITY
	vec3_t		impactpoint, bouncedir;
#endif
	vec3_t		tr_start, tr_end;
	qboolean	hitClient = qfalse;

	passent = ent->s.number;
	VectorCopy( start, tr_start );
	VectorCopy( end, tr_end );
	for (i = 0; i < 10; i++) {
		trap_Trace (&tr, tr_start, NULL, NULL, tr_end, passent, MASK_SHOT);
		traceEnt = &g_entities[ tr.entityNum ];

		// send bullet impact
		if (  tr.surfaceFlags & SURF_NOIMPACT ) {
			return qfalse;
		}

		if ( traceEnt->takedamage ) {
			damage = DEFAULT_SHOTGUN_DAMAGE * s_quadFactor;
#if FEAT_PW_INVULNERABILITY
			if ( traceEnt->client && traceEnt->client->invulnerabilityTime > level.time ) {
				if (G_InvulnerabilityEffect( traceEnt, forward, tr.endpos, impactpoint, bouncedir )) {
					G_BounceProjectile( tr_start, impactpoint, bouncedir, tr_end );
					VectorCopy( impactpoint, tr_start );
					// the player can hit him/herself with the bounced rail
					passent = ENTITYNUM_NONE;
				}
				else {
					VectorCopy( tr.endpos, tr_start );
					passent = traceEnt->s.number;
				}
				continue;
			}
#endif
			if( LogAccuracyHit( traceEnt, ent ) ) {
				hitClient = qtrue;
			}

			{
				int pDamage = damage;

				// eser - damage falloff
				pDamage = G_DamageFalloff( pDamage, muzzle, tr.endpos, bg_attacklist[ATT_SHOTGUN_PRIMARY].maxDamageDistance );
				// eser - damage falloff

				G_Damage( traceEnt, ent, ent, forward, tr.endpos, pDamage, 0, mod );
			}

			return hitClient;
		}
		return qfalse;
	}
	return qfalse;
}

// this should match CG_ShotgunPattern
void ShotgunPattern( vec3_t origin, vec3_t origin2, int seed, gentity_t *ent ) {
	int			i;
	float		r, u;
	vec3_t		end;
	vec3_t		localForward, localRight, localUp;
	qboolean	hitClient = qfalse;

	// derive the right and up vectors from the forward vector, because
	// the client won't have any other information
	VectorNormalize2( origin2, localForward );
	PerpendicularVector( localRight, localForward );
	CrossProduct( localForward, localRight, localUp );

#if FEAT_SHOTGUN_PATTERN
	{
		float rotation = ( seed / 256.0f ) * 2.0f * M_PI;
		float spreadScale = DEFAULT_SHOTGUN_SPREAD * 16;

		for ( i = 0; i < DEFAULT_SHOTGUN_COUNT; i++ ) {
			float angle = bg_shotgunPattern[i].angle + rotation;
			float radius = bg_shotgunPattern[i].radius * spreadScale;

			r = radius * cos( angle );
			u = radius * sin( angle );

			VectorMA( origin, 8192 * 16, localForward, end );
			VectorMA( end, r, localRight, end );
			VectorMA( end, u, localUp, end );

			if ( ShotgunPellet( origin, end, ent, MOD_SHOTGUN ) && !hitClient ) {
				hitClient = qtrue;
				if ( ent->client ) {
					ent->client->accuracy_hits++;
					ent->client->attackStats[ATT_SHOTGUN_PRIMARY].hits++;
				}
			}
		}
	}
#else
	// generate the "random" spread pattern
	for ( i = 0 ; i < DEFAULT_SHOTGUN_COUNT ; i++ ) {
		r = Q_crandom( &seed ) * DEFAULT_SHOTGUN_SPREAD * 16;
		u = Q_crandom( &seed ) * DEFAULT_SHOTGUN_SPREAD * 16;
		VectorMA( origin, 8192 * 16, localForward, end);
		VectorMA (end, r, localRight, end);
		VectorMA (end, u, localUp, end);
		if( ShotgunPellet( origin, end, ent, MOD_SHOTGUN ) && !hitClient ) {
			hitClient = qtrue;

			if (ent->client) {
				ent->client->accuracy_hits++;
				ent->client->attackStats[ATT_SHOTGUN_PRIMARY].hits++;
			}
		}
	}
#endif
}


void Attack_Shotgun_Primary (gentity_t *ent) {
	gentity_t		*tent;

	if (ent->client) {
		ent->client->accuracy_shots++;
		ent->client->attackStats[ATT_SHOTGUN_PRIMARY].shots++;
	}

	// send shotgun blast
	tent = G_TempEntity( muzzle, EV_SHOTGUN );
	VectorScale( forward, 4096, tent->s.origin2 );
	SnapVector( tent->s.origin2 );
	tent->s.eventParm = rand() & 255;		// seed for spread pattern
	tent->s.otherEntityNum = ent->s.number;

	ShotgunPattern( tent->s.pos.trBase, tent->s.origin2, tent->s.eventParm, ent );
}

void ShotgunPatternSpread( vec3_t origin, vec3_t origin2, int seed, gentity_t *ent, int spread, int attackIdx, int mod ) {
	int			i;
	float		r, u;
	vec3_t		end;
	vec3_t		localForward, localRight, localUp;
	qboolean	hitClient = qfalse;

	VectorNormalize2( origin2, localForward );
	PerpendicularVector( localRight, localForward );
	CrossProduct( localForward, localRight, localUp );

#if FEAT_SHOTGUN_PATTERN
	{
		float rotation = ( seed / 256.0f ) * 2.0f * M_PI;
		float spreadScale = spread * 16;

		for ( i = 0; i < DEFAULT_SHOTGUN_COUNT; i++ ) {
			float angle = bg_shotgunPattern[i].angle + rotation;
			float radius = bg_shotgunPattern[i].radius * spreadScale;

			r = radius * cos( angle );
			u = radius * sin( angle );

			VectorMA( origin, 8192 * 16, localForward, end );
			VectorMA( end, r, localRight, end );
			VectorMA( end, u, localUp, end );

			if ( ShotgunPellet( origin, end, ent, mod ) && !hitClient ) {
				hitClient = qtrue;
				if ( ent->client ) {
					ent->client->accuracy_hits++;
					ent->client->attackStats[attackIdx].hits++;
				}
			}
		}
	}
#else
	for ( i = 0 ; i < DEFAULT_SHOTGUN_COUNT ; i++ ) {
		r = Q_crandom( &seed ) * spread * 16;
		u = Q_crandom( &seed ) * spread * 16;
		VectorMA( origin, 8192 * 16, localForward, end);
		VectorMA (end, r, localRight, end);
		VectorMA (end, u, localUp, end);
		if( ShotgunPellet( origin, end, ent, mod ) && !hitClient ) {
			hitClient = qtrue;
			if (ent->client) {
				ent->client->accuracy_hits++;
				ent->client->attackStats[attackIdx].hits++;
			}
		}
	}
#endif
}

void Attack_Shotgun_DoubleBlast( gentity_t *ent ) {
	gentity_t		*tent;

	if (ent->client) {
		ent->client->accuracy_shots++;
		ent->client->attackStats[ATT_SHOTGUN_DOUBLE_BLAST].shots++;
	}

	// send shotgun blast with wide spread event for double-blast
	tent = G_TempEntity( muzzle, EV_SHOTGUN_WIDE );
	VectorScale( forward, 4096, tent->s.origin2 );
	SnapVector( tent->s.origin2 );
	tent->s.eventParm = rand() & 255;
	tent->s.otherEntityNum = ent->s.number;

	ShotgunPatternSpread( tent->s.pos.trBase, tent->s.origin2, tent->s.eventParm, ent, DEFAULT_SHOTGUN_DOUBLE_BLAST_SPREAD, ATT_SHOTGUN_DOUBLE_BLAST, MOD_SHOTGUN_DOUBLE_BLAST );
}
