/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.
Copyright (C) 2024 Wired engine contributors

This file is part of the Wired Engine (derived from idTech 3 & 4 source
code and community around it). It is free software released under the terms
of the GNU General Public License version 2 or (at your option) any later
version.

Quake III Arena, q3now, Wired Engine and the rest are licensed under the
**GNU General Public License, version 2 or later (GPL-2.0-or-later)**.
The full license text is in `LICENSE` and `THIRD_PARTY_LICENSES.md` at the
repository root.
===========================================================================
*/

#include "tr_local.h"

static int			r_firstSceneDrawSurf;
#ifdef USE_PMLIGHT
static int			r_firstSceneLitSurf;
#endif

int			r_numdlights;
static int			r_firstSceneDlight;

static int			r_numentities;
static int			r_firstSceneEntity;

static int			r_numpolys;
static int			r_firstScenePoly;

static int			r_numpolyverts;


/*
====================
R_InitNextFrame

====================
*/
void R_InitNextFrame( void ) {

	backEndData->commands.used = 0;

	r_firstSceneDrawSurf = 0;
#ifdef USE_PMLIGHT
	r_firstSceneLitSurf = 0;
#endif

	r_numdlights = 0;
	r_firstSceneDlight = 0;

	r_numentities = 0;
	r_firstSceneEntity = 0;

	r_numpolys = 0;
	r_firstScenePoly = 0;

	r_numpolyverts = 0;

#if FEAT_CORONA
	R_ClearCoronas();
#endif
}


/*
====================
RE_ClearScene

====================
*/
void RE_ClearScene( void ) {
	r_firstSceneDlight = r_numdlights;
	r_firstSceneEntity = r_numentities;
	r_firstScenePoly = r_numpolys;
}

/*
===========================================================================

DISCRETE POLYS

===========================================================================
*/

/*
=====================
R_AddPolygonSurfaces

Adds all the scene's polys into this view's drawsurf list
=====================
*/
void R_AddPolygonSurfaces( void ) {
	int			i;
	shader_t	*sh;
	const srfPoly_t	*poly;

	tr.currentEntityNum = REFENTITYNUM_WORLD;
	tr.shiftedEntityNum = tr.currentEntityNum << QSORT_REFENTITYNUM_SHIFT;

	for ( i = 0, poly = tr.refdef.polys; i < tr.refdef.numPolys ; i++, poly++ ) {
		sh = R_GetShaderByHandle( poly->hShader );
		R_AddDrawSurf( ( void * )poly, sh, poly->fogIndex, 0 );
	}
}

/*
=====================
RE_AddPolyToScene

=====================
*/
void RE_AddPolyToScene( qhandle_t hShader, int numVerts, const polyVert_t *verts, int numPolys ) {
	srfPoly_t	*poly;
	int			i, j;
	int			fogIndex;
	const fog_t		*fog;
	vec3_t		bounds[2];

	if ( !tr.registered ) {
		return;
	}
#if 0
	if ( !hShader ) {
		ri.Log( SEV_WARN, "WARNING: RE_AddPolyToScene: NULL poly shader\n");
		return;
	}
#endif
	for ( j = 0; j < numPolys; j++ ) {
		if ( r_numpolyverts + numVerts > max_polyverts || r_numpolys >= max_polys ) {
      /*
      NOTE TTimo this was initially a PRINT_WARNING
      but it happens a lot with high fighting scenes and particles
      since we don't plan on changing the const and making for room for those effects
      simply cut this message to developer only
      */
			ri.Log( SEV_DEBUG, "WARNING: RE_AddPolyToScene: r_max_polys or r_max_polyverts reached\n");
			return;
		}

		poly = &backEndData->polys[r_numpolys];
		poly->surfaceType = SF_POLY;
		poly->hShader = hShader;
		poly->numVerts = numVerts;
		poly->verts = &backEndData->polyVerts[r_numpolyverts];

		memcpy( poly->verts, &verts[numVerts*j], numVerts * sizeof( *verts ) );
#if 0
		if ( glConfig.hardwareType == GLHW_RAGEPRO ) {
			poly->verts->modulate[0] = 255;
			poly->verts->modulate[1] = 255;
			poly->verts->modulate[2] = 255;
			poly->verts->modulate[3] = 255;
		}
#endif
		// done.
		r_numpolys++;
		r_numpolyverts += numVerts;

		// if no world is loaded
		if ( tr.world == NULL ) {
			fogIndex = 0;
		}
		// see if it is in a fog volume
		else if ( tr.world->numfogs == 1 ) {
			fogIndex = 0;
		} else {
			// find which fog volume the poly is in
			VectorCopy( poly->verts[0].xyz, bounds[0] );
			VectorCopy( poly->verts[0].xyz, bounds[1] );
			for ( i = 1 ; i < poly->numVerts ; i++ ) {
				AddPointToBounds( poly->verts[i].xyz, bounds[0], bounds[1] );
			}
			for ( fogIndex = 1 ; fogIndex < tr.world->numfogs ; fogIndex++ ) {
				fog = &tr.world->fogs[fogIndex];
				if ( bounds[1][0] >= fog->bounds[0][0]
					&& bounds[1][1] >= fog->bounds[0][1]
					&& bounds[1][2] >= fog->bounds[0][2]
					&& bounds[0][0] <= fog->bounds[1][0]
					&& bounds[0][1] <= fog->bounds[1][1]
					&& bounds[0][2] <= fog->bounds[1][2] ) {
					break;
				}
			}
			if ( fogIndex == tr.world->numfogs ) {
				fogIndex = 0;
			}
		}
		poly->fogIndex = fogIndex;
	}
}


