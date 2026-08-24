// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "vk_temporal_iqm_payload.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

static qboolean KeyValid( const vkTemporalIqmPayloadKey_t *key ) {
	uint32_t i, j;
	if ( !key || !key->backend || !key->frameCount
			|| key->frameCount > VK_TEMPORAL_IQM_PAYLOAD_MAX_FRAMES
			|| key->maxStorageBufferRange < (uint64_t)TEMPORAL_IQM_SLOT_BYTES
			|| key->protectedCount > VK_TEMPORAL_IQM_PAYLOAD_MAX_PROTECTED ) return qfalse;
	for ( i = 0; i < key->protectedCount; ++i ) {
		if ( !key->protectedIdentities[i] ) return qfalse;
		for ( j = 0; j < i; ++j )
			if ( key->protectedIdentities[i] == key->protectedIdentities[j] ) return qfalse;
	}
	return qtrue;
}

static qboolean KeyEqual( const vkTemporalIqmPayloadKey_t *a,
		const vkTemporalIqmPayloadKey_t *b ) {
	if ( !a || !b || a->backend != b->backend
			|| a->maxStorageBufferRange != b->maxStorageBufferRange
			|| a->frameCount != b->frameCount
			|| a->protectedCount != b->protectedCount ) return qfalse;
	for ( uint32_t i = 0; i < a->protectedCount; ++i )
		if ( a->protectedIdentities[i] != b->protectedIdentities[i] ) return qfalse;
	return qtrue;
}

static qboolean Protected( const vkTemporalIqmPayloadKey_t *key,
		const void *identity ) {
	if ( !key || !identity ) return qfalse;
	if ( identity == key->backend ) return qtrue;
	for ( uint32_t i = 0; i < key->protectedCount; ++i )
		if ( identity == key->protectedIdentities[i] ) return qtrue;
	return qfalse;
}

static qboolean SlotEmpty( const vkTemporalIqmPayloadSlot_t *slot ) {
	return slot && !slot->buffer && !slot->cpuShadow && !slot->group
		&& !slot->allocationGeneration && !slot->prepareGeneration ? qtrue : qfalse;
}

static qboolean RolesDistinct( const vkTemporalIqmPayloadOwner_t *owner ) {
	const void *roles[1u + VK_TEMPORAL_IQM_PAYLOAD_MAX_FRAMES * 3u];
	uint32_t count = 0;
	if ( !owner || !owner->layout ) return qfalse;
	roles[count++] = owner->layout;
	for ( uint32_t i = 0; i < owner->key.frameCount; ++i ) {
		if ( !( owner->readySlotMask & ( 1u << i ) ) ) continue;
		roles[count++] = owner->slots[i].buffer;
		roles[count++] = owner->slots[i].cpuShadow;
		roles[count++] = owner->slots[i].group;
	}
	for ( uint32_t i = 0; i < count; ++i ) {
		if ( !roles[i] || Protected( &owner->key, roles[i] ) ) return qfalse;
		for ( uint32_t j = 0; j < i; ++j )
			if ( roles[i] == roles[j] ) return qfalse;
	}
	return qtrue;
}

static qboolean OwnerValid( const vkTemporalIqmPayloadOwner_t *owner ) {
	uint32_t validMask;
	if ( !owner || owner->initialized != qtrue || owner->ready != qtrue
			|| !KeyValid( &owner->key ) || !owner->layout
			|| !owner->ownerAllocationGeneration
			|| owner->ownerAllocationGeneration == UINT32_MAX
			|| !owner->readySlotMask || owner->preparedSlot >= owner->key.frameCount )
		return qfalse;
	validMask = ( 1u << owner->key.frameCount ) - 1u;
	if ( owner->readySlotMask & ~validMask ) return qfalse;
	for ( uint32_t i = 0; i < VK_TEMPORAL_IQM_PAYLOAD_MAX_FRAMES; ++i ) {
		const vkTemporalIqmPayloadSlot_t *slot = &owner->slots[i];
		if ( i < owner->key.frameCount && ( owner->readySlotMask & ( 1u << i ) ) ) {
			if ( !slot->buffer || !slot->cpuShadow || !slot->group
					|| !slot->allocationGeneration
					|| slot->allocationGeneration == UINT32_MAX
					|| !slot->prepareGeneration
					|| slot->prepareGeneration == UINT32_MAX ) return qfalse;
		} else if ( !SlotEmpty( slot ) ) return qfalse;
	}
	return RolesDistinct( owner );
}

