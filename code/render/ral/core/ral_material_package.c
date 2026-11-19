// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "ral_material_package.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint64_t HashByte( uint64_t hash, unsigned char value ) {
	return ( hash ^ value ) * 1099511628211ull;
}

static uint64_t HashU64( uint64_t hash, uint64_t value ) {
	for ( uint32_t i = 0u; i < 8u; ++i )
		hash = HashByte( hash, (unsigned char)( value >> ( i * 8u ) ) );
	return hash;
}

static qboolean MapNameValid( const char *mapName ) {
	size_t length;
	if ( !mapName || !mapName[0] || mapName[0] == '/'
			|| strchr( mapName, '\\' ) || strstr( mapName, ".." ) ) return qfalse;
	for ( length = 0u; length < MAX_QPATH; ++length )
		if ( mapName[length] == '\0' ) break;
	return length > 0u && length < MAX_QPATH ? qtrue : qfalse;
}

static uint64_t MapNameHash( const char *mapName ) {
	uint64_t hash = 1469598103934665603ull;
	if ( !MapNameValid( mapName ) ) return 0u;
	for ( size_t i = 0u; i <= strlen( mapName ); ++i )
		hash = HashByte( hash, (unsigned char)mapName[i] );
	return hash;
}

static uint64_t ProjectReferenceHash(
		const ralMaterialProjectReferenceReceipt_t *receipt ) {
	uint64_t hash = 1469598103934665603ull;
	hash = HashU64( hash, receipt->schemaVersion );
	hash = HashU64( hash, receipt->mapChecksum );
	hash = HashU64( hash, receipt->materialCount );
	hash = HashU64( hash, receipt->generation );
	hash = HashU64( hash, receipt->manifestArtifactHash );
	hash = HashU64( hash, receipt->corpusHash );
	hash = HashU64( hash, receipt->mapNameHash );
	for ( size_t i = 0u; i <= strlen( receipt->mapName ); ++i )
		hash = HashByte( hash, (unsigned char)receipt->mapName[i] );
	return hash;
}

static qboolean EntryValid( const ralMaterialPackageEntry_t *entry ) {
	size_t length;
	if ( !entry || !entry->semanticName[0] || entry->reserved != 0u
			|| !entry->semanticHash || !entry->dependencyHash
			|| !entry->sourceArtifactHash || !entry->cookedArtifactHash
			|| ( entry->provenance != RAL_MATERIAL_PROVENANCE_Q3_SHADER
				&& entry->provenance != RAL_MATERIAL_PROVENANCE_WIRED_LUA ) )
		return qfalse;
	for ( length = 0u; length < sizeof( entry->semanticName ); ++length )
		if ( entry->semanticName[length] == '\0' ) break;
	return length > 0u && length < sizeof( entry->semanticName ) ? qtrue : qfalse;
}

static uint64_t ReceiptHash( const ralMaterialPackageReceipt_t *receipt ) {
	uint64_t hash = 1469598103934665603ull;
	hash = HashU64( hash, receipt->schemaVersion );
	hash = HashU64( hash, receipt->generation );
	hash = HashU64( hash, receipt->entryCount );
	for ( uint32_t i = 0u; i < receipt->entryCount; ++i ) {
		const ralMaterialPackageEntry_t *entry = &receipt->entries[i];
		for ( size_t c = 0u; c <= strlen( entry->semanticName ); ++c )
			hash = HashByte( hash, (unsigned char)entry->semanticName[c] );
		hash = HashU64( hash, entry->semanticHash );
		hash = HashU64( hash, entry->dependencyHash );
		hash = HashU64( hash, entry->sourceArtifactHash );
		hash = HashU64( hash, entry->cookedArtifactHash );
		hash = HashU64( hash, (uint64_t)entry->provenance );
	}
	return hash;
}