//=================================================================================

static int isnan_fp( const float *f )
{
	uint32_t u = *( (uint32_t*) f );
	u = 0x7F800000 - ( u & 0x7FFFFFFF );
	return (int)( u >> 31 );
}


/*
=====================
RE_AddRefEntityToScene
=====================
*/
void RE_AddRefEntityToScene( const refEntity_t *ent, qboolean intShaderTime ) {
	if ( !tr.registered ) {
		return;
	}
	if ( r_numentities >= MAX_REFENTITIES ) {
		ri.Log( SEV_DEBUG, "RE_AddRefEntityToScene: Dropping refEntity, reached MAX_REFENTITIES\n" );
		return;
	}
	if ( isnan_fp( &ent->origin[0] ) || isnan_fp( &ent->origin[1] ) || isnan_fp( &ent->origin[2] ) ) {
		static qboolean first_time = qtrue;
		if ( first_time ) {
			first_time = qfalse;
			ri.Log( SEV_WARN, "RE_AddRefEntityToScene passed a refEntity which has an origin with a NaN component\n" );
		}
		return;
	}
	if ( (unsigned)ent->reType >= RT_MAX_REF_ENTITY_TYPE ) {
		ri.Terminate( TERM_CLIENT_DROP, "RE_AddRefEntityToScene: bad reType %i", ent->reType );
	}

	backEndData->entities[r_numentities].e = *ent;
	backEndData->entities[r_numentities].lightingCalculated = qfalse;
	backEndData->entities[r_numentities].intShaderTime = intShaderTime;

	r_numentities++;
}


/*
=====================
RE_AddDynamicLightToScene
=====================
*/
static void RE_AddDynamicLightToScene( const vec3_t org, float intensity, float r, float g, float b, int additive ) {
	dlight_t	*dl;

	if ( !tr.registered ) {
		return;
	}
	if ( r_numdlights >= ARRAY_LEN( backEndData->dlights ) ) {
		return;
	}
	if ( intensity <= 0 ) {
		return;
	}
#ifndef USE_VULKAN
	// these cards don't have the correct blend mode
	if ( glConfig.hardwareType == GLHW_RIVA128 || glConfig.hardwareType == GLHW_PERMEDIA2 ) {
		return;
	}
#endif
#ifdef USE_PMLIGHT
#ifdef USE_LEGACY_DLIGHTS
	if ( r_dlightMode->integer )
#endif
	{
		r *= r_dlightIntensity->value;
		g *= r_dlightIntensity->value;
		b *= r_dlightIntensity->value;
		intensity *= r_dlightScale->value;
	}
#endif

	if ( r_dlightSaturation->value != 1.0 )
	{
		float luminance = LUMA( r, g, b );
		r = LERP( luminance, r, r_dlightSaturation->value );
		g = LERP( luminance, g, r_dlightSaturation->value );
		b = LERP( luminance, b, r_dlightSaturation->value );
	}

	dl = &backEndData->dlights[r_numdlights++];
	VectorCopy( org, dl->origin );
	dl->radius = intensity;
	dl->color[0] = r;
	dl->color[1] = g;
	dl->color[2] = b;
	dl->additive = additive;
	dl->linear = qfalse;
}


/*
=====================
RE_AddLinearLightToScene
=====================
*/
void RE_AddLinearLightToScene( const vec3_t start, const vec3_t end, float intensity, float r, float g, float b  ) {
	dlight_t	*dl;
	if ( VectorCompare( start, end ) ) {
		RE_AddDynamicLightToScene( start, intensity, r, g, b, 0 );
		return;
	}
	if ( !tr.registered ) {
		return;
	}
	if ( r_numdlights >= ARRAY_LEN( backEndData->dlights ) ) {
		return;
	}
	if ( intensity <= 0 ) {
		return;
	}
#ifdef USE_PMLIGHT
#ifdef USE_LEGACY_DLIGHTS
	if ( r_dlightMode->integer )
#endif
	{
		r *= r_dlightIntensity->value;
		g *= r_dlightIntensity->value;
		b *= r_dlightIntensity->value;
		intensity *= r_dlightScale->value;
	}
#endif

	if ( r_dlightSaturation->value != 1.0 )
	{
		float luminance = LUMA( r, g, b );
		r = LERP( luminance, r, r_dlightSaturation->value );
		g = LERP( luminance, g, r_dlightSaturation->value );
		b = LERP( luminance, b, r_dlightSaturation->value );
	}

	dl = &backEndData->dlights[ r_numdlights++ ];
	VectorCopy( start, dl->origin );
	VectorCopy( end, dl->origin2 );
	dl->radius = intensity;
	dl->color[0] = r;
	dl->color[1] = g;
	dl->color[2] = b;
	dl->additive = 0;
	dl->linear = qtrue;
}


