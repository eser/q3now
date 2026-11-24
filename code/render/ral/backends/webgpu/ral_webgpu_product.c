// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_webgpu_product.h"
#include "ral_beam_geometry.h"
#include "ral_sky_radiance.h"
#include "ral_webgpu_sky_material.h"
#include "ral_webgpu_taa.h"
#include "ral_webgpu_smaa.h"
#include "ral_temporal.h"

#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

_Static_assert(sizeof(ralWebGpuEffectDrawParams_t) == 64u,
	"WebGPU effect parameter records must match WGSL's four vec4 fields");

typedef struct {
	qhandle_t handle;
	uint64_t generation;
	uint64_t digest;
	ralWebGpuResource_t *texture;
	ralWebGpuResource_t *sampler;
	ralWebGpuResourceReceipt_t textureReceipt;
	ralWebGpuResourceReceipt_t samplerReceipt;
	ralTextureResourceReceipt_t resource;
	ralMaterialRuntimeResource_t runtimeResource;
	qboolean clampToEdge;
	qboolean ready;
} materialEntry_t;

typedef struct {
	ralWebGpuResource_t *vertices;
	ralWebGpuResource_t *indices;
	ralWebGpuResourceReceipt_t vertexReceipt;
	ralWebGpuResourceReceipt_t indexReceipt;
} worldResourceSlot_t;

struct ralWebGpuProduct_s {
	ralWebGpuRuntime_t *runtime;
    ralWebGpuSkyMaterial_t *skyMaterial;
    ralWebGpuSkyMaterial_t *skyCaptureMaterial;
    ralWebGpuResource_t *skyCaptureVertices, *skyCaptureIndices;
    ralWebGpuResourceReceipt_t skyCaptureVertexReceipt, skyCaptureIndexReceipt;
    renderSkyMaterialGpuParams_t skyCaptureParams;
    uintptr_t skyCaptureTextures[10], skyCaptureSamplers[10];
    uint32_t skyCaptureMask;
    float skyCaptureBlend;
    qboolean skyCaptureActive;
    ralWebGpuResource_t *skyVertices[MAX_RENDER_WORLDS];
    ralWebGpuResourceReceipt_t skyVertexReceipts[MAX_RENDER_WORLDS];
    uint64_t skyVertexGenerations[MAX_RENDER_WORLDS];
    uintptr_t physicalSkyTexture, physicalSkySampler;
    float physicalSkyBlend;
    ralWebGpuExposure_t *exposure;
    /* TAA (r_aa 1): the temporal plan, the camera of the last completed frame, and the plan/camera of the frame in flight. */
    ralWebGpuTaa_t *taa;
    qboolean taaEnabled, taaPending;
    ralTemporalState_t taaState;
    uint64_t taaFrameId, taaPendingFrame, taaWorldGeneration;
    uint32_t taaTopologyEpoch, taaResolvedFrames, taaBootstrapFrames;
    qboolean taaPendingResolved;
    ralWebGpuTaaCamera_t taaPreviousCamera, taaPendingCamera;
    ralWebGpuPipelineReceipt_t taaMaskPipeline;   /* entity coverage into the TAA effect mask */
    ralWebGpuSmaa_t *smaa;                       /* SMAA (r_aa 2), exclusive with TAA */
    qboolean smaaEnabled;
    qboolean taaMaskPipelineReady;
    /* Translucent entities by alpha mode (blend, alpha-additive, additive) and their TAA mask pipelines (visible contribution). */
    ralWebGpuPipelineReceipt_t entityBlendPipelines[3], taaTrustPipelines[3];
    qboolean entityBlendPipelinesReady;
    ralExposureSettings_t exposureSettings;
    ralDisplayVisibilityPlan_t exposureVisibility;
	ralWebGpuPipelineReceipt_t worldPipeline;
	ralWebGpuPipelineReceipt_t entityPipeline;
	ralWebGpuPipelineReceipt_t effectPipelines[RAL_WEBGPU_EFFECT_PIPELINE_COUNT];
	ralWebGpuPipelineReceipt_t uiPipeline;
	ralWebGpuPipelineReceipt_t msdfPipeline;
	uint64_t generation;
	uint64_t planGeneration;
	renderEffectDecalPool_t effectDecalPool;
	renderEffectDecalPool_t pendingEffectDecalPool;
	renderEffectDecalPoolReceipt_t pendingEffectDecalPoolReceipt;
	renderEffectBeamPool_t effectBeamPool;
	renderEffectBeamPool_t pendingEffectBeamPool;
	renderEffectBeamPoolReceipt_t pendingEffectBeamPoolReceipt;
	uint32_t viewportWidth;
	uint32_t viewportHeight;
	materialEntry_t materials[RENDER_SUBMISSION_MAX_TEXTURE_RESOURCES];
	uint32_t materialCount;
	uintptr_t cookedMaterialTextureIdentities[RENDER_SUBMISSION_MAX_MATERIALS];
	qboolean cookedMaterialReady[RENDER_SUBMISSION_MAX_MATERIALS];
	uint64_t cookedMaterialRuntimeGeneration;
	/* World-lightmap overbright (r_lightmapBoost), applied in linear space. */
	float lightmapBoost;
	worldResourceSlot_t worlds[MAX_RENDER_WORLDS];
	ralWebGpuResource_t *worldMaterials;
	ralWebGpuResource_t *entityVertices;
	ralWebGpuResource_t *entityIndices;
	ralWebGpuResource_t *entityLighting;
	ralWebGpuResource_t *effectParameters;
	ralWebGpuResourceReceipt_t effectParameterReceipt;
	ralWebGpuResource_t *miscVertices;
	ralWebGpuResource_t *miscIndices;
	ralWebGpuResourceReceipt_t worldMaterialReceipt;
	ralWebGpuResourceReceipt_t entityVertexReceipt;
	ralWebGpuResourceReceipt_t entityIndexReceipt;
	ralWebGpuResourceReceipt_t entityLightingReceipt;
	ralWebGpuResourceReceipt_t miscVertexReceipt;
	ralWebGpuResourceReceipt_t miscIndexReceipt;
	ralWebGpuProductFrameReceipt_t published;
	qboolean inFlight;
	qboolean entityPipelineReady;
	uintptr_t worldMaterialBindGroup;
	qboolean worldMaterialBindGroupReady;
	uintptr_t entityLightingBindGroup;
	qboolean entityLightingBindGroupReady;
	qboolean effectPipelinesReady;
	qboolean uiPipelinesReady;
	uintptr_t atmosphereBindGroups[2u + RAL_WEBGPU_EFFECT_PIPELINE_COUNT];
	qboolean atmosphereBindGroupsReady;
	ralWebGpuResource_t *skySource;
	ralWebGpuResource_t *skyIrradiance[2];
	ralWebGpuResource_t *skyRadiance[2];
	ralWebGpuResource_t *skySh[2];
	ralWebGpuResourceReceipt_t skySourceReceipt;
	ralWebGpuResourceReceipt_t skyIrradianceReceipts[2];
	ralWebGpuResourceReceipt_t skyRadianceReceipts[2];
	ralWebGpuResourceReceipt_t skyShReceipts[2];
	uint64_t skyResourceWorldLifecycleGeneration;
	qboolean skyResourcesReady;
	ralAtmosphereWeatherReceipt_t weatherPlan;
	ralWebGpuWeatherReceipt_t weatherExecution;
	qboolean weatherReady;
	renderDistantWorldDrawMetric_t distantMetricScratch[
		RAL_SKY_DISTANT_MAX_CANDIDATES];
	ralSkyDistantDrawCandidate_t distantCandidateScratch[
		RAL_SKY_DISTANT_MAX_CANDIDATES];
};

#define RAL_WEBGPU_ENTITY_LIGHTING_CAPACITY \
	( RENDER_SUBMISSION_MAX_ENTITIES * RENDER_SUBMISSION_MAX_MODEL_BATCHES )
#define RAL_WEBGPU_ENTITY_LIGHTING_BYTES \
	( (uint64_t)RAL_WEBGPU_ENTITY_LIGHTING_CAPACITY * 16u * sizeof( float ) )
/* Per world batch: emission+sky, baseColor, factors, extras (lightmap flag). */
#define RAL_WEBGPU_WORLD_MATERIAL_FLOATS 16u
#define RAL_WEBGPU_WORLD_MATERIAL_BYTES \
	( (uint64_t)( RENDER_SUBMISSION_MAX_WORLD_BATCHES \
		+ RAL_SKY_DISTANT_MAX_VISIBLE_DRAWS ) * RAL_WEBGPU_WORLD_MATERIAL_FLOATS * sizeof( float ) )

static uint32_t s_productFailureStage;
uint32_t RalWebGpu_ProductLastFailureStage( void ) {
	return s_productFailureStage;
}

static uint64_t HashU64( uint64_t digest, uint64_t value ) {
	for ( uint32_t i = 0u; i < 8u; ++i ) {
		digest ^= ( value >> ( i * 8u ) ) & 0xffu;
		digest *= UINT64_C(1099511628211);
	}
	return digest;
}

static qboolean ReceiptValid( const ralWebGpuProductFrameReceipt_t *receipt ) {
	static const ralWebGpuResourceReceipt_t noResource = {0};
	static const ralWebGpuWriteReceipt_t noWrite = {0};
	qboolean worldResourcesValid;
	qboolean entityResourcesValid;
	qboolean miscResourcesValid;
	qboolean distantResourcesValid;
	if ( !receipt ) return qfalse;
	worldResourcesValid = receipt->plan.worldVertexCount
		? RalWebGpu_ResourceReceiptExact( &receipt->worldVertexBuffer, &receipt->worldVertexBuffer )
			&& RalWebGpu_ResourceReceiptExact( &receipt->worldIndexBuffer, &receipt->worldIndexBuffer )
			&& RalWebGpu_WriteReceiptExact( &receipt->worldVertexWrite, &receipt->worldVertexWrite )
			&& RalWebGpu_WriteReceiptExact( &receipt->worldIndexWrite, &receipt->worldIndexWrite )
			&& RalWebGpu_ResourceReceiptExact( &receipt->worldMaterialBuffer, &receipt->worldMaterialBuffer )
			&& RalWebGpu_WriteReceiptExact( &receipt->worldMaterialWrite, &receipt->worldMaterialWrite )
		: !receipt->plan.worldIndexCount && !receipt->plan.worldBatchCount
			&& !memcmp( &receipt->worldVertexBuffer, &noResource, sizeof(noResource) )
			&& !memcmp( &receipt->worldIndexBuffer, &noResource, sizeof(noResource) )
			&& !memcmp( &receipt->worldMaterialBuffer, &noResource, sizeof(noResource) )
			&& !memcmp( &receipt->worldVertexWrite, &noWrite, sizeof(noWrite) )
			&& !memcmp( &receipt->worldIndexWrite, &noWrite, sizeof(noWrite) )
			&& !memcmp( &receipt->worldMaterialWrite, &noWrite, sizeof(noWrite) );
	entityResourcesValid = receipt->plan.entityBatchCount == 0u
		? qtrue
		: RalWebGpu_ResourceReceiptExact( &receipt->entityVertexBuffer,
			&receipt->entityVertexBuffer )
			&& RalWebGpu_ResourceReceiptExact( &receipt->entityIndexBuffer,
				&receipt->entityIndexBuffer )
			&& RalWebGpu_ResourceReceiptExact( &receipt->entityLightingBuffer,
				&receipt->entityLightingBuffer )
			&& RalWebGpu_WriteReceiptExact( &receipt->entityVertexWrite,
				&receipt->entityVertexWrite )
			&& RalWebGpu_WriteReceiptExact( &receipt->entityIndexWrite,
				&receipt->entityIndexWrite )
			&& RalWebGpu_WriteReceiptExact( &receipt->entityLightingWrite,
				&receipt->entityLightingWrite );
	miscResourcesValid = receipt->plan.polygonBatchCount == 0u
			&& receipt->plan.lightCount == 0u
			&& receipt->plan.effectBatchCount == 0u
			&& receipt->plan.uiPrimitiveCount == 0u
		? qtrue
		: RalWebGpu_ResourceReceiptExact( &receipt->miscVertexBuffer,
			&receipt->miscVertexBuffer )
			&& RalWebGpu_ResourceReceiptExact( &receipt->miscIndexBuffer,
				&receipt->miscIndexBuffer )
			&& RalWebGpu_WriteReceiptExact( &receipt->miscVertexWrite,
				&receipt->miscVertexWrite )
			&& RalWebGpu_WriteReceiptExact( &receipt->miscIndexWrite,
				&receipt->miscIndexWrite );
	distantResourcesValid = receipt->distantWorldDrawCount == 0u
		? receipt->distantWorldCandidateCount <= RAL_SKY_DISTANT_MAX_CANDIDATES
			&& receipt->distantWorldIndexCount == 0u
			&& receipt->distantWorldDepthReset == qfalse
		: receipt->distantWorldCandidateCount > 0u
			&& receipt->distantWorldCandidateCount <= RAL_SKY_DISTANT_MAX_CANDIDATES
			&& receipt->distantWorldDrawCount <= RAL_SKY_DISTANT_MAX_VISIBLE_DRAWS
			&& receipt->distantWorldDrawCount <= receipt->distantWorldCandidateCount
			&& receipt->distantWorldIndexCount > 0u
			&& receipt->distantWorldDepthReset == qtrue
			&& RalWebGpu_ResourceReceiptExact(
				&receipt->distantWorldVertexBuffer,
				&receipt->distantWorldVertexBuffer )
			&& RalWebGpu_ResourceReceiptExact(
				&receipt->distantWorldIndexBuffer,
				&receipt->distantWorldIndexBuffer )
			&& RalWebGpu_WriteReceiptExact( &receipt->distantWorldVertexWrite,
				&receipt->distantWorldVertexWrite )
			&& RalWebGpu_WriteReceiptExact( &receipt->distantWorldIndexWrite,
				&receipt->distantWorldIndexWrite );
	return receipt->schemaVersion == RAL_WEBGPU_PRODUCT_SCHEMA_VERSION
		&& receipt->backendType == RAL_BACKEND_WEBGPU
		&& receipt->backendGeneration && receipt->productGeneration
		&& receipt->frameGeneration && receipt->targetIdentity
		&& RalWebGpu_FrontendPlanReceiptExact( &receipt->plan, &receipt->plan )
		&& worldResourcesValid
		&& RalWebGpu_CommandReceiptExact( &receipt->command, &receipt->command )
		&& RalWebGpu_SubmissionReceiptExact( &receipt->submission,
			&receipt->submission )
		&& entityResourcesValid
		&& miscResourcesValid
		&& distantResourcesValid
		&& receipt->worldDrawCount == receipt->plan.worldBatchCount
		&& receipt->worldEmissiveDrawCount <= receipt->worldDrawCount
			+ receipt->distantWorldDrawCount
		&& receipt->modelEntityCount == receipt->plan.modelEntityCount
		&& receipt->primitiveEntityCount == receipt->plan.primitiveEntityCount
		&& receipt->temporalEntityCount == receipt->plan.temporalEntityCount
		&& receipt->entityDrawCount == receipt->plan.entityBatchCount
		&& receipt->localIrradianceEntityCount
			== receipt->plan.localIrradianceEntityCount
		&& receipt->localIrradianceDrawCount <= receipt->entityDrawCount
		&& ( !receipt->localIrradianceEntityCount
			|| receipt->localIrradianceDrawCount )
		&& receipt->polygonDrawCount == receipt->plan.polygonBatchCount
		&& receipt->lightDrawCount == receipt->plan.lightCount
		&& receipt->effectSpriteDrawCount == receipt->plan.effectSpriteCount
		&& receipt->effectDecalDrawCount == receipt->plan.effectDecalCount
		&& receipt->effectDecalPool.schemaVersion
			== RENDER_EFFECT_DECAL_POOL_SCHEMA_VERSION
		&& receipt->effectDecalPool.frameGeneration == receipt->plan.planGeneration
		&& receipt->effectDecalPool.activeCount == receipt->effectDecalDrawCount
		&& receipt->effectDecalPool.ready == qtrue
		&& receipt->effectRibbonDrawCount == receipt->plan.effectRibbonCount
		&& receipt->effectBeamDrawCount == receipt->plan.effectBeamCount
		&& receipt->effectBeamStageDrawCount >= receipt->effectBeamDrawCount
		&& receipt->effectBeamStageDrawCount <= receipt->effectBeamDrawCount * RENDER_MATERIAL_MAX_STAGES
		&& receipt->effectParameterCount == receipt->effectBeamStageDrawCount + 1u
		&& receipt->effectParameterCount <= RAL_WEBGPU_EFFECT_PARAMETER_CAPACITY
		&& RalWebGpu_ResourceReceiptExact(&receipt->effectParameterBuffer, &receipt->effectParameterBuffer)
		&& RalWebGpu_WriteReceiptExact(&receipt->effectParameterWrite, &receipt->effectParameterWrite)
		&& receipt->effectParameterBuffer.byteSize == RAL_WEBGPU_EFFECT_PARAMETER_BYTES
		&& receipt->effectParameterWrite.kind == RAL_WEBGPU_WRITE_BUFFER
		&& receipt->effectParameterWrite.byteOffset == 0u
		&& receipt->effectParameterWrite.byteSize == (uint64_t)receipt->effectParameterCount * sizeof(ralWebGpuEffectDrawParams_t)
		&& receipt->effectParameterWrite.resourceGeneration == receipt->effectParameterBuffer.resourceGeneration
		&& receipt->effectParameterWrite.backendGeneration == receipt->effectParameterBuffer.backendGeneration
		&& receipt->effectBeamPool.schemaVersion
			== RENDER_EFFECT_BEAM_POOL_SCHEMA_VERSION
		&& receipt->effectBeamPool.frameGeneration == receipt->plan.planGeneration
		&& receipt->effectBeamPool.activeCount == receipt->effectBeamDrawCount
		&& receipt->effectBeamPool.ready == qtrue
		&& receipt->uiDrawCount == receipt->plan.uiPrimitiveCount
        && receipt->toneMapDrawCount <= 1u
        && (receipt->exposureDispatchCount == 0u
            || (receipt->exposureDispatchCount == 2u && receipt->toneMapDrawCount == 1u))
        && (receipt->aaDispatchCount == 0u
            || (receipt->aaDispatchCount == 2u && receipt->toneMapDrawCount == 1u))
        && receipt->command.dispatchCount == receipt->exposureDispatchCount + receipt->aaDispatchCount
		&& receipt->atmosphereBoundDrawCount == receipt->worldDrawCount
			+ receipt->distantWorldDrawCount
			+ receipt->entityDrawCount + receipt->polygonDrawCount
			+ receipt->lightDrawCount + receipt->effectSpriteDrawCount
			+ receipt->effectDecalDrawCount + receipt->effectRibbonDrawCount
			+ receipt->effectBeamStageDrawCount
		&& Ral_AtmosphereWeatherReceiptExact( &receipt->weather,
			&receipt->weather )
		&& receipt->weatherDrawCount == ( receipt->weather.zeroWork ? 0u : 1u )
		&& receipt->deferredNonWorldDrawCount
			== receipt->plan.drawCount - receipt->worldDrawCount
				- receipt->entityDrawCount - receipt->polygonDrawCount
				- receipt->lightDrawCount - receipt->effectSpriteDrawCount
				- receipt->effectDecalDrawCount - receipt->effectRibbonDrawCount
				- receipt->effectBeamDrawCount
				- receipt->uiDrawCount
		&& receipt->deferredNonWorldDrawCount == 0u
		&& !receipt->unresolvedCount && !receipt->fallbackCount
		&& !receipt->fatalCount && receipt->ready == qtrue;
}

qboolean RalWebGpu_ProductFrameReceiptExact(
		const ralWebGpuProductFrameReceipt_t *a,
		const ralWebGpuProductFrameReceipt_t *b ) {
	return ReceiptValid( a ) && ReceiptValid( b )
		&& !memcmp( a, b, sizeof( *a ) );
}

static materialEntry_t *FindMaterial( ralWebGpuProduct_t *product,
		qhandle_t handle ) {
	for ( uint32_t i = 0u; i < product->materialCount; ++i )
		if ( product->materials[i].handle == handle ) return &product->materials[i];
	return NULL;
}

static qboolean EffectPipelinesOwned( const ralWebGpuProduct_t *product,
		const ralWebGpuRuntimeReceipt_t *runtimeReceipt ) {
	if ( !product || !runtimeReceipt || !product->effectPipelinesReady )
		return qfalse;
	for ( uint32_t i = 0u; i < RAL_WEBGPU_EFFECT_PIPELINE_COUNT; ++i )
		if ( !RalWebGpu_RuntimeOwnsPipeline( product->runtime, runtimeReceipt,
				&product->effectPipelines[i] ) ) return qfalse;
	return qtrue;
}

static void DestroyMaterial( ralWebGpuResourceLayer_t *resources,
		materialEntry_t *entry ) {
	if ( !resources || !entry ) return;
	if ( entry->sampler ) RalWebGpu_DestroyResource( resources, entry->sampler,
		&entry->samplerReceipt );
	if ( entry->texture ) RalWebGpu_DestroyResource( resources, entry->texture,
		&entry->textureReceipt );
	memset( entry, 0, sizeof( *entry ) );
}

static qboolean UploadMaterialMip( ralWebGpuResourceLayer_t *resources,
		ralWebGpuResource_t *texture,
		const ralWebGpuResourceReceipt_t *textureReceipt, const byte *rgba8,
		uint32_t width, uint32_t height, uint32_t mipLevel,
		ralWebGpuWriteReceipt_t *outReceipt ) {
	uint32_t tightRowBytes, rowBytes;
	uint64_t paddedBytes;
	byte *padded;
	if ( !resources || !texture || !textureReceipt || !rgba8 || !width || !height
			|| width > UINT32_MAX / 4u ) return qfalse;
	tightRowBytes = width * 4u;
	rowBytes = ( tightRowBytes + 255u ) & ~255u;
	paddedBytes = (uint64_t)rowBytes * height;
	if ( !paddedBytes || paddedBytes > RAL_WEBGPU_MAX_RESOURCE_BYTES ) return qfalse;
	padded = (byte *)calloc( 1u, (size_t)paddedBytes );
	if ( !padded ) return qfalse;
	for ( uint32_t row = 0u; row < height; ++row )
		memcpy( padded + (size_t)row * rowBytes,
			rgba8 + (size_t)row * tightRowBytes, tightRowBytes );
	qboolean ok = RalWebGpu_WriteTextureMip( resources, texture, textureReceipt,
		padded, paddedBytes, rowBytes, height, mipLevel, width, height, 1u,
		outReceipt );
	free( padded ); return ok;
}

static byte *BuildNextMaterialMip( const byte *source, uint32_t width,
		uint32_t height ) {
	uint32_t nextWidth = width > 1u ? width >> 1u : 1u;
	uint32_t nextHeight = height > 1u ? height >> 1u : 1u;
	byte *next = (byte *)malloc( (size_t)nextWidth * nextHeight * 4u );
	if ( !next ) return NULL;
	for ( uint32_t y = 0u; y < nextHeight; ++y ) {
		for ( uint32_t x = 0u; x < nextWidth; ++x ) {
			uint32_t sx[2] = { x * 2u, MIN( x * 2u + 1u, width - 1u ) };
			uint32_t sy[2] = { y * 2u, MIN( y * 2u + 1u, height - 1u ) };
			for ( uint32_t channel = 0u; channel < 4u; ++channel ) {
				uint32_t sum = 0u;
				for ( uint32_t oy = 0u; oy < 2u; ++oy )
					for ( uint32_t ox = 0u; ox < 2u; ++ox )
						sum += source[( (size_t)sy[oy] * width + sx[ox] ) * 4u
							+ channel];
				next[( (size_t)y * nextWidth + x ) * 4u + channel] =
					(byte)( ( sum + 2u ) >> 2u );
			}
		}
	}
	return next;
}

static qboolean UploadMaterial( ralWebGpuResourceLayer_t *resources,
		const renderTextureSnapshot_t *material, materialEntry_t *out ) {
	ralWebGpuTextureDesc_t textureDesc;
	ralWebGpuSamplerDesc_t samplerDesc;
	ralWebGpuWriteReceipt_t writeReceipt;
	const byte *levelPixels;
	byte *ownedLevel = NULL;
	uint32_t width, height, mipLevels;
	if ( !resources || !material || !out || material->ready != qtrue
			|| !material->rgba8 || !material->width || !material->height
			|| material->rowBytes != material->width * 4u ) return qfalse;
	memset( out, 0, sizeof( *out ) );
	memset( &textureDesc, 0, sizeof( textureDesc ) );
	textureDesc.width = material->width; textureDesc.height = material->height;
	textureDesc.depth = 1u;
	mipLevels = material->textureCacheKeyReady
		? material->textureCacheKey.asset.targetMipLevels : 1u;
	textureDesc.mipLevelCount = mipLevels;
	textureDesc.format = material->srgb ? RAL_FORMAT_R8G8B8A8_SRGB
		: RAL_FORMAT_R8G8B8A8_UNORM;
	textureDesc.bytesPerTexel = 4u;
	textureDesc.usage = RAL_WEBGPU_TEXTURE_COPY_DESTINATION
		| RAL_WEBGPU_TEXTURE_SAMPLED;
	memset( &samplerDesc, 0, sizeof( samplerDesc ) );
	samplerDesc.linearMinification = qtrue;
	samplerDesc.linearMagnification = qtrue;
	samplerDesc.linearMipmapping = mipLevels > 1u ? qtrue : qfalse;
	samplerDesc.clampToEdge = material->clampToEdge;
	if ( !RalWebGpu_CreateTexture( resources, &textureDesc, &out->texture,
			&out->textureReceipt ) ) return qfalse;
	levelPixels = material->rgba8; width = material->width; height = material->height;
	for ( uint32_t level = 0u; level < mipLevels; ++level ) {
		if ( !UploadMaterialMip( resources, out->texture, &out->textureReceipt,
				levelPixels, width, height, level, &writeReceipt ) ) {
			free( ownedLevel ); DestroyMaterial( resources, out ); return qfalse;
		}
		if ( level + 1u < mipLevels ) {
			byte *next = BuildNextMaterialMip( levelPixels, width, height );
			if ( !next ) {
				free( ownedLevel ); DestroyMaterial( resources, out ); return qfalse;
			}
			free( ownedLevel ); ownedLevel = next; levelPixels = next;
			width = width > 1u ? width >> 1u : 1u;
			height = height > 1u ? height >> 1u : 1u;
		}
	}
	free( ownedLevel );
	if ( !RalWebGpu_CreateSampler( resources, &samplerDesc, &out->sampler,
			&out->samplerReceipt ) ) {
		DestroyMaterial( resources, out ); return qfalse;
	}
	out->handle = material->handle;
	out->clampToEdge = material->clampToEdge;
	out->generation = material->generation; out->digest = material->digest;
	memset( &out->resource, 0, sizeof( out->resource ) );
	out->resource.schemaVersion = RAL_TEXTURE_RESOURCE_RECEIPT_SCHEMA_VERSION;
	out->resource.backendType = RAL_BACKEND_WEBGPU;
	out->resource.textureIdentity = out->textureReceipt.resourceIdentity;
	out->resource.resourceGeneration = out->textureReceipt.resourceGeneration;
	out->resource.type = RAL_TEXTURE_2D;
	out->resource.format = textureDesc.format;
	out->resource.usage = RAL_TEXTURE_USAGE_SAMPLED
		| RAL_TEXTURE_USAGE_TRANSFER_DST;
	out->resource.width = material->width; out->resource.height = material->height;
	out->resource.mipLevels = mipLevels; out->resource.arrayLayers = 1u;
	out->resource.imported = qfalse; out->resource.ready = qtrue;
	if ( !Ral_TextureResourceReceiptValid( &out->resource )
			|| ( material->textureCacheKeyReady
				&& !Ral_MaterialRuntimeResourceBuild(
					&material->textureCacheKey, &out->resource,
					&out->runtimeResource ) ) ) {
		DestroyMaterial( resources, out ); return qfalse;
	}
	out->ready = qtrue; return qtrue;
}

