// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "render_submission.h"
#include "qfiles.h"

#include <limits.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define MODEL_FNV_OFFSET UINT64_C(1469598103934665603)
#define MODEL_FNV_PRIME UINT64_C(1099511628211)
#define IQM_VERSION 2u

typedef struct {
	char magic[16];
	uint32_t version, filesize, flags;
	uint32_t numText, ofsText, numMeshes, ofsMeshes;
	uint32_t numVertexArrays, numVertexes, ofsVertexArrays;
	uint32_t numTriangles, ofsTriangles, ofsAdjacency;
	uint32_t numJoints, ofsJoints, numPoses, ofsPoses, numAnims, ofsAnims;
	uint32_t numFrames, numFrameChannels, ofsFrames, ofsBounds;
	uint32_t numComment, ofsComment, numExtensions, ofsExtensions;
} neutralIqmHeader_t;

typedef struct {
	uint32_t name, material, firstVertex, numVertexes, firstTriangle, numTriangles;
} neutralIqmMesh_t;

typedef struct { uint32_t vertex[3]; } neutralIqmTriangle_t;
typedef struct { uint32_t type, flags, format, size, offset; } neutralIqmVertexArray_t;

enum { NEUTRAL_IQM_POSITION = 0, NEUTRAL_IQM_TEXCOORD = 1, NEUTRAL_IQM_FLOAT = 7 };

static uint64_t ModelHash( uint64_t digest, const void *data, size_t size ) {
	const byte *bytes = (const byte *)data;
	for ( size_t i = 0u; i < size; ++i ) {
		digest ^= bytes[i]; digest *= MODEL_FNV_PRIME;
	}
	return digest;
}

static qboolean ModelRange( uint32_t byteCount, uint32_t offset,
		uint32_t count, size_t stride ) {
	return ( offset <= byteCount && (uint64_t)count * stride
		<= (uint64_t)byteCount - offset ) ? qtrue : qfalse;
}

static qboolean ModelFinite( const float *values, size_t count ) {
	for ( size_t i = 0u; i < count; ++i )
		if ( !isfinite( values[i] ) ) return qfalse;
	return qtrue;
}

static void FreeSnapshot( renderModelSnapshot_t *snapshot ) {
	if ( !snapshot ) return;
	free( (void *)snapshot->positions );
	free( (void *)snapshot->texCoords );
	free( (void *)snapshot->indices );
	free( (void *)snapshot->batches );
	memset( snapshot, 0, sizeof( *snapshot ) );
}

static uint64_t SnapshotBytes( const renderModelSnapshot_t *snapshot ) {
	return (uint64_t)snapshot->frameCount * snapshot->vertexCount * 3u * sizeof( float )
		+ (uint64_t)snapshot->vertexCount * 2u * sizeof( float )
		+ (uint64_t)snapshot->indexCount * sizeof( uint32_t )
		+ (uint64_t)snapshot->batchCount * sizeof( renderModelBatch_t );
}

static uint64_t SnapshotDigest( const char *name,
		const renderModelSnapshot_t *snapshot ) {
	uint64_t digest = ModelHash( MODEL_FNV_OFFSET, name, strlen( name ) + 1u );
	const uint32_t facts[] = { (uint32_t)snapshot->format, snapshot->frameCount,
		snapshot->vertexCount, snapshot->indexCount, snapshot->batchCount };
	digest = ModelHash( digest, facts, sizeof( facts ) );
	digest = ModelHash( digest, snapshot->positions,
		(size_t)snapshot->frameCount * snapshot->vertexCount * 3u * sizeof( float ) );
	digest = ModelHash( digest, snapshot->texCoords,
		(size_t)snapshot->vertexCount * 2u * sizeof( float ) );
	digest = ModelHash( digest, snapshot->indices,
		(size_t)snapshot->indexCount * sizeof( uint32_t ) );
	for ( uint32_t i = 0u; i < snapshot->batchCount; ++i ) {
		const renderModelBatch_t *batch = &snapshot->batches[i];
		digest = ModelHash( digest, batch, offsetof( renderModelBatch_t, material ) );
		digest = ModelHash( digest, &batch->material, sizeof( batch->material ) );
		digest = ModelHash( digest, batch->materialName,
			strlen( batch->materialName ) + 1u );
	}
	return digest ? digest : 1u;
}

