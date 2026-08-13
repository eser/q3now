// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
cl_wired_attract.c — Wired Attract scheduler
*/

#include "../../client.h"
#include "cl_wired_ui.h"
#include "cl_wired_attract.h"
#include "../../../qcommon/wired/core/scripting/wired_scripting.h"
LOG_DECLARE_CHANNEL( ch_ui, "ui" );

#if FEAT_WIRED_UI

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

/* ── item struct (internal — header exposes only the kind enum) ─────── */
typedef struct {
	int  kind;                   /* wiredAttractItemKind_t */
	char source[MAX_QPATH];      /* file path or menu name */
	int  durationMs;             /* 0 = advance on natural completion */
} wiredAttractItem_t;

/* ── file-scope state — NOT in wired_menuPool ───────────────────────── */
static struct {
	qboolean              initialized;
	wiredAttractState_t   state;
	int                   currentIndex;
	int                   itemStartTime;
	int                   transitionStartTime;
	int                   transitionEndTime;
	int                   lastInputTime;
	int                   disconnectTime;
	int                   prevClientState;
	qboolean              prevHadError;
	int                   playlistCount;
	wiredAttractItem_t    playlist[WIRED_ATTRACT_MAX_ITEMS];
	qboolean              loop;
	qboolean              shuffle;
	int                   transitionMs;
	qboolean              ownsDemo;
	char                  pushedPanel[MAX_QPATH];
	cvar_t               *cvEnabled;
	cvar_t               *cvDelay;
	cvar_t               *cvVolume;
} wui_attract;

/* ── forward declarations ────────────────────────────────────────────── */
static void Attract_Teardown( void );
static void Attract_Advance( void );
static void Attract_DispatchCurrent( void );

/* ── security: validate playlist source before use in commands ───────── */
static qboolean Attract_ValidateSource( int kind, const char *src ) {
	size_t len = strlen( src );
	if ( len == 0 || len >= MAX_QPATH ) return qfalse;

	/* Reject bytes that could escape a Cbuf argument or inject commands */
	for ( size_t i = 0; i < len; i++ ) {
		char c = src[i];
		if ( c == '\n' || c == '\r' || c == ';' || c == '"' || c == '$' )
			return qfalse;
	}

	/* DEMO and CINEMATIC must look like a filesystem path */
	if ( kind == ATTRACT_ITEM_DEMO || kind == ATTRACT_ITEM_CINEMATIC ) {
		for ( size_t i = 0; i < len; i++ ) {
			char c = src[i];
			if ( !( ( c >= 'a' && c <= 'z' ) || ( c >= 'A' && c <= 'Z' ) ||
			        ( c >= '0' && c <= '9' ) ||
			        c == '.' || c == '/' || c == '_' || c == '-' ) ) {
				return qfalse;
			}
		}
	}

	/* PANEL must resolve to a menu that is actually loaded */
	if ( kind == ATTRACT_ITEM_PANEL ) {
		if ( WiredUI_FindMenu( src ) == NULL ) return qfalse;
	}

	return qtrue;
}

/* ── teardown current item without advancing ─────────────────────────── */
static void Attract_Teardown( void ) {
	if ( wui_attract.state == ATTRACT_STATE_IDLE ||
	     wui_attract.state == ATTRACT_STATE_WAITING ||
	     wui_attract.state == ATTRACT_STATE_STOPPED ) {
		return;
	}

	if ( wui_attract.playlistCount == 0 ) return;

	{
		int idx = wui_attract.currentIndex;
		if ( idx >= 0 && idx < wui_attract.playlistCount ) {
			wiredAttractItem_t *item = &wui_attract.playlist[idx];

			switch ( item->kind ) {
			case ATTRACT_ITEM_PANEL:
			case ATTRACT_ITEM_CINEMATIC:
				/* Panels live on WUI_LAYER_BG_ATTRACT and emit via the
				 * compositor's multi-panel walk when m->visible. The
				 * scheduler toggles m->visible directly rather than
				 * touching the user-owned menu stack — that keeps the
				 * attract surface on its own z-layer behind any user
				 * menu and avoids stack-ownership contention. */
				if ( wui_attract.pushedPanel[0] != '\0' ) {
					wiredMenuDef_t *panel = WiredUI_FindMenu( wui_attract.pushedPanel );
					if ( panel ) panel->visible = qfalse;
					wui_attract.pushedPanel[0] = '\0';
				}
				break;

			case ATTRACT_ITEM_DEMO:
				/* Hide the demo overlay panel (pushed in Attract_DispatchCurrent). */
				if ( wui_attract.pushedPanel[0] != '\0' ) {
					wiredMenuDef_t *panel = WiredUI_FindMenu( wui_attract.pushedPanel );
					if ( panel ) panel->visible = qfalse;
					wui_attract.pushedPanel[0] = '\0';
				}
				if ( wui_attract.ownsDemo && clientActiveApp->clc.demoplaying ) {
					CL_Disconnect( clientActiveApp, qtrue );
				}
				wui_attract.ownsDemo = qfalse;
				break;
			}
		}
	}
}

