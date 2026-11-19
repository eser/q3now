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
#define RAL_ATMOSPHERE_RUNTIME_SCHEMA_VERSION 2u
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

/* FULL froxel distance is a media contract, independent of scene clipping.
 * Keep producers, integration and consumers on this same logarithmic range.
 * Non-positive authored visibility means unbounded media; a positive short
 * visibility must not accidentally select the unbounded range. */
#define WIRED_ATMOSPHERE_GRID_NEAR 4.0f
#define WIRED_ATMOSPHERE_GRID_MIN_FAR 4.01f
#define WIRED_ATMOSPHERE_GRID_DEFAULT_FAR 65536.0f
static inline float wired_atmosphere_grid_far( float visibility )
{
	if ( !( visibility > 0.0f ) ) return WIRED_ATMOSPHERE_GRID_DEFAULT_FAR;
	return visibility < WIRED_ATMOSPHERE_GRID_MIN_FAR
		? WIRED_ATMOSPHERE_GRID_MIN_FAR : visibility;
}

#define RAL_SKY_ENVIRONMENT_SCHEMA_VERSION         4u
#define RAL_SKY_ENVIRONMENT_RECEIPT_SCHEMA_VERSION 4u
#define RAL_SKY_ENVIRONMENT_RUNTIME_SCHEMA_VERSION 4u
#define RAL_SKY_ENVIRONMENT_MAX_WORK_ITEMS          8u

#define RAL_SKY_DISTANT_DRAW_SCHEMA_VERSION 1u
#define RAL_SKY_DISTANT_MAX_CANDIDATES       4096u
#define RAL_SKY_DISTANT_MAX_VISIBLE_DRAWS     256u
#define RAL_SKY_DISTANT_MAX_LOD_LEVELS          8u

typedef enum {
	RAL_SKY_ENVIRONMENT_NONE = 0,
	RAL_SKY_ENVIRONMENT_AUTHORED,
	RAL_SKY_ENVIRONMENT_PHYSICAL,
	RAL_SKY_ENVIRONMENT_HYBRID
} ralSkyEnvironmentMode_t;

typedef enum {
	RAL_SKY_FALLBACK_NONE = 0,
	RAL_SKY_FALLBACK_TIER,
	RAL_SKY_FALLBACK_NO_PHYSICAL_LUT,
	RAL_SKY_FALLBACK_NO_CAPTURE,
	RAL_SKY_FALLBACK_NO_CONVOLUTION,
	RAL_SKY_FALLBACK_NO_CLOUD_LIGHT,
	RAL_SKY_FALLBACK_NO_DISTANT_SCENE,
	RAL_SKY_FALLBACK_NO_TEMPORAL_CROSSFADE
} ralSkyEnvironmentFallbackReason_t;

#define RAL_SKY_DIRTY_TRANSMITTANCE ( 1u << 0 )
#define RAL_SKY_DIRTY_MULTISCATTER  ( 1u << 1 )
#define RAL_SKY_DIRTY_SKY_VIEW      ( 1u << 2 )
#define RAL_SKY_DIRTY_RADIANCE      ( 1u << 3 )
#define RAL_SKY_DIRTY_AMBIENT_SH    ( 1u << 4 )
#define RAL_SKY_DIRTY_SPECULAR_IBL  ( 1u << 5 )
#define RAL_SKY_DIRTY_CLOUD_LIGHT   ( 1u << 6 )
#define RAL_SKY_DIRTY_ALL           ( ( 1u << 7 ) - 1u )

#define RAL_SKY_EVENT_CAMERA_CUT      ( 1u << 0 )
#define RAL_SKY_EVENT_MAP_TRANSITION  ( 1u << 1 )
#define RAL_SKY_EVENT_DEVICE_RECREATE ( 1u << 2 )
#define RAL_SKY_EVENT_SOURCE_CHANGE   ( 1u << 3 )
#define RAL_SKY_EVENT_TIME_CHANGE     ( 1u << 4 )
#define RAL_SKY_EVENT_WEATHER_CHANGE  ( 1u << 5 )
#define RAL_SKY_EVENT_ALL             ( ( 1u << 6 ) - 1u )

