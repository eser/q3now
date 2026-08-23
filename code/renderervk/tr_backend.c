// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 1999-2005 Id Software, Inc.
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#include "tr_local.h"
#include "tr_temporal_iqm_motion.h"
#include "../renderercommon/r_log.h"  // rilog-channel-mechanism Turn B — renderer.cmd
#include "../qcommon/q_feats.h"
#include "../renderer/ral/ral.h"       // Ral_Cmd* — RB_MenuBackdrop's fullscreen RAL draw

R_LOG_DECLARE_CHANNEL( rch_cmd,  "renderer.cmd"  );
R_LOG_DECLARE_CHANNEL( rch_rdoc, "renderer.rdoc" );

// TEMPORARY RenderDoc capture trigger (REMOVE later,
// together with the WN_RDOC_CAPTURE block at the end of RB_SwapBuffers
// and code/renderervk/renderdoc_app.h). Gated on WN_RDOC_CAPTURE.
// armed (1) to support tools/visual_regression/vr_check.py,
// which launches via `renderdoccmd capture` and relies on the in-app
// TriggerCapture()+quit here. A normal launch (renderdoc.dll not in the
// process) just logs one line and continues — it does NOT auto-quit
// (the trigger/quit are gated on `s_rdoc != NULL`). Revert to 0 (or
// remove the block) when the vr/RenderDoc work is done.
#ifndef WN_RDOC_CAPTURE
#define WN_RDOC_CAPTURE 1
#endif
#if WN_RDOC_CAPTURE
#if defined( _WIN32 )
#include <windows.h>
#endif
#include <stdbool.h>
#include <stdint.h>
#include "renderdoc_app.h"
#endif

backEndData_t	*backEndData;
backEndState_t	backEnd;

#ifndef USE_VULKAN
static const float s_flipMatrix[16] = {
	// convert from our coordinate system (looking down X)
	// to OpenGL's coordinate system (looking down -Z)
	0, 0, -1, 0,
	-1, 0, 0, 0,
	0, 1, 0, 0,
	0, 0, 0, 1
};


const float *GL_Ortho( const float left, const float right, const float bottom, const float top, const float znear, const float zfar )
{
	static float m[ 16 ] = { 0 };

	m[0] = 2.0f / (right - left);
	m[5] = 2.0f / (top - bottom);
	m[10] = - 2.0f / (zfar - znear);
	m[12] = - (right + left)/(right - left);
	m[13] = - (top + bottom) / (top - bottom);
	m[14] = - (zfar + znear) / (zfar - znear);
	m[15] = 1.0f;

	return m;
}
#endif


/*
** GL_Bind
*/
void GL_Bind( image_t *image ) {
#ifdef USE_VULKAN
	if ( !image ) {
		R_LOG( rch_cmd, SEV_WARN, "GL_Bind: NULL image\n" );
		image = tr.defaultImage;
	}

	if ( r_nobind->integer && tr.dlightImage ) {		// performance evaluation option
		image = tr.dlightImage;
	}

	//if ( glState.currenttextures[glState.currenttmu] != texnum ) {
		image->frameUsed = tr.frameCount;
		// the legacy-mainpath-retire work retired the legacy ring write at
		// `currenttmu + 1`. Its last live consumer was MSDF
		// (set=1 atlas binding), which that work relocated onto the RAL-owned
		// bindless 2D table via a new push constant in
		// vk.pipeline_layout_msdf. Fog was already off the ring by then;
		// q1_ls / q1_ls_array / water are migrated separately
		// (water → bindless slot WIRED_BINDLESS_SCREENMAP_SLOT for the
		// refraction screenmap; q1_ls_array → bindless 2DArray binding=2).
		// A later change deleted vk.cmd->descriptor_set.current[1..6] outright
		// (the array now sizes to [WIRED_ENGINE_RES_SET+1]=3), so this site
		// retains only the vk_bindless_track call (the bindless-table mirror);
		// any write targeting slots 3+ would be out-of-bounds.
		vk_bindless_track( glState.currenttmu, image );

	//}
#else
	GLuint texnum;

	if ( !image ) {
		R_LOG( rch_cmd, SEV_WARN, "GL_Bind: NULL image\n" );
		texnum = tr.defaultImage->texnum;
	} else {
		texnum = image->texnum;
	}

	if ( r_nobind->integer && tr.dlightImage ) {		// performance evaluation option
		texnum = tr.dlightImage->texnum;
	}

	if ( glState.currenttextures[glState.currenttmu] != texnum ) {
		if ( image ) {
			image->frameUsed = tr.frameCount;
		}
		glState.currenttextures[glState.currenttmu] = texnum;
		qglBindTexture (GL_TEXTURE_2D, texnum);
	}
#endif
}


/*
** GL_SelectTexture
*/
void GL_SelectTexture( int unit )
{
#ifndef USE_VULKAN
	if ( glState.currenttmu == unit )
	{
		return;
	}
#endif

	if ( unit >= glConfig.numTextureUnits )
	{
		ri.Terminate( TERM_CLIENT_DROP, "GL_SelectTexture: unit = %i", unit );
	}
#ifndef USE_VULKAN
	qglActiveTextureARB( GL_TEXTURE0_ARB + unit );
#endif
	glState.currenttmu = unit;
}


/*
** GL_SelectClientTexture
*/
#ifndef USE_VULKAN
static void GL_SelectClientTexture( int unit )
{
	if ( glState.currentArray == unit )
	{
		return;
	}

	if ( unit >= glConfig.numTextureUnits )
	{
		ri.Terminate( TERM_CLIENT_DROP, "GL_SelectClientTexture: unit = %i", unit );
	}

	qglClientActiveTextureARB( GL_TEXTURE0_ARB + unit );

	glState.currentArray = unit;
}
#endif


/*
** GL_Cull
*/
void GL_Cull( cullType_t cullType ) {
	if ( glState.faceCulling == cullType ) {
		return;
	}

	glState.faceCulling = cullType;
#ifndef USE_VULKAN
	if ( cullType == CT_TWO_SIDED )
	{
		qglDisable( GL_CULL_FACE );
	}
	else
	{
		qboolean cullFront;
		qglEnable( GL_CULL_FACE );

		cullFront = (cullType == CT_FRONT_SIDED);
		if ( backEnd.viewParms.portalView == PV_MIRROR )
		{
			cullFront = !cullFront;
		}

		qglCullFace( cullFront ? GL_FRONT : GL_BACK );
	}
#endif
}


/*
** GL_TexEnv
*/
void GL_TexEnv( GLint env )
{
#ifndef USE_VULKAN
	if ( env == glState.texEnv[ glState.currenttmu ] )
		return;

	glState.texEnv[ glState.currenttmu ] = env;

	switch ( env )
	{
	case GL_MODULATE:
	case GL_REPLACE:
	case GL_DECAL:
	case GL_ADD:
		qglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, env );
		break;
	default:
		ri.Terminate( TERM_CLIENT_DROP, "GL_TexEnv: invalid env '%d' passed", env );
		break;
	}
#endif
}


/*
** GL_State
**
** This routine is responsible for setting the most commonly changed state
** in Q3.
*/
void GL_State( unsigned stateBits )
{
#ifndef USE_VULKAN
	unsigned diff = stateBits ^ glState.glStateBits;

	if ( !diff )
	{
		return;
	}

	//
	// check depthFunc bits
	//
	if ( diff & GLS_DEPTHFUNC_EQUAL )
	{
		if ( stateBits & GLS_DEPTHFUNC_EQUAL )
		{
			qglDepthFunc( GL_EQUAL );
		}
		else
		{
			qglDepthFunc( GL_LEQUAL );
		}
	}

	//
	// check blend bits
	//
	if ( diff & GLS_BLEND_BITS )
	{
		GLenum srcFactor = GL_ONE, dstFactor = GL_ONE;

		if ( stateBits & GLS_BLEND_BITS )
		{
			switch ( stateBits & GLS_SRCBLEND_BITS )
			{
			case GLS_SRCBLEND_ZERO:
				srcFactor = GL_ZERO;
				break;
			case GLS_SRCBLEND_ONE:
				srcFactor = GL_ONE;
				break;
			case GLS_SRCBLEND_DST_COLOR:
				srcFactor = GL_DST_COLOR;
				break;
			case GLS_SRCBLEND_ONE_MINUS_DST_COLOR:
				srcFactor = GL_ONE_MINUS_DST_COLOR;
				break;
			case GLS_SRCBLEND_SRC_ALPHA:
				srcFactor = GL_SRC_ALPHA;
				break;
			case GLS_SRCBLEND_ONE_MINUS_SRC_ALPHA:
				srcFactor = GL_ONE_MINUS_SRC_ALPHA;
				break;
			case GLS_SRCBLEND_DST_ALPHA:
				srcFactor = GL_DST_ALPHA;
				break;
			case GLS_SRCBLEND_ONE_MINUS_DST_ALPHA:
				srcFactor = GL_ONE_MINUS_DST_ALPHA;
				break;
			case GLS_SRCBLEND_ALPHA_SATURATE:
				srcFactor = GL_SRC_ALPHA_SATURATE;
				break;
			default:
				ri.Terminate( TERM_CLIENT_DROP, "GL_State: invalid src blend state bits" );
				break;
			}

			switch ( stateBits & GLS_DSTBLEND_BITS )
			{
			case GLS_DSTBLEND_ZERO:
				dstFactor = GL_ZERO;
				break;
			case GLS_DSTBLEND_ONE:
				dstFactor = GL_ONE;
				break;
			case GLS_DSTBLEND_SRC_COLOR:
				dstFactor = GL_SRC_COLOR;
				break;
			case GLS_DSTBLEND_ONE_MINUS_SRC_COLOR:
				dstFactor = GL_ONE_MINUS_SRC_COLOR;
				break;
			case GLS_DSTBLEND_SRC_ALPHA:
				dstFactor = GL_SRC_ALPHA;
				break;
			case GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA:
				dstFactor = GL_ONE_MINUS_SRC_ALPHA;
				break;
			case GLS_DSTBLEND_DST_ALPHA:
				dstFactor = GL_DST_ALPHA;
				break;
			case GLS_DSTBLEND_ONE_MINUS_DST_ALPHA:
				dstFactor = GL_ONE_MINUS_DST_ALPHA;
				break;
			default:
				ri.Terminate( TERM_CLIENT_DROP, "GL_State: invalid dst blend state bits" );
				break;
			}

			qglEnable( GL_BLEND );
			qglBlendFunc( srcFactor, dstFactor );
		}
		else
		{
			qglDisable( GL_BLEND );
		}
	}

	//
	// check depthmask
	//
	if ( diff & GLS_DEPTHMASK_TRUE )
	{
		if ( stateBits & GLS_DEPTHMASK_TRUE )
		{
			qglDepthMask( GL_TRUE );
		}
		else
		{
			qglDepthMask( GL_FALSE );
		}
	}

	//
	// fill/line mode
	//
	if ( diff & GLS_POLYMODE_LINE )
	{
		if ( stateBits & GLS_POLYMODE_LINE )
		{
			qglPolygonMode( GL_FRONT_AND_BACK, GL_LINE );
		}
		else
		{
			qglPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
		}
	}

	//
	// depthtest
	//
	if ( diff & GLS_DEPTHTEST_DISABLE )
	{
		if ( stateBits & GLS_DEPTHTEST_DISABLE )
		{
			qglDisable( GL_DEPTH_TEST );
		}
		else
		{
			qglEnable( GL_DEPTH_TEST );
		}
	}

	//
	// alpha test
	//
	if ( diff & GLS_ATEST_BITS )
	{
		switch ( stateBits & GLS_ATEST_BITS )
		{
		case 0:
			qglDisable( GL_ALPHA_TEST );
			break;
		case GLS_ATEST_GT_0:
			qglEnable( GL_ALPHA_TEST );
			qglAlphaFunc( GL_GREATER, 0.0f );
			break;
		case GLS_ATEST_LT_80:
			qglEnable( GL_ALPHA_TEST );
			qglAlphaFunc( GL_LESS, 0.5f );
			break;
		case GLS_ATEST_GE_80:
			qglEnable( GL_ALPHA_TEST );
			qglAlphaFunc( GL_GEQUAL, 0.5f );
			break;
		default:
			ri.Terminate( TERM_CLIENT_DROP, "GL_State: invalid alpha test bits" );
			break;
		}
	}

	glState.glStateBits = stateBits;
#endif // USE_VULKAN
}


#ifndef USE_VULKAN
void GL_ClientState( int unit, unsigned stateBits )
{
	unsigned diff = stateBits ^ glState.glClientStateBits[ unit ];

	if ( diff == 0 )
	{
		if ( stateBits )
		{
			GL_SelectClientTexture( unit );
		}
		return;
	}

	GL_SelectClientTexture( unit );

	if ( diff & CLS_COLOR_ARRAY )
	{
		if ( stateBits & CLS_COLOR_ARRAY )
			qglEnableClientState( GL_COLOR_ARRAY );
		else
			qglDisableClientState( GL_COLOR_ARRAY );
	}

	if ( diff & CLS_NORMAL_ARRAY )
	{
		if ( stateBits & CLS_NORMAL_ARRAY )
			qglEnableClientState( GL_NORMAL_ARRAY );
		else
			qglDisableClientState( GL_NORMAL_ARRAY );
	}

	if ( diff & CLS_TEXCOORD_ARRAY )
	{
		if ( stateBits & CLS_TEXCOORD_ARRAY )
			qglEnableClientState( GL_TEXTURE_COORD_ARRAY );
		else
			qglDisableClientState( GL_TEXTURE_COORD_ARRAY );
	}

	glState.glClientStateBits[ unit ] = stateBits;
}
#endif


