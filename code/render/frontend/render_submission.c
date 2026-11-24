// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "render_submission.h"
#include "maps/map_format_registry.h"

#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define FNV_OFFSET UINT64_C( 1469598103934665603 )
#define FNV_PRIME  UINT64_C( 1099511628211 )

static void RebuildMaterialDigest( renderSubmissionState_t *state );

static qboolean PositionQ16( float value, int32_t *outValue )
{
	double scaled;
	int64_t rounded;
	if ( !outValue || !isfinite( value ) ) return qfalse;
	scaled = (double)value * (double)RAL_LIGHT_Q16_ONE;
	rounded = (int64_t)( scaled < 0.0 ? scaled - 0.5 : scaled + 0.5 );
	if ( rounded <= -( INT64_C( 1 ) << 29 ) || rounded >= ( INT64_C( 1 ) << 29 ) ||
		 rounded < INT32_MIN || rounded > INT32_MAX ) return qfalse;
	*outValue = (int32_t)rounded;
	return qtrue;
}

static uint64_t HashBytes( uint64_t digest, const void *data, size_t size )
{
	const byte *bytes = (const byte *)data;
	size_t		i;
	for ( i = 0u; i < size; ++i ) {
		digest ^= bytes[i];
		digest *= FNV_PRIME;
	}
	return digest;
}

static uint64_t HashU32( uint64_t digest, uint32_t value )
{
	return HashBytes( digest, &value, sizeof( value ) );
}

static uint64_t MaterialSnapshotDigest( const char *name, const renderMaterialSnapshot_t *snapshot )
{
	uint64_t digest = HashBytes( FNV_OFFSET, name, strlen( name ) + 1u );
	digest			= HashBytes( digest, &snapshot->textureCacheKeyReady,
							 sizeof( snapshot->textureCacheKeyReady ) );
	if ( snapshot->textureCacheKeyReady ) {
		digest = HashBytes( digest, &snapshot->textureCacheKey,
			sizeof( snapshot->textureCacheKey ) );
		digest = HashBytes( digest, &snapshot->textureCacheKeyHash,
			sizeof( snapshot->textureCacheKeyHash ) );
	}
	digest			= HashBytes( digest, &snapshot->width, sizeof( snapshot->width ) );
	digest			= HashBytes( digest, &snapshot->height, sizeof( snapshot->height ) );
	digest			= HashBytes( digest, &snapshot->clampToEdge, sizeof( snapshot->clampToEdge ) );
	digest			= HashBytes( digest, &snapshot->msdf, sizeof( snapshot->msdf ) );
	digest			= HashBytes( digest, &snapshot->srgb, sizeof( snapshot->srgb ) );
	digest			= HashBytes( digest, &snapshot->sky, sizeof( snapshot->sky ) );
	digest			= HashBytes( digest, &snapshot->skyBox, sizeof( snapshot->skyBox ) );
	digest = HashBytes( digest, &snapshot->authoredSkySourceReady,
		sizeof( snapshot->authoredSkySourceReady ) );
	if ( snapshot->authoredSkySourceReady ) {
		const ralMaterialSourceSky_t *sky = &snapshot->authoredSkySource;
		digest = HashU32( digest, (uint32_t)sky->outerBox );
		digest = HashU32( digest, (uint32_t)sky->innerBox );
		for ( uint32_t face = 0u; face < RAL_MATERIAL_SKY_FACE_COUNT; ++face ) {
			digest = HashU32( digest, sky->outerDependencies[face] );
			digest = HashU32( digest, sky->innerDependencies[face] );
		}
		digest = HashU32( digest, (uint32_t)sky->hasSun );
		digest = HashU32( digest, (uint32_t)sky->sunExtended );
		digest = HashBytes( digest, sky->sunColor, sizeof( sky->sunColor ) );
		digest = HashBytes( digest, &sky->sunIntensity,
			sizeof( sky->sunIntensity ) );
		digest = HashBytes( digest, &sky->sunAzimuthDegrees,
			sizeof( sky->sunAzimuthDegrees ) );
		digest = HashBytes( digest, &sky->sunElevationDegrees,
			sizeof( sky->sunElevationDegrees ) );
		digest = HashBytes( digest, &sky->sunDevianceDegrees,
			sizeof( sky->sunDevianceDegrees ) );
		digest = HashU32( digest, sky->sunSamples );
	}
	digest = HashBytes( digest, &snapshot->authoredSkyTexturesReady,
		sizeof( snapshot->authoredSkyTexturesReady ) );
	if ( snapshot->authoredSkyTexturesReady ) {
		digest = HashBytes( digest, snapshot->authoredSkyOuterTextures,
			sizeof( snapshot->authoredSkyOuterTextures ) );
		digest = HashBytes( digest, snapshot->authoredSkyInnerTextures,
			sizeof( snapshot->authoredSkyInnerTextures ) );
		digest = HashBytes( digest, snapshot->authoredSkyAverage,
			sizeof( snapshot->authoredSkyAverage ) );
	}
	digest			= HashBytes( digest, &snapshot->noDraw, sizeof( snapshot->noDraw ) );
	digest			= HashBytes( digest, &snapshot->skyCloudHeight,
						 sizeof( snapshot->skyCloudHeight ) );
	digest			= HashBytes( digest, snapshot->skyScaleScroll,
						 sizeof( snapshot->skyScaleScroll ) );
	digest			= HashBytes( digest, &snapshot->skySecondaryMaterial,
						 sizeof( snapshot->skySecondaryMaterial ) );
	digest			= HashBytes( digest, &snapshot->skySecondaryAlphaMode,
						 sizeof( snapshot->skySecondaryAlphaMode ) );
	digest			= HashBytes( digest, snapshot->skySecondaryScaleScroll,
						 sizeof( snapshot->skySecondaryScaleScroll ) );
	digest			= HashBytes( digest, &snapshot->alphaMode, sizeof( snapshot->alphaMode ) );
	digest			= HashBytes( digest, &snapshot->cullMode, sizeof( snapshot->cullMode ) );
	digest			= HashBytes( digest, &snapshot->alphaCutoff, sizeof( snapshot->alphaCutoff ) );
	digest			= HashBytes( digest, &snapshot->depthWrite, sizeof( snapshot->depthWrite ) );
	digest			= HashBytes( digest, &snapshot->sort, sizeof( snapshot->sort ) );
	digest			= HashBytes( digest, &snapshot->stageCount,
						 sizeof( snapshot->stageCount ) );
	digest			= HashBytes( digest, snapshot->stages,
						 (size_t)snapshot->stageCount * sizeof( snapshot->stages[0] ) );
	digest = HashBytes( digest, &snapshot->factors.schemaVersion,
		sizeof( snapshot->factors.schemaVersion ) );
	digest = HashBytes( digest, &snapshot->factors.runtimeGeneration,
		sizeof( snapshot->factors.runtimeGeneration ) );
	digest = HashBytes( digest, &snapshot->factors.ready,
		sizeof( snapshot->factors.ready ) );
	if ( snapshot->factors.ready )
		digest = HashBytes( digest, &snapshot->factors.factors,
			sizeof( snapshot->factors.factors ) );
	digest			= HashBytes( digest, &snapshot->lighting.schemaVersion,
							 sizeof( snapshot->lighting.schemaVersion ) );
	digest			= HashBytes( digest, &snapshot->lighting.sourceGeneration,
							 sizeof( snapshot->lighting.sourceGeneration ) );
	digest			= HashBytes( digest, &snapshot->lighting.provenanceHash,
							 sizeof( snapshot->lighting.provenanceHash ) );
	digest			= HashBytes( digest, snapshot->lighting.diffuseReflectanceQ16,
							 sizeof( snapshot->lighting.diffuseReflectanceQ16 ) );
	digest			= HashBytes( digest, snapshot->lighting.emissionRadianceQ16,
							 sizeof( snapshot->lighting.emissionRadianceQ16 ) );
	digest			= HashBytes( digest, &snapshot->lighting.emissiveMobility,
							 sizeof( snapshot->lighting.emissiveMobility ) );
	digest			= HashBytes( digest, &snapshot->lighting.emissiveInfluenceRangeQ16,
							 sizeof( snapshot->lighting.emissiveInfluenceRangeQ16 ) );
	digest			= HashBytes( digest, &snapshot->lighting.emissiveShadowPriority,
							 sizeof( snapshot->lighting.emissiveShadowPriority ) );
	digest			= HashBytes( digest, &snapshot->lighting.emissiveRequestedProxyCount,
							 sizeof( snapshot->lighting.emissiveRequestedProxyCount ) );
	digest			= HashBytes( digest, &snapshot->lighting.participatesInStaticBake,
							 sizeof( snapshot->lighting.participatesInStaticBake ) );
	digest			= HashBytes( digest, &snapshot->lighting.emissiveExplicitProxyAuthority,
							 sizeof( snapshot->lighting.emissiveExplicitProxyAuthority ) );
	digest			= HashBytes( digest, &snapshot->lighting.emissiveInjectsAtmosphere,
							 sizeof( snapshot->lighting.emissiveInjectsAtmosphere ) );
	digest			= HashBytes( digest, &snapshot->lighting.ready, sizeof( snapshot->lighting.ready ) );
	digest			= HashBytes( digest, &snapshot->ready, sizeof( snapshot->ready ) );
	if ( snapshot->rgba8 && snapshot->byteCount )
		digest = HashBytes( digest, snapshot->rgba8, snapshot->byteCount );
	return digest;
}

static uint64_t TextureSnapshotDigest( const char *name,
		const renderTextureSnapshot_t *snapshot ) {
	uint64_t digest = HashBytes( FNV_OFFSET, name, strlen( name ) + 1u );
	digest = HashBytes( digest, &snapshot->textureCacheKeyReady,
		sizeof( snapshot->textureCacheKeyReady ) );
	if ( snapshot->textureCacheKeyReady ) {
		digest = HashBytes( digest, &snapshot->textureCacheKey,
			sizeof( snapshot->textureCacheKey ) );
		digest = HashBytes( digest, &snapshot->textureCacheKeyHash,
			sizeof( snapshot->textureCacheKeyHash ) );
	}
	digest = HashBytes( digest, &snapshot->width, sizeof( snapshot->width ) );
	digest = HashBytes( digest, &snapshot->height, sizeof( snapshot->height ) );
	digest = HashBytes( digest, &snapshot->clampToEdge,
		sizeof( snapshot->clampToEdge ) );
	digest = HashBytes( digest, &snapshot->msdf, sizeof( snapshot->msdf ) );
	digest = HashBytes( digest, &snapshot->srgb, sizeof( snapshot->srgb ) );
	digest = HashBytes( digest, &snapshot->ready, sizeof( snapshot->ready ) );
	if ( snapshot->rgba8 && snapshot->byteCount )
		digest = HashBytes( digest, snapshot->rgba8, snapshot->byteCount );
	return digest;
}

qboolean RenderSubmission_SetMaterialTextureCacheKey(
		renderSubmissionState_t *state, qhandle_t handle,
		const ralTextureCacheKey_t *textureCacheKey ) {
	if ( !state || !state->initialized || handle <= 0
			|| !Ral_TextureCacheKeyValid( textureCacheKey )
			|| state->nextMaterialGeneration == UINT64_MAX ) return qfalse;
	for ( uint32_t i = 0u; i < state->materialCount; ++i ) {
		renderMaterialSnapshot_t *snapshot = &state->materials[i].snapshot;
		if ( snapshot->handle != handle ) continue;
		if ( snapshot->ready != qtrue ) return qfalse;
		if ( snapshot->textureCacheKeyReady
				&& Ral_TextureCacheKeyExact( &snapshot->textureCacheKey,
					textureCacheKey ) )
			return qtrue;
		/* Texture identity is immutable after first publication. Metal/OpenGL/
		 * WebGPU publish the decoded payload and therefore matching dimensions;
		 * the established Vulkan adapter publishes a bounded 1x1 CPU shadow for
		 * an already-resident native image, whose authoritative dimensions live
		 * in this first cache key. Never let a later re-registration replace it. */
		if ( snapshot->textureCacheKeyReady ) return qfalse;
		if ( ( textureCacheKey->asset.width != snapshot->width
				|| textureCacheKey->asset.height != snapshot->height )
				&& ( snapshot->width != 1u || snapshot->height != 1u
					|| snapshot->byteCount != 4u ) ) return qfalse;
		snapshot->textureCacheKey = *textureCacheKey;
		snapshot->textureCacheKeyHash = textureCacheKey->keyHash;
		snapshot->textureCacheKeyReady = qtrue;
		snapshot->generation = state->nextMaterialGeneration++;
		snapshot->digest = MaterialSnapshotDigest( state->materials[i].name,
			snapshot );
		RebuildMaterialDigest( state );
		return qtrue;
	}
	return qfalse;
}

qboolean RenderSubmission_SetTextureCacheKey( renderSubmissionState_t *state,
		qhandle_t handle, const ralTextureCacheKey_t *textureCacheKey ) {
	if ( !state || !state->initialized || handle <= 0
			|| !Ral_TextureCacheKeyValid( textureCacheKey )
			|| state->nextMaterialGeneration == UINT64_MAX ) return qfalse;
	if ( RenderSubmission_SetMaterialTextureCacheKey( state, handle,
			textureCacheKey ) ) return qtrue;
	for ( uint32_t i = 0u; i < state->stageTextureCount; ++i ) {
		renderTextureSnapshot_t *snapshot = &state->stageTextures[i].snapshot;
		if ( snapshot->handle != handle ) continue;
		if ( snapshot->ready != qtrue
				|| textureCacheKey->asset.width != snapshot->width
				|| textureCacheKey->asset.height != snapshot->height ) return qfalse;
		if ( snapshot->textureCacheKeyReady
				&& Ral_TextureCacheKeyExact( &snapshot->textureCacheKey,
					textureCacheKey ) ) return qtrue;
		snapshot->textureCacheKey = *textureCacheKey;
		snapshot->textureCacheKeyHash = textureCacheKey->keyHash;
		snapshot->textureCacheKeyReady = qtrue;
		snapshot->generation = state->nextMaterialGeneration++;
		snapshot->digest = TextureSnapshotDigest( state->stageTextures[i].name,
			snapshot );
		RebuildMaterialDigest( state );
		return qtrue;
	}
	return qfalse;
}

static void RebuildMaterialDigest( renderSubmissionState_t *state )
{
	uint32_t i;
	state->materialDigest = FNV_OFFSET;
	state->modelDigest	  = FNV_OFFSET;
	for ( i = 0u; i < state->materialCount; ++i ) {
		const renderMaterialSnapshot_t *snapshot = &state->materials[i].snapshot;
		state->materialDigest = HashBytes( state->materialDigest, &snapshot->handle, sizeof( snapshot->handle ) );
		state->materialDigest =
			HashBytes( state->materialDigest, &snapshot->generation, sizeof( snapshot->generation ) );
		state->materialDigest = HashBytes( state->materialDigest, &snapshot->digest, sizeof( snapshot->digest ) );
	}
	for ( i = 0u; i < state->stageTextureCount; ++i ) {
		const renderTextureSnapshot_t *snapshot =
			&state->stageTextures[i].snapshot;
		state->materialDigest = HashBytes( state->materialDigest,
			&snapshot->handle, sizeof( snapshot->handle ) );
		state->materialDigest = HashBytes( state->materialDigest,
			&snapshot->generation, sizeof( snapshot->generation ) );
		state->materialDigest = HashBytes( state->materialDigest,
			&snapshot->digest, sizeof( snapshot->digest ) );
	}
}

static qboolean KindValid( renderAssetKind_t kind )
{
	return kind >= RENDER_ASSET_MODEL && kind <= RENDER_ASSET_LIGHTMAP;
}

static qboolean MaterialKind( renderAssetKind_t kind )
{
	return ( kind == RENDER_ASSET_MATERIAL || kind == RENDER_ASSET_MSDF || kind == RENDER_ASSET_PRIMITIVE_MATERIAL ||
			 kind == RENDER_ASSET_LIGHTMAP )
			   ? qtrue
			   : qfalse;
}

static qboolean AddCount( uint32_t *count, uint32_t amount )
{
	if ( !count || amount > UINT32_MAX - *count )
		return qfalse;
	*count += amount;
	return qtrue;
}

static int WorldBatchSortCompare( const void *leftValue,
		const void *rightValue ) {
	const renderWorldBatch_t *left = (const renderWorldBatch_t *)leftValue;
	const renderWorldBatch_t *right = (const renderWorldBatch_t *)rightValue;
	if ( left->sort < right->sort ) return -1;
	if ( left->sort > right->sort ) return 1;
	/* Established Q3 draw-surface sorting groups equal-sort surfaces by the
	 * registered shader before retaining source order.  Material handles are
	 * the frontend's stable registered-shader identity. */
	if ( left->baseMaterial < right->baseMaterial ) return -1;
	if ( left->baseMaterial > right->baseMaterial ) return 1;
	if ( left->sourceSurfaceIndex < right->sourceSurfaceIndex ) return -1;
	if ( left->sourceSurfaceIndex > right->sourceSurfaceIndex ) return 1;
	return 0;
}

