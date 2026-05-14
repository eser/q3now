// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 1999-2005 Id Software, Inc.
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
// cg_effects.c -- these functions generate localentities, usually as a result
// of event processing

#include "cg_local.h"
#include "../qcommon/wired/render/primitives.h"
#include "../qcommon/wired/render/traps.h"
/* Phase 5: log channels */
LOG_DECLARE_CHANNEL( ch_cgame, "cgame" );


/*
==================
CG_BubbleTrail

Bullets shot underwater
==================
*/
void CG_BubbleTrail( vec3_t start, vec3_t end, float spacing ) {
	vec3_t		move;
	vec3_t		vec;
	float		len;
	int			i;

	if ( cg_noProjectileTrail.integer ) {
		return;
	}

	VectorCopy (start, move);
	VectorSubtract (end, start, vec);
	len = VectorNormalize (vec);

	// advance a random amount first
	i = rand() % (int)spacing;
	VectorMA( move, i, vec, move );

	VectorScale (vec, spacing, vec);

	for ( ; i < len; i += spacing ) {
		localEntity_t	*le;
		refEntity_t		*re;

		le = CG_AllocLocalEntity();
		le->leFlags = LEF_PUFF_DONT_SCALE;
		le->leType = LE_MOVE_SCALE_FADE;
		le->startTime = cg.time;
		le->endTime = cg.time + 1000 + random() * 250;
		le->lifeRate = 1.0 / ( le->endTime - le->startTime );

		re = &le->refEntity;
		re->shaderTime.f =cg.time / 1000.0f;

		re->reType = RT_SPRITE;
		re->rotation = 0;
		re->radius = 3;
		re->customShader = cgs.media.waterBubbleShader;
		re->shader.rgba[0] = 0xff;
		re->shader.rgba[1] = 0xff;
		re->shader.rgba[2] = 0xff;
		re->shader.rgba[3] = 0xff;

		le->color[3] = 1.0;

		le->pos.trType = TR_LINEAR;
		le->pos.trTime = cg.time;
		VectorCopy( move, le->pos.trBase );
		le->pos.trDelta[0] = crandom()*5;
		le->pos.trDelta[1] = crandom()*5;
		le->pos.trDelta[2] = crandom()*5 + 6;

		VectorAdd (move, vec, move);
	}
}

/*
==================
CG_WaterSplash  (Phase 6.5.3)

A short burst of spray droplets + the water-hit sound at a point on a
liquid surface. Quake 3 ships no dedicated splash sprite, so we re-use
cgs.media.waterBubbleShader at a larger radius and launch the droplets on
a gravity arc (up then fall) — reads as a small crown of water. The sound
is Q1's misc/h2ohit1 (or Q3's player/watr_in fallback; silent if neither
asset is present). Pure helper: callers gate on cgs.q1Map.
==================
*/
void CG_WaterSplash( vec3_t point ) {
	int n;

	if ( cg_noProjectileTrail.integer ) {
		return;
	}

	for ( n = 0; n < 6; n++ ) {
		localEntity_t	*le;
		refEntity_t		*re;

		le = CG_AllocLocalEntity();
		le->leFlags   = LEF_PUFF_DONT_SCALE;
		le->leType    = LE_MOVE_SCALE_FADE;
		le->startTime = cg.time;
		le->endTime   = cg.time + 300 + random() * 200;
		le->lifeRate  = 1.0f / ( le->endTime - le->startTime );

		re = &le->refEntity;
		re->shaderTime.f = cg.time / 1000.0f;
		re->reType       = RT_SPRITE;
		re->rotation     = 0;
		re->radius       = 5.0f + random() * 3.0f;	// bigger than a trail bubble
		re->customShader = cgs.media.waterBubbleShader;
		re->shader.rgba[0] = re->shader.rgba[1] = re->shader.rgba[2] = re->shader.rgba[3] = 0xff;
		le->color[3] = 1.0f;

		le->pos.trType = TR_GRAVITY;			// droplets arc up then fall back
		le->pos.trTime = cg.time;
		VectorCopy( point, le->pos.trBase );
		le->pos.trDelta[0] = crandom() * 40.0f;
		le->pos.trDelta[1] = crandom() * 40.0f;
		le->pos.trDelta[2] = 50.0f + random() * 50.0f;
	}

	if ( cgs.media.waterSplashSound ) {
		trap_S_StartSound( point, ENTITYNUM_WORLD, CHAN_AUTO, cgs.media.waterSplashSound );
	}
}

/*
==================
CG_WaterCrossingSplashes  (Phase 6.5.3)

Given a hitscan segment [start,end], emit CG_WaterSplash() at wherever
the shot pierces a liquid surface — once at the entry point and, when the
shot ends in air on the far side, once at the exit point. Recovers the
surface intersection with a CONTENTS_WATER-only re-trace against the world
(Q1 maps carry synthesized AABB water brushes with CONTENTS_WATER; Q3 maps
have water/fog brushes) — the same recovery pattern CG_Bullet /
CG_ShotgunPellet already use for the submerged bubble trail. No-op on Q3
maps (cgs.q1Map gate) so Q3 visuals are unchanged.
==================
*/
void CG_WaterCrossingSplashes( vec3_t start, vec3_t end ) {
	int		sc, ec;
	trace_t	wtr;

	if ( !cgs.q1Map ) {
		return;
	}

	sc = CG_PointContents( start, 0 );
	ec = CG_PointContents( end,   0 );

	if ( ( sc & CONTENTS_WATER ) && ( ec & CONTENTS_WATER ) ) {
		return;	// fully submerged — no surface crossing
	}
	if ( sc & CONTENTS_WATER ) {
		// shooter underwater, shot ends in air: one exit crossing
		trap_CM_BoxTrace( &wtr, end, start, NULL, NULL, 0, CONTENTS_WATER );
		CG_WaterSplash( wtr.endpos );
		return;
	}
	if ( ec & CONTENTS_WATER ) {
		// shot starts in air, ends underwater: one entry crossing
		trap_CM_BoxTrace( &wtr, start, end, NULL, NULL, 0, CONTENTS_WATER );
		CG_WaterSplash( wtr.endpos );
		return;
	}
	// air -> (water volume) -> air: entry at the forward re-trace, exit at the reverse one
	trap_CM_BoxTrace( &wtr, start, end, NULL, NULL, 0, CONTENTS_WATER );
	if ( wtr.fraction >= 1.0f ) {
		return;	// the segment never touched a water volume
	}
	CG_WaterSplash( wtr.endpos );							// entry
	trap_CM_BoxTrace( &wtr, end, start, NULL, NULL, 0, CONTENTS_WATER );
	CG_WaterSplash( wtr.endpos );							// exit
}