static void RB_SetGL2D( void );

/*
================
RB_Hyperspace

A player has predicted a teleport, but hasn't arrived yet
================
*/
static void RB_Hyperspace( void ) {
	color4ub_t c;

	if ( !backEnd.isHyperspace ) {
		// do initialization shit
	}

	if ( tess.shader != tr.whiteShader ) {
		RB_EndSurface();
		RB_SetGL2D();
		RB_BeginSurface( tr.whiteShader, 0 );
	}

#ifdef USE_VBO
	VBO_UnBind();
#endif

	RB_SetGL2D();

	if ( r_teleporterFlash->integer == 0 ) {
		c.rgba[0] = c.rgba[1] = c.rgba[2] = 0; // fade to black
	} else {
		c.rgba[0] = c.rgba[1] = c.rgba[2] = (backEnd.refdef.time & 255); // fade to white
	}
	c.rgba[3] = 255;

	RB_AddQuadStamp2( backEnd.refdef.x, backEnd.refdef.y, backEnd.refdef.width, backEnd.refdef.height,
		0.0, 0.0, 0.0, 0.0, c );

	RB_EndSurface();

	tess.numIndexes = 0;
	tess.numVertexes = 0;

	backEnd.isHyperspace = qtrue;
}


static void SetViewportAndScissor( void ) {
#ifdef USE_VULKAN
	//memcpy( vk_world.modelview_transform, backEnd.or.modelMatrix, 64 );
	//vk_update_mvp();
	// force depth range and viewport/scissor updates
	vk.cmd->depth_range = DEPTH_RANGE_COUNT;
#else
	qglMatrixMode(GL_PROJECTION);
	qglLoadMatrixf( backEnd.viewParms.projectionMatrix );
	qglMatrixMode(GL_MODELVIEW);

	// set the window clipping
	qglViewport( backEnd.viewParms.viewportX, backEnd.viewParms.viewportY,
		backEnd.viewParms.viewportWidth, backEnd.viewParms.viewportHeight );
	qglScissor( backEnd.viewParms.scissorX, backEnd.viewParms.scissorY,
		backEnd.viewParms.scissorWidth, backEnd.viewParms.scissorHeight );
#endif
}


/*
=================
RB_BeginDrawingView

Any mirrored or portaled views have already been drawn, so prepare
to actually render the visible surfaces for this view
=================
*/
static void RB_BeginDrawingView( void ) {
#ifndef USE_VULKAN
	int clearBits = 0;
#endif

	// sync with gl if needed
	if ( r_finish->integer == 1 && !glState.finishCalled ) {
#ifdef USE_VULKAN
		vk_queue_wait_idle();
#else
		qglFinish();
#endif
		glState.finishCalled = qtrue;
	} else if ( r_finish->integer == 0 ) {
		glState.finishCalled = qtrue;
	}

	// we will need to change the projection matrix before drawing
	// 2D images again
	backEnd.projection2D = qfalse;

	//
	// set the modelview matrix for the viewer
	//
	SetViewportAndScissor();

#ifdef USE_VULKAN
	vk_clear_depth( qtrue );
#else
	// ensures that depth writes are enabled for the depth clear
	GL_State( GLS_DEFAULT );

	// clear relevant buffers
	clearBits = GL_DEPTH_BUFFER_BIT;

	// stencil-volume shadows retired; no stencil buffer to clear.
	if ( 0 && r_fastsky->integer && !( backEnd.refdef.rdflags & RDF_NOWORLDMODEL ) )
	{
		clearBits |= GL_COLOR_BUFFER_BIT;	// FIXME: only if sky shaders have been used
#ifdef _DEBUG
		qglClearColor( 0.8f, 0.7f, 0.4f, 1.0f );	// FIXME: get color of sky
#else
		qglClearColor( 0.0f, 0.0f, 0.0f, 1.0f );	// FIXME: get color of sky
#endif
	}
	qglClear( clearBits );
#endif

	if ( backEnd.refdef.rdflags & RDF_HYPERSPACE ) {
		RB_Hyperspace();
		backEnd.projection2D = qfalse;
		SetViewportAndScissor();
	} else {
		backEnd.isHyperspace = qfalse;
	}

	glState.faceCulling = -1;		// force face culling to set next time

	// we will only draw a sun if there was sky rendered in this view
	backEnd.skyRenderedThisView = qfalse;
}

#ifdef USE_PMLIGHT
static void RB_LightingPass( void );
#ifdef USE_VULKAN
static void RB_RenderForwardPlusUnion( void );
#endif
#endif


/*
==================
RB_RenderDrawSurfList
==================
*/
#ifdef USE_VULKAN
static void RB_InvokeDrawSurf( drawSurf_t *drawSurf, uint32_t absoluteOrdinal,
		qboolean temporalPrimaryCommand ) {
	if ( temporalPrimaryCommand )
		vk_temporal_iqm_publish_drawsurf_ordinal( absoluteOrdinal );
	rb_surfaceTable[ *drawSurf->surface ]( drawSurf->surface );
	if ( temporalPrimaryCommand ) vk_temporal_iqm_reset_drawsurf_ordinal();
}
#endif

static void RB_RenderDrawSurfList( drawSurf_t *drawSurfs, int numDrawSurfs
#ifdef USE_VULKAN
		, qboolean temporalPrimaryCommand
#endif
		) {
	shader_t		*shader, *oldShader;
	int				fogNum;
	int				entityNum, oldEntityNum;
	int				dlighted;
	qboolean		depthRange, isCrosshair;
#ifndef USE_VULKAN
	qboolean		oldDepthRange, wasCrosshair;
#endif
	int				i;
	drawSurf_t		*drawSurf;
	unsigned int	oldSort;
#ifdef USE_PMLIGHT
	float			oldShaderSort;
#endif
	double			originalTime; // -EC-
	qboolean		pinShaderTime;

	// save original time for entity shader offsets
	originalTime = backEnd.refdef.floatTime;
	pinShaderTime = r_pinShaderTime->value > 0.0f;

	// draw everything
	oldEntityNum = -1;
	backEnd.currentEntity = &tr.worldEntity;
	oldShader = NULL;
#ifndef USE_VULKAN
	oldDepthRange = qfalse;
	wasCrosshair = qfalse;
#endif
	oldSort = MAX_UINT;
#ifdef USE_PMLIGHT
	oldShaderSort = -1;
#endif
	depthRange = qfalse;

	backEnd.pc.c_surfaces += numDrawSurfs;

	for (i = 0, drawSurf = drawSurfs ; i < numDrawSurfs ; i++, drawSurf++) {
		if ( drawSurf->sort == oldSort ) {
			// fast path, same as previous sort
#ifdef USE_VULKAN
			RB_InvokeDrawSurf( drawSurf, (uint32_t)i,
				temporalPrimaryCommand );
#else
			rb_surfaceTable[ *drawSurf->surface ]( drawSurf->surface );
#endif
			continue;
		}

		R_DecomposeSort( drawSurf->sort, &entityNum, &shader, &fogNum, &dlighted );
#ifdef USE_VULKAN
		if ( vk.renderPassIndex == RENDER_PASS_SCREENMAP && entityNum != REFENTITYNUM_WORLD && backEnd.refdef.entities[ entityNum ].e.renderfx & RF_DEPTHHACK ) {
			continue;
		}
#endif
		//
		// change the tess parameters if needed
		// a "entityMergable" shader is a shader that can have surfaces from separate
		// entities merged into a single batch, like smoke and blood puff sprites
		if ( ( (oldSort ^ drawSurf->sort ) & ~QSORT_REFENTITYNUM_MASK ) || !shader->entityMergable ) {
			//if ( oldShader != NULL ) {
				RB_EndSurface();
			//}
#ifdef USE_PMLIGHT
			#define INSERT_POINT SS_FOG
			if ( ( backEnd.refdef.numLitSurfs
#ifdef USE_VULKAN
				|| ( r_forwardPlus && r_forwardPlus->integer && vk.fpActive && tr.refdef.numFpUnionSurfs > 0 )
#endif
				) && oldShaderSort < INSERT_POINT && shader->sort >= INSERT_POINT ) {
				//RB_BeginDrawingLitSurfs(); // no need, already setup in RB_BeginDrawingView()
#ifdef USE_VULKAN
				// Forward+ XOR-split: the per-light pass draws VARIANT surfaces (it
				// skips the plain ones, RB_RenderLitSurfList); the tile-sum pass draws
				// the PLAIN union once. When r_forwardPlus 0, RB_RenderForwardPlusUnion
				// is a no-op (empty union) and everything stays on the per-light path.
				if ( backEnd.refdef.numLitSurfs )
					RB_LightingPass();
				if ( r_forwardPlus && r_forwardPlus->integer && vk.fpActive )
					RB_RenderForwardPlusUnion();
#else
				if ( depthRange ) {
					qglDepthRange( 0, 1 );
					RB_LightingPass();
					qglDepthRange( 0, 0.3 );
				} else {
					RB_LightingPass();
				}
#endif
				oldEntityNum = -1; // force matrix setup
			}
#ifdef USE_VULKAN
			// Scene-depth copy. The natural snapshot is taken at the
			// opaque->transparent (SS_FOG) boundary. A consumer that draws BEFORE
			// that boundary — soft-particle depth fade can sort as early as
			// SS_DECAL — needs the copy earlier, or it reads the prior frame's (or
			// undefined) depth. When such a consumer is active, force the copy at
			// the SS_OPAQUE->SS_DECAL crossing, the latest point that still precedes
			// the earliest pre-fog consumer. The opaque scene depth is final here;
			// any alpha-tested see-through depth in the SS_DECAL..SS_BANNER band is
			// not yet present (those surfaces draw after this point), so a sprite
			// that sorts that early fades against opaque depth only — an inherent
			// consequence of its sort order, not of the copy point. vk_scene_depth_copy
			// self-guards on .copied, so the SS_FOG block below then no-ops; when no
			// pre-SS_FOG consumer is active this block is skipped entirely and the
			// SS_FOG snapshot stands unchanged (byte-identical default path).
			if ( vk_scene_depth_early_produce()
			     && oldShaderSort < SS_DECAL && shader->sort >= SS_DECAL ) {
				vk_scene_depth_copy();
			}
			// depth fade: copy depth buffer at opaque->transparent boundary
			if ( oldShaderSort < INSERT_POINT && shader->sort >= INSERT_POINT ) {
				vk_scene_depth_copy();
			}
#endif
			oldShaderSort = shader->sort;
#endif
			RB_BeginSurface( shader, fogNum );
			oldShader = shader;
		}

		oldSort = drawSurf->sort;

		//
		// change the modelview matrix if needed
		//
		if ( entityNum != oldEntityNum ) {
			depthRange = isCrosshair = qfalse;

			if ( entityNum != REFENTITYNUM_WORLD ) {
				backEnd.currentEntity = &backEnd.refdef.entities[entityNum];
				/* When r_pinShaderTime is active, force per-entity shader time
				 * to the pinned global (originalTime) instead of the cg.time-
				 * derived `e.shaderTime`. cgame stamps `e.shaderTime` from
				 * `cg.time` for spawn-flash / item-rotation / effect-lifetime
				 * entities; `cg.time` is server-snapshot-driven and wall-clock-
				 * influenced, so without this gate the per-entity override
				 * re-introduces per-launch wave-phase variance. Pin OFF
				 * (default) = legacy behaviour, byte-identical. */
				if ( pinShaderTime )
					backEnd.refdef.floatTime = originalTime;
				else if ( backEnd.currentEntity->intShaderTime )
					backEnd.refdef.floatTime = originalTime - (double)(backEnd.currentEntity->e.shaderTime.i) * 0.001;
				else
					backEnd.refdef.floatTime = originalTime - (double)backEnd.currentEntity->e.shaderTime.f;

				// set up the transformation matrix
				R_RotateForEntity( backEnd.currentEntity, &backEnd.viewParms, &backEnd.or );
				// (Per-pixel dlights transform their light list in the light-node walk;
				// the legacy per-entity dlightBits transform is retired.)
				if ( backEnd.currentEntity->e.renderfx & RF_DEPTHHACK ) {
					// hack the depth range to prevent view model from poking into walls
					depthRange = qtrue;

					if(backEnd.currentEntity->e.renderfx & RF_CROSSHAIR)
						isCrosshair = qtrue;
				}
			} else {
				backEnd.currentEntity = &tr.worldEntity;
				backEnd.refdef.floatTime = originalTime;
				backEnd.or = backEnd.viewParms.world;
			}

			// we have to reset the shaderTime as well otherwise image animations on
			// the world (like water) continue with the wrong frame
			tess.shaderTime = backEnd.refdef.floatTime - tess.shader->timeOffset;

#ifdef USE_VULKAN
			memcpy( vk_world.modelview_transform, backEnd.or.modelMatrix, 64 );
			tess.depthRange = depthRange ? DEPTH_RANGE_WEAPON : DEPTH_RANGE_NORMAL;
			vk_update_mvp( NULL );
#else
			qglLoadMatrixf( backEnd.or.modelMatrix );
#endif

			//
			// change depthrange. Also change projection matrix so first person weapon does not look like coming
			// out of the screen.
			//
#ifndef USE_VULKAN
			if (oldDepthRange != depthRange || wasCrosshair != isCrosshair)
			{
				if (depthRange)
				{
					if(backEnd.viewParms.stereoFrame != STEREO_CENTER)
					{
						if(isCrosshair)
						{
							if(oldDepthRange)
							{
								// was not a crosshair but now is, change back proj matrix
								qglMatrixMode(GL_PROJECTION);
								qglLoadMatrixf(backEnd.viewParms.projectionMatrix);
								qglMatrixMode(GL_MODELVIEW);
							}
						}
						else
						{
							viewParms_t temp = backEnd.viewParms;

							R_SetupProjection(&temp, r_znear->value, qfalse);

							qglMatrixMode(GL_PROJECTION);
							qglLoadMatrixf(temp.projectionMatrix);
							qglMatrixMode(GL_MODELVIEW);
						}
					}

					if(!oldDepthRange)
						qglDepthRange (0, 0.3);
				}
				else
				{
					if(!wasCrosshair && backEnd.viewParms.stereoFrame != STEREO_CENTER)
					{
						qglMatrixMode(GL_PROJECTION);
						qglLoadMatrixf(backEnd.viewParms.projectionMatrix);
						qglMatrixMode(GL_MODELVIEW);
					}

					qglDepthRange (0, 1);
				}
				oldDepthRange = depthRange;
				wasCrosshair = isCrosshair;
			}
#endif

			oldEntityNum = entityNum;
		}

		// add the triangles for this surface
#ifdef USE_VULKAN
		RB_InvokeDrawSurf( drawSurf, (uint32_t)i,
			temporalPrimaryCommand );
#else
		rb_surfaceTable[ *drawSurf->surface ]( drawSurf->surface );
#endif
	}

	// draw the contents of the last shader batch
	if ( oldShader != NULL ) {
		RB_EndSurface();
	}

	backEnd.refdef.floatTime = originalTime;

	// go back to the world modelview matrix
#ifdef USE_VULKAN
	memcpy( vk_world.modelview_transform, backEnd.viewParms.world.modelMatrix, 64 );
	tess.depthRange = DEPTH_RANGE_NORMAL;
	//vk_update_mvp();
#else
	qglLoadMatrixf( backEnd.viewParms.world.modelMatrix );
	if ( depthRange ) {
		qglDepthRange(0, 1);
	}
#endif
}


