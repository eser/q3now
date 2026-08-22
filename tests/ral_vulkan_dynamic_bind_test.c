// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_vulkan_internal.h"

#include <stdio.h>
#include <string.h>

#define CHECK(expr) do { if ( !(expr) ) { \
	fprintf( stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr ); return 1; \
} } while ( 0 )

static uint32_t bindCalls, capturedSetIndex, capturedCount, capturedOffsets[2];
static VkDescriptorSet capturedSet;
static uint32_t pushCalls, capturedPushStages, capturedPushOffset,
	capturedPushSize, capturedPushWord;
static VkPipelineLayout capturedPushLayout;
static uint32_t clearCalls, capturedClearAttachmentCount, capturedClearRectCount;
static VkClearAttachment capturedClearAttachment;
static VkClearRect capturedClearRect;
static uint32_t destroyPipelineLayoutCalls;
static VkPipelineLayout capturedDestroyedPipelineLayout;

static VKAPI_ATTR VkResult VKAPI_CALL CaptureBeginCommandBuffer(
		VkCommandBuffer commandBuffer, const VkCommandBufferBeginInfo *beginInfo ) {
	(void)commandBuffer;
	return beginInfo && beginInfo->sType == VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO
		? VK_SUCCESS : VK_ERROR_INITIALIZATION_FAILED;
}

static VKAPI_ATTR void VKAPI_CALL CaptureBindDescriptorSets(
	VkCommandBuffer commandBuffer, VkPipelineBindPoint bindPoint,
	VkPipelineLayout layout, uint32_t firstSet, uint32_t descriptorSetCount,
	const VkDescriptorSet *descriptorSets, uint32_t dynamicOffsetCount,
	const uint32_t *dynamicOffsets ) {
	(void)commandBuffer; (void)bindPoint; (void)layout;
	bindCalls++;
	capturedSetIndex = firstSet;
	capturedSet = descriptorSetCount == 1 ? descriptorSets[0] : VK_NULL_HANDLE;
	capturedCount = dynamicOffsetCount;
	if ( dynamicOffsetCount > 0 ) capturedOffsets[0] = dynamicOffsets[0];
	if ( dynamicOffsetCount > 1 ) capturedOffsets[1] = dynamicOffsets[1];
}

static void ResetCapture( void ) {
	bindCalls = capturedSetIndex = capturedCount = 0;
	capturedOffsets[0] = capturedOffsets[1] = 0;
	capturedSet = VK_NULL_HANDLE;
}

static VKAPI_ATTR void VKAPI_CALL CapturePushConstants(
		VkCommandBuffer commandBuffer, VkPipelineLayout layout,
		VkShaderStageFlags stageFlags, uint32_t offset, uint32_t size,
		const void *data ) {
	(void)commandBuffer;
	pushCalls++;
	capturedPushLayout = layout;
	capturedPushStages = stageFlags;
	capturedPushOffset = offset;
	capturedPushSize = size;
	capturedPushWord = size >= sizeof( capturedPushWord )
		? *(const uint32_t *)data : 0u;
}

static void ResetPushCapture( void ) {
	pushCalls = capturedPushStages = capturedPushOffset = capturedPushSize = 0u;
	capturedPushWord = 0u;
	capturedPushLayout = VK_NULL_HANDLE;
}

static VKAPI_ATTR void VKAPI_CALL CaptureClearAttachments(
		VkCommandBuffer commandBuffer, uint32_t attachmentCount,
		const VkClearAttachment *attachments, uint32_t rectCount,
		const VkClearRect *rects ) {
	(void)commandBuffer;
	clearCalls++;
	capturedClearAttachmentCount = attachmentCount;
	capturedClearRectCount = rectCount;
	if ( attachmentCount > 0u ) capturedClearAttachment = attachments[0];
	if ( rectCount > 0u ) capturedClearRect = rects[0];
}

static void ResetClearCapture( void ) {
	clearCalls = capturedClearAttachmentCount = capturedClearRectCount = 0u;
	memset( &capturedClearAttachment, 0, sizeof( capturedClearAttachment ) );
	memset( &capturedClearRect, 0, sizeof( capturedClearRect ) );
}

