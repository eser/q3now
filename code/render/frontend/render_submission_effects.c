// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "render_submission.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

#define ATMOSPHERE_FNV_OFFSET UINT64_C( 1469598103934665603 )
#define ATMOSPHERE_FNV_PRIME  UINT64_C( 1099511628211 )

static uint64_t AtmosphereHashBytes( uint64_t digest, const void *data, size_t size )
{
	const byte *bytes = (const byte *)data;
	for ( size_t i = 0u; i < size; ++i ) {
		digest ^= bytes[i];
		digest *= ATMOSPHERE_FNV_PRIME;
	}
	return digest;
}

static qboolean AtmosphereFloatArrayFinite( const float *values, size_t count )
{
	if ( !values )
		return qfalse;
	for ( size_t i = 0u; i < count; ++i )
		if ( !isfinite( values[i] ) )
			return qfalse;
	return qtrue;
}

static qboolean AtmosphereUnitArray( const float *values, size_t count )
{
	if ( !AtmosphereFloatArrayFinite( values, count ) )
		return qfalse;
	for ( size_t i = 0u; i < count; ++i )
		if ( values[i] < 0.0f || values[i] > 1.0f )
			return qfalse;
	return qtrue;
}

static qboolean AtmosphereReservedZero( const uint32_t *values, size_t count )
{
	for ( size_t i = 0u; i < count; ++i )
		if ( values[i] != 0u )
			return qfalse;
	return qtrue;
}

static qboolean AtmosphereEmitterReservedZero( const float *values, size_t count )
{
	for ( size_t i = 0u; i < count; ++i )
		if ( values[i] != 0.0f )
			return qfalse;
	return qtrue;
}

static void AtmosphereDefaults( atmosphereFrameState_t *out )
{
	memset( out, 0, sizeof( *out ) );
	out->schemaVersion	= WIRED_ATMOSPHERE_SCHEMA_VERSION;
	out->qualityTier	= ATMOSPHERE_QUALITY_OFF;
	out->temperatureC	= 15.0f;
	out->humidity		= 0.5f;
	out->indoorExposure = 1.0f;
}

static qboolean AtmosphereBoundsValid( const atmosphereFrameState_t *value )
{
	if ( !AtmosphereFloatArrayFinite( value->bounds, 6u ) || !AtmosphereFloatArrayFinite( value->worldMins, 2u ) ||
		 !AtmosphereFloatArrayFinite( value->worldMaxs, 2u ) )
		return qfalse;
	for ( size_t axis = 0u; axis < 3u; ++axis )
		if ( value->bounds[axis] > value->bounds[axis + 3u] )
			return qfalse;
	for ( size_t axis = 0u; axis < 2u; ++axis )
		if ( value->worldMins[axis] > value->worldMaxs[axis] )
			return qfalse;
	return qtrue;
}

static qboolean AtmosphereNormalize( const atmosphereFrameState_t *value, atmosphereFrameState_t *out )
{
	static const uint32_t knownFlags = ATMOSPHERE_FLAG_ENABLED | ATMOSPHERE_FLAG_HEIGHTGRID |
									   ATMOSPHERE_FLAG_INDOOR_EXPOSURE | ATMOSPHERE_FLAG_SURFACE_CLIMATE |
									   ATMOSPHERE_FLAG_SKY_LIGHTING | ATMOSPHERE_FLAG_VOLUMETRIC_MEDIA;
	if ( !value || !out )
		return qfalse;
	AtmosphereDefaults( out );
	if ( value->schemaVersion == 0u ) {
		if ( value->type < ATMOSPHERE_PRECIP_NONE || value->type > ATMOSPHERE_PRECIP_SNOW ||
			 !isfinite( value->distance ) || value->distance < 0.0f || value->gridSize < 0 || value->gridSize > 4096 ||
			 !AtmosphereBoundsValid( value ) )
			return qfalse;
		out->type = value->type;
		memcpy( out->bounds, value->bounds, sizeof( out->bounds ) );
		out->distance = value->distance;
		memcpy( out->worldMins, value->worldMins, sizeof( out->worldMins ) );
		memcpy( out->worldMaxs, value->worldMaxs, sizeof( out->worldMaxs ) );
		out->gridSize	= value->gridSize;
		out->visibility = value->distance;
		if ( value->type != ATMOSPHERE_PRECIP_NONE ) {
			out->flags = ATMOSPHERE_FLAG_ENABLED;
			if ( value->gridSize > 0 )
				out->flags |= ATMOSPHERE_FLAG_HEIGHTGRID;
			out->qualityTier					= ATMOSPHERE_QUALITY_WEATHER;
			out->precipitation[value->type - 1] = 1.0f;
		}
		return qtrue;
	}
	if ( value->schemaVersion != WIRED_ATMOSPHERE_SCHEMA_VERSION || value->type < ATMOSPHERE_PRECIP_NONE ||
		 value->type > ATMOSPHERE_PRECIP_DUST_ASH || value->qualityTier > ATMOSPHERE_QUALITY_FULL ||
		 ( value->flags & ~knownFlags ) != 0u || value->gridSize < 0 || value->gridSize > 4096 ||
		 !AtmosphereReservedZero( value->reserved, 8u ) || !AtmosphereBoundsValid( value ) )
		return qfalse;
	{
		const float scalarValues[] = {
			value->distance,	   value->timelineSeconds, value->transitionSeconds, value->temperatureC,
			value->humidity,	   value->gustStrength,	   value->visibility,		 value->indoorExposure,
			value->surfaceWetness, value->surfaceFrost,	   value->snowAccumulation,	 value->meltRate,
			value->sunIntensity,   value->moonIntensity,   value->cloudCover,		 value->cloudShadow,
			value->lightning,	   value->mediaDensity,	   value->mediaHeightFalloff };
		if ( !AtmosphereFloatArrayFinite( scalarValues, sizeof( scalarValues ) / sizeof( scalarValues[0] ) ) ||
			 !AtmosphereFloatArrayFinite( value->wind, 3u ) || !AtmosphereFloatArrayFinite( value->sunDirection, 3u ) ||
			 !AtmosphereFloatArrayFinite( value->moonDirection, 3u ) ||
			 !AtmosphereFloatArrayFinite( value->ambientColor, 3u ) ||
			 !AtmosphereUnitArray( value->precipitation, 5u ) || !AtmosphereUnitArray( &value->humidity, 1u ) ||
			 !AtmosphereUnitArray( &value->indoorExposure, 1u ) || !AtmosphereUnitArray( &value->surfaceWetness, 4u ) ||
			 !AtmosphereUnitArray( &value->cloudCover, 3u ) )
			return qfalse;
	}
	if ( value->distance < 0.0f || value->timelineSeconds < 0.0f || value->transitionSeconds < 0.0f ||
		 value->visibility < 0.0f || value->gustStrength < 0.0f || value->sunIntensity < 0.0f ||
		 value->moonIntensity < 0.0f || value->mediaDensity < 0.0f || value->mediaHeightFalloff < 0.0f ||
		 value->ambientColor[0] < 0.0f || value->ambientColor[1] < 0.0f || value->ambientColor[2] < 0.0f ||
		 ( value->qualityTier == ATMOSPHERE_QUALITY_OFF && ( value->flags & ATMOSPHERE_FLAG_ENABLED ) != 0u ) )
		return qfalse;
	*out = *value;
	return qtrue;
}

static qboolean AtmosphereEmitterValid( const atmosphereEmitter_t *value )
{
	const float scalars[] = { value ? value->intensity : 0.0f, value ? value->radius : 0.0f,
							  value ? value->temperatureC : 0.0f, value ? value->humidity : 0.0f,
							  value ? value->lifetime : 0.0f };
	return ( value && value->schemaVersion == WIRED_ATMOSPHERE_SCHEMA_VERSION &&
			 value->kind >= ATMOSPHERE_EMITTER_BREATH && value->kind <= ATMOSPHERE_EMITTER_PRECIPITATION_IMPACT &&
			 value->id != 0u && value->flags == 0u && AtmosphereFloatArrayFinite( value->origin, 3u ) &&
			 AtmosphereFloatArrayFinite( value->direction, 3u ) && AtmosphereFloatArrayFinite( scalars, 5u ) &&
			 AtmosphereEmitterReservedZero( value->reserved, 5u ) && value->intensity >= 0.0f &&
			 value->intensity <= 1.0f && value->radius >= 0.0f && value->humidity >= 0.0f && value->humidity <= 1.0f &&
			 value->lifetime >= 0.0f )
			   ? qtrue
			   : qfalse;
}

static float AtmosphereClamp01( float value )
{
	if ( value <= 0.0f )
		return 0.0f;
	if ( value >= 1.0f )
		return 1.0f;
	return value;
}

