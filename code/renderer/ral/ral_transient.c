// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#include "ral_transient.h"

#include <limits.h>
#include <string.h>

static qboolean BoolValid( qboolean value ) { return value == qfalse || value == qtrue; }
static qboolean PowerOfTwo( uint64_t value ) { return value && !(value & (value-1u)); }

static qboolean AlignSize( uint64_t size, uint64_t alignment, uint64_t *out ) {
	if ( !out || size == 0u || !PowerOfTwo(alignment)
			|| size > UINT64_MAX - (alignment-1u) ) return qfalse;
	*out = (size + alignment - 1u) & ~(alignment - 1u);
	return qtrue;
}

static qboolean RequestValid( const ralTransientRequest_t *request ) {
	return request && request->resourceIdentity != (uintptr_t)0
		&& request->resourceGeneration != 0u && request->resourceGeneration != UINT64_MAX
		&& request->compatibilityKey != 0u && request->size != 0u
		&& PowerOfTwo(request->alignment) && request->firstPass <= request->lastPass
		&& BoolValid(request->allowAlias);
}

static qboolean PolicyValid( const ralTransientPolicy_t *policy ) {
	return policy && policy->backendType >= RAL_BACKEND_VULKAN
		&& policy->backendType <= RAL_BACKEND_WEBGL2
		&& policy->generation != 0u && policy->generation != UINT64_MAX
		&& policy->budgetBytes != 0u && BoolValid(policy->explicitAliasing)
		&& !((policy->backendType == RAL_BACKEND_WEBGPU
			|| policy->backendType == RAL_BACKEND_WEBGL2) && policy->explicitAliasing);
}

static qboolean PlanBuildInternal( const ralTransientPolicy_t *policy,
		const ralTransientRequest_t *requests, uint32_t requestCount,
		ralTransientPlan_t *candidate ) {
	uint32_t i,j;
	if ( !PolicyValid(policy) || !requests || !candidate || requestCount == 0u
			|| requestCount > RAL_TRANSIENT_MAX_REQUESTS ) return qfalse;
	memset(candidate,0,sizeof(*candidate));
	candidate->schemaVersion=RAL_TRANSIENT_SCHEMA_VERSION;
	candidate->policy=*policy;candidate->requestCount=requestCount;
	for(i=0u;i<requestCount;++i) {
		const ralTransientRequest_t *request=&requests[i];
		uint64_t committed;
		uint32_t chosen=UINT32_MAX;
		if ( !RequestValid(request) || !AlignSize(request->size,request->alignment,&committed) ) return qfalse;
		if ( i>0u && (request->firstPass < requests[i-1u].firstPass
				|| (request->firstPass == requests[i-1u].firstPass
					&& request->resourceIdentity <= requests[i-1u].resourceIdentity)) ) return qfalse;
		for(j=0u;j<i;++j)
			if(request->resourceIdentity==requests[j].resourceIdentity) return qfalse;
		if ( candidate->requestedBytes > UINT64_MAX-request->size ) return qfalse;
		candidate->requestedBytes+=request->size;
		if ( policy->explicitAliasing && request->allowAlias ) {
			for(j=0u;j<candidate->slotCount;++j) {
				ralTransientSlot_t *slot=&candidate->slots[j];
				if(slot->aliasable && slot->compatibilityKey==request->compatibilityKey
						&& slot->lastPass < request->firstPass) { chosen=j;break; }
			}
		}
		if ( chosen==UINT32_MAX ) {
			ralTransientSlot_t *slot;
			if(candidate->slotCount>=RAL_TRANSIENT_MAX_REQUESTS
					|| committed>policy->budgetBytes-candidate->peakBytes) return qfalse;
			chosen=candidate->slotCount++;
			slot=&candidate->slots[chosen];slot->compatibilityKey=request->compatibilityKey;
			slot->committedSize=committed;slot->alignment=request->alignment;
			slot->lastPass=request->lastPass;slot->aliasable=request->allowAlias;
			candidate->peakBytes+=committed;
		} else {
			ralTransientSlot_t *slot=&candidate->slots[chosen];
			uint64_t newSize,newAlignment=slot->alignment>request->alignment?slot->alignment:request->alignment;
			uint64_t maxSize=slot->committedSize>request->size?slot->committedSize:request->size;
			if(!AlignSize(maxSize,newAlignment,&newSize)) return qfalse;
			if(newSize>slot->committedSize) {
				const uint64_t delta=newSize-slot->committedSize;
				if(delta>policy->budgetBytes-candidate->peakBytes) return qfalse;
				candidate->peakBytes+=delta;slot->committedSize=newSize;slot->alignment=newAlignment;
			}
			slot->lastPass=request->lastPass;candidate->aliasCount++;
		}
		candidate->assignments[i].request=*request;
		candidate->assignments[i].slotIndex=chosen;
		candidate->assignments[i].committedSize=candidate->slots[chosen].committedSize;
	}
	for(i=0u;i<requestCount;++i)
		candidate->assignments[i].committedSize=
			candidate->slots[candidate->assignments[i].slotIndex].committedSize;
	candidate->outcome=!policy->explicitAliasing?RAL_TRANSIENT_PLAN_MANAGED_DISJOINT
		:(candidate->aliasCount?RAL_TRANSIENT_PLAN_EXPLICIT_ALIAS:RAL_TRANSIENT_PLAN_DISJOINT);
	candidate->ready=qtrue;
	return qtrue;
}

