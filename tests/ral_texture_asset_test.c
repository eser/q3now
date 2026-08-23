// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_texture_asset.h"
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if ( !(x) ) { fprintf( stderr, "FAIL %s:%d: %s\n", \
	__FILE__, __LINE__, #x ); return 1; } } while ( 0 )

static void Put32( unsigned char *p, uint32_t v ) {
	p[0] = (unsigned char)v; p[1] = (unsigned char)( v >> 8u );
	p[2] = (unsigned char)( v >> 16u ); p[3] = (unsigned char)( v >> 24u );
}
static void Put64( unsigned char *p, uint64_t v ) {
	Put32( p, (uint32_t)v ); Put32( p + 4, (uint32_t)( v >> 32u ) );
}
static uint64_t ArtifactHash( const unsigned char *bytes, uint64_t length ) {
	uint64_t i, hash = UINT64_C( 14695981039346656037 );
	for ( i = 0u; i < length; i++ ) {
		unsigned char value = i >= 24u && i < 32u ? 0u : bytes[i];
		hash ^= value; hash *= UINT64_C( 1099511628211 );
	}
	return hash ? hash : 1u;
}
static void ResealArtifact( unsigned char *bytes, uint64_t length ) {
	Put64( bytes + 24, ArtifactHash( bytes, length ) );
}
static uint64_t MakeKtx2( unsigned char *b, uint32_t model, uint32_t scheme ) {
	static const unsigned char id[12] = { 0xAB, 0x4B, 0x54, 0x58, 0x20,
		0x32, 0x30, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A };
	uint64_t payload = scheme == RAL_KTX2_SUPERCOMPRESSION_BASIS_LZ ? 128u : 120u;
	memset( b, 0, 256u ); memcpy( b, id, sizeof( id ) );
	Put32( b + 12, ( model == 163u || model == 166u ) ? 0u : 37u );
	Put32( b + 16, 1u ); Put32( b + 20, 64u ); Put32( b + 24, 32u );
	Put32( b + 36, 1u ); Put32( b + 40, 1u ); Put32( b + 44, scheme );
	Put32( b + 48, 104u ); Put32( b + 52, 16u );
	if ( scheme == RAL_KTX2_SUPERCOMPRESSION_BASIS_LZ ) {
		Put64( b + 64, 120u ); Put64( b + 72, 8u );
	}
	Put64( b + 80, payload ); Put64( b + 88, 32u );
	Put64( b + 96, scheme == RAL_KTX2_SUPERCOMPRESSION_BASIS_LZ
		? 0u : ( scheme == RAL_KTX2_SUPERCOMPRESSION_NONE ? 32u : 64u ) );
	Put32( b + 104, 16u ); b[116] = (unsigned char)model;
	return payload + 32u;
}
static uint64_t MakeKtx2TwoLevels( unsigned char *b ) {
	(void)MakeKtx2( b, 1u, RAL_KTX2_SUPERCOMPRESSION_NONE );
	Put32( b + 40, 2u ); Put32( b + 48, 128u );
	Put64( b + 80, 144u ); Put64( b + 88, 32u ); Put64( b + 96, 32u );
	Put64( b + 104, 176u ); Put64( b + 112, 16u ); Put64( b + 120, 16u );
	Put32( b + 128, 16u ); b[140] = 1u;
	return 192u;
}

static ralTextureAssetRequest_t Request( void ) {
	ralTextureAssetRequest_t r;
	memset( &r, 0, sizeof( r ) );
	r.schemaVersion = RAL_TEXTURE_ASSET_SCHEMA_VERSION;
	r.assetGeneration = 7u; r.provenanceHash = 0x12345678u;
	r.dimension = RAL_TEXTURE_ASSET_2D;
	r.width = 1024u; r.height = 512u; r.depth = 1u; r.layers = 1u;
	r.sourceMipLevels = 1u; r.colorEncoding = RAL_TEXTURE_ENCODING_SRGB;
	r.channelSemantic = RAL_TEXTURE_CHANNEL_COLOR;
	r.sourceEncoding = RAL_TEXTURE_SOURCE_BASIS_UASTC;
	r.mipPolicy = RAL_TEXTURE_MIPS_GENERATE;
	r.residency = RAL_TEXTURE_RESIDENCY_STREAMED;
	r.hasAlpha = qtrue; r.capabilities.bc = qtrue;
	r.capabilities.astc = qtrue; r.capabilities.etc2 = qtrue;
	r.capabilities.rgba16Float = qtrue;
	r.preferenceCount = 4u;
	r.preferences[0] = RAL_TEXTURE_COMPRESSION_BC;
	r.preferences[1] = RAL_TEXTURE_COMPRESSION_ASTC;
	r.preferences[2] = RAL_TEXTURE_COMPRESSION_ETC2;
	r.preferences[3] = RAL_TEXTURE_COMPRESSION_UNCOMPRESSED;
	r.allowUncompressedFallback = qtrue;
	return r;
}

int main( void ) {
	ralTextureAssetRequest_t request = Request();
	ralTextureAssetReceipt_t receipt, exact, before;
	ralTextureAssetRequest_t artifactRequest, staleRequest;
	ralTextureAssetReceipt_t artifactAsset, staleAsset;
	unsigned char ktx[256], corrupt[256];
	ralKtx2Receipt_t ktxReceipt, ktxExact, ktxBefore;
	uint64_t ktxBytes;
	unsigned char artifact[512], transported[512], artifactBefore[512], payload[96];
	uint64_t artifactLevels[3] = { 64u, 16u, 16u };
	ralTextureArtifactReceipt_t artifactReceipt, decodedArtifact, artifactOutputBefore;
	uint64_t artifactBytes;
	uint32_t p;
	CHECK( Ral_ResolveTextureAsset( &request, &receipt ) );
	CHECK( receipt.assetGeneration == request.assetGeneration
		&& receipt.provenanceHash == request.provenanceHash
		&& receipt.width == request.width && receipt.height == request.height
		&& receipt.colorEncoding == request.colorEncoding
		&& receipt.channelSemantic == request.channelSemantic
		&& receipt.mipPolicy == request.mipPolicy
		&& receipt.residency == request.residency
		&& receipt.targetMipLevels == 11u
		&& receipt.selectedCompression == RAL_TEXTURE_COMPRESSION_BC
		&& receipt.targetFormat == RAL_FORMAT_BC7_SRGB
		&& receipt.transcodeRequired );
	exact = receipt; CHECK( Ral_TextureAssetReceiptExact( &receipt, &exact ) );
	exact.provenanceHash++; CHECK( !Ral_TextureAssetReceiptExact( &receipt, &exact ) );
	exact = receipt; exact.targetFormat = RAL_FORMAT_R8G8B8A8_SRGB;
	CHECK( !Ral_TextureAssetReceiptValid( &exact ) );

	request.capabilities.bc = qfalse;
	CHECK( Ral_ResolveTextureAsset( &request, &receipt )
		&& receipt.targetFormat == RAL_FORMAT_ASTC_4x4_SRGB );
	request.capabilities.astc = qfalse;
	CHECK( Ral_ResolveTextureAsset( &request, &receipt )
		&& receipt.targetFormat == RAL_FORMAT_ETC2_R8G8B8A8_SRGB );
	request.capabilities.etc2 = qfalse; request.preferenceCount = 3u;
	CHECK( Ral_ResolveTextureAsset( &request, &receipt )
		&& receipt.targetFormat == RAL_FORMAT_R8G8B8A8_SRGB
		&& receipt.deterministicFallback );

	request = Request(); request.colorEncoding = RAL_TEXTURE_ENCODING_LINEAR;
	request.channelSemantic = RAL_TEXTURE_CHANNEL_NORMAL_XY;
	CHECK( Ral_ResolveTextureAsset( &request, &receipt )
		&& receipt.targetFormat == RAL_FORMAT_BC5_UNORM );
	request.channelSemantic = RAL_TEXTURE_CHANNEL_ORM;
	CHECK( Ral_ResolveTextureAsset( &request, &receipt )
		&& receipt.targetFormat == RAL_FORMAT_BC7_UNORM );
	request.channelSemantic = RAL_TEXTURE_CHANNEL_HDR_COLOR;
	CHECK( Ral_ResolveTextureAsset( &request, &receipt )
		&& receipt.targetFormat == RAL_FORMAT_BC6H_UFLOAT );
	request.capabilities.bc = qfalse; request.capabilities.astc = qfalse;
	request.capabilities.etc2 = qfalse; request.preferenceCount = 3u;
	CHECK( Ral_ResolveTextureAsset( &request, &receipt )
		&& receipt.targetFormat == RAL_FORMAT_R16G16B16A16_SFLOAT
		&& receipt.deterministicFallback );

	memset( &receipt, 0x5a, sizeof( receipt ) ); before = receipt;
	request.capabilities.rgba16Float = qfalse;
	CHECK( !Ral_ResolveTextureAsset( &request, &receipt )
		&& !memcmp( &receipt, &before, sizeof( receipt ) ) );
	request = Request(); request.channelSemantic = RAL_TEXTURE_CHANNEL_NORMAL_XY;
	CHECK( !Ral_ResolveTextureAsset( &request, &receipt ) );
	request = Request(); request.preferences[1] = request.preferences[0];
	CHECK( !Ral_ResolveTextureAsset( &request, &receipt ) );
	request = Request(); request.dimension = RAL_TEXTURE_ASSET_CUBE;
	request.width = 512u; request.height = 256u; request.layers = 6u;
	CHECK( !Ral_ResolveTextureAsset( &request, &receipt ) );

	ktxBytes = MakeKtx2( ktx, 163u, RAL_KTX2_SUPERCOMPRESSION_BASIS_LZ );
	CHECK( Ral_ParseKtx2( ktx, ktxBytes, 9u, 0xabcdu, &ktxReceipt )
		&& ktxReceipt.dimension == RAL_TEXTURE_ASSET_2D
		&& ktxReceipt.width == 64u && ktxReceipt.height == 32u
		&& ktxReceipt.depth == 1u && ktxReceipt.layers == 1u
		&& ktxReceipt.payloadKind == RAL_KTX2_PAYLOAD_BASIS_ETC1S
		&& ktxReceipt.supercompression == RAL_KTX2_SUPERCOMPRESSION_BASIS_LZ
		&& ktxReceipt.levels[0].byteOffset == 128u
		&& ktxReceipt.levels[0].uncompressedByteLength == 0u );
	ktxExact = ktxReceipt; CHECK( Ral_Ktx2ReceiptExact( &ktxReceipt, &ktxExact ) );
	ktxExact.levels[0].byteLength++;
	CHECK( !Ral_Ktx2ReceiptExact( &ktxReceipt, &ktxExact ) );

	ktxBytes = MakeKtx2( ktx, 166u, RAL_KTX2_SUPERCOMPRESSION_ZSTD );
	CHECK( Ral_ParseKtx2( ktx, ktxBytes, 10u, 0xbcdeu, &ktxReceipt )
		&& ktxReceipt.payloadKind == RAL_KTX2_PAYLOAD_BASIS_UASTC
		&& ktxReceipt.levels[0].uncompressedByteLength == 64u );
	Put64( ktx + 96, 192u );
	Put32( ktx + 36, 6u ); Put32( ktx + 20, 32u );
	CHECK( Ral_ParseKtx2( ktx, ktxBytes, 10u, 0xbcdeu, &ktxReceipt )
		&& ktxReceipt.dimension == RAL_TEXTURE_ASSET_CUBE
		&& ktxReceipt.layers == 6u );
	Put32( ktx + 32, 2u );
	CHECK( Ral_ParseKtx2( ktx, ktxBytes, 10u, 0xbcdeu, &ktxReceipt )
		&& ktxReceipt.layers == 12u );
	Put32( ktx + 36, 1u ); Put32( ktx + 32, 3u );
	CHECK( Ral_ParseKtx2( ktx, ktxBytes, 10u, 0xbcdeu, &ktxReceipt )
		&& ktxReceipt.dimension == RAL_TEXTURE_ASSET_2D_ARRAY
		&& ktxReceipt.layers == 3u );
	Put32( ktx + 32, 0u ); Put32( ktx + 28, 4u );
	CHECK( Ral_ParseKtx2( ktx, ktxBytes, 10u, 0xbcdeu, &ktxReceipt )
		&& ktxReceipt.dimension == RAL_TEXTURE_ASSET_3D
		&& ktxReceipt.depth == 4u );

	ktxBytes = MakeKtx2( ktx, 1u, RAL_KTX2_SUPERCOMPRESSION_NONE );
	Put32( ktx + 40, 0u );
	CHECK( Ral_ParseKtx2( ktx, ktxBytes, 11u, 0xcdefu, &ktxReceipt )
		&& ktxReceipt.payloadKind == RAL_KTX2_PAYLOAD_GPU_FORMAT
		&& ktxReceipt.generateMipmaps && ktxReceipt.levelCount == 1u );
	ktxBytes = MakeKtx2TwoLevels( ktx );
	CHECK( Ral_ParseKtx2( ktx, ktxBytes, 11u, 0xcdefu, &ktxReceipt )
		&& !ktxReceipt.generateMipmaps && ktxReceipt.levelCount == 2u );
	memcpy( corrupt, ktx, sizeof( corrupt ) ); Put64( corrupt + 104, 160u );
	CHECK( !Ral_ParseKtx2( corrupt, ktxBytes, 11u, 0xcdefu, &ktxReceipt ) );

	memset( &ktxBefore, 0x5a, sizeof( ktxBefore ) );
	memcpy( &ktxReceipt, &ktxBefore, sizeof( ktxReceipt ) );
	memcpy( corrupt, ktx, sizeof( corrupt ) ); corrupt[0] = 0u;
	CHECK( !Ral_ParseKtx2( corrupt, ktxBytes, 11u, 0xcdefu, &ktxReceipt )
		&& !memcmp( &ktxReceipt, &ktxBefore, sizeof( ktxReceipt ) ) );
	memcpy( corrupt, ktx, sizeof( corrupt ) ); Put64( corrupt + 80, 104u );
	CHECK( !Ral_ParseKtx2( corrupt, ktxBytes, 11u, 0xcdefu, &ktxReceipt ) );
	memcpy( corrupt, ktx, sizeof( corrupt ) ); Put64( corrupt + 80, UINT64_MAX );
	CHECK( !Ral_ParseKtx2( corrupt, ktxBytes, 11u, 0xcdefu, &ktxReceipt ) );
	memcpy( corrupt, ktx, sizeof( corrupt ) ); Put32( corrupt + 36, 6u );
	CHECK( !Ral_ParseKtx2( corrupt, ktxBytes, 11u, 0xcdefu, &ktxReceipt ) );
	ktxBytes = MakeKtx2( corrupt, 163u, RAL_KTX2_SUPERCOMPRESSION_ZSTD );
	CHECK( !Ral_ParseKtx2( corrupt, ktxBytes, 11u, 0xcdefu, &ktxReceipt ) );

	artifactRequest = Request(); artifactRequest.width = 4u; artifactRequest.height = 4u;
	artifactRequest.sourceMipLevels = 3u; artifactRequest.mipPolicy = RAL_TEXTURE_MIPS_SOURCE;
	CHECK( Ral_ResolveTextureAsset( &artifactRequest, &artifactAsset )
		&& artifactAsset.targetMipLevels == 3u );
	for ( p = 0u; p < sizeof( payload ); p++ ) payload[p] = (unsigned char)( p * 17u + 3u );
	CHECK( Ral_EncodeTextureArtifact( &artifactAsset, artifactLevels, 3u,
		payload, sizeof( payload ), artifact, sizeof( artifact ), &artifactReceipt ) );
	artifactBytes = artifactReceipt.containerByteLength;
	CHECK( artifactReceipt.schemaVersion == RAL_TEXTURE_ARTIFACT_SCHEMA_VERSION
		&& artifactReceipt.levelCount == 3u
		&& artifactReceipt.levels[0].payloadOffset == 0u
		&& artifactReceipt.levels[1].payloadOffset == 64u
		&& artifactReceipt.levels[2].payloadOffset == 80u
		&& artifactReceipt.payloadByteLength == sizeof( payload )
		&& !memcmp( artifact + artifactReceipt.payloadByteOffset,
			payload, sizeof( payload ) ) );
	CHECK( Ral_DecodeTextureArtifact( artifact, artifactBytes,
		&artifactAsset, &decodedArtifact )
		&& Ral_TextureArtifactReceiptExact( &artifactReceipt, &decodedArtifact ) );
	memcpy( transported, artifact, (size_t)artifactBytes );
	CHECK( Ral_DecodeTextureArtifact( transported, artifactBytes,
		&artifactAsset, &decodedArtifact )
		&& decodedArtifact.artifactHash == artifactReceipt.artifactHash );
	memset( artifactBefore, 0xa5, sizeof( artifactBefore ) );
	memcpy( transported, artifactBefore, sizeof( transported ) );
	memset( &artifactOutputBefore, 0x5a, sizeof( artifactOutputBefore ) );
	memcpy( &decodedArtifact, &artifactOutputBefore, sizeof( decodedArtifact ) );
	CHECK( !Ral_EncodeTextureArtifact( &artifactAsset, artifactLevels, 3u,
		payload, sizeof( payload ), transported, 32u, &decodedArtifact )
		&& !memcmp( transported, artifactBefore, sizeof( transported ) )
		&& !memcmp( &decodedArtifact, &artifactOutputBefore, sizeof( decodedArtifact ) ) );

	memcpy( transported, artifact, (size_t)artifactBytes ); transported[artifactBytes - 1u] ^= 1u;
	CHECK( !Ral_DecodeTextureArtifact( transported, artifactBytes,
		&artifactAsset, &decodedArtifact ) );
	CHECK( !Ral_DecodeTextureArtifact( artifact, artifactBytes - 1u,
		&artifactAsset, &decodedArtifact ) );
	memcpy( transported, artifact, (size_t)artifactBytes ); Put32( transported + 8, 2u );
	ResealArtifact( transported, artifactBytes );
	CHECK( !Ral_DecodeTextureArtifact( transported, artifactBytes,
		&artifactAsset, &decodedArtifact ) );
	memcpy( transported, artifact, (size_t)artifactBytes ); Put64( transported + 48,
		artifactAsset.provenanceHash + 1u ); ResealArtifact( transported, artifactBytes );
	CHECK( !Ral_DecodeTextureArtifact( transported, artifactBytes,
		&artifactAsset, &decodedArtifact ) );
	memcpy( transported, artifact, (size_t)artifactBytes ); Put32( transported + 112,
		RAL_FORMAT_UNDEFINED ); ResealArtifact( transported, artifactBytes );
	CHECK( !Ral_DecodeTextureArtifact( transported, artifactBytes,
		&artifactAsset, &decodedArtifact ) );
	memcpy( transported, artifact, (size_t)artifactBytes ); Put64( transported + 160, 63u );
	ResealArtifact( transported, artifactBytes );
	CHECK( !Ral_DecodeTextureArtifact( transported, artifactBytes,
		&artifactAsset, &decodedArtifact ) );
	staleRequest = artifactRequest; staleRequest.capabilities.bc = qfalse;
	CHECK( Ral_ResolveTextureAsset( &staleRequest, &staleAsset )
		&& staleAsset.targetFormat != artifactAsset.targetFormat
		&& !Ral_DecodeTextureArtifact( artifact, artifactBytes,
			&staleAsset, &decodedArtifact ) );
	memcpy( &decodedArtifact, &artifactOutputBefore, sizeof( decodedArtifact ) );
	CHECK( !Ral_DecodeTextureArtifact( artifact, artifactBytes - 1u,
		&artifactAsset, &decodedArtifact )
		&& !memcmp( &decodedArtifact, &artifactOutputBefore, sizeof( decodedArtifact ) ) );
	puts( "RAL texture asset policy: PASS" ); return 0;
}
