// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026-present Wired Engine contributors

/*
Client-owned WiredFX orchestration. Cgame emits one semantic POD event; this
layer schedules the immutable recipe and fans actions out to renderer, audio
and presentation services. Backend-native objects never enter the profile or
event ABI.
*/

#include "client.h"
#include "cl_wired_fx.h"

#include "../render/frontend/render_effect_runtime.h"

#ifdef WIRED_FX_LUA_AUTHORING
#include "../qcommon/wired/core/scripting/wired_fx_authoring.h"
#include "../qcommon/wired/core/scripting/wired_scripting.h"
#include <lua.h>
#include <lauxlib.h>
#endif

#include <math.h>
#include <string.h>

#define CL_WIRED_FX_PARTICLE_NAME 64
#define CL_WIRED_FX_MAX_SHAKES 64

typedef struct {
	char name[CL_WIRED_FX_PARTICLE_NAME];
	particleClassHandle_t handle;
} clWiredFxParticleName_t;

typedef struct {
	float start;
	float end;
	float magnitude;
	float fadeIn;
	float fadeOut;
	float radius;
	float decayExponent;
	float origin[3];
	float maxAngles[3];
	float maxOffset[3];
	uint32_t mode;
	uint32_t seed;
} clWiredFxShake_t;

typedef struct {
	wiredFxRegistry_t registry;
	wiredFxRuntime_t runtime;
	clWiredFxParticleName_t particleNames[MAX_PARTICLE_CLASSES];
	uint32_t particleNameCount;
	clWiredFxShake_t shakes[CL_WIRED_FX_MAX_SHAKES];
	wiredFxFrameState_t presentation;
	float windEnd;
	float radialBlurEnd;
	uint64_t actionDispatches[WIRED_FX_ACTION_COUNT];
	uint64_t degradedActions;
	uint64_t presentationDrops;
	uint64_t profileLoadFailures;
	uint64_t unknownParticleResources;
} clWiredFxState_t;

static clWiredFxState_t s_fx;
LOG_DECLARE_CHANNEL( ch_wired_fx, "wired-fx" );

static uint32_t CL_WiredFx_Hash( const char *text ) {
	uint32_t hash = 2166136261u;
	while ( text && *text ) { hash ^= (uint8_t)*text++; hash *= 16777619u; }
	return hash ? hash : 1u;
}

void CL_WiredFx_BeginRegistration( void ) {
	memset( &s_fx, 0, sizeof( s_fx ) );
	WiredFx_InitRegistry( &s_fx.registry );
	WiredFx_InitRuntime( &s_fx.runtime );
	s_fx.presentation.schemaVersion = WIRED_FX_FRAME_SCHEMA_VERSION;
}

void CL_WiredFx_Stats_f( void ) {
	uint32_t i;
	Com_Log( SEV_INFO, LOG_CH(ch_wired_fx), "profiles=%u active=%u admitted=%llu expired=%llu droppedEvents=%llu duplicates=%llu\n",
		s_fx.registry.count, s_fx.runtime.activeCount,
		(unsigned long long)s_fx.runtime.admittedEvents,
		(unsigned long long)s_fx.runtime.expiredEvents,
		(unsigned long long)s_fx.runtime.droppedEvents,
		(unsigned long long)s_fx.runtime.duplicateEvents );
	Com_Log( SEV_INFO, LOG_CH(ch_wired_fx), "actions dispatched=%llu unsupported=%llu dropped=%llu degraded=%llu presentationDrops=%llu\n",
		(unsigned long long)s_fx.runtime.dispatchedActions,
		(unsigned long long)s_fx.runtime.unsupportedActions,
		(unsigned long long)s_fx.runtime.droppedActions,
		(unsigned long long)s_fx.degradedActions,
		(unsigned long long)s_fx.presentationDrops );
	Com_Log( SEV_INFO, LOG_CH(ch_wired_fx), "loadFailures=%llu unknownParticles=%llu frameFlags=0x%x environment=%u parms=%u\n",
		(unsigned long long)s_fx.profileLoadFailures,
		(unsigned long long)s_fx.unknownParticleResources,
		s_fx.presentation.flags, s_fx.presentation.environment,
		s_fx.presentation.renderParmCount );
	for ( i = 0u; i < WIRED_FX_ACTION_COUNT; ++i )
		Com_Log( SEV_INFO, LOG_CH(ch_wired_fx), "%-16s %llu\n", WiredFx_ActionTypeName( i ),
			(unsigned long long)s_fx.actionDispatches[i] );
}

void CL_WiredFx_RegisterParticleClass( particleClassHandle_t handle,
		const particleClass_t *particleClass, const char *name ) {
	uint32_t i;
	if ( !particleClass || !name || !*name || handle <= 0 ) return;
	if ( re.RegisterParticleClass ) re.RegisterParticleClass( handle, particleClass );
	for ( i = 0u; i < s_fx.particleNameCount; ++i ) {
		if ( !Q_stricmp( s_fx.particleNames[i].name, name ) ) {
			s_fx.particleNames[i].handle = handle;
			return;
		}
	}
	if ( s_fx.particleNameCount >= ARRAY_LEN( s_fx.particleNames ) ) return;
	Q_strncpyz( s_fx.particleNames[s_fx.particleNameCount].name, name,
		sizeof( s_fx.particleNames[s_fx.particleNameCount].name ) );
	s_fx.particleNames[s_fx.particleNameCount++].handle = handle;
}

