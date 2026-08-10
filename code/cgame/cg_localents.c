// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 1999-2005 Id Software, Inc.
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//

// cg_localents.c -- every frame, generate renderer commands for locally
// processed entities, like smoke puffs, gibs, shells, etc.

#include "cg_local.h"
#include "../qcommon/wired/render/primitives.h"	// emitterDesc_t (MIG-trail-2)
#include "../qcommon/wired/render/traps.h"		// trap_R_EmitParticles (MIG-trail-2)
#if FEAT_WIRED_UI
#include "wired/cg_wired_store.h"				// WUI_StageMarkers_* (WA-2b plum markers)
#endif

#define	MAX_LOCAL_ENTITIES	2048

// Gib bounce-mark tunables (modder-tunable code-level constants, NOT user
// cvars — same policy as the gib GIB_PART_* and brass BRASS_* constants).
// A gib now leaves a blood mark on every bounce whose impact speed clears
// GIB_BOUNCE_MARK_MIN_SPEED (replacing the old one-mark-per-gib guard, which a
// gently-settling gib would otherwise spam). Mark radius scales with impact
// speed up to GIB_BOUNCE_MARK_SPEED_REF, then by a per-gib-model factor.
#define GIB_BOUNCE_MARK_MIN_SPEED	200.0f	// min per-bounce impact speed to mark
#define GIB_BOUNCE_MARK_SPEED_REF	800.0f	// impact speed that saturates the size factor
#define GIB_BOUNCE_MARK_BASE_RADIUS	16.0f	// minimum blood-mark radius
#define GIB_BOUNCE_MARK_SPEED_SPREAD	24.0f	// extra radius added by the speed/jitter factor

// Better-gibs D-feel (0035): per-bounce restitution randomization for blood gibs.
// Each blood-gib bounce scales its bounceFactor by (1 - r*GIB_BOUNCE_RANDOMNESS),
// r = (random()+random())*0.5 (a tighter-than-uniform triangular distribution), so
// identical gibs don't bounce in lockstep — they settle with varied energy, reading
// as "dead" rather than synchronized. Blood-only (brass/debris keep flat restitution
// so the S-group brass settle is byte-identical). Modder-tunable code-level constant,
// NOT a user cvar. 0.0 = flat (byte-identical to pre-0035). Local random() is fine:
// bounce is per-client cosmetic, non-networked (same as brass S-group).
#define GIB_BOUNCE_RANDOMNESS	0.5f

// Base cubic half-extent of a gib's movement-collision box, scaled per gib model
// below. Gibs use a small box (instead of a point) so they rest ON a surface
// instead of sinking their lower half into it; brass/debris keep the point trace.
// Modder-tunable code-level constant, NOT a user cvar.
#define GIB_COLLISION_SIZE	2.0f
localEntity_t	cg_localEntities[MAX_LOCAL_ENTITIES];
localEntity_t	cg_activeLocalEntities;		// double linked list
localEntity_t	*cg_freeLocalEntities;		// single linked list

/*
===================
CG_InitLocalEntities

This is called at startup and for tournement restarts
===================
*/
void	CG_InitLocalEntities( void ) {
	memset( cg_localEntities, 0, sizeof( cg_localEntities ) );
	cg_activeLocalEntities.next = &cg_activeLocalEntities;
	cg_activeLocalEntities.prev = &cg_activeLocalEntities;
	cg_freeLocalEntities = cg_localEntities;
	for ( int i = 0 ; i < MAX_LOCAL_ENTITIES - 1 ; i++ ) {
		cg_localEntities[i].next = &cg_localEntities[i+1];
	}
}


/*
==================
CG_FreeLocalEntity
==================
*/
void CG_FreeLocalEntity( localEntity_t *le ) {
	if ( !le->prev ) {
		Com_Terminate( TERM_CLIENT_DROP, "CG_FreeLocalEntity: not active" );
	}

	// remove from the doubly linked active list
	le->prev->next = le->next;
	le->next->prev = le->prev;

	// the free list is only singly linked
	le->next = cg_freeLocalEntities;
	cg_freeLocalEntities = le;
}

/*
===================
CG_AllocLocalEntity

Will always succeed, even if it requires freeing an old active entity
===================
*/
localEntity_t	*CG_AllocLocalEntity( void ) {
	localEntity_t	*le;

	if ( !cg_freeLocalEntities ) {
		// no free entities, so free the one at the end of the chain
		// remove the oldest active entity
		CG_FreeLocalEntity( cg_activeLocalEntities.prev );
	}

	le = cg_freeLocalEntities;
	cg_freeLocalEntities = cg_freeLocalEntities->next;

	memset( le, 0, sizeof( *le ) );

	// link into the active list
	le->next = cg_activeLocalEntities.next;
	le->prev = &cg_activeLocalEntities;
	cg_activeLocalEntities.next->prev = le;
	cg_activeLocalEntities.next = le;
	return le;
}


/*
====================================================================================

FRAGMENT PROCESSING

A fragment localentity interacts with the environment in some way (hitting walls),
or generates more localentities along a trail.

====================================================================================
*/

