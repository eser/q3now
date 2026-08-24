// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_frame_graph_execution.h"

#include <limits.h>
#include <string.h>

static qboolean QueueValid( ralQueueType_t queue ) {
	return queue == RAL_QUEUE_GRAPHICS || queue == RAL_QUEUE_COMPUTE
		|| queue == RAL_QUEUE_TRANSFER;
}

static uint32_t QueueIndex( ralQueueType_t queue ) {
	return (uint32_t)queue;
}

static qboolean BindingEmpty( const ralFrameGraphResourceBinding_t *binding ) {
	static const ralFrameGraphResourceBinding_t empty;
	return memcmp( binding, &empty, sizeof( empty ) ) == 0 ? qtrue : qfalse;
}

static int FindResource( const ralFrameGraphPlan_t *graph, uint32_t resourceId ) {
	uint32_t i;
	for ( i = 0u; i < graph->description.resourceCount; ++i )
		if ( graph->description.resources[i].id == resourceId ) return (int)i;
	return -1;
}

static int FindBinding( const ralFrameGraphExecutionDescription_t *description,
		uint32_t resourceId ) {
	uint32_t i;
	for ( i = 0u; i < description->bindingCount; ++i )
		if ( description->bindings[i].resourceId == resourceId ) return (int)i;
	return -1;
}

static int FindPass( const ralFrameGraphPlan_t *graph, uint32_t passId ) {
	uint32_t i;
	for ( i = 0u; i < graph->description.passCount; ++i )
		if ( graph->description.passes[i].id == passId ) return (int)i;
	return -1;
}

static int FindExecutionPass( const ralFrameGraphExecutionPlan_t *plan,
		uint32_t passId ) {
	uint32_t i;
	for ( i = 0u; i < plan->passCount; ++i )
		if ( plan->passes[i].passId == passId ) return (int)i;
	return -1;
}

static qboolean StateExact( const ralResourceState_t *a,
		const ralResourceState_t *b ) {
	return a->usage == b->usage && a->shaderStages == b->shaderStages;
}

static qboolean HandoffMatchesTransition(
		const ralFrameGraphQueueHandoff_t *handoff,
		const ralFrameGraphTransition_t *transition ) {
	return handoff->resourceId == transition->resourceId
		&& handoff->resourceIdentity == transition->resourceIdentity
		&& handoff->resourceGeneration == transition->resourceGeneration
		&& handoff->resourceKind == transition->resourceKind
		&& handoff->releasePassId == transition->sourcePassId
		&& handoff->acquirePassId == transition->destinationPassId
		&& StateExact( &handoff->before, &transition->before )
		&& StateExact( &handoff->after, &transition->after )
		&& handoff->sourceQueue == transition->sourceQueue
		&& handoff->destinationQueue == transition->destinationQueue;
}

static int FindHandoffForTransition( const ralFrameGraphPlan_t *graph,
		const ralFrameGraphTransition_t *transition ) {
	uint32_t i;
	for ( i = 0u; i < graph->queueHandoffCount; ++i )
		if ( HandoffMatchesTransition( &graph->queueHandoffs[i], transition ) )
			return (int)i;
	return -1;
}

static int FindTransitionForHandoff( const ralFrameGraphPlan_t *graph,
		const ralFrameGraphQueueHandoff_t *handoff ) {
	uint32_t i;
	for ( i = 0u; i < graph->transitionCount; ++i )
		if ( HandoffMatchesTransition( handoff, &graph->transitions[i] ) )
			return (int)i;
	return -1;
}

static qboolean PhysicalQueuesDiffer(
		const ralFrameGraphExecutionDescription_t *description,
		ralQueueType_t source, ralQueueType_t destination ) {
	return description->physicalQueueForLogical[QueueIndex(source)]
		!= description->physicalQueueForLogical[QueueIndex(destination)];
}

