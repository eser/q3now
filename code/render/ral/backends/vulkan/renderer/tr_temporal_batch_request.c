// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "tr_temporal_batch_request.h"

#include <string.h>

static qboolean TupleValid( const temporalBatchRequest_t *request ) {
	return request && request->token && request->frameId
		&& request->worldIndex >= 0 && request->width && request->height
		&& request->topologyEpoch && request->planGeneration
		&& request->enabled <= 1u ? qtrue : qfalse;
}

qboolean R_TemporalBatchRequestValidateExact(
		const temporalBatchRequest_t *request ) {
	return request && request->state == TEMPORAL_BATCH_REQUEST_EXACT
		&& TupleValid( request ) ? qtrue : qfalse;
}

static qboolean TupleEqual( const temporalBatchRequest_t *a,
		const temporalBatchRequest_t *b ) {
	return a->token == b->token && a->worldIndex == b->worldIndex
		&& a->frameId == b->frameId && a->width == b->width
		&& a->height == b->height
		&& a->topologyEpoch == b->topologyEpoch
		&& a->planGeneration == b->planGeneration
		&& a->enabled == b->enabled ? qtrue : qfalse;
}

void R_TemporalBatchRequestReset( temporalBatchRequest_t *request,
		uint64_t token ) {
	if ( !request || !token ) return;
	memset( request, 0, sizeof( *request ) );
	request->state = TEMPORAL_BATCH_REQUEST_NONE;
	request->token = token;
}

qboolean R_TemporalBatchRequestPublish( temporalBatchRequest_t *request,
		const temporalBatchRequest_t *candidate ) {
	if ( !request || !candidate || !R_TemporalBatchRequestValidateExact( candidate )
			|| !request->token || request->token != candidate->token
			|| request->state == TEMPORAL_BATCH_REQUEST_CONSUMED ) return qfalse;
	if ( request->state == TEMPORAL_BATCH_REQUEST_CONFLICT ) return qfalse;
	if ( request->state == TEMPORAL_BATCH_REQUEST_EXACT ) {
		if ( TupleEqual( request, candidate ) ) return qtrue;
		request->state = TEMPORAL_BATCH_REQUEST_CONFLICT;
		return qfalse;
	}
	if ( request->state != TEMPORAL_BATCH_REQUEST_NONE ) return qfalse;
	*request = *candidate;
	request->state = TEMPORAL_BATCH_REQUEST_EXACT;
	return qtrue;
}

qboolean R_TemporalBatchRequestConsume( temporalBatchRequest_t *request,
		temporalBatchRequest_t *outRequest ) {
	temporalBatchRequest_t snapshot;
	if ( !request || !outRequest || request == outRequest || !request->token
			|| request->state == TEMPORAL_BATCH_REQUEST_CONSUMED ) return qfalse;
	snapshot = *request;
	request->state = TEMPORAL_BATCH_REQUEST_CONSUMED;
	*outRequest = snapshot;
	return qtrue;
}
