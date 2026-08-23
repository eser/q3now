// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_texture_upload.h"

#include <limits.h>
#include <string.h>

typedef struct { uint32_t width, height, bytes; } ralTextureBlock_t;

static qboolean Mul64( uint64_t a, uint64_t b, uint64_t *out ) {
	if ( !out || ( a && b > UINT64_MAX / a ) ) return qfalse;
	*out = a * b; return qtrue;
}

static qboolean Align256( uint64_t value, uint64_t *out ) {
	if ( !out || value > UINT64_MAX - ( RAL_TEXTURE_UPLOAD_ALIGNMENT - 1u ) )
		return qfalse;
	*out = ( value + RAL_TEXTURE_UPLOAD_ALIGNMENT - 1u )
		& ~(uint64_t)( RAL_TEXTURE_UPLOAD_ALIGNMENT - 1u );
	return qtrue;
}

static qboolean FormatBlock( ralFormat_t format, ralTextureBlock_t *out ) {
	ralTextureBlock_t v = { 1u, 1u, 0u };
	if ( !out ) return qfalse;
	switch ( format ) {
	case RAL_FORMAT_R8G8B8A8_UNORM:
	case RAL_FORMAT_R8G8B8A8_SRGB: v.bytes = 4u; break;
	case RAL_FORMAT_R16G16B16A16_SFLOAT: v.bytes = 8u; break;
	case RAL_FORMAT_BC5_UNORM:
	case RAL_FORMAT_BC6H_UFLOAT:
	case RAL_FORMAT_BC7_UNORM:
	case RAL_FORMAT_BC7_SRGB:
	case RAL_FORMAT_ASTC_4x4_UNORM:
	case RAL_FORMAT_ASTC_4x4_SRGB:
	case RAL_FORMAT_ETC2_R8G8B8A8_UNORM:
	case RAL_FORMAT_ETC2_R8G8B8A8_SRGB:
		v.width = v.height = 4u; v.bytes = 16u; break;
	default: return qfalse;
	}
	*out = v; return qtrue;
}

static qboolean TextureFacts( const ralTextureAssetReceipt_t *asset,
		ralTextureCreateInfo_t *out ) {
	ralTextureCreateInfo_t v;
	if ( !asset || !out ) return qfalse;
	memset( &v, 0, sizeof( v ) );
	switch ( asset->dimension ) {
	case RAL_TEXTURE_ASSET_2D: v.type = RAL_TEXTURE_2D; break;
	case RAL_TEXTURE_ASSET_2D_ARRAY: v.type = RAL_TEXTURE_2D_ARRAY; break;
	case RAL_TEXTURE_ASSET_CUBE:
		v.type = asset->layers == 6u ? RAL_TEXTURE_CUBE : RAL_TEXTURE_CUBE_ARRAY;
		break;
	case RAL_TEXTURE_ASSET_3D: v.type = RAL_TEXTURE_3D; break;
	default: return qfalse;
	}
	v.format = asset->targetFormat; v.width = asset->width; v.height = asset->height;
	v.depthOrArrayLayers = asset->dimension == RAL_TEXTURE_ASSET_3D
		? asset->depth : asset->layers;
	v.mipLevels = asset->targetMipLevels; v.sampleCount = 1u;
	v.usage = (ralTextureUsage_t)( RAL_TEXTURE_USAGE_SAMPLED
		| RAL_TEXTURE_USAGE_TRANSFER_DST | RAL_TEXTURE_USAGE_TRANSFER_SRC );
	v.memory = RAL_MEMORY_DEVICE_LOCAL; v.concurrentGraphicsTransfer = qtrue;
	*out = v; return qtrue;
}

