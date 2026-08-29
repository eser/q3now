// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#ifndef WIRED_RENDER_FRONTEND_SUBMISSION_LIGHTING_H
#define WIRED_RENDER_FRONTEND_SUBMISSION_LIGHTING_H

#include "../ral/core/ral_lighting_bake.h"
#include "../ral/core/ral_lighting_artifact.h"
#include "../ral/core/ral_irradiance_runtime.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RENDER_LIGHTING_EXTRACTION_SCHEMA_VERSION 1u
#define RENDER_LIGHTING_VISIBILITY_SCHEMA_VERSION 1u
#define RENDER_EMISSIVE_ROUTING_SCHEMA_VERSION 1u
#define RENDER_SUBMISSION_MAX_IRRADIANCE_VOLUMES RAL_IRRADIANCE_MAX_VOLUMES
#define RENDER_SUBMISSION_MAX_EMISSIVE_ROUTES 1024u
#define RENDER_SUBMISSION_MAX_EMISSIVE_PROXY_LIGHTS 32u

typedef struct {
	uint32_t schemaVersion;
	uint64_t extractionGeneration;
	uint64_t worldDigest;
	uint64_t materialDigest;
	uint64_t geometryHash;
	uint64_t materialHash;
	uint64_t emissiveHash;
	uint32_t triangleCount;
	uint32_t skippedBatchCount;
	qboolean ready;
} renderLightingExtractionReceipt_t;

typedef struct {
	uint32_t receiverTriangle;
	uint32_t emitterTriangle;
} renderLightingVisibilityCandidate_t;

typedef qboolean ( *renderLightingVisibilityQueryFn )(
	void *userData, uint64_t receiverTriangleId, uint64_t emitterTriangleId,
	const ralLightVec3Q16_t *receiverCentroid, const ralLightVec3Q16_t *emitterCentroid,
	uint32_t *outVisibilityQ16, uint64_t *outQueryProvenance );

typedef struct {
	uint32_t schemaVersion;
	uint64_t queryGeneration;
	uint64_t visibilityAuthorityHash;
	const ralLightingPatchTriangle_t *triangles;
	uint32_t triangleCount;
	const renderLightingVisibilityCandidate_t *candidates;
	uint32_t candidateCount;
	uint32_t dirtyRegionCount;
	uint64_t dirtyRegionIds[RAL_LIGHTING_PATCH_MAX_DIRTY_REGIONS];
	renderLightingVisibilityQueryFn query;
	void *queryUserData;
} renderLightingVisibilityRequest_t;

typedef struct {
	uint32_t schemaVersion;
	uint64_t queryGeneration;
	uint64_t visibilityAuthorityHash;
	uint64_t candidateHash;
	uint64_t visibilityHash;
	uint32_t candidateCount;
	uint32_t queriedCount;
	uint32_t visibleCount;
	uint32_t occludedCount;
	uint32_t dirtyRegionCount;
	qboolean ready;
} renderLightingVisibilityReceipt_t;

typedef struct {
	uint32_t schemaVersion;
	uint64_t routingGeneration;
	uint64_t worldDigest;
	uint64_t materialDigest;
	uint64_t routeDigest;
	uint32_t routeCount;
	uint32_t proxyLightCount;
	uint32_t directRouteCount;
	uint32_t bakeRouteCount;
	uint32_t bloomRouteCount;
	uint32_t atmosphereRouteCount;
	uint32_t rejectedProxyRouteCount;
	qboolean ready;
} renderEmissiveRoutingReceipt_t;

typedef struct renderSubmissionState_s renderSubmissionState_t;

typedef struct {
	ralIrradianceVolumePlacement_t placement;
	ralLightingArtifactReceipt_t artifact;
	ralIrradianceProbeVolume_t volume;
	void *ownedArtifactBytes;
	uint64_t artifactByteLength;
	const void *coefficientBytes;
	uint64_t coefficientByteLength;
	const uint8_t *validityBytes;
	uint32_t validityCount;
} renderIrradianceVolumeRecord_t;

