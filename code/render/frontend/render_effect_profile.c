// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "render_effect_profile.h"

#include <math.h>
#include <string.h>

#define WIRED_FX_KNOWN_ACTION_FLAGS ( WIRED_FX_ACTION_RESTART | WIRED_FX_ACTION_LOOP | \
	WIRED_FX_ACTION_NO_SHADOWS | WIRED_FX_ACTION_FILTER_HIGH_VIOLENCE | \
	WIRED_FX_ACTION_OPTIONAL_RESOURCE | WIRED_FX_ACTION_TRACK_VELOCITY | \
	WIRED_FX_ACTION_USE_GPU_LIFECYCLE )

#define WIRED_FX_KNOWN_EVENT_FLAGS \
	( WIRED_FX_EVENT_HAS_END_ORIGIN | WIRED_FX_EVENT_HAS_VELOCITY \
		| WIRED_FX_EVENT_UNDERWATER | WIRED_FX_EVENT_FREE_AIR \
		| WIRED_FX_EVENT_HAS_SOURCE_ENTITY | WIRED_FX_EVENT_HAS_SHAKE_OVERRIDE )

static const char *const wiredFxActionTypeNames[WIRED_FX_ACTION_COUNT] = {
	"light", "particle", "decal", "decal2", "model", "sound", "screenShake",
	"controllerShake", "wind", "renderParm", "envOverride", "envChange", "flare",
	"radialBlur", "ribbon", "fadeParent", "godray", "sprite", "beam"
};

const char *WiredFx_ActionTypeName( uint32_t type ) {
	return type < WIRED_FX_ACTION_COUNT ? wiredFxActionTypeNames[type] : "invalid";
}

static qboolean WiredFx_Finite( float value ) {
	return isfinite( value ) ? qtrue : qfalse;
}

static qboolean WiredFx_FiniteArray( const float *values, uint32_t count ) {
	uint32_t i;
	for ( i = 0; i < count; ++i ) {
		if ( !WiredFx_Finite( values[i] ) ) return qfalse;
	}
	return qtrue;
}

static qboolean WiredFx_ResourcePresent( const wiredFxAction_t *action, uint32_t handle ) {
	return handle != 0u || ( action->flags & WIRED_FX_ACTION_OPTIONAL_RESOURCE ) != 0u;
}

