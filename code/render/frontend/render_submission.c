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

#define FNV_OFFSET UINT64_C(1469598103934665603)
#define FNV_PRIME UINT64_C(1099511628211)

static uint64_t HashBytes( uint64_t digest, const void *data, size_t size ) {
	const byte *bytes = (const byte *)data;
	size_t i;
	for ( i = 0u; i < size; ++i ) {
		digest ^= bytes[i];
		digest *= FNV_PRIME;
	}
	return digest;
}

static uint64_t HashU32( uint64_t digest, uint32_t value ) {
	return HashBytes( digest, &value, sizeof( value ) );
}

static uint64_t MaterialSnapshotDigest( const char *name,
		const renderMaterialSnapshot_t *snapshot ) {
	uint64_t digest = HashBytes( FNV_OFFSET, name, strlen( name ) + 1u );
	digest = HashBytes( digest, &snapshot->width, sizeof( snapshot->width ) );
	digest = HashBytes( digest, &snapshot->height, sizeof( snapshot->height ) );
	digest = HashBytes( digest, &snapshot->clampToEdge,
		sizeof( snapshot->clampToEdge ) );
	digest = HashBytes( digest, &snapshot->msdf, sizeof( snapshot->msdf ) );
	digest = HashBytes( digest, &snapshot->srgb, sizeof( snapshot->srgb ) );
	digest = HashBytes( digest, &snapshot->alphaMode,
		sizeof( snapshot->alphaMode ) );
	digest = HashBytes( digest, &snapshot->alphaCutoff,
		sizeof( snapshot->alphaCutoff ) );
	digest = HashBytes( digest, &snapshot->depthWrite,
		sizeof( snapshot->depthWrite ) );
	digest = HashBytes( digest, &snapshot->ready, sizeof( snapshot->ready ) );
	if ( snapshot->rgba8 && snapshot->byteCount )
		digest = HashBytes( digest, snapshot->rgba8, snapshot->byteCount );
	return digest;
}

static void RebuildMaterialDigest( renderSubmissionState_t *state ) {
	uint32_t i;
	state->materialDigest = FNV_OFFSET;
	state->modelDigest = FNV_OFFSET;
	for ( i = 0u; i < state->materialCount; ++i ) {
		const renderMaterialSnapshot_t *snapshot = &state->materials[i].snapshot;
		state->materialDigest = HashBytes( state->materialDigest,
			&snapshot->handle, sizeof( snapshot->handle ) );
		state->materialDigest = HashBytes( state->materialDigest,
			&snapshot->generation, sizeof( snapshot->generation ) );
		state->materialDigest = HashBytes( state->materialDigest,
			&snapshot->digest, sizeof( snapshot->digest ) );
	}
}

static qboolean KindValid( renderAssetKind_t kind ) {
	return kind >= RENDER_ASSET_MODEL && kind <= RENDER_ASSET_LIGHTMAP;
}

static qboolean MaterialKind( renderAssetKind_t kind ) {
	return ( kind == RENDER_ASSET_MATERIAL || kind == RENDER_ASSET_MSDF
		|| kind == RENDER_ASSET_PRIMITIVE_MATERIAL
		|| kind == RENDER_ASSET_LIGHTMAP ) ? qtrue : qfalse;
}

static qboolean AddCount( uint32_t *count, uint32_t amount ) {
	if ( !count || amount > UINT32_MAX - *count ) return qfalse;
	*count += amount;
	return qtrue;
}

static void DefaultMaterialRasterPolicy( renderAssetKind_t kind,
		const byte *rgba8, uint32_t byteCount,
		renderMaterialSnapshot_t *snapshot ) {
	snapshot->alphaMode = RENDER_ALPHA_OPAQUE;
	snapshot->alphaCutoff = 0.5f;
	snapshot->depthWrite = qtrue;
	if ( kind == RENDER_ASSET_MATERIAL && rgba8 ) {
		qboolean masked = qfalse;
		for ( uint32_t offset = 3u; offset < byteCount; offset += 4u ) {
			if ( rgba8[offset] > 0u && rgba8[offset] < 255u ) {
				snapshot->alphaMode = RENDER_ALPHA_BLEND;
				snapshot->depthWrite = qfalse;
				return;
			}
			if ( rgba8[offset] == 0u ) masked = qtrue;
		}
		if ( masked ) snapshot->alphaMode = RENDER_ALPHA_MASK;
	}
}

static qboolean ReceiptValid( const renderSubmissionReceipt_t *receipt ) {
	return ( receipt && receipt->schemaVersion == RENDER_SUBMISSION_SCHEMA_VERSION
		&& receipt->ownerGeneration != 0u && receipt->ownerGeneration != UINT64_MAX
		&& receipt->frameGeneration != 0u && receipt->frameGeneration != UINT64_MAX
		&& receipt->materialDigest != 0u && receipt->sceneDigest != 0u
		&& receipt->uiDigest != 0u
		&& receipt->frameDigest != 0u
		&& ( receipt->worldLoaded == qfalse || receipt->worldDigest != 0u )
		&& receipt->ready == qtrue ) ? qtrue : qfalse;
}

