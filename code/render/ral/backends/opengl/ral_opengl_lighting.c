// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "ral_opengl_lighting.h"
#include "ral_opengl_internal.h"

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
typedef void ( RAL_GL_APIENTRY *createTexturesFn )( glEnum_t, glSize_t, glName_t * );
typedef void ( RAL_GL_APIENTRY *textureStorage2DFn )( glName_t, glSize_t, glEnum_t, glSize_t, glSize_t );
typedef void ( RAL_GL_APIENTRY *textureStorage3DFn )( glName_t, glSize_t, glEnum_t, glSize_t, glSize_t, glSize_t );
typedef void ( RAL_GL_APIENTRY *textureSubImage2DFn )( glName_t, glInt_t, glInt_t, glInt_t, glSize_t, glSize_t,
	glEnum_t, glEnum_t, const void * );
typedef void ( RAL_GL_APIENTRY *textureSubImage3DFn )( glName_t, glInt_t, glInt_t, glInt_t, glInt_t, glSize_t,
	glSize_t, glSize_t, glEnum_t, glEnum_t, const void * );
typedef void ( RAL_GL_APIENTRY *textureParameteriFn )( glName_t, glEnum_t, glInt_t );
typedef void ( RAL_GL_APIENTRY *pixelStoreiFn )( glEnum_t, glInt_t );
typedef void ( RAL_GL_APIENTRY *deleteTexturesFn )( glSize_t, const glName_t * );
typedef glEnum_t ( RAL_GL_APIENTRY *getErrorFn )( void );

enum {
	GL_TEXTURE_2D_VALUE = 0x0de1,
	GL_TEXTURE_2D_ARRAY_VALUE = 0x8c1a,
	GL_RGB9_E5_VALUE = 0x8c3d,
	GL_RG8_VALUE = 0x822b,
	GL_R8_VALUE = 0x8229,
	GL_RG16_SNORM_VALUE = 0x8f99,
	GL_RGBA16F_VALUE = 0x881a,
	GL_RED_VALUE = 0x1903,
	GL_RG_VALUE = 0x8227,
	GL_RGB_VALUE = 0x1907,
	GL_RGBA_VALUE = 0x1908,
	GL_UNSIGNED_BYTE_VALUE = 0x1401,
	GL_SHORT_VALUE = 0x1402,
	GL_HALF_FLOAT_VALUE = 0x140b,
	GL_UNSIGNED_INT_5_9_9_9_REV_VALUE = 0x8c3e,
	GL_UNPACK_ALIGNMENT_VALUE = 0x0cf5,
	GL_TEXTURE_MIN_FILTER_VALUE = 0x2801,
	GL_TEXTURE_MAG_FILTER_VALUE = 0x2800,
	GL_TEXTURE_WRAP_S_VALUE = 0x2802,
	GL_TEXTURE_WRAP_T_VALUE = 0x2803,
	GL_LINEAR_VALUE = 0x2601,
	GL_CLAMP_TO_EDGE_VALUE = 0x812f
};

typedef struct {
	createTexturesFn CreateTextures;
	textureStorage2DFn TextureStorage2D;
	textureStorage3DFn TextureStorage3D;
	textureSubImage2DFn TextureSubImage2D;
	textureSubImage3DFn TextureSubImage3D;
	textureParameteriFn TextureParameteri;
	pixelStoreiFn PixelStorei;
	deleteTexturesFn DeleteTextures;
	getErrorFn GetError;
} lightingDispatch_t;

struct ralOpenGlLighting_s {
	ralOpenGlLightingReceipt_t receipt;
};

static qboolean LoadDispatch( const ralOpenGlCore_t *core, lightingDispatch_t *gl )
{
#define LOAD_GL( member, symbol, type )                                      \
	do {                                                                       \
		ralOpenGlProc_t proc = RalOpenGl_CoreResolve( core, symbol );            \
		if ( !proc )                                                             \
			return qfalse;                                                        \
		gl->member = (type)proc;                                                 \
	} while ( 0 )
	memset( gl, 0, sizeof( *gl ) );
	LOAD_GL( CreateTextures, "glCreateTextures", createTexturesFn );
	LOAD_GL( TextureStorage2D, "glTextureStorage2D", textureStorage2DFn );
	LOAD_GL( TextureStorage3D, "glTextureStorage3D", textureStorage3DFn );
	LOAD_GL( TextureSubImage2D, "glTextureSubImage2D", textureSubImage2DFn );
	LOAD_GL( TextureSubImage3D, "glTextureSubImage3D", textureSubImage3DFn );
	LOAD_GL( TextureParameteri, "glTextureParameteri", textureParameteriFn );
	LOAD_GL( PixelStorei, "glPixelStorei", pixelStoreiFn );
	LOAD_GL( DeleteTextures, "glDeleteTextures", deleteTexturesFn );
	LOAD_GL( GetError, "glGetError", getErrorFn );
#undef LOAD_GL
	return qtrue;
}

