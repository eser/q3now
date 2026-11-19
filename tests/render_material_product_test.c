// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "render_material_product.h"

#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if ( !(x) ) { fprintf( stderr, "FAIL %s:%d: %s\n", \
	__FILE__, __LINE__, #x ); return 1; } } while ( 0 )

typedef struct {
	char key[RAL_MATERIAL_COOK_CACHE_KEY_TEXT_CAPACITY];
	uint64_t hash;
	uint64_t length;
	unsigned char bytes[sizeof( ralMaterialCookReceipt_t )];
	qboolean present;
} memoryCache_t;

static qboolean Write( void *opaque, uint64_t hash, const char *key,
		const void *bytes, uint64_t length ) {
	memoryCache_t *cache = opaque;
	if ( !cache || !key || !bytes || length != sizeof( cache->bytes ) )
		return qfalse;
	cache->hash = hash;
	cache->length = length;
	(void)snprintf( cache->key, sizeof( cache->key ), "%s", key );
	memcpy( cache->bytes, bytes, (size_t)length );
	cache->present = qtrue;
	return qtrue;
}

static qboolean Read( void *opaque, uint64_t hash, const char *key,
		void *bytes, uint64_t capacity, uint64_t *outLength ) {
	memoryCache_t *cache = opaque;
	if ( !cache || !cache->present || hash != cache->hash
			|| strcmp( key, cache->key ) || capacity < cache->length ) return qfalse;
	memcpy( bytes, cache->bytes, (size_t)cache->length );
	*outLength = cache->length;
	return qtrue;
}

static ralMaterialSourceIr_t Source( void ) {
	ralMaterialSourceIr_t source;
	memset( &source, 0, sizeof( source ) );
	source.schemaVersion = RAL_MATERIAL_SOURCE_IR_SCHEMA_VERSION;
	source.generation = 12u;
	source.provenance = RAL_MATERIAL_PROVENANCE_WIRED_LUA;
	source.declaration.sourceId = 700u;
	source.declaration.line = source.declaration.column = 1u;
	(void)snprintf( source.semanticName, sizeof( source.semanticName ),
		"materials/test/panel" );
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
	source.dependencies[0].sourceId = 701u;
	source.dependencies[0].fsGeneration = 3u;
	source.dependencies[0].size = 256u;
	source.dependencies[0].creationOptionsHash = 19u;
	(void)snprintf( source.dependencies[0].canonicalPath,
		sizeof( source.dependencies[0].canonicalPath ), "textures/panel.ktx2" );
	source.pbrTextures[0].span = source.declaration;
	source.pbrTextures[0].dependencyIndex = 0u;
	source.pbrTextures[0].role = RAL_MATERIAL_TEXTURE_BASE_COLOR;
	source.pbrTextures[0].channels = RAL_MATERIAL_CHANNEL_RGBA;
	return source;
}

typedef struct { qboolean reject; } binder_t;

static qboolean Bind( void *opaque, uint32_t index,
		const ralMaterialSourceDependency_t *dependency,
		ralMaterialCookBinding_t *out ) {
	binder_t *binder = opaque;
	ralTextureAssetRequest_t request;
	ralTextureAssetReceipt_t receipt;
	if ( binder->reject ) return qfalse;
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

int main( void ) {
	const ralMaterialCookCacheOps_t cacheOps = { Write, Read };
	ralMaterialSourceIr_t ir = Source();
	ralMaterialSourceReceipt_t source;
	ralMaterialCookReceipt_t cooked, loaded, before;
	renderMaterialProductDiagnostic_t diagnostic;
	memoryCache_t cache;
	binder_t binder = { qfalse };
	memset( &cache, 0, sizeof( cache ) );
	CHECK( Ral_MaterialSourceCompile( &ir, &source, NULL ) );
	memset( &cooked, 0xA5, sizeof( cooked ) );
	CHECK( RenderMaterialProduct_CookAndStore( &source, Bind, &binder,
		&cacheOps, &cache, &cooked, &diagnostic ) );
	CHECK( diagnostic.reason == RENDER_MATERIAL_PRODUCT_OK );
	CHECK( Ral_MaterialCookReceiptValid( &cooked ) && cache.present );
	CHECK( RenderMaterialProduct_LoadCooked( cooked.artifactHash, &cacheOps,
		&cache, &loaded, &diagnostic ) );
	CHECK( Ral_MaterialCookReceiptExact( &cooked, &loaded ) );
	before = loaded;
	CHECK( !RenderMaterialProduct_LoadCooked( cooked.artifactHash + 1u,
		&cacheOps, &cache, &loaded, &diagnostic ) );
	CHECK( diagnostic.reason == RENDER_MATERIAL_PRODUCT_CACHE_LOAD_REJECTED );
	CHECK( !memcmp( &loaded, &before, sizeof( loaded ) ) );
	binder.reject = qtrue;
	CHECK( !RenderMaterialProduct_CookAndStore( &source, Bind, &binder,
		&cacheOps, &cache, &loaded, &diagnostic ) );
	CHECK( diagnostic.reason == RENDER_MATERIAL_PRODUCT_BINDING_REJECTED
		&& diagnostic.dependencyIndex == 0u );
	CHECK( !memcmp( &loaded, &before, sizeof( loaded ) ) );
	puts( "render_material_product_test: ok" );
	return 0;
}