static wiredFxValidationError_t WiredFx_ValidatePayload( const wiredFxAction_t *action ) {
	uint32_t i;

	switch ( (wiredFxActionType_t)action->type ) {
	case WIRED_FX_ACTION_LIGHT:
		if ( !WiredFx_ResourcePresent( action, action->payload.light.material ) )
			return WIRED_FX_INVALID_ACTION_RESOURCE;
		if ( !WiredFx_FiniteArray( action->payload.light.radius, 3u ) ||
			 !WiredFx_Finite( action->payload.light.intensity ) ||
			 !WiredFx_Finite( action->payload.light.radiusJitter ) ||
			 !WiredFx_Finite( action->payload.light.lifetime ) ||
			 !WiredFx_Finite( action->payload.light.startTimeJitter ) ||
			 action->payload.light.radius[0] < 0.0f || action->payload.light.radius[1] < 0.0f ||
			 action->payload.light.radius[2] < 0.0f || action->payload.light.intensity < 0.0f ||
			 action->payload.light.radiusJitter < 0.0f ||
			 action->payload.light.lifetime < 0.0f ||
			 action->payload.light.startTimeJitter < 0.0f ||
			 action->payload.light.radiusJitterSteps == 1u ||
			 action->payload.light.startTimeJitterSteps == 1u )
			return WIRED_FX_INVALID_ACTION_RANGE;
		break;
	case WIRED_FX_ACTION_PARTICLE:
		if ( !WiredFx_ResourcePresent( action, action->payload.particle.particleClass ) )
			return WIRED_FX_INVALID_ACTION_RESOURCE;
		if ( action->payload.particle.maxParticles == 0u ||
			 action->payload.particle.maxParticles > action->maxInstances ||
			 !WiredFx_Finite( action->payload.particle.velocityScale ) ||
			 !WiredFx_Finite( action->payload.particle.minVelocity ) ||
			 !WiredFx_Finite( action->payload.particle.spawnRate ) ||
			 !WiredFx_Finite( action->payload.particle.trailSpacing ) ||
			 !WiredFx_Finite( action->payload.particle.screenExcludeAngle ) ||
			 action->payload.particle.spawnRate < 0.0f || action->payload.particle.trailSpacing < 0.0f ||
			 action->payload.particle.rateBoundaryAligned > 1u ||
			 action->payload.particle.pathSampling >= WIRED_FX_PATH_SAMPLING_COUNT )
			return WIRED_FX_INVALID_ACTION_RANGE;
		break;
	case WIRED_FX_ACTION_DECAL:
	case WIRED_FX_ACTION_DECAL2:
		if ( !WiredFx_ResourcePresent( action, action->payload.decal.material ) )
			return WIRED_FX_INVALID_ACTION_RESOURCE;
		if ( !WiredFx_Finite( action->payload.decal.angle ) ||
			 !WiredFx_Finite( action->payload.decal.depth ) ||
			 !WiredFx_Finite( action->payload.decal.size ) ||
			 action->payload.decal.depth < 0.0f || action->payload.decal.size <= 0.0f )
			return WIRED_FX_INVALID_ACTION_RANGE;
		break;
	case WIRED_FX_ACTION_MODEL:
		if ( !WiredFx_ResourcePresent( action, action->payload.model.model ) )
			return WIRED_FX_INVALID_ACTION_RESOURCE;
		break;
	case WIRED_FX_ACTION_SOUND:
		if ( !WiredFx_ResourcePresent( action, action->payload.sound.sound ) )
			return WIRED_FX_INVALID_ACTION_RESOURCE;
		if ( action->payload.sound.looping > 1u ||
			 action->payload.sound.sourceBound > 1u )
			return WIRED_FX_INVALID_ACTION_RANGE;
		break;
	case WIRED_FX_ACTION_SCREEN_SHAKE:
		if ( !WiredFx_Finite( action->payload.screenShake.magnitude ) ||
			 !WiredFx_Finite( action->payload.screenShake.controllerScale ) ||
			 !WiredFx_FiniteArray( action->payload.screenShake.maxAngles, 3u ) ||
			 !WiredFx_FiniteArray( action->payload.screenShake.maxOffset, 3u ) ||
			 !WiredFx_Finite( action->payload.screenShake.radius ) ||
			 !WiredFx_Finite( action->payload.screenShake.decayExponent ) ||
			 action->payload.screenShake.magnitude < 0.0f ||
			 action->payload.screenShake.radius < 0.0f ||
			 action->payload.screenShake.decayExponent <= 0.0f ||
			 action->payload.screenShake.mode > 1u )
			return WIRED_FX_INVALID_ACTION_RANGE;
		break;
	case WIRED_FX_ACTION_CONTROLLER_SHAKE:
		if ( !WiredFx_Finite( action->payload.controllerShake.highMagnitude ) ||
			 !WiredFx_Finite( action->payload.controllerShake.lowMagnitude ) ||
			 !WiredFx_Finite( action->payload.controllerShake.highDuration ) ||
			 !WiredFx_Finite( action->payload.controllerShake.lowDuration ) ||
			 action->payload.controllerShake.highDuration < 0.0f ||
			 action->payload.controllerShake.lowDuration < 0.0f )
			return WIRED_FX_INVALID_ACTION_RANGE;
		break;
	case WIRED_FX_ACTION_WIND:
		if ( !WiredFx_Finite( action->payload.wind.angle ) ||
			 !WiredFx_Finite( action->payload.wind.multiplier ) ||
			 !WiredFx_FiniteArray( action->payload.wind.strength, 2u ) ||
			 action->payload.wind.strength[1] < action->payload.wind.strength[0] )
			return WIRED_FX_INVALID_ACTION_RANGE;
		break;
	case WIRED_FX_ACTION_RENDER_PARM:
		if ( !WiredFx_ResourcePresent( action, action->payload.renderParm.handle ) )
			return WIRED_FX_INVALID_ACTION_RESOURCE;
		if ( !WiredFx_FiniteArray( action->payload.renderParm.value, 4u ) )
			return WIRED_FX_INVALID_ACTION_RANGE;
		break;
	case WIRED_FX_ACTION_ENV_OVERRIDE:
	case WIRED_FX_ACTION_ENV_CHANGE:
		if ( !WiredFx_ResourcePresent( action, action->payload.environment.environment ) )
			return WIRED_FX_INVALID_ACTION_RESOURCE;
		if ( action->payload.environment.renderParmCount > WIRED_FX_MAX_ENV_RENDER_PARMS )
			return WIRED_FX_INVALID_ACTION_RANGE;
		for ( i = 0; i < action->payload.environment.renderParmCount; ++i ) {
			if ( action->payload.environment.renderParms[i].handle == 0u ||
				 !WiredFx_FiniteArray( action->payload.environment.renderParms[i].value, 4u ) )
				return WIRED_FX_INVALID_ACTION_RESOURCE;
		}
		break;
	case WIRED_FX_ACTION_FLARE:
		if ( !WiredFx_ResourcePresent( action, action->payload.flare.flare ) )
			return WIRED_FX_INVALID_ACTION_RESOURCE;
		if ( !WiredFx_FiniteArray( action->payload.flare.position, 3u ) )
			return WIRED_FX_INVALID_ACTION_RANGE;
		break;
	case WIRED_FX_ACTION_RADIAL_BLUR:
		if ( !WiredFx_Finite( action->payload.radialBlur.maxScale ) ||
			 action->payload.radialBlur.maxScale < 0.0f )
			return WIRED_FX_INVALID_ACTION_RANGE;
		break;
	case WIRED_FX_ACTION_RIBBON:
	case WIRED_FX_ACTION_BEAM:
		if ( !WiredFx_ResourcePresent( action, action->payload.ribbon.ribbon ) )
			return WIRED_FX_INVALID_ACTION_RESOURCE;
		if ( !WiredFx_Finite( action->payload.ribbon.width ) ||
			 !WiredFx_Finite( action->payload.ribbon.endWidth ) ||
			 !WiredFx_FiniteArray( action->payload.ribbon.length, 2u ) ||
			 !WiredFx_FiniteArray( action->payload.ribbon.normalScale, 2u ) ||
			 !WiredFx_Finite( action->payload.ribbon.spread ) ||
			 !WiredFx_Finite( action->payload.ribbon.lifetime ) ||
			 !WiredFx_Finite( action->payload.ribbon.fadeOut ) ||
			 !WiredFx_FiniteArray( action->payload.ribbon.startColor, 4u ) ||
			 !WiredFx_FiniteArray( action->payload.ribbon.endColor, 4u ) ||
			 action->payload.ribbon.width <= 0.0f || action->payload.ribbon.endWidth < 0.0f ||
			 action->payload.ribbon.count == 0u || action->payload.ribbon.count > action->maxInstances ||
			 action->payload.ribbon.length[0] < 0.0f ||
			 action->payload.ribbon.length[1] < action->payload.ribbon.length[0] ||
			 action->payload.ribbon.normalScale[1] < action->payload.ribbon.normalScale[0] ||
			 action->payload.ribbon.spread < 0.0f || action->payload.ribbon.lifetime < 0.0f ||
			 action->payload.ribbon.fadeOut < 0.0f ||
			 action->payload.ribbon.fadeOut > action->payload.ribbon.lifetime )
			return WIRED_FX_INVALID_ACTION_RANGE;
		break;
	case WIRED_FX_ACTION_FADE_PARENT:
		break;
	case WIRED_FX_ACTION_GODRAY:
		if ( !WiredFx_ResourcePresent( action, action->payload.godray.material ) )
			return WIRED_FX_INVALID_ACTION_RESOURCE;
		if ( !WiredFx_FiniteArray( action->payload.godray.color, 4u ) ||
			 !WiredFx_Finite( action->payload.godray.colorScale ) ||
			 action->payload.godray.size == 0u || action->payload.godray.sourceSize == 0u )
			return WIRED_FX_INVALID_ACTION_RANGE;
		break;
	case WIRED_FX_ACTION_SPRITE:
		if ( !WiredFx_ResourcePresent( action, action->payload.sprite.material ) )
			return WIRED_FX_INVALID_ACTION_RESOURCE;
		if ( !WiredFx_FiniteArray( action->payload.sprite.radius, 2u ) ||
			 !WiredFx_FiniteArray( action->payload.sprite.velocity, 3u ) ||
			 !WiredFx_Finite( action->payload.sprite.lifetime ) ||
			 !WiredFx_Finite( action->payload.sprite.startTimeJitter ) ||
			 action->payload.sprite.radius[0] <= 0.0f ||
			 action->payload.sprite.radius[1] <= 0.0f ||
			 action->payload.sprite.lifetime < 0.0f ||
			 action->payload.sprite.startTimeJitter < 0.0f ||
			 action->payload.sprite.startTimeJitterSteps == 1u ||
			 action->payload.sprite.randomRotation > 1u )
			return WIRED_FX_INVALID_ACTION_RANGE;
		break;
	default:
		return WIRED_FX_INVALID_ACTION_TYPE;
	}

	return WIRED_FX_VALID;
}

