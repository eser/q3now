// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef WIRED_RENDER_LIGHTING_PROJECT_COOK_H
#define WIRED_RENDER_LIGHTING_PROJECT_COOK_H

#include "render_lighting_project_source.h"
#include "ral_lighting_cook_driver.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RENDER_LIGHTING_PROJECT_COOK_SCHEMA_VERSION 1u
#define RENDER_LIGHTING_PROJECT_COOK_RECEIPT_SCHEMA_VERSION 1u
#define RENDER_LIGHTING_PROJECT_WORLD_STEM_CAPACITY 256u

typedef struct {
	uint32_t schemaVersion;
	uint64_t cookGeneration;
	uint64_t producerVersion;
	uint64_t settingsHash;
	uint64_t visibilityAuthorityHash;
	uint32_t bounceCount;
	uint32_t maximumLinksPerPatch;
	uint32_t maximumSamplesPerProbe;
	uint32_t workerRegionBudget;
	int32_t energyClampQ16;
	const char *derivedRoot;
	const char *worldStem;
} renderLightingProjectCookRequest_t;

typedef struct {
	uint32_t schemaVersion;
	uint64_t cookGeneration;
	uint64_t sourceRevision;
	uint64_t directionalManifestHash;
	uint64_t irradianceManifestHash;
	uint64_t cacheManifestHash;
	uint64_t directionalByteLength;
	uint64_t irradianceByteLength;
	uint32_t patchCount;
	uint32_t linkCount;
	uint32_t mappedTexelCount;
	uint32_t volumeCount;
	uint32_t batchCount;
	uint32_t completedRegionCount;
	qboolean ready;
} renderLightingProjectCookReceipt_t;

qboolean RenderLightingProjectCook_Execute(
	const renderSubmissionState_t *submission, const mapFile_t *map,
	const refimport_t *imports, const renderLightingProjectCookRequest_t *request,
	renderLightingProjectCookReceipt_t *outReceipt );
qboolean RenderLightingProjectCook_ReceiptValid(
	const renderLightingProjectCookReceipt_t *receipt );
qboolean RenderLightingProjectCook_DefaultRequest( const mapFile_t *map,
	const char *derivedRoot, renderLightingProjectCookRequest_t *outRequest,
	char outWorldStem[RENDER_LIGHTING_PROJECT_WORLD_STEM_CAPACITY] );

#ifdef __cplusplus
}
#endif

#endif
