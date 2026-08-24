// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "vk_atmospheric_frame.h"

static const vkRalFrameUniformConfig_t atmosphericConfig = {
	VK_ATMOSPHERIC_FRAME_BYTE_SIZE,
	VK_ATMOSPHERIC_FRAME_COMPUTE_OFFSET,
	VK_ATMOSPHERIC_FRAME_COMPUTE_SIZE,
	qtrue,
	"wired-atmospheric-frame"
};

void VK_AtmosphericFrameInit( vkAtmosphericFrameOwner_t *owner ) {
	VK_RalFrameUniformInit( owner );
}

qboolean VK_AtmosphericFrameEnsure( vkAtmosphericFrameOwner_t *owner,
		ralBackend_t *backend ) {
	return VK_RalFrameUniformEnsure( owner, backend, &atmosphericConfig );
}

qboolean VK_AtmosphericFrameGetResources(
		const vkAtmosphericFrameOwner_t *owner,
		vkAtmosphericFrameResourcesReceipt_t *outReceipt ) {
	return VK_RalFrameUniformGetResources( owner, outReceipt );
}

qboolean VK_AtmosphericFrameResourcesReceiptExact(
		const vkAtmosphericFrameResourcesReceipt_t *a,
		const vkAtmosphericFrameResourcesReceipt_t *b ) {
	return VK_RalFrameUniformResourcesReceiptExact( a, b );
}

qboolean VK_AtmosphericFrameBeginSlot( vkAtmosphericFrameOwner_t *owner,
		uint32_t commandSlot, qboolean fenceRequired,
		qboolean fenceCompleted ) {
	return VK_RalFrameUniformBeginSlot( owner, commandSlot, fenceRequired,
		fenceCompleted );
}

qboolean VK_AtmosphericFrameGetShadow( vkAtmosphericFrameOwner_t *owner,
		uint32_t commandSlot, void **outShadow ) {
	return VK_RalFrameUniformGetShadow( owner, commandSlot, outShadow );
}

qboolean VK_AtmosphericFramePublishCompute(
		vkAtmosphericFrameOwner_t *owner, uint32_t commandSlot ) {
	return VK_RalFrameUniformPublishPartial( owner, commandSlot );
}

qboolean VK_AtmosphericFramePublishFinal(
		vkAtmosphericFrameOwner_t *owner, uint32_t commandSlot,
		vkAtmosphericFrameReceipt_t *outReceipt ) {
	return VK_RalFrameUniformPublishFinal( owner, commandSlot, outReceipt );
}

qboolean VK_AtmosphericFrameGetReceipt(
		const vkAtmosphericFrameOwner_t *owner, uint32_t commandSlot,
		vkAtmosphericFrameReceipt_t *outReceipt ) {
	return VK_RalFrameUniformGetReceipt( owner, commandSlot, outReceipt );
}

qboolean VK_AtmosphericFrameReceiptExact(
		const vkAtmosphericFrameReceipt_t *a,
		const vkAtmosphericFrameReceipt_t *b ) {
	return VK_RalFrameUniformReceiptExact( a, b );
}

qboolean VK_AtmosphericFrameHasLive(
		const vkAtmosphericFrameOwner_t *owner ) {
	return VK_RalFrameUniformHasLive( owner );
}

void VK_AtmosphericFrameRelease( vkAtmosphericFrameOwner_t *owner ) {
	VK_RalFrameUniformRelease( owner );
}