#define RAL_SKY_PASS_AUTHORED_BACKGROUND ( 1u << 0 )
#define RAL_SKY_PASS_TRANSMITTANCE       ( 1u << 1 )
#define RAL_SKY_PASS_MULTISCATTER        ( 1u << 2 )
#define RAL_SKY_PASS_SKY_VIEW            ( 1u << 3 )
#define RAL_SKY_PASS_RADIANCE_CAPTURE    ( 1u << 4 )
#define RAL_SKY_PASS_AMBIENT_SH          ( 1u << 5 )
#define RAL_SKY_PASS_SPECULAR_IBL        ( 1u << 6 )
#define RAL_SKY_PASS_CLOUD_LIGHT         ( 1u << 7 )
#define RAL_SKY_PASS_DISTANT_SCENE       ( 1u << 8 )
#define RAL_SKY_PASS_CROSSFADE           ( 1u << 9 )

typedef struct {
	qboolean physicalLuts;
	qboolean radianceCapture;
	qboolean convolution;
	qboolean cloudLighting;
	qboolean distantScene;
	qboolean temporalCrossfade;
} ralSkyEnvironmentCapabilities_t;

/* Bounded sky-radiance work owned by the existing atmosphere system.  These
 * revisions are content identities, never backend-native resource handles. */
typedef struct {
	uint32_t schemaVersion;
	ralBackendType_t backendType;
	uint64_t frameGeneration;
	uint32_t requestedTier; /* ralAtmosphereTier_t; declared below */
	ralSkyEnvironmentMode_t mode;
	uint64_t sourceRevision;
	uint64_t currentRadianceRevision;
	uint64_t previousRadianceRevision;
	uint32_t dirtyMask;
	uint32_t inFlightWorkMask;
	uint32_t completedProductMask;
	uint32_t eventMask;
	uint32_t maxWorkItems;
	float crossfadeSeconds;
	float crossfadeProgress;
	float sunDirection[3];
	float sunRadiance[3];
	float moonDirection[3];
	float moonRadiance[3];
	float ambientSh[27];
	float cloudSunOcclusion;
	float skyLuminance;
	float exposure;
	float opticsVisibility;
	uint64_t distantSceneIdentity;
	float distantSceneOrigin[3];
	float distantSceneScale;
	float distantSceneLodBias;
	qboolean authoredSkyAvailable;
	qboolean distantSceneRequested;
	ralSkyEnvironmentCapabilities_t capabilities;
} ralSkyEnvironmentPlanRequest_t;

typedef struct {
	uint32_t schemaVersion;
	ralBackendType_t backendType;
	uint64_t frameGeneration;
	ralSkyEnvironmentMode_t requestedMode;
	ralSkyEnvironmentMode_t selectedMode;
	ralSkyEnvironmentFallbackReason_t fallbackReason;
	uint64_t sourceRevision;
	uint64_t currentRadianceRevision;
	uint64_t previousRadianceRevision;
	uint32_t passMask;
	uint32_t admittedWorkMask;
	uint32_t deferredWorkMask;
	uint32_t cancelledWorkMask;
	uint32_t completedProductMask;
	uint32_t admittedWorkCount;
	uint32_t deferredWorkCount;
	float crossfadeAlpha;
	float sunDirection[3];
	float sunRadiance[3];
	float moonDirection[3];
	float moonRadiance[3];
	float ambientSh[27];
	float cloudSunOcclusion;
	float skyLuminance;
	float exposure;
	float opticsVisibility;
	uint64_t distantSceneIdentity;
	float distantSceneOrigin[3];
	float distantSceneScale;
	float distantSceneLodBias;
	qboolean distantSceneActive;
	qboolean previousProductValid;
	qboolean zeroWork;
	qboolean ready;
} ralSkyEnvironmentPlanReceipt_t;

/* Backend-neutral decision for presenting the authored sky and the completed
 * physical sky-view product. Native texture/view handles remain adapter-owned;
 * this receipt only decides whether the product is safe to sample and how much
 * of it may be composed. HYBRID deliberately preserves the authored sky as the
 * visible background while still allowing physical lighting products. */
#define RAL_SKY_SURFACE_RECEIPT_SCHEMA_VERSION 2u

typedef struct {
	uint32_t schemaVersion;
	ralBackendType_t backendType;
	uint64_t frameGeneration;
	uint64_t sourceRevision;
	uint64_t physicalProductRevision;
	ralSkyEnvironmentMode_t selectedMode;
	float physicalBlend;
	qboolean authoredComposite;
	qboolean authoredFallback;
	qboolean physicalReadable;
	qboolean ready;
} ralSkySurfaceReceipt_t;