/*
=====================
Primitive submission (wired/render).

Renderer-side handlers for trap_R_Add*ToScene / trap_R_EmitParticles.
The ribbon path is wired to the renderervk ribbon pipeline; the
others remain stubs until their respective pipelines land.
=====================
*/
void RE_AddRibbonToScene( const ribbonDesc_t *desc ) {
	uint32_t frame, dstPointBase, dstHeaderIdx;
	byte *dstPoints, *dstHeader;
	uint32_t *udst;
	float    *fdst;

	// Validation. Drop silently on any failure — the renderer never
	// owns the right to log per-frame from this path. Cgame guarantees
	// the descriptor is valid by the time it reaches this trap; the
	// guards here are belt-and-braces against engine-side bugs.
	if ( !vk.ribbon.available )
		return;
	if ( desc == NULL || desc->points == NULL )
		return;
	if ( desc->numPoints < 2 || desc->numPoints > RIBBON_MAX_POINTS )
		return;

	// Bounds-check the per-frame ring buffers.
	if ( vk.ribbon.numHeadersThisFrame >= RIBBON_HEADERS_PER_FRAME )
		return;
	if ( vk.ribbon.numPointsThisFrame + (uint32_t)desc->numPoints
	     > RIBBON_POINTS_PER_FRAME )
		return;

	frame        = vk.cmd_index;
	dstPointBase = vk.ribbon.numPointsThisFrame;
	dstHeaderIdx = vk.ribbon.numHeadersThisFrame;

	// Append points. The host ribbonPoint_t struct (vec3 pos + float
	// width + vec4 rgba = 32 B) is layout-compatible with the GPU
	// RibbonPoint (vec4 posW + vec4 rgba, std430), so a memcpy is
	// safe — pos[0..2] lands in posW.xyz and `width` in posW.w
	// without reinterpret-cast tricks.
	dstPoints = vk.ribbon.points_ptr[frame] + dstPointBase * RIBBON_POINT_BYTES;
	memcpy( dstPoints, desc->points, (size_t)desc->numPoints * RIBBON_POINT_BYTES );

	// Append header.
	// Layout (24 B std430, must match ribbon.vert RibbonHeader):
	//   offset  0..15  4 × uint  (pointOffset, pointCount, shaderHandle, flags)
	//   offset 16..23  vec2      uvScroll
	// Ribbon is transient-only; uvScroll references the absolute frame
	// clock (frameParams.y) directly, so no per-submission spawnTime is
	// needed (Phase 5U dropped the dormant field). For a future
	// persistent ribbon variant, restore the field here and in
	// ribbon.vert's RibbonHeader (mirror beam's flag-branch pattern).
	dstHeader = vk.ribbon.headers_ptr[frame] + dstHeaderIdx * RIBBON_HEADER_BYTES;
	udst = (uint32_t *)dstHeader;
	fdst = (float    *)dstHeader;
	udst[0] = dstPointBase;
	udst[1] = (uint32_t)desc->numPoints;
	// Phase 5K: cgame submits a qhandle; the GPU header carries a
	// primitive registry slot. Translate via the indirection table.
	udst[2] = vk_qhandle_to_prim_slot( desc->shader );
	udst[3] = (uint32_t)desc->flags;
	fdst[4] = desc->uvScroll[0];
	fdst[5] = desc->uvScroll[1];

	vk.ribbon.numPointsThisFrame  += (uint32_t)desc->numPoints;
	vk.ribbon.numHeadersThisFrame += 1;
}

/*
=====================
RE_AddBeamToScene

Append one beam descriptor to the engine-managed pool. The pool
holds both transient (one-frame, duration == 0) and persistent
(lifetime + fade, duration > 0) beams in a single BEAM_POOL_MAX-slot
array; RB_DrawBeams walks the pool each frame, resolves
entity-attached endpoints into world space, computes fade alpha for
persistent slots, writes a compacted run of GPU headers to the
per-frame SSBO, and issues a single vkCmdDraw.

Pool exhaustion drops the submission silently — at the current
BEAM_POOL_MAX (128, post-Phase 5P), this is rare in practice. The
heaviest current consumer is LG primary at 2 slots per firing player
(main body + tapered tail); 16-player matches with all firing land
~70 slots used, comfortably within budget. A noisy log on overflow
would be more disruptive than helpful.

Validates handle / range; on any failure, the pool stays unchanged.
=====================
*/
void RE_AddBeamToScene( const beamDesc_t *desc ) {
	int slot;
	int copies;

	if ( !tr.registered ) return;
	if ( desc == NULL ) return;
	if ( !vk.beam.available ) return;

	// Linear scan for a free slot. 64 entries; sub-microsecond cost.
	for ( slot = 0; slot < (int)BEAM_POOL_MAX; slot++ ) {
		if ( !vk.beam.active[slot] ) break;
	}
	if ( slot == (int)BEAM_POOL_MAX ) {
		// Pool full — drop silently.
		return;
	}

	// Clamp axialCopies to [1, BEAM_AXIAL_MAX]. Out-of-range values
	// from older or buggy callers degrade gracefully.
	copies = desc->axialCopies;
	if ( copies < 1 )                    copies = 1;
	if ( copies > (int)BEAM_AXIAL_MAX )  copies = (int)BEAM_AXIAL_MAX;

	// Copy the descriptor into the pool slot. Persistent metadata
	// is captured even for transient beams (duration == 0); the
	// lifetime check in RB_DrawBeams handles both uniformly via
	// the duration == 0 branch. spawnTime uses the current frame's
	// floatTime so persistent beams' fadeIn ramp is anchored at
	// submission time.
	vk.beam.desc[slot]              = *desc;
	vk.beam.desc[slot].axialCopies  = copies;
	// PRIM_FLAG_TRANSIENT is engine-managed, derived from duration.
	// Mask any caller-set value (cgame should not set this bit) then
	// re-set it ourselves based on duration. Other flag bits pass
	// through unchanged. Without this, transient beams' uvScroll is
	// non-functional because age == 0 every per-frame submit; the
	// vertex shader needs the bit to know to use absolute frame time
	// as the scroll reference instead.
	vk.beam.desc[slot].flags &= ~PRIM_FLAG_TRANSIENT;
	if ( desc->duration <= 0.0f ) {
		vk.beam.desc[slot].flags |= PRIM_FLAG_TRANSIENT;
	}
	// spawnTime uses tr.refdef.floatTime (front-end current-frame
	// time, set in RE_BeginScene) rather than backEnd.refdef.floatTime
	// — the back-end's refdef holds the PREVIOUS frame's value at
	// the moment RE_AddBeamToScene runs (re_AddBeamToScene is invoked
	// during cgame frame processing, before commands are flushed to
	// the back-end). Using the front-end value keeps spawnTime
	// aligned with what RB_DrawBeams will see as the current draw-
	// frame's tr.refdef.floatTime, so age = 0 on the first render
	// and fadeIn ramps cleanly from there.
	//
	// For transient beams the value is still captured (kept uniform
	// with the persistent path to avoid branchy host code), but the
	// vertex shader ignores it because PRIM_FLAG_TRANSIENT routes
	// uvScroll to absolute frameParams.y.
	vk.beam.spawnTime[slot]         = (float)tr.refdef.floatTime;
	vk.beam.duration[slot]          = desc->duration;
	vk.beam.fadeIn[slot]            = desc->fadeIn;
	vk.beam.fadeOut[slot]           = desc->fadeOut;
	vk.beam.active[slot]            = qtrue;
}

