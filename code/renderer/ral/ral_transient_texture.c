// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#include "ral_transient_texture.h"

#include <stdlib.h>
#include <string.h>

struct ralTransientTextureCohort_s {
	ralTransientTextureOps_t ops;
	void *context;
	ralTexture_t *textures[RAL_TRANSIENT_MAX_REQUESTS];
	void *allocations[RAL_TRANSIENT_MAX_REQUESTS];
	ralTransientBatchLifecycle_t lifecycle;
	ralTransientTextureCohortReceipt_t receipt;
};

static qboolean BoolValid( qboolean value ) { return value==qfalse||value==qtrue; }

static qboolean OpsValid( const ralTransientTextureOps_t *ops ) {
	return ops&&ops->createUnbound&&ops->allocateSlot&&ops->bindComplete
		&&ops->candidateAllowed&&ops->getAllocationReceipt&&ops->destroyTextureCandidate
		&&ops->destroyAllocationCandidate&&ops->retireTexture&&ops->retireAllocation;
}

static qboolean TextureRequestValid( const ralTransientTextureRequest_t *request ) {
	const ralTextureCreateInfo_t *texture;
	if(!request||request->resourceIdentity==(uintptr_t)0
			||request->resourceGeneration==0u||request->resourceGeneration==UINT64_MAX
			||request->firstPass>request->lastPass||!BoolValid(request->allowAlias)) return qfalse;
	texture=&request->texture;
	return texture->type>=RAL_TEXTURE_1D&&texture->type<=RAL_TEXTURE_CUBE_ARRAY
		&&texture->format>RAL_FORMAT_UNDEFINED&&texture->format<RAL_FORMAT_COUNT
		&&texture->width>0u&&texture->height>0u&&texture->depthOrArrayLayers>0u
		&&(texture->sampleCount==0u||texture->sampleCount==1u||texture->sampleCount==2u
			||texture->sampleCount==4u||texture->sampleCount==8u)
		&&texture->usage!=0&&texture->memory==RAL_MEMORY_LAZY_ALLOC
		&&BoolValid(texture->concurrentGraphicsCompute)
		&&BoolValid(texture->concurrentGraphicsTransfer);
}

static void CleanupCandidates( ralTransientTextureCohort_t *cohort,
		uint32_t textureCount,uint32_t allocationCount ) {
	uint32_t i;
	for(i=textureCount;i>0u;--i)
		if(cohort->textures[i-1u]) cohort->ops.destroyTextureCandidate(cohort->context,cohort->textures[i-1u]);
	for(i=allocationCount;i>0u;--i)
		if(cohort->allocations[i-1u]) cohort->ops.destroyAllocationCandidate(cohort->context,cohort->allocations[i-1u]);
}

