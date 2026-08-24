// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#ifndef WIRED_VK_TEMPORAL_MOTION_RECORDING_H
#define WIRED_VK_TEMPORAL_MOTION_RECORDING_H

#include "vk_temporal_entmat_runtime.h"
#include "vk_temporal_generic_pipeline_table.h"
#include "vk_temporal_motion_materialization.h"
#include "vk_temporal_motion_sequence.h"

typedef struct {
	uint64_t token;
	uint64_t frameId;
	int32_t worldIndex;
	uint32_t width;
	uint32_t height;
	uint32_t topologyEpoch;
	uint32_t planGeneration;
	uint64_t geometryBufferSize;
	uint32_t uniformItemSize;
	uint32_t requiredCapacity;
	uint32_t rawEntMatCapacity;
	uint32_t rawEntMatAllocationGeneration;
	uint32_t payloadAllocationGeneration;
	uint32_t payloadLayoutGeneration;
	uint32_t targetAllocationGeneration;
	uint32_t pipelineLayoutAllocationGeneration;
	uint32_t materializationGeneration;
	uint32_t pipelineTableGeneration;
	uint32_t frameIndex;
} vkTemporalMotionRecordingAuthority_t;

typedef struct {
	vkTemporalMotionRecordingAuthority_t authority;
	vkTemporalEntMatRuntimeFrameReceipt_t runtimeReceipt;
	uint32_t prepared;
	uint32_t appended;
	uint32_t preserved;
	uint32_t invalidated;
	uint32_t deferred;
	uint32_t rejected;
	vkTemporalMotionDrawSequence_t drawSequence;
	qboolean ready;
} vkTemporalMotionRecordingReceipt_t;

typedef struct {
	uint32_t slot;
	qboolean fresh;
} vkTemporalEntMatSlotPlan_t;

qboolean VK_TemporalEntMatPlanUniformSlot( uint32_t uniformOffset,
	uint32_t lastUniformOffset, uint32_t slotCursor, uint32_t capacity,
	vkTemporalEntMatSlotPlan_t *outPlan );

typedef struct {
	vkTemporalMotionRecordingAuthority_t authority;
	vkTemporalEntMatRuntimeFrameReceipt_t runtimeReceipt;
	temporalMotionOutcome_t pendingOutcome;
	temporalMotionMatrices_t pendingMatrices;
	vkTemporalGenericPipelineReceipt_t pendingPipelineReceipt;
	vkTemporalMotionDrawSequence_t drawSequence;
	uint32_t pendingPipelineSlot;
	uint32_t prepared;
	uint32_t appended;
	uint32_t preserved;
	uint32_t invalidated;
	uint32_t deferred;
	uint32_t rejected;
	qboolean pendingHasMatrices;
	qboolean active;
	qboolean pending;
	qboolean poisoned;
	qboolean primaryCommandActive;
	uint32_t primaryCommandCount;
	vkTemporalMotionRecordingReceipt_t receipt;
} vkTemporalMotionRecordingOwner_t;

void VK_TemporalMotionRecordingInit(
	vkTemporalMotionRecordingOwner_t *owner );

qboolean VK_TemporalMotionRecordingBegin(
	vkTemporalMotionRecordingOwner_t *owner,
	vkTemporalEntMatRuntime_t *runtime,
	const vkTemporalMotionRecordingAuthority_t *authority,
	const vkTemporalMotionMaterializationReceipt_t *materialization,
	const vkTemporalGenericPipelineTable_t *pipelineTable );

qboolean VK_TemporalMotionRecordingIsActive(
	const vkTemporalMotionRecordingOwner_t *owner );

qboolean VK_TemporalMotionRecordingBeginPrimaryCommand(
	vkTemporalMotionRecordingOwner_t *owner );
void VK_TemporalMotionRecordingEndPrimaryCommand(
	vkTemporalMotionRecordingOwner_t *owner );

// Stages exactly one ordinary draw. PRESERVE and DEFER_ATEST consume no payload
// slot (both are explicit non-admitted gaps). WRITE_VALID is downgraded
// conservatively to INVALIDATE_OPAQUE when prior pose math is unavailable.
qboolean VK_TemporalMotionRecordingPrepareDraw(
	vkTemporalMotionRecordingOwner_t *owner,
	const vkTemporalGenericPipelineTable_t *pipelineTable,
	uint32_t pipelineSlot, const ralPipeline_t *ordinaryPipeline,
	const temporalMotionDrawFacts_t *facts,
	const temporalCameraPoseReceipt_t *camera,
	const temporalEntityPoseReceipt_t *entity );

// Read-only effective draw decision for the currently staged logical draw.
// This carries no pipeline pointers; A2c4 must re-run the authoritative table
// preflight when it authors a segment.
qboolean VK_TemporalMotionRecordingPeekPendingDraw(
	const vkTemporalMotionRecordingOwner_t *owner,
	temporalMotionOutcome_t *outOutcome, uint32_t *outPipelineSlot );

// Consumes the staged draw at the reserved, prevalidated absolute slot
// immediately before the infallible mapped ordinary entMat publication.
// Ordinary rendering continues on false.
qboolean VK_TemporalMotionRecordingConsumeAt(
	vkTemporalMotionRecordingOwner_t *owner,
	vkTemporalEntMatRuntime_t *runtime, uint32_t absoluteEntMatSlot );

void VK_TemporalMotionRecordingPoison(
	vkTemporalMotionRecordingOwner_t *owner );
void VK_TemporalMotionRecordingFinish(
	vkTemporalMotionRecordingOwner_t *owner );
qboolean VK_TemporalMotionRecordingGetReceipt(
	const vkTemporalMotionRecordingOwner_t *owner,
	vkTemporalMotionRecordingReceipt_t *outReceipt );

#endif
