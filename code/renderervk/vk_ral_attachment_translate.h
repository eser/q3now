// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef WIRED_VK_RAL_ATTACHMENT_TRANSLATE_H
#define WIRED_VK_RAL_ATTACHMENT_TRANSLATE_H

#ifndef VK_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#endif
#include "../renderercommon/vulkan/vulkan.h"
#include "../renderer/ral/ral_pipeline.h"

typedef struct {
	ralFormat_t                    colorFormats[ RAL_MAX_COLOR_ATTACHMENTS ];
	ralColorBlendAttachment_t      colorBlends[ RAL_MAX_COLOR_ATTACHMENTS ];
	uint32_t                       numColorAttachments;
	ralFormat_t                    depthFormat;
} vkRalAttachmentContract_t;

// Translate one exact Vulkan dynamic-rendering attachment signature. Count
// zero is an unambiguous depth-only contract. On failure `out` is unchanged.
qboolean VK_RalAttachmentContractFromVk(
	const ralFormat_t *colorFormats, uint32_t numColorFormats,
	ralFormat_t depthFormat,
	const VkPipelineColorBlendStateCreateInfo *vkBlend,
	vkRalAttachmentContract_t *out );

// Validate an authored special-pipeline contract. On success `out` receives a
// value-copy safe for a synchronous Ral_CreateGraphicsPipeline call; on failure
// it is unchanged.
qboolean VK_RalAttachmentContractCopy(
	const vkRalAttachmentContract_t *in,
	vkRalAttachmentContract_t *out );

#endif
