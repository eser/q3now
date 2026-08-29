// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "ral_irradiance_runtime.h"

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

qboolean Ral_IrradianceVolumePlacementValid(
		const ralIrradianceVolumePlacement_t *p ) {
	uint32_t axis;
	if ( !p || p->schemaVersion != RAL_IRRADIANCE_PLACEMENT_SCHEMA_VERSION
			|| !p->volumeId || !p->sourceGeneration
			|| p->sourceGeneration == UINT64_MAX || !p->provenanceHash
			|| !p->priority || p->blendDistanceQ16 > (uint32_t)INT32_MAX
			|| p->fallback < RAL_IRRADIANCE_FALLBACK_LIGHTGRID
			|| p->fallback > RAL_IRRADIANCE_FALLBACK_GLOBAL_AMBIENT
			|| !p->ready ) return qfalse;
	for ( axis = 0u; axis < 3u; ++axis ) {
		const int32_t origin = axis == 0u ? p->origin.x : axis == 1u ? p->origin.y : p->origin.z;
		const int32_t spacing = axis == 0u ? p->spacing.x : axis == 1u ? p->spacing.y : p->spacing.z;
		const int32_t minimum = axis == 0u ? p->boundsMin.x : axis == 1u ? p->boundsMin.y : p->boundsMin.z;
		const int32_t maximum = axis == 0u ? p->boundsMax.x : axis == 1u ? p->boundsMax.y : p->boundsMax.z;
		int64_t last;
		if ( spacing <= 0 || p->dimensions[axis] < 2u || minimum > origin || maximum <= minimum ) return qfalse;
		last = (int64_t)origin + (int64_t)spacing * ( p->dimensions[axis] - 1u );
		if ( last > INT32_MAX || last > maximum ) return qfalse;
	}
	return qtrue;
}

uint64_t Ral_IrradianceVolumeLayoutHash(
		const ralIrradianceVolumePlacement_t *p ) {
	uint64_t h;
	if ( !Ral_IrradianceVolumePlacementValid( p ) ) return 0u;
	h = HashU32( FNV64_OFFSET, RAL_IRRADIANCE_PLACEMENT_SCHEMA_VERSION );
	h = HashU64( h, p->volumeId ); h = HashU64( h, p->sourceGeneration );
	h = HashU64( h, p->provenanceHash );
	h = HashU32( h, (uint32_t)p->origin.x ); h = HashU32( h, (uint32_t)p->origin.y ); h = HashU32( h, (uint32_t)p->origin.z );
	h = HashU32( h, (uint32_t)p->spacing.x ); h = HashU32( h, (uint32_t)p->spacing.y ); h = HashU32( h, (uint32_t)p->spacing.z );
	h = HashU32( h, (uint32_t)p->boundsMin.x ); h = HashU32( h, (uint32_t)p->boundsMin.y ); h = HashU32( h, (uint32_t)p->boundsMin.z );
	h = HashU32( h, (uint32_t)p->boundsMax.x ); h = HashU32( h, (uint32_t)p->boundsMax.y ); h = HashU32( h, (uint32_t)p->boundsMax.z );
	h = HashU32( h, p->dimensions[0] ); h = HashU32( h, p->dimensions[1] ); h = HashU32( h, p->dimensions[2] );
	h = HashU32( h, p->priority ); h = HashU32( h, p->blendDistanceQ16 ); h = HashU32( h, p->fallback );
	return h ? h : 1u;
}

