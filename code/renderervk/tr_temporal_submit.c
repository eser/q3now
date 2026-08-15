// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "tr_temporal_history.h"

temporalBackendSubmitQuery_t R_TemporalHistoryClassifyBackendSubmitAuthority(
		uint32_t pendingCount, qboolean backendPresent,
		qboolean authorityExact ) {
	if ( !pendingCount && !backendPresent )
		return TEMPORAL_BACKEND_SUBMIT_NONE;
	if ( pendingCount == 1u && backendPresent && authorityExact )
		return TEMPORAL_BACKEND_SUBMIT_EXACT;
	return TEMPORAL_BACKEND_SUBMIT_INVALID;
}

temporalBackendSubmitQuery_t R_TemporalHistoryClassifyBatchDelivery(
		qboolean requestPresent, qboolean tokenExact,
		temporalBatchRequestState_t requestState ) {
	if ( !requestPresent || requestState == TEMPORAL_BATCH_REQUEST_NONE )
		return TEMPORAL_BACKEND_SUBMIT_NONE;
	if ( requestState == TEMPORAL_BATCH_REQUEST_EXACT && tokenExact )
		return TEMPORAL_BACKEND_SUBMIT_EXACT;
	return TEMPORAL_BACKEND_SUBMIT_INVALID;
}

qboolean R_TemporalBackendSubmitAuthorityMatchesRequest(
		const temporalBackendSubmitAuthority_t *authority,
		const temporalBatchRequest_t *request ) {
	return authority && authority->valid
		&& R_TemporalBatchRequestValidateExact( request )
		&& authority->batchToken == request->token
		&& authority->worldIndex == request->worldIndex
		&& authority->frameId == request->frameId
		&& authority->width == request->width
		&& authority->height == request->height
		&& authority->topologyEpoch == request->topologyEpoch
		&& authority->planGeneration == request->planGeneration
		&& authority->enabled == request->enabled
		&& authority->plan.frameId == request->frameId
		&& authority->plan.generation == request->planGeneration
		&& authority->plan.enabled == request->enabled ? qtrue : qfalse;
}

temporalHistorySubmitDecision_t R_TemporalHistoryChooseSubmitDecision(
		qboolean submitted, qboolean planEnabled, qboolean historyRecorded,
		qboolean stagedWriteValid, qboolean authorityPresent,
		qboolean authorityExact, qboolean historyPublishable ) {
	if ( !submitted ) return TEMPORAL_HISTORY_SUBMIT_CANCEL;
	if ( !planEnabled )
		return historyRecorded || stagedWriteValid || authorityPresent
			? TEMPORAL_HISTORY_SUBMIT_REJECT
			: TEMPORAL_HISTORY_SUBMIT_COMMIT_FRAME;
	if ( historyRecorded != stagedWriteValid )
		return TEMPORAL_HISTORY_SUBMIT_REJECT;
	if ( !historyRecorded )
		return authorityPresent ? TEMPORAL_HISTORY_SUBMIT_REJECT
			: TEMPORAL_HISTORY_SUBMIT_CANCEL;
	if ( !authorityPresent || !authorityExact || !historyPublishable )
		return TEMPORAL_HISTORY_SUBMIT_REJECT;
	return TEMPORAL_HISTORY_SUBMIT_COMMIT_HISTORY;
}
