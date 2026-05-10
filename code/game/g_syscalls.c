/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

#include "g_local.h"
#include "bg_public.h"
#include "inv.h"
#include "../qcommon/q_feats.h"
#if FEAT_RECAST_NAVMESH
#include "../qcommon/nav/nav_types.h"
#endif
#ifndef MAX_STRINGFIELD
#define MAX_STRINGFIELD 80
#endif
#include "../botlib/be_ai_weap.h"
/* Phase 5: log channels */
LOG_DECLARE_CHANNEL( ch_game, "game" );

#ifdef WASM_MODULE
extern intptr_t QDECL syscall( intptr_t arg, ... );
#else
static intptr_t (QDECL *syscall)( intptr_t arg, ... ) = (intptr_t (QDECL *)( intptr_t, ...))-1;


Q_EXPORT void dllEntry( intptr_t (QDECL *syscallptr)( intptr_t arg,... ) ) {
	syscall = syscallptr;
}
#endif

int PASSFLOAT( float x ) {
	floatint_t fi;
	fi.f = x;
	return fi.i;
}

static qboolean Trap_ParseLuaCharacterName( const char *charfile, char *out, int outSize ) {
	const char *prefix = "characters/";
	const char *start;
	const char *slash;

	if ( !charfile || !out || outSize <= 0 ) {
		return qfalse;
	}

	if ( Q_stricmpn( charfile, prefix, strlen( prefix ) ) ) {
		return qfalse;
	}

	start = charfile + strlen( prefix );
	slash = strchr( start, '/' );
	if ( !slash || slash == start ) {
		return qfalse;
	}

	Q_strncpyz( out, start, outSize );
	out[ slash - start ] = '\0';

	if ( !out[0] ) {
		return qfalse;
	}

	return qtrue;
}

void	trap_Print( const char *text ) {
	syscall( G_PRINT, text );
}

void trap_Error( const char *text )
{
	syscall( G_ERROR, text );
	// shut up GCC warning about returning functions, because we know better
	exit(1);
}

void trap_Log( log_severity_t severity, const char *text ) {
	syscall( G_LOG, (int)severity, text );
}

void trap_Terminate( terminationReason_t reason, const char *text ) {
	syscall( G_TERMINATE, (int)reason, text );
	exit(1);
}

int		trap_Milliseconds( void ) {
	return syscall( G_MILLISECONDS ); 
}
int		trap_Argc( void ) {
	return syscall( G_ARGC );
}

void	trap_Argv( int n, char *buffer, int bufferLength ) {
	syscall( G_ARGV, n, buffer, bufferLength );
}

int		trap_FS_FOpenFile( const char *qpath, fileHandle_t *f, fsMode_t mode ) {
	return syscall( G_FS_FOPEN_FILE, qpath, f, mode );
}

void	trap_FS_Read( void *buffer, int len, fileHandle_t f ) {
	syscall( G_FS_READ, buffer, len, f );
}

void	trap_FS_Write( const void *buffer, int len, fileHandle_t f ) {
	syscall( G_FS_WRITE, buffer, len, f );
}

void	trap_FS_FCloseFile( fileHandle_t f ) {
	syscall( G_FS_FCLOSE_FILE, f );
}

int trap_FS_GetFileList(  const char *path, const char *extension, char *listbuf, int bufsize ) {
	return syscall( G_FS_GETFILELIST, path, extension, listbuf, bufsize );
}

int trap_FS_Seek( fileHandle_t f, long offset, int origin ) {
	return syscall( G_FS_SEEK, f, offset, origin );
}

void	trap_SendConsoleCommand( int exec_when, const char *text ) {
	syscall( G_SEND_CONSOLE_COMMAND, exec_when, text );
}

void	trap_Cvar_Register( vmCvar_t *cvar, const char *var_name, const char *value, int flags ) {
	syscall( G_CVAR_REGISTER, cvar, var_name, value, flags );
}

void	trap_Cvar_Update( vmCvar_t *cvar ) {
	syscall( G_CVAR_UPDATE, cvar );
}

void trap_Cvar_Set( const char *var_name, const char *value ) {
	syscall( G_CVAR_SET, var_name, value );
}

int trap_Cvar_VariableIntegerValue( const char *var_name ) {
	return syscall( G_CVAR_VARIABLE_INTEGER_VALUE, var_name );
}

void trap_Cvar_VariableStringBuffer( const char *var_name, char *buffer, int bufsize ) {
	syscall( G_CVAR_VARIABLE_STRING_BUFFER, var_name, buffer, bufsize );
}


void trap_LocateGameData( gentity_t *gEnts, int numGEntities, int sizeofGEntity_t,
						 playerState_t *clients, int sizeofGClient ) {
	syscall( G_LOCATE_GAME_DATA, gEnts, numGEntities, sizeofGEntity_t, clients, sizeofGClient );
}

void trap_DropClient( int clientNum, const char *reason ) {
	syscall( G_DROP_CLIENT, clientNum, reason );
}

void trap_SendServerCommand( int clientNum, const char *text ) {
	syscall( G_SEND_SERVER_COMMAND, clientNum, text );
}

void trap_SetConfigstring( int num, const char *string ) {
	syscall( G_SET_CONFIGSTRING, num, string );
}

void trap_GetConfigstring( int num, char *buffer, int bufferSize ) {
	syscall( G_GET_CONFIGSTRING, num, buffer, bufferSize );
}

void trap_GetUserinfo( int num, char *buffer, int bufferSize ) {
	syscall( G_GET_USERINFO, num, buffer, bufferSize );
}

void trap_SetUserinfo( int num, const char *buffer ) {
	syscall( G_SET_USERINFO, num, buffer );
}

void trap_GetServerinfo( char *buffer, int bufferSize ) {
	syscall( G_GET_SERVERINFO, buffer, bufferSize );
}

void trap_SetBrushModel( gentity_t *ent, const char *name ) {
	syscall( G_SET_BRUSH_MODEL, ent, name );
}