static float AtmosphereMax( float a, float b )
{
	return a > b ? a : b;
}

float RenderSubmission_AtmosphereEmitterIntensity( const atmosphereFrameState_t *atmosphere,
												   const atmosphereEmitter_t	*emitter )
{
	atmosphereFrameState_t state;
	float				   context		 = 0.0f;
	float				   precipitation = 0.0f;
	float				   windSpeed;

	if ( !AtmosphereNormalize( atmosphere, &state ) || !AtmosphereEmitterValid( emitter ) ||
		 state.qualityTier == ATMOSPHERE_QUALITY_OFF || ( state.flags & ATMOSPHERE_FLAG_ENABLED ) == 0u )
		return 0.0f;

	for ( uint32_t i = 0u; i < 5u; ++i )
		precipitation += state.precipitation[i];
	precipitation = AtmosphereClamp01( precipitation );
	windSpeed = sqrtf( state.wind[0] * state.wind[0] + state.wind[1] * state.wind[1] + state.wind[2] * state.wind[2] );

	switch ( emitter->kind ) {
	case ATMOSPHERE_EMITTER_BREATH: {
		const float contrast = AtmosphereClamp01( ( emitter->temperatureC - state.temperatureC - 5.0f ) / 25.0f );
		const float cold	 = AtmosphereClamp01( ( 15.0f - state.temperatureC ) / 20.0f );
		context				 = AtmosphereMax( contrast, cold ) * ( 0.35f + 0.65f * emitter->humidity );
		break;
	}
	case ATMOSPHERE_EMITTER_STEAM:
		context = AtmosphereClamp01( ( emitter->temperatureC - state.temperatureC ) / 40.0f ) *
				  ( 0.25f + 0.75f * emitter->humidity );
		break;
	case ATMOSPHERE_EMITTER_GROUND_MIST: {
		const float cool = AtmosphereClamp01( ( 20.0f - state.temperatureC ) / 25.0f );
		const float moisture =
			AtmosphereMax( state.humidity, AtmosphereMax( state.surfaceWetness, emitter->humidity ) );
		context = cool * AtmosphereClamp01( moisture );
		break;
	}
	case ATMOSPHERE_EMITTER_SPRAY:
		context = AtmosphereClamp01(
			AtmosphereMax( emitter->humidity, AtmosphereMax( state.surfaceWetness, precipitation ) ) );
		break;
	case ATMOSPHERE_EMITTER_DEBRIS:
		context = AtmosphereClamp01( windSpeed / 18.0f + state.gustStrength / 24.0f );
		break;
	case ATMOSPHERE_EMITTER_PRECIPITATION_IMPACT:
		context = precipitation * state.indoorExposure;
		break;
	default:
		return 0.0f;
	}
	return AtmosphereClamp01( emitter->intensity * context );
}

qboolean RenderSubmission_AtmosphereSurfaceTargets( const atmosphereFrameState_t  *atmosphere,
													renderSurfaceClimateTargets_t *outTargets )
{
	atmosphereFrameState_t state;
	float				   rain, snow, freeze, thaw;
	if ( !outTargets || !AtmosphereNormalize( atmosphere, &state ) )
		return qfalse;
	memset( outTargets, 0, sizeof( *outTargets ) );
	if ( state.qualityTier < ATMOSPHERE_QUALITY_WEATHER ||
		 ( state.flags & ( ATMOSPHERE_FLAG_ENABLED | ATMOSPHERE_FLAG_SURFACE_CLIMATE ) ) !=
			 ( ATMOSPHERE_FLAG_ENABLED | ATMOSPHERE_FLAG_SURFACE_CLIMATE ) )
		return qtrue;

	rain =
		AtmosphereClamp01( state.precipitation[0] + 0.55f * state.precipitation[2] + 0.30f * state.precipitation[3] );
	snow				= AtmosphereClamp01( state.precipitation[1] + 0.45f * state.precipitation[2] );
	freeze				= AtmosphereClamp01( -state.temperatureC / 12.0f );
	thaw				= AtmosphereClamp01( state.temperatureC / 12.0f );
	outTargets->wetness = AtmosphereMax(
		state.surfaceWetness, state.indoorExposure * AtmosphereClamp01( rain + snow * ( 0.15f + 0.85f * thaw ) ) );
	outTargets->frost	= AtmosphereMax( state.surfaceFrost, state.humidity * state.indoorExposure * freeze );
	outTargets->snow	= AtmosphereMax( state.snowAccumulation, state.indoorExposure * snow * ( 1.0f - thaw ) );
	outTargets->melt	= AtmosphereMax( state.meltRate, thaw * ( 0.25f + 0.75f * ( 1.0f - state.humidity ) ) );
	outTargets->wetness = AtmosphereClamp01( outTargets->wetness );
	outTargets->frost	= AtmosphereClamp01( outTargets->frost );
	outTargets->snow	= AtmosphereClamp01( outTargets->snow );
	outTargets->melt	= AtmosphereClamp01( outTargets->melt );
	return qtrue;
}

static void AtmosphereSurfaceRebuildDigest( renderSubmissionState_t *state )
{
	state->surfaceClimateDigest = AtmosphereHashBytes( ATMOSPHERE_FNV_OFFSET, &state->surfaceClimateTileCount,
													   sizeof( state->surfaceClimateTileCount ) );
	state->surfaceClimateDigest =
		AtmosphereHashBytes( state->surfaceClimateDigest, state->surfaceClimateTiles,
							 (size_t)state->surfaceClimateTileCount * sizeof( state->surfaceClimateTiles[0] ) );
}

static void AtmosphereSurfaceAdvanceTile( renderSurfaceClimateTile_t		  *tile,
										  const renderSurfaceClimateTargets_t *targets, float timeline,
										  float transitionSeconds )
{
	float dt, blend;
	if ( !tile || !targets || timeline < tile->lastTimelineSeconds )
		return;
	dt	  = timeline - tile->lastTimelineSeconds;
	blend = transitionSeconds > 0.0f ? 1.0f - expf( -dt / transitionSeconds ) : 1.0f;
	blend = AtmosphereClamp01( blend );
	tile->wetness += ( targets->wetness - tile->wetness ) * blend;
	tile->frost += ( targets->frost - tile->frost ) * blend;
	tile->snow += ( targets->snow - tile->snow ) * blend;
	tile->melt += ( targets->melt - tile->melt ) * blend;
	tile->lastTimelineSeconds = timeline;
}

static qboolean AtmosphereSurfaceApplyState( renderSubmissionState_t *state, const atmosphereFrameState_t *next )
{
	renderSurfaceClimateTargets_t targets;
	if ( !state || !next || !RenderSubmission_AtmosphereSurfaceTargets( next, &targets ) )
		return qfalse;
	if ( next->timelineSeconds < state->atmosphere.timelineSeconds || next->qualityTier == ATMOSPHERE_QUALITY_OFF ||
		 ( next->flags & ATMOSPHERE_FLAG_SURFACE_CLIMATE ) == 0u ) {
		if ( state->surfaceClimateGeneration == UINT64_MAX )
			return qfalse;
		RenderSubmission_ResetAtmosphereSurface( state );
		return qtrue;
	}
	if ( state->surfaceClimateTileCount > 0u && state->surfaceClimateGeneration == UINT64_MAX )
		return qfalse;
	for ( uint32_t i = 0u; i < state->surfaceClimateTileCount; ++i )
		AtmosphereSurfaceAdvanceTile( &state->surfaceClimateTiles[i], &targets, next->timelineSeconds,
									  next->transitionSeconds );
	if ( state->surfaceClimateTileCount > 0u ) {
		state->surfaceClimateGeneration++;
		AtmosphereSurfaceRebuildDigest( state );
	}
	return qtrue;
}

static qboolean AtmosphereSurfaceEventValid( const atmosphereSurfaceEvent_t *event )
{
	const float scalars[] = { event ? event->radius : 0.0f, event ? event->strength : 0.0f,
							  event ? event->timelineSeconds : 0.0f, event ? event->lifetime : 0.0f };
	return ( event && event->schemaVersion == WIRED_ATMOSPHERE_SURFACE_EVENT_SCHEMA_VERSION &&
			 event->kind >= ATMOSPHERE_SURFACE_EVENT_FOOTPRINT && event->kind <= ATMOSPHERE_SURFACE_EVENT_TRAVERSAL &&
			 event->id != 0u && event->flags == 0u && AtmosphereFloatArrayFinite( event->origin, 3u ) &&
			 AtmosphereFloatArrayFinite( event->direction, 3u ) && AtmosphereFloatArrayFinite( scalars, 4u ) &&
			 AtmosphereReservedZero( event->reserved, 2u ) && event->radius >= 0.0f && event->radius <= 256.0f &&
			 event->strength >= 0.0f && event->strength <= 1.0f && event->timelineSeconds >= 0.0f &&
			 event->lifetime >= 0.0f && event->lifetime <= 3600.0f && fabsf( event->origin[0] ) <= 16777216.0f &&
			 fabsf( event->origin[1] ) <= 16777216.0f )
			   ? qtrue
			   : qfalse;
}

