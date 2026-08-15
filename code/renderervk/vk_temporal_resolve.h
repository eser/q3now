// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#ifndef WIRED_VK_TEMPORAL_RESOLVE_H
#define WIRED_VK_TEMPORAL_RESOLVE_H

#include "vk_temporal_resolve_authority.h"
#include "../renderer/ral/ral_command.h"
#include "../renderer/ral/ral_pipeline.h"

typedef struct {
	ralBackend_t *backend;
	uint32_t width, height, topologyEpoch;
	uint32_t sceneColorAttachmentGeneration;
	uint32_t historyAllocationGeneration;
	uint32_t motionTargetAllocationGeneration;
	uint32_t resolvedTargetAllocationGeneration;
	ralTexture_t *currentColor;
	ralTexture_t *currentDepth;
	ralTexture_t *historyColor[2];
	ralTextureView_t *historyColorView[2];
	ralTexture_t *historyDepth[2];
	ralTextureView_t *historyDepthView[2];
	ralTexture_t *velocity;
	ralTextureView_t *velocityView;
	ralTexture_t *validity;
	ralTextureView_t *validityView;
	ralTexture_t *resolvedTarget;
	ralTextureView_t *resolvedTargetView;
	const uint32_t *computeSpirv;
	size_t computeSpirvSize;
} vkTemporalResolveKey_t;

typedef struct {
	uint32_t extent[2];
	float currentEffectiveJitterUv[2];
	float previousEffectiveJitterUv[2];
	float zNear, zFar;
	float depthThresholdAbsolute, depthThresholdRelative;
	float historyWeight;
	uint32_t historyReadIndex;
} vkTemporalResolvePush_t;

typedef struct {
	vkTemporalResolveKey_t key;
	ralTextureView_t *currentColorView;
	ralTextureView_t *currentDepthView;
	ralSampler_t *nearestSampler;
	ralBindGroupLayout_t *layout;
	ralBindGroup_t *groups[2];
	ralPipeline_t *pipeline;
	uint32_t allocationGeneration;
	qboolean initialized;
	qboolean ready;
} vkTemporalResolveOwner_t;

typedef struct {
	uint32_t allocationGeneration;
	uint32_t width, height, topologyEpoch;
	uint32_t sceneColorAttachmentGeneration;
	uint32_t historyAllocationGeneration;
	uint32_t motionTargetAllocationGeneration;
	uint32_t resolvedTargetAllocationGeneration;
	qboolean ready;
} vkTemporalResolveOwnerReceipt_t;

typedef struct {
	vkTemporalResolveAuthorityReceipt_t authority;
	// Exact borrowed product cohort bound by the dispatch.  Diagnostics may
	// consume only this snapshot, never a later renderer-global reconstruction.
	vkTemporalResolveProductView_t products;
	vkTemporalResolvedHdrContentReceipt_t content;
	temporalHistoryCommittedReceipt_t committedWriteExpected;
	// Exact shader ABI payload recorded for this command.  H3 diagnostics
	// consume this immutable snapshot after submit; rebuilding it from mutable
	// renderer state would not prove the dispatch that actually ran.
	vkTemporalResolvePush_t push;
	uint32_t ownerAllocationGeneration;
	qboolean recorded;
	qboolean submitted;
} vkTemporalResolveTicket_t;

void VK_TemporalResolveInit( vkTemporalResolveOwner_t *owner );
qboolean VK_TemporalResolveNeedsIdle(
	const vkTemporalResolveOwner_t *owner, const vkTemporalResolveKey_t *key );
qboolean VK_TemporalResolveEnsureAfterFence(
	vkTemporalResolveOwner_t *owner, const vkTemporalResolveKey_t *key,
	qboolean idleProven );
qboolean VK_TemporalResolveGetReceipt(
	const vkTemporalResolveOwner_t *owner,
	vkTemporalResolveOwnerReceipt_t *outReceipt );
qboolean VK_TemporalResolveRecord(
	vkTemporalResolveOwner_t *owner, ralCommandBuffer_t *commandBuffer,
	const vkTemporalResolveAuthorityReceipt_t *authority,
	const vkTemporalResolveProductView_t *products,
	const vkHdrPostprocessSource_t *current,
	const vkTemporalResolvedHdrReceipt_t *target,
	uint64_t contentSerial, vkTemporalResolveTicket_t *outTicket );
qboolean VK_TemporalResolveResolveSubmit(
	const vkTemporalResolveTicket_t *ticket,
	const vkTemporalResolveOwnerReceipt_t *ownerReceipt,
	const vkTemporalMainActivationReceipt_t *acceptedActivation,
	const temporalHistoryCommittedReceipt_t *committedWrite,
	const vkTemporalResolvedHdrReceipt_t *targetReceipt,
	qboolean submitted,
	vkTemporalResolveTicket_t *outSubmittedTicket );
// Padding-independent identity comparison for immutable recorded/submitted
// tickets. Diagnostics must never use whole-struct byte comparison as an
// authority join.
qboolean VK_TemporalResolveTicketEqualExact(
	const vkTemporalResolveTicket_t *a,
	const vkTemporalResolveTicket_t *b );
qboolean VK_TemporalResolveTicketValidateRecordedExact(
	const vkTemporalResolveTicket_t *ticket );
qboolean VK_TemporalResolveBindStoreExpected(
	const vkTemporalResolveTicket_t *recorded,
	uint32_t storeOwnerAllocationGeneration,
	vkTemporalResolveTicket_t *outBound );
qboolean VK_TemporalResolveHasLive( const vkTemporalResolveOwner_t *owner );
qboolean VK_TemporalResolveReleaseAfterIdle(
	vkTemporalResolveOwner_t *owner, qboolean idleProven );

#endif