/*
=====================
CG_SmokePuff

Adds a smoke puff or blood trail localEntity.
=====================
*/
localEntity_t *CG_SmokePuff( const vec3_t p, const vec3_t vel,
				   float radius,
				   float r, float g, float b, float a,
				   float duration,
				   int startTime,
				   int fadeInTime,
				   int leFlags,
				   qhandle_t hShader ) {
	static int	seed = 0x92;
	localEntity_t	*le;
	refEntity_t		*re;
//	int fadeInTime = startTime + duration / 2;

	le = CG_AllocLocalEntity();
	le->leFlags = leFlags;
	le->radius = radius;

	re = &le->refEntity;
	re->rotation = Q_random( &seed ) * 360;
	re->radius = radius;
	re->shaderTime.f =startTime / 1000.0f;

	le->leType = LE_MOVE_SCALE_FADE;
	le->startTime = startTime;
	le->fadeInTime = fadeInTime;
	le->endTime = startTime + duration;
	if ( fadeInTime > startTime ) {
		le->lifeRate = 1.0 / ( le->endTime - le->fadeInTime );
	}
	else {
		le->lifeRate = 1.0 / ( le->endTime - le->startTime );
	}
	le->color[0] = r;
	le->color[1] = g;
	le->color[2] = b;
	le->color[3] = a;


	le->pos.trType = TR_LINEAR;
	le->pos.trTime = startTime;
	VectorCopy( vel, le->pos.trDelta );
	VectorCopy( p, le->pos.trBase );

	VectorCopy( p, re->origin );
	re->customShader = hShader;

	// rage pro can't alpha fade, so use a different shader
	if ( cgs.glconfig.hardwareType == GLHW_RAGEPRO ) {
		re->customShader = cgs.media.smokePuffRageProShader;
		re->shader.rgba[0] = 0xff;
		re->shader.rgba[1] = 0xff;
		re->shader.rgba[2] = 0xff;
		re->shader.rgba[3] = 0xff;
	} else {
		re->shader.rgba[0] = le->color[0] * 0xff;
		re->shader.rgba[1] = le->color[1] * 0xff;
		re->shader.rgba[2] = le->color[2] * 0xff;
		re->shader.rgba[3] = 0xff;
	}

	re->reType = RT_SPRITE;
	re->radius = le->radius;

	return le;
}

/*
==================
CG_SpawnEffect

Player teleporting in or out
==================
*/
void CG_SpawnEffect( vec3_t org ) {
	localEntity_t	*le;
	refEntity_t		*re;

	le = CG_AllocLocalEntity();
	le->leFlags = 0;
	le->leType = LE_FADE_RGB;
	le->startTime = cg.time;
	le->endTime = cg.time + 1200; // 500
	le->lifeRate = 1.0 / ( le->endTime - le->startTime );

	le->color[0] = le->color[1] = le->color[2] = le->color[3] = 1.0;

	re = &le->refEntity;

	re->reType = RT_MODEL;
	re->shaderTime.f =cg.time / 1000.0f;

	re->customShader = cgs.media.teleportEffectShader;
	re->hModel = cgs.media.teleportEffectModel;
	AxisClear( re->axis );

	VectorCopy( org, re->origin );
	re->origin[2] -= 24;
}

// eser - lightning discharge
/*
====================
CG_Lightning_Discharge
====================
*/
void CG_Lightning_Discharge (vec3_t origin, int msec)
{
    localEntity_t		*le;

    if (msec <= 0) Com_Terminate( TERM_CLIENT_DROP,"CG_Lightning_Discharge: msec = %i", msec);

    le = CG_SmokePuff (	origin,			// where
        vec3_origin,			// where to
        ((48 + (msec * 10)) / 16),	// radius
        1, 1, 1, 1,			// RGBA color shift
        300 + msec,			// duration
        cg.time,			// start when?
        0,					// fade in time
        0,				// flags (?)
        trap_R_RegisterShader ("models/weaphits/electric.tga"));

    le->leType = LE_SCALE_FADE;
}
// eser - lightning discharge

/*
===============
CG_LightningBoltBeam
===============
*/
void CG_LightningBoltBeam( vec3_t start, vec3_t end ) {
	beamDesc_t bd;
	memset( &bd, 0, sizeof( bd ) );
	VectorCopy( start, bd.start );
	VectorCopy( end, bd.end );
	bd.startWidth     = 8.0f;
	bd.endWidth       = 8.0f;
	bd.startColor[0] = bd.startColor[1] = bd.startColor[2] = bd.startColor[3] = 1.0f;
	bd.endColor[0]   = bd.endColor[1]   = bd.endColor[2]   = bd.endColor[3]   = 1.0f;
	bd.shader         = cgs.media.lightningShaderPrim;
	bd.duration       = 0.050f;        // 50 ms persistent
	bd.fadeOut        = 0.020f;
	bd.axialCopies    = 4;
	bd.startEntityNum = -1;
	bd.endEntityNum   = -1;
	bd.uvScroll[0]    = 0.0f;          // shader handles scroll
	bd.uvScroll[1]    = 0.0f;
	trap_R_AddBeamToScene( &bd );
}

/*
=================
CG_LightningArcBeam
Renders the chain arc beam from primary impact point to secondary target.
Uses a dedicated shader for visual distinction from the primary beam.
=================
*/
void CG_LightningArcBeam( vec3_t start, vec3_t end ) {
	beamDesc_t bd;
	memset( &bd, 0, sizeof( bd ) );
	VectorCopy( start, bd.start );
	VectorCopy( end, bd.end );
	bd.startWidth     = 4.0f;          // honors cgame intent (engine ignored beam->radius before)
	bd.endWidth       = 4.0f;
	bd.startColor[0] = bd.startColor[1] = bd.startColor[2] = bd.startColor[3] = 1.0f;
	bd.endColor[0]   = bd.endColor[1]   = bd.endColor[2]   = bd.endColor[3]   = 1.0f;
	bd.shader         = cgs.media.lightningArcShaderPrim;
	bd.duration       = 0.050f;
	bd.fadeOut        = 0.020f;
	bd.axialCopies    = 2;             // thinner crackle (legacy was 4 forced by engine)
	bd.startEntityNum = -1;
	bd.endEntityNum   = -1;
	bd.uvScroll[0]    = 0.0f;          // shader handles scroll
	bd.uvScroll[1]    = 0.0f;
	trap_R_AddBeamToScene( &bd );
}


// ── Phase 5T: generic player-trail infrastructure ─────────────────
//
// Multiple trail types per player can be active concurrently; each
// (client, type) pair has its own expiry timestamp. Per-frame
// transient re-submit: cgame recomputes the anchor and player origin
// every frame and submits a fresh beamDesc with the freshly-faded
// alpha. The engine sees only generic primitive shader / particle
// handles; effect-specific naming (PTRAIL_PUSH, "pushTrail") lives
// only here and in shader.script.
//
// State: cg_playerTrails[client][type] holds an expiry timestamp in
// cg.time. > cg.time → active this frame. Per-type fade window
// (def->fadeWindowMs) controls the linear ramp-down; outside the
// window alpha is alphaCeiling × 1.0.
//
// Anchor: behind the player along the negative velocity vector, at
// PTRAIL_ANCHOR_DISTANCE. Below PTRAIL_SPEED_THRESHOLD the trail is
// suppressed regardless of expiry (still moving but barely).

