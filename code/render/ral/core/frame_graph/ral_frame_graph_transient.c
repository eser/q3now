// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_frame_graph_transient.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

struct ralFrameGraphTransient_s {
	ralFrameGraphTransientOps_t ops;
	void *context;
	const ralFrameGraphPlan_t *graphPlanIdentity;
	ralFrameGraphPlan_t boundGraphPlan;
	ralTransientTextureRequest_t requests[RAL_TRANSIENT_MAX_REQUESTS];
	ralTransientTextureCohort_t *cohort;
	ralFrameGraphTransientReceipt_t receipt;
};

static qboolean OpsValid( const ralFrameGraphTransientOps_t *ops ) {
	return ops && ops->createCohort && ops->candidateAllowed && ops->getReceipt
		&& ops->getTexture && ops->begin && ops->submit && ops->retire
		&& ops->cancel && ops->releaseFresh && ops->releaseTerminal;
}

static qboolean BindingEmpty( const ralFrameGraphResourceBinding_t *binding ) {
	static const ralFrameGraphResourceBinding_t empty;
	return memcmp(binding,&empty,sizeof(empty)) == 0 ? qtrue : qfalse;
}

static const ralFrameGraphResource_t *FindTransientTextureResource(
		const ralFrameGraphPlan_t *plan, uintptr_t identity, uint64_t generation ) {
	uint32_t i;
	for ( i=0u; i<plan->description.resourceCount; ++i ) {
		const ralFrameGraphResource_t *resource=&plan->description.resources[i];
		if ( resource->identity == identity && resource->generation == generation
				&& resource->kind == RAL_FRAME_GRAPH_RESOURCE_TEXTURE
				&& resource->lifetime == RAL_FRAME_GRAPH_RESOURCE_TRANSIENT )
			return resource;
	}
	return NULL;
}

static qboolean RequestShapeMatchesPlan(
		const ralFrameGraphTransientCreateInfo_t *ci ) {
	uint32_t i;
	if ( !ci || ci->schemaVersion != RAL_FRAME_GRAPH_TRANSIENT_SCHEMA_VERSION
			|| ci->generation == 0u || ci->generation == UINT64_MAX
			|| !ci->graphPlan || !ci->textureRequests
			|| !RalFrameGraph_PlanExact(ci->graphPlan,ci->graphPlan)
			|| ci->graphPlan->transientRequestCount == 0u
			|| ci->textureRequestCount != ci->graphPlan->transientRequestCount )
		return qfalse;
	for ( i=0u; i<ci->textureRequestCount; ++i ) {
		const ralTransientTextureRequest_t *texture=&ci->textureRequests[i];
		const ralTransientRequest_t *request=&ci->graphPlan->transientRequests[i];
		if ( texture->resourceIdentity != request->resourceIdentity
				|| texture->resourceGeneration != request->resourceGeneration
				|| texture->firstPass != request->firstPass
				|| texture->lastPass != request->lastPass
				|| texture->allowAlias != request->allowAlias
				|| !FindTransientTextureResource(ci->graphPlan,
					texture->resourceIdentity,texture->resourceGeneration) ) return qfalse;
	}
	return qtrue;
}

static ralTextureAspectFlags_t TextureAspects( ralFormat_t format ) {
	switch ( format ) {
	case RAL_FORMAT_D16_UNORM:
	case RAL_FORMAT_D32_SFLOAT:
	case RAL_FORMAT_X8_D24_UNORM:
		return RAL_TEXTURE_ASPECT_DEPTH;
	case RAL_FORMAT_D24_UNORM_S8_UINT:
	case RAL_FORMAT_D32_SFLOAT_S8_UINT:
	case RAL_FORMAT_D16_UNORM_S8_UINT:
		return RAL_TEXTURE_ASPECT_DEPTH|RAL_TEXTURE_ASPECT_STENCIL;
	default:
		return RAL_TEXTURE_ASPECT_COLOR;
	}
}

static uint32_t FullMipCount( uint32_t width, uint32_t height, uint32_t depth ) {
	uint32_t count=1u;
	while ( width>1u || height>1u || depth>1u ) {
		if ( width>1u ) width>>=1u;
		if ( height>1u ) height>>=1u;
		if ( depth>1u ) depth>>=1u;
		++count;
	}
	return count;
}

