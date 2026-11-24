// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "render_material_project_cook.h"
#include "render_q1_miptex.h"
#include "render_world_material_usage.h"

#include "render_image_decode.h"
#include "render_material_product.h"
#include "render_submission.h"
#include "maps/map_format_registry.h"
#include "material_cook_fs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define FNV64_OFFSET UINT64_C(14695981039346656037)
#define FNV64_PRIME UINT64_C(1099511628211)

typedef struct {
	const refimport_t *imports;
	const ralMaterialSourceReceipt_t *source;
} projectBinder_t;

static uint64_t HashU64( uint64_t hash, uint64_t value ) {
	for ( uint32_t i = 0u; i < 8u; ++i ) {
		hash ^= (unsigned char)( value >> ( i * 8u ) );
		hash *= FNV64_PRIME;
	}
	return hash;
}

static qboolean ContainsU64( const uint64_t *values, uint32_t count,
		uint64_t value ) {
	for ( uint32_t i = 0u; i < count; ++i )
		if ( values[i] == value ) return qtrue;
	return qfalse;
}

static qboolean ContainsLegacyVariant(
		const ralLegacyMaterialReceipt_t *values, uint32_t count,
		const ralLegacyMaterialReceipt_t *value ) {
	for ( uint32_t i = 0u; i < count; ++i )
		if ( Ral_LegacyMaterialReceiptExact( &values[i], value ) ) return qtrue;
	return qfalse;
}

static void Fail( renderMaterialProjectCookDiagnostic_t *diagnostic,
		renderMaterialProjectCookFailure_t reason, uint32_t shaderIndex,
		uint32_t dependencyIndex ) {
	if ( !diagnostic ) return;
	memset( diagnostic, 0, sizeof( *diagnostic ) );
	diagnostic->reason = reason;
	diagnostic->shaderIndex = shaderIndex;
	diagnostic->dependencyIndex = dependencyIndex;
}

static qboolean BindDependency( void *opaque, uint32_t dependencyIndex,
		const ralMaterialSourceDependency_t *dependency,
		ralMaterialCookBinding_t *outBinding ) {
	projectBinder_t *binder = (projectBinder_t *)opaque;
	ralTextureCacheKey_t textureKey;
	byte *pixels = NULL;
	uint32_t width = 0u, height = 0u;
	char resolved[MAX_QPATH];
	qboolean success = qfalse;
	if ( !binder || !binder->imports || !binder->source || !dependency
			|| !outBinding || dependencyIndex >=
				binder->source->source.dependencyCount
			|| dependency->role != RAL_MATERIAL_DEPENDENCY_TEXTURE
			|| !binder->imports->Free ) return qfalse;
	if ( !RenderImage_DecodeRgba8( dependency->canonicalPath, &pixels,
			&width, &height, resolved ) || !pixels || !width || !height )
		return qfalse;
	if ( Ral_MaterialRuntimeTextureKeyBuild( dependency->sourceId,
			dependency->fsGeneration, pixels, width, height, qtrue,
			binder->source->source.noMipMaps, RAL_TEXTURE_RESIDENCY_STREAMED,
			&textureKey ) ) {
		memset( outBinding, 0, sizeof( *outBinding ) );
		outBinding->dependencyIndex = dependencyIndex;
		outBinding->role = dependency->role;
		outBinding->sourceId = dependency->sourceId;
		outBinding->fsGeneration = dependency->fsGeneration;
		outBinding->creationOptionsHash = dependency->creationOptionsHash;
		outBinding->texture = textureKey;
		if ( Ral_TextureCacheKeyValid( &outBinding->texture ) ) {
			outBinding->artifactHash = outBinding->texture.keyHash;
			success = qtrue;
		}
	}
	binder->imports->Free( pixels );
	return success;
}

