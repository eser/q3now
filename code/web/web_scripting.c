// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
 * Browser scripting boundary. Native builds retain LuaJIT. Browser builds do
 * not forge its raw ABI: supported menu/localization/scene inputs are compiled
 * ahead of time into the bounded portable authored-content catalog, while
 * unrelated general-purpose/user/server scripting stays unavailable.
 */
#include "../client/client.h"
#include "../qcommon/wired/core/scripting/user_vm.h"
#include "../qcommon/wired/core/scripting/wired_scripting.h"
#include "../qcommon/wired/scene/wired_scene.h"
#include "../client/wired/hud/cl_wired_crosshair.h"
#include "../client/wired/l10n/cl_wired_l10n.h"
#include "../client/wired/scene/cl_wired_scene_lua.h"
#include "../client/wired/store/cl_wired_store.h"
#include "../client/wired/ui/cl_wired_attract.h"
#include "../client/wired/ui/cl_wired_lua_test.h"
#include "../server/sv_lua.h"
#include "../server/sv_wired_rcon_lua.h"
#include "web_authored_content.h"

#include <string.h>

static void WiredWeb_Clear( char *out, size_t size ) {
	if ( out && size ) out[0] = '\0';
}

void WiredScript_Init( void ) {}
void WiredScript_PostInit( void ) {}
void WiredScript_Shutdown( void ) {}
qboolean WiredScript_TryEval( const char *text ) { (void)text; return qfalse; }
void WiredScript_EnumerateGlobalsAndMembers(
		void (*callback)( const char *, int, void * ), void *context ) {
	(void)callback; (void)context;
}
qboolean WiredScript_TryExecFile( const char *filename ) { (void)filename; return qfalse; }
void WiredScript_ExecFile( const char *filename ) { (void)WiredScript_TryExecFile( filename ); }
void WiredScript_RegisterBindings( WiredScript_BindingFn fn ) { (void)fn; }
lua_State *WiredScript_GetState( void ) { return NULL; }
int WiredScript_CompileChunk( const char *text, const char *name ) {
	(void)text; (void)name; return WIRED_CHUNK_NOREF;
}
void WiredScript_ReleaseChunk( int chunk ) { (void)chunk; }
int WiredScript_CallChunkArrayLen( int chunk ) { (void)chunk; return -1; }
qboolean WiredScript_ChunkArrayItemAsString( int index, char *out,
		size_t size ) { (void)index; WiredWeb_Clear( out, size ); return qfalse; }
qboolean WiredScript_ChunkArrayItemAsNumber( int index, double *out ) {
	(void)index; if ( out ) *out = 0.0; return qfalse;
}
qboolean WiredScript_ChunkArrayItemFieldAsString( int index,
		const char *field, char *out, size_t size ) {
	(void)index; (void)field; WiredWeb_Clear( out, size ); return qfalse;
}
void WiredScript_ChunkArrayRelease( void ) {}
qboolean WiredScript_CallChunkBool( int chunk, qboolean fallback ) {
	(void)chunk; return fallback;
}
qboolean WiredScript_CallChunkString( int chunk, char *out, size_t size ) {
	(void)chunk; WiredWeb_Clear( out, size ); return qfalse;
}
qboolean WiredScript_CallChunkNumber( int chunk, double *out ) {
	(void)chunk; if ( out ) *out = 0.0; return qfalse;
}
lua_State *WiredScript_PushChunkForArgs( int chunk ) { (void)chunk; return NULL; }
qboolean WiredScript_PCallArgs( int args, int results, const char *tag ) {
	(void)args; (void)results; (void)tag; return qfalse;
}

