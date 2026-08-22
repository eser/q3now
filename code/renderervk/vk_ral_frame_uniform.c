// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "vk_ral_frame_uniform.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static qboolean BoolValid( qboolean value ) {
	return value == qfalse || value == qtrue;
}

static qboolean ConfigValid( const vkRalFrameUniformConfig_t *config,
		qboolean requireDebugName ) {
	if ( !config || config->byteSize == 0u
			|| config->byteSize > VK_RAL_FRAME_UNIFORM_MAX_BYTES
			|| !BoolValid( config->partialRequired ) ) return qfalse;
	if ( requireDebugName == qtrue
			&& ( !config->debugName || !config->debugName[0] ) ) return qfalse;
	if ( config->partialRequired == qfalse )
		return config->partialOffset == 0u && config->partialSize == 0u;
	return config->partialSize > 0u
		&& config->partialOffset < config->byteSize
		&& config->partialSize <= config->byteSize - config->partialOffset;
}

static uint64_t HashBytes( const unsigned char *bytes, uint64_t size ) {
	uint64_t hash = UINT64_C( 1469598103934665603 );
	uint64_t i;
	if ( !bytes || size == 0u ) return 0u;
	for ( i = 0u; i < size; i++ ) {
		hash ^= bytes[i];
		hash *= UINT64_C( 1099511628211 );
	}
	return hash ? hash : 1u;
}

static qboolean AllocationValid( const ralAllocationReceipt_t *allocation,
		const ralBuffer_t *buffer, uint64_t byteSize ) {
	return allocation && buffer && byteSize > 0u
		&& Ral_AllocationReceiptExact( allocation, allocation )
		&& allocation->ready == qtrue
		&& allocation->memoryClass == RAL_ALLOCATION_UPLOAD
		&& allocation->residency == RAL_ALLOCATION_RESIDENCY_PERMANENT
		&& allocation->ownerIdentity == (uintptr_t)buffer
		&& allocation->allocationGeneration > 0u
		&& allocation->allocationGeneration < UINT64_MAX
		&& allocation->requestedSize == byteSize
		&& allocation->committedSize >= byteSize;
}

static qboolean OwnerConfigValid( const vkRalFrameUniformOwner_t *owner ) {
	vkRalFrameUniformConfig_t config;
	if ( !owner ) return qfalse;
	memset( &config, 0, sizeof( config ) );
	config.byteSize = owner->byteSize;
	config.partialOffset = owner->partialOffset;
	config.partialSize = owner->partialSize;
	config.partialRequired = owner->partialRequired;
	return ConfigValid( &config, qfalse );
}

static qboolean ResourcesValid( const vkRalFrameUniformOwner_t *owner ) {
	uint32_t i;
	if ( !owner || owner->initialized != qtrue || owner->ready != qtrue
			|| !owner->backend || !OwnerConfigValid( owner )
			|| owner->ownerGeneration == 0u
			|| owner->ownerGeneration == UINT64_MAX
			|| !owner->buffers[0] || !owner->buffers[1]
			|| owner->buffers[0] == owner->buffers[1]
			|| !owner->shadows[0] || !owner->shadows[1]
			|| owner->shadows[0] == owner->shadows[1] ) return qfalse;
	for ( i = 0u; i < VK_RAL_FRAME_UNIFORM_SLOT_COUNT; i++ ) {
		if ( !AllocationValid( &owner->allocations[i], owner->buffers[i],
				owner->byteSize ) ) return qfalse;
	}
	return qtrue;
}

