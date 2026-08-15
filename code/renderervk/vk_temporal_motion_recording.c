// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "vk_temporal_motion_recording.h"

#include <limits.h>
#include <string.h>

static qboolean AuthorityValid(
		const vkTemporalMotionRecordingAuthority_t *authority ) {
	return authority && authority->token && authority->frameId
		&& authority->worldIndex >= 0 && authority->width && authority->height
		&& authority->topologyEpoch && authority->planGeneration
		&& authority->geometryBufferSize && authority->uniformItemSize
		&& authority->requiredCapacity > 0
		&& authority->requiredCapacity <= TEMPORAL_MOTION_PAYLOAD_MAX_SLOTS
		&& authority->rawEntMatCapacity >= authority->requiredCapacity
		&& authority->geometryBufferSize / authority->uniformItemSize
			== authority->requiredCapacity
		&& authority->rawEntMatAllocationGeneration
		&& authority->rawEntMatAllocationGeneration != UINT32_MAX
		&& authority->payloadAllocationGeneration
		&& authority->payloadLayoutGeneration
		&& authority->targetAllocationGeneration
		&& authority->pipelineLayoutAllocationGeneration
		&& authority->materializationGeneration
		&& authority->pipelineTableGeneration;
}

qboolean VK_TemporalEntMatPlanUniformSlot( uint32_t uniformOffset,
		uint32_t lastUniformOffset, uint32_t slotCursor, uint32_t capacity,
		vkTemporalEntMatSlotPlan_t *outPlan ) {
	vkTemporalEntMatSlotPlan_t plan;
	if ( !outPlan || uniformOffset == UINT32_MAX || !capacity ) return qfalse;
	if ( uniformOffset == lastUniformOffset ) {
		if ( slotCursor == 0 || slotCursor > capacity ) return qfalse;
		plan.slot = slotCursor - 1u;
		plan.fresh = qfalse;
	} else {
		if ( slotCursor >= capacity ) return qfalse;
		plan.slot = slotCursor;
		plan.fresh = qtrue;
	}
	*outPlan = plan;
	return qtrue;
}

void VK_TemporalMotionRecordingInit(
		vkTemporalMotionRecordingOwner_t *owner ) {
	if ( owner ) memset( owner, 0, sizeof( *owner ) );
}

qboolean VK_TemporalMotionRecordingBegin(
		vkTemporalMotionRecordingOwner_t *owner,
		vkTemporalEntMatRuntime_t *runtime,
		const vkTemporalMotionRecordingAuthority_t *authority,
		const vkTemporalMotionMaterializationReceipt_t *materialization,
		const vkTemporalGenericPipelineTable_t *pipelineTable ) {
	vkTemporalEntMatRuntimeFrameReceipt_t runtimeReceipt;
	vkTemporalEntMatRuntimeFrameReceipt_t preflightReceipt;
	if ( !owner ) return qfalse;
	memset( owner, 0, sizeof( *owner ) );
	if ( !runtime || !AuthorityValid( authority ) || !materialization
			|| !materialization->ready || !pipelineTable || !pipelineTable->slots
			|| !pipelineTable->guardLease || !pipelineTable->layoutOwner
			|| authority->materializationGeneration !=
				materialization->allocationGeneration
			|| authority->pipelineTableGeneration !=
				pipelineTable->allocationGeneration
			|| pipelineTable->layoutAllocationGeneration !=
				authority->pipelineLayoutAllocationGeneration
			|| authority->planGeneration != materialization->planGeneration
			|| authority->worldIndex != materialization->worldIndex
			|| authority->width != materialization->width
			|| authority->height != materialization->height
			|| authority->topologyEpoch != materialization->topologyEpoch
			|| authority->payloadLayoutGeneration !=
				materialization->payloadLayoutGeneration
			|| authority->targetAllocationGeneration !=
				materialization->targetAllocationGeneration
			|| authority->pipelineLayoutAllocationGeneration !=
				materialization->pipelineLayoutAllocationGeneration
			|| !VK_TemporalEntMatRuntimePeekFrameReceipt( runtime,
				authority->frameIndex, &preflightReceipt )
			|| preflightReceipt.entityAllocationGeneration !=
				authority->rawEntMatAllocationGeneration
			|| preflightReceipt.payloadAllocationGeneration !=
				authority->payloadAllocationGeneration
			|| preflightReceipt.payloadLayoutGeneration !=
				authority->payloadLayoutGeneration
			|| preflightReceipt.capacity != authority->requiredCapacity
			|| !VK_TemporalEntMatRuntimeBeginFrame( runtime,
				authority->frameIndex, &runtimeReceipt )
			|| runtimeReceipt.frameIndex != preflightReceipt.frameIndex
			|| runtimeReceipt.entityAllocationGeneration !=
				preflightReceipt.entityAllocationGeneration
			|| runtimeReceipt.payloadAllocationGeneration !=
				preflightReceipt.payloadAllocationGeneration
			|| runtimeReceipt.payloadLayoutGeneration !=
				preflightReceipt.payloadLayoutGeneration
			|| runtimeReceipt.capacity != preflightReceipt.capacity ) return qfalse;
	owner->authority = *authority;
	owner->runtimeReceipt = runtimeReceipt;
	owner->active = qtrue;
	return qtrue;
}

