// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#ifndef WIRED_RAL_STATIC_LIGHTING_H
#define WIRED_RAL_STATIC_LIGHTING_H

#include "ral_lighting.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RAL_STATIC_LIGHTING_SCHEMA_VERSION 1u
#define RAL_STATIC_LIGHTING_RECEIPT_SCHEMA_VERSION 1u
#define RAL_STATIC_LIGHTING_PLAN_SCHEMA_VERSION 1u
#define RAL_STATIC_LIGHTING_PLAN_RECEIPT_SCHEMA_VERSION 1u
#define RAL_IRRADIANCE_VOLUME_SCHEMA_VERSION 1u
#define RAL_IRRADIANCE_QUERY_SCHEMA_VERSION 1u
#define RAL_IRRADIANCE_RECEIPT_SCHEMA_VERSION 1u
#define RAL_IRRADIANCE_MAX_CORNERS 8u
#define RAL_IRRADIANCE_SH_L1_COEFFICIENTS 4u

typedef enum {
	RAL_STATIC_LIGHTING_ENCODING_RGB9E5_OCT8 = 1,
	RAL_STATIC_LIGHTING_ENCODING_RGBA16F_OCT16,
	RAL_STATIC_LIGHTING_ENCODING_LEGACY_SRGB8
} ralStaticLightingEncoding_t;

typedef struct {
	uint32_t schemaVersion;
	uint64_t productGeneration;
	uint64_t cacheKey;
	uint64_t producerVersion;
	uint64_t geometryHash;
	uint64_t materialHash;
	uint64_t staticBakeHash;
	uint64_t radiancePayloadHash;
	uint64_t directionPayloadHash;
	uint64_t stationaryVisibilityPayloadHash;
	uint32_t pageWidth;
	uint32_t pageHeight;
	uint32_t pageCount;
	uint32_t bounceCount;
	uint32_t energyClampQ16;
	ralStaticLightingEncoding_t primaryEncoding;
	ralStaticLightingEncoding_t fallbackEncoding;
	qboolean hasStationaryVisibility;
} ralStaticLightingDescription_t;

typedef struct {
	uint32_t schemaVersion;
	ralStaticLightingDescription_t description;
	uint64_t artifactHash;
	qboolean ready;
} ralStaticLightingReceipt_t;

typedef struct {
	qboolean sampledRgb9e5;
	qboolean sampledRgba16Float;
	qboolean sampledRg16Snorm;
	qboolean textureArrays;
} ralStaticLightingCapabilities_t;

typedef struct {
	uint32_t schemaVersion;
	uint64_t frameGeneration;
	ralStaticLightingReceipt_t product;
	ralStaticLightingCapabilities_t capabilities;
	qboolean requireStationaryVisibility;
} ralStaticLightingPlanRequest_t;

typedef enum {
	RAL_STATIC_LIGHTING_PLAN_PRIMARY = 1,
	RAL_STATIC_LIGHTING_PLAN_FALLBACK,
	RAL_STATIC_LIGHTING_PLAN_LEGACY_COMPATIBILITY
} ralStaticLightingPlanMode_t;

typedef struct {
	uint32_t schemaVersion;
	uint64_t frameGeneration;
	uint64_t artifactHash;
	ralStaticLightingPlanMode_t mode;
	ralStaticLightingEncoding_t selectedEncoding;
	uint32_t textureSampleCount;
	qboolean staticDiffuseIndirectOnly;
	qboolean stationaryVisibilityActive;
	qboolean ready;
} ralStaticLightingPlanReceipt_t;

typedef enum {
	RAL_IRRADIANCE_SH_L1_RGB16F = 1,
	RAL_IRRADIANCE_SH_L1_RGB9E5
} ralIrradianceEncoding_t;

typedef enum {
	RAL_IRRADIANCE_FALLBACK_LIGHTGRID = 1,
	RAL_IRRADIANCE_FALLBACK_GLOBAL_AMBIENT
} ralIrradianceFallback_t;

typedef struct {
	uint32_t schemaVersion;
	uint64_t productGeneration;
	uint64_t cacheKey;
	uint64_t coefficientPayloadHash;
	uint64_t validityPayloadHash;
	ralLightVec3Q16_t origin;
	ralLightVec3Q16_t spacing;
	uint32_t dimensions[3];
	ralIrradianceEncoding_t encoding;
	ralIrradianceFallback_t fallback;
	qboolean ready;
} ralIrradianceProbeVolume_t;

typedef struct {
	uint32_t schemaVersion;
	uint64_t queryGeneration;
	uint64_t productGeneration;
	ralLightVec3Q16_t position;
} ralIrradianceProbeQuery_t;

typedef struct {
	uint32_t schemaVersion;
	uint64_t queryGeneration;
	uint64_t productGeneration;
	uint32_t sampleCount;
	uint32_t probeIndices[RAL_IRRADIANCE_MAX_CORNERS];
	uint32_t weightsQ16[RAL_IRRADIANCE_MAX_CORNERS];
	ralIrradianceFallback_t fallback;
	qboolean usedFallback;
	qboolean ready;
} ralIrradianceProbeReceipt_t;

qboolean Ral_StaticLightingBuild( const ralStaticLightingDescription_t *description,
	ralStaticLightingReceipt_t *outReceipt );
qboolean Ral_StaticLightingReceiptValid( const ralStaticLightingReceipt_t *receipt );
qboolean Ral_StaticLightingPlanBuild( const ralStaticLightingPlanRequest_t *request,
	ralStaticLightingPlanReceipt_t *outReceipt );
qboolean Ral_StaticLightingPlanReceiptValid( const ralStaticLightingPlanReceipt_t *receipt );
qboolean Ral_StaticLightingEncodeRgb9e5( const int32_t radianceQ16[3],
	uint32_t *outPacked );
qboolean Ral_StaticLightingEncodeOct8( ralLightVec3Q16_t direction,
	uint16_t *outPacked );
qboolean Ral_StaticLightingEncodeRgba16f( const int32_t radianceQ16[3],
	uint64_t *outPacked );
qboolean Ral_StaticLightingEncodeOct16( ralLightVec3Q16_t direction,
	uint32_t *outPacked );
qboolean Ral_IrradianceProbeVolumeValid( const ralIrradianceProbeVolume_t *volume );
qboolean Ral_IrradianceProbeResolve( const ralIrradianceProbeVolume_t *volume,
	const ralIrradianceProbeQuery_t *query, const uint8_t *validity,
	uint32_t validityCount, ralIrradianceProbeReceipt_t *outReceipt );
qboolean Ral_IrradianceProbeReceiptValid( const ralIrradianceProbeReceipt_t *receipt );

#ifdef __cplusplus
}
#endif
#endif
