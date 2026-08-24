// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "ral_material_runtime.h"

#include <string.h>

static qboolean ResourceMatches( const ralTextureResourceReceipt_t *r,
		const ralTextureAssetReceipt_t *a ) {
	ralTextureType_t type;
	uint32_t layers;
	if ( !r || !a || r->schemaVersion != RAL_TEXTURE_RESOURCE_RECEIPT_SCHEMA_VERSION
			|| r->backendType < RAL_BACKEND_VULKAN || r->backendType >= RAL_BACKEND_COUNT
			|| !r->textureIdentity || !r->resourceGeneration || r->ready != qtrue
			|| !( r->usage & RAL_TEXTURE_USAGE_SAMPLED ) ) return qfalse;
	switch ( a->dimension ) {
	case RAL_TEXTURE_ASSET_2D: type = RAL_TEXTURE_2D; break;
	case RAL_TEXTURE_ASSET_2D_ARRAY: type = RAL_TEXTURE_2D_ARRAY; break;
	case RAL_TEXTURE_ASSET_CUBE: type = a->layers == 6u ? RAL_TEXTURE_CUBE
		: RAL_TEXTURE_CUBE_ARRAY; break;
	case RAL_TEXTURE_ASSET_3D: type = RAL_TEXTURE_3D; break;
	default: return qfalse;
	}
	layers = type == RAL_TEXTURE_3D ? 1u : a->layers;
	return r->type == type && r->format == a->targetFormat
		&& r->width == a->width && r->height == a->height
		&& r->mipLevels == a->targetMipLevels && r->arrayLayers == layers;
}

static qboolean Build( const ralMaterialReceipt_t *material,
		const ralTextureResourceReceipt_t *resources, uint32_t count,
		uint64_t generation, ralMaterialRuntimePlan_t *out ) {
	ralMaterialRuntimePlan_t v;
	uint32_t i, j;
	if ( !out || !generation || !Ral_MaterialReceiptValid( material )
			|| count != material->material.textureCount
			|| ( count && !resources ) || count > RAL_MATERIAL_MAX_TEXTURES ) return qfalse;
	memset( &v, 0, sizeof( v ) ); v.schemaVersion = RAL_MATERIAL_RUNTIME_SCHEMA_VERSION;
	v.runtimeGeneration = generation; v.material = *material; v.bindingCount = count;
	for ( i = 0u; i < count; i++ ) {
		const ralMaterialTexture_t *texture = &material->material.textures[i];
		if ( !ResourceMatches( &resources[i], &texture->texture.asset ) ) return qfalse;
		if ( i && resources[i].backendType != resources[0].backendType ) return qfalse;
		for ( j = 0u; j < i; j++ )
			if ( resources[j].textureIdentity == resources[i].textureIdentity ) return qfalse;
		v.bindings[i].bindingSlot = (uint32_t)texture->role - 1u;
		v.bindings[i].role = texture->role;
		v.bindings[i].cacheKeyHash = texture->texture.keyHash;
		v.bindings[i].resource = resources[i];
	}
	v.ready = qtrue; *out = v; return qtrue;
}

qboolean Ral_MaterialRuntimePlanBuild( const ralMaterialReceipt_t *material,
		const ralTextureResourceReceipt_t *resources, uint32_t count,
		uint64_t generation, ralMaterialRuntimePlan_t *out ) {
	ralMaterialRuntimePlan_t v;
	if ( !out || !Build( material, resources, count, generation, &v ) ) return qfalse;
	*out = v; return qtrue;
}

qboolean Ral_MaterialRuntimePlanValid( const ralMaterialRuntimePlan_t *p ) {
	ralMaterialRuntimePlan_t expected;
	ralTextureResourceReceipt_t resources[RAL_MATERIAL_MAX_TEXTURES];
	uint32_t i;
	if ( !p || p->schemaVersion != RAL_MATERIAL_RUNTIME_SCHEMA_VERSION
			|| p->ready != qtrue || p->bindingCount > RAL_MATERIAL_MAX_TEXTURES ) return qfalse;
	for ( i = 0u; i < p->bindingCount; i++ ) resources[i] = p->bindings[i].resource;
	return Build( &p->material, resources, p->bindingCount,
		p->runtimeGeneration, &expected ) && !memcmp( p, &expected, sizeof( *p ) );
}

