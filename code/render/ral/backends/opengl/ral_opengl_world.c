// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_opengl_world.h"
#include "ral_beam_geometry.h"
#include "ral_opengl_internal.h"
#include "render_material_stage_gpu.h"
#include "render_material_script.h"
#include "render_temporal_entity_submission.h"

#include <limits.h>
#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#define RAL_GL_APIENTRY __stdcall
#else
#define RAL_GL_APIENTRY
#endif

typedef unsigned int glEnum_t;
typedef unsigned int glName_t;
typedef int glSize_t;
typedef int glInt_t;
typedef long glSizePtr_t;
typedef long glIntPtr_t;
typedef unsigned char glBool_t;

enum {
	GL_NO_ERROR_VALUE = 0,
	GL_ONE_VALUE = 1,
	GL_FUNC_ADD_VALUE = 0x8006,
	GL_FUNC_REVERSE_SUBTRACT_VALUE = 0x800b,
	GL_TEXTURE_2D_VALUE = 0x0de1,
	GL_RGBA8_VALUE = 0x8058,
	GL_SRGB8_ALPHA8_VALUE = 0x8c43,
	GL_RGBA_VALUE = 0x1908,
	GL_UNSIGNED_BYTE_VALUE = 0x1401,
	GL_UNSIGNED_INT_VALUE = 0x1405,
	GL_FLOAT_VALUE = 0x1406,
	GL_TRIANGLES_VALUE = 0x0004,
	GL_TEXTURE_MIN_FILTER_VALUE = 0x2801,
	GL_TEXTURE_MAG_FILTER_VALUE = 0x2800,
	GL_TEXTURE_WRAP_S_VALUE = 0x2802,
	GL_TEXTURE_WRAP_T_VALUE = 0x2803,
	GL_LINEAR_VALUE = 0x2601,
	GL_REPEAT_VALUE = 0x2901,
	GL_CLAMP_TO_EDGE_VALUE = 0x812f,
	GL_BLEND_VALUE = 0x0be2,
	GL_CULL_FACE_VALUE = 0x0b44,
	GL_FRONT_VALUE = 0x0404,
	GL_BACK_VALUE = 0x0405,
	GL_CW_VALUE = 0x0900,
	GL_CCW_VALUE = 0x0901,
	GL_DEPTH_TEST_VALUE = 0x0b71,
	GL_LEQUAL_VALUE = 0x0203,
	GL_COLOR_BUFFER_BIT_VALUE = 0x00004000,
	GL_COLOR_VALUE = 0x1800,
	GL_DEPTH_BUFFER_BIT_VALUE = 0x00000100,
	GL_SRC_ALPHA_VALUE = 0x0302,
	GL_ONE_MINUS_SRC_ALPHA_VALUE = 0x0303,
	GL_UNIFORM_BUFFER_VALUE = 0x8a11,
	PRODUCT_MATERIAL_UNIFORM_BINDING = 2,
	GL_SHADER_STORAGE_BUFFER_VALUE = 0x90d2,
	GL_DYNAMIC_STORAGE_BIT_VALUE = 0x0100
};

typedef void ( RAL_GL_APIENTRY *createBuffersFn )( glSize_t, glName_t * );
typedef void ( RAL_GL_APIENTRY *namedBufferStorageFn )( glName_t, glSizePtr_t,
	const void *, unsigned int );
typedef void ( RAL_GL_APIENTRY *namedBufferSubDataFn )( glName_t, glIntPtr_t,
	glSizePtr_t, const void * );
typedef void ( RAL_GL_APIENTRY *bindBufferBaseFn )( glEnum_t, unsigned int,
	glName_t );
typedef void ( RAL_GL_APIENTRY *deleteBuffersFn )( glSize_t, const glName_t * );
typedef void ( RAL_GL_APIENTRY *createVertexArraysFn )( glSize_t, glName_t * );
typedef void ( RAL_GL_APIENTRY *deleteVertexArraysFn )( glSize_t,
	const glName_t * );
typedef void ( RAL_GL_APIENTRY *vertexArrayVertexBufferFn )( glName_t,
	unsigned int, glName_t, glIntPtr_t, glSize_t );
typedef void ( RAL_GL_APIENTRY *vertexArrayElementBufferFn )( glName_t,
	glName_t );
typedef void ( RAL_GL_APIENTRY *enableVertexArrayAttribFn )( glName_t,
	unsigned int );
typedef void ( RAL_GL_APIENTRY *vertexArrayAttribFormatFn )( glName_t,
	unsigned int, glInt_t, glEnum_t, glBool_t, unsigned int );
typedef void ( RAL_GL_APIENTRY *vertexArrayAttribBindingFn )( glName_t,
	unsigned int, unsigned int );
typedef void ( RAL_GL_APIENTRY *createTexturesFn )( glEnum_t, glSize_t,
	glName_t * );
typedef void ( RAL_GL_APIENTRY *textureStorage2DFn )( glName_t, glSize_t,
	glEnum_t, glSize_t, glSize_t );
typedef void ( RAL_GL_APIENTRY *textureSubImage2DFn )( glName_t, glInt_t,
	glInt_t, glInt_t, glSize_t, glSize_t, glEnum_t, glEnum_t, const void * );
typedef void ( RAL_GL_APIENTRY *textureParameteriFn )( glName_t, glEnum_t,
	glInt_t );
typedef void ( RAL_GL_APIENTRY *deleteTexturesFn )( glSize_t,
	const glName_t * );
typedef void ( RAL_GL_APIENTRY *bindTextureUnitFn )( unsigned int, glName_t );
typedef void ( RAL_GL_APIENTRY *bindVertexArrayFn )( glName_t );
typedef void ( RAL_GL_APIENTRY *useProgramFn )( glName_t );
typedef void ( RAL_GL_APIENTRY *drawElementsFn )( glEnum_t, glSize_t,
	glEnum_t, const void * );
typedef void ( RAL_GL_APIENTRY *drawArraysInstancedFn )( glEnum_t, glInt_t,
	glSize_t, glSize_t );
typedef void ( RAL_GL_APIENTRY *enableFn )( glEnum_t );
typedef void ( RAL_GL_APIENTRY *disableFn )( glEnum_t );
typedef void ( RAL_GL_APIENTRY *cullFaceFn )( glEnum_t );
typedef void ( RAL_GL_APIENTRY *frontFaceFn )( glEnum_t );
typedef void ( RAL_GL_APIENTRY *blendFuncFn )( glEnum_t, glEnum_t );
typedef void ( RAL_GL_APIENTRY *blendEquationiFn )( unsigned int, glEnum_t );
typedef void ( RAL_GL_APIENTRY *depthMaskFn )( glBool_t );
typedef void ( RAL_GL_APIENTRY *depthFuncFn )( glEnum_t );
typedef void ( RAL_GL_APIENTRY *viewportFn )( glInt_t, glInt_t, glSize_t,
	glSize_t );
typedef void ( RAL_GL_APIENTRY *clearColorFn )( float, float, float, float );
typedef void ( RAL_GL_APIENTRY *clearFn )( unsigned int );
typedef glEnum_t ( RAL_GL_APIENTRY *getErrorFn )( void );

typedef struct {
	createBuffersFn CreateBuffers;
	namedBufferStorageFn NamedBufferStorage;
	namedBufferSubDataFn NamedBufferSubData;
	bindBufferBaseFn BindBufferBase;
	deleteBuffersFn DeleteBuffers;
	createVertexArraysFn CreateVertexArrays;
	deleteVertexArraysFn DeleteVertexArrays;
	vertexArrayVertexBufferFn VertexArrayVertexBuffer;
	vertexArrayElementBufferFn VertexArrayElementBuffer;
	enableVertexArrayAttribFn EnableVertexArrayAttrib;
	vertexArrayAttribFormatFn VertexArrayAttribFormat;
	vertexArrayAttribBindingFn VertexArrayAttribBinding;
	createTexturesFn CreateTextures;
	textureStorage2DFn TextureStorage2D;
	textureSubImage2DFn TextureSubImage2D;
	textureParameteriFn TextureParameteri;
	deleteTexturesFn DeleteTextures;
	bindTextureUnitFn BindTextureUnit;
	bindVertexArrayFn BindVertexArray;
	useProgramFn UseProgram;
	void ( RAL_GL_APIENTRY *ClearBufferfv )( glEnum_t, glInt_t, const float * );
	void ( RAL_GL_APIENTRY *ColorMaski )( unsigned int, unsigned char, unsigned char, unsigned char, unsigned char );
	drawElementsFn DrawElements;
	drawArraysInstancedFn DrawArraysInstanced;
	enableFn Enable;
	disableFn Disable;
	cullFaceFn CullFace;
	frontFaceFn FrontFace;
	blendFuncFn BlendFunc;
	blendEquationiFn BlendEquationi;
	depthMaskFn DepthMask;
	depthFuncFn DepthFunc;
	viewportFn Viewport;
	clearColorFn ClearColor;
	clearFn Clear;
	getErrorFn GetError;
} worldDispatch_t;

typedef struct {
	float position[3];
	float texCoord[2];
	float lightmapCoord[2];
	unsigned char color[4];
	float normal[3];
} effectVertex_t;

typedef struct {
	glName_t vertexBuffer;
	glName_t indexBuffer;
	glName_t vao;
} transientGeometry_t;

typedef struct {
	float viewOriginDrawSpace[4];
	float viewForward[4];
	float viewLeft[4];
	float viewUp[4];
	float projectionLightmap[4];
	float viewportAlpha[4];
	float atmosphereEyeDensity[4];
	float atmosphereColorVisibility[4];
	/* base height, falloff, physical-sky blend weight, sky-surface flag */
	float atmosphereHeightSky[4];
	uint32_t atmosphereFroxelGrid[4];
	float localSh[4][4];
	uint32_t staticLighting[4];
	float emissiveRadiance[4];
} productDrawUniforms_t;

typedef struct {
	float startColor[4], endColor[4], scaleOffset[4];
	uint32_t flags[4]; /* [0] beam draw, [1] MSDF text (ProductMaterialFactors.beamFlags in ral_opengl_product.frag) */
} productBeamParams_t;

/* Entity and effect draws mark the TAA coverage target (beamFlags.z); see ral_opengl_product.frag. */
static const productBeamParams_t s_coverageParams = { { 0.0f }, { 0.0f }, { 0.0f }, { 0u, 0u, 1u, 0u } };

/* Folds the TAA sub-pixel jitter into the view axes: the product vertex stage computes clip.x = -dot(d, left) * px and
 * clip.y = dot(d, up) * py over w = dot(d, forward), so adding jitter * w is left -= forward * jx / px, up += forward * jy / py. Only
 * the world pass calls this; the lens projection and the UI stay unjittered. */
static void JitterWorldView( productDrawUniforms_t *uniforms, const float jitterNdc[2] ) {
	if ( ( jitterNdc[0] == 0.0f && jitterNdc[1] == 0.0f ) || uniforms->projectionLightmap[0] == 0.0f
			|| uniforms->projectionLightmap[1] == 0.0f ) return;
	for ( uint32_t axis = 0u; axis < 3u; ++axis ) {
		uniforms->viewLeft[axis] -= uniforms->viewForward[axis] * jitterNdc[0] / uniforms->projectionLightmap[0];
		uniforms->viewUp[axis] += uniforms->viewForward[axis] * jitterNdc[1] / uniforms->projectionLightmap[1];
	}
}

typedef struct {
	float materialBaseColor[4];
	/* metallic, roughness, occlusion strength, previous-to-current blend;
	 * metallic < 0 means that no cooked factor cohort is published. */
	float materialFactors[4];
	productBeamParams_t beam;
} productMaterialUniforms_t;

typedef char productDrawUniformsMustMatchStd140[
	sizeof( productDrawUniforms_t ) == 256u ? 1 : -1];
typedef char productMaterialUniformsMustMatchStd140[
	sizeof( productMaterialUniforms_t ) == 96u ? 1 : -1];

static qboolean LoadDispatch( const ralOpenGlCore_t *core,
		worldDispatch_t *gl ) {
#define LOAD_GL( member, symbol, type ) do { \
	ralOpenGlProc_t proc = RalOpenGl_CoreResolve( core, symbol ); \
	if ( !proc ) return qfalse; gl->member = (type)proc; \
} while ( 0 )
	memset( gl, 0, sizeof( *gl ) );
	LOAD_GL( CreateBuffers, "glCreateBuffers", createBuffersFn );
	LOAD_GL( NamedBufferStorage, "glNamedBufferStorage", namedBufferStorageFn );
	LOAD_GL( NamedBufferSubData, "glNamedBufferSubData", namedBufferSubDataFn );
	LOAD_GL( BindBufferBase, "glBindBufferBase", bindBufferBaseFn );
	LOAD_GL( DeleteBuffers, "glDeleteBuffers", deleteBuffersFn );
	LOAD_GL( CreateVertexArrays, "glCreateVertexArrays", createVertexArraysFn );
	LOAD_GL( DeleteVertexArrays, "glDeleteVertexArrays", deleteVertexArraysFn );
	LOAD_GL( VertexArrayVertexBuffer, "glVertexArrayVertexBuffer", vertexArrayVertexBufferFn );
	LOAD_GL( VertexArrayElementBuffer, "glVertexArrayElementBuffer", vertexArrayElementBufferFn );
	LOAD_GL( EnableVertexArrayAttrib, "glEnableVertexArrayAttrib", enableVertexArrayAttribFn );
	LOAD_GL( VertexArrayAttribFormat, "glVertexArrayAttribFormat", vertexArrayAttribFormatFn );
	LOAD_GL( VertexArrayAttribBinding, "glVertexArrayAttribBinding", vertexArrayAttribBindingFn );
	LOAD_GL( CreateTextures, "glCreateTextures", createTexturesFn );
	LOAD_GL( TextureStorage2D, "glTextureStorage2D", textureStorage2DFn );
	LOAD_GL( TextureSubImage2D, "glTextureSubImage2D", textureSubImage2DFn );
	LOAD_GL( TextureParameteri, "glTextureParameteri", textureParameteriFn );
	LOAD_GL( DeleteTextures, "glDeleteTextures", deleteTexturesFn );
	LOAD_GL( BindTextureUnit, "glBindTextureUnit", bindTextureUnitFn );
	LOAD_GL( BindVertexArray, "glBindVertexArray", bindVertexArrayFn );
	LOAD_GL( UseProgram, "glUseProgram", useProgramFn );
	{
		ralOpenGlProc_t proc = RalOpenGl_CoreResolve( core, "glClearBufferfv" );
		if ( !proc ) return qfalse;
		memcpy( &gl->ClearBufferfv, &proc, sizeof( proc ) );
		proc = RalOpenGl_CoreResolve( core, "glColorMaski" );
		if ( !proc ) return qfalse;
		memcpy( &gl->ColorMaski, &proc, sizeof( proc ) );
	}
	LOAD_GL( DrawElements, "glDrawElements", drawElementsFn );
	LOAD_GL( DrawArraysInstanced, "glDrawArraysInstanced",
		drawArraysInstancedFn );
	LOAD_GL( Enable, "glEnable", enableFn );
	LOAD_GL( Disable, "glDisable", disableFn );
	LOAD_GL( CullFace, "glCullFace", cullFaceFn );
	LOAD_GL( FrontFace, "glFrontFace", frontFaceFn );
	LOAD_GL( BlendFunc, "glBlendFunc", blendFuncFn );
	LOAD_GL( BlendEquationi, "glBlendEquationi", blendEquationiFn );
	LOAD_GL( DepthMask, "glDepthMask", depthMaskFn );
	LOAD_GL( DepthFunc, "glDepthFunc", depthFuncFn );
	LOAD_GL( Viewport, "glViewport", viewportFn );
	LOAD_GL( ClearColor, "glClearColor", clearColorFn );
	LOAD_GL( Clear, "glClear", clearFn );
	LOAD_GL( GetError, "glGetError", getErrorFn );
