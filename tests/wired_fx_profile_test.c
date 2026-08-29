// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "render_effect_profile.h"

#include <stdio.h>
#include <string.h>

#define CHECK( condition )                                                                    \
	do {                                                                                        \
		if ( !( condition ) ) {                                                                  \
			fprintf( stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #condition ); \
			return 1;                                                                             \
		}                                                                                       \
	} while ( 0 )

static void InitAction( wiredFxAction_t *action, uint32_t type, uint32_t id ) {
	memset( action, 0, sizeof( *action ) );
	action->type = type;
	action->actionId = id;
	action->parentAction = WIRED_FX_NO_ACTION;
	action->fireAction = WIRED_FX_NO_ACTION;
	action->originType = WIRED_FX_ORIGIN_START;
	action->rotationType = WIRED_FX_ROTATION_START_AXIS;
	action->duration = 0.5f;
	action->lodFar = 1024.0f;
	action->boundsRadius = 64.0f;
	action->color[0] = action->color[1] = action->color[2] = action->color[3] = 1.0f;
	action->maxInstances = 64u;
}

static wiredFxProfile_t CompleteProfile( void ) {
	wiredFxProfile_t profile;
	uint32_t i;
	memset( &profile, 0, sizeof( profile ) );
	profile.schemaVersion = WIRED_FX_PROFILE_SCHEMA_VERSION;
	profile.actionCount = WIRED_FX_ACTION_COUNT;
	profile.maxActiveActions = WIRED_FX_ACTION_COUNT;
	profile.maxInstances = 1024u;
	profile.seed = 0x225u;
	profile.lodFar = 2048.0f;
	profile.boundsRadius = 256.0f;

	for ( i = 0u; i < profile.actionCount; ++i ) InitAction( &profile.actions[i], i, i + 1u );

	profile.actions[WIRED_FX_ACTION_LIGHT].payload.light.material = 1u;
	profile.actions[WIRED_FX_ACTION_LIGHT].payload.light.radius[0] = 64.0f;
	profile.actions[WIRED_FX_ACTION_LIGHT].payload.light.radius[1] = 64.0f;
	profile.actions[WIRED_FX_ACTION_LIGHT].payload.light.radius[2] = 64.0f;
	profile.actions[WIRED_FX_ACTION_LIGHT].payload.light.intensity = 2.0f;

	profile.actions[WIRED_FX_ACTION_PARTICLE].payload.particle.particleClass = 1u;
	profile.actions[WIRED_FX_ACTION_PARTICLE].payload.particle.maxParticles = 32u;
	profile.actions[WIRED_FX_ACTION_PARTICLE].payload.particle.velocityScale = 1.0f;
	profile.actions[WIRED_FX_ACTION_PARTICLE].payload.particle.spawnRate = 8.0f;

	profile.actions[WIRED_FX_ACTION_DECAL].payload.decal.material = 2u;
	profile.actions[WIRED_FX_ACTION_DECAL].payload.decal.depth = 4.0f;
	profile.actions[WIRED_FX_ACTION_DECAL].payload.decal.size = 32.0f;
	profile.actions[WIRED_FX_ACTION_DECAL2].payload.decal.material = 3u;
	profile.actions[WIRED_FX_ACTION_DECAL2].payload.decal.depth = 8.0f;
	profile.actions[WIRED_FX_ACTION_DECAL2].payload.decal.size = 48.0f;
	profile.actions[WIRED_FX_ACTION_MODEL].payload.model.model = 4u;
	profile.actions[WIRED_FX_ACTION_SOUND].payload.sound.sound = 5u;
	profile.actions[WIRED_FX_ACTION_SCREEN_SHAKE].payload.screenShake.magnitude = 0.5f;
	profile.actions[WIRED_FX_ACTION_CONTROLLER_SHAKE].payload.controllerShake.highDuration = 0.2f;
	profile.actions[WIRED_FX_ACTION_CONTROLLER_SHAKE].payload.controllerShake.lowDuration = 0.4f;
	profile.actions[WIRED_FX_ACTION_WIND].payload.wind.multiplier = 1.0f;
	profile.actions[WIRED_FX_ACTION_WIND].payload.wind.strength[1] = 10.0f;
	profile.actions[WIRED_FX_ACTION_RENDER_PARM].payload.renderParm.handle = 6u;
	profile.actions[WIRED_FX_ACTION_ENV_OVERRIDE].payload.environment.environment = 7u;
	profile.actions[WIRED_FX_ACTION_ENV_OVERRIDE].payload.environment.renderParmCount = 1u;
	profile.actions[WIRED_FX_ACTION_ENV_OVERRIDE].payload.environment.renderParms[0].handle = 8u;
	profile.actions[WIRED_FX_ACTION_ENV_CHANGE].payload.environment.environment = 9u;
	profile.actions[WIRED_FX_ACTION_FLARE].payload.flare.flare = 10u;
	profile.actions[WIRED_FX_ACTION_RADIAL_BLUR].payload.radialBlur.maxScale = 0.3f;
	profile.actions[WIRED_FX_ACTION_RIBBON].payload.ribbon.ribbon = 11u;
	profile.actions[WIRED_FX_ACTION_GODRAY].payload.godray.material = 12u;
	profile.actions[WIRED_FX_ACTION_GODRAY].payload.godray.color[0] = 1.0f;
	profile.actions[WIRED_FX_ACTION_GODRAY].payload.godray.color[1] = 0.8f;
	profile.actions[WIRED_FX_ACTION_GODRAY].payload.godray.color[2] = 0.5f;
	profile.actions[WIRED_FX_ACTION_GODRAY].payload.godray.color[3] = 1.0f;
	profile.actions[WIRED_FX_ACTION_GODRAY].payload.godray.colorScale = 0.015f;
	profile.actions[WIRED_FX_ACTION_GODRAY].payload.godray.size = 512u;
	profile.actions[WIRED_FX_ACTION_GODRAY].payload.godray.sourceSize = 128u;
	return profile;
}

int main( void ) {
	static wiredFxRegistry_t registry;
	wiredFxProfile_t profile = CompleteProfile();
	wiredFxProfile_t stored;
	wiredFxProfile_t child;
	wiredFxEvent_t event;
	uint32_t i;
	static const char *const expectedNames[WIRED_FX_ACTION_COUNT] = {
		"light", "particle", "decal", "decal2", "model", "sound", "screenShake",
		"controllerShake", "wind", "renderParm", "envOverride", "envChange", "flare",
		"radialBlur", "ribbon", "fadeParent", "godray"
	};

	CHECK( WIRED_FX_ACTION_COUNT == 17u );
	for ( i = 0u; i < WIRED_FX_ACTION_COUNT; ++i )
		CHECK( strcmp( WiredFx_ActionTypeName( i ), expectedNames[i] ) == 0 );
	CHECK( strcmp( WiredFx_ActionTypeName( WIRED_FX_ACTION_COUNT ), "invalid" ) == 0 );
	CHECK( WiredFx_ValidateProfile( &profile ) == WIRED_FX_VALID );
	profile.actions[0].flags |= WIRED_FX_ACTION_LOOP;
	profile.actions[0].duration = 0.0f;
	CHECK( WiredFx_ValidateProfile( &profile ) == WIRED_FX_INVALID_ACTION_RANGE );
	profile = CompleteProfile();

	WiredFx_InitRegistry( &registry );
	CHECK( registry.generation == 1u && registry.count == 0u );
	CHECK( WiredFx_RegisterProfile( &registry, 1u, &profile ) );
	CHECK( registry.generation == 2u && registry.count == 1u );
	CHECK( !WiredFx_RegisterProfile( &registry, 1u, &profile ) );
	CHECK( !WiredFx_RegisterProfile( &registry, 0u, &profile ) );
	profile.seed = 0u;
	CHECK( WiredFx_GetProfile( &registry, 1u, &stored ) );
	CHECK( stored.seed == 0x225u );

	/* Parent actions are materialized once at registration. The child only
	 * authors the slot selected by actionOverrideMask. */
	memset( &child, 0, sizeof( child ) );
	child.schemaVersion = WIRED_FX_PROFILE_SCHEMA_VERSION;
	child.actionCount = WIRED_FX_ACTION_COUNT;
	child.maxActiveActions = WIRED_FX_ACTION_COUNT;
	child.maxInstances = 1024u;
	child.parentProfile = 1u;
	child.actionOverrideMask = 1u << WIRED_FX_ACTION_LIGHT;
	child.seed = 0x226u;
	child.lodFar = 2048.0f;
	child.boundsRadius = 256.0f;
	InitAction( &child.actions[WIRED_FX_ACTION_LIGHT], WIRED_FX_ACTION_LIGHT, 100u );
	child.actions[WIRED_FX_ACTION_LIGHT].payload.light.material = 20u;
	child.actions[WIRED_FX_ACTION_LIGHT].payload.light.radius[0] = 96.0f;
	child.actions[WIRED_FX_ACTION_LIGHT].payload.light.radius[1] = 96.0f;
	child.actions[WIRED_FX_ACTION_LIGHT].payload.light.radius[2] = 96.0f;
	child.actions[WIRED_FX_ACTION_LIGHT].payload.light.intensity = 4.0f;
	CHECK( WiredFx_ValidateProfile( &child ) == WIRED_FX_VALID );
	CHECK( WiredFx_RegisterProfile( &registry, 2u, &child ) );
	CHECK( WiredFx_GetProfile( &registry, 2u, &stored ) );
	CHECK( stored.actions[WIRED_FX_ACTION_LIGHT].actionId == 100u );
	CHECK( stored.actions[WIRED_FX_ACTION_PARTICLE].actionId == WIRED_FX_ACTION_PARTICLE + 1u );
	CHECK( stored.parentProfile == 1u );

	memset( &event, 0, sizeof( event ) );
	event.schemaVersion = WIRED_FX_EVENT_SCHEMA_VERSION;
	event.profile = 2u;
	event.eventId = 77u;
	event.seed = 123u;
	event.startTimeSeconds = 10.0f;
	event.axis[0] = event.axis[4] = event.axis[8] = 1.0f;
	event.color[0] = event.color[1] = event.color[2] = event.color[3] = 1.0f;
	event.intensity = 1.0f;
	event.sizeScale = 1.0f;
	CHECK( WiredFx_ValidateEvent( &registry, &event ) == WIRED_FX_VALID );
	event.profile = 3u;
	CHECK( WiredFx_ValidateEvent( &registry, &event ) == WIRED_FX_INVALID_EVENT );
	event.profile = 2u;
	event.flags = 0x80000000u;
	CHECK( WiredFx_ValidateEvent( &registry, &event ) == WIRED_FX_INVALID_EVENT );

	profile = CompleteProfile();
	profile.schemaVersion++;
	CHECK( WiredFx_ValidateProfile( &profile ) == WIRED_FX_INVALID_SCHEMA );
	profile = CompleteProfile();
	profile.actions[0].type = WIRED_FX_ACTION_COUNT;
	CHECK( WiredFx_ValidateProfile( &profile ) == WIRED_FX_INVALID_ACTION_TYPE );
	profile = CompleteProfile();
	profile.actions[1].actionId = profile.actions[0].actionId;
	CHECK( WiredFx_ValidateProfile( &profile ) == WIRED_FX_INVALID_ACTION_ID );
	profile = CompleteProfile();
	profile.actions[0].parentAction = 0u;
	CHECK( WiredFx_ValidateProfile( &profile ) == WIRED_FX_INVALID_ACTION_GRAPH );
	profile = CompleteProfile();
	profile.actions[0].fireAction = 1u;
	profile.actions[1].fireAction = 0u;
	CHECK( WiredFx_ValidateProfile( &profile ) == WIRED_FX_INVALID_ACTION_GRAPH );
	profile = CompleteProfile();
	profile.actions[WIRED_FX_ACTION_PARTICLE].payload.particle.particleClass = 0u;
	CHECK( WiredFx_ValidateProfile( &profile ) == WIRED_FX_INVALID_ACTION_RESOURCE );
	profile.actions[WIRED_FX_ACTION_PARTICLE].flags |= WIRED_FX_ACTION_OPTIONAL_RESOURCE;
	CHECK( WiredFx_ValidateProfile( &profile ) == WIRED_FX_VALID );
	profile = CompleteProfile();
	profile.actions[WIRED_FX_ACTION_PARTICLE].payload.particle.maxParticles = 65u;
	CHECK( WiredFx_ValidateProfile( &profile ) == WIRED_FX_INVALID_ACTION_RANGE );
	profile = CompleteProfile();
	profile.lodNear = 10.0f;
	profile.lodFar = 5.0f;
	CHECK( WiredFx_ValidateProfile( &profile ) == WIRED_FX_INVALID_PROFILE_RANGE );
	profile = CompleteProfile();
	profile.actionOverrideMask = 1u;
	CHECK( WiredFx_ValidateProfile( &profile ) == WIRED_FX_INVALID_INHERITANCE );

	puts( "wired_fx_profile_test: PASS" );
	return 0;
}
