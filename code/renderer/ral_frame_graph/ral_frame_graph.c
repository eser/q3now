// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_frame_graph.h"

#include <limits.h>
#include <string.h>

typedef struct {
	int16_t useByPassResource[RAL_FRAME_GRAPH_MAX_PASSES][RAL_FRAME_GRAPH_MAX_RESOURCES];
	qboolean adjacency[RAL_FRAME_GRAPH_MAX_PASSES][RAL_FRAME_GRAPH_MAX_PASSES];
	uint32_t passUseCounts[RAL_FRAME_GRAPH_MAX_PASSES];
	uint32_t resourceUseCounts[RAL_FRAME_GRAPH_MAX_RESOURCES];
} ralFrameGraphCompileScratch_t;

static qboolean BoolValid( qboolean value ) {
	return value == qfalse || value == qtrue;
}

static qboolean QueueValid( ralQueueType_t queue ) {
	return queue == RAL_QUEUE_GRAPHICS || queue == RAL_QUEUE_COMPUTE
		|| queue == RAL_QUEUE_TRANSFER;
}

static qboolean BackendValid( ralBackendType_t backend ) {
	return backend >= RAL_BACKEND_VULKAN && backend <= RAL_BACKEND_WEBGL2;
}

static qboolean PowerOfTwo( uint64_t value ) {
	return value != 0u && (value & (value - 1u)) == 0u;
}

static qboolean StateExact( const ralResourceState_t *a,
		const ralResourceState_t *b ) {
	return a->usage == b->usage && a->shaderStages == b->shaderStages;
}

static qboolean ResourceStateValid( ralFrameGraphResourceKind_t kind,
		const ralResourceState_t *state ) {
	return kind == RAL_FRAME_GRAPH_RESOURCE_BUFFER
		? Ral_ResourceStateValidForBuffer( state )
		: kind == RAL_FRAME_GRAPH_RESOURCE_TEXTURE
			? Ral_ResourceStateValidForTexture( state ) : qfalse;
}

static int FindPass( const ralFrameGraphDescription_t *description,
		uint32_t id ) {
	uint32_t i;
	for ( i = 0u; i < description->passCount; ++i )
		if ( description->passes[i].id == id ) return (int)i;
	return -1;
}

static int FindResource( const ralFrameGraphDescription_t *description,
		uint32_t id ) {
	uint32_t i;
	for ( i = 0u; i < description->resourceCount; ++i )
		if ( description->resources[i].id == id ) return (int)i;
	return -1;
}

static qboolean ResourceValid( const ralFrameGraphResource_t *resource ) {
	if ( !resource || resource->id == 0u || resource->generation == 0u
			|| resource->generation == UINT64_MAX || resource->identity == (uintptr_t)0
			|| !QueueValid( resource->initialQueue )
			|| !ResourceStateValid( resource->kind, &resource->initialState )
			|| resource->lifetime < RAL_FRAME_GRAPH_RESOURCE_EXTERNAL
			|| resource->lifetime > RAL_FRAME_GRAPH_RESOURCE_TRANSIENT
			|| !BoolValid( resource->allowAlias ) ) return qfalse;
	if ( resource->lifetime == RAL_FRAME_GRAPH_RESOURCE_TRANSIENT )
		return resource->compatibilityKey != 0u && resource->size != 0u
			&& PowerOfTwo( resource->alignment )
			&& resource->initialState.usage == RAL_RESOURCE_USAGE_UNDEFINED;
	return resource->compatibilityKey == 0u && resource->allowAlias == qfalse;
}

