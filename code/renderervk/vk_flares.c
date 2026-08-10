// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 1999-2005 Id Software, Inc.
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
// tr_flares.c

#include "tr_local.h"

/*
=============================================================================

LIGHT FLARES

A light flare is an effect that takes place inside the eye when bright light
sources are visible.  The size of the flare relative to the screen is nearly
constant, irrespective of distance, but the intensity should be proportional to the
projected area of the light source.

A surface that has been flagged as having a light flare will calculate the depth
buffer value that its midpoint should have when the surface is added.

After all opaque surfaces have been rendered, the depth buffer is read back for
each flare in view.  If the point has not been obscured by a closer surface, the
flare should be drawn.

Surfaces that have a repeated texture should never be flagged as flaring, because
there will only be a single flare added at the midpoint of the polygon.

To prevent abrupt popping, the intensity of the flare is interpolated up and
down as it changes visibility.  This involves scene to scene state, unlike almost
all other aspects of the renderer, and is complicated by the fact that a single
frame may have multiple scenes.

RB_RenderFlares() will be called once per view (twice in a mirrored scene, potentially
up to five or more times in a frame with 3D status bar icons).

=============================================================================
*/


// flare states maintain visibility over multiple frames for fading
// layers: view, mirror, menu
typedef struct flare_s {
	struct		flare_s	*next;		// for active chain

	int			addedFrame;
	uint32_t	testCount;

	portalView_t portalView;
	int			frameSceneNum;
	void		*surface;
	int			fogNum;

	int			fadeTime;

	qboolean	visible;			// state of last test
	float		drawIntensity;		// may be non 0 even if !visible due to fading

	int			windowX, windowY;
	float		eyeZ;
	float		drawZ;

	vec3_t		origin;
	vec3_t		color;
} flare_t;

static flare_t	r_flareStructs[ MAX_FLARES ];
static flare_t	*r_activeFlares, *r_inactiveFlares;


/*
==================
R_ClearFlares
==================
*/
void R_ClearFlares( void ) {
	if ( !vk.fragmentStores )
		return;

	memset( r_flareStructs, 0, sizeof( r_flareStructs ) );
	r_activeFlares = NULL;
	r_inactiveFlares = NULL;

	for ( int i = 0 ; i < MAX_FLARES ; i++ ) {
		r_flareStructs[i].next = r_inactiveFlares;
		r_inactiveFlares = &r_flareStructs[i];
	}
}


static flare_t *R_SearchFlare( void *surface )
{
	flare_t *f;

	// see if a flare with a matching surface, scene, and view exists
	for ( f = r_activeFlares ; f ; f = f->next ) {
		if ( f->surface == surface && f->frameSceneNum == backEnd.viewParms.frameSceneNum && f->portalView == backEnd.viewParms.portalView ) {
			return f;
		}
	}

	return NULL;
}


/*
==================
RB_AddFlare

This is called at surface tesselation time
==================
*/
void RB_AddFlare( void *surface, int fogNum, vec3_t point, vec3_t color, vec3_t normal ) {
	flare_t			*f;
	vec3_t			local;
	float			d = 1;
	lensScreenProj_t	proj;

	backEnd.pc.c_flareAdds++;

	if ( normal && (normal[0] || normal[1] || normal[2] ) )	{
		VectorSubtract( backEnd.viewParms.or.origin, point, local );
		VectorNormalizeFast( local );
		d = DotProduct( local, normal );
		// If the viewer is behind the flare don't add it.
		if ( d < 0 ) {
			return;
		}
	}

	// if the point is off the screen, don't bother adding it
	// calculate screen coordinates and depth (current-entity orientation)
	R_ProjectLightToScreen( point, &backEnd.or, &backEnd.viewParms, &proj );

	// check to see if the point is completely off screen
	for ( int i = 0 ; i < 3 ; i++ ) {
		if ( proj.clip[i] >= proj.clip[3] || proj.clip[i] <= -proj.clip[3] ) {
			return;
		}
	}

	if ( proj.windowX < 0 || proj.windowX >= backEnd.viewParms.viewportWidth || proj.windowY < 0 || proj.windowY >= backEnd.viewParms.viewportHeight ) {
		return;	// shouldn't happen, since we check the clip[] above, except for FP rounding
	}

	f = R_SearchFlare( surface );

	// allocate a new one
	if ( !f ) {
		if ( !r_inactiveFlares ) {
			// the list is completely full
			return;
		}
		f = r_inactiveFlares;
		r_inactiveFlares = r_inactiveFlares->next;
		f->next = r_activeFlares;
		r_activeFlares = f;

		f->surface = surface;
		f->frameSceneNum = backEnd.viewParms.frameSceneNum;
		f->portalView = backEnd.viewParms.portalView;
		f->visible = qfalse;
		f->fadeTime = backEnd.refdef.time - 2000;
		f->testCount = 0;
	} else {
		++f->testCount;
	}

	f->addedFrame = backEnd.viewParms.frameCount;
	f->fogNum = fogNum;

	VectorCopy( point, f->origin );
	VectorCopy( color, f->color );

	// fade the intensity of the flare down as the
	// light surface turns away from the viewer
	VectorScale( f->color, d, f->color );

	// save info needed to test
	f->windowX = backEnd.viewParms.viewportX + (int)proj.windowX;
	f->windowY = backEnd.viewParms.viewportY + (int)proj.windowY;

	f->eyeZ = proj.eyeZ;

#ifdef USE_REVERSED_DEPTH
	f->drawZ = (proj.clip[2]+0.20) / proj.clip[3];
#else
	f->drawZ = (proj.clip[2]-0.20) / proj.clip[3];
#endif

}