/* ── dispatch current playlist item → PLAYING ────────────────────────── */
static void Attract_DispatchCurrent( void ) {
	if ( wui_attract.playlistCount == 0 ) {
		wui_attract.state = ATTRACT_STATE_IDLE;
		return;
	}

	wiredAttractItem_t *item = &wui_attract.playlist[wui_attract.currentIndex];
	wui_attract.itemStartTime = cls.realtime;
	wui_attract.pushedPanel[0] = '\0';
	wui_attract.ownsDemo = qfalse;

	switch ( item->kind ) {
	case ATTRACT_ITEM_PANEL: {
		wiredMenuDef_t *panel = WiredUI_FindMenu( item->source );
		if ( panel == NULL ) {
			Com_Log( SEV_INFO, LOG_CH(ch_ui), "attract: panel '%s' not found, skipping\n", item->source );
			Attract_Advance();
			return;
		}
		panel->visible = qtrue;
		Q_strncpyz( wui_attract.pushedPanel, item->source, sizeof( wui_attract.pushedPanel ) );
		break;
	}

	case ATTRACT_ITEM_DEMO: {
		/* Demo items play the recorded match fullscreen AND show a bg_attract
		 * overlay panel on top (the poster strip/index chrome). The demo runs
		 * at CA_CONNECTED, so bg_attract_policy keys its layer on the demo-
		 * overlay predicate (see WiredAttract_IsDemoOverlayActive) to keep the
		 * panel visible over the scene. Attract owning the demo-overlay is its
		 * OWN chrome, not the user menu — the menu still hides attract. */
		wiredMenuDef_t *panel;
		int dispatchedIndex = wui_attract.currentIndex;
		wui_attract.ownsDemo = qtrue;
		/* source is already validated — safe to use in command */
		Cbuf_ExecuteText( EXEC_NOW, va( "demo %s\n", item->source ) );
		/* The demo command reads its first frame synchronously.  A malformed
		 * attract-owned demo can therefore fail and advance the playlist before
		 * this dispatch returns.  Do not let the stale dispatch install its
		 * overlay or overwrite the successor's TRANSITIONING state. */
		if ( wui_attract.currentIndex != dispatchedIndex ||
		     wui_attract.state != ATTRACT_STATE_STARTING ||
		     !wui_attract.ownsDemo ) {
			return;
		}
		panel = WiredUI_FindMenu( "attract_demo_overlay" );
		if ( panel ) {
			panel->visible = qtrue;
			Q_strncpyz( wui_attract.pushedPanel, "attract_demo_overlay", sizeof( wui_attract.pushedPanel ) );
		}
		break;
	}

	case ATTRACT_ITEM_CINEMATIC: {
		/* MVP: attract_cinematic.wmenu has a hardcoded cinematic.
		   Store the file path in a WiredUI state key for the menu to read. */
		wiredMenuDef_t *panel;
		WiredUI_StateSetString( "attract.cinematic.file", item->source );
		panel = WiredUI_FindMenu( "attract_cinematic" );
		if ( panel == NULL ) {
			Com_Log( SEV_INFO, LOG_CH(ch_ui), "attract: attract_cinematic menu not found, skipping\n" );
			Attract_Advance();
			return;
		}
		panel->visible = qtrue;
		Q_strncpyz( wui_attract.pushedPanel, "attract_cinematic", sizeof( wui_attract.pushedPanel ) );
		break;
	}

	default:
		Com_Log( SEV_INFO, LOG_CH(ch_ui), "attract: unknown item kind %d, skipping\n", item->kind );
		Attract_Advance();
		return;
	}

	wui_attract.state = ATTRACT_STATE_PLAYING;
}