/*
================
CG_BloodTrail

Leave expanding blood puffs behind gibs
================
*/
void CG_BloodTrail( localEntity_t *le ) {
	int		t;
	int		t2;
	int		step;

#if FEAT_SCREENSHOT_TOOLS
	if ( cg.stopTime ) return;
#endif

	step = 150;
	t = step * ( (cg.time - cg.frametime + step ) / step );
	t2 = step * ( cg.time / step );

	if ( cgs.media.gibTrailClass ) {
		// GPU-ring blood-trail (MIG-trail-2, single path — W-51 cvar gate
		// retired): ONE EMIT_PATH per frame over the step=150 grid in [t, t2]
		// instead of N legacy LE_FALL_SCALE_FADE puffs. Density is TIME-quantized
		// (one drift-sprite per 150 ms grid point), matching the old loop exactly
		// — NOT distance-quantized. count = number of grid points; the path runs
		// from the trajectory position at the first grid point to the last. The
		// blood_trail class carries the exact look (alpha-blend, grow 16→36, alpha
		// 1.0→0 over 2 s, linear -Z drift 40 u), so colorTint is neutral white.
		// The gib BODY (LE_FRAGMENT) is unaffected — only the per-frame
		// drift-sprites migrate.
		emitterDesc_t emitter;
		vec3_t        segStart;
		vec3_t        segEnd;
		int           count;

		count = ( t <= t2 ) ? ( ( t2 - t ) / step + 1 ) : 0;
		if ( count > 0 ) {
			vec3_t axis;
			BG_EvaluateTrajectory( &le->pos, t,  segStart ); // first grid point
			BG_EvaluateTrajectory( &le->pos, t2, segEnd );   // last grid point
			memset( &emitter, 0, sizeof( emitter ) );
			emitter.cls   = cgs.media.gibTrailClass;
			emitter.count = count;
			VectorCopy( segStart, emitter.origin );
			VectorCopy( segEnd,   emitter.end );
			VectorSubtract( segEnd, segStart, axis );
			VectorNormalize( axis );
			VectorCopy( axis, emitter.axis );
			emitter.colorTint[0] = 1.0f; emitter.colorTint[1] = 1.0f;
			emitter.colorTint[2] = 1.0f; emitter.colorTint[3] = 1.0f;
			trap_R_EmitParticles( &emitter );
		}
	}
}


/*
================
CG_FragmentBounceMark
================
*/
void CG_FragmentBounceMark( localEntity_t *le, trace_t *trace, const vec3_t impactVelocityDiff ) {
	float		radius;

	if ( le->leMarkType == LEMT_BLOOD ) {
		float	radiusFactor;
		float	modelScale;

		// Speed-dependent size: a harder impact splats a bigger mark. The factor
		// saturates at GIB_BOUNCE_MARK_SPEED_REF and degrades to ~0 at low speed,
		// so a slow settle still leaves only the small base mark.
		radiusFactor = VectorLengthSquared( impactVelocityDiff ) / Square( GIB_BOUNCE_MARK_SPEED_REF );
		if ( radiusFactor > 1.0f ) {
			radiusFactor = 1.0f;
		}

		// Per-gib-model scale: bigger body parts leave bigger marks, small bits
		// smaller. Keyed on the model handle CG_LaunchGib set on this fragment.
		if ( le->refEntity.hModel == cgs.media.gibIntestine ) {
			modelScale = 0.25f;
		} else if ( le->refEntity.hModel == cgs.media.gibSkull ||
					le->refEntity.hModel == cgs.media.gibFist ) {
			modelScale = 0.5f;
		} else if ( le->refEntity.hModel == cgs.media.gibAbdomen ||
					le->refEntity.hModel == cgs.media.gibChest ||
					le->refEntity.hModel == cgs.media.gibLeg ) {
			modelScale = 1.25f;
		} else {
			modelScale = 1.0f;
		}

		radius = GIB_BOUNCE_MARK_BASE_RADIUS +
			( radiusFactor + 0.25f * crandom() ) * GIB_BOUNCE_MARK_SPEED_SPREAD;
		radius *= modelScale;
		// CG_ImpactMark Com_Terminates on radius <= 0; floor it so no future
		// tuning of the speed/scale factors can ever drive it to zero.
		if ( radius < 1.0f ) {
			radius = 1.0f;
		}

		CG_ImpactMark( cgs.media.bloodMarkShader, trace->endpos, trace->plane.normal, random()*360,
			1,1,1,1, qtrue, radius, qfalse );
	} else if ( le->leMarkType == LEMT_BURN ) {

		radius = 8 + (rand()&15);
		CG_ImpactMark( cgs.media.burnMarkShader, trace->endpos, trace->plane.normal, random()*360,
			1,1,1,1, qtrue, radius, qfalse );
	}

	// No one-shot guard: a gib may mark on each bounce. The caller gates the call
	// on impact speed (GIB_BOUNCE_MARK_MIN_SPEED), so a settling gib whose bounces
	// fall below the threshold stops marking instead of piling marks up.
}

/*
================
CG_FragmentBounceSound
================
*/
void CG_FragmentBounceSound( localEntity_t *le, trace_t *trace ) {
	if ( le->leBounceSoundType == LEBS_BLOOD ) {
		// half the gibs will make splat sounds
		if ( rand() & 1 ) {
			int r = rand()&3;
			sfxHandle_t	s;

			if ( r == 0 ) {
				s = cgs.media.gibBounce1Sound;
			} else if ( r == 1 ) {
				s = cgs.media.gibBounce2Sound;
			} else {
				s = cgs.media.gibBounce3Sound;
			}
			trap_S_StartSound( trace->endpos, ENTITYNUM_WORLD, CHAN_AUTO, s );
		}
	} else if ( le->leBounceSoundType == LEBS_BRASS ) {

	}

	// don't allow a fragment to make multiple bounce sounds,
	// or it gets too noisy as they settle
	le->leBounceSoundType = LEBS_NONE;
}