#undef LOAD_GL
	return qtrue;
}

static qboolean ReceiptValid( const ralOpenGlWorldReceipt_t *receipt ) {
	return receipt && receipt->schemaVersion == RAL_OPENGL_WORLD_SCHEMA_VERSION
		&& receipt->backendType == RAL_BACKEND_OPENGL
		&& receipt->coreGeneration && receipt->generation
		&& receipt->frontendFrameDigest && receipt->loweringDigest
		&& receipt->effectBeamStageDrawCount >= receipt->effectBeamDrawCount
		&& receipt->effectBeamStageDrawCount <= receipt->effectBeamDrawCount * RENDER_MATERIAL_MAX_STAGES
		&& receipt->localIrradianceEntityCount
			<= receipt->modelEntityCount + receipt->primitiveEntityCount
		&& receipt->localIrradianceDrawCount <= receipt->entityDrawCount
		&& receipt->entityMotionPrevious <= receipt->entityMotionRecorded
		&& receipt->legacyLightmapDrawCount + receipt->directionalStaticDrawCount
			<= receipt->worldDrawCount + receipt->distantWorldDrawCount
		&& receipt->distantWorldDrawCount <= RAL_SKY_DISTANT_MAX_VISIBLE_DRAWS
		&& ( receipt->distantWorldDrawCount
			? receipt->uploadedDistantWorldVertexCount > 0u
				&& receipt->uploadedDistantWorldIndexCount > 0u
				&& receipt->distantWorldCandidateCount
					>= receipt->distantWorldDrawCount
			: !receipt->uploadedDistantWorldVertexCount
				&& !receipt->uploadedDistantWorldIndexCount
				&& !receipt->distantWorldCandidateCount )
		&& ( ( receipt->legacyLightmapDrawCount
			|| receipt->directionalStaticDrawCount )
			? receipt->surfaceLightingBindingDigest != 0u
			: receipt->surfaceLightingBindingDigest == 0u )
		&& !receipt->unresolvedCount && !receipt->fallbackCount
		&& !receipt->fatalCount && receipt->ready == qtrue;
}

qboolean RalOpenGl_WorldReceiptExact( const ralOpenGlWorldReceipt_t *a,
		const ralOpenGlWorldReceipt_t *b ) {
	return ReceiptValid( a ) && ReceiptValid( b )
		&& !memcmp( a, b, sizeof( *a ) );
}

static glName_t MaterialTexture( const renderSubmissionState_t *frontend,
		const glName_t *textures, qhandle_t handle ) {
	for ( uint32_t i = 0u; i < frontend->materialCount; ++i )
		if ( frontend->materials[i].snapshot.handle == handle ) return textures[i];
	for ( uint32_t i = 0u; i < frontend->stageTextureCount; ++i )
		if ( frontend->stageTextures[i].snapshot.handle == handle )
			return textures[frontend->materialCount + i];
	return 0u;
}

static void ConfigureVertexArray( const worldDispatch_t *gl, glName_t vao,
		glName_t vertexBuffer, glName_t indexBuffer, glSize_t stride ) {
	gl->VertexArrayVertexBuffer( vao, 0u, vertexBuffer, 0, stride );
	gl->VertexArrayElementBuffer( vao, indexBuffer );
	for ( unsigned int i = 0u; i < 5u; ++i ) {
		gl->EnableVertexArrayAttrib( vao, i );
		gl->VertexArrayAttribBinding( vao, i, 0u );
	}
	gl->VertexArrayAttribFormat( vao, 0u, 3, GL_FLOAT_VALUE, 0u,
		(unsigned int)offsetof( effectVertex_t, position ) );
	gl->VertexArrayAttribFormat( vao, 1u, 2, GL_FLOAT_VALUE, 0u,
		(unsigned int)offsetof( effectVertex_t, texCoord ) );
	gl->VertexArrayAttribFormat( vao, 2u, 2, GL_FLOAT_VALUE, 0u,
		(unsigned int)offsetof( effectVertex_t, lightmapCoord ) );
	gl->VertexArrayAttribFormat( vao, 3u, 4, GL_UNSIGNED_BYTE_VALUE, 1u,
		(unsigned int)offsetof( effectVertex_t, color ) );
	gl->VertexArrayAttribFormat( vao, 4u, 3, GL_FLOAT_VALUE, 0u,
		(unsigned int)offsetof( effectVertex_t, normal ) );
}

static void UploadProductDraw( const worldDispatch_t *gl,
		glName_t uniformBuffer, const productDrawUniforms_t *uniforms ) {
	gl->NamedBufferSubData( uniformBuffer, 0,
		(glSizePtr_t)sizeof( *uniforms ), uniforms );
}

static void ApplyRaster( const worldDispatch_t *gl, glName_t uniformBuffer,
		glName_t materialUniformBuffer,
		productDrawUniforms_t *uniforms, const productBeamParams_t *beamParams,
		renderAlphaMode_t alpha, float alphaCutoff, qboolean depthWrite,
		float lightmapBoost, qboolean skySurface,
		const renderEntityCommand_t *entityCommand,
		const ralLightingSurfaceBindingReceipt_t *surfaceLighting,
		const ralLightingRuntimePlan_t *lightingPlan,
		const float emissiveRadiance[4],
		const renderMaterialFactorsSnapshot_t *materialFactors,
		float environmentBlend, const float colorScale[4] ) {
	productMaterialUniforms_t materialUniforms = { 0 };
	if (beamParams) materialUniforms.beam = *beamParams;
	/* beamFlags.w: the draw blends, so its TAA trust write goes through the blend (see wiredWriteTrust in the product shader). */
	materialUniforms.beam.flags[3] = alpha == RENDER_ALPHA_BLEND || alpha == RENDER_ALPHA_ALPHA_ADDITIVE
		|| alpha == RENDER_ALPHA_ADDITIVE ? 1u : 0u;
	gl->Disable( GL_CULL_FACE_VALUE );
	if ( alpha == RENDER_ALPHA_BLEND ) {
		gl->Enable( GL_BLEND_VALUE );
		gl->BlendFunc( GL_SRC_ALPHA_VALUE, GL_ONE_MINUS_SRC_ALPHA_VALUE );
	} else if ( alpha == RENDER_ALPHA_ALPHA_ADDITIVE ) {
		gl->Enable( GL_BLEND_VALUE );
		gl->BlendFunc( GL_SRC_ALPHA_VALUE, GL_ONE_VALUE );
	} else if ( alpha == RENDER_ALPHA_ADDITIVE ) {
		gl->Enable( GL_BLEND_VALUE );
		gl->BlendFunc( GL_ONE_VALUE, GL_ONE_VALUE );
	} else gl->Disable( GL_BLEND_VALUE );
	gl->DepthMask( depthWrite ? 1u : 0u );
	/* > 0 selects the legacy lightmap and carries its overbright (>= 1, so the shader's >= 0.5 test is unchanged). */
	uniforms->projectionLightmap[3] = lightmapBoost;
	uniforms->atmosphereHeightSky[3] = skySurface ? 1.0f : 0.0f;
	memset( uniforms->staticLighting, 0, sizeof( uniforms->staticLighting ) );
	if ( surfaceLighting ) {
		if ( surfaceLighting->directionalStaticBound && lightingPlan ) {
			uniforms->projectionLightmap[3] = 0.0f;
			uniforms->staticLighting[0] = 2u;
			uniforms->staticLighting[1] = surfaceLighting->arrayLayer;
			uniforms->staticLighting[2] = (uint32_t)lightingPlan->encoding;
			uniforms->staticLighting[3] = surfaceLighting->visibilityPlane
				!= RAL_LIGHTING_SURFACE_BINDING_NO_PLANE ? 1u : 0u;
		} else if ( surfaceLighting->legacyLightmapBound )
			uniforms->staticLighting[0] = 1u;
	}
	uniforms->viewportAlpha[2] = (float)alpha;
	uniforms->viewportAlpha[3] = alphaCutoff;
	memset( uniforms->localSh, 0, sizeof( uniforms->localSh ) );
	if ( entityCommand && entityCommand->hasLocalIrradiance ) {
		for ( uint32_t coefficient = 0u; coefficient < 4u; ++coefficient )
			for ( uint32_t channel = 0u; channel < 3u; ++channel )
				uniforms->localSh[coefficient][channel] =
					(float)entityCommand->localIrradiance
						.blendedCoefficientsQ16[coefficient][channel]
					/ (float)RAL_LIGHT_Q16_ONE;
		uniforms->localSh[0][3] = 1.0f;
	}
	if ( emissiveRadiance )
		memcpy( uniforms->emissiveRadiance, emissiveRadiance,
			sizeof( uniforms->emissiveRadiance ) );
	else memset( uniforms->emissiveRadiance, 0,
		sizeof( uniforms->emissiveRadiance ) );
	materialUniforms.materialBaseColor[0] = materialUniforms.materialBaseColor[1] =
		materialUniforms.materialBaseColor[2] =
		materialUniforms.materialBaseColor[3] = 1.0f;
	materialUniforms.materialFactors[0] = -1.0f;
	materialUniforms.materialFactors[1] = 1.0f;
	materialUniforms.materialFactors[2] = 1.0f;
	materialUniforms.materialFactors[3] = 1.0f;
	if ( materialFactors && materialFactors->ready == qtrue ) {
		memcpy( materialUniforms.materialBaseColor,
			materialFactors->factors.baseColor,
			sizeof( materialUniforms.materialBaseColor ) );
		materialUniforms.materialFactors[0] = materialFactors->factors.metallic;
		materialUniforms.materialFactors[1] = materialFactors->factors.roughness;
		materialUniforms.materialFactors[2] =
			materialFactors->factors.occlusionStrength;
		materialUniforms.materialFactors[3] = environmentBlend;
	}
	if ( colorScale )
		for ( uint32_t c = 0u; c < 4u; ++c ) materialUniforms.materialBaseColor[c] *= colorScale[c];
	gl->NamedBufferSubData( materialUniformBuffer, 0,
		(glSizePtr_t)sizeof( materialUniforms ), &materialUniforms );
	UploadProductDraw( gl, uniformBuffer, uniforms );
}

static qboolean MaterialEmission( const renderSubmissionState_t *frontend,
	qhandle_t materialHandle, float outRadiance[4] ) {
	renderMaterialSnapshot_t material;
	qboolean emissive = qfalse;
	if ( !frontend || !outRadiance ) return qfalse;
	memset( outRadiance, 0, 4u * sizeof( float ) );
	if ( materialHandle <= 0 ||
		!RenderSubmission_MaterialSnapshot( frontend, materialHandle, &material ) )
		return materialHandle <= 0 ? qtrue : qfalse;
	if ( material.lighting.schemaVersion != RENDER_MATERIAL_LIGHTING_SCHEMA_VERSION )
		return qtrue;
	for ( uint32_t channel = 0u; channel < 3u; ++channel ) {
		if ( material.lighting.emissionRadianceQ16[channel] < 0 ) return qfalse;
		outRadiance[channel] =
			(float)material.lighting.emissionRadianceQ16[channel]
			/ (float)RENDER_MATERIAL_LIGHTING_Q16_ONE;
		if ( material.lighting.emissionRadianceQ16[channel] ) emissive = qtrue;
	}
	outRadiance[3] = emissive ? 1.0f : 0.0f;
	return qtrue;
}

static qboolean ApplyWorldView( productDrawUniforms_t *uniforms,
		const renderWorldSnapshot_t *view ) {
	if ( !view || !isfinite( view->fovX ) || !isfinite( view->fovY )
			|| view->fovX <= 1.0f || view->fovX >= 179.0f
			|| view->fovY <= 1.0f || view->fovY >= 179.0f ) return qfalse;
	memcpy( uniforms->viewOriginDrawSpace, view->viewOrigin,
		3u * sizeof( float ) );
	uniforms->viewOriginDrawSpace[3] = 0.0f;
	memcpy( uniforms->viewForward, view->viewAxis[0], 3u * sizeof( float ) );
	memcpy( uniforms->viewLeft, view->viewAxis[1], 3u * sizeof( float ) );
	memcpy( uniforms->viewUp, view->viewAxis[2], 3u * sizeof( float ) );
	uniforms->projectionLightmap[0] = 1.0f
		/ tanf( view->fovX * 0.00872664625997164788f );
	uniforms->projectionLightmap[1] = 1.0f
		/ tanf( view->fovY * 0.00872664625997164788f );
	uniforms->projectionLightmap[2] = 4.0f;
	return qtrue;
}

qboolean RalOpenGl_WorldProjectLens( const renderWorldSnapshot_t *view,
		const float origin[3], uint32_t outputHeight, float projected[3], float *radiusScale ) {
	productDrawUniforms_t uniforms;
	float delta[3], forward, point[3], radius;
	if ( !origin || !projected || !radiusScale || !outputHeight ) return qfalse;
	memset( &uniforms, 0, sizeof( uniforms ) );
	if ( !ApplyWorldView( &uniforms, view ) ) return qfalse;
	for ( uint32_t i = 0; i < 3u; ++i ) delta[i] = origin[i] - view->viewOrigin[i];
	forward = DotProduct( delta, view->viewAxis[0] );
	/* World vertex shader: clip.z = forward - near; clip.w = forward.
	 * GL's default [-1,1] NDC maps to [0,1] window depth after division. */
	if ( !isfinite( forward ) || forward <= uniforms.projectionLightmap[2] * 0.5f ) return qfalse;
	point[0] = 0.5f - 0.5f * DotProduct( delta, view->viewAxis[1] )
		* uniforms.projectionLightmap[0] / forward;
	point[1] = 0.5f + 0.5f * DotProduct( delta, view->viewAxis[2] )
		* uniforms.projectionLightmap[1] / forward;
	point[2] = 1.0f - 0.5f * uniforms.projectionLightmap[2] / forward;
	radius = 0.5f * outputHeight * uniforms.projectionLightmap[1] / forward;
	if ( !isfinite( point[0] ) || !isfinite( point[1] ) || !isfinite( point[2] )
			|| !isfinite( radius ) ) return qfalse;
	memcpy( projected, point, sizeof( point ) );
	*radiusScale = radius;
	return qtrue;
}