typedef struct {
	ralLightingArtifactReceipt_t artifact;
	void *ownedArtifactBytes;
	uint64_t artifactByteLength;
	ralLightingPayloadView_t payloads[RAL_LIGHTING_ARTIFACT_MAX_PAYLOADS];
} renderDirectionalLightingRecord_t;

typedef struct {
	ralIrradianceVolumePlacement_t placement;
	const void *artifactBytes;
	uint64_t artifactByteLength;
} renderIrradianceVolumeSource_t;

qboolean RenderSubmission_ExtractLightingTriangles(
	const renderSubmissionState_t *state, uint64_t extractionGeneration,
	ralLightingPatchTriangle_t *outTriangles, uint32_t triangleCapacity,
	renderLightingExtractionReceipt_t *outReceipt );
qboolean RenderSubmission_LightingExtractionReceiptValid(
	const renderLightingExtractionReceipt_t *receipt );
qboolean RenderSubmission_BuildLightingVisibility(
	const renderLightingVisibilityRequest_t *request,
	ralLightingPatchVisibility_t *scratchVisibility, uint32_t scratchCapacity,
	ralLightingPatchVisibility_t *outVisibility, uint32_t outputCapacity,
	renderLightingVisibilityReceipt_t *outReceipt );
qboolean RenderSubmission_LightingVisibilityReceiptValid(
	const renderLightingVisibilityReceipt_t *receipt );
qboolean RenderSubmission_BuildEmissiveRoutes(
	const renderSubmissionState_t *state, uint64_t routingGeneration,
	const ralEmissiveProxyPolicy_t *policy,
	ralEmissiveRouteReceipt_t *outRoutes, uint32_t routeCapacity,
	ralLightDescription_t *outProxyLights, uint32_t proxyLightCapacity,
	renderEmissiveRoutingReceipt_t *outReceipt );
qboolean RenderSubmission_EmissiveRoutingReceiptValid(
	const renderEmissiveRoutingReceipt_t *receipt );
qboolean RenderSubmission_RebuildEmissiveAuthority(
	renderSubmissionState_t *state, uint64_t routingGeneration );
qboolean RenderSubmission_EmissiveAuthoritySnapshot(
	const renderSubmissionState_t *state,
	const ralEmissiveRouteReceipt_t **outRoutes, uint32_t *outRouteCount,
	const ralLightDescription_t **outProxyLights, uint32_t *outProxyLightCount,
	const renderEmissiveRoutingReceipt_t **outReceipt );
qboolean RenderSubmission_RegisterIrradianceVolume(
	renderSubmissionState_t *state, const ralIrradianceVolumePlacement_t *placement,
	const void *artifactBytes, uint64_t artifactByteLength );
qboolean RenderSubmission_ReplaceIrradianceVolumes(
	renderSubmissionState_t *state, const renderIrradianceVolumeSource_t *sources,
	uint32_t sourceCount );
qboolean RenderSubmission_ClearIrradianceVolumes( renderSubmissionState_t *state );
qboolean RenderSubmission_AttachConfiguredEntityIrradiance(
	renderSubmissionState_t *state, uint32_t entityIndex,
	const ralLightVec3Q16_t *referenceNormal,
	const int32_t fallbackIrradianceQ16[3] );
qboolean RenderSubmission_AttachLegacyLightGridEntityIrradiance(
	renderSubmissionState_t *state, uint32_t entityIndex,
	float ambientScale, float directedScale );
qboolean RenderSubmission_IrradianceVolumeSnapshot(
	const renderSubmissionState_t *state,
	const renderIrradianceVolumeRecord_t **outVolumes, uint32_t *outVolumeCount,
	uint64_t *outDigest );
qboolean RenderSubmission_RegisterDirectionalLighting(
	renderSubmissionState_t *state, const void *artifactBytes,
	uint64_t artifactByteLength );
qboolean RenderSubmission_ClearDirectionalLighting( renderSubmissionState_t *state );
qboolean RenderSubmission_DirectionalLightingSnapshot(
	const renderSubmissionState_t *state,
	const renderDirectionalLightingRecord_t **outLighting, uint64_t *outDigest );

#ifdef __cplusplus
}
#endif

#endif
