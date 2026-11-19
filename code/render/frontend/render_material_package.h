// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#ifndef WIRED_RENDER_MATERIAL_PACKAGE_H
#define WIRED_RENDER_MATERIAL_PACKAGE_H

#include "render_material_product.h"
#include "../ral/core/ral_material_package.h"
#include "../ral/core/ral_material_runtime.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	RENDER_MATERIAL_PACKAGE_OK = 0,
	RENDER_MATERIAL_PACKAGE_INVALID_ARGUMENT,
	RENDER_MATERIAL_PACKAGE_PRODUCT_REJECTED,
	RENDER_MATERIAL_PACKAGE_MANIFEST_REJECTED,
	RENDER_MATERIAL_PACKAGE_MANIFEST_STORE_REJECTED,
	RENDER_MATERIAL_PACKAGE_PROJECT_REFERENCE_LOAD_REJECTED,
	RENDER_MATERIAL_PACKAGE_MANIFEST_LOAD_REJECTED,
	RENDER_MATERIAL_PACKAGE_MATERIAL_NOT_FOUND,
	RENDER_MATERIAL_PACKAGE_MATERIAL_LOAD_REJECTED,
	RENDER_MATERIAL_PACKAGE_RUNTIME_PLAN_REJECTED,
	RENDER_MATERIAL_PACKAGE_ALLOCATION_REJECTED
} renderMaterialPackageFailure_t;

typedef struct {
	renderMaterialPackageFailure_t reason;
	uint32_t itemIndex;
	renderMaterialProductDiagnostic_t product;
	ralMaterialRuntimeBuildDiagnostic_t runtime;
} renderMaterialPackageDiagnostic_t;

typedef qboolean (*renderMaterialRuntimeResolveResourceFn)( void *context,
	uint64_t cacheKeyHash, ralMaterialRuntimeResource_t *outResource );

typedef struct {
	char semanticName[MAX_QPATH];
	ralMaterialRuntimePlan_t plan;
} renderMaterialRuntimeCatalogEntry_t;

typedef struct {
	uint32_t schemaVersion;
	uint32_t entryCount;
	uint64_t runtimeGeneration;
	uint64_t manifestArtifactHash;
	renderMaterialRuntimeCatalogEntry_t *entries;
	qboolean ready;
} renderMaterialRuntimeCatalog_t;

#define RENDER_MATERIAL_RUNTIME_CATALOG_SCHEMA_VERSION 1u

/* Offline package boundary. Individual content-addressed blobs may be written
 * before a later item fails, but no manifest is published until every item is
 * valid. Such unreachable blobs are immutable cache entries, not partial
 * package state. */
qboolean RenderMaterialPackage_CookAndPublish( uint64_t generation,
	const ralMaterialSourceReceipt_t *sources, uint32_t sourceCount,
	renderMaterialProductBindDependencyFn bindDependency, void *bindContext,
	const ralMaterialCookCacheOps_t *materialCacheOps,
	void *materialCacheContext,
	const ralMaterialPackageCacheOps_t *packageCacheOps,
	void *packageCacheContext,
	ralMaterialPackageReceipt_t *outManifest,
	renderMaterialPackageDiagnostic_t *outDiagnostic );

/* Release/runtime boundary: expected manifest hash + semantic material name
 * are the only selectors. Source IR, text and Lua cannot enter this path. */
qboolean RenderMaterialPackage_LoadCooked( uint64_t expectedManifestHash,
	const char *semanticName,
	const ralMaterialPackageCacheOps_t *packageCacheOps,
	void *packageCacheContext,
	const ralMaterialCookCacheOps_t *materialCacheOps,
	void *materialCacheContext,
	ralMaterialCookReceipt_t *outReceipt,
	renderMaterialPackageDiagnostic_t *outDiagnostic );

/* Shipping selector chain. The stable map identity resolves a validated
 * project reference, which is the only authority for the package hash; the
 * semantic name then resolves the immutable cooked material. No authoring
 * source, text or Lua surface exists on this path. */
qboolean RenderMaterialPackage_LoadProjectCooked( const char *mapName,
	uint32_t mapChecksum, const char *semanticName,
	const ralMaterialProjectReferenceOps_t *projectReferenceOps,
	void *projectReferenceContext,
	const ralMaterialPackageCacheOps_t *packageCacheOps,
	void *packageCacheContext,
	const ralMaterialCookCacheOps_t *materialCacheOps,
	void *materialCacheContext,
	ralMaterialProjectReferenceReceipt_t *outProjectReference,
	ralMaterialCookReceipt_t *outReceipt,
	renderMaterialPackageDiagnostic_t *outDiagnostic );

/* Complete backend-neutral Release consumer boundary. The stable project
 * selector loads one validated cooked product, then cache-keyed resident
 * receipts for every cooked texture dependency (PBR slots, Q3 stages and sky
 * faces alike) become a portable runtime plan. No backend handle is created
 * or retained here. */
qboolean RenderMaterialPackage_LoadProjectRuntimePlan( const char *mapName,
	uint32_t mapChecksum, const char *semanticName,
	const ralMaterialProjectReferenceOps_t *projectReferenceOps,
	void *projectReferenceContext,
	const ralMaterialPackageCacheOps_t *packageCacheOps,
	void *packageCacheContext,
	const ralMaterialCookCacheOps_t *materialCacheOps,
	void *materialCacheContext,
	const ralMaterialRuntimeResource_t *resources, uint32_t resourceCount,
	uint64_t runtimeGeneration,
	ralMaterialProjectReferenceReceipt_t *outProjectReference,
	ralMaterialRuntimePlan_t *outPlan,
	renderMaterialPackageDiagnostic_t *outDiagnostic );

/* Production map cutover: load the project/package once, materialize every
 * cooked entry against the adapter's already-resident texture receipts, then
 * publish one immutable runtime catalog.  The caller swaps the completed
 * candidate into its per-world owner; failures leave the prior catalog intact. */
qboolean RenderMaterialPackage_LoadProjectRuntimeCatalog( const char *mapName,
	uint32_t mapChecksum,
	const ralMaterialProjectReferenceOps_t *projectReferenceOps,
	void *projectReferenceContext,
	const ralMaterialPackageCacheOps_t *packageCacheOps,
	void *packageCacheContext,
	const ralMaterialCookCacheOps_t *materialCacheOps,
	void *materialCacheContext,
	renderMaterialRuntimeResolveResourceFn resolveResource,
	void *resolveResourceContext, uint64_t runtimeGeneration,
	renderMaterialRuntimeCatalog_t *outCatalog,
	renderMaterialPackageDiagnostic_t *outDiagnostic );
void RenderMaterialRuntimeCatalog_Destroy(
	renderMaterialRuntimeCatalog_t *catalog );
const ralMaterialRuntimePlan_t *RenderMaterialRuntimeCatalog_Lookup(
	const renderMaterialRuntimeCatalog_t *catalog, const char *semanticName );
qboolean RenderMaterialRuntimeCatalog_ResolveTexture(
	const renderMaterialRuntimeCatalog_t *catalog, const char *semanticName,
	uint64_t cacheKeyHash, ralTextureResourceReceipt_t *outResource );

#ifdef __cplusplus
}
#endif
#endif
