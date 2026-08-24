// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_texture_asset.h"
#include <limits.h>
#include <string.h>

static const unsigned char Ktx2Identifier[12] = {
	0xAB, 0x4B, 0x54, 0x58, 0x20, 0x32, 0x30, 0xBB,
	0x0D, 0x0A, 0x1A, 0x0A
};

static uint32_t ReadLE32( const unsigned char *p ) {
	return (uint32_t)p[0] | (uint32_t)p[1] << 8u
		| (uint32_t)p[2] << 16u | (uint32_t)p[3] << 24u;
}
static uint64_t ReadLE64( const unsigned char *p ) {
	return (uint64_t)ReadLE32( p ) | (uint64_t)ReadLE32( p + 4 ) << 32u;
}
static void WriteLE32( unsigned char *p, uint32_t v ) {
	p[0] = (unsigned char)v; p[1] = (unsigned char)( v >> 8u );
	p[2] = (unsigned char)( v >> 16u ); p[3] = (unsigned char)( v >> 24u );
}
static void WriteLE64( unsigned char *p, uint64_t v ) {
	WriteLE32( p, (uint32_t)v ); WriteLE32( p + 4, (uint32_t)( v >> 32u ) );
}
static qboolean RangeValid( uint64_t offset, uint64_t length, uint64_t total ) {
	return length && offset <= total && length <= total - offset;
}
static qboolean RangesOverlap( uint64_t a, uint64_t an, uint64_t b, uint64_t bn ) {
	return an && bn && a < b + bn && b < a + an;
}

static qboolean BoolValid( qboolean v ) { return v == qfalse || v == qtrue; }
static uint32_t FullMipCount( uint32_t w, uint32_t h, uint32_t d ) {
	uint32_t e = w > h ? w : h, n = 1u;
	if ( d > e ) e = d;
	while ( e > 1u ) { e >>= 1u; n++; }
	return n;
}

static qboolean MetadataValid( const ralTextureAssetRequest_t *r ) {
	uint32_t i, j, maxMips;
	if ( !r || r->schemaVersion != RAL_TEXTURE_ASSET_SCHEMA_VERSION
			|| !r->assetGeneration || r->assetGeneration == UINT64_MAX
			|| !r->provenanceHash || r->dimension > RAL_TEXTURE_ASSET_3D
			|| !r->width || !r->height || !r->depth || !r->layers
			|| r->colorEncoding > RAL_TEXTURE_ENCODING_SRGB
			|| r->channelSemantic > RAL_TEXTURE_CHANNEL_HDR_COLOR
			|| r->sourceEncoding > RAL_TEXTURE_SOURCE_BASIS_UASTC
			|| r->mipPolicy > RAL_TEXTURE_MIPS_NONE
			|| r->residency > RAL_TEXTURE_RESIDENCY_TRANSIENT
			|| !BoolValid( r->hasAlpha ) || !BoolValid( r->allowUncompressedFallback )
			|| !BoolValid( r->capabilities.bc ) || !BoolValid( r->capabilities.astc )
			|| !BoolValid( r->capabilities.etc2 )
			|| !BoolValid( r->capabilities.rgba16Float )
			|| !r->preferenceCount
			|| r->preferenceCount > RAL_TEXTURE_ASSET_MAX_PREFERENCES ) return qfalse;
	if ( r->dimension == RAL_TEXTURE_ASSET_2D && ( r->depth != 1u || r->layers != 1u ) ) return qfalse;
	if ( r->dimension == RAL_TEXTURE_ASSET_2D_ARRAY && r->depth != 1u ) return qfalse;
	if ( r->dimension == RAL_TEXTURE_ASSET_CUBE && ( r->depth != 1u
			|| r->width != r->height || r->layers % 6u ) ) return qfalse;
	if ( r->dimension == RAL_TEXTURE_ASSET_3D && r->layers != 1u ) return qfalse;
	if ( r->colorEncoding == RAL_TEXTURE_ENCODING_SRGB
			&& r->channelSemantic != RAL_TEXTURE_CHANNEL_COLOR ) return qfalse;
	maxMips = FullMipCount( r->width, r->height, r->depth );
	if ( !r->sourceMipLevels || r->sourceMipLevels > maxMips
			|| ( r->mipPolicy == RAL_TEXTURE_MIPS_NONE && r->sourceMipLevels != 1u ) ) return qfalse;
	for ( i = 0u; i < r->preferenceCount; i++ ) {
		if ( r->preferences[i] > RAL_TEXTURE_COMPRESSION_ETC2 ) return qfalse;
		for ( j = 0u; j < i; j++ ) if ( r->preferences[i] == r->preferences[j] ) return qfalse;
	}
	return qtrue;
}

static qboolean FamilySupported( const ralTextureAssetRequest_t *r,
		ralTextureCompressionFamily_t f ) {
	if ( f == RAL_TEXTURE_COMPRESSION_UNCOMPRESSED ) return qtrue;
	if ( f == RAL_TEXTURE_COMPRESSION_BC ) return r->capabilities.bc;
	if ( f == RAL_TEXTURE_COMPRESSION_ASTC ) return r->capabilities.astc;
	return r->capabilities.etc2;
}

