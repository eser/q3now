// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "ral_material_source.h"

#include <math.h>
#include <string.h>

#define FNV64_OFFSET UINT64_C(14695981039346656037)
#define FNV64_PRIME UINT64_C(1099511628211)

static const ralMaterialProxyProgram_t zeroProxyProgram;

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

static qboolean PbrTextureSemanticValid(
		const ralMaterialSourcePbrTexture_t *texture ) {
	if ( !texture || texture->role < RAL_MATERIAL_TEXTURE_BASE_COLOR
			|| texture->role > RAL_MATERIAL_TEXTURE_ENVIRONMENT
			|| texture->uvSet > 1u ) return qfalse;
	switch ( texture->role ) {
	case RAL_MATERIAL_TEXTURE_BASE_COLOR:
		return texture->channels == RAL_MATERIAL_CHANNEL_RGBA;
	case RAL_MATERIAL_TEXTURE_NORMAL:
		return texture->channels == RAL_MATERIAL_CHANNEL_XY_NORMAL;
	case RAL_MATERIAL_TEXTURE_ORM:
		return texture->channels == RAL_MATERIAL_CHANNEL_RGB_ORM;
	case RAL_MATERIAL_TEXTURE_OCCLUSION:
		return texture->channels == RAL_MATERIAL_CHANNEL_R_OCCLUSION;
	case RAL_MATERIAL_TEXTURE_METALLIC_ROUGHNESS:
		return texture->channels
			== RAL_MATERIAL_CHANNEL_GB_ROUGHNESS_METALLIC;
	case RAL_MATERIAL_TEXTURE_EMISSIVE:
		return texture->channels == RAL_MATERIAL_CHANNEL_RGB_EMISSIVE;
	case RAL_MATERIAL_TEXTURE_LIGHTMAP:
	case RAL_MATERIAL_TEXTURE_ENVIRONMENT:
		return texture->channels == RAL_MATERIAL_CHANNEL_RGBA;
	default:
		return qfalse;
	}
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

static qboolean ProxyFieldNameValid( const char *name ) {
	size_t length;
	if ( !name || !name[0] ) return qfalse;
	length = strnlen( name, RAL_MATERIAL_PROXY_FIELD_NAME );
	return length > 0u && length < RAL_MATERIAL_PROXY_FIELD_NAME;
}

/* Source receipts carry an artifact-unbound expression program.  Cook binds
 * it to the canonical material artifact before the existing proxy-v2 builder
 * validates and hashes the runtime product. */
static qboolean AuthoredProxyValid( const ralMaterialProxyProgram_t *program ) {
	uint32_t i, j;
	if ( !program
			|| program->schemaVersion != RAL_MATERIAL_PROXY_SCHEMA_VERSION
			|| program->materialArtifactHash != 0u
			|| program->instructionCount != 0u
			|| program->expressionRegisterCount
				< RAL_MATERIAL_PROXY_REGISTER_BASE_VALUES
					+ RAL_MATERIAL_PROXY_BASE_VALUE_COUNT
			|| program->expressionRegisterCount
				> RAL_MATERIAL_PROXY_EXPRESSION_MAX_REGISTERS
			|| program->expressionConstantCount
				> RAL_MATERIAL_PROXY_EXPRESSION_MAX_CONSTANTS
			|| !program->expressionInstructionCount
			|| program->expressionInstructionCount
				> RAL_MATERIAL_PROXY_EXPRESSION_MAX_INSTRUCTIONS
			|| !program->expressionFieldCount
			|| program->expressionFieldCount
				> RAL_MATERIAL_PROXY_EXPRESSION_MAX_FIELDS
			|| program->expressionTableCount
				> RAL_MATERIAL_PROXY_EXPRESSION_MAX_TABLES ) return qfalse;
	for ( i = 0u; i < program->expressionTableCount; ++i ) {
		const ralMaterialExpressionTable_t *table =
			&program->expressionTables[i];
		if ( table->sampleCount < 2u
				|| table->sampleCount
					> RAL_MATERIAL_PROXY_EXPRESSION_TABLE_SAMPLES
				|| ( table->flags
					& ~( RAL_MATERIAL_EXPRESSION_TABLE_WRAP
						| RAL_MATERIAL_EXPRESSION_TABLE_SNAP ) ) ) return qfalse;
	}
	for ( i = 0u; i < program->expressionInstructionCount; ++i ) {
		const ralMaterialExpressionInstruction_t *op =
			&program->expressionInstructions[i];
		if ( op->opcode >= RAL_MATERIAL_EXPRESSION_OPCODE_COUNT
				|| op->destination >= program->expressionRegisterCount ) return qfalse;
		switch ( op->opcode ) {
		case RAL_MATERIAL_EXPRESSION_UNMASKED_MOVE_CONSTANT:
			if ( op->a >= program->expressionConstantCount || op->b ) return qfalse;
			break;
		case RAL_MATERIAL_EXPRESSION_UNMASKED_MOVE:
			if ( op->a >= program->expressionRegisterCount || op->b ) return qfalse;
			break;
		case RAL_MATERIAL_EXPRESSION_MOVE_CONSTANT:
			if ( op->a >= program->expressionConstantCount || !( op->b & 15u )
					|| ( op->b & ~15u ) ) return qfalse;
			break;
		case RAL_MATERIAL_EXPRESSION_MOVE:
			if ( op->a >= program->expressionRegisterCount || !( op->b & 15u )
					|| ( op->b & ~15u ) ) return qfalse;
			break;
		case RAL_MATERIAL_EXPRESSION_SWIZZLE:
			if ( op->a >= program->expressionRegisterCount || ( op->b & ~255u ) )
				return qfalse;
			break;
		case RAL_MATERIAL_EXPRESSION_TABLE:
			if ( op->a >= program->expressionRegisterCount
					|| op->b >= program->expressionTableCount ) return qfalse;
			break;
		default:
			if ( op->a >= program->expressionRegisterCount
					|| op->b >= program->expressionRegisterCount ) return qfalse;
			break;
		}
	}
	for ( i = 0u; i < program->expressionFieldCount; ++i ) {
		const ralMaterialExpressionField_t *field =
			&program->expressionFields[i];
		uint32_t inlineEnd, outputEnd;
		if ( !ProxyFieldNameValid( field->name ) || field->reserved
				|| field->registerIndex >= program->expressionRegisterCount
				|| !field->componentCount || field->componentCount > 4u
				|| ( field->inlineByteOffset & 3u ) ) return qfalse;
		inlineEnd = field->inlineByteOffset + field->componentCount * 4u;
		outputEnd = field->outputValueOffset + field->componentCount;
		if ( inlineEnd > RAL_MATERIAL_PROXY_INLINE_MAX_BYTES
				|| outputEnd > RAL_MATERIAL_PROXY_VALUE_COUNT ) return qfalse;
		for ( j = 0u; j < i; ++j ) {
			const ralMaterialExpressionField_t *previous =
				&program->expressionFields[j];
			const uint32_t previousInlineEnd = previous->inlineByteOffset
				+ previous->componentCount * 4u;
			const uint32_t previousOutputEnd = previous->outputValueOffset
				+ previous->componentCount;
			if ( !strcmp( field->name, previous->name )
					|| ( field->inlineByteOffset < previousInlineEnd
						&& previous->inlineByteOffset < inlineEnd )
					|| ( field->outputValueOffset < previousOutputEnd
						&& previous->outputValueOffset < outputEnd ) ) return qfalse;
		}
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

static qboolean SkyValid( const ralMaterialSourceIr_t *source ) {
	const ralMaterialSourceSky_t *sky = &source->skySource;
	if ( !BoolValid( sky->outerBox ) || !BoolValid( sky->innerBox )
			|| !BoolValid( sky->hasSun ) || !BoolValid( sky->sunExtended )
			|| ( ( sky->outerBox || sky->innerBox ) && !source->sky )
			|| ( sky->sunExtended && !sky->hasSun )
			|| !isfinite( sky->sunIntensity ) || sky->sunIntensity < 0.0f
			|| !isfinite( sky->sunAzimuthDegrees )
			|| !isfinite( sky->sunElevationDegrees )
			|| !isfinite( sky->sunDevianceDegrees )
			|| sky->sunDevianceDegrees < 0.0f
			|| ( !sky->sunExtended
				&& ( sky->sunDevianceDegrees != 0.0f || sky->sunSamples ) )
			|| ( !sky->hasSun && ( sky->sunIntensity != 0.0f
				|| sky->sunAzimuthDegrees != 0.0f
				|| sky->sunElevationDegrees != 0.0f ) ) ) return qfalse;
	for ( uint32_t i = 0u; i < 3u; ++i )
		if ( !isfinite( sky->sunColor[i] ) || sky->sunColor[i] < 0.0f
				|| ( !sky->hasSun && sky->sunColor[i] != 0.0f ) ) return qfalse;
	for ( uint32_t box = 0u; box < 2u; ++box ) {
		qboolean present = box ? sky->innerBox : sky->outerBox;
		const uint32_t *faces = box ? sky->innerDependencies
			: sky->outerDependencies;
		for ( uint32_t face = 0u; face < RAL_MATERIAL_SKY_FACE_COUNT; ++face ) {
			if ( !present ) {
				if ( faces[face] ) return qfalse;
			} else if ( faces[face] >= source->dependencyCount
					|| source->dependencies[faces[face]].role
						!= RAL_MATERIAL_DEPENDENCY_TEXTURE ) return qfalse;
		}
	}
	return qtrue;
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
			|| source->deformCount > RAL_MATERIAL_SOURCE_MAX_DEFORMS
			|| source->pbrTextureCount > RAL_MATERIAL_MAX_TEXTURES ) {
		Fail( diagnostic, RAL_MATERIAL_SOURCE_OVER_CAPACITY, 0u,
			source->declaration ); return qfalse;
	}
	if ( !SkyValid( source ) ) {
		Fail( diagnostic, RAL_MATERIAL_SOURCE_INVALID_SKY, 0u,
			source->declaration ); return qfalse;
	}
	if ( !BoolValid( source->clearSolid )
			|| !BoolValid( source->sky ) || !BoolValid( source->noDraw )
			|| !BoolValid( source->polygonOffset )
			|| !BoolValid( source->noPicMip )
			|| !BoolValid( source->noMipMaps )
			|| source->fogType > RAL_MATERIAL_FOG_EXP2
			|| !isfinite( source->sort )
			|| !isfinite( source->skyCloudHeight ) || !isfinite( source->fogDepth )
			|| !isfinite( source->fogDensity )
			|| !isfinite( source->fogFarClip )
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
	if ( source->provenance == RAL_MATERIAL_PROVENANCE_WIRED_LUA ) {
		qboolean hasOrm = qfalse, hasSplit = qfalse;
		for ( i = 0u; i < 4u; ++i ) {
			if ( !isfinite( source->pbrFactors.baseColor[i] )
					|| source->pbrFactors.baseColor[i] < 0.0f
					|| source->pbrFactors.baseColor[i] > 1.0f ) {
				Fail( diagnostic, RAL_MATERIAL_SOURCE_NONFINITE_VALUE, i,
					source->declaration ); return qfalse;
			}
		}
		for ( i = 0u; i < 3u; ++i ) {
			if ( !isfinite( source->pbrFactors.emissive[i] )
					|| source->pbrFactors.emissive[i] < 0.0f ) {
				Fail( diagnostic, RAL_MATERIAL_SOURCE_NONFINITE_VALUE, i,
					source->declaration ); return qfalse;
			}
		}
		if ( !isfinite( source->pbrFactors.metallic )
				|| source->pbrFactors.metallic < 0.0f
				|| source->pbrFactors.metallic > 1.0f
				|| !isfinite( source->pbrFactors.roughness )
				|| source->pbrFactors.roughness < 0.0f
				|| source->pbrFactors.roughness > 1.0f
				|| !isfinite( source->pbrFactors.normalScale )
				|| source->pbrFactors.normalScale < 0.0f
				|| !isfinite( source->pbrFactors.occlusionStrength )
				|| source->pbrFactors.occlusionStrength < 0.0f
				|| source->pbrFactors.occlusionStrength > 1.0f
				|| source->pbrRenderPolicy.alphaMode
					> RAL_MATERIAL_ALPHA_ADDITIVE
				|| !isfinite( source->pbrRenderPolicy.alphaCutoff )
				|| ( source->pbrRenderPolicy.alphaMode
					== RAL_MATERIAL_ALPHA_MASK
					? ( source->pbrRenderPolicy.alphaCutoff <= 0.0f
						|| source->pbrRenderPolicy.alphaCutoff > 1.0f )
					: source->pbrRenderPolicy.alphaCutoff != 0.0f )
				|| !BoolValid( source->pbrRenderPolicy.doubleSided )
				|| !BoolValid( source->pbrRenderPolicy.depthTest )
				|| !BoolValid( source->pbrRenderPolicy.depthWrite ) ) {
			Fail( diagnostic, RAL_MATERIAL_SOURCE_NONFINITE_VALUE, 0u,
				source->declaration ); return qfalse;
		}
		for ( i = 0u; i < source->pbrTextureCount; ++i ) {
			const ralMaterialSourcePbrTexture_t *texture =
				&source->pbrTextures[i];
			if ( !PbrTextureSemanticValid( texture )
					|| texture->dependencyIndex >= source->dependencyCount
					|| source->dependencies[texture->dependencyIndex].role
						!= RAL_MATERIAL_DEPENDENCY_TEXTURE ) {
				Fail( diagnostic, RAL_MATERIAL_SOURCE_INVALID_PBR_TEXTURE, i,
					texture->span ); return qfalse;
			}
			if ( i && source->pbrTextures[i - 1u].role >= texture->role ) {
				Fail( diagnostic, RAL_MATERIAL_SOURCE_DUPLICATE_PBR_TEXTURE, i,
					texture->span ); return qfalse;
			}
			if ( texture->role == RAL_MATERIAL_TEXTURE_ORM ) hasOrm = qtrue;
			if ( texture->role == RAL_MATERIAL_TEXTURE_OCCLUSION
					|| texture->role
						== RAL_MATERIAL_TEXTURE_METALLIC_ROUGHNESS ) hasSplit = qtrue;
		}
		if ( hasOrm && hasSplit ) {
			Fail( diagnostic, RAL_MATERIAL_SOURCE_INVALID_PBR_TEXTURE, 0u,
				source->declaration ); return qfalse;
		}
	} else if ( source->pbrTextureCount ) {
		Fail( diagnostic, RAL_MATERIAL_SOURCE_INVALID_PBR_TEXTURE, 0u,
			source->declaration ); return qfalse;
	}
	if ( !BoolValid( source->hasAuthoredProxy )
			|| ( source->hasAuthoredProxy
				? ( source->provenance != RAL_MATERIAL_PROVENANCE_WIRED_LUA
					|| !AuthoredProxyValid( &source->authoredProxy ) )
				: memcmp( &source->authoredProxy, &zeroProxyProgram,
					sizeof( source->authoredProxy ) ) != 0 ) ) {
		Fail( diagnostic, RAL_MATERIAL_SOURCE_INVALID_AUTHORED_PROXY, 0u,
			source->declaration ); return qfalse;
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
				|| stage->depthFunc > RAL_MATERIAL_DEPTH_EQUAL
				|| stage->alphaTest
					> RAL_MATERIAL_ALPHA_TEST_KEEP_GREATER_EQUAL
				|| stage->rgbGen > RAL_MATERIAL_COLOR_CONST
				|| stage->alphaGen > RAL_MATERIAL_ALPHA_CONST
				|| !BoolValid( stage->depthWrite )
				|| !BoolValid( stage->animationLooping )
				|| stage->animationFrameCount
					> RAL_MATERIAL_SOURCE_MAX_ANIMATION_FRAMES
				|| stage->tcModCount > RAL_MATERIAL_SOURCE_MAX_TCMODS
				|| !isfinite( stage->alphaCutoff )
				|| !isfinite( stage->portalRange )
				|| !isfinite( stage->animationFrequency )
				|| ( stage->mapKind == RAL_MATERIAL_MAP_IMAGE
					&& stage->dependencyIndex >= source->dependencyCount ) ) {
			Fail( diagnostic, RAL_MATERIAL_SOURCE_INVALID_STAGE, i,
				stage->span ); return qfalse;
		}
		for ( j = 0u; j < 2u; ++j ) {
			uint32_t axis;
			for ( axis = 0u; axis < 3u; ++axis ) if ( !isfinite(
					stage->tcGenVectors[j][axis] ) ) {
				Fail( diagnostic, RAL_MATERIAL_SOURCE_NONFINITE_VALUE, i,
					stage->span ); return qfalse;
			}
		}
		for ( j = 0u; j < 4u; ++j ) if ( !isfinite(
				stage->constantColor[j] ) ) {
			Fail( diagnostic, RAL_MATERIAL_SOURCE_NONFINITE_VALUE, i,
				stage->span ); return qfalse;
		}
		if ( stage->rgbWave.function > RAL_MATERIAL_WAVE_NOISE
				|| stage->alphaWave.function > RAL_MATERIAL_WAVE_NOISE
				|| !isfinite( stage->rgbWave.base )
				|| !isfinite( stage->rgbWave.amplitude )
				|| !isfinite( stage->rgbWave.phase )
				|| !isfinite( stage->rgbWave.frequency )
				|| !isfinite( stage->alphaWave.base )
				|| !isfinite( stage->alphaWave.amplitude )
				|| !isfinite( stage->alphaWave.phase )
				|| !isfinite( stage->alphaWave.frequency ) ) {
			Fail( diagnostic, RAL_MATERIAL_SOURCE_NONFINITE_VALUE, i,
				stage->span ); return qfalse;
		}
		for ( j = 0u; j < stage->animationFrameCount; ++j ) {
			if ( stage->animationDependencyIndices[j]
					>= source->dependencyCount ) {
				Fail( diagnostic, RAL_MATERIAL_SOURCE_INVALID_STAGE, i,
					stage->span ); return qfalse;
			}
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
				|| source->deforms[i].type > RAL_MATERIAL_DEFORM_TEXT7 ) {
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

static uint64_t HashAuthoredProxy( uint64_t hash,
		const ralMaterialProxyProgram_t *program ) {
	uint32_t i, lane;
	hash = HashU32( hash, program->schemaVersion );
	hash = HashU32( hash, program->expressionRegisterCount );
	hash = HashU32( hash, program->expressionConstantCount );
	for ( i = 0u; i < program->expressionConstantCount; ++i )
		for ( lane = 0u; lane < 4u; ++lane ) hash = HashU32( hash,
			(uint32_t)program->expressionConstants[i].lane[lane] );
	hash = HashU32( hash, program->expressionInstructionCount );
	for ( i = 0u; i < program->expressionInstructionCount; ++i ) {
		const ralMaterialExpressionInstruction_t *op =
			&program->expressionInstructions[i];
		hash = HashU32( hash, op->opcode );
		hash = HashU32( hash, op->destination );
		hash = HashU32( hash, op->a );
		hash = HashU32( hash, op->b );
	}
	hash = HashU32( hash, program->expressionFieldCount );
	for ( i = 0u; i < program->expressionFieldCount; ++i ) {
		const ralMaterialExpressionField_t *field =
			&program->expressionFields[i];
		uint32_t character;
		for ( character = 0u; character < RAL_MATERIAL_PROXY_FIELD_NAME;
				++character ) hash = HashByte( hash,
				(unsigned char)field->name[character] );
		hash = HashU32( hash, field->registerIndex );
		hash = HashU32( hash, field->inlineByteOffset );
		hash = HashU32( hash, field->componentCount );
		hash = HashU32( hash, field->outputValueOffset );
	}
	hash = HashU32( hash, program->expressionTableCount );
	for ( i = 0u; i < program->expressionTableCount; ++i ) {
		const ralMaterialExpressionTable_t *table =
			&program->expressionTables[i];
		hash = HashU32( hash, table->sampleCount );
		hash = HashU32( hash, table->flags );
		for ( uint32_t sample = 0u; sample < table->sampleCount; ++sample )
			hash = HashU32( hash, (uint32_t)table->samplesQ16[sample] );
	}
	return hash;
}

static uint64_t HashSemantic( const ralMaterialSourceIr_t *source ) {
	uint64_t hash = HashU32( FNV64_OFFSET,
		RAL_MATERIAL_SOURCE_IR_SCHEMA_VERSION );
	uint32_t i, j;
	hash = HashU32( hash, (uint32_t)source->provenance );
	hash = HashString( hash, source->semanticName );
	hash = HashU32( hash, source->surfaceFlags );
	hash = HashU32( hash, source->contentFlags );
	hash = HashU32( hash, (uint32_t)source->clearSolid );
	hash = HashU32( hash, source->cullMode );
	hash = HashFloat( hash, source->sort );
	hash = HashU32( hash, (uint32_t)source->sky );
	hash = HashU32( hash, (uint32_t)source->noDraw );
	hash = HashU32( hash, (uint32_t)source->polygonOffset );
	hash = HashU32( hash, (uint32_t)source->noPicMip );
	hash = HashU32( hash, (uint32_t)source->noMipMaps );
	hash = HashFloat( hash, source->skyCloudHeight );
	hash = HashU32( hash, (uint32_t)source->skySource.outerBox );
	hash = HashU32( hash, (uint32_t)source->skySource.innerBox );
	for ( i = 0u; i < RAL_MATERIAL_SKY_FACE_COUNT; ++i ) {
		hash = HashU32( hash, source->skySource.outerDependencies[i] );
		hash = HashU32( hash, source->skySource.innerDependencies[i] );
	}
	hash = HashU32( hash, (uint32_t)source->skySource.hasSun );
	hash = HashU32( hash, (uint32_t)source->skySource.sunExtended );
	for ( i = 0u; i < 3u; ++i )
		hash = HashFloat( hash, source->skySource.sunColor[i] );
	hash = HashFloat( hash, source->skySource.sunIntensity );
	hash = HashFloat( hash, source->skySource.sunAzimuthDegrees );
	hash = HashFloat( hash, source->skySource.sunElevationDegrees );
	hash = HashFloat( hash, source->skySource.sunDevianceDegrees );
	hash = HashU32( hash, source->skySource.sunSamples );
	for ( i = 0u; i < 3u; ++i ) hash = HashFloat( hash, source->fogColor[i] );
	hash = HashFloat( hash, source->fogDepth );
	hash = HashU32( hash, (uint32_t)source->fogType );
	hash = HashFloat( hash, source->fogDensity );
	hash = HashFloat( hash, source->fogFarClip );
	for ( i = 0u; i < 3u; ++i ) hash = HashFloat( hash,
		source->diffuseReflectance[i] );
	for ( i = 0u; i < 3u; ++i ) hash = HashFloat( hash,
		source->emissiveRadiance[i] );
	hash = HashFloat( hash, source->emissiveRange );
	hash = HashU32( hash, source->lightingFlags );
	if ( source->provenance == RAL_MATERIAL_PROVENANCE_WIRED_LUA ) {
		for ( i = 0u; i < 4u; ++i ) hash = HashFloat( hash,
			source->pbrFactors.baseColor[i] );
		for ( i = 0u; i < 3u; ++i ) hash = HashFloat( hash,
			source->pbrFactors.emissive[i] );
		hash = HashFloat( hash, source->pbrFactors.metallic );
		hash = HashFloat( hash, source->pbrFactors.roughness );
		hash = HashFloat( hash, source->pbrFactors.normalScale );
		hash = HashFloat( hash, source->pbrFactors.occlusionStrength );
		hash = HashU32( hash, (uint32_t)source->pbrRenderPolicy.alphaMode );
		hash = HashFloat( hash, source->pbrRenderPolicy.alphaCutoff );
		hash = HashU32( hash, (uint32_t)source->pbrRenderPolicy.doubleSided );
		hash = HashU32( hash, (uint32_t)source->pbrRenderPolicy.depthTest );
		hash = HashU32( hash, (uint32_t)source->pbrRenderPolicy.depthWrite );
		hash = HashU32( hash, source->pbrTextureCount );
		for ( i = 0u; i < source->pbrTextureCount; ++i ) {
			hash = HashU32( hash, source->pbrTextures[i].dependencyIndex );
			hash = HashU32( hash, (uint32_t)source->pbrTextures[i].role );
			hash = HashU32( hash, (uint32_t)source->pbrTextures[i].channels );
			hash = HashU32( hash, source->pbrTextures[i].uvSet );
		}
		hash = HashU32( hash, (uint32_t)source->hasAuthoredProxy );
		if ( source->hasAuthoredProxy )
			hash = HashAuthoredProxy( hash, &source->authoredProxy );
	}
	hash = HashU32( hash, source->stageCount );
	for ( i = 0u; i < source->stageCount; ++i ) {
		const ralMaterialSourceStage_t *stage = &source->stages[i];
		hash = HashU32( hash, (uint32_t)stage->mapKind );
		hash = HashU32( hash, stage->dependencyIndex );
		hash = HashU32( hash, (uint32_t)stage->tcGen );
		for ( j = 0u; j < 2u; ++j ) {
			uint32_t axis;
			for ( axis = 0u; axis < 3u; ++axis ) hash = HashFloat( hash,
				stage->tcGenVectors[j][axis] );
		}
		hash = HashU32( hash, stage->sourceBlend );
		hash = HashU32( hash, stage->destinationBlend );
		hash = HashU32( hash, stage->alphaTest );
		hash = HashFloat( hash, stage->alphaCutoff );
		hash = HashU32( hash, (uint32_t)stage->depthWrite );
		hash = HashU32( hash, (uint32_t)stage->depthFunc );
		hash = HashU32( hash, (uint32_t)stage->rgbGen );
		hash = HashU32( hash, (uint32_t)stage->alphaGen );
		for ( j = 0u; j < 4u; ++j ) hash = HashFloat( hash,
			stage->constantColor[j] );
		hash = HashU32( hash, (uint32_t)stage->rgbWave.function );
		hash = HashFloat( hash, stage->rgbWave.base );
		hash = HashFloat( hash, stage->rgbWave.amplitude );
		hash = HashFloat( hash, stage->rgbWave.phase );
		hash = HashFloat( hash, stage->rgbWave.frequency );
		hash = HashU32( hash, (uint32_t)stage->alphaWave.function );
		hash = HashFloat( hash, stage->alphaWave.base );
		hash = HashFloat( hash, stage->alphaWave.amplitude );
		hash = HashFloat( hash, stage->alphaWave.phase );
		hash = HashFloat( hash, stage->alphaWave.frequency );
		hash = HashFloat( hash, stage->portalRange );
		hash = HashFloat( hash, stage->animationFrequency );
		hash = HashU32( hash, (uint32_t)stage->animationLooping );
		hash = HashU32( hash, stage->animationFrameCount );
		for ( j = 0u; j < stage->animationFrameCount; ++j )
			hash = HashU32( hash, stage->animationDependencyIndices[j] );
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
	uint64_t semanticHash, dependencyHash, artifactHash;
	if ( !receipt
			|| receipt->schemaVersion != RAL_MATERIAL_SOURCE_RECEIPT_SCHEMA_VERSION
			|| receipt->ready != qtrue || !Validate( &receipt->source, NULL ) )
		return qfalse;
	semanticHash = HashSemantic( &receipt->source );
	dependencyHash = HashDependencies( &receipt->source );
	artifactHash = HashU64( HashU64( semanticHash, dependencyHash ),
		receipt->source.generation );
	artifactHash = HashU64( artifactHash, receipt->source.declaration.sourceId );
	if ( !artifactHash ) artifactHash = 1u;
	return receipt->semanticHash == semanticHash
		&& receipt->dependencyHash == dependencyHash
		&& receipt->artifactHash == artifactHash ? qtrue : qfalse;
}

qboolean Ral_MaterialSourceReceiptExact(
		const ralMaterialSourceReceipt_t *a,
		const ralMaterialSourceReceipt_t *b ) {
	return Ral_MaterialSourceReceiptValid( a )
		&& Ral_MaterialSourceReceiptValid( b )
		&& !memcmp( a, b, sizeof( *a ) );
}
