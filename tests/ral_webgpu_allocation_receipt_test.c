// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#include "ral_allocation.h"

#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"CHECK %s:%d: %s\n",__FILE__,__LINE__,#x); return 1; } } while (0)

static qboolean Managed( ralAllocationClass_t memoryClass, qboolean hostVisible,
		ralAllocationReceipt_t *out ) {
	ralAllocationRequest_t request;
	ralAllocationFacts_t facts;
	memset(&request,0,sizeof(request));memset(&facts,0,sizeof(facts));
	request.memoryClass=memoryClass;request.residency=RAL_ALLOCATION_RESIDENCY_STREAMED;
	request.size=1024u;request.alignment=256u;request.ownerIdentity=(uintptr_t)0x2000u;
	request.ownerGeneration=9u;request.allowFallback=qtrue;
	facts.backendType=RAL_BACKEND_WEBGPU;facts.placement=RAL_ALLOCATION_PLACEMENT_MANAGED;
	facts.committedSize=1024u;facts.actualAlignment=256u;facts.allocationGeneration=11u;
	facts.deviceLocal=memoryClass==RAL_ALLOCATION_DEVICE_LOCAL?qtrue:qfalse;
	facts.hostVisible=hostVisible;facts.hostCoherent=hostVisible;
	return Ral_AllocationReceiptBuild(&request,&facts,out);
}

int main( void ) {
	ralAllocationReceipt_t device, upload, readback;
	CHECK(Managed(RAL_ALLOCATION_DEVICE_LOCAL,qfalse,&device));
	CHECK(Managed(RAL_ALLOCATION_UPLOAD,qtrue,&upload));
	CHECK(Managed(RAL_ALLOCATION_READBACK,qtrue,&readback));
	CHECK(device.outcome==RAL_ALLOCATION_OUTCOME_EMULATED);
	CHECK(device.placement==RAL_ALLOCATION_PLACEMENT_MANAGED);
	CHECK(device.pressureAfter==RAL_ALLOCATION_PRESSURE_UNKNOWN&&!device.budgetKnown);
	CHECK(upload.outcome==RAL_ALLOCATION_OUTCOME_EMULATED);
	CHECK(readback.outcome==RAL_ALLOCATION_OUTCOME_EMULATED);
	CHECK(!Ral_AllocationReceiptExact(&device,&upload));
	puts("ral WebGPU allocation receipt: PASS");
	return 0;
}
