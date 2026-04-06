/*
===========================================================================
cl_wired_store.c — Wired UI Store: generic key-value state bridge

Phase 4: Game-agnostic state store replacing monolithic wiredHudState_t.
cgame writes via staging buffer + batch syscall, client reads at render time.
===========================================================================
*/

#include "../client.h"
#include "cl_wired_store.h"

#include <ctype.h>
#include <stdlib.h>

#if FEAT_WIRED_UI

/* ── file-scope state ───────────────────────────────────────────────── */

static wuiStore_t wired_store;

/* ── hash function (FNV-1a 32-bit, case-insensitive) ────────────────── */

static unsigned int WiredStore_Hash( const char *key ) {
	unsigned int hash = 2166136261u;  /* FNV offset basis */
	const char *p;

	for ( p = key; *p; p++ ) {
		hash ^= (unsigned char)tolower( *p );
		hash *= 16777619u;           /* FNV prime */
	}
	return hash % WUI_STORE_BUCKETS;
}

/* ── core API ───────────────────────────────────────────────────────── */

/*
==================
WiredStore_Get

O(1) average lookup. Returns NULL if the key is not found.
==================
*/
wuiStoreEntry_t *WiredStore_Get( const char *key ) {
	unsigned int bucket;
	wuiStoreEntry_t *e;

	bucket = WiredStore_Hash( key );
	e = wired_store.buckets[bucket];

	while ( e ) {
		if ( Q_stricmp( e->key, key ) == 0 ) {
			return e;
		}
		e = e->next;
	}

	return NULL;
}

/*
==================
WiredStore_Set

Get-or-create: returns existing entry if found, otherwise allocates
from pool and links into the hash chain head.
==================
*/
wuiStoreEntry_t *WiredStore_Set( const char *key ) {
	wuiStoreEntry_t *e;
	unsigned int bucket;
	int i;

	e = WiredStore_Get( key );
	if ( e ) {
		return e;
	}

	if ( wired_store.numEntries >= WUI_STORE_MAX_ENTRIES ) {
		Com_Printf( "WiredStore: WARNING — pool exhausted (%d entries)\n", WUI_STORE_MAX_ENTRIES );
		return NULL;
	}

	/* find a free slot in the pool */
	for ( i = 0; i < WUI_STORE_MAX_ENTRIES; i++ ) {
		if ( wired_store.pool[i].key[0] == '\0' ) {
			e = &wired_store.pool[i];
			break;
		}
	}

	if ( !e ) {
		Com_Printf( "WiredStore: WARNING — no free pool slot found\n" );
		return NULL;
	}

	Q_strncpyz( e->key, key, sizeof( e->key ) );

	/* link into bucket chain head */
	bucket = WiredStore_Hash( key );
	e->next = wired_store.buckets[bucket];
	wired_store.buckets[bucket] = e;

	wired_store.numEntries++;

	return e;
}

/*
==================
WiredStore_Delete

Remove entry from hash chain, clear slot, decrement count.
==================
*/
void WiredStore_Delete( const char *key ) {
	unsigned int bucket;
	wuiStoreEntry_t *e, *prev;

	bucket = WiredStore_Hash( key );
	prev = NULL;
	e = wired_store.buckets[bucket];

	while ( e ) {
		if ( Q_stricmp( e->key, key ) == 0 ) {
			/* unlink from chain */
			if ( prev ) {
				prev->next = e->next;
			} else {
				wired_store.buckets[bucket] = e->next;
			}

			/* clear the pool slot */
			Com_Memset( e, 0, sizeof( *e ) );
			wired_store.numEntries--;
			return;
		}
		prev = e;
		e = e->next;
	}
}

/*
==================
WiredStore_Clear

Zero all entries and buckets but keep console commands registered.
==================
*/
void WiredStore_Clear( void ) {
	Com_Memset( wired_store.buckets, 0, sizeof( wired_store.buckets ) );
	Com_Memset( wired_store.pool, 0, sizeof( wired_store.pool ) );
	wired_store.numEntries = 0;
	wired_store.generation = 0;
}

