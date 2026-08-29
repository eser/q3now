// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#ifndef WIRED_RAL_LIGHTING_COMPOSITION_H
#define WIRED_RAL_LIGHTING_COMPOSITION_H

#include "ral_lighting.h"
#include "ral_lighting_runtime.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RAL_LIGHTING_COMPOSITION_SCHEMA_VERSION 1u
#define RAL_LIGHTING_COMPOSITION_RECEIPT_SCHEMA_VERSION 1u
#define RAL_LIGHTING_COMPOSITION_TERM_COUNT 4u
#define RAL_LIGHTING_SURFACE_BINDING_SCHEMA_VERSION 1u
#define RAL_LIGHTING_SURFACE_BINDING_RECEIPT_SCHEMA_VERSION 1u
#define RAL_LIGHTING_SURFACE_BINDING_NO_PLANE UINT32_MAX

typedef enum {
	RAL_LIGHTING_DIFFUSE_AUTHORITY_LEGACY_LIGHTMAP = 1,
	RAL_LIGHTING_DIFFUSE_AUTHORITY_DIRECTIONAL_STATIC,
	RAL_LIGHTING_DIFFUSE_AUTHORITY_LOCAL_SH
} ralLightingDiffuseAuthority_t;

typedef enum {
	RAL_LIGHTING_TERM_DYNAMIC_DIRECT = 1u << 0,
	RAL_LIGHTING_TERM_STATIC_INDIRECT = 1u << 1,
	RAL_LIGHTING_TERM_LOCAL_SH = 1u << 2,
	RAL_LIGHTING_TERM_SPECULAR_IBL = 1u << 3,
	RAL_LIGHTING_TERM_ALL = (1u << RAL_LIGHTING_COMPOSITION_TERM_COUNT) - 1u
} ralLightingCompositionTerm_t;

typedef enum {
	RAL_LIGHTING_DEBUG_FINAL = 0,
	RAL_LIGHTING_DEBUG_DYNAMIC_DIRECT,
	RAL_LIGHTING_DEBUG_STATIC_INDIRECT,
	RAL_LIGHTING_DEBUG_LOCAL_SH,
	RAL_LIGHTING_DEBUG_SPECULAR_IBL
} ralLightingDebugView_t;

typedef struct {
	uint32_t schemaVersion;
	uint64_t frameGeneration;
	uint64_t surfaceId;
	ralLightingDiffuseAuthority_t diffuseAuthority;
	ralLightingDebugView_t debugView;
	uint32_t availableTermMask;
	qboolean legacyLightmapBound;
	qboolean directionalStaticBound;
	qboolean localShBound;
	qboolean dynamicDirectBound;
	qboolean specularIblBound;
	qboolean staticProductDiffuseIndirectOnly;
} ralLightingCompositionRequest_t;

typedef struct {
	uint32_t schemaVersion;
	uint64_t frameGeneration;
	uint64_t surfaceId;
	ralLightingDiffuseAuthority_t diffuseAuthority;
	ralLightingDebugView_t debugView;
	uint32_t activeTermMask;
	uint32_t visibleTermMask;
	uint8_t compositionWriteCount[RAL_LIGHTING_COMPOSITION_TERM_COUNT];
	qboolean ready;
} ralLightingCompositionReceipt_t;

typedef struct {
	uint32_t schemaVersion;
	uint64_t frameGeneration;
	uint64_t surfaceId;
	int32_t lightmapIndex;
	qboolean legacyLightmapAvailable;
} ralLightingSurfaceBindingRequest_t;

typedef struct {
	uint32_t schemaVersion;
	uint64_t frameGeneration;
	uint64_t surfaceId;
	ralLightingDiffuseAuthority_t diffuseAuthority;
	uint32_t arrayLayer;
	uint32_t radiancePlane;
	uint32_t directionPlane;
	uint32_t visibilityPlane;
	qboolean legacyLightmapBound;
	qboolean directionalStaticBound;
	qboolean ready;
} ralLightingSurfaceBindingReceipt_t;

qboolean Ral_LightingCompositionBuild(
	const ralLightingCompositionRequest_t *request,
	ralLightingCompositionReceipt_t *outReceipt );
qboolean Ral_LightingCompositionReceiptValid(
	const ralLightingCompositionReceipt_t *receipt );
qboolean Ral_LightingCompositionEvaluateQ16(
	const ralLightingCompositionReceipt_t *receipt,
	const int32_t termRgbQ16[RAL_LIGHTING_COMPOSITION_TERM_COUNT][3],
	int32_t outRgbQ16[3] );
qboolean Ral_LightingSurfaceBindingBuild(
	const ralLightingSurfaceBindingRequest_t *request,
	const ralLightingRuntimePlan_t *modernPlan,
	ralLightingSurfaceBindingReceipt_t *outReceipt );
qboolean Ral_LightingSurfaceBindingReceiptValid(
	const ralLightingSurfaceBindingReceipt_t *receipt );
qboolean Ral_LightingSurfaceBindingReceiptExact(
	const ralLightingSurfaceBindingReceipt_t *a,
	const ralLightingSurfaceBindingReceipt_t *b );

#ifdef __cplusplus
}
#endif
#endif