static ralFormat_t TargetFormat( ralTextureChannelSemantic_t s,
		ralTextureAssetColorEncoding_t e, ralTextureCompressionFamily_t f ) {
	qboolean srgb = e == RAL_TEXTURE_ENCODING_SRGB;
	if ( s == RAL_TEXTURE_CHANNEL_HDR_COLOR ) {
		if ( f == RAL_TEXTURE_COMPRESSION_BC ) return RAL_FORMAT_BC6H_UFLOAT;
		return f == RAL_TEXTURE_COMPRESSION_UNCOMPRESSED
			? RAL_FORMAT_R16G16B16A16_SFLOAT : RAL_FORMAT_UNDEFINED;
	}
	if ( s == RAL_TEXTURE_CHANNEL_NORMAL_XY ) {
		if ( f == RAL_TEXTURE_COMPRESSION_BC ) return RAL_FORMAT_BC5_UNORM;
		if ( f == RAL_TEXTURE_COMPRESSION_ASTC ) return RAL_FORMAT_ASTC_4x4_UNORM;
		if ( f == RAL_TEXTURE_COMPRESSION_ETC2 ) return RAL_FORMAT_ETC2_R8G8B8A8_UNORM;
		return RAL_FORMAT_R8G8B8A8_UNORM;
	}
	if ( f == RAL_TEXTURE_COMPRESSION_BC ) return srgb ? RAL_FORMAT_BC7_SRGB : RAL_FORMAT_BC7_UNORM;
	if ( f == RAL_TEXTURE_COMPRESSION_ASTC ) return srgb ? RAL_FORMAT_ASTC_4x4_SRGB : RAL_FORMAT_ASTC_4x4_UNORM;
	if ( f == RAL_TEXTURE_COMPRESSION_ETC2 ) return srgb ? RAL_FORMAT_ETC2_R8G8B8A8_SRGB : RAL_FORMAT_ETC2_R8G8B8A8_UNORM;
	return srgb ? RAL_FORMAT_R8G8B8A8_SRGB : RAL_FORMAT_R8G8B8A8_UNORM;
}

qboolean Ral_ResolveTextureAsset( const ralTextureAssetRequest_t *r,
		ralTextureAssetReceipt_t *out ) {
	ralTextureAssetReceipt_t v;
	ralTextureCompressionFamily_t family = RAL_TEXTURE_COMPRESSION_UNCOMPRESSED;
	ralFormat_t format = RAL_FORMAT_UNDEFINED;
	uint32_t i; qboolean fallback = qfalse, sourceMatches;
	if ( !out || !MetadataValid( r ) ) return qfalse;
	for ( i = 0u; i < r->preferenceCount; i++ ) {
		family = r->preferences[i];
		if ( FamilySupported( r, family ) ) format = TargetFormat( r->channelSemantic, r->colorEncoding, family );
		if ( format != RAL_FORMAT_UNDEFINED ) break;
	}
	if ( format == RAL_FORMAT_UNDEFINED && r->allowUncompressedFallback ) {
		family = RAL_TEXTURE_COMPRESSION_UNCOMPRESSED;
		format = TargetFormat( r->channelSemantic, r->colorEncoding, family );
		fallback = qtrue;
	}
	if ( format == RAL_FORMAT_R16G16B16A16_SFLOAT && !r->capabilities.rgba16Float ) format = RAL_FORMAT_UNDEFINED;
	if ( format == RAL_FORMAT_UNDEFINED ) return qfalse;
	memset( &v, 0, sizeof( v ) );
	v.schemaVersion = r->schemaVersion; v.assetGeneration = r->assetGeneration;
	v.provenanceHash = r->provenanceHash; v.dimension = r->dimension;
	v.width = r->width; v.height = r->height; v.depth = r->depth; v.layers = r->layers;
	v.sourceMipLevels = r->sourceMipLevels;
	v.targetMipLevels = r->mipPolicy == RAL_TEXTURE_MIPS_GENERATE
		? FullMipCount( r->width, r->height, r->depth ) : r->sourceMipLevels;
	v.colorEncoding = r->colorEncoding; v.channelSemantic = r->channelSemantic;
	v.sourceEncoding = r->sourceEncoding; v.mipPolicy = r->mipPolicy;
	v.residency = r->residency; v.hasAlpha = r->hasAlpha;
	v.selectedCompression = family; v.targetFormat = format;
	sourceMatches = ( r->sourceEncoding == RAL_TEXTURE_SOURCE_BC && family == RAL_TEXTURE_COMPRESSION_BC )
		|| ( r->sourceEncoding == RAL_TEXTURE_SOURCE_ASTC && family == RAL_TEXTURE_COMPRESSION_ASTC )
		|| ( r->sourceEncoding == RAL_TEXTURE_SOURCE_ETC2 && family == RAL_TEXTURE_COMPRESSION_ETC2 )
		|| ( r->sourceEncoding == RAL_TEXTURE_SOURCE_UNCOMPRESSED && family == RAL_TEXTURE_COMPRESSION_UNCOMPRESSED );
	v.transcodeRequired = sourceMatches ? qfalse : qtrue;
	v.deterministicFallback = fallback;
	if ( !Ral_TextureAssetReceiptValid( &v ) ) return qfalse;
	*out = v; return qtrue;
}