static qboolean ApplyAtmosphere( productDrawUniforms_t *uniforms,
		const renderSubmissionState_t *frontend,
		const ralOpenGlWorldLowerInfo_t *info ) {
	renderAtmosphereSnapshot_t snapshot;
	ralSkySurfaceReceipt_t skySurface;
	const atmosphereEmitter_t *emitters;
	float cloudTransmission, lightning;
	if ( !uniforms || !frontend || !info
			|| !RenderSubmission_AtmosphereSnapshot( frontend, &snapshot,
				&emitters ) ) return qfalse;
	(void)emitters;
	memcpy( uniforms->atmosphereEyeDensity, uniforms->viewOriginDrawSpace,
		3u * sizeof( float ) );
	if ( info->atmosphere.skyEnvironment.schemaVersion != 0u ) {
		if ( !Ral_SkySurfacePlan( &info->atmosphere.skyEnvironment,
				info->physicalSkyTextureName ? qtrue : qfalse,
				&skySurface ) ) return qfalse;
		uniforms->atmosphereHeightSky[2] = skySurface.physicalBlend;
	}
	if ( !snapshot.active
			|| snapshot.state.qualityTier < ATMOSPHERE_QUALITY_ANALYTIC )
		return qtrue;
	uniforms->atmosphereEyeDensity[3] = snapshot.state.mediaDensity;
	uniforms->atmosphereColorVisibility[3] = snapshot.state.visibility;
	cloudTransmission = 1.0f - snapshot.state.cloudCover
		* info->atmosphere.skyEnvironment.cloudSunOcclusion;
	lightning = snapshot.state.lightning;
	for ( uint32_t channel = 0u; channel < 3u; ++channel )
		uniforms->atmosphereColorVisibility[channel] = fminf( 16.0f,
			info->atmosphere.skyEnvironment.ambientSh[channel]
			+ 0.08f * info->atmosphere.skyEnvironment.sunRadiance[channel]
				* cloudTransmission + lightning );
	uniforms->atmosphereHeightSky[0] = snapshot.state.bounds[2];
	uniforms->atmosphereHeightSky[1] = snapshot.state.mediaHeightFalloff;
	if ( info->atmosphere.selectedTier == RAL_ATMOSPHERE_TIER_FULL ) {
		/* FULL uses the same logarithmic range as EncodeAtmosphere; visibility
		 * zero means an unbounded authored visibility, not a 4-unit grid. */
		uniforms->atmosphereColorVisibility[3] = wired_atmosphere_grid_far( snapshot.state.visibility );
		uniforms->atmosphereFroxelGrid[0] = info->atmosphere.froxelWidth;
		uniforms->atmosphereFroxelGrid[1] = info->atmosphere.froxelHeight;
		uniforms->atmosphereFroxelGrid[2] = info->atmosphere.froxelDepth;
		uniforms->atmosphereFroxelGrid[3] = 0u;
	} else if ( info->aerialHaze > 0.0f && info->skyRadianceConsumer.ready ) {
		/* Aerial-perspective haze for the analytic tier: sun direction (snorm8
		 * xyz) and weight (unorm8, top byte) ride the froxel arena offset,
		 * which only the FULL tier consumes; keeps the 256-byte draw ABI. */
		const float *sun = info->atmosphere.skyEnvironment.sunDirection;
		uint32_t packed = 0u;
		for ( uint32_t axis = 0u; axis < 3u; ++axis ) {
			float clamped = fminf( 1.0f, fmaxf( -1.0f, sun[axis] ) );
			int32_t snorm = (int32_t)lroundf( clamped * 127.0f );
			packed |= ( (uint32_t)snorm & 0xffu ) << ( 8u * axis );
		}
		packed |= (uint32_t)lroundf( fminf( 1.0f, info->aerialHaze ) * 255.0f ) << 24u;
		uniforms->atmosphereFroxelGrid[3] = packed;
	}
	return qtrue;
}

/* Face culling from the material, as Metal and Vulkan take it (world batches carry it in renderWorldBatch_t.cullMode, model batches in
 * their material snapshot). ApplyRaster leaves culling off for the effects and beams that call it; the world, sky and model draws
 * apply their own mode after it. Drawing every face two-sided showed back faces the other renderers cull: from above a Quake 1 map
 * the inside of its sky ceiling covered the room below (TASK-266). RAL_OPENGL_WORLD_FRONT_FACE is the winding of a front face in
 * this renderer's window space (set once per world pass). */
#define RAL_OPENGL_WORLD_FRONT_FACE GL_CW_VALUE
static void ApplyCull( const worldDispatch_t *gl, renderCullMode_t mode ) {
	if ( mode != RENDER_CULL_BACK && mode != RENDER_CULL_FRONT ) { gl->Disable( GL_CULL_FACE_VALUE ); return; }
	gl->Enable( GL_CULL_FACE_VALUE );
	gl->CullFace( mode == RENDER_CULL_FRONT ? GL_FRONT_VALUE : GL_BACK_VALUE );
}

static qboolean DrawSkyBatch( const worldDispatch_t *gl,
		glName_t uniformBuffer, glName_t materialUniformBuffer,
		productDrawUniforms_t *uniforms,
		const renderSubmissionState_t *frontend, const glName_t *textures,
		const ralOpenGlWorldLowerInfo_t *info, const renderWorldBatch_t *batch,
		const renderMaterialSnapshot_t *material ) {
	renderSkyMaterialGpuParams_t params = { 0 };
	glName_t images[8];
	qhandle_t handles[RENDER_MATERIAL_MAX_STAGES];
	const glName_t owner = MaterialTexture( frontend, textures, batch->baseMaterial );
	if ( !info->skyProgramName || !owner
			|| !wired_sky_material_pack(material, batch->lightmapMaterial,
				frontend->worldSnapshot.timeMs, &params, handles) ) return qfalse;
	for ( uint32_t i = 0u; i < RENDER_MATERIAL_MAX_STAGES; ++i ) {
		images[i] = MaterialTexture(frontend, textures, handles[i]);
		if (!images[i]) return qfalse;
	}
	ApplyRaster( gl, uniformBuffer, materialUniformBuffer, uniforms, NULL,
		RENDER_ALPHA_OPAQUE, 0.0f, qfalse, 0.0f, qtrue,
		NULL, NULL, NULL, NULL, NULL, 0.0f, NULL );
	ApplyCull( gl, batch->cullMode );
	gl->NamedBufferSubData( materialUniformBuffer, 0, sizeof( params ), &params );
	for ( uint32_t i = 0u; i < 8u; ++i ) gl->BindTextureUnit( i, images[i] );
	gl->BindTextureUnit( 8u, owner );
	gl->BindTextureUnit( 9u, info->physicalSkyTextureName );
	gl->UseProgram( info->skyProgramName );
	/* The sky program writes no coverage output: keep it off the TAA coverage target. */
	if ( info->coverageActive ) gl->ColorMaski( 1u, 0u, 0u, 0u, 0u );
	gl->DrawElements( GL_TRIANGLES_VALUE, (glSize_t)batch->indexCount,
		GL_UNSIGNED_INT_VALUE, (const void *)(uintptr_t)(batch->firstIndex * sizeof(uint32_t)) );
	if ( info->coverageActive ) gl->ColorMaski( 1u, 1u, 0u, 0u, 0u );   /* scene programs write the trust (r) only */
	// Restore the product's global inputs; later world/entity/FX draws own 0..4.
	gl->UseProgram( info->programName );
	gl->BindTextureUnit( 5u, info->physicalSkyTextureName );
	gl->BindTextureUnit( 6u, info->currentSkyRadianceTextureName );
	gl->BindTextureUnit( 7u, info->previousSkyRadianceTextureName );
	gl->BindTextureUnit( 8u, info->currentSkyIrradianceTextureName );
	gl->BindTextureUnit( 9u, info->previousSkyIrradianceTextureName );
	return qtrue;
}

static qboolean DrawWorldBatch( const worldDispatch_t *gl,
		glName_t uniformBuffer, glName_t materialUniformBuffer,
		productDrawUniforms_t *uniforms,
		const renderSubmissionState_t *frontend, const glName_t *textures,
		const ralOpenGlWorldLowerInfo_t *info,
		const renderWorldBatch_t *batch,
		ralOpenGlWorldReceipt_t *receipt ) {
	renderMaterialSnapshot_t materialSnapshot;
	float emissiveRadiance[4];
	ralLightingSurfaceBindingReceipt_t surfaceLighting;
	ralLightingSurfaceBindingRequest_t bindingRequest;
	const ralLightingRuntimePlan_t *lightingPlan = info->directionalLighting
		? &info->directionalLighting->plan : NULL;
	const ralLightingSurfaceBindingReceipt_t *binding = NULL;
	if ( !gl || !uniforms || !frontend || !textures || !info || !batch
			|| !receipt || !MaterialEmission( frontend, batch->baseMaterial,
				emissiveRadiance ) ) return qfalse;
	memset( &materialSnapshot, 0, sizeof( materialSnapshot ) );
	(void)RenderSubmission_MaterialSnapshot( frontend,
		batch->baseMaterial, &materialSnapshot );
	if ( batch->sky ) return DrawSkyBatch( gl, uniformBuffer, materialUniformBuffer,
		uniforms, frontend, textures, info, batch, &materialSnapshot );
	/* The radiance cohort remains bound for surface IBL.  It cannot replace a
	 * visible authored sky because it does not encode ordered Q3 stage programs. */
	emissiveRadiance[3] = -1.0f;
	gl->BindTextureUnit( 0u, MaterialTexture( frontend, textures,
		batch->baseMaterial ) );
	if ( batch->lightmapIndex >= 0 ) {
		memset( &bindingRequest, 0, sizeof( bindingRequest ) );
		bindingRequest.schemaVersion = RAL_LIGHTING_SURFACE_BINDING_SCHEMA_VERSION;
		bindingRequest.frameGeneration = info->generation;
		bindingRequest.surfaceId = (uint64_t)batch->sourceSurfaceIndex + 1u;
		bindingRequest.lightmapIndex = batch->lightmapIndex;
		bindingRequest.legacyLightmapAvailable = batch->lightmapMaterial > 0
			? qtrue : qfalse;
		if ( !Ral_LightingSurfaceBindingBuild( &bindingRequest, lightingPlan,
				&surfaceLighting ) ) return qfalse;
		binding = &surfaceLighting;
	}
	if ( binding && binding->directionalStaticBound ) {
		gl->BindTextureUnit( 1u, 0u );
		gl->BindTextureUnit( 2u, info->directionalLighting->textureNames[
			binding->radiancePlane] );
		gl->BindTextureUnit( 3u, info->directionalLighting->textureNames[
			binding->directionPlane] );
		gl->BindTextureUnit( 4u,
			binding->visibilityPlane == RAL_LIGHTING_SURFACE_BINDING_NO_PLANE
				? 0u : info->directionalLighting->textureNames[
					binding->visibilityPlane] );
	} else {
		gl->BindTextureUnit( 1u, MaterialTexture( frontend, textures,
			batch->lightmapMaterial ) );
		gl->BindTextureUnit( 2u, 0u );
		gl->BindTextureUnit( 3u, 0u );
		gl->BindTextureUnit( 4u, 0u );
	}
	ApplyRaster( gl, uniformBuffer, materialUniformBuffer, uniforms, NULL,
		batch->alphaMode, batch->alphaCutoff, batch->depthWrite,
		( binding && binding->legacyLightmapBound ) ? info->lightmapBoost : 0.0f, batch->sky, NULL, binding,
		lightingPlan, emissiveRadiance,
		frontend->pbrEnabled && info->skyRadianceConsumer.ready
			? &materialSnapshot.factors : NULL,
		info->skyRadianceConsumer.previousToCurrentBlend, NULL );
	ApplyCull( gl, batch->cullMode );
	if ( binding ) {
		uint64_t identity = binding->surfaceId
			^ ( (uint64_t)binding->diffuseAuthority << 56u )
			^ ( (uint64_t)binding->arrayLayer << 24u );
		if ( binding->legacyLightmapBound ) receipt->legacyLightmapDrawCount++;
		else receipt->directionalStaticDrawCount++;
		receipt->surfaceLightingBindingDigest ^= identity
			+ UINT64_C( 0x9e3779b97f4a7c15 )
			+ ( receipt->surfaceLightingBindingDigest << 6u )
			+ ( receipt->surfaceLightingBindingDigest >> 2u );
		if ( !receipt->surfaceLightingBindingDigest )
			receipt->surfaceLightingBindingDigest = 1u;
	}
	gl->DrawElements( GL_TRIANGLES_VALUE, (glSize_t)batch->indexCount,
		GL_UNSIGNED_INT_VALUE, (const void *)(uintptr_t)( batch->firstIndex
			* sizeof( uint32_t ) ) );
	return qtrue;
}

static qboolean TransientCreate( const worldDispatch_t *gl,
		const effectVertex_t *vertices, uint32_t vertexCount,
		const uint32_t *indices, uint32_t indexCount,
		transientGeometry_t *out ) {
	glName_t buffers[2] = { 0u };
	if ( !vertices || !vertexCount || !indices || !indexCount || !out ) return qfalse;
	memset( out, 0, sizeof( *out ) );
	gl->CreateBuffers( 2, buffers );
	gl->NamedBufferStorage( buffers[0],
		(glSizePtr_t)( vertexCount * sizeof( *vertices ) ), vertices, 0u );
	gl->NamedBufferStorage( buffers[1],
		(glSizePtr_t)( indexCount * sizeof( *indices ) ), indices, 0u );
	gl->CreateVertexArrays( 1, &out->vao );
	out->vertexBuffer = buffers[0]; out->indexBuffer = buffers[1];
	ConfigureVertexArray( gl, out->vao, out->vertexBuffer, out->indexBuffer,
		(glSize_t)sizeof( *vertices ) );
	return qtrue;
}

static void TransientDestroy( const worldDispatch_t *gl,
		transientGeometry_t *geometry ) {
	glName_t buffers[2];
	if ( !geometry ) return;
	buffers[0] = geometry->vertexBuffer; buffers[1] = geometry->indexBuffer;
	if ( geometry->vao ) gl->DeleteVertexArrays( 1, &geometry->vao );
	if ( buffers[0] || buffers[1] ) gl->DeleteBuffers( 2, buffers );
	memset( geometry, 0, sizeof( *geometry ) );
}

static void EntityTransform( const refEntity_t *entity, const float local[3],
		float out[3] ) {
	for ( uint32_t axis = 0u; axis < 3u; ++axis )
		out[axis] = entity->origin[axis]
			+ local[0] * entity->axis[0][axis]
			+ local[1] * entity->axis[1][axis]
			+ local[2] * entity->axis[2][axis];
}

static void EntityTransformNormal( const refEntity_t *entity,
		const float local[3], float out[3] ) {
	float length;
	for ( uint32_t axis = 0u; axis < 3u; ++axis )
		out[axis] = local[0] * entity->axis[0][axis]
			+ local[1] * entity->axis[1][axis]
			+ local[2] * entity->axis[2][axis];
	length = sqrtf( out[0] * out[0] + out[1] * out[1] + out[2] * out[2] );
	if ( length > 0.000001f ) for ( uint32_t axis = 0u; axis < 3u; ++axis )
		out[axis] /= length;
	else out[2] = 1.0f;
}

/* Q3 applies refEntity_t::shaderRGBA only through a stage's entity generators (rgbGen entity / oneMinusEntity, alphaGen entity /
 * oneMinusEntity) or RF_FORCE_ENT_ALPHA; every other model surface is untinted. The product program draws the material's first
 * stage, so that stage selects the tint. */
static void EntityStageColor( const renderSubmissionState_t *frontend, qhandle_t material, const refEntity_t *entity,
		float out[4] ) {
	renderMaterialSnapshot_t snapshot;
	out[0] = out[1] = out[2] = out[3] = 1.0f;
	if ( RenderSubmission_MaterialSnapshot( frontend, material, &snapshot ) && snapshot.stageCount ) {
		const renderMaterialStageSnapshot_t *stage = &snapshot.stages[0];
		for ( uint32_t c = 0u; c < 3u; ++c ) {
			if ( stage->rgbGen == RAL_MATERIAL_COLOR_ENTITY ) out[c] = entity->shader.rgba[c] / 255.0f;
			else if ( stage->rgbGen == RAL_MATERIAL_COLOR_ONE_MINUS_ENTITY ) out[c] = 1.0f - entity->shader.rgba[c] / 255.0f;
		}
		if ( stage->alphaGen == RAL_MATERIAL_ALPHA_ENTITY ) out[3] = entity->shader.rgba[3] / 255.0f;
		else if ( stage->alphaGen == RAL_MATERIAL_ALPHA_ONE_MINUS_ENTITY ) out[3] = 1.0f - entity->shader.rgba[3] / 255.0f;
	}
	if ( entity->renderfx & RF_FORCE_ENT_ALPHA ) out[3] = entity->shader.rgba[3] / 255.0f;
}