/* A texture-only material: one opaque stage drawing `imageName` (the material's own name, or a Quake 1 map's embedded reference). */
static qboolean ImplicitEntry( const dshader_t *shader, const char *imageName,
		const refimport_t *imports, renderMaterialScriptEntry_t *out ) {
	char canonical[MAX_QPATH], resolved[MAX_QPATH];
	byte *pixels = NULL;
	uint32_t width = 0u, height = 0u;
	uint64_t sourceId = 0u, size = 0u;
	unsigned fsGeneration = 0u;
	if ( !shader || !shader->shader[0] || !imports || !imports->FS_ResolveResource
			|| !imports->Free || !out ) return qfalse;
	if ( !imageName || !RenderImage_DecodeRgba8( imageName, &pixels, &width,
			&height, resolved ) || !pixels ) return qfalse;
	imports->Free( pixels );
	if ( !width || !height || !resolved[0] ) return qfalse;
	if ( !imports->FS_ResolveResource( resolved, canonical,
			sizeof( canonical ), &sourceId, &size, &fsGeneration ) ) return qfalse;
	memset( out, 0, sizeof( *out ) );
	(void)snprintf( out->name, sizeof( out->name ), "%s", shader->shader );
	(void)snprintf( out->declarationPath, sizeof( out->declarationPath ),
		"%s", canonical );
	out->declarationSourceId = sourceId;
	out->declarationSize = size;
	out->declarationGeneration = fsGeneration;
	out->declarationLine = out->declarationColumn = 1u;
	(void)snprintf( out->imageName, sizeof( out->imageName ), "%s",
		canonical );
	out->alphaMode = RENDER_ALPHA_OPAQUE;
	out->cullMode = RENDER_CULL_BACK;
	out->depthWrite = qtrue;
	out->sort = RENDER_MATERIAL_SORT_OPAQUE;
	out->surfaceFlags = shader->surfaceFlags;
	out->contentFlags = shader->contentFlags;
	out->stageCount = 1u;
	(void)snprintf( out->stages[0].imageName,
		sizeof( out->stages[0].imageName ), "%s", canonical );
	out->stages[0].imageSource = RENDER_MATERIAL_STAGE_IMAGE;
	out->stages[0].tcGen = RENDER_MATERIAL_TCGEN_TEXTURE;
	out->stages[0].sourceBlend = RENDER_MATERIAL_BLEND_ONE;
	out->stages[0].destinationBlend = RENDER_MATERIAL_BLEND_ZERO;
	out->stages[0].depthWrite = qtrue;
	out->stages[0].scale[0] = out->stages[0].scale[1] = 1.0f;
	out->stages[0].sourceLine = out->stages[0].sourceColumn = 1u;
	return qtrue;
}

qboolean RenderMaterialProjectCook_ReceiptValid(
		const renderMaterialProjectCookReceipt_t *receipt ) {
	return receipt
		&& receipt->schemaVersion == RENDER_MATERIAL_PROJECT_COOK_SCHEMA_VERSION
		&& receipt->generation && receipt->mapShaderCount
		&& receipt->cookedMaterialCount
		&& receipt->cookedMaterialCount <= receipt->mapShaderCount
		&& receipt->materialVariantCount
		&& receipt->materialVariantCount <= receipt->cookedMaterialCount
		&& receipt->legacyStageCount <= receipt->cookedMaterialCount
			* RAL_MATERIAL_SOURCE_MAX_STAGES
		&& receipt->legacyVariantCount <= receipt->legacyStageCount
		&& receipt->fallbackStageCount <= receipt->legacyStageCount
		&& receipt->proxyProgramCount <= receipt->cookedMaterialCount
		&& receipt->manifestArtifactHash && receipt->corpusHash
		&& receipt->projectReferenceArtifactHash && receipt->mapChecksum
		&& receipt->ready == qtrue ? qtrue : qfalse;
}