qboolean RalWebGpu_ProductPrepareMaterialResources(
		ralWebGpuProduct_t *product,
		const ralWebGpuRuntimeReceipt_t *runtimeReceipt,
		const renderSubmissionState_t *frontend ) {
	ralWebGpuResourceLayer_t *resources;
	materialEntry_t *staged = NULL;
	uint32_t stagedCount = 0u, newCount = 0u, textureCount;
	qboolean ok = qfalse;
	if ( !product || !runtimeReceipt || !frontend || product->skyCaptureActive
			|| frontend->materialCount > RENDER_SUBMISSION_MAX_MATERIALS
			|| frontend->stageTextureCount > RENDER_SUBMISSION_MAX_STAGE_TEXTURES )
		return qfalse;
	textureCount = frontend->materialCount + frontend->stageTextureCount;
	resources = RalWebGpu_RuntimeResources( product->runtime, runtimeReceipt );
	if ( !resources ) return qfalse;
	if ( textureCount ) {
		staged = (materialEntry_t *)calloc( textureCount,
			sizeof( *staged ) );
		if ( !staged ) return qfalse;
	}
	for ( uint32_t i = 0u; i < textureCount; ++i ) {
		renderTextureSnapshot_t material;
		if ( i < frontend->materialCount ) {
			if ( !RenderSubmission_TextureSnapshot( frontend,
					frontend->materials[i].snapshot.handle, &material ) ) goto done;
		} else material = frontend->stageTextures[
			i - frontend->materialCount].snapshot;
		materialEntry_t *cached = FindMaterial( product, material.handle );
		if ( cached && ( ( cached->generation == material.generation
				&& cached->digest == material.digest )
				|| ( cached->clampToEdge == material.clampToEdge
					&& RenderSubmission_TextureResourceMatches( &material,
						&cached->runtimeResource ) ) ) ) {
			cached->generation = material.generation;
			cached->digest = material.digest;
			continue;
		}
		if ( !cached && product->materialCount + ++newCount
				> RENDER_SUBMISSION_MAX_TEXTURE_RESOURCES ) goto done;
		if ( !UploadMaterial( resources, &material, &staged[stagedCount] ) )
			goto done;
		stagedCount++;
	}
	for ( uint32_t i = 0u; i < stagedCount; ++i ) {
		materialEntry_t *cached = FindMaterial( product, staged[i].handle );
		if ( cached ) DestroyMaterial( resources, cached );
		else cached = &product->materials[product->materialCount++];
		*cached = staged[i];
		memset( &staged[i], 0, sizeof( staged[i] ) );
	}
	/* Frontend world teardown owns the live texture set. Remove entries no
	 * longer reachable from either semantic materials or stage resources so
	 * repeated map loads cannot accumulate GPU objects. */
	for ( uint32_t i = 0u; i < product->materialCount; ) {
		renderTextureSnapshot_t snapshot;
		if ( RenderSubmission_TextureSnapshot( frontend,
				product->materials[i].handle, &snapshot ) ) { ++i; continue; }
		DestroyMaterial( resources, &product->materials[i] );
		product->materialCount--;
		if ( i != product->materialCount ) {
			product->materials[i] = product->materials[product->materialCount];
			memset( &product->materials[product->materialCount], 0,
				sizeof( product->materials[product->materialCount] ) );
		}
	}
	ok = qtrue;
done:
	if ( staged ) {
		for ( uint32_t i = 0u; i < stagedCount; ++i )
			DestroyMaterial( resources, &staged[i] );
		free( staged );
	}
	return ok;
}

static void DestroyWorldResources( ralWebGpuProduct_t *product,
		ralWebGpuResourceLayer_t *resources, int worldIndex ) {
	worldResourceSlot_t *slot;
	if ( !product || !resources || worldIndex < 0
			|| worldIndex >= MAX_RENDER_WORLDS ) return;
	slot = &product->worlds[worldIndex];
	if ( slot->indices ) RalWebGpu_DestroyResource( resources,
		slot->indices, &slot->indexReceipt );
	if ( slot->vertices ) RalWebGpu_DestroyResource( resources,
		slot->vertices, &slot->vertexReceipt );
	memset( slot, 0, sizeof( *slot ) );
}

static void DestroyFrameResources( ralWebGpuProduct_t *product,
		ralWebGpuResourceLayer_t *resources ) {
	if ( !product || !resources ) return;
	if ( product->miscIndices ) RalWebGpu_DestroyResource( resources,
		product->miscIndices, &product->miscIndexReceipt );
	if ( product->miscVertices ) RalWebGpu_DestroyResource( resources,
		product->miscVertices, &product->miscVertexReceipt );
	if ( product->entityIndices ) RalWebGpu_DestroyResource( resources,
		product->entityIndices, &product->entityIndexReceipt );
	if ( product->entityVertices ) RalWebGpu_DestroyResource( resources,
		product->entityVertices, &product->entityVertexReceipt );
	product->entityIndices = NULL; product->entityVertices = NULL;
	product->miscIndices = NULL; product->miscVertices = NULL;
	memset( &product->entityIndexReceipt, 0, sizeof( product->entityIndexReceipt ) );
	memset( &product->entityVertexReceipt, 0, sizeof( product->entityVertexReceipt ) );
	memset( &product->miscIndexReceipt, 0, sizeof( product->miscIndexReceipt ) );
	memset( &product->miscVertexReceipt, 0, sizeof( product->miscVertexReceipt ) );
}

typedef struct {
	uint32_t firstIndex;
	uint32_t indexCount;
	qhandle_t material;
	uint64_t contentDigest;
	float localSh[4][4];
} entityDrawPlan_t;

static qboolean EntityLighting( const renderEntityCommand_t *command,
		float out[4][4] ) {
	if ( !command || !out ) return qfalse;
	memset( out, 0, 16u * sizeof( float ) );
	if ( !command->hasLocalIrradiance ) return qtrue;
	if ( !Ral_IrradianceEntitySampleReceiptValid( &command->localIrradiance )
			|| !Ral_LightingCompositionReceiptValid(
				&command->lightingComposition )
			|| command->lightingComposition.diffuseAuthority
				!= RAL_LIGHTING_DIFFUSE_AUTHORITY_LOCAL_SH
			|| command->lightingComposition.activeTermMask
				!= RAL_LIGHTING_TERM_LOCAL_SH ) return qfalse;
	for ( uint32_t coefficient = 0u; coefficient < 4u; ++coefficient )
		for ( uint32_t channel = 0u; channel < 3u; ++channel )
			out[coefficient][channel] = (float)command->localIrradiance
				.blendedCoefficientsQ16[coefficient][channel] / 65536.0f;
	out[0][3] = 1.0f;
	return qtrue;
}

static void SetEntityVertex( renderWorldVertex_t *vertex, float x, float y,
		float z, float s, float t, const byte color[4] ) {
	memset( vertex, 0, sizeof( *vertex ) );
	vertex->position[0] = x; vertex->position[1] = y; vertex->position[2] = z;
	vertex->texCoord[0] = s; vertex->texCoord[1] = t;
	vertex->normal[2] = 1.0f;
	memcpy( vertex->color, color, sizeof( vertex->color ) );
}

qboolean RalWebGpu_ProductProjection( const renderWorldSnapshot_t *world,
        float outProjection[4] ) {
	const float nearPlane = 4.0f, farPlane = 65536.0f;
	float tanHalfX, tanHalfY;
	if ( !world || !outProjection
			|| !isfinite( world->fovX ) || !isfinite( world->fovY )
			|| world->fovX <= 1.0f || world->fovX >= 179.0f
			|| world->fovY <= 1.0f || world->fovY >= 179.0f ) return qfalse;
	tanHalfX = tanf( world->fovX * 0.00872664625997164788f );
	tanHalfY = tanf( world->fovY * 0.00872664625997164788f );
	if ( !isfinite( tanHalfX ) || !isfinite( tanHalfY )
			|| tanHalfX <= 0.0f || tanHalfY <= 0.0f ) return qfalse;
	outProjection[0] = tanHalfX;
	outProjection[1] = tanHalfY;
	outProjection[2] = nearPlane;
	outProjection[3] = farPlane / ( farPlane - nearPlane );
	return qtrue;
}

/* Entity/misc geometry keeps the compact frontend vertex for the entity
 * pipeline: clip W and the fog coordinates overwrite texCoord[0] and
 * lightmapCoord in place (those pipelines sample no textures). */
static qboolean ProjectVerticesInPlace( const renderWorldSnapshot_t *world,
        renderWorldVertex_t *vertices, uint32_t vertexCount ) {
    float projection[4];
    if ( (vertexCount && !vertices) || !RalWebGpu_ProductProjection(world, projection) ) return qfalse;
    const float depthScale = projection[3], depthBias = -projection[2] * depthScale;
	for ( uint32_t i = 0u; i < vertexCount; ++i ) {
		float delta[3], forward, side, up, worldZ;
		worldZ = vertices[i].position[2];
		for ( uint32_t axis = 0u; axis < 3u; ++axis )
			delta[axis] = vertices[i].position[axis] - world->viewOrigin[axis];
		forward = delta[0] * world->viewAxis[0][0]
			+ delta[1] * world->viewAxis[0][1]
			+ delta[2] * world->viewAxis[0][2];
		side = -( delta[0] * world->viewAxis[1][0]
			+ delta[1] * world->viewAxis[1][1]
			+ delta[2] * world->viewAxis[1][2] );
		up = delta[0] * world->viewAxis[2][0]
			+ delta[1] * world->viewAxis[2][1]
			+ delta[2] * world->viewAxis[2][2];
		vertices[i].position[0] = side / projection[0];
		vertices[i].position[1] = up / projection[1];
		vertices[i].position[2] = forward * depthScale + depthBias;
		vertices[i].texCoord[0] = forward;
		vertices[i].lightmapCoord[0] = sqrtf( delta[0] * delta[0]
			+ delta[1] * delta[1] + delta[2] * delta[2] );
		vertices[i].lightmapCoord[1] = worldZ;
	}
	return qtrue;
}

/* `jitterNdc` is the TAA sub-pixel offset (zero without TAA), added in clip space (x w) to world and entity geometry only. */
static qboolean ProjectVertices( const renderWorldSnapshot_t *world,
        const renderWorldVertex_t *source, ralWebGpuWorldVertex_t *vertices,
        uint32_t vertexCount, const float jitterNdc[2] ) {
    float projection[4];
    if ( (vertexCount && (!vertices || !source)) || !RalWebGpu_ProductProjection(world, projection) ) return qfalse;
    const float depthScale = projection[3], depthBias = -projection[2] * depthScale;
	for ( uint32_t i = 0u; i < vertexCount; ++i ) {
		float delta[3], forward, side, up, worldZ;
		vertices[i].authored = source[i];
		worldZ = source[i].position[2];
		for ( uint32_t axis = 0u; axis < 3u; ++axis )
			delta[axis] = source[i].position[axis] - world->viewOrigin[axis];
		forward = delta[0] * world->viewAxis[0][0]
			+ delta[1] * world->viewAxis[0][1]
			+ delta[2] * world->viewAxis[0][2];
		side = -( delta[0] * world->viewAxis[1][0]
			+ delta[1] * world->viewAxis[1][1]
			+ delta[2] * world->viewAxis[1][2] );
		up = delta[0] * world->viewAxis[2][0]
			+ delta[1] * world->viewAxis[2][1]
			+ delta[2] * world->viewAxis[2][2];
		vertices[i].authored.position[0] = side / projection[0] + jitterNdc[0] * forward;
		vertices[i].authored.position[1] = up / projection[1] + jitterNdc[1] * forward;
		vertices[i].authored.position[2] = forward * depthScale + depthBias;
		vertices[i].clipW = forward;
		vertices[i].fogCoord[0] = sqrtf( delta[0] * delta[0]
			+ delta[1] * delta[1] + delta[2] * delta[2] );
		vertices[i].fogCoord[1] = worldZ;
	}
	return qtrue;
}

static qboolean BuildEntityGeometry( const renderSubmissionState_t *frontend,
		const ralWebGpuFrontendPlanReceipt_t *plan,
		const renderWorldSnapshot_t *view,
		renderWorldVertex_t **outVertices, uint32_t *outVertexCount,
		uint32_t **outIndices, uint32_t *outIndexCount,
		entityDrawPlan_t **outDraws, uint32_t *outDrawCount ) {
	const renderEntityCommand_t *entities;
	renderWorldVertex_t *vertices;
	entityDrawPlan_t *draws;
	uint32_t *indices;
	uint32_t entityCount = 0u, vertexCapacity = 0u;
	uint32_t vertexCount = 0u, indexCount = 0u, drawCount = 0u;
	if ( !frontend || !plan || !view || !outVertices || !outVertexCount || !outIndices
			|| !outIndexCount || !outDraws || !outDrawCount ) return qfalse;
	entities = RenderSubmission_EntityCommands( frontend, &entityCount );
	if ( entityCount && !entities ) return qfalse;
	for ( uint32_t i = 0u; i < entityCount; ++i ) {
		uint32_t add = 0u;
		if ( entities[i].entity.reType == RT_MODEL ) {
			renderModelSnapshot_t model;
			if ( !RenderSubmission_ModelSnapshot( frontend,
					entities[i].entity.hModel, &model ) || !model.ready ) return qfalse;
			add = model.vertexCount;
		} else if ( entities[i].entity.reType == RT_SPRITE ) add = 4u;
		else if ( entities[i].entity.reType == RT_BEAM ) add = 8u;
		else return qfalse;
		if ( add > UINT32_MAX - vertexCapacity ) return qfalse;
		vertexCapacity += add;
	}
	if ( !vertexCapacity || !plan->entityIndexCount || !plan->entityBatchCount )
		return qfalse;
	if ( (uint64_t)vertexCapacity * sizeof( *vertices )
			> RAL_WEBGPU_MAX_RESOURCE_BYTES
			|| (uint64_t)plan->entityIndexCount * sizeof( *indices )
				> RAL_WEBGPU_MAX_RESOURCE_BYTES ) return qfalse;
	vertices = calloc( vertexCapacity, sizeof( *vertices ) );
	indices = calloc( plan->entityIndexCount, sizeof( *indices ) );
	draws = calloc( plan->entityBatchCount, sizeof( *draws ) );
	if ( !vertices || !indices || !draws ) {
		free( vertices ); free( indices ); free( draws ); return qfalse;
	}
	for ( uint32_t i = 0u; i < entityCount; ++i ) {
		const refEntity_t *entity = &entities[i].entity;
		float localSh[4][4];
		uint32_t baseVertex = vertexCount;
		if ( !EntityLighting( &entities[i], localSh ) ) goto fail;
		if ( entity->reType == RT_MODEL ) {
			renderModelSnapshot_t model;
			if ( !RenderSubmission_ModelSnapshot( frontend, entity->hModel, &model )
					|| !model.ready || !model.positions || !model.normals || !model.texCoords
					|| !model.indices || !model.batches ) goto fail;
			for ( uint32_t vertex = 0u; vertex < model.vertexCount; ++vertex ) {
				const float *current = &model.positions[
					( (uint32_t)entity->frame * model.vertexCount + vertex ) * 3u];
				const float *old = &model.positions[
					( (uint32_t)entity->oldframe * model.vertexCount + vertex ) * 3u];
				const float *currentNormal = &model.normals[
					( (uint32_t)entity->frame * model.vertexCount + vertex ) * 3u];
				const float *oldNormal = &model.normals[
					( (uint32_t)entity->oldframe * model.vertexCount + vertex ) * 3u];
				float local[3], localNormal[3], world[3], worldNormal[3];
				for ( uint32_t component = 0u; component < 3u; ++component )
					local[component] = current[component] * ( 1.0f - entity->backlerp )
						+ old[component] * entity->backlerp;
				for ( uint32_t component = 0u; component < 3u; ++component )
					localNormal[component] = currentNormal[component]
						* ( 1.0f - entity->backlerp )
						+ oldNormal[component] * entity->backlerp;
				for ( uint32_t component = 0u; component < 3u; ++component )
					world[component] = entity->origin[component]
						+ entity->axis[0][component] * local[0]
						+ entity->axis[1][component] * local[1]
						+ entity->axis[2][component] * local[2];
				for ( uint32_t component = 0u; component < 3u; ++component )
					worldNormal[component] = entity->axis[0][component] * localNormal[0]
						+ entity->axis[1][component] * localNormal[1]
						+ entity->axis[2][component] * localNormal[2];
				SetEntityVertex( &vertices[vertexCount++], world[0], world[1],
					world[2], model.texCoords[vertex * 2u],
					model.texCoords[vertex * 2u + 1u], entity->shader.rgba );
				{
					renderWorldVertex_t *written = &vertices[vertexCount - 1u];
					float length = sqrtf( worldNormal[0] * worldNormal[0]
						+ worldNormal[1] * worldNormal[1]
						+ worldNormal[2] * worldNormal[2] );
					if ( length > 0.000001f ) for ( uint32_t component = 0u;
							component < 3u; ++component )
						written->normal[component] = worldNormal[component] / length;
				}
			}
			for ( uint32_t batchIndex = 0u; batchIndex < model.batchCount;
					++batchIndex ) {
				const renderModelBatch_t *batch = &model.batches[batchIndex];
				entityDrawPlan_t *draw;
				if ( drawCount >= plan->entityBatchCount
						|| batch->firstIndex > model.indexCount
						|| batch->indexCount > model.indexCount - batch->firstIndex
						|| indexCount > plan->entityIndexCount - batch->indexCount )
					goto fail;
				draw = &draws[drawCount++]; draw->firstIndex = indexCount;
				draw->indexCount = batch->indexCount;
				draw->material = RenderSubmission_EntityBatchMaterial(
					frontend, &entities[i], batch );
				draw->contentDigest = HashU64( model.digest,
					( (uint64_t)i << 32u ) | batchIndex );
				memcpy( draw->localSh, localSh, sizeof( draw->localSh ) );
				for ( uint32_t index = 0u; index < batch->indexCount; ++index ) {
					uint32_t source = model.indices[batch->firstIndex + index];
					if ( source >= model.vertexCount ) goto fail;
					indices[indexCount++] = baseVertex + source;
				}
			}
		} else if ( entity->reType == RT_SPRITE ) {
			static const uint32_t quad[6] = { 0u, 1u, 2u, 0u, 2u, 3u };
			static const float uv[4][2] = {{0,0},{1,0},{1,1},{0,1}};
			float radius = entity->radius > 0.0f ? entity->radius : 1.0f;
			const float angle = entity->rotation * 0.01745329251994329577f;
			const float sine = sinf(angle), cosine = cosf(angle);
			float left[3], up[3];
			for (uint32_t axis=0; axis<3; ++axis) {
				left[axis]=radius*(cosine*view->viewAxis[1][axis]-sine*view->viewAxis[2][axis]);
				up[axis]=radius*(cosine*view->viewAxis[2][axis]+sine*view->viewAxis[1][axis]);
			}
			entityDrawPlan_t *draw = &draws[drawCount++];
			if ( drawCount > plan->entityBatchCount
					|| indexCount > plan->entityIndexCount - 6u ) goto fail;
			for (uint32_t corner=0; corner<4; ++corner) {
				float position[3];
				const float ls=(corner==0 || corner==3)?1.0f:-1.0f;
				const float us=corner<2?1.0f:-1.0f;
				for (uint32_t axis=0; axis<3; ++axis)
					position[axis]=entity->origin[axis]+ls*left[axis]+us*up[axis];
				SetEntityVertex(&vertices[vertexCount++],position[0],position[1],position[2],
					uv[corner][0],uv[corner][1],entity->shader.rgba);
			}
			for ( uint32_t index = 0u; index < 6u; ++index )
				indices[indexCount++] = baseVertex + quad[index];
			draw->firstIndex = indexCount - 6u; draw->indexCount = 6u;
			draw->material = entity->customShader;
			draw->contentDigest = HashU64( plan->frontendFrameDigest, i );
			memcpy( draw->localSh, localSh, sizeof( draw->localSh ) );
		} else {
			static const uint32_t box[36] = { 0,1,2,0,2,3,4,6,5,4,7,6,
				0,4,5,0,5,1,1,5,6,1,6,2,2,6,7,2,7,3,3,7,4,3,4,0 };
			float width = entity->radius > 0.0f ? entity->radius : 1.0f;
			float dx = entity->oldorigin[0] - entity->origin[0];
			float dy = entity->oldorigin[1] - entity->origin[1];
			float px = dy != 0.0f ? -dy : width;
			float py = dx != 0.0f ? dx : 0.0f;
			entityDrawPlan_t *draw = &draws[drawCount++];
			if ( drawCount > plan->entityBatchCount
					|| indexCount > plan->entityIndexCount - 36u ) goto fail;
			for ( uint32_t end = 0u; end < 2u; ++end ) {
				const float *origin = end ? entity->oldorigin : entity->origin;
				SetEntityVertex( &vertices[vertexCount++], origin[0] - px,
					origin[1] - py, origin[2] - width, 0.0f, 0.0f,
					entity->shader.rgba );
				SetEntityVertex( &vertices[vertexCount++], origin[0] + px,
					origin[1] + py, origin[2] - width, 1.0f, 0.0f,
					entity->shader.rgba );
				SetEntityVertex( &vertices[vertexCount++], origin[0] + px,
					origin[1] + py, origin[2] + width, 1.0f, 1.0f,
					entity->shader.rgba );
				SetEntityVertex( &vertices[vertexCount++], origin[0] - px,
					origin[1] - py, origin[2] + width, 0.0f, 1.0f,
					entity->shader.rgba );
			}
			for ( uint32_t index = 0u; index < 36u; ++index )
				indices[indexCount++] = baseVertex + box[index];
			draw->firstIndex = indexCount - 36u; draw->indexCount = 36u;
			draw->material = entity->customShader;
			draw->contentDigest = HashU64( plan->frontendFrameDigest, i );
			memcpy( draw->localSh, localSh, sizeof( draw->localSh ) );
		}
	}
	if ( vertexCount != vertexCapacity || indexCount != plan->entityIndexCount
			|| drawCount != plan->entityBatchCount ) goto fail;
	*outVertices = vertices; *outVertexCount = vertexCount;
	*outIndices = indices; *outIndexCount = indexCount;
	*outDraws = draws; *outDrawCount = drawCount; return qtrue;
fail:
	free( vertices ); free( indices ); free( draws ); return qfalse;
}

typedef struct {
	ralWebGpuDrawKind_t kind;
	uint32_t effectFamily;
	uint32_t effectPipeline;
	uint32_t firstIndex;
	uint32_t indexCount;
	qhandle_t material;
	qboolean textured;
	qboolean msdf;
	uint64_t contentDigest;
	ralWebGpuEffectDrawParams_t effectParams;
	uint32_t effectParameterIndex;
	qboolean beamFirstStage;
} miscDrawPlan_t;

enum {
	MISC_EFFECT_FAMILY_NONE,
	MISC_EFFECT_FAMILY_POLYGON,
	MISC_EFFECT_FAMILY_LIGHT,
	MISC_EFFECT_FAMILY_SPRITE,
	MISC_EFFECT_FAMILY_DECAL,
	MISC_EFFECT_FAMILY_RIBBON,
	MISC_EFFECT_FAMILY_BEAM
};

static uint32_t EffectPipelineForFlags( int flags ) {
	const qboolean additive = ( flags & PRIM_FLAG_ADDITIVE ) ? qtrue : qfalse;
	const qboolean noDepth = ( flags & PRIM_FLAG_NO_DEPTH_TEST ) ? qtrue : qfalse;
	if ( noDepth ) return additive
		? RAL_WEBGPU_EFFECT_PIPELINE_ADDITIVE_NO_DEPTH
		: RAL_WEBGPU_EFFECT_PIPELINE_ALPHA_NO_DEPTH;
	return additive ? RAL_WEBGPU_EFFECT_PIPELINE_ADDITIVE_DEPTH
		: RAL_WEBGPU_EFFECT_PIPELINE_ALPHA_DEPTH;
}

static byte ColorByte( float value ) {
	if ( value <= 0.0f ) return 0u;
	if ( value >= 1.0f ) return 255u;
	return (byte)( value * 255.0f + 0.5f );
}

