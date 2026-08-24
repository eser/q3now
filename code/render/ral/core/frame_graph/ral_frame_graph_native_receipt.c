// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_frame_graph_native.h"

#include <limits.h>

static qboolean GenerationValid( uint64_t value ) {
	return value != 0u && value != UINT64_MAX ? qtrue : qfalse;
}

static uint32_t RatioPermille( uint64_t value, uint64_t total ) {
	uint64_t remainder=0u;
	uint32_t result=0u,i;
	if ( total==0u || value>total ) return UINT32_MAX;
	for ( i=0u; i<1000u; ++i ) {
		if ( remainder>=total-value ) {
			remainder-=total-value;
			++result;
		} else remainder+=value;
	}
	return result;
}

static qboolean ReceiptValid( const ralFrameGraphNativeReceipt_t *receipt ) {
	if ( !receipt
			|| receipt->schemaVersion!=RAL_FRAME_GRAPH_NATIVE_SCHEMA_VERSION
			|| receipt->backendIdentity==(uintptr_t)0
			|| !GenerationValid(receipt->generation)
			|| receipt->graphGeneration!=receipt->generation
			|| receipt->materializationGeneration!=receipt->generation
			|| !GenerationValid(receipt->batchGeneration)
			|| receipt->recordingGeneration!=receipt->generation
			|| receipt->submissionGeneration!=receipt->generation
			|| !GenerationValid(receipt->nativeSubmissionGeneration)
			|| receipt->textureCount!=RAL_FRAME_GRAPH_NATIVE_TEXTURE_COUNT
			|| receipt->allocationCount!=1u
			|| receipt->passCount!=RAL_FRAME_GRAPH_NATIVE_PASS_COUNT
			|| receipt->submissionCount!=1u
			|| receipt->physicalCommittedBytes==0u
			|| receipt->disjointEquivalentCommittedBytes
				<=receipt->physicalCommittedBytes
			|| receipt->savedBytes!=receipt->disjointEquivalentCommittedBytes
				-receipt->physicalCommittedBytes
			|| receipt->savedPermille!=RatioPermille(receipt->savedBytes,
				receipt->disjointEquivalentCommittedBytes)
			|| receipt->savedPermille==0u || receipt->savedPermille>1000u
			|| receipt->timelineBaseValue==UINT64_MAX
			|| receipt->timelineFinalValue==UINT64_MAX
			|| receipt->timelineFinalValue!=receipt->timelineBaseValue+1u
			|| receipt->timelineCompleted!=qtrue || receipt->retired!=qtrue
			|| receipt->ready!=qtrue ) return qfalse;
	return qtrue;
}

qboolean RalFrameGraphNative_ReceiptExact(
		const ralFrameGraphNativeReceipt_t *a,
		const ralFrameGraphNativeReceipt_t *b ) {
	return ReceiptValid(a) && ReceiptValid(b)
		&& a->schemaVersion==b->schemaVersion
		&& a->backendIdentity==b->backendIdentity
		&& a->generation==b->generation
		&& a->graphGeneration==b->graphGeneration
		&& a->materializationGeneration==b->materializationGeneration
		&& a->batchGeneration==b->batchGeneration
		&& a->recordingGeneration==b->recordingGeneration
		&& a->submissionGeneration==b->submissionGeneration
		&& a->nativeSubmissionGeneration==b->nativeSubmissionGeneration
		&& a->textureCount==b->textureCount
		&& a->allocationCount==b->allocationCount
		&& a->passCount==b->passCount
		&& a->submissionCount==b->submissionCount
		&& a->disjointEquivalentCommittedBytes
			==b->disjointEquivalentCommittedBytes
		&& a->physicalCommittedBytes==b->physicalCommittedBytes
		&& a->savedBytes==b->savedBytes
		&& a->savedPermille==b->savedPermille
		&& a->timelineBaseValue==b->timelineBaseValue
		&& a->timelineFinalValue==b->timelineFinalValue
		&& a->timelineCompleted==b->timelineCompleted
		&& a->retired==b->retired && a->ready==b->ready ? qtrue:qfalse;
}
