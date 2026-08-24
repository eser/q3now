// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "sdl_opengl46_ral.h"
#include "ral_opengl_product.h"
#include "maps/map_format_registry.h"

#include <stdio.h>
#include <string.h>

#define CHECK( condition ) do { if ( !( condition ) ) { \
	fprintf( stderr, "check failed at %s:%d: %s\n", __FILE__, __LINE__, \
		#condition ); return 1; } } while ( 0 )

static ralMemoryFailureEvent_t DeviceLoss( void ) {
	ralMemoryFailureEvent_t event;
	memset( &event, 0, sizeof( event ) );
	event.backendType = RAL_BACKEND_OPENGL;
	event.cause = RAL_MEMORY_FAILURE_DEVICE_LOST;
	event.memoryClass = RAL_ALLOCATION_DEVICE_LOCAL;
	event.residency = RAL_ALLOCATION_RESIDENCY_PERMANENT;
	event.criticality = RAL_MEMORY_CRITICALITY_REQUIRED;
	event.requestedBytes = 4096u;
	event.attempt = 1u;
	event.maxAttempts = 1u;
	event.liveParent = qtrue;
	return event;
}

static qboolean RunContentFrame( ralOpenGlProduct_t *product,
		const char *mapName, uint64_t generation,
		ralOpenGlProductFrameReceipt_t *outReceipt ) {
	renderSubmissionState_t frontend;
	renderSubmissionReceipt_t submission;
	mapFile_t map;
	dsurface_t surface;
	drawVert_t vertices[3];
	int indices[3] = { 0, 1, 2 };
	dshader_t shader;
	refdef_t view;
	refEntity_t modelEntity, spriteEntity;
	polyVert_t poly[3];
	qhandle_t material, model;
	const byte pixels[16] = { 255,255,255,255, 255,255,255,255,
		255,255,255,255, 255,255,255,255 };
	qboolean ok = qfalse;
	memset( &frontend, 0, sizeof( frontend ) );
	memset( &map, 0, sizeof( map ) ); memset( &surface, 0, sizeof( surface ) );
	memset( vertices, 0, sizeof( vertices ) ); memset( &shader, 0, sizeof( shader ) );
	memset( &view, 0, sizeof( view ) ); memset( &modelEntity, 0, sizeof( modelEntity ) );
	memset( &spriteEntity, 0, sizeof( spriteEntity ) ); memset( poly, 0, sizeof( poly ) );
	if ( !RenderSubmission_Init( &frontend, 201u ) ) return qfalse;
	material = RenderSubmission_RegisterMaterialImage( &frontend,
		RENDER_ASSET_MATERIAL, "textures/opengl-product", qtrue, pixels, 2u, 2u );
	if ( !material ) goto cleanup;
	(void)snprintf( map.name, sizeof( map.name ), "maps/%s.bsp", mapName );
	map.checksum = !strcmp( mapName, "arena1" ) ? 0xa1 : 0xa17;
	map.numSurfaces = 1; map.surfaces = &surface;
	map.numDrawVerts = 3; map.drawVerts = vertices;
	map.numDrawIndexes = 3; map.drawIndexes = indices;
	map.numShaders = 1; map.shaders = &shader;
	(void)snprintf( shader.shader, sizeof( shader.shader ), "textures/opengl-product" );
	surface.surfaceType = MST_PLANAR; surface.numVerts = 3;
	surface.numIndexes = 3; surface.lightmapNum = -1;
	vertices[0].xyz[0] = -0.5f; vertices[0].xyz[1] = -0.5f;
	vertices[1].xyz[0] = 0.5f; vertices[1].xyz[1] = -0.5f;
	vertices[2].xyz[1] = 0.5f;
	if ( !RenderSubmission_LoadWorld( &frontend, &map, 0 ) ) goto cleanup;
	model = RenderSubmission_RegisterInlineModel( &frontend, "*1", 0u, 1u );
	if ( !model || !RenderSubmission_BeginFrame( &frontend, generation ) ) goto cleanup;
	view.width = 1280; view.height = 720; view.fov_x = 90.0f; view.fov_y = 60.0f;
	view.viewaxis[0][0] = view.viewaxis[1][1] = view.viewaxis[2][2] = 1.0f;
	if ( !RenderSubmission_RenderScene( &frontend, &view, 0 ) ) goto cleanup;
	modelEntity.reType = RT_MODEL; modelEntity.hModel = model;
	modelEntity.axis[0][0] = modelEntity.axis[1][1] = modelEntity.axis[2][2] = 1.0f;
	memset( modelEntity.shader.rgba, 255, sizeof( modelEntity.shader.rgba ) );
	spriteEntity.reType = RT_SPRITE; spriteEntity.radius = 0.1f;
	spriteEntity.customShader = material;
	memset( spriteEntity.shader.rgba, 255, sizeof( spriteEntity.shader.rgba ) );
	if ( !RenderSubmission_AddEntity( &frontend, &modelEntity, NULL )
			|| !RenderSubmission_AddEntity( &frontend, &spriteEntity, NULL )
			|| !RenderSubmission_AddUiQuad( &frontend, -0.9f, -0.9f, 0.2f, 0.2f,
				0.0f, 0.0f, 1.0f, 1.0f, 0.0f, material )
			|| !RenderSubmission_AddPoly( &frontend, material, 3, poly, 1 )
			|| !RenderSubmission_AddLight( &frontend, spriteEntity.origin, NULL,
				16.0f, 1.0f, 1.0f, 1.0f )
			|| !RenderSubmission_EndFrame( &frontend, generation, &submission )
			|| !RalOpenGl_ProductRender( product, &frontend, &submission,
				generation, outReceipt ) ) goto cleanup;
	ok = outReceipt->plan.loweredWorldBatchCount > 0u
		&& outReceipt->plan.loweredModelEntityCount > 0u
		&& outReceipt->plan.loweredPrimitiveEntityCount > 0u
		&& outReceipt->plan.loweredUiPrimitiveCount > 0u
		&& outReceipt->plan.loweredPolygonCount > 0u
		&& outReceipt->plan.loweredLightCount > 0u
		&& outReceipt->unresolvedCount == 0u
		&& outReceipt->fallbackCount == 0u && outReceipt->fatalCount == 0u;
cleanup:
	RenderSubmission_Reset( &frontend );
	return ok;
}

int main( int argc, char **argv ) {
	const qboolean contractOnly = argc == 2 && !strcmp( argv[1], "--contract" )
		? qtrue : qfalse;
	wiredSdlOpenGl46CreateInfo_t info;
	wiredSdlOpenGl46_t *adapter = NULL;
	wiredSdlOpenGl46Receipt_t receipt, resized, presented, recreated, before;
	wiredSdlOpenGl46Status_t status;
	ralOpenGlCore_t *core = NULL;
	ralOpenGlCoreReceipt_t coreReceipt;
	ralOpenGlProduct_t *product = NULL;
	ralOpenGlProductFrameReceipt_t arena1, arena17;
	ralMemoryFailureEvent_t loss;
	memset( &info, 0, sizeof( info ) );
	info.logicalWidth = 1280u;
	info.logicalHeight = 720u;
	info.firstGeneration = 101u;
	info.visible = contractOnly ? qfalse : qtrue;
	memset( &receipt, 0xa5, sizeof( receipt ) );
	before = receipt;
	status = WiredSdlOpenGl46_Create( &info, &adapter, &receipt );
	if ( status == WIRED_SDL_OPENGL46_UNSUPPORTED ) {
		CHECK( adapter == NULL && !memcmp( &receipt, &before, sizeof( receipt ) ) );
		puts( "OpenGL 4.6 Core unavailable: canonical adapter failed closed" );
		return contractOnly ? 0 : 77;
	}
	CHECK( status == WIRED_SDL_OPENGL46_READY && adapter );
	CHECK( receipt.host.logicalWidth == 1280u && receipt.host.logicalHeight == 720u );
	CHECK( receipt.core.versionMajor == 4u && receipt.core.versionMinor >= 6u );
	CHECK( WiredSdlOpenGl46_ReceiptExact( &receipt, &receipt ) );
	CHECK( WiredSdlOpenGl46_BorrowCore( adapter, &receipt, &core, &coreReceipt )
		&& core && RalOpenGl_CoreReceiptExact( &receipt.core, &coreReceipt ) );
	CHECK( RalOpenGl_ProductCreate( core, &coreReceipt, 201u, &product ) );
	CHECK( RalOpenGl_ProductSetOutputExtent( product, 1280u, 720u ) );
	CHECK( RunContentFrame( product, "arena1", 301u, &arena1 ) );
	CHECK( WiredSdlOpenGl46_Present( adapter, &receipt, &presented ) );
	CHECK( presented.presentedFrames == 1u );
	CHECK( RunContentFrame( product, "arena17", 302u, &arena17 ) );
	CHECK( WiredSdlOpenGl46_Present( adapter, &presented, &recreated ) );
	CHECK( recreated.presentedFrames == 2u
		&& arena17.native.nativeDrawCount == arena1.native.nativeDrawCount );
	CHECK( !WiredSdlOpenGl46_Present( adapter, &receipt, &resized ) );
	CHECK( WiredSdlOpenGl46_RequestResize( adapter, &recreated, 1600u, 900u,
		&resized ) );
	CHECK( resized.host.logicalWidth == 1600u
		&& resized.host.logicalHeight == 900u
		&& resized.host.surfaceGeneration > presented.host.surfaceGeneration );
	loss = DeviceLoss();
	RalOpenGl_ProductDestroy( product ); product = NULL;
	CHECK( WiredSdlOpenGl46_RecreateAfterLoss( adapter, &resized, &loss,
		&recreated ) );
	CHECK( recreated.adapterGeneration > resized.adapterGeneration
		&& recreated.host.ownerGeneration > resized.host.ownerGeneration
		&& recreated.host.surfaceGeneration > resized.host.surfaceGeneration
		&& recreated.core.generation > resized.core.generation );
	CHECK( !WiredSdlOpenGl46_BorrowCore( adapter, &resized, &core, &coreReceipt ) );
	WiredSdlOpenGl46_Destroy( adapter );
	puts( contractOnly ? "SDL OpenGL 4.6 contract: ok"
		: "SDL OpenGL 4.6 1280x720 window smoke: ok" );
	return 0;
}