// Trail anchor: distance behind the player along the negative
// velocity vector. Trail beam runs anchor → player. Shared across
// all trail types — geometry, not visual.
#define PTRAIL_ANCHOR_DISTANCE         48.0f

// Vertical offset applied to both endpoints so the beam centers on
// the player body silhouette rather than sitting at feet-level.
// Q3 player bbox is ~56u tall; +24 lands near the chest.
#define PTRAIL_VERTICAL_OFFSET         24.0f

// Movement threshold. Below this speed (u/s) no trail renders even
// if expiry is in the future.
#define PTRAIL_SPEED_THRESHOLD         50.0f

// Per-type extension durations used by
// CG_UpdatePlayerTrailExtensions per-frame. Picked > frame budget
// at any sane FPS so a transiently-skipped frame doesn't pop.
#define PTRAIL_HASTE_EXTEND_MS         200
// Future: PTRAIL_FLAG_EXTEND_MS, PTRAIL_SKULLS_EXTEND_MS

// PTRAIL_PUSH def-table values.
#define PTRAIL_PUSH_ALPHA_CEILING      0.25f
#define PTRAIL_PUSH_START_WIDTH       28.0f   // wide enough to cover body silhouette
#define PTRAIL_PUSH_END_WIDTH         28.0f   // uniform — vertical coverage matters more than taper
#define PTRAIL_PUSH_AXIAL_COPIES       2
#define PTRAIL_PUSH_FADE_WINDOW_MS     200
#define PTRAIL_PUSH_PARTICLE_RATE_MIN  30.0f
#define PTRAIL_PUSH_PARTICLE_RATE_MAX  120.0f

static const vec3_t PTRAIL_PUSH_DEFAULT_COLOR = { 0.9f, 0.95f, 1.0f };

// Per-(client, type) expiry timestamps. BSS-zero-init: all trails
// start inactive. Public — third-party hooks may write directly.
int cg_playerTrails[MAX_CLIENTS][PTRAIL_COUNT];

// Per-type visual / behavioural parameters. Populated once by
// CG_RegisterPlayerTrailDefs after cgs.media.* handles are bound.
static playerTrailDef_t cg_playerTrailDefs[PTRAIL_COUNT];

void CG_RegisterPlayerTrailDefs( void )
{
	playerTrailDef_t *def;

	memset( cg_playerTrailDefs, 0, sizeof( cg_playerTrailDefs ) );

	// PTRAIL_PUSH: jumppad + haste/speed.
	def = &cg_playerTrailDefs[PTRAIL_PUSH];
	def->shaderPtrPtr        = &cgs.media.pushTrailShader;
	def->particleClassPtrPtr = &cgs.media.pushStreamClass;
	VectorCopy( PTRAIL_PUSH_DEFAULT_COLOR, def->defaultColor );
	def->colorResolveFn      = NULL;   // use defaultColor
	def->alphaCeiling        = PTRAIL_PUSH_ALPHA_CEILING;
	def->startWidth          = PTRAIL_PUSH_START_WIDTH;
	def->endWidth            = PTRAIL_PUSH_END_WIDTH;
	def->axialCopies         = PTRAIL_PUSH_AXIAL_COPIES;
	def->fadeWindowMs        = PTRAIL_PUSH_FADE_WINDOW_MS;
	def->particleRateMin     = PTRAIL_PUSH_PARTICLE_RATE_MIN;
	def->particleRateMax     = PTRAIL_PUSH_PARTICLE_RATE_MAX;

	// Future PTRAIL_FLAG / PTRAIL_SKULLS def entries land here.
}

void CG_TriggerPlayerTrail( int clientNum,
                            playerTrailType_t type,
                            int durationMs )
{
	int newExpiry;

	if ( clientNum < 0 || clientNum >= MAX_CLIENTS ) return;
	if ( type < 0 || type >= PTRAIL_COUNT ) return;
	if ( durationMs <= 0 ) return;

	newExpiry = cg.time + durationMs;
	if ( newExpiry > cg_playerTrails[clientNum][type] ) {
		cg_playerTrails[clientNum][type] = newExpiry;
	}
}

// Per-frame condition checks: scan all clients, bump the matching
// trail-type expiry while the condition is active. PTRAIL_PUSH is
// extended while the player is haste-buffed; future PTRAIL_FLAG /
// PTRAIL_SKULLS extensions slot in here.
static void CG_UpdatePlayerTrailExtensions( void )
{
	int i;

	for ( i = 0; i < MAX_CLIENTS; i++ ) {
		const playerState_t *ps;
		const centity_t     *cent;
		qboolean             hasHaste;

		// Resolve player-state source: predicted for local,
		// snapped entity state for remote.
		if ( i == cg.predictedPlayerState.clientNum ) {
			ps   = &cg.predictedPlayerState;
			cent = NULL;
		} else {
			cent = &cg_entities[i];
			if ( !cent->currentValid ) continue;
			ps = NULL;
		}

		hasHaste = qfalse;

		if ( ps != NULL ) {
			// Local: ps->powerups[PW_HASTE] holds expiry timestamp;
			// > cg.time means active.
			if ( ps->powerups[PW_HASTE] > cg.time ) hasHaste = qtrue;
		} else if ( cent != NULL ) {
			// Remote: bitmask in cent->currentState.powerups.
			// Pattern matches existing usage in cg_players.c and
			// cg_znudge.c.
			if ( cent->currentState.powerups & ( 1 << PW_HASTE ) ) {
				hasHaste = qtrue;
			}
		}

		if ( hasHaste ) {
			CG_TriggerPlayerTrail( i, PTRAIL_PUSH, PTRAIL_HASTE_EXTEND_MS );
		}

		// Future: PTRAIL_FLAG / PTRAIL_SKULLS extension checks land
		// here. Each trail type's "active condition" check + a
		// CG_TriggerPlayerTrail call.
	}
}

