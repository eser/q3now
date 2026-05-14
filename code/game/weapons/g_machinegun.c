// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 1999-2005 Id Software, Inc.
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

// g_machinegun.c -- Machinegun weapon implementation
#include "../g_local.h"

extern float s_quadFactor;
extern vec3_t forward, right, up;
extern vec3_t muzzle;
extern int G_DamageFalloff( int damage, vec3_t start, vec3_t end, float maxDamageDistance );
extern void SnapVectorTowards( vec3_t v, vec3_t to );

#define MACHINEGUN_DAMAGE   8
#define MACHINEGUN_SPREAD	250
void Attack_Machinegun_Primary ( gentity_t *ent ) {
	trace_t		tr;
	vec3_t		end;
	vec3_t		impactpoint, bouncedir;
	float		r;
	float		u;
	gentity_t	*tent;
	gentity_t	*traceEnt;
	int			i, passent;
	int         damage;

	damage = MACHINEGUN_DAMAGE * s_quadFactor;

	if (ent->client) {
		ent->client->accuracy_shots++;
		ent->client->attackStats[ATT_MACHINEGUN_PRIMARY].shots++;
	}

	r = random() * M_PI * 2.0f;
	u = sin(r) * crandom() * MACHINEGUN_SPREAD * 16;
	r = cos(r) * crandom() * MACHINEGUN_SPREAD * 16;
	VectorMA (muzzle, 8192*16, forward, end);
	VectorMA (end, r, right, end);
	VectorMA (end, u, up, end);

	passent = ent->s.number;
	for (i = 0; i < 10; i++) {

		trap_Trace (&tr, muzzle, NULL, NULL, end, passent, MASK_SHOT);
		if ( tr.surfaceFlags & SURF_NOIMPACT ) {
			return;
		}

		traceEnt = &g_entities[ tr.entityNum ];

		// snap the endpos to integers, but nudged towards the line
		SnapVectorTowards( tr.endpos, muzzle );

		// send bullet impact
		if ( traceEnt->takedamage && traceEnt->client ) {
			tent = G_TempEntity( tr.endpos, EV_BULLET_HIT_FLESH );
			tent->s.eventParm = traceEnt->s.number;
			if( LogAccuracyHit( traceEnt, ent ) ) {
				if (ent->client) {
					ent->client->accuracy_hits++;
					ent->client->attackStats[ATT_MACHINEGUN_PRIMARY].hits++;
				}
			}
		} else {
			tent = G_TempEntity( tr.endpos, EV_BULLET_HIT_WALL );
			tent->s.eventParm = DirToByte( tr.plane.normal );
		}
		tent->s.otherEntityNum = ent->s.number;

		if ( traceEnt->takedamage) {
			if ( traceEnt->client && traceEnt->client->deflectorTime > level.time ) {
				if (G_DeflectorEffect( traceEnt, forward, tr.endpos, impactpoint, bouncedir )) {
					G_BounceProjectile( muzzle, impactpoint, bouncedir, end );
					VectorCopy( impactpoint, muzzle );
					// the player can hit him/herself with the bounced rail
					passent = ENTITYNUM_NONE;
				}
				else {
					VectorCopy( tr.endpos, muzzle );
					passent = traceEnt->s.number;
				}
				continue;
			}
			int bDamage = damage;

			// eser - damage falloff
			bDamage =
				G_DamageFalloff( bDamage, muzzle, tr.endpos, bg_attacklist[ATT_MACHINEGUN_PRIMARY].maxDamageDistance );
			// eser - damage falloff

			G_Damage( traceEnt, ent, ent, forward, tr.endpos, bDamage, 0, MOD_MACHINEGUN );
		}
		break;
	}
}

#define MACHINEGUN_BURST_DAMAGE   10
#define MACHINEGUN_BURST_SPREAD   20
void Attack_Machinegun_Burst( gentity_t *ent ) {
	trace_t		tr;
	vec3_t		end;
	vec3_t		impactpoint, bouncedir;
	float		r;
	float		u;
	gentity_t	*tent;
	gentity_t	*traceEnt;
	int			i, passent;
	int         damage;

	damage = MACHINEGUN_BURST_DAMAGE * s_quadFactor;

	if (ent->client) {
		ent->client->accuracy_shots++;
		ent->client->attackStats[ATT_MACHINEGUN_BURST].shots++;
	}

	r = random() * M_PI * 2.0f;
	u = sin(r) * crandom() * MACHINEGUN_BURST_SPREAD * 16;
	r = cos(r) * crandom() * MACHINEGUN_BURST_SPREAD * 16;
	VectorMA (muzzle, 8192*16, forward, end);
	VectorMA (end, r, right, end);
	VectorMA (end, u, up, end);

	passent = ent->s.number;
	for (i = 0; i < 10; i++) {

		trap_Trace (&tr, muzzle, NULL, NULL, end, passent, MASK_SHOT);
		if ( tr.surfaceFlags & SURF_NOIMPACT ) {
			return;
		}

		traceEnt = &g_entities[ tr.entityNum ];

		// snap the endpos to integers, but nudged towards the line
		SnapVectorTowards( tr.endpos, muzzle );

		// send bullet impact
		if ( traceEnt->takedamage && traceEnt->client ) {
			tent = G_TempEntity( tr.endpos, EV_BULLET_HIT_FLESH );
			tent->s.eventParm = traceEnt->s.number;
			if( LogAccuracyHit( traceEnt, ent ) ) {
				if (ent->client) {
					ent->client->accuracy_hits++;
					ent->client->attackStats[ATT_MACHINEGUN_BURST].hits++;
				}
			}
		} else {
			tent = G_TempEntity( tr.endpos, EV_BULLET_HIT_WALL );
			tent->s.eventParm = DirToByte( tr.plane.normal );
		}
		tent->s.otherEntityNum = ent->s.number;

		if ( traceEnt->takedamage) {
			if ( traceEnt->client && traceEnt->client->deflectorTime > level.time ) {
				if (G_DeflectorEffect( traceEnt, forward, tr.endpos, impactpoint, bouncedir )) {
					G_BounceProjectile( muzzle, impactpoint, bouncedir, end );
					VectorCopy( impactpoint, muzzle );
					// the player can hit him/herself with the bounced rail
					passent = ENTITYNUM_NONE;
				}
				else {
					VectorCopy( tr.endpos, muzzle );
					passent = traceEnt->s.number;
				}
				continue;
			}
			int bDamage = damage;

			// eser - damage falloff
			bDamage =
				G_DamageFalloff( bDamage, muzzle, tr.endpos, bg_attacklist[ATT_MACHINEGUN_BURST].maxDamageDistance );
			// eser - damage falloff

			G_Damage( traceEnt, ent, ent, forward, tr.endpos, bDamage, 0, MOD_MACHINEGUN_BURST );
		}
		break;
	}
}
