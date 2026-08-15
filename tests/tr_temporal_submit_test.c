// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "tr_temporal_history.h"
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { \
	fprintf(stderr, "FAIL temporal submit %d: %s\n", __LINE__, #x); return 1; \
} } while (0)
#define DEC(s,e,r,v,p,x,u) R_TemporalHistoryChooseSubmitDecision( \
	(s),(e),(r),(v),(p),(x),(u))

int main(void) {
	temporalBackendSubmitQuery_t query;
	temporalBackendSubmitAuthority_t authority;
	temporalBatchRequest_t request, mutated;
	memset( &authority, 0, sizeof( authority ) );
	memset( &request, 0, sizeof( request ) );
	authority.batchToken = 101u;
	authority.frameId = 77u;
	authority.worldIndex = 1;
	authority.planGeneration = 9u;
	authority.topologyEpoch = 5u;
	authority.width = 1280u;
	authority.height = 720u;
	authority.historyWriteIndex = 1u;
	authority.enabled = 1u;
	authority.plan.frameId = authority.frameId;
	authority.plan.generation = authority.planGeneration;
	authority.plan.enabled = 1u;
	authority.valid = qtrue;
	request.state = TEMPORAL_BATCH_REQUEST_EXACT;
	request.token = authority.batchToken;
	request.worldIndex = authority.worldIndex;
	request.frameId = authority.frameId;
	request.width = authority.width;
	request.height = authority.height;
	request.topologyEpoch = authority.topologyEpoch;
	request.planGeneration = authority.planGeneration;
	request.enabled = 1u;
	CHECK(R_TemporalBackendSubmitAuthorityMatchesRequest(
		&authority, &request));
	mutated=request; mutated.state=TEMPORAL_BATCH_REQUEST_CONFLICT;
	CHECK(!R_TemporalBackendSubmitAuthorityMatchesRequest(&authority,&mutated));
	mutated=request; mutated.token++;
	CHECK(!R_TemporalBackendSubmitAuthorityMatchesRequest(&authority,&mutated));
	mutated=request; mutated.worldIndex++;
	CHECK(!R_TemporalBackendSubmitAuthorityMatchesRequest(&authority,&mutated));
	mutated=request; mutated.frameId++;
	CHECK(!R_TemporalBackendSubmitAuthorityMatchesRequest(&authority,&mutated));
	mutated=request; mutated.width++;
	CHECK(!R_TemporalBackendSubmitAuthorityMatchesRequest(&authority,&mutated));
	mutated=request; mutated.height++;
	CHECK(!R_TemporalBackendSubmitAuthorityMatchesRequest(&authority,&mutated));
	mutated=request; mutated.topologyEpoch++;
	CHECK(!R_TemporalBackendSubmitAuthorityMatchesRequest(&authority,&mutated));
	mutated=request; mutated.planGeneration++;
	CHECK(!R_TemporalBackendSubmitAuthorityMatchesRequest(&authority,&mutated));
	mutated=request; mutated.enabled=0u;
	CHECK(!R_TemporalBackendSubmitAuthorityMatchesRequest(&authority,&mutated));
	authority.plan.frameId++;
	CHECK(!R_TemporalBackendSubmitAuthorityMatchesRequest(&authority,&request));
	authority.plan.frameId=request.frameId;
	authority.valid=qfalse;
	CHECK(!R_TemporalBackendSubmitAuthorityMatchesRequest(&authority,&request));
	authority.valid=qtrue;
	authority.enabled=0u;
	authority.plan.enabled=0u;
	request.enabled=0u;
	authority.recorded=qfalse;
	CHECK(R_TemporalBackendSubmitAuthorityMatchesRequest(&authority,&request));
	CHECK(R_TemporalHistoryClassifyBackendSubmitAuthority(
		1u,qtrue,qtrue)==TEMPORAL_BACKEND_SUBMIT_EXACT);
	authority.enabled=1u;
	authority.plan.enabled=1u;
	request.enabled=1u;
	CHECK(R_TemporalBackendSubmitAuthorityMatchesRequest(&authority,&request));
	CHECK(R_TemporalHistoryClassifyBackendSubmitAuthority(
		1u,qtrue,qfalse)==TEMPORAL_BACKEND_SUBMIT_INVALID);
	mutated=request;mutated.token++;
	CHECK(!R_TemporalBackendSubmitAuthorityMatchesRequest(&authority,&mutated));
	CHECK(R_TemporalHistoryClassifyBackendSubmitAuthority(
		1u,qtrue,qfalse)==TEMPORAL_BACKEND_SUBMIT_INVALID);
	CHECK(R_TemporalHistoryClassifyBackendSubmitAuthority(
		0u, qfalse, qfalse) == TEMPORAL_BACKEND_SUBMIT_NONE);
	CHECK(R_TemporalHistoryClassifyBackendSubmitAuthority(
		1u, qfalse, qfalse) == TEMPORAL_BACKEND_SUBMIT_INVALID);
	CHECK(R_TemporalHistoryClassifyBackendSubmitAuthority(
		0u, qtrue, qfalse) == TEMPORAL_BACKEND_SUBMIT_INVALID);
	CHECK(R_TemporalHistoryClassifyBackendSubmitAuthority(
		1u, qtrue, qfalse) == TEMPORAL_BACKEND_SUBMIT_INVALID);
	CHECK(R_TemporalHistoryClassifyBackendSubmitAuthority(
		2u, qtrue, qtrue) == TEMPORAL_BACKEND_SUBMIT_INVALID);
	CHECK(R_TemporalHistoryClassifyBackendSubmitAuthority(
		1u, qtrue, qtrue) == TEMPORAL_BACKEND_SUBMIT_EXACT);
	CHECK(R_TemporalHistoryClassifyBatchDelivery(
		qfalse, qfalse, TEMPORAL_BATCH_REQUEST_CONFLICT)
		== TEMPORAL_BACKEND_SUBMIT_NONE);
	CHECK(R_TemporalHistoryClassifyBatchDelivery(
		qtrue, qfalse, TEMPORAL_BATCH_REQUEST_NONE)
		== TEMPORAL_BACKEND_SUBMIT_NONE);
	CHECK(R_TemporalHistoryClassifyBatchDelivery(
		qtrue, qtrue, TEMPORAL_BATCH_REQUEST_NONE)
		== TEMPORAL_BACKEND_SUBMIT_NONE);
	CHECK(R_TemporalHistoryClassifyBatchDelivery(
		qtrue, qtrue, TEMPORAL_BATCH_REQUEST_EXACT)
		== TEMPORAL_BACKEND_SUBMIT_EXACT);
	CHECK(R_TemporalHistoryClassifyBatchDelivery(
		qtrue, qfalse, TEMPORAL_BATCH_REQUEST_EXACT)
		== TEMPORAL_BACKEND_SUBMIT_INVALID);
	CHECK(R_TemporalHistoryClassifyBatchDelivery(
		qtrue, qtrue, TEMPORAL_BATCH_REQUEST_CONFLICT)
		== TEMPORAL_BACKEND_SUBMIT_INVALID);
	CHECK(R_TemporalHistoryClassifyBatchDelivery(
		qtrue, qtrue, TEMPORAL_BATCH_REQUEST_CONSUMED)
		== TEMPORAL_BACKEND_SUBMIT_INVALID);
	query = R_TemporalHistoryClassifyBackendSubmitAuthority(
		1u, qtrue, qtrue);
	CHECK(query == TEMPORAL_BACKEND_SUBMIT_EXACT);
	query = R_TemporalHistoryClassifyBackendSubmitAuthority(
		0u, qfalse, qfalse);
	CHECK(query == TEMPORAL_BACKEND_SUBMIT_NONE);
	CHECK(DEC(qfalse,qtrue,qtrue,qtrue,qtrue,qfalse,qfalse)
		==TEMPORAL_HISTORY_SUBMIT_CANCEL);
	CHECK(DEC(qtrue,qtrue,qfalse,qfalse,qfalse,qfalse,qfalse)
		==TEMPORAL_HISTORY_SUBMIT_CANCEL);
	CHECK(DEC(qtrue,qtrue,qtrue,qtrue,qfalse,qfalse,qtrue)
		==TEMPORAL_HISTORY_SUBMIT_REJECT);
	CHECK(DEC(qtrue,qtrue,qtrue,qtrue,qtrue,qfalse,qtrue)
		==TEMPORAL_HISTORY_SUBMIT_REJECT);
	CHECK(DEC(qtrue,qtrue,qtrue,qtrue,qtrue,qtrue,qfalse)
		==TEMPORAL_HISTORY_SUBMIT_REJECT);
	CHECK(DEC(qtrue,qtrue,qtrue,qtrue,qtrue,qtrue,qtrue)
		==TEMPORAL_HISTORY_SUBMIT_COMMIT_HISTORY);
	CHECK(DEC(qtrue,qtrue,qtrue,qfalse,qtrue,qtrue,qtrue)
		==TEMPORAL_HISTORY_SUBMIT_REJECT);
	CHECK(DEC(qtrue,qfalse,qfalse,qfalse,qfalse,qfalse,qtrue)
		==TEMPORAL_HISTORY_SUBMIT_COMMIT_FRAME);
	CHECK(DEC(qtrue,qfalse,qfalse,qfalse,qtrue,qtrue,qtrue)
		==TEMPORAL_HISTORY_SUBMIT_REJECT);
	return 0;
}
