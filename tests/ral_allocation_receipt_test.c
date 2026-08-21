// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#include "ral_allocation.h"

#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"CHECK %s:%d: %s\n",__FILE__,__LINE__,#x); return 1; } } while (0)

static void NativeDevice( ralAllocationRequest_t *request, ralAllocationFacts_t *facts ) {
	memset(request,0,sizeof(*request));
	memset(facts,0,sizeof(*facts));
	request->memoryClass=RAL_ALLOCATION_DEVICE_LOCAL;
	request->residency=RAL_ALLOCATION_RESIDENCY_PERMANENT;
	request->size=4096u;request->alignment=256u;
	request->ownerIdentity=(uintptr_t)0x1000u;request->ownerGeneration=3u;
	facts->backendType=RAL_BACKEND_VULKAN;
	facts->placement=RAL_ALLOCATION_PLACEMENT_DEDICATED;
	facts->committedSize=4096u;facts->actualAlignment=256u;
	facts->allocationGeneration=7u;facts->deviceLocal=qtrue;
	facts->budgetKnown=qtrue;facts->budgetBytes=16384u;facts->usedBytesBefore=4096u;
}

int main( void ) {
	ralAllocationRequest_t request;
	ralAllocationFacts_t facts;
	ralAllocationReceipt_t receipt, exact, before;
	NativeDevice(&request,&facts);
	CHECK(Ral_AllocationReceiptBuild(&request,&facts,&receipt));
	CHECK(receipt.schemaVersion==1u&&receipt.outcome==RAL_ALLOCATION_OUTCOME_NATIVE);
	CHECK(receipt.usedBytesAfter==8192u&&receipt.pressureAfter==RAL_ALLOCATION_PRESSURE_NORMAL);
	exact=receipt;CHECK(Ral_AllocationReceiptExact(&receipt,&exact));
	exact.ownerGeneration++;CHECK(!Ral_AllocationReceiptExact(&receipt,&exact));
	exact=receipt;exact.pressureAfter=RAL_ALLOCATION_PRESSURE_CRITICAL;
	CHECK(!Ral_AllocationReceiptExact(&exact,&exact));
	exact=receipt;exact.requestedSize=4095u;exact.committedSize=4095u;
	CHECK(!Ral_AllocationReceiptExact(&exact,&exact));

	NativeDevice(&request,&facts);
	request.memoryClass=RAL_ALLOCATION_UPLOAD;facts.deviceLocal=qfalse;
	facts.hostVisible=qtrue;facts.hostCoherent=qtrue;
	CHECK(Ral_AllocationReceiptBuild(&request,&facts,&receipt));
	CHECK(receipt.memoryClass==RAL_ALLOCATION_UPLOAD);
	NativeDevice(&request,&facts);
	request.memoryClass=RAL_ALLOCATION_READBACK;facts.deviceLocal=qfalse;facts.hostVisible=qtrue;
	CHECK(Ral_AllocationReceiptBuild(&request,&facts,&receipt));

	NativeDevice(&request,&facts);
	request.memoryClass=RAL_ALLOCATION_TRANSIENT;
	request.residency=RAL_ALLOCATION_RESIDENCY_TRANSIENT;request.allowFallback=qtrue;
	facts.lazy=qtrue;
	CHECK(Ral_AllocationReceiptBuild(&request,&facts,&receipt));
	CHECK(receipt.outcome==RAL_ALLOCATION_OUTCOME_NATIVE);
	facts.lazy=qfalse;
	CHECK(Ral_AllocationReceiptBuild(&request,&facts,&receipt));
	CHECK(receipt.outcome==RAL_ALLOCATION_OUTCOME_EMULATED);

#define REJECT() do { memset(&receipt,0x5a,sizeof(receipt));before=receipt; \
	CHECK(!Ral_AllocationReceiptBuild(&request,&facts,&receipt)); \
	CHECK(memcmp(&receipt,&before,sizeof(receipt))==0); } while(0)
	NativeDevice(&request,&facts);request.alignment=3u;REJECT();
	NativeDevice(&request,&facts);request.size=UINT64_MAX;REJECT();
	NativeDevice(&request,&facts);request.ownerGeneration=UINT64_MAX;REJECT();
	NativeDevice(&request,&facts);facts.allocationGeneration=0u;REJECT();
	NativeDevice(&request,&facts);facts.committedSize=2048u;REJECT();
	NativeDevice(&request,&facts);facts.usedBytesBefore=15000u;REJECT();
	NativeDevice(&request,&facts);facts.hostCoherent=qtrue;REJECT();
	NativeDevice(&request,&facts);request.memoryClass=RAL_ALLOCATION_UPLOAD;REJECT();
	NativeDevice(&request,&facts);facts.budgetKnown=qfalse;REJECT();
	NativeDevice(&request,&facts);request.allowFallback=(qboolean)2;REJECT();
	NativeDevice(&request,&facts);facts.hostVisible=(qboolean)2;REJECT();
	NativeDevice(&request,&facts);request.memoryClass=RAL_ALLOCATION_TRANSIENT;
	request.residency=RAL_ALLOCATION_RESIDENCY_TRANSIENT;facts.lazy=qfalse;REJECT();
#undef REJECT
	puts("ral allocation receipt: PASS");
	return 0;
}
