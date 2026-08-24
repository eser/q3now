// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_vulkan_internal.h"
#include <stdio.h>
#include <string.h>

#define CHECK(expr) do { if ( !(expr) ) { \
	fprintf( stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr ); return 1; \
} } while ( 0 )

typedef struct {
	uint32_t calls;
	VkPipelineStageFlags srcStage;
	VkPipelineStageFlags dstStage;
	uint32_t bufferCount;
	uint32_t imageCount;
	VkBufferMemoryBarrier buffer;
	VkImageMemoryBarrier image;
} Capture;

static Capture capture;

static VKAPI_ATTR void VKAPI_CALL CaptureBarrier( VkCommandBuffer commandBuffer,
	VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask,
	VkDependencyFlags dependencyFlags, uint32_t memoryBarrierCount,
	const VkMemoryBarrier *memoryBarriers, uint32_t bufferMemoryBarrierCount,
	const VkBufferMemoryBarrier *bufferMemoryBarriers,
	uint32_t imageMemoryBarrierCount, const VkImageMemoryBarrier *imageMemoryBarriers ) {
	(void)commandBuffer;
	(void)dependencyFlags;
	(void)memoryBarrierCount;
	(void)memoryBarriers;
	capture.calls++;
	capture.srcStage = srcStageMask;
	capture.dstStage = dstStageMask;
	capture.bufferCount = bufferMemoryBarrierCount;
	capture.imageCount = imageMemoryBarrierCount;
	if ( bufferMemoryBarrierCount ) capture.buffer = bufferMemoryBarriers[0];
	if ( imageMemoryBarrierCount ) capture.image = imageMemoryBarriers[0];
}

static void SetupBackend( ralBackend_t *backend, uint32_t graphicsFamily,
	                      uint32_t computeFamily, uint32_t transferFamily ) {
	memset( backend, 0, sizeof( *backend ) );
	backend->queueFamily[RAL_QUEUE_GRAPHICS] = graphicsFamily;
	backend->queueFamily[RAL_QUEUE_COMPUTE] = computeFamily;
	backend->queueFamily[RAL_QUEUE_TRANSFER] = transferFamily;
	backend->vk.CmdPipelineBarrier = CaptureBarrier;
}

static void SetupCommand( ralCommandBuffer_t *command, ralBackend_t *backend,
	                      ralQueueType_t queue ) {
	memset( command, 0, sizeof( *command ) );
	command->backend = backend;
	command->cb = (VkCommandBuffer)(uintptr_t)0x100u;
	command->queue = queue;
	command->state = RAL_VK_CMD_RECORDING;
	command->lifecycle.state = RAL_COMMAND_RECORDING;
}

static void SetupBuffer( ralBuffer_t *buffer, ralBackend_t *backend,
	                     uintptr_t handle, uint64_t size,
	                     ralResourceUsage_t usage, ralQueueType_t owner ) {
	memset( buffer, 0, sizeof( *buffer ) );
	buffer->backend = backend;
	buffer->buffer = (VkBuffer)handle;
	buffer->size = (VkDeviceSize)size;
	buffer->portableStateKnown = qtrue;
	buffer->portableState.usage = usage;
	buffer->portableOwnerQueue = owner;
	Ral_BufferMapLifecycleInit( &buffer->mapLifecycle );
}

static void SetupTexture( ralTexture_t *texture, ralBackend_t *backend,
	                      uintptr_t handle, ralResourceUsage_t usage,
	                      VkImageLayout layout, ralQueueType_t owner ) {
	memset( texture, 0, sizeof( *texture ) );
	texture->backend = backend;
	texture->image = (VkImage)handle;
	texture->mipLevels = 1;
	texture->arrayLayers = 1;
	texture->aspect = VK_IMAGE_ASPECT_COLOR_BIT;
	texture->currentLayout = layout;
	texture->portableStateKnown = qtrue;
	texture->portableState.usage = usage;
	texture->portableOwnerQueue = owner;
}

