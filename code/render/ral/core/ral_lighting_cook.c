// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors
#include "ral_lighting_cook.h"
#include <string.h>
#define FO UINT64_C(14695981039346656037)
#define FP UINT64_C(1099511628211)
static uint64_t B(uint64_t h,unsigned char v){return(h^v)*FP;}
static uint64_t U32(uint64_t h,uint32_t v){uint32_t i;for(i=0;i<4;i++)h=B(h,(unsigned char)(v>>(i*8)));return h;}
static uint64_t U64(uint64_t h,uint64_t v){uint32_t i;for(i=0;i<8;i++)h=B(h,(unsigned char)(v>>(i*8)));return h;}
static uint64_t PlanHash(const ralLightingCookReceipt_t*r){uint64_t h=U32(FO,RAL_LIGHTING_COOK_RECEIPT_SCHEMA_VERSION);uint32_t i;
	h=U64(h,r->cookGeneration);h=U64(h,r->sourceRevision);h=U32(h,r->requestedProducts);h=U32(h,r->invalidatedProducts);
	h=U32(h,r->scheduledRegionCount);h=U32(h,r->deferredRegionCount);for(i=0;i<r->scheduledRegionCount;i++)h=U64(h,r->scheduledRegionIds[i]);return h?h:1u;}
qboolean Ral_LightingCookPlan(const ralLightingCookRequest_t*q,ralLightingCookReceipt_t*out){
	ralLightingCookReceipt_t r;uint32_t dirty,i;if(!out||!q||q->schemaVersion!=RAL_LIGHTING_COOK_SCHEMA_VERSION
		||!q->cookGeneration||!q->sourceRevision||!q->requestedProducts||(q->requestedProducts&~RAL_LIGHTING_COOK_ALL)
		||!q->workerRegionBudget||q->workerRegionBudget>RAL_LIGHTING_COOK_MAX_DIRTY_REGIONS
		||q->dirtyRegionCount>RAL_LIGHTING_COOK_MAX_DIRTY_REGIONS||!Ral_LightingCacheKeyValid(&q->desiredKey))return qfalse;
	for(i=0;i<q->dirtyRegionCount;i++)if(!q->dirtyRegionIds[i]||(i&&q->dirtyRegionIds[i-1]>=q->dirtyRegionIds[i]))return qfalse;
	memset(&r,0,sizeof(r));r.schemaVersion=RAL_LIGHTING_COOK_RECEIPT_SCHEMA_VERSION;r.cookGeneration=q->cookGeneration;
	r.sourceRevision=q->sourceRevision;r.requestedProducts=q->requestedProducts;
	dirty=Ral_LightingCacheDirtyMask(&q->publishedKey,&q->desiredKey);
	if(dirty&RAL_LIGHTING_DIRTY_STATIC_INDIRECT)r.invalidatedProducts|=RAL_LIGHTING_COOK_DIRECTIONAL_LIGHTMAP;
	if(dirty&RAL_LIGHTING_DIRTY_STATIONARY_SHADOW)r.invalidatedProducts|=RAL_LIGHTING_COOK_STATIONARY_VISIBILITY;
	if(dirty&RAL_LIGHTING_DIRTY_PROBE_VOLUME)r.invalidatedProducts|=RAL_LIGHTING_COOK_IRRADIANCE_PROBES;
	r.invalidatedProducts&=r.requestedProducts;r.scheduledRegionCount=q->dirtyRegionCount;
	if(r.scheduledRegionCount>q->workerRegionBudget)r.scheduledRegionCount=q->workerRegionBudget;
	r.deferredRegionCount=q->dirtyRegionCount-r.scheduledRegionCount;
	for(i=0;i<r.scheduledRegionCount;i++)r.scheduledRegionIds[i]=q->dirtyRegionIds[i];
	r.state=RAL_LIGHTING_COOK_PLANNED;r.transitionSerial=1u;r.ready=qtrue;r.planHash=PlanHash(&r);*out=r;return qtrue;
}
qboolean Ral_LightingCookReceiptValid(const ralLightingCookReceipt_t*r){uint32_t i;if(!r||r->schemaVersion!=RAL_LIGHTING_COOK_RECEIPT_SCHEMA_VERSION
		||!r->cookGeneration||!r->sourceRevision||!r->planHash||!r->requestedProducts||(r->requestedProducts&~RAL_LIGHTING_COOK_ALL)
		||(r->invalidatedProducts&~r->requestedProducts)||r->scheduledRegionCount>RAL_LIGHTING_COOK_MAX_DIRTY_REGIONS
		||r->state<RAL_LIGHTING_COOK_PLANNED||r->state>RAL_LIGHTING_COOK_PUBLISHED||!r->transitionSerial||r->ready!=qtrue)return qfalse;
	if((r->state>=RAL_LIGHTING_COOK_STAGED)!=(r->stagedManifestHash!=0u))return qfalse;
	for(i=0;i<r->scheduledRegionCount;i++)if(!r->scheduledRegionIds[i]||(i&&r->scheduledRegionIds[i-1]>=r->scheduledRegionIds[i]))return qfalse;
	for(i=r->scheduledRegionCount;i<RAL_LIGHTING_COOK_MAX_DIRTY_REGIONS;i++)if(r->scheduledRegionIds[i])return qfalse;
	return r->planHash==PlanHash(r);
}
qboolean Ral_LightingCookTransition(const ralLightingCookReceipt_t*c,ralLightingCookState_t next,
		uint32_t products,uint64_t manifest,ralLightingCookReceipt_t*out){ralLightingCookReceipt_t r;qboolean allowed=qfalse;
	if(!out||!Ral_LightingCookReceiptValid(c)||products&~c->requestedProducts)return qfalse;
	if(next==RAL_LIGHTING_COOK_CANCELLED)allowed=c->state==RAL_LIGHTING_COOK_PLANNED||c->state==RAL_LIGHTING_COOK_RUNNING;
	else if(c->state==RAL_LIGHTING_COOK_PLANNED&&next==RAL_LIGHTING_COOK_RUNNING)allowed=qtrue;
	else if(c->state==RAL_LIGHTING_COOK_RUNNING&&next==RAL_LIGHTING_COOK_STAGED)allowed=products==c->invalidatedProducts&&manifest;
	else if(c->state==RAL_LIGHTING_COOK_STAGED&&next==RAL_LIGHTING_COOK_VERIFIED)allowed=products==c->invalidatedProducts&&manifest==c->stagedManifestHash;
	else if(c->state==RAL_LIGHTING_COOK_VERIFIED&&next==RAL_LIGHTING_COOK_PUBLISHED)allowed=products==c->invalidatedProducts&&manifest==c->stagedManifestHash;
	if(!allowed)return qfalse;
	r=*c;r.state=next;r.transitionSerial++;
	if(next==RAL_LIGHTING_COOK_STAGED)r.stagedManifestHash=manifest;
	*out=r;return qtrue;}