static qboolean DescriptionShapeValid(
		const ralFrameGraphDescription_t *description ) {
	uint32_t i,j,transientCount=0u;
	if ( !description
			|| description->schemaVersion != RAL_FRAME_GRAPH_SCHEMA_VERSION
			|| description->generation == 0u || description->generation == UINT64_MAX
			|| !BackendValid( description->backendType )
			|| description->resourceCount == 0u
			|| description->resourceCount > RAL_FRAME_GRAPH_MAX_RESOURCES
			|| description->passCount == 0u
			|| description->passCount > RAL_FRAME_GRAPH_MAX_PASSES
			|| description->useCount == 0u
			|| description->useCount > RAL_FRAME_GRAPH_MAX_USES
			|| description->explicitDependencyCount
				> RAL_FRAME_GRAPH_MAX_EXPLICIT_DEPENDENCIES ) return qfalse;
	for ( i = 0u; i < description->resourceCount; ++i ) {
		if ( !ResourceValid( &description->resources[i] ) ) return qfalse;
		if ( description->resources[i].lifetime
				== RAL_FRAME_GRAPH_RESOURCE_TRANSIENT ) transientCount++;
		for ( j = 0u; j < i; ++j )
			if ( description->resources[i].id == description->resources[j].id
					|| description->resources[i].identity
						== description->resources[j].identity ) return qfalse;
	}
	for ( i = 0u; i < description->passCount; ++i ) {
		if ( description->passes[i].id == 0u
				|| !QueueValid( description->passes[i].queue ) ) return qfalse;
		for ( j = 0u; j < i; ++j )
			if ( description->passes[i].id == description->passes[j].id ) return qfalse;
	}
	if ( transientCount == 0u
			&& (description->transientPolicy.backendType != RAL_BACKEND_VULKAN
				|| description->transientPolicy.generation != 0u
				|| description->transientPolicy.budgetBytes != 0u
				|| description->transientPolicy.explicitAliasing != qfalse) ) return qfalse;
	return qtrue;
}

static qboolean AddEdge( ralFrameGraphPlan_t *candidate,
		ralFrameGraphCompileScratch_t *scratch, uint32_t sourceIndex,
		uint32_t destinationIndex, uint32_t resourceBit,
		ralFrameGraphHazardFlags_t hazard ) {
	uint32_t i;
	if ( sourceIndex == destinationIndex || hazard == 0u ) return qfalse;
	for ( i = 0u; i < candidate->edgeCount; ++i ) {
		ralFrameGraphEdge_t *edge = &candidate->edges[i];
		if ( edge->sourcePassId == candidate->description.passes[sourceIndex].id
				&& edge->destinationPassId
					== candidate->description.passes[destinationIndex].id ) {
			edge->resourceMask |= resourceBit;
			edge->hazards |= hazard;
			scratch->adjacency[sourceIndex][destinationIndex] = qtrue;
			return qtrue;
		}
	}
	if ( candidate->edgeCount >= RAL_FRAME_GRAPH_MAX_EDGES ) return qfalse;
	candidate->edges[candidate->edgeCount].sourcePassId
		= candidate->description.passes[sourceIndex].id;
	candidate->edges[candidate->edgeCount].destinationPassId
		= candidate->description.passes[destinationIndex].id;
	candidate->edges[candidate->edgeCount].resourceMask = resourceBit;
	candidate->edges[candidate->edgeCount].hazards = hazard;
	candidate->edgeCount++;
	scratch->adjacency[sourceIndex][destinationIndex] = qtrue;
	return qtrue;
}

