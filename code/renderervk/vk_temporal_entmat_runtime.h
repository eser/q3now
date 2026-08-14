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

void VK_TemporalEntMatRuntimeInit( vkTemporalEntMatRuntime_t *runtime );

// Fence-authorized materialization/rebind only. This prerequisite deliberately
// does not begin recording, append payloads or expose the composite bind group.
qboolean VK_TemporalEntMatRuntimeEnsureAfterFence(
	vkTemporalEntMatRuntime_t *runtime, ralBackend_t *backend,
	uint32_t frameCount, uint32_t frameIndex, void *nativeBuffer,
	size_t size, uint32_t allocationGeneration, uint32_t capacity );

qboolean VK_TemporalEntMatRuntimeHasLive(
	const vkTemporalEntMatRuntime_t *runtime );

// Caller proves global device/queue idle and drains deferred RAL destruction
// after success. Payload consumers detach before adopted wrappers are freed.
qboolean VK_TemporalEntMatRuntimeReleaseAfterIdle(
	vkTemporalEntMatRuntime_t *runtime, qboolean idleProven );

#endif
