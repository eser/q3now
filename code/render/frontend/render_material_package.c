// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "render_material_package.h"

#include <stdlib.h>
#include <string.h>

static void Fail( renderMaterialPackageDiagnostic_t *diagnostic,
		renderMaterialPackageFailure_t reason, uint32_t itemIndex,
		const renderMaterialProductDiagnostic_t *product ) {
	if ( !diagnostic ) return;
	memset( diagnostic, 0, sizeof( *diagnostic ) );
	diagnostic->reason = reason;
	diagnostic->itemIndex = itemIndex;
	if ( product ) diagnostic->product = *product;
}

qboolean RenderMaterialPackage_CookAndPublish( uint64_t generation,
		const ralMaterialSourceReceipt_t *sources, uint32_t sourceCount,
		renderMaterialProductBindDependencyFn bindDependency, void *bindContext,
		const ralMaterialCookCacheOps_t *materialCacheOps,
		void *materialCacheContext,
		const ralMaterialPackageCacheOps_t *packageCacheOps,
		void *packageCacheContext,
		ralMaterialPackageReceipt_t *outManifest,
		renderMaterialPackageDiagnostic_t *outDiagnostic ) {
	ralMaterialPackageEntry_t *entries;
	ralMaterialPackageReceipt_t *candidate;
	ralMaterialCookReceipt_t *cooked;
	renderMaterialProductDiagnostic_t product;
	qboolean success = qfalse;
	if ( outDiagnostic ) memset( outDiagnostic, 0, sizeof( *outDiagnostic ) );
	if ( !generation || !sources || !sourceCount
			|| sourceCount > RAL_MATERIAL_PACKAGE_MAX_ENTRIES
			|| !bindDependency || !materialCacheOps
			|| !packageCacheOps || !packageCacheOps->writeAtomic
			|| !outManifest ) {
		Fail( outDiagnostic, RENDER_MATERIAL_PACKAGE_INVALID_ARGUMENT, 0u, NULL );
		return qfalse;
	}
	entries = calloc( RAL_MATERIAL_PACKAGE_MAX_ENTRIES, sizeof( *entries ) );
	candidate = malloc( sizeof( *candidate ) );
	cooked = malloc( sizeof( *cooked ) );
	if ( !entries || !candidate || !cooked ) {
		Fail( outDiagnostic, RENDER_MATERIAL_PACKAGE_ALLOCATION_REJECTED,
			0u, NULL );
		goto done;
	}
	for ( uint32_t i = 0u; i < sourceCount; ++i ) {
		if ( !RenderMaterialProduct_CookAndStore( &sources[i], bindDependency,
				bindContext, materialCacheOps, materialCacheContext, cooked,
				&product ) ) {
			Fail( outDiagnostic, RENDER_MATERIAL_PACKAGE_PRODUCT_REJECTED,
				i, &product );
			goto done;
		}
		if ( !Ral_MaterialPackageEntryFromCook( cooked, &entries[i] ) ) {
			Fail( outDiagnostic, RENDER_MATERIAL_PACKAGE_MANIFEST_REJECTED,
				i, NULL );
			goto done;
		}
	}
	if ( !Ral_MaterialPackageSeal( generation, entries, sourceCount,
			candidate ) ) {
		Fail( outDiagnostic, RENDER_MATERIAL_PACKAGE_MANIFEST_REJECTED,
			sourceCount, NULL );
		goto done;
	}
	if ( !Ral_MaterialPackageCacheStore( packageCacheOps,
			packageCacheContext, candidate ) ) {
		Fail( outDiagnostic, RENDER_MATERIAL_PACKAGE_MANIFEST_STORE_REJECTED,
			sourceCount, NULL );
		goto done;
	}
	*outManifest = *candidate;
	success = qtrue;
done:
	free( cooked );
	free( candidate );
	free( entries );
	return success;
}

