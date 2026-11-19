// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "ral_material_cook.h"
#include "ral_material_proxy_lower.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define FNV64_OFFSET UINT64_C(14695981039346656037)
#define FNV64_PRIME UINT64_C(1099511628211)

static const ralMaterialProxyProgramReceipt_t zeroProxyReceipt;
static const ralMaterialCookBinding_t zeroCookBinding;
static const ralLegacyMaterialReceipt_t zeroLegacyReceipt;

static uint64_t HashByte( uint64_t hash, unsigned char value ) {
	return ( hash ^ value ) * FNV64_PRIME;
}

static uint64_t HashU32( uint64_t hash, uint32_t value ) {
	for ( uint32_t i = 0u; i < 4u; ++i )
		hash = HashByte( hash, (unsigned char)( value >> ( i * 8u ) ) );
	return hash;
}

static uint64_t HashU64( uint64_t hash, uint64_t value ) {
	for ( uint32_t i = 0u; i < 8u; ++i )
		hash = HashByte( hash, (unsigned char)( value >> ( i * 8u ) ) );
	return hash;
}

static uint64_t HashFloat( uint64_t hash, float value ) {
	uint32_t bits;
	memcpy( &bits, &value, sizeof( bits ) );
	return HashU32( hash, bits );
}

static uint64_t HashLegacy( uint64_t hash,
		const ralLegacyMaterialReceipt_t *legacy ) {
	const ralLegacyMaterialFacts_t *facts = &legacy->facts;
	const ralLegacyMaterialVariant_t *variant = &legacy->variant;
	hash = HashU32( hash, facts->textureCount );
	hash = HashU32( hash, (uint32_t)facts->shaderFog );
	hash = HashU32( hash, (uint32_t)facts->environmentMapping );
	hash = HashU32( hash, (uint32_t)facts->textureCoordinateAnimation );
	hash = HashU32( hash, (uint32_t)facts->vertexDeform );
	hash = HashU32( hash, (uint32_t)variant->entityStorageTransform );
	hash = HashU32( hash, (uint32_t)variant->alphaTest );
	hash = HashFloat( hash, variant->alphaTestValue );
	hash = HashFloat( hash, variant->depthFragment );
	hash = HashU32( hash, (uint32_t)variant->alphaToCoverage );
	hash = HashU32( hash, variant->textureDomainMask );
	hash = HashU32( hash, (uint32_t)variant->absoluteLight );
	hash = HashU32( hash, (uint32_t)variant->combine );
	hash = HashU32( hash, variant->discardMode );
	hash = HashFloat( hash, variant->fixedColor );
	hash = HashFloat( hash, variant->fixedAlpha );
	hash = HashU32( hash, variant->fogFactor );
	hash = HashFloat( hash, variant->depthFadeScale );
	hash = HashU32( hash, variant->normalFormat );
	hash = HashU32( hash, (uint32_t)variant->iblEnabled );
	hash = HashU32( hash, variant->lightmapSlot );
	hash = HashU32( hash, (uint32_t)legacy->outcome );
	hash = HashU32( hash, (uint32_t)legacy->fallbackReason );
	return hash;
}

static uint64_t CookArtifactHash( const ralMaterialCookReceipt_t *receipt ) {
	uint64_t hash = HashU32( FNV64_OFFSET, RAL_MATERIAL_COOK_SCHEMA_VERSION );
	hash = HashU64( hash, receipt->source.artifactHash );
	for ( uint32_t i = 0u; i < receipt->bindingCount; ++i ) {
		hash = HashU32( hash, receipt->bindings[i].dependencyIndex );
		hash = HashU64( hash, receipt->bindings[i].sourceId );
		hash = HashU64( hash, receipt->bindings[i].fsGeneration );
		hash = HashU64( hash, receipt->bindings[i].creationOptionsHash );
		hash = HashU64( hash, receipt->bindings[i].artifactHash );
	}
	hash = HashU64( hash, receipt->material.artifactHash );
	hash = HashU32( hash, receipt->legacyStageCount );
	for ( uint32_t i = 0u; i < receipt->legacyStageCount; ++i )
		hash = HashLegacy( hash, &receipt->legacyStages[i] );
	hash = HashU32( hash, receipt->hasProxy );
	if ( receipt->hasProxy ) hash = HashU64( hash, receipt->proxy.programHash );
	return hash ? hash : 1u;
}

