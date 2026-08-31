// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "ral_material_source.h"

#include <math.h>
#include <string.h>

#define FNV64_OFFSET UINT64_C(14695981039346656037)
#define FNV64_PRIME UINT64_C(1099511628211)

static uint64_t HashByte( uint64_t hash, unsigned char value ) {
	return ( hash ^ value ) * FNV64_PRIME;
}

static uint64_t HashU32( uint64_t hash, uint32_t value ) {
	uint32_t i;
	for ( i = 0u; i < 4u; ++i ) hash = HashByte( hash,
		(unsigned char)( value >> ( i * 8u ) ) );
	return hash;
}

static uint64_t HashU64( uint64_t hash, uint64_t value ) {
	uint32_t i;
	for ( i = 0u; i < 8u; ++i ) hash = HashByte( hash,
		(unsigned char)( value >> ( i * 8u ) ) );
	return hash;
}

static uint64_t HashFloat( uint64_t hash, float value ) {
	uint32_t bits = 0u;
	if ( value != 0.0f ) memcpy( &bits, &value, sizeof( bits ) );
	return HashU32( hash, bits );
}

static uint64_t HashString( uint64_t hash, const char *value ) {
	while ( value && *value ) hash = HashByte( hash,
		(unsigned char)*value++ );
	return HashByte( hash, 0u );
}

static qboolean BoolValid( qboolean value ) {
	return value == qfalse || value == qtrue;
}

static qboolean CanonicalPathValid( const char *path ) {
	const char *component;
	if ( !path || !path[0] || path[0] == '/' || path[0] == '\\'
			|| strchr( path, ':' ) || strlen( path ) >= MAX_QPATH ) return qfalse;
	component = path;
	while ( *component ) {
		const char *end = component;
		while ( *end && *end != '/' ) {
			if ( *end == '\\' || (unsigned char)*end < 32u ) return qfalse;
			++end;
		}
		if ( end == component || ( end - component == 1 && component[0] == '.' )
				|| ( end - component == 2 && component[0] == '.'
					&& component[1] == '.' ) ) return qfalse;
		component = *end ? end + 1 : end;
	}
	return qtrue;
}

static void Fail( ralMaterialSourceDiagnostic_t *diagnostic,
	ralMaterialSourceFailure_t reason, uint32_t itemIndex,
	ralMaterialSourceSpan_t span ) {
	if ( !diagnostic ) return;
	memset( diagnostic, 0, sizeof( *diagnostic ) );
	diagnostic->reason = reason;
	diagnostic->itemIndex = itemIndex;
	diagnostic->span = span;
}