static qboolean WiredFx_ActionGraphAcyclic( const wiredFxProfile_t *profile ) {
	qboolean reachable[WIRED_FX_MAX_ACTIONS][WIRED_FX_MAX_ACTIONS];
	uint32_t i, j, k;
	memset( reachable, 0, sizeof( reachable ) );
	for ( i = 0u; i < profile->actionCount; ++i ) {
		const wiredFxAction_t *action = &profile->actions[i];
		if ( action->parentAction != WIRED_FX_NO_ACTION ) reachable[action->parentAction][i] = qtrue;
		if ( action->fireAction != WIRED_FX_NO_ACTION ) reachable[i][action->fireAction] = qtrue;
	}
	for ( k = 0u; k < profile->actionCount; ++k )
		for ( i = 0u; i < profile->actionCount; ++i )
			for ( j = 0u; j < profile->actionCount; ++j )
				reachable[i][j] = reachable[i][j] || ( reachable[i][k] && reachable[k][j] );
	for ( i = 0u; i < profile->actionCount; ++i )
		if ( reachable[i][i] ) return qfalse;
	return qtrue;
}

wiredFxValidationError_t WiredFx_ValidateProfile( const wiredFxProfile_t *profile ) {
	uint32_t i;

	if ( !profile ) return WIRED_FX_INVALID_ARGUMENT;
	if ( profile->schemaVersion != WIRED_FX_PROFILE_SCHEMA_VERSION ) return WIRED_FX_INVALID_SCHEMA;
	if ( profile->actionCount == 0u || profile->actionCount > WIRED_FX_MAX_ACTIONS ||
		 profile->maxActiveActions == 0u || profile->maxActiveActions > profile->actionCount ||
		 profile->maxInstances == 0u )
		return WIRED_FX_INVALID_PROFILE_BUDGET;
	if ( !WiredFx_Finite( profile->duration ) || !WiredFx_Finite( profile->lodNear ) ||
		 !WiredFx_Finite( profile->lodFar ) || !WiredFx_Finite( profile->boundsRadius ) ||
		 profile->duration < 0.0f || profile->lodNear < 0.0f || profile->lodFar < profile->lodNear ||
		 profile->boundsRadius < 0.0f )
		return WIRED_FX_INVALID_PROFILE_RANGE;
	if ( profile->flags != 0u ||
		 ( profile->parentProfile == 0u && profile->actionOverrideMask != 0u ) ||
		 ( profile->actionCount < 32u &&
		   ( profile->actionOverrideMask >> profile->actionCount ) != 0u ) )
		return WIRED_FX_INVALID_INHERITANCE;

	for ( i = 0u; i < profile->actionCount; ++i ) {
		const wiredFxAction_t *action = &profile->actions[i];
		const qboolean inheritedSlot =
			profile->parentProfile != 0u && ( profile->actionOverrideMask & ( 1u << i ) ) == 0u;
		uint32_t j;
		wiredFxValidationError_t payloadResult;

		if ( inheritedSlot ) continue;

		if ( action->type >= WIRED_FX_ACTION_COUNT ) return WIRED_FX_INVALID_ACTION_TYPE;
		if ( action->actionId == 0u ) return WIRED_FX_INVALID_ACTION_ID;
		for ( j = 0u; j < i; ++j ) {
			const qboolean earlierInherited = profile->parentProfile != 0u &&
				( profile->actionOverrideMask & ( 1u << j ) ) == 0u;
			if ( !earlierInherited && profile->actions[j].actionId == action->actionId )
				return WIRED_FX_INVALID_ACTION_ID;
		}
		if ( ( action->flags & ~WIRED_FX_KNOWN_ACTION_FLAGS ) != 0u )
			return WIRED_FX_INVALID_ACTION_FLAGS;
		if ( ( action->flags & ( WIRED_FX_ACTION_LOOP | WIRED_FX_ACTION_RESTART ) ) != 0u &&
			 ( action->duration <= 0.0f || profile->duration <= 0.0f ) )
			return WIRED_FX_INVALID_ACTION_RANGE;
		if ( action->originType > WIRED_FX_ORIGIN_MIDPOINT ||
			 action->rotationType > WIRED_FX_ROTATION_EXTERNAL ||
			 ( action->parentAction != WIRED_FX_NO_ACTION && action->parentAction >= i ) ||
			 ( action->fireAction != WIRED_FX_NO_ACTION && action->fireAction >= profile->actionCount ) )
			return WIRED_FX_INVALID_ACTION_GRAPH;
		if ( !WiredFx_FiniteArray( action->delay, 2u ) || !WiredFx_Finite( action->duration ) ||
			 !WiredFx_Finite( action->fadeInTime ) || !WiredFx_Finite( action->fadeOutTime ) ||
			 !WiredFx_Finite( action->lodNear ) || !WiredFx_Finite( action->lodFar ) ||
			 !WiredFx_Finite( action->boundsRadius ) || !WiredFx_FiniteArray( action->offset, 3u ) ||
			 !WiredFx_FiniteArray( action->rotationDegrees, 3u ) ||
			 !WiredFx_FiniteArray( action->color, 4u ) || action->delay[0] < 0.0f ||
			 action->delay[1] < action->delay[0] || action->duration < 0.0f ||
			 action->fadeInTime < 0.0f || action->fadeOutTime < 0.0f || action->lodNear < 0.0f ||
			 action->lodFar < action->lodNear || action->boundsRadius < 0.0f )
			return WIRED_FX_INVALID_ACTION_RANGE;
		if ( action->maxInstances == 0u || action->maxInstances > profile->maxInstances )
			return WIRED_FX_INVALID_ACTION_BUDGET;
		payloadResult = WiredFx_ValidatePayload( action );
		if ( payloadResult != WIRED_FX_VALID ) return payloadResult;
	}

	if ( profile->parentProfile == 0u && !WiredFx_ActionGraphAcyclic( profile ) )
		return WIRED_FX_INVALID_ACTION_GRAPH;
	return WIRED_FX_VALID;
}

