// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_webgpu_runtime.h"

#include <stdlib.h>
#include <string.h>

typedef enum {
	RUNTIME_CORE_PENDING = 1,
	RUNTIME_READY,
	RUNTIME_FAILED,
	RUNTIME_LOST
} runtimeState_t;

typedef struct {
	ralWebGpuPipeline_t *pipeline;
	ralWebGpuPipelineReceipt_t receipt;
} pipelineSlot_t;

struct ralWebGpuRuntime_s {
	ralWebGpuRuntimeCreateInfo_t createInfo;
	ralWebGpuCore_t *core;
	ralWebGpuCoreReceipt_t coreReceipt;
	ralWebGpuResourceLayer_t *resources;
	ralWebGpuCommand_t *command;
	ralWebGpuPresentation_t *presentation;
	pipelineSlot_t pipelines[RAL_WEBGPU_RUNTIME_MAX_PIPELINES];
	uint32_t pipelineCount;
	runtimeState_t state;
};

static qboolean ReceiptValid( const ralWebGpuRuntimeReceipt_t *receipt ) {
	return receipt && receipt->schemaVersion == RAL_WEBGPU_RUNTIME_SCHEMA_VERSION
		&& receipt->backendType == RAL_BACKEND_WEBGPU
		&& receipt->generation && receipt->generation != UINT64_MAX
		&& RalWebGpu_CoreReceiptExact( &receipt->core, &receipt->core )
		&& receipt->generation == receipt->core.generation
		&& receipt->resourcesIdentity && receipt->commandIdentity
		&& receipt->presentationIdentity
		&& receipt->pipelineCount <= RAL_WEBGPU_RUNTIME_MAX_PIPELINES
		&& receipt->ready == qtrue;
}

static void BuildReceipt( const ralWebGpuRuntime_t *runtime,
		ralWebGpuRuntimeReceipt_t *out ) {
	memset( out, 0, sizeof( *out ) );
	out->schemaVersion = RAL_WEBGPU_RUNTIME_SCHEMA_VERSION;
	out->backendType = RAL_BACKEND_WEBGPU;
	out->generation = runtime->coreReceipt.generation;
	out->core = runtime->coreReceipt;
	out->resourcesIdentity = (uintptr_t)runtime->resources;
	out->commandIdentity = (uintptr_t)runtime->command;
	out->presentationIdentity = (uintptr_t)runtime->presentation;
	out->pipelineCount = runtime->pipelineCount; out->ready = qtrue;
}

static void DestroyChildren( ralWebGpuRuntime_t *runtime ) {
	while ( runtime->pipelineCount ) {
		pipelineSlot_t *slot = &runtime->pipelines[runtime->pipelineCount - 1u];
		RalWebGpu_PipelineDestroy( runtime->core, slot->pipeline, &slot->receipt );
		memset( slot, 0, sizeof( *slot ) ); runtime->pipelineCount--;
	}
	if ( runtime->resources ) {
		RalWebGpu_ResourcesDestroy( runtime->resources ); runtime->resources = NULL;
	}
	if ( runtime->command ) {
		RalWebGpu_CommandDestroy( runtime->command ); runtime->command = NULL;
	}
	if ( runtime->presentation ) {
		RalWebGpu_PresentationDestroy( runtime->presentation );
		runtime->presentation = NULL;
	}
}

qboolean RalWebGpu_RuntimeBegin( const ralWebGpuRuntimeCreateInfo_t *createInfo,
		ralWebGpuRuntime_t **outRuntime ) {
	ralWebGpuRuntime_t *runtime;
	if ( !createInfo || !outRuntime ) return qfalse;
	runtime = (ralWebGpuRuntime_t *)calloc( 1u, sizeof( *runtime ) );
	if ( !runtime ) return qfalse;
	runtime->createInfo = *createInfo;
	if ( !RalWebGpu_CoreBegin( &runtime->createInfo.core, &runtime->core ) ) {
		free( runtime ); return qfalse;
	}
	runtime->state = RUNTIME_CORE_PENDING; *outRuntime = runtime; return qtrue;
}

