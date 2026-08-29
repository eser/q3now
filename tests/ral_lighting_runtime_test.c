// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "ral_lighting_product.h"
#include "ral_lighting_runtime.h"

#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if ( !(x) ) { fprintf( stderr, "FAIL %d: %s\n", \
	__LINE__, #x ); return 1; } } while ( 0 )
#define Q(x) ((x) * RAL_LIGHT_Q16_ONE)

static void Request( ralLightingProductRequest_t *request,
		ralStaticLightingEncoding_t encoding ) {
	static const ralLightVec3Q16_t radiance[2] = {
		{ Q(4), Q(1), 0 }, { Q(1), Q(2), Q(3) }
	};
	static const ralLightVec3Q16_t direction[2] = {
		{ 0, 0, Q(1) }, { Q(1), 0, Q(1) }
	};
	static const uint8_t visibility[2] = { 255u, 128u };
	memset( request, 0, sizeof( *request ) );
	request->schemaVersion = RAL_LIGHTING_PRODUCT_SCHEMA_VERSION;
	request->artifactGeneration = 7u;
	request->bake.schemaVersion = RAL_LIGHTING_BAKE_RECEIPT_SCHEMA_VERSION;
	request->bake.bakeGeneration = 1u;
	request->bake.staticIndirectKey = 2u;
	request->bake.producerVersion = 3u;
	request->bake.radianceHash = 4u;
	request->bake.directionHash = 5u;
	request->bake.patchCount = 2u;
	request->bake.linkCount = 1u;
	request->bake.completedBounces = 4u;
	request->bake.ready = qtrue;
	request->encoding = encoding;
	request->pageWidth = 1u;
	request->pageHeight = 1u;
	request->pageCount = 2u;
	request->indirectRadiance = radiance;
	request->dominantDirection = direction;
	request->texelCount = 2u;
	request->stationaryVisibility = visibility;
	request->stationaryVisibilityCount = 2u;
}

int main( void ) {
	ralLightingProductRequest_t request;
	ralLightingArtifactReceipt_t artifactReceipt, wrongReceipt;
	ralLightingRuntimePlan_t plan[RAL_BACKEND_COUNT], repeated, untouched;
	ralStaticLightingCapabilities_t capabilities = { qtrue, qtrue, qtrue, qtrue };
	unsigned char radiance[16], direction[8], artifact[512];
	uint32_t backend;

	Request( &request, RAL_STATIC_LIGHTING_ENCODING_RGB9E5_OCT8 );
	CHECK( Ral_LightingProductWrite( &request, radiance, sizeof( radiance ),
		direction, sizeof( direction ), artifact, sizeof( artifact ),
		&artifactReceipt ) );
	for ( backend = 0u; backend < RAL_BACKEND_COUNT; ++backend ) {
		CHECK( Ral_LightingRuntimePlanBuild( (ralBackendType_t)backend, 11u,
			artifact, artifactReceipt.byteLength, &artifactReceipt,
			&capabilities, &plan[backend] ) );
		CHECK( plan[backend].planes[0].texture.format
			== RAL_FORMAT_E5B9G9R9_UFLOAT );
		CHECK( plan[backend].planes[1].texture.format == RAL_FORMAT_R8G8_UNORM );
		CHECK( plan[backend].planes[2].texture.format == RAL_FORMAT_R8_UNORM );
		CHECK( plan[backend].planes[0].texture.type == RAL_TEXTURE_2D_ARRAY );
		CHECK( plan[backend].planes[0].tightBytesPerRow == 4u );
	}
	CHECK( Ral_LightingRuntimePlanBuild( RAL_BACKEND_WEBGPU, 11u, artifact,
		artifactReceipt.byteLength, &artifactReceipt, &capabilities, &repeated ) );
	CHECK( Ral_LightingRuntimePlanExact( &plan[RAL_BACKEND_WEBGPU], &repeated ) );

	Request( &request, RAL_STATIC_LIGHTING_ENCODING_RGBA16F_OCT16 );
	request.artifactGeneration++;
	CHECK( Ral_LightingProductWrite( &request, radiance, sizeof( radiance ),
		direction, sizeof( direction ), artifact, sizeof( artifact ),
		&artifactReceipt ) );
	CHECK( Ral_LightingRuntimePlanBuild( RAL_BACKEND_METAL, 12u, artifact,
		artifactReceipt.byteLength, &artifactReceipt, &capabilities, &repeated ) );
	CHECK( repeated.planes[0].texture.format == RAL_FORMAT_R16G16B16A16_SFLOAT );
	CHECK( repeated.planes[1].texture.format == RAL_FORMAT_R16G16_SNORM );
	CHECK( repeated.planes[0].tightBytesPerRow == 8u );

	memset( &untouched, 0x5a, sizeof( untouched ) );
	wrongReceipt = artifactReceipt; wrongReceipt.manifestHash++;
	CHECK( !Ral_LightingRuntimePlanBuild( RAL_BACKEND_WEBGPU, 12u, artifact,
		artifactReceipt.byteLength, &wrongReceipt, &capabilities, &untouched ) );
	CHECK( ((const unsigned char *)&untouched)[0] == 0x5au );
	capabilities.sampledRg16Snorm = qfalse;
	CHECK( !Ral_LightingRuntimePlanBuild( RAL_BACKEND_WEBGPU, 12u, artifact,
		artifactReceipt.byteLength, &artifactReceipt, &capabilities, &untouched ) );
	puts( "ral_lighting_runtime_test: ok" );
	return 0;
}
