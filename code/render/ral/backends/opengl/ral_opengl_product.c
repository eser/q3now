// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_opengl_product.h"
#include "ral_opengl_internal.h"

#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "ral_opengl_product_shader_catalog.inc"

#if defined( _WIN32 )
#define RAL_GL_APIENTRY __stdcall
#else
#define RAL_GL_APIENTRY
#endif

typedef unsigned int glName_t;
typedef unsigned int glEnum_t;
typedef int			 glInt_t;
typedef int			 glSize_t;
typedef long		 glSizePtr_t;
typedef long		 glIntPtr_t;
typedef char		 glChar_t;
typedef glName_t( RAL_GL_APIENTRY *createShaderFn )( glEnum_t );
typedef void( RAL_GL_APIENTRY *shaderSourceFn )( glName_t, glSize_t, const glChar_t *const *, const glInt_t * );
typedef void( RAL_GL_APIENTRY *compileShaderFn )( glName_t );
typedef void( RAL_GL_APIENTRY *getShaderivFn )( glName_t, glEnum_t, glInt_t * );
typedef void( RAL_GL_APIENTRY *deleteShaderFn )( glName_t );
typedef glName_t( RAL_GL_APIENTRY *createProgramFn )( void );
typedef void( RAL_GL_APIENTRY *attachShaderFn )( glName_t, glName_t );
typedef void( RAL_GL_APIENTRY *linkProgramFn )( glName_t );
typedef void( RAL_GL_APIENTRY *getProgramivFn )( glName_t, glEnum_t, glInt_t * );
typedef void( RAL_GL_APIENTRY *deleteProgramFn )( glName_t );
typedef void( RAL_GL_APIENTRY *useProgramFn )( glName_t );
typedef void( RAL_GL_APIENTRY *createBuffersFn )( glSize_t, glName_t * );
typedef void( RAL_GL_APIENTRY *namedBufferStorageFn )( glName_t, glSizePtr_t, const void *, unsigned int );
typedef void( RAL_GL_APIENTRY *namedBufferSubDataFn )( glName_t, glIntPtr_t, glSizePtr_t, const void * );
typedef void( RAL_GL_APIENTRY *bindBufferBaseFn )( glEnum_t, unsigned int, glName_t );
typedef void( RAL_GL_APIENTRY *deleteBuffersFn )( glSize_t, const glName_t * );
typedef void( RAL_GL_APIENTRY *dispatchComputeFn )( unsigned int, unsigned int, unsigned int );
typedef void( RAL_GL_APIENTRY *memoryBarrierFn )( unsigned int );
typedef void( RAL_GL_APIENTRY *pixelStoreiFn )( glEnum_t, glInt_t );
typedef void( RAL_GL_APIENTRY *readPixelsFn )( glInt_t, glInt_t, glSize_t, glSize_t, glEnum_t, glEnum_t, void * );
typedef glEnum_t( RAL_GL_APIENTRY *getErrorFn )( void );

enum {
	GL_NO_ERROR_VALUE					= 0,
	GL_VERTEX_SHADER_VALUE				= 0x8b31,
	GL_FRAGMENT_SHADER_VALUE			= 0x8b30,
	GL_COMPUTE_SHADER_VALUE				= 0x91b9,
	GL_COMPILE_STATUS_VALUE				= 0x8b81,
	GL_LINK_STATUS_VALUE				= 0x8b82,
	GL_UNIFORM_BUFFER_VALUE				= 0x8a11,
	GL_SHADER_STORAGE_BUFFER_VALUE		= 0x90d2,
	GL_DYNAMIC_STORAGE_BIT_VALUE		= 0x0100,
	GL_UNIFORM_BARRIER_BIT_VALUE		= 0x00000004,
	GL_SHADER_STORAGE_BARRIER_BIT_VALUE = 0x00002000,
	GL_PACK_ALIGNMENT_VALUE				= 0x0d05,
	GL_RGB_VALUE						= 0x1907,
	GL_UNSIGNED_BYTE_VALUE				= 0x1401
};

typedef struct {
	createShaderFn		 CreateShader;
	shaderSourceFn		 ShaderSource;
	compileShaderFn		 CompileShader;
	getShaderivFn		 GetShaderiv;
	deleteShaderFn		 DeleteShader;
	createProgramFn		 CreateProgram;
	attachShaderFn		 AttachShader;
	linkProgramFn		 LinkProgram;
	getProgramivFn		 GetProgramiv;
	deleteProgramFn		 DeleteProgram;
	useProgramFn		 UseProgram;
	createBuffersFn		 CreateBuffers;
	namedBufferStorageFn NamedBufferStorage;
	namedBufferSubDataFn NamedBufferSubData;
	bindBufferBaseFn	 BindBufferBase;
	deleteBuffersFn		 DeleteBuffers;
	dispatchComputeFn	 DispatchCompute;
	memoryBarrierFn		 MemoryBarrier;
	pixelStoreiFn		 PixelStorei;
	readPixelsFn		 ReadPixels;
	getErrorFn			 GetError;
} productDispatch_t;

#define RAL_OPENGL_ATMOSPHERE_FROXEL_CAPACITY 262144u
#define RAL_OPENGL_ATMOSPHERE_BUFFER_COUNT	  11u
#define RAL_OPENGL_WEATHER_PARTICLE_CAPACITY  8192u

enum {
	RAL_OPENGL_ATMOSPHERE_PARAMS = 0,
	RAL_OPENGL_ATMOSPHERE_VOLUMES,
	RAL_OPENGL_ATMOSPHERE_MEDIA,
	RAL_OPENGL_ATMOSPHERE_TILE_LIGHTS,
	RAL_OPENGL_ATMOSPHERE_LIGHTS,
	RAL_OPENGL_ATMOSPHERE_LIT,
	RAL_OPENGL_ATMOSPHERE_CLOUD,
	RAL_OPENGL_ATMOSPHERE_INTEGRATED,
	RAL_OPENGL_ATMOSPHERE_HISTORY_A,
	RAL_OPENGL_ATMOSPHERE_HISTORY_B,
	RAL_OPENGL_ATMOSPHERE_FALLBACK
};

typedef struct {
	float originRadius[4];
	float extentShape[4];
	float albedoAnisotropy[4];
	float emissiveIntensity[4];
	float media[4];
} productAtmosphereVolume_t;

typedef struct {
	float	 invViewProjection[16];
	float	 eyeNear[4];
	float	 farGlobalDensity[4];
	float	 globalAlbedoAnisotropy[4];
	uint32_t gridVolumeCount[4];
	float	 timelineSeed[4];
} productAtmosphereInjectParams_t;

typedef struct {
	float	 invViewProjection[16];
	float	 eyeNear[4];
	float	 farPad[4];
	uint32_t gridLightCount[4];
	float	 sunDirectionIntensity[4];
	float	 sunColorCloudShadow[4];
	float	 moonDirectionIntensity[4];
	float	 moonColorLightning[4];
	float	 ambientCloud[4];
} productAtmosphereLightParams_t;

typedef struct {
	float	 invViewProjection[16];
	float	 eyeNear[4];
	float	 farTimeCoverageShadow[4];
	float	 windDensity[4];
	float	 layerErosion[4];
	float	 sunDirectionIntensity[4];
	float	 sunColorLightning[4];
	float	 ambientColor[4];
	uint32_t grid[4];
} productAtmosphereCloudParams_t;

typedef struct {
	uint32_t gridHistory[4];
	float	 depthRangeTemporal[4];
} productAtmosphereIntegrateParams_t;

typedef char productAtmosphereInjectParamsMustMatchStd140[sizeof( productAtmosphereInjectParams_t ) == 144u ? 1 : -1];
typedef char productAtmosphereLightParamsMustMatchStd140[sizeof( productAtmosphereLightParams_t ) == 192u ? 1 : -1];
typedef char productAtmosphereCloudParamsMustMatchStd140[sizeof( productAtmosphereCloudParams_t ) == 192u ? 1 : -1];
typedef char
	productAtmosphereIntegrateParamsMustMatchStd140[sizeof( productAtmosphereIntegrateParams_t ) == 32u ? 1 : -1];

