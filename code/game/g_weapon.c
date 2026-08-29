// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 1999-2005 Id Software, Inc.
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
// g_weapon.c
// perform the server side effects of a weapon firing

#include "g_local.h"

extern void Attack_LightningGun_ChainArc( gentity_t *ent );

float	s_quadFactor;
vec3_t	forward, right, up;
vec3_t	muzzle;

#define THIRD_PERSON_CENTER_AIM_MAX_DELTA 45.0f

/*
========================
G_ResolveWeaponAimAngles

Resolve the separately transported third-person weapon direction. Viewangles
remain authoritative for camera and movement. A bounded angular delta prevents
a modified client from looking one way while firing arbitrarily elsewhere.
========================
*/
qboolean G_ResolveWeaponAimAngles( const gentity_t *ent, vec3_t aimAngles ) {
	const usercmd_t *cmd;
	float pitchDelta;
	float yawDelta;

	if ( !ent || !ent->client ) {
		return qfalse;
	}

	cmd = &ent->client->pers.cmd;
	if ( cmd->aimMode != UCMD_AIM_THIRD_PERSON_CENTER ) {
		return qfalse;
	}

	aimAngles[PITCH] = SHORT2ANGLE( cmd->aimAngles[PITCH] );
	aimAngles[YAW] = SHORT2ANGLE( cmd->aimAngles[YAW] );
	aimAngles[ROLL] = 0.0f;
	pitchDelta = fabsf( AngleDelta( aimAngles[PITCH], ent->client->ps.viewangles[PITCH] ) );
	yawDelta = fabsf( AngleDelta( aimAngles[YAW], ent->client->ps.viewangles[YAW] ) );
	if ( pitchDelta > THIRD_PERSON_CENTER_AIM_MAX_DELTA ||
		 yawDelta > THIRD_PERSON_CENTER_AIM_MAX_DELTA ) {
		return qfalse;
	}

	return qtrue;
}

void G_WeaponAimVectors( const gentity_t *ent, vec3_t aimForward,
	vec3_t aimRight, vec3_t aimUp ) {
	vec3_t aimAngles;

	if ( !G_ResolveWeaponAimAngles( ent, aimAngles ) ) {
		VectorCopy( ent->client->ps.viewangles, aimAngles );
	}
	AngleVectors( aimAngles, aimForward, aimRight, aimUp );
}

void G_ClampWeaponMuzzle( const gentity_t *ent, const vec3_t origin,
	vec3_t muzzlePoint ) {
	vec3_t aimAngles;
	vec3_t eye;
	trace_t tr;

	if ( !G_ResolveWeaponAimAngles( ent, aimAngles ) ) {
		return;
	}

	VectorCopy( origin, eye );
	eye[2] += ent->client->ps.viewheight;
	trap_Trace( &tr, eye, NULL, NULL, muzzlePoint, ent->s.number, MASK_SHOT );
	if ( tr.fraction < 1.0f ) {
		VectorCopy( tr.endpos, muzzlePoint );
		SnapVectorTowards( muzzlePoint, eye );
	}
}

// eser - damage falloff
/*
============
G_DamageFalloff
Reduce damage linearly over distance. Full damage within 256 units,
zero at g_damageFalloff distance. (11H)
============
*/
int G_DamageFalloff( int damage, vec3_t start, vec3_t end, float maxDamageDistance ) {
	float	dist, maxDist, frac;

	if ( !maxDamageDistance ) {
		return damage;
	}

	dist = Distance( start, end );
	if ( dist <= 256.0f ) {
		return damage;
	}

	frac = 1.0f - ( ( dist - 256.0f ) / ( maxDamageDistance - 256.0f ) );
	if ( frac < 0.1f ) {
		frac = 0.1f;
	}

	return (int)( damage * frac );
}
// eser - damage falloff

/*
================
G_BounceProjectile
================
*/
void G_BounceProjectile( vec3_t start, vec3_t impact, vec3_t dir, vec3_t endout ) {
	vec3_t v, newv;
	float dot;

	VectorSubtract( impact, start, v );
	dot = DotProduct( v, dir );
	VectorMA( v, -2*dot, dir, newv );

	VectorNormalize(newv);
	VectorMA(impact, 8192, newv, endout);
}