#ifdef USE_PMLIGHT
/*
=================
RB_BeginDrawingLitView
=================
*/
static void RB_BeginDrawingLitSurfs( void )
{
	// we will need to change the projection matrix before drawing
	// 2D images again
	backEnd.projection2D = qfalse;

	// we will only draw a sun if there was sky rendered in this view
	backEnd.skyRenderedThisView = qfalse;

	//
	// set the modelview matrix for the viewer
	//
	SetViewportAndScissor();

	glState.faceCulling = -1;		// force face culling to set next time
}


/*
==================
RB_RenderLitSurfList
==================
*/
static void RB_RenderLitSurfList( dlight_t* dl ) {
	shader_t		*shader, *oldShader;
	int				fogNum;
	int				entityNum, oldEntityNum;
#ifndef USE_VULKAN
	qboolean		oldDepthRange, wasCrosshair;
#endif
	qboolean		depthRange, isCrosshair;
	const litSurf_t	*litSurf;
	unsigned int	oldSort;
	double			originalTime; // -EC-
	qboolean		pinShaderTime;

	// save original time for entity shader offsets
	originalTime = backEnd.refdef.floatTime;
	pinShaderTime = r_pinShaderTime->value > 0.0f;

	// draw everything
	oldEntityNum = -1;
	backEnd.currentEntity = &tr.worldEntity;
	oldShader = NULL;
#ifndef USE_VULKAN
	oldDepthRange = qfalse;
	wasCrosshair = qfalse;
#endif
	oldSort = MAX_UINT;
	depthRange = qfalse;

	tess.dlightUpdateParams = qtrue;

	for ( litSurf = dl->head; litSurf; litSurf = litSurf->next ) {
		//if ( litSurf->sort == sort ) {
		if ( litSurf->sort == oldSort ) {
			// fast path, same as previous sort
			rb_surfaceTable[ *litSurf->surface ]( litSurf->surface );
			continue;
		}

		R_DecomposeLitSort( litSurf->sort, &entityNum, &shader, &fogNum );
#ifdef USE_VULKAN
		if ( vk.renderPassIndex == RENDER_PASS_SCREENMAP && entityNum != REFENTITYNUM_WORLD && backEnd.refdef.entities[ entityNum ].e.renderfx & RF_DEPTHHACK ) {
			continue;
		}
		// Forward+ XOR-split: when r_forwardPlus is on AND the tile-sum pass is
		// live this frame (vk.fpActive), PLAIN surfaces are drawn once by the
		// union (RB_RenderForwardPlusUnion) — skip them here so they aren't ALSO
		// lit additively per-light (double-brightening). VARIANT surfaces
		// (linear/fog/pbr/parallax/two-sided) are NOT in the fp union, so they
		// fall through to the per-light path here.
		//
		// The skip MUST gate on vk.fpActive, identical to the two union call sites
		// (the SS_FOG-boundary site + the trailing lit-surf site). When dispatch
		// bails for a frame (e.g. singular view matrix → vk.fpActive false), the
		// union never runs; without the vk.fpActive gate the plain surface would be
		// skipped here yet never tile-summed, losing its dlight that frame. With
		// the symmetric gate it falls through to the per-light pass and keeps its
		// dlight — so each surface is drawn exactly once in both fpActive states.
		if ( r_forwardPlus && r_forwardPlus->integer && vk.fpActive
			&& R_LitSurfIsPlain( shader, dl, fogNum ) ) {
			continue;
		}
#endif
		// anything BEFORE opaque is sky/portal, anything AFTER it should never have been added
		//assert( shader->sort == SS_OPAQUE );
		// !!! but MIRRORS can trip that assert, so just do this for now
		//if ( shader->sort < SS_OPAQUE )
		//	continue;

		//
		// change the tess parameters if needed
		// a "entityMergable" shader is a shader that can have surfaces from separate
		// entities merged into a single batch, like smoke and blood puff sprites
		if ( ( (oldSort ^ litSurf->sort) & ~QSORT_REFENTITYNUM_MASK ) || !shader->entityMergable ) {
			if ( oldShader != NULL ) {
				RB_EndSurface();
			}
			RB_BeginSurface( shader, fogNum );
			oldShader = shader;
		}

		oldSort = litSurf->sort;

		//
		// change the modelview matrix if needed
		//
		if ( entityNum != oldEntityNum ) {
			depthRange = isCrosshair = qfalse;

			if ( entityNum != REFENTITYNUM_WORLD ) {
				backEnd.currentEntity = &backEnd.refdef.entities[entityNum];

				/* Sibling of the gate in RB_RenderDrawSurfList above; same
				 * rationale (force per-entity shader time to the pinned
				 * global when r_pinShaderTime is active). */
				if ( pinShaderTime )
					backEnd.refdef.floatTime = originalTime;
				else if ( backEnd.currentEntity->intShaderTime )
					backEnd.refdef.floatTime = originalTime - (double)(backEnd.currentEntity->e.shaderTime.i) * 0.001;
				else
					backEnd.refdef.floatTime = originalTime - (double)backEnd.currentEntity->e.shaderTime.f;

				// set up the transformation matrix
				R_RotateForEntity( backEnd.currentEntity, &backEnd.viewParms, &backEnd.or );

				if ( backEnd.currentEntity->e.renderfx & RF_DEPTHHACK ) {
					// hack the depth range to prevent view model from poking into walls
					depthRange = qtrue;

					if(backEnd.currentEntity->e.renderfx & RF_CROSSHAIR)
						isCrosshair = qtrue;
				}
			} else {
				backEnd.currentEntity = &tr.worldEntity;
				backEnd.refdef.floatTime = originalTime;
				backEnd.or = backEnd.viewParms.world;
			}

			// we have to reset the shaderTime as well otherwise image animations on
			// the world (like water) continue with the wrong frame
			tess.shaderTime = backEnd.refdef.floatTime - tess.shader->timeOffset;

			// set up the dynamic lighting
			R_TransformDlights( 1, dl, &backEnd.or );
			tess.dlightUpdateParams = qtrue;

#ifdef USE_VULKAN
			tess.depthRange = depthRange ? DEPTH_RANGE_WEAPON : DEPTH_RANGE_NORMAL;
			memcpy( vk_world.modelview_transform, backEnd.or.modelMatrix, 64 );
			vk_update_mvp( NULL );
#else
			qglLoadMatrixf( backEnd.or.modelMatrix );

			//
			// change depthrange. Also change projection matrix so first person weapon does not look like coming
			// out of the screen.
			//

			if (oldDepthRange != depthRange || wasCrosshair != isCrosshair)
			{
				if (depthRange)
				{
					if(backEnd.viewParms.stereoFrame != STEREO_CENTER)
					{
						if(isCrosshair)
						{
							if(oldDepthRange)
							{
								// was not a crosshair but now is, change back proj matrix
								qglMatrixMode(GL_PROJECTION);
								qglLoadMatrixf(backEnd.viewParms.projectionMatrix);
								qglMatrixMode(GL_MODELVIEW);
							}
						}
						else
						{
							viewParms_t temp = backEnd.viewParms;

							R_SetupProjection(&temp, r_znear->value, qfalse);

							qglMatrixMode(GL_PROJECTION);
							qglLoadMatrixf(temp.projectionMatrix);
							qglMatrixMode(GL_MODELVIEW);
						}
					}

					if(!oldDepthRange)
						qglDepthRange (0, 0.3);
				}
				else
				{
					if(!wasCrosshair && backEnd.viewParms.stereoFrame != STEREO_CENTER)
					{
						qglMatrixMode(GL_PROJECTION);
						qglLoadMatrixf(backEnd.viewParms.projectionMatrix);
						qglMatrixMode(GL_MODELVIEW);
					}

					qglDepthRange (0, 1);
				}
				oldDepthRange = depthRange;
				wasCrosshair = isCrosshair;
			}
#endif

			oldEntityNum = entityNum;
		}

		// add the triangles for this surface
		rb_surfaceTable[ *litSurf->surface ]( litSurf->surface );
	}

	// draw the contents of the last shader batch
	if ( oldShader != NULL ) {
		RB_EndSurface();
	}

	backEnd.refdef.floatTime = originalTime;

	// go back to the world modelview matrix
#ifdef USE_VULKAN
	memcpy( vk_world.modelview_transform, backEnd.viewParms.world.modelMatrix, 64 );
	tess.depthRange = DEPTH_RANGE_NORMAL;
	//vk_update_mvp();
#else
	qglLoadMatrixf( backEnd.viewParms.world.modelMatrix );
	if ( depthRange ) {
		qglDepthRange (0, 1);
	}
#endif // !USE_VULKAN
}


#ifdef USE_VULKAN
/*
==================
RB_RenderForwardPlusUnion

The Forward+ tile-sum lit pass (replaces the per-light additive passes for PLAIN
surfaces when r_forwardPlus 1). Walks the deduped lit-surface union (each plain
surface ONCE — world + entity, built in R_AddLitSurf), and draws each through the
fp pipeline: the fragment reads its screen tile's light list and sums the tile's
lights in ONE pass. Per-entity transform setup mirrors RB_RenderLitSurfList (the
per-draw modelMatrix ring write — H1), but NO R_TransformDlights / per-light state
(the lights ride the world-space dlight SSBO). tess.forwardPlusPass routes the
surface end to VK_ForwardPlusPass.
==================
*/
static void RB_RenderForwardPlusUnion( void )
{
	shader_t		*shader, *oldShader;
	int				fogNum;
	int				entityNum, oldEntityNum;
	const litSurf_t	*u;
	unsigned int	oldSort;
	double			originalTime;
	qboolean		pinShaderTime;
	int				i;

	if ( tr.refdef.numFpUnionSurfs <= 0 )
		return;

	originalTime  = backEnd.refdef.floatTime;
	pinShaderTime = r_pinShaderTime->value > 0.0f;

	oldEntityNum = -1;
	backEnd.currentEntity = &tr.worldEntity;
	oldShader = NULL;
	oldSort = MAX_UINT;

	tess.forwardPlusPass = qtrue;

	for ( i = 0; i < tr.refdef.numFpUnionSurfs; i++ ) {
		u = &tr.refdef.fpUnionSurfs[ i ];
		if ( u->sort == oldSort ) {
			rb_surfaceTable[ *u->surface ]( u->surface );
			continue;
		}

		R_DecomposeLitSort( u->sort, &entityNum, &shader, &fogNum );
		if ( vk.renderPassIndex == RENDER_PASS_SCREENMAP && entityNum != REFENTITYNUM_WORLD && backEnd.refdef.entities[ entityNum ].e.renderfx & RF_DEPTHHACK )
			continue;

		if ( ( ( oldSort ^ u->sort ) & ~QSORT_REFENTITYNUM_MASK ) || !shader->entityMergable ) {
			if ( oldShader != NULL )
				RB_EndSurface();
			RB_BeginSurface( shader, fogNum );
			oldShader = shader;
		}
		oldSort = u->sort;

		if ( entityNum != oldEntityNum ) {
			if ( entityNum != REFENTITYNUM_WORLD ) {
				backEnd.currentEntity = &backEnd.refdef.entities[ entityNum ];
				if ( pinShaderTime )
					backEnd.refdef.floatTime = originalTime;
				else if ( backEnd.currentEntity->intShaderTime )
					backEnd.refdef.floatTime = originalTime - (double)( backEnd.currentEntity->e.shaderTime.i ) * 0.001;
				else
					backEnd.refdef.floatTime = originalTime - (double)backEnd.currentEntity->e.shaderTime.f;
				R_RotateForEntity( backEnd.currentEntity, &backEnd.viewParms, &backEnd.or );
			} else {
				backEnd.currentEntity = &tr.worldEntity;
				backEnd.refdef.floatTime = originalTime;
				backEnd.or = backEnd.viewParms.world;
			}
			tess.shaderTime = backEnd.refdef.floatTime - tess.shader->timeOffset;

			// per-draw transform ring write (H1): mvp stash + the modelMatrix is read
			// from backEnd.or by VK_ForwardPlusPass. NO R_TransformDlights (lights are
			// world-space in the SSBO).
			tess.depthRange = ( backEnd.currentEntity->e.renderfx & RF_DEPTHHACK ) ? DEPTH_RANGE_WEAPON : DEPTH_RANGE_NORMAL;
			memcpy( vk_world.modelview_transform, backEnd.or.modelMatrix, 64 );
			vk_update_mvp( NULL );

			oldEntityNum = entityNum;
		}

		rb_surfaceTable[ *u->surface ]( u->surface );
	}

	if ( oldShader != NULL )
		RB_EndSurface();

	tess.forwardPlusPass = qfalse;
	backEnd.refdef.floatTime = originalTime;

	memcpy( vk_world.modelview_transform, backEnd.viewParms.world.modelMatrix, 64 );
	tess.depthRange = DEPTH_RANGE_NORMAL;

	// Consume the union so a second drain point (the mid-loop SS_FOG site AND the
	// post-loop catch-all can both be reached in one frame) is a no-op — mirrors how
	// RB_LightingPass zeroes num_dlights. Additive pass → without this a double drain
	// would double-brighten. Harmless for the runtime-dlight path (single drain).
	tr.refdef.numFpUnionSurfs = 0;
}
#endif // USE_VULKAN
#endif // USE_PMLIGHT


