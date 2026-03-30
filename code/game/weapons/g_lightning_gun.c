// g_lightning_gun.c -- Lightning Gun weapon implementation
#include "../g_local.h"
#include "../bg_promode.h"

extern float s_quadFactor;
extern vec3_t forward, right, up;
extern vec3_t muzzle;
extern int G_DamageFalloff( int damage, vec3_t start, vec3_t end, float maxDamageDistance );
extern void SnapVectorTowards( vec3_t v, vec3_t to );

void Attack_LightningGun_Primary( gentity_t *ent ) {
	trace_t		tr;
	vec3_t		end;
#if FEAT_PW_INVULNERABILITY
	vec3_t impactpoint, bouncedir;
#endif
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

#if FEAT_PW_INVULNERABILITY
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
#endif
		if ( tr.entityNum == ENTITYNUM_NONE ) {
			return;
		}

		traceEnt = &g_entities[ tr.entityNum ];

		if ( traceEnt->takedamage) {
#if FEAT_PW_INVULNERABILITY
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
#endif
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
