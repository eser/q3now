// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "vk_temporal_pipeline_cohort.h"
#include "../../../core/ral.h"

#include <string.h>

qboolean VK_TemporalPreserveContractBuild( ralFormat_t sceneFormat,
		const VkPipelineColorBlendAttachmentState *sceneBlend,
		vkTemporalPreserveContract_t *out ) {
	vkTemporalPreserveContract_t candidate;
	if ( sceneFormat == RAL_FORMAT_UNDEFINED || !sceneBlend || !out ) return qfalse;
	memset( &candidate, 0, sizeof( candidate ) );
	candidate.formats[0] = sceneFormat;
	candidate.formats[1] = RAL_FORMAT_R16G16_SFLOAT;
	candidate.formats[2] = RAL_FORMAT_R8_UNORM;
	candidate.blends[0] = *sceneBlend;
	// A zero Vulkan colorWriteMask is explicit. The exact translator preserves it
	// via writeMaskExplicit=qtrue instead of applying the legacy zero=>ALL rule.
	candidate.blends[1].colorWriteMask = 0;
	candidate.blends[2].colorWriteMask = 0;
	candidate.count = 3;
	*out = candidate;
	return qtrue;
}

qboolean VK_TemporalPreservePipelineCreate( ralBackend_t *backend,
		const VkGraphicsPipelineCreateInfo *base, ralPipelineLayout_t *layout,
		ralFormat_t sceneFormat, ralFormat_t depthFormat, const char *debugName,
		vkTemporalExactPipelineCreateFn exactCreate,
		ralPipeline_t **outPipeline ) {
	const ralTextureUsage_t auxUsage = (ralTextureUsage_t)(
		RAL_TEXTURE_USAGE_COLOR_ATTACHMENT | RAL_TEXTURE_USAGE_SAMPLED
		| RAL_TEXTURE_USAGE_TRANSFER_SRC );
	vkTemporalPreserveContract_t contract;
	VkPipelineColorBlendStateCreateInfo blend;
	VkGraphicsPipelineCreateInfo exact;
	ralPipeline_t *candidate;
	const ralCaps_t *caps;

	if ( !backend || !base || !layout || sceneFormat == RAL_FORMAT_UNDEFINED
			|| depthFormat == RAL_FORMAT_UNDEFINED || !exactCreate
			|| !outPipeline || *outPipeline ) return qfalse;
	caps = Ral_GetCaps( backend );
	if ( !caps || !caps->independentBlend || caps->maxColorAttachments < 3
			|| !base->pColorBlendState
			|| base->pColorBlendState->attachmentCount != 1
			|| !base->pColorBlendState->pAttachments ) return qfalse;
	if ( !Ral_TextureFormatSupports( backend, sceneFormat,
			RAL_TEXTURE_USAGE_COLOR_ATTACHMENT )
			|| !Ral_TextureFormatSupports( backend, RAL_FORMAT_R16G16_SFLOAT, auxUsage )
			|| !Ral_TextureFormatSupports( backend, RAL_FORMAT_R8_UNORM, auxUsage ) ) {
		return qfalse;
	}
	if ( !VK_TemporalPreserveContractBuild( sceneFormat,
			base->pColorBlendState->pAttachments, &contract ) ) return qfalse;

	blend = *base->pColorBlendState;
	blend.attachmentCount = contract.count;
	blend.pAttachments = contract.blends;
	exact = *base;
	exact.pColorBlendState = &blend;
	candidate = exactCreate( &exact, layout, contract.formats, contract.count,
		depthFormat, debugName );
	if ( !candidate ) return qfalse;
	*outPipeline = candidate;
	return qtrue;
}
