// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#ifndef WIRED_RAL_LIGHTING_H
#define WIRED_RAL_LIGHTING_H

#include "ral_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RAL_LIGHT_SCHEMA_VERSION 1u
#define RAL_LIGHT_CATALOG_RECEIPT_SCHEMA_VERSION 1u
#define RAL_EMISSIVE_PROXY_SCHEMA_VERSION 1u
#define RAL_EMISSIVE_PROXY_RECEIPT_SCHEMA_VERSION 1u
#define RAL_EMISSIVE_ROUTE_RECEIPT_SCHEMA_VERSION 1u
#define RAL_LIGHTING_CACHE_SCHEMA_VERSION 1u
#define RAL_LIGHTING_CACHE_KEY_SCHEMA_VERSION 1u
#define RAL_LIGHT_UPDATE_PLAN_SCHEMA_VERSION 1u
#define RAL_CASTER_UPDATE_PLAN_SCHEMA_VERSION 1u
#define RAL_LIGHT_Q16_ONE 65536
#define RAL_LIGHT_MAX_CATALOG 1024u
#define RAL_EMISSIVE_PROXY_MAX_PER_SURFACE 4u
#define RAL_CASTER_MAX_AFFECTED_LOCAL_LIGHTS 32u

typedef struct { int32_t x, y, z; } ralLightVec3Q16_t;

typedef enum {
	RAL_LIGHT_KIND_DIRECTIONAL = 1,
	RAL_LIGHT_KIND_POINT,
	RAL_LIGHT_KIND_SPOT,
	RAL_LIGHT_KIND_AREA_PROXY
} ralLightKind_t;

typedef enum {
	RAL_LIGHT_MOBILITY_STATIC = 1,
	RAL_LIGHT_MOBILITY_STATIONARY,
	RAL_LIGHT_MOBILITY_DYNAMIC
} ralLightMobility_t;

typedef enum {
	RAL_LIGHT_CONTRIBUTE_DIRECT = 1u << 0,
	RAL_LIGHT_CONTRIBUTE_BAKE = 1u << 1,
	RAL_LIGHT_INJECT_ATMOSPHERE = 1u << 2,
	RAL_LIGHT_CAST_SHADOW = 1u << 3,
	RAL_LIGHT_EMISSIVE_PROXY = 1u << 4
} ralLightFlags_t;

typedef struct {
	uint32_t schemaVersion;
	uint64_t lightId;
	uint64_t sourceGeneration;
	uint64_t provenanceHash;
	ralLightKind_t kind;
	ralLightMobility_t mobility;
	ralLightVec3Q16_t position;
	ralLightVec3Q16_t direction;
	ralLightVec3Q16_t boundsMin;
	ralLightVec3Q16_t boundsMax;
	int32_t radianceQ16[3];
	int32_t rangeQ16;
	int32_t innerConeCosQ16;
	int32_t outerConeCosQ16;
	uint32_t shadowPriority;
	uint32_t flags;
} ralLightDescription_t;

typedef struct {
	uint32_t schemaVersion;
	uint64_t catalogGeneration;
	uint64_t catalogHash;
	uint64_t staticBakeHash;
	uint64_t stationaryDirectHash;
	uint64_t dynamicRuntimeHash;
	uint32_t lightCount;
	uint32_t staticCount;
	uint32_t stationaryCount;
	uint32_t dynamicCount;
	uint32_t shadowCandidateCount;
	uint32_t atmosphereLightCount;
	qboolean ready;
} ralLightCatalogReceipt_t;

typedef enum {
	RAL_EMISSIVE_PROXY_ACCEPTED = 0,
	RAL_EMISSIVE_PROXY_REJECT_EXPLICIT_AUTHORITY,
	RAL_EMISSIVE_PROXY_REJECT_LOW_AREA,
	RAL_EMISSIVE_PROXY_REJECT_LOW_RADIANCE,
	RAL_EMISSIVE_PROXY_REJECT_GLOBAL_BUDGET
} ralEmissiveProxyReason_t;

typedef struct {
	uint32_t schemaVersion;
	uint64_t surfaceId;
	uint64_t sourceGeneration;
	uint64_t provenanceHash;
	uint64_t materialArtifactHash;
	ralLightMobility_t mobility;
	ralLightVec3Q16_t boundsMin;
	ralLightVec3Q16_t boundsMax;
	ralLightVec3Q16_t normal;
	int32_t radianceQ16[3];
	int32_t areaQ16;
	int32_t influenceRangeQ16;
	uint32_t shadowPriority;
	uint32_t requestedProxyCount;
	qboolean explicitProxyAuthority;
	qboolean contributesToBake;
	qboolean injectsAtmosphere;
} ralEmissiveSurface_t;

typedef struct {
	uint32_t maxPerSurface;
	uint32_t remainingGlobalBudget;
	int32_t minimumAreaQ16;
	int32_t minimumPeakRadianceQ16;
} ralEmissiveProxyPolicy_t;

typedef struct {
	uint32_t schemaVersion;
	uint64_t surfaceId;
	uint64_t sourceGeneration;
	uint64_t proxySetHash;
	ralEmissiveProxyReason_t reason;
	uint32_t proxyCount;
	uint32_t consumedGlobalBudget;
	qboolean ready;
} ralEmissiveProxyReceipt_t;

typedef enum {
	RAL_EMISSIVE_CONSUMER_DIRECT_PROXY = 1u << 0,
	RAL_EMISSIVE_CONSUMER_STATIC_BAKE = 1u << 1,
	RAL_EMISSIVE_CONSUMER_BLOOM_SOURCE = 1u << 2,
	RAL_EMISSIVE_CONSUMER_ATMOSPHERE = 1u << 3
} ralEmissiveConsumer_t;

