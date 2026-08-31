// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
===========================================================================
cg_wired_particles.c — wired-render particle class registry (cgame side)

Cgame-side store of particleClass_t recipes. Each registered class gets
a dense slot index plus a 1-based handle. The class definitions are
mirrored to the renderer via trap_R_RegisterParticleClass at registration
time so the renderer's compute shader can consume them later.

The legacy CPU particle system in cg_particles.c (Rafael smoke/blood
trails) is unrelated to this file and untouched by phase 1.

The registry is intentionally small (MAX_PARTICLE_CLASSES = 64) and
uses a linear scan for name lookup — registration is rare (init time)
and lookup is expected to happen once per consumer at startup.

Names are registry-only metadata. They never enter particleClass_t,
they never cross the trap boundary, and the renderer never sees them.
===========================================================================
*/

#include "cg_local.h"
#include "../../qcommon/wired/render/particle_class.h"
#include "../../qcommon/wired/render/traps.h"

// Slot 0 is reserved as INVALID_PARTICLE_CLASS, so slot 0 of these
// arrays is left unused; valid slots are [1..cg_numParticleClasses].
// Equivalently: slot index = handle - 1 once we offset by 1.
#define CG_PARTICLE_NAME_MAX 64

static particleClass_t cg_particleClasses[MAX_PARTICLE_CLASSES];
static char			   cg_particleClassNames[MAX_PARTICLE_CLASSES][CG_PARTICLE_NAME_MAX];
static int			   cg_numParticleClasses;

void CG_ResetParticleClassRegistry( void )
{
	memset( cg_particleClasses, 0, sizeof( cg_particleClasses ) );
	memset( cg_particleClassNames, 0, sizeof( cg_particleClassNames ) );
	cg_numParticleClasses = 0;
}

particleClassHandle_t CG_RegisterParticleClass( const char *name, const particleClass_t *cls )
{
	int slot;

	if ( name == NULL || cls == NULL )
		return INVALID_PARTICLE_CLASS;
	if ( name[0] == '\0' )
		return INVALID_PARTICLE_CLASS;
	if ( cg_numParticleClasses >= MAX_PARTICLE_CLASSES )
		return INVALID_PARTICLE_CLASS;

	// Duplicate-name guard. Returns 0 on duplicate per the doc-comment
	// in particle_class.h ("Returns ... 0 on failure (... duplicate
	// name ...)") — explicitly NOT returning the existing handle.
	if ( CG_FindParticleClass( name ) != INVALID_PARTICLE_CLASS )
		return INVALID_PARTICLE_CLASS;

	slot = cg_numParticleClasses;
	memcpy( &cg_particleClasses[slot], cls, sizeof( *cls ) );
	Q_strncpyz( cg_particleClassNames[slot], name, CG_PARTICLE_NAME_MAX );
	cg_numParticleClasses++;

	{
		// Handles are 1-based so 0 stays as INVALID_PARTICLE_CLASS.
		particleClassHandle_t handle = (particleClassHandle_t)( slot + 1 );

		// Mirror to renderer's shadow registry. The renderer keeps a
		// host-side copy for its compute shader; this trap is the
		// only path by which it learns about a class.
		trap_R_RegisterParticleClassNamed( handle, &cg_particleClasses[slot], name );

		return handle;
	}
}

particleClassHandle_t CG_FindParticleClass( const char *name )
{
	int i;

	if ( name == NULL || name[0] == '\0' )
		return INVALID_PARTICLE_CLASS;

	for ( i = 0; i < cg_numParticleClasses; i++ ) {
		if ( !strcmp( cg_particleClassNames[i], name ) )
			return (particleClassHandle_t)( i + 1 );
	}
	return INVALID_PARTICLE_CLASS;
}

const particleClass_t *CG_GetParticleClass( particleClassHandle_t handle )
{
	if ( handle <= 0 || handle > cg_numParticleClasses )
		return NULL;
	return &cg_particleClasses[handle - 1];
}

/*
==========================
CG_RegisterRailParticleClasses

Register the two particle classes used by the rail trail's debris and
impact sparks. Called once from CG_RegisterWeapon's WP_RAILGUN case
after the rail shaders bind, so cgs.media.railRingsShader and
cgs.media.whiteShader are valid by the time we read them.

Class parameters mirror the legacy CPU loops in CG_RailTrail (spawn
time) and CG_AddRailTrails (per-frame). Each value below is annotated
against the CPU source it derives from.
==========================
*/
void CG_RegisterRailParticleClasses( void )
{
	particleClass_t cls;

	// ── rail_debris ────────────────────────────────────────────────
	// Q2-spirit grey debris emitted along the beam path. Mirrors the
	// CPU debris loop (cg_weapons.c CG_RailTrail, ~line 322-358 +
	// CG_AddRailTrails ~line 506-533):
	//   emit along trail->start..trail->end every 7.5 units → EMIT_PATH
	//   uniform sampling on the path (CPU determinism vs GPU random
	//   uniform are visually equivalent at this density)
	//   spawn scatter: crand()*3 per axis → SCATTER_CUBE, mag 3.0
	//   velocity: crand()*3 per axis (NO axial component, despite
	//     the surrounding "drift" terminology) → VEL_PURE_CUBE,
	//     jitter 3.0
	//   lifetime: alpha = 1 - frac over RAIL_TRAILTIME = 1500 ms
	//     → lifetimeMean = 1.5s, no jitter
	//   shader: cgs.media.railRingsShader (stock Q3 "railDisc",
	//     additive)
	//   color: per-particle grey 255 - (rand()&15)*8 ∈ [135..255].
	//     The class system supports a 16-entry palette, but the
	//     phase 3 vertex shader uses palette index 0 only as a
	//     deliberate simplification. Mid-grey 0.78 is the centre of
	//     the CPU range; per-particle variance is lost in the port.
	//   colorEndMult: alpha → 0 over lifetime (CPU fades alpha
	//     directly via the trail's frac).
	//   sizeStart/sizeEnd = 0.5 (CPU CG_BuildBillboardQuad radius;
	//     CPU does not shrink debris).
	//   gravityScale = 0.0: CPU code does NOT apply gravity to
	//     debris despite the misleading "with gravity drift"
	//     comment — render-time integration is purely
	//     pos = debrisOrg + elapsed * debrisDelta. This is verified
	//     parity, not a regression.
	//   drag = 0.0: CPU velocities are constant.
	memset( &cls, 0, sizeof( cls ) );
	cls.shader			 = cgs.media.railRingsShader;
	cls.renderFlags		 = PRIM_FLAG_ADDITIVE;
	cls.emitMode		 = EMIT_PATH;
	cls.scatterShape	 = SCATTER_CUBE;
	cls.scatterMagnitude = 3.0f;
	cls.velocityShape	 = VEL_PURE_CUBE;
	cls.axialSpeed		 = 0.0f;
	cls.cubeJitter		 = 3.0f;
	cls.coneHalfAngle	 = 0.0f;
	cls.lifetimeMean	 = 1.5f;
	cls.lifetimeJitter	 = 0.0f;
	// 16-step grey ramp matching CPU's per-particle
	// randomness. CPU original: 255 - (rand() & 15) * 8 →
	// [255, 247, 239, ..., 143, 135] (16 distinct values).
	// Normalize to float [0.529, 1.0]; each step = 8/255 ≈ 0.0314.
	// RE_EmitParticles assigns each particle a random index in
	// [0, paletteCount), particle.vert samples
	// colorPalette[paletteIndex] per particle.
	cls.paletteCount = 16;
	for ( int i = 0; i < 16; i++ ) {
		float grey			   = ( 255.0f - i * 8.0f ) / 255.0f;
		cls.colorPalette[i][0] = grey;
		cls.colorPalette[i][1] = grey;
		cls.colorPalette[i][2] = grey;
		cls.colorPalette[i][3] = 1.0f;
	}
	cls.colorEndMult[0] = 1.0f;
	cls.colorEndMult[1] = 1.0f;
	cls.colorEndMult[2] = 1.0f;
	cls.colorEndMult[3] = 0.0f;
	cls.sizeStart		= 0.5f;
	cls.sizeEnd			= 0.5f;
	cls.gravityScale	= 0.0f;
	cls.drag			= 0.0f;

	cgs.media.railDebrisClass = (qhandle_t)CG_RegisterParticleClass( "rail_debris", &cls );

	// ── rail_sparks ────────────────────────────────────────────────
	// Warm-white embers from the impact point. Mirrors the CPU sparks
	// loop (CG_RailTrail ~line 360-374 + CG_AddRailTrails ~line 535-563):
	//   emit AT trail->end (point, no path) → EMIT_POINT, scatter NONE
	//   velocity: impactNormal*80 + crand()*40 per axis
	//     → VEL_AXIAL_PLUS_CUBE, axialSpeed 80, cubeJitter 40
	//     (axis is supplied at emit time as desc->axis = impactNormal)
	//   lifetime: visible only while frac < 0.2 → 0.2 * RAIL_TRAILTIME
	//     = 0.3s, no jitter
	//   shader: cgs.media.whiteShader. R_FindShader's LIGHTMAP_2D
	//     auto-default gives blendFunc SRC_ALPHA / ONE_MINUS_SRC_ALPHA
	//     (verified via tr_shader.c R_CreateDefaultShading), so
	//     renderFlags = 0 (alpha blend, NOT additive).
	//   color: CPU (255, 255, 220) warm white → (1, 1, 220/255, 1).
	//   colorEndMult: alpha → 0 over lifetime.
	//   sizeStart/sizeEnd = 0.3 (CPU CG_BuildBillboardQuad radius).
	//   gravityScale = 0.5: CPU applies pos[2] -= 0.5 * 400 * t²
	//     (g = 400 units/s²); compute shader's WORLD_GRAVITY = 800
	//     (q3 cg_gravity default), so 400/800 = 0.5.
	memset( &cls, 0, sizeof( cls ) );
	cls.shader			   = cgs.media.whiteShader;
	cls.renderFlags		   = 0;
	cls.emitMode		   = EMIT_POINT;
	cls.scatterShape	   = SCATTER_NONE;
	cls.scatterMagnitude   = 0.0f;
	cls.velocityShape	   = VEL_AXIAL_PLUS_CUBE;
	cls.axialSpeed		   = 80.0f;
	cls.cubeJitter		   = 40.0f;
	cls.coneHalfAngle	   = 0.0f;
	cls.lifetimeMean	   = 0.3f;
	cls.lifetimeJitter	   = 0.0f;
	cls.paletteCount	   = 1;
	cls.colorPalette[0][0] = 1.0f;
	cls.colorPalette[0][1] = 1.0f;
	cls.colorPalette[0][2] = 220.0f / 255.0f;
	cls.colorPalette[0][3] = 1.0f;
	cls.colorEndMult[0]	   = 1.0f;
	cls.colorEndMult[1]	   = 1.0f;
	cls.colorEndMult[2]	   = 1.0f;
	cls.colorEndMult[3]	   = 0.0f;
	cls.sizeStart		   = 0.3f;
	cls.sizeEnd			   = 0.3f;
	cls.gravityScale	   = 0.5f;
	cls.drag			   = 0.0f;

	cgs.media.railSparksClass = (qhandle_t)CG_RegisterParticleClass( "rail_sparks", &cls );
}

