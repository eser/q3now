// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026-present Wired Engine contributors

#include "wired_fx_authoring.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>

#include <lua.h>

typedef struct {
	char id[64];
	char parent[64];
	char fire[64];
} wiredFxAuthoringRefs_t;

static void WiredFxAuthoring_Error( char *error, uint32_t size, const char *format, ... ) {
	va_list args;
	if ( !error || size == 0u ) return;
	va_start( args, format );
	vsnprintf( error, size, format, args );
	va_end( args );
	error[size - 1u] = '\0';
}

static int WiredFxAuthoring_AbsIndex( lua_State *L, int index ) {
	return index < 0 ? lua_gettop( L ) + index + 1 : index;
}

static qboolean WiredFxAuthoring_Number( lua_State *L, int table, const char *key,
	float defaultValue, float *out ) {
	lua_getfield( L, table, key );
	if ( lua_isnil( L, -1 ) ) *out = defaultValue;
	else if ( lua_isnumber( L, -1 ) ) *out = (float)lua_tonumber( L, -1 );
	else { lua_pop( L, 1 ); return qfalse; }
	lua_pop( L, 1 );
	return qtrue;
}

static qboolean WiredFxAuthoring_U32( lua_State *L, int table, const char *key,
	uint32_t defaultValue, uint32_t *out ) {
	float value;
	if ( !WiredFxAuthoring_Number( L, table, key, (float)defaultValue, &value ) ||
		 value < 0.0f || value > 4294967295.0f || value != (float)(uint32_t)value ) return qfalse;
	*out = (uint32_t)value;
	return qtrue;
}

/* Lua numbers are exact for the condition bits commonly authored directly.
   A {low32, high32} tuple preserves the full ABI without precision loss. */
static qboolean WiredFxAuthoring_U64( lua_State *L, int table, const char *key,
	uint64_t *out ) {
	lua_Number value;
	lua_getfield( L, table, key );
	if ( lua_isnil( L, -1 ) ) *out = 0u;
	else if ( lua_isnumber( L, -1 ) ) {
		value = lua_tonumber( L, -1 );
		if ( value < 0.0 || value > 9007199254740991.0 || value != (lua_Number)(uint64_t)value ) {
			lua_pop( L, 1 ); return qfalse;
		}
		*out = (uint64_t)value;
	} else if ( lua_istable( L, -1 ) ) {
		uint32_t words[2]; int i;
		for ( i = 0; i < 2; ++i ) {
			lua_rawgeti( L, -1, i + 1 );
			value = lua_tonumber( L, -1 );
			if ( !lua_isnumber( L, -1 ) || value < 0.0 || value > 4294967295.0 ||
				 value != (lua_Number)(uint32_t)value ) { lua_pop( L, 2 ); return qfalse; }
			words[i] = (uint32_t)value; lua_pop( L, 1 );
		}
		*out = (uint64_t)words[0] | ( (uint64_t)words[1] << 32 );
	} else { lua_pop( L, 1 ); return qfalse; }
	lua_pop( L, 1 ); return qtrue;
}

static qboolean WiredFxAuthoring_Vector( lua_State *L, int table, const char *key,
	float *out, uint32_t count, const float *defaults ) {
	uint32_t i;
	for ( i = 0u; i < count; ++i ) out[i] = defaults ? defaults[i] : 0.0f;
	lua_getfield( L, table, key );
	if ( lua_isnil( L, -1 ) ) { lua_pop( L, 1 ); return qtrue; }
	if ( !lua_istable( L, -1 ) ) { lua_pop( L, 1 ); return qfalse; }
	for ( i = 0u; i < count; ++i ) {
		lua_rawgeti( L, -1, (int)i + 1 );
		if ( !lua_isnumber( L, -1 ) ) { lua_pop( L, 2 ); return qfalse; }
		out[i] = (float)lua_tonumber( L, -1 );
		lua_pop( L, 1 );
	}
	lua_pop( L, 1 );
	return qtrue;
}

