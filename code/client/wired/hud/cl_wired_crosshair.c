// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
===========================================================================
cl_wired_crosshair.c -- Wired Crosshair: procedural per-weapon reticle

Hosts the three-layer Lua pipeline (default base -> weapon static base ->
per-frame update(state) delta) on the System VM, exposes the q3.* constant
table, and produces the merged draw-spec the crosshair HUD element renders.

Reuses the WiredScript chunk-ref machinery (CompileChunk / ReleaseChunk /
PushChunkForArgs / PCallArgs) and the System VM (WiredScript_GetState). The
spec deep-merge (§5.3) lives in Lua; this module builds the C state table and
reads the merged result table back into a C scratch struct (§8: no per-frame
heap).
===========================================================================
*/

#include "../../client.h"
#include "../../../qcommon/wired/core/scripting/wired_scripting.h"
#include "../store/cl_wired_store.h"
#include "cl_wired_crosshair.h"

LOG_DECLARE_CHANNEL( ch_client, "client" );

#if FEAT_WIRED_UI

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

/* ── q3.* value-mirror (literal ints) ──────────────────────────────────
 * The Lua scripts run in the engine-side System VM, which must not include
 * the game-private bg_public.h. The constants below are LITERAL-INT mirrors
 * of the canonical game enums; the cross-references identify the source of
 * truth. Mirrors must stay in step with those defs (drift would mis-tint /
 * mis-normalize, never crash). weapon_t lives in the engine-visible
 * protocol.h, so WP_* could be pulled in, but is mirrored here too to keep
 * the whole q3 table in one place and the script-facing contract stable. */

/* weaponstate_t — mirrors code/game/bg_public.h:148 (WEAPON_*) */
#define Q3_WPSTAT_READY    0
#define Q3_WPSTAT_RAISING  1
#define Q3_WPSTAT_DROPPING 2
#define Q3_WPSTAT_FIRING   3
/* armor_t — mirrors code/game/bg_public.h:307 (ARM_*) */
#define Q3_ARM_NONE        0
#define Q3_ARM_JACKET      1
#define Q3_ARM_COMBAT      2
#define Q3_ARM_HEAVY       3
/* reference player run speed — mirrors code/game/bg_public.h:38
 * (DEFAULT_MOVESPEED_PLAYER) */
#define Q3_RUN_SPEED       320
/* weapon_t — mirrors code/qcommon/wired/protocol.h:522 (WP_*) */
#define Q3_WP_NONE              0
#define Q3_WP_GAUNTLET          1
#define Q3_WP_MACHINEGUN        2
#define Q3_WP_SHOTGUN           3
#define Q3_WP_GRENADE_LAUNCHER  4
#define Q3_WP_ROCKET_LAUNCHER   5
#define Q3_WP_LIGHTNING_GUN     6
#define Q3_WP_RAILGUN           7
#define Q3_WP_PLASMA_RIFLE      8

/* ── module state ──────────────────────────────────────────────────────
 * Per-weapon cache keyed by dir-token (e.g. "machinegun"). staticRef holds
 * the deep-merged (default + weapon.base) table; updateRef holds the
 * weapon's update function (LUA_NOREF when not dynamic). */

#define XH_MAX_WEAPONS   32
#define XH_TOKEN_LEN     64

typedef struct {
	char     token[XH_TOKEN_LEN];
	int      staticRef;    /* registry ref: merged static base table */
	int      updateRef;    /* registry ref: update fn, or LUA_NOREF */
	qboolean dynamic;
	qboolean loaded;
} xhWeaponEntry_t;

static struct {
	qboolean        ready;          /* default loaded, q3.* bound, system live */
	int             defaultBaseRef; /* registry ref: default.lua's base table */
	int             deepMergeRef;   /* registry ref: deep_merge(a,b) function */
	xhWeaponEntry_t weapons[XH_MAX_WEAPONS];
	int             weaponCount;
} xh;

/* ── q3.helpers.armor_tint(target) ─────────────────────────────────────
 * Single source of truth for the tier-color mapping (spec §5.5). Reads
 * target.exists + target.armor_class from the passed state.target table;
 * returns {r,g,b,a} or nil when no armor / target absent. */
