// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_readback.h"

#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if(!(x)){fprintf(stderr,"CHECK %s:%d: %s\n",__FILE__,__LINE__,#x);return 1;} } while(0)

typedef struct {
	int stagingToken;
	int submissionToken;
	unsigned char bytes[256];
	ralBufferMapLifecycle_t map;
	qboolean completed;
	qboolean managed;
	qboolean mapReady;
	int failAt;
	char retired[8];
	uint32_t retiredCount;
} Fake;

static ralBuffer_t *Staging( Fake *f ) { return (ralBuffer_t *)(void *)&f->stagingToken; }

static qboolean CreateStaging( void *opaque, uint64_t bytes, ralBuffer_t **out,
		ralAllocationReceipt_t *allocation ) {
	Fake *f=(Fake *)opaque;ralAllocationRequest_t request;ralAllocationFacts_t facts;
	if(f->failAt==1)return qfalse;memset(&request,0,sizeof(request));memset(&facts,0,sizeof(facts));
	request.memoryClass=RAL_ALLOCATION_READBACK;request.residency=RAL_ALLOCATION_RESIDENCY_STREAMED;
	request.size=bytes;request.alignment=16u;request.ownerIdentity=(uintptr_t)Staging(f);request.ownerGeneration=4u;
	request.allowFallback=f->managed;
	facts.backendType=f->managed?RAL_BACKEND_WEBGPU:RAL_BACKEND_VULKAN;
	facts.placement=f->managed?RAL_ALLOCATION_PLACEMENT_MANAGED:RAL_ALLOCATION_PLACEMENT_DEDICATED;
	facts.committedSize=bytes;facts.actualAlignment=16u;facts.allocationGeneration=5u;
	facts.hostVisible=qtrue;facts.hostCoherent=f->managed?qfalse:qtrue;
	if(!Ral_AllocationReceiptBuild(&request,&facts,allocation))return qfalse;
	*out=Staging(f);return qtrue;
}
static qboolean Submit( void *opaque,const ralTransferRequest_t *request,ralBuffer_t *staging,
		ralTransferOutcome_t *outcome,uintptr_t *identity,uint64_t *generation ) {
	Fake *f=(Fake *)opaque;if(f->failAt==2||staging!=Staging(f)||request->byteSize!=sizeof(f->bytes))return qfalse;
	*outcome=f->managed?RAL_TRANSFER_OUTCOME_MANAGED_ASYNC:RAL_TRANSFER_OUTCOME_NATIVE_ASYNC;
	*identity=(uintptr_t)&f->submissionToken;*generation=6u;return qtrue;
}
static qboolean Completed(void *opaque,uintptr_t identity,qboolean *out){Fake*f=(Fake*)opaque;if(identity!=(uintptr_t)&f->submissionToken)return qfalse;*out=f->completed;return qtrue;}
static qboolean Wait(void *opaque,uintptr_t identity){Fake*f=(Fake*)opaque;if(identity!=(uintptr_t)&f->submissionToken)return qfalse;f->completed=qtrue;return qtrue;}
static ralResult_t MapBegin(void *opaque,ralBuffer_t*buffer,const ralBufferMapRequest_t*request,ralBufferMapTicket_t*out){Fake*f=(Fake*)opaque;return Ral_BufferMapLifecyclePublishBegin(&f->map,buffer,sizeof(f->bytes),request,f->mapReady,f->mapReady?f->bytes:NULL,out);}
static ralResult_t MapPoll(void *opaque,ralBuffer_t*buffer,const ralBufferMapTicket_t*authority,ralBufferMapTicket_t*out){Fake*f=(Fake*)opaque;(void)buffer;if(!f->mapReady)return Ral_BufferMapLifecyclePoll(&f->map,authority,out);if(authority->status==RAL_BUFFER_MAP_PENDING)return Ral_BufferMapLifecyclePublishReady(&f->map,authority,f->bytes,out);return Ral_BufferMapLifecyclePoll(&f->map,authority,out);}
static ralResult_t Unmap(void *opaque,ralBuffer_t*buffer,const ralBufferMapTicket_t*ticket){Fake*f=(Fake*)opaque;(void)buffer;return Ral_BufferMapLifecycleUnmap(&f->map,ticket);}
static ralResult_t Cancel(void *opaque,ralBuffer_t*buffer,const ralBufferMapTicket_t*ticket){Fake*f=(Fake*)opaque;(void)buffer;return Ral_BufferMapLifecycleCancel(&f->map,ticket);}
static qboolean Allowed(void *opaque,ralReadbackOwnedRole_t role,uintptr_t identity){Fake*f=(Fake*)opaque;if(f->failAt==3&&role==RAL_READBACK_ROLE_STAGING)return qfalse;if(f->failAt==4&&role==RAL_READBACK_ROLE_SUBMISSION)return qfalse;return identity!=0;}
static void RetireStaging(void*opaque,ralBuffer_t*buffer){Fake*f=(Fake*)opaque;if(buffer==Staging(f))f->retired[f->retiredCount++]='S';}
static void RetireSubmission(void*opaque,uintptr_t identity){Fake*f=(Fake*)opaque;if(identity==(uintptr_t)&f->submissionToken)f->retired[f->retiredCount++]='F';}

static const ralReadbackOps_t ops={CreateStaging,Submit,Completed,Wait,MapBegin,MapPoll,Unmap,Cancel,Allowed,RetireStaging,RetireSubmission,NULL};