/*
==================
RB_AddDlightFlares
==================
*/
void RB_AddDlightFlares( void ) {
	dlight_t		*l;
	int				i, j, k;
	fog_t			*fog = NULL;

	if ( !r_flares->integer ) {
		return;
	}

	l = backEnd.refdef.dlights;

	if ( tr.world )
		fog = tr.world->fogs;

	for ( i = 0 ; i < backEnd.refdef.num_dlights; i++, l++ ) {

		if ( fog )
		{
			// find which fog volume the light is in
			for ( j = 1 ; j < tr.world->numfogs ; j++ ) {
				fog = &tr.world->fogs[j];
				for ( k = 0 ; k < 3 ; k++ ) {
					if ( l->origin[k] < fog->bounds[0][k] || l->origin[k] > fog->bounds[1][k] ) {
						break;
					}
				}
				if ( k == 3 ) {
					break;
				}
			}
			if ( j == tr.world->numfogs ) {
				j = 0;
			}
		}
		else
			j = 0;

		RB_AddFlare( (void *)l, j, l->origin, l->color, NULL );
	}
}

/*
===============================================================================

FLARE BACK END

===============================================================================
*/


static float *vk_ortho( float x1, float x2,
						float y2, float y1,
						float z1, float z2 ) {

	static float m[16] = { 0 };

	m[0] = 2.0f / (x2 - x1);
	m[5] = 2.0f / (y2 - y1);
	m[10] = 1.0f / (z1 - z2);
	m[12] = -(x2 + x1) / (x2 - x1);
	m[13] = -(y2 + y1) / (y2 - y1);
	m[14] = z1 / (z1 - z2);
	m[15] = 1.0f;

	return m;
}


/*
==================
RB_AddLensSourceFlares

Project the game-registered lens sources (stashed by RE_AddLensSourceToScene this
frame) and write their oracle registry records, so the depth-sampling oracle
(vk_lens_dispatch) tests each one's visibility. Runs at render time — backEnd.viewParms
is valid here — and BEFORE the oracle dispatches. Each source maps to a stable slot in
the cgame strip [LENS_SLOT_CGSOURCES .. +LENS_MAX_CGSOURCES) via id%N so cgame reads back
the matching visibility next frame (the 1-frame delay, like flares). Disjoint from the
flare slots [0..255] and the sun slot [256] — no aliasing.
==================
*/
void RB_AddLensSourceFlares( void ) {
	int i;

	if ( !r_lens || !r_lens->integer || !vk.ral_lens_pipeline || !vk.lensSourcesPtr )
		return;
	if ( r_numLensSources <= 0 )
		return;
	if ( vk.renderPassIndex == RENDER_PASS_SCREENMAP || backEnd.isHyperspace )
		return;

	// Project in world space (the sources are world positions), matching the halo
	// path; backEnd.viewParms is valid here (render time).
	backEnd.or = backEnd.viewParms.world;

	for ( i = 0; i < r_numLensSources; i++ ) {
		lensSceneSource_t *s = &backEndData->lensSources[i];
		int    slot = LENS_SLOT_CGSOURCES + ( ( s->id % LENS_MAX_CGSOURCES + LENS_MAX_CGSOURCES ) % LENS_MAX_CGSOURCES );
		float *rec  = (float *)vk.lensSourcesPtr + (size_t)slot * ( LENS_SOURCE_VEC4S * 4 );
		lensScreenProj_t lp;

		R_ProjectLightToScreen( s->origin, &backEnd.or, &backEnd.viewParms, &lp );
		rec[0] = lp.screenU;
		rec[1] = lp.screenV;
		// Unbiased reversed-Z device depth of the source; behind-near → off-screen
		// sentinel so the oracle reports it occluded (no rays from behind the camera).
		rec[2] = ( !lp.behindNear && lp.clip[3] != 0.0f ) ? ( lp.clip[2] / lp.clip[3] ) : -1.0f;
		rec[3] = 0.0f;
		rec[4] = rec[5] = rec[6] = 0.0f;
		/* rec[7] (visibility) is the oracle's output — leave it for the compute pass */
	}

	// Cover the whole cgame strip so vk_lens_dispatch processes these slots (the
	// max(count, level) high-water pattern; reset to 0 after dispatch each frame).
	if ( vk.lensSourceCount < LENS_SLOT_CGSOURCES + LENS_MAX_CGSOURCES )
		vk.lensSourceCount = LENS_SLOT_CGSOURCES + LENS_MAX_CGSOURCES;
}


