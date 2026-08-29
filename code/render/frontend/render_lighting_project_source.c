// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "render_lighting_project_source.h"

#include "maps/map_format_registry.h"

#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define FNV_OFFSET UINT64_C( 14695981039346656037 )
#define FNV_PRIME UINT64_C( 1099511628211 )
#define PROJECT_PROBE_CLASSNAME "wired_probe_volume"

typedef struct {
	uint32_t index;
	uint64_t distance;
} projectNearest_t;

typedef struct {
	const refimport_t *imports;
	uint64_t authorityHash;
} projectVisibilityContext_t;

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

static uint64_t HashBytes( uint64_t hash, const void *memory, size_t length )
{
	const uint8_t *bytes = (const uint8_t *)memory;
	size_t index;
	for ( index = 0u; index < length; ++index )
		hash = HashByte( hash, bytes[index] );
	return hash;
}

static uint64_t NonZero( uint64_t value )
{
	return value ? value : 1u;
}

static ralLightVec3Q16_t TriangleCentroid(
	const ralLightingPatchTriangle_t *triangle )
{
	ralLightVec3Q16_t value;
	value.x = (int32_t)( ( (int64_t)triangle->vertices[0].x +
		triangle->vertices[1].x + triangle->vertices[2].x ) / 3 );
	value.y = (int32_t)( ( (int64_t)triangle->vertices[0].y +
		triangle->vertices[1].y + triangle->vertices[2].y ) / 3 );
	value.z = (int32_t)( ( (int64_t)triangle->vertices[0].z +
		triangle->vertices[1].z + triangle->vertices[2].z ) / 3 );
	return value;
}

static uint64_t AbsDifference( int32_t a, int32_t b )
{
	int64_t difference = (int64_t)a - b;
	return (uint64_t)( difference < 0 ? -difference : difference );
}

static uint64_t CentroidDistance(
	const ralLightingPatchTriangle_t *a,
	const ralLightingPatchTriangle_t *b )
{
	ralLightVec3Q16_t ca = TriangleCentroid( a );
	ralLightVec3Q16_t cb = TriangleCentroid( b );
	uint64_t value = AbsDifference( ca.x, cb.x );
	if ( UINT64_MAX - value < AbsDifference( ca.y, cb.y ) ) return UINT64_MAX;
	value += AbsDifference( ca.y, cb.y );
	if ( UINT64_MAX - value < AbsDifference( ca.z, cb.z ) ) return UINT64_MAX;
	return value + AbsDifference( ca.z, cb.z );
}

static qboolean NearestBefore( const projectNearest_t *a,
	const projectNearest_t *b )
{
	return a->distance < b->distance ||
		( a->distance == b->distance && a->index < b->index );
}

static void NearestInsert( projectNearest_t *nearest, uint32_t *count,
	uint32_t capacity, uint32_t index, uint64_t distance )
{
	projectNearest_t value = { index, distance };
	uint32_t cursor;
	if ( !capacity ) return;
	if ( *count < capacity ) {
		cursor = ( *count )++;
	} else {
		if ( !NearestBefore( &value, &nearest[capacity - 1u] ) ) return;
		cursor = capacity - 1u;
	}
	while ( cursor && NearestBefore( &value, &nearest[cursor - 1u] ) ) {
		nearest[cursor] = nearest[cursor - 1u];
		cursor--;
	}
	nearest[cursor] = value;
}

static void SortNearestByIndex( projectNearest_t *nearest, uint32_t count )
{
	uint32_t index;
	for ( index = 1u; index < count; ++index ) {
		projectNearest_t value = nearest[index];
		uint32_t cursor = index;
		while ( cursor && nearest[cursor - 1u].index > value.index ) {
			nearest[cursor] = nearest[cursor - 1u];
			cursor--;
		}
		nearest[cursor] = value;
	}
}

