// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#ifndef WIRED_VK_RAL_POOL_RENDER_H
#define WIRED_VK_RAL_POOL_RENDER_H

#include "../../../core/ral.h"

#define VK_RAL_POOL_RENDER_MAX_PIPELINES 3u

typedef struct {
	ralCommandBuffer_t *commandBuffer;
	ralBindGroup_t *bindGroup;
	ralPipeline_t *pipelines[VK_RAL_POOL_RENDER_MAX_PIPELINES];
	uint32_t pipelineCount;
	ralViewport_t viewport;
	ralRect_t scissor;
	uint32_t vertexCount;
	uint32_t instanceCount;
	uint32_t firstVertex;
	uint32_t firstInstance;
} vkRalPoolRenderPlan_t;

// Emits one backend-neutral instanced pool draw per pipeline. Validation is
// complete before the first command, so qfalse is guaranteed command-inert.
qboolean VK_RalPoolRenderExecute( const vkRalPoolRenderPlan_t *plan );

#define VK_RAL_COMPUTE_MAX_GROUPS_PER_DIMENSION 65535u

typedef struct {
	ralCommandBuffer_t *commandBuffer;
	ralPipeline_t *pipeline;
	ralBindGroup_t *bindGroup;
	uint32_t groupCountX;
	uint32_t groupCountY;
	uint32_t groupCountZ;
} vkRalComputePlan_t;

// Emits one compute dispatch and the semantic compute-to-graphics boundary.
// Structural validation is complete before the first command. Resource-level
// state remains owned by the caller; this plan deliberately does not infer it.
qboolean VK_RalComputeExecute( const vkRalComputePlan_t *plan );

#endif