static qboolean AnyRole( const vkTemporalIqmPayloadOwner_t *owner,
		const void *identity ) {
	if ( !owner || !identity ) return qfalse;
	if ( identity == owner->layout ) return qtrue;
	for ( uint32_t i = 0; i < VK_TEMPORAL_IQM_PAYLOAD_MAX_FRAMES; ++i )
		if ( identity == owner->slots[i].buffer || identity == owner->slots[i].cpuShadow
				|| identity == owner->slots[i].group ) return qtrue;
	return qfalse;
}

static void DestroySlot( vkTemporalIqmPayloadSlot_t *slot ) {
	if ( !slot ) return;
	if ( slot->group ) Ral_DestroyBindGroup( slot->group );
	free( slot->cpuShadow );
	if ( slot->buffer ) Ral_DestroyBuffer( slot->buffer );
	memset( slot, 0, sizeof( *slot ) );
}

static void DestroyLive( vkTemporalIqmPayloadOwner_t *owner ) {
	if ( !owner ) return;
	for ( uint32_t i = owner->key.frameCount; i-- > 0; )
		if ( owner->readySlotMask & ( 1u << i ) ) DestroySlot( &owner->slots[i] );
	if ( owner->layout ) Ral_DestroyBindGroupLayout( owner->layout );
	owner->layout = NULL; owner->readySlotMask = 0; owner->ready = qfalse;
}

static void PreserveCountersAndClear( vkTemporalIqmPayloadOwner_t *owner ) {
	uint32_t ownerCounter, slotCounters[VK_TEMPORAL_IQM_PAYLOAD_MAX_FRAMES];
	uint32_t prepareCounters[VK_TEMPORAL_IQM_PAYLOAD_MAX_FRAMES];
	if ( !owner ) return;
	ownerCounter = owner->nextOwnerAllocationGeneration;
	memcpy( slotCounters, owner->nextSlotAllocationGeneration, sizeof( slotCounters ) );
	memcpy( prepareCounters, owner->nextPrepareGeneration, sizeof( prepareCounters ) );
	memset( owner, 0, sizeof( *owner ) ); owner->initialized = qtrue;
	owner->nextOwnerAllocationGeneration = ownerCounter;
	memcpy( owner->nextSlotAllocationGeneration, slotCounters, sizeof( slotCounters ) );
	memcpy( owner->nextPrepareGeneration, prepareCounters, sizeof( prepareCounters ) );
}

void VK_TemporalIqmPayloadInit( vkTemporalIqmPayloadOwner_t *owner ) {
	if ( owner ) { memset( owner, 0, sizeof( *owner ) ); owner->initialized = qtrue; }
}

qboolean VK_TemporalIqmPayloadNeedsIdle(
		const vkTemporalIqmPayloadOwner_t *owner,
		const vkTemporalIqmPayloadKey_t *key ) {
	if ( !owner || !KeyValid( key ) || !VK_TemporalIqmPayloadHasLive( owner ) )
		return qfalse;
	if ( !OwnerValid( owner ) ) return qtrue;
	return KeyEqual( &owner->key, key ) ? qfalse : qtrue;
}

