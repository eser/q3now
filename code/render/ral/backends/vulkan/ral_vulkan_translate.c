// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_vulkan_internal.h"

static qboolean ralVk_TranslateShaderStages( uint32_t stages,
	                                         VkPipelineStageFlags *out ) {
	VkPipelineStageFlags candidate = 0;
	if ( !out || stages == 0 || ( stages & ~RAL_STAGE_ALL ) != 0 )
		return qfalse;
	if ( stages & RAL_STAGE_VERTEX )
		candidate |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
	if ( stages & RAL_STAGE_FRAGMENT )
		candidate |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	if ( stages & RAL_STAGE_COMPUTE )
		candidate |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
	*out = candidate;
	return qtrue;
}

VkFormat ralVk_TranslateFormat( ralFormat_t f ) {
	switch ( f ) {
	case RAL_FORMAT_R8_UNORM: return VK_FORMAT_R8_UNORM;
	case RAL_FORMAT_R8G8_UNORM: return VK_FORMAT_R8G8_UNORM;
	case RAL_FORMAT_R8G8B8_UNORM: return VK_FORMAT_R8G8B8_UNORM;
	case RAL_FORMAT_R8G8B8A8_UNORM: return VK_FORMAT_R8G8B8A8_UNORM;
	case RAL_FORMAT_R8G8B8A8_SRGB: return VK_FORMAT_R8G8B8A8_SRGB;
	case RAL_FORMAT_B8G8R8A8_UNORM: return VK_FORMAT_B8G8R8A8_UNORM;
	case RAL_FORMAT_B8G8R8A8_SRGB: return VK_FORMAT_B8G8R8A8_SRGB;
	case RAL_FORMAT_A2B10G10R10_UNORM: return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
	case RAL_FORMAT_A2R10G10B10_UNORM: return VK_FORMAT_A2R10G10B10_UNORM_PACK32;
	case RAL_FORMAT_B5G6R5_UNORM: return VK_FORMAT_B5G6R5_UNORM_PACK16;
	case RAL_FORMAT_R5G6B5_UNORM: return VK_FORMAT_R5G6B5_UNORM_PACK16;
	case RAL_FORMAT_B4G4R4A4_UNORM: return VK_FORMAT_B4G4R4A4_UNORM_PACK16;
	case RAL_FORMAT_A1R5G5B5_UNORM: return VK_FORMAT_A1R5G5B5_UNORM_PACK16;
	case RAL_FORMAT_R16_UNORM: return VK_FORMAT_R16_UNORM;
	case RAL_FORMAT_R16_SFLOAT: return VK_FORMAT_R16_SFLOAT;
	case RAL_FORMAT_R16G16_SFLOAT: return VK_FORMAT_R16G16_SFLOAT;
	case RAL_FORMAT_R16G16_SNORM: return VK_FORMAT_R16G16_SNORM;
	case RAL_FORMAT_R16G16B16A16_SFLOAT: return VK_FORMAT_R16G16B16A16_SFLOAT;
	case RAL_FORMAT_R16G16B16A16_UNORM: return VK_FORMAT_R16G16B16A16_UNORM;
	case RAL_FORMAT_R11G11B10_UFLOAT: return VK_FORMAT_B10G11R11_UFLOAT_PACK32;
	case RAL_FORMAT_E5B9G9R9_UFLOAT: return VK_FORMAT_E5B9G9R9_UFLOAT_PACK32;
	case RAL_FORMAT_R32_SFLOAT: return VK_FORMAT_R32_SFLOAT;
	case RAL_FORMAT_R32G32_SFLOAT: return VK_FORMAT_R32G32_SFLOAT;
	case RAL_FORMAT_R32G32B32_SFLOAT: return VK_FORMAT_R32G32B32_SFLOAT;
	case RAL_FORMAT_R32G32B32A32_SFLOAT: return VK_FORMAT_R32G32B32A32_SFLOAT;
	case RAL_FORMAT_R8G8B8A8_UINT: return VK_FORMAT_R8G8B8A8_UINT;
	case RAL_FORMAT_D16_UNORM: return VK_FORMAT_D16_UNORM;
	case RAL_FORMAT_D24_UNORM_S8_UINT: return VK_FORMAT_D24_UNORM_S8_UINT;
	case RAL_FORMAT_D32_SFLOAT: return VK_FORMAT_D32_SFLOAT;
	case RAL_FORMAT_D32_SFLOAT_S8_UINT: return VK_FORMAT_D32_SFLOAT_S8_UINT;
	case RAL_FORMAT_D16_UNORM_S8_UINT: return VK_FORMAT_D16_UNORM_S8_UINT;
	case RAL_FORMAT_X8_D24_UNORM: return VK_FORMAT_X8_D24_UNORM_PACK32;
	case RAL_FORMAT_BC1_RGBA_UNORM: return VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
	case RAL_FORMAT_BC1_RGBA_SRGB: return VK_FORMAT_BC1_RGBA_SRGB_BLOCK;
	case RAL_FORMAT_BC3_UNORM: return VK_FORMAT_BC3_UNORM_BLOCK;
	case RAL_FORMAT_BC3_SRGB: return VK_FORMAT_BC3_SRGB_BLOCK;
	case RAL_FORMAT_BC4_UNORM: return VK_FORMAT_BC4_UNORM_BLOCK;
	case RAL_FORMAT_BC5_UNORM: return VK_FORMAT_BC5_UNORM_BLOCK;
	case RAL_FORMAT_BC6H_UFLOAT: return VK_FORMAT_BC6H_UFLOAT_BLOCK;
	case RAL_FORMAT_BC7_UNORM: return VK_FORMAT_BC7_UNORM_BLOCK;
	case RAL_FORMAT_BC7_SRGB: return VK_FORMAT_BC7_SRGB_BLOCK;
	case RAL_FORMAT_BC1_RGB_UNORM: return VK_FORMAT_BC1_RGB_UNORM_BLOCK;
	case RAL_FORMAT_BC1_RGB_SRGB: return VK_FORMAT_BC1_RGB_SRGB_BLOCK;
	case RAL_FORMAT_BC2_UNORM: return VK_FORMAT_BC2_UNORM_BLOCK;
	case RAL_FORMAT_BC2_SRGB: return VK_FORMAT_BC2_SRGB_BLOCK;
	case RAL_FORMAT_BC4_SNORM: return VK_FORMAT_BC4_SNORM_BLOCK;
	case RAL_FORMAT_BC5_SNORM: return VK_FORMAT_BC5_SNORM_BLOCK;
	case RAL_FORMAT_BC6H_SFLOAT: return VK_FORMAT_BC6H_SFLOAT_BLOCK;
	case RAL_FORMAT_ASTC_4x4_UNORM: return VK_FORMAT_ASTC_4x4_UNORM_BLOCK;
	case RAL_FORMAT_ASTC_4x4_SRGB: return VK_FORMAT_ASTC_4x4_SRGB_BLOCK;
	case RAL_FORMAT_ETC2_R8G8B8A8_UNORM: return VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK;
	case RAL_FORMAT_ETC2_R8G8B8A8_SRGB: return VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK;
	default: return VK_FORMAT_UNDEFINED;
	}
}

