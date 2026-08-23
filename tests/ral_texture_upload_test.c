// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_texture_upload.h"

#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if ( !(x) ) { fprintf( stderr, "FAIL %s:%d: %s\n", \
	__FILE__, __LINE__, #x ); return 1; } } while ( 0 )

static int Resolve( ralTextureAssetDimension_t dimension, uint32_t width,
		uint32_t height, uint32_t depth, uint32_t layers, uint32_t mips,
		ralTextureCompressionFamily_t compression,
		ralTextureAssetReceipt_t *out ) {
	ralTextureAssetRequest_t request;
	memset( &request, 0, sizeof( request ) );
	request.schemaVersion = RAL_TEXTURE_ASSET_SCHEMA_VERSION;
	request.assetGeneration = 9u; request.provenanceHash = 0x987654u;
	request.dimension = dimension; request.width = width; request.height = height;
	request.depth = depth; request.layers = layers; request.sourceMipLevels = mips;
	request.colorEncoding = RAL_TEXTURE_ENCODING_SRGB;
	request.channelSemantic = RAL_TEXTURE_CHANNEL_COLOR;
	request.sourceEncoding = RAL_TEXTURE_SOURCE_BASIS_UASTC;
	request.mipPolicy = RAL_TEXTURE_MIPS_SOURCE;
	request.residency = RAL_TEXTURE_RESIDENCY_STREAMED;
	request.capabilities.bc = compression == RAL_TEXTURE_COMPRESSION_BC;
	request.capabilities.astc = compression == RAL_TEXTURE_COMPRESSION_ASTC;
	request.capabilities.etc2 = compression == RAL_TEXTURE_COMPRESSION_ETC2;
	request.preferenceCount = 1u; request.preferences[0] = compression;
	request.allowUncompressedFallback = qtrue;
	return Ral_ResolveTextureAsset( &request, out );
}

static int Exercise( ralTextureAssetDimension_t dimension, uint32_t width,
		uint32_t height, uint32_t depth, uint32_t layers, uint32_t mips,
		ralTextureCompressionFamily_t compression, const uint64_t *levelBytes,
		ralTextureType_t expectedType ) {
	unsigned char payload[4096], artifact[8192], staging[16384], second[16384];
	ralTextureAssetReceipt_t asset;
	ralTextureArtifactReceipt_t encoded;
	ralTextureUploadPlan_t plan, exact;
	uint64_t payloadBytes = 0u;
	uint32_t i;
	for ( i = 0u; i < mips; i++ ) payloadBytes += levelBytes[i];
	CHECK( payloadBytes <= sizeof( payload ) );
	for ( i = 0u; i < payloadBytes; i++ ) payload[i] = (unsigned char)( i * 31u + 7u );
	CHECK( Resolve( dimension, width, height, depth, layers, mips,
		compression, &asset ) );
	CHECK( Ral_EncodeTextureArtifact( &asset, levelBytes, mips, payload,
		payloadBytes, artifact, sizeof( artifact ), &encoded ) );
	memset( &plan, 0xA5, sizeof( plan ) );
	CHECK( Ral_BuildTextureArtifactUploadPlan( artifact,
		encoded.containerByteLength, &asset, &plan ) );
	CHECK( Ral_TextureUploadPlanValid( &plan ) && plan.texture.type == expectedType
		&& plan.texture.concurrentGraphicsTransfer && plan.levelCount == mips );
	CHECK( plan.levels[mips - 1u].stagingOffset
			+ plan.levels[mips - 1u].stagingLength == plan.stagingByteLength );
	CHECK( plan.stagingByteLength <= sizeof( staging ) );
	memset( staging, 0xCC, sizeof( staging ) );
	CHECK( Ral_PackTextureArtifactUpload( artifact, encoded.containerByteLength,
		&plan, staging, sizeof( staging ) ) );
	CHECK( !memcmp( staging + plan.levels[0].stagingOffset,
		artifact + plan.levels[0].artifactOffset,
		plan.levels[0].tightBytesPerRow ) );
	CHECK( plan.levels[0].region.bytesPerRow % 256u == 0u );
	CHECK( Ral_BuildTextureArtifactUploadPlan( artifact,
		encoded.containerByteLength, &asset, &exact )
		&& Ral_TextureUploadPlanExact( &plan, &exact ) );
	memset( second, 0x5A, sizeof( second ) );
	CHECK( Ral_PackTextureArtifactUpload( artifact, encoded.containerByteLength,
		&exact, second, sizeof( second ) ) );
	CHECK( !memcmp( staging, second, (size_t)plan.stagingByteLength ) );
	return 0;
}