static qboolean TextureSubresources( const ralTextureCreateInfo_t *texture,
		ralTextureAspectFlags_t *outAspects, uint32_t *outMips,
		uint32_t *outLayers ) {
	uint32_t depth=1u,layers=1u;
	if ( !texture || !outAspects || !outMips || !outLayers
			|| texture->width==0u || texture->height==0u
			|| texture->depthOrArrayLayers==0u ) return qfalse;
	switch ( texture->type ) {
	case RAL_TEXTURE_3D: depth=texture->depthOrArrayLayers;break;
	case RAL_TEXTURE_2D_ARRAY: layers=texture->depthOrArrayLayers;break;
	case RAL_TEXTURE_CUBE: layers=6u;break;
	case RAL_TEXTURE_CUBE_ARRAY:
		if ( texture->depthOrArrayLayers>UINT32_MAX/6u ) return qfalse;
		layers=texture->depthOrArrayLayers*6u;break;
	case RAL_TEXTURE_1D:
	case RAL_TEXTURE_2D: break;
	default: return qfalse;
	}
	*outAspects=TextureAspects(texture->format);
	*outMips=texture->mipLevels?texture->mipLevels
		:FullMipCount(texture->width,texture->height,depth);
	*outLayers=layers;
	return *outAspects && *outMips && *outLayers;
}

static uint32_t RatioPermille( uint64_t value, uint64_t total ) {
	uint64_t remainder=0u;
	uint32_t result=0u,i;
	if ( total==0u || value>total ) return UINT32_MAX;
	// Overflow-free floor(value * 1000 / total). At each step remainder is
	// strictly below total and the subtraction form avoids remainder+value.
	for ( i=0u; i<1000u; ++i ) {
		if ( remainder >= total-value ) {
			remainder-=total-value;
			++result;
		} else remainder+=value;
	}
	return result;
}

static qboolean ComputeMeasurements(
		const ralTransientTextureCohortReceipt_t *physical,
		uint64_t *outDisjoint, uint64_t *outPhysical,
		uint64_t *outSaved, uint32_t *outPermille ) {
	uint64_t disjoint=0u,committed=0u;
	uint32_t i;
	if ( !physical || !outDisjoint || !outPhysical || !outSaved || !outPermille )
		return qfalse;
	for ( i=0u; i<physical->allocationCount; ++i ) {
		const uint64_t bytes=physical->allocations[i].allocation.committedSize;
		if ( bytes==0u || committed>UINT64_MAX-bytes ) return qfalse;
		committed+=bytes;
	}
	for ( i=0u; i<physical->textureCount; ++i ) {
		const uint32_t slot=physical->textures[i].slotIndex;
		uint64_t bytes;
		if ( slot>=physical->allocationCount ) return qfalse;
		bytes=physical->allocations[slot].allocation.committedSize;
		if ( bytes==0u || disjoint>UINT64_MAX-bytes ) return qfalse;
		disjoint+=bytes;
	}
	if ( committed>disjoint ) return qfalse;
	*outDisjoint=disjoint;*outPhysical=committed;*outSaved=disjoint-committed;
	*outPermille=RatioPermille(*outSaved,disjoint);
	if ( *outPermille==UINT32_MAX ) return qfalse;
	if ( physical->plan.outcome==RAL_TRANSIENT_PLAN_EXPLICIT_ALIAS )
		return physical->plan.aliasCount>0u && physical->allocationCount<physical->textureCount
			&& *outSaved>0u ? qtrue:qfalse;
	return physical->plan.aliasCount==0u && physical->allocationCount==physical->textureCount
		&& *outSaved==0u && *outPermille==0u ? qtrue:qfalse;
}

