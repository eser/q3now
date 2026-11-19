// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#ifndef WIRED_RENDER_MATERIAL_PRODUCT_H
#define WIRED_RENDER_MATERIAL_PRODUCT_H

#include "../ral/core/ral_material_cook_cache.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RENDER_MATERIAL_PRODUCT_SCHEMA_VERSION 1u

typedef enum {
	RENDER_MATERIAL_PRODUCT_OK = 0,
	RENDER_MATERIAL_PRODUCT_INVALID_ARGUMENT,
	RENDER_MATERIAL_PRODUCT_BINDING_REJECTED,
	RENDER_MATERIAL_PRODUCT_COOK_REJECTED,
	RENDER_MATERIAL_PRODUCT_CACHE_STORE_REJECTED,
	RENDER_MATERIAL_PRODUCT_CACHE_LOAD_REJECTED,
	RENDER_MATERIAL_PRODUCT_ALLOCATION_REJECTED
} renderMaterialProductFailure_t;

typedef struct {
	renderMaterialProductFailure_t reason;
	uint32_t dependencyIndex;
	ralMaterialCookDiagnostic_t cook;
} renderMaterialProductDiagnostic_t;

typedef qboolean (*renderMaterialProductBindDependencyFn)( void *context,
	uint32_t dependencyIndex, const ralMaterialSourceDependency_t *dependency,
	ralMaterialCookBinding_t *outBinding );

/* DEV/offline authoring path. Source parsing happens before this boundary.
 * Every canonical dependency is bound once, the product is cooked, validated,
 * then atomically published to the content-addressed cache. */
qboolean RenderMaterialProduct_CookAndStore(
	const ralMaterialSourceReceipt_t *source,
	renderMaterialProductBindDependencyFn bindDependency, void *bindContext,
	const ralMaterialCookCacheOps_t *cacheOps, void *cacheContext,
	ralMaterialCookReceipt_t *outReceipt,
	renderMaterialProductDiagnostic_t *outDiagnostic );

/* Release/runtime path. Deliberately accepts neither source text nor source
 * IR: a build manifest supplies the expected artifact hash and only a fully
 * validated cooked product can cross this boundary. */
qboolean RenderMaterialProduct_LoadCooked( uint64_t expectedArtifactHash,
	const ralMaterialCookCacheOps_t *cacheOps, void *cacheContext,
	ralMaterialCookReceipt_t *outReceipt,
	renderMaterialProductDiagnostic_t *outDiagnostic );

#ifdef __cplusplus
}
#endif
#endif
