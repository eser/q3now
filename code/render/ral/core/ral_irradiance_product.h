// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#ifndef WIRED_RAL_IRRADIANCE_PRODUCT_H
#define WIRED_RAL_IRRADIANCE_PRODUCT_H

#include "ral_lighting_artifact.h"
#include "ral_lighting_bake.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RAL_IRRADIANCE_PRODUCT_SCHEMA_VERSION 1u
#define RAL_IRRADIANCE_PRODUCT_MAX_SAMPLES 65536u

typedef struct {
	uint32_t probeIndex;
	uint32_t patchIndex;
	ralLightVec3Q16_t directionToPatch;
	uint32_t weightQ16;
	qboolean occluded;
} ralIrradianceProductSample_t;

typedef struct {
	uint32_t schemaVersion;
	uint64_t artifactGeneration;
	uint64_t producerVersion;
	uint64_t geometryHash;
	uint64_t materialHash;
	uint64_t layoutHash;
	uint64_t settingsHash;
	ralLightingBakeReceipt_t bake;
	uint32_t dimensions[3];
	ralIrradianceEncoding_t encoding;
	const ralLightVec3Q16_t *patchRadiance;
	uint32_t patchCount;
	const ralIrradianceProductSample_t *samples;
	uint32_t sampleCount;
} ralIrradianceProductRequest_t;

qboolean Ral_IrradianceProductWrite( const ralIrradianceProductRequest_t *request,
	void *coefficientScratch, uint64_t coefficientCapacity,
	uint8_t *validityScratch, uint32_t validityCapacity,
	void *artifactBytes, uint64_t artifactCapacity,
	ralLightingArtifactReceipt_t *outReceipt );

#ifdef __cplusplus
}
#endif
#endif