static qhandle_t RegisterModelAsset( renderSubmissionState_t *state,
		const char *name ) {
	qhandle_t handle;
	uint32_t kind = (uint32_t)RENDER_ASSET_MODEL;
	if ( !state || state->nextHandle >= INT_MAX
			|| state->registeredAssetCount == UINT32_MAX ) return 0;
	handle = (qhandle_t)state->nextHandle++;
	state->assetDigest = ModelHash( state->assetDigest, &kind, sizeof( kind ) );
	state->assetDigest = ModelHash( state->assetDigest, &handle, sizeof( handle ) );
	state->assetDigest = ModelHash( state->assetDigest, name, strlen( name ) + 1u );
	state->registeredAssetCount++;
	return handle;
}

static qboolean DecodeMd3( const void *fileBytes, uint32_t byteCount,
		renderModelSnapshot_t *out ) {
	const byte *bytes = (const byte *)fileBytes;
	md3Header_t header;
	uint32_t totalVertices = 0u, totalIndices = 0u, offset;
	if ( byteCount < sizeof( header ) ) return qfalse;
	memcpy( &header, bytes, sizeof( header ) );
	header.ident = LittleLong( header.ident ); header.version = LittleLong( header.version );
	header.numFrames = LittleLong( header.numFrames );
	header.numSurfaces = LittleLong( header.numSurfaces );
	header.ofsSurfaces = LittleLong( header.ofsSurfaces );
	header.ofsEnd = LittleLong( header.ofsEnd );
	if ( header.ident != MD3_IDENT || header.version != MD3_VERSION
			|| header.numFrames <= 0
			|| (uint32_t)header.numFrames > RENDER_SUBMISSION_MAX_MODEL_FRAMES
			|| header.numSurfaces < 0
			|| header.numSurfaces > (int32_t)RENDER_SUBMISSION_MAX_MODEL_BATCHES
			|| header.ofsEnd > byteCount || header.ofsSurfaces > header.ofsEnd ) return qfalse;
	if ( header.numSurfaces == 0 ) {
		out->format = RENDER_MODEL_MD3;
		out->frameCount = (uint32_t)header.numFrames;
		return qtrue;
	}
	offset = header.ofsSurfaces;
	for ( int32_t i = 0; i < header.numSurfaces; ++i ) {
		md3Surface_t surface;
		uint32_t end;
		if ( !ModelRange( header.ofsEnd, offset, 1u, sizeof( surface ) ) ) return qfalse;
		memcpy( &surface, bytes + offset, sizeof( surface ) );
		surface.ident = LittleLong( surface.ident );
		surface.numFrames = LittleLong( surface.numFrames );
		surface.numShaders = LittleLong( surface.numShaders );
		surface.numVerts = LittleLong( surface.numVerts );
		surface.numTriangles = LittleLong( surface.numTriangles );
		surface.ofsTriangles = LittleLong( surface.ofsTriangles );
		surface.ofsShaders = LittleLong( surface.ofsShaders );
		surface.ofsSt = LittleLong( surface.ofsSt );
		surface.ofsXyzNormals = LittleLong( surface.ofsXyzNormals );
		surface.ofsEnd = LittleLong( surface.ofsEnd );
		if ( surface.ident != MD3_IDENT || surface.numFrames != header.numFrames
				|| surface.numVerts <= 0 || surface.numTriangles <= 0
				|| surface.numVerts > MD3_MAX_VERTS
				|| surface.numTriangles > MD3_MAX_TRIANGLES
				|| surface.numShaders < 0 || surface.numShaders > MD3_MAX_SHADERS
				|| surface.ofsEnd < sizeof( surface )
				|| surface.ofsEnd > header.ofsEnd - offset ) return qfalse;
		end = offset + surface.ofsEnd;
		if ( !ModelRange( end, offset + surface.ofsTriangles,
				(uint32_t)surface.numTriangles, sizeof( md3Triangle_t ) )
				|| !ModelRange( end, offset + surface.ofsSt,
					(uint32_t)surface.numVerts, sizeof( md3St_t ) )
				|| !ModelRange( end, offset + surface.ofsXyzNormals,
					(uint32_t)surface.numVerts * (uint32_t)header.numFrames,
					sizeof( md3XyzNormal_t ) )
				|| ( surface.numShaders && !ModelRange( end,
					offset + surface.ofsShaders, (uint32_t)surface.numShaders,
					sizeof( md3Shader_t ) ) ) ) return qfalse;
		if ( (uint32_t)surface.numVerts > RENDER_SUBMISSION_MAX_MODEL_VERTICES
				- totalVertices || (uint32_t)surface.numTriangles * 3u
				> RENDER_SUBMISSION_MAX_MODEL_INDICES - totalIndices ) return qfalse;
		totalVertices += (uint32_t)surface.numVerts;
		totalIndices += (uint32_t)surface.numTriangles * 3u;
		offset = end;
	}
	out->format = RENDER_MODEL_MD3; out->frameCount = (uint32_t)header.numFrames;
	out->vertexCount = totalVertices; out->indexCount = totalIndices;
	out->batchCount = (uint32_t)header.numSurfaces;
	out->positions = (float *)calloc( (size_t)out->frameCount * totalVertices * 3u,
		sizeof( float ) );
	out->texCoords = (float *)calloc( (size_t)totalVertices * 2u, sizeof( float ) );
	out->indices = (uint32_t *)calloc( totalIndices, sizeof( uint32_t ) );
	out->batches = (renderModelBatch_t *)calloc( out->batchCount,
		sizeof( renderModelBatch_t ) );
	if ( !out->positions || !out->texCoords || !out->indices || !out->batches ) return qfalse;
	offset = header.ofsSurfaces;
	uint32_t vertexBase = 0u, indexBase = 0u;
	for ( uint32_t i = 0u; i < out->batchCount; ++i ) {
		md3Surface_t surface; renderModelBatch_t *batch =
			(renderModelBatch_t *)&out->batches[i];
		memcpy( &surface, bytes + offset, sizeof( surface ) );
		surface.numVerts = LittleLong( surface.numVerts );
		surface.numTriangles = LittleLong( surface.numTriangles );
		surface.numShaders = LittleLong( surface.numShaders );
		surface.ofsTriangles = LittleLong( surface.ofsTriangles );
		surface.ofsShaders = LittleLong( surface.ofsShaders );
		surface.ofsSt = LittleLong( surface.ofsSt );
		surface.ofsXyzNormals = LittleLong( surface.ofsXyzNormals );
		surface.ofsEnd = LittleLong( surface.ofsEnd );
		batch->firstVertex = vertexBase; batch->vertexCount = (uint32_t)surface.numVerts;
		batch->firstIndex = indexBase;
		batch->indexCount = (uint32_t)surface.numTriangles * 3u;
		if ( surface.numShaders ) {
			const md3Shader_t *shader = (const md3Shader_t *)( bytes + offset
				+ surface.ofsShaders );
			(void)snprintf( batch->materialName, sizeof( batch->materialName ),
				"%.*s", (int)sizeof( shader->name ), shader->name );
		}
		if ( !batch->materialName[0] )
			(void)snprintf( batch->materialName, sizeof( batch->materialName ), "*white" );
		for ( uint32_t vertex = 0u; vertex < batch->vertexCount; ++vertex ) {
			md3St_t st; memcpy( &st, bytes + offset + surface.ofsSt
				+ (size_t)vertex * sizeof( st ), sizeof( st ) );
			((float *)out->texCoords)[( vertexBase + vertex ) * 2u + 0u] = LittleFloat( st.st[0] );
			((float *)out->texCoords)[( vertexBase + vertex ) * 2u + 1u] = LittleFloat( st.st[1] );
			for ( uint32_t frame = 0u; frame < out->frameCount; ++frame ) {
				md3XyzNormal_t xyz; size_t source = (size_t)frame * batch->vertexCount + vertex;
				memcpy( &xyz, bytes + offset + surface.ofsXyzNormals
					+ source * sizeof( xyz ), sizeof( xyz ) );
				for ( uint32_t axis = 0u; axis < 3u; ++axis )
					((float *)out->positions)[((size_t)frame * totalVertices
						+ vertexBase + vertex ) * 3u + axis]
						= (float)(int16_t)LittleShort( xyz.xyz[axis] ) * (float)MD3_XYZ_SCALE;
			}
		}
		for ( uint32_t triangle = 0u; triangle < (uint32_t)surface.numTriangles; ++triangle ) {
			md3Triangle_t tri; memcpy( &tri, bytes + offset + surface.ofsTriangles
				+ (size_t)triangle * sizeof( tri ), sizeof( tri ) );
			for ( uint32_t corner = 0u; corner < 3u; ++corner ) {
				uint32_t local = LittleLong( tri.indexes[corner] );
				if ( local >= batch->vertexCount ) return qfalse;
				((uint32_t *)out->indices)[indexBase + triangle * 3u + corner]
					= vertexBase + local;
			}
		}
		vertexBase += batch->vertexCount; indexBase += batch->indexCount;
		offset += surface.ofsEnd;
	}
	return ModelFinite( out->positions,
		(size_t)out->frameCount * out->vertexCount * 3u )
		&& ModelFinite( out->texCoords, (size_t)out->vertexCount * 2u );
}

