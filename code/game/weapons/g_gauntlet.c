// g_gauntlet.c -- Gauntlet weapon implementation
#include "../g_local.h"

extern float s_quadFactor;
extern vec3_t forward, right, up;
extern vec3_t muzzle;

void Attack_Gauntlet_Primary( gentity_t *ent ) {

}

/*
===============
CheckGauntletAttack
===============
*/
qboolean CheckGauntletAttack( gentity_t *ent ) {
	trace_t		tr;
	vec3_t		end;
	gentity_t	*tent;
	gentity_t	*traceEnt;
	int			damage;

	if (ent->client) {
		ent->client->accuracy_shots++;
		ent->client->attackStats[ATT_GAUNTLET_PRIMARY].shots++;
	}

	// set aiming directions
	AngleVectors (ent->client->ps.viewangles, forward, right, up);

	CalcMuzzlePoint ( ent, forward, right, up, muzzle );

	VectorMA (muzzle, 32, forward, end);

	trap_Trace (&tr, muzzle, NULL, NULL, end, ent->s.number, MASK_SHOT);
	if ( tr.surfaceFlags & SURF_NOIMPACT ) {
		return qfalse;
	}

	if ( ent->client->noclip ) {
		return qfalse;
	}

	traceEnt = &g_entities[ tr.entityNum ];

	// send blood impact
	if ( traceEnt->takedamage && traceEnt->client ) {
		tent = G_TempEntity( tr.endpos, EV_MISSILE_HIT );
		tent->s.otherEntityNum = traceEnt->s.number;
		tent->s.eventParm = DirToByte( tr.plane.normal );
		tent->s.weapon = ent->s.weapon;
	}

	if ( !traceEnt->takedamage) {
		return qfalse;
	}

	if (ent->client->ps.powerups[PW_QUAD] ) {
		G_AddEvent( ent, EV_POWERUP_QUAD, 0 );
		s_quadFactor = QUAD_FACTOR;
	} else {
		s_quadFactor = 1;
	}

	damage = 80 * s_quadFactor;
	G_Damage( traceEnt, ent, ent, forward, tr.endpos,
		damage, 0, MOD_GAUNTLET );

	if ( ent->client ) {
		ent->client->accuracy_hits++;
		ent->client->attackStats[ATT_GAUNTLET_PRIMARY].hits++;
	}

	return qtrue;
}

/*
===============
Attack_Gauntlet_Lunge

Extended-range melee attack triggered by lunge alt-fire.
80-unit trace range, 100 damage.
===============
*/
void Attack_Gauntlet_Lunge( gentity_t *ent ) {
	trace_t		tr;
	vec3_t		end;
	gentity_t	*tent;
	gentity_t	*traceEnt;
	int			damage;

	if (ent->client) {
		ent->client->accuracy_shots++;
		ent->client->attackStats[ATT_GAUNTLET_LUNGE].shots++;
	}

	// set aiming directions
	AngleVectors( ent->client->ps.viewangles, forward, right, up );
	CalcMuzzlePoint( ent, forward, right, up, muzzle );

	// extended trace range: 80 units (vs 32 for primary gauntlet)
	VectorMA( muzzle, 80, forward, end );

	trap_Trace( &tr, muzzle, NULL, NULL, end, ent->s.number, MASK_SHOT );
	if ( tr.surfaceFlags & SURF_NOIMPACT ) {
		return;
	}

	if ( ent->client->noclip ) {
		return;
	}

	traceEnt = &g_entities[ tr.entityNum ];

	// send impact event
	if ( traceEnt->takedamage && traceEnt->client ) {
		tent = G_TempEntity( tr.endpos, EV_MISSILE_HIT );
		tent->s.otherEntityNum = traceEnt->s.number;
		tent->s.eventParm = DirToByte( tr.plane.normal );
		tent->s.weapon = ent->s.weapon;
	}

	if ( !traceEnt->takedamage ) {
		return;
	}

	if ( ent->client->ps.powerups[PW_QUAD] ) {
		G_AddEvent( ent, EV_POWERUP_QUAD, 0 );
		s_quadFactor = QUAD_FACTOR;
	} else {
		s_quadFactor = 1;
	}

	damage = 100 * s_quadFactor;
	G_Damage( traceEnt, ent, ent, forward, tr.endpos, damage, 0, MOD_GAUNTLET_LUNGE );

	if ( ent->client ) {
		ent->client->accuracy_hits++;
		ent->client->attackStats[ATT_GAUNTLET_LUNGE].hits++;
	}
}
