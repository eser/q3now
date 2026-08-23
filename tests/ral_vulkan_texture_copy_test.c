// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_vulkan_internal.h"
#include "ral_texture_upload.h"

#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if ( !(x) ) { \
	fprintf( stderr, "CHECK %s:%d: %s\n", __FILE__, __LINE__, #x ); return 1; \
} } while ( 0 )

static uint32_t copyCalls;
static VkBufferImageCopy captured;
static uint32_t uploadCalls, uploadRegionCount;
static VkBufferImageCopy capturedUpload[4];

static VKAPI_ATTR void VKAPI_CALL CopyImageToBuffer( VkCommandBuffer command,
		VkImage image, VkImageLayout layout, VkBuffer buffer,
		uint32_t count, const VkBufferImageCopy *regions ) {
	(void)command; (void)image; (void)buffer;
	if ( layout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL && count == 1u ) {
		copyCalls++;
		captured = regions[0];
	}
}

static VKAPI_ATTR void VKAPI_CALL CopyBufferToImage( VkCommandBuffer command,
		VkBuffer buffer, VkImage image, VkImageLayout layout,
		uint32_t count, const VkBufferImageCopy *regions ) {
	uint32_t i;
	(void)command; (void)buffer; (void)image;
	if ( layout != VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
			|| count > 4u ) return;
	uploadCalls++;
	uploadRegionCount = count;
	for ( i = 0u; i < count; ++i ) capturedUpload[i] = regions[i];
}

int main( void ) {
	ralBackend_t backend, otherBackend;
	ralCommandBuffer_t command;
	ralTexture_t texture;
	ralBuffer_t buffer;
	ralBufferTextureCopy_t copy, bad;
	memset( &backend, 0, sizeof( backend ) );
	memset( &otherBackend, 0, sizeof( otherBackend ) );
	memset( &command, 0, sizeof( command ) );
	memset( &texture, 0, sizeof( texture ) );
	memset( &buffer, 0, sizeof( buffer ) );
	backend.type = RAL_BACKEND_VULKAN;
	backend.vk.CmdCopyImageToBuffer = CopyImageToBuffer;
	backend.vk.CmdCopyBufferToImage = CopyBufferToImage;
	command.backend = &backend;
	command.cb = (VkCommandBuffer)(uintptr_t)0x10u;
	command.queue = RAL_QUEUE_GRAPHICS;
	command.state = RAL_VK_CMD_RECORDING;
	command.lifecycle.state = RAL_COMMAND_RECORDING;
	texture.backend = &backend;
	texture.image = (VkImage)(uintptr_t)0x20u;
	texture.ralFormat = RAL_FORMAT_R8G8B8A8_UNORM;
	texture.width = 17u; texture.height = 9u;
	texture.mipLevels = 1u; texture.arrayLayers = 1u;
	texture.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
	texture.portableStateKnown = qtrue;
	texture.portableState.usage = RAL_RESOURCE_USAGE_COPY_SOURCE;
	buffer.backend = &backend;
	buffer.buffer = (VkBuffer)(uintptr_t)0x30u;
	buffer.size = 256u * 9u;
	buffer.usage = RAL_BUFFER_TRANSFER_SRC | RAL_BUFFER_TRANSFER_DST;
	buffer.portableStateKnown = qtrue;
	buffer.portableState.usage = RAL_RESOURCE_USAGE_COPY_DESTINATION;
	Ral_BufferMapLifecycleInit( &buffer.mapLifecycle );
	memset( &copy, 0, sizeof( copy ) );
	copy.bytesPerRow = 256u;
	copy.rowsPerImage = 9u;
	copy.aspects = RAL_TEXTURE_ASPECT_COLOR;
	copy.imageRect.width = 17u;
	copy.imageRect.height = 9u;
	CHECK( Ral_CmdCopyTextureToBuffer( &command, &texture, &buffer, &copy ) );
	CHECK( copyCalls == 1u && captured.bufferOffset == 0u
		&& captured.bufferRowLength == 64u
		&& captured.bufferImageHeight == 9u
		&& captured.imageExtent.width == 17u
		&& captured.imageExtent.height == 9u );
#define REJECT(statement) do { bad = copy; statement; copyCalls = 0u; \
	CHECK( !Ral_CmdCopyTextureToBuffer( &command, &texture, &buffer, &bad ) \
		&& copyCalls == 0u ); } while ( 0 )
	REJECT( bad.bytesPerRow = 255u );
	REJECT( bad.rowsPerImage = 8u );
	REJECT( bad.bufferOffset = 1u );
	REJECT( bad.imageRect.width = 18u );
	REJECT( bad.aspects = RAL_TEXTURE_ASPECT_DEPTH );
	buffer.size--; REJECT( (void)0 ); buffer.size++;
	buffer.backend = &otherBackend; REJECT( (void)0 ); buffer.backend = &backend;
	texture.ralFormat = RAL_FORMAT_BC1_RGBA_UNORM; REJECT( (void)0 );
