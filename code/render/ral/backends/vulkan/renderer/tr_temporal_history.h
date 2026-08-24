// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#ifndef WIRED_TR_TEMPORAL_HISTORY_H
#define WIRED_TR_TEMPORAL_HISTORY_H

#include "../../../core/ral_backend.h"
#include "../../../core/ral_resource.h"
#include "../../../core/ral_temporal.h"
#include "tr_temporal_batch_request.h"

typedef struct {
	ralBackend_t *backend;
	ralTexture_t *color[2];
	ralTextureView_t *colorView[2];
	ralTexture_t *depth[2];
	ralTextureView_t *depthView[2];
	uint32_t width, height, topologyEpoch, allocationGeneration;
	qboolean ready;
} temporalHistoryResources_t;

typedef enum {
	TEMPORAL_HISTORY_WRITE_NONE = 0,
	TEMPORAL_HISTORY_WRITE_CURRENT_SEED = 1,
	TEMPORAL_HISTORY_WRITE_RESOLVED_FEEDBACK = 2
} temporalHistoryWriteProducer_t;

typedef struct {
	ralBackend_t *backend;
	ralTexture_t *sourceSceneColor;
	ralBindGroup_t *sourcePostprocessGroup;
	ralBindGroup_t *sourceHistogramGroup;
	ralTexture_t *sourceColor;
	ralTextureView_t *sourceColorView;
	ralBindGroup_t *postprocessGroup;
	ralBindGroup_t *histogramGroup;
	uint64_t batchToken;
	uint64_t frameId;
	uint64_t contentSerial;
	uint32_t commandSlot;
	uint32_t frameCount;
	int32_t worldIndex;
	uint32_t width;
	uint32_t height;
	uint32_t topologyEpoch;
	uint32_t planGeneration;
	uint32_t sceneColorAttachmentGeneration;
	uint32_t targetAllocationGeneration;
	uint32_t resolveOwnerAllocationGeneration;
	uint32_t storeOwnerAllocationGeneration;
	ralFormat_t sceneFormat;
	temporalHistoryWriteProducer_t producer;
} temporalHistoryFeedbackSource_t;

typedef struct {
	uint64_t frameId;
	int32_t worldIndex;
	uint32_t planGeneration;
	uint32_t allocationGeneration;
	uint32_t historyIndex;
	uint32_t width;
	uint32_t height;
	uint32_t topologyEpoch;
	ralTexture_t *color;
	ralTextureView_t *colorView;
	ralTexture_t *depth;
	ralTextureView_t *depthView;
	temporalHistoryFeedbackSource_t source;
	qboolean valid;
} temporalHistoryCommittedReceipt_t;

typedef struct {
	temporalHistoryCommittedReceipt_t committed;
	uint32_t readIndex;
	uint32_t writeIndex;
	qboolean historyValid;
	ralTexture_t *readColor;
	ralTextureView_t *readColorView;
	ralTexture_t *readDepth;
	ralTextureView_t *readDepthView;
	ralTexture_t *writeColor;
	ralTextureView_t *writeColorView;
	ralTexture_t *writeDepth;
	ralTextureView_t *writeDepthView;
} temporalHistoryFrameView_t;

// Exact command-local provenance for the physical history write.  This is a
// pre-submit candidate, not a committed receipt: the backend may publish the
// embedded write only after the matching producer transaction is accepted.
typedef struct {
	temporalHistoryCommittedReceipt_t write;
	qboolean valid;
} temporalHistoryPendingWriteReceipt_t;

typedef enum {
	TEMPORAL_HISTORY_SUBMIT_REJECT = 0,
	TEMPORAL_HISTORY_SUBMIT_CANCEL = 1,
	TEMPORAL_HISTORY_SUBMIT_COMMIT_FRAME = 2,
	TEMPORAL_HISTORY_SUBMIT_COMMIT_HISTORY = 3
} temporalHistorySubmitDecision_t;

typedef enum {
	TEMPORAL_BACKEND_SUBMIT_NONE = 0,
	TEMPORAL_BACKEND_SUBMIT_EXACT = 1,
	TEMPORAL_BACKEND_SUBMIT_INVALID = 2
} temporalBackendSubmitQuery_t;