static qboolean Validate( const ralMaterialSourceIr_t *source,
	ralMaterialSourceDiagnostic_t *diagnostic ) {
	uint32_t i, j;
	if ( source->schemaVersion != RAL_MATERIAL_SOURCE_IR_SCHEMA_VERSION ) {
		Fail( diagnostic, RAL_MATERIAL_SOURCE_INVALID_SCHEMA, 0u,
			source->declaration ); return qfalse;
	}
	if ( !source->generation || source->provenance < RAL_MATERIAL_PROVENANCE_Q3_SHADER
			|| source->provenance > RAL_MATERIAL_PROVENANCE_WIRED_LUA
			|| !source->declaration.sourceId ) {
		Fail( diagnostic, RAL_MATERIAL_SOURCE_INVALID_PROVENANCE, 0u,
			source->declaration ); return qfalse;
	}
	if ( !source->semanticName[0]
			|| strlen( source->semanticName ) >= sizeof( source->semanticName ) ) {
		Fail( diagnostic, RAL_MATERIAL_SOURCE_INVALID_SEMANTIC_NAME, 0u,
			source->declaration ); return qfalse;
	}
	if ( source->dependencyCount > RAL_MATERIAL_SOURCE_MAX_DEPENDENCIES
			|| source->stageCount > RAL_MATERIAL_SOURCE_MAX_STAGES
			|| source->deformCount > RAL_MATERIAL_SOURCE_MAX_DEFORMS ) {
		Fail( diagnostic, RAL_MATERIAL_SOURCE_OVER_CAPACITY, 0u,
			source->declaration ); return qfalse;
	}
	if ( !BoolValid( source->sky ) || !BoolValid( source->noDraw )
			|| !BoolValid( source->polygonOffset ) || !isfinite( source->sort )
			|| !isfinite( source->skyCloudHeight ) || !isfinite( source->fogDepth )
			|| !isfinite( source->emissiveRange ) ) {
		Fail( diagnostic, RAL_MATERIAL_SOURCE_NONFINITE_VALUE, 0u,
			source->declaration ); return qfalse;
	}
	for ( i = 0u; i < 3u; ++i ) {
		if ( !isfinite( source->fogColor[i] )
				|| !isfinite( source->diffuseReflectance[i] )
				|| !isfinite( source->emissiveRadiance[i] ) ) {
			Fail( diagnostic, RAL_MATERIAL_SOURCE_NONFINITE_VALUE, i,
				source->declaration ); return qfalse;
		}
	}
	for ( i = 0u; i < source->dependencyCount; ++i ) {
		const ralMaterialSourceDependency_t *dependency = &source->dependencies[i];
		if ( dependency->role < RAL_MATERIAL_DEPENDENCY_TEXTURE
				|| dependency->role > RAL_MATERIAL_DEPENDENCY_RESIDENCY
				|| !dependency->sourceId || !dependency->fsGeneration
				|| !CanonicalPathValid( dependency->canonicalPath ) ) {
			Fail( diagnostic, RAL_MATERIAL_SOURCE_INVALID_DEPENDENCY, i,
				dependency->span ); return qfalse;
		}
		for ( j = 0u; j < i; ++j ) {
			const ralMaterialSourceDependency_t *previous = &source->dependencies[j];
			if ( dependency->role == previous->role
					&& dependency->sourceId == previous->sourceId
					&& dependency->creationOptionsHash == previous->creationOptionsHash ) {
				Fail( diagnostic, RAL_MATERIAL_SOURCE_DUPLICATE_DEPENDENCY, i,
					dependency->span ); return qfalse;
			}
		}
	}
	for ( i = 0u; i < source->stageCount; ++i ) {
		const ralMaterialSourceStage_t *stage = &source->stages[i];
		if ( stage->mapKind < RAL_MATERIAL_MAP_IMAGE
				|| stage->mapKind > RAL_MATERIAL_MAP_WHITE
				|| stage->tcGen > RAL_MATERIAL_TCGEN_VECTOR
				|| !BoolValid( stage->depthWrite )
				|| stage->tcModCount > RAL_MATERIAL_SOURCE_MAX_TCMODS
				|| !isfinite( stage->alphaCutoff )
				|| ( stage->mapKind == RAL_MATERIAL_MAP_IMAGE
					&& stage->dependencyIndex >= source->dependencyCount ) ) {
			Fail( diagnostic, RAL_MATERIAL_SOURCE_INVALID_STAGE, i,
				stage->span ); return qfalse;
		}
		for ( j = 0u; j < stage->tcModCount; ++j ) {
			uint32_t p;
			if ( stage->tcMods[j].type < RAL_MATERIAL_TCMOD_SCALE
					|| stage->tcMods[j].type > RAL_MATERIAL_TCMOD_TRANSFORM ) {
				Fail( diagnostic, RAL_MATERIAL_SOURCE_INVALID_TCMOD, i,
					stage->tcMods[j].span ); return qfalse;
			}
			for ( p = 0u; p < 6u; ++p ) if ( !isfinite(
					stage->tcMods[j].parameters[p] ) ) {
				Fail( diagnostic, RAL_MATERIAL_SOURCE_NONFINITE_VALUE, i,
					stage->tcMods[j].span ); return qfalse;
			}
		}
	}
	for ( i = 0u; i < source->deformCount; ++i ) {
		if ( source->deforms[i].type < RAL_MATERIAL_DEFORM_WAVE
				|| source->deforms[i].type > RAL_MATERIAL_DEFORM_AUTOSPRITE2 ) {
			Fail( diagnostic, RAL_MATERIAL_SOURCE_INVALID_DEFORM, i,
				source->deforms[i].span ); return qfalse;
		}
		for ( j = 0u; j < 8u; ++j ) if ( !isfinite(
				source->deforms[i].parameters[j] ) ) {
			Fail( diagnostic, RAL_MATERIAL_SOURCE_NONFINITE_VALUE, i,
				source->deforms[i].span ); return qfalse;
		}
	}
	return qtrue;
}

static uint64_t HashDependencies( const ralMaterialSourceIr_t *source ) {
	uint64_t hash = HashU32( FNV64_OFFSET, source->dependencyCount );
	uint32_t i;
	for ( i = 0u; i < source->dependencyCount; ++i ) {
		const ralMaterialSourceDependency_t *dependency = &source->dependencies[i];
		hash = HashU32( hash, (uint32_t)dependency->role );
		hash = HashU64( hash, dependency->sourceId );
		hash = HashU64( hash, dependency->fsGeneration );
		hash = HashU64( hash, dependency->size );
		hash = HashU64( hash, dependency->creationOptionsHash );
		hash = HashString( hash, dependency->canonicalPath );
	}
	return hash ? hash : 1u;
}