qboolean Ral_TextureAssetReceiptValid( const ralTextureAssetReceipt_t *r ) {
	qboolean sourceMatches;
	if ( !r || ( r->dimension == RAL_TEXTURE_ASSET_2D
			&& ( r->depth != 1u || r->layers != 1u ) )
			|| ( r->dimension == RAL_TEXTURE_ASSET_2D_ARRAY && r->depth != 1u )
			|| ( r->dimension == RAL_TEXTURE_ASSET_CUBE
				&& ( r->depth != 1u || r->width != r->height || r->layers % 6u ) )
			|| ( r->dimension == RAL_TEXTURE_ASSET_3D && r->layers != 1u )
			|| ( r->colorEncoding == RAL_TEXTURE_ENCODING_SRGB
				&& r->channelSemantic != RAL_TEXTURE_CHANNEL_COLOR ) ) return qfalse;
	sourceMatches = ( r->sourceEncoding == RAL_TEXTURE_SOURCE_BC
		&& r->selectedCompression == RAL_TEXTURE_COMPRESSION_BC )
		|| ( r->sourceEncoding == RAL_TEXTURE_SOURCE_ASTC
			&& r->selectedCompression == RAL_TEXTURE_COMPRESSION_ASTC )
		|| ( r->sourceEncoding == RAL_TEXTURE_SOURCE_ETC2
			&& r->selectedCompression == RAL_TEXTURE_COMPRESSION_ETC2 )
		|| ( r->sourceEncoding == RAL_TEXTURE_SOURCE_UNCOMPRESSED
			&& r->selectedCompression == RAL_TEXTURE_COMPRESSION_UNCOMPRESSED );
	return r->schemaVersion == RAL_TEXTURE_ASSET_SCHEMA_VERSION
		&& r->assetGeneration && r->assetGeneration != UINT64_MAX && r->provenanceHash
		&& r->dimension <= RAL_TEXTURE_ASSET_3D && r->width && r->height && r->depth && r->layers
		&& r->sourceMipLevels && r->targetMipLevels
		&& r->targetMipLevels <= FullMipCount( r->width, r->height, r->depth )
		&& r->targetMipLevels == ( r->mipPolicy == RAL_TEXTURE_MIPS_GENERATE
			? FullMipCount( r->width, r->height, r->depth ) : r->sourceMipLevels )
		&& r->colorEncoding <= RAL_TEXTURE_ENCODING_SRGB
		&& r->channelSemantic <= RAL_TEXTURE_CHANNEL_HDR_COLOR
		&& r->sourceEncoding <= RAL_TEXTURE_SOURCE_BASIS_UASTC
		&& r->mipPolicy <= RAL_TEXTURE_MIPS_NONE && r->residency <= RAL_TEXTURE_RESIDENCY_TRANSIENT
		&& BoolValid( r->hasAlpha ) && r->selectedCompression <= RAL_TEXTURE_COMPRESSION_ETC2
		&& r->targetFormat > RAL_FORMAT_UNDEFINED && r->targetFormat < RAL_FORMAT_COUNT
		&& r->targetFormat == TargetFormat( r->channelSemantic,
			r->colorEncoding, r->selectedCompression )
		&& BoolValid( r->transcodeRequired )
		&& r->transcodeRequired == ( sourceMatches ? qfalse : qtrue )
		&& BoolValid( r->deterministicFallback )
		&& ( !r->deterministicFallback
			|| r->selectedCompression == RAL_TEXTURE_COMPRESSION_UNCOMPRESSED );
}

qboolean Ral_TextureAssetReceiptExact( const ralTextureAssetReceipt_t *a,
		const ralTextureAssetReceipt_t *b ) {
	return Ral_TextureAssetReceiptValid( a ) && Ral_TextureAssetReceiptValid( b )
		&& a->schemaVersion == b->schemaVersion
		&& a->assetGeneration == b->assetGeneration
		&& a->provenanceHash == b->provenanceHash
		&& a->dimension == b->dimension && a->width == b->width
		&& a->height == b->height && a->depth == b->depth && a->layers == b->layers
		&& a->sourceMipLevels == b->sourceMipLevels
		&& a->targetMipLevels == b->targetMipLevels
		&& a->colorEncoding == b->colorEncoding
		&& a->channelSemantic == b->channelSemantic
		&& a->sourceEncoding == b->sourceEncoding && a->mipPolicy == b->mipPolicy
		&& a->residency == b->residency && a->hasAlpha == b->hasAlpha
		&& a->selectedCompression == b->selectedCompression
		&& a->targetFormat == b->targetFormat
		&& a->transcodeRequired == b->transcodeRequired
		&& a->deterministicFallback == b->deterministicFallback;
}

