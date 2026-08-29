// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "render_submission.h"
#include "maps/map_format_registry.h"

#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define FNV_OFFSET UINT64_C( 14695981039346656037 )
#define FNV_PRIME  UINT64_C( 1099511628211 )
#define PATCH_COORD_LIMIT_Q16 ( 1 << 29 )

static qboolean PositionQ16( float value, int32_t *outValue );

static uint64_t HashByte( uint64_t digest, uint8_t value )
{
	return ( digest ^ value ) * FNV_PRIME;
}

static uint64_t HashU32( uint64_t digest, uint32_t value )
{
	uint32_t byteIndex;
	for ( byteIndex = 0u; byteIndex < 4u; ++byteIndex )
		digest = HashByte( digest, (uint8_t)( value >> ( byteIndex * 8u ) ) );
	return digest;
}

static uint64_t HashU64( uint64_t digest, uint64_t value )
{
	uint32_t byteIndex;
	for ( byteIndex = 0u; byteIndex < 8u; ++byteIndex )
		digest = HashByte( digest, (uint8_t)( value >> ( byteIndex * 8u ) ) );
	return digest;
}

static void RebuildIrradianceVolumeDigest( renderSubmissionState_t *state )
{
	uint64_t digest = FNV_OFFSET;
	uint32_t index;
	for ( index = 0u; index < state->irradianceVolumeCount; ++index ) {
		const renderIrradianceVolumeRecord_t *record = &state->irradianceVolumes[index];
		digest = HashU64( digest, Ral_IrradianceVolumeLayoutHash( &record->placement ) );
		digest = HashU64( digest, record->artifact.manifestHash );
		digest = HashU64( digest, record->artifact.byteLength );
	}
	state->irradianceVolumeDigest = digest ? digest : 1u;
}

qboolean RenderSubmission_ClearDirectionalLighting( renderSubmissionState_t *state )
{
	if ( !state || state->frameOpen )
		return qfalse;
	free( state->directionalLighting.ownedArtifactBytes );
	memset( &state->directionalLighting, 0, sizeof( state->directionalLighting ) );
	state->directionalLightingCount = 0u;
	state->directionalLightingDigest = FNV_OFFSET;
	return qtrue;
}

qboolean RenderSubmission_RegisterDirectionalLighting(
	renderSubmissionState_t *state, const void *artifactBytes,
	uint64_t artifactByteLength )
{
	ralLightingArtifactReceipt_t sourceReceipt, ownedReceipt;
	ralLightingPayloadView_t sourcePayloads[RAL_LIGHTING_ARTIFACT_MAX_PAYLOADS];
	ralLightingPayloadView_t ownedPayloads[RAL_LIGHTING_ARTIFACT_MAX_PAYLOADS];
	void *ownedBytes;
	if ( !state || !state->initialized || state->frameOpen || state->directionalLightingCount ||
		 !artifactBytes || !artifactByteLength ||
		 !Ral_LightingArtifactRead( artifactBytes, artifactByteLength, &sourceReceipt, sourcePayloads ) ||
		 sourceReceipt.kind != RAL_LIGHTING_ARTIFACT_DIRECTIONAL_LIGHTMAP )
		return qfalse;
	ownedBytes = malloc( (size_t)artifactByteLength );
	if ( !ownedBytes )
		return qfalse;
	memcpy( ownedBytes, artifactBytes, (size_t)artifactByteLength );
	if ( !Ral_LightingArtifactRead( ownedBytes, artifactByteLength, &ownedReceipt, ownedPayloads ) ||
		 ownedReceipt.kind != RAL_LIGHTING_ARTIFACT_DIRECTIONAL_LIGHTMAP ) {
		free( ownedBytes );
		return qfalse;
	}
	state->directionalLighting.artifact = ownedReceipt;
	state->directionalLighting.ownedArtifactBytes = ownedBytes;
	state->directionalLighting.artifactByteLength = artifactByteLength;
	memcpy( state->directionalLighting.payloads, ownedPayloads, sizeof( ownedPayloads ) );
	state->directionalLightingCount = 1u;
	state->directionalLightingDigest = HashU64( HashU64( FNV_OFFSET, ownedReceipt.manifestHash ), artifactByteLength );
	if ( !state->directionalLightingDigest )
		state->directionalLightingDigest = 1u;
	return qtrue;
}

qboolean RenderSubmission_DirectionalLightingSnapshot(
	const renderSubmissionState_t *state,
	const renderDirectionalLightingRecord_t **outLighting, uint64_t *outDigest )
{
	if ( !state || !state->initialized || !outLighting || !outDigest ||
		 !state->directionalLightingDigest )
		return qfalse;
	*outLighting = state->directionalLightingCount ? &state->directionalLighting : NULL;
	*outDigest = state->directionalLightingDigest;
	return qtrue;
}

qboolean RenderSubmission_ClearIrradianceVolumes( renderSubmissionState_t *state )
{
	uint32_t index;
	if ( !state || state->frameOpen )
		return qfalse;
	for ( index = 0u; index < state->irradianceVolumeCount; ++index )
		free( state->irradianceVolumes[index].ownedArtifactBytes );
	memset( state->irradianceVolumes, 0, sizeof( state->irradianceVolumes ) );
	state->irradianceVolumeCount = 0u;
	state->irradianceVolumeDigest = FNV_OFFSET;
	return qtrue;
}

static qboolean BuildIrradianceVolumeRecord(
	const ralIrradianceVolumePlacement_t *placement, const void *artifactBytes,
	uint64_t artifactByteLength, renderIrradianceVolumeRecord_t *outRecord )
{
	ralLightingArtifactReceipt_t sourceReceipt, ownedReceipt;
	ralLightingPayloadView_t sourcePayloads[RAL_LIGHTING_ARTIFACT_MAX_PAYLOADS];
	ralLightingPayloadView_t ownedPayloads[RAL_LIGHTING_ARTIFACT_MAX_PAYLOADS];
	ralIrradianceProbeVolume_t volume;
	renderIrradianceVolumeRecord_t record;
	void *ownedBytes;
	if ( !placement || !artifactBytes || !artifactByteLength || !outRecord ||
		 !Ral_LightingArtifactRead( artifactBytes, artifactByteLength, &sourceReceipt, sourcePayloads ) ||
		 sourceReceipt.artifactGeneration != placement->sourceGeneration ||
		 !Ral_IrradianceRuntimeVolumeBuild( placement, &sourceReceipt, &volume ) )
		return qfalse;
	ownedBytes = malloc( (size_t)artifactByteLength );
	if ( !ownedBytes )
		return qfalse;
	memcpy( ownedBytes, artifactBytes, (size_t)artifactByteLength );
	if ( !Ral_LightingArtifactRead( ownedBytes, artifactByteLength, &ownedReceipt, ownedPayloads ) ||
		 !Ral_IrradianceRuntimeVolumeBuild( placement, &ownedReceipt, &volume ) ||
		 ownedPayloads[0].role != RAL_LIGHTING_PAYLOAD_SH_COEFFICIENTS ||
		 ownedPayloads[1].role != RAL_LIGHTING_PAYLOAD_PROBE_VALIDITY ||
		 ownedPayloads[1].byteLength > UINT32_MAX ) {
		free( ownedBytes );
		return qfalse;
	}
	memset( &record, 0, sizeof( record ) );
	record.placement = *placement;
	record.artifact = ownedReceipt;
	record.volume = volume;
	record.ownedArtifactBytes = ownedBytes;
	record.artifactByteLength = artifactByteLength;
	record.coefficientBytes = ownedPayloads[0].bytes;
	record.coefficientByteLength = ownedPayloads[0].byteLength;
	record.validityBytes = (const uint8_t *)ownedPayloads[1].bytes;
	record.validityCount = (uint32_t)ownedPayloads[1].byteLength;
	*outRecord = record;
	return qtrue;
}

