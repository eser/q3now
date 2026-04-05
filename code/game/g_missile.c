/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
//
#include "g_local.h"
#include "bg_promode.h" // CPM

#define	MISSILE_PRESTEP_TIME	50
#if FEAT_TELEPORTING_MISSILES
#define MISSILE_MAX_TELEPORTS   3
#endif

#if FEAT_DESTROYABLE_MISSILES
/*
============
G_MakeMissileDestroyable
Mark a missile entity as damageable so it can be shot down. (11B)
============
*/
static void G_MissileExplodeDie( gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, int mod ) {
	(void)inflictor; (void)attacker; (void)damage; (void)mod;
	G_ExplodeMissile( self );
}

static void G_MakeMissileDestroyable( gentity_t *bolt ) {
	bolt->takedamage = qtrue;
	bolt->health = 100;  // cvar value = missile HP
	bolt->die = G_MissileExplodeDie;
	bolt->r.contents = CONTENTS_BODY;               // hitscan can hit it
	VectorSet( bolt->r.mins, -4, -4, -4 );
	VectorSet( bolt->r.maxs,  4,  4,  4 );
}
#endif

/*
================
G_BounceMissile

================
*/
void G_BounceMissile( gentity_t *ent, trace_t *trace ) {
	vec3_t	velocity;
	float	dot;
	int		hitTime;

	// reflect the velocity on the trace plane
	hitTime = level.previousTime + ( level.time - level.previousTime ) * trace->fraction;
	BG_EvaluateTrajectoryDelta( &ent->s.pos, hitTime, velocity );
	dot = DotProduct( velocity, trace->plane.normal );

	if ( ent->s.eFlags & EF_GRENADE_BOUNCE ) {
		// Q1/Q2-style: overbounce 1.5 (halve normal component, preserve tangential)
		VectorMA( velocity, -1.5f * dot, trace->plane.normal, ent->s.pos.trDelta );
		// Q1/Q2 stop: freeze on near-flat floors when vertical velocity decays
		if ( trace->plane.normal[2] > 0.7f &&
			 ent->s.pos.trDelta[2] > -60 && ent->s.pos.trDelta[2] < 60 ) {
			G_SetOrigin( ent, trace->endpos );
#if FEAT_GRENADE_REST_FIX
			// freeze at current visual angle instead of snapping to axis
			BG_EvaluateTrajectory( &ent->s.apos, level.time, ent->s.angles );
			VectorCopy( ent->s.angles, ent->s.apos.trBase );
			VectorClear( ent->s.apos.trDelta );
			ent->s.apos.trType = TR_STATIONARY;
#endif
			ent->s.time = level.time / 4;
			return;
		}
	} else {
		VectorMA( velocity, -2*dot, trace->plane.normal, ent->s.pos.trDelta );

		if ( ent->s.eFlags & EF_BOUNCE_HALF ) {
			VectorScale( ent->s.pos.trDelta, 0.65, ent->s.pos.trDelta );
			// check for stop
			if ( trace->plane.normal[2] > 0.2 && VectorLength( ent->s.pos.trDelta ) < 40 ) {
				G_SetOrigin( ent, trace->endpos );
#if FEAT_GRENADE_REST_FIX
				// freeze at current visual angle instead of snapping to axis
				BG_EvaluateTrajectory( &ent->s.apos, level.time, ent->s.angles );
				VectorCopy( ent->s.angles, ent->s.apos.trBase );
				VectorClear( ent->s.apos.trDelta );
				ent->s.apos.trType = TR_STATIONARY;
#endif
				ent->s.time = level.time / 4;
				return;
			}
		}
	}

	VectorAdd( ent->r.currentOrigin, trace->plane.normal, ent->r.currentOrigin);
	VectorCopy( ent->r.currentOrigin, ent->s.pos.trBase );
	ent->s.pos.trTime = level.time;
}


