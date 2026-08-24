// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#ifndef WIRED_RAL_SUBALLOCATION_H
#define WIRED_RAL_SUBALLOCATION_H

#include "ral_allocation.h"

#define RAL_SUBALLOCATION_SCHEMA_VERSION 1u
#define RAL_SUBALLOCATION_MAX_SLICES 128u

typedef struct {
	uint64_t offset;
	uint64_t size;
} ralSuballocationRange_t;

typedef struct {
	uint32_t schemaVersion;
	ralBackendType_t backendType;
	ralAllocationPlacement_t placement;
	uintptr_t blockIdentity;
	uint64_t blockGeneration;
	uint64_t blockBytes;
	uintptr_t ownerIdentity;
	uint64_t ownerGeneration;
	uint64_t allocationGeneration;
	uint64_t offset;
	uint64_t requestedSize;
	uint64_t committedSize;
	uint64_t alignment;
	qboolean ready;
} ralSuballocationReceipt_t;

typedef struct {
	ralBackendType_t backendType;
	uintptr_t blockIdentity;
	uint64_t blockGeneration;
	uint64_t blockBytes;
	uint64_t nextAllocationGeneration;
	ralSuballocationRange_t freeRanges[RAL_SUBALLOCATION_MAX_SLICES + 1u];
	uint32_t freeRangeCount;
	ralSuballocationReceipt_t live[RAL_SUBALLOCATION_MAX_SLICES];
	uint32_t liveCount;
	qboolean ready;
} ralSuballocator_t;

typedef struct {
	uint64_t freeBytes;
	uint64_t largestFreeRange;
	uint32_t freeRangeCount;
	uint32_t liveCount;
	qboolean empty;
} ralSuballocationStats_t;

qboolean Ral_SuballocatorInit( ralSuballocator_t *allocator,
	ralBackendType_t backendType, uintptr_t blockIdentity,
	uint64_t blockGeneration, uint64_t blockBytes );
qboolean Ral_SuballocatorAllocate( ralSuballocator_t *allocator,
	uintptr_t ownerIdentity, uint64_t ownerGeneration, uint64_t size,
	uint64_t alignment, ralSuballocationReceipt_t *outReceipt );
qboolean Ral_SuballocatorFree( ralSuballocator_t *allocator,
	const ralSuballocationReceipt_t *receipt );
qboolean Ral_SuballocatorGetStats( const ralSuballocator_t *allocator,
	ralSuballocationStats_t *outStats );
qboolean Ral_SuballocationReceiptExact( const ralSuballocationReceipt_t *a,
	const ralSuballocationReceipt_t *b );
qboolean Ral_BufferGetSuballocationReceipt( const ralBuffer_t *buffer,
	ralSuballocationReceipt_t *outReceipt );
qboolean Ral_TextureGetSuballocationReceipt( const ralTexture_t *texture,
	ralSuballocationReceipt_t *outReceipt );

#endif