static qboolean IrradianceRecordBefore( const renderIrradianceVolumeRecord_t *a,
	const renderIrradianceVolumeRecord_t *b )
{
	return a->placement.priority > b->placement.priority ||
		( a->placement.priority == b->placement.priority &&
		  a->placement.volumeId < b->placement.volumeId );
}

qboolean RenderSubmission_ReplaceIrradianceVolumes(
	renderSubmissionState_t *state, const renderIrradianceVolumeSource_t *sources,
	uint32_t sourceCount )
{
	renderIrradianceVolumeRecord_t records[RENDER_SUBMISSION_MAX_IRRADIANCE_VOLUMES];
	uint32_t built = 0u, index, prior;
	if ( !state || !state->initialized || state->frameOpen ||
		 sourceCount > RENDER_SUBMISSION_MAX_IRRADIANCE_VOLUMES ||
		 ( sourceCount && !sources ) )
		return qfalse;
	memset( records, 0, sizeof( records ) );
	for ( index = 0u; index < sourceCount; ++index ) {
		for ( prior = 0u; prior < index; ++prior )
			if ( sources[prior].placement.volumeId == sources[index].placement.volumeId )
				goto fail;
		if ( !BuildIrradianceVolumeRecord( &sources[index].placement,
			sources[index].artifactBytes, sources[index].artifactByteLength, &records[index] ) )
			goto fail;
		built++;
	}
	for ( index = 1u; index < sourceCount; ++index ) {
		renderIrradianceVolumeRecord_t value = records[index];
		prior = index;
		while ( prior && IrradianceRecordBefore( &value, &records[prior - 1u] ) ) {
			records[prior] = records[prior - 1u];
			prior--;
		}
		records[prior] = value;
	}
	if ( !RenderSubmission_ClearIrradianceVolumes( state ) )
		goto fail;
	memcpy( state->irradianceVolumes, records,
		(size_t)sourceCount * sizeof( records[0] ) );
	state->irradianceVolumeCount = sourceCount;
	RebuildIrradianceVolumeDigest( state );
	return qtrue;
fail:
	for ( index = 0u; index < built; ++index )
		free( records[index].ownedArtifactBytes );
	return qfalse;
}

qboolean RenderSubmission_RegisterIrradianceVolume(
	renderSubmissionState_t *state, const ralIrradianceVolumePlacement_t *placement,
	const void *artifactBytes, uint64_t artifactByteLength )
{
	renderIrradianceVolumeRecord_t record;
	uint32_t index, insertAt;
	if ( !state || !state->initialized || state->frameOpen || !placement || !artifactBytes ||
		 !artifactByteLength || state->irradianceVolumeCount >= RENDER_SUBMISSION_MAX_IRRADIANCE_VOLUMES )
		return qfalse;
	insertAt = state->irradianceVolumeCount;
	for ( index = 0u; index < state->irradianceVolumeCount; ++index ) {
		const ralIrradianceVolumePlacement_t *current = &state->irradianceVolumes[index].placement;
		if ( current->volumeId == placement->volumeId )
			return qfalse;
		if ( insertAt == state->irradianceVolumeCount &&
			 ( placement->priority > current->priority ||
			   ( placement->priority == current->priority && placement->volumeId < current->volumeId ) ) )
			insertAt = index;
	}
	if ( !BuildIrradianceVolumeRecord( placement, artifactBytes, artifactByteLength, &record ) )
		return qfalse;
	if ( insertAt < state->irradianceVolumeCount )
		memmove( &state->irradianceVolumes[insertAt + 1u], &state->irradianceVolumes[insertAt],
			(size_t)( state->irradianceVolumeCount - insertAt ) * sizeof( state->irradianceVolumes[0] ) );
	state->irradianceVolumes[insertAt] = record;
	state->irradianceVolumeCount++;
	RebuildIrradianceVolumeDigest( state );
	return qtrue;
}

qboolean RenderSubmission_AttachConfiguredEntityIrradiance(
	renderSubmissionState_t *state, uint32_t entityIndex,
	const ralLightVec3Q16_t *referenceNormal,
	const int32_t fallbackIrradianceQ16[3] )
{
	ralIrradianceVolumePlacement_t placements[RENDER_SUBMISSION_MAX_IRRADIANCE_VOLUMES];
	ralIrradianceVolumeSelectionReceipt_t selection;
	ralIrradianceEntitySampleRequest_t request;
	ralIrradianceEntitySampleReceipt_t receipt;
	const renderIrradianceVolumeRecord_t *record;
	uint32_t index, selectedIndex = 0u;
	if ( !state || !state->frameOpen || !state->currentFrameGeneration ||
		 entityIndex >= state->entityCount || !state->irradianceVolumeCount ||
		 !referenceNormal || !fallbackIrradianceQ16 )
		return qfalse;
	memset( &request, 0, sizeof( request ) );
	request.schemaVersion = RAL_IRRADIANCE_ENTITY_SAMPLE_SCHEMA_VERSION;
	request.queryGeneration = state->currentFrameGeneration;
	request.entityId = (uint64_t)entityIndex + 1u;
	if ( !PositionQ16( state->entities[entityIndex].entity.origin[0], &request.position.x ) ||
		 !PositionQ16( state->entities[entityIndex].entity.origin[1], &request.position.y ) ||
		 !PositionQ16( state->entities[entityIndex].entity.origin[2], &request.position.z ) )
		return qfalse;
	request.normal = *referenceNormal;
	memcpy( request.fallbackIrradianceQ16, fallbackIrradianceQ16, sizeof( request.fallbackIrradianceQ16 ) );
	for ( index = 0u; index < state->irradianceVolumeCount; ++index )
		placements[index] = state->irradianceVolumes[index].placement;
	if ( Ral_IrradianceVolumeSelect( placements, state->irradianceVolumeCount, &request.position, &selection ) )
		selectedIndex = selection.volumeIndex;
	record = &state->irradianceVolumes[selectedIndex];
	if ( !Ral_IrradianceEntitySample( &record->placement, &record->volume, &request,
		 record->coefficientBytes, record->coefficientByteLength, record->validityBytes,
		 record->validityCount, &receipt ) )
		return qfalse;
	return RenderSubmission_AttachEntityIrradiance( state, entityIndex, &receipt );
}