/* Backend-owned product lifecycle for the shared sky authority.  A plan only
 * moves work from pending to in-flight; the backend must report actual GPU
 * completion on a later advance before the target radiance revision becomes
 * current.  This prevents a planner-only backend from publishing fictional
 * LUT/IBL readiness. */
typedef struct {
	uint32_t schemaVersion;
	ralBackendType_t backendType;
	uint64_t stateGeneration;
	uint64_t lastFrameGeneration;
	uint64_t sourceRevision;
	uint64_t targetRadianceRevision;
	uint64_t currentRadianceRevision;
	uint64_t previousRadianceRevision;
	ralSkyEnvironmentMode_t mode;
	uint32_t pendingDirtyMask;
	uint32_t inFlightWorkMask;
	uint32_t completedProductMask;
	float crossfadeSeconds;
	float crossfadeProgress;
	float timelineSeconds;
	qboolean targetReady;
} ralSkyEnvironmentRuntimeState_t;

typedef struct {
	uint32_t schemaVersion;
	ralBackendType_t backendType;
	uint64_t frameGeneration;
	uint32_t requestedTier; /* ralAtmosphereTier_t */
	ralSkyEnvironmentMode_t mode;
	uint64_t sourceRevision;
	uint32_t eventMask;
	uint32_t completedWorkMask;
	uint32_t maxWorkItems;
	float deltaSeconds;
	float timelineSeconds;
	float crossfadeSeconds;
	float sunDirection[3];
	float sunRadiance[3];
	float moonDirection[3];
	float moonRadiance[3];
	float ambientSh[27];
	float cloudSunOcclusion;
	float skyLuminance;
	float exposure;
	float opticsVisibility;
	uint64_t distantSceneIdentity;
	float distantSceneOrigin[3];
	float distantSceneScale;
	float distantSceneLodBias;
	qboolean authoredSkyAvailable;
	qboolean distantSceneRequested;
	ralSkyEnvironmentCapabilities_t capabilities;
} ralSkyEnvironmentRuntimeRequest_t;

typedef struct {
	uint32_t schemaVersion;
	ralBackendType_t backendType;
	uint64_t stateGeneration;
	uint64_t frameGeneration;
	uint64_t sourceRevision;
	uint32_t invalidatedWorkMask;
	uint32_t cancelledWorkMask;
	uint32_t acceptedCompletionMask;
	qboolean sourceChanged;
	qboolean targetReady;
	qboolean ready;
} ralSkyEnvironmentRuntimeReceipt_t;

/* Distant-scene geometry reuses the ordinary world-draw representation, but
 * it is admitted through a separate draw class.  The request may reference
 * frontend-owned candidates; the receipt is pointer-free, bounded and carries
 * no gameplay entity or collision work.  projectedCoverageQ16 is screen-area
 * coverage in [1, 65536] for visible candidates and keeps LOD selection exact
 * across Vulkan, Metal, OpenGL and WebGPU. */
typedef enum {
	RAL_SKY_DRAW_CLASS_NONE = 0,
	RAL_SKY_DRAW_CLASS_DISTANT_SCENE
} ralSkyDrawClass_t;

typedef struct {
	uint64_t drawIdentity;
	uint32_t sourceDrawIndex;
	uint32_t projectedCoverageQ16;
	uint32_t lodLevelCount;
	qboolean visible;
} ralSkyDistantDrawCandidate_t;

typedef struct {
	uint32_t schemaVersion;
	ralSkyEnvironmentPlanReceipt_t skyEnvironment;
	const ralSkyDistantDrawCandidate_t *candidates;
	uint32_t candidateCount;
	uint32_t maxVisibleDraws;
} ralSkyDistantDrawPlanRequest_t;

typedef struct {
	uint64_t drawIdentity;
	uint32_t sourceDrawIndex;
	uint32_t lodLevel;
} ralSkyDistantDrawSelection_t;

typedef struct {
	uint32_t schemaVersion;
	ralBackendType_t backendType;
	uint64_t frameGeneration;
	uint64_t distantSceneIdentity;
	ralSkyDrawClass_t drawClass;
	uint32_t candidateCount;
	uint32_t maxVisibleDraws;
	uint32_t visibleCount;
	uint32_t admittedCount;
	uint32_t visibilityCulledCount;
	uint32_t budgetDroppedCount;
	uint32_t collisionQueryCount;
	uint32_t entityDrawCount;
	float distantSceneOrigin[3];
	float distantSceneScale;
	float distantSceneLodBias;
	ralSkyDistantDrawSelection_t selections[RAL_SKY_DISTANT_MAX_VISIBLE_DRAWS];
	qboolean ready;
} ralSkyDistantDrawPlanReceipt_t;

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

