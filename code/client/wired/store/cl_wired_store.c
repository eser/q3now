// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
===========================================================================
cl_wired_store.c — Wired UI Store: generic key-value state bridge

Game-agnostic state store replacing monolithic wiredHudState_t.
cgame writes via staging buffer + batch syscall, client reads at render time.
===========================================================================
*/

#include "../../client.h"
#include "cl_wired_store.h"

#include <ctype.h>
#include <stdlib.h>
LOG_DECLARE_CHANNEL( ch_client, "client" );

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
	unsigned int bucket = WiredStore_Hash( key );
	wuiStoreEntry_t *e = wired_store.buckets[bucket];

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
	wuiStoreEntry_t *e = WiredStore_Get( key );
	if ( e ) {
		return e;
	}

	if ( wired_store.numEntries >= WUI_STORE_MAX_ENTRIES ) {
		Com_Log( SEV_INFO, LOG_CH(ch_client), "WiredStore: WARNING — pool exhausted (%d entries)\n", WUI_STORE_MAX_ENTRIES );
		return NULL;
	}

	/* find a free slot in the pool */
	for ( int i = 0; i < WUI_STORE_MAX_ENTRIES; i++ ) {
		if ( wired_store.pool[i].key[0] == '\0' ) {
			e = &wired_store.pool[i];
			break;
		}
	}

	if ( !e ) {
		Com_Log( SEV_INFO, LOG_CH(ch_client), "WiredStore: WARNING — no free pool slot found\n" );
		return NULL;
	}

	Q_strncpyz( e->key, key, sizeof( e->key ) );

	/* link into bucket chain head */
	unsigned int bucket = WiredStore_Hash( key );
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
	unsigned int bucket = WiredStore_Hash( key );
	wuiStoreEntry_t *prev = NULL;
	wuiStoreEntry_t *e = wired_store.buckets[bucket];

	while ( e ) {
		if ( Q_stricmp( e->key, key ) == 0 ) {
			/* unlink from chain */
			if ( prev ) {
				prev->next = e->next;
			} else {
				wired_store.buckets[bucket] = e->next;
			}

			/* clear the pool slot */
			memset( e, 0, sizeof( *e ) );
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
	memset( wired_store.buckets, 0, sizeof( wired_store.buckets ) );
	memset( wired_store.pool, 0, sizeof( wired_store.pool ) );
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
	wired_store.generation++;

	for ( int i = 0; i < WUI_STORE_MAX_ENTRIES; i++ ) {
		wuiStoreEntry_t *e = &wired_store.pool[i];

		if ( e->key[0] == '\0' ) {
			continue;
		}

		if ( ( e->flags & WUI_STORE_FLAG_DIRTY ) && ( e->flags & WUI_STORE_FLAG_WATCHED ) ) {
			Com_Log( SEV_INFO, LOG_CH(ch_client), "WiredStore [watch] %s = \"%s\" (%.2f) [%s]\n",
				e->key, e->text, e->value, e->state );
		}

		e->flags &= ~WUI_STORE_FLAG_DIRTY;
	}

	/* WA-2a: marker lists are frame-transient — zero every list's count so a
	 * listKey that cgame does NOT push this frame draws nothing (no stale
	 * markers). The slot (and its key) is retained for cheap re-fill; only the
	 * count is reset. A PushMarkerList this frame overwrites count + markers. */
	for ( int i = 0; i < WUI_MAX_MARKER_LISTS; i++ ) {
		wired_store.markerLists[i].count = 0;
	}
}

/*
==================
WiredStore_SetMarkerList

WA-2a: REPLACE the listKey's marker list for this frame. Find-or-alloc the named
slot, copy up to WUI_MAX_MARKERS_PER_LIST markers (logged drop on overflow).
==================
*/
void WiredStore_SetMarkerList( const char *listKey, const wuiMarker_t *markers, int count ) {
	wuiMarkerList_t *list = NULL;
	int i, free = -1;

	if ( !listKey || !listKey[0] ) {
		return;
	}

	/* find existing slot for this key, remembering the first free one */
	for ( i = 0; i < WUI_MAX_MARKER_LISTS; i++ ) {
		if ( wired_store.markerLists[i].key[0] == '\0' ) {
			if ( free < 0 ) free = i;
			continue;
		}
		if ( Q_stricmp( wired_store.markerLists[i].key, listKey ) == 0 ) {
			list = &wired_store.markerLists[i];
			break;
		}
	}

	if ( !list ) {
		if ( free < 0 ) {
			Com_Log( SEV_INFO, LOG_CH(ch_client),
				"WiredStore: WARNING — marker-list slots full (%d), dropping list '%s'\n",
				WUI_MAX_MARKER_LISTS, listKey );
			return;
		}
		list = &wired_store.markerLists[free];
		Q_strncpyz( list->key, listKey, sizeof( list->key ) );
	}

	if ( count < 0 ) count = 0;
	if ( count > WUI_MAX_MARKERS_PER_LIST ) {
		Com_Log( SEV_INFO, LOG_CH(ch_client),
			"WiredStore: WARNING — marker list '%s' over cap (%d > %d), truncating\n",
			listKey, count, WUI_MAX_MARKERS_PER_LIST );
		count = WUI_MAX_MARKERS_PER_LIST;
	}

	if ( count > 0 && markers ) {
		memcpy( list->markers, markers, count * sizeof( wuiMarker_t ) );
	}
	list->count = count;
}

/*
==================
WiredStore_GetMarkerList

WA-2a: return the marker array for listKey (NULL + *outCount=0 if absent/empty).
The text element iterates this each frame (stateless — no per-marker cache).
==================
*/
const wuiMarker_t *WiredStore_GetMarkerList( const char *listKey, int *outCount ) {
	int i;
	if ( outCount ) *outCount = 0;
	if ( !listKey || !listKey[0] ) {
		return NULL;
	}
	for ( i = 0; i < WUI_MAX_MARKER_LISTS; i++ ) {
		if ( wired_store.markerLists[i].key[0] &&
		     Q_stricmp( wired_store.markerLists[i].key, listKey ) == 0 ) {
			if ( wired_store.markerLists[i].count <= 0 ) {
				return NULL;
			}
			if ( outCount ) *outCount = wired_store.markerLists[i].count;
			return wired_store.markerLists[i].markers;
		}
	}
	return NULL;
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
	if ( !fn ) return;
	int prefixLen = ( prefix && prefix[0] ) ? (int)strlen( prefix ) : 0;

	for ( int i = 0; i < WUI_STORE_MAX_ENTRIES; i++ ) {
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
		Com_Log( SEV_INFO, LOG_CH(ch_client), "Usage: wui_store_get <key>\n" );
		return;
	}

	key = Cmd_Argv( 1 );
	e = WiredStore_Get( key );

	if ( !e ) {
		Com_Log( SEV_INFO, LOG_CH(ch_client), "Key not found.\n" );
		return;
	}

	Com_Log( SEV_INFO, LOG_CH(ch_client), "  key:   %s\n", e->key );
	Com_Log( SEV_INFO, LOG_CH(ch_client), "  text:  \"%s\"\n", e->text );
	Com_Log( SEV_INFO, LOG_CH(ch_client), "  value: %.4f\n", e->value );
	Com_Log( SEV_INFO, LOG_CH(ch_client), "  color: %.2f %.2f %.2f %.2f\n", e->color[0], e->color[1], e->color[2], e->color[3] );
	Com_Log( SEV_INFO, LOG_CH(ch_client), "  icon:  %d\n", e->icon );
	Com_Log( SEV_INFO, LOG_CH(ch_client), "  state: \"%s\"\n", e->state );
	Com_Log( SEV_INFO, LOG_CH(ch_client), "  flags: 0x%x\n", e->flags );
	Com_Log( SEV_INFO, LOG_CH(ch_client), "  gen:   %d\n", e->generation );
}

/*
==================
WiredStore_Cmd_Fingerprint

Privacy-bounded Store evidence for automated behavior gates.  This proves the
exact byte sequence reached the authoritative Store without echoing editfield
contents (which may intentionally contain command-injection probes).

Usage: wui_store_fingerprint <key>
==================
*/
static void WiredStore_Cmd_Fingerprint( void ) {
	const char *key;
	const unsigned char *p;
	wuiStoreEntry_t *e;
	unsigned int hash = 2166136261u;
	int length;

	if ( Cmd_Argc() != 2 ) {
		Com_Log( SEV_INFO, LOG_CH(ch_client), "Usage: wui_store_fingerprint <key>\n" );
		return;
	}

	key = Cmd_Argv( 1 );
	e = WiredStore_Get( key );
	if ( !e ) {
		Com_Log( SEV_INFO, LOG_CH(ch_client),
			"WiredStore fingerprint: key=%s missing\n", key );
		return;
	}

	length = (int)strlen( e->text );
	for ( p = (const unsigned char *)e->text; *p; p++ ) {
		hash ^= *p;
		hash *= 16777619u;
	}
	Com_Log( SEV_INFO, LOG_CH(ch_client),
		"WiredStore fingerprint: key=%s length=%d fnv1a32=%08x\n",
		e->key, length, hash );
}

/*
==================
WiredStore_Cmd_Dump

Prints all entries in bucket order.
==================
*/
static void WiredStore_Cmd_Dump( void ) {
	int count = 0;
	for ( int i = 0; i < WUI_STORE_BUCKETS; i++ ) {
		wuiStoreEntry_t *e = wired_store.buckets[i];
		while ( e ) {
			Com_Log( SEV_INFO, LOG_CH(ch_client), "[%3d] %-40s text=\"%s\" value=%.2f state=\"%s\"\n",
				i, e->key, e->text, e->value, e->state );
			count++;
			e = e->next;
		}
	}

	Com_Log( SEV_INFO, LOG_CH(ch_client), "WiredStore: %d entries dumped\n", count );
}

/*
==================
WiredStore_Cmd_List

Prints all keys sorted alphabetically.
==================
*/
static void WiredStore_Cmd_List( void ) {
	const char *keys[WUI_STORE_MAX_ENTRIES];
	int count = 0;
	for ( int i = 0; i < WUI_STORE_MAX_ENTRIES; i++ ) {
		if ( wired_store.pool[i].key[0] ) {
			keys[count++] = wired_store.pool[i].key;
		}
	}

	qsort( keys, count, sizeof( keys[0] ), WiredStore_KeyCmp );

	for ( int i = 0; i < count; i++ ) {
		Com_Log( SEV_INFO, LOG_CH(ch_client), "  %s\n", keys[i] );
	}

	Com_Log( SEV_INFO, LOG_CH(ch_client), "WiredStore: %d keys\n", count );
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
		Com_Log( SEV_INFO, LOG_CH(ch_client), "Usage: wui_store_watch <key>\n" );
		return;
	}

	key = Cmd_Argv( 1 );
	e = WiredStore_Get( key );

	if ( !e ) {
		Com_Log( SEV_INFO, LOG_CH(ch_client), "Key not found.\n" );
		return;
	}

	e->flags ^= WUI_STORE_FLAG_WATCHED;

	if ( e->flags & WUI_STORE_FLAG_WATCHED ) {
		Com_Log( SEV_INFO, LOG_CH(ch_client), "WiredStore: watching \"%s\"\n", key );
	} else {
		Com_Log( SEV_INFO, LOG_CH(ch_client), "WiredStore: unwatching \"%s\"\n", key );
	}
}