/* A model batch's raster state from its material, as the Metal and Vulkan entity paths take it: alpha mode, cutoff, depth write and
 * nodraw. Translucent and additive model effects (the teleport-in swirl, explosion and sphere models) used to be drawn opaque here,
 * writing depth and covering the world behind them. */
typedef struct {
	renderAlphaMode_t alphaMode;
	float alphaCutoff;
	qboolean depthWrite;
	qboolean visible;
	renderCullMode_t cullMode;
} modelBatchRaster_t;

static modelBatchRaster_t ModelBatchRaster( const renderSubmissionState_t *frontend, qhandle_t material ) {
	modelBatchRaster_t raster = { RENDER_ALPHA_OPAQUE, 0.5f, qtrue, qtrue, RENDER_CULL_BACK };
	renderMaterialSnapshot_t snapshot;
	if ( material > 0 && RenderSubmission_MaterialSnapshot( frontend, material, &snapshot ) ) {
		raster.alphaMode = snapshot.alphaMode;
		raster.alphaCutoff = snapshot.alphaCutoff;
		raster.depthWrite = snapshot.depthWrite;
		raster.cullMode = snapshot.cullMode;
		if ( snapshot.noDraw ) raster.visible = qfalse;
	}
	return raster;
}

/* Entity draw phase: depth-writing geometry first, blended batches after it (Metal's composition order). */
static uint32_t RasterPhase( renderAlphaMode_t alpha ) {
	return alpha == RENDER_ALPHA_BLEND || alpha == RENDER_ALPHA_ADDITIVE || alpha == RENDER_ALPHA_ALPHA_ADDITIVE ? 1u : 0u;
}

/* An entity's motion this TAA frame: its pose went into the entity pose cache, and with a committed previous pose (in range of the
 * model's frames) its batches can carry object motion instead of invalidating the history under them. */
typedef struct {
	qboolean hasPrevious;
	qboolean rotated;
	temporalEntityPose_t previous;
} entityMotionState_t;

static qboolean EntityBatchWritesMotion( const renderSubmissionState_t *frontend, qhandle_t material,
		const entityMotionState_t *motion ) {
	return motion && motion->hasPrevious
		&& RenderTemporal_EntityBatchWritesMotion( frontend, material, motion->rotated ) ? qtrue : qfalse;
}

static qboolean DrawModelEntity( const worldDispatch_t *gl,
		glName_t uniformBuffer, glName_t materialUniformBuffer,
		productDrawUniforms_t *uniforms,
		const renderSubmissionState_t *frontend, const glName_t *textures,
		const renderEntityCommand_t *command, const entityMotionState_t *motion, uint32_t phase,
		qboolean twoSided, uint32_t *outIndices, uint32_t *outDraws ) {
	renderModelSnapshot_t model;
	effectVertex_t *vertices = NULL;
	transientGeometry_t geometry;
	uint32_t frame, oldFrame;
	float backlerp;
	qboolean ok = qfalse;
	if ( !RenderSubmission_ModelSnapshot( frontend, command->entity.hModel, &model )
			|| !model.ready || !model.vertexCount || !model.indexCount
			|| !model.positions || !model.normals || !model.texCoords || !model.indices
			|| !model.batches || !model.frameCount ) return qfalse;
	frame = command->entity.frame < 0 ? 0u : (uint32_t)command->entity.frame;
	oldFrame = command->entity.oldframe < 0 ? 0u : (uint32_t)command->entity.oldframe;
	if ( frame >= model.frameCount || oldFrame >= model.frameCount ) return qfalse;
	backlerp = command->entity.backlerp;
	if ( backlerp < 0.0f || backlerp > 1.0f ) return qfalse;
	{
		qboolean any = qfalse;
		for ( uint32_t i = 0u; i < model.batchCount && !any; ++i ) {
			const modelBatchRaster_t raster = ModelBatchRaster( frontend,
				RenderSubmission_EntityBatchMaterial( frontend, command, &model.batches[i] ) );
			any = raster.visible && RasterPhase( raster.alphaMode ) == phase ? qtrue : qfalse;
		}
		if ( !any ) return qtrue;   /* nothing of this entity in this phase: no geometry */
	}
	vertices = calloc( model.vertexCount, sizeof( *vertices ) );
	if ( !vertices ) return qfalse;
	for ( uint32_t i = 0u; i < model.vertexCount; ++i ) {
		float local[3], localNormal[3];
		for ( uint32_t axis = 0u; axis < 3u; ++axis ) {
			float current = model.positions[((size_t)frame * model.vertexCount + i)
				* 3u + axis];
			float previous = model.positions[((size_t)oldFrame * model.vertexCount + i)
				* 3u + axis];
			local[axis] = current + ( previous - current ) * backlerp;
			current = model.normals[((size_t)frame * model.vertexCount + i)
				* 3u + axis];
			previous = model.normals[((size_t)oldFrame * model.vertexCount + i)
				* 3u + axis];
			localNormal[axis] = current + ( previous - current ) * backlerp;
		}
		EntityTransform( &command->entity, local, vertices[i].position );
		EntityTransformNormal( &command->entity, localNormal, vertices[i].normal );
		memcpy( vertices[i].texCoord, model.texCoords + (size_t)i * 2u,
			sizeof( vertices[i].texCoord ) );
		/* Model vertices carry the model's own colours (white when it has none), as on Metal; the entity colour is a stage
		 * generator input, applied per batch below. */
		if ( model.colors ) memcpy( vertices[i].color, model.colors[i].rgba, sizeof( vertices[i].color ) );
		else memset( vertices[i].color, 255, sizeof( vertices[i].color ) );
	}
	if ( !TransientCreate( gl, vertices, model.vertexCount, model.indices,
		model.indexCount, &geometry ) ) goto cleanup;
	gl->BindVertexArray( geometry.vao );
	for ( uint32_t i = 0u; i < model.batchCount; ++i ) {
		qhandle_t material = RenderSubmission_EntityBatchMaterial(
			frontend, command, &model.batches[i] );
		const modelBatchRaster_t raster = ModelBatchRaster( frontend, material );
		float entityColor[4];
		if ( !raster.visible || RasterPhase( raster.alphaMode ) != phase ) continue;
		EntityStageColor( frontend, material, &command->entity, entityColor );
		gl->BindTextureUnit( 0u, MaterialTexture( frontend, textures, material ) );
		/* A batch that carries its object motion keeps full trust (the motion draw supplies its offset); every other entity batch
		 * marks the history under it as untrusted. */
		ApplyRaster( gl, uniformBuffer, materialUniformBuffer, uniforms,
			EntityBatchWritesMotion( frontend, material, motion ) ? NULL : &s_coverageParams,
			raster.alphaMode,
			raster.alphaCutoff, raster.depthWrite, 0.0f, qfalse, command, NULL, NULL, NULL,
			NULL, 1.0f, entityColor );
		ApplyCull( gl, twoSided ? RENDER_CULL_NONE : raster.cullMode );
		gl->DrawElements( GL_TRIANGLES_VALUE, (glSize_t)model.batches[i].indexCount,
			GL_UNSIGNED_INT_VALUE, (const void *)(uintptr_t)( model.batches[i].firstIndex
				* sizeof( uint32_t ) ) );
		*outIndices += model.batches[i].indexCount; ( *outDraws )++;
	}
	ok = qtrue;
	TransientDestroy( gl, &geometry );
cleanup:
	free( vertices );
	return ok;
}

/* Records the pose of every model entity with a motion identity (the first-person weapon included) into this TAA frame's entity
 * pose cache transaction, keeping the committed previous pose where there is one. The same rule set as Metal and WebGPU
 * (render_temporal_entity_submission.h). */
static void PrepareEntityMotion( const ralOpenGlWorldLowerInfo_t *info, const renderSubmissionState_t *frontend,
		const renderEntityCommand_t *entities, uint32_t entityCount, entityMotionState_t *states,
		ralOpenGlWorldReceipt_t *receipt ) {
	if ( !info->coverageActive || !info->entityMotionActive || !states ) return;
	for ( uint32_t i = 0u; i < entityCount; ++i ) {
		const refEntity_t *entity = &entities[i].entity;
		renderModelSnapshot_t model;
		temporalEntityPose_t pose;
		temporalEntityPoseReceipt_t poseReceipt;
		if ( !entities[i].hasTemporal || entity->reType != RT_MODEL || entity->hModel <= 0
				|| !RalOpenGl_EntityInPrimaryView( entity )
				|| !RenderSubmission_ModelSnapshot( frontend, entity->hModel, &model ) || !model.ready
				|| !model.positions || !model.frameCount ) continue;
		RenderTemporal_EntityPose( entity, &model, &pose );
		if ( !R_TemporalEntityCacheRecord( info->entityMotionWorld, info->entityMotionFrame, (uintptr_t)&entities[i],
				&entities[i].motion, &pose, &poseReceipt ) ) continue;
		receipt->entityMotionRecorded++;
		if ( poseReceipt.previousValid && poseReceipt.previous.frame >= 0 && poseReceipt.previous.oldframe >= 0
				&& (uint32_t)poseReceipt.previous.frame < model.frameCount
				&& (uint32_t)poseReceipt.previous.oldframe < model.frameCount ) {
			states[i].hasPrevious = qtrue;
			states[i].previous = poseReceipt.previous;
			states[i].rotated = RenderTemporal_PoseRotated( &poseReceipt.previous, &pose );
			receipt->entityMotionPrevious++;
		}
	}
}

typedef struct {
	float current[3];
	float previous[3];
} motionVertex_t;

static void ModelLocalPosition( const renderModelSnapshot_t *model, uint32_t frame, uint32_t oldFrame, float backlerp,
		uint32_t vertex, float out[3] ) {
	for ( uint32_t axis = 0u; axis < 3u; ++axis ) {
		const float current = model->positions[( (size_t)frame * model->vertexCount + vertex ) * 3u + axis];
		const float previous = model->positions[( (size_t)oldFrame * model->vertexCount + vertex ) * 3u + axis];
		out[axis] = current + ( previous - current ) * backlerp;
	}
}

/* The object motion draw of one entity: its motion-carrying batches over the finished opaque scene, depth-tested without depth
 * write, writing previous minus current world position into the effect mask's gba (the program from RalOpenGl_AaObjectMotion
 * Program). The current position is the one DrawModelEntity drew; the previous one is the model at its previous animation frame
 * under its previous transform. */
static qboolean DrawEntityMotion( const worldDispatch_t *gl, const renderSubmissionState_t *frontend,
		const renderEntityCommand_t *command, const entityMotionState_t *motion, uint32_t *outDraws ) {
	renderModelSnapshot_t model;
	motionVertex_t *vertices;
	glName_t buffers[2] = { 0u }, vao = 0u;
	const temporalEntityPose_t *previousPose = &motion->previous;
	uint32_t frame, oldFrame, batches = 0u;
	if ( !RenderSubmission_ModelSnapshot( frontend, command->entity.hModel, &model ) || !model.ready
			|| !model.vertexCount || !model.indexCount || !model.positions || !model.indices || !model.batches
			|| !model.frameCount ) return qfalse;
	for ( uint32_t i = 0u; i < model.batchCount; ++i ) {
		const qhandle_t material = RenderSubmission_EntityBatchMaterial( frontend, command, &model.batches[i] );
		const modelBatchRaster_t raster = ModelBatchRaster( frontend, material );
		if ( raster.visible && RasterPhase( raster.alphaMode ) == 0u && EntityBatchWritesMotion( frontend, material, motion ) )
			batches++;
	}
	if ( !batches ) return qtrue;
	frame = command->entity.frame < 0 ? 0u : (uint32_t)command->entity.frame;
	oldFrame = command->entity.oldframe < 0 ? 0u : (uint32_t)command->entity.oldframe;
	if ( frame >= model.frameCount || oldFrame >= model.frameCount
			|| command->entity.backlerp < 0.0f || command->entity.backlerp > 1.0f ) return qfalse;
	vertices = calloc( model.vertexCount, sizeof( *vertices ) );
	if ( !vertices ) return qfalse;
	for ( uint32_t i = 0u; i < model.vertexCount; ++i ) {
		float local[3];
		ModelLocalPosition( &model, frame, oldFrame, command->entity.backlerp, i, local );
		EntityTransform( &command->entity, local, vertices[i].current );
		ModelLocalPosition( &model, (uint32_t)previousPose->frame, (uint32_t)previousPose->oldframe,
			previousPose->backlerp, i, local );
		for ( uint32_t axis = 0u; axis < 3u; ++axis )
			vertices[i].previous[axis] = previousPose->origin[axis]
				+ previousPose->axis[0u * 3u + axis] * local[0]
				+ previousPose->axis[1u * 3u + axis] * local[1]
				+ previousPose->axis[2u * 3u + axis] * local[2];
	}
	gl->CreateBuffers( 2, buffers );
	gl->NamedBufferStorage( buffers[0], (glSizePtr_t)( model.vertexCount * sizeof( *vertices ) ), vertices, 0u );
	gl->NamedBufferStorage( buffers[1], (glSizePtr_t)( model.indexCount * sizeof( *model.indices ) ), model.indices, 0u );
	free( vertices );
	gl->CreateVertexArrays( 1, &vao );
	gl->VertexArrayVertexBuffer( vao, 0u, buffers[0], 0, (glSize_t)sizeof( motionVertex_t ) );
	gl->VertexArrayElementBuffer( vao, buffers[1] );
	for ( unsigned int attribute = 0u; attribute < 2u; ++attribute ) {
		gl->EnableVertexArrayAttrib( vao, attribute );
		gl->VertexArrayAttribBinding( vao, attribute, 0u );
		gl->VertexArrayAttribFormat( vao, attribute, 3, GL_FLOAT_VALUE, 0u,
			attribute ? (unsigned int)offsetof( motionVertex_t, previous ) : (unsigned int)offsetof( motionVertex_t, current ) );
	}
	gl->BindVertexArray( vao );
	for ( uint32_t i = 0u; i < model.batchCount; ++i ) {
		const qhandle_t material = RenderSubmission_EntityBatchMaterial( frontend, command, &model.batches[i] );
		const modelBatchRaster_t raster = ModelBatchRaster( frontend, material );
		if ( !raster.visible || RasterPhase( raster.alphaMode ) != 0u || !EntityBatchWritesMotion( frontend, material, motion ) )
			continue;
		ApplyCull( gl, raster.cullMode );   /* the faces the colour pass drew, so the offset lands on the same pixels */
		gl->DrawElements( GL_TRIANGLES_VALUE, (glSize_t)model.batches[i].indexCount, GL_UNSIGNED_INT_VALUE,
			(const void *)(uintptr_t)( model.batches[i].firstIndex * sizeof( uint32_t ) ) );
		( *outDraws )++;
	}
	gl->DeleteVertexArrays( 1, &vao );
	gl->DeleteBuffers( 2, buffers );
	return qtrue;
}

