// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#ifndef WIRED_RAL_WEBGPU_SKY_H
#define WIRED_RAL_WEBGPU_SKY_H

#include "ral_webgpu_browser_bridge.h"
#include "ral_webgpu_runtime.h"
#include "ral_atmosphere.h"
#include "ral_sky_radiance.h"
#include "render_submission.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RAL_WEBGPU_SKY_SCHEMA_VERSION 1u

typedef struct ralWebGpuSky_s ralWebGpuSky_t;
typedef struct ralWebGpuProduct_s ralWebGpuProduct_t;

typedef struct {
	uint32_t schemaVersion;
	ralBackendType_t backendType;
	uint64_t executorGeneration;
	uint64_t frameGeneration;
	uint64_t worldLifecycleGeneration;
	uint64_t sourceRevision;
	uint32_t workMask;
	ralWebGpuCommandReceipt_t command;
	ralWebGpuSubmissionReceipt_t submission;
	qboolean ready;
} ralWebGpuSkyReceipt_t;

#define RAL_WEBGPU_SKY_RADIANCE_RECEIPT_SCHEMA_VERSION 3u

typedef struct {
	uint32_t schemaVersion;
	ralBackendType_t backendType;
	uint64_t executorGeneration;
	uint64_t frameGeneration;
	uint64_t productRevision;
	uint32_t sliceMask;
	ralSkyRadianceReceipt_t plan;
	ralSkyRadianceConsumerReceipt_t consumer;
	ralWebGpuCommandReceipt_t command;
	ralWebGpuSubmissionReceipt_t submission;
	qboolean ready;
} ralWebGpuSkyRadianceReceipt_t;

qboolean RalWebGpu_SkyCreate( ralWebGpuRuntime_t *runtime,
	ralWebGpuRuntimeReceipt_t *runtimeReceipt,
	ralWebGpuBrowserBridge_t *bridge, uint64_t generation,
	ralWebGpuSky_t **outSky );
void RalWebGpu_SkyDestroy( ralWebGpuSky_t *sky,
	const ralWebGpuRuntimeReceipt_t *runtimeReceipt );
qboolean RalWebGpu_SkyEnsureWorld( ralWebGpuSky_t *sky,
	const ralWebGpuRuntimeReceipt_t *runtimeReceipt,
	uint64_t worldLifecycleGeneration );
qboolean RalWebGpu_SkyUnloadWorld( ralWebGpuSky_t *sky,
	const ralWebGpuRuntimeReceipt_t *runtimeReceipt );
qboolean RalWebGpu_SkyBegin( ralWebGpuSky_t *sky,
	const ralWebGpuRuntimeReceipt_t *runtimeReceipt,
	const renderAtmosphereSnapshot_t *snapshot,
	const ralSkyEnvironmentPlanReceipt_t *plan,
	ralWebGpuSkyReceipt_t *outReceipt );
ralWebGpuAsyncStatus_t RalWebGpu_SkyPoll( ralWebGpuSky_t *sky,
	const ralWebGpuSkyReceipt_t *receipt, uint32_t *outCompletedWorkMask );
qboolean RalWebGpu_SkyRadianceBegin( ralWebGpuSky_t *sky,
	const ralWebGpuRuntimeReceipt_t *runtimeReceipt,
	ralWebGpuProduct_t *product, const renderSubmissionState_t *frontend,
	const ralSkyEnvironmentPlanReceipt_t *plan, ralAtmosphereTier_t tier,
	uint32_t eventMask, float timelineSeconds,
	ralWebGpuSkyRadianceReceipt_t *outReceipt );
ralWebGpuAsyncStatus_t RalWebGpu_SkyRadiancePoll( ralWebGpuSky_t *sky,
	const ralWebGpuRuntimeReceipt_t *runtimeReceipt,
	const ralWebGpuSkyRadianceReceipt_t *receipt,
	uint32_t *outCompletedSliceMask, uint64_t *outCompletedRevision );
qboolean RalWebGpu_SkyRadianceReceiptExact(
	const ralWebGpuSkyRadianceReceipt_t *a,
	const ralWebGpuSkyRadianceReceipt_t *b );
qboolean RalWebGpu_SkyReceiptExact( const ralWebGpuSkyReceipt_t *a,
	const ralWebGpuSkyReceipt_t *b );
qboolean RalWebGpu_SkyResourcesReady( const ralWebGpuSky_t *sky );
qboolean RalWebGpu_SkyInFlight( const ralWebGpuSky_t *sky );
qboolean RalWebGpu_SkyLastPhysicalGpuTiming( const ralWebGpuSky_t *sky,
	ralWebGpuGpuTimingState_t *outState, uint64_t *outDurationNanoseconds );
qboolean RalWebGpu_SkyLastRadianceGpuTiming( const ralWebGpuSky_t *sky,
	ralWebGpuGpuTimingState_t *outState, uint64_t *outDurationNanoseconds );
qboolean RalWebGpu_SkySampleResources( const ralWebGpuSky_t *sky,
	uintptr_t *outTextureIdentity, uintptr_t *outSamplerIdentity,
	uint64_t *outWorldLifecycleGeneration );
qboolean RalWebGpu_SkyFallbackResources( const ralWebGpuSky_t *sky,
	uintptr_t *outTextureIdentity, uintptr_t *outSamplerIdentity );
qboolean RalWebGpu_SkyRadianceFallbackResources( const ralWebGpuSky_t *sky,
	uintptr_t *outTextureIdentity, uintptr_t *outSamplerIdentity );

#ifdef __cplusplus
}
#endif

#endif
