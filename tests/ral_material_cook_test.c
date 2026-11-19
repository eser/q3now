// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "ral_material_cook.h"

#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if ( !(x) ) { fprintf( stderr, "FAIL %s:%d: %s\n", \
	__FILE__, __LINE__, #x ); return 1; } } while ( 0 )

static qboolean Key( uint64_t generation, ralTextureCacheKey_t *out ) {
	ralTextureAssetRequest_t request;
	ralTextureAssetReceipt_t receipt;
	memset( &request, 0, sizeof( request ) );
	request.schemaVersion = RAL_TEXTURE_ASSET_SCHEMA_VERSION;
	request.assetGeneration = generation;
	request.provenanceHash = generation * 101u;
	request.dimension = RAL_TEXTURE_ASSET_2D;
	request.width = request.height = 8u;
	request.depth = request.layers = 1u;
	request.sourceMipLevels = 1u;
	request.colorEncoding = RAL_TEXTURE_ENCODING_SRGB;
	request.channelSemantic = RAL_TEXTURE_CHANNEL_COLOR;
	request.sourceEncoding = RAL_TEXTURE_SOURCE_UNCOMPRESSED;
	request.mipPolicy = RAL_TEXTURE_MIPS_SOURCE;
	request.residency = RAL_TEXTURE_RESIDENCY_STREAMED;
	request.preferenceCount = 1u;
	request.preferences[0] = RAL_TEXTURE_COMPRESSION_UNCOMPRESSED;
	request.allowUncompressedFallback = qtrue;
	return Ral_ResolveTextureAsset( &request, &receipt )
		&& Ral_TextureCacheKeyBuild( &receipt, out );
}

static qboolean PbrKey( uint64_t generation, ralMaterialTextureRole_t role,
		ralTextureCacheKey_t *out ) {
	ralTextureAssetRequest_t request;
	ralTextureAssetReceipt_t receipt;
	memset( &request, 0, sizeof( request ) );
	request.schemaVersion = RAL_TEXTURE_ASSET_SCHEMA_VERSION;
	request.assetGeneration = generation;
	request.provenanceHash = generation * 101u;
	request.dimension = RAL_TEXTURE_ASSET_2D;
	request.width = request.height = 8u;
	request.depth = request.layers = 1u;
	request.sourceMipLevels = 1u;
	request.colorEncoding = role == RAL_MATERIAL_TEXTURE_NORMAL
		|| role == RAL_MATERIAL_TEXTURE_ORM
		? RAL_TEXTURE_ENCODING_LINEAR : RAL_TEXTURE_ENCODING_SRGB;
	request.channelSemantic = role == RAL_MATERIAL_TEXTURE_NORMAL
		? RAL_TEXTURE_CHANNEL_NORMAL_XY : role == RAL_MATERIAL_TEXTURE_ORM
		? RAL_TEXTURE_CHANNEL_ORM : RAL_TEXTURE_CHANNEL_COLOR;
	request.sourceEncoding = RAL_TEXTURE_SOURCE_UNCOMPRESSED;
	request.mipPolicy = RAL_TEXTURE_MIPS_SOURCE;
	request.residency = RAL_TEXTURE_RESIDENCY_STREAMED;
	request.preferenceCount = 1u;
	request.preferences[0] = RAL_TEXTURE_COMPRESSION_UNCOMPRESSED;
	request.allowUncompressedFallback = qtrue;
	return Ral_ResolveTextureAsset( &request, &receipt )
		&& Ral_TextureCacheKeyBuild( &receipt, out );
}

