// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#include "ral_transient_texture.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"CHECK %s:%d: %s\n",__FILE__,__LINE__,#x); return 1; } } while (0)

typedef struct { uint32_t role,index;uintptr_t ownerIdentity; } FakeObject;
typedef struct {
	uint32_t failCreate,failAllocate,failBind;
	qboolean failReceipt,incompatibleTexture;
	uint32_t createCount,allocateCount,bindCount,destroyTextureCount,destroyAllocationCount;
	uint32_t retireCount;char retireOrder[16];
	qboolean duplicateTexture,duplicateAllocation,crossRoleAllocation,rejectCandidate;
	FakeObject *textures[4];FakeObject *allocations[4];FakeObject borrowed;
} FakeContext;

static qboolean CreateUnbound( void *opaque,const ralTransientTextureRequest_t *request,
		uint32_t index,ralTexture_t **out,ralTransientTextureCandidateFacts_t *facts ) {
	FakeContext *c=(FakeContext*)opaque;FakeObject *object;
	(void)request;c->createCount++;
	if(c->failCreate==c->createCount)return qfalse;
	if(c->rejectCandidate){*out=(ralTexture_t*)&c->borrowed;facts->size=1024u;facts->alignment=256u;facts->compatibilityKey=7u;return qtrue;}
	if(c->duplicateTexture&&index==1u){*out=(ralTexture_t*)c->textures[0];facts->size=1024u;facts->alignment=256u;facts->compatibilityKey=7u;return qtrue;}
	object=(FakeObject*)malloc(sizeof(*object));if(!object)return qfalse;
	object->role=1u;object->index=index;c->textures[index]=object;*out=(ralTexture_t*)object;
	facts->size=1024u;facts->alignment=256u;
	facts->compatibilityKey=c->incompatibleTexture&&index==1u?8u:7u;return qtrue;
}

static qboolean AllocateSlot( void *opaque,const ralTexture_t *representative,
		uint32_t slot,const ralTransientSlot_t *facts,void **out ) {
	FakeContext *c=(FakeContext*)opaque;FakeObject *object;
	(void)facts;c->allocateCount++;
	if(c->failAllocate==c->allocateCount)return qfalse;
	if(c->crossRoleAllocation){*out=(void*)representative;return qtrue;}
	if(c->duplicateAllocation&&slot==1u){*out=c->allocations[0];return qtrue;}
	object=(FakeObject*)malloc(sizeof(*object));if(!object)return qfalse;
	object->role=2u;object->index=slot;object->ownerIdentity=(uintptr_t)representative;
	c->allocations[slot]=object;*out=object;return qtrue;
}

static qboolean BindComplete( void *opaque,ralTexture_t *texture,void *allocation,
		uint32_t requestIndex,uint32_t slotIndex ) {
	FakeContext *c=(FakeContext*)opaque;(void)texture;(void)allocation;(void)requestIndex;(void)slotIndex;
	c->bindCount++;return c->failBind==c->bindCount?qfalse:qtrue;
}
static qboolean Allowed( void *opaque,ralTransientPhysicalRole_t role,uintptr_t identity ) {
	FakeContext *c=(FakeContext*)opaque;(void)role;
	return identity==(uintptr_t)&c->borrowed?qfalse:qtrue;
}
static qboolean GetAllocationReceipt( void *opaque,const void *allocation,
		ralAllocationReceipt_t *out ) {
	const FakeContext *c=(const FakeContext*)opaque;const FakeObject *object=(const FakeObject*)allocation;
	ralAllocationRequest_t request;ralAllocationFacts_t facts;
	if(c->failReceipt)return qfalse;
	memset(&request,0,sizeof(request));memset(&facts,0,sizeof(facts));
	request.memoryClass=RAL_ALLOCATION_TRANSIENT;request.residency=RAL_ALLOCATION_RESIDENCY_TRANSIENT;
	request.size=1024u;request.alignment=256u;request.ownerIdentity=object->ownerIdentity;
	request.ownerGeneration=object->index+1u;request.allowFallback=qtrue;
	facts.backendType=RAL_BACKEND_VULKAN;facts.placement=RAL_ALLOCATION_PLACEMENT_DEDICATED;
	facts.committedSize=1024u;facts.actualAlignment=256u;facts.allocationGeneration=object->index+1u;
	facts.deviceLocal=qtrue;facts.budgetKnown=qtrue;facts.budgetBytes=4096u;
	return Ral_AllocationReceiptBuild(&request,&facts,out);
}
static void DestroyTexture( void *opaque,ralTexture_t *texture ) { FakeContext *c=(FakeContext*)opaque;c->destroyTextureCount++;free(texture); }
static void DestroyAllocation( void *opaque,void *allocation ) { FakeContext *c=(FakeContext*)opaque;c->destroyAllocationCount++;free(allocation); }
static void RetireTexture( void *opaque,ralTexture_t *texture ) { FakeContext *c=(FakeContext*)opaque;c->retireOrder[c->retireCount++]='T';free(texture); }
static void RetireAllocation( void *opaque,void *allocation ) { FakeContext *c=(FakeContext*)opaque;c->retireOrder[c->retireCount++]='A';free(allocation); }

