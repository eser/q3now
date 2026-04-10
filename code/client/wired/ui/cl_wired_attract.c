/*
===========================================================================
cl_wired_attract.c — Wired Attract scheduler

Fills idle time (disconnected + attract_delay seconds of inactivity) with
a Lua-authored playlist of cinematics, .wmenu panels, and demo playbacks.

State machine:
  IDLE         → not initialized or no playlist
  WAITING      → post-disconnect, counting attract_delay
  STARTING     → dispatching the next playlist item
  PLAYING      → item is currently visible
  TRANSITIONING→ 250ms fade-out + 250ms fade-in black overlay
  STOPPED      → manually stopped; waits for next idle trigger

Integration hooks (called from cl_wired_ui.c):
  WiredUI_Init         → WiredAttract_Init
  WiredUI_Shutdown     → WiredAttract_Shutdown
  WiredUI_Refresh      → WiredAttract_Frame   (before early-return guard)
  WiredUI_KeyEvent     → WiredAttract_NoteInput
  WiredUI_MouseEvent   → WiredAttract_NoteMouse
  WiredUI_SafeReload   → WiredAttract_OnMenuReload  (post-swap)
  cl_main.c:CL_DemoCompleted → WiredAttract_OnDemoCompleted
===========================================================================
*/

#include "../../client.h"
#include "cl_wired_ui.h"
#include "cl_wired_attract.h"
#include "../../../qcommon/scripting/wired_scripting.h"

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
} s_attract;

/* ── forward declarations ────────────────────────────────────────────── */
static void Attract_Teardown( void );
static void Attract_Advance( void );
static void Attract_DispatchCurrent( void );