qboolean Ral_Ktx2ReceiptValid( const ralKtx2Receipt_t *r ) {
	uint32_t i, j;
	if ( !r || r->schemaVersion != RAL_KTX2_SCHEMA_VERSION
			|| !r->assetGeneration || r->assetGeneration == UINT64_MAX
			|| !r->provenanceHash || r->containerByteLength < 104u
			|| r->dimension > RAL_TEXTURE_ASSET_3D || !r->width || !r->height
			|| !r->depth || !r->layers || !r->levelCount
			|| r->levelCount > RAL_KTX2_MAX_LEVELS
			|| !BoolValid( r->generateMipmaps )
			|| r->payloadKind > RAL_KTX2_PAYLOAD_BASIS_UASTC
			|| r->supercompression > RAL_KTX2_SUPERCOMPRESSION_ZLIB ) return qfalse;
	if ( r->generateMipmaps && ( r->levelCount != 1u
			|| r->payloadKind != RAL_KTX2_PAYLOAD_GPU_FORMAT ) ) return qfalse;
	if ( r->dimension == RAL_TEXTURE_ASSET_2D
			&& ( r->depth != 1u || r->layers != 1u ) ) return qfalse;
	if ( r->dimension == RAL_TEXTURE_ASSET_2D_ARRAY && r->depth != 1u ) return qfalse;
	if ( r->dimension == RAL_TEXTURE_ASSET_CUBE
			&& ( r->depth != 1u || r->width != r->height || r->layers % 6u ) ) return qfalse;
	if ( r->dimension == RAL_TEXTURE_ASSET_3D && r->layers != 1u ) return qfalse;
	if ( r->payloadKind == RAL_KTX2_PAYLOAD_GPU_FORMAT && !r->containerFormatCode ) return qfalse;
	if ( r->payloadKind != RAL_KTX2_PAYLOAD_GPU_FORMAT && r->containerFormatCode ) return qfalse;
	if ( r->payloadKind == RAL_KTX2_PAYLOAD_BASIS_ETC1S
			&& r->supercompression != RAL_KTX2_SUPERCOMPRESSION_BASIS_LZ ) return qfalse;
	if ( r->payloadKind == RAL_KTX2_PAYLOAD_BASIS_UASTC
			&& r->supercompression != RAL_KTX2_SUPERCOMPRESSION_NONE
			&& r->supercompression != RAL_KTX2_SUPERCOMPRESSION_ZSTD ) return qfalse;
	if ( r->payloadKind != RAL_KTX2_PAYLOAD_BASIS_ETC1S
			&& r->supercompression == RAL_KTX2_SUPERCOMPRESSION_BASIS_LZ ) return qfalse;
	for ( i = 0u; i < r->levelCount; i++ ) {
		const ralKtx2LevelReceipt_t *level = &r->levels[i];
		if ( !RangeValid( level->byteOffset, level->byteLength,
				r->containerByteLength ) ) return qfalse;
		if ( r->supercompression == RAL_KTX2_SUPERCOMPRESSION_NONE
				&& level->byteLength != level->uncompressedByteLength ) return qfalse;
		if ( r->supercompression == RAL_KTX2_SUPERCOMPRESSION_BASIS_LZ
				&& level->uncompressedByteLength != 0u ) return qfalse;
		if ( r->supercompression != RAL_KTX2_SUPERCOMPRESSION_NONE
				&& r->supercompression != RAL_KTX2_SUPERCOMPRESSION_BASIS_LZ
				&& !level->uncompressedByteLength ) return qfalse;
		for ( j = 0u; j < i; j++ ) if ( RangesOverlap( level->byteOffset,
			level->byteLength, r->levels[j].byteOffset,
			r->levels[j].byteLength ) ) return qfalse;
	}
	return qtrue;
}

