// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef WIRED_RAL_VULKAN_TRANSLATE_H
#define WIRED_RAL_VULKAN_TRANSLATE_H

#include "../ral/ral_command.h"
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

#endif