qboolean RenderMaterialProjectCook_LogTelemetry(
		const renderMaterialProjectCookReceipt_t *receipt,
		const refimport_t *imports ) {
	if ( !RenderMaterialProjectCook_ReceiptValid( receipt )
			|| !imports || !imports->Log ) return qfalse;
	imports->Log( SEV_INFO,
		"material project cook: shaders=%u products=%u variants=%u "
		"legacy-stages=%u legacy-variants=%u fallbacks=%u proxies=%u "
		"manifest=%016llx corpus=%016llx\n",
		receipt->mapShaderCount, receipt->cookedMaterialCount,
		receipt->materialVariantCount, receipt->legacyStageCount,
		receipt->legacyVariantCount, receipt->fallbackStageCount,
		receipt->proxyProgramCount,
		(unsigned long long)receipt->manifestArtifactHash,
		(unsigned long long)receipt->corpusHash );
	return qtrue;
}

qboolean RenderMaterialProjectCook_Execute(
		const renderMaterialScriptCatalog_t *catalog, const mapFile_t *map,
		const refimport_t *imports, const char *derivedRoot, uint64_t generation,
		renderMaterialProjectCookReceipt_t *outReceipt,
		renderMaterialProjectCookDiagnostic_t *outDiagnostic ) {
	wiredMaterialCookFilesystem_t filesystem;
	ralMaterialPackageEntry_t *entries = NULL;
	ralMaterialSourceReceipt_t *source = NULL;
	ralMaterialCookReceipt_t *cooked = NULL;
	ralMaterialPackageReceipt_t *manifest = NULL;
	uint64_t *materialVariants = NULL;
	uint64_t *proxyPrograms = NULL;
	ralLegacyMaterialReceipt_t *legacyVariants = NULL;
	ralMaterialProjectReferenceReceipt_t projectReference;
	renderMaterialProjectCookReceipt_t result;
	projectBinder_t binder;
	uint32_t entryCount = 0u;
	uint32_t materialVariantCount = 0u;
	uint32_t legacyStageCount = 0u;
	uint32_t legacyVariantCount = 0u;
	uint32_t fallbackStageCount = 0u;
	uint32_t proxyProgramCount = 0u;
	qboolean success = qfalse;
	if ( outDiagnostic ) memset( outDiagnostic, 0, sizeof( *outDiagnostic ) );
	if ( !catalog || catalog->ready != qtrue || !map || !map->name[0]
			|| !map->checksum || map->numShaders <= 0
			|| !map->shaders || map->numSurfaces < 0 || map->numFogs < 0
			|| ( map->numSurfaces && !map->surfaces )
			|| ( map->numFogs && !map->fogs )
			|| !imports || !derivedRoot || !derivedRoot[0]
			|| !generation || !outReceipt ) {
		Fail( outDiagnostic, RENDER_MATERIAL_PROJECT_COOK_INVALID_ARGUMENT,
			0u, 0u ); return qfalse;
	}
	if ( (uint32_t)map->numShaders > RAL_MATERIAL_PACKAGE_MAX_ENTRIES ) {
		Fail( outDiagnostic, RENDER_MATERIAL_PROJECT_COOK_OVER_CAPACITY,
			(uint32_t)map->numShaders, 0u ); return qfalse;
	}
	/* Shader lumps retain compiler-only/unused rows. Resolve only material
	 * dependencies of actual surfaces and fog volumes, never every table row.
	 * This bounded load-time mask does not change collision data or rendering. */
	byte referenced[RAL_MATERIAL_PACKAGE_MAX_ENTRIES] = {0};
	if ( !RenderWorldMaterialUsage_Build( map, referenced,
			RAL_MATERIAL_PACKAGE_MAX_ENTRIES ) ) {
		Fail( outDiagnostic, RENDER_MATERIAL_PROJECT_COOK_INVALID_ARGUMENT,
			0u, 0u ); return qfalse;
	}
	entries = calloc( (size_t)map->numShaders, sizeof( *entries ) );
	materialVariants = calloc( (size_t)map->numShaders,
		sizeof( *materialVariants ) );
	proxyPrograms = calloc( (size_t)map->numShaders,
		sizeof( *proxyPrograms ) );
	legacyVariants = calloc( (size_t)map->numShaders
		* RAL_MATERIAL_SOURCE_MAX_STAGES, sizeof( *legacyVariants ) );
	source = calloc( 1u, sizeof( *source ) );
	cooked = calloc( 1u, sizeof( *cooked ) );
	manifest = calloc( 1u, sizeof( *manifest ) );
	if ( !entries || !materialVariants || !proxyPrograms || !legacyVariants
			|| !source || !cooked || !manifest
			|| !WiredMaterialCookFilesystem_Init( &filesystem, derivedRoot ) ) {
		Fail( outDiagnostic, RENDER_MATERIAL_PROJECT_COOK_IO_REJECTED,
			0u, 0u ); goto done;
	}
	binder.imports = imports;
	binder.source = source;
	for ( uint32_t shaderIndex = 0u;
			shaderIndex < (uint32_t)map->numShaders; ++shaderIndex ) {
		renderMaterialScriptEntry_t entry;
		char embeddedImage[MAX_QPATH];
		renderMaterialProductDiagnostic_t productDiagnostic;
		ralMaterialSourceDiagnostic_t sourceDiagnostic;
		qboolean duplicate = qfalse;
		/* `noshader` is the BSP compiler's explicit missing-material sentinel,
		 * not a declaration or image.  Runtime deliberately routes it to the
		 * built-in diagnostic shader, so it must not become a fabricated cooked
		 * material or make an otherwise complete corpus unpublished. */
		if ( !referenced[shaderIndex] || !map->shaders[shaderIndex].shader[0]
				|| !strcasecmp( map->shaders[shaderIndex].shader, "noshader" ) )
			continue;
		for ( uint32_t previous = 0u; previous < entryCount; ++previous )
			if ( !strcasecmp( entries[previous].semanticName,
					map->shaders[shaderIndex].shader ) ) duplicate = qtrue;
		if ( duplicate ) continue;
		memset( &entry, 0, sizeof( entry ) );
		/* A Quake 1 map's embedded miptex takes precedence over a declaration of the same name, as in the renderer that draws
		 * those maps natively (R_FindShader builds the Q1 synthetic shader before the text-file path). */
		if ( !( RenderQ1Miptex_MaterialImage( map, map->shaders[shaderIndex].shader, embeddedImage )
					&& ImplicitEntry( &map->shaders[shaderIndex], embeddedImage, imports, &entry ) )
				&& !RenderMaterialScript_Lookup( catalog,
					map->shaders[shaderIndex].shader, &entry )
				&& !ImplicitEntry( &map->shaders[shaderIndex], map->shaders[shaderIndex].shader, imports, &entry ) ) {
			Fail( outDiagnostic, RENDER_MATERIAL_PROJECT_COOK_SOURCE_REJECTED,
				shaderIndex, 0u ); goto done;
		}
		memset( &sourceDiagnostic, 0, sizeof( sourceDiagnostic ) );
		if ( !RenderMaterialScript_CompileSource( &entry, imports, generation,
				source, &sourceDiagnostic ) ) {
			Fail( outDiagnostic, RENDER_MATERIAL_PROJECT_COOK_SOURCE_REJECTED,
				shaderIndex, 0u );
			if ( outDiagnostic ) outDiagnostic->source = sourceDiagnostic;
			goto done;
		}
		binder.source = source;
		if ( !RenderMaterialProduct_CookAndStore( source, BindDependency,
				&binder, WiredMaterialCookFilesystem_MaterialOps(), &filesystem,
				cooked, &productDiagnostic ) ) {
			Fail( outDiagnostic,
				productDiagnostic.reason == RENDER_MATERIAL_PRODUCT_BINDING_REJECTED
					? RENDER_MATERIAL_PROJECT_COOK_DEPENDENCY_REJECTED
					: RENDER_MATERIAL_PROJECT_COOK_PRODUCT_REJECTED,
				shaderIndex, productDiagnostic.dependencyIndex );
			if ( outDiagnostic ) outDiagnostic->cook = productDiagnostic.cook;
			goto done;
		}
		if ( !ContainsU64( materialVariants, materialVariantCount,
				cooked->material.variantKey ) )
			materialVariants[materialVariantCount++] =
				cooked->material.variantKey;
		for ( uint32_t stage = 0u; stage < cooked->legacyStageCount;
				++stage ) {
			++legacyStageCount;
			if ( cooked->legacyStages[stage].outcome
					== RAL_LEGACY_MATERIAL_FALLBACK )
				++fallbackStageCount;
			if ( !ContainsLegacyVariant( legacyVariants, legacyVariantCount,
					&cooked->legacyStages[stage] ) )
				legacyVariants[legacyVariantCount++] =
					cooked->legacyStages[stage];
		}
		if ( cooked->hasProxy && !ContainsU64( proxyPrograms,
				proxyProgramCount, cooked->proxy.programHash ) )
			proxyPrograms[proxyProgramCount++] = cooked->proxy.programHash;
		if ( !Ral_MaterialPackageEntryFromCook( cooked,
				&entries[entryCount++] ) ) {
			Fail( outDiagnostic, RENDER_MATERIAL_PROJECT_COOK_PRODUCT_REJECTED,
				shaderIndex, 0u ); goto done;
		}
	}
	if ( !entryCount || !Ral_MaterialPackageSeal( generation, entries,
			entryCount, manifest ) ) {
		Fail( outDiagnostic, RENDER_MATERIAL_PROJECT_COOK_MANIFEST_REJECTED,
			0u, 0u ); goto done;
	}
	memset( &result, 0, sizeof( result ) );
	result.schemaVersion = RENDER_MATERIAL_PROJECT_COOK_SCHEMA_VERSION;
	result.generation = generation;
	result.mapShaderCount = (uint32_t)map->numShaders;
	result.cookedMaterialCount = entryCount;
	result.materialVariantCount = materialVariantCount;
	result.legacyStageCount = legacyStageCount;
	result.legacyVariantCount = legacyVariantCount;
	result.fallbackStageCount = fallbackStageCount;
	result.proxyProgramCount = proxyProgramCount;
	result.manifestArtifactHash = manifest->artifactHash;
	result.corpusHash = FNV64_OFFSET;
	result.mapChecksum = (uint32_t)map->checksum;
	for ( uint32_t i = 0u; i < entryCount; ++i )
		result.corpusHash = HashU64( result.corpusHash,
			entries[i].cookedArtifactHash );
	if ( !result.corpusHash ) result.corpusHash = 1u;
	if ( !Ral_MaterialProjectReferenceSeal( map->name,
			(uint32_t)map->checksum, generation, entryCount, result.corpusHash,
			manifest->artifactHash, &projectReference ) ) {
		Fail( outDiagnostic, RENDER_MATERIAL_PROJECT_COOK_MANIFEST_REJECTED,
			0u, 0u ); goto done;
	}
	if ( !Ral_MaterialPackageCacheStore(
			WiredMaterialCookFilesystem_PackageOps(), &filesystem, manifest ) ) {
		Fail( outDiagnostic, RENDER_MATERIAL_PROJECT_COOK_IO_REJECTED,
			0u, 0u ); goto done;
	}
	if ( !Ral_MaterialProjectReferenceStore(
			WiredMaterialCookFilesystem_ProjectReferenceOps(), &filesystem,
			&projectReference ) ) {
		Fail( outDiagnostic, RENDER_MATERIAL_PROJECT_COOK_IO_REJECTED,
			0u, 0u ); goto done;
	}
	result.projectReferenceArtifactHash = projectReference.artifactHash;
	result.ready = qtrue;
	if ( !RenderMaterialProjectCook_ReceiptValid( &result ) ) goto done;
	*outReceipt = result;
	if ( outDiagnostic ) outDiagnostic->reason = RENDER_MATERIAL_PROJECT_COOK_OK;
	success = qtrue;
done:
	free( legacyVariants );
	free( proxyPrograms );
	free( materialVariants );
	free( manifest );
	free( cooked );
	free( source );
	free( entries );
	return success;
}