qboolean RenderMaterialPackage_LoadCooked( uint64_t expectedManifestHash,
		const char *semanticName,
		const ralMaterialPackageCacheOps_t *packageCacheOps,
		void *packageCacheContext,
		const ralMaterialCookCacheOps_t *materialCacheOps,
		void *materialCacheContext,
		ralMaterialCookReceipt_t *outReceipt,
		renderMaterialPackageDiagnostic_t *outDiagnostic ) {
	ralMaterialPackageReceipt_t *manifest;
	ralMaterialPackageEntry_t entry;
	renderMaterialProductDiagnostic_t product;
	qboolean success = qfalse;
	if ( outDiagnostic ) memset( outDiagnostic, 0, sizeof( *outDiagnostic ) );
	if ( !expectedManifestHash || !semanticName || !semanticName[0]
			|| !packageCacheOps || !materialCacheOps || !outReceipt ) {
		Fail( outDiagnostic, RENDER_MATERIAL_PACKAGE_INVALID_ARGUMENT, 0u, NULL );
		return qfalse;
	}
	manifest = malloc( sizeof( *manifest ) );
	if ( !manifest ) {
		Fail( outDiagnostic, RENDER_MATERIAL_PACKAGE_ALLOCATION_REJECTED,
			0u, NULL );
		return qfalse;
	}
	if ( !Ral_MaterialPackageCacheLoad( packageCacheOps, packageCacheContext,
			expectedManifestHash, manifest ) ) {
		Fail( outDiagnostic, RENDER_MATERIAL_PACKAGE_MANIFEST_LOAD_REJECTED,
			0u, NULL );
		goto done;
	}
	if ( !Ral_MaterialPackageLookup( manifest, semanticName, &entry ) ) {
		Fail( outDiagnostic, RENDER_MATERIAL_PACKAGE_MATERIAL_NOT_FOUND,
			0u, NULL );
		goto done;
	}
	if ( !RenderMaterialProduct_LoadCooked( entry.cookedArtifactHash,
			materialCacheOps, materialCacheContext, outReceipt, &product ) ) {
		Fail( outDiagnostic, RENDER_MATERIAL_PACKAGE_MATERIAL_LOAD_REJECTED,
			0u, &product );
		goto done;
	}
	success = qtrue;
done:
	free( manifest );
	return success;
}

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
		renderMaterialPackageDiagnostic_t *outDiagnostic ) {
	ralMaterialProjectReferenceReceipt_t project;
	ralMaterialCookReceipt_t *material;
	renderMaterialPackageDiagnostic_t diagnostic;
	qboolean success = qfalse;
	if ( outDiagnostic ) memset( outDiagnostic, 0, sizeof( *outDiagnostic ) );
	if ( !mapName || !mapName[0] || !mapChecksum || !semanticName
			|| !semanticName[0] || !projectReferenceOps || !packageCacheOps
			|| !materialCacheOps || !outReceipt ) {
		Fail( outDiagnostic, RENDER_MATERIAL_PACKAGE_INVALID_ARGUMENT, 0u, NULL );
		return qfalse;
	}
	material = malloc( sizeof( *material ) );
	if ( !material ) {
		Fail( outDiagnostic, RENDER_MATERIAL_PACKAGE_ALLOCATION_REJECTED,
			0u, NULL );
		return qfalse;
	}
	if ( !Ral_MaterialProjectReferenceLoad( projectReferenceOps,
			projectReferenceContext, mapName, mapChecksum, &project ) ) {
		Fail( outDiagnostic,
			RENDER_MATERIAL_PACKAGE_PROJECT_REFERENCE_LOAD_REJECTED, 0u, NULL );
		goto done;
	}
	memset( &diagnostic, 0, sizeof( diagnostic ) );
	if ( !RenderMaterialPackage_LoadCooked( project.manifestArtifactHash,
			semanticName, packageCacheOps, packageCacheContext,
			materialCacheOps, materialCacheContext, material, &diagnostic ) ) {
		if ( outDiagnostic ) *outDiagnostic = diagnostic;
		goto done;
	}
	if ( outProjectReference ) *outProjectReference = project;
	*outReceipt = *material;
	if ( outDiagnostic ) outDiagnostic->reason = RENDER_MATERIAL_PACKAGE_OK;
	success = qtrue;
done:
	free( material );
	return success;
}

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
		renderMaterialPackageDiagnostic_t *outDiagnostic ) {
	ralMaterialProjectReferenceReceipt_t project;
	ralMaterialCookReceipt_t *material;
	ralMaterialRuntimePlan_t plan;
	renderMaterialPackageDiagnostic_t packageDiagnostic;
	ralMaterialRuntimeBuildDiagnostic_t runtimeDiagnostic;
	qboolean success = qfalse;
	if ( outDiagnostic ) memset( outDiagnostic, 0, sizeof( *outDiagnostic ) );
	if ( !outPlan || !runtimeGeneration ) {
		Fail( outDiagnostic, RENDER_MATERIAL_PACKAGE_INVALID_ARGUMENT, 0u, NULL );
		return qfalse;
	}
	material = malloc( sizeof( *material ) );
	if ( !material ) {
		Fail( outDiagnostic, RENDER_MATERIAL_PACKAGE_ALLOCATION_REJECTED,
			0u, NULL ); return qfalse;
	}
	memset( &packageDiagnostic, 0, sizeof( packageDiagnostic ) );
	if ( !RenderMaterialPackage_LoadProjectCooked( mapName, mapChecksum,
			semanticName, projectReferenceOps, projectReferenceContext,
			packageCacheOps, packageCacheContext, materialCacheOps,
			materialCacheContext, &project, material, &packageDiagnostic ) ) {
		if ( outDiagnostic ) *outDiagnostic = packageDiagnostic;
		goto done;
	}
	memset( &runtimeDiagnostic, 0, sizeof( runtimeDiagnostic ) );
	if ( !Ral_MaterialRuntimePlanBuildCooked( material, resources,
			resourceCount, runtimeGeneration, &plan, &runtimeDiagnostic ) ) {
		Fail( outDiagnostic, RENDER_MATERIAL_PACKAGE_RUNTIME_PLAN_REJECTED,
			runtimeDiagnostic.itemIndex, NULL );
		if ( outDiagnostic ) outDiagnostic->runtime = runtimeDiagnostic;
		goto done;
	}
	if ( outProjectReference ) *outProjectReference = project;
	*outPlan = plan;
	if ( outDiagnostic ) outDiagnostic->reason = RENDER_MATERIAL_PACKAGE_OK;
	success = qtrue;
done:
	free( material );
	return success;
}

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
		renderMaterialPackageDiagnostic_t *outDiagnostic ) {
	ralMaterialProjectReferenceReceipt_t project;
	ralMaterialPackageReceipt_t *manifest = NULL;
	ralMaterialCookReceipt_t *cooked = NULL;
	renderMaterialRuntimeCatalogEntry_t *entries = NULL;
	renderMaterialRuntimeCatalog_t candidate;
	renderMaterialProductDiagnostic_t productDiagnostic;
	ralMaterialRuntimeBuildDiagnostic_t runtimeDiagnostic;
	qboolean success = qfalse;
	if ( outDiagnostic ) memset( outDiagnostic, 0, sizeof( *outDiagnostic ) );
	if ( !mapName || !mapName[0] || !mapChecksum || !projectReferenceOps
			|| !packageCacheOps || !materialCacheOps || !resolveResource
			|| !runtimeGeneration || !outCatalog
			|| runtimeGeneration > UINT64_MAX
				- RAL_MATERIAL_PACKAGE_MAX_ENTRIES ) {
		Fail( outDiagnostic, RENDER_MATERIAL_PACKAGE_INVALID_ARGUMENT, 0u, NULL );
		return qfalse;
	}
	if ( !Ral_MaterialProjectReferenceLoad( projectReferenceOps,
			projectReferenceContext, mapName, mapChecksum, &project ) ) {
		Fail( outDiagnostic,
			RENDER_MATERIAL_PACKAGE_PROJECT_REFERENCE_LOAD_REJECTED, 0u, NULL );
		return qfalse;
	}
	manifest = malloc( sizeof( *manifest ) );
	cooked = malloc( sizeof( *cooked ) );
	if ( !manifest || !cooked ) {
		Fail( outDiagnostic, RENDER_MATERIAL_PACKAGE_ALLOCATION_REJECTED,
			0u, NULL ); goto done;
	}
	if ( !Ral_MaterialPackageCacheLoad( packageCacheOps, packageCacheContext,
			project.manifestArtifactHash, manifest )
			|| manifest->entryCount != project.materialCount ) {
		Fail( outDiagnostic, RENDER_MATERIAL_PACKAGE_MANIFEST_LOAD_REJECTED,
			0u, NULL ); goto done;
	}
	entries = calloc( manifest->entryCount, sizeof( *entries ) );
	if ( !entries ) {
		Fail( outDiagnostic, RENDER_MATERIAL_PACKAGE_ALLOCATION_REJECTED,
			0u, NULL ); goto done;
	}
	for ( uint32_t i = 0u; i < manifest->entryCount; ++i ) {
		ralMaterialRuntimeResource_t resources[RAL_MATERIAL_COOK_MAX_BINDINGS];
		uint32_t resourceCount = 0u;
		const ralMaterialPackageEntry_t *packageEntry = &manifest->entries[i];
		memset( &productDiagnostic, 0, sizeof( productDiagnostic ) );
		if ( !RenderMaterialProduct_LoadCooked(
				packageEntry->cookedArtifactHash, materialCacheOps,
				materialCacheContext, cooked, &productDiagnostic ) ) {
			Fail( outDiagnostic,
				RENDER_MATERIAL_PACKAGE_MATERIAL_LOAD_REJECTED, i,
				&productDiagnostic ); goto done;
		}
		if ( strcmp( packageEntry->semanticName,
				cooked->source.source.semanticName ) ) {
			Fail( outDiagnostic,
				RENDER_MATERIAL_PACKAGE_MATERIAL_LOAD_REJECTED, i, NULL );
			goto done;
		}
		memset( resources, 0, sizeof( resources ) );
		for ( uint32_t dependency = 0u;
				dependency < cooked->bindingCount; ++dependency ) {
			const ralMaterialCookBinding_t *binding =
				&cooked->bindings[dependency];
			uint64_t key;
			if ( binding->role != RAL_MATERIAL_DEPENDENCY_TEXTURE ) continue;
			key = binding->texture.keyHash;
			if ( !resolveResource( resolveResourceContext, key,
					&resources[resourceCount] ) ) {
				Fail( outDiagnostic,
					RENDER_MATERIAL_PACKAGE_RUNTIME_PLAN_REJECTED, i, NULL );
				if ( outDiagnostic ) {
					outDiagnostic->runtime.reason =
						RAL_MATERIAL_RUNTIME_BUILD_MISSING_RESOURCE;
					outDiagnostic->runtime.itemIndex = dependency;
					outDiagnostic->runtime.cacheKeyHash = key;
				}
				goto done;
			}
			resourceCount++;
		}
		memset( &runtimeDiagnostic, 0, sizeof( runtimeDiagnostic ) );
		if ( !Ral_MaterialRuntimePlanBuildCooked( cooked, resources,
				resourceCount, runtimeGeneration + i,
				&entries[i].plan, &runtimeDiagnostic ) ) {
			Fail( outDiagnostic,
				RENDER_MATERIAL_PACKAGE_RUNTIME_PLAN_REJECTED, i, NULL );
			if ( outDiagnostic ) outDiagnostic->runtime = runtimeDiagnostic;
			goto done;
		}
		memcpy( entries[i].semanticName, packageEntry->semanticName,
			sizeof( entries[i].semanticName ) );
	}
	memset( &candidate, 0, sizeof( candidate ) );
	candidate.schemaVersion = RENDER_MATERIAL_RUNTIME_CATALOG_SCHEMA_VERSION;
	candidate.entryCount = manifest->entryCount;
	candidate.runtimeGeneration = runtimeGeneration;
	candidate.manifestArtifactHash = manifest->artifactHash;
	candidate.entries = entries;
	candidate.ready = qtrue;
	*outCatalog = candidate;
	entries = NULL;
	if ( outDiagnostic ) outDiagnostic->reason = RENDER_MATERIAL_PACKAGE_OK;
	success = qtrue;
done:
	free( entries );
	free( cooked );
	free( manifest );
	return success;
}
