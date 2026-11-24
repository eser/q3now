// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef WIRED_RAL_METAL_PRESENT_H
#define WIRED_RAL_METAL_PRESENT_H

#include "ral_metal_core.h"
#include "ral_metal_lighting.h"
#include "ral_atmosphere.h"
#include "ral_display_visibility.h"
#include "ral_exposure.h"
#include "ral_material_runtime.h"
#include "render_material_package.h"
#include "render_submission_effects.h"
#include "ral_swapchain.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RAL_METAL_PRESENT_SCHEMA_VERSION 23u

typedef struct ralMetalPresent_s ralMetalPresent_t;
typedef struct renderSubmissionState_s renderSubmissionState_t;

/* Material filtering only; UI/no-mip resources retain their linear sampler.
 * Changes are atomic, cached and accepted only outside a drawable lease. */
qboolean wired_metal_present_set_sampling( ralMetalPresent_t *present,
	const ralSamplerCreateInfo_t *sampling );

/* Diagnostic cumulative samples from already-completed readback commands.
 * Not a fence or a completion authority; querying never waits or reaps. */
typedef struct {
    uint64_t sampleCount;
    uint64_t durationNanoseconds;
    uint64_t unavailableCount;
    /* Paired subset of sampleCount. Commit-to-start includes driver/scheduler
     * latency, not just GPU queueing; never add it to overlapping CPU spans. */
    uint64_t timelineSampleCount;
    uint64_t commitToStartNanoseconds;
    uint64_t timelineDurationNanoseconds;
    uint64_t timelineUnavailableCount;
    /* Split-only subset of sampleCount; unsplit commands have no phase
     * attribution. Scene includes bloom/exposure, output includes tonemap/UI.
     * Overlap is subtracted from execution; gap is NOT GPU execution. */
    uint64_t splitSampleCount;
    uint64_t sceneNanoseconds;
    uint64_t outputNanoseconds;
    uint64_t overlapNanoseconds;
    uint64_t gapNanoseconds;
    /* Same sampled submission: acquire-call interval partitioned against
     * its scene GPU interval. After-scene is NOT proof that the GPU is idle. */
    uint64_t acquireSampleCount;
    uint64_t acquireUnavailableCount;
    uint64_t acquireDurationNanoseconds;
    uint64_t acquireBeforeSceneNanoseconds;
    uint64_t acquireDuringSceneNanoseconds;
    uint64_t acquireAfterSceneNanoseconds;
} ralMetalGpuTiming_t;
/* Disjoint CPU spans for one successful late-acquire submission. Nanoseconds
 * avoid rounding drift when checking the partition; NULL disables measurement.
 * GPU execution/presentation can overlap these spans and must not be added. */
typedef struct {
    uint64_t prepareNs;
    uint64_t encodeBeforeAcquireNs;
    uint64_t acquireNs;
    uint64_t encodeAfterAcquireNs;
    uint64_t submitNs;
    uint64_t retireNs;
    uint64_t totalNs;
} ralMetalSubmitTiming_t;
qboolean RalMetal_PresentGpuTiming( const ralMetalPresent_t *present,
    ralMetalGpuTiming_t *outTiming );

typedef struct {
	uint32_t schemaVersion;
	ralBackendType_t backendType;
	uint64_t coreGeneration;
	uint64_t presentationGeneration;
	uintptr_t ownerIdentity;
	uintptr_t layerIdentity;
	ralSwapchainInfo_t selected;
	qboolean displaySyncEnabled;
	qboolean extendedDynamicRange;
	qboolean framebufferOnly;
	qboolean ownsLayer;
	qboolean ready;
} ralMetalPresentLayerReceipt_t;

