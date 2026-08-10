// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026-present Wired Engine contributors

/*
===========================================================================
cl_wired_l10n.c -- localization table loader (l10n key -> localized text).

A scripts/l10n/<lang>.lua returns a flat table { ["<key>"] = "<text>", ... }.
This compiles the chunk on the System VM (WiredScript_CompileChunk + PCallArgs,
0 args / 1 result — the same load idiom as cl_wired_scene_lua.c / wired_scene.c
and cl_wired_crosshair.c), then walks the returned table with lua_next and
copies every string key -> string value pair into a plain-C chained hash. The
Lua VM is used ONLY here at load time; WiredL10n_Get is a plain-C hash lookup
with no lua_* call.

Client-side (System VM) because the caption consumer, cgame CG_SceneView, runs
in the WASM VM which has no lua_State; a later phase bridges its captionKey
through WiredL10n_Get here on the engine side.

Language is chosen by the cl_language cvar (default "en"). l10n_reload rereads
scripts/l10n/<cl_language>.lua; the table is also reloaded automatically when
the System VM re-inits (vid_restart re-fires the PostInit registrars). The old
table is freed on every reload.
===========================================================================
*/

#include "../../client.h"
#include "../../../qcommon/wired/core/scripting/wired_scripting.h"
#include "cl_wired_l10n.h"

LOG_DECLARE_CHANNEL( ch_client, "client" );

#if FEAT_WIRED_UI

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

/* ── plain-C key -> text hash (no per-lookup Lua) ──────────────────────── */

#define L10N_HASH_SIZE 256   /* power of two; buckets chained on collision */

typedef struct l10nEntry_s {
	char               *key;    /* heap-owned (CopyString) */
	char               *text;   /* heap-owned (CopyString) */
	struct l10nEntry_s *next;   /* bucket chain */
} l10nEntry_t;

typedef struct {
	l10nEntry_t *buckets[L10N_HASH_SIZE];
	int          count;         /* number of keys loaded */
	qboolean     initialized;   /* cvar/command registered + queued */
	char         loadedLang[64];/* language of the table currently held */
} l10nState_t;

static l10nState_t l10n;

static cvar_t *cl_language;

/* djb2 string hash, masked to the bucket count. */
static unsigned int WiredL10n_Hash( const char *s ) {
	unsigned int h = 5381;
	while ( *s ) {
		h = ( ( h << 5 ) + h ) + (unsigned char)*s;   /* h*33 + c */
		s++;
	}
	return h & ( L10N_HASH_SIZE - 1 );
}

/* Free every entry and clear the buckets (leaves the state struct valid). */
static void WiredL10n_Clear( void ) {
	int i;
	for ( i = 0; i < L10N_HASH_SIZE; i++ ) {
		l10nEntry_t *e = l10n.buckets[i];
		while ( e ) {
			l10nEntry_t *next = e->next;
			Z_Free( e->key );
			Z_Free( e->text );
			Z_Free( e );
			e = next;
		}
		l10n.buckets[i] = NULL;
	}
	l10n.count = 0;
	l10n.loadedLang[0] = '\0';
}

/* Insert (or overwrite) key -> text. Both strings are copied. A duplicate key
 * (last-wins) replaces the existing text so a later table entry can override an
 * earlier one deterministically. */
static void WiredL10n_Insert( const char *key, const char *text ) {
	unsigned int  b = WiredL10n_Hash( key );
	l10nEntry_t  *e;

	for ( e = l10n.buckets[b]; e; e = e->next ) {
		if ( strcmp( e->key, key ) == 0 ) {
			Z_Free( e->text );
			e->text = CopyString( text );
			return;
		}
	}

	e       = (l10nEntry_t *)Z_Malloc( sizeof( *e ) );
	e->key  = CopyString( key );
	e->text = CopyString( text );
	e->next = l10n.buckets[b];
	l10n.buckets[b] = e;
	l10n.count++;
}

/* ── table load (System VM, load-time only) ───────────────────────────── */

/* Walk the table on top of the System VM stack, copying every string-key ->
 * string-value pair into the hash. Non-string keys/values are skipped with a
 * warning (the l10n table is key->text; anything else is an authoring error).
 * Leaves the stack as it found it. Returns the number of pairs read. */
static int WiredL10n_ReadTable( lua_State *L, const char *path ) {
	int t = lua_gettop( L );   /* the table's stack index */
	int read = 0;

	lua_pushnil( L );          /* first key */
	while ( lua_next( L, t ) != 0 ) {
		/* key at -2, value at -1. lua_next requires the key be left UNMODIFIED
		 * on the stack for the next iteration. lua_tostring coerces a number to
		 * a string IN PLACE, which would corrupt a numeric key and break the
		 * iteration — so the key is gated on lua_type == LUA_TSTRING (strict, no
		 * number coercion), never lua_isstring (which accepts numbers). The
		 * value is popped each round, so lua_isstring (string or number) is safe
		 * there. */
		if ( lua_type( L, -2 ) == LUA_TSTRING && lua_isstring( L, -1 ) ) {
			const char *key  = lua_tostring( L, -2 );
			const char *text = lua_tostring( L, -1 );
			WiredL10n_Insert( key, text );
			read++;
		} else {
			Com_Log( SEV_WARN, LOG_CH(ch_client),
			         "l10n: '%s' has a non-string key or value (skipped)\n", path );
		}
		lua_pop( L, 1 );       /* pop value, keep key for lua_next */
	}
	return read;
}