/*
================
G_ExplodeMissile

Explode a missile without an impact
================
*/
void G_ExplodeMissile( gentity_t *ent ) {
	vec3_t		dir;
	vec3_t		origin;

	BG_EvaluateTrajectory( &ent->s.pos, level.time, origin );
	SnapVector( origin );
	G_SetOrigin( ent, origin );

	// we don't have a valid direction, so just point straight up
	dir[0] = dir[1] = 0;
	dir[2] = 1;

	ent->s.eType = ET_GENERAL;
	G_AddEvent( ent, EV_MISSILE_MISS, DirToByte( dir ) );

	ent->freeAfterEvent = qtrue;

	// splash damage
	if ( ent->splashDamage &&
		G_RadiusDamage( ent->r.currentOrigin, ent->parent, ent->splashDamage, ent->splashRadius, ent, ent->splashMethodOfDeath, qfalse ) ) {
		int att = G_AttackFromMOD(ent->splashMethodOfDeath);

		if (g_entities[ent->r.ownerNum].client) {
			g_entities[ent->r.ownerNum].client->accuracy_hits++;

			if ( att > ATT_NONE && att < ATT_NUM_ATTACKS ) {
				g_entities[ent->r.ownerNum].client->attackStats[att].hits++;
			}
		}
	}

	trap_LinkEntity( ent );
}


#if FEAT_PROJECTILE_BOUNCE
/*
================
G_ReflectProjectile

Reflects a projectile's velocity off a surface normal.
Used when a missile hits an invulnerability shield (10H).
================
*/
static void G_ReflectProjectile( gentity_t *ent, trace_t *trace ) {
	vec3_t	velocity;
	float	dot;
	int		hitTime;

	// evaluate velocity at impact time
	hitTime = level.previousTime + ( level.time - level.previousTime ) * trace->fraction;
	BG_EvaluateTrajectoryDelta( &ent->s.pos, hitTime, velocity );

	// reflect velocity across surface normal: v' = v - 2(v . n)n
	dot = DotProduct( velocity, trace->plane.normal );
	VectorMA( velocity, -2 * dot, trace->plane.normal, ent->s.pos.trDelta );

	// nudge origin off the surface and reset trajectory
	VectorAdd( ent->r.currentOrigin, trace->plane.normal, ent->r.currentOrigin );
	VectorCopy( ent->r.currentOrigin, ent->s.pos.trBase );
	ent->s.pos.trTime = level.time;

	// snap for network bandwidth
	SnapVector( ent->s.pos.trDelta );
}
#endif

