// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#include "ral_suballocation.h"

#include <string.h>

static qboolean PowerOfTwo( uint64_t value ) {
	return value && ( value & ( value - 1u ) ) == 0;
}

static qboolean AlignUp( uint64_t value, uint64_t alignment, uint64_t *out ) {
	if ( !out || !PowerOfTwo( alignment ) || value > UINT64_MAX - ( alignment - 1u ) )
		return qfalse;
	*out = ( value + alignment - 1u ) & ~( alignment - 1u );
	return qtrue;
}

static qboolean ReceiptValid( const ralSuballocationReceipt_t *receipt ) {
	uint64_t committed;
	return receipt && receipt->schemaVersion == RAL_SUBALLOCATION_SCHEMA_VERSION
		&& receipt->backendType >= RAL_BACKEND_VULKAN
		&& receipt->backendType <= RAL_BACKEND_WEBGL2
		&& receipt->placement == RAL_ALLOCATION_PLACEMENT_SUBALLOCATED
		&& receipt->blockIdentity && receipt->ownerIdentity
		&& receipt->blockIdentity != receipt->ownerIdentity
		&& receipt->blockGeneration && receipt->blockGeneration != UINT64_MAX
		&& receipt->blockBytes && receipt->ownerGeneration
		&& receipt->ownerGeneration != UINT64_MAX
		&& receipt->allocationGeneration
		&& receipt->allocationGeneration != UINT64_MAX
		&& receipt->requestedSize && PowerOfTwo( receipt->alignment )
		&& AlignUp( receipt->requestedSize, receipt->alignment, &committed )
		&& receipt->committedSize == committed
		&& ( receipt->offset & ( receipt->alignment - 1u ) ) == 0
		&& receipt->offset <= receipt->blockBytes
		&& receipt->committedSize <= receipt->blockBytes - receipt->offset
		&& receipt->ready == qtrue;
}

qboolean Ral_SuballocationReceiptExact( const ralSuballocationReceipt_t *a,
		const ralSuballocationReceipt_t *b ) {
	return ReceiptValid( a ) && ReceiptValid( b ) && memcmp( a, b, sizeof( *a ) ) == 0;
}

static qboolean AllocatorValid( const ralSuballocator_t *allocator ) {
	uint64_t total = 0;
	uint32_t i, j;
	if ( !allocator || allocator->ready != qtrue
			|| allocator->backendType < RAL_BACKEND_VULKAN
			|| allocator->backendType > RAL_BACKEND_WEBGL2
			|| allocator->backendType == RAL_BACKEND_WEBGPU
			|| !allocator->blockIdentity || !allocator->blockGeneration
			|| allocator->blockGeneration == UINT64_MAX || !allocator->blockBytes
			|| allocator->nextAllocationGeneration == UINT64_MAX
			|| allocator->freeRangeCount > RAL_SUBALLOCATION_MAX_SLICES + 1u
			|| allocator->liveCount > RAL_SUBALLOCATION_MAX_SLICES ) return qfalse;
	for ( i = 0; i < allocator->freeRangeCount; ++i ) {
		const ralSuballocationRange_t *range = &allocator->freeRanges[i];
		if ( !range->size || range->offset > allocator->blockBytes
				|| range->size > allocator->blockBytes - range->offset
				|| ( i && allocator->freeRanges[i-1].offset
					+ allocator->freeRanges[i-1].size >= range->offset )
				|| range->size > UINT64_MAX - total ) return qfalse;
		total += range->size;
	}
	for ( i = 0; i < allocator->liveCount; ++i ) {
		const ralSuballocationReceipt_t *receipt = &allocator->live[i];
		if ( !ReceiptValid( receipt ) || receipt->backendType != allocator->backendType
				|| receipt->blockIdentity != allocator->blockIdentity
				|| receipt->blockGeneration != allocator->blockGeneration
				|| receipt->blockBytes != allocator->blockBytes
				|| receipt->allocationGeneration > allocator->nextAllocationGeneration
				|| receipt->committedSize > UINT64_MAX - total ) return qfalse;
		total += receipt->committedSize;
		for ( j = 0; j < i; ++j ) {
			const ralSuballocationReceipt_t *other = &allocator->live[j];
			if ( receipt->ownerIdentity == other->ownerIdentity
					|| !( receipt->offset + receipt->committedSize <= other->offset
						|| other->offset + other->committedSize <= receipt->offset ) ) return qfalse;
		}
		for ( j = 0; j < allocator->freeRangeCount; ++j ) {
			const ralSuballocationRange_t *range = &allocator->freeRanges[j];
			if ( !( receipt->offset + receipt->committedSize <= range->offset
					|| range->offset + range->size <= receipt->offset ) ) return qfalse;
		}
	}
	return total == allocator->blockBytes;
}