static qboolean BindingValid( const ralFrameGraphResource_t *resource,
		const ralFrameGraphResourceBinding_t *binding ) {
	if ( !resource || !binding || binding->resourceId != resource->id
			|| binding->resourceIdentity != resource->identity
			|| binding->resourceGeneration != resource->generation
			|| binding->resourceKind != resource->kind ) return qfalse;
	if ( resource->kind == RAL_FRAME_GRAPH_RESOURCE_BUFFER )
		return binding->buffer && !binding->texture && binding->bufferSize != 0u
			&& binding->textureAspects == 0u && binding->textureMipLevels == 0u
			&& binding->textureArrayLayers == 0u
			&& (resource->size == 0u || binding->bufferSize >= resource->size);
	if ( resource->kind == RAL_FRAME_GRAPH_RESOURCE_TEXTURE )
		return !binding->buffer && binding->texture && binding->bufferSize == 0u
			&& binding->textureAspects != 0u && binding->textureMipLevels != 0u
			&& binding->textureArrayLayers != 0u;
	return qfalse;
}

static qboolean DescriptionValid(
		const ralFrameGraphExecutionDescription_t *description ) {
	const ralFrameGraphPlan_t *graph;
	uint32_t i,j;
	if ( !description
			|| description->schemaVersion != RAL_FRAME_GRAPH_EXECUTION_SCHEMA_VERSION
			|| description->generation == 0u || description->generation == UINT64_MAX
			|| !description->backendIdentity || !description->graphPlan
			|| !RalFrameGraph_PlanExact(description->graphPlan,description->graphPlan) )
		return qfalse;
	graph = description->graphPlan;
	if ( description->bindingCount != graph->description.resourceCount
			|| description->bindingCount > RAL_FRAME_GRAPH_MAX_RESOURCES ) return qfalse;
	for ( i = 0u; i < RAL_FRAME_GRAPH_PHYSICAL_QUEUE_COUNT; ++i ) {
		if ( !QueueValid(description->physicalQueueForLogical[i])
				|| description->timelineBaseValues[i]
					> UINT64_MAX - graph->description.passCount - 1u ) return qfalse;
	}
	if ( graph->description.backendType == RAL_BACKEND_WEBGPU
			&& (description->physicalQueueForLogical[RAL_QUEUE_GRAPHICS]
				!= description->physicalQueueForLogical[RAL_QUEUE_COMPUTE]
			|| description->physicalQueueForLogical[RAL_QUEUE_GRAPHICS]
				!= description->physicalQueueForLogical[RAL_QUEUE_TRANSFER]) ) return qfalse;
	for ( i = 0u; i < graph->description.resourceCount; ++i ) {
		const int bindingIndex = FindBinding( description,
			graph->description.resources[i].id );
		if ( bindingIndex < 0 || !BindingValid(&graph->description.resources[i],
			&description->bindings[bindingIndex]) ) return qfalse;
	}
	for ( i = 0u; i < description->bindingCount; ++i ) {
		if ( FindResource(graph,description->bindings[i].resourceId) < 0 ) return qfalse;
		for ( j = 0u; j < i; ++j ) {
			if ( description->bindings[i].resourceId
					== description->bindings[j].resourceId ) return qfalse;
			if ( description->bindings[i].resourceKind
					== RAL_FRAME_GRAPH_RESOURCE_BUFFER
					&& description->bindings[i].buffer
					== description->bindings[j].buffer ) return qfalse;
			if ( description->bindings[i].resourceKind
					== RAL_FRAME_GRAPH_RESOURCE_TEXTURE
					&& description->bindings[i].texture
					== description->bindings[j].texture ) return qfalse;
		}
	}
	for ( i = description->bindingCount; i < RAL_FRAME_GRAPH_MAX_RESOURCES; ++i )
		if ( !BindingEmpty(&description->bindings[i]) ) return qfalse;
	return qtrue;
}

static qboolean MakeBufferTransition(
		const ralFrameGraphResourceBinding_t *binding,
		const ralFrameGraphTransition_t *fact,
		ralBufferTransition_t *out ) {
	memset( out, 0, sizeof(*out) );
	out->buffer = binding->buffer;
	out->size = binding->bufferSize;
	out->before = fact->before;
	out->after = fact->after;
	out->sourceQueue = fact->sourceQueue;
	out->destinationQueue = fact->destinationQueue;
	return Ral_BufferTransitionValid(out,binding->bufferSize);
}

