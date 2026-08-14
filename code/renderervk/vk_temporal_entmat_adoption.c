// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "vk_temporal_entmat_adoption.h"

#include <string.h>

static qboolean OwnerConfiguredFor( vkTemporalEntMatAdoptionOwner_t *owner,
		uint32_t frameCount ) {
	if ( !owner || frameCount == 0 || frameCount > VK_TEMPORAL_ENTMAT_MAX_SLOTS )
		return qfalse;
	if ( owner->configured ) return owner->frameCount == frameCount ? qtrue : qfalse;
	owner->frameCount = frameCount;
	owner->configured = qtrue;
	return qtrue;
}

static qboolean WrapperIsLive( const vkTemporalEntMatAdoptionOwner_t *owner,
		const ralBuffer_t *wrapper ) {
	uint32_t i;
	if ( !owner || !wrapper ) return qfalse;
	for ( i = 0; i < owner->frameCount; ++i ) {
		if ( owner->slots[i].ready && owner->slots[i].adopted == wrapper )
			return qtrue;
	}
	return qfalse;
}

void VK_TemporalEntMatAdoptionInit( vkTemporalEntMatAdoptionOwner_t *owner ) {
	if ( owner ) memset( owner, 0, sizeof( *owner ) );
}

qboolean VK_TemporalEntMatAdoptionResetAfterFence(
		vkTemporalEntMatAdoptionOwner_t *owner, uint32_t frameCount,
		uint32_t frameIndex ) {
	vkTemporalEntMatAdoptionSlot_t *slot;
	if ( !owner || frameCount == 0 || frameCount > VK_TEMPORAL_ENTMAT_MAX_SLOTS
			|| frameIndex >= frameCount || owner->releasing ) return qfalse;
	if ( !OwnerConfiguredFor( owner, frameCount ) )
		return qfalse;
	slot = &owner->slots[frameIndex];
	slot->begun = qfalse;
	slot->resetAfterFence = qtrue;
	return qtrue;
}

qboolean VK_TemporalEntMatAdoptionEnsure(
		vkTemporalEntMatAdoptionOwner_t *owner, ralBackend_t *backend,
		uint32_t frameIndex, void *nativeBuffer, size_t size,
		uint32_t allocationGeneration,
		const vkTemporalEntMatAdoptionOps_t *ops ) {
	vkTemporalEntMatAdoptionSlot_t *slot;
	ralBuffer_t *candidate;
	ralBuffer_t *oldWrapper;
	uint32_t i;

	if ( !owner || !owner->configured || owner->releasing
			|| !backend || !nativeBuffer || size == 0
			|| allocationGeneration == 0 || allocationGeneration == UINT32_MAX
			|| frameIndex >= owner->frameCount
			|| !ops || !ops->adopt || !ops->destroy || !ops->consumerRebind )
		return qfalse;
	slot = &owner->slots[frameIndex];
	if ( !slot->resetAfterFence || slot->begun ) return qfalse;
	if ( owner->backend && owner->backend != backend ) return qfalse;

	if ( slot->ready ) {
		if ( allocationGeneration == slot->allocationGeneration ) {
			if ( nativeBuffer != slot->nativeBuffer || size != slot->size )
				return qfalse;
			return ops->consumerRebind( ops->userData, frameIndex,
				slot->adopted, slot->allocationGeneration );
		}
		if ( allocationGeneration < slot->allocationGeneration ) return qfalse;
	}

	for ( i = 0; i < owner->frameCount; ++i ) {
		if ( i != frameIndex && owner->slots[i].ready
				&& owner->slots[i].nativeBuffer == nativeBuffer ) return qfalse;
	}

	candidate = ops->adopt( ops->userData, backend, nativeBuffer, size,
		"wired-temporal-entmat-adopted" );
	if ( !candidate ) return qfalse;
	if ( (void *)candidate == nativeBuffer ) {
		// A broken adoption callback returned the raw handle itself rather than
		// a wrapper. Never route that engine-owned identity through destroy.
		return qfalse;
	}
	if ( WrapperIsLive( owner, candidate ) ) {
		// The callback returned an already-owned wrapper. It is not a candidate
		// allocation and therefore must not be destroyed here.
		return qfalse;
	}
	if ( !ops->consumerRebind( ops->userData, frameIndex, candidate,
			allocationGeneration ) ) {
		ops->destroy( ops->userData, candidate );
		return qfalse;
	}

	oldWrapper = slot->ready ? slot->adopted : NULL;
	slot->nativeBuffer = nativeBuffer;
	slot->size = size;
	slot->adopted = candidate;
	slot->allocationGeneration = allocationGeneration;
	slot->consumerDetached = qfalse;
	slot->ready = qtrue;
	owner->backend = backend;
	if ( oldWrapper ) ops->destroy( ops->userData, oldWrapper );
	return qtrue;
}

qboolean VK_TemporalEntMatAdoptionBeginFrame(
		vkTemporalEntMatAdoptionOwner_t *owner, uint32_t frameIndex,
		const vkTemporalEntMatAdoptionOps_t *ops ) {
	vkTemporalEntMatAdoptionSlot_t *slot;
	if ( !owner || !owner->configured || owner->releasing
			|| frameIndex >= owner->frameCount
			|| !ops || !ops->consumerBeginFrame ) return qfalse;
	slot = &owner->slots[frameIndex];
	if ( !slot->ready || !slot->adopted || !slot->resetAfterFence || slot->begun )
		return qfalse;
	if ( !ops->consumerBeginFrame( ops->userData, frameIndex, slot->adopted,
			slot->allocationGeneration ) ) return qfalse;
	slot->resetAfterFence = qfalse;
	slot->begun = qtrue;
	return qtrue;
}

const ralBuffer_t *VK_TemporalEntMatAdoptionGet(
		const vkTemporalEntMatAdoptionOwner_t *owner, uint32_t frameIndex,
		uint32_t *outAllocationGeneration ) {
	if ( !owner || !owner->configured || owner->releasing
			|| !outAllocationGeneration
			|| frameIndex >= owner->frameCount || !owner->slots[frameIndex].ready )
		return NULL;
	if ( owner->slots[frameIndex].consumerDetached ) return NULL;
	*outAllocationGeneration = owner->slots[frameIndex].allocationGeneration;
	return owner->slots[frameIndex].adopted;
}

qboolean VK_TemporalEntMatAdoptionReleaseAfterIdle(
		vkTemporalEntMatAdoptionOwner_t *owner, qboolean idleProven,
		const vkTemporalEntMatAdoptionOps_t *ops ) {
	uint32_t i;
	if ( !owner || !idleProven || !ops || !ops->destroy || !ops->consumerDetach )
		return qfalse;
	if ( !owner->configured ) return qtrue;
	owner->releasing = qtrue;
	for ( i = 0; i < owner->frameCount; ++i ) {
		vkTemporalEntMatAdoptionSlot_t *slot = &owner->slots[i];
		if ( slot->ready && !slot->consumerDetached ) {
			if ( !ops->consumerDetach( ops->userData, i,
					slot->adopted, slot->allocationGeneration ) ) return qfalse;
			slot->consumerDetached = qtrue;
		}
	}
	for ( i = owner->frameCount; i > 0; --i ) {
		vkTemporalEntMatAdoptionSlot_t *slot = &owner->slots[i - 1u];
		if ( slot->ready ) ops->destroy( ops->userData, slot->adopted );
	}
	memset( owner, 0, sizeof( *owner ) );
	return qtrue;
}