/*
================
CG_ReflectVelocity
================
*/
void CG_ReflectVelocity( localEntity_t *le, trace_t *trace, vec3_t velocityDifference ) {
	vec3_t	velocity;
	float	dot;
	int		hitTime;

	// reflect the velocity on the trace plane
	hitTime = cg.time - cg.frametime + cg.frametime * trace->fraction;
	BG_EvaluateTrajectoryDelta( &le->pos, hitTime, velocity );
	dot = DotProduct( velocity, trace->plane.normal );
	VectorMA( velocity, -2*dot, trace->plane.normal, le->pos.trDelta );

	{
		// Better-gibs D-feel (0035): randomize the restitution per bounce for blood
		// gibs only (tighter-than-uniform), so they settle with varied energy
		// instead of in lockstep. Brass/debris (LEBS_BRASS / LEBS_NONE) keep the
		// flat le->bounceFactor — byte-identical to the S-group brass settle.
		float bounceFactor = le->bounceFactor;
		if ( le->leBounceSoundType == LEBS_BLOOD ) {
			float r = ( random() + random() ) * 0.5f;
			bounceFactor *= 1.0f - r * GIB_BOUNCE_RANDOMNESS;
		}
		VectorScale( le->pos.trDelta, bounceFactor, le->pos.trDelta );
	}

	// impact magnitude = how much velocity the bounce shed (pre-bounce minus the
	// reflected/damped post-bounce). The caller uses this to gate + size the mark.
	if ( velocityDifference != NULL ) {
		VectorSubtract( velocity, le->pos.trDelta, velocityDifference );
	}

	VectorCopy( trace->endpos, le->pos.trBase );
	le->pos.trTime = cg.time;


	// check for stop, making sure that even on low FPS systems it doesn't bobble
	if ( trace->allsolid ||
		( trace->plane.normal[2] > 0 &&
		( le->pos.trDelta[2] < 40 || le->pos.trDelta[2] < -cg.frametime * le->pos.trDelta[2] ) ) ) {
		le->pos.trType = TR_STATIONARY;

		// Brass-only ground-settle: lay the shell flat on the surface instead of
		// freezing at its mid-air tumble orientation. Build a resting axis whose
		// "up" is the ground-plane normal; the two in-plane axes are an arbitrary
		// perpendicular basis (brass is roughly symmetric, so any flat yaw reads
		// fine). Gated on LEF_BRASS so gib settle (and other fragments) is
		// byte-identical — gibs keep their frozen-tumble settle. Snap, not lerp:
		// the settle is a single one-shot transition, brass is small, and a clean
		// snap avoids per-frame lerp state on the local entity.
		if ( ( le->leFlags & LEF_BRASS ) && !trace->allsolid &&
			 trace->plane.normal[2] > 0 ) {
			vec3_t	flatAxis[3];

			VectorCopy( trace->plane.normal, flatAxis[2] );
			PerpendicularVector( flatAxis[0], flatAxis[2] );
			CrossProduct( flatAxis[2], flatAxis[0], flatAxis[1] );
			AxisCopy( flatAxis, le->refEntity.axis );
		}
	} else {

	}
}

/*
================
GetFragmentMinsMaxs

Per-gib collision box for the movement trace. Gibs (LEMT_BLOOD) get a small
CUBIC box scaled per model so they rest on a surface instead of clipping their
lower half into it; everything else (brass / explode-debris, LEMT_NONE) gets no
box and keeps the zero-extent point trace. Returns qtrue if a box was set.

The box stays CUBIC (symmetric, all axes equal before the per-model scale)
because the gib tumbles inside it (LEF_TUMBLE) — a model-shaped box would snag
as it rotates. Per-model factors are hand-tuned (ported from the better-gibs
mod) so each part rests naturally without sticking in floors/walls.
================
*/
static qboolean GetFragmentMinsMaxs( const localEntity_t *le, vec3_t mins, vec3_t maxs ) {
	float	sizeFactor;
	float	half;

	if ( le->leMarkType != LEMT_BLOOD ) {
		return qfalse;		// brass / debris: point trace, unchanged
	}

	if ( le->refEntity.hModel == cgs.media.gibSkull ) {
		sizeFactor = 2.0f;
	} else if ( le->refEntity.hModel == cgs.media.gibIntestine ||
				le->refEntity.hModel == cgs.media.gibBrain ||
				le->refEntity.hModel == cgs.media.gibFist ||
				le->refEntity.hModel == cgs.media.gibForearm ) {
		sizeFactor = 0.5f;
	} else if ( le->refEntity.hModel == cgs.media.gibAbdomen ) {
		sizeFactor = 1.5f;
	} else if ( le->refEntity.hModel == cgs.media.gibChest ) {
		sizeFactor = 1.75f;
	} else if ( le->refEntity.hModel == cgs.media.gibLeg ) {
		sizeFactor = 1.25f;
	} else if ( le->refEntity.hModel == cgs.media.gibFoot ) {
		sizeFactor = 0.25f;		// deliberately tiny — a bigger box sticks in the ground
	} else {
		sizeFactor = 1.0f;
	}

	half = GIB_COLLISION_SIZE * sizeFactor;
	VectorSet( mins, -half, -half, -half );
	VectorSet( maxs,  half,  half,  half );
	return qtrue;
}