static qboolean MakeTextureTransition(
		const ralFrameGraphResourceBinding_t *binding,
		const ralFrameGraphTransition_t *fact,
		ralTextureTransition_t *out ) {
	memset( out, 0, sizeof(*out) );
	out->texture = binding->texture;
	out->aspects = binding->textureAspects;
	out->mipLevelCount = binding->textureMipLevels;
	out->arrayLayerCount = binding->textureArrayLayers;
	out->before = fact->before;
	out->after = fact->after;
	out->sourceQueue = fact->sourceQueue;
	out->destinationQueue = fact->destinationQueue;
	return Ral_TextureTransitionValid(out,binding->textureMipLevels,
		binding->textureArrayLayers,binding->textureAspects);
}

static qboolean AppendPreTransition( ralFrameGraphExecutionPlan_t *candidate,
		const ralFrameGraphTransition_t *fact ) {
	const int bindingIndex = FindBinding(&candidate->description,fact->resourceId);
	if ( bindingIndex < 0 ) return qfalse;
	if ( fact->resourceKind == RAL_FRAME_GRAPH_RESOURCE_BUFFER ) {
		if ( candidate->preBufferTransitionCount >= RAL_FRAME_GRAPH_MAX_TRANSITIONS )
			return qfalse;
		return MakeBufferTransition(&candidate->description.bindings[bindingIndex],fact,
			&candidate->preBufferTransitions[candidate->preBufferTransitionCount++]);
	}
	if ( fact->resourceKind == RAL_FRAME_GRAPH_RESOURCE_TEXTURE ) {
		if ( candidate->preTextureTransitionCount >= RAL_FRAME_GRAPH_MAX_TRANSITIONS )
			return qfalse;
		return MakeTextureTransition(&candidate->description.bindings[bindingIndex],fact,
			&candidate->preTextureTransitions[candidate->preTextureTransitionCount++]);
	}
	return qfalse;
}

static qboolean AppendOwnershipTransition(
		ralFrameGraphExecutionPlan_t *candidate,
		const ralFrameGraphTransition_t *fact, qboolean acquire ) {
	const int bindingIndex = FindBinding(&candidate->description,fact->resourceId);
	if ( bindingIndex < 0 ) return qfalse;
	if ( fact->resourceKind == RAL_FRAME_GRAPH_RESOURCE_BUFFER ) {
		uint32_t *count = acquire ? &candidate->acquireBufferTransitionCount
			: &candidate->releaseBufferTransitionCount;
		ralBufferTransition_t *array = acquire ? candidate->acquireBufferTransitions
			: candidate->releaseBufferTransitions;
		if ( *count >= RAL_FRAME_GRAPH_MAX_QUEUE_HANDOFFS ) return qfalse;
		return MakeBufferTransition(&candidate->description.bindings[bindingIndex],fact,
			&array[(*count)++]);
	}
	if ( fact->resourceKind == RAL_FRAME_GRAPH_RESOURCE_TEXTURE ) {
		uint32_t *count = acquire ? &candidate->acquireTextureTransitionCount
			: &candidate->releaseTextureTransitionCount;
		ralTextureTransition_t *array = acquire ? candidate->acquireTextureTransitions
			: candidate->releaseTextureTransitions;
		if ( *count >= RAL_FRAME_GRAPH_MAX_QUEUE_HANDOFFS ) return qfalse;
		return MakeTextureTransition(&candidate->description.bindings[bindingIndex],fact,
			&array[(*count)++]);
	}
	return qfalse;
}

