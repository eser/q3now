// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "render_effect_runtime.h"

#include <stdio.h>
#include <string.h>

#define CHECK( x ) do { if ( !( x ) ) { fprintf( stderr, "CHECK %s:%d: %s\n", __FILE__, __LINE__, #x ); return 1; } } while ( 0 )

typedef struct { uint32_t count; uint32_t types[8]; uint32_t cycles[8]; } capture_t;

static wiredFxDispatchResult_t Capture( const wiredFxProfile_t *profile,
	const wiredFxAction_t *action, const wiredFxEvent_t *event, uint32_t actionIndex,
	uint32_t cycle, float scheduledTimeSeconds, void *userData ) {
	capture_t *capture = userData;
	(void)profile; (void)event; (void)actionIndex; (void)scheduledTimeSeconds;
	if ( capture->count < 8u ) {
		capture->types[capture->count] = action->type;
		capture->cycles[capture->count] = cycle;
	}
	capture->count++;
	return action->type == WIRED_FX_ACTION_GODRAY ? WIRED_FX_DISPATCH_UNSUPPORTED : WIRED_FX_DISPATCH_EXECUTED;
}

static wiredFxProfile_t Profile( void ) {
	wiredFxProfile_t p;
	wiredFxAction_t *a;
	memset( &p, 0, sizeof( p ) );
	p.schemaVersion = WIRED_FX_PROFILE_SCHEMA_VERSION;
	p.actionCount = 4u; p.maxActiveActions = 4u; p.maxInstances = 64u;
	p.duration = 3.0f; p.lodFar = 1000.0f; p.boundsRadius = 64.0f;

	a = &p.actions[0]; a->type = WIRED_FX_ACTION_PARTICLE; a->actionId = 1u;
	a->parentAction = WIRED_FX_NO_ACTION; a->fireAction = 1u;
	a->originType = WIRED_FX_ORIGIN_START; a->rotationType = WIRED_FX_ROTATION_START_AXIS;
	a->duration = 0.5f; a->lodFar = 1000.0f; a->boundsRadius = 32.0f; a->maxInstances = 32u;
	a->payload.particle.particleClass = 1u; a->payload.particle.maxParticles = 16u;

	a = &p.actions[1]; a->type = WIRED_FX_ACTION_SOUND; a->actionId = 2u;
	a->parentAction = WIRED_FX_NO_ACTION; a->fireAction = WIRED_FX_NO_ACTION;
	a->originType = WIRED_FX_ORIGIN_START; a->rotationType = WIRED_FX_ROTATION_START_AXIS;
	a->delay[0] = a->delay[1] = 0.25f; a->duration = 0.1f; a->lodFar = 1000.0f;
	a->boundsRadius = 32.0f; a->maxInstances = 1u; a->payload.sound.sound = 2u;

	a = &p.actions[2]; a->type = WIRED_FX_ACTION_LIGHT; a->actionId = 3u;
	a->parentAction = WIRED_FX_NO_ACTION; a->fireAction = WIRED_FX_NO_ACTION;
	a->originType = WIRED_FX_ORIGIN_START; a->rotationType = WIRED_FX_ROTATION_START_AXIS;
	a->flags = WIRED_FX_ACTION_LOOP; a->duration = 0.5f; a->lodFar = 1000.0f;
	a->boundsRadius = 32.0f; a->maxInstances = 1u; a->extraConditionMask = 4u;
	a->payload.light.material = 3u; a->payload.light.radius[0] = 1.0f;
	a->payload.light.radius[1] = 1.0f; a->payload.light.radius[2] = 1.0f;
	a->payload.light.intensity = 1.0f;

	a = &p.actions[3]; a->type = WIRED_FX_ACTION_GODRAY; a->actionId = 4u;
	a->parentAction = 1u; a->fireAction = WIRED_FX_NO_ACTION;
	a->originType = WIRED_FX_ORIGIN_START; a->rotationType = WIRED_FX_ROTATION_START_AXIS;
	a->duration = 0.1f; a->lodFar = 1000.0f; a->boundsRadius = 32.0f; a->maxInstances = 1u;
	a->payload.godray.material = 4u; a->payload.godray.size = 8u;
	a->payload.godray.sourceSize = 4u; a->payload.godray.colorScale = 1.0f;
	return p;
}