qboolean Ral_TransientTextureCohortCreateWithOps(
		const ralTransientTextureCohortCreateInfo_t *ci,
		const ralTransientTextureOps_t *ops,void *context,
		ralTransientTextureCohort_t **out ) {
	ralTransientTextureCohort_t *candidate;
	ralTransientRequest_t planRequests[RAL_TRANSIENT_MAX_REQUESTS];
	uint32_t i,j,textureCount=0u,allocationCount=0u;
	if(!ci||!out||!OpsValid(ops)||!ci->requests||ci->requestCount==0u
			||ci->requestCount>RAL_TRANSIENT_MAX_REQUESTS) return qfalse;
	candidate=(ralTransientTextureCohort_t*)malloc(sizeof(*candidate));
	if(!candidate) return qfalse;
	memset(candidate,0,sizeof(*candidate));candidate->ops=*ops;candidate->context=context;
	for(i=0u;i<ci->requestCount;++i) {
		ralTransientTextureCandidateFacts_t facts;
		ralTexture_t *texture=NULL;
		if(!TextureRequestValid(&ci->requests[i])) goto fail;
		memset(&facts,0,sizeof(facts));
		if(!ops->createUnbound(context,&ci->requests[i],i,&texture,&facts)||!texture
				||!ops->candidateAllowed(context,RAL_TRANSIENT_PHYSICAL_TEXTURE,(uintptr_t)texture)) goto fail;
		for(j=0u;j<textureCount;++j) if(candidate->textures[j]==texture) goto fail_borrowed_texture;
		candidate->textures[textureCount++]=texture;
		if(facts.size==0u||facts.alignment==0u||facts.compatibilityKey==0u) goto fail;
		memset(&planRequests[i],0,sizeof(planRequests[i]));
		planRequests[i].resourceIdentity=ci->requests[i].resourceIdentity;
		planRequests[i].resourceGeneration=ci->requests[i].resourceGeneration;
		planRequests[i].compatibilityKey=facts.compatibilityKey;
		planRequests[i].size=facts.size;planRequests[i].alignment=facts.alignment;
		planRequests[i].firstPass=ci->requests[i].firstPass;
		planRequests[i].lastPass=ci->requests[i].lastPass;
		planRequests[i].allowAlias=ci->requests[i].allowAlias;
		continue;
fail_borrowed_texture:
		texture=NULL;
		goto fail;
	}
	if(!Ral_TransientPlanBuild(&ci->policy,planRequests,ci->requestCount,&candidate->receipt.plan)) goto fail;
	for(i=0u;i<candidate->receipt.plan.slotCount;++i) {
		void *allocation=NULL;const ralTexture_t *representative=NULL;
		for(j=0u;j<ci->requestCount;++j)
			if(candidate->receipt.plan.assignments[j].slotIndex==i){representative=candidate->textures[j];break;}
		if(!representative||!ops->allocateSlot(context,representative,i,&candidate->receipt.plan.slots[i],&allocation)
				||!allocation||!ops->candidateAllowed(context,RAL_TRANSIENT_PHYSICAL_ALLOCATION,(uintptr_t)allocation)) goto fail;
		for(j=0u;j<textureCount;++j) if((uintptr_t)candidate->textures[j]==(uintptr_t)allocation) goto fail_borrowed_allocation;
		for(j=0u;j<allocationCount;++j) if(candidate->allocations[j]==allocation) goto fail_borrowed_allocation;
		candidate->allocations[allocationCount++]=allocation;
		continue;
fail_borrowed_allocation:
		allocation=NULL;
		goto fail;
	}
	for(i=0u;i<ci->requestCount;++i) {
		const uint32_t slot=candidate->receipt.plan.assignments[i].slotIndex;
		if(slot>=allocationCount||!ops->bindComplete(context,candidate->textures[i],
				candidate->allocations[slot],i,slot)) goto fail;
		candidate->receipt.textures[i].resourceIdentity=ci->requests[i].resourceIdentity;
		candidate->receipt.textures[i].resourceGeneration=ci->requests[i].resourceGeneration;
		candidate->receipt.textures[i].textureIdentity=(uintptr_t)candidate->textures[i];
		candidate->receipt.textures[i].allocationIdentity=(uintptr_t)candidate->allocations[slot];
		candidate->receipt.textures[i].slotIndex=slot;
	}
	for(i=0u;i<allocationCount;++i) {
		candidate->receipt.allocations[i].allocationIdentity=(uintptr_t)candidate->allocations[i];
		if(!ops->getAllocationReceipt(context,candidate->allocations[i],
				&candidate->receipt.allocations[i].allocation)) goto fail;
	}
	candidate->receipt.schemaVersion=RAL_TRANSIENT_TEXTURE_SCHEMA_VERSION;
	candidate->receipt.cohortIdentity=(uintptr_t)candidate;
	candidate->receipt.generation=ci->policy.generation;
	candidate->receipt.textureCount=ci->requestCount;
	candidate->receipt.allocationCount=allocationCount;
	candidate->receipt.ready=qtrue;
	Ral_TransientBatchLifecycleInit(&candidate->lifecycle);
	*out=candidate;return qtrue;
fail:
	CleanupCandidates(candidate,textureCount,allocationCount);free(candidate);return qfalse;
}

