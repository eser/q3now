// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_frame_graph_transient.h"

#include <string.h>

static qboolean CreateCohort( void *context,
		const ralTransientTextureCohortCreateInfo_t *ci,
		ralTransientTextureCohort_t **out ) {
	ralTransientTextureCohort_t *candidate;
	if ( !context || !ci || !out ) return qfalse;
	candidate=Ral_CreateTransientTextureCohort((ralBackend_t*)context,ci);
	if ( !candidate ) return qfalse;
	*out=candidate;return qtrue;
}

static qboolean CandidateAllowed( void *context,
		ralFrameGraphTransientPhysicalRole_t role, uintptr_t identity ) {
	return context && role==RAL_FRAME_GRAPH_TRANSIENT_PHYSICAL_COHORT
		&& identity!=(uintptr_t)0 && identity!=(uintptr_t)context ? qtrue:qfalse;
}

static qboolean GetReceipt( void *context,
		const ralTransientTextureCohort_t *cohort,
		ralTransientTextureCohortReceipt_t *out ) {
	(void)context;return Ral_TransientTextureCohortGetReceipt(cohort,out);
}

static const ralTexture_t *GetTexture( void *context,
		const ralTransientTextureCohort_t *cohort, uint32_t requestIndex ) {
	(void)context;return Ral_TransientTextureCohortGetTexture(cohort,requestIndex);
}

static qboolean Begin( void *context, ralTransientTextureCohort_t *cohort,
		uint32_t commandSlot, ralTransientBatchReceipt_t *out ) {
	(void)context;return Ral_TransientTextureCohortBegin(cohort,commandSlot,out);
}

static qboolean Submit( void *context, ralTransientTextureCohort_t *cohort,
		const ralTransientBatchReceipt_t *planned, uint64_t submissionGeneration,
		ralTransientBatchReceipt_t *out ) {
	(void)context;return Ral_TransientTextureCohortSubmit(cohort,planned,
		submissionGeneration,out);
}

static qboolean Retire( void *context, ralTransientTextureCohort_t *cohort,
		const ralTransientBatchReceipt_t *submitted, qboolean fenceCompleted,
		ralTransientBatchReceipt_t *out ) {
	(void)context;return Ral_TransientTextureCohortRetire(cohort,submitted,
		fenceCompleted,out);
}

static qboolean Cancel( void *context, ralTransientTextureCohort_t *cohort,
		const ralTransientBatchReceipt_t *planned,
		ralTransientBatchReceipt_t *out ) {
	(void)context;return Ral_TransientTextureCohortCancel(cohort,planned,out);
}

static qboolean ReleaseFresh( void *context,
		ralTransientTextureCohort_t **cohort ) {
	(void)context;return Ral_TransientTextureCohortReleaseFresh(cohort);
}

static qboolean ReleaseTerminal( void *context,
		ralTransientTextureCohort_t **cohort,
		const ralTransientBatchReceipt_t *terminalBatch ) {
	(void)context;return Ral_TransientTextureCohortReleaseTerminal(cohort,
		terminalBatch);
}

const ralFrameGraphTransientOps_t *RalFrameGraphTransient_RalOps( void ) {
	static const ralFrameGraphTransientOps_t ops={
		CreateCohort,CandidateAllowed,GetReceipt,GetTexture,Begin,Submit,
		Retire,Cancel,ReleaseFresh,ReleaseTerminal
	};
	return &ops;
}

ralFrameGraphTransient_t *RalFrameGraphTransient_Create(
		ralBackend_t *backend, const ralFrameGraphTransientCreateInfo_t *ci ) {
	ralFrameGraphTransient_t *owner=NULL;
	return RalFrameGraphTransient_CreateWithOps(ci,
		RalFrameGraphTransient_RalOps(),backend,&owner)?owner:NULL;
}