static qboolean AddCapacity( uint32_t *value, uint64_t add ) {
	if ( !value || add > UINT32_MAX - *value ) return qfalse;
	*value += (uint32_t)add; return qtrue;
}

static void ResolveEffectBeamEndpoint( const renderSubmissionState_t *frontend,
		const beamDesc_t *beam, qboolean start, float out[3] ) {
	const int entityNum = start ? beam->startEntityNum : beam->endEntityNum;
	const float *position = start ? beam->start : beam->end;
	const float *offset = start ? beam->startOffset : beam->endOffset;
	if ( entityNum >= 0 && (uint32_t)entityNum < frontend->entityCount ) {
		for ( uint32_t axis = 0u; axis < 3u; ++axis )
			out[axis] = frontend->entities[entityNum].entity.origin[axis]
				+ offset[axis];
	} else memcpy( out, position, 3u * sizeof( float ) );
}

static float EffectBeamFade( const renderEffectBeamSlot_t *slot, float now ) {
	float fade = 1.0f;
	const float age = slot ? fmaxf( now - slot->spawnTime, 0.0f ) : 0.0f;
	if ( !slot || slot->transient || slot->beam.duration <= 0.0f ) return 1.0f;
	if ( slot->beam.fadeIn > 0.0f ) fade = fminf( fade, age / slot->beam.fadeIn );
	if ( slot->beam.fadeOut > 0.0f ) fade = fminf( fade,
		fmaxf( slot->beam.duration - age, 0.0f ) / slot->beam.fadeOut );
	return fminf( 1.0f, fmaxf( 0.0f, fade ) );
}

static void RotateEffectBeamAxis( const float vector[3], const float axis[3],
		float angle, float out[3] ) {
	const float cosine = cosf( angle ), sine = sinf( angle );
	const float dot = vector[0] * axis[0] + vector[1] * axis[1]
		+ vector[2] * axis[2];
	const float cross[3] = { axis[1] * vector[2] - axis[2] * vector[1],
		axis[2] * vector[0] - axis[0] * vector[2],
		axis[0] * vector[1] - axis[1] * vector[0] };
	for ( uint32_t component = 0u; component < 3u; ++component )
		out[component] = vector[component] * cosine + cross[component] * sine
			+ axis[component] * dot * ( 1.0f - cosine );
}

static qboolean BuildMiscGeometry( const renderSubmissionState_t *frontend,
		const ralWebGpuFrontendPlanReceipt_t *plan,
		const renderEffectDecalPool_t *effectDecalPool,
		const renderEffectDecalPoolReceipt_t *effectDecalPoolReceipt,
		const renderEffectBeamPool_t *effectBeamPool,
		const renderEffectBeamPoolReceipt_t *effectBeamPoolReceipt,
		uint32_t viewportWidth, uint32_t viewportHeight,
		renderWorldVertex_t **outVertices, uint32_t *outVertexCount,
		uint32_t **outIndices, uint32_t *outIndexCount,
		miscDrawPlan_t **outDraws, uint32_t *outDrawCount ) {
	static const uint32_t quad[6] = { 0u, 1u, 2u, 0u, 2u, 3u };
	const renderPolyCommand_t *polygons;
	const polyVert_t *polyVertices;
	const renderLightCommand_t *lights;
	const renderUiPrimitive_t *ui;
	renderEffectPrimitiveSnapshot_t effectPrimitives;
	const spriteDesc_t *effectSprites;
	const emitterDesc_t *effectEmitters;
	const decalDesc_t *effectDecals;
	const renderEffectRibbonCommand_t *effectRibbons;
	const ribbonPoint_t *effectRibbonPoints;
	const beamDesc_t *effectBeams;
	renderWorldSnapshot_t effectView;
	renderWorldVertex_t *vertices = NULL;
	uint32_t *indices = NULL;
	miscDrawPlan_t *draws = NULL;
	uint32_t polygonCommandCount = 0u, polyVertexCount = 0u;
	uint32_t extraBeamDraws = 0u;
	uint32_t lightCount = 0u, uiCount = 0u;
	uint32_t vertexCapacity = 0u, indexCapacity = 0u, drawCapacity = 0u;
	uint32_t vertexCount = 0u, indexCount = 0u, drawCount = 0u;
	if ( !frontend || !plan || !outVertices || !outVertexCount || !outIndices
			|| !outIndexCount || !outDraws || !outDrawCount
			|| !effectDecalPool || !effectDecalPoolReceipt
			|| !RenderEffectDecalPool_ReceiptExact( effectDecalPool,
				effectDecalPoolReceipt )
			|| effectDecalPoolReceipt->frameGeneration != plan->planGeneration
			|| !effectBeamPool || !effectBeamPoolReceipt
			|| !RenderEffectBeamPool_ReceiptExact( effectBeamPool,
				effectBeamPoolReceipt )
			|| effectBeamPoolReceipt->frameGeneration != plan->planGeneration
			|| !RenderSubmission_EffectSnapshots( frontend, &polygons,
				&polygonCommandCount, &polyVertices, &polyVertexCount,
				&lights, &lightCount ) ) return qfalse;
	memset( &effectPrimitives, 0, sizeof( effectPrimitives ) );
	if ( !RenderSubmission_EffectPrimitiveSnapshots( frontend,
			&effectPrimitives, &effectSprites, &effectEmitters, &effectDecals,
			&effectRibbons, &effectRibbonPoints, &effectBeams )
			|| effectPrimitives.emitterCount
			|| effectPrimitives.decalCount
				!= effectDecalPoolReceipt->submittedCount
			|| effectPrimitives.beamCount != effectBeamPoolReceipt->submittedCount
			|| effectPrimitives.spriteCount != plan->effectSpriteCount
			|| effectPrimitives.ribbonCount != plan->effectRibbonCount
			|| ( effectPrimitives.spriteCount && !effectSprites )
			|| ( effectPrimitives.ribbonCount && !effectRibbons )
			|| ( effectPrimitives.ribbonPointCount && !effectRibbonPoints )
			|| ( plan->effectBatchCount
				&& !RenderSubmission_ViewSnapshot( frontend, &effectView ) ) )
		return qfalse;
	(void)effectEmitters; (void)effectDecals; (void)effectBeams;
	ui = RenderSubmission_UiPrimitives( frontend, &uiCount );
	if ( polygonCommandCount != plan->polygonBatchCount
			|| lightCount != plan->lightCount || uiCount != plan->uiPrimitiveCount
			|| ( uiCount && ( !viewportWidth || !viewportHeight ) )
			|| ( polygonCommandCount && ( !polygons || !polyVertices ) )
			|| ( lightCount && !lights ) || ( uiCount && !ui ) ) return qfalse;
	for ( uint32_t i = 0u; i < polygonCommandCount; ++i ) {
		uint64_t verticesInCommand = (uint64_t)polygons[i].verticesPerPolygon
			* polygons[i].polygonCount;
		uint64_t indicesInCommand;
		if ( polygons[i].verticesPerPolygon < 3u
				|| polygons[i].firstVertex > polyVertexCount
				|| verticesInCommand > polyVertexCount - polygons[i].firstVertex )
			return qfalse;
		indicesInCommand = (uint64_t)( polygons[i].verticesPerPolygon - 2u )
			* 3u * polygons[i].polygonCount;
		if ( !AddCapacity( &vertexCapacity, verticesInCommand )
				|| !AddCapacity( &indexCapacity, indicesInCommand ) ) return qfalse;
	}
	for ( uint32_t slotIndex = 0u;
			slotIndex < RENDER_SUBMISSION_MAX_EFFECT_DECALS; ++slotIndex ) {
		if ( !effectDecalPool->slots[slotIndex].active ) continue;
		if ( !AddCapacity( &vertexCapacity, 4u ) ) return qfalse;
	}
	for ( uint32_t slotIndex = 0u;
			slotIndex < RENDER_SUBMISSION_MAX_EFFECT_BEAMS; ++slotIndex ) {
		const renderEffectBeamSlot_t *slot = &effectBeamPool->slots[slotIndex];
		if ( !slot->active ) continue;
		renderMaterialSnapshot_t material;
		if (!RenderSubmission_MaterialSnapshot(frontend, slot->beam.shader, &material)
				|| material.stageCount > RENDER_MATERIAL_MAX_STAGES) return qfalse;
		if (material.stageCount > 1u) extraBeamDraws += material.stageCount - 1u;
		if ( !AddCapacity( &vertexCapacity,
				(uint64_t)(uint32_t)slot->beam.axialCopies * 4u ) ) return qfalse;
	}
	if ( !AddCapacity( &vertexCapacity, (uint64_t)( lightCount + uiCount ) * 4u )
			|| !AddCapacity( &indexCapacity,
				(uint64_t)( lightCount + uiCount ) * 6u )
			|| !AddCapacity( &vertexCapacity,
				(uint64_t)effectPrimitives.spriteCount * 4u
					+ (uint64_t)effectPrimitives.ribbonPointCount * 2u )
			|| !AddCapacity( &indexCapacity, plan->effectIndexCount )
			|| !AddCapacity( &drawCapacity,
				(uint64_t)polygonCommandCount + lightCount
					+ plan->effectBatchCount + uiCount + extraBeamDraws )
			|| drawCapacity != plan->polygonBatchCount + plan->lightCount
				+ plan->effectBatchCount + plan->uiPrimitiveCount + extraBeamDraws
			|| !vertexCapacity || !indexCapacity || !drawCapacity
			|| (uint64_t)vertexCapacity * sizeof( *vertices )
				> RAL_WEBGPU_MAX_RESOURCE_BYTES
			|| (uint64_t)indexCapacity * sizeof( *indices )
			> RAL_WEBGPU_MAX_RESOURCE_BYTES ) return qfalse;
	vertices = calloc( vertexCapacity, sizeof( *vertices ) );
	indices = calloc( indexCapacity, sizeof( *indices ) );
	draws = calloc( drawCapacity, sizeof( *draws ) );
	if ( !vertices || !indices || !draws ) goto fail;
	for ( uint32_t commandIndex = 0u; commandIndex < polygonCommandCount;
			++commandIndex ) {
		const renderPolyCommand_t *command = &polygons[commandIndex];
		miscDrawPlan_t *draw = &draws[drawCount++];
		draw->kind = RAL_WEBGPU_DRAW_EFFECT; draw->firstIndex = indexCount;
		draw->effectFamily = MISC_EFFECT_FAMILY_POLYGON;
		draw->effectPipeline = RAL_WEBGPU_EFFECT_PIPELINE_LEGACY;
		draw->material = command->material; draw->textured = qtrue;
		for ( uint32_t polygon = 0u; polygon < command->polygonCount; ++polygon ) {
			uint32_t baseVertex = vertexCount;
			uint32_t sourceBase = command->firstVertex
				+ polygon * command->verticesPerPolygon;
			for ( uint32_t vertex = 0u; vertex < command->verticesPerPolygon;
					++vertex ) {
				const polyVert_t *source = &polyVertices[sourceBase + vertex];
				SetEntityVertex( &vertices[vertexCount++], source->xyz[0],
					source->xyz[1], source->xyz[2], source->st[0], source->st[1],
					source->modulate.rgba );
			}
			for ( uint32_t triangle = 1u;
					triangle + 1u < command->verticesPerPolygon; ++triangle ) {
				indices[indexCount++] = baseVertex;
				indices[indexCount++] = baseVertex + triangle;
				indices[indexCount++] = baseVertex + triangle + 1u;
			}
		}
		draw->indexCount = indexCount - draw->firstIndex;
		draw->contentDigest = HashU64( plan->loweringDigest,
			( (uint64_t)RAL_WEBGPU_DRAW_EFFECT << 56u ) | commandIndex + 1u );
	}
	for ( uint32_t i = 0u; i < lightCount; ++i ) {
		byte color[4] = { ColorByte( lights[i].color[0] ),
			ColorByte( lights[i].color[1] ), ColorByte( lights[i].color[2] ), 255u };
		float radius = lights[i].intensity < 4096.0f ? lights[i].intensity : 4096.0f;
		uint32_t baseVertex = vertexCount;
		miscDrawPlan_t *draw = &draws[drawCount++];
		SetEntityVertex( &vertices[vertexCount++], lights[i].origin[0] - radius,
			lights[i].origin[1] - radius, lights[i].origin[2], 0.0f, 0.0f, color );
		SetEntityVertex( &vertices[vertexCount++], lights[i].origin[0] + radius,
			lights[i].origin[1] - radius, lights[i].origin[2], 1.0f, 0.0f, color );
		SetEntityVertex( &vertices[vertexCount++], lights[i].origin[0] + radius,
			lights[i].origin[1] + radius, lights[i].origin[2], 1.0f, 1.0f, color );
		SetEntityVertex( &vertices[vertexCount++], lights[i].origin[0] - radius,
			lights[i].origin[1] + radius, lights[i].origin[2], 0.0f, 1.0f, color );
		for ( uint32_t index = 0u; index < 6u; ++index )
			indices[indexCount++] = baseVertex + quad[index];
		draw->kind = RAL_WEBGPU_DRAW_EFFECT; draw->firstIndex = indexCount - 6u;
		draw->effectFamily = MISC_EFFECT_FAMILY_LIGHT;
		draw->effectPipeline = RAL_WEBGPU_EFFECT_PIPELINE_LEGACY;
		draw->indexCount = 6u; draw->textured = qfalse;
		draw->contentDigest = HashU64( plan->loweringDigest,
			( (uint64_t)RAL_WEBGPU_DRAW_EFFECT << 56u )
				| ( (uint64_t)1u << 48u ) | i + 1u );
	}
	for ( uint32_t i = 0u; i < effectPrimitives.spriteCount; ++i ) {
		static const float uv[4][2] = {
			{ 0.0f, 0.0f }, { 1.0f, 0.0f },
			{ 1.0f, 1.0f }, { 0.0f, 1.0f }
		};
		const spriteDesc_t *sprite = &effectSprites[i];
		byte color[4];
		const uint32_t baseVertex = vertexCount;
		miscDrawPlan_t *draw = &draws[drawCount++];
		for ( uint32_t channel = 0u; channel < 4u; ++channel )
			color[channel] = ColorByte( sprite->rgba[channel] );
		for ( uint32_t corner = 0u; corner < 4u; ++corner ) {
			const float leftSign = ( corner == 0u || corner == 3u ) ? 1.0f : -1.0f;
			const float upSign = corner < 2u ? 1.0f : -1.0f;
			float position[3];
			for ( uint32_t axis = 0u; axis < 3u; ++axis )
				position[axis] = sprite->origin[axis]
					+ effectView.viewAxis[1][axis] * sprite->radius * leftSign
					+ effectView.viewAxis[2][axis] * sprite->radius * upSign;
			SetEntityVertex( &vertices[vertexCount++], position[0], position[1],
				position[2], uv[corner][0], uv[corner][1], color );
		}
		for ( uint32_t index = 0u; index < 6u; ++index )
			indices[indexCount++] = baseVertex + quad[index];
		draw->kind = RAL_WEBGPU_DRAW_EFFECT;
		draw->effectFamily = MISC_EFFECT_FAMILY_SPRITE;
		draw->effectPipeline = EffectPipelineForFlags( sprite->flags );
		draw->firstIndex = indexCount - 6u; draw->indexCount = 6u;
		draw->material = sprite->shader; draw->textured = qtrue;
		draw->contentDigest = HashU64( plan->loweringDigest,
			( (uint64_t)RAL_WEBGPU_DRAW_EFFECT << 56u )
				| ( (uint64_t)2u << 48u ) | i + 1u );
	}
	for ( uint32_t slotIndex = 0u;
			slotIndex < RENDER_SUBMISSION_MAX_EFFECT_DECALS; ++slotIndex ) {
		static const float uv[4][2] = {
			{ 0.0f, 0.0f }, { 1.0f, 0.0f },
			{ 1.0f, 1.0f }, { 0.0f, 1.0f }
		};
		const renderEffectDecalSlot_t *slot = &effectDecalPool->slots[slotIndex];
		const decalDesc_t *decal;
		float seed[3], tangent[3], bitangent[3], tangentLength;
		float rotatedTangent[3], rotatedBitangent[3], corners[4][3];
		byte color[4];
		uint32_t baseVertex;
		miscDrawPlan_t *draw;
		if ( !slot->active ) continue;
		decal = &slot->decal;
		seed[0] = fabsf( decal->normal[2] ) < 0.9f ? 0.0f : 1.0f;
		seed[1] = 0.0f;
		seed[2] = fabsf( decal->normal[2] ) < 0.9f ? 1.0f : 0.0f;
		tangent[0] = decal->normal[1] * seed[2] - decal->normal[2] * seed[1];
		tangent[1] = decal->normal[2] * seed[0] - decal->normal[0] * seed[2];
		tangent[2] = decal->normal[0] * seed[1] - decal->normal[1] * seed[0];
		tangentLength = sqrtf( tangent[0] * tangent[0]
			+ tangent[1] * tangent[1] + tangent[2] * tangent[2] );
		if ( tangentLength < 0.00001f ) goto fail;
		for ( uint32_t axis = 0u; axis < 3u; ++axis )
			tangent[axis] /= tangentLength;
		bitangent[0] = decal->normal[1] * tangent[2]
			- decal->normal[2] * tangent[1];
		bitangent[1] = decal->normal[2] * tangent[0]
			- decal->normal[0] * tangent[2];
		bitangent[2] = decal->normal[0] * tangent[1]
			- decal->normal[1] * tangent[0];
		for ( uint32_t axis = 0u; axis < 3u; ++axis ) {
			const float cosine = cosf( decal->orientation );
			const float sine = sinf( decal->orientation );
			const float center = decal->origin[axis] + decal->normal[axis] * 0.25f;
			rotatedTangent[axis] = ( tangent[axis] * cosine
				+ bitangent[axis] * sine ) * decal->radius;
			rotatedBitangent[axis] = ( bitangent[axis] * cosine
				- tangent[axis] * sine ) * decal->radius;
			corners[0][axis] = center - rotatedTangent[axis]
				- rotatedBitangent[axis];
			corners[1][axis] = center + rotatedTangent[axis]
				- rotatedBitangent[axis];
			corners[2][axis] = center + rotatedTangent[axis]
				+ rotatedBitangent[axis];
			corners[3][axis] = center - rotatedTangent[axis]
				+ rotatedBitangent[axis];
		}
		for ( uint32_t channel = 0u; channel < 4u; ++channel )
			color[channel] = ColorByte( decal->rgba[channel] );
		baseVertex = vertexCount;
		for ( uint32_t corner = 0u; corner < 4u; ++corner )
			SetEntityVertex( &vertices[vertexCount++], corners[corner][0],
				corners[corner][1], corners[corner][2], uv[corner][0],
				uv[corner][1], color );
		for ( uint32_t index = 0u; index < 6u; ++index )
			indices[indexCount++] = baseVertex + quad[index];
		draw = &draws[drawCount++];
		draw->kind = RAL_WEBGPU_DRAW_EFFECT;
		draw->effectFamily = MISC_EFFECT_FAMILY_DECAL;
		draw->effectPipeline = RAL_WEBGPU_EFFECT_PIPELINE_ALPHA_DEPTH;
		draw->firstIndex = indexCount - 6u; draw->indexCount = 6u;
		draw->material = decal->shader; draw->textured = qtrue;
		draw->contentDigest = HashU64( plan->loweringDigest,
			( (uint64_t)RAL_WEBGPU_DRAW_EFFECT << 56u )
				| ( (uint64_t)5u << 48u ) | slotIndex + 1u );
	}
	for ( uint32_t i = 0u; i < effectPrimitives.ribbonCount; ++i ) {
		const renderEffectRibbonCommand_t *ribbon = &effectRibbons[i];
		const uint32_t firstVertex = vertexCount;
		miscDrawPlan_t *draw = &draws[drawCount++];
		for ( uint32_t pointIndex = 0u; pointIndex < ribbon->pointCount;
				++pointIndex ) {
			const ribbonPoint_t *point = &effectRibbonPoints[
				ribbon->firstPoint + pointIndex];
			const ribbonPoint_t *previous = &effectRibbonPoints[
				ribbon->firstPoint + ( pointIndex ? pointIndex - 1u : pointIndex )];
			const ribbonPoint_t *next = &effectRibbonPoints[
				ribbon->firstPoint + ( pointIndex + 1u < ribbon->pointCount
					? pointIndex + 1u : pointIndex )];
			float tangent[3], extrude[3], length;
			byte color[4];
			for ( uint32_t axis = 0u; axis < 3u; ++axis )
				tangent[axis] = next->pos[axis] - previous->pos[axis];
			if ( ribbon->flags & PRIM_FLAG_CUSTOM_NORMAL ) {
				memcpy( extrude, point->normal, sizeof( extrude ) );
			} else {
				const float *plane = ( ribbon->flags & PRIM_FLAG_VIEW_UP_PLANE )
					? effectView.viewAxis[2] : effectView.viewAxis[0];
				extrude[0] = tangent[1] * plane[2] - tangent[2] * plane[1];
				extrude[1] = tangent[2] * plane[0] - tangent[0] * plane[2];
				extrude[2] = tangent[0] * plane[1] - tangent[1] * plane[0];
			}
			length = sqrtf( extrude[0] * extrude[0] + extrude[1] * extrude[1]
				+ extrude[2] * extrude[2] );
			if ( length < 0.00001f ) goto fail;
			for ( uint32_t channel = 0u; channel < 4u; ++channel )
				color[channel] = ColorByte( point->rgba[channel] );
			for ( uint32_t side = 0u; side < 2u; ++side ) {
				const float sign = side ? -1.0f : 1.0f;
				float position[3];
				for ( uint32_t axis = 0u; axis < 3u; ++axis )
					position[axis] = point->pos[axis]
						+ extrude[axis] / length * point->width * sign;
				SetEntityVertex( &vertices[vertexCount++], position[0], position[1],
					position[2], (float)pointIndex
						/ (float)( ribbon->pointCount - 1u ), (float)side, color );
			}
		}
		for ( uint32_t segment = 0u; segment + 1u < ribbon->pointCount;
				++segment ) {
			const uint32_t base = firstVertex + segment * 2u;
			const uint32_t order[6] = { base, base + 1u, base + 2u,
				base + 2u, base + 1u, base + 3u };
			memcpy( indices + indexCount, order, sizeof( order ) );
			indexCount += 6u;
		}
		draw->kind = RAL_WEBGPU_DRAW_EFFECT;
		draw->effectFamily = MISC_EFFECT_FAMILY_RIBBON;
		draw->effectPipeline = EffectPipelineForFlags( ribbon->flags );
		draw->firstIndex = indexCount - ( ribbon->pointCount - 1u ) * 6u;
		draw->indexCount = ( ribbon->pointCount - 1u ) * 6u;
		draw->material = ribbon->material; draw->textured = qtrue;
		draw->contentDigest = HashU64( plan->loweringDigest,
			( (uint64_t)RAL_WEBGPU_DRAW_EFFECT << 56u )
				| ( (uint64_t)3u << 48u ) | i + 1u );
	}
	for ( uint32_t slotIndex = 0u;
			slotIndex < RENDER_SUBMISSION_MAX_EFFECT_BEAMS; ++slotIndex ) {
		const renderEffectBeamSlot_t *slot = &effectBeamPool->slots[slotIndex];
		const beamDesc_t *beam;
		float start[3], end[3], axis[3], baseExtrude[3];
		float age, fade;
		miscDrawPlan_t *draw;
		if ( !slot->active ) continue;
		beam = &slot->beam;
		ResolveEffectBeamEndpoint( frontend, beam, qtrue, start );
		ResolveEffectBeamEndpoint( frontend, beam, qfalse, end );
		if ( !wired_ral_beam_frame( start, end, effectView.viewOrigin,
				axis, baseExtrude ) ) goto fail;
		age = fmaxf( (float)effectView.timeMs * 0.001f - slot->spawnTime, 0.0f );
		/* Per-frame beams reset spawnTime on every submission. Their UV
		 * phase follows absolute scene time, as in canonical beam.vert;
		 * persistent lifetime/fade still uses age. */
		const float scrollTime = slot->transient
			? (float)effectView.timeMs * 0.001f : age;
		fade = EffectBeamFade( slot, (float)effectView.timeMs * 0.001f );
		draw = &draws[drawCount++];
		draw->kind = RAL_WEBGPU_DRAW_EFFECT;
		draw->effectFamily = MISC_EFFECT_FAMILY_BEAM;
		draw->effectPipeline = EffectPipelineForFlags( beam->flags );
		draw->firstIndex = indexCount;
		draw->material = beam->shader;
		draw->textured = qtrue;
		for ( uint32_t copy = 0u; copy < (uint32_t)beam->axialCopies; ++copy ) {
			float extrude[3], corners[4][3];
			byte startColor[4], endColor[4];
			const float angle = (float)M_PI * (float)copy
				/ (float)beam->axialCopies;
			const uint32_t baseVertex = vertexCount;
			RotateEffectBeamAxis( baseExtrude, axis, angle, extrude );
			for ( uint32_t component = 0u; component < 3u; ++component ) {
				corners[0][component] = start[component]
					- extrude[component] * beam->startWidth;
				corners[1][component] = start[component]
					+ extrude[component] * beam->startWidth;
				corners[2][component] = end[component]
					- extrude[component] * beam->endWidth;
				corners[3][component] = end[component]
					+ extrude[component] * beam->endWidth;
			}
			for ( uint32_t channel = 0u; channel < 4u; ++channel ) {
				const float startAlpha = beam->startColor[3] * fade;
				const float endAlpha = beam->endColor[3] * fade;
				startColor[channel] = ColorByte( channel == 3u ? startAlpha
					: beam->startColor[channel] );
				endColor[channel] = ColorByte( channel == 3u ? endAlpha
					: beam->endColor[channel] );
			}
			/* Both effect blends consume straight colour. The fragment samples
			 * texture alpha and fixed-function SRC_ALPHA applies coverage once. */
			SetEntityVertex( &vertices[vertexCount++], corners[0][0], corners[0][1],
				corners[0][2], 0.0f, 0.0f,
				startColor );
			SetEntityVertex( &vertices[vertexCount++], corners[1][0], corners[1][1],
				corners[1][2], 0.0f, 1.0f,
				startColor );
			SetEntityVertex( &vertices[vertexCount++], corners[2][0], corners[2][1],
				corners[2][2], 1.0f, 0.0f, endColor );
			SetEntityVertex( &vertices[vertexCount++], corners[3][0], corners[3][1],
				corners[3][2], 1.0f, 1.0f, endColor );
			/* Beam corners are a two-row strip (start-/+, end-/+), not
			 * perimeter order. The UI quad indices overlap half this strip. */
			static const uint32_t beamQuad[6] = { 0u, 1u, 2u, 2u, 1u, 3u };
			for ( uint32_t index = 0u; index < 6u; ++index )
				indices[indexCount++] = baseVertex + beamQuad[index];
		}
		draw->indexCount = indexCount - draw->firstIndex;
		draw->contentDigest = HashU64( plan->loweringDigest,
			( (uint64_t)RAL_WEBGPU_DRAW_EFFECT << 56u )
				| ( (uint64_t)4u << 48u ) | slotIndex + 1u );
		renderMaterialSnapshot_t material;
		if (!RenderSubmission_MaterialSnapshot(frontend, beam->shader, &material)) goto fail;
		const uint32_t stages = material.stageCount ? material.stageCount : 1u;
		const miscDrawPlan_t baseDraw = *draw;
		for (uint32_t s = 0u; s < stages; ++s) {
			if (s) { if (drawCount >= drawCapacity) goto fail; draw = &draws[drawCount++]; }
			*draw = baseDraw;
			draw->beamFirstStage = s == 0u;
			draw->effectParams.meta[0] = 1u;
			memcpy(draw->effectParams.startColor, beam->startColor, sizeof(beam->startColor));
			memcpy(draw->effectParams.endColor, beam->endColor, sizeof(beam->endColor));
			draw->effectParams.startColor[3] *= fade;
			draw->effectParams.endColor[3] *= fade;
			draw->effectParams.scaleOffset[0] = draw->effectParams.scaleOffset[1] = 1.0f;
			for (uint32_t axisIndex = 0u; axisIndex < 2u; ++axisIndex)
				draw->effectParams.scaleOffset[2u + axisIndex] = beam->uvScroll[axisIndex] * scrollTime;
			if (material.stageCount) {
				const renderMaterialStageSnapshot_t *stage = &material.stages[s];
				/* Canonical primitive stages accumulate radiance, independently
				 * of the surface blend program. Coverage is applied once by the
				 * existing SRC_ALPHA/ONE pipeline; preserve depth flags. */
				draw->effectPipeline = EffectPipelineForFlags(beam->flags | PRIM_FLAG_ADDITIVE);
				/* Primitive stages use their image and scale/scroll, not the
				 * world surface lighting/rgbGen program (canonical beam.vert). */
				if (stage->imageSource != 0u || stage->material <= 0) goto fail;
				draw->material = stage->material;
				if (stage->animationFrameCount && stage->animationFrequency > 0.0f) {
					double frame = fmax(floor((double)effectView.timeMs * 0.001 * stage->animationFrequency), 0.0);
					frame = stage->animationLooping ? fmod(frame, stage->animationFrameCount)
						: fmin(frame, stage->animationFrameCount - 1u);
					draw->material = stage->animationMaterials[(uint32_t)frame];
					if (draw->material <= 0) goto fail;
				}
				for (uint32_t axisIndex = 0u; axisIndex < 2u; ++axisIndex) {
					draw->effectParams.scaleOffset[axisIndex] = stage->scaleScroll[axisIndex];
					draw->effectParams.scaleOffset[2u + axisIndex] += stage->scaleScroll[2u + axisIndex] * scrollTime;
				}
			}
			draw->contentDigest = HashU64(baseDraw.contentDigest, s);
		}
	}
	for ( uint32_t i = 0u; i < uiCount; ++i ) {
		const renderUiPrimitive_t *primitive = &ui[i];
		renderMaterialSnapshot_t material;
		byte color[4]; float positions[4][2];
		float texCoords[4][2];
		uint32_t baseVertex = vertexCount;
		miscDrawPlan_t *draw = &draws[drawCount++];
		if ( !RenderSubmission_MaterialSnapshot( frontend, primitive->material,
				&material ) || !material.ready ) goto fail;
		for ( uint32_t component = 0u; component < 4u; ++component )
			color[component] = ColorByte( primitive->color[component] );
		memcpy( positions, primitive->positions, sizeof( positions ) );
		if ( primitive->kind == RENDER_UI_LINE ) {
			texCoords[0][0] = 0.0f; texCoords[0][1] = 0.0f;
			texCoords[1][0] = 1.0f; texCoords[1][1] = 0.0f;
			texCoords[2][0] = 1.0f; texCoords[2][1] = 1.0f;
			texCoords[3][0] = 0.0f; texCoords[3][1] = 1.0f;
		} else {
			texCoords[0][0] = primitive->s1; texCoords[0][1] = primitive->t1;
			texCoords[1][0] = primitive->s2; texCoords[1][1] = primitive->t1;
			texCoords[2][0] = primitive->s2; texCoords[2][1] = primitive->t2;
			texCoords[3][0] = primitive->s1; texCoords[3][1] = primitive->t2;
		}
		for ( uint32_t corner = 0u; corner < 4u; ++corner ) {
			positions[corner][0] = positions[corner][0]
				/ (float)viewportWidth * 2.0f - 1.0f;
			positions[corner][1] = 1.0f - positions[corner][1]
				/ (float)viewportHeight * 2.0f;
		}
		SetEntityVertex( &vertices[vertexCount++], positions[0][0], positions[0][1],
			0.0f, texCoords[0][0], texCoords[0][1], color );
		SetEntityVertex( &vertices[vertexCount++], positions[1][0], positions[1][1],
			0.0f, texCoords[1][0], texCoords[1][1], color );
		SetEntityVertex( &vertices[vertexCount++], positions[2][0], positions[2][1],
			0.0f, texCoords[2][0], texCoords[2][1], color );
		SetEntityVertex( &vertices[vertexCount++], positions[3][0], positions[3][1],
			0.0f, texCoords[3][0], texCoords[3][1], color );
		for ( uint32_t index = 0u; index < 6u; ++index )
			indices[indexCount++] = baseVertex + quad[index];
		draw->kind = RAL_WEBGPU_DRAW_UI; draw->firstIndex = indexCount - 6u;
		draw->indexCount = 6u; draw->material = primitive->material;
		draw->textured = qtrue; draw->msdf = material.msdf;
		draw->contentDigest = HashU64( plan->loweringDigest,
			( (uint64_t)RAL_WEBGPU_DRAW_UI << 56u ) | i + 1u );
	}
	if ( vertexCount != vertexCapacity || indexCount != indexCapacity
			|| drawCount != drawCapacity ) goto fail;
	*outVertices = vertices; *outVertexCount = vertexCount;
	*outIndices = indices; *outIndexCount = indexCount;
	*outDraws = draws; *outDrawCount = drawCount; return qtrue;
fail:
	free( vertices ); free( indices ); free( draws ); return qfalse;
}

