// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "ral_material_reload.h"

#include <string.h>

static qboolean IsZero( const void *data, size_t size ) {
	const unsigned char *bytes = (const unsigned char *)data;
	size_t i;
	for ( i = 0u; i < size; i++ ) if ( bytes[i] ) return qfalse;
	return qtrue;
}

static qboolean ProxyMatches( const ralMaterialRuntimePlan_t *plan,
		qboolean hasProxy, const ralMaterialProxyProgramReceipt_t *proxy ) {
	if ( hasProxy == qtrue ) return Ral_MaterialProxyProgramReceiptValid( proxy )
		&& proxy->program.materialArtifactHash == plan->material.artifactHash;
	return hasProxy == qfalse && IsZero( proxy, sizeof( *proxy ) );
}

qboolean Ral_MaterialReloadReceiptValid( const ralMaterialReloadReceipt_t *r ) {
	uint32_t i, j;
	if ( !r || r->schemaVersion != RAL_MATERIAL_RELOAD_RECEIPT_SCHEMA_VERSION
			|| !r->transactionGeneration || r->ready != qtrue
			|| !Ral_MaterialRuntimePlanValid( &r->activePlan )
			|| !ProxyMatches( &r->activePlan, r->hasProxy, &r->proxy )
			|| r->retirementCount > RAL_MATERIAL_MAX_TEXTURES ) return qfalse;
	for ( i = 0u; i < r->retirementCount; i++ ) {
		const ralMaterialRetirementItem_t *item = &r->retirements[i];
		if ( item->backendType < RAL_BACKEND_VULKAN
				|| item->backendType > RAL_BACKEND_WEBGL2
				|| !item->textureIdentity || !item->resourceGeneration ) return qfalse;
		for ( j = 0u; j < i; j++ )
			if ( r->retirements[j].backendType == item->backendType
					&& r->retirements[j].textureIdentity == item->textureIdentity
					&& r->retirements[j].resourceGeneration == item->resourceGeneration )
				return qfalse;
	}
	for ( i = r->retirementCount; i < RAL_MATERIAL_MAX_TEXTURES; i++ )
		if ( !IsZero( &r->retirements[i], sizeof( r->retirements[i] ) ) ) return qfalse;
	return qtrue;
}

qboolean Ral_MaterialReloadReceiptExact( const ralMaterialReloadReceipt_t *a,
		const ralMaterialReloadReceipt_t *b ) {
	return Ral_MaterialReloadReceiptValid( a ) && Ral_MaterialReloadReceiptValid( b )
		&& !memcmp( a, b, sizeof( *a ) );
}

qboolean Ral_MaterialReloadOwnerValid( const ralMaterialReloadOwner_t *o ) {
	if ( !o || o->schemaVersion != RAL_MATERIAL_RELOAD_SCHEMA_VERSION
			|| !o->transactionGeneration
			|| !Ral_MaterialRuntimePlanValid( &o->fallbackPlan )
			|| !Ral_MaterialRuntimePlanValid( &o->livePlan )
			|| !ProxyMatches( &o->livePlan, o->hasProxy, &o->proxy ) ) return qfalse;
	if ( o->usingFallback == qtrue ) {
		if ( o->hasProxy != qfalse || o->retirementPending != qfalse
				|| !Ral_MaterialRuntimePlanExact( &o->fallbackPlan, &o->livePlan ) ) return qfalse;
	} else if ( o->usingFallback != qfalse ) return qfalse;
	if ( o->retirementPending == qtrue )
		return o->pendingRetirement.retirementCount > 0u
			&& o->pendingRetirement.transactionGeneration == o->transactionGeneration
			&& Ral_MaterialReloadReceiptValid( &o->pendingRetirement )
			&& Ral_MaterialRuntimePlanExact( &o->pendingRetirement.activePlan, &o->livePlan )
			&& o->pendingRetirement.hasProxy == o->hasProxy
			&& !memcmp( &o->pendingRetirement.proxy, &o->proxy, sizeof( o->proxy ) );
	return o->retirementPending == qfalse
		&& IsZero( &o->pendingRetirement, sizeof( o->pendingRetirement ) );
}

qboolean Ral_MaterialReloadOwnerInit( const ralMaterialRuntimePlan_t *fallback,
		uint64_t generation, ralMaterialReloadOwner_t *out ) {
	ralMaterialReloadOwner_t value;
	if ( !out || !generation || !Ral_MaterialRuntimePlanValid( fallback ) ) return qfalse;
	memset( &value, 0, sizeof( value ) );
	value.schemaVersion = RAL_MATERIAL_RELOAD_SCHEMA_VERSION;
	value.transactionGeneration = generation;
	value.fallbackPlan = *fallback; value.livePlan = *fallback;
	value.usingFallback = qtrue;
	*out = value; return qtrue;
}