// Emit particles flowing anchor → player along the beam axis.
// Frame-rate-independent count via cg.frametime; per-frame rate
// scales linearly between def->particleRateMin and
// def->particleRateMax based on speed (clamped at
// PTRAIL_JUMPPAD_SPEED_NORM).
static void CG_EmitPlayerTrailParticles(
	const vec3_t start, const vec3_t end,
	const vec3_t color, float alphaScale, float speed,
	const playerTrailDef_t *def, qhandle_t particleClass )
{
	float         particlesPerSecond;
	emitterDesc_t emitter;
	vec3_t        axis;
	float         length;
	float         frameTime;
	int           count;
	float         t;

	VectorSubtract( end, start, axis );
	length = VectorLength( axis );
	if ( length < 1.0f ) return;

	t = speed / PTRAIL_JUMPPAD_SPEED_NORM;
	if ( t > 1.0f ) t = 1.0f;
	if ( t < 0.0f ) t = 0.0f;
	particlesPerSecond = def->particleRateMin +
		t * ( def->particleRateMax - def->particleRateMin );

	frameTime = (float)cg.frametime / 1000.0f;
	count     = (int)( particlesPerSecond * frameTime + 0.5f );
	if ( count <= 0 ) return;

	memset( &emitter, 0, sizeof( emitter ) );
	emitter.cls   = particleClass;
	emitter.count = count;
	VectorCopy( start, emitter.origin );
	VectorCopy( end,   emitter.end );  // EMIT_PATH samples uniformly start..end
	VectorScale( axis, 1.0f / length, emitter.axis );
	emitter.colorTint[0] = color[0];
	emitter.colorTint[1] = color[1];
	emitter.colorTint[2] = color[2];
	emitter.colorTint[3] = alphaScale;
	trap_R_EmitParticles( &emitter );
}

// Generic per-(client, type) render. Reads the type's def for
// visual params; computes anchor + alpha + color; submits the beam
// + particle emit.
static void CG_RenderOnePlayerTrail( int clientNum,
                                     playerTrailType_t type )
{
	const playerTrailDef_t *def;
	vec3_t                  playerOrigin;
	vec3_t                  playerVelocity;
	float                   speed;
	vec3_t                  velNorm;
	vec3_t                  anchor;
	int                     remaining;
	float                   fadeAlpha;
	vec3_t                  color;
	qhandle_t               shader;
	qhandle_t               particleClass;
	beamDesc_t              bd;

	def = &cg_playerTrailDefs[type];

	// Late-bound handle resolution: defs hold pointer-to-handle so
	// registration order between the def table and the underlying
	// cgs.media.* slot doesn't matter.
	shader        = def->shaderPtrPtr        ? *def->shaderPtrPtr        : 0;
	particleClass = def->particleClassPtrPtr ? *def->particleClassPtrPtr : 0;
	if ( shader == 0 ) return;   // unregistered type — silent skip

	// Position + velocity: predicted for local, lerp / trDelta for
	// remote.
	if ( clientNum == cg.predictedPlayerState.clientNum ) {
		VectorCopy( cg.predictedPlayerState.origin,   playerOrigin );
		VectorCopy( cg.predictedPlayerState.velocity, playerVelocity );
	} else {
		const centity_t *cent = &cg_entities[clientNum];
		if ( !cent->currentValid ) return;
		VectorCopy( cent->lerpOrigin,                 playerOrigin );
		VectorCopy( cent->currentState.pos.trDelta,   playerVelocity );
	}

	speed = VectorLength( playerVelocity );
	if ( speed < PTRAIL_SPEED_THRESHOLD ) return;

	VectorScale( playerVelocity, 1.0f / speed, velNorm );
	VectorMA( playerOrigin, -PTRAIL_ANCHOR_DISTANCE, velNorm, anchor );

	// Lift both endpoints to body-center height so the beam covers
	// the player silhouette instead of skimming the feet plane.
	anchor[2]       += PTRAIL_VERTICAL_OFFSET;
	playerOrigin[2] += PTRAIL_VERTICAL_OFFSET;

	remaining = cg_playerTrails[clientNum][type] - cg.time;
	if ( remaining <= 0 ) return;
	if ( def->fadeWindowMs > 0 && remaining < def->fadeWindowMs ) {
		fadeAlpha = (float)remaining / (float)def->fadeWindowMs;
	} else {
		fadeAlpha = 1.0f;
	}
	fadeAlpha *= def->alphaCeiling;
	if ( fadeAlpha <= 0.0f ) return;

	if ( def->colorResolveFn ) {
		def->colorResolveFn( clientNum, color );
	} else {
		VectorCopy( def->defaultColor, color );
	}

	memset( &bd, 0, sizeof( bd ) );
	VectorCopy( anchor,       bd.start );
	VectorCopy( playerOrigin, bd.end );
	bd.startWidth     = def->startWidth;
	bd.endWidth       = def->endWidth;
	bd.startColor[0]  = color[0];
	bd.startColor[1]  = color[1];
	bd.startColor[2]  = color[2];
	bd.startColor[3]  = 0.0f;       // anchor end fully faded — rearward ghost-out
	bd.endColor[0]    = color[0];
	bd.endColor[1]    = color[1];
	bd.endColor[2]    = color[2];
	bd.endColor[3]    = fadeAlpha;  // player end carries the full time-faded alpha
	bd.shader         = shader;
	bd.duration       = 0.0f;             // transient — re-submitted each frame
	bd.axialCopies    = def->axialCopies;
	bd.startEntityNum = -1;
	bd.endEntityNum   = -1;
	bd.uvScroll[0]    = 0.0f;             // shader.script tcMod scroll handles animation
	bd.uvScroll[1]    = 0.0f;
	trap_R_AddBeamToScene( &bd );

	if ( particleClass != 0 ) {
		CG_EmitPlayerTrailParticles( anchor, playerOrigin,
		                             color, fadeAlpha, speed,
		                             def, particleClass );
	}
}

void CG_AddPlayerTrails( void )
{
	int clientNum;
	int type;

	CG_UpdatePlayerTrailExtensions();

	for ( clientNum = 0; clientNum < MAX_CLIENTS; clientNum++ ) {
		for ( type = 0; type < PTRAIL_COUNT; type++ ) {
			if ( cg_playerTrails[clientNum][type] <= cg.time ) continue;
			CG_RenderOnePlayerTrail( clientNum,
			                         (playerTrailType_t)type );
		}
	}
}


/*
==================
CG_KamikazeEffect
==================
*/
void CG_KamikazeEffect( vec3_t org ) {
	localEntity_t	*le;
	refEntity_t		*re;

	le = CG_AllocLocalEntity();
	le->leFlags = 0;
	le->leType = LE_KAMIKAZE;
	le->startTime = cg.time;
	le->endTime = cg.time + 3000;//2250;
	le->lifeRate = 1.0 / ( le->endTime - le->startTime );

	le->color[0] = le->color[1] = le->color[2] = le->color[3] = 1.0;

	VectorClear(le->angles.trBase);

	re = &le->refEntity;

	re->reType = RT_MODEL;
	re->shaderTime.f =cg.time / 1000.0f;

	re->hModel = cgs.media.kamikazeEffectModel;

	VectorCopy( org, re->origin );

}