qboolean RalWebGpu_ProductCreate( ralWebGpuRuntime_t *runtime,
		const ralWebGpuRuntimeReceipt_t *runtimeReceipt,
		const ralWebGpuPipelineReceipt_t *worldPipeline, uint64_t generation,
		ralWebGpuProduct_t **outProduct ) {
	ralWebGpuProduct_t *product;
	ralWebGpuResourceLayer_t *resources;
	ralWebGpuBufferDesc_t lightingDesc;
	if ( !runtime || !runtimeReceipt || !worldPipeline || !outProduct
			|| !generation || generation == UINT64_MAX
			|| worldPipeline->kind != RAL_SHADER_PIPELINE_GRAPHICS
			|| !RalWebGpu_RuntimeOwnsPipeline( runtime, runtimeReceipt,
				worldPipeline ) ) return qfalse;
	product = (ralWebGpuProduct_t *)calloc( 1u, sizeof( *product ) );
	if ( !product ) return qfalse;
	product->runtime = runtime; product->worldPipeline = *worldPipeline;
	product->generation = generation; product->planGeneration = generation;
	product->lightmapBoost = 4.6f;
	RenderEffectDecalPool_Reset( &product->effectDecalPool );
	RenderEffectDecalPool_Reset( &product->pendingEffectDecalPool );
	RenderEffectBeamPool_Reset( &product->effectBeamPool );
	RenderEffectBeamPool_Reset( &product->pendingEffectBeamPool );
	resources = RalWebGpu_RuntimeResources( runtime, runtimeReceipt );
	memset( &lightingDesc, 0, sizeof( lightingDesc ) );
	lightingDesc.size = RAL_WEBGPU_ENTITY_LIGHTING_BYTES;
	lightingDesc.usage = RAL_WEBGPU_BUFFER_STORAGE
		| RAL_WEBGPU_BUFFER_COPY_DESTINATION;
	lightingDesc.memoryClass = RAL_ALLOCATION_DEVICE_LOCAL;
	if ( !resources || !RalWebGpu_CreateBuffer( resources, &lightingDesc,
			&product->entityLighting, &product->entityLightingReceipt ) ) {
		free( product ); return qfalse;
	}
	lightingDesc.size = RAL_WEBGPU_WORLD_MATERIAL_BYTES;
	if ( !RalWebGpu_CreateBuffer( resources, &lightingDesc,
			&product->worldMaterials, &product->worldMaterialReceipt ) ) {
		RalWebGpu_DestroyResource( resources, product->entityLighting,
			&product->entityLightingReceipt );
		free( product ); return qfalse;
	}
	{
		ralAtmosphereWeatherRequest_t emptyWeather;
		memset( &emptyWeather, 0, sizeof( emptyWeather ) );
		emptyWeather.schemaVersion = RAL_ATMOSPHERE_WEATHER_SCHEMA_VERSION;
		emptyWeather.tier = RAL_ATMOSPHERE_TIER_OFF;
		emptyWeather.maxParticles = RAL_WEBGPU_WEATHER_PARTICLE_CAPACITY;
		if ( !Ral_AtmospherePlanWeather( &emptyWeather,
				&product->weatherPlan ) ) {
			RalWebGpu_DestroyResource( resources, product->worldMaterials,
				&product->worldMaterialReceipt );
			RalWebGpu_DestroyResource( resources, product->entityLighting,
				&product->entityLightingReceipt );
			free( product ); return qfalse;
		}
	}
	lightingDesc.size = RAL_WEBGPU_EFFECT_PARAMETER_BYTES;
	if (!RalWebGpu_CreateBuffer(resources, &lightingDesc,
			&product->effectParameters, &product->effectParameterReceipt)) {
		RalWebGpu_ProductDestroy(product, runtimeReceipt); return qfalse;
	}
	*outProduct = product; return qtrue;
}

qboolean RalWebGpu_ProductGetEffectParameterBuffer(const ralWebGpuProduct_t *product,
	const ralWebGpuRuntimeReceipt_t *rr, uintptr_t *identity, uint64_t *bytes) {
	if (!product || !rr || !identity || !bytes || product->inFlight
			|| !RalWebGpu_RuntimeResources(product->runtime, rr)
			|| !product->effectParameters) return qfalse;
	*identity = product->effectParameterReceipt.resourceIdentity;
	*bytes = product->effectParameterReceipt.byteSize;
	return qtrue;
}

qboolean RalWebGpu_ProductGetWorldMaterialBuffer(
		const ralWebGpuProduct_t *product,
		const ralWebGpuRuntimeReceipt_t *runtimeReceipt,
		uintptr_t *outIdentity, uint64_t *outByteSize ) {
	if ( !product || !runtimeReceipt || !outIdentity || !outByteSize
			|| !product->worldMaterials
			|| !RalWebGpu_ResourceReceiptExact( &product->worldMaterialReceipt,
				&product->worldMaterialReceipt )
			|| !RalWebGpu_RuntimeOwnsPipeline( product->runtime, runtimeReceipt,
				&product->worldPipeline ) ) return qfalse;
	*outIdentity = product->worldMaterialReceipt.resourceIdentity;
	*outByteSize = product->worldMaterialReceipt.byteSize;
	return *outIdentity && *outByteSize == RAL_WEBGPU_WORLD_MATERIAL_BYTES;
}

qboolean RalWebGpu_ProductSetWorldMaterialBindGroup(
		ralWebGpuProduct_t *product,
		const ralWebGpuRuntimeReceipt_t *runtimeReceipt,
		uintptr_t bindGroupIdentity ) {
	if ( !product || !runtimeReceipt || !bindGroupIdentity || product->inFlight
			|| !RalWebGpu_RuntimeOwnsPipeline( product->runtime, runtimeReceipt,
				&product->worldPipeline ) ) return qfalse;
	product->worldMaterialBindGroup = bindGroupIdentity;
	product->worldMaterialBindGroupReady = qtrue;
	return qtrue;
}

qboolean RalWebGpu_ProductSetEntityPipeline( ralWebGpuProduct_t *product,
		const ralWebGpuRuntimeReceipt_t *runtimeReceipt,
		const ralWebGpuPipelineReceipt_t *entityPipeline ) {
	if ( !product || !runtimeReceipt || !entityPipeline || product->inFlight
			|| entityPipeline->kind != RAL_SHADER_PIPELINE_GRAPHICS
			|| !RalWebGpu_RuntimeOwnsPipeline( product->runtime, runtimeReceipt,
				entityPipeline ) ) return qfalse;
	product->entityPipeline = *entityPipeline;
	product->entityPipelineReady = qtrue; return qtrue;
}

qboolean RalWebGpu_ProductSetTaaMaskPipeline( ralWebGpuProduct_t *product,
		const ralWebGpuRuntimeReceipt_t *runtimeReceipt,
		const ralWebGpuPipelineReceipt_t *maskPipeline ) {
	if ( !product || !runtimeReceipt || !maskPipeline || product->inFlight
			|| maskPipeline->kind != RAL_SHADER_PIPELINE_GRAPHICS
			|| !RalWebGpu_RuntimeOwnsPipeline( product->runtime, runtimeReceipt, maskPipeline ) ) return qfalse;
	product->taaMaskPipeline = *maskPipeline;
	product->taaMaskPipelineReady = qtrue; return qtrue;
}

qboolean RalWebGpu_ProductSetEntityBlendPipelines( ralWebGpuProduct_t *product,
		const ralWebGpuRuntimeReceipt_t *runtimeReceipt,
		const ralWebGpuPipelineReceipt_t entityBlend[3], const ralWebGpuPipelineReceipt_t taaTrust[3] ) {
	if ( !product || !runtimeReceipt || !entityBlend || !taaTrust || product->inFlight ) return qfalse;
	for ( uint32_t i = 0u; i < 3u; ++i ) {
		if ( entityBlend[i].kind != RAL_SHADER_PIPELINE_GRAPHICS || taaTrust[i].kind != RAL_SHADER_PIPELINE_GRAPHICS
				|| !RalWebGpu_RuntimeOwnsPipeline( product->runtime, runtimeReceipt, &entityBlend[i] )
				|| !RalWebGpu_RuntimeOwnsPipeline( product->runtime, runtimeReceipt, &taaTrust[i] ) ) return qfalse;
		product->entityBlendPipelines[i] = entityBlend[i];
		product->taaTrustPipelines[i] = taaTrust[i];
	}
	product->entityBlendPipelinesReady = qtrue; return qtrue;
}

/* 0 for depth-writing entity batches, 1..3 (blend, alpha-additive, additive) for translucent ones, from the batch material. */
static uint32_t EntityBlendIndex( const renderSubmissionState_t *frontend, qhandle_t material ) {
	renderMaterialSnapshot_t snapshot;
	if ( material <= 0 || !RenderSubmission_MaterialSnapshot( frontend, material, &snapshot ) ) return 0u;
	switch ( snapshot.alphaMode ) {
	case RENDER_ALPHA_BLEND: return 1u;
	case RENDER_ALPHA_ALPHA_ADDITIVE: return 2u;
	case RENDER_ALPHA_ADDITIVE: return 3u;
	default: return 0u;
	}
}

qboolean RalWebGpu_ProductGetEntityLightingBuffer(
		const ralWebGpuProduct_t *product,
		const ralWebGpuRuntimeReceipt_t *runtimeReceipt,
		uintptr_t *outIdentity, uint64_t *outByteSize ) {
	if ( !product || !runtimeReceipt || !outIdentity || !outByteSize
			|| !product->entityLighting
			|| !RalWebGpu_ResourceReceiptExact( &product->entityLightingReceipt,
				&product->entityLightingReceipt )
			|| !RalWebGpu_RuntimeOwnsPipeline( product->runtime, runtimeReceipt,
				&product->worldPipeline ) ) return qfalse;
	*outIdentity = product->entityLightingReceipt.resourceIdentity;
	*outByteSize = product->entityLightingReceipt.byteSize;
	return *outIdentity && *outByteSize == RAL_WEBGPU_ENTITY_LIGHTING_BYTES;
}

qboolean RalWebGpu_ProductSetEntityLightingBindGroup(
		ralWebGpuProduct_t *product,
		const ralWebGpuRuntimeReceipt_t *runtimeReceipt,
		uintptr_t bindGroupIdentity ) {
	if ( !product || !runtimeReceipt || !bindGroupIdentity || product->inFlight
			|| !product->entityPipelineReady
			|| !RalWebGpu_RuntimeOwnsPipeline( product->runtime, runtimeReceipt,
				&product->entityPipeline ) ) return qfalse;
	product->entityLightingBindGroup = bindGroupIdentity;
	product->entityLightingBindGroupReady = qtrue;
	return qtrue;
}

qboolean RalWebGpu_ProductSetEffectPipelines( ralWebGpuProduct_t *product,
		const ralWebGpuRuntimeReceipt_t *runtimeReceipt,
		const ralWebGpuPipelineReceipt_t
			effectPipelines[RAL_WEBGPU_EFFECT_PIPELINE_COUNT] ) {
	if ( !product || !runtimeReceipt || !effectPipelines || product->inFlight )
		return qfalse;
	for ( uint32_t i = 0u; i < RAL_WEBGPU_EFFECT_PIPELINE_COUNT; ++i ) {
		if ( effectPipelines[i].kind != RAL_SHADER_PIPELINE_GRAPHICS
				|| !RalWebGpu_RuntimeOwnsPipeline( product->runtime, runtimeReceipt,
					&effectPipelines[i] ) ) return qfalse;
	}
	memcpy( product->effectPipelines, effectPipelines,
		sizeof( product->effectPipelines ) );
	product->effectPipelinesReady = qtrue; return qtrue;
}

qboolean RalWebGpu_ProductSetUiPipelines( ralWebGpuProduct_t *product,
		const ralWebGpuRuntimeReceipt_t *runtimeReceipt,
		const ralWebGpuPipelineReceipt_t *uiPipeline,
		const ralWebGpuPipelineReceipt_t *msdfPipeline ) {
	if ( !product || !runtimeReceipt || !uiPipeline || !msdfPipeline
			|| product->inFlight
			|| uiPipeline->kind != RAL_SHADER_PIPELINE_GRAPHICS
			|| msdfPipeline->kind != RAL_SHADER_PIPELINE_GRAPHICS
			|| !RalWebGpu_RuntimeOwnsPipeline( product->runtime, runtimeReceipt,
				uiPipeline )
			|| !RalWebGpu_RuntimeOwnsPipeline( product->runtime, runtimeReceipt,
				msdfPipeline ) ) return qfalse;
	product->uiPipeline = *uiPipeline; product->msdfPipeline = *msdfPipeline;
	product->uiPipelinesReady = qtrue; return qtrue;
}

qboolean RalWebGpu_ProductSetAtmosphereBindGroups(
		ralWebGpuProduct_t *product,
		const ralWebGpuRuntimeReceipt_t *runtimeReceipt,
		uintptr_t worldBindGroup, uintptr_t entityBindGroup,
		const uintptr_t effectBindGroups[RAL_WEBGPU_EFFECT_PIPELINE_COUNT] ) {
	if ( !product || !runtimeReceipt || product->inFlight || !worldBindGroup
			|| !entityBindGroup || !effectBindGroups
			|| !RalWebGpu_RuntimeOwnsPipeline( product->runtime, runtimeReceipt,
				&product->worldPipeline ) ) return qfalse;
	for ( uint32_t i = 0u; i < RAL_WEBGPU_EFFECT_PIPELINE_COUNT; ++i )
		if ( !effectBindGroups[i] ) return qfalse;
	product->atmosphereBindGroups[0] = worldBindGroup;
	product->atmosphereBindGroups[1] = entityBindGroup;
	memcpy( &product->atmosphereBindGroups[2], effectBindGroups,
		RAL_WEBGPU_EFFECT_PIPELINE_COUNT * sizeof( effectBindGroups[0] ) );
	product->atmosphereBindGroupsReady = qtrue;
	return qtrue;
}

qboolean RalWebGpu_ProductSetViewport( ralWebGpuProduct_t *product,
		const ralWebGpuRuntimeReceipt_t *runtimeReceipt,
		uint32_t width, uint32_t height ) {
	if ( !product || !runtimeReceipt || !width || !height || product->inFlight
			|| !RalWebGpu_RuntimeOwnsPipeline( product->runtime, runtimeReceipt,
				&product->worldPipeline ) ) return qfalse;
	product->viewportWidth = width; product->viewportHeight = height;
	return qtrue;
}

qboolean RalWebGpu_ProductSetLightmapBoost( ralWebGpuProduct_t *product,
		float lightmapBoost ) {
	if ( !product || product->inFlight || !isfinite( lightmapBoost )
			|| lightmapBoost < 1.0f || lightmapBoost > 24.0f ) return qfalse;
	product->lightmapBoost = lightmapBoost;
	return qtrue;
}

qboolean RalWebGpu_ProductSetWeather( ralWebGpuProduct_t *product,
		const ralWebGpuRuntimeReceipt_t *runtimeReceipt,
		const ralAtmosphereWeatherReceipt_t *plan,
		const ralWebGpuWeatherReceipt_t *execution ) {
	if ( !product || !runtimeReceipt || !plan || product->inFlight
			|| !Ral_AtmosphereWeatherReceiptExact( plan, plan ) ) return qfalse;
	if ( plan->zeroWork ) {
		if ( execution ) return qfalse;
		product->weatherPlan = *plan;
		memset( &product->weatherExecution, 0,
			sizeof( product->weatherExecution ) );
		product->weatherReady = qfalse;
		return qtrue;
	}
	if ( !execution || !RalWebGpu_WeatherReceiptExact( execution, execution )
			|| !Ral_AtmosphereWeatherReceiptExact( plan, &execution->plan )
			|| !RalWebGpu_RuntimeOwnsPipeline( product->runtime, runtimeReceipt,
				&execution->renderPipeline ) ) return qfalse;
	product->weatherPlan = *plan;
	product->weatherExecution = *execution;
	product->weatherReady = qtrue;
	return qtrue;
}

static void DestroySkyRadianceResources( ralWebGpuProduct_t *product,
		ralWebGpuResourceLayer_t *resources ) {
	if ( !product || !resources ) return;
	if ( product->skySource )
		RalWebGpu_DestroyResource( resources, product->skySource,
			&product->skySourceReceipt );
	for ( uint32_t i = 0u; i < 2u; ++i ) {
		if ( product->skyIrradiance[i] )
			RalWebGpu_DestroyResource( resources, product->skyIrradiance[i],
				&product->skyIrradianceReceipts[i] );
		if ( product->skyRadiance[i] )
			RalWebGpu_DestroyResource( resources, product->skyRadiance[i],
				&product->skyRadianceReceipts[i] );
		if ( product->skySh[i] )
			RalWebGpu_DestroyResource( resources, product->skySh[i],
				&product->skyShReceipts[i] );
	}
	product->skySource = NULL;
	memset( product->skyIrradiance, 0, sizeof( product->skyIrradiance ) );
	memset( product->skyRadiance, 0, sizeof( product->skyRadiance ) );
	memset( product->skySh, 0, sizeof( product->skySh ) );
	memset( &product->skySourceReceipt, 0,
		sizeof( product->skySourceReceipt ) );
	memset( product->skyIrradianceReceipts, 0,
		sizeof( product->skyIrradianceReceipts ) );
	memset( product->skyRadianceReceipts, 0,
		sizeof( product->skyRadianceReceipts ) );
	memset( product->skyShReceipts, 0,
		sizeof( product->skyShReceipts ) );
	product->skyResourceWorldLifecycleGeneration = 0u;
	product->skyResourcesReady = qfalse;
}