/*
=====================
RE_AddSpriteToScene

Append one GPU SpriteHeader (std430, 48 bytes) to the per-frame
SSBO indexed by vk.cmd_index. Drops silently on validation failure
or capacity exhaustion — the renderer never owns the right to log
per-frame from this path.

GPU layout (must match sprite.vert):
    bytes  0..15  vec4  originW       (.xyz=position, .w=radius)
    bytes 16..31  vec4  rgba
    bytes 32..35  uint  shaderHandle  (reserved)
    bytes 36..39  uint  flags         (PRIM_FLAG_*)
    bytes 40..47  uint[2] padding to std430 vec4 alignment
=====================
*/
void RE_AddSpriteToScene( const spriteDesc_t *desc ) {
	uint32_t  frame, idx;
	byte     *dst;
	float    *fdst;
	uint32_t *udst;

	if ( !vk.sprite.available )
		return;
	if ( desc == NULL || desc->radius <= 0.0f )
		return;

	if ( vk.sprite.numThisFrame >= SPRITES_PER_FRAME )
		return;

	frame = vk.cmd_index;
	idx   = vk.sprite.numThisFrame;
	dst   = vk.sprite.headers_ptr[frame] + idx * SPRITE_HEADER_BYTES;
	fdst  = (float    *)dst;
	udst  = (uint32_t *)dst;

	// originW.xyz = origin, originW.w = radius
	fdst[0] = desc->origin[0];
	fdst[1] = desc->origin[1];
	fdst[2] = desc->origin[2];
	fdst[3] = desc->radius;

	// rgba (already float [0..1] per primitives.h convention)
	fdst[4] = desc->rgba[0];
	fdst[5] = desc->rgba[1];
	fdst[6] = desc->rgba[2];
	fdst[7] = desc->rgba[3];

	// Phase 5K: cgame qhandle → primitive registry slot translation.
	udst[8]  = vk_qhandle_to_prim_slot( desc->shader );
	udst[9]  = (uint32_t)desc->flags;
	udst[10] = 0; // pad0
	udst[11] = 0; // pad1

	vk.sprite.numThisFrame += 1;
}

// ── particle emit helpers ────────────────────────────────────────
//
// File-local scatter / velocity helpers. Each scatter helper writes
// a per-axis offset into out_offset; caller adds that to a base
// position. Each velocity helper writes a velocity into out_vel.
// random() / crandom() come from q_shared.h ([0, 1) and [-1, +1]
// respectively); PerpendicularVector and CrossProduct are q_math.

static void Particle_ScatterNone( vec3_t out_offset ) {
	VectorClear( out_offset );
}

static void Particle_ScatterCube( float mag, vec3_t out_offset ) {
	out_offset[0] = crandom() * mag;
	out_offset[1] = crandom() * mag;
	out_offset[2] = crandom() * mag;
}

static void Particle_ScatterSphere( float mag, vec3_t out_offset ) {
	vec3_t v;
	// Rejection sampling for uniform point in unit sphere. Average
	// ~1.91 iterations per call; tighter than Marsaglia for vec3.
	do {
		v[0] = crandom();
		v[1] = crandom();
		v[2] = crandom();
	} while ( DotProduct( v, v ) > 1.0f );
	VectorScale( v, mag, out_offset );
}

static void Particle_ScatterPerpDisc( float mag, const vec3_t axis, vec3_t out_offset ) {
	vec3_t right, up;
	float u, v;

	PerpendicularVector( right, axis );
	CrossProduct( axis, right, up );
	VectorNormalize( right );
	VectorNormalize( up );

	// Rejection in unit disc, then scale.
	do {
		u = crandom();
		v = crandom();
	} while ( u * u + v * v > 1.0f );

	VectorScale( right, u * mag, out_offset );
	VectorMA( out_offset, v * mag, up, out_offset );
}

static void Particle_VelocityAxial( const vec3_t axis, float axialSpeed, vec3_t out_vel ) {
	VectorScale( axis, axialSpeed, out_vel );
}

static void Particle_VelocityAxialPlusCube( const vec3_t axis, float axialSpeed,
                                            float cubeJitter, vec3_t out_vel ) {
	VectorScale( axis, axialSpeed, out_vel );
	out_vel[0] += crandom() * cubeJitter;
	out_vel[1] += crandom() * cubeJitter;
	out_vel[2] += crandom() * cubeJitter;
}

