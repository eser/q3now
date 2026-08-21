// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#ifndef WIRED_RAL_ALLOCATION_H
#define WIRED_RAL_ALLOCATION_H

#include "ral_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RAL_ALLOCATION_SCHEMA_VERSION 1u

typedef enum {
	RAL_ALLOCATION_DEVICE_LOCAL = 1,
	RAL_ALLOCATION_UPLOAD,
	RAL_ALLOCATION_READBACK,
	RAL_ALLOCATION_TRANSIENT
} ralAllocationClass_t;

typedef enum {
	RAL_ALLOCATION_RESIDENCY_PERMANENT = 1,
	RAL_ALLOCATION_RESIDENCY_STREAMED,
	RAL_ALLOCATION_RESIDENCY_TRANSIENT
} ralAllocationResidency_t;

typedef enum {
	RAL_ALLOCATION_PLACEMENT_DEDICATED = 1,
	RAL_ALLOCATION_PLACEMENT_SUBALLOCATED,
	RAL_ALLOCATION_PLACEMENT_MANAGED
} ralAllocationPlacement_t;

typedef enum {
	RAL_ALLOCATION_OUTCOME_NATIVE = 1,
	RAL_ALLOCATION_OUTCOME_EMULATED
} ralAllocationOutcome_t;

typedef enum {
	RAL_ALLOCATION_PRESSURE_UNKNOWN = 1,
	RAL_ALLOCATION_PRESSURE_NORMAL,
	RAL_ALLOCATION_PRESSURE_WARNING,
	RAL_ALLOCATION_PRESSURE_CRITICAL
} ralAllocationPressure_t;

typedef struct {
	ralAllocationClass_t memoryClass;
	ralAllocationResidency_t residency;
	uint64_t size;
	uint64_t alignment;
	uintptr_t ownerIdentity;
	uint64_t ownerGeneration;
	qboolean allowFallback;
} ralAllocationRequest_t;

typedef struct {
	ralBackendType_t backendType;
	ralAllocationPlacement_t placement;
	uint64_t committedSize;
	uint64_t actualAlignment;
	uint64_t allocationGeneration;
	qboolean deviceLocal;
	qboolean hostVisible;
	qboolean hostCoherent;
	qboolean lazy;
	qboolean budgetKnown;
	uint64_t budgetBytes;
	uint64_t usedBytesBefore;
} ralAllocationFacts_t;

typedef struct {
	uint32_t schemaVersion;
	ralBackendType_t backendType;
	ralAllocationClass_t memoryClass;
	ralAllocationResidency_t residency;
	ralAllocationPlacement_t placement;
	ralAllocationOutcome_t outcome;
	ralAllocationPressure_t pressureAfter;
	uint64_t requestedSize;
	uint64_t committedSize;
	uint64_t alignment;
	uintptr_t ownerIdentity;
	uint64_t ownerGeneration;
	uint64_t allocationGeneration;
	qboolean budgetKnown;
	uint64_t budgetBytes;
	uint64_t usedBytesBefore;
	uint64_t usedBytesAfter;
	qboolean ready;
} ralAllocationReceipt_t;

qboolean Ral_AllocationReceiptBuild( const ralAllocationRequest_t *request,
	const ralAllocationFacts_t *facts, ralAllocationReceipt_t *out );
qboolean Ral_AllocationReceiptExact( const ralAllocationReceipt_t *a,
	const ralAllocationReceipt_t *b );
qboolean Ral_BufferGetAllocationReceipt( const ralBuffer_t *buffer,
	ralAllocationReceipt_t *out );
qboolean Ral_TextureGetAllocationReceipt( const ralTexture_t *texture,
	ralAllocationReceipt_t *out );

#ifdef __cplusplus
}
#endif

#endif
