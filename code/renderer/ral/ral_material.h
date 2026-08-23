// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#ifndef WIRED_RAL_MATERIAL_H
#define WIRED_RAL_MATERIAL_H

#include "ral_texture_cache.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RAL_MATERIAL_SCHEMA_VERSION 1u
#define RAL_MATERIAL_RECEIPT_SCHEMA_VERSION 1u
#define RAL_MATERIAL_MAX_TEXTURES 12u

typedef enum { RAL_MATERIAL_SOURCE_Q3 = 1, RAL_MATERIAL_SOURCE_PBR,
	RAL_MATERIAL_SOURCE_GLTF } ralMaterialSource_t;
typedef enum {
	RAL_MATERIAL_TEXTURE_BASE_COLOR = 1,
	RAL_MATERIAL_TEXTURE_NORMAL,
	RAL_MATERIAL_TEXTURE_ORM,
	RAL_MATERIAL_TEXTURE_OCCLUSION,
	RAL_MATERIAL_TEXTURE_METALLIC_ROUGHNESS,
	RAL_MATERIAL_TEXTURE_EMISSIVE,
	RAL_MATERIAL_TEXTURE_LIGHTMAP,
	RAL_MATERIAL_TEXTURE_ENVIRONMENT,
	RAL_MATERIAL_TEXTURE_LEGACY_STAGE0,
	RAL_MATERIAL_TEXTURE_LEGACY_STAGE1
} ralMaterialTextureRole_t;
typedef enum {
	RAL_MATERIAL_CHANNEL_RGBA = 1,
	RAL_MATERIAL_CHANNEL_XY_NORMAL,
	RAL_MATERIAL_CHANNEL_RGB_ORM,
	RAL_MATERIAL_CHANNEL_R_OCCLUSION,
	RAL_MATERIAL_CHANNEL_GB_ROUGHNESS_METALLIC,
	RAL_MATERIAL_CHANNEL_RGB_EMISSIVE,
	RAL_MATERIAL_CHANNEL_LEGACY_RGBA
} ralMaterialChannelConvention_t;
typedef enum { RAL_MATERIAL_ALPHA_OPAQUE = 0, RAL_MATERIAL_ALPHA_MASK,
	RAL_MATERIAL_ALPHA_BLEND, RAL_MATERIAL_ALPHA_ADDITIVE } ralMaterialAlphaMode_t;

typedef struct {
	ralMaterialTextureRole_t role;
	ralMaterialChannelConvention_t channels;
	uint32_t uvSet;
	ralTextureCacheKey_t texture;
} ralMaterialTexture_t;

typedef struct {
	float baseColor[4];
	float emissive[3];
	float metallic, roughness, normalScale, occlusionStrength;
} ralMaterialFactors_t;

typedef struct {
	ralMaterialAlphaMode_t alphaMode;
	float alphaCutoff;
	qboolean doubleSided;
	qboolean depthTest;
	qboolean depthWrite;
} ralMaterialRenderPolicy_t;

typedef struct {
	uint32_t schemaVersion;
	uint64_t materialGeneration, provenanceHash;
	ralMaterialSource_t source;
	uint32_t textureCount;
	ralMaterialTexture_t textures[RAL_MATERIAL_MAX_TEXTURES];
	ralMaterialFactors_t factors;
	ralMaterialRenderPolicy_t renderPolicy;
} ralMaterialDescription_t;

typedef struct {
	uint32_t schemaVersion;
	ralMaterialDescription_t material;
	uint64_t variantKey;
	uint64_t artifactHash;
	qboolean ready;
} ralMaterialReceipt_t;

qboolean Ral_MaterialCompile( const ralMaterialDescription_t *description,
	ralMaterialReceipt_t *outReceipt );
qboolean Ral_MaterialReceiptValid( const ralMaterialReceipt_t *receipt );
qboolean Ral_MaterialReceiptExact( const ralMaterialReceipt_t *a,
	const ralMaterialReceipt_t *b );

#ifdef __cplusplus
}
#endif
#endif
