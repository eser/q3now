// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "vk_ral_pool_render.h"

qboolean VK_RalPoolRenderExecute( const vkRalPoolRenderPlan_t *plan ) {
	uint32_t i;
	if ( !plan || !plan->commandBuffer || !plan->bindGroup
			|| !plan->pipelineCount
			|| plan->pipelineCount > VK_RAL_POOL_RENDER_MAX_PIPELINES
			|| plan->viewport.width <= 0.0f || plan->viewport.height <= 0.0f
			|| plan->viewport.minDepth < 0.0f
			|| plan->viewport.maxDepth > 1.0f
			|| plan->viewport.minDepth > plan->viewport.maxDepth
			|| !plan->scissor.width || !plan->scissor.height
			|| !plan->vertexCount || !plan->instanceCount ) return qfalse;
	for ( i = 0; i < plan->pipelineCount; i++ ) {
		if ( !plan->pipelines[i] ) return qfalse;
	}

	Ral_CmdSetViewport( plan->commandBuffer, &plan->viewport );
	Ral_CmdSetScissor( plan->commandBuffer, &plan->scissor );
	for ( i = 0; i < plan->pipelineCount; i++ ) {
		Ral_CmdBindPipeline( plan->commandBuffer, plan->pipelines[i] );
		Ral_CmdBindBindGroup( plan->commandBuffer, 0u, plan->bindGroup );
		Ral_CmdDraw( plan->commandBuffer, plan->vertexCount,
			plan->instanceCount, plan->firstVertex, plan->firstInstance );
	}
	return qtrue;
}

qboolean VK_RalComputeExecute( const vkRalComputePlan_t *plan ) {
	if ( !plan || !plan->commandBuffer || !plan->pipeline || !plan->bindGroup
			|| !plan->groupCountX || !plan->groupCountY || !plan->groupCountZ
			|| plan->groupCountX > VK_RAL_COMPUTE_MAX_GROUPS_PER_DIMENSION
			|| plan->groupCountY > VK_RAL_COMPUTE_MAX_GROUPS_PER_DIMENSION
			|| plan->groupCountZ > VK_RAL_COMPUTE_MAX_GROUPS_PER_DIMENSION ) return qfalse;

	Ral_CmdBindPipeline( plan->commandBuffer, plan->pipeline );
	Ral_CmdBindBindGroup( plan->commandBuffer, 0u, plan->bindGroup );
	Ral_CmdDispatch( plan->commandBuffer,
		plan->groupCountX, plan->groupCountY, plan->groupCountZ );
	Ral_CmdPipelineBarrier( plan->commandBuffer, RAL_BARRIER_COMPUTE_TO_GRAPHICS );
	return qtrue;
}