#if FEAT_OVERLOAD
/*
==================
CG_ObeliskExplode
==================
*/
void CG_ObeliskExplode( vec3_t org, int entityNum ) {
	localEntity_t	*le;
	vec3_t origin;

	// create an explosion
	VectorCopy( org, origin );
	origin[2] += 64;
	le = CG_MakeExplosion( origin, vec3_origin,
						   cgs.media.dishFlashModel,
						   cgs.media.rocketExplosionShader,
						   600, qtrue );
	le->light = 300;
	le->lightColor[0] = 1;
	le->lightColor[1] = 0.75;
	le->lightColor[2] = 0.0;
}

/*
==================
CG_ObeliskPain
==================
*/
void CG_ObeliskPain( vec3_t org ) {
	float r;
	sfxHandle_t sfx;

	// hit sound
	r = rand() & 3;
	if ( r < 2 ) {
		sfx = cgs.media.obeliskHitSound1;
	} else if ( r == 2 ) {
		sfx = cgs.media.obeliskHitSound2;
	} else {
		sfx = cgs.media.obeliskHitSound3;
	}
	trap_S_StartSound ( org, ENTITYNUM_NONE, CHAN_BODY, sfx );
}
#endif

/*
==================
CG_DeflectorImpact
==================
*/
void CG_DeflectorImpact( vec3_t org, vec3_t angles ) {
	localEntity_t	*le;
	refEntity_t		*re;
	int				r;
	sfxHandle_t		sfx;

	le = CG_AllocLocalEntity();
	le->leFlags = 0;
	le->leType = LE_DEFLECTOR_IMPACT;
	le->startTime = cg.time;
	le->endTime = cg.time + 1000;
	le->lifeRate = 1.0 / ( le->endTime - le->startTime );

	le->color[0] = le->color[1] = le->color[2] = le->color[3] = 1.0;

	re = &le->refEntity;

	re->reType = RT_MODEL;
	re->shaderTime.f =cg.time / 1000.0f;

	re->hModel = cgs.media.deflectorImpactModel;

	VectorCopy( org, re->origin );
	AnglesToAxis( angles, re->axis );

	r = rand() & 3;
	if ( r < 2 ) {
		sfx = cgs.media.deflectorImpactSound1;
	} else if ( r == 2 ) {
		sfx = cgs.media.deflectorImpactSound2;
	} else {
		sfx = cgs.media.deflectorImpactSound3;
	}
	trap_S_StartSound (org, ENTITYNUM_NONE, CHAN_BODY, sfx );
}

/*
==================
CG_DeflectorJuiced
==================
*/
void CG_DeflectorJuiced( vec3_t org ) {
	localEntity_t	*le;
	refEntity_t		*re;
	vec3_t			angles;

	le = CG_AllocLocalEntity();
	le->leFlags = 0;
	le->leType = LE_DEFLECTOR_JUICED;
	le->startTime = cg.time;
	le->endTime = cg.time + 10000;
	le->lifeRate = 1.0 / ( le->endTime - le->startTime );

	le->color[0] = le->color[1] = le->color[2] = le->color[3] = 1.0;

	re = &le->refEntity;

	re->reType = RT_MODEL;
	re->shaderTime.f =cg.time / 1000.0f;

	re->hModel = cgs.media.deflectorJuicedModel;

	VectorCopy( org, re->origin );
	VectorClear(angles);
	AnglesToAxis( angles, re->axis );

	trap_S_StartSound (org, ENTITYNUM_NONE, CHAN_BODY, cgs.media.deflectorJuicedSound );
}

// cg_scorePlums bitmask
#define SCOREPLUMS_SCORES	1	// bit 0: show score plums
#define SCOREPLUMS_DAMAGES	2	// bit 1: show damage plums

/*
==================
CG_ScorePlum
==================
*/
void CG_ScorePlum( int client, vec3_t org, int score ) {
	localEntity_t	*le;
	refEntity_t		*re;
	vec3_t			angles;
	static vec3_t lastPos;

	// only visualize for the client that scored
	if (client != cg.predictedPlayerState.clientNum || !(cg_scorePlums.integer & SCOREPLUMS_SCORES)) {
		return;
	}

	le = CG_AllocLocalEntity();
	le->leFlags = 0;
	le->leType = LE_SCOREPLUM;
	le->startTime = cg.time;
	le->endTime = cg.time + 4000;
	le->lifeRate = 1.0 / ( le->endTime - le->startTime );


	le->color[0] = le->color[1] = le->color[2] = le->color[3] = 1.0;
	le->radius = score;

	VectorCopy( org, le->pos.trBase );
	if (org[2] >= lastPos[2] - 20 && org[2] <= lastPos[2] + 20) {
		le->pos.trBase[2] -= 20;
	}

	//Com_Log( SEV_INFO, LOG_CH(ch_cgame), "Plum origin %i %i %i -- %i\n", (int)org[0], (int)org[1], (int)org[2], (int)Distance(org, lastPos));
	VectorCopy(org, lastPos);


	re = &le->refEntity;

	re->reType = RT_SPRITE;
	re->radius = 16;

	VectorClear(angles);
	AnglesToAxis( angles, re->axis );
}

#if FEAT_DAMAGE_PLUMS
/*
==================
CG_DamagePlum
Shows a floating damage number at the target's position, visible only
to the attacker who dealt the damage. (2A)
==================
*/
void CG_DamagePlum( int client, vec3_t org, int damage ) {
	localEntity_t	*le;
	refEntity_t		*re;
	vec3_t			angles;
	static vec3_t	lastPos;

	// only show to the client who dealt the damage
	if ( client != cg.predictedPlayerState.clientNum || !(cg_scorePlums.integer & SCOREPLUMS_DAMAGES) ) {
		return;
	}

	le = CG_AllocLocalEntity();
	le->leFlags = 0;
	le->leType = LE_DAMAGEPLUM;
	le->startTime = cg.time;
	le->endTime = cg.time + 1000;
	le->lifeRate = 1.0f / ( le->endTime - le->startTime );

	le->color[0] = 1.0f;	// red tint for damage
	le->color[1] = 0.3f;
	le->color[2] = 0.3f;
	le->color[3] = 1.0f;
	le->radius = damage;

	VectorCopy( org, le->pos.trBase );
	if ( org[2] >= lastPos[2] - 20 && org[2] <= lastPos[2] + 20 ) {
		le->pos.trBase[2] -= 20;
	}
	VectorCopy( org, lastPos );

	// slight random drift
	le->pos.trType = TR_LINEAR;
	le->pos.trDelta[0] = 2.0f * crandom();
	le->pos.trDelta[1] = 2.0f * crandom();
	le->pos.trDelta[2] = 12.0f;
	le->pos.trTime = cg.time;

	re = &le->refEntity;
	re->reType = RT_SPRITE;
	re->radius = 11;

	VectorClear( angles );
	AnglesToAxis( angles, re->axis );
}
#endif