static qboolean CreateSlot( vkTemporalIqmPayloadOwner_t *candidate,
		const vkTemporalIqmPayloadOwner_t *live, uint32_t commandSlot ) {
	ralBufferCreateInfo_t bci;
	ralBindingValue_t value;
	ralBindGroupCreateInfo_t gci;
	vkTemporalIqmPayloadSlot_t slot;
	memset( &slot, 0, sizeof( slot ) );
	memset( &bci, 0, sizeof( bci ) ); bci.size = TEMPORAL_IQM_SLOT_BYTES;
	bci.usage = RAL_BUFFER_STORAGE | RAL_BUFFER_TRANSFER_DST;
	bci.memory = RAL_MEMORY_DEVICE_LOCAL;
	bci.debugName = "wired-temporal-iqm-payload";
	slot.buffer = Ral_CreateBuffer( candidate->key.backend, &bci );
	if ( !slot.buffer || Protected( &candidate->key, slot.buffer )
			|| AnyRole( live, slot.buffer ) || AnyRole( candidate, slot.buffer ) )
		return qfalse;
	slot.cpuShadow = calloc( 1, TEMPORAL_IQM_SLOT_BYTES );
	if ( !slot.cpuShadow || Protected( &candidate->key, slot.cpuShadow )
			|| AnyRole( live, slot.cpuShadow ) || AnyRole( candidate, slot.cpuShadow )
			|| slot.cpuShadow == (void *)slot.buffer ) {
		free( slot.cpuShadow );
		Ral_DestroyBuffer( slot.buffer ); return qfalse;
	}
	memset( &value, 0, sizeof( value ) ); value.binding = 0;
	value.type = RAL_BIND_STORAGE_BUFFER; value.buffer = slot.buffer;
	value.bufferOffset = 0; value.bufferRange = TEMPORAL_IQM_SLOT_BYTES;
	memset( &gci, 0, sizeof( gci ) ); gci.layout = candidate->layout;
	gci.values = &value; gci.numValues = 1; gci.debugName = "wired-temporal-iqm-payload-bg";
	slot.group = Ral_CreateBindGroup( candidate->key.backend, &gci );
	if ( !slot.group || Protected( &candidate->key, slot.group )
			|| AnyRole( live, slot.group ) || AnyRole( candidate, slot.group )
			|| (void *)slot.group == (void *)slot.buffer || (void *)slot.group == slot.cpuShadow ) {
		if ( slot.group && (void *)slot.group != (void *)slot.buffer
				&& (void *)slot.group != slot.cpuShadow && !Protected( &candidate->key, slot.group )
				&& !AnyRole( live, slot.group ) && !AnyRole( candidate, slot.group ) )
			Ral_DestroyBindGroup( slot.group );
		free( slot.cpuShadow ); Ral_DestroyBuffer( slot.buffer ); return qfalse;
	}
	if ( candidate->nextSlotAllocationGeneration[commandSlot] >= UINT32_MAX - 1u ) {
		DestroySlot( &slot ); return qfalse;
	}
	if ( candidate->nextPrepareGeneration[commandSlot] >= UINT32_MAX - 1u ) {
		DestroySlot( &slot ); return qfalse;
	}
	slot.allocationGeneration = ++candidate->nextSlotAllocationGeneration[commandSlot];
	slot.prepareGeneration = ++candidate->nextPrepareGeneration[commandSlot];
	candidate->slots[commandSlot] = slot;
	candidate->readySlotMask |= 1u << commandSlot;
	candidate->preparedSlot = commandSlot;
	return qtrue;
}