static qboolean UploadValid( const ralBufferUploadReceipt_t *upload,
		const ralBuffer_t *buffer,
		const ralAllocationReceipt_t *allocation,
		uint64_t offset, uint64_t size ) {
	const ralTransferRequest_t *request;
	if ( !upload || !buffer || !allocation
			|| !Ral_BufferUploadReceiptExact( upload, upload ) ) return qfalse;
	request = &upload->transfer.request;
	return request->direction == RAL_TRANSFER_UPLOAD
		&& request->resourceKind == RAL_TRANSFER_BUFFER
		&& request->resourceIdentity == (uintptr_t)buffer
		&& request->resourceGeneration == allocation->allocationGeneration
		&& request->byteOffset == offset && request->byteSize == size
		&& request->byteBudget == allocation->committedSize
		&& request->queue == RAL_QUEUE_GRAPHICS
		&& upload->transfer.state == RAL_TRANSFER_COMPLETED
		&& upload->transfer.transferGeneration > 0u
		&& upload->transfer.transferGeneration < UINT64_MAX
		&& upload->graphicsVisibilityGeneration
			== upload->transfer.completionGeneration
		&& upload->ready == qtrue;
}

static qboolean UploadEmpty( const ralBufferUploadReceipt_t *upload ) {
	ralBufferUploadReceipt_t empty;
	if ( !upload ) return qfalse;
	memset( &empty, 0, sizeof( empty ) );
	return memcmp( upload, &empty, sizeof( empty ) ) == 0 ? qtrue : qfalse;
}

static qboolean ResourceReceiptValid(
		const vkRalFrameUniformResourcesReceipt_t *receipt ) {
	vkRalFrameUniformConfig_t config;
	uint32_t i;
	if ( !receipt
			|| receipt->schemaVersion != VK_RAL_FRAME_UNIFORM_RECEIPT_SCHEMA
			|| !receipt->backend
			|| receipt->ownerGeneration == 0u
			|| receipt->ownerGeneration == UINT64_MAX
			|| receipt->ready != qtrue
			|| !receipt->buffers[0] || !receipt->buffers[1]
			|| receipt->buffers[0] == receipt->buffers[1] ) return qfalse;
	memset( &config, 0, sizeof( config ) );
	config.byteSize = receipt->byteSize;
	config.partialOffset = receipt->partialOffset;
	config.partialSize = receipt->partialSize;
	config.partialRequired = receipt->partialRequired;
	if ( !ConfigValid( &config, qfalse ) ) return qfalse;
	for ( i = 0u; i < VK_RAL_FRAME_UNIFORM_SLOT_COUNT; i++ ) {
		if ( !AllocationValid( &receipt->allocations[i], receipt->buffers[i],
				receipt->byteSize ) ) return qfalse;
	}
	return qtrue;
}

static qboolean FrameReceiptValid(
		const vkRalFrameUniformReceipt_t *receipt ) {
	vkRalFrameUniformConfig_t config;
	if ( !receipt
			|| receipt->schemaVersion != VK_RAL_FRAME_UNIFORM_RECEIPT_SCHEMA
			|| !receipt->backend || !receipt->buffer
			|| receipt->ownerGeneration == 0u
			|| receipt->ownerGeneration == UINT64_MAX
			|| receipt->slotGeneration == 0u
			|| receipt->slotGeneration == UINT64_MAX
			|| receipt->contentHash == 0u
			|| receipt->commandSlot >= VK_RAL_FRAME_UNIFORM_SLOT_COUNT
			|| !BoolValid( receipt->partialRequired )
			|| !BoolValid( receipt->fenceRequired )
			|| !BoolValid( receipt->fenceCompleted )
			|| receipt->fenceRequired != receipt->fenceCompleted
			|| receipt->ready != qtrue ) return qfalse;
	memset( &config, 0, sizeof( config ) );
	config.byteSize = receipt->byteSize;
	config.partialOffset = receipt->partialOffset;
	config.partialSize = receipt->partialSize;
	config.partialRequired = receipt->partialRequired;
	if ( !ConfigValid( &config, qfalse )
			|| !AllocationValid( &receipt->allocation, receipt->buffer,
				receipt->byteSize )
			|| !UploadValid( &receipt->finalWrite, receipt->buffer,
				&receipt->allocation, 0u, receipt->byteSize ) ) return qfalse;
	if ( receipt->partialRequired == qfalse )
		return receipt->partialHash == 0u
			&& UploadEmpty( &receipt->partialWrite );
	return receipt->partialHash > 0u
		&& UploadValid( &receipt->partialWrite, receipt->buffer,
			&receipt->allocation, receipt->partialOffset,
			receipt->partialSize )
		&& receipt->finalWrite.transfer.transferGeneration
			> receipt->partialWrite.transfer.transferGeneration;
}

