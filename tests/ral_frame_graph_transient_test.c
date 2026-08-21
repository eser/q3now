// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_frame_graph_transient.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"CHECK %s:%d: %s\n",__FILE__,__LINE__,#x); return 1; } } while (0)

typedef struct { uint32_t role,index;uintptr_t ownerIdentity; } FakeObject;
typedef struct {
	ralBackendType_t backendType;
	uint64_t committedSize;
	uint64_t compatibilityKey;
	uint32_t createCount,allocateCount,bindCount;
	uint32_t destroyTextureCount,destroyAllocationCount;
	uint32_t retireTextureCount,retireAllocationCount;
	qboolean failOuterReceipt,returnBorrowedCohort;
	FakeObject *textures[4],*allocations[4];
	FakeObject borrowed;
} FakeContext;

static qboolean PhysicalCreate( void *opaque,
		const ralTransientTextureRequest_t *request,uint32_t index,
		ralTexture_t **out,ralTransientTextureCandidateFacts_t *facts ) {
	FakeContext *context=(FakeContext*)opaque;FakeObject *object;
	(void)request;object=(FakeObject*)malloc(sizeof(*object));if(!object)return qfalse;
	object->role=1u;object->index=index;object->ownerIdentity=0u;
	context->textures[index]=object;context->createCount++;*out=(ralTexture_t*)object;
	facts->size=1024u;facts->alignment=256u;
	facts->compatibilityKey=context->compatibilityKey;return qtrue;
}

static qboolean PhysicalAllocate( void *opaque,const ralTexture_t *representative,
		uint32_t slot,const ralTransientSlot_t *facts,void **out ) {
	FakeContext *context=(FakeContext*)opaque;FakeObject *object;
	(void)facts;object=(FakeObject*)malloc(sizeof(*object));if(!object)return qfalse;
	object->role=2u;object->index=slot;object->ownerIdentity=(uintptr_t)representative;
	context->allocations[slot]=object;context->allocateCount++;*out=object;return qtrue;
}

static qboolean PhysicalBind( void *opaque,ralTexture_t *texture,void *allocation,
		uint32_t requestIndex,uint32_t slotIndex ) {
	FakeContext *context=(FakeContext*)opaque;(void)texture;(void)allocation;
	(void)requestIndex;(void)slotIndex;context->bindCount++;return qtrue;
}

static qboolean PhysicalAllowed( void *opaque,ralTransientPhysicalRole_t role,
		uintptr_t identity ) {
	FakeContext *context=(FakeContext*)opaque;(void)role;
	return identity && identity!=(uintptr_t)&context->borrowed ? qtrue:qfalse;
}

static qboolean PhysicalReceipt( void *opaque,const void *allocation,
		ralAllocationReceipt_t *out ) {
	FakeContext *context=(FakeContext*)opaque;const FakeObject *object=(const FakeObject*)allocation;
	ralAllocationRequest_t request;ralAllocationFacts_t facts;
	memset(&request,0,sizeof(request));memset(&facts,0,sizeof(facts));
	request.memoryClass=RAL_ALLOCATION_TRANSIENT;
	request.residency=RAL_ALLOCATION_RESIDENCY_TRANSIENT;
	request.size=1024u;request.alignment=256u;
	request.ownerIdentity=object->ownerIdentity;request.ownerGeneration=object->index+1u;
	request.allowFallback=qtrue;facts.backendType=context->backendType;
	facts.placement=RAL_ALLOCATION_PLACEMENT_DEDICATED;
	facts.committedSize=context->committedSize;facts.actualAlignment=256u;
	facts.allocationGeneration=object->index+1u;facts.deviceLocal=qtrue;
	facts.budgetKnown=qtrue;facts.budgetBytes=16384u;
	return Ral_AllocationReceiptBuild(&request,&facts,out);
}

static void DestroyTexture( void *opaque,ralTexture_t *texture ) {
	FakeContext *context=(FakeContext*)opaque;context->destroyTextureCount++;free(texture);
}
static void DestroyAllocation( void *opaque,void *allocation ) {
	FakeContext *context=(FakeContext*)opaque;context->destroyAllocationCount++;free(allocation);
}
static void RetireTexture( void *opaque,ralTexture_t *texture ) {
	FakeContext *context=(FakeContext*)opaque;context->retireTextureCount++;free(texture);
}
static void RetireAllocation( void *opaque,void *allocation ) {
	FakeContext *context=(FakeContext*)opaque;context->retireAllocationCount++;free(allocation);
}