qboolean Ral_MaterialPackageEntryFromCook(
		const ralMaterialCookReceipt_t *cooked,
		ralMaterialPackageEntry_t *outEntry ) {
	ralMaterialPackageEntry_t candidate;
	if ( !cooked || !outEntry || !Ral_MaterialCookReceiptValid( cooked ) )
		return qfalse;
	memset( &candidate, 0, sizeof( candidate ) );
	(void)snprintf( candidate.semanticName, sizeof( candidate.semanticName ),
		"%s", cooked->source.source.semanticName );
	candidate.semanticHash = cooked->source.semanticHash;
	candidate.dependencyHash = cooked->source.dependencyHash;
	candidate.sourceArtifactHash = cooked->source.artifactHash;
	candidate.cookedArtifactHash = cooked->artifactHash;
	candidate.provenance = cooked->source.source.provenance;
	if ( !EntryValid( &candidate ) ) return qfalse;
	*outEntry = candidate;
	return qtrue;
}

qboolean Ral_MaterialPackageSeal( uint64_t generation,
		const ralMaterialPackageEntry_t *entries, uint32_t entryCount,
		ralMaterialPackageReceipt_t *outReceipt ) {
	ralMaterialPackageReceipt_t *candidate;
	qboolean success = qfalse;
	if ( !generation || !entries || !entryCount || !outReceipt
			|| entryCount > RAL_MATERIAL_PACKAGE_MAX_ENTRIES ) return qfalse;
	candidate = calloc( 1u, sizeof( *candidate ) );
	if ( !candidate ) return qfalse;
	candidate->schemaVersion = RAL_MATERIAL_PACKAGE_SCHEMA_VERSION;
	candidate->generation = generation;
	candidate->entryCount = entryCount;
	for ( uint32_t i = 0u; i < entryCount; ++i ) {
		uint32_t at = i;
		if ( !EntryValid( &entries[i] ) ) goto done;
		candidate->entries[i] = entries[i];
		while ( at > 0u && strcmp( candidate->entries[at - 1u].semanticName,
				candidate->entries[at].semanticName ) > 0 ) {
			ralMaterialPackageEntry_t swap = candidate->entries[at - 1u];
			candidate->entries[at - 1u] = candidate->entries[at];
			candidate->entries[at] = swap;
			--at;
		}
	}
	for ( uint32_t i = 1u; i < entryCount; ++i )
		if ( !strcmp( candidate->entries[i - 1u].semanticName,
				candidate->entries[i].semanticName ) ) goto done;
	candidate->artifactHash = ReceiptHash( candidate );
	if ( !candidate->artifactHash ) goto done;
	candidate->ready = qtrue;
	*outReceipt = *candidate;
	success = qtrue;
done:
	free( candidate );
	return success;
}

qboolean Ral_MaterialPackageReceiptValid(
		const ralMaterialPackageReceipt_t *receipt ) {
	if ( !receipt || receipt->schemaVersion != RAL_MATERIAL_PACKAGE_SCHEMA_VERSION
			|| !receipt->generation || !receipt->entryCount
			|| receipt->entryCount > RAL_MATERIAL_PACKAGE_MAX_ENTRIES
			|| !receipt->artifactHash || receipt->ready != qtrue ) return qfalse;
	for ( uint32_t i = 0u; i < receipt->entryCount; ++i ) {
		if ( !EntryValid( &receipt->entries[i] ) ) return qfalse;
		if ( i && strcmp( receipt->entries[i - 1u].semanticName,
				receipt->entries[i].semanticName ) >= 0 ) return qfalse;
	}
	return ReceiptHash( receipt ) == receipt->artifactHash ? qtrue : qfalse;
}

qboolean Ral_MaterialPackageReceiptExact(
		const ralMaterialPackageReceipt_t *a,
		const ralMaterialPackageReceipt_t *b ) {
	return Ral_MaterialPackageReceiptValid( a )
		&& Ral_MaterialPackageReceiptValid( b )
		&& !memcmp( a, b, sizeof( *a ) ) ? qtrue : qfalse;
}

