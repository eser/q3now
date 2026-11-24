// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef WIRED_RAL_WEBGPU_PRODUCT_H
#define WIRED_RAL_WEBGPU_PRODUCT_H

#include "ral_webgpu_frontend.h"
#include "ral_webgpu_weather.h"
#include "ral_webgpu_exposure.h"
#include "render_material_package.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RAL_WEBGPU_PRODUCT_SCHEMA_VERSION 12u

/* Adapter-private std430 draw data. Record zero is an ordinary primitive;
 * beams use float endpoint colours and an unmodified [0,1] quad on the GPU. */
typedef struct {
	float startColor[4], endColor[4];
	float scaleOffset[4];
	uint32_t meta[4];
} ralWebGpuEffectDrawParams_t;
#define RAL_WEBGPU_EFFECT_PARAMETER_CAPACITY \
	(1u + RENDER_SUBMISSION_MAX_EFFECT_BEAMS * RENDER_MATERIAL_MAX_STAGES)
#define RAL_WEBGPU_EFFECT_PARAMETER_BYTES \
	((uint64_t)RAL_WEBGPU_EFFECT_PARAMETER_CAPACITY * sizeof(ralWebGpuEffectDrawParams_t))

#define RAL_WEBGPU_EFFECT_PIPELINE_COUNT 5u
typedef enum {
	RAL_WEBGPU_EFFECT_PIPELINE_LEGACY,
	RAL_WEBGPU_EFFECT_PIPELINE_ALPHA_DEPTH,
	RAL_WEBGPU_EFFECT_PIPELINE_ADDITIVE_DEPTH,
	RAL_WEBGPU_EFFECT_PIPELINE_ALPHA_NO_DEPTH,
	RAL_WEBGPU_EFFECT_PIPELINE_ADDITIVE_NO_DEPTH
} ralWebGpuEffectPipeline_t;

typedef struct ralWebGpuProduct_s ralWebGpuProduct_t;

/* GPU vertex of the WebGPU world pipeline: the authored frontend vertex
 * (position, base UV, lightmap UV, colour, normal) kept intact, followed by
 * the CPU-projected clip W and analytic fog coordinates. Earlier the
 * projection overwrote the UVs in place, which left the world untextured. */
typedef struct {
	renderWorldVertex_t authored;
	float clipW;
	float fogCoord[2];
} ralWebGpuWorldVertex_t;

#define RAL_WEBGPU_SKY_RADIANCE_RESOURCES_SCHEMA_VERSION 1u

typedef struct {
	uint32_t schemaVersion;
	ralBackendType_t backendType;
	uint64_t productGeneration;
	uint64_t worldLifecycleGeneration;
	uintptr_t sourceTextureIdentity;
	uintptr_t currentIrradianceIdentity;
	uintptr_t targetIrradianceIdentity;
	uintptr_t currentRadianceIdentity;
	uintptr_t targetRadianceIdentity;
	uintptr_t currentShIdentity;
	uintptr_t targetShIdentity;
	uintptr_t authoredTextureIdentities[RAL_MATERIAL_SKY_FACE_COUNT];
	uintptr_t authoredSamplerIdentity;
	qboolean authoredReady;
} ralWebGpuSkyRadianceResources_t;

