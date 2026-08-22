// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "vk_ral_shadow_storage.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static qboolean BoolValid( qboolean value ) {
	return value == qfalse || value == qtrue;
}

static uint64_t HashFold( uint64_t hash, const void *data, size_t size ) {
	const unsigned char *bytes = (const unsigned char *)data;
	size_t i;
	if ( !data || size == 0u ) return hash;
	for ( i = 0u; i < size; i++ ) {
		hash ^= bytes[i];
		hash *= UINT64_C( 1099511628211 );
	}
	return hash;
}

static qboolean ConfigValid( const vkRalShadowStorageConfig_t *config,
		qboolean requireDebugName, uint64_t *outByteSize ) {
	uint64_t byteSize;
	if ( !config || config->bufferCount == 0u
			|| config->bufferCount > VK_RAL_SHADOW_STORAGE_MAX_BUFFERS
			|| config->elementSize == 0u || config->elementCount == 0u
			|| config->elementCount > VK_RAL_SHADOW_STORAGE_MAX_ELEMENTS )
		return qfalse;
	if ( requireDebugName == qtrue
			&& ( !config->debugName || !config->debugName[0] ) ) return qfalse;
	byteSize = (uint64_t)config->elementSize * config->elementCount;
	if ( byteSize == 0u || byteSize > VK_RAL_SHADOW_STORAGE_MAX_BYTES )
		return qfalse;
	if ( outByteSize ) *outByteSize = byteSize;
	return qtrue;
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

static qboolean UploadValid( const ralBufferUploadReceipt_t *upload,
		const ralBuffer_t *buffer,
		const ralAllocationReceipt_t *allocation,
		uint64_t offset, uint64_t size ) {
	const ralTransferRequest_t *request;
	if ( !upload || !buffer || !allocation || size == 0u
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

static qboolean OwnerValid( const vkRalShadowStorageOwner_t *owner ) {
	vkRalShadowStorageConfig_t config;
	uint64_t byteSize;
	uint32_t i, j;
	if ( !owner || owner->initialized != qtrue || owner->ready != qtrue
			|| !owner->backend || owner->ownerGeneration == 0u
			|| owner->ownerGeneration == UINT64_MAX ) return qfalse;
	memset( &config, 0, sizeof( config ) );
	config.bufferCount = owner->bufferCount;
	config.elementSize = owner->elementSize;
	config.elementCount = owner->elementCount;
	if ( !ConfigValid( &config, qfalse, &byteSize )
			|| byteSize != owner->byteSize ) return qfalse;
	for ( i = 0u; i < VK_RAL_SHADOW_STORAGE_MAX_BUFFERS; i++ ) {
		if ( i >= owner->bufferCount ) {
			if ( owner->buffers[i] || owner->shadows[i] || owner->dirty[i]
					|| owner->allocations[i].ready != qfalse ) return qfalse;
			continue;
		}
		if ( !owner->buffers[i] || !owner->shadows[i] || !owner->dirty[i]
				|| owner->shadowGenerations[i] == 0u
				|| owner->shadowGenerations[i] == UINT64_MAX
				|| !AllocationValid( &owner->allocations[i], owner->buffers[i],
					owner->byteSize ) ) return qfalse;
		for ( j = i + 1u; j < owner->bufferCount; j++ ) {
			if ( owner->buffers[i] == owner->buffers[j]
					|| owner->shadows[i] == owner->shadows[j]
					|| owner->dirty[i] == owner->dirty[j] ) return qfalse;
		}
	}
	return qtrue;
}

static qboolean ResourceReceiptValid(
		const vkRalShadowStorageResourcesReceipt_t *receipt ) {
	vkRalShadowStorageConfig_t config;
	uint64_t byteSize;
	uint32_t i, j;
	if ( !receipt
			|| receipt->schemaVersion != VK_RAL_SHADOW_STORAGE_RECEIPT_SCHEMA
			|| !receipt->backend || receipt->ownerGeneration == 0u
			|| receipt->ownerGeneration == UINT64_MAX
			|| receipt->ready != qtrue ) return qfalse;
	memset( &config, 0, sizeof( config ) );
	config.bufferCount = receipt->bufferCount;
	config.elementSize = receipt->elementSize;
	config.elementCount = receipt->elementCount;
	if ( !ConfigValid( &config, qfalse, &byteSize )
			|| byteSize != receipt->byteSize ) return qfalse;
	for ( i = 0u; i < VK_RAL_SHADOW_STORAGE_MAX_BUFFERS; i++ ) {
		if ( i >= receipt->bufferCount ) {
			if ( receipt->buffers[i] || receipt->allocations[i].ready != qfalse )
				return qfalse;
			continue;
		}
		if ( !AllocationValid( &receipt->allocations[i], receipt->buffers[i],
				receipt->byteSize ) ) return qfalse;
		for ( j = i + 1u; j < receipt->bufferCount; j++ )
			if ( receipt->buffers[i] == receipt->buffers[j] ) return qfalse;
	}
	return qtrue;
}

static qboolean FlushReceiptValid(
		const vkRalShadowStorageFlushReceipt_t *receipt ) {
	uint64_t firstOffset, firstSize, lastOffset, lastSize;
	if ( !receipt
			|| receipt->schemaVersion != VK_RAL_SHADOW_STORAGE_RECEIPT_SCHEMA
			|| !receipt->backend || !receipt->buffer
			|| receipt->ownerGeneration == 0u
			|| receipt->ownerGeneration == UINT64_MAX
			|| receipt->shadowGeneration == 0u
			|| receipt->shadowGeneration == UINT64_MAX
			|| receipt->flushGeneration == 0u
			|| receipt->flushGeneration == UINT64_MAX
			|| receipt->byteSize == 0u
			|| receipt->bufferIndex >= VK_RAL_SHADOW_STORAGE_MAX_BUFFERS
			|| receipt->elementSize == 0u || receipt->elementCount == 0u
			|| (uint64_t)receipt->elementSize * receipt->elementCount
				!= receipt->byteSize
			|| receipt->dirtyElementCount > receipt->elementCount
			|| !BoolValid( receipt->wrote ) || receipt->ready != qtrue
			|| !AllocationValid( &receipt->allocation, receipt->buffer,
				receipt->byteSize ) ) return qfalse;
	if ( receipt->wrote == qfalse ) {
		return receipt->dirtyElementCount == 0u
			&& receipt->writeCount == 0u
			&& receipt->firstDirtyElement == UINT32_MAX
			&& receipt->lastDirtyElement == UINT32_MAX
			&& receipt->dirtyContentHash == 0u
			&& receipt->writeDigest == 0u
			&& UploadEmpty( &receipt->firstWrite )
			&& UploadEmpty( &receipt->lastWrite );
	}
	if ( receipt->dirtyElementCount == 0u || receipt->writeCount == 0u
			|| receipt->writeCount > receipt->dirtyElementCount
			|| receipt->firstDirtyElement >= receipt->elementCount
			|| receipt->lastDirtyElement < receipt->firstDirtyElement
			|| receipt->lastDirtyElement >= receipt->elementCount
			|| receipt->dirtyContentHash == 0u
			|| receipt->writeDigest == 0u ) return qfalse;
	firstOffset = (uint64_t)receipt->firstDirtyElement * receipt->elementSize;
	firstSize = receipt->firstWrite.transfer.request.byteSize;
	lastOffset = receipt->lastWrite.transfer.request.byteOffset;
	lastSize = receipt->lastWrite.transfer.request.byteSize;
	return UploadValid( &receipt->firstWrite, receipt->buffer,
			&receipt->allocation, firstOffset, firstSize )
		&& UploadValid( &receipt->lastWrite, receipt->buffer,
			&receipt->allocation, lastOffset, lastSize )
		&& firstSize % receipt->elementSize == 0u
		&& lastOffset % receipt->elementSize == 0u
		&& lastSize % receipt->elementSize == 0u
		&& lastOffset + lastSize
			== (uint64_t)( receipt->lastDirtyElement + 1u )
				* receipt->elementSize;
}

static void DestroyCandidate( vkRalShadowStorageOwner_t *candidate ) {
	uint32_t i, j;
	if ( !candidate ) return;
	for ( i = candidate->bufferCount; i > 0u; i-- ) {
		uint32_t index = i - 1u;
		qboolean duplicate = qfalse;
		for ( j = 0u; j < index; j++ )
			if ( candidate->buffers[index] == candidate->buffers[j] )
				duplicate = qtrue;
		if ( candidate->buffers[index] && duplicate == qfalse )
			Ral_DestroyBuffer( candidate->buffers[index] );
		free( candidate->dirty[index] );
		free( candidate->shadows[index] );
		candidate->buffers[index] = NULL;
		candidate->dirty[index] = NULL;
		candidate->shadows[index] = NULL;
	}
}

static void BuildResources( const vkRalShadowStorageOwner_t *owner,
		vkRalShadowStorageResourcesReceipt_t *outReceipt ) {
	vkRalShadowStorageResourcesReceipt_t candidate;
	memset( &candidate, 0, sizeof( candidate ) );
	candidate.schemaVersion = VK_RAL_SHADOW_STORAGE_RECEIPT_SCHEMA;
	candidate.backend = owner->backend;
	memcpy( candidate.buffers, owner->buffers, sizeof( candidate.buffers ) );
	memcpy( candidate.allocations, owner->allocations,
		sizeof( candidate.allocations ) );
	candidate.ownerGeneration = owner->ownerGeneration;
	candidate.byteSize = owner->byteSize;
	candidate.bufferCount = owner->bufferCount;
	candidate.elementSize = owner->elementSize;
	candidate.elementCount = owner->elementCount;
	candidate.ready = qtrue;
	*outReceipt = candidate;
}

void VK_RalShadowStorageInit( vkRalShadowStorageOwner_t *owner ) {
	if ( !owner ) return;
	memset( owner, 0, sizeof( *owner ) );
	owner->initialized = qtrue;
}

qboolean VK_RalShadowStorageEnsure( vkRalShadowStorageOwner_t *owner,
		ralBackend_t *backend, const vkRalShadowStorageConfig_t *config ) {
	vkRalShadowStorageOwner_t candidate;
	ralBufferCreateInfo_t createInfo;
	uint64_t byteSize;
	uint32_t i;
	if ( !owner || owner->initialized != qtrue || !backend
			|| !ConfigValid( config, qtrue, &byteSize ) ) return qfalse;
	if ( owner->ready == qtrue ) return OwnerValid( owner )
		&& owner->backend == backend && owner->byteSize == byteSize
		&& owner->bufferCount == config->bufferCount
		&& owner->elementSize == config->elementSize
		&& owner->elementCount == config->elementCount;
	if ( VK_RalShadowStorageHasLive( owner )
			|| owner->ownerGeneration >= UINT64_MAX - 1u ) return qfalse;
	for ( i = 0u; i < config->bufferCount; i++ )
		if ( owner->shadowGenerations[i] >= UINT64_MAX - 1u ) return qfalse;
	candidate = *owner;
	candidate.backend = backend;
	candidate.byteSize = byteSize;
	candidate.bufferCount = config->bufferCount;
	candidate.elementSize = config->elementSize;
	candidate.elementCount = config->elementCount;
	memset( &createInfo, 0, sizeof( createInfo ) );
	createInfo.size = byteSize;
	createInfo.usage = RAL_BUFFER_STORAGE | RAL_BUFFER_TRANSFER_DST;
	createInfo.memory = RAL_MEMORY_HOST_COHERENT;
	createInfo.debugName = config->debugName;
	for ( i = 0u; i < config->bufferCount; i++ ) {
		candidate.shadows[i] = (unsigned char *)calloc( 1u, (size_t)byteSize );
		candidate.dirty[i] = (unsigned char *)malloc( config->elementCount );
		if ( !candidate.shadows[i] || !candidate.dirty[i] ) goto fail;
		memset( candidate.dirty[i], 1, config->elementCount );
		candidate.buffers[i] = Ral_CreateBuffer( backend, &createInfo );
		if ( !candidate.buffers[i]
				|| ( i > 0u && candidate.buffers[i] == candidate.buffers[0] )
				|| !Ral_BufferGetAllocationReceipt( candidate.buffers[i],
					&candidate.allocations[i] )
				|| !AllocationValid( &candidate.allocations[i],
					candidate.buffers[i], byteSize ) ) goto fail;
		candidate.shadowGenerations[i]++;
	}
	candidate.ownerGeneration++;
	candidate.ready = qtrue;
	if ( !OwnerValid( &candidate ) ) goto fail;
	*owner = candidate;
	return qtrue;

fail:
	DestroyCandidate( &candidate );
	return qfalse;
}

qboolean VK_RalShadowStorageGetResources(
		const vkRalShadowStorageOwner_t *owner,
		vkRalShadowStorageResourcesReceipt_t *outReceipt ) {
	vkRalShadowStorageResourcesReceipt_t candidate;
	if ( !OwnerValid( owner ) || !outReceipt ) return qfalse;
	BuildResources( owner, &candidate );
	if ( !ResourceReceiptValid( &candidate ) ) return qfalse;
	*outReceipt = candidate;
	return qtrue;
}

qboolean VK_RalShadowStorageResourcesReceiptExact(
		const vkRalShadowStorageResourcesReceipt_t *a,
		const vkRalShadowStorageResourcesReceipt_t *b ) {
	uint32_t i;
	if ( !ResourceReceiptValid( a ) || !ResourceReceiptValid( b )
			|| a->schemaVersion != b->schemaVersion
			|| a->backend != b->backend
			|| a->ownerGeneration != b->ownerGeneration
			|| a->byteSize != b->byteSize
			|| a->bufferCount != b->bufferCount
			|| a->elementSize != b->elementSize
			|| a->elementCount != b->elementCount
			|| a->ready != b->ready ) return qfalse;
	for ( i = 0u; i < a->bufferCount; i++ ) {
		if ( a->buffers[i] != b->buffers[i]
				|| !Ral_AllocationReceiptExact( &a->allocations[i],
					&b->allocations[i] ) ) return qfalse;
	}
	return qtrue;
}

qboolean VK_RalShadowStorageWriteElement( vkRalShadowStorageOwner_t *owner,
		uint32_t bufferIndex, uint32_t elementIndex, const void *data,
		uint32_t dataSize ) {
	uint64_t offset;
	if ( !OwnerValid( owner ) || !data
			|| bufferIndex >= owner->bufferCount
			|| elementIndex >= owner->elementCount
			|| dataSize != owner->elementSize
			|| owner->shadowGenerations[bufferIndex] >= UINT64_MAX - 1u )
		return qfalse;
	offset = (uint64_t)elementIndex * owner->elementSize;
	memcpy( owner->shadows[bufferIndex] + offset, data, dataSize );
	owner->dirty[bufferIndex][elementIndex] = 1u;
	owner->shadowGenerations[bufferIndex]++;
	memset( &owner->flushReceipts[bufferIndex], 0,
		sizeof( owner->flushReceipts[bufferIndex] ) );
	return qtrue;
}

qboolean VK_RalShadowStorageReadElement(
		const vkRalShadowStorageOwner_t *owner, uint32_t bufferIndex,
		uint32_t elementIndex, const void **outData ) {
	const void *candidate;
	if ( !OwnerValid( owner ) || !outData
			|| bufferIndex >= owner->bufferCount
			|| elementIndex >= owner->elementCount ) return qfalse;
	candidate = owner->shadows[bufferIndex]
		+ (uint64_t)elementIndex * owner->elementSize;
	*outData = candidate;
	return qtrue;
}

qboolean VK_RalShadowStorageHasDirty(
		const vkRalShadowStorageOwner_t *owner, uint32_t bufferIndex ) {
	uint32_t i;
	if ( !OwnerValid( owner ) || bufferIndex >= owner->bufferCount )
		return qfalse;
	for ( i = 0u; i < owner->elementCount; i++ )
		if ( owner->dirty[bufferIndex][i] ) return qtrue;
	return qfalse;
}

qboolean VK_RalShadowStorageFlush( vkRalShadowStorageOwner_t *owner,
		uint32_t bufferIndex, vkRalShadowStorageFlushReceipt_t *outReceipt ) {
	vkRalShadowStorageFlushReceipt_t candidate;
	ralBufferUploadReceipt_t upload;
	uint64_t dirtyHash = UINT64_C( 1469598103934665603 );
	uint64_t writeDigest = UINT64_C( 1469598103934665603 );
	uint32_t dirtyCount = 0u, writeCount = 0u;
	uint32_t firstDirty = UINT32_MAX, lastDirty = UINT32_MAX, i;
	if ( !OwnerValid( owner ) || !outReceipt
			|| bufferIndex >= owner->bufferCount
			|| owner->flushGenerations[bufferIndex] >= UINT64_MAX - 1u )
		return qfalse;
	memset( &candidate, 0, sizeof( candidate ) );
	for ( i = 0u; i < owner->elementCount; i++ ) {
		uint32_t runStart, runEnd, j;
		uint64_t offset, size;
		if ( !owner->dirty[bufferIndex][i] ) continue;
		runStart = i;
		runEnd = i + 1u;
		while ( runEnd < owner->elementCount
				&& owner->dirty[bufferIndex][runEnd] ) runEnd++;
		if ( firstDirty == UINT32_MAX ) firstDirty = runStart;
		lastDirty = runEnd - 1u;
		for ( j = runStart; j < runEnd; j++ ) {
			uint64_t elementOffset = (uint64_t)j * owner->elementSize;
			dirtyHash = HashFold( dirtyHash, &j, sizeof( j ) );
			dirtyHash = HashFold( dirtyHash,
				owner->shadows[bufferIndex] + elementOffset,
				owner->elementSize );
			dirtyCount++;
		}
		offset = (uint64_t)runStart * owner->elementSize;
		size = (uint64_t)( runEnd - runStart ) * owner->elementSize;
		if ( !Ral_BufferWriteImmediate( owner->buffers[bufferIndex], offset,
				owner->shadows[bufferIndex] + offset, size, &upload )
				|| !UploadValid( &upload, owner->buffers[bufferIndex],
					&owner->allocations[bufferIndex], offset, size ) ) return qfalse;
		if ( writeCount == 0u ) candidate.firstWrite = upload;
		candidate.lastWrite = upload;
		writeDigest = HashFold( writeDigest, &runStart, sizeof( runStart ) );
		writeDigest = HashFold( writeDigest, &runEnd, sizeof( runEnd ) );
		writeDigest = HashFold( writeDigest, &upload, sizeof( upload ) );
		writeCount++;
		i = runEnd - 1u;
	}
	candidate.schemaVersion = VK_RAL_SHADOW_STORAGE_RECEIPT_SCHEMA;
	candidate.backend = owner->backend;
	candidate.buffer = owner->buffers[bufferIndex];
	candidate.allocation = owner->allocations[bufferIndex];
	candidate.ownerGeneration = owner->ownerGeneration;
	candidate.shadowGeneration = owner->shadowGenerations[bufferIndex];
	candidate.flushGeneration = owner->flushGenerations[bufferIndex] + 1u;
	candidate.byteSize = owner->byteSize;
	candidate.bufferIndex = bufferIndex;
	candidate.elementSize = owner->elementSize;
	candidate.elementCount = owner->elementCount;
	candidate.dirtyElementCount = dirtyCount;
	candidate.writeCount = writeCount;
	candidate.firstDirtyElement = firstDirty;
	candidate.lastDirtyElement = lastDirty;
	candidate.wrote = writeCount ? qtrue : qfalse;
	candidate.ready = qtrue;
	if ( writeCount ) {
		candidate.dirtyContentHash = dirtyHash ? dirtyHash : 1u;
		candidate.writeDigest = writeDigest ? writeDigest : 1u;
	}
	if ( !FlushReceiptValid( &candidate ) ) return qfalse;
	if ( writeCount ) memset( owner->dirty[bufferIndex], 0, owner->elementCount );
	owner->flushGenerations[bufferIndex] = candidate.flushGeneration;
	owner->flushReceipts[bufferIndex] = candidate;
	*outReceipt = candidate;
	return qtrue;
}

qboolean VK_RalShadowStorageGetFlushReceipt(
		const vkRalShadowStorageOwner_t *owner, uint32_t bufferIndex,
		vkRalShadowStorageFlushReceipt_t *outReceipt ) {
	vkRalShadowStorageFlushReceipt_t candidate;
	if ( !OwnerValid( owner ) || !outReceipt
			|| bufferIndex >= owner->bufferCount
			|| VK_RalShadowStorageHasDirty( owner, bufferIndex ) ) return qfalse;
	candidate = owner->flushReceipts[bufferIndex];
	if ( !FlushReceiptValid( &candidate )
			|| candidate.backend != owner->backend
			|| candidate.buffer != owner->buffers[bufferIndex]
			|| candidate.ownerGeneration != owner->ownerGeneration
			|| candidate.shadowGeneration
				!= owner->shadowGenerations[bufferIndex]
			|| candidate.flushGeneration != owner->flushGenerations[bufferIndex]
			|| !Ral_AllocationReceiptExact( &candidate.allocation,
				&owner->allocations[bufferIndex] ) ) return qfalse;
	*outReceipt = candidate;
	return qtrue;
}

qboolean VK_RalShadowStorageFlushReceiptExact(
		const vkRalShadowStorageFlushReceipt_t *a,
		const vkRalShadowStorageFlushReceipt_t *b ) {
	return FlushReceiptValid( a ) && FlushReceiptValid( b )
		&& !memcmp( a, b, sizeof( *a ) );
}

qboolean VK_RalShadowStorageHasLive(
		const vkRalShadowStorageOwner_t *owner ) {
	uint32_t i;
	if ( !owner ) return qfalse;
	if ( owner->backend || owner->byteSize || owner->bufferCount
			|| owner->elementSize || owner->elementCount || owner->ready )
		return qtrue;
	for ( i = 0u; i < VK_RAL_SHADOW_STORAGE_MAX_BUFFERS; i++ )
		if ( owner->buffers[i] || owner->shadows[i] || owner->dirty[i] )
			return qtrue;
	return qfalse;
}

void VK_RalShadowStorageRelease( vkRalShadowStorageOwner_t *owner ) {
	uint64_t ownerGeneration;
	uint64_t shadowGenerations[VK_RAL_SHADOW_STORAGE_MAX_BUFFERS];
	uint64_t flushGenerations[VK_RAL_SHADOW_STORAGE_MAX_BUFFERS];
	if ( !owner || owner->initialized != qtrue ) return;
	ownerGeneration = owner->ownerGeneration;
	memcpy( shadowGenerations, owner->shadowGenerations,
		sizeof( shadowGenerations ) );
	memcpy( flushGenerations, owner->flushGenerations,
		sizeof( flushGenerations ) );
	DestroyCandidate( owner );
	memset( owner, 0, sizeof( *owner ) );
	owner->initialized = qtrue;
	owner->ownerGeneration = ownerGeneration;
	memcpy( owner->shadowGenerations, shadowGenerations,
		sizeof( shadowGenerations ) );
	memcpy( owner->flushGenerations, flushGenerations,
		sizeof( flushGenerations ) );
}