/* ── security: validate playlist source before use in commands ───────── */
static qboolean Attract_ValidateSource( int kind, const char *src ) {
	size_t len, i;

	len = strlen( src );
	if ( len == 0 || len >= MAX_QPATH ) return qfalse;

	/* Reject bytes that could escape a Cbuf argument or inject commands */
	for ( i = 0; i < len; i++ ) {
		char c = src[i];
		if ( c == '\n' || c == '\r' || c == ';' || c == '"' || c == '$' )
			return qfalse;
	}

	/* DEMO and CINEMATIC must look like a filesystem path */
	if ( kind == ATTRACT_ITEM_DEMO || kind == ATTRACT_ITEM_CINEMATIC ) {
		for ( i = 0; i < len; i++ ) {
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
	if ( s_attract.state == ATTRACT_STATE_IDLE ||
	     s_attract.state == ATTRACT_STATE_WAITING ||
	     s_attract.state == ATTRACT_STATE_STOPPED ) {
		return;
	}

	if ( s_attract.playlistCount == 0 ) return;

	{
		int idx = s_attract.currentIndex;
		if ( idx >= 0 && idx < s_attract.playlistCount ) {
			wiredAttractItem_t *item = &s_attract.playlist[idx];

			switch ( item->kind ) {
			case ATTRACT_ITEM_PANEL:
				if ( s_attract.pushedPanel[0] != '\0' ) {
					/* Only pop if our panel is still top of stack */
					if ( WiredUI_GetMenuStackDepth() > 0 &&
					     Q_stricmp( WiredUI_GetMenuStackTop(), s_attract.pushedPanel ) == 0 ) {
						WiredUI_PopMenu();
					}
					s_attract.pushedPanel[0] = '\0';
				}
				break;

			case ATTRACT_ITEM_DEMO:
				if ( s_attract.ownsDemo && clc.demoplaying ) {
					CL_Disconnect( qtrue );
				}
				s_attract.ownsDemo = qfalse;
				break;

			case ATTRACT_ITEM_CINEMATIC:
				/* Cinematic runs inside attract_cinematic.wmenu — pop the panel */
				if ( s_attract.pushedPanel[0] != '\0' ) {
					if ( WiredUI_GetMenuStackDepth() > 0 &&
					     Q_stricmp( WiredUI_GetMenuStackTop(), s_attract.pushedPanel ) == 0 ) {
						WiredUI_PopMenu();
					}
					s_attract.pushedPanel[0] = '\0';
				}
				break;
			}
		}
	}
}

/* ── dispatch current playlist item → PLAYING ────────────────────────── */
static void Attract_DispatchCurrent( void ) {
	wiredAttractItem_t *item;

	if ( s_attract.playlistCount == 0 ) {
		s_attract.state = ATTRACT_STATE_IDLE;
		return;
	}

	item = &s_attract.playlist[s_attract.currentIndex];
	s_attract.itemStartTime = cls.realtime;
	s_attract.pushedPanel[0] = '\0';
	s_attract.ownsDemo = qfalse;

	switch ( item->kind ) {
	case ATTRACT_ITEM_PANEL:
		if ( WiredUI_FindMenu( item->source ) == NULL ) {
			Com_Printf( "attract: panel '%s' not found, skipping\n", item->source );
			Attract_Advance();
			return;
		}
		WiredUI_PushMenu( item->source );
		Q_strncpyz( s_attract.pushedPanel, item->source, sizeof( s_attract.pushedPanel ) );
		break;

	case ATTRACT_ITEM_DEMO:
		s_attract.ownsDemo = qtrue;
		/* source is already validated — safe to use in command */
		Cbuf_ExecuteText( EXEC_NOW, va( "demo %s\n", item->source ) );
		break;

	case ATTRACT_ITEM_CINEMATIC:
		/* MVP: attract_cinematic.wmenu has a hardcoded cinematic.
		   Store the file path in a WiredUI state key for the menu to read. */
		WiredUI_StateSetString( "attract.cinematic.file", item->source );
		if ( WiredUI_FindMenu( "attract_cinematic" ) == NULL ) {
			Com_Printf( "attract: attract_cinematic menu not found, skipping\n" );
			Attract_Advance();
			return;
		}
		WiredUI_PushMenu( "attract_cinematic" );
		Q_strncpyz( s_attract.pushedPanel, "attract_cinematic", sizeof( s_attract.pushedPanel ) );
		break;

	default:
		Com_Printf( "attract: unknown item kind %d, skipping\n", item->kind );
		Attract_Advance();
		return;
	}

	s_attract.state = ATTRACT_STATE_PLAYING;
}

/* ── advance to the next item, beginning a transition ────────────────── */
static void Attract_Advance( void ) {
	Attract_Teardown();

	s_attract.currentIndex++;
	if ( s_attract.currentIndex >= s_attract.playlistCount ) {
		if ( s_attract.loop ) {
			s_attract.currentIndex = 0;
		} else {
			s_attract.state = ATTRACT_STATE_STOPPED;
			return;
		}
	}

	/* Kick off transition — frame tick will draw the black fade */
	s_attract.state = ATTRACT_STATE_TRANSITIONING;
	s_attract.transitionStartTime = cls.realtime;
	s_attract.transitionEndTime   = cls.realtime + s_attract.transitionMs;
}

/* ── console commands ────────────────────────────────────────────────── */
static void Attract_Start_f( void ) { WiredAttract_Start(); }
static void Attract_Stop_f( void )  { WiredAttract_Stop(); }
static void Attract_Skip_f( void )  { WiredAttract_Skip(); }

static void Attract_Restart_f( void ) {
	WiredAttract_Stop();
	s_attract.currentIndex = 0;
	WiredAttract_Start();
}

static void Attract_Status_f( void ) {
	static const char *stateNames[] = {
		"IDLE", "WAITING", "STARTING", "PLAYING", "TRANSITIONING", "STOPPED"
	};
	int state = (int)s_attract.state;
	const char *stateName = ( state >= 0 && state <= 5 ) ? stateNames[state] : "?";

	Com_Printf( "attract_status:\n" );
	Com_Printf( "  state         : %s\n", stateName );
	Com_Printf( "  playlist      : %d items (loop=%d shuffle=%d)\n",
	            s_attract.playlistCount, s_attract.loop, s_attract.shuffle );
	Com_Printf( "  currentIndex  : %d\n", s_attract.currentIndex );
	Com_Printf( "  itemElapsed   : %d ms\n",
	            s_attract.state == ATTRACT_STATE_PLAYING
	            ? (int)( cls.realtime - s_attract.itemStartTime ) : 0 );
	Com_Printf( "  attract_delay : %d s\n",
	            s_attract.cvDelay ? s_attract.cvDelay->integer : 0 );
	Com_Printf( "  attract_volume: %.2f\n",
	            s_attract.cvVolume ? s_attract.cvVolume->value : 0.0f );
	Com_Printf( "  ownsDemo      : %d (demoplaying=%d)\n",
	            s_attract.ownsDemo, clc.demoplaying );
	Com_Printf( "  wiredHealthy  : %d\n", WiredUI_IsHealthy() );
	Com_Printf( "  recoveryFail  : %d ms ago\n",
	            WiredUI_GetLastRecoveryFailTime() != 0
	            ? (int)( cls.realtime - WiredUI_GetLastRecoveryFailTime() ) : -1 );
	Com_Printf( "  prevHadError  : %d\n", s_attract.prevHadError );
	Com_Printf( "  idle-detect   : keyboard only (mouse gated on KEYCATCH_UI)\n" );
}

/* ── Lua bindings ────────────────────────────────────────────────────── */

static int AttractLua_Add( lua_State *L ) {
	const char *kindStr, *src;
	int kind, durationMs;
	wiredAttractItem_t *item;

	if ( lua_gettop( L ) < 2 ) {
		return luaL_error( L, "attract.add(kind, source [, duration_ms])" );
	}

	kindStr = luaL_checkstring( L, 1 );
	src     = luaL_checkstring( L, 2 );
	durationMs = lua_gettop( L ) >= 3 ? (int)luaL_checkinteger( L, 3 ) : 0;

	if ( Q_stricmp( kindStr, "cinematic" ) == 0 )      kind = ATTRACT_ITEM_CINEMATIC;
	else if ( Q_stricmp( kindStr, "demo" ) == 0 )      kind = ATTRACT_ITEM_DEMO;
	else if ( Q_stricmp( kindStr, "panel" ) == 0 )     kind = ATTRACT_ITEM_PANEL;
	else return luaL_error( L, "attract.add: unknown kind '%s' (cinematic|demo|panel)", kindStr );

	if ( !Attract_ValidateSource( kind, src ) ) {
		return luaL_error( L, "attract.add: invalid source '%s' for kind '%s'", src, kindStr );
	}

	if ( s_attract.playlistCount >= WIRED_ATTRACT_MAX_ITEMS ) {
		return luaL_error( L, "attract.add: playlist full (%d items)", WIRED_ATTRACT_MAX_ITEMS );
	}

	item = &s_attract.playlist[s_attract.playlistCount];
	item->kind       = kind;
	item->durationMs = durationMs;
	Q_strncpyz( item->source, src, sizeof( item->source ) );
	s_attract.playlistCount++;
	return 0;
}

static int AttractLua_Clear( lua_State *L ) {
	(void)L;
	s_attract.playlistCount = 0;
	s_attract.currentIndex  = 0;
	return 0;
}

static int AttractLua_SetLoop( lua_State *L ) {
	s_attract.loop = lua_toboolean( L, 1 ) ? qtrue : qfalse;
	return 0;
}

static int AttractLua_SetShuffle( lua_State *L ) {
	s_attract.shuffle = lua_toboolean( L, 1 ) ? qtrue : qfalse;
	return 0;
}

static int AttractLua_SetTransition( lua_State *L ) {
	int ms = (int)luaL_checkinteger( L, 1 );
	if ( ms < 0 ) ms = 0;
	s_attract.transitionMs = ms;
	return 0;
}

static const luaL_Reg s_attractLib[] = {
	{ "add",            AttractLua_Add },
	{ "clear",          AttractLua_Clear },
	{ "set_loop",       AttractLua_SetLoop },
	{ "set_shuffle",    AttractLua_SetShuffle },
	{ "set_transition", AttractLua_SetTransition },
	{ NULL, NULL }
};

static void Attract_LuaRegister( lua_State *L ) {
	luaL_newlib( L, s_attractLib );
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
	Com_Memset( &s_attract, 0, sizeof( s_attract ) );

	s_attract.transitionMs = 500; /* 250ms out + 250ms in */
	s_attract.loop         = qtrue;
	s_attract.prevClientState = cls.state;

	s_attract.cvEnabled = Cvar_Get( "attract_enabled", "1", CVAR_ARCHIVE );
	s_attract.cvDelay   = Cvar_Get( "attract_delay",  "30", CVAR_ARCHIVE );
	s_attract.cvVolume  = Cvar_Get( "attract_volume", "0.5", CVAR_ARCHIVE );

	Cmd_AddCommand( "attract_start",   Attract_Start_f );
	Cmd_AddCommand( "attract_stop",    Attract_Stop_f );
	Cmd_AddCommand( "attract_skip",    Attract_Skip_f );
	Cmd_AddCommand( "attract_restart", Attract_Restart_f );
	Cmd_AddCommand( "attract_status",  Attract_Status_f );

	/* Binding was already registered in WiredAttract_LuaInit (called from
	   CL_Init before WiredScript_PostInit).  Just exec the playlist now. */
	WiredScript_ExecFile( "scripts/attract.lua" );

	s_attract.initialized = qtrue;
	Com_Printf( "WiredAttract: initialized (%d items in playlist)\n",
	            s_attract.playlistCount );
}

void WiredAttract_Shutdown( void ) {
	if ( !s_attract.initialized ) return;

	Attract_Teardown();

	Cmd_RemoveCommand( "attract_start" );
	Cmd_RemoveCommand( "attract_stop" );
	Cmd_RemoveCommand( "attract_skip" );
	Cmd_RemoveCommand( "attract_restart" );
	Cmd_RemoveCommand( "attract_status" );

	s_attract.initialized = qfalse;
}

/* ── per-frame tick ──────────────────────────────────────────────────── */
void WiredAttract_Frame( int msec ) {
	int curState, delayMs;

	(void)msec;

	if ( !s_attract.initialized ) return;
	if ( !s_attract.cvEnabled || !s_attract.cvEnabled->integer ) return;

	/* ── edge detect cls.state (connect / disconnect) ─────────────── */
	if ( s_attract.prevClientState == CA_ACTIVE && cls.state < CA_ACTIVE ) {
		/* Just disconnected — start idle timer */
		s_attract.disconnectTime = cls.realtime;
		if ( s_attract.state != ATTRACT_STATE_STOPPED ) {
			s_attract.state = ATTRACT_STATE_WAITING;
		}
	} else if ( s_attract.prevClientState < CA_ACTIVE && cls.state == CA_ACTIVE ) {
		/* Just connected — stop attract */
		WiredAttract_Stop();
	}
	s_attract.prevClientState = cls.state;

	/* ── edge detect Com_HasLastError — stop attract immediately ──── */
	{
		qboolean hasError = Com_HasLastError();
		if ( !s_attract.prevHadError && hasError ) {
			Attract_Teardown();
			s_attract.ownsDemo = qfalse;
			s_attract.state    = ATTRACT_STATE_WAITING;
		}
		s_attract.prevHadError = hasError;
	}

	/* ── demo ownership sanity (handles /disconnect during attract) ── */
	if ( s_attract.state == ATTRACT_STATE_PLAYING &&
	     s_attract.ownsDemo && !clc.demoplaying ) {
		s_attract.ownsDemo = qfalse;
		Attract_Advance();
		return;
	}

	/* ── gate: only advance state machine while disconnected and idle ─ */
	if ( cls.state >= CA_ACTIVE ) return;
	if ( Com_HasLastError() ) return;

	/* Don't fight user menus — only run when attract owns the stack */
	{
		int depth = WiredUI_GetMenuStackDepth();
		if ( depth > 0 ) {
			const char *top = WiredUI_GetMenuStackTop();
			if ( s_attract.pushedPanel[0] == '\0' ||
			     Q_stricmp( top, s_attract.pushedPanel ) != 0 ) {
				/* A user menu is on top that we didn't push — back off */
				goto draw_transition;
			}
		}
	}

	/* ── state dispatch ──────────────────────────────────────────────── */
	curState = (int)s_attract.state;
	delayMs  = s_attract.cvDelay ? s_attract.cvDelay->integer * 1000 : 30000;

	switch ( curState ) {

	case ATTRACT_STATE_IDLE:
		/* No playlist — nothing to do */
		break;

	case ATTRACT_STATE_WAITING:
		if ( s_attract.playlistCount == 0 ) break;
		/* Require both disconnect-idle AND input-idle to have elapsed */
		if ( (int)( cls.realtime - s_attract.disconnectTime ) >= delayMs &&
		     (int)( cls.realtime - s_attract.lastInputTime )  >= delayMs ) {
			WiredAttract_Start();
		}
		break;

	case ATTRACT_STATE_STARTING:
		Attract_DispatchCurrent();
		break;

	case ATTRACT_STATE_PLAYING:
		if ( s_attract.playlistCount == 0 ) break;
		{
			wiredAttractItem_t *item = &s_attract.playlist[s_attract.currentIndex];
			if ( item->durationMs > 0 &&
			     (int)( cls.realtime - s_attract.itemStartTime ) >= item->durationMs ) {
				Attract_Advance();
			}
		}
		break;

	case ATTRACT_STATE_TRANSITIONING:
		if ( (int)( cls.realtime - s_attract.transitionStartTime ) >= s_attract.transitionMs ) {
			s_attract.state = ATTRACT_STATE_STARTING;
		}
		break;

	case ATTRACT_STATE_STOPPED:
		/* Manually stopped — do not auto-restart; wait for next disconnect event */
		break;
	}

draw_transition:
	/* ── draw transition overlay ─────────────────────────────────────── */
	if ( s_attract.state == ATTRACT_STATE_TRANSITIONING ) {
		int   halfMs   = s_attract.transitionMs / 2;
		int   elapsed  = (int)( cls.realtime - s_attract.transitionStartTime );
		float alpha;
		vec4_t c;

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

	s_attract.lastInputTime = cls.realtime;

	if ( s_attract.state == ATTRACT_STATE_PLAYING ||
	     s_attract.state == ATTRACT_STATE_TRANSITIONING ) {
		WiredAttract_Stop();
	}
}

void WiredAttract_NoteMouse( int dx, int dy ) {
	/* 64 squared = 8px threshold — filters optical-mouse jitter */
	if ( dx * dx + dy * dy <= 64 ) return;

	s_attract.lastInputTime = cls.realtime;

	if ( s_attract.state == ATTRACT_STATE_PLAYING ||
	     s_attract.state == ATTRACT_STATE_TRANSITIONING ) {
		WiredAttract_Stop();
	}
}

/* ── SafeReload hook ─────────────────────────────────────────────────── */
void WiredAttract_OnMenuReload( void ) {
	/* The pool was rebuilt — our pushedPanel pointer may be stale.
	   Clear it and re-dispatch the current item on the next frame so
	   the panel gets pushed against the fresh pool. */
	if ( s_attract.state == ATTRACT_STATE_PLAYING &&
	     s_attract.pushedPanel[0] != '\0' ) {
		s_attract.pushedPanel[0] = '\0';
		s_attract.state = ATTRACT_STATE_STARTING;
	}
}

/* ── control ─────────────────────────────────────────────────────────── */
void WiredAttract_Start( void ) {
	if ( !s_attract.initialized ) return;
	if ( s_attract.playlistCount == 0 ) {
		Com_Printf( "attract: playlist is empty\n" );
		return;
	}
	s_attract.currentIndex  = 0;
	s_attract.lastInputTime = cls.realtime;
	s_attract.state         = ATTRACT_STATE_STARTING;
}

void WiredAttract_Stop( void ) {
	if ( !s_attract.initialized ) return;
	Attract_Teardown();
	s_attract.state = ATTRACT_STATE_STOPPED;
}

void WiredAttract_Skip( void ) {
	if ( !s_attract.initialized ) return;
	if ( s_attract.state == ATTRACT_STATE_PLAYING ||
	     s_attract.state == ATTRACT_STATE_STARTING ) {
		Attract_Advance();
	}
}

/* ── query ───────────────────────────────────────────────────────────── */
wiredAttractState_t WiredAttract_GetState( void ) {
	return s_attract.state;
}

qboolean WiredAttract_IsActive( void ) {
	return s_attract.initialized &&
	       ( s_attract.state == ATTRACT_STATE_PLAYING ||
	         s_attract.state == ATTRACT_STATE_STARTING ||
	         s_attract.state == ATTRACT_STATE_TRANSITIONING );
}

/* ── completion callbacks ────────────────────────────────────────────── */
qboolean WiredAttract_OnDemoCompleted( void ) {
	if ( !s_attract.initialized ) return qfalse;
	if ( !s_attract.ownsDemo )    return qfalse;
	if ( s_attract.state != ATTRACT_STATE_PLAYING ) return qfalse;

	s_attract.ownsDemo = qfalse;
	Attract_Advance();
	return qtrue;
}

qboolean WiredAttract_OnCinematicCompleted( void ) {
	/* MVP: cinematic items use explicit durationMs — natural completion
	   is not relied on. This hook is a no-op placeholder for future use. */
	return qfalse;
}

#endif /* FEAT_WIRED_UI */