#undef REJECT

	/* Multi-region upload: the complete portable batch validates before one
	 * native command, preserving mip/layer operands. */
	{
		ralBufferTextureCopy_t uploads[2], one, badUpload;
		memset( uploads, 0, sizeof( uploads ) );
		texture.ralFormat = RAL_FORMAT_R8G8B8A8_UNORM;
		texture.type = RAL_TEXTURE_2D_ARRAY;
		texture.width = 16u; texture.height = 8u;
		texture.depthOrArrayLayers = 2u;
		texture.mipLevels = 2u; texture.arrayLayers = 2u;
		texture.sampleCount = 1u;
		texture.usage = RAL_TEXTURE_USAGE_TRANSFER_DST;
		texture.currentLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		texture.portableStateKnown = qtrue;
		texture.portableState.usage = RAL_RESOURCE_USAGE_COPY_DESTINATION;
		texture.portableOwnerQueue = RAL_QUEUE_GRAPHICS;
		buffer.size = 4096u;
		buffer.portableState.usage = RAL_RESOURCE_USAGE_COPY_SOURCE;
		buffer.portableOwnerQueue = RAL_QUEUE_GRAPHICS;
		uploads[0].aspects = RAL_TEXTURE_ASPECT_COLOR;
		uploads[0].imageRect.width = 16u;
		uploads[0].imageRect.height = 8u;
		uploads[1].bufferOffset = 512u;
		uploads[1].mipLevel = 1u;
		uploads[1].arrayLayer = 1u;
		uploads[1].aspects = RAL_TEXTURE_ASPECT_COLOR;
		uploads[1].imageRect.width = 8u;
		uploads[1].imageRect.height = 4u;
		CHECK( Ral_CmdCopyBufferToTextureRegionsExact( &command, &buffer,
			&texture, 2u, uploads ) );
		CHECK( uploadCalls == 1u && uploadRegionCount == 2u
			&& capturedUpload[0].imageSubresource.mipLevel == 0u
			&& capturedUpload[1].imageSubresource.mipLevel == 1u
			&& capturedUpload[1].imageSubresource.baseArrayLayer == 1u );
		one = uploads[0];
#define REJECT_UPLOAD(statement) do { badUpload = one; statement; \
		uploadCalls = 0u; CHECK( !Ral_CmdCopyBufferToTextureRegionsExact( \
			&command, &buffer, &texture, 1u, &badUpload ) \
			&& uploadCalls == 0u ); } while ( 0 )
		REJECT_UPLOAD( badUpload.imageRect.x = -1 );
		REJECT_UPLOAD( badUpload.imageRect.width = 17u );
		REJECT_UPLOAD( badUpload.mipLevel = 2u );
		REJECT_UPLOAD( badUpload.arrayLayer = 2u );
		REJECT_UPLOAD( badUpload.bytesPerRow = 64u );
		REJECT_UPLOAD( badUpload.bufferOffset = 2u );
		REJECT_UPLOAD( badUpload.aspects = RAL_TEXTURE_ASPECT_DEPTH );
		texture.currentLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		REJECT_UPLOAD( (void)0 );
		texture.currentLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		texture.portableState.usage = RAL_RESOURCE_USAGE_SAMPLED_TEXTURE;
		REJECT_UPLOAD( (void)0 );
		texture.portableState.usage = RAL_RESOURCE_USAGE_COPY_DESTINATION;
		buffer.portableState.usage = RAL_RESOURCE_USAGE_HOST_WRITE;
		REJECT_UPLOAD( (void)0 );
		buffer.portableState.usage = RAL_RESOURCE_USAGE_COPY_SOURCE;
		command.renderingActive = qtrue; REJECT_UPLOAD( (void)0 );
		command.renderingActive = qfalse;
		command.lifecycle.state = RAL_COMMAND_IDLE; REJECT_UPLOAD( (void)0 );
		command.lifecycle.state = RAL_COMMAND_RECORDING;
		buffer.backend = &otherBackend; REJECT_UPLOAD( (void)0 );
		buffer.backend = &backend;
		CHECK( !Ral_CmdCopyBufferToTextureRegionsExact( &command, &buffer,
			&texture, 0u, uploads ) );
		CHECK( !Ral_CmdCopyBufferToTextureRegionsExact( &command, &buffer,
			&texture, 97u, uploads ) );

		/* Block-compressed edge rules and footprint. */
		texture.type = RAL_TEXTURE_2D;
		texture.ralFormat = RAL_FORMAT_BC1_RGBA_UNORM;
		texture.width = texture.height = 8u;
		texture.depthOrArrayLayers = texture.arrayLayers = 1u;
		texture.mipLevels = 1u;
		buffer.size = 32u;
		memset( &one, 0, sizeof( one ) );
		one.aspects = RAL_TEXTURE_ASPECT_COLOR;
		one.imageRect.width = one.imageRect.height = 8u;
		uploadCalls = 0u;
		CHECK( Ral_CmdCopyBufferToTextureRegionsExact( &command, &buffer,
			&texture, 1u, &one ) && uploadCalls == 1u );
		REJECT_UPLOAD( badUpload.imageRect.x = 1 );
		REJECT_UPLOAD( badUpload.imageRect.width = 6u );

		/* Three-byte legacy source pixels retain their exact copy footprint. */
		texture.ralFormat = RAL_FORMAT_R8G8B8_UNORM;
		texture.width = 3u; texture.height = 1u;
		buffer.size = 9u;
		memset( &one, 0, sizeof( one ) );
		one.aspects = RAL_TEXTURE_ASPECT_COLOR;
		one.imageRect.width = 3u; one.imageRect.height = 1u;
		uploadCalls = 0u;
		CHECK( Ral_CmdCopyBufferToTextureRegionsExact( &command, &buffer,
			&texture, 1u, &one ) && uploadCalls == 1u );

		/* A 3D copy uses z/depth, never array-layer multiplicity. */
		texture.type = RAL_TEXTURE_3D;
		texture.ralFormat = RAL_FORMAT_R8G8B8A8_UNORM;
		texture.width = texture.height = 8u;
		texture.depthOrArrayLayers = 8u;
		texture.arrayLayers = 1u;
		buffer.size = 8u * 8u * 4u * 4u;
		memset( &one, 0, sizeof( one ) );
		one.aspects = RAL_TEXTURE_ASPECT_COLOR;
		one.imageRect.width = one.imageRect.height = 8u;
		one.imageZ = 2u; one.imageDepth = 4u;
		uploadCalls = 0u;
		CHECK( Ral_CmdCopyBufferToTextureRegionsExact( &command, &buffer,
			&texture, 1u, &one ) && uploadCalls == 1u
			&& capturedUpload[0].imageOffset.z == 2
			&& capturedUpload[0].imageExtent.depth == 4u );
		REJECT_UPLOAD( badUpload.arrayLayer = 1u );
		REJECT_UPLOAD( badUpload.imageZ = 7u; badUpload.imageDepth = 2u );
		REJECT_UPLOAD( badUpload.imageZ = UINT32_MAX );