qboolean Ral_ParseKtx2( const void *data, uint64_t byteLength,
		uint64_t assetGeneration, uint64_t provenanceHash,
		ralKtx2Receipt_t *out ) {
	const unsigned char *bytes = (const unsigned char *)data;
	ralKtx2Receipt_t v;
	uint32_t vkFormat, typeSize, pixelWidth, pixelHeight, pixelDepth;
	uint32_t layerCount, faceCount, declaredLevels, levels, scheme;
	uint32_t dfdOffset, dfdLength, kvdOffset, kvdLength, dfdTotal;
	uint64_t sgdOffset, sgdLength, indexLength, imageCount;
	uint32_t colorModel, i, j;
	if ( !bytes || !out || byteLength < 104u || !assetGeneration
			|| assetGeneration == UINT64_MAX || !provenanceHash
			|| memcmp( bytes, Ktx2Identifier, sizeof( Ktx2Identifier ) ) ) return qfalse;
	vkFormat = ReadLE32( bytes + 12 ); typeSize = ReadLE32( bytes + 16 );
	pixelWidth = ReadLE32( bytes + 20 ); pixelHeight = ReadLE32( bytes + 24 );
	pixelDepth = ReadLE32( bytes + 28 ); layerCount = ReadLE32( bytes + 32 );
	faceCount = ReadLE32( bytes + 36 ); declaredLevels = ReadLE32( bytes + 40 );
	scheme = ReadLE32( bytes + 44 ); dfdOffset = ReadLE32( bytes + 48 );
	dfdLength = ReadLE32( bytes + 52 ); kvdOffset = ReadLE32( bytes + 56 );
	kvdLength = ReadLE32( bytes + 60 ); sgdOffset = ReadLE64( bytes + 64 );
	sgdLength = ReadLE64( bytes + 72 );
	levels = declaredLevels ? declaredLevels : 1u;
	if ( !pixelWidth || !pixelHeight || !typeSize || ( typeSize & ( typeSize - 1u ) )
			|| typeSize > 8u || ( faceCount != 1u && faceCount != 6u )
			|| scheme > RAL_KTX2_SUPERCOMPRESSION_ZLIB
			|| levels > RAL_KTX2_MAX_LEVELS ) return qfalse;
	if ( levels > FullMipCount( pixelWidth, pixelHeight,
			pixelDepth ? pixelDepth : 1u ) ) return qfalse;
	indexLength = 80u + (uint64_t)levels * 24u;
	if ( indexLength > byteLength || !RangeValid( dfdOffset, dfdLength, byteLength )
			|| ( dfdOffset & 3u ) || dfdLength < 16u
			|| RangesOverlap( 0u, indexLength, dfdOffset, dfdLength ) ) return qfalse;
	if ( ( kvdLength && ( !RangeValid( kvdOffset, kvdLength, byteLength )
			|| ( kvdOffset & 3u ) ) ) || ( !kvdLength && kvdOffset ) ) return qfalse;
	if ( ( sgdLength && ( !RangeValid( sgdOffset, sgdLength, byteLength )
			|| ( sgdOffset & 7u ) ) ) || ( !sgdLength && sgdOffset ) ) return qfalse;
	if ( RangesOverlap( dfdOffset, dfdLength, kvdOffset, kvdLength )
			|| RangesOverlap( dfdOffset, dfdLength, sgdOffset, sgdLength )
			|| RangesOverlap( kvdOffset, kvdLength, sgdOffset, sgdLength )
			|| RangesOverlap( 0u, indexLength, kvdOffset, kvdLength )
			|| RangesOverlap( 0u, indexLength, sgdOffset, sgdLength ) ) return qfalse;
	dfdTotal = ReadLE32( bytes + dfdOffset );
	if ( dfdTotal != dfdLength ) return qfalse;
	colorModel = bytes[dfdOffset + 12u];
	memset( &v, 0, sizeof( v ) );
	v.schemaVersion = RAL_KTX2_SCHEMA_VERSION; v.assetGeneration = assetGeneration;
	v.provenanceHash = provenanceHash; v.containerByteLength = byteLength;
	v.width = pixelWidth; v.height = pixelHeight; v.depth = pixelDepth ? pixelDepth : 1u;
	if ( pixelDepth ) {
		if ( layerCount || faceCount != 1u ) return qfalse;
		v.dimension = RAL_TEXTURE_ASSET_3D; v.layers = 1u;
	} else if ( faceCount == 6u ) {
		if ( pixelWidth != pixelHeight || layerCount > UINT32_MAX / 6u ) return qfalse;
		v.dimension = RAL_TEXTURE_ASSET_CUBE;
		v.layers = ( layerCount ? layerCount : 1u ) * 6u;
	} else if ( layerCount ) {
		v.dimension = RAL_TEXTURE_ASSET_2D_ARRAY; v.layers = layerCount;
	} else {
		v.dimension = RAL_TEXTURE_ASSET_2D; v.layers = 1u;
	}
	v.levelCount = levels; v.generateMipmaps = declaredLevels ? qfalse : qtrue;
	v.containerFormatCode = vkFormat; v.supercompression = (ralKtx2Supercompression_t)scheme;
	if ( !vkFormat && colorModel == 163u ) v.payloadKind = RAL_KTX2_PAYLOAD_BASIS_ETC1S;
	else if ( !vkFormat && colorModel == 166u ) v.payloadKind = RAL_KTX2_PAYLOAD_BASIS_UASTC;
	else if ( vkFormat ) v.payloadKind = RAL_KTX2_PAYLOAD_GPU_FORMAT;
	else return qfalse;
	if ( v.payloadKind != RAL_KTX2_PAYLOAD_GPU_FORMAT
			&& ( typeSize != 1u || !declaredLevels ) ) return qfalse;
	if ( v.payloadKind == RAL_KTX2_PAYLOAD_BASIS_ETC1S && ( scheme != 1u
			|| !sgdLength ) ) return qfalse;
	if ( v.payloadKind == RAL_KTX2_PAYLOAD_BASIS_UASTC
			&& scheme != 0u && scheme != 2u ) return qfalse;
	if ( v.payloadKind != RAL_KTX2_PAYLOAD_BASIS_ETC1S && scheme == 1u ) return qfalse;
	if ( scheme != 1u && sgdLength ) return qfalse;
	imageCount = (uint64_t)faceCount * ( layerCount ? layerCount : 1u );
	for ( i = 0u; i < levels; i++ ) {
		const unsigned char *entry = bytes + 80u + (uint64_t)i * 24u;
		v.levels[i].byteOffset = ReadLE64( entry );
		v.levels[i].byteLength = ReadLE64( entry + 8u );
		v.levels[i].uncompressedByteLength = ReadLE64( entry + 16u );
		if ( !RangeValid( v.levels[i].byteOffset, v.levels[i].byteLength, byteLength )
				|| ( scheme == 0u && ( v.levels[i].byteOffset & 3u ) )
				|| RangesOverlap( v.levels[i].byteOffset, v.levels[i].byteLength,
					0u, indexLength )
				|| RangesOverlap( v.levels[i].byteOffset, v.levels[i].byteLength,
					dfdOffset, dfdLength )
				|| RangesOverlap( v.levels[i].byteOffset, v.levels[i].byteLength,
					kvdOffset, kvdLength )
				|| RangesOverlap( v.levels[i].byteOffset, v.levels[i].byteLength,
					sgdOffset, sgdLength )
				|| ( scheme == 0u && v.levels[i].byteLength
					!= v.levels[i].uncompressedByteLength )
				|| ( scheme == 1u && v.levels[i].uncompressedByteLength )
				|| ( scheme > 1u && !v.levels[i].uncompressedByteLength )
				|| ( v.levels[i].uncompressedByteLength
					&& v.levels[i].uncompressedByteLength % imageCount ) ) return qfalse;
		for ( j = 0u; j < i; j++ ) if ( RangesOverlap( v.levels[i].byteOffset,
			v.levels[i].byteLength, v.levels[j].byteOffset,
			v.levels[j].byteLength ) ) return qfalse;
	}
	if ( !Ral_Ktx2ReceiptValid( &v ) ) return qfalse;
	*out = v; return qtrue;
}

