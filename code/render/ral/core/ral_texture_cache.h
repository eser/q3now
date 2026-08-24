// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef WIRED_RAL_TEXTURE_CACHE_H
#define WIRED_RAL_TEXTURE_CACHE_H

#include "ral_texture_asset.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RAL_TEXTURE_CACHE_KEY_SCHEMA_VERSION 1u
#define RAL_TEXTURE_CACHE_KEY_TEXT_CAPACITY 32u

typedef struct {
	uint32_t schemaVersion;
	uint64_t keyHash;
	ralTextureAssetReceipt_t asset;
} ralTextureCacheKey_t;

typedef struct {
	qboolean (*writeAtomic)( void *context, const ralTextureCacheKey_t *key,
		const char *keyText, const void *bytes, uint64_t byteLength );
	qboolean (*read)( void *context, const ralTextureCacheKey_t *key,
		const char *keyText, void *bytes, uint64_t capacity,
		uint64_t *outByteLength );
} ralTextureCacheOps_t;

qboolean Ral_TextureCacheKeyBuild( const ralTextureAssetReceipt_t *asset,
	ralTextureCacheKey_t *outKey );
qboolean Ral_TextureCacheKeyValid( const ralTextureCacheKey_t *key );
qboolean Ral_TextureCacheKeyExact( const ralTextureCacheKey_t *a,
	const ralTextureCacheKey_t *b );
qboolean Ral_TextureCacheKeyText( const ralTextureCacheKey_t *key,
	char *outText, uint32_t capacity );
qboolean Ral_TextureArtifactCacheStore( const ralTextureCacheOps_t *ops,
	void *context, const void *artifactBytes, uint64_t artifactByteLength,
	const ralTextureAssetReceipt_t *expectedAsset, ralTextureCacheKey_t *outKey,
	ralTextureArtifactReceipt_t *outReceipt );
qboolean Ral_TextureArtifactCacheLoad( const ralTextureCacheOps_t *ops,
	void *context, const ralTextureAssetReceipt_t *expectedAsset,
	void *outBytes, uint64_t outCapacity, uint64_t *outByteLength,
	ralTextureArtifactReceipt_t *outReceipt );

#ifdef __cplusplus
}
#endif
#endif