qboolean Ral_LightingCacheSelectionReceiptValid(
		const ralLightingCacheSelectionReceipt_t *r ) {
	if ( !r || r->schemaVersion != RAL_LIGHTING_CACHE_SELECTION_RECEIPT_SCHEMA_VERSION
			|| !r->expectedArtifactGeneration || !r->expectedCacheKey
			|| r->expectedKind < RAL_LIGHTING_ARTIFACT_DIRECTIONAL_LIGHTMAP
			|| r->expectedKind > RAL_LIGHTING_ARTIFACT_IRRADIANCE_VOLUME
			|| r->mode < RAL_LIGHTING_CACHE_MODERN_FINAL
			|| r->mode > RAL_LIGHTING_CACHE_BLOCKED || r->ready != qtrue ) return qfalse;
	if ( r->mode == RAL_LIGHTING_CACHE_MODERN_FINAL )
		return r->artifactManifestHash && !r->requiresRegeneration && r->finalQuality;
	if ( r->artifactManifestHash ) return qfalse;
	if ( r->mode == RAL_LIGHTING_CACHE_LEGACY_COMPATIBILITY )
		return r->requiresRegeneration && r->finalQuality;
	return r->requiresRegeneration && !r->finalQuality;
}

qboolean Ral_LightingCacheSelect( const ralLightingCacheSelectionRequest_t *q,
		ralLightingCacheSelectionReceipt_t *out ) {
	ralLightingCacheSelectionReceipt_t r; qboolean exact;
	if ( !q || !out || q->schemaVersion != RAL_LIGHTING_CACHE_SELECTION_SCHEMA_VERSION
			|| !q->expectedArtifactGeneration || !q->expectedCacheKey
			|| q->expectedKind < RAL_LIGHTING_ARTIFACT_DIRECTIONAL_LIGHTMAP
			|| q->expectedKind > RAL_LIGHTING_ARTIFACT_IRRADIANCE_VOLUME ) return qfalse;
	memset( &r, 0, sizeof( r ) );
	r.schemaVersion = RAL_LIGHTING_CACHE_SELECTION_RECEIPT_SCHEMA_VERSION;
	r.expectedArtifactGeneration = q->expectedArtifactGeneration;
	r.expectedCacheKey = q->expectedCacheKey; r.expectedKind = q->expectedKind;
	exact = q->artifact && Ral_LightingArtifactReceiptValid( q->artifact )
		&& q->artifact->artifactGeneration == q->expectedArtifactGeneration
		&& q->artifact->cacheKey == q->expectedCacheKey
		&& q->artifact->kind == q->expectedKind;
	if ( exact ) {
		r.mode = RAL_LIGHTING_CACHE_MODERN_FINAL;
		r.artifactManifestHash = q->artifact->manifestHash; r.finalQuality = qtrue;
	} else {
		r.requiresRegeneration = qtrue;
		if ( q->legacyCompatibilityAvailable && !q->requireModernProduct ) {
			r.mode = RAL_LIGHTING_CACHE_LEGACY_COMPATIBILITY; r.finalQuality = qtrue;
		} else if ( q->allowPreview ) r.mode = RAL_LIGHTING_CACHE_PREVIEW;
		else r.mode = RAL_LIGHTING_CACHE_BLOCKED;
	}
	r.ready = qtrue;
	if ( !Ral_LightingCacheSelectionReceiptValid( &r ) ) return qfalse;
	*out = r; return qtrue;
}