static const char *LegacyLightGridQuotedToken( const char *cursor,
	const char *end, char *out, size_t capacity )
{
	size_t length = 0u;
	if ( !cursor || !end || !out || capacity < 2u ) return NULL;
	while ( cursor < end && ( *cursor == ' ' || *cursor == '\t'
			|| *cursor == '\r' || *cursor == '\n' ) ) cursor++;
	if ( cursor >= end || *cursor != '"' ) return NULL;
	cursor++;
	while ( cursor < end && *cursor != '"' ) {
		if ( length + 1u >= capacity ) return NULL;
		out[length++] = *cursor++;
	}
	if ( cursor >= end || *cursor != '"' ) return NULL;
	out[length] = '\0';
	return cursor + 1;
}

static void LegacyLightGridSize( const mapFile_t *world, float outSize[3] )
{
	const char *cursor, *end;
	char key[MAX_TOKEN_CHARS], value[MAX_TOKEN_CHARS];
	outSize[0] = 64.0f; outSize[1] = 64.0f; outSize[2] = 128.0f;
	if ( !world || !world->entityString || world->entityStringLength <= 0 ) return;
	cursor = world->entityString;
	end = cursor + world->entityStringLength;
	while ( cursor < end && *cursor != '{' ) cursor++;
	if ( cursor >= end ) return;
	cursor++;
	for ( ;; ) {
		float x, y, z;
		while ( cursor < end && ( *cursor == ' ' || *cursor == '\t'
				|| *cursor == '\r' || *cursor == '\n' ) ) cursor++;
		if ( cursor >= end || *cursor == '}' ) return;
		cursor = LegacyLightGridQuotedToken( cursor, end, key, sizeof( key ) );
		if ( !cursor ) return;
		cursor = LegacyLightGridQuotedToken( cursor, end, value, sizeof( value ) );
		if ( !cursor ) return;
		if ( !strcasecmp( key, "gridsize" )
				&& sscanf( value, "%f %f %f", &x, &y, &z ) == 3
				&& isfinite( x ) && isfinite( y ) && isfinite( z )
				&& x > 0.0f && y > 0.0f && z > 0.0f ) {
			outSize[0] = x; outSize[1] = y; outSize[2] = z;
			return;
		}
	}
}

static void LegacyLightGridShiftRgb( const byte *sample, float outRgb[3] )
{
	int rgb[3] = { sample[0] * 2, sample[1] * 2, sample[2] * 2 };
	int maximum = rgb[0] > rgb[1] ? rgb[0] : rgb[1];
	if ( rgb[2] > maximum ) maximum = rgb[2];
	if ( maximum > 255 ) {
		rgb[0] = rgb[0] * 255 / maximum;
		rgb[1] = rgb[1] * 255 / maximum;
		rgb[2] = rgb[2] * 255 / maximum;
	}
	outRgb[0] = (float)rgb[0]; outRgb[1] = (float)rgb[1];
	outRgb[2] = (float)rgb[2];
}

qboolean RenderSubmission_AttachLegacyLightGridEntityIrradiance(
	renderSubmissionState_t *state, uint32_t entityIndex,
	float ambientScale, float directedScale )
{
	const mapFile_t *world;
	const refEntity_t *entity;
	float size[3], origin[3], fraction[3];
	float ambient[3] = { 0.0f, 0.0f, 0.0f };
	float directed[3] = { 0.0f, 0.0f, 0.0f };
	float direction[3] = { 0.0f, 0.0f, 0.0f };
	uint32_t bounds[3], position[3], steps[3], corner, axis;
	uint64_t expectedPoints;
	float total = 0.0f, directionLength;
	ralIrradianceEntitySampleReceipt_t receipt;
	if ( !state || !state->frameOpen || entityIndex >= state->entityCount
			|| !isfinite( ambientScale ) || !isfinite( directedScale )
			|| ambientScale < 0.0f || directedScale < 0.0f ) return qfalse;
	world = state->worldMap;
	if ( !world || !world->lightGridData || world->numGridPoints <= 0
			|| !world->subModels || world->numSubModels <= 0 ) return qfalse;
	LegacyLightGridSize( world, size );
	for ( axis = 0u; axis < 3u; ++axis ) {
		float maximum;
		origin[axis] = size[axis] * ceilf( world->subModels[0].mins[axis] / size[axis] );
		maximum = size[axis] * floorf( world->subModels[0].maxs[axis] / size[axis] );
		if ( maximum < origin[axis] ) return qfalse;
		bounds[axis] = (uint32_t)( ( maximum - origin[axis] ) / size[axis] ) + 1u;
		if ( !bounds[axis] ) return qfalse;
	}
	expectedPoints = (uint64_t)bounds[0] * bounds[1] * bounds[2];
	if ( expectedPoints != (uint32_t)world->numGridPoints ) return qfalse;
	entity = &state->entities[entityIndex].entity;
	for ( axis = 0u; axis < 3u; ++axis ) {
		float coordinate = ( ( entity->renderfx & RF_LIGHTING_ORIGIN )
			? entity->lightingOrigin[axis] : entity->origin[axis] ) - origin[axis];
		float cell = coordinate / size[axis];
		int base = (int)floorf( cell );
		fraction[axis] = cell - (float)base;
		if ( base < 0 ) base = 0;
		else if ( base >= (int)bounds[axis] ) base = (int)bounds[axis] - 1;
		position[axis] = (uint32_t)base;
	}
	steps[0] = 1u; steps[1] = bounds[0]; steps[2] = bounds[0] * bounds[1];
	for ( corner = 0u; corner < 8u; ++corner ) {
		uint32_t index = position[0] + position[1] * steps[1]
			+ position[2] * steps[2];
		float weight = 1.0f, ambientRgb[3], directedRgb[3], normal[3];
		const byte *sample;
		for ( axis = 0u; axis < 3u; ++axis ) {
			if ( corner & ( 1u << axis ) ) {
				if ( position[axis] + 1u >= bounds[axis] ) { weight = 0.0f; break; }
				weight *= fraction[axis]; index += steps[axis];
			} else weight *= 1.0f - fraction[axis];
		}
		if ( weight <= 0.0f ) continue;
		sample = world->lightGridData + (size_t)index * 8u;
		if ( !( sample[0] + sample[1] + sample[2] ) ) continue;
		LegacyLightGridShiftRgb( sample, ambientRgb );
		LegacyLightGridShiftRgb( sample + 3, directedRgb );
		normal[0] = cosf( (float)sample[7] * ( 6.28318530718f / 256.0f ) )
			* sinf( (float)sample[6] * ( 6.28318530718f / 256.0f ) );
		normal[1] = sinf( (float)sample[7] * ( 6.28318530718f / 256.0f ) )
			* sinf( (float)sample[6] * ( 6.28318530718f / 256.0f ) );
		normal[2] = cosf( (float)sample[6] * ( 6.28318530718f / 256.0f ) );
		total += weight;
		for ( axis = 0u; axis < 3u; ++axis ) {
			ambient[axis] += weight * ambientRgb[axis];
			directed[axis] += weight * directedRgb[axis];
			direction[axis] += weight * normal[axis];
		}
	}
	if ( total > 0.0f && total < 0.99f ) {
		for ( axis = 0u; axis < 3u; ++axis ) {
			ambient[axis] /= total; directed[axis] /= total;
		}
	}
	directionLength = sqrtf( direction[0] * direction[0]
		+ direction[1] * direction[1] + direction[2] * direction[2] );
	if ( directionLength > 0.00001f )
		for ( axis = 0u; axis < 3u; ++axis ) direction[axis] /= directionLength;
	else direction[2] = 1.0f;
	memset( &receipt, 0, sizeof( receipt ) );
	receipt.schemaVersion = RAL_IRRADIANCE_ENTITY_RECEIPT_SCHEMA_VERSION;
	receipt.queryGeneration = state->currentFrameGeneration;
	receipt.entityId = (uint64_t)entityIndex + 1u;
	receipt.volumeId = state->worldDigest ? state->worldDigest : 1u;
	receipt.layoutHash = expectedPoints ^ ( (uint64_t)bounds[0] << 42u )
		^ ( (uint64_t)bounds[1] << 21u ) ^ bounds[2];
	if ( !receipt.layoutHash ) receipt.layoutHash = 1u;
	receipt.probes.schemaVersion = RAL_IRRADIANCE_RECEIPT_SCHEMA_VERSION;
	receipt.probes.queryGeneration = receipt.queryGeneration;
	receipt.probes.productGeneration = receipt.volumeId;
	receipt.probes.sampleCount = 1u;
	receipt.probes.probeIndices[0] = position[0] + position[1] * steps[1]
		+ position[2] * steps[2];
	receipt.probes.weightsQ16[0] = RAL_LIGHT_Q16_ONE;
	receipt.probes.fallback = RAL_IRRADIANCE_FALLBACK_LIGHTGRID;
	receipt.probes.ready = qtrue;
	for ( axis = 0u; axis < 3u; ++axis ) {
		float ambientValue = fminf( ambient[axis] * ambientScale + 32.0f, 255.0f ) / 255.0f;
		float directedValue = directed[axis] * directedScale / 255.0f;
		receipt.blendedCoefficientsQ16[0][axis] = (int32_t)lrintf(
			ambientValue * (float)RAL_LIGHT_Q16_ONE );
		for ( uint32_t coefficient = 1u; coefficient < 4u; ++coefficient )
			receipt.blendedCoefficientsQ16[coefficient][axis] = (int32_t)lrintf(
				directedValue * direction[coefficient - 1u]
				* (float)RAL_LIGHT_Q16_ONE );
		receipt.diffuseIrradianceQ16[axis] = receipt.blendedCoefficientsQ16[0][axis];
	}
	receipt.contributorHash = receipt.layoutHash ^ receipt.entityId;
	if ( !receipt.contributorHash ) receipt.contributorHash = 1u;
	receipt.coefficientHash = Ral_IrradianceCoefficientHash(
		receipt.blendedCoefficientsQ16 );
	receipt.ready = qtrue;
	return RenderSubmission_AttachEntityIrradiance( state, entityIndex, &receipt );
}