static void Particle_VelocityCone( const vec3_t axis, float axialSpeed,
                                   float coneHalfAngle, vec3_t out_vel ) {
	float cosHalf = cosf( coneHalfAngle );
	float z       = cosHalf + ( 1.0f - cosHalf ) * random();
	float phi     = random() * 2.0f * (float)M_PI;
	float r       = sqrtf( 1.0f - z * z );
	vec3_t right, up;

	PerpendicularVector( right, axis );
	CrossProduct( axis, right, up );
	VectorNormalize( right );
	VectorNormalize( up );

	// Uniform on spherical cap of half-angle coneHalfAngle around
	// axis; magnitude = axialSpeed.
	VectorScale( axis,                  z * axialSpeed,           out_vel );
	VectorMA  ( out_vel, r * cosf( phi ) * axialSpeed, right, out_vel );
	VectorMA  ( out_vel, r * sinf( phi ) * axialSpeed, up,    out_vel );
}

static void Particle_VelocityPureCube( float cubeJitter, vec3_t out_vel ) {
	out_vel[0] = crandom() * cubeJitter;
	out_vel[1] = crandom() * cubeJitter;
	out_vel[2] = crandom() * cubeJitter;
}

void RE_EmitParticles( const emitterDesc_t *desc ) {
	const particleClassGPU_t *gpuClasses;
	const particleClassGPU_t *cls;
	particleGPU_t *pool;
	int            i;
	uint32_t       pingRead;

	if ( !vk.particle.available ) return;
	if ( desc == NULL ) return;
	if ( desc->cls < 1
	  || (uint32_t)desc->cls > vk.particle.numClasses ) return;
	if ( desc->count <= 0 ) return;

	gpuClasses = (const particleClassGPU_t *)vk.particle.classes_ptr;
	cls        = &gpuClasses[ desc->cls - 1 ];

	// Emit happens during cgame sim, BEFORE vk_begin_frame's
	// compute dispatch + flip. At emit time, pingPongRead points to
	// the buffer last frame's compute wrote; this frame's compute
	// will read it (integrating these new particles by one frame
	// before the first render), then write to 1-pingPongRead.
	pingRead = vk.particle.pingPongRead;
	pool     = (particleGPU_t *)vk.particle.pool_ptr[ pingRead ];

	for ( i = 0; i < desc->count; i++ ) {
		particleGPU_t p;
		vec3_t        basePos, scatterOff, vel;
		float         lifetime, lifetimeInv;
		uint32_t      slot;

		memset( &p, 0, sizeof( p ) );

		// Position: emit mode picks the base point along the
		// origin→end path (or just origin), scatter shape adds
		// an offset from there.
		if ( cls->emitMode == EMIT_POINT ) {
			VectorCopy( desc->origin, basePos );
		} else {  // EMIT_PATH
			float t = random();
			basePos[0] = desc->origin[0] + t * ( desc->end[0] - desc->origin[0] );
			basePos[1] = desc->origin[1] + t * ( desc->end[1] - desc->origin[1] );
			basePos[2] = desc->origin[2] + t * ( desc->end[2] - desc->origin[2] );
		}

		switch ( cls->scatterShape ) {
			case SCATTER_NONE:
				Particle_ScatterNone( scatterOff );
				break;
			case SCATTER_CUBE:
				Particle_ScatterCube( cls->scatterMagnitude, scatterOff );
				break;
			case SCATTER_SPHERE:
				Particle_ScatterSphere( cls->scatterMagnitude, scatterOff );
				break;
			case SCATTER_PERP_DISC:
				Particle_ScatterPerpDisc( cls->scatterMagnitude,
				                          desc->axis, scatterOff );
				break;
			default:
				VectorClear( scatterOff );
				break;
		}

		VectorAdd( basePos, scatterOff, p.pos );

		// Per-particle effective axial speed. speedJitter defaults to
		// zero, in which case effectiveAxialSpeed == axialSpeed and
		// pre-extension behavior is byte-identical. Picked once per
		// particle so the shape helpers themselves remain stateless.
		// VEL_PURE_CUBE has no axial component, so the pick is wasted
		// for that shape — cheap enough to compute unconditionally
		// and avoid a switch on shape just to skip the rand call.
		{
			float effectiveAxialSpeed = cls->axialSpeed
			                          + crandom() * cls->speedJitter;

			switch ( cls->velocityShape ) {
				case VEL_AXIAL:
					Particle_VelocityAxial( desc->axis, effectiveAxialSpeed, vel );
					break;
				case VEL_AXIAL_PLUS_CUBE:
					Particle_VelocityAxialPlusCube( desc->axis, effectiveAxialSpeed,
					                                cls->cubeJitter, vel );
					break;
				case VEL_CONE:
					Particle_VelocityCone( desc->axis, effectiveAxialSpeed,
					                       cls->coneHalfAngle, vel );
					break;
				case VEL_PURE_CUBE:
					Particle_VelocityPureCube( cls->cubeJitter, vel );
					break;
				default:
					VectorClear( vel );
					break;
			}
		}

		// Post-shape velocity bias + per-axis symmetric jitter. Both
		// default to zero (memset-zeroed in the GPU mirror when the
		// class doesn't populate them), so existing classes are
		// untouched. Asymmetric ranges express via midpoint+halfwidth:
		// CPU's "vel.z += [0, 100]" → bias=(0,0,50), jitter=(0,0,50).
		vel[0] += cls->velocityBias[0]
		        + crandom() * cls->velocityBiasJitter[0];
		vel[1] += cls->velocityBias[1]
		        + crandom() * cls->velocityBiasJitter[1];
		vel[2] += cls->velocityBias[2]
		        + crandom() * cls->velocityBiasJitter[2];

		VectorCopy( vel, p.vel );

		// Per-particle sizeStart offset, picked once at emit. Vertex
		// shader reads p.sizeJitterPick each frame and adds it to
		// c.sizeStart inside the size lerp. Classes with sizeJitter
		// == 0 store 0 here, so the lerp degenerates to the existing
		// mix(c.sizeStart, c.sizeEnd, p.age).
		p.sizeJitterPick = crandom() * cls->sizeJitter;

		// Lifetime: mean ± jitter, signed. Clamp at 1ms to avoid
		// divide-by-zero in lifetimeInv (the compute shader also
		// guards against age >= 1.0 each frame, so a pathologically
		// short lifetime just means the particle dies in 1-2 frames).
		lifetime = cls->lifetimeMean + crandom() * cls->lifetimeJitter;
		if ( lifetime < 0.001f ) lifetime = 0.001f;
		lifetimeInv = 1.0f / lifetime;

		p.age         = 0.0f;
		p.lifetimeInv = lifetimeInv;
		p.classHandle = (uint32_t)desc->cls;

		// Phase 6: per-particle palette index. Random pick in
		// [0, paletteCount). Class's paletteCount is clamped to
		// >= 1 by RE_RegisterParticleClass; the > 1 branch
		// documents intent and avoids a no-op modulo on
		// single-palette classes.
		if ( cls->paletteCount > 1 ) {
			p.paletteIndex = (uint32_t)( rand() % (int)cls->paletteCount );
		} else {
			p.paletteIndex = 0;
		}
		// p.pad1..pad2 stay zero from the memset.

		// Round-robin slot allocation. Wrap-around overwrites the
		// oldest particle (which is either dead or near-end-of-life
		// given pool size 16384 and typical emit rates).
		slot                  = vk.particle.nextSlot;
		vk.particle.nextSlot  = ( slot + 1 ) % PARTICLES_PER_POOL;

		memcpy( &pool[ slot ], &p, sizeof( particleGPU_t ) );
	}
}