/* ── advance to the next item, beginning a transition ────────────────── */
static void Attract_Advance( void ) {
	Attract_Teardown();

	wui_attract.currentIndex++;
	if ( wui_attract.currentIndex >= wui_attract.playlistCount ) {
		if ( wui_attract.loop ) {
			wui_attract.currentIndex = 0;
		} else {
			wui_attract.state = ATTRACT_STATE_STOPPED;
			return;
		}
	}

	/* Kick off transition — frame tick will draw the black fade */
	wui_attract.state = ATTRACT_STATE_TRANSITIONING;
	wui_attract.transitionStartTime = cls.realtime;
	wui_attract.transitionEndTime   = cls.realtime + wui_attract.transitionMs;
}

/* ── console commands ────────────────────────────────────────────────── */
static void Attract_Start_f( void ) { WiredAttract_Start(); }
static void Attract_Stop_f( void )  { WiredAttract_Stop(); }
static void Attract_Skip_f( void )  { WiredAttract_Skip(); }

static void Attract_Restart_f( void ) {
	WiredAttract_Stop();
	wui_attract.currentIndex = 0;
	WiredAttract_Start();
}

static void Attract_Status_f( void ) {
	static const char *stateNames[] = {
		"IDLE", "WAITING", "STARTING", "PLAYING", "TRANSITIONING", "STOPPED"
	};
	int state = (int)wui_attract.state;
	const char *stateName = ( state >= 0 && state <= 5 ) ? stateNames[state] : "?";

	Com_Log( SEV_INFO, LOG_CH(ch_ui), "attract_status:\n" );
	Com_Log( SEV_INFO, LOG_CH(ch_ui), "  state         : %s\n", stateName );
	Com_Log( SEV_INFO, LOG_CH(ch_ui), "  playlist      : %d items (loop=%d shuffle=%d)\n",
	            wui_attract.playlistCount, wui_attract.loop, wui_attract.shuffle );
	Com_Log( SEV_INFO, LOG_CH(ch_ui), "  currentIndex  : %d\n", wui_attract.currentIndex );
	Com_Log( SEV_INFO, LOG_CH(ch_ui), "  itemElapsed   : %d ms\n",
	            wui_attract.state == ATTRACT_STATE_PLAYING
	            ? (int)( cls.realtime - wui_attract.itemStartTime ) : 0 );
	Com_Log( SEV_INFO, LOG_CH(ch_ui), "  attract_delay : %d s\n",
	            wui_attract.cvDelay ? wui_attract.cvDelay->integer : 0 );
	Com_Log( SEV_INFO, LOG_CH(ch_ui), "  attract_volume: %.2f\n",
	            wui_attract.cvVolume ? wui_attract.cvVolume->value : 0.0f );
	Com_Log( SEV_INFO, LOG_CH(ch_ui), "  ownsDemo      : %d (demoplaying=%d)\n",
	            wui_attract.ownsDemo, clientActiveApp->clc.demoplaying );
	Com_Log( SEV_INFO, LOG_CH(ch_ui), "  pushedPanel   : %s\n",
	            wui_attract.pushedPanel[0] != '\0' ? wui_attract.pushedPanel : "none" );
	Com_Log( SEV_INFO, LOG_CH(ch_ui), "  wiredHealthy  : %d\n", WiredUI_IsHealthy() );
	Com_Log( SEV_INFO, LOG_CH(ch_ui), "  recoveryFail  : %d ms ago\n",
	            WiredUI_GetLastRecoveryFailTime() != 0
	            ? (int)( cls.realtime - WiredUI_GetLastRecoveryFailTime() ) : -1 );
	Com_Log( SEV_INFO, LOG_CH(ch_ui), "  prevHadError  : %d\n", wui_attract.prevHadError );
	Com_Log( SEV_INFO, LOG_CH(ch_ui), "  idle-detect   : keyboard only (mouse gated on KEYCATCH_UI)\n" );
}