static renderSurfaceClimateTile_t *AtmosphereSurfaceFindOrAllocate( renderSubmissionState_t *state, int32_t tileX,
																	int32_t								 tileY,
																	const renderSurfaceClimateTargets_t *targets,
																	float								 timeline )
{
	renderSurfaceClimateTile_t *tile = NULL;
	for ( uint32_t i = 0u; i < state->surfaceClimateTileCount; ++i ) {
		renderSurfaceClimateTile_t *candidate = &state->surfaceClimateTiles[i];
		if ( candidate->tileX == tileX && candidate->tileY == tileY )
			return candidate;
		if ( !tile || candidate->lastTimelineSeconds < tile->lastTimelineSeconds ||
			 ( candidate->lastTimelineSeconds == tile->lastTimelineSeconds &&
			   candidate->generation < tile->generation ) )
			tile = candidate;
	}
	if ( state->surfaceClimateTileCount < RENDER_SUBMISSION_MAX_SURFACE_CLIMATE_TILES ) {
		tile = &state->surfaceClimateTiles[state->surfaceClimateTileCount++];
	} else {
		state->surfaceClimateEvictionCount++;
	}
	memset( tile, 0, sizeof( *tile ) );
	tile->tileX				  = tileX;
	tile->tileY				  = tileY;
	tile->wetness			  = targets->wetness;
	tile->frost				  = targets->frost;
	tile->snow				  = targets->snow;
	tile->melt				  = targets->melt;
	tile->lastTimelineSeconds = timeline;
	return tile;
}

qboolean RenderSubmission_AddAtmosphereSurfaceEvent( renderSubmissionState_t		*state,
													 const atmosphereSurfaceEvent_t *event )
{
	renderSurfaceClimateTargets_t targets;
	int32_t						  minX, maxX, minY, maxY;
	uint32_t					  affected = 0u;
	if ( !state || !state->frameOpen || !AtmosphereSurfaceEventValid( event ) ||
		 state->atmosphereSurfaceEventCount >= RENDER_SUBMISSION_MAX_ATMOSPHERE_SURFACE_EVENTS ||
		 state->surfaceClimateGeneration > UINT64_MAX - 26u || state->surfaceClimateEvictionCount > UINT32_MAX - 25u ||
		 state->atmosphere.qualityTier < ATMOSPHERE_QUALITY_WEATHER ||
		 ( state->atmosphere.flags & ( ATMOSPHERE_FLAG_ENABLED | ATMOSPHERE_FLAG_SURFACE_CLIMATE ) ) !=
			 ( ATMOSPHERE_FLAG_ENABLED | ATMOSPHERE_FLAG_SURFACE_CLIMATE ) ||
		 event->timelineSeconds != state->atmosphere.timelineSeconds ||
		 !RenderSubmission_AtmosphereSurfaceTargets( &state->atmosphere, &targets ) )
		return qfalse;
	for ( uint32_t i = 0u; i < state->atmosphereSurfaceEventCount; ++i )
		if ( state->atmosphereSurfaceEventIds[i] == event->id )
			return qfalse;
	minX = (int32_t)floorf( ( event->origin[0] - event->radius ) / RENDER_SUBMISSION_SURFACE_CLIMATE_TILE_SIZE );
	maxX = (int32_t)floorf( ( event->origin[0] + event->radius ) / RENDER_SUBMISSION_SURFACE_CLIMATE_TILE_SIZE );
	minY = (int32_t)floorf( ( event->origin[1] - event->radius ) / RENDER_SUBMISSION_SURFACE_CLIMATE_TILE_SIZE );
	maxY = (int32_t)floorf( ( event->origin[1] + event->radius ) / RENDER_SUBMISSION_SURFACE_CLIMATE_TILE_SIZE );
	if ( (uint64_t)( (int64_t)maxX - minX + 1 ) * (uint64_t)( (int64_t)maxY - minY + 1 ) > 25u )
		return qfalse;
	for ( int32_t y = minY; y <= maxY; ++y ) {
		for ( int32_t x = minX; x <= maxX; ++x ) {
			renderSurfaceClimateTile_t *tile =
				AtmosphereSurfaceFindOrAllocate( state, x, y, &targets, event->timelineSeconds );
			AtmosphereSurfaceAdvanceTile( tile, &targets, event->timelineSeconds, state->atmosphere.transitionSeconds );
			switch ( event->kind ) {
			case ATMOSPHERE_SURFACE_EVENT_FOOTPRINT:
				tile->snow *= 1.0f - 0.85f * event->strength;
				tile->frost *= 1.0f - 0.50f * event->strength;
				break;
			case ATMOSPHERE_SURFACE_EVENT_IMPACT:
				tile->wetness = AtmosphereMax( tile->wetness, event->strength * state->atmosphere.indoorExposure );
				tile->snow *= 1.0f - 0.70f * event->strength;
				tile->frost *= 1.0f - 0.25f * event->strength;
				break;
			case ATMOSPHERE_SURFACE_EVENT_TRAVERSAL:
				tile->snow *= 1.0f - 0.35f * event->strength;
				tile->frost *= 1.0f - 0.20f * event->strength;
				break;
			}
			tile->lastEventId = event->id;
			tile->eventMask |= 1u << event->kind;
			tile->generation = ++state->surfaceClimateGeneration;
			affected++;
		}
	}
	if ( affected == 0u )
		return qfalse;
	state->atmosphereSurfaceEventIds[state->atmosphereSurfaceEventCount++] = event->id;
	AtmosphereSurfaceRebuildDigest( state );
	return qtrue;
}

qboolean RenderSubmission_AtmosphereSurfaceSnapshot( const renderSubmissionState_t	   *state,
													 renderSurfaceClimateSnapshot_t	   *outSnapshot,
													 const renderSurfaceClimateTile_t **outTiles )
{
	if ( !state || !state->initialized || !state->atmosphereReady || !outSnapshot || !outTiles )
		return qfalse;
	memset( outSnapshot, 0, sizeof( *outSnapshot ) );
	outSnapshot->generation		 = state->surfaceClimateGeneration;
	outSnapshot->digest			 = state->surfaceClimateDigest;
	outSnapshot->tileCount		 = state->surfaceClimateTileCount;
	outSnapshot->frameEventCount = state->atmosphereSurfaceEventCount;
	outSnapshot->evictionCount	 = state->surfaceClimateEvictionCount;
	*outTiles					 = state->surfaceClimateTiles;
	return qtrue;
}

void RenderSubmission_ClearAtmosphereSurfaceEvents( renderSubmissionState_t *state )
{
	if ( !state )
		return;
	state->atmosphereSurfaceEventCount = 0u;
	memset( state->atmosphereSurfaceEventIds, 0, sizeof( state->atmosphereSurfaceEventIds ) );
}

void RenderSubmission_ResetAtmosphereSurface( renderSubmissionState_t *state )
{
	if ( !state )
		return;
	memset( state->surfaceClimateTiles, 0, sizeof( state->surfaceClimateTiles ) );
	state->surfaceClimateTileCount	   = 0u;
	state->surfaceClimateEvictionCount = 0u;
	RenderSubmission_ClearAtmosphereSurfaceEvents( state );
	if ( state->surfaceClimateGeneration != UINT64_MAX )
		state->surfaceClimateGeneration++;
	AtmosphereSurfaceRebuildDigest( state );
}

static void AtmosphereMediaRebuildDigest( renderSubmissionState_t *state )
{
	state->atmosphereMediaDigest = AtmosphereHashBytes( ATMOSPHERE_FNV_OFFSET, &state->atmosphereMediaVolumeCount,
														sizeof( state->atmosphereMediaVolumeCount ) );
	state->atmosphereMediaDigest =
		AtmosphereHashBytes( state->atmosphereMediaDigest, state->atmosphereMediaVolumes,
							 (size_t)state->atmosphereMediaVolumeCount * sizeof( state->atmosphereMediaVolumes[0] ) );
	state->atmosphereMediaDigest =
		AtmosphereHashBytes( state->atmosphereMediaDigest, &state->atmosphereMediaDroppedCount,
							 sizeof( state->atmosphereMediaDroppedCount ) );
}