static void BuildResources( const vkRalFrameUniformOwner_t *owner,
		vkRalFrameUniformResourcesReceipt_t *outReceipt ) {
	vkRalFrameUniformResourcesReceipt_t candidate;
	memset( &candidate, 0, sizeof( candidate ) );
	candidate.schemaVersion = VK_RAL_FRAME_UNIFORM_RECEIPT_SCHEMA;
	candidate.backend = owner->backend;
	memcpy( candidate.buffers, owner->buffers, sizeof( candidate.buffers ) );
	memcpy( candidate.allocations, owner->allocations,
		sizeof( candidate.allocations ) );
	candidate.ownerGeneration = owner->ownerGeneration;
	candidate.byteSize = owner->byteSize;
	candidate.partialOffset = owner->partialOffset;
	candidate.partialSize = owner->partialSize;
	candidate.partialRequired = owner->partialRequired;
	candidate.ready = qtrue;
	*outReceipt = candidate;
}

static void BuildFrame( const vkRalFrameUniformOwner_t *owner,
		uint32_t slot, vkRalFrameUniformReceipt_t *outReceipt ) {
	vkRalFrameUniformReceipt_t candidate;
	memset( &candidate, 0, sizeof( candidate ) );
	candidate.schemaVersion = VK_RAL_FRAME_UNIFORM_RECEIPT_SCHEMA;
	candidate.backend = owner->backend;
	candidate.buffer = owner->buffers[slot];
	candidate.allocation = owner->allocations[slot];
	candidate.partialWrite = owner->partialWrites[slot];
	candidate.finalWrite = owner->finalWrites[slot];
	candidate.ownerGeneration = owner->ownerGeneration;
	candidate.slotGeneration = owner->slotGenerations[slot];
	candidate.partialHash = owner->partialHashes[slot];
	candidate.contentHash = owner->contentHashes[slot];
	candidate.byteSize = owner->byteSize;
	candidate.partialOffset = owner->partialOffset;
	candidate.partialSize = owner->partialSize;
	candidate.commandSlot = slot;
	candidate.partialRequired = owner->partialRequired;
	candidate.fenceRequired = owner->fenceRequired;
	candidate.fenceCompleted = owner->fenceCompleted;
	candidate.ready = qtrue;
	*outReceipt = candidate;
}

static void DestroyCandidate( vkRalFrameUniformOwner_t *candidate ) {
	if ( !candidate ) return;
	if ( candidate->buffers[1]
			&& candidate->buffers[1] != candidate->buffers[0] )
		Ral_DestroyBuffer( candidate->buffers[1] );
	if ( candidate->buffers[0] ) Ral_DestroyBuffer( candidate->buffers[0] );
	free( candidate->shadows[1] );
	if ( candidate->shadows[0] != candidate->shadows[1] )
		free( candidate->shadows[0] );
	candidate->buffers[0] = candidate->buffers[1] = NULL;
	candidate->shadows[0] = candidate->shadows[1] = NULL;
}

void VK_RalFrameUniformInit( vkRalFrameUniformOwner_t *owner ) {
	if ( !owner ) return;
	memset( owner, 0, sizeof( *owner ) );
	owner->initialized = qtrue;
	owner->preparedSlot = UINT32_MAX;
}

