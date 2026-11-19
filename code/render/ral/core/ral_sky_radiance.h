// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#ifndef WIRED_RAL_SKY_RADIANCE_H
#define WIRED_RAL_SKY_RADIANCE_H

#include "ral_atmosphere.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RAL_SKY_RADIANCE_SCHEMA_VERSION 2u
#define RAL_SKY_RADIANCE_RECEIPT_SCHEMA_VERSION 3u
#define RAL_SKY_RADIANCE_CONSUMER_RECEIPT_SCHEMA_VERSION 2u
#define RAL_SKY_RADIANCE_CAPTURE_FACE_COUNT 6u
#define RAL_SKY_RADIANCE_AMBIENT_SH_COEFFICIENT_COUNT 9u
#define RAL_SKY_RADIANCE_MAX_SPECULAR_MIPS 12u
#define RAL_SKY_RADIANCE_MAX_SLICES_PER_FRAME 4u

/* One production quality profile for every adapter. Specular spans the full
 * 128..1 chain; changing the capture size must not truncate rough surfaces.
 * The planner's independent MAX_SPECULAR_MIPS remains a capacity bound. */
#define WIRED_SKY_SOURCE_SIZE 128u
#define WIRED_SKY_IRRADIANCE_SIZE 32u
#define WIRED_SKY_SPECULAR_SIZE 128u
#define WIRED_SKY_SPECULAR_MIP_COUNT 8u

#define RAL_SKY_RADIANCE_CAPTURE_MASK 0x0000003fu
#define RAL_SKY_RADIANCE_AMBIENT_SH_MASK 0x00007fc0u
#define RAL_SKY_RADIANCE_SPECULAR_MASK 0x07ff8000u
#define RAL_SKY_RADIANCE_ALL_SLICE_MASK 0x07ffffffu

/* A published environment product is consumed as one atomic cohort.  Keep
 * these logical capabilities handle-free: adapters prove native readability
 * with this mask without leaking images, views or buffers into RAL core. */
#define RAL_SKY_RADIANCE_PRODUCT_VISIBLE_RADIANCE 0x01u
#define RAL_SKY_RADIANCE_PRODUCT_DIFFUSE_IRRADIANCE 0x02u
#define RAL_SKY_RADIANCE_PRODUCT_AMBIENT_SH 0x04u
#define RAL_SKY_RADIANCE_PRODUCT_SPECULAR_RADIANCE 0x08u
#define RAL_SKY_RADIANCE_PRODUCT_ALL 0x0fu

typedef enum {
	RAL_SKY_SOURCE_ANALYTIC = 0,
	RAL_SKY_SOURCE_AUTHORED_FACES = 1,
	/* Linear HDR output of the visible material/physical composition. Its
	 * lighting and exposure are already applied, not another outer-box input. */
	RAL_SKY_SOURCE_COMPOSITED_RADIANCE = 2
} ralSkyRadianceSourceType_t;

/* Native-free, 64-byte source_sky push/uniform payload.  Cloud coverage
 * comes from the same atmosphere snapshot as the sky plan.  Only the direct
 * sun lobe is attenuated; authored pixels/ambient and exposure stay intact. */
typedef struct {
	uint32_t meta[4];
	float authoredRadiance[4];
	float sunDirectionIntensity[4];
	float sunRadianceExposure[4];
} ralSkyRadianceSourceParams_t;

qboolean Ral_SkyRadianceSourceParamsBuild(
		const ralSkyEnvironmentPlanReceipt_t *sky, float cloudCoverage,
		ralSkyRadianceSourceType_t sourceType, ralSkyRadianceSourceParams_t *outParams );

/* Capture camera axes (forward, left, up), in authored Q3 rt/bk/lf/ft/up/dn
 * order. The resulting top-left-origin face images feed the same source-cube
 * sampler on every backend. Invalid faces leave the output unchanged. */
qboolean Ral_SkyRadianceCaptureAxes( uint32_t face, float outAxes[3][3] );

typedef struct {
	uint32_t schemaVersion;
	ralBackendType_t backendType;
	uint64_t stateGeneration;
	uint64_t lastFrameGeneration;
	uint64_t sourceRevision;
	uint64_t nextProductRevision;
	uint64_t targetProductRevision;
	uint64_t currentProductRevision;
	uint64_t previousProductRevision;
	ralSkyEnvironmentMode_t mode;
	uint32_t specularMipCount;
	uint32_t pendingSliceMask;
	uint32_t inFlightSliceMask;
	uint32_t completedSliceMask;
	float crossfadeSeconds;
	float crossfadeProgress;
	float secondsSinceCapture;
	qboolean targetReady;
} ralSkyRadianceState_t;