void UserVM_Init( void ) {}
void UserVM_PostInit( void ) {}
void UserVM_Shutdown( void ) {}
lua_State *UserVM_GetState( void ) { return NULL; }
void UserVM_RegisterBindings( UserVM_BindingFn fn ) { (void)fn; }
void UserVM_SetAdminContext( qboolean admin ) { (void)admin; }
qboolean UserVM_IsAdminContext( void ) { return qfalse; }
qboolean UserVM_RconExecute( const char *code, char *out, int size ) {
	(void)code; WiredWeb_Clear( out, size > 0 ? (size_t)size : 0u ); return qfalse;
}
int UserVM_CompileChunk( const char *text, const char *name ) {
	(void)text; (void)name; return WIRED_CHUNK_NOREF;
}
void UserVM_ReleaseChunk( int chunk ) { (void)chunk; }
int UserVM_CallChunkArrayLen( int chunk ) { (void)chunk; return -1; }
qboolean UserVM_ChunkArrayItemAsString( int index, char *out, size_t size ) {
	(void)index; WiredWeb_Clear( out, size ); return qfalse;
}
qboolean UserVM_ChunkArrayItemAsNumber( int index, double *out ) {
	(void)index; if ( out ) *out = 0.0; return qfalse;
}
qboolean UserVM_ChunkArrayItemFieldAsString( int index, const char *field,
		char *out, size_t size ) {
	(void)index; (void)field; WiredWeb_Clear( out, size ); return qfalse;
}
void UserVM_ChunkArrayRelease( void ) {}
qboolean UserVM_CallChunkBool( int chunk, qboolean fallback ) {
	(void)chunk; return fallback;
}
qboolean UserVM_CallChunkString( int chunk, char *out, size_t size ) {
	(void)chunk; WiredWeb_Clear( out, size ); return qfalse;
}
qboolean UserVM_CallChunkNumber( int chunk, double *out ) {
	(void)chunk; if ( out ) *out = 0.0; return qfalse;
}

int WiredScene_ReadTable( wiredScene_t *out, lua_State *state,
		const char *name ) {
	(void)state; (void)name; if ( out ) memset( out, 0, sizeof( *out ) ); return 0;
}
int WiredScene_LoadFromFile( wiredScene_t *out, const char *path ) {
	if ( out ) memset( out, 0, sizeof( *out ) );
	return WiredWebAuthored_LoadScene( out, path );
}

#ifdef WIRED_WEB_UI_NATIVE

void WiredStoreLua_Init( void ) {}
void WiredScene_LuaInit( void ) {}
void WiredUITest_LuaInit( void ) {}
void WiredUITest_Init( void ) {}
void WiredUITest_Shutdown( void ) {}
void WiredUITest_Tick( void ) {}

void WiredL10n_Init( void ) {}
void WiredL10n_Shutdown( void ) {}
void WiredL10n_Reload( void ) {}
const char *WiredL10n_Get( const char *key ) {
	return WiredWebAuthored_Localize( key );
}

void WiredAttract_LuaInit( void ) {}
void WiredAttract_Init( void ) {}
void WiredAttract_Shutdown( void ) {}
void WiredAttract_Frame( int msec ) { (void)msec; }
void WiredAttract_NoteInput( int key ) { (void)key; }
void WiredAttract_NoteMouse( int dx, int dy ) { (void)dx; (void)dy; }
void WiredAttract_OnMenuReload( void ) {}
void WiredAttract_Start( void ) {}
void WiredAttract_Stop( void ) {}
void WiredAttract_Skip( void ) {}
wiredAttractState_t WiredAttract_GetState( void ) { return ATTRACT_STATE_IDLE; }
qboolean WiredAttract_IsActive( void ) { return qfalse; }
qboolean WiredAttract_IsDemoOverlayActive( void ) { return qfalse; }
qboolean WiredAttract_OnDemoCompleted( void ) { return qfalse; }
qboolean WiredAttract_OnDemoFailed( void ) { return qfalse; }
qboolean WiredAttract_OnCinematicCompleted( void ) { return qfalse; }

static float WiredWebCrosshair_StoreValue( const char *key, float fallback ) {
	const wuiStoreEntry_t *entry = WiredStore_Get( key );
	return entry ? entry->value : fallback;
}

static const wiredWebAuthoredCrosshair_t *WiredWebCrosshair_Find(
		const wiredWebAuthoredCatalog_t *catalog, int weapon ) {
	const wiredWebAuthoredCrosshair_t *fallback = NULL;
	int i;
	for ( i = 0; i < catalog->crosshairCount; ++i ) {
		if ( catalog->crosshairs[i].weapon == 0 ) fallback = &catalog->crosshairs[i];
		if ( catalog->crosshairs[i].weapon == weapon ) return &catalog->crosshairs[i];
	}
	return fallback;
}

