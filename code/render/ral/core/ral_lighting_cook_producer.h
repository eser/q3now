// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#ifndef WIRED_RAL_LIGHTING_COOK_PRODUCER_H
#define WIRED_RAL_LIGHTING_COOK_PRODUCER_H

#include "ral_irradiance_product.h"
#include "ral_lighting_cook_pipeline.h"
#include "ral_lighting_product.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RAL_LIGHTING_COOK_PRODUCER_SCHEMA_VERSION 1u
#define RAL_LIGHTING_COOK_PRODUCER_RECEIPT_SCHEMA_VERSION 1u
#define RAL_LIGHTING_COOK_UNMAPPED_TEXEL UINT32_MAX

typedef struct {
	uint32_t schemaVersion;
	ralLightingBakeRequest_t bake;
	uint64_t directionalArtifactGeneration;
	ralStaticLightingEncoding_t directionalEncoding;
	uint32_t pageWidth;
	uint32_t pageHeight;
	uint32_t pageCount;
	const uint32_t *texelPatchIndices;
	uint32_t texelCount;
	const uint8_t *stationaryVisibility;
	uint32_t stationaryVisibilityCount;
	uint64_t irradianceArtifactGeneration;
	uint64_t geometryHash;
	uint64_t materialHash;
	uint64_t layoutHash;
	uint32_t irradianceDimensions[3];
	const ralIrradianceProductSample_t *irradianceSamples;
	uint32_t irradianceSampleCount;
} ralLightingCookProducerRequest_t;

typedef struct {
	ralLightVec3Q16_t *incoming;
	ralLightVec3Q16_t *outgoing;
	ralLightingBakeDirectionAccum_t *directionAccum;
	ralLightVec3Q16_t *patchRadiance;
	ralLightVec3Q16_t *patchDirection;
	uint32_t patchCapacity;
	ralLightVec3Q16_t *texelRadiance;
	ralLightVec3Q16_t *texelDirection;
	uint32_t texelCapacity;
	void *directionalRadianceBytes;
	uint64_t directionalRadianceCapacity;
	void *directionalDirectionBytes;
	uint64_t directionalDirectionCapacity;
	void *irradianceCoefficientBytes;
	uint64_t irradianceCoefficientCapacity;
	uint8_t *irradianceValidityBytes;
	uint32_t irradianceValidityCapacity;
	void *directionalArtifactBytes;
	uint64_t directionalArtifactCapacity;
	void *irradianceArtifactBytes;
	uint64_t irradianceArtifactCapacity;
} ralLightingCookProducerWorkspace_t;

typedef struct {
	uint32_t schemaVersion;
	ralLightingBakeReceipt_t bake;
	ralLightingArtifactReceipt_t directional;
	ralLightingArtifactReceipt_t irradiance;
	ralLightingCookArtifact_t artifacts[RAL_LIGHTING_COOK_MAX_ARTIFACTS];
	uint32_t artifactCount;
	qboolean ready;
} ralLightingCookProducerReceipt_t;

qboolean Ral_LightingCookProducerExecute(
	const ralLightingCookProducerRequest_t *request,
	const ralLightingCookProducerWorkspace_t *workspace,
	ralLightingCookProducerReceipt_t *outReceipt );
qboolean Ral_LightingCookProducerReceiptValid(
	const ralLightingCookProducerReceipt_t *receipt );

#ifdef __cplusplus
}
#endif

#endif