/*
==========================
CG_RegisterLightningParticleClasses

Register the particle class used by Lightning Gun primary impact
sparks. Called once from CG_RegisterWeapon's WP_LIGHTNING_GUN case;
cgs.media.lightningSparkShader is bound earlier in CG_RegisterGraphics
(cg_main.c), so it is valid by the time this runs.

Class parameters mirror the legacy CPU body of CG_LightningSparks
(cg_effects.c:881-927). Each value below is annotated against the
CPU source it derives from. The expressivity-extension fields
(speedJitter, velocityBias, velocityBiasJitter, sizeJitter) carry
the parts of CPU behaviour that the four base velocity shapes
cannot express:
   speedJitter            ← random speed magnitude (CPU's
                            "(100 + random()*200)" scaling).
   velocityBias[2]        ← asymmetric upward kick midpoint
                            (CPU's "vel.z += random()*100").
   velocityBiasJitter[2]  ← asymmetric upward kick halfwidth.
   sizeJitter             ← per-spark radius scatter (CPU's
                            "1.5 + random()*1.5").
==========================
*/
void CG_RegisterLightningParticleClasses( void )
{
	particleClass_t cls;

	// ── lg_sparks ────────────────────────────────────────────────
	// Blue spark shower at LG primary impact. Mirrors CPU body
	// CG_LightningSparks at cg_effects.c:881-927. Per-frame caller
	// emits 3 particles (CPU loop count = 3, line 890); the GPU
	// path mirrors this via emitter.count = 3 at the call site.
	memset( &cls, 0, sizeof( cls ) );

	cls.shader = cgs.media.lightningSparkShader;
	// Informational per phase 5; renderer derives blend from the
	// shader's stages[0]→stateBits at registration time.
	cls.renderFlags = PRIM_FLAG_ADDITIVE;

	// Emit at impact origin, no spawn-position scatter.
	cls.emitMode		 = EMIT_POINT;
	cls.scatterShape	 = SCATTER_NONE;
	cls.scatterMagnitude = 0.0f;

	// Velocity. CPU formula (cg_effects.c:920-925):
	//   v = surfaceNormal + crand()*0.7  per axis  (pre-normalize)
	//   v = normalize(v)
	//   v *= 100 + random()*200                    (uniform speed [100, 300])
	//   v.z += random()*100                        (asymmetric +Z kick [0, 100])
	//
	// Mapped to expressivity-extended class:
	//   - VEL_CONE with axialSpeed=200 + speedJitter=100 produces
	//     uniform speed in [100, 300] (axialSpeed + crand()*100).
	//   - coneHalfAngle ≈ atan(0.7) ≈ 0.611 rad models the
	//     direction fan post-normalize; CPU's pre-normalize cube
	//     perturbation produces a slightly non-uniform distribution
	//     within this cone (clustering toward axes), but the
	//     visual is close.
	//   - velocityBias[2] = 50, velocityBiasJitter[2] = 50 yields
	//     vel.z += 50 + crand()*50 = uniform [0, 100], matching
	//     CPU's "vel.z += random()*100".
	cls.velocityShape		  = VEL_CONE;
	cls.axialSpeed			  = 200.0f;
	cls.speedJitter			  = 100.0f;
	cls.coneHalfAngle		  = 0.611f;
	cls.cubeJitter			  = 0.0f;
	cls.velocityBias[0]		  = 0.0f;
	cls.velocityBias[1]		  = 0.0f;
	cls.velocityBias[2]		  = 50.0f;
	cls.velocityBiasJitter[0] = 0.0f;
	cls.velocityBiasJitter[1] = 0.0f;
	cls.velocityBiasJitter[2] = 50.0f;

	// Lifetime. CPU: cg.time + 200 + (rand() & 0xff) ms = uniform
	// [200, 455] ms. Renderer's mean ± jitter produces uniform
	// [mean - jitter, mean + jitter]. Match: mean = 327.5 ms,
	// jitter = 127.5 ms (in seconds).
	cls.lifetimeMean   = 0.3275f;
	cls.lifetimeJitter = 0.1275f;

	// Color. CPU shaderRGBA = (0x55, 0x99, 0xff, 0xff)
	//                      = (85/255, 153/255, 255/255, 1.0).
	// 0x99/255 written as 153.0f/255.0f to keep the integer source
	// visible at code-review time.
	cls.paletteCount	   = 1;
	cls.colorPalette[0][0] = 85.0f / 255.0f;
	cls.colorPalette[0][1] = 153.0f / 255.0f;
	cls.colorPalette[0][2] = 1.0f;
	cls.colorPalette[0][3] = 1.0f;

	// Alpha → 0 over lifetime. CPU LE_MOVE_SCALE_FADE fades both
	// the size and the alpha to 0; size handling is below.
	cls.colorEndMult[0] = 1.0f;
	cls.colorEndMult[1] = 1.0f;
	cls.colorEndMult[2] = 1.0f;
	cls.colorEndMult[3] = 0.0f;

	// Size. CPU: re->radius = 1.5 + random()*1.5 = uniform
	// [1.5, 3.0]. Symmetric crand() jitter expresses this as
	// midpoint 2.25 ± halfwidth 0.75. sizeEnd = 0 reproduces
	// LE_MOVE_SCALE_FADE's shrink-to-zero behavior.
	cls.sizeStart  = 2.25f;
	cls.sizeJitter = 0.75f;
	cls.sizeEnd	   = 0.0f;

	// Gravity. CPU TR_GRAVITY uses q3 default 800 u/s²; compute
	// shader's WORLD_GRAVITY also = 800, so gravityScale = 1.0
	// reproduces CPU fall behavior. drag = 0 because CPU has no
	// velocity damping.
	cls.gravityScale = 1.0f;
	cls.drag		 = 0.0f;

	cgs.media.lgSparksClass = (qhandle_t)CG_RegisterParticleClass( "lg_sparks", &cls );
}