static qboolean BuildUsesAndExplicitEdges( ralFrameGraphPlan_t *candidate,
		ralFrameGraphCompileScratch_t *scratch ) {
	const ralFrameGraphDescription_t *description = &candidate->description;
	uint32_t i,j;
	for ( i = 0u; i < RAL_FRAME_GRAPH_MAX_PASSES; ++i )
		for ( j = 0u; j < RAL_FRAME_GRAPH_MAX_RESOURCES; ++j )
			scratch->useByPassResource[i][j] = -1;
	for ( i = 0u; i < description->useCount; ++i ) {
		const ralFrameGraphUse_t *use = &description->uses[i];
		const int passIndex = FindPass( description, use->passId );
		const int resourceIndex = FindResource( description, use->resourceId );
		if ( passIndex < 0 || resourceIndex < 0
				|| use->access < RAL_FRAME_GRAPH_ACCESS_READ
				|| use->access > RAL_FRAME_GRAPH_ACCESS_READ_WRITE
				|| use->state.usage == RAL_RESOURCE_USAGE_UNDEFINED
				|| !ResourceStateValid( description->resources[resourceIndex].kind,
					&use->state )
				|| scratch->useByPassResource[passIndex][resourceIndex] >= 0 ) return qfalse;
		scratch->useByPassResource[passIndex][resourceIndex] = (int16_t)i;
		scratch->passUseCounts[passIndex]++;
		scratch->resourceUseCounts[resourceIndex]++;
	}
	for ( i = 0u; i < description->passCount; ++i )
		if ( scratch->passUseCounts[i] == 0u ) return qfalse;
	for ( i = 0u; i < description->resourceCount; ++i )
		if ( scratch->resourceUseCounts[i] == 0u ) return qfalse;
	for ( i = 0u; i < description->explicitDependencyCount; ++i ) {
		const ralFrameGraphExplicitDependency_t *dependency
			= &description->explicitDependencies[i];
		const int source = FindPass( description, dependency->sourcePassId );
		const int destination = FindPass( description, dependency->destinationPassId );
		if ( source < 0 || destination < 0 || source == destination ) return qfalse;
		for ( j = 0u; j < i; ++j )
			if ( dependency->sourcePassId
					== description->explicitDependencies[j].sourcePassId
					&& dependency->destinationPassId
						== description->explicitDependencies[j].destinationPassId ) return qfalse;
		if ( !AddEdge( candidate, scratch, (uint32_t)source,
				(uint32_t)destination, 0u, RAL_FRAME_GRAPH_HAZARD_EXPLICIT ) ) return qfalse;
	}
	return qtrue;
}

static qboolean BuildHazardEdges( ralFrameGraphPlan_t *candidate,
		ralFrameGraphCompileScratch_t *scratch ) {
	const ralFrameGraphDescription_t *description = &candidate->description;
	uint32_t resourceIndex;
	for ( resourceIndex = 0u; resourceIndex < description->resourceCount;
			++resourceIndex ) {
		int lastWriter = -1;
		int lastUsePass = -1;
		uint32_t readers = 0u;
		uint32_t passIndex;
		const uint32_t resourceBit = 1u << resourceIndex;
		for ( passIndex = 0u; passIndex < description->passCount; ++passIndex ) {
			const int useIndex = scratch->useByPassResource[passIndex][resourceIndex];
			const ralFrameGraphUse_t *use;
			uint32_t readerIndex;
			if ( useIndex < 0 ) continue;
			use = &description->uses[useIndex];
			if ( lastUsePass >= 0 ) {
				const int previousUseIndex
					= scratch->useByPassResource[lastUsePass][resourceIndex];
				const ralFrameGraphUse_t *previousUse = &description->uses[previousUseIndex];
				ralFrameGraphHazardFlags_t orderFlags = 0u;
				if ( !StateExact( &previousUse->state, &use->state ) )
					orderFlags |= RAL_FRAME_GRAPH_HAZARD_STATE;
				if ( description->passes[lastUsePass].queue
						!= description->passes[passIndex].queue )
					orderFlags |= RAL_FRAME_GRAPH_HAZARD_QUEUE_HANDOFF;
				if ( orderFlags != 0u && !AddEdge( candidate, scratch,
						(uint32_t)lastUsePass, passIndex, resourceBit, orderFlags ) ) return qfalse;
			}
			if ( use->access == RAL_FRAME_GRAPH_ACCESS_READ ) {
				if ( lastWriter >= 0 && !AddEdge( candidate, scratch,
						(uint32_t)lastWriter, passIndex, resourceBit,
						RAL_FRAME_GRAPH_HAZARD_RAW ) ) return qfalse;
				readers |= 1u << passIndex;
				lastUsePass = (int)passIndex;
				continue;
			}
			if ( lastWriter >= 0 ) {
				ralFrameGraphHazardFlags_t flags = RAL_FRAME_GRAPH_HAZARD_WAW;
				if ( use->access == RAL_FRAME_GRAPH_ACCESS_READ_WRITE )
					flags |= RAL_FRAME_GRAPH_HAZARD_RAW;
				if ( !AddEdge( candidate, scratch, (uint32_t)lastWriter,
						passIndex, resourceBit, flags ) ) return qfalse;
			}
			for ( readerIndex = 0u; readerIndex < description->passCount;
					++readerIndex )
				if ( (readers & (1u << readerIndex)) != 0u
						&& !AddEdge( candidate, scratch, readerIndex, passIndex,
							resourceBit, RAL_FRAME_GRAPH_HAZARD_WAR ) ) return qfalse;
			readers = 0u;
			lastWriter = (int)passIndex;
			lastUsePass = (int)passIndex;
		}
	}
	return qtrue;
}

