// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "ral_lighting_artifact.h"

#include <limits.h>
#include <string.h>

#define RAL_LIGHTING_ARTIFACT_MAGIC UINT32_C(0x54494c57)
#define FNV64_OFFSET UINT64_C(14695981039346656037)
#define FNV64_PRIME UINT64_C(1099511628211)

static uint64_t HashBytes( const void *memory, uint64_t byteLength ) {
	const unsigned char *bytes = (const unsigned char *)memory;
	uint64_t hash = FNV64_OFFSET, i;
	for ( i = 0u; i < byteLength; i++ ) hash = ( hash ^ bytes[i] ) * FNV64_PRIME;
	return hash ? hash : 1u;
}

static uint64_t HashU32( uint64_t hash, uint32_t value ) {
	unsigned i;
	for ( i = 0u; i < 4u; i++ ) hash = ( hash ^ (unsigned char)( value >> ( i * 8u ) ) ) * FNV64_PRIME;
	return hash;
}

static uint64_t HashU64( uint64_t hash, uint64_t value ) {
	unsigned i;
	for ( i = 0u; i < 8u; i++ ) hash = ( hash ^ (unsigned char)( value >> ( i * 8u ) ) ) * FNV64_PRIME;
	return hash;
}

static void Put32( unsigned char *bytes, uint32_t value ) {
	unsigned i; for ( i = 0u; i < 4u; i++ ) bytes[i] = (unsigned char)( value >> ( i * 8u ) );
}

static void Put64( unsigned char *bytes, uint64_t value ) {
	unsigned i; for ( i = 0u; i < 8u; i++ ) bytes[i] = (unsigned char)( value >> ( i * 8u ) );
}

static uint32_t Get32( const unsigned char *bytes ) {
	return (uint32_t)bytes[0] | ( (uint32_t)bytes[1] << 8u )
		| ( (uint32_t)bytes[2] << 16u ) | ( (uint32_t)bytes[3] << 24u );
}

static uint64_t Get64( const unsigned char *bytes ) {
	uint64_t value = 0u; unsigned i;
	for ( i = 0u; i < 8u; i++ ) value |= (uint64_t)bytes[i] << ( i * 8u );
	return value;
}

static qboolean RoleAllowed( ralLightingArtifactKind_t kind,
		ralLightingPayloadRole_t role ) {
	if ( kind == RAL_LIGHTING_ARTIFACT_DIRECTIONAL_LIGHTMAP )
		return role >= RAL_LIGHTING_PAYLOAD_RADIANCE
			&& role <= RAL_LIGHTING_PAYLOAD_STATIONARY_VISIBILITY;
	if ( kind == RAL_LIGHTING_ARTIFACT_IRRADIANCE_VOLUME )
		return role == RAL_LIGHTING_PAYLOAD_SH_COEFFICIENTS
			|| role == RAL_LIGHTING_PAYLOAD_PROBE_VALIDITY;
	return qfalse;
}

static qboolean ExpectedPayloadBytes( ralLightingArtifactKind_t kind, uint32_t encoding,
		const uint32_t dimensions[3], ralLightingPayloadRole_t role, uint64_t *outBytes ) {
	uint64_t count = dimensions[0]; uint32_t bytesPerElement = 0u;
	if ( !outBytes || !count || !dimensions[1] || !dimensions[2]
			|| count > RAL_LIGHTING_ARTIFACT_MAX_BYTES / dimensions[1] ) return qfalse;
	count *= dimensions[1];
	if ( count > RAL_LIGHTING_ARTIFACT_MAX_BYTES / dimensions[2] ) return qfalse;
	count *= dimensions[2];
	if ( kind == RAL_LIGHTING_ARTIFACT_DIRECTIONAL_LIGHTMAP ) {
		if ( role == RAL_LIGHTING_PAYLOAD_RADIANCE )
			bytesPerElement = encoding == RAL_STATIC_LIGHTING_ENCODING_RGBA16F_OCT16 ? 8u
				: encoding == RAL_STATIC_LIGHTING_ENCODING_LEGACY_SRGB8 ? 3u : 4u;
		else if ( role == RAL_LIGHTING_PAYLOAD_DIRECTION )
			bytesPerElement = encoding == RAL_STATIC_LIGHTING_ENCODING_RGBA16F_OCT16 ? 4u : 2u;
		else if ( role == RAL_LIGHTING_PAYLOAD_STATIONARY_VISIBILITY ) bytesPerElement = 1u;
	} else if ( kind == RAL_LIGHTING_ARTIFACT_IRRADIANCE_VOLUME ) {
		if ( role == RAL_LIGHTING_PAYLOAD_SH_COEFFICIENTS )
			bytesPerElement = encoding == RAL_IRRADIANCE_SH_L1_RGB16F ? 24u : 16u;
		else if ( role == RAL_LIGHTING_PAYLOAD_PROBE_VALIDITY ) bytesPerElement = 1u;
	}
	if ( !bytesPerElement || count > RAL_LIGHTING_ARTIFACT_MAX_BYTES / bytesPerElement ) return qfalse;
	*outBytes = count * bytesPerElement; return qtrue;
}