/*
==========================
CG_RegisterPushParticleClasses

Register the sparkle-stream particle class consumed by
the PTRAIL_PUSH def-table entry. Called once from
CG_RegisterGraphics (cg_main.c) after the particle shader is
bound and before CG_RegisterPlayerTrailDefs wires the handle into
the def table.

The stream flows along the beam axis (anchor → player). Per-emission
color tint and alphaScale are supplied by CG_EmitPlayerTrailParticles
from the resolved per-trail color × the (capped) fade alpha; the
class itself stores a neutral-white palette that the tint
multiplies.

Class parameters mirror the jumppad_stream verbatim — only
the registration function and class-name string change. Behaviour
tweaks (velocity-scaled emit count, alpha ceiling) live in the
def table and at the render-side call sites, not in the class.
==========================
*/
void CG_RegisterPushParticleClasses( void )
{
	particleClass_t cls;

	// ── push_stream ────────────────────────────────────────────────
	// Soft sparkles flowing along the beam axis. Emitted along the
	// trigger→player path; velocity along the same axis carries them
	// toward the player. No gravity (axial flow only); short lifetime
	// so the stream stays tightly clustered around the beam.
	memset( &cls, 0, sizeof( cls ) );
	cls.shader = cgs.media.lightningSparkShader; // same soft sparkle texture
												 // the LG impact shower uses;
												 // tint at emit time covers
												 // the color difference.
	cls.renderFlags		 = PRIM_FLAG_ADDITIVE;
	cls.emitMode		 = EMIT_PATH;
	cls.scatterShape	 = SCATTER_CUBE;
	cls.scatterMagnitude = 6.0f; // perpendicular jitter around beam axis
	cls.velocityShape	 = VEL_AXIAL_PLUS_CUBE;
	cls.axialSpeed		 = 384.0f; // flow speed trigger→player
	cls.cubeJitter		 = 32.0f;  // small lateral drift
	cls.coneHalfAngle	 = 0.0f;
	cls.lifetimeMean	 = 0.4f; // 400 ms
	cls.lifetimeJitter	 = 0.1f;
	// Neutral white palette; per-emission colorTint applies the
	// hook-color multiplier.
	cls.paletteCount	   = 1;
	cls.colorPalette[0][0] = 1.0f;
	cls.colorPalette[0][1] = 1.0f;
	cls.colorPalette[0][2] = 1.0f;
	cls.colorPalette[0][3] = 1.0f;
	cls.colorEndMult[0]	   = 1.0f;
	cls.colorEndMult[1]	   = 1.0f;
	cls.colorEndMult[2]	   = 1.0f;
	cls.colorEndMult[3]	   = 0.0f; // fade alpha to 0 over lifetime
	cls.sizeStart		   = 3.0f;
	cls.sizeEnd			   = 0.0f;
	cls.gravityScale	   = 0.0f; // axial flow only, no fall
	cls.drag			   = 0.0f;

	cgs.media.pushStreamClass = (qhandle_t)CG_RegisterParticleClass( "push_stream", &cls );
}


/*
==========================
CG_RegisterRocketTrailParticleClass

The rocket follows Quake 4's projectile-bound, composited fx_fly idea rather
than treating the trail as one enlarged smoke puff. Three independent bounded
GPU classes form the effect: a compact hot exhaust core, a dark expanding smoke
wake and sparse micro-embers. The grenade remains its unrelated classic point
trail. All four recipes share only the trajectory/liquid scheduler and the
GPU emit-and-forget lifecycle.
==========================
*/
void CG_RegisterRocketTrailParticleClass( void )
{
	particleClass_t cls;
	qhandle_t       exhaustShader;
	static const float easeOut[PARTICLE_CURVE_SAMPLES] = {
		0.00f, 0.45f, 0.70f, 0.84f, 0.92f, 0.96f, 0.99f, 1.00f
	};
	static const float lateFade[PARTICLE_CURVE_SAMPLES] = {
		1.00f, 1.00f, 0.97f, 0.90f, 0.76f, 0.55f, 0.28f, 0.00f
	};

	CG_RegisterParticleCurve( "ease-out", easeOut );
	CG_RegisterParticleCurve( "late-fade", lateFade );
	exhaustShader = trap_R_RegisterShader( "rocketExhaustGlow" );

	// Short additive core: always touches the nozzle, then contracts and cools
	// within a fraction of a second. Negative axial speed moves spawned energy
	// back along the flight axis while the projectile continues forward.
	memset( &cls, 0, sizeof( cls ) );
	cls.shader             = exhaustShader;
	cls.renderFlags        = PRIM_FLAG_ADDITIVE;
	cls.emitMode           = EMIT_PATH;
	cls.scatterShape       = SCATTER_PERP_DISC;
	cls.scatterMagnitude   = 0.8f;
	cls.velocityShape      = VEL_AXIAL_PLUS_CUBE;
	cls.axialSpeed         = -24.0f;
	cls.cubeJitter         = 3.5f;
	cls.lifetimeMean       = 0.18f;
	cls.lifetimeJitter     = 0.04f;
	cls.paletteCount       = 3;
	Vector4Set( cls.colorPalette[0], 1.00f, 0.78f, 0.32f, 0.95f );
	Vector4Set( cls.colorPalette[1], 1.00f, 0.46f, 0.10f, 0.90f );
	Vector4Set( cls.colorPalette[2], 0.90f, 0.22f, 0.04f, 0.82f );
	Vector4Set( cls.colorEndMult, 0.80f, 0.12f, 0.02f, 0.0f );
	cls.sizeStart          = 4.5f;
	cls.sizeEnd            = 0.8f;
	cls.sizeJitter         = 0.45f;
	cls.gravityScale       = 0.0f;
	cls.drag               = 3.0f;
	cgs.media.rocketExhaustClass =
		(qhandle_t)CG_RegisterParticleClass( "rocket_exhaust_core", &cls );

	// Dark warm-grey wake. It expands quickly, then hangs and fades late like
	// Q4's persistent projectile smoke instead of the former 8→56 white plume.
	memset( &cls, 0, sizeof( cls ) );
	cls.shader             = cgs.media.smokePuffShader;
	cls.renderFlags        = 0;
	cls.emitMode           = EMIT_PATH;
	cls.scatterShape       = SCATTER_PERP_DISC;
	cls.scatterMagnitude   = 1.1f;
	cls.velocityShape      = VEL_AXIAL_PLUS_CUBE;
	cls.axialSpeed         = -10.0f;
	cls.cubeJitter         = 4.0f;
	cls.lifetimeMean       = 1.10f;
	cls.lifetimeJitter     = 0.18f;
	cls.paletteCount       = 4;
	Vector4Set( cls.colorPalette[0], 0.52f, 0.46f, 0.40f, 1.0f );
	Vector4Set( cls.colorPalette[1], 0.45f, 0.41f, 0.37f, 1.0f );
	Vector4Set( cls.colorPalette[2], 0.38f, 0.36f, 0.34f, 1.0f );
	Vector4Set( cls.colorPalette[3], 0.31f, 0.31f, 0.31f, 1.0f );
	Vector4Set( cls.colorEndMult, 0.35f, 0.35f, 0.35f, 0.0f );
	cls.sizeStart          = 3.5f;
	cls.sizeEnd            = 24.0f;
	cls.sizeJitter         = 0.9f;
	cls.gravityScale       = -0.015f;
	cls.drag               = 0.7f;
	cls.sizeParm.calc      = PARM_CURVE;
	cls.sizeParm.val0      = 3.5f;
	cls.sizeParm.val1      = 24.0f;
	CG_ResolveParticleParmCurve( &cls.sizeParm, "ease-out" );
	cls.alphaParm.calc     = PARM_CURVE;
	cls.alphaParm.val0     = 0.0f;
	cls.alphaParm.val1     = 0.62f;
	CG_ResolveParticleParmCurve( &cls.alphaParm, "late-fade" );
	cgs.media.rocketSmokeClass =
		(qhandle_t)CG_RegisterParticleClass( "rocket_smoke_wake", &cls );

	// Sparse sparks punctuate the exhaust without becoming a second solid
	// ribbon. Their scheduler runs at half the smoke rate and the GPU owns the
	// complete ballistic/fade lifetime after the one-shot emit.
	memset( &cls, 0, sizeof( cls ) );
	cls.shader             = exhaustShader;
	cls.renderFlags        = PRIM_FLAG_ADDITIVE;
	cls.emitMode           = EMIT_PATH;
	cls.scatterShape       = SCATTER_PERP_DISC;
	cls.scatterMagnitude   = 2.0f;
	cls.velocityShape      = VEL_AXIAL_PLUS_CUBE;
	cls.axialSpeed         = -28.0f;
	cls.cubeJitter         = 9.0f;
	cls.lifetimeMean       = 0.42f;
	cls.lifetimeJitter     = 0.12f;
	cls.paletteCount       = 3;
	Vector4Set( cls.colorPalette[0], 1.00f, 0.72f, 0.22f, 0.92f );
	Vector4Set( cls.colorPalette[1], 1.00f, 0.36f, 0.06f, 0.88f );
	Vector4Set( cls.colorPalette[2], 0.78f, 0.12f, 0.02f, 0.80f );
	Vector4Set( cls.colorEndMult, 0.85f, 0.10f, 0.01f, 0.0f );
	cls.sizeStart          = 0.75f;
	cls.sizeEnd            = 0.12f;
	cls.sizeJitter         = 0.18f;
	cls.gravityScale       = 0.06f;
	cls.drag               = 1.2f;
	cgs.media.rocketEmberClass =
		(qhandle_t)CG_RegisterParticleClass( "rocket_hot_embers", &cls );

	// Grenade-only classic trail.  The original Quake grenade recipe uses the
	// dark half of its fire ramp, a roughly 3-unit path cadence, small spawn
	// jitter and an upward-drifting lifetime.  Quake II's diminishing
	// grenade trail similarly uses dark palette particles with positional and
	// velocity jitter.  Represent that character with small solid particles,
	// not smokePuff billboards; the shared emitter uses a pragmatic 4-unit
	// cadence so the GPU path stays bounded while preserving the dotted chain.
	memset( &cls, 0, sizeof( cls ) );
	cls.shader             = cgs.media.whiteShader;
	cls.renderFlags        = 0;
	cls.emitMode           = EMIT_PATH;
	cls.scatterShape       = SCATTER_CUBE;
	cls.scatterMagnitude   = 3.0f;
	cls.velocityShape      = VEL_PURE_CUBE;
	cls.cubeJitter         = 6.0f;
	/* Preserve the authored point size and scatter, but keep the chain readable
	 * a little longer for competitive trajectory tracking. */
	cls.lifetimeMean       = 0.90f;
	cls.lifetimeJitter     = 0.18f;
	cls.paletteCount       = 8;
	for ( int i = 0; i < cls.paletteCount; ++i ) {
		const float grey = 0.18f + 0.03f * (float)i;
		cls.colorPalette[i][0] = grey;
		cls.colorPalette[i][1] = grey * 0.96f;
		cls.colorPalette[i][2] = grey * 0.88f;
		cls.colorPalette[i][3] = 0.90f;
	}
	cls.colorEndMult[0] = 0.25f;
	cls.colorEndMult[1] = 0.25f;
	cls.colorEndMult[2] = 0.25f;
	cls.colorEndMult[3] = 0.0f;
	cls.sizeStart        = 0.65f;
	cls.sizeEnd          = 0.35f;
	cls.sizeJitter       = 0.15f;
	// Quake's pt_fire rises and Quake II applies a small +Z acceleration.
	// Negative gravityScale means a gentle up-drift in Wired's z-up solver.
	cls.gravityScale     = -0.03f;
	cls.drag             = 0.0f;
	cgs.media.grenadeTrailClass =
		(qhandle_t)CG_RegisterParticleClass( "grenade_classic_trail", &cls );
}


