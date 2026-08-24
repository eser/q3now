// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef WIRED_VK_ATMOSPHERIC_POOL_H
#define WIRED_VK_ATMOSPHERIC_POOL_H

#include "../../../core/ral.h"

#define VK_ATMOSPHERIC_POOL_BUFFER_COUNT 2u
#define VK_ATMOSPHERIC_POOL_RECEIPT_SCHEMA 1u

typedef struct {
	uint32_t schemaVersion;
	ralBackend_t *backend;
	ralBuffer_t *buffers[VK_ATMOSPHERIC_POOL_BUFFER_COUNT];
	ralAllocationReceipt_t allocations[VK_ATMOSPHERIC_POOL_BUFFER_COUNT];
	ralBufferUploadReceipt_t uploads[VK_ATMOSPHERIC_POOL_BUFFER_COUNT];
	uint64_t poolGeneration;
	uint64_t byteSize;
	qboolean ready;
} vkAtmosphericPoolReceipt_t;

typedef struct {
	qboolean initialized;
	ralBackend_t *backend;
	ralBuffer_t *buffers[VK_ATMOSPHERIC_POOL_BUFFER_COUNT];
	ralAllocationReceipt_t allocations[VK_ATMOSPHERIC_POOL_BUFFER_COUNT];
	ralBufferUploadReceipt_t uploads[VK_ATMOSPHERIC_POOL_BUFFER_COUNT];
	uint64_t poolGeneration;
	uint64_t byteSize;
	qboolean ready;
} vkAtmosphericPoolOwner_t;

void VK_AtmosphericPoolInit( vkAtmosphericPoolOwner_t *owner );
qboolean VK_AtmosphericPoolEnsure( vkAtmosphericPoolOwner_t *owner,
	ralBackend_t *backend, uint64_t byteSize );
qboolean VK_AtmosphericPoolGetReceipt(
	const vkAtmosphericPoolOwner_t *owner,
	vkAtmosphericPoolReceipt_t *outReceipt );
qboolean VK_AtmosphericPoolReceiptExact(
	const vkAtmosphericPoolReceipt_t *a,
	const vkAtmosphericPoolReceipt_t *b );
qboolean VK_AtmosphericPoolHasLive(
	const vkAtmosphericPoolOwner_t *owner );
void VK_AtmosphericPoolRelease( vkAtmosphericPoolOwner_t *owner );

#endif
