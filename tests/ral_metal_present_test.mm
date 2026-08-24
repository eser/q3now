// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_metal_present.h"
#include "ral_presentation_policy.h"
#include "render_submission.h"
#include "maps/map_format_registry.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#x); return 1; } } while (0)

int main( void ) {
	ralMetalCoreCreateInfo_t coreInfo = { 71u };
	ralMetalCore_t *core = NULL;
	ralMetalCoreReceipt_t coreReceipt, staleCore;
	ralSurfaceFormat_t sdrFormat = {
		RAL_FORMAT_B8G8R8A8_UNORM, RAL_COLORSPACE_SRGB_NONLINEAR };
	ralSurfaceFormat_t hdrFormat = {
		RAL_FORMAT_R16G16B16A16_SFLOAT, RAL_COLORSPACE_DISPLAY_P3 };
	ralSurfaceFormat_t unsupportedFormat = {
		RAL_FORMAT_A2B10G10R10_UNORM, RAL_COLORSPACE_HDR10_ST2084 };
	ralPresentPreference_t fifo = { RAL_PRESENT_FIFO, 3u, 3u };
	ralPresentationPolicyRequest_t unlockedRequest;
	ralPresentationPolicy_t unlockedPolicy;
	ralSwapchainCreateInfo_t createInfo;
	ralMetalPresent_t *present = (ralMetalPresent_t *)(uintptr_t)0x1234u;
	ralMetalPresentLayerReceipt_t layerReceipt, layerBefore, exactLayer;
	ralMetalDrawableReceipt_t drawableReceipt, drawableBefore, exactDrawable;
	ralMetalPresentReceipt_t presentReceipt, presentBefore, exactPresent;
	ralMemoryFailureEvent_t lossEvent;
	ralMemoryFailureReceipt_t lossReceipt;
	renderSubmissionState_t frontend;
	mapFile_t world;
	dsurface_t worldSurface;
	drawVert_t worldVertices[3];
	int worldIndices[3] = { 0, 1, 2 };
	dshader_t worldShader;
	refdef_t worldView;
	refEntity_t modelEntity, spriteEntity, beamEntity;
	refEntityMotion_t spriteMotion = { sizeof( spriteMotion ),
		REF_ENTITY_MOTION_VERSION, 17u, 3u,
		REF_ENTITY_MOTION_ROLE_GENERAL, 0u };
	const byte greenTexture[16] = {
		0u, 255u, 0u, 255u, 0u, 255u, 0u, 255u,
		0u, 255u, 0u, 255u, 0u, 255u, 0u, 255u
	};
	const byte neutralLightmap[4] = { 128u, 128u, 128u, 255u };
	char lightmapName[MAX_QPATH];
	qhandle_t uiMaterial, worldMaterial, lightmapMaterial, inlineModel;
	float sdrClear[4] = { 0.125f, 0.25f, 0.5f, 1.0f };
	float hdrClear[4] = { 2.0f, 1.25f, 0.5f, 1.0f };
	const byte *captureRgb;
	uint32_t captureWidth, captureHeight;
	{
		ralSurfaceFormat_t preferences[2] = { unsupportedFormat, hdrFormat };
		ralSurfaceFormatSelectionInfo_t query;
		ralSurfaceFormatSelection_t selected, before;
		memset( &query, 0, sizeof( query ) );
		query.preferences = preferences;
		query.preferenceCount = 2u;
		CHECK( Ral_SelectSurfaceFormat( (ralBackend_t *)(uintptr_t)1u,
			&query, &selected ) == ralSuccess );
		CHECK( selected.selectedPreference == 1u
			&& selected.selected.format == RAL_FORMAT_R16G16B16A16_SFLOAT
			&& selected.availableFormatCount == 2u );
		memset( &selected, 0x5a, sizeof( selected ) ); before = selected;
		query.backendExtensionChain = (const void *)(uintptr_t)1u;
		CHECK( Ral_SelectSurfaceFormat( (ralBackend_t *)(uintptr_t)1u,
			&query, &selected ) == ralErrorInvalidArgument );
		CHECK( !memcmp( &selected, &before, sizeof( selected ) ) );
	}
	CHECK( RalMetal_CoreCreate( &coreInfo, &core, &coreReceipt ) );
	CHECK( RenderSubmission_Init( &frontend, 71u ) );
	uiMaterial = RenderSubmission_RegisterMaterialImage( &frontend,
		RENDER_ASSET_MATERIAL, "textures/green", qtrue, greenTexture, 2u, 2u );
	CHECK( uiMaterial > 0 );
	memset( &world, 0, sizeof( world ) );
	memset( &worldSurface, 0, sizeof( worldSurface ) );
	memset( worldVertices, 0, sizeof( worldVertices ) );
	memset( &worldShader, 0, sizeof( worldShader ) );
	strcpy( world.name, "maps/metal-present-fixture.bsp" );
	world.checksum = 0x10203040;
	world.numSurfaces = 1; world.surfaces = &worldSurface;
	world.numDrawVerts = 3; world.drawVerts = worldVertices;
	world.numDrawIndexes = 3; world.drawIndexes = worldIndices;
	world.numShaders = 1; world.shaders = &worldShader;
	strcpy( worldShader.shader, "textures/world-green" );
	worldSurface.surfaceType = MST_PLANAR;
	worldSurface.numVerts = 3; worldSurface.numIndexes = 3;
	worldSurface.lightmapNum = 0;
	worldVertices[0].xyz[0] = worldVertices[1].xyz[0] = worldVertices[2].xyz[0] = 10.0f;
	worldVertices[0].xyz[1] = -1.0f; worldVertices[1].xyz[1] = 1.0f;
	worldVertices[2].xyz[2] = 1.0f;
	for ( int vertex = 0; vertex < 3; ++vertex ) {
		worldVertices[vertex].color.rgba[0] = worldVertices[vertex].color.rgba[3] = 255u;
		worldVertices[vertex].st[0] = vertex == 1 ? 1.0f : 0.0f;
		worldVertices[vertex].st[1] = vertex == 2 ? 1.0f : 0.0f;
		worldVertices[vertex].lightmap[0] = worldVertices[vertex].st[0];
		worldVertices[vertex].lightmap[1] = worldVertices[vertex].st[1];
	}
	worldMaterial = RenderSubmission_RegisterMaterialImage( &frontend,
		RENDER_ASSET_MATERIAL, worldShader.shader, qfalse,
		greenTexture, 2u, 2u );
	CHECK( worldMaterial > uiMaterial );
	CHECK( RenderSubmission_SetMaterialRasterPolicy( &frontend, worldMaterial,
		RENDER_ALPHA_BLEND, 0.25f, qfalse ) );
	CHECK( RenderSubmission_LightmapMaterialName( lightmapName,
		(uint32_t)world.checksum, 0 ) );
	lightmapMaterial = RenderSubmission_RegisterMaterialImage( &frontend,
		RENDER_ASSET_LIGHTMAP, lightmapName, qtrue, neutralLightmap, 1u, 1u );
	CHECK( lightmapMaterial > 0 );
	CHECK( RenderSubmission_LoadWorld( &frontend, &world, 0 ) );
	inlineModel = RenderSubmission_RegisterInlineModel( &frontend, "*1", 0u, 1u );
	CHECK( inlineModel > 0 );
	CHECK( RenderSubmission_BeginFrame( &frontend, 75u ) );
	memset( &worldView, 0, sizeof( worldView ) );
	worldView.width = 16; worldView.height = 16;
	worldView.fov_x = 90.0f; worldView.fov_y = 90.0f;
	worldView.viewaxis[0][0] = 1.0f; worldView.viewaxis[1][1] = 1.0f;
	worldView.viewaxis[2][2] = 1.0f;
	CHECK( RenderSubmission_RenderScene( &frontend, &worldView, 0 ) );
	memset( &modelEntity, 0, sizeof( modelEntity ) );
	modelEntity.reType = RT_MODEL; modelEntity.hModel = inlineModel;
	modelEntity.axis[0][0] = modelEntity.axis[1][1] = modelEntity.axis[2][2] = 1.0f;
	memset( modelEntity.shader.rgba, 255, sizeof( modelEntity.shader.rgba ) );
	CHECK( RenderSubmission_AddEntity( &frontend, &modelEntity, NULL ) );
	memset( &spriteEntity, 0, sizeof( spriteEntity ) );
	spriteEntity.reType = RT_SPRITE; spriteEntity.origin[0] = 8.0f;
	spriteEntity.rotation = 45.0f;
	spriteEntity.radius = 1.0f; spriteEntity.customShader = uiMaterial;
	memset( spriteEntity.shader.rgba, 255, sizeof( spriteEntity.shader.rgba ) );
	CHECK( RenderSubmission_AddEntity( &frontend, &spriteEntity, &spriteMotion ) );
	memset( &beamEntity, 0, sizeof( beamEntity ) );
	beamEntity.reType = RT_BEAM; beamEntity.origin[0] = 8.0f;
	beamEntity.origin[1] = -1.0f; beamEntity.oldorigin[0] = 8.0f;
	beamEntity.oldorigin[1] = 1.0f; beamEntity.customShader = uiMaterial;
	memset( beamEntity.shader.rgba, 255, sizeof( beamEntity.shader.rgba ) );
	CHECK( RenderSubmission_AddEntity( &frontend, &beamEntity, NULL ) );
	{
		const float white[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
		CHECK( RenderSubmission_SetColor( &frontend, white ) );
		CHECK( RenderSubmission_AddUiQuad( &frontend, 0.0f, 0.0f, 16.0f, 16.0f,
			0.0f, 0.0f, 1.0f, 1.0f, 0.0f, uiMaterial ) );
	}
	memset( &createInfo, 0, sizeof( createInfo ) );
	createInfo.desiredWidth = 16u; createInfo.desiredHeight = 16u;
	createInfo.formatPreferences = &sdrFormat; createInfo.formatPreferenceCount = 1u;
	createInfo.presentPreferences = &fifo; createInfo.presentPreferenceCount = 1u;
	createInfo.requiredUsage = RAL_TEXTURE_USAGE_COLOR_ATTACHMENT;
	memset( &layerReceipt, 0x5a, sizeof( layerReceipt ) ); layerBefore = layerReceipt;
	createInfo.desiredWidth = 0u;
	CHECK( !RalMetal_PresentCreate( core, &coreReceipt, &createInfo, 72u,
		&present, &layerReceipt ) );
	CHECK( present == (ralMetalPresent_t *)(uintptr_t)0x1234u
		&& memcmp( &layerReceipt, &layerBefore, sizeof( layerReceipt ) ) == 0 );
	createInfo.desiredWidth = 16u; createInfo.formatPreferences = &unsupportedFormat;
	CHECK( !RalMetal_PresentCreate( core, &coreReceipt, &createInfo, 72u,
		&present, &layerReceipt ) );
	createInfo.formatPreferences = &sdrFormat; fifo.desiredImageCount = 4u;
	CHECK( !RalMetal_PresentCreate( core, &coreReceipt, &createInfo, 72u,
		&present, &layerReceipt ) );
	fifo.desiredImageCount = 3u;
	fifo.unboundedImageCount = 5u;
	CHECK( !RalMetal_PresentCreate( core, &coreReceipt, &createInfo, 72u,
		&present, &layerReceipt ) );
	fifo.unboundedImageCount = 3u;
	ralPresentPreference_t unsupportedMode = { RAL_PRESENT_MAILBOX, 3u, 3u };
	createInfo.presentPreferences = &unsupportedMode;
	CHECK( !RalMetal_PresentCreate( core, &coreReceipt, &createInfo, 72u,
		&present, &layerReceipt ) );
	createInfo.presentPreferences = &fifo;
	createInfo.requiredUsage = RAL_TEXTURE_USAGE_SAMPLED;
	CHECK( !RalMetal_PresentCreate( core, &coreReceipt, &createInfo, 72u,
		&present, &layerReceipt ) );
	createInfo.requiredUsage = RAL_TEXTURE_USAGE_COLOR_ATTACHMENT;
	CHECK( RalMetal_PresentCreate( core, &coreReceipt, &createInfo, 72u,
		&present, &layerReceipt ) );
	CHECK( layerReceipt.selected.format == RAL_FORMAT_B8G8R8A8_UNORM
		&& layerReceipt.selected.colorSpace == RAL_COLORSPACE_SRGB_NONLINEAR
		&& layerReceipt.selected.presentMode == RAL_PRESENT_FIFO
		&& layerReceipt.displaySyncEnabled == qtrue
		&& layerReceipt.extendedDynamicRange == qfalse
		&& layerReceipt.ownsLayer == qtrue );
	exactLayer = layerReceipt;
	CHECK( RalMetal_PresentLayerReceiptExact( &layerReceipt, &exactLayer ) );
#define MUTATE_LAYER(field) do { exactLayer = layerReceipt; exactLayer.field++; \
	CHECK( !RalMetal_PresentLayerReceiptExact( &layerReceipt, &exactLayer ) ); } while (0)
	MUTATE_LAYER( coreGeneration ); MUTATE_LAYER( presentationGeneration );
	MUTATE_LAYER( selected.width ); MUTATE_LAYER( selected.imageCount );
	MUTATE_LAYER( selected.requestedImageCount );
#undef MUTATE_LAYER
	exactLayer = layerReceipt; exactLayer.ownsLayer = qfalse;
	CHECK( !RalMetal_PresentLayerReceiptExact( &layerReceipt, &exactLayer ) );
	exactLayer = layerReceipt; exactLayer.ownerIdentity = exactLayer.layerIdentity;
	CHECK( !RalMetal_PresentLayerReceiptExact( &exactLayer, &exactLayer ) );

	memset( &drawableReceipt, 0x5a, sizeof( drawableReceipt ) );
	drawableBefore = drawableReceipt;
	staleCore = coreReceipt; staleCore.generation++;
	CHECK( !RalMetal_PresentAcquire( present, &staleCore, &layerReceipt, 73u,
		&drawableReceipt ) );
	exactLayer = layerReceipt; exactLayer.presentationGeneration++;
	CHECK( !RalMetal_PresentAcquire( present, &coreReceipt, &exactLayer, 73u,
		&drawableReceipt ) );
	CHECK( RalMetal_PresentAcquire( present, &coreReceipt, &layerReceipt, 73u,
		&drawableReceipt ) );
	CHECK( drawableReceipt.width == 16u && drawableReceipt.height == 16u
		&& drawableReceipt.format == RAL_FORMAT_B8G8R8A8_UNORM );
	CHECK( !RalMetal_PresentAcquire( present, &coreReceipt, &layerReceipt, 74u,
		&drawableBefore ) );
	CHECK( !RalMetal_PresentReconfigure( present, &coreReceipt, &layerReceipt,
		&createInfo, 74u, &exactLayer ) );
	exactDrawable = drawableReceipt;
	CHECK( RalMetal_DrawableReceiptExact( &drawableReceipt, &exactDrawable ) );
	exactDrawable.textureIdentity++;
	CHECK( !RalMetal_DrawableReceiptExact( &drawableReceipt, &exactDrawable ) );
	memset( &presentReceipt, 0x5a, sizeof( presentReceipt ) ); presentBefore = presentReceipt;
	CHECK( !RalMetal_PresentClearAndSubmit( present, &coreReceipt, &layerReceipt,
		&exactDrawable, NULL, sdrClear, 75u, &presentReceipt ) );
	CHECK( memcmp( &presentReceipt, &presentBefore, sizeof( presentReceipt ) ) == 0 );
	sdrClear[0] = NAN;
	CHECK( !RalMetal_PresentClearAndSubmit( present, &coreReceipt, &layerReceipt,
		&drawableReceipt, NULL, sdrClear, 75u, &presentReceipt ) );
	sdrClear[0] = 0.125f;
	CHECK( memcmp( &presentReceipt, &presentBefore, sizeof( presentReceipt ) ) == 0 );
	CHECK( RalMetal_PresentRequestCapture( present ) );
	CHECK( !RalMetal_PresentRequestCapture( present ) );
	CHECK( RalMetal_PresentClearAndSubmit( present, &coreReceipt, &layerReceipt,
		&drawableReceipt, &frontend, sdrClear, 75u, &presentReceipt ) );
	captureRgb = RalMetal_PresentCaptureRgb( present, &captureWidth, &captureHeight );
	CHECK( captureRgb && captureWidth == 16u && captureHeight == 16u
		&& captureRgb[( 7u * 16u + 8u ) * 3u + 0u] == 0u
		&& captureRgb[( 7u * 16u + 8u ) * 3u + 1u] == 255u
		&& captureRgb[( 7u * 16u + 8u ) * 3u + 2u] == 0u );
	CHECK( presentReceipt.presented == qtrue
		&& presentReceipt.submission.commands[0].state == RAL_COMMAND_SUBMITTED
		&& presentReceipt.clearDigest != 0u
		&& presentReceipt.readbackDigest != 0u
		&& presentReceipt.loweredWorldIndexCount == 3u
		&& presentReceipt.loweredWorldBatchCount == 1u
		&& presentReceipt.texturedWorldBatchCount == 1u
		&& presentReceipt.lightmappedWorldBatchCount == 1u
		&& presentReceipt.patchWorldBatchCount == 0u
		&& presentReceipt.maskedWorldBatchCount == 0u
		&& presentReceipt.blendedWorldBatchCount == 1u
		&& presentReceipt.depthWriteWorldBatchCount == 0u
		&& presentReceipt.loweredEntityIndexCount == 45u
		&& presentReceipt.loweredEntityBatchCount == 3u
		&& presentReceipt.modelEntityCount == 1u
		&& presentReceipt.primitiveEntityCount == 2u
		&& presentReceipt.temporalEntityCount == 1u
		&& presentReceipt.unresolvedEntityCount == 0u
		&& presentReceipt.loweredUiPrimitiveCount == 1u
		&& presentReceipt.texturedUiPrimitiveCount == 1u
		&& presentReceipt.readbackX == 8u && presentReceipt.readbackY == 8u
		&& presentReceipt.readbackByteCount == 4u );
	if ( presentReceipt.readbackBytes[0] != 0u
			|| presentReceipt.readbackBytes[1] != 255u
			|| presentReceipt.readbackBytes[2] != 0u
			|| presentReceipt.readbackBytes[3] != 255u )
		fprintf( stderr, "textured readback: %u %u %u %u\n",
			presentReceipt.readbackBytes[0], presentReceipt.readbackBytes[1],
			presentReceipt.readbackBytes[2], presentReceipt.readbackBytes[3] );
	CHECK( presentReceipt.readbackBytes[0] == 0u
		&& presentReceipt.readbackBytes[1] == 255u
		&& presentReceipt.readbackBytes[2] == 0u
		&& presentReceipt.readbackBytes[3] == 255u );
	CHECK( !RalMetal_PresentClearAndSubmit( present, &coreReceipt, &layerReceipt,
		&drawableReceipt, NULL, sdrClear, 76u, &presentBefore ) );
	CHECK( !RalMetal_PresentAcquire( present, &coreReceipt, &layerReceipt, 73u,
		&drawableBefore ) );
	exactPresent = presentReceipt;
	CHECK( RalMetal_PresentReceiptExact( &presentReceipt, &exactPresent ) );
#define MUTATE_PRESENT(field) do { exactPresent = presentReceipt; exactPresent.field++; \
	CHECK( !RalMetal_PresentReceiptExact( &presentReceipt, &exactPresent ) ); } while (0)
	MUTATE_PRESENT( coreGeneration ); MUTATE_PRESENT( presentationGeneration );
	MUTATE_PRESENT( acquireGeneration ); MUTATE_PRESENT( presentGeneration );
	MUTATE_PRESENT( completionGeneration ); MUTATE_PRESENT( submission.generation );
	MUTATE_PRESENT( clearDigest ); MUTATE_PRESENT( readbackDigest );
	MUTATE_PRESENT( loweredWorldBatchCount );
	MUTATE_PRESENT( texturedWorldBatchCount );
	MUTATE_PRESENT( lightmappedWorldBatchCount );
	MUTATE_PRESENT( patchWorldBatchCount );
	MUTATE_PRESENT( maskedWorldBatchCount );
	MUTATE_PRESENT( blendedWorldBatchCount );
	MUTATE_PRESENT( depthWriteWorldBatchCount );
	MUTATE_PRESENT( loweredEntityIndexCount );
	MUTATE_PRESENT( loweredEntityBatchCount );
	MUTATE_PRESENT( modelEntityCount );
	MUTATE_PRESENT( primitiveEntityCount );
	MUTATE_PRESENT( temporalEntityCount );
	MUTATE_PRESENT( unresolvedEntityCount );
	MUTATE_PRESENT( texturedUiPrimitiveCount );
	MUTATE_PRESENT( msdfUiPrimitiveCount );
