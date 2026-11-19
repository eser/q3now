// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors
#ifndef WIRED_RAL_MATERIAL_STAGE_DEPTH_H
#define WIRED_RAL_MATERIAL_STAGE_DEPTH_H

#include "ral_material_source.h"

/* All stages use the same geometry/depth bias. This proves depth and coverage
 * equivalence ONLY, not blend/color/fog equivalence or permission to fuse a
 * complete material program. Adapters must not mistake singleDraw=false for
 * permission to use the first stage's state for the whole program. */
typedef struct {
	uint32_t depthFunc;
	qboolean depthWrite;
	uint32_t alphaTest;
} ralMaterialStageDepth_t;

typedef struct {
	qboolean singleDraw;
	uint32_t depthFunc;
	qboolean depthWrite;
} ralMaterialStageDepthPlan_t;

/* Bounded publication-time analysis, no native handles or allocation. Invalid
 * input leaves out unchanged. Valid but non-collapsible depth returns a zero
 * plan; ordered draws are still required. */
static inline qboolean Ral_MaterialStageDepthCollapse(
		const ralMaterialStageDepth_t *stages, uint32_t count,
		ralMaterialStageDepthPlan_t *out ) {
	ralMaterialStageDepthPlan_t plan = { qfalse, 0u, qfalse };
	qboolean collapsible = qtrue, wroteDepth = qfalse;
	if ( !stages || !out || !count || count > RAL_MATERIAL_SOURCE_MAX_STAGES )
		return qfalse;
	for ( uint32_t i = 0u; i < count; ++i ) {
		const ralMaterialStageDepth_t *stage = &stages[i];
		if ( stage->depthFunc > RAL_MATERIAL_DEPTH_EQUAL
				|| stage->alphaTest > RAL_MATERIAL_ALPHA_TEST_KEEP_GREATER_EQUAL
				|| ( stage->depthWrite != qfalse && stage->depthWrite != qtrue ) )
			return qfalse;
		/* Stage-dependent discard can change both visible contributors and
		 * subsequent depth. Do not turn a later discard into a material discard. */
		if ( count > 1u && stage->alphaTest != RAL_MATERIAL_ALPHA_TEST_NONE )
			collapsible = qfalse;
		if ( i && stage->depthFunc != stages[0].depthFunc ) {
			/* LEQUAL -> EQUAL is equivalent only AFTER an unconditional write.
			 * EQUAL -> LEQUAL is never equivalent: if the first draw fails on
			 * a nearer fragment, the later LEQUAL draw may still pass. */
			if ( stages[0].depthFunc != RAL_MATERIAL_DEPTH_LEQUAL || !wroteDepth )
				collapsible = qfalse;
		}
		if ( stage->depthWrite ) wroteDepth = qtrue;
	}
	if ( collapsible ) {
		plan.singleDraw = qtrue;
		plan.depthFunc = stages[0].depthFunc;
		plan.depthWrite = wroteDepth;
	}
	*out = plan;
	return qtrue;
}

#endif
