#pragma once

// cl_wired_attract.h — Wired Attract scheduler public API.
//
// Attract fills idle time (disconnected, after attract_delay seconds) with a
// Lua-authored playlist of cinematics, .wmenu panels, and demo playbacks.
// The scheduler lives entirely inside the Wired UI module; its file-scope
// static state (s_attract) is deliberately outside wired_menuPool so that
// WiredUI_SafeReload does not wipe it.
//
// Dependency chain:
//   cl_wired_ui.c → calls WiredAttract_{Init,Shutdown,Frame,NoteInput,...}
//   cl_main.c     → calls WiredAttract_OnDemoCompleted inside CL_DemoCompleted

// ── item types ───────────────────────────────────────────────────────
// Only concrete item kinds are listed; stubs (FLYTHROUGH, LUA) are deferred
// until a real use case exists.
typedef enum {
	ATTRACT_ITEM_CINEMATIC = 0,   // .roq playback via attract_cinematic.wmenu
	ATTRACT_ITEM_DEMO,            // .dm_* demo file via 'demo' cbuf command
	ATTRACT_ITEM_PANEL            // .wmenu name — pushed via WiredUI_PushMenu
} wiredAttractItemKind_t;

// ── state machine ────────────────────────────────────────────────────
typedef enum {
	ATTRACT_STATE_IDLE = 0,       // disabled or not initialized
	ATTRACT_STATE_WAITING,        // post-disconnect, counting down attract_delay
	ATTRACT_STATE_STARTING,       // dispatching next playlist item
	ATTRACT_STATE_PLAYING,        // item currently visible
	ATTRACT_STATE_TRANSITIONING,  // hold-to-black fade between items
	ATTRACT_STATE_STOPPED         // manually stopped; waits for next trigger
} wiredAttractState_t;

#define WIRED_ATTRACT_MAX_ITEMS 64

// ── pre-PostInit Lua binding registration (called from WiredUI_LuaInit) ─
// WiredUI_LuaInit is the single CL_Init-level entry point; it calls this
// internally. Do NOT call directly from CL_Init.
void WiredAttract_LuaInit( void );

// ── lifecycle (called from WiredUI_Init / WiredUI_Shutdown) ──────────
void WiredAttract_Init( void );
void WiredAttract_Shutdown( void );

// ── per-frame tick (called from WiredUI_Refresh) ─────────────────────
void WiredAttract_Frame( int msec );

// ── input / state hooks ──────────────────────────────────────────────
void WiredAttract_NoteInput( int key );       // from WiredUI_KeyEvent
void WiredAttract_NoteMouse( int dx, int dy );// from WiredUI_MouseEvent
void WiredAttract_OnMenuReload( void );       // from WiredUI_SafeReload

// ── control ──────────────────────────────────────────────────────────
void WiredAttract_Start( void );
void WiredAttract_Stop( void );
void WiredAttract_Skip( void );

// ── query ────────────────────────────────────────────────────────────
wiredAttractState_t WiredAttract_GetState( void );
qboolean            WiredAttract_IsActive( void );

// ── completion callbacks (called from engine when content finishes) ───
qboolean WiredAttract_OnDemoCompleted( void );      // qtrue = attract handled it
qboolean WiredAttract_OnCinematicCompleted( void ); // qtrue = attract handled it
