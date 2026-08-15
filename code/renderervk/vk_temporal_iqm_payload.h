// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#ifndef WIRED_VK_TEMPORAL_IQM_PAYLOAD_H
#define WIRED_VK_TEMPORAL_IQM_PAYLOAD_H

#include "tr_temporal_iqm_motion.h"
#include "../renderer/ral/ral_resource.h"

#define VK_TEMPORAL_IQM_PAYLOAD_MAX_FRAMES 4u
#define VK_TEMPORAL_IQM_PAYLOAD_MAX_PROTECTED 32u

typedef struct {
	ralBackend_t *backend;
	uint64_t maxStorageBufferRange;
	uint32_t frameCount;
	uint32_t protectedCount;
	const void *protectedIdentities[VK_TEMPORAL_IQM_PAYLOAD_MAX_PROTECTED];
} vkTemporalIqmPayloadKey_t;

typedef struct {
	ralBuffer_t *buffer;
	void *mapped;
	ralBindGroup_t *group;
	uint32_t allocationGeneration;
	uint32_t prepareGeneration;
} vkTemporalIqmPayloadSlot_t;

typedef struct {
	vkTemporalIqmPayloadKey_t key;
	ralBindGroupLayout_t *layout;
	vkTemporalIqmPayloadSlot_t slots[VK_TEMPORAL_IQM_PAYLOAD_MAX_FRAMES];
	uint32_t ownerAllocationGeneration;
	uint32_t nextOwnerAllocationGeneration;
	uint32_t nextSlotAllocationGeneration[VK_TEMPORAL_IQM_PAYLOAD_MAX_FRAMES];
	uint32_t nextPrepareGeneration[VK_TEMPORAL_IQM_PAYLOAD_MAX_FRAMES];
	uint32_t preparedSlot;
	uint32_t readySlotMask;
	uint32_t layoutLeaseCount;
	qboolean initialized;
	qboolean ready;
} vkTemporalIqmPayloadOwner_t;

typedef struct {
	ralBackend_t *backend;
	ralBindGroupLayout_t *layout;
	ralBuffer_t *buffer;
	void *mappedIdentity;
	ralBindGroup_t *group;
	uint64_t descriptorRange;
	uint32_t recordCapacity;
	uint32_t recordBytes;
	uint32_t ownerAllocationGeneration;
	uint32_t slotAllocationGeneration;
	uint32_t prepareGeneration;
	uint32_t commandSlot;
	uint32_t frameCount;
	qboolean ready;
} vkTemporalIqmPayloadReceipt_t;

void VK_TemporalIqmPayloadInit( vkTemporalIqmPayloadOwner_t *owner );
qboolean VK_TemporalIqmPayloadNeedsIdle(
	const vkTemporalIqmPayloadOwner_t *owner,
	const vkTemporalIqmPayloadKey_t *key );
qboolean VK_TemporalIqmPayloadPrepareAfterFence(
	vkTemporalIqmPayloadOwner_t *owner,
	const vkTemporalIqmPayloadKey_t *key, uint32_t commandSlot,
	qboolean slotFenceCompleted, qboolean idleProven );
qboolean VK_TemporalIqmPayloadGetReceipt(
	const vkTemporalIqmPayloadOwner_t *owner, uint32_t commandSlot,
	vkTemporalIqmPayloadReceipt_t *outReceipt );
qboolean VK_TemporalIqmPayloadReceiptExact(
	const vkTemporalIqmPayloadReceipt_t *a,
	const vkTemporalIqmPayloadReceipt_t *b );
qboolean VK_TemporalIqmPayloadAcquireLayoutLease(
	vkTemporalIqmPayloadOwner_t *owner,
	ralBindGroupLayout_t **outLayout, uint32_t *outOwnerGeneration );
qboolean VK_TemporalIqmPayloadReleaseLayoutLease(
	vkTemporalIqmPayloadOwner_t *owner,
	ralBindGroupLayout_t *layout, uint32_t ownerGeneration );
qboolean VK_TemporalIqmPayloadHasLive(
	const vkTemporalIqmPayloadOwner_t *owner );
qboolean VK_TemporalIqmPayloadReleaseAfterIdle(
	vkTemporalIqmPayloadOwner_t *owner, qboolean idleProven );

#endif
