// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "render_material_package.h"

#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if ( !(x) ) { fprintf( stderr, "FAIL %s:%d: %s\n", \
	__FILE__, __LINE__, #x ); return 1; } } while ( 0 )

typedef struct {
	uint64_t hash;
	char key[RAL_MATERIAL_COOK_CACHE_KEY_TEXT_CAPACITY];
	ralMaterialCookReceipt_t receipt;
	qboolean present;
} materialSlot_t;

typedef struct { materialSlot_t slots[8]; uint32_t writes; } materialCache_t;
typedef struct {
	uint64_t hash;
	char key[RAL_MATERIAL_PACKAGE_KEY_TEXT_CAPACITY];
	ralMaterialPackageReceipt_t receipt;
	uint32_t writes;
	qboolean present;
} packageCache_t;
typedef struct {
	char key[RAL_MATERIAL_PROJECT_REFERENCE_KEY_TEXT_CAPACITY];
	ralMaterialProjectReferenceReceipt_t receipt;
	uint32_t writes;
	qboolean present;
} projectCache_t;

typedef struct {
	ralMaterialRuntimeResource_t resources[16];
	uint32_t count;
	uint32_t calls;
	qboolean reject;
} runtimeResolver_t;

static qboolean MaterialWrite( void *opaque, uint64_t hash, const char *key,
		const void *bytes, uint64_t length ) {
	materialCache_t *cache = opaque;
	materialSlot_t *slot = NULL;
	if ( !cache || !key || !bytes || length != sizeof( ralMaterialCookReceipt_t ) )
		return qfalse;
	for ( uint32_t i = 0u; i < 8u; ++i ) {
		if ( cache->slots[i].present && cache->slots[i].hash == hash ) {
			slot = &cache->slots[i]; break;
		}
		if ( !slot && !cache->slots[i].present ) slot = &cache->slots[i];
	}
	if ( !slot ) return qfalse;
	slot->hash = hash;
	(void)snprintf( slot->key, sizeof( slot->key ), "%s", key );
	memcpy( &slot->receipt, bytes, sizeof( slot->receipt ) );
	slot->present = qtrue;
	cache->writes++;
	return qtrue;
}

static qboolean MaterialRead( void *opaque, uint64_t hash, const char *key,
		void *bytes, uint64_t capacity, uint64_t *outLength ) {
	materialCache_t *cache = opaque;
	if ( !cache || !key || !bytes || !outLength
			|| capacity < sizeof( ralMaterialCookReceipt_t ) ) return qfalse;
	for ( uint32_t i = 0u; i < 8u; ++i ) {
		const materialSlot_t *slot = &cache->slots[i];
		if ( slot->present && slot->hash == hash && !strcmp( slot->key, key ) ) {
			memcpy( bytes, &slot->receipt, sizeof( slot->receipt ) );
			*outLength = sizeof( slot->receipt );
			return qtrue;
		}
	}
	return qfalse;
}

static qboolean PackageWrite( void *opaque, uint64_t hash, const char *key,
		const void *bytes, uint64_t length ) {
	packageCache_t *cache = opaque;
	if ( !cache || !key || !bytes || length != sizeof( cache->receipt ) )
		return qfalse;
	cache->hash = hash;
	(void)snprintf( cache->key, sizeof( cache->key ), "%s", key );
	memcpy( &cache->receipt, bytes, sizeof( cache->receipt ) );
	cache->writes++;
	cache->present = qtrue;
	return qtrue;
}

static qboolean PackageRead( void *opaque, uint64_t hash, const char *key,
		void *bytes, uint64_t capacity, uint64_t *outLength ) {
	packageCache_t *cache = opaque;
	if ( !cache || !cache->present || hash != cache->hash
			|| strcmp( key, cache->key ) || capacity < sizeof( cache->receipt ) )
		return qfalse;
	memcpy( bytes, &cache->receipt, sizeof( cache->receipt ) );
	*outLength = sizeof( cache->receipt );
	return qtrue;
}

static qboolean ProjectWrite( void *opaque, const char *key,
		const void *bytes, uint64_t length ) {
	projectCache_t *cache = opaque;
	if ( !cache || !key || !bytes || length != sizeof( cache->receipt ) )
		return qfalse;
	(void)snprintf( cache->key, sizeof( cache->key ), "%s", key );
	memcpy( &cache->receipt, bytes, sizeof( cache->receipt ) );
	cache->writes++;
	cache->present = qtrue;
	return qtrue;
}

