// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 1999-2005 Id Software, Inc.
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
// tr_shade.c

#include "tr_local.h"
#include "../../../../frontend/r_log.h"  // rilog-channel-mechanism Turn B — renderer.cmd
#include "../../../../frontend/r_q1_texture.h"
#include "../../../../../qcommon/q_feats.h"

R_LOG_DECLARE_CHANNEL( rch_cmd,       "renderer.cmd"       );
R_LOG_DECLARE_CHANNEL( rch_assets_q1, "renderer.assets.q1" );

/*

  THIS ENTIRE FILE IS BACK END

  This file deals with applying shaders to surface data in the tess struct.
*/


/*
==================
R_DrawElements
==================
*/
#ifndef USE_VULKAN
void R_DrawElements( int numIndexes, const glIndex_t *indexes ) {
	qglDrawElements( GL_TRIANGLES, numIndexes, GL_INDEX_TYPE, indexes );
}
#endif


/*
=============================================================

SURFACE SHADERS

=============================================================
*/

shaderCommands_t	tess;
#ifndef USE_VULKAN
static qboolean	setArraysOnce;
#endif

/*
=================
R_BindAnimatedImage
=================
*/
static void R_BindAnimatedImage( const textureBundle_t *bundle ) {
	int64_t index;
	double	v;

	if ( bundle->isVideoMap ) {
		ri.CIN_RunCinematic(bundle->videoMapHandle);
		ri.CIN_UploadCinematic(bundle->videoMapHandle);
		return;
	}

	if ( bundle->isScreenMap /*&& backEnd.viewParms.frameSceneNum == 1*/ ) {
		if ( !backEnd.screenMapDone )
			GL_Bind( tr.blackImage );
		// legacy-mainpath-retire STEP 2 — the per-image legacy descriptor-set
		// ring write for sets 1-6 retired with the rotating-set machinery.
		// Bindless-side screenmap routing lands separately.
		return;
	}

	if ( bundle->numImageAnimations <= 1 ) {
		GL_Bind( bundle->image[0] );
		return;
	}

	// it is necessary to do this messy calc to make sure animations line up
	// exactly with waveforms of the same frequency
	//v = tess.shaderTime * bundle->imageAnimationSpeed * FUNCTABLE_SIZE;
	//index = v;
	//index >>= FUNCTABLE_SIZE2;

	v = tess.shaderTime * bundle->imageAnimationSpeed; // fix for frameloss bug -EC-
	index = v;

	if ( index < 0 ) {
		index = 0;	// may happen with shader time offsets
	}

	if ( bundle->loopingImageAnim || bundle->numImageAnimations == 0 ) {
		index %= bundle->numImageAnimations;
	} else {
		if ( index >= bundle->numImageAnimations ) {
			index = bundle->numImageAnimations - 1;
		}
	}

	GL_Bind( bundle->image[ index ] );
}


/*
================
DrawTris

Draws triangle outlines for debugging
================
*/
static void DrawTris( const shaderCommands_t *input ) {
#ifdef USE_VULKAN
	uint32_t pipeline;

	if ( r_showTris->integer == 1 && backEnd.drawConsole )
		return;

	if ( tess.numIndexes == 0 )
		return;

	if ( r_fastsky->integer && input->shader->isSky )
		return;

#ifdef USE_VBO
	if ( tess.vboIndex ) {
#ifdef USE_PMLIGHT
		if ( tess.dlightPass )
			pipeline = backEnd.viewParms.portalView == PV_MIRROR ? vk.tris_mirror_debug_red_pipeline : vk.tris_debug_red_pipeline;
		else
#endif
			pipeline = backEnd.viewParms.portalView == PV_MIRROR ? vk.tris_mirror_debug_green_pipeline : vk.tris_debug_green_pipeline;
	} else
#endif
	{
#ifdef USE_PMLIGHT
		if ( tess.dlightPass )
			pipeline = backEnd.viewParms.portalView == PV_MIRROR ? vk.tris_mirror_debug_red_pipeline : vk.tris_debug_red_pipeline;
		else
#endif
			pipeline = backEnd.viewParms.portalView == PV_MIRROR ? vk.tris_mirror_debug_pipeline : vk.tris_debug_pipeline;
	}

	vk_bind_pipeline( pipeline );
	VK_PushUniformScratch();
	vk_draw_geometry( DEPTH_RANGE_ZERO, qtrue );

#else
	if ( r_showTris->integer == 1 && backEnd.drawConsole )
		return;

	GL_ClientState( 0, CLS_NONE );
	qglDisable( GL_TEXTURE_2D );

	qglColor4f( 1, 1, 1, 1 );

	GL_State( GLS_POLYMODE_LINE | GLS_DEPTHMASK_TRUE );
	qglDepthRange( 0, 0 );

	qglVertexPointer( 3, GL_FLOAT, sizeof( input->xyz[0] ), input->xyz );

	if ( qglLockArraysEXT ) {
		qglLockArraysEXT( 0, input->numVertexes );
	}

	R_DrawElements( input->numIndexes, input->indexes );

	if ( qglUnlockArraysEXT ) {
		qglUnlockArraysEXT();
	}

	qglEnable( GL_TEXTURE_2D );

	qglDepthRange( 0, 1 );
#endif
}


/*
================
DrawNormals

Draws vertex normals for debugging
================
*/
static void DrawNormals( const shaderCommands_t *input ) {
	int		i;
#ifdef USE_VULKAN
#ifdef USE_VBO
	if ( tess.vboIndex )
		return; // must be handled specially
#endif

	GL_Bind( tr.whiteImage );

	tess.numIndexes = 0;
	for ( i = 0; i < tess.numVertexes; i++ ) {
		VectorMA( tess.xyz[i], 2.0, tess.normal[i], tess.xyz[i + tess.numVertexes] );
		tess.indexes[  tess.numIndexes + 0 ] = i;
		tess.indexes[  tess.numIndexes + 1 ] = i + tess.numVertexes;
		tess.numIndexes += 2;
	}
	tess.numVertexes *= 2;
	/* full-white initialization (was identityLightByte
	 * = 0.5*255 in the legacy LDR pipeline). Normals-debug overlay
	 * inherits linear-pipeline color authoring. */
	memset( tess.svars.colors[0][0].rgba, 255, tess.numVertexes * sizeof( color4ub_t ) );

	vk_bind_pipeline( vk.normals_debug_pipeline );
	vk_bind_index();
	vk_bind_geometry( TESS_XYZ | TESS_ST0 | TESS_RGBA0 );
	VK_PushUniformScratch();
	vk_draw_geometry( DEPTH_RANGE_ZERO, qtrue );
#else
	GL_ClientState( 0, CLS_NONE );

	qglDisable( GL_TEXTURE_2D );
	qglColor4f( 1, 1, 1, 1 );

	qglDepthRange( 0, 0 );	// never occluded

	GL_State( GLS_DEPTHMASK_TRUE );

	for ( i = tess.numVertexes-1; i >= 0; i-- ) {
		VectorMA( tess.xyz[i], 2.0, tess.normal[i], tess.xyz[i*2 + 1] );
		VectorCopy( tess.xyz[i], tess.xyz[i*2] );
	}

	qglVertexPointer( 3, GL_FLOAT, sizeof( tess.xyz[0] ), tess.xyz );

	if ( qglLockArraysEXT ) {
		qglLockArraysEXT( 0, tess.numVertexes * 2 );
	}

	qglDrawArrays( GL_LINES, 0, tess.numVertexes * 2 );

	if ( qglUnlockArraysEXT ) {
		qglUnlockArraysEXT();
	}

	qglEnable( GL_TEXTURE_2D );

	qglDepthRange( 0, 1 );
#endif
}


/*
==============
RB_BeginSurface

We must set some things up before beginning any tesselation,
because a surface may be forced to perform a RB_End due
to overflow.
==============
*/
void RB_BeginSurface( shader_t *shader, int fogNum ) {

	shader_t *state;

#ifdef USE_VBO
	if ( shader->isStaticShader && !shader->remappedShader ) {
		tess.allowVBO = qtrue;
	} else {
		tess.allowVBO = qfalse;
	}
#endif

	if ( shader->remappedShader ) {
		state = shader->remappedShader;
	} else {
		state = shader;
	}

#ifdef USE_PMLIGHT
	if ( tess.fogNum != fogNum ) {
		tess.dlightUpdateParams = qtrue;
	}
#endif

#ifdef USE_TESS_NEEDS_NORMAL
#ifdef USE_PMLIGHT
	tess.needsNormal = state->needsNormal || tess.dlightPass || tess.forwardPlusPass || r_showNormals->integer;
#else
	tess.needsNormal = state->needsNormal || r_showNormals->integer;
#endif
#endif

#ifdef USE_TESS_NEEDS_ST2
	tess.needsST2 = state->needsST2;
#endif

	tess.numIndexes = 0;
	tess.numVertexes = 0;
	tess.shader = state;
	tess.fogNum = fogNum;

	tess.xstages = state->stages;
	tess.numPasses = state->numUnfoggedPasses;

	tess.shaderTime = backEnd.refdef.floatTime - tess.shader->timeOffset;
	if ( tess.shader->clampTime && tess.shaderTime >= tess.shader->clampTime ) {
		tess.shaderTime = tess.shader->clampTime;
	}
}


/*
===================
DrawMultitextured

output = t0 * t1 or t0 + t1

t0 = most upstream according to spec
t1 = most downstream according to spec
===================
*/
#ifndef USE_VULKAN
static void DrawMultitextured( const shaderCommands_t *input, int stage ) {
	const shaderStage_t *pStage;

	pStage = tess.xstages[ stage ];

	GL_State( pStage->stateBits );

	if ( !setArraysOnce ) {
		R_ComputeColors( 0, tess.svars.colors[0], pStage );
		R_ComputeTexCoords( 0, &pStage->bundle[0] );
		R_ComputeTexCoords( 1, &pStage->bundle[1] );
		GL_ClientState( 0, CLS_TEXCOORD_ARRAY | CLS_COLOR_ARRAY );

		qglTexCoordPointer( 2, GL_FLOAT, 0, input->svars.texcoordPtr[0] );
		qglColorPointer( 4, GL_UNSIGNED_BYTE, 0, input->svars.colors[0].rgba );

		GL_ClientState( 1, CLS_TEXCOORD_ARRAY );
		qglTexCoordPointer( 2, GL_FLOAT, 0, input->svars.texcoordPtr[1] );
	}

	//
	// base
	//
	GL_SelectTexture( 0 );
	R_BindAnimatedImage( &pStage->bundle[0] );

	//
	// lightmap/secondary pass
	//
	GL_SelectTexture( 1 );
	qglEnable( GL_TEXTURE_2D );
	R_BindAnimatedImage( &pStage->bundle[1] );

	if ( r_lightmap->integer ) {
		GL_TexEnv( GL_REPLACE );
	} else {
		GL_TexEnv( pStage->mtEnv );
	}

	R_DrawElements( input->numIndexes, input->indexes );

	//
	// disable texturing on TEXTURE1, then select TEXTURE0
	//
	//GL_ClientState( 1, CLS_NONE );

	qglDisable( GL_TEXTURE_2D );
	GL_SelectTexture( 0 );
}
#endif