qboolean Ral_MaterialPackageLookup(
		const ralMaterialPackageReceipt_t *receipt, const char *semanticName,
		ralMaterialPackageEntry_t *outEntry ) {
	uint32_t lo = 0u, hi;
	if ( !Ral_MaterialPackageReceiptValid( receipt ) || !semanticName
			|| !semanticName[0] || !outEntry ) return qfalse;
	hi = receipt->entryCount;
	while ( lo < hi ) {
		const uint32_t mid = lo + ( hi - lo ) / 2u;
		const int order = strcmp( semanticName,
			receipt->entries[mid].semanticName );
		if ( order < 0 ) hi = mid;
		else if ( order > 0 ) lo = mid + 1u;
		else { *outEntry = receipt->entries[mid]; return qtrue; }
	}
	return qfalse;
}

qboolean Ral_MaterialPackageCacheKeyText( uint64_t artifactHash,
		char *outText, uint32_t capacity ) {
	int length;
	if ( !artifactHash || !outText || !capacity ) return qfalse;
	length = snprintf( outText, capacity, "wrmat-package-v1-%016" PRIx64,
		artifactHash );
	return length == 33 && (uint32_t)length < capacity ? qtrue : qfalse;
}

qboolean Ral_MaterialPackageCacheStore(
		const ralMaterialPackageCacheOps_t *ops, void *context,
		const ralMaterialPackageReceipt_t *receipt ) {
	char key[RAL_MATERIAL_PACKAGE_KEY_TEXT_CAPACITY];
	return ops && ops->writeAtomic && Ral_MaterialPackageReceiptValid( receipt )
		&& Ral_MaterialPackageCacheKeyText( receipt->artifactHash, key,
			sizeof( key ) )
		&& ops->writeAtomic( context, receipt->artifactHash, key, receipt,
			sizeof( *receipt ) );
}

qboolean Ral_MaterialPackageCacheLoad(
		const ralMaterialPackageCacheOps_t *ops, void *context,
		uint64_t expectedArtifactHash, ralMaterialPackageReceipt_t *outReceipt ) {
	ralMaterialPackageReceipt_t *candidate;
	char key[RAL_MATERIAL_PACKAGE_KEY_TEXT_CAPACITY];
	uint64_t byteLength = 0u;
	if ( !ops || !ops->read || !outReceipt
			|| !Ral_MaterialPackageCacheKeyText( expectedArtifactHash, key,
				sizeof( key ) ) ) return qfalse;
	candidate = malloc( sizeof( *candidate ) );
	if ( !candidate ) return qfalse;
	if ( !ops->read( context, expectedArtifactHash, key, candidate,
				sizeof( *candidate ), &byteLength )
			|| byteLength != sizeof( *candidate )
			|| candidate->artifactHash != expectedArtifactHash
			|| !Ral_MaterialPackageReceiptValid( candidate ) ) {
		free( candidate );
		return qfalse;
	}
	*outReceipt = *candidate;
	free( candidate );
	return qtrue;
}

qboolean Ral_MaterialProjectReferenceSeal( const char *mapName,
		uint32_t mapChecksum, uint64_t generation, uint32_t materialCount,
		uint64_t corpusHash, uint64_t manifestArtifactHash,
		ralMaterialProjectReferenceReceipt_t *outReceipt ) {
	ralMaterialProjectReferenceReceipt_t candidate;
	if ( !MapNameValid( mapName ) || !mapChecksum || !generation
			|| !materialCount || materialCount > RAL_MATERIAL_PACKAGE_MAX_ENTRIES
			|| !corpusHash || !manifestArtifactHash || !outReceipt ) return qfalse;
	memset( &candidate, 0, sizeof( candidate ) );
	candidate.schemaVersion = RAL_MATERIAL_PROJECT_REFERENCE_SCHEMA_VERSION;
	candidate.mapChecksum = mapChecksum;
	candidate.materialCount = materialCount;
	candidate.generation = generation;
	candidate.manifestArtifactHash = manifestArtifactHash;
	candidate.corpusHash = corpusHash;
	candidate.mapNameHash = MapNameHash( mapName );
	(void)snprintf( candidate.mapName, sizeof( candidate.mapName ), "%s", mapName );
	candidate.artifactHash = ProjectReferenceHash( &candidate );
	if ( !candidate.mapNameHash || !candidate.artifactHash ) return qfalse;
	candidate.ready = qtrue;
	*outReceipt = candidate;
	return qtrue;
}

