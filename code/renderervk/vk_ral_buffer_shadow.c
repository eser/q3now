// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "vk_ral_buffer_shadow.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static qboolean UsageValid( ralBufferUsage_t usage ) {
	const uint32_t consumers = RAL_BUFFER_VERTEX | RAL_BUFFER_INDEX
		| RAL_BUFFER_UNIFORM | RAL_BUFFER_STORAGE | RAL_BUFFER_INDIRECT;
	const uint32_t forbidden = RAL_BUFFER_MAP_READ | RAL_BUFFER_MAP_WRITE;
	return ( (uint32_t)usage & consumers ) != 0u
		&& ( (uint32_t)usage & forbidden ) == 0u;
}

static qboolean AllocationValid( const ralAllocationReceipt_t *allocation,
		const ralBuffer_t *buffer, uint64_t byteSize ) {
	return allocation && buffer && byteSize > 0u
		&& Ral_AllocationReceiptExact( allocation, allocation )
		&& allocation->ready == qtrue
		&& allocation->memoryClass == RAL_ALLOCATION_UPLOAD
		&& allocation->residency == RAL_ALLOCATION_RESIDENCY_PERMANENT
		&& allocation->ownerIdentity == (uintptr_t)buffer
		&& allocation->requestedSize == byteSize
		&& allocation->committedSize >= byteSize;
}

static qboolean OwnerValid( const vkRalBufferShadow_t *shadow ) {
	return shadow
		&& shadow->schemaVersion == VK_RAL_BUFFER_SHADOW_RECEIPT_SCHEMA
		&& shadow->backend && shadow->buffer && shadow->bytes
		&& shadow->byteSize > 0u && shadow->generation > 0u
		&& shadow->generation < UINT64_MAX
		&& UsageValid( shadow->consumerUsage )
		&& AllocationValid( &shadow->allocation, shadow->buffer,
			shadow->byteSize )
		&& shadow->ready == qtrue;
}

static qboolean WriteReceiptValid(
		const vkRalBufferShadowWriteReceipt_t *receipt ) {
	const ralTransferRequest_t *request;
	if ( !receipt
			|| receipt->schemaVersion != VK_RAL_BUFFER_SHADOW_RECEIPT_SCHEMA
			|| !receipt->buffer || receipt->shadowGeneration == 0u
			|| receipt->shadowGeneration == UINT64_MAX
			|| receipt->size == 0u
			|| !AllocationValid( &receipt->allocation, receipt->buffer,
				receipt->allocation.requestedSize )
			|| !Ral_BufferUploadReceiptExact( &receipt->upload,
				&receipt->upload ) || receipt->ready != qtrue ) return qfalse;
	request = &receipt->upload.transfer.request;
	return request->direction == RAL_TRANSFER_UPLOAD
		&& request->resourceKind == RAL_TRANSFER_BUFFER
		&& request->resourceIdentity == (uintptr_t)receipt->buffer
		&& request->resourceGeneration
			== receipt->allocation.allocationGeneration
		&& request->byteOffset == receipt->offset
		&& request->byteSize == receipt->size
		&& request->byteBudget == receipt->allocation.committedSize
		&& receipt->upload.ready == qtrue;
}

void VK_RalBufferShadowInit( vkRalBufferShadow_t *shadow ) {
	if ( !shadow ) return;
	memset( shadow, 0, sizeof( *shadow ) );
	shadow->schemaVersion = VK_RAL_BUFFER_SHADOW_RECEIPT_SCHEMA;
}