VkColorSpaceKHR ralVk_TranslateColorSpace( ralColorSpace_t cs ) {
	switch ( cs ) {
	case RAL_COLORSPACE_EXTENDED_SRGB_LINEAR: return VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT;
	case RAL_COLORSPACE_HDR10_ST2084: return VK_COLOR_SPACE_HDR10_ST2084_EXT;
	case RAL_COLORSPACE_HDR10_HLG: return VK_COLOR_SPACE_HDR10_HLG_EXT;
	case RAL_COLORSPACE_DISPLAY_P3: return VK_COLOR_SPACE_DISPLAY_P3_NONLINEAR_EXT;
	default: return VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
	}
}

VkPresentModeKHR ralVk_TranslatePresentMode( ralPresentMode_t pm ) {
	switch ( pm ) {
	case RAL_PRESENT_MAILBOX: return VK_PRESENT_MODE_MAILBOX_KHR;
	case RAL_PRESENT_IMMEDIATE: return VK_PRESENT_MODE_IMMEDIATE_KHR;
	case RAL_PRESENT_FIFO_RELAXED: return VK_PRESENT_MODE_FIFO_RELAXED_KHR;
	case RAL_PRESENT_FIFO_LATEST_READY: return VK_PRESENT_MODE_FIFO_LATEST_READY_EXT;
	default: return VK_PRESENT_MODE_FIFO_KHR;
	}
}

