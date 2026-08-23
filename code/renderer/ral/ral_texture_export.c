// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "ral_texture_export.h"

#include <limits.h>
#include <string.h>

#define FNV64_OFFSET UINT64_C(14695981039346656037)
#define FNV64_PRIME UINT64_C(1099511628211)

static qboolean ResourceValid( const ralTextureResourceReceipt_t *r ) {
	return r && r->schemaVersion == RAL_TEXTURE_RESOURCE_RECEIPT_SCHEMA_VERSION
		&& r->backendType >= RAL_BACKEND_VULKAN && r->backendType <= RAL_BACKEND_WEBGL2
		&& r->textureIdentity && r->resourceGeneration && r->resourceGeneration != UINT64_MAX
		&& r->type >= RAL_TEXTURE_1D && r->type <= RAL_TEXTURE_CUBE_ARRAY
		&& r->format > RAL_FORMAT_UNDEFINED && r->format < RAL_FORMAT_COUNT
		&& r->width && r->height && r->mipLevels && r->arrayLayers
		&& ( r->imported == qfalse || r->imported == qtrue ) && r->ready == qtrue;
}

static qboolean FormatInfo( ralFormat_t format, ralTextureAspectFlags_t aspect,
		uint32_t *outBytes, qboolean *outSrgb ) {
	uint32_t bytes = 0u; qboolean srgb = qfalse;
	if ( aspect == RAL_TEXTURE_ASPECT_DEPTH ) {
		if ( format == RAL_FORMAT_D16_UNORM ) bytes = 2u;
		else if ( format == RAL_FORMAT_D32_SFLOAT ) bytes = 4u;
		else return qfalse;
	} else if ( aspect == RAL_TEXTURE_ASPECT_COLOR ) {
		switch ( format ) {
		case RAL_FORMAT_R8_UNORM: bytes = 1u; break;
		case RAL_FORMAT_R8G8_UNORM:
		case RAL_FORMAT_R16_UNORM:
		case RAL_FORMAT_R16_SFLOAT: bytes = 2u; break;
		case RAL_FORMAT_R8G8B8A8_SRGB:
		case RAL_FORMAT_B8G8R8A8_SRGB: srgb = qtrue; /* fall through */
		case RAL_FORMAT_R8G8B8A8_UNORM:
		case RAL_FORMAT_B8G8R8A8_UNORM:
		case RAL_FORMAT_A2B10G10R10_UNORM:
		case RAL_FORMAT_A2R10G10B10_UNORM:
		case RAL_FORMAT_R16G16_SFLOAT:
		case RAL_FORMAT_R11G11B10_UFLOAT:
		case RAL_FORMAT_R32_SFLOAT: bytes = 4u; break;
		case RAL_FORMAT_R16G16B16A16_SFLOAT:
		case RAL_FORMAT_R16G16B16A16_UNORM:
		case RAL_FORMAT_R32G32_SFLOAT: bytes = 8u; break;
		case RAL_FORMAT_R32G32B32A32_SFLOAT: bytes = 16u; break;
		default: return qfalse;
		}
	} else return qfalse;
	*outBytes = bytes; *outSrgb = srgb; return qtrue;
}

qboolean Ral_TextureExportReceiptValid( const ralTextureExportReceipt_t *r ) {
	uint32_t bytes, mipDepth;
	qboolean srgb;
	ralTextureUsage_t attachment;
	if ( !r || r->schemaVersion != RAL_TEXTURE_EXPORT_SCHEMA_VERSION
			|| !ResourceValid( &r->resource ) || !r->exportGeneration
			|| r->exportGeneration == UINT64_MAX || !r->volumeDepth
			|| !r->mipLevelCount || r->baseMipLevel >= r->resource.mipLevels
			|| r->mipLevelCount > r->resource.mipLevels - r->baseMipLevel
			|| r->encoding > RAL_TEXTURE_EXPORT_SRGB || r->ready != qtrue
			|| !FormatInfo( r->resource.format, r->aspect, &bytes, &srgb )
			|| ( srgb != ( r->encoding == RAL_TEXTURE_EXPORT_SRGB ) ) ) return qfalse;
	attachment = r->aspect == RAL_TEXTURE_ASPECT_COLOR
		? RAL_TEXTURE_USAGE_COLOR_ATTACHMENT : RAL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT;
	if ( ( r->resource.usage & ( attachment | RAL_TEXTURE_USAGE_TRANSFER_SRC ) )
			!= ( attachment | RAL_TEXTURE_USAGE_TRANSFER_SRC ) ) return qfalse;
	if ( r->resource.type == RAL_TEXTURE_3D ) {
		mipDepth = r->volumeDepth >> r->baseMipLevel; if ( !mipDepth ) mipDepth = 1u;
		return r->resource.arrayLayers == 1u && r->mipLevelCount == 1u
			&& r->baseArrayLayer == 0u && r->arrayLayerCount == 1u
			&& r->depthSliceCount && r->baseDepthSlice < mipDepth
			&& r->depthSliceCount <= mipDepth - r->baseDepthSlice;
	}
	return r->volumeDepth == 1u && r->baseDepthSlice == 0u
		&& r->depthSliceCount == 1u && r->arrayLayerCount
		&& r->baseArrayLayer < r->resource.arrayLayers
		&& r->arrayLayerCount <= r->resource.arrayLayers - r->baseArrayLayer;
}

