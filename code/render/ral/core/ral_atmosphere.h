// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#ifndef WIRED_RAL_ATMOSPHERE_H
#define WIRED_RAL_ATMOSPHERE_H

#include "ral_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RAL_ATMOSPHERE_PLAN_SCHEMA_VERSION	  1u
#define RAL_ATMOSPHERE_RECEIPT_SCHEMA_VERSION 1u
#define RAL_ATMOSPHERE_RUNTIME_SCHEMA_VERSION 1u
#define RAL_ATMOSPHERE_WEATHER_SCHEMA_VERSION 2u
#define RAL_ATMOSPHERE_MAX_VOLUMES			  64u
#define RAL_ATMOSPHERE_MAX_LIGHTS			  256u
#define RAL_ATMOSPHERE_MAX_SHADOWED_LIGHTS	  16u
#define RAL_ATMOSPHERE_MAX_SURFACE_TILES	  256u
#define RAL_ATMOSPHERE_MAX_EFFECT_WORKLOADS	  8u
#define RAL_ATMOSPHERE_SURFACE_TILE_SIZE	  128.0f
#define RAL_ATMOSPHERE_FROXEL_TILE_SIZE		  16u
#define RAL_ATMOSPHERE_MIN_SLICES			  16u
#define RAL_ATMOSPHERE_MAX_SLICES			  64u

#define RAL_ATMOSPHERE_WEATHER_RAIN		( 1u << 0 )
#define RAL_ATMOSPHERE_WEATHER_SNOW		( 1u << 1 )
#define RAL_ATMOSPHERE_WEATHER_SLEET	( 1u << 2 )
#define RAL_ATMOSPHERE_WEATHER_HAIL		( 1u << 3 )
#define RAL_ATMOSPHERE_WEATHER_DUST_ASH ( 1u << 4 )
#define RAL_ATMOSPHERE_WEATHER_ALL		( ( 1u << 5 ) - 1u )

typedef enum {
	RAL_ATMOSPHERE_TIER_OFF = 0,
	RAL_ATMOSPHERE_TIER_ANALYTIC,
	RAL_ATMOSPHERE_TIER_WEATHER,
	RAL_ATMOSPHERE_TIER_FULL
} ralAtmosphereTier_t;

typedef enum {
	RAL_ATMOSPHERE_FALLBACK_NONE = 0,
	RAL_ATMOSPHERE_FALLBACK_BACKEND_UNAVAILABLE,
	RAL_ATMOSPHERE_FALLBACK_NO_COMPUTE,
	RAL_ATMOSPHERE_FALLBACK_NO_STORAGE_BUFFER,
	RAL_ATMOSPHERE_FALLBACK_FROXEL_BUDGET,
	RAL_ATMOSPHERE_FALLBACK_CLOUD_UNSUPPORTED
} ralAtmosphereFallbackReason_t;

#define RAL_ATMOSPHERE_PASS_ANALYTIC_COMPOSITE ( 1u << 0 )
#define RAL_ATMOSPHERE_PASS_MEDIA_INJECT	   ( 1u << 1 )
#define RAL_ATMOSPHERE_PASS_LIGHT_INJECT	   ( 1u << 2 )
#define RAL_ATMOSPHERE_PASS_SHADOW_INJECT	   ( 1u << 3 )
#define RAL_ATMOSPHERE_PASS_TEMPORAL_REPROJECT ( 1u << 4 )
#define RAL_ATMOSPHERE_PASS_FROXEL_INTEGRATE   ( 1u << 5 )
#define RAL_ATMOSPHERE_PASS_SINGLE_COMPOSITE   ( 1u << 6 )
#define RAL_ATMOSPHERE_PASS_CLOUDS			   ( 1u << 7 )

#define RAL_ATMOSPHERE_EVENT_RESIZE				  ( 1u << 0 )
#define RAL_ATMOSPHERE_EVENT_CAMERA_CUT			  ( 1u << 1 )
#define RAL_ATMOSPHERE_EVENT_MAP_TRANSITION		  ( 1u << 2 )
#define RAL_ATMOSPHERE_EVENT_PAUSE				  ( 1u << 3 )
#define RAL_ATMOSPHERE_EVENT_RESUME				  ( 1u << 4 )
#define RAL_ATMOSPHERE_EVENT_DEVICE_LOST		  ( 1u << 5 )
#define RAL_ATMOSPHERE_EVENT_DEVICE_RECREATED	  ( 1u << 6 )
#define RAL_ATMOSPHERE_EVENT_CAPABILITY_DOWNGRADE ( 1u << 7 )