/*
============================================================================

RENDER BACK END FUNCTIONS

============================================================================
*/

/*
================
RB_SetGL2D
================
*/
static void RB_SetGL2D( void ) {

	if ( backEnd.projection2D ) {
		return;
	}

	backEnd.projection2D = qtrue;

#ifdef USE_VULKAN
	vk_update_mvp( NULL );

	// force depth range and viewport/scissor updates
	vk.cmd->depth_range = DEPTH_RANGE_COUNT;
#else
	// set 2D virtual screen size
	qglViewport( 0, 0, glConfig.vidWidth, glConfig.vidHeight );
	qglScissor( 0, 0, glConfig.vidWidth, glConfig.vidHeight );
	qglMatrixMode( GL_PROJECTION );
	qglLoadMatrixf( GL_Ortho( 0, glConfig.vidWidth, glConfig.vidHeight, 0, 0, 1 ) );
	qglMatrixMode( GL_MODELVIEW );
	qglLoadIdentity();

	GL_State( GLS_DEPTHTEST_DISABLE |
		GLS_SRCBLEND_SRC_ALPHA |
		GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA );

	GL_Cull( CT_TWO_SIDED );
	qglDisable( GL_CLIP_PLANE0 );
#endif

	// set time for 2D shaders
	backEnd.refdef.time = ri.Milliseconds();
	backEnd.refdef.floatTime = (double)backEnd.refdef.time * 0.001; // -EC-: cast to double
}


/*
=============
RE_StretchRaw

FIXME: not exactly backend
Stretches a raw 32 bit power of 2 bitmap image over the given screen rectangle.
Used for cinematics.
=============
*/
void RE_StretchRaw( int x, int y, int w, int h, int cols, int rows, byte *data, int client, qboolean dirty ) {
	int			i, j;
	int			start, end;

	if ( !tr.registered ) {
		return;
	}

	start = 0;
	if ( r_speeds->integer ) {
		start = ri.Milliseconds();
	}

	// make sure rows and cols are powers of 2
	for ( i = 0 ; ( 1 << i ) < cols ; i++ ) {
	}
	for ( j = 0 ; ( 1 << j ) < rows ; j++ ) {
	}

	if ( ( 1 << i ) != cols || ( 1 << j ) != rows ) {
		ri.Terminate( TERM_CLIENT_DROP, "%s(): size not a power of 2: %i by %i", __func__, cols, rows );
	}

	RE_UploadCinematic( w, h, cols, rows, data, client, dirty );

	if ( r_speeds->integer ) {
		end = ri.Milliseconds();
		R_LOG( rch_cmd, SEV_INFO, "RE_UploadCinematic( %i, %i ): %i msec\n", cols, rows, end - start );
	}

	tr.cinematicShader->stages[0]->bundle[0].image[0] = tr.scratchImage[client];
	RE_StretchPic( x, y, w, h, 0.5f / cols, 0.5f / rows, 1.0f - 0.5f / cols, 1.0f - 0.5 / rows, tr.cinematicShader->index );
}


void RE_UploadCinematic( int w, int h, int cols, int rows, byte *data, int client, qboolean dirty ) {

	image_t *image;

	if ( !tr.scratchImage[ client ] ) {
		tr.scratchImage[ client ] = R_CreateImage( va( "*scratch%i", client ), NULL, data, cols, rows, IMGFLAG_CLAMPTOEDGE | IMGFLAG_RGB | IMGFLAG_NOSCALE );
		return;
	}

	image = tr.scratchImage[ client ];

#ifndef USE_VULKAN
	GL_Bind( image );
#endif

	// if the scratchImage isn't in the format we want, specify it as a new texture
	if ( cols != image->width || rows != image->height ) {
		image->width = image->uploadWidth = cols;
		image->height = image->uploadHeight = rows;
#ifdef USE_VULKAN
		vk_create_image( image, cols, rows, 1 );
		vk_upload_image_data( image, 0, 0, cols, rows, 1, data, cols * rows * 4, qfalse, 0 );
#else
		qglTexImage2D( GL_TEXTURE_2D, 0, image->internalFormat, cols, rows, 0, GL_RGBA, GL_UNSIGNED_BYTE, data );
		qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
		qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, gl_clamp_mode );
		qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, gl_clamp_mode );
#endif
	} else if ( dirty ) {
		// otherwise, just subimage upload it so that drivers can tell we are going to be changing
		// it and don't try and do a texture compression
#ifdef USE_VULKAN
		vk_upload_image_data( image, 0, 0, cols, rows, 1, data, cols * rows * 4, qtrue, 0 );
#else
		qglTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, cols, rows, GL_RGBA, GL_UNSIGNED_BYTE, data );
#endif
	}
}


/*
=============
RB_SetColor
=============
*/
static const void *RB_SetColor( const void *data ) {
	const setColorCommand_t	*cmd;

	cmd = (const setColorCommand_t *)data;

	backEnd.color2D.rgba[0] = cmd->color[0] * 255;
	backEnd.color2D.rgba[1] = cmd->color[1] * 255;
	backEnd.color2D.rgba[2] = cmd->color[2] * 255;
	backEnd.color2D.rgba[3] = cmd->color[3] * 255;

	return (const void *)(cmd + 1);
}


#ifdef USE_VULKAN
/*
=============
RB_TransitionToUI — Block 8 (Delta 2)

3D→2D handoff, called from every transition site (RB_StretchPic /
RB_RotatedPic / RB_DrawLine / RB_FinishBloom). Decoupled tonemap
(vk_tonemap, UI-agnostic) + UI pass (vk_open_ui_pass) + SMAA. Once-guarded
by backEnd.doneUIPass — the single "the orchestrator already ran this
frame" flag (replaces the old doneTonemap once-guard).
=============
*/
static void RB_TransitionToUI( void )
{
	if ( !vk.fboActive || backEnd.doneUIPass )
		return;

	if ( !backEnd.sceneRenderedThisFrame ) {
		// Pure-2D frame (menu / loading screen): no scene this frame, so
		// skip tonemap entirely — draw the 2D straight into a freshly-
		// cleared img 265 (render_pass.ui_clear). render_pass.main (img
		// 264) held only its CLEAR; vk_open_ui_pass(qtrue) ends it and
		// that clear is discarded.
		vk_open_ui_pass( qtrue );
		backEnd.doneUIPass = qtrue;
		return;
	}

	if ( !backEnd.doneSurfaces ) {
		// Gameplay frame, but this 2D draw arrived BEFORE RB_DrawSurfs
		// (cgame queues CG_TileClear letterbox fills ahead of the scene).
		// Let it land in render_pass.main (img 264) — the scene composites
		// over it correctly there — and wait. The post-scene HUD draws (or
		// RB_FinishBloom) hit this again with doneSurfaces set.
		return;
	}

	// Gameplay frame, scene finished: temporal history ingest → bloom →
	// tonemap → SMAA → open the
	// LOAD-mode UI pass on img 265 so the HUD blends on top of the
	// tonemapped (+ anti-aliased) scene.
	// Seal the command-wide motion/activation receipt before any history or
	// postprocess consumer can inspect it. vk_end_frame repeats this as an
	// idempotent fallback for gameplay frames that never transition to UI.
	{
		(void)vk_temporal_motion_seal_primary();
		vk_temporal_recursive_record();
	}
	if ( r_bloom->integer )
		vk_bloom();
	vk_tonemap();
	if ( r_smaa->integer > 0 )
		vk_smaa();
	vk_open_ui_pass( qfalse );
	backEnd.doneUIPass = qtrue;
}
#endif // USE_VULKAN


/*
=============
RB_StretchPic
=============
*/
static const void *RB_StretchPic( const void *data ) {
	const stretchPicCommand_t	*cmd;
	shader_t *shader;

	cmd = (const stretchPicCommand_t *)data;

	shader = cmd->shader;
	if ( shader != tess.shader ) {
		RB_EndSurface();
		backEnd.currentEntity = &backEnd.entity2D;
		RB_SetGL2D(); // set correct shader time before RB_BeginSurface() on 3D->2D transition
		RB_BeginSurface( shader, 0 );
	}

#ifdef USE_VBO
	VBO_UnBind();
#endif

	RB_SetGL2D();

#ifdef USE_VULKAN
	RB_TransitionToUI();
#endif

	RB_AddQuadStamp2( cmd->x, cmd->y, cmd->w, cmd->h, cmd->s1, cmd->t1, cmd->s2, cmd->t2, backEnd.color2D );
	return (const void *)(cmd + 1);
}


