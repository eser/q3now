// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "vk_temporal_shader_cohort.h"

#include <string.h>

static qboolean BlobValid( vkTemporalShaderBlob_t blob ) {
	return blob.bytes != NULL && blob.size != 0u && (blob.size & 3u) == 0u;
}

qboolean VK_TemporalShaderRecipeBuild(
		const vkTemporalShaderRecipeInput_t *input,
		vkTemporalShaderRecipe_t *out ) {
	vkTemporalShaderRecipe_t candidate;
	uint32_t i;

	if ( !input || !out || input->sceneFormat == RAL_FORMAT_UNDEFINED
			|| input->alphaTested || input->depthOnly || input->blended
			|| input->special || input->dynamicDiscard
			|| input->sceneBlend.blendEnable ) return qfalse;
	if ( input->kind < VK_TEMPORAL_SHADER_PRESERVE
			|| input->kind > VK_TEMPORAL_SHADER_IQM_INVALIDATE ) return qfalse;
	if ( input->kind == VK_TEMPORAL_SHADER_IQM_INVALIDATE && input->fog ) return qfalse;

	memset( &candidate, 0, sizeof( candidate ) );
	candidate.kind = input->kind;
	candidate.colorFormats[0] = input->sceneFormat;
	candidate.colorFormats[1] = RAL_FORMAT_R16G16_SFLOAT;
	candidate.colorFormats[2] = RAL_FORMAT_R8_UNORM;
	candidate.colorBlends[0] = input->sceneBlend;
	candidate.colorBlends[1].writeMaskExplicit = qtrue;
	candidate.colorBlends[2].writeMaskExplicit = qtrue;
	candidate.numColorAttachments = 3u;

	if ( input->kind == VK_TEMPORAL_SHADER_IQM_INVALIDATE ) {
		for ( i = 0; i < 2u; ++i ) {
			if ( !input->iqmSetLayouts[i] ) return qfalse;
			candidate.setLayouts[i] = input->iqmSetLayouts[i];
		}
		candidate.numSetLayouts = 2u;
		candidate.vertex = input->iqmVertex;
		candidate.fragment = input->iqmInvalidateFragment;
	} else {
		for ( i = 0; i < 4u; ++i ) {
			if ( !input->genericSetLayouts[i] ) return qfalse;
			candidate.setLayouts[i] = input->genericSetLayouts[i];
		}
		candidate.numSetLayouts = 4u;
		if ( input->fog ) {
			candidate.pushOffset = VK_TEMPORAL_FOG_PUSH_OFFSET;
			candidate.pushSize = VK_TEMPORAL_FOG_PUSH_SIZE;
			candidate.pushStages = RAL_STAGE_FRAGMENT;
			candidate.numPushRanges = 1u;
		}
		if ( input->kind == VK_TEMPORAL_SHADER_PRESERVE ) {
			candidate.vertex = input->ordinaryVertex;
			candidate.fragment = input->ordinaryFragment;
		} else {
			candidate.vertex = input->temporalVertex;
			candidate.fragment = input->kind == VK_TEMPORAL_SHADER_WRITE
				? input->temporalWriteFragment
				: input->temporalInvalidateFragment;
		}
	}
	if ( !BlobValid( candidate.vertex ) || !BlobValid( candidate.fragment ) ) return qfalse;

	if ( input->kind == VK_TEMPORAL_SHADER_PRESERVE ) {
		candidate.colorBlends[1].writeMask = 0u;
		candidate.colorBlends[2].writeMask = 0u;
		candidate.directSpirvOverride = qfalse;
	} else {
		candidate.colorBlends[1].writeMask = RAL_COLOR_WRITE_R | RAL_COLOR_WRITE_G;
		candidate.colorBlends[2].writeMask = RAL_COLOR_WRITE_R;
		candidate.directSpirvOverride = qtrue;
	}
	*out = candidate;
	return qtrue;
}
