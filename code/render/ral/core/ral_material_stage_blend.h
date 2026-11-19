// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#ifndef WIRED_RAL_MATERIAL_STAGE_BLEND_H
#define WIRED_RAL_MATERIAL_STAGE_BLEND_H

#include "ral_material_source.h"

typedef struct {
	uint32_t sourceBlend;
	uint32_t destinationBlend;
} ralMaterialStageBlend_t;

/* Source-IR blend encoding: ONE=1, SRC_ALPHA=6, SRC_ALPHA_SATURATE=10.
 * This class has framebuffer RGB = previous RGB + sum(covered stage RGB).
 * Adapters must add that sum with ONE/ONE, never apply SRC_ALPHA again.
 * This is a blend classification, not a geometry/depth/alpha-test/fog fusion
 * proof. In particular it says nothing about destination-dependent factors or
 * arbitrary post-lighting transforms. Alpha-channel factors are a separate
 * attachment contract, not inferred from the RGB factors here. */
static inline qboolean Ral_MaterialStageBlendAdditive(
		const ralMaterialStageBlend_t *stages, uint32_t count, qboolean *out ) {
	qboolean additive = qtrue;
	if ( !stages || !out || !count || count > RAL_MATERIAL_SOURCE_MAX_STAGES )
		return qfalse;
	for ( uint32_t i = 0u; i < count; ++i ) {
		if ( stages[i].sourceBlend > 10u || stages[i].destinationBlend > 10u )
			return qfalse;
		if ( stages[i].destinationBlend != 1u
				|| ( stages[i].sourceBlend != 1u && stages[i].sourceBlend != 6u ) )
			additive = qfalse;
	}
	*out = additive;
	return qtrue;
}

#endif