void WiredCrosshair_Init( void ) { (void)WiredWebAuthored_EnsureLoaded(); }
void WiredCrosshair_Shutdown( void ) {}
qboolean WiredCrosshair_Eval( cgCrosshairDrawSpec_t *out ) {
	const wiredWebAuthoredCatalog_t *catalog;
	const wiredWebAuthoredCrosshair_t *source;
	const wuiStoreEntry_t *weaponEntry;
	float dynamicGap = 0.0f;
	int weapon = 0, i, armorClass;

	if ( !out || !WiredWebAuthored_EnsureLoaded() ) return qfalse;
	catalog = WiredWebAuthored_Catalog();
	weaponEntry = WiredStore_Get( "crosshair.weapon" );
	if ( weaponEntry && !Q_stricmp( weaponEntry->text, "machinegun" ) ) weapon = 2;
	else if ( weaponEntry && !Q_stricmp( weaponEntry->text, "lightning" ) ) weapon = 6;
	source = WiredWebCrosshair_Find( catalog, weapon );
	if ( !source ) return qfalse;

	memset( out, 0, sizeof( *out ) );
	out->visible = qtrue;
	out->scale = 1.0f;
	Vector4Copy( source->color, out->color );
	if ( source->dynamicKind == 1 ) {
		dynamicGap += Com_Clamp( 0.0f, 1.0f,
			WiredWebCrosshair_StoreValue( "crosshair.speed", 0.0f ) / 320.0f ) * 8.0f;
		dynamicGap += Com_Clamp( 0.0f, 1.0f,
			WiredWebCrosshair_StoreValue( "crosshair.recoil", 0.0f ) ) * 14.0f;
	}
	out->gap = source->gap + dynamicGap;
	for ( i = 0; i < 4; ++i ) {
		out->arms[i].enabled = qtrue;
		out->arms[i].length = source->armLength;
		out->arms[i].thickness = source->armThickness;
	}
	out->dot.enabled = source->dotEnabled ? qtrue : qfalse;
	out->dot.radius = source->dotRadius;
	out->dot.filled = qtrue;
	out->ring.enabled = source->ringEnabled ? qtrue : qfalse;
	out->ring.radius = source->ringRadius;
	out->ring.thickness = source->ringThickness;
	out->ring.filled = qfalse;
	out->outline.thickness = source->outlineThickness;
	out->outline.alpha = source->outlineAlpha;
	armorClass = (int)WiredWebCrosshair_StoreValue( "crosshair.target.armorClass", 0.0f );
	if ( WiredWebCrosshair_StoreValue( "crosshair.target.exists", 0.0f ) != 0.0f
			&& armorClass > 0 ) {
		if ( armorClass == 1 ) VectorSet( out->color, 0.4f, 1.0f, 0.4f );
		else if ( armorClass == 2 ) VectorSet( out->color, 1.0f, 1.0f, 0.3f );
		else VectorSet( out->color, 1.0f, 0.3f, 0.3f );
	}
	i = (int)WiredWebCrosshair_StoreValue( "crosshair.weapon_status", 0.0f );
	if ( i == 1 || i == 2 ) out->visible = qfalse;
	return qtrue;
}

#endif /* WIRED_WEB_UI_NATIVE */

void CL_Characters_Init( void ) {}
void CL_Characters_Reload( void ) {}
void CL_Characters_Shutdown( void ) {}
void CL_Characters_RegisterIcons( void ) {}
void CL_Characters_RegisterShaders( void ) {}
const characterManifest_t *CL_Characters_Get( const char *name ) {
	(void)name; return NULL;
}
int CL_Characters_Count( void ) { return 0; }
const clCharacterEntry_t *CL_Characters_At( int index ) { (void)index; return NULL; }
int CL_Characters_SelectableCount( void ) { return 0; }
const clCharacterEntry_t *CL_Characters_SelectableAt( int index ) {
	(void)index; return NULL;
}
qboolean CL_Characters_IsBotEligible( const char *name ) {
	(void)name; return qfalse;
}
unsigned int CL_Characters_Generation( void ) { return 0u; }
qboolean CL_Characters_GetManifest( const char *name, char *out, int size ) {
	(void)name; WiredWeb_Clear( out, size > 0 ? (size_t)size : 0u ); return qfalse;
}
const cmSkin_t *CL_GetCharacterSkin( qhandle_t handle ) { (void)handle; return NULL; }

