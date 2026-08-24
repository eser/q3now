// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#ifndef WIRED_RAL_TEXTURE_EXPORT_H
#define WIRED_RAL_TEXTURE_EXPORT_H

#include "ral_readback.h"
#include "ral_resource.h"
#include "ral_transition.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RAL_TEXTURE_EXPORT_SCHEMA_VERSION 1u
#define RAL_TEXTURE_READBACK_PLAN_SCHEMA_VERSION 1u
#define RAL_TEXTURE_READBACK_RESULT_SCHEMA_VERSION 1u

typedef enum {
	RAL_TEXTURE_EXPORT_LINEAR = 0,
	RAL_TEXTURE_EXPORT_SRGB
} ralTextureExportEncoding_t;

typedef struct {
	ralTextureResourceReceipt_t resource;
	uint64_t exportGeneration;
	uint32_t volumeDepth;
	ralTextureAspectFlags_t aspect;
	uint32_t baseMipLevel, mipLevelCount;
	uint32_t baseArrayLayer, arrayLayerCount;
	uint32_t baseDepthSlice, depthSliceCount;
	ralTextureExportEncoding_t encoding;
} ralTextureExportCreateInfo_t;

typedef struct {
	uint32_t schemaVersion;
	ralTextureResourceReceipt_t resource;
	uint64_t exportGeneration;
	uint32_t volumeDepth;
	ralTextureAspectFlags_t aspect;
	uint32_t baseMipLevel, mipLevelCount;
	uint32_t baseArrayLayer, arrayLayerCount;
	uint32_t baseDepthSlice, depthSliceCount;
	ralTextureExportEncoding_t encoding;
	qboolean ready;
} ralTextureExportReceipt_t;

typedef struct {
	ralTextureExportReceipt_t exportReceipt;
	uint64_t readbackGeneration;
	uint32_t mipLevel, arrayLayer, depthSlice;
	uint32_t x, y, width, height;
} ralTextureReadbackRequest_t;

typedef struct {
	uint32_t schemaVersion;
	ralTextureReadbackRequest_t request;
	uint32_t bytesPerTexel;
	uint32_t tightBytesPerRow;
	uint32_t stagingBytesPerRow;
	uint32_t rowsPerImage;
	uint64_t tightByteLength;
	uint64_t stagingByteLength;
	ralTransferRequest_t transfer;
} ralTextureReadbackPlan_t;

typedef struct {
	uint32_t schemaVersion;
	ralTextureReadbackPlan_t plan;
	ralReadbackReceipt_t readback;
	uint64_t tightByteLength;
	uint64_t payloadHash;
	qboolean ready;
} ralTextureReadbackResult_t;

qboolean Ral_TextureExportBuild( const ralTextureExportCreateInfo_t *createInfo,
	ralTextureExportReceipt_t *outReceipt );
qboolean Ral_TextureExportReceiptValid( const ralTextureExportReceipt_t *receipt );
qboolean Ral_TextureExportReceiptExact( const ralTextureExportReceipt_t *a,
	const ralTextureExportReceipt_t *b );
qboolean Ral_TextureReadbackPlanBuild( const ralTextureReadbackRequest_t *request,
	ralTextureReadbackPlan_t *outPlan );
qboolean Ral_TextureReadbackPlanValid( const ralTextureReadbackPlan_t *plan );
qboolean Ral_TextureReadbackPlanExact( const ralTextureReadbackPlan_t *a,
	const ralTextureReadbackPlan_t *b );
qboolean Ral_TextureReadbackRegionFromPlan( const ralTextureReadbackPlan_t *plan,
	ralTextureReadbackRegion_t *outRegion );
qboolean Ral_TextureReadbackResultPublish( const ralTextureReadbackPlan_t *plan,
	const ralReadbackReceipt_t *completedReadback, const void *mappedBytes,
	uint64_t mappedByteLength, ralTextureReadbackResult_t *outResult );
qboolean Ral_TextureReadbackResultExact( const ralTextureReadbackResult_t *a,
	const ralTextureReadbackResult_t *b );

#ifdef __cplusplus
}
#endif
#endif