static qboolean DecodeIqm( const void *fileBytes, uint32_t byteCount,
		renderModelSnapshot_t *out ) {
	static const char magic[16] = "INTERQUAKEMODEL";
	const byte *bytes = (const byte *)fileBytes;
	neutralIqmHeader_t header;
	const float *positions = NULL, *texCoords = NULL;
	if ( byteCount < sizeof( header ) ) return qfalse;
	memcpy( &header, bytes, sizeof( header ) );
	if ( memcmp( header.magic, magic, sizeof( magic ) ) ) return qfalse;
	uint32_t *fields = &header.version;
	for ( size_t i = 0u; i < ( sizeof( header ) - sizeof( header.magic ) )
			/ sizeof( uint32_t ); ++i ) fields[i] = LittleLong( fields[i] );
	if ( header.version != IQM_VERSION || header.filesize > byteCount
			|| !header.numVertexes || header.numVertexes > RENDER_SUBMISSION_MAX_MODEL_VERTICES
			|| !header.numTriangles || header.numTriangles * 3u
				> RENDER_SUBMISSION_MAX_MODEL_INDICES
			|| !header.numMeshes || header.numMeshes > RENDER_SUBMISSION_MAX_MODEL_BATCHES
			|| !ModelRange( header.filesize, header.ofsVertexArrays,
				header.numVertexArrays, sizeof( neutralIqmVertexArray_t ) )
			|| !ModelRange( header.filesize, header.ofsTriangles,
				header.numTriangles, sizeof( neutralIqmTriangle_t ) )
			|| !ModelRange( header.filesize, header.ofsMeshes,
				header.numMeshes, sizeof( neutralIqmMesh_t ) )
			|| !ModelRange( header.filesize, header.ofsText,
				header.numText, sizeof( char ) ) ) return qfalse;
	for ( uint32_t i = 0u; i < header.numVertexArrays; ++i ) {
		neutralIqmVertexArray_t array;
		memcpy( &array, bytes + header.ofsVertexArrays
			+ (size_t)i * sizeof( array ), sizeof( array ) );
		array.type = LittleLong( array.type ); array.format = LittleLong( array.format );
		array.size = LittleLong( array.size ); array.offset = LittleLong( array.offset );
		if ( array.type == NEUTRAL_IQM_POSITION && array.format == NEUTRAL_IQM_FLOAT
				&& array.size == 3u && ModelRange( header.filesize, array.offset,
					header.numVertexes, 3u * sizeof( float ) ) ) positions =
					(const float *)( bytes + array.offset );
		if ( array.type == NEUTRAL_IQM_TEXCOORD && array.format == NEUTRAL_IQM_FLOAT
				&& array.size == 2u && ModelRange( header.filesize, array.offset,
					header.numVertexes, 2u * sizeof( float ) ) ) texCoords =
					(const float *)( bytes + array.offset );
	}
	if ( !positions ) return qfalse;
	out->format = RENDER_MODEL_IQM; out->frameCount = 1u;
	out->vertexCount = header.numVertexes; out->indexCount = header.numTriangles * 3u;
	out->batchCount = header.numMeshes;
	out->positions = (float *)calloc( (size_t)out->vertexCount * 3u, sizeof( float ) );
	out->texCoords = (float *)calloc( (size_t)out->vertexCount * 2u, sizeof( float ) );
	out->indices = (uint32_t *)calloc( out->indexCount, sizeof( uint32_t ) );
	out->batches = (renderModelBatch_t *)calloc( out->batchCount,
		sizeof( renderModelBatch_t ) );
	if ( !out->positions || !out->texCoords || !out->indices || !out->batches ) return qfalse;
	for ( uint32_t vertex = 0u; vertex < out->vertexCount; ++vertex ) {
		for ( uint32_t axis = 0u; axis < 3u; ++axis )
			((float *)out->positions)[vertex * 3u + axis] =
				LittleFloat( positions[vertex * 3u + axis] );
		if ( texCoords ) for ( uint32_t axis = 0u; axis < 2u; ++axis )
			((float *)out->texCoords)[vertex * 2u + axis] =
				LittleFloat( texCoords[vertex * 2u + axis] );
	}
	for ( uint32_t triangle = 0u; triangle < header.numTriangles; ++triangle ) {
		neutralIqmTriangle_t tri; memcpy( &tri, bytes + header.ofsTriangles
			+ (size_t)triangle * sizeof( tri ), sizeof( tri ) );
		for ( uint32_t corner = 0u; corner < 3u; ++corner ) {
			uint32_t index = LittleLong( tri.vertex[corner] );
			if ( index >= out->vertexCount ) return qfalse;
			((uint32_t *)out->indices)[triangle * 3u + corner] = index;
		}
	}
	for ( uint32_t i = 0u; i < out->batchCount; ++i ) {
		neutralIqmMesh_t mesh; renderModelBatch_t *batch =
			(renderModelBatch_t *)&out->batches[i];
		memcpy( &mesh, bytes + header.ofsMeshes + (size_t)i * sizeof( mesh ),
			sizeof( mesh ) );
		mesh.material = LittleLong( mesh.material );
		mesh.firstVertex = LittleLong( mesh.firstVertex );
		mesh.numVertexes = LittleLong( mesh.numVertexes );
		mesh.firstTriangle = LittleLong( mesh.firstTriangle );
		mesh.numTriangles = LittleLong( mesh.numTriangles );
		if ( mesh.firstVertex > out->vertexCount
				|| mesh.numVertexes > out->vertexCount - mesh.firstVertex
				|| mesh.firstTriangle > header.numTriangles
				|| mesh.numTriangles > header.numTriangles - mesh.firstTriangle
				|| mesh.material >= header.numText ) return qfalse;
		batch->firstVertex = mesh.firstVertex; batch->vertexCount = mesh.numVertexes;
		batch->firstIndex = mesh.firstTriangle * 3u;
		batch->indexCount = mesh.numTriangles * 3u;
		const char *material = (const char *)( bytes + header.ofsText + mesh.material );
		size_t available = header.numText - mesh.material;
		size_t length = strnlen( material, available );
		if ( length == available || length >= sizeof( batch->materialName ) ) return qfalse;
		memcpy( batch->materialName, material, length + 1u );
		if ( !batch->materialName[0] )
			(void)snprintf( batch->materialName, sizeof( batch->materialName ), "*white" );
	}
	return ModelFinite( out->positions, (size_t)out->vertexCount * 3u )
		&& ModelFinite( out->texCoords, (size_t)out->vertexCount * 2u );
}