/*
================
G_MissileImpact
================
*/
void G_MissileImpact( gentity_t *ent, trace_t *trace, vec3_t impactDir ) {
	gentity_t		*other;
	qboolean		hitClient = qfalse;
#if FEAT_PW_INVULNERABILITY
	vec3_t			forward, impactpoint, bouncedir;
	int				eFlags;
#endif
    vec3_t	vec2; // CPM

	other = &g_entities[trace->entityNum];

	// skip collision between helix pair rockets
	if ( ent->helixPairEntity && other == ent->helixPairEntity ) {
		return;
	}

    // CPM: copy velocity for special radius damage
    BG_EvaluateTrajectoryDelta(&ent->s.pos, level.time, vec2);
    // !CPM

	// check for bounce
	if ( !other->takedamage &&
		( ent->s.eFlags & ( EF_BOUNCE | EF_BOUNCE_HALF | EF_GRENADE_BOUNCE ) ) ) {
		G_BounceMissile( ent, trace );
#if FEAT_BOUNCE_SOUND_LIMIT
		if ( level.time - ent->pain_debounce_time > 100 ) {  // max 10 bounces/sec
			G_AddEvent( ent, EV_GRENADE_BOUNCE, 0 );
			ent->pain_debounce_time = level.time;
		}
#else
		G_AddEvent( ent, EV_GRENADE_BOUNCE, 0 );
#endif
		return;
	}

#if FEAT_PROJECTILE_BOUNCE
	// projectile bounce (10H): reflect missile off invulnerability shield
	if ( g_projectileBounce.integer && other->takedamage &&
		 other->client && other->client->ps.powerups[PW_BATTLESUIT] &&
		 other->client->ps.powerups[PW_BATTLESUIT] > level.time ) {
		G_ReflectProjectile( ent, trace );
		ent->target_ent = other;
		return;
	}
#endif
#if FEAT_PW_INVULNERABILITY
	if ( other->takedamage ) {
		if ( other->client && other->client->invulnerabilityTime > level.time ) {
			//
			VectorCopy( ent->s.pos.trDelta, forward );
			VectorNormalize( forward );
			if (G_InvulnerabilityEffect( other, forward, ent->s.pos.trBase, impactpoint, bouncedir )) {
				VectorCopy( bouncedir, trace->plane.normal );
				eFlags = ent->s.eFlags & EF_BOUNCE_HALF;
				ent->s.eFlags &= ~EF_BOUNCE_HALF;
				G_BounceMissile( ent, trace );
				ent->s.eFlags |= eFlags;
			}
			ent->target_ent = other;
			return;
		}
	}
#endif
	// impact damage
	if (other->takedamage) {
		// FIXME: wrong damage direction?
		if ( ent->damage ) {
			vec3_t	velocity;

			if( LogAccuracyHit( other, &g_entities[ent->r.ownerNum] ) ) {
				int att;

				att = G_AttackFromMOD( ent->methodOfDeath );

				if (g_entities[ent->r.ownerNum].client) {
					g_entities[ent->r.ownerNum].client->accuracy_hits++;
					g_entities[ent->r.ownerNum].client->attackStats[att].hits++;
				}

				hitClient = qtrue;
			}

			BG_EvaluateTrajectoryDelta( &ent->s.pos, level.time, velocity );

			if ( VectorLength( velocity ) == 0 ) {
				velocity[2] = 1;	// stepped on a grenade
			}

			G_Damage (other, ent, &g_entities[ent->r.ownerNum], velocity,
				ent->s.origin, ent->damage, 
				0, ent->methodOfDeath);
		}
	}

	if (!strcmp(ent->classname, "hook")) {
		gentity_t *nent;
		vec3_t v;

		nent = G_Spawn();
		if ( other->takedamage && other->client ) {

			G_AddEvent( nent, EV_MISSILE_HIT, DirToByte( trace->plane.normal ) );
			nent->s.otherEntityNum = other->s.number;

			ent->enemy = other;

			v[0] = other->r.currentOrigin[0] + (other->r.mins[0] + other->r.maxs[0]) * 0.5;
			v[1] = other->r.currentOrigin[1] + (other->r.mins[1] + other->r.maxs[1]) * 0.5;
			v[2] = other->r.currentOrigin[2] + (other->r.mins[2] + other->r.maxs[2]) * 0.5;

			SnapVectorTowards( v, ent->s.pos.trBase );	// save net bandwidth
		} else {
			VectorCopy(trace->endpos, v);
			G_AddEvent( nent, EV_MISSILE_MISS, DirToByte( trace->plane.normal ) );
			ent->enemy = NULL;
		}

		SnapVectorTowards( v, ent->s.pos.trBase );	// save net bandwidth

		nent->freeAfterEvent = qtrue;
		// change over to a normal entity right at the point of impact
		nent->s.eType = ET_GENERAL;
		ent->s.eType = ET_GRAPPLE;

		G_SetOrigin( ent, v );
		G_SetOrigin( nent, v );

		ent->think = Offhand_Grapple_Hook_Think;
		ent->nextthink = level.time + FRAMETIME;

		// use this to track when to damage the enemy
		ent->last_move_time = level.time - 10000;

		ent->parent->client->ps.pm_flags |= PMF_GRAPPLE_PULL;
		VectorCopy( ent->r.currentOrigin, ent->parent->client->ps.grapplePoint);

		trap_LinkEntity( ent );
		trap_LinkEntity( nent );

		return;
	}

	// is it cheaper in bandwidth to just remove this ent and create a new
	// one, rather than changing the missile into the explosion?

	if ( other->takedamage && other->client ) {
		G_AddEvent( ent, EV_MISSILE_HIT, DirToByte( trace->plane.normal ) );
		ent->s.otherEntityNum = other->s.number;
	} else if( trace->surfaceFlags & SURF_METALSTEPS ) {
		G_AddEvent( ent, EV_MISSILE_MISS_METAL, DirToByte( trace->plane.normal ) );
	} else {
		G_AddEvent( ent, EV_MISSILE_MISS, DirToByte( trace->plane.normal ) );
	}

	ent->freeAfterEvent = qtrue;

	// change over to a normal entity right at the point of impact
	ent->s.eType = ET_GENERAL;

	SnapVectorTowards( trace->endpos, ent->s.pos.trBase );	// save net bandwidth

	G_SetOrigin( ent, trace->endpos );

	// splash damage (doesn't apply to person directly hit)
	if ( ent->splashDamage ) {
		// if( G_RadiusDamage( trace->endpos, ent->parent, ent->splashDamage, ent->splashRadius, 
		//	other, ent->splashMethodOfDeath, qfalse ) ) {
		//	if( !hitClient ) {
		//		g_entities[ent->r.ownerNum].client->accuracy_hits++;
		//	}
		// }

        // CPM: check new radius damage rules
        if (cpm_radiusdamagefix)
        {
            // find "viewpoint" for explosion
            // backtrace 10 units
            VectorNormalize(vec2);
            VectorScale(vec2, 10, vec2);
            VectorSubtract(trace->endpos, vec2, vec2);
            // use new radius damage
            if (CPM_RadiusDamage(trace->endpos, ent->parent, ent->splashDamage, ent->splashRadius, other, ent->splashMethodOfDeath, vec2)) {
				int att;

				att = G_AttackFromMOD( ent->splashMethodOfDeath );

                if (!hitClient) {
                    g_entities[ent->r.ownerNum].client->accuracy_hits++;
                    if (g_entities[ent->r.ownerNum].client) g_entities[ent->r.ownerNum].client->attackStats[att].hits++;
                }
            }
        }
        else if (G_RadiusDamage(trace->endpos, ent->parent, ent->splashDamage, ent->splashRadius, other, ent->splashMethodOfDeath, qfalse)) {
            if (!hitClient) {
				int att;

				att = G_AttackFromMOD( ent->splashMethodOfDeath );

				g_entities[ent->r.ownerNum].client->accuracy_hits++;
                if (g_entities[ent->r.ownerNum].client) g_entities[ent->r.ownerNum].client->attackStats[att].hits++;
            }
        }
        // !CPM
	}

	trap_LinkEntity( ent );
}

