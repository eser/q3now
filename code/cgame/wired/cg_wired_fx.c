// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026-present Wired Engine contributors

/*
 * Cgame-side semantic WiredFX event construction. This file owns occurrence
 * identity and the common pointer-free transform envelope; weapon code still
 * owns gameplay classification, traces and collision results.
 */

#include "../cg_local.h"

static uint32_t s_wiredFxEventId = 1u;

void CG_WiredFx_InitEvent( wiredFxEvent_t *event, uint32_t profile,
		const vec3_t origin, const vec3_t direction ) {
	vec3_t forward;

	if ( !event ) return;
	memset( event, 0, sizeof( *event ) );
	event->schemaVersion = WIRED_FX_EVENT_SCHEMA_VERSION;
	event->profile = profile;
	event->eventId = s_wiredFxEventId++;
	if ( s_wiredFxEventId == 0u ) s_wiredFxEventId = 1u;
	event->seed = event->eventId ^ (uint32_t)cg.time * 0x9E3779B9u;
	event->startTimeSeconds = (float)cg.time * 0.001f;
	VectorCopy( origin, event->origin );
	if ( !direction || VectorNormalize2( direction, forward ) == 0.0f )
		VectorSet( forward, 0.0f, 0.0f, 1.0f );
	VectorCopy( forward, &event->axis[6] );
	PerpendicularVector( &event->axis[0], forward );
	CrossProduct( forward, &event->axis[0], &event->axis[3] );
	Vector4Set( event->color, 1.0f, 1.0f, 1.0f, 1.0f );
	event->intensity = 1.0f;
	event->sizeScale = 1.0f;
	event->sourceEntityNum = -1;
}

void CG_WiredFx_EmitPath( uint32_t profile, const vec3_t start,
		const vec3_t end, float timeSpanSeconds, float pathSpacing,
		uint64_t conditionMask ) {
	wiredFxEvent_t event;
	vec3_t direction;

	VectorSubtract( end, start, direction );
	CG_WiredFx_InitEvent( &event, profile, start, direction );
	event.flags |= WIRED_FX_EVENT_HAS_END_ORIGIN;
	VectorCopy( end, event.endOrigin );
	event.timeSpanSeconds = Com_Clamp( 0.0f, 1.0f, timeSpanSeconds );
	event.pathSpacing = MAX( pathSpacing, 0.0f );
	event.conditionMask = conditionMask;
	trap_WiredFx_EmitEvent( &event );
}