void WiredFx_InitRegistry( wiredFxRegistry_t *registry ) {
	if ( !registry ) return;
	memset( registry, 0, sizeof( *registry ) );
	registry->generation = 1u;
}

qboolean WiredFx_RegisterProfile( wiredFxRegistry_t *registry, uint32_t handle,
	const wiredFxProfile_t *profile ) {
	uint32_t slot;
	wiredFxProfile_t resolved;
	if ( !registry || handle == 0u || handle > WIRED_FX_MAX_PROFILES ||
		 WiredFx_ValidateProfile( profile ) != WIRED_FX_VALID )
		return qfalse;
	slot = handle - 1u;
	if ( registry->registered[slot] || registry->generation == UINT64_MAX ) return qfalse;

	resolved = *profile;
	if ( profile->parentProfile != 0u ) {
		const wiredFxProfile_t *parent;
		uint32_t finalActionCount;
		uint32_t i;
		uint32_t parentProfile = profile->parentProfile;
		uint32_t actionOverrideMask = profile->actionOverrideMask;

		if ( parentProfile >= handle || parentProfile > WIRED_FX_MAX_PROFILES ||
			 !registry->registered[parentProfile - 1u] )
			return qfalse;
		parent = &registry->profiles[parentProfile - 1u];
		finalActionCount = profile->actionCount > parent->actionCount ? profile->actionCount : parent->actionCount;
		resolved.actionCount = finalActionCount;
		for ( i = 0u; i < finalActionCount; ++i ) {
			const qboolean overridden = i < profile->actionCount &&
				( actionOverrideMask & ( 1u << i ) ) != 0u;
			if ( overridden ) {
				resolved.actions[i] = profile->actions[i];
			} else if ( i < parent->actionCount ) {
				resolved.actions[i] = parent->actions[i];
			} else {
				return qfalse;
			}
		}

		/* Validate the fully materialized graph, then retain provenance. */
		resolved.parentProfile = 0u;
		resolved.actionOverrideMask = 0u;
		if ( WiredFx_ValidateProfile( &resolved ) != WIRED_FX_VALID ) return qfalse;
		resolved.parentProfile = parentProfile;
		resolved.actionOverrideMask = actionOverrideMask;
	}

	registry->profiles[slot] = resolved;
	registry->registered[slot] = qtrue;
	registry->count++;
	registry->generation++;
	return qtrue;
}