static qboolean BuildLevel( const ralTextureAssetReceipt_t *asset,
		const ralTextureArtifactReceipt_t *artifact, uint32_t level,
		uint64_t stagingCursor, ralTextureUploadLevelPlan_t *out,
		uint64_t *nextCursor ) {
	ralTextureBlock_t block;
	ralTextureUploadLevelPlan_t v;
	uint64_t blocksWide, blockRows, tightRow, imageBytes, tightLength;
	uint64_t stagingRow, stagingImage, lastImage, stagingLength;
	uint32_t width = asset->width >> level, height = asset->height >> level;
	uint32_t depth = asset->depth >> level, images;
	if ( !width ) width = 1u;
	if ( !height ) height = 1u;
	if ( !depth ) depth = 1u;
	if ( !FormatBlock( asset->targetFormat, &block ) ) return qfalse;
	blocksWide = ( width + block.width - 1u ) / block.width;
	blockRows = ( height + block.height - 1u ) / block.height;
	images = asset->dimension == RAL_TEXTURE_ASSET_3D ? depth : asset->layers;
	if ( !Mul64( blocksWide, block.bytes, &tightRow )
			|| !Mul64( tightRow, blockRows, &imageBytes )
			|| !Mul64( imageBytes, images, &tightLength )
			|| tightLength != artifact->levels[level].payloadLength
			|| tightRow > UINT32_MAX || blockRows > UINT32_MAX ) return qfalse;
	if ( !Align256( stagingCursor, &stagingCursor )
			|| !Align256( tightRow, &stagingRow ) || stagingRow > UINT32_MAX
			|| !Mul64( stagingRow, blockRows, &stagingImage )
			|| !Mul64( stagingImage, images - 1u, &lastImage )
			|| !Mul64( stagingRow, blockRows - 1u, &stagingLength )
			|| lastImage > UINT64_MAX - stagingLength
			|| lastImage + stagingLength > UINT64_MAX - tightRow ) return qfalse;
	stagingLength = lastImage + stagingLength + tightRow;
	if ( stagingCursor > UINT64_MAX - stagingLength ) return qfalse;
	memset( &v, 0, sizeof( v ) );
	v.artifactOffset = artifact->payloadByteOffset
		+ artifact->levels[level].payloadOffset;
	v.artifactLength = tightLength; v.stagingOffset = stagingCursor;
	v.stagingLength = stagingLength; v.tightBytesPerRow = (uint32_t)tightRow;
	v.blockRows = (uint32_t)blockRows; v.imageCount = images;
	v.region.bufferOffset = stagingCursor; v.region.bytesPerRow = (uint32_t)stagingRow;
	v.region.rowsPerImage = height; v.region.mipLevel = level;
	v.region.aspects = RAL_TEXTURE_ASPECT_COLOR;
	v.region.imageRect.width = width; v.region.imageRect.height = height;
	if ( asset->dimension == RAL_TEXTURE_ASSET_3D ) v.region.imageDepth = depth;
	else v.region.arrayLayerCount = images;
	*out = v; *nextCursor = stagingCursor + stagingLength; return qtrue;
}

qboolean Ral_TextureUploadPlanValid( const ralTextureUploadPlan_t *p ) {
	ralTextureArtifactReceipt_t artifact;
	ralTextureCreateInfo_t texture;
	uint64_t cursor = 0u;
	uint32_t i;
	if ( !p || p->schemaVersion != RAL_TEXTURE_UPLOAD_PLAN_SCHEMA_VERSION
			|| !p->artifactHash || !p->artifactByteLength
			|| !Ral_TextureAssetReceiptValid( &p->asset )
			|| !TextureFacts( &p->asset, &texture )
			|| p->texture.type != texture.type || p->texture.format != texture.format
			|| p->texture.width != texture.width || p->texture.height != texture.height
			|| p->texture.depthOrArrayLayers != texture.depthOrArrayLayers
			|| p->texture.mipLevels != texture.mipLevels
			|| p->texture.sampleCount != texture.sampleCount
			|| p->texture.usage != texture.usage || p->texture.memory != texture.memory
			|| p->texture.debugName || p->texture.concurrentGraphicsCompute
			|| p->texture.concurrentGraphicsTransfer != qtrue
			|| p->levelCount != p->asset.targetMipLevels
			|| !p->levelCount || p->levelCount > RAL_KTX2_MAX_LEVELS ) return qfalse;
	memset( &artifact, 0, sizeof( artifact ) );
	artifact.schemaVersion = RAL_TEXTURE_ARTIFACT_SCHEMA_VERSION;
	artifact.containerByteLength = p->artifactByteLength;
	artifact.artifactHash = p->artifactHash; artifact.asset = p->asset;
	artifact.levelCount = p->levelCount;
	artifact.payloadByteOffset = p->levels[0].artifactOffset;
	for ( i = 0u; i < p->levelCount; i++ ) {
		ralTextureUploadLevelPlan_t expected;
		uint64_t next;
		artifact.levels[i].payloadOffset = i ? artifact.levels[i - 1u].payloadOffset
			+ artifact.levels[i - 1u].payloadLength : 0u;
		artifact.levels[i].payloadLength = p->levels[i].artifactLength;
		artifact.payloadByteLength += p->levels[i].artifactLength;
		if ( !BuildLevel( &p->asset, &artifact, i, cursor, &expected, &next )
				|| memcmp( &expected, &p->levels[i], sizeof( expected ) ) ) return qfalse;
		cursor = next;
	}
	return cursor == p->stagingByteLength
		&& artifact.payloadByteOffset + artifact.payloadByteLength
			== artifact.containerByteLength;
}