static int WiredCrosshair_ArmorTint( lua_State *L ) {
	int      armorClass = Q3_ARM_NONE;
	qboolean exists = qfalse;

	if ( lua_istable( L, 1 ) ) {
		lua_getfield( L, 1, "exists" );
		exists = lua_toboolean( L, -1 ) ? qtrue : qfalse;
		lua_pop( L, 1 );
		lua_getfield( L, 1, "armor_class" );
		armorClass = (int)lua_tointeger( L, -1 );
		lua_pop( L, 1 );
	}

	if ( !exists || armorClass <= Q3_ARM_NONE ) {
		lua_pushnil( L );
		return 1;
	}

	lua_newtable( L );
	switch ( armorClass ) {
		case Q3_ARM_JACKET:  /* green */
			lua_pushnumber( L, 0.4 ); lua_rawseti( L, -2, 1 );
			lua_pushnumber( L, 1.0 ); lua_rawseti( L, -2, 2 );
			lua_pushnumber( L, 0.4 ); lua_rawseti( L, -2, 3 );
			lua_pushnumber( L, 1.0 ); lua_rawseti( L, -2, 4 );
			break;
		case Q3_ARM_COMBAT:  /* yellow */
			lua_pushnumber( L, 1.0 ); lua_rawseti( L, -2, 1 );
			lua_pushnumber( L, 1.0 ); lua_rawseti( L, -2, 2 );
			lua_pushnumber( L, 0.3 ); lua_rawseti( L, -2, 3 );
			lua_pushnumber( L, 1.0 ); lua_rawseti( L, -2, 4 );
			break;
		case Q3_ARM_HEAVY:   /* red */
		default:
			lua_pushnumber( L, 1.0 ); lua_rawseti( L, -2, 1 );
			lua_pushnumber( L, 0.3 ); lua_rawseti( L, -2, 2 );
			lua_pushnumber( L, 0.3 ); lua_rawseti( L, -2, 3 );
			lua_pushnumber( L, 1.0 ); lua_rawseti( L, -2, 4 );
			break;
	}
	return 1;
}

/* Register the global q3 table: constants + helpers. Queued via
 * WiredScript_RegisterBindings, invoked at PostInit on the System VM. */
static void WiredCrosshair_RegisterQ3( lua_State *L ) {
	lua_newtable( L );   /* q3 */

	#define Q3_SET_INT( name, val ) \
		do { lua_pushinteger( L, (val) ); lua_setfield( L, -2, name ); } while ( 0 )

	Q3_SET_INT( "WPSTAT_READY",    Q3_WPSTAT_READY );
	Q3_SET_INT( "WPSTAT_RAISING",  Q3_WPSTAT_RAISING );
	Q3_SET_INT( "WPSTAT_DROPPING", Q3_WPSTAT_DROPPING );
	Q3_SET_INT( "WPSTAT_FIRING",   Q3_WPSTAT_FIRING );

	Q3_SET_INT( "ARM_NONE",   Q3_ARM_NONE );
	Q3_SET_INT( "ARM_JACKET", Q3_ARM_JACKET );
	Q3_SET_INT( "ARM_COMBAT", Q3_ARM_COMBAT );
	Q3_SET_INT( "ARM_HEAVY",  Q3_ARM_HEAVY );

	Q3_SET_INT( "RUN_SPEED",  Q3_RUN_SPEED );

	Q3_SET_INT( "WP_NONE",             Q3_WP_NONE );
	Q3_SET_INT( "WP_GAUNTLET",         Q3_WP_GAUNTLET );
	Q3_SET_INT( "WP_MACHINEGUN",       Q3_WP_MACHINEGUN );
	Q3_SET_INT( "WP_SHOTGUN",          Q3_WP_SHOTGUN );
	Q3_SET_INT( "WP_GRENADE_LAUNCHER", Q3_WP_GRENADE_LAUNCHER );
	Q3_SET_INT( "WP_ROCKET_LAUNCHER",  Q3_WP_ROCKET_LAUNCHER );
	Q3_SET_INT( "WP_LIGHTNING_GUN",    Q3_WP_LIGHTNING_GUN );
	Q3_SET_INT( "WP_LIGHTNING",        Q3_WP_LIGHTNING_GUN );  /* spec alias */
	Q3_SET_INT( "WP_RAILGUN",          Q3_WP_RAILGUN );
	Q3_SET_INT( "WP_PLASMA_RIFLE",     Q3_WP_PLASMA_RIFLE );

	#undef Q3_SET_INT

	/* q3.helpers = { armor_tint = <cfn> } */
	lua_newtable( L );
	lua_pushcfunction( L, WiredCrosshair_ArmorTint );
	lua_setfield( L, -2, "armor_tint" );
	lua_setfield( L, -2, "helpers" );

	lua_setglobal( L, "q3" );

	Com_Log( SEV_INFO, LOG_CH(ch_client),
		"WiredCrosshair: q3.* constants + helpers registered\n" );
}

