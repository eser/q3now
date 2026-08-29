// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "render_effect_runtime.h"

#include <math.h>
#include <string.h>

static uint32_t WiredFx_ActionBit( uint32_t index ) {
	return 1u << index;
}

static float WiredFx_Delay( const wiredFxEvent_t *event, const wiredFxAction_t *action ) {
	uint32_t value = event->seed ^ action->actionId * 0x9E3779B9u;
	float fraction;
	value ^= value >> 16;
	value *= 0x7FEB352Du;
	value ^= value >> 15;
	fraction = (float)( value & 0x00FFFFFFu ) / 16777215.0f;
	return action->delay[0] + ( action->delay[1] - action->delay[0] ) * fraction;
}

static qboolean WiredFx_ActionConditionsPass( const wiredFxAction_t *action,
	const wiredFxEvent_t *event ) {
	if ( action->startConditionMask != 0u &&
		 ( event->conditionMask & action->startConditionMask ) != action->startConditionMask )
		return qfalse;
	if ( action->stopConditionMask != 0u &&
		 ( event->conditionMask & action->stopConditionMask ) != 0u )
		return qfalse;
	if ( action->extraConditionMask != 0u &&
		 ( event->extraConditionMask & action->extraConditionMask ) == 0u )
		return qfalse;
	return qtrue;
}

static qboolean WiredFx_ActionInLod( const wiredFxProfile_t *profile,
	const wiredFxAction_t *action, const wiredFxEvent_t *event, const float viewOrigin[3] ) {
	float dx = event->origin[0] - viewOrigin[0];
	float dy = event->origin[1] - viewOrigin[1];
	float dz = event->origin[2] - viewOrigin[2];
	float distance = sqrtf( dx * dx + dy * dy + dz * dz );
	float nearDistance = action->lodNear > profile->lodNear ? action->lodNear : profile->lodNear;
	float farDistance = action->lodFar < profile->lodFar ? action->lodFar : profile->lodFar;
	return distance >= nearDistance && distance <= farDistance;
}

static void WiredFx_TriggerDependents( wiredFxEventInstance_t *instance,
	const wiredFxProfile_t *profile, uint32_t source, float timeSeconds ) {
	uint32_t i;
	const wiredFxAction_t *action = &profile->actions[source];
	if ( action->fireAction != WIRED_FX_NO_ACTION ) {
		instance->triggeredMask |= WiredFx_ActionBit( action->fireAction );
		instance->triggerTime[action->fireAction] = timeSeconds;
	}
	for ( i = source + 1u; i < profile->actionCount; ++i ) {
		if ( profile->actions[i].parentAction == source ) {
			instance->triggeredMask |= WiredFx_ActionBit( i );
			instance->triggerTime[i] = timeSeconds;
		}
	}
}

static qboolean WiredFx_EventIdActive( const wiredFxRuntime_t *runtime, uint32_t eventId ) {
	uint32_t i;
	for ( i = 0u; i < WIRED_FX_MAX_ACTIVE_EVENTS; ++i )
		if ( runtime->events[i].active && runtime->events[i].event.eventId == eventId ) return qtrue;
	return qfalse;
}

void WiredFx_InitRuntime( wiredFxRuntime_t *runtime ) {
	if ( runtime ) memset( runtime, 0, sizeof( *runtime ) );
}

qboolean WiredFx_SubmitEvent( wiredFxRuntime_t *runtime, const wiredFxRegistry_t *registry,
	const wiredFxEvent_t *event ) {
	const wiredFxProfile_t *profile;
	uint32_t slot, i, activeForProfile = 0u;
	uint32_t targetMask = 0u;
	if ( !runtime || WiredFx_ValidateEvent( registry, event ) != WIRED_FX_VALID ) {
		if ( runtime ) runtime->droppedEvents++;
		return qfalse;
	}
	if ( WiredFx_EventIdActive( runtime, event->eventId ) ) {
		runtime->duplicateEvents++;
		return qfalse;
	}
	if ( runtime->activeCount >= WIRED_FX_MAX_ACTIVE_EVENTS ) {
		runtime->droppedEvents++;
		return qfalse;
	}
	profile = &registry->profiles[event->profile - 1u];
	for ( i = 0u; i < WIRED_FX_MAX_ACTIVE_EVENTS; ++i )
		if ( runtime->events[i].active && runtime->events[i].event.profile == event->profile ) activeForProfile++;
	if ( activeForProfile >= profile->maxInstances ) {
		runtime->droppedEvents++;
		return qfalse;
	}
	for ( i = 0u; i < profile->actionCount; ++i )
		if ( profile->actions[i].fireAction != WIRED_FX_NO_ACTION )
			targetMask |= WiredFx_ActionBit( profile->actions[i].fireAction );
	for ( slot = 0u; slot < WIRED_FX_MAX_ACTIVE_EVENTS; ++slot )
		if ( !runtime->events[slot].active ) break;
	memset( &runtime->events[slot], 0, sizeof( runtime->events[slot] ) );
	runtime->events[slot].event = *event;
	runtime->events[slot].active = qtrue;
	for ( i = 0u; i < WIRED_FX_MAX_ACTIONS; ++i )
		runtime->events[slot].lastCycle[i] = UINT32_MAX;
	for ( i = 0u; i < profile->actionCount; ++i ) {
		if ( profile->actions[i].parentAction == WIRED_FX_NO_ACTION &&
			 ( targetMask & WiredFx_ActionBit( i ) ) == 0u ) {
			runtime->events[slot].triggeredMask |= WiredFx_ActionBit( i );
			runtime->events[slot].triggerTime[i] = event->startTimeSeconds;
		}
	}
	runtime->activeCount++;
	runtime->admittedEvents++;
	return qtrue;
}

