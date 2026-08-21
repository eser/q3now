// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#include "ral_memory_recovery.h"

#include <stdio.h>
#include <string.h>

#define CHECK(x) do{if(!(x)){fprintf(stderr,"CHECK %s:%d: %s\n",__FILE__,__LINE__,#x);return 1;}}while(0)

static ralMemoryFailureEvent_t Event( ralMemoryFailureCause_t cause,
		ralMemoryCriticality_t criticality ) {
	ralMemoryFailureEvent_t event;memset(&event,0,sizeof(event));
	event.backendType=RAL_BACKEND_VULKAN;event.cause=cause;
	event.memoryClass=criticality==RAL_MEMORY_CRITICALITY_OPTIONAL
		? RAL_ALLOCATION_TRANSIENT:RAL_ALLOCATION_DEVICE_LOCAL;
	event.residency=criticality==RAL_MEMORY_CRITICALITY_OPTIONAL
		? RAL_ALLOCATION_RESIDENCY_TRANSIENT:criticality==RAL_MEMORY_CRITICALITY_STREAMING
			? RAL_ALLOCATION_RESIDENCY_STREAMED:RAL_ALLOCATION_RESIDENCY_PERMANENT;
	event.criticality=criticality;event.requestedBytes=256u;
	event.budgetKnown=qtrue;event.budgetBytes=4096u;event.usedBytes=3072u;
	event.attempt=1u;event.maxAttempts=2u;return event;
}