typedef struct {
	uint32_t schemaVersion;
	ralBackendType_t backendType;
	uint64_t backendGeneration;
	uint64_t productGeneration;
	uint64_t frameGeneration;
	uintptr_t targetIdentity;
	ralWebGpuFrontendPlanReceipt_t plan;
	renderEffectDecalPoolReceipt_t effectDecalPool;
	renderEffectBeamPoolReceipt_t effectBeamPool;
	ralWebGpuResourceReceipt_t worldVertexBuffer;
	ralWebGpuResourceReceipt_t worldIndexBuffer;
	ralWebGpuWriteReceipt_t worldVertexWrite;
	ralWebGpuWriteReceipt_t worldIndexWrite;
	ralWebGpuResourceReceipt_t distantWorldVertexBuffer;
	ralWebGpuResourceReceipt_t distantWorldIndexBuffer;
	ralWebGpuWriteReceipt_t distantWorldVertexWrite;
	ralWebGpuWriteReceipt_t distantWorldIndexWrite;
	ralWebGpuResourceReceipt_t worldMaterialBuffer;
	ralWebGpuWriteReceipt_t worldMaterialWrite;
	ralWebGpuResourceReceipt_t entityVertexBuffer;
	ralWebGpuResourceReceipt_t entityIndexBuffer;
	ralWebGpuResourceReceipt_t entityLightingBuffer;
	ralWebGpuWriteReceipt_t entityVertexWrite;
	ralWebGpuWriteReceipt_t entityIndexWrite;
	ralWebGpuWriteReceipt_t entityLightingWrite;
	ralWebGpuResourceReceipt_t miscVertexBuffer;
	ralWebGpuResourceReceipt_t miscIndexBuffer;
	ralWebGpuWriteReceipt_t miscVertexWrite;
	ralWebGpuWriteReceipt_t miscIndexWrite;
	ralWebGpuResourceReceipt_t effectParameterBuffer;
	ralWebGpuWriteReceipt_t effectParameterWrite;
	uint32_t effectParameterCount;
	uint32_t effectBeamStageDrawCount;
	ralWebGpuCommandReceipt_t command;
	ralWebGpuSubmissionReceipt_t submission;
	uint32_t uploadedMaterialCount;
	uint32_t reusedMaterialCount;
	uint32_t worldDrawCount;
	uint32_t worldEmissiveDrawCount;
	uint32_t distantWorldCandidateCount;
	uint32_t distantWorldDrawCount;
	uint32_t distantWorldIndexCount;
	qboolean distantWorldDepthReset;
	uint32_t modelEntityCount;
	uint32_t primitiveEntityCount;
	uint32_t temporalEntityCount;
	uint32_t entityDrawCount;
	uint32_t localIrradianceEntityCount;
	uint32_t localIrradianceDrawCount;
	uint32_t polygonDrawCount;
	uint32_t lightDrawCount;
	uint32_t effectSpriteDrawCount;
	uint32_t effectDecalDrawCount;
	uint32_t effectRibbonDrawCount;
	uint32_t effectBeamDrawCount;
	uint32_t uiDrawCount;
    uint32_t toneMapDrawCount;
    uint32_t exposureDispatchCount;
    uint32_t aaDispatchCount;      /* TAA: motion + resolve (or bootstrap) in one compute pass, 0 without TAA */
	uint32_t atmosphereBoundDrawCount;
	ralAtmosphereWeatherReceipt_t weather;
	uint32_t weatherDrawCount;
	uint32_t deferredNonWorldDrawCount;
	uint32_t unresolvedCount;
	uint32_t fallbackCount;
	uint32_t fatalCount;
	qboolean ready;
} ralWebGpuProductFrameReceipt_t;

qboolean RalWebGpu_ProductCreate( ralWebGpuRuntime_t *runtime,
	const ralWebGpuRuntimeReceipt_t *runtimeReceipt,
	const ralWebGpuPipelineReceipt_t *worldPipeline, uint64_t generation,
	ralWebGpuProduct_t **outProduct );
qboolean RalWebGpu_ProductGetEffectParameterBuffer(
	const ralWebGpuProduct_t *product,
	const ralWebGpuRuntimeReceipt_t *runtimeReceipt,
	uintptr_t *outIdentity, uint64_t *outByteSize );
/* TAA (r_aa 1): created once with the product's owners; SetTaa follows the cvar each frame. */
qboolean RalWebGpu_ProductEnableTaa( ralWebGpuProduct_t *product,
	ralWebGpuRuntimeReceipt_t *rr, ralWebGpuBrowserBridge_t *bridge, uint64_t generation );
qboolean RalWebGpu_ProductEnableSmaa( ralWebGpuProduct_t *product,
	ralWebGpuRuntimeReceipt_t *rr, ralWebGpuBrowserBridge_t *bridge, uint64_t generation );
/* r_aa: 0 off, 1 TAA, 2 SMAA; smaaQuality is r_smaa (0 selects 3). Call between frames. */
qboolean RalWebGpu_ProductSetAntiAliasing( ralWebGpuProduct_t *product, ralWebGpuRuntimeReceipt_t *rr, int mode,
	int smaaQuality );
void RalWebGpu_ProductTaaFrameCounts( const ralWebGpuProduct_t *product, uint32_t *resolved, uint32_t *bootstrap );
qboolean RalWebGpu_ProductEnableExposure( ralWebGpuProduct_t *product,
    ralWebGpuRuntimeReceipt_t *rr, ralWebGpuBrowserBridge_t *bridge, uint64_t generation );
qboolean RalWebGpu_ProductEnableSkyMaterial(ralWebGpuProduct_t *product,
    ralWebGpuRuntimeReceipt_t *rr, ralWebGpuBrowserBridge_t *bridge, uint64_t generation);
qboolean RalWebGpu_ProductSetPhysicalSky(ralWebGpuProduct_t *product,
    uintptr_t texture, uintptr_t sampler, float blend);
