// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#ifndef WIRED_RAL_TRANSIENT_H
#define WIRED_RAL_TRANSIENT_H

#include "ral_types.h"

#define RAL_TRANSIENT_SCHEMA_VERSION 1u
#define RAL_TRANSIENT_MAX_REQUESTS 64u
#define RAL_TRANSIENT_MAX_COMMAND_SLOTS 8u

typedef enum {
	RAL_TRANSIENT_PLAN_DISJOINT = 1,
	RAL_TRANSIENT_PLAN_EXPLICIT_ALIAS,
	RAL_TRANSIENT_PLAN_MANAGED_DISJOINT
} ralTransientPlanOutcome_t;

typedef struct {
	ralBackendType_t backendType;
	uint64_t generation;
	uint64_t budgetBytes;
	qboolean explicitAliasing;
} ralTransientPolicy_t;

typedef struct {
	uintptr_t resourceIdentity;
	uint64_t resourceGeneration;
	uint64_t compatibilityKey;
	uint64_t size;
	uint64_t alignment;
	uint32_t firstPass;
	uint32_t lastPass;
	qboolean allowAlias;
} ralTransientRequest_t;

typedef struct {
	ralTransientRequest_t request;
	uint32_t slotIndex;
	uint64_t committedSize;
} ralTransientAssignment_t;

typedef struct {
	uint64_t compatibilityKey;
	uint64_t committedSize;
	uint64_t alignment;
	uint32_t lastPass;
	qboolean aliasable;
} ralTransientSlot_t;

typedef struct {
	uint32_t schemaVersion;
	ralTransientPolicy_t policy;
	ralTransientPlanOutcome_t outcome;
	ralTransientAssignment_t assignments[RAL_TRANSIENT_MAX_REQUESTS];
	ralTransientSlot_t slots[RAL_TRANSIENT_MAX_REQUESTS];
	uint32_t requestCount;
	uint32_t slotCount;
	uint32_t aliasCount;
	uint64_t requestedBytes;
	uint64_t peakBytes;
	qboolean ready;
} ralTransientPlan_t;

typedef enum {
	RAL_TRANSIENT_BATCH_EMPTY = 0,
	RAL_TRANSIENT_BATCH_PLANNED,
	RAL_TRANSIENT_BATCH_SUBMITTED,
	RAL_TRANSIENT_BATCH_RETIRED,
	RAL_TRANSIENT_BATCH_CANCELED
} ralTransientBatchState_t;

typedef struct {
	uint32_t schemaVersion;
	ralTransientBatchState_t state;
	const ralTransientPlan_t *planIdentity;
	uint64_t planGeneration;
	uint64_t batchGeneration;
	uint64_t submissionGeneration;
	uint32_t commandSlot;
	uint32_t requestCount;
	uint64_t peakBytes;
	qboolean ready;
} ralTransientBatchReceipt_t;

typedef struct {
	uint64_t nextBatchGeneration;
	ralTransientPlan_t boundPlan;
	ralTransientBatchReceipt_t active;
} ralTransientBatchLifecycle_t;

qboolean Ral_TransientPlanBuild( const ralTransientPolicy_t *policy,
	const ralTransientRequest_t *requests, uint32_t requestCount,
	ralTransientPlan_t *out );
qboolean Ral_TransientPlanExact( const ralTransientPlan_t *a,
	const ralTransientPlan_t *b );

// The plan pointer bound at Begin must remain alive and byte-immutable until
// Retire or Cancel. Every transition also receives and exact-validates it.
void Ral_TransientBatchLifecycleInit( ralTransientBatchLifecycle_t *lifecycle );
qboolean Ral_TransientBatchBegin( ralTransientBatchLifecycle_t *lifecycle,
	const ralTransientPlan_t *plan, uint32_t commandSlot,
	ralTransientBatchReceipt_t *out );
qboolean Ral_TransientBatchSubmit( ralTransientBatchLifecycle_t *lifecycle,
	const ralTransientBatchReceipt_t *authority, const ralTransientPlan_t *plan,
	uint64_t submissionGeneration, ralTransientBatchReceipt_t *out );
qboolean Ral_TransientBatchRetire( ralTransientBatchLifecycle_t *lifecycle,
	const ralTransientBatchReceipt_t *authority, const ralTransientPlan_t *plan,
	qboolean fenceCompleted, ralTransientBatchReceipt_t *out );
qboolean Ral_TransientBatchCancel( ralTransientBatchLifecycle_t *lifecycle,
	const ralTransientBatchReceipt_t *authority, const ralTransientPlan_t *plan,
	ralTransientBatchReceipt_t *out );
qboolean Ral_TransientBatchReceiptExact( const ralTransientBatchReceipt_t *a,
	const ralTransientBatchReceipt_t *b );

#endif
