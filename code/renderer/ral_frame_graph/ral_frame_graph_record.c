// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_frame_graph_record.h"

#include <limits.h>
#include <string.h>

typedef struct {
	ralFrameGraphResourceKind_t kind;
	ralBufferTransition_t buffer;
	ralTextureTransition_t texture;
	ralQueueTransferReceipt_t receipt;
	qboolean pending;
} pendingTransfer_t;

static qboolean BatchCommandEmpty( const ralFrameGraphBatchCommand_t *command ) {
	static const ralFrameGraphBatchCommand_t empty;
	return memcmp(command,&empty,sizeof(empty)) == 0 ? qtrue : qfalse;
}

static qboolean StateExact( const ralResourceState_t *a,
		const ralResourceState_t *b ) {
	return a->usage == b->usage && a->shaderStages == b->shaderStages;
}

static qboolean BufferTransitionExact( const ralBufferTransition_t *a,
		const ralBufferTransition_t *b ) {
	return a->buffer == b->buffer && a->offset == b->offset && a->size == b->size
		&& StateExact(&a->before,&b->before) && StateExact(&a->after,&b->after)
		&& a->sourceQueue == b->sourceQueue
		&& a->destinationQueue == b->destinationQueue;
}

static qboolean TextureTransitionExact( const ralTextureTransition_t *a,
		const ralTextureTransition_t *b ) {
	return a->texture == b->texture && a->aspects == b->aspects
		&& a->baseMipLevel == b->baseMipLevel
		&& a->mipLevelCount == b->mipLevelCount
		&& a->baseArrayLayer == b->baseArrayLayer
		&& a->arrayLayerCount == b->arrayLayerCount
		&& StateExact(&a->before,&b->before) && StateExact(&a->after,&b->after)
		&& a->sourceQueue == b->sourceQueue
		&& a->destinationQueue == b->destinationQueue;
}

static qboolean OpsValid( const ralFrameGraphRecordOps_t *ops ) {
	return ops && ops->getCommandReceipt && ops->transitionResources
		&& ops->releaseBuffer && ops->acquireBuffer && ops->cancelBuffer
		&& ops->releaseTexture && ops->acquireTexture && ops->cancelTexture;
}

static qboolean DescriptionValid( const ralFrameGraphRecordDescription_t *description ) {
	const ralFrameGraphExecutionPlan_t *plan;
	uint32_t i,j;
	if ( !description
			|| description->schemaVersion != RAL_FRAME_GRAPH_RECORD_SCHEMA_VERSION
			|| description->generation == 0u || description->generation == UINT64_MAX
			|| !description->executionPlan
			|| !RalFrameGraphExecution_PlanExact(description->executionPlan,
				description->executionPlan)
			|| !OpsValid(description->ops)
			|| !description->passCallbacks.preflight
			|| !description->passCallbacks.record ) return qfalse;
	plan = description->executionPlan;
	if ( description->batchCommandCount != plan->submissionBatchCount
			|| description->batchCommandCount == 0u
			|| description->batchCommandCount > RAL_FRAME_GRAPH_MAX_PASSES ) return qfalse;
	for ( i = 0u; i < description->batchCommandCount; ++i ) {
		const ralFrameGraphBatchCommand_t *command = &description->batchCommands[i];
		if ( command->submissionBatchIndex != i || !command->commandBuffer
				|| !Ral_CommandReceiptValid(&command->recordingReceipt)
				|| command->recordingReceipt.backendIdentity
					!= plan->description.backendIdentity
				|| command->recordingReceipt.commandIdentity != command->commandBuffer
				|| command->recordingReceipt.queue
					!= plan->submissionBatches[i].physicalQueue
				|| command->recordingReceipt.state != RAL_COMMAND_RECORDING ) return qfalse;
		for ( j = 0u; j < i; ++j )
			if ( command->commandBuffer == description->batchCommands[j].commandBuffer
					|| command->recordingReceipt.commandIdentity
					== description->batchCommands[j].recordingReceipt.commandIdentity )
				return qfalse;
	}
	for ( i = description->batchCommandCount; i < RAL_FRAME_GRAPH_MAX_PASSES; ++i )
		if ( !BatchCommandEmpty(&description->batchCommands[i]) ) return qfalse;
	return qtrue;
}

static qboolean CommandAuthorityCurrent( const ralFrameGraphRecordDescription_t *description,
		const ralFrameGraphRecordOps_t *ops, uint32_t batchIndex ) {
	ralCommandReceipt_t current;
	const ralFrameGraphBatchCommand_t *command = &description->batchCommands[batchIndex];
	memset(&current,0,sizeof(current));
	return ops->getCommandReceipt(description->opsContext,command->commandBuffer,
		&current) && Ral_CommandReceiptExact(&current,&command->recordingReceipt);
}