static int Hostile( void ) {
	unsigned char payload[80], artifact[1024], badArtifact[1024];
	unsigned char staging[2048], before[2048];
	uint64_t levels[2] = { 64u, 16u }, wrong[2] = { 63u, 17u };
	ralTextureAssetReceipt_t asset, stale;
	ralTextureArtifactReceipt_t encoded;
	ralTextureUploadPlan_t plan, beforePlan, badPlan;
	uint64_t validLength;
	memset( payload, 3, sizeof( payload ) );
	CHECK( Resolve( RAL_TEXTURE_ASSET_2D, 8u, 8u, 1u, 1u, 2u,
		RAL_TEXTURE_COMPRESSION_BC, &asset ) );
	CHECK( Ral_EncodeTextureArtifact( &asset, levels, 2u, payload, sizeof( payload ),
		artifact, sizeof( artifact ), &encoded ) );
	validLength = encoded.containerByteLength;
	memset( &plan, 0x4D, sizeof( plan ) ); beforePlan = plan;
	stale = asset; stale.provenanceHash++;
	CHECK( !Ral_BuildTextureArtifactUploadPlan( artifact, encoded.containerByteLength,
		&stale, &plan ) && !memcmp( &plan, &beforePlan, sizeof( plan ) ) );
	memcpy( badArtifact, artifact, (size_t)encoded.containerByteLength );
	badArtifact[encoded.payloadByteOffset] ^= 1u;
	CHECK( !Ral_BuildTextureArtifactUploadPlan( badArtifact,
		encoded.containerByteLength, &asset, &plan )
		&& !memcmp( &plan, &beforePlan, sizeof( plan ) ) );
	CHECK( Ral_EncodeTextureArtifact( &asset, wrong, 2u, payload, sizeof( payload ),
		badArtifact, sizeof( badArtifact ), &encoded ) );
	CHECK( !Ral_BuildTextureArtifactUploadPlan( badArtifact,
		encoded.containerByteLength, &asset, &plan )
		&& !memcmp( &plan, &beforePlan, sizeof( plan ) ) );
	CHECK( Ral_BuildTextureArtifactUploadPlan( artifact, validLength, &asset, &plan ) );
	badPlan = plan; badPlan.levels[0].region.bytesPerRow = 255u;
	memset( staging, 0xCC, sizeof( staging ) ); memcpy( before, staging, sizeof( staging ) );
	CHECK( !Ral_PackTextureArtifactUpload( artifact, validLength, &badPlan,
		staging, sizeof( staging ) ) && !memcmp( staging, before, sizeof( staging ) ) );
	CHECK( !Ral_PackTextureArtifactUpload( artifact, validLength, &plan,
		staging, plan.stagingByteLength - 1u )
		&& !memcmp( staging, before, sizeof( staging ) ) );
	return 0;
}

int main( void ) {
	const uint64_t bc2d[2] = { 64u, 16u };
	const uint64_t rgbaArray[2] = { 384u, 96u };
	const uint64_t astcCube[1] = { 384u };
	const uint64_t astcCubeArray[1] = { 768u };
	const uint64_t rgba3d[2] = { 512u, 64u };
	CHECK( Exercise( RAL_TEXTURE_ASSET_2D, 8u, 8u, 1u, 1u, 2u,
		RAL_TEXTURE_COMPRESSION_BC, bc2d, RAL_TEXTURE_2D ) == 0 );
	CHECK( Exercise( RAL_TEXTURE_ASSET_2D_ARRAY, 8u, 4u, 1u, 3u, 2u,
		RAL_TEXTURE_COMPRESSION_UNCOMPRESSED, rgbaArray, RAL_TEXTURE_2D_ARRAY ) == 0 );
	CHECK( Exercise( RAL_TEXTURE_ASSET_CUBE, 8u, 8u, 1u, 6u, 1u,
		RAL_TEXTURE_COMPRESSION_ASTC, astcCube, RAL_TEXTURE_CUBE ) == 0 );
	CHECK( Exercise( RAL_TEXTURE_ASSET_CUBE, 8u, 8u, 1u, 12u, 1u,
		RAL_TEXTURE_COMPRESSION_ASTC, astcCubeArray, RAL_TEXTURE_CUBE_ARRAY ) == 0 );
	CHECK( Exercise( RAL_TEXTURE_ASSET_3D, 8u, 4u, 4u, 1u, 2u,
		RAL_TEXTURE_COMPRESSION_UNCOMPRESSED, rgba3d, RAL_TEXTURE_3D ) == 0 );
	CHECK( Hostile() == 0 );
	puts( "ral_texture_upload_test: ok" );
	return 0;
}