void RE_AddDecalToScene( const decalDesc_t *desc ) {
	// TODO: implement decal projection.
	(void)desc;
}

void RE_RegisterParticleClass( particleClassHandle_t handle, const particleClass_t *cls ) {
	particleClassGPU_t *gpuClasses;
	particleClassGPU_t *dst;
	int i;

	if ( !vk.particle.available ) return;
	if ( cls == NULL ) return;
	if ( handle < 1 || handle > MAX_PARTICLE_CLASSES ) return;

	// Host-coherent SSBO; direct write, no staging.
	gpuClasses = (particleClassGPU_t *)vk.particle.classes_ptr;
	dst        = &gpuClasses[ handle - 1 ];

	memset( dst, 0, sizeof( *dst ) );

	dst->shader            = (uint32_t)cls->shader;
	dst->renderFlags       = (uint32_t)cls->renderFlags;
	dst->emitMode          = (uint32_t)cls->emitMode;
	dst->scatterShape      = (uint32_t)cls->scatterShape;
	dst->scatterMagnitude  = cls->scatterMagnitude;
	dst->velocityShape     = (uint32_t)cls->velocityShape;
	dst->axialSpeed        = cls->axialSpeed;
	dst->cubeJitter        = cls->cubeJitter;
	dst->coneHalfAngle     = cls->coneHalfAngle;
	dst->lifetimeMean      = cls->lifetimeMean;
	dst->lifetimeJitter    = cls->lifetimeJitter;
	// Phase 6: clamp paletteCount to [1, PARTICLE_CLASS_MAX_PALETTE].
	// 0 → would crash RE_EmitParticles's modulo at emit time.
	// >16 → would let particle.vert read colorPalette[idx] out of
	// bounds (the GLSL array is fixed at PARTICLE_CLASS_MAX_PALETTE).
	// Silent clamp; class definitions with bad paletteCount get
	// repaired rather than rejected.
	{
		int pc = cls->paletteCount;
		if ( pc < 1 ) pc = 1;
		if ( pc > PARTICLE_CLASS_MAX_PALETTE ) pc = PARTICLE_CLASS_MAX_PALETTE;
		dst->paletteCount = pc;
	}

	for ( i = 0; i < PARTICLE_CLASS_MAX_PALETTE; i++ ) {
		dst->colorPalette[i][0] = cls->colorPalette[i][0];
		dst->colorPalette[i][1] = cls->colorPalette[i][1];
		dst->colorPalette[i][2] = cls->colorPalette[i][2];
		dst->colorPalette[i][3] = cls->colorPalette[i][3];
	}

	dst->colorEndMult[0] = cls->colorEndMult[0];
	dst->colorEndMult[1] = cls->colorEndMult[1];
	dst->colorEndMult[2] = cls->colorEndMult[2];
	dst->colorEndMult[3] = cls->colorEndMult[3];

	dst->sizeStart    = cls->sizeStart;
	dst->sizeEnd      = cls->sizeEnd;
	dst->gravityScale = cls->gravityScale;
	dst->drag         = cls->drag;
	// shaderBlendIsAdditive set below; pad1..pad3 stay zero from the memset.

	// Expressivity-extension fields. .w lanes of the two vec4s are
	// unused — only .xyz carry data. Memset above already zeroed them
	// (which is the zero-effect default for any class that does not
	// opt in), so this block only matters when the class did populate
	// these fields.
	dst->velocityBias[0]       = cls->velocityBias[0];
	dst->velocityBias[1]       = cls->velocityBias[1];
	dst->velocityBias[2]       = cls->velocityBias[2];
	dst->velocityBias[3]       = 0.0f;
	dst->velocityBiasJitter[0] = cls->velocityBiasJitter[0];
	dst->velocityBiasJitter[1] = cls->velocityBiasJitter[1];
	dst->velocityBiasJitter[2] = cls->velocityBiasJitter[2];
	dst->velocityBiasJitter[3] = 0.0f;
	dst->speedJitter           = cls->speedJitter;
	dst->sizeJitter            = cls->sizeJitter;
	// pad4, pad5 stay zero from the memset.

	// ── Phase 5: resolve class shader → image, write to sampler array.
	//
	// Three-tier fallback mirrors the IQM precedent at
	// tr_model_iqm.c:1495-1504. The resolved image_t is cached in
	// vk.particle.classImages[] so vk_init_descriptors's re-alloc
	// path can re-walk the registry after a pool reset.
	//
	// Blend mode is derived from the class shader's stages[0] state
	// bits (additive vs alpha). The vertex shader filters per blend
	// variant by reading shaderBlendIsAdditive — so the per-particle
	// PRIM_FLAG_ADDITIVE bit on cls->renderFlags is no longer
	// load-bearing for blend selection. See primitives.h.
	{
		shader_t *resolvedShader = R_GetShaderByHandle( cls->shader );
		image_t  *resolvedImage;
		uint32_t  isAdditive = 0;

		if ( resolvedShader && resolvedShader->stages[0]
		  && resolvedShader->stages[0]->bundle[0].image[0] ) {
			resolvedImage = resolvedShader->stages[0]->bundle[0].image[0];
		} else if ( tr.defaultShader && tr.defaultShader->stages[0]
		         && tr.defaultShader->stages[0]->bundle[0].image[0] ) {
			resolvedImage = tr.defaultShader->stages[0]->bundle[0].image[0];
		} else {
			resolvedImage = tr.whiteImage;
		}

		// Derive blend mode from the resolved shader's stage 0 state
		// bits. Two patterns are recognised as additive:
		//   GL_SRC_ALPHA / GL_ONE  (alpha-modulated additive — q3 sprite convention)
		//   GL_ONE       / GL_ONE  (pure additive — "blendfunc add")
		// Any other combination falls back to alpha-blend for now;
		// dst-color / modulate / etc. are out of phase-5 scope.
		if ( resolvedShader && resolvedShader->stages[0] ) {
			uint32_t blendBits = resolvedShader->stages[0]->stateBits & GLS_BLEND_BITS;
			if ( blendBits == ( GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE )
			  || blendBits == ( GLS_SRCBLEND_ONE       | GLS_DSTBLEND_ONE ) ) {
				isAdditive = 1;
			}
		}
		dst->shaderBlendIsAdditive = isAdditive;

		// Cache image pointer for re-alloc-after-pool-reset.
		vk.particle.classImages[ handle - 1 ] = resolvedImage;

		// Update slot (handle - 1) of the sampler array on every
		// per-frame render descriptor set. Helper lives in vk.c
		// because the qvk* function pointers are static there.
		vk_particle_set_class_image( handle, resolvedImage );
	}

	// Registration is monotonic in the static-init use case (handle
	// equals numClasses + 1), but tolerate re-registration as
	// overwrite without bumping the counter.
	if ( (uint32_t)handle > vk.particle.numClasses ) {
		vk.particle.numClasses = (uint32_t)handle;
	}
}