/*
=============
RB_MenuBackdrop

WiredUI SCENE procedural backdrop (menubg.frag) — a blended full-viewport quad
drawn INTO the currently-open 2D UI pass, as the backmost UI layer. The menu
content queued after it (RB_StretchPic / MSDF text) composites on top.

Flush discipline (mirrors the effect subsystems — vk_render_beam/sprite, which
also swap to a custom RAL pipeline mid-open-pass): flush any pending tess batch
with RB_EndSurface, enter the 2D projection and open the UI pass (RB_SetGL2D +
RB_TransitionToUI — same as RB_StretchPic), then do the raw RAL bind+draw. It
does NOT open/close a render pass: the UI pass is already open. After the draw
we invalidate the cached pipeline/descriptor state so the next tess 2D quad
fully rebinds the 2D pipeline (the exact cleanup vk_render_beam does).

The incoming x/y/w/h are physical framebuffer-pixel coordinates (the WiredUI /
Clay backend drives layout at vidWidth×vidHeight; RB_SetGL2D's ortho matches),
so they map straight onto the RAL viewport/scissor and into the UBO's resX/resY.
=============
*/
static const void *RB_MenuBackdrop( const void *data ) {
	const menuBackdropCommand_t *cmd = (const menuBackdropCommand_t *)data;

#ifdef USE_VULKAN
	// Only the FBO/RAL path draws the procedural backdrop; the RAL sibling is
	// built unconditionally in vk_update_post_process_pipelines when fboActive.
	// If either is unavailable, drop the command (menu just shows no backdrop).
	if ( vk.fboActive && vk.ral_menubg_pipeline ) {
		vk_menubg_block_t *ubo;
		ralViewport_t      vp;
		ralRect_t          sc;

		// Flush any pending 2D tess batch so it lands before the backdrop, then
		// enter 2D projection and ensure the UI pass (img 265) is open — exactly
		// the RB_StretchPic 3D→2D handoff.
		if ( tess.numIndexes )
			RB_EndSurface();
		backEnd.currentEntity = &backEnd.entity2D;
		RB_SetGL2D();
		RB_TransitionToUI();

		// The UI pass only exists on gameplay/pure-2D frames that reached the
		// transition; if it did not open (e.g. a pre-scene queue), skip rather
		// than draw into the wrong pass.
		if ( backEnd.doneUIPass ) {
			// Write the per-frame MenuBgBlock UBO (set 2). resX/resY = the pass
			// extent in pixels (the quad's width/height).
			ubo = (vk_menubg_block_t *)vk.menubg.ptr[ vk.cmd_index ];
			ubo->time       = cmd->time;
			ubo->mouseX     = cmd->mouseX;
			ubo->mouseY     = cmd->mouseY;
			ubo->transition = cmd->transition;
			ubo->resX       = cmd->w;
			ubo->resY       = cmd->h;
			ubo->pad0       = 0.0f;
			ubo->pad1       = 0.0f;
			if ( !vk_publish_menubg_shadow( (uint32_t)vk.cmd_index ) )
				return (const void *)(cmd + 1);

			vp.x = cmd->x; vp.y = cmd->y;
			vp.width = cmd->w; vp.height = cmd->h;
			vp.minDepth = 0.0f; vp.maxDepth = 1.0f;
			Ral_CmdSetViewport( vk.cmd->ral_cmd, &vp );
			sc.x = (int32_t)cmd->x; sc.y = (int32_t)cmd->y;
			sc.width = (uint32_t)cmd->w; sc.height = (uint32_t)cmd->h;
			Ral_CmdSetScissor( vk.cmd->ral_cmd, &sc );

			Ral_CmdBindPipeline( vk.cmd->ral_cmd, vk.ral_menubg_pipeline );
			// Set 0 / set 1 satisfy the shared post-process layout (the menubg
			// shader reads NEITHER sampler). Bind the blue-noise tile to BOTH —
			// NOT vk.ral_tonemapped_descriptor: img 265 is the UI pass's live
			// color attachment, so binding it here (even unused) trips the
			// validation feedback-loop advisory (VUID sampled-image ==
			// color-attachment). Blue-noise is a plain texture, never a target.
			if ( vk.blueNoise.ral_descriptor ) {
				Ral_CmdBindBindGroup( vk.cmd->ral_cmd, 0, vk.blueNoise.ral_descriptor );
				Ral_CmdBindBindGroup( vk.cmd->ral_cmd, 1, vk.blueNoise.ral_descriptor );
			} else {
				// Fallback only if the blue-noise tile isn't resident yet.
				Ral_CmdBindBindGroup( vk.cmd->ral_cmd, 0, vk.ral_tonemapped_descriptor );
			}
			Ral_CmdBindBindGroup( vk.cmd->ral_cmd, 2, vk.menubg.ral_descriptor[ vk.cmd_index ] );
			Ral_CmdDraw( vk.cmd->ral_cmd, 4, 1, 0, 0 );

			// Invalidate cached binding state so the next tess 2D quad rebinds the
			// 2D pipeline + descriptors (same cleanup vk_render_beam/sprite do).
			vk.cmd->last_ral_pipeline = NULL;
			vk.cmd->depth_range       = DEPTH_RANGE_COUNT;
			memset( vk.cmd->descriptor_set.current, 0, sizeof( VkDescriptorSet ) * WIRED_BINDLESS_SET );
			vk.cmd->descriptor_set.start = ~0U;
			vk.cmd->descriptor_set.end   = 0;
		}
	}
#endif

	return (const void *)(cmd + 1);
}


/*
=============
RB_StretchPicOverlay

A `composite overlay` widget fill. Unlike RB_StretchPic this does NOT route the
quad through the linear-HDR UI pass (RB_TransitionToUI / RB_AddQuadStamp2);
instead it records the quad into backEnd.overlayQuads so the gamma present pass
can replay it onto the swapchain image in display/sRGB space, where its alpha
reads light-independently. backEnd.color2D (set by the preceding RC_SET_COLOR)
carries the fill color verbatim — no sRGB-linear conversion, since the target
is already display-space.
=============
*/
static const void *RB_StretchPicOverlay( const void *data ) {
	const stretchPicCommand_t	*cmd;

	cmd = (const stretchPicCommand_t *)data;

	if ( backEnd.numOverlayQuads < MAX_OVERLAY_QUADS ) {
		overlayQuad_t *q = &backEnd.overlayQuads[ backEnd.numOverlayQuads++ ];
		q->x  = cmd->x;  q->y  = cmd->y;  q->w  = cmd->w;  q->h  = cmd->h;
		q->s1 = cmd->s1; q->t1 = cmd->t1; q->s2 = cmd->s2; q->t2 = cmd->t2;
		q->color  = backEnd.color2D;
		q->shader = cmd->shader;
	}
	return (const void *)(cmd + 1);
}


/*
=============
RB_RotatedPic
=============
*/
static const void *RB_RotatedPic( const void *data ) {
	const rotatedPicCommand_t *cmd = (const rotatedPicCommand_t *)data;
	shader_t *shader;
	float cx, cy, hw, hh;
	float ang, c, s;

	shader = cmd->shader;
	if ( shader != tess.shader ) {
		RB_EndSurface();
		backEnd.currentEntity = &backEnd.entity2D;
		RB_SetGL2D();
		RB_BeginSurface( shader, 0 );
	}

#ifdef USE_VBO
	VBO_UnBind();
#endif

	RB_SetGL2D();

#ifdef USE_VULKAN
	RB_TransitionToUI();
#endif

#ifdef USE_VBO
	VBO_Flush();
#endif

	RB_CHECKOVERFLOW( 4, 6 );

#ifdef USE_VBO
	tess.surfType = SF_TRIANGLES;
#endif

	int numIndexes = tess.numIndexes;
	int numVerts = tess.numVertexes;

	tess.numVertexes += 4;
	tess.numIndexes += 6;

	tess.indexes[numIndexes + 0] = numVerts + 3;
	tess.indexes[numIndexes + 1] = numVerts + 0;
	tess.indexes[numIndexes + 2] = numVerts + 2;
	tess.indexes[numIndexes + 3] = numVerts + 2;
	tess.indexes[numIndexes + 4] = numVerts + 0;
	tess.indexes[numIndexes + 5] = numVerts + 1;

	tess.vertexColors[numVerts + 0] =
	tess.vertexColors[numVerts + 1] =
	tess.vertexColors[numVerts + 2] =
	tess.vertexColors[numVerts + 3] = backEnd.color2D;

	cx = cmd->x + cmd->w * 0.5f;
	cy = cmd->y + cmd->h * 0.5f;
	hw = cmd->w * 0.5f;
	hh = cmd->h * 0.5f;

	ang = cmd->angle * ( ( float )M_PI / 180.0f );
	c = cosf( ang );
	s = sinf( ang );

	tess.xyz[numVerts + 0][0] = cx + ( -hw * c - -hh * s );
	tess.xyz[numVerts + 0][1] = cy + ( -hw * s + -hh * c );
	tess.xyz[numVerts + 0][2] = 0;

	tess.xyz[numVerts + 1][0] = cx + (  hw * c - -hh * s );
	tess.xyz[numVerts + 1][1] = cy + (  hw * s + -hh * c );
	tess.xyz[numVerts + 1][2] = 0;

	tess.xyz[numVerts + 2][0] = cx + (  hw * c -  hh * s );
	tess.xyz[numVerts + 2][1] = cy + (  hw * s +  hh * c );
	tess.xyz[numVerts + 2][2] = 0;

	tess.xyz[numVerts + 3][0] = cx + ( -hw * c -  hh * s );
	tess.xyz[numVerts + 3][1] = cy + ( -hw * s +  hh * c );
	tess.xyz[numVerts + 3][2] = 0;

	tess.texCoords[0][numVerts + 0][0] = cmd->s1;
	tess.texCoords[0][numVerts + 0][1] = cmd->t1;
	tess.texCoords[0][numVerts + 1][0] = cmd->s2;
	tess.texCoords[0][numVerts + 1][1] = cmd->t1;
	tess.texCoords[0][numVerts + 2][0] = cmd->s2;
	tess.texCoords[0][numVerts + 2][1] = cmd->t2;
	tess.texCoords[0][numVerts + 3][0] = cmd->s1;
	tess.texCoords[0][numVerts + 3][1] = cmd->t2;

	return (const void *)( cmd + 1 );
}


/*
=============
RB_SetClipRegion
=============
*/
static const void *RB_SetClipRegion( const void *data ) {
	const setClipRegionCommand_t *cmd = (const setClipRegionCommand_t *)data;

	if ( tess.numIndexes ) {
		RB_EndSurface();
		RB_BeginSurface( tess.shader, tess.fogNum );
	}

#ifdef USE_VULKAN
	if ( !cmd->hasRegion ) {
		vk_set_2d_scissor( NULL );
	} else {
		/* The clip region arrives in physical framebuffer-pixel space: the
		 * sole caller (WiredUI/Clay backend, cl_wired_clay.c) feeds Clay
		 * layout coordinates, and Clay is driven at physical resolution via
		 * Clay_SetLayoutDimensions(vidWidth, vidHeight). RB_SetGL2D's ortho
		 * is likewise 0..vidWidth / 0..vidHeight, so 2D draws and the scissor
		 * share the same pixel space — pass the rect straight through. The
		 * legacy 640x480-virtual scale here mismatched that space and pushed
		 * the scissor off-screen, hiding every clipped child. */
		int rect[4];
		rect[0] = (int)( cmd->x );
		rect[1] = (int)( cmd->y );
		rect[2] = (int)( cmd->w );
		rect[3] = (int)( cmd->h );
		vk_set_2d_scissor( rect );
	}
#else
	if ( !cmd->hasRegion ) {
		qglDisable( GL_SCISSOR_TEST );
	} else {
		int sx, sy, sw, sh;
		float xScale = (float)glConfig.vidWidth / 640.0f;
		float yScale = (float)glConfig.vidHeight / 480.0f;
		sx = (int)( cmd->x * xScale );
		sy = glConfig.vidHeight - (int)( ( cmd->y + cmd->h ) * yScale );
		sw = (int)( cmd->w * xScale );
		sh = (int)( cmd->h * yScale );
		if ( sw < 0 ) sw = 0;
		if ( sh < 0 ) sh = 0;
		qglScissor( sx, sy, sw, sh );
		qglEnable( GL_SCISSOR_TEST );
	}
#endif

	return (const void *)( cmd + 1 );
}


/*
=============
RB_DrawLine
=============
*/
static const void *RB_DrawLine( const void *data ) {
	const drawLineCommand_t	*cmd;
	shader_t *shader;
	float	dx, dy, len, px, py, halfWidth;

	cmd = (const drawLineCommand_t *)data;

	shader = cmd->shader;
	if ( shader != tess.shader ) {
		if ( tess.numIndexes ) {
			RB_EndSurface();
		}
		backEnd.currentEntity = &backEnd.entity2D;
		RB_BeginSurface( shader, 0 );
	}

#ifdef USE_VBO
	VBO_UnBind();
#endif

	if ( !backEnd.projection2D ) {
		RB_SetGL2D();
	}

#ifdef USE_VULKAN
	RB_TransitionToUI();
#endif

	halfWidth = cmd->width * 0.5f;

	// handle zero-length lines: draw nothing
	dx = cmd->x2 - cmd->x1;
	dy = cmd->y2 - cmd->y1;
	len = sqrtf( dx * dx + dy * dy );
	if ( len < 0.001f ) {
		return (const void *)(cmd + 1);
	}

	// perpendicular direction, normalized and scaled by half-width
	px = -dy / len * halfWidth;
	py =  dx / len * halfWidth;

	// clamp very thin widths to at least 0.5 pixel
	if ( halfWidth < 0.25f ) {
		halfWidth = 0.25f;
		px = -dy / len * halfWidth;
		py =  dx / len * halfWidth;
	}

#ifdef USE_VBO
	VBO_Flush();
#endif

	RB_CHECKOVERFLOW( 4, 6 );

#ifdef USE_VBO
	tess.surfType = SF_TRIANGLES;
#endif

	int numIndexes = tess.numIndexes;
	int numVerts = tess.numVertexes;

	tess.numVertexes += 4;
	tess.numIndexes += 6;

	tess.indexes[numIndexes + 0] = numVerts + 3;
	tess.indexes[numIndexes + 1] = numVerts + 0;
	tess.indexes[numIndexes + 2] = numVerts + 2;
	tess.indexes[numIndexes + 3] = numVerts + 2;
	tess.indexes[numIndexes + 4] = numVerts + 0;
	tess.indexes[numIndexes + 5] = numVerts + 1;

	tess.vertexColors[numVerts + 0] =
	tess.vertexColors[numVerts + 1] =
	tess.vertexColors[numVerts + 2] =
	tess.vertexColors[numVerts + 3] = backEnd.color2D;

	// quad corners: p1 +/- perp, p2 +/- perp
	tess.xyz[numVerts + 0][0] = cmd->x1 + px;
	tess.xyz[numVerts + 0][1] = cmd->y1 + py;
	tess.xyz[numVerts + 0][2] = 0;

	tess.xyz[numVerts + 1][0] = cmd->x1 - px;
	tess.xyz[numVerts + 1][1] = cmd->y1 - py;
	tess.xyz[numVerts + 1][2] = 0;

	tess.xyz[numVerts + 2][0] = cmd->x2 - px;
	tess.xyz[numVerts + 2][1] = cmd->y2 - py;
	tess.xyz[numVerts + 2][2] = 0;

	tess.xyz[numVerts + 3][0] = cmd->x2 + px;
	tess.xyz[numVerts + 3][1] = cmd->y2 + py;
	tess.xyz[numVerts + 3][2] = 0;

	// stretch texture along the line
	tess.texCoords[0][numVerts + 0][0] = 0;
	tess.texCoords[0][numVerts + 0][1] = 0;
	tess.texCoords[0][numVerts + 1][0] = 1;
	tess.texCoords[0][numVerts + 1][1] = 0;
	tess.texCoords[0][numVerts + 2][0] = 1;
	tess.texCoords[0][numVerts + 2][1] = 1;
	tess.texCoords[0][numVerts + 3][0] = 0;
	tess.texCoords[0][numVerts + 3][1] = 1;

	return (const void *)(cmd + 1);
}