#if FEAT_TELEPORTING_MISSILES
/*
================
G_TeleportMissile
Teleports a missile through a teleporter trigger to its destination.
Rotates velocity to match the destination portal's orientation.
================
*/
void G_TeleportMissile( gentity_t *ent, trace_t *trace, gentity_t *portal ) {
	gentity_t *dest;
	vec3_t    velocity;
	vec3_t    portalInVec, portalInAngles, rotationAngles;
	vec3_t    rotationMatrix[3];
	vec3_t    tmp;
	vec_t     len_norm, len_neg_norm;
	int       hitTime;

	dest = G_PickTarget( portal->target );
	if ( !dest ) {
		G_Printf( "G_TeleportMissile: couldn't find destination\n" );
		return;
	}

	// evaluate velocity at impact time
	hitTime = level.previousTime + (int)( ( level.time - level.previousTime ) * trace->fraction );
	BG_EvaluateTrajectoryDelta( &ent->s.pos, hitTime, velocity );

	// pick the portal normal direction that opposes velocity
	VectorAdd( trace->plane.normal, velocity, tmp );
	len_norm = VectorLengthSquared( tmp );
	VectorNegate( trace->plane.normal, portalInVec );
	VectorAdd( portalInVec, velocity, tmp );
	len_neg_norm = VectorLengthSquared( tmp );

	vectoangles( portalInVec, portalInAngles );
	if ( len_norm > len_neg_norm ) {
		VectorSubtract( dest->s.angles, portalInAngles, rotationAngles );
	} else {
		VectorSubtract( portalInAngles, dest->s.angles, rotationAngles );
	}

	// rotate velocity to destination orientation
	AngleVectors( rotationAngles, rotationMatrix[0], rotationMatrix[1], rotationMatrix[2] );
	VectorInverse( rotationMatrix[1] );
	VectorRotate( velocity, (const vec3_t *)rotationMatrix, ent->s.pos.trDelta );
	SnapVector( ent->s.pos.trDelta );

	// flip teleport bit so clients play teleport effect
	ent->s.eFlags ^= EF_TELEPORT_BIT;

	// move to destination
	VectorCopy( dest->s.origin, ent->r.currentOrigin );
	VectorCopy( ent->r.currentOrigin, ent->s.pos.trBase );
	ent->s.pos.trTime = level.time;
}
#endif