qboolean Ral_TextureExportBuild( const ralTextureExportCreateInfo_t *ci,
		ralTextureExportReceipt_t *out ) {
	ralTextureExportReceipt_t v;
	if ( !ci || !out ) return qfalse;
	memset( &v, 0, sizeof( v ) );
	v.schemaVersion = RAL_TEXTURE_EXPORT_SCHEMA_VERSION; v.resource = ci->resource;
	v.exportGeneration = ci->exportGeneration; v.volumeDepth = ci->volumeDepth;
	v.aspect = ci->aspect; v.baseMipLevel = ci->baseMipLevel;
	v.mipLevelCount = ci->mipLevelCount; v.baseArrayLayer = ci->baseArrayLayer;
	v.arrayLayerCount = ci->arrayLayerCount; v.baseDepthSlice = ci->baseDepthSlice;
	v.depthSliceCount = ci->depthSliceCount; v.encoding = ci->encoding; v.ready = qtrue;
	if ( !Ral_TextureExportReceiptValid( &v ) ) return qfalse;
	*out = v; return qtrue;
}

qboolean Ral_TextureExportReceiptExact( const ralTextureExportReceipt_t *a,
		const ralTextureExportReceipt_t *b ) {
	return Ral_TextureExportReceiptValid( a ) && Ral_TextureExportReceiptValid( b )
		&& !memcmp( a, b, sizeof( *a ) );
}

static qboolean BuildPlan( const ralTextureReadbackRequest_t *r,
		ralTextureReadbackPlan_t *out ) {
	ralTextureReadbackPlan_t v;
	uint32_t mipWidth, mipHeight, bytes;
	uint64_t tightRow, stagingRow, tightLength, stagingLength;
	qboolean srgb;
	const ralTextureExportReceipt_t *e;
	if ( !r || !out || !r->readbackGeneration || r->readbackGeneration == UINT64_MAX
			|| !Ral_TextureExportReceiptValid( &r->exportReceipt ) ) return qfalse;
	e = &r->exportReceipt;
	if ( r->mipLevel < e->baseMipLevel
			|| r->mipLevel >= e->baseMipLevel + e->mipLevelCount
			|| !r->width || !r->height
			|| !FormatInfo( e->resource.format, e->aspect, &bytes, &srgb ) ) return qfalse;
	mipWidth = e->resource.width >> r->mipLevel; if ( !mipWidth ) mipWidth = 1u;
	mipHeight = e->resource.height >> r->mipLevel; if ( !mipHeight ) mipHeight = 1u;
	if ( r->x >= mipWidth || r->width > mipWidth - r->x || r->y >= mipHeight
			|| r->height > mipHeight - r->y ) return qfalse;
	if ( e->resource.type == RAL_TEXTURE_3D ) {
		if ( r->arrayLayer != 0u || r->depthSlice < e->baseDepthSlice
				|| r->depthSlice >= e->baseDepthSlice + e->depthSliceCount ) return qfalse;
	} else if ( r->depthSlice != 0u || r->arrayLayer < e->baseArrayLayer
			|| r->arrayLayer >= e->baseArrayLayer + e->arrayLayerCount ) return qfalse;
	if ( r->width > UINT64_MAX / bytes ) return qfalse;
	tightRow = (uint64_t)r->width * bytes;
	if ( tightRow > UINT32_MAX || tightRow > UINT64_MAX - 255u ) return qfalse;
	stagingRow = ( tightRow + 255u ) & ~(uint64_t)255u;
	if ( r->height > UINT64_MAX / tightRow || r->height > UINT64_MAX / stagingRow )
		return qfalse;
	tightLength = tightRow * r->height; stagingLength = stagingRow * r->height;
	memset( &v, 0, sizeof( v ) ); v.schemaVersion = RAL_TEXTURE_READBACK_PLAN_SCHEMA_VERSION;
	v.request = *r; v.bytesPerTexel = bytes; v.tightBytesPerRow = (uint32_t)tightRow;
	v.stagingBytesPerRow = (uint32_t)stagingRow; v.rowsPerImage = r->height;
	v.tightByteLength = tightLength; v.stagingByteLength = stagingLength;
	v.transfer.backendType = e->resource.backendType;
	v.transfer.direction = RAL_TRANSFER_READBACK;
	v.transfer.resourceKind = RAL_TRANSFER_TEXTURE;
	v.transfer.resourceIdentity = e->resource.textureIdentity;
	v.transfer.resourceGeneration = e->resource.resourceGeneration;
	v.transfer.byteSize = stagingLength; v.transfer.byteBudget = stagingLength;
	v.transfer.mipLevel = r->mipLevel; v.transfer.arrayLayer = r->arrayLayer;
	v.transfer.offsetX = r->x; v.transfer.offsetY = r->y; v.transfer.offsetZ = r->depthSlice;
	v.transfer.width = r->width; v.transfer.height = r->height; v.transfer.depth = 1u;
	v.transfer.bytesPerRow = (uint32_t)stagingRow; v.transfer.rowsPerImage = r->height;
	v.transfer.textureAspects = e->aspect; v.transfer.queue = RAL_QUEUE_GRAPHICS;
	*out = v; return qtrue;
}

