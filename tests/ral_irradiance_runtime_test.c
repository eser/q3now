// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "ral_irradiance_runtime.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#define CHECK(x) do { if ( !(x) ) { fprintf( stderr, "FAIL %d: %s\n", __LINE__, #x ); return 1; } } while ( 0 )
#define Q(x) ((x) * RAL_LIGHT_Q16_ONE)

static void PutHalf( uint8_t *bytes, uint16_t half ) {
	bytes[0] = (uint8_t)half; bytes[1] = (uint8_t)( half >> 8u );
}

int main( int argc, char **argv ) {
	ralIrradianceVolumePlacement_t placement;
	ralLightingArtifactDefinition_t definition;
	ralLightingPayloadView_t payloads[2], readPayloads[RAL_LIGHTING_ARTIFACT_MAX_PAYLOADS];
	ralLightingArtifactReceipt_t artifactReceipt, readReceipt;
	ralIrradianceProbeVolume_t volume, untouchedVolume;
	ralIrradianceVolumeSelectionReceipt_t selection;
	ralIrradianceEntitySampleRequest_t request;
	ralIrradianceEntitySampleReceipt_t receipt, untouched;
	ralIrradianceEntitySampleReceipt_t sweepReceipt, sweepExact;
	ralIrradianceDebugReceipt_t debug, debugExact, debugGuard;
	uint8_t coefficients[8u * 24u] = { 0 }, validity[8], artifact[512];
	uint8_t gradientCoefficients[8u * 24u] = { 0 };
	uint32_t probe, channel;

	for ( probe = 0u; probe < 8u; ++probe ) {
		for ( channel = 0u; channel < 3u; ++channel )
			PutHalf( coefficients + probe * 24u + channel * 2u, UINT16_C(0x3c00) );
		PutHalf( coefficients + probe * 24u + 18u, UINT16_C(0x3c00) );
		validity[probe] = 255u;
	}
	memset( &definition, 0, sizeof( definition ) );
	definition.schemaVersion = RAL_LIGHTING_ARTIFACT_SCHEMA_VERSION;
	definition.kind = RAL_LIGHTING_ARTIFACT_IRRADIANCE_VOLUME;
	definition.artifactGeneration = 3u; definition.cacheKey = 4u;
	definition.producerVersion = 5u; definition.encoding = RAL_IRRADIANCE_SH_L1_RGB16F;
	definition.dimensions[0] = definition.dimensions[1] = definition.dimensions[2] = 2u;
	definition.payloadCount = 2u;
	payloads[0] = (ralLightingPayloadView_t){ RAL_LIGHTING_PAYLOAD_SH_COEFFICIENTS, coefficients, sizeof( coefficients ) };
	payloads[1] = (ralLightingPayloadView_t){ RAL_LIGHTING_PAYLOAD_PROBE_VALIDITY, validity, sizeof( validity ) };
	CHECK( Ral_LightingArtifactWrite( &definition, payloads, artifact, sizeof( artifact ), &artifactReceipt ) );
	CHECK( Ral_LightingArtifactRead( artifact, artifactReceipt.byteLength, &readReceipt, readPayloads ) );

	memset( &placement, 0, sizeof( placement ) );
	placement.schemaVersion = RAL_IRRADIANCE_PLACEMENT_SCHEMA_VERSION;
	placement.volumeId = 7u; placement.sourceGeneration = 3u; placement.provenanceHash = 9u;
	placement.spacing.x = placement.spacing.y = placement.spacing.z = Q(1);
	placement.boundsMax.x = placement.boundsMax.y = placement.boundsMax.z = Q(1);
	placement.dimensions[0] = placement.dimensions[1] = placement.dimensions[2] = 2u;
	placement.priority = 10u; placement.blendDistanceQ16 = Q(1) / 4;
	placement.fallback = RAL_IRRADIANCE_FALLBACK_LIGHTGRID; placement.ready = qtrue;
	CHECK( Ral_IrradianceVolumePlacementValid( &placement ) );
	CHECK( Ral_IrradianceVolumeLayoutHash( &placement ) != 0u );
	{
		ralIrradianceVolumePlacement_t placements[2] = { placement, placement };
		ralLightVec3Q16_t point = { Q(1) / 2, Q(1) / 2, Q(1) / 2 };
		placements[0].volumeId = 20u; placements[0].priority = 1u;
		placements[1].volumeId = 10u; placements[1].priority = 2u;
		CHECK( Ral_IrradianceVolumeSelect( placements, 2u, &point, &selection ) );
		CHECK( selection.volumeId == 10u && selection.volumeIndex == 1u );
		CHECK( selection.blendWeightQ16 == RAL_LIGHT_Q16_ONE );
		point.x = 0;
		CHECK( Ral_IrradianceVolumeSelect( placements, 2u, &point, &selection ) );
		CHECK( selection.blendWeightQ16 == 0u );
		point.x = Q(2);
		CHECK( !Ral_IrradianceVolumeSelect( placements, 2u, &point, &selection ) );
	}
	CHECK( Ral_IrradianceRuntimeVolumeBuild( &placement, &readReceipt, &volume ) );

	memset( &request, 0, sizeof( request ) );
	request.schemaVersion = RAL_IRRADIANCE_ENTITY_SAMPLE_SCHEMA_VERSION;
	request.queryGeneration = 11u; request.entityId = 12u;
	request.position.x = request.position.y = request.position.z = Q(1) / 2;
	request.normal.z = Q(1);
	request.fallbackIrradianceQ16[0] = Q(1) / 4;
	request.fallbackIrradianceQ16[1] = Q(1) / 2;
	request.fallbackIrradianceQ16[2] = Q(1);
	CHECK( Ral_IrradianceEntitySample( &placement, &volume, &request,
		readPayloads[0].bytes, readPayloads[0].byteLength,
		(const uint8_t *)readPayloads[1].bytes, (uint32_t)readPayloads[1].byteLength, &receipt ) );
	CHECK( !receipt.usedFallback && receipt.probes.sampleCount == 8u );
	CHECK( receipt.diffuseIrradianceQ16[0] == Q(2) );
	CHECK( receipt.diffuseIrradianceQ16[1] == Q(1) );
	CHECK( receipt.diffuseIrradianceQ16[2] == Q(1) );
	CHECK( receipt.blendedCoefficientsQ16[0][0] == Q(1)
		&& receipt.blendedCoefficientsQ16[0][1] == Q(1)
		&& receipt.blendedCoefficientsQ16[0][2] == Q(1)
		&& receipt.blendedCoefficientsQ16[3][0] == Q(1)
		&& receipt.coefficientHash != 0u );
	/* A deterministic 256-step entity sweep across the X probe cell must be
	 * continuous and repeatable. This is shared by monster, weapon and prop
	 * model submissions, so one contract guards all animated entity roles. */
	for ( probe = 0u; probe < 8u; ++probe ) {
		PutHalf( gradientCoefficients + probe * 24u,
			( probe & 1u ) ? UINT16_C(0x3c00) : UINT16_C(0x3400) );
		PutHalf( gradientCoefficients + probe * 24u + 2u, UINT16_C(0x3800) );
		PutHalf( gradientCoefficients + probe * 24u + 4u, UINT16_C(0x3800) );
	}
	{
		int32_t previousRed = -1;
		for ( uint32_t step = 0u; step <= 256u; ++step ) {
			request.queryGeneration = 100u + step;
			request.position.x = (int32_t)( (int64_t)Q(1) * step / 256u );
			CHECK( Ral_IrradianceEntitySample( &placement, &volume, &request,
				gradientCoefficients, sizeof( gradientCoefficients ), validity,
				sizeof( validity ), &sweepReceipt ) );
			CHECK( Ral_IrradianceEntitySample( &placement, &volume, &request,
				gradientCoefficients, sizeof( gradientCoefficients ), validity,
				sizeof( validity ), &sweepExact ) );
			CHECK( memcmp( &sweepReceipt, &sweepExact, sizeof( sweepReceipt ) ) == 0 );
			CHECK( !sweepReceipt.usedFallback && sweepReceipt.probes.sampleCount > 0u );
			CHECK( sweepReceipt.diffuseIrradianceQ16[0] >= previousRed );
			if ( previousRed >= 0 )
				CHECK( sweepReceipt.diffuseIrradianceQ16[0] - previousRed <= Q(1) / 128 );
			previousRed = sweepReceipt.diffuseIrradianceQ16[0];
		}
	}
	request.position.x = Q(1) / 2;
	if ( argc == 2 && !strcmp( argv[1], "--benchmark" ) ) {
		const uint32_t sampleCount = 500000u;
		struct timespec start, end;
		uint64_t elapsedNanoseconds, sink = 0u;
		CHECK( clock_gettime( CLOCK_MONOTONIC_RAW, &start ) == 0 );
		for ( uint32_t sample = 0u; sample < sampleCount; ++sample ) {
			request.queryGeneration = 1000u + sample;
			request.position.x = (int32_t)( (int64_t)Q(1) * ( sample & 255u ) / 255u );
			CHECK( Ral_IrradianceEntitySample( &placement, &volume, &request,
				gradientCoefficients, sizeof( gradientCoefficients ), validity,
				sizeof( validity ), &sweepReceipt ) );
			sink ^= sweepReceipt.coefficientHash;
		}
		CHECK( clock_gettime( CLOCK_MONOTONIC_RAW, &end ) == 0 );
		elapsedNanoseconds = (uint64_t)( end.tv_sec - start.tv_sec ) * UINT64_C(1000000000)
			+ (uint64_t)( end.tv_nsec - start.tv_nsec );
		printf( "probeSampleNanoseconds=%.3f samples=%u sink=%llu\n",
			(double)elapsedNanoseconds / (double)sampleCount, sampleCount,
			(unsigned long long)sink );
		return 0;
	}
	validity[7] = 0u;
	CHECK( Ral_IrradianceEntitySample( &placement, &volume, &request,
		readPayloads[0].bytes, readPayloads[0].byteLength, validity,
		(uint32_t)sizeof( validity ), &receipt ) );
	CHECK( Ral_IrradianceDebugBuild( &placement, &volume, &receipt,
		readPayloads[0].bytes, readPayloads[0].byteLength, validity,
		(uint32_t)sizeof( validity ), &debug ) );
	CHECK( debug.volumeProbeCount == 8u && debug.validProbeCount == 7u
		&& debug.invalidProbeCount == 1u && debug.selectedProbeCount == 7u
		&& debug.selected[0].validity == 255u
		&& debug.selected[0].weightQ16 > 0u
		&& debug.selected[0].coefficientsQ16[0][0] == Q(1)
		&& debug.contributorHash == receipt.contributorHash
		&& debug.coefficientHash == receipt.coefficientHash );
	debugExact = debug;
	CHECK( Ral_IrradianceDebugReceiptExact( &debug, &debugExact ) );
	debugExact.selected[0].weightQ16++;
	CHECK( !Ral_IrradianceDebugReceiptExact( &debug, &debugExact ) );
	memset( &debugGuard, 0x5a, sizeof( debugGuard ) );
	CHECK( !Ral_IrradianceDebugBuild( &placement, &volume, &receipt,
		readPayloads[0].bytes, readPayloads[0].byteLength, validity, 7u,
		&debugGuard ) );
	CHECK( ((const uint8_t *)&debugGuard)[0] == 0x5au );
	validity[7] = 255u;

	request.position.x = Q(2);
	CHECK( Ral_IrradianceEntitySample( &placement, &volume, &request,
		readPayloads[0].bytes, readPayloads[0].byteLength,
		(const uint8_t *)readPayloads[1].bytes, (uint32_t)readPayloads[1].byteLength, &receipt ) );
	CHECK( receipt.usedFallback && receipt.probes.sampleCount == 0u );
	CHECK( receipt.diffuseIrradianceQ16[0] == Q(1) / 4
		&& receipt.diffuseIrradianceQ16[1] == Q(1) / 2
		&& receipt.diffuseIrradianceQ16[2] == Q(1)
		&& receipt.blendedCoefficientsQ16[0][0] == Q(1) / 4
		&& receipt.blendedCoefficientsQ16[1][0] == 0 );

	memset( &untouched, 0x5a, sizeof( untouched ) );
	request.position.x = Q(1) / 2;
	request.normal.z = 0;
	CHECK( !Ral_IrradianceEntitySample( &placement, &volume, &request,
		readPayloads[0].bytes, readPayloads[0].byteLength,
		(const uint8_t *)readPayloads[1].bytes, (uint32_t)readPayloads[1].byteLength, &untouched ) );
	CHECK( ((const uint8_t *)&untouched)[0] == 0x5au );
	memset( &untouchedVolume, 0x5a, sizeof( untouchedVolume ) );
	placement.sourceGeneration = 8u;
	CHECK( !Ral_IrradianceRuntimeVolumeBuild( &placement, &readReceipt, &untouchedVolume ) );
	CHECK( ((const uint8_t *)&untouchedVolume)[0] == 0x5au );
	placement.sourceGeneration = 3u;
	placement.dimensions[0] = 3u;
	CHECK( !Ral_IrradianceRuntimeVolumeBuild( &placement, &readReceipt, &untouchedVolume ) );
	CHECK( ((const uint8_t *)&untouchedVolume)[0] == 0x5au );
	puts( "ral_irradiance_runtime_test: ok" );
	return 0;
}