qboolean RalWebGpu_ProductEnsureSkyRadianceResources(
		ralWebGpuProduct_t *product,
		const ralWebGpuRuntimeReceipt_t *runtimeReceipt,
		uint64_t worldLifecycleGeneration ) {
	ralWebGpuResourceLayer_t *resources;
	ralWebGpuResource_t *source = NULL, *irradiance[2] = { NULL, NULL };
	ralWebGpuResource_t *radiance[2] = { NULL, NULL }, *sh[2] = { NULL, NULL };
	ralWebGpuResourceReceipt_t sourceReceipt, irradianceReceipts[2];
	ralWebGpuResourceReceipt_t radianceReceipts[2], shReceipts[2];
	ralWebGpuTextureDesc_t textureDesc;
	ralWebGpuBufferDesc_t bufferDesc;
	if ( !product || !runtimeReceipt || product->inFlight
			|| !worldLifecycleGeneration
			|| worldLifecycleGeneration == UINT64_MAX ) return qfalse;
	if ( product->skyResourcesReady
			&& product->skyResourceWorldLifecycleGeneration
				== worldLifecycleGeneration ) return qtrue;
	if(product->skyCaptureActive) return qfalse;
	resources = RalWebGpu_RuntimeResources( product->runtime, runtimeReceipt );
	if ( !resources ) return qfalse;
	memset( &sourceReceipt, 0, sizeof( sourceReceipt ) );
	memset( irradianceReceipts, 0, sizeof( irradianceReceipts ) );
	memset( radianceReceipts, 0, sizeof( radianceReceipts ) );
	memset( shReceipts, 0, sizeof( shReceipts ) );
	memset( &textureDesc, 0, sizeof( textureDesc ) );
	textureDesc.width = textureDesc.height = WIRED_SKY_SOURCE_SIZE;
	textureDesc.depth = RAL_SKY_RADIANCE_CAPTURE_FACE_COUNT;
	textureDesc.mipLevelCount = 1u;
	textureDesc.format = RAL_FORMAT_R16G16B16A16_SFLOAT;
	textureDesc.bytesPerTexel = 8u;
	textureDesc.usage = RAL_WEBGPU_TEXTURE_COPY_SOURCE
		| RAL_WEBGPU_TEXTURE_COPY_DESTINATION | RAL_WEBGPU_TEXTURE_SAMPLED
		| RAL_WEBGPU_TEXTURE_STORAGE | RAL_WEBGPU_TEXTURE_RENDER_ATTACHMENT;
	if ( !RalWebGpu_CreateTexture( resources, &textureDesc, &source,
			&sourceReceipt ) ) goto fail;
	textureDesc.usage &= ~RAL_WEBGPU_TEXTURE_RENDER_ATTACHMENT;
	textureDesc.width = textureDesc.height = WIRED_SKY_IRRADIANCE_SIZE;
	for ( uint32_t i = 0u; i < 2u; ++i )
		if ( !RalWebGpu_CreateTexture( resources, &textureDesc, &irradiance[i],
				&irradianceReceipts[i] ) ) goto fail;
	textureDesc.width = textureDesc.height = WIRED_SKY_SPECULAR_SIZE;
	textureDesc.mipLevelCount = WIRED_SKY_SPECULAR_MIP_COUNT;
	for ( uint32_t i = 0u; i < 2u; ++i )
		if ( !RalWebGpu_CreateTexture( resources, &textureDesc, &radiance[i],
				&radianceReceipts[i] ) ) goto fail;
	memset( &bufferDesc, 0, sizeof( bufferDesc ) );
	bufferDesc.size = RAL_SKY_RADIANCE_AMBIENT_SH_COEFFICIENT_COUNT
		* 4u * sizeof( float );
	bufferDesc.usage = RAL_WEBGPU_BUFFER_STORAGE
		| RAL_WEBGPU_BUFFER_COPY_DESTINATION;
	bufferDesc.memoryClass = RAL_ALLOCATION_DEVICE_LOCAL;
	for ( uint32_t i = 0u; i < 2u; ++i )
		if ( !RalWebGpu_CreateBuffer( resources, &bufferDesc, &sh[i],
				&shReceipts[i] ) ) goto fail;

	DestroySkyRadianceResources( product, resources );
	product->skySource = source;
	product->skySourceReceipt = sourceReceipt;
	for ( uint32_t i = 0u; i < 2u; ++i ) {
		product->skyIrradiance[i] = irradiance[i];
		product->skyIrradianceReceipts[i] = irradianceReceipts[i];
		product->skyRadiance[i] = radiance[i];
		product->skyRadianceReceipts[i] = radianceReceipts[i];
		product->skySh[i] = sh[i];
		product->skyShReceipts[i] = shReceipts[i];
	}
	product->skyResourceWorldLifecycleGeneration = worldLifecycleGeneration;
	product->skyResourcesReady = qtrue;
	return qtrue;

fail:
	if ( source ) RalWebGpu_DestroyResource( resources, source, &sourceReceipt );
	for ( uint32_t i = 0u; i < 2u; ++i ) {
		if ( irradiance[i] ) RalWebGpu_DestroyResource( resources, irradiance[i],
			&irradianceReceipts[i] );
		if ( radiance[i] ) RalWebGpu_DestroyResource( resources, radiance[i],
			&radianceReceipts[i] );
		if ( sh[i] ) RalWebGpu_DestroyResource( resources, sh[i], &shReceipts[i] );
	}
	return qfalse;
}

qboolean RalWebGpu_ProductUnloadSkyRadianceResources(
		ralWebGpuProduct_t *product,
		const ralWebGpuRuntimeReceipt_t *runtimeReceipt ) {
	ralWebGpuResourceLayer_t *resources;
	if ( !product || !runtimeReceipt || product->inFlight || product->skyCaptureActive ) return qfalse;
	resources = RalWebGpu_RuntimeResources( product->runtime, runtimeReceipt );
	if ( !resources ) return qfalse;
	RalWebGpu_SkyMaterialReset(product->skyCaptureMaterial,runtimeReceipt);
	DestroySkyRadianceResources( product, resources );
	return qtrue;
}

qboolean RalWebGpu_ProductSkyRadianceResources(
		ralWebGpuProduct_t *product,
		const ralWebGpuRuntimeReceipt_t *runtimeReceipt,
		const renderSubmissionState_t *frontend,
		ralWebGpuSkyRadianceResources_t *outResources ) {
	renderMaterialSnapshot_t material;
	char semanticName[MAX_QPATH];
	if ( !product || !runtimeReceipt || !frontend || !outResources
			|| product->inFlight || !product->skyResourcesReady
			|| !RalWebGpu_RuntimeResources( product->runtime,
				runtimeReceipt ) ) return qfalse;
	memset( outResources, 0, sizeof( *outResources ) );
	outResources->schemaVersion =
		RAL_WEBGPU_SKY_RADIANCE_RESOURCES_SCHEMA_VERSION;
	outResources->backendType = RAL_BACKEND_WEBGPU;
	outResources->productGeneration = product->generation;
	outResources->worldLifecycleGeneration =
		product->skyResourceWorldLifecycleGeneration;
	outResources->sourceTextureIdentity =
		product->skySourceReceipt.resourceIdentity;
	outResources->currentIrradianceIdentity =
		product->skyIrradianceReceipts[0].resourceIdentity;
	outResources->targetIrradianceIdentity =
		product->skyIrradianceReceipts[1].resourceIdentity;
	outResources->currentRadianceIdentity =
		product->skyRadianceReceipts[0].resourceIdentity;
	outResources->targetRadianceIdentity =
		product->skyRadianceReceipts[1].resourceIdentity;
	outResources->currentShIdentity = product->skyShReceipts[0].resourceIdentity;
	outResources->targetShIdentity = product->skyShReceipts[1].resourceIdentity;
	memset( &material, 0, sizeof( material ) );
	memset( semanticName, 0, sizeof( semanticName ) );
	if ( RenderSubmission_ActiveSkyMaterial( frontend, &material, semanticName )
			&& material.authoredSkySource.outerBox ) {
		/* Only an authored outer cube requires six resident faces. Stage-only
		 * skies use the shared authored radiance receipt, not cube textures. */
		if ( !material.authoredSkyTexturesReady ) return qfalse;
		for ( uint32_t face = 0u; face < RAL_MATERIAL_SKY_FACE_COUNT; ++face ) {
			materialEntry_t *entry = FindMaterial( product,
				material.authoredSkyOuterTextures[face] );
			if ( !entry || !entry->ready ) return qfalse;
			outResources->authoredTextureIdentities[face] =
				entry->textureReceipt.resourceIdentity;
			if ( face == 0u ) outResources->authoredSamplerIdentity =
				entry->samplerReceipt.resourceIdentity;
		}
		outResources->authoredReady = qtrue;
	}
	return outResources->sourceTextureIdentity
		&& outResources->targetIrradianceIdentity
		&& outResources->targetRadianceIdentity
		&& outResources->targetShIdentity;
}

qboolean RalWebGpu_ProductPublishSkyRadiance(
		ralWebGpuProduct_t *product,
		const ralWebGpuRuntimeReceipt_t *runtimeReceipt ) {
	ralWebGpuResource_t *resourceSwap;
	ralWebGpuResourceReceipt_t receiptSwap;
	if ( !product || !runtimeReceipt || product->inFlight
			|| !product->skyResourcesReady
			|| !RalWebGpu_RuntimeResources( product->runtime,
				runtimeReceipt ) ) return qfalse;
	resourceSwap = product->skyIrradiance[0];
	product->skyIrradiance[0] = product->skyIrradiance[1];
	product->skyIrradiance[1] = resourceSwap;
	receiptSwap = product->skyIrradianceReceipts[0];
	product->skyIrradianceReceipts[0] = product->skyIrradianceReceipts[1];
	product->skyIrradianceReceipts[1] = receiptSwap;
	resourceSwap = product->skyRadiance[0];
	product->skyRadiance[0] = product->skyRadiance[1];
	product->skyRadiance[1] = resourceSwap;
	receiptSwap = product->skyRadianceReceipts[0];
	product->skyRadianceReceipts[0] = product->skyRadianceReceipts[1];
	product->skyRadianceReceipts[1] = receiptSwap;
	resourceSwap = product->skySh[0];
	product->skySh[0] = product->skySh[1];
	product->skySh[1] = resourceSwap;
	receiptSwap = product->skyShReceipts[0];
	product->skyShReceipts[0] = product->skyShReceipts[1];
	product->skyShReceipts[1] = receiptSwap;
	return qtrue;
}

qboolean RalWebGpu_ProductEnableExposure( ralWebGpuProduct_t *product,
        ralWebGpuRuntimeReceipt_t *rr, ralWebGpuBrowserBridge_t *bridge, uint64_t generation ) {
    if (!product || product->inFlight || product->exposure) return qfalse;
    return RalWebGpu_ExposureCreate(product->runtime,rr,bridge,generation,&product->exposure);
}
qboolean RalWebGpu_ProductEnableTaa( ralWebGpuProduct_t *product,
        ralWebGpuRuntimeReceipt_t *rr, ralWebGpuBrowserBridge_t *bridge, uint64_t generation ) {
    if (!product || product->inFlight || product->taa) return qfalse;
    Ral_TemporalInit( &product->taaState );
    return RalWebGpu_TaaCreate(product->runtime,rr,bridge,generation,&product->taa);
}
qboolean RalWebGpu_ProductEnableSmaa( ralWebGpuProduct_t *product,
        ralWebGpuRuntimeReceipt_t *rr, ralWebGpuBrowserBridge_t *bridge, uint64_t generation ) {
    if (!product || product->inFlight || product->smaa) return qfalse;
    return RalWebGpu_SmaaCreate(product->runtime,rr,bridge,generation,&product->smaa);
}
/* r_aa: 0 off, 1 TAA, 2 SMAA (exclusive by construction); `smaaQuality` is r_smaa (0 selects preset 3). Between frames. */
qboolean RalWebGpu_ProductSetAntiAliasing( ralWebGpuProduct_t *product, ralWebGpuRuntimeReceipt_t *rr, int mode,
        int smaaQuality ) {
    if (!product || !product->taa || !product->smaa || product->inFlight) return qfalse;
    product->taaEnabled = mode == 1 ? qtrue : qfalse;
    product->smaaEnabled = mode == 2 ? qtrue : qfalse;
    return !product->smaaEnabled || RalWebGpu_SmaaSetQuality(product->smaa,rr,smaaQuality);
}
void RalWebGpu_ProductTaaFrameCounts( const ralWebGpuProduct_t *product, uint32_t *resolved, uint32_t *bootstrap ) {
    *resolved = product ? product->taaResolvedFrames : 0u;
    *bootstrap = product ? product->taaBootstrapFrames : 0u;
}
qboolean RalWebGpu_ProductEnableSkyMaterial(ralWebGpuProduct_t *product,
        ralWebGpuRuntimeReceipt_t *rr,ralWebGpuBrowserBridge_t *bridge,uint64_t generation) {
    if(!product||product->inFlight||product->skyMaterial) return qfalse;
    return RalWebGpu_SkyMaterialCreate(product->runtime,rr,bridge,generation,qtrue,&product->skyMaterial)
        && RalWebGpu_SkyMaterialCreate(product->runtime,rr,bridge,generation,qfalse,&product->skyCaptureMaterial);
}
qboolean RalWebGpu_ProductSetPhysicalSky(ralWebGpuProduct_t *product,
        uintptr_t texture,uintptr_t sampler,float blend) {
    if(!product||product->inFlight||!texture||!sampler||!isfinite(blend)||blend<0||blend>1) return qfalse;
    product->physicalSkyTexture=texture; product->physicalSkySampler=sampler;
    product->physicalSkyBlend=blend; return qtrue;
}

// Direction basis for the radiance array, NOT raw Q3 rt/bk/lf/ft/up/dn UVs.
// The canonical WGSL vertex includes the SPIR-V viewport-Y conversion, so
// viewUp = dDirection/dV produces the same array convention as source_sky.
static const float skyCaptureBasis[6][3][3] = {
    {{1,0,0},{0,0,-1},{0,-1,0}}, {{-1,0,0},{0,0,1},{0,-1,0}},
    {{0,1,0},{1,0,0},{0,0,1}}, {{0,-1,0},{1,0,0},{0,0,-1}},
    {{0,0,1},{1,0,0},{0,-1,0}}, {{0,0,-1},{-1,0,0},{0,-1,0}}
};
static qboolean EnsureSkyCaptureGeometry(ralWebGpuProduct_t *p,
        const ralWebGpuRuntimeReceipt_t *rr) {
    if(p->skyCaptureVertices && p->skyCaptureIndices) return qtrue;
    ralWebGpuResourceLayer_t *r=RalWebGpu_RuntimeResources(p->runtime,rr);
    renderWorldVertex_t vertices[24]={0}; uint32_t indices[36];
    const uint32_t quad[6]={0,1,2,2,1,3};
    if(!r) return qfalse;
    for(unsigned f=0;f<6;++f) {
        for(unsigned v=0;v<4;++v) {
            renderWorldVertex_t *vertex=&vertices[f*4+v];
            float u=(v&1)?1:-1,t=(v&2)?1:-1;
            for(unsigned c=0;c<3;++c) {
                vertex->position[c]=1024*(skyCaptureBasis[f][0][c]
                    +u*skyCaptureBasis[f][1][c]+t*skyCaptureBasis[f][2][c]);
                vertex->normal[c]=-skyCaptureBasis[f][0][c];
            }
            memset(vertex->color,255,sizeof(vertex->color));
        }
        for(unsigned i=0;i<6;++i) indices[f*6+i]=f*4+quad[i];
    }
    ralWebGpuResource_t *vb=NULL,*ib=NULL;
    ralWebGpuResourceReceipt_t vr={0},ir={0};
    ralWebGpuBufferDesc_t desc={0}; ralWebGpuWriteReceipt_t write;
    desc.size=sizeof(vertices); desc.memoryClass=RAL_ALLOCATION_DEVICE_LOCAL;
    desc.usage=RAL_WEBGPU_BUFFER_VERTEX|RAL_WEBGPU_BUFFER_COPY_DESTINATION;
    if(!RalWebGpu_CreateBuffer(r,&desc,&vb,&vr)
        ||!RalWebGpu_WriteBuffer(r,vb,&vr,0,vertices,sizeof(vertices),&write)) goto fail;
    desc.size=sizeof(indices); desc.usage=RAL_WEBGPU_BUFFER_INDEX|RAL_WEBGPU_BUFFER_COPY_DESTINATION;
    if(!RalWebGpu_CreateBuffer(r,&desc,&ib,&ir)
        ||!RalWebGpu_WriteBuffer(r,ib,&ir,0,indices,sizeof(indices),&write)) goto fail;
    p->skyCaptureVertices=vb; p->skyCaptureIndices=ib;
    p->skyCaptureVertexReceipt=vr; p->skyCaptureIndexReceipt=ir; return qtrue;
fail:
    if(vb) RalWebGpu_DestroyResource(r,vb,&vr);
    if(ib) RalWebGpu_DestroyResource(r,ib,&ir);
    return qfalse;
}
qboolean RalWebGpu_ProductSkyCaptureBegin(ralWebGpuProduct_t *p,
        const ralWebGpuRuntimeReceipt_t *rr,const renderSubmissionState_t *frontend,
        const renderMaterialSnapshot_t *material,int timeMs,uint32_t mask,
        uintptr_t physicalTexture,uintptr_t physicalSampler,float blend) {
    qhandle_t handles[8]; renderSkyMaterialGpuParams_t params;
    uintptr_t textures[10],samplers[10]; unsigned count=0;
    if(!p||!rr||!frontend||!material||p->inFlight||p->skyCaptureActive
        ||!p->skyCaptureMaterial||!p->skyResourcesReady||!mask||(mask&~63u)
        ||!physicalTexture||!physicalSampler||!isfinite(blend)||blend<0||blend>1
        ||!wired_sky_material_pack(material,0,timeMs,&params,handles)) return qfalse;
    // The caller prepared resident material resources before admission. No
    // alternate shader or base-texture substitution for missing stage data.
    for(unsigned i=0;i<9;++i) {
        materialEntry_t *entry=FindMaterial(p,i<8?handles[i]:material->handle);
        if(!entry||!entry->ready) return qfalse;
        textures[i]=entry->textureReceipt.resourceIdentity;
        samplers[i]=entry->samplerReceipt.resourceIdentity;
    }
    textures[9]=physicalTexture; samplers[9]=physicalSampler;
    for(unsigned f=0;f<6;++f) if(mask&(1u<<f)) ++count;
    if(!EnsureSkyCaptureGeometry(p,rr)||!RalWebGpu_SkyMaterialPrepare(p->skyCaptureMaterial,rr,count)) return qfalse;
    p->skyCaptureParams=params; p->skyCaptureMask=mask; p->skyCaptureBlend=blend;
    memcpy(p->skyCaptureTextures,textures,sizeof(textures));
    memcpy(p->skyCaptureSamplers,samplers,sizeof(samplers));
    p->skyCaptureActive=qtrue; return qtrue;
}
qboolean RalWebGpu_ProductSkyCaptureFace(ralWebGpuProduct_t *p,
        const ralWebGpuRuntimeReceipt_t *rr,ralWebGpuCommand_t *command,uint32_t face,
        const ralWebGpuCommandReceipt_t *recording,ralWebGpuCommandReceipt_t *updated) {
    if(!p||!p->skyCaptureActive||face>=6||!(p->skyCaptureMask&(1u<<face))) return qfalse;
    renderProductDrawGpuParams_t view={0}; ralWebGpuIndexedDraw_t draw={0};
    for(unsigned c=0;c<3;++c) {
        view.viewForward[c]=skyCaptureBasis[face][0][c];
        view.viewLeft[c]=-skyCaptureBasis[face][1][c];
        view.viewUp[c]=skyCaptureBasis[face][2][c];
    }
    view.projectionLightmap[0]=view.projectionLightmap[1]=view.projectionLightmap[2]=1;
    view.viewForward[3]=view.viewLeft[3]=1; view.viewUp[3]=.18f;
	view.viewportAlpha[0]=view.viewportAlpha[1]=WIRED_SKY_SOURCE_SIZE;
    view.atmosphereHeightSky[2]=p->skyCaptureBlend; view.atmosphereHeightSky[3]=1;
    draw.kind=RAL_WEBGPU_DRAW_WORLD;
    draw.textured=qtrue; draw.textureIdentity=p->skyCaptureTextures[8];
    draw.samplerIdentity=p->skyCaptureSamplers[8];
    draw.vertexBufferIdentity=p->skyCaptureVertexReceipt.resourceIdentity;
    draw.indexBufferIdentity=p->skyCaptureIndexReceipt.resourceIdentity;
    draw.firstIndex=face*6; draw.indexCount=6; draw.instanceCount=1;
    draw.contentDigest=HashU64(HashU64(UINT64_C(1469598103934665603),
        p->skyResourceWorldLifecycleGeneration),face+1);
    if(!draw.contentDigest) draw.contentDigest=1u;
    if(!RalWebGpu_SkyMaterialRecord(p->skyCaptureMaterial,rr,command,&view,&p->skyCaptureParams,
        p->skyCaptureTextures,p->skyCaptureSamplers,&draw,recording,updated)) return qfalse;
    p->skyCaptureMask &= ~(1u<<face); return qtrue;
}
void RalWebGpu_ProductSkyCaptureEnd(ralWebGpuProduct_t *p) {
    if(p) { p->skyCaptureActive=qfalse; p->skyCaptureMask=0; }
}
static void DestroySkyVertices(ralWebGpuProduct_t *product,ralWebGpuResourceLayer_t *resources,int index) {
    if(product->skyVertices[index]) RalWebGpu_DestroyResource(resources,product->skyVertices[index],
        &product->skyVertexReceipts[index]);
    product->skyVertices[index]=NULL; product->skyVertexGenerations[index]=0;
    memset(&product->skyVertexReceipts[index],0,sizeof(product->skyVertexReceipts[index]));
}
qboolean RalWebGpu_ProductSetExposure( ralWebGpuProduct_t *product,
        const ralExposureSettings_t *s, const ralDisplayVisibilityPlan_t *v,
        int toneMode, float toneExposure ) {
    if (!product || product->inFlight || !product->exposure
        || !Ral_ExposureSettingsValid(s) || !Ral_DisplayVisibilityPlanValid(v)) return qfalse;
    if (!RalWebGpu_ExposureSetToneMap(product->exposure,toneMode,toneExposure)) return qfalse;
    product->exposureSettings=*s; product->exposureVisibility=*v; return qtrue;
}
void RalWebGpu_ProductDestroy( ralWebGpuProduct_t *product,
		const ralWebGpuRuntimeReceipt_t *runtimeReceipt ) {
	ralWebGpuResourceLayer_t *resources;
	if ( !product ) return;
	resources = runtimeReceipt ? RalWebGpu_RuntimeResources( product->runtime,
		runtimeReceipt ) : NULL;
	if ( resources && !product->inFlight ) {
        ralWebGpuRuntimeReceipt_t authority = *runtimeReceipt;
        RalWebGpu_ExposureDestroy(product->exposure,&authority);
        (void)RalWebGpu_RuntimeGetReceipt(product->runtime,&authority);
        RalWebGpu_TaaDestroy(product->taa,&authority);
        (void)RalWebGpu_RuntimeGetReceipt(product->runtime,&authority);
        RalWebGpu_SmaaDestroy(product->smaa,&authority);
        // Exposure retires pipelines, changing the exact runtime receipt.
        // Refresh before retiring the next resource owner.
        (void)RalWebGpu_RuntimeGetReceipt(product->runtime,&authority);
        RalWebGpu_SkyMaterialDestroy(product->skyMaterial,&authority);
        (void)RalWebGpu_RuntimeGetReceipt(product->runtime,&authority);
        RalWebGpu_SkyMaterialDestroy(product->skyCaptureMaterial,&authority);
        if(product->skyCaptureVertices) RalWebGpu_DestroyResource(resources,product->skyCaptureVertices,&product->skyCaptureVertexReceipt);
        if(product->skyCaptureIndices) RalWebGpu_DestroyResource(resources,product->skyCaptureIndices,&product->skyCaptureIndexReceipt);
        for(int i=0;i<MAX_RENDER_WORLDS;++i) DestroySkyVertices(product,resources,i);
		DestroySkyRadianceResources( product, resources );
		DestroyFrameResources( product, resources );
		for ( int i = 0; i < MAX_RENDER_WORLDS; ++i )
			DestroyWorldResources( product, resources, i );
		for ( uint32_t i = 0u; i < product->materialCount; ++i )
			DestroyMaterial( resources, &product->materials[i] );
		if ( product->entityLighting )
			RalWebGpu_DestroyResource( resources, product->entityLighting,
				&product->entityLightingReceipt );
		if (product->effectParameters)
			RalWebGpu_DestroyResource(resources, product->effectParameters, &product->effectParameterReceipt);
		if ( product->worldMaterials )
			RalWebGpu_DestroyResource( resources, product->worldMaterials,
				&product->worldMaterialReceipt );
	}
	memset( product, 0, sizeof( *product ) ); free( product );
}

qboolean RalWebGpu_ProductUnloadWorld( ralWebGpuProduct_t *product,
		const ralWebGpuRuntimeReceipt_t *runtimeReceipt, int worldIndex ) {
	ralWebGpuResourceLayer_t *resources;
	if ( !product || !runtimeReceipt || product->inFlight || product->skyCaptureActive || worldIndex < 0
			|| worldIndex >= MAX_RENDER_WORLDS ) return qfalse;
	resources = RalWebGpu_RuntimeResources( product->runtime, runtimeReceipt );
	if ( !resources ) return qfalse;
	DestroyWorldResources( product, resources, worldIndex );
    RalWebGpu_SkyMaterialReset(product->skyMaterial,runtimeReceipt);
    RalWebGpu_SkyMaterialReset(product->skyCaptureMaterial,runtimeReceipt);
    DestroySkyVertices(product,resources,worldIndex);
	if ( worldIndex == 0 ) {
        RalWebGpu_ExposureReset(product->exposure);
        Ral_TemporalInit( &product->taaState ); product->taaFrameId = 0u;
		RenderEffectDecalPool_Reset( &product->effectDecalPool );
		RenderEffectDecalPool_Reset( &product->pendingEffectDecalPool );
		memset( &product->pendingEffectDecalPoolReceipt, 0,
			sizeof( product->pendingEffectDecalPoolReceipt ) );
		RenderEffectBeamPool_Reset( &product->effectBeamPool );
		RenderEffectBeamPool_Reset( &product->pendingEffectBeamPool );
		memset( &product->pendingEffectBeamPoolReceipt, 0,
			sizeof( product->pendingEffectBeamPoolReceipt ) );
	}
	return qtrue;
}

qboolean RalWebGpu_ProductResetRegistrationAssets( ralWebGpuProduct_t *product,
		const ralWebGpuRuntimeReceipt_t *runtimeReceipt, renderSubmissionState_t *frontend ) {
	ralWebGpuResourceLayer_t *resources;
	if ( !product || !runtimeReceipt || product->inFlight || product->skyCaptureActive || !frontend
			|| !frontend->initialized || frontend->frameOpen
			|| RenderSubmission_ResidentWorldCount( frontend ) ) return qfalse;
	for ( int i = 0; i < MAX_RENDER_WORLDS; ++i )
		if ( product->worlds[i].vertices || product->worlds[i].indices ) return qfalse;
	resources = RalWebGpu_RuntimeResources( product->runtime, runtimeReceipt );
	if ( !resources ) return qfalse;
	DestroySkyRadianceResources( product, resources );
	RalWebGpu_SkyMaterialReset(product->skyMaterial,runtimeReceipt);
	RalWebGpu_SkyMaterialReset(product->skyCaptureMaterial,runtimeReceipt);
	for ( uint32_t i = 0; i < product->materialCount; ++i )
		DestroyMaterial( resources, &product->materials[i] );
	product->materialCount = 0u;
	memset( product->cookedMaterialTextureIdentities, 0,
		sizeof( product->cookedMaterialTextureIdentities ) );
	memset( product->cookedMaterialReady, 0, sizeof( product->cookedMaterialReady ) );
	product->cookedMaterialRuntimeGeneration = 0u;
	return RenderSubmission_ResetRegistrationAssets( frontend );
}