static qboolean ProjectRead( void *opaque, const char *key, void *bytes,
		uint64_t capacity, uint64_t *outLength ) {
	projectCache_t *cache = opaque;
	if ( !cache || !cache->present || strcmp( key, cache->key )
			|| !bytes || !outLength || capacity < sizeof( cache->receipt ) )
		return qfalse;
	memcpy( bytes, &cache->receipt, sizeof( cache->receipt ) );
	*outLength = sizeof( cache->receipt );
	return qtrue;
}

static qboolean ResolveRuntimeResource( void *opaque, uint64_t cacheKeyHash,
		ralMaterialRuntimeResource_t *outResource ) {
	runtimeResolver_t *resolver = opaque;
	if ( !resolver || !outResource ) return qfalse;
	resolver->calls++;
	if ( resolver->reject ) return qfalse;
	for ( uint32_t i = 0u; i < resolver->count; ++i ) {
		if ( resolver->resources[i].cacheKeyHash == cacheKeyHash ) {
			*outResource = resolver->resources[i];
			return qtrue;
		}
	}
	return qfalse;
}

static ralMaterialSourceIr_t Source( const char *name, uint64_t sourceId ) {
	ralMaterialSourceIr_t source;
	memset( &source, 0, sizeof( source ) );
	source.schemaVersion = RAL_MATERIAL_SOURCE_IR_SCHEMA_VERSION;
	source.generation = 12u;
	source.provenance = RAL_MATERIAL_PROVENANCE_WIRED_LUA;
	source.declaration.sourceId = sourceId;
	source.declaration.line = source.declaration.column = 1u;
	(void)snprintf( source.semanticName, sizeof( source.semanticName ), "%s", name );
	source.diffuseReflectance[0] = source.diffuseReflectance[1] =
		source.diffuseReflectance[2] = 1.0f;
	source.pbrFactors.baseColor[0] = source.pbrFactors.baseColor[1] =
		source.pbrFactors.baseColor[2] = source.pbrFactors.baseColor[3] = 1.0f;
	source.pbrFactors.roughness = source.pbrFactors.normalScale =
		source.pbrFactors.occlusionStrength = 1.0f;
	source.pbrRenderPolicy.depthTest = source.pbrRenderPolicy.depthWrite = qtrue;
	source.dependencyCount = source.pbrTextureCount = 1u;
	source.dependencies[0].role = RAL_MATERIAL_DEPENDENCY_TEXTURE;
	source.dependencies[0].span = source.declaration;
	source.dependencies[0].sourceId = sourceId + 1u;
	source.dependencies[0].fsGeneration = 3u;
	source.dependencies[0].size = 256u;
	source.dependencies[0].creationOptionsHash = 19u;
	(void)snprintf( source.dependencies[0].canonicalPath,
		sizeof( source.dependencies[0].canonicalPath ),
		"textures/package/%llu.ktx2", (unsigned long long)sourceId );
	source.pbrTextures[0].span = source.declaration;
	source.pbrTextures[0].dependencyIndex = 0u;
	source.pbrTextures[0].role = RAL_MATERIAL_TEXTURE_BASE_COLOR;
	source.pbrTextures[0].channels = RAL_MATERIAL_CHANNEL_RGBA;
	return source;
}

static ralMaterialSourceIr_t SkySource( void ) {
	ralMaterialSourceIr_t source;
	memset( &source, 0, sizeof( source ) );
	source.schemaVersion = RAL_MATERIAL_SOURCE_IR_SCHEMA_VERSION;
	source.generation = 12u;
	source.provenance = RAL_MATERIAL_PROVENANCE_Q3_SHADER;
	source.declaration.sourceId = 1000u;
	source.declaration.line = source.declaration.column = 1u;
	(void)snprintf( source.semanticName, sizeof( source.semanticName ),
		"materials/package/sky" );
	source.diffuseReflectance[0] = source.diffuseReflectance[1] =
		source.diffuseReflectance[2] = 1.0f;
	source.sky = qtrue;
	source.skyCloudHeight = 512.0f;
	source.skySource.outerBox = qtrue;
	source.skySource.hasSun = qtrue;
	source.skySource.sunColor[0] = 1.0f;
	source.skySource.sunColor[1] = 0.8f;
	source.skySource.sunColor[2] = 0.6f;
	source.skySource.sunIntensity = 120.0f;
	source.skySource.sunAzimuthDegrees = 90.0f;
	source.skySource.sunElevationDegrees = 35.0f;
	source.dependencyCount = 7u;
	for ( uint32_t i = 0u; i < source.dependencyCount; ++i ) {
		source.dependencies[i].role = RAL_MATERIAL_DEPENDENCY_TEXTURE;
		source.dependencies[i].span = source.declaration;
		source.dependencies[i].sourceId = 1001u + i;
		source.dependencies[i].fsGeneration = 3u;
		source.dependencies[i].size = 256u;
		source.dependencies[i].creationOptionsHash = 29u + i;
		(void)snprintf( source.dependencies[i].canonicalPath,
			sizeof( source.dependencies[i].canonicalPath ),
			"textures/package/sky/%u.png", i );
		if ( i < RAL_MATERIAL_SKY_FACE_COUNT )
			source.skySource.outerDependencies[i] = i;
	}
	source.stageCount = 1u;
	source.stages[0].span = source.declaration;
	source.stages[0].mapKind = RAL_MATERIAL_MAP_IMAGE;
	source.stages[0].dependencyIndex = 6u;
	source.stages[0].tcGen = RAL_MATERIAL_TCGEN_TEXTURE;
	source.stages[0].depthWrite = qtrue;
	return source;
}