/* Compile scripts/l10n/<lang>.lua, pcall it (0 args, 1 result) so the returned
 * table is on top of the System VM stack, and read it into the hash. Mirrors
 * WiredScene_LoadFromFile / WiredCrosshair_RunModule. Returns qtrue on a table
 * that loaded (even if empty); qfalse on missing file / parse / non-table. */
static qboolean WiredL10n_LoadFile( const char *path, const char *lang ) {
	fileHandle_t f;
	int          len;
	char        *buf;
	int          ref;
	lua_State   *L;

	len = FS_FOpenFileRead( path, &f, qfalse );
	if ( len <= 0 || f == 0 ) {
		if ( f ) FS_FCloseFile( f );
		Com_Log( SEV_WARN, LOG_CH(ch_client), "l10n: '%s' not found\n", path );
		return qfalse;
	}

	buf = Z_Malloc( len + 1 );
	FS_Read( buf, len, f );
	buf[len] = '\0';
	FS_FCloseFile( f );

	ref = WiredScript_CompileChunk( buf, path );
	Z_Free( buf );
	if ( ref == WIRED_CHUNK_NOREF ) {
		return qfalse;   /* CompileChunk already logged SEV_ERROR */
	}

	L = WiredScript_PushChunkForArgs( ref );
	if ( !L ) {
		WiredScript_ReleaseChunk( ref );
		return qfalse;
	}
	if ( !WiredScript_PCallArgs( 0, 1, path ) ) {
		WiredScript_ReleaseChunk( ref );
		return qfalse;
	}
	WiredScript_ReleaseChunk( ref );   /* chunk fn released; result on the stack */

	if ( !lua_istable( L, -1 ) ) {
		Com_Log( SEV_ERROR, LOG_CH(ch_client), "l10n: '%s' did not return a table\n", path );
		lua_pop( L, 1 );
		return qfalse;
	}

	WiredL10n_ReadTable( L, path );
	lua_pop( L, 1 );                   /* pop the result table */

	Q_strncpyz( l10n.loadedLang, lang, sizeof( l10n.loadedLang ) );
	return qtrue;
}

/* ── public: reload / lookup ──────────────────────────────────────────── */

void WiredL10n_Reload( void ) {
	const char *lang = ( cl_language && cl_language->string[0] )
	                   ? cl_language->string : "en";
	char        path[MAX_QPATH];

	WiredL10n_Clear();   /* free the old table first (no leak across reloads) */

	Com_sprintf( path, sizeof( path ), "scripts/l10n/%s.lua", lang );
	if ( WiredL10n_LoadFile( path, lang ) ) {
		Com_Log( SEV_INFO, LOG_CH(ch_client),
		         "l10n: loaded %d keys from %s\n", l10n.count, path );
	} else {
		Com_Log( SEV_WARN, LOG_CH(ch_client),
		         "l10n: no table for language '%s' (lookups fall back to the key)\n", lang );
	}
}

const char *WiredL10n_Get( const char *key ) {
	unsigned int  b;
	l10nEntry_t  *e;

	if ( !key || !*key ) {
		return key ? key : "";
	}

	b = WiredL10n_Hash( key );
	for ( e = l10n.buckets[b]; e; e = e->next ) {
		if ( strcmp( e->key, key ) == 0 ) {
			return e->text;
		}
	}
	return key;   /* graceful fallback: missing translation shows the raw key */
}

/* ── init / shutdown ──────────────────────────────────────────────────── */

/* l10n_reload console command: reread scripts/l10n/<cl_language>.lua. */
static void WiredL10n_Reload_f( void ) {
	WiredL10n_Reload();
}

/* PostInit registrar: the initial table load, run against the live System VM in
 * the same window as the crosshair/scene loaders. The System VM is process-scoped
 * (created once in Com_Init, not re-created by vid_restart), so a language change
 * is picked up via the l10n_reload command rather than a VM re-init. */
static void WiredL10n_LoadRegistrar( lua_State *L ) {
	(void)L;   /* the load uses the System VM through WiredScript_* helpers */
	WiredL10n_Reload();
}

void WiredL10n_Init( void ) {
	if ( l10n.initialized ) {
		return;
	}
	memset( &l10n, 0, sizeof( l10n ) );

	/* Language selector. "en" is the base language shipped with the fixtures. */
	cl_language = Cvar_Get( "cl_language", "en", CVAR_ARCHIVE );

	Cmd_AddCommand( "l10n_reload", WiredL10n_Reload_f );

	/* Load at PostInit alongside the other Lua-content loaders. */
	WiredScript_RegisterBindings( WiredL10n_LoadRegistrar );

	l10n.initialized = qtrue;
}

void WiredL10n_Shutdown( void ) {
	WiredL10n_Clear();
	if ( l10n.initialized ) {
		Cmd_RemoveCommand( "l10n_reload" );
	}
	memset( &l10n, 0, sizeof( l10n ) );
}

#endif /* FEAT_WIRED_UI */