int main( void ) {
	ralBackend_t backend, otherBackend;
	ralCommandBuffer_t command, sourceCommand, destinationCommand;
	ralBuffer_t buffer, buffer2;
	ralTexture_t texture, texture2;
	ralBufferTransition_t bufferTransition;
	ralTextureTransition_t textureTransition;
	ralResourceTransitionBatch_t batch;

	SetupBackend( &backend, 3, 3, 3 );
	SetupBackend( &otherBackend, 9, 9, 9 );
	SetupCommand( &command, &backend, RAL_QUEUE_GRAPHICS );
	SetupBuffer( &buffer, &backend, 0x200u, 256, RAL_RESOURCE_USAGE_COPY_DESTINATION, RAL_QUEUE_GRAPHICS );
	SetupTexture( &texture, &backend, 0x300u, RAL_RESOURCE_USAGE_UNDEFINED,
		VK_IMAGE_LAYOUT_UNDEFINED, RAL_QUEUE_GRAPHICS );
	memset( &bufferTransition, 0, sizeof( bufferTransition ) );
	bufferTransition.buffer = &buffer;
	bufferTransition.size = 256;
	bufferTransition.before.usage = RAL_RESOURCE_USAGE_COPY_DESTINATION;
	bufferTransition.after.usage = RAL_RESOURCE_USAGE_STORAGE_READ;
	bufferTransition.after.shaderStages = RAL_STAGE_COMPUTE;
	bufferTransition.sourceQueue = RAL_QUEUE_GRAPHICS;
	bufferTransition.destinationQueue = RAL_QUEUE_GRAPHICS;
	memset( &textureTransition, 0, sizeof( textureTransition ) );
	textureTransition.texture = &texture;
	textureTransition.aspects = RAL_TEXTURE_ASPECT_COLOR;
	textureTransition.mipLevelCount = 1;
	textureTransition.arrayLayerCount = 1;
	textureTransition.before.usage = RAL_RESOURCE_USAGE_UNDEFINED;
	textureTransition.after.usage = RAL_RESOURCE_USAGE_COPY_DESTINATION;
	textureTransition.sourceQueue = RAL_QUEUE_GRAPHICS;
	textureTransition.destinationQueue = RAL_QUEUE_GRAPHICS;
	memset( &batch, 0, sizeof( batch ) );
	batch.bufferTransitions = &bufferTransition;
	batch.bufferTransitionCount = 1;
	batch.textureTransitions = &textureTransition;
	batch.textureTransitionCount = 1;

	CHECK( Ral_CmdTransitionResources( &command, &batch ) == ralSuccess );
	CHECK( capture.calls == 1 && capture.bufferCount == 1 && capture.imageCount == 1 );
	CHECK( capture.buffer.srcQueueFamilyIndex == VK_QUEUE_FAMILY_IGNORED );
	CHECK( capture.buffer.dstQueueFamilyIndex == VK_QUEUE_FAMILY_IGNORED );
	CHECK( capture.image.oldLayout == VK_IMAGE_LAYOUT_UNDEFINED );
	CHECK( capture.image.newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL );
	CHECK( buffer.portableState.usage == RAL_RESOURCE_USAGE_STORAGE_READ );
	CHECK( texture.portableState.usage == RAL_RESOURCE_USAGE_COPY_DESTINATION );
	CHECK( texture.currentLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL );

	textureTransition.before = texture.portableState;
	textureTransition.after.usage = RAL_RESOURCE_USAGE_SAMPLED_TEXTURE;
	textureTransition.after.shaderStages = RAL_STAGE_FRAGMENT;
	batch.bufferTransitionCount = 0;
	CHECK( Ral_CmdTransitionResources( &command, &batch ) == ralSuccess );
	CHECK( capture.calls == 2 && capture.image.oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL );
	CHECK( capture.image.newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );
	CHECK( texture.currentLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );

	// A combined depth-stencil resource transitions both native planes even
	// when later sampling views select only the depth plane.
	SetupTexture( &texture2, &backend, 0x301u, RAL_RESOURCE_USAGE_UNDEFINED,
		VK_IMAGE_LAYOUT_UNDEFINED, RAL_QUEUE_GRAPHICS );
	texture2.aspect = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
	textureTransition.texture = &texture2;
	textureTransition.aspects = RAL_TEXTURE_ASPECT_DEPTH | RAL_TEXTURE_ASPECT_STENCIL;
	textureTransition.before.usage = RAL_RESOURCE_USAGE_UNDEFINED;
	textureTransition.before.shaderStages = 0;
	textureTransition.after.usage = RAL_RESOURCE_USAGE_COPY_DESTINATION;
	textureTransition.after.shaderStages = 0;
	CHECK( Ral_CmdTransitionResources( &command, &batch ) == ralSuccess );
	CHECK( capture.calls == 3
		&& capture.image.subresourceRange.aspectMask
			== ( VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT ) );

	// Restore the color transition fixture for rejection coverage below.
	textureTransition.texture = &texture;
	textureTransition.aspects = RAL_TEXTURE_ASPECT_COLOR;

	// Active render/compute pass boundaries reject before any command or state mutation.
	command.renderingActive = qtrue;
	textureTransition.before = texture.portableState;
	textureTransition.after.usage = RAL_RESOURCE_USAGE_STORAGE_READ;
	textureTransition.after.shaderStages = RAL_STAGE_COMPUTE;
	CHECK( Ral_CmdTransitionResources( &command, &batch ) == ralErrorInvalidArgument );
	CHECK( capture.calls == 3 && texture.portableState.usage == RAL_RESOURCE_USAGE_SAMPLED_TEXTURE );
	command.renderingActive = qfalse;

	// A batch with one invalid member is output-atomic.
	SetupBuffer( &buffer, &backend, 0x200u, 256, RAL_RESOURCE_USAGE_COPY_DESTINATION, RAL_QUEUE_GRAPHICS );
	SetupBuffer( &buffer2, &otherBackend, 0x201u, 256, RAL_RESOURCE_USAGE_COPY_DESTINATION, RAL_QUEUE_GRAPHICS );
	{
		ralBufferTransition_t transitions[2] = { bufferTransition, bufferTransition };
		transitions[0].buffer = &buffer;
		transitions[1].buffer = &buffer2;
		batch.bufferTransitions = transitions;
		batch.bufferTransitionCount = 2;
		batch.textureTransitionCount = 0;
		CHECK( Ral_CmdTransitionResources( &command, &batch ) == ralErrorInvalidArgument );
		CHECK( capture.calls == 3 );
		CHECK( buffer.portableState.usage == RAL_RESOURCE_USAGE_COPY_DESTINATION );
	}

	// Partial ranges and stale/legacy state are rejected without emission.
	batch.bufferTransitions = &bufferTransition;
	batch.bufferTransitionCount = 1;
	bufferTransition.buffer = &buffer;
	bufferTransition.size = 128;
	CHECK( Ral_CmdTransitionResources( &command, &batch ) == ralErrorInvalidArgument );
	bufferTransition.size = 256;
	buffer.portableStateKnown = qfalse;
	CHECK( Ral_CmdTransitionResources( &command, &batch ) == ralErrorInvalidArgument );
	CHECK( capture.calls == 3 );

	// A generation-bound typed map excludes portable GPU transitions.
	SetupBuffer( &buffer, &backend, 0x200u, 256, RAL_RESOURCE_USAGE_COPY_DESTINATION, RAL_QUEUE_GRAPHICS );
	bufferTransition.buffer = &buffer;
	bufferTransition.size = 256;
	bufferTransition.before = buffer.portableState;
	bufferTransition.after.usage = RAL_RESOURCE_USAGE_STORAGE_READ;
	bufferTransition.after.shaderStages = RAL_STAGE_COMPUTE;
	batch.bufferTransitions = &bufferTransition;
	batch.bufferTransitionCount = 1;
	batch.textureTransitionCount = 0;
	{
		ralBufferMapRequest_t request = { RAL_MAP_WRITE, 0, 256 };
		ralBufferMapTicket_t ticket;
		CHECK( Ral_BufferMapLifecyclePublishBegin( &buffer.mapLifecycle, &buffer, 256,
			&request, qtrue, (void *)(uintptr_t)0x500u, &ticket ) == ralSuccess );
		CHECK( Ral_CmdTransitionResources( &command, &batch ) == ralErrorInvalidArgument );
		CHECK( capture.calls == 3 );
		CHECK( Ral_BufferMapLifecycleUnmap( &buffer.mapLifecycle, &ticket ) == ralSuccess );
	}

	// Different logical queues work on one physical family (WebGPU shape).
	SetupTexture( &texture2, &backend, 0x301u, RAL_RESOURCE_USAGE_UNDEFINED,
		VK_IMAGE_LAYOUT_UNDEFINED, RAL_QUEUE_GRAPHICS );
	textureTransition.texture = &texture2;
	textureTransition.before.usage = RAL_RESOURCE_USAGE_UNDEFINED;
	textureTransition.before.shaderStages = 0;
	textureTransition.after.usage = RAL_RESOURCE_USAGE_SAMPLED_TEXTURE;
	textureTransition.after.shaderStages = RAL_STAGE_FRAGMENT;
	textureTransition.sourceQueue = RAL_QUEUE_TRANSFER;
	textureTransition.destinationQueue = RAL_QUEUE_GRAPHICS;
	batch.bufferTransitionCount = 0;
	batch.textureTransitions = &textureTransition;
	batch.textureTransitionCount = 1;
	CHECK( Ral_CmdTransitionResources( &command, &batch ) == ralSuccess );
	CHECK( capture.calls == 4 && texture2.portableOwnerQueue == RAL_QUEUE_GRAPHICS );

	// Dedicated Vulkan families need a paired release/acquire slice: fail closed.
	backend.queueFamily[RAL_QUEUE_TRANSFER] = 7;
	SetupTexture( &texture2, &backend, 0x302u, RAL_RESOURCE_USAGE_UNDEFINED,
		VK_IMAGE_LAYOUT_UNDEFINED, RAL_QUEUE_GRAPHICS );
	textureTransition.texture = &texture2;
	CHECK( Ral_CmdTransitionResources( &command, &batch ) == ralUnsupported );
	CHECK( capture.calls == 4 && texture2.portableState.usage == RAL_RESOURCE_USAGE_UNDEFINED );

	// Dedicated families use one exact generation-bound release/acquire receipt.
	{
		ralQueueTransferReceipt_t receipt, stale, sentinel, unchanged;
		SetupBackend( &backend, 3, 5, 7 );
		SetupCommand( &sourceCommand, &backend, RAL_QUEUE_TRANSFER );
		SetupCommand( &destinationCommand, &backend, RAL_QUEUE_GRAPHICS );
		SetupBuffer( &buffer, &backend, 0x410u, 256,
			RAL_RESOURCE_USAGE_COPY_SOURCE, RAL_QUEUE_TRANSFER );
		memset( &bufferTransition, 0, sizeof( bufferTransition ) );
		bufferTransition.buffer = &buffer; bufferTransition.size = 256;
		bufferTransition.before.usage = RAL_RESOURCE_USAGE_COPY_SOURCE;
		bufferTransition.after.usage = RAL_RESOURCE_USAGE_VERTEX_BUFFER;
		bufferTransition.sourceQueue = RAL_QUEUE_TRANSFER;
		bufferTransition.destinationQueue = RAL_QUEUE_GRAPHICS;
		memset( &sentinel, 0xa5, sizeof( sentinel ) ); unchanged = sentinel;
		CHECK( Ral_CmdReleaseBufferOwnership( &sourceCommand,
			&bufferTransition, &receipt ) == ralSuccess );
		CHECK( receipt.ready && receipt.generation == 1
			&& receipt.resourceIdentity == &buffer
			&& receipt.resourceType == RAL_QUEUE_TRANSFER_RESOURCE_BUFFER );
		CHECK( !ralVk_BufferGpuUseAllowed( &buffer ) );
		CHECK( capture.calls == 5 && capture.buffer.srcQueueFamilyIndex == 7
			&& capture.buffer.dstQueueFamilyIndex == 3
			&& capture.buffer.srcAccessMask == VK_ACCESS_TRANSFER_READ_BIT
			&& capture.buffer.dstAccessMask == 0 );
		CHECK( Ral_CmdReleaseBufferOwnership( &sourceCommand,
			&bufferTransition, &sentinel ) == ralErrorInvalidArgument );
		CHECK( memcmp( &sentinel, &unchanged, sizeof( sentinel ) ) == 0
			&& capture.calls == 5 );
		stale = receipt; stale.generation++;
		CHECK( Ral_CmdAcquireBufferOwnership( &destinationCommand,
			&bufferTransition, &stale ) == ralErrorInvalidArgument );
		CHECK( capture.calls == 5 );
		CHECK( Ral_CmdAcquireBufferOwnership( &destinationCommand,
			&bufferTransition, &receipt ) == ralSuccess );
		CHECK( capture.calls == 6 && capture.buffer.srcAccessMask == 0
			&& capture.buffer.dstAccessMask == VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT
			&& buffer.portableOwnerQueue == RAL_QUEUE_GRAPHICS
			&& buffer.portableState.usage == RAL_RESOURCE_USAGE_VERTEX_BUFFER
			&& !buffer.queueTransfer.pending.ready );
		CHECK( Ral_CmdAcquireBufferOwnership( &destinationCommand,
			&bufferTransition, &receipt ) == ralErrorInvalidArgument );

		// A discarded release command can cancel only its exact pending receipt.
		SetupBuffer( &buffer2, &backend, 0x411u, 256,
			RAL_RESOURCE_USAGE_COPY_SOURCE, RAL_QUEUE_TRANSFER );
		bufferTransition.buffer = &buffer2;
		CHECK( Ral_CmdReleaseBufferOwnership( &sourceCommand,
			&bufferTransition, &receipt ) == ralSuccess );
		stale = receipt; stale.sourceQueue = RAL_QUEUE_COMPUTE;
		CHECK( Ral_CancelBufferOwnershipTransfer( &buffer2, &stale )
			== ralErrorInvalidArgument );
		CHECK( Ral_CancelBufferOwnershipTransfer( &buffer2, &receipt ) == ralSuccess );
		CHECK( !buffer2.queueTransfer.pending.ready
			&& buffer2.portableOwnerQueue == RAL_QUEUE_TRANSFER
			&& ralVk_BufferGpuUseAllowed( &buffer2 ) );
	}

	{
		ralQueueTransferReceipt_t receipt, stale;
		SetupTexture( &texture2, &backend, 0x420u,
			RAL_RESOURCE_USAGE_COPY_DESTINATION,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, RAL_QUEUE_TRANSFER );
		memset( &textureTransition, 0, sizeof( textureTransition ) );
		textureTransition.texture = &texture2;
		textureTransition.aspects = RAL_TEXTURE_ASPECT_COLOR;
		textureTransition.mipLevelCount = 1; textureTransition.arrayLayerCount = 1;
		textureTransition.before.usage = RAL_RESOURCE_USAGE_COPY_DESTINATION;
		textureTransition.after.usage = RAL_RESOURCE_USAGE_SAMPLED_TEXTURE;
		textureTransition.after.shaderStages = RAL_STAGE_FRAGMENT;
		textureTransition.sourceQueue = RAL_QUEUE_TRANSFER;
		textureTransition.destinationQueue = RAL_QUEUE_GRAPHICS;
		CHECK( Ral_CmdReleaseTextureOwnership( &sourceCommand,
			&textureTransition, &receipt ) == ralSuccess );
		CHECK( capture.calls == 8 && capture.image.srcQueueFamilyIndex == 7
			&& capture.image.dstQueueFamilyIndex == 3
			&& capture.image.oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
			&& capture.image.newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );
		stale = receipt; stale.after.shaderStages = RAL_STAGE_VERTEX;
		CHECK( Ral_CmdAcquireTextureOwnership( &destinationCommand,
			&textureTransition, &stale ) == ralErrorInvalidArgument );
		CHECK( Ral_CmdAcquireTextureOwnership( &destinationCommand,
			&textureTransition, &receipt ) == ralSuccess );
		CHECK( capture.calls == 9 && texture2.currentLayout
			== VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
			&& texture2.portableOwnerQueue == RAL_QUEUE_GRAPHICS );
	}

	// The lifecycle itself is the WebGPU single-queue scheduler contract.
	{
		ralQueueTransferLifecycle_t lifecycle;
		ralQueueTransferReceipt_t receipt, mutation;
		ralResourceState_t before = { RAL_RESOURCE_USAGE_COPY_SOURCE, 0 };
		ralResourceState_t after = { RAL_RESOURCE_USAGE_VERTEX_BUFFER, 0 };
		Ral_QueueTransferLifecycleInit( &lifecycle );
		CHECK( Ral_QueueTransferLifecycleRelease( &lifecycle, &buffer,
			RAL_QUEUE_TRANSFER_RESOURCE_BUFFER, &before, &after,
			RAL_QUEUE_TRANSFER, RAL_QUEUE_GRAPHICS, &receipt ) == ralSuccess );
		mutation = receipt; mutation.resourceIdentity = &buffer2;
		CHECK( Ral_QueueTransferLifecycleAcquire( &lifecycle, &mutation )
			== ralErrorInvalidArgument );
		CHECK( Ral_QueueTransferLifecycleAcquire( &lifecycle, &receipt ) == ralSuccess );
		lifecycle.pending.ready = qfalse; lifecycle.pending.generation = 9;
		CHECK( Ral_QueueTransferLifecycleRelease( &lifecycle, &buffer,
			RAL_QUEUE_TRANSFER_RESOURCE_BUFFER, &before, &after,
			RAL_QUEUE_TRANSFER, RAL_QUEUE_GRAPHICS, &receipt )
			== ralErrorInvalidArgument );
		memset( &lifecycle.pending, 0, sizeof( lifecycle.pending ) );
		lifecycle.nextGeneration = UINT64_MAX - 1u;
		CHECK( Ral_QueueTransferLifecycleRelease( &lifecycle, &buffer,
			RAL_QUEUE_TRANSFER_RESOURCE_BUFFER, &before, &after,
			RAL_QUEUE_TRANSFER, RAL_QUEUE_GRAPHICS, &receipt )
			== ralErrorInvalidArgument );
	}

	// Dedicated entry points refuse a same-family backend; the shared command
	// remains the only lowering for WebGPU/single-queue physical execution.
	{
		ralQueueTransferReceipt_t output, unchanged;
		SetupBackend( &backend, 3, 3, 3 );
		SetupCommand( &sourceCommand, &backend, RAL_QUEUE_TRANSFER );
		SetupBuffer( &buffer, &backend, 0x430u, 256,
			RAL_RESOURCE_USAGE_COPY_SOURCE, RAL_QUEUE_TRANSFER );
		bufferTransition.buffer = &buffer;
		memset( &output, 0xa5, sizeof( output ) ); unchanged = output;
		CHECK( Ral_CmdReleaseBufferOwnership( &sourceCommand,
			&bufferTransition, &output ) == ralErrorInvalidArgument );
		CHECK( memcmp( &output, &unchanged, sizeof( output ) ) == 0 );
	}

	// Dynamic-rendering color outputs use this exact semantic handoff for UI,
	// screenmap, bloom, temporal post-bloom and screenshot capture. Pin both
	// sampled and copy-source lowerings independently of product source policy.
	{
		ralTexture_t attachment;
		ralTextureTransition_t transition;
		ralResourceTransitionBatch_t transitionBatch;

		SetupBackend( &backend, 3, 3, 3 );
		SetupCommand( &command, &backend, RAL_QUEUE_GRAPHICS );
		SetupTexture( &attachment, &backend, 0x440u,
			RAL_RESOURCE_USAGE_COLOR_ATTACHMENT,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, RAL_QUEUE_GRAPHICS );
		memset( &transition, 0, sizeof( transition ) );
		transition.texture = &attachment;
		transition.aspects = RAL_TEXTURE_ASPECT_COLOR;
		transition.mipLevelCount = 1u;
		transition.arrayLayerCount = 1u;
		transition.before.usage = RAL_RESOURCE_USAGE_COLOR_ATTACHMENT;
		transition.after.usage = RAL_RESOURCE_USAGE_SAMPLED_TEXTURE;
		transition.after.shaderStages = RAL_STAGE_FRAGMENT;
		transition.sourceQueue = RAL_QUEUE_GRAPHICS;
		transition.destinationQueue = RAL_QUEUE_GRAPHICS;
		memset( &transitionBatch, 0, sizeof( transitionBatch ) );
		transitionBatch.textureTransitions = &transition;
		transitionBatch.textureTransitionCount = 1u;
		memset( &capture, 0, sizeof( capture ) );
		CHECK( Ral_CmdTransitionResources( &command, &transitionBatch ) == ralSuccess );
		CHECK( capture.calls == 1u
			&& capture.srcStage == VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
			&& capture.dstStage == VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
			&& capture.image.srcAccessMask
				== ( VK_ACCESS_COLOR_ATTACHMENT_READ_BIT
					| VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT )
			&& capture.image.dstAccessMask == VK_ACCESS_SHADER_READ_BIT
			&& capture.image.oldLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
			&& capture.image.newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );

		SetupTexture( &attachment, &backend, 0x441u,
			RAL_RESOURCE_USAGE_COLOR_ATTACHMENT,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, RAL_QUEUE_GRAPHICS );
		transition.texture = &attachment;
		transition.after.usage = RAL_RESOURCE_USAGE_COPY_SOURCE;
		transition.after.shaderStages = 0u;
		memset( &capture, 0, sizeof( capture ) );
		CHECK( Ral_CmdTransitionResources( &command, &transitionBatch ) == ralSuccess );
		CHECK( capture.calls == 1u
			&& capture.srcStage == VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
			&& capture.dstStage == VK_PIPELINE_STAGE_TRANSFER_BIT
			&& capture.image.srcAccessMask
				== ( VK_ACCESS_COLOR_ATTACHMENT_READ_BIT
					| VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT )
			&& capture.image.dstAccessMask == VK_ACCESS_TRANSFER_READ_BIT
			&& capture.image.oldLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
			&& capture.image.newLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL );
	}

	// A depth-only attachment view over a combined D24S8 resource retains its
	// narrow rendering aspect, while the whole-resource semantic handoff must
	// cover depth+stencil.  This is the CSM adoption shape and the portability
	// model WebGPU texture/view aspects require.
	{
		ralTexture_t shadow;
		ralTextureTransition_t transition;
		ralResourceTransitionBatch_t transitionBatch;

		SetupBackend( &backend, 3, 3, 3 );
		SetupCommand( &command, &backend, RAL_QUEUE_GRAPHICS );
		memset( &shadow, 0, sizeof( shadow ) );
		shadow.backend = &backend;
		shadow.image = (VkImage)(uintptr_t)0x450u;
		shadow.vkFormat = VK_FORMAT_D24_UNORM_S8_UINT;
		shadow.mipLevels = 1u;
		shadow.arrayLayers = 4u;
		shadow.aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
		shadow.currentLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		shadow.portableStateKnown = qtrue;
		shadow.portableState.usage = RAL_RESOURCE_USAGE_DEPTH_STENCIL_WRITE;
		shadow.portableOwnerQueue = RAL_QUEUE_GRAPHICS;
		memset( &transition, 0, sizeof( transition ) );
		transition.texture = &shadow;
		transition.aspects = RAL_TEXTURE_ASPECT_DEPTH | RAL_TEXTURE_ASPECT_STENCIL;
		transition.mipLevelCount = 1u;
		transition.arrayLayerCount = 4u;
		transition.before.usage = RAL_RESOURCE_USAGE_DEPTH_STENCIL_WRITE;
		transition.after.usage = RAL_RESOURCE_USAGE_SAMPLED_TEXTURE;
		transition.after.shaderStages = RAL_STAGE_FRAGMENT;
		transition.sourceQueue = RAL_QUEUE_GRAPHICS;
		transition.destinationQueue = RAL_QUEUE_GRAPHICS;
		memset( &transitionBatch, 0, sizeof( transitionBatch ) );
		transitionBatch.textureTransitions = &transition;
		transitionBatch.textureTransitionCount = 1u;
		memset( &capture, 0, sizeof( capture ) );
		CHECK( Ral_CmdTransitionResources( &command, &transitionBatch ) == ralSuccess );
		CHECK( capture.calls == 1u
			&& capture.srcStage == ( VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
				| VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT )
			&& capture.dstStage == VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
			&& capture.image.srcAccessMask
				== ( VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT
					| VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT )
			&& capture.image.dstAccessMask == VK_ACCESS_SHADER_READ_BIT
			&& capture.image.subresourceRange.aspectMask
				== ( VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT )
			&& capture.image.subresourceRange.layerCount == 4u
			&& shadow.aspect == VK_IMAGE_ASPECT_DEPTH_BIT );

		shadow.currentLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		shadow.portableStateKnown = qtrue;
		shadow.portableState.usage = RAL_RESOURCE_USAGE_DEPTH_STENCIL_WRITE;
		transition.aspects = RAL_TEXTURE_ASPECT_DEPTH;
		memset( &capture, 0, sizeof( capture ) );
		CHECK( Ral_CmdTransitionResources( &command, &transitionBatch )
			== ralErrorInvalidArgument );
		CHECK( capture.calls == 0u );
	}

	puts( "PASS portable transitions plus paired dedicated-queue ownership" );
	return 0;
}
