// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
// Automatic RAL command recording for an exact frame-graph execution plan.

#ifndef WIRED_RAL_FRAME_GRAPH_RECORD_H
#define WIRED_RAL_FRAME_GRAPH_RECORD_H

#include "ral_frame_graph_execution.h"
#include "../ral/ral_command.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RAL_FRAME_GRAPH_RECORD_SCHEMA_VERSION 1u

typedef struct {
	uint32_t submissionBatchIndex;
	ralCommandBuffer_t *commandBuffer;
	ralCommandReceipt_t recordingReceipt;
} ralFrameGraphBatchCommand_t;

typedef struct {
	qboolean (*getCommandReceipt)( void *context, ralCommandBuffer_t *command,
		ralCommandReceipt_t *outReceipt );
	ralResult_t (*transitionResources)( void *context, ralCommandBuffer_t *command,
		const ralResourceTransitionBatch_t *batch );
	ralResult_t (*releaseBuffer)( void *context, ralCommandBuffer_t *command,
		const ralBufferTransition_t *transition,
		ralQueueTransferReceipt_t *outReceipt );
	ralResult_t (*acquireBuffer)( void *context, ralCommandBuffer_t *command,
		const ralBufferTransition_t *transition,
		const ralQueueTransferReceipt_t *receipt );
	ralResult_t (*cancelBuffer)( void *context, ralBuffer_t *buffer,
		const ralQueueTransferReceipt_t *receipt );
	ralResult_t (*releaseTexture)( void *context, ralCommandBuffer_t *command,
		const ralTextureTransition_t *transition,
		ralQueueTransferReceipt_t *outReceipt );
	ralResult_t (*acquireTexture)( void *context, ralCommandBuffer_t *command,
		const ralTextureTransition_t *transition,
		const ralQueueTransferReceipt_t *receipt );
	ralResult_t (*cancelTexture)( void *context, ralTexture_t *texture,
		const ralQueueTransferReceipt_t *receipt );
} ralFrameGraphRecordOps_t;

typedef struct {
	qboolean (*preflight)( void *context,
		const ralFrameGraphExecutionPass_t *pass,
		const ralFrameGraphExecutionPlan_t *plan,
		ralCommandBuffer_t *command );
	void (*record)( void *context, const ralFrameGraphExecutionPass_t *pass,
		const ralFrameGraphExecutionPlan_t *plan,
		ralCommandBuffer_t *command );
	void *context;
} ralFrameGraphPassCallbacks_t;

typedef struct {
	uint32_t schemaVersion;
	uint64_t generation;
	const ralFrameGraphExecutionPlan_t *executionPlan;
	ralFrameGraphBatchCommand_t batchCommands[RAL_FRAME_GRAPH_MAX_PASSES];
	uint32_t batchCommandCount;
	const ralFrameGraphRecordOps_t *ops;
	void *opsContext;
	ralFrameGraphPassCallbacks_t passCallbacks;
} ralFrameGraphRecordDescription_t;

typedef struct {
	uint32_t schemaVersion;
	const ralBackend_t *backendIdentity;
	const ralFrameGraphExecutionPlan_t *executionPlanIdentity;
	const ralFrameGraphRecordOps_t *opsIdentity;
	void *opsContextIdentity;
	qboolean (*passPreflightIdentity)( void *context,
		const ralFrameGraphExecutionPass_t *pass,
		const ralFrameGraphExecutionPlan_t *plan,
		ralCommandBuffer_t *command );
	void (*passRecordIdentity)( void *context,
		const ralFrameGraphExecutionPass_t *pass,
		const ralFrameGraphExecutionPlan_t *plan,
		ralCommandBuffer_t *command );
	void *passContextIdentity;
	uint64_t executionGeneration;
	uint64_t graphGeneration;
	uint64_t recordingGeneration;
	uint32_t passCount;
	uint32_t submissionBatchCount;
	uint32_t transitionBatchCount;
	uint32_t ownershipReleaseCount;
	uint32_t ownershipAcquireCount;
	ralCommandReceipt_t recordingCommands[RAL_FRAME_GRAPH_MAX_PASSES];
	uint32_t recordingCommandCount;
	qboolean ready;
} ralFrameGraphRecordingReceipt_t;

qboolean RalFrameGraphRecord_Record(
	const ralFrameGraphRecordDescription_t *description,
	ralFrameGraphRecordingReceipt_t *outReceipt );
qboolean RalFrameGraphRecord_ReceiptExact(
	const ralFrameGraphRecordingReceipt_t *a,
	const ralFrameGraphRecordingReceipt_t *b );

// Production adapter: every callback delegates to the public RAL command
// surface. Keeping it separate lets host tests capture exact call order while
// the core recorder remains backend-neutral.
const ralFrameGraphRecordOps_t *RalFrameGraphRecord_RalOps( void );

#ifdef __cplusplus
}
#endif

#endif // WIRED_RAL_FRAME_GRAPH_RECORD_H