static ralTransientTextureOps_t Ops( void ) {
	ralTransientTextureOps_t ops;memset(&ops,0,sizeof(ops));
	ops.createUnbound=CreateUnbound;ops.allocateSlot=AllocateSlot;ops.bindComplete=BindComplete;
	ops.candidateAllowed=Allowed;ops.getAllocationReceipt=GetAllocationReceipt;
	ops.destroyTextureCandidate=DestroyTexture;
	ops.destroyAllocationCandidate=DestroyAllocation;ops.retireTexture=RetireTexture;
	ops.retireAllocation=RetireAllocation;return ops;
}

static void Fixture( ralTransientTextureCohortCreateInfo_t *ci,
		ralTransientTextureRequest_t *requests ) {
	uint32_t i;memset(ci,0,sizeof(*ci));memset(requests,0,sizeof(*requests)*3u);
	ci->policy.backendType=RAL_BACKEND_VULKAN;ci->policy.generation=4u;
	ci->policy.budgetBytes=4096u;ci->policy.explicitAliasing=qtrue;
	ci->requests=requests;ci->requestCount=3u;
	for(i=0u;i<3u;++i){requests[i].resourceIdentity=(uintptr_t)(100u+i);
		requests[i].resourceGeneration=i+1u;requests[i].firstPass=i*2u;
		requests[i].lastPass=i*2u+1u;requests[i].allowAlias=qtrue;
		requests[i].texture.type=RAL_TEXTURE_2D;requests[i].texture.format=RAL_FORMAT_R8G8B8A8_UNORM;
		requests[i].texture.width=64u;requests[i].texture.height=64u;
		requests[i].texture.depthOrArrayLayers=1u;requests[i].texture.mipLevels=1u;
		requests[i].texture.sampleCount=1u;requests[i].texture.usage=RAL_TEXTURE_USAGE_COLOR_ATTACHMENT;
		requests[i].texture.memory=RAL_MEMORY_LAZY_ALLOC;}
}

