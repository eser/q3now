// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef WIRED_RAL_WEBGPU_RUNTIME_H
#define WIRED_RAL_WEBGPU_RUNTIME_H

#include "ral_webgpu_pipeline.h"
#include "ral_webgpu_presentation.h"
#include "ral_webgpu_resource.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RAL_WEBGPU_RUNTIME_SCHEMA_VERSION 1u
#define RAL_WEBGPU_RUNTIME_MAX_PIPELINES 64u

typedef struct ralWebGpuRuntime_s ralWebGpuRuntime_t;

typedef struct {
	ralWebGpuCoreCreateInfo_t core;
	ralWebGpuResourceLayerCreateInfo_t resources;
	ralWebGpuCommandCreateInfo_t command;
	ralWebGpuPresentationCreateInfo_t presentation;
} ralWebGpuRuntimeCreateInfo_t;

typedef struct {
	uint32_t schemaVersion;
	ralBackendType_t backendType;
	uint64_t generation;
	ralWebGpuCoreReceipt_t core;
	uintptr_t resourcesIdentity;
	uintptr_t commandIdentity;
	uintptr_t presentationIdentity;
	uint32_t pipelineCount;
	qboolean ready;
} ralWebGpuRuntimeReceipt_t;

qboolean RalWebGpu_RuntimeBegin( const ralWebGpuRuntimeCreateInfo_t *createInfo,
	ralWebGpuRuntime_t **outRuntime );
ralWebGpuPollStatus_t RalWebGpu_RuntimePoll( ralWebGpuRuntime_t *runtime,
	ralWebGpuRuntimeReceipt_t *outReceipt );
qboolean RalWebGpu_RuntimeGetReceipt( ralWebGpuRuntime_t *runtime,
	ralWebGpuRuntimeReceipt_t *outReceipt );
void RalWebGpu_RuntimeDestroy( ralWebGpuRuntime_t *runtime );
qboolean RalWebGpu_RuntimeReceiptExact( const ralWebGpuRuntimeReceipt_t *a,
	const ralWebGpuRuntimeReceipt_t *b );
qboolean RalWebGpu_RuntimeCreatePipeline( ralWebGpuRuntime_t *runtime,
	const ralWebGpuRuntimeReceipt_t *authority,
	const ralWebGpuPipelineCreateInfo_t *createInfo,
	ralWebGpuPipeline_t **outPipeline,
	ralWebGpuPipelineReceipt_t *outReceipt );
qboolean RalWebGpu_RuntimeDestroyPipeline( ralWebGpuRuntime_t *runtime,
	ralWebGpuPipeline_t *pipeline,
	const ralWebGpuPipelineReceipt_t *authority );
qboolean RalWebGpu_RuntimeOwnsPipeline( ralWebGpuRuntime_t *runtime,
	const ralWebGpuRuntimeReceipt_t *runtimeAuthority,
	const ralWebGpuPipelineReceipt_t *pipelineAuthority );
qboolean RalWebGpu_RuntimePublishDeviceLoss( ralWebGpuRuntime_t *runtime,
	const ralMemoryFailureEvent_t *event,
	ralMemoryFailureReceipt_t *outReceipt );
ralWebGpuResourceLayer_t *RalWebGpu_RuntimeResources(
	ralWebGpuRuntime_t *runtime, const ralWebGpuRuntimeReceipt_t *authority );
ralWebGpuCommand_t *RalWebGpu_RuntimeCommand( ralWebGpuRuntime_t *runtime,
	const ralWebGpuRuntimeReceipt_t *authority );
ralWebGpuPresentation_t *RalWebGpu_RuntimePresentation(
	ralWebGpuRuntime_t *runtime, const ralWebGpuRuntimeReceipt_t *authority );

#ifdef __cplusplus
}
#endif

#endif