static qboolean DrawPrimitiveEntity( const worldDispatch_t *gl,
		glName_t uniformBuffer, glName_t materialUniformBuffer,
		productDrawUniforms_t *uniforms,
		const renderSubmissionState_t *frontend, const glName_t *textures,
		const renderEntityCommand_t *command, uint32_t *outIndices,
		uint32_t *outDraws ) {
	const refEntity_t *entity;
	effectVertex_t vertices[8];
	uint32_t indices[36];
	transientGeometry_t geometry;
	uint32_t vertexCount, indexCount;
	if ( !command ) return qfalse;
	entity = &command->entity;
	memset( vertices, 0, sizeof( vertices ) );
	memset( indices, 0, sizeof( indices ) );
	if ( entity->reType == RT_SPRITE ) {
		float radius = entity->radius > 0.0f ? entity->radius : 0.01f;
		vertexCount = 4u; indexCount = 6u;
		for ( uint32_t i = 0u; i < 4u; ++i ) {
			memcpy( vertices[i].position, entity->origin, sizeof( entity->origin ) );
			vertices[i].position[0] += ( i & 1u ) ? radius : -radius;
			vertices[i].position[1] += ( i & 2u ) ? radius : -radius;
			vertices[i].texCoord[0] = ( i & 1u ) ? 1.0f : 0.0f;
			vertices[i].texCoord[1] = ( i & 2u ) ? 1.0f : 0.0f;
			memcpy( vertices[i].color, entity->shader.rgba, sizeof( vertices[i].color ) );
			vertices[i].normal[2] = 1.0f;
		}
		{ const uint32_t quad[6] = { 0u, 1u, 2u, 2u, 1u, 3u };
			memcpy( indices, quad, sizeof( quad ) ); }
	} else if ( entity->reType == RT_BEAM
			|| ( entity->reType == RT_MODEL && entity->hModel == 0 ) ) {
		static const uint32_t box[36] = { 0,1,2,2,1,3, 4,6,5,5,6,7,
			0,4,1,1,4,5, 2,3,6,6,3,7, 0,2,4,4,2,6, 1,5,3,3,5,7 };
		qboolean beam = entity->reType == RT_BEAM ? qtrue : qfalse;
		float radius = beam
			? ( entity->frame > 0 ? (float)entity->frame * 0.5f : 0.5f )
			: 4.0f;
		vertexCount = 8u; indexCount = 36u;
		for ( uint32_t i = 0u; i < 8u; ++i ) {
			const float *point = beam && ( i & 4u )
				? entity->oldorigin : entity->origin;
			memcpy( vertices[i].position, point, sizeof( entity->origin ) );
			vertices[i].position[0] += ( i & 1u ) ? radius : -radius;
			vertices[i].position[1] += ( i & 2u ) ? radius : -radius;
			if ( !beam ) vertices[i].position[2] += ( i & 4u ) ? radius : -radius;
			memcpy( vertices[i].color, entity->shader.rgba, sizeof( vertices[i].color ) );
			vertices[i].normal[2] = 1.0f;
		}
		memcpy( indices, box, sizeof( box ) );
	} else return qfalse;
	if ( !TransientCreate( gl, vertices, vertexCount, indices, indexCount,
		&geometry ) ) return qfalse;
	gl->BindVertexArray( geometry.vao );
	gl->BindTextureUnit( 0u, MaterialTexture( frontend, textures,
		entity->customShader ) );
	ApplyRaster( gl, uniformBuffer, materialUniformBuffer, uniforms, &s_coverageParams,
		RENDER_ALPHA_BLEND,
		0.5f, qfalse, 0.0f, qfalse, command, NULL, NULL, NULL,
		NULL, 1.0f, NULL );
	gl->DrawElements( GL_TRIANGLES_VALUE, (glSize_t)indexCount,
		GL_UNSIGNED_INT_VALUE, NULL );
	TransientDestroy( gl, &geometry );
	*outIndices += indexCount; ( *outDraws )++;
	return qtrue;
}

static qboolean DrawUiPrimitive( const worldDispatch_t *gl,
		glName_t uniformBuffer, glName_t materialUniformBuffer,
		productDrawUniforms_t *uniforms,
		const renderSubmissionState_t *frontend, const glName_t *textures,
		const renderUiPrimitive_t *ui, qboolean msdf ) {
	productBeamParams_t materialParams;
	effectVertex_t vertices[4];
	const uint32_t indices[6] = { 0u, 1u, 2u, 2u, 1u, 3u };
	transientGeometry_t geometry;
	static const uint8_t positionIndex[4] = { 0u, 1u, 3u, 2u };
	memset( vertices, 0, sizeof( vertices ) );
	for ( uint32_t i = 0u; i < 4u; ++i ) {
		uint32_t corner = positionIndex[i];
		vertices[i].position[0] = ui->positions[corner][0];
		vertices[i].position[1] = ui->positions[corner][1];
		vertices[i].texCoord[0] = ui->kind == RENDER_UI_LINE
			? ( ( i & 1u ) ? 1.0f : 0.0f ) : ( ( i & 1u ) ? ui->s2 : ui->s1 );
		vertices[i].texCoord[1] = ui->kind == RENDER_UI_LINE
			? ( ( i & 2u ) ? 1.0f : 0.0f ) : ( ( i & 2u ) ? ui->t2 : ui->t1 );
		for ( uint32_t c = 0u; c < 4u; ++c ) {
			float value = ui->color[c] * 255.0f;
			vertices[i].color[c] = (unsigned char)( value < 0.0f ? 0u
				: value > 255.0f ? 255u : (unsigned int)value );
		}
	}
	memset( &materialParams, 0, sizeof( materialParams ) );
	materialParams.flags[1] = msdf ? 1u : 0u;
	if ( !TransientCreate( gl, vertices, 4u, indices, 6u, &geometry ) ) return qfalse;
	gl->BindVertexArray( geometry.vao );
	gl->BindTextureUnit( 0u, MaterialTexture( frontend, textures, ui->material ) );
	ApplyRaster( gl, uniformBuffer, materialUniformBuffer, uniforms, &materialParams,
		RENDER_ALPHA_BLEND,
		0.5f, qfalse, 0.0f, qfalse, NULL, NULL, NULL, NULL,
		NULL, 1.0f, NULL );
	gl->DrawElements( GL_TRIANGLES_VALUE, 6, GL_UNSIGNED_INT_VALUE, NULL );
	TransientDestroy( gl, &geometry );
	return qtrue;
}

static void ResolveEffectBeamEndpoint( const renderSubmissionState_t *frontend,
		const beamDesc_t *beam, qboolean start, float out[3] )
{
	const int entityNum = start ? beam->startEntityNum : beam->endEntityNum;
	const float *position = start ? beam->start : beam->end;
	const float *offset = start ? beam->startOffset : beam->endOffset;
	if ( frontend && entityNum >= 0 && (uint32_t)entityNum < frontend->entityCount ) {
		for ( uint32_t axis = 0u; axis < 3u; ++axis )
			out[axis] = frontend->entities[entityNum].entity.origin[axis]
				+ offset[axis];
	} else memcpy( out, position, 3u * sizeof( float ) );
}

static float EffectBeamFade( const renderEffectBeamSlot_t *slot, float now )
{
	float fade = 1.0f;
	const float age = slot ? fmaxf( now - slot->spawnTime, 0.0f ) : 0.0f;
	if ( !slot || slot->transient || slot->beam.duration <= 0.0f ) return 1.0f;
	if ( slot->beam.fadeIn > 0.0f ) fade = fminf( fade, age / slot->beam.fadeIn );
	if ( slot->beam.fadeOut > 0.0f ) fade = fminf( fade,
		fmaxf( slot->beam.duration - age, 0.0f ) / slot->beam.fadeOut );
	return fminf( 1.0f, fmaxf( 0.0f, fade ) );
}

static void RotateEffectBeamAxis( const float vector[3], const float axis[3],
		float angle, float out[3] )
{
	const float cosine = cosf( angle ), sine = sinf( angle );
	const float dot = vector[0] * axis[0] + vector[1] * axis[1]
		+ vector[2] * axis[2];
	const float cross[3] = { axis[1] * vector[2] - axis[2] * vector[1],
		axis[2] * vector[0] - axis[0] * vector[2],
		axis[0] * vector[1] - axis[1] * vector[0] };
	for ( uint32_t component = 0u; component < 3u; ++component )
		out[component] = vector[component] * cosine + cross[component] * sine
			+ axis[component] * dot * ( 1.0f - cosine );
}