qboolean WiredFx_GetProfile( const wiredFxRegistry_t *registry, uint32_t handle,
	wiredFxProfile_t *outProfile ) {
	uint32_t slot;
	if ( !registry || !outProfile || handle == 0u || handle > WIRED_FX_MAX_PROFILES ) return qfalse;
	slot = handle - 1u;
	if ( !registry->registered[slot] ) return qfalse;
	*outProfile = registry->profiles[slot];
	return qtrue;
}

wiredFxValidationError_t WiredFx_ValidateEvent( const wiredFxRegistry_t *registry,
	const wiredFxEvent_t *event ) {
	if ( !registry || !event || event->schemaVersion != WIRED_FX_EVENT_SCHEMA_VERSION ||
		 event->profile == 0u || event->profile > WIRED_FX_MAX_PROFILES ||
		 !registry->registered[event->profile - 1u] || event->eventId == 0u ||
		 ( event->flags & ~WIRED_FX_KNOWN_EVENT_FLAGS ) != 0u ||
		 !WiredFx_Finite( event->startTimeSeconds ) || event->startTimeSeconds < 0.0f ||
		 !WiredFx_FiniteArray( event->origin, 3u ) || !WiredFx_FiniteArray( event->axis, 9u ) ||
		 !WiredFx_FiniteArray( event->color, 4u ) || !WiredFx_Finite( event->intensity ) ||
		 !WiredFx_Finite( event->sizeScale ) || event->intensity < 0.0f || event->sizeScale < 0.0f )
		return WIRED_FX_INVALID_EVENT;
	if ( ( event->flags & WIRED_FX_EVENT_HAS_END_ORIGIN ) != 0u &&
		 !WiredFx_FiniteArray( event->endOrigin, 3u ) )
		return WIRED_FX_INVALID_EVENT;
	if ( ( event->flags & WIRED_FX_EVENT_HAS_VELOCITY ) != 0u &&
		 !WiredFx_FiniteArray( event->velocity, 3u ) )
		return WIRED_FX_INVALID_EVENT;
	if ( ( event->flags & WIRED_FX_EVENT_HAS_SOURCE_ENTITY ) != 0u &&
		 ( event->sourceEntityNum < 0 || event->sourceEntityNum >= MAX_GENTITIES ) )
		return WIRED_FX_INVALID_EVENT;
	if ( !WiredFx_Finite( event->pathSpacing ) || event->pathSpacing < 0.0f ||
		 !WiredFx_Finite( event->timeSpanSeconds ) || event->timeSpanSeconds < 0.0f ||
		 event->timeSpanSeconds > 1.0f )
		return WIRED_FX_INVALID_EVENT;
	if ( !WiredFx_Finite( event->shakeDurationSeconds ) ||
		 !WiredFx_Finite( event->shakeFadeInSeconds ) ||
		 !WiredFx_Finite( event->shakeFadeOutSeconds ) ||
		 !WiredFx_Finite( event->shakeRadius ) ||
		 event->shakeDurationSeconds < 0.0f || event->shakeFadeInSeconds < 0.0f ||
		 event->shakeFadeOutSeconds < 0.0f || event->shakeRadius < 0.0f )
		return WIRED_FX_INVALID_EVENT;
	if ( event->flags & WIRED_FX_EVENT_HAS_SHAKE_OVERRIDE ) {
		if ( event->shakeDurationSeconds <= 0.0f ||
			 event->shakeFadeInSeconds + event->shakeFadeOutSeconds >
				event->shakeDurationSeconds )
			return WIRED_FX_INVALID_EVENT;
	} else if ( event->shakeDurationSeconds != 0.0f ||
			 event->shakeFadeInSeconds != 0.0f || event->shakeFadeOutSeconds != 0.0f ||
			 event->shakeRadius != 0.0f ) {
		return WIRED_FX_INVALID_EVENT;
	}
	return WIRED_FX_VALID;
}