static qboolean WorldSurfaceBounds( const mapFile_t *bsp,
		const dsurface_t *surface, renderWorldBatch_t *batch ) {
	float mins[3], maxs[3], radiusSquared = 0.0f;
	if ( !bsp || !surface || !batch || surface->numVerts < 0
			|| surface->firstVert < 0
			|| surface->firstVert > bsp->numDrawVerts - surface->numVerts )
		return qfalse;
	if ( !surface->numVerts ) {
		memset( batch->boundsCenter, 0, sizeof( batch->boundsCenter ) );
		batch->boundsRadius = 0.0f;
		return qtrue;
	}
	for ( uint32_t axis = 0u; axis < 3u; ++axis ) {
		mins[axis] = maxs[axis] = bsp->drawVerts[surface->firstVert].xyz[axis];
		if ( !isfinite( mins[axis] ) ) return qfalse;
	}
	for ( int vertex = 1; vertex < surface->numVerts; ++vertex ) {
		const vec3_t point = {
			bsp->drawVerts[surface->firstVert + vertex].xyz[0],
			bsp->drawVerts[surface->firstVert + vertex].xyz[1],
			bsp->drawVerts[surface->firstVert + vertex].xyz[2]
		};
		for ( uint32_t axis = 0u; axis < 3u; ++axis ) {
			if ( !isfinite( point[axis] ) ) return qfalse;
			if ( point[axis] < mins[axis] ) mins[axis] = point[axis];
			if ( point[axis] > maxs[axis] ) maxs[axis] = point[axis];
		}
	}
	for ( uint32_t axis = 0u; axis < 3u; ++axis )
		batch->boundsCenter[axis] = 0.5f * ( mins[axis] + maxs[axis] );
	for ( int vertex = 0; vertex < surface->numVerts; ++vertex ) {
		float distanceSquared = 0.0f;
		for ( uint32_t axis = 0u; axis < 3u; ++axis ) {
			const float delta = bsp->drawVerts[surface->firstVert + vertex].xyz[axis]
				- batch->boundsCenter[axis];
			distanceSquared += delta * delta;
		}
		if ( distanceSquared > radiusSquared ) radiusSquared = distanceSquared;
	}
	batch->boundsRadius = sqrtf( radiusSquared );
	return isfinite( batch->boundsRadius ) ? qtrue : qfalse;
}

static void DefaultMaterialRasterPolicy( renderAssetKind_t kind, const byte *rgba8, uint32_t byteCount,
										 renderMaterialSnapshot_t *snapshot )
{
	qboolean hasTransparent = qfalse;
	qboolean hasOpaque = qfalse;
	qboolean hasPartialAlpha = qfalse;
	(void)kind;
	snapshot->alphaMode	  = RENDER_ALPHA_OPAQUE;
	snapshot->cullMode = RENDER_CULL_BACK;
	snapshot->alphaCutoff = 0.5f;
	snapshot->depthWrite  = qtrue;
	snapshot->sort = RENDER_MATERIAL_SORT_OPAQUE;
	snapshot->skyCloudHeight = 512.0f;
	snapshot->skyScaleScroll[0] = 1.0f;
	snapshot->skyScaleScroll[1] = 1.0f;
	/* Scriptless Q3 assets can carry their cutout contract in image alpha.
	 * Preserve mixed binary alpha as a depth-writing mask and reserve blending
	 * for fractional alpha.  Some legacy conversions (including skull_door_*)
	 * carry useful opaque RGB with an entirely-zero alpha plane; that sentinel
	 * remains opaque, matching the established renderer. */
	if ( rgba8 && byteCount >= 4u ) {
		for ( uint32_t offset = 3u; offset < byteCount; offset += 4u ) {
			const byte alpha = rgba8[offset];
			if ( alpha == 255u ) { hasOpaque = qtrue; continue; }
			hasTransparent = qtrue;
			if ( alpha != 0u ) { hasPartialAlpha = qtrue; break; }
		}
	}
	if ( hasPartialAlpha ) {
		snapshot->alphaMode = RENDER_ALPHA_BLEND;
		snapshot->depthWrite = qfalse;
		snapshot->sort = RENDER_MATERIAL_SORT_BLEND;
	} else if ( hasTransparent && hasOpaque ) {
		snapshot->alphaMode = RENDER_ALPHA_MASK;
		snapshot->sort = RENDER_MATERIAL_SORT_SEE_THROUGH;
	}
}

static qboolean ReceiptValid( const renderSubmissionReceipt_t *receipt )
{
	return ( receipt && receipt->schemaVersion == RENDER_SUBMISSION_SCHEMA_VERSION && receipt->ownerGeneration != 0u &&
			 receipt->ownerGeneration != UINT64_MAX && receipt->frameGeneration != 0u &&
			 receipt->frameGeneration != UINT64_MAX && receipt->materialDigest != 0u && receipt->sceneDigest != 0u &&
			 receipt->uiDigest != 0u && receipt->atmosphereDigest != 0u && receipt->atmosphereGeneration != 0u &&
			 receipt->atmosphereGeneration != UINT64_MAX && receipt->atmosphereProfileDigest != 0u &&
			 receipt->atmosphereProfileGeneration != 0u && receipt->atmosphereProfileGeneration != UINT64_MAX &&
			 receipt->particleClassDigest != 0u && receipt->particleClassGeneration != 0u &&
			 receipt->particleClassGeneration != UINT64_MAX && receipt->surfaceClimateDigest != 0u &&
			 receipt->surfaceClimateGeneration != 0u && receipt->surfaceClimateGeneration != UINT64_MAX &&
			 receipt->atmosphereMediaDigest != 0u && receipt->irradianceVolumeDigest != 0u &&
			 receipt->directionalLightingDigest != 0u &&
			 receipt->effectPrimitiveDigest != 0u &&
			 receipt->frameDigest != 0u &&
			 ( receipt->worldLoaded == qfalse || receipt->worldDigest != 0u ) &&
			 ( receipt->atmosphereActive == qfalse || receipt->atmosphereActive == qtrue ) && receipt->ready == qtrue )
			   ? qtrue
			   : qfalse;
}

qboolean RenderSubmission_Init( renderSubmissionState_t *state, uint64_t ownerGeneration )
{
	if ( !state || ownerGeneration == 0u || ownerGeneration == UINT64_MAX )
		return qfalse;
	memset( state, 0, sizeof( *state ) );
	state->irradianceVolumes = state->fallbackIrradianceVolumes;
	state->directionalLighting = &state->fallbackDirectionalLighting;
	state->activeWorldIndex       = -1;
	state->pbrEnabled = qtrue;
	state->ownerGeneration		  = ownerGeneration;
	state->assetDigest			  = FNV_OFFSET;
	state->materialDigest		  = FNV_OFFSET;
	state->irradianceVolumeDigest = FNV_OFFSET;
	state->directionalLightingDigest = FNV_OFFSET;
	state->sceneDigest			  = FNV_OFFSET;
	state->effectPrimitiveDigest = FNV_OFFSET;
	state->lensSourceDigest       = FNV_OFFSET;
	state->uiDigest				  = FNV_OFFSET;
	state->nextHandle			  = 1u;
	state->nextMaterialGeneration = 1u;
	state->nextModelGeneration	  = 1u;
	state->color[0] = state->color[1] = state->color[2] = state->color[3] = 1.0f;
	state->initialized													  = qtrue;
	if ( !RenderSubmission_InitAtmosphere( state ) ) {
		memset( state, 0, sizeof( *state ) );
		return qfalse;
	}
	state->uiPrimitives = (renderUiPrimitive_t *)calloc(
		RENDER_SUBMISSION_MAX_UI_PRIMITIVES, sizeof( *state->uiPrimitives ) );
	state->effectSprites = (spriteDesc_t *)calloc(
		RENDER_SUBMISSION_MAX_EFFECT_SPRITES, sizeof( *state->effectSprites ) );
	state->effectEmitters = (emitterDesc_t *)calloc(
		RENDER_SUBMISSION_MAX_EFFECT_EMITTERS, sizeof( *state->effectEmitters ) );
	state->effectDecals = (decalDesc_t *)calloc(
		RENDER_SUBMISSION_MAX_EFFECT_DECALS, sizeof( *state->effectDecals ) );
	state->effectRibbons = (renderEffectRibbonCommand_t *)calloc(
		RENDER_SUBMISSION_MAX_EFFECT_RIBBONS, sizeof( *state->effectRibbons ) );
	state->effectRibbonPoints = (ribbonPoint_t *)calloc(
		RENDER_SUBMISSION_MAX_EFFECT_RIBBON_POINTS,
		sizeof( *state->effectRibbonPoints ) );
	state->effectBeams = (beamDesc_t *)calloc(
		RENDER_SUBMISSION_MAX_EFFECT_BEAMS, sizeof( *state->effectBeams ) );
	if ( !state->uiPrimitives || !state->effectSprites || !state->effectEmitters
			|| !state->effectDecals || !state->effectRibbons
			|| !state->effectRibbonPoints || !state->effectBeams ) {
		free( state->uiPrimitives ); free( state->effectSprites );
		free( state->effectEmitters ); free( state->effectDecals );
		free( state->effectRibbons ); free( state->effectRibbonPoints );
		free( state->effectBeams );
		memset( state, 0, sizeof( *state ) );
		return qfalse;
	}
	return qtrue;
}

static void RenderSubmission_FreeWorldSlot( renderSubmissionWorldSlot_t *slot )
{
	uint32_t index;
	if ( !slot ) return;
	free( (void *)slot->snapshot.vertices );
	free( (void *)slot->snapshot.indices );
	free( (void *)slot->snapshot.batches );
	free( slot->surfaceVisibility );
	free( (void *)slot->heightgrid.samples );
	for ( index = 0u; index < slot->irradianceVolumeCount; ++index )
		free( slot->irradianceVolumes[index].ownedArtifactBytes );
	free( slot->directionalLighting.ownedArtifactBytes );
	memset( slot, 0, sizeof( *slot ) );
}

static void RenderSubmission_ClearActiveWorld( renderSubmissionState_t *state )
{
	state->activeWorldIndex = -1;
	state->worldMap = NULL;
	state->worldSurfaceVisibility = NULL;
	state->worldSurfaceVisibilityBytes = 0u;
	memset( &state->worldSnapshot, 0, sizeof( state->worldSnapshot ) );
	state->worldDigest = 0u;
	state->worldSurfaceCount = 0u;
	state->worldVertexCount = 0u;
	state->worldIndexCount = 0u;
	state->worldLoaded = qfalse;
	state->irradianceVolumes = state->fallbackIrradianceVolumes;
	state->directionalLighting = &state->fallbackDirectionalLighting;
	state->irradianceVolumeCount = state->directionalLightingCount = 0u;
	state->irradianceVolumeDigest = state->directionalLightingDigest = FNV_OFFSET;
}

static void RenderSubmission_CommitActiveWorld( renderSubmissionState_t *state )
{
	renderSubmissionWorldSlot_t *slot;
	if ( !state || state->activeWorldIndex < 0
			|| state->activeWorldIndex >= MAX_RENDER_WORLDS ) return;
	slot = &state->worlds[state->activeWorldIndex];
	if ( !slot->loaded ) return;
	slot->snapshot = state->worldSnapshot;
	slot->digest = state->worldDigest;
	slot->surfaceCount = state->worldSurfaceCount;
	slot->vertexCount = state->worldVertexCount;
	slot->indexCount = state->worldIndexCount;
	slot->irradianceVolumeDigest = state->irradianceVolumeDigest;
	slot->directionalLightingDigest = state->directionalLightingDigest;
	slot->irradianceVolumeCount = state->irradianceVolumeCount;
	slot->directionalLightingCount = state->directionalLightingCount;
}

qboolean RenderSubmission_SelectWorld( renderSubmissionState_t *state, int worldIndex )
{
	renderSubmissionWorldSlot_t *slot;
	if ( !state || !state->initialized || worldIndex < 0
			|| worldIndex >= MAX_RENDER_WORLDS ) return qfalse;
	slot = &state->worlds[worldIndex];
	if ( !slot->loaded || !slot->map || !slot->snapshot.ready ) return qfalse;
	RenderSubmission_CommitActiveWorld( state );
	state->activeWorldIndex = worldIndex;
	state->worldMap = slot->map;
	state->worldSurfaceVisibility = slot->surfaceVisibility;
	state->worldSurfaceVisibilityBytes = slot->surfaceVisibilityBytes;
	state->worldSnapshot = slot->snapshot;
	state->worldDigest = slot->digest;
	state->worldSurfaceCount = slot->surfaceCount;
	state->worldVertexCount = slot->vertexCount;
	state->worldIndexCount = slot->indexCount;
	state->irradianceVolumes = slot->irradianceVolumes;
	state->directionalLighting = &slot->directionalLighting;
	state->irradianceVolumeDigest = slot->irradianceVolumeDigest;
	state->directionalLightingDigest = slot->directionalLightingDigest;
	state->irradianceVolumeCount = slot->irradianceVolumeCount;
	state->directionalLightingCount = slot->directionalLightingCount;
	state->worldLoaded = qtrue;
	return qtrue;
}

qboolean RenderSubmission_WorldResident( const renderSubmissionState_t *state,
		int worldIndex )
{
	return state && state->initialized && worldIndex >= 0
		&& worldIndex < MAX_RENDER_WORLDS
		&& state->worlds[worldIndex].loaded ? qtrue : qfalse;
}

int RenderSubmission_ResidentWorldCount( const renderSubmissionState_t *state )
{
	int count = 0;
	if ( !state || !state->initialized ) return 0;
	for ( int worldIndex = 0; worldIndex < MAX_RENDER_WORLDS; ++worldIndex )
		if ( state->worlds[worldIndex].loaded ) count++;
	return count;
}

qboolean RenderSubmission_UnloadWorld( renderSubmissionState_t *state, int worldIndex )
{
	int replacement;
	if ( !state || !state->initialized || state->frameOpen || worldIndex < 0
			|| worldIndex >= MAX_RENDER_WORLDS ) return qfalse;
	if ( !state->worlds[worldIndex].loaded ) return qtrue;
	if ( state->activeWorldIndex == worldIndex )
		RenderSubmission_ClearActiveWorld( state );
	RenderSubmission_ReleaseWorldModels( state, worldIndex );
	RenderSubmission_FreeWorldSlot( &state->worlds[worldIndex] );
	if ( state->activeWorldIndex < 0 ) {
		for ( replacement = 0; replacement < MAX_RENDER_WORLDS; ++replacement )
			if ( state->worlds[replacement].loaded ) {
				(void)RenderSubmission_SelectWorld( state, replacement );
				break;
			}
	}
	return qtrue;
}

static void FreeRegistrationAssets( renderSubmissionState_t *state )
{
	for ( uint32_t i = 0u; i < state->materialCount; ++i )
		free( (void *)state->materials[i].snapshot.rgba8 );
	for ( uint32_t i = 0u; i < state->stageTextureCount; ++i )
		free( (void *)state->stageTextures[i].snapshot.rgba8 );
	for ( uint32_t i = 0u; i < state->modelCount; ++i ) {
		free( (void *)state->models[i].snapshot.positions );
		free( (void *)state->models[i].snapshot.normals );
		free( (void *)state->models[i].snapshot.texCoords );
		free( (void *)state->models[i].snapshot.colors );
		free( (void *)state->models[i].snapshot.indices );
		free( (void *)state->models[i].snapshot.batches );
		free( (void *)state->models[i].snapshot.tags );
	}
}

qboolean RenderSubmission_ResetRegistrationAssets( renderSubmissionState_t *state )
{
	if ( !state || !state->initialized || state->frameOpen
			|| RenderSubmission_ResidentWorldCount( state ) ) return qfalse;
	RenderSubmission_CancelFrame( state );
	FreeRegistrationAssets( state );
	memset( state->materials, 0, sizeof( state->materials ) );
	memset( state->stageTextures, 0, sizeof( state->stageTextures ) );
	memset( state->models, 0, sizeof( state->models ) );
	state->materialCount = state->materialBytes = state->resolvedMaterialCount = 0u;
	state->stageTextureCount = state->stageTextureBytes = 0u;
	state->modelCount = state->modelBytes = 0u;
	state->registeredAssetCount = state->registeredMaterialCount = 0u;
	state->assetDigest = state->materialDigest = state->modelDigest = FNV_OFFSET;
	return qtrue;
}

void RenderSubmission_Reset( renderSubmissionState_t *state )
{
	/* A renderer module may fail before frontend ownership is initialized (for
	 * example WebGPU without a browser runtime). Shutdown must remain idempotent
	 * and must not dereference the zero-state pointer aliases. */
	if ( !state ) return;
	if ( !state->initialized ) {
		memset( state, 0, sizeof( *state ) );
		return;
	}
	if ( state ) {
		uint32_t i;
		state->frameOpen = qfalse;
		(void)RenderSubmission_ClearIrradianceVolumes( state );
		(void)RenderSubmission_ClearDirectionalLighting( state );
		FreeRegistrationAssets( state );
		for ( i = 0u; i < MAX_RENDER_WORLDS; ++i )
			RenderSubmission_FreeWorldSlot( &state->worlds[i] );
		free( state->polyCommands );
		free( state->polyVertices );
		free( state->lights );
		free( state->uiPrimitives );
		free( state->effectSprites );
		free( state->effectEmitters );
		free( state->effectDecals );
		free( state->effectRibbons );
		free( state->effectRibbonPoints );
		free( state->effectBeams );
		memset( state, 0, sizeof( *state ) );
	}
}

qboolean RenderSubmission_ReadableFrameGeneration(
		const renderSubmissionState_t *state, uint64_t *outFrameGeneration ) {
	uint64_t generation;
	if ( !state || !outFrameGeneration || !state->initialized
			|| ( state->frameOpen == state->frameSealed ) ) return qfalse;
	generation = state->frameOpen ? state->currentFrameGeneration
		: state->sealedFrameGeneration;
	if ( !generation || generation == UINT64_MAX ) return qfalse;
	*outFrameGeneration = generation;
	return qtrue;
}

