// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_opengl_frontend.h"
#include "ral_opengl_lighting.h"
#include "ral_opengl_world.h"
#include "ral_opengl_product.h"
#include "ral_lighting_product.h"
#include "ral_atmosphere_conformance.h"
#include "maps/map_format_registry.h"

#include <stdio.h>
#include <string.h>

#define CHECK( condition ) do { if ( !( condition ) ) { \
	fprintf( stderr, "check failed at %s:%d: %s\n", __FILE__, __LINE__, \
		#condition ); return 1; } } while ( 0 )

static ralIrradianceEntitySampleReceipt_t LocalIrradiance( uint64_t generation,
		uint64_t entityId ) {
	ralIrradianceEntitySampleReceipt_t r;
	memset( &r, 0, sizeof( r ) );
	r.schemaVersion = RAL_IRRADIANCE_ENTITY_RECEIPT_SCHEMA_VERSION;
	r.queryGeneration = generation; r.entityId = entityId;
	r.volumeId = 7u; r.layoutHash = 8u;
	r.probes.schemaVersion = RAL_IRRADIANCE_RECEIPT_SCHEMA_VERSION;
	r.probes.queryGeneration = generation; r.probes.productGeneration = 9u;
	r.probes.fallback = RAL_IRRADIANCE_FALLBACK_LIGHTGRID;
	r.probes.usedFallback = qtrue; r.probes.ready = qtrue;
	for ( uint32_t channel = 0u; channel < 3u; ++channel ) {
		r.blendedCoefficientsQ16[0][channel] = RAL_LIGHT_Q16_ONE;
		r.diffuseIrradianceQ16[channel] = RAL_LIGHT_Q16_ONE;
	}
	r.contributorHash = 10u;
	r.coefficientHash = Ral_IrradianceCoefficientHash( r.blendedCoefficientsQ16 );
	r.usedFallback = qtrue; r.ready = qtrue;
	return r;
}

typedef unsigned int fakeName_t;
typedef unsigned int fakeEnum_t;
typedef int fakeInt_t;
typedef int fakeSize_t;
typedef long fakeSizePtr_t;
typedef long fakeIntPtr_t;
typedef unsigned char fakeBool_t;

static struct {
	fakeName_t nextName;
	fakeEnum_t error;
	uint32_t drawCount;
	uint32_t textureUploadCount;
	uint32_t dispatchCount;
	uint32_t barrierCount;
	uint32_t weatherDrawCount;
	fakeName_t boundTextureByUnit[5];
} s_gl;

static void FakeGetIntegerv( fakeEnum_t name, fakeInt_t *out ) {
	switch ( name ) {
	case 0x821b: *out = 4; break;
	case 0x821c: *out = 6; break;
	case 0x9126: *out = 1; break;
	case 0x8cdf: *out = 8; break;
	case 0x0d33: *out = 16384; break;
	case 0x8073: *out = 2048; break;
	case 0x88ff: *out = 2048; break;
	case 0x90eb: *out = 1024; break;
	case 0x8a34: *out = 256; break;
	case 0x90df: *out = 16; break;
	case 0x8b4d: *out = 192; break;
	default: *out = 0; s_gl.error = 0x0500; break;
	}
}
static const unsigned char *FakeGetString( fakeEnum_t name ) {
	(void)name; return (const unsigned char *)"Wired OpenGL 4.6 test";
}
static fakeEnum_t FakeGetError( void ) {
	fakeEnum_t error = s_gl.error; s_gl.error = 0u; return error;
}
static void FakeCreateNames( fakeSize_t count, fakeName_t *names ) {
	for ( int i = 0; i < count; ++i ) names[i] = ++s_gl.nextName;
}
static void FakeCreateTextures( fakeEnum_t target, fakeSize_t count,
		fakeName_t *names ) { (void)target; FakeCreateNames( count, names ); }
static void FakeNamedBufferStorage( fakeName_t name, fakeSizePtr_t size,
		const void *data, unsigned int flags ) {
	(void)data; (void)flags; if ( !name || size <= 0 ) s_gl.error = 0x0501;
}
static void FakeNamedBufferSubData( fakeName_t name, fakeIntPtr_t offset,
		fakeSizePtr_t size, const void *data ) {
	if ( !name || offset != 0 || size <= 0 || !data ) s_gl.error = 0x0501;
}
static void FakeBindBufferBase( fakeEnum_t target, unsigned int index,
		fakeName_t name ) {
	if ( ( target != 0x8a11u && target != 0x90d2u )
			|| index > 3u || !name ) s_gl.error = 0x0501;
}
static void *FakeMapNamedBufferRange( fakeName_t name, fakeIntPtr_t offset,
		fakeSizePtr_t size, unsigned int flags ) {
	static unsigned char storage[4096];
	(void)offset; (void)flags;
	if ( !name || size <= 0 || size > (fakeSizePtr_t)sizeof( storage ) ) return NULL;
	return storage;
}
static fakeBool_t FakeUnmapNamedBuffer( fakeName_t name ) { return name ? 1u : 0u; }
static void FakeCopyNamedBufferSubData( fakeName_t source, fakeName_t destination,
		fakeIntPtr_t sourceOffset, fakeIntPtr_t destinationOffset, fakeSizePtr_t size ) {
	(void)sourceOffset; (void)destinationOffset;
	if ( !source || !destination || size <= 0 ) s_gl.error = 0x0501;
}
static void FakeDeleteNames( fakeSize_t count, const fakeName_t *names ) {
	(void)count; (void)names;
}
static void FakeTextureStorage2D( fakeName_t name, fakeSize_t levels,
		fakeEnum_t format, fakeSize_t width, fakeSize_t height ) {
	(void)format; if ( !name || levels != 1 || width <= 0 || height <= 0 )
		s_gl.error = 0x0501;
}
static void FakeTextureStorage3D( fakeName_t name, fakeSize_t levels,
		fakeEnum_t format, fakeSize_t width, fakeSize_t height, fakeSize_t depth ) {
	(void)format; if ( !name || levels != 1 || width <= 0 || height <= 0 || depth <= 0 )
		s_gl.error = 0x0501;
}
static void FakeTextureSubImage2D( fakeName_t name, fakeInt_t level,
		fakeInt_t x, fakeInt_t y, fakeSize_t width, fakeSize_t height,
		fakeEnum_t format, fakeEnum_t type, const void *pixels ) {
	(void)level; (void)x; (void)y; (void)format; (void)type;
	if ( !name || width <= 0 || height <= 0 || !pixels ) s_gl.error = 0x0501;
	else s_gl.textureUploadCount++;
}
static void FakeTextureSubImage3D( fakeName_t name, fakeInt_t level,
		fakeInt_t x, fakeInt_t y, fakeInt_t z, fakeSize_t width, fakeSize_t height,
		fakeSize_t depth, fakeEnum_t format, fakeEnum_t type, const void *pixels ) {
	(void)level; (void)x; (void)y; (void)z; (void)format; (void)type;
	if ( !name || width <= 0 || height <= 0 || depth <= 0 || !pixels ) s_gl.error = 0x0501;
	else s_gl.textureUploadCount++;
}
static void FakeTextureParameteri( fakeName_t name, fakeEnum_t key,
		fakeInt_t value ) { (void)key; (void)value; if ( !name ) s_gl.error = 0x0501; }
static void FakeSamplerParameteri( fakeName_t name, fakeEnum_t key,
		fakeInt_t value ) { (void)key; (void)value; if ( !name ) s_gl.error = 0x0501; }
static void FakeVertexArrayVertexBuffer( fakeName_t vao, unsigned int binding,
		fakeName_t buffer, fakeIntPtr_t offset, fakeSize_t stride ) {
	(void)binding; (void)offset; if ( !vao || !buffer || stride <= 0 ) s_gl.error = 0x0501;
}
static void FakeVertexArrayElementBuffer( fakeName_t vao, fakeName_t buffer ) {
	if ( !vao || !buffer ) s_gl.error = 0x0501;
}
static void FakeEnableVertexArrayAttrib( fakeName_t vao, unsigned int location ) {
	(void)location; if ( !vao ) s_gl.error = 0x0501;
}
static void FakeVertexArrayAttribFormat( fakeName_t vao, unsigned int location,
		fakeInt_t size, fakeEnum_t type, fakeBool_t normalized,
		unsigned int offset ) {
	(void)location; (void)type; (void)normalized; (void)offset;
	if ( !vao || size <= 0 ) s_gl.error = 0x0501;
}
static void FakeVertexArrayAttribBinding( fakeName_t vao, unsigned int location,
		unsigned int binding ) { (void)location; (void)binding; if ( !vao ) s_gl.error = 0x0501; }
static void FakeBindTextureUnit( unsigned int unit, fakeName_t texture ) {
	if ( unit < 5u ) s_gl.boundTextureByUnit[unit] = texture;
}
static void FakeBindVertexArray( fakeName_t vao ) { if ( !vao ) s_gl.error = 0x0501; }
static void FakeUseProgram( fakeName_t program ) { if ( !program ) s_gl.error = 0x0501; }
static void FakeDrawElements( fakeEnum_t mode, fakeSize_t count,
		fakeEnum_t type, const void *offset ) {
	(void)offset; if ( mode != 4u || count <= 0 || type != 0x1405 ) s_gl.error = 0x0501;
	else s_gl.drawCount++;
}
static void FakeDrawArraysInstanced( fakeEnum_t mode, fakeInt_t first,
		fakeSize_t count, fakeSize_t instances ) {
	if ( mode != 4u || first != 0 || count != 6 || instances <= 0 )
		s_gl.error = 0x0501;
	else s_gl.weatherDrawCount++;
}
static void FakeState( fakeEnum_t state ) { (void)state; }
static void FakeBlendFunc( fakeEnum_t source, fakeEnum_t destination ) {
	(void)source; (void)destination;
}
static void FakeDepthMask( fakeBool_t enabled ) { (void)enabled; }
static void FakeViewport( fakeInt_t x, fakeInt_t y, fakeSize_t width,
		fakeSize_t height ) {
	(void)x; (void)y; if ( width <= 0 || height <= 0 ) s_gl.error = 0x0501;
}
static void FakeClearColor( float red, float green, float blue, float alpha ) {
	(void)red; (void)green; (void)blue; (void)alpha;
}
static void FakeClear( unsigned int mask ) {
	if ( mask != ( 0x00004000u | 0x00000100u ) ) s_gl.error = 0x0501;
}
static void FakePixelStorei( fakeEnum_t name, fakeInt_t value ) {
	if ( ( name != 0x0d05u && name != 0x0cf5u ) || value != 1 ) s_gl.error = 0x0501;
}
static void FakeReadPixels( fakeInt_t x, fakeInt_t y, fakeSize_t width,
		fakeSize_t height, fakeEnum_t format, fakeEnum_t type, void *pixels ) {
	(void)x; (void)y;
	if ( width <= 0 || height <= 0 || format != 0x1907u
			|| type != 0x1401u || !pixels ) s_gl.error = 0x0501;
	else memset( pixels, 0x7f, (size_t)width * (size_t)height * 3u );
}
static void FakeFinish( void ) {}
static fakeName_t FakeCreateShader( fakeEnum_t stage ) {
	return ( stage == 0x8b31 || stage == 0x8b30 || stage == 0x91b9 )
		? ++s_gl.nextName : 0u;
}
static void FakeDispatchCompute( unsigned int x, unsigned int y,
		unsigned int z ) {
	if ( !x || !y || !z ) s_gl.error = 0x0501;
	else s_gl.dispatchCount++;
}
static void FakeMemoryBarrier( unsigned int barriers ) {
	if ( !( barriers & 0x2000u ) ) s_gl.error = 0x0501;
	else s_gl.barrierCount++;
}
static void FakeShaderSource( fakeName_t shader, fakeSize_t count,
		const char *const *source, const fakeInt_t *length ) {
	if ( !shader || count != 1 || !source || !source[0]
			|| !length || length[0] <= 0 ) s_gl.error = 0x0501;
}
static void FakeCompileShader( fakeName_t shader ) { if ( !shader ) s_gl.error = 0x0501; }
static void FakeGetCompileStatus( fakeName_t name, fakeEnum_t key, fakeInt_t *status ) {
	if ( !name || ( key != 0x8b81 && key != 0x8b82 ) ) { *status = 0; s_gl.error = 0x0501; }
	else *status = 1;
}
static void FakeDeleteShader( fakeName_t shader ) { (void)shader; }
static fakeName_t FakeCreateProgram( void ) { return ++s_gl.nextName; }
static void FakeAttachShader( fakeName_t program, fakeName_t shader ) {
	if ( !program || !shader ) s_gl.error = 0x0501;
}
static void FakeLinkProgram( fakeName_t program ) { if ( !program ) s_gl.error = 0x0501; }
static void FakeDeleteProgram( fakeName_t program ) { (void)program; }

#define RETURN_PROC( functionName ) return (ralOpenGlProc_t)( functionName )
static ralOpenGlProc_t FakeResolve( const char *name, void *userData ) {
	(void)userData;
	if ( !strcmp( name, "glGetIntegerv" ) ) RETURN_PROC( FakeGetIntegerv );
	if ( !strcmp( name, "glGetString" ) ) RETURN_PROC( FakeGetString );
	if ( !strcmp( name, "glGetError" ) ) RETURN_PROC( FakeGetError );
	if ( !strcmp( name, "glCreateBuffers" ) ) RETURN_PROC( FakeCreateNames );
	if ( !strcmp( name, "glNamedBufferStorage" ) ) RETURN_PROC( FakeNamedBufferStorage );
	if ( !strcmp( name, "glNamedBufferSubData" ) ) RETURN_PROC( FakeNamedBufferSubData );
	if ( !strcmp( name, "glBindBufferBase" ) ) RETURN_PROC( FakeBindBufferBase );
	if ( !strcmp( name, "glMapNamedBufferRange" ) ) RETURN_PROC( FakeMapNamedBufferRange );
	if ( !strcmp( name, "glUnmapNamedBuffer" ) ) RETURN_PROC( FakeUnmapNamedBuffer );
	if ( !strcmp( name, "glCopyNamedBufferSubData" ) ) RETURN_PROC( FakeCopyNamedBufferSubData );
	if ( !strcmp( name, "glDeleteBuffers" ) ) RETURN_PROC( FakeDeleteNames );
	if ( !strcmp( name, "glCreateTextures" ) ) RETURN_PROC( FakeCreateTextures );
	if ( !strcmp( name, "glTextureStorage2D" ) ) RETURN_PROC( FakeTextureStorage2D );
	if ( !strcmp( name, "glTextureStorage3D" ) ) RETURN_PROC( FakeTextureStorage3D );
	if ( !strcmp( name, "glTextureSubImage2D" ) ) RETURN_PROC( FakeTextureSubImage2D );
	if ( !strcmp( name, "glTextureSubImage3D" ) ) RETURN_PROC( FakeTextureSubImage3D );
	if ( !strcmp( name, "glTextureParameteri" ) ) RETURN_PROC( FakeTextureParameteri );
	if ( !strcmp( name, "glDeleteTextures" ) ) RETURN_PROC( FakeDeleteNames );
	if ( !strcmp( name, "glCreateSamplers" ) ) RETURN_PROC( FakeCreateNames );
	if ( !strcmp( name, "glSamplerParameteri" ) ) RETURN_PROC( FakeSamplerParameteri );
	if ( !strcmp( name, "glDeleteSamplers" ) ) RETURN_PROC( FakeDeleteNames );
	if ( !strcmp( name, "glFinish" ) ) RETURN_PROC( FakeFinish );
	if ( !strcmp( name, "glCreateVertexArrays" ) ) RETURN_PROC( FakeCreateNames );
	if ( !strcmp( name, "glDeleteVertexArrays" ) ) RETURN_PROC( FakeDeleteNames );
	if ( !strcmp( name, "glVertexArrayVertexBuffer" ) ) RETURN_PROC( FakeVertexArrayVertexBuffer );
	if ( !strcmp( name, "glVertexArrayElementBuffer" ) ) RETURN_PROC( FakeVertexArrayElementBuffer );
	if ( !strcmp( name, "glEnableVertexArrayAttrib" ) ) RETURN_PROC( FakeEnableVertexArrayAttrib );
	if ( !strcmp( name, "glVertexArrayAttribFormat" ) ) RETURN_PROC( FakeVertexArrayAttribFormat );
	if ( !strcmp( name, "glVertexArrayAttribBinding" ) ) RETURN_PROC( FakeVertexArrayAttribBinding );
	if ( !strcmp( name, "glBindTextureUnit" ) ) RETURN_PROC( FakeBindTextureUnit );
	if ( !strcmp( name, "glBindVertexArray" ) ) RETURN_PROC( FakeBindVertexArray );
	if ( !strcmp( name, "glUseProgram" ) ) RETURN_PROC( FakeUseProgram );
	if ( !strcmp( name, "glDrawElements" ) ) RETURN_PROC( FakeDrawElements );
	if ( !strcmp( name, "glDrawArraysInstanced" ) )
		RETURN_PROC( FakeDrawArraysInstanced );
	if ( !strcmp( name, "glEnable" ) ) RETURN_PROC( FakeState );
	if ( !strcmp( name, "glDisable" ) ) RETURN_PROC( FakeState );
	if ( !strcmp( name, "glBlendFunc" ) ) RETURN_PROC( FakeBlendFunc );
	if ( !strcmp( name, "glDepthMask" ) ) RETURN_PROC( FakeDepthMask );
	if ( !strcmp( name, "glDepthFunc" ) ) RETURN_PROC( FakeState );
	if ( !strcmp( name, "glViewport" ) ) RETURN_PROC( FakeViewport );
	if ( !strcmp( name, "glClearColor" ) ) RETURN_PROC( FakeClearColor );
	if ( !strcmp( name, "glClear" ) ) RETURN_PROC( FakeClear );
	if ( !strcmp( name, "glPixelStorei" ) ) RETURN_PROC( FakePixelStorei );
	if ( !strcmp( name, "glReadPixels" ) ) RETURN_PROC( FakeReadPixels );
	if ( !strcmp( name, "glCreateShader" ) ) RETURN_PROC( FakeCreateShader );
	if ( !strcmp( name, "glShaderSource" ) ) RETURN_PROC( FakeShaderSource );
	if ( !strcmp( name, "glCompileShader" ) ) RETURN_PROC( FakeCompileShader );
	if ( !strcmp( name, "glGetShaderiv" ) ) RETURN_PROC( FakeGetCompileStatus );
	if ( !strcmp( name, "glDeleteShader" ) ) RETURN_PROC( FakeDeleteShader );
	if ( !strcmp( name, "glCreateProgram" ) ) RETURN_PROC( FakeCreateProgram );
	if ( !strcmp( name, "glAttachShader" ) ) RETURN_PROC( FakeAttachShader );
	if ( !strcmp( name, "glLinkProgram" ) ) RETURN_PROC( FakeLinkProgram );
	if ( !strcmp( name, "glGetProgramiv" ) ) RETURN_PROC( FakeGetCompileStatus );
	if ( !strcmp( name, "glDeleteProgram" ) ) RETURN_PROC( FakeDeleteProgram );
	if ( !strcmp( name, "glDispatchCompute" ) ) RETURN_PROC( FakeDispatchCompute );
	if ( !strcmp( name, "glMemoryBarrier" ) ) RETURN_PROC( FakeMemoryBarrier );
	return NULL;
}
#undef RETURN_PROC

int main( void ) {
	renderSubmissionState_t frontend;
	atmosphereFrameState_t atmosphere;
	renderSubmissionReceipt_t submission, staleSubmission;
	ralOpenGlCore_t *coreOwner = NULL;
	ralOpenGlCoreReceipt_t core;
	ralOpenGlCoreCreateInfo_t coreInfo;
	ralOpenGlFrontendPlanReceipt_t plan, before, exact;
	ralOpenGlWorldLowerInfo_t lowerInfo;
	ralOpenGlWorldReceipt_t worldReceipt, worldExact;
	ralOpenGlLighting_t *lighting = NULL;
	ralOpenGlLightingReceipt_t lightingReceipt, lightingExact;
	ralOpenGlProduct_t *product = NULL;
	ralOpenGlProductFrameReceipt_t productReceipt, productExact;
	ralAtmospherePlanRequest_t atmosphereRequest;
	byte readback[12];
	mapFile_t map;
	dsurface_t surface;
	drawVert_t vertices[3];
	int indices[3] = { 0, 1, 2 };
	dshader_t shader;
	refdef_t view;
	refEntity_t modelEntity, spriteEntity;
	refEntityMotion_t modelMotion;
	particleClass_t breathClass;
	atmosphereEffectProfile_t breathProfile;
	atmosphereEmitter_t breathEmitter;
	atmosphereMediaVolume_t media;
	qhandle_t material, msdfMaterial, model;
	const byte green[16] = {
		0u, 255u, 0u, 255u, 0u, 255u, 0u, 255u,
		0u, 255u, 0u, 255u, 0u, 255u, 0u, 255u
	};
	memset( &s_gl, 0, sizeof( s_gl ) ); s_gl.nextName = 10u;
	memset( &coreInfo, 0, sizeof( coreInfo ) );
	coreInfo.generation = 71u; coreInfo.contextIdentity = 0x7102u;
	coreInfo.resolveProc = FakeResolve;
	CHECK( RalOpenGl_CoreCreate( &coreInfo, &coreOwner, &core ) );
	{
		ralLightingProductRequest_t request;
		ralLightingArtifactReceipt_t artifact;
		ralLightingRuntimePlan_t runtimePlan;
		ralStaticLightingCapabilities_t capabilities = { qtrue, qtrue, qtrue, qtrue };
		const ralLightVec3Q16_t radiance[2] = {
			{ 4 * RAL_LIGHT_Q16_ONE, RAL_LIGHT_Q16_ONE, 0 },
			{ RAL_LIGHT_Q16_ONE, 2 * RAL_LIGHT_Q16_ONE, 3 * RAL_LIGHT_Q16_ONE }
		};
		const ralLightVec3Q16_t direction[2] = {
			{ 0, 0, RAL_LIGHT_Q16_ONE }, { RAL_LIGHT_Q16_ONE, 0, RAL_LIGHT_Q16_ONE }
		};
		const uint8_t visibility[2] = { 255u, 128u };
		uint8_t radianceBytes[16], directionBytes[8], artifactBytes[512];
		memset( &request, 0, sizeof( request ) );
		request.schemaVersion = RAL_LIGHTING_PRODUCT_SCHEMA_VERSION;
		request.artifactGeneration = 7u;
		request.bake.schemaVersion = RAL_LIGHTING_BAKE_RECEIPT_SCHEMA_VERSION;
		request.bake.bakeGeneration = 1u;
		request.bake.staticIndirectKey = 2u;
		request.bake.producerVersion = 3u;
		request.bake.radianceHash = 4u;
		request.bake.directionHash = 5u;
		request.bake.patchCount = 2u;
		request.bake.linkCount = 1u;
		request.bake.completedBounces = 4u;
		request.bake.ready = qtrue;
		request.encoding = RAL_STATIC_LIGHTING_ENCODING_RGB9E5_OCT8;
		request.pageWidth = request.pageHeight = 1u;
		request.pageCount = request.texelCount = 2u;
		request.indirectRadiance = radiance;
		request.dominantDirection = direction;
		request.stationaryVisibility = visibility;
		request.stationaryVisibilityCount = 2u;
		CHECK( Ral_LightingProductWrite( &request, radianceBytes, sizeof( radianceBytes ), directionBytes,
										 sizeof( directionBytes ), artifactBytes, sizeof( artifactBytes ), &artifact ) );
		CHECK( Ral_LightingRuntimePlanBuild( RAL_BACKEND_OPENGL, 8u, artifactBytes, artifact.byteLength, &artifact,
											&capabilities, &runtimePlan ) );
		CHECK( RalOpenGl_LightingUpload( coreOwner, &core, artifactBytes, artifact.byteLength, &runtimePlan, &lighting,
										&lightingReceipt ) && lightingReceipt.plan.planeCount == 3u &&
			   s_gl.textureUploadCount == 3u );
		lightingExact = lightingReceipt;
		CHECK( RalOpenGl_LightingReceiptExact( &lightingReceipt, &lightingExact ) );
		s_gl.textureUploadCount = 0u;
	}
	CHECK( RalOpenGl_ProductCreate( coreOwner, &core, 72u, &product ) );
	CHECK( RalOpenGl_ProductSetDirectionalLighting( product, &lightingReceipt ) );
	CHECK( RalOpenGl_ProductSetOutputExtent( product, 2u, 2u )
		&& RalOpenGl_ProductReadbackRgb( product, readback,
			(uint32_t)sizeof( readback ) )
		&& readback[0] == 0x7f
		&& RalOpenGl_ProductSetOutputExtent( product, 1280u, 720u ) );
	CHECK( RalOpenGl_CoreReceiptExact( &core, &core ) );
	CHECK( RenderSubmission_Init( &frontend, 71u ) );
	material = RenderSubmission_RegisterMaterialImage( &frontend,
		RENDER_ASSET_MATERIAL, "textures/green", qtrue, green, 2u, 2u );
	CHECK( material > 0 );
	memset( &breathClass, 0, sizeof( breathClass ) );
	breathClass.shader = material;
	breathClass.emitMode = EMIT_POINT;
	breathClass.scatterShape = SCATTER_SPHERE;
	breathClass.velocityShape = VEL_AXIAL_PLUS_CUBE;
	breathClass.scatterMagnitude = 1.5f;
	breathClass.axialSpeed = 8.0f;
	breathClass.cubeJitter = 1.0f;
	breathClass.lifetimeMean = 1.0f;
	breathClass.paletteCount = 1;
	breathClass.colorPalette[0][0] = 0.82f;
	breathClass.colorPalette[0][1] = 0.90f;
	breathClass.colorPalette[0][2] = 1.0f;
	breathClass.colorPalette[0][3] = 0.38f;
	breathClass.sizeStart = 1.0f;
	breathClass.sizeEnd = 8.0f;
	breathClass.drag = 0.8f;
	CHECK( RenderSubmission_RegisterParticleClass( &frontend, 1, &breathClass ) );
	memset( &breathProfile, 0, sizeof( breathProfile ) );
	breathProfile.schemaVersion = WIRED_ATMOSPHERE_EFFECT_PROFILE_SCHEMA_VERSION;
	breathProfile.stageCount = 1u;
	breathProfile.maxParticles = 24u;
	breathProfile.seed = 216u;
	breathProfile.duration = 1.0f;
	breathProfile.lodFar = 512.0f;
	breathProfile.boundsRadius = 32.0f;
	breathProfile.stages[0].trigger = ATMOSPHERE_STAGE_CONTINUOUS;
	breathProfile.stages[0].particleClass = 1u;
	breathProfile.stages[0].parentStage = UINT32_MAX;
	breathProfile.stages[0].maxParticles = 24u;
	breathProfile.stages[0].spawnRate = 24.0f;
	breathProfile.stages[0].duration = 1.0f;
	breathProfile.stages[0].lodFar = 512.0f;
	breathProfile.stages[0].boundsRadius = 32.0f;
	breathProfile.stages[0].intensityScale = 1.0f;
	CHECK( RenderSubmission_RegisterAtmosphereEffectProfile( &frontend, 1u,
		&breathProfile ) );
	memset( &map, 0, sizeof( map ) );
	memset( &surface, 0, sizeof( surface ) );
	memset( vertices, 0, sizeof( vertices ) );
	memset( &shader, 0, sizeof( shader ) );
	strcpy( map.name, "maps/opengl-plan.bsp" );
	map.checksum = 0x12345678;
	map.numSurfaces = 1; map.surfaces = &surface;
	map.numDrawVerts = 3; map.drawVerts = vertices;
	map.numDrawIndexes = 3; map.drawIndexes = indices;
	map.numShaders = 1; map.shaders = &shader;
	strcpy( shader.shader, "textures/green" );
	surface.surfaceType = MST_PLANAR; surface.numVerts = 3;
	surface.numIndexes = 3; surface.lightmapNum = 0;
	vertices[1].xyz[0] = 1.0f; vertices[2].xyz[1] = 1.0f;
	CHECK( RenderSubmission_LoadWorld( &frontend, &map, 0 ) );
	memset( &atmosphere, 0, sizeof( atmosphere ) );
	atmosphere.schemaVersion = WIRED_ATMOSPHERE_SCHEMA_VERSION;
	atmosphere.flags = ATMOSPHERE_FLAG_ENABLED
		| ATMOSPHERE_FLAG_VOLUMETRIC_MEDIA | ATMOSPHERE_FLAG_SKY_LIGHTING;
	atmosphere.qualityTier = ATMOSPHERE_QUALITY_FULL;
	atmosphere.seed = 216u;
	atmosphere.bounds[0] = atmosphere.bounds[1] = atmosphere.bounds[2] = -64.0f;
	atmosphere.bounds[3] = atmosphere.bounds[4] = atmosphere.bounds[5] = 64.0f;
	atmosphere.temperatureC = -8.0f;
	atmosphere.humidity = 0.9f;
	atmosphere.indoorExposure = 1.0f;
	atmosphere.ambientColor[0] = 0.12f;
	atmosphere.ambientColor[1] = 0.16f;
	atmosphere.ambientColor[2] = 0.22f;
	atmosphere.mediaDensity = 0.1f;
	atmosphere.mediaHeightFalloff = 0.01f;
	atmosphere.cloudCover = 0.65f;
	atmosphere.cloudShadow = 0.4f;
	atmosphere.precipitation[1] = 0.5f;
	atmosphere.precipitation[2] = 0.5f;
	CHECK( RenderSubmission_SetAtmosphere( &frontend, &atmosphere ) );
	model = RenderSubmission_RegisterInlineModel( &frontend, "*1", 0u, 1u );
	CHECK( model > 0 );
	/* A loaded world can coexist with a UI-only loading frame before the first
	 * RenderScene call. That frame is presentable but owns no world draws. */
	CHECK( RenderSubmission_BeginFrame( &frontend, 70u ) );
	CHECK( RenderSubmission_AddUiQuad( &frontend, 0.0f, 0.0f, 1280.0f, 720.0f,
		0.0f, 0.0f, 1.0f, 1.0f, 0.0f, material ) );
	CHECK( RenderSubmission_EndFrame( &frontend, 70u, &submission ) );
	CHECK( submission.worldLoaded && !submission.sceneRendered );
	CHECK( RalOpenGl_FrontendPlanBuild( &core, &frontend, &submission, 71u,
		&plan ) );
	CHECK( plan.loweredWorldVertexCount == 0u
		&& plan.loweredWorldIndexCount == 0u
		&& plan.loweredWorldBatchCount == 0u
		&& plan.loweredUiPrimitiveCount == 1u
		&& plan.unresolvedCount == 0u );
	CHECK( RenderSubmission_BeginFrame( &frontend, 72u ) );
	memset( &view, 0, sizeof( view ) );
	view.width = 1280; view.height = 720; view.fov_x = 90.0f; view.fov_y = 60.0f;
	view.viewaxis[0][0] = view.viewaxis[1][1] = view.viewaxis[2][2] = 1.0f;
	CHECK( RenderSubmission_RenderScene( &frontend, &view, 0 ) );
	memset( &modelEntity, 0, sizeof( modelEntity ) );
	modelEntity.reType = RT_MODEL; modelEntity.hModel = model;
	modelEntity.axis[0][0] = modelEntity.axis[1][1] = modelEntity.axis[2][2] = 1.0f;
	memset( modelEntity.shader.rgba, 255, sizeof( modelEntity.shader.rgba ) );
	CHECK( RenderSubmission_AddEntity( &frontend, &modelEntity, NULL ) );
	{
		ralIrradianceEntitySampleReceipt_t local = LocalIrradiance( 72u, 1u );
		CHECK( RenderSubmission_AttachEntityIrradiance( &frontend, 0u, &local ) );
	}
	memset( &spriteEntity, 0, sizeof( spriteEntity ) );
	spriteEntity.reType = RT_SPRITE; spriteEntity.radius = 1.0f;
	spriteEntity.customShader = material;
	memset( spriteEntity.shader.rgba, 255, sizeof( spriteEntity.shader.rgba ) );
	CHECK( RenderSubmission_AddEntity( &frontend, &spriteEntity, NULL ) );
	CHECK( RenderSubmission_AddUiQuad( &frontend, 0.0f, 0.0f, 1280.0f, 720.0f,
		0.0f, 0.0f, 1.0f, 1.0f, 0.0f, material ) );
	CHECK( RenderSubmission_EndFrame( &frontend, 72u, &submission ) );
	CHECK( RalOpenGl_FrontendPlanBuild( &core, &frontend, &submission, 73u,
		&plan ) );
	CHECK( plan.loweredMaterialCount == 1u
		&& plan.loweredWorldVertexCount == 3u
		&& plan.loweredWorldIndexCount == 3u
		&& plan.loweredWorldBatchCount == 1u
		&& plan.texturedWorldBatchCount == 1u
		&& plan.loweredModelEntityCount == 1u
		&& plan.loweredLocalIrradianceEntityCount == 1u
		&& plan.loweredPrimitiveEntityCount == 1u
		&& plan.loweredEntityIndexCount == 9u
		&& plan.loweredEntityBatchCount == 2u
		&& plan.loweredUiPrimitiveCount == 1u
		&& plan.unresolvedCount == 0u );
	exact = plan;
	CHECK( RalOpenGl_FrontendPlanReceiptExact( &plan, &exact ) );
	exact.loweringDigest++;
	CHECK( !RalOpenGl_FrontendPlanReceiptExact( &plan, &exact ) );
	memset( &plan, 0xa5, sizeof( plan ) ); before = plan;
	staleSubmission = submission; staleSubmission.frameDigest++;
	CHECK( !RalOpenGl_FrontendPlanBuild( &core, &frontend, &staleSubmission,
		74u, &plan ) && !memcmp( &plan, &before, sizeof( plan ) ) );
	msdfMaterial = RenderSubmission_RegisterMaterialImage( &frontend,
		RENDER_ASSET_MSDF, "fonts/opengl-msdf", qtrue, green, 2u, 2u );
	CHECK( msdfMaterial > 0 );
	CHECK( RenderSubmission_BeginFrame( &frontend, 75u ) );
	memset( &breathEmitter, 0, sizeof( breathEmitter ) );
	breathEmitter.schemaVersion = WIRED_ATMOSPHERE_SCHEMA_VERSION;
	breathEmitter.kind = ATMOSPHERE_EMITTER_BREATH;
	breathEmitter.id = 1u;
	breathEmitter.seed = 216u;
	breathEmitter.profile = 1u;
	breathEmitter.intensity = 1.0f;
	breathEmitter.direction[0] = 1.0f;
	breathEmitter.radius = 8.0f;
	breathEmitter.temperatureC = 37.0f;
	breathEmitter.humidity = 1.0f;
	CHECK( RenderSubmission_AddAtmosphereEmitter( &frontend, &breathEmitter ) );
	CHECK( RenderSubmission_RenderScene( &frontend, &view, 0 ) );
	memset( &modelMotion, 0, sizeof( modelMotion ) );
	modelMotion.structSize = sizeof( modelMotion );
	modelMotion.version = REF_ENTITY_MOTION_VERSION;
	modelMotion.ownerId = 1u; modelMotion.generation = 75u;
	modelMotion.role = REF_ENTITY_MOTION_ROLE_PLAYER_BODY;
	CHECK( RenderSubmission_AddEntity( &frontend, &modelEntity, &modelMotion ) );
	{
		ralIrradianceEntitySampleReceipt_t local = LocalIrradiance( 75u, 1u );
		CHECK( RenderSubmission_AttachEntityIrradiance( &frontend, 0u, &local ) );
	}
	CHECK( RenderSubmission_AddEntity( &frontend, &spriteEntity, NULL ) );
	CHECK( RenderSubmission_AddUiQuad( &frontend, 0.0f, 0.0f, 1280.0f, 720.0f,
		0.0f, 0.0f, 1.0f, 1.0f, 15.0f, material ) );
	CHECK( RenderSubmission_AddUiLine( &frontend, 10.0f, 20.0f, 200.0f, 220.0f,
		2.0f, msdfMaterial ) );
	{
		polyVert_t poly[3];
		memset( poly, 0, sizeof( poly ) );
		CHECK( RenderSubmission_AddPoly( &frontend, material, 3, poly, 1 ) );
	}
	CHECK( RenderSubmission_AddLight( &frontend, spriteEntity.origin, NULL,
		64.0f, 1.0f, 0.5f, 0.25f ) );
	CHECK( RenderSubmission_EndFrame( &frontend, 75u, &submission ) );
	CHECK( RalOpenGl_FrontendPlanBuild( &core, &frontend, &submission, 76u,
		&plan ) );
	CHECK( plan.loweredPolygonCount == 1u && plan.loweredLightCount == 1u
		&& plan.loweredEffectIndexCount == 9u
		&& plan.loweredEffectBatchCount == 2u
		&& plan.loweredModelEntityCount == 1u
		&& plan.loweredPrimitiveEntityCount == 1u
		&& plan.loweredTemporalEntityCount == 1u
		&& plan.loweredLocalIrradianceEntityCount == 1u
		&& plan.loweredUiPrimitiveCount == 2u
		&& plan.texturedUiPrimitiveCount == 2u
		&& plan.msdfUiPrimitiveCount == 1u
		&& plan.unresolvedCount == 0u );
	memset( &lowerInfo, 0, sizeof( lowerInfo ) );
	lowerInfo.generation = 77u;
	memset( &worldReceipt, 0xa5, sizeof( worldReceipt ) ); worldExact = worldReceipt;
	CHECK( !RalOpenGl_WorldLower( coreOwner, &core, &frontend, &submission,
		&plan, &lowerInfo, &worldReceipt )
		&& !memcmp( &worldReceipt, &worldExact, sizeof( worldReceipt ) ) );
	lowerInfo.programName = 9001u;
	lowerInfo.outputWidth = 1280u;
	lowerInfo.outputHeight = 720u;
	memset( &atmosphereRequest, 0, sizeof( atmosphereRequest ) );
	atmosphereRequest.schemaVersion = RAL_ATMOSPHERE_PLAN_SCHEMA_VERSION;
	atmosphereRequest.backendType = RAL_BACKEND_OPENGL;
	atmosphereRequest.frameGeneration = lowerInfo.generation;
	atmosphereRequest.width = 1280u; atmosphereRequest.height = 720u;
	atmosphereRequest.requestedTier = RAL_ATMOSPHERE_TIER_FULL;
	atmosphereRequest.maxFroxelCount = 262144u;
	atmosphereRequest.maxLocalVolumes = RAL_ATMOSPHERE_MAX_VOLUMES;
	atmosphereRequest.maxLights = RAL_ATMOSPHERE_MAX_LIGHTS;
	atmosphereRequest.maxShadowedLights = RAL_ATMOSPHERE_MAX_SHADOWED_LIGHTS;
	atmosphereRequest.mediaActive = qtrue;
	atmosphereRequest.skyLightingActive = qtrue;
	atmosphereRequest.cloudsRequested = qtrue;
	atmosphereRequest.capabilities.analyticComposite = qtrue;
	atmosphereRequest.capabilities.compute = qtrue;
	atmosphereRequest.capabilities.storageBuffers = qtrue;
	atmosphereRequest.capabilities.temporalHistory = qtrue;
	atmosphereRequest.capabilities.fullClouds = qtrue;
	CHECK( Ral_AtmospherePlan( &atmosphereRequest, &lowerInfo.atmosphere ) );
	lowerInfo.atmosphereBufferName = 9002u;
	lowerInfo.directionalLighting = &lightingReceipt;
	CHECK( Ral_DisplayVisibilityPlanBuild( 1.0f,
		&lowerInfo.displayVisibility ) );
	CHECK( RalOpenGl_WorldLower( coreOwner, &core, &frontend, &submission,
		&plan, &lowerInfo, &worldReceipt ) );
	CHECK( worldReceipt.uploadedMaterialCount == 2u
		&& worldReceipt.uploadedWorldVertexCount == 3u
		&& worldReceipt.uploadedWorldIndexCount == 3u
		&& worldReceipt.worldDrawCount == 1u
		&& worldReceipt.legacyLightmapDrawCount == 0u
		&& worldReceipt.directionalStaticDrawCount == 1u
		&& worldReceipt.surfaceLightingBindingDigest != 0u
		&& worldReceipt.polygonCount == 1u
		&& worldReceipt.lightCount == 1u
		&& worldReceipt.effectIndexCount == 9u
		&& worldReceipt.effectDrawCount == 2u
		&& worldReceipt.modelEntityCount == 1u
		&& worldReceipt.primitiveEntityCount == 1u
		&& worldReceipt.temporalEntityCount == 1u
		&& worldReceipt.localIrradianceEntityCount == 1u
		&& worldReceipt.localIrradianceDrawCount == 1u
		&& worldReceipt.entityIndexCount == 9u
		&& worldReceipt.entityDrawCount == 2u
		&& worldReceipt.uiPrimitiveCount == 2u
		&& worldReceipt.texturedUiPrimitiveCount == 2u
		&& worldReceipt.msdfUiPrimitiveCount == 1u
		&& worldReceipt.uiDrawCount == 2u
		&& worldReceipt.nativeDrawCount == 7u
		&& worldReceipt.unresolvedCount == 0u
		&& s_gl.drawCount == 7u && s_gl.textureUploadCount == 2u
		&& s_gl.boundTextureByUnit[1] == 0u
		&& s_gl.boundTextureByUnit[2]
			== lightingReceipt.textureNames[0]
		&& s_gl.boundTextureByUnit[3]
			== lightingReceipt.textureNames[1]
		&& s_gl.boundTextureByUnit[4]
			== lightingReceipt.textureNames[2] );
	worldExact = worldReceipt;
	CHECK( RalOpenGl_WorldReceiptExact( &worldReceipt, &worldExact ) );
	worldExact.nativeDrawCount++;
	CHECK( !RalOpenGl_WorldReceiptExact( &worldReceipt, &worldExact ) );
	s_gl.drawCount = 0u; s_gl.textureUploadCount = 0u;
	CHECK( RalOpenGl_ProductRender( product, &frontend, &submission, 78u,
		&productReceipt ) );
	CHECK( productReceipt.plan.loweredWorldBatchCount == 1u
		&& productReceipt.plan.loweredModelEntityCount == 1u
		&& productReceipt.plan.loweredUiPrimitiveCount == 2u
		&& productReceipt.plan.loweredPolygonCount == 1u
		&& productReceipt.plan.loweredLightCount == 1u
		&& productReceipt.native.nativeDrawCount == 8u
		&& productReceipt.atmosphere.selectedTier == RAL_ATMOSPHERE_TIER_FULL
		&& productReceipt.atmosphere.fallbackReason
			== RAL_ATMOSPHERE_FALLBACK_NONE
		&& productReceipt.atmosphereDispatchCount == 4u
		&& productReceipt.atmosphereFroxelCount == 230400u
		&& productReceipt.atmosphereCompositeCount == 1u
		&& productReceipt.atmosphereCloudsActive == qtrue
		&& productReceipt.weather.familyMask == ( ( 1u << 1 ) | ( 1u << 2 ) )
		&& productReceipt.weather.activeParticleCount == 8192u
		&& productReceipt.weather.precipitationParticleCount == 8168u
		&& productReceipt.weather.semanticEmitterCount == 1u
		&& productReceipt.weather.semanticParticleCount == 24u
		&& productReceipt.weather.droppedParticleCount == 24u
		&& productReceipt.weatherDispatchCount == 1u
		&& productReceipt.weatherDrawCount == 1u
		&& productReceipt.native.weatherDrawCount == 1u
		&& productReceipt.native.weatherInstanceCount == 8192u
		&& productReceipt.unresolvedCount == 0u
		&& productReceipt.fallbackCount == 0u
		&& productReceipt.fatalCount == 0u
		&& s_gl.drawCount == 7u && s_gl.textureUploadCount == 2u
		&& s_gl.dispatchCount == 5u && s_gl.barrierCount == 5u
		&& s_gl.weatherDrawCount == 1u );
	productExact = productReceipt;
	CHECK( RalOpenGl_ProductFrameReceiptExact( &productReceipt, &productExact ) );
	memset( &productExact, 0xa5, sizeof( productExact ) ); worldExact = worldReceipt;
	CHECK( !RalOpenGl_ProductRender( product, &frontend, &submission, 78u,
		&productExact ) );
	/* Handle zero is the established null-model sentinel. The canonical
	 * adapter lowers it as an explicit proxy primitive, not a fallback. */
	CHECK( RenderSubmission_BeginFrame( &frontend, 79u ) );
	CHECK( RenderSubmission_RenderScene( &frontend, &view, 0 ) );
	memset( &modelEntity, 0, sizeof( modelEntity ) );
	modelEntity.reType = RT_MODEL; modelEntity.customShader = material;
	memset( modelEntity.shader.rgba, 255, sizeof( modelEntity.shader.rgba ) );
	CHECK( RenderSubmission_AddEntity( &frontend, &modelEntity, NULL ) );
	CHECK( RenderSubmission_EndFrame( &frontend, 79u, &submission ) );
	CHECK( RalOpenGl_ProductRender( product, &frontend, &submission, 80u,
		&productReceipt ) );
	CHECK( productReceipt.plan.loweredModelEntityCount == 0u
		&& productReceipt.plan.loweredPrimitiveEntityCount == 1u
		&& productReceipt.plan.loweredEntityIndexCount == 36u
		&& productReceipt.plan.loweredEntityBatchCount == 1u
		&& productReceipt.native.primitiveEntityCount == 1u
		&& productReceipt.native.nativeDrawCount == 3u
		&& productReceipt.fallbackCount == 0u
		&& productReceipt.unresolvedCount == 0u );
	for ( int fixtureIndex = RAL_ATMOSPHERE_FIXTURE_CLEAR;
			fixtureIndex < RAL_ATMOSPHERE_FIXTURE_COUNT; ++fixtureIndex ) {
		ralAtmosphereFixtureState_t authored;
		const uint64_t frontendGeneration = 100u + (uint64_t)fixtureIndex;
		const uint64_t productGeneration = 200u + (uint64_t)fixtureIndex;
		CHECK( Ral_AtmosphereConformanceFixture(
			(ralAtmosphereFixture_t)fixtureIndex, &authored ) );
		authored.state.timelineSeconds = (float)fixtureIndex;
		CHECK( RenderSubmission_SetAtmosphere( &frontend, &authored.state ) );
		CHECK( RenderSubmission_BeginFrame( &frontend, frontendGeneration ) );
		if ( authored.breathEmitter ) {
			Ral_AtmosphereConformanceBreathEmitter( 1u, &breathEmitter );
			CHECK( RenderSubmission_AddAtmosphereEmitter( &frontend,
				&breathEmitter ) );
		}
		if ( authored.localMedia ) {
			Ral_AtmosphereConformanceLocalMedia( &media );
			CHECK( RenderSubmission_AddAtmosphereMediaVolume( &frontend,
				&media ) );
		}
		CHECK( RenderSubmission_RenderScene( &frontend, &view, 0 ) );
		CHECK( RenderSubmission_EndFrame( &frontend, frontendGeneration,
			&submission ) );
		CHECK( RalOpenGl_ProductRender( product, &frontend, &submission,
			productGeneration, &productReceipt ) );
		CHECK( productReceipt.atmosphere.requestedTier ==
			(ralAtmosphereTier_t)authored.state.qualityTier &&
			productReceipt.atmosphere.selectedTier ==
			(ralAtmosphereTier_t)authored.state.qualityTier &&
			productReceipt.atmosphere.fallbackReason ==
				RAL_ATMOSPHERE_FALLBACK_NONE &&
			productReceipt.weather.familyMask == authored.familyMask );
		if ( authored.breathEmitter )
			CHECK( productReceipt.weather.semanticEmitterCount == 1u &&
				productReceipt.weather.semanticParticleCount == 24u );
		if ( fixtureIndex == RAL_ATMOSPHERE_FIXTURE_CLEAR ||
				fixtureIndex == RAL_ATMOSPHERE_FIXTURE_FOG )
			CHECK( productReceipt.weather.zeroWork );
		if ( fixtureIndex == RAL_ATMOSPHERE_FIXTURE_FOG )
			CHECK( productReceipt.atmosphereDispatchCount == 3u &&
				productReceipt.atmosphereFroxelCount == 230400u &&
				productReceipt.atmosphereCompositeCount == 1u );
		if ( fixtureIndex == RAL_ATMOSPHERE_FIXTURE_STORM )
			CHECK( productReceipt.atmosphereDispatchCount == 4u &&
				productReceipt.atmosphereCloudsActive &&
				productReceipt.weather.activeParticleCount == 8192u );
	}
	RenderSubmission_Reset( &frontend );
	CHECK( RalOpenGl_ProductSetDirectionalLighting( product, NULL ) );
	CHECK( RalOpenGl_LightingDestroy( coreOwner, &core, lighting,
		&lightingReceipt ) );
	RalOpenGl_ProductDestroy( product );
	RalOpenGl_CoreDestroy( coreOwner );
	puts( "ral opengl frontend plan contract: ok" );
	return 0;
}
