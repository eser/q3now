// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#ifndef WIRED_RAL_TRANSIENT_TEXTURE_H
#define WIRED_RAL_TRANSIENT_TEXTURE_H

#include "ral_allocation.h"
#include "ral_resource.h"
#include "ral_transient.h"

#define RAL_TRANSIENT_TEXTURE_SCHEMA_VERSION 1u

typedef struct {
	uintptr_t resourceIdentity;
	uint64_t resourceGeneration;
	ralTextureCreateInfo_t texture;
	uint32_t firstPass;
	uint32_t lastPass;
	qboolean allowAlias;
} ralTransientTextureRequest_t;

typedef struct {
	ralTransientPolicy_t policy;
	const ralTransientTextureRequest_t *requests;
	uint32_t requestCount;
} ralTransientTextureCohortCreateInfo_t;

typedef struct {
	uint64_t size;
	uint64_t alignment;
	uint64_t compatibilityKey;
} ralTransientTextureCandidateFacts_t;

typedef enum {
	RAL_TRANSIENT_PHYSICAL_TEXTURE = 1,
	RAL_TRANSIENT_PHYSICAL_ALLOCATION
} ralTransientPhysicalRole_t;

typedef struct {
	qboolean (*createUnbound)( void *context,
		const ralTransientTextureRequest_t *request, uint32_t requestIndex,
		ralTexture_t **outTexture, ralTransientTextureCandidateFacts_t *outFacts );
	qboolean (*allocateSlot)( void *context, const ralTexture_t *representative,
		uint32_t slotIndex, const ralTransientSlot_t *slot, void **outAllocation );
	qboolean (*bindComplete)( void *context, ralTexture_t *texture,
		void *allocation, uint32_t requestIndex, uint32_t slotIndex );
	qboolean (*candidateAllowed)( void *context, ralTransientPhysicalRole_t role,
		uintptr_t identity );
	qboolean (*getAllocationReceipt)( void *context, const void *allocation,
		ralAllocationReceipt_t *out );
	void (*destroyTextureCandidate)( void *context, ralTexture_t *texture );
	void (*destroyAllocationCandidate)( void *context, void *allocation );
	void (*retireTexture)( void *context, ralTexture_t *texture );
	void (*retireAllocation)( void *context, void *allocation );
} ralTransientTextureOps_t;

typedef struct {
	uintptr_t resourceIdentity;
	uint64_t resourceGeneration;
	uintptr_t textureIdentity;
	uintptr_t allocationIdentity;
	uint32_t slotIndex;
} ralTransientTextureBindingReceipt_t;

typedef struct {
	uintptr_t allocationIdentity;
	ralAllocationReceipt_t allocation;
} ralTransientTextureSlotReceipt_t;

typedef struct {
	uint32_t schemaVersion;
	uintptr_t cohortIdentity;
	uint64_t generation;
	ralTransientPlan_t plan;
	ralTransientTextureBindingReceipt_t textures[RAL_TRANSIENT_MAX_REQUESTS];
	ralTransientTextureSlotReceipt_t allocations[RAL_TRANSIENT_MAX_REQUESTS];
	uint32_t textureCount;
	uint32_t allocationCount;
	qboolean ready;
} ralTransientTextureCohortReceipt_t;

_Static_assert( sizeof( ralTransientTextureCohortReceipt_t ) <= 65536u,
	"transient texture receipt must remain bounded" );

typedef struct ralTransientTextureCohort_s ralTransientTextureCohort_t;

qboolean Ral_TransientTextureCohortCreateWithOps(
	const ralTransientTextureCohortCreateInfo_t *ci,
	const ralTransientTextureOps_t *ops, void *context,
	ralTransientTextureCohort_t **out );
ralTransientTextureCohort_t *Ral_CreateTransientTextureCohort(
	ralBackend_t *backend, const ralTransientTextureCohortCreateInfo_t *ci );
const ralTexture_t *Ral_TransientTextureCohortGetTexture(
	const ralTransientTextureCohort_t *cohort, uint32_t requestIndex );
const ralTransientPlan_t *Ral_TransientTextureCohortGetPlan(
	const ralTransientTextureCohort_t *cohort );
qboolean Ral_TransientTextureCohortGetReceipt(
	const ralTransientTextureCohort_t *cohort,
	ralTransientTextureCohortReceipt_t *out );
qboolean Ral_TransientTextureCohortReceiptExact(
	const ralTransientTextureCohortReceipt_t *a,
	const ralTransientTextureCohortReceipt_t *b );
qboolean Ral_TransientTextureCohortBegin( ralTransientTextureCohort_t *cohort,
	uint32_t commandSlot, ralTransientBatchReceipt_t *out );
qboolean Ral_TransientTextureCohortSubmit( ralTransientTextureCohort_t *cohort,
	const ralTransientBatchReceipt_t *planned, uint64_t submissionGeneration,
	ralTransientBatchReceipt_t *out );
qboolean Ral_TransientTextureCohortRetire( ralTransientTextureCohort_t *cohort,
	const ralTransientBatchReceipt_t *submitted, qboolean fenceCompleted,
	ralTransientBatchReceipt_t *out );
qboolean Ral_TransientTextureCohortCancel( ralTransientTextureCohort_t *cohort,
	const ralTransientBatchReceipt_t *planned, ralTransientBatchReceipt_t *out );
qboolean Ral_TransientTextureCohortReleaseTerminal(
	ralTransientTextureCohort_t **cohort,
	const ralTransientBatchReceipt_t *terminalBatch );

#endif
