// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
// Physical transient-texture materialization for a compiled frame graph.
// The owner remains above RAL: it consumes the public transient cohort,
// publishes portable execution bindings, and measures alias savings solely
// from exact RAL allocation receipts.

#ifndef WIRED_RAL_FRAME_GRAPH_TRANSIENT_H
#define WIRED_RAL_FRAME_GRAPH_TRANSIENT_H

#include "ral_frame_graph_execution.h"
#include "../ral_transient_texture.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RAL_FRAME_GRAPH_TRANSIENT_SCHEMA_VERSION 1u

typedef struct {
	uint32_t schemaVersion;
	uint64_t generation;
	// Borrowed and byte-immutable for the materialization lifetime.
	const ralFrameGraphPlan_t *graphPlan;
	// Canonical transient-plan order, not graph resource declaration order.
	const ralTransientTextureRequest_t *textureRequests;
	uint32_t textureRequestCount;
} ralFrameGraphTransientCreateInfo_t;

typedef enum {
	RAL_FRAME_GRAPH_TRANSIENT_PHYSICAL_COHORT = 1
} ralFrameGraphTransientPhysicalRole_t;

typedef struct {
	qboolean (*createCohort)( void *context,
		const ralTransientTextureCohortCreateInfo_t *ci,
		ralTransientTextureCohort_t **out );
	qboolean (*candidateAllowed)( void *context,
		ralFrameGraphTransientPhysicalRole_t role, uintptr_t identity );
	qboolean (*getReceipt)( void *context,
		const ralTransientTextureCohort_t *cohort,
		ralTransientTextureCohortReceipt_t *out );
	const ralTexture_t *(*getTexture)( void *context,
		const ralTransientTextureCohort_t *cohort, uint32_t requestIndex );
	qboolean (*begin)( void *context, ralTransientTextureCohort_t *cohort,
		uint32_t commandSlot, ralTransientBatchReceipt_t *out );
	qboolean (*submit)( void *context, ralTransientTextureCohort_t *cohort,
		const ralTransientBatchReceipt_t *planned, uint64_t submissionGeneration,
		ralTransientBatchReceipt_t *out );
	qboolean (*retire)( void *context, ralTransientTextureCohort_t *cohort,
		const ralTransientBatchReceipt_t *submitted, qboolean fenceCompleted,
		ralTransientBatchReceipt_t *out );
	qboolean (*cancel)( void *context, ralTransientTextureCohort_t *cohort,
		const ralTransientBatchReceipt_t *planned,
		ralTransientBatchReceipt_t *out );
	qboolean (*releaseFresh)( void *context,
		ralTransientTextureCohort_t **cohort );
	qboolean (*releaseTerminal)( void *context,
		ralTransientTextureCohort_t **cohort,
		const ralTransientBatchReceipt_t *terminalBatch );
} ralFrameGraphTransientOps_t;

typedef struct {
	uint32_t schemaVersion;
	uintptr_t materializationIdentity;
	uint64_t generation;
	const ralFrameGraphPlan_t *graphPlanIdentity;
	uint64_t graphGeneration;
	uintptr_t opsContextIdentity;
	ralTransientTextureCohortReceipt_t physical;
	ralFrameGraphResourceBinding_t bindings[RAL_TRANSIENT_MAX_REQUESTS];
	uint32_t bindingCount;
	// A disjoint-equivalent count repeats the authoritative backing allocation
	// receipt once per logical binding. physicalCommittedBytes counts each
	// distinct allocation exactly once.
	uint64_t disjointEquivalentCommittedBytes;
	uint64_t physicalCommittedBytes;
	uint64_t savedBytes;
	uint32_t savedPermille;
	qboolean ready;
} ralFrameGraphTransientReceipt_t;

_Static_assert( sizeof( ralFrameGraphTransientReceipt_t ) <= 131072u,
	"frame graph transient receipt must remain bounded" );

typedef struct ralFrameGraphTransient_s ralFrameGraphTransient_t;

qboolean RalFrameGraphTransient_CreateWithOps(
	const ralFrameGraphTransientCreateInfo_t *ci,
	const ralFrameGraphTransientOps_t *ops, void *context,
	ralFrameGraphTransient_t **out );
ralFrameGraphTransient_t *RalFrameGraphTransient_Create(
	ralBackend_t *backend, const ralFrameGraphTransientCreateInfo_t *ci );
qboolean RalFrameGraphTransient_GetReceipt(
	const ralFrameGraphTransient_t *owner,
	ralFrameGraphTransientReceipt_t *out );
qboolean RalFrameGraphTransient_ReceiptExact(
	const ralFrameGraphTransientReceipt_t *a,
	const ralFrameGraphTransientReceipt_t *b );
qboolean RalFrameGraphTransient_GetExecutionBindings(
	const ralFrameGraphTransient_t *owner,
	const ralFrameGraphPlan_t *currentGraphPlan,
	ralFrameGraphResourceBinding_t *outBindings, uint32_t bindingCapacity,
	uint32_t *outBindingCount );
qboolean RalFrameGraphTransient_Begin( ralFrameGraphTransient_t *owner,
	uint32_t commandSlot, ralTransientBatchReceipt_t *out );
qboolean RalFrameGraphTransient_Submit( ralFrameGraphTransient_t *owner,
	const ralTransientBatchReceipt_t *planned, uint64_t submissionGeneration,
	ralTransientBatchReceipt_t *out );
qboolean RalFrameGraphTransient_Retire( ralFrameGraphTransient_t *owner,
	const ralTransientBatchReceipt_t *submitted, qboolean fenceCompleted,
	ralTransientBatchReceipt_t *out );
qboolean RalFrameGraphTransient_Cancel( ralFrameGraphTransient_t *owner,
	const ralTransientBatchReceipt_t *planned, ralTransientBatchReceipt_t *out );
// Candidate cleanup before the first batch begins. This mirrors the physical
// cohort's create-time cleanup authority and is unavailable after Begin.
qboolean RalFrameGraphTransient_ReleaseFresh(
	ralFrameGraphTransient_t **owner );
qboolean RalFrameGraphTransient_ReleaseTerminal(
	ralFrameGraphTransient_t **owner,
	const ralTransientBatchReceipt_t *terminalBatch );

// Public-RAL adapter used by RalFrameGraphTransient_Create.
const ralFrameGraphTransientOps_t *RalFrameGraphTransient_RalOps( void );

#ifdef __cplusplus
}
#endif

#endif // WIRED_RAL_FRAME_GRAPH_TRANSIENT_H