static qboolean Format( ralFormat_t format, glEnum_t *internalFormat,
	glEnum_t *externalFormat, glEnum_t *type )
{
	if ( !internalFormat || !externalFormat || !type )
		return qfalse;
	switch ( format ) {
	case RAL_FORMAT_E5B9G9R9_UFLOAT:
		*internalFormat = GL_RGB9_E5_VALUE;
		*externalFormat = GL_RGB_VALUE;
		*type = GL_UNSIGNED_INT_5_9_9_9_REV_VALUE;
		return qtrue;
	case RAL_FORMAT_R8G8_UNORM:
		*internalFormat = GL_RG8_VALUE;
		*externalFormat = GL_RG_VALUE;
		*type = GL_UNSIGNED_BYTE_VALUE;
		return qtrue;
	case RAL_FORMAT_R8_UNORM:
		*internalFormat = GL_R8_VALUE;
		*externalFormat = GL_RED_VALUE;
		*type = GL_UNSIGNED_BYTE_VALUE;
		return qtrue;
	case RAL_FORMAT_R16G16_SNORM:
		*internalFormat = GL_RG16_SNORM_VALUE;
		*externalFormat = GL_RG_VALUE;
		*type = GL_SHORT_VALUE;
		return qtrue;
	case RAL_FORMAT_R16G16B16A16_SFLOAT:
		*internalFormat = GL_RGBA16F_VALUE;
		*externalFormat = GL_RGBA_VALUE;
		*type = GL_HALF_FLOAT_VALUE;
		return qtrue;
	default:
		return qfalse;
	}
}

static qboolean ReceiptValid( const ralOpenGlLightingReceipt_t *receipt )
{
	uint32_t plane;
	if ( !receipt || receipt->schemaVersion != RAL_OPENGL_LIGHTING_SCHEMA_VERSION || !receipt->coreGeneration ||
		 receipt->plan.backendType != RAL_BACKEND_OPENGL || !Ral_LightingRuntimePlanValid( &receipt->plan ) ||
		 !receipt->uploadHash || receipt->ready != qtrue )
		return qfalse;
	for ( plane = 0u; plane < receipt->plan.planeCount; ++plane )
		if ( !receipt->textureNames[plane] )
			return qfalse;
	for ( ; plane < RAL_LIGHTING_RUNTIME_MAX_PLANES; ++plane )
		if ( receipt->textureNames[plane] )
			return qfalse;
	return qtrue;
}

qboolean RalOpenGl_LightingReceiptExact( const ralOpenGlLightingReceipt_t *a,
	const ralOpenGlLightingReceipt_t *b )
{
	return ReceiptValid( a ) && ReceiptValid( b ) && !memcmp( a, b, sizeof( *a ) ) ? qtrue : qfalse;
}