qboolean RenderSubmission_IrradianceVolumeSnapshot(
	const renderSubmissionState_t *state,
	const renderIrradianceVolumeRecord_t **outVolumes, uint32_t *outVolumeCount,
	uint64_t *outDigest )
{
	if ( !state || !state->initialized || !outVolumes || !outVolumeCount || !outDigest ||
		 !state->irradianceVolumeDigest )
		return qfalse;
	*outVolumes = state->irradianceVolumeCount ? state->irradianceVolumes : NULL;
	*outVolumeCount = state->irradianceVolumeCount;
	*outDigest = state->irradianceVolumeDigest;
	return qtrue;
}

static qboolean PositionQ16( float value, int32_t *outValue )
{
	double scaled;
	int64_t rounded;
	if ( !outValue || !isfinite( value ) )
		return qfalse;
	scaled  = (double)value * (double)RAL_LIGHT_Q16_ONE;
	rounded = (int64_t)( scaled < 0.0 ? scaled - 0.5 : scaled + 0.5 );
	if ( rounded <= -PATCH_COORD_LIMIT_Q16 || rounded >= PATCH_COORD_LIMIT_Q16 || rounded < INT32_MIN ||
		 rounded > INT32_MAX )
		return qfalse;
	*outValue = (int32_t)rounded;
	return qtrue;
}

static qboolean TriangleVertices( const renderWorldSnapshot_t *world, uint32_t firstIndex,
								 ralLightVec3Q16_t outVertices[3] )
{
	uint32_t corner, axis;
	for ( corner = 0u; corner < 3u; ++corner ) {
		uint32_t vertexIndex;
		if ( firstIndex + corner >= world->indexCount )
			return qfalse;
		vertexIndex = world->indices[firstIndex + corner];
		if ( vertexIndex >= world->vertexCount )
			return qfalse;
		for ( axis = 0u; axis < 3u; ++axis ) {
			int32_t *target = axis == 0u ? &outVertices[corner].x
									 : ( axis == 1u ? &outVertices[corner].y : &outVertices[corner].z );
			if ( !PositionQ16( world->vertices[vertexIndex].position[axis], target ) )
				return qfalse;
		}
	}
	return qtrue;
}

static qboolean MaterialLighting( const renderSubmissionState_t *state, qhandle_t handle,
								  renderMaterialSnapshot_t *outMaterial )
{
	return handle > 0 && RenderSubmission_MaterialSnapshot( state, handle, outMaterial ) &&
		   outMaterial->lighting.schemaVersion == RENDER_MATERIAL_LIGHTING_SCHEMA_VERSION &&
		   outMaterial->lighting.sourceGeneration && outMaterial->lighting.sourceGeneration != UINT64_MAX &&
		   outMaterial->lighting.provenanceHash && outMaterial->lighting.provenanceHash != UINT64_MAX &&
		   outMaterial->lighting.ready == qtrue;
}

static qboolean EmissiveMaterial( const renderMaterialSnapshot_t *material )
{
	return material && ( material->lighting.emissionRadianceQ16[0] ||
		material->lighting.emissionRadianceQ16[1] || material->lighting.emissionRadianceQ16[2] );
}