qboolean VK_RalFrameUniformEnsure( vkRalFrameUniformOwner_t *owner,
		ralBackend_t *backend, const vkRalFrameUniformConfig_t *config ) {
	vkRalFrameUniformOwner_t candidate;
	ralBufferCreateInfo_t createInfo;
	uint32_t i;
	if ( !owner || owner->initialized != qtrue || !backend
			|| !ConfigValid( config, qtrue ) ) return qfalse;
	if ( owner->ready ) return ResourcesValid( owner )
		&& owner->backend == backend
		&& owner->byteSize == config->byteSize
		&& owner->partialOffset == config->partialOffset
		&& owner->partialSize == config->partialSize
		&& owner->partialRequired == config->partialRequired;
	if ( owner->backend || owner->buffers[0] || owner->buffers[1]
			|| owner->shadows[0] || owner->shadows[1] || owner->byteSize
			|| owner->partialOffset || owner->partialSize
			|| owner->partialRequired != qfalse
			|| owner->ownerGeneration >= UINT64_MAX - 1u ) return qfalse;
	candidate = *owner;
	memset( &createInfo, 0, sizeof( createInfo ) );
	createInfo.size = config->byteSize;
	createInfo.usage = RAL_BUFFER_UNIFORM | RAL_BUFFER_TRANSFER_DST;
	createInfo.memory = RAL_MEMORY_HOST_COHERENT;
	createInfo.debugName = config->debugName;
	for ( i = 0u; i < VK_RAL_FRAME_UNIFORM_SLOT_COUNT; i++ ) {
		candidate.shadows[i] = (unsigned char *)calloc( 1u,
			(size_t)config->byteSize );
		if ( !candidate.shadows[i] ) goto fail;
		candidate.buffers[i] = Ral_CreateBuffer( backend, &createInfo );
		if ( !candidate.buffers[i]
				|| ( i > 0u && candidate.buffers[i] == candidate.buffers[0] )
				|| !Ral_BufferGetAllocationReceipt( candidate.buffers[i],
					&candidate.allocations[i] )
				|| !AllocationValid( &candidate.allocations[i],
					candidate.buffers[i], config->byteSize ) ) goto fail;
	}
	candidate.backend = backend;
	candidate.byteSize = config->byteSize;
	candidate.partialOffset = config->partialOffset;
	candidate.partialSize = config->partialSize;
	candidate.partialRequired = config->partialRequired;
	candidate.ownerGeneration++;
	candidate.preparedSlot = UINT32_MAX;
	candidate.ready = qtrue;
	if ( !ResourcesValid( &candidate ) ) goto fail;
	*owner = candidate;
	return qtrue;

fail:
	DestroyCandidate( &candidate );
	return qfalse;
}

qboolean VK_RalFrameUniformGetResources(
		const vkRalFrameUniformOwner_t *owner,
		vkRalFrameUniformResourcesReceipt_t *outReceipt ) {
	vkRalFrameUniformResourcesReceipt_t candidate;
	if ( !ResourcesValid( owner ) || !outReceipt ) return qfalse;
	BuildResources( owner, &candidate );
	if ( !ResourceReceiptValid( &candidate ) ) return qfalse;
	*outReceipt = candidate;
	return qtrue;
}

qboolean VK_RalFrameUniformResourcesReceiptExact(
		const vkRalFrameUniformResourcesReceipt_t *a,
		const vkRalFrameUniformResourcesReceipt_t *b ) {
	uint32_t i;
	if ( !ResourceReceiptValid( a ) || !ResourceReceiptValid( b )
			|| a->schemaVersion != b->schemaVersion
			|| a->backend != b->backend
			|| a->ownerGeneration != b->ownerGeneration
			|| a->byteSize != b->byteSize
			|| a->partialOffset != b->partialOffset
			|| a->partialSize != b->partialSize
			|| a->partialRequired != b->partialRequired
			|| a->ready != b->ready ) return qfalse;
	for ( i = 0u; i < VK_RAL_FRAME_UNIFORM_SLOT_COUNT; i++ ) {
		if ( a->buffers[i] != b->buffers[i]
				|| !Ral_AllocationReceiptExact( &a->allocations[i],
					&b->allocations[i] ) ) return qfalse;
	}
	return qtrue;
}