qboolean Ral_SuballocatorInit( ralSuballocator_t *allocator,
		ralBackendType_t backendType, uintptr_t blockIdentity,
		uint64_t blockGeneration, uint64_t blockBytes ) {
	ralSuballocator_t candidate;
	if ( !allocator || backendType < RAL_BACKEND_VULKAN || backendType > RAL_BACKEND_WEBGL2
			|| backendType == RAL_BACKEND_WEBGPU
			|| !blockIdentity || !blockGeneration || blockGeneration == UINT64_MAX
			|| !blockBytes ) return qfalse;
	memset( &candidate, 0, sizeof( candidate ) );
	candidate.backendType = backendType;
	candidate.blockIdentity = blockIdentity;
	candidate.blockGeneration = blockGeneration;
	candidate.blockBytes = blockBytes;
	candidate.freeRanges[0].size = blockBytes;
	candidate.freeRangeCount = 1;
	candidate.ready = qtrue;
	if ( !AllocatorValid( &candidate ) ) return qfalse;
	*allocator = candidate;
	return qtrue;
}

qboolean Ral_SuballocatorAllocate( ralSuballocator_t *allocator,
		uintptr_t ownerIdentity, uint64_t ownerGeneration, uint64_t size,
		uint64_t alignment, ralSuballocationReceipt_t *outReceipt ) {
	ralSuballocator_t candidate;
	ralSuballocationReceipt_t receipt;
	uint64_t committed, offset, prefix, suffix;
	uint32_t i, j;
	if ( !outReceipt || !AllocatorValid( allocator ) || !ownerIdentity
			|| ownerIdentity == allocator->blockIdentity || !ownerGeneration
			|| ownerGeneration == UINT64_MAX || !size || !PowerOfTwo( alignment )
			|| allocator->liveCount >= RAL_SUBALLOCATION_MAX_SLICES
			|| allocator->nextAllocationGeneration >= UINT64_MAX - 1u
			|| !AlignUp( size, alignment, &committed ) ) return qfalse;
	for ( i = 0; i < allocator->liveCount; ++i )
		if ( allocator->live[i].ownerIdentity == ownerIdentity ) return qfalse;
	for ( i = 0; i < allocator->freeRangeCount; ++i ) {
		const ralSuballocationRange_t range = allocator->freeRanges[i];
		if ( !AlignUp( range.offset, alignment, &offset ) || offset < range.offset ) continue;
		prefix = offset - range.offset;
		if ( prefix > range.size || committed > range.size - prefix ) continue;
		suffix = range.size - prefix - committed;
		if ( prefix && suffix && allocator->freeRangeCount >= RAL_SUBALLOCATION_MAX_SLICES + 1u )
			return qfalse;
		candidate = *allocator;
		for ( j = i; j + 1u < candidate.freeRangeCount; ++j )
			candidate.freeRanges[j] = candidate.freeRanges[j+1u];
		candidate.freeRangeCount--;
		if ( suffix ) {
			for ( j = candidate.freeRangeCount; j > i; --j )
				candidate.freeRanges[j] = candidate.freeRanges[j-1u];
			candidate.freeRanges[i].offset = offset + committed;
			candidate.freeRanges[i].size = suffix;
			candidate.freeRangeCount++;
		}
		if ( prefix ) {
			for ( j = candidate.freeRangeCount; j > i; --j )
				candidate.freeRanges[j] = candidate.freeRanges[j-1u];
			candidate.freeRanges[i].offset = range.offset;
			candidate.freeRanges[i].size = prefix;
			candidate.freeRangeCount++;
		}
		memset( &receipt, 0, sizeof( receipt ) );
		receipt.schemaVersion = RAL_SUBALLOCATION_SCHEMA_VERSION;
		receipt.backendType = candidate.backendType;
		receipt.placement = RAL_ALLOCATION_PLACEMENT_SUBALLOCATED;
		receipt.blockIdentity = candidate.blockIdentity;
		receipt.blockGeneration = candidate.blockGeneration;
		receipt.blockBytes = candidate.blockBytes;
		receipt.ownerIdentity = ownerIdentity;
		receipt.ownerGeneration = ownerGeneration;
		receipt.allocationGeneration = candidate.nextAllocationGeneration + 1u;
		receipt.offset = offset;
		receipt.requestedSize = size;
		receipt.committedSize = committed;
		receipt.alignment = alignment;
		receipt.ready = qtrue;
		candidate.nextAllocationGeneration = receipt.allocationGeneration;
		candidate.live[candidate.liveCount++] = receipt;
		if ( !AllocatorValid( &candidate ) ) return qfalse;
		*allocator = candidate;
		*outReceipt = receipt;
		return qtrue;
	}
	return qfalse;
}