qhandle_t RenderSubmission_RegisterAsset( renderSubmissionState_t *state, renderAssetKind_t kind, const char *name )
{
	if ( MaterialKind( kind ) )
		return RenderSubmission_RegisterMaterialImage( state, kind, name, qfalse, NULL, 0u, 0u );
	if ( !state || !state->initialized || !KindValid( kind ) || !name || !name[0] || state->nextHandle >= INT_MAX )
		return 0;
	if ( !RenderSubmission_RecordAsset( state, kind, name, (qhandle_t)state->nextHandle ) )
		return 0;
	return (qhandle_t)state->nextHandle++;
}

qhandle_t RenderSubmission_RegisterMaterialImage( renderSubmissionState_t *state, renderAssetKind_t kind,
												  const char *name, qboolean clampToEdge, const byte *rgba8,
												  uint32_t width, uint32_t height )
{
	renderMaterialRecord_t *record;
	byte				   *owned = NULL;
	uint64_t				byteCount64;
	uint32_t				byteCount = 0u, i;
	qhandle_t				handle;
	if ( !state || !state->initialized || !MaterialKind( kind ) || !name || !name[0] || strlen( name ) >= MAX_QPATH ||
		 ( clampToEdge != qfalse && clampToEdge != qtrue ) )
		return 0;
	for ( i = 0u; i < state->materialCount; ++i ) {
		if ( !strcasecmp( state->materials[i].name, name ) ) {
			renderMaterialSnapshot_t *snapshot = &state->materials[i].snapshot;
			if ( !rgba8 || snapshot->ready )
				return snapshot->handle;
			if ( !width || !height || width > RENDER_SUBMISSION_MAX_IMAGE_DIMENSION ||
				 height > RENDER_SUBMISSION_MAX_IMAGE_DIMENSION || state->nextMaterialGeneration == UINT64_MAX )
				return 0;
			byteCount64 = (uint64_t)width * (uint64_t)height * 4u;
			if ( byteCount64 > UINT32_MAX || byteCount64 > RENDER_SUBMISSION_MAX_MATERIAL_BYTES - state->materialBytes )
				return 0;
			byteCount = (uint32_t)byteCount64;
			owned	  = (byte *)malloc( byteCount );
			if ( !owned )
				return 0;
			memcpy( owned, rgba8, byteCount );
			snapshot->generation  = state->nextMaterialGeneration++;
			snapshot->width		  = width;
			snapshot->height	  = height;
			snapshot->rowBytes	  = width * 4u;
			snapshot->byteCount	  = byteCount;
			snapshot->rgba8		  = owned;
			snapshot->clampToEdge = clampToEdge;
			snapshot->msdf		  = kind == RENDER_ASSET_MSDF ? qtrue : qfalse;
			/* Legacy lightmaps are authored colour data. Decode them to linear
			 * before applying r_lightmapBoost on every RAL backend. */
			snapshot->srgb		  = snapshot->msdf ? qfalse : qtrue;
			DefaultMaterialRasterPolicy( kind, owned, byteCount, snapshot );
			snapshot->ready	 = qtrue;
			snapshot->digest = MaterialSnapshotDigest( state->materials[i].name, snapshot );
			state->resolvedMaterialCount++;
			state->materialBytes += byteCount;
			RebuildMaterialDigest( state );
			return snapshot->handle;
		}
	}
	if ( state->materialCount >= RENDER_SUBMISSION_MAX_MATERIALS || state->nextHandle >= INT_MAX ||
		 state->nextMaterialGeneration == UINT64_MAX )
		return 0;
	if ( rgba8 ) {
		if ( !width || !height || width > RENDER_SUBMISSION_MAX_IMAGE_DIMENSION ||
			 height > RENDER_SUBMISSION_MAX_IMAGE_DIMENSION )
			return 0;
		byteCount64 = (uint64_t)width * (uint64_t)height * 4u;
		if ( byteCount64 > UINT32_MAX || byteCount64 > RENDER_SUBMISSION_MAX_MATERIAL_BYTES - state->materialBytes )
			return 0;
		byteCount = (uint32_t)byteCount64;
		owned	  = (byte *)malloc( byteCount );
		if ( !owned )
			return 0;
		memcpy( owned, rgba8, byteCount );
	} else if ( width || height ) {
		return 0;
	}
	handle = (qhandle_t)state->nextHandle;
	if ( !RenderSubmission_RecordAsset( state, kind, name, handle ) ) {
		free( owned );
		return 0;
	}
	record = &state->materials[state->materialCount++];
	memset( record, 0, sizeof( *record ) );
	(void)snprintf( record->name, sizeof( record->name ), "%s", name );
	record->snapshot.handle		 = handle;
	record->snapshot.generation	 = state->nextMaterialGeneration++;
	record->snapshot.width		 = width;
	record->snapshot.height		 = height;
	record->snapshot.rowBytes	 = width * 4u;
	record->snapshot.byteCount	 = byteCount;
	record->snapshot.rgba8		 = owned;
	record->snapshot.clampToEdge = clampToEdge;
	record->snapshot.msdf		 = kind == RENDER_ASSET_MSDF ? qtrue : qfalse;
	record->snapshot.srgb		 = record->snapshot.msdf ? qfalse : qtrue;
	DefaultMaterialRasterPolicy( kind, owned, byteCount, &record->snapshot );
	record->snapshot.ready	= owned ? qtrue : qfalse;
	record->snapshot.digest = MaterialSnapshotDigest( record->name, &record->snapshot );
	if ( owned ) {
		state->resolvedMaterialCount++;
		state->materialBytes += byteCount;
	}
	RebuildMaterialDigest( state );
	state->nextHandle++;
	return handle;
}

qhandle_t RenderSubmission_RegisterStageTexture( renderSubmissionState_t *state,
		renderAssetKind_t kind, const char *name, qboolean clampToEdge,
		const byte *rgba8, uint32_t width, uint32_t height ) {
	renderTextureRecord_t *record;
	byte *owned;
	uint64_t byteCount64;
	uint32_t byteCount;
	qhandle_t semanticHandle;
	if ( !state || !state->initialized || !MaterialKind( kind ) || !name
			|| !name[0] || strlen( name ) >= MAX_QPATH || !rgba8
			|| !width || !height
			|| width > RENDER_SUBMISSION_MAX_IMAGE_DIMENSION
			|| height > RENDER_SUBMISSION_MAX_IMAGE_DIMENSION
			|| ( clampToEdge != qfalse && clampToEdge != qtrue ) ) return 0;
	byteCount64 = (uint64_t)width * (uint64_t)height * 4u;
	if ( byteCount64 > UINT32_MAX ) return 0;
	byteCount = (uint32_t)byteCount64;
	/* A semantic material name may be the shader identity while its snapshot
	 * carries that shader's primary stage.  Reuse it only when the requested
	 * stage is the exact same texture payload; otherwise a later stage whose
	 * authored image happens to equal the shader name would incorrectly sample
	 * the primary image (for example the plasma body's glow map). */
	semanticHandle = RenderSubmission_MaterialHandle( state, name );
	if ( semanticHandle > 0 ) {
		renderTextureSnapshot_t snapshot;
		if ( RenderSubmission_TextureSnapshot( state, semanticHandle, &snapshot )
				&& snapshot.ready && snapshot.width == width
				&& snapshot.height == height && snapshot.byteCount == byteCount
				&& snapshot.clampToEdge == clampToEdge
				&& snapshot.rgba8 && !memcmp( snapshot.rgba8, rgba8, byteCount ) )
			return semanticHandle;
	}
	for ( uint32_t i = 0u; i < state->stageTextureCount; ++i ) {
		const renderTextureSnapshot_t *snapshot =
			&state->stageTextures[i].snapshot;
		if ( strcasecmp( state->stageTextures[i].name, name ) ) continue;
		/* A stage binding includes sampler addressing: map and clampmap are
		 * valid simultaneous uses of the same immutable image. */
		if ( snapshot->clampToEdge != clampToEdge ) continue;
		if ( snapshot->width != width || snapshot->height != height
				|| snapshot->byteCount != byteCount
				|| !snapshot->rgba8
				|| memcmp( snapshot->rgba8, rgba8, byteCount ) ) return 0;
		return snapshot->handle;
	}
	if ( state->stageTextureCount >= RENDER_SUBMISSION_MAX_STAGE_TEXTURES
			|| state->nextHandle >= INT_MAX
			|| state->nextMaterialGeneration == UINT64_MAX ) return 0;
	if ( byteCount64 > RENDER_SUBMISSION_MAX_STAGE_TEXTURE_BYTES
				- state->stageTextureBytes ) return 0;
	owned = (byte *)malloc( byteCount );
	if ( !owned ) return 0;
	memcpy( owned, rgba8, byteCount );
	record = &state->stageTextures[state->stageTextureCount++];
	memset( record, 0, sizeof( *record ) );
	(void)snprintf( record->name, sizeof( record->name ), "%s", name );
	record->snapshot.handle = (qhandle_t)state->nextHandle++;
	record->snapshot.generation = state->nextMaterialGeneration++;
	record->snapshot.width = width;
	record->snapshot.height = height;
	record->snapshot.rowBytes = width * 4u;
	record->snapshot.byteCount = byteCount;
	record->snapshot.rgba8 = owned;
	record->snapshot.clampToEdge = clampToEdge;
	record->snapshot.msdf = kind == RENDER_ASSET_MSDF ? qtrue : qfalse;
	record->snapshot.srgb = record->snapshot.msdf ? qfalse : qtrue;
	record->snapshot.ready = qtrue;
	record->snapshot.digest = TextureSnapshotDigest( record->name,
		&record->snapshot );
	state->stageTextureBytes += byteCount;
	RebuildMaterialDigest( state );
	return record->snapshot.handle;
}

qboolean RenderSubmission_SetMaterialRasterPolicy( renderSubmissionState_t *state, qhandle_t handle,
												   renderAlphaMode_t alphaMode, float alphaCutoff, qboolean depthWrite )
{
	uint32_t i;
	if ( !state || !state->initialized || handle <= 0 || alphaMode < RENDER_ALPHA_OPAQUE ||
		 alphaMode > RENDER_ALPHA_ALPHA_ADDITIVE || !isfinite( alphaCutoff ) || alphaCutoff < 0.0f || alphaCutoff > 1.0f ||
		 ( depthWrite != qfalse && depthWrite != qtrue ) || state->nextMaterialGeneration == UINT64_MAX )
		return qfalse;
	for ( i = 0u; i < state->materialCount; ++i ) {
		renderMaterialSnapshot_t *snapshot = &state->materials[i].snapshot;
		if ( snapshot->handle != handle )
			continue;
		if ( snapshot->alphaMode == alphaMode && snapshot->alphaCutoff == alphaCutoff &&
			 snapshot->depthWrite == depthWrite )
			return qtrue;
		snapshot->alphaMode	  = alphaMode;
		snapshot->alphaCutoff = alphaCutoff;
		snapshot->depthWrite  = depthWrite;
		snapshot->generation  = state->nextMaterialGeneration++;
		snapshot->digest	  = MaterialSnapshotDigest( state->materials[i].name, snapshot );
		RebuildMaterialDigest( state );
		return qtrue;
	}
	return qfalse;
}

qboolean RenderSubmission_SetMaterialSort( renderSubmissionState_t *state,
		qhandle_t handle, float sort ) {
	if ( !state || !state->initialized || handle <= 0 || !isfinite( sort )
			|| sort <= 0.0f || state->nextMaterialGeneration == UINT64_MAX )
		return qfalse;
	for ( uint32_t i = 0u; i < state->materialCount; ++i ) {
		renderMaterialSnapshot_t *snapshot = &state->materials[i].snapshot;
		if ( snapshot->handle != handle ) continue;
		if ( snapshot->sort == sort ) return qtrue;
		snapshot->sort = sort;
		snapshot->generation = state->nextMaterialGeneration++;
		snapshot->digest = MaterialSnapshotDigest( state->materials[i].name,
			snapshot );
		RebuildMaterialDigest( state );
		return qtrue;
	}
	return qfalse;
}

qboolean RenderSubmission_SetMaterialCullMode( renderSubmissionState_t *state,
		qhandle_t handle, renderCullMode_t cullMode ) {
	if ( !state || !state->initialized || handle <= 0
			|| cullMode < RENDER_CULL_BACK || cullMode > RENDER_CULL_NONE
			|| state->nextMaterialGeneration == UINT64_MAX ) return qfalse;
	for ( uint32_t i = 0u; i < state->materialCount; ++i ) {
		renderMaterialSnapshot_t *snapshot = &state->materials[i].snapshot;
		if ( snapshot->handle != handle ) continue;
		if ( snapshot->cullMode == cullMode ) return qtrue;
		snapshot->cullMode = cullMode;
		snapshot->generation = state->nextMaterialGeneration++;
		snapshot->digest = MaterialSnapshotDigest( state->materials[i].name,
			snapshot );
		RebuildMaterialDigest( state );
		return qtrue;
	}
	return qfalse;
}

qboolean RenderSubmission_SetMaterialSky( renderSubmissionState_t *state,
		qhandle_t handle, qboolean sky ) {
	if ( !state || !state->initialized || handle <= 0
			|| ( sky != qfalse && sky != qtrue )
			|| state->nextMaterialGeneration == UINT64_MAX ) return qfalse;
	for ( uint32_t i = 0u; i < state->materialCount; ++i ) {
		renderMaterialSnapshot_t *snapshot = &state->materials[i].snapshot;
		if ( snapshot->handle != handle ) continue;
		if ( snapshot->sky == sky ) return qtrue;
		snapshot->sky = sky;
		snapshot->generation = state->nextMaterialGeneration++;
		snapshot->digest = MaterialSnapshotDigest( state->materials[i].name,
			snapshot );
		RebuildMaterialDigest( state );
		return qtrue;
	}
	return qfalse;
}

qboolean RenderSubmission_SetMaterialAuthoredSkySource(
		renderSubmissionState_t *state, qhandle_t handle,
		const ralMaterialSourceSky_t *source ) {
	if ( !state || !state->initialized || handle <= 0 || !source
			|| state->nextMaterialGeneration == UINT64_MAX
			|| ( source->hasSun != qfalse && source->hasSun != qtrue )
			|| !isfinite( source->sunIntensity ) || source->sunIntensity < 0.0f
			|| !isfinite( source->sunAzimuthDegrees )
			|| !isfinite( source->sunElevationDegrees )
			|| !isfinite( source->sunDevianceDegrees )
			|| source->sunDevianceDegrees < 0.0f ) return qfalse;
	for ( uint32_t channel = 0u; channel < 3u; ++channel )
		if ( !isfinite( source->sunColor[channel] )
				|| source->sunColor[channel] < 0.0f ) return qfalse;
	for ( uint32_t i = 0u; i < state->materialCount; ++i ) {
		renderMaterialSnapshot_t *snapshot = &state->materials[i].snapshot;
		if ( snapshot->handle != handle ) continue;
		if ( !snapshot->sky || !snapshot->rgba8 || !snapshot->width
				|| !snapshot->height ) return qfalse;
		snapshot->authoredSkySource = *source;
		/* Source metadata does not imply a colour source.  In particular, the
		 * frontend material record may still contain its 1x1 white registration
		 * placeholder.  Cube faces or ordered Q3 sky stages publish the actual
		 * environment average separately. */
		for ( uint32_t channel = 0u; channel < 3u; ++channel )
			snapshot->authoredSkyAverage[channel] = 0.04f;
		snapshot->authoredSkySourceReady = qtrue;
		snapshot->generation = state->nextMaterialGeneration++;
		snapshot->digest = MaterialSnapshotDigest( state->materials[i].name,
			snapshot );
		RebuildMaterialDigest( state );
		return qtrue;
	}
	return qfalse;
}

qboolean RenderSubmission_SetMaterialAuthoredSkyAverage(
		renderSubmissionState_t *state, qhandle_t handle,
		const float average[3] ) {
	if ( !state || !state->initialized || handle <= 0 || !average
			|| state->nextMaterialGeneration == UINT64_MAX ) return qfalse;
	for ( uint32_t channel = 0u; channel < 3u; ++channel )
		if ( !isfinite( average[channel] ) || average[channel] < 0.0f
				|| average[channel] > 16.0f ) return qfalse;
	for ( uint32_t i = 0u; i < state->materialCount; ++i ) {
		renderMaterialSnapshot_t *snapshot = &state->materials[i].snapshot;
		if ( snapshot->handle != handle ) continue;
		if ( !snapshot->sky || !snapshot->authoredSkySourceReady ) return qfalse;
		if ( !memcmp( snapshot->authoredSkyAverage, average,
				sizeof( snapshot->authoredSkyAverage ) ) ) return qtrue;
		memcpy( snapshot->authoredSkyAverage, average,
			sizeof( snapshot->authoredSkyAverage ) );
		snapshot->generation = state->nextMaterialGeneration++;
		snapshot->digest = MaterialSnapshotDigest( state->materials[i].name,
			snapshot );
		RebuildMaterialDigest( state );
		return qtrue;
	}
	return qfalse;
}