static uint64_t HashSemantic( const ralMaterialSourceIr_t *source ) {
	uint64_t hash = HashU32( FNV64_OFFSET,
		RAL_MATERIAL_SOURCE_IR_SCHEMA_VERSION );
	uint32_t i, j;
	hash = HashU32( hash, (uint32_t)source->provenance );
	hash = HashString( hash, source->semanticName );
	hash = HashU32( hash, source->surfaceFlags );
	hash = HashU32( hash, source->cullMode );
	hash = HashFloat( hash, source->sort );
	hash = HashU32( hash, (uint32_t)source->sky );
	hash = HashU32( hash, (uint32_t)source->noDraw );
	hash = HashU32( hash, (uint32_t)source->polygonOffset );
	hash = HashFloat( hash, source->skyCloudHeight );
	for ( i = 0u; i < 3u; ++i ) hash = HashFloat( hash, source->fogColor[i] );
	hash = HashFloat( hash, source->fogDepth );
	for ( i = 0u; i < 3u; ++i ) hash = HashFloat( hash,
		source->diffuseReflectance[i] );
	for ( i = 0u; i < 3u; ++i ) hash = HashFloat( hash,
		source->emissiveRadiance[i] );
	hash = HashFloat( hash, source->emissiveRange );
	hash = HashU32( hash, source->lightingFlags );
	hash = HashU32( hash, source->stageCount );
	for ( i = 0u; i < source->stageCount; ++i ) {
		const ralMaterialSourceStage_t *stage = &source->stages[i];
		hash = HashU32( hash, (uint32_t)stage->mapKind );
		hash = HashU32( hash, stage->dependencyIndex );
		hash = HashU32( hash, (uint32_t)stage->tcGen );
		hash = HashU32( hash, stage->sourceBlend );
		hash = HashU32( hash, stage->destinationBlend );
		hash = HashU32( hash, stage->alphaTest );
		hash = HashFloat( hash, stage->alphaCutoff );
		hash = HashU32( hash, (uint32_t)stage->depthWrite );
		hash = HashU32( hash, stage->tcModCount );
		for ( j = 0u; j < stage->tcModCount; ++j ) {
			uint32_t p;
			hash = HashU32( hash, (uint32_t)stage->tcMods[j].type );
			for ( p = 0u; p < 6u; ++p ) hash = HashFloat( hash,
				stage->tcMods[j].parameters[p] );
		}
	}
	hash = HashU32( hash, source->deformCount );
	for ( i = 0u; i < source->deformCount; ++i ) {
		hash = HashU32( hash, (uint32_t)source->deforms[i].type );
		for ( j = 0u; j < 8u; ++j ) hash = HashFloat( hash,
			source->deforms[i].parameters[j] );
	}
	return hash ? hash : 1u;
}

qboolean Ral_MaterialSourceCompile( const ralMaterialSourceIr_t *source,
		ralMaterialSourceReceipt_t *outReceipt,
		ralMaterialSourceDiagnostic_t *outDiagnostic ) {
	ralMaterialSourceReceipt_t candidate;
	if ( outDiagnostic ) memset( outDiagnostic, 0, sizeof( *outDiagnostic ) );
	if ( !source || !outReceipt ) return qfalse;
	if ( !Validate( source, outDiagnostic ) ) return qfalse;
	memset( &candidate, 0, sizeof( candidate ) );
	candidate.schemaVersion = RAL_MATERIAL_SOURCE_RECEIPT_SCHEMA_VERSION;
	candidate.source = *source;
	candidate.semanticHash = HashSemantic( source );
	candidate.dependencyHash = HashDependencies( source );
	candidate.artifactHash = HashU64( HashU64( candidate.semanticHash,
		candidate.dependencyHash ), source->generation );
	candidate.artifactHash = HashU64( candidate.artifactHash,
		source->declaration.sourceId );
	if ( !candidate.artifactHash ) candidate.artifactHash = 1u;
	candidate.ready = qtrue;
	*outReceipt = candidate;
	return qtrue;
}

qboolean Ral_MaterialSourceReceiptValid(
		const ralMaterialSourceReceipt_t *receipt ) {
	ralMaterialSourceReceipt_t candidate;
	return receipt
		&& receipt->schemaVersion == RAL_MATERIAL_SOURCE_RECEIPT_SCHEMA_VERSION
		&& receipt->ready == qtrue
		&& Ral_MaterialSourceCompile( &receipt->source, &candidate, NULL )
		&& candidate.semanticHash == receipt->semanticHash
		&& candidate.dependencyHash == receipt->dependencyHash
		&& candidate.artifactHash == receipt->artifactHash;
}

qboolean Ral_MaterialSourceReceiptExact(
		const ralMaterialSourceReceipt_t *a,
		const ralMaterialSourceReceipt_t *b ) {
	return Ral_MaterialSourceReceiptValid( a )
		&& Ral_MaterialSourceReceiptValid( b )
		&& !memcmp( a, b, sizeof( *a ) );
}
