// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef WIRED_VK_ATMOSPHERIC_FRAME_H
#define WIRED_VK_ATMOSPHERIC_FRAME_H

#include "vk_ral_frame_uniform.h"

#define VK_ATMOSPHERIC_FRAME_SLOT_COUNT VK_RAL_FRAME_UNIFORM_SLOT_COUNT
#define VK_ATMOSPHERIC_FRAME_BYTE_SIZE 208u
#define VK_ATMOSPHERIC_FRAME_COMPUTE_OFFSET 96u
#define VK_ATMOSPHERIC_FRAME_COMPUTE_SIZE 100u
#define VK_ATMOSPHERIC_FRAME_RECEIPT_SCHEMA VK_RAL_FRAME_UNIFORM_RECEIPT_SCHEMA

typedef vkRalFrameUniformResourcesReceipt_t
	vkAtmosphericFrameResourcesReceipt_t;
typedef vkRalFrameUniformReceipt_t vkAtmosphericFrameReceipt_t;
typedef vkRalFrameUniformOwner_t vkAtmosphericFrameOwner_t;

void VK_AtmosphericFrameInit( vkAtmosphericFrameOwner_t *owner );
qboolean VK_AtmosphericFrameEnsure( vkAtmosphericFrameOwner_t *owner,
	ralBackend_t *backend );
qboolean VK_AtmosphericFrameGetResources(
	const vkAtmosphericFrameOwner_t *owner,
	vkAtmosphericFrameResourcesReceipt_t *outReceipt );
qboolean VK_AtmosphericFrameResourcesReceiptExact(
	const vkAtmosphericFrameResourcesReceipt_t *a,
	const vkAtmosphericFrameResourcesReceipt_t *b );
qboolean VK_AtmosphericFrameBeginSlot( vkAtmosphericFrameOwner_t *owner,
	uint32_t commandSlot, qboolean fenceRequired,
	qboolean fenceCompleted );
qboolean VK_AtmosphericFrameGetShadow( vkAtmosphericFrameOwner_t *owner,
	uint32_t commandSlot, void **outShadow );
qboolean VK_AtmosphericFramePublishCompute(
	vkAtmosphericFrameOwner_t *owner, uint32_t commandSlot );
qboolean VK_AtmosphericFramePublishFinal(
	vkAtmosphericFrameOwner_t *owner, uint32_t commandSlot,
	vkAtmosphericFrameReceipt_t *outReceipt );
qboolean VK_AtmosphericFrameGetReceipt(
	const vkAtmosphericFrameOwner_t *owner, uint32_t commandSlot,
	vkAtmosphericFrameReceipt_t *outReceipt );
qboolean VK_AtmosphericFrameReceiptExact(
	const vkAtmosphericFrameReceipt_t *a,
	const vkAtmosphericFrameReceipt_t *b );
qboolean VK_AtmosphericFrameHasLive(
	const vkAtmosphericFrameOwner_t *owner );
void VK_AtmosphericFrameRelease( vkAtmosphericFrameOwner_t *owner );

#endif
