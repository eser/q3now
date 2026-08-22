// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_vulkan_internal.h"

#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if ( !(x) ) { \
	fprintf( stderr, "CHECK %s:%d: %s\n", __FILE__, __LINE__, #x ); return 1; \
} } while ( 0 )

static uint32_t copyCalls;
static VkBufferImageCopy captured;

static VKAPI_ATTR void VKAPI_CALL CopyImageToBuffer( VkCommandBuffer command,
		VkImage image, VkImageLayout layout, VkBuffer buffer,
		uint32_t count, const VkBufferImageCopy *regions ) {
	(void)command; (void)image; (void)buffer;
	if ( layout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL && count == 1u ) {
		copyCalls++;
		captured = regions[0];
	}
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
	command.backend = &backend;
	command.cb = (VkCommandBuffer)(uintptr_t)0x10u;
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
	puts( "ral Vulkan padded texture copy: PASS" );
	return 0;
}