static ralMaterialSourceIr_t Source( void ) {
	ralMaterialSourceIr_t source;
	memset( &source, 0, sizeof( source ) );
	source.schemaVersion = RAL_MATERIAL_SOURCE_IR_SCHEMA_VERSION;
	source.generation = 3u;
	source.provenance = RAL_MATERIAL_PROVENANCE_Q3_SHADER;
	source.declaration.sourceId = 101u;
	source.declaration.line = 11u;
	source.declaration.column = 3u;
	(void)snprintf( source.semanticName, sizeof( source.semanticName ),
		"textures/base/wall" );
	source.sort = 3.0f;
	source.diffuseReflectance[0] = source.diffuseReflectance[1] =
		source.diffuseReflectance[2] = 1.0f;
	source.dependencyCount = 2u;
	for ( uint32_t i = 0u; i < source.dependencyCount; ++i ) {
		ralMaterialSourceDependency_t *dependency = &source.dependencies[i];
		dependency->role = RAL_MATERIAL_DEPENDENCY_TEXTURE;
		dependency->sourceId = 5001u + i;
		dependency->fsGeneration = 9u;
		dependency->size = 4096u + i;
		dependency->creationOptionsHash = 17u + i;
		dependency->span.sourceId = 101u;
		dependency->span.line = 12u + i;
		dependency->span.column = 5u;
		(void)snprintf( dependency->canonicalPath,
			sizeof( dependency->canonicalPath ), "textures/base/wall%u.ktx2", i );
	}
	source.stageCount = 2u;
	for ( uint32_t i = 0u; i < source.stageCount; ++i ) {
		ralMaterialSourceStage_t *stage = &source.stages[i];
		stage->mapKind = RAL_MATERIAL_MAP_IMAGE;
		stage->dependencyIndex = i;
		stage->tcGen = i ? RAL_MATERIAL_TCGEN_LIGHTMAP
			: RAL_MATERIAL_TCGEN_TEXTURE;
		stage->sourceBlend = 1u;
		stage->destinationBlend = 0u;
		stage->depthWrite = i ? qfalse : qtrue;
		stage->rgbGen = RAL_MATERIAL_COLOR_IDENTITY;
		stage->alphaGen = RAL_MATERIAL_ALPHA_IDENTITY;
		stage->constantColor[0] = stage->constantColor[1] =
			stage->constantColor[2] = stage->constantColor[3] = 1.0f;
		stage->animationLooping = qtrue;
		stage->span.sourceId = 101u;
		stage->span.line = 20u + i;
		stage->span.column = 7u;
	}
	source.stages[1].alphaTest = RAL_MATERIAL_ALPHA_TEST_KEEP_GREATER_EQUAL;
	source.stages[1].alphaCutoff = 0.5f;
	source.stages[0].tcModCount = 1u;
	source.stages[0].tcMods[0].type = RAL_MATERIAL_TCMOD_SCROLL;
	source.stages[0].tcMods[0].span = source.stages[0].span;
	source.stages[0].tcMods[0].parameters[0] = 0.25f;
	return source;
}

static ralMaterialSourceIr_t PbrSource( void ) {
	ralMaterialSourceIr_t source = Source();
	source.provenance = RAL_MATERIAL_PROVENANCE_WIRED_LUA;
	source.stageCount = 0u;
	source.pbrFactors.baseColor[0] = 0.8f;
	source.pbrFactors.baseColor[1] = 0.7f;
	source.pbrFactors.baseColor[2] = 0.6f;
	source.pbrFactors.baseColor[3] = 1.0f;
	source.pbrFactors.metallic = 0.25f;
	source.pbrFactors.roughness = 0.45f;
	source.pbrFactors.normalScale = 1.0f;
	source.pbrFactors.occlusionStrength = 0.9f;
	source.pbrRenderPolicy.depthTest = qtrue;
	source.pbrRenderPolicy.depthWrite = qtrue;
	source.pbrTextureCount = 2u;
	source.pbrTextures[0].span = source.declaration;
	source.pbrTextures[0].dependencyIndex = 0u;
	source.pbrTextures[0].role = RAL_MATERIAL_TEXTURE_BASE_COLOR;
	source.pbrTextures[0].channels = RAL_MATERIAL_CHANNEL_RGBA;
	source.pbrTextures[1].span = source.declaration;
	source.pbrTextures[1].dependencyIndex = 1u;
	source.pbrTextures[1].role = RAL_MATERIAL_TEXTURE_NORMAL;
	source.pbrTextures[1].channels = RAL_MATERIAL_CHANNEL_XY_NORMAL;
	return source;
}

static qboolean Bind( const ralMaterialSourceReceipt_t *source,
		uint32_t dependencyIndex, uint64_t textureGeneration,
		ralMaterialCookBinding_t *out ) {
	const ralMaterialSourceDependency_t *dependency =
		&source->source.dependencies[dependencyIndex];
	memset( out, 0, sizeof( *out ) );
	out->dependencyIndex = dependencyIndex;
	out->role = dependency->role;
	out->sourceId = dependency->sourceId;
	out->fsGeneration = dependency->fsGeneration;
	out->creationOptionsHash = dependency->creationOptionsHash;
	if ( !Key( textureGeneration, &out->texture ) ) return qfalse;
	out->artifactHash = out->texture.keyHash;
	return qtrue;
}