// Moved to weapons/g_gauntlet.c


/*
======================================================================

MACHINEGUN

======================================================================
*/

/*
======================
SnapVectorTowards

Round a vector to integers for more efficient network
transmission, but make sure that it rounds towards a given point
rather than blindly truncating.  This prevents it from truncating
into a wall.
======================
*/
void SnapVectorTowards( vec3_t v, vec3_t to ) {
	int		i;

	for ( i = 0 ; i < 3 ; i++ ) {
		if ( to[i] <= v[i] ) {
			v[i] = floor(v[i]);
		} else {
			v[i] = ceil(v[i]);
		}
	}
}

// Moved to weapons/g_machinegun.c


// Moved to weapons/g_shotgun.c


// Moved to weapons/g_grenade_launcher.c

// Moved to weapons/g_rocket_launcher.c


// Moved to weapons/g_plasma_rifle.c

// Moved to weapons/g_railgun.c


// Moved to weapons/g_grappling_hook.c (Offhand_Grapple_Hook_Think)

// Moved to weapons/g_lightning_gun.c

// Moved to weapons/g_grappling_hook.c (Offhand_Grapple_Fire, Offhand_Grapple_Free, Offhand_Grapple_Think)

/*
===============
LogAccuracyHit
===============
*/
qboolean LogAccuracyHit( gentity_t *target, gentity_t *attacker ) {
	if( !target->takedamage ) {
		return qfalse;
	}

	if ( target == attacker ) {
		return qfalse;
	}

	if( !target->client ) {
		return qfalse;
	}

	if( !attacker->client ) {
		return qfalse;
	}

	if( target->client->ps.stats[STAT_HEALTH] <= 0 ) {
		return qfalse;
	}

	if ( OnSameTeam( target, attacker ) ) {
		return qfalse;
	}

	return qtrue;
}


/*
===============
CalcMuzzlePoint

set muzzle location relative to pivoting eye
===============
*/
void CalcMuzzlePoint ( gentity_t *ent, vec3_t localForward, vec3_t localRight, vec3_t localUp, vec3_t muzzlePoint ) {
	VectorCopy( ent->s.pos.trBase, muzzlePoint );
	muzzlePoint[2] += ent->client->ps.viewheight;
	VectorMA( muzzlePoint, 14, localForward, muzzlePoint );
	// snap to integer coordinates for more efficient network bandwidth usage
	SnapVector( muzzlePoint );
}

/*
===============
CalcMuzzlePointOrigin

set muzzle location relative to pivoting eye
===============
*/
void CalcMuzzlePointOrigin ( gentity_t *ent, vec3_t origin, vec3_t localForward, vec3_t localRight, vec3_t localUp, vec3_t muzzlePoint ) {
	// The caller supplies the lag-compensated/current firing origin. Using the
	// entity trajectory base here silently discards that authority and can leave
	// projectile spawn points laterally displaced from the rendered eye after
	// prediction, teleport or mover correction. A straight projectile then only
	// converges toward the crosshair with distance, while its historical trail
	// remains visibly detached.
	VectorCopy( origin, muzzlePoint );
	muzzlePoint[2] += ent->client->ps.viewheight;
	VectorMA( muzzlePoint, 14, localForward, muzzlePoint );
	// snap to integer coordinates for more efficient network bandwidth usage
	SnapVector( muzzlePoint );
}



