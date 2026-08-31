// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_metal_present.h"
#include "ral_lighting_product.h"
#include "ral_presentation_policy.h"
#include "render_submission.h"
#include "maps/map_format_registry.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#x); return 1; } } while (0)

static ralIrradianceEntitySampleReceipt_t LocalIrradiance(
		uint64_t generation, uint64_t entityId ) {
	ralIrradianceEntitySampleReceipt_t receipt;
	memset( &receipt, 0, sizeof( receipt ) );
	receipt.schemaVersion = RAL_IRRADIANCE_ENTITY_RECEIPT_SCHEMA_VERSION;
	receipt.queryGeneration = generation;
	receipt.entityId = entityId;
	receipt.volumeId = 7u;
	receipt.layoutHash = 8u;
	receipt.probes.schemaVersion = RAL_IRRADIANCE_RECEIPT_SCHEMA_VERSION;
	receipt.probes.queryGeneration = generation;
	receipt.probes.productGeneration = 9u;
	receipt.probes.fallback = RAL_IRRADIANCE_FALLBACK_LIGHTGRID;
	receipt.probes.usedFallback = qtrue;
	receipt.probes.ready = qtrue;
	receipt.contributorHash = 10u;
	for ( uint32_t channel = 0u; channel < 3u; ++channel ) {
		receipt.blendedCoefficientsQ16[0][channel] = RAL_LIGHT_Q16_ONE;
		receipt.diffuseIrradianceQ16[channel] = RAL_LIGHT_Q16_ONE;
	}
	receipt.coefficientHash = Ral_IrradianceCoefficientHash(
		receipt.blendedCoefficientsQ16 );
	receipt.usedFallback = qtrue;
	receipt.ready = qtrue;
	return receipt;
}

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
	ralMetalDrawableReceipt_t drawableReceipt, drawableBefore, exactDrawable,
		persistentDrawable;
	ralMetalPresentReceipt_t presentReceipt, presentBefore, exactPresent,
		persistentReceipt;
	renderSubmissionReceipt_t submissionReceipt;
	ralMetalLighting_t *directionalLighting = NULL;
	ralMetalLightingReceipt_t directionalLightingReceipt;
	ralLightingProductRequest_t lightingRequest;
	ralLightingArtifactReceipt_t lightingArtifact;
	ralLightingRuntimePlan_t lightingPlan;
	ralStaticLightingCapabilities_t lightingCapabilities = {
		qtrue, qtrue, qtrue, qtrue };
	ralMemoryFailureEvent_t lossEvent;
	ralMemoryFailureReceipt_t lossReceipt;
	renderSubmissionState_t frontend;
	atmosphereFrameState_t atmosphere;
	mapFile_t world;
	dsurface_t worldSurface;
	drawVert_t worldVertices[3];
	int worldIndices[3] = { 0, 1, 2 };
	dshader_t worldShader;
	refdef_t worldView;
	refEntity_t modelEntity, spriteEntity, beamEntity;
	spriteDesc_t effectSprite;
	emitterDesc_t effectEmitter;
	decalDesc_t effectDecal;
	ribbonDesc_t effectRibbon;
	ribbonPoint_t effectRibbonPoints[2];
	beamDesc_t effectBeam;
	particleClass_t effectParticleClass;
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
	const ralLightVec3Q16_t indirectRadiance = {
		RAL_LIGHT_Q16_ONE, RAL_LIGHT_Q16_ONE, RAL_LIGHT_Q16_ONE };
	const ralLightVec3Q16_t dominantDirection = {
		0, 0, RAL_LIGHT_Q16_ONE };
	const uint8_t stationaryVisibility = 255u;
	uint8_t lightingRadianceBytes[8], lightingDirectionBytes[4];
	uint8_t lightingArtifactBytes[512];
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
	memset( &atmosphere, 0, sizeof( atmosphere ) );
	atmosphere.schemaVersion = WIRED_ATMOSPHERE_SCHEMA_VERSION;
	atmosphere.flags = ATMOSPHERE_FLAG_ENABLED
		| ATMOSPHERE_FLAG_VOLUMETRIC_MEDIA | ATMOSPHERE_FLAG_SKY_LIGHTING;
	atmosphere.qualityTier = ATMOSPHERE_QUALITY_ANALYTIC;
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
	CHECK( RenderSubmission_SetAtmosphere( &frontend, &atmosphere ) );
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
	{
		ralIrradianceEntitySampleReceipt_t local = LocalIrradiance( 75u, 1u );
		CHECK( RenderSubmission_AttachEntityIrradiance( &frontend, 0u, &local ) );
	}
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
	memset( &effectParticleClass, 0, sizeof( effectParticleClass ) );
	effectParticleClass.shader = uiMaterial;
	effectParticleClass.emitMode = EMIT_POINT;
	effectParticleClass.scatterShape = SCATTER_SPHERE;
	effectParticleClass.scatterMagnitude = 1.0f;
	effectParticleClass.velocityShape = VEL_AXIAL_PLUS_CUBE;
	effectParticleClass.axialSpeed = 8.0f;
	effectParticleClass.cubeJitter = 2.0f;
	effectParticleClass.lifetimeMean = 0.5f;
	effectParticleClass.colorPalette[0][0] = 1.0f;
	effectParticleClass.colorPalette[0][1] = 0.5f;
	effectParticleClass.colorPalette[0][3] = 1.0f;
	effectParticleClass.paletteCount = 1;
	effectParticleClass.colorEndMult[0] = 1.0f;
	effectParticleClass.colorEndMult[1] = 1.0f;
	effectParticleClass.colorEndMult[2] = 1.0f;
	effectParticleClass.sizeStart = 0.25f;
	effectParticleClass.sizeEnd = 0.5f;
	CHECK( RenderSubmission_RegisterParticleClass( &frontend, 1,
		&effectParticleClass ) );
	memset( &effectSprite, 0, sizeof( effectSprite ) );
	effectSprite.origin[0] = 8.0f; effectSprite.radius = 0.5f;
	effectSprite.rgba[0] = effectSprite.rgba[1] = effectSprite.rgba[2]
		= effectSprite.rgba[3] = 1.0f;
	effectSprite.shader = uiMaterial;
	CHECK( RenderSubmission_AddEffectSprite( &frontend, &effectSprite ) );
	memset( &effectEmitter, 0, sizeof( effectEmitter ) );
	effectEmitter.cls = 1; effectEmitter.count = 8;
	effectEmitter.origin[0] = 8.0f; effectEmitter.axis[2] = 1.0f;
	effectEmitter.colorTint[0] = effectEmitter.colorTint[1]
		= effectEmitter.colorTint[2] = effectEmitter.colorTint[3] = 1.0f;
	CHECK( RenderSubmission_AddEffectEmitter( &frontend, &effectEmitter ) );
	memset( &effectDecal, 0, sizeof( effectDecal ) );
	effectDecal.origin[0] = 9.0f; effectDecal.normal[0] = -1.0f;
	effectDecal.radius = 0.5f; effectDecal.shader = uiMaterial;
	effectDecal.rgba[0] = effectDecal.rgba[1] = effectDecal.rgba[2]
		= effectDecal.rgba[3] = 1.0f;
	effectDecal.lifetime = 1.0f;
	CHECK( RenderSubmission_AddEffectDecal( &frontend, &effectDecal ) );
	memset( effectRibbonPoints, 0, sizeof( effectRibbonPoints ) );
	for ( uint32_t point = 0u; point < 2u; ++point ) {
		effectRibbonPoints[point].pos[0] = 8.0f;
		effectRibbonPoints[point].pos[1] = point ? 1.0f : -1.0f;
		effectRibbonPoints[point].width = 0.25f;
		effectRibbonPoints[point].rgba[0] = 1.0f;
		effectRibbonPoints[point].rgba[3] = 1.0f;
	}
	memset( &effectRibbon, 0, sizeof( effectRibbon ) );
	effectRibbon.points = effectRibbonPoints;
	effectRibbon.numPoints = 2;
	effectRibbon.shader = uiMaterial;
	CHECK( RenderSubmission_AddEffectRibbon( &frontend, &effectRibbon ) );
	memset( &effectBeam, 0, sizeof( effectBeam ) );
	effectBeam.start[0] = effectBeam.end[0] = 8.0f;
	effectBeam.start[1] = -1.0f; effectBeam.end[1] = 1.0f;
	effectBeam.startWidth = 0.25f; effectBeam.endWidth = 0.1f;
	effectBeam.startColor[0] = effectBeam.startColor[3] = 1.0f;
	effectBeam.endColor[0] = effectBeam.endColor[3] = 1.0f;
	effectBeam.shader = uiMaterial; effectBeam.duration = 1.0f;
	effectBeam.fadeOut = 0.25f; effectBeam.axialCopies = 1;
	effectBeam.startEntityNum = effectBeam.endEntityNum = -1;
	CHECK( RenderSubmission_AddEffectBeam( &frontend, &effectBeam ) );
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
		&& presentReceipt.loweredEntityIndexCount == 69u
		&& presentReceipt.loweredEntityBatchCount == 7u
		&& presentReceipt.modelEntityCount == 1u
		&& presentReceipt.primitiveEntityCount == 2u
		&& presentReceipt.temporalEntityCount == 1u
		&& presentReceipt.localIrradianceEntityCount == 1u
		&& presentReceipt.localIrradianceDrawCount == 1u
		&& presentReceipt.unresolvedEntityCount == 0u
		&& presentReceipt.loweredEffectSpriteCount == 1u
		&& presentReceipt.loweredEffectDecalCount == 1u
		&& presentReceipt.loweredEffectRibbonCount == 1u
		&& presentReceipt.loweredEffectBeamCount == 1u
		&& presentReceipt.effectEmitterDispatchCount == 1u
		&& presentReceipt.effectParticleDrawCount == 8u
		&& presentReceipt.effectPrimitiveDroppedCount == 0u
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
	CHECK( RenderSubmission_EndFrame( &frontend, 75u, &submissionReceipt ) );
	CHECK( RenderSubmission_BeginFrame( &frontend, 76u ) );
	worldView.time = 250;
	CHECK( RenderSubmission_RenderScene( &frontend, &worldView, 0 ) );
	CHECK( RalMetal_PresentAcquire( present, &coreReceipt, &layerReceipt, 76u,
		&persistentDrawable ) );
	CHECK( RalMetal_PresentClearAndSubmit( present, &coreReceipt, &layerReceipt,
		&persistentDrawable, &frontend, sdrClear, 77u, &persistentReceipt ) );
	CHECK( persistentReceipt.loweredEntityIndexCount == 12u
		&& persistentReceipt.loweredEntityBatchCount == 2u
		&& persistentReceipt.loweredEffectSpriteCount == 0u
		&& persistentReceipt.loweredEffectDecalCount == 1u
		&& persistentReceipt.loweredEffectRibbonCount == 0u
		&& persistentReceipt.loweredEffectBeamCount == 1u
		&& persistentReceipt.effectEmitterDispatchCount == 0u
		&& persistentReceipt.effectParticleDrawCount == 8u
		&& persistentReceipt.effectPrimitiveDroppedCount == 0u );
	memset( &lightingRequest, 0, sizeof( lightingRequest ) );
	lightingRequest.schemaVersion = RAL_LIGHTING_PRODUCT_SCHEMA_VERSION;
	lightingRequest.artifactGeneration = 76u;
	lightingRequest.bake.schemaVersion = RAL_LIGHTING_BAKE_RECEIPT_SCHEMA_VERSION;
	lightingRequest.bake.bakeGeneration = 1u;
	lightingRequest.bake.staticIndirectKey = 2u;
	lightingRequest.bake.producerVersion = 3u;
	lightingRequest.bake.radianceHash = 4u;
	lightingRequest.bake.directionHash = 5u;
	lightingRequest.bake.patchCount = 1u;
	lightingRequest.bake.linkCount = 1u;
	lightingRequest.bake.completedBounces = 4u;
	lightingRequest.bake.ready = qtrue;
	lightingRequest.encoding = RAL_STATIC_LIGHTING_ENCODING_RGB9E5_OCT8;
	lightingRequest.pageWidth = lightingRequest.pageHeight = 1u;
	lightingRequest.pageCount = lightingRequest.texelCount = 1u;
	lightingRequest.indirectRadiance = &indirectRadiance;
	lightingRequest.dominantDirection = &dominantDirection;
	lightingRequest.stationaryVisibility = &stationaryVisibility;
	lightingRequest.stationaryVisibilityCount = 1u;
	CHECK( Ral_LightingProductWrite( &lightingRequest, lightingRadianceBytes,
		sizeof( lightingRadianceBytes ), lightingDirectionBytes,
		sizeof( lightingDirectionBytes ), lightingArtifactBytes,
		sizeof( lightingArtifactBytes ), &lightingArtifact ) );
	CHECK( Ral_LightingRuntimePlanBuild( RAL_BACKEND_METAL, 76u,
		lightingArtifactBytes, lightingArtifact.byteLength, &lightingArtifact,
		&lightingCapabilities, &lightingPlan ) );
	CHECK( RalMetal_LightingUpload( core, &coreReceipt, lightingArtifactBytes,
		lightingArtifact.byteLength, &lightingPlan, &directionalLighting,
		&directionalLightingReceipt ) );
	CHECK( RalMetal_PresentSetDirectionalLighting( present,
		&directionalLightingReceipt ) );
	CHECK( RalMetal_PresentAcquire( present, &coreReceipt, &layerReceipt, 78u,
		&drawableReceipt ) );
	CHECK( RalMetal_PresentClearAndSubmit( present, &coreReceipt, &layerReceipt,
		&drawableReceipt, &frontend, sdrClear, 79u, &presentReceipt ) );
	CHECK( presentReceipt.directionalStaticDrawCount == 1u
		&& presentReceipt.legacyLightmapDrawCount == 0u
		&& presentReceipt.surfaceLightingBindingDigest != 0u );
	CHECK( !RalMetal_PresentClearAndSubmit( present, &coreReceipt, &layerReceipt,
		&drawableReceipt, NULL, sdrClear, 80u, &presentBefore ) );
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
	MUTATE_PRESENT( legacyLightmapDrawCount );
	MUTATE_PRESENT( directionalStaticDrawCount );
	MUTATE_PRESENT( surfaceLightingBindingDigest );
	MUTATE_PRESENT( patchWorldBatchCount );
	MUTATE_PRESENT( maskedWorldBatchCount );
	MUTATE_PRESENT( blendedWorldBatchCount );
	MUTATE_PRESENT( depthWriteWorldBatchCount );
	MUTATE_PRESENT( loweredEntityIndexCount );
	MUTATE_PRESENT( loweredEntityBatchCount );
	MUTATE_PRESENT( modelEntityCount );
	MUTATE_PRESENT( primitiveEntityCount );
	MUTATE_PRESENT( temporalEntityCount );
	MUTATE_PRESENT( localIrradianceEntityCount );
	MUTATE_PRESENT( localIrradianceDrawCount );
	MUTATE_PRESENT( unresolvedEntityCount );
	MUTATE_PRESENT( loweredEffectSpriteCount );
	MUTATE_PRESENT( loweredEffectDecalCount );
	MUTATE_PRESENT( loweredEffectRibbonCount );
	MUTATE_PRESENT( loweredEffectBeamCount );
	MUTATE_PRESENT( effectEmitterDispatchCount );
	MUTATE_PRESENT( effectParticleDrawCount );
	MUTATE_PRESENT( effectPrimitiveDroppedCount );
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
	CHECK( RalMetal_PresentSetDirectionalLighting( present, NULL ) );
	CHECK( RalMetal_LightingDestroy( core, &coreReceipt, directionalLighting,
		&directionalLightingReceipt ) );
	directionalLighting = NULL;
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