static uint32_t WiredFxAuthoring_Hash( const char *text ) {
	uint32_t hash = 2166136261u;
	while ( text && *text ) { hash ^= (uint8_t)*text++; hash *= 16777619u; }
	return hash ? hash : 1u;
}

static qboolean WiredFxAuthoring_Id( lua_State *L, int table, const char *key,
	char *name, uint32_t nameSize, uint32_t *id, qboolean required ) {
	lua_getfield( L, table, key );
	if ( lua_isnumber( L, -1 ) ) {
		float value = (float)lua_tonumber( L, -1 );
		if ( value <= 0.0f || value > 4294967295.0f || value != (float)(uint32_t)value ) {
			lua_pop( L, 1 ); return qfalse;
		}
		*id = (uint32_t)value;
		if ( name && nameSize ) snprintf( name, nameSize, "#%u", *id );
	} else if ( lua_isstring( L, -1 ) ) {
		const char *value = lua_tostring( L, -1 );
		if ( !value || !*value ) { lua_pop( L, 1 ); return qfalse; }
		*id = WiredFxAuthoring_Hash( value );
		if ( name && nameSize ) { strncpy( name, value, nameSize - 1u ); name[nameSize - 1u] = '\0'; }
	} else if ( !required && lua_isnil( L, -1 ) ) {
		*id = 0u;
		if ( name && nameSize ) name[0] = '\0';
	} else { lua_pop( L, 1 ); return qfalse; }
	lua_pop( L, 1 );
	return qtrue;
}

static qboolean WiredFxAuthoring_Ref( lua_State *L, int table, const char *key,
	char *out, uint32_t outSize ) {
	lua_getfield( L, table, key );
	if ( lua_isnil( L, -1 ) ) out[0] = '\0';
	else if ( lua_isstring( L, -1 ) ) {
		const char *value = lua_tostring( L, -1 );
		strncpy( out, value ? value : "", outSize - 1u ); out[outSize - 1u] = '\0';
	} else if ( lua_isnumber( L, -1 ) ) {
		float value = (float)lua_tonumber( L, -1 );
		if ( value < 1.0f || value > (float)WIRED_FX_MAX_ACTIONS || value != (float)(uint32_t)value ) {
			lua_pop( L, 1 ); return qfalse;
		}
		snprintf( out, outSize, "@%u", (uint32_t)value - 1u );
	} else { lua_pop( L, 1 ); return qfalse; }
	lua_pop( L, 1 );
	return qtrue;
}

static qboolean WiredFxAuthoring_Resource( lua_State *L, int table, const char *key,
	wiredFxResourceType_t type, const wiredFxAuthoringResolver_t *resolver, uint32_t *out ) {
	lua_getfield( L, table, key );
	if ( lua_isnumber( L, -1 ) ) {
		float value = (float)lua_tonumber( L, -1 );
		if ( value < 0.0f || value > 4294967295.0f || value != (float)(uint32_t)value ) {
			lua_pop( L, 1 ); return qfalse;
		}
		*out = (uint32_t)value;
	} else if ( lua_isstring( L, -1 ) && resolver && resolver->resolveResource ) {
		*out = resolver->resolveResource( type, lua_tostring( L, -1 ), resolver->userData );
	} else if ( lua_isnil( L, -1 ) ) *out = 0u;
	else { lua_pop( L, 1 ); return qfalse; }
	lua_pop( L, 1 );
	return qtrue;
}

static int WiredFxAuthoring_ActionType( const char *name ) {
	static const char *const names[WIRED_FX_ACTION_COUNT] = {
		"light", "particle", "decal", "decal2", "model", "sound", "screenShake",
		"controllerShake", "wind", "renderParm", "envOverride", "envChange", "flare",
		"radialBlur", "ribbon", "fadeParent", "godray"
	};
	uint32_t i;
	for ( i = 0u; i < WIRED_FX_ACTION_COUNT; ++i )
		if ( name && strcmp( name, names[i] ) == 0 ) return (int)i;
	return -1;
}

