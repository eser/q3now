// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef WIRED_RAL_TEXTURE_ASSET_H
#define WIRED_RAL_TEXTURE_ASSET_H

#include "ral_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RAL_TEXTURE_ASSET_SCHEMA_VERSION 1u
#define RAL_TEXTURE_ASSET_MAX_PREFERENCES 4u
#define RAL_KTX2_SCHEMA_VERSION 1u
#define RAL_KTX2_MAX_LEVELS 32u

typedef enum { RAL_TEXTURE_ASSET_2D = 0, RAL_TEXTURE_ASSET_2D_ARRAY,
	RAL_TEXTURE_ASSET_CUBE, RAL_TEXTURE_ASSET_3D } ralTextureAssetDimension_t;
typedef enum { RAL_TEXTURE_ENCODING_LINEAR = 0,
	RAL_TEXTURE_ENCODING_SRGB } ralTextureAssetColorEncoding_t;
typedef enum { RAL_TEXTURE_CHANNEL_COLOR = 0, RAL_TEXTURE_CHANNEL_NORMAL_XY,
	RAL_TEXTURE_CHANNEL_ORM, RAL_TEXTURE_CHANNEL_DATA,
	RAL_TEXTURE_CHANNEL_HDR_COLOR } ralTextureChannelSemantic_t;
typedef enum { RAL_TEXTURE_SOURCE_UNCOMPRESSED = 0, RAL_TEXTURE_SOURCE_BC,
	RAL_TEXTURE_SOURCE_ASTC, RAL_TEXTURE_SOURCE_ETC2,
	RAL_TEXTURE_SOURCE_BASIS_ETC1S,
	RAL_TEXTURE_SOURCE_BASIS_UASTC } ralTextureSourceEncoding_t;
typedef enum { RAL_TEXTURE_COMPRESSION_UNCOMPRESSED = 0,
	RAL_TEXTURE_COMPRESSION_BC, RAL_TEXTURE_COMPRESSION_ASTC,
	RAL_TEXTURE_COMPRESSION_ETC2 } ralTextureCompressionFamily_t;
typedef enum { RAL_TEXTURE_MIPS_SOURCE = 0, RAL_TEXTURE_MIPS_GENERATE,
	RAL_TEXTURE_MIPS_NONE } ralTextureMipPolicy_t;
typedef enum { RAL_TEXTURE_RESIDENCY_PINNED = 0,
	RAL_TEXTURE_RESIDENCY_STREAMED,
	RAL_TEXTURE_RESIDENCY_TRANSIENT } ralTextureAssetResidency_t;
typedef enum { RAL_KTX2_PAYLOAD_GPU_FORMAT = 0,
	RAL_KTX2_PAYLOAD_BASIS_ETC1S,
	RAL_KTX2_PAYLOAD_BASIS_UASTC } ralKtx2PayloadKind_t;
typedef enum { RAL_KTX2_SUPERCOMPRESSION_NONE = 0,
	RAL_KTX2_SUPERCOMPRESSION_BASIS_LZ,
	RAL_KTX2_SUPERCOMPRESSION_ZSTD,
	RAL_KTX2_SUPERCOMPRESSION_ZLIB } ralKtx2Supercompression_t;

typedef struct {
	uint64_t byteOffset, byteLength, uncompressedByteLength;
} ralKtx2LevelReceipt_t;

typedef struct {
	uint32_t schemaVersion;
	uint64_t assetGeneration, provenanceHash, containerByteLength;
	ralTextureAssetDimension_t dimension;
	uint32_t width, height, depth, layers, levelCount;
	qboolean generateMipmaps;
	ralKtx2PayloadKind_t payloadKind;
	ralKtx2Supercompression_t supercompression;
	/* Exact numeric field carried by the KTX2 container. It is provenance,
	 * not a runtime backend format or permission to upload. */
	uint32_t containerFormatCode;
	ralKtx2LevelReceipt_t levels[RAL_KTX2_MAX_LEVELS];
} ralKtx2Receipt_t;

typedef struct { qboolean bc, astc, etc2, rgba16Float; }
	ralTextureAssetCapabilities_t;

typedef struct {
	uint32_t schemaVersion;
	uint64_t assetGeneration, provenanceHash;
	ralTextureAssetDimension_t dimension;
	uint32_t width, height, depth, layers, sourceMipLevels;
	ralTextureAssetColorEncoding_t colorEncoding;
	ralTextureChannelSemantic_t channelSemantic;
	ralTextureSourceEncoding_t sourceEncoding;
	ralTextureMipPolicy_t mipPolicy;
	ralTextureAssetResidency_t residency;
	qboolean hasAlpha;
	ralTextureAssetCapabilities_t capabilities;
	uint32_t preferenceCount;
	ralTextureCompressionFamily_t preferences[RAL_TEXTURE_ASSET_MAX_PREFERENCES];
	qboolean allowUncompressedFallback;
} ralTextureAssetRequest_t;

typedef struct {
	uint32_t schemaVersion;
	uint64_t assetGeneration, provenanceHash;
	ralTextureAssetDimension_t dimension;
	uint32_t width, height, depth, layers, sourceMipLevels, targetMipLevels;
	ralTextureAssetColorEncoding_t colorEncoding;
	ralTextureChannelSemantic_t channelSemantic;
	ralTextureSourceEncoding_t sourceEncoding;
	ralTextureMipPolicy_t mipPolicy;
	ralTextureAssetResidency_t residency;
	qboolean hasAlpha;
	ralTextureCompressionFamily_t selectedCompression;
	ralFormat_t targetFormat;
	qboolean transcodeRequired, deterministicFallback;
} ralTextureAssetReceipt_t;

qboolean Ral_ResolveTextureAsset( const ralTextureAssetRequest_t *request,
	ralTextureAssetReceipt_t *outReceipt );
qboolean Ral_TextureAssetReceiptValid( const ralTextureAssetReceipt_t *receipt );
qboolean Ral_TextureAssetReceiptExact( const ralTextureAssetReceipt_t *a,
	const ralTextureAssetReceipt_t *b );
qboolean Ral_ParseKtx2( const void *bytes, uint64_t byteLength,
	uint64_t assetGeneration, uint64_t provenanceHash,
	ralKtx2Receipt_t *outReceipt );
qboolean Ral_Ktx2ReceiptValid( const ralKtx2Receipt_t *receipt );
qboolean Ral_Ktx2ReceiptExact( const ralKtx2Receipt_t *a,
	const ralKtx2Receipt_t *b );

#ifdef __cplusplus
}
#endif
#endif
