// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
===========================================================================
cg_wired_store.c -- Wired Store: cgame staging buffer

cgame writes game state to the Wired Store via a staging buffer.
WUI_Stage_Set* helpers accumulate changes. WUI_Stage_Flush sends a single
CG_WUI_STORE_PUSH_BATCH syscall per frame. Deduplicates: multiple writes
to the same key within one frame merge into one staged entry.
===========================================================================
*/

#include "cg_local.h"
LOG_DECLARE_CHANNEL( ch_cgame, "cgame" );

#if FEAT_WIRED_UI

#include "cg_wired_store.h"

/* ---- staging buffer -------------------------------------------------- */

static wuiStagedEntry_t wui_stage[WUI_STAGE_MAX_ENTRIES];
static int              wui_stageCount = 0;

/* ---- internal: find or allocate a staged slot for key ---------------- */

static wuiStagedEntry_t *WUI_Stage_Find( const char *key ) {
	int i;

	/* deduplication: check if we already have this key staged */
	for ( i = 0; i < wui_stageCount; i++ ) {
		if ( !Q_stricmp( wui_stage[i].key, key ) ) {
			return &wui_stage[i];
		}
	}

	/* allocate new slot */
	if ( wui_stageCount >= WUI_STAGE_MAX_ENTRIES ) {
		Com_Log( SEV_INFO, LOG_CH(ch_cgame), "WARNING: WUI staging buffer full (%d entries), dropping key '%s'\n",
					 WUI_STAGE_MAX_ENTRIES, key );
		return NULL;
	}

	memset( &wui_stage[wui_stageCount], 0, sizeof( wuiStagedEntry_t ) );
	Q_strncpyz( wui_stage[wui_stageCount].key, key, sizeof( wui_stage[0].key ) );
	return &wui_stage[wui_stageCount++];
}

/* ---- stage setters --------------------------------------------------- */

void WUI_Stage_SetString( const char *key, const char *text ) {
	wuiStagedEntry_t *e = WUI_Stage_Find( key );
	if ( !e ) return;
	Q_strncpyz( e->text, text, sizeof( e->text ) );
	e->fields |= WUI_STAGED_TEXT;
}

void WUI_Stage_SetInt( const char *key, int val ) {
	wuiStagedEntry_t *e = WUI_Stage_Find( key );
	if ( !e ) return;
	e->value = (float)val;
	Com_sprintf( e->text, sizeof( e->text ), "%d", val );
	e->fields |= WUI_STAGED_VALUE | WUI_STAGED_TEXT;
}

void WUI_Stage_SetFloat( const char *key, float val ) {
	wuiStagedEntry_t *e = WUI_Stage_Find( key );
	if ( !e ) return;
	e->value = val;
	Com_sprintf( e->text, sizeof( e->text ), "%.2f", val );
	e->fields |= WUI_STAGED_VALUE | WUI_STAGED_TEXT;
}

void WUI_Stage_SetColor( const char *key, const vec4_t color ) {
	wuiStagedEntry_t *e = WUI_Stage_Find( key );
	if ( !e ) return;
	Vector4Copy( color, e->color );
	e->fields |= WUI_STAGED_COLOR;
}

void WUI_Stage_SetIcon( const char *key, qhandle_t icon ) {
	wuiStagedEntry_t *e = WUI_Stage_Find( key );
	if ( !e ) return;
	e->icon = icon;
	e->fields |= WUI_STAGED_ICON;
}

void WUI_Stage_SetState( const char *key, const char *state ) {
	wuiStagedEntry_t *e = WUI_Stage_Find( key );
	if ( !e ) return;
	Q_strncpyz( e->state, state, sizeof( e->state ) );
	e->fields |= WUI_STAGED_STATE;
}

/* ---- flush ----------------------------------------------------------- */

void WUI_Stage_Flush( void ) {
	if ( wui_stageCount <= 0 ) {
		return;
	}
	trap_WiredStore_PushBatch( wui_stage, wui_stageCount );
	wui_stageCount = 0;
}

/* ---- clear buffer without flushing ----------------------------------- */

void WUI_Stage_Clear( void ) {
	wui_stageCount = 0;
}

/* ---- world-anchored marker-list staging (WA-2a) ---------------------- */
/* A single fixed scratch list: Begin(listKey) starts a fresh list, Push appends
 * a marker (real-pixel x/y + color + short text), Flush sends it via the marker
 * channel syscall (REPLACES the listKey's prior-frame list client-side). Separate
 * from the scalar dedup store. One list is staged+flushed at a time per frame. */

static wuiMarker_t wui_markerScratch[WUI_MAX_MARKERS_PER_LIST];
static int         wui_markerCount = 0;
static char        wui_markerListKey[64];

void WUI_StageMarkers_Begin( const char *listKey ) {
	Q_strncpyz( wui_markerListKey, listKey ? listKey : "", sizeof( wui_markerListKey ) );
	wui_markerCount = 0;
}

void WUI_StageMarkers_Push( float x, float y, const vec4_t color, const char *text ) {
	wuiMarker_t *m;
	if ( wui_markerCount >= WUI_MAX_MARKERS_PER_LIST ) {
		Com_Log( SEV_INFO, LOG_CH(ch_cgame),
			"WARNING: WUI marker list '%s' full (%d), dropping marker\n",
			wui_markerListKey, WUI_MAX_MARKERS_PER_LIST );
		return;
	}
	m = &wui_markerScratch[wui_markerCount++];
	m->x = x;
	m->y = y;
	Vector4Copy( color, m->color );
	Q_strncpyz( m->text, text ? text : "", sizeof( m->text ) );
}

void WUI_StageMarkers_Flush( void ) {
	if ( !wui_markerListKey[0] ) {
		return;
	}
	/* Always push (even count 0) so an emptied list clears client-side this frame. */
	trap_WiredStore_PushMarkerList( wui_markerListKey, wui_markerScratch, wui_markerCount );
	wui_markerCount = 0;
	wui_markerListKey[0] = '\0';
}

/* ---- immediate operations (not staged) ------------------------------- */

void WUI_Stage_Delete( const char *key ) {
	trap_WiredStore_Delete( key );
}

void WUI_Stage_ClearStore( void ) {
	trap_WiredStore_Clear();
}

#endif /* FEAT_WIRED_UI */
