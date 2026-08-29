// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "ral_lighting_cook_pipeline.h"

#include <string.h>

#define FNV_OFFSET UINT64_C( 14695981039346656037 )
#define FNV_PRIME UINT64_C( 1099511628211 )

static uint64_t HashByte( uint64_t hash, uint8_t value )
{
	return ( hash ^ value ) * FNV_PRIME;
}

static uint64_t HashU32( uint64_t hash, uint32_t value )
{
	uint32_t index;
	for ( index = 0u; index < 4u; ++index )
		hash = HashByte( hash, (uint8_t)( value >> ( index * 8u ) ) );
	return hash;
}

static uint64_t HashU64( uint64_t hash, uint64_t value )
{
	uint32_t index;
	for ( index = 0u; index < 8u; ++index )
		hash = HashByte( hash, (uint8_t)( value >> ( index * 8u ) ) );
	return hash;
}

static qboolean ArtifactRoleValid( const ralLightingCookArtifact_t *artifact )
{
	const uint32_t directional = RAL_LIGHTING_COOK_DIRECTIONAL_LIGHTMAP |
		RAL_LIGHTING_COOK_STATIONARY_VISIBILITY;
	if ( !artifact || !artifact->productMask ||
		 ( artifact->productMask & ~RAL_LIGHTING_COOK_ALL ) ||
		 !artifact->artifactGeneration || !artifact->cacheKey ||
		 !artifact->bytes || !artifact->byteLength )
		return qfalse;
	if ( artifact->kind == RAL_LIGHTING_ARTIFACT_DIRECTIONAL_LIGHTMAP )
		return !( artifact->productMask & ~directional );
	if ( artifact->kind == RAL_LIGHTING_ARTIFACT_IRRADIANCE_VOLUME )
		return artifact->productMask == RAL_LIGHTING_COOK_IRRADIANCE_PROBES;
	return qfalse;
}

static void Progress( const ralLightingCookPipelineRequest_t *request,
	ralLightingCookPipelineReceipt_t *receipt, uint32_t artifactIndex )
{
	receipt->progressEventCount++;
	if ( request->progress )
		request->progress( request->callbackContext, &receipt->cook,
			receipt->verifiedProducts, artifactIndex, request->artifactCount );
}

qboolean Ral_LightingCookPipelineReceiptValid(
	const ralLightingCookPipelineReceipt_t *receipt )
{
	if ( !receipt ||
		receipt->schemaVersion != RAL_LIGHTING_COOK_PIPELINE_RECEIPT_SCHEMA_VERSION ||
		!Ral_LightingCookReceiptValid( &receipt->cook ) ||
		( receipt->storedProducts & ~receipt->cook.invalidatedProducts ) ||
		( receipt->verifiedProducts & ~receipt->storedProducts ) ||
		receipt->artifactCount > RAL_LIGHTING_COOK_MAX_ARTIFACTS ||
		!receipt->progressEventCount || receipt->ready != qtrue )
		return qfalse;
	if ( receipt->cook.state == RAL_LIGHTING_COOK_CANCELLED )
		return !receipt->verifiedProducts &&
			!receipt->artifactCount && !receipt->artifactManifestHash;
	return receipt->cook.state == RAL_LIGHTING_COOK_PUBLISHED &&
		receipt->storedProducts == receipt->cook.invalidatedProducts &&
		receipt->verifiedProducts == receipt->cook.invalidatedProducts &&
		receipt->artifactManifestHash;
}