int main( void ) {
	static wiredFxRegistry_t registry;
	static wiredFxRuntime_t runtime;
	wiredFxProfile_t profile = Profile();
	wiredFxEvent_t event;
	capture_t capture;
	float viewOrigin[3] = { 0.0f, 0.0f, 0.0f };

	WiredFx_InitRegistry( &registry );
	WiredFx_InitRuntime( &runtime );
	CHECK( WiredFx_RegisterProfile( &registry, 1u, &profile ) );
	memset( &event, 0, sizeof( event ) );
	event.schemaVersion = WIRED_FX_EVENT_SCHEMA_VERSION; event.profile = 1u;
	event.eventId = 10u; event.seed = 5u; event.startTimeSeconds = 1.0f;
	event.axis[0] = event.axis[4] = event.axis[8] = 1.0f;
	event.color[0] = event.color[1] = event.color[2] = event.color[3] = 1.0f;
	event.intensity = event.sizeScale = 1.0f;
	CHECK( WiredFx_SubmitEvent( &runtime, &registry, &event ) );
	CHECK( !WiredFx_SubmitEvent( &runtime, &registry, &event ) );
	memset( &capture, 0, sizeof( capture ) );
	CHECK( WiredFx_Service( &runtime, &registry, 1.0f, viewOrigin, Capture, &capture ) == 1u );
	CHECK( capture.types[0] == WIRED_FX_ACTION_PARTICLE );
	CHECK( runtime.conditionCulls == 1u );
	CHECK( WiredFx_Service( &runtime, &registry, 1.20f, viewOrigin, Capture, &capture ) == 0u );
	CHECK( WiredFx_Service( &runtime, &registry, 1.25f, viewOrigin, Capture, &capture ) == 2u );
	CHECK( capture.types[1] == WIRED_FX_ACTION_SOUND );
	CHECK( capture.types[2] == WIRED_FX_ACTION_GODRAY );
	CHECK( runtime.unsupportedActions == 1u );
	CHECK( runtime.activeCount == 0u );

	/* A looping action is admitted when its condition exists and dispatches once
	 * per cycle until the profile duration cap expires. */
	event.eventId = 11u; event.extraConditionMask = 4u;
	CHECK( WiredFx_SubmitEvent( &runtime, &registry, &event ) );
	memset( &capture, 0, sizeof( capture ) );
	CHECK( WiredFx_Service( &runtime, &registry, 1.0f, viewOrigin, Capture, &capture ) == 2u );
	CHECK( WiredFx_Service( &runtime, &registry, 1.51f, viewOrigin, Capture, &capture ) == 3u );
	CHECK( capture.types[3] == WIRED_FX_ACTION_LIGHT );
	CHECK( capture.cycles[3] == 1u );
	CHECK( WiredFx_Service( &runtime, &registry, 4.01f, viewOrigin, Capture, &capture ) == 0u );
	CHECK( runtime.activeCount == 0u );

	/* The authored per-profile instance cap is an admission boundary, not a
	 * hint: a distinct event is rejected while the first instance is live. */
	WiredFx_InitRuntime( &runtime );
	profile.maxInstances = 1u;
	for ( uint32_t i = 0u; i < profile.actionCount; ++i ) profile.actions[i].maxInstances = 1u;
	profile.actions[0].payload.particle.maxParticles = 1u;
	WiredFx_InitRegistry( &registry );
	CHECK( WiredFx_RegisterProfile( &registry, 1u, &profile ) );
	event.eventId = 20u;
	event.extraConditionMask = 4u;
	CHECK( WiredFx_SubmitEvent( &runtime, &registry, &event ) );
	event.eventId = 21u;
	CHECK( !WiredFx_SubmitEvent( &runtime, &registry, &event ) );
	CHECK( runtime.activeCount == 1u );
	CHECK( runtime.droppedEvents == 1u );

	/* A frame-local action budget is fail-closed: an over-budget one-shot is
	 * counted once, completed, and never retried every frame. */
	{
		wiredFxProfile_t bounded;
		memset( &bounded, 0, sizeof( bounded ) );
		bounded.schemaVersion = WIRED_FX_PROFILE_SCHEMA_VERSION;
		bounded.actionCount = 2u; bounded.maxActiveActions = 1u;
		bounded.maxInstances = 1u; bounded.lodFar = 1000.0f;
		for ( uint32_t i = 0u; i < bounded.actionCount; ++i ) {
			wiredFxAction_t *action = &bounded.actions[i];
			action->type = WIRED_FX_ACTION_SOUND; action->actionId = i + 1u;
			action->parentAction = action->fireAction = WIRED_FX_NO_ACTION;
			action->originType = WIRED_FX_ORIGIN_START;
			action->rotationType = WIRED_FX_ROTATION_START_AXIS;
			action->lodFar = 1000.0f; action->maxInstances = 1u;
			action->payload.sound.sound = i + 1u;
		}
		WiredFx_InitRegistry( &registry ); WiredFx_InitRuntime( &runtime );
		CHECK( WiredFx_RegisterProfile( &registry, 1u, &bounded ) );
		event.profile = 1u; event.eventId = 30u; event.extraConditionMask = 0u;
		CHECK( WiredFx_SubmitEvent( &runtime, &registry, &event ) );
		memset( &capture, 0, sizeof( capture ) );
		CHECK( WiredFx_Service( &runtime, &registry, 1.0f, viewOrigin, Capture, &capture ) == 1u );
		CHECK( runtime.droppedActions == 1u );
		CHECK( runtime.activeCount == 0u );
		CHECK( WiredFx_Service( &runtime, &registry, 1.1f, viewOrigin, Capture, &capture ) == 0u );
		CHECK( runtime.droppedActions == 1u );
	}

	puts( "wired_fx_runtime_test: PASS" );
	return 0;
}