/*
================
CG_AddFragment
================
*/
void CG_AddFragment( localEntity_t *le ) {
	vec3_t	newOrigin;
	trace_t	trace;
	vec3_t	mins, maxs;
	qboolean hasBox;

	if ( le->pos.trType == TR_STATIONARY ) {
		// sink into the ground if near the removal time
		int		t;
		float	oldZ;

		t = le->endTime - cg.time;
		if ( t < SINK_TIME ) {
			// we must use an explicit lighting origin, otherwise the
			// lighting would be lost as soon as the origin went
			// into the ground
			VectorCopy( le->refEntity.origin, le->refEntity.lightingOrigin );
			le->refEntity.renderfx |= RF_LIGHTING_ORIGIN;
			oldZ = le->refEntity.origin[2];
			le->refEntity.origin[2] -= 16 * ( 1.0 - (float)t / SINK_TIME );
			trap_R_AddRefEntityToScene( &le->refEntity );
			le->refEntity.origin[2] = oldZ;
		} else {
			trap_R_AddRefEntityToScene( &le->refEntity );
		}

		return;
	}

	// calculate new position
	BG_EvaluateTrajectory( &le->pos, cg.time, newOrigin );

	// trace from previous to new position. Gibs sweep a small per-model box so
	// they rest on surfaces instead of sinking; brass/debris pass NULL/NULL for a
	// zero-extent point trace, byte-identical to the original behavior.
	hasBox = GetFragmentMinsMaxs( le, mins, maxs );
	if ( hasBox ) {
		CG_Trace( &trace, le->refEntity.origin, mins, maxs, newOrigin, -1, CONTENTS_SOLID );
	} else {
		CG_Trace( &trace, le->refEntity.origin, NULL, NULL, newOrigin, -1, CONTENTS_SOLID );
	}
	if ( trace.fraction == 1.0 ) {
		// still in free fall
		VectorCopy( newOrigin, le->refEntity.origin );

		if ( le->leFlags & LEF_TUMBLE ) {
			vec3_t angles;

			BG_EvaluateTrajectory( &le->angles, cg.time, angles );
			AnglesToAxis( angles, le->refEntity.axis );
		}

		trap_R_AddRefEntityToScene( &le->refEntity );

		// add a blood trail. Gated on a dedicated leFlags bit, not on
		// leBounceSoundType: the bounce-sound code clears that field to LEBS_NONE
		// on the first bounce (so the splat is one-shot), which used to silently
		// kill the trail too. LEF_BLOOD_TRAIL is never cleared, so the trail keeps
		// emitting across bounces.
		if ( le->leFlags & LEF_BLOOD_TRAIL ) {
			CG_BloodTrail( le );
		}

		return;
	}

	// if it is in a nodrop zone, remove it
	// this keeps gibs from waiting at the bottom of pits of death
	// and floating levels
	if ( CG_PointContents( trace.endpos, 0 ) & CONTENTS_NODROP ) {
		CG_FreeLocalEntity( le );
		return;
	}

	// reflect the velocity on the trace plane first, capturing the impact
	// magnitude so the mark can be gated and sized by how hard the gib hit
	{
		vec3_t	velocityDifference;

		CG_ReflectVelocity( le, &trace, velocityDifference );

		// leave a mark — only on a bounce that hits hard enough, so a gib does
		// not pile marks while gently settling (replaces the old one-mark guard)
		if ( VectorLengthSquared( velocityDifference ) >= Square( GIB_BOUNCE_MARK_MIN_SPEED ) ) {
			CG_FragmentBounceMark( le, &trace, velocityDifference );
		}
	}

	// do a bouncy sound (unchanged — not gated on impact speed)
	CG_FragmentBounceSound( le, &trace );

	trap_R_AddRefEntityToScene( &le->refEntity );
}

/*
=====================================================================

TRIVIAL LOCAL ENTITIES

These only do simple scaling or modulation before passing to the renderer
=====================================================================
*/

/*
====================
CG_AddFadeRGB
====================
*/
void CG_AddFadeRGB( localEntity_t *le ) {
	refEntity_t *re;
	float c;

	re = &le->refEntity;

	c = ( le->endTime - cg.time ) * le->lifeRate;
	c *= 0xff;

	re->shaderRGBA[0] = le->color[0] * c;
	re->shaderRGBA[1] = le->color[1] * c;
	re->shaderRGBA[2] = le->color[2] * c;
	re->shaderRGBA[3] = le->color[3] * c;

	trap_R_AddRefEntityToScene( re );
}

/*
==================
CG_AddMoveScaleFade
==================
*/
static void CG_AddMoveScaleFade( localEntity_t *le ) {
	refEntity_t	*re;
	float		c;
	vec3_t		delta;
	float		len;

	re = &le->refEntity;

	if ( le->fadeInTime > le->startTime && cg.time < le->fadeInTime ) {
		// fade / grow time
		c = 1.0 - (float) ( le->fadeInTime - cg.time ) / ( le->fadeInTime - le->startTime );
	}
	else {
		// fade / grow time
		c = ( le->endTime - cg.time ) * le->lifeRate;
	}

	re->shaderRGBA[3] = 0xff * c * le->color[3];

	if ( !( le->leFlags & LEF_PUFF_DONT_SCALE ) ) {
		re->radius = le->radius * ( 1.0 - c ) + 8;
	}

	BG_EvaluateTrajectory( &le->pos, cg.time, re->origin );

	// if the view would be "inside" the sprite, kill the sprite
	// so it doesn't add too much overdraw
	VectorSubtract( re->origin, cg.refdef.vieworg, delta );
	len = VectorLength( delta );
	if ( len < le->radius ) {
#if FEAT_SCREENSHOT_TOOLS
		if ( cg.stopTime ) return;
#endif
		CG_FreeLocalEntity( le );
		return;
	}

	trap_R_AddRefEntityToScene( re );
}


