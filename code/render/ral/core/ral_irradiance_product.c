// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "ral_irradiance_product.h"

#include <limits.h>
#include <string.h>

#define FNV64_OFFSET UINT64_C(14695981039346656037)
#define FNV64_PRIME UINT64_C(1099511628211)

static uint64_t HashU32( uint64_t h, uint32_t v ) {
	uint32_t i; for ( i = 0u; i < 4u; ++i ) h = ( h ^ (uint8_t)( v >> ( i * 8u ) ) ) * FNV64_PRIME;
	return h;
}
static uint64_t HashU64( uint64_t h, uint64_t v ) {
	uint32_t i; for ( i = 0u; i < 8u; ++i ) h = ( h ^ (uint8_t)( v >> ( i * 8u ) ) ) * FNV64_PRIME;
	return h;
}
static uint64_t CacheKey( const ralIrradianceProductRequest_t *r ) {
	uint64_t h = HashU32( FNV64_OFFSET, RAL_IRRADIANCE_PRODUCT_SCHEMA_VERSION );
	h = HashU64( h, r->geometryHash ); h = HashU64( h, r->materialHash );
	h = HashU64( h, r->bake.radianceHash ); h = HashU64( h, r->layoutHash );
	h = HashU64( h, r->producerVersion ); h = HashU64( h, r->settingsHash );
	return h ? h : 1u;
}

static uint16_t UnsignedQ16ToHalf( uint32_t value ) {
	uint32_t highest = 0u, exponent, mantissa, base, shift;
	if ( !value ) return 0u;
	while ( highest < 30u && ( value >> ( highest + 1u ) ) ) highest++;
	if ( highest < 2u ) return (uint16_t)( value << 8u );
	exponent = highest - 1u; base = 1u << highest;
	if ( highest <= 10u ) mantissa = ( value - base ) << ( 10u - highest );
	else { shift = highest - 10u; mantissa = ( ( value - base )
		+ ( 1u << ( shift - 1u ) ) ) >> shift; }
	if ( mantissa == 1024u ) { mantissa = 0u; exponent++; }
	if ( exponent >= 31u ) return UINT16_C(0x7bff);
	return (uint16_t)( ( exponent << 10u ) | mantissa );
}

static uint16_t SignedQ16ToHalf( int32_t value ) {
	uint32_t magnitude;
	if ( value >= 0 ) return UnsignedQ16ToHalf( (uint32_t)value );
	magnitude = value == INT32_MIN ? (uint32_t)INT32_MAX + 1u : (uint32_t)-value;
	return (uint16_t)( UINT16_C(0x8000) | UnsignedQ16ToHalf( magnitude ) );
}

static void Put16( uint8_t *bytes, uint16_t value ) {
	bytes[0] = (uint8_t)value; bytes[1] = (uint8_t)( value >> 8u );
}

static qboolean RequestValid( const ralIrradianceProductRequest_t *r,
		uint32_t *outProbeCount ) {
	uint64_t probes;
	if ( !r || !outProbeCount
			|| r->schemaVersion != RAL_IRRADIANCE_PRODUCT_SCHEMA_VERSION
			|| !r->artifactGeneration || !r->producerVersion || !r->geometryHash
			|| !r->materialHash || !r->layoutHash || !r->settingsHash
			|| !Ral_LightingBakeReceiptValid( &r->bake )
			|| r->encoding != RAL_IRRADIANCE_SH_L1_RGB16F
			|| r->patchCount != r->bake.patchCount || !r->patchRadiance
			|| r->sampleCount > RAL_IRRADIANCE_PRODUCT_MAX_SAMPLES
			|| ( r->sampleCount && !r->samples )
			|| r->dimensions[0] < 2u || r->dimensions[1] < 2u
			|| r->dimensions[2] < 2u ) return qfalse;
	probes = (uint64_t)r->dimensions[0] * r->dimensions[1] * r->dimensions[2];
	if ( !probes || probes > UINT32_MAX ) return qfalse;
	*outProbeCount = (uint32_t)probes; return qtrue;
}