static void Fail( ralMaterialCookDiagnostic_t *diagnostic,
		ralMaterialCookFailure_t reason, uint32_t itemIndex,
		ralMaterialSourceSpan_t span ) {
	if ( !diagnostic ) return;
	memset( diagnostic, 0, sizeof( *diagnostic ) );
	diagnostic->reason = reason;
	diagnostic->itemIndex = itemIndex;
	diagnostic->span = span;
}

static const ralMaterialCookBinding_t *FindBinding(
		const ralMaterialCookBinding_t *bindings, uint32_t bindingCount,
		uint32_t dependencyIndex ) {
	for ( uint32_t i = 0u; i < bindingCount; ++i )
		if ( bindings[i].dependencyIndex == dependencyIndex ) return &bindings[i];
	return NULL;
}

static qboolean ValidateBindings( const ralMaterialSourceReceipt_t *source,
		const ralMaterialCookBinding_t *bindings, uint32_t bindingCount,
		ralMaterialCookDiagnostic_t *diagnostic ) {
	if ( bindingCount > RAL_MATERIAL_COOK_MAX_BINDINGS ) {
		Fail( diagnostic, RAL_MATERIAL_COOK_OVER_CAPACITY, bindingCount,
			source->source.declaration ); return qfalse;
	}
	if ( bindingCount && !bindings ) {
		Fail( diagnostic, RAL_MATERIAL_COOK_INVALID_BINDING, 0u,
			source->source.declaration ); return qfalse;
	}
	for ( uint32_t i = 0u; i < bindingCount; ++i ) {
		const ralMaterialCookBinding_t *binding = &bindings[i];
		const ralMaterialSourceDependency_t *dependency;
		if ( binding->dependencyIndex >= source->source.dependencyCount ) {
			Fail( diagnostic, RAL_MATERIAL_COOK_INVALID_BINDING, i,
				source->source.declaration ); return qfalse;
		}
		dependency = &source->source.dependencies[binding->dependencyIndex];
		for ( uint32_t previous = 0u; previous < i; ++previous ) {
			if ( bindings[previous].dependencyIndex == binding->dependencyIndex ) {
				Fail( diagnostic, RAL_MATERIAL_COOK_DUPLICATE_BINDING, i,
					dependency->span ); return qfalse;
			}
		}
		if ( binding->role != dependency->role
				|| binding->sourceId != dependency->sourceId
				|| binding->creationOptionsHash
					!= dependency->creationOptionsHash
				|| !binding->artifactHash ) {
			Fail( diagnostic, RAL_MATERIAL_COOK_INVALID_BINDING, i,
				dependency->span ); return qfalse;
		}
		if ( binding->fsGeneration != dependency->fsGeneration ) {
			Fail( diagnostic, RAL_MATERIAL_COOK_STALE_BINDING, i,
				dependency->span ); return qfalse;
		}
		if ( dependency->role == RAL_MATERIAL_DEPENDENCY_TEXTURE ) {
			if ( !Ral_TextureCacheKeyValid( &binding->texture )
					|| binding->artifactHash != binding->texture.keyHash ) {
				Fail( diagnostic, RAL_MATERIAL_COOK_INVALID_BINDING, i,
					dependency->span ); return qfalse;
			}
		} else {
			ralTextureCacheKey_t zero;
			memset( &zero, 0, sizeof( zero ) );
			if ( memcmp( &binding->texture, &zero, sizeof( zero ) ) ) {
				Fail( diagnostic, RAL_MATERIAL_COOK_INVALID_BINDING, i,
					dependency->span ); return qfalse;
			}
		}
	}
	for ( uint32_t dependencyIndex = 0u;
			dependencyIndex < source->source.dependencyCount; ++dependencyIndex ) {
		if ( !FindBinding( bindings, bindingCount, dependencyIndex ) ) {
			Fail( diagnostic, RAL_MATERIAL_COOK_MISSING_BINDING,
				dependencyIndex, source->source.dependencies[dependencyIndex].span );
			return qfalse;
		}
	}
	return qtrue;
}

static ralLegacyAlphaTest_t LegacyAlphaTest(
		ralMaterialSourceAlphaTest_t alphaTest ) {
	return (ralLegacyAlphaTest_t)alphaTest;
}