VkExtent2D ralVk_TranslateShadingRate( ralFragmentShadingRate_t rate ) {
	VkExtent2D extent = { 1, 1 };
	switch ( rate ) {
	case RAL_SHADING_RATE_2x2: extent.width = 2; extent.height = 2; break;
	case RAL_SHADING_RATE_2x4: extent.width = 2; extent.height = 4; break;
	case RAL_SHADING_RATE_4x2: extent.width = 4; extent.height = 2; break;
	case RAL_SHADING_RATE_4x4: extent.width = 4; extent.height = 4; break;
	default: break;
	}
	return extent;
}

VkAttachmentLoadOp ralVk_TranslateLoadOp( ralLoadOp_t op ) {
	switch ( op ) {
	case RAL_LOAD_OP_LOAD:  return VK_ATTACHMENT_LOAD_OP_LOAD;
	case RAL_LOAD_OP_CLEAR: return VK_ATTACHMENT_LOAD_OP_CLEAR;
	case RAL_LOAD_OP_DONT_CARE:
	default:                return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	}
}

VkAttachmentStoreOp ralVk_TranslateStoreOp( ralStoreOp_t op ) {
	return ( op == RAL_STORE_OP_STORE ) ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE;
}

VkAccessFlags ralVk_TranslateStageAccess( ralPipelineStageFlags_t stage ) {
	VkAccessFlags access = 0;
	if ( stage & RAL_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT )
		access |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
	if ( stage & ( RAL_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | RAL_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT ) )
		access |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
	if ( stage & ( RAL_PIPELINE_STAGE_COMPUTE_SHADER_BIT | RAL_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
	             | RAL_PIPELINE_STAGE_VERTEX_SHADER_BIT ) )
		access |= VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
	if ( stage & RAL_PIPELINE_STAGE_TRANSFER_BIT )
		access |= VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
	return access;
}

ralVkBarrierTranslation_t ralVk_TranslateBarrierScope( ralBarrierScope_t scope ) {
	ralVkBarrierTranslation_t out = {
		VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
		VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
		VK_ACCESS_MEMORY_WRITE_BIT,
		VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT
	};
	switch ( scope ) {
	case RAL_BARRIER_COMPUTE_TO_GRAPHICS:
		out.srcStage  = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
		out.dstStage  = VK_PIPELINE_STAGE_VERTEX_INPUT_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT
		              | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;
		out.srcAccess = VK_ACCESS_SHADER_WRITE_BIT;
		out.dstAccess = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT | VK_ACCESS_INDEX_READ_BIT
		              | VK_ACCESS_UNIFORM_READ_BIT | VK_ACCESS_SHADER_READ_BIT
		              | VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
		break;
	case RAL_BARRIER_COMPUTE_TO_COMPUTE:
		out.srcStage  = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
		out.dstStage  = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
		out.srcAccess = VK_ACCESS_SHADER_WRITE_BIT;
		out.dstAccess = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
		break;
	case RAL_BARRIER_COMPUTE_TO_TRANSFER:
		out.srcStage  = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
		out.dstStage  = VK_PIPELINE_STAGE_TRANSFER_BIT;
		out.srcAccess = VK_ACCESS_SHADER_WRITE_BIT;
		out.dstAccess = VK_ACCESS_TRANSFER_READ_BIT;
		break;
	case RAL_BARRIER_GRAPHICS_TO_COMPUTE:
		out.srcStage  = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
		              | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		out.dstStage  = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
		out.srcAccess = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		out.dstAccess = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
		break;
	case RAL_BARRIER_TRANSFER_TO_GRAPHICS:
		out.srcStage  = VK_PIPELINE_STAGE_TRANSFER_BIT;
		out.dstStage  = VK_PIPELINE_STAGE_VERTEX_INPUT_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT
		              | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		out.srcAccess = VK_ACCESS_TRANSFER_WRITE_BIT;
		out.dstAccess = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT | VK_ACCESS_INDEX_READ_BIT
		              | VK_ACCESS_UNIFORM_READ_BIT | VK_ACCESS_SHADER_READ_BIT;
		break;
	case RAL_BARRIER_COLOR_ATTACHMENT_TO_FRAGMENT:
		out.srcStage  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		out.dstStage  = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		out.srcAccess = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		out.dstAccess = VK_ACCESS_SHADER_READ_BIT;
		break;
	case RAL_BARRIER_INDIRECT:
		out.srcStage  = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
		out.dstStage  = VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;
		out.srcAccess = VK_ACCESS_SHADER_WRITE_BIT;
		out.dstAccess = VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
		break;
	case RAL_BARRIER_ALL:
	default:
		break;
	}
	return out;
}