// Capture uses independent uniform slots. End only after cancellation or the
// sky executor's completed submit; until then registration/unload is busy.
qboolean RalWebGpu_ProductSkyCaptureBegin(ralWebGpuProduct_t *product,
    const ralWebGpuRuntimeReceipt_t *rr, const renderSubmissionState_t *frontend,
    const renderMaterialSnapshot_t *material, int timeMs, uint32_t faceMask,
    uintptr_t physicalTexture, uintptr_t physicalSampler, float physicalBlend);
qboolean RalWebGpu_ProductSkyCaptureFace(ralWebGpuProduct_t *product,
    const ralWebGpuRuntimeReceipt_t *rr, ralWebGpuCommand_t *command, uint32_t face,
    const ralWebGpuCommandReceipt_t *recording, ralWebGpuCommandReceipt_t *updated);
void RalWebGpu_ProductSkyCaptureEnd(ralWebGpuProduct_t *product);
qboolean RalWebGpu_ProductSetExposure( ralWebGpuProduct_t *product,
    const ralExposureSettings_t *settings, const ralDisplayVisibilityPlan_t *visibility,
    int toneMode, float toneExposure );
/* The TAA effect mask pipelines: entity coverage (invalidate) and object motion (rigid and IQM entities with a previous pose). */
qboolean RalWebGpu_ProductSetTaaMaskPipeline( ralWebGpuProduct_t *product,
	const ralWebGpuRuntimeReceipt_t *runtimeReceipt, const ralWebGpuPipelineReceipt_t *maskPipeline,
	const ralWebGpuPipelineReceipt_t *motionPipeline );
/* Translucent entity pipelines (alpha blend, alpha-additive, additive) and their TAA mask pipelines, in that order. */
qboolean RalWebGpu_ProductSetEntityBlendPipelines( ralWebGpuProduct_t *product,
	const ralWebGpuRuntimeReceipt_t *runtimeReceipt,
	const ralWebGpuPipelineReceipt_t entityBlend[3], const ralWebGpuPipelineReceipt_t taaTrust[3] );
qboolean RalWebGpu_ProductSetEntityPipeline( ralWebGpuProduct_t *product,
	const ralWebGpuRuntimeReceipt_t *runtimeReceipt,
	const ralWebGpuPipelineReceipt_t *entityPipeline );
qboolean RalWebGpu_ProductGetWorldMaterialBuffer(
	const ralWebGpuProduct_t *product,
	const ralWebGpuRuntimeReceipt_t *runtimeReceipt,
	uintptr_t *outIdentity, uint64_t *outByteSize );
qboolean RalWebGpu_ProductSetWorldMaterialBindGroup(
	ralWebGpuProduct_t *product,
	const ralWebGpuRuntimeReceipt_t *runtimeReceipt,
	uintptr_t bindGroupIdentity );
qboolean RalWebGpu_ProductGetEntityLightingBuffer(
	const ralWebGpuProduct_t *product,
	const ralWebGpuRuntimeReceipt_t *runtimeReceipt,
	uintptr_t *outIdentity, uint64_t *outByteSize );
qboolean RalWebGpu_ProductSetEntityLightingBindGroup(
	ralWebGpuProduct_t *product,
	const ralWebGpuRuntimeReceipt_t *runtimeReceipt,
	uintptr_t bindGroupIdentity );
qboolean RalWebGpu_ProductSetEffectPipelines( ralWebGpuProduct_t *product,
	const ralWebGpuRuntimeReceipt_t *runtimeReceipt,
	const ralWebGpuPipelineReceipt_t
		effectPipelines[RAL_WEBGPU_EFFECT_PIPELINE_COUNT] );
qboolean RalWebGpu_ProductSetUiPipelines( ralWebGpuProduct_t *product,
	const ralWebGpuRuntimeReceipt_t *runtimeReceipt,
	const ralWebGpuPipelineReceipt_t *uiPipeline,
	const ralWebGpuPipelineReceipt_t *msdfPipeline );
qboolean RalWebGpu_ProductSetAtmosphereBindGroups(
	ralWebGpuProduct_t *product,
	const ralWebGpuRuntimeReceipt_t *runtimeReceipt,
	uintptr_t worldBindGroup, uintptr_t entityBindGroup,
	const uintptr_t effectBindGroups[RAL_WEBGPU_EFFECT_PIPELINE_COUNT] );
qboolean RalWebGpu_ProductSetViewport( ralWebGpuProduct_t *product,
	const ralWebGpuRuntimeReceipt_t *runtimeReceipt,
	uint32_t width, uint32_t height );
qboolean RalWebGpu_ProductSetLightmapBoost( ralWebGpuProduct_t *product,
	float lightmapBoost );