/*
===============
FireWeapon
===============
*/
void FireWeapon( gentity_t *ent, int attackIndex ) {
	if (ent->client->ps.powerups[PW_QUAD] ) {
        s_quadFactor = QUAD_FACTOR;
	} else {
		s_quadFactor = 1;
	}

	if ( attackIndex > 0 ) {
		// Alt-fire dispatch
		G_WeaponAimVectors( ent, forward, right, up );
		CalcMuzzlePointOrigin(ent, ent->client->oldOrigin, forward, right, up, muzzle);
		G_ClampWeaponMuzzle( ent, ent->client->oldOrigin, muzzle );
		G_DoTimeShiftFor(ent);

		switch( ent->s.weapon ) {
		case WP_GAUNTLET:
			Attack_Gauntlet_Lunge( ent );
			break;
		case WP_MACHINEGUN:
			Attack_Machinegun_Burst( ent );
			break;
		case WP_SHOTGUN:
			Attack_Shotgun_DoubleBlast( ent );
			break;
		case WP_ROCKET_LAUNCHER:
			Attack_RocketLauncher_Mortar( ent );
			break;
		case WP_LIGHTNING_GUN:
			Attack_LightningGun_ChainArc( ent );
			break;
		default:
			break;
		}

		G_UndoTimeShiftFor(ent);

		// alt-fire recoil kick
		if ( ent->client && ent->s.weapon != WP_NONE ) {
			vec3_t kickBack;
			float scale = bg_attacklist[bg_weaponlist[ent->s.weapon].attackAlt].recoilKick;
			if ( ent->client->ps.pm_flags & PMF_DUCKED ) {
				scale *= 0.5f;
			}
			if ( scale > 0.0f ) {
				VectorScale( forward, -scale, kickBack );
				VectorAdd( ent->client->ps.velocity, kickBack, ent->client->ps.velocity );
			}
		}
		return;
	}

	// set aiming directions
	G_WeaponAimVectors( ent, forward, right, up );

	CalcMuzzlePointOrigin ( ent, ent->client->oldOrigin, forward, right, up, muzzle );
	G_ClampWeaponMuzzle( ent, ent->client->oldOrigin, muzzle );

	// fire the specific weapon
	// unlagged: rewind other clients for hitscan weapons
	G_DoTimeShiftFor( ent );

	switch( ent->s.weapon ) {
	case WP_GAUNTLET:
		Attack_Gauntlet_Primary( ent );
		break;
	case WP_MACHINEGUN:
		Attack_Machinegun_Primary( ent );
		break;
	case WP_SHOTGUN:
		Attack_Shotgun_Primary( ent );
		break;
	case WP_GRENADE_LAUNCHER:
		Attack_GrenadeLauncher_Primary( ent );
		break;
	case WP_ROCKET_LAUNCHER:
		Attack_RocketLauncher_Primary( ent );
		break;
	case WP_LIGHTNING_GUN:
		Attack_LightningGun_Primary( ent );
		break;
	case WP_RAILGUN:
		Attack_Railgun_Primary( ent );
		break;
	case WP_PLASMA_RIFLE:
		Weapon_PlasmaRifle_Primary( ent );
		break;
	default:
// FIXME		Com_Terminate( TERM_CLIENT_DROP, "Bad ent->s.weapon" );
		break;
	}

	// unlagged: restore all clients to real positions
	G_UndoTimeShiftFor( ent );

	// firing knockback (11G): small self-knockback push when firing (halved when crouched)
	if ( ent->client && ent->s.weapon != WP_NONE ) {
		vec3_t	kickBack;
		float	scale = bg_attacklist[bg_weaponlist[ent->s.weapon].attack].recoilKick; // units of push per shot

		if ( ent->client->ps.pm_flags & PMF_DUCKED ) {
			scale *= 0.5f;
		}

		VectorScale( forward, -scale, kickBack );
		VectorAdd( ent->client->ps.velocity, kickBack, ent->client->ps.velocity );
	}
}


