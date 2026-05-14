// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 1999-2005 Id Software, Inc.
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
// cg_znudge.c -- client-side forward extrapolation (player / missile prediction)
//
// Adapted from ZNudge by zNj.  Predicts where players and projectiles will
// be in the near future so the local client can lead targets correctly even
// on high-ping connections.
//

#include "cg_local.h"

#if FEAT_ZNUDGE

// ── cvars (registered elsewhere, declared extern here) ──────────────
extern vmCvar_t		cg_znudge;
extern vmCvar_t		cg_znSmoothweight;
extern vmCvar_t		cg_znProjectiles;
extern vmCvar_t		cg_znOffset;
extern vmCvar_t		cg_znPingWeight;
extern vmCvar_t		cg_znGravity;
extern vmCvar_t		cg_znMaxclips;
extern vmCvar_t		cg_znClimbheight;
extern vmCvar_t		cg_znRunningspeed;
extern vmCvar_t		cg_znDrawball;

#define ZN_MAX_OFFSET		1000
#define ZN_BOUNCE_FACTOR	0.65f

/*
===============
ZN_GetNudge

Returns the time in seconds to nudge the current frame by.
Nudge is the smoothed ping plus user offset, converted to seconds.
===============
*/
float ZN_GetNudge( void ) {
	int		ping;
	float	nudge;
	int		offset;

	offset = cg_znOffset.integer;
	if ( offset > ZN_MAX_OFFSET )
		offset = ZN_MAX_OFFSET;
	if ( offset < -ZN_MAX_OFFSET )
		offset = -ZN_MAX_OFFSET;

	ping = cg.snap ? cg.snap->ping : 0;

	if ( cg_znPingWeight.value <= 1.0f ) {
		float weight = cg_znPingWeight.value;

		if ( weight < 0.0f )
			weight = 0.0f;

		cg.smoothPing = (float)ping * weight + cg.smoothPing * ( 1.0f - weight );
		ping = (int)cg.smoothPing;
	}

	nudge = ( ping + offset ) / 1000.0f;

	return nudge;
}

/*
===============
ZN_GetVelocity

Retrieve a smoothed velocity for the given entity.  The smooth
weight blends the previous velocity with the current snapshot
velocity to reduce jitter from dropped or delayed packets.
===============
*/
void ZN_GetVelocity( centity_t *cent, vec3_t velocity ) {
	int		index;
	float	weight;

	index = cent->currentState.number;

	weight = cg_znSmoothweight.value;
	if ( weight > 1.0f )
		weight = 1.0f;
	if ( weight < 0.0f )
		weight = 0.0f;

	if ( weight == 1.0f ) {
		VectorCopy( cent->currentState.pos.trDelta, velocity );
	} else {
		velocity[0] = cg.smoothVelocities[index][0] =
			( 1.0f - weight ) * cg.smoothVelocities[index][0] + weight * cent->currentState.pos.trDelta[0];
		velocity[1] = cg.smoothVelocities[index][1] =
			( 1.0f - weight ) * cg.smoothVelocities[index][1] + weight * cent->currentState.pos.trDelta[1];
		velocity[2] = cg.smoothVelocities[index][2] =
			( 1.0f - weight ) * cg.smoothVelocities[index][2] + weight * cent->currentState.pos.trDelta[2];
	}
}

/*
===============
ZN_PredictSimple

Forward-extrapolate origin by nudge seconds without clipping.
Applies optional gravity to the Z component.
===============
*/
void ZN_PredictSimple( vec3_t origin, vec3_t velocity, float gravity, float nudge, vec3_t predicted ) {
	predicted[0] = origin[0] + velocity[0] * nudge;
	predicted[1] = origin[1] + velocity[1] * nudge;
	predicted[2] = origin[2] + velocity[2] * nudge;

	if ( gravity != 0.0f ) {
		predicted[2] -= nudge * nudge * gravity / 2.0f;
	}
}