typedef struct {
	float								mvp[16];
	float								viewLeft[4];
	float								viewUp[4];
	float								eyeWorld[4];
	float								dt;
	float								time;
	uint32_t							poolSize;
	uint32_t							pingPongRead;
	float								boundsMin[4];
	float								boundsMax[4];
	float								worldMins[2];
	float								worldMaxs[2];
	float								invGridStep[2];
	uint32_t							gridSize;
	uint32_t							atmType;
	float								distance;
	float								computePad[3];
	float								windGust[4];
	float								precipitation[4];
	float								dustAsh;
	float								indoorExposure;
	uint32_t							climateSeed;
	uint32_t							climatePad;
	float								climate[4];
	float								surfaceClimate[4];
	float								sun[4];
	float								moon[4];
	float								ambientCloud[4];
	float								cloudMedia[4];
	float								effectMeta[4];
	renderAtmosphereEffectGpuWorkload_t effectWorkloads[RENDER_SUBMISSION_MAX_ATMOSPHERE_EFFECT_WORKLOADS];
	float								renderParams[4];
} productWeatherFrame_t;

typedef char productWeatherFrameMustMatchStd140[sizeof( productWeatherFrame_t ) == 1152u ? 1 : -1];

struct ralOpenGlProduct_s {
	ralOpenGlCore_t		  *core;
	ralOpenGlCoreReceipt_t coreReceipt;
	productDispatch_t	   gl;
	uint64_t			   generation;
	uint64_t			   lastFrameGeneration;
	uint32_t			   outputWidth;
	uint32_t			   outputHeight;
	glName_t			   program;
	glName_t			   atmospherePrograms[4];
	glName_t			   atmosphereBuffers[RAL_OPENGL_ATMOSPHERE_BUFFER_COUNT];
	glName_t			   weatherPrograms[2];
	glName_t			   weatherBuffers[3];
	qboolean			   atmosphereBuffersReady;
	qboolean			   atmosphereHistoryValid;
	qboolean			   atmosphereHistoryParity;
	qboolean			   weatherBuffersReady;
	qboolean			   weatherParity;
	float				   weatherTimeline;
	ralOpenGlLightingReceipt_t directionalLighting;
	qboolean                 hasDirectionalLighting;
	ralDisplayVisibilityPlan_t displayVisibility;
};

static qboolean LoadDispatch( const ralOpenGlCore_t *core, productDispatch_t *gl )
{
#define LOAD_GL( member, symbol, type )                               \
	do {                                                              \
		ralOpenGlProc_t proc = RalOpenGl_CoreResolve( core, symbol ); \
		if ( !proc )                                                  \
			return qfalse;                                            \
		gl->member = (type)proc;                                      \
	} while ( 0 )
	memset( gl, 0, sizeof( *gl ) );
	LOAD_GL( CreateShader, "glCreateShader", createShaderFn );
	LOAD_GL( ShaderSource, "glShaderSource", shaderSourceFn );
	LOAD_GL( CompileShader, "glCompileShader", compileShaderFn );
	LOAD_GL( GetShaderiv, "glGetShaderiv", getShaderivFn );
	LOAD_GL( DeleteShader, "glDeleteShader", deleteShaderFn );
	LOAD_GL( CreateProgram, "glCreateProgram", createProgramFn );
	LOAD_GL( AttachShader, "glAttachShader", attachShaderFn );
	LOAD_GL( LinkProgram, "glLinkProgram", linkProgramFn );
	LOAD_GL( GetProgramiv, "glGetProgramiv", getProgramivFn );
	LOAD_GL( DeleteProgram, "glDeleteProgram", deleteProgramFn );
	LOAD_GL( UseProgram, "glUseProgram", useProgramFn );
	LOAD_GL( CreateBuffers, "glCreateBuffers", createBuffersFn );
	LOAD_GL( NamedBufferStorage, "glNamedBufferStorage", namedBufferStorageFn );
	LOAD_GL( NamedBufferSubData, "glNamedBufferSubData", namedBufferSubDataFn );
	LOAD_GL( BindBufferBase, "glBindBufferBase", bindBufferBaseFn );
	LOAD_GL( DeleteBuffers, "glDeleteBuffers", deleteBuffersFn );
	LOAD_GL( DispatchCompute, "glDispatchCompute", dispatchComputeFn );
	LOAD_GL( MemoryBarrier, "glMemoryBarrier", memoryBarrierFn );
	LOAD_GL( PixelStorei, "glPixelStorei", pixelStoreiFn );
	LOAD_GL( ReadPixels, "glReadPixels", readPixelsFn );
	LOAD_GL( GetError, "glGetError", getErrorFn );
#undef LOAD_GL
	return qtrue;
}

static glName_t Compile( const productDispatch_t *gl, glEnum_t stage, const unsigned char *source,
						 unsigned int sourceByteCount )
{
	glName_t		shader	 = gl->CreateShader( stage );
	glInt_t			compiled = 0;
	glInt_t			sourceLength;
	const glChar_t *sourceText = (const glChar_t *)source;
	if ( !shader || !source || !sourceByteCount || sourceByteCount > INT_MAX )
		return 0u;
	sourceLength = (glInt_t)sourceByteCount;
	gl->ShaderSource( shader, 1, &sourceText, &sourceLength );
	gl->CompileShader( shader );
	gl->GetShaderiv( shader, GL_COMPILE_STATUS_VALUE, &compiled );
	if ( !compiled || gl->GetError() != GL_NO_ERROR_VALUE ) {
		gl->DeleteShader( shader );
		return 0u;
	}
	return shader;
}

static glName_t LinkSingleStage( const productDispatch_t *gl, glName_t shader )
{
	glName_t program;
	glInt_t	 linked = 0;
	if ( !gl || !shader )
		return 0u;
	program = gl->CreateProgram();
	if ( !program )
		return 0u;
	gl->AttachShader( program, shader );
	gl->LinkProgram( program );
	gl->GetProgramiv( program, GL_LINK_STATUS_VALUE, &linked );
	if ( !linked || gl->GetError() != GL_NO_ERROR_VALUE ) {
		gl->DeleteProgram( program );
		return 0u;
	}
	return program;
}

static qboolean FrameValid( const ralOpenGlProductFrameReceipt_t *receipt )
{
	return receipt && receipt->schemaVersion == RAL_OPENGL_PRODUCT_SCHEMA_VERSION &&
		   receipt->backendType == RAL_BACKEND_OPENGL && receipt->productGeneration && receipt->frameGeneration &&
		   receipt->vertexShaderByteCount == ralOpenGlProductVertexByteCount &&
		   receipt->fragmentShaderByteCount == ralOpenGlProductFragmentByteCount &&
		   receipt->vertexShaderDigestLane0 == ralOpenGlProductVertexDigestLane0 &&
		   receipt->vertexShaderDigestLane1 == ralOpenGlProductVertexDigestLane1 &&
		   receipt->fragmentShaderDigestLane0 == ralOpenGlProductFragmentDigestLane0 &&
		   receipt->fragmentShaderDigestLane1 == ralOpenGlProductFragmentDigestLane1 &&
		   RalOpenGl_FrontendPlanReceiptExact( &receipt->plan, &receipt->plan ) &&
		   Ral_DisplayVisibilityPlanValid( &receipt->displayVisibility ) &&
		   RalOpenGl_WorldReceiptExact( &receipt->native, &receipt->native ) &&
		   Ral_AtmospherePlanReceiptExact( &receipt->atmosphere, &receipt->atmosphere ) &&
		   receipt->plan.planGeneration == receipt->frameGeneration &&
		   receipt->native.generation == receipt->frameGeneration &&
		   receipt->atmosphere.frameGeneration == receipt->frameGeneration &&
		   receipt->atmosphereDispatchCount == receipt->atmosphere.computeDispatchCount &&
		   receipt->atmosphereFroxelCount == receipt->atmosphere.froxelCount &&
		   receipt->atmosphereCompositeCount == receipt->atmosphere.compositeCount &&
		   receipt->atmosphereCloudsActive == receipt->atmosphere.cloudsActive &&
		   Ral_AtmosphereWeatherReceiptExact( &receipt->weather, &receipt->weather ) &&
		   receipt->weatherDispatchCount == ( receipt->weather.zeroWork ? 0u : 1u ) &&
		   receipt->weatherDrawCount == ( receipt->weather.zeroWork ? 0u : 1u ) &&
		   receipt->native.weatherInstanceCount == receipt->weather.activeParticleCount &&
		   receipt->native.loweringDigest == receipt->plan.loweringDigest && !receipt->unresolvedCount &&
		   !receipt->fallbackCount && !receipt->fatalCount && receipt->ready == qtrue;
}