#if FEAT_PING_LOCATION
/*
==================
CG_PingLocation
Team ping marker — pulsing diamond at the pinged world position. (4G)
==================
*/
void CG_PingLocation( centity_t *cent ) {
	localEntity_t	*le;
	refEntity_t		*re;
	vec3_t			angles;
	int				team;

	// only show to teammates
	team = cgs.clientinfo[cent->currentState.otherEntityNum].team;
	if ( team != cg.snap->ps.persistant[PERS_TEAM] ) {
		return;
	}

	le = CG_AllocLocalEntity();
	le->leFlags = 0;
	le->leType = LE_PING_LOCATION;
	le->startTime = cg.time;
	le->endTime = cg.time + 5000;
	le->lifeRate = 1.0f / ( le->endTime - le->startTime );

	// team-colored: blue for blue team, red for red team
	if ( team == TEAM_BLUE ) {
		le->color[0] = 0.2f; le->color[1] = 0.5f; le->color[2] = 1.0f;
	} else {
		le->color[0] = 1.0f; le->color[1] = 0.3f; le->color[2] = 0.2f;
	}
	le->color[3] = 1.0f;

	le->pos.trType = TR_STATIONARY;
	le->pos.trTime = cg.time;
	VectorCopy( cent->lerpOrigin, le->pos.trBase );

	re = &le->refEntity;
	re->reType = RT_SPRITE;
	re->radius = 8;
	re->customShader = cgs.media.crosshairBurstShader;

	VectorClear( angles );
	AnglesToAxis( angles, re->axis );

	trap_S_StartSound( cent->lerpOrigin, ENTITYNUM_WORLD, CHAN_AUTO,
		cgs.media.teleInSound );
}
#endif


/*
====================
CG_MakeExplosion
====================
*/
localEntity_t *CG_MakeExplosion( vec3_t origin, vec3_t dir,
								qhandle_t hModel, qhandle_t shader,
								int msec, qboolean isSprite ) {
	float			ang;
	localEntity_t	*ex;
	int				offset;
	vec3_t			tmpVec, newOrigin;

	if ( msec <= 0 ) {
		Com_Terminate( TERM_CLIENT_DROP, "CG_MakeExplosion: msec = %i", msec );
	}

	// skew the time a bit so they aren't all in sync
	offset = rand() & 63;

	ex = CG_AllocLocalEntity();
	if ( isSprite ) {
		ex->leType = LE_SPRITE_EXPLOSION;

		// randomly rotate sprite orientation
		ex->refEntity.rotation = rand() % 360;
		VectorScale( dir, 16, tmpVec );
		VectorAdd( tmpVec, origin, newOrigin );
	} else {
		ex->leType = LE_EXPLOSION;
		VectorCopy( origin, newOrigin );

		// set axis with random rotate
		if ( !dir ) {
			AxisClear( ex->refEntity.axis );
		} else {
			ang = rand() % 360;
			VectorCopy( dir, ex->refEntity.axis[0] );
			RotateAroundDirection( ex->refEntity.axis, ang );
		}
	}

	ex->startTime = cg.time - offset;
	ex->endTime = ex->startTime + msec;

	// bias the time so all shader effects start correctly
	ex->refEntity.shaderTime.f =ex->startTime / 1000.0f;

	ex->refEntity.hModel = hModel;
	ex->refEntity.customShader = shader;

	// set origin
	VectorCopy( newOrigin, ex->refEntity.origin );
	VectorCopy( newOrigin, ex->refEntity.oldorigin );

	ex->color[0] = ex->color[1] = ex->color[2] = 1.0;

	return ex;
}


/*
=================
CG_Bleed

This is the spurt of blood when a character gets hit
=================
*/
void CG_Bleed( vec3_t origin, int entityNum ) {
	localEntity_t	*ex;

	if ( !cg_blood.integer ) {
		return;
	}

	ex = CG_AllocLocalEntity();
	ex->leType = LE_EXPLOSION;

	ex->startTime = cg.time;
	ex->endTime = ex->startTime + 500;

	VectorCopy ( origin, ex->refEntity.origin);
	ex->refEntity.reType = RT_SPRITE;
	ex->refEntity.rotation = rand() % 360;
	ex->refEntity.radius = 24;

	ex->refEntity.customShader = cgs.media.bloodExplosionShader;

	// don't show player's own blood in view
	if ( entityNum == cg.snap->ps.clientNum ) {
		ex->refEntity.renderfx |= RF_THIRD_PERSON;
	}
}



/*
==================
CG_LaunchGib
==================
*/
void CG_LaunchGib( vec3_t origin, vec3_t velocity, qhandle_t hModel ) {
	localEntity_t	*le;
	refEntity_t		*re;

	le = CG_AllocLocalEntity();
	re = &le->refEntity;

	le->leType = LE_FRAGMENT;
	le->startTime = cg.time;
	le->endTime = le->startTime + 5000 + random() * 3000;

	VectorCopy( origin, re->origin );
	AxisCopy( axisDefault, re->axis );
	re->hModel = hModel;

	le->pos.trType = TR_GRAVITY;
	VectorCopy( origin, le->pos.trBase );
	VectorCopy( velocity, le->pos.trDelta );
	le->pos.trTime = cg.time;

	le->bounceFactor = 0.6f;

	le->leBounceSoundType = LEBS_BLOOD;
	le->leMarkType = LEMT_BLOOD;
}