/* ── Lua bindings ────────────────────────────────────────────────────── */

static int AttractLua_Add( lua_State *L ) {
	if ( lua_gettop( L ) < 2 ) {
		return luaL_error( L, "attract.add(kind, source [, duration_ms])" );
	}

	const char *kindStr = luaL_checkstring( L, 1 );
	const char *src     = luaL_checkstring( L, 2 );
	int durationMs = lua_gettop( L ) >= 3 ? (int)luaL_checkinteger( L, 3 ) : 0;
	int kind;

	if ( Q_stricmp( kindStr, "cinematic" ) == 0 )      kind = ATTRACT_ITEM_CINEMATIC;
	else if ( Q_stricmp( kindStr, "demo" ) == 0 )      kind = ATTRACT_ITEM_DEMO;
	else if ( Q_stricmp( kindStr, "panel" ) == 0 )     kind = ATTRACT_ITEM_PANEL;
	else return luaL_error( L, "attract.add: unknown kind '%s' (cinematic|demo|panel)", kindStr );

	if ( !Attract_ValidateSource( kind, src ) ) {
		return luaL_error( L, "attract.add: invalid source '%s' for kind '%s'", src, kindStr );
	}

	if ( wui_attract.playlistCount >= WIRED_ATTRACT_MAX_ITEMS ) {
		return luaL_error( L, "attract.add: playlist full (%d items)", WIRED_ATTRACT_MAX_ITEMS );
	}

	wiredAttractItem_t *item = &wui_attract.playlist[wui_attract.playlistCount];
	item->kind       = kind;
	item->durationMs = durationMs;
	Q_strncpyz( item->source, src, sizeof( item->source ) );
	wui_attract.playlistCount++;
	return 0;
}

static int AttractLua_Clear( lua_State *L ) {
	(void)L;
	wui_attract.playlistCount = 0;
	wui_attract.currentIndex  = 0;
	return 0;
}

static int AttractLua_SetLoop( lua_State *L ) {
	wui_attract.loop = lua_toboolean( L, 1 ) ? qtrue : qfalse;
	return 0;
}

static int AttractLua_SetShuffle( lua_State *L ) {
	wui_attract.shuffle = lua_toboolean( L, 1 ) ? qtrue : qfalse;
	return 0;
}

static int AttractLua_SetTransition( lua_State *L ) {
	int ms = (int)luaL_checkinteger( L, 1 );
	if ( ms < 0 ) ms = 0;
	wui_attract.transitionMs = ms;
	return 0;
}

static const luaL_Reg wui_attractLib[] = {
	{ "add",            AttractLua_Add },
	{ "clear",          AttractLua_Clear },
	{ "set_loop",       AttractLua_SetLoop },
	{ "set_shuffle",    AttractLua_SetShuffle },
	{ "set_transition", AttractLua_SetTransition },
	{ NULL, NULL }
};

static void Attract_LuaRegister( lua_State *L ) {
	luaL_newlib( L, wui_attractLib );
	lua_setglobal( L, "attract" );
}


/* ══════════════════════════════════════════════════════════════════════ */
/* Public API                                                            */
/* ══════════════════════════════════════════════════════════════════════ */

/* Called from CL_Init — BEFORE WiredScript_PostInit — so that attract.*
   globals are live in the Lua VM when attract.lua runs in WiredAttract_Init. */
void WiredAttract_LuaInit( void ) {
	WiredScript_RegisterBindings( Attract_LuaRegister );
}

