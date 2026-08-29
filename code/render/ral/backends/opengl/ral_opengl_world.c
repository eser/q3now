// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_opengl_world.h"
#include "ral_opengl_internal.h"

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
	GL_DEPTH_TEST_VALUE = 0x0b71,
	GL_LEQUAL_VALUE = 0x0203,
	GL_COLOR_BUFFER_BIT_VALUE = 0x00004000,
	GL_DEPTH_BUFFER_BIT_VALUE = 0x00000100,
	GL_SRC_ALPHA_VALUE = 0x0302,
	GL_ONE_MINUS_SRC_ALPHA_VALUE = 0x0303,
	GL_UNIFORM_BUFFER_VALUE = 0x8a11,
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
typedef void ( RAL_GL_APIENTRY *blendFuncFn )( glEnum_t, glEnum_t );
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
	drawElementsFn DrawElements;
	drawArraysInstancedFn DrawArraysInstanced;
	enableFn Enable;
	disableFn Disable;
	blendFuncFn BlendFunc;
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
	float atmosphereHeightCloud[4];
	uint32_t atmosphereFroxelGrid[4];
	float localSh[4][4];
	uint32_t staticLighting[4];
	float emissiveRadiance[4];
} productDrawUniforms_t;

typedef char productDrawUniformsMustMatchStd140[
	sizeof( productDrawUniforms_t ) == 256u ? 1 : -1];

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
	LOAD_GL( DrawElements, "glDrawElements", drawElementsFn );
	LOAD_GL( DrawArraysInstanced, "glDrawArraysInstanced",
		drawArraysInstancedFn );
	LOAD_GL( Enable, "glEnable", enableFn );
	LOAD_GL( Disable, "glDisable", disableFn );
	LOAD_GL( BlendFunc, "glBlendFunc", blendFuncFn );
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
		&& receipt->localIrradianceEntityCount
			<= receipt->modelEntityCount + receipt->primitiveEntityCount
		&& receipt->localIrradianceDrawCount <= receipt->entityDrawCount
		&& receipt->legacyLightmapDrawCount + receipt->directionalStaticDrawCount
			<= receipt->worldDrawCount
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
		productDrawUniforms_t *uniforms,
		renderAlphaMode_t alpha, float alphaCutoff, qboolean depthWrite,
		qboolean hasLightmap, const renderEntityCommand_t *entityCommand,
		const ralLightingSurfaceBindingReceipt_t *surfaceLighting,
		const ralLightingRuntimePlan_t *lightingPlan,
		const float emissiveRadiance[4] ) {
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
	uniforms->projectionLightmap[3] = hasLightmap ? 1.0f : 0.0f;
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

static qboolean ApplyAtmosphere( productDrawUniforms_t *uniforms,
		const renderSubmissionState_t *frontend,
		const ralOpenGlWorldLowerInfo_t *info ) {
	renderAtmosphereSnapshot_t snapshot;
	const atmosphereEmitter_t *emitters;
	float sunWeight, lightning;
	if ( !uniforms || !frontend || !info
			|| !RenderSubmission_AtmosphereSnapshot( frontend, &snapshot,
				&emitters ) ) return qfalse;
	(void)emitters;
	memcpy( uniforms->atmosphereEyeDensity, uniforms->viewOriginDrawSpace,
		3u * sizeof( float ) );
	if ( !snapshot.active
			|| snapshot.state.qualityTier < ATMOSPHERE_QUALITY_ANALYTIC )
		return qtrue;
	uniforms->atmosphereEyeDensity[3] = snapshot.state.mediaDensity;
	uniforms->atmosphereColorVisibility[3] = snapshot.state.visibility;
	sunWeight = snapshot.state.sunIntensity * ( 1.0f
		- snapshot.state.cloudCover * snapshot.state.cloudShadow );
	lightning = snapshot.state.lightning;
	for ( uint32_t channel = 0u; channel < 3u; ++channel )
		uniforms->atmosphereColorVisibility[channel] = fminf( 16.0f,
			snapshot.state.ambientColor[channel]
			+ 0.08f * sunWeight + lightning );
	uniforms->atmosphereHeightCloud[0] = snapshot.state.bounds[2];
	uniforms->atmosphereHeightCloud[1] = snapshot.state.mediaHeightFalloff;
	uniforms->atmosphereHeightCloud[2] = snapshot.state.cloudCover;
	uniforms->atmosphereHeightCloud[3] = snapshot.state.cloudShadow;
	if ( info->atmosphere.selectedTier == RAL_ATMOSPHERE_TIER_FULL ) {
		uniforms->atmosphereFroxelGrid[0] = info->atmosphere.froxelWidth;
		uniforms->atmosphereFroxelGrid[1] = info->atmosphere.froxelHeight;
		uniforms->atmosphereFroxelGrid[2] = info->atmosphere.froxelDepth;
		uniforms->atmosphereFroxelGrid[3] = 0u;
	}
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

static qboolean DrawModelEntity( const worldDispatch_t *gl,
		glName_t uniformBuffer, productDrawUniforms_t *uniforms,
		const renderSubmissionState_t *frontend, const glName_t *textures,
		const renderEntityCommand_t *command, uint32_t *outIndices,
		uint32_t *outDraws ) {
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
		memcpy( vertices[i].color, command->entity.shader.rgba,
			sizeof( vertices[i].color ) );
	}
	if ( !TransientCreate( gl, vertices, model.vertexCount, model.indices,
		model.indexCount, &geometry ) ) goto cleanup;
	gl->BindVertexArray( geometry.vao );
	for ( uint32_t i = 0u; i < model.batchCount; ++i ) {
		qhandle_t material = RenderSubmission_EntityBatchMaterial(
			frontend, command, &model.batches[i] );
		gl->BindTextureUnit( 0u, MaterialTexture( frontend, textures, material ) );
		ApplyRaster( gl, uniformBuffer, uniforms, RENDER_ALPHA_OPAQUE,
			0.5f, qtrue, qfalse, command, NULL, NULL, NULL );
		gl->DrawElements( GL_TRIANGLES_VALUE, (glSize_t)model.batches[i].indexCount,
			GL_UNSIGNED_INT_VALUE, (const void *)(uintptr_t)( model.batches[i].firstIndex
				* sizeof( uint32_t ) ) );
	}
	*outIndices += model.indexCount; *outDraws += model.batchCount; ok = qtrue;
	TransientDestroy( gl, &geometry );
cleanup:
	free( vertices );
	return ok;
}

static qboolean DrawPrimitiveEntity( const worldDispatch_t *gl,
		glName_t uniformBuffer, productDrawUniforms_t *uniforms,
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
	ApplyRaster( gl, uniformBuffer, uniforms, RENDER_ALPHA_BLEND,
		0.5f, qfalse, qfalse, command, NULL, NULL, NULL );
	gl->DrawElements( GL_TRIANGLES_VALUE, (glSize_t)indexCount,
		GL_UNSIGNED_INT_VALUE, NULL );
	TransientDestroy( gl, &geometry );
	*outIndices += indexCount; ( *outDraws )++;
	return qtrue;
}

static qboolean DrawUiPrimitive( const worldDispatch_t *gl,
		glName_t uniformBuffer, productDrawUniforms_t *uniforms,
		const renderSubmissionState_t *frontend, const glName_t *textures,
		const renderUiPrimitive_t *ui ) {
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
	if ( !TransientCreate( gl, vertices, 4u, indices, 6u, &geometry ) ) return qfalse;
	gl->BindVertexArray( geometry.vao );
	gl->BindTextureUnit( 0u, MaterialTexture( frontend, textures, ui->material ) );
	ApplyRaster( gl, uniformBuffer, uniforms, RENDER_ALPHA_BLEND,
		0.5f, qfalse, qfalse, NULL, NULL, NULL, NULL );
	gl->DrawElements( GL_TRIANGLES_VALUE, 6, GL_UNSIGNED_INT_VALUE, NULL );
	TransientDestroy( gl, &geometry );
	return qtrue;
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
	const renderPolyCommand_t *polygons = NULL;
	const polyVert_t *polyVertices = NULL;
	const renderLightCommand_t *lights = NULL;
	const renderEntityCommand_t *entities = NULL;
	const renderUiPrimitive_t *ui = NULL;
	uint32_t polygonCommandCount = 0u, polyVertexCount = 0u, lightCount = 0u;
	uint32_t entityCount = 0u, uiCount = 0u;
	glName_t buffers[4] = { 0u }, vaos[2] = { 0u };
	glName_t uniformBuffer = 0u;
	productDrawUniforms_t uniforms = { 0 };
	glName_t textures[RENDER_SUBMISSION_MAX_MATERIALS] = { 0u };
	effectVertex_t *effectVertices = NULL;
	uint32_t *effectIndices = NULL;
	uint32_t effectVertexCount, effectIndexCount, effectVertexCursor = 0u;
	uint32_t effectIndexCursor = 0u;
	qboolean ok = qfalse;
	if ( !core || !coreReceipt || !frontend || !submission || !plan || !info
			|| !outReceipt || !info->generation || !info->programName
			|| !info->outputWidth || !info->outputHeight
			|| !RalOpenGl_CoreMatchesReceipt( core, coreReceipt )
			|| !RalOpenGl_FrontendPlanReceiptExact( plan, plan )
			|| !Ral_AtmospherePlanReceiptExact( &info->atmosphere,
				&info->atmosphere )
			|| !Ral_DisplayVisibilityPlanValid( &info->displayVisibility )
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
	if ( !RenderSubmission_EffectSnapshots( frontend, &polygons,
				&polygonCommandCount, &polyVertices, &polyVertexCount,
				&lights, &lightCount )
			|| plan->loweredPolygonCount != submission->polygonCount
			|| plan->loweredLightCount != lightCount ) return qfalse;
	if ( submission->worldLoaded && submission->sceneRendered ) {
		if ( !RenderSubmission_WorldSnapshot( frontend, &world )
				|| plan->loweredWorldVertexCount != world.vertexCount
				|| plan->loweredWorldIndexCount != world.indexCount
				|| plan->loweredWorldBatchCount != world.batchCount ) return qfalse;
	} else if ( plan->loweredWorldVertexCount || plan->loweredWorldIndexCount
			|| plan->loweredWorldBatchCount ) return qfalse;
	entities = RenderSubmission_EntityCommands( frontend, &entityCount );
	ui = RenderSubmission_UiPrimitives( frontend, &uiCount );
	if ( entityCount != submission->entityCount || uiCount != submission->uiPrimitiveCount
			|| ( entityCount && !entities ) || ( uiCount && !ui ) ) return qfalse;
	if ( lightCount > ( UINT32_MAX - polyVertexCount ) / 4u ) return qfalse;
	effectVertexCount = polyVertexCount + lightCount * 4u;
	effectIndexCount = plan->loweredEffectIndexCount;
	if ( effectVertexCount ) {
		effectVertices = calloc( effectVertexCount, sizeof( *effectVertices ) );
		effectIndices = calloc( effectIndexCount, sizeof( *effectIndices ) );
		if ( !effectVertices || !effectIndices ) goto cleanup;
	}
	gl.CreateBuffers( 1, &uniformBuffer );
	if ( !uniformBuffer ) goto cleanup;
	gl.NamedBufferStorage( uniformBuffer, (glSizePtr_t)sizeof( uniforms ), NULL,
		GL_DYNAMIC_STORAGE_BIT_VALUE );
	gl.BindBufferBase( GL_UNIFORM_BUFFER_VALUE, 0u, uniformBuffer );
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
	if ( effectIndexCursor != effectIndexCount
			|| effectVertexCursor != effectVertexCount ) goto cleanup;
	for ( uint32_t i = 0u; i < frontend->materialCount; ++i ) {
		const renderMaterialSnapshot_t *material = &frontend->materials[i].snapshot;
		gl.CreateTextures( GL_TEXTURE_2D_VALUE, 1, &textures[i] );
		gl.TextureStorage2D( textures[i], 1,
			material->srgb ? GL_SRGB8_ALPHA8_VALUE : GL_RGBA8_VALUE,
			(glSize_t)material->width, (glSize_t)material->height );
		gl.TextureSubImage2D( textures[i], 0, 0, 0, (glSize_t)material->width,
			(glSize_t)material->height, GL_RGBA_VALUE, GL_UNSIGNED_BYTE_VALUE,
			material->rgba8 );
		gl.TextureParameteri( textures[i], GL_TEXTURE_MIN_FILTER_VALUE, GL_LINEAR_VALUE );
		gl.TextureParameteri( textures[i], GL_TEXTURE_MAG_FILTER_VALUE, GL_LINEAR_VALUE );
		gl.TextureParameteri( textures[i], GL_TEXTURE_WRAP_S_VALUE,
			material->clampToEdge ? GL_CLAMP_TO_EDGE_VALUE : GL_REPEAT_VALUE );
		gl.TextureParameteri( textures[i], GL_TEXTURE_WRAP_T_VALUE,
			material->clampToEdge ? GL_CLAMP_TO_EDGE_VALUE : GL_REPEAT_VALUE );
	}
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
	gl.Viewport( 0, 0, (glSize_t)info->outputWidth,
		(glSize_t)info->outputHeight );
	gl.ClearColor( 0.015f, 0.02f, 0.03f, 1.0f );
	gl.Clear( GL_COLOR_BUFFER_BIT_VALUE | GL_DEPTH_BUFFER_BIT_VALUE );
	gl.Enable( GL_DEPTH_TEST_VALUE );
	gl.DepthFunc( GL_LEQUAL_VALUE );
	if ( ( world.batchCount || effectVertexCount || entityCount )
			&& ( !ApplyWorldView( &uniforms, &world )
				|| !ApplyAtmosphere( &uniforms, frontend, info ) ) ) goto cleanup;
	if ( world.batchCount ) gl.BindVertexArray( vaos[0] );
	for ( uint32_t i = 0u; i < world.batchCount; ++i ) {
		const renderWorldBatch_t *batch = &world.batches[i];
		float emissiveRadiance[4];
		ralLightingSurfaceBindingReceipt_t surfaceLighting;
		ralLightingSurfaceBindingRequest_t bindingRequest;
		const ralLightingRuntimePlan_t *lightingPlan = info->directionalLighting
			? &info->directionalLighting->plan : NULL;
		const ralLightingSurfaceBindingReceipt_t *binding = NULL;
		if ( !MaterialEmission( frontend, batch->baseMaterial,
			emissiveRadiance ) ) goto cleanup;
		gl.BindTextureUnit( 0u, MaterialTexture( frontend, textures,
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
					&surfaceLighting ) ) goto cleanup;
			binding = &surfaceLighting;
		}
		if ( binding && binding->directionalStaticBound ) {
			gl.BindTextureUnit( 1u, 0u );
			gl.BindTextureUnit( 2u, info->directionalLighting->textureNames[
				binding->radiancePlane] );
			gl.BindTextureUnit( 3u, info->directionalLighting->textureNames[
				binding->directionPlane] );
			gl.BindTextureUnit( 4u,
				binding->visibilityPlane == RAL_LIGHTING_SURFACE_BINDING_NO_PLANE
					? 0u : info->directionalLighting->textureNames[
						binding->visibilityPlane] );
		} else {
			gl.BindTextureUnit( 1u, MaterialTexture( frontend, textures,
				batch->lightmapMaterial ) );
			gl.BindTextureUnit( 2u, 0u );
			gl.BindTextureUnit( 3u, 0u );
			gl.BindTextureUnit( 4u, 0u );
		}
		ApplyRaster( &gl, uniformBuffer, &uniforms, batch->alphaMode,
			batch->alphaCutoff, batch->depthWrite,
			binding && binding->legacyLightmapBound, NULL, binding, lightingPlan,
			emissiveRadiance );
		if ( binding ) {
			uint64_t identity = binding->surfaceId
				^ ( (uint64_t)binding->diffuseAuthority << 56u )
				^ ( (uint64_t)binding->arrayLayer << 24u );
			if ( binding->legacyLightmapBound ) receipt.legacyLightmapDrawCount++;
			else receipt.directionalStaticDrawCount++;
			receipt.surfaceLightingBindingDigest ^= identity
				+ UINT64_C( 0x9e3779b97f4a7c15 )
				+ ( receipt.surfaceLightingBindingDigest << 6u )
				+ ( receipt.surfaceLightingBindingDigest >> 2u );
			if ( !receipt.surfaceLightingBindingDigest )
				receipt.surfaceLightingBindingDigest = 1u;
		}
		gl.DrawElements( GL_TRIANGLES_VALUE, (glSize_t)batch->indexCount,
			GL_UNSIGNED_INT_VALUE, (const void *)(uintptr_t)( batch->firstIndex
				* sizeof( uint32_t ) ) );
	}
	if ( effectVertexCount ) {
		uint32_t firstIndex = 0u;
		gl.BindVertexArray( vaos[1] );
		for ( uint32_t i = 0u; i < polygonCommandCount; ++i ) {
			uint32_t count = ( polygons[i].verticesPerPolygon - 2u ) * 3u
				* polygons[i].polygonCount;
			gl.BindTextureUnit( 0u, MaterialTexture( frontend, textures,
				polygons[i].material ) );
			ApplyRaster( &gl, uniformBuffer, &uniforms, RENDER_ALPHA_BLEND, 0.5f,
				qfalse, qfalse, NULL, NULL, NULL, NULL );
			gl.DrawElements( GL_TRIANGLES_VALUE, (glSize_t)count,
				GL_UNSIGNED_INT_VALUE, (const void *)(uintptr_t)( firstIndex
					* sizeof( uint32_t ) ) );
			firstIndex += count;
		}
		for ( uint32_t i = 0u; i < lightCount; ++i ) {
			gl.BindTextureUnit( 0u, 0u );
			ApplyRaster( &gl, uniformBuffer, &uniforms, RENDER_ALPHA_BLEND, 0.5f,
				qfalse, qfalse, NULL, NULL, NULL, NULL );
			gl.DrawElements( GL_TRIANGLES_VALUE, 6, GL_UNSIGNED_INT_VALUE,
				(const void *)(uintptr_t)( firstIndex * sizeof( uint32_t ) ) );
			firstIndex += 6u;
		}
	}
	for ( uint32_t i = 0u; i < entityCount; ++i ) {
		uint32_t drawsBefore = receipt.entityDrawCount;
		if ( entities[i].entity.reType == RT_MODEL
				&& entities[i].entity.hModel > 0 ) {
			if ( !DrawModelEntity( &gl, uniformBuffer, &uniforms, frontend, textures,
				&entities[i],
				&receipt.entityIndexCount, &receipt.entityDrawCount ) ) goto cleanup;
			receipt.modelEntityCount++;
		} else {
			if ( !DrawPrimitiveEntity( &gl, uniformBuffer, &uniforms, frontend, textures,
				&entities[i],
				&receipt.entityIndexCount, &receipt.entityDrawCount ) ) goto cleanup;
			receipt.primitiveEntityCount++;
		}
		if ( entities[i].hasTemporal ) receipt.temporalEntityCount++;
		if ( entities[i].hasLocalIrradiance ) {
			receipt.localIrradianceEntityCount++;
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
		gl.DrawArraysInstanced( GL_TRIANGLES_VALUE, 0, 6,
			(glSize_t)info->weatherParticleCount );
		gl.DeleteVertexArrays( 1, &weatherVao );
		receipt.weatherDrawCount = 1u;
		receipt.weatherInstanceCount = info->weatherParticleCount;
		gl.UseProgram( info->programName );
		gl.BindBufferBase( GL_UNIFORM_BUFFER_VALUE, 0u, uniformBuffer );
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
				|| !DrawUiPrimitive( &gl, uniformBuffer, &uniforms, frontend, textures,
					&ui[i] ) ) goto cleanup;
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
	receipt.polygonCount = plan->loweredPolygonCount;
	receipt.lightCount = plan->loweredLightCount;
	receipt.effectIndexCount = effectIndexCount;
	receipt.effectDrawCount = polygonCommandCount + lightCount;
	receipt.nativeDrawCount = receipt.worldDrawCount + receipt.effectDrawCount
		+ receipt.entityDrawCount + receipt.uiDrawCount + receipt.weatherDrawCount;
	receipt.ready = qtrue;
	if ( receipt.uploadedMaterialCount != plan->loweredMaterialCount
			|| receipt.worldDrawCount != plan->loweredWorldBatchCount
			|| receipt.effectIndexCount != plan->loweredEffectIndexCount
			|| receipt.effectDrawCount != plan->loweredEffectBatchCount
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
	if ( uniformBuffer ) gl.DeleteBuffers( 1, &uniformBuffer );
	if ( vaos[0] || vaos[1] ) gl.DeleteVertexArrays( 2, vaos );
	if ( buffers[0] || buffers[1] || buffers[2] || buffers[3] )
		gl.DeleteBuffers( 4, buffers );
	if ( frontend ) gl.DeleteTextures( (glSize_t)frontend->materialCount, textures );
	free( effectIndices );
	free( effectVertices );
	if ( ok ) *outReceipt = receipt;
	return ok;
}