qboolean Ral_Ktx2ReceiptExact( const ralKtx2Receipt_t *a,
		const ralKtx2Receipt_t *b ) {
	uint32_t i;
	if ( !Ral_Ktx2ReceiptValid( a ) || !Ral_Ktx2ReceiptValid( b )
			|| a->schemaVersion != b->schemaVersion
			|| a->assetGeneration != b->assetGeneration
			|| a->provenanceHash != b->provenanceHash
			|| a->containerByteLength != b->containerByteLength
			|| a->dimension != b->dimension || a->width != b->width
			|| a->height != b->height || a->depth != b->depth
			|| a->layers != b->layers || a->levelCount != b->levelCount
			|| a->generateMipmaps != b->generateMipmaps
			|| a->payloadKind != b->payloadKind
			|| a->supercompression != b->supercompression
			|| a->containerFormatCode != b->containerFormatCode ) return qfalse;
	for ( i = 0u; i < a->levelCount; i++ )
		if ( a->levels[i].byteOffset != b->levels[i].byteOffset
				|| a->levels[i].byteLength != b->levels[i].byteLength
				|| a->levels[i].uncompressedByteLength
					!= b->levels[i].uncompressedByteLength ) return qfalse;
	return qtrue;
}

#define RAL_TEXTURE_ARTIFACT_FIXED_HEADER 144u
#define RAL_TEXTURE_ARTIFACT_HASH_OFFSET 24u
static const unsigned char TextureArtifactMagic[8] = {
	'W', 'R', 'T', 'X', 'A', 'R', 'T', 0
};

static uint64_t TextureArtifactHash( const unsigned char *bytes, uint64_t length ) {
	uint64_t i, hash = UINT64_C( 14695981039346656037 );
	for ( i = 0u; i < length; i++ ) {
		unsigned char value = i >= RAL_TEXTURE_ARTIFACT_HASH_OFFSET
			&& i < RAL_TEXTURE_ARTIFACT_HASH_OFFSET + 8u ? 0u : bytes[i];
		hash ^= value; hash *= UINT64_C( 1099511628211 );
	}
	return hash ? hash : 1u;
}

qboolean Ral_TextureArtifactReceiptValid( const ralTextureArtifactReceipt_t *r ) {
	uint32_t i; uint64_t cursor = 0u, headerBytes;
	if ( !r || r->schemaVersion != RAL_TEXTURE_ARTIFACT_SCHEMA_VERSION
			|| !r->artifactHash || !Ral_TextureAssetReceiptValid( &r->asset )
			|| !r->levelCount || r->levelCount > RAL_KTX2_MAX_LEVELS
			|| r->levelCount != r->asset.targetMipLevels ) return qfalse;
	headerBytes = RAL_TEXTURE_ARTIFACT_FIXED_HEADER
		+ (uint64_t)r->levelCount * 16u;
	if ( r->payloadByteOffset != headerBytes || !r->payloadByteLength
			|| r->payloadByteOffset > UINT64_MAX - r->payloadByteLength
			|| r->containerByteLength
				!= r->payloadByteOffset + r->payloadByteLength ) return qfalse;
	for ( i = 0u; i < r->levelCount; i++ ) {
		if ( !r->levels[i].payloadLength
				|| r->levels[i].payloadOffset != cursor
				|| cursor > UINT64_MAX - r->levels[i].payloadLength ) return qfalse;
		cursor += r->levels[i].payloadLength;
	}
	return cursor == r->payloadByteLength;
}