void WiredAttract_Init( void ) {
	memset( &wui_attract, 0, sizeof( wui_attract ) );

	wui_attract.transitionMs = 500; /* 250ms out + 250ms in */
	wui_attract.loop         = qtrue;
	wui_attract.prevClientState = clientActiveApp->state;

	wui_attract.cvEnabled = Cvar_Get( "attract_enabled", "1", CVAR_ARCHIVE );
	/* Zero-delay default: attract surface up from boot. Modders raise
	 * attract_delay to restore the legacy "wait N seconds of idle
	 * before starting" behaviour. */
	wui_attract.cvDelay   = Cvar_Get( "attract_delay",  "0", CVAR_ARCHIVE );
	wui_attract.cvVolume  = Cvar_Get( "attract_volume", "0.5", CVAR_ARCHIVE );

	Cmd_AddCommand( "attract_start",   Attract_Start_f );
	Cmd_AddCommand( "attract_stop",    Attract_Stop_f );
	Cmd_AddCommand( "attract_skip",    Attract_Skip_f );
	Cmd_AddCommand( "attract_restart", Attract_Restart_f );
	Cmd_AddCommand( "attract_status",  Attract_Status_f );

	/* Binding was already registered in WiredAttract_LuaInit (called from
	   CL_Init before WiredScript_PostInit).  Just exec the playlist now. */
	WiredScript_ExecFile( "scripts/attract.lua" );

	wui_attract.initialized = qtrue;
	Com_Log( SEV_INFO, LOG_CH(ch_ui), "WiredAttract: initialized (%d items in playlist)\n",
	            wui_attract.playlistCount );

	/* Boot kick into WAITING so the per-frame tick promotes to PLAYING
	 * on the first frame with attract_delay=0. WAITING (not STARTING)
	 * preserves the existing elapsed-vs-delay authority; raising
	 * attract_delay restores the boot delay naturally. */
	if ( wui_attract.cvEnabled && wui_attract.cvEnabled->integer
	  && wui_attract.playlistCount > 0
	  && clientActiveApp->state < CA_ACTIVE ) {
		wui_attract.disconnectTime = cls.realtime;
		wui_attract.lastInputTime  = cls.realtime;
		wui_attract.state          = ATTRACT_STATE_WAITING;
	}
}

void WiredAttract_Shutdown( void ) {
	if ( !wui_attract.initialized ) return;

	Attract_Teardown();

	Cmd_RemoveCommand( "attract_start" );
	Cmd_RemoveCommand( "attract_stop" );
	Cmd_RemoveCommand( "attract_skip" );
	Cmd_RemoveCommand( "attract_restart" );
	Cmd_RemoveCommand( "attract_status" );

	wui_attract.initialized = qfalse;
}