qboolean VK_TemporalMotionRecordingBeginPrimaryCommand(
		vkTemporalMotionRecordingOwner_t *owner ) {
	if ( !owner || !owner->active || owner->poisoned
			|| owner->primaryCommandActive ) return qfalse;
	if ( owner->primaryCommandCount ) {
		owner->poisoned = qtrue;
		return qfalse;
	}
	owner->primaryCommandActive = qtrue;
	return qtrue;
}

void VK_TemporalMotionRecordingEndPrimaryCommand(
		vkTemporalMotionRecordingOwner_t *owner ) {
	if ( !owner || !owner->active || !owner->primaryCommandActive ) return;
	if ( owner->pending ) owner->poisoned = qtrue;
	owner->pending = qfalse;
	owner->pendingHasMatrices = qfalse;
	owner->primaryCommandActive = qfalse;
	owner->primaryCommandCount++;
}

qboolean VK_TemporalMotionRecordingIsActive(
		const vkTemporalMotionRecordingOwner_t *owner ) {
	return owner && owner->active ? qtrue : qfalse;
}

qboolean VK_TemporalMotionRecordingPrepareDraw(
		vkTemporalMotionRecordingOwner_t *owner,
		const vkTemporalGenericPipelineTable_t *pipelineTable,
		uint32_t pipelineSlot, const ralPipeline_t *ordinaryPipeline,
		const temporalMotionDrawFacts_t *facts,
		const temporalCameraPoseReceipt_t *camera,
		const temporalEntityPoseReceipt_t *entity ) {
	temporalMotionOutcome_t outcome;
	temporalMotionMatrices_t matrices;
	vkTemporalGenericPipelineReceipt_t pipelineReceipt;
	ralPipeline_t *pipelines[3];
	if ( !owner || !owner->active || !owner->primaryCommandActive
			|| owner->poisoned || owner->pending
			|| !facts ) return qfalse;
	owner->prepared++;
	outcome = R_TemporalMotionClassify( facts );
	if ( outcome == TEMPORAL_MOTION_PRESERVE ) {
		owner->preserved++;
		owner->pendingOutcome = outcome;
		owner->pendingPipelineSlot = pipelineSlot;
		memset( &owner->pendingPipelineReceipt, 0,
			sizeof( owner->pendingPipelineReceipt ) );
		owner->pending = qtrue;
		return qtrue;
	}
	if ( outcome == TEMPORAL_MOTION_DEFER_ATEST ) {
		owner->deferred++;
		owner->preserved++;
		owner->pendingOutcome = TEMPORAL_MOTION_PRESERVE;
		owner->pendingPipelineSlot = pipelineSlot;
		memset( &owner->pendingPipelineReceipt, 0,
			sizeof( owner->pendingPipelineReceipt ) );
		owner->pending = qtrue;
		return qtrue;
	}
	if ( !pipelineTable || pipelineTable->allocationGeneration !=
			owner->authority.pipelineTableGeneration
			|| !VK_TemporalGenericPipelineTablePreflightSlot( pipelineTable,
				pipelineSlot, ordinaryPipeline, &pipelineReceipt, pipelines )
			|| pipelineReceipt.tableGeneration !=
				owner->authority.pipelineTableGeneration
			|| pipelineReceipt.slot != pipelineSlot
			|| !pipelineReceipt.entryGeneration
			|| !pipelineReceipt.recipeOwnerEpoch
			|| !pipelineReceipt.recipeEntryGeneration
			|| !pipelineReceipt.factoryAllocationGeneration
			|| !pipelineReceipt.catalogId
			|| !pipelines[0] || !pipelines[1] || !pipelines[2]
			|| pipelines[0] == pipelines[1] || pipelines[0] == pipelines[2]
			|| pipelines[1] == pipelines[2] ) {
		owner->rejected++;
		owner->poisoned = qtrue;
		return qfalse;
	}
	if ( outcome == TEMPORAL_MOTION_WRITE_VALID
			&& !R_TemporalMotionBuildMatrices( facts->geometry, camera,
				entity, &matrices ) ) {
		outcome = TEMPORAL_MOTION_INVALIDATE_OPAQUE;
	}
	owner->pendingOutcome = outcome;
	owner->pendingPipelineSlot = pipelineSlot;
	owner->pendingPipelineReceipt = pipelineReceipt;
	owner->pendingHasMatrices = outcome == TEMPORAL_MOTION_WRITE_VALID
		? qtrue : qfalse;
	if ( owner->pendingHasMatrices ) owner->pendingMatrices = matrices;
	else memset( &owner->pendingMatrices, 0, sizeof( owner->pendingMatrices ) );
	owner->pending = qtrue;
	if ( outcome == TEMPORAL_MOTION_INVALIDATE_OPAQUE ) owner->invalidated++;
	return qtrue;
}