qboolean RalOpenGl_ProductFrameReceiptExact( const ralOpenGlProductFrameReceipt_t *a,
											 const ralOpenGlProductFrameReceipt_t *b )
{
	return FrameValid( a ) && FrameValid( b ) && !memcmp( a, b, sizeof( *a ) );
}

qboolean RalOpenGl_ProductCreate( ralOpenGlCore_t *core, const ralOpenGlCoreReceipt_t *coreReceipt, uint64_t generation,
								  ralOpenGlProduct_t **outProduct )
{
	ralOpenGlProduct_t	*product;
	glName_t			 vertex = 0u, fragment = 0u, compute = 0u;
	glName_t			 weatherVertex = 0u, weatherFragment = 0u;
	const unsigned char *computeSources[4] = {
		ralOpenGlProductAtmosphereInjectSource, ralOpenGlProductAtmosphereLightSource,
		ralOpenGlProductAtmosphereCloudSource, ralOpenGlProductAtmosphereIntegrateSource };
	const unsigned int computeByteCounts[4] = {
		ralOpenGlProductAtmosphereInjectByteCount, ralOpenGlProductAtmosphereLightByteCount,
		ralOpenGlProductAtmosphereCloudByteCount, ralOpenGlProductAtmosphereIntegrateByteCount };
	glInt_t linked = 0;
	if ( !core || !coreReceipt || !generation || generation == UINT64_MAX || !outProduct ||
		 !RalOpenGl_CoreMatchesReceipt( core, coreReceipt ) )
		return qfalse;
	product = calloc( 1u, sizeof( *product ) );
	if ( !product )
		return qfalse;
	product->core		 = core;
	product->coreReceipt = *coreReceipt;
	product->generation	 = generation;
	if ( !Ral_DisplayVisibilityPlanBuild( RAL_DISPLAY_BRIGHTNESS_AUTHORED,
			&product->displayVisibility ) ) goto fail;
	if ( !LoadDispatch( core, &product->gl ) )
		goto fail;
	vertex =
		Compile( &product->gl, GL_VERTEX_SHADER_VALUE, ralOpenGlProductVertexSource, ralOpenGlProductVertexByteCount );
	fragment = Compile( &product->gl, GL_FRAGMENT_SHADER_VALUE, ralOpenGlProductFragmentSource,
						ralOpenGlProductFragmentByteCount );
	if ( !vertex || !fragment )
		goto fail;
	product->program = product->gl.CreateProgram();
	if ( !product->program )
		goto fail;
	product->gl.AttachShader( product->program, vertex );
	product->gl.AttachShader( product->program, fragment );
	product->gl.LinkProgram( product->program );
	product->gl.GetProgramiv( product->program, GL_LINK_STATUS_VALUE, &linked );
	if ( !linked || product->gl.GetError() != GL_NO_ERROR_VALUE )
		goto fail;
	for ( uint32_t i = 0u; i < 4u; ++i ) {
		compute = Compile( &product->gl, GL_COMPUTE_SHADER_VALUE, computeSources[i], computeByteCounts[i] );
		if ( !compute )
			goto fail;
		product->atmospherePrograms[i] = LinkSingleStage( &product->gl, compute );
		product->gl.DeleteShader( compute );
		compute = 0u;
		if ( !product->atmospherePrograms[i] )
			goto fail;
	}
	compute = Compile( &product->gl, GL_COMPUTE_SHADER_VALUE, ralOpenGlProductWeatherIntegrateSource,
					   ralOpenGlProductWeatherIntegrateByteCount );
	if ( !compute )
		goto fail;
	product->weatherPrograms[0] = LinkSingleStage( &product->gl, compute );
	product->gl.DeleteShader( compute );
	compute			= 0u;
	weatherVertex	= Compile( &product->gl, GL_VERTEX_SHADER_VALUE, ralOpenGlProductWeatherVertexSource,
							   ralOpenGlProductWeatherVertexByteCount );
	weatherFragment = Compile( &product->gl, GL_FRAGMENT_SHADER_VALUE, ralOpenGlProductWeatherFragmentSource,
							   ralOpenGlProductWeatherFragmentByteCount );
	if ( !product->weatherPrograms[0] || !weatherVertex || !weatherFragment )
		goto fail;
	product->weatherPrograms[1] = product->gl.CreateProgram();
	if ( !product->weatherPrograms[1] )
		goto fail;
	product->gl.AttachShader( product->weatherPrograms[1], weatherVertex );
	product->gl.AttachShader( product->weatherPrograms[1], weatherFragment );
	product->gl.LinkProgram( product->weatherPrograms[1] );
	product->gl.GetProgramiv( product->weatherPrograms[1], GL_LINK_STATUS_VALUE, &linked );
	if ( !linked || product->gl.GetError() != GL_NO_ERROR_VALUE )
		goto fail;
	product->gl.DeleteShader( weatherFragment );
	weatherFragment = 0u;
	product->gl.DeleteShader( weatherVertex );
	weatherVertex = 0u;
	product->gl.CreateBuffers( 1, &product->atmosphereBuffers[RAL_OPENGL_ATMOSPHERE_FALLBACK] );
	if ( !product->atmosphereBuffers[RAL_OPENGL_ATMOSPHERE_FALLBACK] )
		goto fail;
	product->gl.NamedBufferStorage( product->atmosphereBuffers[RAL_OPENGL_ATMOSPHERE_FALLBACK], 16, NULL, 0u );
	if ( product->gl.GetError() != GL_NO_ERROR_VALUE )
		goto fail;
	product->gl.DeleteShader( fragment );
	product->gl.DeleteShader( vertex );
	*outProduct = product;
	return qtrue;
fail:
	if ( compute )
		product->gl.DeleteShader( compute );
	if ( weatherFragment )
		product->gl.DeleteShader( weatherFragment );
	if ( weatherVertex )
		product->gl.DeleteShader( weatherVertex );
	if ( product->atmosphereBuffers[RAL_OPENGL_ATMOSPHERE_FALLBACK] )
		product->gl.DeleteBuffers( 1, &product->atmosphereBuffers[RAL_OPENGL_ATMOSPHERE_FALLBACK] );
	for ( uint32_t i = 0u; i < 4u; ++i )
		if ( product->atmospherePrograms[i] )
			product->gl.DeleteProgram( product->atmospherePrograms[i] );
	for ( uint32_t i = 0u; i < 2u; ++i )
		if ( product->weatherPrograms[i] )
			product->gl.DeleteProgram( product->weatherPrograms[i] );
	if ( product->program )
		product->gl.DeleteProgram( product->program );
	if ( fragment )
		product->gl.DeleteShader( fragment );
	if ( vertex )
		product->gl.DeleteShader( vertex );
	free( product );
	return qfalse;
}

void RalOpenGl_ProductDestroy( ralOpenGlProduct_t *product )
{
	if ( !product )
		return;
	for ( uint32_t i = 0u; i < 4u; ++i )
		if ( product->atmospherePrograms[i] )
			product->gl.DeleteProgram( product->atmospherePrograms[i] );
	for ( uint32_t i = 0u; i < 2u; ++i )
		if ( product->weatherPrograms[i] )
			product->gl.DeleteProgram( product->weatherPrograms[i] );
	if ( product->weatherBuffers[0] )
		product->gl.DeleteBuffers( 3, product->weatherBuffers );
	if ( product->atmosphereBuffers[0] || product->atmosphereBuffers[RAL_OPENGL_ATMOSPHERE_FALLBACK] )
		product->gl.DeleteBuffers( RAL_OPENGL_ATMOSPHERE_BUFFER_COUNT, product->atmosphereBuffers );
	if ( product->program )
		product->gl.DeleteProgram( product->program );
	memset( product, 0, sizeof( *product ) );
	free( product );
}