qhandle_t RenderSubmission_RegisterModelData( renderSubmissionState_t *state,
		const char *name, const void *bytes, uint32_t byteCount ) {
	renderModelSnapshot_t candidate;
	renderModelRecord_t *record;
	qhandle_t handle;
	uint64_t ownedBytes;
	if ( !state || !state->initialized || !name || !name[0]
			|| strlen( name ) >= MAX_QPATH || !bytes || !byteCount ) return 0;
	for ( uint32_t i = 0u; i < state->modelCount; ++i )
		if ( !strcasecmp( state->models[i].name, name ) )
			return state->models[i].snapshot.handle;
	if ( state->modelCount >= RENDER_SUBMISSION_MAX_MODELS ) return 0;
	memset( &candidate, 0, sizeof( candidate ) );
	if ( !DecodeMd3( bytes, byteCount, &candidate )
			&& ( FreeSnapshot( &candidate ),
				memset( &candidate, 0, sizeof( candidate ) ),
				!DecodeIqm( bytes, byteCount, &candidate ) ) ) {
		FreeSnapshot( &candidate ); return 0;
	}
	ownedBytes = SnapshotBytes( &candidate );
	if ( ownedBytes > RENDER_SUBMISSION_MAX_MODEL_BYTES - state->modelBytes ) {
		FreeSnapshot( &candidate ); return 0;
	}
	handle = RegisterModelAsset( state, name );
	if ( !handle ) { FreeSnapshot( &candidate ); return 0; }
	record = &state->models[state->modelCount++]; memset( record, 0, sizeof( *record ) );
	(void)snprintf( record->name, sizeof( record->name ), "%s", name );
	candidate.handle = handle; candidate.generation = state->nextModelGeneration++;
	candidate.ready = qtrue; candidate.digest = SnapshotDigest( name, &candidate );
	record->snapshot = candidate; state->modelBytes += (uint32_t)ownedBytes;
	state->modelDigest = ModelHash( state->modelDigest, &candidate.digest,
		sizeof( candidate.digest ) );
	return handle;
}