static qboolean ReceiptValid( const ralFrameGraphTransientReceipt_t *receipt ) {
	uint64_t disjoint,physical,saved;
	uint32_t permille,i,j;
	if ( !receipt || receipt->schemaVersion!=RAL_FRAME_GRAPH_TRANSIENT_SCHEMA_VERSION
			|| receipt->materializationIdentity==(uintptr_t)0
			|| receipt->generation==0u || receipt->generation==UINT64_MAX
			|| !receipt->graphPlanIdentity || receipt->graphGeneration==0u
			|| receipt->graphGeneration==UINT64_MAX
			|| !Ral_TransientTextureCohortReceiptExact(&receipt->physical,
				&receipt->physical)
			|| receipt->physical.generation!=receipt->graphGeneration
			|| receipt->bindingCount==0u
			|| receipt->bindingCount!=receipt->physical.textureCount
			|| receipt->ready!=qtrue ) return qfalse;
	for ( i=0u; i<receipt->bindingCount; ++i ) {
		const ralFrameGraphResourceBinding_t *binding=&receipt->bindings[i];
		const ralTransientTextureBindingReceipt_t *texture=&receipt->physical.textures[i];
		if ( binding->resourceId==0u
				|| binding->resourceIdentity!=texture->resourceIdentity
				|| binding->resourceGeneration!=texture->resourceGeneration
				|| binding->resourceKind!=RAL_FRAME_GRAPH_RESOURCE_TEXTURE
				|| binding->buffer || !binding->texture || binding->bufferSize!=0u
				|| (uintptr_t)binding->texture!=texture->textureIdentity
				|| binding->textureAspects==0u
				|| (binding->textureAspects&~(RAL_TEXTURE_ASPECT_COLOR
					|RAL_TEXTURE_ASPECT_DEPTH|RAL_TEXTURE_ASPECT_STENCIL))!=0u
				|| binding->textureMipLevels==0u
				|| binding->textureArrayLayers==0u ) return qfalse;
		for ( j=0u; j<i; ++j )
			if ( receipt->bindings[j].resourceId==binding->resourceId
					|| receipt->bindings[j].texture==binding->texture ) return qfalse;
	}
	for ( i=receipt->bindingCount; i<RAL_TRANSIENT_MAX_REQUESTS; ++i )
		if ( !BindingEmpty(&receipt->bindings[i]) ) return qfalse;
	if ( !ComputeMeasurements(&receipt->physical,&disjoint,&physical,&saved,
			&permille)
			|| receipt->disjointEquivalentCommittedBytes!=disjoint
			|| receipt->physicalCommittedBytes!=physical
			|| receipt->savedBytes!=saved || receipt->savedPermille!=permille )
		return qfalse;
	return qtrue;
}

qboolean RalFrameGraphTransient_ReceiptExact(
		const ralFrameGraphTransientReceipt_t *a,
		const ralFrameGraphTransientReceipt_t *b ) {
	uint32_t i;
	if ( !ReceiptValid(a) || !ReceiptValid(b)
			|| a->materializationIdentity!=b->materializationIdentity
			|| a->generation!=b->generation
			|| a->graphPlanIdentity!=b->graphPlanIdentity
			|| a->graphGeneration!=b->graphGeneration
			|| a->opsContextIdentity!=b->opsContextIdentity
			|| !Ral_TransientTextureCohortReceiptExact(&a->physical,&b->physical)
			|| a->bindingCount!=b->bindingCount
			|| a->disjointEquivalentCommittedBytes
				!=b->disjointEquivalentCommittedBytes
			|| a->physicalCommittedBytes!=b->physicalCommittedBytes
			|| a->savedBytes!=b->savedBytes || a->savedPermille!=b->savedPermille
			|| a->ready!=b->ready ) return qfalse;
	for ( i=0u; i<a->bindingCount; ++i )
		if ( memcmp(&a->bindings[i],&b->bindings[i],sizeof(a->bindings[i]))!=0 )
			return qfalse;
	return qtrue;
}

static qboolean OwnerValid( const ralFrameGraphTransient_t *owner ) {
	ralTransientTextureCohortReceipt_t current;
	uint32_t i;
	if ( !owner || !OpsValid(&owner->ops) || !owner->cohort
			|| !owner->graphPlanIdentity
			|| !RalFrameGraph_PlanExact(&owner->boundGraphPlan,&owner->boundGraphPlan)
			|| !RalFrameGraph_PlanExact(owner->graphPlanIdentity,
				&owner->boundGraphPlan)
			|| !ReceiptValid(&owner->receipt)
			|| owner->receipt.materializationIdentity!=(uintptr_t)owner
			|| owner->receipt.graphPlanIdentity!=owner->graphPlanIdentity
			|| owner->receipt.physical.cohortIdentity!=(uintptr_t)owner->cohort )
		return qfalse;
	memset(&current,0,sizeof(current));
	if ( !owner->ops.getReceipt(owner->context,owner->cohort,&current)
			|| !Ral_TransientTextureCohortReceiptExact(&current,
				&owner->receipt.physical) ) return qfalse;
	for ( i=0u; i<owner->receipt.bindingCount; ++i )
		if ( owner->ops.getTexture(owner->context,owner->cohort,i)
				!=owner->receipt.bindings[i].texture ) return qfalse;
	return qtrue;
}