qboolean VK_RalFrameUniformBeginSlot( vkRalFrameUniformOwner_t *owner,
		uint32_t commandSlot, qboolean fenceRequired,
		qboolean fenceCompleted ) {
	if ( !ResourcesValid( owner )
			|| commandSlot >= VK_RAL_FRAME_UNIFORM_SLOT_COUNT
			|| !BoolValid( fenceRequired ) || !BoolValid( fenceCompleted )
			|| fenceRequired != fenceCompleted
			|| owner->slotGenerations[commandSlot] >= UINT64_MAX - 1u )
		return qfalse;
	memset( owner->shadows[commandSlot], 0, (size_t)owner->byteSize );
	memset( &owner->partialWrites[commandSlot], 0,
		sizeof( owner->partialWrites[commandSlot] ) );
	memset( &owner->finalWrites[commandSlot], 0,
		sizeof( owner->finalWrites[commandSlot] ) );
	owner->partialHashes[commandSlot] = 0u;
	owner->contentHashes[commandSlot] = 0u;
	owner->slotGenerations[commandSlot]++;
	owner->preparedSlot = commandSlot;
	owner->fenceRequired = fenceRequired;
	owner->fenceCompleted = fenceCompleted;
	owner->prepared = qtrue;
	owner->partialReady = qfalse;
	owner->finalReady = qfalse;
	return qtrue;
}

qboolean VK_RalFrameUniformGetShadow( vkRalFrameUniformOwner_t *owner,
		uint32_t commandSlot, void **outShadow ) {
	void *candidate;
	if ( !ResourcesValid( owner ) || !outShadow
			|| owner->prepared != qtrue
			|| commandSlot != owner->preparedSlot
			|| commandSlot >= VK_RAL_FRAME_UNIFORM_SLOT_COUNT ) return qfalse;
	candidate = owner->shadows[commandSlot];
	if ( !candidate ) return qfalse;
	*outShadow = candidate;
	return qtrue;
}

qboolean VK_RalFrameUniformPublishPartial(
		vkRalFrameUniformOwner_t *owner, uint32_t commandSlot ) {
	ralBufferUploadReceipt_t write;
	uint64_t hash;
	if ( !ResourcesValid( owner ) || owner->prepared != qtrue
			|| owner->partialRequired != qtrue
			|| owner->partialReady != qfalse || owner->finalReady != qfalse
			|| commandSlot != owner->preparedSlot
			|| commandSlot >= VK_RAL_FRAME_UNIFORM_SLOT_COUNT ) return qfalse;
	hash = HashBytes( owner->shadows[commandSlot] + owner->partialOffset,
		owner->partialSize );
	if ( !hash || !Ral_BufferWriteImmediate( owner->buffers[commandSlot],
			owner->partialOffset,
			owner->shadows[commandSlot] + owner->partialOffset,
			owner->partialSize, &write )
			|| !UploadValid( &write, owner->buffers[commandSlot],
				&owner->allocations[commandSlot], owner->partialOffset,
				owner->partialSize ) ) return qfalse;
	owner->partialWrites[commandSlot] = write;
	owner->partialHashes[commandSlot] = hash;
	owner->partialReady = qtrue;
	return qtrue;
}