static qboolean BuildTopologicalOrder( ralFrameGraphPlan_t *candidate,
		const ralFrameGraphCompileScratch_t *scratch, uint32_t *outPassIndices ) {
	uint32_t indegree[RAL_FRAME_GRAPH_MAX_PASSES] = { 0u };
	qboolean emitted[RAL_FRAME_GRAPH_MAX_PASSES] = { qfalse };
	uint32_t i,j;
	for ( i = 0u; i < candidate->description.passCount; ++i )
		for ( j = 0u; j < candidate->description.passCount; ++j )
			if ( scratch->adjacency[i][j] ) indegree[j]++;
	for ( i = 0u; i < candidate->description.passCount; ++i ) {
		uint32_t selected = UINT32_MAX;
		for ( j = 0u; j < candidate->description.passCount; ++j )
			if ( !emitted[j] && indegree[j] == 0u ) { selected = j; break; }
		if ( selected == UINT32_MAX ) return qfalse;
		emitted[selected] = qtrue;
		outPassIndices[i] = selected;
		candidate->topologicalPassIds[i] = candidate->description.passes[selected].id;
		for ( j = 0u; j < candidate->description.passCount; ++j )
			if ( scratch->adjacency[selected][j] ) indegree[j]--;
	}
	candidate->topologicalPassCount = candidate->description.passCount;
	return qtrue;
}

static qboolean AddTransition( ralFrameGraphPlan_t *candidate,
		const ralFrameGraphResource_t *resource, uint32_t sourcePassId,
		uint32_t destinationPassId, const ralResourceState_t *before,
		const ralResourceState_t *after, ralQueueType_t sourceQueue,
		ralQueueType_t destinationQueue ) {
	ralFrameGraphTransition_t *transition;
	if ( candidate->transitionCount >= RAL_FRAME_GRAPH_MAX_TRANSITIONS ) return qfalse;
	transition = &candidate->transitions[candidate->transitionCount++];
	transition->resourceId = resource->id;
	transition->resourceIdentity = resource->identity;
	transition->resourceGeneration = resource->generation;
	transition->resourceKind = resource->kind;
	transition->sourcePassId = sourcePassId;
	transition->destinationPassId = destinationPassId;
	transition->before = *before;
	transition->after = *after;
	transition->sourceQueue = sourceQueue;
	transition->destinationQueue = destinationQueue;
	return qtrue;
}

static qboolean AddQueueHandoff( ralFrameGraphPlan_t *candidate,
		const ralFrameGraphResource_t *resource, uint32_t releasePassId,
		uint32_t acquirePassId, const ralResourceState_t *before,
		const ralResourceState_t *after, ralQueueType_t sourceQueue,
		ralQueueType_t destinationQueue ) {
	ralFrameGraphQueueHandoff_t *handoff;
	if ( candidate->queueHandoffCount >= RAL_FRAME_GRAPH_MAX_QUEUE_HANDOFFS ) return qfalse;
	handoff = &candidate->queueHandoffs[candidate->queueHandoffCount++];
	handoff->resourceId = resource->id;
	handoff->resourceIdentity = resource->identity;
	handoff->resourceGeneration = resource->generation;
	handoff->resourceKind = resource->kind;
	handoff->releasePassId = releasePassId;
	handoff->acquirePassId = acquirePassId;
	handoff->before = *before;
	handoff->after = *after;
	handoff->sourceQueue = sourceQueue;
	handoff->destinationQueue = destinationQueue;
	return qtrue;
}