/* ── deep_merge (§5.3) bootstrap ───────────────────────────────────────
 * The merge lives in Lua: hash tables recurse, array-style tables (first key
 * [1], length == #t) are replaced wholesale (so color {r,g,b,a} swaps whole).
 * Compiled once, the returned function held by ref. */
static const char *XH_DEEP_MERGE_SRC =
	"local function is_array(t)\n"
	"  if type(t) ~= 'table' then return false end\n"
	"  if t[1] == nil then return false end\n"
	"  local n = 0\n"
	"  for _ in pairs(t) do n = n + 1 end\n"
	"  return n == #t\n"
	"end\n"
	"local function merge(a, b)\n"
	"  if type(b) ~= 'table' then return b end\n"
	"  if is_array(b) then\n"
	"    local out = {}\n"
	"    for i = 1, #b do out[i] = b[i] end\n"
	"    return out\n"
	"  end\n"
	"  local out = {}\n"
	"  if type(a) == 'table' then\n"
	"    for k, v in pairs(a) do out[k] = v end\n"
	"  end\n"
	"  for k, v in pairs(b) do\n"
	"    out[k] = merge(out[k], v)\n"
	"  end\n"
	"  return out\n"
	"end\n"
	"return merge\n";

/* Run a .lua script as a chunk and leave its returned value on the Lua stack.
 * Returns qtrue with the result on top, qfalse (nothing pushed) on
 * missing/parse/exec error. severityMissing chooses how a missing file is
 * logged (default.lua is FATAL-worthy; weapon files are silent). */
static qboolean WiredCrosshair_RunModule( lua_State *L, const char *path,
                                          qboolean *outMissing ) {
	fileHandle_t f;
	int          len;
	char        *buf;
	int          ref;

	if ( outMissing ) *outMissing = qfalse;

	len = FS_FOpenFileRead( path, &f, qfalse );
	if ( len <= 0 || f == 0 ) {
		if ( f ) FS_FCloseFile( f );
		if ( outMissing ) *outMissing = qtrue;
		return qfalse;
	}

	buf = Z_Malloc( len + 1 );
	FS_Read( buf, len, f );
	buf[len] = '\0';
	FS_FCloseFile( f );

	/* Compile to a chunk-ref, then call it with PushChunkForArgs + PCallArgs
	 * (nargs=0, nresults=1) so the returned module table is left on the stack.
	 * Reuses the shared error-logging path. */
	ref = WiredScript_CompileChunk( buf, path );
	Z_Free( buf );
	if ( ref == WIRED_CHUNK_NOREF ) {
		return qfalse;   /* CompileChunk already logged SEV_ERROR */
	}

	if ( !WiredScript_PushChunkForArgs( ref ) ) {
		WiredScript_ReleaseChunk( ref );
		return qfalse;
	}
	if ( !WiredScript_PCallArgs( 0, 1, path ) ) {
		WiredScript_ReleaseChunk( ref );
		return qfalse;
	}
	WiredScript_ReleaseChunk( ref );   /* the chunk fn; the result is on the stack */

	if ( !lua_istable( L, -1 ) ) {
		Com_Log( SEV_ERROR, LOG_CH(ch_client),
			"WiredCrosshair: '%s' did not return a table\n", path );
		lua_pop( L, 1 );
		return qfalse;
	}
	return qtrue;
}

/* deep_merge(a, b): a and b are tables already on the stack via callers? No —
 * this pushes refs. Given two registry refs (or LUA_NOREF for "empty"),
 * returns a NEW registry ref to the merged table, or LUA_NOREF on failure. */
