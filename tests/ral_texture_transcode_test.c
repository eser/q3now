// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_texture_transcode.h"
#include "fixtures/ral_texture_basis_fixtures.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if ( !(x) ) { fprintf( stderr, "FAIL %s:%d: %s\n", \
	__FILE__, __LINE__, #x ); return 1; } } while ( 0 )

static int Base64Value( unsigned char c ) {
	if ( c >= 'A' && c <= 'Z' ) return c - 'A';
	if ( c >= 'a' && c <= 'z' ) return c - 'a' + 26;
	if ( c >= '0' && c <= '9' ) return c - '0' + 52;
	if ( c == '+' ) return 62;
	if ( c == '/' ) return 63;
	return -1;
}

static size_t DecodeBase64( const char *text, unsigned char *out,
		size_t capacity ) {
	size_t i = 0u, n = 0u, length = strlen( text );
	while ( i < length ) {
		int a, b, c, d;
		if ( i + 4u > length || ( a = Base64Value( (unsigned char)text[i] ) ) < 0
				|| ( b = Base64Value( (unsigned char)text[i + 1u] ) ) < 0 ) return 0u;
		c = text[i + 2u] == '=' ? -2 : Base64Value( (unsigned char)text[i + 2u] );
		d = text[i + 3u] == '=' ? -2 : Base64Value( (unsigned char)text[i + 3u] );
		if ( c < -2 || d < -2 || ( c == -2 && d != -2 ) || n >= capacity ) return 0u;
		out[n++] = (unsigned char)( ( a << 2 ) | ( b >> 4 ) );
		if ( c != -2 ) {
			if ( c < 0 || n >= capacity ) return 0u;
			out[n++] = (unsigned char)( ( b << 4 ) | ( c >> 2 ) );
			if ( d != -2 ) {
				if ( d < 0 || n >= capacity ) return 0u;
				out[n++] = (unsigned char)( ( c << 6 ) | d );
			}
		}
		i += 4u;
	}
	return n;
}

static size_t MakeEtc1sFixture( unsigned char *out, size_t capacity ) {
	return DecodeBase64( RalEtc1sFixtureBase64, out, capacity );
}

static size_t MakeUastcFixture( unsigned char *out, size_t capacity ) {
	size_t i, n = DecodeBase64( RalUastcFixturePrefixBase64, out, capacity );
	if ( n != 272u || capacity - n < 256u * sizeof( RalUastcFixtureBlock ) ) return 0u;
	for ( i = 0u; i < 256u; i++ ) {
		memcpy( out + n, RalUastcFixtureBlock, sizeof( RalUastcFixtureBlock ) );
		n += sizeof( RalUastcFixtureBlock );
	}
	return n;
}

static int ResolveTarget( const ralKtx2Receipt_t *source,
		ralTextureSourceEncoding_t encoding, qboolean compressed,
		ralTextureAssetReceipt_t *out ) {
	ralTextureAssetRequest_t request;
	memset( &request, 0, sizeof( request ) );
	request.schemaVersion = RAL_TEXTURE_ASSET_SCHEMA_VERSION;
	request.assetGeneration = source->assetGeneration;
	request.provenanceHash = source->provenanceHash;
	request.dimension = source->dimension;
	request.width = source->width; request.height = source->height;
	request.depth = source->depth; request.layers = source->layers;
	request.sourceMipLevels = source->levelCount;
	request.colorEncoding = RAL_TEXTURE_ENCODING_SRGB;
	request.channelSemantic = RAL_TEXTURE_CHANNEL_COLOR;
	request.sourceEncoding = encoding;
	request.mipPolicy = RAL_TEXTURE_MIPS_SOURCE;
	request.residency = RAL_TEXTURE_RESIDENCY_STREAMED;
	request.hasAlpha = qfalse;
	request.capabilities.bc = compressed;
	request.preferenceCount = 1u;
	request.preferences[0] = RAL_TEXTURE_COMPRESSION_BC;
	request.allowUncompressedFallback = qtrue;
	return Ral_ResolveTextureAsset( &request, out );
}