#define RAL_ATMOSPHERE_INVALIDATE_RESIZE	 ( 1u << 0 )
#define RAL_ATMOSPHERE_INVALIDATE_CAMERA	 ( 1u << 1 )
#define RAL_ATMOSPHERE_INVALIDATE_MAP		 ( 1u << 2 )
#define RAL_ATMOSPHERE_INVALIDATE_DEVICE	 ( 1u << 3 )
#define RAL_ATMOSPHERE_INVALIDATE_CAPABILITY ( 1u << 4 )
#define RAL_ATMOSPHERE_INVALIDATE_TIMELINE	 ( 1u << 5 )

typedef struct {
	qboolean analyticComposite;
	qboolean compute;
	qboolean storageBuffers;
	qboolean temporalHistory;
	qboolean volumetricShadows;
	qboolean fullClouds;
} ralAtmosphereCapabilities_t;

typedef struct {
	uint32_t					schemaVersion;
	ralBackendType_t			backendType;
	uint64_t					frameGeneration;
	uint32_t					width;
	uint32_t					height;
	ralAtmosphereTier_t			requestedTier;
	uint32_t					localVolumeCount;
	uint32_t					lightCount;
	uint32_t					shadowedLightCount;
	uint32_t					maxFroxelCount;
	uint32_t					maxLocalVolumes;
	uint32_t					maxLights;
	uint32_t					maxShadowedLights;
	qboolean					mediaActive;
	qboolean					skyLightingActive;
	qboolean					cloudsRequested;
	qboolean					historyValid;
	qboolean					cameraCut;
	qboolean					deviceRecreated;
	ralAtmosphereCapabilities_t capabilities;
} ralAtmospherePlanRequest_t;

typedef struct {
	uint32_t					  schemaVersion;
	ralBackendType_t			  backendType;
	uint64_t					  frameGeneration;
	ralAtmosphereTier_t			  requestedTier;
	ralAtmosphereTier_t			  selectedTier;
	ralAtmosphereFallbackReason_t fallbackReason;
	uint32_t					  passMask;
	uint32_t					  froxelWidth;
	uint32_t					  froxelHeight;
	uint32_t					  froxelDepth;
	uint32_t					  froxelCount;
	uint32_t					  admittedVolumeCount;
	uint32_t					  droppedVolumeCount;
	uint32_t					  admittedLightCount;
	uint32_t					  droppedLightCount;
	uint32_t					  admittedShadowedLightCount;
	uint32_t					  droppedShadowedLightCount;
	uint32_t					  computeDispatchCount;
	uint32_t					  compositeCount;
	uint32_t					  temporalReuseCount;
	uint32_t					  temporalRejectCount;
	qboolean					  cloudsActive;
	qboolean					  zeroWork;
	qboolean					  ready;
} ralAtmospherePlanReceipt_t;

typedef struct {
	uint32_t		 schemaVersion;
	ralBackendType_t backendType;
	uint64_t		 frameGeneration;
	uint64_t		 deviceGeneration;
	uint32_t		 width;
	uint32_t		 height;
	float			 timelineSeconds;
	float			 timeScale;
	uint32_t		 climateSeed;
	uint32_t		 eventMask;
	uint64_t		 capabilityDigest;
} ralAtmosphereRuntimeRequest_t;

typedef struct {
	uint32_t		 schemaVersion;
	ralBackendType_t backendType;
	uint64_t		 stateGeneration;
	uint64_t		 lastFrameGeneration;
	uint64_t		 deviceGeneration;
	uint64_t		 historyGeneration;
	uint32_t		 width;
	uint32_t		 height;
	float			 timelineSeconds;
	uint32_t		 climateSeed;
	uint64_t		 capabilityDigest;
	qboolean		 deviceReady;
	qboolean		 paused;
	qboolean		 historyValid;
} ralAtmosphereRuntimeState_t;

