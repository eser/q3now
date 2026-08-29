// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "ral_vulkan_lighting.h"

#include <stdlib.h>
#include <string.h>

struct ralVulkanLighting_s {
	ralTexture_t *planes[RAL_LIGHTING_RUNTIME_MAX_PLANES];
	ralVulkanLightingReceipt_t receipt;
};

static qboolean ReceiptValid( const ralVulkanLightingReceipt_t *receipt )
{
	uint32_t plane;
	if ( !receipt || receipt->schemaVersion != RAL_VULKAN_LIGHTING_SCHEMA_VERSION ||
		 receipt->plan.backendType != RAL_BACKEND_VULKAN || !Ral_LightingRuntimePlanValid( &receipt->plan ) ||
		 !receipt->uploadHash || receipt->ready != qtrue )
		return qfalse;
	for ( plane = 0u; plane < receipt->plan.planeCount; ++plane )
		if ( !Ral_TextureResourceReceiptExact( &receipt->resources[plane], &receipt->resources[plane] ) )
			return qfalse;
	for ( ; plane < RAL_LIGHTING_RUNTIME_MAX_PLANES; ++plane )
		if ( memcmp( &receipt->resources[plane], &(ralTextureResourceReceipt_t){ 0 },
					 sizeof( receipt->resources[plane] ) ) )
			return qfalse;
	return qtrue;
}

qboolean RalVulkan_LightingReceiptExact( const ralVulkanLightingReceipt_t *a,
	const ralVulkanLightingReceipt_t *b )
{
	return ReceiptValid( a ) && ReceiptValid( b ) && !memcmp( a, b, sizeof( *a ) ) ? qtrue : qfalse;
}

qboolean RalVulkan_LightingUpload( ralBackend_t *backend,
	const void *artifactMemory, uint64_t artifactByteLength,
	const ralLightingRuntimePlan_t *plan, ralVulkanLighting_t **outLighting,
	ralVulkanLightingReceipt_t *outReceipt )
{
	const uint8_t *artifactBytes = (const uint8_t *)artifactMemory;
	ralLightingArtifactReceipt_t artifact;
	ralLightingPayloadView_t views[RAL_LIGHTING_ARTIFACT_MAX_PAYLOADS];
	ralStaticLightingCapabilities_t capabilities = { qtrue, qtrue, qtrue, qtrue };
	ralLightingRuntimePlan_t exactPlan;
	ralVulkanLighting_t *lighting;
	uint32_t plane;
	if ( !backend || !artifactBytes || !artifactByteLength ||
		 !plan || plan->backendType != RAL_BACKEND_VULKAN || !outLighting || !outReceipt ||
		 !Ral_LightingArtifactRead( artifactBytes, artifactByteLength, &artifact, views ) ||
		 !Ral_LightingRuntimePlanBuild( RAL_BACKEND_VULKAN, plan->frameGeneration, artifactBytes, artifactByteLength,
										 &artifact, &capabilities, &exactPlan ) ||
		 !Ral_LightingRuntimePlanExact( plan, &exactPlan ) )
		return qfalse;
	lighting = (ralVulkanLighting_t *)calloc( 1u, sizeof( *lighting ) );
	if ( !lighting )
		return qfalse;
	lighting->receipt.schemaVersion = RAL_VULKAN_LIGHTING_SCHEMA_VERSION;
	lighting->receipt.plan = exactPlan;
	for ( plane = 0u; plane < exactPlan.planeCount; ++plane ) {
		const ralLightingRuntimePlanePlan_t *planePlan = &exactPlan.planes[plane];
		uint32_t layer;
		if ( !Ral_TextureFormatSupportsFeatures( backend, planePlan->texture.format,
				(ralTextureFormatFeatures_t)( RAL_TEXTURE_FORMAT_FEATURE_SAMPLED |
										 RAL_TEXTURE_FORMAT_FEATURE_FILTER_LINEAR |
										 RAL_TEXTURE_FORMAT_FEATURE_TRANSFER_DST ) ) )
			goto fail;
		lighting->planes[plane] = Ral_CreateTexture( backend, &planePlan->texture );
		if ( !lighting->planes[plane] )
			goto fail;
		for ( layer = 0u; layer < planePlan->texture.depthOrArrayLayers; ++layer ) {
			ralTextureUploadDesc_t upload;
			ralFence_t *fence;
			memset( &upload, 0, sizeof( upload ) );
			upload.mipLevel = 0u;
			upload.arrayLayer = layer;
			upload.data = artifactBytes + planePlan->artifactOffset +
				(uint64_t)layer * planePlan->tightBytesPerRow * planePlan->rowsPerImage;
			upload.dataSize = (uint64_t)planePlan->tightBytesPerRow * planePlan->rowsPerImage;
			upload.suppressMipGeneration = qtrue;
			fence = Ral_TextureUploadAsync( lighting->planes[plane], &upload );
			if ( !fence || Ral_WaitFenceExact( fence, RAL_TIMEOUT_INFINITE ) != ralSuccess || !Ral_FenceSignaled( fence ) ) {
				if ( fence )
					Ral_DestroyFence( fence );
				goto fail;
			}
			Ral_DestroyFence( fence );
		}
		if ( !Ral_TextureGetResourceReceipt( lighting->planes[plane], &lighting->receipt.resources[plane] ) )
			goto fail;
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
		Ral_DestroyTexture( lighting->planes[plane] );
	free( lighting );
	return qfalse;
}

qboolean RalVulkan_LightingDestroy( ralBackend_t *backend,
	ralVulkanLighting_t *lighting, const ralVulkanLightingReceipt_t *authority )
{
	uint32_t plane;
	if ( !backend || !lighting || !authority ||
		 !RalVulkan_LightingReceiptExact( &lighting->receipt, authority ) )
		return qfalse;
	for ( plane = 0u; plane < lighting->receipt.plan.planeCount; ++plane )
		Ral_DestroyTexture( lighting->planes[plane] );
	memset( lighting, 0, sizeof( *lighting ) );
	free( lighting );
	return qtrue;
}
