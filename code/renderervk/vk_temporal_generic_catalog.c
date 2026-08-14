// SPDX-License-Identifier: GPL-3.0-or-later
#include "vk_temporal_generic_catalog.h"

#include <string.h>

#define VK_TEMPORAL_BLOB(name, size) extern const unsigned char name[size]; enum { name##_size = size };
#define VK_TEMPORAL_PAIR(tx, fam, env, fog, ordinaryVS, ordinaryFS, temporalVS, writeFS, invalidateFS)
#include "shaders/spirv/temporal_generic_catalog.inc"
#undef VK_TEMPORAL_PAIR
#undef VK_TEMPORAL_BLOB

#define VK_TEMPORAL_BLOB_VALUE(name) { name, name##_size }

static qboolean BlobEqual( vkTemporalShaderBlob_t a,
		vkTemporalShaderBlob_t b ) {
	return a.bytes == b.bytes && a.size == b.size ? qtrue : qfalse;
}

qboolean VK_TemporalGenericCatalogSelect( const vkTemporalGenericKey_t *key,
		vkTemporalGenericCatalogEntry_t *out ) {
	vkTemporalGenericCatalogEntry_t candidate;
	if ( !key || !out || key->textureCount > 2u
			|| key->family < VK_TEMPORAL_GENERIC_PLAIN
			|| key->family > VK_TEMPORAL_GENERIC_CL
			|| ( key->environment != qfalse && key->environment != qtrue )
			|| ( key->shaderFog != qfalse && key->shaderFog != qtrue ) ) return qfalse;
	memset( &candidate, 0, sizeof(candidate) );
	candidate.key = *key;
#define VK_TEMPORAL_BLOB(name, size)
#define VK_TEMPORAL_PAIR(tx, fam, env, fog, ordinaryVS, ordinaryFS, temporalVS, writeFS, invalidateFS) \
	if ( key->textureCount == (tx) && key->family == VK_TEMPORAL_GENERIC_##fam \
			&& key->environment == (env) && key->shaderFog == (fog) ) { \
		candidate.ordinaryVertex = (vkTemporalShaderBlob_t)VK_TEMPORAL_BLOB_VALUE(ordinaryVS); \
		candidate.ordinaryFragment = (vkTemporalShaderBlob_t)VK_TEMPORAL_BLOB_VALUE(ordinaryFS); \
		candidate.temporalVertex = (vkTemporalShaderBlob_t)VK_TEMPORAL_BLOB_VALUE(temporalVS); \
		candidate.temporalWriteFragment = (vkTemporalShaderBlob_t)VK_TEMPORAL_BLOB_VALUE(writeFS); \
		candidate.temporalInvalidateFragment = (vkTemporalShaderBlob_t)VK_TEMPORAL_BLOB_VALUE(invalidateFS); \
		*out = candidate; return qtrue; \
	}
#include "shaders/spirv/temporal_generic_catalog.inc"
#undef VK_TEMPORAL_PAIR
#undef VK_TEMPORAL_BLOB
	return qfalse;
}

qboolean VK_TemporalGenericCatalogIdentify( vkTemporalShaderBlob_t ordinaryVertex,
		vkTemporalShaderBlob_t ordinaryFragment, vkTemporalGenericKey_t *outKey,
		uint32_t *outCatalogId ) {
	vkTemporalGenericKey_t foundKey;
	uint32_t id = 0, foundId = 0, matches = 0;
	if ( !ordinaryVertex.bytes || !ordinaryVertex.size
			|| !ordinaryFragment.bytes || !ordinaryFragment.size
			|| !outKey || !outCatalogId ) return qfalse;
#define VK_TEMPORAL_BLOB(name, size)
#define VK_TEMPORAL_PAIR(tx, fam, env, fog, ordinaryVS, ordinaryFS, temporalVS, writeFS, invalidateFS) \
	do { \
		const vkTemporalShaderBlob_t vs = VK_TEMPORAL_BLOB_VALUE( ordinaryVS ); \
		const vkTemporalShaderBlob_t fs = VK_TEMPORAL_BLOB_VALUE( ordinaryFS ); \
		if ( BlobEqual( ordinaryVertex, vs ) && BlobEqual( ordinaryFragment, fs ) ) { \
			foundKey.textureCount = (tx); \
			foundKey.family = VK_TEMPORAL_GENERIC_##fam; \
			foundKey.environment = (env) ? qtrue : qfalse; \
			foundKey.shaderFog = (fog) ? qtrue : qfalse; \
			foundId = id; matches++; \
		} \
		id++; \
	} while ( 0 );
#include "shaders/spirv/temporal_generic_catalog.inc"
#undef VK_TEMPORAL_PAIR
#undef VK_TEMPORAL_BLOB
	if ( id != VK_TEMPORAL_GENERIC_CATALOG_COUNT || matches != 1u ) return qfalse;
	*outKey = foundKey;
	*outCatalogId = foundId;
	return qtrue;
}

qboolean VK_TemporalGenericCatalogKeyId( const vkTemporalGenericKey_t *key,
		uint32_t *outCatalogId ) {
	vkTemporalGenericCatalogEntry_t entry;
	vkTemporalGenericKey_t identified;
	uint32_t id;
	if ( !key || !outCatalogId || !VK_TemporalGenericCatalogSelect( key, &entry )
			|| !VK_TemporalGenericCatalogIdentify( entry.ordinaryVertex,
				entry.ordinaryFragment, &identified, &id )
			|| identified.textureCount != key->textureCount
			|| identified.family != key->family
			|| identified.environment != key->environment
			|| identified.shaderFog != key->shaderFog ) return qfalse;
	*outCatalogId = id;
	return qtrue;
}

#undef VK_TEMPORAL_BLOB_VALUE