/*
==================
WiredStore_Cmd_Stats

Prints bucket distribution statistics.
==================
*/
static void WiredStore_Cmd_Stats( void ) {
	int emptyBuckets = 0;
	int maxChain = 0;
	float totalChain = 0.0f;
	int nonEmptyBuckets = 0;

	for ( int i = 0; i < WUI_STORE_BUCKETS; i++ ) {
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

	Com_Log( SEV_INFO, LOG_CH(ch_client), "WiredStore stats:\n" );
	Com_Log( SEV_INFO, LOG_CH(ch_client), "  Entries: %d / %d\n", wired_store.numEntries, WUI_STORE_MAX_ENTRIES );
	Com_Log( SEV_INFO, LOG_CH(ch_client), "  Buckets: %d (%d empty)\n", WUI_STORE_BUCKETS, emptyBuckets );
	Com_Log( SEV_INFO, LOG_CH(ch_client), "  Max chain: %d\n", maxChain );
	Com_Log( SEV_INFO, LOG_CH(ch_client), "  Avg chain: %.2f\n", nonEmptyBuckets ? totalChain / nonEmptyBuckets : 0.0f );
	Com_Log( SEV_INFO, LOG_CH(ch_client), "  Load factor: %.2f\n", (float)wired_store.numEntries / WUI_STORE_BUCKETS );
}

/* ── init / shutdown ────────────────────────────────────────────────── */

/*
==================
WiredStore_Init
==================
*/
void WiredStore_Init( void ) {
	memset( &wired_store, 0, sizeof( wired_store ) );

	Cmd_AddCommand( "wui_store_get", WiredStore_Cmd_Get );
	Cmd_AddCommand( "wui_store_fingerprint", WiredStore_Cmd_Fingerprint );
	Cmd_AddCommand( "wui_store_dump", WiredStore_Cmd_Dump );
	Cmd_AddCommand( "wui_store_list", WiredStore_Cmd_List );
	Cmd_AddCommand( "wui_store_watch", WiredStore_Cmd_Watch );
	Cmd_AddCommand( "wui_store_stats", WiredStore_Cmd_Stats );

	Com_Log( SEV_INFO, LOG_CH(ch_client), "WiredStore: initialized (%d buckets, %d max entries)\n", WUI_STORE_BUCKETS, WUI_STORE_MAX_ENTRIES );
}

/*
==================
WiredStore_Shutdown
==================
*/
void WiredStore_Shutdown( void ) {
	Cmd_RemoveCommand( "wui_store_get" );
	Cmd_RemoveCommand( "wui_store_fingerprint" );
	Cmd_RemoveCommand( "wui_store_dump" );
	Cmd_RemoveCommand( "wui_store_list" );
	Cmd_RemoveCommand( "wui_store_watch" );
	Cmd_RemoveCommand( "wui_store_stats" );

	memset( &wired_store, 0, sizeof( wired_store ) );
}

#endif /* FEAT_WIRED_UI */
