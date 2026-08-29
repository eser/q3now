// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors
#ifndef WIRED_RAL_LIGHTING_COOK_H
#define WIRED_RAL_LIGHTING_COOK_H
#include "ral_lighting_artifact.h"
#ifdef __cplusplus
extern "C" {
#endif
#define RAL_LIGHTING_COOK_SCHEMA_VERSION 1u
#define RAL_LIGHTING_COOK_RECEIPT_SCHEMA_VERSION 1u
#define RAL_LIGHTING_COOK_MAX_DIRTY_REGIONS 256u
#define RAL_LIGHTING_CACHE_SELECTION_SCHEMA_VERSION 1u
#define RAL_LIGHTING_CACHE_SELECTION_RECEIPT_SCHEMA_VERSION 1u
enum {
	RAL_LIGHTING_COOK_DIRECTIONAL_LIGHTMAP = 1u<<0,
	RAL_LIGHTING_COOK_STATIONARY_VISIBILITY = 1u<<1,
	RAL_LIGHTING_COOK_IRRADIANCE_PROBES = 1u<<2,
	RAL_LIGHTING_COOK_ALL = (1u<<3)-1u
};
typedef enum {
	RAL_LIGHTING_COOK_PLANNED = 1,
	RAL_LIGHTING_COOK_RUNNING,
	RAL_LIGHTING_COOK_CANCELLED,
	RAL_LIGHTING_COOK_STAGED,
	RAL_LIGHTING_COOK_VERIFIED,
	RAL_LIGHTING_COOK_PUBLISHED
} ralLightingCookState_t;
typedef struct {
	uint32_t schemaVersion;
	uint64_t cookGeneration;
	uint64_t sourceRevision;
	ralLightingCacheKey_t desiredKey;
	ralLightingCacheKey_t publishedKey;
	uint32_t requestedProducts;
	uint32_t workerRegionBudget;
	uint32_t dirtyRegionCount;
	uint64_t dirtyRegionIds[RAL_LIGHTING_COOK_MAX_DIRTY_REGIONS];
} ralLightingCookRequest_t;
typedef struct {
	uint32_t schemaVersion;
	uint64_t cookGeneration;
	uint64_t sourceRevision;
	uint64_t planHash;
	uint64_t stagedManifestHash;
	uint32_t requestedProducts;
	uint32_t invalidatedProducts;
	uint32_t scheduledRegionCount;
	uint32_t deferredRegionCount;
	uint64_t scheduledRegionIds[RAL_LIGHTING_COOK_MAX_DIRTY_REGIONS];
	ralLightingCookState_t state;
	uint32_t transitionSerial;
	qboolean ready;
} ralLightingCookReceipt_t;

typedef enum {
	RAL_LIGHTING_CACHE_MODERN_FINAL = 1,
	RAL_LIGHTING_CACHE_LEGACY_COMPATIBILITY,
	RAL_LIGHTING_CACHE_PREVIEW,
	RAL_LIGHTING_CACHE_BLOCKED
} ralLightingCacheMode_t;

typedef struct {
	uint32_t schemaVersion;
	uint64_t expectedArtifactGeneration;
	uint64_t expectedCacheKey;
	ralLightingArtifactKind_t expectedKind;
	const ralLightingArtifactReceipt_t *artifact;
	qboolean legacyCompatibilityAvailable;
	qboolean requireModernProduct;
	qboolean allowPreview;
} ralLightingCacheSelectionRequest_t;

typedef struct {
	uint32_t schemaVersion;
	uint64_t expectedArtifactGeneration;
	uint64_t expectedCacheKey;
	ralLightingArtifactKind_t expectedKind;
	ralLightingCacheMode_t mode;
	uint64_t artifactManifestHash;
	qboolean requiresRegeneration;
	qboolean finalQuality;
	qboolean ready;
} ralLightingCacheSelectionReceipt_t;
qboolean Ral_LightingCookPlan(const ralLightingCookRequest_t*request,ralLightingCookReceipt_t*outReceipt);
qboolean Ral_LightingCookReceiptValid(const ralLightingCookReceipt_t*receipt);
qboolean Ral_LightingCookTransition(const ralLightingCookReceipt_t*current,
	ralLightingCookState_t next,uint32_t completedProducts,uint64_t stagedManifestHash,
	ralLightingCookReceipt_t*outReceipt);
qboolean Ral_LightingCacheSelect( const ralLightingCacheSelectionRequest_t *request,
	ralLightingCacheSelectionReceipt_t *outReceipt );
qboolean Ral_LightingCacheSelectionReceiptValid(
	const ralLightingCacheSelectionReceipt_t *receipt );
#ifdef __cplusplus
}
#endif
#endif