static int FindPendingBuffer( const pendingTransfer_t *pending, uint32_t count,
		const ralBufferTransition_t *transition ) {
	uint32_t i;
	for ( i = 0u; i < count; ++i )
		if ( pending[i].pending && pending[i].kind == RAL_FRAME_GRAPH_RESOURCE_BUFFER
				&& BufferTransitionExact(&pending[i].buffer,transition) ) return (int)i;
	return -1;
}

static int FindPendingTexture( const pendingTransfer_t *pending, uint32_t count,
		const ralTextureTransition_t *transition ) {
	uint32_t i;
	for ( i = 0u; i < count; ++i )
		if ( pending[i].pending && pending[i].kind == RAL_FRAME_GRAPH_RESOURCE_TEXTURE
				&& TextureTransitionExact(&pending[i].texture,transition) ) return (int)i;
	return -1;
}

static qboolean ReceiptMatchesBuffer( const ralQueueTransferReceipt_t *receipt,
		const ralBufferTransition_t *transition ) {
	return Ral_QueueTransferReceiptExact(receipt,receipt)
		&& receipt->resourceIdentity == transition->buffer
		&& receipt->resourceType == RAL_QUEUE_TRANSFER_RESOURCE_BUFFER
		&& StateExact(&receipt->before,&transition->before)
		&& StateExact(&receipt->after,&transition->after)
		&& receipt->sourceQueue == transition->sourceQueue
		&& receipt->destinationQueue == transition->destinationQueue;
}

static qboolean ReceiptMatchesTexture( const ralQueueTransferReceipt_t *receipt,
		const ralTextureTransition_t *transition ) {
	return Ral_QueueTransferReceiptExact(receipt,receipt)
		&& receipt->resourceIdentity == transition->texture
		&& receipt->resourceType == RAL_QUEUE_TRANSFER_RESOURCE_TEXTURE
		&& StateExact(&receipt->before,&transition->before)
		&& StateExact(&receipt->after,&transition->after)
		&& receipt->sourceQueue == transition->sourceQueue
		&& receipt->destinationQueue == transition->destinationQueue;
}

static void CancelPending( const ralFrameGraphRecordDescription_t *description,
		const ralFrameGraphRecordOps_t *ops, pendingTransfer_t *pending,
		uint32_t pendingCount ) {
	uint32_t i;
	for ( i = 0u; i < pendingCount; ++i ) {
		if ( !pending[i].pending ) continue;
		if ( pending[i].kind == RAL_FRAME_GRAPH_RESOURCE_BUFFER )
			(void)ops->cancelBuffer(description->opsContext,pending[i].buffer.buffer,
				&pending[i].receipt);
		else
			(void)ops->cancelTexture(description->opsContext,pending[i].texture.texture,
				&pending[i].receipt);
		pending[i].pending=qfalse;
	}
}

static qboolean ReceiptValid( const ralFrameGraphRecordingReceipt_t *receipt ) {
	static const ralCommandReceipt_t empty;
	uint32_t i,j;
	if ( !receipt || receipt->schemaVersion != RAL_FRAME_GRAPH_RECORD_SCHEMA_VERSION
			|| !receipt->backendIdentity || !receipt->executionPlanIdentity
			|| !receipt->opsIdentity || !receipt->passPreflightIdentity
			|| !receipt->passRecordIdentity
			|| receipt->executionGeneration == 0u
			|| receipt->executionGeneration == UINT64_MAX
			|| receipt->graphGeneration == 0u || receipt->graphGeneration == UINT64_MAX
			|| receipt->recordingGeneration == 0u
			|| receipt->recordingGeneration == UINT64_MAX
			|| receipt->passCount == 0u
			|| receipt->passCount > RAL_FRAME_GRAPH_MAX_PASSES
			|| receipt->submissionBatchCount == 0u
			|| receipt->submissionBatchCount > receipt->passCount
			|| receipt->recordingCommandCount != receipt->submissionBatchCount
			|| receipt->ownershipReleaseCount != receipt->ownershipAcquireCount
			|| receipt->ready != qtrue ) return qfalse;
	for ( i = 0u; i < receipt->recordingCommandCount; ++i ) {
		if ( !Ral_CommandReceiptValid(&receipt->recordingCommands[i])
				|| receipt->recordingCommands[i].backendIdentity != receipt->backendIdentity
				|| receipt->recordingCommands[i].state != RAL_COMMAND_RECORDING ) return qfalse;
		for ( j = 0u; j < i; ++j )
			if ( receipt->recordingCommands[i].commandIdentity
					== receipt->recordingCommands[j].commandIdentity ) return qfalse;
	}
	for ( i = receipt->recordingCommandCount; i < RAL_FRAME_GRAPH_MAX_PASSES; ++i )
		if ( memcmp(&receipt->recordingCommands[i],&empty,sizeof(empty)) != 0 )
			return qfalse;
	return qtrue;
}

