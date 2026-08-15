// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "vk_temporal_generic_pipeline_table.h"

#include <limits.h>
#include <string.h>

typedef struct {
	const vkTemporalGenericPipelineTable_t *table;
	const ralPipeline_t *ordinary;
	const ralPipeline_t *retainedOrdinary;
} vkTemporalCandidateGuard_t;

static qboolean CandidateAllowed( ralPipeline_t *candidate,
		const void *opaque ) {
	const vkTemporalCandidateGuard_t *guard =
		(const vkTemporalCandidateGuard_t *)opaque;
	uint32_t i, j;
	if ( !candidate || !guard || !guard->table
			|| candidate == guard->ordinary
			|| candidate == guard->retainedOrdinary ) return qfalse;
	for ( i = 0; i < guard->table->capacity; ++i ) {
		const vkTemporalGenericPipelineSlot_t *slot = &guard->table->slots[i];
		if ( !slot->occupied ) continue;
		if ( slot->ordinaryPipeline == candidate ) return qfalse;
		if ( !slot->factory.ready ) continue;
		for ( j = 0; j < 3u; ++j )
			if ( slot->factory.pipelines[j] == candidate ) return qfalse;
	}
	return qtrue;
}

void VK_TemporalGenericPipelineTableInit(
		vkTemporalGenericPipelineTable_t *owner ) {
	if ( owner ) memset( owner, 0, sizeof( *owner ) );
}

qboolean VK_TemporalGenericPipelineTablePrepare(
		vkTemporalGenericPipelineTable_t *owner, uint32_t capacity,
		vkTemporalPipelineLayoutOwner_t *layoutOwner,
		const vkTemporalGenericPipelineTableMemoryOps_t *memoryOps ) {
	vkTemporalGenericPipelineSlot_t *candidate;
	size_t bytes;
	uint32_t nextGeneration;
	if ( !owner || !capacity || !layoutOwner || !layoutOwner->ready
			|| !layoutOwner->adopted || !layoutOwner->allocationGeneration
			|| !memoryOps || !memoryOps->alloc || !memoryOps->free ) return qfalse;
	if ( owner->slots ) {
		return owner->capacity == capacity && owner->guardLease
			&& owner->layoutOwner == layoutOwner
			&& owner->layoutAllocationGeneration ==
				layoutOwner->allocationGeneration;
	}
	if ( owner->allocationGeneration == UINT32_MAX
			|| (size_t)capacity > SIZE_MAX / sizeof( *candidate ) ) return qfalse;
	bytes = (size_t)capacity * sizeof( *candidate );
	candidate = (vkTemporalGenericPipelineSlot_t *)memoryOps->alloc( bytes );
	if ( !candidate ) return qfalse;
	memset( candidate, 0, bytes );
	if ( !VK_TemporalPipelineLayoutAcquire( layoutOwner ) ) {
		memoryOps->free( candidate );
		return qfalse;
	}
	nextGeneration = owner->allocationGeneration + 1u;
	owner->slots = candidate;
	owner->capacity = capacity;
	owner->allocationGeneration = nextGeneration;
	owner->layoutOwner = layoutOwner;
	owner->layoutAllocationGeneration = layoutOwner->allocationGeneration;
	owner->guardLease = qtrue;
	return qtrue;
}