static qboolean DefinitionValid( const ralLightingArtifactDefinition_t *d,
		const ralLightingPayloadView_t *payloads ) {
	uint32_t i, required = 0u;
	if ( !d || !payloads || d->schemaVersion != RAL_LIGHTING_ARTIFACT_SCHEMA_VERSION
			|| !d->artifactGeneration || !d->cacheKey || !d->producerVersion
			|| d->kind < RAL_LIGHTING_ARTIFACT_DIRECTIONAL_LIGHTMAP
			|| d->kind > RAL_LIGHTING_ARTIFACT_IRRADIANCE_VOLUME
			|| !d->dimensions[0] || !d->dimensions[1] || !d->dimensions[2]
			|| !d->payloadCount || d->payloadCount > RAL_LIGHTING_ARTIFACT_MAX_PAYLOADS )
		return qfalse;
	if ( d->kind == RAL_LIGHTING_ARTIFACT_DIRECTIONAL_LIGHTMAP ) {
		if ( d->encoding < RAL_STATIC_LIGHTING_ENCODING_RGB9E5_OCT8
				|| d->encoding > RAL_STATIC_LIGHTING_ENCODING_LEGACY_SRGB8
				|| !( d->flags & RAL_LIGHTING_ARTIFACT_STATIC_DIFFUSE_INDIRECT_ONLY ) )
			return qfalse;
		required = ( 1u << RAL_LIGHTING_PAYLOAD_RADIANCE )
			| ( 1u << RAL_LIGHTING_PAYLOAD_DIRECTION );
		if ( d->flags & RAL_LIGHTING_ARTIFACT_HAS_STATIONARY_VISIBILITY )
			required |= 1u << RAL_LIGHTING_PAYLOAD_STATIONARY_VISIBILITY;
	} else {
		if ( d->encoding < RAL_IRRADIANCE_SH_L1_RGB16F
				|| d->encoding > RAL_IRRADIANCE_SH_L1_RGB9E5 || d->flags ) return qfalse;
		required = ( 1u << RAL_LIGHTING_PAYLOAD_SH_COEFFICIENTS )
			| ( 1u << RAL_LIGHTING_PAYLOAD_PROBE_VALIDITY );
	}
	for ( i = 0u; i < d->payloadCount; i++ ) {
		uint32_t bit; uint64_t expectedBytes;
		if ( !payloads[i].bytes || !payloads[i].byteLength
				|| !RoleAllowed( d->kind, payloads[i].role )
				|| !ExpectedPayloadBytes( d->kind, d->encoding, d->dimensions,
					payloads[i].role, &expectedBytes )
				|| payloads[i].byteLength != expectedBytes ) return qfalse;
		bit = 1u << payloads[i].role;
		if ( !( required & bit ) ) return qfalse;
		required &= ~bit;
	}
	return required == 0u;
}

static uint64_t ManifestHash( const ralLightingArtifactReceipt_t *r ) {
	uint64_t hash = HashU32( FNV64_OFFSET, RAL_LIGHTING_ARTIFACT_RECEIPT_SCHEMA_VERSION );
	uint32_t i;
	hash = HashU64( hash, r->artifactGeneration ); hash = HashU64( hash, r->cacheKey );
	hash = HashU64( hash, r->producerVersion ); hash = HashU32( hash, (uint32_t)r->kind );
	hash = HashU32( hash, r->encoding ); hash = HashU32( hash, r->flags );
	for ( i = 0u; i < 3u; i++ ) hash = HashU32( hash, r->dimensions[i] );
	hash = HashU32( hash, r->payloadCount );
	for ( i = 0u; i < r->payloadCount; i++ ) {
		hash = HashU32( hash, (uint32_t)r->payloads[i].role );
		hash = HashU64( hash, r->payloads[i].byteOffset );
		hash = HashU64( hash, r->payloads[i].byteLength );
		hash = HashU64( hash, r->payloads[i].payloadHash );
	}
	hash = HashU64( hash, r->byteLength ); return hash ? hash : 1u;
}