/*
===================
CG_AddScaleFade

For rocket smokes that hang in place, fade out, and are
removed if the view passes through them.
There are often many of these, so it needs to be simple.
===================
*/
static void CG_AddScaleFade( localEntity_t *le ) {
	refEntity_t	*re;
	float		c;
	vec3_t		delta;
	float		len;

	re = &le->refEntity;

	// fade / grow time
	c = ( le->endTime - cg.time ) * le->lifeRate;

	re->shaderRGBA[3] = 0xff * c * le->color[3];
	re->radius = le->radius * ( 1.0 - c ) + 8;

	// if the view would be "inside" the sprite, kill the sprite
	// so it doesn't add too much overdraw
	VectorSubtract( re->origin, cg.refdef.vieworg, delta );
	len = VectorLength( delta );
	if ( len < le->radius ) {
#if FEAT_SCREENSHOT_TOOLS
		if ( cg.stopTime ) return;
#endif
		CG_FreeLocalEntity( le );
		return;
	}

	trap_R_AddRefEntityToScene( re );
}


/*
=================
CG_AddFallScaleFade

This is just an optimized CG_AddMoveScaleFade
For blood mists that drift down, fade out, and are
removed if the view passes through them.
There are often 100+ of these, so it needs to be simple.
=================
*/
static void CG_AddFallScaleFade( localEntity_t *le ) {
	refEntity_t	*re;
	float		c;
	vec3_t		delta;
	float		len;

	re = &le->refEntity;

	// fade time
	c = ( le->endTime - cg.time ) * le->lifeRate;

	re->shaderRGBA[3] = 0xff * c * le->color[3];

	re->origin[2] = le->pos.trBase[2] - ( 1.0 - c ) * le->pos.trDelta[2];

	re->radius = le->radius * ( 1.0 - c ) + 16;

	// if the view would be "inside" the sprite, kill the sprite
	// so it doesn't add too much overdraw
	VectorSubtract( re->origin, cg.refdef.vieworg, delta );
	len = VectorLength( delta );
	if ( len < le->radius ) {
		CG_FreeLocalEntity( le );
		return;
	}

	trap_R_AddRefEntityToScene( re );
}



/*
================
CG_AddExplosion
================
*/
static void CG_AddExplosion( localEntity_t *ex ) {
	refEntity_t	*ent;

	ent = &ex->refEntity;

	// add the entity
	//
	// Light-only LE (hModel == 0 && customShader == 0): the visual is supplied
	// elsewhere (e.g. the rocket-explosion fire flipbook on the GPU particle
	// ring); this LE exists solely to carry the impact dlight fade. Submitting
	// a zero-model refEntity would draw the renderer's invalid-model fallback
	// (an RGB coordinate axis), so skip the scene-entity add and run only the
	// dlight below. Real-model explosions always have hModel > 0, so this guard
	// is true for them and their behaviour is unchanged.
	if ( ent->hModel || ent->customShader ) {
		trap_R_AddRefEntityToScene(ent);
	}

	// add the dlight
	if ( ex->light ) {
		float		light;

		light = (float)( cg.time - ex->startTime ) / ( ex->endTime - ex->startTime );
		if ( light < 0.5 ) {
			light = 1.0;
		} else {
			light = 1.0 - ( light - 0.5 ) * 2;
		}
		light = ex->light * light;
		trap_R_AddLightToScene(ent->origin, light, ex->lightColor[0], ex->lightColor[1], ex->lightColor[2] );
	}
}

/*
================
CG_AddSpriteExplosion
================
*/
static void CG_AddSpriteExplosion( localEntity_t *le ) {
	refEntity_t	re;
	float c;

	re = le->refEntity;

	c = ( le->endTime - cg.time ) / ( float ) ( le->endTime - le->startTime );
	if ( c > 1 ) {
		c = 1.0;	// can happen during connection problems
	}

	re.shaderRGBA[0] = 0xff;
	re.shaderRGBA[1] = 0xff;
	re.shaderRGBA[2] = 0xff;
	re.shaderRGBA[3] = 0xff * c * 0.33;

	re.reType = RT_SPRITE;
	re.radius = 42 * ( 1.0 - c ) + 30;

	trap_R_AddRefEntityToScene( &re );

	// add the dlight
	if ( le->light ) {
		float		light;

		light = (float)( cg.time - le->startTime ) / ( le->endTime - le->startTime );
		if ( light < 0.5 ) {
			light = 1.0;
		} else {
			light = 1.0 - ( light - 0.5 ) * 2;
		}
		light = le->light * light;
		trap_R_AddLightToScene(re.origin, light, le->lightColor[0], le->lightColor[1], le->lightColor[2] );
	}
}