qboolean VK_TemporalIqmPayloadPrepareAfterFence(
		vkTemporalIqmPayloadOwner_t *owner,
		const vkTemporalIqmPayloadKey_t *key, uint32_t commandSlot,
		qboolean slotFenceCompleted, qboolean idleProven ) {
	vkTemporalIqmPayloadOwner_t candidate, live;
	ralBindEntry_t entry;
	ralBindGroupLayoutCreateInfo_t lci;
	qboolean replacing, createdLayout = qfalse;
	if ( !owner || owner->initialized != qtrue || !KeyValid( key )
			|| commandSlot >= key->frameCount ) return qfalse;
	live = *owner;
	if ( VK_TemporalIqmPayloadHasLive( &live ) && !OwnerValid( &live ) ) return qfalse;
	replacing = live.ready && !KeyEqual( &live.key, key );
	if ( replacing && ( idleProven != qtrue || live.layoutLeaseCount ) ) return qfalse;
	if ( live.ready && !replacing && ( live.readySlotMask & ( 1u << commandSlot ) ) ) {
		if ( slotFenceCompleted != qtrue ) return qfalse;
		if ( owner->nextPrepareGeneration[commandSlot] >= UINT32_MAX - 1u ) return qfalse;
		owner->preparedSlot = commandSlot;
		owner->slots[commandSlot].prepareGeneration =
			++owner->nextPrepareGeneration[commandSlot];
		memset( owner->slots[commandSlot].cpuShadow, 0, TEMPORAL_IQM_SLOT_BYTES );
		return qtrue;
	}
	if ( replacing ) {
		candidate = live;
		PreserveCountersAndClear( &candidate );
	} else candidate = live;
	if ( !candidate.ready ) {
		if ( candidate.nextOwnerAllocationGeneration >= UINT32_MAX - 1u ) return qfalse;
		candidate.key = *key; candidate.ownerAllocationGeneration =
			++candidate.nextOwnerAllocationGeneration;
		entry = (ralBindEntry_t){ 0u, RAL_BIND_STORAGE_BUFFER, 1u, RAL_STAGE_VERTEX };
		memset( &lci, 0, sizeof( lci ) ); lci.entries = &entry; lci.numEntries = 1;
		lci.debugName = "wired-temporal-iqm-payload-bgl";
		candidate.layout = Ral_CreateBindGroupLayout( key->backend, &lci );
		if ( !candidate.layout || Protected( key, candidate.layout )
				|| AnyRole( &live, candidate.layout ) ) {
			if ( candidate.layout && !Protected( key, candidate.layout )
					&& !AnyRole( &live, candidate.layout ) )
				Ral_DestroyBindGroupLayout( candidate.layout );
			return qfalse;
		}
		createdLayout = qtrue;
		candidate.ready = qtrue;
	}
	if ( !CreateSlot( &candidate, &live, commandSlot ) ) {
		if ( createdLayout && candidate.layout ) Ral_DestroyBindGroupLayout( candidate.layout );
		return qfalse;
	}
	if ( !OwnerValid( &candidate ) ) {
		DestroySlot( &candidate.slots[commandSlot] );
		if ( createdLayout && candidate.layout ) Ral_DestroyBindGroupLayout( candidate.layout );
		return qfalse;
	}
	*owner = candidate;
	memset( owner->slots[commandSlot].cpuShadow, 0, TEMPORAL_IQM_SLOT_BYTES );
	if ( replacing ) DestroyLive( &live );
	return qtrue;
}

qboolean VK_TemporalIqmPayloadGetReceipt(
		const vkTemporalIqmPayloadOwner_t *owner, uint32_t commandSlot,
		vkTemporalIqmPayloadReceipt_t *outReceipt ) {
	vkTemporalIqmPayloadReceipt_t candidate;
	if ( !outReceipt || !OwnerValid( owner ) || commandSlot >= owner->key.frameCount
			|| commandSlot != owner->preparedSlot
			|| !( owner->readySlotMask & ( 1u << commandSlot ) ) ) return qfalse;
	memset( &candidate, 0, sizeof( candidate ) ); candidate.backend = owner->key.backend;
	candidate.layout = owner->layout; candidate.buffer = owner->slots[commandSlot].buffer;
	candidate.cpuShadowIdentity = owner->slots[commandSlot].cpuShadow;
	candidate.group = owner->slots[commandSlot].group;
	candidate.descriptorRange = TEMPORAL_IQM_SLOT_BYTES;
	candidate.recordCapacity = TEMPORAL_IQM_MAX_RECORDS;
	candidate.recordBytes = TEMPORAL_IQM_RECORD_SIZE;
	candidate.ownerAllocationGeneration = owner->ownerAllocationGeneration;
	candidate.slotAllocationGeneration = owner->slots[commandSlot].allocationGeneration;
	candidate.prepareGeneration = owner->slots[commandSlot].prepareGeneration;
	candidate.commandSlot = commandSlot; candidate.frameCount = owner->key.frameCount;
	candidate.ready = qtrue; *outReceipt = candidate; return qtrue;
}