static qboolean Bind( void *opaque, uint32_t index,
		const ralMaterialSourceDependency_t *dependency,
		ralMaterialCookBinding_t *out ) {
	ralTextureAssetRequest_t request;
	ralTextureAssetReceipt_t receipt;
	(void)opaque;
	memset( &request, 0, sizeof( request ) );
	request.schemaVersion = RAL_TEXTURE_ASSET_SCHEMA_VERSION;
	request.assetGeneration = 22u;
	request.provenanceHash = dependency->sourceId;
	request.dimension = RAL_TEXTURE_ASSET_2D;
	request.width = request.height = 8u;
	request.depth = request.layers = request.sourceMipLevels = 1u;
	request.colorEncoding = RAL_TEXTURE_ENCODING_SRGB;
	request.channelSemantic = RAL_TEXTURE_CHANNEL_COLOR;
	request.sourceEncoding = RAL_TEXTURE_SOURCE_UNCOMPRESSED;
	request.mipPolicy = RAL_TEXTURE_MIPS_SOURCE;
	request.residency = RAL_TEXTURE_RESIDENCY_STREAMED;
	request.preferenceCount = 1u;
	request.preferences[0] = RAL_TEXTURE_COMPRESSION_UNCOMPRESSED;
	request.allowUncompressedFallback = qtrue;
	if ( !Ral_ResolveTextureAsset( &request, &receipt ) ) return qfalse;
	memset( out, 0, sizeof( *out ) );
	out->dependencyIndex = index;
	out->role = dependency->role;
	out->sourceId = dependency->sourceId;
	out->fsGeneration = dependency->fsGeneration;
	out->creationOptionsHash = dependency->creationOptionsHash;
	if ( !Ral_TextureCacheKeyBuild( &receipt, &out->texture ) ) return qfalse;
	out->artifactHash = out->texture.keyHash;
	return qtrue;
}

static ralTextureResourceReceipt_t RuntimeResource(
		const ralMaterialTexture_t *texture ) {
	ralTextureResourceReceipt_t resource;
	memset( &resource, 0, sizeof( resource ) );
	resource.schemaVersion = RAL_TEXTURE_RESOURCE_RECEIPT_SCHEMA_VERSION;
	resource.backendType = RAL_BACKEND_WEBGPU;
	resource.textureIdentity = (uintptr_t)0x5000u;
	resource.resourceGeneration = 31u;
	resource.type = RAL_TEXTURE_2D;
	resource.format = texture->texture.asset.targetFormat;
	resource.usage = (ralTextureUsage_t)( RAL_TEXTURE_USAGE_SAMPLED
		| RAL_TEXTURE_USAGE_TRANSFER_DST );
	resource.width = texture->texture.asset.width;
	resource.height = texture->texture.asset.height;
	resource.mipLevels = texture->texture.asset.targetMipLevels;
	resource.arrayLayers = 1u;
	resource.ready = qtrue;
	return resource;
}

