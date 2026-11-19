// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "ral_material_cook_cache.h"

#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if ( !(x) ) { fprintf( stderr, "FAIL %s:%d: %s\n", \
	__FILE__, __LINE__, #x ); return 1; } } while ( 0 )

typedef struct {
	uint64_t artifactHash;
	char keyText[RAL_MATERIAL_COOK_CACHE_KEY_TEXT_CAPACITY];
	uint64_t byteLength;
	unsigned char bytes[sizeof( ralMaterialCookReceipt_t )];
} memoryStore_t;

static qboolean WriteAtomic( void *opaque, uint64_t artifactHash,
		const char *keyText, const void *bytes, uint64_t byteLength ) {
	memoryStore_t *store = opaque;
	if ( !store || !artifactHash || !keyText || !bytes
			|| byteLength != sizeof( store->bytes ) ) return qfalse;
	store->artifactHash = artifactHash;
	(void)snprintf( store->keyText, sizeof( store->keyText ), "%s", keyText );
	store->byteLength = byteLength;
	memcpy( store->bytes, bytes, (size_t)byteLength );
	return qtrue;
}

static qboolean Read( void *opaque, uint64_t artifactHash,
		const char *keyText, void *bytes, uint64_t capacity,
		uint64_t *outByteLength ) {
	memoryStore_t *store = opaque;
	if ( !store || artifactHash != store->artifactHash
			|| strcmp( keyText, store->keyText ) || !bytes || !outByteLength
			|| capacity < store->byteLength ) return qfalse;
	memcpy( bytes, store->bytes, (size_t)store->byteLength );
	*outByteLength = store->byteLength;
	return qtrue;
}

static qboolean TextureKey( ralTextureCacheKey_t *out ) {
	ralTextureAssetRequest_t request;
	ralTextureAssetReceipt_t receipt;
	memset( &request, 0, sizeof( request ) );
	request.schemaVersion = RAL_TEXTURE_ASSET_SCHEMA_VERSION;
	request.assetGeneration = 7u;
	request.provenanceHash = 701u;
	request.dimension = RAL_TEXTURE_ASSET_2D;
	request.width = request.height = 8u;
	request.depth = request.layers = 1u;
	request.sourceMipLevels = 1u;
	request.colorEncoding = RAL_TEXTURE_ENCODING_SRGB;
	request.channelSemantic = RAL_TEXTURE_CHANNEL_COLOR;
	request.sourceEncoding = RAL_TEXTURE_SOURCE_UNCOMPRESSED;
	request.mipPolicy = RAL_TEXTURE_MIPS_SOURCE;
	request.residency = RAL_TEXTURE_RESIDENCY_STREAMED;
	request.preferenceCount = 1u;
	request.preferences[0] = RAL_TEXTURE_COMPRESSION_UNCOMPRESSED;
	request.allowUncompressedFallback = qtrue;
	return Ral_ResolveTextureAsset( &request, &receipt )
		&& Ral_TextureCacheKeyBuild( &receipt, out );
}

