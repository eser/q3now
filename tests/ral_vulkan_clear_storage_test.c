// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "ral_vulkan_internal.h"

#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if ( !(x) ) { \
	fprintf( stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x ); return 1; \
} } while ( 0 )

static uint32_t barrierCalls, fillCalls;
static VkPipelineStageFlags srcStages[2], dstStages[2];
static VkBufferMemoryBarrier barriers[2];
static VkBuffer fillBuffer;
static VkDeviceSize fillOffset, fillSize;
static uint32_t fillValue;

static VKAPI_ATTR void VKAPI_CALL CaptureBarrier( VkCommandBuffer cb,
		VkPipelineStageFlags src, VkPipelineStageFlags dst,
		VkDependencyFlags dependency, uint32_t memoryCount,
		const VkMemoryBarrier *memory, uint32_t bufferCount,
		const VkBufferMemoryBarrier *buffer, uint32_t imageCount,
		const VkImageMemoryBarrier *image ) {
	(void)cb; (void)dependency; (void)memoryCount; (void)memory;
	(void)imageCount; (void)image;
	if ( barrierCalls < 2u && bufferCount == 1u ) {
		srcStages[barrierCalls] = src;
		dstStages[barrierCalls] = dst;
		barriers[barrierCalls] = buffer[0];
	}
	barrierCalls++;
}

static VKAPI_ATTR void VKAPI_CALL CaptureFill( VkCommandBuffer cb,
		VkBuffer buffer, VkDeviceSize offset, VkDeviceSize size, uint32_t data ) {
	(void)cb;
	fillCalls++;
	fillBuffer = buffer;
	fillOffset = offset;
	fillSize = size;
	fillValue = data;
}

static void ResetCapture( void ) {
	barrierCalls = fillCalls = 0u;
	memset( srcStages, 0, sizeof( srcStages ) );
	memset( dstStages, 0, sizeof( dstStages ) );
	memset( barriers, 0, sizeof( barriers ) );
	fillBuffer = VK_NULL_HANDLE;
	fillOffset = fillSize = 0u;
	fillValue = 0xffffffffu;
}