qboolean Ral_SuballocatorFree( ralSuballocator_t *allocator,
		const ralSuballocationReceipt_t *receipt ) {
	ralSuballocator_t candidate;
	uint32_t i, j, insert;
	if ( !AllocatorValid( allocator ) || !ReceiptValid( receipt )
			|| receipt->blockIdentity != allocator->blockIdentity
			|| receipt->blockGeneration != allocator->blockGeneration ) return qfalse;
	for ( i = 0; i < allocator->liveCount; ++i )
		if ( Ral_SuballocationReceiptExact( &allocator->live[i], receipt ) ) break;
	if ( i == allocator->liveCount ) return qfalse;
	candidate = *allocator;
	for ( j = i; j + 1u < candidate.liveCount; ++j ) candidate.live[j] = candidate.live[j+1u];
	candidate.liveCount--;
	insert = 0;
	while ( insert < candidate.freeRangeCount
			&& candidate.freeRanges[insert].offset < receipt->offset ) insert++;
	if ( insert > 0 && candidate.freeRanges[insert-1u].offset
			+ candidate.freeRanges[insert-1u].size == receipt->offset ) {
		candidate.freeRanges[insert-1u].size += receipt->committedSize;
		if ( insert < candidate.freeRangeCount
				&& receipt->offset + receipt->committedSize
					== candidate.freeRanges[insert].offset ) {
			candidate.freeRanges[insert-1u].size += candidate.freeRanges[insert].size;
			for ( j = insert; j + 1u < candidate.freeRangeCount; ++j )
				candidate.freeRanges[j] = candidate.freeRanges[j+1u];
			candidate.freeRangeCount--;
		}
	} else if ( insert < candidate.freeRangeCount
			&& receipt->offset + receipt->committedSize
				== candidate.freeRanges[insert].offset ) {
		candidate.freeRanges[insert].offset = receipt->offset;
		candidate.freeRanges[insert].size += receipt->committedSize;
	} else {
		if ( candidate.freeRangeCount >= RAL_SUBALLOCATION_MAX_SLICES + 1u ) return qfalse;
		for ( j = candidate.freeRangeCount; j > insert; --j )
			candidate.freeRanges[j] = candidate.freeRanges[j-1u];
		candidate.freeRanges[insert].offset = receipt->offset;
		candidate.freeRanges[insert].size = receipt->committedSize;
		candidate.freeRangeCount++;
	}
	for ( i = 0; i + 1u < candidate.freeRangeCount; ) {
		ralSuballocationRange_t *left = &candidate.freeRanges[i];
		ralSuballocationRange_t *right = &candidate.freeRanges[i+1u];
		if ( left->offset + left->size == right->offset ) {
			left->size += right->size;
			for ( j = i + 1u; j + 1u < candidate.freeRangeCount; ++j )
				candidate.freeRanges[j] = candidate.freeRanges[j+1u];
			candidate.freeRangeCount--;
		} else i++;
	}
	if ( !AllocatorValid( &candidate ) ) return qfalse;
	*allocator = candidate;
	return qtrue;
}

qboolean Ral_SuballocatorGetStats( const ralSuballocator_t *allocator,
		ralSuballocationStats_t *outStats ) {
	ralSuballocationStats_t stats;
	uint32_t i;
	if ( !outStats || !AllocatorValid( allocator ) ) return qfalse;
	memset( &stats, 0, sizeof( stats ) );
	for ( i = 0; i < allocator->freeRangeCount; ++i ) {
		stats.freeBytes += allocator->freeRanges[i].size;
		if ( allocator->freeRanges[i].size > stats.largestFreeRange )
			stats.largestFreeRange = allocator->freeRanges[i].size;
	}
	stats.freeRangeCount = allocator->freeRangeCount;
	stats.liveCount = allocator->liveCount;
	stats.empty = allocator->liveCount == 0 ? qtrue : qfalse;
	*outStats = stats;
	return qtrue;
}
