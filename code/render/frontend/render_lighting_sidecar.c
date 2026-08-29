// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "render_lighting_sidecar.h"

#include <stdio.h>
#include <string.h>

#define RENDER_LIGHTING_SIDECAR_MAX_BYTES ( UINT64_C( 64 ) * 1024u * 1024u )
#define RENDER_IRRADIANCE_SIDECAR_MAX_BYTES ( UINT64_C( 256 ) * 1024u * 1024u )
#define FNV_OFFSET UINT64_C( 14695981039346656037 )
#define FNV_PRIME UINT64_C( 1099511628211 )

static const uint8_t s_irradianceMagic[8] = { 'W', 'P', 'R', 'O', 'B', 'E', '1', 0 };

static uint32_t Get32( const uint8_t *p ) {
	return (uint32_t)p[0] | ( (uint32_t)p[1] << 8u ) | ( (uint32_t)p[2] << 16u ) |
		( (uint32_t)p[3] << 24u );
}

static uint64_t Get64( const uint8_t *p ) {
	return (uint64_t)Get32( p ) | ( (uint64_t)Get32( p + 4 ) << 32u );
}

static void Put32( uint8_t *p, uint32_t v ) {
	p[0] = (uint8_t)v; p[1] = (uint8_t)( v >> 8u );
	p[2] = (uint8_t)( v >> 16u ); p[3] = (uint8_t)( v >> 24u );
}

static void Put64( uint8_t *p, uint64_t v ) {
	Put32( p, (uint32_t)v ); Put32( p + 4, (uint32_t)( v >> 32u ) );
}

static uint64_t HashBytes( const uint8_t *bytes, uint64_t byteLength ) {
	uint64_t hash = FNV_OFFSET, index;
	for ( index = 0u; index < byteLength; ++index ) hash = ( hash ^ bytes[index] ) * FNV_PRIME;
	return hash ? hash : 1u;
}

static void PutVec3( uint8_t *p, const ralLightVec3Q16_t *v ) {
	Put32( p, (uint32_t)v->x ); Put32( p + 4, (uint32_t)v->y ); Put32( p + 8, (uint32_t)v->z );
}

static void GetVec3( const uint8_t *p, ralLightVec3Q16_t *v ) {
	v->x = (int32_t)Get32( p ); v->y = (int32_t)Get32( p + 4 ); v->z = (int32_t)Get32( p + 8 );
}

static qboolean SidecarPath( const char *worldName, char path[MAX_QPATH] ) {
	const char *slash, *dot;
	size_t length, stemLength;
	if ( !worldName || !worldName[0] || !path ) return qfalse;
	length = strlen( worldName );
	if ( length >= MAX_QPATH ) return qfalse;
	slash = strrchr( worldName, '/' );
	dot = strrchr( worldName, '.' );
	stemLength = dot && ( !slash || dot > slash ) ? (size_t)( dot - worldName ) : length;
	if ( stemLength + sizeof( ".wlight" ) > MAX_QPATH ) return qfalse;
	memcpy( path, worldName, stemLength );
	memcpy( path + stemLength, ".wlight", sizeof( ".wlight" ) );
	return qtrue;
}

static qboolean IrradianceSidecarPath( const char *worldName, char path[MAX_QPATH] ) {
	const char *slash, *dot;
	size_t length, stemLength;
	if ( !worldName || !worldName[0] || !path ) return qfalse;
	length = strlen( worldName );
	if ( length >= MAX_QPATH ) return qfalse;
	slash = strrchr( worldName, '/' ); dot = strrchr( worldName, '.' );
	stemLength = dot && ( !slash || dot > slash ) ? (size_t)( dot - worldName ) : length;
	if ( stemLength + sizeof( ".wprobe" ) > MAX_QPATH ) return qfalse;
	memcpy( path, worldName, stemLength );
	memcpy( path + stemLength, ".wprobe", sizeof( ".wprobe" ) );
	return qtrue;
}

qboolean RenderLightingSidecar_ReceiptValid(
		const renderLightingSidecarReceipt_t *receipt ) {
	if ( !receipt || receipt->schemaVersion != RENDER_LIGHTING_SIDECAR_SCHEMA_VERSION
			|| !receipt->ready || !receipt->path[0] ) return qfalse;
	if ( receipt->status == RENDER_LIGHTING_SIDECAR_MISSING_COMPATIBILITY )
		return receipt->byteLength == 0u && receipt->artifactGeneration == 0u
			&& receipt->cacheKey == 0u;
	return receipt->status == RENDER_LIGHTING_SIDECAR_MODERN_LOADED
		&& receipt->byteLength > 0u && receipt->artifactGeneration > 0u
		&& receipt->cacheKey > 0u;
}