static qboolean AtmosphereMediaVolumeValid( const atmosphereMediaVolume_t *volume )
{
	static const uint32_t knownFlags = ATMOSPHERE_MEDIA_VOLUME_CAST_SHADOW | ATMOSPHERE_MEDIA_VOLUME_NOISE;
	const float			  scalars[]	 = { volume ? volume->radius : 0.0f,		volume ? volume->extinction : 0.0f,
								 volume ? volume->anisotropy : 0.0f,	volume ? volume->emissionIntensity : 0.0f,
							  volume ? volume->heightFalloff : 0.0f, volume ? volume->noiseScale : 0.0f,
							  volume ? volume->timelineStart : 0.0f, volume ? volume->timelineEnd : 0.0f };
	if ( !volume || volume->schemaVersion != WIRED_ATMOSPHERE_MEDIA_VOLUME_SCHEMA_VERSION ||
		 volume->shape < ATMOSPHERE_MEDIA_VOLUME_SPHERE || volume->shape > ATMOSPHERE_MEDIA_VOLUME_BOX ||
		 volume->id == 0u || ( volume->flags & ~knownFlags ) != 0u ||
		 !AtmosphereFloatArrayFinite( volume->origin, 3u ) || !AtmosphereFloatArrayFinite( volume->extent, 3u ) ||
		 !AtmosphereUnitArray( volume->albedo, 3u ) || !AtmosphereFloatArrayFinite( volume->emissive, 3u ) ||
		 !AtmosphereFloatArrayFinite( scalars, sizeof( scalars ) / sizeof( scalars[0] ) ) ||
		 !AtmosphereReservedZero( volume->reserved, 4u ) || volume->radius <= 0.0f || volume->radius > 16384.0f ||
		 volume->extinction < 0.0f || volume->extinction > 64.0f || volume->anisotropy < -0.95f ||
		 volume->anisotropy > 0.95f || volume->emissionIntensity < 0.0f || volume->emissionIntensity > 65536.0f ||
		 volume->heightFalloff < 0.0f || volume->timelineStart < 0.0f ||
		 ( volume->timelineEnd != 0.0f && volume->timelineEnd < volume->timelineStart ) )
		return qfalse;
	for ( uint32_t i = 0u; i < 3u; ++i )
		if ( volume->emissive[i] < 0.0f || fabsf( volume->origin[i] ) > 16777216.0f )
			return qfalse;
	if ( volume->shape == ATMOSPHERE_MEDIA_VOLUME_SPHERE ) {
		if ( volume->extent[0] != 0.0f || volume->extent[1] != 0.0f || volume->extent[2] != 0.0f )
			return qfalse;
	} else {
		for ( uint32_t i = 0u; i < 3u; ++i )
			if ( volume->extent[i] <= 0.0f || volume->extent[i] > volume->radius )
				return qfalse;
	}
	return ( volume->flags & ATMOSPHERE_MEDIA_VOLUME_NOISE ) != 0u ? ( volume->noiseScale > 0.0f ? qtrue : qfalse )
																   : ( volume->noiseScale == 0.0f ? qtrue : qfalse );
}

qboolean RenderSubmission_AddAtmosphereMediaVolume( renderSubmissionState_t		  *state,
													const atmosphereMediaVolume_t *volume )
{
	uint32_t slot;
	if ( !state || !state->frameOpen || !AtmosphereMediaVolumeValid( volume ) ||
		 state->atmosphere.qualityTier < ATMOSPHERE_QUALITY_FULL ||
		 ( state->atmosphere.flags & ( ATMOSPHERE_FLAG_ENABLED | ATMOSPHERE_FLAG_VOLUMETRIC_MEDIA ) ) !=
			 ( ATMOSPHERE_FLAG_ENABLED | ATMOSPHERE_FLAG_VOLUMETRIC_MEDIA ) )
		return qfalse;
	for ( uint32_t i = 0u; i < state->atmosphereMediaVolumeCount; ++i )
		if ( state->atmosphereMediaVolumes[i].id == volume->id )
			return qfalse;
	if ( state->atmosphereMediaVolumeCount < RENDER_SUBMISSION_MAX_ATMOSPHERE_MEDIA_VOLUMES ) {
		slot = state->atmosphereMediaVolumeCount++;
	} else {
		slot = 0u;
		for ( uint32_t i = 1u; i < state->atmosphereMediaVolumeCount; ++i ) {
			const atmosphereMediaVolume_t *candidate = &state->atmosphereMediaVolumes[i];
			const atmosphereMediaVolume_t *victim	 = &state->atmosphereMediaVolumes[slot];
			if ( candidate->priority < victim->priority ||
				 ( candidate->priority == victim->priority && candidate->id > victim->id ) )
				slot = i;
		}
		if ( volume->priority < state->atmosphereMediaVolumes[slot].priority ||
			 ( volume->priority == state->atmosphereMediaVolumes[slot].priority &&
			   volume->id > state->atmosphereMediaVolumes[slot].id ) ) {
			if ( state->atmosphereMediaDroppedCount != UINT32_MAX )
				state->atmosphereMediaDroppedCount++;
			AtmosphereMediaRebuildDigest( state );
			return qtrue;
		}
		if ( state->atmosphereMediaDroppedCount == UINT32_MAX )
			return qfalse;
		state->atmosphereMediaDroppedCount++;
	}
	state->atmosphereMediaVolumes[slot] = *volume;
	AtmosphereMediaRebuildDigest( state );
	return qtrue;
}

qboolean RenderSubmission_AtmosphereMediaSnapshot( const renderSubmissionState_t   *state,
												   renderAtmosphereMediaSnapshot_t *outSnapshot,
												   const atmosphereMediaVolume_t  **outVolumes )
{
	if ( !state || !state->initialized || !state->atmosphereReady || !outSnapshot || !outVolumes )
		return qfalse;
	outSnapshot->digest		  = state->atmosphereMediaDigest;
	outSnapshot->count		  = state->atmosphereMediaVolumeCount;
	outSnapshot->droppedCount = state->atmosphereMediaDroppedCount;
	*outVolumes				  = state->atmosphereMediaVolumes;
	return qtrue;
}

void RenderSubmission_ClearAtmosphereMediaVolumes( renderSubmissionState_t *state )
{
	if ( !state )
		return;
	state->atmosphereMediaVolumeCount  = 0u;
	state->atmosphereMediaDroppedCount = 0u;
	AtmosphereMediaRebuildDigest( state );
}

static qboolean AtmosphereEffectStageValid( const atmosphereEffectStage_t *stage, uint32_t ordinal )
{
	static const uint32_t knownFlags = ATMOSPHERE_STAGE_INHERIT_POSITION | ATMOSPHERE_STAGE_INHERIT_VELOCITY |
									   ATMOSPHERE_STAGE_INHERIT_COLOR | ATMOSPHERE_STAGE_INHERIT_INTENSITY;
	const float scalars[] = { stage ? stage->spawnRate : 0.0f,	   stage ? stage->delay : 0.0f,
							  stage ? stage->duration : 0.0f,	   stage ? stage->lodNear : 0.0f,
							  stage ? stage->lodFar : 0.0f,		   stage ? stage->boundsRadius : 0.0f,
							  stage ? stage->intensityScale : 0.0f };
	return ( stage && stage->trigger <= ATMOSPHERE_STAGE_DEATH && stage->particleClass > 0u &&
			 stage->particleClass <= MAX_PARTICLE_CLASSES &&
			 ( stage->parentStage == UINT32_MAX || stage->parentStage < ordinal ) &&
			 ( stage->flags & ~knownFlags ) == 0u && stage->maxParticles > 0u &&
			 ( stage->burstCount > 0u || stage->spawnRate > 0.0f ) && stage->burstCount <= stage->maxParticles &&
			 AtmosphereFloatArrayFinite( scalars, 7u ) && stage->spawnRate >= 0.0f && stage->delay >= 0.0f &&
			 stage->duration > 0.0f && stage->lodNear >= 0.0f && stage->lodFar >= stage->lodNear &&
			 stage->boundsRadius >= 0.0f && stage->intensityScale >= 0.0f &&
			 AtmosphereReservedZero( stage->reserved, 3u ) )
			   ? qtrue
			   : qfalse;
}