int main( void ) {
	ralMaterialSourceIr_t sourceIr = Source();
	ralMaterialSourceIr_t pbrIr;
	ralMaterialSourceReceipt_t source;
	ralMaterialCookBinding_t bindings[2], reversed[2], bad[2];
	ralMaterialCookReceipt_t receipt, exact, reordered, before, changed;
	ralMaterialCookDiagnostic_t diagnostic;
	CHECK( Ral_MaterialSourceCompile( &sourceIr, &source, NULL ) );
	CHECK( Bind( &source, 0u, 41u, &bindings[0] ) );
	CHECK( Bind( &source, 1u, 42u, &bindings[1] ) );
	memset( &receipt, 0xA5, sizeof( receipt ) );
	CHECK( Ral_MaterialCook( &source, bindings, 2u, &receipt, &diagnostic ) );
	CHECK( diagnostic.reason == RAL_MATERIAL_COOK_OK );
	CHECK( Ral_MaterialCookReceiptValid( &receipt ) );
	CHECK( receipt.bindingCount == 2u && receipt.legacyStageCount == 2u );
	CHECK( receipt.hasProxy
		&& Ral_MaterialProxyProgramReceiptValid( &receipt.proxy ) );
	CHECK( receipt.material.material.textureCount == 2u );
	CHECK( receipt.material.material.renderPolicy.alphaMode
		== RAL_MATERIAL_ALPHA_MASK );
	CHECK( receipt.legacyStages[1].outcome == RAL_LEGACY_MATERIAL_FALLBACK );
	CHECK( receipt.legacyStages[1].fallbackReason
		== RAL_LEGACY_FALLBACK_ALPHA_TEST );
	exact = receipt;
	CHECK( Ral_MaterialCookReceiptExact( &receipt, &exact ) );

	reversed[0] = bindings[1]; reversed[1] = bindings[0];
	CHECK( Ral_MaterialCook( &source, reversed, 2u, &reordered, NULL ) );
	CHECK( Ral_MaterialCookReceiptExact( &receipt, &reordered ) );

	before = receipt;
	CHECK( !Ral_MaterialCook( &source, bindings, 1u, &receipt, &diagnostic ) );
	CHECK( diagnostic.reason == RAL_MATERIAL_COOK_MISSING_BINDING );
	CHECK( !memcmp( &receipt, &before, sizeof( receipt ) ) );

	memcpy( bad, bindings, sizeof( bad ) );
	bad[1].dependencyIndex = 0u;
	CHECK( !Ral_MaterialCook( &source, bad, 2u, &receipt, &diagnostic ) );
	CHECK( diagnostic.reason == RAL_MATERIAL_COOK_DUPLICATE_BINDING );
	CHECK( !memcmp( &receipt, &before, sizeof( receipt ) ) );

	memcpy( bad, bindings, sizeof( bad ) );
	bad[1].fsGeneration++;
	CHECK( !Ral_MaterialCook( &source, bad, 2u, &receipt, &diagnostic ) );
	CHECK( diagnostic.reason == RAL_MATERIAL_COOK_STALE_BINDING );
	CHECK( diagnostic.span.line == 13u );
	CHECK( !memcmp( &receipt, &before, sizeof( receipt ) ) );

	memcpy( bad, bindings, sizeof( bad ) );
	CHECK( Key( 43u, &bad[1].texture ) );
	bad[1].artifactHash = bad[1].texture.keyHash;
	CHECK( Ral_MaterialCook( &source, bad, 2u, &changed, NULL ) );
	CHECK( changed.artifactHash != before.artifactHash );
	CHECK( changed.material.variantKey == before.material.variantKey );

	pbrIr = PbrSource();
	CHECK( Ral_MaterialSourceCompile( &pbrIr, &source, NULL ) );
	CHECK( Bind( &source, 0u, 51u, &bindings[0] ) );
	CHECK( Bind( &source, 1u, 52u, &bindings[1] ) );
	CHECK( PbrKey( 51u, RAL_MATERIAL_TEXTURE_BASE_COLOR,
		&bindings[0].texture ) );
	bindings[0].artifactHash = bindings[0].texture.keyHash;
	CHECK( PbrKey( 52u, RAL_MATERIAL_TEXTURE_NORMAL,
		&bindings[1].texture ) );
	bindings[1].artifactHash = bindings[1].texture.keyHash;
	CHECK( Ral_MaterialCook( &source, bindings, 2u, &receipt, &diagnostic ) );
	CHECK( Ral_MaterialCookReceiptValid( &receipt ) );
	CHECK( receipt.legacyStageCount == 0u );
	CHECK( !receipt.hasProxy );
	CHECK( receipt.material.material.source == RAL_MATERIAL_SOURCE_PBR );
	CHECK( receipt.material.material.textureCount == 2u );
	CHECK( receipt.material.material.textures[0].role
		== RAL_MATERIAL_TEXTURE_BASE_COLOR );
	CHECK( receipt.material.material.textures[1].role
		== RAL_MATERIAL_TEXTURE_NORMAL );
	CHECK( receipt.material.material.factors.roughness == 0.45f );
	CHECK( receipt.material.material.factors.metallic == 0.25f );

	puts( "ral_material_cook_test: ok" );
	return 0;
}