qboolean Ral_IrradianceVolumeSelect(
		const ralIrradianceVolumePlacement_t *placements, uint32_t count,
		const ralLightVec3Q16_t *position,
		ralIrradianceVolumeSelectionReceipt_t *out ) {
	ralIrradianceVolumeSelectionReceipt_t candidate;
	uint32_t index, best = UINT32_MAX;
	int32_t bestEdge = 0;
	if ( !placements || !count || count > RAL_IRRADIANCE_MAX_VOLUMES
			|| !position || !out ) return qfalse;
	for ( index = 0u; index < count; ++index ) {
		const ralIrradianceVolumePlacement_t *p = &placements[index];
		int32_t edge;
		if ( !Ral_IrradianceVolumePlacementValid( p ) ) return qfalse;
		if ( position->x < p->boundsMin.x || position->x > p->boundsMax.x
				|| position->y < p->boundsMin.y || position->y > p->boundsMax.y
				|| position->z < p->boundsMin.z || position->z > p->boundsMax.z ) continue;
		edge = position->x - p->boundsMin.x;
		if ( p->boundsMax.x - position->x < edge ) edge = p->boundsMax.x - position->x;
		if ( position->y - p->boundsMin.y < edge ) edge = position->y - p->boundsMin.y;
		if ( p->boundsMax.y - position->y < edge ) edge = p->boundsMax.y - position->y;
		if ( position->z - p->boundsMin.z < edge ) edge = position->z - p->boundsMin.z;
		if ( p->boundsMax.z - position->z < edge ) edge = p->boundsMax.z - position->z;
		if ( best == UINT32_MAX || p->priority > placements[best].priority
				|| ( p->priority == placements[best].priority
					&& p->volumeId < placements[best].volumeId ) ) {
			best = index; bestEdge = edge;
		}
	}
	if ( best == UINT32_MAX ) return qfalse;
	memset( &candidate, 0, sizeof( candidate ) );
	candidate.schemaVersion = RAL_IRRADIANCE_SELECTION_RECEIPT_SCHEMA_VERSION;
	candidate.volumeId = placements[best].volumeId;
	candidate.layoutHash = Ral_IrradianceVolumeLayoutHash( &placements[best] );
	candidate.volumeIndex = best; candidate.priority = placements[best].priority;
	if ( !placements[best].blendDistanceQ16 || bestEdge >= (int32_t)placements[best].blendDistanceQ16 )
		candidate.blendWeightQ16 = RAL_LIGHT_Q16_ONE;
	else candidate.blendWeightQ16 = (uint32_t)( (int64_t)bestEdge
		* RAL_LIGHT_Q16_ONE / placements[best].blendDistanceQ16 );
	candidate.ready = qtrue;
	if ( !Ral_IrradianceVolumeSelectionReceiptValid( &candidate ) ) return qfalse;
	*out = candidate; return qtrue;
}

qboolean Ral_IrradianceVolumeSelectionReceiptValid(
		const ralIrradianceVolumeSelectionReceipt_t *r ) {
	return r && r->schemaVersion == RAL_IRRADIANCE_SELECTION_RECEIPT_SCHEMA_VERSION
		&& r->volumeId && r->layoutHash && r->volumeIndex < RAL_IRRADIANCE_MAX_VOLUMES
		&& r->priority && r->blendWeightQ16 <= RAL_LIGHT_Q16_ONE && r->ready;
}

qboolean Ral_IrradianceRuntimeVolumeBuild(
		const ralIrradianceVolumePlacement_t *p,
		const ralLightingArtifactReceipt_t *a,
		ralIrradianceProbeVolume_t *out ) {
	ralIrradianceProbeVolume_t candidate;
	if ( !out || !Ral_IrradianceVolumePlacementValid( p )
			|| !Ral_LightingArtifactReceiptValid( a )
			|| a->kind != RAL_LIGHTING_ARTIFACT_IRRADIANCE_VOLUME
			|| a->encoding != RAL_IRRADIANCE_SH_L1_RGB16F
			|| a->artifactGeneration != p->sourceGeneration
			|| a->payloadCount != 2u
			|| a->payloads[0].role != RAL_LIGHTING_PAYLOAD_SH_COEFFICIENTS
			|| a->payloads[1].role != RAL_LIGHTING_PAYLOAD_PROBE_VALIDITY
			|| memcmp( a->dimensions, p->dimensions, sizeof( p->dimensions ) ) ) return qfalse;
	memset( &candidate, 0, sizeof( candidate ) );
	candidate.schemaVersion = RAL_IRRADIANCE_VOLUME_SCHEMA_VERSION;
	candidate.productGeneration = a->artifactGeneration;
	candidate.cacheKey = a->cacheKey;
	candidate.coefficientPayloadHash = a->payloads[0].payloadHash;
	candidate.validityPayloadHash = a->payloads[1].payloadHash;
	candidate.origin = p->origin; candidate.spacing = p->spacing;
	memcpy( candidate.dimensions, p->dimensions, sizeof( candidate.dimensions ) );
	candidate.encoding = RAL_IRRADIANCE_SH_L1_RGB16F;
	candidate.fallback = p->fallback; candidate.ready = qtrue;
	if ( !Ral_IrradianceProbeVolumeValid( &candidate ) ) return qfalse;
	*out = candidate; return qtrue;
}

static int32_t HalfToQ16( const uint8_t *bytes ) {
	uint32_t bits = (uint32_t)bytes[0] | ( (uint32_t)bytes[1] << 8u );
	uint32_t sign = bits >> 15u, exponent = ( bits >> 10u ) & 31u, mantissa = bits & 1023u;
	int64_t value;
	if ( exponent == 31u ) return sign ? INT32_MIN : INT32_MAX;
	if ( exponent == 0u ) value = (int64_t)mantissa >> 8u;
	else {
		value = (int64_t)( 1024u + mantissa );
		if ( exponent >= 9u ) value <<= exponent - 9u;
		else value >>= 9u - exponent;
	}
	if ( value > INT32_MAX ) value = INT32_MAX;
	return sign ? (int32_t)-value : (int32_t)value;
}

