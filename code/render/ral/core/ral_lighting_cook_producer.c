// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "ral_lighting_cook_producer.h"

#include <string.h>

qboolean Ral_LightingCookProducerReceiptValid(
	const ralLightingCookProducerReceipt_t *receipt )
{
	if ( !receipt ||
		receipt->schemaVersion != RAL_LIGHTING_COOK_PRODUCER_RECEIPT_SCHEMA_VERSION ||
		!Ral_LightingBakeReceiptValid( &receipt->bake ) ||
		!Ral_LightingArtifactReceiptValid( &receipt->directional ) ||
		!Ral_LightingArtifactReceiptValid( &receipt->irradiance ) ||
		receipt->artifactCount != 2u || receipt->ready != qtrue ) return qfalse;
	return receipt->artifacts[0].kind == RAL_LIGHTING_ARTIFACT_DIRECTIONAL_LIGHTMAP &&
		receipt->artifacts[0].bytes && receipt->artifacts[0].byteLength &&
		receipt->artifacts[1].kind == RAL_LIGHTING_ARTIFACT_IRRADIANCE_VOLUME &&
		receipt->artifacts[1].bytes && receipt->artifacts[1].byteLength &&
		( receipt->artifacts[0].productMask & RAL_LIGHTING_COOK_DIRECTIONAL_LIGHTMAP ) &&
		receipt->artifacts[1].productMask == RAL_LIGHTING_COOK_IRRADIANCE_PROBES;
}