static qboolean BuildExecutionPasses( ralFrameGraphExecutionPlan_t *candidate ) {
	const ralFrameGraphPlan_t *graph = candidate->description.graphPlan;
	uint32_t position;
	for ( position = 0u; position < graph->topologicalPassCount; ++position ) {
		ralFrameGraphExecutionPass_t *pass = &candidate->passes[position];
		const uint32_t passId = graph->topologicalPassIds[position];
		const int graphPassIndex = FindPass(graph,passId);
		uint32_t i;
		if ( graphPassIndex < 0 ) return qfalse;
		pass->passId = passId;
		pass->topologicalIndex = position;
		pass->logicalQueue = graph->description.passes[graphPassIndex].queue;
		pass->physicalQueue = candidate->description.physicalQueueForLogical[
			QueueIndex(pass->logicalQueue)];
		pass->firstPreBufferTransition = candidate->preBufferTransitionCount;
		pass->firstPreTextureTransition = candidate->preTextureTransitionCount;
		for ( i = 0u; i < graph->transitionCount; ++i ) {
			const ralFrameGraphTransition_t *fact = &graph->transitions[i];
			if ( fact->destinationPassId != passId ) continue;
			if ( PhysicalQueuesDiffer(&candidate->description,fact->sourceQueue,
					fact->destinationQueue) ) {
				if ( fact->sourcePassId == 0u
						|| FindHandoffForTransition(graph,fact) < 0 ) return qfalse;
				continue;
			}
			if ( !AppendPreTransition(candidate,fact) ) return qfalse;
		}
		pass->preBufferTransitionCount = candidate->preBufferTransitionCount
			- pass->firstPreBufferTransition;
		pass->preTextureTransitionCount = candidate->preTextureTransitionCount
			- pass->firstPreTextureTransition;
		pass->firstAcquireBufferTransition = candidate->acquireBufferTransitionCount;
		pass->firstAcquireTextureTransition = candidate->acquireTextureTransitionCount;
		for ( i = 0u; i < graph->queueHandoffCount; ++i ) {
			const ralFrameGraphQueueHandoff_t *handoff = &graph->queueHandoffs[i];
			const int transitionIndex = FindTransitionForHandoff(graph,handoff);
			if ( transitionIndex < 0 ) return qfalse;
			if ( handoff->acquirePassId == passId
					&& PhysicalQueuesDiffer(&candidate->description,
						handoff->sourceQueue,handoff->destinationQueue)
					&& !AppendOwnershipTransition(candidate,
						&graph->transitions[transitionIndex],qtrue) ) return qfalse;
		}
		pass->acquireBufferTransitionCount = candidate->acquireBufferTransitionCount
			- pass->firstAcquireBufferTransition;
		pass->acquireTextureTransitionCount = candidate->acquireTextureTransitionCount
			- pass->firstAcquireTextureTransition;
		pass->firstReleaseBufferTransition = candidate->releaseBufferTransitionCount;
		pass->firstReleaseTextureTransition = candidate->releaseTextureTransitionCount;
		for ( i = 0u; i < graph->queueHandoffCount; ++i ) {
			const ralFrameGraphQueueHandoff_t *handoff = &graph->queueHandoffs[i];
			const int transitionIndex = FindTransitionForHandoff(graph,handoff);
			if ( transitionIndex < 0 ) return qfalse;
			if ( handoff->releasePassId == passId
					&& PhysicalQueuesDiffer(&candidate->description,
						handoff->sourceQueue,handoff->destinationQueue)
					&& !AppendOwnershipTransition(candidate,
						&graph->transitions[transitionIndex],qfalse) ) return qfalse;
		}
		pass->releaseBufferTransitionCount = candidate->releaseBufferTransitionCount
			- pass->firstReleaseBufferTransition;
		pass->releaseTextureTransitionCount = candidate->releaseTextureTransitionCount
			- pass->firstReleaseTextureTransition;
	}
	candidate->passCount = graph->topologicalPassCount;
	return qtrue;
}

