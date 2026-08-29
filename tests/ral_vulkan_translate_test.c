// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_vulkan_translate.h"
#include "ral_vulkan_internal.h"
#include <stdio.h>
#include <string.h>

#define CHECK(expr) do { if ( !(expr) ) { \
	fprintf( stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr ); return 1; \
} } while ( 0 )

int main( void ) {
	static const VkFormat formats[ RAL_FORMAT_COUNT ] = {
		VK_FORMAT_UNDEFINED,
		VK_FORMAT_R8_UNORM, VK_FORMAT_R8G8_UNORM,
		VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_R8G8B8A8_SRGB,
		VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_B8G8R8A8_SRGB,
		VK_FORMAT_A2B10G10R10_UNORM_PACK32, VK_FORMAT_A2R10G10B10_UNORM_PACK32,
		VK_FORMAT_B5G6R5_UNORM_PACK16, VK_FORMAT_R5G6B5_UNORM_PACK16,
		VK_FORMAT_R16_UNORM, VK_FORMAT_R16_SFLOAT, VK_FORMAT_R16G16_SFLOAT,
		VK_FORMAT_R16G16B16A16_SFLOAT, VK_FORMAT_R16G16B16A16_UNORM,
		VK_FORMAT_B10G11R11_UFLOAT_PACK32, VK_FORMAT_R32_SFLOAT,
		VK_FORMAT_R32G32_SFLOAT, VK_FORMAT_R32G32B32_SFLOAT,
		VK_FORMAT_R32G32B32A32_SFLOAT, VK_FORMAT_R8G8B8A8_UINT,
		VK_FORMAT_D16_UNORM, VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D32_SFLOAT,
		VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D16_UNORM_S8_UINT,
		VK_FORMAT_X8_D24_UNORM_PACK32,
		VK_FORMAT_BC1_RGBA_UNORM_BLOCK, VK_FORMAT_BC1_RGBA_SRGB_BLOCK,
		VK_FORMAT_BC3_UNORM_BLOCK, VK_FORMAT_BC3_SRGB_BLOCK,
		VK_FORMAT_BC4_UNORM_BLOCK, VK_FORMAT_BC5_UNORM_BLOCK,
		VK_FORMAT_BC6H_UFLOAT_BLOCK, VK_FORMAT_BC7_UNORM_BLOCK,
		VK_FORMAT_BC7_SRGB_BLOCK, VK_FORMAT_ASTC_4x4_UNORM_BLOCK,
		VK_FORMAT_ASTC_4x4_SRGB_BLOCK, VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK,
		VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK,
		VK_FORMAT_BC1_RGB_UNORM_BLOCK, VK_FORMAT_BC1_RGB_SRGB_BLOCK,
		VK_FORMAT_BC2_UNORM_BLOCK, VK_FORMAT_BC2_SRGB_BLOCK,
		VK_FORMAT_BC4_SNORM_BLOCK, VK_FORMAT_BC5_SNORM_BLOCK,
		VK_FORMAT_BC6H_SFLOAT_BLOCK,
		/* Append-only native upload/source formats. */
		VK_FORMAT_R8G8B8_UNORM, VK_FORMAT_B4G4R4A4_UNORM_PACK16,
		VK_FORMAT_A1R5G5B5_UNORM_PACK16,
		/* Append-only directional static-lighting product formats. */
		VK_FORMAT_E5B9G9R9_UFLOAT_PACK32, VK_FORMAT_R16G16_SNORM
	};
	static const VkColorSpaceKHR colorSpaces[] = {
		VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
		VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT,
		VK_COLOR_SPACE_HDR10_ST2084_EXT,
		VK_COLOR_SPACE_HDR10_HLG_EXT,
		VK_COLOR_SPACE_DISPLAY_P3_NONLINEAR_EXT
	};
	static const VkPresentModeKHR presentModes[] = {
		VK_PRESENT_MODE_FIFO_KHR, VK_PRESENT_MODE_MAILBOX_KHR,
		VK_PRESENT_MODE_IMMEDIATE_KHR, VK_PRESENT_MODE_FIFO_RELAXED_KHR,
		VK_PRESENT_MODE_FIFO_LATEST_READY_EXT
	};
	static const uint32_t rates[][2] = { {1,1}, {2,2}, {2,4}, {4,2}, {4,4} };
	uint32_t i;
	const ralResourceState_t shaderRead = {
		RAL_RESOURCE_USAGE_STORAGE_READ, RAL_STAGE_VERTEX | RAL_STAGE_COMPUTE
	};

	CHECK( sizeof( formats ) / sizeof( formats[0] ) == RAL_FORMAT_COUNT );
	for ( i = 0; i < RAL_FORMAT_COUNT; ++i )
		CHECK( ralVk_TranslateFormat( (ralFormat_t)i ) == formats[i] );
	CHECK( ralVk_TranslateFormat( (ralFormat_t)-1 ) == VK_FORMAT_UNDEFINED );
	CHECK( ralVk_TranslateFormat( RAL_FORMAT_COUNT ) == VK_FORMAT_UNDEFINED );

	for ( i = 0; i < sizeof( colorSpaces ) / sizeof( colorSpaces[0] ); ++i )
		CHECK( ralVk_TranslateColorSpace( (ralColorSpace_t)i ) == colorSpaces[i] );
	CHECK( ralVk_TranslateColorSpace( (ralColorSpace_t)99 ) == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR );

	for ( i = 0; i < sizeof( presentModes ) / sizeof( presentModes[0] ); ++i )
		CHECK( ralVk_TranslatePresentMode( (ralPresentMode_t)i ) == presentModes[i] );
	CHECK( ralVk_TranslatePresentMode( (ralPresentMode_t)99 ) == VK_PRESENT_MODE_FIFO_KHR );

	for ( i = 0; i < sizeof( rates ) / sizeof( rates[0] ); ++i ) {
		VkExtent2D extent = ralVk_TranslateShadingRate( (ralFragmentShadingRate_t)i );
		CHECK( extent.width == rates[i][0] && extent.height == rates[i][1] );
	}
	{
		VkExtent2D extent = ralVk_TranslateShadingRate( (ralFragmentShadingRate_t)99 );
		CHECK( extent.width == 1 && extent.height == 1 );
	}

	CHECK( ralVk_TranslateLoadOp( RAL_LOAD_OP_LOAD ) == VK_ATTACHMENT_LOAD_OP_LOAD );
	CHECK( ralVk_TranslateLoadOp( RAL_LOAD_OP_CLEAR ) == VK_ATTACHMENT_LOAD_OP_CLEAR );
	CHECK( ralVk_TranslateLoadOp( RAL_LOAD_OP_DONT_CARE ) == VK_ATTACHMENT_LOAD_OP_DONT_CARE );
	CHECK( ralVk_TranslateLoadOp( (ralLoadOp_t)99 ) == VK_ATTACHMENT_LOAD_OP_DONT_CARE );
	CHECK( ralVk_TranslateStoreOp( RAL_STORE_OP_STORE ) == VK_ATTACHMENT_STORE_OP_STORE );
	CHECK( ralVk_TranslateStoreOp( RAL_STORE_OP_DONT_CARE ) == VK_ATTACHMENT_STORE_OP_DONT_CARE );
	CHECK( ralVk_TranslateStoreOp( (ralStoreOp_t)99 ) == VK_ATTACHMENT_STORE_OP_DONT_CARE );

	CHECK( ralVk_TranslateStageAccess( RAL_PIPELINE_STAGE_TOP_OF_PIPE_BIT ) == 0 );
	CHECK( ralVk_TranslateStageAccess( RAL_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT )
	       == ( VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT ) );
	CHECK( ralVk_TranslateStageAccess( RAL_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | RAL_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT )
	       == ( VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT ) );
	CHECK( ralVk_TranslateStageAccess( RAL_PIPELINE_STAGE_VERTEX_SHADER_BIT | RAL_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | RAL_PIPELINE_STAGE_COMPUTE_SHADER_BIT )
	       == ( VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT ) );
	CHECK( ralVk_TranslateStageAccess( RAL_PIPELINE_STAGE_TRANSFER_BIT )
	       == ( VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT ) );
	CHECK( ralVk_TranslateStageAccess( RAL_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | RAL_PIPELINE_STAGE_TRANSFER_BIT )
	       == ( VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
	          | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT ) );

	{
		static const ralVkBarrierTranslation_t barriers[] = {
			{ VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
			  VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT },
			{ VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			  VK_PIPELINE_STAGE_VERTEX_INPUT_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
			  VK_ACCESS_SHADER_WRITE_BIT,
			  VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT | VK_ACCESS_INDEX_READ_BIT | VK_ACCESS_UNIFORM_READ_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_INDIRECT_COMMAND_READ_BIT },
			{ VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			  VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT },
			{ VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
			  VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT },
			{ VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			  VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			  VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			  VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT },
			{ VK_PIPELINE_STAGE_TRANSFER_BIT,
			  VK_PIPELINE_STAGE_VERTEX_INPUT_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			  VK_ACCESS_TRANSFER_WRITE_BIT,
			  VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT | VK_ACCESS_INDEX_READ_BIT | VK_ACCESS_UNIFORM_READ_BIT | VK_ACCESS_SHADER_READ_BIT },
			{ VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			  VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			  VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			  VK_ACCESS_SHADER_READ_BIT },
			{ VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
			  VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_INDIRECT_COMMAND_READ_BIT }
		};
		for ( i = 0; i < sizeof( barriers ) / sizeof( barriers[0] ); ++i ) {
			ralVkBarrierTranslation_t got = ralVk_TranslateBarrierScope( (ralBarrierScope_t)i );
			CHECK( got.srcStage == barriers[i].srcStage );
			CHECK( got.dstStage == barriers[i].dstStage );
			CHECK( got.srcAccess == barriers[i].srcAccess );
			CHECK( got.dstAccess == barriers[i].dstAccess );
		}
		{
			ralVkBarrierTranslation_t invalid = ralVk_TranslateBarrierScope( (ralBarrierScope_t)99 );
			CHECK( invalid.srcStage == barriers[0].srcStage && invalid.dstStage == barriers[0].dstStage );
			CHECK( invalid.srcAccess == barriers[0].srcAccess && invalid.dstAccess == barriers[0].dstAccess );
		}
	}

	{
		ralVkLayoutTranslation_t t;
		t = ralVk_TranslateSourceLayout( VK_IMAGE_LAYOUT_UNDEFINED );
		CHECK( t.stage == VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT && t.access == 0 );
		t = ralVk_TranslateSourceLayout( VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL );
		CHECK( t.stage == VK_PIPELINE_STAGE_TRANSFER_BIT && t.access == VK_ACCESS_TRANSFER_WRITE_BIT );
		t = ralVk_TranslateSourceLayout( VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL );
		CHECK( t.stage == VK_PIPELINE_STAGE_TRANSFER_BIT && t.access == VK_ACCESS_TRANSFER_READ_BIT );
		t = ralVk_TranslateSourceLayout( VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL );
		CHECK( t.stage == VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
		       && t.access == ( VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT ) );
		t = ralVk_TranslateSourceLayout( VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL );
		CHECK( t.stage == ( VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT )
		       && t.access == ( VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT ) );
		t = ralVk_TranslateSourceLayout( VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );
		CHECK( t.stage == VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT && t.access == VK_ACCESS_SHADER_READ_BIT );

		t = ralVk_TranslateDestinationLayout( VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL );
		CHECK( t.stage == VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
		       && t.access == ( VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT ) );
		t = ralVk_TranslateDestinationLayout( VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL );
		CHECK( t.stage == ( VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT )
		       && t.access == ( VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT ) );
		t = ralVk_TranslateDestinationLayout( VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );
		CHECK( t.stage == VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT
		       && t.access == ( VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT ) );
	}

	CHECK( Ral_ResourceStateValidForBuffer( &shaderRead ) );
	CHECK( Ral_ResourceStateValidForTexture( &shaderRead ) );
	{
		typedef struct {
			ralResourceUsage_t usage;
			uint32_t stages;
			VkPipelineStageFlags vkStage;
			VkAccessFlags vkAccess;
		} BufferCase;
		static const BufferCase cases[] = {
			{ RAL_RESOURCE_USAGE_UNDEFINED, 0, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0 },
			{ RAL_RESOURCE_USAGE_COPY_SOURCE, 0, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT },
			{ RAL_RESOURCE_USAGE_COPY_DESTINATION, 0, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT },
			{ RAL_RESOURCE_USAGE_VERTEX_BUFFER, 0, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT },
			{ RAL_RESOURCE_USAGE_INDEX_BUFFER, 0, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, VK_ACCESS_INDEX_READ_BIT },
			{ RAL_RESOURCE_USAGE_INDIRECT_BUFFER, 0, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, VK_ACCESS_INDIRECT_COMMAND_READ_BIT },
			{ RAL_RESOURCE_USAGE_UNIFORM_BUFFER, RAL_STAGE_ALL,
			  VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			  VK_ACCESS_UNIFORM_READ_BIT },
			{ RAL_RESOURCE_USAGE_STORAGE_READ, RAL_STAGE_COMPUTE, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT },
			{ RAL_RESOURCE_USAGE_STORAGE_WRITE, RAL_STAGE_COMPUTE, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT },
			{ RAL_RESOURCE_USAGE_STORAGE_READ_WRITE, RAL_STAGE_COMPUTE, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			  VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT },
			{ RAL_RESOURCE_USAGE_HOST_READ, 0, VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_HOST_READ_BIT },
			{ RAL_RESOURCE_USAGE_HOST_WRITE, 0, VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_HOST_WRITE_BIT }
		};
		for ( i = 0; i < sizeof( cases ) / sizeof( cases[0] ); ++i ) {
			ralResourceState_t state = { cases[i].usage, cases[i].stages };
			ralVkResourceStateTranslation_t out;
			CHECK( Ral_ResourceStateValidForBuffer( &state ) );
			CHECK( ralVk_TranslateBufferResourceState( &state, &out ) );
			CHECK( out.stage == cases[i].vkStage && out.access == cases[i].vkAccess );
			CHECK( out.layout == VK_IMAGE_LAYOUT_UNDEFINED );
		}
	}
	{
		typedef struct {
			ralResourceUsage_t usage;
			uint32_t stages;
			VkPipelineStageFlags vkStage;
			VkAccessFlags vkAccess;
			VkImageLayout vkLayout;
		} TextureCase;
		static const TextureCase cases[] = {
			{ RAL_RESOURCE_USAGE_UNDEFINED, 0, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, VK_IMAGE_LAYOUT_UNDEFINED },
			{ RAL_RESOURCE_USAGE_COPY_SOURCE, 0, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL },
			{ RAL_RESOURCE_USAGE_COPY_DESTINATION, 0, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL },
			{ RAL_RESOURCE_USAGE_SAMPLED_TEXTURE, RAL_STAGE_FRAGMENT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
			{ RAL_RESOURCE_USAGE_STORAGE_READ, RAL_STAGE_COMPUTE, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_GENERAL },
			{ RAL_RESOURCE_USAGE_STORAGE_WRITE, RAL_STAGE_COMPUTE, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL },
			{ RAL_RESOURCE_USAGE_STORAGE_READ_WRITE, RAL_STAGE_COMPUTE, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			  VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL },
			{ RAL_RESOURCE_USAGE_COLOR_ATTACHMENT, 0, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			  VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL },
			{ RAL_RESOURCE_USAGE_DEPTH_STENCIL_READ, 0,
			  VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
			  VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL },
			{ RAL_RESOURCE_USAGE_DEPTH_STENCIL_WRITE, 0,
			  VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
			  VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
			  VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL },
			{ RAL_RESOURCE_USAGE_PRESENT, 0, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR }
		};
		for ( i = 0; i < sizeof( cases ) / sizeof( cases[0] ); ++i ) {
			ralResourceState_t state = { cases[i].usage, cases[i].stages };
			ralVkResourceStateTranslation_t out;
			CHECK( Ral_ResourceStateValidForTexture( &state ) );
			CHECK( ralVk_TranslateTextureResourceState( &state, &out ) );
			CHECK( out.stage == cases[i].vkStage && out.access == cases[i].vkAccess );
			CHECK( out.layout == cases[i].vkLayout );
		}
	}
	{
		ralResourceState_t state = { RAL_RESOURCE_USAGE_VERTEX_BUFFER, 0 };
		ralVkResourceStateTranslation_t out;
		memset( &out, 0xA5, sizeof( out ) );
		CHECK( ralVk_TranslateBufferResourceState( &state, &out ) );
		CHECK( out.stage == VK_PIPELINE_STAGE_VERTEX_INPUT_BIT );
		CHECK( out.access == VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT );
		CHECK( out.layout == VK_IMAGE_LAYOUT_UNDEFINED );
		state.usage = RAL_RESOURCE_USAGE_SAMPLED_TEXTURE;
		state.shaderStages = RAL_STAGE_FRAGMENT | RAL_STAGE_COMPUTE;
		CHECK( ralVk_TranslateTextureResourceState( &state, &out ) );
		CHECK( out.stage == ( VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT ) );
		CHECK( out.access == VK_ACCESS_SHADER_READ_BIT );
		CHECK( out.layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );
		state.usage = RAL_RESOURCE_USAGE_PRESENT;
		state.shaderStages = 0;
		CHECK( ralVk_TranslateTextureResourceState( &state, &out ) );
		CHECK( out.stage == VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT );
		CHECK( out.access == 0 );
		CHECK( out.layout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR );
	}
	{
		VkImageAspectFlags out = 0, before;
		const VkImageAspectFlags combined = VK_IMAGE_ASPECT_DEPTH_BIT
			| VK_IMAGE_ASPECT_STENCIL_BIT;
		CHECK( ralVk_TranslateTextureViewAspect(
			RAL_TEXTURE_VIEW_ASPECT_ALL, combined, &out ) && out == combined );
		CHECK( ralVk_TranslateTextureViewAspect(
			RAL_TEXTURE_VIEW_ASPECT_DEPTH_ONLY, combined, &out )
			&& out == VK_IMAGE_ASPECT_DEPTH_BIT );
		CHECK( ralVk_TranslateTextureViewAspect(
			RAL_TEXTURE_VIEW_ASPECT_STENCIL_ONLY, combined, &out )
			&& out == VK_IMAGE_ASPECT_STENCIL_BIT );
		before = out;
		CHECK( !ralVk_TranslateTextureViewAspect(
			RAL_TEXTURE_VIEW_ASPECT_DEPTH_ONLY, VK_IMAGE_ASPECT_COLOR_BIT, &out )
			&& out == before );
		CHECK( !ralVk_TranslateTextureViewAspect( 99, combined, &out )
			&& out == before );
	}
	{
		VkImageAspectFlags out = VK_IMAGE_ASPECT_COLOR_BIT, before;
		const VkImageAspectFlags combined = VK_IMAGE_ASPECT_DEPTH_BIT
			| VK_IMAGE_ASPECT_STENCIL_BIT;
		CHECK( ralVk_TranslateTextureCopyAspect(
			RAL_TEXTURE_ASPECT_DEPTH, combined, &out )
			&& out == VK_IMAGE_ASPECT_DEPTH_BIT );
		CHECK( ralVk_TranslateTextureCopyAspect(
			RAL_TEXTURE_ASPECT_STENCIL, combined, &out )
			&& out == VK_IMAGE_ASPECT_STENCIL_BIT );
		CHECK( ralVk_TranslateTextureCopyAspect(
			0u, VK_IMAGE_ASPECT_COLOR_BIT, &out )
			&& out == VK_IMAGE_ASPECT_COLOR_BIT );
		before = out;
		CHECK( !ralVk_TranslateTextureCopyAspect( 0u, combined, &out )
			&& out == before );
		CHECK( !ralVk_TranslateTextureCopyAspect(
			RAL_TEXTURE_ASPECT_DEPTH | RAL_TEXTURE_ASPECT_STENCIL,
			combined, &out ) && out == before );
		CHECK( !ralVk_TranslateTextureCopyAspect(
			RAL_TEXTURE_ASPECT_COLOR, combined, &out ) && out == before );
		CHECK( !ralVk_TranslateTextureCopyAspect( 1u << 7,
			VK_IMAGE_ASPECT_COLOR_BIT, &out ) && out == before );
	}
	{
		ralTexture_t texture, unchanged;
		ralResourceState_t state = { RAL_RESOURCE_USAGE_UNDEFINED, 0 };
		memset( &texture, 0, sizeof( texture ) );
		texture.image = (VkImage)(uintptr_t)0x700u;
		CHECK( ralVk_PublishAdoptedTextureResourceState(
			&texture, &state, RAL_QUEUE_GRAPHICS ) );
		CHECK( texture.portableStateKnown
			&& texture.portableState.usage == RAL_RESOURCE_USAGE_UNDEFINED
			&& texture.currentLayout == VK_IMAGE_LAYOUT_UNDEFINED
			&& texture.portableOwnerQueue == RAL_QUEUE_GRAPHICS );
		unchanged = texture;
		CHECK( !ralVk_PublishAdoptedTextureResourceState(
			&texture, &state, RAL_QUEUE_GRAPHICS )
			&& memcmp( &texture, &unchanged, sizeof( texture ) ) == 0 );
		texture = unchanged; texture.portableStateKnown = qfalse; texture.ownsImage = qtrue;
		unchanged = texture;
		CHECK( !ralVk_PublishAdoptedTextureResourceState(
			&texture, &state, RAL_QUEUE_GRAPHICS )
			&& memcmp( &texture, &unchanged, sizeof( texture ) ) == 0 );
	}
	{
		ralBackend_t backend, otherBackend;
		ralCommandBuffer_t command;
		ralTexture_t texture, unchanged;
		memset( &backend, 0, sizeof( backend ) );
		memset( &otherBackend, 0, sizeof( otherBackend ) );
		memset( &command, 0, sizeof( command ) );
		memset( &texture, 0, sizeof( texture ) );
		command.backend = &backend;
		command.queue = RAL_QUEUE_GRAPHICS;
		texture.backend = &backend;
		texture.image = (VkImage)(uintptr_t)0x701u;
		texture.currentLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		CHECK( ralVk_PublishAttachmentResourceState( &command, &texture,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL ) );
		CHECK( texture.portableStateKnown
			&& texture.portableState.usage == RAL_RESOURCE_USAGE_COLOR_ATTACHMENT
			&& texture.portableState.shaderStages == 0u
			&& texture.portableOwnerQueue == RAL_QUEUE_GRAPHICS );

		texture.currentLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		CHECK( ralVk_PublishAttachmentResourceState( &command, &texture,
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL ) );
		CHECK( texture.portableStateKnown
			&& texture.portableState.usage == RAL_RESOURCE_USAGE_DEPTH_STENCIL_WRITE );

		unchanged = texture;
		CHECK( !ralVk_PublishAttachmentResourceState( &command, &texture,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL ) );
		CHECK( memcmp( &texture, &unchanged, sizeof( texture ) ) == 0 );
		command.queue = RAL_QUEUE_COMPUTE;
		CHECK( !ralVk_PublishAttachmentResourceState( &command, &texture,
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL ) );
		CHECK( memcmp( &texture, &unchanged, sizeof( texture ) ) == 0 );
		command.queue = RAL_QUEUE_GRAPHICS;
		texture.backend = &otherBackend;
		CHECK( !ralVk_PublishAttachmentResourceState( &command, &texture,
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL ) );
	}
	{
		ralResourceState_t invalid = { RAL_RESOURCE_USAGE_VERTEX_BUFFER, RAL_STAGE_VERTEX };
		ralVkResourceStateTranslation_t before, after;
		memset( &before, 0x5A, sizeof( before ) );
		after = before;
		CHECK( !Ral_ResourceStateValidForBuffer( &invalid ) );
		CHECK( !ralVk_TranslateBufferResourceState( &invalid, &after ) );
		CHECK( memcmp( &before, &after, sizeof( before ) ) == 0 );
		invalid.usage = RAL_RESOURCE_USAGE_COUNT;
		invalid.shaderStages = 0;
		CHECK( !ralVk_TranslateTextureResourceState( &invalid, &after ) );
		CHECK( memcmp( &before, &after, sizeof( before ) ) == 0 );
	}
	{
		ralBufferTransition_t t;
		memset( &t, 0, sizeof( t ) );
		t.buffer = (ralBuffer_t *)(uintptr_t)1;
		t.offset = 64;
		t.size = 128;
		t.before.usage = RAL_RESOURCE_USAGE_COPY_DESTINATION;
		t.after = shaderRead;
		t.sourceQueue = RAL_QUEUE_TRANSFER;
		t.destinationQueue = RAL_QUEUE_GRAPHICS;
		CHECK( Ral_BufferTransitionValid( &t, 256 ) );
		// Logical queue classes remain valid even on a future single-queue WebGPU
		// backend; physical ownership policy is backend-private.
		t.sourceQueue = RAL_QUEUE_GRAPHICS;
		CHECK( Ral_BufferTransitionValid( &t, 256 ) );
		t.size = 193;
		CHECK( !Ral_BufferTransitionValid( &t, 256 ) );
		t.size = 128;
		t.destinationQueue = (ralQueueType_t)99;
		CHECK( !Ral_BufferTransitionValid( &t, 256 ) );
	}
	{
		ralTextureTransition_t t;
		memset( &t, 0, sizeof( t ) );
		t.texture = (ralTexture_t *)(uintptr_t)1;
		t.aspects = RAL_TEXTURE_ASPECT_COLOR;
		t.baseMipLevel = 1;
		t.mipLevelCount = 3;
		t.baseArrayLayer = 2;
		t.arrayLayerCount = 4;
		t.before.usage = RAL_RESOURCE_USAGE_COPY_DESTINATION;
		t.after.usage = RAL_RESOURCE_USAGE_SAMPLED_TEXTURE;
		t.after.shaderStages = RAL_STAGE_FRAGMENT;
		t.sourceQueue = RAL_QUEUE_TRANSFER;
		t.destinationQueue = RAL_QUEUE_GRAPHICS;
		CHECK( Ral_TextureTransitionValid( &t, 4, 6, RAL_TEXTURE_ASPECT_COLOR ) );
		t.mipLevelCount = 4;
		CHECK( !Ral_TextureTransitionValid( &t, 4, 6, RAL_TEXTURE_ASPECT_COLOR ) );
		t.mipLevelCount = 3;
		t.after.usage = RAL_RESOURCE_USAGE_DEPTH_STENCIL_WRITE;
		t.after.shaderStages = 0;
		CHECK( !Ral_TextureTransitionValid( &t, 4, 6, RAL_TEXTURE_ASPECT_COLOR ) );
		t.aspects = RAL_TEXTURE_ASPECT_DEPTH | RAL_TEXTURE_ASPECT_STENCIL;
		CHECK( Ral_TextureTransitionValid( &t, 4, 6,
			RAL_TEXTURE_ASPECT_DEPTH | RAL_TEXTURE_ASPECT_STENCIL ) );
	}

	puts( "PASS portable RAL resource-state validation and exact Vulkan lowering" );
	return 0;
}