ralVkLayoutTranslation_t ralVk_TranslateSourceLayout( VkImageLayout layout ) {
	ralVkLayoutTranslation_t out = { VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0 };
	switch ( layout ) {
	case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
		out.stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		out.access = VK_ACCESS_TRANSFER_WRITE_BIT;
		break;
	case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
		out.stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		out.access = VK_ACCESS_TRANSFER_READ_BIT;
		break;
	case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
		out.stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		out.access = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		break;
	case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
		out.stage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		out.access = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		break;
	case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
		out.stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		out.access = VK_ACCESS_SHADER_READ_BIT;
		break;
	default:
		break;
	}
	return out;
}

ralVkLayoutTranslation_t ralVk_TranslateDestinationLayout( VkImageLayout layout ) {
	ralVkLayoutTranslation_t out = {
		VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
		VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT
	};
	if ( layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL ) {
		out.stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		out.access = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	} else if ( layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL ) {
		out.stage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		out.access = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	}
	return out;
}

qboolean ralVk_TranslateBufferResourceState( const ralResourceState_t *state,
	                                          ralVkResourceStateTranslation_t *out ) {
	ralVkResourceStateTranslation_t candidate = {
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, VK_IMAGE_LAYOUT_UNDEFINED
	};
	if ( !state || !out )
		return qfalse;
	switch ( state->usage ) {
	case RAL_RESOURCE_USAGE_UNDEFINED:
		if ( state->shaderStages != 0 ) return qfalse;
		break;
	case RAL_RESOURCE_USAGE_COPY_SOURCE:
		if ( state->shaderStages != 0 ) return qfalse;
		candidate.stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		candidate.access = VK_ACCESS_TRANSFER_READ_BIT;
		break;
	case RAL_RESOURCE_USAGE_COPY_DESTINATION:
		if ( state->shaderStages != 0 ) return qfalse;
		candidate.stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		candidate.access = VK_ACCESS_TRANSFER_WRITE_BIT;
		break;
	case RAL_RESOURCE_USAGE_VERTEX_BUFFER:
		if ( state->shaderStages != 0 ) return qfalse;
		candidate.stage = VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
		candidate.access = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
		break;
	case RAL_RESOURCE_USAGE_INDEX_BUFFER:
		if ( state->shaderStages != 0 ) return qfalse;
		candidate.stage = VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
		candidate.access = VK_ACCESS_INDEX_READ_BIT;
		break;
	case RAL_RESOURCE_USAGE_INDIRECT_BUFFER:
		if ( state->shaderStages != 0 ) return qfalse;
		candidate.stage = VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;
		candidate.access = VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
		break;
	case RAL_RESOURCE_USAGE_UNIFORM_BUFFER:
		if ( !ralVk_TranslateShaderStages( state->shaderStages, &candidate.stage ) ) return qfalse;
		candidate.access = VK_ACCESS_UNIFORM_READ_BIT;
		break;
	case RAL_RESOURCE_USAGE_STORAGE_READ:
	case RAL_RESOURCE_USAGE_STORAGE_WRITE:
	case RAL_RESOURCE_USAGE_STORAGE_READ_WRITE:
		if ( !ralVk_TranslateShaderStages( state->shaderStages, &candidate.stage ) ) return qfalse;
		candidate.access = state->usage == RAL_RESOURCE_USAGE_STORAGE_READ
			? VK_ACCESS_SHADER_READ_BIT
			: state->usage == RAL_RESOURCE_USAGE_STORAGE_WRITE
			? VK_ACCESS_SHADER_WRITE_BIT
			: VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
		break;
	case RAL_RESOURCE_USAGE_HOST_READ:
	case RAL_RESOURCE_USAGE_HOST_WRITE:
		if ( state->shaderStages != 0 ) return qfalse;
		candidate.stage = VK_PIPELINE_STAGE_HOST_BIT;
		candidate.access = state->usage == RAL_RESOURCE_USAGE_HOST_READ
			? VK_ACCESS_HOST_READ_BIT : VK_ACCESS_HOST_WRITE_BIT;
		break;
	default:
		return qfalse;
	}
	*out = candidate;
	return qtrue;
}