void SV_Lua_Init( void ) {}
void SV_Lua_Shutdown( void ) {}
void SV_Lua_EnsureInit( void ) {}
int SV_Lua_LoadCharacter( const char *name, float skill ) {
	(void)name; (void)skill; return 0;
}
void SV_Lua_FreeCharacter( int handle ) { (void)handle; }
float SV_Lua_CharacteristicBFloat( int handle, int index, float min,
		float max ) { (void)handle; (void)index; (void)max; return min; }
void SV_Lua_CharacteristicString( int handle, int index, char *out, int size ) {
	(void)handle; (void)index; WiredWeb_Clear( out, size > 0 ? (size_t)size : 0u );
}
int SV_Lua_BindBot( int client, int handle ) { (void)client; (void)handle; return 0; }
int SV_Lua_BotThink( int client, float time ) { (void)client; (void)time; return 0; }
float SV_Lua_BotProfileField( int client, int field ) {
	(void)client; (void)field; return 0.0f;
}
int SV_Lua_BotPickWeapon( int client, const wbCombatCtx_t *context,
		char *out, int size ) {
	(void)client; (void)context; WiredWeb_Clear( out, size > 0 ? (size_t)size : 0u ); return 0;
}
float SV_Lua_BotGetAttackAimHeight( int client, int weapon ) {
	(void)client; (void)weapon; return 0.0f;
}
int SV_Lua_BotEvalItem( int client, const wbItemEvalCtx_t *context ) {
	(void)client; (void)context; return 0;
}
int SV_Lua_BotDecide( int client, const wbDecideCtx_t *context,
		char *out, int size ) {
	(void)client; (void)context; WiredWeb_Clear( out, size > 0 ? (size_t)size : 0u ); return 0;
}
int SV_Lua_BotOnChat( int client, const char *event, const wbChatCtx_t *context,
		char *out, int size ) {
	(void)client; (void)event; (void)context;
	WiredWeb_Clear( out, size > 0 ? (size_t)size : 0u ); return 0;
}
int SV_Lua_MonsterBind( int entity, int handle ) { (void)entity; (void)handle; return 0; }
void SV_Lua_MonsterUnbind( int entity ) { (void)entity; }
float SV_Lua_MonsterProfileField( int entity, int field ) {
	(void)entity; (void)field; return 0.0f;
}
int SV_Lua_MonsterDecide( int entity, const wbDecideCtx_t *context,
		char *out, int size ) {
	(void)entity; (void)context; WiredWeb_Clear( out, size > 0 ? (size_t)size : 0u ); return 0;
}
void SV_BotVerifyCharacter_f( void ) {}
void SV_BotDebugWeapons_f( void ) {}
qboolean SV_Lua_GetCharacterDisplayName( const char *name, char *out, int size ) {
	(void)name; WiredWeb_Clear( out, size > 0 ? (size_t)size : 0u ); return qfalse;
}
qboolean SV_Lua_GetCharacterBBox( const char *name, char *out, int size ) {
	return SV_Lua_GetCharacterDisplayName( name, out, size );
}
qboolean SV_Lua_GetCharacterMovement( const char *name, char *out, int size ) {
	return SV_Lua_GetCharacterDisplayName( name, out, size );
}
qboolean SV_Lua_GetCharacterAttack( const char *name, char *out, int size ) {
	return SV_Lua_GetCharacterDisplayName( name, out, size );
}
qboolean SV_Lua_GetCharacterCanActivate( const char *name, char *out, int size ) {
	return SV_Lua_GetCharacterDisplayName( name, out, size );
}
int SV_Lua_GetCharacterCount( void ) { return 0; }
qboolean SV_Lua_GetCharacterAt( int index, char *out, int size ) {
	(void)index; WiredWeb_Clear( out, size > 0 ? (size_t)size : 0u ); return qfalse;
}
void SV_RconLua_Init( void ) {}
void SV_RconLua_Shutdown( void ) {}
qboolean SV_RconLua_Execute( const char *code, char *out, int size ) {
	(void)code; WiredWeb_Clear( out, size > 0 ? (size_t)size : 0u ); return qfalse;
}
