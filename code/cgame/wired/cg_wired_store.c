/*
===========================================================================
cg_wired_store.c -- Wired Store: cgame staging buffer

Phase 4: cgame writes game state to the Wired Store via a staging buffer.
WUI_Stage_Set* helpers accumulate changes. WUI_Stage_Flush sends a single
CG_WUI_STORE_PUSH_BATCH syscall per frame. Deduplicates: multiple writes
to the same key within one frame merge into one staged entry.
===========================================================================
*/

#include "cg_local.h"

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
		Com_Printf( "WARNING: WUI staging buffer full (%d entries), dropping key '%s'\n",
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

/* ---- immediate operations (not staged) ------------------------------- */

void WUI_Stage_Delete( const char *key ) {
	trap_WiredStore_Delete( key );
}

void WUI_Stage_ClearStore( void ) {
	trap_WiredStore_Clear();
}

#endif /* FEAT_WIRED_UI */
