/*
===========================================================================
Copyright (C) 2024 Wired engine contributors

This file is part of Quake III Arena source code.
Released under GPLv2 — see meta.h for the full notice.
===========================================================================
*/

// meta_remap.c -- typed remap_set_t implementation, Maps_Arena allocator,
// and the renderer/sound facing R_TryRemap* / S_TryRemap* hooks.
//
// Storage discipline: every allocation in this file is bump-allocated
// from maps_arena. The arena is reset on FS_Restart and destroyed on
// engine shutdown. There is no per-entry free; Remap_Free is a stub
// that exists only so callers have a symmetric API. Calling Remap_Free
// does NOT reclaim memory — only Maps_ResetArena / Maps_ShutdownArena
// do that.

#include "meta.h"
#include "../qcommon.h"
#include "../arena.h"
LOG_DECLARE_CHANNEL( ch_maps, "maps" );

static arena_t           *maps_arena      = NULL;
static const remap_set_t *r_activeRemapSet = NULL;
static int                r_remapBypassDepth = 0;   // recursion guard

void Maps_InitArena( void ) {
	if ( maps_arena ) return;
	maps_arena = Arena_Create( "Maps", 1u << 20 );  // 1 MB
}

void Maps_ShutdownArena( void ) {
	r_activeRemapSet   = NULL;
	r_remapBypassDepth = 0;
	if ( maps_arena ) {
		Arena_Destroy( maps_arena );
		maps_arena = NULL;
	}
}

void Maps_ResetArena( void ) {
	r_activeRemapSet   = NULL;
	r_remapBypassDepth = 0;
	if ( maps_arena ) {
		Arena_Reset( maps_arena );
	}
}

// ── FNV-1a 32-bit on the source path ────────────────────────────────────
// Matches the FNV-1a convention used elsewhere in q3now (WiredStore).
// Case-folded for filesystem-style case-insensitive matching of shader
// paths.
static unsigned int Remap_HashPath( const char *s ) {
	unsigned int hash = 0x811c9dc5u;   // FNV offset basis (32-bit)
	while ( *s ) {
		char c = *s;
		if ( c >= 'A' && c <= 'Z' ) c = (char)( c - 'A' + 'a' );
		if ( c == '\\' )            c = '/';
		hash ^= (unsigned char)c;
		hash *= 0x01000193u;          // FNV prime
		s++;
	}
	return hash;
}

remap_table_t *Remap_Create( void ) {
	if ( !maps_arena ) {
		Com_Log( SEV_WARN, LOG_CH(ch_maps),
			"Remap_Create called before Maps_InitArena\n" );
		return NULL;
	}
	remap_table_t *t = (remap_table_t *)Arena_Alloc(
		maps_arena, sizeof( remap_table_t ), _Alignof( remap_table_t ) );
	if ( !t ) return NULL;
	memset( t, 0, sizeof( *t ) );
	return t;
}

void Remap_Add( remap_table_t *t, const char *src, const char *dst ) {
	if ( !t || !src || !dst || !maps_arena ) return;
	if ( t->count >= META_MAX_REMAPS ) {
		Com_Log( SEV_DEBUG, LOG_CH(ch_maps),
			"Remap_Add: table full (%d entries), dropping '%s' -> '%s'\n",
			t->count, src, dst );
		return;
	}

	remap_entry_t *e = (remap_entry_t *)Arena_Alloc(
		maps_arena, sizeof( remap_entry_t ), _Alignof( remap_entry_t ) );
	if ( !e ) return;

	Q_strncpyz( e->src, src, sizeof( e->src ) );
	Q_strncpyz( e->dst, dst, sizeof( e->dst ) );

	unsigned int slot = Remap_HashPath( src ) & ( ARRAY_LEN( t->buckets ) - 1 );
	e->next = t->buckets[slot];
	t->buckets[slot] = e;
	t->count++;
}

const char *Remap_Lookup( const remap_table_t *t, const char *src ) {
	if ( !t || !src || !*src ) return NULL;
	unsigned int slot = Remap_HashPath( src ) & ( ARRAY_LEN( t->buckets ) - 1 );
	for ( const remap_entry_t *e = t->buckets[slot]; e; e = e->next ) {
		if ( !Q_stricmp( e->src, src ) ) {
			return e->dst;
		}
	}
	return NULL;
}

void Remap_Free( remap_table_t *t ) {
	(void)t;
	// Arena-backed; reclaimed via Maps_ResetArena / Maps_ShutdownArena.
}

// ── remap_set_t typed container ────────────────────────────────────────

const char *RemapSet_Lookup( const remap_set_t *s, remap_kind_t kind, const char *src ) {
	if ( !s || kind < 0 || kind >= REMAP_KIND_COUNT ) return NULL;
	return Remap_Lookup( s->tables[kind], src );
}

void RemapSet_Add( remap_set_t *s, remap_kind_t kind, const char *src, const char *dst ) {
	if ( !s || kind < 0 || kind >= REMAP_KIND_COUNT ) return;
	if ( !s->tables[kind] ) {
		s->tables[kind] = Remap_Create();
		if ( !s->tables[kind] ) return;
	}
	Remap_Add( s->tables[kind], src, dst );
}

qboolean RemapSet_IsEmpty( const remap_set_t *s ) {
	if ( !s ) return qtrue;
	for ( int k = 0; k < REMAP_KIND_COUNT; k++ ) {
		if ( s->tables[k] && s->tables[k]->count > 0 ) return qfalse;
	}
	return qtrue;
}

// ── Active remap (renderer / sound hook) ───────────────────────────────

void R_SetActiveRemapSet( const remap_set_t *s ) {
	r_activeRemapSet   = s;
	r_remapBypassDepth = 0;
}

static const char *TryRemap( remap_kind_t kind, const char *name ) {
	if ( r_remapBypassDepth > 0 ) return NULL;
	if ( !r_activeRemapSet )      return NULL;
	if ( !name || !*name )        return NULL;
	return RemapSet_Lookup( r_activeRemapSet, kind, name );
}

const char *R_TryRemapShader ( const char *name ) { return TryRemap( REMAP_KIND_SHADER,  name ); }
const char *R_TryRemapTexture( const char *name ) { return TryRemap( REMAP_KIND_TEXTURE, name ); }
const char *S_TryRemapSound  ( const char *name ) { return TryRemap( REMAP_KIND_SOUND,   name ); }
const char *S_TryRemapMusic  ( const char *name ) { return TryRemap( REMAP_KIND_MUSIC,   name ); }

void R_PushNullRemap( void ) {
	r_remapBypassDepth++;
}

void R_PopRemap( void ) {
	if ( r_remapBypassDepth > 0 ) r_remapBypassDepth--;
}

// Adapter exposed to the renderer DLL via refimport_t.MetaRemap_Lookup.
// Takes int (not remap_kind_t) so the function pointer signature in
// tr_public.h matches without forcing renderer code to include meta.h.
// Out-of-range kinds map to NULL via the bounds check in TryRemap.
const char *MetaRemap_LookupAdapter( int kind, const char *name ) {
	return TryRemap( (remap_kind_t)kind, name );
}
