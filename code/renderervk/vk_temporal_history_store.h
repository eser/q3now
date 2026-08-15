// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#ifndef WIRED_VK_TEMPORAL_HISTORY_STORE_H
#define WIRED_VK_TEMPORAL_HISTORY_STORE_H

#include "tr_temporal_history.h"
#include "vk_temporal_resolved_hdr.h"
#include "../renderer/ral/ral_command.h"
#include "../renderer/ral/ral_pipeline.h"

typedef struct {
	ralBackend_t *backend;
	int32_t worldIndex;
	uint32_t width, height, topologyEpoch;
	uint32_t historyAllocationGeneration;
	uint32_t sceneColorAttachmentGeneration;
	uint32_t resolvedTargetAllocationGeneration;
	ralFormat_t sceneFormat;
	ralTexture_t *feedbackColor;
	ralTextureView_t *feedbackColorView;
	ralTexture_t *currentDepth;
	ralTexture_t *historyColor[2];
	ralTextureView_t *historyColorView[2];
	ralTexture_t *historyDepth[2];
	ralTextureView_t *historyDepthView[2];
	const uint32_t *computeSpirv;
	size_t computeSpirvSize;
} vkTemporalHistoryStoreKey_t;

typedef struct {
	uint32_t extent[2];
	float zNear, zFar;
} vkTemporalHistoryStorePush_t;

typedef struct {
	vkTemporalHistoryStoreKey_t key;
	ralTextureView_t *currentDepthView;
	ralSampler_t *sampler;
	ralBindGroupLayout_t *layout;
	ralBindGroup_t *groups[2];
	ralPipeline_t *pipeline;
	uint32_t allocationGeneration;
	qboolean initialized;
	qboolean ready;
} vkTemporalHistoryStoreOwner_t;

void VK_TemporalHistoryStoreInit( vkTemporalHistoryStoreOwner_t *owner );
qboolean VK_TemporalHistoryStoreNeedsIdle(
	const vkTemporalHistoryStoreOwner_t *owner,
	const vkTemporalHistoryStoreKey_t *key );
qboolean VK_TemporalHistoryStoreEnsureAfterFence(
	vkTemporalHistoryStoreOwner_t *owner,
	const vkTemporalHistoryStoreKey_t *key, qboolean idleProven );
qboolean VK_TemporalHistoryStoreMatchesExact(
	const vkTemporalHistoryStoreOwner_t *owner,
	const vkTemporalHistoryStoreKey_t *key );
qboolean VK_TemporalHistoryStoreRecord(
	const vkTemporalHistoryStoreOwner_t *owner,
	const vkTemporalHistoryStoreKey_t *expectedKey,
	uint32_t expectedAllocationGeneration,
	ralCommandBuffer_t *commandBuffer, uint32_t writeIndex,
	const vkTemporalHistoryStorePush_t *push );
qboolean VK_TemporalHistoryStoreHasLive(
	const vkTemporalHistoryStoreOwner_t *owner );
qboolean VK_TemporalHistoryStoreReleaseAfterIdle(
	vkTemporalHistoryStoreOwner_t *owner, qboolean idleProven );

#endif