uint32_t VK_PushUniform( const vkUniform_t *uniform );
void VK_SetFogParams( vkUniform_t *uniform, int *fogStage );
static vkUniform_t uniform;

/*
===================
RB_FogPass

Blends a fog texture on top of everything else
===================
*/
#ifdef USE_VULKAN
static void RB_FogPass( qboolean rebindIndex ) {
	uint32_t pipeline = vk.fog_pipelines[tess.shader->fogPass - 1][tess.shader->cullType][tess.shader->polygonOffset];
#ifdef USE_FOG_ONLY
	int fog_stage;

	// fog parameters
	vk_bind_pipeline( pipeline );
	if ( rebindIndex ) {
		vk_bind_index();
	}
	VK_SetFogParams( &uniform, &fog_stage );
	VK_PushUniform( &uniform );
	// publish tr.fogImage into role 3 (the
	// fog role per gen_frag.tmpl's role convention). vk_push_bindless_indices
	// (called per-draw downstream) folds the slot into the existing FS push
	// range at offset 96 of vk.pipeline_layout, which fog.frag now samples
	// instead of the retired legacy set=2 ring binding. Restores the density
	// ramp; STEP 2 had silently degraded fog to flat colour by leaving role
	// 3 unset → vk.whiteImage substituted.
	vk_bindless_track( 3 /* role: fog */, tr.fogImage );
	vk_draw_geometry( DEPTH_RANGE_NORMAL, qtrue );
#else
	const fog_t	*fog = tr.world->fogs + tess.fogNum;
	int	i;

	for ( i = 0; i < tess.numVertexes; i++ ) {
		tess.svars.colors[0][i] = fog->colorInt;
	}

	RB_CalcFogTexCoords( ( float * ) tess.svars.texcoords[0] );
	tess.svars.texcoordPtr[ 0 ] = tess.svars.texcoords[ 0 ];
	GL_Bind( tr.fogImage );

	vk_bind_pipeline( pipeline );
	if ( rebindIndex ) {
		vk_bind_index();
	}
	vk_bind_geometry( TESS_ST0 | TESS_RGBA0 );
	vk_draw_geometry( DEPTH_RANGE_NORMAL, qtrue );
#endif
#else
static void RB_FogPass( void ) {
	const fog_t	*fog;
	int			i;

	RB_CalcFogTexCoords( ( float * ) tess.svars.texcoords[0] );

	GL_ClientState( 1, CLS_NONE );
	GL_ClientState( 0, CLS_TEXCOORD_ARRAY | CLS_COLOR_ARRAY );

	qglColorPointer( 4, GL_UNSIGNED_BYTE, 0, tess.svars.colors[0].rgba );
	qglTexCoordPointer( 2, GL_FLOAT, 0, tess.svars.texcoords[0] );

	GL_SelectTexture( 0 );
	GL_Bind( tr.fogImage );

	if ( tess.shader->fogPass == FP_EQUAL ) {
		GL_State( GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA | GLS_DEPTHFUNC_EQUAL );
	} else {
		GL_State( GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA );
	}

	R_DrawElements( tess.numIndexes, tess.indexes );
#endif
}


/*
===============
R_ComputeColors
===============
*/
void R_ComputeColors( const int b, color4ub_t *dest, const shaderStage_t *pStage )
{
	int		i;

	if ( tess.numVertexes == 0 )
		return;

	//
	// rgbGen
	//
	switch ( pStage->bundle[b].rgbGen )
	{
		case CGEN_IDENTITY:
			memset( dest, 0xff, tess.numVertexes * 4 );
			break;
		default:
		case CGEN_IDENTITY_LIGHTING:
			/* identityLightByte (= 128 under legacy
			 * obScale=2) collapses to full-white 255 in the linear
			 * pipeline. CGEN_IDENTITY_LIGHTING is functionally
			 * indistinguishable from CGEN_IDENTITY post-migration;
			 * the case is retained for shader-script compatibility. */
			memset( dest, 255, tess.numVertexes * 4 );
			break;
		case CGEN_LIGHTING_DIFFUSE:
			RB_CalcDiffuseColor( ( unsigned char * ) dest );
			break;
		case CGEN_EXACT_VERTEX:
			memcpy( dest, tess.vertexColors, tess.numVertexes * sizeof( tess.vertexColors[0] ) );
			break;
		case CGEN_CONST:
			for ( i = 0; i < tess.numVertexes; i++ ) {
				dest[i] = pStage->bundle[b].constantColor;
			}
			break;
		case CGEN_VERTEX:
			/* linear-pipeline migration drops the
			 * `* tr.identityLight` halving; vertex colors pass
			 * through verbatim. The two-branch slow/fast path
			 * collapses to the original memcpy. */
			memcpy( dest, tess.vertexColors, tess.numVertexes * sizeof( tess.vertexColors[0] ) );
			break;
		case CGEN_ONE_MINUS_VERTEX:
			/* same as CGEN_VERTEX above — no
			 * identityLight halving in the linear pipeline. */
			for ( i = 0; i < tess.numVertexes; i++ )
			{
				dest[i].rgba[0] = 255 - tess.vertexColors[i].rgba[0];
				dest[i].rgba[1] = 255 - tess.vertexColors[i].rgba[1];
				dest[i].rgba[2] = 255 - tess.vertexColors[i].rgba[2];
			}
			break;
		case CGEN_FOG:
			{
				const fog_t *fog = tr.world->fogs + tess.fogNum;

				for ( i = 0; i < tess.numVertexes; i++ ) {
					dest[i] = fog->colorInt;
				}
			}
			break;
		case CGEN_WAVEFORM:
			RB_CalcWaveColor( &pStage->bundle[b].rgbWave, dest->rgba );
			break;
		case CGEN_ENTITY:
			RB_CalcColorFromEntity( dest->rgba );
			break;
		case CGEN_ONE_MINUS_ENTITY:
			RB_CalcColorFromOneMinusEntity( dest->rgba );
			break;
	}

	//
	// alphaGen
	//
	switch ( pStage->bundle[b].alphaGen )
	{
	case AGEN_SKIP:
		break;
	case AGEN_IDENTITY:
		/* Force alpha to opaque, except for vertex-color stages
		 * (CGEN_VERTEX) which carry their own alpha. */
		if ( pStage->bundle[b].rgbGen != CGEN_VERTEX ) {
			for ( i = 0; i < tess.numVertexes; i++ ) {
				dest[i].rgba[3] = 255;
			}
		}
		break;
	case AGEN_CONST:
		for ( i = 0; i < tess.numVertexes; i++ ) {
			dest[i].rgba[3] = pStage->bundle[b].constantColor.rgba[3];
		}
		break;
	case AGEN_WAVEFORM:
		RB_CalcWaveAlpha( &pStage->bundle[b].alphaWave, dest->rgba );
		break;
	case AGEN_LIGHTING_SPECULAR:
		RB_CalcSpecularAlpha( dest->rgba );
		break;
	case AGEN_ENTITY:
		RB_CalcAlphaFromEntity( dest->rgba );
		break;
	case AGEN_ONE_MINUS_ENTITY:
		RB_CalcAlphaFromOneMinusEntity( dest->rgba );
		break;
	case AGEN_VERTEX:
		for ( i = 0; i < tess.numVertexes; i++ ) {
			dest[i].rgba[3] = tess.vertexColors[i].rgba[3];
		}
		break;
	case AGEN_ONE_MINUS_VERTEX:
		for ( i = 0; i < tess.numVertexes; i++ )
		{
			dest[i].rgba[3] = 255 - tess.vertexColors[i].rgba[3];
		}
		break;
	case AGEN_PORTAL:
		{
			for ( i = 0; i < tess.numVertexes; i++ )
			{
				unsigned char alpha;
				float len;
				vec3_t v;

				VectorSubtract( tess.xyz[i], backEnd.viewParms.or.origin, v );
				len = VectorLength( v ) * tess.shader->portalRangeR;

				if ( len > 1 )
				{
					alpha = 0xff;
				}
				else
				{
					alpha = len * 0xff;
				}

				dest[i].rgba[3] = alpha;
			}
		}
		break;
	}

	//
	// fog adjustment for colors to fade out as fog increases
	//
	if ( tess.fogNum )
	{
		switch ( pStage->bundle[b].adjustColorsForFog )
		{
		case ACFF_MODULATE_RGB:
			RB_CalcModulateColorsByFog( dest->rgba );
			break;
		case ACFF_MODULATE_ALPHA:
			RB_CalcModulateAlphasByFog( dest->rgba );
			break;
		case ACFF_MODULATE_RGBA:
			RB_CalcModulateRGBAsByFog( dest->rgba );
			break;
		case ACFF_NONE:
			break;
		}
	}
}


/*
===============
R_ComputeTexCoords
===============
*/
void R_ComputeTexCoords( const int b, const textureBundle_t *bundle ) {
	int	i;
	vec2_t *src, *dst;

	if ( !tess.numVertexes )
		return;

	src = dst = tess.svars.texcoords[b];

	//
	// generate the texture coordinates
	//
	switch ( bundle->tcGen )
	{
	case TCGEN_IDENTITY:
		src = tess.texCoords00;
		break;
	case TCGEN_TEXTURE:
		src = tess.texCoords[0];
		break;
	case TCGEN_LIGHTMAP:
		src = tess.texCoords[1];
		break;
	case TCGEN_VECTOR:
		for ( i = 0 ; i < tess.numVertexes ; i++ ) {
			dst[i][0] = DotProduct( tess.xyz[i], bundle->tcGenVectors[0] );
			dst[i][1] = DotProduct( tess.xyz[i], bundle->tcGenVectors[1] );
		}
		break;
	case TCGEN_FOG:
		RB_CalcFogTexCoords( ( float * ) dst );
		break;
	case TCGEN_ENVIRONMENT_MAPPED:
		RB_CalcEnvironmentTexCoords( ( float * ) dst );
		break;
	case TCGEN_ENVIRONMENT_MAPPED_FP:
		RB_CalcEnvironmentTexCoordsFP( ( float * ) dst, bundle->isScreenMap );
		break;
	case TCGEN_BAD:
		return;
	}

	//
	// alter texture coordinates
	//
	for ( int tm = 0; tm < bundle->numTexMods ; tm++ ) {
		switch ( bundle->texMods[tm].type )
		{
		case TMOD_NONE:
			tm = TR_MAX_TEXMODS; // break out of for loop
			break;

		case TMOD_TURBULENT:
			RB_CalcTurbulentTexCoords( &bundle->texMods[tm].wave, (float *)src, (float *) dst );
			src = dst;
			break;

		case TMOD_ENTITY_TRANSLATE:
			RB_CalcScrollTexCoords( backEnd.currentEntity->e.shaderTexCoord, (float *)src, (float *) dst );
			src = dst;
			break;

		case TMOD_SCROLL:
			RB_CalcScrollTexCoords( bundle->texMods[tm].scroll, (float *)src, (float *) dst );
			src = dst;
			break;

		case TMOD_SCALE:
			RB_CalcScaleTexCoords( bundle->texMods[tm].scale, (float *) src, (float *) dst );
			src = dst;
			break;

		case TMOD_OFFSET:
			for ( i = 0; i < tess.numVertexes; i++ ) {
				dst[i][0] = src[i][0] + bundle->texMods[tm].offset[0];
				dst[i][1] = src[i][1] + bundle->texMods[tm].offset[1];
			}
			src = dst;
			break;

		case TMOD_SCALE_OFFSET:
			for ( i = 0; i < tess.numVertexes; i++ ) {
				dst[i][0] = (src[i][0] * bundle->texMods[tm].scale[0] ) + bundle->texMods[tm].offset[0];
				dst[i][1] = (src[i][1] * bundle->texMods[tm].scale[1] ) + bundle->texMods[tm].offset[1];
			}
			src = dst;
			break;

		case TMOD_OFFSET_SCALE:
			for ( i = 0; i < tess.numVertexes; i++ ) {
				dst[i][0] = (src[i][0] + bundle->texMods[tm].offset[0]) * bundle->texMods[tm].scale[0];
				dst[i][1] = (src[i][1] + bundle->texMods[tm].offset[1]) * bundle->texMods[tm].scale[1];
			}
			src = dst;
			break;

		case TMOD_STRETCH:
			RB_CalcStretchTexCoords( &bundle->texMods[tm].wave, (float *)src, (float *) dst );
			src = dst;
			break;

		case TMOD_TRANSFORM:
			RB_CalcTransformTexCoords( &bundle->texMods[tm], (float *)src, (float *) dst );
			src = dst;
			break;

		case TMOD_ROTATE:
			RB_CalcRotateTexCoords( bundle->texMods[tm].rotateSpeed, (float *) src, (float *) dst );
			src = dst;
			break;

		default:
			ri.Terminate( TERM_CLIENT_DROP, "ERROR: unknown texmod '%d' in shader '%s'", bundle->texMods[tm].type, tess.shader->name );
			break;
		}
	}

	tess.svars.texcoordPtr[ b ] = src;
}


#ifdef USE_VULKAN
static void R_ComputeLightstyles( void ) {
	// Unified 64-style loop: pattern strings in tr.lightstylePatterns[] are the
	// sole source of truth (populated from configstrings by RE_SetLightstylePattern).
	// 'a'=0.0 (dark), 'm'=1.0 (baseline), 'z'=2.083 (overbright). 10Hz stepped.
	int   style;
	int   t = backEnd.refdef.time;

	for ( style = 0; style < 64; style++ ) {
		const char *pattern = tr.lightstylePatterns[style];
		int   len = (int)strlen( pattern );
		float intensity;

		if ( len == 0 ) {
			intensity = 0.0f;
		} else if ( len == 1 ) {
			intensity = ( pattern[0] - 'a' ) / 12.0f;
		} else {
			int   frame = ( t / 100 ) % len;
			float curr  = ( pattern[frame] - 'a' ) / 12.0f;
			if ( r_lerpLightstyles->integer ) {
				int   nextFrame = ( frame + 1 ) % len;
				float next      = ( pattern[nextFrame] - 'a' ) / 12.0f;
				float frac      = (float)( t % 100 ) / 100.0f;
				intensity = curr + ( next - curr ) * frac;
			} else {
				intensity = curr;
			}
		}

		tr.lightstyleValues[style] = intensity;
	}
}


/*
RE_SetLightstylePattern
Store a pattern string for a lightstyle slot.
style in [0,63]; pattern is a NUL-terminated string up to LIGHTSTYLE_PATTERN_MAX chars.
tr.lightstyleValues[] is now a per-frame output of R_ComputeLightstyles only.
*/
void RE_SetLightstylePattern( int style, const char *pattern ) {
	if ( style < 0 || style >= 64 ) return;
	Q_strncpyz( tr.lightstylePatterns[style], pattern ? pattern : "",
	            sizeof( tr.lightstylePatterns[style] ) );
}
#endif


/*
** RB_IterateStagesGeneric
*/
#ifdef USE_VULKAN
static void RB_IterateStagesGeneric( const shaderCommands_t *input, qboolean fogCollapse )
#else
static void RB_IterateStagesGeneric( const shaderCommands_t *input )
#endif
{
	const shaderStage_t *pStage;
	int tess_flags;
	int stage, i;

#ifdef USE_VULKAN
	uint32_t pipeline;
	int fog_stage;
	qboolean pushUniform;

	vk_bind_index();

	tess_flags = input->shader->tessFlags;

	pushUniform = qfalse;

#ifdef USE_FOG_COLLAPSE
	if ( fogCollapse ) {
		VK_SetFogParams( &uniform, &fog_stage );
		VectorCopy( backEnd.or.viewOrigin, uniform.eyePos );
		// legacy-mainpath-retire STEP 2 — legacy ring write retired; bindless track stays.
		vk_bindless_track( 3 /* role: fog */, tr.fogImage );
		pushUniform = qtrue;
	} else
#endif
	{
		fog_stage = 0;
		if ( tess_flags & TESS_VPOS ) {
			VectorCopy( backEnd.or.viewOrigin, uniform.eyePos );
			tess_flags &= ~TESS_VPOS;
			pushUniform = qtrue;
		}
	}
#endif // USE_VULKAN

	for ( stage = 0; stage < MAX_SHADER_STAGES; stage++ )
	{
		pStage = tess.xstages[ stage ];
		if ( !pStage ) {
			break;
		}

#ifdef USE_VBO
		tess.vboStage = stage;
#endif

#ifdef USE_VULKAN
		tess_flags |= pStage->tessFlags;

		for ( i = 0;  i < pStage->numTexBundles; i++ ) {
			if ( pStage->bundle[i].image[0] != NULL ) {
				GL_SelectTexture( i );
				R_BindAnimatedImage( &pStage->bundle[i] );
				if ( tess_flags & ( TESS_ST0 << i ) ) {
					R_ComputeTexCoords( i, &pStage->bundle[i] );
				}
				if ( tess_flags & ( TESS_RGBA0 << i ) ) {
					R_ComputeColors( i, tess.svars.colors[i], pStage );
#if FEAT_THIRD_PERSON
#if FEAT_FORCE_ENTITY_VERTEX_ALPHA
					// RF_FORCE_ENT_ALPHA: override vertex color alpha
					if ( i == 0 && backEnd.currentEntity &&
						 ( backEnd.currentEntity->e.renderfx & RF_FORCE_ENT_ALPHA ) ) {
						byte a = backEnd.currentEntity->e.shader.rgba[3];
						for ( int j = 0; j < tess.numVertexes; j++ ) {
							tess.svars.colors[0][j].rgba[3] = a;
						}
					}
#endif
#endif
				}
				if ( tess_flags & (TESS_ENT0 << i) && backEnd.currentEntity ) {
					// decode the entity colour to linear domain
					// at the UBO fill site. gen_frag.tmpl's USE_ENT_COLOR variants
					// multiply this against the (sampleColorTex-decoded) texel in
					// linear domain. shaderRGBA is a display-domain byte triple;
					// the alpha component below carries per-stage alphaGen state,
					// not an sRGB-encoded colour — left raw.
					uniform.ent.color[i][0] = R_SRGBToLinear( backEnd.currentEntity->e.shader.rgba[0] / 255.0f );
					uniform.ent.color[i][1] = R_SRGBToLinear( backEnd.currentEntity->e.shader.rgba[1] / 255.0f );
					uniform.ent.color[i][2] = R_SRGBToLinear( backEnd.currentEntity->e.shader.rgba[2] / 255.0f );
				#if FEAT_THIRD_PERSON
				#if FEAT_FORCE_ENTITY_VERTEX_ALPHA
					// RF_FORCE_ENT_ALPHA: force alpha from entity regardless of alphaGen
					if ( backEnd.currentEntity->e.renderfx & RF_FORCE_ENT_ALPHA ) {
						uniform.ent.color[i][3] = backEnd.currentEntity->e.shader.rgba[3] / 255.0;
					} else
			#endif
			#endif
					{
						uniform.ent.color[i][3] = pStage->bundle[i].alphaGen == AGEN_IDENTITY ? 1.0 : (backEnd.currentEntity->e.shader.rgba[3] / 255.0);
					}
					pushUniform = qtrue;
				}
			}
		}

		// Always push a per-draw UBO ring item: gen_vert now reads the MVP from
		// the UBO (ubo.mvp), so every base-pass draw needs a bound uniform — not
		// just the fog/env/ent cases that historically set pushUniform. The mvp is
		// filled by VK_PushUniform from the vk_update_mvp stash; the fog/env/ent
		// fields are written above when relevant and unread otherwise. (pushUniform
		// still gates whether those OTHER fields were freshly set this stage; the
		// push itself is unconditional.)
		(void)pushUniform;
		pushUniform = qfalse;
		VectorClear( uniform.emissionRadiance );
		// The typed material is stamped only onto retained world/BSP shaders.
		// Consume it once for every 3D use of that material, including moving
		// brush models, while keeping 2D/UI draws dark even when they reuse a
		// world shader.
		if ( stage == 0 && !backEnd.projection2D )
			VectorCopy( tess.shader->emissionRadiance, uniform.emissionRadiance );
		VK_PushUniform( &uniform );

		GL_SelectTexture( 0 );

		if ( r_lightmap->integer ) {
			if ( pStage->bundle[1].lightmap != LIGHTMAP_INDEX_NONE ) {
				// collapsed-lightmap surface: lightmap in bundle[1], replace the
				// bundle[0] diffuse with white so only the lightmap shows.
				//GL_SelectTexture( 0 );
				GL_Bind( tr.whiteImage );
			} else if ( tess.shader->separateLightmapPass ) {
				// Shader whose lightmap is a separate trailing pass (lightmap in
				// bundle[0] of its own stage). For a NON-lightmap (diffuse) stage,
				// white-replace every diffuse operand so it contributes white; the
				// lightmap stage (bundle[0] is the lightmap) draws unchanged. The
				// trailing lightmap pass then modulates against white = lightmap-only,
				// matching the collapsed-lightmap path above.
				if ( pStage->bundle[0].lightmap == LIGHTMAP_INDEX_NONE ) {
					for ( i = 0; i < pStage->numTexBundles; i++ ) {
						if ( pStage->bundle[i].lightmap == LIGHTMAP_INDEX_NONE &&
						     pStage->bundle[i].image[0] != NULL ) {
							GL_SelectTexture( i );
							GL_Bind( tr.whiteImage );
						}
					}
					GL_SelectTexture( 0 );
				}
			}
		}

		if ( backEnd.viewParms.portalView == PV_MIRROR ) {
			pipeline = pStage->vk_mirror_pipeline[fog_stage];
		} else {
			pipeline = pStage->vk_pipeline[fog_stage];
		}

		// The per-map srgb-variant
		// swap is gone — gen_frag.tmpl decodes colour texels
		// unconditionally now, so there is no second variant to pick.

#if FEAT_THIRD_PERSON
#if FEAT_FORCE_ENTITY_VERTEX_ALPHA
		// RF_FORCE_ENT_ALPHA: swap to alpha-blended pipeline for entity fade.
		// Must mask-and-set (not OR) because GLS blend values are enumerated
		// within 4-bit fields, not independent bitmasks.
		// e.g. GLS_SRCBLEND_ONE(0x02) | GLS_SRCBLEND_SRC_ALPHA(0x05) = 0x07
		//      which decodes as GLS_SRCBLEND_DST_ALPHA — wrong blend factor.
		if ( backEnd.currentEntity &&
			 ( backEnd.currentEntity->e.renderfx & RF_FORCE_ENT_ALPHA ) &&
			 backEnd.currentEntity->e.shader.rgba[3] < 255 ) {
			Vk_Pipeline_Def def;
			vk_get_pipeline_def( pipeline, &def );
			def.state_bits = ( def.state_bits & ~( GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS ) )
				| GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
			pipeline = vk_find_pipeline_ext( 0, &def, qtrue );
		}
#endif
#endif

		/* MSDF shader override — use dedicated pipeline when available */
		if ( tess.shader->msdf && vk.msdf_pipeline != 0 ) {
			pipeline = vk.msdf_pipeline;
			/* Push outline/glow/shadow params (also re-pushes MVP via MSDF layout) */
			vk_update_msdf_outline( tr.msdfOutlineWidth,
			                         tr.msdfOutlineColor,
			                         tr.msdfGlowWidth,
			                         tr.msdfGlowColor,
			                         tr.msdfShadowOffset,
			                         tr.msdfShadowColor );
		}

		{
			static int q1ls_log_count = 0;
			if ( q1ls_log_count < 20 ) {
				q1ls_log_count++;
				R_LOG( rch_assets_q1, SEV_TRACE, "gate check sh='%s' b1lm=%d q1ls_pipeline=%d lightStyles=%s\n",
				        tess.shader->name, pStage->bundle[1].lightmap,
				        (int)vk.q1ls_pipeline, ( tr.world && tr.world->lightStyles ) ? "yes" : "no" );
			}
		}
		if ( tr.world && tr.world->lightStyles && vk.q1ls_pipeline != 0
		     && pStage->bundle[1].lightmap != LIGHTMAP_INDEX_NONE ) {
			int lmIdx = pStage->bundle[1].lightmap - LIGHTMAP_INDEX_OFFSET;
			const byte *s = tess.q1SurfaceStyles;
			R_ComputeLightstyles();
			uniform.q1StyleIntensities[0] = s[0] < 64 ? tr.lightstyleValues[s[0]] : 0.0f;
			uniform.q1StyleIntensities[1] = s[1] < 64 ? tr.lightstyleValues[s[1]] : 0.0f;
			uniform.q1StyleIntensities[2] = s[2] < 64 ? tr.lightstyleValues[s[2]] : 0.0f;
			uniform.q1StyleIntensities[3] = s[3] < 64 ? tr.lightstyleValues[s[3]] : 0.0f;

			// resolve the four style lightmaps the q1_ls/q1_ls_array
			// fragment shaders sample at bindless roles 2..5. Style 0 lives in
			// tr.lightmaps[] (or tr.propLightmaps[] for prop-BSP lightmap pages);
			// styles 1..3 live in tr.lightmapsStyle[0..2][] (R_LoadQ1StyledLightmaps).
			// All four are image_t entries with bindless slots already allocated
			// at R_CreateImage time, so vk_bindless_track packs them per-draw the
			// same way fog (role 3) and the water surface (role 0) ride. Missing
			// per-style atlases (Q1 maps that only author style 0) degrade to
			// tr.whiteImage — the shader still produces tStyle*intensity, and an
			// intensity of zero (s[k] >= 64) makes the sample irrelevant anyway.
			image_t *lmImgs[4] = { tr.whiteImage, tr.whiteImage, tr.whiteImage, tr.whiteImage };
			if ( lmIdx >= LIGHTMAP_PROP_OFFSET ) {
				int pi = lmIdx - LIGHTMAP_PROP_OFFSET;
				if ( tr.propLightmaps && pi < tr.numPropLightmaps && tr.propLightmaps[pi] )
					lmImgs[0] = tr.propLightmaps[pi];
				/* Prop BSPs author only style 0; lmImgs[1..3] stay whiteImage. */
			} else if ( tr.lightmaps && lmIdx >= 0 && lmIdx < tr.numLightmaps ) {
				if ( tr.lightmaps[lmIdx] )
					lmImgs[0] = tr.lightmaps[lmIdx];
				for ( int k = 0; k < 3; k++ ) {
					if ( tr.lightmapsStyle[k] && lmIdx < tr.numLightmapsStyle && tr.lightmapsStyle[k][lmIdx] )
						lmImgs[k + 1] = tr.lightmapsStyle[k][lmIdx];
				}
			}

			if ( tess.shader->q1AnimArray && vk.q1ls_array_pipeline != 0 ) {
				/* GPU texture array path — shader computes frame from tr.refdef.time */
				{
					static int q1arr_log_count = 0;
					if ( q1arr_log_count < 10 ) {
						q1arr_log_count++;
						R_LOG( rch_assets_q1, SEV_DEBUG, "array path sh='%s' numFrames=%d animArray=%p\n",
						        tess.shader->name, tess.shader->q1NumAnimFrames,
						        (void *)tess.shader->q1AnimArray );
					}
				}
				uniform.light.pos[0] = (float)tr.refdef.time;
				uniform.light.pos[1] = (float)tess.shader->q1NumAnimFrames;
				VK_PushUniform( &uniform );
				// publish the four style lightmaps at
				// roles 2..5 AND the animArray 2DArray at role 0 for
				// q1_ls_array.frag. The bundle iteration above (lines 1023-1026)
				// already wrote roles 0 and 1 with bundle[0].image[idx] /
				// bundle[1].image[0] (the 2D albedo + 2D lightmap from
				// R_BindAnimatedImage); we overwrite role 0 here with the
				// q1AnimArray image_t — that image_t carries a 2DArray
				// VkImageView and its ralBindlessSlot indexes the SAMPLED_IMAGE
				// binding at WIRED_BINDLESS_BIND_ARRAY_IMAGES (set 7, binding 2,
				// disjoint slot space from the 2D binding). vk_push_bindless_
				// indices packs the same way; the shader's
				// WIRED_BINDLESS_TEX_ARRAY macro picks the right binding.
				// Role 1 stays whatever the bundle loop wrote — harmless,
				// q1_ls_array.frag never samples role 1.
				if ( tess.shader->q1AnimArray ) {
					vk_bindless_track( 0 /* role: animArray (2DArray) */, tess.shader->q1AnimArray );
				}
				vk_bindless_track( 2 /* role: lm0 */, lmImgs[0] );
				vk_bindless_track( 3 /* role: lm1 */, lmImgs[1] );
				vk_bindless_track( 4 /* role: lm2 */, lmImgs[2] );
				vk_bindless_track( 5 /* role: lm3 */, lmImgs[3] );
				pipeline = vk.q1ls_array_pipeline;
			} else {
				/* Non-animated single-frame Q1 surface — or a per-frame anim chain
				 * driven by tess.shaderTime (R_BindAnimatedImage semantics). */
				uniform.light.pos[0] = 0.0f;
				VK_PushUniform( &uniform );

				// resolve current + next anim frame from
				// pStage->bundle[0] for q1_ls.frag's diffuseMap / diffuseMapNext
				// cross-fade. Mirrors R_BindAnimatedImage's index math; the
				// non-array dispatch keeps animBlend=_animPad.x at its prior
				// (uniform-zero) value, so the cross-fade collapses to
				// diffuseMap on the static-content branch — diffuseMapNext is
				// still published to silence the NULL-gap whiteImage fallback.
				const textureBundle_t *b = &pStage->bundle[0];
				image_t *curImg, *nextImg;
				if ( b->numImageAnimations <= 1 ) {
					curImg  = b->image[0] ? b->image[0] : tr.whiteImage;
					nextImg = curImg;
				} else {
					int64_t idx = (int64_t)( tess.shaderTime * b->imageAnimationSpeed );
					if ( idx < 0 )
						idx = 0;
					if ( b->loopingImageAnim || b->numImageAnimations == 0 ) {
						idx %= b->numImageAnimations;
					} else if ( idx >= b->numImageAnimations ) {
						idx = b->numImageAnimations - 1;
					}
					int64_t nextIdx = ( idx + 1 ) % b->numImageAnimations;
					curImg  = b->image[ idx ]     ? b->image[ idx ]     : tr.whiteImage;
					nextImg = b->image[ nextIdx ] ? b->image[ nextIdx ] : tr.whiteImage;
				}
				vk_bindless_track( 0 /* role: diffuseMap     */, curImg );
				vk_bindless_track( 1 /* role: diffuseMapNext */, nextImg );
				vk_bindless_track( 2 /* role: lm0            */, lmImgs[0] );
				vk_bindless_track( 3 /* role: lm1            */, lmImgs[1] );
				vk_bindless_track( 4 /* role: lm2            */, lmImgs[2] );
				vk_bindless_track( 5 /* role: lm3            */, lmImgs[3] );
				pipeline = vk.q1ls_pipeline;
			}
		}

#if FEAT_PBR
		// Base-pass IBL ambient swap. For a WORLDSPAWN lightmap-modulate surface
		// carrying a pbrMap under r_pbr, route the draw through the USE_IBL gen
		// variant: it adds the light-independent analytic-sky environment ambient
		// (the probe cubes + split-sum LUT on set 2) on top of the baked lightmap.
		// Mirrors the sun-shadow swap idiom directly below. Tightly gated so the
		// hot base pass is byte-identical on every non-pbrMap / r_pbr-0 / entity
		// surface. Entities are excluded (worldspawn-only — they keep the
		// lightgrid path); the IBL needs the world normal, so TESS_NNN is OR'd in.
		if ( r_pbr->integer && pStage->bundle[3].image[0] != NULL
		     && backEnd.currentEntity == &tr.worldEntity ) {
			Vk_Pipeline_Def idef;
			Vk_Shader_Type  ibase;
			vk_get_pipeline_def( pipeline, &idef );
			ibase = idef.shader_type;
			switch ( ibase ) {
				case TYPE_MULTI_TEXTURE_MUL2: idef.shader_type = TYPE_MULTI_TEXTURE_MUL2_IBL; break;
				case TYPE_BLEND2_MUL:         idef.shader_type = TYPE_BLEND2_MUL_IBL;         break;
				case TYPE_MULTI_TEXTURE_MUL3: idef.shader_type = TYPE_MULTI_TEXTURE_MUL3_IBL; break;
				case TYPE_BLEND3_MUL:         idef.shader_type = TYPE_BLEND3_MUL_IBL;         break;
				default: break;   // _ENV / identity / fixed / non-lightmap — no IBL variant for these
			}
			if ( idef.shader_type != ibase ) {
				idef.ibl_enabled = 1;
				pipeline = vk_find_pipeline_ext( 0, &idef, qtrue );
				// pbrMap (ORM) at bindless role 8 — distinct from the light-pass
				// pbrMap (role 3); the base pass and PMLIGHT pass reset bindless
				// tracking independently, so no collision.
				vk_bindless_track( 8 /* role: pbrMap */, pStage->bundle[3].image[0] );
				// The IBL gen pipeline binds the world normal at location 5; make
				// sure the normal vertex stream is computed + bound for this draw.
				tess_flags |= TESS_NNN;
			}
		}
#endif

#if FEAT_SHADOW_MAPPING
		// sun-shadow receiver swap. For a lightmapped
		// world-modulate surface with r_shadows on, route the draw
		// through the USE_SHADOWMAP gen variant: it samples the load-time
		// sun-mask atlas (bindless role 7) and modulates the baked lightmap by
		// the runtime CSM sun term. Mirrors the PBR swap idiom. Q1-lightstyle
		// surfaces fall through the switch default (pipeline is q1ls here);
		// bmodel / non-merged-map lightmap surfaces sample a zero sun-mask
		// (the sun-mask is baked for merged-lightmap worldspawn surfaces only) so
		// wired_apply_sun_shadow is a no-op there — correct by construction.
		// Sun-shadow receiver swap. The lightmap may be the operand in bundle[1] (the
		// collapsed-lightmap MUL2/MUL3 case) OR bundle[0] of a lone single-texture pass
		// (a separate-lightmap-pass surface — the inset/circuit floor, shader.separate-
		// LightmapPass). Gate on the lightmap OPERAND being present in this stage, not
		// bundle[1] only, so the inset's lightmap stage also becomes a receiver — the
		// operand-keyed apply (gen_frag wired_apply_sun_shadow_operand, lightmap_slot)
		// then samples the sun shadow on whichever slot is the lightmap. Mirrors the
		// #215 operand-is-lightmap unification (whiten + boost + sun shadow, one path).
		if ( r_shadows->integer == 1 && vk.shadowMap.active
		     && tr.sunMaskAtlas != NULL
		     && ( pStage->bundle[0].lightmap != LIGHTMAP_INDEX_NONE
		          || pStage->bundle[1].lightmap != LIGHTMAP_INDEX_NONE ) ) {
			Vk_Pipeline_Def sdef;
			Vk_Shader_Type  sbase;
			vk_get_pipeline_def( pipeline, &sdef );
			sbase = sdef.shader_type;
			switch ( sbase ) {
				case TYPE_MULTI_TEXTURE_MUL2: sdef.shader_type = TYPE_MULTI_TEXTURE_MUL2_SHADOW; break;
				case TYPE_MULTI_TEXTURE_MUL3: sdef.shader_type = TYPE_MULTI_TEXTURE_MUL3_SHADOW; break;
				case TYPE_BLEND2_MUL:         sdef.shader_type = TYPE_BLEND2_MUL_SHADOW;         break;
				case TYPE_BLEND3_MUL:         sdef.shader_type = TYPE_BLEND3_MUL_SHADOW;         break;
				// Path PA Fault 2 — rgbGen identity / identityLighting collapse
				// to these colour-mode variants (the common vanilla-Q3 case).
				case TYPE_MULTI_TEXTURE_MUL2_IDENTITY:    sdef.shader_type = TYPE_MULTI_TEXTURE_MUL2_IDENTITY_SHADOW;    break;
				case TYPE_MULTI_TEXTURE_MUL2_FIXED_COLOR: sdef.shader_type = TYPE_MULTI_TEXTURE_MUL2_FIXED_COLOR_SHADOW; break;
				// Shadow-Unification Part 1 — single-texture lightmap stage of a
				// separate-lightmap-pass surface (lightmap in bundle[0]).
				case TYPE_SIGNLE_TEXTURE:               sdef.shader_type = TYPE_SINGLE_TEXTURE_SHADOW;             break;
				case TYPE_SINGLE_TEXTURE_IDENTITY:      sdef.shader_type = TYPE_SINGLE_TEXTURE_IDENTITY_SHADOW;    break;
				case TYPE_SINGLE_TEXTURE_FIXED_COLOR:   sdef.shader_type = TYPE_SINGLE_TEXTURE_FIXED_COLOR_SHADOW; break;
				default: break;   // _ENV / MUL3-variant lightmap types — no shadow-receiver variant for these
			}
			if ( sdef.shader_type != sbase ) {
				const int smPage = tess.shader->lightmapIndex;
				pipeline = vk_find_pipeline_ext( 0, &sdef, qtrue );
				// F-1 — publish the sun-mask atlas page at bindless role 7.
				if ( smPage >= 0 && smPage < tr.numLightmaps && tr.sunMaskAtlas[smPage] != NULL )
					vk_bindless_track( 7 /* role: sun-mask */, tr.sunMaskAtlas[smPage] );
				// F-2 — the main world pass never populates the CSM cascades
				// (only VK_LightingPass does, ~tr_shade.c:1452); copy them into
				// the UBO here and push so gen_frag's sampleShadow reads them.
				memcpy( uniform.cascadeMVP, vk.shadowMap.cascadeMVP, sizeof( uniform.cascadeMVP ) );
				Vector4Copy( vk.shadowMap.cascadeSplits, uniform.cascadeSplits );
				VK_PushUniform( &uniform );
			}
		}
#endif
		vk_bind_pipeline( pipeline );
		vk_bind_geometry( tess_flags );
		vk_draw_geometry( tess.depthRange, qtrue );

		if ( pStage->depthFragment ) {
			if ( backEnd.viewParms.portalView == PV_MIRROR )
				pipeline = pStage->vk_mirror_pipeline_df;
			else
				pipeline = pStage->vk_pipeline_df;
			vk_bind_pipeline( pipeline );
			vk_draw_geometry( tess.depthRange, qtrue );
		}
#else
		R_ComputeColors( 0, tess.svars.colors[0].rgba, pStage );

		R_ComputeTexCoords( 0, &pStage->bundle[0] );

		//
		// do multitexture
		//
		if ( pStage->bundle[1].image[0] != NULL )
		{
			DrawMultitextured( input, stage );
		}
		else
		{
			if ( !setArraysOnce )
			{
				R_ComputeTexCoords( 0, &pStage->bundle[0] );
				R_ComputeColors( 0, tess.svars.colors[0], pStage );

				GL_ClientState( 1, CLS_NONE );
				GL_ClientState( 0, CLS_TEXCOORD_ARRAY | CLS_COLOR_ARRAY );

				qglTexCoordPointer( 2, GL_FLOAT, 0, input->svars.texcoordPtr[0] );
				qglColorPointer( 4, GL_UNSIGNED_BYTE, 0, input->svars.colors[0].rgba );
			}

			//
			// set state
			//
			R_BindAnimatedImage( &pStage->bundle[0] );

			GL_State( pStage->stateBits );

			//
			// draw
			//
			R_DrawElements( input->numIndexes, input->indexes );
		}
#endif

		// allow skipping out to show just lightmaps during development
		if ( r_lightmap->integer && ( pStage->bundle[0].lightmap != LIGHTMAP_INDEX_NONE || pStage->bundle[1].lightmap != LIGHTMAP_INDEX_NONE ) )
			break;

		tess_flags = 0;
	}

#ifdef USE_VULKAN
	if ( pushUniform ) {
		VK_PushUniform( &uniform );
	}
	if ( tess_flags ) // fog-only shaders?
		vk_bind_geometry( tess_flags );
#endif
}


#ifdef USE_VULKAN

void VK_SetFogParams( vkUniform_t *uniform, int *fogStage )
{
	if ( tess.fogNum && tess.shader->fogPass ) {
		const fogProgramParms_t *fp = RB_CalcFogProgramParms();
		// vertex data
		Vector4Copy( fp->fogDistanceVector, uniform->fogDistanceVector );
		Vector4Copy( fp->fogDepthVector, uniform->fogDepthVector );
		uniform->fogEyeT[0] = fp->eyeT;
		if ( fp->eyeOutside ) {
			uniform->fogEyeT[1] = 0.0; // fog eye out
		} else {
			uniform->fogEyeT[1] = 1.0; // fog eye in
		}
		// fragment data
		// decode fogColor to linear domain at the
		// (single) UBO fill site. fog.frag and gen_frag.tmpl's USE_FOG
		// branch consume this in linear-domain math now; light_frag.tmpl's
		// USE_FOG branch reads only the fog-texture alpha, so it's
		// unaffected. Alpha (fp->fogColor[3], normally 1.0) is not an
		// sRGB-encoded channel — copied verbatim.
		uniform->fogColor[0] = R_SRGBToLinear( fp->fogColor[0] );
		uniform->fogColor[1] = R_SRGBToLinear( fp->fogColor[1] );
		uniform->fogColor[2] = R_SRGBToLinear( fp->fogColor[2] );
		uniform->fogColor[3] = fp->fogColor[3];
		*fogStage = 1;
	} else {
		*fogStage = 0;
	}
}


#ifdef USE_PMLIGHT
static void VK_SetLightParams( vkUniform_t *uniform, const dlight_t *dl ) {
	float radius;

// NOLINTNEXTLINE(readability-redundant-preprocessor) — branch retained for portability with non-Vulkan derivatives
#ifdef USE_VULKAN
	if ( !glConfig.deviceSupportsGamma && !vk.fboActive )
#else
	if ( !glConfig.deviceSupportsGamma )
#endif
		VectorScale( dl->color, 2 * powf( r_intensity->value, r_gamma->value ), uniform->light.color);
	else {
		// decode the dlight colour to linear domain.
		// light_frag.tmpl's BRDF (classic Phong + the GGX/Schlick/Smith
		// Cook-Torrance path) needs linear-radiance light values for
		// colorimetrically correct math; light.color[3] (1/r^2 falloff)
		// is set below and is not a colour. The branch above is the legacy
		// non-FBO gamma-ramp path (dead in Wired — vk.fboActive is always
		// set with r_fbo 1) and does its own gamma compensation, so it is
		// left untouched.
		uniform->light.color[0] = R_SRGBToLinear( dl->color[0] );
		uniform->light.color[1] = R_SRGBToLinear( dl->color[1] );
		uniform->light.color[2] = R_SRGBToLinear( dl->color[2] );
	}

	radius = dl->radius;

	// vertex data
	VectorCopy( backEnd.or.viewOrigin, uniform->eyePos ); uniform->eyePos[3] = 0.0f;
#if FEAT_SHADOW_MAPPING
	// repurpose the unused eyePos.w slot as the live
	// r_csmShowCascades debug flag (light_frag.tmpl tints by sampled cascade).
	// The vertex stage's V = eyePos - vec4(in_position,1) only uses V.xyz, so
	// V.w changing 0→? is harmless; no UBO layout change, no spec constant.
	if ( vk.shadowMap.active && ri.Cvar_Get( "r_csmShowCascades", "0", 0 )->integer )
		uniform->eyePos[3] = 1.0f;
#endif
	VectorCopy( dl->transformed, uniform->light.pos ); uniform->light.pos[3] = 0.0f;

	// fragment data
	uniform->light.color[3] = 1.0f / Square( radius );

	if ( dl->linear )
	{
		vec4_t ab;
		VectorSubtract( dl->transformed2, dl->transformed, ab );
		ab[3] = 1.0f / DotProduct( ab, ab );
		Vector4Copy( ab, uniform->light.vector );
	}
}
#endif


uint32_t VK_PushUniform( const vkUniform_t *uniform ) {
	const uint32_t offset = vk.cmd->uniform_read_offset = PAD( vk.cmd->vertex_buffer_offset, vk.uniform_alignment );

	if ( offset + vk.uniform_item_size > vk.geometry_buffer_size ) {
		// Ring overflow this frame: no item was written. Mark the read offset
		// invalid so vk_push_bindless_indices skips its packed_indices write
		// (the offset would be out of bounds) — the bindless table moved into
		// this ring item in the push→UBO migration, so an unbounded write here
		// is no longer harmless the way the old push-constant write was.
		vk.cmd->uniform_read_offset = ~0U;
		return ~0U;
	}

	// push uniform
	memcpy( vk.cmd->vertex_buffer_ptr + offset, uniform, sizeof( *uniform ) );
	// Overwrite the mvp field with the latest stashed MVP (vk_update_mvp keeps it
	// current per-entity) so the main-path vertex shaders read it from the UBO
	// rather than the push constant — independent of whether the caller's local
	// uniform happened to fill it. Same byte value as the pushed MVP.
	memcpy( ( (vkUniform_t *)( vk.cmd->vertex_buffer_ptr + offset ) )->mvp, vk_world.mvp,
		sizeof( vk_world.mvp ) );
	// Advanced fog follows the same bounded per-draw UBO ownership as MVP. The
	// renderer-global stash is zero in feature-OFF builds and changes only at fog
	// transitions when enabled. Unconditional stamping keeps every local uniform
	// source deterministic without any push-constant backend dependency.
	memcpy( ( (vkUniform_t *)( vk.cmd->vertex_buffer_ptr + offset ) )->advancedFogColorDensity,
		vk_world.advancedFogColorDensity, sizeof( vk_world.advancedFogColorDensity ) );
	memcpy( ( (vkUniform_t *)( vk.cmd->vertex_buffer_ptr + offset ) )->advancedFogTypeFarEnabled,
		vk_world.advancedFogTypeFarEnabled, sizeof( vk_world.advancedFogTypeFarEnabled ) );
	// World/material globals are the same value in every ring item, so stamp them
	// here at the single push choke point rather than threading them through every
	// caller's local uniform. .x = r_lightmapBoost; .yzw = wetness/frost/snow.
	// Melt is folded into the wet response because thawed accumulation becomes
	// water, while retaining the fixed 640-byte portable UBO ABI. Live climate
	// changes show next frame with no pipeline rebuild or descriptor mutation.
	// r_unbakeStaticLights dims the baked lightmap by the map's tuned factor to bound
	// the additive double-count from the extracted static lights. When off (or no
	// static lights), unbakeDim is exactly 1.0f → the UBO bytes are identical to today
	// (byte-identical OFF path). staticLightDim is 1.0f unless extraction ran.
	{
		float unbakeDim = 1.0f;
		if ( r_unbakeStaticLights && r_unbakeStaticLights->integer && tr.world )
			unbakeDim = tr.world->staticLightDim;
		( (vkUniform_t *)( vk.cmd->vertex_buffer_ptr + offset ) )->worldLightParams[0] =
			( r_lightmapBoost ? r_lightmapBoost->value : 4.6f ) * unbakeDim;
		( (vkUniform_t *)( vk.cmd->vertex_buffer_ptr + offset ) )->worldLightParams[1] =
			Com_Clamp( 0.0f, 1.0f,
				vk.atm.surfaceTargets[0] + 0.5f * vk.atm.surfaceTargets[3] );
		( (vkUniform_t *)( vk.cmd->vertex_buffer_ptr + offset ) )->worldLightParams[2] =
			Com_Clamp( 0.0f, 1.0f, vk.atm.surfaceTargets[1] );
		( (vkUniform_t *)( vk.cmd->vertex_buffer_ptr + offset ) )->worldLightParams[3] =
			Com_Clamp( 0.0f, 1.0f, vk.atm.surfaceTargets[2] );
	}
	// (The bindless packed_indices field is filled later by vk_push_bindless_indices
	// in vk_draw_geometry — which runs AFTER this, at the actual draw, with this
	// draw's per-role images resolved — writing directly into this ring item at
	// vk.cmd->uniform_read_offset. It is NOT copied here: a copy here would carry
	// the previous draw's indices.)
	if ( !vk_tess_publish_shadow_range( offset, vk.uniform_item_size ) ) {
		vk.cmd->uniform_read_offset = ~0U;
		return ~0U;
	}
	vk.cmd->vertex_buffer_offset = offset + vk.uniform_item_size;

	vk_reset_descriptor( VK_DESC_UNIFORM );
	vk_update_descriptor( VK_DESC_UNIFORM, vk.cmd->uniform_descriptor );
	vk_update_descriptor_offset( VK_DESC_UNIFORM, vk.cmd->uniform_read_offset );

	return offset;
}


uint32_t VK_PushUniformScratch( void ) {
	// Allocate a per-draw UBO ring item for a draw path that has no light/fog/
	// shadow inputs of its own (sky, beam, the legacy projected-dlight blend,
	// the debug-tris/normals/show-images overlays, shadow volumes). These paths
	// take their MVP from the vertex push constant and their fragment shaders do
	// not read the bindless index table — but vk_draw_geometry still calls
	// vk_push_bindless_indices, which writes 48 B at vk.cmd->uniform_read_offset.
	// Without a ring item of their own they would inherit the previous draw's
	// offset and overwrite a still-in-flight bindless draw's index table (the GPU
	// reads the ring at execution time, not at record time). A scratch item gives
	// them their own slot so the write is harmless. mvp is stamped from the
	// vk_update_mvp stash (unread by these push-MVP shaders); everything else is
	// zero.
	vkUniform_t scratch;
	memset( &scratch, 0, sizeof( scratch ) );
	return VK_PushUniform( &scratch );
}


#ifdef USE_PMLIGHT
void VK_LightingPass( void )
{
	static uint32_t uniform_offset;
	static int fog_stage;
	uint32_t pipeline;
	const shaderStage_t *pStage;
	cullType_t cull;

	if ( tess.shader->lightingStage < 0 )
		return;

	pStage = tess.xstages[ tess.shader->lightingStage ];

	// we may need to update programs for fog transitions
	if ( tess.dlightUpdateParams ) {

		// fog parameters
		VK_SetFogParams( &uniform, &fog_stage );
		// light parameters
		VK_SetLightParams( &uniform, tess.light );

		tess.dlightUpdateParams = qfalse;
	}

#if FEAT_SHADOW_MAPPING
	// feed the 4 per-cascade light matrices + split distances to
	// the lit shader. These live in proper UBO fields appended at the end of
	// vkUniform_t (std140 offset 144 / 400 / 416) — the lit shader's
	// USE_SHADOWMAP block declares them at matching layout(offset=)s. (No effect
	// when shadow mapping is inactive, or on surfaces that keep a non-shadow
	// pipeline — those just don't read the tail of the UBO.) The cascade matrices
	// are per-light (constant across this light's surfaces); modelMatrix is
	// per-surface (backEnd.or changes per surface) and so is refreshed below on
	// every call, not just on the per-light param update.
	if ( vk.shadowMap.active ) {
		const orientationr_t *o = &backEnd.or;
		memcpy( uniform.cascadeMVP, vk.shadowMap.cascadeMVP, sizeof( uniform.cascadeMVP ) );
		Vector4Copy( vk.shadowMap.cascadeSplits, uniform.cascadeSplits );
		// model->world for this surface so light_vert.tmpl can
		// put shadowData.xyz in world space. backEnd.or is set per surface by
		// R_RotateForEntity — identity (axis = identity, origin = 0) for the
		// worldspawn, the entity's [axis|origin] for entity / brush-model
		// surfaces. Column-major, same convention as cascadeMVP / the caster
		// push constant. (Matches the matrix R_RotateForEntity builds before
		// composing with the world->view matrix.)
		uniform.modelMatrix[ 0] = o->axis[0][0]; uniform.modelMatrix[ 4] = o->axis[1][0]; uniform.modelMatrix[ 8] = o->axis[2][0]; uniform.modelMatrix[12] = o->origin[0];
		uniform.modelMatrix[ 1] = o->axis[0][1]; uniform.modelMatrix[ 5] = o->axis[1][1]; uniform.modelMatrix[ 9] = o->axis[2][1]; uniform.modelMatrix[13] = o->origin[1];
		uniform.modelMatrix[ 2] = o->axis[0][2]; uniform.modelMatrix[ 6] = o->axis[1][2]; uniform.modelMatrix[10] = o->axis[2][2]; uniform.modelMatrix[14] = o->origin[2];
		uniform.modelMatrix[ 3] = 0.0f;          uniform.modelMatrix[ 7] = 0.0f;          uniform.modelMatrix[11] = 0.0f;          uniform.modelMatrix[15] = 1.0f;
	}
#endif

	// Push a fresh per-draw ring item for EVERY lit surface. light_vert reads the
	// MVP from the UBO (ubo.mvp, filled by VK_PushUniform from the per-entity
	// vk_update_mvp stash), and modelMatrix above is per-surface — so reusing one
	// ring item across a light's surfaces (the previous "push only on param
	// change" behaviour) made the 2nd+ entity/brush-model surface read the first
	// surface's stale transform. It also gives each draw its own slot for the
	// bindless index write in vk_draw_geometry. The light/fog/cascade fields
	// persist in the static `uniform` across calls (recomputed only on a light/fog
	// transition above), so re-pushing them per surface is the same byte values.
	uniform_offset = VK_PushUniform( &uniform );

	if ( uniform_offset == ~0 )
		return; // no space left...

	cull = tess.shader->cullType;
	if ( backEnd.viewParms.portalView == PV_MIRROR ) {
		switch ( cull ) {
			case CT_FRONT_SIDED: cull = CT_BACK_SIDED; break;
			case CT_BACK_SIDED: cull = CT_FRONT_SIDED; break;
			default: break;
		}
	}

	int abs_light = /* (pStage->stateBits & GLS_ATEST_BITS) && */ (cull == CT_TWO_SIDED) ? 1 : 0;

	if ( fog_stage ) {
		// legacy-mainpath-retire STEP 2 — legacy ring write retired; bindless track stays.
		vk_bindless_track( 1 /* role: fog (FOG_DLIGHT slot, was set 2) */, tr.fogImage );
	}

	if ( tess.light->linear )
		// NOLINTNEXTLINE(clang-analyzer-security.ArrayBound) — index bounded by upstream invariant (sun-shadow cascades, surfaceIndexSets count, dlight pipeline count); analyzer doesn't see the bound
		pipeline = vk.dlight1_pipelines_x[cull][tess.shader->polygonOffset][fog_stage][abs_light];
	else
		// NOLINTNEXTLINE(clang-analyzer-security.ArrayBound) — index bounded by upstream invariant (sun-shadow cascades, surfaceIndexSets count, dlight pipeline count); analyzer doesn't see the bound
		pipeline = vk.dlight_pipelines_x[cull][tess.shader->polygonOffset][fog_stage][abs_light];

	// the per-map srgb-variant swap
	// is gone — light_frag.tmpl decodes the albedo texel unconditionally
	// now. (The PBR/shadow/parallax swaps below are unaffected.)

#if FEAT_PBR
	// Swap to PBR pipeline when surface has a pbrMap. r_pbr is the
	// runtime gate (live conversion); the pipeline cache
	// (vk_find_pipeline_ext) holds both PBR and non-PBR variants so
	// flipping r_pbr selects between cached pipelines per draw with
	// no rebuild.
	if ( pStage->bundle[3].image[0] && r_pbr->integer ) {
		Vk_Pipeline_Def def;
		vk_get_pipeline_def( pipeline, &def );
		if ( def.shader_type == TYPE_SINGLE_TEXTURE_LIGHTING )
			def.shader_type = TYPE_SINGLE_TEXTURE_LIGHTING_PBR;
		else if ( def.shader_type == TYPE_SINGLE_TEXTURE_LIGHTING_LINEAR )
			def.shader_type = TYPE_SINGLE_TEXTURE_LIGHTING_PBR_LINEAR;
		pipeline = vk_find_pipeline_ext( 0, &def, qtrue );
		// legacy-mainpath-retire STEP 2 — legacy ring write retired; bindless track stays.
		vk_bindless_track( 3 /* role: pbrMap (FOG_COLLAPSE slot, was set 4) */, pStage->bundle[3].image[0] );
	}
#endif

#if FEAT_SHADOW_MAPPING
	// dlight-diagnose-and-fix — the SHADOW pipeline swap is RETIRED for
	// VK_LightingPass. The previous code swapped the per-light pipeline
	// to TYPE_SINGLE_TEXTURE_LIGHTING_SHADOW whenever vk.shadowMap.active,
	// which routed dlight draws through the USE_SHADOWMAP variant of
	// light_frag.tmpl. That variant ends with
	//     intens *= sampleShadow( shadowData.xyz, shadowData.w, ... );  // line ~440
	// — i.e. the SUN's CSM shadow factor multiplies the per-light intens.
	// Dlights (rocket-in-flight glow, explosion light, muzzle-flash) are
	// independent point lights; the SUN's shadow map has nothing to do
	// with whether a given fragment is illuminated by a rocket. For any
	// fragment that lands in sun shadow (most floor/wall geometry hit by
	// a rocket explosion indoors), sampleShadow returns ~0 and the entire
	// dlight contribution gets multiplied to zero → invisible dlights.
	// This is the regression that survived the original
	// dlight-shadowmap-rebind and dlight-lightingpass-rebind
	// fix attempts: every previous fix correctly ensured the shadowMap
	// descriptor was bound for the shader to sample, but the sampling
	// itself was the bug.
	//
	// Diagnosed end-to-end by the dlight-render-path-diagnostic build:
	// probes A/B/B'/D/E/F all confirmed the renderer-side dlight path was
	// complete (dlights created, VK_LightingPass entered, set 8 bound,
	// shader variant SHADOW selected, uniform inputs valid); JPEG
	// before/after captures of bot-fired rocket explosions on arena17
	// showed dlights invisible with r_shadows 1 and visible with
	// r_shadows 0. The probe data named line 440 of light_frag as
	// the lost-contribution site.
	//
	// Retiring the swap leaves dlight draws on the LIGHTING variant
	// (shader_type=7) under both r_shadows settings — matching the
	// working r_shadows 0 case exactly. The dlight-lightingpass-
	// rebind's vk_update_descriptor(WIRED_ENGINE_RES_SET, ...) is also
	// retired here: with no SHADOW variant in this pass, set 8 binding 0
	// is no longer sampled, and the main-pass fold continues to handle
	// set 8 binding 1 (water.frag's screenmap). The SHADOW pipeline
	// variant itself stays compiled (it's still listed in
	// shaders.manifest.mjs and vk_create_pipeline) for future use if a
	// world-shadow main-pass path lands — but until then nothing in
	// renderervk requests it via vk_find_pipeline_ext.
#endif

#if FEAT_ADVANCED_WATER
	// Swap to water pipeline for liquid surfaces (refraction + Fresnel)
	if ( tess.shader->contentFlags & ( CONTENTS_WATER | CONTENTS_LAVA | CONTENTS_SLIME ) ) {
		Vk_Pipeline_Def def;
		vk_get_pipeline_def( pipeline, &def );
		def.shader_type = TYPE_WATER;
		pipeline = vk_find_pipeline_ext( 0, &def, qtrue );
		// the screenmap publish via
		// vk_bindless_track(4, vk_get_screenmap_bindless_image()) is
		// retired. The screenmap now lives in the engine-resources
		// descriptor set (set 8, binding 1) and is bound per-draw via
		// the same fold pattern as set 7 — no per-water-draw publish
		// needed here. The water surface texture (bindless role 0) is
		// still published automatically by GL_Bind→vk_bindless_track at
		// tmu 0 below.
	}
#endif

#if FEAT_PARALLAX_MAPPING
	// Swap to parallax pipeline variant when surface has a normalmap
	if ( r_parallaxMapping->integer && pStage->bundle[2].image[0] ) {
		Vk_Pipeline_Def def;
		vk_get_pipeline_def( pipeline, &def );
		if ( def.shader_type == TYPE_SINGLE_TEXTURE_LIGHTING )
			def.shader_type = TYPE_SINGLE_TEXTURE_LIGHTING_PARALLAX;
		else if ( def.shader_type == TYPE_SINGLE_TEXTURE_LIGHTING_LINEAR )
			def.shader_type = TYPE_SINGLE_TEXTURE_LIGHTING_PARALLAX_LINEAR;
		// a BC5 (2-channel ATI2N/3Dc) normal map only stores
		// X+Y — the shader reconstructs Z. RGB-encoded normal maps (BC1 /
		// BC3 / uncompressed) keep the legacy all-3-channels path. Keys the
		// parallax pipeline variant so the two don't share a pipeline.
		switch ( pStage->bundle[2].image[0]->internalFormat ) {
			case VK_FORMAT_BC5_UNORM_BLOCK: def.normal_format = 1; break;
			case VK_FORMAT_BC5_SNORM_BLOCK: def.normal_format = 2; break;
			default:                        def.normal_format = 0; break;
		}
		pipeline = vk_find_pipeline_ext( 0, &def, qtrue );
		// legacy-mainpath-retire STEP 2 — legacy ring write retired; bindless track stays.
		vk_bindless_track( 5 /* role: normalmap (NORMALMAP slot, was set 6) */, pStage->bundle[2].image[0] );
	}
#endif

	GL_SelectTexture( 0 );
	R_BindAnimatedImage( &pStage->bundle[ tess.shader->lightingBundle ] );

#ifdef USE_VBO
	if ( tess.vboIndex == 0 )
#endif
	{
		R_ComputeTexCoords( tess.shader->lightingBundle, &pStage->bundle[ tess.shader->lightingBundle ] );
	}

	vk_bind_pipeline( pipeline );
	vk_bind_index();
	vk_bind_lighting( tess.shader->lightingStage, tess.shader->lightingBundle );
	vk_draw_geometry( tess.depthRange, qtrue );
}

/*
===================
VK_ForwardPlusPass — the Forward+ tile-sum lit pass (sibling of VK_LightingPass).

Draws ONE plain classic-Phong lit surface through the fp pipeline: the fragment
reads its screen tile's light list (the tile-classification compute's SSBO) and
sums the tile's lights — replacing the per-light additive accumulation with a
single pass. NO per-light params (VK_SetLightParams) — the lights ride the dlight
SSBO in world space; the only per-draw state is the model->world matrix (so the VS
can emit world_pos/world_normal, H1) + the diffuse (role 0, bindless). The per-draw
UBO ring item is pushed fresh per surface (H1 — a stale ring write = wrong-place
lighting). Only reached for surfaces R_LitSurfIsPlain() routed to the union
(variant surfaces stay on VK_LightingPass).
===================
*/
void VK_ForwardPlusPass( void )
{
	static vkUniform_t uniform;
	const shaderStage_t *pStage;
	const orientationr_t *o = &backEnd.or;
	uint32_t uniform_offset;

	if ( tess.shader->lightingStage < 0 )
		return;
	pStage = tess.xstages[ tess.shader->lightingStage ];

	// per-draw model->world (identity for worldspawn, the entity's [axis|origin] for
	// models) — the fp VS reads ubo.modelMatrix@416 to put world_pos/world_normal in
	// world space. backEnd.or is set per surface by R_RotateForEntity in the union
	// walk. Column-major, same convention as VK_LightingPass's shadow modelMatrix.
	memset( &uniform, 0, sizeof( uniform ) );
	uniform.modelMatrix[ 0] = o->axis[0][0]; uniform.modelMatrix[ 4] = o->axis[1][0]; uniform.modelMatrix[ 8] = o->axis[2][0]; uniform.modelMatrix[12] = o->origin[0];
	uniform.modelMatrix[ 1] = o->axis[0][1]; uniform.modelMatrix[ 5] = o->axis[1][1]; uniform.modelMatrix[ 9] = o->axis[2][1]; uniform.modelMatrix[13] = o->origin[1];
	uniform.modelMatrix[ 2] = o->axis[0][2]; uniform.modelMatrix[ 6] = o->axis[1][2]; uniform.modelMatrix[10] = o->axis[2][2]; uniform.modelMatrix[14] = o->origin[2];
	uniform.modelMatrix[ 3] = 0.0f;          uniform.modelMatrix[ 7] = 0.0f;          uniform.modelMatrix[11] = 0.0f;          uniform.modelMatrix[15] = 1.0f;
	// eyePos for the VS world-view vector (world-space eye).
	VectorCopy( backEnd.viewParms.or.origin, uniform.eyePos );

	// diffuse → bindless role 0 (the fp frag reads texture0 == role 0, same as
	// light_frag). GL_Bind (via R_BindAnimatedImage) tracks the role-0 slot.
	GL_SelectTexture( 0 );
	R_BindAnimatedImage( &pStage->bundle[ tess.shader->lightingBundle ] );
#ifdef USE_VBO
	if ( tess.vboIndex == 0 )
#endif
	{
		R_ComputeTexCoords( tess.shader->lightingBundle, &pStage->bundle[ tess.shader->lightingBundle ] );
	}

	// push a fresh ring item for THIS draw (mvp from the per-entity vk_update_mvp
	// stash filled in the union walk; modelMatrix above), then bind the fp pipeline
	// + set 2 (tile lights) + geometry + draw. vk_draw_forwardplus is the vk.c helper
	// that binds ral_fpLitPipeline + the fp set-2 descriptor at draw time.
	uniform_offset = VK_PushUniform( &uniform );
	if ( uniform_offset == ~0u )
		return;

	vk_bind_index();
	vk_bind_lighting( tess.shader->lightingStage, tess.shader->lightingBundle );
	vk_draw_forwardplus( tess.depthRange );
}
#endif // USE_PMLIGHT


void RB_StageIteratorGeneric( void )
{
// NOLINTNEXTLINE(readability-redundant-preprocessor) — branch retained for portability with non-Vulkan derivatives
#ifdef USE_VULKAN
	qboolean rebindIndex = qfalse;
#endif
	qboolean fogCollapse = qfalse;


#ifdef USE_VBO
	if ( tess.vboIndex != 0 ) {
		VBO_PrepareQueues(); // must run before PMLIGHT check (VK_LightingPass needs sorted IBO)
		tess.vboStage = 0;
	} else
#endif
	RB_DeformTessGeometry();

#ifdef USE_PMLIGHT
	// NOLINTNEXTLINE(readability-misleading-indentation) — Q3 split-else-if / preprocessor-conditional idiom; statement is at correct enclosing scope
	if ( tess.forwardPlusPass ) {
		VK_ForwardPlusPass();
		return;
	}
	// NOLINTNEXTLINE(readability-misleading-indentation) — Q3 split-else-if / preprocessor-conditional idiom; statement is at correct enclosing scope
	if ( tess.dlightPass ) {
		VK_LightingPass();
		return;
	}
#endif

	// Q1 sky surfaces route here: Q1TC_SKY shaders drop
	// shader.isSky, so ComputeStageIteratorFunc picks the generic
	// iterator. The dlightPass early-return above means this only runs
	// for a real sky-surface draw. r_drawSky gates the sky pass
	// uniformly with Q3 (RB_StageIteratorSky); backEnd.skyRenderedThisView
	// restores RB_DrawSun's gate — RB_StageIteratorSky was the only
	// writer of that flag, and Q1 sky no longer routes there.
	if ( tess.shader->surfaceFlags & SURF_SKY ) {
		if ( !r_drawSky->integer )
			return;
		backEnd.skyRenderedThisView = qtrue;
	}

#ifdef USE_FOG_COLLAPSE
	fogCollapse = tess.fogNum && tess.shader->fogPass && tess.shader->fogCollapse;
#endif

#ifdef USE_VBO
	// lightstyle: dispatch one sub-group per unique lightStyles signature so each
	// gets its own VK_PushUniform with the correct q1StyleIntensities.
	// VBO_PrepareQueues already sorted the queue; iterate in sorted order.
	if ( tess.vboIndex != 0 && tr.world && tr.world->lightStyles ) {
		int pos = 0;
		const int total = VBO_GetQueueCount();
		while ( pos < total ) {
			uint32_t curPacked = VBO_GetQueueItemStylesPacked( pos );
			int n = 1;
			while ( pos + n < total && VBO_GetQueueItemStylesPacked( pos + n ) == curPacked )
				n++;
			memcpy( tess.q1SurfaceStyles, &curPacked, 4 );
			VBO_PrepareSubqueue( pos, n );
			tess.vboStage = 0;
			RB_IterateStagesGeneric( &tess, fogCollapse );
			pos += n;
		}
		return;
	}
#endif

	// call shader function
	RB_IterateStagesGeneric( &tess, fogCollapse );

	// (The legacy fake-dlight extra pass is retired — per-pixel dynamic lights are
	// added in the dedicated lit pass, not as a per-surface texture-projection pass
	// here. rebindIndex stays qfalse: there is no legacy dlight pass to rebind after.)

	// NOLINTNEXTLINE(readability-misleading-indentation) — Q3 split-else-if / preprocessor-conditional idiom; statement is at correct enclosing scope
	// now do fog
	// NOLINTNEXTLINE(readability-misleading-indentation) — Q3 split-else-if pattern; statement at function scope
	if ( tess.fogNum && tess.shader->fogPass && !fogCollapse ) {
// NOLINTNEXTLINE(readability-redundant-preprocessor) — branch retained for portability with non-Vulkan derivatives
#ifdef USE_VULKAN
		RB_FogPass( rebindIndex );
#else
		RB_FogPass();
#endif
	}
}

#else

/*
** RB_StageIteratorGeneric
*/
void RB_StageIteratorGeneric( void )
{
	const shaderCommands_t *input;
	shader_t		*shader;

	RB_DeformTessGeometry();

	input = &tess;
	shader = input->shader;

	//
	// set face culling appropriately
	//
	GL_Cull( shader->cullType );

	// set polygon offset if necessary
	if ( shader->polygonOffset )
	{
		qglEnable( GL_POLYGON_OFFSET_FILL );
		qglPolygonOffset( r_offsetFactor->value, r_offsetUnits->value );
	}

	//
	// if there is only a single pass then we can enable color
	// and texture arrays before we compile, otherwise we need
	// to avoid compiling those arrays since they will change
	// during multipass rendering
	//
	if ( tess.numPasses > 1 )
	{
		setArraysOnce = qfalse;

		GL_ClientState( 1, CLS_NONE );
		GL_ClientState( 0, CLS_NONE );
	}
	else
	{
		// FIXME: we can't do that if going to lighting/fog later?
		setArraysOnce = qtrue;

		GL_ClientState( 0, CLS_COLOR_ARRAY | CLS_TEXCOORD_ARRAY );

		if ( tess.xstages[0] )
		{
			R_ComputeColors( 0, tess.svars.colors, tess.xstages[0] );
			qglColorPointer( 4, GL_UNSIGNED_BYTE, 0, tess.svars.colors[0].rgba );
			R_ComputeTexCoords( 0, &tess.xstages[0]->bundle[0] );
			qglTexCoordPointer( 2, GL_FLOAT, 0, tess.svars.texcoordPtr[0] );
			if ( shader->multitextureEnv )
			{
				GL_ClientState( 1, CLS_TEXCOORD_ARRAY );
				R_ComputeTexCoords( 1, &tess.xstages[0]->bundle[1] );
				qglTexCoordPointer( 2, GL_FLOAT, 0, tess.svars.texcoordPtr[1] );
			}
			else
			{
				GL_ClientState( 1, CLS_NONE );
			}
		}
	}

	qglVertexPointer( 3, GL_FLOAT, sizeof( input->xyz[0] ), input->xyz ); // padded for SIMD

	//
	// lock XYZ
	//
	if ( qglLockArraysEXT )
	{
		qglLockArraysEXT( 0, input->numVertexes );
	}

	//
	// call shader function
	//
	RB_IterateStagesGeneric( input );

	//
	// now do any dynamic lighting needed
	//
	if ( tess.dlightBits && tess.shader->sort <= SS_OPAQUE && !(tess.shader->surfaceFlags & (SURF_NODLIGHT | SURF_SKY) ) )
	{
		ProjectDlightTexture();
	}

	//
	// now do fog
	//
	if ( tess.fogNum && tess.shader->fogPass )
	{
		RB_FogPass();
	}

	//
	// unlock arrays
	//
	if ( qglUnlockArraysEXT )
	{
		qglUnlockArraysEXT();
	}

	GL_ClientState( 1, CLS_NONE );

	//
	// reset polygon offset
	//
	if ( shader->polygonOffset )
	{
		qglDisable( GL_POLYGON_OFFSET_FILL );
	}
}
#endif // !USE_VULKAN


/*
** RB_EndSurface
*/
void RB_EndSurface( void ) {
	const shaderCommands_t *input;

	input = &tess;

	if ( input->numIndexes == 0 ) {
		//VBO_UnBind();
		return;
	}

	if ( input->numIndexes > SHADER_MAX_INDEXES ) {
		ri.Terminate( TERM_CLIENT_DROP, "RB_EndSurface() - SHADER_MAX_INDEXES hit" );
	}

	if ( input->numVertexes > SHADER_MAX_VERTEXES ) {
		ri.Terminate( TERM_CLIENT_DROP, "RB_EndSurface() - SHADER_MAX_VERTEXES hit" );
	}

	if ( tess.shader == tr.shadowShader ) {
		RB_ShadowTessEnd();
		return;
	}

	// for debugging of sort order issues, stop rendering after a given sort value
	if ( r_debugSort->integer && r_debugSort->integer < tess.shader->sort && !backEnd.doneSurfaces ) {
#ifdef USE_VBO
		tess.vboIndex = 0; //VBO_UnBind();
#endif
		return;
	}

	//
	// update performance counters
	//
#ifdef USE_PMLIGHT
	if ( tess.dlightPass ) {
		backEnd.pc.c_lit_batches++;
		backEnd.pc.c_lit_vertices += tess.numVertexes;
		backEnd.pc.c_lit_indices += tess.numIndexes;
	} else
#endif
	{
		backEnd.pc.c_shaders++;
		backEnd.pc.c_vertexes += tess.numVertexes;
		backEnd.pc.c_indexes += tess.numIndexes;
		if ( backEnd.projection2D ) {
			int batchClass;
			backEnd.pc.c_2dShaders++;
			if ( tess.shader == tr.whiteShader ) {
				backEnd.pc.c_2dWhiteShaders++;
				batchClass = 1;
			} else if ( tess.shader->msdf ) {
				backEnd.pc.c_2dMsdfShaders++;
				batchClass = 2;
			} else {
				backEnd.pc.c_2dOtherShaders++;
				batchClass = 3;
			}
			if ( ( backEnd.pc.c_2dLastClass == 1 && batchClass == 2 )
			  || ( backEnd.pc.c_2dLastClass == 2 && batchClass == 1 ) ) {
				backEnd.pc.c_2dWhiteMsdfTransitions++;
			}
			backEnd.pc.c_2dLastClass = batchClass;
		}
	}
	backEnd.pc.c_totalIndexes += tess.numIndexes * tess.numPasses;

	//
	// call off to shader specific tess end function
	//
	tess.shader->optimalStageIteratorFunc();

	//
	// draw debugging stuff
	//
	if ( r_showTris->integer ) {
		DrawTris( input );
	}
	if ( r_showNormals->integer ) {
		DrawNormals( input );
	}

	// clear shader so we can tell we don't have any unclosed surfaces
	tess.numIndexes = 0;
	tess.numVertexes = 0;

#ifdef USE_VBO
	tess.vboIndex = 0;
	//VBO_ClearQueue();
#endif
}
