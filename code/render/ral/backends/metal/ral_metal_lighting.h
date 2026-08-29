// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#ifndef WIRED_RAL_METAL_LIGHTING_H
#define WIRED_RAL_METAL_LIGHTING_H

#include "ral_metal_core.h"
#include "ral_lighting_runtime.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RAL_METAL_LIGHTING_SCHEMA_VERSION 1u

typedef struct ralMetalLighting_s ralMetalLighting_t;

typedef struct {
	uint32_t schemaVersion;
	uint64_t coreGeneration;
	ralLightingRuntimePlan_t plan;
	uintptr_t textureIdentities[RAL_LIGHTING_RUNTIME_MAX_PLANES];
	uint64_t uploadHash;
	qboolean ready;
} ralMetalLightingReceipt_t;

qboolean RalMetal_LightingUpload( ralMetalCore_t *core,
	const ralMetalCoreReceipt_t *coreReceipt, const void *artifactBytes,
	uint64_t artifactByteLength, const ralLightingRuntimePlan_t *plan,
	ralMetalLighting_t **outLighting, ralMetalLightingReceipt_t *outReceipt );
qboolean RalMetal_LightingReceiptExact( const ralMetalLightingReceipt_t *a,
	const ralMetalLightingReceipt_t *b );
qboolean RalMetal_LightingDestroy( ralMetalCore_t *core,
	const ralMetalCoreReceipt_t *coreReceipt, ralMetalLighting_t *lighting,
	const ralMetalLightingReceipt_t *authority );

#ifdef __cplusplus
}
#endif

#endif