void trap_Trace( trace_t *results, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int passEntityNum, int contentmask ) {
	syscall( G_TRACE, results, start, mins, maxs, end, passEntityNum, contentmask );
}

void trap_TraceCapsule( trace_t *results, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int passEntityNum, int contentmask ) {
	syscall( G_TRACECAPSULE, results, start, mins, maxs, end, passEntityNum, contentmask );
}

int trap_PointContents( const vec3_t point, int passEntityNum ) {
	return syscall( G_POINT_CONTENTS, point, passEntityNum );
}


qboolean trap_InPVS( const vec3_t p1, const vec3_t p2 ) {
	return syscall( G_IN_PVS, p1, p2 );
}

qboolean trap_InPVSIgnorePortals( const vec3_t p1, const vec3_t p2 ) {
	return syscall( G_IN_PVS_IGNORE_PORTALS, p1, p2 );
}

void trap_AdjustAreaPortalState( gentity_t *ent, qboolean open ) {
	syscall( G_ADJUST_AREA_PORTAL_STATE, ent, open );
}

qboolean trap_AreasConnected( int area1, int area2 ) {
	return syscall( G_AREAS_CONNECTED, area1, area2 );
}

void trap_LinkEntity( gentity_t *ent ) {
	syscall( G_LINKENTITY, ent );
}

void trap_UnlinkEntity( gentity_t *ent ) {
	syscall( G_UNLINKENTITY, ent );
}

int trap_EntitiesInBox( const vec3_t mins, const vec3_t maxs, int *list, int maxcount ) {
	return syscall( G_ENTITIES_IN_BOX, mins, maxs, list, maxcount );
}

qboolean trap_EntityContact( const vec3_t mins, const vec3_t maxs, const gentity_t *ent ) {
	return syscall( G_ENTITY_CONTACT, mins, maxs, ent );
}

qboolean trap_EntityContactCapsule( const vec3_t mins, const vec3_t maxs, const gentity_t *ent ) {
	return syscall( G_ENTITY_CONTACTCAPSULE, mins, maxs, ent );
}

int trap_BotAllocateClient( void ) {
	return syscall( G_BOT_ALLOCATE_CLIENT );
}

void trap_BotFreeClient( int clientNum ) {
	syscall( G_BOT_FREE_CLIENT, clientNum );
}

void trap_GetUsercmd( int clientNum, usercmd_t *cmd ) {
	syscall( G_GET_USERCMD, clientNum, cmd );
}

qboolean trap_GetEntityToken( char *buffer, int bufferSize ) {
	return syscall( G_GET_ENTITY_TOKEN, buffer, bufferSize );
}

int trap_DebugPolygonCreate(int color, int numPoints, vec3_t *points) {
	return syscall( G_DEBUG_POLYGON_CREATE, color, numPoints, points );
}

void trap_DebugPolygonDelete(int id) {
	syscall( G_DEBUG_POLYGON_DELETE, id );
}

int trap_RealTime( qtime_t *qtime ) {
	return syscall( G_REAL_TIME, qtime );
}

void trap_SnapVector( float *v ) {
	syscall( G_SNAPVECTOR, v );
}

// BotLib traps start here
int trap_BotLibSetup( void ) {
	return syscall( BOTLIB_SETUP );
}

int trap_BotLibShutdown( void ) {
	return syscall( BOTLIB_SHUTDOWN );
}

int trap_BotLibVarSet(char *var_name, char *value) {
	return syscall( BOTLIB_LIBVAR_SET, var_name, value );
}

int trap_BotLibVarGet(char *var_name, char *value, int size) {
	return syscall( BOTLIB_LIBVAR_GET, var_name, value, size );
}

int trap_BotLibDefine(char *string) {
	return syscall( BOTLIB_PC_ADD_GLOBAL_DEFINE, string );
}

int trap_BotLibStartFrame(float time) {
	return syscall( BOTLIB_START_FRAME, PASSFLOAT( time ) );
}

int trap_BotLibLoadMap(const char *mapname) {
	return syscall( BOTLIB_LOAD_MAP, mapname );
}

int trap_BotLibUpdateEntity(int ent, void /* struct bot_updateentity_s */ *bue) {
	return syscall( BOTLIB_UPDATENTITY, ent, bue );
}

int trap_BotLibTest(int parm0, char *parm1, vec3_t parm2, vec3_t parm3) {
	return syscall( BOTLIB_TEST, parm0, parm1, parm2, parm3 );
}

int trap_BotGetSnapshotEntity( int clientNum, int sequence ) {
	return syscall( BOTLIB_GET_SNAPSHOT_ENTITY, clientNum, sequence );
}

int trap_BotGetServerCommand(int clientNum, char *message, int size) {
	return syscall( BOTLIB_GET_CONSOLE_MESSAGE, clientNum, message, size );
}

void trap_BotUserCommand(int clientNum, usercmd_t *ucmd) {
	syscall( BOTLIB_USER_COMMAND, clientNum, ucmd );
}

void trap_AAS_EntityInfo(int entnum, void /* struct aas_entityinfo_s */ *info) {
	syscall( BOTLIB_AAS_ENTITY_INFO, entnum, info );
}

int trap_AAS_Initialized(void) {
	return syscall( BOTLIB_AAS_INITIALIZED );
}

void trap_AAS_PresenceTypeBoundingBox(int presencetype, vec3_t mins, vec3_t maxs) {
	syscall( BOTLIB_AAS_PRESENCE_TYPE_BOUNDING_BOX, presencetype, mins, maxs );
}

float trap_AAS_Time(void) {
	floatint_t fi;
	fi.i = syscall( BOTLIB_AAS_TIME );
	return fi.f;
}

int trap_AAS_PointAreaNum(vec3_t point) {
	return syscall( BOTLIB_AAS_POINT_AREA_NUM, point );
}

int trap_AAS_PointReachabilityAreaIndex(vec3_t point) {
	return syscall( BOTLIB_AAS_POINT_REACHABILITY_AREA_INDEX, point );
}

