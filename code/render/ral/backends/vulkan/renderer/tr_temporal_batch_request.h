// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#ifndef WIRED_TR_TEMPORAL_BATCH_REQUEST_H
#define WIRED_TR_TEMPORAL_BATCH_REQUEST_H

#include "../../../../../qcommon/q_shared.h"

#include <stdint.h>

typedef enum {
	TEMPORAL_BATCH_REQUEST_NONE = 0,
	TEMPORAL_BATCH_REQUEST_EXACT,
	TEMPORAL_BATCH_REQUEST_CONFLICT,
	TEMPORAL_BATCH_REQUEST_CONSUMED
} temporalBatchRequestState_t;

typedef struct {
	temporalBatchRequestState_t state;
	uint64_t token;
	int32_t worldIndex;
	uint64_t frameId;
	uint32_t width;
	uint32_t height;
	uint32_t topologyEpoch;
	uint32_t planGeneration;
	uint8_t enabled;
} temporalBatchRequest_t;

void R_TemporalBatchRequestReset( temporalBatchRequest_t *request,
	uint64_t token );

// Publishes one pointer-free request into the current command batch. Repeating
// the exact authored tuple is idempotent; any distinct tuple makes conflict
// sticky. A consumed batch cannot be republished.
qboolean R_TemporalBatchRequestPublish( temporalBatchRequest_t *request,
	const temporalBatchRequest_t *candidate );
qboolean R_TemporalBatchRequestValidateExact(
	const temporalBatchRequest_t *request );

// Snapshots NONE/EXACT/CONFLICT once and marks the source CONSUMED. Rejection
// leaves both source and output byte-identical.
qboolean R_TemporalBatchRequestConsume( temporalBatchRequest_t *request,
	temporalBatchRequest_t *outRequest );

#endif