/*
==================
RB_TestFlare
==================
*/
// Lens occlusion oracle visibility for one flare. Reads this flare's per-slot
// visibility (0..1) the compute oracle wrote LAST frame into the lens SSBO, then
// writes THIS frame's record (screen UV + reversed-Z compare depth) for the oracle
// to sample at the depth-copy seam. The flare index is the stable oracle slot — the
// same per-flare addressing the dot-probe used. 1-frame coord delay, matching the
// retired dot-probe (vk.sceneDepth copy precedes RB_RenderFlares). Returns the soft
// visibility this frame (1 == fully unoccluded). Off if the oracle isn't up.
static float RB_LensOracleVisibility( flare_t *f ) {
	int    slot = (int)( f - r_flareStructs );
	float *rec;
	float  vis;

	if ( slot < 0 || slot >= LENS_MAX_SOURCES )
		return 1.0f;
	if ( !vk.lensSourcesPtr )
		return 1.0f;

	rec = (float *)vk.lensSourcesPtr + (size_t)slot * ( LENS_SOURCE_VEC4S * 4 );

	// Read last-frame visibility (rec1.w = float index 7). On the very first frame a
	// slot may be unwritten (zero) → treated as fully occluded, then the fade ramps.
	vis = ( f->testCount ) ? rec[7] : 0.0f;

	// Write this frame's record for the oracle: rec0 = {screenU, screenV, compareZ,
	// radiusPx}; rec1 cleared except the oracle overwrites .w. Project the flare origin
	// fresh through the single projection authority (P1): screenUV is the same
	// 0.5 + clip.xy*0.5/w convention the sunray pass uses to sample the depth copy
	// (known-correct for the FBO texture), and compareZ = clip.z/clip.w is the
	// UNBIASED reversed-Z device depth (NOT f->drawZ, which carries the ortho dot-test
	// bias). Behind-near sources get visibility forced off (compareZ left at 0).
	{
		lensScreenProj_t lp;
		R_ProjectLightToScreen( f->origin, &backEnd.or, &backEnd.viewParms, &lp );
		rec[0] = lp.screenU;
		rec[1] = lp.screenV;
		rec[2] = ( !lp.behindNear && lp.clip[3] != 0.0f ) ? ( lp.clip[2] / lp.clip[3] ) : -1.0f;
		rec[3] = 0.0f;
		rec[4] = rec[5] = rec[6] = 0.0f;
		/* rec[7] (visibility) is the oracle's output — leave it for the compute pass */
	}

	// Cover all possible slots so the oracle processes this one (sparse addressing,
	// like the dot-probe). lensSourceCount is reset to 0 each frame and raised here.
	if ( vk.lensSourceCount < LENS_MAX_SOURCES )
		vk.lensSourceCount = LENS_MAX_SOURCES;

	return vis;
}