qboolean RalFrameGraphRecord_ReceiptExact(
		const ralFrameGraphRecordingReceipt_t *a,
		const ralFrameGraphRecordingReceipt_t *b ) {
	uint32_t i;
	if ( !ReceiptValid(a) || !ReceiptValid(b)
			|| a->schemaVersion != b->schemaVersion
			|| a->backendIdentity != b->backendIdentity
			|| a->executionPlanIdentity != b->executionPlanIdentity
			|| a->opsIdentity != b->opsIdentity
			|| a->opsContextIdentity != b->opsContextIdentity
			|| a->passPreflightIdentity != b->passPreflightIdentity
			|| a->passRecordIdentity != b->passRecordIdentity
			|| a->passContextIdentity != b->passContextIdentity
			|| a->executionGeneration != b->executionGeneration
			|| a->graphGeneration != b->graphGeneration
			|| a->recordingGeneration != b->recordingGeneration
			|| a->passCount != b->passCount
			|| a->submissionBatchCount != b->submissionBatchCount
			|| a->transitionBatchCount != b->transitionBatchCount
			|| a->ownershipReleaseCount != b->ownershipReleaseCount
			|| a->ownershipAcquireCount != b->ownershipAcquireCount
			|| a->recordingCommandCount != b->recordingCommandCount
			|| a->ready != b->ready ) return qfalse;
	for ( i = 0u; i < a->recordingCommandCount; ++i )
		if ( !Ral_CommandReceiptExact(&a->recordingCommands[i],
			&b->recordingCommands[i]) ) return qfalse;
	return qtrue;
}