static qboolean TraceVisible( const projectVisibilityContext_t *context,
	const ralLightVec3Q16_t *from, const ralLightVec3Q16_t *to,
	uint32_t *outVisibilityQ16, uint64_t *outProvenance )
{
	vec3_t start, end, delta, mins = { 0.0f, 0.0f, 0.0f },
		maxs = { 0.0f, 0.0f, 0.0f };
	trace_t trace;
	float length;
	uint32_t axis;
	uint64_t hash;
	if ( !context || !context->imports || !context->imports->CM_BoxTrace ||
		!from || !to || !outVisibilityQ16 || !outProvenance ) return qfalse;
	for ( axis = 0u; axis < 3u; ++axis ) {
		int32_t a = axis == 0u ? from->x : axis == 1u ? from->y : from->z;
		int32_t b = axis == 0u ? to->x : axis == 1u ? to->y : to->z;
		start[axis] = (float)a / (float)RAL_LIGHT_Q16_ONE;
		end[axis] = (float)b / (float)RAL_LIGHT_Q16_ONE;
		delta[axis] = end[axis] - start[axis];
	}
	length = sqrtf( DotProduct( delta, delta ) );
	if ( isfinite( length ) && length > 1.0f ) {
		for ( axis = 0u; axis < 3u; ++axis ) {
			float inset = delta[axis] / length * 0.5f;
			start[axis] += inset;
			end[axis] -= inset;
		}
	}
	memset( &trace, 0, sizeof( trace ) );
	context->imports->CM_BoxTrace( &trace, start, end, mins, maxs,
		0, CONTENTS_SOLID, qfalse );
	if ( !isfinite( trace.fraction ) || trace.fraction < 0.0f ||
		trace.fraction > 1.0f ) return qfalse;
	*outVisibilityQ16 = !trace.startsolid && !trace.allsolid &&
		trace.fraction >= 0.9999f ? RAL_LIGHT_Q16_ONE : 0u;
	hash = HashU64( FNV_OFFSET, context->authorityHash );
	hash = HashBytes( hash, start, sizeof( start ) );
	hash = HashBytes( hash, end, sizeof( end ) );
	hash = HashBytes( hash, &trace.fraction, sizeof( trace.fraction ) );
	*outProvenance = NonZero( hash );
	return qtrue;
}

static qboolean VisibilityQuery( void *userData, uint64_t receiverTriangleId,
	uint64_t emitterTriangleId, const ralLightVec3Q16_t *receiverCentroid,
	const ralLightVec3Q16_t *emitterCentroid, uint32_t *outVisibilityQ16,
	uint64_t *outQueryProvenance )
{
	projectVisibilityContext_t *context =
		(projectVisibilityContext_t *)userData;
	uint64_t pairHash;
	if ( !TraceVisible( context, receiverCentroid, emitterCentroid,
		outVisibilityQ16, outQueryProvenance ) ) return qfalse;
	pairHash = HashU64( HashU64( FNV_OFFSET, receiverTriangleId ),
		emitterTriangleId );
	*outQueryProvenance = NonZero( HashU64( *outQueryProvenance, pairHash ) );
	return qtrue;
}

static qboolean BuildCandidates( const ralLightingPatchTriangle_t *triangles,
	uint32_t triangleCount, uint32_t maximumLinksPerPatch,
	renderLightingVisibilityCandidate_t *outCandidates, uint32_t capacity,
	uint32_t *outCount )
{
	projectNearest_t nearest[64];
	uint32_t receiver, count = 0u;
	if ( !triangles || triangleCount < 2u || !maximumLinksPerPatch ||
		maximumLinksPerPatch > 64u || !outCandidates || !outCount ) return qfalse;
	for ( receiver = 0u; receiver < triangleCount; ++receiver ) {
		uint32_t emitter, nearestCount = 0u, index;
		for ( emitter = 0u; emitter < triangleCount; ++emitter ) {
			if ( emitter == receiver ) continue;
			NearestInsert( nearest, &nearestCount, maximumLinksPerPatch,
				emitter, CentroidDistance( &triangles[receiver],
					&triangles[emitter] ) );
		}
		SortNearestByIndex( nearest, nearestCount );
		if ( nearestCount > capacity - count ) return qfalse;
		for ( index = 0u; index < nearestCount; ++index ) {
			outCandidates[count].receiverTriangle = receiver;
			outCandidates[count].emitterTriangle = nearest[index].index;
			count++;
		}
	}
	*outCount = count;
	return count ? qtrue : qfalse;
}