/*
===================
CG_GibPlayer

Generated a bunch of gibs launching out from the bodies location
===================
*/
void CG_GibPlayer( vec3_t playerOrigin ) {
	vec3_t	origin, velocity;

	if ( !cg_blood.integer ) {
		return;
	}

	VectorCopy( playerOrigin, origin );
	velocity[0] = crandom()*GIB_VELOCITY;
	velocity[1] = crandom()*GIB_VELOCITY;
	velocity[2] = GIB_JUMP + crandom()*GIB_VELOCITY;
	if ( rand() & 1 ) {
		CG_LaunchGib( origin, velocity, cgs.media.gibSkull );
	} else {
		CG_LaunchGib( origin, velocity, cgs.media.gibBrain );
	}

	// allow gibs to be turned off for speed
	if ( !cg_gibs.integer ) {
		return;
	}

	VectorCopy( playerOrigin, origin );
	velocity[0] = crandom()*GIB_VELOCITY;
	velocity[1] = crandom()*GIB_VELOCITY;
	velocity[2] = GIB_JUMP + crandom()*GIB_VELOCITY;
	CG_LaunchGib( origin, velocity, cgs.media.gibAbdomen );

	VectorCopy( playerOrigin, origin );
	velocity[0] = crandom()*GIB_VELOCITY;
	velocity[1] = crandom()*GIB_VELOCITY;
	velocity[2] = GIB_JUMP + crandom()*GIB_VELOCITY;
	CG_LaunchGib( origin, velocity, cgs.media.gibArm );

	VectorCopy( playerOrigin, origin );
	velocity[0] = crandom()*GIB_VELOCITY;
	velocity[1] = crandom()*GIB_VELOCITY;
	velocity[2] = GIB_JUMP + crandom()*GIB_VELOCITY;
	CG_LaunchGib( origin, velocity, cgs.media.gibChest );

	VectorCopy( playerOrigin, origin );
	velocity[0] = crandom()*GIB_VELOCITY;
	velocity[1] = crandom()*GIB_VELOCITY;
	velocity[2] = GIB_JUMP + crandom()*GIB_VELOCITY;
	CG_LaunchGib( origin, velocity, cgs.media.gibFist );

	VectorCopy( playerOrigin, origin );
	velocity[0] = crandom()*GIB_VELOCITY;
	velocity[1] = crandom()*GIB_VELOCITY;
	velocity[2] = GIB_JUMP + crandom()*GIB_VELOCITY;
	CG_LaunchGib( origin, velocity, cgs.media.gibFoot );

	VectorCopy( playerOrigin, origin );
	velocity[0] = crandom()*GIB_VELOCITY;
	velocity[1] = crandom()*GIB_VELOCITY;
	velocity[2] = GIB_JUMP + crandom()*GIB_VELOCITY;
	CG_LaunchGib( origin, velocity, cgs.media.gibForearm );

	VectorCopy( playerOrigin, origin );
	velocity[0] = crandom()*GIB_VELOCITY;
	velocity[1] = crandom()*GIB_VELOCITY;
	velocity[2] = GIB_JUMP + crandom()*GIB_VELOCITY;
	CG_LaunchGib( origin, velocity, cgs.media.gibIntestine );

	VectorCopy( playerOrigin, origin );
	velocity[0] = crandom()*GIB_VELOCITY;
	velocity[1] = crandom()*GIB_VELOCITY;
	velocity[2] = GIB_JUMP + crandom()*GIB_VELOCITY;
	CG_LaunchGib( origin, velocity, cgs.media.gibLeg );

	VectorCopy( playerOrigin, origin );
	velocity[0] = crandom()*GIB_VELOCITY;
	velocity[1] = crandom()*GIB_VELOCITY;
	velocity[2] = GIB_JUMP + crandom()*GIB_VELOCITY;
	CG_LaunchGib( origin, velocity, cgs.media.gibLeg );
}

/*
==================
CG_LaunchExplode
==================
*/
void CG_LaunchExplode( vec3_t origin, vec3_t velocity, qhandle_t hModel ) {
	localEntity_t	*le;
	refEntity_t		*re;

	le = CG_AllocLocalEntity();
	re = &le->refEntity;

	le->leType = LE_FRAGMENT;
	le->startTime = cg.time;
	le->endTime = le->startTime + 10000 + random() * 6000;

	VectorCopy( origin, re->origin );
	AxisCopy( axisDefault, re->axis );
	re->hModel = hModel;

	le->pos.trType = TR_GRAVITY;
	VectorCopy( origin, le->pos.trBase );
	VectorCopy( velocity, le->pos.trDelta );
	le->pos.trTime = cg.time;

	le->bounceFactor = 0.1f;

	le->leBounceSoundType = LEBS_BRASS;
	le->leMarkType = LEMT_NONE;
}

#define	EXP_VELOCITY	100
#define	EXP_JUMP		150
/*
===================
CG_BigExplode

Generated a bunch of gibs launching out from the bodies location
===================
*/
void CG_BigExplode( vec3_t playerOrigin ) {
	vec3_t	origin, velocity;

	if ( !cg_blood.integer ) {
		return;
	}

	VectorCopy( playerOrigin, origin );
	velocity[0] = crandom()*EXP_VELOCITY;
	velocity[1] = crandom()*EXP_VELOCITY;
	velocity[2] = EXP_JUMP + crandom()*EXP_VELOCITY;
	CG_LaunchExplode( origin, velocity, cgs.media.smoke2 );

	VectorCopy( playerOrigin, origin );
	velocity[0] = crandom()*EXP_VELOCITY;
	velocity[1] = crandom()*EXP_VELOCITY;
	velocity[2] = EXP_JUMP + crandom()*EXP_VELOCITY;
	CG_LaunchExplode( origin, velocity, cgs.media.smoke2 );

	VectorCopy( playerOrigin, origin );
	velocity[0] = crandom()*EXP_VELOCITY*1.5;
	velocity[1] = crandom()*EXP_VELOCITY*1.5;
	velocity[2] = EXP_JUMP + crandom()*EXP_VELOCITY;
	CG_LaunchExplode( origin, velocity, cgs.media.smoke2 );

	VectorCopy( playerOrigin, origin );
	velocity[0] = crandom()*EXP_VELOCITY*2.0;
	velocity[1] = crandom()*EXP_VELOCITY*2.0;
	velocity[2] = EXP_JUMP + crandom()*EXP_VELOCITY;
	CG_LaunchExplode( origin, velocity, cgs.media.smoke2 );

	VectorCopy( playerOrigin, origin );
	velocity[0] = crandom()*EXP_VELOCITY*2.5;
	velocity[1] = crandom()*EXP_VELOCITY*2.5;
	velocity[2] = EXP_JUMP + crandom()*EXP_VELOCITY;
	CG_LaunchExplode( origin, velocity, cgs.media.smoke2 );
}