static int WiredCrosshair_DeepMergeRefs( lua_State *L, int aRef, int bRef ) {
	int outRef;

	if ( !WiredScript_PushChunkForArgs( xh.deepMergeRef ) ) return LUA_NOREF;

	/* arg a */
	if ( aRef == LUA_NOREF ) lua_newtable( L );
	else                     lua_rawgeti( L, LUA_REGISTRYINDEX, aRef );
	/* arg b */
	if ( bRef == LUA_NOREF ) lua_newtable( L );
	else                     lua_rawgeti( L, LUA_REGISTRYINDEX, bRef );

	if ( !WiredScript_PCallArgs( 2, 1, "crosshair-deepmerge" ) ) {
		return LUA_NOREF;
	}
	if ( !lua_istable( L, -1 ) ) {
		lua_pop( L, 1 );
		return LUA_NOREF;
	}
	outRef = luaL_ref( L, LUA_REGISTRYINDEX );   /* pops result, holds it */
	return outRef;
}

/* Extract the `base` subtable of a module table (top of stack) as a new
 * registry ref, or LUA_NOREF if absent. Does not pop the module table. */
static int WiredCrosshair_RefField( lua_State *L, const char *field ) {
	int ref;
	lua_getfield( L, -1, field );
	if ( lua_isnil( L, -1 ) ) {
		lua_pop( L, 1 );
		return LUA_NOREF;
	}
	ref = luaL_ref( L, LUA_REGISTRYINDEX );   /* pops the field value */
	return ref;
}

/* Load one weapon's crosshair.lua into entry. Assumes default loaded. */
/* Returns qtrue if this weapon actually has a crosshair.lua (loaded, or present
 * but errored); qfalse if the file is absent, so the caller can skip claiming a
 * slot without a separate existence probe (RunModule already opens the file
 * once and reports absence via `missing`). */
static qboolean WiredCrosshair_LoadWeapon( lua_State *L, xhWeaponEntry_t *e ) {
	char     path[MAX_QPATH];
	qboolean missing = qfalse;
	int      baseRef, updateRef;

	e->loaded    = qtrue;
	e->dynamic   = qfalse;
	e->updateRef = LUA_NOREF;
	e->staticRef = LUA_NOREF;

	Com_sprintf( path, sizeof( path ), "weapons/%s/crosshair.lua", e->token );

	if ( !WiredCrosshair_RunModule( L, path, &missing ) ) {
		/* Absent file: no slot will be kept (caller drops it), so don't allocate
		 * the fallback ref — that would leak once the slot is reused. Present-but-
		 * errored: keep the graceful default-base fallback (parse/exec error was
		 * already logged) and count it as a real script. */
		if ( missing ) {
			return qfalse;
		}
		e->staticRef = WiredCrosshair_DeepMergeRefs( L, xh.defaultBaseRef, LUA_NOREF );
		return qtrue;
	}
	/* module table on top */

	lua_getfield( L, -1, "dynamic" );
	e->dynamic = lua_toboolean( L, -1 ) ? qtrue : qfalse;
	lua_pop( L, 1 );

	baseRef   = WiredCrosshair_RefField( L, "base" );    /* may be LUA_NOREF */
	updateRef = WiredCrosshair_RefField( L, "update" );  /* may be LUA_NOREF */

	lua_pop( L, 1 );   /* module table */

	/* static base = deep_merge(default_base, weapon_base) */
	e->staticRef = WiredCrosshair_DeepMergeRefs( L, xh.defaultBaseRef, baseRef );
	if ( baseRef != LUA_NOREF ) luaL_unref( L, LUA_REGISTRYINDEX, baseRef );

	if ( e->dynamic && updateRef != LUA_NOREF ) {
		e->updateRef = updateRef;
	} else {
		if ( updateRef != LUA_NOREF ) luaL_unref( L, LUA_REGISTRYINDEX, updateRef );
		e->dynamic = qfalse;
	}
	return qtrue;
}

