/*
===========================================================================
cl_wired_hud.h — Wired UI HUD: client-side HUD element rendering

Phase 3: SuperHUD elements live in the client. The wiredHudState_t struct
(defined in cg_public.h) is pushed by cgame each frame. This header provides
the client-side API.
===========================================================================
*/

#ifndef CL_WIRED_HUD_H
#define CL_WIRED_HUD_H

#include "../../cgame/cg_public.h"

#if FEAT_WIRED_UI

// global HUD state — written by cgame trap handler, read by element routines
extern wiredHudState_t  wired_hudStateStorage;
extern wiredHudState_t *wiredHud;

// called from cl_cgame.c when cgame pushes state
void WiredHud_ReceiveState( wiredHudState_t *state );

// called from cl_cgame.c when cgame pushes an event (chat, frag, etc.)
void WiredHud_ReceiveEvent( int type, const char *data );

// called from WiredUI_Refresh each frame (when in-game)
void WiredHud_Routine( int realtime );

// init / shutdown
void WiredHud_Init( void );
void WiredHud_Shutdown( void );

// data binding lookup (returns NULL if not found)
const wiredHudBinding_t *WiredHud_FindBinding( const char *name );

#endif // FEAT_WIRED_UI
#endif // CL_WIRED_HUD_H
