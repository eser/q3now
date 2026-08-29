// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors
#ifndef WIRED_RAL_LIGHTING_PRODUCT_H
#define WIRED_RAL_LIGHTING_PRODUCT_H
#include "ral_lighting_artifact.h"
#include "ral_lighting_bake.h"
#ifdef __cplusplus
extern "C" {
#endif
#define RAL_LIGHTING_PRODUCT_SCHEMA_VERSION 1u
typedef struct {
	uint32_t schemaVersion;
	uint64_t artifactGeneration;
	ralLightingBakeReceipt_t bake;
	ralStaticLightingEncoding_t encoding;
	uint32_t pageWidth;
	uint32_t pageHeight;
	uint32_t pageCount;
	const ralLightVec3Q16_t *indirectRadiance;
	const ralLightVec3Q16_t *dominantDirection;
	uint32_t texelCount;
	const uint8_t *stationaryVisibility;
	uint32_t stationaryVisibilityCount;
} ralLightingProductRequest_t;
qboolean Ral_LightingProductWrite( const ralLightingProductRequest_t *request,
	void *radianceScratch, uint64_t radianceScratchCapacity,
	void *directionScratch, uint64_t directionScratchCapacity,
	void *artifactBytes, uint64_t artifactCapacity,
	ralLightingArtifactReceipt_t *outReceipt );
#ifdef __cplusplus
}
#endif
#endif
