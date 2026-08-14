// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#ifndef WIRED_VK_TEMPORAL_PIPELINE_COHORT_H
#define WIRED_VK_TEMPORAL_PIPELINE_COHORT_H

#ifndef VK_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#endif
#include "../renderercommon/vulkan/vulkan.h"
#include "../renderer/ral/ral_pipeline.h"

typedef struct {
	ralFormat_t formats[3];
	VkPipelineColorBlendAttachmentState blends[3];
	uint32_t count;
} vkTemporalPreserveContract_t;

typedef ralPipeline_t *(*vkTemporalExactPipelineCreateFn)(
	const VkGraphicsPipelineCreateInfo *createInfo,
	ralPipelineLayout_t *layout, const ralFormat_t *colorFormats,
	uint32_t numColorFormats, ralFormat_t depthFormat,
	const char *debugName );

// Authors the inert three-target signature used by a future temporal MAIN
// cohort. Attachment zero is copied byte-for-byte; velocity/validity have
// explicit zero write masks. On failure `out` is unchanged.
qboolean VK_TemporalPreserveContractBuild( ralFormat_t sceneFormat,
	const VkPipelineColorBlendAttachmentState *sceneBlend,
	vkTemporalPreserveContract_t *out );

// Shipping preserve-sibling factory with one injected synchronous exact-create
// seam. vk.c supplies its Vulkan->RAL translator; the host contract supplies a
// fake and executes this same validation/copy/publication path.
qboolean VK_TemporalPreservePipelineCreate( ralBackend_t *backend,
	const VkGraphicsPipelineCreateInfo *base, ralPipelineLayout_t *layout,
	ralFormat_t sceneFormat, ralFormat_t depthFormat, const char *debugName,
	vkTemporalExactPipelineCreateFn exactCreate,
	ralPipeline_t **outPipeline );

#endif
