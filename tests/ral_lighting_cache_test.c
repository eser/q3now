// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "ral_lighting_cache.h"

#include <stdio.h>
#include <string.h>

#define CHECK( x ) do { if ( !(x) ) { fprintf( stderr, "FAIL %d: %s\n", __LINE__, #x ); return 1; } } while ( 0 )

typedef struct {
	char key[RAL_LIGHTING_CACHE_KEY_TEXT_CAPACITY];
	unsigned char bytes[512];
	uint64_t length;
	qboolean failWrite;
} fixtureStore_t;

static qboolean WriteAtomic( void *context, const char *key,
	const void *bytes, uint64_t length )
{
	fixtureStore_t *store = (fixtureStore_t *)context;
	if ( store->failWrite || length > sizeof( store->bytes ) ) return qfalse;
	strcpy( store->key, key );
	memcpy( store->bytes, bytes, (size_t)length );
	store->length = length;
	return qtrue;
}

static qboolean Read( void *context, const char *key, void *bytes,
	uint64_t capacity, uint64_t *outLength )
{
	fixtureStore_t *store = (fixtureStore_t *)context;
	if ( strcmp( store->key, key ) || store->length > capacity ) return qfalse;
	memcpy( bytes, store->bytes, (size_t)store->length );
	*outLength = store->length;
	return qtrue;
}

int main( void )
{
	static const unsigned char radiance[4] = { 1, 2, 3, 4 };
	static const unsigned char direction[2] = { 5, 6 };
	ralLightingArtifactDefinition_t definition;
	ralLightingPayloadView_t payloads[2];
	ralLightingArtifactReceipt_t written, stored, loaded, beforeReceipt;
	ralLightingCacheOps_t ops = { WriteAtomic, Read };
	fixtureStore_t store;
	unsigned char artifact[512], output[512], beforeOutput[512];
	uint64_t outputLength = 0u, beforeLength;
	char key[RAL_LIGHTING_CACHE_KEY_TEXT_CAPACITY];

	memset( &definition, 0, sizeof( definition ) );
	definition.schemaVersion = RAL_LIGHTING_ARTIFACT_SCHEMA_VERSION;
	definition.kind = RAL_LIGHTING_ARTIFACT_DIRECTIONAL_LIGHTMAP;
	definition.artifactGeneration = 7u;
	definition.cacheKey = 11u;
	definition.producerVersion = 3u;
	definition.encoding = RAL_STATIC_LIGHTING_ENCODING_RGB9E5_OCT8;
	definition.flags = RAL_LIGHTING_ARTIFACT_STATIC_DIFFUSE_INDIRECT_ONLY;
	definition.dimensions[0] = definition.dimensions[1] = definition.dimensions[2] = 1u;
	definition.payloadCount = 2u;
	payloads[0] = (ralLightingPayloadView_t){ RAL_LIGHTING_PAYLOAD_RADIANCE, radiance, sizeof( radiance ) };
	payloads[1] = (ralLightingPayloadView_t){ RAL_LIGHTING_PAYLOAD_DIRECTION, direction, sizeof( direction ) };
	CHECK( Ral_LightingArtifactWrite( &definition, payloads, artifact,
		sizeof( artifact ), &written ) );
	CHECK( Ral_LightingArtifactCacheKeyText( definition.kind, 7u, 11u,
		key, sizeof( key ) ) );
	CHECK( strstr( key, "wrlight-v1-1-" ) == key );

	memset( &store, 0, sizeof( store ) );
	store.failWrite = qtrue;
	CHECK( !Ral_LightingArtifactCacheStore( &ops, &store, artifact, written.byteLength,
		definition.kind, 7u, 11u, &stored ) );
	store.failWrite = qfalse;
	CHECK( Ral_LightingArtifactCacheStore( &ops, &store, artifact, written.byteLength,
		definition.kind, 7u, 11u, &stored ) );
	CHECK( Ral_LightingArtifactReceiptExact( &written, &stored ) );

	memset( output, 0xa5, sizeof( output ) );
	memcpy( beforeOutput, output, sizeof( output ) );
	memset( &loaded, 0x5a, sizeof( loaded ) ); beforeReceipt = loaded;
	beforeLength = outputLength;
	CHECK( !Ral_LightingArtifactCacheLoad( &ops, &store, definition.kind, 8u, 11u,
		output, sizeof( output ), &outputLength, &loaded ) );
	CHECK( !memcmp( output, beforeOutput, sizeof( output ) ) &&
		!memcmp( &loaded, &beforeReceipt, sizeof( loaded ) ) && outputLength == beforeLength );
	store.bytes[RAL_LIGHTING_ARTIFACT_WIRE_HEADER_BYTES] ^= 1u;
	CHECK( !Ral_LightingArtifactCacheLoad( &ops, &store, definition.kind, 7u, 11u,
		output, sizeof( output ), &outputLength, &loaded ) );
	store.bytes[RAL_LIGHTING_ARTIFACT_WIRE_HEADER_BYTES] ^= 1u;
	CHECK( Ral_LightingArtifactCacheLoad( &ops, &store, definition.kind, 7u, 11u,
		output, sizeof( output ), &outputLength, &loaded ) );
	CHECK( outputLength == written.byteLength && !memcmp( output, artifact, (size_t)outputLength ) &&
		Ral_LightingArtifactReceiptExact( &written, &loaded ) );
	puts( "ral_lighting_cache_test: ok" );
	return 0;
}