qboolean ralVk_TranslateTextureResourceState( const ralResourceState_t *state,
	                                           ralVkResourceStateTranslation_t *out ) {
	ralVkResourceStateTranslation_t candidate = {
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, VK_IMAGE_LAYOUT_UNDEFINED
	};
	if ( !state || !out )
		return qfalse;
	switch ( state->usage ) {
	case RAL_RESOURCE_USAGE_UNDEFINED:
		if ( state->shaderStages != 0 ) return qfalse;
		break;
	case RAL_RESOURCE_USAGE_COPY_SOURCE:
	case RAL_RESOURCE_USAGE_COPY_DESTINATION:
		if ( state->shaderStages != 0 ) return qfalse;
		candidate.stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		candidate.access = state->usage == RAL_RESOURCE_USAGE_COPY_SOURCE
			? VK_ACCESS_TRANSFER_READ_BIT : VK_ACCESS_TRANSFER_WRITE_BIT;
		candidate.layout = state->usage == RAL_RESOURCE_USAGE_COPY_SOURCE
			? VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL : VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		break;
	case RAL_RESOURCE_USAGE_SAMPLED_TEXTURE:
		if ( !ralVk_TranslateShaderStages( state->shaderStages, &candidate.stage ) ) return qfalse;
		candidate.access = VK_ACCESS_SHADER_READ_BIT;
		candidate.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		break;
	case RAL_RESOURCE_USAGE_STORAGE_READ:
	case RAL_RESOURCE_USAGE_STORAGE_WRITE:
	case RAL_RESOURCE_USAGE_STORAGE_READ_WRITE:
		if ( !ralVk_TranslateShaderStages( state->shaderStages, &candidate.stage ) ) return qfalse;
		candidate.access = state->usage == RAL_RESOURCE_USAGE_STORAGE_READ
			? VK_ACCESS_SHADER_READ_BIT
			: state->usage == RAL_RESOURCE_USAGE_STORAGE_WRITE
			? VK_ACCESS_SHADER_WRITE_BIT
			: VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
		candidate.layout = VK_IMAGE_LAYOUT_GENERAL;
		break;
	case RAL_RESOURCE_USAGE_COLOR_ATTACHMENT:
		if ( state->shaderStages != 0 ) return qfalse;
		candidate.stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		candidate.access = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		candidate.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		break;
	case RAL_RESOURCE_USAGE_DEPTH_STENCIL_READ:
	case RAL_RESOURCE_USAGE_DEPTH_STENCIL_WRITE:
		if ( state->shaderStages != 0 ) return qfalse;
		candidate.stage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		candidate.access = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
		candidate.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
		if ( state->usage == RAL_RESOURCE_USAGE_DEPTH_STENCIL_WRITE ) {
			candidate.access |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			candidate.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		}
		break;
	case RAL_RESOURCE_USAGE_PRESENT:
		if ( state->shaderStages != 0 ) return qfalse;
		candidate.stage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		candidate.layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		break;
	default:
		return qfalse;
	}
	*out = candidate;
	return qtrue;
}

