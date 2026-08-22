// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_vulkan_internal.h"

static qboolean ralVk_PipelineSetLayoutMatches(
		const ralPipeline_t *pipeline, uint32_t setIndex,
		const ralBindGroup_t *group, qboolean requireExact ) {
	if ( !pipeline || !group ) return qfalse;
	if ( !pipeline->bindGroupLayoutsRegistered )
		return requireExact ? qfalse : qtrue;
	return setIndex < pipeline->numSetLayouts
		&& pipeline->setLayouts[setIndex] == group->layout->layout;
}

static qboolean ralVk_BindGroupDynamicValid( const ralCommandBuffer_t *cb,
		const ralPipeline_t *pipeline, uint32_t setIndex,
		const ralBindGroup_t *group,
		const uint32_t *dynamicOffsets, uint32_t dynamicOffsetCount,
		qboolean requireExactLayout ) {
	uint32_t i;
	if ( !cb || !group || !cb->backend || group->backend != cb->backend
	  || group->set == VK_NULL_HANDLE
	  || !group->layout || group->layout->backend != cb->backend
	  || !ralVk_BindGroupArenaLive( group )
	  || setIndex >= RAL_VK_MAX_TRACKED_BIND_GROUPS
	  || !pipeline || pipeline->backend != cb->backend
	  || ( requireExactLayout && pipeline->layout == VK_NULL_HANDLE )
	  || cb->state != RAL_VK_CMD_RECORDING
	  || cb->lifecycle.state != RAL_COMMAND_RECORDING
	  || !cb->backend->vk.CmdBindDescriptorSets
	  || dynamicOffsetCount != group->dynamicOffsetCount
	  || dynamicOffsetCount != group->layout->dynamicOffsetCount
	  || dynamicOffsetCount > RAL_VK_MAX_DYNAMIC_OFFSETS
	  || ( dynamicOffsetCount > 0 && !dynamicOffsets )
	  || !ralVk_BindGroupBuffersGpuUseAllowed( group )
	  || !ralVk_PipelineSetLayoutMatches(
		  pipeline, setIndex, group, requireExactLayout ) ) return qfalse;

	for ( i = 0; i < dynamicOffsetCount; ++i ) {
		const ralVkDynamicBufferBinding_t *dynamic = &group->dynamicBindings[i];
		uint64_t alignment, offset = dynamicOffsets[i];
		if ( !dynamic->registered || !dynamic->buffer
		  || dynamic->buffer->backend != cb->backend
		  || !ralVk_BufferGpuUseAllowed( dynamic->buffer ) ) return qfalse;
		if ( dynamic->vkType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC )
			alignment = cb->backend->caps.minUniformBufferAlignment;
		else if ( dynamic->vkType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC )
			alignment = cb->backend->caps.minStorageBufferAlignment;
		else return qfalse;
		if ( alignment == 0 ) alignment = 1;
		if ( offset % alignment != 0
		  || dynamic->baseOffset > dynamic->buffer->size
		  || offset > dynamic->buffer->size - dynamic->baseOffset
		  || dynamic->range > dynamic->buffer->size - dynamic->baseOffset - offset )
			return qfalse;
	}
	return qtrue;
}

qboolean ralVk_ValidateBindGroupDynamicExact( const ralCommandBuffer_t *cb,
		const ralPipeline_t *pipeline, uint32_t setIndex,
		const ralBindGroup_t *group, const uint32_t *dynamicOffsets,
		uint32_t dynamicOffsetCount ) {
	return ralVk_BindGroupDynamicValid( cb, pipeline, setIndex, group,
		dynamicOffsets, dynamicOffsetCount, qtrue );
}

static qboolean ralVk_CmdBindBindGroupDynamicCore( ralCommandBuffer_t *cb,
		uint32_t setIndex, ralBindGroup_t *group,
		const uint32_t *dynamicOffsets, uint32_t dynamicOffsetCount,
		qboolean requireExactLayout ) {
	if ( !cb || !cb->currentPipeline || cb->currentLayout == VK_NULL_HANDLE
		  || ( requireExactLayout
			  && cb->currentLayout != cb->currentPipeline->layout )
		  || !ralVk_BindGroupDynamicValid( cb, cb->currentPipeline, setIndex,
			  group, dynamicOffsets, dynamicOffsetCount,
			  requireExactLayout ) ) return qfalse;

	cb->backend->vk.CmdBindDescriptorSets( cb->cb, cb->currentBindPoint,
		cb->currentLayout, setIndex, 1, &group->set,
		dynamicOffsetCount, dynamicOffsets );
	cb->boundBindGroups[setIndex] = group;
	return qtrue;
}

qboolean ralVk_CmdBindBindGroupDynamic( ralCommandBuffer_t *cb, uint32_t setIndex,
		ralBindGroup_t *group, const uint32_t *dynamicOffsets,
		uint32_t dynamicOffsetCount ) {
	return ralVk_CmdBindBindGroupDynamicCore( cb, setIndex, group,
		dynamicOffsets, dynamicOffsetCount, qfalse );
}

qboolean ralVk_CmdBindBindGroupDynamicExact( ralCommandBuffer_t *cb,
		uint32_t setIndex, ralBindGroup_t *group,
		const uint32_t *dynamicOffsets, uint32_t dynamicOffsetCount ) {
	return ralVk_CmdBindBindGroupDynamicCore( cb, setIndex, group,
		dynamicOffsets, dynamicOffsetCount, qtrue );
}
