// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "ral_metal_lighting.h"
#include "ral_metal_internal.h"

#include <stdlib.h>
#include <string.h>

struct ralMetalLighting_s {
	id<MTLTexture> planes[RAL_LIGHTING_RUNTIME_MAX_PLANES];
	ralMetalLightingReceipt_t receipt;
};

static qboolean PixelFormat( ralFormat_t format, MTLPixelFormat *outFormat )
{
	if ( !outFormat )
		return qfalse;
	switch ( format ) {
	case RAL_FORMAT_E5B9G9R9_UFLOAT: *outFormat = MTLPixelFormatRGB9E5Float; return qtrue;
	case RAL_FORMAT_R8G8_UNORM: *outFormat = MTLPixelFormatRG8Unorm; return qtrue;
	case RAL_FORMAT_R8_UNORM: *outFormat = MTLPixelFormatR8Unorm; return qtrue;
	case RAL_FORMAT_R16G16_SNORM: *outFormat = MTLPixelFormatRG16Snorm; return qtrue;
	case RAL_FORMAT_R16G16B16A16_SFLOAT: *outFormat = MTLPixelFormatRGBA16Float; return qtrue;
	default: return qfalse;
	}
}

static qboolean ReceiptValid( const ralMetalLightingReceipt_t *receipt )
{
	uint32_t plane;
	if ( !receipt || receipt->schemaVersion != RAL_METAL_LIGHTING_SCHEMA_VERSION || !receipt->coreGeneration ||
		 receipt->plan.backendType != RAL_BACKEND_METAL || !Ral_LightingRuntimePlanValid( &receipt->plan ) ||
		 !receipt->uploadHash || receipt->ready != qtrue )
		return qfalse;
	for ( plane = 0u; plane < receipt->plan.planeCount; ++plane )
		if ( !receipt->textureIdentities[plane] )
			return qfalse;
	for ( ; plane < RAL_LIGHTING_RUNTIME_MAX_PLANES; ++plane )
		if ( receipt->textureIdentities[plane] )
			return qfalse;
	return qtrue;
}

qboolean RalMetal_LightingReceiptExact( const ralMetalLightingReceipt_t *a,
	const ralMetalLightingReceipt_t *b )
{
	return ReceiptValid( a ) && ReceiptValid( b ) && !memcmp( a, b, sizeof( *a ) ) ? qtrue : qfalse;
}