qboolean VK_TemporalGenericPipelineTableEnsureSlot(
		vkTemporalGenericPipelineTable_t *owner,
		const vkTemporalGenericPipelineTableEnsureInput_t *input,
		const vkTemporalPipelineFactoryOps_t *factoryOps,
		vkTemporalGenericPipelineReceipt_t *outReceipt ) {
	vkTemporalGenericPipelineSlot_t *slot;
	vkTemporalGenericPipelineFactoryInput_t factoryInput;
	vkTemporalGenericPipelineReceipt_t receipt;
	vkTemporalPipelineFactoryOps_t guardedOps;
	vkTemporalCandidateGuard_t guard;
	uint32_t nextGeneration;
	if ( !owner || !owner->slots || !owner->guardLease || !owner->layoutOwner
			|| !input || !input->recipe || !input->recipeReceipt || !input->base
			|| !input->ordinaryPipeline || !input->topologyGeneration
			|| !input->moduleRegistryGeneration || !factoryOps || !outReceipt
			|| input->slot >= owner->capacity
			|| input->recipeReceipt->slot != input->slot
			|| input->recipe->slot != input->slot
			|| input->recipe->ownerEpoch != input->recipeReceipt->ownerEpoch
			|| input->recipe->entryGeneration !=
				input->recipeReceipt->entryGeneration
			|| input->recipe->catalogId != input->recipeReceipt->catalogId
			|| !input->recipe->valid ) return qfalse;
	slot = &owner->slots[input->slot];
	if ( slot->occupied && ( slot->recipeOwnerEpoch != input->recipe->ownerEpoch
			|| slot->recipeEntryGeneration != input->recipe->entryGeneration
			|| slot->catalogId != input->recipe->catalogId
			|| slot->moduleRegistryGeneration != input->moduleRegistryGeneration
			|| slot->ordinaryPipeline != input->ordinaryPipeline ) )
		return qfalse;
	if ( !slot->occupied && owner->lastEntryGeneration == UINT32_MAX ) return qfalse;
	memset( &factoryInput, 0, sizeof( factoryInput ) );
	factoryInput.key = input->recipe->key;
	factoryInput.pipelineGeneration = input->recipe->entryGeneration;
	factoryInput.topologyGeneration = input->topologyGeneration;
	factoryInput.catalogGeneration = VK_TEMPORAL_GENERIC_CATALOG_GENERATION;
	factoryInput.sceneFormat = input->recipe->sceneFormat;
	factoryInput.depthFormat = input->recipe->depthFormat;
	guardedOps = *factoryOps;
	guard.table = owner;
	guard.ordinary = input->ordinaryPipeline;
	guard.retainedOrdinary = input->retainedOrdinaryPipeline;
	guardedOps.candidateAllowed = CandidateAllowed;
	guardedOps.candidateContext = &guard;
	if ( !VK_TemporalGenericPipelineFactoryEnsure( &slot->factory,
			owner->layoutOwner, input->base, &factoryInput, &guardedOps ) )
		return qfalse;
	if ( !slot->occupied ) {
		nextGeneration = owner->lastEntryGeneration + 1u;
		slot->recipeOwnerEpoch = input->recipe->ownerEpoch;
		slot->recipeEntryGeneration = input->recipe->entryGeneration;
		slot->catalogId = input->recipe->catalogId;
		slot->moduleRegistryGeneration = input->moduleRegistryGeneration;
		slot->ordinaryPipeline = input->ordinaryPipeline;
		slot->entryGeneration = nextGeneration;
		slot->occupied = qtrue;
		owner->lastEntryGeneration = nextGeneration;
	}
	receipt.tableGeneration = owner->allocationGeneration;
	receipt.slot = input->slot;
	receipt.entryGeneration = slot->entryGeneration;
	receipt.recipeOwnerEpoch = slot->recipeOwnerEpoch;
	receipt.recipeEntryGeneration = slot->recipeEntryGeneration;
	receipt.factoryAllocationGeneration = slot->factory.allocationGeneration;
	receipt.catalogId = slot->catalogId;
	*outReceipt = receipt;
	return qtrue;
}

qboolean VK_TemporalGenericPipelineTableGet(
		const vkTemporalGenericPipelineTable_t *owner,
		const vkTemporalGenericPipelineReceipt_t *receipt,
		ralPipeline_t *outPipelines[3] ) {
	const vkTemporalGenericPipelineSlot_t *slot;
	ralPipeline_t *candidate[3];
	if ( !owner || !owner->slots || !receipt || !outPipelines
			|| receipt->tableGeneration != owner->allocationGeneration
			|| receipt->slot >= owner->capacity ) return qfalse;
	slot = &owner->slots[receipt->slot];
	if ( !slot->occupied || !slot->factory.ready
			|| receipt->entryGeneration != slot->entryGeneration
			|| receipt->recipeOwnerEpoch != slot->recipeOwnerEpoch
			|| receipt->recipeEntryGeneration != slot->recipeEntryGeneration
			|| receipt->factoryAllocationGeneration !=
				slot->factory.allocationGeneration
			|| receipt->catalogId != slot->catalogId ) return qfalse;
	candidate[0] = slot->factory.pipelines[0];
	candidate[1] = slot->factory.pipelines[1];
	candidate[2] = slot->factory.pipelines[2];
	if ( !candidate[0] || !candidate[1] || !candidate[2]
			|| candidate[0] == candidate[1] || candidate[0] == candidate[2]
			|| candidate[1] == candidate[2] ) return qfalse;
	memcpy( outPipelines, candidate, sizeof( candidate ) );
	return qtrue;
}