static qboolean DeriveLightmapExtent( const mapFile_t *map,
	uint32_t *outWidth, uint32_t *outHeight, uint32_t *outPages,
	uint32_t *outTexels )
{
	uint64_t pixels, total;
	uint32_t side;
	if ( !map || !outWidth || !outHeight || !outPages || !outTexels ||
		map->numLightmapPages <= 0 || map->lightmapPageSize <= 0 ||
		map->lightmapPageSize % 3 ) return qfalse;
	pixels = (uint32_t)map->lightmapPageSize / 3u;
	for ( side = 1u; (uint64_t)side * side < pixels; ++side )
		if ( side >= RENDER_SUBMISSION_MAX_IMAGE_DIMENSION ) return qfalse;
	if ( (uint64_t)side * side != pixels ||
		side > RENDER_SUBMISSION_MAX_IMAGE_DIMENSION ) return qfalse;
	total = pixels * (uint32_t)map->numLightmapPages;
	if ( !total || total > UINT32_MAX ) return qfalse;
	*outWidth = side;
	*outHeight = side;
	*outPages = (uint32_t)map->numLightmapPages;
	*outTexels = (uint32_t)total;
	return qtrue;
}

static double Edge2( double ax, double ay, double bx, double by,
	double px, double py )
{
	return ( px - ax ) * ( by - ay ) - ( py - ay ) * ( bx - ax );
}

static qboolean RasterizeTriangle( const renderWorldSnapshot_t *world,
	const renderWorldBatch_t *batch, uint32_t localIndex, uint32_t patchIndex,
	uint32_t width, uint32_t height, uint32_t pages, uint32_t *texels,
	uint32_t *mappedCount )
{
	double u[3], v[3], minimumU = DBL_MAX, minimumV = DBL_MAX,
		maximumU = -DBL_MAX, maximumV = -DBL_MAX, area;
	int minimumX, minimumY, maximumX, maximumY, x, y;
	uint32_t corner;
	if ( !world || !batch || !texels || !mappedCount ||
		batch->lightmapIndex < 0 || (uint32_t)batch->lightmapIndex >= pages )
		return batch && batch->lightmapIndex < 0 ? qtrue : qfalse;
	for ( corner = 0u; corner < 3u; ++corner ) {
		uint32_t vertexIndex = world->indices[batch->firstIndex + localIndex + corner];
		if ( vertexIndex >= world->vertexCount ) return qfalse;
		u[corner] = world->vertices[vertexIndex].lightmapCoord[0];
		v[corner] = world->vertices[vertexIndex].lightmapCoord[1];
		if ( !isfinite( u[corner] ) || !isfinite( v[corner] ) ) return qfalse;
		if ( u[corner] < minimumU ) minimumU = u[corner];
		if ( u[corner] > maximumU ) maximumU = u[corner];
		if ( v[corner] < minimumV ) minimumV = v[corner];
		if ( v[corner] > maximumV ) maximumV = v[corner];
	}
	area = Edge2( u[0], v[0], u[1], v[1], u[2], v[2] );
	if ( fabs( area ) < 1e-15 ) return qtrue;
	minimumX = (int)floor( minimumU * width - 0.5 );
	minimumY = (int)floor( minimumV * height - 0.5 );
	maximumX = (int)ceil( maximumU * width - 0.5 );
	maximumY = (int)ceil( maximumV * height - 0.5 );
	if ( minimumX < 0 ) minimumX = 0;
	if ( minimumY < 0 ) minimumY = 0;
	if ( maximumX >= (int)width ) maximumX = (int)width - 1;
	if ( maximumY >= (int)height ) maximumY = (int)height - 1;
	for ( y = minimumY; y <= maximumY; ++y ) {
		for ( x = minimumX; x <= maximumX; ++x ) {
			double px = ( (double)x + 0.5 ) / width;
			double py = ( (double)y + 0.5 ) / height;
			double e0 = Edge2( u[0], v[0], u[1], v[1], px, py );
			double e1 = Edge2( u[1], v[1], u[2], v[2], px, py );
			double e2 = Edge2( u[2], v[2], u[0], v[0], px, py );
			qboolean inside = area > 0.0 ?
				( e0 >= -1e-9 && e1 >= -1e-9 && e2 >= -1e-9 ) :
				( e0 <= 1e-9 && e1 <= 1e-9 && e2 <= 1e-9 );
			uint32_t offset;
			if ( !inside ) continue;
			offset = (uint32_t)batch->lightmapIndex * width * height +
				(uint32_t)y * width + (uint32_t)x;
			if ( texels[offset] == RAL_LIGHTING_COOK_UNMAPPED_TEXEL ) {
				texels[offset] = patchIndex;
				( *mappedCount )++;
			} else if ( patchIndex < texels[offset] ) {
				texels[offset] = patchIndex;
			}
		}
	}
	return qtrue;
}