typedef struct {
	uint32_t		 schemaVersion;
	ralBackendType_t backendType;
	uint64_t		 stateGeneration;
	uint64_t		 frameGeneration;
	uint64_t		 deviceGeneration;
	uint64_t		 historyGeneration;
	uint32_t		 invalidationMask;
	qboolean		 renderAllowed;
	qboolean		 historyReusable;
	qboolean		 zeroTimeStep;
	qboolean		 replayDeterministic;
	qboolean		 ready;
} ralAtmosphereRuntimeReceipt_t;

// Backend-neutral sparse surface-climate table. The builder publishes entries
// lexicographically by signed world-tile coordinate, so shaders use an exact
// eight-step lower_bound instead of a per-pixel linear scan or backend-specific
// hash policy. The 32-byte entry is std430/storage-buffer portable.
typedef struct {
	int32_t	 tileX;
	int32_t	 tileY;
	uint32_t eventMask;
	uint32_t reserved;
	float	 climate[4]; // wetness, frost, snow, melt
} ralAtmosphereSurfaceTile_t;

typedef struct {
	uint32_t sourceCount;
	uint32_t admittedCount;
	uint32_t comparisonLimit;
	uint32_t byteCount;
} ralAtmosphereSurfaceTableReceipt_t;

// Portable weather workload admission. Backends receive one exact particle
// ceiling plus explicit collision/intersection availability; no backend may
// silently simulate a full pool indoors or claim roof/depth masking without
// the corresponding resources.
typedef struct {
	uint32_t			schemaVersion;
	ralAtmosphereTier_t tier;
	uint32_t			maxParticles;
	uint32_t			semanticEmitterCount;
	uint32_t			semanticParticleCount;
	float				precipitation[5]; // rain, snow, sleet, hail, dust/ash
	float				indoorExposure;
	qboolean			enabled;
	qboolean			heightgridRequested;
	qboolean			heightgridAvailable;
	qboolean			depthIntersectionRequested;
	qboolean			depthIntersectionAvailable;
} ralAtmosphereWeatherRequest_t;

typedef struct {
	uint32_t schemaVersion;
	uint32_t familyMask;
	uint32_t activeParticleCount;
	uint32_t precipitationParticleCount;
	uint32_t semanticEmitterCount;
	uint32_t semanticParticleCount;
	uint32_t droppedParticleCount;
	float	 authoredIntensity;
	float	 admittedCoverage;
	qboolean heightgridActive;
	qboolean depthIntersectionActive;
	qboolean zeroWork;
	qboolean ready;
} ralAtmosphereWeatherReceipt_t;

qboolean Ral_AtmospherePlan( const ralAtmospherePlanRequest_t *request, ralAtmospherePlanReceipt_t *outReceipt );
qboolean Ral_AtmospherePlanUnavailable( ralBackendType_t backendType, uint64_t frameGeneration,
										ralAtmosphereTier_t requestedTier, ralAtmospherePlanReceipt_t *outReceipt );
qboolean Ral_AtmospherePlanReceiptExact( const ralAtmospherePlanReceipt_t *a, const ralAtmospherePlanReceipt_t *b );
qboolean Ral_AtmosphereRuntimeInit( ralBackendType_t backendType, ralAtmosphereRuntimeState_t *outState );
qboolean Ral_AtmosphereRuntimeAdvance( ralAtmosphereRuntimeState_t *state, const ralAtmosphereRuntimeRequest_t *request,
									   ralAtmosphereRuntimeReceipt_t *outReceipt );
qboolean Ral_AtmosphereRuntimeReceiptExact( const ralAtmosphereRuntimeReceipt_t *a,
											const ralAtmosphereRuntimeReceipt_t *b );
qboolean Ral_AtmosphereBuildSurfaceTable( const ralAtmosphereSurfaceTile_t *source, uint32_t sourceCount,
										  ralAtmosphereSurfaceTile_t		 *outEntries,
										  ralAtmosphereSurfaceTableReceipt_t *outReceipt );
qboolean Ral_AtmospherePlanWeather( const ralAtmosphereWeatherRequest_t *request,
									ralAtmosphereWeatherReceipt_t		*outReceipt );
qboolean Ral_AtmosphereWeatherReceiptExact( const ralAtmosphereWeatherReceipt_t *a,
											const ralAtmosphereWeatherReceipt_t *b );

#ifdef __cplusplus
}
#endif

#endif