qboolean Ral_EncodeTextureArtifact( const ralTextureAssetReceipt_t *asset,
		const uint64_t *levelByteLengths, uint32_t levelCount,
		const void *payload, uint64_t payloadByteLength,
		void *outData, uint64_t outCapacity,
		ralTextureArtifactReceipt_t *outReceipt ) {
	ralTextureArtifactReceipt_t v;
	unsigned char *out = (unsigned char *)outData;
	uint64_t headerBytes, totalBytes, cursor = 0u;
	uint32_t i;
	if ( !asset || !levelByteLengths || !payload || !out || !outReceipt
			|| !Ral_TextureAssetReceiptValid( asset ) || !levelCount
			|| levelCount > RAL_KTX2_MAX_LEVELS
			|| levelCount != asset->targetMipLevels || !payloadByteLength ) return qfalse;
	headerBytes = RAL_TEXTURE_ARTIFACT_FIXED_HEADER + (uint64_t)levelCount * 16u;
	if ( headerBytes > UINT64_MAX - payloadByteLength ) return qfalse;
	totalBytes = headerBytes + payloadByteLength;
	if ( totalBytes > outCapacity || totalBytes > (uint64_t)(size_t)-1 ) return qfalse;
	memset( &v, 0, sizeof( v ) );
	v.schemaVersion = RAL_TEXTURE_ARTIFACT_SCHEMA_VERSION;
	v.containerByteLength = totalBytes; v.artifactHash = 1u; v.asset = *asset;
	v.levelCount = levelCount; v.payloadByteOffset = headerBytes;
	v.payloadByteLength = payloadByteLength;
	for ( i = 0u; i < levelCount; i++ ) {
		if ( !levelByteLengths[i] || cursor > UINT64_MAX - levelByteLengths[i] ) return qfalse;
		v.levels[i].payloadOffset = cursor;
		v.levels[i].payloadLength = levelByteLengths[i];
		cursor += levelByteLengths[i];
	}
	if ( cursor != payloadByteLength || !Ral_TextureArtifactReceiptValid( &v ) ) return qfalse;
	memset( out, 0, (size_t)totalBytes );
	memcpy( out, TextureArtifactMagic, sizeof( TextureArtifactMagic ) );
	WriteLE32( out + 8, RAL_TEXTURE_ARTIFACT_SCHEMA_VERSION );
	WriteLE32( out + 12, (uint32_t)headerBytes ); WriteLE64( out + 16, totalBytes );
	WriteLE32( out + 32, asset->schemaVersion );
	WriteLE64( out + 40, asset->assetGeneration ); WriteLE64( out + 48, asset->provenanceHash );
	WriteLE32( out + 56, (uint32_t)asset->dimension ); WriteLE32( out + 60, asset->width );
	WriteLE32( out + 64, asset->height ); WriteLE32( out + 68, asset->depth );
	WriteLE32( out + 72, asset->layers ); WriteLE32( out + 76, asset->sourceMipLevels );
	WriteLE32( out + 80, asset->targetMipLevels );
	WriteLE32( out + 84, (uint32_t)asset->colorEncoding );
	WriteLE32( out + 88, (uint32_t)asset->channelSemantic );
	WriteLE32( out + 92, (uint32_t)asset->sourceEncoding );
	WriteLE32( out + 96, (uint32_t)asset->mipPolicy );
	WriteLE32( out + 100, (uint32_t)asset->residency );
	WriteLE32( out + 104, (uint32_t)asset->hasAlpha );
	WriteLE32( out + 108, (uint32_t)asset->selectedCompression );
	WriteLE32( out + 112, (uint32_t)asset->targetFormat );
	WriteLE32( out + 116, (uint32_t)asset->transcodeRequired );
	WriteLE32( out + 120, (uint32_t)asset->deterministicFallback );
	WriteLE32( out + 124, levelCount ); WriteLE64( out + 128, headerBytes );
	WriteLE64( out + 136, payloadByteLength );
	for ( i = 0u; i < levelCount; i++ ) {
		WriteLE64( out + RAL_TEXTURE_ARTIFACT_FIXED_HEADER + (uint64_t)i * 16u,
			v.levels[i].payloadOffset );
		WriteLE64( out + RAL_TEXTURE_ARTIFACT_FIXED_HEADER + (uint64_t)i * 16u + 8u,
			v.levels[i].payloadLength );
	}
	memcpy( out + headerBytes, payload, (size_t)payloadByteLength );
	v.artifactHash = TextureArtifactHash( out, totalBytes );
	WriteLE64( out + RAL_TEXTURE_ARTIFACT_HASH_OFFSET, v.artifactHash );
	*outReceipt = v; return qtrue;
}

