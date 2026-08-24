// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef WIRED_VK_RAL_BUFFER_SHADOW_H
#define WIRED_VK_RAL_BUFFER_SHADOW_H

#include "../../../core/ral.h"

#define VK_RAL_BUFFER_SHADOW_RECEIPT_SCHEMA 1u

typedef struct {
	uint32_t schemaVersion;
	ralBackend_t *backend;
	ralBuffer_t *buffer;
	unsigned char *bytes;
	ralAllocationReceipt_t allocation;
	uint64_t byteSize;
	uint64_t generation;
	ralBufferUsage_t consumerUsage;
	qboolean ready;
} vkRalBufferShadow_t;

typedef struct {
	uint32_t schemaVersion;
	ralBuffer_t *buffer;
	ralAllocationReceipt_t allocation;
	ralBufferUploadReceipt_t upload;
	uint64_t shadowGeneration;
	uint64_t offset;
	uint64_t size;
	qboolean ready;
} vkRalBufferShadowWriteReceipt_t;

void VK_RalBufferShadowInit( vkRalBufferShadow_t *shadow );
qboolean VK_RalBufferShadowEnsure( vkRalBufferShadow_t *shadow,
	ralBackend_t *backend, uint64_t byteSize, ralBufferUsage_t consumerUsage,
	const char *debugName );
qboolean VK_RalBufferShadowWrite( vkRalBufferShadow_t *shadow,
	uint64_t offset, const void *data, uint64_t size );
qboolean VK_RalBufferShadowMarkWritten( vkRalBufferShadow_t *shadow,
	uint64_t offset, uint64_t size );
qboolean VK_RalBufferShadowPublish( vkRalBufferShadow_t *shadow,
	uint64_t offset, uint64_t size, vkRalBufferShadowWriteReceipt_t *outReceipt );
qboolean VK_RalBufferShadowWriteReceiptExact(
	const vkRalBufferShadowWriteReceipt_t *a,
	const vkRalBufferShadowWriteReceipt_t *b );
qboolean VK_RalBufferShadowHasLive( const vkRalBufferShadow_t *shadow );
void VK_RalBufferShadowRelease( vkRalBufferShadow_t *shadow );

#endif