qhandle_t RenderSubmission_RegisterInlineModel( renderSubmissionState_t *state,
		const char *name, uint32_t firstSurface, uint32_t surfaceCount ) {
	renderModelSnapshot_t candidate;
	renderModelRecord_t *record;
	const renderWorldSnapshot_t *world;
	uint32_t batchCount = 0u, indexCount = 0u, batchCursor = 0u, indexCursor = 0u;
	uint64_t ownedBytes;
	qhandle_t handle;
	if ( !state || !state->initialized || !name || name[0] != '*'
			|| !name[1] || strlen( name ) >= MAX_QPATH || !surfaceCount
			|| firstSurface > state->worldSurfaceCount
			|| surfaceCount > state->worldSurfaceCount - firstSurface
			|| !state->worldSnapshot.ready ) return 0;
	for ( uint32_t i = 0u; i < state->modelCount; ++i )
		if ( !strcasecmp( state->models[i].name, name ) )
			return state->models[i].snapshot.handle;
	if ( state->modelCount >= RENDER_SUBMISSION_MAX_MODELS ) return 0;
	world = &state->worldSnapshot;
	for ( uint32_t i = 0u; i < world->batchCount; ++i ) {
		const renderWorldBatch_t *batch = &world->batches[i];
		if ( batch->sourceSurfaceIndex < firstSurface
				|| batch->sourceSurfaceIndex >= firstSurface + surfaceCount ) continue;
		if ( batchCount >= RENDER_SUBMISSION_MAX_MODEL_BATCHES
				|| batch->indexCount > RENDER_SUBMISSION_MAX_MODEL_INDICES - indexCount )
			return 0;
		batchCount++; indexCount += batch->indexCount;
	}
	if ( !batchCount || !indexCount || world->vertexCount > RENDER_SUBMISSION_MAX_MODEL_VERTICES )
		return 0;
	memset( &candidate, 0, sizeof( candidate ) );
	candidate.format = RENDER_MODEL_INLINE_BSP; candidate.frameCount = 1u;
	candidate.vertexCount = world->vertexCount; candidate.indexCount = indexCount;
	candidate.batchCount = batchCount;
	candidate.positions = (float *)calloc( (size_t)candidate.vertexCount * 3u,
		sizeof( float ) );
	candidate.texCoords = (float *)calloc( (size_t)candidate.vertexCount * 2u,
		sizeof( float ) );
	candidate.indices = (uint32_t *)calloc( indexCount, sizeof( uint32_t ) );
	candidate.batches = (renderModelBatch_t *)calloc( batchCount,
		sizeof( renderModelBatch_t ) );
	if ( !candidate.positions || !candidate.texCoords || !candidate.indices
			|| !candidate.batches ) { FreeSnapshot( &candidate ); return 0; }
	for ( uint32_t vertex = 0u; vertex < candidate.vertexCount; ++vertex ) {
		memcpy( (float *)candidate.positions + (size_t)vertex * 3u,
			world->vertices[vertex].position, 3u * sizeof( float ) );
		memcpy( (float *)candidate.texCoords + (size_t)vertex * 2u,
			world->vertices[vertex].texCoord, 2u * sizeof( float ) );
	}
	for ( uint32_t i = 0u; i < world->batchCount; ++i ) {
		const renderWorldBatch_t *source = &world->batches[i];
		renderModelBatch_t *batch;
		if ( source->sourceSurfaceIndex < firstSurface
				|| source->sourceSurfaceIndex >= firstSurface + surfaceCount ) continue;
		batch = &((renderModelBatch_t *)candidate.batches)[batchCursor++];
		batch->firstVertex = 0u; batch->vertexCount = candidate.vertexCount;
		batch->firstIndex = indexCursor; batch->indexCount = source->indexCount;
		batch->material = source->baseMaterial;
		(void)snprintf( batch->materialName, sizeof( batch->materialName ), "*inline" );
		memcpy( (uint32_t *)candidate.indices + indexCursor,
			world->indices + source->firstIndex,
			(size_t)source->indexCount * sizeof( uint32_t ) );
		indexCursor += source->indexCount;
	}
	ownedBytes = SnapshotBytes( &candidate );
	if ( ownedBytes > RENDER_SUBMISSION_MAX_MODEL_BYTES - state->modelBytes ) {
		FreeSnapshot( &candidate ); return 0;
	}
	handle = RegisterModelAsset( state, name );
	if ( !handle ) { FreeSnapshot( &candidate ); return 0; }
	record = &state->models[state->modelCount++]; memset( record, 0, sizeof( *record ) );
	(void)snprintf( record->name, sizeof( record->name ), "%s", name );
	candidate.handle = handle; candidate.generation = state->nextModelGeneration++;
	candidate.ready = qtrue; candidate.digest = SnapshotDigest( name, &candidate );
	record->snapshot = candidate; state->modelBytes += (uint32_t)ownedBytes;
	state->modelDigest = ModelHash( state->modelDigest, &candidate.digest,
		sizeof( candidate.digest ) );
	return handle;
}

