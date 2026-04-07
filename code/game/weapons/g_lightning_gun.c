// g_lightning_gun.c -- Lightning Gun weapon implementation
#include "../g_local.h"

extern float s_quadFactor;
extern vec3_t forward, right, up;
extern vec3_t muzzle;
extern int G_DamageFalloff( int damage, vec3_t start, vec3_t end, float maxDamageDistance );
extern void SnapVectorTowards( vec3_t v, vec3_t to );

void Attack_LightningGun_Primary( gentity_t *ent ) {
	trace_t		tr;
	vec3_t		end;
	vec3_t impactpoint, bouncedir;
	gentity_t	*traceEnt, *tent;
	int			damage, i, passent;

	damage = 8 * s_quadFactor;

	if (ent->client) {
		ent->client->accuracy_shots++;
		ent->client->attackStats[ATT_LIGHTNING_GUN_PRIMARY].shots++;
	}

	passent = ent->s.number;
	for (i = 0; i < 10; i++) {
		VectorMA( muzzle, LIGHTNING_RANGE, forward, end );

// eser - lightning discharge
	if (trap_PointContents (muzzle, -1) & MASK_WATER)
	{
		int zaps;
		gentity_t *tent;

		zaps = ent->client->ps.ammo[WP_LIGHTNING_GUN];	// determines size/power of discharge
		if (!zaps) return;	// prevents any subsequent frames causing second discharge + error
		zaps++;		// pmove does an ammo[gun]--, so we must compensate
		SnapVectorTowards (muzzle, ent->s.origin);	// save bandwidth

		tent = G_TempEntity (muzzle, EV_LIGHTNING_DISCHARGE);
		tent->s.eventParm = zaps;				// duration / size of explosion graphic

        ent->client->ps.ammo[WP_LIGHTNING_GUN] = 0;		// drain ent's lightning count
		if (G_RadiusDamage (muzzle, ent, damage * zaps, (damage * zaps) + 16, NULL, MOD_LIGHTNING_DISCHARGE, qtrue)) {
			ent->client->accuracy_hits++;
			if (ent->client) ent->client->attackStats[ATT_LIGHTNING_GUN_PRIMARY].hits++;
		}

		return;
	}
// eser - lightning discharge


		{
// eser - lightning push
			// QW-style: beam has physical width for more forgiving hits
			static vec3_t lgMins = { -4, -4, -4 };
			static vec3_t lgMaxs = {  4,  4,  4 };
			trap_Trace( &tr, muzzle, lgMins, lgMaxs, end, passent, MASK_SHOT );
		}
// eser - lightning push

		// if not the first trace (the lightning bounced of an invulnerability sphere)
		if (i) {
			// add bounced off lightning bolt temp entity
			// the first lightning bolt is a cgame only visual
			//
			tent = G_TempEntity( muzzle, EV_LIGHTNINGBOLT );
			VectorCopy( tr.endpos, end );
			SnapVector( end );
			VectorCopy( end, tent->s.origin2 );
		}

		if ( tr.entityNum == ENTITYNUM_NONE ) {
			return;
		}

		traceEnt = &g_entities[ tr.entityNum ];

		if ( traceEnt->takedamage) {
			if ( traceEnt->client && traceEnt->client->invulnerabilityTime > level.time ) {
				if (G_InvulnerabilityEffect( traceEnt, forward, tr.endpos, impactpoint, bouncedir )) {
					G_BounceProjectile( muzzle, impactpoint, bouncedir, end );
					VectorCopy( impactpoint, muzzle );
					VectorSubtract( end, impactpoint, forward );
					VectorNormalize(forward);
					// the player can hit him/herself with the bounced lightning
					passent = ENTITYNUM_NONE;
				}
				else {
					VectorCopy( tr.endpos, muzzle );
					passent = traceEnt->s.number;
				}
				continue;
			}

			if( LogAccuracyHit( traceEnt, ent ) ) {
				ent->client->accuracy_hits++;
				if (ent->client) ent->client->attackStats[ATT_LIGHTNING_GUN_PRIMARY].hits++;
			}
			{
				int lgDamage = damage;

				// eser - damage falloff
				lgDamage = G_DamageFalloff( lgDamage, muzzle, tr.endpos, bg_attacklist[ATT_LIGHTNING_GUN_PRIMARY].maxDamageDistance );
				// eser - damage falloff

// eser - lightning beams
				{
			       // QW-style: LG pushes targets upward for juggle/combo potential
			       vec3_t lgDir;

				   VectorCopy( forward, lgDir );
			       lgDir[2] += 0.5f;
			       VectorNormalize( lgDir );
			       G_Damage( traceEnt, ent, ent, lgDir, tr.endpos, lgDamage, 0, MOD_LIGHTNING );
			    }
// eser - lightning beams
			}
		}

		if ( traceEnt->takedamage && traceEnt->client ) {
			tent = G_TempEntity( tr.endpos, EV_MISSILE_HIT );
			tent->s.otherEntityNum = traceEnt->s.number;
			tent->s.eventParm = DirToByte( tr.plane.normal );
			tent->s.weapon = ent->s.weapon;
		} else if ( !( tr.surfaceFlags & SURF_NOIMPACT ) ) {
			tent = G_TempEntity( tr.endpos, EV_MISSILE_MISS );
			tent->s.eventParm = DirToByte( tr.plane.normal );
		}

		break;
	}
}