qboolean VK_TemporalGenericPipelineTableGetSlotReceipt(
		const vkTemporalGenericPipelineTable_t *owner, uint32_t slotIndex,
		vkTemporalGenericPipelineReceipt_t *outReceipt ) {
	const vkTemporalGenericPipelineSlot_t *slot;
	vkTemporalGenericPipelineReceipt_t receipt;
	if ( !owner || !owner->slots || !outReceipt || slotIndex >= owner->capacity )
		return qfalse;
	slot = &owner->slots[slotIndex];
	if ( !slot->occupied || !slot->factory.ready || !slot->entryGeneration
			|| !slot->factory.allocationGeneration ) return qfalse;
	receipt.tableGeneration = owner->allocationGeneration;
	receipt.slot = slotIndex;
	receipt.entryGeneration = slot->entryGeneration;
	receipt.recipeOwnerEpoch = slot->recipeOwnerEpoch;
	receipt.recipeEntryGeneration = slot->recipeEntryGeneration;
	receipt.factoryAllocationGeneration = slot->factory.allocationGeneration;
	receipt.catalogId = slot->catalogId;
	*outReceipt = receipt;
	return qtrue;
}

qboolean VK_TemporalGenericPipelineTablePreflightSlot(
		const vkTemporalGenericPipelineTable_t *owner, uint32_t slotIndex,
		const ralPipeline_t *expectedOrdinary,
		vkTemporalGenericPipelineReceipt_t *outReceipt,
		ralPipeline_t *outPipelines[3] ) {
	vkTemporalGenericPipelineReceipt_t receipt;
	ralPipeline_t *pipelines[3];
	if ( !owner || !owner->slots || !expectedOrdinary || !outReceipt
			|| !outPipelines || slotIndex >= owner->capacity
			|| owner->slots[slotIndex].ordinaryPipeline != expectedOrdinary
			|| !VK_TemporalGenericPipelineTableGetSlotReceipt( owner,
				slotIndex, &receipt )
			|| !VK_TemporalGenericPipelineTableGet( owner, &receipt,
				pipelines ) ) return qfalse;
	*outReceipt = receipt;
	memcpy( outPipelines, pipelines, sizeof( pipelines ) );
	return qtrue;
}

qboolean VK_TemporalGenericPipelineTableSlotReady(
		const vkTemporalGenericPipelineTable_t *owner, uint32_t slot ) {
	return owner && owner->slots && slot < owner->capacity
		&& owner->slots[slot].occupied && owner->slots[slot].factory.ready;
}

qboolean VK_TemporalGenericPipelineTableHasLive(
		const vkTemporalGenericPipelineTable_t *owner ) {
	return owner && owner->slots && owner->guardLease;
}