static qboolean BuildLegacyStage( const ralMaterialSourceIr_t *source,
		const ralMaterialSourceStage_t *stage,
		ralLegacyMaterialReceipt_t *out ) {
	ralLegacyMaterialFacts_t facts;
	ralLegacyMaterialVariant_t variant;
	memset( &facts, 0, sizeof( facts ) );
	memset( &variant, 0, sizeof( variant ) );
	facts.textureCount = stage->mapKind == RAL_MATERIAL_MAP_WHITE ? 0u : 1u;
	facts.shaderFog = source->fogType == RAL_MATERIAL_FOG_NONE ? qfalse : qtrue;
	facts.environmentMapping = stage->tcGen == RAL_MATERIAL_TCGEN_ENVIRONMENT
		? qtrue : qfalse;
	facts.textureCoordinateAnimation = stage->tcModCount
		|| stage->animationFrameCount ? qtrue : qfalse;
	facts.vertexDeform = source->deformCount ? qtrue : qfalse;
	variant.entityStorageTransform = qtrue;
	variant.alphaTest = LegacyAlphaTest( stage->alphaTest );
	variant.alphaTestValue = stage->alphaTest == RAL_MATERIAL_ALPHA_TEST_NONE
		? 0.0f : stage->alphaCutoff;
	variant.depthFragment = 0.85f;
	variant.textureDomainMask = facts.textureCount ? 3u : 1u;
	variant.combine = RAL_LEGACY_COMBINE_MODULATE;
	variant.fixedColor = 1.0f;
	variant.fixedAlpha = 1.0f;
	variant.fogFactor = facts.shaderFog ? 1u : 0u;
	variant.depthFadeScale = 1.0f;
	variant.lightmapSlot = stage->tcGen == RAL_MATERIAL_TCGEN_LIGHTMAP ? 1u : 0u;
	return Ral_LegacyMaterialResolve( &facts, &variant, out );
}

static ralMaterialAlphaMode_t MaterialAlphaMode(
		const ralMaterialSourceIr_t *source, float *outCutoff ) {
	*outCutoff = 0.0f;
	for ( uint32_t i = 0u; i < source->stageCount; ++i ) {
		if ( source->stages[i].alphaTest != RAL_MATERIAL_ALPHA_TEST_NONE ) {
			*outCutoff = source->stages[i].alphaCutoff;
			return RAL_MATERIAL_ALPHA_MASK;
		}
	}
	if ( source->stageCount ) {
		const ralMaterialSourceStage_t *stage = &source->stages[0];
		if ( stage->sourceBlend == 1u && stage->destinationBlend == 1u )
			return RAL_MATERIAL_ALPHA_ADDITIVE;
		if ( stage->sourceBlend != 1u || stage->destinationBlend != 0u )
			return RAL_MATERIAL_ALPHA_BLEND;
	}
	return RAL_MATERIAL_ALPHA_OPAQUE;
}

