// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef WIRED_RAL_METAL_PRESENT_H
#define WIRED_RAL_METAL_PRESENT_H

#include "ral_metal_core.h"
#include "ral_metal_lighting.h"
#include "ral_atmosphere.h"
#include "ral_display_visibility.h"
#include "ral_swapchain.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RAL_METAL_PRESENT_SCHEMA_VERSION 17u

typedef struct ralMetalPresent_s ralMetalPresent_t;
typedef struct renderSubmissionState_s renderSubmissionState_t;

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
	uint32_t texturedWorldBatchCount;
	uint32_t lightmappedWorldBatchCount;
	uint32_t legacyLightmapDrawCount;
	uint32_t directionalStaticDrawCount;
	uint64_t surfaceLightingBindingDigest;
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
	uint32_t effectEmitterDispatchCount;
	uint32_t effectParticleDrawCount;
	uint32_t effectPrimitiveDroppedCount;
	uint32_t loweredUiPrimitiveCount;
	uint32_t texturedUiPrimitiveCount;
	uint32_t msdfUiPrimitiveCount;
	uint32_t atmosphereDispatchCount;
	uint32_t atmosphereFroxelCount;
	uint32_t atmosphereCompositeCount;
	qboolean atmosphereCloudsActive;
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
qboolean RalMetal_PresentSetDirectionalLighting( ralMetalPresent_t *present,
	const ralMetalLightingReceipt_t *lightingReceipt );
qboolean RalMetal_PresentSetDisplayVisibility( ralMetalPresent_t *present,
	const ralDisplayVisibilityPlan_t *visibility );
qboolean RalMetal_PresentSetLightmapBoost( ralMetalPresent_t *present,
	float lightmapBoost );
qboolean RalMetal_PresentSetShaderTimeOverride( ralMetalPresent_t *present,
	float shaderTimeOverride );
qboolean RalMetal_PresentSetToneMap( ralMetalPresent_t *present,
	uint32_t mode, float exposure, float lottesContrast,
	float lottesShoulder, float lottesMidIn, float lottesMidOut,
	float lottesHdrMax );

qboolean RalMetal_PresentAcquire( ralMetalPresent_t *present,
	const ralMetalCoreReceipt_t *coreReceipt,
	const ralMetalPresentLayerReceipt_t *layerReceipt, uint64_t acquireGeneration,
	ralMetalDrawableReceipt_t *outReceipt );
qboolean RalMetal_PresentClearAndSubmit( ralMetalPresent_t *present,
	const ralMetalCoreReceipt_t *coreReceipt,
	const ralMetalPresentLayerReceipt_t *layerReceipt,
	const ralMetalDrawableReceipt_t *drawableReceipt,
	const renderSubmissionState_t *frontend,
	const float clearColor[4], uint64_t presentGeneration,
	ralMetalPresentReceipt_t *outReceipt );
qboolean RalMetal_PresentPlanAtmosphere( const ralMetalPresent_t *present,
	const renderSubmissionState_t *frontend, uint64_t frameGeneration,
	uint32_t width, uint32_t height, ralAtmospherePlanReceipt_t *outReceipt );
qboolean RalMetal_PresentRequestCapture( ralMetalPresent_t *present );
const byte *RalMetal_PresentCaptureRgb( const ralMetalPresent_t *present,
	uint32_t *outWidth, uint32_t *outHeight );
qboolean RalMetal_PresentRequestCapture( ralMetalPresent_t *present );
const byte *RalMetal_PresentCaptureRgb( const ralMetalPresent_t *present,
	uint32_t *outWidth, uint32_t *outHeight );

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
