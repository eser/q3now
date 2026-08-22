// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef WIRED_VK_ATMOSPHERIC_HEIGHTGRID_H
#define WIRED_VK_ATMOSPHERIC_HEIGHTGRID_H

#include "../renderer/ral/ral.h"

#define VK_ATMOSPHERIC_HEIGHTGRID_SIZE 256u
#define VK_ATMOSPHERIC_HEIGHTGRID_TEXELS \
	( VK_ATMOSPHERIC_HEIGHTGRID_SIZE * VK_ATMOSPHERIC_HEIGHTGRID_SIZE )
#define VK_ATMOSPHERIC_HEIGHTGRID_BYTES \
	( (uint64_t)VK_ATMOSPHERIC_HEIGHTGRID_TEXELS * sizeof( float ) )

typedef struct {
	uint32_t schemaVersion;
	ralBackend_t *backend;
	ralTexture_t *texture;
	ralSampler_t *sampler;
	ralAllocationReceipt_t allocation;
	ralTransferReceipt_t transfer;
	uint64_t uploadSerial;
	uint32_t width;
	uint32_t height;
	uint64_t byteSize;
	qboolean ready;
} vkAtmosphericHeightgridReceipt_t;

typedef struct {
	qboolean initialized;
	ralBackend_t *backend;
	ralTexture_t *texture;
	ralSampler_t *sampler;
	ralAllocationReceipt_t allocation;
	ralTransferReceipt_t transfer;
	uint64_t uploadSerial;
	qboolean ready;
} vkAtmosphericHeightgridOwner_t;

void VK_AtmosphericHeightgridInit( vkAtmosphericHeightgridOwner_t *owner );
qboolean VK_AtmosphericHeightgridEnsure( vkAtmosphericHeightgridOwner_t *owner,
	ralBackend_t *backend );
qboolean VK_AtmosphericHeightgridUpload( vkAtmosphericHeightgridOwner_t *owner,
	const float *grid, int count, vkAtmosphericHeightgridReceipt_t *outReceipt );
qboolean VK_AtmosphericHeightgridGetReceipt(
	const vkAtmosphericHeightgridOwner_t *owner,
	vkAtmosphericHeightgridReceipt_t *outReceipt );
qboolean VK_AtmosphericHeightgridReceiptExact(
	const vkAtmosphericHeightgridReceipt_t *a,
	const vkAtmosphericHeightgridReceipt_t *b );
qboolean VK_AtmosphericHeightgridHasLive(
	const vkAtmosphericHeightgridOwner_t *owner );
void VK_AtmosphericHeightgridRelease( vkAtmosphericHeightgridOwner_t *owner );

#endif