#ifdef WIRED_FX_LUA_AUTHORING
static qboolean CL_WiredFx_PushFileResult( lua_State *L, const char *path ) {
	fileHandle_t file;
	char *source;
	char chunkName[MAX_QPATH + 2];
	int length = FS_FOpenFileRead( path, &file, qfalse );
	int status;
	if ( length <= 0 || !file ) return qfalse;
	source = Z_Malloc( length + 1 );
	FS_Read( source, length, file ); source[length] = '\0'; FS_FCloseFile( file );
	Com_sprintf( chunkName, sizeof( chunkName ), "@%s", path );
	status = luaL_loadbuffer( L, source, (size_t)length, chunkName ); Z_Free( source );
	if ( status == 0 ) status = lua_pcall( L, 0, 1, 0 );
	if ( status != 0 ) {
		Com_Log( SEV_ERROR, LOG_CH(ch_wired_fx), "Lua load failed for '%s': %s\n",
			path, lua_tostring( L, -1 ) ); lua_pop( L, 1 ); return qfalse;
	}
	if ( !lua_istable( L, -1 ) ) { lua_pop( L, 1 ); return qfalse; }
	return qtrue;
}

static uint32_t CL_WiredFx_ResolveResource( wiredFxResourceType_t type,
		const char *name, void *userData ) {
	uint32_t i;
	(void)userData;
	switch ( type ) {
	case WIRED_FX_RESOURCE_MATERIAL: return re.RegisterShader ? (uint32_t)re.RegisterShader( name ) : 0u;
	case WIRED_FX_RESOURCE_PARTICLE_CLASS:
		for ( i = 0u; i < s_fx.particleNameCount; ++i )
			if ( !Q_stricmp( s_fx.particleNames[i].name, name ) ) return (uint32_t)s_fx.particleNames[i].handle;
		s_fx.unknownParticleResources++; return 0u;
	case WIRED_FX_RESOURCE_MODEL: return re.RegisterModel ? (uint32_t)re.RegisterModel( name ) : 0u;
	case WIRED_FX_RESOURCE_SOUND: return (uint32_t)S_RegisterSound( name, qfalse );
	case WIRED_FX_RESOURCE_FLARE: return re.RegisterShader ? (uint32_t)re.RegisterShader( name ) : 0u;
	case WIRED_FX_RESOURCE_RIBBON: return re.RegisterPrimitiveShader ? (uint32_t)re.RegisterPrimitiveShader( name ) : 0u;
	case WIRED_FX_RESOURCE_CURVE:
	case WIRED_FX_RESOURCE_RENDER_PARM:
	case WIRED_FX_RESOURCE_ENVIRONMENT:
		return CL_WiredFx_Hash( name );
	default: return 0u;
	}
}
#endif

void CL_WiredFx_LoadProfiles( void ) {
#ifdef WIRED_FX_LUA_AUTHORING
	lua_State *L = WiredScript_GetState();
	wiredFxAuthoringResolver_t resolver = { CL_WiredFx_ResolveResource, NULL };
	int base, manifest, count, i;
	if ( !L ) return;
	base = lua_gettop( L );
	if ( !CL_WiredFx_PushFileResult( L, "scripts/effects/manifest.lua" ) ) {
		lua_settop( L, base );
		Com_Log( SEV_INFO, LOG_CH(ch_wired_fx), "No WiredFX manifest; runtime remains empty\n" );
		return;
	}
	manifest = lua_gettop( L ); count = (int)lua_objlen( L, manifest );
	for ( i = 1; i <= count; ++i ) {
		uint32_t handle = 0u;
		const char *path = NULL;
		wiredFxProfile_t profile;
		char error[192];
		error[0] = '\0';
		lua_rawgeti( L, manifest, i );
		if ( lua_istable( L, -1 ) ) {
			lua_getfield( L, -1, "handle" ); if ( lua_isnumber( L, -1 ) ) handle = (uint32_t)lua_tonumber( L, -1 ); lua_pop( L, 1 );
			lua_getfield( L, -1, "path" ); if ( lua_isstring( L, -1 ) ) path = lua_tostring( L, -1 ); lua_pop( L, 1 );
			if ( handle && path ) {
				char pathCopy[MAX_QPATH]; Q_strncpyz( pathCopy, path, sizeof( pathCopy ) );
				if ( CL_WiredFx_PushFileResult( L, pathCopy ) &&
					 WiredFxAuthoring_ReadProfile( &profile, L, -1, &resolver, error, sizeof( error ) ) &&
					 WiredFx_RegisterProfile( &s_fx.registry, handle, &profile ) ) {
					lua_pop( L, 1 );
					Com_Log( SEV_INFO, LOG_CH(ch_wired_fx), "registered profile %u '%s' (%u actions)\n",
						handle, pathCopy, profile.actionCount );
				} else {
					if ( lua_istable( L, -1 ) && lua_gettop( L ) > manifest + 1 ) lua_pop( L, 1 );
					s_fx.profileLoadFailures++;
					Com_Log( SEV_ERROR, LOG_CH(ch_wired_fx), "rejected profile %u '%s': %s\n",
						handle, pathCopy, error[0] ? error : "load/validation failure" );
				}
			}
		}
		lua_pop( L, 1 );
	}
	lua_settop( L, base );
#endif
}

qboolean CL_WiredFx_SubmitEvent( const wiredFxEvent_t *event ) {
	return WiredFx_SubmitEvent( &s_fx.runtime, &s_fx.registry, event );
}

static void CL_WiredFx_Color( float out[4], const wiredFxAction_t *action,
		const wiredFxEvent_t *event ) {
	int i; for ( i = 0; i < 4; ++i ) out[i] = action->color[i] * event->color[i];
}

typedef struct { float now; } clWiredFxDispatchContext_t;

static void CL_WiredFx_CopyEventAxis( vec3_t out[3], const wiredFxEvent_t *event ) {
	int i;
	for ( i = 0; i < 3; ++i ) VectorCopy( &event->axis[i * 3], out[i] );
}

