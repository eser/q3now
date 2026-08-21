// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_frame_graph_native.h"

#include "../ral/ral.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#define NATIVE_BUDGET_BYTES (UINT64_C(64) * 1024u * 1024u)

typedef struct {
	uint32_t identityTokens[RAL_FRAME_GRAPH_NATIVE_TEXTURE_COUNT];
	ralTransientTextureRequest_t requests[RAL_FRAME_GRAPH_NATIVE_TEXTURE_COUNT];
	ralTransientTextureCohortReceipt_t probeReceipt;
	ralFrameGraphDescription_t graphDescription;
	ralFrameGraphPlan_t graphPlan;
	ralFrameGraphTransientCreateInfo_t materialCreate;
	ralFrameGraphTransientReceipt_t materialReceipt;
	ralFrameGraphResourceBinding_t bindings[RAL_FRAME_GRAPH_NATIVE_TEXTURE_COUNT];
	uint32_t bindingCount;
	ralFrameGraphExecutionDescription_t executionDescription;
	ralFrameGraphExecutionPlan_t executionPlan;
	ralFrameGraphRecordDescription_t recordDescription;
	ralFrameGraphRecordingReceipt_t recordingReceipt;
	ralFrameGraphSubmitDescription_t submitDescription;
	ralFrameGraphSubmissionReceipt_t submissionReceipt;
	ralFrameGraphSubmissionFailureReceipt_t submissionFailure;
} nativeState_t;

static qboolean PassPreflight( void *context,
		const ralFrameGraphExecutionPass_t *pass,
		const ralFrameGraphExecutionPlan_t *plan,
		ralCommandBuffer_t *command ) {
	(void)context;
	return pass && plan && command && pass->topologicalIndex<plan->passCount
		&& pass->submissionBatchIndex<plan->submissionBatchCount ? qtrue:qfalse;
}

static void PassRecord( void *context,
		const ralFrameGraphExecutionPass_t *pass,
		const ralFrameGraphExecutionPlan_t *plan,
		ralCommandBuffer_t *command ) {
	(void)context;(void)pass;(void)plan;(void)command;
}

static void BuildRequests( nativeState_t *state, uint64_t generation ) {
	static const char *const names[RAL_FRAME_GRAPH_NATIVE_TEXTURE_COUNT]={
		"ral.framegraph.native.a","ral.framegraph.native.b",
		"ral.framegraph.native.c"
	};
	uint32_t i;
	for ( i=0u; i<RAL_FRAME_GRAPH_NATIVE_TEXTURE_COUNT; ++i ) {
		ralTransientTextureRequest_t *request=&state->requests[i];
		memset(request,0,sizeof(*request));
		state->identityTokens[i]=i+1u;
		request->resourceIdentity=(uintptr_t)&state->identityTokens[i];
		request->resourceGeneration=generation;
		request->texture.type=RAL_TEXTURE_2D;
		request->texture.format=RAL_FORMAT_R8G8B8A8_UNORM;
		request->texture.width=64u;request->texture.height=64u;
		request->texture.depthOrArrayLayers=1u;
		request->texture.mipLevels=1u;request->texture.sampleCount=1u;
		request->texture.usage=(ralTextureUsage_t)(RAL_TEXTURE_USAGE_COLOR_ATTACHMENT
			|RAL_TEXTURE_USAGE_SAMPLED);
		request->texture.memory=RAL_MEMORY_LAZY_ALLOC;
		request->texture.debugName=names[i];
		request->firstPass=i*2u;request->lastPass=i*2u+1u;
		request->allowAlias=qtrue;
	}
}