/* ── per-frame tick ──────────────────────────────────────────────────── */
void WiredAttract_Frame( int msec ) {
	(void)msec;

	if ( !wui_attract.initialized ) return;
	if ( !wui_attract.cvEnabled || !wui_attract.cvEnabled->integer ) return;

	/* ── edge detect cls.state (connect / disconnect) ─────────────── */
	if ( wui_attract.prevClientState == CA_ACTIVE && clientActiveApp->state < CA_ACTIVE ) {
		/* Just disconnected — start idle timer */
		wui_attract.disconnectTime = cls.realtime;
		if ( wui_attract.state != ATTRACT_STATE_STOPPED ) {
			wui_attract.state = ATTRACT_STATE_WAITING;
		}
	} else if ( wui_attract.prevClientState < CA_ACTIVE && clientActiveApp->state == CA_ACTIVE ) {
		/* Just connected — stop attract */
		WiredAttract_Stop();
	}
	wui_attract.prevClientState = clientActiveApp->state;

	/* ── edge detect Com_HasLastError — stop attract immediately ──── */
	{
		qboolean hasError = Com_HasLastError();
		if ( !wui_attract.prevHadError && hasError ) {
			Attract_Teardown();
			wui_attract.ownsDemo = qfalse;
			wui_attract.state    = ATTRACT_STATE_WAITING;
		}
		wui_attract.prevHadError = hasError;
	}

	/* ── demo ownership sanity (handles /disconnect during attract) ── */
	if ( wui_attract.state == ATTRACT_STATE_PLAYING &&
	     wui_attract.ownsDemo && !clientActiveApp->clc.demoplaying ) {
		wui_attract.ownsDemo = qfalse;
		Attract_Advance();
		return;
	}

	/* ── gate: only advance state machine while disconnected and idle ─ */
	if ( clientActiveApp->state >= CA_ACTIVE ) return;
	if ( Com_HasLastError() ) return;

	/* Attract panels live on WUI_LAYER_BG_ATTRACT and the scheduler
	 * drives their visibility directly (not via the menu stack). The
	 * legacy "back off if a user menu is on top" gate retires — attract
	 * keeps cycling its playlist as a persistent background surface
	 * while the user navigates menus on the foreground layer. */

	/* ── state dispatch ──────────────────────────────────────────────── */
	int curState = (int)wui_attract.state;
	int delayMs  = wui_attract.cvDelay ? wui_attract.cvDelay->integer * 1000 : 30000;

	switch ( curState ) {

	case ATTRACT_STATE_IDLE:
		/* No playlist — nothing to do */
		break;

	case ATTRACT_STATE_WAITING:
		if ( wui_attract.playlistCount == 0 ) break;
		/* Require both disconnect-idle AND input-idle to have elapsed */
		if ( (int)( cls.realtime - wui_attract.disconnectTime ) >= delayMs &&
		     (int)( cls.realtime - wui_attract.lastInputTime )  >= delayMs ) {
			WiredAttract_Start();
		}
		break;

	case ATTRACT_STATE_STARTING:
		Attract_DispatchCurrent();
		break;

	case ATTRACT_STATE_PLAYING:
		if ( wui_attract.playlistCount == 0 ) break;
		{
			wiredAttractItem_t *item = &wui_attract.playlist[wui_attract.currentIndex];
			if ( item->durationMs > 0 &&
			     (int)( cls.realtime - wui_attract.itemStartTime ) >= item->durationMs ) {
				Attract_Advance();
			}
		}
		break;

	case ATTRACT_STATE_TRANSITIONING:
		if ( (int)( cls.realtime - wui_attract.transitionStartTime ) >= wui_attract.transitionMs ) {
			wui_attract.state = ATTRACT_STATE_STARTING;
		}
		break;

	case ATTRACT_STATE_STOPPED:
		/* Manually stopped — do not auto-restart; wait for next disconnect event */
		break;
	}

draw_transition:
	/* ── draw transition overlay ─────────────────────────────────────── */
	if ( wui_attract.state == ATTRACT_STATE_TRANSITIONING ) {
		int   halfMs   = wui_attract.transitionMs / 2;
		int   elapsed  = (int)( cls.realtime - wui_attract.transitionStartTime );
		float alpha;

		if ( elapsed < halfMs ) {
			/* Fade out: 0 → 1 */
			alpha = halfMs > 0 ? (float)elapsed / (float)halfMs : 1.0f;
		} else {
			/* Fade in: 1 → 0 */
			int inElapsed = elapsed - halfMs;
			alpha = halfMs > 0 ? 1.0f - (float)inElapsed / (float)halfMs : 0.0f;
		}
		if ( alpha < 0.0f ) alpha = 0.0f;
		if ( alpha > 1.0f ) alpha = 1.0f;

		vec4_t c;
		c[0] = 0.0f; c[1] = 0.0f; c[2] = 0.0f; c[3] = alpha;
		re.SetColor( c );
		re.DrawStretchPic( 0, 0,
		                   (float)cls.glconfig.vidWidth,
		                   (float)cls.glconfig.vidHeight,
		                   0, 0, 0, 0, cls.whiteShader );
		re.SetColor( NULL );
	}
}

/* ── input taps ──────────────────────────────────────────────────────── */
void WiredAttract_NoteInput( int key ) {
	/* Ignore pure modifier keys so Alt+Tab doesn't stop attract */
	if ( key == K_ALT     || key == K_CTRL ||
	     key == K_SHIFT   || key == K_SUPER ) {
		return;
	}

	wui_attract.lastInputTime = cls.realtime;

	/* User input no longer stops attract. The scheduler keeps cycling
	 * on the bg_attract layer until clientActiveApp->state leaves CA_DISCONNECTED;
	 * the foreground menu (promoted by cl_keys.c first-input branch)
	 * draws on top. */
}

