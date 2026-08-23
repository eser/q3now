// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_texture_cache.h"

#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if ( !(x) ) { fprintf( stderr, "FAIL %s:%d: %s\n", \
	__FILE__, __LINE__, #x ); return 1; } } while ( 0 )

typedef struct {
	unsigned char bytes[2048];
	uint64_t length;
	ralTextureCacheKey_t key;
	char keyText[RAL_TEXTURE_CACHE_KEY_TEXT_CAPACITY];
	qboolean present, failWrite, corruptRead, truncateRead;
} memoryStore_t;

static qboolean WriteAtomic( void *context, const ralTextureCacheKey_t *key,
		const char *keyText, const void *bytes, uint64_t length ) {
	memoryStore_t *store = (memoryStore_t *)context;
	if ( !store || store->failWrite || length > sizeof( store->bytes ) ) return qfalse;
	memcpy( store->bytes, bytes, (size_t)length ); store->length = length;
	store->key = *key; strcpy( store->keyText, keyText ); store->present = qtrue;
	return qtrue;
}

static qboolean Read( void *context, const ralTextureCacheKey_t *key,
		const char *keyText, void *bytes, uint64_t capacity, uint64_t *outLength ) {
	memoryStore_t *store = (memoryStore_t *)context;
	uint64_t length;
	if ( !store || !store->present || !Ral_TextureCacheKeyExact( key, &store->key )
			|| strcmp( keyText, store->keyText ) ) return qfalse;
	length = store->truncateRead ? store->length - 1u : store->length;
	if ( length > capacity ) return qfalse;
	memcpy( bytes, store->bytes, (size_t)length );
	if ( store->corruptRead ) ( (unsigned char *)bytes )[length - 1u] ^= 1u;
	*outLength = length; return qtrue;
}

static int MakeArtifact( ralTextureAssetReceipt_t *asset, unsigned char *bytes,
		uint64_t capacity, ralTextureArtifactReceipt_t *artifact ) {
	ralTextureAssetRequest_t request;
	unsigned char payload[320];
	uint64_t levels[2] = { 256u, 64u };
	memset( &request, 0, sizeof( request ) ); memset( payload, 0x61, sizeof( payload ) );
	request.schemaVersion = RAL_TEXTURE_ASSET_SCHEMA_VERSION;
	request.assetGeneration = 77u; request.provenanceHash = 0x1234ABCDu;
	request.dimension = RAL_TEXTURE_ASSET_2D; request.width = request.height = 8u;
	request.depth = request.layers = 1u; request.sourceMipLevels = 2u;
	request.colorEncoding = RAL_TEXTURE_ENCODING_SRGB;
	request.channelSemantic = RAL_TEXTURE_CHANNEL_COLOR;
	request.sourceEncoding = RAL_TEXTURE_SOURCE_BASIS_UASTC;
	request.mipPolicy = RAL_TEXTURE_MIPS_SOURCE;
	request.residency = RAL_TEXTURE_RESIDENCY_STREAMED;
	request.preferenceCount = 1u;
	request.preferences[0] = RAL_TEXTURE_COMPRESSION_UNCOMPRESSED;
	request.allowUncompressedFallback = qtrue;
	return Ral_ResolveTextureAsset( &request, asset )
		&& Ral_EncodeTextureArtifact( asset, levels, 2u, payload, sizeof( payload ),
			bytes, capacity, artifact );
}