#ifdef USE_PMLIGHT
static void RB_LightingPass( void )
{
	dlight_t	*dl;

#ifdef USE_VBO
	//VBO_Flush();
	//tess.allowVBO = qfalse; // for now
#endif

	tess.dlightPass = qtrue;

	// the sun shadow cascades are now rendered once per
	// frame as a pre-pass in vk_begin_frame (before any render pass opens), not
	// from here. The earlier placement called vk_render_shadow_map() mid-frame
	// inside the open main pass and bracketed it with vk_end/begin_main_render
	// — and the re-begin re-clears the main FBO, wiping the opaque scene
	// whenever a dynamic light was visible (the d1 black-screen regression).

	for ( int i = 0; i < backEnd.viewParms.num_dlights; i++ )
	{
		dl = &backEnd.viewParms.dlights[i];
		if ( dl->head )
		{
			tess.light = dl;
			RB_RenderLitSurfList( dl );
		}
	}

	tess.dlightPass = qfalse;

	backEnd.viewParms.num_dlights = 0;
}
#endif


static void transform_to_eye_space( const vec3_t v, vec3_t v_eye )
{
	const float *m = backEnd.viewParms.world.modelMatrix;
	v_eye[0] = m[0]*v[0] + m[4]*v[1] + m[8 ]*v[2] + m[12];
	v_eye[1] = m[1]*v[0] + m[5]*v[1] + m[9 ]*v[2] + m[13];
	v_eye[2] = m[2]*v[0] + m[6]*v[1] + m[10]*v[2] + m[14];
};


/*
================
RB_DebugPolygon
================
*/
static void RB_DebugPolygon( int color, int numPoints, float *points ) {
	vec3_t pa;
	vec3_t pb;
	vec3_t p;
	vec3_t q;
	vec3_t n;

	if ( numPoints < 3 ) {
		return;
	}

	transform_to_eye_space( &points[0], pa );
	transform_to_eye_space( &points[3], pb );
	VectorSubtract( pb, pa, p );

	for ( int i = 2; i < numPoints; i++ ) {
		transform_to_eye_space( &points[3*i], pb );
		VectorSubtract( pb, pa, q );
		CrossProduct( q, p, n );
		if ( VectorLength( n ) > 1e-5 ) {
			break;
		}
	}

	if ( DotProduct( n, pa ) >= 0 ) {
		return; // discard backfacing polygon
	}

#ifdef USE_VULKAN
	// Solid shade.
	for (int i = 0; i < numPoints; i++) {
		VectorCopy(&points[3*i], tess.xyz[i]);

		tess.svars.colors[0][i].rgba[0] = (color&1) ? 255 : 0;
		tess.svars.colors[0][i].rgba[1] = (color&2) ? 255 : 0;
		tess.svars.colors[0][i].rgba[2] = (color&4) ? 255 : 0;
		tess.svars.colors[0][i].rgba[3] = 255;
	}
	tess.numVertexes = numPoints;

	tess.numIndexes = 0;
	for (int i = 1; i < numPoints - 1; i++) {
		tess.indexes[tess.numIndexes + 0] = 0;
		tess.indexes[tess.numIndexes + 1] = i;
		tess.indexes[tess.numIndexes + 2] = i + 1;
		tess.numIndexes += 3;
	}

	vk_bind_index();
	vk_bind_pipeline( vk.surface_debug_pipeline_solid );
	vk_bind_geometry( TESS_XYZ | TESS_RGBA0 | TESS_ST0 );
	VK_PushUniformScratch();
	vk_draw_geometry( DEPTH_RANGE_NORMAL, qtrue );

	// Outline. Was identityLightByte (= 128 under
	// legacy obScale=2); linear pipeline writes full-bright 255.
	memset( tess.svars.colors[0], 255, numPoints * 2 * sizeof( color4ub_t ) );

	for ( int i = 0; i < numPoints; i++ ) {
		VectorCopy( &points[3*i], tess.xyz[2*i] );
		VectorCopy( &points[3*((i + 1) % numPoints)], tess.xyz[2*i + 1] );
	}
	tess.numVertexes = numPoints * 2;
	tess.numIndexes = 0;

	vk_bind_pipeline( vk.surface_debug_pipeline_outline );
	vk_bind_geometry( TESS_XYZ | TESS_RGBA0 );
	VK_PushUniformScratch();
	vk_draw_geometry( DEPTH_RANGE_ZERO, qfalse );
	tess.numVertexes = 0;
#else
	GL_SelectTexture( 0 );
	qglDisable( GL_TEXTURE_2D );

	GL_ClientState( 0, CLS_NONE );
	qglVertexPointer( 3, GL_FLOAT, 0, points );

	// draw solid shade
	GL_State( GLS_DEPTHMASK_TRUE | GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE );
	qglColor4f( color&1, (color>>1)&1, (color>>2)&1, 1 );
	qglDrawArrays( GL_TRIANGLE_FAN, 0, numPoints );

	// draw wireframe outline
	qglDepthRange( 0, 0 );
	qglColor4f( 1, 1, 1, 1 );
	qglDrawArrays( GL_LINE_LOOP, 0, numPoints );
	qglDepthRange( 0, 1 );

	qglEnable( GL_TEXTURE_2D );
#endif
}


/*
====================
RB_DebugGraphics

Visualization aid for movement clipping debugging
====================
*/
static void RB_DebugGraphics( void ) {

	if ( !r_debugSurface->integer ) {
		return;
	}

	GL_Bind( tr.whiteImage );
#ifdef USE_VULKAN
	vk_update_mvp( NULL );
#else
	GL_Cull( CT_FRONT_SIDED );
#endif
	ri.CM_DrawDebugSurface( RB_DebugPolygon );
}

static uint32_t RB_TemporalTopologyFold( uint32_t hash, uint32_t value ) {
	return ( hash ^ value ) * 16777619u;
}

static void RB_TemporalPoseFromEntity( const refEntity_t *entity,
		temporalEntityPose_t *pose ) {
	model_t *model;
	memset( pose, 0, sizeof( *pose ) );
	pose->hModel = entity->hModel;
	model = R_GetModelByHandle( entity->hModel );
	pose->modelToken = (uintptr_t)model;
	pose->modelDataToken = (uintptr_t)( model ? model->modelData : NULL );
	pose->modelType = model ? (uint32_t)model->type : 0u;
	if ( model ) {
#if FEAT_IQM
		if ( model->type == MOD_IQM ) {
			const iqmData_t *data = (const iqmData_t *)model->modelData;
			int normalizedFrame, normalizedOldFrame;
			float normalizedBacklerp;
			if ( data && data->temporalH5Eligible
					&& R_TemporalIqmNormalizeFrameTuple(
						(uint32_t)data->num_frames, entity->frame,
						entity->oldframe, entity->backlerp,
						&normalizedFrame, &normalizedOldFrame,
						&normalizedBacklerp ) ) {
				pose->modelTopology = data->temporalTopologyGeneration;
				pose->modelAllocationGeneration =
					data->temporalModelAllocationGeneration;
				pose->modelContentDigest = data->temporalContentDigest;
				pose->frame = normalizedFrame;
				pose->oldframe = normalizedOldFrame;
				pose->backlerp = normalizedBacklerp;
			}
		} else
#endif
		{
			pose->modelTopology = RB_TemporalTopologyFold(
				(uint32_t)model->dataSize + 1u, (uint32_t)model->numLods );
			if ( !pose->modelTopology ) pose->modelTopology = 1u;
		}
	}
	if ( model && model->type != MOD_IQM ) {
		pose->frame = entity->frame;
		pose->oldframe = entity->oldframe;
		pose->backlerp = entity->backlerp;
	}
	memcpy( pose->origin, entity->origin, sizeof( pose->origin ) );
	memcpy( pose->axis, entity->axis, sizeof( pose->axis ) );
	pose->nonNormalizedAxes = (uint32_t)( entity->nonNormalizedAxes != qfalse );
}

// Prefer one stable local-player witness for diagnostics. The MD3 torso is
// retained when the legs briefly leave the visible draw-surface set; IQM uses
// the single PLAYER_BODY role. Other entities retain deterministic tuple order.
static uint32_t RB_TemporalReceiptRoleRank( uint32_t role ) {
	switch ( role ) {
	case REF_ENTITY_MOTION_ROLE_PLAYER_BODY: return 0u;
	case REF_ENTITY_MOTION_ROLE_PLAYER_TORSO: return 1u;
	case REF_ENTITY_MOTION_ROLE_PLAYER_LEGS: return 2u;
	case REF_ENTITY_MOTION_ROLE_PLAYER_HEAD: return 3u;
	default: return 16u + role;
	}
}

static qboolean RB_TemporalReceiptPrecedes(
		const temporalEntityPoseReceipt_t *candidate,
		const temporalEntityPoseReceipt_t *current ) {
	uint32_t candidateRank, currentRank;
	if ( !current->valid ) return qtrue;
	if ( candidate->identity.ownerId != current->identity.ownerId )
		return candidate->identity.ownerId < current->identity.ownerId;
	if ( candidate->identity.generation != current->identity.generation )
		return candidate->identity.generation < current->identity.generation;
	candidateRank = RB_TemporalReceiptRoleRank( candidate->identity.role );
	currentRank = RB_TemporalReceiptRoleRank( current->identity.role );
	return candidateRank < currentRank;
}

static void RB_RecordTemporalEntityReceipts( const drawSurfsCommand_t *cmd ) {
	byte seen[(MAX_REFENTITIES + 7) / 8];
	uint32_t seenCount = 0, accepted = 0, previous = 0, rejected = 0;
	qboolean failed = qfalse;
	temporalEntityPoseReceipt_t sample;

	if ( !cmd->viewParms.temporalFrameId
			|| !( cmd->refdef.rdflags & RDF_TEMPORAL_PRIMARY ) ) return;
	if ( !R_TemporalBackendEntityReceiptsBegin() ) return;
	memset( &sample, 0, sizeof( sample ) );
	memset( seen, 0, sizeof( seen ) );
	for ( int i = 0; i < cmd->refdef.num_entities; ++i ) {
		memset( &cmd->refdef.entities[i].temporalReceipt, 0,
			sizeof( cmd->refdef.entities[i].temporalReceipt ) );
	}
	for ( int i = 0; i < cmd->numDrawSurfs; ++i ) {
		int entityNum, fogNum, dlighted;
		shader_t *shader;
		trRefEntity_t *entity;
		temporalEntityPose_t pose;
		R_DecomposeSort( cmd->drawSurfs[i].sort, &entityNum, &shader,
			&fogNum, &dlighted );
		if ( entityNum == REFENTITYNUM_WORLD || entityNum < 0
				|| entityNum >= cmd->refdef.num_entities ) continue;
		if ( seen[entityNum >> 3] & ( 1u << ( entityNum & 7 ) ) ) continue;
		seen[entityNum >> 3] |= (byte)( 1u << ( entityNum & 7 ) );
		entity = &cmd->refdef.entities[entityNum];
		if ( !entity->hasTemporal || entity->e.reType != RT_MODEL ) continue;
		seenCount++;
		RB_TemporalPoseFromEntity( &entity->e, &pose );
		if ( !R_TemporalEntityCacheRecord(
				cmd->viewParms.temporalWorldIndex,
				cmd->viewParms.temporalFrameId, (uintptr_t)entity,
				&entity->motion, &pose, &entity->temporalReceipt ) ) {
			rejected++;
			failed = qtrue;
			continue;
		}
		accepted++;
		if ( RB_TemporalReceiptPrecedes( &entity->temporalReceipt, &sample ) ) {
			sample = entity->temporalReceipt;
		}
		if ( entity->temporalReceipt.previousValid ) previous++;
	}
	if ( failed ) {
		for ( int i = 0; i < cmd->refdef.num_entities; ++i ) {
			memset( &cmd->refdef.entities[i].temporalReceipt, 0,
				sizeof( cmd->refdef.entities[i].temporalReceipt ) );
		}
		accepted = previous = 0;
		memset( &sample, 0, sizeof( sample ) );
	}
	R_TemporalBackendEntityReceipts( (uint32_t)cmd->numDrawSurfs, seenCount,
		accepted, previous, rejected, &sample );
}