static qboolean BuildMaterial( const ralMaterialSourceReceipt_t *source,
		const ralMaterialCookBinding_t *bindings, uint32_t bindingCount,
		ralMaterialReceipt_t *out ) {
	ralMaterialDescription_t description;
	memset( &description, 0, sizeof( description ) );
	description.schemaVersion = RAL_MATERIAL_SCHEMA_VERSION;
	description.materialGeneration = source->source.generation;
	description.provenanceHash = source->artifactHash;
	if ( source->source.provenance == RAL_MATERIAL_PROVENANCE_WIRED_LUA ) {
		description.source = RAL_MATERIAL_SOURCE_PBR;
		description.factors = source->source.pbrFactors;
		description.renderPolicy = source->source.pbrRenderPolicy;
		for ( uint32_t textureIndex = 0u;
				textureIndex < source->source.pbrTextureCount; ++textureIndex ) {
			const ralMaterialSourcePbrTexture_t *sourceTexture =
				&source->source.pbrTextures[textureIndex];
			const ralMaterialCookBinding_t *binding = FindBinding( bindings,
				bindingCount, sourceTexture->dependencyIndex );
			ralMaterialTexture_t *texture =
				&description.textures[description.textureCount++];
			if ( !binding ) return qfalse;
			texture->role = sourceTexture->role;
			texture->channels = sourceTexture->channels;
			texture->uvSet = sourceTexture->uvSet;
			texture->texture = binding->texture;
		}
		return Ral_MaterialCompile( &description, out );
	}
	description.source = RAL_MATERIAL_SOURCE_Q3;
	description.factors.baseColor[0] = description.factors.baseColor[1] =
		description.factors.baseColor[2] = description.factors.baseColor[3] = 1.0f;
	memcpy( description.factors.emissive, source->source.emissiveRadiance,
		sizeof( description.factors.emissive ) );
	description.factors.roughness = 1.0f;
	description.factors.normalScale = 1.0f;
	description.factors.occlusionStrength = 1.0f;
	description.renderPolicy.alphaMode = MaterialAlphaMode( &source->source,
		&description.renderPolicy.alphaCutoff );
	description.renderPolicy.doubleSided = source->source.cullMode == 2u
		? qtrue : qfalse;
	description.renderPolicy.depthTest = qtrue;
	description.renderPolicy.depthWrite = source->source.stageCount
		? source->source.stages[0].depthWrite : qfalse;
	for ( uint32_t stageIndex = 0u; stageIndex < source->source.stageCount
			&& description.textureCount < 2u; ++stageIndex ) {
		const ralMaterialSourceStage_t *stage = &source->source.stages[stageIndex];
		const ralMaterialCookBinding_t *binding;
		ralMaterialTexture_t *texture;
		if ( stage->mapKind != RAL_MATERIAL_MAP_IMAGE ) continue;
		binding = FindBinding( bindings, bindingCount, stage->dependencyIndex );
		if ( !binding ) return qfalse;
		texture = &description.textures[description.textureCount];
		texture->role = description.textureCount
			? RAL_MATERIAL_TEXTURE_LEGACY_STAGE1
			: RAL_MATERIAL_TEXTURE_LEGACY_STAGE0;
		texture->channels = RAL_MATERIAL_CHANNEL_LEGACY_RGBA;
		texture->uvSet = stage->tcGen == RAL_MATERIAL_TCGEN_LIGHTMAP ? 1u : 0u;
		texture->texture = binding->texture;
		description.textureCount++;
	}
	return Ral_MaterialCompile( &description, out );
}

qboolean Ral_MaterialCook( const ralMaterialSourceReceipt_t *source,
		const ralMaterialCookBinding_t *bindings, uint32_t bindingCount,
		ralMaterialCookReceipt_t *outReceipt,
		ralMaterialCookDiagnostic_t *outDiagnostic ) {
	ralMaterialCookReceipt_t *candidate;
	qboolean success = qfalse;
	if ( outDiagnostic ) memset( outDiagnostic, 0, sizeof( *outDiagnostic ) );
	if ( !source || !outReceipt || !Ral_MaterialSourceReceiptValid( source ) ) {
		if ( source ) Fail( outDiagnostic, RAL_MATERIAL_COOK_INVALID_SOURCE, 0u,
			source->source.declaration );
		return qfalse;
	}
	if ( !ValidateBindings( source, bindings, bindingCount, outDiagnostic ) )
		return qfalse;
	candidate = calloc( 1u, sizeof( *candidate ) );
	if ( !candidate ) {
		Fail( outDiagnostic, RAL_MATERIAL_COOK_ALLOCATION_REJECTED, 0u,
			source->source.declaration );
		return qfalse;
	}
	candidate->schemaVersion = RAL_MATERIAL_COOK_SCHEMA_VERSION;
	candidate->source = *source;
	candidate->bindingCount = bindingCount;
	for ( uint32_t dependencyIndex = 0u; dependencyIndex < bindingCount;
			++dependencyIndex ) {
		const ralMaterialCookBinding_t *binding = FindBinding( bindings,
			bindingCount, dependencyIndex );
		if ( !binding ) goto done;
		candidate->bindings[dependencyIndex] = *binding;
	}
	if ( !BuildMaterial( source, candidate->bindings, candidate->bindingCount,
			&candidate->material ) ) {
		Fail( outDiagnostic, RAL_MATERIAL_COOK_MATERIAL_REJECTED, 0u,
			source->source.declaration ); goto done;
	}
	candidate->legacyStageCount = source->source.provenance
		== RAL_MATERIAL_PROVENANCE_Q3_SHADER ? source->source.stageCount : 0u;
	for ( uint32_t i = 0u; i < candidate->legacyStageCount; ++i ) {
		if ( !BuildLegacyStage( &source->source, &source->source.stages[i],
				&candidate->legacyStages[i] ) ) {
			Fail( outDiagnostic, RAL_MATERIAL_COOK_LEGACY_REJECTED, i,
				source->source.stages[i].span ); goto done;
		}
	}
	{
		ralMaterialProxyLowerDiagnostic_t proxyDiagnostic;
		if ( !Ral_MaterialProxyLowerSource( source, &candidate->material,
				&candidate->hasProxy, &candidate->proxy, &proxyDiagnostic ) ) {
			Fail( outDiagnostic, RAL_MATERIAL_COOK_PROXY_REJECTED,
				proxyDiagnostic.operationIndex, proxyDiagnostic.span );
			goto done;
		}
	}
	candidate->artifactHash = CookArtifactHash( candidate );
	candidate->ready = qtrue;
	*outReceipt = *candidate;
	success = qtrue;
done:
	free( candidate );
	return success;
}