static qboolean EmissiveSurfaceBuild( const renderWorldSnapshot_t *world,
	const renderWorldBatch_t *batch, const renderMaterialSnapshot_t *material,
	ralEmissiveSurface_t *outSurface )
{
	ralEmissiveSurface_t surface;
	double minValue[3] = { 0.0, 0.0, 0.0 }, maxValue[3] = { 0.0, 0.0, 0.0 };
	double normal[3] = { 0.0, 0.0, 0.0 }, area = 0.0;
	uint32_t localIndex, axis;
	qboolean first = qtrue;
	if ( !world || !batch || !material || !outSurface || !EmissiveMaterial( material ) ||
		 !batch->indexCount || batch->indexCount % 3u || batch->firstIndex > world->indexCount ||
		 batch->indexCount > world->indexCount - batch->firstIndex )
		return qfalse;
	for ( localIndex = 0u; localIndex < batch->indexCount; localIndex += 3u ) {
		double p[3][3], edgeA[3], edgeB[3], cross[3], magnitude;
		uint32_t corner;
		for ( corner = 0u; corner < 3u; ++corner ) {
			uint32_t vertexIndex = world->indices[batch->firstIndex + localIndex + corner];
			if ( vertexIndex >= world->vertexCount ) return qfalse;
			for ( axis = 0u; axis < 3u; ++axis ) {
				p[corner][axis] = world->vertices[vertexIndex].position[axis];
				if ( !isfinite( p[corner][axis] ) ) return qfalse;
				if ( first || p[corner][axis] < minValue[axis] ) minValue[axis] = p[corner][axis];
				if ( first || p[corner][axis] > maxValue[axis] ) maxValue[axis] = p[corner][axis];
			}
			first = qfalse;
		}
		for ( axis = 0u; axis < 3u; ++axis ) {
			edgeA[axis] = p[1][axis] - p[0][axis];
			edgeB[axis] = p[2][axis] - p[0][axis];
		}
		cross[0] = edgeA[1] * edgeB[2] - edgeA[2] * edgeB[1];
		cross[1] = edgeA[2] * edgeB[0] - edgeA[0] * edgeB[2];
		cross[2] = edgeA[0] * edgeB[1] - edgeA[1] * edgeB[0];
		magnitude = sqrt( cross[0] * cross[0] + cross[1] * cross[1] + cross[2] * cross[2] );
		if ( !isfinite( magnitude ) || magnitude <= 0.0 ) return qfalse;
		area += magnitude * 0.5;
		normal[0] += cross[0]; normal[1] += cross[1]; normal[2] += cross[2];
	}
	{
		double magnitude = sqrt( normal[0] * normal[0] + normal[1] * normal[1] + normal[2] * normal[2] );
		int64_t areaQ16;
		if ( !isfinite( area ) || !isfinite( magnitude ) || area <= 0.0 || magnitude <= 0.0 )
			return qfalse;
		memset( &surface, 0, sizeof( surface ) );
		for ( axis = 0u; axis < 3u; ++axis ) {
			int32_t *minimum = axis == 0u ? &surface.boundsMin.x :
				( axis == 1u ? &surface.boundsMin.y : &surface.boundsMin.z );
			int32_t *maximum = axis == 0u ? &surface.boundsMax.x :
				( axis == 1u ? &surface.boundsMax.y : &surface.boundsMax.z );
			int32_t *normalAxis = axis == 0u ? &surface.normal.x :
				( axis == 1u ? &surface.normal.y : &surface.normal.z );
			if ( !PositionQ16( (float)minValue[axis], minimum ) ||
				 !PositionQ16( (float)maxValue[axis], maximum ) ) return qfalse;
			if ( *minimum == *maximum ) {
				if ( *minimum <= -PATCH_COORD_LIMIT_Q16 + 1 || *maximum >= PATCH_COORD_LIMIT_Q16 - 1 )
					return qfalse;
				( *minimum )--; ( *maximum )++;
			}
			*normalAxis = (int32_t)( normal[axis] / magnitude * RAL_LIGHT_Q16_ONE );
		}
		areaQ16 = (int64_t)( area * RAL_LIGHT_Q16_ONE + 0.5 );
		if ( areaQ16 <= 0 ) areaQ16 = 1;
		if ( areaQ16 > INT32_MAX ) areaQ16 = INT32_MAX;
		surface.areaQ16 = (int32_t)areaQ16;
	}
	surface.schemaVersion = RAL_EMISSIVE_PROXY_SCHEMA_VERSION;
	surface.surfaceId = (uint64_t)batch->sourceSurfaceIndex + 1u;
	surface.sourceGeneration = material->lighting.sourceGeneration;
	surface.provenanceHash = material->lighting.provenanceHash;
	surface.materialArtifactHash = material->digest;
	surface.mobility = material->lighting.emissiveMobility;
	memcpy( surface.radianceQ16, material->lighting.emissionRadianceQ16,
		sizeof( surface.radianceQ16 ) );
	surface.influenceRangeQ16 = material->lighting.emissiveInfluenceRangeQ16;
	surface.shadowPriority = material->lighting.emissiveShadowPriority;
	surface.requestedProxyCount = material->lighting.emissiveRequestedProxyCount;
	surface.explicitProxyAuthority = material->lighting.emissiveExplicitProxyAuthority;
	surface.contributesToBake = material->lighting.participatesInStaticBake;
	surface.injectsAtmosphere = material->lighting.emissiveInjectsAtmosphere;
	*outSurface = surface;
	return qtrue;
}

qboolean RenderSubmission_EmissiveRoutingReceiptValid(
	const renderEmissiveRoutingReceipt_t *receipt )
{
	return receipt && receipt->schemaVersion == RENDER_EMISSIVE_ROUTING_SCHEMA_VERSION &&
		receipt->routingGeneration && receipt->routingGeneration != UINT64_MAX &&
		receipt->worldDigest && receipt->materialDigest && receipt->routeDigest &&
		receipt->routeCount <= RENDER_SUBMISSION_MAX_EMISSIVE_ROUTES &&
		receipt->proxyLightCount <= receipt->routeCount * RAL_EMISSIVE_PROXY_MAX_PER_SURFACE &&
		receipt->directRouteCount <= receipt->routeCount &&
		receipt->bakeRouteCount <= receipt->routeCount &&
		receipt->bloomRouteCount == receipt->routeCount &&
		receipt->atmosphereRouteCount <= receipt->routeCount &&
		receipt->rejectedProxyRouteCount <= receipt->routeCount && receipt->ready == qtrue;
}