typedef struct {
	uint32_t schemaVersion;
	ralBackendType_t backendType;
	uint64_t coreGeneration;
	uint64_t presentationGeneration;
	uint64_t acquireGeneration;
	uintptr_t ownerIdentity;
	uintptr_t layerIdentity;
	uintptr_t drawableIdentity;
	uintptr_t textureIdentity;
	uint32_t width;
	uint32_t height;
	ralFormat_t format;
	qboolean acquired;
} ralMetalDrawableReceipt_t;

typedef struct {
	uint32_t schemaVersion;
	ralBackendType_t backendType;
	uint64_t coreGeneration;
	uint64_t presentationGeneration;
	uint64_t acquireGeneration;
	uint64_t presentGeneration;
	uint64_t frontendFrameGeneration;
	uintptr_t ownerIdentity;
	uintptr_t layerIdentity;
	uintptr_t drawableIdentity;
	uintptr_t textureIdentity;
	ralFormat_t format;
	ralColorSpace_t colorSpace;
	ralPresentMode_t presentMode;
	uint32_t width;
	uint32_t height;
	float clearColor[4];
	ralDisplayVisibilityPlan_t displayVisibility;
	ralCommandReceipt_t recording;
	ralCommandReceipt_t executable;
	ralSubmissionReceipt_t submission;
	uint64_t completionGeneration;
	uint64_t clearDigest;
	uint64_t readbackDigest;
	uint32_t loweredWorldIndexCount;
	uint32_t loweredWorldBatchCount;
	uint32_t distantWorldCandidateCount;
	uint32_t distantWorldDrawCount;
	uint32_t distantWorldIndexCount;
	qboolean distantWorldDepthReset;
	uint32_t texturedWorldBatchCount;
	uint32_t lightmappedWorldBatchCount;
	uint32_t legacyLightmapDrawCount;
	uint32_t directionalStaticDrawCount;
	uint64_t surfaceLightingBindingDigest;
	uint32_t submittedDirectLightCount;
	uint32_t directLightDrawCount;
	uint32_t directLightReferenceCount;
	uint32_t patchWorldBatchCount;
	uint32_t maskedWorldBatchCount;
	uint32_t blendedWorldBatchCount;
	uint32_t depthWriteWorldBatchCount;
	uint32_t loweredEntityIndexCount;
	uint32_t loweredEntityBatchCount;
	uint32_t modelEntityCount;
	uint32_t primitiveEntityCount;
	uint32_t temporalEntityCount;
	uint32_t localIrradianceEntityCount;
	uint32_t localIrradianceDrawCount;
	uint32_t unresolvedEntityCount;
	uint32_t loweredEffectSpriteCount;
	uint32_t loweredEffectDecalCount;
	uint32_t loweredEffectRibbonCount;
	uint32_t loweredEffectBeamCount;
	uint32_t effectEmitterDispatchCount;
	uint32_t effectParticleDrawCount;
	uint32_t effectPrimitiveDroppedCount;
	renderEffectParticlePlanReceipt_t effectParticle;
	uint32_t lensSourceCount;
	uint32_t lensVisibilityDispatchCount;
	uint32_t loweredUiPrimitiveCount;
	uint32_t texturedUiPrimitiveCount;
	uint32_t msdfUiPrimitiveCount;
	/* Native dispatches: semantic atmosphere stages plus GPU light tiling. */
	uint32_t atmosphereDispatchCount;
	uint32_t atmosphereFroxelCount;
	uint32_t atmosphereCompositeCount;
	qboolean atmosphereCloudsActive;
	ralAtmospherePlanReceipt_t atmosphere;
	ralAtmosphereWeatherReceipt_t weather;
	uint32_t weatherDispatchCount;
	uint32_t weatherDrawCount;
	uint32_t readbackX;
	uint32_t readbackY;
	uint32_t readbackByteCount;
	uint8_t readbackBytes[8];
	qboolean presented;
} ralMetalPresentReceipt_t;

qboolean RalMetal_PresentCreate( ralMetalCore_t *core,
	const ralMetalCoreReceipt_t *coreReceipt,
	const ralSwapchainCreateInfo_t *createInfo, uint64_t presentationGeneration,
	ralMetalPresent_t **outPresent, ralMetalPresentLayerReceipt_t *outReceipt );