qboolean VK_RalBufferShadowEnsure( vkRalBufferShadow_t *shadow,
		ralBackend_t *backend, uint64_t byteSize,
		ralBufferUsage_t consumerUsage, const char *debugName ) {
	vkRalBufferShadow_t candidate;
	ralBufferCreateInfo_t createInfo;
	if ( !shadow || shadow->schemaVersion
			!= VK_RAL_BUFFER_SHADOW_RECEIPT_SCHEMA || !backend
			|| byteSize == 0u || !UsageValid( consumerUsage )
			|| !debugName || !debugName[0] ) return qfalse;
	if ( shadow->ready == qtrue ) return OwnerValid( shadow )
		&& shadow->backend == backend && shadow->byteSize == byteSize
		&& shadow->consumerUsage == consumerUsage;
	if ( VK_RalBufferShadowHasLive( shadow )
			|| shadow->generation >= UINT64_MAX - 1u ) return qfalse;
	candidate = *shadow;
	memset( &createInfo, 0, sizeof( createInfo ) );
	createInfo.size = byteSize;
	createInfo.usage = (ralBufferUsage_t)( consumerUsage
		| RAL_BUFFER_TRANSFER_DST );
	createInfo.memory = RAL_MEMORY_HOST_COHERENT;
	createInfo.debugName = debugName;
	candidate.bytes = (unsigned char *)calloc( 1u, (size_t)byteSize );
	candidate.buffer = Ral_CreateBuffer( backend, &createInfo );
	if ( !candidate.bytes || !candidate.buffer
			|| !Ral_BufferGetAllocationReceipt( candidate.buffer,
				&candidate.allocation ) ) goto fail;
	candidate.backend = backend;
	candidate.byteSize = byteSize;
	candidate.consumerUsage = consumerUsage;
	candidate.generation++;
	candidate.ready = qtrue;
	if ( !OwnerValid( &candidate ) ) goto fail;
	*shadow = candidate;
	return qtrue;

fail:
	if ( candidate.buffer ) Ral_DestroyBuffer( candidate.buffer );
	free( candidate.bytes );
	return qfalse;
}

qboolean VK_RalBufferShadowWrite( vkRalBufferShadow_t *shadow,
		uint64_t offset, const void *data, uint64_t size ) {
	if ( !data || !VK_RalBufferShadowMarkWritten( shadow, offset, size ) )
		return qfalse;
	memcpy( shadow->bytes + offset, data, (size_t)size );
	return qtrue;
}

qboolean VK_RalBufferShadowMarkWritten( vkRalBufferShadow_t *shadow,
		uint64_t offset, uint64_t size ) {
	if ( !OwnerValid( shadow ) || size == 0u
			|| offset > shadow->byteSize
			|| size > shadow->byteSize - offset
			|| shadow->generation >= UINT64_MAX - 1u ) return qfalse;
	shadow->generation++;
	return qtrue;
}

qboolean VK_RalBufferShadowPublish( vkRalBufferShadow_t *shadow,
		uint64_t offset, uint64_t size,
		vkRalBufferShadowWriteReceipt_t *outReceipt ) {
	vkRalBufferShadowWriteReceipt_t candidate;
	if ( !OwnerValid( shadow ) || !outReceipt || size == 0u
			|| offset > shadow->byteSize
			|| size > shadow->byteSize - offset ) return qfalse;
	memset( &candidate, 0, sizeof( candidate ) );
	if ( !Ral_BufferWriteImmediate( shadow->buffer, offset,
			shadow->bytes + offset, size, &candidate.upload ) ) return qfalse;
	candidate.schemaVersion = VK_RAL_BUFFER_SHADOW_RECEIPT_SCHEMA;
	candidate.buffer = shadow->buffer;
	candidate.allocation = shadow->allocation;
	candidate.shadowGeneration = shadow->generation;
	candidate.offset = offset;
	candidate.size = size;
	candidate.ready = qtrue;
	if ( !WriteReceiptValid( &candidate ) ) return qfalse;
	*outReceipt = candidate;
	return qtrue;
}

qboolean VK_RalBufferShadowWriteReceiptExact(
		const vkRalBufferShadowWriteReceipt_t *a,
		const vkRalBufferShadowWriteReceipt_t *b ) {
	return WriteReceiptValid( a ) && WriteReceiptValid( b )
		&& a->schemaVersion == b->schemaVersion
		&& a->buffer == b->buffer
		&& Ral_AllocationReceiptExact( &a->allocation, &b->allocation )
		&& Ral_BufferUploadReceiptExact( &a->upload, &b->upload )
		&& a->shadowGeneration == b->shadowGeneration
		&& a->offset == b->offset && a->size == b->size
		&& a->ready == b->ready;
}

qboolean VK_RalBufferShadowHasLive( const vkRalBufferShadow_t *shadow ) {
	return shadow && ( shadow->backend || shadow->buffer || shadow->bytes
		|| shadow->allocation.ready || shadow->byteSize || shadow->ready );
}

void VK_RalBufferShadowRelease( vkRalBufferShadow_t *shadow ) {
	uint64_t nextGeneration;
	if ( !shadow ) return;
	nextGeneration = shadow->generation < UINT64_MAX
		? shadow->generation + 1u : UINT64_MAX;
	if ( shadow->buffer ) Ral_DestroyBuffer( shadow->buffer );
	free( shadow->bytes );
	VK_RalBufferShadowInit( shadow );
	shadow->generation = nextGeneration;
}