qboolean RenderSubmission_BuildEmissiveRoutes(
	const renderSubmissionState_t *state, uint64_t routingGeneration,
	const ralEmissiveProxyPolicy_t *policy,
	ralEmissiveRouteReceipt_t *outRoutes, uint32_t routeCapacity,
	ralLightDescription_t *outProxyLights, uint32_t proxyCapacity,
	renderEmissiveRoutingReceipt_t *outReceipt )
{
	renderEmissiveRoutingReceipt_t receipt;
	ralEmissiveRouteReceipt_t *routes;
	ralLightDescription_t *proxies;
	ralEmissiveProxyPolicy_t remainingPolicy;
	uint32_t batchIndex, routeCount = 0u, proxyCount = 0u;
	uint64_t digest = FNV_OFFSET;
	if ( !state || !state->initialized || !state->worldLoaded || !state->worldSnapshot.ready ||
		 !routingGeneration || routingGeneration == UINT64_MAX || !policy || !outReceipt ||
		 !routeCapacity || routeCapacity > RENDER_SUBMISSION_MAX_EMISSIVE_ROUTES || !outRoutes ||
		 !proxyCapacity || proxyCapacity > RENDER_SUBMISSION_MAX_EMISSIVE_ROUTES * RAL_EMISSIVE_PROXY_MAX_PER_SURFACE ||
		 !outProxyLights ) return qfalse;
	routes = (ralEmissiveRouteReceipt_t *)calloc( routeCapacity, sizeof( *routes ) );
	proxies = (ralLightDescription_t *)calloc( proxyCapacity, sizeof( *proxies ) );
	if ( !routes || !proxies ) { free( routes ); free( proxies ); return qfalse; }
	remainingPolicy = *policy;
	memset( &receipt, 0, sizeof( receipt ) );
	for ( batchIndex = 0u; batchIndex < state->worldSnapshot.batchCount; ++batchIndex ) {
		const renderWorldBatch_t *batch = &state->worldSnapshot.batches[batchIndex];
		renderMaterialSnapshot_t material;
		ralEmissiveSurface_t surface;
		ralEmissiveProxyReceipt_t proxyReceipt;
		uint32_t available;
		if ( !MaterialLighting( state, batch->baseMaterial, &material ) ) continue;
		if ( !EmissiveMaterial( &material ) ) continue;
		if ( routeCount >= routeCapacity || !EmissiveSurfaceBuild( &state->worldSnapshot, batch, &material, &surface ) )
			goto fail;
		available = proxyCapacity - proxyCount;
		if ( !Ral_EmissiveProxyBuild( &surface, &remainingPolicy, available,
			available ? proxies + proxyCount : NULL, &proxyReceipt ) ||
			 !Ral_EmissiveRouteBuild( &surface, &proxyReceipt, &routes[routeCount] ) ) goto fail;
		if ( proxyReceipt.consumedGlobalBudget > remainingPolicy.remainingGlobalBudget ) goto fail;
		remainingPolicy.remainingGlobalBudget -= proxyReceipt.consumedGlobalBudget;
		proxyCount += proxyReceipt.proxyCount;
		if ( routes[routeCount].consumerMask & RAL_EMISSIVE_CONSUMER_DIRECT_PROXY ) receipt.directRouteCount++;
		if ( routes[routeCount].consumerMask & RAL_EMISSIVE_CONSUMER_STATIC_BAKE ) receipt.bakeRouteCount++;
		if ( routes[routeCount].consumerMask & RAL_EMISSIVE_CONSUMER_BLOOM_SOURCE ) receipt.bloomRouteCount++;
		if ( routes[routeCount].consumerMask & RAL_EMISSIVE_CONSUMER_ATMOSPHERE ) receipt.atmosphereRouteCount++;
		if ( routes[routeCount].proxyReason != RAL_EMISSIVE_PROXY_ACCEPTED ) receipt.rejectedProxyRouteCount++;
		digest = HashU64( digest, routes[routeCount].radianceAuthorityHash );
		digest = HashU64( digest, routes[routeCount].proxySetHash );
		digest = HashU32( digest, routes[routeCount].consumerMask );
		routeCount++;
	}
	receipt.schemaVersion = RENDER_EMISSIVE_ROUTING_SCHEMA_VERSION;
	receipt.routingGeneration = routingGeneration;
	receipt.worldDigest = state->worldDigest;
	receipt.materialDigest = state->materialDigest;
	receipt.routeDigest = digest ? digest : 1u;
	receipt.routeCount = routeCount;
	receipt.proxyLightCount = proxyCount;
	receipt.ready = qtrue;
	if ( !RenderSubmission_EmissiveRoutingReceiptValid( &receipt ) ) goto fail;
	if ( routeCount ) memcpy( outRoutes, routes, routeCount * sizeof( *routes ) );
	if ( proxyCount ) memcpy( outProxyLights, proxies, proxyCount * sizeof( *proxies ) );
	*outReceipt = receipt;
	free( routes ); free( proxies );
	return qtrue;
fail:
	free( routes ); free( proxies );
	return qfalse;
}

qboolean RenderSubmission_RebuildEmissiveAuthority(
	renderSubmissionState_t *state, uint64_t routingGeneration )
{
	renderEmissiveRoutingReceipt_t receipt;
	ralEmissiveProxyPolicy_t policy = {
		RAL_EMISSIVE_PROXY_MAX_PER_SURFACE,
		RENDER_SUBMISSION_MAX_EMISSIVE_PROXY_LIGHTS,
		1, 1
	};
	if ( !state || !state->initialized || state->frameOpen || !state->worldLoaded )
		return qfalse;
	if ( !RenderSubmission_BuildEmissiveRoutes( state, routingGeneration, &policy,
		state->emissiveRoutes, RENDER_SUBMISSION_MAX_EMISSIVE_ROUTES,
		state->emissiveProxyLights, RENDER_SUBMISSION_MAX_EMISSIVE_PROXY_LIGHTS,
		&receipt ) ) {
		memset( state->emissiveRoutes, 0, sizeof( state->emissiveRoutes ) );
		memset( state->emissiveProxyLights, 0, sizeof( state->emissiveProxyLights ) );
		memset( &state->emissiveRouting, 0, sizeof( state->emissiveRouting ) );
		return qfalse;
	}
	state->emissiveRouting = receipt;
	return qtrue;
}

qboolean RenderSubmission_EmissiveAuthoritySnapshot(
	const renderSubmissionState_t *state,
	const ralEmissiveRouteReceipt_t **outRoutes, uint32_t *outRouteCount,
	const ralLightDescription_t **outProxyLights, uint32_t *outProxyLightCount,
	const renderEmissiveRoutingReceipt_t **outReceipt )
{
	if ( !state || !state->initialized || !outRoutes || !outRouteCount ||
		!outProxyLights || !outProxyLightCount || !outReceipt ||
		!RenderSubmission_EmissiveRoutingReceiptValid( &state->emissiveRouting ) )
		return qfalse;
	*outRoutes = state->emissiveRoutes;
	*outRouteCount = state->emissiveRouting.routeCount;
	*outProxyLights = state->emissiveProxyLights;
	*outProxyLightCount = state->emissiveRouting.proxyLightCount;
	*outReceipt = &state->emissiveRouting;
	return qtrue;
}

qboolean RenderSubmission_LightingExtractionReceiptValid( const renderLightingExtractionReceipt_t *receipt )
{
	return receipt && receipt->schemaVersion == RENDER_LIGHTING_EXTRACTION_SCHEMA_VERSION &&
		   receipt->extractionGeneration && receipt->extractionGeneration != UINT64_MAX && receipt->worldDigest &&
		   receipt->materialDigest && receipt->geometryHash && receipt->materialHash && receipt->emissiveHash &&
		   receipt->triangleCount && receipt->triangleCount <= RAL_LIGHTING_BAKE_MAX_PATCHES && receipt->ready == qtrue;
}

