// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_texture_cache.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FNV64_OFFSET UINT64_C(14695981039346656037)
#define FNV64_PRIME UINT64_C(1099511628211)

static uint64_t HashByte( uint64_t hash, unsigned char byte ) {
	return ( hash ^ byte ) * FNV64_PRIME;
}

static uint64_t HashU32( uint64_t hash, uint32_t value ) {
	uint32_t i;
	for ( i = 0u; i < 4u; i++ ) hash = HashByte( hash,
		(unsigned char)( value >> ( i * 8u ) ) );
	return hash;
}

static uint64_t HashU64( uint64_t hash, uint64_t value ) {
	uint32_t i;
	for ( i = 0u; i < 8u; i++ ) hash = HashByte( hash,
		(unsigned char)( value >> ( i * 8u ) ) );
	return hash;
}

static uint64_t HashAsset( const ralTextureAssetReceipt_t *a ) {
	uint64_t h = FNV64_OFFSET;
	h = HashU32( h, RAL_TEXTURE_CACHE_KEY_SCHEMA_VERSION );
	h = HashU32( h, a->schemaVersion ); h = HashU64( h, a->assetGeneration );
	h = HashU64( h, a->provenanceHash ); h = HashU32( h, (uint32_t)a->dimension );
	h = HashU32( h, a->width ); h = HashU32( h, a->height );
	h = HashU32( h, a->depth ); h = HashU32( h, a->layers );
	h = HashU32( h, a->sourceMipLevels ); h = HashU32( h, a->targetMipLevels );
	h = HashU32( h, (uint32_t)a->colorEncoding );
	h = HashU32( h, (uint32_t)a->channelSemantic );
	h = HashU32( h, (uint32_t)a->sourceEncoding );
	h = HashU32( h, (uint32_t)a->mipPolicy );
	h = HashU32( h, (uint32_t)a->residency ); h = HashU32( h, (uint32_t)a->hasAlpha );
	h = HashU32( h, (uint32_t)a->selectedCompression );
	h = HashU32( h, (uint32_t)a->targetFormat );
	h = HashU32( h, (uint32_t)a->transcodeRequired );
	h = HashU32( h, (uint32_t)a->deterministicFallback );
	return h ? h : 1u;
}

qboolean Ral_TextureCacheKeyBuild( const ralTextureAssetReceipt_t *asset,
		ralTextureCacheKey_t *out ) {
	ralTextureCacheKey_t v;
	if ( !out || !Ral_TextureAssetReceiptValid( asset ) ) return qfalse;
	memset( &v, 0, sizeof( v ) );
	v.schemaVersion = RAL_TEXTURE_CACHE_KEY_SCHEMA_VERSION;
	v.keyHash = HashAsset( asset ); v.asset = *asset;
	*out = v; return qtrue;
}

qboolean Ral_TextureCacheKeyValid( const ralTextureCacheKey_t *key ) {
	return key && key->schemaVersion == RAL_TEXTURE_CACHE_KEY_SCHEMA_VERSION
		&& key->keyHash && Ral_TextureAssetReceiptValid( &key->asset )
		&& key->keyHash == HashAsset( &key->asset );
}

qboolean Ral_TextureCacheKeyExact( const ralTextureCacheKey_t *a,
		const ralTextureCacheKey_t *b ) {
	return Ral_TextureCacheKeyValid( a ) && Ral_TextureCacheKeyValid( b )
		&& a->schemaVersion == b->schemaVersion && a->keyHash == b->keyHash
		&& Ral_TextureAssetReceiptExact( &a->asset, &b->asset );
}

qboolean Ral_TextureCacheKeyText( const ralTextureCacheKey_t *key,
		char *out, uint32_t capacity ) {
	char text[RAL_TEXTURE_CACHE_KEY_TEXT_CAPACITY];
	int length;
	if ( !out || !Ral_TextureCacheKeyValid( key ) ) return qfalse;
	length = snprintf( text, sizeof( text ), "wrtex-v1-%016" PRIx64,
		key->keyHash );
	if ( length != 25 || capacity <= (uint32_t)length ) return qfalse;
	memcpy( out, text, (size_t)length + 1u ); return qtrue;
}

qboolean Ral_TextureArtifactCacheStore( const ralTextureCacheOps_t *ops,
		void *context, const void *bytes, uint64_t length,
		const ralTextureAssetReceipt_t *expectedAsset, ralTextureCacheKey_t *outKey,
		ralTextureArtifactReceipt_t *outReceipt ) {
	ralTextureArtifactReceipt_t receipt;
	ralTextureCacheKey_t key;
	char keyText[RAL_TEXTURE_CACHE_KEY_TEXT_CAPACITY];
	if ( !ops || !ops->writeAtomic || !bytes || !outKey || !outReceipt
			|| !Ral_DecodeTextureArtifact( bytes, length, expectedAsset, &receipt )
			|| !Ral_TextureCacheKeyBuild( expectedAsset, &key )
			|| !Ral_TextureCacheKeyText( &key, keyText, sizeof( keyText ) )
			|| !ops->writeAtomic( context, &key, keyText, bytes, length ) ) return qfalse;
	*outKey = key; *outReceipt = receipt; return qtrue;
}

qboolean Ral_TextureArtifactCacheLoad( const ralTextureCacheOps_t *ops,
		void *context, const ralTextureAssetReceipt_t *expectedAsset,
		void *outBytes, uint64_t capacity, uint64_t *outLength,
		ralTextureArtifactReceipt_t *outReceipt ) {
	ralTextureCacheKey_t key;
	ralTextureArtifactReceipt_t receipt;
	char keyText[RAL_TEXTURE_CACHE_KEY_TEXT_CAPACITY];
	unsigned char *scratch;
	uint64_t length = 0u;
	qboolean ok = qfalse;
	if ( !ops || !ops->read || !outBytes || !outLength || !outReceipt
			|| !capacity || capacity > (uint64_t)SIZE_MAX
			|| !Ral_TextureCacheKeyBuild( expectedAsset, &key )
			|| !Ral_TextureCacheKeyText( &key, keyText, sizeof( keyText ) ) ) return qfalse;
	scratch = (unsigned char *)malloc( (size_t)capacity );
	if ( !scratch ) return qfalse;
	if ( ops->read( context, &key, keyText, scratch, capacity, &length )
			&& length && length <= capacity
			&& Ral_DecodeTextureArtifact( scratch, length, expectedAsset, &receipt ) ) {
		memcpy( outBytes, scratch, (size_t)length );
		*outLength = length; *outReceipt = receipt; ok = qtrue;
	}
	free( scratch ); return ok;
}