typedef struct {
	uint32_t schemaVersion;
	ralBackendType_t backendType;
	uint64_t frameGeneration;
	uint64_t sourceRevision;
	uint64_t completedProductRevision;
	ralSkyEnvironmentMode_t mode;
	uint32_t specularMipCount;
	uint32_t eventMask;
	uint32_t completedSliceMask;
	uint32_t maxSlicesPerFrame;
	float deltaSeconds;
	float crossfadeSeconds;
	/* Zero: immutable source. Positive: minimum time between animated
	 * capture cohorts; never cancels in-flight work or a sampled crossfade. */
	float refreshIntervalSeconds;
} ralSkyRadianceRequest_t;

typedef struct {
	uint32_t schemaVersion;
	ralBackendType_t backendType;
	uint64_t stateGeneration;
	uint64_t frameGeneration;
	uint64_t sourceRevision;
	uint64_t targetProductRevision;
	uint64_t currentProductRevision;
	uint64_t previousProductRevision;
	uint32_t requiredSliceMask;
	uint32_t admittedSliceMask;
	uint32_t acceptedCompletionMask;
	uint32_t staleCompletionMask;
	uint32_t cancelledSliceMask;
	uint32_t pendingSliceMask;
	uint32_t inFlightSliceMask;
	uint32_t completedSliceMask;
	uint32_t admittedSliceCount;
	uint32_t acceptedCompletionCount;
	uint32_t staleCompletionCount;
	uint32_t cancelledSliceCount;
	uint32_t pendingSliceCount;
	uint32_t inFlightSliceCount;
	uint32_t completedSliceCount;
	float crossfadeAlpha;
	qboolean sourceChanged;
	qboolean refreshStarted;
	qboolean targetPublished;
	qboolean previousProductValid;
	qboolean targetReady;
	qboolean zeroWork;
	qboolean ready;
} ralSkyRadianceReceipt_t;

/*
 * Handle-free publication decision shared by every backend consumer.  The
 * caller supplies completion/readability facts for the logical current and
 * previous products; native image/buffer handles remain adapter-owned.
 */
typedef struct {
	uint32_t schemaVersion;
	ralBackendType_t backendType;
	uint64_t sourceRevision;
	uint64_t currentProductRevision;
	uint64_t previousProductRevision;
	uint32_t requiredProductMask;
	uint32_t currentReadableProductMask;
	uint32_t previousReadableProductMask;
	float previousToCurrentBlend;
	qboolean currentProductReadable;
	qboolean previousProductReadable;
	qboolean usePreviousProduct;
	qboolean ready;
} ralSkyRadianceConsumerReceipt_t;

qboolean Ral_SkyRadianceInit( ralBackendType_t backendType,
		ralSkyRadianceState_t *outState );
qboolean Ral_SkyRadianceAdvance( ralSkyRadianceState_t *state,
		const ralSkyRadianceRequest_t *request,
		ralSkyRadianceReceipt_t *outReceipt );
qboolean Ral_SkyRadianceReceiptValid(
		const ralSkyRadianceReceipt_t *receipt );
qboolean Ral_SkyRadianceReceiptExact( const ralSkyRadianceReceipt_t *a,
		const ralSkyRadianceReceipt_t *b );
qboolean Ral_SkyRadianceConsumerPlan(
		const ralSkyRadianceReceipt_t *radiance,
		uint32_t currentReadableProductMask,
		uint32_t previousReadableProductMask,
		ralSkyRadianceConsumerReceipt_t *outReceipt );
qboolean Ral_SkyRadianceConsumerReceiptExact(
		const ralSkyRadianceConsumerReceipt_t *a,
		const ralSkyRadianceConsumerReceipt_t *b );
/* Frame admission is distinct from GPU residency. Zero is the existing
 * no-consumer marker; a completed cohort may remain cached while withdrawn.
 * Input/output may alias. Invalid input leaves output unchanged. */
qboolean Ral_SkyRadianceConsumerAdmit(
		const ralSkyTierBudgetReceipt_t *budget,
		const ralSkyRadianceConsumerReceipt_t *completed,
		ralSkyRadianceConsumerReceipt_t *outConsumer );

#ifdef __cplusplus
}
#endif

#endif