qboolean RenderLightingSidecar_LoadDirectional(
		renderSubmissionState_t *submission, const refimport_t *imports,
		const char *worldName, renderLightingSidecarReceipt_t *outReceipt ) {
	renderLightingSidecarReceipt_t receipt;
	const renderDirectionalLightingRecord_t *record;
	uint64_t digest;
	void *bytes = NULL;
	int byteLength;
	if ( !submission || !imports || !outReceipt ) return qfalse;
	memset( &receipt, 0, sizeof( receipt ) );
	receipt.schemaVersion = RENDER_LIGHTING_SIDECAR_SCHEMA_VERSION;
	if ( !SidecarPath( worldName, receipt.path ) ) return qfalse;
	if ( !imports->FS_ReadFile || !imports->FS_FreeFile ) {
		receipt.status = RENDER_LIGHTING_SIDECAR_MISSING_COMPATIBILITY;
		receipt.ready = qtrue;
		*outReceipt = receipt;
		return qtrue;
	}
	byteLength = imports->FS_ReadFile( receipt.path, &bytes );
	if ( byteLength <= 0 || !bytes ) {
		if ( bytes ) imports->FS_FreeFile( bytes );
		if ( byteLength > 0 || bytes ) return qfalse;
		receipt.status = RENDER_LIGHTING_SIDECAR_MISSING_COMPATIBILITY;
		receipt.ready = qtrue;
		*outReceipt = receipt;
		return qtrue;
	}
	if ( (uint64_t)byteLength > RENDER_LIGHTING_SIDECAR_MAX_BYTES
			|| !RenderSubmission_RegisterDirectionalLighting( submission,
				bytes, (uint64_t)byteLength ) ) {
		imports->FS_FreeFile( bytes );
		return qfalse;
	}
	imports->FS_FreeFile( bytes );
	if ( !RenderSubmission_DirectionalLightingSnapshot( submission, &record, &digest )
			|| !record || !digest ) return qfalse;
	receipt.status = RENDER_LIGHTING_SIDECAR_MODERN_LOADED;
	receipt.byteLength = (uint64_t)byteLength;
	receipt.artifactGeneration = record->artifact.artifactGeneration;
	receipt.cacheKey = record->artifact.cacheKey;
	receipt.ready = qtrue;
	if ( !RenderLightingSidecar_ReceiptValid( &receipt ) ) return qfalse;
	*outReceipt = receipt;
	return qtrue;
}

qboolean RenderLightingSidecar_PackIrradiance(
		const renderIrradianceVolumeSource_t *sources, uint32_t sourceCount,
		void *outBytes, uint64_t outputCapacity, uint64_t *outByteLength,
		uint64_t *outManifestHash ) {
	uint8_t *bytes = (uint8_t *)outBytes;
	uint64_t cursor, total, manifest;
	uint32_t index;
	if ( !sources || !sourceCount || sourceCount > RENDER_SUBMISSION_MAX_IRRADIANCE_VOLUMES ||
		 !bytes || !outByteLength || !outManifestHash ) return qfalse;
	total = RENDER_IRRADIANCE_SIDECAR_HEADER_BYTES +
		(uint64_t)sourceCount * RENDER_IRRADIANCE_SIDECAR_ENTRY_BYTES;
	for ( index = 0u; index < sourceCount; ++index ) {
		ralLightingArtifactReceipt_t artifact;
		ralLightingPayloadView_t payloads[RAL_LIGHTING_ARTIFACT_MAX_PAYLOADS];
		uint32_t prior;
		if ( !Ral_IrradianceVolumePlacementValid( &sources[index].placement ) ||
			 !sources[index].artifactBytes || !sources[index].artifactByteLength ||
			 !Ral_LightingArtifactRead( sources[index].artifactBytes, sources[index].artifactByteLength,
				&artifact, payloads ) || artifact.kind != RAL_LIGHTING_ARTIFACT_IRRADIANCE_VOLUME ||
			 artifact.artifactGeneration != sources[index].placement.sourceGeneration ||
			 sources[index].artifactByteLength > UINT64_MAX - total ) return qfalse;
		for ( prior = 0u; prior < index; ++prior )
			if ( sources[prior].placement.volumeId == sources[index].placement.volumeId ) return qfalse;
		total += sources[index].artifactByteLength;
	}
	if ( total > outputCapacity || total > RENDER_IRRADIANCE_SIDECAR_MAX_BYTES ) return qfalse;
	memset( bytes, 0, (size_t)total );
	memcpy( bytes, s_irradianceMagic, sizeof( s_irradianceMagic ) );
	Put32( bytes + 8, RENDER_IRRADIANCE_SIDECAR_SCHEMA_VERSION );
	Put32( bytes + 12, RENDER_IRRADIANCE_SIDECAR_HEADER_BYTES );
	Put32( bytes + 16, RENDER_IRRADIANCE_SIDECAR_ENTRY_BYTES );
	Put32( bytes + 20, sourceCount ); Put64( bytes + 24, total );
	cursor = RENDER_IRRADIANCE_SIDECAR_HEADER_BYTES +
		(uint64_t)sourceCount * RENDER_IRRADIANCE_SIDECAR_ENTRY_BYTES;
	for ( index = 0u; index < sourceCount; ++index ) {
		const ralIrradianceVolumePlacement_t *p = &sources[index].placement;
		ralLightingArtifactReceipt_t artifact;
		ralLightingPayloadView_t payloads[RAL_LIGHTING_ARTIFACT_MAX_PAYLOADS];
		uint8_t *entry = bytes + RENDER_IRRADIANCE_SIDECAR_HEADER_BYTES +
			(uint64_t)index * RENDER_IRRADIANCE_SIDECAR_ENTRY_BYTES;
		if ( !Ral_LightingArtifactRead( sources[index].artifactBytes, sources[index].artifactByteLength,
			&artifact, payloads ) ) return qfalse;
		Put64( entry, p->volumeId ); Put64( entry + 8, p->sourceGeneration );
		Put64( entry + 16, p->provenanceHash ); PutVec3( entry + 24, &p->origin );
		PutVec3( entry + 36, &p->spacing ); PutVec3( entry + 48, &p->boundsMin );
		PutVec3( entry + 60, &p->boundsMax ); Put32( entry + 72, p->dimensions[0] );
		Put32( entry + 76, p->dimensions[1] ); Put32( entry + 80, p->dimensions[2] );
		Put32( entry + 84, p->priority ); Put32( entry + 88, p->blendDistanceQ16 );
		Put32( entry + 92, (uint32_t)p->fallback ); Put32( entry + 96, p->ready ? 1u : 0u );
		Put64( entry + 104, cursor ); Put64( entry + 112, sources[index].artifactByteLength );
		Put64( entry + 120, artifact.manifestHash );
		memcpy( bytes + cursor, sources[index].artifactBytes, (size_t)sources[index].artifactByteLength );
		cursor += sources[index].artifactByteLength;
	}
	manifest = HashBytes( bytes, total ); Put64( bytes + 32, manifest );
	*outByteLength = total; *outManifestHash = manifest;
	return qtrue;
}

