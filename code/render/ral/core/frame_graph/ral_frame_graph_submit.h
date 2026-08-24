// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
// Exact end-and-submit transaction for recorded frame-graph batches.

#ifndef WIRED_RAL_FRAME_GRAPH_SUBMIT_H
#define WIRED_RAL_FRAME_GRAPH_SUBMIT_H

#include "ral_frame_graph_record.h"
#include "../ral_sync.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RAL_FRAME_GRAPH_SUBMIT_SCHEMA_VERSION 1u

typedef enum {
	RAL_FRAME_GRAPH_SUBMIT_REJECTED = 0,
	RAL_FRAME_GRAPH_SUBMIT_COMPLETE,
	RAL_FRAME_GRAPH_SUBMIT_FAILED
} ralFrameGraphSubmitOutcome_t;

typedef enum {
	RAL_FRAME_GRAPH_SUBMIT_FAILURE_END = 1,
	RAL_FRAME_GRAPH_SUBMIT_FAILURE_QUEUE
} ralFrameGraphSubmitFailureStage_t;

typedef struct {
	qboolean (*getCommandReceipt)( void *context, ralCommandBuffer_t *command,
		ralCommandReceipt_t *outReceipt );
	uint64_t (*getTimelineValue)( void *context, ralSemaphore_t *semaphore );
	ralResult_t (*endCommand)( void *context, ralCommandBuffer_t *command,
		const ralCommandReceipt_t *recording,
		ralCommandReceipt_t *outExecutable );
	ralResult_t (*cancelCommand)( void *context, ralCommandBuffer_t *command,
		const ralCommandReceipt_t *authority );
	ralResult_t (*submitExact)( void *context, ralBackend_t *backend,
		ralQueueType_t queue, const ralSubmitInfo_t *submit,
		const ralCommandReceipt_t *executable,
		ralSubmissionReceipt_t *outReceipt );
} ralFrameGraphSubmitOps_t;

typedef struct {
	uint32_t schemaVersion;
	uint64_t generation;
	const ralFrameGraphExecutionPlan_t *executionPlan;
	const ralFrameGraphRecordingReceipt_t *recordingReceipt;
	ralFrameGraphBatchCommand_t batchCommands[RAL_FRAME_GRAPH_MAX_PASSES];
	uint32_t batchCommandCount;
	qboolean timelineSemaphoresSupported;
	ralSemaphore_t *queueTimelines[RAL_FRAME_GRAPH_PHYSICAL_QUEUE_COUNT];
	const ralFrameGraphSubmitOps_t *ops;
	void *opsContext;
} ralFrameGraphSubmitDescription_t;

typedef struct {
	uint32_t schemaVersion;
	const ralBackend_t *backendIdentity;
	const ralFrameGraphExecutionPlan_t *executionPlanIdentity;
	const ralFrameGraphRecordingReceipt_t *recordingReceiptIdentity;
	const ralFrameGraphSubmitOps_t *opsIdentity;
	void *opsContextIdentity;
	uint64_t executionGeneration;
	uint64_t graphGeneration;
	uint64_t recordingGeneration;
	uint64_t submissionGeneration;
	qboolean timelineSemaphoresSupported;
	ralSemaphore_t *queueTimelines[RAL_FRAME_GRAPH_PHYSICAL_QUEUE_COUNT];
	uint64_t timelineBaseValues[RAL_FRAME_GRAPH_PHYSICAL_QUEUE_COUNT];
	uint64_t timelineFinalValues[RAL_FRAME_GRAPH_PHYSICAL_QUEUE_COUNT];
	ralSubmissionReceipt_t submissions[RAL_FRAME_GRAPH_MAX_PASSES];
	uint32_t submissionCount;
	qboolean ready;
} ralFrameGraphSubmissionReceipt_t;

typedef struct {
	uint32_t schemaVersion;
	const ralBackend_t *backendIdentity;
	const ralFrameGraphExecutionPlan_t *executionPlanIdentity;
	const ralFrameGraphRecordingReceipt_t *recordingReceiptIdentity;
	uint64_t submissionGeneration;
	ralFrameGraphSubmitFailureStage_t stage;
	uint32_t failedBatchIndex;
	uint32_t submittedBatchCount;
	ralSubmissionReceipt_t submittedPrefix[RAL_FRAME_GRAPH_MAX_PASSES];
	uint32_t cancellationAttemptCount;
	uint32_t cancellationFailureCount;
	qboolean ready;
} ralFrameGraphSubmissionFailureReceipt_t;

ralFrameGraphSubmitOutcome_t RalFrameGraphSubmit_Submit(
	const ralFrameGraphSubmitDescription_t *description,
	ralFrameGraphSubmissionReceipt_t *outComplete,
	ralFrameGraphSubmissionFailureReceipt_t *outFailure );
qboolean RalFrameGraphSubmit_ReceiptExact(
	const ralFrameGraphSubmissionReceipt_t *a,
	const ralFrameGraphSubmissionReceipt_t *b );
qboolean RalFrameGraphSubmit_FailureReceiptExact(
	const ralFrameGraphSubmissionFailureReceipt_t *a,
	const ralFrameGraphSubmissionFailureReceipt_t *b );

const ralFrameGraphSubmitOps_t *RalFrameGraphSubmit_RalOps( void );

#ifdef __cplusplus
}
#endif

#endif // WIRED_RAL_FRAME_GRAPH_SUBMIT_H