static int TestFixture( unsigned char *ktx, size_t ktxBytes,
		ralKtx2PayloadKind_t payloadKind, ralTextureSourceEncoding_t sourceEncoding,
		qboolean compressed, uint64_t expectedPayloadBytes ) {
	unsigned char artifactA[20000], artifactB[20000];
	ralKtx2Receipt_t source;
	ralTextureAssetReceipt_t target;
	ralTextureArtifactReceipt_t encodedA, encodedB, decoded;
	CHECK( Ral_ParseKtx2( ktx, ktxBytes, 17u, 0xABCDEFu, &source ) );
	CHECK( source.payloadKind == payloadKind );
	CHECK( ResolveTarget( &source, sourceEncoding, compressed, &target ) );
	CHECK( target.targetFormat == ( compressed ? RAL_FORMAT_BC7_SRGB
		: RAL_FORMAT_R8G8B8A8_SRGB ) );
	CHECK( target.deterministicFallback == ( compressed ? qfalse : qtrue ) );
	memset( artifactA, 0xA5, sizeof( artifactA ) );
	memset( artifactB, 0x5A, sizeof( artifactB ) );
	CHECK( Ral_TranscodeKtx2ToArtifact( ktx, ktxBytes, &source, &target,
		artifactA, sizeof( artifactA ), &encodedA ) );
	CHECK( encodedA.payloadByteLength == expectedPayloadBytes );
	CHECK( Ral_DecodeTextureArtifact( artifactA, encodedA.containerByteLength,
		&target, &decoded ) );
	CHECK( Ral_TextureArtifactReceiptExact( &encodedA, &decoded ) );
	CHECK( Ral_TranscodeKtx2ToArtifact( ktx, ktxBytes, &source, &target,
		artifactB, sizeof( artifactB ), &encodedB ) );
	CHECK( Ral_TextureArtifactReceiptExact( &encodedA, &encodedB ) );
	CHECK( !memcmp( artifactA, artifactB, (size_t)encodedA.containerByteLength ) );
	return 0;
}

static int HostileFailures( unsigned char *ktx, size_t ktxBytes ) {
	unsigned char output[20000], before[20000];
	ralKtx2Receipt_t source, stale;
	ralTextureAssetReceipt_t target, unsupported;
	ralTextureArtifactReceipt_t receipt;
	CHECK( Ral_ParseKtx2( ktx, ktxBytes, 23u, 0x123456u, &source ) );
	CHECK( ResolveTarget( &source, RAL_TEXTURE_SOURCE_BASIS_ETC1S, qtrue, &target ) );
	memset( output, 0xCC, sizeof( output ) ); memcpy( before, output, sizeof( output ) );
	CHECK( !Ral_TranscodeKtx2ToArtifact( ktx, ktxBytes, &source, &target,
		output, 16u, &receipt ) );
	CHECK( !memcmp( output, before, sizeof( output ) ) );
	stale = source; stale.width++;
	CHECK( !Ral_TranscodeKtx2ToArtifact( ktx, ktxBytes, &stale, &target,
		output, sizeof( output ), &receipt ) );
	CHECK( !memcmp( output, before, sizeof( output ) ) );
	unsupported = target;
	unsupported.channelSemantic = RAL_TEXTURE_CHANNEL_HDR_COLOR;
	unsupported.colorEncoding = RAL_TEXTURE_ENCODING_LINEAR;
	unsupported.targetFormat = RAL_FORMAT_BC6H_UFLOAT;
	CHECK( Ral_TextureAssetReceiptValid( &unsupported ) );
	CHECK( !Ral_TranscodeKtx2ToArtifact( ktx, ktxBytes, &source, &unsupported,
		output, sizeof( output ), &receipt ) );
	CHECK( !memcmp( output, before, sizeof( output ) ) );
	ktx[0] ^= 1u;
	CHECK( !Ral_TranscodeKtx2ToArtifact( ktx, ktxBytes, &source, &target,
		output, sizeof( output ), &receipt ) );
	CHECK( !memcmp( output, before, sizeof( output ) ) );
	ktx[0] ^= 1u;
	return 0;
}

int main( void ) {
	unsigned char etc1s[512], uastc[4400];
	size_t etc1sBytes = MakeEtc1sFixture( etc1s, sizeof( etc1s ) );
	size_t uastcBytes = MakeUastcFixture( uastc, sizeof( uastc ) );
	CHECK( etc1sBytes == 418u ); CHECK( uastcBytes == 4368u );
	CHECK( TestFixture( etc1s, etc1sBytes, RAL_KTX2_PAYLOAD_BASIS_ETC1S,
		RAL_TEXTURE_SOURCE_BASIS_ETC1S, qtrue, 4096u ) == 0 );
	CHECK( TestFixture( uastc, uastcBytes, RAL_KTX2_PAYLOAD_BASIS_UASTC,
		RAL_TEXTURE_SOURCE_BASIS_UASTC, qfalse, 16384u ) == 0 );
	CHECK( HostileFailures( etc1s, etc1sBytes ) == 0 );
	puts( "ral_texture_transcode_test: ok" );
	return 0;
}
