// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "render_material_package.h"

#include <stdlib.h>
#include <string.h>

void RenderMaterialRuntimeCatalog_Destroy(
		renderMaterialRuntimeCatalog_t *catalog ) {
	if ( !catalog ) return;
	free( catalog->entries );
	memset( catalog, 0, sizeof( *catalog ) );
}

const ralMaterialRuntimePlan_t *RenderMaterialRuntimeCatalog_Lookup(
		const renderMaterialRuntimeCatalog_t *catalog,
		const char *semanticName ) {
	uint32_t first, count;
	if ( !catalog || catalog->schemaVersion
			!= RENDER_MATERIAL_RUNTIME_CATALOG_SCHEMA_VERSION
			|| catalog->ready != qtrue || !catalog->entries
			|| !semanticName || !semanticName[0] ) return NULL;
	first = 0u; count = catalog->entryCount;
	while ( count ) {
		uint32_t step = count / 2u;
		uint32_t index = first + step;
		int compare = strcmp( catalog->entries[index].semanticName,
			semanticName );
		if ( compare < 0 ) { first = index + 1u; count -= step + 1u; }
		else if ( compare > 0 ) count = step;
		else return &catalog->entries[index].plan;
	}
	return NULL;
}

qboolean RenderMaterialRuntimeCatalog_ResolveTexture(
		const renderMaterialRuntimeCatalog_t *catalog, const char *semanticName,
		uint64_t cacheKeyHash, ralTextureResourceReceipt_t *outResource ) {
	const ralMaterialRuntimePlan_t *plan;
	const ralTextureResourceReceipt_t *match = NULL;
	if ( !cacheKeyHash || !outResource ) return qfalse;
	plan = RenderMaterialRuntimeCatalog_Lookup( catalog, semanticName );
	if ( !plan || !Ral_MaterialRuntimePlanValid( plan ) ) return qfalse;
	for ( uint32_t i = 0u; i < plan->dependencyCount; ++i ) {
		const ralMaterialRuntimeDependency_t *dependency = &plan->dependencies[i];
		if ( dependency->hasTextureResource != qtrue
				|| dependency->cacheKeyHash != cacheKeyHash ) continue;
		if ( match || !Ral_TextureResourceReceiptValid(
				&dependency->textureResource ) )
			return qfalse;
		match = &dependency->textureResource;
	}
	if ( !plan->dependencyCount ) {
		for ( uint32_t i = 0u; i < plan->bindingCount; ++i ) {
			const ralMaterialRuntimeBinding_t *binding = &plan->bindings[i];
			if ( binding->cacheKeyHash != cacheKeyHash ) continue;
			if ( match || !Ral_TextureResourceReceiptValid( &binding->resource ) )
				return qfalse;
			match = &binding->resource;
		}
	}
	if ( !match ) return qfalse;
	*outResource = *match;
	return qtrue;
}