static qboolean AtmosphereEffectProfileResolve( const renderSubmissionState_t *state, uint32_t handle,
												const atmosphereEffectProfile_t *value, atmosphereEffectProfile_t *out )
{
	uint64_t	stageBudget = 0u;
	const float scalars[]	= { value ? value->duration : 0.0f, value ? value->lodNear : 0.0f,
								value ? value->lodFar : 0.0f, value ? value->boundsRadius : 0.0f };
	if ( !state || !value || !out || handle == 0u || handle > ATMOSPHERE_EFFECT_MAX_PROFILES ||
		 value->schemaVersion != WIRED_ATMOSPHERE_EFFECT_PROFILE_SCHEMA_VERSION || value->stageCount == 0u ||
		 value->stageCount > ATMOSPHERE_EFFECT_MAX_STAGES || value->maxParticles == 0u || value->maxParticles > 8192u ||
		 value->flags != 0u || value->reserved0 != 0u || !AtmosphereReservedZero( value->reserved, 8u ) ||
		 !AtmosphereFloatArrayFinite( scalars, 4u ) || value->duration < 0.0f || value->lodNear < 0.0f ||
		 value->lodFar < value->lodNear || value->boundsRadius < 0.0f )
		return qfalse;
	*out = *value;
	if ( value->parentProfile != 0u ) {
		const uint32_t parent	 = value->parentProfile - 1u;
		const uint32_t validMask = ( 1u << value->stageCount ) - 1u;
		if ( value->parentProfile >= handle || parent >= ATMOSPHERE_EFFECT_MAX_PROFILES ||
			 !state->atmosphereProfileRegistered[parent] ||
			 state->atmosphereProfiles[parent].stageCount != value->stageCount || value->stageOverrideMask == 0u ||
			 ( value->stageOverrideMask & ~validMask ) != 0u )
			return qfalse;
		for ( uint32_t i = 0u; i < value->stageCount; ++i )
			if ( ( value->stageOverrideMask & ( 1u << i ) ) == 0u )
				out->stages[i] = state->atmosphereProfiles[parent].stages[i];
	} else if ( value->stageOverrideMask != 0u )
		return qfalse;
	for ( uint32_t i = 0u; i < out->stageCount; ++i ) {
		if ( !AtmosphereEffectStageValid( &out->stages[i], i ) )
			return qfalse;
		stageBudget += out->stages[i].maxParticles;
	}
	for ( uint32_t i = out->stageCount; i < ATMOSPHERE_EFFECT_MAX_STAGES; ++i ) {
		const atmosphereEffectStage_t zero = { 0 };
		if ( memcmp( &out->stages[i], &zero, sizeof( zero ) ) )
			return qfalse;
	}
	if ( stageBudget > out->maxParticles )
		return qfalse;
	return qtrue;
}

qboolean RenderSubmission_InitAtmosphere( renderSubmissionState_t *state )
{
	if ( !state || !state->initialized )
		return qfalse;
	AtmosphereDefaults( &state->atmosphere );
	state->atmosphereDigest =
		AtmosphereHashBytes( ATMOSPHERE_FNV_OFFSET, &state->atmosphere, sizeof( state->atmosphere ) );
	state->atmosphereEmitterDigest	   = ATMOSPHERE_FNV_OFFSET;
	state->atmosphereGeneration		   = 1u;
	state->atmosphereProfileDigest	   = ATMOSPHERE_FNV_OFFSET;
	state->atmosphereProfileGeneration = 1u;
	state->particleClassDigest		   = ATMOSPHERE_FNV_OFFSET;
	state->particleClassGeneration	   = 1u;
	state->surfaceClimateGeneration	   = 1u;
	state->surfaceClimateDigest		   = AtmosphereHashBytes( ATMOSPHERE_FNV_OFFSET, &state->surfaceClimateTileCount,
															  sizeof( state->surfaceClimateTileCount ) );
	state->atmosphereMediaDigest	   = AtmosphereHashBytes( ATMOSPHERE_FNV_OFFSET, &state->atmosphereMediaVolumeCount,
															  sizeof( state->atmosphereMediaVolumeCount ) );
	state->atmosphereMediaDigest =
		AtmosphereHashBytes( state->atmosphereMediaDigest, &state->atmosphereMediaDroppedCount,
							 sizeof( state->atmosphereMediaDroppedCount ) );
	state->atmosphereEmitterCount = 0u;
	state->atmosphereProfileCount = 0u;
	state->particleClassCount	  = 0u;
	state->atmosphereReady		  = qtrue;
	return qtrue;
}

qboolean RenderSubmission_RegisterAtmosphereEffectProfile( renderSubmissionState_t *state, uint32_t handle,
														   const atmosphereEffectProfile_t *profile )
{
	atmosphereEffectProfile_t resolved;
	const uint32_t			  slot = handle ? handle - 1u : UINT32_MAX;
	if ( !state || !state->initialized || !state->atmosphereReady ||
		 !AtmosphereEffectProfileResolve( state, handle, profile, &resolved ) )
		return qfalse;
	if ( state->atmosphereProfileRegistered[slot] )
		return !memcmp( &state->atmosphereProfiles[slot], &resolved, sizeof( resolved ) ) ? qtrue : qfalse;
	if ( state->atmosphereProfileGeneration == UINT64_MAX )
		return qfalse;
	state->atmosphereProfileCount++;
	state->atmosphereProfiles[slot]			 = resolved;
	state->atmosphereProfileRegistered[slot] = qtrue;
	state->atmosphereProfileDigest			 = ATMOSPHERE_FNV_OFFSET;
	for ( uint32_t i = 0u; i < ATMOSPHERE_EFFECT_MAX_PROFILES; ++i ) {
		if ( !state->atmosphereProfileRegistered[i] )
			continue;
		const uint32_t registeredHandle = i + 1u;
		state->atmosphereProfileDigest =
			AtmosphereHashBytes( state->atmosphereProfileDigest, &registeredHandle, sizeof( registeredHandle ) );
		state->atmosphereProfileDigest = AtmosphereHashBytes(
			state->atmosphereProfileDigest, &state->atmosphereProfiles[i], sizeof( state->atmosphereProfiles[i] ) );
	}
	state->atmosphereProfileGeneration++;
	return qtrue;
}

qboolean RenderSubmission_GetAtmosphereEffectProfile( const renderSubmissionState_t *state, uint32_t handle,
													  atmosphereEffectProfile_t *outProfile )
{
	const uint32_t slot = handle ? handle - 1u : UINT32_MAX;
	if ( !state || !outProfile || slot >= ATMOSPHERE_EFFECT_MAX_PROFILES || !state->atmosphereProfileRegistered[slot] )
		return qfalse;
	*outProfile = state->atmosphereProfiles[slot];
	return qtrue;
}

static qboolean ParticleClassValid( const particleClass_t *value )
{
	if ( !value || value->shader <= 0 || value->emitMode < EMIT_POINT || value->emitMode > EMIT_PATH ||
		 value->scatterShape < SCATTER_NONE || value->scatterShape > SCATTER_PERP_DISC ||
		 value->velocityShape < VEL_AXIAL || value->velocityShape > VEL_PURE_CUBE || value->paletteCount < 1 ||
		 value->paletteCount > PARTICLE_CLASS_MAX_PALETTE || value->frameCount < 0 ||
		 value->frameCount > PARTICLE_CLASS_MAX_FRAMES || ( value->frameBlend != 0 && value->frameBlend != 1 ) )
		return qfalse;
	{
		const float scalars[] = { value->scatterMagnitude, value->axialSpeed,	  value->cubeJitter,
								  value->coneHalfAngle,	   value->speedJitter,	  value->sizeJitter,
								  value->lifetimeMean,	   value->lifetimeJitter, value->sizeStart,
								  value->sizeEnd,		   value->gravityScale,	  value->drag };
		if ( !AtmosphereFloatArrayFinite( scalars, sizeof( scalars ) / sizeof( scalars[0] ) ) ||
			 !AtmosphereFloatArrayFinite( value->velocityBias, 3u ) ||
			 !AtmosphereFloatArrayFinite( value->velocityBiasJitter, 3u ) ||
			 !AtmosphereFloatArrayFinite( value->colorPalette[0], PARTICLE_CLASS_MAX_PALETTE * 4u ) ||
			 !AtmosphereFloatArrayFinite( value->colorEndMult, 4u ) )
			return qfalse;
	}
	return ( value->scatterMagnitude >= 0.0f && value->cubeJitter >= 0.0f && value->coneHalfAngle >= 0.0f &&
			 value->speedJitter >= 0.0f && value->sizeJitter >= 0.0f && value->lifetimeMean > 0.0f &&
			 value->lifetimeJitter >= 0.0f && value->sizeStart >= 0.0f && value->sizeEnd >= 0.0f &&
			 value->drag >= 0.0f )
			   ? qtrue
			   : qfalse;
}