qboolean Ral_MaterialCookReceiptValid(
		const ralMaterialCookReceipt_t *receipt ) {
	ralMaterialReceipt_t expectedMaterial;
	ralLegacyMaterialReceipt_t expectedLegacy;
	if ( !receipt || receipt->schemaVersion != RAL_MATERIAL_COOK_SCHEMA_VERSION
			|| receipt->ready != qtrue
			|| !Ral_MaterialSourceReceiptValid( &receipt->source )
			|| !ValidateBindings( &receipt->source, receipt->bindings,
				receipt->bindingCount, NULL )
			|| !Ral_MaterialReceiptValid( &receipt->material )
			|| receipt->legacyStageCount !=
				( receipt->source.source.provenance
					== RAL_MATERIAL_PROVENANCE_Q3_SHADER
					? receipt->source.source.stageCount : 0u )
			|| ( receipt->hasProxy != qfalse && receipt->hasProxy != qtrue ) )
		return qfalse;
	for ( uint32_t i = receipt->bindingCount;
			i < RAL_MATERIAL_COOK_MAX_BINDINGS; ++i )
		if ( memcmp( &receipt->bindings[i], &zeroCookBinding,
				sizeof( zeroCookBinding ) ) ) return qfalse;
	for ( uint32_t i = receipt->legacyStageCount;
			i < RAL_MATERIAL_SOURCE_MAX_STAGES; ++i )
		if ( memcmp( &receipt->legacyStages[i], &zeroLegacyReceipt,
				sizeof( zeroLegacyReceipt ) ) ) return qfalse;
	{
		const unsigned char *bytes = (const unsigned char *)receipt;
		const size_t used = offsetof( ralMaterialCookReceipt_t, ready )
			+ sizeof( receipt->ready );
		for ( size_t i = used; i < sizeof( *receipt ); ++i )
			if ( bytes[i] ) return qfalse;
	}
	if ( !BuildMaterial( &receipt->source, receipt->bindings,
			receipt->bindingCount, &expectedMaterial )
			|| !Ral_MaterialReceiptExact( &receipt->material,
				&expectedMaterial ) ) return qfalse;
	for ( uint32_t i = 0u; i < receipt->legacyStageCount; ++i ) {
		if ( !BuildLegacyStage( &receipt->source.source,
				&receipt->source.source.stages[i], &expectedLegacy )
				|| !Ral_LegacyMaterialReceiptExact( &receipt->legacyStages[i],
					&expectedLegacy ) ) return qfalse;
	}
	if ( receipt->hasProxy ) {
		if ( !Ral_MaterialProxyProgramReceiptValid( &receipt->proxy )
				|| receipt->proxy.program.materialArtifactHash
					!= receipt->material.artifactHash ) return qfalse;
	} else {
		if ( memcmp( &receipt->proxy, &zeroProxyReceipt,
				sizeof( zeroProxyReceipt ) ) ) return qfalse;
	}
	return receipt->artifactHash == CookArtifactHash( receipt ) ? qtrue : qfalse;
}

qboolean Ral_MaterialCookReceiptExact( const ralMaterialCookReceipt_t *a,
		const ralMaterialCookReceipt_t *b ) {
	return Ral_MaterialCookReceiptValid( a )
		&& Ral_MaterialCookReceiptValid( b )
		&& !memcmp( a, b, sizeof( *a ) );
}