uint64_t Ral_IrradianceCoefficientHash(
		const int32_t coefficientsQ16[4][3] ) {
	uint64_t hash = FNV64_OFFSET;
	uint32_t coefficient, channel;
	if ( !coefficientsQ16 ) return 0u;
	for ( coefficient = 0u; coefficient < 4u; ++coefficient )
		for ( channel = 0u; channel < 3u; ++channel )
			hash = HashU32( hash, (uint32_t)coefficientsQ16[coefficient][channel] );
	return hash ? hash : 1u;
}

qboolean Ral_IrradianceEntitySample(
		const ralIrradianceVolumePlacement_t *p,
		const ralIrradianceProbeVolume_t *v,
		const ralIrradianceEntitySampleRequest_t *q,
		const void *coefficientMemory, uint64_t coefficientByteLength,
		const uint8_t *validity, uint32_t validityCount,
		ralIrradianceEntitySampleReceipt_t *out ) {
	const uint8_t *coefficients = (const uint8_t *)coefficientMemory;
	ralIrradianceEntitySampleReceipt_t r;
	ralIrradianceProbeQuery_t probeQuery;
	uint64_t probeCount, expectedBytes, contributorHash = FNV64_OFFSET;
	int64_t lengthSquared;
	uint32_t sample, coefficient, channel;
	if ( !out || !Ral_IrradianceVolumePlacementValid( p )
			|| !Ral_IrradianceProbeVolumeValid( v ) || !q
			|| q->schemaVersion != RAL_IRRADIANCE_ENTITY_SAMPLE_SCHEMA_VERSION
			|| !q->queryGeneration || !q->entityId || !coefficients || !validity ) return qfalse;
	probeCount = (uint64_t)v->dimensions[0] * v->dimensions[1] * v->dimensions[2];
	expectedBytes = probeCount * 24u;
	if ( coefficientByteLength != expectedBytes || validityCount != probeCount ) return qfalse;
	lengthSquared = (int64_t)q->normal.x * q->normal.x
		+ (int64_t)q->normal.y * q->normal.y + (int64_t)q->normal.z * q->normal.z;
	if ( lengthSquared < (int64_t)RAL_LIGHT_Q16_ONE * RAL_LIGHT_Q16_ONE / 2
			|| lengthSquared > (int64_t)RAL_LIGHT_Q16_ONE * RAL_LIGHT_Q16_ONE * 2 ) return qfalse;
	for ( channel = 0u; channel < 3u; ++channel )
		if ( q->fallbackIrradianceQ16[channel] < 0 ) return qfalse;
	memset( &probeQuery, 0, sizeof( probeQuery ) );
	probeQuery.schemaVersion = RAL_IRRADIANCE_QUERY_SCHEMA_VERSION;
	probeQuery.queryGeneration = q->queryGeneration;
	probeQuery.productGeneration = v->productGeneration;
	probeQuery.position = q->position;
	memset( &r, 0, sizeof( r ) );
	r.schemaVersion = RAL_IRRADIANCE_ENTITY_RECEIPT_SCHEMA_VERSION;
	r.queryGeneration = q->queryGeneration; r.entityId = q->entityId;
	r.volumeId = p->volumeId; r.layoutHash = Ral_IrradianceVolumeLayoutHash( p );
	if ( !r.layoutHash || !Ral_IrradianceProbeResolve( v, &probeQuery, validity,
		validityCount, &r.probes ) ) return qfalse;
	r.usedFallback = r.probes.usedFallback;
	if ( r.usedFallback ) {
		for ( channel = 0u; channel < 3u; ++channel ) {
			r.blendedCoefficientsQ16[0][channel] = q->fallbackIrradianceQ16[channel];
			r.diffuseIrradianceQ16[channel] = q->fallbackIrradianceQ16[channel];
		}
	} else {
		for ( sample = 0u; sample < r.probes.sampleCount; ++sample ) {
			const uint32_t probe = r.probes.probeIndices[sample];
			const uint32_t weight = r.probes.weightsQ16[sample];
			contributorHash = HashU32( HashU32( contributorHash, probe ), weight );
			for ( coefficient = 0u; coefficient < 4u; ++coefficient ) {
				for ( channel = 0u; channel < 3u; ++channel ) {
					const int32_t c = HalfToQ16( coefficients + (uint64_t)probe * 24u
						+ coefficient * 6u + channel * 2u );
					int64_t weighted = (int64_t)c * weight / RAL_LIGHT_Q16_ONE;
					int64_t total = (int64_t)r.blendedCoefficientsQ16[coefficient][channel]
						+ weighted;
					if ( total > INT32_MAX ) total = INT32_MAX;
					if ( total < INT32_MIN ) total = INT32_MIN;
					r.blendedCoefficientsQ16[coefficient][channel] = (int32_t)total;
				}
			}
		}
		for ( channel = 0u; channel < 3u; ++channel ) {
			int64_t value = r.blendedCoefficientsQ16[0][channel];
			value += (int64_t)r.blendedCoefficientsQ16[1][channel]
				* q->normal.x / RAL_LIGHT_Q16_ONE;
			value += (int64_t)r.blendedCoefficientsQ16[2][channel]
				* q->normal.y / RAL_LIGHT_Q16_ONE;
			value += (int64_t)r.blendedCoefficientsQ16[3][channel]
				* q->normal.z / RAL_LIGHT_Q16_ONE;
			if ( value > INT32_MAX ) value = INT32_MAX;
			r.diffuseIrradianceQ16[channel] = value > 0 ? (int32_t)value : 0;
		}
	}
	r.contributorHash = contributorHash ? contributorHash : 1u;
	r.coefficientHash = Ral_IrradianceCoefficientHash( r.blendedCoefficientsQ16 );
	r.ready = qtrue;
	if ( !Ral_IrradianceEntitySampleReceiptValid( &r ) ) return qfalse;
	*out = r; return qtrue;
}