qboolean Ral_LightingCookProducerExecute(
	const ralLightingCookProducerRequest_t *request,
	const ralLightingCookProducerWorkspace_t *workspace,
	ralLightingCookProducerReceipt_t *outReceipt )
{
	ralLightingCookProducerReceipt_t result;
	ralLightingProductRequest_t directional;
	ralIrradianceProductRequest_t irradiance;
	uint64_t texels;
	uint32_t index;
	if ( !request || !workspace || !outReceipt ||
		request->schemaVersion != RAL_LIGHTING_COOK_PRODUCER_SCHEMA_VERSION ||
		!request->directionalArtifactGeneration ||
		!request->irradianceArtifactGeneration || !request->geometryHash ||
		!request->materialHash || !request->layoutHash ||
		!request->pageWidth || !request->pageHeight || !request->pageCount ||
		!request->texelPatchIndices || !request->texelCount ||
		workspace->patchCapacity < request->bake.patchCount ||
		workspace->texelCapacity < request->texelCount ||
		!workspace->incoming || !workspace->outgoing || !workspace->directionAccum ||
		!workspace->patchRadiance || !workspace->patchDirection ||
		!workspace->texelRadiance || !workspace->texelDirection ||
		!workspace->directionalRadianceBytes ||
		!workspace->directionalDirectionBytes ||
		!workspace->irradianceCoefficientBytes ||
		!workspace->irradianceValidityBytes ||
		!workspace->directionalArtifactBytes ||
		!workspace->irradianceArtifactBytes ) return qfalse;
	texels = (uint64_t)request->pageWidth * request->pageHeight * request->pageCount;
	if ( texels != request->texelCount ) return qfalse;
	memset( &result, 0, sizeof( result ) );
	result.schemaVersion = RAL_LIGHTING_COOK_PRODUCER_RECEIPT_SCHEMA_VERSION;
	if ( !Ral_LightingBakeRun( &request->bake, workspace->incoming,
		workspace->patchCapacity, workspace->outgoing, workspace->patchCapacity,
		workspace->directionAccum, workspace->patchCapacity,
		workspace->patchRadiance, workspace->patchCapacity,
		workspace->patchDirection, workspace->patchCapacity, &result.bake ) )
		return qfalse;
	for ( index = 0u; index < request->texelCount; ++index ) {
		uint32_t patch = request->texelPatchIndices[index];
		if ( patch == RAL_LIGHTING_COOK_UNMAPPED_TEXEL ) {
			memset( &workspace->texelRadiance[index], 0,
				sizeof( workspace->texelRadiance[index] ) );
			memset( &workspace->texelDirection[index], 0,
				sizeof( workspace->texelDirection[index] ) );
			workspace->texelDirection[index].z = RAL_LIGHT_Q16_ONE;
			continue;
		}
		if ( patch >= result.bake.patchCount ) return qfalse;
		workspace->texelRadiance[index] = workspace->patchRadiance[patch];
		workspace->texelDirection[index] = workspace->patchDirection[patch];
		if ( !workspace->texelDirection[index].x &&
			!workspace->texelDirection[index].y &&
			!workspace->texelDirection[index].z )
			workspace->texelDirection[index] = request->bake.patches[patch].normal;
	}
	memset( &directional, 0, sizeof( directional ) );
	directional.schemaVersion = RAL_LIGHTING_PRODUCT_SCHEMA_VERSION;
	directional.artifactGeneration = request->directionalArtifactGeneration;
	directional.bake = result.bake;
	directional.encoding = request->directionalEncoding;
	directional.pageWidth = request->pageWidth;
	directional.pageHeight = request->pageHeight;
	directional.pageCount = request->pageCount;
	directional.indirectRadiance = workspace->texelRadiance;
	directional.dominantDirection = workspace->texelDirection;
	directional.texelCount = request->texelCount;
	directional.stationaryVisibility = request->stationaryVisibility;
	directional.stationaryVisibilityCount = request->stationaryVisibilityCount;
	if ( !Ral_LightingProductWrite( &directional,
		workspace->directionalRadianceBytes, workspace->directionalRadianceCapacity,
		workspace->directionalDirectionBytes, workspace->directionalDirectionCapacity,
		workspace->directionalArtifactBytes, workspace->directionalArtifactCapacity,
		&result.directional ) ) return qfalse;
	memset( &irradiance, 0, sizeof( irradiance ) );
	irradiance.schemaVersion = RAL_IRRADIANCE_PRODUCT_SCHEMA_VERSION;
	irradiance.artifactGeneration = request->irradianceArtifactGeneration;
	irradiance.producerVersion = result.bake.producerVersion;
	irradiance.geometryHash = request->geometryHash;
	irradiance.materialHash = request->materialHash;
	irradiance.layoutHash = request->layoutHash;
	irradiance.settingsHash = request->bake.settingsHash;
	irradiance.bake = result.bake;
	memcpy( irradiance.dimensions, request->irradianceDimensions,
		sizeof( irradiance.dimensions ) );
	irradiance.encoding = RAL_IRRADIANCE_SH_L1_RGB16F;
	irradiance.patchRadiance = workspace->patchRadiance;
	irradiance.patchCount = result.bake.patchCount;
	irradiance.samples = request->irradianceSamples;
	irradiance.sampleCount = request->irradianceSampleCount;
	if ( !Ral_IrradianceProductWrite( &irradiance,
		workspace->irradianceCoefficientBytes, workspace->irradianceCoefficientCapacity,
		workspace->irradianceValidityBytes, workspace->irradianceValidityCapacity,
		workspace->irradianceArtifactBytes, workspace->irradianceArtifactCapacity,
		&result.irradiance ) ) return qfalse;
	result.artifacts[0] = (ralLightingCookArtifact_t){
		RAL_LIGHTING_COOK_DIRECTIONAL_LIGHTMAP |
			( request->stationaryVisibility ? RAL_LIGHTING_COOK_STATIONARY_VISIBILITY : 0u ),
		RAL_LIGHTING_ARTIFACT_DIRECTIONAL_LIGHTMAP,
		result.directional.artifactGeneration, result.directional.cacheKey,
		workspace->directionalArtifactBytes, result.directional.byteLength };
	result.artifacts[1] = (ralLightingCookArtifact_t){
		RAL_LIGHTING_COOK_IRRADIANCE_PROBES,
		RAL_LIGHTING_ARTIFACT_IRRADIANCE_VOLUME,
		result.irradiance.artifactGeneration, result.irradiance.cacheKey,
		workspace->irradianceArtifactBytes, result.irradiance.byteLength };
	result.artifactCount = 2u;
	result.ready = qtrue;
	if ( !Ral_LightingCookProducerReceiptValid( &result ) ) return qfalse;
	*outReceipt = result;
	return qtrue;
}