qboolean RalMetal_PresentAdoptBorrowedLayer( ralMetalCore_t *core,
	const ralMetalCoreReceipt_t *coreReceipt,
	const ralSwapchainCreateInfo_t *createInfo, uint64_t presentationGeneration,
	void *borrowedLayer, ralMetalPresent_t **outPresent,
	ralMetalPresentLayerReceipt_t *outReceipt );
qboolean RalMetal_PresentReconfigure( ralMetalPresent_t *present,
	const ralMetalCoreReceipt_t *coreReceipt,
	const ralMetalPresentLayerReceipt_t *currentReceipt,
	const ralSwapchainCreateInfo_t *createInfo, uint64_t presentationGeneration,
	ralMetalPresentLayerReceipt_t *outReceipt );
void RalMetal_PresentDestroy( ralMetalPresent_t *present );
qboolean RalMetal_PresentUnloadWorld( ralMetalPresent_t *present,
	int worldIndex );
qboolean RalMetal_PresentUnloadSkyRadianceResources(
	ralMetalPresent_t *present );
qboolean RalMetal_PresentSetDirectionalLighting( ralMetalPresent_t *present,
	const ralMetalLightingReceipt_t *lightingReceipt );
qboolean RalMetal_PresentSetDisplayVisibility( ralMetalPresent_t *present,
	const ralDisplayVisibilityPlan_t *visibility );
qboolean RalMetal_PresentSetLightmapBoost( ralMetalPresent_t *present,
	float lightmapBoost );
qboolean RalMetal_PresentSetAerialHaze( ralMetalPresent_t *present,
	float aerialHaze );
qboolean RalMetal_PresentSetShaderTimeOverride( ralMetalPresent_t *present,
	float shaderTimeOverride );
qboolean RalMetal_PresentSetToneMap( ralMetalPresent_t *present,
	uint32_t mode, float exposure, float lottesContrast,
	float lottesShoulder, float lottesMidIn, float lottesMidOut,
	float lottesHdrMax );
#include "../../core/ral_bloom.h"
qboolean RalMetal_PresentSetBloom( ralMetalPresent_t *present,
	const ralBloomSettings_t *settings );
/* SMAA preset 1..4 applied to the offscreen scene after bloom and before exposure/tone mapping; 0 = off. */
qboolean RalMetal_PresentSetSmaaQuality( ralMetalPresent_t *present, int quality );
/* Temporal anti-aliasing (r_aa 1) on 3D scene frames: jitters the projection, rebuilds camera motion from the depth buffer and resolves
 * against a history, after the scene pass and before bloom. */
qboolean RalMetal_PresentSetTaa( ralMetalPresent_t *present, qboolean enabled );
/* Frames whose TAA pass ran the resolve (history valid) and the bootstrap copy (no history) since the presenter was created. */
void RalMetal_PresentTaaFrameCounts( const ralMetalPresent_t *present, uint32_t *resolved, uint32_t *bootstrap );
/* Entity object motion since the TAA history last restarted: rigid entity poses recorded / rejected by the pose cache, those with a
 * committed previous pose, the batches that wrote object motion and the frames that had any. */
typedef struct {
	uint32_t recorded, rejected, previousValid, motionBatches, motionFrames;
} ralMetalEntityMotionStats_t;
void RalMetal_PresentEntityMotionStats( const ralMetalPresent_t *present, ralMetalEntityMotionStats_t *outStats );
qboolean RalMetal_PresentSetExposure( ralMetalPresent_t *present,
	const ralExposureSettings_t *settings );
qboolean RalMetal_PresentPrepareMaterialResources(
		ralMetalPresent_t *present, const renderSubmissionState_t *frontend );
qboolean RalMetal_PresentResetRegistrationAssets(
		ralMetalPresent_t *present, renderSubmissionState_t *frontend );