int main( void ) {
	ralTransientTextureCohortCreateInfo_t ci;
	ralTransientTextureRequest_t requests[3];
	ralTransientTextureOps_t ops=Ops();FakeContext context;
	ralTransientTextureCohort_t *cohort,*beforeCohort;
	ralTransientTextureCohortReceipt_t receipt,exact;
	ralTransientBatchReceipt_t planned,submitted,terminal,bad;
	uint32_t fail;
	Fixture(&ci,requests);memset(&context,0,sizeof(context));
	cohort=(ralTransientTextureCohort_t*)(uintptr_t)0x55u;beforeCohort=cohort;
	for(fail=1u;fail<=3u;++fail){memset(&context,0,sizeof(context));context.failCreate=fail;cohort=beforeCohort;
		CHECK(!Ral_TransientTextureCohortCreateWithOps(&ci,&ops,&context,&cohort));CHECK(cohort==beforeCohort);}
	memset(&context,0,sizeof(context));context.failAllocate=1u;cohort=beforeCohort;
	CHECK(!Ral_TransientTextureCohortCreateWithOps(&ci,&ops,&context,&cohort));CHECK(cohort==beforeCohort&&context.destroyTextureCount==3u);
	for(fail=1u;fail<=3u;++fail){memset(&context,0,sizeof(context));context.failBind=fail;cohort=beforeCohort;
		CHECK(!Ral_TransientTextureCohortCreateWithOps(&ci,&ops,&context,&cohort));CHECK(cohort==beforeCohort);}
	memset(&context,0,sizeof(context));context.rejectCandidate=qtrue;cohort=beforeCohort;
	CHECK(!Ral_TransientTextureCohortCreateWithOps(&ci,&ops,&context,&cohort));CHECK(context.destroyTextureCount==0u);
	memset(&context,0,sizeof(context));context.duplicateTexture=qtrue;cohort=beforeCohort;
	CHECK(!Ral_TransientTextureCohortCreateWithOps(&ci,&ops,&context,&cohort));CHECK(context.destroyTextureCount==1u);
	memset(&context,0,sizeof(context));context.crossRoleAllocation=qtrue;cohort=beforeCohort;
	CHECK(!Ral_TransientTextureCohortCreateWithOps(&ci,&ops,&context,&cohort));CHECK(context.destroyAllocationCount==0u);
	memset(&context,0,sizeof(context));context.failReceipt=qtrue;cohort=beforeCohort;
	CHECK(!Ral_TransientTextureCohortCreateWithOps(&ci,&ops,&context,&cohort));
	CHECK(cohort==beforeCohort&&context.destroyTextureCount==3u&&context.destroyAllocationCount==1u);
	ci.policy.budgetBytes=512u;memset(&context,0,sizeof(context));cohort=beforeCohort;
	CHECK(!Ral_TransientTextureCohortCreateWithOps(&ci,&ops,&context,&cohort));
	CHECK(cohort==beforeCohort&&context.destroyTextureCount==3u&&context.allocateCount==0u);
	ci.policy.budgetBytes=4096u;
	requests[1].firstPass=1u;requests[1].lastPass=2u;
	memset(&context,0,sizeof(context));context.duplicateAllocation=qtrue;cohort=beforeCohort;
	CHECK(!Ral_TransientTextureCohortCreateWithOps(&ci,&ops,&context,&cohort));
	CHECK(cohort==beforeCohort&&context.destroyAllocationCount==1u);
	requests[1].firstPass=2u;requests[1].lastPass=3u;

	memset(&context,0,sizeof(context));cohort=NULL;
	CHECK(Ral_TransientTextureCohortCreateWithOps(&ci,&ops,&context,&cohort));CHECK(cohort);
	CHECK(context.createCount==3u&&context.allocateCount==1u&&context.bindCount==3u);
	CHECK(Ral_TransientTextureCohortGetTexture(cohort,0u)!=NULL);
	CHECK(Ral_TransientTextureCohortGetTexture(cohort,3u)==NULL);
	CHECK(Ral_TransientTextureCohortGetReceipt(cohort,&receipt));
	CHECK(receipt.textureCount==3u&&receipt.allocationCount==1u);
	exact=receipt;CHECK(Ral_TransientTextureCohortReceiptExact(&receipt,&exact));
	exact.plan.assignments[1].slotIndex=1u;CHECK(!Ral_TransientTextureCohortReceiptExact(&exact,&exact));
	exact=receipt;exact.textures[1].resourceGeneration++;CHECK(!Ral_TransientTextureCohortReceiptExact(&exact,&exact));
	exact=receipt;exact.textures[1].allocationIdentity=(uintptr_t)3u;CHECK(!Ral_TransientTextureCohortReceiptExact(&exact,&exact));
	exact=receipt;exact.allocations[0].allocation.allocationGeneration++;CHECK(!Ral_TransientTextureCohortReceiptExact(&exact,&exact));
	CHECK(Ral_TransientTextureCohortBegin(cohort,1u,&planned));
	CHECK(Ral_TransientTextureCohortSubmit(cohort,&planned,9u,&submitted));
	bad=submitted;bad.commandSlot=0u;CHECK(!Ral_TransientTextureCohortRetire(cohort,&bad,qtrue,&terminal));
	CHECK(!Ral_TransientTextureCohortRetire(cohort,&submitted,qfalse,&terminal));
	CHECK(Ral_TransientTextureCohortRetire(cohort,&submitted,qtrue,&terminal));
	bad=terminal;bad.batchGeneration++;CHECK(!Ral_TransientTextureCohortReleaseTerminal(&cohort,&bad));CHECK(cohort!=NULL);
	CHECK(Ral_TransientTextureCohortReleaseTerminal(&cohort,&terminal));CHECK(cohort==NULL);
	CHECK(!Ral_TransientTextureCohortReleaseTerminal(&cohort,&terminal));
	CHECK(context.retireCount==4u&&!memcmp(context.retireOrder,"TTTA",4u));

	memset(&context,0,sizeof(context));context.incompatibleTexture=qtrue;cohort=NULL;
	CHECK(Ral_TransientTextureCohortCreateWithOps(&ci,&ops,&context,&cohort));
	CHECK(context.allocateCount==2u);CHECK(Ral_TransientTextureCohortBegin(cohort,0u,&planned));
	CHECK(Ral_TransientTextureCohortCancel(cohort,&planned,&terminal));
	CHECK(Ral_TransientTextureCohortReleaseTerminal(&cohort,&terminal));
	CHECK(context.retireCount==5u&&!memcmp(context.retireOrder,"TTTAA",5u));

	memset(&context,0,sizeof(context));cohort=NULL;
	CHECK(Ral_TransientTextureCohortCreateWithOps(&ci,&ops,&context,&cohort));
	CHECK(Ral_TransientTextureCohortReleaseFresh(&cohort));CHECK(cohort==NULL);
	CHECK(context.destroyTextureCount==3u&&context.destroyAllocationCount==1u);
	CHECK(!Ral_TransientTextureCohortReleaseFresh(&cohort));

	memset(&context,0,sizeof(context));cohort=NULL;
	CHECK(Ral_TransientTextureCohortCreateWithOps(&ci,&ops,&context,&cohort));
	CHECK(Ral_TransientTextureCohortBegin(cohort,0u,&planned));
	CHECK(!Ral_TransientTextureCohortReleaseFresh(&cohort));CHECK(cohort!=NULL);
	CHECK(Ral_TransientTextureCohortCancel(cohort,&planned,&terminal));
	CHECK(Ral_TransientTextureCohortReleaseTerminal(&cohort,&terminal));CHECK(cohort==NULL);
	puts("ral transient texture cohort: PASS");return 0;
}
