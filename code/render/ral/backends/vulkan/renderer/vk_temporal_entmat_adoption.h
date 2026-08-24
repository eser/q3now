// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#ifndef WIRED_VK_TEMPORAL_ENTMAT_ADOPTION_H
#define WIRED_VK_TEMPORAL_ENTMAT_ADOPTION_H

#include "../../../core/ral_backend.h"
#include "../../../core/ral_resource.h"

#include <stddef.h>

#define VK_TEMPORAL_ENTMAT_MAX_SLOTS 4u

typedef struct {
	void *nativeBuffer;
	size_t size;
	ralBuffer_t *adopted;
	uint32_t allocationGeneration;
	qboolean resetAfterFence;
	qboolean begun;
	qboolean consumerDetached;
	qboolean ready;
} vkTemporalEntMatAdoptionSlot_t;

typedef struct {
	ralBackend_t *backend;
	vkTemporalEntMatAdoptionSlot_t slots[VK_TEMPORAL_ENTMAT_MAX_SLOTS];
	uint32_t frameCount;
	qboolean configured;
	qboolean releasing;
} vkTemporalEntMatAdoptionOwner_t;

// Callbacks are one transaction boundary. adopt/destroy own only the non-owning
// wrapper. consumerRebind must be idempotent for an exact repeated key, retain
// its old binding on false and publish the candidate binding before returning
// true. consumerDetach must leave its binding unchanged on false; the owner
// records successful detaches and never calls them twice during release retry.
typedef struct {
	ralBuffer_t *(*adopt)( void *userData, ralBackend_t *backend,
		void *nativeBuffer, size_t size, const char *debugName );
	void (*destroy)( void *userData, ralBuffer_t *adopted );
	qboolean (*consumerRebind)( void *userData, uint32_t frameIndex,
		const ralBuffer_t *adopted, uint32_t allocationGeneration );
	qboolean (*consumerBeginFrame)( void *userData, uint32_t frameIndex,
		const ralBuffer_t *adopted, uint32_t allocationGeneration );
	qboolean (*consumerDetach)( void *userData, uint32_t frameIndex,
		const ralBuffer_t *adopted, uint32_t allocationGeneration );
	void *userData;
} vkTemporalEntMatAdoptionOps_t;

void VK_TemporalEntMatAdoptionInit( vkTemporalEntMatAdoptionOwner_t *owner );

// Publishes the caller's proof that this command-frame slot's submission fence
// completed. It is the only authority that permits Ensure or BeginFrame.
qboolean VK_TemporalEntMatAdoptionResetAfterFence(
	vkTemporalEntMatAdoptionOwner_t *owner, uint32_t frameCount,
	uint32_t frameIndex );

// Candidate-adopts first, then asks the consumer to publish a binding to that
// wrapper. Only after both succeed is the owner updated and the old wrapper
// destroyed. The exact native pointer/size/generation tuple is stable; a new
// allocation must advance generation even if the native pointer is reused.
// UINT32_MAX is reserved as the exhausted/disabled generation and never
// materializes, including on the first call. An exact repeated key performs no
// adoption but deliberately re-invokes the idempotent consumerRebind callback.
qboolean VK_TemporalEntMatAdoptionEnsure(
	vkTemporalEntMatAdoptionOwner_t *owner, ralBackend_t *backend,
	uint32_t frameIndex, void *nativeBuffer, size_t size,
	uint32_t allocationGeneration,
	const vkTemporalEntMatAdoptionOps_t *ops );

// Lets the bound consumer enter recording, then consumes the completed-fence
// grant. A failed consumer begin leaves the owner and grant unchanged.
qboolean VK_TemporalEntMatAdoptionBeginFrame(
	vkTemporalEntMatAdoptionOwner_t *owner, uint32_t frameIndex,
	const vkTemporalEntMatAdoptionOps_t *ops );

const ralBuffer_t *VK_TemporalEntMatAdoptionGet(
	const vkTemporalEntMatAdoptionOwner_t *owner, uint32_t frameIndex,
	uint32_t *outAllocationGeneration );

// `idleProven` is explicit global-idle authority. Every consumer is detached
// before any wrapper is destroyed; wrapper teardown then runs in reverse slot
// order. Completed detaches are recorded and skipped if a later detach fails,
// so retry never depends on consumer idempotence. Ensure/Reset/Begin are blocked
// while such a release is pending. No raw VkBuffer/VkDeviceMemory and no
// pipeline belongs to this owner.
qboolean VK_TemporalEntMatAdoptionReleaseAfterIdle(
	vkTemporalEntMatAdoptionOwner_t *owner, qboolean idleProven,
	const vkTemporalEntMatAdoptionOps_t *ops );

#endif