/*
================
G_RunMissile
================
*/
void G_RunMissile( gentity_t *ent ) {
	vec3_t		origin;
	trace_t		tr;
	int			passent;
	vec3_t dir;

	// get current position
	BG_EvaluateTrajectory( &ent->s.pos, level.time, origin );

	// if this missile bounced off an invulnerability sphere
	if ( ent->target_ent ) {
		passent = ent->target_ent->s.number;
	}
	else {
		// ignore interactions with the missile owner
		passent = ent->r.ownerNum;
	}
	// trace a line from the previous position to the current position
	trap_Trace( &tr, ent->r.currentOrigin, ent->r.mins, ent->r.maxs, origin, passent, ent->clipmask );

#if FEAT_TELEPORTING_MISSILES
	// missile teleportation (2F): check for teleporter triggers along path
	if ( MISSILE_MAX_TELEPORTS > 0 && ent->classname && ( !strcmp( ent->classname, "rocket" ) || !strcmp( ent->classname, "plasma" ) ) ) {
		trace_t trTrig;
		trap_Trace( &trTrig, ent->r.currentOrigin, ent->r.mins, ent->r.maxs, origin, passent, ent->clipmask | CONTENTS_TRIGGER );
		if ( trTrig.fraction < tr.fraction && trTrig.entityNum != ENTITYNUM_NONE ) {
			gentity_t *trigEnt = &g_entities[trTrig.entityNum];
			if ( trigEnt->s.eType == ET_TELEPORT_TRIGGER && trigEnt->target ) {
				if ( ent->missileTeleportCount < MISSILE_MAX_TELEPORTS ) {
					G_TeleportMissile( ent, &trTrig, trigEnt );
					ent->missileTeleportCount++;
					// update origin after teleport
					BG_EvaluateTrajectory( &ent->s.pos, level.time, origin );
					trap_Trace( &tr, ent->r.currentOrigin, ent->r.mins, ent->r.maxs, origin, passent, ent->clipmask );
				}
			}
		}
	}
#endif

	// get the direction
	VectorSubtract(origin, ent->r.currentOrigin, dir);
	VectorNormalize(dir);

	if ( tr.startsolid || tr.allsolid ) {
		// make sure the tr.entityNum is set to the entity we're stuck in
		trap_Trace( &tr, ent->r.currentOrigin, ent->r.mins, ent->r.maxs, ent->r.currentOrigin, passent, ent->clipmask );
		tr.fraction = 0;
	}
	else {
		VectorCopy( tr.endpos, ent->r.currentOrigin );
	}

	trap_LinkEntity( ent );

	if ( tr.fraction != 1 ) {
		// never explode or bounce on sky
		if ( tr.surfaceFlags & SURF_NOIMPACT ) {
			// If grapple, reset owner
            if (ent->parent && ent->parent->client && ent->parent->client->hook == ent)
            {
                ent->parent->client->hook = NULL;
                if (g_grapple.integer) {
                    ent->parent->client->hookhasbeenfired = qfalse;
                    ent->parent->client->fireHeld = qfalse;
                }
            }

			G_FreeEntity( ent );
			return;
		}
		G_MissileImpact( ent, &tr, dir );
		if ( ent->s.eType != ET_MISSILE ) {
			return;		// exploded
		}
	}
	// check think function after bouncing
	G_RunThink( ent );
}


//=============================================================================

/*
=================
fire_plasma

=================
*/
#define PLASMA_SPREAD 500