/*
===============
KamikazeRadiusDamage
===============
*/
static void KamikazeRadiusDamage( vec3_t origin, gentity_t *attacker, float damage, float radius ) {
	float		dist;
	gentity_t	*ent;
	int			entityList[MAX_GENTITIES];
	int			numListedEntities;
	vec3_t		mins, maxs;
	vec3_t		v;
	vec3_t		dir;
	int			i, e;

	if ( radius < 1 ) {
		radius = 1;
	}

	for ( i = 0 ; i < 3 ; i++ ) {
		mins[i] = origin[i] - radius;
		maxs[i] = origin[i] + radius;
	}

	numListedEntities = trap_EntitiesInBox( mins, maxs, entityList, MAX_GENTITIES );

	for ( e = 0 ; e < numListedEntities ; e++ ) {
		ent = &g_entities[entityList[ e ]];

		if (!ent->takedamage) {
			continue;
		}

		// don't hit things we have already hit
		if( ent->kamikazeTime > level.time ) {
			continue;
		}

		// find the distance from the edge of the bounding box
		for ( i = 0 ; i < 3 ; i++ ) {
			if ( origin[i] < ent->r.absmin[i] ) {
				v[i] = ent->r.absmin[i] - origin[i];
			} else if ( origin[i] > ent->r.absmax[i] ) {
				v[i] = origin[i] - ent->r.absmax[i];
			} else {
				v[i] = 0;
			}
		}

		dist = VectorLength( v );
		if ( dist >= radius ) {
			continue;
		}

//		if( CanDamage (ent, origin) ) {
			VectorSubtract (ent->r.currentOrigin, origin, dir);
			// push the center of mass higher than the origin so players
			// get knocked into the air more
			dir[2] += 24;
			G_Damage( ent, NULL, attacker, dir, origin, damage, DAMAGE_RADIUS|DAMAGE_NO_TEAM_PROTECTION, MOD_KAMIKAZE );
			ent->kamikazeTime = level.time + 3000;
//		}
	}
}

/*
===============
KamikazeShockWave
===============
*/
static void KamikazeShockWave( vec3_t origin, gentity_t *attacker, float damage, float push, float radius ) {
	float		dist;
	gentity_t	*ent;
	int			entityList[MAX_GENTITIES];
	int			numListedEntities;
	vec3_t		mins, maxs;
	vec3_t		v;
	vec3_t		dir;
	int			i, e;

	if ( radius < 1 )
		radius = 1;

	for ( i = 0 ; i < 3 ; i++ ) {
		mins[i] = origin[i] - radius;
		maxs[i] = origin[i] + radius;
	}

	numListedEntities = trap_EntitiesInBox( mins, maxs, entityList, MAX_GENTITIES );

	for ( e = 0 ; e < numListedEntities ; e++ ) {
		ent = &g_entities[entityList[ e ]];

		// don't hit things we have already hit
		if( ent->kamikazeShockTime > level.time ) {
			continue;
		}

		// find the distance from the edge of the bounding box
		for ( i = 0 ; i < 3 ; i++ ) {
			if ( origin[i] < ent->r.absmin[i] ) {
				v[i] = ent->r.absmin[i] - origin[i];
			} else if ( origin[i] > ent->r.absmax[i] ) {
				v[i] = origin[i] - ent->r.absmax[i];
			} else {
				v[i] = 0;
			}
		}

		dist = VectorLength( v );
		if ( dist >= radius ) {
			continue;
		}

//		if( CanDamage (ent, origin) ) {
			VectorSubtract (ent->r.currentOrigin, origin, dir);
			dir[2] += 24;
			G_Damage( ent, NULL, attacker, dir, origin, damage, DAMAGE_RADIUS|DAMAGE_NO_TEAM_PROTECTION, MOD_KAMIKAZE );
			//
			dir[2] = 0;
			VectorNormalize(dir);
			if ( ent->client ) {
				ent->client->ps.velocity[0] = dir[0] * push;
				ent->client->ps.velocity[1] = dir[1] * push;
				ent->client->ps.velocity[2] = 100;
			}
			ent->kamikazeShockTime = level.time + 3000;
//		}
	}
}