qboolean RenderSubmission_SetModelBatchMaterial( renderSubmissionState_t *state,
		qhandle_t model, uint32_t batchIndex, qhandle_t material ) {
	if ( !state || !state->initialized || model <= 0 || material <= 0
			|| state->nextModelGeneration == UINT64_MAX ) return qfalse;
	for ( uint32_t i = 0u; i < state->modelCount; ++i ) {
		renderModelSnapshot_t *snapshot = &state->models[i].snapshot;
		if ( snapshot->handle != model ) continue;
		if ( batchIndex >= snapshot->batchCount ) return qfalse;
		((renderModelBatch_t *)snapshot->batches)[batchIndex].material = material;
		snapshot->generation = state->nextModelGeneration++;
		snapshot->digest = SnapshotDigest( state->models[i].name, snapshot );
		state->modelDigest = MODEL_FNV_OFFSET;
		for ( uint32_t j = 0u; j < state->modelCount; ++j )
			state->modelDigest = ModelHash( state->modelDigest,
				&state->models[j].snapshot.digest, sizeof( uint64_t ) );
		return qtrue;
	}
	return qfalse;
}

qboolean RenderSubmission_ModelSnapshot( const renderSubmissionState_t *state,
		qhandle_t handle, renderModelSnapshot_t *outSnapshot ) {
	if ( !state || !state->initialized || handle <= 0 || !outSnapshot ) return qfalse;
	for ( uint32_t i = 0u; i < state->modelCount; ++i ) {
		if ( state->models[i].snapshot.handle != handle ) continue;
		*outSnapshot = state->models[i].snapshot; return qtrue;
	}
	return qfalse;
}

const renderEntityCommand_t *RenderSubmission_EntityCommands(
		const renderSubmissionState_t *state, uint32_t *outCount ) {
	if ( outCount ) *outCount = 0u;
	if ( !state || !state->initialized
			|| ( !state->frameOpen && !state->frameSealed ) || !outCount
			|| !state->entityCount ) return NULL;
	*outCount = state->entityCount;
	return state->entities;
}