qboolean Ral_TransientPlanBuild( const ralTransientPolicy_t *policy,
		const ralTransientRequest_t *requests, uint32_t requestCount,
		ralTransientPlan_t *out ) {
	ralTransientPlan_t candidate;
	if(!out || !PlanBuildInternal(policy,requests,requestCount,&candidate)) return qfalse;
	*out=candidate;return qtrue;
}

static qboolean PlanFieldsExact( const ralTransientPlan_t *a,const ralTransientPlan_t *b ) {
	uint32_t i;
	if(a->schemaVersion!=b->schemaVersion || a->policy.backendType!=b->policy.backendType
			|| a->policy.generation!=b->policy.generation || a->policy.budgetBytes!=b->policy.budgetBytes
			|| a->policy.explicitAliasing!=b->policy.explicitAliasing || a->outcome!=b->outcome
			|| a->requestCount!=b->requestCount || a->slotCount!=b->slotCount
			|| a->aliasCount!=b->aliasCount || a->requestedBytes!=b->requestedBytes
			|| a->peakBytes!=b->peakBytes || a->ready!=b->ready) return qfalse;
	for(i=0u;i<a->requestCount;++i) {
		const ralTransientAssignment_t *x=&a->assignments[i],*y=&b->assignments[i];
		if(x->request.resourceIdentity!=y->request.resourceIdentity
				|| x->request.resourceGeneration!=y->request.resourceGeneration
				|| x->request.compatibilityKey!=y->request.compatibilityKey
				|| x->request.size!=y->request.size || x->request.alignment!=y->request.alignment
				|| x->request.firstPass!=y->request.firstPass || x->request.lastPass!=y->request.lastPass
				|| x->request.allowAlias!=y->request.allowAlias || x->slotIndex!=y->slotIndex
				|| x->committedSize!=y->committedSize) return qfalse;
	}
	for(i=0u;i<a->slotCount;++i)
		if(a->slots[i].compatibilityKey!=b->slots[i].compatibilityKey
				|| a->slots[i].committedSize!=b->slots[i].committedSize
				|| a->slots[i].alignment!=b->slots[i].alignment
				|| a->slots[i].lastPass!=b->slots[i].lastPass
				|| a->slots[i].aliasable!=b->slots[i].aliasable) return qfalse;
	return qtrue;
}