qboolean Ral_BuildTextureArtifactUploadPlan( const void *bytes, uint64_t length,
		const ralTextureAssetReceipt_t *expectedAsset,
		ralTextureUploadPlan_t *out ) {
	ralTextureArtifactReceipt_t artifact;
	ralTextureUploadPlan_t v;
	uint64_t cursor = 0u;
	uint32_t i;
	if ( !bytes || !expectedAsset || !out
			|| !Ral_DecodeTextureArtifact( bytes, length, expectedAsset, &artifact ) )
		return qfalse;
	memset( &v, 0, sizeof( v ) );
	v.schemaVersion = RAL_TEXTURE_UPLOAD_PLAN_SCHEMA_VERSION;
	v.artifactHash = artifact.artifactHash; v.artifactByteLength = length;
	v.asset = artifact.asset; v.levelCount = artifact.levelCount;
	if ( !TextureFacts( &v.asset, &v.texture ) ) return qfalse;
	for ( i = 0u; i < v.levelCount; i++ )
		if ( !BuildLevel( &v.asset, &artifact, i, cursor, &v.levels[i], &cursor ) )
			return qfalse;
	v.stagingByteLength = cursor;
	if ( !Ral_TextureUploadPlanValid( &v ) ) return qfalse;
	*out = v; return qtrue;
}

qboolean Ral_TextureUploadPlanExact( const ralTextureUploadPlan_t *a,
		const ralTextureUploadPlan_t *b ) {
	return Ral_TextureUploadPlanValid( a ) && Ral_TextureUploadPlanValid( b )
		&& !memcmp( a, b, sizeof( *a ) );
}

qboolean Ral_PackTextureArtifactUpload( const void *artifactBytes,
		uint64_t artifactByteLength, const ralTextureUploadPlan_t *plan,
		void *stagingBytes, uint64_t stagingCapacity ) {
	ralTextureUploadPlan_t exact;
	const unsigned char *source = (const unsigned char *)artifactBytes;
	unsigned char *staging = (unsigned char *)stagingBytes;
	uint32_t level;
	if ( !source || !plan || !staging || stagingCapacity < plan->stagingByteLength
			|| plan->stagingByteLength > (uint64_t)SIZE_MAX
			|| !Ral_TextureUploadPlanValid( plan )
			|| !Ral_BuildTextureArtifactUploadPlan( source, artifactByteLength,
				&plan->asset, &exact ) || !Ral_TextureUploadPlanExact( plan, &exact ) )
		return qfalse;
	memset( staging, 0, (size_t)plan->stagingByteLength );
	for ( level = 0u; level < plan->levelCount; level++ ) {
		const ralTextureUploadLevelPlan_t *p = &plan->levels[level];
		uint32_t image, row;
		uint64_t sourceCursor = p->artifactOffset;
		uint64_t stagingImageStride = (uint64_t)p->region.bytesPerRow * p->blockRows;
		for ( image = 0u; image < p->imageCount; image++ )
			for ( row = 0u; row < p->blockRows; row++ ) {
				memcpy( staging + p->stagingOffset + (uint64_t)image * stagingImageStride
					+ (uint64_t)row * p->region.bytesPerRow,
					source + sourceCursor, p->tightBytesPerRow );
				sourceCursor += p->tightBytesPerRow;
			}
	}
	return qtrue;
}
