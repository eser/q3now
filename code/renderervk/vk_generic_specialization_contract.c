// SPDX-License-Identifier: GPL-3.0-or-later
#include "vk_generic_specialization_contract.h"

#include <math.h>
#include <string.h>

static const uint32_t s_fragmentIds[VK_GENERIC_FRAGMENT_SPEC_COUNT] = {
	0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 9u, 10u, 11u, 15u, 14u, 26u
};

qboolean VK_GenericSpecializationAuthor( vkGenericSpecializationGraph_t *out,
		const void *vertexWord, const void *fragmentWords ) {
	vkGenericSpecializationGraph_t candidate;
	uint32_t i;
	if ( !out || !vertexWord || !fragmentWords ) return qfalse;
	memset( &candidate, 0, sizeof(candidate) );
	candidate.vertexMap[0].constantID = 16u;
	candidate.vertexMap[0].offset = 0u;
	candidate.vertexMap[0].size = sizeof(uint32_t);
	for ( i = 0; i < VK_GENERIC_FRAGMENT_SPEC_COUNT; ++i ) {
		candidate.fragmentMaps[i].constantID = s_fragmentIds[i];
		candidate.fragmentMaps[i].offset = i * sizeof(uint32_t);
		candidate.fragmentMaps[i].size = sizeof(uint32_t);
	}
	candidate.vertexInfo.mapEntryCount = VK_GENERIC_VERTEX_SPEC_COUNT;
	candidate.vertexInfo.dataSize = sizeof(uint32_t);
	candidate.vertexInfo.pData = vertexWord;
	candidate.fragmentInfo.mapEntryCount = VK_GENERIC_FRAGMENT_SPEC_COUNT;
	candidate.fragmentInfo.dataSize = VK_GENERIC_FRAGMENT_SPEC_COUNT * sizeof(uint32_t);
	candidate.fragmentInfo.pData = fragmentWords;
	*out = candidate;
	// Repair the self-references after the output-atomic structure copy.
	out->vertexInfo.pMapEntries = out->vertexMap;
	out->fragmentInfo.pMapEntries = out->fragmentMaps;
	return qtrue;
}

static qboolean MapExact( const VkSpecializationMapEntry *map, uint32_t count,
		const uint32_t *ids ) {
	uint32_t i;
	if ( !map || !ids ) return qfalse;
	for ( i = 0; i < count; ++i ) {
		if ( map[i].constantID != ids[i]
				|| map[i].offset != i * sizeof(uint32_t)
				|| map[i].size != sizeof(uint32_t) ) return qfalse;
	}
	return qtrue;
}

qboolean VK_GenericTemporalSpecializationValidate(
		const VkGraphicsPipelineCreateInfo *base,
		const vkGenericSpecializationFacts_t *facts,
		vkGenericSpecializationReceipt_t *out ) {
	vkGenericSpecializationReceipt_t candidate;
	const VkSpecializationInfo *vs, *fs;
	static const uint32_t vertexId = 16u;
	uint32_t depthThresholdBits, texDomainMask;
	float depthThreshold = 0.85f, fixedColor, fixedAlpha, depthFade;
	if ( !base || !facts || facts->textureCount > 2u
			|| (facts->shaderFog != qfalse && facts->shaderFog != qtrue)
			|| base->stageCount != 2u || !base->pStages
			|| base->pStages[0].stage != VK_SHADER_STAGE_VERTEX_BIT
			|| base->pStages[1].stage != VK_SHADER_STAGE_FRAGMENT_BIT ) return qfalse;
	vs = base->pStages[0].pSpecializationInfo;
	fs = base->pStages[1].pSpecializationInfo;
	if ( !vs || !fs
			|| vs->mapEntryCount != VK_GENERIC_VERTEX_SPEC_COUNT
			|| vs->dataSize != sizeof(uint32_t) || !vs->pData
			|| fs->mapEntryCount != VK_GENERIC_FRAGMENT_SPEC_COUNT
			|| fs->dataSize != VK_GENERIC_FRAGMENT_SPEC_COUNT * sizeof(uint32_t)
			|| !fs->pData
			|| !MapExact( vs->pMapEntries, VK_GENERIC_VERTEX_SPEC_COUNT, &vertexId )
			|| !MapExact( fs->pMapEntries, VK_GENERIC_FRAGMENT_SPEC_COUNT, s_fragmentIds ) ) return qfalse;
	memcpy( &candidate.vertexWord, vs->pData, sizeof(candidate.vertexWord) );
	memcpy( candidate.fragmentWords, fs->pData, sizeof(candidate.fragmentWords) );
	memcpy( &depthThresholdBits, &depthThreshold, sizeof(depthThresholdBits) );
	memcpy( &fixedColor, &candidate.fragmentWords[8], sizeof(fixedColor) );
	memcpy( &fixedAlpha, &candidate.fragmentWords[9], sizeof(fixedAlpha) );
	memcpy( &depthFade, &candidate.fragmentWords[11], sizeof(depthFade) );
	texDomainMask = (1u << (facts->textureCount + 1u)) - 1u;
	if ( candidate.vertexWord != 1u
			|| candidate.fragmentWords[0] != 0u
			|| candidate.fragmentWords[1] != 0u
			|| candidate.fragmentWords[2] != depthThresholdBits
			|| candidate.fragmentWords[3] != 0u
			|| (candidate.fragmentWords[4] & ~texDomainMask) != 0u
			|| candidate.fragmentWords[5] != 0u
			|| candidate.fragmentWords[6] > 7u
			|| (facts->textureCount == 0u && candidate.fragmentWords[6] != 0u)
			|| candidate.fragmentWords[7] != 0u
			|| !isfinite(fixedColor) || fixedColor < 0.0f || fixedColor > 1.0f
			|| !isfinite(fixedAlpha) || fixedAlpha < 0.0f || fixedAlpha > 1.0f
			|| (!facts->shaderFog && candidate.fragmentWords[10] != 0u)
			|| (facts->shaderFog && candidate.fragmentWords[10] > 3u)
			|| !isfinite(depthFade) || depthFade <= 0.0f
			|| candidate.fragmentWords[12] != 0u
			|| candidate.fragmentWords[13] != 0u
			|| candidate.fragmentWords[14] > facts->textureCount + 1u ) return qfalse;
	if ( out ) *out = candidate;
	return qtrue;
}