/*
===============
ZN_TimeToPoint

Compute how long it takes for a point starting at origin with the
given velocity and gravity to reach destination.  Uses x or y for
linear prediction, falls back to the quadratic formula on z when
gravity is non-zero.
===============
*/
static float ZN_TimeToPoint( vec3_t origin, vec3_t velocity, float gravity, vec3_t destination ) {
	vec3_t	diff;
	int		sorted[3] = { 0, 1, 2 };
	int		temp, i, j, coord;

	diff[0] = destination[0] - origin[0];
	diff[1] = destination[1] - origin[1];
	diff[2] = destination[2] - origin[2];

	// insertion sort: decreasing order by magnitude
	for ( i = 1; i < 3; i++ ) {
		for ( j = 0; j < i; j++ ) {
			if ( abs( (int)diff[i] ) > abs( (int)diff[j] ) ) {
				temp = sorted[i];
				sorted[i] = sorted[j];
				sorted[j] = temp;
			}
		}
	}

	if ( diff[sorted[0]] == 0.0f ) {
		return 0.0f;
	}

	for ( i = 0; i < 3; i++ ) {
		coord = sorted[i];

		if ( coord == 2 && gravity != 0.0f ) {
			// quadratic formula for z with gravity
			float a = -gravity / 2.0f;
			float b = velocity[2];
			float c = -diff[2];
			float disc = b * b - 4.0f * a * c;

			if ( disc >= 0.0f ) {
				float sqrtDisc = sqrt( disc );
				float root1 = ( -b + sqrtDisc ) / ( 2.0f * a );
				float root2 = ( -b - sqrtDisc ) / ( 2.0f * a );
				float time;

				if ( root1 >= 0.0f || root2 >= 0.0f ) {
					if ( root1 < 0.0f )
						time = root2;
					else if ( root2 < 0.0f )
						time = root1;
					else if ( root1 < root2 )
						time = root1;
					else
						time = root2;

					return time;
				}
			}
		} else if ( velocity[coord] != 0.0f ) {
			return diff[coord] / velocity[coord];
		}
	}

	return -1.0f;
}

/*
===============
ZN_CheckGround

Check whether the player is on the ground at origin.  If so, adjust
for stair-climbing and cap horizontal speed to cg_znRunningspeed.
Returns qtrue if on ground.
===============
*/
int ZN_CheckGround( centity_t *cent, vec3_t origin, vec3_t velocity, vec3_t predictedOrigin ) {
	vec3_t		traceStart, traceEnd;
	trace_t		trace;
	int			onGround;

	traceStart[0] = origin[0];
	traceStart[1] = origin[1];
	traceStart[2] = origin[2] + 22;		// 32 - 10

	traceEnd[0] = origin[0];
	traceEnd[1] = origin[1];
	traceEnd[2] = origin[2] - 26;

	CG_Trace( &trace, traceStart, NULL, NULL, traceEnd, cent->currentState.number, MASK_PLAYERSOLID );

	onGround = ( trace.fraction < 1.0f );

	if ( onGround ) {
		float	speed, maxSpeed;
		float	newZ = trace.endpos[2] + 24;

		if ( newZ > origin[2] ) {
			origin[2] = predictedOrigin[2] = newZ;
		}

		// cap running speed
		speed = sqrt( velocity[0] * velocity[0] + velocity[1] * velocity[1] );

		maxSpeed = cg_znRunningspeed.value;
		if ( cent->currentState.powerups & ( 1 << PW_HASTE ) )
			maxSpeed *= 1.3f;

		if ( speed > maxSpeed ) {
			float ratio = maxSpeed / speed;
			velocity[0] *= ratio;
			velocity[1] *= ratio;
		}
	}

	return onGround;
}

/*
===============
ZN_PredictPlayer

Full physics-aware forward extrapolation for a player entity.
Handles ground/air detection, gravity, wall clipping with slide,
stair climbing, haste powerup, flight, and water.
===============
*/
void ZN_PredictPlayer( centity_t *cent, float nudge, vec3_t predictedOrigin ) {
	vec3_t		mins = { -15, -15, -24 };
	vec3_t		maxs = { 15, 15, 32 };
	int			onGround;
	int			inWater;
	trace_t		trace;
	float		nudgeRemaining = nudge;
	int			clips = 0;
	vec3_t		origin;
	vec3_t		velocity;
	vec3_t		temp;
	float		gravity;
	int			contents;

	VectorCopy( cent->lerpOrigin, origin );
	ZN_GetVelocity( cent, velocity );
	VectorCopy( origin, predictedOrigin );

	onGround = ( cent->currentState.groundEntityNum != ENTITYNUM_NONE );

	while ( nudgeRemaining > 0.0f && clips < cg_znMaxclips.integer ) {
		// check water
		temp[0] = origin[0];
		temp[1] = origin[1];
		temp[2] = origin[2] - 24;
		contents = trap_CM_PointContents( temp, 0 );
		inWater = contents & ( CONTENTS_WATER | CONTENTS_SLIME | CONTENTS_LAVA );

		if ( onGround ) {
			// shrink lower bound to step over stairs
			mins[2] = -24 + cg_znClimbheight.integer;
			gravity = 0.0f;
		} else {
			mins[2] = -24;

			if ( ( cent->currentState.powerups & ( 1 << PW_FLIGHT ) ) || inWater ) {
				gravity = 0.0f;
			} else {
				gravity = cg_znGravity.value;
			}
		}

		ZN_PredictSimple( origin, velocity, gravity, nudgeRemaining, predictedOrigin );

		trap_CM_BoxTrace( &trace, origin, predictedOrigin, mins, maxs, 0, MASK_PLAYERSOLID );

		if ( trace.fraction == 1.0f ) {
			// no obstruction
			break;
		} else {
			float	travelTime;
			float	dot;

			clips++;

			travelTime = ZN_TimeToPoint( origin, velocity, gravity, trace.endpos );
			if ( travelTime < 0.0f ) {
				travelTime = 0.05f;
			}
			nudgeRemaining -= travelTime;

			VectorCopy( trace.endpos, origin );
			VectorCopy( trace.endpos, predictedOrigin );

			if ( !onGround ) {
				// update velocity with gravity
				velocity[2] -= gravity * travelTime;
			}

			// slide along the clipping plane
			dot = DotProduct( velocity, trace.plane.normal );
			velocity[0] -= dot * trace.plane.normal[0];
			velocity[1] -= dot * trace.plane.normal[1];
			velocity[2] -= dot * trace.plane.normal[2];
		}

		onGround = ZN_CheckGround( cent, origin, velocity, predictedOrigin );
	}
}