static qboolean BuildSubmissionBatches( ralFrameGraphExecutionPlan_t *candidate ) {
	const ralFrameGraphPlan_t *graph = candidate->description.graphPlan;
	uint64_t nextValues[RAL_FRAME_GRAPH_PHYSICAL_QUEUE_COUNT];
	uint32_t i;
	memcpy(nextValues,candidate->description.timelineBaseValues,sizeof(nextValues));
	for ( i = 0u; i < candidate->passCount; ++i ) {
		ralFrameGraphExecutionPass_t *pass = &candidate->passes[i];
		ralFrameGraphSubmissionBatch_t *batch;
		if ( candidate->submissionBatchCount == 0u
				|| candidate->submissionBatches[candidate->submissionBatchCount-1u]
					.physicalQueue != pass->physicalQueue ) {
			if ( candidate->submissionBatchCount >= RAL_FRAME_GRAPH_MAX_PASSES )
				return qfalse;
			batch = &candidate->submissionBatches[candidate->submissionBatchCount++];
			batch->firstExecutionPass = i;
			batch->physicalQueue = pass->physicalQueue;
			if ( nextValues[QueueIndex(pass->physicalQueue)] >= UINT64_MAX - 1u )
				return qfalse;
			batch->signalValue = ++nextValues[QueueIndex(pass->physicalQueue)];
		} else batch = &candidate->submissionBatches[candidate->submissionBatchCount-1u];
		batch->executionPassCount++;
		pass->submissionBatchIndex = candidate->submissionBatchCount - 1u;
	}
	for ( i = 0u; i < graph->edgeCount; ++i ) {
		const int sourcePass = FindExecutionPass(candidate,graph->edges[i].sourcePassId);
		const int destinationPass = FindExecutionPass(candidate,
			graph->edges[i].destinationPassId);
		ralFrameGraphSubmissionBatch_t *sourceBatch,*destinationBatch;
		uint32_t q,j;
		if ( sourcePass < 0 || destinationPass < 0 ) return qfalse;
		sourceBatch = &candidate->submissionBatches[
			candidate->passes[sourcePass].submissionBatchIndex];
		destinationBatch = &candidate->submissionBatches[
			candidate->passes[destinationPass].submissionBatchIndex];
		if ( sourceBatch == destinationBatch
				|| sourceBatch->physicalQueue == destinationBatch->physicalQueue ) continue;
		q = QueueIndex(sourceBatch->physicalQueue);
		for ( j = 0u; j < destinationBatch->waitCount; ++j )
			if ( QueueIndex(destinationBatch->waits[j].sourcePhysicalQueue) == q ) {
				if ( destinationBatch->waits[j].value < sourceBatch->signalValue )
					destinationBatch->waits[j].value = sourceBatch->signalValue;
				break;
			}
		if ( j == destinationBatch->waitCount ) {
			if ( destinationBatch->waitCount >= RAL_FRAME_GRAPH_MAX_SUBMISSION_WAITS )
				return qfalse;
			destinationBatch->waits[j].sourcePhysicalQueue = sourceBatch->physicalQueue;
			destinationBatch->waits[j].value = sourceBatch->signalValue;
			destinationBatch->waitCount++;
		}
	}
	// Canonical queue-order wait arrays make receipts stable across edge order.
	for ( i = 0u; i < candidate->submissionBatchCount; ++i ) {
		ralFrameGraphSubmissionBatch_t *batch = &candidate->submissionBatches[i];
		uint32_t j;
		for ( j = 1u; j < batch->waitCount; ++j ) {
			ralFrameGraphSubmissionWait_t item = batch->waits[j];
			uint32_t k = j;
			while ( k > 0u && QueueIndex(batch->waits[k-1u].sourcePhysicalQueue)
					> QueueIndex(item.sourcePhysicalQueue) ) {
				batch->waits[k] = batch->waits[k-1u]; --k;
			}
			batch->waits[k] = item;
		}
	}
	memcpy(candidate->timelineFinalValues,nextValues,sizeof(nextValues));
	return qtrue;
}

static qboolean CompileInternal(
		const ralFrameGraphExecutionDescription_t *description,
		ralFrameGraphExecutionPlan_t *candidate ) {
	if ( !candidate || !DescriptionValid(description) ) return qfalse;
	memset(candidate,0,sizeof(*candidate));
	candidate->schemaVersion = RAL_FRAME_GRAPH_EXECUTION_SCHEMA_VERSION;
	candidate->description = *description;
	if ( !BuildExecutionPasses(candidate) || !BuildSubmissionBatches(candidate) )
		return qfalse;
	candidate->ready = qtrue;
	return qtrue;
}

qboolean RalFrameGraphExecution_Compile(
		const ralFrameGraphExecutionDescription_t *description,
		ralFrameGraphExecutionPlan_t *outPlan ) {
	ralFrameGraphExecutionPlan_t candidate;
	if ( !outPlan || !CompileInternal(description,&candidate) ) return qfalse;
	*outPlan = candidate;
	return qtrue;
}

qboolean RalFrameGraphExecution_PlanExact(
		const ralFrameGraphExecutionPlan_t *a,
		const ralFrameGraphExecutionPlan_t *b ) {
	ralFrameGraphExecutionPlan_t rebuiltA,rebuiltB;
	if ( !a || !b || a->ready != qtrue || b->ready != qtrue
			|| !CompileInternal(&a->description,&rebuiltA)
			|| !CompileInternal(&b->description,&rebuiltB) ) return qfalse;
	return memcmp(a,&rebuiltA,sizeof(*a)) == 0
		&& memcmp(b,&rebuiltB,sizeof(*b)) == 0
		&& memcmp(a,b,sizeof(*a)) == 0 ? qtrue : qfalse;
}