qboolean RalMetal_PresentResolveMaterialRuntimeResource( void *present,
	uint64_t cacheKeyHash, ralMaterialRuntimeResource_t *outResource );
qboolean RalMetal_PresentSetMaterialRuntimeCatalog(
	ralMetalPresent_t *present,
	const renderMaterialRuntimeCatalog_t *catalog,
	renderSubmissionState_t *frontend );

qboolean RalMetal_PresentAcquire( ralMetalPresent_t *present,
	const ralMetalCoreReceipt_t *coreReceipt,
	const ralMetalPresentLayerReceipt_t *layerReceipt, uint64_t acquireGeneration,
	ralMetalDrawableReceipt_t *outReceipt );
/* Both submit entry points consume presentGeneration at native commit, even
 * if later readback/receipt publication fails. Pre-commit failures leave it
 * retryable; already submitted generations must never be executed again. */
qboolean RalMetal_PresentClearAndSubmit( ralMetalPresent_t *present,
	const ralMetalCoreReceipt_t *coreReceipt,
	const ralMetalPresentLayerReceipt_t *layerReceipt,
	const ralMetalDrawableReceipt_t *drawableReceipt,
	renderSubmissionState_t *frontend,
	const float clearColor[4], uint64_t presentGeneration,
	ralMetalPresentReceipt_t *outReceipt );
/* Production transaction: submit offscreen work before waiting for a drawable;
 * its same-queue output tail completes the logical submission. Outputs publish
 * only on success. A missing drawable publishes no acquired receipt. When the
 * scene was already committed, its generation/history remain consumed and the
 * work is drained before returning: only a NEW generation may then proceed.
 * UI-only/pre-commit failures may retry the same generation. */
typedef enum {
	RAL_METAL_SUBMIT_FAILED = 0,
	RAL_METAL_SUBMIT_PRESENTED,
	RAL_METAL_SUBMIT_DRAWABLE_UNAVAILABLE
} ralMetalSubmitResult_t;
ralMetalSubmitResult_t RalMetal_PresentRenderAndSubmit( ralMetalPresent_t *present,
	const ralMetalCoreReceipt_t *coreReceipt,
	const ralMetalPresentLayerReceipt_t *layerReceipt,
	renderSubmissionState_t *frontend, const float clearColor[4],
	uint64_t acquireGeneration, uint64_t presentGeneration,
	ralMetalDrawableReceipt_t *outDrawable, ralMetalPresentReceipt_t *outReceipt,
	ralMetalSubmitTiming_t *outTiming );
qboolean RalMetal_PresentPlanAtmosphere( ralMetalPresent_t *present,
	const renderSubmissionState_t *frontend, uint64_t frameGeneration,
	uint32_t width, uint32_t height, ralAtmospherePlanReceipt_t *outReceipt );
qboolean RalMetal_PresentRequestCapture( ralMetalPresent_t *present );
const byte *RalMetal_PresentCaptureRgb( const ralMetalPresent_t *present,
	uint32_t *outWidth, uint32_t *outHeight );
qboolean RalMetal_PresentLensVisibility( ralMetalPresent_t *present,
	int id, float *outVisibility );
qboolean RalMetal_PresentSkyGpuTelemetry( const ralMetalPresent_t *present,
	ralSkyGpuTelemetryReceipt_t *outReceipt );

qboolean RalMetal_PresentLayerReceiptExact( const ralMetalPresentLayerReceipt_t *a,
	const ralMetalPresentLayerReceipt_t *b );
qboolean RalMetal_DrawableReceiptExact( const ralMetalDrawableReceipt_t *a,
	const ralMetalDrawableReceipt_t *b );
qboolean RalMetal_PresentReceiptExact( const ralMetalPresentReceipt_t *a,
	const ralMetalPresentReceipt_t *b );

#ifdef __cplusplus
}
#endif

#endif