#define RAL_SKY_TIER_BUDGET_SCHEMA_VERSION 1u
#define RAL_SKY_TIER_MAX_RADIANCE_SLICES_PER_FRAME 4u
#define RAL_SKY_GPU_TELEMETRY_SCHEMA_VERSION 1u

/* Exact, backend-neutral sky work policy for each atmosphere tier.  This is
 * a work/capability receipt, never a native resource description. */
typedef struct {
	uint32_t schemaVersion;
	ralAtmosphereTier_t tier;
	uint32_t allowedWorkMask;
	uint32_t maxWorkItemsPerFrame;
	uint32_t maxRadianceSlicesPerFrame;
	qboolean physicalMode;
	qboolean temporalCrossfade;
	qboolean distantScene;
	qboolean ready;
} ralSkyTierBudgetReceipt_t;

/* GPU timing is never synthesized from CPU wall time. A backend which cannot
 * issue or resolve native timestamps publishes UNAVAILABLE; PENDING and FAILED
 * likewise carry a zero duration. */
typedef enum {
	RAL_SKY_GPU_TIMING_UNAVAILABLE = 0,
	RAL_SKY_GPU_TIMING_PENDING,
	RAL_SKY_GPU_TIMING_VALID,
	RAL_SKY_GPU_TIMING_FAILED
} ralSkyGpuTimingState_t;

typedef struct {
	uint32_t schemaVersion;
	ralBackendType_t backendType;
	uint64_t frameGeneration;
	uint64_t sourceRevision;
	ralAtmosphereTier_t selectedTier;
	ralSkyEnvironmentMode_t selectedMode;
	uint32_t submittedWorkMask;
	uint32_t completedWorkMask;
	uint32_t deferredWorkMask;
	uint32_t cancelledWorkMask;
	uint32_t submittedSliceMask;
	uint32_t completedSliceMask;
	uint32_t deferredSliceCount;
	uint32_t cancelledSliceCount;
	uint64_t currentRadianceRevision;
	uint64_t targetRadianceRevision;
	uint64_t physicalTimingFrameGeneration;
	uint64_t radianceTimingFrameGeneration;
	ralSkyGpuTimingState_t physicalTimingState;
	ralSkyGpuTimingState_t radianceTimingState;
	uint64_t physicalGpuDurationNanoseconds;
	uint64_t radianceGpuDurationNanoseconds;
	qboolean ready;
} ralSkyGpuTelemetryReceipt_t;

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
// Explicit sun/time-of-day and weather-tier transitions: the producer declares
// them when its quantized sun/weather digest changes; undeclared digest drift
// fails closed like an undeclared resize.
#define RAL_ATMOSPHERE_EVENT_SUN_CHANGE			  ( 1u << 8 )
#define RAL_ATMOSPHERE_EVENT_WEATHER_TRANSITION	  ( 1u << 9 )

#define RAL_ATMOSPHERE_INVALIDATE_RESIZE	 ( 1u << 0 )
#define RAL_ATMOSPHERE_INVALIDATE_CAMERA	 ( 1u << 1 )
#define RAL_ATMOSPHERE_INVALIDATE_MAP		 ( 1u << 2 )
#define RAL_ATMOSPHERE_INVALIDATE_DEVICE	 ( 1u << 3 )
#define RAL_ATMOSPHERE_INVALIDATE_CAPABILITY ( 1u << 4 )
#define RAL_ATMOSPHERE_INVALIDATE_TIMELINE	 ( 1u << 5 )
#define RAL_ATMOSPHERE_INVALIDATE_SUN		 ( 1u << 6 )
#define RAL_ATMOSPHERE_INVALIDATE_WEATHER	 ( 1u << 7 )

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
	/* Optional schema-versioned sky authority. schemaVersion == 0 preserves
	 * the pre-TASK-239 authored-sky path without additional work. */
	ralSkyEnvironmentPlanRequest_t skyEnvironment;
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
	ralSkyEnvironmentPlanReceipt_t skyEnvironment;
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
	uint64_t		 sunDigest;		// quantized sun direction/radiance identity (0 = unused)
	uint64_t		 weatherDigest;	// quantized weather tier/precipitation identity (0 = unused)
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
	uint64_t		 sunDigest;
	uint64_t		 weatherDigest;
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
qboolean Ral_SkyTierBudget( ralAtmosphereTier_t tier,
		ralSkyTierBudgetReceipt_t *outReceipt );
