// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#ifndef WIRED_VK_TEMPORAL_RESOLVE_AUTHORITY_H
#define WIRED_VK_TEMPORAL_RESOLVE_AUTHORITY_H

#include "tr_temporal_history.h"
#include "vk_temporal_main_activation.h"
#include "vk_temporal_resolved_hdr.h"

typedef struct {
	ralBackend_t *backend;
	ralTexture_t *currentColor;
	ralTexture_t *currentDepth;
	uint32_t sceneColorAttachmentGeneration;
	uint32_t commandSlot;
	uint32_t frameCount;
	float zNear;
	float zFar;
	const ralTemporalFramePlan_t *plan;
	const temporalHistoryFrameView_t *history;
	const vkTemporalMainActivationReceipt_t *activation;
	const vkTemporalMotionMaterializationReceipt_t *motion;
	const vkTemporalMotionMaterializationProductView_t *motionProducts;
	const vkTemporalResolvedHdrReceipt_t *resolved;
} vkTemporalResolveAuthorityInput_t;

typedef struct {
	uint64_t batchToken;
	uint64_t frameId;
	uint64_t previousFrameId;
	int32_t worldIndex;
	uint32_t width;
	uint32_t height;
	uint32_t topologyEpoch;
	uint32_t planGeneration;
	uint32_t commandSlot;
	uint32_t frameCount;
	uint32_t historyAllocationGeneration;
	uint32_t historyReadIndex;
	uint32_t historyWriteIndex;
	temporalHistoryFeedbackSource_t previousHistorySource;
	uint32_t motionTargetAllocationGeneration;
	uint32_t motionPipelineLayoutAllocationGeneration;
	uint32_t motionMaterializationGeneration;
	uint32_t pipelineTableGeneration;
	uint32_t sceneColorAttachmentGeneration;
	uint32_t resolvedTargetAllocationGeneration;
	uint32_t temporalSegments;
	uint32_t written;
	uint32_t invalidated;
	float currentEffectiveJitterUv[2];
	float previousEffectiveJitterUv[2];
	float zNear;
	float zFar;
	vkTemporalMotionDrawSequence_t drawSequence;
	vkTemporalMainActivationReceipt_t activation;
	qboolean ready;
} vkTemporalResolveAuthorityReceipt_t;

typedef struct {
	ralBackend_t *backend;
	ralTexture_t *currentColor;
	ralTexture_t *currentDepth;
	ralTexture_t *previousColor;
	ralTextureView_t *previousColorView;
	ralTexture_t *previousDepth;
	ralTextureView_t *previousDepthView;
	ralTexture_t *velocity;
	ralTextureView_t *velocityView;
	ralTexture_t *validity;
	ralTextureView_t *validityView;
	ralTexture_t *resolvedTarget;
	ralTextureView_t *resolvedTargetView;
} vkTemporalResolveProductView_t;

// Joins the immutable pre-submit activation receipt with the exact committed
// physical history slot and both resource-owner generations. It records no GPU
// command and publishes both outputs atomically.
qboolean VK_TemporalResolveBuildAuthority(
	const vkTemporalResolveAuthorityInput_t *input,
	vkTemporalResolveAuthorityReceipt_t *outReceipt,
	vkTemporalResolveProductView_t *outProducts );

#endif