qboolean RenderSubmission_SetMaterialAuthoredSkyTextures(
		renderSubmissionState_t *state, qhandle_t handle,
		const qhandle_t outerTextures[RAL_MATERIAL_SKY_FACE_COUNT],
		const qhandle_t innerTextures[RAL_MATERIAL_SKY_FACE_COUNT] ) {
	if ( !state || !state->initialized || handle <= 0 || !outerTextures
			|| !innerTextures || state->nextMaterialGeneration == UINT64_MAX )
		return qfalse;
	for ( uint32_t i = 0u; i < state->materialCount; ++i ) {
		renderMaterialSnapshot_t *snapshot = &state->materials[i].snapshot;
		uint64_t red = 0u, green = 0u, blue = 0u, samples = 0u;
		const qhandle_t *averageTextures;
		if ( snapshot->handle != handle ) continue;
		if ( !snapshot->sky || !snapshot->authoredSkySourceReady ) return qfalse;
		for ( uint32_t box = 0u; box < 2u; ++box ) {
			const qboolean required = box == 0u
				? snapshot->authoredSkySource.outerBox
				: snapshot->authoredSkySource.innerBox;
			const qhandle_t *textures = box == 0u ? outerTextures : innerTextures;
			for ( uint32_t face = 0u; face < RAL_MATERIAL_SKY_FACE_COUNT; ++face ) {
				renderTextureSnapshot_t texture;
				if ( !required ) {
					if ( textures[face] != 0 ) return qfalse;
					continue;
				}
				if ( textures[face] <= 0
						|| !RenderSubmission_TextureSnapshot( state,
							textures[face], &texture ) || !texture.ready
						|| !texture.rgba8 || !texture.width || !texture.height )
					return qfalse;
			}
		}
		averageTextures = snapshot->authoredSkySource.outerBox
			? outerTextures : ( snapshot->authoredSkySource.innerBox
				? innerTextures : NULL );
		if ( averageTextures ) {
			for ( uint32_t face = 0u; face < RAL_MATERIAL_SKY_FACE_COUNT; ++face ) {
				renderTextureSnapshot_t texture;
				uint32_t pixelCount, step;
				if ( !RenderSubmission_TextureSnapshot( state,
						averageTextures[face], &texture ) ) return qfalse;
				pixelCount = texture.width * texture.height;
				step = pixelCount > 1024u ? pixelCount / 1024u : 1u;
				for ( uint32_t pixel = 0u; pixel < pixelCount; pixel += step ) {
					red += texture.rgba8[4u * pixel + 0u];
					green += texture.rgba8[4u * pixel + 1u];
					blue += texture.rgba8[4u * pixel + 2u];
					samples++;
				}
			}
			if ( !samples ) return qfalse;
		} else {
			/* Stage-only skies have no cube faces. Derive their environment
			 * seed from the already registered canonical stage images on every
			 * backend. This is load-time metadata, not the animated sky capture:
			 * the ordered GPU stage program remains the radiance authority. */
			for ( uint32_t stageIndex = 0u; stageIndex < snapshot->stageCount; ++stageIndex ) {
				const renderMaterialStageSnapshot_t *stage = &snapshot->stages[stageIndex];
				renderTextureSnapshot_t texture;
				uint32_t pixelCount, step;
				if ( stage->imageSource != 0u ) continue;
				if ( !RenderSubmission_TextureSnapshot( state, stage->material, &texture )
						|| !texture.ready || !texture.rgba8 || !texture.width || !texture.height )
					return qfalse;
				pixelCount = texture.width * texture.height;
				step = pixelCount > 2048u ? pixelCount / 2048u : 1u;
				for ( uint32_t pixel = 0u; pixel < pixelCount; pixel += step ) {
					red += texture.rgba8[4u * pixel + 0u];
					green += texture.rgba8[4u * pixel + 1u];
					blue += texture.rgba8[4u * pixel + 2u];
					samples++;
				}
			}
		}
		memcpy( snapshot->authoredSkyOuterTextures, outerTextures,
			sizeof( snapshot->authoredSkyOuterTextures ) );
		memcpy( snapshot->authoredSkyInnerTextures, innerTextures,
			sizeof( snapshot->authoredSkyInnerTextures ) );
		if ( samples ) {
			snapshot->authoredSkyAverage[0] = (float)red / ( 255.0f * samples );
			snapshot->authoredSkyAverage[1] = (float)green / ( 255.0f * samples );
			snapshot->authoredSkyAverage[2] = (float)blue / ( 255.0f * samples );
		}
		snapshot->authoredSkyTexturesReady = qtrue;
		snapshot->generation = state->nextMaterialGeneration++;
		snapshot->digest = MaterialSnapshotDigest( state->materials[i].name,
			snapshot );
		RebuildMaterialDigest( state );
		return qtrue;
	}
	return qfalse;
}

qboolean RenderSubmission_MaterialStagesUseTime( const renderMaterialSnapshot_t *material ) {
	if ( !material || material->stageCount > RENDER_MATERIAL_MAX_STAGES ) return qfalse;
	for ( uint32_t slot = 0u; slot < material->stageCount; ++slot ) {
		const renderMaterialStageSnapshot_t *stage = &material->stages[slot];
		if ( stage->animationFrameCount > 1u && stage->animationFrequency > 0.0f ) return qtrue;
		if ( stage->rgbGen == RAL_MATERIAL_COLOR_WAVE
				&& stage->rgbWave.function != RAL_MATERIAL_WAVE_NONE
				&& stage->rgbWave.amplitude != 0.0f && stage->rgbWave.frequency != 0.0f ) return qtrue;
		if ( stage->alphaGen == RAL_MATERIAL_ALPHA_WAVE
				&& stage->alphaWave.function != RAL_MATERIAL_WAVE_NONE
				&& stage->alphaWave.amplitude != 0.0f && stage->alphaWave.frequency != 0.0f ) return qtrue;
		if ( !stage->tcModCount ) {
			if ( stage->scaleScroll[2] != 0.0f || stage->scaleScroll[3] != 0.0f
					|| stage->rotateDegrees != 0.0f
					|| ( stage->hasTurbulence && stage->turbulence[1] != 0.0f && stage->turbulence[3] != 0.0f )
					|| ( stage->hasStretch && stage->stretch[1] != 0.0f && stage->stretch[3] != 0.0f ) ) return qtrue;
		} else for ( uint32_t op = 0u; op < stage->tcModCount && op < RAL_MATERIAL_SOURCE_MAX_TCMODS; ++op ) {
			const renderMaterialTcMod_t *tcMod = &stage->tcMods[op];
			const float *p = tcMod->parameters;
			if ( ( tcMod->type == RAL_MATERIAL_TCMOD_SCROLL && ( p[0] != 0.0f || p[1] != 0.0f ) )
					|| ( tcMod->type == RAL_MATERIAL_TCMOD_ROTATE && p[0] != 0.0f )
					|| ( tcMod->type == RAL_MATERIAL_TCMOD_TURBULENCE && p[1] != 0.0f && p[3] != 0.0f )
					|| ( tcMod->type == RAL_MATERIAL_TCMOD_STRETCH
						&& p[0] != (float)RAL_MATERIAL_WAVE_NONE && p[2] != 0.0f && p[4] != 0.0f ) ) return qtrue;
		}
	}
	return qfalse;
}

qboolean RenderSubmission_ActiveSkyMaterial(
		const renderSubmissionState_t *state,
		renderMaterialSnapshot_t *outMaterial,
		char outSemanticName[MAX_QPATH] ) {
	const renderSubmissionWorldSlot_t *world;
	if ( !state || !state->initialized || !outMaterial || !outSemanticName
			|| state->activeWorldIndex < 0
			|| state->activeWorldIndex >= MAX_RENDER_WORLDS ) return qfalse;
	world = &state->worlds[state->activeWorldIndex];
	if ( !world->loaded || !world->snapshot.ready ) return qfalse;
	for ( uint32_t batchIndex = 0u; batchIndex < world->snapshot.batchCount;
			++batchIndex ) {
		const renderWorldBatch_t *batch = &world->snapshot.batches[batchIndex];
		if ( !batch->sky || batch->noDraw || batch->baseMaterial <= 0 ) continue;
		for ( uint32_t materialIndex = 0u;
				materialIndex < state->materialCount; ++materialIndex ) {
			const renderMaterialRecord_t *record = &state->materials[materialIndex];
			if ( record->snapshot.handle != batch->baseMaterial
					|| !record->snapshot.sky
					|| !record->snapshot.authoredSkySourceReady ) continue;
			*outMaterial = record->snapshot;
			memcpy( outSemanticName, record->name, MAX_QPATH );
			outSemanticName[MAX_QPATH - 1u] = '\0';
			return qtrue;
		}
	}
	return qfalse;
}

qboolean RenderSubmission_SetMaterialSkyBox( renderSubmissionState_t *state,
		qhandle_t handle, qboolean skyBox ) {
	if ( !state || !state->initialized || handle <= 0
			|| ( skyBox != qfalse && skyBox != qtrue )
			|| state->nextMaterialGeneration == UINT64_MAX ) return qfalse;
	for ( uint32_t i = 0u; i < state->materialCount; ++i ) {
		renderMaterialSnapshot_t *snapshot = &state->materials[i].snapshot;
		if ( snapshot->handle != handle ) continue;
		if ( snapshot->skyBox == skyBox ) return qtrue;
		snapshot->skyBox = skyBox;
		snapshot->generation = state->nextMaterialGeneration++;
		snapshot->digest = MaterialSnapshotDigest( state->materials[i].name,
			snapshot );
		RebuildMaterialDigest( state );
		return qtrue;
	}
	return qfalse;
}

qboolean RenderSubmission_SetMaterialSkyCloudHeight( renderSubmissionState_t *state,
		qhandle_t handle, float cloudHeight ) {
	if ( !state || !state->initialized || handle <= 0
			|| !isfinite( cloudHeight ) || cloudHeight <= 0.0f
			|| state->nextMaterialGeneration == UINT64_MAX ) return qfalse;
	for ( uint32_t i = 0u; i < state->materialCount; ++i ) {
		renderMaterialSnapshot_t *snapshot = &state->materials[i].snapshot;
		if ( snapshot->handle != handle ) continue;
		if ( snapshot->skyCloudHeight == cloudHeight ) return qtrue;
		snapshot->skyCloudHeight = cloudHeight;
		snapshot->generation = state->nextMaterialGeneration++;
		snapshot->digest = MaterialSnapshotDigest( state->materials[i].name,
			snapshot );
		RebuildMaterialDigest( state );
		return qtrue;
	}
	return qfalse;
}

qboolean RenderSubmission_SetMaterialNoDraw( renderSubmissionState_t *state,
		qhandle_t handle, qboolean noDraw ) {
	if ( !state || !state->initialized || handle <= 0
			|| ( noDraw != qfalse && noDraw != qtrue )
			|| state->nextMaterialGeneration == UINT64_MAX ) return qfalse;
	for ( uint32_t i = 0u; i < state->materialCount; ++i ) {
		renderMaterialSnapshot_t *snapshot = &state->materials[i].snapshot;
		if ( snapshot->handle != handle ) continue;
		if ( snapshot->noDraw == noDraw ) return qtrue;
		snapshot->noDraw = noDraw;
		snapshot->generation = state->nextMaterialGeneration++;
		snapshot->digest = MaterialSnapshotDigest( state->materials[i].name,
			snapshot );
		RebuildMaterialDigest( state );
		return qtrue;
	}
	return qfalse;
}

qboolean RenderSubmission_SetMaterialSkyProjection( renderSubmissionState_t *state,
		qhandle_t handle, const float scale[2], const float scroll[2] ) {
	if ( !state || !state->initialized || handle <= 0 || !scale || !scroll
			|| !isfinite( scale[0] ) || !isfinite( scale[1] )
			|| !isfinite( scroll[0] ) || !isfinite( scroll[1] )
			|| scale[0] == 0.0f || scale[1] == 0.0f
			|| state->nextMaterialGeneration == UINT64_MAX ) return qfalse;
	for ( uint32_t i = 0u; i < state->materialCount; ++i ) {
		renderMaterialSnapshot_t *snapshot = &state->materials[i].snapshot;
		float values[4] = { scale[0], scale[1], scroll[0], scroll[1] };
		if ( snapshot->handle != handle ) continue;
		if ( !memcmp( snapshot->skyScaleScroll, values, sizeof( values ) ) )
			return qtrue;
		memcpy( snapshot->skyScaleScroll, values, sizeof( values ) );
		snapshot->generation = state->nextMaterialGeneration++;
		snapshot->digest = MaterialSnapshotDigest( state->materials[i].name,
			snapshot );
		RebuildMaterialDigest( state );
		return qtrue;
	}
	return qfalse;
}

qboolean RenderSubmission_SetMaterialSkySecondary( renderSubmissionState_t *state,
		qhandle_t handle, qhandle_t secondaryMaterial, renderAlphaMode_t alphaMode,
		const float scale[2], const float scroll[2] ) {
	qboolean secondaryExists = qfalse;
	if ( !state || !state->initialized || handle <= 0 || secondaryMaterial <= 0
			|| handle == secondaryMaterial || !scale || !scroll
			|| alphaMode < RENDER_ALPHA_OPAQUE || alphaMode > RENDER_ALPHA_ALPHA_ADDITIVE
			|| !isfinite( scale[0] ) || !isfinite( scale[1] )
			|| !isfinite( scroll[0] ) || !isfinite( scroll[1] )
			|| scale[0] == 0.0f || scale[1] == 0.0f
			|| state->nextMaterialGeneration == UINT64_MAX ) return qfalse;
	for ( uint32_t i = 0u; i < state->materialCount; ++i )
		if ( state->materials[i].snapshot.handle == secondaryMaterial ) {
			secondaryExists = qtrue; break;
		}
	if ( !secondaryExists ) return qfalse;
	for ( uint32_t i = 0u; i < state->materialCount; ++i ) {
		renderMaterialSnapshot_t *snapshot = &state->materials[i].snapshot;
		float values[4] = { scale[0], scale[1], scroll[0], scroll[1] };
		if ( snapshot->handle != handle ) continue;
		if ( snapshot->skySecondaryMaterial == secondaryMaterial
				&& snapshot->skySecondaryAlphaMode == alphaMode
				&& !memcmp( snapshot->skySecondaryScaleScroll, values,
					sizeof( values ) ) ) return qtrue;
		snapshot->skySecondaryMaterial = secondaryMaterial;
		snapshot->skySecondaryAlphaMode = alphaMode;
		memcpy( snapshot->skySecondaryScaleScroll, values, sizeof( values ) );
		snapshot->generation = state->nextMaterialGeneration++;
		snapshot->digest = MaterialSnapshotDigest( state->materials[i].name,
			snapshot );
		RebuildMaterialDigest( state );
		return qtrue;
	}
	return qfalse;
}