/*
=============
RB_DrawSurfs
=============
*/
static const void *RB_DrawSurfs( const void *data ) {
	const drawSurfsCommand_t *cmd;
#ifdef USE_VULKAN
	qboolean temporalPrimaryCommand = qfalse;
#endif

	// finish any 2D drawing if needed
	RB_EndSurface();

	cmd = (const drawSurfsCommand_t *)data;

	backEnd.refdef = cmd->refdef;
	backEnd.viewParms = cmd->viewParms;
	if ( cmd->viewParms.temporalFrameId ) {
		R_TemporalBackendRecorded( cmd->viewParms.temporalWorldIndex,
			cmd->viewParms.temporalFrameId );
	}
	RB_RecordTemporalEntityReceipts( cmd );
#ifdef USE_VULKAN
	temporalPrimaryCommand = vk_temporal_motion_begin_primary_command();
#if FEAT_IQM
	if ( temporalPrimaryCommand ) {
		(void)vk_temporal_iqm_prescan_primary_command(
			cmd->drawSurfs, cmd->numDrawSurfs );
		(void)vk_temporal_iqm_bind_primary_command();
	}
#endif
#endif

#if defined(USE_VULKAN) && FEAT_SHADOW_MAPPING
	// Capture the budgeted dlight-shadow light while viewParms.dlights is LIVE (the
	// no-render-pass seam that renders the omni shadow runs before this command, so it
	// can't see this frame's dlights — next frame's seam consumes the capture, the
	// 1-frame lag the CSM cascade fit also accepts).
	vk_dlight_shadow_capture_light();
#endif
#if defined(USE_VULKAN)
	// Same seam-ordering workaround for the Forward+ tile-cull consumer: snapshot this
	// frame's dlights here (viewParms.dlights live) so next frame's seam dispatch sees
	// them (backEnd.viewParms.num_dlights is 0 in the seam). Without this, fpActive was
	// never true and dlights never reached the Forward+ tile-lit path.
	vk_forwardplus_capture_dlights();
	if ( r_dlightShadowProfile && r_dlightShadowProfile->integer ) {
		static int nextProfileLog;
		int now = ri.Milliseconds();
		if ( now >= nextProfileLog ) {
			R_LOG( rch_cmd, SEV_WARN,
				"dlightShadowReceiverProfile: dlights=%d litSurfs=%d fpUnionSurfs=%d fpActive=%d\n",
				backEnd.viewParms.num_dlights, backEnd.refdef.numLitSurfs,
				tr.refdef.numFpUnionSurfs, vk.fpActive ? 1 : 0 );
			nextProfileLog = now + 1000;
		}
	}
#endif

#ifdef USE_VBO
	VBO_UnBind();
	// Reset the per-view world batch-list contract: each RB_DrawSurfs
	// is one view (main + each portal/mirror + the screenmap duplicate), with its
	// own frustum-culled drawSurfs[] and thus its own batch decomposition.
	VBO_BeginView();
#endif

	// clear the z buffer, set the modelview, etc
	RB_BeginDrawingView();

	RB_RenderDrawSurfList( cmd->drawSurfs, cmd->numDrawSurfs
#ifdef USE_VULKAN
		, temporalPrimaryCommand
#endif
		);

#ifdef USE_VBO
	VBO_UnBind();
#endif

	// Primitive ribbons (world-space, translucent). Drawn after world
	// surfaces, before screen-space sun/flares/lit-surface passes.
	// RB_DrawRibbons internally skips during the screenmap pass, since
	// the ribbon pipeline is created against vk.render_pass.main only.
	RB_DrawRibbons();

	// Parametric rail-ribbon helices (world-space, alpha). The pool walk
	// regenerates each live helix's evolving geometry GPU-side from its
	// spawn params + age; expired slots are freed here. Same screenmap-pass
	// guard as ribbons. Drawn right after transient ribbons (same depth slot).
	RB_DrawRailRibbons();

	// Primitive sprites (world-space, translucent). Drawn immediately
	// after ribbons so sprites composite over ribbons at the same depth
	// slot. Same screenmap-pass guard as ribbons.
	RB_DrawSprites();

	// Primitive beams (world-space, additive). Drawn after sprites
	// so beam highlights composite over sprite particles. Walks the
	// engine-managed pool, expires/fades persistent slots, and emits
	// one vkCmdDraw with instanceCount = drawCount. Same
	// screenmap-pass guard as ribbons.
	RB_DrawBeams();

	// Primitive particles (compute-integrated GPU pool). The compute
	// pass runs from vk_begin_frame BEFORE any render pass opens —
	// vkCmdDispatch is spec-forbidden inside a render pass instance.
	// What's left here is the graphics pass that consumes the just-
	// integrated pool and emits billboard quads. Same screenmap-pass
	// guard as ribbons / sprites.
	RB_DrawParticles();

	// GPU-resident atmospheric weather (rain/snow). The compute pass ran from
	// vk_begin_frame; this draws the integrated pool as instanced streaks /
	// billboards over the world. Self-guards on r_atmosphericGPU / availability /
	// active weather type.
	RB_DrawAtmospheric();

	// GPU decals (surface-aligned instanced quads from the decal ring). Drawn
	// with the other main-pass primitives, over the world, against the world
	// depth (tested, not written). Self-guards on r_gpuDecals / availability.
	RB_DrawDecals();

	if ( r_drawSun->integer ) {
		RB_DrawSun( 0.1f, tr.sunShader );
	}

	// stencil-volume shadows retired; the stencil-darken pass is
	// no longer invoked. (RB_ShadowFinish self-gates on r_shadows==2 internally, but the
	// unified level 2 now means CSM `cast`, not stencil — gate the call out here so it
	// never runs the stencil finish with no stencil data. tr_shadows.c stays untouched.)
	/* RB_ShadowFinish(); — retired */

	// register game lens sources with the occlusion oracle (independent of r_flares —
	// these are the game's map/missile flares, gated by r_lens), then add renderer
	// light flares. Both project at this point where backEnd.viewParms is valid.
	RB_AddLensSourceFlares();

	// add light flares on lights that aren't obscured
	RB_RenderFlares();

#ifdef USE_PMLIGHT
	{
#ifdef USE_VULKAN
		// The Forward+ union can carry surfaces with NO runtime dlight — r_unbakeStaticLights
		// adds every visible plain world surface so the extracted static lights (fp SSBO only,
		// no PMLIGHT surface walk) can light them. So the lit-surf block must run when there's
		// an fp union even if numLitSurfs==0 (else the static-lit surfaces never draw).
		const qboolean haveFpUnion = ( r_forwardPlus && r_forwardPlus->integer && vk.fpActive
			&& tr.refdef.numFpUnionSurfs > 0 ) ? qtrue : qfalse;
#else
		const qboolean haveFpUnion = qfalse;
#endif
		if ( backEnd.refdef.numLitSurfs || haveFpUnion ) {
			RB_BeginDrawingLitSurfs();
			if ( backEnd.refdef.numLitSurfs )
				RB_LightingPass();
#ifdef USE_VULKAN
			// Forward+ XOR-split (see the SS_FOG-boundary site): plain surfaces drawn
			// once by the tile-sum pass; the per-light pass above skipped them.
			if ( r_forwardPlus && r_forwardPlus->integer && vk.fpActive )
				RB_RenderForwardPlusUnion();
#endif
		}
	}
#endif

	// draw main system development information (surface outlines, etc)
	RB_DebugGraphics();

#ifdef USE_VULKAN
	if ( cmd->refdef.switchRenderPass ) {
		// renderer-perframe-log-sweep — the per-frame `[FBO_DEBUG] Switching:
		// screenmap → main render pass` DEBUG line was deleted. It fired every
		// frame on maps with a screenmap pass and was dev scaffolding for a
		// since-stable code path. The render-pass transition below stays.
		vk_end_render_pass();
		vk_begin_main_render_pass();
		backEnd.screenMapDone = qtrue;
	}
#endif

	//TODO Maybe check for rdf_noworld stuff but q3mme has full 3d ui
	backEnd.doneSurfaces = qtrue; // for bloom

#ifdef USE_VULKAN
	vk_temporal_motion_end_primary_command( temporalPrimaryCommand );
#endif

	return (const void *)(cmd + 1);
}


/*
=============
RB_DrawBuffer
=============
*/
static const void *RB_DrawBuffer( const void *data,
		const temporalBatchRequest_t *temporalRequest,
		qboolean *temporalRequestDelivered ) {
	const drawBufferCommand_t	*cmd;
#ifdef USE_VULKAN
	temporalBackendSubmitQuery_t temporalDelivery;
#endif

	cmd = (const drawBufferCommand_t *)data;

#ifdef USE_VULKAN
	temporalDelivery = R_TemporalHistoryClassifyBatchDelivery(
		temporalRequest ? qtrue : qfalse,
		temporalRequest && cmd->temporalRequestToken == temporalRequest->token
			? qtrue : qfalse,
		temporalRequest ? temporalRequest->state : TEMPORAL_BATCH_REQUEST_NONE );
	if ( temporalRequestDelivered && !*temporalRequestDelivered
			&& temporalDelivery == TEMPORAL_BACKEND_SUBMIT_EXACT ) {
		R_TemporalBackendRequestDelivered( temporalRequest );
		vk_begin_frame( temporalRequest );
		*temporalRequestDelivered = qtrue;
	} else if ( temporalRequestDelivered && !*temporalRequestDelivered
			&& temporalDelivery == TEMPORAL_BACKEND_SUBMIT_INVALID ) {
		R_TemporalBackendRequestDelivered( NULL );
		vk_begin_frame( NULL );
		*temporalRequestDelivered = qtrue;
	} else {
		vk_begin_frame( NULL );
	}

	tess.depthRange = DEPTH_RANGE_NORMAL;

	// force depth range and viewport/scissor updates
	vk.cmd->depth_range = DEPTH_RANGE_COUNT;

	if ( r_clear->integer && vk.clearAttachment ) {
		const vec4_t color = {1, 0, 0.5, 1};
		backEnd.projection2D = qtrue; // to ensure we have viewport that occupies entire window
		vk_clear_color( color );
		backEnd.projection2D = qfalse;
	}
#else
	qglDrawBuffer( cmd->buffer );

	// clear screen for debugging
	if ( r_clear->integer ) {
		qglClearColor( 1, 0, 0.5, 1 );
		qglClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
	}
#endif

	return (const void *)(cmd + 1);
}


/*
===============
RB_ShowImages

Draw all the images to the screen, on top of whatever
was there.  This is used to test for texture thrashing.

Also called by RE_EndRegistration
===============
*/
#ifdef USE_VULKAN
void RB_ShowImages( void )
{
	RB_SetGL2D();

	// draw full-screen quad
	tess.numVertexes = 4;

	tess.svars.colors[0][0].u32 = ~0U; // 255-255-255-255
	tess.svars.colors[0][1].u32 = ~0U;
	tess.svars.colors[0][2].u32 = ~0U;
	tess.svars.colors[0][3].u32 = ~0U;

	tess.svars.texcoords[0][0][0] = 0.0f;
	tess.svars.texcoords[0][0][1] = 0.0f;

	tess.svars.texcoords[0][1][0] = 1.0f;
	tess.svars.texcoords[0][1][1] = 0.0f;

	tess.svars.texcoords[0][2][0] = 0.0f;
	tess.svars.texcoords[0][2][1] = 1.0f;

	tess.svars.texcoords[0][3][0] = 1.0f;
	tess.svars.texcoords[0][3][1] = 1.0f;

	tess.svars.texcoordPtr[0] = tess.svars.texcoords[0];

	tess.xyz[0][0] = 0.0f;
	tess.xyz[0][1] = 0.0f;

	tess.xyz[1][0] = (float)glConfig.vidWidth;
	tess.xyz[1][1] = 0.0f;

	tess.xyz[2][0] = 0.0f;
	tess.xyz[2][1] = (float)glConfig.vidHeight;

	tess.xyz[3][0] = (float)glConfig.vidWidth;
	tess.xyz[3][1] = (float)glConfig.vidHeight;

	vk_bind_pipeline( vk.images_debug_pipeline2 );
	vk_bind_geometry( TESS_XYZ | TESS_RGBA0 | TESS_ST0 );
	VK_PushUniformScratch();
	vk_draw_geometry( DEPTH_RANGE_NORMAL, qfalse );

	for ( int i = 0; i < tr.numImages; i++ ) {
		image_t* image = tr.images[i];

		float w = glConfig.vidWidth / 20;
		float h = glConfig.vidHeight / 15;
		float x = i % 20 * w;
		float y = i / 20 * h;

		// show in proportional size in mode 2
		if ( r_showImages->integer == 2 ) {
			w *= image->uploadWidth / 512.0f;
			h *= image->uploadHeight / 512.0f;
		}

		tess.xyz[0][0] = x;
		tess.xyz[0][1] = y;

		tess.xyz[1][0] = x + w;
		tess.xyz[1][1] = y;

		tess.xyz[2][0] = x;
		tess.xyz[2][1] = y + h;

		tess.xyz[3][0] = x + w;
		tess.xyz[3][1] = y + h;

		GL_Bind( image );
		vk_bind_pipeline( vk.images_debug_pipeline );
		vk_bind_geometry( TESS_XYZ );
		VK_PushUniformScratch();
		vk_draw_geometry( DEPTH_RANGE_NORMAL, qfalse );
	}

	tess.numIndexes = 0;
	tess.numVertexes = 0;
}
#else
void RB_ShowImages( void ) {
	int		i;
	image_t	*image;
	float	x, y, w, h;
	int		start, end;
	const vec2_t t[4] = { {0,0}, {1,0}, {0,1}, {1,1} };
	vec3_t v[4];

	RB_SetGL2D();

	qglClear( GL_COLOR_BUFFER_BIT );

	qglFinish();

	GL_ClientState( 0, CLS_TEXCOORD_ARRAY );
	qglTexCoordPointer( 2, GL_FLOAT, 0, t );

	start = ri.Milliseconds();

	for ( i = 0; i < tr.numImages; i++ ) {
		image = tr.images[ i ];
		w = glConfig.vidWidth / 20;
		h = glConfig.vidHeight / 15;
		x = i % 20 * w;
		y = i / 20 * h;

		// show in proportional size in mode 2
		if ( r_showImages->integer == 2 ) {
			w *= image->uploadWidth / 512.0f;
			h *= image->uploadHeight / 512.0f;
		}

		GL_Bind( image );

		VectorSet(v[0],x,y,0);
		VectorSet(v[1],x+w,y,0);
		VectorSet(v[2],x,y+h,0);
		VectorSet(v[3],x+w,y+h,0);

		qglVertexPointer( 3, GL_FLOAT, 0, v );
		qglDrawArrays( GL_TRIANGLE_STRIP, 0, 4 );
	}

	qglFinish();

	end = ri.Milliseconds();
	R_LOG( rch_cmd, SEV_INFO, "%i msec to draw all images\n", end - start );
}
#endif