qboolean Ral_TransientPlanExact( const ralTransientPlan_t *a,const ralTransientPlan_t *b ) {
	ralTransientPlan_t rebuiltA,rebuiltB;
	if(!a||!b||a->ready!=qtrue||b->ready!=qtrue
			|| a->requestCount==0u||a->requestCount>RAL_TRANSIENT_MAX_REQUESTS
			|| b->requestCount==0u||b->requestCount>RAL_TRANSIENT_MAX_REQUESTS) return qfalse;
	// Assignment structs are not a packed request array; rebuild from explicit copies.
	{
		ralTransientRequest_t requestsA[RAL_TRANSIENT_MAX_REQUESTS],requestsB[RAL_TRANSIENT_MAX_REQUESTS];
		uint32_t i;
		for(i=0u;i<a->requestCount;++i) requestsA[i]=a->assignments[i].request;
		for(i=0u;i<b->requestCount && i<RAL_TRANSIENT_MAX_REQUESTS;++i) requestsB[i]=b->assignments[i].request;
		if(!PlanBuildInternal(&a->policy,requestsA,a->requestCount,&rebuiltA)
				|| !PlanBuildInternal(&b->policy,requestsB,b->requestCount,&rebuiltB)) return qfalse;
	}
	return PlanFieldsExact(a,&rebuiltA)&&PlanFieldsExact(b,&rebuiltB)&&PlanFieldsExact(a,b);
}

static qboolean ReceiptValid( const ralTransientBatchReceipt_t *receipt ) {
	return receipt && receipt->schemaVersion==RAL_TRANSIENT_SCHEMA_VERSION
		&& receipt->state>=RAL_TRANSIENT_BATCH_PLANNED && receipt->state<=RAL_TRANSIENT_BATCH_CANCELED
		&& receipt->planIdentity && receipt->planGeneration!=0u && receipt->planGeneration!=UINT64_MAX
		&& receipt->batchGeneration!=0u && receipt->batchGeneration!=UINT64_MAX
		&& receipt->commandSlot<RAL_TRANSIENT_MAX_COMMAND_SLOTS
		&& receipt->requestCount>0u && receipt->requestCount<=RAL_TRANSIENT_MAX_REQUESTS
		&& receipt->peakBytes>0u && receipt->ready==qtrue
		&& ((receipt->state==RAL_TRANSIENT_BATCH_SUBMITTED||receipt->state==RAL_TRANSIENT_BATCH_RETIRED)
			? receipt->submissionGeneration!=0u&&receipt->submissionGeneration!=UINT64_MAX
			: receipt->submissionGeneration==0u);
}

qboolean Ral_TransientBatchReceiptExact( const ralTransientBatchReceipt_t *a,
		const ralTransientBatchReceipt_t *b ) {
	return ReceiptValid(a)&&ReceiptValid(b)&&a->state==b->state
		&& a->planIdentity==b->planIdentity&&a->planGeneration==b->planGeneration
		&& a->batchGeneration==b->batchGeneration
		&& a->submissionGeneration==b->submissionGeneration
		&& a->commandSlot==b->commandSlot&&a->requestCount==b->requestCount
		&& a->peakBytes==b->peakBytes;
}

void Ral_TransientBatchLifecycleInit( ralTransientBatchLifecycle_t *lifecycle ) {
	if(lifecycle) memset(lifecycle,0,sizeof(*lifecycle));
}

static qboolean ExactActive( const ralTransientBatchLifecycle_t *lifecycle,
		const ralTransientBatchReceipt_t *authority,const ralTransientPlan_t *plan,
		ralTransientBatchState_t state ) {
	return lifecycle&&authority&&plan&&lifecycle->active.state==state
		&& Ral_TransientBatchReceiptExact(&lifecycle->active,authority)
		&& authority->planIdentity==plan&&Ral_TransientPlanExact(&lifecycle->boundPlan,plan)
		&& authority->planGeneration==plan->policy.generation;
}