/*
==========================
CG_RegisterGibTrailParticleClass

MIG-trail-2: GPU-ring class mirroring the CPU CG_BloodTrail drift-sprites
(CG_SmokePuff + LE_FALL_SCALE_FADE + trDelta[2]=40). The gib BODY itself
(LE_FRAGMENT physics/collision/bounce) stays CPU; only the per-frame
drift-sprites migrate. GPU single path (W-51 — legacy LE_ loop retired).

Look match (vs CG_AddFallScaleFade, radius 20, lifetime 2000 ms):
  - bloodTrailShader, ALPHA blend (renderFlags 0).
  - EMIT_PATH along the travelled trajectory segment.
  - size 16 → 36 (CPU re->radius = radius*(1-c)+16 with radius 20: c=1→16, c=0→36).
  - alpha 1.0 → 0 linear (CPU shaderRGBA[3]=0xff*c*color[3], color[3]=1 from
    CG_SmokePuff a=1).
  - LINEAR -Z drift 40 u over 2 s = velocityBias[2] -20 (NOT gravityScale —
    CPU origin[2] = trBase[2] - (1-c)*trDelta[2], trDelta[2]=40: a constant
    downward push reaching -40 at death, no acceleration).
  - single sprite (frameCount 0 — no flipbook).
==========================
*/
void CG_RegisterGibTrailParticleClass( void )
{
	particleClass_t cls;

	memset( &cls, 0, sizeof( cls ) );
	cls.shader			   = cgs.media.bloodTrailShader; // same sprite as the LE_ path
	cls.renderFlags		   = 0;							 // ALPHA blend (not PRIM_FLAG_ADDITIVE)
	cls.emitMode		   = EMIT_PATH;					 // uniform along the travelled segment
	cls.scatterShape	   = SCATTER_NONE;
	cls.scatterMagnitude   = 0.0f;
	cls.velocityShape	   = VEL_PURE_CUBE; // no scatter; bias supplies drift
	cls.axialSpeed		   = 0.0f;
	cls.cubeJitter		   = 0.0f;
	cls.coneHalfAngle	   = 0.0f;
	cls.lifetimeMean	   = 2.0f; // 2000 ms
	cls.lifetimeJitter	   = 0.0f;
	cls.paletteCount	   = 1;
	cls.colorPalette[0][0] = 1.0f;
	cls.colorPalette[0][1] = 1.0f;
	cls.colorPalette[0][2] = 1.0f;
	cls.colorPalette[0][3] = 1.0f; // base alpha 1.0
	cls.colorEndMult[0]	   = 1.0f;
	cls.colorEndMult[1]	   = 1.0f;
	cls.colorEndMult[2]	   = 1.0f;
	cls.colorEndMult[3]	   = 0.0f;	// alpha → 0 over lifetime
	cls.sizeStart		   = 16.0f; // radius(20)*(1-c)+16 at c=1 = 16
	cls.sizeEnd			   = 36.0f; // radius(20)*(1-c)+16 at c=0 = 36
	cls.gravityScale	   = 0.0f;	// drift is LINEAR (velocityBias), no accel
	cls.drag			   = 0.0f;
	cls.velocityBias[2]	   = -20.0f; // -Z drift 40 u over 2 s lifetime

	cgs.media.gibTrailClass = (qhandle_t)CG_RegisterParticleClass( "blood_trail", &cls );
}


