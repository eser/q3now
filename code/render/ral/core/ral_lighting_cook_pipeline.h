// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#ifndef WIRED_RAL_LIGHTING_COOK_PIPELINE_H
#define WIRED_RAL_LIGHTING_COOK_PIPELINE_H

#include "ral_lighting_cache.h"
#include "ral_lighting_cook.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RAL_LIGHTING_COOK_PIPELINE_SCHEMA_VERSION 1u
#define RAL_LIGHTING_COOK_PIPELINE_RECEIPT_SCHEMA_VERSION 1u
#define RAL_LIGHTING_COOK_MAX_ARTIFACTS 2u

typedef struct {
	uint32_t productMask;
	ralLightingArtifactKind_t kind;
	uint64_t artifactGeneration;
	uint64_t cacheKey;
	const void *bytes;
	uint64_t byteLength;
} ralLightingCookArtifact_t;

typedef qboolean ( *ralLightingCookCancelFn )( void *context );
typedef void ( *ralLightingCookProgressFn )(
	void *context, const ralLightingCookReceipt_t *receipt,
	uint32_t completedProducts, uint32_t artifactIndex,
	uint32_t artifactCount );

typedef struct {
	uint32_t schemaVersion;
	ralLightingCookRequest_t cook;
	const ralLightingCookArtifact_t *artifacts;
	uint32_t artifactCount;
	const ralLightingCacheOps_t *cacheOps;
	void *cacheContext;
	void *verifyScratch;
	uint64_t verifyScratchCapacity;
	ralLightingCookCancelFn shouldCancel;
	ralLightingCookProgressFn progress;
	void *callbackContext;
} ralLightingCookPipelineRequest_t;

typedef struct {
	uint32_t schemaVersion;
	ralLightingCookReceipt_t cook;
	uint32_t storedProducts;
	uint32_t verifiedProducts;
	uint32_t artifactCount;
	uint32_t progressEventCount;
	uint64_t artifactManifestHash;
	qboolean ready;
} ralLightingCookPipelineReceipt_t;

qboolean Ral_LightingCookPipelineExecute(
	const ralLightingCookPipelineRequest_t *request,
	ralLightingCookPipelineReceipt_t *outReceipt );
qboolean Ral_LightingCookPipelineReceiptValid(
	const ralLightingCookPipelineReceipt_t *receipt );

#ifdef __cplusplus
}
#endif

#endif
