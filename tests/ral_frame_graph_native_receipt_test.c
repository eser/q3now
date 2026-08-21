// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_frame_graph_native.h"

#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"CHECK %s:%d: %s\n",\
	__FILE__,__LINE__,#x); return 1; } } while (0)

static ralFrameGraphNativeReceipt_t Receipt( void ) {
	ralFrameGraphNativeReceipt_t receipt;
	memset(&receipt,0,sizeof(receipt));
	receipt.schemaVersion=RAL_FRAME_GRAPH_NATIVE_SCHEMA_VERSION;
	receipt.backendIdentity=(uintptr_t)0x1000u;
	receipt.generation=7u;receipt.graphGeneration=7u;
	receipt.materializationGeneration=7u;receipt.batchGeneration=1u;
	receipt.recordingGeneration=7u;receipt.submissionGeneration=7u;
	receipt.nativeSubmissionGeneration=11u;
	receipt.textureCount=RAL_FRAME_GRAPH_NATIVE_TEXTURE_COUNT;
	receipt.allocationCount=1u;receipt.passCount=RAL_FRAME_GRAPH_NATIVE_PASS_COUNT;
	receipt.submissionCount=1u;
	receipt.disjointEquivalentCommittedBytes=49152u;
	receipt.physicalCommittedBytes=16384u;receipt.savedBytes=32768u;
	receipt.savedPermille=666u;
	receipt.timelineBaseValue=3u;receipt.timelineFinalValue=4u;
	receipt.timelineCompleted=qtrue;receipt.retired=qtrue;receipt.ready=qtrue;
	return receipt;
}

int main( void ) {
	ralFrameGraphNativeReceipt_t receipt=Receipt(),mutated;
	CHECK(RalFrameGraphNative_ReceiptExact(&receipt,&receipt));
#define REJECT(statement) do { mutated=receipt;statement; \
	CHECK(!RalFrameGraphNative_ReceiptExact(&mutated,&mutated)); } while (0)
	REJECT(mutated.backendIdentity=0u);
	REJECT(mutated.generation++);
	REJECT(mutated.graphGeneration++);
	REJECT(mutated.materializationGeneration++);
	REJECT(mutated.batchGeneration=0u);
	REJECT(mutated.recordingGeneration++);
	REJECT(mutated.submissionGeneration++);
	REJECT(mutated.nativeSubmissionGeneration=0u);
	REJECT(mutated.textureCount--);
	REJECT(mutated.allocationCount++);
	REJECT(mutated.passCount--);
	REJECT(mutated.submissionCount++);
	REJECT(mutated.disjointEquivalentCommittedBytes++);
	REJECT(mutated.physicalCommittedBytes++);
	REJECT(mutated.savedBytes++);
	REJECT(mutated.savedPermille++);
	REJECT(mutated.timelineFinalValue++);
	REJECT(mutated.timelineCompleted=qfalse);
	REJECT(mutated.retired=qfalse);
	REJECT(mutated.ready=qfalse);
#undef REJECT
	puts("ral frame graph native receipt: PASS");return 0;
}