/*
====================
CG_AddKamikaze
====================
*/
void CG_AddKamikaze( localEntity_t *le ) {
	refEntity_t	*re;
	refEntity_t shockwave;
	float		c;
	vec3_t		test, axis[3];
	int			t;

	re = &le->refEntity;

	t = cg.time - le->startTime;
	VectorClear( test );
	AnglesToAxis( test, axis );

	if (t > KAMI_SHOCKWAVE_STARTTIME && t < KAMI_SHOCKWAVE_ENDTIME) {

		if (!(le->leFlags & LEF_SOUND1)) {
//			trap_S_StartSound (re->origin, ENTITYNUM_WORLD, CHAN_AUTO, cgs.media.kamikazeExplodeSound );
			trap_S_StartLocalSound(cgs.media.kamikazeExplodeSound, CHAN_AUTO);
			le->leFlags |= LEF_SOUND1;
		}
		// 1st kamikaze shockwave
		memset(&shockwave, 0, sizeof(shockwave));
		shockwave.hModel = cgs.media.kamikazeShockWave;
		shockwave.reType = RT_MODEL;
		shockwave.shaderTime = re->shaderTime;
		VectorCopy(re->origin, shockwave.origin);

		c = (float)(t - KAMI_SHOCKWAVE_STARTTIME) / (float)(KAMI_SHOCKWAVE_ENDTIME - KAMI_SHOCKWAVE_STARTTIME);
		VectorScale( axis[0], c * KAMI_SHOCKWAVE_MAXRADIUS / KAMI_SHOCKWAVEMODEL_RADIUS, shockwave.axis[0] );
		VectorScale( axis[1], c * KAMI_SHOCKWAVE_MAXRADIUS / KAMI_SHOCKWAVEMODEL_RADIUS, shockwave.axis[1] );
		VectorScale( axis[2], c * KAMI_SHOCKWAVE_MAXRADIUS / KAMI_SHOCKWAVEMODEL_RADIUS, shockwave.axis[2] );
		shockwave.nonNormalizedAxes = qtrue;

		if (t > KAMI_SHOCKWAVEFADE_STARTTIME) {
			c = (float)(t - KAMI_SHOCKWAVEFADE_STARTTIME) / (float)(KAMI_SHOCKWAVE_ENDTIME - KAMI_SHOCKWAVEFADE_STARTTIME);
		}
		else {
			c = 0;
		}
		c *= 0xff;
		shockwave.shaderRGBA[0] = 0xff - c;
		shockwave.shaderRGBA[1] = 0xff - c;
		shockwave.shaderRGBA[2] = 0xff - c;
		shockwave.shaderRGBA[3] = 0xff - c;

		trap_R_AddRefEntityToScene( &shockwave );
	}

	if (t > KAMI_EXPLODE_STARTTIME && t < KAMI_IMPLODE_ENDTIME) {
		// explosion and implosion
		c = ( le->endTime - cg.time ) * le->lifeRate;
		c *= 0xff;
		re->shaderRGBA[0] = le->color[0] * c;
		re->shaderRGBA[1] = le->color[1] * c;
		re->shaderRGBA[2] = le->color[2] * c;
		re->shaderRGBA[3] = le->color[3] * c;

		if( t < KAMI_IMPLODE_STARTTIME ) {
			c = (float)(t - KAMI_EXPLODE_STARTTIME) / (float)(KAMI_IMPLODE_STARTTIME - KAMI_EXPLODE_STARTTIME);
		}
		else {
			if (!(le->leFlags & LEF_SOUND2)) {
//				trap_S_StartSound (re->origin, ENTITYNUM_WORLD, CHAN_AUTO, cgs.media.kamikazeImplodeSound );
				trap_S_StartLocalSound(cgs.media.kamikazeImplodeSound, CHAN_AUTO);
				le->leFlags |= LEF_SOUND2;
			}
			c = (float)(KAMI_IMPLODE_ENDTIME - t) / (float) (KAMI_IMPLODE_ENDTIME - KAMI_IMPLODE_STARTTIME);
		}
		VectorScale( axis[0], c * KAMI_BOOMSPHERE_MAXRADIUS / KAMI_BOOMSPHEREMODEL_RADIUS, re->axis[0] );
		VectorScale( axis[1], c * KAMI_BOOMSPHERE_MAXRADIUS / KAMI_BOOMSPHEREMODEL_RADIUS, re->axis[1] );
		VectorScale( axis[2], c * KAMI_BOOMSPHERE_MAXRADIUS / KAMI_BOOMSPHEREMODEL_RADIUS, re->axis[2] );
		re->nonNormalizedAxes = qtrue;

		trap_R_AddRefEntityToScene( re );
		// add the dlight
		trap_R_AddLightToScene( re->origin, c * 1000.0, 1.0, 1.0, c );
	}

	if (t > KAMI_SHOCKWAVE2_STARTTIME && t < KAMI_SHOCKWAVE2_ENDTIME) {
		// 2nd kamikaze shockwave
		if (le->angles.trBase[0] == 0 &&
			le->angles.trBase[1] == 0 &&
			le->angles.trBase[2] == 0) {
			le->angles.trBase[0] = random() * 360;
			le->angles.trBase[1] = random() * 360;
			le->angles.trBase[2] = random() * 360;
		}
		memset(&shockwave, 0, sizeof(shockwave));
		shockwave.hModel = cgs.media.kamikazeShockWave;
		shockwave.reType = RT_MODEL;
		shockwave.shaderTime = re->shaderTime;
		VectorCopy(re->origin, shockwave.origin);

		test[0] = le->angles.trBase[0];
		test[1] = le->angles.trBase[1];
		test[2] = le->angles.trBase[2];
		AnglesToAxis( test, axis );

		c = (float)(t - KAMI_SHOCKWAVE2_STARTTIME) / (float)(KAMI_SHOCKWAVE2_ENDTIME - KAMI_SHOCKWAVE2_STARTTIME);
		VectorScale( axis[0], c * KAMI_SHOCKWAVE2_MAXRADIUS / KAMI_SHOCKWAVEMODEL_RADIUS, shockwave.axis[0] );
		VectorScale( axis[1], c * KAMI_SHOCKWAVE2_MAXRADIUS / KAMI_SHOCKWAVEMODEL_RADIUS, shockwave.axis[1] );
		VectorScale( axis[2], c * KAMI_SHOCKWAVE2_MAXRADIUS / KAMI_SHOCKWAVEMODEL_RADIUS, shockwave.axis[2] );
		shockwave.nonNormalizedAxes = qtrue;

		if (t > KAMI_SHOCKWAVE2FADE_STARTTIME) {
			c = (float)(t - KAMI_SHOCKWAVE2FADE_STARTTIME) / (float)(KAMI_SHOCKWAVE2_ENDTIME - KAMI_SHOCKWAVE2FADE_STARTTIME);
		}
		else {
			c = 0;
		}
		c *= 0xff;
		shockwave.shaderRGBA[0] = 0xff - c;
		shockwave.shaderRGBA[1] = 0xff - c;
		shockwave.shaderRGBA[2] = 0xff - c;
		shockwave.shaderRGBA[3] = 0xff - c;

		trap_R_AddRefEntityToScene( &shockwave );
	}
}