int main( void ) {
	unsigned char artifactBytes[1024], loaded[1024], before[1024];
	ralTextureAssetReceipt_t asset, stale;
	ralTextureArtifactReceipt_t artifact, stored, loadedReceipt, receiptBefore;
	ralTextureCacheKey_t key, key2, keyBefore;
	ralTextureCacheOps_t ops = { WriteAtomic, Read };
	memoryStore_t store;
	uint64_t loadedLength = UINT64_C(0xAAAAAAAAAAAAAAAA), lengthBefore;
	char keyText[RAL_TEXTURE_CACHE_KEY_TEXT_CAPACITY], textBefore[sizeof( keyText )];
	memset( &store, 0, sizeof( store ) );
	CHECK( MakeArtifact( &asset, artifactBytes, sizeof( artifactBytes ), &artifact ) );
	CHECK( Ral_TextureCacheKeyBuild( &asset, &key )
		&& Ral_TextureCacheKeyBuild( &asset, &key2 )
		&& Ral_TextureCacheKeyExact( &key, &key2 )
		&& Ral_TextureCacheKeyText( &key, keyText, sizeof( keyText ) )
		&& !strcmp( keyText, "wrtex-v1-9e79545020a9ac3e" ) );
	stale = asset; stale.provenanceHash++;
	CHECK( Ral_TextureCacheKeyBuild( &stale, &key2 ) && key2.keyHash != key.keyHash );
	memset( &key2, 0x4A, sizeof( key2 ) ); keyBefore = key2;
	memset( &stored, 0x5B, sizeof( stored ) ); receiptBefore = stored;
	store.failWrite = qtrue;
	CHECK( !Ral_TextureArtifactCacheStore( &ops, &store, artifactBytes,
		artifact.containerByteLength, &asset, &key2, &stored )
		&& !store.present && !memcmp( &key2, &keyBefore, sizeof( key2 ) )
		&& !memcmp( &stored, &receiptBefore, sizeof( stored ) ) );
	store.failWrite = qfalse;
	CHECK( Ral_TextureArtifactCacheStore( &ops, &store, artifactBytes,
		artifact.containerByteLength, &asset, &key2, &stored )
		&& Ral_TextureCacheKeyExact( &key, &key2 )
		&& Ral_TextureArtifactReceiptExact( &artifact, &stored ) );
	memset( loaded, 0xCC, sizeof( loaded ) ); memcpy( before, loaded, sizeof( loaded ) );
	memset( &loadedReceipt, 0x6C, sizeof( loadedReceipt ) ); receiptBefore = loadedReceipt;
	lengthBefore = loadedLength;
	store.present = qfalse;
	CHECK( !Ral_TextureArtifactCacheLoad( &ops, &store, &asset, loaded,
		sizeof( loaded ), &loadedLength, &loadedReceipt )
		&& !memcmp( loaded, before, sizeof( loaded ) ) && loadedLength == lengthBefore
		&& !memcmp( &loadedReceipt, &receiptBefore, sizeof( loadedReceipt ) ) );
	store.present = qtrue; store.truncateRead = qtrue;
	CHECK( !Ral_TextureArtifactCacheLoad( &ops, &store, &asset, loaded,
		sizeof( loaded ), &loadedLength, &loadedReceipt )
		&& !memcmp( loaded, before, sizeof( loaded ) ) && loadedLength == lengthBefore );
	store.truncateRead = qfalse; store.corruptRead = qtrue;
	CHECK( !Ral_TextureArtifactCacheLoad( &ops, &store, &asset, loaded,
		sizeof( loaded ), &loadedLength, &loadedReceipt )
		&& !memcmp( loaded, before, sizeof( loaded ) ) && loadedLength == lengthBefore );
	store.corruptRead = qfalse;
	CHECK( !Ral_TextureArtifactCacheLoad( &ops, &store, &stale, loaded,
		sizeof( loaded ), &loadedLength, &loadedReceipt )
		&& !memcmp( loaded, before, sizeof( loaded ) ) );
	CHECK( !Ral_TextureArtifactCacheLoad( &ops, &store, &asset, loaded,
		artifact.containerByteLength - 1u, &loadedLength, &loadedReceipt )
		&& !memcmp( loaded, before, sizeof( loaded ) ) );
	CHECK( Ral_TextureArtifactCacheLoad( &ops, &store, &asset, loaded,
		sizeof( loaded ), &loadedLength, &loadedReceipt )
		&& loadedLength == artifact.containerByteLength
		&& !memcmp( loaded, artifactBytes, (size_t)loadedLength )
		&& Ral_TextureArtifactReceiptExact( &artifact, &loadedReceipt ) );
	memset( textBefore, 'x', sizeof( textBefore ) );
	CHECK( !Ral_TextureCacheKeyText( &key, textBefore, 25u )
		&& textBefore[0] == 'x' );
	puts( "ral_texture_cache_test: ok" );
	return 0;
}
