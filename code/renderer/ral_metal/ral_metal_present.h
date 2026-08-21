// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef WIRED_RAL_METAL_PRESENT_H
#define WIRED_RAL_METAL_PRESENT_H

#include "ral_metal_core.h"
#include "ral_swapchain.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RAL_METAL_PRESENT_SCHEMA_VERSION 2u

typedef struct ralMetalPresent_s ralMetalPresent_t;

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
	ralCommandReceipt_t recording;
	ralCommandReceipt_t executable;
	ralSubmissionReceipt_t submission;
	uint64_t completionGeneration;
	uint64_t clearDigest;
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

qboolean RalMetal_PresentAcquire( ralMetalPresent_t *present,
	const ralMetalCoreReceipt_t *coreReceipt,
	const ralMetalPresentLayerReceipt_t *layerReceipt, uint64_t acquireGeneration,
	ralMetalDrawableReceipt_t *outReceipt );
qboolean RalMetal_PresentClearAndSubmit( ralMetalPresent_t *present,
	const ralMetalCoreReceipt_t *coreReceipt,
	const ralMetalPresentLayerReceipt_t *layerReceipt,
	const ralMetalDrawableReceipt_t *drawableReceipt,
	const float clearColor[4], uint64_t presentGeneration,
	ralMetalPresentReceipt_t *outReceipt );

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
