// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
// Bounded native execution diagnostic for the above-RAL frame graph. The
// public receipt is native-handle-free; the runner consumes only public RAL.

#ifndef WIRED_RAL_FRAME_GRAPH_NATIVE_H
#define WIRED_RAL_FRAME_GRAPH_NATIVE_H

#include "ral_frame_graph_submit.h"
#include "ral_frame_graph_transient.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RAL_FRAME_GRAPH_NATIVE_SCHEMA_VERSION 1u
#define RAL_FRAME_GRAPH_NATIVE_TEXTURE_COUNT 3u
#define RAL_FRAME_GRAPH_NATIVE_PASS_COUNT 6u

typedef struct {
	uint32_t schemaVersion;
	uintptr_t backendIdentity;
	uint64_t generation;
	uint64_t graphGeneration;
	uint64_t materializationGeneration;
	uint64_t batchGeneration;
	uint64_t recordingGeneration;
	uint64_t submissionGeneration;
	uint64_t nativeSubmissionGeneration;
	uint32_t textureCount;
	uint32_t allocationCount;
	uint32_t passCount;
	uint32_t submissionCount;
	uint64_t disjointEquivalentCommittedBytes;
	uint64_t physicalCommittedBytes;
	uint64_t savedBytes;
	uint32_t savedPermille;
	uint64_t timelineBaseValue;
	uint64_t timelineFinalValue;
	qboolean timelineCompleted;
	qboolean retired;
	qboolean ready;
} ralFrameGraphNativeReceipt_t;

qboolean RalFrameGraphNative_Run( ralBackend_t *backend, uint64_t generation,
	ralFrameGraphNativeReceipt_t *outReceipt );
qboolean RalFrameGraphNative_ReceiptExact(
	const ralFrameGraphNativeReceipt_t *a,
	const ralFrameGraphNativeReceipt_t *b );

#ifdef __cplusplus
}
#endif

#endif // WIRED_RAL_FRAME_GRAPH_NATIVE_H