#undef REJECT_UPLOAD
	}

	/* A decoded WRTXART plan uses the same portable copy vocabulary, including
	 * WebGPU-compatible 256-byte rows, without a Vulkan-shaped planner branch. */
	{
		unsigned char payload[480], artifactBytes[1024], staging[5000];
		uint64_t levelBytes[2] = { 384u, 96u };
		ralTextureAssetRequest_t request;
		ralTextureAssetReceipt_t asset;
		ralTextureArtifactReceipt_t artifact;
		ralTextureUploadPlan_t plan;
		ralBufferTextureCopy_t regions[2];
		memset( payload, 0x7B, sizeof( payload ) );
		memset( &request, 0, sizeof( request ) );
		request.schemaVersion = RAL_TEXTURE_ASSET_SCHEMA_VERSION;
		request.assetGeneration = 41u; request.provenanceHash = 0xCAFEu;
		request.dimension = RAL_TEXTURE_ASSET_2D_ARRAY;
		request.width = 8u; request.height = 4u; request.depth = 1u;
		request.layers = 3u; request.sourceMipLevels = 2u;
		request.colorEncoding = RAL_TEXTURE_ENCODING_SRGB;
		request.channelSemantic = RAL_TEXTURE_CHANNEL_COLOR;
		request.sourceEncoding = RAL_TEXTURE_SOURCE_BASIS_UASTC;
		request.mipPolicy = RAL_TEXTURE_MIPS_SOURCE;
		request.residency = RAL_TEXTURE_RESIDENCY_STREAMED;
		request.preferenceCount = 1u;
		request.preferences[0] = RAL_TEXTURE_COMPRESSION_BC;
		request.allowUncompressedFallback = qtrue;
		CHECK( Ral_ResolveTextureAsset( &request, &asset ) );
		CHECK( Ral_EncodeTextureArtifact( &asset, levelBytes, 2u, payload,
			sizeof( payload ), artifactBytes, sizeof( artifactBytes ), &artifact ) );
		CHECK( Ral_BuildTextureArtifactUploadPlan( artifactBytes,
			artifact.containerByteLength, &asset, &plan ) );
		CHECK( Ral_PackTextureArtifactUpload( artifactBytes,
			artifact.containerByteLength, &plan, staging, sizeof( staging ) ) );
		texture.type = plan.texture.type; texture.ralFormat = plan.texture.format;
		texture.width = plan.texture.width; texture.height = plan.texture.height;
		texture.depthOrArrayLayers = plan.texture.depthOrArrayLayers;
		texture.arrayLayers = plan.texture.depthOrArrayLayers;
		texture.mipLevels = plan.texture.mipLevels;
		texture.sampleCount = 1u; texture.usage = RAL_TEXTURE_USAGE_TRANSFER_DST;
		texture.currentLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		texture.portableStateKnown = qtrue;
		texture.portableState.usage = RAL_RESOURCE_USAGE_COPY_DESTINATION;
		texture.portableOwnerQueue = RAL_QUEUE_GRAPHICS;
		buffer.size = plan.stagingByteLength;
		buffer.portableState.usage = RAL_RESOURCE_USAGE_COPY_SOURCE;
		regions[0] = plan.levels[0].region; regions[1] = plan.levels[1].region;
		uploadCalls = 0u;
		CHECK( Ral_CmdCopyBufferToTextureRegionsExact( &command, &buffer,
			&texture, 2u, regions ) );
		CHECK( uploadCalls == 1u && uploadRegionCount == 2u
			&& capturedUpload[0].bufferRowLength == 64u
			&& capturedUpload[0].bufferImageHeight == 4u
			&& capturedUpload[0].imageSubresource.layerCount == 3u
			&& capturedUpload[1].imageSubresource.mipLevel == 1u
			&& capturedUpload[1].bufferOffset == plan.levels[1].stagingOffset );
	}
	puts( "ral Vulkan padded texture copy: PASS" );
	return 0;
}