static void RB_TestFlare( flare_t *f ) {
	qboolean		visible;
	float			fade;
	float			oracleVis = 1.0f;
	qboolean		oracleUp = ( vk.ral_lens_pipeline != NULL && r_lens->integer );

	backEnd.pc.c_flareTests++;

	// Depth-sampled occlusion oracle: the N-tap visibility (0..1) the compute pass
	// wrote into this flare's lens-SSBO slot last frame replaces the old per-flare
	// dot-probe (a 1-vertex ortho draw + 1-frame storage readback — the last VS-MVP
	// push consumer, retired). Mirror the dot-probe's 1-frame-delay contract: on a
	// flare's FIRST frame (testCount 0) write its record but don't fold visibility
	// yet — leave testCount 0 so RB_RenderFlares keeps it alive ("wait 1 frame for
	// test result"). Next frame the oracle has written a real visibility into the
	// slot. When the oracle is unavailable (r_lens 0, no FBO/depth copy), flares are
	// unoccluded — there is no longer a CPU fallback occlusion path.
	if ( oracleUp ) {
		if ( f->testCount == 0 ) {
			RB_LensOracleVisibility( f );   // write the record; ignore the (stale) read
			oracleVis = 1.0f;               // not folded — testCount stays 0
		} else {
			oracleVis = RB_LensOracleVisibility( f );
		}
		visible = ( oracleVis > 0.0f ) ? qtrue : qfalse;
	} else {
		visible = qtrue;   // no occlusion oracle → always visible
	}

	if ( visible ) {
		if ( !f->visible ) {
			f->visible = qtrue;
			f->fadeTime = backEnd.refdef.time - 1;
		}
		fade = ( ( backEnd.refdef.time - f->fadeTime ) /1000.0f ) * r_flareFade->value;
	} else {
		if ( f->visible ) {
			f->visible = qfalse;
			f->fadeTime = backEnd.refdef.time - 1;
		}
		fade = 1.0f - ( ( backEnd.refdef.time - f->fadeTime ) / 1000.0f ) * r_flareFade->value;
	}

	if ( fade < 0 ) {
		fade = 0;
	} else if ( fade > 1 ) {
		fade = 1;
	}

	// Fold the smooth area visibility into the fade so partial occlusion dims the
	// halo (the ⊆-superset improvement over the binary probe). The time-based fade
	// still smooths pops; the oracle factor scales the target intensity.
	if ( oracleUp )
		fade *= oracleVis;

	f->drawIntensity = fade;
}


/*
==================
RB_RenderFlare
==================
*/
static void RB_RenderFlare( flare_t *f ) {
	float			size;
	vec3_t			color;
	float distance, intensity, factor;
	byte fogFactors[3] = {255, 255, 255};
	color4ub_t		c;

	//if ( f->drawIntensity == 0.0 )
	//	return;

	backEnd.pc.c_flareRenders++;

	// We don't want too big values anyways when dividing by distance.
	if ( f->eyeZ > -1.0f )
		distance = 1.0f;
	else
		distance = -f->eyeZ;

	// calculate the flare size..
	// Pure screen-fraction: the on-screen flare size stays nearly constant with
	// distance (see the header note above); proximity is expressed through the
	// intensity falloff below, not by growing the quad.
	size = backEnd.viewParms.viewportWidth * ( r_flareSize->value / 640.0f );

/*
 * This is an alternative to intensity scaling. It changes the size of the flare on screen instead
 * with growing distance. See in the description at the top why this is not the way to go.
	// size will change ~ 1/r.
	size = backEnd.viewParms.viewportWidth * (r_flareSize->value / (distance * -2.0f));
*/

/*
 * As flare sizes stay nearly constant with increasing distance we must decrease the intensity
 * to achieve a reasonable visual result. The intensity is ~ (size^2 / distance^2) which can be
 * got by considering the ratio of
 * (flaresurface on screen) : (Surface of sphere defined by flare origin and distance from flare)
 * An important requirement is:
 * intensity <= 1 for all distances.
 *
 * The formula used here to compute the intensity is as follows:
 * intensity = flareCoeff * size^2 / (distance + size*sqrt(flareCoeff))^2
 * As you can see, the intensity will have a max. of 1 when the distance is 0.
 * The coefficient flareCoeff will determine the falloff speed with increasing distance.
 */

	factor = distance + size * sqrt( r_flareCoeff->value );

	intensity = r_flareCoeff->value * size * size / ( factor * factor );

	// The flare is composited additively into the HDR scene buffer, which the
	// tonemap then multiplies by exposure_bias (tonemap.frag). Pre-dividing the
	// flare colour by that same exposure_bias cancels the downstream multiply, so
	// the flare lands at a fixed post-exposure operating point (r_flareTarget)
	// regardless of the auto-exposure state — a dark room and a bright outdoor
	// scene read the flare at the same display brightness instead of blowing it
	// out to flat white. Read the host-coherent exposure UBO the same way the
	// tonemap-fill path does; fall back to 1.0 when there is no FBO/HDR buffer.
	{
		float exposureScale = 1.0f;
		if ( vk.fboActive && vk.exposure.ptr[ vk.cmd_index ] ) {
			exposureScale = ((vk_exposure_block_t *)vk.exposure.ptr[ vk.cmd_index ])->exposure_bias;
			if ( exposureScale < 0.001f )
				exposureScale = 0.001f;
		}
		VectorScale( f->color, f->drawIntensity * intensity * ( r_flareTarget->value / exposureScale ), color );
	}

	// Calculations for fogging
	if ( tr.world && f->fogNum > 0 && f->fogNum < tr.world->numfogs )
	{
		tess.numVertexes = 1;
		VectorCopy( f->origin, tess.xyz[0] );
		tess.fogNum = f->fogNum;

		RB_CalcModulateColorsByFog( fogFactors );

		// We don't need to render the flare if colors are 0 anyways.
		if ( !(fogFactors[0] || fogFactors[1] || fogFactors[2]) )
			return;
	}

	RB_BeginSurface( tr.flareShader, f->fogNum );

	c.rgba[0] = color[0] * fogFactors[0];
	c.rgba[1] = color[1] * fogFactors[1];
	c.rgba[2] = color[2] * fogFactors[2];
	c.rgba[3] = 255;

	RB_AddQuadStamp2( f->windowX - size, f->windowY - size, size * 2, size * 2, 0, 0, 1, 1, c );

	RB_EndSurface();
}