static qboolean BuildTexelMap( const renderSubmissionState_t *submission,
	const ralLightingPatchTriangle_t *triangles, uint32_t patchCount,
	uint32_t width, uint32_t height, uint32_t pages,
	uint32_t *texels, uint32_t texelCount, uint32_t *outMappedCount )
{
	const renderWorldSnapshot_t *world;
	uint32_t batchIndex, patchCursor = 0u, mapped = 0u, index;
	if ( !submission || !triangles || !texels || !texelCount || !outMappedCount )
		return qfalse;
	world = &submission->worldSnapshot;
	for ( index = 0u; index < texelCount; ++index )
		texels[index] = RAL_LIGHTING_COOK_UNMAPPED_TEXEL;
	for ( batchIndex = 0u; batchIndex < world->batchCount; ++batchIndex ) {
		const renderWorldBatch_t *batch = &world->batches[batchIndex];
		uint64_t surfaceId = (uint64_t)batch->sourceSurfaceIndex + 1u;
		uint32_t local;
		if ( submission->worldSnapshot.batches[batchIndex].indexCount % 3u )
			return qfalse;
		if ( submission->worldSnapshot.ready != qtrue ) return qfalse;
		if ( submission->worldSnapshot.indices == NULL ||
			submission->worldSnapshot.vertices == NULL ) return qfalse;
		if ( submission->worldSnapshot.indexCount < batch->firstIndex + batch->indexCount )
			return qfalse;
		if ( submission->worldSnapshot.batchCount &&
			submission->worldSnapshot.batches == NULL ) return qfalse;
		if ( submission->worldSnapshot.vertices == NULL ) return qfalse;
		/* ExtractLightingTriangles omits non-bake batches; surface ids preserve the join. */
		if ( patchCursor < patchCount && triangles[patchCursor].surfaceId == surfaceId ) {
			for ( local = 0u; local < batch->indexCount; local += 3u ) {
				if ( patchCursor >= patchCount ||
					triangles[patchCursor].surfaceId != surfaceId ) return qfalse;
				if ( batch->lightmapIndex >= 0 &&
					!RasterizeTriangle( world, batch, local, patchCursor, width,
						height, pages, texels, &mapped ) )
					return qfalse;
				patchCursor++;
			}
		}
	}
	if ( patchCursor != patchCount || !mapped ) return qfalse;
	*outMappedCount = mapped;
	return qtrue;
}

static void ApplyVolumeDefaults( renderLightingVolumeAuthoring_t *authoring )
{
	const wiredMetadataRegistry_t *registry =
		Render_LightingVolumeMetadataRegistry();
	uint32_t index;
	memset( authoring, 0, sizeof( *authoring ) );
	authoring->schemaVersion = RENDER_LIGHTING_VOLUME_AUTHORING_SCHEMA_VERSION;
	if ( !registry ) return;
	for ( index = 0u; index < registry->fieldCount; ++index )
		(void)WiredMetadata_Parse( &registry->fields[index], authoring,
			registry->fields[index].defaultValue, NULL );
}

static qboolean ReadQuoted( const char **cursor, char *out, size_t capacity )
{
	const char *source;
	size_t length = 0u;
	if ( !cursor || !( source = *cursor ) || !out || capacity < 2u ) return qfalse;
	while ( *source && (unsigned char)*source <= ' ' ) source++;
	if ( *source != '"' ) return qfalse;
	source++;
	while ( *source && *source != '"' ) {
		if ( length + 1u >= capacity ) return qfalse;
		out[length++] = *source++;
	}
	if ( *source != '"' ) return qfalse;
	out[length] = '\0';
	*cursor = source + 1u;
	return qtrue;
}