static void CL_WiredFx_ActionTransform( vec3_t origin, vec3_t axis[3],
		const wiredFxAction_t *action, const wiredFxEvent_t *event ) {
	vec3_t direction;
	int i;

	CL_WiredFx_CopyEventAxis( axis, event );
	switch ( (wiredFxRotationType_t)action->rotationType ) {
	case WIRED_FX_ROTATION_TRACK_AXIS:
	case WIRED_FX_ROTATION_TRACK_AXIS_PARENT:
	case WIRED_FX_ROTATION_TRACK_LOCAL_AXIS:
		if ( event->flags & WIRED_FX_EVENT_HAS_VELOCITY ) {
			VectorCopy( event->velocity, direction );
		} else if ( event->flags & WIRED_FX_EVENT_HAS_END_ORIGIN ) {
			VectorSubtract( event->endOrigin, event->origin, direction );
		} else {
			s_fx.degradedActions++;
			break;
		}
		if ( VectorNormalize( direction ) == 0.0f ) {
			s_fx.degradedActions++;
			break;
		}
		VectorCopy( direction, axis[0] );
		MakeNormalVectors( axis[0], axis[1], axis[2] );
		break;
	case WIRED_FX_ROTATION_EXPLICIT_ANGLES:
		AnglesToAxis( action->rotationDegrees, axis );
		break;
	case WIRED_FX_ROTATION_EXPLICIT_CURVES:
	case WIRED_FX_ROTATION_EXPLICIT_CURVES_LOCAL:
		/* Curve sampling is not part of schema 1. Preserve the authored static
		 * angle and expose the deterministic reduction in telemetry. */
		AnglesToAxis( action->rotationDegrees, axis );
		s_fx.degradedActions++;
		break;
	case WIRED_FX_ROTATION_EXTERNAL:
		/* External transforms require a future entity binding in the semantic
		 * event. Schema 1 falls back to the captured start axis. */
		s_fx.degradedActions++;
		break;
	default:
		break;
	}

	switch ( (wiredFxOriginType_t)action->originType ) {
	case WIRED_FX_ORIGIN_MIDPOINT:
		if ( event->flags & WIRED_FX_EVENT_HAS_END_ORIGIN ) {
			VectorAdd( event->origin, event->endOrigin, origin );
			VectorScale( origin, 0.5f, origin );
		} else {
			VectorCopy( event->origin, origin );
			s_fx.degradedActions++;
		}
		break;
	case WIRED_FX_ORIGIN_TRACK:
	case WIRED_FX_ORIGIN_TRACK_LOCAL:
	case WIRED_FX_ORIGIN_EXTERNAL:
		if ( event->flags & WIRED_FX_EVENT_HAS_END_ORIGIN ) {
			VectorCopy( event->endOrigin, origin );
		} else {
			VectorCopy( event->origin, origin );
			s_fx.degradedActions++;
		}
		break;
	default:
		VectorCopy( event->origin, origin );
		break;
	}
	for ( i = 0; i < 3; ++i ) VectorMA( origin, action->offset[i], axis[i], origin );
}

static qboolean CL_WiredFx_OptionalResourceMissing( const wiredFxAction_t *action ) {
	if ( ( action->flags & WIRED_FX_ACTION_OPTIONAL_RESOURCE ) == 0u ) return qfalse;
	switch ( (wiredFxActionType_t)action->type ) {
	case WIRED_FX_ACTION_LIGHT: return action->payload.light.material == 0u;
	case WIRED_FX_ACTION_PARTICLE: return action->payload.particle.particleClass == 0u;
	case WIRED_FX_ACTION_DECAL:
	case WIRED_FX_ACTION_DECAL2: return action->payload.decal.material == 0u;
	case WIRED_FX_ACTION_MODEL: return action->payload.model.model == 0u;
	case WIRED_FX_ACTION_SOUND: return action->payload.sound.sound == 0u;
	case WIRED_FX_ACTION_RENDER_PARM: return action->payload.renderParm.handle == 0u;
	case WIRED_FX_ACTION_ENV_OVERRIDE:
	case WIRED_FX_ACTION_ENV_CHANGE: return action->payload.environment.environment == 0u;
	case WIRED_FX_ACTION_FLARE: return action->payload.flare.flare == 0u;
	case WIRED_FX_ACTION_RIBBON:
	case WIRED_FX_ACTION_BEAM: return action->payload.ribbon.ribbon == 0u;
	case WIRED_FX_ACTION_GODRAY: return action->payload.godray.material == 0u;
	case WIRED_FX_ACTION_SPRITE: return action->payload.sprite.material == 0u;
	default: return qfalse;
	}
}

static float CL_WiredFx_FadeForDuration( const wiredFxAction_t *action,
		float age, float duration ) {
	float fade = 1.0f;
	if ( action->fadeInTime > 0.0f ) fade = MIN( fade, age / action->fadeInTime );
	if ( action->fadeOutTime > 0.0f && duration > 0.0f )
		fade = MIN( fade, MAX( duration - age, 0.0f ) / action->fadeOutTime );
	return Com_Clamp( 0.0f, 1.0f, fade );
}

static float CL_WiredFx_FadeAtAge( const wiredFxProfile_t *profile,
		const wiredFxAction_t *action, float age ) {
	return CL_WiredFx_FadeForDuration( action, age, profile->duration );
}

static float CL_WiredFx_Fade( const wiredFxProfile_t *profile,
		const wiredFxAction_t *action, const wiredFxEvent_t *event, float now ) {
	return CL_WiredFx_FadeAtAge( profile, action,
		MAX( now - event->startTimeSeconds, 0.0f ) );
}

static clWiredFxShake_t *CL_WiredFx_AllocateShake( float start ) {
	uint32_t i;

	for ( i = 0u; i < CL_WIRED_FX_MAX_SHAKES; ++i ) {
		clWiredFxShake_t *shake = &s_fx.shakes[i];
		if ( shake->end <= start ) return shake;
	}
	/* Match the retired cgame pool: preserve every admitted active shake and
	 * drop a new one when all 64 slots are occupied. */
	return NULL;
}