qboolean VK_TemporalGenericPipelineTableReleaseRangeAfterIdle(
		vkTemporalGenericPipelineTable_t *owner, uint32_t first, uint32_t end,
		ralBackend_t *backend, const vkTemporalPipelineFactoryOps_t *factoryOps ) {
	uint32_t i;
	if ( !owner || !backend || !factoryOps || !factoryOps->destroy
			|| !factoryOps->drain || first > end ) return qfalse;
	if ( !owner->slots ) return qtrue;
	if ( end > owner->capacity ) return qfalse;
	for ( i = first; i < end; ++i ) {
		if ( owner->slots[i].occupied
				&& !VK_TemporalGenericPipelineFactoryRetire(
					&owner->slots[i].factory, factoryOps ) ) return qfalse;
	}
	// One same-backend drain covers every retired child and any failed
	// candidate protected by the table's guard lease.
	if ( !factoryOps->drain( backend ) ) return qfalse;
	for ( i = first; i < end; ++i ) {
		if ( owner->slots[i].occupied
				&& !VK_TemporalGenericPipelineFactoryFinalizeAfterDrain(
					&owner->slots[i].factory ) ) return qfalse;
		memset( &owner->slots[i], 0, sizeof( owner->slots[i] ) );
	}
	return qtrue;
}

qboolean VK_TemporalGenericPipelineTableReleaseAfterIdle(
		vkTemporalGenericPipelineTable_t *owner, ralBackend_t *backend,
		const vkTemporalPipelineFactoryOps_t *factoryOps,
		const vkTemporalGenericPipelineTableMemoryOps_t *memoryOps ) {
	uint32_t generation, entryGeneration;
	if ( !owner || !backend || !factoryOps || !memoryOps || !memoryOps->free )
		return qfalse;
	if ( !owner->slots ) return qtrue;
	if ( !VK_TemporalGenericPipelineTableReleaseRangeAfterIdle( owner, 0,
			owner->capacity, backend, factoryOps ) ) return qfalse;
	if ( owner->guardLease ) {
		if ( !owner->layoutOwner
				|| !VK_TemporalPipelineLayoutReleaseLease(owner->layoutOwner) )
			return qfalse;
		owner->guardLease = qfalse;
	}
	generation = owner->allocationGeneration;
	entryGeneration = owner->lastEntryGeneration;
	memoryOps->free( owner->slots );
	memset( owner, 0, sizeof( *owner ) );
	owner->allocationGeneration = generation;
	owner->lastEntryGeneration = entryGeneration;
	return qtrue;
}

qboolean VK_TemporalCachedMainRebuild(
		ralPipeline_t **ordinaryPipeline, qboolean *depthFade,
		int32_t *pipelineCreateCount, void *context,
		const vkTemporalCachedMainRebuildOps_t *ops,
		qboolean *outDeferredWork ) {
	ralPipeline_t *oldPipeline, *candidate;
	qboolean oldDepthFade, attempted = qfalse, valid = qfalse;
	qboolean deferredWork = qfalse;
	int32_t oldCreateCount;
	vkTemporalCachedMainMaterializeResult_t materializeResult =
		VK_TEMPORAL_CACHED_MAIN_REJECTED;
	if ( !ordinaryPipeline || !*ordinaryPipeline || !depthFade
			|| !pipelineCreateCount || !ops || !ops->createAndCapture
			|| !ops->captureState || !ops->materialize || !ops->destroy
			|| !ops->decline || !outDeferredWork ) return qfalse;
	oldPipeline = *ordinaryPipeline;
	oldDepthFade = *depthFade;
	oldCreateCount = *pipelineCreateCount;
	*ordinaryPipeline = NULL;
	ops->createAndCapture( context );
	candidate = *ordinaryPipeline;
	*pipelineCreateCount = oldCreateCount;
	if ( ops->captureState( context, &attempted, &valid )
			&& candidate && candidate != oldPipeline && attempted && valid )
		materializeResult = ops->materialize( context, oldPipeline );
	if ( materializeResult == VK_TEMPORAL_CACHED_MAIN_READY ) {
		ops->destroy( oldPipeline );
		*outDeferredWork = qtrue;
		return qtrue;
	}
	if ( materializeResult == VK_TEMPORAL_CACHED_MAIN_DEFERRED_FAILURE )
		deferredWork = qtrue;
	if ( candidate && candidate != oldPipeline ) {
		ops->destroy( candidate );
		deferredWork = qtrue;
	}
	*ordinaryPipeline = oldPipeline;
	*depthFade = oldDepthFade;
	*pipelineCreateCount = oldCreateCount;
	ops->decline( context );
	*outDeferredWork = deferredWork;
	return qfalse;
}