static uint32_t WiredFxAuthoring_Flag( const char *name ) {
	if ( !strcmp( name, "restart" ) ) return WIRED_FX_ACTION_RESTART;
	if ( !strcmp( name, "loop" ) ) return WIRED_FX_ACTION_LOOP;
	if ( !strcmp( name, "noShadows" ) ) return WIRED_FX_ACTION_NO_SHADOWS;
	if ( !strcmp( name, "filterHighViolence" ) ) return WIRED_FX_ACTION_FILTER_HIGH_VIOLENCE;
	if ( !strcmp( name, "optionalResource" ) ) return WIRED_FX_ACTION_OPTIONAL_RESOURCE;
	if ( !strcmp( name, "trackVelocity" ) ) return WIRED_FX_ACTION_TRACK_VELOCITY;
	if ( !strcmp( name, "gpuLifecycle" ) ) return WIRED_FX_ACTION_USE_GPU_LIFECYCLE;
	return 0u;
}

static qboolean WiredFxAuthoring_Flags( lua_State *L, int table, uint32_t *out ) {
	int count, i;
	*out = 0u;
	lua_getfield( L, table, "flags" );
	if ( lua_isnil( L, -1 ) ) { lua_pop( L, 1 ); return qtrue; }
	if ( lua_isnumber( L, -1 ) ) {
		float value = (float)lua_tonumber( L, -1 );
		if ( value < 0.0f || value > 4294967295.0f || value != (float)(uint32_t)value ) {
			lua_pop( L, 1 ); return qfalse;
		}
		*out = (uint32_t)value; lua_pop( L, 1 ); return qtrue;
	}
	if ( !lua_istable( L, -1 ) ) { lua_pop( L, 1 ); return qfalse; }
	count = (int)lua_objlen( L, -1 );
	for ( i = 1; i <= count; ++i ) {
		uint32_t flag;
		lua_rawgeti( L, -1, i );
		if ( !lua_isstring( L, -1 ) || !( flag = WiredFxAuthoring_Flag( lua_tostring( L, -1 ) ) ) ) {
			lua_pop( L, 2 ); return qfalse;
		}
		*out |= flag; lua_pop( L, 1 );
	}
	lua_pop( L, 1 );
	return qtrue;
}

static int WiredFxAuthoring_Origin( const char *name ) {
	static const char *const names[] = { "start", "track", "trackLocal", "external" };
	uint32_t i; for ( i = 0u; i < ARRAY_LEN( names ); ++i ) if ( !strcmp( name, names[i] ) ) return (int)i;
	return -1;
}

static int WiredFxAuthoring_Rotation( const char *name ) {
	static const char *const names[] = { "startAxis", "startAxisParent", "trackAxis", "trackAxisParent",
		"trackLocalAxis", "angles", "curves", "curvesLocal", "external" };
	uint32_t i; for ( i = 0u; i < ARRAY_LEN( names ); ++i ) if ( !strcmp( name, names[i] ) ) return (int)i;
	return -1;
}

static qboolean WiredFxAuthoring_EnumField( lua_State *L, int table, const char *key,
	int defaultValue, int ( *decode )( const char * ), uint32_t *out ) {
	lua_getfield( L, table, key );
	if ( lua_isnil( L, -1 ) ) *out = (uint32_t)defaultValue;
	else if ( lua_isstring( L, -1 ) ) {
		int value = decode( lua_tostring( L, -1 ) );
		if ( value < 0 ) { lua_pop( L, 1 ); return qfalse; }
		*out = (uint32_t)value;
	} else { lua_pop( L, 1 ); return qfalse; }
	lua_pop( L, 1 ); return qtrue;
}

