// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_frame_graph_submit.h"

static qboolean GetCommandReceipt( void *context, ralCommandBuffer_t *command,
		ralCommandReceipt_t *outReceipt ) {
	(void)context;
	return Ral_GetCommandBufferReceipt(command,outReceipt) == ralSuccess
		? qtrue : qfalse;
}

static uint64_t GetTimelineValue( void *context, ralSemaphore_t *semaphore ) {
	(void)context;
	return Ral_GetTimelineValue(semaphore);
}

static ralResult_t EndCommand( void *context, ralCommandBuffer_t *command,
		const ralCommandReceipt_t *recording,
		ralCommandReceipt_t *outExecutable ) {
	(void)context;
	return Ral_EndCommandBufferExact(command,recording,outExecutable);
}

static ralResult_t CancelCommand( void *context, ralCommandBuffer_t *command,
		const ralCommandReceipt_t *authority ) {
	(void)context;
	return Ral_CancelCommandBuffer(command,authority);
}

static ralResult_t SubmitExact( void *context, ralBackend_t *backend,
		ralQueueType_t queue, const ralSubmitInfo_t *submit,
		const ralCommandReceipt_t *executable,
		ralSubmissionReceipt_t *outReceipt ) {
	(void)context;
	return Ral_SubmitExact(backend,queue,submit,executable,outReceipt);
}

const ralFrameGraphSubmitOps_t *RalFrameGraphSubmit_RalOps( void ) {
	static const ralFrameGraphSubmitOps_t ops = {
		GetCommandReceipt, GetTimelineValue, EndCommand, CancelCommand, SubmitExact
	};
	return &ops;
}