qboolean RenderSubmission_SetMaterialStages( renderSubmissionState_t *state,
		qhandle_t handle, const renderMaterialStageSnapshot_t *stages,
		uint32_t stageCount ) {
	ralMaterialStageDepth_t depthStages[RENDER_MATERIAL_MAX_STAGES];
	ralMaterialStageDepthPlan_t depthPlan;
	ralMaterialStageBlend_t blendStages[RENDER_MATERIAL_MAX_STAGES];
	qboolean blendAdditive;
	if ( !state || !state->initialized || handle <= 0 || !stages
			|| !stageCount || stageCount > RENDER_MATERIAL_MAX_STAGES
			|| state->nextMaterialGeneration == UINT64_MAX ) return qfalse;
	for ( uint32_t stageIndex = 0u; stageIndex < stageCount; ++stageIndex ) {
		const renderMaterialStageSnapshot_t *stage = &stages[stageIndex];
		depthStages[stageIndex].depthFunc = stage->depthFunc;
		depthStages[stageIndex].depthWrite = stage->depthWrite;
		depthStages[stageIndex].alphaTest = stage->alphaTest;
		blendStages[stageIndex].sourceBlend = stage->sourceBlend;
		blendStages[stageIndex].destinationBlend = stage->destinationBlend;
		qboolean materialExists = stage->imageSource != 0u;
		if ( stage->imageSource > 2u
				|| stage->tcModCount > RAL_MATERIAL_SOURCE_MAX_TCMODS
				|| stage->tcGen > RENDER_MATERIAL_TCGEN_VECTOR
				|| stage->sourceBlend > 10u
				|| stage->destinationBlend > 10u
				|| stage->alphaTest > RAL_MATERIAL_ALPHA_TEST_KEEP_GREATER_EQUAL
				|| stage->depthFunc > RAL_MATERIAL_DEPTH_EQUAL
				|| ( stage->depthWrite != qfalse && stage->depthWrite != qtrue )
				|| stage->rgbGen > RAL_MATERIAL_COLOR_CONST
				|| stage->alphaGen > RAL_MATERIAL_ALPHA_CONST
				|| stage->animationFrameCount > RENDER_MATERIAL_MAX_ANIMATION_FRAMES
				|| ( stage->animationFrameCount
					&& ( !isfinite( stage->animationFrequency )
						|| stage->animationFrequency <= 0.0f ) )
				|| ( stage->animationLooping != qfalse
					&& stage->animationLooping != qtrue )
				|| !isfinite( stage->alphaCutoff ) || stage->alphaCutoff < 0.0f
				|| stage->alphaCutoff > 1.0f
				|| !isfinite( stage->scaleScroll[0] )
				|| !isfinite( stage->scaleScroll[1] )
				|| !isfinite( stage->scaleScroll[2] )
				|| !isfinite( stage->scaleScroll[3] )
				|| ( !stage->tcModCount
					&& ( stage->scaleScroll[0] == 0.0f || stage->scaleScroll[1] == 0.0f ) )
				|| !isfinite( stage->rotateDegrees )
				|| ( stage->hasTurbulence != qfalse
					&& stage->hasTurbulence != qtrue )
				|| ( stage->hasStretch != qfalse && stage->hasStretch != qtrue ) )
			return qfalse;
		for ( uint32_t component = 0u; component < 4u; ++component )
			if ( !isfinite( stage->turbulence[component] )
					|| !isfinite( stage->stretch[component] )
					|| !isfinite( stage->constantColor[component] ) ) return qfalse;
		for ( uint32_t op = 0u; op < stage->tcModCount; ++op ) {
			const renderMaterialTcMod_t *mod = &stage->tcMods[op];
			if ( mod->type < RAL_MATERIAL_TCMOD_SCALE
					|| mod->type > RAL_MATERIAL_TCMOD_TRANSFORM ) return qfalse;
			for ( uint32_t p = 0u; p < 6u; ++p )
				if ( !isfinite( mod->parameters[p] ) ) return qfalse;
			if ( mod->type == RAL_MATERIAL_TCMOD_STRETCH
					&& ( mod->parameters[0] < RAL_MATERIAL_WAVE_SIN
						|| mod->parameters[0] > RAL_MATERIAL_WAVE_NOISE
						|| floorf( mod->parameters[0] ) != mod->parameters[0] ) )
				return qfalse;
		}
		for ( uint32_t vector = 0u; vector < 2u; ++vector )
			for ( uint32_t component = 0u; component < 3u; ++component )
				if ( !isfinite( stage->tcGenVectors[vector][component] ) )
					return qfalse;
		if ( !isfinite( stage->portalRange )
				|| !isfinite( stage->rgbWave.base )
				|| !isfinite( stage->rgbWave.amplitude )
				|| !isfinite( stage->rgbWave.phase )
				|| !isfinite( stage->rgbWave.frequency )
				|| !isfinite( stage->alphaWave.base )
				|| !isfinite( stage->alphaWave.amplitude )
				|| !isfinite( stage->alphaWave.phase )
				|| !isfinite( stage->alphaWave.frequency ) ) return qfalse;
		if ( stage->imageSource == 0u ) {
			renderTextureSnapshot_t texture;
			if ( stage->material <= 0 ) return qfalse;
			materialExists = RenderSubmission_TextureSnapshot( state,
				stage->material, &texture );
		}
		if ( !materialExists ) return qfalse;
		for ( uint32_t frame = 0u; frame < stage->animationFrameCount; ++frame ) {
			renderTextureSnapshot_t texture;
			if ( stage->animationMaterials[frame] <= 0 ) return qfalse;
			if ( !RenderSubmission_TextureSnapshot( state,
					stage->animationMaterials[frame], &texture ) ) return qfalse;
		}
	}
	if ( !Ral_MaterialStageDepthCollapse( depthStages, stageCount, &depthPlan ) )
		return qfalse;
	if ( !Ral_MaterialStageBlendAdditive( blendStages, stageCount, &blendAdditive ) )
		return qfalse;
	for ( uint32_t materialIndex = 0u;
			materialIndex < state->materialCount; ++materialIndex ) {
		renderMaterialSnapshot_t *snapshot =
			&state->materials[materialIndex].snapshot;
		if ( snapshot->handle != handle ) continue;
		if ( snapshot->stageCount == stageCount
				&& !memcmp( snapshot->stages, stages,
					(size_t)stageCount * sizeof( stages[0] ) ) ) return qtrue;
		memset( snapshot->stages, 0, sizeof( snapshot->stages ) );
		memcpy( snapshot->stages, stages,
			(size_t)stageCount * sizeof( stages[0] ) );
		snapshot->stageCount = stageCount;
		snapshot->stageDepth = depthPlan;
		snapshot->stageBlendAdditive = blendAdditive;
		snapshot->generation = state->nextMaterialGeneration++;
		snapshot->digest = MaterialSnapshotDigest(
			state->materials[materialIndex].name, snapshot );
		RebuildMaterialDigest( state );
		return qtrue;
	}
	return qfalse;
}

qboolean RenderSubmission_SetMaterialLighting( renderSubmissionState_t *state, qhandle_t handle,
											const renderMaterialLighting_t *lighting )
{
	uint32_t i, channel;
	qboolean emissive;
	if ( !state || !state->initialized || handle <= 0 || !lighting ||
		 lighting->schemaVersion != RENDER_MATERIAL_LIGHTING_SCHEMA_VERSION || !lighting->sourceGeneration ||
		 lighting->sourceGeneration == UINT64_MAX || !lighting->provenanceHash ||
		 lighting->provenanceHash == UINT64_MAX ||
		 ( lighting->participatesInStaticBake != qfalse && lighting->participatesInStaticBake != qtrue ) ||
		 lighting->ready != qtrue || state->nextMaterialGeneration == UINT64_MAX )
		return qfalse;
	for ( channel = 0u; channel < 3u; ++channel ) {
		if ( lighting->diffuseReflectanceQ16[channel] < 0 ||
			 lighting->diffuseReflectanceQ16[channel] > RENDER_MATERIAL_LIGHTING_Q16_ONE ||
			 lighting->emissionRadianceQ16[channel] < 0 )
			return qfalse;
	}
	emissive = lighting->emissionRadianceQ16[0] || lighting->emissionRadianceQ16[1] ||
		lighting->emissionRadianceQ16[2];
	if ( ( lighting->emissiveExplicitProxyAuthority != qfalse &&
		   lighting->emissiveExplicitProxyAuthority != qtrue ) ||
		 ( lighting->emissiveInjectsAtmosphere != qfalse &&
		   lighting->emissiveInjectsAtmosphere != qtrue ) ||
		 lighting->emissiveRequestedProxyCount > RAL_EMISSIVE_PROXY_MAX_PER_SURFACE ||
		 lighting->emissiveShadowPriority > UINT16_MAX ||
		 ( emissive && ( lighting->emissiveMobility < RAL_LIGHT_MOBILITY_STATIC ||
			 lighting->emissiveMobility > RAL_LIGHT_MOBILITY_DYNAMIC ||
			 lighting->emissiveInfluenceRangeQ16 <= 0 ||
			 ( lighting->emissiveMobility == RAL_LIGHT_MOBILITY_DYNAMIC &&
			   lighting->participatesInStaticBake ) ) ) ||
		 ( !emissive && ( lighting->emissiveMobility || lighting->emissiveInfluenceRangeQ16 ||
			 lighting->emissiveShadowPriority || lighting->emissiveRequestedProxyCount ||
			 lighting->emissiveExplicitProxyAuthority || lighting->emissiveInjectsAtmosphere ) ) )
		return qfalse;
	for ( i = 0u; i < state->materialCount; ++i ) {
		renderMaterialSnapshot_t *snapshot = &state->materials[i].snapshot;
		if ( snapshot->handle != handle )
			continue;
		if ( !memcmp( &snapshot->lighting, lighting, sizeof( *lighting ) ) )
			return qtrue;
		snapshot->lighting   = *lighting;
		snapshot->generation = state->nextMaterialGeneration++;
		snapshot->digest     = MaterialSnapshotDigest( state->materials[i].name, snapshot );
		RebuildMaterialDigest( state );
		if ( state->worldLoaded ) {
			if ( state->frameOpen )
				state->emissiveAuthorityDirty = qtrue;
			else
				state->emissiveAuthorityDirty = RenderSubmission_RebuildEmissiveAuthority( state,
					state->nextMaterialGeneration ) ? qfalse : qtrue;
		}
		return qtrue;
	}
	return qfalse;
}

static qboolean MaterialFactorsValid( const ralMaterialFactors_t *factors )
{
	uint32_t channel;
	if ( !factors ) return qfalse;
	for ( channel = 0u; channel < 4u; ++channel )
		if ( !isfinite( factors->baseColor[channel] )
				|| factors->baseColor[channel] < 0.0f
				|| factors->baseColor[channel] > 1.0f ) return qfalse;
	for ( channel = 0u; channel < 3u; ++channel )
		if ( !isfinite( factors->emissive[channel] )
				|| factors->emissive[channel] < 0.0f ) return qfalse;
	return isfinite( factors->metallic ) && factors->metallic >= 0.0f
		&& factors->metallic <= 1.0f
		&& isfinite( factors->roughness ) && factors->roughness >= 0.0f
		&& factors->roughness <= 1.0f
		&& isfinite( factors->normalScale ) && factors->normalScale >= 0.0f
		&& isfinite( factors->occlusionStrength )
		&& factors->occlusionStrength >= 0.0f
		&& factors->occlusionStrength <= 1.0f;
}

qboolean RenderSubmission_PublishMaterialFactors(
		renderSubmissionState_t *state, uint64_t runtimeGeneration,
		const renderMaterialFactorPublicationEntry_t *entries,
		uint32_t entryCount )
{
	ralMaterialFactors_t candidates[RENDER_SUBMISSION_MAX_MATERIALS];
	qboolean candidateReady[RENDER_SUBMISSION_MAX_MATERIALS];
	uint32_t changedCount = 0u;
	if ( !state || !state->initialized
			|| entryCount > state->materialCount
			|| ( entryCount && ( !entries || !runtimeGeneration
				|| runtimeGeneration == UINT64_MAX ) )
			|| ( !entryCount && runtimeGeneration ) ) return qfalse;
	memset( candidates, 0, sizeof( candidates ) );
	memset( candidateReady, 0, sizeof( candidateReady ) );
	for ( uint32_t entry = 0u; entry < entryCount; ++entry ) {
		uint32_t index;
		if ( entries[entry].material <= 0
				|| !MaterialFactorsValid( &entries[entry].factors )
				|| !RenderSubmission_MaterialIndex( state, entries[entry].material,
					&index ) || candidateReady[index] ) return qfalse;
		candidates[index] = entries[entry].factors;
		candidateReady[index] = qtrue;
	}
	if ( state->worldLoaded ) {
		for ( uint32_t batch = 0u; batch < state->worldSnapshot.batchCount;
				++batch ) {
			const renderWorldBatch_t *worldBatch =
				&state->worldSnapshot.batches[batch];
			uint32_t index;
			qboolean found = worldBatch->noDraw
				|| ( RenderSubmission_MaterialIndex( state, worldBatch->baseMaterial,
					&index ) && candidateReady[index] );
			if ( !found && entryCount ) return qfalse;
		}
	}
	for ( uint32_t index = 0u; index < state->materialCount; ++index ) {
		const renderMaterialFactorsSnapshot_t *current =
			&state->materials[index].snapshot.factors;
		if ( current->ready != candidateReady[index]
				|| ( candidateReady[index]
					&& ( current->schemaVersion
						!= RENDER_MATERIAL_FACTORS_SCHEMA_VERSION
						|| current->runtimeGeneration != runtimeGeneration
						|| memcmp( &current->factors, &candidates[index],
							sizeof( current->factors ) ) ) ) ) changedCount++;
	}
	if ( changedCount > UINT64_MAX - state->nextMaterialGeneration ) return qfalse;
	for ( uint32_t index = 0u; index < state->materialCount; ++index ) {
		renderMaterialSnapshot_t *snapshot = &state->materials[index].snapshot;
		renderMaterialFactorsSnapshot_t next;
		memset( &next, 0, sizeof( next ) );
		if ( candidateReady[index] ) {
			next.schemaVersion = RENDER_MATERIAL_FACTORS_SCHEMA_VERSION;
			next.runtimeGeneration = runtimeGeneration;
			next.factors = candidates[index];
			next.ready = qtrue;
		}
		if ( !memcmp( &snapshot->factors, &next, sizeof( next ) ) ) continue;
		snapshot->factors = next;
		snapshot->generation = state->nextMaterialGeneration++;
		snapshot->digest = MaterialSnapshotDigest(
			state->materials[index].name, snapshot );
	}
	if ( changedCount ) RebuildMaterialDigest( state );
	return qtrue;
}

qboolean RenderSubmission_RecordAsset( renderSubmissionState_t *state, renderAssetKind_t kind, const char *name,
									   qhandle_t handle )
{
	uint32_t length;
	qboolean material;
	if ( !state || !state->initialized || !KindValid( kind ) || !name || !name[0] || handle <= 0 )
		return qfalse;
	length	 = (uint32_t)strlen( name );
	material = ( kind == RENDER_ASSET_MATERIAL || kind == RENDER_ASSET_MSDF ||
				 kind == RENDER_ASSET_PRIMITIVE_MATERIAL || kind == RENDER_ASSET_LIGHTMAP )
				   ? qtrue
				   : qfalse;
	if ( length >= MAX_QPATH || state->registeredAssetCount == UINT32_MAX ||
		 ( material && state->registeredMaterialCount == UINT32_MAX ) )
		return qfalse;
	state->assetDigest = HashU32( state->assetDigest, (uint32_t)kind );
	state->assetDigest = HashBytes( state->assetDigest, &handle, sizeof( handle ) );
	state->assetDigest = HashBytes( state->assetDigest, name, length + 1u );
	state->registeredAssetCount++;
	if ( material )
		state->registeredMaterialCount++;
	return qtrue;
}

/* The lowered size of one BSP surface: *outSkip for a surface the renderer never draws (bad, flare, nodraw / skip material),
 * *outPatch for a tessellated patch. False on a malformed surface. Shared by the static world (model 0) and inline brush models. */
qboolean RenderSubmission_WorldSurfaceSize( const mapFile_t *bsp, int surfaceIndex, qboolean *outSkip, qboolean *outPatch,
		uint32_t *outVertices, uint32_t *outIndices ) {
	const dsurface_t *surface;
	uint64_t		  addVertices, addIndices;
	if ( !bsp || surfaceIndex < 0 || surfaceIndex >= bsp->numSurfaces || !outSkip || !outPatch || !outVertices || !outIndices )
		return qfalse;
	surface = &bsp->surfaces[surfaceIndex];
	*outSkip = qtrue; *outPatch = qfalse; *outVertices = *outIndices = 0u;
	if ( surface->surfaceType == MST_BAD || surface->surfaceType == MST_FLARE )
		return qtrue;
	if ( surface->surfaceType != MST_PLANAR && surface->surfaceType != MST_TRIANGLE_SOUP &&
		 surface->surfaceType != MST_PATCH )
		return qfalse;
	if ( surface->shaderNum < 0 || surface->shaderNum >= bsp->numShaders || surface->firstVert < 0 ||
		 surface->numVerts < 0 || surface->firstVert > bsp->numDrawVerts - surface->numVerts )
		return qfalse;
	if ( bsp->shaders[surface->shaderNum].surfaceFlags & ( SURF_NODRAW | SURF_SKIP ) )
		return qtrue;
	if ( surface->surfaceType == MST_PATCH ) {
		uint64_t blocks;
		if ( surface->patchWidth < 3 || surface->patchHeight < 3 || !( surface->patchWidth & 1 ) ||
			 !( surface->patchHeight & 1 ) || surface->patchWidth > surface->numVerts ||
			 surface->patchHeight > surface->numVerts / surface->patchWidth ||
			 surface->patchWidth * surface->patchHeight != surface->numVerts )
			return qfalse;
		blocks = (uint64_t)( ( surface->patchWidth - 1 ) / 2 ) * (uint64_t)( ( surface->patchHeight - 1 ) / 2 );
		addVertices =
			blocks * ( RENDER_SUBMISSION_PATCH_SUBDIVISIONS + 1u ) * ( RENDER_SUBMISSION_PATCH_SUBDIVISIONS + 1u );
		addIndices = blocks * RENDER_SUBMISSION_PATCH_SUBDIVISIONS * RENDER_SUBMISSION_PATCH_SUBDIVISIONS * 6u;
		*outPatch = qtrue;
	} else {
		if ( surface->firstIndex < 0 || surface->numIndexes < 0 ||
			 surface->firstIndex > bsp->numDrawIndexes - surface->numIndexes || ( surface->numIndexes % 3 ) != 0 )
			return qfalse;
		addVertices = (uint32_t)surface->numVerts;
		addIndices	= (uint32_t)surface->numIndexes;
	}
	if ( addVertices > UINT32_MAX || addIndices > UINT32_MAX ) return qfalse;
	*outSkip = qfalse;
	*outVertices = (uint32_t)addVertices;
	*outIndices = (uint32_t)addIndices;
	return qtrue;
}

/* Lowers one drawable BSP surface (sized by RenderSubmission_WorldSurfaceSize) into world vertices, indices and one batch:
 * normalised normals, tessellated patches, the base and lightmap materials. */
