// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "ral_lighting_cook_producer.h"

#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if ( !(x) ) { fprintf( stderr, "FAIL %d: %s\n", __LINE__, #x ); return 1; } } while ( 0 )
#define Q(x) ((x) * RAL_LIGHT_Q16_ONE)

int main( void )
{
	ralLightingBakePatch_t patches[4];
	ralLightingBakeLink_t links[2] = {
		{ 1u, 0u, Q(1) / 3u }, { 2u, 1u, Q(1) / 4u }
	};
	uint32_t texelPatches[8] = { 0u, 0u, 1u, 1u, 2u, 2u, 3u,
		RAL_LIGHTING_COOK_UNMAPPED_TEXEL };
	uint8_t visibility[8] = { 255u, 255u, 220u, 220u, 192u, 192u, 0u, 0u };
	ralIrradianceProductSample_t samples[3] = {
		{ 0u, 0u, { 0, 0, Q(1) }, Q(1), qfalse },
		{ 0u, 1u, { Q(1), 0, 0 }, Q(1), qfalse },
		{ 7u, 2u, { 0, Q(1), 0 }, Q(1), qfalse }
	};
	ralLightingCookProducerRequest_t request;
	ralLightingCookProducerWorkspace_t workspace;
	ralLightingCookProducerReceipt_t first, second, before;
	ralLightVec3Q16_t incoming[4], outgoing[4], patchRadiance[4], patchDirection[4];
	ralLightVec3Q16_t texelRadiance[8], texelDirection[8];
	ralLightingBakeDirectionAccum_t directionAccum[4];
	uint8_t radianceBytes[64], directionBytes[32], coefficientBytes[192], validity[8];
	uint8_t directionalArtifact[2048], irradianceArtifact[2048];
	uint8_t directionalExact[2048], irradianceExact[2048];
	uint32_t index;
	memset( patches, 0, sizeof( patches ) );
	for ( index = 0u; index < 4u; ++index ) {
		patches[index].patchId = 100u + index;
		patches[index].sourceGeneration = 7u;
		patches[index].provenanceHash = 200u + index;
		patches[index].regionId = 300u + index;
		patches[index].centroid.x = Q( (int)index * 4 );
		patches[index].normal.z = Q(1);
		patches[index].areaQ16 = Q(2);
		patches[index].diffuseReflectanceQ16[0] = Q(1) / 2;
		patches[index].diffuseReflectanceQ16[1] = Q(1) / 4;
		patches[index].diffuseReflectanceQ16[2] = Q(1) / 8;
	}
	patches[0].emissionRadianceQ16[0] = Q(6);
	patches[0].emissionRadianceQ16[1] = Q(2);
	patches[0].emissionRadianceQ16[2] = Q(1) / 2;
	memset( &request, 0, sizeof( request ) );
	request.schemaVersion = RAL_LIGHTING_COOK_PRODUCER_SCHEMA_VERSION;
	request.bake.schemaVersion = RAL_LIGHTING_BAKE_SCHEMA_VERSION;
	request.bake.bakeGeneration = 9u;
	request.bake.staticIndirectKey = 10u;
	request.bake.staticBakeHash = 11u;
	request.bake.producerVersion = 12u;
	request.bake.settingsHash = 13u;
	request.bake.patches = patches;
	request.bake.patchCount = 4u;
	request.bake.links = links;
	request.bake.linkCount = 2u;
	request.bake.bounceCount = 4u;
	request.bake.energyClampQ16 = Q(16);
	request.directionalArtifactGeneration = 14u;
	request.directionalEncoding = RAL_STATIC_LIGHTING_ENCODING_RGB9E5_OCT8;
	request.pageWidth = 4u; request.pageHeight = 2u; request.pageCount = 1u;
	request.texelPatchIndices = texelPatches; request.texelCount = 8u;
	request.stationaryVisibility = visibility; request.stationaryVisibilityCount = 8u;
	request.irradianceArtifactGeneration = 15u;
	request.geometryHash = 16u; request.materialHash = 17u; request.layoutHash = 18u;
	request.irradianceDimensions[0] = request.irradianceDimensions[1] =
		request.irradianceDimensions[2] = 2u;
	request.irradianceSamples = samples; request.irradianceSampleCount = 3u;
	memset( &workspace, 0, sizeof( workspace ) );
	workspace.incoming = incoming; workspace.outgoing = outgoing;
	workspace.directionAccum = directionAccum;
	workspace.patchRadiance = patchRadiance; workspace.patchDirection = patchDirection;
	workspace.patchCapacity = 4u;
	workspace.texelRadiance = texelRadiance; workspace.texelDirection = texelDirection;
	workspace.texelCapacity = 8u;
	workspace.directionalRadianceBytes = radianceBytes;
	workspace.directionalRadianceCapacity = sizeof( radianceBytes );
	workspace.directionalDirectionBytes = directionBytes;
	workspace.directionalDirectionCapacity = sizeof( directionBytes );
	workspace.irradianceCoefficientBytes = coefficientBytes;
	workspace.irradianceCoefficientCapacity = sizeof( coefficientBytes );
	workspace.irradianceValidityBytes = validity;
	workspace.irradianceValidityCapacity = sizeof( validity );
	workspace.directionalArtifactBytes = directionalArtifact;
	workspace.directionalArtifactCapacity = sizeof( directionalArtifact );
	workspace.irradianceArtifactBytes = irradianceArtifact;
	workspace.irradianceArtifactCapacity = sizeof( irradianceArtifact );
	CHECK( Ral_LightingCookProducerExecute( &request, &workspace, &first ) &&
		Ral_LightingCookProducerReceiptValid( &first ) &&
		first.bake.clampedChannelCount == 0u && patchRadiance[1].x > patchRadiance[1].y &&
		patchRadiance[2].x > 0 && patchRadiance[3].x == 0 &&
		texelRadiance[7].x == 0 && texelRadiance[7].y == 0 &&
		texelRadiance[7].z == 0 && texelDirection[7].x == 0 &&
		texelDirection[7].y == 0 &&
		texelDirection[7].z == RAL_LIGHT_Q16_ONE &&
		first.artifacts[0].productMask ==
			( RAL_LIGHTING_COOK_DIRECTIONAL_LIGHTMAP |
			  RAL_LIGHTING_COOK_STATIONARY_VISIBILITY ) &&
		first.artifacts[1].productMask == RAL_LIGHTING_COOK_IRRADIANCE_PROBES );
	memcpy( directionalExact, directionalArtifact, (size_t)first.directional.byteLength );
	memcpy( irradianceExact, irradianceArtifact, (size_t)first.irradiance.byteLength );
	CHECK( Ral_LightingCookProducerExecute( &request, &workspace, &second ) &&
		Ral_LightingArtifactReceiptExact( &first.directional, &second.directional ) &&
		Ral_LightingArtifactReceiptExact( &first.irradiance, &second.irradiance ) &&
		!memcmp( directionalExact, directionalArtifact,
			(size_t)second.directional.byteLength ) &&
		!memcmp( irradianceExact, irradianceArtifact,
			(size_t)second.irradiance.byteLength ) );
	before = second; texelPatches[7] = 99u;
	CHECK( !Ral_LightingCookProducerExecute( &request, &workspace, &second ) &&
		!memcmp( &before, &second, sizeof( second ) ) );
	puts( "ral_lighting_cook_producer_test: ok" );
	return 0;
}