int trap_AAS_TraceAreas(vec3_t start, vec3_t end, int *areas, vec3_t *points, int maxareas) {
	return syscall( BOTLIB_AAS_TRACE_AREAS, start, end, areas, points, maxareas );
}

int trap_AAS_BBoxAreas(vec3_t absmins, vec3_t absmaxs, int *areas, int maxareas) {
	return syscall( BOTLIB_AAS_BBOX_AREAS, absmins, absmaxs, areas, maxareas );
}

int trap_AAS_AreaInfo( int areanum, void /* struct aas_areainfo_s */ *info ) {
	return syscall( BOTLIB_AAS_AREA_INFO, areanum, info );
}

int trap_AAS_PointContents(vec3_t point) {
	return syscall( BOTLIB_AAS_POINT_CONTENTS, point );
}

int trap_AAS_NextBSPEntity(int ent) {
	return syscall( BOTLIB_AAS_NEXT_BSP_ENTITY, ent );
}

int trap_AAS_ValueForBSPEpairKey(int ent, char *key, char *value, int size) {
	return syscall( BOTLIB_AAS_VALUE_FOR_BSP_EPAIR_KEY, ent, key, value, size );
}

int trap_AAS_VectorForBSPEpairKey(int ent, char *key, vec3_t v) {
	return syscall( BOTLIB_AAS_VECTOR_FOR_BSP_EPAIR_KEY, ent, key, v );
}

int trap_AAS_FloatForBSPEpairKey(int ent, char *key, float *value) {
	return syscall( BOTLIB_AAS_FLOAT_FOR_BSP_EPAIR_KEY, ent, key, value );
}

int trap_AAS_IntForBSPEpairKey(int ent, char *key, int *value) {
	return syscall( BOTLIB_AAS_INT_FOR_BSP_EPAIR_KEY, ent, key, value );
}

int trap_AAS_AreaReachability(int areanum) {
	return syscall( BOTLIB_AAS_AREA_REACHABILITY, areanum );
}

int trap_AAS_AreaTravelTimeToGoalArea(int areanum, vec3_t origin, int goalareanum, int travelflags) {
	return syscall( BOTLIB_AAS_AREA_TRAVEL_TIME_TO_GOAL_AREA, areanum, origin, goalareanum, travelflags );
}

int trap_AAS_EnableRoutingArea( int areanum, int enable ) {
	return syscall( BOTLIB_AAS_ENABLE_ROUTING_AREA, areanum, enable );
}

int trap_AAS_PredictRoute(void /*struct aas_predictroute_s*/ *route, int areanum, vec3_t origin,
							int goalareanum, int travelflags, int maxareas, int maxtime,
							int stopevent, int stopcontents, int stoptfl, int stopareanum) {
	return syscall( BOTLIB_AAS_PREDICT_ROUTE, route, areanum, origin, goalareanum, travelflags, maxareas, maxtime, stopevent, stopcontents, stoptfl, stopareanum );
}

int trap_AAS_AlternativeRouteGoals(vec3_t start, int startareanum, vec3_t goal, int goalareanum, int travelflags,
										void /*struct aas_altroutegoal_s*/ *altroutegoals, int maxaltroutegoals,
										int type) {
	return syscall( BOTLIB_AAS_ALTERNATIVE_ROUTE_GOAL, start, startareanum, goal, goalareanum, travelflags, altroutegoals, maxaltroutegoals, type );
}

int trap_AAS_Swimming(vec3_t origin) {
	return syscall( BOTLIB_AAS_SWIMMING, origin );
}

int trap_AAS_PredictClientMovement(void /* struct aas_clientmove_s */ *move, int entnum, vec3_t origin, int presencetype, int onground, vec3_t velocity, vec3_t cmdmove, int cmdframes, int maxframes, float frametime, int stopevent, int stopareanum, int visualize) {
	return syscall( BOTLIB_AAS_PREDICT_CLIENT_MOVEMENT, move, entnum, origin, presencetype, onground, velocity, cmdmove, cmdframes, maxframes, PASSFLOAT(frametime), stopevent, stopareanum, visualize );
}

void trap_EA_Say(int client, char *str) {
	syscall( BOTLIB_EA_SAY, client, str );
}

void trap_EA_SayTeam(int client, char *str) {
	syscall( BOTLIB_EA_SAY_TEAM, client, str );
}

void trap_EA_Command(int client, char *command) {
	syscall( BOTLIB_EA_COMMAND, client, command );
}

void trap_EA_Action(int client, int action) {
	syscall( BOTLIB_EA_ACTION, client, action );
}

void trap_EA_Gesture(int client) {
	syscall( BOTLIB_EA_GESTURE, client );
}

void trap_EA_Talk(int client) {
	syscall( BOTLIB_EA_TALK, client );
}

void trap_EA_Attack(int client) {
	syscall( BOTLIB_EA_ATTACK, client );
}

void trap_EA_Use(int client) {
	syscall( BOTLIB_EA_USE, client );
}

void trap_EA_Respawn(int client) {
	syscall( BOTLIB_EA_RESPAWN, client );
}

void trap_EA_Crouch(int client) {
	syscall( BOTLIB_EA_CROUCH, client );
}

void trap_EA_MoveUp(int client) {
	syscall( BOTLIB_EA_MOVE_UP, client );
}

void trap_EA_MoveDown(int client) {
	syscall( BOTLIB_EA_MOVE_DOWN, client );
}

void trap_EA_MoveForward(int client) {
	syscall( BOTLIB_EA_MOVE_FORWARD, client );
}

void trap_EA_MoveBack(int client) {
	syscall( BOTLIB_EA_MOVE_BACK, client );
}

void trap_EA_MoveLeft(int client) {
	syscall( BOTLIB_EA_MOVE_LEFT, client );
}

void trap_EA_MoveRight(int client) {
	syscall( BOTLIB_EA_MOVE_RIGHT, client );
}