qboolean RalOpenGl_ProductSetOutputExtent( ralOpenGlProduct_t *product, uint32_t width, uint32_t height )
{
	if ( !product || !width || !height || width > 16384u || height > 16384u )
		return qfalse;
	product->outputWidth			= width;
	product->outputHeight			= height;
	product->atmosphereHistoryValid = qfalse;
	return qtrue;
}

qboolean RalOpenGl_ProductSetDirectionalLighting( ralOpenGlProduct_t *product,
		const ralOpenGlLightingReceipt_t *lighting ) {
	if ( !product ) return qfalse;
	if ( !lighting ) {
		memset( &product->directionalLighting, 0,
			sizeof( product->directionalLighting ) );
		product->hasDirectionalLighting = qfalse;
		return qtrue;
	}
	if ( !RalOpenGl_LightingReceiptExact( lighting, lighting )
			|| lighting->coreGeneration != product->coreReceipt.generation )
		return qfalse;
	product->directionalLighting = *lighting;
	product->hasDirectionalLighting = qtrue;
	return qtrue;
}

qboolean RalOpenGl_ProductSetDisplayVisibility( ralOpenGlProduct_t *product,
		const ralDisplayVisibilityPlan_t *visibility ) {
	if ( !product || !Ral_DisplayVisibilityPlanValid( visibility ) ) return qfalse;
	product->displayVisibility = *visibility;
	return qtrue;
}

qboolean RalOpenGl_ProductReadbackRgb( ralOpenGlProduct_t *product, byte *outPixels, uint32_t byteCount )
{
	uint64_t required;
	if ( !product || !outPixels || !product->outputWidth || !product->outputHeight )
		return qfalse;
	required = (uint64_t)product->outputWidth * product->outputHeight * 3u;
	if ( required != byteCount || required > INT_MAX )
		return qfalse;
	product->gl.PixelStorei( GL_PACK_ALIGNMENT_VALUE, 1 );
	product->gl.ReadPixels( 0, 0, (glSize_t)product->outputWidth, (glSize_t)product->outputHeight, GL_RGB_VALUE,
							GL_UNSIGNED_BYTE_VALUE, outPixels );
	return product->gl.GetError() == GL_NO_ERROR_VALUE ? qtrue : qfalse;
}

static void IdentityMatrix( float matrix[16] )
{
	memset( matrix, 0, 16u * sizeof( *matrix ) );
	matrix[0] = matrix[5] = matrix[10] = matrix[15] = 1.0f;
}

static qboolean InvertMatrix( const float source[16], float out[16] )
{
	float rows[4][8];
	for ( uint32_t row = 0u; row < 4u; ++row ) {
		for ( uint32_t column = 0u; column < 4u; ++column ) {
			rows[row][column]	   = source[column * 4u + row];
			rows[row][column + 4u] = row == column ? 1.0f : 0.0f;
		}
	}
	for ( uint32_t column = 0u; column < 4u; ++column ) {
		uint32_t pivot = column;
		for ( uint32_t row = column + 1u; row < 4u; ++row )
			if ( fabsf( rows[row][column] ) > fabsf( rows[pivot][column] ) )
				pivot = row;
		if ( fabsf( rows[pivot][column] ) < 1.0e-8f )
			return qfalse;
		if ( pivot != column )
			for ( uint32_t i = 0u; i < 8u; ++i ) {
				float swap		= rows[column][i];
				rows[column][i] = rows[pivot][i];
				rows[pivot][i]	= swap;
			}
		{
			float divisor = rows[column][column];
			for ( uint32_t i = 0u; i < 8u; ++i )
				rows[column][i] /= divisor;
		}
		for ( uint32_t row = 0u; row < 4u; ++row )
			if ( row != column ) {
				float factor = rows[row][column];
				for ( uint32_t i = 0u; i < 8u; ++i )
					rows[row][i] -= factor * rows[column][i];
			}
	}
	for ( uint32_t row = 0u; row < 4u; ++row )
		for ( uint32_t column = 0u; column < 4u; ++column )
			out[column * 4u + row] = rows[row][column + 4u];
	return qtrue;
}

static qboolean AtmosphereView( const renderSubmissionState_t *frontend, float inverse[16], float mvp[16],
								float eye[3] )
{
	renderWorldSnapshot_t view;
	float				  matrix[16] = { 0 };
	float				  projectionX, projectionY;
	if ( !frontend || !inverse || !mvp || !eye || !RenderSubmission_WorldSnapshot( frontend, &view ) ||
		 !isfinite( view.fovX ) || !isfinite( view.fovY ) || view.fovX <= 1.0f || view.fovX >= 179.0f ||
		 view.fovY <= 1.0f || view.fovY >= 179.0f ) {
		IdentityMatrix( inverse );
		IdentityMatrix( mvp );
		memset( eye, 0, 3u * sizeof( *eye ) );
		return qtrue;
	}
	projectionX = 1.0f / tanf( view.fovX * 0.00872664625997164788f );
	projectionY = 1.0f / tanf( view.fovY * 0.00872664625997164788f );
	for ( uint32_t axis = 0u; axis < 3u; ++axis ) {
		matrix[axis * 4u + 0u] = -view.viewAxis[1][axis] * projectionX;
		matrix[axis * 4u + 1u] = view.viewAxis[2][axis] * projectionY;
		matrix[axis * 4u + 2u] = view.viewAxis[0][axis];
		matrix[axis * 4u + 3u] = view.viewAxis[0][axis];
	}
	for ( uint32_t axis = 0u; axis < 3u; ++axis ) {
		matrix[12] += view.viewOrigin[axis] * view.viewAxis[1][axis] * projectionX;
		matrix[13] -= view.viewOrigin[axis] * view.viewAxis[2][axis] * projectionY;
		matrix[14] -= view.viewOrigin[axis] * view.viewAxis[0][axis];
		matrix[15] -= view.viewOrigin[axis] * view.viewAxis[0][axis];
	}
	matrix[14] -= 4.0f;
	memcpy( mvp, matrix, sizeof( matrix ) );
	memcpy( eye, view.viewOrigin, 3u * sizeof( *eye ) );
	return InvertMatrix( matrix, inverse );
}

static qboolean PlanAtmosphere( const ralOpenGlProduct_t *product, const renderSubmissionState_t *frontend,
								const renderSubmissionReceipt_t *submission, uint64_t frameGeneration,
								ralAtmospherePlanReceipt_t *outReceipt )
{
	renderAtmosphereSnapshot_t		snapshot;
	renderAtmosphereMediaSnapshot_t media;
	ralAtmospherePlanRequest_t		request;
	const atmosphereEmitter_t	   *emitters;
	const atmosphereMediaVolume_t  *volumes;
	if ( !product || !frontend || !submission || !outReceipt ||
		 !RenderSubmission_AtmosphereSnapshot( frontend, &snapshot, &emitters ) ||
		 !RenderSubmission_AtmosphereMediaSnapshot( frontend, &media, &volumes ) )
		return qfalse;
	(void)emitters;
	(void)volumes;
	(void)submission;
	memset( &request, 0, sizeof( request ) );
	request.schemaVersion	= RAL_ATMOSPHERE_PLAN_SCHEMA_VERSION;
	request.backendType		= RAL_BACKEND_OPENGL;
	request.frameGeneration = frameGeneration;
	request.width			= product->outputWidth;
	request.height			= product->outputHeight;
	request.requestedTier = snapshot.active ? (ralAtmosphereTier_t)snapshot.state.qualityTier : RAL_ATMOSPHERE_TIER_OFF;
	request.localVolumeCount  = media.count;
	request.lightCount = RenderSubmission_AtmosphereLightCount( frontend );
	if ( request.lightCount > RAL_ATMOSPHERE_MAX_LIGHTS )
		request.lightCount = RAL_ATMOSPHERE_MAX_LIGHTS;
	request.shadowedLightCount = RenderSubmission_AtmosphereShadowedLightCount( frontend );
	if ( request.shadowedLightCount > RAL_ATMOSPHERE_MAX_SHADOWED_LIGHTS )
		request.shadowedLightCount = RAL_ATMOSPHERE_MAX_SHADOWED_LIGHTS;
	request.maxFroxelCount	  = RAL_OPENGL_ATMOSPHERE_FROXEL_CAPACITY;
	request.maxLocalVolumes	  = RAL_ATMOSPHERE_MAX_VOLUMES;
	request.maxLights		  = RAL_ATMOSPHERE_MAX_LIGHTS;
	request.maxShadowedLights = RAL_ATMOSPHERE_MAX_SHADOWED_LIGHTS;
	request.mediaActive = snapshot.active && ( snapshot.state.mediaDensity > 0.0f || snapshot.state.visibility > 0.0f ||
											   media.count > 0u )
							  ? qtrue
							  : qfalse;
	request.skyLightingActive			   = snapshot.active;
	request.cloudsRequested				   = snapshot.active && snapshot.state.cloudCover > 0.0f;
	request.historyValid				   = product->atmosphereHistoryValid;
	request.capabilities.analyticComposite = qtrue;
	request.capabilities.compute		   = qtrue;
	request.capabilities.storageBuffers	   = qtrue;
	request.capabilities.temporalHistory   = qtrue;
	request.capabilities.fullClouds		   = qtrue;
	return Ral_AtmospherePlan( &request, outReceipt );
}