static qboolean ParseVolumes( const mapFile_t *map, uint64_t sourceGeneration,
	renderLightingProjectVolume_t *volumes, uint32_t capacity,
	uint32_t *outCount, uint64_t *outLayoutHash )
{
	const wiredMetadataRegistry_t *registry = Render_LightingVolumeMetadataRegistry();
	const char *cursor;
	uint32_t entityIndex = 0u, count = 0u;
	uint64_t combined = HashU32( FNV_OFFSET,
		RENDER_LIGHTING_VOLUME_AUTHORING_SCHEMA_VERSION );
	if ( !map || !map->entityString || !registry || !volumes || !capacity ||
		!outCount || !outLayoutHash ) return qfalse;
	cursor = map->entityString;
	while ( *cursor ) {
		renderLightingVolumeAuthoring_t authoring;
		char classname[64] = "";
		char origin[128] = "";
		qboolean explicitOrigin = qfalse;
		uint64_t provenance = HashU64( FNV_OFFSET, (uint64_t)entityIndex + 1u );
		while ( *cursor && *cursor != '{' ) cursor++;
		if ( !*cursor ) break;
		cursor++;
		ApplyVolumeDefaults( &authoring );
		while ( *cursor && *cursor != '}' ) {
			char key[96], value[256];
			const wiredMetadataField_t *field;
			while ( *cursor && (unsigned char)*cursor <= ' ' ) cursor++;
			if ( *cursor == '}' ) break;
			if ( !ReadQuoted( &cursor, key, sizeof( key ) ) ||
				!ReadQuoted( &cursor, value, sizeof( value ) ) ) return qfalse;
			provenance = HashBytes( provenance, key, strlen( key ) + 1u );
			provenance = HashBytes( provenance, value, strlen( value ) + 1u );
			if ( !strcmp( key, "classname" ) ) {
				if ( strlen( value ) >= sizeof( classname ) ) return qfalse;
				memcpy( classname, value, strlen( value ) + 1u );
			} else if ( !strcmp( key, "origin" ) ) {
				if ( strlen( value ) >= sizeof( origin ) ) return qfalse;
				memcpy( origin, value, strlen( value ) + 1u );
			} else if ( ( field = WiredMetadata_Find( registry, key ) ) != NULL ) {
				if ( !WiredMetadata_Parse( field, &authoring, value, NULL ) ) return qfalse;
				if ( !strcmp( key, "probe_origin" ) ) explicitOrigin = qtrue;
			}
		}
		if ( *cursor != '}' ) return qfalse;
		cursor++;
		if ( !strcmp( classname, PROJECT_PROBE_CLASSNAME ) ) {
			const wiredMetadataField_t *field;
			if ( count >= capacity ) return qfalse;
			if ( !explicitOrigin && origin[0] ) {
				field = WiredMetadata_Find( registry, "probe_origin" );
				if ( !field || !WiredMetadata_Parse( field, &authoring, origin, NULL ) )
					return qfalse;
			}
			provenance = NonZero( HashU32( provenance, (uint32_t)map->checksum ) );
			if ( !Render_LightingVolumePlacementBuild( &authoring,
				(uint64_t)entityIndex + 1u, sourceGeneration, provenance,
				&volumes[count].placement ) ) return qfalse;
			volumes[count].layoutHash =
				Ral_IrradianceVolumeLayoutHash( &volumes[count].placement );
			if ( !volumes[count].layoutHash ) return qfalse;
			combined = HashU64( combined, volumes[count].layoutHash );
			count++;
		}
		entityIndex++;
	}
	if ( !count ) return qfalse;
	*outCount = count;
	*outLayoutHash = NonZero( combined );
	return qtrue;
}

static qboolean ProbePosition( const ralIrradianceVolumePlacement_t *placement,
	uint32_t index, ralLightVec3Q16_t *outPosition )
{
	uint64_t plane, x, y, z;
	int64_t value[3];
	if ( !placement || !outPosition ) return qfalse;
	plane = (uint64_t)placement->dimensions[0] * placement->dimensions[1];
	if ( !plane ) return qfalse;
	z = index / plane;
	y = ( index % plane ) / placement->dimensions[0];
	x = index % placement->dimensions[0];
	value[0] = (int64_t)placement->origin.x + (int64_t)placement->spacing.x * x;
	value[1] = (int64_t)placement->origin.y + (int64_t)placement->spacing.y * y;
	value[2] = (int64_t)placement->origin.z + (int64_t)placement->spacing.z * z;
	if ( value[0] < INT32_MIN || value[0] > INT32_MAX ||
		value[1] < INT32_MIN || value[1] > INT32_MAX ||
		value[2] < INT32_MIN || value[2] > INT32_MAX ) return qfalse;
	outPosition->x = (int32_t)value[0];
	outPosition->y = (int32_t)value[1];
	outPosition->z = (int32_t)value[2];
	return qtrue;
}

