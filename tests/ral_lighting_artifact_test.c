// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "ral_lighting_artifact.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if ( !(x) ) { fprintf( stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x ); return 1; } } while ( 0 )

static int DirectionalLightmap( void ) {
	static const unsigned char radiance[] = { 1, 2, 3, 4, 5, 6, 7, 8 };
	static const unsigned char direction[] = { 9, 10, 11, 12 };
	static const unsigned char visibility[] = { 255, 192 };
	ralLightingArtifactDefinition_t d; ralLightingPayloadView_t p[3], decoded[5];
	ralLightingArtifactReceipt_t written, read, before; unsigned char a[512], b[512]; uint64_t size;
	memset( &d, 0, sizeof( d ) ); d.schemaVersion = RAL_LIGHTING_ARTIFACT_SCHEMA_VERSION;
	d.kind = RAL_LIGHTING_ARTIFACT_DIRECTIONAL_LIGHTMAP; d.artifactGeneration = 1u;
	d.cacheKey = 2u; d.producerVersion = 3u; d.encoding = RAL_STATIC_LIGHTING_ENCODING_RGB9E5_OCT8;
	d.flags = RAL_LIGHTING_ARTIFACT_STATIC_DIFFUSE_INDIRECT_ONLY
		| RAL_LIGHTING_ARTIFACT_HAS_STATIONARY_VISIBILITY;
	d.dimensions[0] = 2u; d.dimensions[1] = 1u; d.dimensions[2] = 1u; d.payloadCount = 3u;
	p[0] = (ralLightingPayloadView_t){ RAL_LIGHTING_PAYLOAD_RADIANCE, radiance, sizeof( radiance ) };
	p[1] = (ralLightingPayloadView_t){ RAL_LIGHTING_PAYLOAD_DIRECTION, direction, sizeof( direction ) };
	p[2] = (ralLightingPayloadView_t){ RAL_LIGHTING_PAYLOAD_STATIONARY_VISIBILITY, visibility, sizeof( visibility ) };
	CHECK( Ral_LightingArtifactMeasure( &d, p, &size ) && size == 254u );
	CHECK( Ral_LightingArtifactWrite( &d, p, a, sizeof( a ), &written ) );
	CHECK( Ral_LightingArtifactWrite( &d, p, b, sizeof( b ), &read ) );
	CHECK( Ral_LightingArtifactReceiptExact( &written, &read ) && !memcmp( a, b, (size_t)size ) );
	CHECK( Ral_LightingArtifactRead( a, size, &read, decoded ) );
	CHECK( Ral_LightingArtifactReceiptExact( &written, &read )
		&& decoded[0].byteLength == sizeof( radiance )
		&& !memcmp( decoded[0].bytes, radiance, sizeof( radiance ) ) );
	before = read; a[size - 1u] ^= 1u;
	CHECK( !Ral_LightingArtifactRead( a, size, &read, decoded ) && !memcmp( &read, &before, sizeof( read ) ) );
	a[size - 1u] ^= 1u; a[76u] = 1u;
	CHECK( !Ral_LightingArtifactRead( a, size, &read, decoded ) );
	a[76u] = 0u; a[232u] = 1u;
	CHECK( !Ral_LightingArtifactRead( a, size, &read, decoded ) );
	return 0;
}

static int ProbeVolume( void ) {
	static const unsigned char coefficients[192] = { 1, 2, 3, 4 };
	static const unsigned char validity[8] = { 1, 1, 1, 1, 1, 1, 1, 0 };
	ralLightingArtifactDefinition_t d; ralLightingPayloadView_t p[2], decoded[5];
	ralLightingArtifactReceipt_t receipt; unsigned char bytes[512];
	memset( &d, 0, sizeof( d ) ); d.schemaVersion = RAL_LIGHTING_ARTIFACT_SCHEMA_VERSION;
	d.kind = RAL_LIGHTING_ARTIFACT_IRRADIANCE_VOLUME; d.artifactGeneration = 4u;
	d.cacheKey = 5u; d.producerVersion = 6u; d.encoding = RAL_IRRADIANCE_SH_L1_RGB16F;
	d.dimensions[0] = d.dimensions[1] = d.dimensions[2] = 2u; d.payloadCount = 2u;
	p[0] = (ralLightingPayloadView_t){ RAL_LIGHTING_PAYLOAD_SH_COEFFICIENTS, coefficients, sizeof( coefficients ) };
	p[1] = (ralLightingPayloadView_t){ RAL_LIGHTING_PAYLOAD_PROBE_VALIDITY, validity, sizeof( validity ) };
	CHECK( Ral_LightingArtifactWrite( &d, p, bytes, sizeof( bytes ), &receipt ) );
	CHECK( Ral_LightingArtifactRead( bytes, receipt.byteLength, &receipt, decoded ) );
	p[1].role = RAL_LIGHTING_PAYLOAD_RADIANCE;
	CHECK( !Ral_LightingArtifactWrite( &d, p, bytes, sizeof( bytes ), &receipt ) );
	return 0;
}

int main( void ) {
	CHECK( DirectionalLightmap() == 0 ); CHECK( ProbeVolume() == 0 );
	puts( "ral_lighting_artifact_test: ok" ); return 0;
}