static qboolean EnsureAtmosphereBuffers( ralOpenGlProduct_t *product )
{
	const glSizePtr_t froxelBytes = (glSizePtr_t)RAL_OPENGL_ATMOSPHERE_FROXEL_CAPACITY * 16;
	const glSizePtr_t tileBytes =
		(glSizePtr_t)( RAL_OPENGL_ATMOSPHERE_FROXEL_CAPACITY / RAL_ATMOSPHERE_MIN_SLICES ) * 33 * 4;
	if ( !product )
		return qfalse;
	if ( product->atmosphereBuffersReady )
		return qtrue;
	product->gl.CreateBuffers( RAL_OPENGL_ATMOSPHERE_FALLBACK, product->atmosphereBuffers );
	for ( uint32_t i = 0u; i < RAL_OPENGL_ATMOSPHERE_FALLBACK; ++i )
		if ( !product->atmosphereBuffers[i] )
			return qfalse;
	product->gl.NamedBufferStorage( product->atmosphereBuffers[RAL_OPENGL_ATMOSPHERE_PARAMS], 192, NULL,
									GL_DYNAMIC_STORAGE_BIT_VALUE );
	product->gl.NamedBufferStorage( product->atmosphereBuffers[RAL_OPENGL_ATMOSPHERE_VOLUMES],
									(glSizePtr_t)RAL_ATMOSPHERE_MAX_VOLUMES * sizeof( productAtmosphereVolume_t ), NULL,
									GL_DYNAMIC_STORAGE_BIT_VALUE );
	product->gl.NamedBufferStorage( product->atmosphereBuffers[RAL_OPENGL_ATMOSPHERE_MEDIA], froxelBytes, NULL, 0u );
	product->gl.NamedBufferStorage( product->atmosphereBuffers[RAL_OPENGL_ATMOSPHERE_TILE_LIGHTS], tileBytes, NULL,
									0u );
	product->gl.NamedBufferStorage( product->atmosphereBuffers[RAL_OPENGL_ATMOSPHERE_LIGHTS],
									(glSizePtr_t)RAL_ATMOSPHERE_MAX_LIGHTS * 48, NULL, 0u );
	for ( uint32_t i = RAL_OPENGL_ATMOSPHERE_LIT; i <= RAL_OPENGL_ATMOSPHERE_HISTORY_B; ++i )
		product->gl.NamedBufferStorage( product->atmosphereBuffers[i], froxelBytes, NULL, 0u );
	if ( product->gl.GetError() != GL_NO_ERROR_VALUE )
		return qfalse;
	product->atmosphereBuffersReady = qtrue;
	return qtrue;
}

static qboolean UploadAtmosphereVolumes( ralOpenGlProduct_t *product, const atmosphereMediaVolume_t *volumes,
										 uint32_t count )
{
	productAtmosphereVolume_t packed[RAL_ATMOSPHERE_MAX_VOLUMES];
	if ( !product || count > RAL_ATMOSPHERE_MAX_VOLUMES || ( count && !volumes ) )
		return qfalse;
	memset( packed, 0, sizeof( packed ) );
	for ( uint32_t i = 0u; i < count; ++i ) {
		memcpy( packed[i].originRadius, volumes[i].origin, 3u * sizeof( float ) );
		packed[i].originRadius[3] = volumes[i].radius;
		memcpy( packed[i].extentShape, volumes[i].extent, 3u * sizeof( float ) );
		packed[i].extentShape[3] = (float)volumes[i].shape;
		memcpy( packed[i].albedoAnisotropy, volumes[i].albedo, 3u * sizeof( float ) );
		packed[i].albedoAnisotropy[3] = volumes[i].anisotropy;
		memcpy( packed[i].emissiveIntensity, volumes[i].emissive, 3u * sizeof( float ) );
		packed[i].emissiveIntensity[3] = volumes[i].emissionIntensity;
		packed[i].media[0]			   = volumes[i].extinction;
		packed[i].media[1]			   = volumes[i].heightFalloff;
		packed[i].media[2]			   = volumes[i].noiseScale;
		packed[i].media[3]			   = (float)volumes[i].flags;
	}
	product->gl.NamedBufferSubData( product->atmosphereBuffers[RAL_OPENGL_ATMOSPHERE_VOLUMES], 0,
									(glSizePtr_t)sizeof( packed ), packed );
	return product->gl.GetError() == GL_NO_ERROR_VALUE ? qtrue : qfalse;
}

static void BindComputeBuffer( const ralOpenGlProduct_t *product, unsigned int binding, uint32_t buffer )
{
	product->gl.BindBufferBase( GL_SHADER_STORAGE_BUFFER_VALUE, binding, product->atmosphereBuffers[buffer] );
}