/*
===================
CG_AddDeflectorImpact
===================
*/
void CG_AddDeflectorImpact( localEntity_t *le ) {
	trap_R_AddRefEntityToScene( &le->refEntity );
}

/*
===================
CG_AddDeflectorJuiced
===================
*/
void CG_AddDeflectorJuiced( localEntity_t *le ) {
	int t = cg.time - le->startTime;
	if ( t > 3000 ) {
		le->refEntity.axis[0][0] = (float) 1.0 + 0.3 * (t - 3000) / 2000;
		le->refEntity.axis[1][1] = (float) 1.0 + 0.3 * (t - 3000) / 2000;
		le->refEntity.axis[2][2] = (float) 0.7 + 0.3 * (2000 - (t - 3000)) / 2000;
	}
	if ( t > 5000 ) {
		vec3_t	angles;

		le->endTime = 0;
		// no meaningful body orientation here; launch upright from the entity.
		// No damage direction here → NULL/0 reverts to the omnidirectional launch.
		VectorClear( angles );
		CG_GibPlayer( le->refEntity.origin, angles, le->pos.trDelta, NULL, NULL, 0 );
	}
	else {
		trap_R_AddRefEntityToScene( &le->refEntity );
	}
}

/*
===================
CG_AddRefEntity
===================
*/
void CG_AddRefEntity( localEntity_t *le ) {
	if (le->endTime < cg.time) {
		CG_FreeLocalEntity( le );
		return;
	}
	trap_R_AddRefEntityToScene( &le->refEntity );
}

// WA-2b: score + damage plums are staged as world-anchored MARKERS (Wired UI
// draws them via the markerlist element bound to "markers.plums") instead of the
// retired imperative cg_plumOverlays[] / CG_DrawPlumOverlays / trap_R_DrawTextNorm
// 2D path. The marker list is Begun once per frame in CG_AddLocalEntities, each
// live plum is Pushed at its real-pixel screen position (CG_WorldToScreenPixels),
// and Flushed in the bridge. Spawn/trajectory/lifetime/damage are unchanged.

/*
===================
CG_AddScorePlum
===================
*/
void CG_AddScorePlum( localEntity_t *le ) {
	vec3_t		origin, delta, dir, vec, up = {0, 0, 1};
	float		c, len;
	int			score;

	c = ( le->endTime - cg.time ) * le->lifeRate;

	VectorCopy( le->pos.trBase, origin );
	origin[2] += 110 - c * 100;

	VectorSubtract( cg.refdef.vieworg, origin, dir );
	CrossProduct( dir, up, vec );
	VectorNormalize( vec );
	VectorMA( origin, -10 + 20 * sin( c * 2 * M_PI ), vec, origin );

	VectorSubtract( origin, cg.refdef.vieworg, delta );
	len = VectorLength( delta );
	if ( len < 20 ) {
		CG_FreeLocalEntity( le );
		return;
	}

	score = (int)le->radius;

#if FEAT_WIRED_UI
	/* WA-2b: stage as a world-anchored marker at its REAL-pixel screen position
	 * (reproject the world `origin` via the WA-1 helper — NOT the old 640x480
	 * virtual coord). Behind camera → skip. Animated alpha (c) staged per frame;
	 * multi-color by score band; text fits 24. */
	{
		float xPx, yPx;
		if ( CG_WorldToScreenPixels( origin, &xPx, &yPx ) ) {
			vec4_t col;
			char   txt[24];
			col[3] = ( c < 0.25f ) ? c * 4.0f : 1.0f;
			if ( score < 0 ) {
				col[0] = 1.0f; col[1] = 0.067f; col[2] = 0.067f;
				Com_sprintf( txt, sizeof( txt ), "%d score", score );
			} else {
				if ( score >= 50 ) {
					col[0] = 1.0f; col[1] = 0.0f; col[2] = 1.0f;
				} else if ( score >= 20 ) {
					col[0] = 0.0f; col[1] = 0.0f; col[2] = 1.0f;
				} else if ( score >= 10 ) {
					col[0] = 1.0f; col[1] = 1.0f; col[2] = 0.0f;
				} else if ( score >= 2 ) {
					col[0] = 0.0f; col[1] = 1.0f; col[2] = 0.0f;
				} else {
					col[0] = col[1] = col[2] = 1.0f;
				}
				Com_sprintf( txt, sizeof( txt ), "+%d score", score );
			}
			WUI_StageMarkers_Push( xPx, yPx, col, txt );
		}
	}
#else
	(void)score;
#endif
}

#if FEAT_DAMAGE_PLUMS
/*
===================
CG_AddDamagePlum
Floating damage number (red), shown only to the attacker. (2A)
===================
*/
void CG_AddDamagePlum( localEntity_t *le ) {
	vec3_t		origin, delta, dir, vec, up = {0, 0, 1};
	float		c, len;
	int			dmg;

	c = ( le->endTime - cg.time ) * le->lifeRate;

	BG_EvaluateTrajectory( &le->pos, cg.time, origin );
	origin[2] += 40 - c * 40;

	VectorSubtract( cg.refdef.vieworg, origin, dir );
	CrossProduct( dir, up, vec );
	VectorNormalize( vec );
	VectorMA( origin, -5 + 10 * sin( c * 2 * M_PI ), vec, origin );

	VectorSubtract( origin, cg.refdef.vieworg, delta );
	len = VectorLength( delta );
	if ( len < 20 ) {
		CG_FreeLocalEntity( le );
		return;
	}

	dmg = (int)le->radius;
	if ( dmg <= 0 ) dmg = 1;

#if FEAT_WIRED_UI
	/* WA-2b: stage as a world-anchored marker at its REAL-pixel screen position
	 * (reproject the world `origin` via the WA-1 helper). Behind camera → skip.
	 * Red, animated alpha (c) staged per frame; "%d" damage text. */
	{
		float xPx, yPx;
		if ( CG_WorldToScreenPixels( origin, &xPx, &yPx ) ) {
			vec4_t col = { 1.0f, 0.2f, 0.2f, ( c < 0.25f ) ? c * 4.0f : 1.0f };
			char   txt[24];
			Com_sprintf( txt, sizeof( txt ), "%d", dmg );
			WUI_StageMarkers_Push( xPx, yPx, col, txt );
		}
	}
#else
	(void)dmg;
#endif
}
#endif