static qboolean DirectionQ16( const ralLightVec3Q16_t *from,
	const ralLightVec3Q16_t *to, ralLightVec3Q16_t *outDirection,
	uint32_t *outWeight )
{
	double x, y, z, length, worldDistance, weight;
	if ( !from || !to || !outDirection || !outWeight ) return qfalse;
	x = (double)to->x - from->x;
	y = (double)to->y - from->y;
	z = (double)to->z - from->z;
	length = sqrt( x * x + y * y + z * z );
	if ( !isfinite( length ) || length <= 0.0 ) return qfalse;
	outDirection->x = (int32_t)( x / length * RAL_LIGHT_Q16_ONE );
	outDirection->y = (int32_t)( y / length * RAL_LIGHT_Q16_ONE );
	outDirection->z = (int32_t)( z / length * RAL_LIGHT_Q16_ONE );
	worldDistance = length / RAL_LIGHT_Q16_ONE;
	weight = (double)RAL_LIGHT_Q16_ONE * 64.0 / ( 64.0 + worldDistance );
	if ( weight < 1.0 ) weight = 1.0;
	if ( weight > RAL_LIGHT_Q16_ONE ) weight = RAL_LIGHT_Q16_ONE;
	*outWeight = (uint32_t)( weight + 0.5 );
	return qtrue;
}

static qboolean BuildProbeSamples( const ralLightingBakePatch_t *patches,
	uint32_t patchCount, projectVisibilityContext_t *visibilityContext,
	uint32_t maximumSamplesPerProbe, renderLightingProjectVolume_t *volumes,
	uint32_t volumeCount, ralIrradianceProductSample_t *samples,
	uint32_t capacity, uint32_t *outCount )
{
	projectNearest_t nearest[64];
	uint32_t volumeIndex, count = 0u;
	if ( !patches || !patchCount || !visibilityContext ||
		!maximumSamplesPerProbe || maximumSamplesPerProbe > 64u || !volumes ||
		!volumeCount || !samples || !outCount ) return qfalse;
	for ( volumeIndex = 0u; volumeIndex < volumeCount; ++volumeIndex ) {
		renderLightingProjectVolume_t *volume = &volumes[volumeIndex];
		uint64_t probeCount64 = (uint64_t)volume->placement.dimensions[0] *
			volume->placement.dimensions[1] * volume->placement.dimensions[2];
		uint32_t probe;
		if ( !probeCount64 || probeCount64 > UINT32_MAX ) return qfalse;
		volume->firstSample = count;
		for ( probe = 0u; probe < (uint32_t)probeCount64; ++probe ) {
			ralLightVec3Q16_t position;
			uint32_t patch, nearestCount = 0u, index;
			if ( !ProbePosition( &volume->placement, probe, &position ) ) return qfalse;
			for ( patch = 0u; patch < patchCount; ++patch ) {
				uint64_t distance = AbsDifference( position.x, patches[patch].centroid.x );
				if ( UINT64_MAX - distance < AbsDifference( position.y, patches[patch].centroid.y ) )
					distance = UINT64_MAX;
				else distance += AbsDifference( position.y, patches[patch].centroid.y );
				if ( UINT64_MAX - distance < AbsDifference( position.z, patches[patch].centroid.z ) )
					distance = UINT64_MAX;
				else distance += AbsDifference( position.z, patches[patch].centroid.z );
				NearestInsert( nearest, &nearestCount, maximumSamplesPerProbe,
					patch, distance );
			}
			SortNearestByIndex( nearest, nearestCount );
			if ( nearestCount > capacity - count ) return qfalse;
			for ( index = 0u; index < nearestCount; ++index ) {
				ralIrradianceProductSample_t sample;
				uint32_t visibilityQ16;
				uint64_t provenance;
				memset( &sample, 0, sizeof( sample ) );
				sample.probeIndex = probe;
				sample.patchIndex = nearest[index].index;
				if ( !DirectionQ16( &position,
					&patches[sample.patchIndex].centroid,
					&sample.directionToPatch, &sample.weightQ16 ) ||
					!TraceVisible( visibilityContext, &position,
						&patches[sample.patchIndex].centroid,
						&visibilityQ16, &provenance ) ) return qfalse;
				(void)provenance;
				sample.occluded = visibilityQ16 ? qfalse : qtrue;
				samples[count++] = sample;
			}
		}
		volume->sampleCount = count - volume->firstSample;
	}
	*outCount = count;
	return count ? qtrue : qfalse;
}