static ralTextureResourceReceipt_t RuntimeResourceKey(
		const ralTextureCacheKey_t *texture, uintptr_t identity ) {
	ralTextureResourceReceipt_t resource;
	memset( &resource, 0, sizeof( resource ) );
	resource.schemaVersion = RAL_TEXTURE_RESOURCE_RECEIPT_SCHEMA_VERSION;
	resource.backendType = RAL_BACKEND_WEBGPU;
	resource.textureIdentity = identity;
	resource.resourceGeneration = 31u;
	resource.type = RAL_TEXTURE_2D;
	resource.format = texture->asset.targetFormat;
	resource.usage = (ralTextureUsage_t)( RAL_TEXTURE_USAGE_SAMPLED
		| RAL_TEXTURE_USAGE_TRANSFER_DST );
	resource.width = texture->asset.width;
	resource.height = texture->asset.height;
	resource.mipLevels = texture->asset.targetMipLevels;
	resource.arrayLayers = 1u;
	resource.ready = qtrue;
	return resource;
}

int main( void ) {
	const ralMaterialCookCacheOps_t materialOps = { MaterialWrite, MaterialRead };
	const ralMaterialPackageCacheOps_t packageOps = { PackageWrite, PackageRead };
	const ralMaterialProjectReferenceOps_t projectOps = {
		ProjectWrite, ProjectRead };
	ralMaterialSourceIr_t ir[3] = {
		Source( "materials/package/zeta", 900u ),
		Source( "materials/package/alpha", 800u ),
		SkySource() };
	ralMaterialSourceReceipt_t sources[3], reversed[3], duplicate[2];
	ralMaterialPackageReceipt_t manifest, reordered, before;
	ralMaterialProjectReferenceReceipt_t project, loadedProject;
	ralMaterialCookReceipt_t loaded, loadedBefore;
	ralMaterialRuntimeResource_t runtimeResource;
	ralMaterialRuntimePlan_t runtimePlan, runtimeBefore;
	renderMaterialRuntimeCatalog_t runtimeCatalog, catalogBefore;
	runtimeResolver_t resolver;
	renderMaterialPackageDiagnostic_t diagnostic;
	materialCache_t materials;
	packageCache_t packages;
	projectCache_t projects;
	memset( &materials, 0, sizeof( materials ) );
	memset( &packages, 0, sizeof( packages ) );
	memset( &projects, 0, sizeof( projects ) );
	memset( &runtimeCatalog, 0, sizeof( runtimeCatalog ) );
	memset( &resolver, 0, sizeof( resolver ) );
	CHECK( Ral_MaterialSourceCompile( &ir[0], &sources[0], NULL ) );
	CHECK( Ral_MaterialSourceCompile( &ir[1], &sources[1], NULL ) );
	CHECK( Ral_MaterialSourceCompile( &ir[2], &sources[2], NULL ) );
	CHECK( RenderMaterialPackage_CookAndPublish( 7u, sources, 3u, Bind, NULL,
		&materialOps, &materials, &packageOps, &packages, &manifest,
		&diagnostic ) );
	CHECK( diagnostic.reason == RENDER_MATERIAL_PACKAGE_OK );
	CHECK( Ral_MaterialPackageReceiptValid( &manifest ) );
	CHECK( !strcmp( manifest.entries[0].semanticName,
		"materials/package/alpha" ) );
	CHECK( packages.writes == 1u && materials.writes == 3u );
	CHECK( Ral_MaterialProjectReferenceSeal( "maps/package-test.bsp",
		0xdeadbeefu, 7u, manifest.entryCount, UINT64_C( 0x12345678 ),
		manifest.artifactHash, &project ) );
	CHECK( Ral_MaterialProjectReferenceStore( &projectOps, &projects,
		&project ) );
	CHECK( projects.writes == 1u );
	reversed[0] = sources[2]; reversed[1] = sources[1]; reversed[2] = sources[0];
	CHECK( RenderMaterialPackage_CookAndPublish( 7u, reversed, 3u, Bind, NULL,
		&materialOps, &materials, &packageOps, &packages, &reordered,
		&diagnostic ) );
	CHECK( Ral_MaterialPackageReceiptExact( &manifest, &reordered ) );
	CHECK( RenderMaterialPackage_LoadCooked( manifest.artifactHash,
		"materials/package/zeta", &packageOps, &packages, &materialOps,
		&materials, &loaded, &diagnostic ) );
	CHECK( !strcmp( loaded.source.source.semanticName,
		"materials/package/zeta" ) );
	CHECK( RenderMaterialPackage_LoadProjectCooked( "maps/package-test.bsp",
		0xdeadbeefu, "materials/package/zeta", &projectOps, &projects,
		&packageOps, &packages, &materialOps, &materials, &loadedProject,
		&loaded, &diagnostic ) );
	CHECK( loadedProject.artifactHash == project.artifactHash );
	runtimeResource.cacheKeyHash =
		loaded.material.material.textures[0].texture.keyHash;
	runtimeResource.resource = RuntimeResource(
		&loaded.material.material.textures[0] );
	CHECK( RenderMaterialPackage_LoadProjectRuntimePlan(
		"maps/package-test.bsp", 0xdeadbeefu, "materials/package/zeta",
		&projectOps, &projects, &packageOps, &packages, &materialOps,
		&materials, &runtimeResource, 1u, 41u, &loadedProject,
		&runtimePlan, &diagnostic ) );
	CHECK( diagnostic.reason == RENDER_MATERIAL_PACKAGE_OK
		&& runtimePlan.runtimeGeneration == 41u
		&& runtimePlan.bindings[0].cacheKeyHash
			== runtimeResource.cacheKeyHash );
	for ( uint32_t i = 0u; i < 8u; ++i ) {
		if ( !materials.slots[i].present ) continue;
		for ( uint32_t binding = 0u;
				binding < materials.slots[i].receipt.bindingCount; ++binding ) {
			const ralMaterialCookBinding_t *cookedBinding =
				&materials.slots[i].receipt.bindings[binding];
			if ( cookedBinding->role != RAL_MATERIAL_DEPENDENCY_TEXTURE )
				continue;
			resolver.resources[resolver.count].cacheKeyHash =
				cookedBinding->texture.keyHash;
			resolver.resources[resolver.count].resource = RuntimeResourceKey(
				&cookedBinding->texture, (uintptr_t)( 0x5000u + resolver.count ) );
			resolver.count++;
		}
	}
	CHECK( resolver.count == 9u );
	CHECK( RenderMaterialPackage_LoadProjectRuntimeCatalog(
		"maps/package-test.bsp", 0xdeadbeefu, &projectOps, &projects,
		&packageOps, &packages, &materialOps, &materials,
		ResolveRuntimeResource, &resolver, 51u, &runtimeCatalog,
		&diagnostic ) );
	CHECK( diagnostic.reason == RENDER_MATERIAL_PACKAGE_OK
		&& runtimeCatalog.ready == qtrue
		&& runtimeCatalog.entryCount == 3u
		&& runtimeCatalog.runtimeGeneration == 51u
		&& resolver.calls == 9u );
	CHECK( RenderMaterialRuntimeCatalog_Lookup( &runtimeCatalog,
		"materials/package/alpha" )->runtimeGeneration == 51u );
	CHECK( RenderMaterialRuntimeCatalog_Lookup( &runtimeCatalog,
		"materials/package/zeta" )->runtimeGeneration == 53u );
	{
		const ralMaterialRuntimePlan_t *sky = RenderMaterialRuntimeCatalog_Lookup(
			&runtimeCatalog, "materials/package/sky" );
		ralTextureResourceReceipt_t face;
		CHECK( sky && sky->sky.present == qtrue
			&& sky->sky.authored.outerBox == qtrue
			&& sky->sky.stageCount == 1u );
		for ( uint32_t i = 0u; i < RAL_MATERIAL_SKY_FACE_COUNT; ++i )
			CHECK( Ral_MaterialRuntimePlanResolveDependencyTexture( sky,
				sky->sky.authored.outerDependencies[i], &face ) );
	}
	{
		ralTextureResourceReceipt_t selected;
		ralTextureResourceReceipt_t selectedBeforeFailure;
		const ralMaterialRuntimePlan_t *plan = RenderMaterialRuntimeCatalog_Lookup(
			&runtimeCatalog, "materials/package/alpha" );
		CHECK( plan && plan->bindingCount == 1u );
		memset( &selected, 0, sizeof( selected ) );
		CHECK( RenderMaterialRuntimeCatalog_ResolveTexture( &runtimeCatalog,
			"materials/package/alpha", plan->bindings[0].cacheKeyHash,
			&selected ) );
		CHECK( Ral_TextureResourceReceiptExact( &selected,
			&plan->bindings[0].resource ) );
		selectedBeforeFailure = selected;
		CHECK( !RenderMaterialRuntimeCatalog_ResolveTexture( &runtimeCatalog,
			"materials/package/alpha", plan->bindings[0].cacheKeyHash ^ 1u,
			&selected ) );
		CHECK( memcmp( &selected, &selectedBeforeFailure,
			sizeof( selected ) ) == 0 );
	}
	CHECK( !RenderMaterialRuntimeCatalog_Lookup( &runtimeCatalog,
		"materials/package/missing" ) );
	catalogBefore = runtimeCatalog;
	resolver.reject = qtrue;
	CHECK( !RenderMaterialPackage_LoadProjectRuntimeCatalog(
		"maps/package-test.bsp", 0xdeadbeefu, &projectOps, &projects,
		&packageOps, &packages, &materialOps, &materials,
		ResolveRuntimeResource, &resolver, 61u, &runtimeCatalog,
		&diagnostic ) );
	CHECK( diagnostic.reason == RENDER_MATERIAL_PACKAGE_RUNTIME_PLAN_REJECTED
		&& diagnostic.runtime.reason
			== RAL_MATERIAL_RUNTIME_BUILD_MISSING_RESOURCE
		&& !memcmp( &runtimeCatalog, &catalogBefore,
			sizeof( runtimeCatalog ) ) );
	resolver.reject = qfalse;
	CHECK( !RenderMaterialPackage_LoadProjectRuntimeCatalog(
		"maps/package-test.bsp", 0xdeadbeefu, &projectOps, &projects,
		&packageOps, &packages, &materialOps, &materials,
		ResolveRuntimeResource, &resolver,
		UINT64_MAX - RAL_MATERIAL_PACKAGE_MAX_ENTRIES + 1u,
		&runtimeCatalog, &diagnostic ) );
	CHECK( diagnostic.reason == RENDER_MATERIAL_PACKAGE_INVALID_ARGUMENT
		&& !memcmp( &runtimeCatalog, &catalogBefore,
			sizeof( runtimeCatalog ) ) );
	RenderMaterialRuntimeCatalog_Destroy( &runtimeCatalog );
	CHECK( runtimeCatalog.entries == NULL && runtimeCatalog.ready == qfalse
		&& runtimeCatalog.entryCount == 0u );
	runtimeBefore = runtimePlan;
	runtimeResource.cacheKeyHash ^= 1u;
	CHECK( !RenderMaterialPackage_LoadProjectRuntimePlan(
		"maps/package-test.bsp", 0xdeadbeefu, "materials/package/zeta",
		&projectOps, &projects, &packageOps, &packages, &materialOps,
		&materials, &runtimeResource, 1u, 42u, &loadedProject,
		&runtimePlan, &diagnostic ) );
	CHECK( diagnostic.reason == RENDER_MATERIAL_PACKAGE_RUNTIME_PLAN_REJECTED
		&& diagnostic.runtime.reason
			== RAL_MATERIAL_RUNTIME_BUILD_MISSING_RESOURCE
		&& !memcmp( &runtimePlan, &runtimeBefore, sizeof( runtimePlan ) ) );
	loadedBefore = loaded;
	CHECK( !RenderMaterialPackage_LoadProjectCooked( "maps/package-test.bsp",
		0xdeadbeeeu, "materials/package/zeta", &projectOps, &projects,
		&packageOps, &packages, &materialOps, &materials, &loadedProject,
		&loaded, &diagnostic ) );
	CHECK( diagnostic.reason
		== RENDER_MATERIAL_PACKAGE_PROJECT_REFERENCE_LOAD_REJECTED );
	CHECK( !memcmp( &loaded, &loadedBefore, sizeof( loaded ) ) );
	CHECK( !RenderMaterialPackage_LoadCooked( manifest.artifactHash,
		"materials/package/missing", &packageOps, &packages, &materialOps,
		&materials, &loaded, &diagnostic ) );
	CHECK( diagnostic.reason == RENDER_MATERIAL_PACKAGE_MATERIAL_NOT_FOUND );
	CHECK( !memcmp( &loaded, &loadedBefore, sizeof( loaded ) ) );
	duplicate[0] = sources[0]; duplicate[1] = sources[0];
	before = manifest;
	{
		const uint32_t manifestWrites = packages.writes;
		CHECK( !RenderMaterialPackage_CookAndPublish( 8u, duplicate, 2u,
			Bind, NULL, &materialOps, &materials, &packageOps, &packages,
			&manifest, &diagnostic ) );
		CHECK( diagnostic.reason == RENDER_MATERIAL_PACKAGE_MANIFEST_REJECTED );
		CHECK( packages.writes == manifestWrites );
		CHECK( !memcmp( &manifest, &before, sizeof( manifest ) ) );
	}
	puts( "render_material_package_test: ok" );
	return 0;
}