qboolean VK_RalFrameUniformPublishFinal(
		vkRalFrameUniformOwner_t *owner, uint32_t commandSlot,
		vkRalFrameUniformReceipt_t *outReceipt ) {
	vkRalFrameUniformOwner_t candidateOwner;
	vkRalFrameUniformReceipt_t candidateReceipt;
	ralBufferUploadReceipt_t write;
	uint64_t partialHash = 0u, contentHash;
	if ( !ResourcesValid( owner ) || !outReceipt
			|| owner->prepared != qtrue
			|| ( owner->partialRequired == qtrue
				&& owner->partialReady != qtrue )
			|| commandSlot != owner->preparedSlot
			|| commandSlot >= VK_RAL_FRAME_UNIFORM_SLOT_COUNT ) return qfalse;
	if ( owner->partialRequired == qtrue ) {
		partialHash = HashBytes( owner->shadows[commandSlot]
			+ owner->partialOffset, owner->partialSize );
		if ( !partialHash
				|| partialHash != owner->partialHashes[commandSlot] ) return qfalse;
	}
	contentHash = HashBytes( owner->shadows[commandSlot], owner->byteSize );
	if ( !contentHash
			|| !Ral_BufferWriteImmediate( owner->buffers[commandSlot], 0u,
				owner->shadows[commandSlot], owner->byteSize, &write )
			|| !UploadValid( &write, owner->buffers[commandSlot],
				&owner->allocations[commandSlot], 0u,
				owner->byteSize ) ) return qfalse;
	candidateOwner = *owner;
	candidateOwner.finalWrites[commandSlot] = write;
	candidateOwner.contentHashes[commandSlot] = contentHash;
	candidateOwner.finalReady = qtrue;
	BuildFrame( &candidateOwner, commandSlot, &candidateReceipt );
	if ( !FrameReceiptValid( &candidateReceipt ) ) return qfalse;
	*owner = candidateOwner;
	*outReceipt = candidateReceipt;
	return qtrue;
}

qboolean VK_RalFrameUniformGetReceipt(
		const vkRalFrameUniformOwner_t *owner, uint32_t commandSlot,
		vkRalFrameUniformReceipt_t *outReceipt ) {
	vkRalFrameUniformReceipt_t candidate;
	if ( !ResourcesValid( owner ) || !outReceipt
			|| owner->prepared != qtrue || owner->finalReady != qtrue
			|| ( owner->partialRequired == qtrue
				&& owner->partialReady != qtrue )
			|| commandSlot != owner->preparedSlot
			|| commandSlot >= VK_RAL_FRAME_UNIFORM_SLOT_COUNT ) return qfalse;
	BuildFrame( owner, commandSlot, &candidate );
	if ( !FrameReceiptValid( &candidate )
			|| ( owner->partialRequired == qtrue
				&& HashBytes( owner->shadows[commandSlot]
					+ owner->partialOffset, owner->partialSize )
					!= candidate.partialHash )
			|| HashBytes( owner->shadows[commandSlot], owner->byteSize )
				!= candidate.contentHash ) return qfalse;
	*outReceipt = candidate;
	return qtrue;
}

qboolean VK_RalFrameUniformReceiptExact(
		const vkRalFrameUniformReceipt_t *a,
		const vkRalFrameUniformReceipt_t *b ) {
	return FrameReceiptValid( a ) && FrameReceiptValid( b )
		&& !memcmp( a, b, sizeof( *a ) );
}

qboolean VK_RalFrameUniformHasLive(
		const vkRalFrameUniformOwner_t *owner ) {
	return owner && ( owner->backend || owner->buffers[0] || owner->buffers[1]
		|| owner->shadows[0] || owner->shadows[1] || owner->byteSize
		|| owner->partialOffset || owner->partialSize
		|| owner->partialRequired || owner->ready ) ? qtrue : qfalse;
}

void VK_RalFrameUniformRelease( vkRalFrameUniformOwner_t *owner ) {
	uint64_t ownerGeneration;
	uint64_t slotGenerations[VK_RAL_FRAME_UNIFORM_SLOT_COUNT];
	if ( !owner || owner->initialized != qtrue ) return;
	ownerGeneration = owner->ownerGeneration;
	memcpy( slotGenerations, owner->slotGenerations,
		sizeof( slotGenerations ) );
	DestroyCandidate( owner );
	memset( owner, 0, sizeof( *owner ) );
	owner->initialized = qtrue;
	owner->preparedSlot = UINT32_MAX;
	owner->ownerGeneration = ownerGeneration;
	memcpy( owner->slotGenerations, slotGenerations,
		sizeof( slotGenerations ) );
}