ralWebGpuPollStatus_t RalWebGpu_RuntimePoll( ralWebGpuRuntime_t *runtime,
		ralWebGpuRuntimeReceipt_t *outReceipt ) {
	ralWebGpuPollStatus_t status;
	ralWebGpuRuntimeReceipt_t receipt;
	if ( !runtime ) return RAL_WEBGPU_POLL_FAILED;
	if ( runtime->state == RUNTIME_LOST ) return RAL_WEBGPU_POLL_DEVICE_LOST;
	if ( runtime->state == RUNTIME_FAILED ) return RAL_WEBGPU_POLL_FAILED;
	if ( runtime->state == RUNTIME_CORE_PENDING ) {
		status = RalWebGpu_CorePoll( runtime->core, &runtime->coreReceipt );
		if ( status != RAL_WEBGPU_POLL_READY ) {
			if ( status != RAL_WEBGPU_POLL_PENDING ) runtime->state =
				status == RAL_WEBGPU_POLL_DEVICE_LOST ? RUNTIME_LOST : RUNTIME_FAILED;
			return status;
		}
		if ( !RalWebGpu_ResourcesCreate( runtime->core, &runtime->coreReceipt,
				&runtime->createInfo.resources, &runtime->resources )
				|| !RalWebGpu_CommandCreate( runtime->core, &runtime->coreReceipt,
					&runtime->createInfo.command, &runtime->command )
				|| !RalWebGpu_PresentationCreate( runtime->core, &runtime->coreReceipt,
					&runtime->createInfo.presentation, &runtime->presentation ) ) {
			DestroyChildren( runtime ); RalWebGpu_CoreDestroy( runtime->core );
			runtime->core = NULL; runtime->state = RUNTIME_FAILED;
			return RAL_WEBGPU_POLL_FAILED;
		}
		runtime->state = RUNTIME_READY;
	}
	if ( runtime->state != RUNTIME_READY || !outReceipt )
		return RAL_WEBGPU_POLL_FAILED;
	BuildReceipt( runtime, &receipt );
	if ( !ReceiptValid( &receipt ) ) return RAL_WEBGPU_POLL_FAILED;
	*outReceipt = receipt; return RAL_WEBGPU_POLL_READY;
}

qboolean RalWebGpu_RuntimeGetReceipt( ralWebGpuRuntime_t *runtime,
		ralWebGpuRuntimeReceipt_t *outReceipt ) {
	ralWebGpuRuntimeReceipt_t receipt;
	if ( !runtime || runtime->state != RUNTIME_READY || !outReceipt
			|| !RalWebGpu_CoreMatchesReceipt( runtime->core,
				&runtime->coreReceipt ) ) return qfalse;
	BuildReceipt( runtime, &receipt );
	if ( !ReceiptValid( &receipt ) ) return qfalse;
	*outReceipt = receipt; return qtrue;
}

void RalWebGpu_RuntimeDestroy( ralWebGpuRuntime_t *runtime ) {
	if ( !runtime ) return;
	DestroyChildren( runtime );
	if ( runtime->core ) RalWebGpu_CoreDestroy( runtime->core );
	memset( runtime, 0, sizeof( *runtime ) ); free( runtime );
}

qboolean RalWebGpu_RuntimeReceiptExact( const ralWebGpuRuntimeReceipt_t *a,
		const ralWebGpuRuntimeReceipt_t *b ) {
	return ReceiptValid( a ) && ReceiptValid( b )
		&& !memcmp( a, b, sizeof( *a ) );
}

static qboolean Matches( ralWebGpuRuntime_t *runtime,
		const ralWebGpuRuntimeReceipt_t *authority ) {
	ralWebGpuRuntimeReceipt_t current;
	if ( !runtime || runtime->state != RUNTIME_READY || !authority
			|| !RalWebGpu_CoreMatchesReceipt( runtime->core,
				&runtime->coreReceipt ) ) return qfalse;
	BuildReceipt( runtime, &current );
	return RalWebGpu_RuntimeReceiptExact( &current, authority );
}