static qboolean Receipt( ralMaterialCookReceipt_t *out ) {
	ralMaterialSourceIr_t ir;
	ralMaterialSourceReceipt_t source;
	ralMaterialCookBinding_t binding;
	memset( &ir, 0, sizeof( ir ) );
	ir.schemaVersion = RAL_MATERIAL_SOURCE_IR_SCHEMA_VERSION;
	ir.generation = 3u;
	ir.provenance = RAL_MATERIAL_PROVENANCE_Q3_SHADER;
	ir.declaration.sourceId = 101u;
	ir.declaration.line = 1u;
	ir.declaration.column = 1u;
	(void)snprintf( ir.semanticName, sizeof( ir.semanticName ), "textures/test" );
	ir.sort = 3.0f;
	ir.diffuseReflectance[0] = ir.diffuseReflectance[1] =
		ir.diffuseReflectance[2] = 1.0f;
	ir.dependencyCount = 1u;
	ir.dependencies[0].role = RAL_MATERIAL_DEPENDENCY_TEXTURE;
	ir.dependencies[0].sourceId = 5001u;
	ir.dependencies[0].fsGeneration = 9u;
	ir.dependencies[0].size = 4096u;
	ir.dependencies[0].creationOptionsHash = 17u;
	ir.dependencies[0].span = ir.declaration;
	(void)snprintf( ir.dependencies[0].canonicalPath,
		sizeof( ir.dependencies[0].canonicalPath ), "textures/test.ktx2" );
	ir.stageCount = 1u;
	ir.stages[0].span = ir.declaration;
	ir.stages[0].mapKind = RAL_MATERIAL_MAP_IMAGE;
	ir.stages[0].dependencyIndex = 0u;
	ir.stages[0].sourceBlend = 1u;
	ir.stages[0].depthWrite = qtrue;
	ir.stages[0].rgbGen = RAL_MATERIAL_COLOR_IDENTITY;
	ir.stages[0].alphaGen = RAL_MATERIAL_ALPHA_IDENTITY;
	ir.stages[0].constantColor[0] = ir.stages[0].constantColor[1] =
		ir.stages[0].constantColor[2] = ir.stages[0].constantColor[3] = 1.0f;
	ir.stages[0].animationLooping = qtrue;
	if ( !Ral_MaterialSourceCompile( &ir, &source, NULL ) ) return qfalse;
	memset( &binding, 0, sizeof( binding ) );
	binding.dependencyIndex = 0u;
	binding.role = ir.dependencies[0].role;
	binding.sourceId = ir.dependencies[0].sourceId;
	binding.fsGeneration = ir.dependencies[0].fsGeneration;
	binding.creationOptionsHash = ir.dependencies[0].creationOptionsHash;
	if ( !TextureKey( &binding.texture ) ) return qfalse;
	binding.artifactHash = binding.texture.keyHash;
	return Ral_MaterialCook( &source, &binding, 1u, out, NULL );
}

int main( void ) {
	const ralMaterialCookCacheOps_t ops = { WriteAtomic, Read };
	memoryStore_t store;
	ralMaterialCookReceipt_t receipt, loaded, before;
	char keyText[RAL_MATERIAL_COOK_CACHE_KEY_TEXT_CAPACITY];
	memset( &store, 0, sizeof( store ) );
	CHECK( Receipt( &receipt ) );
	CHECK( Ral_MaterialCookCacheKeyText( receipt.artifactHash, keyText,
		sizeof( keyText ) ) );
	CHECK( !strncmp( keyText, "wrmat-v4-", 9u ) );
	CHECK( Ral_MaterialCookCacheStore( &ops, &store, &receipt ) );
	CHECK( Ral_MaterialCookCacheLoad( &ops, &store, receipt.artifactHash,
		&loaded ) );
	CHECK( Ral_MaterialCookReceiptExact( &receipt, &loaded ) );

	before = loaded;
	store.byteLength--;
	CHECK( !Ral_MaterialCookCacheLoad( &ops, &store, receipt.artifactHash,
		&loaded ) );
	CHECK( !memcmp( &loaded, &before, sizeof( loaded ) ) );
	store.byteLength++;
	store.bytes[sizeof( store.bytes ) - 1u] ^= 1u;
	CHECK( !Ral_MaterialCookCacheLoad( &ops, &store, receipt.artifactHash,
		&loaded ) );
	CHECK( !memcmp( &loaded, &before, sizeof( loaded ) ) );
	store.bytes[sizeof( store.bytes ) - 1u] ^= 1u;
	CHECK( !Ral_MaterialCookCacheLoad( &ops, &store,
		receipt.artifactHash + 1u, &loaded ) );
	CHECK( !memcmp( &loaded, &before, sizeof( loaded ) ) );

	puts( "ral_material_cook_cache_test: ok" );
	return 0;
}
