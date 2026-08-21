// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#include "ral_suballocation.h"

#include <stdio.h>
#include <string.h>

#define CHECK(x) do{if(!(x)){fprintf(stderr,"CHECK %s:%d: %s\n",__FILE__,__LINE__,#x);return 1;}}while(0)

int main(void){
	ralSuballocator_t allocator,before;
	ralSuballocationReceipt_t a,b,c,d,bad,outBefore;
	ralSuballocationStats_t stats;
	memset(&allocator,0x5a,sizeof(allocator));before=allocator;
	CHECK(!Ral_SuballocatorInit(&allocator,RAL_BACKEND_VULKAN,0,1u,1024u));
	CHECK(!memcmp(&allocator,&before,sizeof(allocator)));
	CHECK(Ral_SuballocatorInit(&allocator,RAL_BACKEND_VULKAN,(uintptr_t)0x1000u,1u,1024u));
	CHECK(Ral_SuballocatorGetStats(&allocator,&stats));CHECK(stats.empty&&stats.freeBytes==1024u&&stats.largestFreeRange==1024u);
	CHECK(Ral_SuballocatorAllocate(&allocator,(uintptr_t)0x2000u,1u,200u,64u,&a));
	CHECK(a.offset==0u&&a.committedSize==256u&&a.placement==RAL_ALLOCATION_PLACEMENT_SUBALLOCATED);
	CHECK(Ral_SuballocatorAllocate(&allocator,(uintptr_t)0x3000u,1u,100u,64u,&b));
	CHECK(b.offset==256u&&b.committedSize==128u);
	CHECK(Ral_SuballocatorAllocate(&allocator,(uintptr_t)0x4000u,1u,300u,64u,&c));
	CHECK(c.offset==384u&&c.committedSize==320u);
	CHECK(Ral_SuballocatorFree(&allocator,&b));
	CHECK(Ral_SuballocatorGetStats(&allocator,&stats));CHECK(stats.freeBytes==448u&&stats.freeRangeCount==2u&&stats.largestFreeRange==320u);
	CHECK(Ral_SuballocatorAllocate(&allocator,(uintptr_t)0x5000u,1u,96u,32u,&d));
	CHECK(d.offset==256u&&d.committedSize==96u);
	CHECK(!Ral_SuballocatorFree(&allocator,&b));
	bad=d;bad.ownerGeneration++;CHECK(!Ral_SuballocatorFree(&allocator,&bad));
	CHECK(Ral_SuballocatorFree(&allocator,&a));CHECK(Ral_SuballocatorFree(&allocator,&c));CHECK(Ral_SuballocatorFree(&allocator,&d));
	CHECK(Ral_SuballocatorGetStats(&allocator,&stats));CHECK(stats.empty&&stats.freeBytes==1024u&&stats.freeRangeCount==1u&&stats.largestFreeRange==1024u);
	CHECK(Ral_SuballocationReceiptExact(&a,&a));bad=a;bad.offset=64u;CHECK(!Ral_SuballocationReceiptExact(&a,&bad));
	bad=a;bad.placement=RAL_ALLOCATION_PLACEMENT_DEDICATED;CHECK(!Ral_SuballocationReceiptExact(&bad,&bad));

	CHECK(Ral_SuballocatorAllocate(&allocator,(uintptr_t)0x6000u,2u,1024u,1u,&a));
	memset(&outBefore,0x6b,sizeof(outBefore));bad=outBefore;
	CHECK(!Ral_SuballocatorAllocate(&allocator,(uintptr_t)0x7000u,2u,1u,1u,&bad));CHECK(!memcmp(&bad,&outBefore,sizeof(bad)));
	CHECK(Ral_SuballocatorFree(&allocator,&a));
	allocator.nextAllocationGeneration=UINT64_MAX-1u;bad=outBefore;
	CHECK(!Ral_SuballocatorAllocate(&allocator,(uintptr_t)0x8000u,2u,1u,1u,&bad));CHECK(!memcmp(&bad,&outBefore,sizeof(bad)));

	// Managed WebGPU placement is intentionally not forged as an explicit block.
	memset(&before,0x5a,sizeof(before));allocator=before;
	CHECK(!Ral_SuballocatorInit(&allocator,RAL_BACKEND_WEBGPU,0,1u,1024u));CHECK(!memcmp(&allocator,&before,sizeof(allocator)));
	puts("ral suballocation: PASS");return 0;
}
