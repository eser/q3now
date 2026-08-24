// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_opengl_frontend.h"
#include "ral_opengl_world.h"
#include "ral_opengl_product.h"
#include "maps/map_format_registry.h"

#include <stdio.h>
#include <string.h>

#define CHECK( condition ) do { if ( !( condition ) ) { \
	fprintf( stderr, "check failed at %s:%d: %s\n", __FILE__, __LINE__, \
		#condition ); return 1; } } while ( 0 )

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
	if ( !name || offset != 0 || size != 96 || !data ) s_gl.error = 0x0501;
}
static void FakeBindBufferBase( fakeEnum_t target, unsigned int index,
		fakeName_t name ) {
	if ( target != 0x8a11u || index != 0u || !name ) s_gl.error = 0x0501;
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
static void FakeTextureSubImage2D( fakeName_t name, fakeInt_t level,
		fakeInt_t x, fakeInt_t y, fakeSize_t width, fakeSize_t height,
		fakeEnum_t format, fakeEnum_t type, const void *pixels ) {
	(void)level; (void)x; (void)y; (void)format; (void)type;
	if ( !name || width <= 0 || height <= 0 || !pixels ) s_gl.error = 0x0501;
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
	(void)unit; (void)texture;
}
static void FakeBindVertexArray( fakeName_t vao ) { if ( !vao ) s_gl.error = 0x0501; }
static void FakeUseProgram( fakeName_t program ) { if ( !program ) s_gl.error = 0x0501; }
static void FakeDrawElements( fakeEnum_t mode, fakeSize_t count,
		fakeEnum_t type, const void *offset ) {
	(void)offset; if ( mode != 4u || count <= 0 || type != 0x1405 ) s_gl.error = 0x0501;
	else s_gl.drawCount++;
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
	if ( name != 0x0d05u || value != 1 ) s_gl.error = 0x0501;
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
	return ( stage == 0x8b31 || stage == 0x8b30 ) ? ++s_gl.nextName : 0u;
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
	if ( !strcmp( name, "glTextureSubImage2D" ) ) RETURN_PROC( FakeTextureSubImage2D );
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
	return NULL;
}
#undef RETURN_PROC

int main( void ) {
	renderSubmissionState_t frontend;
	renderSubmissionReceipt_t submission, staleSubmission;
	ralOpenGlCore_t *coreOwner = NULL;
	ralOpenGlCoreReceipt_t core;
	ralOpenGlCoreCreateInfo_t coreInfo;
	ralOpenGlFrontendPlanReceipt_t plan, before, exact;
	ralOpenGlWorldLowerInfo_t lowerInfo;
	ralOpenGlWorldReceipt_t worldReceipt, worldExact;
	ralOpenGlProduct_t *product = NULL;
	ralOpenGlProductFrameReceipt_t productReceipt, productExact;
	byte readback[12];
	mapFile_t map;
	dsurface_t surface;
	drawVert_t vertices[3];
	int indices[3] = { 0, 1, 2 };
	dshader_t shader;
	refdef_t view;
	refEntity_t modelEntity, spriteEntity;
	refEntityMotion_t modelMotion;
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
	CHECK( RalOpenGl_ProductCreate( coreOwner, &core, 72u, &product ) );
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
	surface.numIndexes = 3; surface.lightmapNum = -1;
	vertices[1].xyz[0] = 1.0f; vertices[2].xyz[1] = 1.0f;
	CHECK( RenderSubmission_LoadWorld( &frontend, &map, 0 ) );
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
	CHECK( RenderSubmission_RenderScene( &frontend, &view, 0 ) );
	memset( &modelMotion, 0, sizeof( modelMotion ) );
	modelMotion.structSize = sizeof( modelMotion );
	modelMotion.version = REF_ENTITY_MOTION_VERSION;
	modelMotion.ownerId = 1u; modelMotion.generation = 75u;
	modelMotion.role = REF_ENTITY_MOTION_ROLE_PLAYER_BODY;
	CHECK( RenderSubmission_AddEntity( &frontend, &modelEntity, &modelMotion ) );
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
	CHECK( RalOpenGl_WorldLower( coreOwner, &core, &frontend, &submission,
		&plan, &lowerInfo, &worldReceipt ) );
	CHECK( worldReceipt.uploadedMaterialCount == 2u
		&& worldReceipt.uploadedWorldVertexCount == 3u
		&& worldReceipt.uploadedWorldIndexCount == 3u
		&& worldReceipt.worldDrawCount == 1u
		&& worldReceipt.polygonCount == 1u
		&& worldReceipt.lightCount == 1u
		&& worldReceipt.effectIndexCount == 9u
		&& worldReceipt.effectDrawCount == 2u
		&& worldReceipt.modelEntityCount == 1u
		&& worldReceipt.primitiveEntityCount == 1u
		&& worldReceipt.temporalEntityCount == 1u
		&& worldReceipt.entityIndexCount == 9u
		&& worldReceipt.entityDrawCount == 2u
		&& worldReceipt.uiPrimitiveCount == 2u
		&& worldReceipt.texturedUiPrimitiveCount == 2u
		&& worldReceipt.msdfUiPrimitiveCount == 1u
		&& worldReceipt.uiDrawCount == 2u
		&& worldReceipt.nativeDrawCount == 7u
		&& worldReceipt.unresolvedCount == 0u
		&& s_gl.drawCount == 7u && s_gl.textureUploadCount == 2u );
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
		&& productReceipt.native.nativeDrawCount == 7u
		&& productReceipt.unresolvedCount == 0u
		&& productReceipt.fallbackCount == 0u
		&& productReceipt.fatalCount == 0u
		&& s_gl.drawCount == 7u && s_gl.textureUploadCount == 2u );
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
		&& productReceipt.native.nativeDrawCount == 2u
		&& productReceipt.fallbackCount == 0u
		&& productReceipt.unresolvedCount == 0u );
	RenderSubmission_Reset( &frontend );
	RalOpenGl_ProductDestroy( product );
	RalOpenGl_CoreDestroy( coreOwner );
	puts( "ral opengl frontend plan contract: ok" );
	return 0;
}
