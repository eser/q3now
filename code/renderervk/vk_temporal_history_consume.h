// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#ifndef WIRED_VK_TEMPORAL_HISTORY_CONSUME_H
#define WIRED_VK_TEMPORAL_HISTORY_CONSUME_H

#include "tr_temporal_history.h"
#include "../renderer/ral/ral_command.h"
#include "../renderer/ral/ral_pipeline.h"
#include <stddef.h>

#define VK_TEMPORAL_HISTORY_CONSUME_MAX_FRAMES 4u
#define VK_TEMPORAL_HISTORY_WITNESS_WORDS 20u
#define VK_TEMPORAL_HISTORY_WITNESS_BYTES \
	( VK_TEMPORAL_HISTORY_WITNESS_WORDS * sizeof( uint32_t ) )
#define VK_TEMPORAL_HISTORY_WITNESS_MAGIC 0x48535431u

typedef struct {
	ralBackend_t *backend;
	int32_t worldIndex;
	uint32_t width, height, topologyEpoch, historyAllocationGeneration;
	ralTexture_t *currentColor;
	ralTexture_t *currentDepth;
	ralTexture_t *historyColor[2];
	ralTextureView_t *historyColorView[2];
	ralTexture_t *historyDepth[2];
	ralTextureView_t *historyDepthView[2];
} vkTemporalHistoryConsumeKey_t;

typedef struct {
	uint64_t frameId;
	int32_t worldIndex;
	uint32_t planGeneration;
	uint32_t historyAllocationGeneration;
	uint32_t historyReadIndex;
	uint32_t historyWriteIndex;
	uint32_t width, height, topologyEpoch;
	uint32_t commandSlot;
	uint32_t captureSerial;
	uint32_t bufferAllocationGeneration;
	temporalHistoryCommittedReceipt_t previousCommitted;
	qboolean historyValid;
	qboolean storeAccepted;
	qboolean submitted;
} vkTemporalHistoryConsumeTicket_t;

typedef struct {
	vkTemporalHistoryConsumeTicket_t ticket;
	uint32_t currentColorRG, currentColorBA, currentDepth;
	uint32_t previousColorRG, previousColorBA, previousDepth;
	qboolean fenceComplete;
	qboolean ready;
} vkTemporalHistoryConsumeReceipt_t;

typedef enum {
	VK_TEMPORAL_HISTORY_CONSUME_EMPTY = 0,
	VK_TEMPORAL_HISTORY_CONSUME_READY,
	VK_TEMPORAL_HISTORY_CONSUME_RECORDED,
	VK_TEMPORAL_HISTORY_CONSUME_SUBMITTED
} vkTemporalHistoryConsumeSlotState_t;

typedef struct {
	ralBuffer_t *gpuBuffer;
	ralBuffer_t *readbackBuffer;
	ralBindGroup_t *groups[2];
	uint32_t allocationGeneration;
	vkTemporalHistoryConsumeSlotState_t state;
	vkTemporalHistoryConsumeTicket_t ticket;
	qboolean gpuWritable;
	qboolean hostReadable;
} vkTemporalHistoryConsumeSlot_t;

typedef struct {
	vkTemporalHistoryConsumeKey_t key;
	ralTextureView_t *currentColorView;
	ralTextureView_t *currentDepthView;
	ralSampler_t *sampler;
	ralBindGroupLayout_t *layout;
	ralPipeline_t *pipeline;
	vkTemporalHistoryConsumeSlot_t slots[VK_TEMPORAL_HISTORY_CONSUME_MAX_FRAMES];
	vkTemporalHistoryConsumeReceipt_t latest;
	uint32_t frameCount;
	uint32_t capturesRemaining;
	uint32_t nextCaptureSerial;
	qboolean initialized;
	qboolean ready;
} vkTemporalHistoryConsumeOwner_t;

void VK_TemporalHistoryConsumeInit( vkTemporalHistoryConsumeOwner_t *owner );
qboolean VK_TemporalHistoryConsumeArm(
	vkTemporalHistoryConsumeOwner_t *owner, uint32_t captureCount );
qboolean VK_TemporalHistoryConsumeIsArmed(
	const vkTemporalHistoryConsumeOwner_t *owner );
qboolean VK_TemporalHistoryConsumeEnsureAfterFence(
	vkTemporalHistoryConsumeOwner_t *owner,
	const vkTemporalHistoryConsumeKey_t *key, uint32_t frameCount,
	uint32_t frameIndex, const uint32_t *computeSpirv, size_t computeSpirvSize );
qboolean VK_TemporalHistoryConsumeRecord(
	vkTemporalHistoryConsumeOwner_t *owner, ralCommandBuffer_t *commandBuffer,
	uint32_t frameIndex, const ralTemporalFramePlan_t *plan,
	const temporalHistoryFrameView_t *view, float zNear, float zFar );
qboolean VK_TemporalHistoryConsumeAcceptStore(
	vkTemporalHistoryConsumeOwner_t *owner, uint32_t frameIndex,
	qboolean storeRecorded );
qboolean VK_TemporalHistoryConsumeResolveSubmit(
	vkTemporalHistoryConsumeOwner_t *owner, uint32_t frameIndex,
	qboolean submitted,
	const temporalHistoryCommittedReceipt_t committed[2],
	vkTemporalHistoryConsumeTicket_t *outTicket );
qboolean VK_TemporalHistoryConsumeCompleteAfterFence(
	vkTemporalHistoryConsumeOwner_t *owner, uint32_t frameIndex,
	qboolean fenceProven, vkTemporalHistoryConsumeReceipt_t *outReceipt );
qboolean VK_TemporalHistoryConsumeHasLive(
	const vkTemporalHistoryConsumeOwner_t *owner );
qboolean VK_TemporalHistoryConsumeReleaseAfterIdle(
	vkTemporalHistoryConsumeOwner_t *owner, qboolean idleProven );

#endif
