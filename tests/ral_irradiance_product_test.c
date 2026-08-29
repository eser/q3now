// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "ral_irradiance_product.h"

#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if ( !(x) ) { fprintf( stderr, "FAIL %d: %s\n", __LINE__, #x ); return 1; } } while ( 0 )
#define Q(x) ((x) * RAL_LIGHT_Q16_ONE)

int main( void ) {
	ralIrradianceProductRequest_t request;
	ralLightingArtifactReceipt_t first, second, read;
	ralLightingPayloadView_t views[RAL_LIGHTING_ARTIFACT_MAX_PAYLOADS];
	ralLightVec3Q16_t radiance[2] = { { Q(4), Q(1), 0 }, { Q(1), Q(2), Q(3) } };
	ralIrradianceProductSample_t samples[4] = {
		{ 0u, 0u, { Q(1), 0, 0 }, Q(1), qfalse },
		{ 0u, 1u, { 0, Q(1), 0 }, Q(1), qfalse },
		{ 1u, 0u, { 0, 0, Q(1) }, Q(1), qtrue },
		{ 7u, 1u, { 0, 0, -Q(1) }, Q(1), qfalse }
	};
	uint8_t coefficients[8u * 24u], validity[8], artifact[512], repeat[512];
	memset( &request, 0, sizeof( request ) );
	request.schemaVersion = RAL_IRRADIANCE_PRODUCT_SCHEMA_VERSION;
	request.artifactGeneration = 9u; request.producerVersion = 10u;
	request.geometryHash = 11u; request.materialHash = 12u;
	request.layoutHash = 13u; request.settingsHash = 14u;
	request.bake.schemaVersion = RAL_LIGHTING_BAKE_RECEIPT_SCHEMA_VERSION;
	request.bake.bakeGeneration = 1u; request.bake.staticIndirectKey = 2u;
	request.bake.producerVersion = 10u; request.bake.radianceHash = 3u;
	request.bake.directionHash = 4u; request.bake.patchCount = 2u;
	request.bake.linkCount = 1u; request.bake.completedBounces = 4u;
	request.bake.ready = qtrue;
	request.dimensions[0] = request.dimensions[1] = request.dimensions[2] = 2u;
	request.encoding = RAL_IRRADIANCE_SH_L1_RGB16F;
	request.patchRadiance = radiance; request.patchCount = 2u;
	request.samples = samples; request.sampleCount = 4u;
	CHECK( Ral_IrradianceProductWrite( &request, coefficients, sizeof( coefficients ),
		validity, sizeof( validity ), artifact, sizeof( artifact ), &first ) );
	CHECK( validity[0] == 255u && validity[1] == 0u && validity[7] == 255u );
	CHECK( Ral_LightingArtifactRead( artifact, first.byteLength, &read, views ) );
	CHECK( Ral_LightingArtifactReceiptExact( &first, &read ) );
	CHECK( views[0].byteLength == sizeof( coefficients )
		&& views[1].byteLength == sizeof( validity ) );
	CHECK( coefficients[6] == 0u && coefficients[7] == 0x40u );
	CHECK( coefficients[7u * 24u + 18u + 1u] & 0x80u );
	CHECK( Ral_IrradianceProductWrite( &request, coefficients, sizeof( coefficients ),
		validity, sizeof( validity ), repeat, sizeof( repeat ), &second ) );
	CHECK( Ral_LightingArtifactReceiptExact( &first, &second )
		&& !memcmp( artifact, repeat, (size_t)first.byteLength ) );
	samples[1].patchIndex = 0u;
	CHECK( !Ral_IrradianceProductWrite( &request, coefficients, sizeof( coefficients ),
		validity, sizeof( validity ), repeat, sizeof( repeat ), &second ) );
	request.encoding = RAL_IRRADIANCE_SH_L1_RGB9E5;
	CHECK( !Ral_IrradianceProductWrite( &request, coefficients, sizeof( coefficients ),
		validity, sizeof( validity ), repeat, sizeof( repeat ), &second ) );
	puts( "ral_irradiance_product_test: ok" );
	return 0;
}