qboolean RenderSubmission_Init( renderSubmissionState_t *state,
		uint64_t ownerGeneration ) {
	if ( !state || ownerGeneration == 0u || ownerGeneration == UINT64_MAX ) return qfalse;
	memset( state, 0, sizeof( *state ) );
	state->ownerGeneration = ownerGeneration;
	state->assetDigest = FNV_OFFSET;
	state->materialDigest = FNV_OFFSET;
	state->sceneDigest = FNV_OFFSET;
	state->uiDigest = FNV_OFFSET;
	state->nextHandle = 1u;
	state->nextMaterialGeneration = 1u;
	state->nextModelGeneration = 1u;
	state->color[0] = state->color[1] = state->color[2] = state->color[3] = 1.0f;
	state->initialized = qtrue;
	return qtrue;
}

void RenderSubmission_Reset( renderSubmissionState_t *state ) {
	if ( state ) {
		uint32_t i;
		for ( i = 0u; i < state->materialCount; ++i )
			free( (void *)state->materials[i].snapshot.rgba8 );
		for ( i = 0u; i < state->modelCount; ++i ) {
			free( (void *)state->models[i].snapshot.positions );
			free( (void *)state->models[i].snapshot.texCoords );
			free( (void *)state->models[i].snapshot.indices );
			free( (void *)state->models[i].snapshot.batches );
		}
		free( (void *)state->worldSnapshot.vertices );
		free( (void *)state->worldSnapshot.indices );
		free( (void *)state->worldSnapshot.batches );
		free( state->polyCommands );
		free( state->polyVertices );
		free( state->lights );
		memset( state, 0, sizeof( *state ) );
	}
}

qhandle_t RenderSubmission_RegisterAsset( renderSubmissionState_t *state,
		renderAssetKind_t kind, const char *name ) {
	if ( MaterialKind( kind ) ) return RenderSubmission_RegisterMaterialImage(
		state, kind, name, qfalse, NULL, 0u, 0u );
	if ( !state || !state->initialized || !KindValid( kind ) || !name || !name[0]
			|| state->nextHandle >= INT_MAX ) return 0;
	if ( !RenderSubmission_RecordAsset( state, kind, name,
			(qhandle_t)state->nextHandle ) ) return 0;
	return (qhandle_t)state->nextHandle++;
}

qhandle_t RenderSubmission_RegisterMaterialImage( renderSubmissionState_t *state,
		renderAssetKind_t kind, const char *name, qboolean clampToEdge,
		const byte *rgba8, uint32_t width, uint32_t height ) {
	renderMaterialRecord_t *record;
	byte *owned = NULL;
	uint64_t byteCount64;
	uint32_t byteCount = 0u, i;
	qhandle_t handle;
	if ( !state || !state->initialized || !MaterialKind( kind ) || !name || !name[0]
			|| strlen( name ) >= MAX_QPATH
			|| ( clampToEdge != qfalse && clampToEdge != qtrue ) ) return 0;
	for ( i = 0u; i < state->materialCount; ++i ) {
		if ( !strcasecmp( state->materials[i].name, name ) ) {
			renderMaterialSnapshot_t *snapshot = &state->materials[i].snapshot;
			if ( !rgba8 || snapshot->ready ) return snapshot->handle;
			if ( !width || !height || width > RENDER_SUBMISSION_MAX_IMAGE_DIMENSION
					|| height > RENDER_SUBMISSION_MAX_IMAGE_DIMENSION
					|| state->nextMaterialGeneration == UINT64_MAX ) return 0;
			byteCount64 = (uint64_t)width * (uint64_t)height * 4u;
			if ( byteCount64 > UINT32_MAX
					|| byteCount64 > RENDER_SUBMISSION_MAX_MATERIAL_BYTES
						- state->materialBytes ) return 0;
			byteCount = (uint32_t)byteCount64;
			owned = (byte *)malloc( byteCount );
			if ( !owned ) return 0;
			memcpy( owned, rgba8, byteCount );
			snapshot->generation = state->nextMaterialGeneration++;
			snapshot->width = width; snapshot->height = height;
			snapshot->rowBytes = width * 4u; snapshot->byteCount = byteCount;
			snapshot->rgba8 = owned; snapshot->clampToEdge = clampToEdge;
			snapshot->msdf = kind == RENDER_ASSET_MSDF ? qtrue : qfalse;
			snapshot->srgb = ( snapshot->msdf || kind == RENDER_ASSET_LIGHTMAP )
				? qfalse : qtrue;
			DefaultMaterialRasterPolicy( kind, owned, byteCount, snapshot );
			snapshot->ready = qtrue;
			snapshot->digest = MaterialSnapshotDigest(
				state->materials[i].name, snapshot );
			state->resolvedMaterialCount++;
			state->materialBytes += byteCount;
			RebuildMaterialDigest( state );
			return snapshot->handle;
		}
	}
	if ( state->materialCount >= RENDER_SUBMISSION_MAX_MATERIALS
			|| state->nextHandle >= INT_MAX
			|| state->nextMaterialGeneration == UINT64_MAX ) return 0;
	if ( rgba8 ) {
		if ( !width || !height || width > RENDER_SUBMISSION_MAX_IMAGE_DIMENSION
				|| height > RENDER_SUBMISSION_MAX_IMAGE_DIMENSION ) return 0;
		byteCount64 = (uint64_t)width * (uint64_t)height * 4u;
		if ( byteCount64 > UINT32_MAX
				|| byteCount64 > RENDER_SUBMISSION_MAX_MATERIAL_BYTES
					- state->materialBytes ) return 0;
		byteCount = (uint32_t)byteCount64;
		owned = (byte *)malloc( byteCount );
		if ( !owned ) return 0;
		memcpy( owned, rgba8, byteCount );
	} else if ( width || height ) {
		return 0;
	}
	handle = (qhandle_t)state->nextHandle;
	if ( !RenderSubmission_RecordAsset( state, kind, name, handle ) ) {
		free( owned ); return 0;
	}
	record = &state->materials[state->materialCount++];
	memset( record, 0, sizeof( *record ) );
	(void)snprintf( record->name, sizeof( record->name ), "%s", name );
	record->snapshot.handle = handle;
	record->snapshot.generation = state->nextMaterialGeneration++;
	record->snapshot.width = width; record->snapshot.height = height;
	record->snapshot.rowBytes = width * 4u;
	record->snapshot.byteCount = byteCount;
	record->snapshot.rgba8 = owned;
	record->snapshot.clampToEdge = clampToEdge;
	record->snapshot.msdf = kind == RENDER_ASSET_MSDF ? qtrue : qfalse;
	record->snapshot.srgb = ( record->snapshot.msdf
		|| kind == RENDER_ASSET_LIGHTMAP ) ? qfalse : qtrue;
	DefaultMaterialRasterPolicy( kind, owned, byteCount, &record->snapshot );
	record->snapshot.ready = owned ? qtrue : qfalse;
	record->snapshot.digest = MaterialSnapshotDigest( record->name,
		&record->snapshot );
	if ( owned ) {
		state->resolvedMaterialCount++;
		state->materialBytes += byteCount;
	}
	RebuildMaterialDigest( state );
	state->nextHandle++;
	return handle;
}