/*
=============
RB_ColorMask
=============
*/
static const void *RB_ColorMask( const void *data )
{
	const colorMaskCommand_t *cmd = data;
#ifdef USE_VULKAN
	// TODO: implement! ZZZZZZZZZZZ
#else
	qglColorMask( cmd->rgba[0], cmd->rgba[1], cmd->rgba[2], cmd->rgba[3] );
#endif

	return (const void *)(cmd + 1);
}


/*
=============
RB_ClearDepth
=============
*/
static const void *RB_ClearDepth( const void *data )
{
	const clearDepthCommand_t *cmd = data;

	RB_EndSurface();

#ifdef USE_VULKAN
	// stencil shadows retired; never clear the stencil here.
	vk_clear_depth( qfalse );
#else
	qglClear( GL_DEPTH_BUFFER_BIT );
#endif

	return (const void *)(cmd + 1);
}


/*
=============
RB_ClearColor
=============
*/
static const void *RB_ClearColor( const void *data )
{
	const clearColorCommand_t *cmd = data;

#ifdef USE_VULKAN
	backEnd.projection2D = qtrue;
	vk_clear_color( colorBlack );
	backEnd.projection2D = qfalse;
#else
	qglViewport( 0, 0, glConfig.vidWidth, glConfig.vidHeight );
	qglScissor( 0, 0, glConfig.vidWidth, glConfig.vidHeight );
	qglClearColor( 0.0f, 0.0f, 0.0f, 1.0f );
	qglClear( GL_COLOR_BUFFER_BIT );
#endif

	return (const void *)(cmd + 1);
}


/*
=============
RB_FinishBloom
=============
*/
static const void *RB_FinishBloom( const void *data )
{
	const finishBloomCommand_t *cmd = data;

	RB_EndSurface();

#ifdef USE_VULKAN
	RB_TransitionToUI();
#endif

	// texture swapping test
	if ( r_showImages->integer ) {
		RB_ShowImages();
	}

	backEnd.drawConsole = qtrue;

	return (const void *)(cmd + 1);
}


static const void *RB_SwapBuffers( const void *data ) {

	const swapBuffersCommand_t	*cmd;

	// finish any 2D drawing if needed
	RB_EndSurface();

	// texture swapping test
	if ( r_showImages->integer && !backEnd.drawConsole ) {
		RB_ShowImages();
	}

	cmd = (const swapBuffersCommand_t *)data;

	tr.needScreenMap = 0;

#ifdef USE_VULKAN
	vk_end_frame();

	if ( backEnd.doneSurfaces && !glState.finishCalled ) {
		vk_queue_wait_idle();
	}
#else
	if ( backEnd.doneSurfaces && !glState.finishCalled ) {
		qglFinish();
	}
#endif

#ifdef USE_VULKAN
	if ( backEnd.screenshotMask && vk.cmd->waitForFence ) {
#else
	if ( backEnd.screenshotMask && tr.frameCount > 1 ) {
#endif
#ifdef USE_VULKAN
		// ss=0 reads the swapchain (window dimensions); ss=1 reads
		// vk.capture.image (FBO-render dimensions). gls.captureWidth tracks
		// the FBO render size, so a non-supersample swapchain readback must
		// use the window dimensions -- otherwise the copy extent overruns the
		// smaller swapchain image (VK_ERROR_DEVICE_LOST under an
		// r_renderScale-decoupled config). Matches vk_read_pixels' own
		// vk.capture.image branch so readback extent and file size agree.
		int ssW = vk.capture.ral_image ? gls.captureWidth : gls.windowWidth;
		int ssH = vk.capture.ral_image ? gls.captureHeight : gls.windowHeight;
#else
		int ssW = gls.captureWidth;
		int ssH = gls.captureHeight;
#endif
		if ( backEnd.screenshotMask & SCREENSHOT_TGA && backEnd.screenshotTGA[0] ) {
			RB_TakeScreenshot( 0, 0, ssW, ssH, backEnd.screenshotTGA );
			if ( !backEnd.screenShotTGAsilent ) {
				R_ScreenshotPrintSaved( backEnd.screenshotTGA );
			}
		}
		if ( backEnd.screenshotMask & SCREENSHOT_JPG && backEnd.screenshotJPG[0] ) {
			RB_TakeScreenshotJPEG( 0, 0, ssW, ssH, backEnd.screenshotJPG );
			if ( !backEnd.screenShotJPGsilent ) {
				R_ScreenshotPrintSaved( backEnd.screenshotJPG );
			}
		}
		if ( backEnd.screenshotMask & SCREENSHOT_BMP && ( backEnd.screenshotBMP[0] || ( backEnd.screenshotMask & SCREENSHOT_BMP_CLIPBOARD ) ) ) {
			RB_TakeScreenshotBMP( 0, 0, ssW, ssH, backEnd.screenshotBMP, backEnd.screenshotMask & SCREENSHOT_BMP_CLIPBOARD );
			if ( !backEnd.screenShotBMPsilent ) {
				R_ScreenshotPrintSaved( backEnd.screenshotBMP );
			}
		}
		if ( backEnd.screenshotMask & SCREENSHOT_PNG && ( backEnd.screenshotPNG[0] || ( backEnd.screenshotMask & SCREENSHOT_PNG_CLIPBOARD ) ) ) {
			RB_TakeScreenshotPNG( 0, 0, ssW, ssH, backEnd.screenshotPNG, backEnd.screenshotMask & SCREENSHOT_PNG_CLIPBOARD );
			if ( !backEnd.screenShotPNGsilent ) {
				R_ScreenshotPrintSaved( backEnd.screenshotPNG );
			}
		}
		if ( backEnd.screenshotMask & SCREENSHOT_AVI ) {
			RB_TakeVideoFrameCmd( &backEnd.vcmd );
		}

		backEnd.screenshotJPG[0] = '\0';
		backEnd.screenshotTGA[0] = '\0';
		backEnd.screenshotBMP[0] = '\0';
		backEnd.screenshotPNG[0] = '\0';
		backEnd.screenshotMask = 0;
	}

	// throttle presentation while a map is loading so the renderer does
	// not starve the loader thread. r_loadingFpsCap sets the maximum
	// present rate (0 disables the throttle, default 10).
	{
		qboolean skipSwap = qfalse;
		if ( tr.mapLoading && r_loadingFpsCap->integer > 0 ) {
			const int minInterval = 1000 / r_loadingFpsCap->integer;
			const int nowMsec = ri.Milliseconds();
			if ( nowMsec - backEnd.lastLoadingSwapMsec < minInterval ) {
				skipSwap = qtrue;
			} else {
				backEnd.lastLoadingSwapMsec = nowMsec;
			}
		} else {
			backEnd.lastLoadingSwapMsec = 0;
		}

		if ( !skipSwap ) {
#ifdef USE_VULKAN
			vk_present_frame();
#else
			ri.GLimp_EndFrame();
#endif
		}
	}

	backEnd.projection2D = qfalse;
	backEnd.doneSurfaces = qfalse;
	backEnd.drawConsole = qfalse;
#ifdef USE_VULKAN
	backEnd.doneBloom = qfalse;
	backEnd.doneUIPass = qfalse;
#endif

#if WN_RDOC_CAPTURE
	// TEMPORARY: drive a single RenderDoc frame capture, then
	// quit. We launch the engine via `renderdoccmd capture …`, so renderdoc.dll
	// is already injected (and hooked the Vulkan instance before vkCreateInstance);
	// here we just grab the in-app API and TriggerCapture() on a fixed frame so
	// the .rdc lands deterministically with no GUI / keypress. REMOVE later.
	{
		static int                   s_rdocState = 0;   // 0 init, 1 armed, 2 triggered, 3 quitting
		static int                   s_rdocFrame = 0;
		static RENDERDOC_API_1_0_0  *s_rdoc      = NULL;
		s_rdocFrame++;
		if ( s_rdocState == 0 ) {
#if defined( _WIN32 )
			HMODULE mod = GetModuleHandleA( "renderdoc.dll" );
			if ( mod != NULL ) {
				pRENDERDOC_GetAPI getApi = (pRENDERDOC_GetAPI)GetProcAddress( mod, "RENDERDOC_GetAPI" );
				if ( getApi != NULL && getApi( eRENDERDOC_API_Version_1_4_2, (void **)&s_rdoc ) == 1 && s_rdoc != NULL ) {
					R_LOG( rch_rdoc, SEV_INFO, "in-app API acquired\n" );
				} else {
					s_rdoc = NULL;
					R_LOG( rch_rdoc, SEV_INFO, "RENDERDOC_GetAPI unavailable\n" );
				}
			} else {
				R_LOG( rch_rdoc, SEV_INFO, "renderdoc.dll not in process (launch via renderdoccmd capture)\n" );
			}
#endif
			s_rdocState = 1;
		}
		if ( s_rdocState == 1 && s_rdoc != NULL && s_rdocFrame >= 120 ) {
			s_rdoc->SetCaptureFilePathTemplate( "wn_capture" );  // relative to engine cwd (renderdoccmd -d sets it to build/debug)
			s_rdoc->TriggerCapture();
			R_LOG( rch_rdoc, SEV_INFO, "TriggerCapture() at frame %d\n", s_rdocFrame );
			s_rdocState = 2;
		}
		if ( s_rdocState == 2 && s_rdocFrame >= 120 + 90 ) {
			R_LOG( rch_rdoc, SEV_INFO, "capture window elapsed; quitting\n" );
			ri.Cmd_ExecuteText( EXEC_APPEND, "quit\n" );
			s_rdocState = 3;
		}
	}
#endif

	return (const void *)(cmd + 1);
}


/*
====================
RB_ExecuteRenderCommands
====================
*/
void RB_ExecuteRenderCommands( const void *data,
		const temporalBatchRequest_t *temporalRequest ) {
	qboolean temporalRequestDelivered = qfalse;

	backEnd.pc.msec = ri.Milliseconds();

	while ( 1 ) {
		data = PADP(data, sizeof(void *));

		switch ( *(const int *)data ) {
		case RC_SET_COLOR:
			data = RB_SetColor( data );
			break;
		case RC_STRETCH_PIC:
			data = RB_StretchPic( data );
			break;
		case RC_STRETCH_PIC_OVERLAY:
			data = RB_StretchPicOverlay( data );
			break;
		case RC_MENU_BACKDROP:
			data = RB_MenuBackdrop( data );
			break;
		case RC_ROTATED_PIC:
			data = RB_RotatedPic( data );
			break;
		case RC_SET_CLIP_REGION:
			data = RB_SetClipRegion( data );
			break;
		case RC_DRAW_LINE:
			data = RB_DrawLine( data );
			break;
		case RC_DRAW_SURFS:
			data = RB_DrawSurfs( data );
			break;
		case RC_DRAW_BUFFER:
			data = RB_DrawBuffer( data, temporalRequest,
				&temporalRequestDelivered );
			break;
		case RC_SWAP_BUFFERS:
			data = RB_SwapBuffers( data );
			break;
		case RC_FINISHBLOOM:
			data = RB_FinishBloom(data);
			break;
		case RC_COLORMASK:
			data = RB_ColorMask(data);
			break;
		case RC_CLEARDEPTH:
			data = RB_ClearDepth(data);
			break;
		case RC_CLEARCOLOR:
			data = RB_ClearColor(data);
			break;
		case RC_SET_MSDF_OUTLINE:
		{
			const setMsdfOutlineCommand_t *oc = (const setMsdfOutlineCommand_t *)data;
			tr.msdfOutlineWidth = oc->outlineWidth;
			memcpy( tr.msdfOutlineColor, oc->outlineColor, sizeof( tr.msdfOutlineColor ) );
			tr.msdfGlowWidth = oc->glowWidth;
			memcpy( tr.msdfGlowColor, oc->glowColor, sizeof( tr.msdfGlowColor ) );
			data = (const void *)( oc + 1 );
			break;
		}
		case RC_SET_MSDF_SHADOW:
		{
			const setMsdfShadowCommand_t *sc = (const setMsdfShadowCommand_t *)data;
			tr.msdfShadowOffset[0] = sc->shadowOffset[0];
			tr.msdfShadowOffset[1] = sc->shadowOffset[1];
			memcpy( tr.msdfShadowColor, sc->shadowColor, sizeof( tr.msdfShadowColor ) );
			data = (const void *)( sc + 1 );
			break;
		}
		case RC_END_OF_LIST:
		default:
			// stop rendering
#ifdef USE_VULKAN
			vk_end_frame();
//			if (com_errorEntered && (begin_frame_called && !end_frame_called)) {
//				vk_end_frame();
//			}
#else
			backEnd.pc.msec = ri.Milliseconds() - backEnd.pc.msec;
#endif
			return;
		}
	}
}