static qboolean EncodeAtmosphere( ralOpenGlProduct_t *product, const renderSubmissionState_t *frontend,
								  const ralAtmospherePlanReceipt_t *plan )
{
	renderAtmosphereSnapshot_t		snapshot;
	renderAtmosphereMediaSnapshot_t media;
	const atmosphereEmitter_t	   *emitters;
	const atmosphereMediaVolume_t  *volumes;
	float							inverse[16], mvp[16], eye[3], farDistance, cloudSpan;
	uint32_t						previous, next, source;
	if ( !product || !frontend || !plan || plan->selectedTier != RAL_ATMOSPHERE_TIER_FULL ||
		 plan->froxelCount > RAL_OPENGL_ATMOSPHERE_FROXEL_CAPACITY || !EnsureAtmosphereBuffers( product ) ||
		 !RenderSubmission_AtmosphereSnapshot( frontend, &snapshot, &emitters ) ||
		 !RenderSubmission_AtmosphereMediaSnapshot( frontend, &media, &volumes ) ||
		 !AtmosphereView( frontend, inverse, mvp, eye ) ||
		 !UploadAtmosphereVolumes( product, volumes, plan->admittedVolumeCount ) )
		return qfalse;
	(void)emitters;
	(void)media;
	(void)mvp;
	farDistance = snapshot.state.visibility > 4.0f ? snapshot.state.visibility : 65536.0f;
	previous	= product->atmosphereHistoryParity ? RAL_OPENGL_ATMOSPHERE_HISTORY_B : RAL_OPENGL_ATMOSPHERE_HISTORY_A;
	next		= product->atmosphereHistoryParity ? RAL_OPENGL_ATMOSPHERE_HISTORY_A : RAL_OPENGL_ATMOSPHERE_HISTORY_B;

	{
		productAtmosphereInjectParams_t params = { 0 };
		memcpy( params.invViewProjection, inverse, sizeof( inverse ) );
		memcpy( params.eyeNear, eye, sizeof( eye ) );
		params.eyeNear[3]				 = 4.0f;
		params.farGlobalDensity[0]		 = farDistance;
		params.farGlobalDensity[1]		 = snapshot.state.mediaDensity;
		params.farGlobalDensity[2]		 = snapshot.state.mediaHeightFalloff;
		params.globalAlbedoAnisotropy[0] = 0.72f;
		params.globalAlbedoAnisotropy[1] = 0.78f;
		params.globalAlbedoAnisotropy[2] = 0.86f;
		params.gridVolumeCount[0]		 = plan->froxelWidth;
		params.gridVolumeCount[1]		 = plan->froxelHeight;
		params.gridVolumeCount[2]		 = plan->froxelDepth;
		params.gridVolumeCount[3]		 = plan->admittedVolumeCount;
		params.timelineSeed[0]			 = snapshot.state.wind[0] * snapshot.state.timelineSeconds;
		params.timelineSeed[1]			 = snapshot.state.wind[1] * snapshot.state.timelineSeconds;
		params.timelineSeed[2]			 = snapshot.state.timelineSeconds;
		params.timelineSeed[3]			 = (float)snapshot.state.seed;
		product->gl.NamedBufferSubData( product->atmosphereBuffers[RAL_OPENGL_ATMOSPHERE_PARAMS], 0, sizeof( params ),
										&params );
		product->gl.UseProgram( product->atmospherePrograms[0] );
		product->gl.BindBufferBase( GL_UNIFORM_BUFFER_VALUE, 0u,
									product->atmosphereBuffers[RAL_OPENGL_ATMOSPHERE_PARAMS] );
		BindComputeBuffer( product, 0u, RAL_OPENGL_ATMOSPHERE_VOLUMES );
		BindComputeBuffer( product, 1u, RAL_OPENGL_ATMOSPHERE_MEDIA );
		product->gl.DispatchCompute( ( plan->froxelWidth + 3u ) / 4u, ( plan->froxelHeight + 3u ) / 4u,
									 ( plan->froxelDepth + 3u ) / 4u );
		product->gl.MemoryBarrier( GL_SHADER_STORAGE_BARRIER_BIT_VALUE | GL_UNIFORM_BARRIER_BIT_VALUE );
	}
	{
		productAtmosphereLightParams_t params = { 0 };
		memcpy( params.invViewProjection, inverse, sizeof( inverse ) );
		memcpy( params.eyeNear, eye, sizeof( eye ) );
		params.eyeNear[3]		 = 4.0f;
		params.farPad[0]		 = farDistance;
		params.gridLightCount[0] = plan->froxelWidth;
		params.gridLightCount[1] = plan->froxelHeight;
		params.gridLightCount[2] = plan->froxelDepth;
		params.gridLightCount[3] = 0u;
		memcpy( params.sunDirectionIntensity, snapshot.state.sunDirection, 3u * sizeof( float ) );
		params.sunDirectionIntensity[3] = snapshot.state.sunIntensity;
		params.sunColorCloudShadow[0] = params.sunColorCloudShadow[1] = params.sunColorCloudShadow[2] = 1.0f;
		params.sunColorCloudShadow[3] = snapshot.state.cloudShadow;
		memcpy( params.moonDirectionIntensity, snapshot.state.moonDirection, 3u * sizeof( float ) );
		params.moonDirectionIntensity[3] = snapshot.state.moonIntensity;
		params.moonColorLightning[0] = params.moonColorLightning[1] = params.moonColorLightning[2] = 0.25f;
		params.moonColorLightning[3] = snapshot.state.lightning;
		memcpy( params.ambientCloud, snapshot.state.ambientColor, 3u * sizeof( float ) );
		params.ambientCloud[3] = snapshot.state.cloudCover;
		product->gl.NamedBufferSubData( product->atmosphereBuffers[RAL_OPENGL_ATMOSPHERE_PARAMS], 0, sizeof( params ),
										&params );
		product->gl.UseProgram( product->atmospherePrograms[1] );
		BindComputeBuffer( product, 0u, RAL_OPENGL_ATMOSPHERE_MEDIA );
		BindComputeBuffer( product, 1u, RAL_OPENGL_ATMOSPHERE_TILE_LIGHTS );
		BindComputeBuffer( product, 2u, RAL_OPENGL_ATMOSPHERE_LIGHTS );
		BindComputeBuffer( product, 3u, RAL_OPENGL_ATMOSPHERE_LIT );
		product->gl.DispatchCompute( ( plan->froxelWidth + 3u ) / 4u, ( plan->froxelHeight + 3u ) / 4u,
									 ( plan->froxelDepth + 3u ) / 4u );
		product->gl.MemoryBarrier( GL_SHADER_STORAGE_BARRIER_BIT_VALUE | GL_UNIFORM_BARRIER_BIT_VALUE );
	}
	source = RAL_OPENGL_ATMOSPHERE_LIT;
	if ( plan->cloudsActive ) {
		productAtmosphereCloudParams_t params = { 0 };
		memcpy( params.invViewProjection, inverse, sizeof( inverse ) );
		memcpy( params.eyeNear, eye, sizeof( eye ) );
		params.eyeNear[3]				= 4.0f;
		params.farTimeCoverageShadow[0] = farDistance;
		params.farTimeCoverageShadow[1] = snapshot.state.timelineSeconds;
		params.farTimeCoverageShadow[2] = snapshot.state.cloudCover;
		params.farTimeCoverageShadow[3] = snapshot.state.cloudShadow;
		memcpy( params.windDensity, snapshot.state.wind, 3u * sizeof( float ) );
		params.windDensity[3] = fmaxf( snapshot.state.mediaDensity * 2.0f, 0.002f );
		cloudSpan			  = snapshot.state.bounds[5] - snapshot.state.bounds[2];
		if ( cloudSpan > 4.0f ) {
			params.layerErosion[0] = snapshot.state.bounds[2] + cloudSpan * 0.65f;
			params.layerErosion[1] = snapshot.state.bounds[2] + cloudSpan * 0.90f;
		} else {
			params.layerErosion[0] = eye[2] + 96.0f;
			params.layerErosion[1] = eye[2] + 384.0f;
		}
		params.layerErosion[2] = 0.15f;
		params.layerErosion[3] = 0.18f;
		memcpy( params.sunDirectionIntensity, snapshot.state.sunDirection, 3u * sizeof( float ) );
		params.sunDirectionIntensity[3] = snapshot.state.sunIntensity;
		params.sunColorLightning[0] = params.sunColorLightning[1] = params.sunColorLightning[2] = 1.0f;
		params.sunColorLightning[3] = snapshot.state.lightning;
		memcpy( params.ambientColor, snapshot.state.ambientColor, 3u * sizeof( float ) );
		params.grid[0] = plan->froxelWidth;
		params.grid[1] = plan->froxelHeight;
		params.grid[2] = plan->froxelDepth;
		product->gl.NamedBufferSubData( product->atmosphereBuffers[RAL_OPENGL_ATMOSPHERE_PARAMS], 0, sizeof( params ),
										&params );
		product->gl.UseProgram( product->atmospherePrograms[2] );
		BindComputeBuffer( product, 0u, RAL_OPENGL_ATMOSPHERE_LIT );
		BindComputeBuffer( product, 1u, RAL_OPENGL_ATMOSPHERE_CLOUD );
		product->gl.DispatchCompute( ( plan->froxelWidth + 3u ) / 4u, ( plan->froxelHeight + 3u ) / 4u,
									 ( plan->froxelDepth + 3u ) / 4u );
		product->gl.MemoryBarrier( GL_SHADER_STORAGE_BARRIER_BIT_VALUE | GL_UNIFORM_BARRIER_BIT_VALUE );
		source = RAL_OPENGL_ATMOSPHERE_CLOUD;
	}
	{
		productAtmosphereIntegrateParams_t params = { 0 };
		params.gridHistory[0]					  = plan->froxelWidth;
		params.gridHistory[1]					  = plan->froxelHeight;
		params.gridHistory[2]					  = plan->froxelDepth;
		params.gridHistory[3]		 = product->atmosphereHistoryValid && plan->temporalReuseCount ? 1u : 0u;
		params.depthRangeTemporal[0] = 4.0f;
		params.depthRangeTemporal[1] = farDistance;
		params.depthRangeTemporal[2] = 0.88f;
		product->gl.NamedBufferSubData( product->atmosphereBuffers[RAL_OPENGL_ATMOSPHERE_PARAMS], 0, sizeof( params ),
										&params );
		product->gl.UseProgram( product->atmospherePrograms[3] );
		BindComputeBuffer( product, 0u, source );
		BindComputeBuffer( product, 1u, previous );
		BindComputeBuffer( product, 2u, RAL_OPENGL_ATMOSPHERE_INTEGRATED );
		BindComputeBuffer( product, 3u, next );
		product->gl.DispatchCompute( ( plan->froxelWidth + 7u ) / 8u, ( plan->froxelHeight + 7u ) / 8u, 1u );
		product->gl.MemoryBarrier( GL_SHADER_STORAGE_BARRIER_BIT_VALUE | GL_UNIFORM_BARRIER_BIT_VALUE );
	}
	if ( product->gl.GetError() != GL_NO_ERROR_VALUE )
		return qfalse;
	product->atmosphereHistoryValid	 = qtrue;
	product->atmosphereHistoryParity = product->atmosphereHistoryParity ? qfalse : qtrue;
	return qtrue;
}

