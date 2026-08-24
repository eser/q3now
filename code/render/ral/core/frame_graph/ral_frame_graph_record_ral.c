// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_frame_graph_record.h"

static qboolean GetCommandReceipt( void *context, ralCommandBuffer_t *command,
		ralCommandReceipt_t *outReceipt ) {
	(void)context;
	return Ral_GetCommandBufferReceipt(command,outReceipt) == ralSuccess
		? qtrue : qfalse;
}

static ralResult_t TransitionResources( void *context,
		ralCommandBuffer_t *command, const ralResourceTransitionBatch_t *batch ) {
	(void)context;
	return Ral_CmdTransitionResources(command,batch);
}

static ralResult_t ReleaseBuffer( void *context, ralCommandBuffer_t *command,
		const ralBufferTransition_t *transition,
		ralQueueTransferReceipt_t *outReceipt ) {
	(void)context;
	return Ral_CmdReleaseBufferOwnership(command,transition,outReceipt);
}

static ralResult_t AcquireBuffer( void *context, ralCommandBuffer_t *command,
		const ralBufferTransition_t *transition,
		const ralQueueTransferReceipt_t *receipt ) {
	(void)context;
	return Ral_CmdAcquireBufferOwnership(command,transition,receipt);
}

static ralResult_t CancelBuffer( void *context, ralBuffer_t *buffer,
		const ralQueueTransferReceipt_t *receipt ) {
	(void)context;
	return Ral_CancelBufferOwnershipTransfer(buffer,receipt);
}

static ralResult_t ReleaseTexture( void *context, ralCommandBuffer_t *command,
		const ralTextureTransition_t *transition,
		ralQueueTransferReceipt_t *outReceipt ) {
	(void)context;
	return Ral_CmdReleaseTextureOwnership(command,transition,outReceipt);
}

static ralResult_t AcquireTexture( void *context, ralCommandBuffer_t *command,
		const ralTextureTransition_t *transition,
		const ralQueueTransferReceipt_t *receipt ) {
	(void)context;
	return Ral_CmdAcquireTextureOwnership(command,transition,receipt);
}

static ralResult_t CancelTexture( void *context, ralTexture_t *texture,
		const ralQueueTransferReceipt_t *receipt ) {
	(void)context;
	return Ral_CancelTextureOwnershipTransfer(texture,receipt);
}

const ralFrameGraphRecordOps_t *RalFrameGraphRecord_RalOps( void ) {
	static const ralFrameGraphRecordOps_t ops = {
		GetCommandReceipt, TransitionResources,
		ReleaseBuffer, AcquireBuffer, CancelBuffer,
		ReleaseTexture, AcquireTexture, CancelTexture
	};
	return &ops;
}
