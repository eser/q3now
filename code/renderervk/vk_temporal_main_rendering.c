// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "vk_temporal_main_rendering.h"

#include <string.h>
#include "../renderercommon/vulkan/vulkan.h"

static void Common( ralRenderingInfo_t *info, ralTexture_t *depth,
		uint32_t width, uint32_t height, qboolean hasStencil ) {
	info->depthAttachment = depth;
	info->renderArea.x = 0;
	info->renderArea.y = 0;
	info->renderArea.width = width;
	info->renderArea.height = height;
	if ( !hasStencil ) {
		info->stencilLoadOp = RAL_LOAD_OP_DONT_CARE;
		info->stencilStoreOp = RAL_STORE_OP_DONT_CARE;
	}
}

qboolean VK_TemporalMainRenderingBuildInitial(
		ralTexture_t *scene, ralTexture_t *depth, uint32_t width, uint32_t height,
		qboolean hasStencil, qboolean storeDepthStencil,
		ralRenderingInfo_t *outInfo ) {
	ralRenderingInfo_t info;
	if ( !scene || !depth || !width || !height || !outInfo ) return qfalse;
	memset( &info, 0, sizeof( info ) );
	info.colorAttachments[0] = scene;
	info.colorLoadOps[0] = RAL_LOAD_OP_CLEAR;
	info.colorStoreOps[0] = RAL_STORE_OP_STORE;
	info.numColorAttachments = 1;
	info.depthLoadOp = RAL_LOAD_OP_CLEAR;
	info.depthStoreOp = storeDepthStencil
		? RAL_STORE_OP_STORE : RAL_STORE_OP_DONT_CARE;
	info.stencilLoadOp = hasStencil ? RAL_LOAD_OP_CLEAR : RAL_LOAD_OP_DONT_CARE;
	info.stencilStoreOp = hasStencil && storeDepthStencil
		? RAL_STORE_OP_STORE : RAL_STORE_OP_DONT_CARE;
	Common( &info, depth, width, height, hasStencil );
	*outInfo = info;
	return qtrue;
}

qboolean VK_TemporalMainRenderingBuildExact3(
		ralTexture_t *scene, ralTexture_t *velocity, ralTexture_t *validity,
		ralTexture_t *depth, uint32_t width, uint32_t height,
		qboolean hasStencil, qboolean clearAuxiliary,
		ralRenderingInfo_t *outInfo ) {
	ralRenderingInfo_t info;
	uint32_t i;
	if ( !scene || !velocity || !validity || !depth || !width || !height
			|| !outInfo || scene == velocity || scene == validity
			|| velocity == validity || scene == depth
			|| velocity == depth || validity == depth ) return qfalse;
	memset( &info, 0, sizeof( info ) );
	info.colorAttachments[0] = scene;
	info.colorAttachments[1] = velocity;
	info.colorAttachments[2] = validity;
	info.colorLoadOps[0] = RAL_LOAD_OP_LOAD;
	info.colorLoadOps[1] = clearAuxiliary ? RAL_LOAD_OP_CLEAR : RAL_LOAD_OP_LOAD;
	info.colorLoadOps[2] = clearAuxiliary ? RAL_LOAD_OP_CLEAR : RAL_LOAD_OP_LOAD;
	for ( i = 0; i < 3; ++i ) info.colorStoreOps[i] = RAL_STORE_OP_STORE;
	info.numColorAttachments = 3;
	info.depthLoadOp = RAL_LOAD_OP_LOAD;
	info.depthStoreOp = RAL_STORE_OP_STORE;
	info.stencilLoadOp = hasStencil ? RAL_LOAD_OP_LOAD : RAL_LOAD_OP_DONT_CARE;
	info.stencilStoreOp = hasStencil ? RAL_STORE_OP_STORE : RAL_STORE_OP_DONT_CARE;
	Common( &info, depth, width, height, hasStencil );
	*outInfo = info;
	return qtrue;
}

qboolean VK_TemporalMainRenderingBuildResume(
		ralTexture_t *scene, ralTexture_t *depth, uint32_t width, uint32_t height,
		qboolean hasStencil, ralRenderingInfo_t *outInfo ) {
	ralRenderingInfo_t info;
	if ( !scene || !depth || !width || !height || !outInfo
			|| scene == depth ) return qfalse;
	memset( &info, 0, sizeof( info ) );
	info.colorAttachments[0] = scene;
	info.colorLoadOps[0] = RAL_LOAD_OP_LOAD;
	info.colorStoreOps[0] = RAL_STORE_OP_STORE;
	info.numColorAttachments = 1;
	info.depthLoadOp = RAL_LOAD_OP_LOAD;
	info.depthStoreOp = RAL_STORE_OP_STORE;
	info.stencilLoadOp = hasStencil ? RAL_LOAD_OP_LOAD : RAL_LOAD_OP_DONT_CARE;
	info.stencilStoreOp = hasStencil ? RAL_STORE_OP_STORE : RAL_STORE_OP_DONT_CARE;
	Common( &info, depth, width, height, hasStencil );
	*outInfo = info;
	return qtrue;
}

qboolean VK_TemporalMainRenderingBuildAttachmentBarrier(
		ralMemoryBarrier_t *outMemory, ralPipelineBarrierInfo_t *outBarrier ) {
	ralMemoryBarrier_t memory;
	ralPipelineBarrierInfo_t barrier;
	if ( !outMemory || !outBarrier ) return qfalse;
	memset( &memory, 0, sizeof( memory ) );
	memset( &barrier, 0, sizeof( barrier ) );
	memory.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
		| VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	memory.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT
		| VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
		| VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT
		| VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	barrier.srcStageMask = RAL_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
		| RAL_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
		| RAL_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	barrier.dstStageMask = barrier.srcStageMask;
	barrier.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
	barrier.memoryBarrierCount = 1;
	barrier.memoryBarriers = outMemory;
	*outMemory = memory;
	*outBarrier = barrier;
	return qtrue;
}
