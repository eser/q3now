/*
===========================================================================
cg_wired_store.h -- Wired Store: cgame staging buffer

Phase 4: cgame writes game state to the Wired Store via a staging buffer.
Each frame, accumulated changes are flushed in a single batch syscall.
===========================================================================
*/

#ifndef CG_WIRED_STORE_H
#define CG_WIRED_STORE_H

#if FEAT_WIRED_UI

/* wuiStagedEntry_t and WUI_STAGED_* constants live in cg_public.h
   (shared between cgame and client across the VM boundary) */

#define WUI_STAGE_MAX_ENTRIES 1024

/* Stage setters -- accumulate changes for a key. Deduplicates automatically:
   multiple writes to the same key merge into one entry. */
void WUI_Stage_SetString( const char *key, const char *text );
void WUI_Stage_SetInt( const char *key, int val );
void WUI_Stage_SetFloat( const char *key, float val );
void WUI_Stage_SetColor( const char *key, const vec4_t color );
void WUI_Stage_SetIcon( const char *key, qhandle_t icon );
void WUI_Stage_SetState( const char *key, const char *state );

/* Flush all staged entries to client in one syscall. Call once per frame.
   Clears the staging buffer after flush. */
void WUI_Stage_Flush( void );

/* Clear staging buffer without flushing (e.g. on map change) */
void WUI_Stage_Clear( void );

/* Delete a key from the store (immediate syscall, not staged) */
void WUI_Stage_Delete( const char *key );

/* Clear entire store (immediate syscall, not staged) */
void WUI_Stage_ClearStore( void );

#endif /* FEAT_WIRED_UI */

#endif /* CG_WIRED_STORE_H */