qboolean RenderLightingProjectSource_ReceiptValid(
	const renderLightingProjectSourceReceipt_t *receipt )
{
	uint64_t texels;
	uint32_t index;
	if ( !receipt ||
		receipt->schemaVersion != RENDER_LIGHTING_PROJECT_SOURCE_RECEIPT_SCHEMA_VERSION ||
		!receipt->sourceGeneration || !receipt->sourceRevision ||
		!receipt->geometryHash || !receipt->materialHash || !receipt->emissiveHash ||
		!receipt->visibilityHash || !receipt->graphHash || !receipt->probeLayoutHash ||
		!receipt->triangleCount || !receipt->candidateCount ||
		!receipt->visibilityCount || !receipt->patchCount || !receipt->linkCount ||
		!receipt->pageWidth || !receipt->pageHeight || !receipt->pageCount ||
		!receipt->texelCount || !receipt->mappedTexelCount ||
		receipt->mappedTexelCount > receipt->texelCount || !receipt->volumeCount ||
		receipt->volumeCount > RENDER_LIGHTING_PROJECT_MAX_VOLUMES ||
		!receipt->sampleCount || !receipt->dirtyRegionCount ||
		receipt->dirtyRegionCount > RAL_LIGHTING_PATCH_MAX_DIRTY_REGIONS ||
		receipt->ready != qtrue ) return qfalse;
	texels = (uint64_t)receipt->pageWidth * receipt->pageHeight * receipt->pageCount;
	if ( texels != receipt->texelCount ) return qfalse;
	for ( index = 0u; index < receipt->dirtyRegionCount; ++index )
		if ( !receipt->dirtyRegionIds[index] ||
			( index && receipt->dirtyRegionIds[index - 1u] >=
				receipt->dirtyRegionIds[index] ) ) return qfalse;
	return qtrue;
}