int main(void){
	ralMemoryFailureEvent_t event;ralMemoryFailureReceipt_t receipt,bad,before;
	ralMemoryFailureLedger_t ledger;
	event=Event(RAL_MEMORY_FAILURE_DEVICE_OOM,RAL_MEMORY_CRITICALITY_REQUIRED);
	CHECK(Ral_MemoryFailureDecide(&event,1u,&receipt));
	CHECK(receipt.action==RAL_MEMORY_RECOVERY_RETRY_AFTER_DRAIN&&receipt.retryAllowed);
	event.reclaimableBytes=512u;CHECK(Ral_MemoryFailureDecide(&event,2u,&receipt));
	CHECK(receipt.action==RAL_MEMORY_RECOVERY_EVICT_AND_RETRY);
	event.attempt=2u;CHECK(Ral_MemoryFailureDecide(&event,3u,&receipt));
	CHECK(receipt.action==RAL_MEMORY_RECOVERY_FAIL&&!receipt.retryAllowed);
	event=Event(RAL_MEMORY_FAILURE_FRAGMENTED,RAL_MEMORY_CRITICALITY_OPTIONAL);
	CHECK(Ral_MemoryFailureDecide(&event,4u,&receipt));CHECK(receipt.action==RAL_MEMORY_RECOVERY_REDUCE_QUALITY);
	event=Event(RAL_MEMORY_FAILURE_DEVICE_OOM,RAL_MEMORY_CRITICALITY_STREAMING);
	CHECK(Ral_MemoryFailureDecide(&event,5u,&receipt));CHECK(receipt.action==RAL_MEMORY_RECOVERY_DEFER);
	event=Event(RAL_MEMORY_FAILURE_PRESSURE,RAL_MEMORY_CRITICALITY_STREAMING);
	CHECK(Ral_MemoryFailureDecide(&event,6u,&receipt));CHECK(receipt.action==RAL_MEMORY_RECOVERY_EVICT_AND_RETRY);
	event=Event(RAL_MEMORY_FAILURE_RESIZE,RAL_MEMORY_CRITICALITY_REQUIRED);event.liveParent=qtrue;
	CHECK(Ral_MemoryFailureDecide(&event,7u,&receipt));CHECK(receipt.action==RAL_MEMORY_RECOVERY_DEFER&&receipt.preserveLiveParent);
	event.liveParent=qfalse;CHECK(Ral_MemoryFailureDecide(&event,8u,&receipt));CHECK(receipt.action==RAL_MEMORY_RECOVERY_RETRY_AFTER_DRAIN);
	event=Event(RAL_MEMORY_FAILURE_DEVICE_LOST,RAL_MEMORY_CRITICALITY_REQUIRED);event.maxAttempts=1u;
	CHECK(Ral_MemoryFailureDecide(&event,9u,&receipt));CHECK(receipt.action==RAL_MEMORY_RECOVERY_RECREATE_BACKEND&&!receipt.retryAllowed);
	event=Event(RAL_MEMORY_FAILURE_NO_COMPATIBLE_TYPE,RAL_MEMORY_CRITICALITY_OPTIONAL);
	CHECK(Ral_MemoryFailureDecide(&event,10u,&receipt));CHECK(receipt.action==RAL_MEMORY_RECOVERY_REDUCE_QUALITY);
	event=Event(RAL_MEMORY_FAILURE_HOST_OOM,RAL_MEMORY_CRITICALITY_REQUIRED);
	CHECK(Ral_MemoryFailureDecide(&event,11u,&receipt));CHECK(receipt.action==RAL_MEMORY_RECOVERY_FAIL);

	// WebGPU may not expose a budget; its managed device-loss decision is still exact.
	event=Event(RAL_MEMORY_FAILURE_DEVICE_LOST,RAL_MEMORY_CRITICALITY_STREAMING);
	event.backendType=RAL_BACKEND_WEBGPU;event.budgetKnown=qfalse;event.budgetBytes=0;event.usedBytes=0;
	event.maxAttempts=1u;CHECK(Ral_MemoryFailureDecide(&event,12u,&receipt));
	CHECK(receipt.action==RAL_MEMORY_RECOVERY_RECREATE_BACKEND);
	CHECK(Ral_MemoryFailureReceiptExact(&receipt,&receipt));
	bad=receipt;bad.event.cause=RAL_MEMORY_FAILURE_PRESSURE;CHECK(!Ral_MemoryFailureReceiptExact(&receipt,&bad));
	bad=receipt;bad.failureGeneration++;CHECK(!Ral_MemoryFailureReceiptExact(&receipt,&bad));
	bad=receipt;bad.action=RAL_MEMORY_RECOVERY_DEFER;CHECK(!Ral_MemoryFailureReceiptExact(&bad,&bad));
	bad=receipt;bad.preserveLiveParent=qtrue;CHECK(!Ral_MemoryFailureReceiptExact(&bad,&bad));

	memset(&before,0x5a,sizeof(before));bad=before;event.attempt=0;
	CHECK(!Ral_MemoryFailureDecide(&event,13u,&bad));CHECK(!memcmp(&bad,&before,sizeof(bad)));
	event.attempt=1u;event.maxAttempts=0;bad=before;CHECK(!Ral_MemoryFailureDecide(&event,13u,&bad));CHECK(!memcmp(&bad,&before,sizeof(bad)));
	event.maxAttempts=1u;event.reclaimableBytes=1u;bad=before;CHECK(!Ral_MemoryFailureDecide(&event,UINT64_MAX,&bad));CHECK(!memcmp(&bad,&before,sizeof(bad)));

	Ral_MemoryFailureLedgerInit(&ledger);bad=before;CHECK(!Ral_MemoryFailureLedgerGet(&ledger,&bad));CHECK(!memcmp(&bad,&before,sizeof(bad)));
	event=Event(RAL_MEMORY_FAILURE_DEVICE_OOM,RAL_MEMORY_CRITICALITY_REQUIRED);
	CHECK(Ral_MemoryFailureLedgerPublish(&ledger,&event));CHECK(Ral_MemoryFailureLedgerGet(&ledger,&receipt));CHECK(receipt.failureGeneration==1u);
	event.cause=RAL_MEMORY_FAILURE_PRESSURE;CHECK(Ral_MemoryFailureLedgerPublish(&ledger,&event));CHECK(Ral_MemoryFailureLedgerGet(&ledger,&bad));CHECK(bad.failureGeneration==2u&&bad.action==RAL_MEMORY_RECOVERY_EVICT_AND_RETRY);
	ledger.nextGeneration=UINT64_MAX-1u;before=bad;CHECK(!Ral_MemoryFailureLedgerPublish(&ledger,&event));CHECK(Ral_MemoryFailureReceiptExact(&ledger.last,&before));
	puts("ral memory recovery: PASS");return 0;
}