qboolean RalWebGpu_ProductResolveMaterialRuntimeResource( void *opaque,
		uint64_t cacheKeyHash, ralMaterialRuntimeResource_t *outResource ) {
	const ralWebGpuProduct_t *product = (const ralWebGpuProduct_t *)opaque;
	const ralMaterialRuntimeResource_t *match = NULL;
	if ( !product || !cacheKeyHash || !outResource ) return qfalse;
	for ( uint32_t i = 0u; i < product->materialCount; ++i ) {
		const ralMaterialRuntimeResource_t *resource =
			&product->materials[i].runtimeResource;
		if ( resource->cacheKeyHash != cacheKeyHash ) continue;
		if ( match || !Ral_TextureResourceReceiptValid( &resource->resource ) )
			return qfalse;
		match = resource;
	}
	if ( !match ) return qfalse;
	*outResource = *match;
	return qtrue;
}

qboolean RalWebGpu_ProductSetMaterialRuntimeCatalog(
		ralWebGpuProduct_t *product,
		const renderMaterialRuntimeCatalog_t *catalog,
		renderSubmissionState_t *frontend ) {
	renderWorldSnapshot_t world;
	uintptr_t candidateTextureIdentities[RENDER_SUBMISSION_MAX_MATERIALS];
	qboolean candidateReady[RENDER_SUBMISSION_MAX_MATERIALS];
	renderMaterialFactorPublicationEntry_t factorEntries[
		RENDER_SUBMISSION_MAX_MATERIALS];
	uint32_t factorEntryCount = 0u;
	if ( !product || product->inFlight ) return qfalse;
	if ( !catalog ) {
		if ( frontend && !RenderSubmission_PublishMaterialFactors(
				frontend, 0u, NULL, 0u ) ) return qfalse;
		memset( product->cookedMaterialTextureIdentities, 0,
			sizeof( product->cookedMaterialTextureIdentities ) );
		memset( product->cookedMaterialReady, 0,
			sizeof( product->cookedMaterialReady ) );
		product->cookedMaterialRuntimeGeneration = 0u;
		return qtrue;
	}
	if ( !frontend || catalog->ready != qtrue || !catalog->runtimeGeneration
			|| !RenderSubmission_ResidentWorldSnapshot( frontend,
				frontend->activeWorldIndex, catalog->runtimeGeneration, &world ) ) return qfalse;
	memset( candidateTextureIdentities, 0,
		sizeof( candidateTextureIdentities ) );
	memset( candidateReady, 0, sizeof( candidateReady ) );
	for ( uint32_t i = 0u; i < frontend->materialCount; ++i ) {
		const renderMaterialRecord_t *record = &frontend->materials[i];
		const ralMaterialRuntimePlan_t *plan;
		materialEntry_t *entry;
		ralTextureResourceReceipt_t resource;
		plan = RenderMaterialRuntimeCatalog_Lookup( catalog, record->name );
		if ( !plan ) continue;
		factorEntries[factorEntryCount].material = record->snapshot.handle;
		factorEntries[factorEntryCount++].factors = plan->material.material.factors;
		/* Pure declarations (e.g. fog volumes) have no sampled image. Keep
		 * their factors, but do not invent texture readiness for draw batches. */
		if ( plan->bindingCount == 0u && plan->dependencyCount == 0u ) continue;
		entry = FindMaterial( product, record->snapshot.handle );
		if ( record->snapshot.textureCacheKeyReady != qtrue || !entry
				|| !entry->ready
				|| !RenderMaterialRuntimeCatalog_ResolveTexture( catalog,
					record->name, record->snapshot.textureCacheKeyHash, &resource )
				|| resource.backendType != RAL_BACKEND_WEBGPU
				|| resource.textureIdentity
					!= entry->textureReceipt.resourceIdentity ) return qfalse;
		candidateTextureIdentities[i] = resource.textureIdentity;
		candidateReady[i] = qtrue;
	}
	for ( uint32_t batch = 0u; batch < world.batchCount; ++batch ) {
		qboolean found = qfalse;
		if ( world.batches[batch].noDraw ) continue;
		for ( uint32_t i = 0u; i < frontend->materialCount; ++i ) {
			if ( frontend->materials[i].snapshot.handle
					!= world.batches[batch].baseMaterial ) continue;
			found = candidateReady[i];
			break;
		}
		if ( !found ) return qfalse;
	}
	if ( !RenderSubmission_PublishMaterialFactors( frontend,
		catalog->runtimeGeneration, factorEntries, factorEntryCount ) ) return qfalse;
	memcpy( product->cookedMaterialTextureIdentities,
		candidateTextureIdentities, sizeof( candidateTextureIdentities ) );
	memcpy( product->cookedMaterialReady, candidateReady,
		sizeof( candidateReady ) );
	product->cookedMaterialRuntimeGeneration = catalog->runtimeGeneration;
	return qtrue;
}

qboolean RalWebGpu_ProductPlan( ralWebGpuProduct_t *product,
		const ralWebGpuRuntimeReceipt_t *runtimeReceipt,
		const renderSubmissionState_t *frontend,
		const renderSubmissionReceipt_t *submission,
		ralWebGpuFrontendPlanReceipt_t *outPlan ) {
	uint64_t generation;
	renderWorldSnapshot_t view;
	renderEffectPrimitiveSnapshot_t effects;
	const spriteDesc_t *sprites;
	const emitterDesc_t *emitters;
	const decalDesc_t *decals;
	const renderEffectRibbonCommand_t *ribbons;
	const ribbonPoint_t *points;
	const beamDesc_t *beams;
	renderEffectDecalPool_t decalCandidate;
	renderEffectDecalPoolReceipt_t decalReceipt;
	renderEffectBeamPool_t beamCandidate;
	renderEffectBeamPoolReceipt_t beamReceipt;
	if ( !product || !runtimeReceipt || !frontend || !submission || !outPlan
			|| product->inFlight
			|| product->planGeneration >= UINT64_MAX - 1u
			|| !RalWebGpu_RuntimeOwnsPipeline( product->runtime, runtimeReceipt,
				&product->worldPipeline ) ) return qfalse;
	generation = product->planGeneration + 1u;
	memset( &effects, 0, sizeof( effects ) );
	if ( !RenderSubmission_EffectPrimitiveSnapshots( frontend, &effects,
			&sprites, &emitters, &decals, &ribbons, &points, &beams ) )
		return qfalse;
	(void)sprites; (void)emitters; (void)decals; (void)ribbons; (void)points;
	memset( &view, 0, sizeof( view ) );
	const qboolean hasTimeline = RenderSubmission_ViewSnapshot( frontend, &view );
	if ( !RenderEffectDecalPool_Advance( &product->effectDecalPool, generation,
			hasTimeline, hasTimeline ? (float)view.timeMs * 0.001f : 0.0f,
			decals, effects.decalCount, &decalCandidate, &decalReceipt ) )
		return qfalse;
	if ( !RenderEffectBeamPool_Advance( &product->effectBeamPool, generation,
			hasTimeline, hasTimeline ? (float)view.timeMs * 0.001f : 0.0f,
			beams, effects.beamCount, &beamCandidate, &beamReceipt ) ) return qfalse;
	if ( !RalWebGpu_FrontendPlanBuild( product->runtime, runtimeReceipt,
			frontend, submission, generation, &decalCandidate, &decalReceipt,
			&beamCandidate, &beamReceipt,
			outPlan ) ) return qfalse;
	product->pendingEffectDecalPool = decalCandidate;
	product->pendingEffectDecalPoolReceipt = decalReceipt;
	product->pendingEffectBeamPool = beamCandidate;
	product->pendingEffectBeamPoolReceipt = beamReceipt;
	product->planGeneration = generation; return qtrue;
}

static qboolean RecordWorldBatch( ralWebGpuProduct_t *product,
		const ralWebGpuRuntimeReceipt_t *runtimeReceipt,
		ralWebGpuCommand_t *command, const renderSubmissionState_t *frontend,
		uint32_t materialCount, materialEntry_t **resolved,
		const renderWorldSnapshot_t *world, const renderWorldBatch_t *batch,
		const ralWebGpuResourceReceipt_t *vertexReceipt,
		const ralWebGpuResourceReceipt_t *indexReceipt,
		uint32_t firstInstance, uint64_t loweringDigest,
		const ralWebGpuCommandReceipt_t *recording,
		ralWebGpuCommandReceipt_t *updated ) {
	materialEntry_t *base = NULL, *lightmap = NULL;
	uintptr_t cookedBaseIdentity = 0u;
	ralWebGpuIndexedDraw_t draw;
	if ( !product || !command || !frontend || !resolved || !world || !batch
			|| !vertexReceipt || !indexReceipt || !recording || !updated )
		return qfalse;
	for ( uint32_t materialIndex = 0u;
			materialIndex < materialCount; ++materialIndex ) {
		if ( frontend->materials[materialIndex].snapshot.handle
				== batch->baseMaterial ) {
			base = resolved[materialIndex];
			if ( product->cookedMaterialReady[materialIndex] )
				cookedBaseIdentity =
					product->cookedMaterialTextureIdentities[materialIndex];
		}
		if ( batch->lightmapMaterial > 0
				&& frontend->materials[materialIndex].snapshot.handle
					== batch->lightmapMaterial )
			lightmap = resolved[materialIndex];
	}
	if ( !base || ( product->cookedMaterialRuntimeGeneration
			&& !cookedBaseIdentity )
			|| ( batch->lightmapMaterial > 0 && !lightmap )
			|| batch->firstIndex > world->indexCount
			|| batch->indexCount > world->indexCount - batch->firstIndex )
		return qfalse;
	memset( &draw, 0, sizeof( draw ) );
	draw.kind = RAL_WEBGPU_DRAW_WORLD;
	draw.pipelineIdentity = product->worldPipeline.pipelineIdentity;
	draw.vertexBufferIdentity = vertexReceipt->resourceIdentity;
	draw.indexBufferIdentity = indexReceipt->resourceIdentity;
	draw.textureIdentity = cookedBaseIdentity ? cookedBaseIdentity
		: base->textureReceipt.resourceIdentity;
	draw.secondaryTextureIdentity = lightmap
		? lightmap->textureReceipt.resourceIdentity : (uintptr_t)0;
	draw.samplerIdentity = base->samplerReceipt.resourceIdentity;
	draw.textured = qtrue;
	/* Group 2 of the world pipeline: base texture, sampler, lightmap. A batch
	 * without a lightmap binds its base texture twice and the material extras
	 * tell the shader to ignore the second sample. */
	draw.textureBindGroupSlot = 3u;
	if ( !draw.secondaryTextureIdentity )
		draw.secondaryTextureIdentity = draw.textureIdentity;
	draw.firstIndex = batch->firstIndex;
	draw.indexCount = batch->indexCount;
	draw.instanceCount = 1u;
	draw.firstInstance = firstInstance;
	draw.contentDigest = HashU64( loweringDigest,
		( (uint64_t)batch->sourceSurfaceIndex << 32u ) | batch->indexCount );
	draw.bindGroupCount = 2u;
	draw.bindGroupIdentities[0] = product->atmosphereBindGroups[0];
	draw.bindGroupIdentities[1] = product->worldMaterialBindGroup;
    if(batch->sky) {
        renderMaterialSnapshot_t material;
        renderSkyMaterialGpuParams_t params;
        renderProductDrawGpuParams_t view={0};
        qhandle_t handles[RENDER_MATERIAL_MAX_STAGES];
        uintptr_t textures[10],samplers[10];
        const int wi=frontend->activeWorldIndex;
        if(!product->skyMaterial||wi<0||wi>=MAX_RENDER_WORLDS||!product->skyVertices[wi]
            ||!RenderSubmission_MaterialSnapshot(frontend,batch->baseMaterial,&material)
            ||!wired_sky_material_pack(&material,batch->lightmapMaterial,world->timeMs,&params,handles)) return qfalse;
        for(unsigned stage=0;stage<8;++stage) {
            materialEntry_t *image=NULL;
            for(uint32_t i=0;i<materialCount;++i) if(resolved[i]&&resolved[i]->handle==handles[stage]) {
                image=resolved[i]; break;
            }
            if(!image) return qfalse;
            textures[stage]=image->textureReceipt.resourceIdentity;
            samplers[stage]=image->samplerReceipt.resourceIdentity;
        }
        textures[8]=draw.textureIdentity; samplers[8]=draw.samplerIdentity;
        textures[9]=product->physicalSkyTexture; samplers[9]=product->physicalSkySampler;
        if(!isfinite(world->fovX)||!isfinite(world->fovY)||world->fovX<=1||world->fovX>=179
            ||world->fovY<=1||world->fovY>=179) return qfalse;
        memcpy(view.viewOriginDrawSpace,world->viewOrigin,3*sizeof(float));
        memcpy(view.atmosphereEyeDensity,world->viewOrigin,3*sizeof(float));
        memcpy(view.viewForward,world->viewAxis[0],3*sizeof(float));
        memcpy(view.viewLeft,world->viewAxis[1],3*sizeof(float));
        memcpy(view.viewUp,world->viewAxis[2],3*sizeof(float));
        view.projectionLightmap[0]=1.0f/tanf(world->fovX*.00872664625997164788f);
        view.projectionLightmap[1]=1.0f/tanf(world->fovY*.00872664625997164788f);
        view.projectionLightmap[2]=4;
        view.viewportAlpha[0]=(float)product->viewportWidth;
        view.viewportAlpha[1]=(float)product->viewportHeight;
        view.atmosphereHeightSky[2]=product->physicalSkyBlend;
        view.atmosphereHeightSky[3]=1;
        // Scene remains linear HDR; final exposure owns brightness once.
        view.viewForward[3]=view.viewLeft[3]=1;
        view.viewUp[3]=product->exposureVisibility.shadowPivot;
        draw.vertexBufferIdentity=product->skyVertexReceipts[wi].resourceIdentity;
        return RalWebGpu_SkyMaterialRecord(product->skyMaterial,runtimeReceipt,command,&view,&params,
            textures,samplers,&draw,recording,updated);
    }
	return RalWebGpu_CommandRecordIndexedDraw( command, recording, &draw,
		updated );
}

/* TAA (when planned for this frame) resolves the scene HDR, then the exposure meters and tonemaps the TAA output instead of the
 * scene HDR. The plan and the camera wait for the submission to complete (RalWebGpu_ProductPoll) before they become history. */
static qboolean RecordToneMap( ralWebGpuProduct_t *product, const ralWebGpuRuntimeReceipt_t *runtimeReceipt,
		ralWebGpuCommand_t *command, ralWebGpuCommandReceipt_t *recording, uintptr_t target, uintptr_t sceneTarget,
		const ralWebGpuTaaFrame_t *taaFrame, const ralWebGpuIndexedDraw_t *maskDraws, uint32_t maskDrawCount,
		ralWebGpuProductFrameReceipt_t *candidate ) {
	uintptr_t source = 0;
	if ( taaFrame ) {
		uint32_t dispatches = 0u;
		/* Effect mask: the entity draws of this frame again, with the mask pipeline, against the finished scene depth. Entities have no
		 * tracked motion and the first-person weapon moves with the camera, so they take the resolve's spatial fallback (the Vulkan
		 * renderer invalidates the same draws). */
		const uintptr_t mask = RalWebGpu_TaaEffectMask( product->taa, runtimeReceipt, sceneTarget, taaFrame->width, taaFrame->height );
		ralWebGpuPassDesc_t pass;
		ralWebGpuCommandReceipt_t next;
		if ( !mask || !product->taaMaskPipelineReady ) { s_productFailureStage = 97u; return qfalse; }
		memset( &pass, 0, sizeof( pass ) );
		pass.kind = RAL_WEBGPU_PASS_RENDER; pass.targetIdentity = mask; pass.colorLoadOp = RAL_WEBGPU_LOAD_CLEAR;
		pass.depthEnabled = qtrue; pass.depthLoadOp = RAL_WEBGPU_LOAD_PRESERVE; pass.depthStore = qtrue; pass.clearDepth = 1.0f;
		if ( !RalWebGpu_CommandNextPass( command, recording, &pass, &next ) ) { s_productFailureStage = 96u; return qfalse; }
		*recording = next;
		for ( uint32_t i = 0u; i < maskDrawCount; ++i ) {
			ralWebGpuIndexedDraw_t draw = maskDraws[i];   /* carries its mask pipeline (opaque mark or translucent contribution) */
			if ( !RalWebGpu_CommandRecordIndexedDraw( command, recording, &draw, &next ) ) { s_productFailureStage = 95u; return qfalse; }
			*recording = next;
		}
		if ( !RalWebGpu_TaaRecord( product->taa, runtimeReceipt, command, recording, sceneTarget, taaFrame, &source,
				&dispatches ) ) { s_productFailureStage = 100u + RalWebGpu_TaaLastFailure(); return qfalse; }
		candidate->aaDispatchCount = dispatches;
		product->taaPending = qtrue; product->taaPendingFrame = product->taaState.pendingPlan.frameId;
		product->taaPendingCamera = taaFrame->current; product->taaPendingResolved = taaFrame->historyValid ? qtrue : qfalse;
	}
	else if ( product->smaa && product->smaaEnabled ) {
		if ( !RalWebGpu_SmaaRecord( product->smaa, runtimeReceipt, command, recording, sceneTarget, product->viewportWidth,
				product->viewportHeight, &source ) ) {
			s_productFailureStage = 200u + RalWebGpu_SmaaLastFailure(); return qfalse;
		}
	}
	if ( !RalWebGpu_ExposureRecord( product->exposure, command, recording, target, source ) ) {
		s_productFailureStage = source ? 99u : 98u; return qfalse;
	}
	candidate->toneMapDrawCount = 1;
	return qtrue;
}

static void TaaFinish( ralWebGpuProduct_t *product, qboolean success ) {
	if ( !product->taaPending ) {
		if ( product->taaState.pending )
			(void)Ral_TemporalCancelFrame( &product->taaState, product->taaState.pendingPlan.frameId );
		return;
	}
	product->taaPending = qfalse;
	if ( success && Ral_TemporalCommitFrame( &product->taaState, product->taaPendingFrame ) ) {
		product->taaFrameId = product->taaPendingFrame;
		product->taaPreviousCamera = product->taaPendingCamera;
		if ( product->taaPendingResolved ) product->taaResolvedFrames++; else product->taaBootstrapFrames++;
	} else (void)Ral_TemporalCancelFrame( &product->taaState, product->taaPendingFrame );
}