qboolean Ral_TextureReadbackPlanBuild( const ralTextureReadbackRequest_t *r,
		ralTextureReadbackPlan_t *out ) {
	ralTextureReadbackPlan_t v;
	ralTransferReceipt_t prepared;
	if ( !out || !BuildPlan( r, &v )
			|| !Ral_TransferPrepare( &v.transfer, r->readbackGeneration, &prepared ) )
		return qfalse;
	*out = v; return qtrue;
}

qboolean Ral_TextureReadbackPlanValid( const ralTextureReadbackPlan_t *p ) {
	ralTextureReadbackPlan_t expected;
	ralTransferReceipt_t prepared;
	return p && p->schemaVersion == RAL_TEXTURE_READBACK_PLAN_SCHEMA_VERSION
		&& BuildPlan( &p->request, &expected )
		&& !memcmp( p, &expected, sizeof( *p ) )
		&& Ral_TransferPrepare( &p->transfer, p->request.readbackGeneration, &prepared );
}

qboolean Ral_TextureReadbackPlanExact( const ralTextureReadbackPlan_t *a,
		const ralTextureReadbackPlan_t *b ) {
	return Ral_TextureReadbackPlanValid( a ) && Ral_TextureReadbackPlanValid( b )
		&& !memcmp( a, b, sizeof( *a ) );
}

qboolean Ral_TextureReadbackRegionFromPlan( const ralTextureReadbackPlan_t *p,
		ralTextureReadbackRegion_t *out ) {
	ralTextureReadbackRegion_t v;
	if ( !out || !Ral_TextureReadbackPlanValid( p ) ) return qfalse;
	memset( &v, 0, sizeof( v ) ); v.mipLevel = p->request.mipLevel;
	v.arrayLayer = p->request.arrayLayer; v.z = p->request.depthSlice;
	v.aspects = p->request.exportReceipt.aspect; v.x = p->request.x; v.y = p->request.y;
	v.width = p->request.width; v.height = p->request.height;
	*out = v; return qtrue;
}

static qboolean ResultValid( const ralTextureReadbackResult_t *r ) {
	return r && r->schemaVersion == RAL_TEXTURE_READBACK_RESULT_SCHEMA_VERSION
		&& Ral_TextureReadbackPlanValid( &r->plan )
		&& Ral_ReadbackReceiptExact( &r->readback, &r->readback )
		&& r->readback.ready == qtrue
		&& !memcmp( &r->readback.transfer.request, &r->plan.transfer,
			sizeof( r->plan.transfer ) )
		&& r->tightByteLength == r->plan.tightByteLength
		&& r->payloadHash && r->ready == qtrue;
}

qboolean Ral_TextureReadbackResultPublish( const ralTextureReadbackPlan_t *p,
		const ralReadbackReceipt_t *readback, const void *mapped,
		uint64_t mappedLength, ralTextureReadbackResult_t *out ) {
	ralTextureReadbackResult_t v;
	const unsigned char *bytes = (const unsigned char *)mapped;
	uint64_t hash = FNV64_OFFSET;
	uint32_t row, column;
	if ( !out || !bytes || !Ral_TextureReadbackPlanValid( p )
			|| mappedLength != p->stagingByteLength
			|| !Ral_ReadbackReceiptExact( readback, readback ) || !readback->ready
			|| memcmp( &readback->transfer.request, &p->transfer,
				sizeof( p->transfer ) ) ) return qfalse;
	for ( row = 0u; row < p->rowsPerImage; row++ )
		for ( column = 0u; column < p->tightBytesPerRow; column++ )
			hash = ( hash ^ bytes[(uint64_t)row * p->stagingBytesPerRow + column] )
				* FNV64_PRIME;
	memset( &v, 0, sizeof( v ) ); v.schemaVersion = RAL_TEXTURE_READBACK_RESULT_SCHEMA_VERSION;
	v.plan = *p; v.readback = *readback; v.tightByteLength = p->tightByteLength;
	v.payloadHash = hash ? hash : 1u; v.ready = qtrue;
	if ( !ResultValid( &v ) ) return qfalse;
	*out = v; return qtrue;
}

qboolean Ral_TextureReadbackResultExact( const ralTextureReadbackResult_t *a,
		const ralTextureReadbackResult_t *b ) {
	return ResultValid( a ) && ResultValid( b ) && !memcmp( a, b, sizeof( *a ) );
}