qboolean RenderSubmission_ExtractLightingTriangles(
	const renderSubmissionState_t *state, uint64_t extractionGeneration,
	ralLightingPatchTriangle_t *outTriangles, uint32_t triangleCapacity,
	renderLightingExtractionReceipt_t *outReceipt )
{
	const renderWorldSnapshot_t *world;
	renderLightingExtractionReceipt_t receipt;
	uint32_t batchIndex, triangleCount = 0u, skippedBatchCount = 0u, outputIndex = 0u;
	uint64_t geometryHash = FNV_OFFSET, materialHash = FNV_OFFSET, emissiveHash = FNV_OFFSET;
	if ( !state || !state->initialized || !state->worldLoaded || !state->worldSnapshot.ready ||
		 !state->worldDigest || !state->materialDigest || !extractionGeneration || extractionGeneration == UINT64_MAX ||
		 !outTriangles || !triangleCapacity || !outReceipt )
		return qfalse;
	world = &state->worldSnapshot;
	for ( batchIndex = 0u; batchIndex < world->batchCount; ++batchIndex ) {
		const renderWorldBatch_t *batch = &world->batches[batchIndex];
		renderMaterialSnapshot_t material;
		uint32_t localIndex;
		if ( batchIndex && world->batches[batchIndex - 1u].sourceSurfaceIndex >= batch->sourceSurfaceIndex )
			return qfalse;
		if ( !MaterialLighting( state, batch->baseMaterial, &material ) )
			return qfalse;
		if ( material.lighting.participatesInStaticBake == qfalse ) {
			skippedBatchCount++;
			continue;
		}
		if ( batch->indexCount == 0u || batch->indexCount % 3u || batch->firstIndex > world->indexCount ||
			 batch->indexCount > world->indexCount - batch->firstIndex )
			return qfalse;
		if ( batch->indexCount / 3u > RAL_LIGHTING_BAKE_MAX_PATCHES - triangleCount )
			return qfalse;
		for ( localIndex = 0u; localIndex < batch->indexCount; localIndex += 3u ) {
			ralLightVec3Q16_t vertices[3];
			if ( !TriangleVertices( world, batch->firstIndex + localIndex, vertices ) )
				return qfalse;
		}
		triangleCount += batch->indexCount / 3u;
	}
	if ( !triangleCount || triangleCount > triangleCapacity )
		return qfalse;
	for ( batchIndex = 0u; batchIndex < world->batchCount; ++batchIndex ) {
		const renderWorldBatch_t *batch = &world->batches[batchIndex];
		renderMaterialSnapshot_t material;
		uint32_t localIndex, localTriangle = 0u, channel;
		(void)MaterialLighting( state, batch->baseMaterial, &material );
		if ( material.lighting.participatesInStaticBake == qfalse )
			continue;
		for ( localIndex = 0u; localIndex < batch->indexCount; localIndex += 3u, ++localTriangle ) {
			ralLightingPatchTriangle_t triangle;
			uint64_t surfaceId = (uint64_t)batch->sourceSurfaceIndex + 1u;
			memset( &triangle, 0, sizeof( triangle ) );
			triangle.schemaVersion    = RAL_LIGHTING_PATCH_SCHEMA_VERSION;
			triangle.surfaceId       = surfaceId;
			triangle.triangleId      = ( surfaceId << 32u ) | ( (uint64_t)localTriangle + 1u );
			triangle.sourceGeneration = state->worldDigest;
			triangle.regionId         = surfaceId;
			(void)TriangleVertices( world, batch->firstIndex + localIndex, triangle.vertices );
			triangle.provenanceHash = HashU64( HashU64( HashU64( FNV_OFFSET, state->worldDigest ), surfaceId ),
				triangle.triangleId );
			triangle.provenanceHash = HashU64( triangle.provenanceHash, material.lighting.provenanceHash );
			triangle.provenanceHash = HashU64( triangle.provenanceHash, material.lighting.sourceGeneration );
			if ( !triangle.provenanceHash )
				triangle.provenanceHash = 1u;
			memcpy( triangle.diffuseReflectanceQ16, material.lighting.diffuseReflectanceQ16,
					sizeof( triangle.diffuseReflectanceQ16 ) );
			memcpy( triangle.emissionRadianceQ16, material.lighting.emissionRadianceQ16,
					sizeof( triangle.emissionRadianceQ16 ) );
			geometryHash = HashU64( geometryHash, triangle.triangleId );
			for ( uint32_t corner = 0u; corner < 3u; ++corner ) {
				geometryHash = HashU32( geometryHash, (uint32_t)triangle.vertices[corner].x );
				geometryHash = HashU32( geometryHash, (uint32_t)triangle.vertices[corner].y );
				geometryHash = HashU32( geometryHash, (uint32_t)triangle.vertices[corner].z );
			}
			materialHash = HashU64( materialHash, triangle.provenanceHash );
			for ( channel = 0u; channel < 3u; ++channel ) {
				materialHash = HashU32( materialHash, (uint32_t)triangle.diffuseReflectanceQ16[channel] );
				emissiveHash = HashU32( emissiveHash, (uint32_t)triangle.emissionRadianceQ16[channel] );
			}
			outTriangles[outputIndex++] = triangle;
		}
	}
	memset( &receipt, 0, sizeof( receipt ) );
	receipt.schemaVersion        = RENDER_LIGHTING_EXTRACTION_SCHEMA_VERSION;
	receipt.extractionGeneration = extractionGeneration;
	receipt.worldDigest          = state->worldDigest;
	receipt.materialDigest       = state->materialDigest;
	receipt.geometryHash         = geometryHash ? geometryHash : 1u;
	receipt.materialHash         = materialHash ? materialHash : 1u;
	receipt.emissiveHash         = emissiveHash ? emissiveHash : 1u;
	receipt.triangleCount        = triangleCount;
	receipt.skippedBatchCount    = skippedBatchCount;
	receipt.ready                = qtrue;
	if ( outputIndex != triangleCount || !RenderSubmission_LightingExtractionReceiptValid( &receipt ) )
		return qfalse;
	*outReceipt = receipt;
	return qtrue;
}

static ralLightVec3Q16_t TriangleCentroid( const ralLightingPatchTriangle_t *triangle )
{
	ralLightVec3Q16_t centroid;
	centroid.x = (int32_t)( ( (int64_t)triangle->vertices[0].x + triangle->vertices[1].x +
							 triangle->vertices[2].x ) /
						   3 );
	centroid.y = (int32_t)( ( (int64_t)triangle->vertices[0].y + triangle->vertices[1].y +
							 triangle->vertices[2].y ) /
						   3 );
	centroid.z = (int32_t)( ( (int64_t)triangle->vertices[0].z + triangle->vertices[1].z +
							 triangle->vertices[2].z ) /
						   3 );
	return centroid;
}

static qboolean RegionDirty( const renderLightingVisibilityRequest_t *request, uint64_t regionId )
{
	uint32_t first = 0u, count = request->dirtyRegionCount;
	if ( !count )
		return qtrue;
	while ( count ) {
		uint32_t step = count / 2u, index = first + step;
		if ( request->dirtyRegionIds[index] < regionId ) {
			first = index + 1u;
			count -= step + 1u;
		} else if ( request->dirtyRegionIds[index] > regionId ) {
			count = step;
		} else {
			return qtrue;
		}
	}
	return qfalse;
}