qboolean RenderLightingProjectSource_Build(
	const renderSubmissionState_t *submission, const mapFile_t *map,
	const refimport_t *imports, const renderLightingProjectSourceRequest_t *request,
	const renderLightingProjectSourceWorkspace_t *workspace,
	renderLightingProjectSourceReceipt_t *outReceipt )
{
	renderLightingProjectSourceReceipt_t result;
	renderLightingExtractionReceipt_t extraction;
	renderLightingVisibilityRequest_t visibilityRequest;
	renderLightingVisibilityReceipt_t visibilityReceipt;
	ralLightingPatchGraphRequest_t graphRequest;
	ralLightingPatchGraphReceipt_t graphReceipt;
	projectVisibilityContext_t visibilityContext;
	uint32_t index, regionCount = 0u;
	uint64_t revision;
	if ( !submission || !map || !imports || !request || !workspace || !outReceipt ||
		request->schemaVersion != RENDER_LIGHTING_PROJECT_SOURCE_SCHEMA_VERSION ||
		!request->sourceGeneration || !request->visibilityAuthorityHash ||
		!request->maximumLinksPerPatch || request->maximumLinksPerPatch > 64u ||
		!request->maximumSamplesPerProbe || request->maximumSamplesPerProbe > 64u ||
		!workspace->triangles || !workspace->triangleCapacity ||
		!workspace->candidates || !workspace->candidateCapacity ||
		!workspace->visibilityScratch || !workspace->visibility ||
		!workspace->visibilityCapacity || !workspace->patches ||
		!workspace->patchCapacity || !workspace->links || !workspace->linkCapacity ||
		!workspace->dirtyPatches || !workspace->dirtyPatchCapacity ||
		!workspace->texelPatchIndices || !workspace->texelCapacity ||
		!workspace->volumes || !workspace->volumeCapacity ||
		!workspace->samples || !workspace->sampleCapacity || !imports->CM_BoxTrace )
		return qfalse;
	memset( &result, 0, sizeof( result ) );
	if ( !RenderSubmission_ExtractLightingTriangles( submission,
		request->sourceGeneration, workspace->triangles,
		workspace->triangleCapacity, &extraction ) ||
		!BuildCandidates( workspace->triangles, extraction.triangleCount,
			request->maximumLinksPerPatch, workspace->candidates,
			workspace->candidateCapacity, &result.candidateCount ) ) return qfalse;
	visibilityContext.imports = imports;
	visibilityContext.authorityHash = request->visibilityAuthorityHash;
	memset( &visibilityRequest, 0, sizeof( visibilityRequest ) );
	visibilityRequest.schemaVersion = RENDER_LIGHTING_VISIBILITY_SCHEMA_VERSION;
	visibilityRequest.queryGeneration = request->sourceGeneration;
	visibilityRequest.visibilityAuthorityHash = request->visibilityAuthorityHash;
	visibilityRequest.triangles = workspace->triangles;
	visibilityRequest.triangleCount = extraction.triangleCount;
	visibilityRequest.candidates = workspace->candidates;
	visibilityRequest.candidateCount = result.candidateCount;
	visibilityRequest.query = VisibilityQuery;
	visibilityRequest.queryUserData = &visibilityContext;
	if ( !RenderSubmission_BuildLightingVisibility( &visibilityRequest,
		workspace->visibilityScratch, workspace->visibilityCapacity,
		workspace->visibility, workspace->visibilityCapacity,
		&visibilityReceipt ) ) return qfalse;
	memset( &graphRequest, 0, sizeof( graphRequest ) );
	graphRequest.schemaVersion = RAL_LIGHTING_PATCH_GRAPH_SCHEMA_VERSION;
	graphRequest.graphGeneration = request->sourceGeneration;
	graphRequest.triangles = workspace->triangles;
	graphRequest.triangleCount = extraction.triangleCount;
	graphRequest.visibility = workspace->visibility;
	graphRequest.visibilityCount = visibilityReceipt.visibleCount;
	graphRequest.minimumFormFactorQ16 = 1u;
	if ( !Ral_LightingPatchGraphBuild( &graphRequest, workspace->patches,
		workspace->patchCapacity, workspace->links, workspace->linkCapacity,
		workspace->dirtyPatches, workspace->dirtyPatchCapacity,
		&graphReceipt ) ) return qfalse;
	if ( !DeriveLightmapExtent( map, &result.pageWidth, &result.pageHeight,
		&result.pageCount, &result.texelCount ) ||
		result.texelCount > workspace->texelCapacity ||
		!BuildTexelMap( submission, workspace->triangles, graphReceipt.patchCount,
			result.pageWidth,
			result.pageHeight, result.pageCount, workspace->texelPatchIndices,
			result.texelCount, &result.mappedTexelCount ) ||
		!ParseVolumes( map, request->sourceGeneration, workspace->volumes,
			workspace->volumeCapacity, &result.volumeCount,
			&result.probeLayoutHash ) ||
		!BuildProbeSamples( workspace->patches, graphReceipt.patchCount,
			&visibilityContext, request->maximumSamplesPerProbe,
			workspace->volumes, result.volumeCount, workspace->samples,
			workspace->sampleCapacity, &result.sampleCount ) ) return qfalse;
	for ( index = 0u; index < graphReceipt.patchCount; ++index ) {
		uint64_t region = workspace->patches[index].regionId;
		uint32_t insert = 0u;
		while ( insert < regionCount && result.dirtyRegionIds[insert] < region )
			insert++;
		if ( insert < regionCount && result.dirtyRegionIds[insert] == region ) continue;
		if ( regionCount >= RAL_LIGHTING_PATCH_MAX_DIRTY_REGIONS ) return qfalse;
		memmove( &result.dirtyRegionIds[insert + 1u],
			&result.dirtyRegionIds[insert],
			(size_t)( regionCount - insert ) * sizeof( result.dirtyRegionIds[0] ) );
		result.dirtyRegionIds[insert] = region;
		regionCount++;
	}
	revision = HashU64( FNV_OFFSET, extraction.geometryHash );
	revision = HashU64( revision, extraction.materialHash );
	revision = HashU64( revision, extraction.emissiveHash );
	revision = HashU64( revision, visibilityReceipt.visibilityHash );
	revision = HashU64( revision, graphReceipt.graphHash );
	revision = HashU64( revision, result.probeLayoutHash );
	result.schemaVersion = RENDER_LIGHTING_PROJECT_SOURCE_RECEIPT_SCHEMA_VERSION;
	result.sourceGeneration = request->sourceGeneration;
	result.sourceRevision = NonZero( revision );
	result.geometryHash = extraction.geometryHash;
	result.materialHash = extraction.materialHash;
	result.emissiveHash = extraction.emissiveHash;
	result.visibilityHash = visibilityReceipt.visibilityHash;
	result.graphHash = graphReceipt.graphHash;
	result.triangleCount = extraction.triangleCount;
	result.visibilityCount = visibilityReceipt.visibleCount;
	result.patchCount = graphReceipt.patchCount;
	result.linkCount = graphReceipt.linkCount;
	result.dirtyRegionCount = regionCount;
	result.ready = qtrue;
	if ( !RenderLightingProjectSource_ReceiptValid( &result ) ) return qfalse;
	*outReceipt = result;
	return qtrue;
}