qboolean RenderSubmission_SetMaterialRasterPolicy( renderSubmissionState_t *state,
		qhandle_t handle, renderAlphaMode_t alphaMode, float alphaCutoff,
		qboolean depthWrite ) {
	uint32_t i;
	if ( !state || !state->initialized || handle <= 0
			|| alphaMode < RENDER_ALPHA_OPAQUE || alphaMode > RENDER_ALPHA_BLEND
			|| !isfinite( alphaCutoff ) || alphaCutoff < 0.0f || alphaCutoff > 1.0f
			|| ( depthWrite != qfalse && depthWrite != qtrue )
			|| state->nextMaterialGeneration == UINT64_MAX ) return qfalse;
	for ( i = 0u; i < state->materialCount; ++i ) {
		renderMaterialSnapshot_t *snapshot = &state->materials[i].snapshot;
		if ( snapshot->handle != handle ) continue;
		if ( snapshot->alphaMode == alphaMode && snapshot->alphaCutoff == alphaCutoff
				&& snapshot->depthWrite == depthWrite ) return qtrue;
		snapshot->alphaMode = alphaMode;
		snapshot->alphaCutoff = alphaCutoff;
		snapshot->depthWrite = depthWrite;
		snapshot->generation = state->nextMaterialGeneration++;
		snapshot->digest = MaterialSnapshotDigest( state->materials[i].name,
			snapshot );
		RebuildMaterialDigest( state );
		return qtrue;
	}
	return qfalse;
}

qboolean RenderSubmission_RecordAsset( renderSubmissionState_t *state,
		renderAssetKind_t kind, const char *name, qhandle_t handle ) {
	uint32_t length;
	qboolean material;
	if ( !state || !state->initialized || !KindValid( kind ) || !name || !name[0]
			|| handle <= 0 ) return qfalse;
	length = (uint32_t)strlen( name );
	material = ( kind == RENDER_ASSET_MATERIAL || kind == RENDER_ASSET_MSDF
		|| kind == RENDER_ASSET_PRIMITIVE_MATERIAL
		|| kind == RENDER_ASSET_LIGHTMAP ) ? qtrue : qfalse;
	if ( length >= MAX_QPATH || state->registeredAssetCount == UINT32_MAX
			|| ( material && state->registeredMaterialCount == UINT32_MAX ) ) return qfalse;
	state->assetDigest = HashU32( state->assetDigest, (uint32_t)kind );
	state->assetDigest = HashBytes( state->assetDigest, &handle, sizeof( handle ) );
	state->assetDigest = HashBytes( state->assetDigest, name, length + 1u );
	state->registeredAssetCount++;
	if ( material ) state->registeredMaterialCount++;
	return qtrue;
}