static qboolean ReceiptValid( const ralTransientTextureCohortReceipt_t *receipt ) {
	uint32_t i,j;
	if(!(receipt&&receipt->schemaVersion==RAL_TRANSIENT_TEXTURE_SCHEMA_VERSION
		&&receipt->cohortIdentity!=(uintptr_t)0&&receipt->generation!=0u
		&&receipt->generation!=UINT64_MAX&&receipt->textureCount>0u
		&&receipt->textureCount<=RAL_TRANSIENT_MAX_REQUESTS
		&&receipt->allocationCount>0u&&receipt->allocationCount<=receipt->textureCount
		&&receipt->allocationCount==receipt->plan.slotCount
		&&receipt->generation==receipt->plan.policy.generation
		&&receipt->ready==qtrue&&Ral_TransientPlanExact(&receipt->plan,&receipt->plan))) return qfalse;
	for(i=0u;i<receipt->allocationCount;++i) {
		const ralTransientTextureSlotReceipt_t *slot=&receipt->allocations[i];
		if(slot->allocationIdentity==(uintptr_t)0
				||!Ral_AllocationReceiptExact(&slot->allocation,&slot->allocation)
				||slot->allocation.backendType!=receipt->plan.policy.backendType
				||slot->allocation.memoryClass!=RAL_ALLOCATION_TRANSIENT
				||slot->allocation.residency!=RAL_ALLOCATION_RESIDENCY_TRANSIENT
				||slot->allocation.requestedSize!=receipt->plan.slots[i].committedSize
				||slot->allocation.committedSize<receipt->plan.slots[i].committedSize
				||slot->allocation.alignment!=receipt->plan.slots[i].alignment
				||slot->allocation.ownerGeneration!=slot->allocation.allocationGeneration) return qfalse;
		for(j=0u;j<i;++j) if(receipt->allocations[j].allocationIdentity==slot->allocationIdentity) return qfalse;
	}
	for(i=0u;i<receipt->textureCount;++i) {
		const ralTransientTextureBindingReceipt_t *binding=&receipt->textures[i];
		const ralTransientAssignment_t *assignment=&receipt->plan.assignments[i];
		if(binding->resourceIdentity!=assignment->request.resourceIdentity
				||binding->resourceGeneration!=assignment->request.resourceGeneration
				||binding->textureIdentity==(uintptr_t)0
				||binding->slotIndex!=assignment->slotIndex
				||binding->slotIndex>=receipt->allocationCount
				||binding->allocationIdentity!=receipt->allocations[binding->slotIndex].allocationIdentity)
			return qfalse;
		for(j=0u;j<i;++j) if(receipt->textures[j].textureIdentity==binding->textureIdentity) return qfalse;
		for(j=0u;j<receipt->allocationCount;++j)
			if(receipt->allocations[j].allocationIdentity==binding->textureIdentity) return qfalse;
	}
	for(i=0u;i<receipt->allocationCount;++i) {
		qboolean ownerFound=qfalse;
		for(j=0u;j<receipt->textureCount;++j)
			if(receipt->textures[j].slotIndex==i
					&&receipt->textures[j].textureIdentity==receipt->allocations[i].allocation.ownerIdentity) {
				ownerFound=qtrue;break;
			}
		if(!ownerFound) return qfalse;
	}
	return qtrue;
}

qboolean Ral_TransientTextureCohortReceiptExact(
		const ralTransientTextureCohortReceipt_t *a,
		const ralTransientTextureCohortReceipt_t *b ) {
	return ReceiptValid(a)&&ReceiptValid(b)&&a->cohortIdentity==b->cohortIdentity
		&&a->generation==b->generation&&a->textureCount==b->textureCount
		&&a->allocationCount==b->allocationCount&&Ral_TransientPlanExact(&a->plan,&b->plan)
		&&!memcmp(a->textures,b->textures,sizeof(a->textures))
		&&!memcmp(a->allocations,b->allocations,sizeof(a->allocations));
}