qboolean Ral_IrradianceEntitySampleReceiptValid(
		const ralIrradianceEntitySampleReceipt_t *r ) {
	return r && r->schemaVersion == RAL_IRRADIANCE_ENTITY_RECEIPT_SCHEMA_VERSION
		&& r->queryGeneration && r->entityId && r->volumeId && r->layoutHash
		&& Ral_IrradianceProbeReceiptValid( &r->probes ) && r->contributorHash
		&& r->coefficientHash == Ral_IrradianceCoefficientHash(
			r->blendedCoefficientsQ16 )
		&& r->usedFallback == r->probes.usedFallback
		&& r->diffuseIrradianceQ16[0] >= 0 && r->diffuseIrradianceQ16[1] >= 0
		&& r->diffuseIrradianceQ16[2] >= 0 && r->ready;
}

qboolean Ral_IrradianceDebugReceiptValid(
		const ralIrradianceDebugReceipt_t *r ) {
	uint64_t probeCount;
	uint32_t i, weight = 0u;
	if ( !r || r->schemaVersion != RAL_IRRADIANCE_DEBUG_RECEIPT_SCHEMA_VERSION
			|| !r->queryGeneration || !r->entityId || !r->volumeId || !r->layoutHash
			|| r->dimensions[0] < 2u || r->dimensions[1] < 2u
			|| r->dimensions[2] < 2u || !r->volumeProbeCount
			|| r->selectedProbeCount > RAL_IRRADIANCE_MAX_CORNERS
			|| r->validProbeCount + r->invalidProbeCount != r->volumeProbeCount
			|| !r->contributorHash || !r->coefficientHash
			|| r->fallback < RAL_IRRADIANCE_FALLBACK_LIGHTGRID
			|| r->fallback > RAL_IRRADIANCE_FALLBACK_GLOBAL_AMBIENT
			|| r->usedFallback != ( r->selectedProbeCount == 0u )
			|| r->ready != qtrue ) return qfalse;
	probeCount = (uint64_t)r->dimensions[0] * r->dimensions[1]
		* r->dimensions[2];
	if ( probeCount != r->volumeProbeCount ) return qfalse;
	for ( i = 0u; i < r->selectedProbeCount; ++i ) {
		const ralIrradianceDebugProbe_t *probe = &r->selected[i];
		if ( probe->probeIndex >= r->volumeProbeCount || !probe->validity
				|| !probe->weightQ16 ) return qfalse;
		weight += probe->weightQ16;
		for ( uint32_t previous = 0u; previous < i; ++previous )
			if ( r->selected[previous].probeIndex == probe->probeIndex ) return qfalse;
	}
	if ( r->selectedProbeCount && weight != RAL_LIGHT_Q16_ONE ) return qfalse;
	for ( ; i < RAL_IRRADIANCE_MAX_CORNERS; ++i )
		if ( memcmp( &r->selected[i], &(ralIrradianceDebugProbe_t){ 0 },
				sizeof( r->selected[i] ) ) ) return qfalse;
	return qtrue;
}