qboolean RenderSubmission_RegisterParticleClass( renderSubmissionState_t *state, particleClassHandle_t handle,
												 const particleClass_t *particleClass )
{
	const uint32_t slot = handle > 0 ? (uint32_t)handle - 1u : UINT32_MAX;
	if ( !state || !state->initialized || !state->atmosphereReady || slot >= MAX_PARTICLE_CLASSES ||
		 !ParticleClassValid( particleClass ) )
		return qfalse;
	if ( state->particleClassRegistered[slot] )
		return !memcmp( &state->particleClasses[slot], particleClass, sizeof( *particleClass ) ) ? qtrue : qfalse;
	if ( state->particleClassGeneration == UINT64_MAX )
		return qfalse;
	state->particleClasses[slot]		 = *particleClass;
	state->particleClassRegistered[slot] = qtrue;
	state->particleClassCount++;
	state->particleClassDigest = ATMOSPHERE_FNV_OFFSET;
	for ( uint32_t i = 0u; i < MAX_PARTICLE_CLASSES; ++i ) {
		if ( !state->particleClassRegistered[i] )
			continue;
		const uint32_t registeredHandle = i + 1u;
		state->particleClassDigest =
			AtmosphereHashBytes( state->particleClassDigest, &registeredHandle, sizeof( registeredHandle ) );
		state->particleClassDigest = AtmosphereHashBytes( state->particleClassDigest, &state->particleClasses[i],
														  sizeof( state->particleClasses[i] ) );
	}
	state->particleClassGeneration++;
	return qtrue;
}

qboolean RenderSubmission_GetParticleClass( const renderSubmissionState_t *state, particleClassHandle_t handle,
											particleClass_t *outParticleClass )
{
	const uint32_t slot = handle > 0 ? (uint32_t)handle - 1u : UINT32_MAX;
	if ( !state || !outParticleClass || slot >= MAX_PARTICLE_CLASSES || !state->particleClassRegistered[slot] )
		return qfalse;
	*outParticleClass = state->particleClasses[slot];
	return qtrue;
}

qboolean RenderSubmission_AtmosphereEffectWorkloadSnapshot( const renderSubmissionState_t *state, uint32_t maxParticles,
															renderAtmosphereEffectWorkloadSnapshot_t *out )
{
	uint64_t requested = 0u;
	uint32_t remaining;
	if ( !state || !state->initialized || !state->atmosphereReady || !out ||
		 ( !state->frameOpen && !state->frameSealed ) )
		return qfalse;
	memset( out, 0, sizeof( *out ) );
	out->sourceEmitterCount = state->atmosphereEmitterCount;
	remaining				= maxParticles;
	for ( uint32_t i = 0u; i < state->atmosphereEmitterCount; ++i ) {
		const atmosphereEmitter_t	  *emitter = &state->atmosphereEmitters[i];
		atmosphereEffectProfile_t	   profile;
		particleClass_t				   particleClass;
		const atmosphereEffectStage_t *stage = NULL;
		float						   intensity, liveSeconds, desiredFloat;
		uint32_t					   desired, admitted;
		if ( emitter->profile == 0u ||
			 !RenderSubmission_GetAtmosphereEffectProfile( state, emitter->profile, &profile ) ) {
			out->droppedEmitterCount++;
			continue;
		}
		for ( uint32_t j = 0u; j < profile.stageCount; ++j ) {
			if ( profile.stages[j].parentStage == UINT32_MAX &&
				 ( profile.stages[j].trigger == ATMOSPHERE_STAGE_EMISSION ||
				   profile.stages[j].trigger == ATMOSPHERE_STAGE_CONTINUOUS ) ) {
				stage = &profile.stages[j];
				break;
			}
		}
		if ( !stage || !RenderSubmission_GetParticleClass( state, (particleClassHandle_t)stage->particleClass,
														   &particleClass ) ) {
			out->droppedEmitterCount++;
			continue;
		}
		intensity = RenderSubmission_AtmosphereEmitterIntensity( &state->atmosphere, emitter ) * stage->intensityScale;
		if ( intensity <= 0.0f )
			continue;
		liveSeconds = particleClass.lifetimeMean + particleClass.lifetimeJitter;
		if ( liveSeconds > stage->duration )
			liveSeconds = stage->duration;
		if ( liveSeconds < 0.001f )
			liveSeconds = 0.001f;
		desiredFloat =
			stage->trigger == ATMOSPHERE_STAGE_EMISSION ? (float)stage->burstCount : stage->spawnRate * liveSeconds;
		desiredFloat *= intensity;
		desired = desiredFloat > 0.0f ? (uint32_t)ceilf( desiredFloat ) : 0u;
		if ( desired > stage->maxParticles )
			desired = stage->maxParticles;
		if ( desired > profile.maxParticles )
			desired = profile.maxParticles;
		requested += desired;
		if ( desired == 0u )
			continue;
		if ( out->admittedEmitterCount >= RENDER_SUBMISSION_MAX_ATMOSPHERE_EFFECT_WORKLOADS ) {
			out->droppedEmitterCount++;
			continue;
		}
		admitted = desired < remaining ? desired : remaining;
		if ( admitted == 0u ) {
			out->droppedEmitterCount++;
			continue;
		}
		{
			renderAtmosphereEffectWorkload_t *work = &out->workloads[out->admittedEmitterCount++];
			work->emitter						   = *emitter;
			work->stage							   = *stage;
			work->particleClass					   = particleClass;
			work->firstParticle					   = maxParticles - remaining;
			work->particleCount					   = admitted;
			work->effectiveIntensity			   = intensity;
		}
		remaining -= admitted;
		out->admittedParticleCount += admitted;
	}
	out->requestedParticleCount = requested > UINT32_MAX ? UINT32_MAX : (uint32_t)requested;
	out->droppedParticleCount	= out->requestedParticleCount - out->admittedParticleCount;
	out->digest					= AtmosphereHashBytes( state->particleClassDigest, &out->sourceEmitterCount,
													   sizeof( *out ) -
														   offsetof( renderAtmosphereEffectWorkloadSnapshot_t, sourceEmitterCount ) );
	return qtrue;
}

qboolean RenderSubmission_AtmosphereEffectGpuPayload(
	const renderSubmissionState_t *state, uint32_t maxParticles, uint32_t firstParticle,
	renderAtmosphereEffectGpuWorkload_t		  outWorkloads[RENDER_SUBMISSION_MAX_ATMOSPHERE_EFFECT_WORKLOADS],
	renderAtmosphereEffectWorkloadSnapshot_t *outSnapshot )
{
	if ( !outWorkloads || !outSnapshot || firstParticle > UINT32_MAX - maxParticles ||
		 !RenderSubmission_AtmosphereEffectWorkloadSnapshot( state, maxParticles, outSnapshot ) )
		return qfalse;
	memset( outWorkloads, 0, sizeof( *outWorkloads ) * RENDER_SUBMISSION_MAX_ATMOSPHERE_EFFECT_WORKLOADS );
	for ( uint32_t i = 0u; i < outSnapshot->admittedEmitterCount; ++i ) {
		const renderAtmosphereEffectWorkload_t *source = &outSnapshot->workloads[i];
		renderAtmosphereEffectGpuWorkload_t	   *target = &outWorkloads[i];
		memcpy( target->vectors[0], source->emitter.origin, 3u * sizeof( float ) );
		target->vectors[0][3] = source->emitter.radius;
		memcpy( target->vectors[1], source->emitter.direction, 3u * sizeof( float ) );
		target->vectors[1][3] = source->effectiveIntensity;
		memcpy( target->vectors[2], source->particleClass.colorPalette[0], 4u * sizeof( float ) );
		target->vectors[3][0] = source->particleClass.axialSpeed;
		target->vectors[3][1] = source->particleClass.cubeJitter;
		target->vectors[3][2] = source->particleClass.gravityScale;
		target->vectors[3][3] = source->particleClass.drag;
		target->vectors[4][0] = (float)( firstParticle + source->firstParticle );
		target->vectors[4][1] = target->vectors[4][0] + (float)source->particleCount;
		target->vectors[4][2] = source->particleClass.sizeStart;
		target->vectors[4][3] = source->particleClass.sizeEnd;
		target->vectors[5][0] = source->particleClass.lifetimeMean;
		target->vectors[5][1] = source->particleClass.lifetimeJitter;
		target->vectors[5][2] = source->particleClass.scatterMagnitude;
		target->vectors[5][3] = (float)( source->emitter.seed & 0x00ffffffu );
	}
	return qtrue;
}