const ralTexture_t *Ral_TransientTextureCohortGetTexture(
		const ralTransientTextureCohort_t *cohort,uint32_t requestIndex ) {
	return cohort&&ReceiptValid(&cohort->receipt)&&requestIndex<cohort->receipt.textureCount
		?cohort->textures[requestIndex]:NULL;
}

const ralTransientPlan_t *Ral_TransientTextureCohortGetPlan(
		const ralTransientTextureCohort_t *cohort ) {
	return cohort&&ReceiptValid(&cohort->receipt)?&cohort->receipt.plan:NULL;
}

qboolean Ral_TransientTextureCohortGetReceipt(
		const ralTransientTextureCohort_t *cohort,
		ralTransientTextureCohortReceipt_t *out ) {
	ralTransientTextureCohortReceipt_t candidate;
	if(!cohort||!out||!ReceiptValid(&cohort->receipt)) return qfalse;
	candidate=cohort->receipt;*out=candidate;return qtrue;
}

qboolean Ral_TransientTextureCohortBegin( ralTransientTextureCohort_t *cohort,
		uint32_t commandSlot,ralTransientBatchReceipt_t *out ) {
	return cohort&&ReceiptValid(&cohort->receipt)
		?Ral_TransientBatchBegin(&cohort->lifecycle,&cohort->receipt.plan,commandSlot,out):qfalse;
}

qboolean Ral_TransientTextureCohortSubmit( ralTransientTextureCohort_t *cohort,
		const ralTransientBatchReceipt_t *planned,uint64_t submissionGeneration,
		ralTransientBatchReceipt_t *out ) {
	return cohort&&ReceiptValid(&cohort->receipt)
		?Ral_TransientBatchSubmit(&cohort->lifecycle,planned,&cohort->receipt.plan,submissionGeneration,out):qfalse;
}

qboolean Ral_TransientTextureCohortRetire( ralTransientTextureCohort_t *cohort,
		const ralTransientBatchReceipt_t *submitted,qboolean fenceCompleted,
		ralTransientBatchReceipt_t *out ) {
	return cohort&&ReceiptValid(&cohort->receipt)
		?Ral_TransientBatchRetire(&cohort->lifecycle,submitted,&cohort->receipt.plan,fenceCompleted,out):qfalse;
}

qboolean Ral_TransientTextureCohortCancel( ralTransientTextureCohort_t *cohort,
		const ralTransientBatchReceipt_t *planned,ralTransientBatchReceipt_t *out ) {
	return cohort&&ReceiptValid(&cohort->receipt)
		?Ral_TransientBatchCancel(&cohort->lifecycle,planned,&cohort->receipt.plan,out):qfalse;
}

qboolean Ral_TransientTextureCohortReleaseTerminal(
		ralTransientTextureCohort_t **cohortInOut,
		const ralTransientBatchReceipt_t *terminalBatch ) {
	uint32_t i;
	ralTransientTextureCohort_t *cohort;
	if(!cohortInOut||!(cohort=*cohortInOut)||!terminalBatch||!ReceiptValid(&cohort->receipt)
			||!Ral_TransientBatchReceiptExact(&cohort->lifecycle.active,terminalBatch)
			||(terminalBatch->state!=RAL_TRANSIENT_BATCH_RETIRED
				&&terminalBatch->state!=RAL_TRANSIENT_BATCH_CANCELED)
			||terminalBatch->planIdentity!=&cohort->receipt.plan
			||terminalBatch->planGeneration!=cohort->receipt.plan.policy.generation
			||terminalBatch->requestCount!=cohort->receipt.textureCount
			||terminalBatch->peakBytes!=cohort->receipt.plan.peakBytes) return qfalse;
	for(i=0u;i<cohort->receipt.textureCount;++i)
		cohort->ops.retireTexture(cohort->context,cohort->textures[i]);
	for(i=0u;i<cohort->receipt.allocationCount;++i)
		cohort->ops.retireAllocation(cohort->context,cohort->allocations[i]);
	memset(&cohort->receipt,0,sizeof(cohort->receipt));free(cohort);*cohortInOut=NULL;return qtrue;
}