/*
=====================
RE_AddLightToScene

=====================
*/
void RE_AddLightToScene( const vec3_t org, float intensity, float r, float g, float b ) {
	RE_AddDynamicLightToScene( org, intensity, r, g, b, qfalse );
}


/*
=====================
RE_AddAdditiveLightToScene

=====================
*/
void RE_AddAdditiveLightToScene( const vec3_t org, float intensity, float r, float g, float b ) {
	RE_AddDynamicLightToScene( org, intensity, r, g, b, qtrue );
}


void *R_GetCommandBuffer( int bytes );

/*
@@@@@@@@@@@@@@@@@@@@@
RE_RenderScene

Draw a 3D view into a part of the window, then return
to 2D drawing.

Rendering a scene may require multiple views to be rendered
to handle mirrors,
@@@@@@@@@@@@@@@@@@@@@
*/
void RE_RenderScene( const refdef_t *fd ) {
#ifdef USE_VULKAN
	renderCommand_t	lastRenderCommand;
#endif
	viewParms_t		parms;
	int				startTime;

	if ( !tr.registered ) {
		return;
	}

	if ( r_norefresh->integer ) {
		return;
	}

	startTime = ri.Milliseconds();

	if (!tr.world && !( fd->rdflags & RDF_NOWORLDMODEL ) ) {
		ri.Terminate( TERM_CLIENT_DROP, "R_RenderScene: NULL worldmodel");
	}

	memcpy( tr.refdef.text, fd->text, sizeof( tr.refdef.text ) );

	tr.refdef.x = fd->x;
	tr.refdef.y = fd->y;
	tr.refdef.width = fd->width;
	tr.refdef.height = fd->height;
	tr.refdef.fov_x = fd->fov_x;
	tr.refdef.fov_y = fd->fov_y;

	VectorCopy( fd->vieworg, tr.refdef.vieworg );
	VectorCopy( fd->viewaxis[0], tr.refdef.viewaxis[0] );
	VectorCopy( fd->viewaxis[1], tr.refdef.viewaxis[1] );
	VectorCopy( fd->viewaxis[2], tr.refdef.viewaxis[2] );

	tr.refdef.time = fd->time;
	tr.refdef.rdflags = fd->rdflags;

	// copy the areamask data over and note if it has changed, which
	// will force a reset of the visible leafs even if the view hasn't moved
	tr.refdef.areamaskModified = qfalse;
	if ( ! (tr.refdef.rdflags & RDF_NOWORLDMODEL) ) {
		int		areaDiff;

		// compare the area bits
		areaDiff = 0;
		for ( int i = 0; i < MAX_MAP_AREA_BYTES/sizeof(int); i++ ) {
			areaDiff |= ((int *)tr.refdef.areamask)[i] ^ ((int *)fd->areamask)[i];
			((int *)tr.refdef.areamask)[i] = ((int *)fd->areamask)[i];
		}

		if ( areaDiff ) {
			// a door just opened or something
			tr.refdef.areamaskModified = qtrue;
		}
	}


	// derived info

	tr.refdef.floatTime = (double)tr.refdef.time * 0.001; // -EC-: cast to double

	tr.refdef.numDrawSurfs = r_firstSceneDrawSurf;
	tr.refdef.drawSurfs = backEndData->drawSurfs;

#ifdef USE_PMLIGHT
	tr.refdef.numLitSurfs = r_firstSceneLitSurf;
	tr.refdef.litSurfs = backEndData->litSurfs;
#endif

	tr.refdef.num_entities = r_numentities - r_firstSceneEntity;
	tr.refdef.entities = &backEndData->entities[r_firstSceneEntity];

	tr.refdef.num_dlights = r_numdlights - r_firstSceneDlight;
	tr.refdef.dlights = &backEndData->dlights[r_firstSceneDlight];

	tr.refdef.numPolys = r_numpolys - r_firstScenePoly;
	tr.refdef.polys = &backEndData->polys[r_firstScenePoly];

	// turn off dynamic lighting globally by clearing all the
	// dlights if it needs to be disabled
	if ( r_dynamiclight->integer == 0 || glConfig.hardwareType == GLHW_PERMEDIA2 ) {
		tr.refdef.num_dlights = 0;
	}

	// a single frame may have multiple scenes draw inside it --
	// a 3D game view, 3D status bar renderings, 3D menus, etc.
	// They need to be distinguished by the light flare code, because
	// the visibility state for a given surface may be different in
	// each scene / view.
	tr.frameSceneNum++;
	tr.sceneCount++;

	// setup view parms for the initial view
	//
	// set up viewport
	// The refdef takes 0-at-the-top y coordinates, so
	// convert to GL's 0-at-the-bottom space
	//
	memset( &parms, 0, sizeof( parms ) );
	parms.viewportX = tr.refdef.x;
	parms.viewportY = glConfig.vidHeight - ( tr.refdef.y + tr.refdef.height );
	parms.viewportWidth = tr.refdef.width;
	parms.viewportHeight = tr.refdef.height;

	parms.scissorX = parms.viewportX;
	parms.scissorY = parms.viewportY;
	parms.scissorWidth = parms.viewportWidth;
	parms.scissorHeight = parms.viewportHeight;

	parms.portalView = PV_NONE;

#ifdef USE_PMLIGHT
	parms.dlights = tr.refdef.dlights;
	parms.num_dlights = tr.refdef.num_dlights;
#endif

	parms.fovX = tr.refdef.fov_x;
	parms.fovY = tr.refdef.fov_y;

	parms.stereoFrame = tr.refdef.stereoFrame;

	VectorCopy( fd->vieworg, parms.or.origin );
	VectorCopy( fd->viewaxis[0], parms.or.axis[0] );
	VectorCopy( fd->viewaxis[1], parms.or.axis[1] );
	VectorCopy( fd->viewaxis[2], parms.or.axis[2] );

	VectorCopy( fd->vieworg, parms.pvsOrigin );

#ifdef USE_VULKAN
	lastRenderCommand = tr.lastRenderCommand;
	tr.drawSurfCmd = NULL;
	tr.numDrawSurfCmds = 0;
#endif

	R_RenderView( &parms );

#ifdef USE_VULKAN
	if ( tr.needScreenMap )
	{
		if ( lastRenderCommand == RC_DRAW_BUFFER )
		{
			// duplicate all views, including portals
			drawSurfsCommand_t *cmd, *src = NULL;

			for ( int i = 0; i < tr.numDrawSurfCmds; i++ )
			{
				cmd = R_GetCommandBuffer( sizeof( *cmd ) );
				if ( cmd )
				{
					src = tr.drawSurfCmd + i;
					*cmd = *src;
				}
				else
				{
					break;
				}
			}

			if ( src )
			{
				// first drawsurface
				tr.drawSurfCmd[0].refdef.needScreenMap = qtrue;
				// last drawsurface
				src->refdef.switchRenderPass = qtrue;
			}
		}

		tr.needScreenMap = 0;
	}
#endif

	// the next scene rendered in this frame will tack on after this one
	r_firstSceneDrawSurf = tr.refdef.numDrawSurfs;
#ifdef USE_PMLIGHT
	r_firstSceneLitSurf = tr.refdef.numLitSurfs;
#endif

	r_firstSceneEntity = r_numentities;
	r_firstSceneDlight = r_numdlights;
	r_firstScenePoly = r_numpolys;

	tr.frontEndMsec += ri.Milliseconds() - startTime;
}