static qboolean WiredFxAuthoring_Common( lua_State *L, int table, wiredFxAction_t *action,
	wiredFxAuthoringRefs_t *refs ) {
	static const float white[4] = { 1, 1, 1, 1 };
	static const float defaultDelay[2] = { 0, 0 };
	if ( !WiredFxAuthoring_Id( L, table, "id", refs->id, sizeof( refs->id ), &action->actionId, qtrue ) ||
		 !WiredFxAuthoring_U32( L, table, "group", 0u, &action->groupId ) ||
		 !WiredFxAuthoring_Ref( L, table, "parent", refs->parent, sizeof( refs->parent ) ) ||
		 !WiredFxAuthoring_Ref( L, table, "fire", refs->fire, sizeof( refs->fire ) ) ||
		 !WiredFxAuthoring_Flags( L, table, &action->flags ) ||
		 !WiredFxAuthoring_EnumField( L, table, "origin", WIRED_FX_ORIGIN_START, WiredFxAuthoring_Origin, &action->originType ) ||
		 !WiredFxAuthoring_EnumField( L, table, "rotation", WIRED_FX_ROTATION_START_AXIS, WiredFxAuthoring_Rotation, &action->rotationType ) ||
		 !WiredFxAuthoring_U32( L, table, "maxInstances", 1u, &action->maxInstances ) ||
		 !WiredFxAuthoring_Vector( L, table, "delay", action->delay, 2u, defaultDelay ) ||
		 !WiredFxAuthoring_Number( L, table, "duration", 0.0f, &action->duration ) ||
		 !WiredFxAuthoring_Number( L, table, "fadeIn", 0.0f, &action->fadeInTime ) ||
		 !WiredFxAuthoring_Number( L, table, "fadeOut", 0.0f, &action->fadeOutTime ) ||
		 !WiredFxAuthoring_Number( L, table, "lodNear", 0.0f, &action->lodNear ) ||
		 !WiredFxAuthoring_Number( L, table, "lodFar", FLT_MAX, &action->lodFar ) ||
		 !WiredFxAuthoring_Number( L, table, "boundsRadius", 0.0f, &action->boundsRadius ) ||
		 !WiredFxAuthoring_Vector( L, table, "offset", action->offset, 3u, NULL ) ||
		 !WiredFxAuthoring_Vector( L, table, "angles", action->rotationDegrees, 3u, NULL ) ||
		 !WiredFxAuthoring_Vector( L, table, "color", action->color, 4u, white ) ||
		 !WiredFxAuthoring_U64( L, table, "startCondition", &action->startConditionMask ) ||
		 !WiredFxAuthoring_U64( L, table, "stopCondition", &action->stopConditionMask ) ||
		 !WiredFxAuthoring_U64( L, table, "extraCondition", &action->extraConditionMask ) ) return qfalse;
	action->parentAction = WIRED_FX_NO_ACTION;
	action->fireAction = WIRED_FX_NO_ACTION;
	return qtrue;
}

#define FX_NUM( key, def, dst ) WiredFxAuthoring_Number( L, table, key, def, dst )
#define FX_U32( key, def, dst ) WiredFxAuthoring_U32( L, table, key, def, dst )
#define FX_VEC( key, dst, n, def ) WiredFxAuthoring_Vector( L, table, key, dst, n, def )
#define FX_RES( key, kind, dst ) WiredFxAuthoring_Resource( L, table, key, kind, resolver, dst )