static qboolean ReceiptValid( const vkTemporalIqmPayloadReceipt_t *r ) {
	return r && r->ready == qtrue && r->backend && r->layout && r->buffer
		&& r->cpuShadowIdentity && r->group
		&& r->descriptorRange == TEMPORAL_IQM_SLOT_BYTES
		&& r->recordCapacity == TEMPORAL_IQM_MAX_RECORDS
		&& r->recordBytes == TEMPORAL_IQM_RECORD_SIZE
		&& r->ownerAllocationGeneration && r->ownerAllocationGeneration != UINT32_MAX
		&& r->slotAllocationGeneration && r->slotAllocationGeneration != UINT32_MAX
		&& r->prepareGeneration && r->prepareGeneration != UINT32_MAX
		&& r->frameCount && r->frameCount <= VK_TEMPORAL_IQM_PAYLOAD_MAX_FRAMES
		&& r->commandSlot < r->frameCount ? qtrue : qfalse;
}

qboolean VK_TemporalIqmPayloadReceiptExact(
		const vkTemporalIqmPayloadReceipt_t *a,
		const vkTemporalIqmPayloadReceipt_t *b ) {
	if ( !ReceiptValid( a ) || !ReceiptValid( b ) ) return qfalse;
	return a->backend == b->backend && a->layout == b->layout
		&& a->buffer == b->buffer && a->cpuShadowIdentity == b->cpuShadowIdentity
		&& a->group == b->group
		&& a->descriptorRange == b->descriptorRange
		&& a->recordCapacity == b->recordCapacity && a->recordBytes == b->recordBytes
		&& a->ownerAllocationGeneration == b->ownerAllocationGeneration
		&& a->slotAllocationGeneration == b->slotAllocationGeneration
		&& a->prepareGeneration == b->prepareGeneration
		&& a->commandSlot == b->commandSlot && a->frameCount == b->frameCount
		&& a->ready == b->ready ? qtrue : qfalse;
}

qboolean VK_TemporalIqmPayloadAcquireLayoutLease(
		vkTemporalIqmPayloadOwner_t *owner,
		ralBindGroupLayout_t **outLayout, uint32_t *outOwnerGeneration ) {
	if ( !outLayout || !outOwnerGeneration || !OwnerValid( owner )
			|| owner->layoutLeaseCount == UINT32_MAX ) return qfalse;
	++owner->layoutLeaseCount; *outLayout = owner->layout;
	*outOwnerGeneration = owner->ownerAllocationGeneration; return qtrue;
}

qboolean VK_TemporalIqmPayloadReleaseLayoutLease(
		vkTemporalIqmPayloadOwner_t *owner,
		ralBindGroupLayout_t *layout, uint32_t ownerGeneration ) {
	if ( !OwnerValid( owner ) || !layout || layout != owner->layout
			|| !ownerGeneration || ownerGeneration != owner->ownerAllocationGeneration
			|| !owner->layoutLeaseCount ) return qfalse;
	--owner->layoutLeaseCount; return qtrue;
}

qboolean VK_TemporalIqmPayloadHasLive( const vkTemporalIqmPayloadOwner_t *owner ) {
	if ( !owner ) return qfalse;
	if ( owner->layout ) return qtrue;
	for ( uint32_t i = 0; i < VK_TEMPORAL_IQM_PAYLOAD_MAX_FRAMES; ++i )
		if ( owner->slots[i].buffer || owner->slots[i].cpuShadow || owner->slots[i].group )
			return qtrue;
	return qfalse;
}

qboolean VK_TemporalIqmPayloadReleaseAfterIdle(
		vkTemporalIqmPayloadOwner_t *owner, qboolean idleProven ) {
	vkTemporalIqmPayloadOwner_t live;
	if ( !owner ) return qfalse;
	if ( !VK_TemporalIqmPayloadHasLive( owner ) ) return qtrue;
	if ( idleProven != qtrue || !OwnerValid( owner ) || owner->layoutLeaseCount )
		return qfalse;
	live = *owner; PreserveCountersAndClear( owner ); DestroyLive( &live );
	return qtrue;
}
