// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#ifndef WIRED_RAL_WEBGPU_ATMOSPHERE_H
#define WIRED_RAL_WEBGPU_ATMOSPHERE_H

#include "ral_webgpu_browser_bridge.h"
#include "ral_webgpu_runtime.h"
#include "ral_atmosphere.h"
#include "render_submission.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RAL_WEBGPU_ATMOSPHERE_SCHEMA_VERSION 1u
#define RAL_WEBGPU_ATMOSPHERE_FROXEL_CAPACITY 262144u

typedef struct ralWebGpuAtmosphere_s ralWebGpuAtmosphere_t;

typedef struct {
	uint32_t schemaVersion;
	ralBackendType_t backendType;
	uint64_t frameGeneration;
	uint64_t executorGeneration;
	ralAtmospherePlanReceipt_t plan;
	ralWebGpuCommandReceipt_t command;
	ralWebGpuSubmissionReceipt_t submission;
	uint32_t dispatchCount;
	uint32_t froxelCount;
	qboolean historyReused;
	qboolean ready;
} ralWebGpuAtmosphereReceipt_t;

qboolean RalWebGpu_AtmosphereCreate( ralWebGpuRuntime_t *runtime,
	ralWebGpuRuntimeReceipt_t *runtimeReceipt,
	ralWebGpuBrowserBridge_t *bridge, uint64_t generation,
	ralWebGpuAtmosphere_t **outAtmosphere );
void RalWebGpu_AtmosphereDestroy( ralWebGpuAtmosphere_t *atmosphere,
	const ralWebGpuRuntimeReceipt_t *runtimeReceipt );
qboolean RalWebGpu_AtmosphereBegin( ralWebGpuAtmosphere_t *atmosphere,
	const ralWebGpuRuntimeReceipt_t *runtimeReceipt,
	const renderSubmissionState_t *frontend,
	const ralAtmospherePlanReceipt_t *plan,
	ralWebGpuAtmosphereReceipt_t *outReceipt );
ralWebGpuAsyncStatus_t RalWebGpu_AtmospherePoll(
	ralWebGpuAtmosphere_t *atmosphere,
	const ralWebGpuAtmosphereReceipt_t *receipt );
qboolean RalWebGpu_AtmosphereReceiptExact(
	const ralWebGpuAtmosphereReceipt_t *a,
	const ralWebGpuAtmosphereReceipt_t *b );
uintptr_t RalWebGpu_AtmosphereArenaIdentity(
	const ralWebGpuAtmosphere_t *atmosphere );
uint64_t RalWebGpu_AtmosphereArenaBytes(
	const ralWebGpuAtmosphere_t *atmosphere );
uint32_t RalWebGpu_AtmosphereIntegratedBase(
	const ralWebGpuAtmosphere_t *atmosphere );
qboolean RalWebGpu_AtmosphereHistoryValid(
	const ralWebGpuAtmosphere_t *atmosphere );
void RalWebGpu_AtmosphereInvalidateHistory(
	ralWebGpuAtmosphere_t *atmosphere );

#ifdef __cplusplus
}
#endif

#endif