qboolean Ral_LightingCookPipelineExecute(
	const ralLightingCookPipelineRequest_t *request,
	ralLightingCookPipelineReceipt_t *outReceipt )
{
	ralLightingCookPipelineReceipt_t result;
	ralLightingArtifactReceipt_t stored[RAL_LIGHTING_COOK_MAX_ARTIFACTS], loaded;
	uint64_t loadedLength;
	uint64_t manifestHash = HashU32( FNV_OFFSET,
		RAL_LIGHTING_COOK_PIPELINE_RECEIPT_SCHEMA_VERSION );
	uint32_t productCoverage = 0u, index;
	if ( !request || !outReceipt ||
		 request->schemaVersion != RAL_LIGHTING_COOK_PIPELINE_SCHEMA_VERSION ||
		 request->artifactCount > RAL_LIGHTING_COOK_MAX_ARTIFACTS ||
		 ( request->artifactCount && !request->artifacts ) ||
		 !request->cacheOps || !request->cacheOps->writeAtomic ||
		 !request->cacheOps->read || !request->verifyScratch ||
		 !request->verifyScratchCapacity )
		return qfalse;
	memset( &result, 0, sizeof( result ) );
	result.schemaVersion = RAL_LIGHTING_COOK_PIPELINE_RECEIPT_SCHEMA_VERSION;
	if ( !Ral_LightingCookPlan( &request->cook, &result.cook ) ||
		 result.cook.deferredRegionCount )
		return qfalse;
	for ( index = 0u; index < request->artifactCount; ++index ) {
		const ralLightingCookArtifact_t *artifact = &request->artifacts[index];
		if ( !ArtifactRoleValid( artifact ) ||
			 ( artifact->productMask & productCoverage ) ||
			 ( artifact->productMask & ~result.cook.invalidatedProducts ) )
			return qfalse;
		productCoverage |= artifact->productMask;
	}
	if ( productCoverage != result.cook.invalidatedProducts ||
		( result.cook.invalidatedProducts && !request->artifactCount ) )
		return qfalse;
	Progress( request, &result, 0u );
	if ( request->shouldCancel && request->shouldCancel( request->callbackContext ) ) {
		if ( !Ral_LightingCookTransition( &result.cook,
			RAL_LIGHTING_COOK_CANCELLED, 0u, 0u, &result.cook ) )
			return qfalse;
		result.ready = qtrue;
		Progress( request, &result, 0u );
		if ( !Ral_LightingCookPipelineReceiptValid( &result ) ) return qfalse;
		*outReceipt = result;
		return qtrue;
	}
	if ( !Ral_LightingCookTransition( &result.cook,
		RAL_LIGHTING_COOK_RUNNING, 0u, 0u, &result.cook ) )
		return qfalse;
	Progress( request, &result, 0u );
	for ( index = 0u; index < request->artifactCount; ++index ) {
		const ralLightingCookArtifact_t *artifact = &request->artifacts[index];
		if ( request->shouldCancel && request->shouldCancel( request->callbackContext ) ) {
			if ( !Ral_LightingCookTransition( &result.cook,
				RAL_LIGHTING_COOK_CANCELLED, result.storedProducts, 0u, &result.cook ) )
				return qfalse;
			result.ready = qtrue;
			Progress( request, &result, index );
			if ( !Ral_LightingCookPipelineReceiptValid( &result ) ) return qfalse;
			*outReceipt = result;
			return qtrue;
		}
		if ( !Ral_LightingArtifactCacheStore( request->cacheOps, request->cacheContext,
			artifact->bytes, artifact->byteLength, artifact->kind,
			artifact->artifactGeneration, artifact->cacheKey, &stored[index] ) )
			return qfalse;
		result.storedProducts |= artifact->productMask;
		manifestHash = HashU32( manifestHash, artifact->productMask );
		manifestHash = HashU64( manifestHash, stored[index].manifestHash );
		Progress( request, &result, index + 1u );
	}
	if ( request->shouldCancel && request->shouldCancel( request->callbackContext ) ) {
		if ( !Ral_LightingCookTransition( &result.cook,
			RAL_LIGHTING_COOK_CANCELLED, result.storedProducts, 0u, &result.cook ) )
			return qfalse;
		result.ready = qtrue;
		Progress( request, &result, request->artifactCount );
		if ( !Ral_LightingCookPipelineReceiptValid( &result ) ) return qfalse;
		*outReceipt = result;
		return qtrue;
	}
	if ( !manifestHash ) manifestHash = 1u;
	if ( !Ral_LightingCookTransition( &result.cook, RAL_LIGHTING_COOK_STAGED,
		result.storedProducts, manifestHash, &result.cook ) )
		return qfalse;
	Progress( request, &result, request->artifactCount );
	for ( index = 0u; index < request->artifactCount; ++index ) {
		const ralLightingCookArtifact_t *artifact = &request->artifacts[index];
		loadedLength = 0u;
		if ( !Ral_LightingArtifactCacheLoad( request->cacheOps, request->cacheContext,
			artifact->kind, artifact->artifactGeneration, artifact->cacheKey,
			request->verifyScratch, request->verifyScratchCapacity,
			&loadedLength, &loaded ) || loadedLength != artifact->byteLength ||
			!Ral_LightingArtifactReceiptExact( &stored[index], &loaded ) )
			return qfalse;
		result.verifiedProducts |= artifact->productMask;
		Progress( request, &result, index + 1u );
	}
	if ( !Ral_LightingCookTransition( &result.cook, RAL_LIGHTING_COOK_VERIFIED,
		result.verifiedProducts, manifestHash, &result.cook ) ||
		!Ral_LightingCookTransition( &result.cook, RAL_LIGHTING_COOK_PUBLISHED,
		result.verifiedProducts, manifestHash, &result.cook ) )
		return qfalse;
	result.artifactCount = request->artifactCount;
	result.artifactManifestHash = manifestHash;
	result.ready = qtrue;
	Progress( request, &result, request->artifactCount );
	if ( !Ral_LightingCookPipelineReceiptValid( &result ) ) return qfalse;
	*outReceipt = result;
	return qtrue;
}