qboolean Ral_MaterialProjectReferenceReceiptValid(
		const ralMaterialProjectReferenceReceipt_t *receipt ) {
	return receipt
		&& receipt->schemaVersion == RAL_MATERIAL_PROJECT_REFERENCE_SCHEMA_VERSION
		&& receipt->mapChecksum && receipt->materialCount
		&& receipt->materialCount <= RAL_MATERIAL_PACKAGE_MAX_ENTRIES
		&& receipt->reserved == 0u && receipt->generation
		&& receipt->manifestArtifactHash && receipt->corpusHash
		&& MapNameValid( receipt->mapName )
		&& receipt->mapNameHash == MapNameHash( receipt->mapName )
		&& receipt->artifactHash == ProjectReferenceHash( receipt )
		&& receipt->ready == qtrue ? qtrue : qfalse;
}

qboolean Ral_MaterialProjectReferenceKeyText( const char *mapName,
		uint32_t mapChecksum, char *outText, uint32_t capacity ) {
	const uint64_t mapNameHash = MapNameHash( mapName );
	int length;
	if ( !mapNameHash || !mapChecksum || !outText || !capacity ) return qfalse;
	length = snprintf( outText, capacity,
		"wrmp-v1-%016" PRIx64 "-%08" PRIx32,
		mapNameHash, mapChecksum );
	return length == 33 && (uint32_t)length < capacity ? qtrue : qfalse;
}

qboolean Ral_MaterialProjectReferenceStore(
		const ralMaterialProjectReferenceOps_t *ops, void *context,
		const ralMaterialProjectReferenceReceipt_t *receipt ) {
	char key[RAL_MATERIAL_PROJECT_REFERENCE_KEY_TEXT_CAPACITY];
	return ops && ops->writeAtomic
		&& Ral_MaterialProjectReferenceReceiptValid( receipt )
		&& Ral_MaterialProjectReferenceKeyText( receipt->mapName,
			receipt->mapChecksum, key, sizeof( key ) )
		&& ops->writeAtomic( context, key, receipt, sizeof( *receipt ) );
}

qboolean Ral_MaterialProjectReferenceLoad(
		const ralMaterialProjectReferenceOps_t *ops, void *context,
		const char *expectedMapName, uint32_t expectedMapChecksum,
		ralMaterialProjectReferenceReceipt_t *outReceipt ) {
	ralMaterialProjectReferenceReceipt_t candidate;
	char key[RAL_MATERIAL_PROJECT_REFERENCE_KEY_TEXT_CAPACITY];
	uint64_t byteLength = 0u;
	if ( !ops || !ops->read || !outReceipt
			|| !Ral_MaterialProjectReferenceKeyText( expectedMapName,
				expectedMapChecksum, key, sizeof( key ) )
			|| !ops->read( context, key, &candidate, sizeof( candidate ),
				&byteLength ) || byteLength != sizeof( candidate )
			|| !Ral_MaterialProjectReferenceReceiptValid( &candidate )
			|| candidate.mapChecksum != expectedMapChecksum
			|| strcmp( candidate.mapName, expectedMapName ) ) return qfalse;
	*outReceipt = candidate;
	return qtrue;
}