/*
==========================
CG_RegisterExplosionParticleClasses

Rocket-explosion fallback fire core plus flipbook-free layered primitives.
The fallback is a GPU-ring flipbook (Track-C Stage-2 beat 1).
The eight rlboom_1..8 frames (the same sequence the CPU rocketExplosion
animmap played) become a frameCount-8 flipbook with sub-frame blend, so a
single BURST particle at impact reads as the expanding fireball. One light
core that stays put — the explosion grows through the flipbook frames, not
motion. ROCKET_FX_EXPLOSION=0 keeps this as the reversible comparison path;
the enabled layered profiles never reference it.
==========================
*/
void CG_RegisterExplosionParticleClasses( void )
{
	particleClass_t cls;
	int				i;
	static const float rocketConvexSize[PARTICLE_CURVE_SAMPLES] = {
		0.00f, 0.34f, 0.59f, 0.76f, 0.87f, 0.94f, 0.98f, 1.00f
	};
	static const float rocketHalfLinearSize[PARTICLE_CURVE_SAMPLES] = {
		0.00f, 0.25f, 0.47f, 0.65f, 0.79f, 0.89f, 0.96f, 1.00f
	};
	static const float rocketFireAlpha[PARTICLE_CURVE_SAMPLES] = {
		0.72f, 1.00f, 1.00f, 0.96f, 0.84f, 0.64f, 0.34f, 0.00f
	};
	static const float rocketSparkFlicker[PARTICLE_CURVE_SAMPLES] = {
		1.00f, 0.48f, 0.92f, 0.36f, 0.76f, 0.24f, 0.46f, 0.00f
	};
	static const float rocketSmokeAlpha[PARTICLE_CURVE_SAMPLES] = {
		0.00f, 0.52f, 0.82f, 0.96f, 0.92f, 0.72f, 0.38f, 0.00f
	};

	CG_RegisterParticleCurve( "rocket-convex-size", rocketConvexSize );
	CG_RegisterParticleCurve( "rocket-halflinear-size", rocketHalfLinearSize );
	CG_RegisterParticleCurve( "rocket-fire-alpha", rocketFireAlpha );
	CG_RegisterParticleCurve( "rocket-spark-flicker", rocketSparkFlicker );
	CG_RegisterParticleCurve( "rocket-smoke-alpha", rocketSmokeAlpha );

	memset( &cls, 0, sizeof( cls ) );

	// rlboom_1..8 — the bare texture path registers each PNG as an
	// implicit shader (same convention as cgs.media.* model textures,
	// e.g. cg_main.c "models/weaphits/kamikred"). frame 0 doubles as
	// the static fallback for the frameCount<=1 ring path.
	for ( i = 0; i < 8; i++ ) {
		cls.frameShaders[i] = trap_R_RegisterShader( va( "models/weaphits/rlboom/rlboom_%i", i + 1 ) );
	}
	cls.shader		= cls.frameShaders[0];
	cls.frameCount	= 8;
	cls.frameBlend	= 1;				  // silky sub-frame interpolation
	cls.renderFlags = PRIM_FLAG_ADDITIVE; // rlboom is additive (the renderer
										  // re-derives blend from the frame-0
										  // shader's stateBits regardless)

	// One BURST particle at the impact point; the flipbook IS the
	// explosion, so the core stays put (no scatter, no velocity).
	cls.emitMode		 = EMIT_POINT;
	cls.scatterShape	 = SCATTER_NONE;
	cls.scatterMagnitude = 0.0f;
	cls.velocityShape	 = VEL_AXIAL;
	cls.axialSpeed		 = 0.0f;
	cls.cubeJitter		 = 0.0f;

	// Lifetime 1.0 s = the rocketExplosion duration (1000 ms). The ring
	// advances frame = floor(age * 8) over [0,1] age, so the eight
	// frames play across exactly the same 1 s the LE_ sprite did.
	cls.lifetimeMean   = 1.0f;
	cls.lifetimeJitter = 0.0f;

	// Solid white palette — the additive rlboom texture carries its own
	// fire colour; no per-particle tint (colorEndMult 1 = no fade-mult,
	// the flipbook frames fade themselves).
	cls.paletteCount	   = 1;
	cls.colorPalette[0][0] = 1.0f;
	cls.colorPalette[0][1] = 1.0f;
	cls.colorPalette[0][2] = 1.0f;
	cls.colorPalette[0][3] = 1.0f;
	cls.colorEndMult[0]	   = 1.0f;
	cls.colorEndMult[1]	   = 1.0f;
	cls.colorEndMult[2]	   = 1.0f;
	cls.colorEndMult[3]	   = 1.0f;

	// Size matches the legacy LE_ sprite scale (CG_AddSpriteExplosion radius
	// ~30..42; the ring billboard half-extent is the radius, so ~36).
	cls.sizeStart	 = 36.0f;
	cls.sizeEnd		 = 36.0f; // constant — the frames do the growth
	cls.gravityScale = 0.0f;
	cls.drag		 = 0.0f;

	cgs.media.explosionFireClass = (qhandle_t)CG_RegisterParticleClass( "explosion_fire", &cls );

	/* Preserve the pre-migration Wired recipe: one low-alpha expanding shell.
	 * This class is presentation data, not a license to reinterpret the effect. */
	memset( &cls, 0, sizeof( cls ) );
	cls.shader             = trap_R_RegisterShader( "rocketExhaustGlow" );
	cls.renderFlags        = PRIM_FLAG_ADDITIVE;
	cls.emitMode           = EMIT_POINT;
	cls.scatterShape       = SCATTER_NONE;
	cls.velocityShape      = VEL_AXIAL;
	cls.lifetimeMean       = 0.38f;
	cls.lifetimeJitter     = 0.02f;
	cls.paletteCount       = 3;
	Vector4Set( cls.colorPalette[0], 1.00f, 0.74f, 0.22f, 0.42f );
	Vector4Set( cls.colorPalette[1], 1.00f, 0.40f, 0.06f, 0.34f );
	Vector4Set( cls.colorPalette[2], 0.72f, 0.14f, 0.02f, 0.26f );
	Vector4Set( cls.colorEndMult, 0.42f, 0.05f, 0.01f, 0.0f );
	cls.sizeStart          = 20.0f;
	cls.sizeEnd            = 70.0f;
	cls.gravityScale       = 0.0f;
	cls.drag               = 0.0f;
	(void)CG_RegisterParticleClass( "explosion_fire_shell", &cls );

	// Secondary impact beat shared by rockets and grenades. The old cgame
	// implementation spawned 128/64 broad CPU sprites, each with its own light.
	// A small, fast cone reads as hot casing/shrapnel instead, while using the
	// same warm palette and soft additive material as the rocket exhaust core.
	memset( &cls, 0, sizeof( cls ) );
	cls.shader             = trap_R_RegisterShader( "rocketExhaustGlow" );
	cls.renderFlags        = PRIM_FLAG_ADDITIVE;
	cls.emitMode           = EMIT_POINT;
	cls.scatterShape       = SCATTER_NONE;
	cls.scatterMagnitude   = 0.0f;
	cls.velocityShape      = VEL_CONE;
	cls.axialSpeed         = 360.0f;
	cls.speedJitter        = 120.0f;
	cls.coneHalfAngle      = 0.95f;
	cls.lifetimeMean       = 0.48f;
	cls.lifetimeJitter     = 0.12f;
	cls.paletteCount       = 4;
	Vector4Set( cls.colorPalette[0], 1.00f, 0.78f, 0.32f, 0.96f );
	Vector4Set( cls.colorPalette[1], 1.00f, 0.46f, 0.10f, 0.92f );
	Vector4Set( cls.colorPalette[2], 0.90f, 0.22f, 0.04f, 0.86f );
	Vector4Set( cls.colorPalette[3], 0.62f, 0.10f, 0.02f, 0.76f );
	Vector4Set( cls.colorEndMult, 0.50f, 0.04f, 0.01f, 0.0f );
	cls.sizeStart          = 1.35f;
	cls.sizeEnd            = 0.18f;
	cls.sizeJitter         = 0.30f;
	cls.gravityScale       = 0.45f;
	cls.drag               = 0.65f;
	cgs.media.explosionShrapnelClass =
		(qhandle_t)CG_RegisterParticleClass( "explosion_hot_shrapnel", &cls );

	/* Q4 MP's bright body lives for roughly one second: forty oriented cards
	 * launch from a sphere surface along their own radial normal. The previous
	 * approximation used unrelated cube velocity and camera billboards, which
	 * collapsed into a short-lived stack of round glows. */
	memset( &cls, 0, sizeof( cls ) );
	cls.shader             = trap_R_RegisterShader( "rocketExplosionFireCard" );
	cls.renderFlags        = PRIM_FLAG_ADDITIVE | PRIM_FLAG_PARTICLE_VELOCITY_ORIENTED;
	cls.emitMode           = EMIT_POINT;
	cls.scatterShape       = SCATTER_SPHERE;
	cls.scatterMagnitude   = 10.0f;
	cls.velocityShape      = VEL_RADIAL_FROM_SCATTER;
	cls.axialSpeed         = 66.0f;
	cls.speedJitter        = 16.0f;
	cls.lifetimeMean       = 0.96f;
	cls.lifetimeJitter     = 0.04f;
	cls.paletteCount       = 4;
	Vector4Set( cls.colorPalette[0], 1.00f, 0.91f, 0.60f, 1.00f );
	Vector4Set( cls.colorPalette[1], 1.00f, 0.68f, 0.28f, 0.96f );
	Vector4Set( cls.colorPalette[2], 1.00f, 0.42f, 0.08f, 0.92f );
	Vector4Set( cls.colorPalette[3], 0.82f, 0.20f, 0.03f, 0.86f );
	Vector4Set( cls.colorEndMult, 0.34f, 0.035f, 0.004f, 1.0f );
	cls.sizeStart          = 9.0f;
	cls.sizeEnd            = 36.0f;
	cls.sizeJitter         = 2.5f;
	cls.gravityScale       = 0.025f;
	cls.drag               = 0.38f;
	cls.sizeParm.calc      = PARM_CURVE;
	cls.sizeParm.val0      = 9.0f;
	cls.sizeParm.val1      = 36.0f;
	cls.sizeParm.variance  = 1.0f;
	CG_ResolveParticleParmCurve( &cls.sizeParm, "rocket-convex-size" );
	cls.alphaParm.calc     = PARM_CURVE;
	cls.alphaParm.val0     = 0.0f;
	cls.alphaParm.val1     = 0.92f;
	CG_ResolveParticleParmCurve( &cls.alphaParm, "rocket-fire-alpha" );
	(void)CG_RegisterParticleClass( "rocket_layered_fire_sphere", &cls );

	/* Four broad cards supply the persistent body that impact_mp/fire3 and
	 * detonate_mp/fire4 carry independently from the oriented shell. */
	memset( &cls, 0, sizeof( cls ) );
	cls.shader             = trap_R_RegisterShader( "rocketExplosionFireCard" );
	cls.renderFlags        = PRIM_FLAG_ADDITIVE;
	cls.emitMode           = EMIT_POINT;
	cls.scatterShape       = SCATTER_SPHERE;
	cls.scatterMagnitude   = 12.0f;
	cls.velocityShape      = VEL_RADIAL_FROM_SCATTER;
	cls.axialSpeed         = 25.0f;
	cls.speedJitter        = 9.0f;
	cls.lifetimeMean       = 1.0f;
	cls.lifetimeJitter     = 0.06f;
	cls.paletteCount       = 3;
	Vector4Set( cls.colorPalette[0], 1.00f, 0.76f, 0.38f, 0.88f );
	Vector4Set( cls.colorPalette[1], 1.00f, 0.46f, 0.10f, 0.82f );
	Vector4Set( cls.colorPalette[2], 0.78f, 0.18f, 0.025f, 0.74f );
	Vector4Set( cls.colorEndMult, 0.24f, 0.025f, 0.003f, 1.0f );
	cls.sizeStart          = 18.0f;
	cls.sizeEnd            = 52.0f;
	cls.sizeJitter         = 4.0f;
	cls.gravityScale       = 0.02f;
	cls.drag               = 0.7f;
	cls.sizeParm.calc      = PARM_CURVE;
	cls.sizeParm.val0      = 18.0f;
	cls.sizeParm.val1      = 52.0f;
	cls.sizeParm.variance  = 1.0f;
	CG_ResolveParticleParmCurve( &cls.sizeParm, "rocket-halflinear-size" );
	cls.alphaParm.calc     = PARM_CURVE;
	cls.alphaParm.val0     = 0.0f;
	cls.alphaParm.val1     = 0.82f;
	CG_ResolveParticleParmCurve( &cls.alphaParm, "rocket-fire-alpha" );
	(void)CG_RegisterParticleClass( "rocket_layered_fire_lobe", &cls );

	/* The MP surface impact keeps its expanding ring for 0.75 seconds. */
	memset( &cls, 0, sizeof( cls ) );
	cls.shader             = trap_R_RegisterShader( "rocketExplosionRing" );
	cls.renderFlags        = PRIM_FLAG_ADDITIVE;
	cls.emitMode           = EMIT_POINT;
	cls.scatterShape       = SCATTER_NONE;
	cls.velocityShape      = VEL_AXIAL;
	cls.lifetimeMean       = 0.74f;
	cls.lifetimeJitter     = 0.01f;
	cls.paletteCount       = 1;
	Vector4Set( cls.colorPalette[0], 1.00f, 0.72f, 0.36f, 0.62f );
	Vector4Set( cls.colorEndMult, 0.72f, 0.12f, 0.01f, 1.0f );
	cls.sizeStart          = 13.0f;
	cls.sizeEnd            = 94.0f;
	cls.gravityScale       = 0.0f;
	cls.drag               = 0.0f;
	cls.sizeParm.calc      = PARM_CURVE;
	cls.sizeParm.val0      = 13.0f;
	cls.sizeParm.val1      = 94.0f;
	CG_ResolveParticleParmCurve( &cls.sizeParm, "rocket-halflinear-size" );
	cls.alphaParm.calc     = PARM_CURVE;
	cls.alphaParm.val0     = 0.0f;
	cls.alphaParm.val1     = 0.62f;
	CG_ResolveParticleParmCurve( &cls.alphaParm, "rocket-fire-alpha" );
	(void)CG_RegisterParticleClass( "rocket_layered_pressure_ring", &cls );

	/* Surface-hit streaks inherit the impact normal.  Motion-trail rendering
	 * turns each bounded particle into the short falling trace seen in Q4's
	 * impact_mp sparks instead of a point that teleports away from the wall. */
	memset( &cls, 0, sizeof( cls ) );
	cls.shader             = trap_R_RegisterShader( "gfx/misc/tracer" );
	cls.renderFlags        = PRIM_FLAG_ADDITIVE | PRIM_FLAG_PARTICLE_MOTION_TRAIL;
	cls.emitMode           = EMIT_POINT;
	cls.scatterShape       = SCATTER_SPHERE;
	cls.scatterMagnitude   = 4.0f;
	cls.velocityShape      = VEL_CONE;
	cls.axialSpeed         = 92.0f;
	cls.speedJitter        = 34.0f;
	cls.coneHalfAngle      = 1.0f;
	cls.lifetimeMean       = 1.25f;
	cls.lifetimeJitter     = 0.25f;
	cls.paletteCount       = 3;
	Vector4Set( cls.colorPalette[0], 1.00f, 0.92f, 0.64f, 1.00f );
	Vector4Set( cls.colorPalette[1], 1.00f, 0.58f, 0.14f, 0.96f );
	Vector4Set( cls.colorPalette[2], 0.86f, 0.24f, 0.04f, 0.88f );
	Vector4Set( cls.colorEndMult, 0.56f, 0.06f, 0.008f, 0.0f );
	cls.sizeStart          = 0.9f;
	cls.sizeEnd            = 0.18f;
	cls.sizeJitter         = 0.25f;
	cls.gravityScale       = 0.72f;
	cls.drag               = 0.34f;
	cls.alphaParm.calc     = PARM_CURVE;
	cls.alphaParm.val0     = 0.0f;
	cls.alphaParm.val1     = 1.0f;
	CG_ResolveParticleParmCurve( &cls.alphaParm, "rocket-spark-flicker" );
	(void)CG_RegisterParticleClass( "rocket_layered_impact_streak", &cls );

	/* A free-air detonation has no surface normal to form a believable cone.
	 * Use the same streak primitive with an isotropic velocity budget instead. */
	memset( &cls, 0, sizeof( cls ) );
	cls.shader             = trap_R_RegisterShader( "gfx/misc/tracer" );
	cls.renderFlags        = PRIM_FLAG_ADDITIVE | PRIM_FLAG_PARTICLE_MOTION_TRAIL;
	cls.emitMode           = EMIT_POINT;
	cls.scatterShape       = SCATTER_SPHERE;
	cls.scatterMagnitude   = 5.0f;
	cls.velocityShape      = VEL_PURE_CUBE;
	cls.cubeJitter         = 82.0f;
	cls.velocityBias[2]    = 10.0f;
	cls.velocityBiasJitter[2] = 7.0f;
	cls.lifetimeMean       = 1.25f;
	cls.lifetimeJitter     = 0.25f;
	cls.paletteCount       = 3;
	Vector4Set( cls.colorPalette[0], 1.00f, 0.92f, 0.64f, 1.00f );
	Vector4Set( cls.colorPalette[1], 1.00f, 0.58f, 0.14f, 0.96f );
	Vector4Set( cls.colorPalette[2], 0.86f, 0.24f, 0.04f, 0.88f );
	Vector4Set( cls.colorEndMult, 0.56f, 0.06f, 0.008f, 0.0f );
	cls.sizeStart          = 0.9f;
	cls.sizeEnd            = 0.18f;
	cls.sizeJitter         = 0.25f;
	cls.gravityScale       = 0.65f;
	cls.drag               = 0.30f;
	cls.alphaParm.calc     = PARM_CURVE;
	cls.alphaParm.val0     = 0.0f;
	cls.alphaParm.val1     = 1.0f;
	CG_ResolveParticleParmCurve( &cls.alphaParm, "rocket-spark-flicker" );
	(void)CG_RegisterParticleClass( "rocket_layered_air_streak", &cls );

	/* Q4 delays its hanging/upward smoke behind the flash.  Keep that delay in
	 * the WiredFX recipe; the class only owns the slow expansion and lift. */
	memset( &cls, 0, sizeof( cls ) );
	cls.shader             = cgs.media.smokePuffShader;
	cls.emitMode           = EMIT_POINT;
	cls.scatterShape       = SCATTER_SPHERE;
	cls.scatterMagnitude   = 6.0f;
	cls.velocityShape      = VEL_CONE;
	cls.axialSpeed         = 24.0f;
	cls.speedJitter        = 10.0f;
	cls.coneHalfAngle      = 1.05f;
	cls.velocityBias[2]    = 12.0f;
	cls.velocityBiasJitter[2] = 5.0f;
	cls.lifetimeMean       = 1.0f;
	cls.lifetimeJitter     = 0.20f;
	cls.paletteCount       = 3;
	Vector4Set( cls.colorPalette[0], 0.34f, 0.29f, 0.24f, 0.30f );
	Vector4Set( cls.colorPalette[1], 0.25f, 0.23f, 0.21f, 0.26f );
	Vector4Set( cls.colorPalette[2], 0.18f, 0.18f, 0.18f, 0.22f );
	Vector4Set( cls.colorEndMult, 0.68f, 0.66f, 0.64f, 0.0f );
	cls.sizeStart          = 12.0f;
	cls.sizeEnd            = 42.0f;
	cls.sizeJitter         = 4.0f;
	cls.gravityScale       = -0.025f;
	cls.drag               = 1.15f;
	cls.sizeParm.calc      = PARM_CURVE;
	cls.sizeParm.val0      = 12.0f;
	cls.sizeParm.val1      = 42.0f;
	cls.sizeParm.variance  = 1.0f;
	CG_ResolveParticleParmCurve( &cls.sizeParm, "rocket-halflinear-size" );
	cls.alphaParm.calc     = PARM_CURVE;
	cls.alphaParm.val0     = 0.0f;
	cls.alphaParm.val1     = 0.34f;
	CG_ResolveParticleParmCurve( &cls.alphaParm, "rocket-smoke-alpha" );
	(void)CG_RegisterParticleClass( "rocket_layered_smoke", &cls );

	// Quake 4's authored impact effects open with a very short additive flash,
	// followed by directional streaks plus surface-specific smoke/debris. Keep
	// the flash a separate GPU layer so cgame can select the SP or MP count
	// without turning the whole hit into one oversized smoke billboard.
	memset( &cls, 0, sizeof( cls ) );
	cls.shader             = trap_R_RegisterShader( "rocketExhaustGlow" );
	cls.renderFlags        = PRIM_FLAG_ADDITIVE;
	cls.emitMode           = EMIT_POINT;
	cls.scatterShape       = SCATTER_SPHERE;
	cls.scatterMagnitude   = 1.25f;
	cls.velocityShape      = VEL_AXIAL;
	cls.axialSpeed         = 4.0f;
	cls.speedJitter        = 2.0f;
	cls.lifetimeMean       = 0.10f;
	cls.lifetimeJitter     = 0.015f;
	cls.paletteCount       = 3;
	Vector4Set( cls.colorPalette[0], 1.00f, 0.96f, 0.76f, 0.92f );
	Vector4Set( cls.colorPalette[1], 1.00f, 0.70f, 0.24f, 0.86f );
	Vector4Set( cls.colorPalette[2], 0.86f, 0.42f, 0.08f, 0.78f );
	Vector4Set( cls.colorEndMult, 0.45f, 0.12f, 0.02f, 0.0f );
	cls.sizeStart          = 4.0f;
	cls.sizeEnd            = 0.20f;
	cls.sizeJitter         = 1.0f;
	cls.gravityScale       = 0.0f;
	cls.drag               = 0.0f;
	cgs.media.hitscanFlashClass =
		(qhandle_t)CG_RegisterParticleClass( "hitscan_impact_flash", &cls );

	// q4base's shotgun impact flash is authored at 8 units rather than the
	// compact bullet card. Keep that silhouette as a distinct class so the
	// shared pellet recipe does not have to scale every other particle layer.
	memset( &cls, 0, sizeof( cls ) );
	cls.shader             = trap_R_RegisterShader( "rocketExhaustGlow" );
	cls.renderFlags        = PRIM_FLAG_ADDITIVE;
	cls.emitMode           = EMIT_POINT;
	cls.scatterShape       = SCATTER_SPHERE;
	cls.scatterMagnitude   = 1.25f;
	cls.velocityShape      = VEL_AXIAL;
	cls.axialSpeed         = 4.0f;
	cls.speedJitter        = 2.0f;
	cls.lifetimeMean       = 0.10f;
	cls.lifetimeJitter     = 0.015f;
	cls.paletteCount       = 3;
	Vector4Set( cls.colorPalette[0], 1.00f, 0.96f, 0.76f, 0.92f );
	Vector4Set( cls.colorPalette[1], 1.00f, 0.70f, 0.24f, 0.86f );
	Vector4Set( cls.colorPalette[2], 0.86f, 0.42f, 0.08f, 0.78f );
	Vector4Set( cls.colorEndMult, 0.45f, 0.12f, 0.02f, 0.0f );
	cls.sizeStart          = 8.0f;
	cls.sizeEnd            = 0.30f;
	cls.sizeJitter         = 1.25f;
	cls.gravityScale       = 0.0f;
	cls.drag               = 0.0f;
	(void)CG_RegisterParticleClass( "shotgun_impact_flash", &cls );

	// The remaining classes are generic GPU building blocks. Weapon/material
	// recipe counts live at the composition point in cg_weapons.c and mirror
	// the concrete/default/electronics families in q4base pak001.pk4.
	memset( &cls, 0, sizeof( cls ) );
	cls.shader             = trap_R_RegisterShader( "gfx/misc/tracer" );
	cls.renderFlags        = PRIM_FLAG_ADDITIVE | PRIM_FLAG_PARTICLE_MOTION_TRAIL;
	cls.emitMode           = EMIT_POINT;
	cls.scatterShape       = SCATTER_NONE;
	cls.velocityShape      = VEL_CONE;
	/* q4base impact_default.fx: velocity 50..300, lifetime .35..50,
	 * gravity .5..1 and a 0.1 s / three-sample motion trail. The renderer
	 * reconstructs the continuous equivalent of those samples from velocity. */
	cls.axialSpeed         = 175.0f;
	cls.speedJitter        = 125.0f;
	cls.coneHalfAngle      = 0.70f;
	cls.lifetimeMean       = 0.425f;
	cls.lifetimeJitter     = 0.075f;
	cls.paletteCount       = 3;
	Vector4Set( cls.colorPalette[0], 1.00f, 0.95f, 0.74f, 1.00f );
	Vector4Set( cls.colorPalette[1], 1.00f, 0.68f, 0.20f, 0.96f );
	Vector4Set( cls.colorPalette[2], 0.92f, 0.34f, 0.05f, 0.88f );
	Vector4Set( cls.colorEndMult, 0.65f, 0.08f, 0.01f, 0.0f );
	cls.sizeStart          = 0.75f;
	cls.sizeEnd            = 0.25f;
	cls.sizeJitter         = 0.25f;
	cls.gravityScale       = 0.75f;
	cls.drag               = 0.0f;
	cgs.media.hitscanMetalSparkClass =
		(qhandle_t)CG_RegisterParticleClass( "hitscan_metal_sparks", &cls );

	memset( &cls, 0, sizeof( cls ) );
	cls.shader             = cgs.media.whiteShader;
	cls.renderFlags        = 0;
	cls.emitMode           = EMIT_POINT;
	cls.scatterShape       = SCATTER_NONE;
	cls.velocityShape      = VEL_CONE;
	cls.axialSpeed         = 125.0f;
	cls.speedJitter        = 55.0f;
	cls.coneHalfAngle      = 1.05f;
	cls.lifetimeMean       = 0.34f;
	cls.lifetimeJitter     = 0.10f;
	cls.paletteCount       = 3;
	Vector4Set( cls.colorPalette[0], 0.46f, 0.43f, 0.38f, 0.88f );
	Vector4Set( cls.colorPalette[1], 0.30f, 0.28f, 0.25f, 0.82f );
	Vector4Set( cls.colorPalette[2], 0.18f, 0.17f, 0.16f, 0.76f );
	Vector4Set( cls.colorEndMult, 0.45f, 0.42f, 0.38f, 0.0f );
	cls.sizeStart          = 0.48f;
	cls.sizeEnd            = 0.16f;
	cls.sizeJitter         = 0.12f;
	cls.gravityScale       = 0.85f;
	cls.drag               = 0.18f;
	cgs.media.hitscanChipClass =
		(qhandle_t)CG_RegisterParticleClass( "hitscan_surface_chips", &cls );

	memset( &cls, 0, sizeof( cls ) );
	cls.shader             = cgs.media.smokePuffShader;
	// Impact dust begins immediately in front of the struck surface. Generic
	// soft-particle fading would erase the billboard at that intersection and
	// leave the already-composited bullet mark visible through the centre of the
	// smoke. Preserve normal depth testing, but identify this class as deliberately
	// surface-anchored so the renderer keeps its authored opacity there.
	cls.renderFlags        = PRIM_FLAG_PARTICLE_SURFACE_ANCHORED;
	cls.emitMode           = EMIT_POINT;
	cls.scatterShape       = SCATTER_NONE;
	cls.velocityShape      = VEL_CONE;
	// This layer visually veils the persistent impact mark, so its centre must
	// remain registered to that mark.  Normal/global-Z velocity made a floor hit
	// visibly crawl upward during the puff's one-second lifetime.  Expansion and
	// alpha decay provide the smoke motion without translating the impact centre.
	cls.axialSpeed         = 0.0f;
	cls.speedJitter        = 0.0f;
	cls.coneHalfAngle      = 1.15f;
	cls.lifetimeMean       = 1.10f;
	cls.lifetimeJitter     = 0.20f;
	cls.paletteCount       = 3;
	Vector4Set( cls.colorPalette[0], 0.52f, 0.47f, 0.39f, 0.20f );
	Vector4Set( cls.colorPalette[1], 0.40f, 0.37f, 0.32f, 0.17f );
	Vector4Set( cls.colorPalette[2], 0.30f, 0.29f, 0.27f, 0.14f );
	Vector4Set( cls.colorEndMult, 0.78f, 0.76f, 0.72f, 0.0f );
	cls.sizeStart          = 1.10f;
	cls.sizeEnd            = 7.50f;
	cls.sizeJitter         = 0.35f;
	cls.gravityScale       = 0.0f;
	cls.drag               = 0.80f;
	cls.velocityBias[2]    = 0.0f;
	cgs.media.hitscanDustClass =
		(qhandle_t)CG_RegisterParticleClass( "hitscan_dust_puff", &cls );

	/* Shared path bubbles and surface crown droplets cover the liquid branches
	 * of all four Part-I weapons through authored profiles. */
	memset( &cls, 0, sizeof( cls ) );
	cls.shader             = cgs.media.waterBubbleShader;
	cls.emitMode           = EMIT_PATH;
	cls.scatterShape       = SCATTER_NONE;
	cls.velocityShape      = VEL_PURE_CUBE;
	cls.cubeJitter         = 5.0f;
	cls.velocityBias[2]    = 6.0f;
	cls.lifetimeMean       = 1.125f;
	cls.lifetimeJitter     = 0.125f;
	cls.paletteCount       = 1;
	Vector4Set( cls.colorPalette[0], 1.0f, 1.0f, 1.0f, 1.0f );
	Vector4Set( cls.colorEndMult, 1.0f, 1.0f, 1.0f, 0.0f );
	cls.sizeStart          = 3.0f;
	cls.sizeEnd            = 3.0f;
	(void)CG_RegisterParticleClass( "weapon_water_bubble_trail", &cls );

	memset( &cls, 0, sizeof( cls ) );
	cls.shader             = cgs.media.waterBubbleShader;
	cls.emitMode           = EMIT_POINT;
	cls.scatterShape       = SCATTER_NONE;
	cls.velocityShape      = VEL_AXIAL;
	cls.axialSpeed         = 75.0f;
	cls.speedJitter        = 25.0f;
	cls.velocityBiasJitter[0] = 40.0f;
	cls.velocityBiasJitter[1] = 40.0f;
	cls.lifetimeMean       = 0.40f;
	cls.lifetimeJitter     = 0.10f;
	cls.paletteCount       = 1;
	Vector4Set( cls.colorPalette[0], 1.0f, 1.0f, 1.0f, 1.0f );
	Vector4Set( cls.colorEndMult, 1.0f, 1.0f, 1.0f, 0.0f );
	cls.sizeStart          = 6.5f;
	cls.sizeEnd            = 6.5f;
	cls.sizeJitter         = 1.5f;
	cls.sizeParm.calc      = PARM_CONSTANT;
	cls.sizeParm.val0      = 6.5f;
	cls.sizeParm.variance  = 1.0f;
	cls.gravityScale       = 1.0f;
	(void)CG_RegisterParticleClass( "weapon_water_splash", &cls );

	/* Underwater rocket detonation owns a distinct rising-bubble family.  It is
	 * registered by name for WiredFX and intentionally needs no cgame handle:
	 * the semantic recipe emits it once and the renderer owns its lifecycle. */
	memset( &cls, 0, sizeof( cls ) );
	cls.shader             = cgs.media.waterBubbleShader;
	cls.emitMode           = EMIT_POINT;
	cls.scatterShape       = SCATTER_SPHERE;
	cls.scatterMagnitude   = 20.0f;
	cls.velocityShape      = VEL_AXIAL_PLUS_CUBE;
	cls.axialSpeed         = 42.0f;
	cls.cubeJitter         = 16.0f;
	cls.velocityBias[2]    = 18.0f;
	cls.velocityBiasJitter[2] = 10.0f;
	cls.lifetimeMean       = 1.0f;
	cls.lifetimeJitter     = 0.35f;
	cls.paletteCount       = 2;
	Vector4Set( cls.colorPalette[0], 0.72f, 0.86f, 1.0f, 0.62f );
	Vector4Set( cls.colorPalette[1], 0.48f, 0.70f, 0.88f, 0.48f );
	Vector4Set( cls.colorEndMult, 0.9f, 0.95f, 1.0f, 0.0f );
	cls.sizeStart          = 0.8f;
	cls.sizeEnd            = 2.8f;
	cls.sizeJitter         = 0.6f;
	cls.gravityScale       = -0.08f;
	cls.drag               = 0.5f;
	(void)CG_RegisterParticleClass( "explosion_water_bubbles", &cls );
}