qboolean Ral_DecodeTextureArtifact( const void *data, uint64_t byteLength,
		const ralTextureAssetReceipt_t *expectedAsset,
		ralTextureArtifactReceipt_t *out ) {
	const unsigned char *bytes = (const unsigned char *)data;
	ralTextureArtifactReceipt_t v;
	uint64_t headerBytes, totalBytes, storedHash;
	uint32_t i, levelCount;
	if ( !bytes || !expectedAsset || !out
			|| !Ral_TextureAssetReceiptValid( expectedAsset )
			|| byteLength < RAL_TEXTURE_ARTIFACT_FIXED_HEADER + 16u
			|| byteLength > (uint64_t)(size_t)-1
			|| memcmp( bytes, TextureArtifactMagic, sizeof( TextureArtifactMagic ) )
			|| ReadLE32( bytes + 8 ) != RAL_TEXTURE_ARTIFACT_SCHEMA_VERSION ) return qfalse;
	headerBytes = ReadLE32( bytes + 12 ); totalBytes = ReadLE64( bytes + 16 );
	storedHash = ReadLE64( bytes + RAL_TEXTURE_ARTIFACT_HASH_OFFSET );
	levelCount = ReadLE32( bytes + 124 );
	if ( !levelCount || levelCount > RAL_KTX2_MAX_LEVELS
			|| headerBytes != RAL_TEXTURE_ARTIFACT_FIXED_HEADER
				+ (uint64_t)levelCount * 16u
			|| totalBytes != byteLength || headerBytes >= totalBytes
			|| !storedHash || TextureArtifactHash( bytes, byteLength ) != storedHash ) return qfalse;
	memset( &v, 0, sizeof( v ) );
	v.schemaVersion = ReadLE32( bytes + 8 ); v.containerByteLength = totalBytes;
	v.artifactHash = storedHash; v.asset.schemaVersion = ReadLE32( bytes + 32 );
	v.asset.assetGeneration = ReadLE64( bytes + 40 );
	v.asset.provenanceHash = ReadLE64( bytes + 48 );
	v.asset.dimension = (ralTextureAssetDimension_t)ReadLE32( bytes + 56 );
	v.asset.width = ReadLE32( bytes + 60 ); v.asset.height = ReadLE32( bytes + 64 );
	v.asset.depth = ReadLE32( bytes + 68 ); v.asset.layers = ReadLE32( bytes + 72 );
	v.asset.sourceMipLevels = ReadLE32( bytes + 76 );
	v.asset.targetMipLevels = ReadLE32( bytes + 80 );
	v.asset.colorEncoding = (ralTextureAssetColorEncoding_t)ReadLE32( bytes + 84 );
	v.asset.channelSemantic = (ralTextureChannelSemantic_t)ReadLE32( bytes + 88 );
	v.asset.sourceEncoding = (ralTextureSourceEncoding_t)ReadLE32( bytes + 92 );
	v.asset.mipPolicy = (ralTextureMipPolicy_t)ReadLE32( bytes + 96 );
	v.asset.residency = (ralTextureAssetResidency_t)ReadLE32( bytes + 100 );
	v.asset.hasAlpha = (qboolean)ReadLE32( bytes + 104 );
	v.asset.selectedCompression = (ralTextureCompressionFamily_t)ReadLE32( bytes + 108 );
	v.asset.targetFormat = (ralFormat_t)ReadLE32( bytes + 112 );
	v.asset.transcodeRequired = (qboolean)ReadLE32( bytes + 116 );
	v.asset.deterministicFallback = (qboolean)ReadLE32( bytes + 120 );
	v.levelCount = levelCount; v.payloadByteOffset = ReadLE64( bytes + 128 );
	v.payloadByteLength = ReadLE64( bytes + 136 );
	for ( i = 0u; i < levelCount; i++ ) {
		v.levels[i].payloadOffset = ReadLE64( bytes
			+ RAL_TEXTURE_ARTIFACT_FIXED_HEADER + (uint64_t)i * 16u );
		v.levels[i].payloadLength = ReadLE64( bytes
			+ RAL_TEXTURE_ARTIFACT_FIXED_HEADER + (uint64_t)i * 16u + 8u );
	}
	if ( !Ral_TextureArtifactReceiptValid( &v )
			|| !Ral_TextureAssetReceiptExact( &v.asset, expectedAsset ) ) return qfalse;
	*out = v; return qtrue;
}

qboolean Ral_TextureArtifactReceiptExact( const ralTextureArtifactReceipt_t *a,
		const ralTextureArtifactReceipt_t *b ) {
	uint32_t i;
	if ( !Ral_TextureArtifactReceiptValid( a )
			|| !Ral_TextureArtifactReceiptValid( b )
			|| a->schemaVersion != b->schemaVersion
			|| a->containerByteLength != b->containerByteLength
			|| a->artifactHash != b->artifactHash
			|| !Ral_TextureAssetReceiptExact( &a->asset, &b->asset )
			|| a->levelCount != b->levelCount
			|| a->payloadByteOffset != b->payloadByteOffset
			|| a->payloadByteLength != b->payloadByteLength ) return qfalse;
	for ( i = 0u; i < a->levelCount; i++ )
		if ( a->levels[i].payloadOffset != b->levels[i].payloadOffset
				|| a->levels[i].payloadLength != b->levels[i].payloadLength ) return qfalse;
	return qtrue;
}