static void WeatherWeights( const atmosphereFrameState_t *state, float weights[5] )
{
	float total = 0.0f;
	memcpy( weights, state->precipitation, 5u * sizeof( *weights ) );
	for ( uint32_t i = 0u; i < 5u; ++i )
		total += weights[i];
	if ( total <= 0.0f && state->type >= ATMOSPHERE_PRECIP_RAIN && state->type <= ATMOSPHERE_PRECIP_DUST_ASH )
		weights[(uint32_t)state->type - 1u] = 1.0f;
}

static qboolean PlanWeather( const renderSubmissionState_t *frontend, const ralAtmospherePlanReceipt_t *atmosphere,
							 ralAtmosphereWeatherReceipt_t *outReceipt )
{
	renderAtmosphereSnapshot_t				 snapshot;
	renderAtmosphereEffectWorkloadSnapshot_t effects;
	ralAtmosphereWeatherRequest_t			 request;
	const atmosphereEmitter_t				*emitters;
	if ( !frontend || !atmosphere || !outReceipt ||
		 !RenderSubmission_AtmosphereSnapshot( frontend, &snapshot, &emitters ) )
		return qfalse;
	(void)emitters;
	memset( &request, 0, sizeof( request ) );
	request.schemaVersion  = RAL_ATMOSPHERE_WEATHER_SCHEMA_VERSION;
	request.tier		   = atmosphere->selectedTier;
	request.maxParticles   = RAL_OPENGL_WEATHER_PARTICLE_CAPACITY;
	request.enabled		   = snapshot.active;
	request.indoorExposure = snapshot.state.indoorExposure;
	if ( !RenderSubmission_AtmosphereEffectWorkloadSnapshot( frontend, request.maxParticles, &effects ) )
		return qfalse;
	request.semanticEmitterCount  = effects.admittedEmitterCount;
	request.semanticParticleCount = effects.admittedParticleCount;
	WeatherWeights( &snapshot.state, request.precipitation );
	return Ral_AtmospherePlanWeather( &request, outReceipt );
}

static qboolean EnsureWeatherBuffers( ralOpenGlProduct_t *product )
{
	void			 *zero;
	const glSizePtr_t bytes = (glSizePtr_t)RAL_OPENGL_WEATHER_PARTICLE_CAPACITY * 32;
	if ( !product )
		return qfalse;
	if ( product->weatherBuffersReady )
		return qtrue;
	zero = calloc( 1u, (size_t)bytes );
	if ( !zero )
		return qfalse;
	product->gl.CreateBuffers( 3, product->weatherBuffers );
	if ( !product->weatherBuffers[0] || !product->weatherBuffers[1] || !product->weatherBuffers[2] ) {
		free( zero );
		return qfalse;
	}
	product->gl.NamedBufferStorage( product->weatherBuffers[0], sizeof( productWeatherFrame_t ), NULL,
									GL_DYNAMIC_STORAGE_BIT_VALUE );
	product->gl.NamedBufferStorage( product->weatherBuffers[1], bytes, zero, 0u );
	product->gl.NamedBufferStorage( product->weatherBuffers[2], bytes, zero, 0u );
	free( zero );
	if ( product->gl.GetError() != GL_NO_ERROR_VALUE )
		return qfalse;
	product->weatherBuffersReady = qtrue;
	return qtrue;
}

static qboolean EncodeWeather( ralOpenGlProduct_t *product, const renderSubmissionState_t *frontend,
							   const ralAtmosphereWeatherReceipt_t *weather, productWeatherFrame_t *outFrame )
{
	renderAtmosphereSnapshot_t				 snapshot;
	renderAtmosphereEffectWorkloadSnapshot_t effects;
	renderWorldSnapshot_t					 view;
	const atmosphereEmitter_t				*emitters;
	productWeatherFrame_t					 frame;
	float									 inverse[16], eye[3], weights[5];
	uint32_t								 readPool, writePool;
	if ( !product || !frontend || !weather || !outFrame || weather->zeroWork || !EnsureWeatherBuffers( product ) ||
		 !RenderSubmission_AtmosphereSnapshot( frontend, &snapshot, &emitters ) )
		return qfalse;
	(void)emitters;
	(void)inverse;
	memset( &frame, 0, sizeof( frame ) );
	if ( !AtmosphereView( frontend, inverse, frame.mvp, eye ) )
		return qfalse;
	if ( RenderSubmission_WorldSnapshot( frontend, &view ) ) {
		memcpy( frame.viewLeft, view.viewAxis[1], 3u * sizeof( float ) );
		memcpy( frame.viewUp, view.viewAxis[2], 3u * sizeof( float ) );
	} else {
		frame.viewLeft[1] = 1.0f;
		frame.viewUp[2]	  = 1.0f;
	}
	memcpy( frame.eyeWorld, eye, sizeof( eye ) );
	frame.viewLeft[3] = product->displayVisibility.exposureScale;
	frame.viewUp[3] = product->displayVisibility.shadowExponent;
	frame.eyeWorld[3] = product->displayVisibility.shadowPivot;
	frame.dt =
		product->weatherTimeline > 0.0f ? snapshot.state.timelineSeconds - product->weatherTimeline : 1.0f / 60.0f;
	if ( frame.dt < 0.0f || frame.dt > 0.1f )
		frame.dt = 1.0f / 60.0f;
	frame.time		   = snapshot.state.timelineSeconds;
	frame.poolSize	   = weather->activeParticleCount;
	frame.pingPongRead = product->weatherParity ? 1u : 0u;
	memcpy( frame.boundsMin, snapshot.state.bounds, 3u * sizeof( float ) );
	memcpy( frame.boundsMax, snapshot.state.bounds + 3u, 3u * sizeof( float ) );
	memcpy( frame.worldMins, snapshot.state.worldMins, 2u * sizeof( float ) );
	memcpy( frame.worldMaxs, snapshot.state.worldMaxs, 2u * sizeof( float ) );
	frame.atmType  = (uint32_t)snapshot.state.type;
	frame.distance = snapshot.state.distance > 0.0f ? snapshot.state.distance : 1024.0f;
	memcpy( frame.windGust, snapshot.state.wind, 3u * sizeof( float ) );
	frame.windGust[3] = snapshot.state.gustStrength;
	WeatherWeights( &snapshot.state, weights );
	memcpy( frame.precipitation, weights, 4u * sizeof( float ) );
	frame.dustAsh = weights[4];
	/* Coverage is already converted into the bounded active prefix by the
	 * portable planner; avoid paying a second full-pool thinning pass. */
	frame.indoorExposure	= 1.0f;
	frame.climateSeed		= snapshot.state.seed;
	frame.climate[0]		= snapshot.state.temperatureC;
	frame.climate[1]		= snapshot.state.humidity;
	frame.climate[2]		= snapshot.state.visibility;
	frame.climate[3]		= snapshot.state.transitionSeconds;
	frame.surfaceClimate[0] = snapshot.state.surfaceWetness;
	frame.surfaceClimate[1] = snapshot.state.surfaceFrost;
	frame.surfaceClimate[2] = snapshot.state.snowAccumulation;
	frame.surfaceClimate[3] = snapshot.state.meltRate;
	memcpy( frame.sun, snapshot.state.sunDirection, 3u * sizeof( float ) );
	frame.sun[3] = snapshot.state.sunIntensity;
	memcpy( frame.moon, snapshot.state.moonDirection, 3u * sizeof( float ) );
	frame.moon[3] = snapshot.state.moonIntensity;
	memcpy( frame.ambientCloud, snapshot.state.ambientColor, 3u * sizeof( float ) );
	frame.ambientCloud[3] = snapshot.state.cloudCover;
	frame.cloudMedia[0]	  = snapshot.state.cloudShadow;
	frame.cloudMedia[1]	  = snapshot.state.lightning;
	frame.cloudMedia[2]	  = snapshot.state.mediaDensity;
	frame.cloudMedia[3]	  = snapshot.state.mediaHeightFalloff;
	frame.renderParams[0] = 1.0f / (float)product->outputWidth;
	frame.renderParams[1] = 1.0f / (float)product->outputHeight;
	if ( !RenderSubmission_AtmosphereEffectGpuPayload( frontend, weather->semanticParticleCount,
													   weather->precipitationParticleCount, frame.effectWorkloads,
													   &effects ) ||
		 effects.admittedEmitterCount != weather->semanticEmitterCount ||
		 effects.admittedParticleCount != weather->semanticParticleCount )
		return qfalse;
	frame.effectMeta[0] = (float)weather->precipitationParticleCount;
	frame.effectMeta[1] = (float)weather->semanticEmitterCount;
	readPool			= product->weatherParity ? 2u : 1u;
	writePool			= product->weatherParity ? 1u : 2u;
	product->gl.NamedBufferSubData( product->weatherBuffers[0], 0, sizeof( frame ), &frame );
	product->gl.UseProgram( product->weatherPrograms[0] );
	product->gl.BindBufferBase( GL_UNIFORM_BUFFER_VALUE, 0u, product->weatherBuffers[0] );
	product->gl.BindBufferBase( GL_SHADER_STORAGE_BUFFER_VALUE, 0u, product->weatherBuffers[readPool] );
	product->gl.BindBufferBase( GL_SHADER_STORAGE_BUFFER_VALUE, 1u, product->weatherBuffers[writePool] );
	product->gl.DispatchCompute( ( weather->activeParticleCount + 63u ) / 64u, 1u, 1u );
	product->gl.MemoryBarrier( GL_SHADER_STORAGE_BARRIER_BIT_VALUE | GL_UNIFORM_BARRIER_BIT_VALUE );
	if ( product->gl.GetError() != GL_NO_ERROR_VALUE )
		return qfalse;
	product->weatherParity	 = product->weatherParity ? qfalse : qtrue;
	product->weatherTimeline = snapshot.state.timelineSeconds;
	*outFrame				 = frame;
	return qtrue;
}

