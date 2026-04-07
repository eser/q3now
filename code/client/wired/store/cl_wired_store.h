/*
===========================================================================
cl_wired_store.h — Wired UI Store: generic key-value state bridge

Phase 4: Game-agnostic state store replacing monolithic wiredHudState_t.
cgame writes via staging buffer + batch syscall, client reads at render time.
===========================================================================
*/

#ifndef CL_WIRED_STORE_H
#define CL_WIRED_STORE_H

#include "../../../qcommon/q_shared.h"

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

typedef struct {
	wuiStoreEntry_t *buckets[WUI_STORE_BUCKETS];    /* hash chains */
	wuiStoreEntry_t  pool[WUI_STORE_MAX_ENTRIES];    /* pre-allocated entry pool */
	int              numEntries;                      /* current entry count */
	int              generation;                      /* incremented each frame/batch */
} wuiStore_t;

/* ── public API ─────────────────────────────────────────────────────── */

void             WiredStore_Init( void );
void             WiredStore_Shutdown( void );
void             WiredStore_Clear( void );
wuiStoreEntry_t *WiredStore_Get( const char *key );
wuiStoreEntry_t *WiredStore_Set( const char *key );
void             WiredStore_Delete( const char *key );
void             WiredStore_BeginFrame( void );

/* Iterate all entries matching a key prefix. Calls fn(entry, userData) for
   each match. prefix="" matches everything. Not for use in render path. */
void             WiredStore_ForEach( const char *prefix,
                                     void (*fn)( wuiStoreEntry_t *entry, void *userData ),
                                     void *userData );

/* ── Lua binding registration (client-side) ────────────────────────── */

void             WiredStoreLua_Init( void );

#endif /* FEAT_WIRED_UI */

#endif /* CL_WIRED_STORE_H */