static qboolean CL_WiredFx_SetRenderParm( uint32_t handle, const float value[4] ) {
	uint32_t i;
	for ( i = 0u; i < s_fx.presentation.renderParmCount; ++i ) {
		if ( s_fx.presentation.renderParms[i].handle == handle ) {
			Vector4Copy( value, s_fx.presentation.renderParms[i].value );
			return qtrue;
		}
	}
	if ( s_fx.presentation.renderParmCount >= WIRED_FX_MAX_FRAME_RENDER_PARMS ) {
		s_fx.presentationDrops++;
		return qfalse;
	}
	i = s_fx.presentation.renderParmCount++;
	s_fx.presentation.renderParms[i].handle = handle;
	Vector4Copy( value, s_fx.presentation.renderParms[i].value );
	return qtrue;
}

static void CL_WiredFx_StartShake( float start, const wiredFxAction_t *action,
		const wiredFxEvent_t *event ) {
	clWiredFxShake_t *shake = CL_WiredFx_AllocateShake( start );
	qboolean override = ( event->flags & WIRED_FX_EVENT_HAS_SHAKE_OVERRIDE ) != 0u;
	float duration;
	if ( !shake ) return;
	memset( shake, 0, sizeof( *shake ) );
	duration = override ? event->shakeDurationSeconds : action->duration;
	shake->start = start;
	shake->end = start + MAX( duration, 0.001f );
	shake->magnitude = action->payload.screenShake.magnitude * event->intensity;
	shake->fadeIn = override ? event->shakeFadeInSeconds : action->fadeInTime;
	shake->fadeOut = override ? event->shakeFadeOutSeconds : action->fadeOutTime;
	shake->radius = override ? event->shakeRadius : action->payload.screenShake.radius;
	shake->decayExponent = action->payload.screenShake.decayExponent;
	shake->mode = action->payload.screenShake.mode;
	shake->seed = event->seed;
	VectorCopy( event->origin, shake->origin );
	VectorCopy( action->payload.screenShake.maxAngles, shake->maxAngles );
	VectorCopy( action->payload.screenShake.maxOffset, shake->maxOffset );
}

static uint32_t CL_WiredFx_RandomStep( uint32_t *seed ) {
	*seed = *seed * 1664525u + 1013904223u;
	return *seed;
}

static float CL_WiredFx_Random01( uint32_t *seed ) {
	return (float)( ( CL_WiredFx_RandomStep( seed ) >> 8 ) & 0xffffu ) / 65535.0f;
}

static uint32_t CL_WiredFx_SpatialSeed( const float origin[3] ) {
	uint32_t seed = (uint32_t)(int)( origin[0] * 8.0f ) * 73856093u
		^ (uint32_t)(int)( origin[1] * 8.0f ) * 19349663u
		^ (uint32_t)(int)( origin[2] * 8.0f ) * 83492791u;
	return seed ? seed : 1u;
}