static ralReadbackCreateInfo_t Info(Fake*f){ralReadbackCreateInfo_t ci;memset(&ci,0,sizeof(ci));ci.transfer.backendType=f->managed?RAL_BACKEND_WEBGPU:RAL_BACKEND_VULKAN;ci.transfer.direction=RAL_TRANSFER_READBACK;ci.transfer.resourceKind=RAL_TRANSFER_TEXTURE;ci.transfer.resourceIdentity=(uintptr_t)0x1200u;ci.transfer.resourceGeneration=3u;ci.transfer.byteSize=sizeof(f->bytes);ci.transfer.byteBudget=sizeof(f->bytes);ci.transfer.width=8u;ci.transfer.height=1u;ci.transfer.depth=1u;ci.transfer.bytesPerRow=256u;ci.transfer.rowsPerImage=1u;ci.transfer.queue=RAL_QUEUE_GRAPHICS;ci.transferGeneration=5u;ci.context=f;ci.ops=&ops;return ci;}

static void Init(Fake*f,qboolean managed){memset(f,0,sizeof(*f));f->managed=managed;Ral_BufferMapLifecycleInit(&f->map);}

int main(void){
	int fail;
	Fake f;ralReadbackOwner_t*owner=NULL;ralReadbackCreateInfo_t ci;ralReadbackReceipt_t receipt,bad,before;ralBufferMapTicket_t ticket,ready,stale;
	Init(&f,qfalse);ci=Info(&f);CHECK(Ral_ReadbackCreate(&ci,&owner));CHECK(owner);CHECK(Ral_ReadbackGetReceipt(owner,&receipt));CHECK(!receipt.ready);CHECK(Ral_ReadbackMapBegin(owner,&ticket)==ralErrorInvalidArgument);CHECK(!Ral_ReadbackComplete(owner));f.completed=qtrue;CHECK(Ral_ReadbackComplete(owner));CHECK(!Ral_ReadbackComplete(owner));CHECK(Ral_ReadbackGetReceipt(owner,&receipt)&&receipt.ready);CHECK(Ral_ReadbackReceiptExact(&receipt,&receipt));bad=receipt;bad.transfer.request.resourceGeneration++;CHECK(!Ral_ReadbackReceiptExact(&receipt,&bad));bad=receipt;bad.stagingAllocation.allocationGeneration++;CHECK(!Ral_ReadbackReceiptExact(&receipt,&bad));bad=receipt;bad.submissionIdentity++;CHECK(!Ral_ReadbackReceiptExact(&receipt,&bad));
	f.mapReady=qtrue;CHECK(Ral_ReadbackMapBegin(owner,&ticket)==ralSuccess);CHECK(ticket.status==RAL_BUFFER_MAP_READY);CHECK(!Ral_ReadbackRelease(&owner));stale=ticket;stale.generation++;CHECK(Ral_ReadbackMapUnmap(owner,&stale)==ralErrorInvalidArgument);CHECK(Ral_ReadbackMapUnmap(owner,&ticket)==ralSuccess);CHECK(Ral_ReadbackRelease(&owner));CHECK(!owner);CHECK(f.retiredCount==2&&!memcmp(f.retired,"SF",2));CHECK(Ral_ReadbackRelease(&owner));

	Init(&f,qtrue);ci=Info(&f);CHECK(Ral_ReadbackCreate(&ci,&owner));f.completed=qtrue;CHECK(Ral_ReadbackComplete(owner));CHECK(Ral_ReadbackMapBegin(owner,&ticket)==ralSuccess&&ticket.status==RAL_BUFFER_MAP_PENDING);CHECK(Ral_ReadbackMapPoll(owner,&ticket,&ready)==ralSuccess&&ready.status==RAL_BUFFER_MAP_PENDING);f.mapReady=qtrue;CHECK(Ral_ReadbackMapPoll(owner,&ticket,&ready)==ralSuccess&&ready.status==RAL_BUFFER_MAP_READY);CHECK(Ral_ReadbackMapUnmap(owner,&ready)==ralSuccess);CHECK(Ral_ReadbackRelease(&owner));
	Init(&f,qfalse);ci=Info(&f);CHECK(Ral_ReadbackCreate(&ci,&owner));CHECK(Ral_ReadbackWait(owner));CHECK(f.completed);f.mapReady=qtrue;CHECK(Ral_ReadbackMapBegin(owner,&ticket)==ralSuccess);CHECK(Ral_ReadbackMapUnmap(owner,&ticket)==ralSuccess);CHECK(Ral_ReadbackRelease(&owner));
	Init(&f,qtrue);ci=Info(&f);CHECK(Ral_ReadbackCreate(&ci,&owner));f.completed=qtrue;CHECK(Ral_ReadbackComplete(owner));CHECK(Ral_ReadbackMapBegin(owner,&ticket)==ralSuccess);CHECK(Ral_ReadbackMapCancel(owner,&ticket)==ralSuccess);CHECK(Ral_ReadbackRelease(&owner));

	memset(&before,0x5a,sizeof(before));for(fail=1;fail<=4;fail++){Init(&f,qfalse);f.failAt=fail;ci=Info(&f);owner=NULL;CHECK(!Ral_ReadbackCreate(&ci,&owner));CHECK(!owner);} (void)before;
	puts("ral readback owner: PASS");return 0;
}