/*
==================
WiredStore_BeginFrame

Increment generation. For each active entry: report watched+dirty,
then clear dirty flag.
==================
*/
void WiredStore_BeginFrame( void ) {
	int i;

	wired_store.generation++;

	for ( i = 0; i < WUI_STORE_MAX_ENTRIES; i++ ) {
		wuiStoreEntry_t *e = &wired_store.pool[i];

		if ( e->key[0] == '\0' ) {
			continue;
		}

		if ( ( e->flags & WUI_STORE_FLAG_DIRTY ) && ( e->flags & WUI_STORE_FLAG_WATCHED ) ) {
			Com_Printf( "WiredStore [watch] %s = \"%s\" (%.2f) [%s]\n",
				e->key, e->text, e->value, e->state );
		}

		e->flags &= ~WUI_STORE_FLAG_DIRTY;
	}
}

/*
==================
WiredStore_ForEach

Iterate all active entries whose key starts with prefix.
prefix="" or NULL matches everything.
==================
*/
void WiredStore_ForEach( const char *prefix,
                         void (*fn)( wuiStoreEntry_t *entry, void *userData ),
                         void *userData ) {
	int i;
	int prefixLen;

	if ( !fn ) return;
	prefixLen = ( prefix && prefix[0] ) ? (int)strlen( prefix ) : 0;

	for ( i = 0; i < WUI_STORE_MAX_ENTRIES; i++ ) {
		wuiStoreEntry_t *e = &wired_store.pool[i];
		if ( e->key[0] == '\0' ) continue;
		if ( prefixLen > 0 && Q_stricmpn( e->key, prefix, prefixLen ) != 0 ) continue;
		fn( e, userData );
	}
}

/* ── qsort comparison for key list ──────────────────────────────────── */

static int WiredStore_KeyCmp( const void *a, const void *b ) {
	const char * const *ka = (const char * const *)a;
	const char * const *kb = (const char * const *)b;
	return Q_stricmp( *ka, *kb );
}

/* ── console commands ───────────────────────────────────────────────── */

/*
==================
WiredStore_Cmd_Get

Usage: wui_store_get <key>
==================
*/
static void WiredStore_Cmd_Get( void ) {
	const char *key;
	wuiStoreEntry_t *e;

	if ( Cmd_Argc() < 2 ) {
		Com_Printf( "Usage: wui_store_get <key>\n" );
		return;
	}

	key = Cmd_Argv( 1 );
	e = WiredStore_Get( key );

	if ( !e ) {
		Com_Printf( "Key not found.\n" );
		return;
	}

	Com_Printf( "  key:   %s\n", e->key );
	Com_Printf( "  text:  \"%s\"\n", e->text );
	Com_Printf( "  value: %.4f\n", e->value );
	Com_Printf( "  color: %.2f %.2f %.2f %.2f\n", e->color[0], e->color[1], e->color[2], e->color[3] );
	Com_Printf( "  icon:  %d\n", e->icon );
	Com_Printf( "  state: \"%s\"\n", e->state );
	Com_Printf( "  flags: 0x%x\n", e->flags );
	Com_Printf( "  gen:   %d\n", e->generation );
}

/*
==================
WiredStore_Cmd_Dump

Prints all entries in bucket order.
==================
*/
static void WiredStore_Cmd_Dump( void ) {
	int i, count;

	count = 0;
	for ( i = 0; i < WUI_STORE_BUCKETS; i++ ) {
		wuiStoreEntry_t *e = wired_store.buckets[i];
		while ( e ) {
			Com_Printf( "[%3d] %-40s text=\"%s\" value=%.2f state=\"%s\"\n",
				i, e->key, e->text, e->value, e->state );
			count++;
			e = e->next;
		}
	}

	Com_Printf( "WiredStore: %d entries dumped\n", count );
}

/*
==================
WiredStore_Cmd_List

Prints all keys sorted alphabetically.
==================
*/
static void WiredStore_Cmd_List( void ) {
	const char *keys[WUI_STORE_MAX_ENTRIES];
	int count, i;

	count = 0;
	for ( i = 0; i < WUI_STORE_MAX_ENTRIES; i++ ) {
		if ( wired_store.pool[i].key[0] ) {
			keys[count++] = wired_store.pool[i].key;
		}
	}

	qsort( keys, count, sizeof( keys[0] ), WiredStore_KeyCmp );

	for ( i = 0; i < count; i++ ) {
		Com_Printf( "  %s\n", keys[i] );
	}

	Com_Printf( "WiredStore: %d keys\n", count );
}