static qboolean WiredFxAuthoring_Payload( lua_State *L, int table, wiredFxAction_t *a,
	const wiredFxAuthoringResolver_t *resolver ) {
	static const float one3[3] = { 1, 1, 1 }, white[4] = { 1, 1, 1, 1 };
	switch ( (wiredFxActionType_t)a->type ) {
	case WIRED_FX_ACTION_LIGHT:
		return FX_RES( "material", WIRED_FX_RESOURCE_MATERIAL, &a->payload.light.material ) &&
			FX_VEC( "radius", a->payload.light.radius, 3u, one3 ) && FX_NUM( "intensity", 1, &a->payload.light.intensity );
	case WIRED_FX_ACTION_PARTICLE:
		return FX_RES( "particle", WIRED_FX_RESOURCE_PARTICLE_CLASS, &a->payload.particle.particleClass ) &&
			FX_U32( "maxParticles", a->maxInstances, &a->payload.particle.maxParticles ) &&
			FX_NUM( "velocityScale", 1, &a->payload.particle.velocityScale ) &&
			FX_NUM( "minVelocity", 0, &a->payload.particle.minVelocity ) &&
			FX_NUM( "spawnRate", 0, &a->payload.particle.spawnRate ) &&
			FX_NUM( "trailSpacing", 0, &a->payload.particle.trailSpacing ) &&
			FX_NUM( "screenExcludeAngle", 0, &a->payload.particle.screenExcludeAngle );
	case WIRED_FX_ACTION_DECAL: case WIRED_FX_ACTION_DECAL2:
		return FX_RES( "material", WIRED_FX_RESOURCE_MATERIAL, &a->payload.decal.material ) &&
			FX_NUM( "angle", 0, &a->payload.decal.angle ) && FX_NUM( "depth", 1, &a->payload.decal.depth ) &&
			FX_NUM( "size", 1, &a->payload.decal.size );
	case WIRED_FX_ACTION_MODEL:
		return FX_RES( "model", WIRED_FX_RESOURCE_MODEL, &a->payload.model.model ) &&
			FX_RES( "material", WIRED_FX_RESOURCE_MATERIAL, &a->payload.model.material );
	case WIRED_FX_ACTION_SOUND:
		return FX_RES( "sound", WIRED_FX_RESOURCE_SOUND, &a->payload.sound.sound ) &&
			FX_U32( "channel", 0u, (uint32_t *)&a->payload.sound.channel );
	case WIRED_FX_ACTION_SCREEN_SHAKE:
		return FX_NUM( "magnitude", 0, &a->payload.screenShake.magnitude ) &&
			FX_NUM( "controllerScale", 0, &a->payload.screenShake.controllerScale ) &&
			FX_VEC( "maxAngles", a->payload.screenShake.maxAngles, 3u, NULL ) &&
			FX_VEC( "maxOffset", a->payload.screenShake.maxOffset, 3u, NULL );
	case WIRED_FX_ACTION_CONTROLLER_SHAKE:
		return FX_NUM( "highMagnitude", 0, &a->payload.controllerShake.highMagnitude ) &&
			FX_NUM( "lowMagnitude", 0, &a->payload.controllerShake.lowMagnitude ) &&
			FX_NUM( "highDuration", 0, &a->payload.controllerShake.highDuration ) &&
			FX_NUM( "lowDuration", 0, &a->payload.controllerShake.lowDuration ) &&
			FX_RES( "amplitudeCurve", WIRED_FX_RESOURCE_CURVE, &a->payload.controllerShake.amplitudeCurve );
	case WIRED_FX_ACTION_WIND:
		return FX_NUM( "angle", 0, &a->payload.wind.angle ) && FX_NUM( "multiplier", 1, &a->payload.wind.multiplier ) &&
			FX_VEC( "strength", a->payload.wind.strength, 2u, NULL );
	case WIRED_FX_ACTION_RENDER_PARM:
		return FX_RES( "parm", WIRED_FX_RESOURCE_RENDER_PARM, &a->payload.renderParm.handle ) &&
			FX_VEC( "value", a->payload.renderParm.value, 4u, NULL );
	case WIRED_FX_ACTION_ENV_OVERRIDE: case WIRED_FX_ACTION_ENV_CHANGE: {
		int i, count;
		if ( !FX_RES( "environment", WIRED_FX_RESOURCE_ENVIRONMENT, &a->payload.environment.environment ) ) return qfalse;
		lua_getfield( L, table, "renderParms" );
		if ( lua_isnil( L, -1 ) ) { lua_pop( L, 1 ); return qtrue; }
		if ( !lua_istable( L, -1 ) ) { lua_pop( L, 1 ); return qfalse; }
		count = (int)lua_objlen( L, -1 );
		if ( count > (int)WIRED_FX_MAX_ENV_RENDER_PARMS ) { lua_pop( L, 1 ); return qfalse; }
		for ( i = 1; i <= count; ++i ) {
			int parm;
			lua_rawgeti( L, -1, i ); if ( !lua_istable( L, -1 ) ) { lua_pop( L, 2 ); return qfalse; }
			parm = lua_gettop( L );
			if ( !WiredFxAuthoring_Resource( L, parm, "parm", WIRED_FX_RESOURCE_RENDER_PARM, resolver,
				&a->payload.environment.renderParms[i - 1].handle ) ||
				 !WiredFxAuthoring_Vector( L, parm, "value", a->payload.environment.renderParms[i - 1].value, 4u, NULL ) ) {
				lua_pop( L, 2 ); return qfalse;
			}
			lua_pop( L, 1 );
		}
		a->payload.environment.renderParmCount = (uint32_t)count; lua_pop( L, 1 ); return qtrue;
	}
	case WIRED_FX_ACTION_FLARE:
		return FX_RES( "flare", WIRED_FX_RESOURCE_FLARE, &a->payload.flare.flare ) &&
			FX_VEC( "position", a->payload.flare.position, 3u, NULL ) &&
			FX_U32( "autosprite", 0, &a->payload.flare.autosprite );
	case WIRED_FX_ACTION_RADIAL_BLUR: return FX_NUM( "maxScale", 0, &a->payload.radialBlur.maxScale );
	case WIRED_FX_ACTION_RIBBON: return FX_RES( "ribbon", WIRED_FX_RESOURCE_RIBBON, &a->payload.ribbon.ribbon );
	case WIRED_FX_ACTION_FADE_PARENT: return qtrue;
	case WIRED_FX_ACTION_GODRAY:
		return FX_RES( "material", WIRED_FX_RESOURCE_MATERIAL, &a->payload.godray.material ) &&
			FX_VEC( "godrayColor", a->payload.godray.color, 4u, white ) &&
			FX_NUM( "colorScale", 1, &a->payload.godray.colorScale ) &&
			FX_U32( "size", 1, &a->payload.godray.size ) && FX_U32( "sourceSize", 1, &a->payload.godray.sourceSize );
	default: return qfalse;
	}
}