static qboolean ProbeRequirements( ralBackend_t *backend, nativeState_t *state,
		uint64_t generation ) {
	ralTransientTextureCohortCreateInfo_t ci;
	ralTransientTextureCohort_t *probe=NULL;
	qboolean result=qfalse;
	memset(&ci,0,sizeof(ci));
	ci.policy.backendType=RAL_BACKEND_VULKAN;
	ci.policy.generation=generation;
	ci.policy.budgetBytes=NATIVE_BUDGET_BYTES;
	ci.policy.explicitAliasing=qtrue;
	ci.requests=state->requests;
	ci.requestCount=RAL_FRAME_GRAPH_NATIVE_TEXTURE_COUNT;
	probe=Ral_CreateTransientTextureCohort(backend,&ci);
	if ( probe && Ral_TransientTextureCohortGetReceipt(probe,&state->probeReceipt)
			&& state->probeReceipt.plan.outcome==RAL_TRANSIENT_PLAN_EXPLICIT_ALIAS
			&& state->probeReceipt.textureCount==RAL_FRAME_GRAPH_NATIVE_TEXTURE_COUNT
			&& state->probeReceipt.allocationCount==1u ) result=qtrue;
	if ( probe && !Ral_TransientTextureCohortReleaseFresh(&probe) ) result=qfalse;
	return result && !probe ? qtrue:qfalse;
}

static qboolean BuildGraph( nativeState_t *state, uint64_t generation ) {
	static const uint32_t passIds[RAL_FRAME_GRAPH_NATIVE_PASS_COUNT]={
		10u,20u,30u,40u,50u,60u
	};
	uint32_t i;
	ralFrameGraphDescription_t *description=&state->graphDescription;
	memset(description,0,sizeof(*description));
	description->schemaVersion=RAL_FRAME_GRAPH_SCHEMA_VERSION;
	description->generation=generation;
	description->backendType=RAL_BACKEND_VULKAN;
	description->resourceCount=RAL_FRAME_GRAPH_NATIVE_TEXTURE_COUNT;
	for ( i=0u; i<RAL_FRAME_GRAPH_NATIVE_TEXTURE_COUNT; ++i ) {
		const ralTransientRequest_t *facts
			=&state->probeReceipt.plan.assignments[i].request;
		ralFrameGraphResource_t *resource=&description->resources[i];
		if ( facts->resourceIdentity!=state->requests[i].resourceIdentity
				|| facts->resourceGeneration!=generation
				|| facts->firstPass!=state->requests[i].firstPass
				|| facts->lastPass!=state->requests[i].lastPass ) return qfalse;
		resource->id=i+1u;resource->generation=generation;
		resource->identity=facts->resourceIdentity;
		resource->kind=RAL_FRAME_GRAPH_RESOURCE_TEXTURE;
		resource->lifetime=RAL_FRAME_GRAPH_RESOURCE_TRANSIENT;
		resource->compatibilityKey=facts->compatibilityKey;
		resource->size=facts->size;resource->alignment=facts->alignment;
		resource->allowAlias=qtrue;
		resource->initialState.usage=RAL_RESOURCE_USAGE_UNDEFINED;
		resource->initialQueue=RAL_QUEUE_GRAPHICS;
	}
	description->passCount=RAL_FRAME_GRAPH_NATIVE_PASS_COUNT;
	for ( i=0u; i<RAL_FRAME_GRAPH_NATIVE_PASS_COUNT; ++i ) {
		description->passes[i].id=passIds[i];
		description->passes[i].queue=RAL_QUEUE_GRAPHICS;
	}
	description->useCount=RAL_FRAME_GRAPH_NATIVE_PASS_COUNT;
	for ( i=0u; i<RAL_FRAME_GRAPH_NATIVE_TEXTURE_COUNT; ++i ) {
		ralFrameGraphUse_t *write=&description->uses[i*2u];
		ralFrameGraphUse_t *read=&description->uses[i*2u+1u];
		write->passId=passIds[i*2u];write->resourceId=i+1u;
		write->access=RAL_FRAME_GRAPH_ACCESS_WRITE;
		write->state.usage=RAL_RESOURCE_USAGE_COLOR_ATTACHMENT;
		read->passId=passIds[i*2u+1u];read->resourceId=i+1u;
		read->access=RAL_FRAME_GRAPH_ACCESS_READ;
		read->state.usage=RAL_RESOURCE_USAGE_SAMPLED_TEXTURE;
		read->state.shaderStages=RAL_STAGE_FRAGMENT;
	}
	description->explicitDependencyCount=2u;
	description->explicitDependencies[0].sourcePassId=passIds[1];
	description->explicitDependencies[0].destinationPassId=passIds[2];
	description->explicitDependencies[1].sourcePassId=passIds[3];
	description->explicitDependencies[1].destinationPassId=passIds[4];
	description->transientPolicy=state->probeReceipt.plan.policy;
	return RalFrameGraph_Compile(description,&state->graphPlan)
		&& state->graphPlan.transientPlan.outcome==RAL_TRANSIENT_PLAN_EXPLICIT_ALIAS
		&& state->graphPlan.transientPlan.aliasCount==2u
		&& state->graphPlan.transientPlan.slotCount==1u ? qtrue:qfalse;
}