static ralTransientTextureOps_t PhysicalOps( void ) {
	ralTransientTextureOps_t ops;memset(&ops,0,sizeof(ops));
	ops.createUnbound=PhysicalCreate;ops.allocateSlot=PhysicalAllocate;
	ops.bindComplete=PhysicalBind;ops.candidateAllowed=PhysicalAllowed;
	ops.getAllocationReceipt=PhysicalReceipt;
	ops.destroyTextureCandidate=DestroyTexture;
	ops.destroyAllocationCandidate=DestroyAllocation;
	ops.retireTexture=RetireTexture;ops.retireAllocation=RetireAllocation;
	return ops;
}

// Host substitute for the backend factory used by the real public-RAL adapter.
ralTransientTextureCohort_t *Ral_CreateTransientTextureCohort(
		ralBackend_t *backend,const ralTransientTextureCohortCreateInfo_t *ci ) {
	FakeContext *context=(FakeContext*)backend;ralTransientTextureOps_t ops=PhysicalOps();
	ralTransientTextureCohort_t *cohort=NULL;
	return Ral_TransientTextureCohortCreateWithOps(ci,&ops,context,&cohort)
		?cohort:NULL;
}

static qboolean CreateCohort( void *opaque,
		const ralTransientTextureCohortCreateInfo_t *ci,
		ralTransientTextureCohort_t **out ) {
	FakeContext *context=(FakeContext*)opaque;ralTransientTextureOps_t ops=PhysicalOps();
	if(context->returnBorrowedCohort){*out=(ralTransientTextureCohort_t*)&context->borrowed;return qtrue;}
	return Ral_TransientTextureCohortCreateWithOps(ci,&ops,context,out);
}
static qboolean OuterAllowed( void *opaque,ralFrameGraphTransientPhysicalRole_t role,
		uintptr_t identity ) {
	FakeContext *context=(FakeContext*)opaque;
	return role==RAL_FRAME_GRAPH_TRANSIENT_PHYSICAL_COHORT
		&& identity!=(uintptr_t)&context->borrowed ? qtrue:qfalse;
}
static qboolean OuterReceipt( void *opaque,const ralTransientTextureCohort_t *cohort,
		ralTransientTextureCohortReceipt_t *out ) {
	FakeContext *context=(FakeContext*)opaque;
	return !context->failOuterReceipt
		&& Ral_TransientTextureCohortGetReceipt(cohort,out);
}
static const ralTexture_t *OuterTexture( void *opaque,
		const ralTransientTextureCohort_t *cohort,uint32_t index ) {
	(void)opaque;return Ral_TransientTextureCohortGetTexture(cohort,index);
}
static qboolean OuterBegin( void *opaque,ralTransientTextureCohort_t *cohort,
		uint32_t slot,ralTransientBatchReceipt_t *out ) {
	(void)opaque;return Ral_TransientTextureCohortBegin(cohort,slot,out);
}
static qboolean OuterSubmit( void *opaque,ralTransientTextureCohort_t *cohort,
		const ralTransientBatchReceipt_t *planned,uint64_t generation,
		ralTransientBatchReceipt_t *out ) {
	(void)opaque;return Ral_TransientTextureCohortSubmit(cohort,planned,generation,out);
}
static qboolean OuterRetire( void *opaque,ralTransientTextureCohort_t *cohort,
		const ralTransientBatchReceipt_t *submitted,qboolean fence,
		ralTransientBatchReceipt_t *out ) {
	(void)opaque;return Ral_TransientTextureCohortRetire(cohort,submitted,fence,out);
}
static qboolean OuterCancel( void *opaque,ralTransientTextureCohort_t *cohort,
		const ralTransientBatchReceipt_t *planned,ralTransientBatchReceipt_t *out ) {
	(void)opaque;return Ral_TransientTextureCohortCancel(cohort,planned,out);
}
static qboolean OuterReleaseFresh( void *opaque,ralTransientTextureCohort_t **cohort ) {
	(void)opaque;return Ral_TransientTextureCohortReleaseFresh(cohort);
}
static qboolean OuterReleaseTerminal( void *opaque,ralTransientTextureCohort_t **cohort,
		const ralTransientBatchReceipt_t *terminal ) {
	(void)opaque;return Ral_TransientTextureCohortReleaseTerminal(cohort,terminal);
}
static ralFrameGraphTransientOps_t OuterOps( void ) {
	ralFrameGraphTransientOps_t ops;memset(&ops,0,sizeof(ops));
	ops.createCohort=CreateCohort;ops.candidateAllowed=OuterAllowed;
	ops.getReceipt=OuterReceipt;ops.getTexture=OuterTexture;ops.begin=OuterBegin;
	ops.submit=OuterSubmit;ops.retire=OuterRetire;ops.cancel=OuterCancel;
	ops.releaseFresh=OuterReleaseFresh;ops.releaseTerminal=OuterReleaseTerminal;
	return ops;
}