/* Discover + eagerly load all weapons that ship a crosshair.lua. */
static void WiredCrosshair_LoadAllWeapons( lua_State *L ) {
	char **dirs;
	int    numDirs = 0, i, loaded = 0;

	dirs = FS_ListDirectories( "weapons", &numDirs );
	for ( i = 0; i < numDirs && xh.weaponCount < XH_MAX_WEAPONS; i++ ) {
		xhWeaponEntry_t *e = &xh.weapons[xh.weaponCount];

		/* Load once — no separate existence probe. RunModule (inside LoadWeapon)
		 * opens weapons/<dir>/crosshair.lua a single time; if it is absent,
		 * LoadWeapon returns qfalse and we release the tentative slot instead of
		 * re-opening the file just to test existence. */
		Q_strncpyz( e->token, dirs[i], sizeof( e->token ) );
		if ( !WiredCrosshair_LoadWeapon( L, e ) ) {
			continue;   /* no script in this weapon dir — slot not claimed */
		}
		xh.weaponCount++;
		loaded++;
	}
	if ( dirs ) FS_FreeFileList( dirs );

	Com_Log( SEV_INFO, LOG_CH(ch_client),
		"WiredCrosshair: loaded %d weapon crosshair%s\n",
		loaded, loaded == 1 ? "" : "s" );
}

/* Locate (or lazily load) the cache entry for a dir-token. */
static xhWeaponEntry_t *WiredCrosshair_Entry( lua_State *L, const char *token ) {
	int i;
	xhWeaponEntry_t *e;

	if ( !token || !token[0] ) return NULL;

	for ( i = 0; i < xh.weaponCount; i++ ) {
		if ( !Q_stricmp( xh.weapons[i].token, token ) ) {
			return &xh.weapons[i];
		}
	}
	if ( xh.weaponCount >= XH_MAX_WEAPONS ) return NULL;

	e = &xh.weapons[xh.weaponCount++];
	Q_strncpyz( e->token, token, sizeof( e->token ) );
	WiredCrosshair_LoadWeapon( L, e );
	return e;
}

/* PostInit: default.lua + deep_merge + all weapon scripts. */
static void WiredCrosshair_LoadScripts( lua_State *L ) {
	int defModuleBaseRef;
	int deepRef;

	/* deep_merge */
	deepRef = WiredScript_CompileChunk( XH_DEEP_MERGE_SRC, "crosshair/deep_merge" );
	if ( deepRef != WIRED_CHUNK_NOREF &&
	     WiredScript_PushChunkForArgs( deepRef ) &&
	     WiredScript_PCallArgs( 0, 1, "crosshair-deepmerge-init" ) &&
	     lua_isfunction( L, -1 ) ) {
		xh.deepMergeRef = luaL_ref( L, LUA_REGISTRYINDEX );
	} else {
		Com_Log( SEV_FATAL, LOG_CH(ch_client),
			"WiredCrosshair: failed to initialize deep_merge\n" );
		return;
	}
	WiredScript_ReleaseChunk( deepRef );

	/* default.lua — mandatory */
	{
		qboolean missing = qfalse;
		if ( !WiredCrosshair_RunModule( L, "scripts/crosshair/default.lua", &missing ) ) {
			Com_Log( SEV_FATAL, LOG_CH(ch_client),
				"WiredCrosshair: scripts/crosshair/default.lua %s — crosshair system has no safe fallback\n",
				missing ? "missing" : "failed to load" );
			return;
		}
		defModuleBaseRef = WiredCrosshair_RefField( L, "base" );
		lua_pop( L, 1 );   /* module table */
		if ( defModuleBaseRef == LUA_NOREF ) {
			Com_Log( SEV_FATAL, LOG_CH(ch_client),
				"WiredCrosshair: default.lua has no 'base' table\n" );
			return;
		}
		xh.defaultBaseRef = defModuleBaseRef;
		Com_Log( SEV_INFO, LOG_CH(ch_client),
			"WiredCrosshair: loaded default crosshair\n" );
	}

	xh.ready = qtrue;
	WiredCrosshair_LoadAllWeapons( L );
}

/* ── per-frame state-table build ───────────────────────────────────────
 * Reads the staged crosshair.* keys and builds the Lua `state` table the
 * spec's update(state) expects (§4.2). Leaves the table on top of the stack. */
static float WiredCrosshair_StoreValue( const char *key, float def ) {
	const wuiStoreEntry_t *e = WiredStore_Get( key );
	return e ? e->value : def;
}