int main( void ) {
	ralBackend_t backend, other;
	ralCommandBuffer_t command;
	ralBuffer_t buffer;
	ralResourceState_t sentinel;

	memset( &backend, 0, sizeof( backend ) );
	memset( &other, 0, sizeof( other ) );
	backend.vk.CmdPipelineBarrier = CaptureBarrier;
	backend.vk.CmdFillBuffer = CaptureFill;
	memset( &command, 0, sizeof( command ) );
	command.backend = &backend;
	command.cb = (VkCommandBuffer)(uintptr_t)0x10u;
	command.state = RAL_VK_CMD_RECORDING;
	command.lifecycle.state = RAL_COMMAND_RECORDING;
	command.queue = RAL_QUEUE_GRAPHICS;
	memset( &buffer, 0, sizeof( buffer ) );
	buffer.backend = &backend;
	buffer.buffer = (VkBuffer)(uintptr_t)0x20u;
	buffer.size = 64u;
	buffer.usage = RAL_BUFFER_STORAGE | RAL_BUFFER_TRANSFER_DST;
	Ral_BufferMapLifecycleInit( &buffer.mapLifecycle );

	ResetCapture();
	CHECK( Ral_CmdClearStorageBuffer( &command, &buffer, 4u, 8u ) );
	CHECK( barrierCalls == 2u && fillCalls == 1u );
	CHECK( srcStages[0] == VK_PIPELINE_STAGE_ALL_COMMANDS_BIT
		&& dstStages[0] == VK_PIPELINE_STAGE_TRANSFER_BIT );
	CHECK( barriers[0].srcAccessMask == ( VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT )
		&& barriers[0].dstAccessMask == VK_ACCESS_TRANSFER_WRITE_BIT );
	CHECK( srcStages[1] == VK_PIPELINE_STAGE_TRANSFER_BIT
		&& dstStages[1] == VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );
	CHECK( barriers[1].srcAccessMask == VK_ACCESS_TRANSFER_WRITE_BIT
		&& barriers[1].dstAccessMask == ( VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT ) );
	CHECK( barriers[0].buffer == buffer.buffer && barriers[1].buffer == buffer.buffer
		&& barriers[0].offset == 4u && barriers[1].offset == 4u
		&& barriers[0].size == 8u && barriers[1].size == 8u );
	CHECK( fillBuffer == buffer.buffer && fillOffset == 4u && fillSize == 8u && fillValue == 0u );
	CHECK( buffer.portableStateKnown
		&& buffer.portableState.usage == RAL_RESOURCE_USAGE_STORAGE_READ_WRITE
		&& buffer.portableState.shaderStages == RAL_STAGE_COMPUTE
		&& buffer.portableOwnerQueue == RAL_QUEUE_GRAPHICS );

	/* Every rejected mutation is output-atomic: no Vulkan command and no state
	 * publication. */
	sentinel.usage = RAL_RESOURCE_USAGE_COPY_SOURCE;
	sentinel.shaderStages = RAL_STAGE_VERTEX;
#define REJECT(expr) do { \
	buffer.portableStateKnown = qtrue; buffer.portableState = sentinel; \
	buffer.portableOwnerQueue = RAL_QUEUE_TRANSFER; ResetCapture(); \
	CHECK( !(expr) ); CHECK( barrierCalls == 0u && fillCalls == 0u ); \
	CHECK( buffer.portableStateKnown && buffer.portableState.usage == sentinel.usage \
		&& buffer.portableState.shaderStages == sentinel.shaderStages \
		&& buffer.portableOwnerQueue == RAL_QUEUE_TRANSFER ); \
} while ( 0 )
	REJECT( Ral_CmdClearStorageBuffer( NULL, &buffer, 0u, 4u ) );
	REJECT( Ral_CmdClearStorageBuffer( &command, NULL, 0u, 4u ) );
	buffer.backend = &other; REJECT( Ral_CmdClearStorageBuffer( &command, &buffer, 0u, 4u ) ); buffer.backend = &backend;
	command.state = RAL_VK_CMD_IDLE; REJECT( Ral_CmdClearStorageBuffer( &command, &buffer, 0u, 4u ) ); command.state = RAL_VK_CMD_RECORDING;
	command.lifecycle.state = RAL_COMMAND_IDLE; REJECT( Ral_CmdClearStorageBuffer( &command, &buffer, 0u, 4u ) ); command.lifecycle.state = RAL_COMMAND_RECORDING;
	command.renderingActive = qtrue; REJECT( Ral_CmdClearStorageBuffer( &command, &buffer, 0u, 4u ) ); command.renderingActive = qfalse;
	command.queue = (ralQueueType_t)99; REJECT( Ral_CmdClearStorageBuffer( &command, &buffer, 0u, 4u ) ); command.queue = RAL_QUEUE_GRAPHICS;
	backend.vk.CmdFillBuffer = NULL; REJECT( Ral_CmdClearStorageBuffer( &command, &buffer, 0u, 4u ) ); backend.vk.CmdFillBuffer = CaptureFill;
	backend.vk.CmdPipelineBarrier = NULL; REJECT( Ral_CmdClearStorageBuffer( &command, &buffer, 0u, 4u ) ); backend.vk.CmdPipelineBarrier = CaptureBarrier;
	buffer.buffer = VK_NULL_HANDLE; REJECT( Ral_CmdClearStorageBuffer( &command, &buffer, 0u, 4u ) ); buffer.buffer = (VkBuffer)(uintptr_t)0x20u;
	buffer.usage &= ~RAL_BUFFER_STORAGE; REJECT( Ral_CmdClearStorageBuffer( &command, &buffer, 0u, 4u ) ); buffer.usage |= RAL_BUFFER_STORAGE;
	buffer.usage &= ~RAL_BUFFER_TRANSFER_DST; REJECT( Ral_CmdClearStorageBuffer( &command, &buffer, 0u, 4u ) ); buffer.usage |= RAL_BUFFER_TRANSFER_DST;
	buffer.legacyMapped = qtrue; REJECT( Ral_CmdClearStorageBuffer( &command, &buffer, 0u, 4u ) ); buffer.legacyMapped = qfalse;
	buffer.queueTransfer.pending.ready = qtrue; REJECT( Ral_CmdClearStorageBuffer( &command, &buffer, 0u, 4u ) ); buffer.queueTransfer.pending.ready = qfalse;
	REJECT( Ral_CmdClearStorageBuffer( &command, &buffer, 1u, 4u ) );
	REJECT( Ral_CmdClearStorageBuffer( &command, &buffer, 0u, 0u ) );
	REJECT( Ral_CmdClearStorageBuffer( &command, &buffer, 0u, 6u ) );
	REJECT( Ral_CmdClearStorageBuffer( &command, &buffer, 64u, 4u ) );
#undef REJECT

	command.state = RAL_VK_CMD_RECORDING;
	command.lifecycle.state = RAL_COMMAND_RECORDING;
	ResetCapture();
	CHECK( Ral_CmdClearStorageBuffer( &command, &buffer, 0u, 64u ) );
	CHECK( barrierCalls == 2u && fillCalls == 1u && fillSize == 64u );

	puts( "PASS Vulkan/WebGPU-shaped zero-clear storage command" );
	return 0;
}