static ralResourceState_t State( ralResourceUsage_t usage ) {
	ralResourceState_t state;memset(&state,0,sizeof(state));state.usage=usage;
	state.shaderStages=(usage==RAL_RESOURCE_USAGE_SAMPLED_TEXTURE
		||usage==RAL_RESOURCE_USAGE_STORAGE_READ
		||usage==RAL_RESOURCE_USAGE_STORAGE_WRITE
		||usage==RAL_RESOURCE_USAGE_STORAGE_READ_WRITE)?RAL_STAGE_FRAGMENT:0u;
	return state;
}

static qboolean GraphWithFirstKind( ralBackendType_t backend,qboolean alias,
		ralFrameGraphResourceKind_t firstKind,ralFrameGraphPlan_t *out ) {
	ralFrameGraphDescription_t description;uint32_t i;
	memset(&description,0,sizeof(description));description.schemaVersion=1u;
	description.generation=9u;description.backendType=backend;
	description.resourceCount=3u;description.passCount=6u;description.useCount=6u;
	description.transientPolicy.backendType=backend;
	description.transientPolicy.generation=9u;
	description.transientPolicy.budgetBytes=16384u;
	description.transientPolicy.explicitAliasing=alias;
	for(i=0u;i<3u;++i){ralFrameGraphResource_t *resource=&description.resources[i];
		resource->id=i+1u;resource->generation=i+1u;resource->identity=(uintptr_t)(100u+i);
		resource->kind=i==0u?firstKind:RAL_FRAME_GRAPH_RESOURCE_TEXTURE;
		resource->lifetime=RAL_FRAME_GRAPH_RESOURCE_TRANSIENT;
		resource->compatibilityKey=7u;resource->size=1024u;resource->alignment=256u;
		resource->allowAlias=qtrue;resource->initialState=State(RAL_RESOURCE_USAGE_UNDEFINED);
		resource->initialQueue=RAL_QUEUE_GRAPHICS;}
	for(i=0u;i<6u;++i){description.passes[i].id=i+1u;
		description.passes[i].queue=RAL_QUEUE_GRAPHICS;
		description.uses[i].passId=i+1u;description.uses[i].resourceId=i/2u+1u;
		description.uses[i].access=(i&1u)?RAL_FRAME_GRAPH_ACCESS_READ:RAL_FRAME_GRAPH_ACCESS_WRITE;
		if(i<2u&&firstKind==RAL_FRAME_GRAPH_RESOURCE_BUFFER)
			description.uses[i].state=State((i&1u)?RAL_RESOURCE_USAGE_STORAGE_READ
				:RAL_RESOURCE_USAGE_STORAGE_WRITE);
		else description.uses[i].state=State((i&1u)?RAL_RESOURCE_USAGE_SAMPLED_TEXTURE
			:RAL_RESOURCE_USAGE_COLOR_ATTACHMENT);}
	return RalFrameGraph_Compile(&description,out);
}

static qboolean Graph( ralBackendType_t backend,qboolean alias,
		ralFrameGraphPlan_t *out ) {
	return GraphWithFirstKind(backend,alias,RAL_FRAME_GRAPH_RESOURCE_TEXTURE,out);
}

