// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_texture_transcode.h"

#include <ktx.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static qboolean SourceMatchesTarget( const ralKtx2Receipt_t *source,
		const ralTextureAssetReceipt_t *target ) {
	ralTextureSourceEncoding_t encoding;
	if ( source->payloadKind == RAL_KTX2_PAYLOAD_BASIS_ETC1S )
		encoding = RAL_TEXTURE_SOURCE_BASIS_ETC1S;
	else if ( source->payloadKind == RAL_KTX2_PAYLOAD_BASIS_UASTC )
		encoding = RAL_TEXTURE_SOURCE_BASIS_UASTC;
	else return qfalse;
	return source->assetGeneration == target->assetGeneration
		&& source->provenanceHash == target->provenanceHash
		&& source->dimension == target->dimension
		&& source->width == target->width && source->height == target->height
		&& source->depth == target->depth && source->layers == target->layers
		&& source->levelCount == target->sourceMipLevels
		&& source->levelCount == target->targetMipLevels
		&& !source->generateMipmaps && target->mipPolicy != RAL_TEXTURE_MIPS_GENERATE
		&& target->sourceEncoding == encoding && target->transcodeRequired;
}

static qboolean MapTarget( ralFormat_t format, ktx_transcode_fmt_e *out ) {
	if ( !out ) return qfalse;
	switch ( format ) {
	case RAL_FORMAT_BC5_UNORM: *out = KTX_TTF_BC5_RG; return qtrue;
	case RAL_FORMAT_BC7_UNORM:
	case RAL_FORMAT_BC7_SRGB: *out = KTX_TTF_BC7_RGBA; return qtrue;
	case RAL_FORMAT_ASTC_4x4_UNORM:
	case RAL_FORMAT_ASTC_4x4_SRGB: *out = KTX_TTF_ASTC_4x4_RGBA; return qtrue;
	case RAL_FORMAT_ETC2_R8G8B8A8_UNORM:
	case RAL_FORMAT_ETC2_R8G8B8A8_SRGB: *out = KTX_TTF_ETC2_RGBA; return qtrue;
	case RAL_FORMAT_R8G8B8A8_UNORM:
	case RAL_FORMAT_R8G8B8A8_SRGB: *out = KTX_TTF_RGBA32; return qtrue;
	default: return qfalse;
	}
}

static qboolean TextureMatchesReceipt( const ktxTexture2 *texture,
		const ralKtx2Receipt_t *source ) {
	const ktxTexture *base = (const ktxTexture *)texture;
	uint32_t layers = base->numLayers;
	if ( base->isCubemap ) {
		if ( layers > UINT32_MAX / 6u ) return qfalse;
		layers *= 6u;
	}
	return base->baseWidth == source->width && base->baseHeight == source->height
		&& base->baseDepth == source->depth && base->numLevels == source->levelCount
		&& layers == source->layers
		&& !!base->isCubemap == ( source->dimension == RAL_TEXTURE_ASSET_CUBE )
		&& !!base->isArray == ( source->dimension == RAL_TEXTURE_ASSET_2D_ARRAY
			|| ( source->dimension == RAL_TEXTURE_ASSET_CUBE
				&& source->layers > 6u ) );
}

qboolean Ral_TranscodeKtx2ToArtifact( const void *ktxBytes,
		uint64_t ktxByteLength, const ralKtx2Receipt_t *source,
		const ralTextureAssetReceipt_t *target, void *artifactBytes,
		uint64_t artifactCapacity, ralTextureArtifactReceipt_t *outReceipt ) {
	ralKtx2Receipt_t parsed;
	ktxTexture2 *texture = NULL;
	ktxTexture *base;
	ktx_transcode_fmt_e transcodeFormat;
	uint64_t levelBytes[RAL_KTX2_MAX_LEVELS], payloadBytes = 0u, cursor = 0u;
	unsigned char *payload = NULL;
	uint32_t level;
	qboolean ok = qfalse;
	if ( !ktxBytes || !source || !target || !artifactBytes || !outReceipt
			|| ktxByteLength > (uint64_t)SIZE_MAX
			|| !Ral_Ktx2ReceiptValid( source )
			|| !Ral_TextureAssetReceiptValid( target )
			|| !SourceMatchesTarget( source, target ) || !MapTarget( target->targetFormat,
				&transcodeFormat )
			|| !Ral_ParseKtx2( ktxBytes, ktxByteLength, source->assetGeneration,
				source->provenanceHash, &parsed )
			|| !Ral_Ktx2ReceiptExact( &parsed, source ) ) return qfalse;
	if ( ktxTexture2_CreateFromMemory( (const ktx_uint8_t *)ktxBytes,
			(ktx_size_t)ktxByteLength, KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT,
			&texture ) != KTX_SUCCESS || !texture ) return qfalse;
	base = ktxTexture( texture );
	if ( !TextureMatchesReceipt( texture, source )
			|| !ktxTexture_NeedsTranscoding( base )
			|| ktxTexture2_TranscodeBasis( texture, transcodeFormat, 0u ) != KTX_SUCCESS
			|| ktxTexture_NeedsTranscoding( base ) ) goto done;
	for ( level = 0u; level < source->levelCount; level++ ) {
		ktx_size_t offset, bytes = ktxTexture_GetLevelSize( base, level );
		if ( !bytes || ktxTexture_GetImageOffset( base, level, 0u, 0u, &offset )
				!= KTX_SUCCESS || (uint64_t)offset > (uint64_t)base->dataSize
				|| (uint64_t)bytes > (uint64_t)base->dataSize - (uint64_t)offset
				|| payloadBytes > UINT64_MAX - (uint64_t)bytes ) goto done;
		levelBytes[level] = (uint64_t)bytes;
		payloadBytes += (uint64_t)bytes;
	}
	if ( !payloadBytes || payloadBytes > (uint64_t)SIZE_MAX ) goto done;
	payload = (unsigned char *)malloc( (size_t)payloadBytes );
	if ( !payload ) goto done;
	for ( level = 0u; level < source->levelCount; level++ ) {
		ktx_size_t offset;
		if ( ktxTexture_GetImageOffset( base, level, 0u, 0u, &offset )
				!= KTX_SUCCESS ) goto done;
		memcpy( payload + cursor, base->pData + offset, (size_t)levelBytes[level] );
		cursor += levelBytes[level];
	}
	ok = Ral_EncodeTextureArtifact( target, levelBytes, source->levelCount,
		payload, payloadBytes, artifactBytes, artifactCapacity, outReceipt );
done:
	free( payload );
	ktxTexture_Destroy( ktxTexture( texture ) );
	return ok;
}