qboolean RalOpenGl_WorldLower( ralOpenGlCore_t *core,
		const ralOpenGlCoreReceipt_t *coreReceipt,
		const renderSubmissionState_t *frontend,
		const renderSubmissionReceipt_t *submission,
		const ralOpenGlFrontendPlanReceipt_t *plan,
		const ralOpenGlWorldLowerInfo_t *info,
		ralOpenGlWorldReceipt_t *outReceipt ) {
	worldDispatch_t gl;
	ralOpenGlWorldReceipt_t receipt = { 0 };
	renderWorldSnapshot_t world = { 0 };
	qboolean entityTwoSided = qfalse;
	const renderDistantWorldSubmission_t *distant = info
		? info->distantWorld : NULL;
	const renderPolyCommand_t *polygons = NULL;
	const polyVert_t *polyVertices = NULL;
	const renderLightCommand_t *lights = NULL;
	renderEffectPrimitiveSnapshot_t effectPrimitives;
	const spriteDesc_t *effectSprites = NULL;
	const emitterDesc_t *effectEmitters = NULL;
	const decalDesc_t *effectDecals = NULL;
	const renderEffectRibbonCommand_t *effectRibbons = NULL;
	const ribbonPoint_t *effectRibbonPoints = NULL;
	const beamDesc_t *effectBeams = NULL;
	renderWorldSnapshot_t effectView = { 0 };
	const renderEntityCommand_t *entities = NULL;
	const renderUiPrimitive_t *ui = NULL;
	uint32_t polygonCommandCount = 0u, polyVertexCount = 0u, lightCount = 0u;
	uint32_t entityCount = 0u, uiCount = 0u;
	glName_t buffers[6] = { 0u }, vaos[3] = { 0u };
	glName_t uniformBuffer = 0u, materialUniformBuffer = 0u;
	productDrawUniforms_t uniforms = { 0 };
	const glName_t *textures;
	effectVertex_t *effectVertices = NULL;
	uint32_t *effectIndices = NULL;
	entityMotionState_t *entityMotionStates = NULL;
	uint32_t effectVertexCount, effectIndexCount, effectVertexCursor = 0u;
	uint32_t effectIndexCursor = 0u;
	qboolean ok = qfalse;
	if ( !core || !coreReceipt || !frontend || !submission || !plan || !info
			|| !outReceipt || !info->generation || !info->programName
			|| !info->outputWidth || !info->outputHeight
			|| !info->materialTextureNames
			|| info->materialTextureCount != frontend->materialCount + frontend->stageTextureCount
			|| !RalOpenGl_CoreMatchesReceipt( core, coreReceipt )
			|| !RalOpenGl_FrontendPlanReceiptExact( plan, plan )
			|| !Ral_AtmospherePlanReceiptExact( &info->atmosphere,
				&info->atmosphere )
			|| !Ral_DisplayVisibilityPlanValid( &info->displayVisibility )
			|| ( ( info->effectDecalPool
				|| info->effectDecalPoolReceipt.schemaVersion )
				&& ( !info->effectDecalPool
					|| !RenderEffectDecalPool_ReceiptExact( info->effectDecalPool,
						&info->effectDecalPoolReceipt )
					|| info->effectDecalPoolReceipt.frameGeneration
						!= info->generation ) )
			|| ( ( info->effectBeamPool
				|| info->effectBeamPoolReceipt.schemaVersion )
				&& ( !info->effectBeamPool
					|| !RenderEffectBeamPool_ReceiptExact( info->effectBeamPool,
						&info->effectBeamPoolReceipt )
					|| info->effectBeamPoolReceipt.frameGeneration
						!= info->generation ) )
			|| info->atmosphere.frameGeneration != info->generation
			|| !info->atmosphereBufferName
			|| ( info->weatherParticleCount && ( !info->weatherProgramName
				|| !info->weatherUniformBufferName
				|| !info->weatherParticleBufferName ) )
			|| ( info->directionalLighting
				&& ( !RalOpenGl_LightingReceiptExact( info->directionalLighting,
						info->directionalLighting )
					|| info->directionalLighting->coreGeneration
						!= coreReceipt->generation ) )
			|| !RenderSubmission_ReceiptExact( submission, submission )
			|| plan->coreGeneration != coreReceipt->generation
			|| plan->frontendFrameDigest != submission->frameDigest
			|| !frontend->frameSealed || frontend->frameOpen
			|| !LoadDispatch( core, &gl ) ) return qfalse;
	if ( distant && ( !RenderSubmission_DistantWorldSubmissionExact(
			distant, distant ) || distant->backendType != RAL_BACKEND_OPENGL
			|| distant->frameGeneration != info->generation ) ) return qfalse;
	textures = (const glName_t *)info->materialTextureNames;
	if ( !RenderSubmission_EffectSnapshots( frontend, &polygons,
				&polygonCommandCount, &polyVertices, &polyVertexCount,
				&lights, &lightCount )
			|| plan->loweredPolygonCount != submission->polygonCount
			|| plan->loweredLightCount != lightCount ) return qfalse;
	memset( &effectPrimitives, 0, sizeof( effectPrimitives ) );
	if ( !RenderSubmission_EffectPrimitiveSnapshots( frontend,
			&effectPrimitives, &effectSprites, &effectEmitters, &effectDecals,
			&effectRibbons, &effectRibbonPoints, &effectBeams )
			|| effectPrimitives.emitterCount
			|| effectPrimitives.decalCount
				!= ( info->effectDecalPool
					? info->effectDecalPoolReceipt.submittedCount : 0u )
			|| effectPrimitives.beamCount
				!= ( info->effectBeamPool
					? info->effectBeamPoolReceipt.submittedCount : 0u )
			|| effectPrimitives.spriteCount != plan->loweredEffectSpriteCount
			|| effectPrimitives.ribbonCount != plan->loweredEffectRibbonCount
			|| ( effectPrimitives.spriteCount && !effectSprites )
			|| ( effectPrimitives.ribbonCount && !effectRibbons )
			|| ( effectPrimitives.ribbonPointCount && !effectRibbonPoints )
			|| ( ( effectPrimitives.spriteCount || effectPrimitives.ribbonCount
					|| ( info->effectBeamPool
						&& info->effectBeamPool->activeCount ) )
				&& !RenderSubmission_ViewSnapshot( frontend, &effectView ) ) )
		return qfalse;
	(void)effectEmitters; (void)effectDecals; (void)effectBeams;
	if ( submission->worldLoaded && submission->sceneRendered ) {
		if ( !RenderSubmission_WorldSnapshot( frontend, &world ) ) return qfalse;
		/* Retained world storage is not a draw request during teleport/UI views.
		 * Keep view parameters for entities, but perform no BSP uploads/draws. */
		if ( world.rdflags & RDF_NOWORLDMODEL )
			world.vertexCount = world.indexCount = world.batchCount = world.patchBatchCount = 0u;
		if ( plan->loweredWorldVertexCount != world.vertexCount
				|| plan->loweredWorldIndexCount != world.indexCount
				|| plan->loweredWorldBatchCount != world.batchCount ) return qfalse;
	} else if ( plan->loweredWorldVertexCount || plan->loweredWorldIndexCount
			|| plan->loweredWorldBatchCount ) return qfalse;
	entities = RenderSubmission_EntityCommands( frontend, &entityCount );
	{
		/* A UI model view (RDF_NOWORLDMODEL) draws its entities two-sided, as Metal does; read from the scene view, which exists
		 * whether or not a world is loaded. */
		renderWorldSnapshot_t sceneView;
		entityTwoSided = RenderSubmission_WorldSnapshot( frontend, &sceneView )
			&& ( sceneView.rdflags & RDF_NOWORLDMODEL ) ? qtrue : qfalse;
	}
	ui = RenderSubmission_UiPrimitives( frontend, &uiCount );
	if ( entityCount != submission->entityCount || uiCount != submission->uiPrimitiveCount
			|| ( entityCount && !entities ) || ( uiCount && !ui ) ) return qfalse;
	if ( lightCount > ( UINT32_MAX - polyVertexCount ) / 4u ) return qfalse;
	effectVertexCount = polyVertexCount + lightCount * 4u;
	if ( effectPrimitives.spriteCount
			> ( UINT32_MAX - effectVertexCount ) / 4u ) return qfalse;
	effectVertexCount += effectPrimitives.spriteCount * 4u;
	if ( info->effectDecalPool
			&& info->effectDecalPool->activeCount
				> ( UINT32_MAX - effectVertexCount ) / 4u ) return qfalse;
	effectVertexCount += info->effectDecalPool
		? info->effectDecalPool->activeCount * 4u : 0u;
	if ( effectPrimitives.ribbonPointCount
			> ( UINT32_MAX - effectVertexCount ) / 2u ) return qfalse;
	effectVertexCount += effectPrimitives.ribbonPointCount * 2u;
	for ( uint32_t i = 0u; info->effectBeamPool
			&& i < RENDER_SUBMISSION_MAX_EFFECT_BEAMS; ++i ) {
		const renderEffectBeamSlot_t *slot = &info->effectBeamPool->slots[i];
		if ( !slot->active ) continue;
		if ( (uint32_t)slot->beam.axialCopies
				> ( UINT32_MAX - effectVertexCount ) / 4u ) return qfalse;
		effectVertexCount += (uint32_t)slot->beam.axialCopies * 4u;
	}
	effectIndexCount = plan->loweredEffectIndexCount;
	if ( effectVertexCount ) {
		effectVertices = calloc( effectVertexCount, sizeof( *effectVertices ) );
		effectIndices = calloc( effectIndexCount, sizeof( *effectIndices ) );
		if ( !effectVertices || !effectIndices ) goto cleanup;
	}
	if ( distant ) {
		gl.CreateBuffers( 2, buffers + 4 );
		gl.NamedBufferStorage( buffers[4],
			(glSizePtr_t)( distant->world.vertexCount
				* sizeof( *distant->world.vertices ) ),
			distant->world.vertices, 0u );
		gl.NamedBufferStorage( buffers[5],
			(glSizePtr_t)( distant->world.indexCount
				* sizeof( *distant->world.indices ) ),
			distant->world.indices, 0u );
	}
	gl.CreateBuffers( 1, &uniformBuffer );
	if ( !uniformBuffer ) goto cleanup;
	gl.NamedBufferStorage( uniformBuffer, (glSizePtr_t)sizeof( uniforms ), NULL,
		GL_DYNAMIC_STORAGE_BIT_VALUE );
	gl.BindBufferBase( GL_UNIFORM_BUFFER_VALUE, 0u, uniformBuffer );
	gl.CreateBuffers( 1, &materialUniformBuffer );
	if ( !materialUniformBuffer ) goto cleanup;
	gl.NamedBufferStorage( materialUniformBuffer,
		(glSizePtr_t)sizeof( renderSkyMaterialGpuParams_t ), NULL,
		GL_DYNAMIC_STORAGE_BIT_VALUE );
	gl.BindBufferBase( GL_UNIFORM_BUFFER_VALUE,
		PRODUCT_MATERIAL_UNIFORM_BINDING, materialUniformBuffer );
	gl.BindBufferBase( GL_SHADER_STORAGE_BUFFER_VALUE, 0u,
		info->atmosphereBufferName );
	uniforms.viewportAlpha[0] = (float)info->outputWidth;
	uniforms.viewportAlpha[1] = (float)info->outputHeight;
	/* The view axes use xyz only; their std140 padding carries the display
	 * visibility plan while preserving the portable 256-byte draw ABI. */
	uniforms.viewForward[3] = info->displayVisibility.exposureScale;
	uniforms.viewLeft[3] = info->displayVisibility.shadowExponent;
	uniforms.viewUp[3] = info->displayVisibility.shadowPivot;
	for ( uint32_t i = 0u; i < polyVertexCount; ++i ) {
		memcpy( effectVertices[i].position, polyVertices[i].xyz,
			sizeof( effectVertices[i].position ) );
		memcpy( effectVertices[i].texCoord, polyVertices[i].st,
			sizeof( effectVertices[i].texCoord ) );
		memcpy( effectVertices[i].color, polyVertices[i].modulate.rgba,
			sizeof( effectVertices[i].color ) );
	}
	effectVertexCursor = polyVertexCount;
	for ( uint32_t i = 0u; i < polygonCommandCount; ++i ) {
		const renderPolyCommand_t *poly = &polygons[i];
		for ( uint32_t p = 0u; p < poly->polygonCount; ++p ) {
			uint32_t base = poly->firstVertex + p * poly->verticesPerPolygon;
			for ( uint32_t v = 1u; v + 1u < poly->verticesPerPolygon; ++v ) {
				effectIndices[effectIndexCursor++] = base;
				effectIndices[effectIndexCursor++] = base + v;
				effectIndices[effectIndexCursor++] = base + v + 1u;
			}
		}
	}
	for ( uint32_t i = 0u; i < lightCount; ++i ) {
		const renderLightCommand_t *light = &lights[i];
		uint32_t base = effectVertexCursor;
		float radius = light->intensity > 0.0f ? light->intensity * 0.01f : 0.01f;
		for ( uint32_t v = 0u; v < 4u; ++v ) {
			memcpy( effectVertices[base + v].position, light->origin,
				sizeof( light->origin ) );
			effectVertices[base + v].position[0] += ( v & 1u ) ? radius : -radius;
			effectVertices[base + v].position[1] += ( v & 2u ) ? radius : -radius;
			effectVertices[base + v].texCoord[0] = ( v & 1u ) ? 1.0f : 0.0f;
			effectVertices[base + v].texCoord[1] = ( v & 2u ) ? 1.0f : 0.0f;
			for ( uint32_t c = 0u; c < 3u; ++c ) {
				float value = light->color[c] * 255.0f;
				effectVertices[base + v].color[c] = (unsigned char)( value < 0.0f
					? 0u : value > 255.0f ? 255u : (unsigned int)value );
			}
			effectVertices[base + v].color[3] = 255u;
		}
		effectIndices[effectIndexCursor++] = base;
		effectIndices[effectIndexCursor++] = base + 1u;
		effectIndices[effectIndexCursor++] = base + 2u;
		effectIndices[effectIndexCursor++] = base + 2u;
		effectIndices[effectIndexCursor++] = base + 1u;
		effectIndices[effectIndexCursor++] = base + 3u;
		effectVertexCursor += 4u;
	}
	for ( uint32_t i = 0u; i < effectPrimitives.spriteCount; ++i ) {
		static const uint32_t order[6] = { 0u, 1u, 2u, 0u, 2u, 3u };
		static const float uv[4][2] = {
			{ 0.0f, 0.0f }, { 1.0f, 0.0f },
			{ 1.0f, 1.0f }, { 0.0f, 1.0f }
		};
		const spriteDesc_t *sprite = &effectSprites[i];
		uint32_t base = effectVertexCursor;
		for ( uint32_t corner = 0u; corner < 4u; ++corner ) {
			const float leftSign = ( corner == 0u || corner == 3u ) ? 1.0f : -1.0f;
			const float upSign = corner < 2u ? 1.0f : -1.0f;
			for ( uint32_t axis = 0u; axis < 3u; ++axis )
				effectVertices[base + corner].position[axis] = sprite->origin[axis]
					+ effectView.viewAxis[1][axis] * sprite->radius * leftSign
					+ effectView.viewAxis[2][axis] * sprite->radius * upSign;
			memcpy( effectVertices[base + corner].texCoord, uv[corner],
				sizeof( uv[corner] ) );
			for ( uint32_t channel = 0u; channel < 4u; ++channel ) {
				const float value = sprite->rgba[channel] * 255.0f;
				effectVertices[base + corner].color[channel] = (unsigned char)(
					value < 0.0f ? 0u : value > 255.0f ? 255u
						: (unsigned int)value );
			}
		}
		for ( uint32_t index = 0u; index < 6u; ++index )
			effectIndices[effectIndexCursor++] = base + order[index];
		effectVertexCursor += 4u;
	}
	for ( uint32_t slotIndex = 0u; info->effectDecalPool
			&& slotIndex < RENDER_SUBMISSION_MAX_EFFECT_DECALS; ++slotIndex ) {
		static const uint32_t order[6] = { 0u, 1u, 2u, 0u, 2u, 3u };
		static const float uv[4][2] = {
			{ 0.0f, 0.0f }, { 1.0f, 0.0f },
			{ 1.0f, 1.0f }, { 0.0f, 1.0f }
		};
		const renderEffectDecalSlot_t *slot = &info->effectDecalPool->slots[slotIndex];
		const decalDesc_t *decal;
		float seed[3], tangent[3], bitangent[3], tangentLength;
		float rotatedTangent[3], rotatedBitangent[3], corners[4][3];
		uint32_t base;
		if ( !slot->active ) continue;
		decal = &slot->decal;
		seed[0] = fabsf( decal->normal[2] ) < 0.9f ? 0.0f : 1.0f;
		seed[1] = 0.0f;
		seed[2] = fabsf( decal->normal[2] ) < 0.9f ? 1.0f : 0.0f;
		tangent[0] = decal->normal[1] * seed[2] - decal->normal[2] * seed[1];
		tangent[1] = decal->normal[2] * seed[0] - decal->normal[0] * seed[2];
		tangent[2] = decal->normal[0] * seed[1] - decal->normal[1] * seed[0];
		tangentLength = sqrtf( tangent[0] * tangent[0]
			+ tangent[1] * tangent[1] + tangent[2] * tangent[2] );
		if ( tangentLength < 0.00001f ) goto cleanup;
		for ( uint32_t axis = 0u; axis < 3u; ++axis ) tangent[axis] /= tangentLength;
		bitangent[0] = decal->normal[1] * tangent[2] - decal->normal[2] * tangent[1];
		bitangent[1] = decal->normal[2] * tangent[0] - decal->normal[0] * tangent[2];
		bitangent[2] = decal->normal[0] * tangent[1] - decal->normal[1] * tangent[0];
		for ( uint32_t axis = 0u; axis < 3u; ++axis ) {
			const float cosine = cosf( decal->orientation );
			const float sine = sinf( decal->orientation );
			const float center = decal->origin[axis] + decal->normal[axis] * 0.25f;
			rotatedTangent[axis] = ( tangent[axis] * cosine
				+ bitangent[axis] * sine ) * decal->radius;
			rotatedBitangent[axis] = ( bitangent[axis] * cosine
				- tangent[axis] * sine ) * decal->radius;
			corners[0][axis] = center - rotatedTangent[axis] - rotatedBitangent[axis];
			corners[1][axis] = center + rotatedTangent[axis] - rotatedBitangent[axis];
			corners[2][axis] = center + rotatedTangent[axis] + rotatedBitangent[axis];
			corners[3][axis] = center - rotatedTangent[axis] + rotatedBitangent[axis];
		}
		base = effectVertexCursor;
		for ( uint32_t corner = 0u; corner < 4u; ++corner ) {
			effectVertex_t *vertex = &effectVertices[effectVertexCursor++];
			memcpy( vertex->position, corners[corner], sizeof( vertex->position ) );
			memcpy( vertex->texCoord, uv[corner], sizeof( uv[corner] ) );
			for ( uint32_t channel = 0u; channel < 4u; ++channel ) {
				const float value = decal->rgba[channel] * 255.0f;
				vertex->color[channel] = (unsigned char)fminf( 255.0f,
					fmaxf( 0.0f, value ) );
			}
		}
		for ( uint32_t index = 0u; index < 6u; ++index )
			effectIndices[effectIndexCursor++] = base + order[index];
	}
	for ( uint32_t i = 0u; i < effectPrimitives.ribbonCount; ++i ) {
		const renderEffectRibbonCommand_t *ribbon = &effectRibbons[i];
		const uint32_t firstVertex = effectVertexCursor;
		for ( uint32_t pointIndex = 0u; pointIndex < ribbon->pointCount;
				++pointIndex ) {
			const ribbonPoint_t *point = &effectRibbonPoints[
				ribbon->firstPoint + pointIndex];
			const ribbonPoint_t *previous = &effectRibbonPoints[
				ribbon->firstPoint + ( pointIndex ? pointIndex - 1u : pointIndex )];
			const ribbonPoint_t *next = &effectRibbonPoints[
				ribbon->firstPoint + ( pointIndex + 1u < ribbon->pointCount
					? pointIndex + 1u : pointIndex )];
			float tangent[3], extrude[3], length;
			for ( uint32_t axis = 0u; axis < 3u; ++axis )
				tangent[axis] = next->pos[axis] - previous->pos[axis];
			if ( ribbon->flags & PRIM_FLAG_CUSTOM_NORMAL ) {
				memcpy( extrude, point->normal, sizeof( extrude ) );
			} else {
				const float *plane = ( ribbon->flags & PRIM_FLAG_VIEW_UP_PLANE )
					? effectView.viewAxis[2] : effectView.viewAxis[0];
				extrude[0] = tangent[1] * plane[2] - tangent[2] * plane[1];
				extrude[1] = tangent[2] * plane[0] - tangent[0] * plane[2];
				extrude[2] = tangent[0] * plane[1] - tangent[1] * plane[0];
			}
			length = sqrtf( extrude[0] * extrude[0] + extrude[1] * extrude[1]
				+ extrude[2] * extrude[2] );
			if ( length < 0.00001f ) goto cleanup;
			for ( uint32_t side = 0u; side < 2u; ++side ) {
				effectVertex_t *vertex = &effectVertices[effectVertexCursor++];
				const float sign = side ? -1.0f : 1.0f;
				for ( uint32_t axis = 0u; axis < 3u; ++axis )
					vertex->position[axis] = point->pos[axis]
						+ extrude[axis] / length * point->width * sign;
				vertex->texCoord[0] = (float)pointIndex
					/ (float)( ribbon->pointCount - 1u );
				vertex->texCoord[1] = (float)side;
				for ( uint32_t channel = 0u; channel < 4u; ++channel ) {
					const float value = point->rgba[channel] * 255.0f;
					vertex->color[channel] = (unsigned char)( value < 0.0f
						? 0u : value > 255.0f ? 255u : (unsigned int)value );
				}
			}
		}
		for ( uint32_t segment = 0u; segment + 1u < ribbon->pointCount;
				++segment ) {
			const uint32_t base = firstVertex + segment * 2u;
			const uint32_t order[6] = { base, base + 1u, base + 2u,
				base + 2u, base + 1u, base + 3u };
			memcpy( effectIndices + effectIndexCursor, order, sizeof( order ) );
			effectIndexCursor += 6u;
		}
	}
	for ( uint32_t slotIndex = 0u; info->effectBeamPool
			&& slotIndex < RENDER_SUBMISSION_MAX_EFFECT_BEAMS;
			slotIndex++ ) {
		const renderEffectBeamSlot_t *slot = &info->effectBeamPool->slots[slotIndex];
		const beamDesc_t *beam;
		float start[3], end[3], axis[3], baseExtrude[3];
		if ( !slot->active ) continue;
		beam = &slot->beam;
		ResolveEffectBeamEndpoint( frontend, beam, qtrue, start );
		ResolveEffectBeamEndpoint( frontend, beam, qfalse, end );
		if ( !wired_ral_beam_frame( start, end, effectView.viewOrigin,
				axis, baseExtrude ) ) goto cleanup;
		for ( uint32_t copy = 0u; copy < (uint32_t)beam->axialCopies; ++copy ) {
			float extrude[3], corners[4][3];
			const float angle = (float)M_PI * (float)copy
				/ (float)beam->axialCopies;
			const uint32_t base = effectVertexCursor;
			RotateEffectBeamAxis( baseExtrude, axis, angle, extrude );
			for ( uint32_t component = 0u; component < 3u; ++component ) {
				corners[0][component] = start[component] - extrude[component] * beam->startWidth;
				corners[1][component] = start[component] + extrude[component] * beam->startWidth;
				corners[2][component] = end[component] - extrude[component] * beam->endWidth;
				corners[3][component] = end[component] + extrude[component] * beam->endWidth;
			}
			for ( uint32_t corner = 0u; corner < 4u; ++corner ) {
				effectVertex_t *vertex = &effectVertices[effectVertexCursor++];
				memcpy( vertex->position, corners[corner], sizeof( vertex->position ) );
				vertex->texCoord[0] = corner >= 2u ? 1.0f : 0.0f;
				vertex->texCoord[1] = corner & 1u ? 1.0f : 0.0f;
				memset(vertex->color, 255, sizeof(vertex->color));
			}
			const uint32_t order[6] = { base, base + 1u, base + 2u,
				base + 2u, base + 1u, base + 3u };
			memcpy( effectIndices + effectIndexCursor, order, sizeof( order ) );
			effectIndexCursor += 6u;
		}
	}
	if ( effectIndexCursor != effectIndexCount
			|| effectVertexCursor != effectVertexCount ) goto cleanup;
	if ( world.vertexCount ) {
		gl.CreateBuffers( 2, buffers );
		gl.NamedBufferStorage( buffers[0], (glSizePtr_t)( world.vertexCount
			* sizeof( *world.vertices ) ), world.vertices, 0u );
		gl.NamedBufferStorage( buffers[1], (glSizePtr_t)( world.indexCount
			* sizeof( *world.indices ) ), world.indices, 0u );
	}
	if ( effectVertexCount ) {
		gl.CreateBuffers( 2, buffers + 2 );
		gl.NamedBufferStorage( buffers[2], (glSizePtr_t)( effectVertexCount
			* sizeof( *effectVertices ) ), effectVertices, 0u );
		gl.NamedBufferStorage( buffers[3], (glSizePtr_t)( effectIndexCount
			* sizeof( *effectIndices ) ), effectIndices, 0u );
	}
	if ( distant ) {
		gl.CreateVertexArrays( 1, vaos + 2 );
		ConfigureVertexArray( &gl, vaos[2], buffers[4], buffers[5],
			(glSize_t)sizeof( *distant->world.vertices ) );
	}
	if ( world.vertexCount ) {
		gl.CreateVertexArrays( 1, vaos );
		ConfigureVertexArray( &gl, vaos[0], buffers[0], buffers[1],
			(glSize_t)sizeof( *world.vertices ) );
	}
	if ( effectVertexCount ) {
		gl.CreateVertexArrays( 1, vaos + 1 );
		ConfigureVertexArray( &gl, vaos[1], buffers[2], buffers[3],
			(glSize_t)sizeof( *effectVertices ) );
	}
	gl.UseProgram( info->programName );
	gl.BindTextureUnit( 5u, info->physicalSkyTextureName );
	gl.BindTextureUnit( 6u, info->currentSkyRadianceTextureName );
	gl.BindTextureUnit( 7u, info->previousSkyRadianceTextureName );
	gl.BindTextureUnit( 8u, info->currentSkyIrradianceTextureName );
	gl.BindTextureUnit( 9u, info->previousSkyIrradianceTextureName );
	gl.Viewport( 0, 0, (glSize_t)info->outputWidth,
		(glSize_t)info->outputHeight );
	gl.FrontFace( RAL_OPENGL_WORLD_FRONT_FACE );
	gl.ClearColor( 0.015f, 0.02f, 0.03f, 1.0f );
	/* Clear owns its write state; the preceding frame may end in UI/alpha. */
	gl.DepthMask( 1u );
	gl.Clear( GL_COLOR_BUFFER_BIT_VALUE | GL_DEPTH_BUFFER_BIT_VALUE );
	if ( info->coverageActive ) {
		/* TAA history trust starts at 1; draws reduce it through their own blend factors (REVERSE_SUBTRACT on this attachment). */
		static const float fullTrust[4] = { 1.0f, RAL_OPENGL_WORLD_CAMERA_MOTION, RAL_OPENGL_WORLD_CAMERA_MOTION,
			RAL_OPENGL_WORLD_CAMERA_MOTION };
		/* The effect mask: r = full trust, gba = camera motion. The clear honours the colour mask, so open it first; afterwards the
		 * scene programs write r only and the object motion draw gba only. */
		gl.ColorMaski( 1u, 1u, 1u, 1u, 1u );
		gl.ClearBufferfv( GL_COLOR_VALUE, 1, fullTrust );
		gl.ColorMaski( 1u, 1u, 0u, 0u, 0u );
		gl.BlendEquationi( 1u, GL_FUNC_REVERSE_SUBTRACT_VALUE );
	}
	gl.Enable( GL_DEPTH_TEST_VALUE );
	gl.DepthFunc( GL_LEQUAL_VALUE );
	if ( ( world.batchCount || effectVertexCount || entityCount )
			&& ( !ApplyWorldView( &uniforms, &world )
				|| !ApplyAtmosphere( &uniforms, frontend, info ) ) ) goto cleanup;
	JitterWorldView( &uniforms, info->jitterNdc );
	if ( distant ) {
		/* Main authored/physical sky is the background authority.  Distant
		 * geometry is composited over it, then its depth is discarded so the
		 * gameplay world remains the foreground authority. */
		if ( world.batchCount ) gl.BindVertexArray( vaos[0] );
		for ( uint32_t i = 0u; i < world.batchCount; ++i ) {
			const renderWorldBatch_t *batch = &world.batches[i];
			if ( !batch->sky || batch->noDraw || !batch->visible ) continue;
			if ( !DrawWorldBatch( &gl, uniformBuffer, materialUniformBuffer,
					&uniforms, frontend, textures, info, batch,
					&receipt ) ) goto cleanup;
		}
		if ( !ApplyWorldView( &uniforms, &distant->world )
				|| !ApplyAtmosphere( &uniforms, frontend, info ) ) goto cleanup;
		JitterWorldView( &uniforms, info->jitterNdc );
		gl.BindVertexArray( vaos[2] );
		for ( uint32_t i = 0u; i < distant->drawPlan.admittedCount; ++i ) {
			const uint32_t source = distant->drawPlan.selections[i].sourceDrawIndex;
			if ( !DrawWorldBatch( &gl, uniformBuffer, materialUniformBuffer,
					&uniforms, frontend, textures, info,
					&distant->world.batches[source], &receipt ) ) goto cleanup;
			receipt.distantWorldDrawCount++;
		}
		/* The last distant material may disable depth writes. */
		gl.DepthMask( 1u );
		gl.Clear( GL_DEPTH_BUFFER_BIT_VALUE );
		if ( !ApplyWorldView( &uniforms, &world )
				|| !ApplyAtmosphere( &uniforms, frontend, info ) ) goto cleanup;
		JitterWorldView( &uniforms, info->jitterNdc );
		if ( world.batchCount ) gl.BindVertexArray( vaos[0] );
		for ( uint32_t i = 0u; i < world.batchCount; ++i ) {
			const renderWorldBatch_t *batch = &world.batches[i];
			if ( batch->sky ) continue;
			if ( !DrawWorldBatch( &gl, uniformBuffer, materialUniformBuffer,
					&uniforms, frontend, textures, info, batch,
					&receipt ) ) goto cleanup;
		}
	} else {
		if ( world.batchCount ) gl.BindVertexArray( vaos[0] );
		for ( uint32_t i = 0u; i < world.batchCount; ++i )
			if ( !DrawWorldBatch( &gl, uniformBuffer, materialUniformBuffer,
					&uniforms, frontend, textures, info, &world.batches[i],
					&receipt ) ) goto cleanup;
	}
	if ( effectVertexCount ) {
		uint32_t firstIndex = 0u;
		gl.BindVertexArray( vaos[1] );
		for ( uint32_t i = 0u; i < polygonCommandCount; ++i ) {
			uint32_t count = ( polygons[i].verticesPerPolygon - 2u ) * 3u
				* polygons[i].polygonCount;
			gl.BindTextureUnit( 0u, MaterialTexture( frontend, textures,
				polygons[i].material ) );
			ApplyRaster( &gl, uniformBuffer, materialUniformBuffer, &uniforms, NULL,
				RENDER_ALPHA_BLEND, 0.5f,
				qfalse, 0.0f, qfalse, NULL, NULL, NULL, NULL,
				NULL, 1.0f, NULL );
			gl.DrawElements( GL_TRIANGLES_VALUE, (glSize_t)count,
				GL_UNSIGNED_INT_VALUE, (const void *)(uintptr_t)( firstIndex
					* sizeof( uint32_t ) ) );
			firstIndex += count;
		}
		for ( uint32_t i = 0u; i < lightCount; ++i ) {
			gl.BindTextureUnit( 0u, 0u );
			ApplyRaster( &gl, uniformBuffer, materialUniformBuffer, &uniforms, NULL,
				RENDER_ALPHA_BLEND, 0.5f,
				qfalse, 0.0f, qfalse, NULL, NULL, NULL, NULL,
				NULL, 1.0f, NULL );
			gl.DrawElements( GL_TRIANGLES_VALUE, 6, GL_UNSIGNED_INT_VALUE,
				(const void *)(uintptr_t)( firstIndex * sizeof( uint32_t ) ) );
			firstIndex += 6u;
		}
		for ( uint32_t i = 0u; i < effectPrimitives.spriteCount; ++i ) {
			const spriteDesc_t *sprite = &effectSprites[i];
			if ( sprite->flags & PRIM_FLAG_NO_DEPTH_TEST )
				gl.Disable( GL_DEPTH_TEST_VALUE );
			else
				gl.Enable( GL_DEPTH_TEST_VALUE );
			gl.BindTextureUnit( 0u, MaterialTexture( frontend, textures,
				sprite->shader ) );
			ApplyRaster( &gl, uniformBuffer, materialUniformBuffer, &uniforms, &s_coverageParams,
				( sprite->flags & PRIM_FLAG_ADDITIVE )
					? RENDER_ALPHA_ALPHA_ADDITIVE : RENDER_ALPHA_BLEND,
				0.5f, qfalse, 0.0f, qfalse, NULL, NULL, NULL, NULL,
				NULL, 1.0f, NULL );
			gl.DrawElements( GL_TRIANGLES_VALUE, 6, GL_UNSIGNED_INT_VALUE,
				(const void *)(uintptr_t)( firstIndex * sizeof( uint32_t ) ) );
			firstIndex += 6u;
		}
		for ( uint32_t slotIndex = 0u; info->effectDecalPool
				&& slotIndex < RENDER_SUBMISSION_MAX_EFFECT_DECALS; ++slotIndex ) {
			const renderEffectDecalSlot_t *slot = &info->effectDecalPool->slots[slotIndex];
			if ( !slot->active ) continue;
			gl.Enable( GL_DEPTH_TEST_VALUE );
			gl.BindTextureUnit( 0u, MaterialTexture( frontend, textures,
				slot->decal.shader ) );
			ApplyRaster( &gl, uniformBuffer, materialUniformBuffer, &uniforms, &s_coverageParams,
				RENDER_ALPHA_BLEND, 0.5f, qfalse, 0.0f, qfalse,
				NULL, NULL, NULL, NULL, NULL, 1.0f, NULL );
			gl.DrawElements( GL_TRIANGLES_VALUE, 6, GL_UNSIGNED_INT_VALUE,
				(const void *)(uintptr_t)( firstIndex * sizeof( uint32_t ) ) );
			firstIndex += 6u;
			receipt.effectDecalDrawCount++;
		}
		for ( uint32_t i = 0u; i < effectPrimitives.ribbonCount; ++i ) {
			const renderEffectRibbonCommand_t *ribbon = &effectRibbons[i];
			const uint32_t count = ( ribbon->pointCount - 1u ) * 6u;
			if ( ribbon->flags & PRIM_FLAG_NO_DEPTH_TEST )
				gl.Disable( GL_DEPTH_TEST_VALUE );
			else
				gl.Enable( GL_DEPTH_TEST_VALUE );
			gl.BindTextureUnit( 0u, MaterialTexture( frontend, textures,
				ribbon->material ) );
			ApplyRaster( &gl, uniformBuffer, materialUniformBuffer, &uniforms, &s_coverageParams,
				( ribbon->flags & PRIM_FLAG_ADDITIVE )
					? RENDER_ALPHA_ALPHA_ADDITIVE : RENDER_ALPHA_BLEND,
				0.5f, qfalse, 0.0f, qfalse, NULL, NULL, NULL, NULL,
				NULL, 1.0f, NULL );
			gl.DrawElements( GL_TRIANGLES_VALUE, (glSize_t)count,
				GL_UNSIGNED_INT_VALUE,
				(const void *)(uintptr_t)( firstIndex * sizeof( uint32_t ) ) );
			firstIndex += count;
		}
		for ( uint32_t slotIndex = 0u; info->effectBeamPool &&
				slotIndex < RENDER_SUBMISSION_MAX_EFFECT_BEAMS; ++slotIndex ) {
			const renderEffectBeamSlot_t *slot = &info->effectBeamPool->slots[slotIndex];
			if ( !slot->active ) continue;
			const uint32_t count = (uint32_t)slot->beam.axialCopies * 6u;
			if ( slot->beam.flags & PRIM_FLAG_NO_DEPTH_TEST ) gl.Disable( GL_DEPTH_TEST_VALUE );
			else gl.Enable( GL_DEPTH_TEST_VALUE );
			renderMaterialSnapshot_t material;
			if (!RenderSubmission_MaterialSnapshot(frontend, slot->beam.shader, &material)) goto cleanup;
			const float now = (float)effectView.timeMs * .001f;
			const float clock = slot->transient ? now : fmaxf(now - slot->spawnTime, 0.0f);
			const float fade = EffectBeamFade(slot, now);
			const uint32_t stages = material.stageCount ? material.stageCount : 1u;
			for (uint32_t s = 0u; s < stages; ++s) {
				productBeamParams_t params = {0};
				qhandle_t image = slot->beam.shader;
				memcpy(params.startColor, slot->beam.startColor, sizeof(params.startColor));
				memcpy(params.endColor, slot->beam.endColor, sizeof(params.endColor));
				params.startColor[3] *= fade; params.endColor[3] *= fade;
				params.flags[0] = 1u;
				params.flags[2] = 1u;   /* TAA coverage */
				params.scaleOffset[0] = params.scaleOffset[1] = 1.0f;
				for (uint32_t a = 0u; a < 2u; ++a)
					params.scaleOffset[2u+a] = slot->beam.uvScroll[a] * clock;
				if (material.stageCount) {
					const renderMaterialStageSnapshot_t *stage = &material.stages[s];
					if (stage->imageSource != 0u || stage->material <= 0) goto cleanup;
					image = stage->material;
					if (stage->animationFrameCount && stage->animationFrequency > 0.0f) {
						double frame = fmax(floor((double)effectView.timeMs * .001 * stage->animationFrequency), 0.0);
						frame = stage->animationLooping ? fmod(frame, stage->animationFrameCount)
							: fmin(frame, stage->animationFrameCount - 1u);
						image = stage->animationMaterials[(uint32_t)frame];
					}
					for (uint32_t a = 0u; a < 2u; ++a) {
						params.scaleOffset[a] = stage->scaleScroll[a];
						params.scaleOffset[2u+a] += stage->scaleScroll[2u+a] * clock;
					}
				}
				const glName_t texture = MaterialTexture(frontend, textures, image);
				if (!texture) goto cleanup;
				gl.BindTextureUnit(0u, texture);
				ApplyRaster(&gl, uniformBuffer, materialUniformBuffer, &uniforms, &params,
					(material.stageCount || (slot->beam.flags & PRIM_FLAG_ADDITIVE))
						? RENDER_ALPHA_ALPHA_ADDITIVE : RENDER_ALPHA_BLEND,
					0.5f, qfalse, 0.0f, qfalse, NULL, NULL, NULL, NULL, NULL, 1.0f, NULL);
				gl.DrawElements(GL_TRIANGLES_VALUE, (glSize_t)count, GL_UNSIGNED_INT_VALUE,
					(const void *)(uintptr_t)(firstIndex * sizeof(uint32_t)));
				receipt.effectBeamStageDrawCount++;
			}
			firstIndex += count;
			receipt.effectBeamDrawCount++;
		}
		gl.Enable( GL_DEPTH_TEST_VALUE );
	}
	if ( entityCount && info->coverageActive && info->entityMotionActive ) {
		entityMotionStates = calloc( entityCount, sizeof( *entityMotionStates ) );
		if ( !entityMotionStates ) goto cleanup;
		PrepareEntityMotion( info, frontend, entities, entityCount, entityMotionStates, &receipt );
	}
	/* Two phases: depth-writing entity geometry, then blended batches and the primitive (sprite/beam) entities, which blend. Between
	 * them, with the opaque depth final, the moving entities draw their object motion into the effect mask. */
	for ( uint32_t phase = 0u; phase < 2u; ++phase ) for ( uint32_t i = 0u; i < entityCount; ++i ) {
		uint32_t drawsBefore = receipt.entityDrawCount;
		if ( phase == 1u && i == 0u && entityMotionStates && receipt.entityMotionPrevious ) {
			if ( !info->objectMotionProgramName ) goto cleanup;
			gl.UseProgram( info->objectMotionProgramName );
			gl.Disable( GL_BLEND_VALUE );
			gl.Enable( GL_DEPTH_TEST_VALUE );
			gl.DepthFunc( GL_LEQUAL_VALUE );
			gl.DepthMask( 0u );
			gl.ColorMaski( 0u, 0u, 0u, 0u, 0u );
			gl.ColorMaski( 1u, 0u, 1u, 1u, 1u );
			UploadProductDraw( &gl, uniformBuffer, &uniforms );
			for ( uint32_t m = 0u; m < entityCount; ++m ) {
				if ( !entityMotionStates[m].hasPrevious ) continue;
				if ( !DrawEntityMotion( &gl, frontend, &entities[m], &entityMotionStates[m], &receipt.entityMotionDraws ) )
					goto cleanup;
			}
			gl.ColorMaski( 0u, 1u, 1u, 1u, 1u );
			gl.ColorMaski( 1u, 1u, 0u, 0u, 0u );
			gl.UseProgram( info->programName );
		}
		if ( !RalOpenGl_EntityInPrimaryView( &entities[i].entity ) ) continue;
		if ( entities[i].entity.reType == RT_MODEL
				&& entities[i].entity.hModel > 0 ) {
			if ( !DrawModelEntity( &gl, uniformBuffer, materialUniformBuffer,
				&uniforms, frontend, textures,
				&entities[i], entityMotionStates ? &entityMotionStates[i] : NULL, phase,
				entityTwoSided,
				&receipt.entityIndexCount, &receipt.entityDrawCount ) ) goto cleanup;
			if ( phase == 0u ) receipt.modelEntityCount++;
		} else if ( phase == 1u ) {
			if ( !DrawPrimitiveEntity( &gl, uniformBuffer, materialUniformBuffer,
				&uniforms, frontend, textures,
				&entities[i],
				&receipt.entityIndexCount, &receipt.entityDrawCount ) ) goto cleanup;
			receipt.primitiveEntityCount++;
		}
		if ( phase == 0u && entities[i].hasTemporal ) receipt.temporalEntityCount++;
		if ( entities[i].hasLocalIrradiance ) {
			if ( phase == 0u ) receipt.localIrradianceEntityCount++;
			receipt.localIrradianceDrawCount += receipt.entityDrawCount - drawsBefore;
		}
	}
	if ( info->weatherParticleCount ) {
		glName_t weatherVao = 0u;
		gl.CreateVertexArrays( 1, &weatherVao );
		if ( !weatherVao ) goto cleanup;
		gl.UseProgram( info->weatherProgramName );
		gl.BindBufferBase( GL_UNIFORM_BUFFER_VALUE, 0u,
			info->weatherUniformBufferName );
		gl.BindBufferBase( GL_SHADER_STORAGE_BUFFER_VALUE, 0u,
			info->weatherParticleBufferName );
		gl.BindVertexArray( weatherVao );
		gl.Enable( GL_BLEND_VALUE );
		gl.BlendFunc( GL_SRC_ALPHA_VALUE, GL_ONE_MINUS_SRC_ALPHA_VALUE );
		gl.DepthMask( 0u );
		/* Weather writes no coverage output and stays out of the TAA coverage, as on Metal. */
		if ( info->coverageActive ) gl.ColorMaski( 1u, 0u, 0u, 0u, 0u );
		gl.DrawArraysInstanced( GL_TRIANGLES_VALUE, 0, 6,
			(glSize_t)info->weatherParticleCount );
		if ( info->coverageActive ) gl.ColorMaski( 1u, 1u, 1u, 1u, 1u );
		gl.DeleteVertexArrays( 1, &weatherVao );
		receipt.weatherDrawCount = 1u;
		receipt.weatherInstanceCount = info->weatherParticleCount;
		gl.UseProgram( info->programName );
		gl.BindBufferBase( GL_UNIFORM_BUFFER_VALUE, 0u, uniformBuffer );
		gl.BindBufferBase( GL_UNIFORM_BUFFER_VALUE,
			PRODUCT_MATERIAL_UNIFORM_BINDING,
			materialUniformBuffer );
		gl.BindBufferBase( GL_SHADER_STORAGE_BUFFER_VALUE, 0u,
			info->atmosphereBufferName );
	}
	if ( info->sceneComplete ) {
		if ( info->coverageActive ) gl.BlendEquationi( 1u, GL_FUNC_ADD_VALUE );
		if ( !info->sceneComplete( info->sceneCompleteContext ) ) goto cleanup;
		gl.UseProgram( info->programName );
		gl.BindBufferBase( GL_UNIFORM_BUFFER_VALUE, 0u, uniformBuffer );
		gl.BindBufferBase( GL_UNIFORM_BUFFER_VALUE,
			PRODUCT_MATERIAL_UNIFORM_BINDING, materialUniformBuffer );
		gl.BindBufferBase( GL_SHADER_STORAGE_BUFFER_VALUE, 0u,
			info->atmosphereBufferName );
	}
	if ( uiCount ) {
		gl.Disable( GL_DEPTH_TEST_VALUE );
		uniforms.viewOriginDrawSpace[3] = 1.0f;
		memset( uniforms.atmosphereFroxelGrid, 0,
			sizeof( uniforms.atmosphereFroxelGrid ) );
	}
	for ( uint32_t i = 0u; i < uiCount; ++i ) {
		renderMaterialSnapshot_t material;
		if ( !RenderSubmission_MaterialSnapshot( frontend, ui[i].material, &material )
				|| !DrawUiPrimitive( &gl, uniformBuffer, materialUniformBuffer,
					&uniforms, frontend, textures,
					&ui[i], material.msdf ) ) goto cleanup;
		receipt.uiPrimitiveCount++; receipt.texturedUiPrimitiveCount++;
		if ( material.msdf ) receipt.msdfUiPrimitiveCount++;
		receipt.uiDrawCount++;
	}
	if ( gl.GetError() != GL_NO_ERROR_VALUE ) goto cleanup;
	{
		ralOpenGlWorldReceipt_t content = receipt;
		memset( &receipt, 0, sizeof( receipt ) );
		receipt.modelEntityCount = content.modelEntityCount;
		receipt.primitiveEntityCount = content.primitiveEntityCount;
		receipt.temporalEntityCount = content.temporalEntityCount;
		receipt.entityMotionRecorded = content.entityMotionRecorded;
		receipt.entityMotionPrevious = content.entityMotionPrevious;
		receipt.entityMotionDraws = content.entityMotionDraws;
		receipt.localIrradianceEntityCount = content.localIrradianceEntityCount;
		receipt.localIrradianceDrawCount = content.localIrradianceDrawCount;
		receipt.entityIndexCount = content.entityIndexCount;
		receipt.entityDrawCount = content.entityDrawCount;
		receipt.uiPrimitiveCount = content.uiPrimitiveCount;
		receipt.texturedUiPrimitiveCount = content.texturedUiPrimitiveCount;
		receipt.msdfUiPrimitiveCount = content.msdfUiPrimitiveCount;
		receipt.uiDrawCount = content.uiDrawCount;
		receipt.weatherDrawCount = content.weatherDrawCount;
		receipt.weatherInstanceCount = content.weatherInstanceCount;
		receipt.effectBeamDrawCount = content.effectBeamDrawCount;
		receipt.effectBeamStageDrawCount = content.effectBeamStageDrawCount;
		receipt.effectDecalDrawCount = content.effectDecalDrawCount;
		receipt.distantWorldDrawCount = content.distantWorldDrawCount;
		receipt.legacyLightmapDrawCount = content.legacyLightmapDrawCount;
		receipt.directionalStaticDrawCount = content.directionalStaticDrawCount;
		receipt.surfaceLightingBindingDigest = content.surfaceLightingBindingDigest;
	}
	receipt.schemaVersion = RAL_OPENGL_WORLD_SCHEMA_VERSION;
	receipt.backendType = RAL_BACKEND_OPENGL;
	receipt.coreGeneration = coreReceipt->generation;
	receipt.generation = info->generation;
	receipt.frontendFrameDigest = plan->frontendFrameDigest;
	receipt.loweringDigest = plan->loweringDigest;
	receipt.uploadedMaterialCount = frontend->materialCount;
	receipt.uploadedWorldVertexCount = world.vertexCount;
	receipt.uploadedWorldIndexCount = world.indexCount;
	receipt.worldDrawCount = world.batchCount;
	if ( distant ) {
		receipt.uploadedDistantWorldVertexCount = distant->world.vertexCount;
		receipt.uploadedDistantWorldIndexCount = distant->world.indexCount;
		receipt.distantWorldCandidateCount = distant->drawPlan.candidateCount;
	}
	receipt.polygonCount = plan->loweredPolygonCount;
	receipt.lightCount = plan->loweredLightCount;
	receipt.effectIndexCount = effectIndexCount;
	receipt.effectSpriteDrawCount = effectPrimitives.spriteCount;
	receipt.effectDecalDrawCount = info->effectDecalPool
		? info->effectDecalPool->activeCount : 0u;
	receipt.effectRibbonDrawCount = effectPrimitives.ribbonCount;
	receipt.effectDrawCount = polygonCommandCount + lightCount
		+ receipt.effectSpriteDrawCount + receipt.effectDecalDrawCount
		+ receipt.effectRibbonDrawCount
		+ receipt.effectBeamDrawCount;
	receipt.nativeDrawCount = receipt.worldDrawCount + receipt.effectDrawCount
		+ receipt.effectBeamStageDrawCount - receipt.effectBeamDrawCount
		+ receipt.distantWorldDrawCount + receipt.entityDrawCount
		+ receipt.uiDrawCount + receipt.weatherDrawCount;
	receipt.ready = qtrue;
	if ( receipt.uploadedMaterialCount != plan->loweredMaterialCount
			|| receipt.worldDrawCount != plan->loweredWorldBatchCount
			|| receipt.effectIndexCount != plan->loweredEffectIndexCount
			|| receipt.effectDrawCount != plan->loweredEffectBatchCount
			|| receipt.effectSpriteDrawCount != plan->loweredEffectSpriteCount
			|| receipt.effectDecalDrawCount != plan->loweredEffectDecalCount
			|| receipt.effectRibbonDrawCount != plan->loweredEffectRibbonCount
			|| receipt.effectBeamDrawCount != plan->loweredEffectBeamCount
			|| receipt.modelEntityCount != plan->loweredModelEntityCount
			|| receipt.primitiveEntityCount != plan->loweredPrimitiveEntityCount
			|| receipt.temporalEntityCount != plan->loweredTemporalEntityCount
			|| receipt.localIrradianceEntityCount
				!= plan->loweredLocalIrradianceEntityCount
			|| receipt.entityIndexCount != plan->loweredEntityIndexCount
			|| receipt.entityDrawCount != plan->loweredEntityBatchCount
			|| receipt.uiPrimitiveCount != plan->loweredUiPrimitiveCount
			|| receipt.texturedUiPrimitiveCount != plan->texturedUiPrimitiveCount
			|| receipt.msdfUiPrimitiveCount != plan->msdfUiPrimitiveCount
			|| !ReceiptValid( &receipt ) ) goto cleanup;
	ok = qtrue;
cleanup:
	/* A failed pass must not leave the trust attachment's REVERSE_SUBTRACT equation behind (gl is loaded once coverage is set). */
	if ( !ok && info->coverageActive && gl.BlendEquationi ) gl.BlendEquationi( 1u, GL_FUNC_ADD_VALUE );
	/* Culling is global state too: the passes after this one (tone map, anti-aliasing, UI) draw with it off. */
	if ( gl.Disable ) gl.Disable( GL_CULL_FACE_VALUE );
	/* Leave the attachment-1 colour mask open: it is global state, and the next pass clears through it. */
	if ( info->coverageActive && gl.ColorMaski ) gl.ColorMaski( 1u, 1u, 1u, 1u, 1u );
	free( entityMotionStates );
	if ( materialUniformBuffer ) gl.DeleteBuffers( 1, &materialUniformBuffer );
	if ( uniformBuffer ) gl.DeleteBuffers( 1, &uniformBuffer );
	if ( vaos[0] || vaos[1] || vaos[2] ) gl.DeleteVertexArrays( 3, vaos );
	if ( buffers[0] || buffers[1] || buffers[2] || buffers[3]
			|| buffers[4] || buffers[5] ) gl.DeleteBuffers( 6, buffers );
	free( effectIndices );
	free( effectVertices );
	if ( ok ) *outReceipt = receipt;
	return ok;
}
