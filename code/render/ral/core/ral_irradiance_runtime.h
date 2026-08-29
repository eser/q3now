// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#ifndef WIRED_RAL_IRRADIANCE_RUNTIME_H
#define WIRED_RAL_IRRADIANCE_RUNTIME_H

#include "ral_lighting_artifact.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RAL_IRRADIANCE_PLACEMENT_SCHEMA_VERSION 1u
#define RAL_IRRADIANCE_ENTITY_SAMPLE_SCHEMA_VERSION 2u
#define RAL_IRRADIANCE_ENTITY_RECEIPT_SCHEMA_VERSION 2u
#define RAL_IRRADIANCE_SELECTION_RECEIPT_SCHEMA_VERSION 1u
#define RAL_IRRADIANCE_DEBUG_RECEIPT_SCHEMA_VERSION 1u
#define RAL_IRRADIANCE_MAX_VOLUMES 64u

typedef struct {
	uint32_t schemaVersion;
	uint64_t volumeId;
	uint64_t sourceGeneration;
	uint64_t provenanceHash;
	ralLightVec3Q16_t origin;
	ralLightVec3Q16_t spacing;
	ralLightVec3Q16_t boundsMin;
	ralLightVec3Q16_t boundsMax;
	uint32_t dimensions[3];
	uint32_t priority;
	uint32_t blendDistanceQ16;
	ralIrradianceFallback_t fallback;
	qboolean ready;
} ralIrradianceVolumePlacement_t;

typedef struct {
	uint32_t schemaVersion;
	uint64_t volumeId;
	uint64_t layoutHash;
	uint32_t volumeIndex;
	uint32_t priority;
	uint32_t blendWeightQ16;
	qboolean ready;
} ralIrradianceVolumeSelectionReceipt_t;

typedef struct {
	uint32_t schemaVersion;
	uint64_t queryGeneration;
	uint64_t entityId;
	ralLightVec3Q16_t position;
	ralLightVec3Q16_t normal;
	int32_t fallbackIrradianceQ16[3];
} ralIrradianceEntitySampleRequest_t;

typedef struct {
	uint32_t schemaVersion;
	uint64_t queryGeneration;
	uint64_t entityId;
	uint64_t volumeId;
	uint64_t layoutHash;
	ralIrradianceProbeReceipt_t probes;
	int32_t blendedCoefficientsQ16[4][3];
	int32_t diffuseIrradianceQ16[3];
	uint64_t contributorHash;
	uint64_t coefficientHash;
	qboolean usedFallback;
	qboolean ready;
} ralIrradianceEntitySampleReceipt_t;

typedef struct {
	uint32_t probeIndex;
	ralLightVec3Q16_t position;
	uint32_t validity;
	uint32_t weightQ16;
	int32_t coefficientsQ16[4][3];
} ralIrradianceDebugProbe_t;

typedef struct {
	uint32_t schemaVersion;
	uint64_t queryGeneration;
	uint64_t entityId;
	uint64_t volumeId;
	uint64_t layoutHash;
	ralLightVec3Q16_t origin;
	ralLightVec3Q16_t spacing;
	uint32_t dimensions[3];
	uint32_t volumeProbeCount;
	uint32_t validProbeCount;
	uint32_t invalidProbeCount;
	uint32_t selectedProbeCount;
	ralIrradianceDebugProbe_t selected[RAL_IRRADIANCE_MAX_CORNERS];
	uint64_t contributorHash;
	uint64_t coefficientHash;
	ralIrradianceFallback_t fallback;
	qboolean usedFallback;
	qboolean ready;
} ralIrradianceDebugReceipt_t;

qboolean Ral_IrradianceVolumePlacementValid(
	const ralIrradianceVolumePlacement_t *placement );
uint64_t Ral_IrradianceVolumeLayoutHash(
	const ralIrradianceVolumePlacement_t *placement );
qboolean Ral_IrradianceVolumeSelect(
	const ralIrradianceVolumePlacement_t *placements, uint32_t placementCount,
	const ralLightVec3Q16_t *position,
	ralIrradianceVolumeSelectionReceipt_t *outReceipt );
qboolean Ral_IrradianceVolumeSelectionReceiptValid(
	const ralIrradianceVolumeSelectionReceipt_t *receipt );
qboolean Ral_IrradianceRuntimeVolumeBuild(
	const ralIrradianceVolumePlacement_t *placement,
	const ralLightingArtifactReceipt_t *artifact,
	ralIrradianceProbeVolume_t *outVolume );
qboolean Ral_IrradianceEntitySample(
	const ralIrradianceVolumePlacement_t *placement,
	const ralIrradianceProbeVolume_t *volume,
	const ralIrradianceEntitySampleRequest_t *request,
	const void *coefficientBytes, uint64_t coefficientByteLength,
	const uint8_t *validity, uint32_t validityCount,
	ralIrradianceEntitySampleReceipt_t *outReceipt );
uint64_t Ral_IrradianceCoefficientHash(
	const int32_t coefficientsQ16[4][3] );
qboolean Ral_IrradianceEntitySampleReceiptValid(
	const ralIrradianceEntitySampleReceipt_t *receipt );
qboolean Ral_IrradianceDebugBuild(
	const ralIrradianceVolumePlacement_t *placement,
	const ralIrradianceProbeVolume_t *volume,
	const ralIrradianceEntitySampleReceipt_t *sample,
	const void *coefficientBytes, uint64_t coefficientByteLength,
	const uint8_t *validity, uint32_t validityCount,
	ralIrradianceDebugReceipt_t *outReceipt );
qboolean Ral_IrradianceDebugReceiptValid(
	const ralIrradianceDebugReceipt_t *receipt );
qboolean Ral_IrradianceDebugReceiptExact(
	const ralIrradianceDebugReceipt_t *a,
	const ralIrradianceDebugReceipt_t *b );

#ifdef __cplusplus
}
#endif
#endif
