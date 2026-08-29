// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "ral_webgpu_lighting.h"

#include <stdlib.h>
#include <string.h>

struct ralWebGpuLighting_s {
	ralWebGpuResource_t *planes[RAL_LIGHTING_RUNTIME_MAX_PLANES];
	ralWebGpuLightingReceipt_t receipt;
};

static qboolean ReceiptValid( const ralWebGpuLightingReceipt_t *receipt ) {
	uint32_t i;
	if ( !receipt || receipt->schemaVersion != RAL_WEBGPU_LIGHTING_SCHEMA_VERSION
			|| receipt->plan.backendType != RAL_BACKEND_WEBGPU
			|| !Ral_LightingRuntimePlanValid( &receipt->plan ) || !receipt->ready )
		return qfalse;
	for ( i = 0u; i < receipt->plan.planeCount; ++i )
		if ( !RalWebGpu_ResourceReceiptExact( &receipt->resources[i],
				&receipt->resources[i] )
				|| !RalWebGpu_WriteReceiptExact( &receipt->writes[i],
					&receipt->writes[i] ) ) return qfalse;
	for ( ; i < RAL_LIGHTING_RUNTIME_MAX_PLANES; ++i )
		if ( memcmp( &receipt->resources[i], &(ralWebGpuResourceReceipt_t){ 0 },
				sizeof( receipt->resources[i] ) )
				|| memcmp( &receipt->writes[i], &(ralWebGpuWriteReceipt_t){ 0 },
					sizeof( receipt->writes[i] ) ) ) return qfalse;
	return qtrue;
}

qboolean RalWebGpu_LightingReceiptExact( const ralWebGpuLightingReceipt_t *a,
		const ralWebGpuLightingReceipt_t *b ) {
	return ReceiptValid( a ) && ReceiptValid( b ) && !memcmp( a, b, sizeof( *a ) )
		? qtrue : qfalse;
}

qboolean RalWebGpu_LightingUpload( ralWebGpuResourceLayer_t *resources,
		const void *artifactMemory, uint64_t artifactByteLength,
		const ralLightingRuntimePlan_t *plan, ralWebGpuLighting_t **outLighting,
		ralWebGpuLightingReceipt_t *outReceipt ) {
	const uint8_t *artifactBytes = (const uint8_t *)artifactMemory;
	ralLightingArtifactReceipt_t artifact;
	ralLightingPayloadView_t views[RAL_LIGHTING_ARTIFACT_MAX_PAYLOADS];
	ralStaticLightingCapabilities_t capabilities = { qtrue, qtrue, qtrue, qtrue };
	ralLightingRuntimePlan_t exactPlan;
	ralWebGpuLighting_t *lighting;
	uint32_t i;
	if ( !resources || !artifactBytes || !artifactByteLength || !plan
			|| !outLighting || !outReceipt || plan->backendType != RAL_BACKEND_WEBGPU
			|| !Ral_LightingArtifactRead( artifactBytes, artifactByteLength,
				&artifact, views )
			|| !Ral_LightingRuntimePlanBuild( RAL_BACKEND_WEBGPU,
				plan->frameGeneration, artifactBytes, artifactByteLength, &artifact,
				&capabilities, &exactPlan )
			|| !Ral_LightingRuntimePlanExact( plan, &exactPlan ) ) return qfalse;
	lighting = (ralWebGpuLighting_t *)calloc( 1u, sizeof( *lighting ) );
	if ( !lighting ) return qfalse;
	lighting->receipt.schemaVersion = RAL_WEBGPU_LIGHTING_SCHEMA_VERSION;
	lighting->receipt.plan = exactPlan;
	for ( i = 0u; i < exactPlan.planeCount; ++i ) {
		const ralLightingRuntimePlanePlan_t *plane = &exactPlan.planes[i];
		ralWebGpuTextureDesc_t desc;
		uint32_t paddedRow = ( plane->tightBytesPerRow + 255u ) & ~255u;
		uint32_t bytesPerTexel = plane->tightBytesPerRow / plane->texture.width;
		uint64_t paddedBytes = (uint64_t)paddedRow * plane->rowsPerImage
			* plane->texture.depthOrArrayLayers;
		uint8_t *staging;
		uint32_t layer, row;
		if ( !paddedRow || !bytesPerTexel || !paddedBytes
				|| paddedBytes > RAL_WEBGPU_MAX_RESOURCE_BYTES ) goto fail;
		staging = (uint8_t *)calloc( 1u, (size_t)paddedBytes );
		if ( !staging ) goto fail;
		for ( layer = 0u; layer < plane->texture.depthOrArrayLayers; ++layer )
			for ( row = 0u; row < plane->rowsPerImage; ++row ) {
				uint64_t sourceOffset = plane->artifactOffset
					+ ( (uint64_t)layer * plane->rowsPerImage + row )
					* plane->tightBytesPerRow;
				memcpy( staging + ( (uint64_t)layer * plane->rowsPerImage + row )
					* paddedRow, artifactBytes + sourceOffset, plane->tightBytesPerRow );
			}
		memset( &desc, 0, sizeof( desc ) );
		desc.width = plane->texture.width; desc.height = plane->texture.height;
		desc.depth = plane->texture.depthOrArrayLayers;
		desc.format = plane->texture.format; desc.bytesPerTexel = bytesPerTexel;
		if ( !RalWebGpu_CreateTexture( resources, &desc, &lighting->planes[i],
				&lighting->receipt.resources[i] )
				|| !RalWebGpu_WriteTexture( resources, lighting->planes[i],
					&lighting->receipt.resources[i], staging, paddedBytes, paddedRow,
					plane->rowsPerImage, &lighting->receipt.writes[i] ) ) {
			free( staging ); goto fail;
		}
		free( staging );
	}
	lighting->receipt.ready = qtrue;
	if ( !ReceiptValid( &lighting->receipt ) ) goto fail;
	*outLighting = lighting; *outReceipt = lighting->receipt; return qtrue;
fail:
	for ( i = 0u; i < exactPlan.planeCount; ++i ) if ( lighting->planes[i] )
		(void)RalWebGpu_DestroyResource( resources, lighting->planes[i],
			&lighting->receipt.resources[i] );
	free( lighting ); return qfalse;
}

qboolean RalWebGpu_LightingDestroy( ralWebGpuResourceLayer_t *resources,
		ralWebGpuLighting_t *lighting,
		const ralWebGpuLightingReceipt_t *authority ) {
	uint32_t i;
	if ( !resources || !lighting || !authority
			|| !RalWebGpu_LightingReceiptExact( &lighting->receipt, authority ) )
		return qfalse;
	for ( i = 0u; i < lighting->receipt.plan.planeCount; ++i )
		if ( !RalWebGpu_DestroyResource( resources, lighting->planes[i],
				&lighting->receipt.resources[i] ) ) return qfalse;
	memset( lighting, 0, sizeof( *lighting ) ); free( lighting ); return qtrue;
}
