// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef WIRED_VK_RAL_FRAME_UNIFORM_H
#define WIRED_VK_RAL_FRAME_UNIFORM_H

#include "../renderer/ral/ral.h"

#define VK_RAL_FRAME_UNIFORM_SLOT_COUNT 2u
#define VK_RAL_FRAME_UNIFORM_MAX_BYTES 4096u
#define VK_RAL_FRAME_UNIFORM_RECEIPT_SCHEMA 1u

typedef struct {
	uint64_t byteSize;
	uint64_t partialOffset;
	uint64_t partialSize;
	qboolean partialRequired;
	const char *debugName;
} vkRalFrameUniformConfig_t;

typedef struct {
	uint32_t schemaVersion;
	ralBackend_t *backend;
	ralBuffer_t *buffers[VK_RAL_FRAME_UNIFORM_SLOT_COUNT];
	ralAllocationReceipt_t allocations[VK_RAL_FRAME_UNIFORM_SLOT_COUNT];
	uint64_t ownerGeneration;
	uint64_t byteSize;
	uint64_t partialOffset;
	uint64_t partialSize;
	qboolean partialRequired;
	qboolean ready;
} vkRalFrameUniformResourcesReceipt_t;

typedef struct {
	uint32_t schemaVersion;
	ralBackend_t *backend;
	ralBuffer_t *buffer;
	ralAllocationReceipt_t allocation;
	ralBufferUploadReceipt_t partialWrite;
	ralBufferUploadReceipt_t finalWrite;
	uint64_t ownerGeneration;
	uint64_t slotGeneration;
	uint64_t partialHash;
	uint64_t contentHash;
	uint64_t byteSize;
	uint64_t partialOffset;
	uint64_t partialSize;
	uint32_t commandSlot;
	qboolean partialRequired;
	qboolean fenceRequired;
	qboolean fenceCompleted;
	qboolean ready;
} vkRalFrameUniformReceipt_t;

typedef struct {
	qboolean initialized;
	ralBackend_t *backend;
	ralBuffer_t *buffers[VK_RAL_FRAME_UNIFORM_SLOT_COUNT];
	ralAllocationReceipt_t allocations[VK_RAL_FRAME_UNIFORM_SLOT_COUNT];
	unsigned char *shadows[VK_RAL_FRAME_UNIFORM_SLOT_COUNT];
	ralBufferUploadReceipt_t partialWrites[VK_RAL_FRAME_UNIFORM_SLOT_COUNT];
	ralBufferUploadReceipt_t finalWrites[VK_RAL_FRAME_UNIFORM_SLOT_COUNT];
	uint64_t slotGenerations[VK_RAL_FRAME_UNIFORM_SLOT_COUNT];
	uint64_t partialHashes[VK_RAL_FRAME_UNIFORM_SLOT_COUNT];
	uint64_t contentHashes[VK_RAL_FRAME_UNIFORM_SLOT_COUNT];
	uint64_t ownerGeneration;
	uint64_t byteSize;
	uint64_t partialOffset;
	uint64_t partialSize;
	uint32_t preparedSlot;
	qboolean partialRequired;
	qboolean fenceRequired;
	qboolean fenceCompleted;
	qboolean prepared;
	qboolean partialReady;
	qboolean finalReady;
	qboolean ready;
} vkRalFrameUniformOwner_t;

void VK_RalFrameUniformInit( vkRalFrameUniformOwner_t *owner );
qboolean VK_RalFrameUniformEnsure( vkRalFrameUniformOwner_t *owner,
	ralBackend_t *backend, const vkRalFrameUniformConfig_t *config );
qboolean VK_RalFrameUniformGetResources(
	const vkRalFrameUniformOwner_t *owner,
	vkRalFrameUniformResourcesReceipt_t *outReceipt );
qboolean VK_RalFrameUniformResourcesReceiptExact(
	const vkRalFrameUniformResourcesReceipt_t *a,
	const vkRalFrameUniformResourcesReceipt_t *b );
qboolean VK_RalFrameUniformBeginSlot( vkRalFrameUniformOwner_t *owner,
	uint32_t commandSlot, qboolean fenceRequired,
	qboolean fenceCompleted );
qboolean VK_RalFrameUniformGetShadow( vkRalFrameUniformOwner_t *owner,
	uint32_t commandSlot, void **outShadow );
qboolean VK_RalFrameUniformPublishPartial(
	vkRalFrameUniformOwner_t *owner, uint32_t commandSlot );
qboolean VK_RalFrameUniformPublishFinal(
	vkRalFrameUniformOwner_t *owner, uint32_t commandSlot,
	vkRalFrameUniformReceipt_t *outReceipt );
qboolean VK_RalFrameUniformGetReceipt(
	const vkRalFrameUniformOwner_t *owner, uint32_t commandSlot,
	vkRalFrameUniformReceipt_t *outReceipt );
qboolean VK_RalFrameUniformReceiptExact(
	const vkRalFrameUniformReceipt_t *a,
	const vkRalFrameUniformReceipt_t *b );
qboolean VK_RalFrameUniformHasLive(
	const vkRalFrameUniformOwner_t *owner );
void VK_RalFrameUniformRelease( vkRalFrameUniformOwner_t *owner );

#endif