qboolean Ral_MaterialRuntimePlanExact( const ralMaterialRuntimePlan_t *a,
		const ralMaterialRuntimePlan_t *b ) {
	return Ral_MaterialRuntimePlanValid( a ) && Ral_MaterialRuntimePlanValid( b )
		&& !memcmp( a, b, sizeof( *a ) );
}

qboolean Ral_MaterialRuntimeCohortValid( const ralMaterialRuntimeCohort_t *c ) {
	return c && c->schemaVersion == RAL_MATERIAL_RUNTIME_COHORT_SCHEMA_VERSION
		&& c->cohortGeneration && c->dependencyGeneration
		&& ( c->state == RAL_MATERIAL_RUNTIME_READY
			|| c->state == RAL_MATERIAL_RUNTIME_INVALIDATED )
		&& Ral_MaterialRuntimePlanValid( &c->plan );
}

qboolean Ral_MaterialRuntimeCohortInit( const ralMaterialRuntimePlan_t *plan,
		uint64_t cohortGeneration, uint64_t dependencyGeneration,
		ralMaterialRuntimeCohort_t *out ) {
	ralMaterialRuntimeCohort_t v;
	if ( !out || !cohortGeneration || !dependencyGeneration
			|| !Ral_MaterialRuntimePlanValid( plan ) ) return qfalse;
	memset( &v, 0, sizeof( v ) );
	v.schemaVersion = RAL_MATERIAL_RUNTIME_COHORT_SCHEMA_VERSION;
	v.cohortGeneration = cohortGeneration; v.dependencyGeneration = dependencyGeneration;
	v.state = RAL_MATERIAL_RUNTIME_READY; v.plan = *plan;
	*out = v; return qtrue;
}

qboolean Ral_MaterialRuntimeInvalidate( ralMaterialRuntimeCohort_t *c,
		const ralMaterialDependencyEvent_t *e, qboolean *outInvalidated ) {
	ralMaterialRuntimeCohort_t v;
	qboolean matches = qfalse;
	uint32_t i;
	if ( !outInvalidated || !Ral_MaterialRuntimeCohortValid( c ) || !e
			|| e->kind < RAL_MATERIAL_DEPENDENCY_ARTIFACT
			|| e->kind > RAL_MATERIAL_DEPENDENCY_TEXTURE
			|| e->dependencyGeneration <= c->dependencyGeneration
			|| !e->previousGeneration || !e->nextGeneration
			|| e->previousGeneration == e->nextGeneration ) return qfalse;
	if ( e->kind == RAL_MATERIAL_DEPENDENCY_ARTIFACT ) {
		if ( !e->artifactHash || e->textureIdentity ) return qfalse;
		matches = e->artifactHash == c->plan.material.artifactHash
			&& e->previousGeneration == c->plan.material.material.materialGeneration;
	} else {
		if ( e->artifactHash || !e->textureIdentity ) return qfalse;
		for ( i = 0u; i < c->plan.bindingCount; i++ )
			if ( c->plan.bindings[i].resource.textureIdentity == e->textureIdentity
					&& c->plan.bindings[i].resource.resourceGeneration
						== e->previousGeneration ) matches = qtrue;
	}
	if ( !matches ) { *outInvalidated = qfalse; return qtrue; }
	if ( c->state != RAL_MATERIAL_RUNTIME_READY ) return qfalse;
	v = *c; v.state = RAL_MATERIAL_RUNTIME_INVALIDATED;
	v.dependencyGeneration = e->dependencyGeneration;
	*c = v; *outInvalidated = qtrue; return qtrue;
}