qboolean RenderSubmission_LoadWorld( renderSubmissionState_t *state,
		const mapFile_t *bsp, int worldIndex ) {
	uint64_t digest = FNV_OFFSET;
	renderWorldVertex_t *vertices = NULL;
	uint32_t *indices = NULL;
	renderWorldBatch_t *batches = NULL;
	uint32_t vertexCount = 0u, indexCount = 0u, batchCount = 0u;
	uint32_t patchBatchCount = 0u, patchTriangleCount = 0u;
	if ( !state || !state->initialized || !bsp || worldIndex < 0
			|| bsp->numSurfaces < 0 || bsp->numDrawVerts < 0
			|| bsp->numDrawIndexes < 0 || bsp->numShaders < 0
			|| ( bsp->numSurfaces && !bsp->surfaces )
			|| ( bsp->numDrawVerts && !bsp->drawVerts )
			|| ( bsp->numDrawIndexes && !bsp->drawIndexes )
			|| ( bsp->numShaders && !bsp->shaders ) ) return qfalse;
	digest = HashBytes( digest, bsp->name, strnlen( bsp->name, sizeof( bsp->name ) ) );
	digest = HashBytes( digest, &bsp->checksum, sizeof( bsp->checksum ) );
	digest = HashBytes( digest, &worldIndex, sizeof( worldIndex ) );
	digest = HashBytes( digest, bsp->surfaces,
		(size_t)bsp->numSurfaces * sizeof( *bsp->surfaces ) );
	digest = HashBytes( digest, bsp->drawVerts,
		(size_t)bsp->numDrawVerts * sizeof( *bsp->drawVerts ) );
	digest = HashBytes( digest, bsp->drawIndexes,
		(size_t)bsp->numDrawIndexes * sizeof( *bsp->drawIndexes ) );
	digest = HashBytes( digest, bsp->shaders,
		(size_t)bsp->numShaders * sizeof( *bsp->shaders ) );
	for ( int surfaceIndex = 0; surfaceIndex < bsp->numSurfaces; ++surfaceIndex ) {
		const dsurface_t *surface = &bsp->surfaces[surfaceIndex];
		uint64_t addVertices, addIndices;
		if ( surface->surfaceType == MST_BAD || surface->surfaceType == MST_FLARE ) continue;
		if ( surface->surfaceType != MST_PLANAR
				&& surface->surfaceType != MST_TRIANGLE_SOUP
				&& surface->surfaceType != MST_PATCH ) return qfalse;
		if ( surface->shaderNum < 0 || surface->shaderNum >= bsp->numShaders
				|| surface->firstVert < 0 || surface->numVerts < 0
				|| surface->firstVert > bsp->numDrawVerts - surface->numVerts ) return qfalse;
		if ( bsp->shaders[surface->shaderNum].surfaceFlags
				& ( SURF_NODRAW | SURF_SKIP ) ) continue;
		if ( surface->surfaceType == MST_PATCH ) {
			uint64_t blocks;
			if ( surface->patchWidth < 3 || surface->patchHeight < 3
					|| !( surface->patchWidth & 1 ) || !( surface->patchHeight & 1 )
					|| surface->patchWidth > surface->numVerts
					|| surface->patchHeight > surface->numVerts / surface->patchWidth
					|| surface->patchWidth * surface->patchHeight != surface->numVerts )
				return qfalse;
			blocks = (uint64_t)( ( surface->patchWidth - 1 ) / 2 )
				* (uint64_t)( ( surface->patchHeight - 1 ) / 2 );
			addVertices = blocks * ( RENDER_SUBMISSION_PATCH_SUBDIVISIONS + 1u )
				* ( RENDER_SUBMISSION_PATCH_SUBDIVISIONS + 1u );
			addIndices = blocks * RENDER_SUBMISSION_PATCH_SUBDIVISIONS
				* RENDER_SUBMISSION_PATCH_SUBDIVISIONS * 6u;
			patchBatchCount++;
			patchTriangleCount += (uint32_t)( addIndices / 3u );
		} else {
			if ( surface->firstIndex < 0 || surface->numIndexes < 0
					|| surface->firstIndex > bsp->numDrawIndexes - surface->numIndexes
					|| ( surface->numIndexes % 3 ) != 0 ) return qfalse;
			addVertices = (uint32_t)surface->numVerts;
			addIndices = (uint32_t)surface->numIndexes;
		}
		if ( addVertices > RENDER_SUBMISSION_MAX_WORLD_VERTICES - vertexCount
				|| addIndices > RENDER_SUBMISSION_MAX_WORLD_INDICES - indexCount
				|| batchCount >= RENDER_SUBMISSION_MAX_WORLD_BATCHES ) return qfalse;
		vertexCount += (uint32_t)addVertices;
		indexCount += (uint32_t)addIndices;
		batchCount++;
	}
	if ( vertexCount ) vertices = (renderWorldVertex_t *)calloc( vertexCount,
		sizeof( *vertices ) );
	if ( indexCount ) indices = (uint32_t *)calloc( indexCount, sizeof( *indices ) );
	if ( batchCount ) batches = (renderWorldBatch_t *)calloc( batchCount,
		sizeof( *batches ) );
	if ( ( vertexCount && !vertices ) || ( indexCount && !indices )
			|| ( batchCount && !batches ) ) {
		free( batches ); free( indices ); free( vertices ); return qfalse;
	}
	{
		uint32_t vertexCursor = 0u, indexCursor = 0u, batchCursor = 0u;
		for ( int surfaceIndex = 0; surfaceIndex < bsp->numSurfaces; ++surfaceIndex ) {
			const dsurface_t *surface = &bsp->surfaces[surfaceIndex];
			renderWorldBatch_t *batch;
			if ( surface->surfaceType == MST_BAD || surface->surfaceType == MST_FLARE ) continue;
			if ( bsp->shaders[surface->shaderNum].surfaceFlags
					& ( SURF_NODRAW | SURF_SKIP ) ) continue;
			batch = &batches[batchCursor++];
			batch->sourceSurfaceIndex = (uint32_t)surfaceIndex;
			batch->firstIndex = indexCursor;
			batch->shaderIndex = surface->shaderNum;
			batch->lightmapIndex = surface->lightmapNum;
			batch->surfaceType = surface->surfaceType == MST_PATCH
				? RENDER_WORLD_SURFACE_PATCH
				: ( surface->surfaceType == MST_PLANAR
					? RENDER_WORLD_SURFACE_PLANAR : RENDER_WORLD_SURFACE_TRIANGLES );
			batch->alphaMode = RENDER_ALPHA_OPAQUE;
			batch->alphaCutoff = 0.5f;
			batch->depthWrite = qtrue;
			batch->baseMaterial = RenderSubmission_MaterialHandle( state,
				bsp->shaders[surface->shaderNum].shader );
			if ( batch->baseMaterial ) {
				renderMaterialSnapshot_t material;
				if ( RenderSubmission_MaterialSnapshot( state, batch->baseMaterial,
						&material ) ) {
					batch->alphaMode = material.alphaMode;
					batch->alphaCutoff = material.alphaCutoff;
					batch->depthWrite = material.depthWrite;
				}
			}
			if ( surface->lightmapNum >= 0 ) {
				char lightmapName[MAX_QPATH];
				if ( !RenderSubmission_LightmapMaterialName( lightmapName,
						(uint32_t)bsp->checksum, surface->lightmapNum ) ) {
					free( batches ); free( indices ); free( vertices ); return qfalse;
				}
				batch->lightmapMaterial = RenderSubmission_MaterialHandle( state,
					lightmapName );
			}
			if ( surface->surfaceType != MST_PATCH ) {
				uint32_t baseVertex = vertexCursor;
				for ( int i = 0; i < surface->numVerts; ++i ) {
					const drawVert_t *source = &bsp->drawVerts[surface->firstVert + i];
					renderWorldVertex_t *target = &vertices[vertexCursor++];
					memcpy( target->position, source->xyz, sizeof( target->position ) );
					memcpy( target->texCoord, source->st, sizeof( target->texCoord ) );
					memcpy( target->lightmapCoord, source->lightmap,
						sizeof( target->lightmapCoord ) );
					memcpy( target->color, source->color.rgba, sizeof( target->color ) );
				}
				for ( int i = 0; i < surface->numIndexes; ++i ) {
					int local = bsp->drawIndexes[surface->firstIndex + i];
					if ( local < 0 || local >= surface->numVerts ) {
						free( batches ); free( indices ); free( vertices ); return qfalse;
					}
					indices[indexCursor++] = baseVertex + (uint32_t)local;
				}
			} else {
				const uint32_t side = RENDER_SUBMISSION_PATCH_SUBDIVISIONS + 1u;
				for ( int blockY = 0; blockY < surface->patchHeight - 1; blockY += 2 ) {
					for ( int blockX = 0; blockX < surface->patchWidth - 1; blockX += 2 ) {
						uint32_t baseVertex = vertexCursor;
						for ( uint32_t y = 0u; y < side; ++y ) {
							float v = (float)y / RENDER_SUBMISSION_PATCH_SUBDIVISIONS;
							float bv[3] = { ( 1.0f - v ) * ( 1.0f - v ),
								2.0f * v * ( 1.0f - v ), v * v };
							for ( uint32_t x = 0u; x < side; ++x ) {
								float u = (float)x / RENDER_SUBMISSION_PATCH_SUBDIVISIONS;
								float bu[3] = { ( 1.0f - u ) * ( 1.0f - u ),
									2.0f * u * ( 1.0f - u ), u * u };
								renderWorldVertex_t *target = &vertices[vertexCursor++];
								float color[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
								for ( uint32_t cy = 0u; cy < 3u; ++cy ) {
									for ( uint32_t cx = 0u; cx < 3u; ++cx ) {
										const drawVert_t *control = &bsp->drawVerts[
											surface->firstVert + ( blockY + (int)cy )
											* surface->patchWidth + blockX + (int)cx];
										float weight = bu[cx] * bv[cy];
										for ( uint32_t axis = 0u; axis < 3u; ++axis )
											target->position[axis] += control->xyz[axis] * weight;
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
							}
						}
						for ( uint32_t y = 0u; y < RENDER_SUBMISSION_PATCH_SUBDIVISIONS; ++y ) {
							for ( uint32_t x = 0u; x < RENDER_SUBMISSION_PATCH_SUBDIVISIONS; ++x ) {
								uint32_t a = baseVertex + y * side + x;
								uint32_t b = a + 1u, c = a + side, d = c + 1u;
								indices[indexCursor++] = a; indices[indexCursor++] = c;
								indices[indexCursor++] = b; indices[indexCursor++] = b;
								indices[indexCursor++] = c; indices[indexCursor++] = d;
							}
						}
					}
				}
			}
			batch->indexCount = indexCursor - batch->firstIndex;
		}
	}
	free( (void *)state->worldSnapshot.vertices );
	free( (void *)state->worldSnapshot.indices );
	free( (void *)state->worldSnapshot.batches );
	memset( &state->worldSnapshot, 0, sizeof( state->worldSnapshot ) );
	state->worldSnapshot.vertices = vertices;
	state->worldSnapshot.indices = indices;
	state->worldSnapshot.batches = batches;
	state->worldSnapshot.vertexCount = vertexCount;
	state->worldSnapshot.indexCount = indexCount;
	state->worldSnapshot.batchCount = batchCount;
	state->worldSnapshot.patchBatchCount = patchBatchCount;
	state->worldSnapshot.patchTriangleCount = patchTriangleCount;
	state->worldSnapshot.ready = qtrue;
	state->worldDigest = digest;
	state->worldSurfaceCount = (uint32_t)bsp->numSurfaces;
	state->worldVertexCount = (uint32_t)bsp->numDrawVerts;
	state->worldIndexCount = (uint32_t)bsp->numDrawIndexes;
	state->worldLoaded = qtrue;
	return qtrue;
}

qboolean RenderSubmission_BeginFrame( renderSubmissionState_t *state,
		uint64_t frameGeneration ) {
	if ( !state || !state->initialized || state->frameOpen
			|| frameGeneration == 0u || frameGeneration == UINT64_MAX ) return qfalse;
	state->sceneDigest = HashU32( FNV_OFFSET, (uint32_t)frameGeneration );
	state->uiDigest = HashU32( FNV_OFFSET, (uint32_t)( frameGeneration >> 32u ) );
	state->entityCount = state->temporalEntityCount = 0u;
	state->polygonCount = state->lightCount = 0u;
	state->polyCommandCount = state->polyVertexCount = 0u;
	state->uiPrimitiveCount = 0u;
	state->sceneRendered = qfalse;
	state->frameSealed = qfalse;
	state->frameOpen = qtrue;
	return qtrue;
}

void RenderSubmission_CancelFrame( renderSubmissionState_t *state ) {
	if ( state && state->initialized ) {
		state->frameOpen = qfalse;
		state->frameSealed = qfalse;
	}
}

qboolean RenderSubmission_ClearScene( renderSubmissionState_t *state ) {
	/* Registration/loading code may clear the scene before the first BeginFrame.
	 * Treat that as an idempotent staging reset; actual submissions still require
	 * an open frame. */
	if ( !state || !state->initialized ) return qfalse;
	state->sceneDigest = FNV_OFFSET;
	state->entityCount = state->temporalEntityCount = 0u;
	state->polygonCount = state->lightCount = 0u;
	state->polyCommandCount = state->polyVertexCount = 0u;
	state->sceneRendered = qfalse;
	return qtrue;
}

qboolean RenderSubmission_AddEntity( renderSubmissionState_t *state,
		const refEntity_t *entity, const refEntityMotion_t *motion ) {
	renderEntityCommand_t *command;
	if ( !state || !state->frameOpen || !entity
			|| ( motion && !RefEntityMotion_IsValid( motion ) )
			|| entity->reType < RT_MODEL || entity->reType >= RT_MAX_REF_ENTITY_TYPE
			|| state->entityCount >= RENDER_SUBMISSION_MAX_ENTITIES
			|| !AddCount( &state->entityCount, 1u ) ) return qfalse;
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
			state->entityCount--; return qfalse;
		}
	}
	command = &state->entities[state->entityCount - 1u];
	memset( command, 0, sizeof( *command ) );
	command->entity = *entity;
	if ( motion ) {
		command->motion = *motion; command->hasTemporal = qtrue;
		state->temporalEntityCount++;
	}
	state->sceneDigest = HashBytes( state->sceneDigest, entity, sizeof( *entity ) );
	if ( motion ) state->sceneDigest = HashBytes( state->sceneDigest, motion, sizeof( *motion ) );
	return qtrue;
}

qboolean RenderSubmission_AddPoly( renderSubmissionState_t *state,
		qhandle_t material, int verticesPerPoly, const polyVert_t *vertices,
		int polygonCount ) {
	renderPolyCommand_t *newCommands = NULL;
	polyVert_t *newVertices = NULL;
	uint32_t newCommandCapacity, newVertexCapacity;
	size_t vertexCount;
	if ( !state || !state->frameOpen || material <= 0 || verticesPerPoly < 3
			|| polygonCount <= 0 || !vertices ) return qfalse;
	vertexCount = (size_t)verticesPerPoly * (size_t)polygonCount;
	if ( vertexCount > UINT32_MAX
			|| state->polyCommandCount >= RENDER_SUBMISSION_MAX_POLY_COMMANDS
			|| vertexCount > RENDER_SUBMISSION_MAX_POLY_VERTICES
				- state->polyVertexCount
			|| (uint32_t)polygonCount > UINT32_MAX - state->polygonCount ) return qfalse;
	newCommandCapacity = state->polyCommandCapacity;
	if ( state->polyCommandCount == newCommandCapacity ) {
		newCommandCapacity = newCommandCapacity ? newCommandCapacity * 2u : 16u;
		if ( newCommandCapacity > RENDER_SUBMISSION_MAX_POLY_COMMANDS )
			newCommandCapacity = RENDER_SUBMISSION_MAX_POLY_COMMANDS;
		newCommands = (renderPolyCommand_t *)malloc(
			(size_t)newCommandCapacity * sizeof( *newCommands ) );
		if ( !newCommands ) return qfalse;
		if ( state->polyCommandCount ) memcpy( newCommands, state->polyCommands,
			(size_t)state->polyCommandCount * sizeof( *newCommands ) );
	}
	newVertexCapacity = state->polyVertexCapacity;
	if ( vertexCount > newVertexCapacity - state->polyVertexCount ) {
		newVertexCapacity = newVertexCapacity ? newVertexCapacity : 64u;
		while ( vertexCount > newVertexCapacity - state->polyVertexCount
				&& newVertexCapacity < RENDER_SUBMISSION_MAX_POLY_VERTICES ) {
			uint32_t doubled = newVertexCapacity > RENDER_SUBMISSION_MAX_POLY_VERTICES / 2u
				? RENDER_SUBMISSION_MAX_POLY_VERTICES : newVertexCapacity * 2u;
			newVertexCapacity = doubled;
		}
		if ( vertexCount > newVertexCapacity - state->polyVertexCount ) {
			free( newCommands ); return qfalse;
		}
		newVertices = (polyVert_t *)malloc(
			(size_t)newVertexCapacity * sizeof( *newVertices ) );
		if ( !newVertices ) { free( newCommands ); return qfalse; }
		if ( state->polyVertexCount ) memcpy( newVertices, state->polyVertices,
			(size_t)state->polyVertexCount * sizeof( *newVertices ) );
	}
	if ( newCommands ) {
		free( state->polyCommands ); state->polyCommands = newCommands;
		state->polyCommandCapacity = newCommandCapacity;
	}
	if ( newVertices ) {
		free( state->polyVertices ); state->polyVertices = newVertices;
		state->polyVertexCapacity = newVertexCapacity;
	}
	{
		renderPolyCommand_t *command = &state->polyCommands[state->polyCommandCount++];
		command->material = material;
		command->firstVertex = state->polyVertexCount;
		command->verticesPerPolygon = (uint32_t)verticesPerPoly;
		command->polygonCount = (uint32_t)polygonCount;
	}
	memcpy( state->polyVertices + state->polyVertexCount, vertices,
		vertexCount * sizeof( *vertices ) );
	state->polyVertexCount += (uint32_t)vertexCount;
	state->polygonCount += (uint32_t)polygonCount;
	state->sceneDigest = HashBytes( state->sceneDigest, &material, sizeof( material ) );
	state->sceneDigest = HashBytes( state->sceneDigest, vertices,
		vertexCount * sizeof( *vertices ) );
	return qtrue;
}

qboolean RenderSubmission_AddLight( renderSubmissionState_t *state,
		const vec3_t origin, const vec3_t end, float intensity,
		float red, float green, float blue ) {
	renderLightCommand_t *newLights = NULL;
	uint32_t newCapacity;
	if ( !state || !state->frameOpen || !origin
			|| state->lightCount >= RENDER_SUBMISSION_MAX_LIGHTS ) return qfalse;
	newCapacity = state->lightCapacity;
	if ( state->lightCount == newCapacity ) {
		newCapacity = newCapacity ? newCapacity * 2u : 16u;
		if ( newCapacity > RENDER_SUBMISSION_MAX_LIGHTS )
			newCapacity = RENDER_SUBMISSION_MAX_LIGHTS;
		newLights = (renderLightCommand_t *)malloc(
			(size_t)newCapacity * sizeof( *newLights ) );
		if ( !newLights ) return qfalse;
		if ( state->lightCount ) memcpy( newLights, state->lights,
			(size_t)state->lightCount * sizeof( *newLights ) );
		free( state->lights ); state->lights = newLights;
		state->lightCapacity = newCapacity;
	}
	{
		renderLightCommand_t *command = &state->lights[state->lightCount];
		memset( command, 0, sizeof( *command ) );
		memcpy( command->origin, origin, sizeof( command->origin ) );
		if ( end ) { memcpy( command->end, end, sizeof( command->end ) );
			command->hasEnd = qtrue; }
		command->intensity = intensity;
		command->color[0] = red; command->color[1] = green;
		command->color[2] = blue;
	}
	state->lightCount++;
	state->sceneDigest = HashBytes( state->sceneDigest, origin, sizeof( vec3_t ) );
	if ( end ) state->sceneDigest = HashBytes( state->sceneDigest, end, sizeof( vec3_t ) );
	state->sceneDigest = HashBytes( state->sceneDigest, &intensity, sizeof( intensity ) );
	state->sceneDigest = HashBytes( state->sceneDigest, &red, sizeof( red ) );
	state->sceneDigest = HashBytes( state->sceneDigest, &green, sizeof( green ) );
	state->sceneDigest = HashBytes( state->sceneDigest, &blue, sizeof( blue ) );
	return qtrue;
}

qboolean RenderSubmission_RenderScene( renderSubmissionState_t *state,
		const refdef_t *view, int worldIndex ) {
	if ( !state || !state->frameOpen || !view || worldIndex < 0 ) return qfalse;
	state->sceneDigest = HashBytes( state->sceneDigest, view, sizeof( *view ) );
	state->sceneDigest = HashBytes( state->sceneDigest, &worldIndex, sizeof( worldIndex ) );
	memcpy( state->worldSnapshot.viewOrigin, view->vieworg,
		sizeof( state->worldSnapshot.viewOrigin ) );
	memcpy( state->worldSnapshot.viewAxis, view->viewaxis,
		sizeof( state->worldSnapshot.viewAxis ) );
	state->worldSnapshot.fovX = view->fov_x;
	state->worldSnapshot.fovY = view->fov_y;
	state->sceneRendered = qtrue;
	return qtrue;
}

qboolean RenderSubmission_SetColor( renderSubmissionState_t *state,
		const float *rgba ) {
	static const float white[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	if ( !state || !state->initialized ) return qfalse;
	memcpy( state->color, rgba ? rgba : white, sizeof( state->color ) );
	return qtrue;
}

qboolean RenderSubmission_AddUiQuad( renderSubmissionState_t *state,
		float x, float y, float width, float height, float s1, float t1,
		float s2, float t2, float rotation, qhandle_t material ) {
	renderUiPrimitive_t *primitive;
	float values[9] = { x, y, width, height, s1, t1, s2, t2, rotation };
	if ( !state || !state->frameOpen || material <= 0
			|| state->uiPrimitiveCount >= RENDER_SUBMISSION_MAX_UI_PRIMITIVES
			|| !AddCount( &state->uiPrimitiveCount, 1u ) ) return qfalse;
	primitive = &state->uiPrimitives[state->uiPrimitiveCount - 1u];
	memset( primitive, 0, sizeof( *primitive ) );
	primitive->kind = RENDER_UI_QUAD;
	primitive->x = x; primitive->y = y;
	primitive->width = width; primitive->height = height;
	primitive->s1 = s1; primitive->t1 = t1;
	primitive->s2 = s2; primitive->t2 = t2;
	primitive->rotation = rotation;
	memcpy( primitive->color, state->color, sizeof( primitive->color ) );
	primitive->material = material;
	state->uiDigest = HashBytes( state->uiDigest, values, sizeof( values ) );
	state->uiDigest = HashBytes( state->uiDigest, state->color, sizeof( state->color ) );
	state->uiDigest = HashBytes( state->uiDigest, &material, sizeof( material ) );
	return qtrue;
}

qboolean RenderSubmission_AddUiLine( renderSubmissionState_t *state,
		float x1, float y1, float x2, float y2, float width,
		qhandle_t material ) {
	qboolean added = RenderSubmission_AddUiQuad( state, x1, y1, x2, y2, width,
		0.0f, 0.0f, 0.0f, 0.0f, material );
	if ( added ) state->uiPrimitives[state->uiPrimitiveCount - 1u].kind = RENDER_UI_LINE;
	return added;
}

uint64_t RenderSubmission_FrameDigest( const renderSubmissionState_t *state ) {
	uint64_t digest;
	if ( !state || !state->initialized
			|| ( !state->frameOpen && !state->frameSealed ) ) return 0u;
	digest = HashBytes( FNV_OFFSET, &state->worldDigest, sizeof( state->worldDigest ) );
	digest = HashBytes( digest, &state->assetDigest, sizeof( state->assetDigest ) );
	digest = HashBytes( digest, &state->materialDigest,
		sizeof( state->materialDigest ) );
	digest = HashBytes( digest, &state->modelDigest, sizeof( state->modelDigest ) );
	digest = HashBytes( digest, &state->sceneDigest, sizeof( state->sceneDigest ) );
	digest = HashBytes( digest, &state->uiDigest, sizeof( state->uiDigest ) );
	digest = HashBytes( digest, &state->entityCount, sizeof( state->entityCount ) );
	digest = HashBytes( digest, &state->uiPrimitiveCount, sizeof( state->uiPrimitiveCount ) );
	return digest;
}

qboolean RenderSubmission_EndFrame( renderSubmissionState_t *state,
		uint64_t frameGeneration, renderSubmissionReceipt_t *outReceipt ) {
	renderSubmissionReceipt_t receipt;
	if ( !state || !state->frameOpen || !outReceipt || frameGeneration == 0u
			|| frameGeneration == UINT64_MAX ) return qfalse;
	memset( &receipt, 0, sizeof( receipt ) );
	receipt.schemaVersion = RENDER_SUBMISSION_SCHEMA_VERSION;
	receipt.ownerGeneration = state->ownerGeneration;
	receipt.frameGeneration = frameGeneration;
	receipt.assetDigest = state->assetDigest;
	receipt.materialDigest = state->materialDigest;
	receipt.modelDigest = state->modelDigest;
	receipt.worldDigest = state->worldDigest;
	receipt.sceneDigest = state->sceneDigest;
	receipt.uiDigest = state->uiDigest;
	receipt.frameDigest = RenderSubmission_FrameDigest( state );
	receipt.worldSurfaceCount = state->worldSurfaceCount;
	receipt.worldVertexCount = state->worldVertexCount;
	receipt.worldIndexCount = state->worldIndexCount;
	receipt.registeredAssetCount = state->registeredAssetCount;
	receipt.registeredMaterialCount = state->registeredMaterialCount;
	receipt.resolvedMaterialCount = state->resolvedMaterialCount;
	receipt.materialBytes = state->materialBytes;
	receipt.registeredModelCount = state->modelCount;
	receipt.modelBytes = state->modelBytes;
	receipt.entityCount = state->entityCount;
	receipt.temporalEntityCount = state->temporalEntityCount;
	receipt.polygonCount = state->polygonCount;
	receipt.lightCount = state->lightCount;
	receipt.uiPrimitiveCount = state->uiPrimitiveCount;
	receipt.worldLoaded = state->worldLoaded;
	receipt.sceneRendered = state->sceneRendered;
	receipt.ready = qtrue;
	if ( !ReceiptValid( &receipt ) ) return qfalse;
	state->frameOpen = qfalse;
	state->frameSealed = qtrue;
	*outReceipt = receipt;
	return qtrue;
}

qboolean RenderSubmission_ReceiptExact( const renderSubmissionReceipt_t *a,
		const renderSubmissionReceipt_t *b ) {
	return ( ReceiptValid( a ) && ReceiptValid( b )
		&& !memcmp( a, b, sizeof( *a ) ) ) ? qtrue : qfalse;
}