qboolean RalOpenGl_LightingUpload( ralOpenGlCore_t *core,
	const ralOpenGlCoreReceipt_t *coreReceipt, const void *artifactMemory,
	uint64_t artifactByteLength, const ralLightingRuntimePlan_t *plan,
	ralOpenGlLighting_t **outLighting, ralOpenGlLightingReceipt_t *outReceipt )
{
	const uint8_t *artifactBytes = (const uint8_t *)artifactMemory;
	ralLightingArtifactReceipt_t artifact;
	ralLightingPayloadView_t views[RAL_LIGHTING_ARTIFACT_MAX_PAYLOADS];
	ralStaticLightingCapabilities_t capabilities = { qtrue, qtrue, qtrue, qtrue };
	ralLightingRuntimePlan_t exactPlan;
	lightingDispatch_t gl;
	ralOpenGlLighting_t *lighting;
	uint32_t plane;
	if ( !core || !coreReceipt || !RalOpenGl_CoreMatchesReceipt( core, coreReceipt ) || !artifactBytes ||
		 !artifactByteLength || !plan || plan->backendType != RAL_BACKEND_OPENGL || !outLighting || !outReceipt ||
		 !Ral_LightingArtifactRead( artifactBytes, artifactByteLength, &artifact, views ) ||
		 !Ral_LightingRuntimePlanBuild( RAL_BACKEND_OPENGL, plan->frameGeneration, artifactBytes, artifactByteLength,
										 &artifact, &capabilities, &exactPlan ) ||
		 !Ral_LightingRuntimePlanExact( plan, &exactPlan ) || !LoadDispatch( core, &gl ) || gl.GetError() != 0u )
		return qfalse;
	lighting = (ralOpenGlLighting_t *)calloc( 1u, sizeof( *lighting ) );
	if ( !lighting )
		return qfalse;
	lighting->receipt.schemaVersion = RAL_OPENGL_LIGHTING_SCHEMA_VERSION;
	lighting->receipt.coreGeneration = coreReceipt->generation;
	lighting->receipt.plan = exactPlan;
	gl.PixelStorei( GL_UNPACK_ALIGNMENT_VALUE, 1 );
	for ( plane = 0u; plane < exactPlan.planeCount; ++plane ) {
		const ralLightingRuntimePlanePlan_t *planePlan = &exactPlan.planes[plane];
		glEnum_t internalFormat, externalFormat, type;
		glEnum_t target = planePlan->texture.type == RAL_TEXTURE_2D_ARRAY ? GL_TEXTURE_2D_ARRAY_VALUE : GL_TEXTURE_2D_VALUE;
		if ( !Format( planePlan->texture.format, &internalFormat, &externalFormat, &type ) )
			goto fail;
		gl.CreateTextures( target, 1, &lighting->receipt.textureNames[plane] );
		if ( planePlan->texture.type == RAL_TEXTURE_2D_ARRAY ) {
			gl.TextureStorage3D( lighting->receipt.textureNames[plane], 1, internalFormat,
				(glSize_t)planePlan->texture.width, (glSize_t)planePlan->texture.height,
				(glSize_t)planePlan->texture.depthOrArrayLayers );
			gl.TextureSubImage3D( lighting->receipt.textureNames[plane], 0, 0, 0, 0,
				(glSize_t)planePlan->texture.width, (glSize_t)planePlan->texture.height,
				(glSize_t)planePlan->texture.depthOrArrayLayers, externalFormat, type,
				artifactBytes + planePlan->artifactOffset );
		} else {
			gl.TextureStorage2D( lighting->receipt.textureNames[plane], 1, internalFormat,
				(glSize_t)planePlan->texture.width, (glSize_t)planePlan->texture.height );
			gl.TextureSubImage2D( lighting->receipt.textureNames[plane], 0, 0, 0,
				(glSize_t)planePlan->texture.width, (glSize_t)planePlan->texture.height,
				externalFormat, type, artifactBytes + planePlan->artifactOffset );
		}
		gl.TextureParameteri( lighting->receipt.textureNames[plane], GL_TEXTURE_MIN_FILTER_VALUE, GL_LINEAR_VALUE );
		gl.TextureParameteri( lighting->receipt.textureNames[plane], GL_TEXTURE_MAG_FILTER_VALUE, GL_LINEAR_VALUE );
		gl.TextureParameteri( lighting->receipt.textureNames[plane], GL_TEXTURE_WRAP_S_VALUE, GL_CLAMP_TO_EDGE_VALUE );
		gl.TextureParameteri( lighting->receipt.textureNames[plane], GL_TEXTURE_WRAP_T_VALUE, GL_CLAMP_TO_EDGE_VALUE );
		if ( gl.GetError() != 0u )
			goto fail;
	}
	lighting->receipt.uploadHash = exactPlan.manifestHash;
	for ( plane = 0u; plane < exactPlan.planeCount; ++plane )
		lighting->receipt.uploadHash ^= exactPlan.planes[plane].payloadHash +
			( UINT64_C( 0x9e3779b97f4a7c15 ) << ( plane & 1u ) );
	if ( !lighting->receipt.uploadHash )
		lighting->receipt.uploadHash = 1u;
	lighting->receipt.ready = qtrue;
	if ( !ReceiptValid( &lighting->receipt ) )
		goto fail;
	*outLighting = lighting;
	*outReceipt = lighting->receipt;
	return qtrue;
fail:
	for ( plane = 0u; plane < exactPlan.planeCount; ++plane )
		if ( lighting->receipt.textureNames[plane] )
			gl.DeleteTextures( 1, &lighting->receipt.textureNames[plane] );
	free( lighting );
	return qfalse;
}

qboolean RalOpenGl_LightingDestroy( ralOpenGlCore_t *core,
	const ralOpenGlCoreReceipt_t *coreReceipt, ralOpenGlLighting_t *lighting,
	const ralOpenGlLightingReceipt_t *authority )
{
	lightingDispatch_t gl;
	if ( !core || !coreReceipt || !lighting || !authority || !RalOpenGl_CoreMatchesReceipt( core, coreReceipt ) ||
		 !RalOpenGl_LightingReceiptExact( &lighting->receipt, authority ) || !LoadDispatch( core, &gl ) )
		return qfalse;
	gl.DeleteTextures( (glSize_t)lighting->receipt.plan.planeCount, lighting->receipt.textureNames );
	memset( lighting, 0, sizeof( *lighting ) );
	free( lighting );
	return gl.GetError() == 0u ? qtrue : qfalse;
}