qboolean RenderSubmission_LightingVisibilityReceiptValid( const renderLightingVisibilityReceipt_t *receipt )
{
	return receipt && receipt->schemaVersion == RENDER_LIGHTING_VISIBILITY_SCHEMA_VERSION &&
		   receipt->queryGeneration && receipt->queryGeneration != UINT64_MAX && receipt->visibilityAuthorityHash &&
		   receipt->candidateHash && receipt->visibilityHash && receipt->candidateCount &&
		   receipt->candidateCount <= RAL_LIGHTING_BAKE_MAX_LINKS && receipt->queriedCount &&
		   receipt->queriedCount <= receipt->candidateCount &&
		   receipt->visibleCount + receipt->occludedCount == receipt->queriedCount &&
		   receipt->dirtyRegionCount <= RAL_LIGHTING_PATCH_MAX_DIRTY_REGIONS && receipt->ready == qtrue;
}

qboolean RenderSubmission_BuildLightingVisibility(
	const renderLightingVisibilityRequest_t *request,
	ralLightingPatchVisibility_t *scratchVisibility, uint32_t scratchCapacity,
	ralLightingPatchVisibility_t *outVisibility, uint32_t outputCapacity,
	renderLightingVisibilityReceipt_t *outReceipt )
{
	renderLightingVisibilityReceipt_t receipt;
	uint32_t candidateIndex, selectedCount = 0u, visibleCount = 0u, occludedCount = 0u;
	uint64_t candidateHash = FNV_OFFSET, visibilityHash = FNV_OFFSET;
	if ( !request || request->schemaVersion != RENDER_LIGHTING_VISIBILITY_SCHEMA_VERSION ||
		 !request->queryGeneration || request->queryGeneration == UINT64_MAX || !request->visibilityAuthorityHash ||
		 !request->triangles || request->triangleCount < 2u || request->triangleCount > RAL_LIGHTING_BAKE_MAX_PATCHES ||
		 !request->candidates || !request->candidateCount || request->candidateCount > RAL_LIGHTING_BAKE_MAX_LINKS ||
		 request->dirtyRegionCount > RAL_LIGHTING_PATCH_MAX_DIRTY_REGIONS || !request->query || !scratchVisibility ||
		 !outVisibility || !outReceipt )
		return qfalse;
	for ( candidateIndex = 0u; candidateIndex < request->triangleCount; ++candidateIndex ) {
		const ralLightingPatchTriangle_t *triangle = &request->triangles[candidateIndex];
		if ( triangle->schemaVersion != RAL_LIGHTING_PATCH_SCHEMA_VERSION || !triangle->triangleId ||
			 !triangle->regionId ||
			 ( candidateIndex && request->triangles[candidateIndex - 1u].triangleId >= triangle->triangleId ) )
			return qfalse;
	}
	for ( candidateIndex = 0u; candidateIndex < request->dirtyRegionCount; ++candidateIndex ) {
		if ( !request->dirtyRegionIds[candidateIndex] ||
			 ( candidateIndex && request->dirtyRegionIds[candidateIndex - 1u] >= request->dirtyRegionIds[candidateIndex] ) )
			return qfalse;
	}
	for ( candidateIndex = 0u; candidateIndex < request->candidateCount; ++candidateIndex ) {
		const renderLightingVisibilityCandidate_t *candidate = &request->candidates[candidateIndex];
		if ( candidate->receiverTriangle >= request->triangleCount || candidate->emitterTriangle >= request->triangleCount ||
			 candidate->receiverTriangle == candidate->emitterTriangle ||
			 ( candidateIndex &&
			   ( request->candidates[candidateIndex - 1u].receiverTriangle > candidate->receiverTriangle ||
				 ( request->candidates[candidateIndex - 1u].receiverTriangle == candidate->receiverTriangle &&
				   request->candidates[candidateIndex - 1u].emitterTriangle >= candidate->emitterTriangle ) ) ) )
			return qfalse;
		candidateHash = HashU32( candidateHash, candidate->receiverTriangle );
		candidateHash = HashU32( candidateHash, candidate->emitterTriangle );
		if ( !request->dirtyRegionCount ||
			 RegionDirty( request, request->triangles[candidate->receiverTriangle].regionId ) ||
			 RegionDirty( request, request->triangles[candidate->emitterTriangle].regionId ) )
			selectedCount++;
	}
	if ( !selectedCount || selectedCount > scratchCapacity || selectedCount > outputCapacity )
		return qfalse;
	for ( candidateIndex = 0u; candidateIndex < request->candidateCount; ++candidateIndex ) {
		const renderLightingVisibilityCandidate_t *candidate = &request->candidates[candidateIndex];
		const ralLightingPatchTriangle_t *receiver = &request->triangles[candidate->receiverTriangle];
		const ralLightingPatchTriangle_t *emitter = &request->triangles[candidate->emitterTriangle];
		ralLightVec3Q16_t receiverCentroid, emitterCentroid;
		uint32_t visibilityQ16;
		uint64_t queryProvenance;
		if ( request->dirtyRegionCount && !RegionDirty( request, receiver->regionId ) &&
			 !RegionDirty( request, emitter->regionId ) )
			continue;
		receiverCentroid = TriangleCentroid( receiver );
		emitterCentroid  = TriangleCentroid( emitter );
		if ( !request->query( request->queryUserData, receiver->triangleId, emitter->triangleId, &receiverCentroid,
							 &emitterCentroid,
							 &visibilityQ16, &queryProvenance ) ||
			 !queryProvenance || visibilityQ16 > RAL_LIGHT_Q16_ONE )
			return qfalse;
		visibilityHash = HashU32( HashU32( visibilityHash, candidate->receiverTriangle ), candidate->emitterTriangle );
		visibilityHash = HashU32( visibilityHash, visibilityQ16 );
		visibilityHash = HashU64( visibilityHash, queryProvenance );
		if ( visibilityQ16 ) {
			scratchVisibility[visibleCount].receiverTriangle = candidate->receiverTriangle;
			scratchVisibility[visibleCount].emitterTriangle  = candidate->emitterTriangle;
			scratchVisibility[visibleCount].visibilityQ16    = visibilityQ16;
			visibleCount++;
		} else {
			occludedCount++;
		}
	}
	memset( &receipt, 0, sizeof( receipt ) );
	receipt.schemaVersion          = RENDER_LIGHTING_VISIBILITY_SCHEMA_VERSION;
	receipt.queryGeneration        = request->queryGeneration;
	receipt.visibilityAuthorityHash = request->visibilityAuthorityHash;
	receipt.candidateHash          = candidateHash ? candidateHash : 1u;
	receipt.visibilityHash         = visibilityHash ? visibilityHash : 1u;
	receipt.candidateCount         = request->candidateCount;
	receipt.queriedCount           = selectedCount;
	receipt.visibleCount           = visibleCount;
	receipt.occludedCount          = occludedCount;
	receipt.dirtyRegionCount       = request->dirtyRegionCount;
	receipt.ready                  = qtrue;
	if ( !RenderSubmission_LightingVisibilityReceiptValid( &receipt ) )
		return qfalse;
	if ( visibleCount )
		memcpy( outVisibility, scratchVisibility, visibleCount * sizeof( *outVisibility ) );
	*outReceipt = receipt;
	return qtrue;
}