static wiredFxDispatchResult_t CL_WiredFx_Dispatch( const wiredFxProfile_t *profile,
		const wiredFxAction_t *action, const wiredFxEvent_t *event, uint32_t actionIndex,
		uint32_t cycle, float scheduledTime, void *userData ) {
	clWiredFxDispatchContext_t *context = userData;
	vec3_t origin;
	vec3_t actionAxis[3];
	float color[4];
	float fade = CL_WiredFx_Fade( profile, action, event, context->now );
	(void)actionIndex; (void)cycle;
	CL_WiredFx_Color( color, action, event );
	color[3] *= fade;
	CL_WiredFx_ActionTransform( origin, actionAxis, action, event );
	if ( action->type < WIRED_FX_ACTION_COUNT ) s_fx.actionDispatches[action->type]++;
	if ( CL_WiredFx_OptionalResourceMissing( action ) ) {
		s_fx.degradedActions++;
		return WIRED_FX_DISPATCH_DROPPED;
	}
	switch ( (wiredFxActionType_t)action->type ) {
	case WIRED_FX_ACTION_LIGHT:
		if ( !re.AddLightToScene ) return WIRED_FX_DISPATCH_UNSUPPORTED;
		{
			uint32_t jitterSeed = event->seed ^ action->actionId ^ cycle * 0x9E3779B9u;
			uint32_t timeSeed = event->seed;
			float startJitter;
			float age;
			float lifetime = action->payload.light.lifetime > 0.0f
				? action->payload.light.lifetime : profile->duration;
			float lightFade;
			float jitter;
			if ( action->payload.light.startTimeJitterSteps >= 2u ) {
				uint32_t step = CL_WiredFx_RandomStep( &timeSeed ) %
					action->payload.light.startTimeJitterSteps;
				startJitter = action->payload.light.startTimeJitter * (float)step /
					(float)( action->payload.light.startTimeJitterSteps - 1u );
			} else {
				startJitter = action->payload.light.startTimeJitter *
					CL_WiredFx_Random01( &timeSeed );
			}
			age = MAX( context->now - ( event->startTimeSeconds - startJitter ), 0.0f );
			lightFade = CL_WiredFx_FadeForDuration( action, age, lifetime );
			if ( lifetime > 0.0f && age > lifetime ) return WIRED_FX_DISPATCH_EXECUTED;
			if ( action->payload.light.radiusJitterSteps >= 2u ) {
				uint32_t step = CL_WiredFx_RandomStep( &jitterSeed ) %
					action->payload.light.radiusJitterSteps;
				jitter = action->payload.light.radiusJitter * (float)step /
					(float)( action->payload.light.radiusJitterSteps - 1u );
			} else {
				jitter = action->payload.light.radiusJitter * CL_WiredFx_Random01( &jitterSeed );
			}
		re.AddLightToScene( origin,
			action->payload.light.intensity * event->intensity *
			( MAX( action->payload.light.radius[0], MAX( action->payload.light.radius[1], action->payload.light.radius[2] ) ) + jitter ) * lightFade,
			color[0], color[1], color[2] );
		return WIRED_FX_DISPATCH_EXECUTED;
		}
	case WIRED_FX_ACTION_PARTICLE: {
		emitterDesc_t desc;
		uint32_t count = action->payload.particle.maxParticles;
		qboolean authoredCount = qfalse;
		float pathSpacing = 0.0f;
		float pathPhase = 0.0f;
		qboolean usesPath = ( event->flags & WIRED_FX_EVENT_HAS_END_ORIGIN ) != 0u &&
			( event->pathSpacing > 0.0f || action->payload.particle.trailSpacing > 0.0f ||
			  action->payload.particle.spawnRate > 0.0f );
		if ( !re.EmitParticles ) return WIRED_FX_DISPATCH_UNSUPPORTED;
		if ( usesPath ) {
			float spacing = event->pathSpacing > 0.0f ? event->pathSpacing
				: action->payload.particle.trailSpacing;
			pathSpacing = spacing;
			if ( spacing > 0.0f ) {
				vec3_t delta;
				float distance;
				VectorSubtract( event->endOrigin, event->origin, delta );
				distance = VectorLength( delta );
				if ( action->payload.particle.pathSampling ==
					 WIRED_FX_PATH_SAMPLING_RANDOM_SPACING ) {
					uint32_t phaseSeed = event->seed ^ action->actionId;
					pathPhase = floorf( CL_WiredFx_Random01( &phaseSeed ) * spacing );
					count = pathPhase < distance
						? (uint32_t)ceilf( ( distance - pathPhase ) / spacing ) : 0u;
				} else {
					count = (uint32_t)ceilf( distance / spacing );
				}
				authoredCount = qtrue;
			}
		}
		if ( action->payload.particle.spawnRate > 0.0f ) {
			uint32_t rateCount;
			if ( action->payload.particle.rateBoundaryAligned ) {
				float nowBoundary = event->startTimeSeconds * action->payload.particle.spawnRate;
				float oldBoundary = ( event->startTimeSeconds - event->timeSpanSeconds ) *
					action->payload.particle.spawnRate;
				rateCount = (uint32_t)MAX( 0.0f,
					floorf( nowBoundary + 0.0001f ) - floorf( oldBoundary + 0.0001f ) );
			} else {
				rateCount = (uint32_t)ceilf(
					action->payload.particle.spawnRate * event->timeSpanSeconds );
			}
			if ( !authoredCount || rateCount > count ) count = rateCount;
			authoredCount = qtrue;
		}
		if ( count > action->payload.particle.maxParticles )
			count = action->payload.particle.maxParticles;
		if ( authoredCount && count == 0u ) return WIRED_FX_DISPATCH_EXECUTED;
		memset( &desc, 0, sizeof( desc ) ); desc.cls = (particleClassHandle_t)action->payload.particle.particleClass;
		desc.count = (int)count;
		VectorCopy( origin, desc.origin );
		VectorCopy( event->flags & WIRED_FX_EVENT_HAS_VELOCITY ? event->velocity : &event->axis[6], desc.axis );
		VectorNormalize( desc.axis );
		if ( usesPath ) {
			vec3_t path, direction, shift;
			VectorCopy( event->endOrigin, desc.end );
			VectorSubtract( desc.end, desc.origin, path );
			if ( action->payload.particle.pathSampling == WIRED_FX_PATH_SAMPLING_ENDPOINT ) {
				/* EMIT_PATH samples bin centres. This shift reproduces the legacy
				 * projectile helper's endpoint-inclusive distribution. */
				VectorScale( path, 0.5f / (float)count, shift );
				VectorAdd( desc.origin, shift, desc.origin );
				VectorAdd( desc.end, shift, desc.end );
			} else if ( action->payload.particle.pathSampling ==
					   WIRED_FX_PATH_SAMPLING_RANDOM_SPACING && pathSpacing > 0.0f ) {
				/* Rebuild an exact fixed-spacing segment around the requested sample
				 * centres: random leading phase, then one particle per spacing unit. */
				VectorNormalize2( path, direction );
				VectorMA( event->origin, pathPhase - 0.5f * pathSpacing,
					direction, desc.origin );
				VectorMA( event->origin,
					pathPhase + ( (float)count - 0.5f ) * pathSpacing,
					direction, desc.end );
			}
		}
		Vector4Copy( color, desc.colorTint );
		re.EmitParticles( &desc ); return WIRED_FX_DISPATCH_EXECUTED;
	}
	case WIRED_FX_ACTION_DECAL: case WIRED_FX_ACTION_DECAL2: {
		decalDesc_t desc;
		if ( !re.AddDecalToScene ) return WIRED_FX_DISPATCH_UNSUPPORTED;
		memset( &desc, 0, sizeof( desc ) ); VectorCopy( origin, desc.origin );
		VectorCopy( &event->axis[6], desc.normal ); VectorNormalize( desc.normal );
		desc.radius = action->payload.decal.size * event->sizeScale;
		desc.orientation = DEG2RAD( action->payload.decal.angle
			+ (float)( ( event->seed ^ action->actionId ) & 0xffffu )
				* ( 360.0f / 65536.0f ) ); Vector4Copy( color, desc.rgba );
		desc.shader = (qhandle_t)action->payload.decal.material; desc.lifetime = action->duration;
		re.AddDecalToScene( &desc ); return WIRED_FX_DISPATCH_EXECUTED;
	}
	case WIRED_FX_ACTION_MODEL: {
		refEntity_t entity; int i;
		if ( !re.AddRefEntityToScene ) return WIRED_FX_DISPATCH_UNSUPPORTED;
		memset( &entity, 0, sizeof( entity ) ); entity.reType = RT_MODEL;
		entity.hModel = (qhandle_t)action->payload.model.model; entity.customShader = (qhandle_t)action->payload.model.material;
		VectorCopy( origin, entity.origin ); for ( i = 0; i < 3; ++i ) VectorCopy( actionAxis[i], entity.axis[i] );
		for ( i = 0; i < 4; ++i ) entity.shader.rgba[i] = (byte)( Com_Clamp( 0, 1, color[i] ) * 255.0f );
		re.AddRefEntityToScene( &entity, qfalse ); return WIRED_FX_DISPATCH_EXECUTED;
	}
	case WIRED_FX_ACTION_SOUND: {
		if ( action->payload.sound.looping ) {
			if ( !( event->flags & WIRED_FX_EVENT_HAS_SOURCE_ENTITY ) )
				return WIRED_FX_DISPATCH_DROPPED;
			S_AddLoopingSound( event->sourceEntityNum, origin,
				( event->flags & WIRED_FX_EVENT_HAS_VELOCITY ) ? event->velocity : vec3_origin,
				(sfxHandle_t)action->payload.sound.sound );
			return WIRED_FX_DISPATCH_EXECUTED;
		}
		if ( action->payload.sound.sourceBound ) {
			if ( !( event->flags & WIRED_FX_EVENT_HAS_SOURCE_ENTITY ) )
				return WIRED_FX_DISPATCH_DROPPED;
			S_StartSound( NULL, event->sourceEntityNum,
				action->payload.sound.channel,
				(sfxHandle_t)action->payload.sound.sound );
		} else {
			S_StartSound( origin, ENTITYNUM_WORLD,
				action->payload.sound.channel,
				(sfxHandle_t)action->payload.sound.sound );
		}
		return WIRED_FX_DISPATCH_EXECUTED;
	}
	case WIRED_FX_ACTION_SCREEN_SHAKE:
		CL_WiredFx_StartShake( scheduledTime, action, event );
		return WIRED_FX_DISPATCH_EXECUTED;
	case WIRED_FX_ACTION_CONTROLLER_SHAKE: {
		float magnitude = MAX( action->payload.controllerShake.highMagnitude,
			action->payload.controllerShake.lowMagnitude );
		float duration = MAX( action->payload.controllerShake.highDuration,
			action->payload.controllerShake.lowDuration );
		clWiredFxShake_t *shake;
		/* No native haptic object crosses the RAL seam. Until the platform input
		 * service exposes a portable motor contract, degrade deterministically to
		 * camera feedback and make that fact visible in telemetry. */
		/* Controller fallback remains bounded camera feedback. */
		shake = CL_WiredFx_AllocateShake( scheduledTime );
		if ( !shake ) return WIRED_FX_DISPATCH_DROPPED;
		memset( shake, 0, sizeof( *shake ) );
		shake->start = scheduledTime;
		shake->end = scheduledTime + MAX( duration, 0.001f );
		shake->magnitude = magnitude * event->intensity;
		shake->fadeOut = duration;
		shake->decayExponent = 1.0f;
		shake->seed = event->seed ^ action->actionId;
		VectorCopy( event->origin, shake->origin );
		VectorSet( shake->maxOffset, 1.0f, 1.0f, 0.5f );
		s_fx.degradedActions++;
		return WIRED_FX_DISPATCH_EXECUTED;
	}
	case WIRED_FX_ACTION_WIND: {
		uint32_t mixed = event->seed ^ action->actionId * 0x9E3779B9u;
		float fraction = (float)( mixed & 0xffffu ) / 65535.0f;
		float angle = DEG2RAD( action->payload.wind.angle );
		float strength = action->payload.wind.strength[0] +
			( action->payload.wind.strength[1] - action->payload.wind.strength[0] ) * fraction;
		s_fx.presentation.flags |= WIRED_FX_FRAME_HAS_WIND;
		s_fx.presentation.windDirection[0] = cosf( angle );
		s_fx.presentation.windDirection[1] = sinf( angle );
		s_fx.presentation.windDirection[2] = 0.0f;
		s_fx.presentation.windStrength = strength * action->payload.wind.multiplier * event->intensity;
		s_fx.windEnd = scheduledTime + MAX( action->duration, 0.001f );
		return WIRED_FX_DISPATCH_EXECUTED;
	}
	case WIRED_FX_ACTION_RENDER_PARM:
		if ( !CL_WiredFx_SetRenderParm( action->payload.renderParm.handle,
			action->payload.renderParm.value ) ) return WIRED_FX_DISPATCH_DROPPED;
		s_fx.presentation.flags |= WIRED_FX_FRAME_HAS_RENDER_PARMS;
		return WIRED_FX_DISPATCH_EXECUTED;
	case WIRED_FX_ACTION_ENV_OVERRIDE:
		s_fx.presentation.renderParmCount = 0u;
		memset( s_fx.presentation.renderParms, 0, sizeof( s_fx.presentation.renderParms ) );
		/* fall through: override replaces, change merges */
	case WIRED_FX_ACTION_ENV_CHANGE: {
		uint32_t i;
		s_fx.presentation.environment = action->payload.environment.environment;
		s_fx.presentation.flags |= WIRED_FX_FRAME_HAS_ENVIRONMENT;
		for ( i = 0u; i < action->payload.environment.renderParmCount; ++i ) {
			if ( !CL_WiredFx_SetRenderParm( action->payload.environment.renderParms[i].handle,
				action->payload.environment.renderParms[i].value ) ) return WIRED_FX_DISPATCH_DROPPED;
		}
		if ( s_fx.presentation.renderParmCount ) s_fx.presentation.flags |= WIRED_FX_FRAME_HAS_RENDER_PARMS;
		return WIRED_FX_DISPATCH_EXECUTED;
	}
	case WIRED_FX_ACTION_FLARE: case WIRED_FX_ACTION_GODRAY: {
		spriteDesc_t desc;
		if ( !re.AddSpriteToScene ) return WIRED_FX_DISPATCH_UNSUPPORTED;
		memset( &desc, 0, sizeof( desc ) ); VectorCopy( origin, desc.origin ); Vector4Copy( color, desc.rgba );
		desc.shader = action->type == WIRED_FX_ACTION_FLARE ? (qhandle_t)action->payload.flare.flare : (qhandle_t)action->payload.godray.material;
		desc.radius = event->sizeScale * ( action->type == WIRED_FX_ACTION_GODRAY ? (float)action->payload.godray.size : 1.0f );
		desc.flags = PRIM_FLAG_ADDITIVE; re.AddSpriteToScene( &desc ); return WIRED_FX_DISPATCH_EXECUTED;
	}
	case WIRED_FX_ACTION_RIBBON:
	case WIRED_FX_ACTION_BEAM: {
		uint32_t i, count = action->payload.ribbon.count;
		float age = MAX( context->now - event->startTimeSeconds, 0.0f );
		float lifeFade = 1.0f;
		uint32_t seed = CL_WiredFx_SpatialSeed( event->origin );
		if ( action->type == WIRED_FX_ACTION_BEAM ) {
			if ( !re.AddBeamToScene ) return WIRED_FX_DISPATCH_UNSUPPORTED;
		} else if ( !re.AddRibbonToScene ) return WIRED_FX_DISPATCH_UNSUPPORTED;
		if ( action->payload.ribbon.lifetime > 0.0f ) {
			if ( age > action->payload.ribbon.lifetime ) return WIRED_FX_DISPATCH_EXECUTED;
			if ( action->payload.ribbon.fadeOut > 0.0f &&
				 age > action->payload.ribbon.lifetime - action->payload.ribbon.fadeOut )
				lifeFade = ( action->payload.ribbon.lifetime - age ) / action->payload.ribbon.fadeOut;
		}
		for ( i = 0u; i < count; ++i ) {
			ribbonPoint_t points[2]; ribbonDesc_t desc;
			vec3_t end;
			memset( points, 0, sizeof( points ) ); memset( &desc, 0, sizeof( desc ) );
			VectorCopy( origin, points[0].pos );
			if ( count == 1u && ( event->flags & WIRED_FX_EVENT_HAS_END_ORIGIN ) ) {
				VectorCopy( event->endOrigin, end );
			} else {
				vec3_t direction;
				float tangentA = CL_WiredFx_Random01( &seed ) * 2.0f - 1.0f;
				float tangentB = CL_WiredFx_Random01( &seed ) * 2.0f - 1.0f;
				float length = action->payload.ribbon.length[0]
					+ CL_WiredFx_Random01( &seed ) *
						( action->payload.ribbon.length[1] - action->payload.ribbon.length[0] );
				float normalScale = action->payload.ribbon.normalScale[0]
					+ CL_WiredFx_Random01( &seed ) *
						( action->payload.ribbon.normalScale[1] - action->payload.ribbon.normalScale[0] );
				VectorScale( &event->axis[6], normalScale, direction );
				VectorMA( direction, tangentA * action->payload.ribbon.spread, &event->axis[0], direction );
				VectorMA( direction, tangentB * action->payload.ribbon.spread, &event->axis[3], direction );
				VectorNormalize( direction );
				VectorMA( origin, length, direction, end );
			}
			VectorCopy( end, points[1].pos );
			points[0].width = action->payload.ribbon.width * event->sizeScale;
			points[1].width = action->payload.ribbon.endWidth * event->sizeScale;
			for ( int channel = 0; channel < 4; ++channel ) {
				points[0].rgba[channel] = color[channel] * action->payload.ribbon.startColor[channel] * lifeFade;
				points[1].rgba[channel] = color[channel] * action->payload.ribbon.endColor[channel] * lifeFade;
			}
			if ( action->type == WIRED_FX_ACTION_BEAM ) {
				beamDesc_t beam;
				memset( &beam, 0, sizeof( beam ) );
				VectorCopy( points[0].pos, beam.start );
				VectorCopy( points[1].pos, beam.end );
				beam.startWidth = points[0].width;
				beam.endWidth = points[1].width;
				Vector4Copy( points[0].rgba, beam.startColor );
				Vector4Copy( points[1].rgba, beam.endColor );
				beam.shader = (qhandle_t)action->payload.ribbon.ribbon;
				beam.duration = action->payload.ribbon.lifetime;
				beam.fadeOut = action->payload.ribbon.fadeOut;
				beam.axialCopies = 1;
				beam.startEntityNum = -1;
				beam.endEntityNum = -1;
				re.AddBeamToScene( &beam );
			} else {
				desc.points = points; desc.numPoints = 2;
				desc.shader = (qhandle_t)action->payload.ribbon.ribbon;
				re.AddRibbonToScene( &desc );
			}
		}
		return WIRED_FX_DISPATCH_EXECUTED;
	}
	case WIRED_FX_ACTION_RADIAL_BLUR:
		s_fx.presentation.flags |= WIRED_FX_FRAME_HAS_RADIAL_BLUR;
		s_fx.presentation.radialBlurScale = action->payload.radialBlur.maxScale *
			event->intensity * fade;
		s_fx.radialBlurEnd = scheduledTime + MAX( action->duration, 0.001f );
		return WIRED_FX_DISPATCH_EXECUTED;
	case WIRED_FX_ACTION_FADE_PARENT: return WIRED_FX_DISPATCH_EXECUTED;
	case WIRED_FX_ACTION_SPRITE: {
		refEntity_t entity;
		uint32_t seed = event->seed ^ action->actionId;
		uint32_t timeSeed = event->seed;
		float lifetime = action->payload.sprite.lifetime > 0.0f
			? action->payload.sprite.lifetime : profile->duration;
		float jitter;
		if ( action->payload.sprite.startTimeJitterSteps >= 2u ) {
			uint32_t step = CL_WiredFx_RandomStep( &timeSeed ) %
				action->payload.sprite.startTimeJitterSteps;
			jitter = action->payload.sprite.startTimeJitter * (float)step /
				(float)( action->payload.sprite.startTimeJitterSteps - 1u );
		} else {
			jitter = action->payload.sprite.startTimeJitter * CL_WiredFx_Random01( &timeSeed );
		}
		float startTime = event->startTimeSeconds - jitter;
		float age = MAX( context->now - startTime, 0.0f );
		float fraction = lifetime > 0.0f ? Com_Clamp( 0.0f, 1.0f, age / lifetime ) : 0.0f;
		if ( !re.AddRefEntityToScene ) return WIRED_FX_DISPATCH_UNSUPPORTED;
		if ( lifetime > 0.0f && age > lifetime )
			return WIRED_FX_DISPATCH_EXECUTED;
		color[3] = action->color[3] * event->color[3] *
			CL_WiredFx_FadeForDuration( action, age, lifetime );
		memset( &entity, 0, sizeof( entity ) );
		entity.reType = RT_SPRITE;
		entity.customShader = (qhandle_t)action->payload.sprite.material;
		entity.radius = ( action->payload.sprite.radius[0]
			+ ( action->payload.sprite.radius[1] - action->payload.sprite.radius[0] ) * fraction ) * event->sizeScale;
		entity.rotation = action->payload.sprite.randomRotation
			? (float)( CL_WiredFx_RandomStep( &seed ) % 360u ) : 0.0f;
		entity.shaderTime.f = startTime;
		/* First-person weapon geometry owns the bounded weapon depth range.
		 * View-attached muzzle sprites must opt into the same range or nearby
		 * world decals can occlude smoke that physically starts at tag_flash. */
		if ( ( action->flags & WIRED_FX_ACTION_VIEW_DEPTH_HACK ) &&
				( event->conditionMask & WIRED_FX_CONDITION_LOCAL_VIEW ) )
			entity.renderfx = RF_DEPTHHACK | RF_FIRST_PERSON;
		VectorCopy( origin, entity.origin );
		VectorMA( entity.origin, age, action->payload.sprite.velocity, entity.origin );
		for ( int channel = 0; channel < 4; ++channel )
			entity.shader.rgba[channel] = (byte)( Com_Clamp( 0, 1, color[channel] ) * 255.0f );
		re.AddRefEntityToScene( &entity, qfalse );
		return WIRED_FX_DISPATCH_EXECUTED;
	}
	default: return WIRED_FX_DISPATCH_UNSUPPORTED;
	}
}