static void WiredCrosshair_PushState( lua_State *L ) {
	lua_newtable( L );   /* state */

	lua_pushnumber( L, WiredCrosshair_StoreValue( "crosshair.speed", 0.0f ) );
	lua_setfield( L, -2, "speed" );

	lua_pushboolean( L, WiredCrosshair_StoreValue( "crosshair.is_firing", 0.0f ) != 0.0f );
	lua_setfield( L, -2, "is_firing" );

	lua_pushboolean( L, WiredCrosshair_StoreValue( "crosshair.is_crouching", 0.0f ) != 0.0f );
	lua_setfield( L, -2, "is_crouching" );

	/* recoil = the REAL normalized fire-cone size (0..1) from
	 * BG_CalcWeaponSpreadNormalized, staged by the bridge from the predicted
	 * ramp (RS-5). NOT a fake decaying proxy — it matches the server cone. */
	lua_pushnumber( L, WiredCrosshair_StoreValue( "crosshair.recoil", 0.0f ) );
	lua_setfield( L, -2, "recoil" );

	lua_pushnumber( L, WiredCrosshair_StoreValue( "crosshair.charge", 0.0f ) );
	lua_setfield( L, -2, "charge" );

	/* state.target { exists, armor_class } — instantaneous trace (§4.1) */
	lua_newtable( L );
	lua_pushboolean( L, WiredCrosshair_StoreValue( "crosshair.target.exists", 0.0f ) != 0.0f );
	lua_setfield( L, -2, "exists" );
	lua_pushinteger( L, (int)WiredCrosshair_StoreValue( "crosshair.target.armorClass", 0.0f ) );
	lua_setfield( L, -2, "armor_class" );
	lua_pushboolean( L, WiredCrosshair_StoreValue( "crosshair.isTeammate", 0.0f ) != 0.0f );
	lua_setfield( L, -2, "is_teammate" );
	lua_setfield( L, -2, "target" );
}

/* ── merged-spec readback (§5.2) ───────────────────────────────────────
 * Helpers read fields from the table at the given absolute stack index. */
static float xh_field_num( lua_State *L, int t, const char *k, float def ) {
	float v = def;
	lua_getfield( L, t, k );
	if ( lua_isnumber( L, -1 ) ) v = (float)lua_tonumber( L, -1 );
	lua_pop( L, 1 );
	return v;
}

static qboolean xh_field_bool( lua_State *L, int t, const char *k, qboolean def ) {
	qboolean v = def;
	lua_getfield( L, t, k );
	if ( !lua_isnil( L, -1 ) ) v = lua_toboolean( L, -1 ) ? qtrue : qfalse;
	lua_pop( L, 1 );
	return v;
}

/* Read a {r,g,b,a} array field into out. Leaves out unchanged if absent. */
static void xh_field_color( lua_State *L, int t, const char *k, vec4_t out ) {
	int i;
	lua_getfield( L, t, k );
	if ( lua_istable( L, -1 ) ) {
		for ( i = 0; i < 4; i++ ) {
			lua_rawgeti( L, -1, i + 1 );
			if ( lua_isnumber( L, -1 ) ) out[i] = (float)lua_tonumber( L, -1 );
			lua_pop( L, 1 );
		}
	}
	lua_pop( L, 1 );
}

/* Read one arms.<side> subtable (relative to the arms table at index `armsT`). */
static void xh_read_arm( lua_State *L, int armsT, const char *side,
                         cgCrosshairArm_t *arm ) {
	lua_getfield( L, armsT, side );
	if ( lua_istable( L, -1 ) ) {
		int t = lua_gettop( L );
		arm->enabled   = xh_field_bool( L, t, "enabled", arm->enabled );
		arm->length    = xh_field_num ( L, t, "length", arm->length );
		arm->thickness = xh_field_num ( L, t, "thickness", arm->thickness );
	}
	lua_pop( L, 1 );
}