/*
==================
RB_RenderFlares

Because flares are simulating an occular effect, they should be drawn after
everything (all views) in the entire frame has been drawn.

Because of the way portals use the depth buffer to mark off areas, the
needed information would be lost after each view, so we are forced to draw
flares after each view.

The resulting artifact is that flares in mirrors or portals don't dim properly
when occluded by something in the main view, and portal flares that should
extend past the portal edge will be overwritten.
==================
*/
void RB_RenderFlares( void ) {
	flare_t		*f;
	flare_t		**prev;
	qboolean	draw;
	float		*m;

	// halos funnel through this same pump (RB_AddHaloFlares), so it must run
	// when either surface flares (r_flares) or halos (r_halos) are enabled.
	if ( !r_flares->integer && !r_halos->integer ) {
		return;
	}

	if ( vk.renderPassIndex == RENDER_PASS_SCREENMAP ) {
		return;
	}

	if ( backEnd.isHyperspace ) {
		return;
	}

	// Reset currentEntity to world so that any previously referenced entities
	// don't have influence on the rendering of these flares (i.e. RF_ renderer flags).
	backEnd.currentEntity = &tr.worldEntity;
	backEnd.or = backEnd.viewParms.world;

	//RB_AddDlightFlares();

#if FEAT_HALO
	RB_AddHaloFlares();
#endif

	// perform occlusion test on each flare in this view (the lens oracle wrote the
	// visibility into each flare's lens-SSBO slot last frame; RB_TestFlare folds it).
	draw = qfalse;
	prev = &r_activeFlares;
	while ( ( f = *prev ) != NULL ) {
		// throw out any flares that weren't added last frame
		if ( backEnd.viewParms.frameCount - f->addedFrame > 0 && f->portalView == backEnd.viewParms.portalView ) {
			*prev = f->next;
			f->next = r_inactiveFlares;
			r_inactiveFlares = f;
			continue;
		}

		// don't draw any here that aren't from this scene / portal
		f->drawIntensity = 0;
		if ( f->frameSceneNum == backEnd.viewParms.frameSceneNum && f->portalView == backEnd.viewParms.portalView ) {
			RB_TestFlare( f );
			if ( f->testCount == 0 ) {
				// recently added, wait 1 frame for test result
			} else if ( f->drawIntensity ) {
				draw = qtrue;
			} else {
				// this flare has completely faded out, so remove it from the chain
				*prev = f->next;
				f->next = r_inactiveFlares;
				r_inactiveFlares = f;
				continue;
			}
		}

		prev = &f->next;
	}

	if ( !draw ) {
		return;		// none visible
	}

#ifdef USE_REVERSED_DEPTH
	m = vk_ortho( backEnd.viewParms.viewportX, backEnd.viewParms.viewportX + backEnd.viewParms.viewportWidth,
		backEnd.viewParms.viewportY, backEnd.viewParms.viewportY + backEnd.viewParms.viewportHeight, 1.0, 0.0 );
#else
	m = vk_ortho( backEnd.viewParms.viewportX, backEnd.viewParms.viewportX + backEnd.viewParms.viewportWidth,
		backEnd.viewParms.viewportY, backEnd.viewParms.viewportY + backEnd.viewParms.viewportHeight, 0.0, 1.0 );
#endif

	vk_update_mvp( m );

	for ( f = r_activeFlares ; f ; f = f->next ) {
		if ( f->frameSceneNum == backEnd.viewParms.frameSceneNum && f->portalView == backEnd.viewParms.portalView && f->drawIntensity ) {
			RB_RenderFlare( f );
		}
	}

	//memcpy( vk_world.modelview_transform, modelMatrix_original, sizeof( modelMatrix_original ) );
	//vk_update_mvp( NULL );
}