qboolean RenderSubmission_SetAtmosphere( renderSubmissionState_t *state, const atmosphereFrameState_t *atmosphere )
{
	atmosphereFrameState_t normalized;
	uint64_t			   digest;
	if ( !state || !state->initialized || !state->atmosphereReady || !AtmosphereNormalize( atmosphere, &normalized ) )
		return qfalse;
	digest = AtmosphereHashBytes( ATMOSPHERE_FNV_OFFSET, &normalized, sizeof( normalized ) );
	if ( digest == state->atmosphereDigest && !memcmp( &normalized, &state->atmosphere, sizeof( normalized ) ) )
		return qtrue;
	if ( state->atmosphereGeneration == UINT64_MAX )
		return qfalse;
	if ( !AtmosphereSurfaceApplyState( state, &normalized ) )
		return qfalse;
	state->atmosphere		= normalized;
	state->atmosphereDigest = digest;
	state->atmosphereGeneration++;
	return qtrue;
}

qboolean RenderSubmission_AddAtmosphereEmitter( renderSubmissionState_t *state, const atmosphereEmitter_t *emitter )
{
	if ( !state || !state->frameOpen || !AtmosphereEmitterValid( emitter ) ||
		 ( emitter->profile > 0u && ( emitter->profile > ATMOSPHERE_EFFECT_MAX_PROFILES ||
									  !state->atmosphereProfileRegistered[emitter->profile - 1u] ) ) ||
		 state->atmosphereEmitterCount >= RENDER_SUBMISSION_MAX_ATMOSPHERE_EMITTERS )
		return qfalse;
	for ( uint32_t i = 0u; i < state->atmosphereEmitterCount; ++i )
		if ( state->atmosphereEmitters[i].id == emitter->id )
			return qfalse;
	state->atmosphereEmitters[state->atmosphereEmitterCount++] = *emitter;
	state->atmosphereEmitterDigest = AtmosphereHashBytes( state->atmosphereEmitterDigest, emitter, sizeof( *emitter ) );
	return qtrue;
}

void RenderSubmission_ClearAtmosphereEmitters( renderSubmissionState_t *state )
{
	if ( !state )
		return;
	state->atmosphereEmitterCount  = 0u;
	state->atmosphereEmitterDigest = ATMOSPHERE_FNV_OFFSET;
}

uint64_t RenderSubmission_AtmosphereDigest( const renderSubmissionState_t *state )
{
	uint64_t digest;
	if ( !state || !state->initialized || !state->atmosphereReady )
		return 0u;
	digest = AtmosphereHashBytes( state->atmosphereDigest, &state->atmosphereEmitterDigest,
								  sizeof( state->atmosphereEmitterDigest ) );
	digest = AtmosphereHashBytes( digest, &state->atmosphereEmitterCount, sizeof( state->atmosphereEmitterCount ) );
	digest = AtmosphereHashBytes( digest, &state->atmosphereProfileDigest, sizeof( state->atmosphereProfileDigest ) );
	digest = AtmosphereHashBytes( digest, &state->particleClassDigest, sizeof( state->particleClassDigest ) );
	digest = AtmosphereHashBytes( digest, &state->atmosphereProfileCount, sizeof( state->atmosphereProfileCount ) );
	digest = AtmosphereHashBytes( digest, &state->surfaceClimateDigest, sizeof( state->surfaceClimateDigest ) );
	digest = AtmosphereHashBytes( digest, &state->atmosphereSurfaceEventCount,
								  sizeof( state->atmosphereSurfaceEventCount ) );
	digest = AtmosphereHashBytes( digest, &state->atmosphereMediaDigest, sizeof( state->atmosphereMediaDigest ) );
	digest =
		AtmosphereHashBytes( digest, &state->atmosphereMediaVolumeCount, sizeof( state->atmosphereMediaVolumeCount ) );
	return AtmosphereHashBytes( digest, &state->surfaceClimateEvictionCount,
								sizeof( state->surfaceClimateEvictionCount ) );
}

qboolean RenderSubmission_AtmosphereSnapshot( const renderSubmissionState_t *state,
											  renderAtmosphereSnapshot_t	*outSnapshot,
											  const atmosphereEmitter_t	   **outEmitters )
{
	if ( !state || !state->initialized || !state->atmosphereReady || !outSnapshot || !outEmitters )
		return qfalse;
	memset( outSnapshot, 0, sizeof( *outSnapshot ) );
	outSnapshot->state		  = state->atmosphere;
	outSnapshot->generation	  = state->atmosphereGeneration;
	outSnapshot->digest		  = RenderSubmission_AtmosphereDigest( state );
	outSnapshot->emitterCount = state->atmosphereEmitterCount;
	outSnapshot->active =
		( state->atmosphereEmitterCount > 0u || ( state->atmosphere.flags & ATMOSPHERE_FLAG_ENABLED ) != 0u ) ? qtrue
																											  : qfalse;
	*outEmitters = state->atmosphereEmitters;
	return qtrue;
}

qboolean RenderSubmission_EffectSnapshots( const renderSubmissionState_t *state,
										   const renderPolyCommand_t **outPolygons, uint32_t *outPolygonCommandCount,
										   const polyVert_t **outVertices, uint32_t *outVertexCount,
										   const renderLightCommand_t **outLights, uint32_t *outLightCount )
{
	if ( !state || !state->initialized || ( !state->frameOpen && !state->frameSealed ) || !outPolygons ||
		 !outPolygonCommandCount || !outVertices || !outVertexCount || !outLights || !outLightCount )
		return qfalse;
	*outPolygons			= state->polyCommands;
	*outPolygonCommandCount = state->polyCommandCount;
	*outVertices			= state->polyVertices;
	*outVertexCount			= state->polyVertexCount;
	*outLights				= state->lights;
	*outLightCount			= state->lightCount;
	return qtrue;
}

static qboolean EffectVectorFinite( const float *values, uint32_t count )
{
	return AtmosphereFloatArrayFinite( values, count );
}

static qboolean EffectColorValid( const float color[4] )
{
	if ( !EffectVectorFinite( color, 4u ) ) return qfalse;
	for ( uint32_t i = 0u; i < 4u; ++i )
		if ( color[i] < 0.0f ) return qfalse;
	return qtrue;
}

static qboolean EffectNormalValid( const float normal[3] )
{
	float lengthSquared;
	if ( !EffectVectorFinite( normal, 3u ) ) return qfalse;
	lengthSquared = normal[0] * normal[0] + normal[1] * normal[1]
		+ normal[2] * normal[2];
	return lengthSquared > 0.000001f && isfinite( lengthSquared );
}

static qboolean EffectDrop( renderSubmissionState_t *state )
{
	if ( state && state->initialized
			&& state->effectPrimitiveDroppedCount != UINT32_MAX )
		state->effectPrimitiveDroppedCount++;
	return qfalse;
}

qboolean RenderSubmission_AddEffectSprite( renderSubmissionState_t *state,
		const spriteDesc_t *sprite )
{
	if ( !state || !state->initialized || !state->frameOpen || !sprite
			|| state->effectSpriteCount >= RENDER_SUBMISSION_MAX_EFFECT_SPRITES
			|| sprite->shader <= 0 || !isfinite( sprite->radius )
			|| sprite->radius <= 0.0f || !EffectVectorFinite( sprite->origin, 3u )
			|| !EffectColorValid( sprite->rgba ) ) return EffectDrop( state );
	state->effectSprites[state->effectSpriteCount++] = *sprite;
	state->effectPrimitiveDigest = AtmosphereHashBytes(
		state->effectPrimitiveDigest, sprite->origin, sizeof( sprite->origin ) );
	state->effectPrimitiveDigest = AtmosphereHashBytes(
		state->effectPrimitiveDigest, &sprite->radius, sizeof( sprite->radius ) );
	state->effectPrimitiveDigest = AtmosphereHashBytes(
		state->effectPrimitiveDigest, sprite->rgba, sizeof( sprite->rgba ) );
	state->effectPrimitiveDigest = AtmosphereHashBytes(
		state->effectPrimitiveDigest, &sprite->shader, sizeof( sprite->shader ) );
	state->effectPrimitiveDigest = AtmosphereHashBytes(
		state->effectPrimitiveDigest, &sprite->flags, sizeof( sprite->flags ) );
	return qtrue;
}