/*
===============
ZN_PredictMissile

Extrapolate a linear missile (rockets, plasma, BFG) forward by
nudge seconds, clipping against world geometry via CG_Trace.
===============
*/
void ZN_PredictMissile( centity_t *cent, float nudge, vec3_t predictedOrigin ) {
	trace_t	tr;

	predictedOrigin[0] = cent->lerpOrigin[0] + nudge * cent->currentState.pos.trDelta[0];
	predictedOrigin[1] = cent->lerpOrigin[1] + nudge * cent->currentState.pos.trDelta[1];
	predictedOrigin[2] = cent->lerpOrigin[2] + nudge * cent->currentState.pos.trDelta[2];

	CG_Trace( &tr, cent->lerpOrigin, NULL, NULL, predictedOrigin, -1, MASK_SHOT );

	VectorCopy( tr.endpos, predictedOrigin );
}

/*
===============
ZN_PredictGrenade

Extrapolate a grenade with bounce physics.  Simulates gravity,
reflection off surfaces with a bounce damping factor, and halting
when the grenade speed drops below a threshold on a floor.
===============
*/
void ZN_PredictGrenade( centity_t *cent, float nudge, vec3_t predictedOrigin ) {
	trace_t	tr;
	int		clips = 0;
	float	nudgeRemaining = nudge;
	float	stickSpeedSq = 40.0f * 40.0f;		// stop threshold
	vec3_t	origin;
	vec3_t	velocity;

	VectorCopy( cent->lerpOrigin, origin );
	VectorCopy( cent->currentState.pos.trDelta, velocity );

	while ( nudgeRemaining > 0.0f && clips < cg_znMaxclips.integer ) {
		float	nudgeStep = nudgeRemaining;

		ZN_PredictSimple( origin, velocity, cg_znGravity.value, nudgeStep, predictedOrigin );

		CG_Trace( &tr, origin, NULL, NULL, predictedOrigin, -1, MASK_SHOT );

		if ( tr.allsolid || tr.startsolid ) {
			VectorCopy( origin, predictedOrigin );
			break;
		} else if ( tr.fraction == 1.0f ) {
			// no collision this step
			nudgeRemaining -= nudgeStep;

			VectorCopy( predictedOrigin, origin );
			velocity[2] -= nudgeStep * cg_znGravity.value;
		} else {
			float	travelTime;
			float	speedSq;
			float	dot;

			clips++;

			travelTime = ZN_TimeToPoint( origin, velocity, cg_znGravity.value, tr.endpos );
			if ( travelTime < 0.0f ) {
				travelTime = 0.05f;
			}
			nudgeRemaining -= travelTime;

			VectorCopy( tr.endpos, origin );
			VectorCopy( tr.endpos, predictedOrigin );

			velocity[2] -= cg_znGravity.value * travelTime;

			// reflect velocity by surface plane
			dot = DotProduct( velocity, tr.plane.normal );
			velocity[0] -= 2.0f * dot * tr.plane.normal[0];
			velocity[1] -= 2.0f * dot * tr.plane.normal[1];
			velocity[2] -= 2.0f * dot * tr.plane.normal[2];

			// dampen by bounce factor
			velocity[0] *= ZN_BOUNCE_FACTOR;
			velocity[1] *= ZN_BOUNCE_FACTOR;
			velocity[2] *= ZN_BOUNCE_FACTOR;

			speedSq = DotProduct( velocity, velocity );

			// if on a floor-like surface and slow enough, grenade stops
			if ( tr.plane.normal[2] > 0.2f && speedSq < stickSpeedSq ) {
				break;
			}
		}
	}
}

#endif // FEAT_ZNUDGE