/* Read the merged spec table (top of stack) into out. Pops the table. */
static void WiredCrosshair_ReadSpec( lua_State *L, cgCrosshairDrawSpec_t *out ) {
	int t = lua_gettop( L );

	out->visible   = xh_field_bool( L, t, "visible", qtrue );
	out->scale     = xh_field_num ( L, t, "scale", out->scale );
	out->gap       = xh_field_num ( L, t, "gap", out->gap );
	xh_field_color( L, t, "color", out->color );

	lua_getfield( L, t, "arms" );
	if ( lua_istable( L, -1 ) ) {
		int armsT = lua_gettop( L );
		xh_read_arm( L, armsT, "top",    &out->arms[CG_XH_ARM_TOP] );
		xh_read_arm( L, armsT, "right",  &out->arms[CG_XH_ARM_RIGHT] );
		xh_read_arm( L, armsT, "bottom", &out->arms[CG_XH_ARM_BOTTOM] );
		xh_read_arm( L, armsT, "left",   &out->arms[CG_XH_ARM_LEFT] );
	}
	lua_pop( L, 1 );

	lua_getfield( L, t, "dot" );
	if ( lua_istable( L, -1 ) ) {
		int d = lua_gettop( L );
		out->dot.enabled = xh_field_bool( L, d, "enabled", out->dot.enabled );
		out->dot.radius  = xh_field_num ( L, d, "radius", out->dot.radius );
		out->dot.filled  = xh_field_bool( L, d, "filled", out->dot.filled );
	}
	lua_pop( L, 1 );

	lua_getfield( L, t, "ring" );
	if ( lua_istable( L, -1 ) ) {
		int r = lua_gettop( L );
		out->ring.enabled   = xh_field_bool( L, r, "enabled", out->ring.enabled );
		out->ring.radius    = xh_field_num ( L, r, "radius", out->ring.radius );
		out->ring.thickness = xh_field_num ( L, r, "thickness", out->ring.thickness );
		out->ring.filled    = xh_field_bool( L, r, "filled", out->ring.filled );
	}
	lua_pop( L, 1 );

	lua_getfield( L, t, "corners" );
	if ( lua_istable( L, -1 ) ) {
		int c = lua_gettop( L );
		out->corners.enabled    = xh_field_bool( L, c, "enabled", out->corners.enabled );
		out->corners.offset     = xh_field_num ( L, c, "offset", out->corners.offset );
		out->corners.arm_length = xh_field_num ( L, c, "arm_length", out->corners.arm_length );
		out->corners.thickness  = xh_field_num ( L, c, "thickness", out->corners.thickness );
	}
	lua_pop( L, 1 );

	lua_getfield( L, t, "outline" );
	if ( lua_istable( L, -1 ) ) {
		int o = lua_gettop( L );
		out->outline.thickness = xh_field_num( L, o, "thickness", out->outline.thickness );
		out->outline.alpha     = xh_field_num( L, o, "alpha", out->outline.alpha );
	}
	lua_pop( L, 1 );

	lua_pop( L, 1 );   /* the spec table */
}

static void WiredCrosshair_SpecDefaults( cgCrosshairDrawSpec_t *out ) {
	memset( out, 0, sizeof( *out ) );
	out->visible = qtrue;
	out->scale   = 1.0f;
	Vector4Set( out->color, 1.0f, 1.0f, 1.0f, 1.0f );
}

/* ── public ────────────────────────────────────────────────────────────*/

/* Universal weapon-switch hide-gate: while the weapon is being raised or
 * dropped, the reticle is hidden (spec: instant switch, no lingering shape).
 * Applied in C so it covers every weapon uniformly — including those with no
 * script (default base) and static weapons that never run update(). Reads the
 * cgame-staged crosshair.weapon_status (weaponstate_t). */
static void WiredCrosshair_ApplyHideGate( cgCrosshairDrawSpec_t *out ) {
	int status = (int)WiredCrosshair_StoreValue( "crosshair.weapon_status", Q3_WPSTAT_READY );
	if ( status == Q3_WPSTAT_RAISING || status == Q3_WPSTAT_DROPPING ) {
		out->visible = qfalse;
	}
}

/* Push the cached static base (or the default base) and read it into out. */
static void WiredCrosshair_ReadRef( lua_State *L, int ref, cgCrosshairDrawSpec_t *out ) {
	lua_rawgeti( L, LUA_REGISTRYINDEX, ref );
	WiredCrosshair_ReadSpec( L, out );
}