qboolean Ral_IrradianceDebugBuild(
		const ralIrradianceVolumePlacement_t *p,
		const ralIrradianceProbeVolume_t *v,
		const ralIrradianceEntitySampleReceipt_t *sample,
		const void *coefficientMemory, uint64_t coefficientByteLength,
		const uint8_t *validity, uint32_t validityCount,
		ralIrradianceDebugReceipt_t *out ) {
	const uint8_t *coefficients = (const uint8_t *)coefficientMemory;
	ralIrradianceDebugReceipt_t r;
	uint64_t probeCount, expectedBytes;
	uint32_t i, coefficient, channel;
	if ( !out || !Ral_IrradianceVolumePlacementValid( p )
			|| !Ral_IrradianceProbeVolumeValid( v )
			|| !Ral_IrradianceEntitySampleReceiptValid( sample )
			|| !coefficients || !validity || sample->volumeId != p->volumeId
			|| sample->layoutHash != Ral_IrradianceVolumeLayoutHash( p )
			|| sample->probes.productGeneration != v->productGeneration
			|| memcmp( v->dimensions, p->dimensions, sizeof( p->dimensions ) ) )
		return qfalse;
	probeCount = (uint64_t)v->dimensions[0] * v->dimensions[1]
		* v->dimensions[2];
	expectedBytes = probeCount * 24u;
	if ( probeCount > UINT32_MAX || coefficientByteLength != expectedBytes
			|| validityCount != probeCount ) return qfalse;
	memset( &r, 0, sizeof( r ) );
	r.schemaVersion = RAL_IRRADIANCE_DEBUG_RECEIPT_SCHEMA_VERSION;
	r.queryGeneration = sample->queryGeneration; r.entityId = sample->entityId;
	r.volumeId = p->volumeId; r.layoutHash = sample->layoutHash;
	r.origin = p->origin; r.spacing = p->spacing;
	memcpy( r.dimensions, p->dimensions, sizeof( r.dimensions ) );
	r.volumeProbeCount = (uint32_t)probeCount;
	for ( i = 0u; i < r.volumeProbeCount; ++i ) {
		if ( validity[i] ) r.validProbeCount++;
		else r.invalidProbeCount++;
	}
	r.selectedProbeCount = sample->probes.sampleCount;
	for ( i = 0u; i < r.selectedProbeCount; ++i ) {
		ralIrradianceDebugProbe_t *debug = &r.selected[i];
		const uint32_t index = sample->probes.probeIndices[i];
		const uint32_t x = index % p->dimensions[0];
		const uint32_t yz = index / p->dimensions[0];
		const uint32_t y = yz % p->dimensions[1];
		const uint32_t z = yz / p->dimensions[1];
		int64_t position[3] = {
			(int64_t)p->origin.x + (int64_t)p->spacing.x * x,
			(int64_t)p->origin.y + (int64_t)p->spacing.y * y,
			(int64_t)p->origin.z + (int64_t)p->spacing.z * z
		};
		if ( index >= r.volumeProbeCount || !validity[index]
				|| position[0] < INT32_MIN || position[0] > INT32_MAX
				|| position[1] < INT32_MIN || position[1] > INT32_MAX
				|| position[2] < INT32_MIN || position[2] > INT32_MAX ) return qfalse;
		debug->probeIndex = index; debug->position.x = (int32_t)position[0];
		debug->position.y = (int32_t)position[1];
		debug->position.z = (int32_t)position[2];
		debug->validity = validity[index];
		debug->weightQ16 = sample->probes.weightsQ16[i];
		for ( coefficient = 0u; coefficient < 4u; ++coefficient )
			for ( channel = 0u; channel < 3u; ++channel )
				debug->coefficientsQ16[coefficient][channel] = HalfToQ16(
					coefficients + (uint64_t)index * 24u
					+ coefficient * 6u + channel * 2u );
	}
	r.contributorHash = sample->contributorHash;
	r.coefficientHash = sample->coefficientHash;
	r.fallback = sample->probes.fallback;
	r.usedFallback = sample->usedFallback; r.ready = qtrue;
	if ( !Ral_IrradianceDebugReceiptValid( &r ) ) return qfalse;
	*out = r; return qtrue;
}

qboolean Ral_IrradianceDebugReceiptExact(
		const ralIrradianceDebugReceipt_t *a,
		const ralIrradianceDebugReceipt_t *b ) {
	return Ral_IrradianceDebugReceiptValid( a )
		&& Ral_IrradianceDebugReceiptValid( b )
		&& !memcmp( a, b, sizeof( *a ) ) ? qtrue : qfalse;
}