qboolean RalMetal_LightingUpload( ralMetalCore_t *core,
	const ralMetalCoreReceipt_t *coreReceipt, const void *artifactMemory,
	uint64_t artifactByteLength, const ralLightingRuntimePlan_t *plan,
	ralMetalLighting_t **outLighting, ralMetalLightingReceipt_t *outReceipt )
{
	const uint8_t *artifactBytes = (const uint8_t *)artifactMemory;
	ralLightingArtifactReceipt_t artifact;
	ralLightingPayloadView_t views[RAL_LIGHTING_ARTIFACT_MAX_PAYLOADS];
	ralStaticLightingCapabilities_t capabilities = { qtrue, qtrue, qtrue, qtrue };
	ralLightingRuntimePlan_t exactPlan;
	ralMetalLighting_t *lighting;
	uint32_t plane;
	if ( !core || !coreReceipt || !RalMetal_CoreMatchesReceipt( core, coreReceipt ) || !artifactBytes ||
		 !artifactByteLength || !plan || plan->backendType != RAL_BACKEND_METAL || !outLighting || !outReceipt ||
		 !Ral_LightingArtifactRead( artifactBytes, artifactByteLength, &artifact, views ) ||
		 !Ral_LightingRuntimePlanBuild( RAL_BACKEND_METAL, plan->frameGeneration, artifactBytes, artifactByteLength,
										 &artifact, &capabilities, &exactPlan ) ||
		 !Ral_LightingRuntimePlanExact( plan, &exactPlan ) )
		return qfalse;
	lighting = (ralMetalLighting_t *)calloc( 1u, sizeof( *lighting ) );
	if ( !lighting )
		return qfalse;
	lighting->receipt.schemaVersion = RAL_METAL_LIGHTING_SCHEMA_VERSION;
	lighting->receipt.coreGeneration = coreReceipt->generation;
	lighting->receipt.plan = exactPlan;
	@autoreleasepool {
		id<MTLDevice> device = RalMetal_CoreNativeDevice( core );
		for ( plane = 0u; plane < exactPlan.planeCount; ++plane ) {
			const ralLightingRuntimePlanePlan_t *planePlan = &exactPlan.planes[plane];
			MTLPixelFormat pixelFormat;
			MTLTextureDescriptor *descriptor;
			uint32_t layer;
			if ( !PixelFormat( planePlan->texture.format, &pixelFormat ) ||
				 !RalMetal_TextureFormatSupportsFeatures( core, planePlan->texture.format,
					 (ralTextureFormatFeatures_t)( RAL_TEXTURE_FORMAT_FEATURE_SAMPLED |
											 RAL_TEXTURE_FORMAT_FEATURE_FILTER_LINEAR |
											 RAL_TEXTURE_FORMAT_FEATURE_TRANSFER_DST ) ) )
				goto fail;
			descriptor = [[[MTLTextureDescriptor alloc] init] autorelease];
			descriptor.textureType = planePlan->texture.type == RAL_TEXTURE_2D_ARRAY ? MTLTextureType2DArray : MTLTextureType2D;
			descriptor.pixelFormat = pixelFormat;
			descriptor.width = planePlan->texture.width;
			descriptor.height = planePlan->texture.height;
			descriptor.arrayLength = planePlan->texture.depthOrArrayLayers;
			descriptor.mipmapLevelCount = 1u;
			descriptor.sampleCount = 1u;
			descriptor.storageMode = MTLStorageModeShared;
			descriptor.usage = MTLTextureUsageShaderRead;
			lighting->planes[plane] = [device newTextureWithDescriptor:descriptor];
			if ( !lighting->planes[plane] )
				goto fail;
			for ( layer = 0u; layer < planePlan->texture.depthOrArrayLayers; ++layer ) {
				const uint8_t *source = artifactBytes + planePlan->artifactOffset +
					(uint64_t)layer * planePlan->tightBytesPerRow * planePlan->rowsPerImage;
				[lighting->planes[plane] replaceRegion:MTLRegionMake2D( 0u, 0u, planePlan->texture.width,
					planePlan->texture.height ) mipmapLevel:0u slice:layer withBytes:source
					bytesPerRow:planePlan->tightBytesPerRow
					bytesPerImage:(NSUInteger)planePlan->tightBytesPerRow * planePlan->rowsPerImage];
			}
			lighting->receipt.textureIdentities[plane] = (uintptr_t)(void *)lighting->planes[plane];
		}
	}
	lighting->receipt.uploadHash = exactPlan.manifestHash;
	for ( plane = 0u; plane < exactPlan.planeCount; ++plane )
		lighting->receipt.uploadHash ^= exactPlan.planes[plane].payloadHash +
			( UINT64_C( 0x9e3779b97f4a7c15 ) << ( plane & 1u ) );
	if ( !lighting->receipt.uploadHash )
		lighting->receipt.uploadHash = 1u;
	lighting->receipt.ready = qtrue;
	if ( !ReceiptValid( &lighting->receipt ) )
		goto fail;
	*outLighting = lighting;
	*outReceipt = lighting->receipt;
	return qtrue;
fail:
	for ( plane = 0u; plane < exactPlan.planeCount; ++plane )
		[lighting->planes[plane] release];
	free( lighting );
	return qfalse;
}

qboolean RalMetal_LightingDestroy( ralMetalCore_t *core,
	const ralMetalCoreReceipt_t *coreReceipt, ralMetalLighting_t *lighting,
	const ralMetalLightingReceipt_t *authority )
{
	uint32_t plane;
	if ( !core || !coreReceipt || !lighting || !authority || !RalMetal_CoreMatchesReceipt( core, coreReceipt ) ||
		 !RalMetal_LightingReceiptExact( &lighting->receipt, authority ) )
		return qfalse;
	for ( plane = 0u; plane < lighting->receipt.plan.planeCount; ++plane )
		[lighting->planes[plane] release];
	memset( lighting, 0, sizeof( *lighting ) );
	free( lighting );
	return qtrue;
}