qboolean WiredCrosshair_Eval( cgCrosshairDrawSpec_t *out ) {
	lua_State            *L;
	const wuiStoreEntry_t *we;
	const char           *token;
	xhWeaponEntry_t      *e;

	if ( !out ) return qfalse;
	WiredCrosshair_SpecDefaults( out );

	if ( !xh.ready ) return qfalse;
	L = WiredScript_GetState();
	if ( !L ) return qfalse;

	we    = WiredStore_Get( "crosshair.weapon" );
	token = ( we && we->text[0] ) ? we->text : "";

	e = WiredCrosshair_Entry( L, token );

	if ( !e || e->staticRef == LUA_NOREF ) {
		/* no per-weapon base -> fall back to the default base alone */
		if ( xh.defaultBaseRef == LUA_NOREF ) return qfalse;
		WiredCrosshair_ReadRef( L, xh.defaultBaseRef, out );
	} else if ( !e->dynamic || e->updateRef == LUA_NOREF ) {
		/* static weapon: read cached base directly (§8 — no Lua call) */
		WiredCrosshair_ReadRef( L, e->staticRef, out );
	} else {
		/* dynamic: update(state) -> delta (table or nil) */
		qboolean handled = qfalse;
		if ( WiredScript_PushChunkForArgs( e->updateRef ) ) {
			WiredCrosshair_PushState( L );   /* arg 1 */
			if ( WiredScript_PCallArgs( 1, 1, "crosshair-update" ) ) {
				if ( lua_isnil( L, -1 ) || !lua_istable( L, -1 ) ) {
					/* nil delta -> reuse cached static (§5.4) */
					lua_pop( L, 1 );
				} else {
					/* delta -> deep-merge onto static, read, release */
					int deltaRef = luaL_ref( L, LUA_REGISTRYINDEX );  /* pops delta */
					int mergedRef = WiredCrosshair_DeepMergeRefs( L, e->staticRef, deltaRef );
					luaL_unref( L, LUA_REGISTRYINDEX, deltaRef );
					if ( mergedRef != LUA_NOREF ) {
						WiredCrosshair_ReadRef( L, mergedRef, out );
						luaL_unref( L, LUA_REGISTRYINDEX, mergedRef );
						handled = qtrue;
					}
				}
			}
		}
		if ( !handled ) {
			WiredCrosshair_ReadRef( L, e->staticRef, out );
		}
	}

	WiredCrosshair_ApplyHideGate( out );
	return qtrue;
}

void WiredCrosshair_Init( void ) {
	int i;

	memset( &xh, 0, sizeof( xh ) );
	xh.defaultBaseRef = LUA_NOREF;
	xh.deepMergeRef   = LUA_NOREF;
	for ( i = 0; i < XH_MAX_WEAPONS; i++ ) {
		xh.weapons[i].staticRef = LUA_NOREF;
		xh.weapons[i].updateRef = LUA_NOREF;
	}

	/* q3.* binds at PostInit; the scripts load right after (same registrar
	 * mechanism — registrars run in registration order, so q3.* is live
	 * before the scripts that reference it execute). */
	WiredScript_RegisterBindings( WiredCrosshair_RegisterQ3 );
	WiredScript_RegisterBindings( WiredCrosshair_LoadScripts );
}

void WiredCrosshair_Shutdown( void ) {
	lua_State *L = WiredScript_GetState();
	int i;
	if ( L ) {
		if ( xh.defaultBaseRef != LUA_NOREF ) luaL_unref( L, LUA_REGISTRYINDEX, xh.defaultBaseRef );
		if ( xh.deepMergeRef   != LUA_NOREF ) luaL_unref( L, LUA_REGISTRYINDEX, xh.deepMergeRef );
		for ( i = 0; i < xh.weaponCount; i++ ) {
			if ( xh.weapons[i].staticRef != LUA_NOREF ) luaL_unref( L, LUA_REGISTRYINDEX, xh.weapons[i].staticRef );
			if ( xh.weapons[i].updateRef != LUA_NOREF ) luaL_unref( L, LUA_REGISTRYINDEX, xh.weapons[i].updateRef );
		}
	}
	memset( &xh, 0, sizeof( xh ) );
}

#endif /* FEAT_WIRED_UI */