qboolean VK_TemporalMotionRecordingPeekPendingDraw(
		const vkTemporalMotionRecordingOwner_t *owner,
		temporalMotionOutcome_t *outOutcome, uint32_t *outPipelineSlot ) {
	if ( !owner || !owner->active || owner->poisoned || !owner->pending
			|| !outOutcome || !outPipelineSlot ) return qfalse;
	*outOutcome = owner->pendingOutcome;
	*outPipelineSlot = owner->pendingPipelineSlot;
	return qtrue;
}

qboolean VK_TemporalMotionRecordingConsumeAt(
		vkTemporalMotionRecordingOwner_t *owner,
		vkTemporalEntMatRuntime_t *runtime, uint32_t absoluteEntMatSlot ) {
	uint32_t writtenSlot = UINT32_MAX;
	qboolean result;
	if ( !owner || !owner->active ) return qfalse;
	if ( absoluteEntMatSlot >= owner->authority.requiredCapacity ) {
		owner->pending = qfalse;
		owner->pendingHasMatrices = qfalse;
		owner->poisoned = qtrue;
		owner->rejected++;
		return qfalse;
	}
	if ( !owner->pending || owner->poisoned ) {
		owner->pending = qfalse;
		owner->pendingHasMatrices = qfalse;
		owner->poisoned = qtrue;
		owner->rejected++;
		return qfalse;
	}
	if ( owner->pendingOutcome == TEMPORAL_MOTION_PRESERVE ) {
		if ( !VK_TemporalMotionDrawSequenceAppend( &owner->drawSequence,
				absoluteEntMatSlot, owner->pendingPipelineSlot,
				TEMPORAL_MOTION_PRESERVE, NULL ) ) {
			owner->pending = qfalse;
			owner->poisoned = qtrue;
			owner->rejected++;
			return qfalse;
		}
		owner->pending = qfalse;
		return qtrue;
	}
	if ( owner->pendingOutcome != TEMPORAL_MOTION_WRITE_VALID
			&& owner->pendingOutcome != TEMPORAL_MOTION_INVALIDATE_OPAQUE ) {
		owner->pending = qfalse;
		owner->poisoned = qtrue;
		owner->rejected++;
		return qfalse;
	}
	result = VK_TemporalEntMatRuntimeAppendAt( runtime,
		&owner->runtimeReceipt, absoluteEntMatSlot, owner->pendingOutcome,
		owner->pendingHasMatrices ? &owner->pendingMatrices : NULL,
		&writtenSlot );
	owner->pending = qfalse;
	owner->pendingHasMatrices = qfalse;
	if ( !result || writtenSlot != absoluteEntMatSlot ) {
		owner->poisoned = qtrue;
		owner->rejected++;
		return qfalse;
	}
	if ( !VK_TemporalMotionDrawSequenceAppend( &owner->drawSequence,
			absoluteEntMatSlot, owner->pendingPipelineSlot,
			owner->pendingOutcome, &owner->pendingPipelineReceipt ) ) {
		owner->poisoned = qtrue;
		owner->rejected++;
		return qfalse;
	}
	owner->appended++;
	return qtrue;
}

void VK_TemporalMotionRecordingPoison(
		vkTemporalMotionRecordingOwner_t *owner ) {
	if ( !owner || !owner->active ) return;
	owner->pending = qfalse;
	owner->pendingHasMatrices = qfalse;
	owner->poisoned = qtrue;
}

void VK_TemporalMotionRecordingFinish(
		vkTemporalMotionRecordingOwner_t *owner ) {
	if ( !owner ) return;
	if ( owner->pending || owner->primaryCommandActive ) owner->poisoned = qtrue;
	owner->pending = qfalse;
	owner->pendingHasMatrices = qfalse;
	owner->primaryCommandActive = qfalse;
	memset( &owner->receipt, 0, sizeof( owner->receipt ) );
	if ( !owner->poisoned && owner->primaryCommandCount == 1 ) {
		owner->receipt.authority = owner->authority;
		owner->receipt.runtimeReceipt = owner->runtimeReceipt;
		owner->receipt.prepared = owner->prepared;
		owner->receipt.appended = owner->appended;
		owner->receipt.preserved = owner->preserved;
		owner->receipt.invalidated = owner->invalidated;
		owner->receipt.deferred = owner->deferred;
		owner->receipt.rejected = owner->rejected;
		owner->receipt.drawSequence = owner->drawSequence;
		owner->receipt.ready = qtrue;
	}
	owner->active = qfalse;
}

qboolean VK_TemporalMotionRecordingGetReceipt(
		const vkTemporalMotionRecordingOwner_t *owner,
		vkTemporalMotionRecordingReceipt_t *outReceipt ) {
	if ( !owner || !outReceipt || !owner->receipt.ready ) return qfalse;
	*outReceipt = owner->receipt;
	return qtrue;
}