/*
===============
KamikazeDamage
===============
*/
static void KamikazeDamage( gentity_t *self ) {
	float t;
	gentity_t *ent;
	vec3_t newangles;

	self->count += 100;

	if (self->count >= KAMI_SHOCKWAVE_STARTTIME) {
		// shockwave push back
		t = self->count - KAMI_SHOCKWAVE_STARTTIME;
		KamikazeShockWave(self->s.pos.trBase, self->activator, 25, 400,	(int) (float) t * KAMI_SHOCKWAVE_MAXRADIUS / (KAMI_SHOCKWAVE_ENDTIME - KAMI_SHOCKWAVE_STARTTIME) );
	}
	//
	if (self->count >= KAMI_EXPLODE_STARTTIME) {
		// do our damage
		t = self->count - KAMI_EXPLODE_STARTTIME;
		KamikazeRadiusDamage( self->s.pos.trBase, self->activator, 400,	(int) (float) t * KAMI_BOOMSPHERE_MAXRADIUS / (KAMI_IMPLODE_STARTTIME - KAMI_EXPLODE_STARTTIME) );
	}

	// either cycle or kill self
	if( self->count >= KAMI_SHOCKWAVE_ENDTIME ) {
		G_FreeEntity( self );
		return;
	}
	self->nextthink = level.time + 100;

	// add earth quake effect
	newangles[0] = crandom() * 2;
	newangles[1] = crandom() * 2;
	newangles[2] = 0;
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		ent = &g_entities[i];
		if (!ent->inuse)
			continue;
		if (!ent->client)
			continue;

		if (ent->client->ps.groundEntityNum != ENTITYNUM_NONE) {
			ent->client->ps.velocity[0] += crandom() * 120;
			ent->client->ps.velocity[1] += crandom() * 120;
			ent->client->ps.velocity[2] = 30 + random() * 25;
		}

		ent->client->ps.delta_angles[0] += ANGLE2SHORT(newangles[0] - self->movedir[0]);
		ent->client->ps.delta_angles[1] += ANGLE2SHORT(newangles[1] - self->movedir[1]);
		ent->client->ps.delta_angles[2] += ANGLE2SHORT(newangles[2] - self->movedir[2]);
	}
	VectorCopy(newangles, self->movedir);
}

/*
===============
G_StartKamikaze
===============
*/
void G_StartKamikaze( gentity_t *ent ) {
	gentity_t	*explosion;
	gentity_t	*te;
	vec3_t		snapped;

	// start up the explosion logic
	explosion = G_Spawn();

	explosion->s.eType = ET_EVENTS + EV_KAMIKAZE;
	explosion->eventTime = level.time;

	if ( ent->client ) {
		VectorCopy( ent->s.pos.trBase, snapped );
	}
	else {
		VectorCopy( ent->activator->s.pos.trBase, snapped );
	}
	SnapVector( snapped );		// save network bandwidth
	G_SetOrigin( explosion, snapped );

	explosion->classname = "kamikaze";
	explosion->s.pos.trType = TR_STATIONARY;

	explosion->kamikazeTime = level.time;

	explosion->think = KamikazeDamage;
	explosion->nextthink = level.time + 100;
	explosion->count = 0;
	VectorClear(explosion->movedir);

	trap_LinkEntity( explosion );

	if (ent->client) {
		//
		explosion->activator = ent;
		//
		ent->s.eFlags &= ~EF_KAMIKAZE;
		// nuke the guy that used it
		G_Damage( ent, ent, ent, NULL, NULL, 100000, DAMAGE_NO_PROTECTION, MOD_KAMIKAZE );
	}
	else {
		if ( !strcmp(ent->activator->classname, "bodyque") ) {
			explosion->activator = &g_entities[ent->activator->r.ownerNum];
		}
		else {
			explosion->activator = ent->activator;
		}
	}

	// play global sound at all clients
	te = G_TempEntity(snapped, EV_GLOBAL_TEAM_SOUND );
	te->r.svFlags |= SVF_BROADCAST;
	te->s.eventParm = GTS_KAMIKAZE;
}

// ── savegame callback registry — TIER 2 file-local sub-list ──────────────────
// KamikazeDamage is the one file-static callback here (a think handler for the
// kamikaze blast). Published through SG_Register_g_weapon(). Name list
// single-sourced in g_save_localcbs.h (SG_LOCAL_CB_g_weapon). See g_save_funcs.h.
#include "g_save_funcs.h"
#include "g_save_localcbs.h"

SG_DEFINE_LOCAL_REGISTRY( SG_LOCAL_CB_g_weapon, SG_Register_g_weapon )