qboolean RalWebGpu_RuntimeCreatePipeline( ralWebGpuRuntime_t *runtime,
		const ralWebGpuRuntimeReceipt_t *authority,
		const ralWebGpuPipelineCreateInfo_t *createInfo,
		ralWebGpuPipeline_t **outPipeline,
		ralWebGpuPipelineReceipt_t *outReceipt ) {
	ralWebGpuPipeline_t *pipeline;
	ralWebGpuPipelineReceipt_t receipt;
	pipelineSlot_t *slot;
	if ( !Matches( runtime, authority ) || !createInfo || !outPipeline
			|| !outReceipt
			|| runtime->pipelineCount >= RAL_WEBGPU_RUNTIME_MAX_PIPELINES
			|| !RalWebGpu_PipelineCreate( runtime->core, &runtime->coreReceipt,
				createInfo, &pipeline, &receipt ) ) return qfalse;
	slot = &runtime->pipelines[runtime->pipelineCount++];
	slot->pipeline = pipeline; slot->receipt = receipt;
	*outPipeline = pipeline; *outReceipt = receipt; return qtrue;
}

qboolean RalWebGpu_RuntimeDestroyPipeline( ralWebGpuRuntime_t *runtime,
		ralWebGpuPipeline_t *pipeline,
		const ralWebGpuPipelineReceipt_t *authority ) {
	uint32_t i;
	if ( !runtime || !pipeline || !authority ) return qfalse;
	for ( i = 0u; i < runtime->pipelineCount; ++i ) {
		if ( runtime->pipelines[i].pipeline != pipeline ) continue;
		if ( !RalWebGpu_PipelineDestroy( runtime->core, pipeline, authority ) )
			return qfalse;
		memmove( &runtime->pipelines[i], &runtime->pipelines[i + 1u],
			( runtime->pipelineCount - i - 1u ) * sizeof( runtime->pipelines[0] ) );
		runtime->pipelineCount--;
		memset( &runtime->pipelines[runtime->pipelineCount], 0,
			sizeof( runtime->pipelines[0] ) ); return qtrue;
	}
	return qfalse;
}

qboolean RalWebGpu_RuntimeOwnsPipeline( ralWebGpuRuntime_t *runtime,
		const ralWebGpuRuntimeReceipt_t *runtimeAuthority,
		const ralWebGpuPipelineReceipt_t *pipelineAuthority ) {
	if ( !Matches( runtime, runtimeAuthority ) || !pipelineAuthority )
		return qfalse;
	for ( uint32_t i = 0u; i < runtime->pipelineCount; ++i ) {
		if ( RalWebGpu_PipelineReceiptExact( &runtime->pipelines[i].receipt,
				pipelineAuthority ) ) return qtrue;
	}
	return qfalse;
}

qboolean RalWebGpu_RuntimePublishDeviceLoss( ralWebGpuRuntime_t *runtime,
		const ralMemoryFailureEvent_t *event,
		ralMemoryFailureReceipt_t *outReceipt ) {
	if ( !runtime || runtime->state != RUNTIME_READY
			|| !RalWebGpu_CorePublishDeviceLoss( runtime->core, event, outReceipt ) )
		return qfalse;
	runtime->state = RUNTIME_LOST; return qtrue;
}

ralWebGpuResourceLayer_t *RalWebGpu_RuntimeResources(
		ralWebGpuRuntime_t *runtime,
		const ralWebGpuRuntimeReceipt_t *authority ) {
	return Matches( runtime, authority ) ? runtime->resources : NULL;
}
ralWebGpuCommand_t *RalWebGpu_RuntimeCommand( ralWebGpuRuntime_t *runtime,
		const ralWebGpuRuntimeReceipt_t *authority ) {
	return Matches( runtime, authority ) ? runtime->command : NULL;
}
ralWebGpuPresentation_t *RalWebGpu_RuntimePresentation(
		ralWebGpuRuntime_t *runtime,
		const ralWebGpuRuntimeReceipt_t *authority ) {
	return Matches( runtime, authority ) ? runtime->presentation : NULL;
}