void trap_EA_SelectWeapon(int client, int weapon) {
	syscall( BOTLIB_EA_SELECT_WEAPON, client, weapon );
}

void trap_EA_Jump(int client) {
	syscall( BOTLIB_EA_JUMP, client );
}

void trap_EA_DelayedJump(int client) {
	syscall( BOTLIB_EA_DELAYED_JUMP, client );
}

void trap_EA_Move(int client, vec3_t dir, float speed) {
	syscall( BOTLIB_EA_MOVE, client, dir, PASSFLOAT(speed) );
}

void trap_EA_View(int client, vec3_t viewangles) {
	syscall( BOTLIB_EA_VIEW, client, viewangles );
}

void trap_EA_EndRegular(int client, float thinktime) {
	syscall( BOTLIB_EA_END_REGULAR, client, PASSFLOAT(thinktime) );
}

void trap_EA_GetInput(int client, float thinktime, void /* struct bot_input_s */ *input) {
	syscall( BOTLIB_EA_GET_INPUT, client, PASSFLOAT(thinktime), input );
}

void trap_EA_ResetInput(int client) {
	syscall( BOTLIB_EA_RESET_INPUT, client );
}

int trap_BotLuaBindBot(int client, int characterHandle) {
	return syscall( WB_BIND_BOT, client, characterHandle );
}

int trap_BotLuaBotThink(int client, float thinktime) {
	return syscall( WB_BOT_THINK, client, PASSFLOAT(thinktime) );
}

float trap_BotLuaBotProfileField(int client, int field) {
	floatint_t fi;
	fi.i = syscall( WB_BOT_PROFILE_FIELD, client, field );
	return fi.f;
}

int trap_BotLuaBotPickWeapon(int client, const wbCombatCtx_t *ctx, char *weaponKey, int weaponKeySize) {
	return syscall( WB_BOT_PICK_WEAPON, client, ctx, weaponKey, weaponKeySize );
}

float trap_BotLuaBotGetAttackAimHeight(int client, int weaponNum) {
	floatint_t fi;
	fi.i = syscall( WB_BOT_GET_ATTACK_AIM_HEIGHT, client, weaponNum );
	return fi.f;
}

int trap_BotLuaBotEvalItem(int client, const wbItemEvalCtx_t *ctx) {
	return syscall( WB_BOT_EVAL_ITEM, client, ctx );
}

int trap_BotLuaBotDecide(int client, const wbDecideCtx_t *ctx, char *decision, int decisionSize) {
	return syscall( WB_BOT_DECIDE, client, ctx, decision, decisionSize );
}

int trap_BotLuaBotOnChat(int client, const char *eventName, const wbChatCtx_t *ctx, char *outChat, int outChatSize) {
	return syscall( WB_BOT_ON_CHAT, client, eventName, ctx, outChat, outChatSize );
}

static void Trap_BotFillWeaponInfoFromGame( int weapon, weaponinfo_t *weaponinfo ) {
	if ( !weaponinfo ) {
		return;
	}

	memset( weaponinfo, 0, sizeof( *weaponinfo ) );

	if ( weapon <= WP_NONE || weapon >= WP_NUM_WEAPONS ) {
		return;
	}

	weaponinfo->valid = qtrue;
	weaponinfo->number = weapon;
	weaponinfo->weaponindex = weapon;

	if ( bg_weaponlist[weapon].name ) {
		Q_strncpyz( weaponinfo->name, bg_weaponlist[weapon].name, sizeof( weaponinfo->name ) );
	}

	if ( bg_weaponlist[weapon].shortname ) {
		Q_strncpyz( weaponinfo->projectile, bg_weaponlist[weapon].shortname, sizeof( weaponinfo->projectile ) );
	}

	weaponinfo->flags = 0;
	weaponinfo->proj.damage = 0;
	weaponinfo->proj.radius = 0.0f;
	weaponinfo->proj.damagetype = 0;
	weaponinfo->speed = 0.0f;
	weaponinfo->reload = 0.1f;

	switch ( weapon ) {
		case WP_GAUNTLET:
			weaponinfo->reload = 0.4f;
			weaponinfo->proj.damage = 50;
			break;
		case WP_MACHINEGUN:
			weaponinfo->hspread = 250.0f;
			weaponinfo->vspread = 250.0f;
			weaponinfo->reload = 0.1f;
			weaponinfo->proj.damage = 8;
			break;
		case WP_SHOTGUN:
			weaponinfo->hspread = DEFAULT_SHOTGUN_SPREAD;
			weaponinfo->vspread = DEFAULT_SHOTGUN_SPREAD;
			weaponinfo->numprojectiles = DEFAULT_SHOTGUN_COUNT;
			weaponinfo->reload = 1.0f;
			weaponinfo->proj.damage = 8;
			break;
		case WP_GRENADE_LAUNCHER:
			weaponinfo->reload = 0.6f;
			weaponinfo->proj.damage = 120;
			weaponinfo->proj.radius = g_excessive.integer ? 300.0f : 160.0f;
			weaponinfo->proj.damagetype = DAMAGETYPE_RADIAL;
			weaponinfo->speed = g_excessive.integer ? 1200.0f : 600.0f;
			break;
		case WP_ROCKET_LAUNCHER:
			weaponinfo->reload = 0.8f;
			weaponinfo->proj.damage = 120;
			weaponinfo->proj.radius = g_excessive.integer ? 240.0f : 120.0f;
			weaponinfo->proj.damagetype = DAMAGETYPE_RADIAL;
			weaponinfo->speed = g_excessive.integer ? 2000.0f : 1000.0f;
			break;
		case WP_LIGHTNING_GUN:
			weaponinfo->reload = 0.05f;
			weaponinfo->proj.damage = 8;
			break;
		case WP_RAILGUN:
			weaponinfo->reload = 1.0f;
			weaponinfo->proj.damage = 100;
			break;
		case WP_PLASMA_RIFLE:
			weaponinfo->reload = 1.0f;
			weaponinfo->numprojectiles = 15;
			weaponinfo->proj.damage = 20;
			weaponinfo->speed = g_excessive.integer ? 3200.0f : 2355.0f;
			break;
		default:
			break;
	}
}