qboolean Ral_SkyTierBudgetReceiptExact(
	const ralSkyTierBudgetReceipt_t *a,
	const ralSkyTierBudgetReceipt_t *b );
qboolean Ral_SkyGpuTelemetryReceiptExact(
	const ralSkyGpuTelemetryReceipt_t *a,
	const ralSkyGpuTelemetryReceipt_t *b );
qboolean Ral_SkyEnvironmentPlan( const ralSkyEnvironmentPlanRequest_t *request,
		ralSkyEnvironmentPlanReceipt_t *outReceipt );
qboolean Ral_SkyEnvironmentPlanReceiptExact( const ralSkyEnvironmentPlanReceipt_t *a,
											 const ralSkyEnvironmentPlanReceipt_t *b );
/* Capture consumes the completed physical LUT before the radiance target can
 * be published. Display readiness additionally requires that publication. */
/* Global scale (0..1, default 1) applied to the PHYSICAL surface blend so the
 * authored composition can be relit more gently (r_skyPhysicalBlend). */
qboolean Ral_SkySurfaceSetPhysicalBlendScale( float scale );
float Ral_SkySurfacePhysicalBlendScale( void );
qboolean Ral_SkyPhysicalSourceReady( const ralSkyEnvironmentPlanReceipt_t *environment,
							qboolean physicalProductReadable );
qboolean Ral_SkySurfacePlan( const ralSkyEnvironmentPlanReceipt_t *environment,
							 qboolean physicalProductReadable,
							 ralSkySurfaceReceipt_t *outReceipt );
qboolean Ral_SkySurfaceReceiptExact( const ralSkySurfaceReceipt_t *a,
									 const ralSkySurfaceReceipt_t *b );
qboolean Ral_SkyEnvironmentRuntimeInit( ralBackendType_t backendType,
										ralSkyEnvironmentRuntimeState_t *outState );
qboolean Ral_SkyEnvironmentRuntimeAdvance( ralSkyEnvironmentRuntimeState_t *state,
										 const ralSkyEnvironmentRuntimeRequest_t *request,
										 ralSkyEnvironmentPlanRequest_t *outPlanRequest,
										 ralSkyEnvironmentRuntimeReceipt_t *outReceipt );
qboolean Ral_SkyEnvironmentRuntimeSchedule( ralSkyEnvironmentRuntimeState_t *state,
										  const ralSkyEnvironmentPlanReceipt_t *plan );
qboolean Ral_SkyEnvironmentRuntimeReceiptExact( const ralSkyEnvironmentRuntimeReceipt_t *a,
											const ralSkyEnvironmentRuntimeReceipt_t *b );
qboolean Ral_SkyDistantDrawPlan( const ralSkyDistantDrawPlanRequest_t *request,
								ralSkyDistantDrawPlanReceipt_t *outReceipt );
qboolean Ral_SkyDistantDrawPlanReceiptExact( const ralSkyDistantDrawPlanReceipt_t *a,
										 const ralSkyDistantDrawPlanReceipt_t *b );
qboolean Ral_AtmospherePlanUnavailable( ralBackendType_t backendType, uint64_t frameGeneration,
										ralAtmosphereTier_t requestedTier, ralAtmospherePlanReceipt_t *outReceipt );
qboolean Ral_AtmospherePlanReceiptExact( const ralAtmospherePlanReceipt_t *a, const ralAtmospherePlanReceipt_t *b );
qboolean Ral_AtmosphereRuntimeInit( ralBackendType_t backendType, ralAtmosphereRuntimeState_t *outState );
qboolean Ral_AtmosphereRuntimeAdvance( ralAtmosphereRuntimeState_t *state, const ralAtmosphereRuntimeRequest_t *request,
									   ralAtmosphereRuntimeReceipt_t *outReceipt );
qboolean Ral_AtmosphereRuntimeReceiptExact( const ralAtmosphereRuntimeReceipt_t *a,
											const ralAtmosphereRuntimeReceipt_t *b );
qboolean Ral_AtmosphereSkyEventMask( uint32_t invalidationMask,
	uint32_t *outSkyEventMask );
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