static qboolean BuildTransitionsAndTransientPlan( ralFrameGraphPlan_t *candidate,
		const ralFrameGraphCompileScratch_t *scratch,
		const uint32_t *topologicalPassIndices ) {
	const ralFrameGraphDescription_t *description = &candidate->description;
	uint32_t resourceIndex;
	for ( resourceIndex = 0u; resourceIndex < description->resourceCount;
			++resourceIndex ) {
		const ralFrameGraphResource_t *resource = &description->resources[resourceIndex];
		ralResourceState_t currentState = resource->initialState;
		ralQueueType_t currentQueue = resource->initialQueue;
		uint32_t previousPassId = 0u;
		uint32_t firstPosition = UINT32_MAX, lastPosition = 0u;
		qboolean seen = qfalse;
		uint32_t position;
		for ( position = 0u; position < description->passCount; ++position ) {
			const uint32_t passIndex = topologicalPassIndices[position];
			const int useIndex = scratch->useByPassResource[passIndex][resourceIndex];
			const ralFrameGraphUse_t *use;
			const ralFrameGraphPass_t *pass;
			if ( useIndex < 0 ) continue;
			use = &description->uses[useIndex];
			pass = &description->passes[passIndex];
			if ( !seen ) {
				firstPosition = position;
				if ( resource->lifetime == RAL_FRAME_GRAPH_RESOURCE_TRANSIENT )
					currentQueue = pass->queue;
			}
			lastPosition = position;
			if ( !StateExact( &currentState, &use->state ) || currentQueue != pass->queue )
				if ( !AddTransition( candidate, resource, previousPassId, pass->id,
						&currentState, &use->state, currentQueue, pass->queue ) ) return qfalse;
			if ( seen && currentQueue != pass->queue
					&& !AddQueueHandoff( candidate, resource, previousPassId, pass->id,
						&currentState, &use->state, currentQueue, pass->queue ) ) return qfalse;
			currentState = use->state;
			currentQueue = pass->queue;
			previousPassId = pass->id;
			seen = qtrue;
		}
		if ( resource->lifetime == RAL_FRAME_GRAPH_RESOURCE_TRANSIENT ) {
			ralTransientRequest_t *request;
			if ( candidate->transientRequestCount >= RAL_TRANSIENT_MAX_REQUESTS
					|| firstPosition == UINT32_MAX ) return qfalse;
			request = &candidate->transientRequests[candidate->transientRequestCount++];
			request->resourceIdentity = resource->identity;
			request->resourceGeneration = resource->generation;
			request->compatibilityKey = resource->compatibilityKey;
			request->size = resource->size;
			request->alignment = resource->alignment;
			request->firstPass = firstPosition;
			request->lastPass = lastPosition;
			request->allowAlias = resource->allowAlias;
		}
	}
	if ( candidate->transientRequestCount > 0u ) {
		uint32_t i;
		if ( description->transientPolicy.backendType != description->backendType
				|| description->transientPolicy.generation != description->generation ) return qfalse;
		for ( i = 1u; i < candidate->transientRequestCount; ++i ) {
			ralTransientRequest_t item = candidate->transientRequests[i];
			uint32_t j = i;
			while ( j > 0u ) {
				const ralTransientRequest_t *previous = &candidate->transientRequests[j - 1u];
				if ( previous->firstPass < item.firstPass
						|| (previous->firstPass == item.firstPass
							&& previous->resourceIdentity < item.resourceIdentity) ) break;
				candidate->transientRequests[j] = candidate->transientRequests[j - 1u];
				--j;
			}
			candidate->transientRequests[j] = item;
		}
		if ( !Ral_TransientPlanBuild( &description->transientPolicy,
				candidate->transientRequests, candidate->transientRequestCount,
				&candidate->transientPlan ) ) return qfalse;
	}
	return qtrue;
}