qboolean RalFrameGraphRecord_Record(
		const ralFrameGraphRecordDescription_t *description,
		ralFrameGraphRecordingReceipt_t *outReceipt ) {
	ralFrameGraphRecordDescription_t frozen;
	ralFrameGraphRecordOps_t ops;
	ralFrameGraphRecordingReceipt_t candidate;
	pendingTransfer_t pending[RAL_FRAME_GRAPH_MAX_QUEUE_HANDOFFS];
	uint32_t pendingCount=0u,releaseCount=0u,acquireCount=0u,transitionBatchCount=0u;
	uint32_t i;
	if ( !outReceipt || !DescriptionValid(description) ) return qfalse;
	frozen=*description;
	ops=*description->ops;
	memset(pending,0,sizeof(pending));
	// Complete preflight precedes the first resource command or pass callback.
	for ( i = 0u; i < frozen.batchCommandCount; ++i )
		if ( !CommandAuthorityCurrent(&frozen,&ops,i) ) return qfalse;
	for ( i = 0u; i < frozen.executionPlan->passCount; ++i ) {
		const ralFrameGraphExecutionPass_t *pass=&frozen.executionPlan->passes[i];
		ralCommandBuffer_t *command=frozen.batchCommands[pass->submissionBatchIndex]
			.commandBuffer;
		if ( !frozen.passCallbacks.preflight(frozen.passCallbacks.context,pass,
			frozen.executionPlan,command) ) return qfalse;
	}
	for ( i = 0u; i < frozen.executionPlan->passCount; ++i ) {
		const ralFrameGraphExecutionPass_t *pass=&frozen.executionPlan->passes[i];
		ralCommandBuffer_t *command=frozen.batchCommands[pass->submissionBatchIndex]
			.commandBuffer;
		uint32_t j;
		for ( j = 0u; j < pass->acquireBufferTransitionCount; ++j ) {
			const ralBufferTransition_t *transition=&frozen.executionPlan
				->acquireBufferTransitions[pass->firstAcquireBufferTransition+j];
			const int pendingIndex=FindPendingBuffer(pending,pendingCount,transition);
			if ( pendingIndex < 0 || ops.acquireBuffer(frozen.opsContext,command,
					transition,&pending[pendingIndex].receipt) != ralSuccess ) goto fail;
			pending[pendingIndex].pending=qfalse;acquireCount++;
		}
		for ( j = 0u; j < pass->acquireTextureTransitionCount; ++j ) {
			const ralTextureTransition_t *transition=&frozen.executionPlan
				->acquireTextureTransitions[pass->firstAcquireTextureTransition+j];
			const int pendingIndex=FindPendingTexture(pending,pendingCount,transition);
			if ( pendingIndex < 0 || ops.acquireTexture(frozen.opsContext,command,
					transition,&pending[pendingIndex].receipt) != ralSuccess ) goto fail;
			pending[pendingIndex].pending=qfalse;acquireCount++;
		}
		if ( pass->preBufferTransitionCount != 0u
				|| pass->preTextureTransitionCount != 0u ) {
			ralResourceTransitionBatch_t batch;
			memset(&batch,0,sizeof(batch));
			batch.bufferTransitions=&frozen.executionPlan->preBufferTransitions[
				pass->firstPreBufferTransition];
			batch.bufferTransitionCount=pass->preBufferTransitionCount;
			batch.textureTransitions=&frozen.executionPlan->preTextureTransitions[
				pass->firstPreTextureTransition];
			batch.textureTransitionCount=pass->preTextureTransitionCount;
			if ( ops.transitionResources(frozen.opsContext,command,&batch) != ralSuccess )
				goto fail;
			transitionBatchCount++;
		}
		frozen.passCallbacks.record(frozen.passCallbacks.context,pass,
			frozen.executionPlan,command);
		for ( j = 0u; j < pass->releaseBufferTransitionCount; ++j ) {
			pendingTransfer_t *item;
			const ralBufferTransition_t *transition=&frozen.executionPlan
				->releaseBufferTransitions[pass->firstReleaseBufferTransition+j];
			if ( pendingCount >= RAL_FRAME_GRAPH_MAX_QUEUE_HANDOFFS ) goto fail;
			item=&pending[pendingCount];item->kind=RAL_FRAME_GRAPH_RESOURCE_BUFFER;
			item->buffer=*transition;
			if ( ops.releaseBuffer(frozen.opsContext,command,transition,&item->receipt)
					!= ralSuccess ) goto fail;
			item->pending=qtrue;pendingCount++;
			if ( !ReceiptMatchesBuffer(&item->receipt,transition) ) goto fail;
			releaseCount++;
		}
		for ( j = 0u; j < pass->releaseTextureTransitionCount; ++j ) {
			pendingTransfer_t *item;
			const ralTextureTransition_t *transition=&frozen.executionPlan
				->releaseTextureTransitions[pass->firstReleaseTextureTransition+j];
			if ( pendingCount >= RAL_FRAME_GRAPH_MAX_QUEUE_HANDOFFS ) goto fail;
			item=&pending[pendingCount];item->kind=RAL_FRAME_GRAPH_RESOURCE_TEXTURE;
			item->texture=*transition;
			if ( ops.releaseTexture(frozen.opsContext,command,transition,&item->receipt)
					!= ralSuccess ) goto fail;
			item->pending=qtrue;pendingCount++;
			if ( !ReceiptMatchesTexture(&item->receipt,transition) ) goto fail;
			releaseCount++;
		}
	}
	for ( i = 0u; i < pendingCount; ++i ) if ( pending[i].pending ) goto fail;
	if ( releaseCount != acquireCount
			|| memcmp(description,&frozen,sizeof(frozen)) != 0
			|| description->ops != frozen.ops
			|| memcmp(frozen.ops,&ops,sizeof(ops)) != 0
			|| !RalFrameGraphExecution_PlanExact(frozen.executionPlan,
				frozen.executionPlan) ) goto fail;
	for ( i = 0u; i < frozen.batchCommandCount; ++i )
		if ( !CommandAuthorityCurrent(&frozen,&ops,i) ) goto fail;
	memset(&candidate,0,sizeof(candidate));
	candidate.schemaVersion=RAL_FRAME_GRAPH_RECORD_SCHEMA_VERSION;
	candidate.backendIdentity=frozen.executionPlan->description.backendIdentity;
	candidate.executionPlanIdentity=frozen.executionPlan;
	candidate.opsIdentity=frozen.ops;
	candidate.opsContextIdentity=frozen.opsContext;
	candidate.passPreflightIdentity=frozen.passCallbacks.preflight;
	candidate.passRecordIdentity=frozen.passCallbacks.record;
	candidate.passContextIdentity=frozen.passCallbacks.context;
	candidate.executionGeneration=frozen.executionPlan->description.generation;
	candidate.graphGeneration=frozen.executionPlan->description.graphPlan
		->description.generation;
	candidate.recordingGeneration=frozen.generation;
	candidate.passCount=frozen.executionPlan->passCount;
	candidate.submissionBatchCount=frozen.executionPlan->submissionBatchCount;
	candidate.transitionBatchCount=transitionBatchCount;
	candidate.ownershipReleaseCount=releaseCount;
	candidate.ownershipAcquireCount=acquireCount;
	candidate.recordingCommandCount=frozen.batchCommandCount;
	for ( i = 0u; i < frozen.batchCommandCount; ++i )
		candidate.recordingCommands[i]=frozen.batchCommands[i].recordingReceipt;
	candidate.ready=qtrue;
	if ( !ReceiptValid(&candidate) ) goto fail;
	*outReceipt=candidate;
	return qtrue;
fail:
	CancelPending(&frozen,&ops,pending,pendingCount);
	return qfalse;
}