qboolean RalOpenGl_ProductRender( ralOpenGlProduct_t *product, const renderSubmissionState_t *frontend,
								  const renderSubmissionReceipt_t *submission, uint64_t frameGeneration,
								  ralOpenGlProductFrameReceipt_t *outReceipt )
{
	ralOpenGlProductFrameReceipt_t receipt;
	ralOpenGlWorldLowerInfo_t	   lower;
	productWeatherFrame_t		   weatherFrame;
	if ( !product || !frontend || !submission || !outReceipt || !frameGeneration || frameGeneration == UINT64_MAX ||
		 !product->outputWidth || !product->outputHeight || frameGeneration <= product->lastFrameGeneration ||
		 !RalOpenGl_CoreMatchesReceipt( product->core, &product->coreReceipt ) )
		return qfalse;
	memset( &receipt, 0, sizeof( receipt ) );
	receipt.schemaVersion			  = RAL_OPENGL_PRODUCT_SCHEMA_VERSION;
	receipt.backendType				  = RAL_BACKEND_OPENGL;
	receipt.productGeneration		  = product->generation;
	receipt.frameGeneration			  = frameGeneration;
	receipt.vertexShaderByteCount	  = ralOpenGlProductVertexByteCount;
	receipt.fragmentShaderByteCount	  = ralOpenGlProductFragmentByteCount;
	receipt.vertexShaderDigestLane0	  = ralOpenGlProductVertexDigestLane0;
	receipt.vertexShaderDigestLane1	  = ralOpenGlProductVertexDigestLane1;
	receipt.fragmentShaderDigestLane0 = ralOpenGlProductFragmentDigestLane0;
	receipt.fragmentShaderDigestLane1 = ralOpenGlProductFragmentDigestLane1;
	receipt.displayVisibility = product->displayVisibility;
	if ( !RalOpenGl_FrontendPlanBuild( &product->coreReceipt, frontend, submission, frameGeneration, &receipt.plan ) )
		return qfalse;
	if ( !PlanAtmosphere( product, frontend, submission, frameGeneration, &receipt.atmosphere ) )
		return qfalse;
	if ( receipt.atmosphere.selectedTier == RAL_ATMOSPHERE_TIER_FULL &&
		 !EncodeAtmosphere( product, frontend, &receipt.atmosphere ) )
		return qfalse;
	receipt.atmosphereDispatchCount	 = receipt.atmosphere.computeDispatchCount;
	receipt.atmosphereFroxelCount	 = receipt.atmosphere.froxelCount;
	receipt.atmosphereCompositeCount = receipt.atmosphere.compositeCount;
	receipt.atmosphereCloudsActive	 = receipt.atmosphere.cloudsActive;
	if ( !PlanWeather( frontend, &receipt.atmosphere, &receipt.weather ) )
		return qfalse;
	memset( &weatherFrame, 0, sizeof( weatherFrame ) );
	if ( !receipt.weather.zeroWork ) {
		if ( !EncodeWeather( product, frontend, &receipt.weather, &weatherFrame ) )
			return qfalse;
		receipt.weatherDispatchCount = 1u;
		receipt.weatherDrawCount	 = 1u;
	}
	memset( &lower, 0, sizeof( lower ) );
	lower.generation		   = frameGeneration;
	lower.programName		   = product->program;
	lower.outputWidth		   = product->outputWidth;
	lower.outputHeight		   = product->outputHeight;
	lower.atmosphere		   = receipt.atmosphere;
	lower.displayVisibility = receipt.displayVisibility;
	lower.atmosphereBufferName = product->atmosphereBuffers[receipt.atmosphere.selectedTier == RAL_ATMOSPHERE_TIER_FULL
																? RAL_OPENGL_ATMOSPHERE_INTEGRATED
																: RAL_OPENGL_ATMOSPHERE_FALLBACK];
	if ( !receipt.weather.zeroWork ) {
		lower.weatherProgramName		= product->weatherPrograms[1];
		lower.weatherUniformBufferName	= product->weatherBuffers[0];
		lower.weatherParticleBufferName = product->weatherBuffers[product->weatherParity ? 2u : 1u];
		lower.weatherParticleCount		= receipt.weather.activeParticleCount;
	}
	if ( product->hasDirectionalLighting )
		lower.directionalLighting = &product->directionalLighting;
	if ( !RalOpenGl_WorldLower( product->core, &product->coreReceipt, frontend, submission, &receipt.plan, &lower,
								&receipt.native ) )
		return qfalse;
	receipt.unresolvedCount = receipt.plan.unresolvedCount + receipt.native.unresolvedCount;
	receipt.fallbackCount	= receipt.plan.fallbackCount + receipt.native.fallbackCount;
	receipt.fatalCount		= receipt.plan.fatalCount + receipt.native.fatalCount;
	receipt.ready			= qtrue;
	if ( !FrameValid( &receipt ) )
		return qfalse;
	product->lastFrameGeneration = frameGeneration;
	*outReceipt					 = receipt;
	return qtrue;
}