uint32_t WiredFx_Service( wiredFxRuntime_t *runtime, const wiredFxRegistry_t *registry,
	float nowSeconds, const float viewOrigin[3], wiredFxDispatchSink_t sink, void *userData ) {
	uint32_t slot;
	uint32_t dispatched = 0u;
	if ( !runtime || !registry || !viewOrigin || !sink || !isfinite( nowSeconds ) ) return 0u;

	for ( slot = 0u; slot < WIRED_FX_MAX_ACTIVE_EVENTS; ++slot ) {
		wiredFxEventInstance_t *instance = &runtime->events[slot];
		const wiredFxProfile_t *profile;
		uint32_t i;
		uint32_t dispatchBudget;
		uint32_t completeMask;
		if ( !instance->active || nowSeconds < instance->event.startTimeSeconds ) continue;
		profile = &registry->profiles[instance->event.profile - 1u];
		if ( profile->duration > 0.0f &&
			 nowSeconds > instance->event.startTimeSeconds + profile->duration ) {
			instance->active = qfalse;
			runtime->activeCount--;
			runtime->expiredEvents++;
			continue;
		}
		dispatchBudget = profile->maxActiveActions;
		for ( i = 0u; i < profile->actionCount; ++i ) {
			const wiredFxAction_t *action = &profile->actions[i];
			float scheduledTime;
			uint32_t cycle = 0u;
			qboolean repeating;
			wiredFxDispatchResult_t result;
			if ( ( instance->triggeredMask & WiredFx_ActionBit( i ) ) == 0u ) continue;
			scheduledTime = instance->triggerTime[i] + WiredFx_Delay( &instance->event, action );
			if ( nowSeconds < scheduledTime ) continue;
			repeating = ( action->flags & ( WIRED_FX_ACTION_LOOP | WIRED_FX_ACTION_RESTART ) ) != 0u;
			if ( repeating ) {
				if ( action->duration <= 0.0f ) {
					instance->doneMask |= WiredFx_ActionBit( i );
					continue;
				}
				cycle = (uint32_t)floorf( ( nowSeconds - scheduledTime ) / action->duration );
				if ( instance->lastCycle[i] == cycle ) continue;
			} else if ( ( instance->doneMask & WiredFx_ActionBit( i ) ) != 0u ) {
				continue;
			}
			if ( !WiredFx_ActionConditionsPass( action, &instance->event ) ) {
				runtime->conditionCulls++;
				instance->doneMask |= WiredFx_ActionBit( i );
				WiredFx_TriggerDependents( instance, profile, i, scheduledTime );
				continue;
			}
			if ( !WiredFx_ActionInLod( profile, action, &instance->event, viewOrigin ) ) {
				runtime->lodCulls++;
				instance->doneMask |= WiredFx_ActionBit( i );
				WiredFx_TriggerDependents( instance, profile, i, scheduledTime );
				continue;
			}
			if ( dispatchBudget == 0u ) {
				runtime->droppedActions++;
				instance->lastCycle[i] = cycle;
				if ( !repeating ) {
					instance->doneMask |= WiredFx_ActionBit( i );
					WiredFx_TriggerDependents( instance, profile, i, scheduledTime );
				}
				continue;
			}
			result = sink( profile, action, &instance->event, i, cycle,
				scheduledTime + (float)cycle * action->duration, userData );
			dispatchBudget--;
			dispatched++;
			instance->lastCycle[i] = cycle;
			if ( !repeating ) instance->doneMask |= WiredFx_ActionBit( i );
			if ( result == WIRED_FX_DISPATCH_EXECUTED ) runtime->dispatchedActions++;
			else if ( result == WIRED_FX_DISPATCH_UNSUPPORTED ) runtime->unsupportedActions++;
			else runtime->droppedActions++;
			if ( action->type == WIRED_FX_ACTION_FADE_PARENT &&
				 action->parentAction != WIRED_FX_NO_ACTION )
				instance->doneMask |= WiredFx_ActionBit( action->parentAction );
			WiredFx_TriggerDependents( instance, profile, i,
				scheduledTime + (float)cycle * action->duration );
		}

		completeMask = profile->actionCount == 32u ? UINT32_MAX : ( 1u << profile->actionCount ) - 1u;
		if ( ( instance->doneMask & completeMask ) == completeMask ) {
			instance->active = qfalse;
			runtime->activeCount--;
			runtime->expiredEvents++;
		}
	}
	return dispatched;
}
