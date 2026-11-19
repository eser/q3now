// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "render_material_product.h"

#include <stdlib.h>
#include <string.h>

static void Fail( renderMaterialProductDiagnostic_t *diagnostic,
		renderMaterialProductFailure_t reason, uint32_t dependencyIndex,
		const ralMaterialCookDiagnostic_t *cook ) {
	if ( !diagnostic ) return;
	memset( diagnostic, 0, sizeof( *diagnostic ) );
	diagnostic->reason = reason;
	diagnostic->dependencyIndex = dependencyIndex;
	if ( cook ) diagnostic->cook = *cook;
}

qboolean RenderMaterialProduct_CookAndStore(
		const ralMaterialSourceReceipt_t *source,
		renderMaterialProductBindDependencyFn bindDependency, void *bindContext,
		const ralMaterialCookCacheOps_t *cacheOps, void *cacheContext,
		ralMaterialCookReceipt_t *outReceipt,
		renderMaterialProductDiagnostic_t *outDiagnostic ) {
	ralMaterialCookBinding_t bindings[RAL_MATERIAL_COOK_MAX_BINDINGS];
	ralMaterialCookReceipt_t *candidate;
	ralMaterialCookDiagnostic_t cookDiagnostic;
	uint32_t dependencyCount;
	qboolean success = qfalse;
	if ( outDiagnostic ) memset( outDiagnostic, 0, sizeof( *outDiagnostic ) );
	if ( !source || !Ral_MaterialSourceReceiptValid( source )
			|| !bindDependency || !cacheOps || !cacheOps->writeAtomic
			|| !outReceipt ) {
		Fail( outDiagnostic, RENDER_MATERIAL_PRODUCT_INVALID_ARGUMENT, 0u, NULL );
		return qfalse;
	}
	dependencyCount = source->source.dependencyCount;
	if ( dependencyCount > RAL_MATERIAL_COOK_MAX_BINDINGS ) {
		Fail( outDiagnostic, RENDER_MATERIAL_PRODUCT_INVALID_ARGUMENT,
			dependencyCount, NULL );
		return qfalse;
	}
	memset( bindings, 0, sizeof( bindings ) );
	for ( uint32_t i = 0u; i < dependencyCount; ++i ) {
		if ( !bindDependency( bindContext, i, &source->source.dependencies[i],
				&bindings[i] ) ) {
			Fail( outDiagnostic, RENDER_MATERIAL_PRODUCT_BINDING_REJECTED,
				i, NULL );
			return qfalse;
		}
	}
	candidate = malloc( sizeof( *candidate ) );
	if ( !candidate ) {
		Fail( outDiagnostic, RENDER_MATERIAL_PRODUCT_ALLOCATION_REJECTED,
			0u, NULL );
		return qfalse;
	}
	memset( &cookDiagnostic, 0, sizeof( cookDiagnostic ) );
	if ( !Ral_MaterialCook( source, bindings, dependencyCount, candidate,
			&cookDiagnostic ) ) {
		Fail( outDiagnostic, RENDER_MATERIAL_PRODUCT_COOK_REJECTED,
			cookDiagnostic.itemIndex, &cookDiagnostic );
		goto done;
	}
	if ( !Ral_MaterialCookCacheStore( cacheOps, cacheContext, candidate ) ) {
		Fail( outDiagnostic, RENDER_MATERIAL_PRODUCT_CACHE_STORE_REJECTED,
			0u, NULL );
		goto done;
	}
	*outReceipt = *candidate;
	success = qtrue;
done:
	free( candidate );
	return success;
}

qboolean RenderMaterialProduct_LoadCooked( uint64_t expectedArtifactHash,
		const ralMaterialCookCacheOps_t *cacheOps, void *cacheContext,
		ralMaterialCookReceipt_t *outReceipt,
		renderMaterialProductDiagnostic_t *outDiagnostic ) {
	if ( outDiagnostic ) memset( outDiagnostic, 0, sizeof( *outDiagnostic ) );
	if ( !expectedArtifactHash || !cacheOps || !cacheOps->read || !outReceipt ) {
		Fail( outDiagnostic, RENDER_MATERIAL_PRODUCT_INVALID_ARGUMENT, 0u, NULL );
		return qfalse;
	}
	if ( !Ral_MaterialCookCacheLoad( cacheOps, cacheContext,
			expectedArtifactHash, outReceipt ) ) {
		Fail( outDiagnostic, RENDER_MATERIAL_PRODUCT_CACHE_LOAD_REJECTED,
			0u, NULL );
		return qfalse;
	}
	return qtrue;
}
