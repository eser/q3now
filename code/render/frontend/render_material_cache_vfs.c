// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "render_material_cache_vfs.h"

#include <stdio.h>
#include <string.h>

typedef enum {
	MATERIAL_CACHE_VFS_INVALID = 0,
	MATERIAL_CACHE_VFS_PRODUCT,
	MATERIAL_CACHE_VFS_PACKAGE
} materialCacheVfsKind_t;

static materialCacheVfsKind_t KeyKind( uint64_t artifactHash, const char *key ) {
	char expected[RAL_MATERIAL_PACKAGE_KEY_TEXT_CAPACITY];
	if ( !artifactHash || !key ) return MATERIAL_CACHE_VFS_INVALID;
	if ( Ral_MaterialCookCacheKeyText( artifactHash, expected,
			sizeof( expected ) ) && !strcmp( key, expected ) )
		return MATERIAL_CACHE_VFS_PRODUCT;
	if ( Ral_MaterialPackageCacheKeyText( artifactHash, expected,
			sizeof( expected ) ) && !strcmp( key, expected ) )
		return MATERIAL_CACHE_VFS_PACKAGE;
	return MATERIAL_CACHE_VFS_INVALID;
}

static const char *Suffix( materialCacheVfsKind_t kind ) {
	if ( kind == MATERIAL_CACHE_VFS_PRODUCT ) return ".wrmat";
	if ( kind == MATERIAL_CACHE_VFS_PACKAGE ) return ".wrmatpkg";
	return NULL;
}

static qboolean Qpath( const renderMaterialCacheVfs_t *cache,
		uint64_t artifactHash, const char *key, char out[MAX_QPATH] ) {
	const char *suffix;
	int length;
	if ( !cache || cache->ready != qtrue || !out
			|| !( suffix = Suffix( KeyKind( artifactHash, key ) ) ) ) return qfalse;
	length = snprintf( out, MAX_QPATH, "%s/%s%s", cache->root, key, suffix );
	return length > 0 && length < MAX_QPATH ? qtrue : qfalse;
}

static qboolean Read( void *opaque, uint64_t artifactHash, const char *key,
		void *bytes, uint64_t capacity, uint64_t *outLength ) {
	renderMaterialCacheVfs_t *cache = opaque;
	const refimport_t *imports = cache ? cache->imports : NULL;
	char requested[MAX_QPATH], canonical[MAX_QPATH], verifiedCanonical[MAX_QPATH];
	uint64_t sourceId = 0u, byteSize = 0u, verifiedSourceId = 0u,
		verifiedByteSize = 0u;
	unsigned generation = 0u, verifiedGeneration = 0u;
	void *sourceBytes = NULL;
	int readLength;
	if ( !bytes || !capacity || !outLength || capacity > (uint64_t)SIZE_MAX
			|| !imports || !imports->FS_ResolveResource || !imports->FS_ReadFile
			|| !imports->FS_FreeFile
			|| !Qpath( cache, artifactHash, key, requested )
			|| !imports->FS_ResolveResource( requested, canonical,
				sizeof( canonical ), &sourceId, &byteSize, &generation )
			|| strcmp( requested, canonical ) || !sourceId || !byteSize
			|| !generation || byteSize > capacity ) return qfalse;
	readLength = imports->FS_ReadFile( canonical, &sourceBytes );
	if ( readLength <= 0 || !sourceBytes || (uint64_t)readLength != byteSize
			|| !imports->FS_ResolveResource( requested, verifiedCanonical,
				sizeof( verifiedCanonical ), &verifiedSourceId,
				&verifiedByteSize, &verifiedGeneration )
			|| strcmp( canonical, verifiedCanonical )
			|| sourceId != verifiedSourceId || byteSize != verifiedByteSize
			|| generation != verifiedGeneration ) {
		if ( sourceBytes ) imports->FS_FreeFile( sourceBytes );
		return qfalse;
	}
	memcpy( bytes, sourceBytes, (size_t)byteSize );
	imports->FS_FreeFile( sourceBytes );
	*outLength = byteSize;
	return qtrue;
}