// Command-batch identity captured before the outer graphics submit and
// revalidated before the temporal decision is applied.  This prevents an
// ordinary startup/UI submit from masquerading as a temporal cancel while
// still failing closed on a missing, duplicated, or stale temporal batch.
typedef struct {
	uint64_t batchToken;
	uint64_t frameId;
	int32_t worldIndex;
	uint32_t planGeneration;
	uint32_t topologyEpoch;
	uint32_t width;
	uint32_t height;
	uint32_t historyWriteIndex;
	uint32_t enabled;
	ralTemporalFramePlan_t plan;
	qboolean recorded;
	qboolean valid;
} temporalBackendSubmitAuthority_t;

void R_TemporalHistoryInit( temporalHistoryResources_t *history );
qboolean R_TemporalHistoryEnsure( temporalHistoryResources_t *history,
	ralBackend_t *backend, uint32_t width, uint32_t height,
	uint32_t topologyEpoch );
void R_TemporalHistoryRelease( temporalHistoryResources_t *history );

// Validates the logical plan against the physical committed-slot receipt and
// returns an exact borrowed read/write view.  Invalid/bootstrap frames never
// publish a readable previous slot.  Failure leaves outView untouched.
qboolean R_TemporalHistoryBuildFrameView(
	const temporalHistoryResources_t *history,
	const ralTemporalFramePlan_t *plan,
	const temporalHistoryCommittedReceipt_t *committed,
	int worldIndex, temporalHistoryFrameView_t *outView );

// Builds the exact candidate that a later accepted submit may publish.  A
// bootstrap frame accepts only CURRENT_SEED; a history-valid frame accepts
// only RESOLVED_FEEDBACK with a nonzero resolve-owner generation.  Failure is
// output-atomic.
qboolean R_TemporalHistoryBuildPendingWrite(
	const temporalHistoryResources_t *history,
	const ralTemporalFramePlan_t *plan, int worldIndex,
	const temporalHistoryFeedbackSource_t *source,
	temporalHistoryPendingWriteReceipt_t *outPending );
qboolean R_TemporalHistoryPendingWriteEqualExact(
	const temporalHistoryPendingWriteReceipt_t *a,
	const temporalHistoryPendingWriteReceipt_t *b );
temporalHistorySubmitDecision_t R_TemporalHistoryChooseSubmitDecision(
	qboolean submitted, qboolean planEnabled, qboolean historyRecorded,
	qboolean stagedWriteValid, qboolean authorityPresent,
	qboolean authorityExact, qboolean historyPublishable );
temporalBackendSubmitQuery_t R_TemporalHistoryClassifyBackendSubmitAuthority(
	uint32_t pendingCount, qboolean backendPresent, qboolean authorityExact );
temporalBackendSubmitQuery_t R_TemporalHistoryClassifyBatchDelivery(
	qboolean requestPresent, qboolean tokenExact,
	temporalBatchRequestState_t requestState );
qboolean R_TemporalBackendSubmitAuthorityMatchesRequest(
	const temporalBackendSubmitAuthority_t *authority,
	const temporalBatchRequest_t *request );

// Backend hand-off. The frame plan/resources remain owned by tr_temporal_input;
// callers receive borrowed pointers valid through the frame submit decision.
qboolean R_TemporalBackendGetPending( int worldIndex, uint64_t frameId,
	const ralTemporalFramePlan_t **plan, temporalHistoryResources_t **history );
qboolean R_TemporalBackendStageHistoryWrite( int worldIndex, uint64_t frameId,
	const temporalHistoryPendingWriteReceipt_t *pending,
	qboolean previousSlotRead );
qboolean R_TemporalBackendGetPendingHistoryWrite(
	temporalHistoryPendingWriteReceipt_t *outPending );
temporalBackendSubmitQuery_t R_TemporalBackendQuerySubmitAuthority(
	temporalBackendSubmitAuthority_t *outAuthority );
qboolean R_TemporalBackendCanResolveSubmit(
	const temporalBackendSubmitAuthority_t *authority, qboolean submitted,
	const temporalHistoryPendingWriteReceipt_t *authorizedWrite );
void R_TemporalBackendMarkPreviousSlotRead( int worldIndex, uint64_t frameId );
qboolean R_TemporalBackendGetCommittedHistory(
	int worldIndex, uint32_t historyIndex,
	temporalHistoryCommittedReceipt_t *outReceipt );

#endif