int trap_BotLoadCharacter(char *charfile, float skill) {
	char characterName[MAX_QPATH];
	if ( Trap_ParseLuaCharacterName( charfile, characterName, sizeof( characterName ) ) ) {
		float luaSkill = skill;
		if ( luaSkill < 0.0f || luaSkill > 1.0f ) {
			luaSkill = Com_Clamp( 1.0f, 5.0f, luaSkill );
			luaSkill = ( luaSkill - 1.0f ) / 4.0f;
		}
		{
			int handle = syscall( WB_LOAD_CHARACTER, characterName, PASSFLOAT(luaSkill) );
			if ( handle > 0 ) {
				return -handle;
			}
		}
	}
	Com_Log( SEV_INFO, LOG_CH(ch_game), S_COLOR_RED "Unsupported legacy bot character file: %s\n", charfile ? charfile : "<null>" );
	return 0;
}

void trap_BotFreeCharacter(int character) {
	if ( character < 0 ) {
		syscall( WB_FREE_CHARACTER, -character );
		return;
	}
	syscall( BOTLIB_AI_FREE_CHARACTER, character );
}

float trap_Characteristic_Float(int character, int index) {
	floatint_t fi;
	if ( character < 0 ) {
		fi.i = syscall( WB_CHARACTERISTIC_FLOAT, -character, index );
		return fi.f;
	}
	fi.i = syscall( BOTLIB_AI_CHARACTERISTIC_FLOAT, character, index );
	return fi.f;
}

float trap_Characteristic_BFloat(int character, int index, float min, float max) {
	floatint_t fi;
	if ( character < 0 ) {
		fi.i = syscall( WB_CHARACTERISTIC_BFLOAT, -character, index, PASSFLOAT(min), PASSFLOAT(max) );
		return fi.f;
	}
	fi.i = syscall( BOTLIB_AI_CHARACTERISTIC_BFLOAT, character, index, PASSFLOAT(min), PASSFLOAT(max) );
	return fi.f;
}

int trap_Characteristic_Integer(int character, int index) {
	if ( character < 0 ) {
		return syscall( WB_CHARACTERISTIC_INTEGER, -character, index );
	}
	return syscall( BOTLIB_AI_CHARACTERISTIC_INTEGER, character, index );
}

int trap_Characteristic_BInteger(int character, int index, int min, int max) {
	if ( character < 0 ) {
		return syscall( WB_CHARACTERISTIC_BINTEGER, -character, index, min, max );
	}
	return syscall( BOTLIB_AI_CHARACTERISTIC_BINTEGER, character, index, min, max );
}

void trap_Characteristic_String(int character, int index, char *buf, int size) {
	if ( character < 0 ) {
		syscall( WB_CHARACTERISTIC_STRING, -character, index, buf, size );
		return;
	}
	syscall( BOTLIB_AI_CHARACTERISTIC_STRING, character, index, buf, size );
}

int trap_BotAllocChatState(void) {
	return syscall( BOTLIB_AI_ALLOC_CHAT_STATE );
}

void trap_BotFreeChatState(int handle) {
	syscall( BOTLIB_AI_FREE_CHAT_STATE, handle );
}

void trap_BotQueueConsoleMessage(int chatstate, int type, char *message) {
	syscall( BOTLIB_AI_QUEUE_CONSOLE_MESSAGE, chatstate, type, message );
}

void trap_BotRemoveConsoleMessage(int chatstate, int handle) {
	syscall( BOTLIB_AI_REMOVE_CONSOLE_MESSAGE, chatstate, handle );
}

int trap_BotNextConsoleMessage(int chatstate, void /* struct bot_consolemessage_s */ *cm) {
	return syscall( BOTLIB_AI_NEXT_CONSOLE_MESSAGE, chatstate, cm );
}

int trap_BotNumConsoleMessages(int chatstate) {
	return syscall( BOTLIB_AI_NUM_CONSOLE_MESSAGE, chatstate );
}

void trap_BotInitialChat(int chatstate, char *type, int mcontext, char *var0, char *var1, char *var2, char *var3, char *var4, char *var5, char *var6, char *var7 ) {
	syscall( BOTLIB_AI_INITIAL_CHAT, chatstate, type, mcontext, var0, var1, var2, var3, var4, var5, var6, var7 );
}

int	trap_BotNumInitialChats(int chatstate, char *type) {
	return syscall( BOTLIB_AI_NUM_INITIAL_CHATS, chatstate, type );
}

int trap_BotReplyChat(int chatstate, char *message, int mcontext, int vcontext, char *var0, char *var1, char *var2, char *var3, char *var4, char *var5, char *var6, char *var7 ) {
	return syscall( BOTLIB_AI_REPLY_CHAT, chatstate, message, mcontext, vcontext, var0, var1, var2, var3, var4, var5, var6, var7 );
}

int trap_BotChatLength(int chatstate) {
	return syscall( BOTLIB_AI_CHAT_LENGTH, chatstate );
}

void trap_BotEnterChat(int chatstate, int client, int sendto) {
	syscall( BOTLIB_AI_ENTER_CHAT, chatstate, client, sendto );
}

void trap_BotGetChatMessage(int chatstate, char *buf, int size) {
	syscall( BOTLIB_AI_GET_CHAT_MESSAGE, chatstate, buf, size);
}

int trap_StringContains(char *str1, char *str2, int casesensitive) {
	return syscall( BOTLIB_AI_STRING_CONTAINS, str1, str2, casesensitive );
}

int trap_BotFindMatch(char *str, void /* struct bot_match_s */ *match, unsigned long int context) {
	return syscall( BOTLIB_AI_FIND_MATCH, str, match, context );
}

