// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 1999-2005 Id Software, Inc.
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

// g_shotgun.c -- Shotgun weapon implementation
#include "../g_local.h"

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
	vec3_t		impactpoint, bouncedir;
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

			if ( traceEnt->client && traceEnt->client->deflectorTime > level.time ) {
				if (G_DeflectorEffect( traceEnt, forward, tr.endpos, impactpoint, bouncedir )) {
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

			{
				int pDamage = damage;
				vec3_t kbDir;

				// eser - damage falloff
				pDamage = G_DamageFalloff( pDamage, muzzle, tr.endpos, bg_attacklist[ATT_SHOTGUN_PRIMARY].maxDamageDistance );
				// eser - damage falloff

				// Knockback points along THIS pellet's travel direction (muzzle ->
				// impact), not the shared aim vector. Each pellet of the spread
				// cone impacts at a slightly different point, so the per-pellet
				// directions diverge; their vector sum then depends on the spread
				// (a wide spread partially cancels, a tight point-blank cone stays
				// near-parallel). A fresh local per call: G_Damage normalizes dir
				// in place, which would corrupt a shared vector across pellets.
				VectorSubtract( tr.endpos, muzzle, kbDir );
				VectorNormalize( kbDir );

				// A corpse should not absorb a pellet: damage it (so it can still
				// be gibbed by overkill) but keep tracing so the pellet reaches a
				// live target standing behind it. Without this, the dead body of
				// a freshly-killed player soaks up to GIB_HEALTH worth of pellets
				// from the same blast, shorting the player behind it.
				if ( traceEnt->client && traceEnt->client->ps.pm_type == PM_DEAD ) {
					G_Damage( traceEnt, ent, ent, kbDir, tr.endpos, pDamage, DAMAGE_MOMENTUM_EVENT, mod );
					passent = traceEnt->s.number;
					VectorCopy( tr.endpos, tr_start );
					continue;
				}

				if ( LogAccuracyHit( traceEnt, ent ) ) {
					hitClient = qtrue;
				}

				G_Damage( traceEnt, ent, ent, kbDir, tr.endpos, pDamage, DAMAGE_MOMENTUM_EVENT, mod );
			}

			return hitClient;
		}
		return qfalse;
	}
	return qfalse;
}

// Resolve the death-side-effects that G_Damage / player_die deferred for the
// duration of a shotgun blast (see WIRED_SHOTGUN_POSTPONE_MOD in g_local.h).
// Called once after the whole pellet loop: every pellet has now landed, so it
// is safe to shrink the corpse box, re-arm FL_NO_KNOCKBACK, and perform any gib
// that was scheduled (the body stayed solid through the blast and accrued the
// full knockback). `attacker` is the shooter, used as the gib event's killer.
static void G_ResolveShotgunDeferredDeaths( gentity_t *attacker ) {
	int        i;
	gentity_t *ent;

	for ( i = 0, ent = &g_entities[0]; i < level.num_entities; i++, ent++ ) {
		if ( !ent->inuse ) {
			continue;
		}

		if ( ent->client && ent->client->ps.pm_type == PM_DEAD ) {
			SetDeadHeight( ent );
			SetFlNoKnockback( ent );
		}

		if ( ent->gibScheduled ) {
			GibEntity( ent, attacker->s.number );
			ent->gibScheduled = qfalse;
		}
	}
}

// this should match CG_ShotgunPattern
void ShotgunPattern( vec3_t origin, vec3_t origin2, int seed, gentity_t *ent ) {
	int			i;
	float		r, u;
	vec3_t		end;
	vec3_t		localForward, localRight, localUp;
	qboolean	hitClient = qfalse;
	// RS-3: `seed` is the packed eventParm — bits[7:0] = the rotation/PRNG seed,
	// bits[23:8] = the int-quantized bloom magnitude. Mask to low-8 before using
	// it for rotation/PRNG; derive spreadScale from the same int the client
	// unpacks so both trace identical patterns. // this must match CG_ShotgunPattern
	int			seedLow = seed & 255;
	float		spreadScale = (float)( ( seed >> 8 ) & 0xFFFF ) * 16;

	// derive the right and up vectors from the forward vector, because
	// the client won't have any other information
	VectorNormalize2( origin2, localForward );
	PerpendicularVector( localRight, localForward );
	CrossProduct( localForward, localRight, localUp );

#if FEAT_SHOTGUN_PATTERN
	{
		float rotation = ( seedLow / 256.0f ) * 2.0f * M_PI;

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
		r = Q_crandom( &seedLow ) * spreadScale;
		u = Q_crandom( &seedLow ) * spreadScale;
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

	// every pellet of the blast has landed — apply the deferred death effects.
	G_ResolveShotgunDeferredDeaths( ent );
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
	// RS-3: pack the dynamic bloom magnitude alongside the pattern seed so the
	// CLIENT mirror (CG_ShotgunPattern) — which renders EVERY player's blast and
	// has no access to a remote shooter's ps — uses the SAME magnitude as the
	// server trace. Layout: bits[7:0]=seed (rotation/PRNG, unchanged), bits[23:8]=
	// quantized bloom (~600..900, fits 16 bits). The bloom is int-quantized HERE,
	// and both server (ShotgunPattern) and client unpack this same int → they
	// trace byte-identical patterns (no float/int requantization drift). The
	// seed-driven pellet PATTERN (bg_shotgunPattern) is unchanged; only the scalar
	// magnitude ramps (base 600 → ceiling 900). // this must match CG_ShotgunPattern
	{
		int seed  = rand() & 255;
		int bloom = (int)BG_CalcWeaponSpread( &ent->client->ps, ATT_SHOTGUN_PRIMARY, level.time );
		tent->s.eventParm = seed | ( ( bloom & 0xFFFF ) << 8 );
	}
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

	// every pellet of the blast has landed — apply the deferred death effects.
	G_ResolveShotgunDeferredDeaths( ent );
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