gentity_t *fire_plasma(gentity_t *self, vec3_t start, vec3_t forward, vec3_t right, vec3_t up) {
	gentity_t	*bolt;
    vec3_t		dir;
    vec3_t		end;
    float		r, u, scale;
	float       baseSpeed, additionalSpeed;

	if (g_excessive.integer) {
		baseSpeed = 1000;
		additionalSpeed = 2200;
	}
	else {
		baseSpeed = 555;
		additionalSpeed = 1800;
	}

	VectorNormalize (dir);

	bolt = G_Spawn();
	bolt->classname = "plasma";
	bolt->nextthink = level.time + 10000;
	bolt->think = G_ExplodeMissile;
	bolt->s.eType = ET_MISSILE;
	bolt->r.svFlags = SVF_USE_CURRENT_ORIGIN;
	bolt->s.weapon = WP_PLASMA_RIFLE;
	bolt->r.ownerNum = self->s.number;
	bolt->parent = self;
#if FEAT_UNLAGGED
	bolt->s.otherEntityNum = self->s.number;
#endif
    bolt->damage = 20;
	bolt->methodOfDeath = MOD_PLASMA;
	bolt->clipmask = MASK_SHOT;
	bolt->target_ent = NULL;

    bolt->s.pos.trType = TR_ACCELERATE;
    bolt->s.pos.trDuration = 500;

	bolt->s.pos.trType = TR_LINEAR;
	// bolt->s.pos.trTime = level.time - MISSILE_PRESTEP_TIME;		// move a bit on the very first frame
    bolt->s.pos.trTime = level.time;
	VectorCopy( start, bolt->s.pos.trBase );

    r = random() * M_PI * 2.0f;
    u = sin(r) * crandom() * PLASMA_SPREAD * 16;
    r = cos(r) * crandom() * PLASMA_SPREAD * 16;
    VectorMA(start, 8192 * 16, forward, end);
    VectorMA(end, r, right, end);
    VectorMA(end, u, up, end);
    VectorSubtract(end, start, dir);
    VectorNormalize(dir);

    scale = baseSpeed + (random() * additionalSpeed);
    VectorScale(dir, scale, bolt->s.pos.trDelta);
	SnapVector( bolt->s.pos.trDelta );			// save net bandwidth

	VectorCopy (start, bolt->r.currentOrigin);

#if FEAT_DESTROYABLE_MISSILES
	G_MakeMissileDestroyable( bolt );  // 11B
#endif

	return bolt;
}

//=============================================================================


/*
=================
fire_grenade
=================
*/
gentity_t *fire_grenade (gentity_t *self, vec3_t start, vec3_t dir, int time, qboolean bounce) {
	gentity_t	*bolt;
	float       speed;
	int         splashRadius;

	if (g_excessive.integer) {
		speed = 1200;
		splashRadius = 300;
	}
	else {
		speed = 600;
		splashRadius = 160;
	}

	VectorNormalize (dir);

	bolt = G_Spawn();
	bolt->classname = "grenade";
	bolt->nextthink = level.time + time;
	bolt->think = G_ExplodeMissile;
	bolt->s.eType = ET_MISSILE;
	bolt->r.svFlags = SVF_USE_CURRENT_ORIGIN;
	bolt->s.weapon = WP_GRENADE_LAUNCHER;
#if FEAT_GRENADE_REST_FIX
	// give grenades angular velocity so they tumble and rest at natural angles
	bolt->s.apos.trType = TR_LINEAR;
	bolt->s.apos.trTime = level.time;
	bolt->s.angles[0] = rand() % 360;
	bolt->s.angles[1] = rand() % 360;
	VectorCopy( bolt->s.angles, bolt->s.apos.trBase );
	bolt->s.apos.trDelta[0] = 300 + (rand() % 300);
	bolt->s.apos.trDelta[1] = 300 + (rand() % 300);
#endif
	bolt->s.eFlags = (bounce) ? EF_GRENADE_BOUNCE : 0;
	bolt->r.ownerNum = self->s.number;
	bolt->parent = self;
#if FEAT_UNLAGGED
	bolt->s.otherEntityNum = self->s.number;
#endif
	bolt->damage = 120;
	bolt->splashDamage = 120;
	bolt->splashRadius = splashRadius;
	bolt->methodOfDeath = MOD_GRENADE;
	bolt->splashMethodOfDeath = MOD_GRENADE_SPLASH;
	bolt->clipmask = MASK_SHOT;
	bolt->target_ent = NULL;

	bolt->s.pos.trType = TR_GRAVITY;
	bolt->s.pos.trTime = level.time - MISSILE_PRESTEP_TIME;		// move a bit on the very first frame
	VectorCopy( start, bolt->s.pos.trBase );
	VectorScale( dir, speed, bolt->s.pos.trDelta );
	SnapVector( bolt->s.pos.trDelta );			// save net bandwidth

	VectorCopy (start, bolt->r.currentOrigin);

#if FEAT_DESTROYABLE_MISSILES
	G_MakeMissileDestroyable( bolt );  // 11B
#endif

	return bolt;
}

