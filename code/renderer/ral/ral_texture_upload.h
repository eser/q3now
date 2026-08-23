// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef WIRED_RAL_TEXTURE_UPLOAD_H
#define WIRED_RAL_TEXTURE_UPLOAD_H

#include "ral_command.h"
#include "ral_texture_asset.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RAL_TEXTURE_UPLOAD_PLAN_SCHEMA_VERSION 1u
#define RAL_TEXTURE_UPLOAD_ALIGNMENT 256u

typedef struct {
	uint64_t artifactOffset, artifactLength;
	uint64_t stagingOffset, stagingLength;
	uint32_t tightBytesPerRow, blockRows, imageCount;
	ralBufferTextureCopy_t region;
} ralTextureUploadLevelPlan_t;

typedef struct {
	uint32_t schemaVersion;
	uint64_t artifactHash, artifactByteLength;
	ralTextureAssetReceipt_t asset;
	ralTextureCreateInfo_t texture;
	uint32_t levelCount;
	uint64_t stagingByteLength;
	ralTextureUploadLevelPlan_t levels[RAL_KTX2_MAX_LEVELS];
} ralTextureUploadPlan_t;

qboolean Ral_BuildTextureArtifactUploadPlan( const void *artifactBytes,
	uint64_t artifactByteLength, const ralTextureAssetReceipt_t *expectedAsset,
	ralTextureUploadPlan_t *outPlan );
qboolean Ral_TextureUploadPlanValid( const ralTextureUploadPlan_t *plan );
qboolean Ral_TextureUploadPlanExact( const ralTextureUploadPlan_t *a,
	const ralTextureUploadPlan_t *b );
/* Repack tight artifact rows into the plan's WebGPU-compatible staging layout.
 * Output remains untouched on any validation/capacity failure. */
qboolean Ral_PackTextureArtifactUpload( const void *artifactBytes,
	uint64_t artifactByteLength, const ralTextureUploadPlan_t *plan,
	void *stagingBytes, uint64_t stagingCapacity );

#ifdef __cplusplus
}
#endif
#endif