qboolean RenderLightingSidecar_IrradianceReceiptValid(
		const renderIrradianceSidecarReceipt_t *receipt ) {
	if ( !receipt || receipt->schemaVersion != RENDER_IRRADIANCE_SIDECAR_SCHEMA_VERSION ||
		 !receipt->ready || !receipt->path[0] ) return qfalse;
	if ( receipt->status == RENDER_IRRADIANCE_SIDECAR_MISSING_COMPATIBILITY )
		return !receipt->byteLength && !receipt->manifestHash && !receipt->volumeCount;
	return receipt->status == RENDER_IRRADIANCE_SIDECAR_MODERN_LOADED && receipt->byteLength &&
		receipt->manifestHash && receipt->volumeDigest && receipt->volumeCount;
}

qboolean RenderLightingSidecar_LoadIrradiance(
		renderSubmissionState_t *submission, const refimport_t *imports,
		const char *worldName, renderIrradianceSidecarReceipt_t *outReceipt ) {
	renderIrradianceSidecarReceipt_t receipt;
	renderIrradianceVolumeSource_t sources[RENDER_SUBMISSION_MAX_IRRADIANCE_VOLUMES];
	uint8_t *bytes = NULL;
	uint64_t total, storedManifest, actualManifest, cursor;
	uint32_t count, index;
	int byteLength;
	if ( !submission || !imports || !outReceipt ) return qfalse;
	memset( &receipt, 0, sizeof( receipt ) ); memset( sources, 0, sizeof( sources ) );
	receipt.schemaVersion = RENDER_IRRADIANCE_SIDECAR_SCHEMA_VERSION;
	if ( !IrradianceSidecarPath( worldName, receipt.path ) ) return qfalse;
	if ( !imports->FS_ReadFile || !imports->FS_FreeFile ) goto missing;
	byteLength = imports->FS_ReadFile( receipt.path, (void **)&bytes );
	if ( byteLength <= 0 || !bytes ) {
		if ( bytes ) imports->FS_FreeFile( bytes );
		if ( byteLength > 0 || bytes ) return qfalse;
		goto missing;
	}
	if ( (uint64_t)byteLength < RENDER_IRRADIANCE_SIDECAR_HEADER_BYTES ||
		 (uint64_t)byteLength > RENDER_IRRADIANCE_SIDECAR_MAX_BYTES ||
		 memcmp( bytes, s_irradianceMagic, sizeof( s_irradianceMagic ) ) ||
		 Get32( bytes + 8 ) != RENDER_IRRADIANCE_SIDECAR_SCHEMA_VERSION ||
		 Get32( bytes + 12 ) != RENDER_IRRADIANCE_SIDECAR_HEADER_BYTES ||
		 Get32( bytes + 16 ) != RENDER_IRRADIANCE_SIDECAR_ENTRY_BYTES || Get64( bytes + 40 ) ) goto reject;
	count = Get32( bytes + 20 ); total = Get64( bytes + 24 ); storedManifest = Get64( bytes + 32 );
	if ( !count || count > RENDER_SUBMISSION_MAX_IRRADIANCE_VOLUMES || total != (uint64_t)byteLength ||
		 RENDER_IRRADIANCE_SIDECAR_HEADER_BYTES + (uint64_t)count * RENDER_IRRADIANCE_SIDECAR_ENTRY_BYTES > total )
		goto reject;
	Put64( bytes + 32, 0u ); actualManifest = HashBytes( bytes, total ); Put64( bytes + 32, storedManifest );
	if ( !storedManifest || actualManifest != storedManifest ) goto reject;
	cursor = RENDER_IRRADIANCE_SIDECAR_HEADER_BYTES + (uint64_t)count * RENDER_IRRADIANCE_SIDECAR_ENTRY_BYTES;
	for ( index = 0u; index < count; ++index ) {
		uint8_t *entry = bytes + RENDER_IRRADIANCE_SIDECAR_HEADER_BYTES +
			(uint64_t)index * RENDER_IRRADIANCE_SIDECAR_ENTRY_BYTES;
		ralLightingArtifactReceipt_t artifact;
		ralLightingPayloadView_t payloads[RAL_LIGHTING_ARTIFACT_MAX_PAYLOADS];
		ralIrradianceVolumePlacement_t *p = &sources[index].placement;
		uint64_t artifactOffset = Get64( entry + 104 ), artifactLength = Get64( entry + 112 );
		if ( Get32( entry + 100 ) || artifactOffset != cursor || !artifactLength || artifactLength > total - cursor )
			goto reject;
		memset( p, 0, sizeof( *p ) ); p->schemaVersion = RAL_IRRADIANCE_PLACEMENT_SCHEMA_VERSION;
		p->volumeId = Get64( entry ); p->sourceGeneration = Get64( entry + 8 );
		p->provenanceHash = Get64( entry + 16 ); GetVec3( entry + 24, &p->origin );
		GetVec3( entry + 36, &p->spacing ); GetVec3( entry + 48, &p->boundsMin );
		GetVec3( entry + 60, &p->boundsMax ); p->dimensions[0] = Get32( entry + 72 );
		p->dimensions[1] = Get32( entry + 76 ); p->dimensions[2] = Get32( entry + 80 );
		p->priority = Get32( entry + 84 ); p->blendDistanceQ16 = Get32( entry + 88 );
		p->fallback = (ralIrradianceFallback_t)Get32( entry + 92 ); p->ready = Get32( entry + 96 ) == 1u;
		sources[index].artifactBytes = bytes + artifactOffset; sources[index].artifactByteLength = artifactLength;
		if ( !Ral_IrradianceVolumePlacementValid( p ) ||
			 !Ral_LightingArtifactRead( sources[index].artifactBytes, artifactLength, &artifact, payloads ) ||
			 artifact.kind != RAL_LIGHTING_ARTIFACT_IRRADIANCE_VOLUME ||
			 artifact.manifestHash != Get64( entry + 120 ) ) goto reject;
		cursor += artifactLength;
	}
	if ( cursor != total || !RenderSubmission_ReplaceIrradianceVolumes( submission, sources, count ) ) goto reject;
	imports->FS_FreeFile( bytes );
	receipt.status = RENDER_IRRADIANCE_SIDECAR_MODERN_LOADED; receipt.byteLength = total;
	receipt.manifestHash = storedManifest; receipt.volumeCount = count;
	{
		const renderIrradianceVolumeRecord_t *volumes; uint32_t volumeCount;
		if ( !RenderSubmission_IrradianceVolumeSnapshot( submission, &volumes, &volumeCount,
			&receipt.volumeDigest ) || volumeCount != count ) return qfalse;
	}
	receipt.ready = qtrue;
	if ( !RenderLightingSidecar_IrradianceReceiptValid( &receipt ) ) return qfalse;
	*outReceipt = receipt; return qtrue;
reject:
	imports->FS_FreeFile( bytes ); return qfalse;
missing:
	receipt.status = RENDER_IRRADIANCE_SIDECAR_MISSING_COMPATIBILITY; receipt.ready = qtrue;
	*outReceipt = receipt; return qtrue;
}