qboolean ralVk_TranslateTextureViewAspect( int aspect,
		VkImageAspectFlags available, VkImageAspectFlags *out ) {
	VkImageAspectFlags candidate;
	if ( !out || available == 0 ) return qfalse;
	switch ( aspect ) {
	case RAL_TEXTURE_VIEW_ASPECT_ALL:
		candidate = available;
		break;
	case RAL_TEXTURE_VIEW_ASPECT_DEPTH_ONLY:
		if ( !( available & VK_IMAGE_ASPECT_DEPTH_BIT ) ) return qfalse;
		candidate = VK_IMAGE_ASPECT_DEPTH_BIT;
		break;
	case RAL_TEXTURE_VIEW_ASPECT_STENCIL_ONLY:
		if ( !( available & VK_IMAGE_ASPECT_STENCIL_BIT ) ) return qfalse;
		candidate = VK_IMAGE_ASPECT_STENCIL_BIT;
		break;
	default:
		return qfalse;
	}
	*out = candidate;
	return qtrue;
}

qboolean ralVk_TranslateTextureCopyAspect(
		ralTextureAspectFlags_t requested, VkImageAspectFlags available,
		VkImageAspectFlags *out ) {
	const ralTextureAspectFlags_t known = RAL_TEXTURE_ASPECT_COLOR
		| RAL_TEXTURE_ASPECT_DEPTH | RAL_TEXTURE_ASPECT_STENCIL;
	VkImageAspectFlags candidate = 0;
	if ( !out || available == 0u || ( requested & ~known ) != 0u )
		return qfalse;
	if ( requested == 0u ) {
		candidate = available;
	} else {
		if ( requested & RAL_TEXTURE_ASPECT_COLOR )
			candidate |= VK_IMAGE_ASPECT_COLOR_BIT;
		if ( requested & RAL_TEXTURE_ASPECT_DEPTH )
			candidate |= VK_IMAGE_ASPECT_DEPTH_BIT;
		if ( requested & RAL_TEXTURE_ASPECT_STENCIL )
			candidate |= VK_IMAGE_ASPECT_STENCIL_BIT;
	}
	// Vulkan and WebGPU buffer-texture copies both select exactly one plane.
	if ( candidate == 0u || ( candidate & ~available ) != 0u
			|| ( candidate & ( candidate - 1u ) ) != 0u ) return qfalse;
	*out = candidate;
	return qtrue;
}

qboolean ralVk_PublishAdoptedTextureResourceState( ralTexture_t *texture,
		const ralResourceState_t *state, ralQueueType_t ownerQueue ) {
	ralVkResourceStateTranslation_t native;
	if ( !texture || !state || texture->ownsImage || texture->portableStateKnown
	  || texture->queueTransfer.pending.ready
	  || ownerQueue < RAL_QUEUE_GRAPHICS || ownerQueue > RAL_QUEUE_TRANSFER
	  || !Ral_ResourceStateValidForTexture( state )
	  || !ralVk_TranslateTextureResourceState( state, &native ) ) return qfalse;
	texture->portableState = *state;
	texture->portableOwnerQueue = ownerQueue;
	texture->currentLayout = native.layout;
	texture->portableStateKnown = qtrue;
	return qtrue;
}

qboolean ralVk_PublishAttachmentResourceState( ralCommandBuffer_t *command,
		                                             ralTexture_t *texture,
		                                             VkImageLayout attachmentLayout ) {
	ralResourceState_t candidate = { RAL_RESOURCE_USAGE_UNDEFINED, 0u };
	if ( !command || !texture || !command->backend
			|| texture->backend != command->backend
			|| texture->image == VK_NULL_HANDLE
			|| command->queue != RAL_QUEUE_GRAPHICS
			|| texture->currentLayout != attachmentLayout ) return qfalse;
	if ( attachmentLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL )
		candidate.usage = RAL_RESOURCE_USAGE_COLOR_ATTACHMENT;
	else if ( attachmentLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL )
		candidate.usage = RAL_RESOURCE_USAGE_DEPTH_STENCIL_WRITE;
	else
		return qfalse;
	texture->portableState = candidate;
	texture->portableOwnerQueue = command->queue;
	texture->portableStateKnown = qtrue;
	return qtrue;
}