typedef struct {
	uint32_t schemaVersion;
	uint64_t surfaceId;
	uint64_t sourceGeneration;
	uint64_t provenanceHash;
	uint64_t materialArtifactHash;
	uint64_t radianceAuthorityHash;
	uint64_t proxySetHash;
	ralLightMobility_t mobility;
	ralLightVec3Q16_t boundsMin;
	ralLightVec3Q16_t boundsMax;
	int32_t radianceQ16[3];
	uint32_t consumerMask;
	uint32_t shadowPriority;
	uint32_t proxyCount;
	ralEmissiveProxyReason_t proxyReason;
	qboolean ready;
} ralEmissiveRouteReceipt_t;

typedef struct {
	uint32_t schemaVersion;
	uint64_t geometryHash;
	uint64_t materialHash;
	uint64_t probeLayoutHash;
	uint64_t producerVersion;
	uint64_t settingsHash;
	ralLightCatalogReceipt_t lightCatalog;
} ralLightingCacheInputs_t;

typedef struct {
	uint32_t schemaVersion;
	uint64_t staticIndirectKey;
	uint64_t stationaryShadowKey;
	uint64_t probeVolumeKey;
	qboolean ready;
} ralLightingCacheKey_t;

enum {
	RAL_LIGHTING_DIRTY_STATIC_INDIRECT = 1u << 0,
	RAL_LIGHTING_DIRTY_STATIONARY_SHADOW = 1u << 1,
	RAL_LIGHTING_DIRTY_PROBE_VOLUME = 1u << 2,
	RAL_LIGHTING_DIRTY_ALL = (1u << 3) - 1u
};

enum {
	RAL_LIGHT_UPDATE_RUNTIME_DIRECT = 1u << 0,
	RAL_LIGHT_UPDATE_RUNTIME_ATMOSPHERE = 1u << 1
};

typedef struct {
	uint32_t schemaVersion;
	uint64_t lightId;
	uint64_t beforeProvenanceHash;
	uint64_t afterProvenanceHash;
	ralLightVec3Q16_t affectedBoundsMin;
	ralLightVec3Q16_t affectedBoundsMax;
	uint32_t derivedDirtyMask;
	uint32_t runtimeUpdateMask;
	qboolean bounded;
	qboolean ready;
} ralLightUpdatePlan_t;

typedef struct {
	uint64_t lightId;
	uint64_t provenanceHash;
	uint32_t shadowPriority;
	ralLightMobility_t mobility;
} ralCasterAffectedLight_t;

typedef struct {
	uint32_t schemaVersion;
	uint64_t planGeneration;
	uint64_t casterId;
	uint64_t planHash;
	ralLightVec3Q16_t affectedBoundsMin;
	ralLightVec3Q16_t affectedBoundsMax;
	uint32_t candidateCount;
	uint32_t affectedLightCount;
	uint32_t droppedLightCount;
	uint32_t directionalShadowCount;
	ralCasterAffectedLight_t affectedLights[RAL_CASTER_MAX_AFFECTED_LOCAL_LIGHTS];
	qboolean bounded;
	qboolean ready;
} ralCasterUpdatePlan_t;

qboolean Ral_LightDescriptionValid( const ralLightDescription_t *light );
qboolean Ral_LightCatalogBuild( const ralLightDescription_t *lights,
	uint32_t lightCount, uint64_t catalogGeneration,
	ralLightCatalogReceipt_t *outReceipt );
qboolean Ral_LightCatalogReceiptValid( const ralLightCatalogReceipt_t *receipt );
qboolean Ral_EmissiveProxyBuild( const ralEmissiveSurface_t *surface,
	const ralEmissiveProxyPolicy_t *policy, uint32_t outputCapacity,
	ralLightDescription_t *outLights, ralEmissiveProxyReceipt_t *outReceipt );
qboolean Ral_EmissiveProxyReceiptValid( const ralEmissiveProxyReceipt_t *receipt );
qboolean Ral_EmissiveRouteBuild( const ralEmissiveSurface_t *surface,
	const ralEmissiveProxyReceipt_t *proxyReceipt,
	ralEmissiveRouteReceipt_t *outReceipt );
qboolean Ral_EmissiveRouteReceiptValid( const ralEmissiveRouteReceipt_t *receipt );
qboolean Ral_LightingCacheKeyBuild( const ralLightingCacheInputs_t *inputs,
	ralLightingCacheKey_t *outKey );
qboolean Ral_LightingCacheKeyValid( const ralLightingCacheKey_t *key );
uint32_t Ral_LightingCacheDirtyMask( const ralLightingCacheKey_t *current,
	const ralLightingCacheKey_t *next );
qboolean Ral_LightUpdatePlanBuild( const ralLightDescription_t *before,
	const ralLightDescription_t *after, ralLightUpdatePlan_t *outPlan );
qboolean Ral_LightUpdatePlanValid( const ralLightUpdatePlan_t *plan );
qboolean Ral_CasterUpdatePlanBuild( uint64_t planGeneration, uint64_t casterId,
	const ralLightVec3Q16_t *beforeBoundsMin, const ralLightVec3Q16_t *beforeBoundsMax,
	const ralLightVec3Q16_t *afterBoundsMin, const ralLightVec3Q16_t *afterBoundsMax,
	const ralLightDescription_t *lights, uint32_t lightCount,
	ralCasterUpdatePlan_t *outPlan );
qboolean Ral_CasterUpdatePlanValid( const ralCasterUpdatePlan_t *plan );

#ifdef __cplusplus
}
#endif
#endif