void WiredAttract_NoteMouse( int dx, int dy ) {
	/* 64 squared = 8px threshold — filters optical-mouse jitter */
	if ( dx * dx + dy * dy <= 64 ) return;

	wui_attract.lastInputTime = cls.realtime;
	/* Stop semantics retired — see WiredAttract_NoteInput. */
}

/* ── SafeReload hook ─────────────────────────────────────────────────── */
void WiredAttract_OnMenuReload( void ) {
	/* The pool was rebuilt — our pushedPanel pointer may be stale.
	   Clear it and re-dispatch the current item on the next frame so
	   the panel gets pushed against the fresh pool. */
	if ( wui_attract.state == ATTRACT_STATE_PLAYING &&
	     wui_attract.pushedPanel[0] != '\0' ) {
		wui_attract.pushedPanel[0] = '\0';
		wui_attract.state = ATTRACT_STATE_STARTING;
	}
}

/* ── control ─────────────────────────────────────────────────────────── */
void WiredAttract_Start( void ) {
	if ( !wui_attract.initialized ) return;
	if ( wui_attract.playlistCount == 0 ) {
		Com_Log( SEV_INFO, LOG_CH(ch_ui), "attract: playlist is empty\n" );
		return;
	}
	wui_attract.currentIndex  = 0;
	wui_attract.lastInputTime = cls.realtime;
	wui_attract.state         = ATTRACT_STATE_STARTING;
}

void WiredAttract_Stop( void ) {
	if ( !wui_attract.initialized ) return;
	Attract_Teardown();
	wui_attract.state = ATTRACT_STATE_STOPPED;
}

void WiredAttract_Skip( void ) {
	if ( !wui_attract.initialized ) return;
	if ( wui_attract.state == ATTRACT_STATE_PLAYING ||
	     wui_attract.state == ATTRACT_STATE_STARTING ) {
		Attract_Advance();
	}
}

/* ── query ───────────────────────────────────────────────────────────── */
wiredAttractState_t WiredAttract_GetState( void ) {
	return wui_attract.state;
}

qboolean WiredAttract_IsActive( void ) {
	return wui_attract.initialized &&
	       ( wui_attract.state == ATTRACT_STATE_PLAYING ||
	         wui_attract.state == ATTRACT_STATE_STARTING ||
	         wui_attract.state == ATTRACT_STATE_TRANSITIONING );
}

qboolean WiredAttract_IsDemoOverlayActive( void ) {
	/* True while attract is playing a demo it owns — the window in which the
	 * bg_attract layer must stay active (state is CA_CONNECTED, not
	 * DISCONNECTED) so the demo-overlay panel renders over the match. */
	return wui_attract.initialized &&
	       wui_attract.ownsDemo &&
	       clientActiveApp->clc.demoplaying;
}

/* ── completion callbacks ────────────────────────────────────────────── */
qboolean WiredAttract_OnDemoCompleted( void ) {
	if ( !wui_attract.initialized ) return qfalse;
	if ( !wui_attract.ownsDemo )    return qfalse;
	if ( wui_attract.state != ATTRACT_STATE_PLAYING ) return qfalse;

	wui_attract.ownsDemo = qfalse;
	Attract_Advance();
	return qtrue;
}

qboolean WiredAttract_OnDemoFailed( void ) {
	/* The first demo frames are read synchronously while the scheduler is still
	 * STARTING, so the normal PLAYING-only completion callback cannot own this
	 * transition.  Ownership is the authority: consume it once and advance. */
	if ( !wui_attract.initialized ) return qfalse;
	if ( !wui_attract.ownsDemo )    return qfalse;
	wui_attract.ownsDemo = qfalse;
	Attract_Advance();
	/* Never dispatch the successor recursively from this callback.  The first
	 * demo frame is read synchronously inside Attract_DispatchCurrent; a
	 * zero-transition looping playlist of malformed demos would otherwise
	 * recurse until stack exhaustion.  WiredAttract_Frame owns continuation. */
	return qtrue;
}


qboolean WiredAttract_OnCinematicCompleted( void ) {
	return qfalse;
}

#endif /* FEAT_WIRED_UI */