qboolean RenderSubmission_LowerWorldSurface( renderSubmissionState_t *state, const mapFile_t *bsp, int surfaceIndex,
		renderWorldVertex_t *vertices, uint32_t *vertexCursor, uint32_t *indices, uint32_t *indexCursor,
		renderWorldBatch_t *batch ) {
	const dsurface_t *surface = &bsp->surfaces[surfaceIndex];
	batch->sourceSurfaceIndex = (uint32_t)surfaceIndex;
	batch->firstIndex		  = (*indexCursor);
	batch->shaderIndex		  = surface->shaderNum;
	batch->lightmapIndex	  = surface->lightmapNum;
	batch->surfaceType		  = surface->surfaceType == MST_PATCH
									? RENDER_WORLD_SURFACE_PATCH
									: ( surface->surfaceType == MST_PLANAR ? RENDER_WORLD_SURFACE_PLANAR
																		   : RENDER_WORLD_SURFACE_TRIANGLES );
	batch->alphaMode		  = RENDER_ALPHA_OPAQUE;
	batch->cullMode		  = RENDER_CULL_BACK;
	batch->alphaCutoff		  = 0.5f;
	batch->depthWrite		  = qtrue;
	batch->sort               = RENDER_MATERIAL_SORT_OPAQUE;
	batch->visible			  = qtrue;
	if ( !WorldSurfaceBounds( bsp, surface, batch ) ) {
		return qfalse;
	}
	batch->baseMaterial = RenderSubmission_MaterialHandle( state, bsp->shaders[surface->shaderNum].shader );
	if ( batch->baseMaterial ) {
		renderMaterialSnapshot_t material;
		if ( RenderSubmission_MaterialSnapshot( state, batch->baseMaterial, &material ) ) {
			batch->alphaMode   = material.alphaMode;
			batch->cullMode    = material.cullMode;
			batch->alphaCutoff = material.alphaCutoff;
			batch->depthWrite  = material.depthWrite;
			batch->sort        = material.sort;
			batch->sky			 = material.sky;
			batch->noDraw		 = material.noDraw;
		}
	}
	if ( surface->lightmapNum >= 0 ) {
		char lightmapName[MAX_QPATH];
		if ( !RenderSubmission_LightmapMaterialName( lightmapName, (uint32_t)bsp->checksum,
													 surface->lightmapNum ) ) {
			return qfalse;
		}
		batch->lightmapMaterial = RenderSubmission_MaterialHandle( state, lightmapName );
	}
	if ( surface->surfaceType != MST_PATCH ) {
		uint32_t baseVertex = (*vertexCursor);
		for ( int i = 0; i < surface->numVerts; ++i ) {
			const drawVert_t	*source = &bsp->drawVerts[surface->firstVert + i];
			renderWorldVertex_t *target = &vertices[(*vertexCursor)++];
			memcpy( target->position, source->xyz, sizeof( target->position ) );
			memcpy( target->normal, source->normal, sizeof( target->normal ) );
			{
				float length = sqrtf( target->normal[0] * target->normal[0]
					+ target->normal[1] * target->normal[1]
					+ target->normal[2] * target->normal[2] );
				if ( isfinite( length ) && length > 0.000001f )
					for ( uint32_t axis = 0u; axis < 3u; ++axis )
						target->normal[axis] /= length;
				else {
					target->normal[0] = target->normal[1] = 0.0f;
					target->normal[2] = 1.0f;
				}
			}
			memcpy( target->texCoord, source->st, sizeof( target->texCoord ) );
			memcpy( target->lightmapCoord, source->lightmap, sizeof( target->lightmapCoord ) );
			memcpy( target->color, source->color.rgba, sizeof( target->color ) );
		}
		for ( int i = 0; i < surface->numIndexes; ++i ) {
			int local = bsp->drawIndexes[surface->firstIndex + i];
			if ( local < 0 || local >= surface->numVerts ) {
				return qfalse;
			}
			indices[(*indexCursor)++] = baseVertex + (uint32_t)local;
		}
	} else {
		const uint32_t side = RENDER_SUBMISSION_PATCH_SUBDIVISIONS + 1u;
		for ( int blockY = 0; blockY < surface->patchHeight - 1; blockY += 2 ) {
			for ( int blockX = 0; blockX < surface->patchWidth - 1; blockX += 2 ) {
				uint32_t baseVertex = (*vertexCursor);
				for ( uint32_t y = 0u; y < side; ++y ) {
					float v		= (float)y / RENDER_SUBMISSION_PATCH_SUBDIVISIONS;
					float bv[3] = { ( 1.0f - v ) * ( 1.0f - v ), 2.0f * v * ( 1.0f - v ), v * v };
					for ( uint32_t x = 0u; x < side; ++x ) {
						float u		= (float)x / RENDER_SUBMISSION_PATCH_SUBDIVISIONS;
						float bu[3] = { ( 1.0f - u ) * ( 1.0f - u ), 2.0f * u * ( 1.0f - u ), u * u };
						renderWorldVertex_t *target	  = &vertices[(*vertexCursor)++];
						float				 color[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
						for ( uint32_t cy = 0u; cy < 3u; ++cy ) {
							for ( uint32_t cx = 0u; cx < 3u; ++cx ) {
								const drawVert_t *control =
									&bsp->drawVerts[surface->firstVert +
													( blockY + (int)cy ) * surface->patchWidth + blockX +
													(int)cx];
								float weight = bu[cx] * bv[cy];
								for ( uint32_t axis = 0u; axis < 3u; ++axis )
									target->position[axis] += control->xyz[axis] * weight;
								for ( uint32_t axis = 0u; axis < 3u; ++axis )
									target->normal[axis] += control->normal[axis] * weight;
								for ( uint32_t uv = 0u; uv < 2u; ++uv ) {
									target->texCoord[uv] += control->st[uv] * weight;
									target->lightmapCoord[uv] += control->lightmap[uv] * weight;
								}
								for ( uint32_t channel = 0u; channel < 4u; ++channel )
									color[channel] += control->color.rgba[channel] * weight;
							}
						}
						for ( uint32_t channel = 0u; channel < 4u; ++channel )
							target->color[channel] = (uint8_t)( color[channel] + 0.5f );
						{
							float length = sqrtf( target->normal[0] * target->normal[0]
								+ target->normal[1] * target->normal[1]
								+ target->normal[2] * target->normal[2] );
							if ( length > 0.000001f ) for ( uint32_t axis = 0u; axis < 3u; ++axis )
								target->normal[axis] /= length;
							else target->normal[2] = 1.0f;
						}
					}
				}
				for ( uint32_t y = 0u; y < RENDER_SUBMISSION_PATCH_SUBDIVISIONS; ++y ) {
					for ( uint32_t x = 0u; x < RENDER_SUBMISSION_PATCH_SUBDIVISIONS; ++x ) {
						uint32_t a = baseVertex + y * side + x;
						uint32_t b = a + 1u, c = a + side, d = c + 1u;
						indices[(*indexCursor)++] = a;
						indices[(*indexCursor)++] = c;
						indices[(*indexCursor)++] = b;
						indices[(*indexCursor)++] = b;
						indices[(*indexCursor)++] = c;
						indices[(*indexCursor)++] = d;
					}
				}
			}
		}
	}
	batch->indexCount = (*indexCursor) - batch->firstIndex;
	return qtrue;
}

qboolean RenderSubmission_LoadWorld( renderSubmissionState_t *state, const mapFile_t *bsp, int worldIndex )
{
	uint64_t			 digest		 = FNV_OFFSET;
	uint64_t			 contentIdentity = FNV_OFFSET;
	renderWorldVertex_t *vertices	 = NULL;
	uint32_t			*indices	 = NULL;
	renderWorldBatch_t	*batches	 = NULL;
	uint32_t			 vertexCount = 0u, indexCount = 0u, batchCount = 0u;
	uint32_t			 patchBatchCount = 0u, patchTriangleCount = 0u;
	byte                *visibility = NULL;
	uint32_t             visibilityBytes = 0u;
	renderSubmissionWorldSlot_t *slot;
	int				 surfaceFirst = 0, surfaceEnd;
	if ( !state || !state->initialized || state->frameOpen || !bsp || worldIndex < 0
			|| worldIndex >= MAX_RENDER_WORLDS || bsp->numSurfaces < 0 || bsp->numDrawVerts < 0 ||
		 bsp->numDrawIndexes < 0 || bsp->numShaders < 0 || ( bsp->numSurfaces && !bsp->surfaces ) ||
		 ( bsp->numDrawVerts && !bsp->drawVerts ) || ( bsp->numDrawIndexes && !bsp->drawIndexes ) ||
		 ( bsp->numShaders && !bsp->shaders ) || bsp->numSubModels < 0 ||
		 ( bsp->numSubModels && !bsp->subModels ) )
		return qfalse;
	if ( state->nextWorldLifecycleGeneration >= UINT64_MAX - 1u ) return qfalse;
	/* The BSP surface lump also owns inline brush-model surfaces.  Model 0 is
	 * the static world; later models are submitted through refEntity_t and must
	 * not be lowered here as a second, origin-space copy. */
	surfaceEnd = bsp->numSurfaces;
	if ( bsp->numSubModels > 0 ) {
		const dmodel_t *world = &bsp->subModels[0];
		if ( world->firstSurface < 0 || world->numSurfaces < 0
				|| world->firstSurface > bsp->numSurfaces - world->numSurfaces )
			return qfalse;
		surfaceFirst = world->firstSurface;
		surfaceEnd = surfaceFirst + world->numSurfaces;
	}
	digest = HashBytes( digest, bsp->name, strnlen( bsp->name, sizeof( bsp->name ) ) );
	digest = HashBytes( digest, &bsp->checksum, sizeof( bsp->checksum ) );
	digest = HashBytes( digest, &worldIndex, sizeof( worldIndex ) );
	digest = HashBytes( digest, bsp->surfaces, (size_t)bsp->numSurfaces * sizeof( *bsp->surfaces ) );
	digest = HashBytes( digest, bsp->drawVerts, (size_t)bsp->numDrawVerts * sizeof( *bsp->drawVerts ) );
	digest = HashBytes( digest, bsp->drawIndexes, (size_t)bsp->numDrawIndexes * sizeof( *bsp->drawIndexes ) );
	digest = HashBytes( digest, bsp->shaders, (size_t)bsp->numShaders * sizeof( *bsp->shaders ) );
	contentIdentity = HashBytes( contentIdentity, bsp->name,
		strnlen( bsp->name, sizeof( bsp->name ) ) );
	contentIdentity = HashBytes( contentIdentity, &bsp->checksum,
		sizeof( bsp->checksum ) );
	contentIdentity = HashBytes( contentIdentity, bsp->surfaces,
		(size_t)bsp->numSurfaces * sizeof( *bsp->surfaces ) );
	contentIdentity = HashBytes( contentIdentity, bsp->drawVerts,
		(size_t)bsp->numDrawVerts * sizeof( *bsp->drawVerts ) );
	contentIdentity = HashBytes( contentIdentity, bsp->drawIndexes,
		(size_t)bsp->numDrawIndexes * sizeof( *bsp->drawIndexes ) );
	contentIdentity = HashBytes( contentIdentity, bsp->shaders,
		(size_t)bsp->numShaders * sizeof( *bsp->shaders ) );
	if ( !contentIdentity || contentIdentity == UINT64_MAX )
		contentIdentity = HashU32( FNV_OFFSET, (uint32_t)bsp->checksum );
	if ( !contentIdentity || contentIdentity == UINT64_MAX )
		contentIdentity = 1u;
	for ( int surfaceIndex = surfaceFirst; surfaceIndex < surfaceEnd; ++surfaceIndex ) {
		qboolean skip, patch;
		uint32_t addVertices, addIndices;
		if ( !RenderSubmission_WorldSurfaceSize( bsp, surfaceIndex, &skip, &patch, &addVertices, &addIndices ) )
			return qfalse;
		if ( skip )
			continue;
		if ( patch ) {
			patchBatchCount++;
			patchTriangleCount += addIndices / 3u;
		}
		if ( addVertices > RENDER_SUBMISSION_MAX_WORLD_VERTICES - vertexCount ||
			 addIndices > RENDER_SUBMISSION_MAX_WORLD_INDICES - indexCount ||
			 batchCount >= RENDER_SUBMISSION_MAX_WORLD_BATCHES )
			return qfalse;
		vertexCount += (uint32_t)addVertices;
		indexCount += (uint32_t)addIndices;
		batchCount++;
	}
	if ( vertexCount )
		vertices = (renderWorldVertex_t *)calloc( vertexCount, sizeof( *vertices ) );
	if ( indexCount )
		indices = (uint32_t *)calloc( indexCount, sizeof( *indices ) );
	if ( batchCount )
		batches = (renderWorldBatch_t *)calloc( batchCount, sizeof( *batches ) );
	if ( ( vertexCount && !vertices ) || ( indexCount && !indices ) || ( batchCount && !batches ) ) {
		free( batches );
		free( indices );
		free( vertices );
		return qfalse;
	}
	{
		uint32_t vertexCursor = 0u, indexCursor = 0u, batchCursor = 0u;
		for ( int surfaceIndex = surfaceFirst; surfaceIndex < surfaceEnd; ++surfaceIndex ) {
			const dsurface_t   *surface = &bsp->surfaces[surfaceIndex];
			renderWorldBatch_t *batch;
			if ( surface->surfaceType == MST_BAD || surface->surfaceType == MST_FLARE )
				continue;
			if ( bsp->shaders[surface->shaderNum].surfaceFlags & ( SURF_NODRAW | SURF_SKIP ) )
				continue;
			batch = &batches[batchCursor++];
			if ( !RenderSubmission_LowerWorldSurface( state, bsp, surfaceIndex, vertices, &vertexCursor, indices,
					&indexCursor, batch ) ) {
				free( batches );
				free( indices );
				free( vertices );
				return qfalse;
			}
		}
	}
	if ( batchCount > 1u )
		qsort( batches, batchCount, sizeof( *batches ), WorldBatchSortCompare );
	if ( bsp->numSurfaces > 0 ) {
		visibilityBytes = ( (uint32_t)bsp->numSurfaces + 7u ) / 8u;
		visibility = (byte *)calloc( visibilityBytes, 1u );
		if ( !visibility ) {
			free( batches ); free( indices ); free( vertices ); return qfalse;
		}
	}
	if ( state->activeWorldIndex < 0 ) {
		(void)RenderSubmission_ClearIrradianceVolumes( state );
		(void)RenderSubmission_ClearDirectionalLighting( state );
	}
	memset( state->emissiveRoutes, 0, sizeof( state->emissiveRoutes ) );
	memset( state->emissiveProxyLights, 0, sizeof( state->emissiveProxyLights ) );
	memset( &state->emissiveRouting, 0, sizeof( state->emissiveRouting ) );
	slot = &state->worlds[worldIndex];
	if ( state->activeWorldIndex == worldIndex )
		RenderSubmission_ClearActiveWorld( state );
	RenderSubmission_ReleaseWorldModels( state, worldIndex );
	RenderSubmission_FreeWorldSlot( slot );
	slot->map = bsp;
	slot->lifecycleGeneration = ++state->nextWorldLifecycleGeneration;
	slot->surfaceVisibility = visibility;
	slot->surfaceVisibilityBytes = visibilityBytes;
	slot->snapshot.vertices = vertices;
	slot->snapshot.indices = indices;
	slot->snapshot.batches = batches;
	slot->snapshot.vertexCount = vertexCount;
	slot->snapshot.indexCount = indexCount;
	slot->snapshot.batchCount = batchCount;
	slot->snapshot.patchBatchCount = patchBatchCount;
	slot->snapshot.patchTriangleCount = patchTriangleCount;
	slot->snapshot.ready = qtrue;
	slot->digest = digest;
	slot->contentIdentity = contentIdentity;
	slot->surfaceCount = (uint32_t)bsp->numSurfaces;
	slot->vertexCount = (uint32_t)bsp->numDrawVerts;
	slot->indexCount = (uint32_t)bsp->numDrawIndexes;
	slot->loaded = qtrue;
	if ( !RenderSubmission_SelectWorld( state, worldIndex ) ) {
		RenderSubmission_FreeWorldSlot( slot );
		return qfalse;
	}
	(void)RenderSubmission_ClearIrradianceVolumes( state );
	(void)RenderSubmission_ClearDirectionalLighting( state );
	RenderSubmission_ResetAtmosphereSurface( state );
	state->emissiveAuthorityDirty = RenderSubmission_RebuildEmissiveAuthority( state,
		state->nextMaterialGeneration ) ? qfalse : qtrue;
	return qtrue;
}

qboolean RenderSubmission_BeginFrame( renderSubmissionState_t *state, uint64_t frameGeneration )
{
	if ( !state || !state->initialized || state->frameOpen || frameGeneration == 0u || frameGeneration == UINT64_MAX )
		return qfalse;
	state->sceneDigest = HashU32( FNV_OFFSET, (uint32_t)frameGeneration );
	state->effectPrimitiveDigest = HashU32( FNV_OFFSET,
		(uint32_t)( frameGeneration ^ ( frameGeneration >> 32u ) ) );
	state->uiDigest	   = HashU32( FNV_OFFSET, (uint32_t)( frameGeneration >> 32u ) );
	state->currentFrameGeneration = frameGeneration;
	state->sealedFrameGeneration = 0u;
	state->entityCount = state->temporalEntityCount = state->localIrradianceEntityCount = 0u;
	state->characterSkinCount = 0u;
	state->polygonCount = state->lightCount = state->emissiveProxyFrameLightCount = 0u;
	state->effectSpriteCount = state->effectEmitterCount = 0u;
	state->effectDecalCount = state->effectRibbonCount = state->effectBeamCount = 0u;
	state->effectRibbonPointCount = state->effectPrimitiveDroppedCount = 0u;
	state->lensSourceCount = state->lensSourceDroppedCount = 0u;
	state->polyCommandCount = state->polyVertexCount = 0u;
	state->uiPrimitiveCount							 = 0u;
	state->uiTransformActive                         = qfalse;
	RenderSubmission_ClearAtmosphereEmitters( state );
	RenderSubmission_ClearAtmosphereSurfaceEvents( state );
	RenderSubmission_ClearAtmosphereMediaVolumes( state );
	state->sceneRendered = qfalse;
	state->frameSealed	 = qfalse;
	state->frameOpen	 = qtrue;
	return qtrue;
}

void RenderSubmission_CancelFrame( renderSubmissionState_t *state )
{
	if ( state && state->initialized ) {
		state->frameOpen   = qfalse;
		state->frameSealed = qfalse;
		state->currentFrameGeneration = 0u;
		state->sealedFrameGeneration = 0u;
	}
}

qboolean RenderSubmission_ClearScene( renderSubmissionState_t *state )
{
	/* Registration/loading code may clear the scene before the first BeginFrame.
	 * Treat that as an idempotent staging reset; actual submissions still require
	 * an open frame. */
	if ( !state || !state->initialized )
		return qfalse;
	state->sceneDigest = FNV_OFFSET;
	state->entityCount = state->temporalEntityCount = state->localIrradianceEntityCount = 0u;
	state->characterSkinCount = 0u;
	state->polygonCount = state->lightCount = state->emissiveProxyFrameLightCount = 0u;
	state->effectSpriteCount = state->effectEmitterCount = 0u;
	state->effectDecalCount = state->effectRibbonCount = state->effectBeamCount = 0u;
	state->effectRibbonPointCount = state->effectPrimitiveDroppedCount = 0u;
	state->effectPrimitiveDigest = FNV_OFFSET;
	state->lensSourceCount = state->lensSourceDroppedCount = 0u;
	state->lensSourceDigest = FNV_OFFSET;
	state->polyCommandCount = state->polyVertexCount = 0u;
	RenderSubmission_ClearAtmosphereEmitters( state );
	RenderSubmission_ClearAtmosphereSurfaceEvents( state );
	RenderSubmission_ClearAtmosphereMediaVolumes( state );
	state->sceneRendered = qfalse;
	return qtrue;
}

qboolean RenderSubmission_AddEntity( renderSubmissionState_t *state, const refEntity_t *entity,
									 const refEntityMotion_t *motion )
{
	return RenderSubmission_AddEntitySkinned( state, entity, motion, NULL );
}

qboolean RenderSubmission_AddEntitySkinned( renderSubmissionState_t *state,
		const refEntity_t *entity, const refEntityMotion_t *motion,
		const cmSkin_t *characterSkin )
{
	renderEntityCommand_t *command;
	if ( !state || !state->frameOpen || !entity || ( motion && !RefEntityMotion_IsValid( motion ) ) ||
		 entity->reType < RT_MODEL || entity->reType >= RT_MAX_REF_ENTITY_TYPE ||
		 state->entityCount >= RENDER_SUBMISSION_MAX_ENTITIES || !AddCount( &state->entityCount, 1u ) )
		return qfalse;
	for ( uint32_t axis = 0u; axis < 3u; ++axis ) {
		/* Match the established renderer ABI ingress: only the authoritative
		 * origin and entity type are universally initialized across every
		 * refEntity variant. Variant-specific fields are validated by the
		 * lowering path that consumes them, not while retaining the neutral
		 * command (portals/polystrips legitimately leave those fields unset).
		 * Legacy/Vulkan ingress rejects NaN, but deliberately does not invent a
		 * stricter infinity rule at this ABI boundary. Keep backend behaviour
		 * identical; a consuming lowering path may still reject unusable data. */
		if ( isnan( entity->origin[axis] ) ) {
			state->entityCount--;
			return qfalse;
		}
	}
	command = &state->entities[state->entityCount - 1u];
	memset( command, 0, sizeof( *command ) );
	command->entity = *entity;
	if ( characterSkin && entity->characterSkin ) {
		uint32_t index;
		for ( index = 0u; index < state->characterSkinCount; ++index )
			if ( state->characterSkins[index].handle == entity->characterSkin ) break;
		if ( index == state->characterSkinCount ) {
			if ( state->characterSkinCount >= RENDER_SUBMISSION_MAX_CHARACTER_SKINS ) {
				state->entityCount--;
				return qfalse;
			}
			state->characterSkins[index].handle = entity->characterSkin;
			state->characterSkins[index].skin = *characterSkin;
			state->characterSkinCount++;
		}
	}
	if ( motion ) {
		command->motion		 = *motion;
		command->hasTemporal = qtrue;
		state->temporalEntityCount++;
	}
	state->sceneDigest = HashBytes( state->sceneDigest, entity, sizeof( *entity ) );
	if ( motion )
		state->sceneDigest = HashBytes( state->sceneDigest, motion, sizeof( *motion ) );
	return qtrue;
}

static qboolean wired_attach_entity_lighting( renderSubmissionState_t *state,
		uint32_t entityIndex, const ralIrradianceEntitySampleReceipt_t *receipt,
		qboolean legacyLightGrid )
{
	renderEntityCommand_t *command;
	ralLightingCompositionRequest_t compositionRequest;
	ralLightingCompositionReceipt_t compositionReceipt;
	if ( !state || !state->frameOpen || entityIndex >= state->entityCount
			|| !Ral_IrradianceEntitySampleReceiptValid( receipt ) )
		return qfalse;
	command = &state->entities[entityIndex];
	if ( command->hasLocalIrradiance )
		return qfalse;
	memset( &compositionRequest, 0, sizeof( compositionRequest ) );
	compositionRequest.schemaVersion = RAL_LIGHTING_COMPOSITION_SCHEMA_VERSION;
	compositionRequest.frameGeneration = receipt->queryGeneration;
	compositionRequest.surfaceId = receipt->entityId;
	compositionRequest.diffuseAuthority = RAL_LIGHTING_DIFFUSE_AUTHORITY_LOCAL_SH;
	compositionRequest.availableTermMask = RAL_LIGHTING_TERM_LOCAL_SH;
	compositionRequest.localShBound = qtrue;
	if ( !Ral_LightingCompositionBuild( &compositionRequest, &compositionReceipt ) )
		return qfalse;
	command->localIrradiance = *receipt;
	command->lightingComposition = compositionReceipt;
	command->hasLocalIrradiance = qtrue;
	command->legacyLightGrid = legacyLightGrid;
	state->localIrradianceEntityCount++;
	state->sceneDigest = HashBytes( state->sceneDigest, receipt, sizeof( *receipt ) );
	state->sceneDigest = HashBytes( state->sceneDigest, &legacyLightGrid,
		sizeof( legacyLightGrid ) );
	return qtrue;
}

qboolean RenderSubmission_AttachEntityIrradiance( renderSubmissionState_t *state,
		uint32_t entityIndex, const ralIrradianceEntitySampleReceipt_t *receipt ) {
	return wired_attach_entity_lighting( state, entityIndex, receipt, qfalse );
}

qboolean wired_render_attach_legacy_grid_irradiance( renderSubmissionState_t *state,
		uint32_t entityIndex, const ralIrradianceEntitySampleReceipt_t *receipt ) {
	return wired_attach_entity_lighting( state, entityIndex, receipt, qtrue );
}

qboolean RenderSubmission_AddPoly( renderSubmissionState_t *state, qhandle_t material, int verticesPerPoly,
								   const polyVert_t *vertices, int polygonCount )
{
	renderPolyCommand_t *newCommands = NULL;
	polyVert_t			*newVertices = NULL;
	uint32_t			 newCommandCapacity, newVertexCapacity;
	size_t				 vertexCount;
	if ( !state || !state->frameOpen || material <= 0 || verticesPerPoly < 3 || polygonCount <= 0 || !vertices )
		return qfalse;
	vertexCount = (size_t)verticesPerPoly * (size_t)polygonCount;
	if ( vertexCount > UINT32_MAX || state->polyCommandCount >= RENDER_SUBMISSION_MAX_POLY_COMMANDS ||
		 vertexCount > RENDER_SUBMISSION_MAX_POLY_VERTICES - state->polyVertexCount ||
		 (uint32_t)polygonCount > UINT32_MAX - state->polygonCount )
		return qfalse;
	newCommandCapacity = state->polyCommandCapacity;
	if ( state->polyCommandCount == newCommandCapacity ) {
		newCommandCapacity = newCommandCapacity ? newCommandCapacity * 2u : 16u;
		if ( newCommandCapacity > RENDER_SUBMISSION_MAX_POLY_COMMANDS )
			newCommandCapacity = RENDER_SUBMISSION_MAX_POLY_COMMANDS;
		newCommands = (renderPolyCommand_t *)malloc( (size_t)newCommandCapacity * sizeof( *newCommands ) );
		if ( !newCommands )
			return qfalse;
		if ( state->polyCommandCount )
			memcpy( newCommands, state->polyCommands, (size_t)state->polyCommandCount * sizeof( *newCommands ) );
	}
	newVertexCapacity = state->polyVertexCapacity;
	if ( vertexCount > newVertexCapacity - state->polyVertexCount ) {
		newVertexCapacity = newVertexCapacity ? newVertexCapacity : 64u;
		while ( vertexCount > newVertexCapacity - state->polyVertexCount &&
				newVertexCapacity < RENDER_SUBMISSION_MAX_POLY_VERTICES ) {
			uint32_t doubled  = newVertexCapacity > RENDER_SUBMISSION_MAX_POLY_VERTICES / 2u
									? RENDER_SUBMISSION_MAX_POLY_VERTICES
									: newVertexCapacity * 2u;
			newVertexCapacity = doubled;
		}
		if ( vertexCount > newVertexCapacity - state->polyVertexCount ) {
			free( newCommands );
			return qfalse;
		}
		newVertices = (polyVert_t *)malloc( (size_t)newVertexCapacity * sizeof( *newVertices ) );
		if ( !newVertices ) {
			free( newCommands );
			return qfalse;
		}
		if ( state->polyVertexCount )
			memcpy( newVertices, state->polyVertices, (size_t)state->polyVertexCount * sizeof( *newVertices ) );
	}
	if ( newCommands ) {
		free( state->polyCommands );
		state->polyCommands		   = newCommands;
		state->polyCommandCapacity = newCommandCapacity;
	}
	if ( newVertices ) {
		free( state->polyVertices );
		state->polyVertices		  = newVertices;
		state->polyVertexCapacity = newVertexCapacity;
	}
	{
		renderPolyCommand_t *command = &state->polyCommands[state->polyCommandCount++];
		command->material			 = material;
		command->firstVertex		 = state->polyVertexCount;
		command->verticesPerPolygon	 = (uint32_t)verticesPerPoly;
		command->polygonCount		 = (uint32_t)polygonCount;
	}
	memcpy( state->polyVertices + state->polyVertexCount, vertices, vertexCount * sizeof( *vertices ) );
	state->polyVertexCount += (uint32_t)vertexCount;
	state->polygonCount += (uint32_t)polygonCount;
	state->sceneDigest = HashBytes( state->sceneDigest, &material, sizeof( material ) );
	state->sceneDigest = HashBytes( state->sceneDigest, vertices, vertexCount * sizeof( *vertices ) );
	return qtrue;
}

qboolean RenderSubmission_AddLight( renderSubmissionState_t *state, const vec3_t origin, const vec3_t end,
									float intensity, float red, float green, float blue )
{
	renderLightCommand_t *newLights = NULL;
	uint32_t			  newCapacity;
	if ( !state || !state->frameOpen || !origin || state->lightCount >= RENDER_SUBMISSION_MAX_LIGHTS )
		return qfalse;
	newCapacity = state->lightCapacity;
	if ( state->lightCount == newCapacity ) {
		newCapacity = newCapacity ? newCapacity * 2u : 16u;
		if ( newCapacity > RENDER_SUBMISSION_MAX_LIGHTS )
			newCapacity = RENDER_SUBMISSION_MAX_LIGHTS;
		newLights = (renderLightCommand_t *)malloc( (size_t)newCapacity * sizeof( *newLights ) );
		if ( !newLights )
			return qfalse;
		if ( state->lightCount )
			memcpy( newLights, state->lights, (size_t)state->lightCount * sizeof( *newLights ) );
		free( state->lights );
		state->lights		 = newLights;
		state->lightCapacity = newCapacity;
	}
	{
		renderLightCommand_t *command = &state->lights[state->lightCount];
		memset( command, 0, sizeof( *command ) );
		memcpy( command->origin, origin, sizeof( command->origin ) );
		if ( end ) {
			memcpy( command->end, end, sizeof( command->end ) );
			command->hasEnd = qtrue;
		}
		command->intensity = intensity;
		command->color[0]  = red;
		command->color[1]  = green;
		command->color[2]  = blue;
		command->sourceFlags = RAL_LIGHT_CONTRIBUTE_DIRECT | RAL_LIGHT_INJECT_ATMOSPHERE;
	}
	state->lightCount++;
	state->sceneDigest = HashBytes( state->sceneDigest, origin, sizeof( vec3_t ) );
	if ( end )
		state->sceneDigest = HashBytes( state->sceneDigest, end, sizeof( vec3_t ) );
	state->sceneDigest = HashBytes( state->sceneDigest, &intensity, sizeof( intensity ) );
	state->sceneDigest = HashBytes( state->sceneDigest, &red, sizeof( red ) );
	state->sceneDigest = HashBytes( state->sceneDigest, &green, sizeof( green ) );
	state->sceneDigest = HashBytes( state->sceneDigest, &blue, sizeof( blue ) );
	return qtrue;
}

static qboolean AppendEmissiveProxyLights( renderSubmissionState_t *state )
{
	if ( !state || !state->frameOpen ) return qfalse;
	if ( state->emissiveProxyFrameLightCount ) return qtrue;
	if ( !RenderSubmission_EmissiveRoutingReceiptValid( &state->emissiveRouting ) )
		return qtrue;
	for ( uint32_t index = 0u; index < state->emissiveRouting.proxyLightCount; ++index ) {
		const ralLightDescription_t *proxy = &state->emissiveProxyLights[index];
		vec3_t origin;
		uint64_t authorityHash = 0u;
		if ( !( proxy->flags & RAL_LIGHT_CONTRIBUTE_DIRECT ) ) return qfalse;
		for ( uint32_t axis = 0u; axis < 3u; ++axis )
			origin[axis] = (float)( ( axis == 0u ? proxy->position.x :
				( axis == 1u ? proxy->position.y : proxy->position.z ) ) ) /
				(float)RAL_LIGHT_Q16_ONE;
		for ( uint32_t route = 0u; route < state->emissiveRouting.routeCount; ++route ) {
			const ralEmissiveRouteReceipt_t *candidate = &state->emissiveRoutes[route];
			if ( candidate->sourceGeneration == proxy->sourceGeneration &&
				candidate->provenanceHash == proxy->provenanceHash &&
				proxy->position.x >= candidate->boundsMin.x &&
				proxy->position.x <= candidate->boundsMax.x &&
				proxy->position.y >= candidate->boundsMin.y &&
				proxy->position.y <= candidate->boundsMax.y &&
				proxy->position.z >= candidate->boundsMin.z &&
				proxy->position.z <= candidate->boundsMax.z ) {
				authorityHash = candidate->radianceAuthorityHash;
				break;
			}
		}
		if ( !authorityHash || !RenderSubmission_AddLight( state, origin, NULL,
			(float)proxy->rangeQ16 / RAL_LIGHT_Q16_ONE,
			(float)proxy->radianceQ16[0] / RAL_LIGHT_Q16_ONE,
			(float)proxy->radianceQ16[1] / RAL_LIGHT_Q16_ONE,
			(float)proxy->radianceQ16[2] / RAL_LIGHT_Q16_ONE ) ) return qfalse;
		state->lights[state->lightCount - 1u].emissiveAuthorityHash = authorityHash;
		state->lights[state->lightCount - 1u].sourceFlags = proxy->flags;
		state->lights[state->lightCount - 1u].shadowPriority = proxy->shadowPriority;
		state->lights[state->lightCount - 1u].authoredEmissive = qtrue;
		state->sceneDigest = HashBytes( state->sceneDigest, &authorityHash,
			sizeof( authorityHash ) );
	}
	state->emissiveProxyFrameLightCount = state->emissiveRouting.proxyLightCount;
	return qtrue;
}

qboolean RenderSubmission_RenderScene( renderSubmissionState_t *state, const refdef_t *view, int worldIndex )
{
	static const ralLightVec3Q16_t referenceNormal = { 0, 0, RAL_LIGHT_Q16_ONE };
	static const int32_t fallbackIrradiance[3] = {
		RAL_LIGHT_Q16_ONE, RAL_LIGHT_Q16_ONE, RAL_LIGHT_Q16_ONE
	};
	if ( !state || !state->frameOpen || !view || worldIndex < 0
			|| worldIndex >= MAX_RENDER_WORLDS )
		return qfalse;
	if ( RenderSubmission_WorldResident( state, worldIndex ) ) {
		if ( !RenderSubmission_SelectWorld( state, worldIndex ) ) return qfalse;
	}
	if ( !( view->rdflags & RDF_NOWORLDMODEL ) && state->worldLoaded
			&& !RenderSubmission_UpdateWorldVisibilityForView(
				state, state->activeWorldIndex, view ) )
		return qfalse;
	if ( !AppendEmissiveProxyLights( state ) ) return qfalse;
	if ( state->irradianceVolumeCount ) {
		ralIrradianceVolumePlacement_t placements[RENDER_SUBMISSION_MAX_IRRADIANCE_VOLUMES];
		uint32_t entityIndex, volumeIndex;
		for ( volumeIndex = 0u; volumeIndex < state->irradianceVolumeCount; ++volumeIndex )
			placements[volumeIndex] = state->irradianceVolumes[volumeIndex].placement;
		for ( entityIndex = 0u; entityIndex < state->entityCount; ++entityIndex ) {
			ralLightVec3Q16_t position;
			ralIrradianceVolumeSelectionReceipt_t selection;
			renderEntityCommand_t *entity = &state->entities[entityIndex];
			if ( entity->entity.reType != RT_MODEL || entity->hasLocalIrradiance )
				continue;
			if ( !PositionQ16( entity->entity.origin[0], &position.x ) ||
				 !PositionQ16( entity->entity.origin[1], &position.y ) ||
				 !PositionQ16( entity->entity.origin[2], &position.z ) )
				continue;
			if ( !Ral_IrradianceVolumeSelect( placements, state->irradianceVolumeCount,
				&position, &selection ) )
				continue;
			/* A per-entity sample is optional enrichment.  The sidecar itself was
			 * validated atomically at registration; if an individual model cannot
			 * be sampled, retain its deterministic lightgrid/global fallback rather
			 * than poisoning the complete renderer frame. */
			if ( !RenderSubmission_AttachConfiguredEntityIrradiance( state, entityIndex,
				&referenceNormal, fallbackIrradiance ) )
				continue;
		}
	}
	state->sceneDigest = HashBytes( state->sceneDigest, view, sizeof( *view ) );
	state->sceneDigest = HashBytes( state->sceneDigest, &worldIndex, sizeof( worldIndex ) );
	memcpy( state->worldSnapshot.viewOrigin, view->vieworg, sizeof( state->worldSnapshot.viewOrigin ) );
	memcpy( state->worldSnapshot.viewAxis, view->viewaxis, sizeof( state->worldSnapshot.viewAxis ) );
	state->worldSnapshot.fovX = view->fov_x;
	state->worldSnapshot.fovY = view->fov_y;
	state->worldSnapshot.viewportX = view->x;
	state->worldSnapshot.viewportY = view->y;
	state->worldSnapshot.viewportWidth = view->width > 0
		? (uint32_t)view->width : 0u;
	state->worldSnapshot.viewportHeight = view->height > 0
		? (uint32_t)view->height : 0u;
	state->worldSnapshot.uiPrimitiveInsertionIndex = state->uiPrimitiveCount;
	state->worldSnapshot.rdflags = (uint32_t)view->rdflags;
	state->worldSnapshot.timeMs = view->time;
	state->sceneRendered	  = qtrue;
	RenderSubmission_CommitActiveWorld( state );
	return qtrue;
}

qboolean RenderSubmission_SetColor( renderSubmissionState_t *state, const float *rgba )
{
	static const float white[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	if ( !state || !state->initialized )
		return qfalse;
	memcpy( state->color, rgba ? rgba : white, sizeof( state->color ) );
	return qtrue;
}

qboolean RenderSubmission_SetUiTransform( renderSubmissionState_t *state,
		const refUiTransform_t *transform )
{
	if ( !state || !state->initialized ) return qfalse;
	if ( !transform ) {
		memset( &state->uiTransform, 0, sizeof( state->uiTransform ) );
		state->uiTransformActive = qfalse;
		return qtrue;
	}
	if ( transform->schemaVersion != REF_UI_TRANSFORM_SCHEMA_VERSION
			|| !isfinite( transform->x ) || !isfinite( transform->y )
			|| !isfinite( transform->width ) || transform->width <= 0.0f
			|| !isfinite( transform->height ) || transform->height <= 0.0f
			|| !isfinite( transform->perspective )
			|| fabsf( transform->perspective ) > 0.35f ) return qfalse;
	state->uiTransform = *transform;
	state->uiTransformActive = qtrue;
	return qtrue;
}

static void RenderSubmission_TransformUiPoint( const renderSubmissionState_t *state,
		float *x, float *y )
{
	if ( !state || !state->uiTransformActive || !x || !y ) return;
	RenderUi_ProjectPoint( &state->uiTransform, x, y );
}

qboolean RenderSubmission_AddUiQuad( renderSubmissionState_t *state, float x, float y, float width, float height,
									 float s1, float t1, float s2, float t2, float rotation, qhandle_t material )
{
	renderUiPrimitive_t *primitive;
	float				 values[9] = { x, y, width, height, s1, t1, s2, t2, rotation };
	if ( !state || !state->frameOpen || !state->uiPrimitives || material <= 0 ||
		 state->uiPrimitiveCount >= RENDER_SUBMISSION_MAX_UI_PRIMITIVES || !AddCount( &state->uiPrimitiveCount, 1u ) )
		return qfalse;
	primitive = &state->uiPrimitives[state->uiPrimitiveCount - 1u];
	memset( primitive, 0, sizeof( *primitive ) );
	primitive->kind		= RENDER_UI_QUAD;
	primitive->x		= x;
	primitive->y		= y;
	primitive->width	= width;
	primitive->height	= height;
	primitive->s1		= s1;
	primitive->t1		= t1;
	primitive->s2		= s2;
	primitive->t2		= t2;
	primitive->rotation = rotation;
	{
		float centerX = x + width * 0.5f, centerY = y + height * 0.5f;
		float radians = rotation * 0.01745329251994329577f;
		float cosine = cosf( radians ), sine = sinf( radians );
		static const float signs[4][2] = {
			{ -1.0f, -1.0f }, { 1.0f, -1.0f },
			{ 1.0f, 1.0f }, { -1.0f, 1.0f }
		};
		for ( uint32_t corner = 0u; corner < 4u; ++corner ) {
			float localX = signs[corner][0] * width * 0.5f;
			float localY = signs[corner][1] * height * 0.5f;
			primitive->positions[corner][0] = centerX + localX * cosine - localY * sine;
			primitive->positions[corner][1] = centerY + localX * sine + localY * cosine;
			RenderSubmission_TransformUiPoint( state,
				&primitive->positions[corner][0], &primitive->positions[corner][1] );
		}
	}
	memcpy( primitive->color, state->color, sizeof( primitive->color ) );
	primitive->material = material;
	state->uiDigest		= HashBytes( state->uiDigest, values, sizeof( values ) );
	state->uiDigest		= HashBytes( state->uiDigest, primitive->positions,
						 sizeof( primitive->positions ) );
	state->uiDigest		= HashBytes( state->uiDigest, state->color, sizeof( state->color ) );
	state->uiDigest		= HashBytes( state->uiDigest, &material, sizeof( material ) );
	return qtrue;
}

qboolean RenderSubmission_AddUiLine( renderSubmissionState_t *state, float x1, float y1, float x2, float y2,
									 float width, qhandle_t material )
{
	qboolean added = RenderSubmission_AddUiQuad( state, x1, y1, x2, y2, width, 0.0f, 0.0f, 0.0f, 0.0f, material );
	if ( added ) {
		renderUiPrimitive_t *primitive = &state->uiPrimitives[state->uiPrimitiveCount - 1u];
		float dx = x2 - x1, dy = y2 - y1;
		float length = sqrtf( dx * dx + dy * dy );
		if ( length <= 0.0f || !isfinite( length ) ) return qfalse;
		float nx = -dy / length * width * 0.5f;
		float ny = dx / length * width * 0.5f;
		primitive->kind = RENDER_UI_LINE;
		primitive->positions[0][0] = x1 + nx; primitive->positions[0][1] = y1 + ny;
		primitive->positions[1][0] = x2 + nx; primitive->positions[1][1] = y2 + ny;
		primitive->positions[2][0] = x2 - nx; primitive->positions[2][1] = y2 - ny;
		primitive->positions[3][0] = x1 - nx; primitive->positions[3][1] = y1 - ny;
		for ( uint32_t corner = 0u; corner < 4u; ++corner )
			RenderSubmission_TransformUiPoint( state,
				&primitive->positions[corner][0], &primitive->positions[corner][1] );
		state->uiDigest = HashBytes( state->uiDigest, primitive->positions,
							 sizeof( primitive->positions ) );
	}
	return added;
}

uint64_t RenderSubmission_FrameDigest( const renderSubmissionState_t *state )
{
	uint64_t digest;
	if ( !state || !state->initialized || ( !state->frameOpen && !state->frameSealed ) )
		return 0u;
	digest = HashBytes( FNV_OFFSET, &state->worldDigest, sizeof( state->worldDigest ) );
	digest = HashBytes( digest, &state->assetDigest, sizeof( state->assetDigest ) );
	digest = HashBytes( digest, &state->materialDigest, sizeof( state->materialDigest ) );
	digest = HashBytes( digest, &state->modelDigest, sizeof( state->modelDigest ) );
	digest = HashBytes( digest, &state->irradianceVolumeDigest, sizeof( state->irradianceVolumeDigest ) );
	digest = HashBytes( digest, &state->irradianceVolumeCount, sizeof( state->irradianceVolumeCount ) );
	digest = HashBytes( digest, &state->directionalLightingDigest, sizeof( state->directionalLightingDigest ) );
	digest = HashBytes( digest, &state->directionalLightingCount, sizeof( state->directionalLightingCount ) );
	digest = HashBytes( digest, &state->sceneDigest, sizeof( state->sceneDigest ) );
	digest = HashBytes( digest, &state->effectPrimitiveDigest,
		sizeof( state->effectPrimitiveDigest ) );
	digest = HashBytes( digest, &state->effectSpriteCount,
		sizeof( state->effectSpriteCount ) );
	digest = HashBytes( digest, &state->effectEmitterCount,
		sizeof( state->effectEmitterCount ) );
	digest = HashBytes( digest, &state->effectDecalCount,
		sizeof( state->effectDecalCount ) );
	digest = HashBytes( digest, &state->effectRibbonCount,
		sizeof( state->effectRibbonCount ) );
	digest = HashBytes( digest, &state->effectRibbonPointCount,
		sizeof( state->effectRibbonPointCount ) );
	digest = HashBytes( digest, &state->effectBeamCount,
		sizeof( state->effectBeamCount ) );
	digest = HashBytes( digest, &state->effectPrimitiveDroppedCount,
		sizeof( state->effectPrimitiveDroppedCount ) );
	digest = HashBytes( digest, &state->lensSourceDigest,
		sizeof( state->lensSourceDigest ) );
	digest = HashBytes( digest, &state->lensSourceCount,
		sizeof( state->lensSourceCount ) );
	digest = HashBytes( digest, &state->lensSourceDroppedCount,
		sizeof( state->lensSourceDroppedCount ) );
	digest = HashBytes( digest, &state->uiDigest, sizeof( state->uiDigest ) );
	{
		uint64_t atmosphereDigest = RenderSubmission_AtmosphereDigest( state );
		digest					  = HashBytes( digest, &atmosphereDigest, sizeof( atmosphereDigest ) );
	}
	digest = HashBytes( digest, &state->atmosphereGeneration, sizeof( state->atmosphereGeneration ) );
	digest = HashBytes( digest, &state->atmosphereEmitterCount, sizeof( state->atmosphereEmitterCount ) );
	digest = HashBytes( digest, &state->atmosphereProfileGeneration, sizeof( state->atmosphereProfileGeneration ) );
	digest = HashBytes( digest, &state->atmosphereProfileCount, sizeof( state->atmosphereProfileCount ) );
	digest = HashBytes( digest, &state->particleClassDigest, sizeof( state->particleClassDigest ) );
	digest = HashBytes( digest, &state->particleClassGeneration, sizeof( state->particleClassGeneration ) );
	digest = HashBytes( digest, &state->particleClassCount, sizeof( state->particleClassCount ) );
	digest = HashBytes( digest, &state->surfaceClimateDigest, sizeof( state->surfaceClimateDigest ) );
	digest = HashBytes( digest, &state->surfaceClimateGeneration, sizeof( state->surfaceClimateGeneration ) );
	digest = HashBytes( digest, &state->surfaceClimateTileCount, sizeof( state->surfaceClimateTileCount ) );
	digest = HashBytes( digest, &state->atmosphereSurfaceEventCount, sizeof( state->atmosphereSurfaceEventCount ) );
	digest = HashBytes( digest, &state->atmosphereMediaDigest, sizeof( state->atmosphereMediaDigest ) );
	digest = HashBytes( digest, &state->atmosphereMediaVolumeCount, sizeof( state->atmosphereMediaVolumeCount ) );
	digest = HashBytes( digest, &state->entityCount, sizeof( state->entityCount ) );
	digest = HashBytes( digest, &state->uiPrimitiveCount, sizeof( state->uiPrimitiveCount ) );
	return digest;
}

qboolean RenderSubmission_EndFrame( renderSubmissionState_t *state, uint64_t frameGeneration,
									renderSubmissionReceipt_t *outReceipt )
{
	renderSubmissionReceipt_t receipt;
	if ( !state || !state->frameOpen || !outReceipt || frameGeneration == 0u || frameGeneration == UINT64_MAX ||
		 frameGeneration != state->currentFrameGeneration )
		return qfalse;
	memset( &receipt, 0, sizeof( receipt ) );
	receipt.schemaVersion				= RENDER_SUBMISSION_SCHEMA_VERSION;
	receipt.ownerGeneration				= state->ownerGeneration;
	receipt.frameGeneration				= frameGeneration;
	receipt.assetDigest					= state->assetDigest;
	receipt.materialDigest				= state->materialDigest;
	receipt.modelDigest					= state->modelDigest;
	receipt.worldDigest					= state->worldDigest;
	receipt.sceneDigest					= state->sceneDigest;
	receipt.uiDigest					= state->uiDigest;
	receipt.atmosphereDigest			= RenderSubmission_AtmosphereDigest( state );
	receipt.atmosphereGeneration		= state->atmosphereGeneration;
	receipt.atmosphereProfileDigest		= state->atmosphereProfileDigest;
	receipt.atmosphereProfileGeneration = state->atmosphereProfileGeneration;
	receipt.particleClassDigest			= state->particleClassDigest;
	receipt.particleClassGeneration		= state->particleClassGeneration;
	receipt.surfaceClimateDigest		= state->surfaceClimateDigest;
	receipt.surfaceClimateGeneration	= state->surfaceClimateGeneration;
	receipt.atmosphereMediaDigest		= state->atmosphereMediaDigest;
	receipt.irradianceVolumeDigest		= state->irradianceVolumeDigest;
	receipt.directionalLightingDigest	= state->directionalLightingDigest;
	receipt.frameDigest					= RenderSubmission_FrameDigest( state );
	receipt.effectPrimitiveDigest     = state->effectPrimitiveDigest;
	receipt.worldSurfaceCount			= state->worldSurfaceCount;
	receipt.worldVertexCount			= state->worldVertexCount;
	receipt.worldIndexCount				= state->worldIndexCount;
	receipt.registeredAssetCount		= state->registeredAssetCount;
	receipt.registeredMaterialCount		= state->registeredMaterialCount;
	receipt.resolvedMaterialCount		= state->resolvedMaterialCount;
	receipt.materialBytes				= state->materialBytes;
	receipt.stageTextureCount			= state->stageTextureCount;
	receipt.stageTextureBytes			= state->stageTextureBytes;
	receipt.registeredModelCount		= state->modelCount;
	receipt.modelBytes					= state->modelBytes;
	receipt.entityCount					= state->entityCount;
	receipt.temporalEntityCount			= state->temporalEntityCount;
	receipt.localIrradianceEntityCount = state->localIrradianceEntityCount;
	receipt.irradianceVolumeCount		= state->irradianceVolumeCount;
	receipt.directionalLightingCount	= state->directionalLightingCount;
	receipt.polygonCount				= state->polygonCount;
	receipt.lightCount					= state->lightCount;
	receipt.effectSpriteCount          = state->effectSpriteCount;
	receipt.effectEmitterCount         = state->effectEmitterCount;
	receipt.effectDecalCount           = state->effectDecalCount;
	receipt.effectRibbonCount          = state->effectRibbonCount;
	receipt.effectRibbonPointCount     = state->effectRibbonPointCount;
	receipt.effectBeamCount            = state->effectBeamCount;
	receipt.effectPrimitiveDroppedCount = state->effectPrimitiveDroppedCount;
	receipt.lensSourceCount             = state->lensSourceCount;
	receipt.lensSourceDroppedCount      = state->lensSourceDroppedCount;
	receipt.atmosphereEmitterCount		= state->atmosphereEmitterCount;
	receipt.atmosphereProfileCount		= state->atmosphereProfileCount;
	receipt.particleClassCount			= state->particleClassCount;
	receipt.surfaceClimateTileCount		= state->surfaceClimateTileCount;
	receipt.atmosphereSurfaceEventCount = state->atmosphereSurfaceEventCount;
	receipt.surfaceClimateEvictionCount = state->surfaceClimateEvictionCount;
	receipt.atmosphereMediaVolumeCount	= state->atmosphereMediaVolumeCount;
	receipt.atmosphereMediaDroppedCount = state->atmosphereMediaDroppedCount;
	receipt.uiPrimitiveCount			= state->uiPrimitiveCount;
	receipt.worldLoaded					= state->worldLoaded;
	receipt.sceneRendered				= state->sceneRendered;
	receipt.atmosphereActive =
		( state->atmosphereEmitterCount > 0u || ( state->atmosphere.flags & ATMOSPHERE_FLAG_ENABLED ) != 0u ) ? qtrue
																											  : qfalse;
	receipt.ready = qtrue;
	if ( !ReceiptValid( &receipt ) )
		return qfalse;
	state->frameOpen   = qfalse;
	state->frameSealed = qtrue;
	state->sealedFrameGeneration = frameGeneration;
	state->currentFrameGeneration = 0u;
	if ( state->emissiveAuthorityDirty &&
		 RenderSubmission_RebuildEmissiveAuthority( state, state->nextMaterialGeneration ) )
		state->emissiveAuthorityDirty = qfalse;
	*outReceipt		   = receipt;
	return qtrue;
}

qboolean RenderSubmission_ReceiptExact( const renderSubmissionReceipt_t *a, const renderSubmissionReceipt_t *b )
{
	return ( ReceiptValid( a ) && ReceiptValid( b ) && !memcmp( a, b, sizeof( *a ) ) ) ? qtrue : qfalse;
}
