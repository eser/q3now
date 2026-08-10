// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
===========================================================================
cl_wired_store.h — Wired UI Store: generic key-value state bridge

Game-agnostic state store replacing monolithic wiredHudState_t.
cgame writes via staging buffer + batch syscall, client reads at render time.
===========================================================================
*/

#ifndef CL_WIRED_STORE_H
#define CL_WIRED_STORE_H

#include "../../../qcommon/q_shared.h"
#include "../../../cgame/cg_public.h"	/* wuiMarker_t (WA-2a marker-list channel) */

#if FEAT_WIRED_UI

/* ── flags bitfield ─────────────────────────────────────────────────── */

#define WUI_STORE_FLAG_NONE      0
#define WUI_STORE_FLAG_DIRTY     (1 << 0)   /* changed this frame */
#define WUI_STORE_FLAG_WATCHED   (1 << 1)   /* console watch active */

/* ── store entry ────────────────────────────────────────────────────── */

typedef struct wuiStoreEntry_s {
	char            key[128];           /* dot-separated namespace key (e.g. "player.health.text") */
	char            text[256];          /* string value */
	vec4_t          color;              /* RGBA color */
	qhandle_t       icon;               /* shader handle for icons */
	float           value;              /* numeric value */
	char            state[32];          /* semantic state label ("critical", "warning", "normal", etc.) */
	int             flags;              /* bitfield for metadata */
	int             generation;         /* tracks when this entry was last written */
	struct wuiStoreEntry_s *next;       /* hash chain pointer */
} wuiStoreEntry_t;

/* ── store container ────────────────────────────────────────────────── */

#define WUI_STORE_BUCKETS       512
#define WUI_STORE_MAX_ENTRIES   4096

/* ── world-anchored marker lists (WA-2a) ─────────────────────────────────
 * A small fixed set of named marker LISTS (separate channel from the scalar
 * hash store). Each frame cgame REPLACES a list via PushMarkerList; lists not
 * pushed this frame are cleared in WiredStore_BeginFrame (frame-transient, no
 * stale accumulation). MAX_MARKERS_PER_LIST 64 = 32 damage plums + headroom. */
#define WUI_MAX_MARKER_LISTS        4
/* WUI_MAX_MARKERS_PER_LIST is defined in cg_public.h (shared with the cgame
 * staging scratch) — included above. */

typedef struct {
	char        key[64];                            /* listKey (e.g. "markers.plums"); empty = free slot */
	int         count;                              /* live markers this frame */
	wuiMarker_t markers[WUI_MAX_MARKERS_PER_LIST];
} wuiMarkerList_t;

typedef struct {
	wuiStoreEntry_t *buckets[WUI_STORE_BUCKETS];    /* hash chains */
	wuiStoreEntry_t  pool[WUI_STORE_MAX_ENTRIES];    /* pre-allocated entry pool */
	int              numEntries;                      /* current entry count */
	int              generation;                      /* incremented each frame/batch */
	wuiMarkerList_t  markerLists[WUI_MAX_MARKER_LISTS]; /* WA-2a marker channel */
} wuiStore_t;

/* ── public API ─────────────────────────────────────────────────────── */

void             WiredStore_Init( void );
void             WiredStore_Shutdown( void );
void             WiredStore_Clear( void );
wuiStoreEntry_t *WiredStore_Get( const char *key );
wuiStoreEntry_t *WiredStore_Set( const char *key );
void             WiredStore_Delete( const char *key );
void             WiredStore_BeginFrame( void );

/* WA-2a marker channel: SetMarkerList REPLACES listKey's markers for this frame
 * (find-or-alloc by key, caps at WUI_MAX_MARKERS_PER_LIST). GetMarkerList returns
 * the marker array for a listKey (NULL + *outCount=0 if absent). Frame-transient:
 * WiredStore_BeginFrame zeroes all list counts so an unpushed list draws nothing. */
void                WiredStore_SetMarkerList( const char *listKey, const wuiMarker_t *markers, int count );
const wuiMarker_t  *WiredStore_GetMarkerList( const char *listKey, int *outCount );

/* Iterate all entries matching a key prefix. Calls fn(entry, userData) for
   each match. prefix="" matches everything. Not for use in render path. */
void             WiredStore_ForEach( const char *prefix,
                                     void (*fn)( wuiStoreEntry_t *entry, void *userData ),
                                     void *userData );

/* ── Lua binding registration (client-side) ────────────────────────── */

void             WiredStoreLua_Init( void );

#endif /* FEAT_WIRED_UI */

#endif /* CL_WIRED_STORE_H */