void trap_BotMatchVariable(void /* struct bot_match_s */ *match, int variable, char *buf, int size) {
	syscall( BOTLIB_AI_MATCH_VARIABLE, match, variable, buf, size );
}

void trap_UnifyWhiteSpaces(char *string) {
	syscall( BOTLIB_AI_UNIFY_WHITE_SPACES, string );
}

void trap_BotReplaceSynonyms(char *string, unsigned long int context) {
	syscall( BOTLIB_AI_REPLACE_SYNONYMS, string, context );
}

int trap_BotLoadChatFile(int chatstate, char *chatfile, char *chatname) {
	(void)chatstate;
	(void)chatfile;
	(void)chatname;
	return 0;
}

void trap_BotSetChatGender(int chatstate, int gender) {
	syscall( BOTLIB_AI_SET_CHAT_GENDER, chatstate, gender );
}

void trap_BotSetChatName(int chatstate, char *name, int client) {
	syscall( BOTLIB_AI_SET_CHAT_NAME, chatstate, name, client );
}

void trap_BotResetGoalState(int goalstate) {
	syscall( BOTLIB_AI_RESET_GOAL_STATE, goalstate );
}

void trap_BotResetAvoidGoals(int goalstate) {
	syscall( BOTLIB_AI_RESET_AVOID_GOALS, goalstate );
}

void trap_BotRemoveFromAvoidGoals(int goalstate, int number) {
	syscall( BOTLIB_AI_REMOVE_FROM_AVOID_GOALS, goalstate, number);
}

void trap_BotPushGoal(int goalstate, void /* struct bot_goal_s */ *goal) {
	syscall( BOTLIB_AI_PUSH_GOAL, goalstate, goal );
}

void trap_BotPopGoal(int goalstate) {
	syscall( BOTLIB_AI_POP_GOAL, goalstate );
}

void trap_BotEmptyGoalStack(int goalstate) {
	syscall( BOTLIB_AI_EMPTY_GOAL_STACK, goalstate );
}

void trap_BotDumpAvoidGoals(int goalstate) {
	syscall( BOTLIB_AI_DUMP_AVOID_GOALS, goalstate );
}

void trap_BotDumpGoalStack(int goalstate) {
	syscall( BOTLIB_AI_DUMP_GOAL_STACK, goalstate );
}

void trap_BotGoalName(int number, char *name, int size) {
	syscall( BOTLIB_AI_GOAL_NAME, number, name, size );
}

int trap_BotGetTopGoal(int goalstate, void /* struct bot_goal_s */ *goal) {
	return syscall( BOTLIB_AI_GET_TOP_GOAL, goalstate, goal );
}

int trap_BotGetSecondGoal(int goalstate, void /* struct bot_goal_s */ *goal) {
	return syscall( BOTLIB_AI_GET_SECOND_GOAL, goalstate, goal );
}

int trap_BotChooseLTGItem(int goalstate, vec3_t origin, int *inventory, int travelflags) {
	return syscall( BOTLIB_AI_CHOOSE_LTG_ITEM, goalstate, origin, inventory, travelflags );
}

int trap_BotChooseNBGItem(int goalstate, vec3_t origin, int *inventory, int travelflags, void /* struct bot_goal_s */ *ltg, float maxtime) {
	return syscall( BOTLIB_AI_CHOOSE_NBG_ITEM, goalstate, origin, inventory, travelflags, ltg, PASSFLOAT(maxtime) );
}

int trap_BotTouchingGoal(vec3_t origin, void /* struct bot_goal_s */ *goal) {
	return syscall( BOTLIB_AI_TOUCHING_GOAL, origin, goal );
}

int trap_BotItemGoalInVisButNotVisible(int viewer, vec3_t eye, vec3_t viewangles, void /* struct bot_goal_s */ *goal) {
	return syscall( BOTLIB_AI_ITEM_GOAL_IN_VIS_BUT_NOT_VISIBLE, viewer, eye, viewangles, goal );
}

int trap_BotGetLevelItemGoal(int index, char *classname, void /* struct bot_goal_s */ *goal) {
	return syscall( BOTLIB_AI_GET_LEVEL_ITEM_GOAL, index, classname, goal );
}

int trap_BotGetNextCampSpotGoal(int num, void /* struct bot_goal_s */ *goal) {
	return syscall( BOTLIB_AI_GET_NEXT_CAMP_SPOT_GOAL, num, goal );
}

int trap_BotGetMapLocationGoal(char *name, void /* struct bot_goal_s */ *goal) {
	return syscall( BOTLIB_AI_GET_MAP_LOCATION_GOAL, name, goal );
}

float trap_BotAvoidGoalTime(int goalstate, int number) {
	floatint_t fi;
	fi.i = syscall( BOTLIB_AI_AVOID_GOAL_TIME, goalstate, number );
	return fi.f;
}

void trap_BotSetAvoidGoalTime(int goalstate, int number, float avoidtime) {
	syscall( BOTLIB_AI_SET_AVOID_GOAL_TIME, goalstate, number, PASSFLOAT(avoidtime));
}

void trap_BotInitLevelItems(void) {
	syscall( BOTLIB_AI_INIT_LEVEL_ITEMS );
}

void trap_BotUpdateEntityItems(void) {
	syscall( BOTLIB_AI_UPDATE_ENTITY_ITEMS );
}

int trap_BotLoadItemWeights(int goalstate, char *filename) {
	(void)goalstate;
	(void)filename;
	return 0;
}

void trap_BotFreeItemWeights(int goalstate) {
	syscall( BOTLIB_AI_FREE_ITEM_WEIGHTS, goalstate );
}

void trap_BotInterbreedGoalFuzzyLogic(int parent1, int parent2, int child) {
	syscall( BOTLIB_AI_INTERBREED_GOAL_FUZZY_LOGIC, parent1, parent2, child );
}

void trap_BotSaveGoalFuzzyLogic(int goalstate, char *filename) {
	syscall( BOTLIB_AI_SAVE_GOAL_FUZZY_LOGIC, goalstate, filename );
}