static qboolean ReadProjectReference( void *opaque, const char *key,
		void *bytes, uint64_t capacity, uint64_t *outLength ) {
	renderMaterialCacheVfs_t *cache = opaque;
	const refimport_t *imports = cache ? cache->imports : NULL;
	char requested[MAX_QPATH], canonical[MAX_QPATH], verifiedCanonical[MAX_QPATH];
	uint64_t sourceId = 0u, byteSize = 0u, verifiedSourceId = 0u,
		verifiedByteSize = 0u;
	unsigned generation = 0u, verifiedGeneration = 0u;
	void *sourceBytes = NULL;
	int readLength, qpathLength;
	if ( !key || strlen( key ) != 33u
			|| strncmp( key, "wrmp-v1-", 8u )
			|| strspn( key + 8u, "0123456789abcdef" ) != 16u
			|| key[24] != '-'
			|| strspn( key + 25u, "0123456789abcdef" ) != 8u
			|| !bytes || !capacity || !outLength
			|| capacity > (uint64_t)SIZE_MAX || !imports
			|| !imports->FS_ResolveResource || !imports->FS_ReadFile
			|| !imports->FS_FreeFile ) return qfalse;
	qpathLength = snprintf( requested, sizeof( requested ), "%s/%s.wrmatref",
		cache->root, key );
	if ( qpathLength <= 0 || qpathLength >= (int)sizeof( requested )
			|| !imports->FS_ResolveResource( requested, canonical,
				sizeof( canonical ), &sourceId, &byteSize, &generation )
			|| strcmp( requested, canonical ) || !sourceId || !byteSize
			|| !generation || byteSize > capacity ) return qfalse;
	readLength = imports->FS_ReadFile( canonical, &sourceBytes );
	if ( readLength <= 0 || !sourceBytes || (uint64_t)readLength != byteSize
			|| !imports->FS_ResolveResource( requested, verifiedCanonical,
				sizeof( verifiedCanonical ), &verifiedSourceId,
				&verifiedByteSize, &verifiedGeneration )
			|| strcmp( canonical, verifiedCanonical )
			|| sourceId != verifiedSourceId || byteSize != verifiedByteSize
			|| generation != verifiedGeneration ) {
		if ( sourceBytes ) imports->FS_FreeFile( sourceBytes );
		return qfalse;
	}
	memcpy( bytes, sourceBytes, (size_t)byteSize );
	imports->FS_FreeFile( sourceBytes );
	*outLength = byteSize;
	return qtrue;
}

qboolean RenderMaterialCacheVfs_Init( renderMaterialCacheVfs_t *cache,
		const refimport_t *imports, const char *root ) {
	size_t length;
	if ( !cache || !imports || !imports->FS_ResolveResource
			|| !imports->FS_ReadFile || !imports->FS_FreeFile || !root
			|| !( length = strlen( root ) ) || length >= sizeof( cache->root )
			|| root[0] == '/' || root[length - 1u] == '/'
			|| root[length - 1u] == '\\' || strstr( root, ".." ) ) return qfalse;
	memset( cache, 0, sizeof( *cache ) );
	cache->imports = imports;
	memcpy( cache->root, root, length + 1u );
	cache->ready = qtrue;
	return qtrue;
}

const ralMaterialCookCacheOps_t *RenderMaterialCacheVfs_MaterialOps( void ) {
	static const ralMaterialCookCacheOps_t ops = { NULL, Read };
	return &ops;
}

const ralMaterialPackageCacheOps_t *RenderMaterialCacheVfs_PackageOps( void ) {
	static const ralMaterialPackageCacheOps_t ops = { NULL, Read };
	return &ops;
}

const ralMaterialProjectReferenceOps_t *
		RenderMaterialCacheVfs_ProjectReferenceOps( void ) {
	static const ralMaterialProjectReferenceOps_t ops = {
		NULL, ReadProjectReference
	};
	return &ops;
}