qboolean Ral_IrradianceProductWrite( const ralIrradianceProductRequest_t *r,
		void *coefficientMemory, uint64_t coefficientCapacity,
		uint8_t *validity, uint32_t validityCapacity, void *artifactBytes,
		uint64_t artifactCapacity, ralLightingArtifactReceipt_t *outReceipt ) {
	uint8_t *coefficients = (uint8_t *)coefficientMemory;
	ralLightingArtifactDefinition_t definition;
	ralLightingPayloadView_t payloads[2];
	uint32_t probeCount, probe, sampleCursor = 0u;
	uint64_t coefficientBytes;
	if ( !RequestValid( r, &probeCount ) || !coefficients || !validity
			|| !artifactBytes || !outReceipt ) return qfalse;
	coefficientBytes = (uint64_t)probeCount * 24u;
	if ( coefficientCapacity < coefficientBytes || validityCapacity < probeCount ) return qfalse;
	memset( coefficients, 0, (size_t)coefficientBytes );
	memset( validity, 0, probeCount );
	for ( probe = 0u; probe < probeCount; ++probe ) {
		int64_t accum[4][3] = { { 0 } };
		uint64_t totalWeight = 0u;
		uint32_t channel, coefficient;
		while ( sampleCursor < r->sampleCount && r->samples[sampleCursor].probeIndex < probe )
			return qfalse;
		while ( sampleCursor < r->sampleCount && r->samples[sampleCursor].probeIndex == probe ) {
			const ralIrradianceProductSample_t *sample = &r->samples[sampleCursor++];
			const ralLightVec3Q16_t *radiance;
			uint32_t previousPatch = sampleCursor > 1u ? r->samples[sampleCursor - 2u].patchIndex : 0u;
			if ( sample->patchIndex >= r->patchCount || sample->weightQ16 > RAL_LIGHT_Q16_ONE
					|| ( sampleCursor > 1u && r->samples[sampleCursor - 2u].probeIndex == probe
						&& previousPatch >= sample->patchIndex ) ) return qfalse;
			if ( sample->occluded || !sample->weightQ16 ) continue;
			radiance = &r->patchRadiance[sample->patchIndex];
			if ( radiance->x < 0 || radiance->y < 0 || radiance->z < 0 ) return qfalse;
			for ( channel = 0u; channel < 3u; ++channel ) {
				int32_t value = channel == 0u ? radiance->x : channel == 1u ? radiance->y : radiance->z;
				accum[0][channel] += (int64_t)value * sample->weightQ16;
				accum[1][channel] += (int64_t)value * sample->weightQ16
					* sample->directionToPatch.x / RAL_LIGHT_Q16_ONE;
				accum[2][channel] += (int64_t)value * sample->weightQ16
					* sample->directionToPatch.y / RAL_LIGHT_Q16_ONE;
				accum[3][channel] += (int64_t)value * sample->weightQ16
					* sample->directionToPatch.z / RAL_LIGHT_Q16_ONE;
			}
			totalWeight += sample->weightQ16;
		}
		if ( !totalWeight ) continue;
		for ( coefficient = 0u; coefficient < 4u; ++coefficient ) {
			for ( channel = 0u; channel < 3u; ++channel ) {
				int64_t value = accum[coefficient][channel] / (int64_t)totalWeight;
				if ( value < INT32_MIN || value > INT32_MAX ) return qfalse;
				Put16( coefficients + (uint64_t)probe * 24u
					+ coefficient * 6u + channel * 2u,
					SignedQ16ToHalf( (int32_t)value ) );
			}
		}
		validity[probe] = 255u;
	}
	if ( sampleCursor != r->sampleCount ) return qfalse;
	memset( &definition, 0, sizeof( definition ) );
	definition.schemaVersion = RAL_LIGHTING_ARTIFACT_SCHEMA_VERSION;
	definition.kind = RAL_LIGHTING_ARTIFACT_IRRADIANCE_VOLUME;
	definition.artifactGeneration = r->artifactGeneration;
	definition.cacheKey = CacheKey( r );
	definition.producerVersion = r->producerVersion;
	definition.encoding = r->encoding;
	memcpy( definition.dimensions, r->dimensions, sizeof( definition.dimensions ) );
	definition.payloadCount = 2u;
	payloads[0] = (ralLightingPayloadView_t){ RAL_LIGHTING_PAYLOAD_SH_COEFFICIENTS,
		coefficients, coefficientBytes };
	payloads[1] = (ralLightingPayloadView_t){ RAL_LIGHTING_PAYLOAD_PROBE_VALIDITY,
		validity, probeCount };
	return Ral_LightingArtifactWrite( &definition, payloads, artifactBytes,
		artifactCapacity, outReceipt );
}