/*
============
CG_LightningSparks
Blue spark shower for lightning gun wall impacts. Called every frame
the LG beam is hitting a surface, so each call emits a small handful;
they accumulate into a continuous shower while firing. Skips water.
Direction is the surface-outward vector (sparks fly away from wall).
============
*/
void CG_LightningSparks( vec3_t origin, vec3_t dir ) {
	int				i, count, j;
	localEntity_t	*le;
	refEntity_t		*re;

	if ( trap_CM_PointContents( origin, 0 ) & CONTENTS_WATER ) {
		return;
	}

	// GPU path. Default. Mirrors the CPU body via the lg_sparks
	// particle class registered in CG_RegisterLightningParticleClasses.
	// emitter.count = 3 matches the CPU loop count below; per-frame
	// caller (CG_LightningBolt's impact branch) drives the steady-
	// state shower by calling once per frame held against a wall.
	if ( !cg_cpuEffects.integer ) {
		emitterDesc_t emitter;
		memset( &emitter, 0, sizeof( emitter ) );
		emitter.cls   = cgs.media.lgSparksClass;
		emitter.count = 3;
		VectorCopy( origin, emitter.origin );
		VectorCopy( dir,    emitter.axis );
		emitter.colorTint[0] = 1.0f;
		emitter.colorTint[1] = 1.0f;
		emitter.colorTint[2] = 1.0f;
		emitter.colorTint[3] = 1.0f;
		trap_R_EmitParticles( &emitter );
		return;
	}

	count = 3;
	for ( i = 0; i < count; i++ ) {
		le = CG_AllocLocalEntity();
		le->leFlags = LEF_PUFF_DONT_SCALE;
		le->leType = LE_MOVE_SCALE_FADE;
		le->startTime = cg.time;
		le->endTime = cg.time + 200 + ( rand() & 0xff );
		le->lifeRate = 1.0f / ( le->endTime - le->startTime );

		re = &le->refEntity;
		re->reType = RT_SPRITE;
		re->rotation = 0;
		re->radius = 1.5f + random() * 1.5f;
		re->customShader = cgs.media.lightningSparkShader;
		// blue tint via rgbGen vertex
		re->shaderRGBA[0] = 0x55;
		re->shaderRGBA[1] = 0x99;
		re->shaderRGBA[2] = 0xff;
		re->shaderRGBA[3] = 0xff;

		le->color[0] = 0x55 / 255.0f;
		le->color[1] = 0x99 / 255.0f;
		le->color[2] = 1.0f;
		le->color[3] = 1.0f;

		le->pos.trType = TR_GRAVITY;
		le->pos.trTime = cg.time;
		VectorCopy( origin, le->pos.trBase );

		// fan around the surface normal, then push along it
		for ( j = 0; j < 3; j++ ) {
			le->pos.trDelta[j] = dir[j] + crandom() * 0.7f;
		}
		VectorNormalize( le->pos.trDelta );
		VectorScale( le->pos.trDelta, 100.0f + random() * 200.0f, le->pos.trDelta );
		le->pos.trDelta[2] += random() * 100.0f;
	}
}

#if FEAT_IMPACT_SPARKS
/*
============
CG_ImpactSparks
Spawn spark particles at a player hit impact point. (11A)
============
*/
void CG_ImpactSparks( vec3_t origin, vec3_t dir ) {
	int				i, count;
	localEntity_t	*le;
	refEntity_t		*re;
	vec3_t			velocity;

	count = 6 + ( random() * 6 );
	for ( i = 0; i < count; i++ ) {
		le = CG_AllocLocalEntity();
		le->leFlags = 0;
		le->leType = LE_MOVE_SCALE_FADE;
		le->startTime = cg.time;
		le->endTime = cg.time + 200 + random() * 250;
		le->lifeRate = 1.0 / ( le->endTime - le->startTime );

		re = &le->refEntity;
		re->reType = RT_SPRITE;
		re->rotation = 0;
		re->radius = 0.5 + random() * 1.5;
		re->customShader = cgs.media.whiteShader;
		re->shaderRGBA[0] = 0xff;
		re->shaderRGBA[1] = 0xcc;
		re->shaderRGBA[2] = 0x44;
		re->shaderRGBA[3] = 0xff;

		le->color[3] = 1.0;

		le->pos.trType = TR_GRAVITY;
		le->pos.trTime = cg.time;
		VectorCopy( origin, le->pos.trBase );

		velocity[0] = dir[0] * 100 + crandom() * 150;
		velocity[1] = dir[1] * 100 + crandom() * 150;
		velocity[2] = dir[2] * 100 + crandom() * 150 + 50;
		VectorCopy( velocity, le->pos.trDelta );
	}
}
#endif

#if FEAT_ENV_LIGHTS
/*
===================
CG_AddEnvironmentLights

Emit colored dynamic lights from nearby lava, slime, and
teleporter surfaces — KEX-style colored environment lighting.

Traces downward and outward from the player to detect hazard
surfaces, then places a soft colored dlight at the hit point.
===================
*/
#define ENV_LIGHT_TRACE_DIST	512.0f
#define ENV_LIGHT_MAX			6		// max simultaneous env dlights

typedef struct {
	vec3_t	dir;			// trace direction (relative to player)
	float	dist;			// trace distance
} envLightProbe_t;

static const envLightProbe_t envProbes[] = {
	{ {  0,    0,   -1 }, 256 },	// straight down
	{ {  1,    0,   -1 }, 384 },	// forward-down
	{ { -1,    0,   -1 }, 384 },	// back-down
	{ {  0,    1,   -1 }, 384 },	// right-down
	{ {  0,   -1,   -1 }, 384 },	// left-down
	{ {  0.7f, 0.7f, -0.5f }, 384 },	// diagonal
};

void CG_AddEnvironmentLights( void ) {
	trace_t		tr;
	vec3_t		start, end, dir;
	int			i, contents, count;

	if ( !cg_envLights.integer ) {
		return;
	}

	VectorCopy( cg.refdef.vieworg, start );
	count = 0;

	for ( i = 0; i < ARRAY_LEN( envProbes ) && count < ENV_LIGHT_MAX; i++ ) {
		// build trace direction
		VectorCopy( envProbes[i].dir, dir );
		VectorNormalize( dir );
		VectorMA( start, envProbes[i].dist, dir, end );

		CG_Trace( &tr, start, NULL, NULL, end, cg.predictedPlayerState.clientNum, MASK_WATER );

		if ( tr.fraction >= 1.0f ) {
			continue;
		}

		// check what we hit
		contents = CG_PointContents( tr.endpos, -1 );

		if ( contents & CONTENTS_LAVA ) {
			// warm orange glow — like KEX remaster
			float dist = Distance( start, tr.endpos );
			float intensity = 200.0f * ( 1.0f - dist / envProbes[i].dist );
			if ( intensity > 20.0f ) {
				trap_R_AddLightToScene( tr.endpos, intensity, 1.0f, 0.5f, 0.1f );
				count++;
			}
		} else if ( contents & CONTENTS_SLIME ) {
			// sickly green glow
			float dist = Distance( start, tr.endpos );
			float intensity = 150.0f * ( 1.0f - dist / envProbes[i].dist );
			if ( intensity > 20.0f ) {
				trap_R_AddLightToScene( tr.endpos, intensity, 0.2f, 0.8f, 0.1f );
				count++;
			}
		} else if ( contents & CONTENTS_TELEPORTER ) {
			// cool blue-white shimmer
			float dist = Distance( start, tr.endpos );
			float intensity = 120.0f * ( 1.0f - dist / envProbes[i].dist );
			if ( intensity > 20.0f ) {
				trap_R_AddLightToScene( tr.endpos, intensity, 0.4f, 0.6f, 1.0f );
				count++;
			}
		} else if ( contents & CONTENTS_WATER ) {
			// subtle blue tint for water
			float dist = Distance( start, tr.endpos );
			float intensity = 80.0f * ( 1.0f - dist / envProbes[i].dist );
			if ( intensity > 15.0f ) {
				trap_R_AddLightToScene( tr.endpos, intensity, 0.3f, 0.5f, 0.8f );
				count++;
			}
		}
	}
}
#endif // FEAT_ENV_LIGHTS
