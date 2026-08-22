// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef WIRED_RAL_VULKAN_TRANSLATE_H
#define WIRED_RAL_VULKAN_TRANSLATE_H

#include "../ral/ral_command.h"
#include "../ral/ral_transition.h"
#include "../../renderercommon/vulkan/vulkan_core.h"

typedef struct {
	VkPipelineStageFlags srcStage;
	VkPipelineStageFlags dstStage;
	VkAccessFlags        srcAccess;
	VkAccessFlags        dstAccess;
} ralVkBarrierTranslation_t;

typedef struct {
	VkPipelineStageFlags stage;
	VkAccessFlags        access;
} ralVkLayoutTranslation_t;

typedef struct {
	VkPipelineStageFlags stage;
	VkAccessFlags        access;
	VkImageLayout        layout;
} ralVkResourceStateTranslation_t;

VkFormat         ralVk_TranslateFormat      ( ralFormat_t f );
VkColorSpaceKHR  ralVk_TranslateColorSpace  ( ralColorSpace_t cs );
VkPresentModeKHR ralVk_TranslatePresentMode ( ralPresentMode_t pm );
VkExtent2D       ralVk_TranslateShadingRate ( ralFragmentShadingRate_t rate );
VkAttachmentLoadOp  ralVk_TranslateLoadOp   ( ralLoadOp_t op );
VkAttachmentStoreOp ralVk_TranslateStoreOp  ( ralStoreOp_t op );
VkAccessFlags        ralVk_TranslateStageAccess( ralPipelineStageFlags_t stage );
ralVkBarrierTranslation_t ralVk_TranslateBarrierScope( ralBarrierScope_t scope );
ralVkLayoutTranslation_t  ralVk_TranslateSourceLayout( VkImageLayout layout );
ralVkLayoutTranslation_t  ralVk_TranslateDestinationLayout( VkImageLayout layout );
qboolean ralVk_TranslateBufferResourceState( const ralResourceState_t *state,
	                                          ralVkResourceStateTranslation_t *out );
qboolean ralVk_TranslateTextureResourceState( const ralResourceState_t *state,
	                                           ralVkResourceStateTranslation_t *out );
qboolean ralVk_TranslateTextureViewAspect( int aspect,
	                                        VkImageAspectFlags available,
	                                        VkImageAspectFlags *out );
qboolean ralVk_TranslateTextureCopyAspect(
	ralTextureAspectFlags_t requested, VkImageAspectFlags available,
	VkImageAspectFlags *out );
qboolean ralVk_PublishAdoptedTextureResourceState( ralTexture_t *texture,
	const ralResourceState_t *state, ralQueueType_t ownerQueue );
qboolean ralVk_PublishAttachmentResourceState( ralCommandBuffer_t *command,
	                                             ralTexture_t *texture,
	                                             VkImageLayout attachmentLayout );

#endif