#undef MUTATE_PRESENT

	createInfo.desiredWidth = 32u; createInfo.desiredHeight = 16u;
	createInfo.formatPreferences = &hdrFormat;
	CHECK( Ral_PresentationPolicyRequestFromLegacySwapInterval(
		0, 2u, &unlockedRequest ) );
	CHECK( Ral_ResolvePresentationPolicy( &unlockedRequest, &unlockedPolicy ) );
	CHECK( unlockedPolicy.preferenceCount == 5u
		&& unlockedPolicy.preferences[4].mode == RAL_PRESENT_FIFO
		&& unlockedPolicy.preferences[4].unboundedImageCount == 4u );
	createInfo.presentPreferences = unlockedPolicy.preferences;
	createInfo.presentPreferenceCount = unlockedPolicy.preferenceCount;
	CHECK( RalMetal_PresentReconfigure( present, &coreReceipt, &layerReceipt,
		&createInfo, 80u, &layerReceipt ) );
	CHECK( layerReceipt.selected.format == RAL_FORMAT_R16G16B16A16_SFLOAT
		&& layerReceipt.selected.colorSpace == RAL_COLORSPACE_DISPLAY_P3
		&& layerReceipt.selected.presentMode == RAL_PRESENT_IMMEDIATE
		&& layerReceipt.displaySyncEnabled == qfalse
		&& layerReceipt.extendedDynamicRange == qtrue
		&& layerReceipt.selected.width == 32u && layerReceipt.selected.imageCount == 2u );
	CHECK( !RalMetal_PresentReconfigure( present, &coreReceipt, &layerReceipt,
		&createInfo, 80u, &exactLayer ) );
	CHECK( RalMetal_PresentAcquire( present, &coreReceipt, &layerReceipt, 81u,
		&drawableReceipt ) );
	CHECK( drawableReceipt.format == RAL_FORMAT_R16G16B16A16_SFLOAT );
	CHECK( !RalMetal_PresentRequestCapture( present ) );
	CHECK( RalMetal_PresentClearAndSubmit( present, &coreReceipt, &layerReceipt,
		&drawableReceipt, NULL, hdrClear, 82u, &presentReceipt ) );
	CHECK( presentReceipt.readbackDigest != 0u
		&& presentReceipt.readbackByteCount == 8u );
	CHECK( presentReceipt.colorSpace == RAL_COLORSPACE_DISPLAY_P3
		&& presentReceipt.presentMode == RAL_PRESENT_IMMEDIATE );
	CHECK( RalMetal_PresentAcquire( present, &coreReceipt, &layerReceipt, 83u,
		&drawableReceipt ) );
	CHECK( !RalMetal_PresentClearAndSubmit( present, &coreReceipt, &layerReceipt,
		&drawableReceipt, NULL, hdrClear, 82u, &presentBefore ) );
	memset( &lossEvent, 0, sizeof( lossEvent ) );
	lossEvent.backendType = RAL_BACKEND_METAL;
	lossEvent.cause = RAL_MEMORY_FAILURE_DEVICE_LOST;
	lossEvent.memoryClass = RAL_ALLOCATION_DEVICE_LOCAL;
	lossEvent.residency = RAL_ALLOCATION_RESIDENCY_PERMANENT;
	lossEvent.criticality = RAL_MEMORY_CRITICALITY_REQUIRED;
	lossEvent.requestedBytes = 64u; lossEvent.attempt = 1u;
	lossEvent.maxAttempts = 1u; lossEvent.liveParent = qtrue;
	CHECK( RalMetal_CorePublishDeviceLoss( core, &lossEvent, &lossReceipt ) );
	CHECK( !RalMetal_PresentClearAndSubmit( present, &coreReceipt, &layerReceipt,
		&drawableReceipt, NULL, hdrClear, 84u, &presentBefore ) );
	RenderSubmission_CancelFrame( &frontend );
	RalMetal_PresentDestroy( present );
	RalMetal_CoreDestroy( core );
	puts( "ral metal presentation: PASS" );
	return 0;
}