qboolean RalWebGpu_ProductRenderPlan( ralWebGpuProduct_t *product,
		const ralWebGpuRuntimeReceipt_t *runtimeReceipt,
		renderSubmissionState_t *frontend,
		const renderSubmissionReceipt_t *submission,
		const ralWebGpuFrontendPlanReceipt_t *plan,
		const ralSkyEnvironmentPlanReceipt_t *skyEnvironment,
		uintptr_t targetIdentity,
		uint64_t frameGeneration, ralWebGpuProductFrameReceipt_t *outReceipt ) {
	ralWebGpuProductFrameReceipt_t candidate;
	materialEntry_t *staged = NULL;
	materialEntry_t **resolved = NULL;
	qboolean *stagedMaterial = NULL;
	ralWebGpuResourceLayer_t *resources;
	ralWebGpuCommand_t *command;
	renderWorldSnapshot_t world;
	renderDistantWorldSubmission_t distantWorld;
	ralWebGpuBufferDesc_t vertexDesc, indexDesc;
	ralWebGpuResource_t *vertices = NULL, *indices = NULL;
	ralWebGpuResource_t *distantVertices = NULL, *distantIndices = NULL;
	ralWebGpuResource_t *entityVertexResource = NULL, *entityIndexResource = NULL;
	ralWebGpuResource_t *miscVertexResource = NULL, *miscIndexResource = NULL;
	ralWebGpuResourceReceipt_t vertexReceipt, indexReceipt;
	ralWebGpuResourceReceipt_t distantVertexReceipt, distantIndexReceipt;
	ralWebGpuResourceReceipt_t entityVertexReceipt, entityIndexReceipt;
	ralWebGpuResourceReceipt_t miscVertexReceipt, miscIndexReceipt;
	renderWorldVertex_t *entityVertices = NULL;
	renderWorldVertex_t *miscVertices = NULL;
	ralWebGpuWorldVertex_t *projectedWorld = NULL;
	ralWebGpuWorldVertex_t *projectedEntities = NULL;
	ralWebGpuWorldVertex_t *projectedDistantWorld = NULL;
	uint32_t *entityIndices = NULL;
	uint32_t *miscIndices = NULL;
	entityDrawPlan_t *entityDraws = NULL;
	float *entityLightingData = NULL;
	float *worldMaterialData = NULL;
	ralWebGpuEffectDrawParams_t *effectParameterData = NULL;
	miscDrawPlan_t *miscDraws = NULL;
	uint32_t entityVertexCount = 0u, entityIndexCount = 0u, entityDrawCount = 0u;
	uint32_t miscVertexCount = 0u, miscIndexCount = 0u, miscDrawCount = 0u;
	ralWebGpuCommandReceipt_t recording, updated, executable;
	uint32_t stagedCount = 0u, materialCount = 0u, newMaterialCount = 0u;
	int worldIndex, distantWorldIndex = -1;
	qboolean distantWorldReady = qfalse;
	uint32_t worldMaterialEntryCount;
	qboolean commandActive = qfalse;
    uintptr_t sceneTarget=targetIdentity;
	ralTemporalFramePlan_t taaPlan;
	ralWebGpuTaaFrame_t taaFrame;
	ralWebGpuIndexedDraw_t *taaMaskDraws = NULL;
	uint32_t taaMaskDrawCount = 0u;
	float taaJitterNdc[2] = { 0.0f, 0.0f }, taaProjection[4];
	qboolean taaActive = qfalse;
	memset( &taaPlan, 0, sizeof( taaPlan ) ); memset( &taaFrame, 0, sizeof( taaFrame ) );
	ralExposureSettings_t frameExposure;
	s_productFailureStage = 1u;
	if ( !product || !runtimeReceipt || !frontend || !submission || !plan
			|| !outReceipt ) return qfalse;
	if ( !Ral_AtmosphereWeatherReceiptExact( &product->weatherPlan,
			&product->weatherPlan ) ) {
		s_productFailureStage = 14u; return qfalse;
	}
	if ( !product->weatherPlan.zeroWork && !product->weatherReady ) {
		s_productFailureStage = 15u; return qfalse;
	}
	if ( !targetIdentity || !frameGeneration || product->inFlight ) {
		s_productFailureStage = 11u; return qfalse;
	}
	if ( !RalWebGpu_FrontendPlanReceiptExact( plan, plan ) ) {
		s_productFailureStage = 12u; return qfalse;
	}
	if ( plan->planGeneration
			!= product->pendingEffectBeamPoolReceipt.frameGeneration
			|| !RenderEffectBeamPool_ReceiptExact(
				&product->pendingEffectBeamPool,
				&product->pendingEffectBeamPoolReceipt ) ) {
		s_productFailureStage = 16u; return qfalse;
	}
	if ( plan->planGeneration
			!= product->pendingEffectDecalPoolReceipt.frameGeneration
			|| !RenderEffectDecalPool_ReceiptExact(
				&product->pendingEffectDecalPool,
				&product->pendingEffectDecalPoolReceipt ) ) {
		s_productFailureStage = 16u; return qfalse;
	}
	if ( plan->backendGeneration != runtimeReceipt->generation ) {
		s_productFailureStage = 13u; return qfalse;
	}
	if ( plan->frontendOwnerGeneration != submission->ownerGeneration ) {
		s_productFailureStage = 17u; return qfalse;
	}
	if ( plan->frontendFrameGeneration != submission->frameGeneration ) {
		s_productFailureStage = 18u; return qfalse;
	}
	if ( plan->frontendFrameDigest != submission->frameDigest ) {
		s_productFailureStage = 19u; return qfalse;
	}
	if ( product->atmosphereBindGroupsReady != qtrue
			|| product->worldMaterialBindGroupReady != qtrue ) {
		s_productFailureStage = 20u; return qfalse;
	}
	if ( !RalWebGpu_RuntimeOwnsPipeline( product->runtime, runtimeReceipt,
			&product->worldPipeline ) ) {
		s_productFailureStage = 21u; return qfalse;
	}
	s_productFailureStage = 2u;
	resources = RalWebGpu_RuntimeResources( product->runtime, runtimeReceipt );
	command = RalWebGpu_RuntimeCommand( product->runtime, runtimeReceipt );
	if ( !resources || !command ) return qfalse;
	memset( &world, 0, sizeof( world ) );
	/* A sealed UI frame has no view or BSP draw, even while a world is resident.
	 * Do not invent view parameters or feed it through scene exposure. */
	if ( submission->sceneRendered ) {
		if ( !RenderSubmission_ViewSnapshot( frontend, &world ) ) return qfalse;
	} else if ( plan->entityBatchCount || plan->polygonBatchCount || plan->lightCount
			|| plan->effectBatchCount || !product->weatherPlan.zeroWork ) return qfalse;
	/* Retain world residency and view parameters, not its BSP draw request. */
	if ( world.rdflags & RDF_NOWORLDMODEL )
		world.vertexCount = world.indexCount = world.batchCount = world.patchBatchCount = 0u;
	if ( plan->worldVertexCount != world.vertexCount || plan->worldIndexCount != world.indexCount
			|| plan->worldBatchCount != world.batchCount ) return qfalse;
	frameExposure = product->exposureSettings;
	if ( world.rdflags & RDF_NOWORLDMODEL ) frameExposure.enabled = 0;
	/* TAA plan before any projection: the jitter enters the world and entity vertices. A plan abandoned by an earlier failed frame
	 * is cancelled first; switching r_aa off drops the history so a later switch-on starts from the bootstrap frame. */
	if ( product->taaState.pending )
		(void)Ral_TemporalCancelFrame( &product->taaState, product->taaState.pendingPlan.frameId );
	if ( !product->taaEnabled && product->taaState.active ) { Ral_TemporalInit( &product->taaState ); product->taaFrameId = 0u; }
	if ( product->taa && product->taaEnabled && product->exposure && submission->sceneRendered
			&& !( world.rdflags & RDF_NOWORLDMODEL ) && product->viewportWidth && product->viewportHeight
			&& RalWebGpu_ProductProjection( &world, taaProjection ) ) {
		ralTemporalFrameInput_t input;
		const uint64_t lifecycle = frontend->worlds[frontend->activeWorldIndex].lifecycleGeneration;
		memset( &input, 0, sizeof( input ) );
		if ( product->taaWorldGeneration != lifecycle ) {
			product->taaWorldGeneration = lifecycle;
			if ( !++product->taaTopologyEpoch ) product->taaTopologyEpoch = 1u;
		}
		input.frameId = product->taaFrameId + 1u;
		input.width = product->viewportWidth; input.height = product->viewportHeight;
		input.topologyEpoch = product->taaTopologyEpoch; input.enabled = 1u;
		input.cameraCut = ( world.rdflags & ( RDF_CAMERA_CUT | RDF_HYPERSPACE ) ) ? 1u : 0u;
		if ( Ral_TemporalBeginFrame( &product->taaState, &input, &taaPlan ) ) {
			taaActive = qtrue;
			taaJitterNdc[0] = 2.0f * taaPlan.sceneJitterPixels[0] / (float)product->viewportWidth;
			taaJitterNdc[1] = -2.0f * taaPlan.sceneJitterPixels[1] / (float)product->viewportHeight;
			for ( int axis = 0; axis < 3; ++axis ) {
				taaFrame.current.origin[axis] = world.viewOrigin[axis];
				taaFrame.current.forward[axis] = world.viewAxis[0][axis];
				taaFrame.current.left[axis] = world.viewAxis[1][axis];
				taaFrame.current.up[axis] = world.viewAxis[2][axis];
			}
			taaFrame.current.tanHalfX = taaProjection[0]; taaFrame.current.tanHalfY = taaProjection[1];
			taaFrame.current.zNear = taaProjection[2];
			taaFrame.current.zFar = taaProjection[2] * taaProjection[3] / ( taaProjection[3] - 1.0f );   /* f/(f-n) -> f */
			taaFrame.previous = product->taaPreviousCamera;
			taaFrame.width = product->viewportWidth; taaFrame.height = product->viewportHeight;
			taaFrame.jitterNdc[0] = taaJitterNdc[0]; taaFrame.jitterNdc[1] = taaJitterNdc[1];
			taaFrame.currentJitterUv[0] = taaPlan.sceneJitterPixels[0] / (float)product->viewportWidth;
			taaFrame.currentJitterUv[1] = taaPlan.sceneJitterPixels[1] / (float)product->viewportHeight;
			taaFrame.previousJitterUv[0] = taaPlan.previousJitterPixels[0] / (float)product->viewportWidth;
			taaFrame.previousJitterUv[1] = taaPlan.previousJitterPixels[1] / (float)product->viewportHeight;
			taaFrame.historyValid = taaPlan.historyValid;
			taaFrame.readIndex = taaPlan.historyReadIndex; taaFrame.writeIndex = taaPlan.historyWriteIndex;
		}
	}
	worldIndex = frontend->activeWorldIndex;
	if ( submission->sceneRendered && ( worldIndex < 0 || worldIndex >= MAX_RENDER_WORLDS ) ) return qfalse;
	memset( &distantWorld, 0, sizeof( distantWorld ) );
	if ( submission->sceneRendered && skyEnvironment && skyEnvironment->distantSceneActive
			&& !( world.rdflags & RDF_NOWORLDMODEL ) ) {
		if ( !RenderSubmission_DistantWorldSubmissionForActiveView( frontend,
				skyEnvironment, RAL_SKY_DISTANT_MAX_LOD_LEVELS,
				RAL_SKY_DISTANT_MAX_VISIBLE_DRAWS,
				product->distantMetricScratch,
				RAL_SKY_DISTANT_MAX_CANDIDATES,
				product->distantCandidateScratch,
				RAL_SKY_DISTANT_MAX_CANDIDATES, &distantWorld ) ) {
			s_productFailureStage = 22u; return qfalse;
		}
		distantWorldIndex = RENDER_DISTANT_WORLD_BASE + worldIndex;
		distantWorldReady = distantWorld.drawPlan.admittedCount > 0u;
	}
	memset( &candidate, 0, sizeof( candidate ) );
	candidate.plan = *plan;
	candidate.effectDecalPool = product->pendingEffectDecalPoolReceipt;
	candidate.effectBeamPool = product->pendingEffectBeamPoolReceipt;
	candidate.weather = product->weatherPlan;
	if ( distantWorldReady )
		candidate.distantWorldCandidateCount = distantWorld.drawPlan.candidateCount;
	memset( &vertexReceipt, 0, sizeof( vertexReceipt ) );
	memset( &indexReceipt, 0, sizeof( indexReceipt ) );
	memset( &distantVertexReceipt, 0, sizeof( distantVertexReceipt ) );
	memset( &distantIndexReceipt, 0, sizeof( distantIndexReceipt ) );
	memset( &entityVertexReceipt, 0, sizeof( entityVertexReceipt ) );
	memset( &entityIndexReceipt, 0, sizeof( entityIndexReceipt ) );
	memset( &miscVertexReceipt, 0, sizeof( miscVertexReceipt ) );
	memset( &miscIndexReceipt, 0, sizeof( miscIndexReceipt ) );
	worldMaterialEntryCount = world.batchCount + ( distantWorldReady
		? distantWorld.drawPlan.admittedCount : 0u );
	if ( worldMaterialEntryCount > RENDER_SUBMISSION_MAX_WORLD_BATCHES
			+ RAL_SKY_DISTANT_MAX_VISIBLE_DRAWS ) goto fail;
	if ( worldMaterialEntryCount ) {
		worldMaterialData = (float *)calloc( (size_t)worldMaterialEntryCount * RAL_WEBGPU_WORLD_MATERIAL_FLOATS,
			sizeof( worldMaterialData[0] ) );
		if ( !worldMaterialData ) goto fail;
	}
	for ( uint32_t i = 0u; i < world.batchCount; ++i ) {
		renderMaterialSnapshot_t material;
		if ( world.batches[i].baseMaterial <= 0
				|| !RenderSubmission_MaterialSnapshot( frontend,
					world.batches[i].baseMaterial, &material ) ) goto fail;
		for ( uint32_t channel = 0u; channel < 3u; ++channel )
			worldMaterialData[i * RAL_WEBGPU_WORLD_MATERIAL_FLOATS + channel] =
				(float)material.lighting.emissionRadianceQ16[channel]
					/ (float)RENDER_MATERIAL_LIGHTING_Q16_ONE;
		worldMaterialData[i * RAL_WEBGPU_WORLD_MATERIAL_FLOATS + 3u] = world.batches[i].sky ? 1.0f : 0.0f;
		worldMaterialData[i * RAL_WEBGPU_WORLD_MATERIAL_FLOATS + 4u] = 1.0f;
		worldMaterialData[i * RAL_WEBGPU_WORLD_MATERIAL_FLOATS + 5u] = 1.0f;
		worldMaterialData[i * RAL_WEBGPU_WORLD_MATERIAL_FLOATS + 6u] = 1.0f;
		worldMaterialData[i * RAL_WEBGPU_WORLD_MATERIAL_FLOATS + 7u] = 1.0f;
		worldMaterialData[i * RAL_WEBGPU_WORLD_MATERIAL_FLOATS + 8u] = -1.0f;
		worldMaterialData[i * RAL_WEBGPU_WORLD_MATERIAL_FLOATS + 9u] = 1.0f;
		worldMaterialData[i * RAL_WEBGPU_WORLD_MATERIAL_FLOATS + 10u] = 1.0f;
		worldMaterialData[i * RAL_WEBGPU_WORLD_MATERIAL_FLOATS + 11u] = 0.0f;
		/* extras: x = lightmap present (the shader samples the secondary texture only then) */
		worldMaterialData[i * RAL_WEBGPU_WORLD_MATERIAL_FLOATS + 12u] =
			world.batches[i].lightmapMaterial > 0 ? 1.0f : 0.0f;
		/* extras: y = legacy lightmap overbright (hue-preserving, see gen_frag.tmpl) */
		worldMaterialData[i * RAL_WEBGPU_WORLD_MATERIAL_FLOATS + 13u] =
			product->lightmapBoost;
		if ( frontend->pbrEnabled && material.factors.ready == qtrue
				&& material.factors.schemaVersion
					== RENDER_MATERIAL_FACTORS_SCHEMA_VERSION ) {
			memcpy( &worldMaterialData[i * RAL_WEBGPU_WORLD_MATERIAL_FLOATS + 4u],
				material.factors.factors.baseColor, 4u * sizeof( float ) );
			worldMaterialData[i * RAL_WEBGPU_WORLD_MATERIAL_FLOATS + 8u] =
				material.factors.factors.metallic;
			worldMaterialData[i * RAL_WEBGPU_WORLD_MATERIAL_FLOATS + 9u] =
				material.factors.factors.roughness;
			worldMaterialData[i * RAL_WEBGPU_WORLD_MATERIAL_FLOATS + 10u] =
				material.factors.factors.occlusionStrength;
			worldMaterialData[i * RAL_WEBGPU_WORLD_MATERIAL_FLOATS + 11u] = 1.0f;
		}
		if ( material.lighting.emissionRadianceQ16[0]
				|| material.lighting.emissionRadianceQ16[1]
				|| material.lighting.emissionRadianceQ16[2] )
			candidate.worldEmissiveDrawCount++;
	}
	if ( distantWorldReady ) {
		for ( uint32_t selected = 0u;
				selected < distantWorld.drawPlan.admittedCount; ++selected ) {
			const uint32_t batchIndex =
				distantWorld.drawPlan.selections[selected].sourceDrawIndex;
			const renderWorldBatch_t *batch = &distantWorld.world.batches[batchIndex];
			renderMaterialSnapshot_t material;
			float *factors = &worldMaterialData[
				( world.batchCount + selected ) * RAL_WEBGPU_WORLD_MATERIAL_FLOATS];
			if ( batch->baseMaterial <= 0
					|| !RenderSubmission_MaterialSnapshot( frontend,
						batch->baseMaterial, &material ) ) goto fail;
			for ( uint32_t channel = 0u; channel < 3u; ++channel )
				factors[channel] =
					(float)material.lighting.emissionRadianceQ16[channel]
						/ (float)RENDER_MATERIAL_LIGHTING_Q16_ONE;
			factors[3] = 0.0f;
			factors[4] = factors[5] = factors[6] = factors[7] = 1.0f;
			factors[8] = -1.0f;
			factors[9] = factors[10] = 1.0f;
			if ( frontend->pbrEnabled && material.factors.ready == qtrue
					&& material.factors.schemaVersion
						== RENDER_MATERIAL_FACTORS_SCHEMA_VERSION ) {
				memcpy( &factors[4], material.factors.factors.baseColor,
					4u * sizeof( float ) );
				factors[8] = material.factors.factors.metallic;
				factors[9] = material.factors.factors.roughness;
				factors[10] = material.factors.factors.occlusionStrength;
				factors[11] = 1.0f;
			}
			if ( material.lighting.emissionRadianceQ16[0]
					|| material.lighting.emissionRadianceQ16[1]
					|| material.lighting.emissionRadianceQ16[2] )
				candidate.worldEmissiveDrawCount++;
		}
	}
	if ( worldMaterialEntryCount ) {
		if ( !RalWebGpu_WriteBuffer( resources, product->worldMaterials,
				&product->worldMaterialReceipt, 0u, worldMaterialData,
				(uint64_t)worldMaterialEntryCount * RAL_WEBGPU_WORLD_MATERIAL_FLOATS * sizeof( float ),
				&candidate.worldMaterialWrite ) ) goto fail;
		candidate.worldMaterialBuffer = product->worldMaterialReceipt;
	}
	s_productFailureStage = 4u;
	materialCount = frontend->materialCount + frontend->stageTextureCount;
	if ( materialCount > RENDER_SUBMISSION_MAX_TEXTURE_RESOURCES ) goto fail;
	if ( materialCount ) {
		staged = (materialEntry_t *)calloc( materialCount, sizeof( *staged ) );
		resolved = (materialEntry_t **)calloc( materialCount,
			sizeof( *resolved ) );
		stagedMaterial = (qboolean *)calloc( materialCount,
			sizeof( *stagedMaterial ) );
		if ( !staged || !resolved || !stagedMaterial ) goto fail;
	}
	if ( candidate.plan.entityBatchCount
			&& ( product->entityPipelineReady != qtrue
				|| product->entityLightingBindGroupReady != qtrue
				|| !RalWebGpu_RuntimeOwnsPipeline( product->runtime, runtimeReceipt,
					&product->entityPipeline )
				|| !BuildEntityGeometry( frontend, &candidate.plan, &world, &entityVertices,
					&entityVertexCount, &entityIndices, &entityIndexCount,
					&entityDraws, &entityDrawCount )
				|| !( projectedEntities = (ralWebGpuWorldVertex_t *)malloc(
					(size_t)( entityVertexCount ? entityVertexCount : 1u )
						* sizeof( projectedEntities[0] ) ) )
				|| !ProjectVertices( &world, entityVertices, projectedEntities,
					entityVertexCount, taaJitterNdc ) ) ) goto fail;
	if ( entityDrawCount ) {
		if ( entityDrawCount > RAL_WEBGPU_ENTITY_LIGHTING_CAPACITY ) goto fail;
		entityLightingData = calloc( (size_t)entityDrawCount * 16u,
			sizeof( entityLightingData[0] ) );
		if ( !entityLightingData ) goto fail;
		for ( uint32_t i = 0u; i < entityDrawCount; ++i ) {
			memcpy( &entityLightingData[i * 16u], entityDraws[i].localSh,
				16u * sizeof( float ) );
			if ( entityDraws[i].localSh[0][3] > 0.5f )
				candidate.localIrradianceDrawCount++;
		}
		candidate.localIrradianceEntityCount =
			candidate.plan.localIrradianceEntityCount;
		candidate.entityLightingBuffer = product->entityLightingReceipt;
		if ( !RalWebGpu_WriteBuffer( resources, product->entityLighting,
				&product->entityLightingReceipt, 0u, entityLightingData,
				(uint64_t)entityDrawCount * 16u * sizeof( float ),
				&candidate.entityLightingWrite ) ) goto fail;
	}
	if ( candidate.plan.polygonBatchCount || candidate.plan.lightCount
			|| candidate.plan.effectBatchCount
			|| candidate.plan.uiPrimitiveCount ) {
		if ( ( ( candidate.plan.polygonBatchCount || candidate.plan.lightCount
					|| candidate.plan.effectBatchCount )
					&& !EffectPipelinesOwned( product, runtimeReceipt ) )
				|| ( candidate.plan.uiPrimitiveCount
					&& ( product->uiPipelinesReady != qtrue
						|| !RalWebGpu_RuntimeOwnsPipeline( product->runtime,
							runtimeReceipt, &product->uiPipeline )
						|| !RalWebGpu_RuntimeOwnsPipeline( product->runtime,
							runtimeReceipt, &product->msdfPipeline ) ) )
				|| !BuildMiscGeometry( frontend, &candidate.plan,
					&product->pendingEffectDecalPool,
					&product->pendingEffectDecalPoolReceipt,
					&product->pendingEffectBeamPool,
					&product->pendingEffectBeamPoolReceipt,
					product->viewportWidth, product->viewportHeight, &miscVertices,
					&miscVertexCount, &miscIndices, &miscIndexCount, &miscDraws,
					&miscDrawCount ) ) goto fail;
		{
			const uint32_t projectedCount = miscVertexCount
				- candidate.plan.uiPrimitiveCount * 4u;
			/* Projection stores clip-W in texCoord.x. Preserve effect UV in the
			 * otherwise-unused normal.xy vertex lane for the native WGSL effect
			 * variants before projecting the world-space geometry. */
			for ( uint32_t i = 0u; i < projectedCount; ++i ) {
				miscVertices[i].normal[0] = miscVertices[i].texCoord[0];
				miscVertices[i].normal[1] = miscVertices[i].texCoord[1];
			}
			if ( projectedCount && !ProjectVerticesInPlace( &world, miscVertices, projectedCount ) ) goto fail;
		}
	}
	s_productFailureStage = 5u;
	effectParameterData = calloc((size_t)candidate.plan.effectBeamCount * RENDER_MATERIAL_MAX_STAGES + 1u,
		sizeof(*effectParameterData));
	if (!effectParameterData) goto fail;
	candidate.effectParameterCount = 1u;
	for (uint32_t i = 0; i < miscDrawCount; ++i) if (miscDraws[i].effectFamily == MISC_EFFECT_FAMILY_BEAM) {
		if (candidate.effectParameterCount >= RAL_WEBGPU_EFFECT_PARAMETER_CAPACITY) goto fail;
		miscDraws[i].effectParameterIndex = candidate.effectParameterCount;
		effectParameterData[candidate.effectParameterCount++] = miscDraws[i].effectParams;
	}
	if (!RalWebGpu_WriteBuffer(resources, product->effectParameters, &product->effectParameterReceipt,
			0u, effectParameterData, (uint64_t)candidate.effectParameterCount * sizeof(*effectParameterData),
			&candidate.effectParameterWrite)) goto fail;
	candidate.effectParameterBuffer = product->effectParameterReceipt;
	for ( uint32_t i = 0u; i < materialCount; ++i ) {
		renderTextureSnapshot_t texture;
		const qhandle_t handle = i < frontend->materialCount ? frontend->materials[i].snapshot.handle
			: frontend->stageTextures[i - frontend->materialCount].snapshot.handle;
		materialEntry_t *cached = FindMaterial( product, handle );
		if ( !RenderSubmission_TextureSnapshot( frontend, handle,
				&texture ) ) goto fail;
		if ( cached && ( ( cached->generation == texture.generation
				&& cached->digest == texture.digest )
				|| ( cached->clampToEdge == texture.clampToEdge
					&& RenderSubmission_TextureResourceMatches( &texture,
						&cached->runtimeResource ) ) ) ) {
			cached->generation = texture.generation;
			cached->digest = texture.digest;
			resolved[i] = cached; candidate.reusedMaterialCount++; continue;
		}
		if ( !cached && product->materialCount + ++newMaterialCount
				> RENDER_SUBMISSION_MAX_TEXTURE_RESOURCES ) goto fail;
		if ( stagedCount >= RENDER_SUBMISSION_MAX_TEXTURE_RESOURCES
				|| !UploadMaterial( resources, &texture, &staged[stagedCount] ) )
			goto fail;
		resolved[i] = &staged[stagedCount];
		stagedMaterial[i] = qtrue;
		stagedCount++; candidate.uploadedMaterialCount++;
	}
	for ( uint32_t i = 0u; i < frontend->materialCount; ++i )
		if ( product->cookedMaterialReady[i]
				&& ( !resolved[i] || product->cookedMaterialTextureIdentities[i]
					!= resolved[i]->textureReceipt.resourceIdentity ) ) goto fail;
	s_productFailureStage = 6u; memset( &vertexDesc, 0, sizeof( vertexDesc ) );
	uint32_t skyDrawCount=0;
	for(uint32_t i=0;i<world.batchCount;++i) if(world.batches[i].sky) ++skyDrawCount;
	if(skyDrawCount) {
		const uint64_t generation=frontend->worlds[worldIndex].lifecycleGeneration;
		const uint64_t size=(uint64_t)world.vertexCount*sizeof(world.vertices[0]);
		if(!product->skyMaterial||!generation||!size
			||!RalWebGpu_SkyMaterialPrepare(product->skyMaterial,runtimeReceipt,skyDrawCount)) goto fail;
		if(!product->skyVertices[worldIndex]||product->skyVertexGenerations[worldIndex]!=generation
			||product->skyVertexReceipts[worldIndex].byteSize!=size) {
			ralWebGpuResource_t *raw=NULL;
			ralWebGpuResourceReceipt_t rawReceipt;
			ralWebGpuWriteReceipt_t write;
			ralWebGpuBufferDesc_t desc={0}; desc.size=size;
			desc.usage=RAL_WEBGPU_BUFFER_VERTEX|RAL_WEBGPU_BUFFER_COPY_DESTINATION;
			desc.memoryClass=RAL_ALLOCATION_DEVICE_LOCAL;
			if(!RalWebGpu_CreateBuffer(resources,&desc,&raw,&rawReceipt)) goto fail;
			if(!RalWebGpu_WriteBuffer(resources,raw,&rawReceipt,0,world.vertices,size,&write)) {
				RalWebGpu_DestroyResource(resources,raw,&rawReceipt); goto fail;
			}
			DestroySkyVertices(product,resources,worldIndex);
			product->skyVertices[worldIndex]=raw;
			product->skyVertexReceipts[worldIndex]=rawReceipt;
			product->skyVertexGenerations[worldIndex]=generation;
		}
	}
	if ( world.vertexCount ) {
		projectedWorld = (ralWebGpuWorldVertex_t *)malloc(
			(uint64_t)world.vertexCount * sizeof( projectedWorld[0] ) );
		if ( !projectedWorld ) goto fail;
		if ( !ProjectVertices( &world, world.vertices, projectedWorld,
				world.vertexCount, taaJitterNdc ) ) goto fail;
		vertexDesc.size = (uint64_t)world.vertexCount * sizeof( projectedWorld[0] );
		vertexDesc.usage = RAL_WEBGPU_BUFFER_VERTEX | RAL_WEBGPU_BUFFER_COPY_DESTINATION;
		vertexDesc.memoryClass = RAL_ALLOCATION_DEVICE_LOCAL;
		memset( &indexDesc, 0, sizeof( indexDesc ) );
		indexDesc.size = (uint64_t)world.indexCount * sizeof( world.indices[0] );
		indexDesc.usage = RAL_WEBGPU_BUFFER_INDEX | RAL_WEBGPU_BUFFER_COPY_DESTINATION;
		indexDesc.memoryClass = RAL_ALLOCATION_DEVICE_LOCAL;
		if ( !RalWebGpu_CreateBuffer( resources, &vertexDesc, &vertices,
				&vertexReceipt ) || !RalWebGpu_WriteBuffer( resources, vertices,
				&vertexReceipt, 0u, projectedWorld, vertexDesc.size,
				&candidate.worldVertexWrite )
				|| !RalWebGpu_CreateBuffer( resources, &indexDesc, &indices,
					&indexReceipt ) || !RalWebGpu_WriteBuffer( resources, indices,
				&indexReceipt, 0u, world.indices, indexDesc.size,
				&candidate.worldIndexWrite ) ) goto fail;
	}
	if ( distantWorldReady ) {
		const renderWorldSnapshot_t *distant = &distantWorld.world;
		projectedDistantWorld = (ralWebGpuWorldVertex_t *)malloc(
			(uint64_t)distant->vertexCount * sizeof( projectedDistantWorld[0] ) );
		if ( !projectedDistantWorld ) goto fail;
		if ( !ProjectVertices( distant, distant->vertices, projectedDistantWorld,
				distant->vertexCount, taaJitterNdc ) ) goto fail;
		memset( &vertexDesc, 0, sizeof( vertexDesc ) );
		vertexDesc.size = (uint64_t)distant->vertexCount
			* sizeof( projectedDistantWorld[0] );
		vertexDesc.usage = RAL_WEBGPU_BUFFER_VERTEX
			| RAL_WEBGPU_BUFFER_COPY_DESTINATION;
		vertexDesc.memoryClass = RAL_ALLOCATION_DEVICE_LOCAL;
		memset( &indexDesc, 0, sizeof( indexDesc ) );
		indexDesc.size = (uint64_t)distant->indexCount
			* sizeof( distant->indices[0] );
		indexDesc.usage = RAL_WEBGPU_BUFFER_INDEX
			| RAL_WEBGPU_BUFFER_COPY_DESTINATION;
		indexDesc.memoryClass = RAL_ALLOCATION_DEVICE_LOCAL;
		if ( !RalWebGpu_CreateBuffer( resources, &vertexDesc,
				&distantVertices, &distantVertexReceipt )
				|| !RalWebGpu_WriteBuffer( resources, distantVertices,
					&distantVertexReceipt, 0u, projectedDistantWorld,
					vertexDesc.size, &candidate.distantWorldVertexWrite )
				|| !RalWebGpu_CreateBuffer( resources, &indexDesc,
					&distantIndices, &distantIndexReceipt )
				|| !RalWebGpu_WriteBuffer( resources, distantIndices,
					&distantIndexReceipt, 0u, distant->indices, indexDesc.size,
					&candidate.distantWorldIndexWrite ) ) goto fail;
	}
	if ( entityDrawCount ) {
		memset( &vertexDesc, 0, sizeof( vertexDesc ) );
		/* Entities keep their authored UVs: projected into the same vertex
		 * layout as the world so the entity pipeline can sample its texture. */
		vertexDesc.size = (uint64_t)entityVertexCount * sizeof( projectedEntities[0] );
		vertexDesc.usage = RAL_WEBGPU_BUFFER_VERTEX | RAL_WEBGPU_BUFFER_COPY_DESTINATION;
		vertexDesc.memoryClass = RAL_ALLOCATION_DEVICE_LOCAL;
		memset( &indexDesc, 0, sizeof( indexDesc ) );
		indexDesc.size = (uint64_t)entityIndexCount * sizeof( entityIndices[0] );
		indexDesc.usage = RAL_WEBGPU_BUFFER_INDEX | RAL_WEBGPU_BUFFER_COPY_DESTINATION;
		indexDesc.memoryClass = RAL_ALLOCATION_DEVICE_LOCAL;
		if ( !RalWebGpu_CreateBuffer( resources, &vertexDesc,
				&entityVertexResource, &entityVertexReceipt )
				|| !RalWebGpu_WriteBuffer( resources, entityVertexResource,
					&entityVertexReceipt, 0u, projectedEntities, vertexDesc.size,
					&candidate.entityVertexWrite )
				|| !RalWebGpu_CreateBuffer( resources, &indexDesc,
					&entityIndexResource, &entityIndexReceipt )
				|| !RalWebGpu_WriteBuffer( resources, entityIndexResource,
					&entityIndexReceipt, 0u, entityIndices, indexDesc.size,
					&candidate.entityIndexWrite ) ) goto fail;
	}
	if ( miscDrawCount ) {
		memset( &vertexDesc, 0, sizeof( vertexDesc ) );
		vertexDesc.size = (uint64_t)miscVertexCount * sizeof( miscVertices[0] );
		vertexDesc.usage = RAL_WEBGPU_BUFFER_VERTEX | RAL_WEBGPU_BUFFER_COPY_DESTINATION;
		vertexDesc.memoryClass = RAL_ALLOCATION_DEVICE_LOCAL;
		memset( &indexDesc, 0, sizeof( indexDesc ) );
		indexDesc.size = (uint64_t)miscIndexCount * sizeof( miscIndices[0] );
		indexDesc.usage = RAL_WEBGPU_BUFFER_INDEX | RAL_WEBGPU_BUFFER_COPY_DESTINATION;
		indexDesc.memoryClass = RAL_ALLOCATION_DEVICE_LOCAL;
		if ( !RalWebGpu_CreateBuffer( resources, &vertexDesc,
				&miscVertexResource, &miscVertexReceipt )
				|| !RalWebGpu_WriteBuffer( resources, miscVertexResource,
					&miscVertexReceipt, 0u, miscVertices, vertexDesc.size,
					&candidate.miscVertexWrite )
				|| !RalWebGpu_CreateBuffer( resources, &indexDesc,
					&miscIndexResource, &miscIndexReceipt )
				|| !RalWebGpu_WriteBuffer( resources, miscIndexResource,
					&miscIndexReceipt, 0u, miscIndices, indexDesc.size,
					&candidate.miscIndexWrite ) ) goto fail;
	}
	s_productFailureStage = 7u;
    if (submission->sceneRendered && product->exposure && !RalWebGpu_ExposurePrepare(product->exposure,
            runtimeReceipt,product->viewportWidth,product->viewportHeight,
            frontend->worlds[worldIndex].lifecycleGeneration,world.timeMs,
            (world.rdflags & (RDF_CAMERA_CUT | RDF_HYPERSPACE)) ? qtrue : qfalse,
            &frameExposure,&product->exposureVisibility,&sceneTarget)) goto fail;
	if ( submission->sceneRendered ) {
		if ( !RalWebGpu_CommandBegin( command, RAL_WEBGPU_PASS_RENDER,
				sceneTarget, &recording ) ) goto fail;
	} else {
		ralWebGpuPassDesc_t uiPass;
		memset( &uiPass, 0, sizeof( uiPass ) );
		uiPass.kind = RAL_WEBGPU_PASS_RENDER;
		uiPass.targetIdentity = targetIdentity;
		uiPass.colorLoadOp = RAL_WEBGPU_LOAD_CLEAR;
		if ( !RalWebGpu_CommandBeginWithDesc( command, &uiPass, &recording ) ) goto fail;
	}
	commandActive = qtrue;
	s_productFailureStage = 8u;
	for ( uint32_t i = 0u; i < world.batchCount; ++i ) {
		const renderWorldBatch_t *batch = &world.batches[i];
		if ( distantWorldReady && !batch->sky ) continue;
		s_productFailureStage = 84u;
		if ( !RecordWorldBatch( product, runtimeReceipt, command, frontend, materialCount,
				resolved, &world, batch, &vertexReceipt, &indexReceipt, i,
				candidate.plan.loweringDigest, &recording, &updated ) ) goto fail;
		s_productFailureStage = 8u;
		recording = updated; candidate.worldDrawCount++;
		candidate.atmosphereBoundDrawCount++;
	}
	if ( distantWorldReady ) {
		ralWebGpuPassDesc_t nextPass;
		memset( &nextPass, 0, sizeof( nextPass ) );
		nextPass.kind = RAL_WEBGPU_PASS_RENDER;
		nextPass.targetIdentity = sceneTarget;
		nextPass.colorLoadOp = RAL_WEBGPU_LOAD_PRESERVE;
		nextPass.depthEnabled = qtrue;
		nextPass.depthLoadOp = RAL_WEBGPU_LOAD_CLEAR;
		nextPass.depthStore = qtrue;
		nextPass.clearDepth = 1.0f;
		s_productFailureStage = 85u;
		if ( !RalWebGpu_CommandNextRenderPass( command, &recording,
				&nextPass, &updated ) ) goto fail;
		recording = updated;
		for ( uint32_t selected = 0u;
				selected < distantWorld.drawPlan.admittedCount; ++selected ) {
			const uint32_t batchIndex =
				distantWorld.drawPlan.selections[selected].sourceDrawIndex;
			const renderWorldBatch_t *batch =
				&distantWorld.world.batches[batchIndex];
			if ( !RecordWorldBatch( product, runtimeReceipt, command, frontend, materialCount,
					resolved, &distantWorld.world, batch,
					&distantVertexReceipt, &distantIndexReceipt,
					world.batchCount + selected,
					candidate.plan.loweringDigest ^ distantWorld.contentIdentity,
					&recording, &updated ) ) goto fail;
			recording = updated;
			candidate.distantWorldDrawCount++;
			candidate.distantWorldIndexCount += batch->indexCount;
			candidate.atmosphereBoundDrawCount++;
		}
		s_productFailureStage = 86u;
		if ( !RalWebGpu_CommandNextRenderPass( command, &recording,
				&nextPass, &updated ) ) goto fail;
		recording = updated;
		candidate.distantWorldDepthReset = qtrue;
		for ( uint32_t i = 0u; i < world.batchCount; ++i ) {
			const renderWorldBatch_t *batch = &world.batches[i];
			if ( batch->sky ) continue;
			if ( !RecordWorldBatch( product, runtimeReceipt, command, frontend, materialCount,
					resolved, &world, batch, &vertexReceipt, &indexReceipt, i,
					candidate.plan.loweringDigest, &recording, &updated ) ) goto fail;
			recording = updated; candidate.worldDrawCount++;
			candidate.atmosphereBoundDrawCount++;
		}
	}
	s_productFailureStage = 9u;
	if ( taaActive && entityDrawCount ) {
		taaMaskDraws = calloc( entityDrawCount, sizeof( *taaMaskDraws ) );
		if ( !taaMaskDraws ) goto fail;
	}
	/* Two phases, as the effect primitives follow the depth-writing geometry: depth-writing entity batches first, then the
	 * translucent ones with their material's blend (no depth write). Frames without translucent entities record exactly as before. */
	for ( uint32_t phase = 0u; phase < 2u; ++phase ) for ( uint32_t i = 0u; i < entityDrawCount; ++i ) {
		materialEntry_t *material = NULL;
		ralWebGpuIndexedDraw_t draw;
		const uint32_t blendIndex = product->entityBlendPipelinesReady
			? EntityBlendIndex( frontend, entityDraws[i].material ) : 0u;
		if ( ( blendIndex ? 1u : 0u ) != phase ) continue;
		for ( uint32_t m = 0u; m < materialCount; ++m )
			if (resolved[m] && resolved[m]->handle == entityDraws[i].material)
				material = resolved[m];
		if ( !material ) goto fail;
		memset( &draw, 0, sizeof( draw ) ); draw.kind = RAL_WEBGPU_DRAW_ENTITY;
		draw.pipelineIdentity = blendIndex ? product->entityBlendPipelines[blendIndex - 1u].pipelineIdentity
			: product->entityPipeline.pipelineIdentity;
		draw.vertexBufferIdentity = entityVertexReceipt.resourceIdentity;
		draw.indexBufferIdentity = entityIndexReceipt.resourceIdentity;
		draw.textureIdentity = material->textureReceipt.resourceIdentity;
		draw.samplerIdentity = material->samplerReceipt.resourceIdentity;
		draw.textured = qtrue;
		/* Group 2 of the entity pipeline: base texture + sampler. */
		draw.textureBindGroupSlot = 3u;
		draw.firstIndex = entityDraws[i].firstIndex;
		draw.indexCount = entityDraws[i].indexCount;
		draw.instanceCount = 1u; draw.contentDigest = entityDraws[i].contentDigest;
		draw.firstInstance = i;
		draw.bindGroupCount = 2u;
		draw.bindGroupIdentities[0] = product->atmosphereBindGroups[1];
		draw.bindGroupIdentities[1] = product->entityLightingBindGroup;
		if ( !RalWebGpu_CommandRecordIndexedDraw( command, &recording, &draw,
				&updated ) ) goto fail;
		recording = updated; candidate.entityDrawCount++;
		if ( taaMaskDraws ) {
			/* Opaque batches mark their geometry in the effect mask; translucent ones add their visible contribution. */
			draw.pipelineIdentity = blendIndex ? product->taaTrustPipelines[blendIndex - 1u].pipelineIdentity
				: product->taaMaskPipeline.pipelineIdentity;
			taaMaskDraws[taaMaskDrawCount++] = draw;
		}
		candidate.atmosphereBoundDrawCount++;
	}
	if ( !candidate.weather.zeroWork ) {
		ralWebGpuIndexedDraw_t draw;
		s_productFailureStage = 95u;
		memset( &draw, 0, sizeof( draw ) );
		draw.kind = RAL_WEBGPU_DRAW_EFFECT;
		draw.pipelineIdentity =
			product->weatherExecution.renderPipeline.pipelineIdentity;
		draw.vertexBufferIdentity =
			product->weatherExecution.vertexBufferIdentity;
		draw.indexBufferIdentity =
			product->weatherExecution.indexBufferIdentity;
		draw.textured = qfalse;
		draw.indexCount = product->weatherExecution.drawIndexCount;
		draw.instanceCount = candidate.weather.activeParticleCount;
		draw.contentDigest = HashU64( candidate.plan.loweringDigest,
			( (uint64_t)candidate.weather.familyMask << 32u )
				| candidate.weather.activeParticleCount );
		if ( !draw.contentDigest ) draw.contentDigest = 1u;
		draw.bindGroupCount = 1u;
		draw.bindGroupIdentities[0] =
			product->weatherExecution.renderBindGroupIdentity;
		if ( !RalWebGpu_CommandRecordIndexedDraw( command, &recording, &draw,
				&updated ) ) goto fail;
		recording = updated; candidate.weatherDrawCount = 1u;
	}
	s_productFailureStage = 10u;
	for ( uint32_t i = 0u; i < miscDrawCount; ++i ) {
		materialEntry_t *material = NULL;
		ralWebGpuIndexedDraw_t draw;
		if ( submission->sceneRendered && miscDraws[i].kind == RAL_WEBGPU_DRAW_UI && !candidate.uiDrawCount ) {
            if (product->exposure) {
                if (!RecordToneMap(product,runtimeReceipt,command,&recording,targetIdentity,sceneTarget,
                        taaActive?&taaFrame:NULL,taaMaskDraws,taaMaskDrawCount,&candidate)) goto fail;
                candidate.exposureDispatchCount=frameExposure.enabled?2u:0u;
            }
			/* BuildMiscGeometry appends UI after every scene effect. Keep UI in
			 * its own output pass so scene metering/tonemap can precede it. */
			ralWebGpuPassDesc_t uiPass;
			memset( &uiPass, 0, sizeof(uiPass) );
			uiPass.kind = RAL_WEBGPU_PASS_RENDER;
			uiPass.targetIdentity = targetIdentity;
			uiPass.colorLoadOp = RAL_WEBGPU_LOAD_PRESERVE;
			if ( !RalWebGpu_CommandNextPass( command, &recording, &uiPass, &updated ) ) goto fail;
			recording = updated;
		} else if ( miscDraws[i].kind != RAL_WEBGPU_DRAW_UI && candidate.uiDrawCount ) goto fail;
		if ( miscDraws[i].textured ) {
			for ( uint32_t m = 0u; m < materialCount; ++m )
				if (resolved[m] && resolved[m]->handle == miscDraws[i].material) material = resolved[m];
			if ( !material ) goto fail;
		}
		memset( &draw, 0, sizeof( draw ) ); draw.kind = miscDraws[i].kind;
		if ( miscDraws[i].kind == RAL_WEBGPU_DRAW_EFFECT
				&& miscDraws[i].effectPipeline
					>= RAL_WEBGPU_EFFECT_PIPELINE_COUNT ) goto fail;
		draw.pipelineIdentity = miscDraws[i].kind == RAL_WEBGPU_DRAW_UI
			? ( miscDraws[i].msdf ? product->msdfPipeline.pipelineIdentity
				: product->uiPipeline.pipelineIdentity )
			: product->effectPipelines[
				miscDraws[i].effectPipeline].pipelineIdentity;
		draw.vertexBufferIdentity = miscVertexReceipt.resourceIdentity;
		draw.indexBufferIdentity = miscIndexReceipt.resourceIdentity;
		draw.textured = miscDraws[i].textured;
		if ( miscDraws[i].kind == RAL_WEBGPU_DRAW_UI )
			draw.textureBindGroupSlot = 1u;
		else if ( miscDraws[i].kind == RAL_WEBGPU_DRAW_EFFECT
				&& miscDraws[i].effectPipeline
					!= RAL_WEBGPU_EFFECT_PIPELINE_LEGACY )
			draw.textureBindGroupSlot = 2u;
		if ( material ) {
			draw.textureIdentity = material->textureReceipt.resourceIdentity;
			draw.samplerIdentity = material->samplerReceipt.resourceIdentity;
		}
		draw.firstIndex = miscDraws[i].firstIndex;
		draw.indexCount = miscDraws[i].indexCount;
		draw.instanceCount = 1u; draw.contentDigest = miscDraws[i].contentDigest;
		draw.firstInstance = miscDraws[i].effectParameterIndex;
		if ( miscDraws[i].kind != RAL_WEBGPU_DRAW_UI ) {
			draw.bindGroupCount = 1u;
			draw.bindGroupIdentities[0] = product->atmosphereBindGroups[
				2u + miscDraws[i].effectPipeline];
		}
		if ( !RalWebGpu_CommandRecordIndexedDraw( command, &recording, &draw,
				&updated ) ) goto fail;
		recording = updated;
		if ( miscDraws[i].effectFamily == MISC_EFFECT_FAMILY_POLYGON ) {
			candidate.polygonDrawCount++; candidate.atmosphereBoundDrawCount++;
		}
		else if ( miscDraws[i].effectFamily == MISC_EFFECT_FAMILY_LIGHT )
			{ candidate.lightDrawCount++; candidate.atmosphereBoundDrawCount++; }
		else if ( miscDraws[i].effectFamily == MISC_EFFECT_FAMILY_SPRITE ) {
			candidate.effectSpriteDrawCount++; candidate.atmosphereBoundDrawCount++;
		}
		else if ( miscDraws[i].effectFamily == MISC_EFFECT_FAMILY_DECAL ) {
			candidate.effectDecalDrawCount++; candidate.atmosphereBoundDrawCount++;
		}
		else if ( miscDraws[i].effectFamily == MISC_EFFECT_FAMILY_RIBBON ) {
			candidate.effectRibbonDrawCount++; candidate.atmosphereBoundDrawCount++;
		}
		else if ( miscDraws[i].effectFamily == MISC_EFFECT_FAMILY_BEAM ) {
			if (miscDraws[i].beamFirstStage) candidate.effectBeamDrawCount++;
			candidate.effectBeamStageDrawCount++; candidate.atmosphereBoundDrawCount++;
		}
		else if ( miscDraws[i].kind == RAL_WEBGPU_DRAW_UI )
			candidate.uiDrawCount++;
		else goto fail;
	}
	s_productFailureStage = 11u;
    if (submission->sceneRendered && product->exposure && !candidate.toneMapDrawCount) {
        if (!RecordToneMap(product,runtimeReceipt,command,&recording,targetIdentity,sceneTarget,
                taaActive?&taaFrame:NULL,taaMaskDraws,taaMaskDrawCount,&candidate)) goto fail;
        candidate.exposureDispatchCount=frameExposure.enabled?2u:0u;
    }
	if ( !RalWebGpu_CommandEnd( command, &recording, &executable ) ) {
		commandActive = qfalse; goto fail;
	}
	recording = executable;
	s_productFailureStage = 12u;
	if ( !RalWebGpu_CommandSubmit( command, &executable,
			&candidate.submission ) ) goto fail;
	commandActive = qfalse;
	for ( uint32_t i = 0u, stagedIndex = 0u; i < materialCount; ++i ) {
		if ( !stagedMaterial[i] ) continue;
		materialEntry_t *cached = FindMaterial( product, resolved[i]->handle );
		if ( cached ) DestroyMaterial( resources, cached );
		else cached = &product->materials[product->materialCount++];
		*cached = *resolved[i]; memset( &staged[stagedIndex++], 0,
			sizeof( staged[0] ) );
	}
	DestroyFrameResources( product, resources );
	if ( world.vertexCount ) {
		DestroyWorldResources( product, resources, worldIndex );
		product->worlds[worldIndex].vertices = vertices;
		product->worlds[worldIndex].indices = indices;
		product->worlds[worldIndex].vertexReceipt = vertexReceipt;
		product->worlds[worldIndex].indexReceipt = indexReceipt;
	}
	product->entityVertices = entityVertexResource;
	product->entityIndices = entityIndexResource;
	product->miscVertices = miscVertexResource;
	product->miscIndices = miscIndexResource;
	if ( distantWorldReady ) {
		DestroyWorldResources( product, resources, distantWorldIndex );
		product->worlds[distantWorldIndex].vertices = distantVertices;
		product->worlds[distantWorldIndex].indices = distantIndices;
		product->worlds[distantWorldIndex].vertexReceipt = distantVertexReceipt;
		product->worlds[distantWorldIndex].indexReceipt = distantIndexReceipt;
	}
	product->entityVertexReceipt = entityVertexReceipt;
	product->entityIndexReceipt = entityIndexReceipt;
	product->miscVertexReceipt = miscVertexReceipt;
	product->miscIndexReceipt = miscIndexReceipt;
	candidate.schemaVersion = RAL_WEBGPU_PRODUCT_SCHEMA_VERSION;
	candidate.backendType = RAL_BACKEND_WEBGPU;
	candidate.backendGeneration = runtimeReceipt->generation;
	candidate.productGeneration = product->generation;
	candidate.frameGeneration = frameGeneration;
	candidate.targetIdentity = targetIdentity;
	candidate.worldVertexBuffer = vertexReceipt;
	candidate.worldIndexBuffer = indexReceipt;
	if ( distantWorldReady ) {
		candidate.distantWorldVertexBuffer = distantVertexReceipt;
		candidate.distantWorldIndexBuffer = distantIndexReceipt;
	}
	if ( entityDrawCount ) {
		candidate.entityVertexBuffer = entityVertexReceipt;
		candidate.entityIndexBuffer = entityIndexReceipt;
	}
	if ( miscDrawCount ) {
		candidate.miscVertexBuffer = miscVertexReceipt;
		candidate.miscIndexBuffer = miscIndexReceipt;
	}
	candidate.command = executable;
	candidate.modelEntityCount = candidate.plan.modelEntityCount;
	candidate.primitiveEntityCount = candidate.plan.primitiveEntityCount;
	candidate.temporalEntityCount = candidate.plan.temporalEntityCount;
	candidate.deferredNonWorldDrawCount = candidate.plan.drawCount
		- candidate.worldDrawCount - candidate.entityDrawCount
		- candidate.polygonDrawCount - candidate.lightDrawCount
		- candidate.effectSpriteDrawCount - candidate.effectDecalDrawCount
		- candidate.effectRibbonDrawCount
		- candidate.effectBeamDrawCount
		- candidate.uiDrawCount;
	candidate.ready = qtrue;
	s_productFailureStage = 13u;
	if ( !ReceiptValid( &candidate ) ) goto fail_committed;
	free( entityVertices ); free( entityIndices ); free( entityDraws ); free( taaMaskDraws );
	free( entityLightingData );
	free( effectParameterData );
	free( worldMaterialData );
	free( miscVertices ); free( miscIndices ); free( miscDraws );
	free( projectedWorld ); free( projectedEntities );
	free( projectedDistantWorld );
	free( stagedMaterial ); free( resolved ); free( staged );
	product->published = candidate; product->inFlight = qtrue;
	s_productFailureStage = 0u; *outReceipt = candidate; return qtrue;

fail:
	if ( commandActive ) RalWebGpu_CommandCancel( command, &recording );
    RalWebGpu_ExposureComplete(product->exposure,qfalse);
	TaaFinish( product, qfalse );
	free( entityVertices ); free( entityIndices ); free( entityDraws ); free( taaMaskDraws );
	free( entityLightingData );
	free( effectParameterData );
	free( worldMaterialData );
	free( miscVertices ); free( miscIndices ); free( miscDraws );
	free( projectedWorld ); free( projectedEntities );
	free( projectedDistantWorld );
	for ( uint32_t i = 0u; i < stagedCount; ++i )
		DestroyMaterial( resources, &staged[i] );
	free( stagedMaterial ); free( resolved ); free( staged );
	if ( indices ) RalWebGpu_DestroyResource( resources, indices, &indexReceipt );
	if ( vertices ) RalWebGpu_DestroyResource( resources, vertices, &vertexReceipt );
	if ( distantIndices ) RalWebGpu_DestroyResource( resources,
		distantIndices, &distantIndexReceipt );
	if ( distantVertices ) RalWebGpu_DestroyResource( resources,
		distantVertices, &distantVertexReceipt );
	if ( entityIndexResource ) RalWebGpu_DestroyResource( resources,
		entityIndexResource, &entityIndexReceipt );
	if ( entityVertexResource ) RalWebGpu_DestroyResource( resources,
		entityVertexResource, &entityVertexReceipt );
	if ( miscIndexResource ) RalWebGpu_DestroyResource( resources,
		miscIndexResource, &miscIndexReceipt );
	if ( miscVertexResource ) RalWebGpu_DestroyResource( resources,
		miscVertexResource, &miscVertexReceipt );
	return qfalse;
fail_committed:
	free( entityVertices ); free( entityIndices ); free( entityDraws ); free( taaMaskDraws );
	free( entityLightingData );
	free( effectParameterData );
	free( worldMaterialData );
	free( miscVertices ); free( miscIndices ); free( miscDraws );
	free( projectedWorld ); free( projectedEntities );
	free( projectedDistantWorld );
	free( stagedMaterial ); free( resolved ); free( staged );
	if ( world.vertexCount ) {
		DestroyWorldResources( product, resources, worldIndex );
		product->worlds[worldIndex].vertices = vertices;
		product->worlds[worldIndex].indices = indices;
		product->worlds[worldIndex].vertexReceipt = vertexReceipt;
		product->worlds[worldIndex].indexReceipt = indexReceipt;
	}
	if ( distantWorldReady ) {
		DestroyWorldResources( product, resources, distantWorldIndex );
		product->worlds[distantWorldIndex].vertices = distantVertices;
		product->worlds[distantWorldIndex].indices = distantIndices;
		product->worlds[distantWorldIndex].vertexReceipt = distantVertexReceipt;
		product->worlds[distantWorldIndex].indexReceipt = distantIndexReceipt;
	}
	product->entityVertices = entityVertexResource;
	product->entityIndices = entityIndexResource;
	product->entityVertexReceipt = entityVertexReceipt;
	product->entityIndexReceipt = entityIndexReceipt;
	product->miscVertices = miscVertexResource;
	product->miscIndices = miscIndexResource;
	product->miscVertexReceipt = miscVertexReceipt;
	product->miscIndexReceipt = miscIndexReceipt;
	product->published = candidate; product->inFlight = qtrue;
	return qfalse;
}