/*
=================
Attack_LightningGun_ChainArc
Alt-fire: fires primary LG beam + arcs to nearby secondary target.
Arc searches 192 units around the primary hit target.
Arc damage: 3 per tick (~37.5% of primary 8).
=================
*/
void Attack_LightningGun_ChainArc( gentity_t *ent ) {
	trace_t		tr;
	vec3_t		end;
	gentity_t	*traceEnt, *tent;
	int			damage, i, passent;
	vec3_t		impactPoint;
	gentity_t	*primaryTarget = NULL;

	damage = 8 * s_quadFactor;

	// --- Primary beam (same as Attack_LightningGun_Primary) ---
	VectorMA( muzzle, LIGHTNING_RANGE, forward, end );

	passent = ent->s.number;
	for (i = 0; i < 10; i++) {
		vec3_t lgMins = {-4, -4, -4};
		vec3_t lgMaxs = {4, 4, 4};
		trap_Trace( &tr, muzzle, lgMins, lgMaxs, end, passent, MASK_SHOT );

		if ( tr.entityNum == ENTITYNUM_NONE ) {
			break;
		}

		traceEnt = &g_entities[tr.entityNum];

		if ( traceEnt->takedamage ) {
			int finalDamage = damage;

			if ( bg_attacklist[ATT_LIGHTNING_GUN_CHAIN_ARC].maxDamageDistance > 0 ) {
				finalDamage = G_DamageFalloff( damage, muzzle, tr.endpos,
					bg_attacklist[ATT_LIGHTNING_GUN_CHAIN_ARC].maxDamageDistance );
			}

			// Apply primary beam damage with upward bias for juggle
			{
				vec3_t lgDir;
				VectorSubtract( tr.endpos, muzzle, lgDir );
				VectorNormalize( lgDir );
				lgDir[2] += 0.5f;

				G_Damage( traceEnt, ent, ent, lgDir, tr.endpos, finalDamage,
					0, MOD_LIGHTNING );
			}

			// Track primary target for arc search
			if ( traceEnt->client && !primaryTarget ) {
				primaryTarget = traceEnt;
				VectorCopy( tr.endpos, impactPoint );
			}

			// Stats tracking
			if ( traceEnt->client ) {
				ent->client->attackStats[ATT_LIGHTNING_GUN_CHAIN_ARC].hits++;
			}
		}

		if ( traceEnt->takedamage && traceEnt->client ) {
			break;  // Stop at first client hit for primary beam
		}

		// Continue trace through non-client entities
		passent = traceEnt->s.number;
	}

	// Always count as a shot for stats
	ent->client->attackStats[ATT_LIGHTNING_GUN_CHAIN_ARC].shots++;

	// --- Arc to secondary target ---
	if ( primaryTarget && primaryTarget->client ) {
		gentity_t	*arcTarget = NULL;
		float		bestDist = LG_CHAIN_ARC_RANGE + 1;
		int			j;
		trace_t		arcTrace;

		// Search for nearest enemy within arc range of primary target
		for ( j = 0; j < level.maxclients; j++ ) {
			gentity_t *candidate = &g_entities[j];

			// Skip invalid candidates
			if ( !candidate->inuse || !candidate->client ) continue;
			if ( candidate == ent ) continue;           // not the shooter
			if ( candidate == primaryTarget ) continue;  // not the primary target
			if ( candidate->health <= 0 ) continue;      // must be alive
			if ( candidate->client->sess.sessionTeam == TEAM_SPECTATOR ) continue;

			// Team check: don't arc to teammates
			if ( OnSameTeam( candidate, ent ) ) continue;

			// Distance check from primary target
			{
				vec3_t diff;
				float dist;
				VectorSubtract( candidate->r.currentOrigin, primaryTarget->r.currentOrigin, diff );
				dist = VectorLength( diff );

				if ( dist > LG_CHAIN_ARC_RANGE ) continue;
				if ( dist >= bestDist ) continue;

				// LOS check from primary target to candidate
				trap_Trace( &arcTrace, impactPoint, NULL, NULL,
					candidate->r.currentOrigin, primaryTarget->s.number, MASK_SHOT );

				if ( arcTrace.entityNum != candidate->s.number ) continue;

				// Valid arc target
				arcTarget = candidate;
				bestDist = dist;
			}
		}

		// Apply arc damage to secondary target
		if ( arcTarget ) {
			int arcDamage = LG_CHAIN_ARC_DAMAGE * s_quadFactor;
			vec3_t arcDir;

			VectorSubtract( arcTarget->r.currentOrigin, impactPoint, arcDir );
			VectorNormalize( arcDir );

			G_Damage( arcTarget, ent, ent, arcDir, arcTarget->r.currentOrigin,
				arcDamage, 0, MOD_LIGHTNING_CHAIN_ARC );

			// Send arc event for client-side rendering
			tent = G_TempEntity( impactPoint, EV_LIGHTNING_ARC );
			VectorCopy( impactPoint, tent->s.origin );
			VectorCopy( arcTarget->r.currentOrigin, tent->s.origin2 );
			tent->s.otherEntityNum = arcTarget->s.number;
		}
	}
}