//==============================================================================

/*
===================
CG_AddLocalEntities

===================
*/
void CG_AddLocalEntities( void ) {
	localEntity_t	*le, *next;

#if FEAT_WIRED_UI
	/* WA-2b: begin the per-frame "markers.plums" list ONCE, before the entity
	 * loop, so the score/damage plums visited below (CG_AddScorePlum /
	 * CG_AddDamagePlum) rebuild a fresh list each frame (frame-transient — the
	 * bridge Flush sends it, an empty list clears the prior frame). Begin resets
	 * the scratch, so it MUST be once-per-pass and NOT inside the per-plum path.
	 * NOTE: CG_RenderCameraView (cg_main.c) re-runs CG_AddLocalEntities in the
	 * same frame for its secondary camera viewport; that pass re-Begins and
	 * re-Pushes the identical plums (same cg.time → same alpha/text, same global
	 * cg.refdef → same screen pixels), so the final flushed list is unchanged.
	 * This Begin-reset is strictly safer than the retired cg_plumOverlays[] buffer
	 * (which the second pass would have double-appended). */
	WUI_StageMarkers_Begin( "markers.plums" );
#endif

	// walk the list backwards, so any new local entities generated
	// (trails, marks, etc) will be present this frame
	le = cg_activeLocalEntities.prev;
	for ( ; le != &cg_activeLocalEntities ; le = next ) {
		// grab next now, so if the local entity is freed we
		// still have it
		next = le->prev;

		if ( cg.time >= le->endTime ) {
			CG_FreeLocalEntity( le );
			continue;
		}
		switch ( le->leType ) {
		default:
			Com_Terminate( TERM_CLIENT_DROP, "Bad leType: %i", le->leType );
			break;

		case LE_MARK:
			break;

		case LE_SPRITE_EXPLOSION:
			CG_AddSpriteExplosion( le );
			break;

		case LE_EXPLOSION:
			CG_AddExplosion( le );
			break;

		case LE_FRAGMENT:			// gibs and brass
			CG_AddFragment( le );
			break;

		case LE_MOVE_SCALE_FADE:		// water bubbles
			CG_AddMoveScaleFade( le );
			break;

		case LE_FADE_RGB:				// teleporters, railtrails
			CG_AddFadeRGB( le );
			break;

		case LE_FALL_SCALE_FADE: // gib blood trails
			CG_AddFallScaleFade( le );
			break;

		case LE_SCALE_FADE:		// rocket trails
			CG_AddScaleFade( le );
			break;

		case LE_SCOREPLUM:
			CG_AddScorePlum( le );
			break;

#if FEAT_DAMAGE_PLUMS
		case LE_DAMAGEPLUM:
			CG_AddDamagePlum( le );
			break;
#endif
#if FEAT_PING_LOCATION
		case LE_PING_LOCATION:
		{
			float c = ( le->endTime - cg.time ) * le->lifeRate;
			refEntity_t *re = &le->refEntity;

			// pulsing size
			re->radius = 8 + 4 * sin( cg.time * 0.005f );

			// team color, fade out in last 25%
			re->shaderRGBA[0] = (byte)( le->color[0] * 0xff );
			re->shaderRGBA[1] = (byte)( le->color[1] * 0xff );
			re->shaderRGBA[2] = (byte)( le->color[2] * 0xff );
			re->shaderRGBA[3] = ( c < 0.25f ) ? (byte)( 0xff * 4 * c ) : 0xff;

			VectorCopy( le->pos.trBase, re->origin );
			trap_R_AddRefEntityToScene( re );
			break;
		}
#endif

		case LE_KAMIKAZE:
			CG_AddKamikaze( le );
			break;
		case LE_DEFLECTOR_IMPACT:
			CG_AddDeflectorImpact( le );
			break;
		case LE_DEFLECTOR_JUICED:
			CG_AddDeflectorJuiced( le );
			break;
		case LE_SHOWREFENTITY:
			CG_AddRefEntity( le );
			break;
		}
	}

#if FEAT_WIRED_UI
	/* WA-2b/WA-3: TWO complete marker cycles, each fully Begin/Push/Flushed before
	 * the next, because the staging scratch holds ONE list at a time (WA-2a). The
	 * plum cycle's Pushes happened in the LE loop above (Begin at the top); flush
	 * it now. Then the bot-directive cycle (its own listKey). Flushing here in the
	 * scene-build (not the bridge) keeps each cycle atomic and still populates the
	 * client store before the HUD draw. Each is flushed even when empty so a
	 * vanished plum / cleared directive clears the prior frame's list. */
	WUI_StageMarkers_Flush();			/* push "markers.plums" (Begun at the top) */
	CG_StageBotDirectives();			/* Begin "markers.botdir" + Push + Flush */
#endif
}
