// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#ifndef WIRED_VK_TEMPORAL_ENTMAT_RUNTIME_H
#define WIRED_VK_TEMPORAL_ENTMAT_RUNTIME_H

#include "vk_temporal_entmat_adoption.h"
#include "tr_temporal_motion_payload.h"

typedef struct {
	vkTemporalEntMatAdoptionOwner_t adoption;
	temporalMotionPayloadOwner_t payload;
	ralBackend_t *requestedBackend;
	uint32_t requestedCapacity;
	qboolean initialized;
} vkTemporalEntMatRuntime_t;

typedef struct {
	uint32_t frameIndex;
	uint32_t entityAllocationGeneration;
	uint32_t payloadAllocationGeneration;
	uint32_t payloadLayoutGeneration;
	uint32_t capacity;
} vkTemporalEntMatRuntimeFrameReceipt_t;

typedef struct {
	vkTemporalEntMatRuntimeFrameReceipt_t receipt;
	ralBindGroup_t *compositeGroup;
} vkTemporalEntMatRuntimeFrameBinding_t;

void VK_TemporalEntMatRuntimeInit( vkTemporalEntMatRuntime_t *runtime );

// Fence-authorized materialization/rebind only. This prerequisite deliberately
// does not begin recording, append payloads or expose the composite bind group.
qboolean VK_TemporalEntMatRuntimeEnsureAfterFence(
	vkTemporalEntMatRuntime_t *runtime, ralBackend_t *backend,
	uint32_t frameCount, uint32_t frameIndex, void *nativeBuffer,
	size_t size, uint32_t allocationGeneration, uint32_t capacity );

// Consumes the completed-fence grant for one materialized command-frame slot
// and publishes a generation-bound recording receipt.  No bind group is
// exposed and no command is recorded.
qboolean VK_TemporalEntMatRuntimeBeginFrame(
	vkTemporalEntMatRuntime_t *runtime, uint32_t frameIndex,
	vkTemporalEntMatRuntimeFrameReceipt_t *outReceipt );
qboolean VK_TemporalEntMatRuntimePeekFrameReceipt(
	const vkTemporalEntMatRuntime_t *runtime, uint32_t frameIndex,
	vkTemporalEntMatRuntimeFrameReceipt_t *outReceipt );

// Writes the active-only payload at the exact absolute entity-matrix slot.
// The receipt prevents a stale command batch from addressing a replaced
// adopted buffer or payload allocation. Outputs remain atomic on failure.
qboolean VK_TemporalEntMatRuntimeAppendAt(
	vkTemporalEntMatRuntime_t *runtime,
	const vkTemporalEntMatRuntimeFrameReceipt_t *receipt,
	uint32_t absoluteEntMatSlot, temporalMotionOutcome_t outcome,
	const temporalMotionMatrices_t *matrices, uint32_t *outSlot );

// Exposes set 3 only for the exact begun frame receipt. The bind group is
// borrowed and generation-bound; no descriptor command is authored here.
qboolean VK_TemporalEntMatRuntimeGetFrameBinding(
	const vkTemporalEntMatRuntime_t *runtime,
	const vkTemporalEntMatRuntimeFrameReceipt_t *receipt,
	vkTemporalEntMatRuntimeFrameBinding_t *outBinding );

qboolean VK_TemporalEntMatRuntimeHasLive(
	const vkTemporalEntMatRuntime_t *runtime );

// Caller proves global device/queue idle and drains deferred RAL destruction
// after success. Payload consumers detach before adopted wrappers are freed.
qboolean VK_TemporalEntMatRuntimeReleaseAfterIdle(
	vkTemporalEntMatRuntime_t *runtime, qboolean idleProven );

#endif
