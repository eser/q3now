// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef WIRED_RENDER_LIGHTING_PROJECT_SOURCE_H
#define WIRED_RENDER_LIGHTING_PROJECT_SOURCE_H

#include "render_lighting_metadata.h"
#include "render_submission.h"
#include "ral_irradiance_product.h"
#include "ral_lighting_cook_producer.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RENDER_LIGHTING_PROJECT_SOURCE_SCHEMA_VERSION 1u
#define RENDER_LIGHTING_PROJECT_SOURCE_RECEIPT_SCHEMA_VERSION 1u
#define RENDER_LIGHTING_PROJECT_MAX_VOLUMES RENDER_SUBMISSION_MAX_IRRADIANCE_VOLUMES

typedef struct {
	uint32_t schemaVersion;
	uint64_t sourceGeneration;
	uint64_t visibilityAuthorityHash;
	uint32_t maximumLinksPerPatch;
	uint32_t maximumSamplesPerProbe;
} renderLightingProjectSourceRequest_t;

typedef struct {
	ralIrradianceVolumePlacement_t placement;
	uint64_t layoutHash;
	uint32_t firstSample;
	uint32_t sampleCount;
} renderLightingProjectVolume_t;

typedef struct {
	ralLightingPatchTriangle_t *triangles;
	uint32_t triangleCapacity;
	renderLightingVisibilityCandidate_t *candidates;
	uint32_t candidateCapacity;
	ralLightingPatchVisibility_t *visibilityScratch;
	ralLightingPatchVisibility_t *visibility;
	uint32_t visibilityCapacity;
	ralLightingBakePatch_t *patches;
	uint32_t patchCapacity;
	ralLightingBakeLink_t *links;
	uint32_t linkCapacity;
	uint8_t *dirtyPatches;
	uint32_t dirtyPatchCapacity;
	uint32_t *texelPatchIndices;
	uint32_t texelCapacity;
	renderLightingProjectVolume_t *volumes;
	uint32_t volumeCapacity;
	ralIrradianceProductSample_t *samples;
	uint32_t sampleCapacity;
} renderLightingProjectSourceWorkspace_t;

typedef struct {
	uint32_t schemaVersion;
	uint64_t sourceGeneration;
	uint64_t sourceRevision;
	uint64_t geometryHash;
	uint64_t materialHash;
	uint64_t emissiveHash;
	uint64_t visibilityHash;
	uint64_t graphHash;
	uint64_t probeLayoutHash;
	uint32_t triangleCount;
	uint32_t candidateCount;
	uint32_t visibilityCount;
	uint32_t patchCount;
	uint32_t linkCount;
	uint32_t pageWidth;
	uint32_t pageHeight;
	uint32_t pageCount;
	uint32_t texelCount;
	uint32_t mappedTexelCount;
	uint32_t volumeCount;
	uint32_t sampleCount;
	uint32_t dirtyRegionCount;
	uint64_t dirtyRegionIds[RAL_LIGHTING_PATCH_MAX_DIRTY_REGIONS];
	qboolean ready;
} renderLightingProjectSourceReceipt_t;

qboolean RenderLightingProjectSource_Build(
	const renderSubmissionState_t *submission, const mapFile_t *map,
	const refimport_t *imports, const renderLightingProjectSourceRequest_t *request,
	const renderLightingProjectSourceWorkspace_t *workspace,
	renderLightingProjectSourceReceipt_t *outReceipt );
qboolean RenderLightingProjectSource_ReceiptValid(
	const renderLightingProjectSourceReceipt_t *receipt );

#ifdef __cplusplus
}
#endif

#endif
