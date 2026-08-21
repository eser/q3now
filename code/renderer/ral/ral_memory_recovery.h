// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#ifndef WIRED_RAL_MEMORY_RECOVERY_H
#define WIRED_RAL_MEMORY_RECOVERY_H

#include "ral_allocation.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RAL_MEMORY_FAILURE_SCHEMA_VERSION 1u

typedef enum {
	RAL_MEMORY_FAILURE_NO_COMPATIBLE_TYPE = 1,
	RAL_MEMORY_FAILURE_HOST_OOM,
	RAL_MEMORY_FAILURE_DEVICE_OOM,
	RAL_MEMORY_FAILURE_FRAGMENTED,
	RAL_MEMORY_FAILURE_PRESSURE,
	RAL_MEMORY_FAILURE_RESIZE,
	RAL_MEMORY_FAILURE_DEVICE_LOST
} ralMemoryFailureCause_t;

typedef enum {
	RAL_MEMORY_CRITICALITY_OPTIONAL = 1,
	RAL_MEMORY_CRITICALITY_STREAMING,
	RAL_MEMORY_CRITICALITY_REQUIRED
} ralMemoryCriticality_t;

typedef enum {
	RAL_MEMORY_RECOVERY_RETRY_AFTER_DRAIN = 1,
	RAL_MEMORY_RECOVERY_EVICT_AND_RETRY,
	RAL_MEMORY_RECOVERY_REDUCE_QUALITY,
	RAL_MEMORY_RECOVERY_DEFER,
	RAL_MEMORY_RECOVERY_RECREATE_BACKEND,
	RAL_MEMORY_RECOVERY_FAIL
} ralMemoryRecoveryAction_t;

typedef struct {
	ralBackendType_t backendType;
	ralMemoryFailureCause_t cause;
	ralAllocationClass_t memoryClass;
	ralAllocationResidency_t residency;
	ralMemoryCriticality_t criticality;
	uint64_t requestedBytes;
	qboolean budgetKnown;
	uint64_t budgetBytes;
	uint64_t usedBytes;
	uint64_t reclaimableBytes;
	uint32_t attempt;
	uint32_t maxAttempts;
	qboolean liveParent;
} ralMemoryFailureEvent_t;

typedef struct {
	uint32_t schemaVersion;
	ralMemoryFailureEvent_t event;
	ralMemoryRecoveryAction_t action;
	uint64_t failureGeneration;
	qboolean retryAllowed;
	qboolean preserveLiveParent;
	qboolean ready;
} ralMemoryFailureReceipt_t;

qboolean Ral_MemoryFailureDecide( const ralMemoryFailureEvent_t *event,
	uint64_t failureGeneration, ralMemoryFailureReceipt_t *outReceipt );
qboolean Ral_MemoryFailureReceiptExact( const ralMemoryFailureReceipt_t *a,
	const ralMemoryFailureReceipt_t *b );

typedef struct {
	uint64_t nextGeneration;
	ralMemoryFailureReceipt_t last;
	qboolean ready;
} ralMemoryFailureLedger_t;

void Ral_MemoryFailureLedgerInit( ralMemoryFailureLedger_t *ledger );
qboolean Ral_MemoryFailureLedgerPublish( ralMemoryFailureLedger_t *ledger,
	const ralMemoryFailureEvent_t *event );
qboolean Ral_MemoryFailureLedgerGet( const ralMemoryFailureLedger_t *ledger,
	ralMemoryFailureReceipt_t *outReceipt );
qboolean Ral_GetLastMemoryFailure( const ralBackend_t *backend,
	ralMemoryFailureReceipt_t *outReceipt );

#ifdef __cplusplus
}
#endif

#endif