//=============================================================================


/*
=================
fire_rocket
=================
*/
gentity_t *fire_rocket (gentity_t *self, vec3_t start, vec3_t dir) {
	gentity_t	*bolt;
	float       speed;
	int         splashRadius;

	if (g_excessive.integer) {
		speed = 2000;
		splashRadius = 240;
	}
	else {
		speed = 1000;
		splashRadius = 120;
	}

	VectorNormalize (dir);

	bolt = G_Spawn();
	bolt->classname = "rocket";
	bolt->nextthink = level.time + 15000;
	bolt->think = G_ExplodeMissile;
	bolt->s.eType = ET_MISSILE;
	bolt->r.svFlags = SVF_USE_CURRENT_ORIGIN;
	bolt->s.weapon = WP_ROCKET_LAUNCHER;
	bolt->r.ownerNum = self->s.number;
	bolt->parent = self;
#if FEAT_UNLAGGED
	bolt->s.otherEntityNum = self->s.number;
#endif
	bolt->damage = 120;
	bolt->splashDamage = 120;
	bolt->splashRadius = splashRadius;
	bolt->methodOfDeath = MOD_ROCKET;
	bolt->splashMethodOfDeath = MOD_ROCKET_SPLASH;
	bolt->clipmask = MASK_SHOT;
	bolt->target_ent = NULL;

	bolt->s.pos.trType = TR_LINEAR;
	bolt->s.pos.trTime = level.time - MISSILE_PRESTEP_TIME;		// move a bit on the very first frame
	VectorCopy( start, bolt->s.pos.trBase );
    VectorScale(dir, speed, bolt->s.pos.trDelta);
	SnapVector( bolt->s.pos.trDelta );			// save net bandwidth
	VectorCopy (start, bolt->r.currentOrigin);

#if FEAT_DESTROYABLE_MISSILES
	G_MakeMissileDestroyable( bolt );  // 11B
#endif

	return bolt;
}

/*
=================
fire_grapple
=================
*/
gentity_t *fire_grapple (gentity_t *self, vec3_t start, vec3_t dir) {
	gentity_t	*hook;
	float       speed;

	if (g_excessive.integer) {
		speed = 2800;
	}
	else {
		speed = 1800;
	}

	VectorNormalize (dir);

	hook = G_Spawn();
	hook->classname = "hook";
	hook->nextthink = level.time + 10000;
	hook->think = Offhand_Grapple_Free;
	hook->s.eType = ET_MISSILE;
	hook->r.svFlags = SVF_USE_CURRENT_ORIGIN;
	hook->s.weapon = WP_NONE;
	hook->r.ownerNum = self->s.number;
	hook->methodOfDeath = MOD_GRAPPLE;
	hook->clipmask = MASK_SHOT;
	hook->parent = self;
	hook->target_ent = NULL;

	hook->s.pos.trType = TR_GRAVITY_DOUBLE;
	hook->s.pos.trTime = level.time - MISSILE_PRESTEP_TIME;		// move a bit on the very first frame
	hook->s.otherEntityNum = self->s.number; // use to match beam in client
	VectorCopy( start, hook->s.pos.trBase );
	VectorScale( dir, speed, hook->s.pos.trDelta );
	SnapVector( hook->s.pos.trDelta );			// save net bandwidth
	VectorCopy (start, hook->r.currentOrigin);

	self->client->hook = hook;

	return hook;
}