qboolean RalFrameGraphTransient_CreateWithOps(
		const ralFrameGraphTransientCreateInfo_t *ci,
		const ralFrameGraphTransientOps_t *ops, void *context,
		ralFrameGraphTransient_t **out ) {
	ralFrameGraphTransient_t *candidate;
	ralTransientTextureCohortCreateInfo_t physicalCreate;
	uint32_t i;
	qboolean cohortOwned=qfalse;
	if ( !out || !OpsValid(ops) || !RequestShapeMatchesPlan(ci) ) return qfalse;
	candidate=(ralFrameGraphTransient_t*)malloc(sizeof(*candidate));
	if ( !candidate ) return qfalse;
	memset(candidate,0,sizeof(*candidate));candidate->ops=*ops;
	candidate->context=context;candidate->graphPlanIdentity=ci->graphPlan;
	candidate->boundGraphPlan=*ci->graphPlan;
	memcpy(candidate->requests,ci->textureRequests,
		ci->textureRequestCount*sizeof(candidate->requests[0]));
	memset(&physicalCreate,0,sizeof(physicalCreate));
	physicalCreate.policy=ci->graphPlan->transientPlan.policy;
	physicalCreate.requests=candidate->requests;
	physicalCreate.requestCount=ci->textureRequestCount;
	if ( !ops->createCohort(context,&physicalCreate,&candidate->cohort)
			|| !candidate->cohort
			|| !ops->candidateAllowed(context,
				RAL_FRAME_GRAPH_TRANSIENT_PHYSICAL_COHORT,
				(uintptr_t)candidate->cohort) ) goto fail;
	cohortOwned=qtrue;
	if ( !ops->getReceipt(context,candidate->cohort,&candidate->receipt.physical)
			|| candidate->receipt.physical.cohortIdentity
				!=(uintptr_t)candidate->cohort
			|| !Ral_TransientPlanExact(&candidate->receipt.physical.plan,
				&ci->graphPlan->transientPlan) ) goto fail;
	for ( i=0u; i<ci->textureRequestCount; ++i ) {
		const ralFrameGraphResource_t *resource=FindTransientTextureResource(
			ci->graphPlan,ci->textureRequests[i].resourceIdentity,
			ci->textureRequests[i].resourceGeneration);
		ralFrameGraphResourceBinding_t *binding=&candidate->receipt.bindings[i];
		if ( !resource ) goto fail;
		binding->resourceId=resource->id;binding->resourceIdentity=resource->identity;
		binding->resourceGeneration=resource->generation;
		binding->resourceKind=RAL_FRAME_GRAPH_RESOURCE_TEXTURE;
		binding->texture=(ralTexture_t*)ops->getTexture(context,candidate->cohort,i);
		if ( !binding->texture
				|| (uintptr_t)binding->texture
					!=candidate->receipt.physical.textures[i].textureIdentity
				|| !TextureSubresources(&ci->textureRequests[i].texture,
					&binding->textureAspects,&binding->textureMipLevels,
					&binding->textureArrayLayers) ) goto fail;
	}
	candidate->receipt.schemaVersion=RAL_FRAME_GRAPH_TRANSIENT_SCHEMA_VERSION;
	candidate->receipt.materializationIdentity=(uintptr_t)candidate;
	candidate->receipt.generation=ci->generation;
	candidate->receipt.graphPlanIdentity=ci->graphPlan;
	candidate->receipt.graphGeneration=ci->graphPlan->description.generation;
	candidate->receipt.opsContextIdentity=(uintptr_t)context;
	candidate->receipt.bindingCount=ci->textureRequestCount;
	if ( !ComputeMeasurements(&candidate->receipt.physical,
			&candidate->receipt.disjointEquivalentCommittedBytes,
			&candidate->receipt.physicalCommittedBytes,
			&candidate->receipt.savedBytes,&candidate->receipt.savedPermille) )
		goto fail;
	candidate->receipt.ready=qtrue;
	if ( !OwnerValid(candidate) ) goto fail;
	*out=candidate;return qtrue;
fail:
	if ( cohortOwned ) {
		ralTransientTextureCohort_t *physical=candidate->cohort;
		(void)ops->releaseFresh(context,&physical);
	}
	free(candidate);return qfalse;
}