static qboolean WiredFxAuthoring_ResolveRef( const char *ref, wiredFxAuthoringRefs_t *refs,
	uint32_t count, uint32_t *out ) {
	uint32_t i;
	if ( !ref[0] ) { *out = WIRED_FX_NO_ACTION; return qtrue; }
	if ( ref[0] == '@' ) {
		unsigned long index = strtoul( ref + 1, NULL, 10 );
		if ( index >= count ) return qfalse;
		*out = (uint32_t)index; return qtrue;
	}
	for ( i = 0u; i < count; ++i ) if ( !strcmp( ref, refs[i].id ) ) { *out = i; return qtrue; }
	return qfalse;
}

qboolean WiredFxAuthoring_ReadProfile( wiredFxProfile_t *out, lua_State *L,
	int tableIndex, const wiredFxAuthoringResolver_t *resolver, char *error, uint32_t errorSize ) {
	wiredFxAuthoringRefs_t refs[WIRED_FX_MAX_ACTIONS];
	int table, actions, count, i;
	if ( error && errorSize ) error[0] = '\0';
	if ( !out || !L ) return qfalse;
	memset( out, 0, sizeof( *out ) ); memset( refs, 0, sizeof( refs ) );
	table = WiredFxAuthoring_AbsIndex( L, tableIndex );
	if ( !lua_istable( L, table ) ) { WiredFxAuthoring_Error( error, errorSize, "profile: expected table" ); return qfalse; }
	out->schemaVersion = WIRED_FX_PROFILE_SCHEMA_VERSION;
	if ( !FX_U32( "schemaVersion", WIRED_FX_PROFILE_SCHEMA_VERSION, &out->schemaVersion ) ||
		 !FX_U32( "maxActiveActions", 1u, &out->maxActiveActions ) ||
		 !FX_U32( "maxInstances", 1u, &out->maxInstances ) ||
		 !FX_U32( "parentProfile", 0u, &out->parentProfile ) ||
		 !FX_U32( "actionOverrideMask", 0u, &out->actionOverrideMask ) ||
		 !FX_U32( "seed", 0u, &out->seed ) || FX_NUM( "duration", 0, &out->duration ) == qfalse ||
		 FX_NUM( "lodNear", 0, &out->lodNear ) == qfalse || FX_NUM( "lodFar", FLT_MAX, &out->lodFar ) == qfalse ||
		 FX_NUM( "boundsRadius", 0, &out->boundsRadius ) == qfalse ) {
		WiredFxAuthoring_Error( error, errorSize, "profile: malformed common field" ); return qfalse;
	}
	lua_getfield( L, table, "actions" ); actions = lua_gettop( L );
	if ( !lua_istable( L, actions ) ) { lua_pop( L, 1 ); WiredFxAuthoring_Error( error, errorSize, "profile.actions: expected array" ); return qfalse; }
	count = (int)lua_objlen( L, actions );
	if ( count <= 0 || count > (int)WIRED_FX_MAX_ACTIONS ) { lua_pop( L, 1 ); WiredFxAuthoring_Error( error, errorSize, "profile.actions: invalid count" ); return qfalse; }
	out->actionCount = (uint32_t)count;
	for ( i = 0; i < count; ++i ) {
		wiredFxAction_t *action = &out->actions[i];
		const char *typeName;
		int actionTable, type;
		lua_rawgeti( L, actions, i + 1 ); actionTable = lua_gettop( L );
		if ( !lua_istable( L, actionTable ) ) { lua_pop( L, 2 ); WiredFxAuthoring_Error( error, errorSize, "actions[%d]: expected table", i + 1 ); goto fail; }
		lua_getfield( L, actionTable, "type" ); typeName = lua_isstring( L, -1 ) ? lua_tostring( L, -1 ) : NULL;
		type = WiredFxAuthoring_ActionType( typeName ); lua_pop( L, 1 );
		if ( type < 0 ) {
			lua_pop( L, 2 ); WiredFxAuthoring_Error( error, errorSize,
				"actions[%d]: unknown action type", i + 1 ); goto fail;
		}
		action->type = (uint32_t)type;
		if ( !WiredFxAuthoring_Common( L, actionTable, action, &refs[i] ) ||
			 !WiredFxAuthoring_Payload( L, actionTable, action, resolver ) ) {
			lua_pop( L, 2 ); WiredFxAuthoring_Error( error, errorSize, "actions[%d]: malformed %s action", i + 1, typeName ? typeName : "unknown" ); goto fail;
		}
		lua_pop( L, 1 );
	}
	lua_pop( L, 1 );
	for ( i = 0; i < count; ++i ) {
		if ( !WiredFxAuthoring_ResolveRef( refs[i].parent, refs, (uint32_t)count, &out->actions[i].parentAction ) ||
			 !WiredFxAuthoring_ResolveRef( refs[i].fire, refs, (uint32_t)count, &out->actions[i].fireAction ) ) {
			WiredFxAuthoring_Error( error, errorSize, "actions[%d]: unresolved parent/fire reference", i + 1 ); goto fail;
		}
	}
	return qtrue;
fail:
	memset( out, 0, sizeof( *out ) ); return qfalse;
}

#undef FX_NUM
#undef FX_U32
#undef FX_VEC
#undef FX_RES