static void Requests( const ralFrameGraphPlan_t *plan,
		ralTransientTextureRequest_t *requests ) {
	uint32_t i;memset(requests,0,sizeof(*requests)*3u);
	for(i=0u;i<3u;++i){const ralTransientRequest_t *source=&plan->transientRequests[i];
		requests[i].resourceIdentity=source->resourceIdentity;
		requests[i].resourceGeneration=source->resourceGeneration;
		requests[i].firstPass=source->firstPass;requests[i].lastPass=source->lastPass;
		requests[i].allowAlias=source->allowAlias;
		requests[i].texture.type=RAL_TEXTURE_2D;
		requests[i].texture.format=RAL_FORMAT_R8G8B8A8_UNORM;
		requests[i].texture.width=64u;requests[i].texture.height=64u;
		requests[i].texture.depthOrArrayLayers=1u;requests[i].texture.mipLevels=1u;
		requests[i].texture.sampleCount=1u;
		requests[i].texture.usage=RAL_TEXTURE_USAGE_COLOR_ATTACHMENT
			|RAL_TEXTURE_USAGE_SAMPLED;requests[i].texture.memory=RAL_MEMORY_LAZY_ALLOC;}
}

static void CreateInfo( ralFrameGraphTransientCreateInfo_t *ci,
		const ralFrameGraphPlan_t *plan,
		const ralTransientTextureRequest_t *requests ) {
	memset(ci,0,sizeof(*ci));ci->schemaVersion=1u;ci->generation=11u;
	ci->graphPlan=plan;ci->textureRequests=requests;ci->textureRequestCount=3u;
}

