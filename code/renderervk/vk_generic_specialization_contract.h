// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef WIRED_VK_GENERIC_SPECIALIZATION_CONTRACT_H
#define WIRED_VK_GENERIC_SPECIALIZATION_CONTRACT_H

#ifndef VK_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#endif
#include "../renderercommon/vulkan/vulkan.h"
#include "../qcommon/q_shared.h"

enum {
	VK_GENERIC_VERTEX_SPEC_COUNT = 1,
	VK_GENERIC_FRAGMENT_SPEC_COUNT = 15,
	VK_GENERIC_TOTAL_SPEC_COUNT = VK_GENERIC_VERTEX_SPEC_COUNT + VK_GENERIC_FRAGMENT_SPEC_COUNT,
	VK_GENERIC_TRANSLATOR_SPEC_CAPACITY = 18
};

// Map/info storage only. The two pData fields borrow the caller-owned words and
// must not outlive them. Author is output-atomic and never changes either data
// block.
typedef struct {
	VkSpecializationMapEntry vertexMap[VK_GENERIC_VERTEX_SPEC_COUNT];
	VkSpecializationMapEntry fragmentMaps[VK_GENERIC_FRAGMENT_SPEC_COUNT];
	VkSpecializationInfo vertexInfo;
	VkSpecializationInfo fragmentInfo;
} vkGenericSpecializationGraph_t;

typedef struct {
	uint32_t vertexWord;
	uint32_t fragmentWords[VK_GENERIC_FRAGMENT_SPEC_COUNT];
} vkGenericSpecializationReceipt_t;

typedef struct {
	uint32_t textureCount; // Additional texture-coordinate axes: 0..2.
	qboolean shaderFog;
} vkGenericSpecializationFacts_t;

qboolean VK_GenericSpecializationAuthor( vkGenericSpecializationGraph_t *out,
	const void *vertexWord, const void *fragmentWords );

// Accepts only the exact production generic graph: VS {16}, FS
// {0..11,15,14,26}, exact order/offset/size and caller data sizes. Temporal
// admission additionally requires entity-SSBO transforms, passive alpha test,
// disabled alpha-to-coverage/discard, and a supported texture operation.
// `out` is optional; when supplied, failure leaves it byte-identical.
qboolean VK_GenericTemporalSpecializationValidate(
	const VkGraphicsPipelineCreateInfo *base,
	const vkGenericSpecializationFacts_t *facts,
	vkGenericSpecializationReceipt_t *out );

#endif
