// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef WIRED_RAL_BACKEND_CONFORMANCE_H
#define WIRED_RAL_BACKEND_CONFORMANCE_H

#include "ral_allocation.h"
#include "ral_capability.h"
#include "ral_command_lifecycle.h"
#include "ral_memory_recovery.h"
#include "ral_transfer.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RAL_BACKEND_CONFORMANCE_SCHEMA_VERSION 1u

// Backend adapters translate their native completed work into this one
// pointer-free authority.  Identity fields are opaque equality tokens and
// must never be dereferenced or compared across native handle kinds.
typedef struct {
	uint32_t schemaVersion;
	ralBackendType_t backendType;
	uint64_t backendGeneration;
	uintptr_t backendIdentity;
	uintptr_t deviceIdentity;
	uintptr_t graphicsQueueIdentity;
	ralCapabilityProfile_t capabilities;
	ralAllocationReceipt_t uploadAllocation;
	ralAllocationReceipt_t readbackAllocation;
	ralAllocationReceipt_t textureAllocation;
	uintptr_t samplerIdentity;
	uint64_t samplerGeneration;
	ralSubmissionReceipt_t submission;
	ralTransferReceipt_t transfer;
	uint64_t completionGeneration;
	uint64_t copiedByteCount;
	uint64_t copiedByteDigest;
	qboolean ready;
} ralBackendConformanceReceipt_t;

typedef struct {
	ralBackendType_t backendType;
	uint64_t backendGeneration;
	uintptr_t backendIdentity;
	uintptr_t deviceIdentity;
	uintptr_t graphicsQueueIdentity;
	const ralCapabilityProfile_t *capabilities;
	const ralAllocationReceipt_t *uploadAllocation;
	const ralAllocationReceipt_t *readbackAllocation;
	const ralAllocationReceipt_t *textureAllocation;
	uintptr_t samplerIdentity;
	uint64_t samplerGeneration;
	const ralSubmissionReceipt_t *submission;
	const ralTransferReceipt_t *transfer;
	uint64_t completionGeneration;
	uint64_t copiedByteCount;
	uint64_t copiedByteDigest;
} ralBackendConformanceFacts_t;

qboolean Ral_BackendConformanceBuild(
	const ralBackendConformanceFacts_t *facts,
	ralBackendConformanceReceipt_t *outReceipt );
qboolean Ral_BackendConformanceReceiptExact(
	const ralBackendConformanceReceipt_t *a,
	const ralBackendConformanceReceipt_t *b );
qboolean Ral_BackendConformanceCompatible(
	const ralBackendConformanceReceipt_t *a,
	const ralBackendConformanceReceipt_t *b );

typedef struct {
	uint32_t schemaVersion;
	ralBackendType_t backendType;
	ralBackendConformanceReceipt_t previous;
	ralMemoryFailureReceipt_t loss;
	ralBackendConformanceReceipt_t replacement;
	qboolean ready;
} ralBackendRecreateReceipt_t;

qboolean Ral_BackendRecreateBuild(
	const ralBackendConformanceReceipt_t *previous,
	const ralMemoryFailureReceipt_t *loss,
	const ralBackendConformanceReceipt_t *replacement,
	ralBackendRecreateReceipt_t *outReceipt );
qboolean Ral_BackendRecreateReceiptExact(
	const ralBackendRecreateReceipt_t *a,
	const ralBackendRecreateReceipt_t *b );

#ifdef __cplusplus
}
#endif

#endif