void CL_WiredFx_ServiceScene( refdef_t *refdef ) {
	clWiredFxDispatchContext_t context;
	float now;
	if ( !refdef ) return;
	now = (float)refdef->time * 0.001f;
	context.now = now;
	if ( now >= s_fx.windEnd ) {
		s_fx.presentation.flags &= ~WIRED_FX_FRAME_HAS_WIND;
		VectorClear( s_fx.presentation.windDirection );
		s_fx.presentation.windStrength = 0.0f;
	}
	if ( now >= s_fx.radialBlurEnd ) {
		s_fx.presentation.flags &= ~WIRED_FX_FRAME_HAS_RADIAL_BLUR;
		s_fx.presentation.radialBlurScale = 0.0f;
	}
	WiredFx_Service( &s_fx.runtime, &s_fx.registry, now, refdef->vieworg,
		CL_WiredFx_Dispatch, &context );
	refdef->wiredFx = s_fx.presentation;
	{
		vec3_t angleDelta = { 0.0f, 0.0f, 0.0f };
		uint32_t i;
		for ( i = 0u; i < CL_WIRED_FX_MAX_SHAKES; ++i ) {
			clWiredFxShake_t *shake = &s_fx.shakes[i];
			float duration, age, remaining, envelope = 1.0f, distanceScale = 1.0f;
			if ( now < shake->start || now >= shake->end ) continue;
			duration = MAX( shake->end - shake->start, 0.001f );
			age = now - shake->start;
			remaining = ( shake->end - now ) / duration;
			if ( shake->fadeIn > 0.0f && age < shake->fadeIn )
				envelope = age / shake->fadeIn;
			if ( shake->fadeOut > 0.0f && remaining * duration < shake->fadeOut )
				envelope = MIN( envelope, remaining * duration / shake->fadeOut );
			if ( shake->radius > 0.0f ) {
				float distance = Distance( refdef->vieworg, shake->origin );
				if ( distance >= shake->radius ) distanceScale = 0.0f;
				else distanceScale = 1.0f - distance / shake->radius;
			}
			envelope *= distanceScale * shake->magnitude;
			if ( shake->mode == 1u ) {
				float kick = powf( remaining, shake->decayExponent ) * envelope;
				angleDelta[PITCH] -= shake->maxAngles[PITCH] * kick;
				angleDelta[YAW] += shake->maxAngles[YAW] * kick;
				angleDelta[ROLL] += shake->maxAngles[ROLL] * kick *
					( ( shake->seed & 1u ) ? 1.0f : -1.0f );
			} else {
				uint32_t noise = shake->seed ^ (uint32_t)refdef->time * 0x9E3779B9u;
				float randomValue[6];
				int channel;
				for ( channel = 0; channel < 6; ++channel )
					randomValue[channel] = CL_WiredFx_Random01( &noise ) * 2.0f - 1.0f;
				refdef->vieworg[0] += randomValue[0] * shake->maxOffset[0] * envelope;
				refdef->vieworg[1] += randomValue[1] * shake->maxOffset[1] * envelope;
				refdef->vieworg[2] += randomValue[2] * shake->maxOffset[2] * envelope;
				angleDelta[PITCH] += randomValue[3] * shake->maxAngles[PITCH] * envelope;
				angleDelta[YAW] += randomValue[4] * shake->maxAngles[YAW] * envelope;
				angleDelta[ROLL] += randomValue[5] * shake->maxAngles[ROLL] * envelope;
			}
		}
		if ( angleDelta[0] != 0.0f || angleDelta[1] != 0.0f || angleDelta[2] != 0.0f ) {
			vec3_t deltaAxis[3], shakenAxis[3];
			AnglesToAxis( angleDelta, deltaAxis );
			MatrixMultiply( deltaAxis, refdef->viewaxis, shakenAxis );
			AxisCopy( shakenAxis, refdef->viewaxis );
		}
	}
}
