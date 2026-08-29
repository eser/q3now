// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#ifndef WIRED_RAL_LIGHTING_RUNTIME_H
#define WIRED_RAL_LIGHTING_RUNTIME_H

#include "ral_lighting_artifact.h"
#include "ral_resource.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RAL_LIGHTING_RUNTIME_PLAN_SCHEMA_VERSION 1u
#define RAL_LIGHTING_RUNTIME_MAX_PLANES 3u

typedef struct {
	ralLightingPayloadRole_t role;
	ralTextureCreateInfo_t texture;
	uint64_t artifactOffset;
	uint64_t byteLength;
	uint64_t payloadHash;
	uint32_t tightBytesPerRow;
	uint32_t rowsPerImage;
} ralLightingRuntimePlanePlan_t;

typedef struct {
	uint32_t schemaVersion;
	ralBackendType_t backendType;
	uint64_t frameGeneration;
	uint64_t artifactGeneration;
	uint64_t artifactHash;
	uint64_t manifestHash;
	ralStaticLightingEncoding_t encoding;
	uint32_t planeCount;
	ralLightingRuntimePlanePlan_t planes[RAL_LIGHTING_RUNTIME_MAX_PLANES];
	qboolean staticDiffuseIndirectOnly;
	qboolean ready;
} ralLightingRuntimePlan_t;

qboolean Ral_LightingRuntimePlanBuild( ralBackendType_t backendType,
	uint64_t frameGeneration, const void *artifactBytes, uint64_t artifactByteLength,
	const ralLightingArtifactReceipt_t *expectedArtifact,
	const ralStaticLightingCapabilities_t *capabilities,
	ralLightingRuntimePlan_t *outPlan );
qboolean Ral_LightingRuntimePlanValid( const ralLightingRuntimePlan_t *plan );
qboolean Ral_LightingRuntimePlanExact( const ralLightingRuntimePlan_t *a,
	const ralLightingRuntimePlan_t *b );

#ifdef __cplusplus
}
#endif
#endif