static VKAPI_ATTR void VKAPI_CALL CaptureDestroyPipelineLayout(
		VkDevice device, VkPipelineLayout layout,
		const VkAllocationCallbacks *allocator ) {
	(void)device; (void)allocator;
	destroyPipelineLayoutCalls++;
	capturedDestroyedPipelineLayout = layout;
}

int main( void ) {
	ralBackend_t backend, otherBackend;
	ralCommandBuffer_t command;
	ralPipeline_t pipeline, exactPipeline;
	ralBindGroupLayout_t layout, otherLayout;
	ralBindGroupLayout_t *layoutVector[4];
	ralBindGroup_t group, sentinel;
	ralBuffer_t uniformBuffer, storageBuffer;
	uint32_t offsets[2] = { 256u, 32u };
	uint32_t pushPayload[8] = { 0x12345678u };
	ralClearAttachment_t clearAttachment;
	ralClearRect_t clearRect;
	ralBindEntry_t adoptedEntry;
	ralBindGroupLayout_t *adoptedLayout;
	ralBindGroup_t *adoptedGroup;
	ralPipelineLayout_t *pushLayout;
	ralPipelineLayout_t *ownedLayout;
	ralPipelineLayout_t otherPushLayout;
	ralBuffer_t *adoptedBuffer;
	ralBufferCreateInfo_t adoptedBufferInfo;
	ralCommandReceipt_t commandRecording;
	uint32_t adoptedOffset = 256u;

	memset( &backend, 0, sizeof( backend ) );
	memset( &otherBackend, 0, sizeof( otherBackend ) );
	backend.caps.minUniformBufferAlignment = 256u;
	backend.caps.minStorageBufferAlignment = 16u;
	backend.caps.maxPushConstantSize = 128u;
	backend.vk.CmdBindDescriptorSets = CaptureBindDescriptorSets;
	backend.vk.CmdPushConstants = CapturePushConstants;
	backend.vk.CmdClearAttachments = CaptureClearAttachments;
	backend.vk.DestroyPipelineLayout = CaptureDestroyPipelineLayout;
	backend.vk.BeginCommandBuffer = CaptureBeginCommandBuffer;
	backend.device = (VkDevice)(uintptr_t)0x09u;
	memset( &pipeline, 0, sizeof( pipeline ) );
	pipeline.backend = &backend;
	pipeline.layout = (VkPipelineLayout)(uintptr_t)0x20u;
	memset( &command, 0, sizeof( command ) );
	command.backend = &backend;
	command.cb = (VkCommandBuffer)(uintptr_t)0x10u;
	command.queue = RAL_QUEUE_GRAPHICS;
	Ral_CommandLifecycleInit( &command.lifecycle, &backend, &command, command.queue );
	CHECK( Ral_CommandLifecyclePublishBegin( &command.lifecycle,
		&commandRecording ) == ralSuccess );
	command.state = RAL_VK_CMD_RECORDING;
	command.currentPipeline = &pipeline;
	command.currentLayout = (VkPipelineLayout)(uintptr_t)0x20u;
	command.currentBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

	memset( &layout, 0, sizeof( layout ) );
	layout.backend = &backend;
	layout.dynamicOffsetCount = 2u;
	memset( &uniformBuffer, 0, sizeof( uniformBuffer ) );
	uniformBuffer.backend = &backend;
	uniformBuffer.buffer = (VkBuffer)(uintptr_t)0x30u;
	uniformBuffer.size = 2048u;
	Ral_BufferMapLifecycleInit( &uniformBuffer.mapLifecycle );
	memset( &storageBuffer, 0, sizeof( storageBuffer ) );
	storageBuffer.backend = &backend;
	storageBuffer.buffer = (VkBuffer)(uintptr_t)0x31u;
	storageBuffer.size = 1024u;
	Ral_BufferMapLifecycleInit( &storageBuffer.mapLifecycle );

	memset( &group, 0, sizeof( group ) );
	group.backend = &backend;
	group.layout = &layout;
	group.set = (VkDescriptorSet)(uintptr_t)0x40u;
	group.bufferTrackingComplete = qtrue;
	group.bufferCount = 2u;
	group.buffers[0] = &uniformBuffer;
	group.buffers[1] = &storageBuffer;
	group.dynamicOffsetCount = 2u;
	group.dynamicBindings[0].binding = 2u;
	group.dynamicBindings[0].vkType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
	group.dynamicBindings[0].buffer = &uniformBuffer;
	group.dynamicBindings[0].baseOffset = 0u;
	group.dynamicBindings[0].range = 112u;
	group.dynamicBindings[0].registered = qtrue;
	group.dynamicBindings[1].binding = 5u;
	group.dynamicBindings[1].vkType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
	group.dynamicBindings[1].buffer = &storageBuffer;
	group.dynamicBindings[1].baseOffset = 64u;
	group.dynamicBindings[1].range = 128u;
	group.dynamicBindings[1].registered = qtrue;

	ResetCapture();
	CHECK( ralVk_CmdBindBindGroupDynamic( &command, 3u, &group, offsets, 2u ) );
	CHECK( bindCalls == 1u && capturedSetIndex == 3u && capturedSet == group.set );
	CHECK( capturedCount == 2u && capturedOffsets[0] == 256u && capturedOffsets[1] == 32u );
	CHECK( command.boundBindGroups[3] == &group );

	// Exact binding requires a pipeline-owned set-layout vector. Registration
	// is output-atomic/idempotent and a same-shape group from another native
	// layout identity must reject without replacing the tracked group.
	exactPipeline = pipeline;
	exactPipeline.layoutCacheIndex = 0xFFFFFFFFu;
	layout.layout = (VkDescriptorSetLayout)(uintptr_t)0x60u;
	layoutVector[0] = layoutVector[1] = layoutVector[2] = layoutVector[3] = &layout;
	command.currentPipeline = &exactPipeline;
	ResetCapture();
	CHECK( !Ral_CmdBindBindGroupDynamicExact( &command, 3u, &group, offsets, 2u ) );
	CHECK( bindCalls == 0u );
	CHECK( Ral_RegisterExternalPipelineBindGroupLayouts(
		&exactPipeline, 4u, layoutVector ) );
	command.boundBindGroups[3] = &sentinel;
	ResetCapture();
	CHECK( Ral_ValidateBindGroupDynamicExact( &command, &exactPipeline,
		3u, &group, offsets, 2u ) );
	CHECK( bindCalls == 0u && command.boundBindGroups[3] == &sentinel
		&& command.currentPipeline == &exactPipeline );
	exactPipeline.backend = &otherBackend;
	CHECK( !Ral_ValidateBindGroupDynamicExact( &command, &exactPipeline,
		3u, &group, offsets, 2u ) );
	exactPipeline.backend = &backend;
	exactPipeline.layout = VK_NULL_HANDLE;
	CHECK( !Ral_ValidateBindGroupDynamicExact( &command, &exactPipeline,
		3u, &group, offsets, 2u ) );
	exactPipeline.layout = command.currentLayout;
	command.lifecycle.state = RAL_COMMAND_IDLE;
	CHECK( !Ral_ValidateBindGroupDynamicExact( &command, &exactPipeline,
		3u, &group, offsets, 2u ) );
	command.lifecycle.state = RAL_COMMAND_RECORDING;
	offsets[0] = 1u;
	CHECK( !Ral_ValidateBindGroupDynamicExact( &command, &exactPipeline,
		3u, &group, offsets, 2u ) );
	offsets[0] = 256u;
	uniformBuffer.size = 300u;
	CHECK( !Ral_ValidateBindGroupDynamicExact( &command, &exactPipeline,
		3u, &group, offsets, 2u ) );
	uniformBuffer.size = 2048u;
	CHECK( bindCalls == 0u && command.boundBindGroups[3] == &sentinel
		&& command.currentPipeline == &exactPipeline );
	CHECK( Ral_CmdBindBindGroupDynamicExact( &command, 3u, &group, offsets, 2u ) );
	CHECK( bindCalls == 1u && command.boundBindGroups[3] == &group );
	otherLayout = layout;
	otherLayout.layout = (VkDescriptorSetLayout)(uintptr_t)0x61u;
	memset( &sentinel, 0, sizeof( sentinel ) );
	command.boundBindGroups[3] = &sentinel;
	group.layout = &otherLayout;
	ResetCapture();
	CHECK( !Ral_ValidateBindGroupDynamicExact( &command, &exactPipeline,
		3u, &group, offsets, 2u ) );
	CHECK( !Ral_CmdBindBindGroupDynamicExact( &command, 3u, &group, offsets, 2u ) );
	CHECK( bindCalls == 0u && command.boundBindGroups[3] == &sentinel );
	group.layout = &layout;
	layoutVector[3] = &otherLayout;
	CHECK( !Ral_RegisterExternalPipelineBindGroupLayouts(
		&exactPipeline, 4u, layoutVector ) );
	layoutVector[3] = &layout;
	CHECK( Ral_CmdBindBindGroupDynamicExact( &command, 3u, &group, offsets, 2u ) );

	// Explicit-layout pushes are native-free: an adopted layout must publish
	// exact RAL-stage/range authority before it can emit. Registration is
	// idempotent/output-atomic, while every lifecycle/backend/stage/alignment/
	// range mutation rejects without reaching the backend callback.
	pushLayout = Ral_AdoptPipelineLayout( &backend,
		(void *)(uintptr_t)0x70u, NULL );
	CHECK( pushLayout != NULL );
	ResetPushCapture();
	CHECK( !Ral_CmdPushConstantsLayoutExact( &command, pushLayout,
		RAL_STAGE_FRAGMENT, 64u, 32u, pushPayload ) );
	CHECK( pushCalls == 0u && pushLayout->externalPushRangeCount == 0u );
	CHECK( Ral_RegisterExternalPipelineLayoutPushRange( pushLayout,
		RAL_STAGE_FRAGMENT, 64u, 32u ) );
	CHECK( Ral_RegisterExternalPipelineLayoutPushRange( pushLayout,
		RAL_STAGE_FRAGMENT, 64u, 32u )
		&& pushLayout->externalPushRangeCount == 1u );
	CHECK( !Ral_RegisterExternalPipelineLayoutPushRange( pushLayout,
		RAL_STAGE_FRAGMENT, 80u, 16u )
		&& pushLayout->externalPushRangeCount == 1u );
	CHECK( !Ral_RegisterExternalPipelineLayoutPushRange( pushLayout,
		RAL_STAGE_FRAGMENT, 128u, 4u )
		&& pushLayout->externalPushRangeCount == 1u );
	ResetPushCapture();
	CHECK( Ral_CmdPushConstantsLayoutExact( &command, pushLayout,
		RAL_STAGE_FRAGMENT, 64u, 32u, pushPayload ) );
	CHECK( pushCalls == 1u
		&& capturedPushLayout == pushLayout->vkHandle
		&& capturedPushStages == VK_SHADER_STAGE_FRAGMENT_BIT
		&& capturedPushOffset == 64u && capturedPushSize == 32u
		&& capturedPushWord == pushPayload[0] );
	ResetPushCapture();
	CHECK( !Ral_CmdPushConstantsLayoutExact( &command, pushLayout,
		RAL_STAGE_VERTEX, 64u, 32u, pushPayload ) );
	CHECK( !Ral_CmdPushConstantsLayoutExact( &command, pushLayout,
		1u << 7, 64u, 32u, pushPayload ) );
	CHECK( !Ral_CmdPushConstantsLayoutExact( &command, pushLayout,
		RAL_STAGE_FRAGMENT, 60u, 32u, pushPayload ) );
	CHECK( !Ral_CmdPushConstantsLayoutExact( &command, pushLayout,
		RAL_STAGE_FRAGMENT, 64u, 36u, pushPayload ) );
	CHECK( !Ral_CmdPushConstantsLayoutExact( &command, pushLayout,
		RAL_STAGE_FRAGMENT, 66u, 28u, pushPayload ) );
	CHECK( !Ral_CmdPushConstantsLayoutExact( &command, pushLayout,
		RAL_STAGE_FRAGMENT, 64u, 30u, pushPayload ) );
	CHECK( !Ral_CmdPushConstantsLayoutExact( &command, pushLayout,
		RAL_STAGE_FRAGMENT, 64u, 32u, NULL ) );
	otherPushLayout = *pushLayout;
	otherPushLayout.backend = &otherBackend;
	CHECK( !Ral_CmdPushConstantsLayoutExact( &command, &otherPushLayout,
		RAL_STAGE_FRAGMENT, 64u, 32u, pushPayload ) );
	command.state = RAL_VK_CMD_IDLE;
	command.lifecycle.state = RAL_COMMAND_IDLE;
	CHECK( !Ral_CmdPushConstantsLayoutExact( &command, pushLayout,
		RAL_STAGE_FRAGMENT, 64u, 32u, pushPayload ) );
	CHECK( pushCalls == 0u );
	command.state = RAL_VK_CMD_RECORDING;
	command.lifecycle.state = RAL_COMMAND_RECORDING;
	CHECK( Ral_CmdPushConstantsLayoutExact( &command, pushLayout,
		RAL_STAGE_FRAGMENT, 64u, 32u, pushPayload ) );
	CHECK( pushCalls == 1u );
	command.state = RAL_VK_CMD_IDLE;
	command.lifecycle.state = RAL_COMMAND_IDLE;
	command.state = RAL_VK_CMD_RECORDING;
	command.lifecycle.state = RAL_COMMAND_RECORDING;

	// Mid-pass clears are one exact portable transaction. RAL aspect bits and
	// bounded rectangles lower inside the backend only after the complete command
	// validates; every mutation below leaves the callback count unchanged.
	memset( &clearAttachment, 0, sizeof( clearAttachment ) );
	memset( &clearRect, 0, sizeof( clearRect ) );
	clearAttachment.aspectMask = RAL_TEXTURE_ASPECT_COLOR;
	clearAttachment.colorAttachment = 2u;
	clearAttachment.clearValue.color[0] = 0.25f;
	clearAttachment.clearValue.color[3] = 1.0f;
	clearRect.rect.x = 4; clearRect.rect.y = 8;
	clearRect.rect.width = 320u; clearRect.rect.height = 180u;
	clearRect.layerCount = 1u;
	command.renderingActive = qtrue;
	ResetClearCapture();
	CHECK( Ral_CmdClearAttachmentsExact( &command, 1u, &clearAttachment,
		1u, &clearRect ) );
	CHECK( clearCalls == 1u && capturedClearAttachmentCount == 1u
		&& capturedClearRectCount == 1u
		&& capturedClearAttachment.aspectMask == VK_IMAGE_ASPECT_COLOR_BIT
		&& capturedClearAttachment.colorAttachment == 2u
		&& capturedClearAttachment.clearValue.color.float32[0] == 0.25f
		&& capturedClearRect.rect.offset.x == 4
		&& capturedClearRect.rect.offset.y == 8
		&& capturedClearRect.rect.extent.width == 320u
		&& capturedClearRect.rect.extent.height == 180u
		&& capturedClearRect.layerCount == 1u );
	clearAttachment.aspectMask = RAL_TEXTURE_ASPECT_DEPTH | RAL_TEXTURE_ASPECT_STENCIL;
	clearAttachment.colorAttachment = 0u;
	clearAttachment.clearValue.depthStencil.depth = 0.0f;
	clearAttachment.clearValue.depthStencil.stencil = 7u;
	CHECK( Ral_CmdClearAttachmentsExact( &command, 1u, &clearAttachment,
		1u, &clearRect ) );
	CHECK( clearCalls == 2u
		&& capturedClearAttachment.aspectMask
			== ( VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT )
		&& capturedClearAttachment.clearValue.depthStencil.stencil == 7u );
	ResetClearCapture();
	command.state = RAL_VK_CMD_IDLE;
	command.lifecycle.state = RAL_COMMAND_IDLE;
	CHECK( !Ral_CmdClearAttachmentsExact( &command, 1u, &clearAttachment,
		1u, &clearRect ) );
	command.state = RAL_VK_CMD_RECORDING;
	command.lifecycle.state = RAL_COMMAND_RECORDING;
	CHECK( Ral_CmdClearAttachmentsExact( &command, 1u, &clearAttachment,
		1u, &clearRect ) );
	CHECK( clearCalls == 1u );
	ResetClearCapture();
	command.state = RAL_VK_CMD_IDLE;
	command.lifecycle.state = RAL_COMMAND_IDLE;
	command.state = RAL_VK_CMD_RECORDING;
	command.lifecycle.state = RAL_COMMAND_RECORDING;
	command.renderingActive = qfalse;
	CHECK( !Ral_CmdClearAttachmentsExact( &command, 1u, &clearAttachment, 1u, &clearRect ) );
	command.renderingActive = qtrue;
	clearAttachment.aspectMask = 0u;
	CHECK( !Ral_CmdClearAttachmentsExact( &command, 1u, &clearAttachment, 1u, &clearRect ) );
	clearAttachment.aspectMask = 1u << 7;
	CHECK( !Ral_CmdClearAttachmentsExact( &command, 1u, &clearAttachment, 1u, &clearRect ) );
	clearAttachment.aspectMask = RAL_TEXTURE_ASPECT_COLOR | RAL_TEXTURE_ASPECT_DEPTH;
	CHECK( !Ral_CmdClearAttachmentsExact( &command, 1u, &clearAttachment, 1u, &clearRect ) );
	clearAttachment.aspectMask = RAL_TEXTURE_ASPECT_COLOR;
	clearAttachment.colorAttachment = RAL_MAX_COLOR_ATTACHMENTS;
	CHECK( !Ral_CmdClearAttachmentsExact( &command, 1u, &clearAttachment, 1u, &clearRect ) );
	clearAttachment.aspectMask = RAL_TEXTURE_ASPECT_DEPTH;
	clearAttachment.colorAttachment = 1u;
	CHECK( !Ral_CmdClearAttachmentsExact( &command, 1u, &clearAttachment, 1u, &clearRect ) );
	clearAttachment.colorAttachment = 0u;
	clearRect.rect.x = -1;
	CHECK( !Ral_CmdClearAttachmentsExact( &command, 1u, &clearAttachment, 1u, &clearRect ) );
	clearRect.rect.x = 4; clearRect.rect.width = 0u;
	CHECK( !Ral_CmdClearAttachmentsExact( &command, 1u, &clearAttachment, 1u, &clearRect ) );
	clearRect.rect.width = 320u; clearRect.layerCount = 0u;
	CHECK( !Ral_CmdClearAttachmentsExact( &command, 1u, &clearAttachment, 1u, &clearRect ) );
	clearRect.layerCount = 1u; clearRect.baseArrayLayer = UINT32_MAX;
	CHECK( !Ral_CmdClearAttachmentsExact( &command, 1u, &clearAttachment, 1u, &clearRect ) );
	clearRect.baseArrayLayer = 0u;
	CHECK( !Ral_CmdClearAttachmentsExact( &command,
		RAL_MAX_COLOR_ATTACHMENTS + 1u, &clearAttachment, 1u, &clearRect ) );
	CHECK( !Ral_CmdClearAttachmentsExact( &command, 1u, &clearAttachment,
		RAL_MAX_COLOR_ATTACHMENTS + 1u, &clearRect ) );
	backend.vk.CmdClearAttachments = NULL;
	CHECK( !Ral_CmdClearAttachmentsExact( &command, 1u, &clearAttachment, 1u, &clearRect ) );
	backend.vk.CmdClearAttachments = CaptureClearAttachments;
	CHECK( clearCalls == 0u );
	command.renderingActive = qfalse;
	Ral_DestroyPipelineLayout( pushLayout );
	CHECK( destroyPipelineLayoutCalls == 0u );
	ownedLayout = Ral_AdoptOwnedPipelineLayout( &backend,
		(void *)(uintptr_t)0x71u, NULL );
	CHECK( ownedLayout != NULL
		&& Ral_GetPipelineLayoutHandle( ownedLayout ) == (void *)(uintptr_t)0x71u );
	Ral_DestroyPipelineLayout( ownedLayout );
	CHECK( destroyPipelineLayoutCalls == 1u
		&& capturedDestroyedPipelineLayout == (VkPipelineLayout)(uintptr_t)0x71u );
	ResetPushCapture();
	command.currentPipeline = &pipeline;

	// Every invalid mutation rejects before the backend callback and preserves
	// the previously published binding slot.
	memset( &sentinel, 0, sizeof( sentinel ) );
	command.boundBindGroups[3] = &sentinel;
	ResetCapture();
	CHECK( !ralVk_CmdBindBindGroupDynamic( &command, 3u, &group, NULL, 2u ) );
	CHECK( !ralVk_CmdBindBindGroupDynamic( &command, 3u, &group, offsets, 1u ) );
	offsets[0] = 128u;
	CHECK( !ralVk_CmdBindBindGroupDynamic( &command, 3u, &group, offsets, 2u ) );
	offsets[0] = 256u; offsets[1] = 17u;
	CHECK( !ralVk_CmdBindBindGroupDynamic( &command, 3u, &group, offsets, 2u ) );
	offsets[1] = 32u;
	group.dynamicBindings[0].registered = qfalse;
	CHECK( !ralVk_CmdBindBindGroupDynamic( &command, 3u, &group, offsets, 2u ) );
	group.dynamicBindings[0].registered = qtrue;
	group.dynamicBindings[0].range = 1800u;
	CHECK( !ralVk_CmdBindBindGroupDynamic( &command, 3u, &group, offsets, 2u ) );
	group.dynamicBindings[0].range = 112u;
	group.backend = &otherBackend;
	CHECK( !ralVk_CmdBindBindGroupDynamic( &command, 3u, &group, offsets, 2u ) );
	group.backend = &backend;
	uniformBuffer.legacyMapped = qtrue;
	CHECK( !ralVk_CmdBindBindGroupDynamic( &command, 3u, &group, offsets, 2u ) );
	uniformBuffer.legacyMapped = qfalse;
	command.currentPipeline = NULL;
	CHECK( !ralVk_CmdBindBindGroupDynamic( &command, 3u, &group, offsets, 2u ) );
	command.currentPipeline = &pipeline;
	CHECK( !ralVk_CmdBindBindGroupDynamic( &command, RAL_VK_MAX_TRACKED_BIND_GROUPS,
		&group, offsets, 2u ) );
	CHECK( bindCalls == 0u && command.boundBindGroups[3] == &sentinel );

	// A non-dynamic group keeps the compatibility call shape: zero offsets.
	layout.dynamicOffsetCount = 0u;
	group.dynamicOffsetCount = 0u;
	group.bufferCount = 0u;
	ResetCapture();
	CHECK( ralVk_CmdBindBindGroupDynamic( &command, 1u, &group, NULL, 0u ) );
	CHECK( bindCalls == 1u && capturedCount == 0u && command.boundBindGroups[1] == &group );

	// Every exact portable begin retires weak prior-recording bindings before
	// any new command can observe them.
	Ral_CommandLifecycleInit( &command.lifecycle, &backend, &command, command.queue );
	command.state = RAL_VK_CMD_IDLE;
	command.currentPipeline = &pipeline;
	command.currentLayout = (VkPipelineLayout)(uintptr_t)0x20u;
	command.boundVertexBuffers[0] = &uniformBuffer;
	command.boundIndexBuffer = &storageBuffer;
	command.boundBindGroups[0] = &group;
	command.renderingDebugLabelActive = qtrue;
	command.renderingActive = qtrue;
	command.debugLabelBeginCount = command.debugLabelEndCount = 7u;
	CHECK( Ral_BeginCommandBufferExact( &command, &commandRecording ) == ralSuccess );
	CHECK( command.currentPipeline == NULL
		&& command.currentLayout == VK_NULL_HANDLE
		&& command.boundVertexBuffers[0] == NULL
		&& command.boundIndexBuffer == NULL
		&& command.boundBindGroups[0] == NULL
		&& command.renderingDebugLabelActive == qfalse
		&& command.renderingActive == qfalse
		&& command.debugLabelBeginCount == 0u
		&& command.debugLabelEndCount == 0u );
	command.currentPipeline = &pipeline;
	command.currentLayout = (VkPipelineLayout)(uintptr_t)0x20u;

	// The public migration bridge retains the same portable declaration:
	// dynamic uniform entries become Vulkan *_DYNAMIC metadata, the adopted
	// descriptor registers one exact real-buffer range, and the command path
	// consumes the resulting one-offset vector.
	memset( &adoptedEntry, 0, sizeof( adoptedEntry ) );
	adoptedEntry.binding = 0u;
	adoptedEntry.type = RAL_BIND_UNIFORM_BUFFER;
	adoptedEntry.count = 1u;
	adoptedEntry.stageFlags = RAL_STAGE_VERTEX | RAL_STAGE_FRAGMENT;
	adoptedEntry.dynamicOffset = qtrue;
	adoptedLayout = Ral_AdoptBindGroupLayout( &backend,
		(void *)(uintptr_t)0x50u, 1u, &adoptedEntry, NULL );
	CHECK( adoptedLayout != NULL );
	CHECK( adoptedLayout->dynamicOffsetCount == 1u );
	CHECK( adoptedLayout->entries[0].vkType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC );
	memset( &adoptedBufferInfo, 0, sizeof( adoptedBufferInfo ) );
	adoptedBufferInfo.size = 2048u;
	adoptedBufferInfo.usage = RAL_BUFFER_UNIFORM | RAL_BUFFER_TRANSFER_DST;
	adoptedBufferInfo.memory = RAL_MEMORY_HOST_COHERENT;
	adoptedBuffer = Ral_AdoptBufferExact( &backend,
		(void *)(uintptr_t)0x51u, &adoptedBufferInfo );
	CHECK( adoptedBuffer != NULL );
	CHECK( Ral_GetBufferHandle( adoptedBuffer ) == (void *)(uintptr_t)0x51u );
	CHECK( Ral_GetBufferSize( adoptedBuffer ) == 2048u );
	CHECK( Ral_GetBufferUsage( adoptedBuffer )
		== ( RAL_BUFFER_UNIFORM | RAL_BUFFER_TRANSFER_DST ) );
	CHECK( Ral_GetBufferMemoryType( adoptedBuffer ) == RAL_MEMORY_HOST_COHERENT );
	adoptedGroup = Ral_AdoptBindGroup( &backend,
		(void *)(uintptr_t)0x52u, adoptedLayout, NULL );
	CHECK( adoptedGroup != NULL );
	CHECK( Ral_RegisterAdoptedBindGroupDynamicBuffer(
		adoptedGroup, 0u, adoptedBuffer, 0u, 112u ) );
	CHECK( !Ral_RegisterAdoptedBindGroupDynamicBuffer(
		adoptedGroup, 0u, adoptedBuffer, 0u, 112u ) );
	ResetCapture();
	CHECK( Ral_CmdBindBindGroupDynamic( &command, 2u,
		adoptedGroup, &adoptedOffset, 1u ) );
	CHECK( bindCalls == 1u && capturedSetIndex == 2u
		&& capturedCount == 1u && capturedOffsets[0] == adoptedOffset );
	Ral_DestroyBindGroup( adoptedGroup );
	Ral_DestroyBuffer( adoptedBuffer );
	Ral_DestroyBindGroupLayout( adoptedLayout );

	// Exact adoption rejects incomplete/forged metadata output-atomically.
	adoptedBufferInfo.size = 0u;
	CHECK( Ral_AdoptBufferExact( &backend,
		(void *)(uintptr_t)0x55u, &adoptedBufferInfo ) == NULL );
	adoptedBufferInfo.size = 64u;
	adoptedBufferInfo.usage = (ralBufferUsage_t)0;
	CHECK( Ral_AdoptBufferExact( &backend,
		(void *)(uintptr_t)0x55u, &adoptedBufferInfo ) == NULL );
	adoptedBufferInfo.usage = RAL_BUFFER_VERTEX;
	adoptedBufferInfo.memory = (ralMemoryType_t)( RAL_MEMORY_LAZY_ALLOC + 1 );
	CHECK( Ral_AdoptBufferExact( &backend,
		(void *)(uintptr_t)0x55u, &adoptedBufferInfo ) == NULL );

	adoptedEntry.type = RAL_BIND_SAMPLER;
	CHECK( Ral_AdoptBindGroupLayout( &backend,
		(void *)(uintptr_t)0x53u, 1u, &adoptedEntry, NULL ) == NULL );
	adoptedEntry.type = RAL_BIND_UNIFORM_BUFFER;
	adoptedEntry.count = 2u;
	CHECK( Ral_AdoptBindGroupLayout( &backend,
		(void *)(uintptr_t)0x54u, 1u, &adoptedEntry, NULL ) == NULL );

	puts( "PASS Vulkan portable dynamic bind offsets" );
	return 0;
}