qboolean RenderSubmission_AddEffectEmitter( renderSubmissionState_t *state,
		const emitterDesc_t *emitter )
{
	uint32_t slot;
	if ( !state || !state->initialized || !state->frameOpen || !emitter
			|| state->effectEmitterCount >= RENDER_SUBMISSION_MAX_EFFECT_EMITTERS
			|| emitter->cls <= 0 || emitter->count <= 0
			|| emitter->count > 8192 || !EffectVectorFinite( emitter->origin, 3u )
			|| !EffectVectorFinite( emitter->axis, 3u )
			|| !EffectVectorFinite( emitter->end, 3u )
			|| !EffectColorValid( emitter->colorTint ) ) return EffectDrop( state );
	slot = (uint32_t)emitter->cls - 1u;
	if ( slot >= MAX_PARTICLE_CLASSES || !state->particleClassRegistered[slot] )
		return EffectDrop( state );
	state->effectEmitters[state->effectEmitterCount++] = *emitter;
	state->effectPrimitiveDigest = AtmosphereHashBytes(
		state->effectPrimitiveDigest, &emitter->cls, sizeof( emitter->cls ) );
	state->effectPrimitiveDigest = AtmosphereHashBytes(
		state->effectPrimitiveDigest, &emitter->count, sizeof( emitter->count ) );
	state->effectPrimitiveDigest = AtmosphereHashBytes(
		state->effectPrimitiveDigest, emitter->origin, sizeof( emitter->origin ) );
	state->effectPrimitiveDigest = AtmosphereHashBytes(
		state->effectPrimitiveDigest, emitter->axis, sizeof( emitter->axis ) );
	state->effectPrimitiveDigest = AtmosphereHashBytes(
		state->effectPrimitiveDigest, emitter->end, sizeof( emitter->end ) );
	state->effectPrimitiveDigest = AtmosphereHashBytes(
		state->effectPrimitiveDigest, emitter->colorTint,
		sizeof( emitter->colorTint ) );
	state->effectPrimitiveDigest = AtmosphereHashBytes(
		state->effectPrimitiveDigest, &emitter->flags, sizeof( emitter->flags ) );
	return qtrue;
}

qboolean RenderSubmission_AddEffectDecal( renderSubmissionState_t *state,
		const decalDesc_t *decal )
{
	if ( !state || !state->initialized || !state->frameOpen || !decal
			|| state->effectDecalCount >= RENDER_SUBMISSION_MAX_EFFECT_DECALS
			|| decal->shader <= 0 || !isfinite( decal->radius )
			|| decal->radius <= 0.0f || !isfinite( decal->orientation )
			|| !isfinite( decal->lifetime ) || decal->lifetime < 0.0f
			|| !EffectVectorFinite( decal->origin, 3u )
			|| !EffectNormalValid( decal->normal )
			|| !EffectColorValid( decal->rgba ) || decal->reserved[0] != 0 )
		return EffectDrop( state );
	state->effectDecals[state->effectDecalCount++] = *decal;
	state->effectPrimitiveDigest = AtmosphereHashBytes(
		state->effectPrimitiveDigest, decal->origin, sizeof( decal->origin ) );
	state->effectPrimitiveDigest = AtmosphereHashBytes(
		state->effectPrimitiveDigest, decal->normal, sizeof( decal->normal ) );
	state->effectPrimitiveDigest = AtmosphereHashBytes(
		state->effectPrimitiveDigest, &decal->radius, sizeof( decal->radius ) );
	state->effectPrimitiveDigest = AtmosphereHashBytes(
		state->effectPrimitiveDigest, &decal->orientation,
		sizeof( decal->orientation ) );
	state->effectPrimitiveDigest = AtmosphereHashBytes(
		state->effectPrimitiveDigest, decal->rgba, sizeof( decal->rgba ) );
	state->effectPrimitiveDigest = AtmosphereHashBytes(
		state->effectPrimitiveDigest, &decal->shader, sizeof( decal->shader ) );
	state->effectPrimitiveDigest = AtmosphereHashBytes(
		state->effectPrimitiveDigest, &decal->flags, sizeof( decal->flags ) );
	state->effectPrimitiveDigest = AtmosphereHashBytes(
		state->effectPrimitiveDigest, &decal->lifetime,
		sizeof( decal->lifetime ) );
	return qtrue;
}

qboolean RenderSubmission_AddEffectRibbon( renderSubmissionState_t *state,
		const ribbonDesc_t *ribbon )
{
	renderEffectRibbonCommand_t *command;
	uint32_t pointCount;
	if ( !state || !state->initialized || !state->frameOpen || !ribbon
			|| !ribbon->points || ribbon->numPoints < 2
			|| ribbon->numPoints > RIBBON_MAX_POINTS || ribbon->shader <= 0
			|| state->effectRibbonCount >= RENDER_SUBMISSION_MAX_EFFECT_RIBBONS )
		return EffectDrop( state );
	pointCount = (uint32_t)ribbon->numPoints;
	if ( pointCount > RENDER_SUBMISSION_MAX_EFFECT_RIBBON_POINTS
			- state->effectRibbonPointCount
			|| !EffectVectorFinite( ribbon->uvScroll, 2u ) )
		return EffectDrop( state );
	for ( uint32_t i = 0u; i < pointCount; ++i ) {
		const ribbonPoint_t *point = &ribbon->points[i];
		if ( !EffectVectorFinite( point->pos, 3u )
				|| !isfinite( point->width ) || point->width <= 0.0f
				|| !EffectColorValid( point->rgba )
				|| !EffectVectorFinite( point->normal, 3u ) )
			return EffectDrop( state );
	}
	command = &state->effectRibbons[state->effectRibbonCount++];
	memset( command, 0, sizeof( *command ) );
	command->firstPoint = state->effectRibbonPointCount;
	command->pointCount = pointCount;
	command->material = ribbon->shader;
	command->flags = (uint32_t)ribbon->flags;
	memcpy( command->uvScroll, ribbon->uvScroll, sizeof( command->uvScroll ) );
	for ( uint32_t i = 0u; i < pointCount; ++i ) {
		ribbonPoint_t *target = &state->effectRibbonPoints[
			state->effectRibbonPointCount + i];
		*target = ribbon->points[i];
		target->_pad = 0.0f;
	}
	state->effectRibbonPointCount += pointCount;
	state->effectPrimitiveDigest = AtmosphereHashBytes(
		state->effectPrimitiveDigest, command, sizeof( *command ) );
	state->effectPrimitiveDigest = AtmosphereHashBytes(
		state->effectPrimitiveDigest,
		state->effectRibbonPoints + command->firstPoint,
		(size_t)pointCount * sizeof( ribbonPoint_t ) );
	return qtrue;
}

qboolean RenderSubmission_EffectPrimitiveSnapshots(
		const renderSubmissionState_t *state,
		renderEffectPrimitiveSnapshot_t *outSnapshot,
		const spriteDesc_t **outSprites,
		const emitterDesc_t **outEmitters,
		const decalDesc_t **outDecals,
		const renderEffectRibbonCommand_t **outRibbons,
		const ribbonPoint_t **outRibbonPoints )
{
	if ( !state || !state->initialized
			|| ( !state->frameOpen && !state->frameSealed ) || !outSnapshot
			|| !outSprites || !outEmitters || !outDecals || !outRibbons
			|| !outRibbonPoints ) return qfalse;
	memset( outSnapshot, 0, sizeof( *outSnapshot ) );
	outSnapshot->digest = state->effectPrimitiveDigest;
	outSnapshot->spriteCount = state->effectSpriteCount;
	outSnapshot->emitterCount = state->effectEmitterCount;
	outSnapshot->decalCount = state->effectDecalCount;
	outSnapshot->ribbonCount = state->effectRibbonCount;
	outSnapshot->ribbonPointCount = state->effectRibbonPointCount;
	outSnapshot->droppedCount = state->effectPrimitiveDroppedCount;
	*outSprites = state->effectSprites;
	*outEmitters = state->effectEmitters;
	*outDecals = state->effectDecals;
	*outRibbons = state->effectRibbons;
	*outRibbonPoints = state->effectRibbonPoints;
	return qtrue;
}

uint32_t RenderSubmission_AtmosphereLightCount( const renderSubmissionState_t *state )
{
	uint32_t count = 0u;
	if ( !state || !state->initialized || ( !state->frameOpen && !state->frameSealed ) )
		return 0u;
	for ( uint32_t index = 0u; index < state->lightCount; ++index )
		if ( state->lights[index].sourceFlags & RAL_LIGHT_INJECT_ATMOSPHERE ) count++;
	return count;
}

uint32_t RenderSubmission_AtmosphereShadowedLightCount( const renderSubmissionState_t *state )
{
	uint32_t count = 0u;
	if ( !state || !state->initialized || ( !state->frameOpen && !state->frameSealed ) )
		return 0u;
	for ( uint32_t index = 0u; index < state->lightCount; ++index )
		if ( ( state->lights[index].sourceFlags &
			( RAL_LIGHT_INJECT_ATMOSPHERE | RAL_LIGHT_CAST_SHADOW ) ) ==
			( RAL_LIGHT_INJECT_ATMOSPHERE | RAL_LIGHT_CAST_SHADOW ) ) count++;
	return count;
}