qboolean Ral_LightingArtifactReceiptValid( const ralLightingArtifactReceipt_t *r ) {
	uint32_t i, roles = 0u, required;
	if ( !r || r->schemaVersion != RAL_LIGHTING_ARTIFACT_RECEIPT_SCHEMA_VERSION
			|| !r->artifactGeneration || !r->cacheKey || !r->producerVersion
			|| r->kind < RAL_LIGHTING_ARTIFACT_DIRECTIONAL_LIGHTMAP
			|| r->kind > RAL_LIGHTING_ARTIFACT_IRRADIANCE_VOLUME
			|| !r->dimensions[0] || !r->dimensions[1] || !r->dimensions[2]
			|| !r->payloadCount || r->payloadCount > RAL_LIGHTING_ARTIFACT_MAX_PAYLOADS
			|| r->byteLength < RAL_LIGHTING_ARTIFACT_WIRE_HEADER_BYTES
			|| r->byteLength > RAL_LIGHTING_ARTIFACT_MAX_BYTES || !r->manifestHash
			|| r->ready != qtrue ) return qfalse;
	if ( r->kind == RAL_LIGHTING_ARTIFACT_DIRECTIONAL_LIGHTMAP ) {
		if ( r->encoding < RAL_STATIC_LIGHTING_ENCODING_RGB9E5_OCT8
				|| r->encoding > RAL_STATIC_LIGHTING_ENCODING_LEGACY_SRGB8
				|| ( r->flags & ~( RAL_LIGHTING_ARTIFACT_STATIC_DIFFUSE_INDIRECT_ONLY
					| RAL_LIGHTING_ARTIFACT_HAS_STATIONARY_VISIBILITY ) )
				|| !( r->flags & RAL_LIGHTING_ARTIFACT_STATIC_DIFFUSE_INDIRECT_ONLY ) )
			return qfalse;
		required = ( 1u << RAL_LIGHTING_PAYLOAD_RADIANCE )
			| ( 1u << RAL_LIGHTING_PAYLOAD_DIRECTION );
		if ( r->flags & RAL_LIGHTING_ARTIFACT_HAS_STATIONARY_VISIBILITY )
			required |= 1u << RAL_LIGHTING_PAYLOAD_STATIONARY_VISIBILITY;
	} else {
		if ( r->encoding < RAL_IRRADIANCE_SH_L1_RGB16F
				|| r->encoding > RAL_IRRADIANCE_SH_L1_RGB9E5 || r->flags ) return qfalse;
		required = ( 1u << RAL_LIGHTING_PAYLOAD_SH_COEFFICIENTS )
			| ( 1u << RAL_LIGHTING_PAYLOAD_PROBE_VALIDITY );
	}
	for ( i = 0u; i < r->payloadCount; i++ ) {
		const ralLightingArtifactPayload_t *p = &r->payloads[i]; uint32_t bit; uint64_t expectedBytes;
		if ( !RoleAllowed( r->kind, p->role ) || !p->byteLength || !p->payloadHash
				|| !ExpectedPayloadBytes( r->kind, r->encoding, r->dimensions,
					p->role, &expectedBytes ) || p->byteLength != expectedBytes
				|| p->byteOffset < RAL_LIGHTING_ARTIFACT_WIRE_HEADER_BYTES
				|| p->byteOffset > r->byteLength
				|| p->byteLength > r->byteLength - p->byteOffset ) return qfalse;
		bit = 1u << p->role; if ( roles & bit ) return qfalse; roles |= bit;
		if ( i && p->byteOffset != r->payloads[i - 1u].byteOffset
				+ r->payloads[i - 1u].byteLength ) return qfalse;
	}
	if ( roles != required ) return qfalse;
	for ( i = r->payloadCount; i < RAL_LIGHTING_ARTIFACT_MAX_PAYLOADS; i++ )
		if ( r->payloads[i].role || r->payloads[i].byteOffset
				|| r->payloads[i].byteLength || r->payloads[i].payloadHash ) return qfalse;
	if ( r->payloads[r->payloadCount - 1u].byteOffset
			+ r->payloads[r->payloadCount - 1u].byteLength != r->byteLength ) return qfalse;
	return r->manifestHash == ManifestHash( r );
}

