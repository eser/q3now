// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "ral_metal_lighting.h"
#include "ral_lighting_product.h"

#include <stdio.h>
#include <string.h>

#define CHECK( condition ) do { if ( !( condition ) ) { \
	fprintf( stderr, "FAIL %d: %s\n", __LINE__, #condition ); return 1; \
} } while ( 0 )

int main( void )
{
	ralMetalCoreCreateInfo_t coreInfo = { 0 };
	ralMetalCore_t *core = NULL;
	ralMetalCoreReceipt_t coreReceipt;
	ralLightingProductRequest_t request;
	ralLightingArtifactReceipt_t artifact;
	ralLightingRuntimePlan_t plan;
	ralStaticLightingCapabilities_t capabilities = { qtrue, qtrue, qtrue, qtrue };
	ralMetalLighting_t *lighting = NULL;
	ralMetalLightingReceipt_t receipt, exact;
	const ralLightVec3Q16_t radiance[2] = {
		{ 4 * RAL_LIGHT_Q16_ONE, RAL_LIGHT_Q16_ONE, 0 },
		{ RAL_LIGHT_Q16_ONE, 2 * RAL_LIGHT_Q16_ONE, 3 * RAL_LIGHT_Q16_ONE }
	};
	const ralLightVec3Q16_t direction[2] = {
		{ 0, 0, RAL_LIGHT_Q16_ONE }, { RAL_LIGHT_Q16_ONE, 0, RAL_LIGHT_Q16_ONE }
	};
	const uint8_t visibility[2] = { 255u, 128u };
	uint8_t radianceBytes[16], directionBytes[8], artifactBytes[512];
	coreInfo.generation = 19u;
	CHECK( RalMetal_CoreCreate( &coreInfo, &core, &coreReceipt ) );
	memset( &request, 0, sizeof( request ) );
	request.schemaVersion = RAL_LIGHTING_PRODUCT_SCHEMA_VERSION;
	request.artifactGeneration = 7u;
	request.bake.schemaVersion = RAL_LIGHTING_BAKE_RECEIPT_SCHEMA_VERSION;
	request.bake.bakeGeneration = 1u;
	request.bake.staticIndirectKey = 2u;
	request.bake.producerVersion = 3u;
	request.bake.radianceHash = 4u;
	request.bake.directionHash = 5u;
	request.bake.patchCount = 2u;
	request.bake.linkCount = 1u;
	request.bake.completedBounces = 4u;
	request.bake.ready = qtrue;
	request.encoding = RAL_STATIC_LIGHTING_ENCODING_RGB9E5_OCT8;
	request.pageWidth = request.pageHeight = 1u;
	request.pageCount = request.texelCount = 2u;
	request.indirectRadiance = radiance;
	request.dominantDirection = direction;
	request.stationaryVisibility = visibility;
	request.stationaryVisibilityCount = 2u;
	CHECK( Ral_LightingProductWrite( &request, radianceBytes, sizeof( radianceBytes ), directionBytes,
									 sizeof( directionBytes ), artifactBytes, sizeof( artifactBytes ), &artifact ) );
	CHECK( Ral_LightingRuntimePlanBuild( RAL_BACKEND_METAL, 20u, artifactBytes, artifact.byteLength, &artifact,
									   &capabilities, &plan ) );
	CHECK( RalMetal_LightingUpload( core, &coreReceipt, artifactBytes, artifact.byteLength, &plan, &lighting,
									  &receipt ) );
	CHECK( receipt.plan.planeCount == 3u && receipt.textureIdentities[0] && receipt.textureIdentities[1] &&
		   receipt.textureIdentities[2] );
	exact = receipt;
	CHECK( RalMetal_LightingReceiptExact( &receipt, &exact ) );
	CHECK( RalMetal_LightingDestroy( core, &coreReceipt, lighting, &receipt ) );
	RalMetal_CoreDestroy( core );
	puts( "ral_metal_lighting_test: ok" );
	return 0;
}
