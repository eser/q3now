// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef WIRED_VK_RAL_SHADOW_STORAGE_H
#define WIRED_VK_RAL_SHADOW_STORAGE_H

#include "../../../core/ral.h"

#define VK_RAL_SHADOW_STORAGE_MAX_BUFFERS 2u
#define VK_RAL_SHADOW_STORAGE_MAX_BYTES ( 4u * 1024u * 1024u )
#define VK_RAL_SHADOW_STORAGE_MAX_ELEMENTS 65536u
#define VK_RAL_SHADOW_STORAGE_RECEIPT_SCHEMA 1u

typedef struct {
	uint32_t bufferCount;
	uint32_t elementSize;
	uint32_t elementCount;
	const char *debugName;
} vkRalShadowStorageConfig_t;

typedef struct {
	uint32_t schemaVersion;
	ralBackend_t *backend;
	ralBuffer_t *buffers[VK_RAL_SHADOW_STORAGE_MAX_BUFFERS];
	ralAllocationReceipt_t allocations[VK_RAL_SHADOW_STORAGE_MAX_BUFFERS];
	uint64_t ownerGeneration;
	uint64_t byteSize;
	uint32_t bufferCount;
	uint32_t elementSize;
	uint32_t elementCount;
	qboolean ready;
} vkRalShadowStorageResourcesReceipt_t;

typedef struct {
	uint32_t schemaVersion;
	ralBackend_t *backend;
	ralBuffer_t *buffer;
	ralAllocationReceipt_t allocation;
	ralBufferUploadReceipt_t firstWrite;
	ralBufferUploadReceipt_t lastWrite;
	uint64_t ownerGeneration;
	uint64_t shadowGeneration;
	uint64_t flushGeneration;
	uint64_t dirtyContentHash;
	uint64_t writeDigest;
	uint64_t byteSize;
	uint32_t bufferIndex;
	uint32_t elementSize;
	uint32_t elementCount;
	uint32_t dirtyElementCount;
	uint32_t writeCount;
	uint32_t firstDirtyElement;
	uint32_t lastDirtyElement;
	qboolean wrote;
	qboolean ready;
} vkRalShadowStorageFlushReceipt_t;

typedef struct {
	qboolean initialized;
	ralBackend_t *backend;
	ralBuffer_t *buffers[VK_RAL_SHADOW_STORAGE_MAX_BUFFERS];
	ralAllocationReceipt_t allocations[VK_RAL_SHADOW_STORAGE_MAX_BUFFERS];
	unsigned char *shadows[VK_RAL_SHADOW_STORAGE_MAX_BUFFERS];
	unsigned char *dirty[VK_RAL_SHADOW_STORAGE_MAX_BUFFERS];
	vkRalShadowStorageFlushReceipt_t flushReceipts[VK_RAL_SHADOW_STORAGE_MAX_BUFFERS];
	uint64_t shadowGenerations[VK_RAL_SHADOW_STORAGE_MAX_BUFFERS];
	uint64_t flushGenerations[VK_RAL_SHADOW_STORAGE_MAX_BUFFERS];
	uint64_t ownerGeneration;
	uint64_t byteSize;
	uint32_t bufferCount;
	uint32_t elementSize;
	uint32_t elementCount;
	qboolean ready;
} vkRalShadowStorageOwner_t;

void VK_RalShadowStorageInit( vkRalShadowStorageOwner_t *owner );
qboolean VK_RalShadowStorageEnsure( vkRalShadowStorageOwner_t *owner,
	ralBackend_t *backend, const vkRalShadowStorageConfig_t *config );
qboolean VK_RalShadowStorageGetResources(
	const vkRalShadowStorageOwner_t *owner,
	vkRalShadowStorageResourcesReceipt_t *outReceipt );
qboolean VK_RalShadowStorageResourcesReceiptExact(
	const vkRalShadowStorageResourcesReceipt_t *a,
	const vkRalShadowStorageResourcesReceipt_t *b );
qboolean VK_RalShadowStorageWriteElement( vkRalShadowStorageOwner_t *owner,
	uint32_t bufferIndex, uint32_t elementIndex, const void *data,
	uint32_t dataSize );
qboolean VK_RalShadowStorageReadElement(
	const vkRalShadowStorageOwner_t *owner, uint32_t bufferIndex,
	uint32_t elementIndex, const void **outData );
qboolean VK_RalShadowStorageHasDirty(
	const vkRalShadowStorageOwner_t *owner, uint32_t bufferIndex );
qboolean VK_RalShadowStorageFlush( vkRalShadowStorageOwner_t *owner,
	uint32_t bufferIndex, vkRalShadowStorageFlushReceipt_t *outReceipt );
qboolean VK_RalShadowStorageGetFlushReceipt(
	const vkRalShadowStorageOwner_t *owner, uint32_t bufferIndex,
	vkRalShadowStorageFlushReceipt_t *outReceipt );
qboolean VK_RalShadowStorageFlushReceiptExact(
	const vkRalShadowStorageFlushReceipt_t *a,
	const vkRalShadowStorageFlushReceipt_t *b );
qboolean VK_RalShadowStorageHasLive(
	const vkRalShadowStorageOwner_t *owner );
void VK_RalShadowStorageRelease( vkRalShadowStorageOwner_t *owner );

#endif