void trap_BotMutateGoalFuzzyLogic(int goalstate, float range) {
	syscall( BOTLIB_AI_MUTATE_GOAL_FUZZY_LOGIC, goalstate, PASSFLOAT(range) );
}

int trap_BotAllocGoalState(int state) {
	return syscall( BOTLIB_AI_ALLOC_GOAL_STATE, state );
}

void trap_BotFreeGoalState(int handle) {
	syscall( BOTLIB_AI_FREE_GOAL_STATE, handle );
}

void trap_BotResetMoveState(int movestate) {
	syscall( BOTLIB_AI_RESET_MOVE_STATE, movestate );
}

void trap_BotAddAvoidSpot(int movestate, vec3_t origin, float radius, int type) {
	syscall( BOTLIB_AI_ADD_AVOID_SPOT, movestate, origin, PASSFLOAT(radius), type);
}

void trap_BotMoveToGoal(void /* struct bot_moveresult_s */ *result, int movestate, void /* struct bot_goal_s */ *goal, int travelflags) {
	syscall( BOTLIB_AI_MOVE_TO_GOAL, result, movestate, goal, travelflags );
}

int trap_BotMoveInDirection(int movestate, vec3_t dir, float speed, int type) {
	return syscall( BOTLIB_AI_MOVE_IN_DIRECTION, movestate, dir, PASSFLOAT(speed), type );
}

void trap_BotResetAvoidReach(int movestate) {
	syscall( BOTLIB_AI_RESET_AVOID_REACH, movestate );
}

void trap_BotResetLastAvoidReach(int movestate) {
	syscall( BOTLIB_AI_RESET_LAST_AVOID_REACH,movestate  );
}

int trap_BotReachabilityArea(vec3_t origin, int testground) {
	return syscall( BOTLIB_AI_REACHABILITY_AREA, origin, testground );
}

int trap_BotMovementViewTarget(int movestate, void /* struct bot_goal_s */ *goal, int travelflags, float lookahead, vec3_t target) {
	return syscall( BOTLIB_AI_MOVEMENT_VIEW_TARGET, movestate, goal, travelflags, PASSFLOAT(lookahead), target );
}

int trap_BotPredictVisiblePosition(vec3_t origin, int areanum, void /* struct bot_goal_s */ *goal, int travelflags, vec3_t target) {
	return syscall( BOTLIB_AI_PREDICT_VISIBLE_POSITION, origin, areanum, goal, travelflags, target );
}

int trap_BotAllocMoveState(void) {
	return syscall( BOTLIB_AI_ALLOC_MOVE_STATE );
}

void trap_BotFreeMoveState(int handle) {
	syscall( BOTLIB_AI_FREE_MOVE_STATE, handle );
}

void trap_BotInitMoveState(int handle, void /* struct bot_initmove_s */ *initmove) {
	syscall( BOTLIB_AI_INIT_MOVE_STATE, handle, initmove );
}

int trap_BotChooseBestFightWeapon(int weaponstate, int *inventory) {
	(void)weaponstate;

	if ( !inventory ) {
		return WP_MACHINEGUN;
	}

	if ( inventory[INVENTORY_ROCKET_LAUNCHER] > 0 && inventory[INVENTORY_ROCKETS] > 0 ) {
		return WP_ROCKET_LAUNCHER;
	}
	if ( inventory[INVENTORY_RAILGUN] > 0 && inventory[INVENTORY_SLUGS] > 0 ) {
		return WP_RAILGUN;
	}
	if ( inventory[INVENTORY_LIGHTNING_GUN] > 0 && inventory[INVENTORY_LIGHTNING] > 0 ) {
		return WP_LIGHTNING_GUN;
	}
	if ( inventory[INVENTORY_PLASMA_RIFLE] > 0 && inventory[INVENTORY_CELLS] > 0 ) {
		return WP_PLASMA_RIFLE;
	}
	if ( inventory[INVENTORY_SHOTGUN] > 0 && inventory[INVENTORY_SHELLS] > 0 ) {
		return WP_SHOTGUN;
	}
	if ( inventory[INVENTORY_GRENADE_LAUNCHER] > 0 && inventory[INVENTORY_GRENADES] > 0 ) {
		return WP_GRENADE_LAUNCHER;
	}
	if ( inventory[INVENTORY_MACHINEGUN] > 0 && inventory[INVENTORY_BULLETS] > 0 ) {
		return WP_MACHINEGUN;
	}

	if ( inventory[INVENTORY_GAUNTLET] > 0 ) {
		return WP_GAUNTLET;
	}

	return WP_MACHINEGUN;
}

void trap_BotGetWeaponInfo(int weaponstate, int weapon, void /* struct weaponinfo_s */ *weaponinfo) {
	(void)weaponstate;
	Trap_BotFillWeaponInfoFromGame( weapon, (weaponinfo_t *)weaponinfo );
}

int trap_BotLoadWeaponWeights(int weaponstate, char *filename) {
	(void)weaponstate;
	(void)filename;
	return 0;
}

int trap_BotAllocWeaponState(void) {
	return syscall( BOTLIB_AI_ALLOC_WEAPON_STATE );
}

void trap_BotFreeWeaponState(int weaponstate) {
	syscall( BOTLIB_AI_FREE_WEAPON_STATE, weaponstate );
}

void trap_BotResetWeaponState(int weaponstate) {
	syscall( BOTLIB_AI_RESET_WEAPON_STATE, weaponstate );
}

int trap_GeneticParentsAndChildSelection(int numranks, float *ranks, int *parent1, int *parent2, int *child) {
	return syscall( BOTLIB_AI_GENETIC_PARENTS_AND_CHILD_SELECTION, numranks, ranks, parent1, parent2, child );
}

int trap_PC_LoadSource( const char *filename ) {
	return syscall( BOTLIB_PC_LOAD_SOURCE, filename );
}

int trap_PC_FreeSource( int handle ) {
	return syscall( BOTLIB_PC_FREE_SOURCE, handle );
}

