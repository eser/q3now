// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#ifndef WIRED_RAL_ATMOSPHERE_CONFORMANCE_H
#define WIRED_RAL_ATMOSPHERE_CONFORMANCE_H

#include "wired/render/primitives.h"

#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

// Canonical authored inputs used by every native atmosphere backend fixture.
// Keeping the inputs here prevents a backend from earning parity with an easier
// local interpretation of "rain", "cold" or "storm".
typedef enum {
	RAL_ATMOSPHERE_FIXTURE_CLEAR = 0,
	RAL_ATMOSPHERE_FIXTURE_RAIN,
	RAL_ATMOSPHERE_FIXTURE_SNOW,
	RAL_ATMOSPHERE_FIXTURE_COLD,
	RAL_ATMOSPHERE_FIXTURE_FOG,
	RAL_ATMOSPHERE_FIXTURE_STORM,
	RAL_ATMOSPHERE_FIXTURE_COUNT
} ralAtmosphereFixture_t;

typedef struct {
	const char			 *name;
	atmosphereFrameState_t state;
	uint32_t			  familyMask;
	qboolean			  breathEmitter;
	qboolean			  localMedia;
} ralAtmosphereFixtureState_t;

static inline qboolean Ral_AtmosphereConformanceFixture(
		ralAtmosphereFixture_t fixture, ralAtmosphereFixtureState_t *out )
{
	atmosphereFrameState_t *state;
	if ( !out || fixture < RAL_ATMOSPHERE_FIXTURE_CLEAR ||
			fixture >= RAL_ATMOSPHERE_FIXTURE_COUNT )
		return qfalse;
	memset( out, 0, sizeof( *out ) );
	state = &out->state;
	state->schemaVersion = WIRED_ATMOSPHERE_SCHEMA_VERSION;
	state->flags = ATMOSPHERE_FLAG_ENABLED | ATMOSPHERE_FLAG_SKY_LIGHTING;
	state->seed = 216u + (uint32_t)fixture;
	state->timelineSeconds = (float)fixture;
	state->bounds[0] = state->bounds[1] = state->bounds[2] = -512.0f;
	state->bounds[3] = state->bounds[4] = state->bounds[5] = 512.0f;
	state->distance = 1024.0f;
	state->temperatureC = 15.0f;
	state->humidity = 0.5f;
	state->indoorExposure = 1.0f;
	state->sunDirection[2] = 1.0f;
	state->sunIntensity = 1.0f;
	state->ambientColor[0] = 0.12f;
	state->ambientColor[1] = 0.16f;
	state->ambientColor[2] = 0.22f;
	state->transitionSeconds = 2.0f;

	switch ( fixture ) {
	case RAL_ATMOSPHERE_FIXTURE_CLEAR:
		out->name = "clear";
		state->qualityTier = ATMOSPHERE_QUALITY_ANALYTIC;
		break;
	case RAL_ATMOSPHERE_FIXTURE_RAIN:
		out->name = "rain";
		state->qualityTier = ATMOSPHERE_QUALITY_WEATHER;
		state->flags |= ATMOSPHERE_FLAG_INDOOR_EXPOSURE |
			ATMOSPHERE_FLAG_SURFACE_CLIMATE;
		state->type = ATMOSPHERE_PRECIP_RAIN;
		state->temperatureC = 8.0f;
		state->humidity = 0.92f;
		state->precipitation[0] = 0.75f;
		state->surfaceWetness = 0.8f;
		out->familyMask = 1u << 0;
		break;
	case RAL_ATMOSPHERE_FIXTURE_SNOW:
		out->name = "snow";
		state->qualityTier = ATMOSPHERE_QUALITY_WEATHER;
		state->flags |= ATMOSPHERE_FLAG_INDOOR_EXPOSURE |
			ATMOSPHERE_FLAG_SURFACE_CLIMATE;
		state->type = ATMOSPHERE_PRECIP_SNOW;
		state->temperatureC = -6.0f;
		state->humidity = 0.86f;
		state->precipitation[1] = 0.75f;
		state->surfaceFrost = 0.6f;
		state->snowAccumulation = 0.7f;
		out->familyMask = 1u << 1;
		break;
	case RAL_ATMOSPHERE_FIXTURE_COLD:
		out->name = "cold";
		state->qualityTier = ATMOSPHERE_QUALITY_WEATHER;
		state->flags |= ATMOSPHERE_FLAG_SURFACE_CLIMATE;
		state->temperatureC = -12.0f;
		state->humidity = 0.88f;
		state->surfaceFrost = 0.9f;
		out->breathEmitter = qtrue;
		break;
	case RAL_ATMOSPHERE_FIXTURE_FOG:
		out->name = "fog";
		state->qualityTier = ATMOSPHERE_QUALITY_FULL;
		state->flags |= ATMOSPHERE_FLAG_VOLUMETRIC_MEDIA;
		state->visibility = 1024.0f;
		state->mediaDensity = 0.002f;
		state->mediaHeightFalloff = 0.002f;
		out->localMedia = qtrue;
		break;
	case RAL_ATMOSPHERE_FIXTURE_STORM:
		out->name = "storm";
		state->qualityTier = ATMOSPHERE_QUALITY_FULL;
		state->flags |= ATMOSPHERE_FLAG_INDOOR_EXPOSURE |
			ATMOSPHERE_FLAG_SURFACE_CLIMATE |
			ATMOSPHERE_FLAG_VOLUMETRIC_MEDIA;
		state->type = ATMOSPHERE_PRECIP_RAIN;
		state->temperatureC = 3.0f;
		state->humidity = 1.0f;
		state->wind[0] = 180.0f;
		state->wind[1] = 60.0f;
		state->gustStrength = 0.85f;
		state->precipitation[0] = 0.55f;
		state->precipitation[1] = 0.15f;
		state->precipitation[2] = 0.10f;
		state->precipitation[3] = 0.10f;
		state->precipitation[4] = 0.10f;
		state->surfaceWetness = 1.0f;
		state->surfaceFrost = 0.2f;
		state->snowAccumulation = 0.15f;
		state->visibility = 2048.0f;
		state->mediaDensity = 0.0008f;
		state->mediaHeightFalloff = 0.001f;
		state->cloudCover = 0.9f;
		state->cloudShadow = 0.8f;
		state->lightning = 0.75f;
		out->familyMask = ( 1u << 5 ) - 1u;
		out->localMedia = qtrue;
		break;
	default:
		return qfalse;
	}
	return qtrue;
}