qboolean Ral_TransientBatchBegin( ralTransientBatchLifecycle_t *lifecycle,
		const ralTransientPlan_t *plan,uint32_t commandSlot,ralTransientBatchReceipt_t *out ) {
	ralTransientBatchLifecycle_t candidate;ralTransientBatchReceipt_t receipt;
	if(!lifecycle||!plan||!out||commandSlot>=RAL_TRANSIENT_MAX_COMMAND_SLOTS
			|| !Ral_TransientPlanExact(plan,plan)
			|| (lifecycle->active.state!=RAL_TRANSIENT_BATCH_EMPTY
				&& lifecycle->active.state!=RAL_TRANSIENT_BATCH_RETIRED
				&& lifecycle->active.state!=RAL_TRANSIENT_BATCH_CANCELED)
			|| lifecycle->nextBatchGeneration>=UINT64_MAX-1u) return qfalse;
	candidate=*lifecycle;memset(&receipt,0,sizeof(receipt));
	receipt.schemaVersion=RAL_TRANSIENT_SCHEMA_VERSION;receipt.state=RAL_TRANSIENT_BATCH_PLANNED;
	receipt.planIdentity=plan;receipt.planGeneration=plan->policy.generation;
	receipt.batchGeneration=candidate.nextBatchGeneration+1u;receipt.commandSlot=commandSlot;
	receipt.requestCount=plan->requestCount;receipt.peakBytes=plan->peakBytes;receipt.ready=qtrue;
	candidate.nextBatchGeneration=receipt.batchGeneration;candidate.boundPlan=*plan;candidate.active=receipt;
	*lifecycle=candidate;*out=receipt;return qtrue;
}

qboolean Ral_TransientBatchSubmit( ralTransientBatchLifecycle_t *lifecycle,
		const ralTransientBatchReceipt_t *authority,const ralTransientPlan_t *plan,
		uint64_t submissionGeneration,ralTransientBatchReceipt_t *out ) {
	ralTransientBatchLifecycle_t candidate;
	if(!out||submissionGeneration==0u||submissionGeneration==UINT64_MAX
			|| !ExactActive(lifecycle,authority,plan,RAL_TRANSIENT_BATCH_PLANNED)) return qfalse;
	candidate=*lifecycle;candidate.active.state=RAL_TRANSIENT_BATCH_SUBMITTED;
	candidate.active.submissionGeneration=submissionGeneration;
	*lifecycle=candidate;*out=candidate.active;return qtrue;
}

qboolean Ral_TransientBatchRetire( ralTransientBatchLifecycle_t *lifecycle,
		const ralTransientBatchReceipt_t *authority,const ralTransientPlan_t *plan,
		qboolean fenceCompleted,ralTransientBatchReceipt_t *out ) {
	ralTransientBatchLifecycle_t candidate;
	if(!out||fenceCompleted!=qtrue
			|| !ExactActive(lifecycle,authority,plan,RAL_TRANSIENT_BATCH_SUBMITTED)) return qfalse;
	candidate=*lifecycle;candidate.active.state=RAL_TRANSIENT_BATCH_RETIRED;
	*lifecycle=candidate;*out=candidate.active;return qtrue;
}

qboolean Ral_TransientBatchCancel( ralTransientBatchLifecycle_t *lifecycle,
		const ralTransientBatchReceipt_t *authority,const ralTransientPlan_t *plan,
		ralTransientBatchReceipt_t *out ) {
	ralTransientBatchLifecycle_t candidate;
	if(!out||!ExactActive(lifecycle,authority,plan,RAL_TRANSIENT_BATCH_PLANNED)) return qfalse;
	candidate=*lifecycle;candidate.active.state=RAL_TRANSIENT_BATCH_CANCELED;
	*lifecycle=candidate;*out=candidate.active;return qtrue;
}