qboolean RalFrameGraphTransient_GetReceipt(
		const ralFrameGraphTransient_t *owner,
		ralFrameGraphTransientReceipt_t *out ) {
	ralFrameGraphTransientReceipt_t candidate;
	if ( !out || !OwnerValid(owner) ) return qfalse;
	candidate=owner->receipt;*out=candidate;return qtrue;
}

qboolean RalFrameGraphTransient_GetExecutionBindings(
		const ralFrameGraphTransient_t *owner,
		const ralFrameGraphPlan_t *currentGraphPlan,
		ralFrameGraphResourceBinding_t *outBindings, uint32_t bindingCapacity,
		uint32_t *outBindingCount ) {
	ralFrameGraphResourceBinding_t candidate[RAL_TRANSIENT_MAX_REQUESTS];
	if ( !outBindings || !outBindingCount || !OwnerValid(owner)
			|| currentGraphPlan!=owner->graphPlanIdentity
			|| !RalFrameGraph_PlanExact(currentGraphPlan,&owner->boundGraphPlan)
			|| bindingCapacity<owner->receipt.bindingCount ) return qfalse;
	memcpy(candidate,owner->receipt.bindings,
		owner->receipt.bindingCount*sizeof(candidate[0]));
	memcpy(outBindings,candidate,owner->receipt.bindingCount*sizeof(candidate[0]));
	*outBindingCount=owner->receipt.bindingCount;return qtrue;
}

qboolean RalFrameGraphTransient_Begin( ralFrameGraphTransient_t *owner,
		uint32_t commandSlot, ralTransientBatchReceipt_t *out ) {
	return OwnerValid(owner)
		?owner->ops.begin(owner->context,owner->cohort,commandSlot,out):qfalse;
}

qboolean RalFrameGraphTransient_Submit( ralFrameGraphTransient_t *owner,
		const ralTransientBatchReceipt_t *planned, uint64_t submissionGeneration,
		ralTransientBatchReceipt_t *out ) {
	return OwnerValid(owner)?owner->ops.submit(owner->context,owner->cohort,
		planned,submissionGeneration,out):qfalse;
}

qboolean RalFrameGraphTransient_Retire( ralFrameGraphTransient_t *owner,
		const ralTransientBatchReceipt_t *submitted, qboolean fenceCompleted,
		ralTransientBatchReceipt_t *out ) {
	return OwnerValid(owner)?owner->ops.retire(owner->context,owner->cohort,
		submitted,fenceCompleted,out):qfalse;
}

qboolean RalFrameGraphTransient_Cancel( ralFrameGraphTransient_t *owner,
		const ralTransientBatchReceipt_t *planned,
		ralTransientBatchReceipt_t *out ) {
	return OwnerValid(owner)?owner->ops.cancel(owner->context,owner->cohort,
		planned,out):qfalse;
}

qboolean RalFrameGraphTransient_ReleaseFresh(
		ralFrameGraphTransient_t **ownerInOut ) {
	ralFrameGraphTransient_t *owner;
	ralTransientTextureCohort_t *physical;
	if ( !ownerInOut || !(owner=*ownerInOut) || !OwnerValid(owner) ) return qfalse;
	physical=owner->cohort;
	if ( !owner->ops.releaseFresh(owner->context,&physical) || physical )
		return qfalse;
	memset(&owner->receipt,0,sizeof(owner->receipt));free(owner);
	*ownerInOut=NULL;return qtrue;
}

qboolean RalFrameGraphTransient_ReleaseTerminal(
		ralFrameGraphTransient_t **ownerInOut,
		const ralTransientBatchReceipt_t *terminalBatch ) {
	ralFrameGraphTransient_t *owner;
	ralTransientTextureCohort_t *physical;
	if ( !ownerInOut || !(owner=*ownerInOut) || !OwnerValid(owner)
			|| !terminalBatch ) return qfalse;
	physical=owner->cohort;
	if ( !owner->ops.releaseTerminal(owner->context,&physical,terminalBatch)
			|| physical ) return qfalse;
	memset(&owner->receipt,0,sizeof(owner->receipt));free(owner);
	*ownerInOut=NULL;return qtrue;
}