static int AliasSuccessAndLifecycle( void ) {
	ralFrameGraphPlan_t plan,stale,bufferPlan;
	ralTransientTextureRequest_t requests[3],badRequests[3],bufferRequests[3];
	ralFrameGraphTransientCreateInfo_t ci;ralFrameGraphTransientOps_t ops=OuterOps();
	ralFrameGraphTransient_t *owner=NULL,*before=(ralFrameGraphTransient_t*)(uintptr_t)0x55u;
	ralFrameGraphTransientReceipt_t receipt,exact,beforeReceipt;
	ralFrameGraphResourceBinding_t bindings[3],beforeBindings[3];uint32_t count=77u;
	ralFrameGraphExecutionDescription_t exec;ralFrameGraphExecutionPlan_t execPlan;
	ralTransientBatchReceipt_t planned,submitted,terminal,bad;
	FakeContext context;memset(&context,0,sizeof(context));context.backendType=RAL_BACKEND_VULKAN;
	context.committedSize=1280u;context.compatibilityKey=7u;
	CHECK(Graph(RAL_BACKEND_VULKAN,qtrue,&plan));Requests(&plan,requests);CreateInfo(&ci,&plan,requests);
	owner=before;ci.textureRequestCount=2u;
	CHECK(!RalFrameGraphTransient_CreateWithOps(&ci,&ops,&context,&owner));CHECK(owner==before);
	ci.textureRequestCount=3u;memcpy(badRequests,requests,sizeof(requests));
	badRequests[1].resourceGeneration++;ci.textureRequests=badRequests;
	CHECK(!RalFrameGraphTransient_CreateWithOps(&ci,&ops,&context,&owner));CHECK(owner==before);
	memcpy(badRequests,requests,sizeof(requests));badRequests[1]=requests[0];
	ci.textureRequests=badRequests;
	CHECK(!RalFrameGraphTransient_CreateWithOps(&ci,&ops,&context,&owner));CHECK(owner==before);
	memcpy(badRequests,requests,sizeof(requests));badRequests[0]=requests[1];badRequests[1]=requests[0];
	ci.textureRequests=badRequests;
	CHECK(!RalFrameGraphTransient_CreateWithOps(&ci,&ops,&context,&owner));CHECK(owner==before);
	CHECK(GraphWithFirstKind(RAL_BACKEND_VULKAN,qtrue,
		RAL_FRAME_GRAPH_RESOURCE_BUFFER,&bufferPlan));Requests(&bufferPlan,bufferRequests);
	CreateInfo(&ci,&bufferPlan,bufferRequests);
	CHECK(!RalFrameGraphTransient_CreateWithOps(&ci,&ops,&context,&owner));CHECK(owner==before);
	CreateInfo(&ci,&plan,requests);
	ci.textureRequests=requests;context.returnBorrowedCohort=qtrue;
	CHECK(!RalFrameGraphTransient_CreateWithOps(&ci,&ops,&context,&owner));CHECK(owner==before);
	context.returnBorrowedCohort=qfalse;context.failOuterReceipt=qtrue;
	CHECK(!RalFrameGraphTransient_CreateWithOps(&ci,&ops,&context,&owner));CHECK(owner==before);
	CHECK(context.destroyTextureCount==3u&&context.destroyAllocationCount==1u);
	context.failOuterReceipt=qfalse;context.compatibilityKey=8u;
	CHECK(!RalFrameGraphTransient_CreateWithOps(&ci,&ops,&context,&owner));CHECK(owner==before);
	CHECK(context.destroyTextureCount==6u&&context.destroyAllocationCount==2u);
	context.compatibilityKey=7u;owner=NULL;
	CHECK(RalFrameGraphTransient_CreateWithOps(&ci,&ops,&context,&owner));CHECK(owner);
	CHECK(!RalFrameGraphTransient_ReleaseFresh(NULL));
	CHECK(RalFrameGraphTransient_ReleaseFresh(&owner));CHECK(!owner);
	CHECK(context.destroyTextureCount==9u&&context.destroyAllocationCount==3u);
	CHECK(RalFrameGraphTransient_CreateWithOps(&ci,&ops,&context,&owner));CHECK(owner);
	CHECK(RalFrameGraphTransient_GetReceipt(owner,&receipt));
	CHECK(receipt.bindingCount==3u&&receipt.physical.allocationCount==1u);
	CHECK(receipt.disjointEquivalentCommittedBytes==3840u);
	CHECK(receipt.physicalCommittedBytes==1280u&&receipt.savedBytes==2560u);
	CHECK(receipt.savedPermille==666u);
	exact=receipt;CHECK(RalFrameGraphTransient_ReceiptExact(&receipt,&exact));
	exact.savedBytes++;CHECK(!RalFrameGraphTransient_ReceiptExact(&exact,&exact));
	exact=receipt;exact.bindings[1].textureMipLevels=0u;
	CHECK(!RalFrameGraphTransient_ReceiptExact(&exact,&exact));
	exact=receipt;exact.physical.allocations[0].allocation.committedSize++;
	CHECK(!RalFrameGraphTransient_ReceiptExact(&exact,&exact));
	memset(bindings,0x5a,sizeof(bindings));memcpy(beforeBindings,bindings,sizeof(bindings));
	stale=plan;stale.description.generation++;
	CHECK(!RalFrameGraphTransient_GetExecutionBindings(owner,&stale,bindings,3u,&count));
	CHECK(count==77u&&!memcmp(bindings,beforeBindings,sizeof(bindings)));
	CHECK(!RalFrameGraphTransient_GetExecutionBindings(owner,&plan,bindings,2u,&count));
	CHECK(count==77u&&!memcmp(bindings,beforeBindings,sizeof(bindings)));
	CHECK(RalFrameGraphTransient_GetExecutionBindings(owner,&plan,bindings,3u,&count));
	CHECK(count==3u&&bindings[0].texture&&bindings[2].resourceId==3u);
	memset(&exec,0,sizeof(exec));exec.schemaVersion=1u;exec.generation=12u;
	exec.backendIdentity=(const ralBackend_t*)(uintptr_t)0x99u;exec.graphPlan=&plan;
	exec.physicalQueueForLogical[0]=RAL_QUEUE_GRAPHICS;
	exec.physicalQueueForLogical[1]=RAL_QUEUE_GRAPHICS;
	exec.physicalQueueForLogical[2]=RAL_QUEUE_GRAPHICS;
	memcpy(exec.bindings,bindings,count*sizeof(bindings[0]));exec.bindingCount=count;
	CHECK(RalFrameGraphExecution_Compile(&exec,&execPlan));
	beforeReceipt=receipt;CHECK(RalFrameGraphTransient_GetReceipt(owner,&receipt));
	CHECK(RalFrameGraphTransient_ReceiptExact(&beforeReceipt,&receipt));
	CHECK(RalFrameGraphTransient_Begin(owner,1u,&planned));
	CHECK(RalFrameGraphTransient_Submit(owner,&planned,21u,&submitted));
	bad=submitted;bad.commandSlot=0u;
	CHECK(!RalFrameGraphTransient_Retire(owner,&bad,qtrue,&terminal));
	CHECK(!RalFrameGraphTransient_Retire(owner,&submitted,qfalse,&terminal));
	CHECK(RalFrameGraphTransient_Retire(owner,&submitted,qtrue,&terminal));
	bad=terminal;bad.batchGeneration++;
	CHECK(!RalFrameGraphTransient_ReleaseTerminal(&owner,&bad));CHECK(owner);
	CHECK(RalFrameGraphTransient_ReleaseTerminal(&owner,&terminal));CHECK(!owner);
	CHECK(context.retireTextureCount==3u&&context.retireAllocationCount==1u);
	return 0;
}

