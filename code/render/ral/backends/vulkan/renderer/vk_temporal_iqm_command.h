// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#ifndef WIRED_VK_TEMPORAL_IQM_COMMAND_H
#define WIRED_VK_TEMPORAL_IQM_COMMAND_H

#include "vk_temporal_main_activation.h"
#include "vk_temporal_main_rendering.h"

typedef struct {
	ralCommandBuffer_t *commandBuffer;
	ralTexture_t *scene;
	ralTexture_t *velocity;
	ralTexture_t *validity;
	ralTexture_t *depth;
	ralBindGroup_t *payloadGroup;
	ralBindGroup_t *bindlessGroup;
	ralBuffer_t *vertexBuffer;
	ralBuffer_t *indexBuffer;
	vkTemporalIqmExact3FactoryReceipt_t currentFactory;
	uint32_t width;
	uint32_t height;
	qboolean hasStencil;
	qboolean ordinaryOpen;
} vkTemporalIqmCommandResources_t;

typedef struct {
	qboolean (*preflight)( void *context,
		const vkTemporalMainIqmActivationPlan_t *plan,
		const vkTemporalIqmCommandResources_t *resources );
	void (*endOrdinary)( void *context );
	void (*barrier)( void *context );
	void (*beginExact)( void *context, const ralRenderingInfo_t *info );
	void (*setDynamicState)( void *context );
	void (*bindPipeline)( void *context, ralPipeline_t *pipeline );
	void (*bindGroup)( void *context, uint32_t setIndex,
		ralBindGroup_t *group );
	void (*push)( void *context, const vkTemporalIqmExact3Push_t *push );
	void (*bindVertex)( void *context, ralBuffer_t *buffer );
	void (*bindIndex)( void *context, ralBuffer_t *buffer );
	void (*drawIndexed)( void *context, uint32_t indexCount,
		uint32_t firstIndex, uint32_t firstInstance );
	void (*endExact)( void *context );
	void (*beginResume)( void *context, const ralRenderingInfo_t *info );
	void (*resetOrdinaryState)( void *context );
} vkTemporalIqmCommandOps_t;

// All validation and the product-specific preflight happen before endOrdinary.
// After that seam every operation is deliberately void/infallible. A false
// return therefore means no command was emitted and ordinary fallback is safe;
// true means the exact draw was emitted exactly once and must never be replayed.
qboolean VK_TemporalIqmCommandExecute(
	const vkTemporalMainIqmActivationPlan_t *plan,
	const vkTemporalIqmCommandResources_t *resources,
	void *context, const vkTemporalIqmCommandOps_t *ops );

#endif