int trap_PC_ReadToken( int handle, pc_token_t *pc_token ) {
	return syscall( BOTLIB_PC_READ_TOKEN, handle, pc_token );
}

int trap_PC_SourceFileAndLine( int handle, char *filename, int *line ) {
	return syscall( BOTLIB_PC_SOURCE_FILE_AND_LINE, handle, filename, line );
}

// ── QUIC transport event emission ────────────────────────────────
#if FEAT_WIREDNET_OBSERVER
void trap_WiredNet_EmitKill( int attacker, int victim, int mod, vec3_t att_pos, vec3_t vic_pos ) {
	syscall( G_WIREDNET_EMIT_KILL, attacker, victim, mod, att_pos, vic_pos );
}

void trap_WiredNet_EmitDamage( int attacker, int victim, int damage, int mod, vec3_t att_pos, vec3_t vic_pos ) {
	syscall( G_WIREDNET_EMIT_DAMAGE, attacker, victim, damage, mod, att_pos, vic_pos );
}

void trap_WiredNet_EmitItemPickup( int client, const char *item, vec3_t pos ) {
	syscall( G_WIREDNET_EMIT_ITEM_PICKUP, client, item, pos );
}

void trap_WiredNet_EmitChat( int client, const char *msg, qboolean teamOnly ) {
	syscall( G_WIREDNET_EMIT_CHAT, client, msg, teamOnly );
}

void trap_WiredNet_EmitMatchEvent( const char *type, const char *data ) {
	syscall( G_WIREDNET_EMIT_MATCH_EVENT, type, data );
}

#if FEAT_UNLAGGED
void trap_WiredNet_EmitDelag( int shooter, int target, int timeDelta, vec3_t shooterPos, vec3_t targetPos ) {
	syscall( G_WIREDNET_EMIT_DELAG, shooter, target, timeDelta, shooterPos, targetPos );
}
#endif

void trap_WiredNet_EmitBotEvent( int bot_id, const char *event_type, int param1, int param2, vec3_t pos ) {
	syscall( G_WIREDNET_EMIT_BOT_EVENT, bot_id, event_type, param1, param2, pos );
}
#endif

// ── WiredCoreEvents generic emit ─────────────────────────────────────
void trap_WCE_EmitEvent( wce_event_type_t type,
                          int clientNum, int entityNum,
                          const vec3_t origin,
                          int param1, int param2, float fparam,
                          const char *text ) {
    syscall( G_WCE_EMIT_EVENT, type, clientNum, entityNum,
             origin, param1, param2, PASSFLOAT(fparam), text );
}

int trap_WCE_GetSoundEvents( int clientNum, bot_sound_event_t *out, int maxOut ) {
    return syscall( G_WCE_GET_SOUND_EVENTS, clientNum, out, maxOut );
}

// ── Recast/Detour nav traps ───────────────────────────────────────────
// Phase 2: wrappers exist; engine-side stubs return -1 until Phase 3+.
// These are guarded by FEAT_RECAST_NAVMESH so the AAS build compiles clean.
#if FEAT_RECAST_NAVMESH
int trap_Nav_FindPath( vec3_t origin, vec3_t goal, int agentType, navPath_t *pathOut ) {
	return syscall( G_NAV_FIND_PATH, origin, goal, agentType, pathOut );
}

qboolean trap_Nav_Raycast( vec3_t start, vec3_t end, vec3_t hitPosOut ) {
	return (qboolean)syscall( G_NAV_RAYCAST, start, end, hitPosOut );
}

navPolyRef_t trap_Nav_FindNearestPoly( vec3_t origin, vec3_t searchExtents ) {
	return (navPolyRef_t)syscall( G_NAV_FIND_NEAREST_POLY, origin, searchExtents );
}

int trap_Nav_GetPolyAreaFlags( navPolyRef_t polyRef ) {
	return syscall( G_NAV_GET_POLY_AREA_FLAGS, polyRef );
}

void trap_Nav_TriggerOffMeshLink( navPolyRef_t linkRef ) {
	syscall( G_NAV_TRIGGER_OFF_MESH_LINK, linkRef );
}

qboolean trap_Nav_GetRandomPoint( int areaFilter, vec3_t posOut ) {
	return (qboolean)syscall( G_NAV_GET_RANDOM_POINT, areaFilter, posOut );
}

int trap_Nav_AddCrowdAgent( int entityNum, vec3_t origin, int agentType ) {
	return syscall( G_NAV_ADD_CROWD_AGENT, entityNum, origin, agentType );
}

void trap_Nav_UpdateCrowdAgent( int agentId, vec3_t desiredTarget ) {
	syscall( G_NAV_UPDATE_CROWD_AGENT, agentId, desiredTarget );
}

void trap_Nav_RemoveCrowdAgent( int agentId ) {
	syscall( G_NAV_REMOVE_CROWD_AGENT, agentId );
}

void trap_Nav_UpdateCrowd( float deltaTime ) {
	syscall( G_NAV_UPDATE_CROWD, PASSFLOAT( deltaTime ) );
}
qboolean trap_Nav_IsReady( void ) {
	return (qboolean)syscall( G_NAV_IS_READY );
}
void trap_Nav_SetPolyFlagsForDoor( const char *targetname, int setFlags, int clearFlags ) {
	syscall( G_NAV_SET_POLY_FLAGS_FOR_DOOR, targetname, setFlags, clearFlags );
}
void trap_Nav_PredictEnemyPosition( const vec3_t origin, const vec3_t velocity,
                                    float predictTime, vec3_t outPos ) {
	syscall( G_NAV_PREDICT_ENEMY_POSITION, origin, velocity, PASSFLOAT( predictTime ), outPos );
}
#endif /* FEAT_RECAST_NAVMESH */

qboolean trap_GetValue( char *value, int valueSize, const char *key ) {
	return (qboolean)syscall( G_TRAP_GETVALUE, value, valueSize, key );
}
