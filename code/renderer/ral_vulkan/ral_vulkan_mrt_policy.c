// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_vulkan_internal.h"

VkColorComponentFlags ralVk_ColorWriteMask( const ralColorBlendAttachment_t *blend ) {
	if ( !blend ) return (VkColorComponentFlags)RAL_COLOR_WRITE_ALL;
	if ( blend->writeMaskExplicit ) return (VkColorComponentFlags)blend->writeMask;
	return (VkColorComponentFlags)( blend->writeMask ? blend->writeMask : RAL_COLOR_WRITE_ALL );
}

typedef struct {
	qboolean         blendEnable;
	ralBlendFactor_t srcColor, dstColor;
	ralBlendOp_t     colorOp;
	ralBlendFactor_t srcAlpha, dstAlpha;
	ralBlendOp_t     alphaOp;
	uint32_t         writeMask;
} ralVkEffectiveColorBlend_t;

static void ralVk_EffectiveColorBlend( const ralColorBlendAttachment_t *in,
	                                    ralVkEffectiveColorBlend_t *out ) {
	memset( out, 0, sizeof( *out ) );
	out->writeMask = (uint32_t)ralVk_ColorWriteMask( in );
	if ( !in ) return;
	out->blendEnable = in->blendEnable ? qtrue : qfalse;
	out->srcColor = in->srcColor;
	out->dstColor = in->dstColor;
	out->colorOp  = in->colorOp;
	out->srcAlpha = in->srcAlpha;
	out->dstAlpha = in->dstAlpha;
	out->alphaOp  = in->alphaOp;
}

qboolean ralVk_ColorBlendStatesSupported( const ralGraphicsPipelineCreateInfo_t *ci,
	                                       qboolean independentBlend ) {
	ralVkEffectiveColorBlend_t first, current;
	uint32_t i;

	if ( !ci ) return qfalse;
	if ( ci->numColorBlends > 0 && !ci->colorBlends ) return qfalse;
	if ( independentBlend || ci->numColorFormats <= 1 ) return qtrue;
	ralVk_EffectiveColorBlend( ci->numColorBlends ? &ci->colorBlends[0] : NULL, &first );
	for ( i = 1; i < ci->numColorFormats; ++i ) {
		const ralColorBlendAttachment_t *src = i < ci->numColorBlends ? &ci->colorBlends[i] : NULL;
		ralVk_EffectiveColorBlend( src, &current );
		if ( memcmp( &first, &current, sizeof( first ) ) != 0 ) return qfalse;
	}
	return qtrue;
}

qboolean ralVk_IndependentBlendEnabled( qboolean backendOwnsDevice,
	                                     qboolean requested, VkBool32 supported ) {
	return backendOwnsDevice && requested && supported == VK_TRUE ? qtrue : qfalse;
}

VkImageUsageFlags ralVk_TextureUsage( ralTextureUsage_t u ) {
	// SAMPLED + TRANSFER are always on (default view sampling, upload, mip-gen blit, readback).
	VkImageUsageFlags v = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	if ( u & RAL_TEXTURE_USAGE_STORAGE                  ) v |= VK_IMAGE_USAGE_STORAGE_BIT;
	if ( u & RAL_TEXTURE_USAGE_COLOR_ATTACHMENT         ) v |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	if ( u & RAL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT ) v |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	return v;
}

VkFormatFeatureFlags ralVk_TextureUsageFormatFeatures( ralTextureUsage_t u ) {
	// Match ralVk_TextureUsage exactly: every RAL image is sampled, uploadable,
	// and readable even when the caller did not spell those convenience bits.
	VkFormatFeatureFlags v = VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT
	                       | VK_FORMAT_FEATURE_TRANSFER_SRC_BIT
	                       | VK_FORMAT_FEATURE_TRANSFER_DST_BIT;
	if ( u & RAL_TEXTURE_USAGE_STORAGE                  ) v |= VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT;
	if ( u & RAL_TEXTURE_USAGE_COLOR_ATTACHMENT         ) v |= VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT;
	if ( u & RAL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT ) v |= VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;
	return v;
}

qboolean Ral_TextureFormatSupports( ralBackend_t *b, ralFormat_t format,
	                                  ralTextureUsage_t usage ) {
	const uint32_t knownUsage = RAL_TEXTURE_USAGE_SAMPLED | RAL_TEXTURE_USAGE_STORAGE
	                          | RAL_TEXTURE_USAGE_COLOR_ATTACHMENT
	                          | RAL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT
	                          | RAL_TEXTURE_USAGE_TRANSFER_SRC | RAL_TEXTURE_USAGE_TRANSFER_DST;
	VkFormatProperties props;
	VkFormat vkFormat;
	VkFormatFeatureFlags required;

	if ( !b || !b->vk.GetPhysicalDeviceFormatProperties || b->physicalDevice == VK_NULL_HANDLE ) return qfalse;
	if ( ( (uint32_t)usage & ~knownUsage ) != 0u ) return qfalse;
	vkFormat = ralVk_TranslateFormat( format );
	if ( vkFormat == VK_FORMAT_UNDEFINED ) return qfalse;
	required = ralVk_TextureUsageFormatFeatures( usage );
	memset( &props, 0, sizeof( props ) );
	b->vk.GetPhysicalDeviceFormatProperties( b->physicalDevice, vkFormat, &props );
	return ( props.optimalTilingFeatures & required ) == required ? qtrue : qfalse;
}