static qboolean SameResourceGeneration( const ralTextureResourceReceipt_t *a,
		const ralTextureResourceReceipt_t *b ) {
	return a->backendType == b->backendType && a->textureIdentity == b->textureIdentity
		&& a->resourceGeneration == b->resourceGeneration;
}

static void DeriveRetirements( const ralMaterialRuntimePlan_t *oldPlan,
		const ralMaterialRuntimePlan_t *newPlan, ralMaterialReloadReceipt_t *receipt ) {
	uint32_t i, j;
	for ( i = 0u; i < oldPlan->bindingCount; i++ ) {
		qboolean shared = qfalse;
		for ( j = 0u; j < newPlan->bindingCount; j++ )
			if ( SameResourceGeneration( &oldPlan->bindings[i].resource,
					&newPlan->bindings[j].resource ) ) shared = qtrue;
		if ( !shared ) {
			ralMaterialRetirementItem_t *item =
				&receipt->retirements[receipt->retirementCount++];
			item->backendType = oldPlan->bindings[i].resource.backendType;
			item->textureIdentity = oldPlan->bindings[i].resource.textureIdentity;
			item->resourceGeneration = oldPlan->bindings[i].resource.resourceGeneration;
		}
	}
}

qboolean Ral_MaterialReloadTry( ralMaterialReloadOwner_t *owner,
		const ralMaterialReloadCandidate_t *candidate, uint64_t generation,
		ralMaterialReloadReceipt_t *out ) {
	ralMaterialReceipt_t material;
	ralMaterialRuntimePlan_t plan;
	ralMaterialProxyProgramReceipt_t proxy;
	ralMaterialReloadReceipt_t receipt;
	ralMaterialReloadOwner_t next;
	if ( !out || !Ral_MaterialReloadOwnerValid( owner ) || !candidate
			|| generation <= owner->transactionGeneration || owner->retirementPending
			|| !candidate->description || !candidate->runtimeGeneration
			|| candidate->runtimeGeneration <= owner->livePlan.runtimeGeneration
			|| candidate->description->materialGeneration
				<= owner->livePlan.material.material.materialGeneration
			|| candidate->resourceCount > RAL_MATERIAL_MAX_TEXTURES ) return qfalse;
	if ( !Ral_MaterialCompile( candidate->description, &material )
			|| !Ral_MaterialRuntimePlanBuild( &material, candidate->resources,
				candidate->resourceCount, candidate->runtimeGeneration, &plan ) ) return qfalse;
	memset( &proxy, 0, sizeof( proxy ) );
	if ( candidate->proxyProgram
			&& !Ral_MaterialProxyProgramBuild( &material, candidate->proxyProgram, &proxy ) )
		return qfalse;
	memset( &receipt, 0, sizeof( receipt ) );
	receipt.schemaVersion = RAL_MATERIAL_RELOAD_RECEIPT_SCHEMA_VERSION;
	receipt.transactionGeneration = generation; receipt.activePlan = plan;
	receipt.hasProxy = candidate->proxyProgram ? qtrue : qfalse; receipt.proxy = proxy;
	if ( owner->usingFallback == qfalse ) DeriveRetirements( &owner->livePlan, &plan, &receipt );
	receipt.ready = qtrue;
	if ( !Ral_MaterialReloadReceiptValid( &receipt ) ) return qfalse;
	next = *owner; next.transactionGeneration = generation; next.livePlan = plan;
	next.usingFallback = qfalse; next.hasProxy = receipt.hasProxy; next.proxy = proxy;
	memset( &next.pendingRetirement, 0, sizeof( next.pendingRetirement ) );
	next.retirementPending = receipt.retirementCount ? qtrue : qfalse;
	if ( next.retirementPending ) next.pendingRetirement = receipt;
	if ( !Ral_MaterialReloadOwnerValid( &next ) ) return qfalse;
	*owner = next; *out = receipt; return qtrue;
}

qboolean Ral_MaterialReloadAcknowledge( ralMaterialReloadOwner_t *owner,
		const ralMaterialReloadReceipt_t *receipt ) {
	ralMaterialReloadOwner_t next;
	if ( !Ral_MaterialReloadOwnerValid( owner ) || !owner->retirementPending
			|| !Ral_MaterialReloadReceiptExact( receipt, &owner->pendingRetirement ) ) return qfalse;
	next = *owner; next.retirementPending = qfalse;
	memset( &next.pendingRetirement, 0, sizeof( next.pendingRetirement ) );
	if ( !Ral_MaterialReloadOwnerValid( &next ) ) return qfalse;
	*owner = next; return qtrue;
}