qboolean RalWebGpu_ProductSetWeather( ralWebGpuProduct_t *product,
	const ralWebGpuRuntimeReceipt_t *runtimeReceipt,
	const ralAtmosphereWeatherReceipt_t *plan,
	const ralWebGpuWeatherReceipt_t *execution );
qboolean RalWebGpu_ProductEnsureSkyRadianceResources(
	ralWebGpuProduct_t *product,
	const ralWebGpuRuntimeReceipt_t *runtimeReceipt,
	uint64_t worldLifecycleGeneration );
qboolean RalWebGpu_ProductUnloadSkyRadianceResources(
	ralWebGpuProduct_t *product,
	const ralWebGpuRuntimeReceipt_t *runtimeReceipt );
/* Read-only resident handle view: never uploads or replaces borrowed images.
 * PrepareMaterialResources must precede capture admission. */
qboolean RalWebGpu_ProductSkyRadianceResources(
	ralWebGpuProduct_t *product,
	const ralWebGpuRuntimeReceipt_t *runtimeReceipt,
	const renderSubmissionState_t *frontend,
	ralWebGpuSkyRadianceResources_t *outResources );
qboolean RalWebGpu_ProductPublishSkyRadiance(
	ralWebGpuProduct_t *product,
	const ralWebGpuRuntimeReceipt_t *runtimeReceipt );
void RalWebGpu_ProductDestroy( ralWebGpuProduct_t *product,
	const ralWebGpuRuntimeReceipt_t *runtimeReceipt );
qboolean RalWebGpu_ProductUnloadWorld( ralWebGpuProduct_t *product,
		const ralWebGpuRuntimeReceipt_t *runtimeReceipt, int worldIndex );

/* Requires completed submissions and no resident world; keeps pipelines. */
qboolean RalWebGpu_ProductResetRegistrationAssets( ralWebGpuProduct_t *product,
	const ralWebGpuRuntimeReceipt_t *runtimeReceipt, renderSubmissionState_t *frontend );
qboolean RalWebGpu_ProductResolveMaterialRuntimeResource( void *product,
	uint64_t cacheKeyHash, ralMaterialRuntimeResource_t *outResource );
qboolean RalWebGpu_ProductPrepareMaterialResources(
	ralWebGpuProduct_t *product,
	const ralWebGpuRuntimeReceipt_t *runtimeReceipt,
	const renderSubmissionState_t *frontend );
qboolean RalWebGpu_ProductSetMaterialRuntimeCatalog(
	ralWebGpuProduct_t *product,
	const renderMaterialRuntimeCatalog_t *catalog,
	renderSubmissionState_t *frontend );
qboolean RalWebGpu_ProductPlan( ralWebGpuProduct_t *product,
	const ralWebGpuRuntimeReceipt_t *runtimeReceipt,
	const renderSubmissionState_t *frontend,
	const renderSubmissionReceipt_t *submission,
	ralWebGpuFrontendPlanReceipt_t *outPlan );
/* tan(fovX/2), tan(fovY/2), near plane, finite-far depth scale.
 * Keep geometry's division order unchanged for existing frame fidelity. */
qboolean RalWebGpu_ProductProjection( const renderWorldSnapshot_t *world,
    float outProjection[4] );
qboolean RalWebGpu_ProductRenderPlan( ralWebGpuProduct_t *product,
	const ralWebGpuRuntimeReceipt_t *runtimeReceipt,
	renderSubmissionState_t *frontend,
	const renderSubmissionReceipt_t *submission,
	const ralWebGpuFrontendPlanReceipt_t *plan,
	const ralSkyEnvironmentPlanReceipt_t *skyEnvironment,
	uintptr_t targetIdentity,
	uint64_t frameGeneration, ralWebGpuProductFrameReceipt_t *outReceipt );
qboolean RalWebGpu_ProductRender( ralWebGpuProduct_t *product,
	const ralWebGpuRuntimeReceipt_t *runtimeReceipt,
	renderSubmissionState_t *frontend,
	const renderSubmissionReceipt_t *submission, uintptr_t targetIdentity,
	uint64_t frameGeneration, ralWebGpuProductFrameReceipt_t *outReceipt );
ralWebGpuAsyncStatus_t RalWebGpu_ProductPoll( ralWebGpuProduct_t *product,
	const ralWebGpuProductFrameReceipt_t *receipt );
qboolean RalWebGpu_ProductFrameReceiptExact(
	const ralWebGpuProductFrameReceipt_t *a,
	const ralWebGpuProductFrameReceipt_t *b );
uint32_t RalWebGpu_ProductLastFailureStage( void );

#ifdef __cplusplus
}
#endif

#endif