static qboolean CompileInternal( const ralFrameGraphDescription_t *description,
		ralFrameGraphPlan_t *candidate ) {
	ralFrameGraphCompileScratch_t scratch;
	uint32_t topologicalPassIndices[RAL_FRAME_GRAPH_MAX_PASSES];
	if ( !candidate || !DescriptionShapeValid( description ) ) return qfalse;
	memset( candidate, 0, sizeof( *candidate ) );
	memset( &scratch, 0, sizeof( scratch ) );
	memset( topologicalPassIndices, 0, sizeof( topologicalPassIndices ) );
	candidate->schemaVersion = RAL_FRAME_GRAPH_SCHEMA_VERSION;
	candidate->description = *description;
	if ( !BuildUsesAndExplicitEdges( candidate, &scratch )
			|| !BuildHazardEdges( candidate, &scratch )
			|| !BuildTopologicalOrder( candidate, &scratch, topologicalPassIndices )
			|| !BuildTransitionsAndTransientPlan( candidate, &scratch,
				topologicalPassIndices ) ) return qfalse;
	candidate->ready = qtrue;
	return qtrue;
}

qboolean RalFrameGraph_Compile( const ralFrameGraphDescription_t *description,
		ralFrameGraphPlan_t *outPlan ) {
	ralFrameGraphPlan_t candidate;
	if ( !outPlan || !CompileInternal( description, &candidate ) ) return qfalse;
	*outPlan = candidate;
	return qtrue;
}

static qboolean ResourceExact( const ralFrameGraphResource_t *a,
		const ralFrameGraphResource_t *b ) {
	return a->id == b->id && a->generation == b->generation
		&& a->identity == b->identity && a->kind == b->kind
		&& a->lifetime == b->lifetime && a->compatibilityKey == b->compatibilityKey
		&& a->size == b->size && a->alignment == b->alignment
		&& a->allowAlias == b->allowAlias && StateExact( &a->initialState, &b->initialState )
		&& a->initialQueue == b->initialQueue;
}

static qboolean DescriptionExact( const ralFrameGraphDescription_t *a,
		const ralFrameGraphDescription_t *b ) {
	uint32_t i;
	if ( a->schemaVersion != b->schemaVersion || a->generation != b->generation
			|| a->backendType != b->backendType || a->resourceCount != b->resourceCount
			|| a->passCount != b->passCount || a->useCount != b->useCount
			|| a->explicitDependencyCount != b->explicitDependencyCount ) return qfalse;
	for ( i = 0u; i < a->resourceCount; ++i )
		if ( !ResourceExact( &a->resources[i], &b->resources[i] ) ) return qfalse;
	for ( i = 0u; i < a->passCount; ++i )
		if ( a->passes[i].id != b->passes[i].id
				|| a->passes[i].queue != b->passes[i].queue ) return qfalse;
	for ( i = 0u; i < a->useCount; ++i )
		if ( a->uses[i].passId != b->uses[i].passId
				|| a->uses[i].resourceId != b->uses[i].resourceId
				|| a->uses[i].access != b->uses[i].access
				|| !StateExact( &a->uses[i].state, &b->uses[i].state ) ) return qfalse;
	for ( i = 0u; i < a->explicitDependencyCount; ++i )
		if ( a->explicitDependencies[i].sourcePassId
				!= b->explicitDependencies[i].sourcePassId
				|| a->explicitDependencies[i].destinationPassId
					!= b->explicitDependencies[i].destinationPassId ) return qfalse;
	return a->transientPolicy.backendType == b->transientPolicy.backendType
		&& a->transientPolicy.generation == b->transientPolicy.generation
		&& a->transientPolicy.budgetBytes == b->transientPolicy.budgetBytes
		&& a->transientPolicy.explicitAliasing == b->transientPolicy.explicitAliasing;
}

static qboolean EdgeExact( const ralFrameGraphEdge_t *a,
		const ralFrameGraphEdge_t *b ) {
	return a->sourcePassId == b->sourcePassId
		&& a->destinationPassId == b->destinationPassId
		&& a->resourceMask == b->resourceMask && a->hazards == b->hazards;
}