/*
==================
WiredStore_Cmd_Watch

Usage: wui_store_watch <key>
Toggles the WATCHED flag on the entry.
==================
*/
static void WiredStore_Cmd_Watch( void ) {
	const char *key;
	wuiStoreEntry_t *e;

	if ( Cmd_Argc() < 2 ) {
		Com_Printf( "Usage: wui_store_watch <key>\n" );
		return;
	}

	key = Cmd_Argv( 1 );
	e = WiredStore_Get( key );

	if ( !e ) {
		Com_Printf( "Key not found.\n" );
		return;
	}

	e->flags ^= WUI_STORE_FLAG_WATCHED;

	if ( e->flags & WUI_STORE_FLAG_WATCHED ) {
		Com_Printf( "WiredStore: watching \"%s\"\n", key );
	} else {
		Com_Printf( "WiredStore: unwatching \"%s\"\n", key );
	}
}

/*
==================
WiredStore_Cmd_Stats

Prints bucket distribution statistics.
==================
*/
static void WiredStore_Cmd_Stats( void ) {
	int emptyBuckets, maxChain, nonEmptyBuckets, i;
	float totalChain;

	emptyBuckets = 0;
	maxChain = 0;
	totalChain = 0.0f;
	nonEmptyBuckets = 0;

	for ( i = 0; i < WUI_STORE_BUCKETS; i++ ) {
		int chainLen;
		wuiStoreEntry_t *e;

		chainLen = 0;
		e = wired_store.buckets[i];

		while ( e ) {
			chainLen++;
			e = e->next;
		}

		if ( chainLen == 0 ) {
			emptyBuckets++;
		} else {
			nonEmptyBuckets++;
			totalChain += chainLen;
		}

		if ( chainLen > maxChain ) {
			maxChain = chainLen;
		}
	}

	Com_Printf( "WiredStore stats:\n" );
	Com_Printf( "  Entries: %d / %d\n", wired_store.numEntries, WUI_STORE_MAX_ENTRIES );
	Com_Printf( "  Buckets: %d (%d empty)\n", WUI_STORE_BUCKETS, emptyBuckets );
	Com_Printf( "  Max chain: %d\n", maxChain );
	Com_Printf( "  Avg chain: %.2f\n", nonEmptyBuckets ? totalChain / nonEmptyBuckets : 0.0f );
	Com_Printf( "  Load factor: %.2f\n", (float)wired_store.numEntries / WUI_STORE_BUCKETS );
}

/* ── init / shutdown ────────────────────────────────────────────────── */

/*
==================
WiredStore_Init
==================
*/
void WiredStore_Init( void ) {
	Com_Memset( &wired_store, 0, sizeof( wired_store ) );

	Cmd_AddCommand( "wui_store_get", WiredStore_Cmd_Get );
	Cmd_AddCommand( "wui_store_dump", WiredStore_Cmd_Dump );
	Cmd_AddCommand( "wui_store_list", WiredStore_Cmd_List );
	Cmd_AddCommand( "wui_store_watch", WiredStore_Cmd_Watch );
	Cmd_AddCommand( "wui_store_stats", WiredStore_Cmd_Stats );

	Com_Printf( "WiredStore: initialized (%d buckets, %d max entries)\n", WUI_STORE_BUCKETS, WUI_STORE_MAX_ENTRIES );
}

/*
==================
WiredStore_Shutdown
==================
*/
void WiredStore_Shutdown( void ) {
	Cmd_RemoveCommand( "wui_store_get" );
	Cmd_RemoveCommand( "wui_store_dump" );
	Cmd_RemoveCommand( "wui_store_list" );
	Cmd_RemoveCommand( "wui_store_watch" );
	Cmd_RemoveCommand( "wui_store_stats" );

	Com_Memset( &wired_store, 0, sizeof( wired_store ) );
}

#endif /* FEAT_WIRED_UI */