/*
==========================
CG_RegisterAtmosphereParticleClasses

Registers app-authored recipes for contextual atmosphere emitters.  The
renderer receives only generic particle classes and bounded effect graphs;
breath/mist/debris meaning remains on the cgame side of the public seam.
==========================
*/
void CG_RegisterAtmosphereParticleClasses( void )
{
	particleClass_t			  cls;
	atmosphereEffectProfile_t profile;

	memset( &cls, 0, sizeof( cls ) );
	cls.shader			 = cgs.media.smokePuffShader;
	cls.emitMode		 = EMIT_POINT;
	cls.scatterShape	 = SCATTER_SPHERE;
	cls.scatterMagnitude = 2.0f;
	cls.velocityShape	 = VEL_AXIAL_PLUS_CUBE;
	cls.axialSpeed		 = 12.0f;
	cls.cubeJitter		 = 2.0f;
	cls.velocityBias[2]	 = 2.0f;
	cls.lifetimeMean	 = 1.2f;
	cls.lifetimeJitter	 = 0.2f;
	cls.paletteCount	 = 1;
	Vector4Set( cls.colorPalette[0], 0.82f, 0.90f, 1.0f, 0.38f );
	Vector4Set( cls.colorEndMult, 1.0f, 1.0f, 1.0f, 0.0f );
	cls.sizeStart					= 1.5f;
	cls.sizeEnd						= 10.0f;
	cls.drag						= 0.8f;
	cls.alphaParm.calc				= PARM_LINEAR;
	cls.alphaParm.val0				= 1.0f;
	cls.alphaParm.val1				= 0.0f;
	cgs.media.atmosphereBreathClass = (qhandle_t)CG_RegisterParticleClass( "atmosphere_breath", &cls );

	memset( &cls, 0, sizeof( cls ) );
	cls.shader			 = cgs.media.smokePuffShader;
	cls.emitMode		 = EMIT_POINT;
	cls.scatterShape	 = SCATTER_SPHERE;
	cls.scatterMagnitude = 18.0f;
	cls.velocityShape	 = VEL_PURE_CUBE;
	cls.cubeJitter		 = 2.0f;
	cls.velocityBias[2]	 = 1.5f;
	cls.lifetimeMean	 = 3.0f;
	cls.lifetimeJitter	 = 0.6f;
	cls.paletteCount	 = 1;
	Vector4Set( cls.colorPalette[0], 0.72f, 0.80f, 0.86f, 0.20f );
	Vector4Set( cls.colorEndMult, 1.0f, 1.0f, 1.0f, 0.0f );
	cls.sizeStart				  = 8.0f;
	cls.sizeEnd					  = 30.0f;
	cls.drag					  = 1.4f;
	cls.alphaParm.calc			  = PARM_LINEAR;
	cls.alphaParm.val0			  = 1.0f;
	cls.alphaParm.val1			  = 0.0f;
	cgs.media.atmosphereMistClass = (qhandle_t)CG_RegisterParticleClass( "atmosphere_mist", &cls );

	memset( &cls, 0, sizeof( cls ) );
	cls.shader			 = cgs.media.whiteShader;
	cls.renderFlags		 = PRIM_FLAG_ADDITIVE;
	cls.emitMode		 = EMIT_POINT;
	cls.scatterShape	 = SCATTER_SPHERE;
	cls.scatterMagnitude = 48.0f;
	cls.velocityShape	 = VEL_AXIAL_PLUS_CUBE;
	cls.axialSpeed		 = 38.0f;
	cls.cubeJitter		 = 18.0f;
	cls.lifetimeMean	 = 1.5f;
	cls.lifetimeJitter	 = 0.4f;
	cls.paletteCount	 = 1;
	Vector4Set( cls.colorPalette[0], 0.55f, 0.46f, 0.34f, 0.45f );
	Vector4Set( cls.colorEndMult, 1.0f, 1.0f, 1.0f, 0.0f );
	cls.sizeStart					= 1.0f;
	cls.sizeEnd						= 2.5f;
	cls.gravityScale				= 0.15f;
	cls.drag						= 0.3f;
	cgs.media.atmosphereDebrisClass = (qhandle_t)CG_RegisterParticleClass( "atmosphere_debris", &cls );

#define REGISTER_ATMOSPHERE_PROFILE( handle, classHandle, rate, maxCount, life, farLod, radius ) \
	do {                                                                                         \
		memset( &profile, 0, sizeof( profile ) );                                                \
		profile.schemaVersion			 = WIRED_ATMOSPHERE_EFFECT_PROFILE_SCHEMA_VERSION;       \
		profile.stageCount				 = 1u;                                                   \
		profile.maxParticles			 = ( maxCount );                                         \
		profile.seed					 = ( handle ) * 2654435761u;                             \
		profile.lodFar					 = ( farLod );                                           \
		profile.boundsRadius			 = ( radius );                                           \
		profile.stages[0].trigger		 = ATMOSPHERE_STAGE_CONTINUOUS;                          \
		profile.stages[0].particleClass	 = (uint32_t)( classHandle );                            \
		profile.stages[0].parentStage	 = UINT32_MAX;                                           \
		profile.stages[0].maxParticles	 = ( maxCount );                                         \
		profile.stages[0].spawnRate		 = ( rate );                                             \
		profile.stages[0].duration		 = ( life );                                             \
		profile.stages[0].lodFar		 = ( farLod );                                           \
		profile.stages[0].boundsRadius	 = ( radius );                                           \
		profile.stages[0].intensityScale = 1.0f;                                                 \
		if ( ( classHandle ) > 0 )                                                               \
			trap_R_RegisterAtmosphereEffectProfile( ( handle ), &profile );                      \
	} while ( 0 )

	REGISTER_ATMOSPHERE_PROFILE( CG_ATMOSPHERE_PROFILE_BREATH, cgs.media.atmosphereBreathClass, 7.0f, 64u, 1.6f, 768.0f,
								 40.0f );
	REGISTER_ATMOSPHERE_PROFILE( CG_ATMOSPHERE_PROFILE_MIST, cgs.media.atmosphereMistClass, 18.0f, 256u, 3.6f, 1400.0f,
								 180.0f );
	REGISTER_ATMOSPHERE_PROFILE( CG_ATMOSPHERE_PROFILE_DEBRIS, cgs.media.atmosphereDebrisClass, 28.0f, 256u, 2.0f,
								 1800.0f, 220.0f );

#undef REGISTER_ATMOSPHERE_PROFILE
}