qboolean Ral_LightingArtifactMeasure( const ralLightingArtifactDefinition_t *d,
		const ralLightingPayloadView_t *payloads, uint64_t *outByteLength ) {
	uint64_t length = RAL_LIGHTING_ARTIFACT_WIRE_HEADER_BYTES; uint32_t i;
	if ( !outByteLength || !DefinitionValid( d, payloads ) ) return qfalse;
	for ( i = 0u; i < d->payloadCount; i++ ) {
		if ( payloads[i].byteLength > RAL_LIGHTING_ARTIFACT_MAX_BYTES - length ) return qfalse;
		length += payloads[i].byteLength;
	}
	*outByteLength = length; return qtrue;
}

static qboolean BuildReceipt( const ralLightingArtifactDefinition_t *d,
		const ralLightingPayloadView_t *payloads, ralLightingArtifactReceipt_t *out ) {
	ralLightingArtifactReceipt_t r; uint64_t offset, length; uint32_t i;
	if ( !Ral_LightingArtifactMeasure( d, payloads, &length ) || !out ) return qfalse;
	memset( &r, 0, sizeof( r ) ); r.schemaVersion = RAL_LIGHTING_ARTIFACT_RECEIPT_SCHEMA_VERSION;
	r.artifactGeneration = d->artifactGeneration; r.cacheKey = d->cacheKey;
	r.producerVersion = d->producerVersion; r.kind = d->kind; r.encoding = d->encoding;
	r.flags = d->flags; memcpy( r.dimensions, d->dimensions, sizeof( r.dimensions ) );
	r.payloadCount = d->payloadCount; offset = RAL_LIGHTING_ARTIFACT_WIRE_HEADER_BYTES;
	for ( i = 0u; i < d->payloadCount; i++ ) {
		r.payloads[i].role = payloads[i].role; r.payloads[i].byteOffset = offset;
		r.payloads[i].byteLength = payloads[i].byteLength;
		r.payloads[i].payloadHash = HashBytes( payloads[i].bytes, payloads[i].byteLength );
		offset += payloads[i].byteLength;
	}
	r.byteLength = length; r.ready = qtrue; r.manifestHash = ManifestHash( &r );
	if ( !Ral_LightingArtifactReceiptValid( &r ) ) return qfalse; *out = r; return qtrue;
}

qboolean Ral_LightingArtifactWrite( const ralLightingArtifactDefinition_t *d,
		const ralLightingPayloadView_t *payloads, void *destination, uint64_t capacity,
		ralLightingArtifactReceipt_t *out ) {
	unsigned char *bytes = (unsigned char *)destination; ralLightingArtifactReceipt_t r;
	uint32_t i; uint64_t descriptor = 72u;
	if ( !bytes || !out || !BuildReceipt( d, payloads, &r ) || capacity < r.byteLength ) return qfalse;
	memset( bytes, 0, (size_t)RAL_LIGHTING_ARTIFACT_WIRE_HEADER_BYTES );
	Put32( bytes, RAL_LIGHTING_ARTIFACT_MAGIC ); Put32( bytes + 4u, RAL_LIGHTING_ARTIFACT_SCHEMA_VERSION );
	Put32( bytes + 8u, RAL_LIGHTING_ARTIFACT_WIRE_HEADER_BYTES ); Put32( bytes + 12u, (uint32_t)r.kind );
	Put64( bytes + 16u, r.artifactGeneration ); Put64( bytes + 24u, r.cacheKey );
	Put64( bytes + 32u, r.producerVersion ); Put64( bytes + 40u, r.manifestHash );
	Put32( bytes + 48u, r.encoding ); Put32( bytes + 52u, r.flags );
	for ( i = 0u; i < 3u; i++ ) Put32( bytes + 56u + i * 4u, r.dimensions[i] );
	Put32( bytes + 68u, r.payloadCount );
	for ( i = 0u; i < r.payloadCount; i++, descriptor += 32u ) {
		Put32( bytes + descriptor, (uint32_t)r.payloads[i].role );
		Put64( bytes + descriptor + 8u, r.payloads[i].byteOffset );
		Put64( bytes + descriptor + 16u, r.payloads[i].byteLength );
		Put64( bytes + descriptor + 24u, r.payloads[i].payloadHash );
		memcpy( bytes + r.payloads[i].byteOffset, payloads[i].bytes,
			(size_t)payloads[i].byteLength );
	}
	*out = r; return qtrue;
}