static qboolean CancelCommand( ralCommandBuffer_t *command ) {
	ralCommandReceipt_t current;
	if ( !command ) return qtrue;
	memset(&current,0,sizeof(current));
	if ( Ral_GetCommandBufferReceipt(command,&current)!=ralSuccess ) return qfalse;
	if ( current.state==RAL_COMMAND_RECORDING
			|| current.state==RAL_COMMAND_EXECUTABLE )
		return Ral_CancelCommandBuffer(command,&current)==ralSuccess ? qtrue:qfalse;
	return current.state==RAL_COMMAND_IDLE || current.state==RAL_COMMAND_SUBMITTED
		? qtrue:qfalse;
}

qboolean RalFrameGraphNative_Run( ralBackend_t *backend, uint64_t generation,
		ralFrameGraphNativeReceipt_t *outReceipt ) {
	nativeState_t *state=NULL;
	ralFrameGraphTransient_t *materialization=NULL;
	ralSemaphore_t *timeline=NULL;
	ralCommandBuffer_t *command=NULL;
	ralCommandReceipt_t recordingCommand;
	ralTransientBatchReceipt_t plannedBatch,submittedBatch,terminalBatch;
	ralFrameGraphNativeReceipt_t candidate;
	const ralCaps_t *caps;
	qboolean began=qfalse,nativeSubmitted=qfalse,submitted=qfalse;
	qboolean terminal=qfalse,success=qfalse;
	uint64_t timelineBase=0u,timelineFinal=0u;
	if ( !backend || !outReceipt || generation==0u || generation==UINT64_MAX )
		return qfalse;
	caps=Ral_GetCaps(backend);
	if ( !caps || caps->timelineSemaphores!=qtrue ) return qfalse;
	state=(nativeState_t*)calloc(1u,sizeof(*state));
	if ( !state ) return qfalse;
	BuildRequests(state,generation);
	if ( !ProbeRequirements(backend,state,generation)
			|| !BuildGraph(state,generation) ) goto cleanup;
	memset(&state->materialCreate,0,sizeof(state->materialCreate));
	state->materialCreate.schemaVersion=RAL_FRAME_GRAPH_TRANSIENT_SCHEMA_VERSION;
	state->materialCreate.generation=generation;
	state->materialCreate.graphPlan=&state->graphPlan;
	state->materialCreate.textureRequests=state->requests;
	state->materialCreate.textureRequestCount=RAL_FRAME_GRAPH_NATIVE_TEXTURE_COUNT;
	materialization=RalFrameGraphTransient_Create(backend,&state->materialCreate);
	if ( !materialization
			|| !RalFrameGraphTransient_GetReceipt(materialization,
				&state->materialReceipt)
			|| state->materialReceipt.savedBytes==0u
			|| state->materialReceipt.physical.allocationCount!=1u
			|| !RalFrameGraphTransient_GetExecutionBindings(materialization,
				&state->graphPlan,state->bindings,
				RAL_FRAME_GRAPH_NATIVE_TEXTURE_COUNT,&state->bindingCount) ) goto cleanup;
	timeline=Ral_CreateSemaphore(backend,RAL_SEMAPHORE_TIMELINE);
	if ( !timeline ) goto cleanup;
	timelineBase=Ral_GetTimelineValue(timeline);
	if ( timelineBase==UINT64_MAX ) goto cleanup;
	memset(&state->executionDescription,0,sizeof(state->executionDescription));
	state->executionDescription.schemaVersion=RAL_FRAME_GRAPH_EXECUTION_SCHEMA_VERSION;
	state->executionDescription.generation=generation;
	state->executionDescription.backendIdentity=backend;
	state->executionDescription.graphPlan=&state->graphPlan;
	state->executionDescription.physicalQueueForLogical[RAL_QUEUE_GRAPHICS]
		=RAL_QUEUE_GRAPHICS;
	state->executionDescription.physicalQueueForLogical[RAL_QUEUE_COMPUTE]
		=RAL_QUEUE_GRAPHICS;
	state->executionDescription.physicalQueueForLogical[RAL_QUEUE_TRANSFER]
		=RAL_QUEUE_GRAPHICS;
	state->executionDescription.timelineBaseValues[RAL_QUEUE_GRAPHICS]=timelineBase;
	memcpy(state->executionDescription.bindings,state->bindings,
		state->bindingCount*sizeof(state->bindings[0]));
	state->executionDescription.bindingCount=state->bindingCount;
	if ( !RalFrameGraphExecution_Compile(&state->executionDescription,
			&state->executionPlan)
			|| state->executionPlan.submissionBatchCount!=1u
			|| state->executionPlan.timelineFinalValues[RAL_QUEUE_GRAPHICS]
				!=timelineBase+1u ) goto cleanup;
	command=Ral_AcquireCommandBuffer(backend,RAL_QUEUE_GRAPHICS);
	memset(&recordingCommand,0,sizeof(recordingCommand));
	if ( !command || Ral_BeginCommandBufferExact(command,&recordingCommand)
			!=ralSuccess ) goto cleanup;
	if ( !RalFrameGraphTransient_Begin(materialization,0u,&plannedBatch) )
		goto cleanup;
	began=qtrue;
	memset(&state->recordDescription,0,sizeof(state->recordDescription));
	state->recordDescription.schemaVersion=RAL_FRAME_GRAPH_RECORD_SCHEMA_VERSION;
	state->recordDescription.generation=generation;
	state->recordDescription.executionPlan=&state->executionPlan;
	state->recordDescription.batchCommands[0].submissionBatchIndex=0u;
	state->recordDescription.batchCommands[0].commandBuffer=command;
	state->recordDescription.batchCommands[0].recordingReceipt=recordingCommand;
	state->recordDescription.batchCommandCount=1u;
	state->recordDescription.ops=RalFrameGraphRecord_RalOps();
	state->recordDescription.passCallbacks.preflight=PassPreflight;
	state->recordDescription.passCallbacks.record=PassRecord;
	if ( !RalFrameGraphRecord_Record(&state->recordDescription,
			&state->recordingReceipt) ) goto cleanup;
	memset(&state->submitDescription,0,sizeof(state->submitDescription));
	state->submitDescription.schemaVersion=RAL_FRAME_GRAPH_SUBMIT_SCHEMA_VERSION;
	state->submitDescription.generation=generation;
	state->submitDescription.executionPlan=&state->executionPlan;
	state->submitDescription.recordingReceipt=&state->recordingReceipt;
	state->submitDescription.batchCommands[0]
		=state->recordDescription.batchCommands[0];
	state->submitDescription.batchCommandCount=1u;
	state->submitDescription.timelineSemaphoresSupported=qtrue;
	state->submitDescription.queueTimelines[RAL_QUEUE_GRAPHICS]=timeline;
	state->submitDescription.ops=RalFrameGraphSubmit_RalOps();
	if ( RalFrameGraphSubmit_Submit(&state->submitDescription,
			&state->submissionReceipt,&state->submissionFailure)
			!=RAL_FRAME_GRAPH_SUBMIT_COMPLETE ) goto cleanup;
	nativeSubmitted=qtrue;
	if ( !RalFrameGraphTransient_Submit(materialization,&plannedBatch,
			state->submissionReceipt.submissionGeneration,&submittedBatch) )
		goto cleanup;
	submitted=qtrue;
	timelineFinal=state->submissionReceipt.timelineFinalValues[RAL_QUEUE_GRAPHICS];
	Ral_WaitTimeline(timeline,timelineFinal,RAL_TIMEOUT_INFINITE);
	if ( Ral_GetTimelineValue(timeline)!=timelineFinal
			|| Ral_WaitQueueIdle(backend,RAL_QUEUE_GRAPHICS)!=ralSuccess ) goto cleanup;
	if ( !RalFrameGraphTransient_Retire(materialization,&submittedBatch,qtrue,
			&terminalBatch) ) goto cleanup;
	terminal=qtrue;
	Ral_DestroyCommandBuffer(command);command=NULL;
	if ( !RalFrameGraphTransient_ReleaseTerminal(&materialization,&terminalBatch)
			|| materialization ) goto cleanup;
	Ral_DestroySemaphore(timeline);timeline=NULL;
	if ( Ral_WaitIdleAndDrainDeferred(backend)!=ralSuccess ) goto cleanup;
	memset(&candidate,0,sizeof(candidate));
	candidate.schemaVersion=RAL_FRAME_GRAPH_NATIVE_SCHEMA_VERSION;
	candidate.backendIdentity=(uintptr_t)backend;
	candidate.generation=generation;
	candidate.graphGeneration=state->graphPlan.description.generation;
	candidate.materializationGeneration=state->materialReceipt.generation;
	candidate.batchGeneration=terminalBatch.batchGeneration;
	candidate.recordingGeneration=state->recordingReceipt.recordingGeneration;
	candidate.submissionGeneration=state->submissionReceipt.submissionGeneration;
	candidate.nativeSubmissionGeneration
		=state->submissionReceipt.submissions[0].generation;
	candidate.textureCount=state->materialReceipt.bindingCount;
	candidate.allocationCount=state->materialReceipt.physical.allocationCount;
	candidate.passCount=state->executionPlan.passCount;
	candidate.submissionCount=state->submissionReceipt.submissionCount;
	candidate.disjointEquivalentCommittedBytes
		=state->materialReceipt.disjointEquivalentCommittedBytes;
	candidate.physicalCommittedBytes=state->materialReceipt.physicalCommittedBytes;
	candidate.savedBytes=state->materialReceipt.savedBytes;
	candidate.savedPermille=state->materialReceipt.savedPermille;
	candidate.timelineBaseValue=timelineBase;
	candidate.timelineFinalValue=timelineFinal;
	candidate.timelineCompleted=qtrue;candidate.retired=qtrue;candidate.ready=qtrue;
	if ( !RalFrameGraphNative_ReceiptExact(&candidate,&candidate) ) goto cleanup;
	*outReceipt=candidate;success=qtrue;

cleanup:
	if ( !success ) {
		if ( command ) {
			if ( nativeSubmitted )
				(void)Ral_WaitQueueIdle(backend,RAL_QUEUE_GRAPHICS);
			(void)CancelCommand(command);
			Ral_DestroyCommandBuffer(command);command=NULL;
		}
		if ( materialization ) {
			if ( nativeSubmitted )
				(void)Ral_WaitQueueIdle(backend,RAL_QUEUE_GRAPHICS);
			if ( !began ) (void)RalFrameGraphTransient_ReleaseFresh(&materialization);
			else if ( !submitted ) {
				if ( RalFrameGraphTransient_Cancel(materialization,&plannedBatch,
						&terminalBatch) ) {
					terminal=qtrue;
					(void)RalFrameGraphTransient_ReleaseTerminal(&materialization,
						&terminalBatch);
				}
			} else if ( !terminal ) {
				(void)Ral_WaitQueueIdle(backend,RAL_QUEUE_GRAPHICS);
				if ( RalFrameGraphTransient_Retire(materialization,&submittedBatch,
						qtrue,&terminalBatch) ) {
					terminal=qtrue;
					(void)RalFrameGraphTransient_ReleaseTerminal(&materialization,
						&terminalBatch);
				}
			} else {
				(void)RalFrameGraphTransient_ReleaseTerminal(&materialization,
					&terminalBatch);
			}
		}
		if ( timeline ) { Ral_DestroySemaphore(timeline);timeline=NULL; }
		(void)Ral_WaitIdleAndDrainDeferred(backend);
	}
	free(state);
	return success;
}