static qboolean TransitionExact( const ralFrameGraphTransition_t *a,
		const ralFrameGraphTransition_t *b ) {
	return a->resourceId == b->resourceId
		&& a->resourceIdentity == b->resourceIdentity
		&& a->resourceGeneration == b->resourceGeneration
		&& a->resourceKind == b->resourceKind && a->sourcePassId == b->sourcePassId
		&& a->destinationPassId == b->destinationPassId
		&& StateExact( &a->before, &b->before ) && StateExact( &a->after, &b->after )
		&& a->sourceQueue == b->sourceQueue && a->destinationQueue == b->destinationQueue;
}

static qboolean HandoffExact( const ralFrameGraphQueueHandoff_t *a,
		const ralFrameGraphQueueHandoff_t *b ) {
	return a->resourceId == b->resourceId
		&& a->resourceIdentity == b->resourceIdentity
		&& a->resourceGeneration == b->resourceGeneration
		&& a->resourceKind == b->resourceKind && a->releasePassId == b->releasePassId
		&& a->acquirePassId == b->acquirePassId
		&& StateExact( &a->before, &b->before ) && StateExact( &a->after, &b->after )
		&& a->sourceQueue == b->sourceQueue && a->destinationQueue == b->destinationQueue;
}

static qboolean PlanFieldsExact( const ralFrameGraphPlan_t *a,
		const ralFrameGraphPlan_t *b ) {
	const ralTransientPlan_t emptyTransientPlan = { 0 };
	uint32_t i;
	if ( a->schemaVersion != b->schemaVersion || a->ready != b->ready
			|| !DescriptionExact( &a->description, &b->description )
			|| a->edgeCount != b->edgeCount
			|| a->topologicalPassCount != b->topologicalPassCount
			|| a->transitionCount != b->transitionCount
			|| a->queueHandoffCount != b->queueHandoffCount
			|| a->transientRequestCount != b->transientRequestCount ) return qfalse;
	for ( i = 0u; i < a->edgeCount; ++i )
		if ( !EdgeExact( &a->edges[i], &b->edges[i] ) ) return qfalse;
	for ( i = 0u; i < a->topologicalPassCount; ++i )
		if ( a->topologicalPassIds[i] != b->topologicalPassIds[i] ) return qfalse;
	for ( i = 0u; i < a->transitionCount; ++i )
		if ( !TransitionExact( &a->transitions[i], &b->transitions[i] ) ) return qfalse;
	for ( i = 0u; i < a->queueHandoffCount; ++i )
		if ( !HandoffExact( &a->queueHandoffs[i], &b->queueHandoffs[i] ) ) return qfalse;
	for ( i = 0u; i < a->transientRequestCount; ++i ) {
		const ralTransientRequest_t *x = &a->transientRequests[i];
		const ralTransientRequest_t *y = &b->transientRequests[i];
		if ( x->resourceIdentity != y->resourceIdentity
				|| x->resourceGeneration != y->resourceGeneration
				|| x->compatibilityKey != y->compatibilityKey || x->size != y->size
				|| x->alignment != y->alignment || x->firstPass != y->firstPass
				|| x->lastPass != y->lastPass || x->allowAlias != y->allowAlias ) return qfalse;
	}
	if ( a->transientRequestCount > 0u
			&& !Ral_TransientPlanExact( &a->transientPlan, &b->transientPlan ) ) return qfalse;
	if ( a->transientRequestCount == 0u
			&& (memcmp(&a->transientPlan,&emptyTransientPlan,sizeof(emptyTransientPlan)) != 0
				|| memcmp(&b->transientPlan,&emptyTransientPlan,sizeof(emptyTransientPlan)) != 0) ) return qfalse;
	return qtrue;
}

qboolean RalFrameGraph_PlanExact( const ralFrameGraphPlan_t *a,
		const ralFrameGraphPlan_t *b ) {
	ralFrameGraphPlan_t rebuiltA,rebuiltB;
	if ( !a || !b || a->ready != qtrue || b->ready != qtrue
			|| !CompileInternal( &a->description, &rebuiltA )
			|| !CompileInternal( &b->description, &rebuiltB ) ) return qfalse;
	return PlanFieldsExact( a, &rebuiltA ) && PlanFieldsExact( b, &rebuiltB )
		&& PlanFieldsExact( a, b );
}