static inline void Ral_AtmosphereConformanceBreathEmitter(
		uint32_t profile, atmosphereEmitter_t *out )
{
	memset( out, 0, sizeof( *out ) );
	out->schemaVersion = WIRED_ATMOSPHERE_SCHEMA_VERSION;
	out->kind = ATMOSPHERE_EMITTER_BREATH;
	out->id = 216u;
	out->seed = 216u;
	out->profile = profile;
	out->intensity = 1.0f;
	out->direction[0] = 1.0f;
	out->radius = 8.0f;
	out->temperatureC = 37.0f;
	out->humidity = 1.0f;
}

static inline void Ral_AtmosphereConformanceLocalMedia(
		atmosphereMediaVolume_t *out )
{
	memset( out, 0, sizeof( *out ) );
	out->schemaVersion = WIRED_ATMOSPHERE_MEDIA_VOLUME_SCHEMA_VERSION;
	out->shape = ATMOSPHERE_MEDIA_VOLUME_SPHERE;
	out->id = 216u;
	out->priority = 1u;
	out->seed = 216u;
	out->radius = 320.0f;
	out->extinction = 0.002f;
	out->albedo[0] = 0.6f;
	out->albedo[1] = 0.7f;
	out->albedo[2] = 0.8f;
}

#ifdef __cplusplus
}
#endif

#endif