qboolean Ral_LightingArtifactRead( const void *memory, uint64_t byteLength,
		ralLightingArtifactReceipt_t *out,
		ralLightingPayloadView_t views[RAL_LIGHTING_ARTIFACT_MAX_PAYLOADS] ) {
	const unsigned char *bytes = (const unsigned char *)memory; ralLightingArtifactReceipt_t r;
	uint32_t i; uint64_t descriptor = 72u;
	if ( !bytes || !out || !views || byteLength < RAL_LIGHTING_ARTIFACT_WIRE_HEADER_BYTES
			|| byteLength > RAL_LIGHTING_ARTIFACT_MAX_BYTES
			|| Get32( bytes ) != RAL_LIGHTING_ARTIFACT_MAGIC
			|| Get32( bytes + 4u ) != RAL_LIGHTING_ARTIFACT_SCHEMA_VERSION
			|| Get32( bytes + 8u ) != RAL_LIGHTING_ARTIFACT_WIRE_HEADER_BYTES ) return qfalse;
	memset( &r, 0, sizeof( r ) ); memset( views, 0,
		sizeof( ralLightingPayloadView_t ) * RAL_LIGHTING_ARTIFACT_MAX_PAYLOADS );
	r.schemaVersion = RAL_LIGHTING_ARTIFACT_RECEIPT_SCHEMA_VERSION; r.kind = (ralLightingArtifactKind_t)Get32( bytes + 12u );
	r.artifactGeneration = Get64( bytes + 16u ); r.cacheKey = Get64( bytes + 24u );
	r.producerVersion = Get64( bytes + 32u ); r.manifestHash = Get64( bytes + 40u );
	r.encoding = Get32( bytes + 48u ); r.flags = Get32( bytes + 52u );
	for ( i = 0u; i < 3u; i++ ) r.dimensions[i] = Get32( bytes + 56u + i * 4u );
	r.payloadCount = Get32( bytes + 68u ); r.byteLength = byteLength; r.ready = qtrue;
	if ( !r.payloadCount || r.payloadCount > RAL_LIGHTING_ARTIFACT_MAX_PAYLOADS ) return qfalse;
	for ( i = 0u; i < r.payloadCount; i++, descriptor += 32u ) {
		if ( Get32( bytes + descriptor + 4u ) ) return qfalse;
		r.payloads[i].role = (ralLightingPayloadRole_t)Get32( bytes + descriptor );
		r.payloads[i].byteOffset = Get64( bytes + descriptor + 8u );
		r.payloads[i].byteLength = Get64( bytes + descriptor + 16u );
		r.payloads[i].payloadHash = Get64( bytes + descriptor + 24u );
	}
	for ( ; descriptor < RAL_LIGHTING_ARTIFACT_WIRE_HEADER_BYTES; descriptor++ )
		if ( bytes[descriptor] ) return qfalse;
	if ( !Ral_LightingArtifactReceiptValid( &r ) ) return qfalse;
	for ( i = 0u; i < r.payloadCount; i++ ) {
		views[i].role = r.payloads[i].role; views[i].bytes = bytes + r.payloads[i].byteOffset;
		views[i].byteLength = r.payloads[i].byteLength;
		if ( HashBytes( views[i].bytes, views[i].byteLength ) != r.payloads[i].payloadHash ) return qfalse;
	}
	*out = r; return qtrue;
}

qboolean Ral_LightingArtifactReceiptExact( const ralLightingArtifactReceipt_t *a,
		const ralLightingArtifactReceipt_t *b ) {
	return Ral_LightingArtifactReceiptValid( a ) && Ral_LightingArtifactReceiptValid( b )
		&& !memcmp( a, b, sizeof( *a ) );
}