qboolean RalWebGpu_ProductRender( ralWebGpuProduct_t *product,
		const ralWebGpuRuntimeReceipt_t *runtimeReceipt,
		renderSubmissionState_t *frontend,
		const renderSubmissionReceipt_t *submission, uintptr_t targetIdentity,
		uint64_t frameGeneration, ralWebGpuProductFrameReceipt_t *outReceipt ) {
	ralWebGpuFrontendPlanReceipt_t plan;
	if ( !RalWebGpu_ProductPlan( product, runtimeReceipt, frontend, submission,
			&plan ) ) {
		s_productFailureStage = 16u; return qfalse;
	}
	return RalWebGpu_ProductRenderPlan( product, runtimeReceipt, frontend,
		submission, &plan, NULL, targetIdentity, frameGeneration, outReceipt );
}

ralWebGpuAsyncStatus_t RalWebGpu_ProductPoll( ralWebGpuProduct_t *product,
		const ralWebGpuProductFrameReceipt_t *receipt ) {
	ralWebGpuCommand_t *command;
	ralWebGpuResourceLayer_t *resources;
	ralWebGpuRuntimeReceipt_t runtimeReceipt;
	ralWebGpuAsyncStatus_t status;
	if ( !product || !receipt || !product->inFlight
			|| !RalWebGpu_ProductFrameReceiptExact( receipt,
				&product->published )
			|| !RalWebGpu_RuntimeGetReceipt( product->runtime, &runtimeReceipt ) )
		return RAL_WEBGPU_ASYNC_FAILED;
	command = RalWebGpu_RuntimeCommand( product->runtime, &runtimeReceipt );
	resources = RalWebGpu_RuntimeResources( product->runtime, &runtimeReceipt );
	if ( !command || !resources ) return RAL_WEBGPU_ASYNC_DEVICE_LOST;
	status = RalWebGpu_CommandPoll( command, &receipt->submission );
	if ( status == RAL_WEBGPU_ASYNC_READY ) {
        RalWebGpu_ExposureComplete(product->exposure,qtrue);
		TaaFinish( product, qtrue );
		product->effectDecalPool = product->pendingEffectDecalPool;
		product->effectBeamPool = product->pendingEffectBeamPool;
		DestroyFrameResources( product, resources ); product->inFlight = qfalse;
	} else if ( status != RAL_WEBGPU_ASYNC_PENDING ) {
        RalWebGpu_ExposureComplete(product->exposure,qfalse);
		TaaFinish( product, qfalse );
		DestroyFrameResources( product, resources ); product->inFlight = qfalse;
	}
	return status;
}