static int ManagedDisjointAndCancel( void ) {
	ralFrameGraphPlan_t plan;ralTransientTextureRequest_t requests[3];
	ralFrameGraphTransientCreateInfo_t ci;ralFrameGraphTransientOps_t ops=OuterOps();
	ralFrameGraphTransient_t *owner=NULL;ralFrameGraphTransientReceipt_t receipt;
	ralTransientBatchReceipt_t planned,terminal;FakeContext context;
	memset(&context,0,sizeof(context));context.backendType=RAL_BACKEND_WEBGPU;
	context.committedSize=1280u;context.compatibilityKey=7u;
	CHECK(Graph(RAL_BACKEND_WEBGPU,qfalse,&plan));Requests(&plan,requests);CreateInfo(&ci,&plan,requests);
	CHECK(RalFrameGraphTransient_CreateWithOps(&ci,&ops,&context,&owner));
	CHECK(RalFrameGraphTransient_GetReceipt(owner,&receipt));
	CHECK(receipt.physical.plan.outcome==RAL_TRANSIENT_PLAN_MANAGED_DISJOINT);
	CHECK(receipt.physical.allocationCount==3u);
	CHECK(receipt.disjointEquivalentCommittedBytes==3840u);
	CHECK(receipt.physicalCommittedBytes==3840u&&receipt.savedBytes==0u
		&&receipt.savedPermille==0u);
	CHECK(RalFrameGraphTransient_Begin(owner,0u,&planned));
	CHECK(RalFrameGraphTransient_Cancel(owner,&planned,&terminal));
	CHECK(RalFrameGraphTransient_ReleaseTerminal(&owner,&terminal));CHECK(!owner);
	CHECK(context.retireTextureCount==3u&&context.retireAllocationCount==3u);
	return 0;
}

static int PublicRalAdapter( void ) {
	ralFrameGraphPlan_t plan;ralTransientTextureRequest_t requests[3];
	ralFrameGraphTransientCreateInfo_t ci;ralFrameGraphTransient_t *owner;
	ralFrameGraphTransientReceipt_t receipt;ralTransientBatchReceipt_t planned,terminal;
	FakeContext context;memset(&context,0,sizeof(context));
	context.backendType=RAL_BACKEND_VULKAN;context.committedSize=1280u;
	context.compatibilityKey=7u;CHECK(Graph(RAL_BACKEND_VULKAN,qtrue,&plan));
	Requests(&plan,requests);CreateInfo(&ci,&plan,requests);
	owner=RalFrameGraphTransient_Create((ralBackend_t*)&context,&ci);CHECK(owner);
	CHECK(RalFrameGraphTransient_GetReceipt(owner,&receipt));CHECK(receipt.savedBytes==2560u);
	CHECK(RalFrameGraphTransient_Begin(owner,0u,&planned));
	CHECK(RalFrameGraphTransient_Cancel(owner,&planned,&terminal));
	CHECK(RalFrameGraphTransient_ReleaseTerminal(&owner,&terminal));CHECK(!owner);
	return 0;
}

int main( void ) {
	CHECK(AliasSuccessAndLifecycle()==0);CHECK(ManagedDisjointAndCancel()==0);
	CHECK(PublicRalAdapter()==0);
	puts("ral frame graph transient: PASS");return 0;
}
