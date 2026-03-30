/*
===========================================================================
cl_wired_ui.c — Wired UI: unified menu/HUD system implementation

Phase 1 stub: establishes the integration points and symbol/element
registration framework. Actual menu parsing will be extracted from
code/ui/ui_shared.c in subsequent phases.
===========================================================================
*/

#include "../client.h"
#include "cl_wired_ui.h"
#include "cl_wired_hud.h"
#include "cl_wired_hud_private.h"
#include "cl_wired_fonts.h"
#include "../../ui/menudef.h"

#if FEAT_WIRED_UI

// from cl_wired_hud_registry.c
extern void     WiredHud_DestroyAllElements( void );
extern int      WiredHud_GetElementCount( void );
// from cl_wired_hud.c
extern void     WiredHud_LoadFromMenus( void );

// ── symbol registry ───────────────────────────────────────────────────

#define WIRED_MAX_SYMBOLS  256

typedef struct {
	char                    name[64];
	wiredSymbolCallback_t   callback;
	void                   *userData;
	qboolean                active;
} wiredSymbol_t;

static wiredSymbol_t  wired_symbols[WIRED_MAX_SYMBOLS];
static int            wired_numSymbols = 0;

// ── element registry ──────────────────────────────────────────────────

#define WIRED_MAX_ELEMENTS  256

typedef struct {
	char                     name[64];
	wiredElementCreate_t     create;
	wiredElementRoutine_t    routine;
	wiredElementDestroy_t    destroy;
	qboolean                 active;
} wiredElement_t;

static wiredElement_t  wired_elements[WIRED_MAX_ELEMENTS];
static int             wired_numElements = 0;

// ── feeder registry ───────────────────────────────────────────────────

typedef struct {
	int                       feederID;
	wiredFeederCount_t        count;
	wiredFeederItemText_t     itemText;
	wiredFeederSelection_t    selection;
	qboolean                  active;
} wiredFeeder_t;

static wiredFeeder_t  wired_feeders[WIRED_MAX_FEEDERS];
static int            wired_numFeeders = 0;

// ── menu interaction sounds ──────────────────────────────────────────
static sfxHandle_t    wired_sfxFocus;     // item gains focus (hover/arrow key)
static sfxHandle_t    wired_sfxAction;    // button click / action execution
static sfxHandle_t    wired_sfxMenuOpen;  // menu push onto stack
static sfxHandle_t    wired_sfxMenuClose; // menu pop from stack

// ── cursor shader ────────────────────────────────────────────────────
static qhandle_t      wired_cursorShader;

// ── asset globals (parsed from assetGlobalDef) ──────────────────────
static wiredAssetGlobals_t wired_assetGlobals;
static qhandle_t           wired_gradientBarShader;

wiredAssetGlobals_t *WiredUI_GetAssetGlobals( void ) {
	return &wired_assetGlobals;
}

// ── theme system ────────────────────────────────────────────────────
// Returns the manifest path based on ui_theme cvar.
// Empty ui_theme → "ui/menus.txt" (default)
// ui_theme "ta" → "ui/themes/ta/menus.txt"
static const char *WiredUI_GetManifestPath( void ) {
	static char path[MAX_QPATH];
	char theme[64];

	Cvar_VariableStringBuffer( "ui_theme", theme, sizeof( theme ) );
	if ( theme[0] ) {
		Com_sprintf( path, sizeof( path ), "ui/themes/%s/menus.txt", theme );
		// verify the manifest exists
		fileHandle_t f;
		int len = FS_FOpenFileRead( path, &f, qfalse );
		if ( f != FS_INVALID_HANDLE ) {
			FS_FCloseFile( f );
			return path;
		}
		Com_Printf( S_COLOR_YELLOW "WiredUI: theme '%s' not found — staying on current theme.\n", theme );
		return "ui/menus.txt";
	}
	return "ui/menus.txt";
}

void WiredUI_RegisterFeeder( int feederID, wiredFeederCount_t count,
                              wiredFeederItemText_t itemText,
                              wiredFeederSelection_t selection ) {
	int i;
	// update existing
	for ( i = 0; i < wired_numFeeders; i++ ) {
		if ( wired_feeders[i].active && wired_feeders[i].feederID == feederID ) {
			wired_feeders[i].count = count;
			wired_feeders[i].itemText = itemText;
			wired_feeders[i].selection = selection;
			return;
		}
	}
	if ( wired_numFeeders >= WIRED_MAX_FEEDERS ) return;
	wired_feeders[wired_numFeeders].feederID = feederID;
	wired_feeders[wired_numFeeders].count = count;
	wired_feeders[wired_numFeeders].itemText = itemText;
	wired_feeders[wired_numFeeders].selection = selection;
	wired_feeders[wired_numFeeders].active = qtrue;
	wired_numFeeders++;
}

int WiredUI_FeederCount( int feederID ) {
	int i;
	for ( i = 0; i < wired_numFeeders; i++ ) {
		if ( wired_feeders[i].active && wired_feeders[i].feederID == feederID && wired_feeders[i].count ) {
			return wired_feeders[i].count( feederID );
		}
	}
	return 0;
}

const char *WiredUI_FeederItemText( int feederID, int index, int column ) {
	int i;
	for ( i = 0; i < wired_numFeeders; i++ ) {
		if ( wired_feeders[i].active && wired_feeders[i].feederID == feederID && wired_feeders[i].itemText ) {
			return wired_feeders[i].itemText( feederID, index, column );
		}
	}
	return "";
}

void WiredUI_FeederSelection( int feederID, int index ) {
	int i;
	for ( i = 0; i < wired_numFeeders; i++ ) {
		if ( wired_feeders[i].active && wired_feeders[i].feederID == feederID && wired_feeders[i].selection ) {
			wired_feeders[i].selection( feederID, index );
			return;
		}
	}
}

// ── state ─────────────────────────────────────────────────────────────

static qboolean  wired_initialized = qfalse;
static int       wired_activeMenu = UIMENU_NONE;

// ── menu stack ────────────────────────────────────────────────────────
// Supports open/close navigation between screens (e.g., Main → Options → Video).
// Each entry is a menu name. ESC or "close" pops the stack.
// The bottom of the stack is always the root menu (main or ingame).

static char      wired_menuStack[WIRED_MENU_STACK_DEPTH][64];
static int       wired_menuStackDepth = 0;

// ── cursor ────────────────────────────────────────────────────────────

// ── key binding capture state ─────────────────────────────────────────
static qboolean  wired_waitingForKey = qfalse;
static wiredItemDef_t *wired_bindItem = NULL;

// ── text field editing state ──────────────────────────────────────────
static qboolean       wired_editingField = qfalse;
static wiredItemDef_t *wired_editItem = NULL;
static int            wired_editCursorPos = 0;
static int            wired_editPaintOffset = 0;

static float     wired_cursorX = 320.0f;
static float     wired_cursorY = 240.0f;
static int       wired_focusItem = -1;     // index of focused item
static qboolean  wired_focusFromMouse = qfalse;  // qtrue if focus came from mouse hover

// ── double-click detection ───────────────────────────────────────────
#define WIRED_DOUBLECLICK_TIME  300   // ms
static int       wired_lastClickTime = 0;
static int       wired_lastClickRow = -1;
static float     wired_lastClickFeeder = 0;

// ── symbol registration ───────────────────────────────────────────────

void WiredUI_RegisterSymbol( const char *name, wiredSymbolCallback_t callback, void *userData ) {
	int i;

	if ( !name || !name[0] || !callback ) {
		Com_Printf( S_COLOR_YELLOW "WiredUI_RegisterSymbol: invalid args\n" );
		return;
	}

	// check for existing symbol (update in place)
	for ( i = 0; i < wired_numSymbols; i++ ) {
		if ( wired_symbols[i].active && !Q_stricmp( wired_symbols[i].name, name ) ) {
			wired_symbols[i].callback = callback;
			wired_symbols[i].userData = userData;
			return;
		}
	}

	if ( wired_numSymbols >= WIRED_MAX_SYMBOLS ) {
		Com_Printf( S_COLOR_YELLOW "WiredUI_RegisterSymbol: too many symbols (max %d)\n", WIRED_MAX_SYMBOLS );
		return;
	}

	Q_strncpyz( wired_symbols[wired_numSymbols].name, name, sizeof( wired_symbols[0].name ) );
	wired_symbols[wired_numSymbols].callback = callback;
	wired_symbols[wired_numSymbols].userData = userData;
	wired_symbols[wired_numSymbols].active = qtrue;
	wired_numSymbols++;
}

void WiredUI_UnregisterSymbol( const char *name ) {
	int i;
	for ( i = 0; i < wired_numSymbols; i++ ) {
		if ( wired_symbols[i].active && !Q_stricmp( wired_symbols[i].name, name ) ) {
			wired_symbols[i].active = qfalse;
			return;
		}
	}
}

const char *WiredUI_ResolveSymbol( const char *name ) {
	int i;
	for ( i = 0; i < wired_numSymbols; i++ ) {
		if ( wired_symbols[i].active && !Q_stricmp( wired_symbols[i].name, name ) ) {
			return wired_symbols[i].callback( wired_symbols[i].userData );
		}
	}
	return "???";
}

// ── element registration ──────────────────────────────────────────────

void WiredUI_RegisterElement( const char *name,
                               wiredElementCreate_t create,
                               wiredElementRoutine_t routine,
                               wiredElementDestroy_t destroy ) {
	int i;

	if ( !name || !name[0] ) {
		Com_Printf( S_COLOR_YELLOW "WiredUI_RegisterElement: invalid name\n" );
		return;
	}

	// check for existing element (update in place)
	for ( i = 0; i < wired_numElements; i++ ) {
		if ( wired_elements[i].active && !Q_stricmp( wired_elements[i].name, name ) ) {
			wired_elements[i].create = create;
			wired_elements[i].routine = routine;
			wired_elements[i].destroy = destroy;
			return;
		}
	}

	if ( wired_numElements >= WIRED_MAX_ELEMENTS ) {
		Com_Printf( S_COLOR_YELLOW "WiredUI_RegisterElement: too many elements (max %d)\n", WIRED_MAX_ELEMENTS );
		return;
	}

	Q_strncpyz( wired_elements[wired_numElements].name, name, sizeof( wired_elements[0].name ) );
	wired_elements[wired_numElements].create = create;
	wired_elements[wired_numElements].routine = routine;
	wired_elements[wired_numElements].destroy = destroy;
	wired_elements[wired_numElements].active = qtrue;
	wired_numElements++;
}

// ── batch registration stubs ──────────────────────────────────────────
// These will be filled in Phase 3 when SuperHUD elements are wrapped.

void WiredUI_RegisterCoreSymbols( void ) {
	Com_Printf( "WiredUI: core symbols registered (stub — Phase 3)\n" );
}

void WiredUI_RegisterCoreElements( void ) {
	Com_Printf( "WiredUI: core elements registered (stub — Phase 3)\n" );
}

// ── public API ────────────────────────────────────────────────────────

// ── delayed screenshot ────────────────────────────────────────────────
// +set wired_screenshotDelay N triggers screenshotJPEG after N seconds.
// Useful for automated testing: make run-game DEV=1 +set wired_screenshotDelay 5

static cvar_t *wired_screenshotDelay = NULL;
static int     wired_screenshotTime = 0;
static qboolean wired_screenshotTaken = qfalse;

void WiredUI_Init( qboolean inGameUI ) {
	Com_Printf( "------- WiredUI_Init -------\n" );

	Com_Memset( wired_symbols, 0, sizeof( wired_symbols ) );
	Com_Memset( wired_elements, 0, sizeof( wired_elements ) );
	wired_numSymbols = 0;
	wired_numElements = 0;

	// load menu files from manifest (theme-aware)
	WiredUI_ClearMenus();
	WiredUI_LoadMenus( WiredUI_GetManifestPath() );

	// register feeder data sources
	WiredUI_RegisterCoreFeeders();

	// Phase 3: initialize HUD subsystem (fonts + state bridge)
	WiredHud_Init();

	// hot reload commands
	Cmd_AddCommand( "hud_reload", WiredUI_ReloadHud );
	Cmd_AddCommand( "menu_reload", WiredUI_ReloadMenus );

	// initialize asset globals with TA-style defaults
	memset( &wired_assetGlobals, 0, sizeof( wired_assetGlobals ) );
	Q_strncpyz( wired_assetGlobals.cursor, "ui/assets/3_cursor3", sizeof( wired_assetGlobals.cursor ) );
	Q_strncpyz( wired_assetGlobals.gradientBar, "ui/assets/gradientbar2.tga", sizeof( wired_assetGlobals.gradientBar ) );
	Q_strncpyz( wired_assetGlobals.font, "fonts/font", sizeof( wired_assetGlobals.font ) );
	wired_assetGlobals.fontSize = 16;
	Q_strncpyz( wired_assetGlobals.smallFont, "fonts/smallfont", sizeof( wired_assetGlobals.smallFont ) );
	wired_assetGlobals.smallFontSize = 12;
	Q_strncpyz( wired_assetGlobals.bigFont, "fonts/bigfont", sizeof( wired_assetGlobals.bigFont ) );
	wired_assetGlobals.bigFontSize = 20;
	wired_assetGlobals.fadeClamp = 1.0f;
	wired_assetGlobals.fadeCycle = 1;
	wired_assetGlobals.fadeAmount = 0.2f;  // fast 150ms transitions
	Vector4Set( wired_assetGlobals.shadowColor, 0.1f, 0.1f, 0.1f, 0.25f );
	Q_strncpyz( wired_assetGlobals.focusSound, "sound/misc/menu2.wav", sizeof( wired_assetGlobals.focusSound ) );
	Vector4Set( wired_assetGlobals.focusColor, 1.0f, 0.75f, 0.0f, 1.0f );  // TA gold

	// register menu interaction sounds
	wired_sfxFocus     = S_RegisterSound( "sound/misc/menu2.wav", qfalse );
	wired_sfxAction    = S_RegisterSound( "sound/misc/menu1.wav", qfalse );
	wired_sfxMenuOpen  = S_RegisterSound( "sound/misc/menu3.wav", qfalse );
	wired_sfxMenuClose = S_RegisterSound( "sound/misc/menu3.wav", qfalse );

	// register cursor shader — try cvar override, then assetGlobals, then legacy fallback
	{
		char cursorPath[MAX_QPATH];
		Cvar_VariableStringBuffer( "wired_cursor", cursorPath, sizeof( cursorPath ) );
		if ( cursorPath[0] ) {
			wired_cursorShader = re.RegisterShaderNoMip( cursorPath );
		}
		if ( !wired_cursorShader ) {
			wired_cursorShader = re.RegisterShaderNoMip( wired_assetGlobals.cursor );
		}
		if ( !wired_cursorShader ) {
			wired_cursorShader = re.RegisterShaderNoMip( "menu/art/3_cursor2" );
		}
	}

	// register gradient bar shader
	if ( wired_assetGlobals.gradientBar[0] ) {
		wired_gradientBarShader = re.RegisterShaderNoMip( wired_assetGlobals.gradientBar );
	}

	// load TA fonts (fontInfo_t-based, for v6/TA menu text rendering)
	WiredUI_LoadTAFonts();

	wired_activeMenu = UIMENU_NONE;
	wired_initialized = qtrue;

	// restore menu stack after vid_restart
	{
		char stackBuf[512];
		Cvar_VariableStringBuffer( "wired_menuStackSaved", stackBuf, sizeof( stackBuf ) );
		if ( stackBuf[0] ) {
			int savedMenu = Cvar_VariableIntegerValue( "wired_activeMenuSaved" );
			char *p = stackBuf;
			char *tok;

			if ( savedMenu > UIMENU_NONE ) {
				wired_activeMenu = savedMenu;
				Key_SetCatcher( KEYCATCH_UI );
				if ( savedMenu == UIMENU_INGAME ) {
					Cvar_Set( "cl_paused", "1" );
				}
			}

			// push each menu from saved stack
			while ( ( tok = strchr( p, ';' ) ) != NULL || *p ) {
				char name[64];
				int len;
				if ( tok ) {
					len = (int)( tok - p );
					if ( len >= (int)sizeof( name ) ) len = sizeof( name ) - 1;
					Q_strncpyz( name, p, len + 1 );
					p = tok + 1;
				} else {
					Q_strncpyz( name, p, sizeof( name ) );
					p += strlen( p );
				}
				if ( name[0] && WiredUI_FindMenu( name ) && wired_menuStackDepth < WIRED_MENU_STACK_DEPTH ) {
					Q_strncpyz( wired_menuStack[wired_menuStackDepth], name, sizeof( wired_menuStack[0] ) );
					wired_menuStackDepth++;
				}
				if ( !*p ) break;
			}

			// clear saved state
			Cvar_Set( "wired_menuStackSaved", "" );
			Cvar_Set( "wired_activeMenuSaved", "0" );

			if ( wired_menuStackDepth > 0 ) {
				Com_Printf( "WiredUI: restored menu stack (depth %d)\n", wired_menuStackDepth );
			}
		}
	}

	// delayed screenshot support
	wired_screenshotDelay = Cvar_Get( "wired_screenshotDelay", "0", 0 );
	wired_screenshotTime = cls.realtime;
	wired_screenshotTaken = qfalse;

	Com_Printf( "WiredUI: initialized (%d menus loaded)\n", WiredUI_GetMenuCount() );
}

void WiredUI_Shutdown( void ) {
	if ( !wired_initialized ) {
		return;
	}

	// save menu stack to cvar so vid_restart can restore it
	{
		char stackBuf[512] = "";
		int i;
		for ( i = 0; i < wired_menuStackDepth; i++ ) {
			if ( i > 0 ) Q_strcat( stackBuf, sizeof( stackBuf ), ";" );
			Q_strcat( stackBuf, sizeof( stackBuf ), wired_menuStack[i] );
		}
		Cvar_Set( "wired_menuStackSaved", stackBuf );
		Cvar_Set( "wired_activeMenuSaved", va( "%d", wired_activeMenu ) );
	}

	WiredHud_DestroyAllElements();
	Cmd_RemoveCommand( "hud_reload" );
	Cmd_RemoveCommand( "menu_reload" );

	Com_Memset( wired_symbols, 0, sizeof( wired_symbols ) );
	Com_Memset( wired_elements, 0, sizeof( wired_elements ) );
	wired_numSymbols = 0;
	wired_numElements = 0;
	wired_activeMenu = UIMENU_NONE;
	wired_initialized = qfalse;

	Com_Printf( "WiredUI: shutdown\n" );
}

void WiredUI_Refresh( int realtime ) {
	wiredMenuDef_t *menu;
	int i;

	if ( !wired_initialized ) {
		return;
	}

	// delayed screenshot — fire once after N seconds
	if ( wired_screenshotDelay && wired_screenshotDelay->integer > 0 && !wired_screenshotTaken ) {
		if ( realtime - wired_screenshotTime >= wired_screenshotDelay->integer * 1000 ) {
			Cbuf_ExecuteText( EXEC_APPEND, "screenshotJPEG\n" );
			wired_screenshotTaken = qtrue;
			Com_Printf( "WiredUI: delayed screenshot taken\n" );
		}
	}

	// find the active menu (top of stack, or root)
	menu = WiredUI_GetActiveMenu();

	if ( !menu ) {
		return;
	}

	// auto-refresh server pings when server browser is visible
	{
		wiredMenuDef_t *serverMenu = WiredUI_FindMenu( "servers" );
		if ( menu == serverMenu ) {
			static int lastPingUpdate = 0;
			if ( realtime - lastPingUpdate > 1000 ) {  // every second
				int uiSource = Cvar_VariableIntegerValue( "ui_netSource" );
				int engineSource;
				extern qboolean CL_UpdateVisiblePings_f( int source );

				// map UI source values to engine AS_* constants
				if ( uiSource == 0 )       engineSource = AS_LOCAL;
				else if ( uiSource == 6 )  engineSource = AS_FAVORITES;
				else                       engineSource = AS_GLOBAL;

				CL_UpdateVisiblePings_f( engineSource );
				lastPingUpdate = realtime;
			}
		}
	}

	// TODO: animated cloud background — deferred (tcmod scroll not animating in 2D path)
	// See TODOS.md "Wired UI: animated cloud menu background"

	// menu origin offset — non-fullscreen menus position items relative to rect
	float menuX = menu->fullscreen ? 0 : menu->rect.x;
	float menuY = menu->fullscreen ? 0 : menu->rect.y;
	float menuW = menu->fullscreen ? SCREEN_WIDTH : menu->rect.w;
	float menuH = menu->fullscreen ? SCREEN_HEIGHT : menu->rect.h;
	float scrollY = menu->scrollOffset;  // scroll offset applied to all items
	float clipTop = menuY;               // visible area top
	float clipBottom = menuY + menuH;    // visible area bottom

	// render background — supports FILLED, SHADER, GRADIENT, and CINEMATIC styles
	{
		float bgX = menu->fullscreen ? 0 : menu->rect.x;
		float bgY = menu->fullscreen ? 0 : menu->rect.y;
		float bgW = menu->fullscreen ? SCREEN_WIDTH : menu->rect.w;
		float bgH = menu->fullscreen ? SCREEN_HEIGHT : menu->rect.h;

		if ( menu->style == WINDOW_STYLE_CINEMATIC && menu->cinematicHandle >= 0 ) {
			// WINDOW_STYLE_CINEMATIC: run + draw ROQ video as background
			CIN_SetExtents( menu->cinematicHandle, (int)bgX, (int)bgY, (int)bgW, (int)bgH );
			CIN_RunCinematic( menu->cinematicHandle );
			CIN_DrawCinematic( menu->cinematicHandle );
		} else if ( menu->style == WINDOW_STYLE_SHADER && menu->background[0] ) {
			// WINDOW_STYLE_SHADER: draw background image/shader
			qhandle_t bgShader = re.RegisterShaderNoMip( menu->background );
			if ( bgShader ) {
				re.SetColor( NULL );
				SCR_DrawPic( bgX, bgY, bgW, bgH, bgShader );
			} else if ( menu->backcolor[3] > 0.0f ) {
				SCR_FillRect( bgX, bgY, bgW, bgH, menu->backcolor );
			}
		} else if ( menu->style == WINDOW_STYLE_GRADIENT && menu->backcolor[3] > 0.0f ) {
			// WINDOW_STYLE_GRADIENT: solid fill + gradient bar overlay
			SCR_FillRect( bgX, bgY, bgW, bgH, menu->backcolor );
			if ( wired_gradientBarShader ) {
				vec4_t gradColor;
				Vector4Copy( menu->backcolor, gradColor );
				gradColor[3] *= 0.5f;  // semi-transparent gradient overlay
				re.SetColor( gradColor );
				SCR_DrawPic( bgX, bgY, bgW, bgH, wired_gradientBarShader );
				re.SetColor( NULL );
			}
		} else if ( menu->style == WINDOW_STYLE_FILLED && menu->backcolor[3] > 0.0f ) {
			SCR_FillRect( bgX, bgY, bgW, bgH, menu->backcolor );
		} else if ( menu->style == WINDOW_STYLE_EMPTY ) {
			// no background — transparent
		} else {
			// fallback: dark panel
			vec4_t bgColor = { 0.1f, 0.1f, 0.15f, 1.0f };
			SCR_FillRect( bgX, bgY, bgW, bgH, bgColor );
		}
	}

	// ── fade animation ──────────────────────────────────────────────
	// v6 menus fade in using fadeClamp/fadeCycle/fadeAmount from assetGlobalDef or menuDef
	{
		float fadeClamp = menu->fadeClamp > 0 ? menu->fadeClamp : 1.0f;
		int fadeCycle = menu->fadeCycle > 0 ? menu->fadeCycle : 1;
		float fadeAmount = menu->fadeAmount > 0 ? menu->fadeAmount : 0.1f;

		if ( menu->openTime > 0 && menu->fadeAlpha < fadeClamp ) {
			int elapsed = realtime - menu->openTime;
			int steps = elapsed / fadeCycle;
			menu->fadeAlpha = steps * fadeAmount;
			if ( menu->fadeAlpha > fadeClamp ) menu->fadeAlpha = fadeClamp;
		} else if ( menu->openTime == 0 ) {
			// first frame — start fade
			menu->openTime = realtime;
			menu->fadeAlpha = 0;
		}
	}

	// ── transition + fade interpolation ─────────────────────────────
	for ( i = 0; i < menu->itemCount; i++ ) {
		wiredItemDef_t *item = menu->items[i];

		// rect transition
		if ( item->transStartTime > 0 ) {
			int elapsed = realtime - item->transStartTime;
			if ( elapsed >= item->transDuration ) {
				item->rect = item->transTo;
				item->transStartTime = 0;
			} else {
				float t = (float)elapsed / (float)item->transDuration;
				item->rect.x = item->transFrom.x + ( item->transTo.x - item->transFrom.x ) * t;
				item->rect.y = item->transFrom.y + ( item->transTo.y - item->transFrom.y ) * t;
				item->rect.w = item->transFrom.w + ( item->transTo.w - item->transFrom.w ) * t;
				item->rect.h = item->transFrom.h + ( item->transTo.h - item->transFrom.h ) * t;
			}
		}

		// alpha fade (fadein/fadeout)
		if ( item->fadeStartTime > 0 ) {
			int elapsed = realtime - item->fadeStartTime;
			if ( elapsed >= item->fadeDurationItem ) {
				// fade complete
				item->fadeAlphaItem = item->fadeTargetAlpha;
				item->fadeStartTime = 0;
				if ( item->fadeTargetAlpha <= 0.0f ) {
					item->visible = qfalse;  // fadeout hides item when done
				}
			} else {
				float t = (float)elapsed / (float)item->fadeDurationItem;
				float startAlpha = ( item->fadeTargetAlpha > 0.5f ) ? 0.0f : 1.0f;
				item->fadeAlphaItem = startAlpha + ( item->fadeTargetAlpha - startAlpha ) * t;
			}
		}
	}

	// render items — coordinates are relative to menu origin for non-fullscreen
	for ( i = 0; i < menu->itemCount; i++ ) {
		wiredItemDef_t *item = menu->items[i];

		if ( !item->visible ) {
			continue;
		}

		// TA compat: ownerdrawFlag visibility gating
		if ( item->ownerdrawFlag && !WiredUI_OwnerDrawVisible( item->ownerdrawFlag ) ) {
			continue;
		}

		// cvarTest + showCvar/hideCvar conditional visibility
		if ( item->cvarTest[0] ) {
			char testBuf[256];
			Cvar_VariableStringBuffer( item->cvarTest, testBuf, sizeof( testBuf ) );

			// match as integer to handle cvarFloatList values ("5.000000" matches "5")
			if ( item->showCvar[0] ) {
				int testInt = atoi( testBuf );
				char testIntStr[16];
				Com_sprintf( testIntStr, sizeof( testIntStr ), "%d", testInt );
				if ( !strstr( item->showCvar, testBuf ) && !strstr( item->showCvar, testIntStr ) ) continue;
			}
			if ( item->hideCvar[0] ) {
				int testInt = atoi( testBuf );
				char testIntStr[16];
				Com_sprintf( testIntStr, sizeof( testIntStr ), "%d", testInt );
				if ( strstr( item->hideCvar, testBuf ) || strstr( item->hideCvar, testIntStr ) ) continue;
			}
		}

		// compute absolute item position (menu origin + scroll offset)
		float itemX = menuX + item->rect.x;
		float itemY = menuY + item->rect.y - scrollY;

		// clip items outside visible area
		if ( itemY + item->rect.h < clipTop || itemY > clipBottom ) {
			continue;
		}

		// apply fade alpha modulation (fadein/fadeout animation)
		float itemAlpha = 1.0f;
		if ( item->fadeStartTime > 0 || item->fadeAlphaItem < 1.0f ) {
			itemAlpha = item->fadeAlphaItem;
			if ( itemAlpha <= 0.01f ) continue;  // fully transparent — skip drawing
		}

		// draw item background if styled (modulated by itemAlpha)
		if ( item->style == WINDOW_STYLE_SHADER && item->background[0] ) {
			qhandle_t itemBg = re.RegisterShaderNoMip( item->background );
			if ( itemBg ) {
				vec4_t alphaColor = { 1, 1, 1, itemAlpha };
				re.SetColor( itemAlpha < 1.0f ? alphaColor : NULL );
				SCR_DrawPic( itemX, itemY, item->rect.w, item->rect.h, itemBg );
				re.SetColor( NULL );
			}
		} else if ( item->style == WINDOW_STYLE_GRADIENT && item->backcolor[3] > 0.0f ) {
			vec4_t bc;
			Vector4Copy( item->backcolor, bc );
			bc[3] *= itemAlpha;
			SCR_FillRect( itemX, itemY, item->rect.w, item->rect.h, bc );
			if ( wired_gradientBarShader ) {
				vec4_t gc;
				Vector4Copy( item->backcolor, gc );
				gc[3] *= 0.5f * itemAlpha;
				re.SetColor( gc );
				SCR_DrawPic( itemX, itemY, item->rect.w, item->rect.h, wired_gradientBarShader );
				re.SetColor( NULL );
			}
		} else if ( item->style == WINDOW_STYLE_FILLED && item->backcolor[3] > 0.0f ) {
			vec4_t bc;
			Vector4Copy( item->backcolor, bc );
			bc[3] *= itemAlpha;
			SCR_FillRect( itemX, itemY, item->rect.w, item->rect.h, bc );
		}

		// draw background image (levelshots, icons, etc.)
		// auto-update "mappreview" items from ui_mapLevelshot cvar
		if ( item->name[0] && !Q_stricmp( item->name, "mappreview" ) ) {
			char lsBuf[MAX_QPATH];
			Cvar_VariableStringBuffer( "ui_mapLevelshot", lsBuf, sizeof( lsBuf ) );
			if ( lsBuf[0] ) {
				Q_strncpyz( item->background, lsBuf, sizeof( item->background ) );
			}
		}
		if ( item->background[0] ) {
			qhandle_t bgShader = re.RegisterShaderNoMip( item->background );
			if ( bgShader ) {
				re.SetColor( NULL );
				SCR_DrawPic( itemX, itemY, item->rect.w, item->rect.h, bgShader );
			}
		}

		// draw OWNERDRAW items — TA compat (CG_OWNERDRAW_* dispatch)
		if ( item->type == ITEM_TYPE_OWNERDRAW && item->ownerdraw > 0 ) {
			WiredUI_OwnerDraw( item->ownerdraw, itemX, itemY,
				item->rect.w, item->rect.h, item->forecolor, item->textstyle );
			continue;  // ownerdraw items handle their own rendering entirely
		}

		// draw LISTBOX items — feeder-driven scrollable list
		if ( item->type == ITEM_TYPE_LISTBOX && item->feeder != 0 ) {
			int feederID = (int)item->feeder;
			int totalRows = WiredUI_FeederCount( feederID );
			float rowH = item->elementheight > 0 ? item->elementheight : 16.0f;
			int visibleRows = (int)( item->rect.h / rowH );
			int row, col;
			float charSize = item->textscale >= 0.7f ? 16.0f : 8.0f;
			vec4_t selColor = { 0.3f, 0.3f, 0.5f, 0.6f };
			vec4_t rowColor;
			float scrollBarW = 4.0f;
			float contentW = item->rect.w;

			// reserve space for scrollbar when content overflows
			if ( totalRows > visibleRows ) {
				contentW -= scrollBarW + 2.0f;
			}

			// draw list background
			if ( item->backcolor[3] > 0 ) {
				SCR_FillRect( itemX, itemY, item->rect.w, item->rect.h, item->backcolor );
			}

			// draw rows
			for ( row = 0; row < visibleRows && ( item->listScrollOffset + row ) < totalRows; row++ ) {
				int dataRow = item->listScrollOffset + row;
				float rowY = itemY + row * rowH;

				// highlight selected row
				if ( dataRow == item->listSelectedRow ) {
					SCR_FillRect( itemX, rowY, contentW, rowH, selColor );
				}

				// draw columns — clip text to column width
				Vector4Copy( item->forecolor, rowColor );
				float colX = itemX + 4;
				for ( col = 0; col < ( item->columns > 0 ? item->columns : 1 ); col++ ) {
					const char *text = WiredUI_FeederItemText( feederID, dataRow, col );
					float colW = ( col < item->columns && item->columnWidths[col] > 0 )
						? item->columnWidths[col] : contentW;
					if ( text && text[0] ) {
						// truncate to fit column width (account for ^X color codes)
						int maxChars = (int)( ( colW - 4 ) / charSize );
						int visChars = 0, ti;
						if ( maxChars < 1 ) maxChars = 1;
						// count visible (non-color-code) chars
						for ( ti = 0; text[ti]; ti++ ) {
							if ( Q_IsColorString( &text[ti] ) ) { ti++; continue; }
							visChars++;
						}
						if ( visChars > maxChars ) {
							char clipped[128];
							int ci = 0, vc = 0;
							// copy up to maxChars visible chars, preserving color codes
							for ( ti = 0; text[ti] && ci < (int)sizeof(clipped) - 1; ti++ ) {
								if ( Q_IsColorString( &text[ti] ) ) {
									clipped[ci++] = text[ti++];
									if ( text[ti] ) clipped[ci++] = text[ti];
									continue;
								}
								if ( vc >= maxChars ) break;
								clipped[ci++] = text[ti];
								vc++;
							}
							clipped[ci] = '\0';
							SCR_DrawStringExt( (int)colX, (int)( rowY + 2 ), charSize,
								clipped, rowColor, qfalse, qfalse );
						} else {
							SCR_DrawStringExt( (int)colX, (int)( rowY + 2 ), charSize,
								text, rowColor, qfalse, qfalse );
						}
					}
					colX += colW;
				}
			}

			// listbox scrollbar — macOS-style with fade
			if ( totalRows > visibleRows ) {
				float trackX = itemX + item->rect.w - scrollBarW - 1.0f;
				float trackY = itemY + 1.0f;
				float trackH = item->rect.h - 2.0f;
				float visibleFrac = (float)visibleRows / (float)totalRows;
				float thumbH = trackH * visibleFrac;
				float maxScroll = (float)( totalRows - visibleRows );
				float thumbY;
				float alpha = 0.0f;

				if ( thumbH < 16.0f ) thumbH = 16.0f;
				thumbY = trackY;
				if ( maxScroll > 0 ) {
					thumbY += ( trackH - thumbH ) * ( (float)item->listScrollOffset / maxScroll );
				}

				// fade out after 1.5s of inactivity (same as menu scrollbar)
				if ( item->listScrollFadeTime > 0 ) {
					int elapsed = realtime - item->listScrollFadeTime;
					if ( elapsed < 1500 ) {
						alpha = 1.0f;
					} else {
						alpha = 1.0f - (float)( elapsed - 1500 ) / 500.0f;
						if ( alpha < 0 ) alpha = 0;
					}
				}

				if ( alpha > 0 ) {
					vec4_t trackColor = { 0.3f, 0.3f, 0.3f, 0.15f * alpha };
					vec4_t thumbColor = { 0.7f, 0.7f, 0.7f, 0.5f * alpha };
					SCR_FillRect( trackX, trackY, scrollBarW, trackH, trackColor );
					SCR_FillRect( trackX, thumbY, scrollBarW, thumbH, thumbColor );
				}
			}

			continue; // skip normal text rendering for listbox
		}

		// ── text truncation helper ──────────────────────────────────
		// If text is wider than the item rect, truncate with ".." suffix.
		// buf is reused below for cvar-bound items too.
		#define WIRED_TRUNCATE_TEXT(src, charSz, maxW, outBuf, outBufSz) do { \
			if ( (maxW) > 0 && strlen(src) * (charSz) > (maxW) ) { \
				int maxChars = (int)((maxW) / (charSz)); \
				if ( maxChars > 2 ) { \
					Q_strncpyz( (outBuf), (src), MIN( maxChars - 1, (outBufSz) - 1 ) ); \
					Q_strcat( (outBuf), (outBufSz), ".." ); \
				} else { \
					Q_strncpyz( (outBuf), (src), MIN( maxChars + 1, (outBufSz) ) ); \
				} \
			} else { \
				Q_strncpyz( (outBuf), (src), (outBufSz) ); \
			} \
		} while(0)

		// draw cvar-bound item value (right side of label)
		if ( item->cvar[0] && item->type != ITEM_TYPE_TEXT && item->type != ITEM_TYPE_BUTTON ) {
			char cvarBuf[256];
			const char *valueText = "";
			float charSize = item->textscale >= 0.7f ? 16.0f : 8.0f;
			// vertical center text in rect when textaligny is not set
			float textVCenter = ( item->textaligny == 0 && item->rect.h > charSize )
				? ( item->rect.h - charSize ) * 0.5f : item->textaligny;
			float labelX = itemX + item->textalignx;
			float labelY = itemY + textVCenter;
			float valueX;

			// draw the label on the left
			if ( item->text[0] ) {
				SCR_DrawStringExt( (int)labelX, (int)labelY, charSize, item->text,
					item->forecolor, qfalse, qfalse );
			}

			// compute value text based on item type
			Cvar_VariableStringBuffer( item->cvar, cvarBuf, sizeof( cvarBuf ) );

			switch ( item->type ) {
				case ITEM_TYPE_YESNO:
					valueText = atof( cvarBuf ) != 0 ? "Yes" : "No";
					break;

				case ITEM_TYPE_MULTI:
					if ( item->multiData ) {
						int j;
						qboolean found = qfalse;
						for ( j = 0; j < item->multiData->count; j++ ) {
							if ( item->multiData->isStringList ) {
								if ( !Q_stricmp( cvarBuf, item->multiData->strValues[j] ) ) {
									valueText = item->multiData->labels[j];
									found = qtrue;
									break;
								}
							} else {
								if ( item->multiData->floatValues[j] == atof( cvarBuf ) ) {
									valueText = item->multiData->labels[j];
									found = qtrue;
									break;
								}
							}
						}
						// fallback: show raw cvar value if no option matches
						if ( !found && cvarBuf[0] ) {
							valueText = cvarBuf;
						}
					}
					break;

				case ITEM_TYPE_SLIDER:
					{
						float val = atof( cvarBuf );
						float range = item->sliderData.maxVal - item->sliderData.minVal;
						float frac = ( range > 0 ) ? ( val - item->sliderData.minVal ) / range : 0;
						float barX = itemX + item->rect.w * 0.5f;
						float barW = item->rect.w * 0.45f;
						float barY = itemY + item->rect.h * 0.4f;
						float barH = 4.0f;
						vec4_t barBg = { 0.3f, 0.3f, 0.3f, 0.6f };
						vec4_t barFg = { 1.0f, 0.75f, 0.0f, 1.0f };

						if ( frac < 0 ) frac = 0;
						if ( frac > 1 ) frac = 1;

						// draw slider track
						SCR_FillRect( barX, barY, barW, barH, barBg );
						// draw slider fill
						SCR_FillRect( barX, barY, barW * frac, barH, barFg );
						// draw value as text
						valueText = va( "%.1f", val );
					}
					break;

				case ITEM_TYPE_BIND:
					{
						static char bindBuf[128];
						if ( wired_waitingForKey && wired_bindItem == item ) {
							valueText = "Press a key...";
						} else {
							// find primary + alternate keys bound to this command
							const char *key1 = NULL, *key2 = NULL;
							int k;
							for ( k = 0; k < MAX_KEYS; k++ ) {
								const char *b = Key_GetBinding( k );
								if ( b && !Q_stricmp( b, item->cvar ) ) {
									if ( !key1 ) key1 = Key_KeynumToString( k );
									else if ( !key2 ) { key2 = Key_KeynumToString( k ); break; }
								}
							}
							if ( key1 && key2 ) {
								Com_sprintf( bindBuf, sizeof( bindBuf ), "%s ^7or %s", key1, key2 );
								valueText = bindBuf;
							} else if ( key1 ) {
								valueText = key1;
							} else {
								valueText = "---";
							}
						}
					}
					break;

				case ITEM_TYPE_EDITFIELD:
				case ITEM_TYPE_NUMERICFIELD:
					if ( wired_editingField && wired_editItem == item ) {
						// show value with blinking cursor
						static char editBuf[512];
						int curPos = wired_editCursorPos;
						qboolean showCursor = ( (int)( cls.realtime / 250 ) & 1 );

						if ( curPos > (int)strlen( cvarBuf ) ) curPos = strlen( cvarBuf );
						Q_strncpyz( editBuf, cvarBuf, curPos + 1 );
						if ( showCursor ) Q_strcat( editBuf, sizeof(editBuf), "_" );
						else Q_strcat( editBuf, sizeof(editBuf), " " );
						Q_strcat( editBuf, sizeof(editBuf), &cvarBuf[curPos] );
						valueText = editBuf;
					} else {
						valueText = cvarBuf;
					}
					break;

				default:
					valueText = cvarBuf;
					break;
			}

			// draw value text on the right side
			if ( valueText[0] ) {
				float textLen = strlen( item->text[0] ? item->text : "" ) * charSize;
				valueX = itemX + textLen + 16;
				if ( valueX < itemX + item->rect.w * 0.5f ) {
					valueX = itemX + item->rect.w * 0.5f;
				}
				SCR_DrawStringExt( (int)valueX, (int)labelY, charSize, valueText,
					item->forecolor, qfalse, qfalse );
			}
		}
		// draw text-only items (no cvar)
		else if ( item->text[0] ) {
			float charSize = item->textscale >= 0.7f ? 16.0f : 8.0f;
			float textVCenter = ( item->textaligny == 0 && item->rect.h > charSize )
				? ( item->rect.h - charSize ) * 0.5f : item->textaligny;
			float x = itemX + item->textalignx;
			float y = itemY + textVCenter;
			char truncBuf[256];
			const char *displayText;

			// truncate if wider than rect
			if ( item->rect.w > 0 ) {
				WIRED_TRUNCATE_TEXT( item->text, charSize, item->rect.w, truncBuf, sizeof( truncBuf ) );
				displayText = truncBuf;
			} else {
				displayText = item->text;
			}

			// adjust for text alignment within rect
			if ( item->textalign == ITEM_ALIGN_CENTER && item->rect.w > 0 ) {
				float textWidth = strlen( displayText ) * charSize;
				x = itemX + ( item->rect.w - textWidth ) * 0.5f;
			} else if ( item->textalign == ITEM_ALIGN_RIGHT && item->rect.w > 0 ) {
				float textWidth = strlen( displayText ) * charSize;
				x = itemX + item->rect.w - textWidth;
			}

			SCR_DrawStringExt( (int)x, (int)y, charSize, displayText,
				item->forecolor, qfalse, qfalse );
		}
	}

	// draw focus highlight on hovered/selected item
	if ( wired_focusItem >= 0 && wired_focusItem < menu->itemCount ) {
		wiredItemDef_t *focus = menu->items[wired_focusItem];
		if ( focus->visible && !focus->decoration ) {
			// skip focus on cvarTest-hidden items
			if ( focus->cvarTest[0] ) {
				char ftBuf[256];
				Cvar_VariableStringBuffer( focus->cvarTest, ftBuf, sizeof( ftBuf ) );
				if ( focus->showCvar[0] ) {
					int ftInt = atoi( ftBuf );
					char ftIntStr[16];
					Com_sprintf( ftIntStr, sizeof( ftIntStr ), "%d", ftInt );
					if ( !strstr( focus->showCvar, ftBuf ) && !strstr( focus->showCvar, ftIntStr ) )
						goto skip_focus;
				}
				if ( focus->hideCvar[0] ) {
					int ftInt = atoi( ftBuf );
					char ftIntStr[16];
					Com_sprintf( ftIntStr, sizeof( ftIntStr ), "%d", ftInt );
					if ( strstr( focus->hideCvar, ftBuf ) || strstr( focus->hideCvar, ftIntStr ) )
						goto skip_focus;
				}
			}
			float fx = menuX + focus->rect.x;
			float fy = menuY + focus->rect.y - scrollY;
			// don't draw focus highlight outside clip area
			if ( fy + focus->rect.h < clipTop || fy > clipBottom ) goto skip_focus;
			// TA-style: gradient bar behind focused item, or solid fill as fallback
			if ( wired_gradientBarShader ) {
				re.SetColor( menu->focuscolor );
				SCR_DrawPic( fx, fy, focus->rect.w, focus->rect.h, wired_gradientBarShader );
				re.SetColor( NULL );
			} else {
				SCR_FillRect( fx, fy, focus->rect.w, focus->rect.h, menu->focuscolor );
			}
			// redraw the focused item's text on top of highlight
			if ( focus->text[0] ) {
				float charSize = focus->textscale >= 0.7f ? 16.0f : 8.0f;
				float focusVCenter = ( focus->textaligny == 0 && focus->rect.h > charSize )
					? ( focus->rect.h - charSize ) * 0.5f : focus->textaligny;
				float x = fx + focus->textalignx;
				float y = fy + focusVCenter;
				if ( focus->textalign == ITEM_ALIGN_CENTER && focus->rect.w > 0 ) {
					float textWidth = strlen( focus->text ) * charSize;
					x = fx + ( focus->rect.w - textWidth ) * 0.5f;
				}
				SCR_DrawStringExt( (int)x, (int)y, charSize, focus->text,
					focus->forecolor, qfalse, qfalse );
			}
		}
	}
skip_focus:

	// draw tooltip for mouse-hovered item only (ET:Legacy + QL)
	// keyboard focus does NOT show tooltips — they anchor to the cursor
	if ( wired_focusFromMouse && wired_focusItem >= 0 && wired_focusItem < menu->itemCount ) {
		wiredItemDef_t *focus = menu->items[wired_focusItem];
		if ( focus->tooltip[0] ) {
			float tx = wired_cursorX + 16;
			float ty = wired_cursorY + 16;
			float tw = strlen( focus->tooltip ) * 8.0f + 8;
			float th = 16.0f;
			vec4_t tipBg = { 0.0f, 0.0f, 0.0f, 0.85f };
			vec4_t tipFg = { 1.0f, 1.0f, 1.0f, 1.0f };

			// keep tooltip on screen
			if ( tx + tw > SCREEN_WIDTH ) tx = SCREEN_WIDTH - tw;
			if ( ty + th > SCREEN_HEIGHT ) ty = wired_cursorY - th - 4;

			SCR_FillRect( tx, ty, tw, th, tipBg );
			SCR_DrawStringExt( (int)(tx + 4), (int)(ty + 4), 8.0f,
				focus->tooltip, tipFg, qfalse, qfalse );
		}
	}

	// draw cursor centered on the logical point
	if ( Key_GetCatcher() & KEYCATCH_UI ) {
		if ( wired_cursorShader ) {
			re.SetColor( NULL );
			SCR_DrawPic( wired_cursorX - 16, wired_cursorY - 16, 32, 32, wired_cursorShader );
		} else {
			// fallback: simple crosshair cursor if shader not found
			vec4_t cursorColor = { 1.0f, 1.0f, 1.0f, 1.0f };
			re.SetColor( cursorColor );
			SCR_FillRect( wired_cursorX - 1, wired_cursorY - 8, 2, 16, cursorColor );
			SCR_FillRect( wired_cursorX - 8, wired_cursorY - 1, 16, 2, cursorColor );
			re.SetColor( NULL );
		}
	}

	// macOS-style scrollbar — thin, semi-transparent, fades out
	{
		float maxScroll = menu->contentHeight - menuH;
		if ( maxScroll > 0 ) {
			float scrollBarWidth = 4.0f;
			float scrollBarPadding = 2.0f;
			float trackX = menuX + menuW - scrollBarWidth - scrollBarPadding;
			float trackY = menuY + scrollBarPadding;
			float trackH = menuH - scrollBarPadding * 2;

			// thumb size proportional to visible/content ratio
			float visibleFrac = menuH / menu->contentHeight;
			float thumbH = trackH * visibleFrac;
			if ( thumbH < 20.0f ) thumbH = 20.0f;
			float thumbY = trackY + ( trackH - thumbH ) * ( scrollY / maxScroll );

			// fade out after 1.5 seconds of inactivity
			float alpha = 1.0f;
			if ( menu->scrollBarFadeTime > 0 ) {
				int elapsed = realtime - menu->scrollBarFadeTime;
				if ( elapsed > 1500 ) {
					alpha = 1.0f - ( elapsed - 1500 ) / 500.0f;
					if ( alpha < 0 ) alpha = 0;
				}
			} else {
				alpha = 0; // never scrolled — don't show
			}

			if ( alpha > 0 ) {
				vec4_t trackColor = { 0.3f, 0.3f, 0.3f, 0.15f * alpha };
				vec4_t thumbColor = { 0.7f, 0.7f, 0.7f, 0.5f * alpha };

				SCR_FillRect( trackX, trackY, scrollBarWidth, trackH, trackColor );
				SCR_FillRect( trackX, thumbY, scrollBarWidth, thumbH, thumbColor );
			}
		}
	}

	// TODO Phase 2: full rendering with borders, models, etc.
}

// ── script command system ─────────────────────────────────────────────
//
// Based on q3now's ui_script.c command table pattern. Same handler
// signature (name + numArgs + args) but adapted for Wired UI structs.
//
// Key difference from v6: unknown commands pass to the engine console
// via Cbuf_ExecuteText instead of being silently dropped. This means
// any console command works as a script action without hardcoding.

#define WIRED_MAX_SCRIPT_ARGS  8

// forward declarations
static wiredItemDef_t *WiredUI_FindItemByName( wiredMenuDef_t *menu, const char *name );
static void WiredUI_ForEachItemByNameOrGroup( wiredMenuDef_t *menu, const char *name,
	void (*callback)( wiredItemDef_t *item, void *data ), void *data );
static void WiredUI_RunScript( wiredMenuDef_t *menu, wiredItemDef_t *item, const char *script );

typedef void (*wiredScriptHandler_t)( wiredMenuDef_t *menu, wiredItemDef_t *item,
                                       int numArgs, const char **args );

typedef struct {
	const char            *name;
	wiredScriptHandler_t   handler;
} wiredScriptCommand_t;

// ── script handlers ───────────────────────────────────────────────────

static void WiredScript_Show( wiredMenuDef_t *menu, wiredItemDef_t *item, int numArgs, const char **args ) {
	wiredItemDef_t *target;
	if ( numArgs < 1 ) return;
	target = WiredUI_FindItemByName( menu, args[0] );
	if ( target ) target->visible = qtrue;
}

static void WiredScript_Hide( wiredMenuDef_t *menu, wiredItemDef_t *item, int numArgs, const char **args ) {
	wiredItemDef_t *target;
	if ( numArgs < 1 ) return;
	target = WiredUI_FindItemByName( menu, args[0] );
	if ( target ) target->visible = qfalse;
}

static void WiredScript_Open( wiredMenuDef_t *menu, wiredItemDef_t *item, int numArgs, const char **args ) {
	if ( numArgs < 1 ) return;
	WiredUI_PushMenu( args[0] );
}

static void WiredScript_Close( wiredMenuDef_t *menu, wiredItemDef_t *item, int numArgs, const char **args ) {
	WiredUI_PopMenu();
}

static void WiredScript_SetCvar( wiredMenuDef_t *menu, wiredItemDef_t *item, int numArgs, const char **args ) {
	if ( numArgs >= 2 ) {
		Cvar_Set( args[0], args[1] );
	}
}

static void WiredScript_Exec( wiredMenuDef_t *menu, wiredItemDef_t *item, int numArgs, const char **args ) {
	if ( numArgs >= 1 ) {
		Cbuf_ExecuteText( EXEC_APPEND, va( "%s\n", args[0] ) );
	}
}

static void WiredScript_Play( wiredMenuDef_t *menu, wiredItemDef_t *item, int numArgs, const char **args ) {
	if ( numArgs >= 1 ) {
		sfxHandle_t sfx = S_RegisterSound( args[0], qfalse );
		if ( sfx ) S_StartLocalSound( sfx, CHAN_LOCAL_SOUND );
	}
}

static void WiredScript_PlayLooped( wiredMenuDef_t *menu, wiredItemDef_t *item, int numArgs, const char **args ) {
	if ( numArgs >= 1 ) {
		S_StartBackgroundTrack( args[0], args[0] );
	}
}

static void WiredScript_StopMusic( wiredMenuDef_t *menu, wiredItemDef_t *item, int numArgs, const char **args ) {
	S_StopBackgroundTrack();
}

static void WiredScript_FadeIn( wiredMenuDef_t *menu, wiredItemDef_t *item, int numArgs, const char **args ) {
	wiredItemDef_t *target;
	if ( numArgs < 1 ) return;
	target = WiredUI_FindItemByName( menu, args[0] );
	if ( target ) {
		wiredAssetGlobals_t *ag = WiredUI_GetAssetGlobals();
		target->visible = qtrue;
		target->fadeAlphaItem = 0.0f;
		target->fadeTargetAlpha = 1.0f;
		target->fadeStartTime = cls.realtime;
		// derive duration from assetGlobalDef: (fadeClamp / fadeAmount) * fadeCycle ms
		if ( ag->fadeAmount > 0 && ag->fadeCycle > 0 ) {
			target->fadeDurationItem = (int)( ( ag->fadeClamp / ag->fadeAmount ) * ag->fadeCycle );
		} else {
			target->fadeDurationItem = 150;
		}
	}
}

static void WiredScript_FadeOut( wiredMenuDef_t *menu, wiredItemDef_t *item, int numArgs, const char **args ) {
	wiredItemDef_t *target;
	if ( numArgs < 1 ) return;
	target = WiredUI_FindItemByName( menu, args[0] );
	if ( target ) {
		wiredAssetGlobals_t *ag = WiredUI_GetAssetGlobals();
		target->fadeAlphaItem = 1.0f;
		target->fadeTargetAlpha = 0.0f;
		target->fadeStartTime = cls.realtime;
		if ( ag->fadeAmount > 0 && ag->fadeCycle > 0 ) {
			target->fadeDurationItem = (int)( ( ag->fadeClamp / ag->fadeAmount ) * ag->fadeCycle );
		} else {
			target->fadeDurationItem = 150;
		}
	}
}

static void WiredScript_SetFocus( wiredMenuDef_t *menu, wiredItemDef_t *item, int numArgs, const char **args ) {
	int i;
	if ( numArgs < 1 ) return;
	for ( i = 0; i < menu->itemCount; i++ ) {
		if ( !Q_stricmp( menu->items[i]->name, args[0] ) ) {
			wired_focusItem = i;
			break;
		}
	}
}

// forward declaration (non-static — also called from cl_wired_feeders.c)
void WiredUI_UpdateMapPoolButton( void );

// ── per-gametype cvar persistence ─────────────────────────────────────
// Saves/restores fraglimit, timelimit, capturelimit, friendlyfire when
// switching game types — same pattern as q3_ui's ServerOptions_Cache.

int wired_lastSavedGameType = -1;

void WiredUI_SaveGameTypeSettings( void ) {
	int gt = Cvar_VariableIntegerValue( "ui_netGameType" );
	const char *prefix;

	switch ( gt ) {
		case 0: prefix = "ui_ffa"; break;
		case 1: prefix = "ui_tourney"; break;
		case 3: prefix = "ui_koth"; break;
		case 4: prefix = "ui_lms"; break;
		case 5: prefix = "ui_team"; break;
		case 6: prefix = "ui_ctf"; break;
		default: return;
	}

	Cvar_Set( va( "%s_fraglimit", prefix ), Cvar_VariableString( "fraglimit" ) );
	Cvar_Set( va( "%s_timelimit", prefix ), Cvar_VariableString( "timelimit" ) );
	if ( gt == 6 ) Cvar_Set( "ui_ctf_capturelimit", Cvar_VariableString( "capturelimit" ) );
	if ( gt >= 5 ) Cvar_Set( va( "%s_friendly", prefix ), Cvar_VariableString( "g_friendlyfire" ) );

	// save map rotation per gametype
	{
		char saveBuf[1024];
		Cvar_VariableStringBuffer( "g_maprotation", saveBuf, sizeof( saveBuf ) );
		Cvar_Set( va( "%s_maprotation", prefix ), saveBuf );
	}

	wired_lastSavedGameType = gt;
}

void WiredUI_LoadGameTypeSettings( void ) {
	int gt = Cvar_VariableIntegerValue( "ui_netGameType" );
	const char *prefix;
	char buf[64];

	switch ( gt ) {
		case 0: prefix = "ui_ffa"; break;
		case 1: prefix = "ui_tourney"; break;
		case 3: prefix = "ui_koth"; break;
		case 4: prefix = "ui_lms"; break;
		case 5: prefix = "ui_team"; break;
		case 6: prefix = "ui_ctf"; break;
		default: return;
	}

	// restore saved values — use "0" default when cvar was never saved
	Cvar_VariableStringBuffer( va( "%s_fraglimit", prefix ), buf, sizeof( buf ) );
	Cvar_Set( "fraglimit", buf[0] ? buf : "0" );

	Cvar_VariableStringBuffer( va( "%s_timelimit", prefix ), buf, sizeof( buf ) );
	Cvar_Set( "timelimit", buf[0] ? buf : "0" );

	if ( gt == 6 ) {
		Cvar_VariableStringBuffer( "ui_ctf_capturelimit", buf, sizeof( buf ) );
		Cvar_Set( "capturelimit", buf[0] ? buf : "8" );
	}

	if ( gt >= 5 ) {
		Cvar_VariableStringBuffer( va( "%s_friendly", prefix ), buf, sizeof( buf ) );
		Cvar_Set( "g_friendlyfire", buf[0] ? buf : "0" );
	}

	// restore map rotation for this gametype (empty = no rotation)
	{
		char rotBuf[1024];
		Cvar_VariableStringBuffer( va( "%s_maprotation", prefix ), rotBuf, sizeof( rotBuf ) );
		Cvar_Set( "g_maprotation", rotBuf );
		Cvar_Set( "g_maprotationIndex", "0" );
	}
}

// Called from uiScript: uiScript UpdateGameType
static void WiredScript_UpdateGameType( wiredMenuDef_t *menu, wiredItemDef_t *item, int numArgs, const char **args ) {
	int gt = Cvar_VariableIntegerValue( "ui_netGameType" );

	if ( wired_lastSavedGameType == -1 ) {
		// first entry — save current defaults so they exist for future loads
		wired_lastSavedGameType = gt;
		WiredUI_SaveGameTypeSettings();
	} else if ( gt != wired_lastSavedGameType ) {
		// gametype changed — save old, load new
		WiredUI_SaveGameTypeSettings();
		wired_lastSavedGameType = gt;
		WiredUI_LoadGameTypeSettings();
	}

	WiredUI_UpdateMapPoolButton();
}

// ── map pool helpers ──────────────────────────────────────────────────

void WiredUI_UpdateMapPoolButton( void ) {
	char mapName[MAX_QPATH];
	char rotation[1024];
	char token[MAX_QPATH];
	const char *p;
	qboolean inPool = qfalse;
	int count = 0;

	Cvar_VariableStringBuffer( "ui_selectedMap", mapName, sizeof( mapName ) );
	Cvar_VariableStringBuffer( "g_maprotation", rotation, sizeof( rotation ) );

	p = rotation;
	while ( *p ) {
		int ti = 0;
		while ( *p == ' ' ) p++;
		if ( !*p ) break;
		while ( *p && *p != ' ' && ti < (int)sizeof(token) - 1 ) token[ti++] = *p++;
		token[ti] = '\0';
		count++;
		if ( mapName[0] && !Q_stricmp( token, mapName ) ) inPool = qtrue;
	}

	Cvar_Set( "ui_mapPoolAction", inPool ? "Remove from Pool" : "Add to Pool" );
	Cvar_Set( "ui_mapPoolStatus", count > 0
		? va( "^2%d maps in pool", count )
		: "Single map (no rotation)" );
}

// ── map rotation (map pool) ──────────────────────────────────────────

static void WiredScript_ToggleMapPool( wiredMenuDef_t *menu, wiredItemDef_t *item, int numArgs, const char **args ) {
	char mapName[MAX_QPATH];
	char rotation[1024];
	char newRotation[1024];
	char token[MAX_QPATH];
	const char *p;
	qboolean found = qfalse;
	int len;

	Cvar_VariableStringBuffer( "ui_selectedMap", mapName, sizeof( mapName ) );
	if ( !mapName[0] ) {
		Com_Printf( "WiredUI: ToggleMapPool — no map selected\n" );
		return;
	}

	Com_Printf( "WiredUI: ToggleMapPool '%s'\n", mapName );
	Cvar_VariableStringBuffer( "g_maprotation", rotation, sizeof( rotation ) );

	// rebuild rotation without the selected map (or add it if not found)
	newRotation[0] = '\0';
	len = 0;
	p = rotation;
	while ( *p ) {
		int i = 0;
		while ( *p == ' ' ) p++;
		if ( !*p ) break;
		while ( *p && *p != ' ' && i < (int)sizeof(token) - 1 ) token[i++] = *p++;
		token[i] = '\0';
		if ( !Q_stricmp( token, mapName ) ) {
			found = qtrue;
			continue;  // remove
		}
		if ( len > 0 ) { newRotation[len++] = ' '; newRotation[len] = '\0'; }
		Q_strncpyz( newRotation + len, token, sizeof(newRotation) - len );
		len = strlen( newRotation );
	}

	if ( !found ) {
		if ( len > 0 ) { newRotation[len++] = ' '; newRotation[len] = '\0'; }
		Q_strncpyz( newRotation + len, mapName, sizeof(newRotation) - len );
	}

	Cvar_Set( "g_maprotation", newRotation );
	Cvar_Set( "g_maprotationIndex", "0" );

	WiredUI_UpdateMapPoolButton();
}

static void WiredScript_ClearMapPool( wiredMenuDef_t *menu, wiredItemDef_t *item, int numArgs, const char **args ) {
	Cvar_Set( "g_maprotation", "" );
	Cvar_Set( "g_maprotationIndex", "0" );
	WiredUI_UpdateMapPoolButton();
}

// ── favorite maps ────────────────────────────────────────────────────
// Stored in ui_favoriteMaps cvar (CVAR_ARCHIVE — persists across sessions)

static void WiredScript_ToggleFavoriteMap( wiredMenuDef_t *menu, wiredItemDef_t *item, int numArgs, const char **args ) {
	char mapName[MAX_QPATH];
	char favs[2048];
	char newFavs[2048];
	char token[MAX_QPATH];
	const char *p;
	qboolean found = qfalse;
	int len;

	Cvar_VariableStringBuffer( "ui_selectedMap", mapName, sizeof( mapName ) );
	if ( !mapName[0] ) return;

	Cvar_VariableStringBuffer( "ui_favoriteMaps", favs, sizeof( favs ) );

	// rebuild without the map, or add it
	newFavs[0] = '\0';
	len = 0;
	p = favs;
	while ( *p ) {
		int i = 0;
		while ( *p == ' ' ) p++;
		if ( !*p ) break;
		while ( *p && *p != ' ' && i < (int)sizeof(token) - 1 ) token[i++] = *p++;
		token[i] = '\0';
		if ( !Q_stricmp( token, mapName ) ) {
			found = qtrue;
			continue;
		}
		if ( len > 0 ) { newFavs[len++] = ' '; newFavs[len] = '\0'; }
		Q_strncpyz( newFavs + len, token, sizeof(newFavs) - len );
		len = strlen( newFavs );
	}

	if ( !found ) {
		if ( len > 0 ) { newFavs[len++] = ' '; newFavs[len] = '\0'; }
		Q_strncpyz( newFavs + len, mapName, sizeof(newFavs) - len );
	}

	Cvar_Set( "ui_favoriteMaps", newFavs );

	// update button label
	Cvar_Set( "ui_favMapAction", found ? "Favorite" : "Unfavorite" );
}

// update favorite button based on selected map
void WiredUI_UpdateFavoriteButton( void ) {
	char mapName[MAX_QPATH];
	char favs[2048];

	Cvar_VariableStringBuffer( "ui_selectedMap", mapName, sizeof( mapName ) );
	Cvar_VariableStringBuffer( "ui_favoriteMaps", favs, sizeof( favs ) );

	if ( mapName[0] ) {
		extern qboolean WiredFeeder_IsMapInList( const char *list, const char *mapName );
		Cvar_Set( "ui_favMapAction",
			WiredFeeder_IsMapInList( favs, mapName ) ? "Unfavorite" : "Favorite" );
	} else {
		Cvar_Set( "ui_favMapAction", "Favorite" );
	}
}

// ── game action handlers ──────────────────────────────────────────────
// These read cvars set by feeder selection callbacks and execute real game actions.

static void WiredScript_StartServer( wiredMenuDef_t *menu, wiredItemDef_t *item, int numArgs, const char **args ) {
	char mapName[MAX_QPATH];
	int botCount, botSkill, i;
	static const char *botNames[] = {
		"Sarge", "Keel", "Bones", "Slash", "Doom", "Anarki", "Major", "Klesk"
	};

	// use first map from rotation pool if set, otherwise selected map
	{
		char rotation[1024];
		Cvar_VariableStringBuffer( "g_maprotation", rotation, sizeof( rotation ) );
		if ( rotation[0] ) {
			// extract first map from rotation for initial launch
			int k = 0;
			const char *r = rotation;
			while ( *r == ' ' ) r++;
			while ( *r && *r != ' ' && k < (int)sizeof(mapName) - 1 ) mapName[k++] = *r++;
			mapName[k] = '\0';
			Cvar_Set( "g_maprotationIndex", "0" );
		} else {
			Cvar_VariableStringBuffer( "ui_selectedMap", mapName, sizeof( mapName ) );
		}
	}
	if ( !mapName[0] ) {
		Com_Printf( S_COLOR_YELLOW "WiredUI: no map selected\n" );
		return;
	}

	// save per-gametype settings before launch
	WiredUI_SaveGameTypeSettings();

	// apply server cvars from menu selections
	// cvar-bound items (fraglimit, timelimit, capturelimit, sv_maxclients,
	// sv_pure, sv_allowDownload, g_doWarmup, g_friendlyfire,
	// g_teamForceBalance, g_allowvote) are set directly by menu items
	Cvar_Set( "g_gametype", Cvar_VariableString( "ui_netGameType" ) );

	// dedicated mode
	if ( Cvar_VariableIntegerValue( "ui_dedicated" ) <= 0 ) {
		Cvar_Set( "dedicated", "0" );
	} else {
		Cvar_Set( "dedicated", Cvar_VariableString( "ui_dedicated" ) );
	}

	botCount = Cvar_VariableIntegerValue( "ui_botCount" );
	botSkill = Cvar_VariableIntegerValue( "g_spSkill" );
	if ( botSkill < 1 ) botSkill = 3;
	if ( botSkill > 5 ) botSkill = 5;

	WiredUI_CloseAllMenus();

	// launch map (wait allows dedicated cvar to take effect)
	Cbuf_ExecuteText( EXEC_APPEND, va( "wait ; wait ; map %s\n", mapName ) );

	// add bots after map load
	for ( i = 0; i < botCount && i < 8; i++ ) {
		Cbuf_ExecuteText( EXEC_APPEND,
			va( "wait 3 ; addbot %s %d\n", botNames[i], botSkill ) );
	}

	// team preference for human player in team modes
	{
		int gt = Cvar_VariableIntegerValue( "ui_netGameType" );
		int teamPref = Cvar_VariableIntegerValue( "g_localTeamPref" );
		if ( gt >= 5 && teamPref > 0 ) {
			const char *team = ( teamPref == 1 ) ? "red" : "blue";
			Cbuf_ExecuteText( EXEC_APPEND, va( "wait 5 ; team %s\n", team ) );
		}
	}
}

static void WiredScript_JoinServer( wiredMenuDef_t *menu, wiredItemDef_t *item, int numArgs, const char **args ) {
	char addr[256];
	char password[256];

	Cvar_VariableStringBuffer( "ui_selectedServerAddr", addr, sizeof( addr ) );
	if ( !addr[0] ) {
		Com_Printf( S_COLOR_YELLOW "WiredUI: no server selected\n" );
		return;
	}

	// check if server requires password and none is set
	{
		// look up the server by address to check g_needpass
		int uiSource = Cvar_VariableIntegerValue( "ui_netSource" );
		serverInfo_t *servers = ( uiSource == 0 ) ? cls.localServers :
		                        ( uiSource == 6 ) ? cls.favoriteServers : cls.globalServers;
		int count = ( uiSource == 0 ) ? cls.numlocalservers :
		            ( uiSource == 6 ) ? cls.numfavoriteservers : cls.numglobalservers;
		int j;
		for ( j = 0; j < count; j++ ) {
			if ( !Q_stricmp( NET_AdrToStringwPort( &servers[j].adr ), addr ) ) {
				if ( servers[j].g_needpass ) {
					Cvar_VariableStringBuffer( "password", password, sizeof( password ) );
					if ( !password[0] ) {
						WiredUI_PushMenu( "password" );
						return;
					}
				}
				break;
			}
		}
	}

	WiredUI_CloseAllMenus();
	Cbuf_ExecuteText( EXEC_APPEND, va( "connect %s\n", addr ) );
}

static void WiredScript_RunDemo( wiredMenuDef_t *menu, wiredItemDef_t *item, int numArgs, const char **args ) {
	char demoName[MAX_QPATH];

	Cvar_VariableStringBuffer( "ui_selectedDemo", demoName, sizeof( demoName ) );
	if ( !demoName[0] ) {
		Com_Printf( S_COLOR_YELLOW "WiredUI: no demo selected\n" );
		return;
	}

	WiredUI_CloseAllMenus();
	Cbuf_ExecuteText( EXEC_APPEND, va( "demo %s\n", demoName ) );
}

// ── updateMapPreview ─────────────────────────────────────────────────
// Updates a named item's background with the levelshot of the selected map.
// Usage from .menu: action { updateMapPreview "mappreview" }
static void WiredScript_UpdateMapPreview( wiredMenuDef_t *menu, wiredItemDef_t *item, int numArgs, const char **args ) {
	char mapName[MAX_QPATH];
	const char *targetName = ( numArgs >= 1 ) ? args[0] : "mappreview";
	wiredItemDef_t *target;

	Cvar_VariableStringBuffer( "ui_selectedMap", mapName, sizeof( mapName ) );
	target = WiredUI_FindItemByName( menu, targetName );

	if ( target && mapName[0] ) {
		Com_sprintf( target->background, sizeof( target->background ), "levelshots/%s", mapName );
	} else if ( target ) {
		target->background[0] = '\0';
	}
}

static void WiredScript_RunMod( wiredMenuDef_t *menu, wiredItemDef_t *item, int numArgs, const char **args ) {
	char modName[MAX_QPATH];

	Cvar_VariableStringBuffer( "ui_selectedMod", modName, sizeof( modName ) );
	if ( !modName[0] ) {
		Com_Printf( S_COLOR_YELLOW "WiredUI: no mod selected\n" );
		return;
	}

	WiredUI_CloseAllMenus();

	// "baseq3" means return to base game — clear fs_game
	if ( Q_stricmp( modName, "baseq3" ) == 0 ) {
		Cvar_Set( "fs_game", "" );
	} else {
		Cvar_Set( "fs_game", modName );
	}
	Cbuf_ExecuteText( EXEC_APPEND, "vid_restart\n" );
}

static void WiredScript_RefreshServers( wiredMenuDef_t *menu, wiredItemDef_t *item, int numArgs, const char **args ) {
	int source = Cvar_VariableIntegerValue( "ui_netSource" );

	// always query local servers too — they appear instantly
	Cbuf_ExecuteText( EXEC_APPEND, "localservers\n" );

	if ( source > 0 && source < 6 ) {
		// query internet master servers
		Cbuf_ExecuteText( EXEC_APPEND, va( "globalservers %d %d\n", source - 1, PROTOCOL_VERSION ) );
	}

}

static void WiredScript_RefreshFilter( wiredMenuDef_t *menu, wiredItemDef_t *item, int numArgs, const char **args ) {
	extern void WiredFeeder_RebuildServerDisplayList( void );
	WiredFeeder_RebuildServerDisplayList();
}

// ── conditionalScript ─────────────────────────────────────────────────
// ET:Legacy syntax: conditionalScript cvarname mode ( "action_true" ) ( "action_false" )
// mode: 0 = if cvar == 0 run first, else second
//       2/3 = cvar test mode (same logic, historical)
static void WiredScript_ConditionalScript( wiredMenuDef_t *menu, wiredItemDef_t *item, int numArgs, const char **args ) {
	const char *trueAction = NULL;
	const char *falseAction = NULL;
	int cvarVal;
	int i;

	if ( numArgs < 4 ) return;  // cvarname mode ( "action" ) ...

	cvarVal = Cvar_VariableIntegerValue( args[0] );

	// find the two ( "action" ) blocks in the args
	for ( i = 2; i < numArgs; i++ ) {
		if ( !Q_stricmp( args[i], "(" ) && i + 1 < numArgs ) {
			if ( !trueAction ) {
				trueAction = args[i + 1];
			} else if ( !falseAction ) {
				falseAction = args[i + 1];
			}
		}
	}

	if ( !trueAction ) return;

	// mode 0: cvar == 0 → trueAction, else falseAction
	// mode 2/3: same logic (cvar is tested as boolean)
	if ( cvarVal == 0 ) {
		WiredUI_RunScript( menu, item, trueAction );
	} else if ( falseAction ) {
		WiredUI_RunScript( menu, item, falseAction );
	}
}

// ── setitemcolor ──────────────────────────────────────────────────────
// Syntax: setitemcolor "nameOrGroup" forecolor|backcolor R G B A
// Used everywhere for hover effects in Q3:TA/QL/OA/ET:L

typedef struct {
	const char *property;
	float      color[4];
} setItemColorData_t;

static void WiredScript_SetItemColor_Callback( wiredItemDef_t *item, void *data ) {
	setItemColorData_t *d = (setItemColorData_t *)data;
	if ( !Q_stricmp( d->property, "forecolor" ) ) {
		Vector4Copy( d->color, item->forecolor );
	} else if ( !Q_stricmp( d->property, "backcolor" ) ) {
		Vector4Copy( d->color, item->backcolor );
	} else if ( !Q_stricmp( d->property, "bordercolor" ) ) {
		Vector4Copy( d->color, item->bordercolor );
	}
}

static void WiredScript_SetItemColor( wiredMenuDef_t *menu, wiredItemDef_t *item, int numArgs, const char **args ) {
	setItemColorData_t data;
	if ( numArgs < 6 ) return;  // name property R G B A
	data.property = args[1];
	data.color[0] = atof( args[2] );
	data.color[1] = atof( args[3] );
	data.color[2] = atof( args[4] );
	data.color[3] = atof( args[5] );
	WiredUI_ForEachItemByNameOrGroup( menu, args[0], WiredScript_SetItemColor_Callback, &data );
}

// setcolor — alias for setitemcolor (some files use one, some the other)
static void WiredScript_SetColor( wiredMenuDef_t *menu, wiredItemDef_t *item, int numArgs, const char **args ) {
	WiredScript_SetItemColor( menu, item, numArgs, args );
}

// ── conditionalopen ──────────────────────────────────────────────────
// Syntax: conditionalopen "cvar" "menuIfTrue" "menuIfFalse"
static void WiredScript_ConditionalOpen( wiredMenuDef_t *menu, wiredItemDef_t *item, int numArgs, const char **args ) {
	if ( numArgs < 3 ) return;
	if ( Cvar_VariableIntegerValue( args[0] ) != 0 ) {
		WiredUI_PushMenu( args[1] );
	} else {
		WiredUI_PushMenu( args[2] );
	}
}

// ── transition ───────────────────────────────────────────────────────
// Syntax: transition "name" x1 y1 w1 h1 x2 y2 w2 h2 steps duration
// Animated rect interpolation over time
static void WiredScript_Transition( wiredMenuDef_t *menu, wiredItemDef_t *item, int numArgs, const char **args ) {
	wiredItemDef_t *target;
	if ( numArgs < 9 ) return;  // name x1 y1 w1 h1 x2 y2 w2 h2 [steps] [duration]
	target = WiredUI_FindItemByName( menu, args[0] );
	if ( target ) {
		target->transFrom.x = atof( args[1] );
		target->transFrom.y = atof( args[2] );
		target->transFrom.w = atof( args[3] );
		target->transFrom.h = atof( args[4] );
		target->transTo.x   = atof( args[5] );
		target->transTo.y   = atof( args[6] );
		target->transTo.w   = atof( args[7] );
		target->transTo.h   = atof( args[8] );
		// steps (args[9]) and duration (args[10]) — v6 uses steps*frametime
		// we use duration in ms directly; if only steps given, estimate 16ms/step
		if ( numArgs >= 11 ) {
			target->transDuration = atoi( args[9] ) * atoi( args[10] );
		} else if ( numArgs >= 10 ) {
			target->transDuration = atoi( args[9] ) * 16;  // ~60fps
		} else {
			target->transDuration = 200;  // default 200ms
		}
		if ( target->transDuration < 1 ) target->transDuration = 1;
		target->transStartTime = cls.realtime;
		// set initial position
		target->rect = target->transFrom;
	}
}

// ── sort commands (direct, not via uiScript) ─────────────────────────

static void WiredScript_MapSortCmd( wiredMenuDef_t *menu, wiredItemDef_t *item, int numArgs, const char **args ) {
	extern void WiredFeeder_SortMaps( int column );
	int col = ( numArgs >= 1 ) ? atoi( args[0] ) : 0;
	Com_Printf( "WiredUI: MapSort column %d\n", col );
	WiredFeeder_SortMaps( col );
}

static void WiredScript_ServerSortCmd( wiredMenuDef_t *menu, wiredItemDef_t *item, int numArgs, const char **args ) {
	extern void WiredFeeder_SortServers( int column );
	int col = ( numArgs >= 1 ) ? atoi( args[0] ) : 0;
	WiredFeeder_SortServers( col );
}

// ── setbackground ────────────────────────────────────────────────────
// Syntax: setbackground "shader"
static void WiredScript_SetBackground( wiredMenuDef_t *menu, wiredItemDef_t *item, int numArgs, const char **args ) {
	wiredItemDef_t *target;
	if ( numArgs < 1 ) return;
	target = WiredUI_FindItemByName( menu, args[0] );
	if ( target ) {
		Q_strncpyz( target->background, args[0], sizeof( target->background ) );
	}
}

// ── uiScript ─────────────────────────────────────────────────────────
// Maps v6 uiScript command names to our existing handlers.
// Q3:TA/QL/OA/ET:L all use: action { uiScript StartServer } etc.

typedef struct {
	const char *name;
	void (*handler)( wiredMenuDef_t *, wiredItemDef_t *, int, const char ** );
} wiredUiScriptEntry_t;

static const wiredUiScriptEntry_t wiredUiScripts[] = {
	{ "StartServer",      WiredScript_StartServer },
	{ "startserver",      WiredScript_StartServer },
	{ "JoinServer",       WiredScript_JoinServer },
	{ "joinserver",       WiredScript_JoinServer },
	{ "RunDemo",          WiredScript_RunDemo },
	{ "rundemo",          WiredScript_RunDemo },
	{ "RunMod",           WiredScript_RunMod },
	{ "runmod",           WiredScript_RunMod },
	{ "LoadDemos",        NULL },  // feeders auto-load, noop
	{ "LoadMods",         NULL },
	{ "LoadMovies",       NULL },
	{ "RefreshServers",   WiredScript_RefreshServers },
	{ "RefreshFilter",    WiredScript_RefreshFilter },
	{ "StopRefresh",      NULL },  // noop — server queries are fire-and-forget
	{ "closeJoin",        NULL },
	{ "closeingame",      NULL },
	{ NULL, NULL }
};

static void WiredScript_UiScript( wiredMenuDef_t *menu, wiredItemDef_t *item, int numArgs, const char **args ) {
	int i;
	if ( numArgs < 1 ) return;

	for ( i = 0; wiredUiScripts[i].name; i++ ) {
		if ( !Q_stricmp( args[0], wiredUiScripts[i].name ) ) {
			if ( wiredUiScripts[i].handler ) {
				// pass remaining args (skip the uiScript command name)
				wiredUiScripts[i].handler( menu, item, numArgs - 1, numArgs > 1 ? &args[1] : NULL );
			}
			return;
		}
	}

	// common actions that map directly to console commands
	if ( !Q_stricmp( args[0], "Quit" ) || !Q_stricmp( args[0], "quit" ) ) {
		Cbuf_ExecuteText( EXEC_APPEND, "quit\n" );
	} else if ( !Q_stricmp( args[0], "Leave" ) || !Q_stricmp( args[0], "leave" ) ) {
		Cbuf_ExecuteText( EXEC_APPEND, "disconnect\n" );
	} else if ( !Q_stricmp( args[0], "resetDefaults" ) ) {
		Cbuf_ExecuteText( EXEC_APPEND, "exec default.cfg\n" );
	} else if ( !Q_stricmp( args[0], "Controls" ) ) {
		WiredUI_PushMenu( "controls" );
	} else if ( !Q_stricmp( args[0], "clearError" ) ) {
		// noop
	} else if ( !Q_stricmp( args[0], "ServerSort" ) && numArgs >= 2 ) {
		extern void WiredFeeder_SortServers( int column );
		WiredFeeder_SortServers( atoi( args[1] ) );
	} else if ( !Q_stricmp( args[0], "MapSort" ) && numArgs >= 2 ) {
		extern void WiredFeeder_SortMaps( int column );
		Com_Printf( "WiredUI: MapSort column %s\n", args[1] );
		WiredFeeder_SortMaps( atoi( args[1] ) );
	} else if ( !Q_stricmp( args[0], "addFavorite" ) ) {
		Cbuf_ExecuteText( EXEC_APPEND, "addFavorite\n" );
	} else if ( !Q_stricmp( args[0], "deleteFavorite" ) ) {
		Cbuf_ExecuteText( EXEC_APPEND, "deleteFavorite\n" );
	} else {
		Com_Printf( S_COLOR_YELLOW "WiredUI: unknown uiScript '%s'\n", args[0] );
	}
}

// command table — matches q3now's ui_script.c set, plus Wired UI additions + v6 compat
static const wiredScriptCommand_t wiredScriptCommands[] = {
	{ "show",             WiredScript_Show },
	{ "hide",             WiredScript_Hide },
	{ "open",             WiredScript_Open },
	{ "close",            WiredScript_Close },
	{ "setcvar",          WiredScript_SetCvar },
	{ "exec",             WiredScript_Exec },
	{ "play",             WiredScript_Play },
	{ "playlooped",       WiredScript_PlayLooped },
	{ "stopmusic",        WiredScript_StopMusic },
	{ "fadein",           WiredScript_FadeIn },
	{ "fadeout",          WiredScript_FadeOut },
	{ "setfocus",         WiredScript_SetFocus },
	// ── Phase 2.5: v6 compatibility commands ────────────────────────
	{ "setitemcolor",     WiredScript_SetItemColor },
	{ "setcolor",         WiredScript_SetColor },
	{ "conditionalopen",  WiredScript_ConditionalOpen },
	{ "conditionalScript", WiredScript_ConditionalScript },
	{ "conditionalscript", WiredScript_ConditionalScript },
	{ "transition",       WiredScript_Transition },
	{ "setbackground",    WiredScript_SetBackground },
	{ "updateMapPreview", WiredScript_UpdateMapPreview },
	{ "uiScript",         WiredScript_UiScript },
	// ── sort commands ──────────────────────────────────────────────
	{ "MapSort",          WiredScript_MapSortCmd },
	{ "mapsort",          WiredScript_MapSortCmd },
	{ "UpdateGameType",   WiredScript_UpdateGameType },
	{ "updategametype",   WiredScript_UpdateGameType },
	{ "ToggleMapPool",    WiredScript_ToggleMapPool },
	{ "togglemappool",    WiredScript_ToggleMapPool },
	{ "ClearMapPool",     WiredScript_ClearMapPool },
	{ "clearmappool",     WiredScript_ClearMapPool },
	{ "ToggleFavorite",   WiredScript_ToggleFavoriteMap },
	{ "togglefavorite",   WiredScript_ToggleFavoriteMap },
	{ "ServerSort",       WiredScript_ServerSortCmd },
	{ "serversort",       WiredScript_ServerSortCmd },
	// ── game action commands (also reachable via uiScript) ──────────
	{ "startserver",      WiredScript_StartServer },
	{ "joinserver",       WiredScript_JoinServer },
	{ "rundemo",          WiredScript_RunDemo },
	{ "runmod",           WiredScript_RunMod },
	{ "refreshservers",   WiredScript_RefreshServers },

	{ NULL, NULL }
};

// ── script runner ─────────────────────────────────────────────────────
// Based on q3now's UI_RunScript (ui_script.c:282). Same tokenization:
// semicolon-separated commands, quoted string args, up to 8 args each.
// KEY DIFFERENCE: unknown commands pass to engine console instead of
// being silently dropped.

static void WiredUI_RunScript( wiredMenuDef_t *menu, wiredItemDef_t *item, const char *script ) {
	char        token[MAX_STRING_CHARS];
	const char *args[WIRED_MAX_SCRIPT_ARGS];
	static char argBuf[WIRED_MAX_SCRIPT_ARGS][256];
	int         numArgs, i;
	const char *p;
	qboolean    handled;

	if ( !script || !script[0] ) return;

	p = script;

	while ( *p ) {
		// skip whitespace and semicolons
		while ( *p == ' ' || *p == '\t' || *p == ';' ) p++;
		if ( !*p ) break;

		// read command name
		i = 0;
		while ( *p && *p != ' ' && *p != '\t' && *p != ';' && i < (int)sizeof(token) - 1 ) {
			token[i++] = *p++;
		}
		token[i] = '\0';
		if ( !token[0] ) break;

		// read arguments (up to 8, stop at semicolon)
		numArgs = 0;
		while ( numArgs < WIRED_MAX_SCRIPT_ARGS ) {
			while ( *p == ' ' || *p == '\t' ) p++;
			if ( !*p || *p == ';' ) break;

			i = 0;
			if ( *p == '"' ) {
				p++;
				while ( *p && *p != '"' && i < 255 ) argBuf[numArgs][i++] = *p++;
				if ( *p == '"' ) p++;
			} else {
				while ( *p && *p != ' ' && *p != '\t' && *p != ';' && i < 255 ) argBuf[numArgs][i++] = *p++;
			}
			argBuf[numArgs][i] = '\0';
			args[numArgs] = argBuf[numArgs];
			numArgs++;
		}

		// dispatch to command table
		handled = qfalse;
		for ( i = 0; wiredScriptCommands[i].name; i++ ) {
			if ( !Q_stricmp( token, wiredScriptCommands[i].name ) ) {
				wiredScriptCommands[i].handler( menu, item, numArgs, args );
				handled = qtrue;
				break;
			}
		}

		// Wired UI design decision: unknown commands pass to engine console.
		// This means any cvar or console command works as a script action
		// without needing to be hardcoded in the command table.
		if ( !handled ) {
			char cmdBuf[1024];
			Com_sprintf( cmdBuf, sizeof(cmdBuf), "%s", token );
			for ( i = 0; i < numArgs; i++ ) {
				Q_strcat( cmdBuf, sizeof(cmdBuf), va( " \"%s\"", args[i] ) );
			}
			Q_strcat( cmdBuf, sizeof(cmdBuf), "\n" );
			Cbuf_ExecuteText( EXEC_APPEND, cmdBuf );
		}
	}
}

// ── menu stack ────────────────────────────────────────────────────────

void WiredUI_PushMenu( const char *name ) {
	if ( !name || !name[0] ) return;

	// check if menu exists
	if ( !WiredUI_FindMenu( name ) ) {
		Com_Printf( S_COLOR_YELLOW "WiredUI: cannot open menu '%s' — not found (loaded %d menus)\n", name, WiredUI_GetMenuCount() );
		return;
	}

	if ( wired_menuStackDepth >= WIRED_MENU_STACK_DEPTH ) {
		Com_Printf( S_COLOR_YELLOW "WiredUI: menu stack overflow (max %d)\n", WIRED_MENU_STACK_DEPTH );
		return;
	}

	Q_strncpyz( wired_menuStack[wired_menuStackDepth], name, sizeof( wired_menuStack[0] ) );
	wired_menuStackDepth++;
	wired_focusItem = -1;
	wired_focusFromMouse = qfalse;
	if ( wired_sfxMenuOpen ) S_StartLocalSound( wired_sfxMenuOpen, CHAN_LOCAL_SOUND );

	// reset fade animation for the new menu
	{
		wiredMenuDef_t *pushed = WiredUI_FindMenu( name );
		if ( pushed ) {
			pushed->openTime = cls.realtime;
			pushed->fadeAlpha = 0;

			// start cinematic if menu uses WINDOW_STYLE_CINEMATIC
			if ( pushed->style == WINDOW_STYLE_CINEMATIC && pushed->cinematic[0] ) {
				pushed->cinematicHandle = CIN_PlayCinematic( pushed->cinematic,
					0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, CIN_loop | CIN_silent );
				if ( pushed->cinematicHandle >= 0 ) {
					Com_DPrintf( "WiredUI: started cinematic '%s' (handle %d)\n",
						pushed->cinematic, pushed->cinematicHandle );
				}
			}
		}
	}

	Com_DPrintf( "WiredUI: push menu '%s' (depth %d)\n", name, wired_menuStackDepth );

	// run onOpen script if present
	{
		wiredMenuDef_t *opened = WiredUI_FindMenu( name );
		if ( opened && opened->onOpen[0] ) {
			WiredUI_RunScript( opened, NULL, opened->onOpen );
		}
	}
}

void WiredUI_PopMenu( void ) {
	if ( wired_menuStackDepth <= 0 ) {
		// nothing to pop — close UI entirely
		wired_activeMenu = UIMENU_NONE;
		Key_SetCatcher( Key_GetCatcher() & ~KEYCATCH_UI );
		Cvar_Set( "cl_paused", "0" );
		return;
	}

	// stop cinematic on the menu being popped (before decrement)
	{
		wiredMenuDef_t *popped = WiredUI_FindMenu( wired_menuStack[wired_menuStackDepth - 1] );
		if ( popped && popped->cinematicHandle >= 0 ) {
			CIN_StopCinematic( popped->cinematicHandle );
			popped->cinematicHandle = -1;
		}
	}

	wired_menuStackDepth--;
	wired_focusItem = -1;
	wired_focusFromMouse = qfalse;
	if ( wired_sfxMenuClose ) S_StartLocalSound( wired_sfxMenuClose, CHAN_LOCAL_SOUND );

	if ( wired_menuStackDepth <= 0 ) {
		// stack empty — return to root menu behavior
		if ( wired_activeMenu == UIMENU_INGAME ) {
			// close in-game menu entirely
			wired_activeMenu = UIMENU_NONE;
			Key_SetCatcher( Key_GetCatcher() & ~KEYCATCH_UI );
			Cvar_Set( "cl_paused", "0" );
		}
		// for UIMENU_MAIN, the root stays visible (can't close the main menu)
	}

	Com_DPrintf( "WiredUI: pop menu (depth %d)\n", wired_menuStackDepth );
}

void WiredUI_CloseAllMenus( void ) {
	int i;
	// stop all active cinematics on the stack
	for ( i = 0; i < wired_menuStackDepth; i++ ) {
		wiredMenuDef_t *m = WiredUI_FindMenu( wired_menuStack[i] );
		if ( m && m->cinematicHandle >= 0 ) {
			CIN_StopCinematic( m->cinematicHandle );
			m->cinematicHandle = -1;
		}
	}
	wired_menuStackDepth = 0;
	wired_focusItem = -1;
	wired_activeMenu = UIMENU_NONE;
	Key_SetCatcher( Key_GetCatcher() & ~KEYCATCH_UI );
	Cvar_Set( "cl_paused", "0" );
}

// ── helper functions ──────────────────────────────────────────────────

wiredMenuDef_t *WiredUI_GetActiveMenu( void ) {
	// if there's a menu on the stack, show that
	if ( wired_menuStackDepth > 0 ) {
		return WiredUI_FindMenu( wired_menuStack[wired_menuStackDepth - 1] );
	}
	// otherwise show the root menu based on wired_activeMenu
	if ( wired_activeMenu == UIMENU_MAIN )   return WiredUI_FindMenu( "main" );
	if ( wired_activeMenu == UIMENU_INGAME ) return WiredUI_FindMenu( "ingame" );
	return NULL;
}

static wiredItemDef_t *WiredUI_FindItemByName( wiredMenuDef_t *menu, const char *name ) {
	int i;
	if ( !menu || !name ) return NULL;
	for ( i = 0; i < menu->itemCount; i++ ) {
		if ( !Q_stricmp( menu->items[i]->name, name ) ) {
			return menu->items[i];
		}
	}
	return NULL;
}

// apply a callback to all items matching name OR group
static void WiredUI_ForEachItemByNameOrGroup( wiredMenuDef_t *menu, const char *name,
	void (*callback)( wiredItemDef_t *item, void *data ), void *data ) {
	int i;
	if ( !menu || !name ) return;
	for ( i = 0; i < menu->itemCount; i++ ) {
		wiredItemDef_t *it = menu->items[i];
		if ( !Q_stricmp( it->name, name ) || !Q_stricmp( it->group, name ) ) {
			callback( it, data );
		}
	}
}

static qboolean WiredUI_PointInRect( float px, float py, wiredRect_t *r ) {
	return ( px >= r->x && px < r->x + r->w &&
	         py >= r->y && py < r->y + r->h );
}

static int WiredUI_FindItemAtCursor( wiredMenuDef_t *menu, float cx, float cy ) {
	int i;
	float ox = menu->fullscreen ? 0 : menu->rect.x;
	float oy = menu->fullscreen ? 0 : menu->rect.y;
	float sy = menu->scrollOffset;

	// iterate back-to-front so topmost item wins
	for ( i = menu->itemCount - 1; i >= 0; i-- ) {
		wiredItemDef_t *item = menu->items[i];
		wiredRect_t absRect;
		if ( !item->visible || item->decoration ) continue;

		// skip cvarTest-hidden items (can't focus what you can't see)
		if ( item->cvarTest[0] ) {
			char cvBuf[256];
			Cvar_VariableStringBuffer( item->cvarTest, cvBuf, sizeof( cvBuf ) );
			int cvInt = atoi( cvBuf );
			char cvIntStr[16];
			Com_sprintf( cvIntStr, sizeof( cvIntStr ), "%d", cvInt );
			if ( item->showCvar[0] ) {
				if ( !strstr( item->showCvar, cvBuf ) && !strstr( item->showCvar, cvIntStr ) ) continue;
			}
			if ( item->hideCvar[0] ) {
				if ( strstr( item->hideCvar, cvBuf ) || strstr( item->hideCvar, cvIntStr ) ) continue;
			}
		}
		absRect.x = ox + item->rect.x;
		absRect.y = oy + item->rect.y - sy;
		absRect.w = item->rect.w;
		absRect.h = item->rect.h;

		// skip items scrolled out of view
		float clipTop = menu->fullscreen ? 0 : menu->rect.y;
		float clipBottom = clipTop + ( menu->fullscreen ? SCREEN_HEIGHT : menu->rect.h );
		if ( absRect.y + absRect.h < clipTop || absRect.y > clipBottom ) continue;

		if ( WiredUI_PointInRect( cx, cy, &absRect ) ) return i;
	}
	return -1;
}

// ── key event ─────────────────────────────────────────────────────────
// Combines: v6 Menu_HandleKey structure, q3now q3_ui key dispatch,
// and ET:Legacy per-item events (onEsc, onEnter, onTab, execKey).

void WiredUI_KeyEvent( int key, qboolean down ) {
	wiredMenuDef_t *menu;
	wiredItemDef_t *focusedItem = NULL;
	int i;

	if ( !wired_initialized ) return;

	// text field editing mode — intercepts all keys while editing
	if ( wired_editingField && wired_editItem && down ) {
		char buff[1024];
		int len;

		Cvar_VariableStringBuffer( wired_editItem->cvar, buff, sizeof( buff ) );
		len = strlen( buff );

		if ( key & K_CHAR_FLAG ) {
			// character input
			int ch = key & ~K_CHAR_FLAG;

			if ( ch == 'h' - 'a' + 1 ) {
				// ctrl-h = backspace
				if ( wired_editCursorPos > 0 ) {
					memmove( &buff[wired_editCursorPos - 1], &buff[wired_editCursorPos], len + 1 - wired_editCursorPos );
					wired_editCursorPos--;
				}
				Cvar_Set( wired_editItem->cvar, buff );
			} else if ( ch == 'a' - 'a' + 1 ) {
				// ctrl-a = home
				wired_editCursorPos = 0;
			} else if ( ch == 'e' - 'a' + 1 ) {
				// ctrl-e = end
				wired_editCursorPos = len;
			} else if ( ch == 'v' - 'a' + 1 ) {
				// ctrl-v = paste from clipboard
				char *cbd = Sys_GetClipboardData();
				if ( cbd ) {
					int maxC = wired_editItem->maxChars > 0 ? wired_editItem->maxChars : 255;
					int pasteLen = strlen( cbd );
					int space = maxC - len;
					if ( pasteLen > space ) pasteLen = space;
					if ( pasteLen > 0 ) {
						memmove( &buff[wired_editCursorPos + pasteLen], &buff[wired_editCursorPos], len + 1 - wired_editCursorPos );
						Com_Memcpy( &buff[wired_editCursorPos], cbd, pasteLen );
						wired_editCursorPos += pasteLen;
						Cvar_Set( wired_editItem->cvar, buff );
					}
					Z_Free( cbd );
				}
			} else if ( ch >= 32 ) {
				// printable character — insert at cursor
				int maxC = wired_editItem->maxChars > 0 ? wired_editItem->maxChars : 255;
				if ( wired_editItem->type == ITEM_TYPE_NUMERICFIELD && ( ch < '0' || ch > '9' ) && ch != '.' && ch != '-' ) {
					// reject non-numeric
				} else if ( len < maxC ) {
					memmove( &buff[wired_editCursorPos + 1], &buff[wired_editCursorPos], len + 1 - wired_editCursorPos );
					buff[wired_editCursorPos] = ch;
					wired_editCursorPos++;
					Cvar_Set( wired_editItem->cvar, buff );
				}
			}
			return;
		}

		// non-character keys
		switch ( key ) {
			case K_ESCAPE:
				wired_editingField = qfalse;
				wired_editItem = NULL;
				return;

			case K_ENTER:
			case K_KP_ENTER:
				wired_editingField = qfalse;
				wired_editItem = NULL;
				return;

			case K_TAB:
				wired_editingField = qfalse;
				wired_editItem = NULL;
				// fall through to normal key handling for focus change
				break;

			case K_BACKSPACE:
				if ( keys[K_CTRL].down && wired_editCursorPos > 0 ) {
					// ctrl+backspace: delete previous word
					int pos = wired_editCursorPos;
					while ( pos > 0 && buff[pos - 1] == ' ' ) pos--;
					while ( pos > 0 && buff[pos - 1] != ' ' ) pos--;
					memmove( &buff[pos], &buff[wired_editCursorPos], len + 1 - wired_editCursorPos );
					wired_editCursorPos = pos;
					Cvar_Set( wired_editItem->cvar, buff );
				} else if ( wired_editCursorPos > 0 ) {
					memmove( &buff[wired_editCursorPos - 1], &buff[wired_editCursorPos], len + 1 - wired_editCursorPos );
					wired_editCursorPos--;
					Cvar_Set( wired_editItem->cvar, buff );
				}
				return;

			case K_DEL:
			case K_KP_DEL:
				if ( wired_editCursorPos < len ) {
					memmove( &buff[wired_editCursorPos], &buff[wired_editCursorPos + 1], len - wired_editCursorPos );
					Cvar_Set( wired_editItem->cvar, buff );
				}
				return;

			case K_LEFTARROW:
			case K_KP_LEFTARROW:
				if ( wired_editCursorPos > 0 ) wired_editCursorPos--;
				return;

			case K_RIGHTARROW:
			case K_KP_RIGHTARROW:
				if ( wired_editCursorPos < len ) wired_editCursorPos++;
				return;

			case K_HOME:
			case K_KP_HOME:
				wired_editCursorPos = 0;
				return;

			case K_END:
			case K_KP_END:
				wired_editCursorPos = len;
				return;

			default:
				return; // eat all other keys while editing
		}
	}

	// key binding capture mode — waiting for user to press a key
	if ( wired_waitingForKey && down ) {
		if ( key == K_ESCAPE ) {
			// cancel binding
			wired_waitingForKey = qfalse;
			wired_bindItem = NULL;
		} else if ( key == K_BACKSPACE || key == K_DEL ) {
			// clear all bindings for this command
			if ( wired_bindItem && wired_bindItem->cvar[0] ) {
				int k;
				for ( k = 0; k < MAX_KEYS; k++ ) {
					const char *b = Key_GetBinding( k );
					if ( b && !Q_stricmp( b, wired_bindItem->cvar ) ) {
						Key_SetBinding( k, "" );
					}
				}
			}
			wired_waitingForKey = qfalse;
			wired_bindItem = NULL;
		} else if ( wired_bindItem && wired_bindItem->cvar[0] ) {
			// remove this key from any OTHER command (conflict resolution)
			{
				const char *existing = Key_GetBinding( key );
				if ( existing && existing[0] && Q_stricmp( existing, wired_bindItem->cvar ) ) {
					Key_SetBinding( key, "" );
				}
			}
			// bind the key to this command
			Key_SetBinding( key, wired_bindItem->cvar );
			wired_waitingForKey = qfalse;
			wired_bindItem = NULL;
		}
		return;
	}

	menu = WiredUI_GetActiveMenu();
	if ( !menu ) return;

	if ( wired_focusItem >= 0 && wired_focusItem < menu->itemCount ) {
		focusedItem = menu->items[wired_focusItem];
	}

	// only process key-down for most actions
	if ( !down ) return;

	// ET:Legacy execKey: check ALL items for key-specific bindings
	for ( i = 0; i < menu->itemCount; i++ ) {
		wiredItemDef_t *item = menu->items[i];
		if ( item->execKeyCode && item->execKeyCode == key && item->execKeyAction[0] ) {
			WiredUI_RunScript( menu, item, item->execKeyAction );
			return;
		}
	}

	// ET:Legacy per-item events: onEsc, onEnter, onTab
	if ( focusedItem ) {
		if ( key == K_ESCAPE && focusedItem->onEsc[0] ) {
			WiredUI_RunScript( menu, focusedItem, focusedItem->onEsc );
			return;
		}
		if ( ( key == K_ENTER || key == K_KP_ENTER ) && focusedItem->onEnter[0] ) {
			WiredUI_RunScript( menu, focusedItem, focusedItem->onEnter );
			return;
		}
		if ( key == K_TAB && focusedItem->onTab[0] ) {
			WiredUI_RunScript( menu, focusedItem, focusedItem->onTab );
			return;
		}
	}

	// default key handling
	switch ( key ) {
		case K_ESCAPE:
			if ( menu->onESC[0] ) {
				WiredUI_RunScript( menu, NULL, menu->onESC );
			} else {
				// no onESC handler — pop the menu stack
				WiredUI_PopMenu();
			}
			break;

		case K_MOUSE1:
		case K_MOUSE2:
		case K_ENTER:
		case K_KP_ENTER:
			if ( !focusedItem ) {
				break;
			}

			// EDITFIELD items: click to start editing
			if ( ( focusedItem->type == ITEM_TYPE_EDITFIELD || focusedItem->type == ITEM_TYPE_NUMERICFIELD )
			     && focusedItem->cvar[0] ) {
				char buf[256];
				wired_editingField = qtrue;
				wired_editItem = focusedItem;
				Cvar_VariableStringBuffer( focusedItem->cvar, buf, sizeof( buf ) );
				wired_editCursorPos = strlen( buf );
				wired_editPaintOffset = 0;
				break;
			}

			// LISTBOX items: click to select row
			if ( focusedItem->type == ITEM_TYPE_LISTBOX && focusedItem->feeder != 0 ) {
				float rowH = focusedItem->elementheight > 0 ? focusedItem->elementheight : 16.0f;
				float menuOY = menu->fullscreen ? 0 : menu->rect.y;
				float scrollOff = menu->scrollOffset;
				float listAbsY = menuOY + focusedItem->rect.y - scrollOff;
				int clickedRow = (int)( ( wired_cursorY - listAbsY ) / rowH ) + focusedItem->listScrollOffset;
				int total = WiredUI_FeederCount( (int)focusedItem->feeder );

				if ( clickedRow >= 0 && clickedRow < total ) {
					focusedItem->listSelectedRow = clickedRow;
					WiredUI_FeederSelection( (int)focusedItem->feeder, clickedRow );

					// double-click detection: same row + same feeder within threshold
					if ( focusedItem->doubleClick[0] &&
					     clickedRow == wired_lastClickRow &&
					     focusedItem->feeder == wired_lastClickFeeder &&
					     ( cls.realtime - wired_lastClickTime ) < WIRED_DOUBLECLICK_TIME ) {
						WiredUI_RunScript( menu, focusedItem, focusedItem->doubleClick );
						wired_lastClickTime = 0;  // consumed
					} else {
						wired_lastClickTime = cls.realtime;
						wired_lastClickRow = clickedRow;
						wired_lastClickFeeder = focusedItem->feeder;
					}
				}
				if ( focusedItem->action[0] && ( key == K_MOUSE1 || key == K_ENTER || key == K_KP_ENTER ) ) {
					if ( wired_sfxAction ) S_StartLocalSound( wired_sfxAction, CHAN_LOCAL_SOUND );
					WiredUI_RunScript( menu, focusedItem, focusedItem->action );
				}
				break;
			}

			// BIND items: start key capture mode
			if ( focusedItem->type == ITEM_TYPE_BIND && focusedItem->cvar[0] ) {
				wired_waitingForKey = qtrue;
				wired_bindItem = focusedItem;
				break;
			}

			// cvar-bound items: handle interaction based on type
			if ( focusedItem->cvar[0] ) {
				char cvarBuf[256];
				Cvar_VariableStringBuffer( focusedItem->cvar, cvarBuf, sizeof( cvarBuf ) );

				switch ( focusedItem->type ) {
					case ITEM_TYPE_YESNO:
						// toggle 0 <-> 1
						Cvar_Set( focusedItem->cvar, atof( cvarBuf ) != 0 ? "0" : "1" );
						break;

					case ITEM_TYPE_MULTI:
						if ( focusedItem->multiData ) {
							int cur = -1, next, j;
							int dir = ( key == K_MOUSE2 ) ? -1 : 1;
							// find current value index
							for ( j = 0; j < focusedItem->multiData->count; j++ ) {
								if ( focusedItem->multiData->isStringList ) {
									if ( !Q_stricmp( cvarBuf, focusedItem->multiData->strValues[j] ) ) {
										cur = j; break;
									}
								} else {
									if ( focusedItem->multiData->floatValues[j] == atof( cvarBuf ) ) {
										cur = j; break;
									}
								}
							}
							// cycle to next
							next = ( cur + dir + focusedItem->multiData->count ) % focusedItem->multiData->count;
							if ( focusedItem->multiData->isStringList ) {
								Cvar_Set( focusedItem->cvar, focusedItem->multiData->strValues[next] );
							} else {
								Cvar_Set( focusedItem->cvar,
									va( "%g", focusedItem->multiData->floatValues[next] ) );
							}
							// fire action script on value change
							if ( focusedItem->action[0] ) {
								WiredUI_RunScript( menu, focusedItem, focusedItem->action );
							}
						}
						break;

					case ITEM_TYPE_SLIDER:
						{
							float val = atof( cvarBuf );
							float step = ( focusedItem->sliderData.maxVal - focusedItem->sliderData.minVal ) / 20.0f;
							if ( step < 0.01f ) step = 0.01f;
							if ( key == K_MOUSE2 ) step = -step;
							val += step;
							if ( val < focusedItem->sliderData.minVal ) val = focusedItem->sliderData.minVal;
							if ( val > focusedItem->sliderData.maxVal ) val = focusedItem->sliderData.maxVal;
							Cvar_Set( focusedItem->cvar, va( "%g", val ) );
						}
						break;

					default:
						break;
				}
			}

			// always run action script if present
			if ( focusedItem->action[0] ) {
				WiredUI_RunScript( menu, focusedItem, focusedItem->action );
			}
			break;

		case K_MWHEELUP:
			if ( focusedItem && focusedItem->type == ITEM_TYPE_LISTBOX ) {
				// adaptive scroll: 1 row for small lists, 3 for large
				int total = WiredUI_FeederCount( (int)focusedItem->feeder );
				int step = ( total > 20 ) ? 3 : 1;
				if ( focusedItem->listScrollOffset > 0 ) focusedItem->listScrollOffset -= step;
				if ( focusedItem->listScrollOffset < 0 ) focusedItem->listScrollOffset = 0;
				focusedItem->listScrollFadeTime = cls.realtime;
			} else {
				// adaptive menu scroll: larger step for tall menus
				float maxScroll = menu->contentHeight - menu->rect.h;
				float step = ( maxScroll > 400 ) ? 48.0f : 24.0f;
				if ( maxScroll > 0 ) {
					menu->scrollOffset -= step;
					if ( menu->scrollOffset < 0 ) menu->scrollOffset = 0;
					menu->scrollBarFadeTime = cls.realtime;
				}
			}
			break;

		case K_MWHEELDOWN:
			if ( focusedItem && focusedItem->type == ITEM_TYPE_LISTBOX && focusedItem->feeder != 0 ) {
				// adaptive scroll: 1 row for small lists, 3 for large
				int total = WiredUI_FeederCount( (int)focusedItem->feeder );
				float rowH = focusedItem->elementheight > 0 ? focusedItem->elementheight : 16.0f;
				int visibleRows = (int)( focusedItem->rect.h / rowH );
				int step = ( total > 20 ) ? 3 : 1;
				focusedItem->listScrollOffset += step;
				if ( focusedItem->listScrollOffset > total - visibleRows )
					focusedItem->listScrollOffset = total - visibleRows;
				if ( focusedItem->listScrollOffset < 0 ) focusedItem->listScrollOffset = 0;
				focusedItem->listScrollFadeTime = cls.realtime;
			} else {
				// adaptive menu scroll
				float maxScroll = menu->contentHeight - menu->rect.h;
				float step = ( maxScroll > 400 ) ? 48.0f : 24.0f;
				if ( maxScroll > 0 ) {
					menu->scrollOffset += step;
					if ( menu->scrollOffset > maxScroll ) menu->scrollOffset = maxScroll;
					menu->scrollBarFadeTime = cls.realtime;
				}
			}
			break;

		case K_LEFTARROW:
		case K_KP_LEFTARROW:
		case K_RIGHTARROW:
		case K_KP_RIGHTARROW:
			// left/right adjusts sliders and cycles multi items
			if ( focusedItem && focusedItem->cvar[0] ) {
				char cvarBuf[256];
				int dir = ( key == K_LEFTARROW || key == K_KP_LEFTARROW ) ? -1 : 1;
				Cvar_VariableStringBuffer( focusedItem->cvar, cvarBuf, sizeof( cvarBuf ) );

				if ( focusedItem->type == ITEM_TYPE_SLIDER ) {
					float val = atof( cvarBuf );
					float step = ( focusedItem->sliderData.maxVal - focusedItem->sliderData.minVal ) / 20.0f;
					if ( step < 0.01f ) step = 0.01f;
					val += step * dir;
					if ( val < focusedItem->sliderData.minVal ) val = focusedItem->sliderData.minVal;
					if ( val > focusedItem->sliderData.maxVal ) val = focusedItem->sliderData.maxVal;
					Cvar_Set( focusedItem->cvar, va( "%g", val ) );
				}
				else if ( focusedItem->type == ITEM_TYPE_MULTI && focusedItem->multiData ) {
					int cur = -1, next, j;
					for ( j = 0; j < focusedItem->multiData->count; j++ ) {
						if ( focusedItem->multiData->isStringList ) {
							if ( !Q_stricmp( cvarBuf, focusedItem->multiData->strValues[j] ) ) { cur = j; break; }
						} else {
							if ( focusedItem->multiData->floatValues[j] == atof( cvarBuf ) ) { cur = j; break; }
						}
					}
					next = ( cur + dir + focusedItem->multiData->count ) % focusedItem->multiData->count;
					if ( focusedItem->multiData->isStringList ) {
						Cvar_Set( focusedItem->cvar, focusedItem->multiData->strValues[next] );
					} else {
						Cvar_Set( focusedItem->cvar, va( "%g", focusedItem->multiData->floatValues[next] ) );
					}
					// fire action script on value change
					if ( focusedItem->action[0] ) {
						WiredUI_RunScript( menu, focusedItem, focusedItem->action );
					}
				}
				else if ( focusedItem->type == ITEM_TYPE_YESNO ) {
					Cvar_Set( focusedItem->cvar, atof( cvarBuf ) != 0 ? "0" : "1" );
				}
			}
			break;

		case K_UPARROW:
		case K_KP_UPARROW:
			{
				int start = ( wired_focusItem > 0 ) ? wired_focusItem - 1 : menu->itemCount - 1;
				for ( i = 0; i < menu->itemCount; i++ ) {
					int idx = ( start - i + menu->itemCount ) % menu->itemCount;
					if ( menu->items[idx]->visible && !menu->items[idx]->decoration ) {
						wired_focusItem = idx;
						wired_focusFromMouse = qfalse;
						if ( wired_sfxFocus ) S_StartLocalSound( wired_sfxFocus, CHAN_LOCAL_SOUND );
						break;
					}
				}
			}
			break;

		case K_TAB:
		case K_DOWNARROW:
		case K_KP_DOWNARROW:
			{
				int start = wired_focusItem + 1;
				for ( i = 0; i < menu->itemCount; i++ ) {
					int idx = ( start + i ) % menu->itemCount;
					if ( menu->items[idx]->visible && !menu->items[idx]->decoration ) {
						wired_focusItem = idx;
						wired_focusFromMouse = qfalse;
						if ( wired_sfxFocus ) S_StartLocalSound( wired_sfxFocus, CHAN_LOCAL_SOUND );
						break;
					}
				}
			}
			break;
	}
}

// ── mouse event ───────────────────────────────────────────────────────
// Accumulates deltas into virtual 640x480 cursor, updates focus item,
// fires mouseEnter/mouseExit scripts (ET:Legacy per-item events).

void WiredUI_MouseEvent( int dx, int dy ) {
	wiredMenuDef_t *menu;
	int oldFocus, newFocus;

	if ( !wired_initialized ) return;

	// accumulate deltas into virtual 640x480 cursor position
	wired_cursorX += dx;
	if ( wired_cursorX < 0 ) wired_cursorX = 0;
	else if ( wired_cursorX > SCREEN_WIDTH ) wired_cursorX = SCREEN_WIDTH;

	wired_cursorY += dy;
	if ( wired_cursorY < 0 ) wired_cursorY = 0;
	else if ( wired_cursorY > SCREEN_HEIGHT ) wired_cursorY = SCREEN_HEIGHT;

	menu = WiredUI_GetActiveMenu();
	if ( !menu ) return;

	oldFocus = wired_focusItem;
	newFocus = WiredUI_FindItemAtCursor( menu, wired_cursorX, wired_cursorY );

	// any mouse movement reactivates mouse-based focus
	wired_focusFromMouse = ( newFocus >= 0 );

	// fire mouseExit on old item, mouseEnter on new item
	if ( newFocus != oldFocus ) {
		if ( oldFocus >= 0 && oldFocus < menu->itemCount ) {
			wiredItemDef_t *old = menu->items[oldFocus];
			if ( old->mouseExit[0] ) {
				WiredUI_RunScript( menu, old, old->mouseExit );
			}
			if ( old->leaveFocus[0] ) {
				WiredUI_RunScript( menu, old, old->leaveFocus );
			}
		}
		if ( newFocus >= 0 && newFocus < menu->itemCount ) {
			wiredItemDef_t *cur = menu->items[newFocus];
			if ( cur->mouseEnter[0] ) {
				WiredUI_RunScript( menu, cur, cur->mouseEnter );
			}
			if ( cur->onFocus[0] ) {
				WiredUI_RunScript( menu, cur, cur->onFocus );
			}
		}
		wired_focusItem = newFocus;
		if ( newFocus >= 0 && wired_sfxFocus ) S_StartLocalSound( wired_sfxFocus, CHAN_LOCAL_SOUND );
	}
}

void WiredUI_SetActiveMenu( int menu ) {
	if ( !wired_initialized ) {
		return;
	}

	wired_activeMenu = menu;

	if ( menu != UIMENU_NONE ) {
		// activate UI key catcher so the engine routes input and draw calls to us
		Key_SetCatcher( KEYCATCH_UI );

		if ( menu == UIMENU_INGAME ) {
			// in single-player the server pauses when cl_paused is set;
			// the UI VM (q3_ui / ui) does this explicitly — we must too
			Cvar_Set( "cl_paused", "1" );
		}

		Com_DPrintf( "WiredUI: SetActiveMenu %d\n", menu );
	} else {
		Key_SetCatcher( Key_GetCatcher() & ~KEYCATCH_UI );
		Cvar_Set( "cl_paused", "0" );
	}
}

qboolean WiredUI_IsFullscreen( void ) {
	if ( !wired_initialized ) {
		return qfalse;
	}

	// main menu is always "fullscreen" for the engine — prevents 3D scene rendering
	// at CA_DISCONNECTED. Wired UI draws its own background (clouds) in Refresh.
	return ( wired_activeMenu == UIMENU_MAIN );
}

qboolean WiredUI_ConsoleCommand( int realtime ) {
	if ( !wired_initialized ) {
		return qfalse;
	}

	// TODO Phase 2: handle UI console commands
	return qfalse;
}

void WiredUI_DrawConnectScreen( qboolean overlay ) {
	wiredMenuDef_t	*menu;
	const char		*status;
	const char		*info;
	char			buf[MAX_STRING_CHARS];
	vec4_t			white = { 1, 1, 1, 1 };
	vec4_t			dim   = { 0.6f, 0.6f, 0.6f, 1 };
	float			y;

	if ( !wired_initialized ) {
		return;
	}

	// CA_LOADING/CA_PRIMED overlay: just show a brief "Loading..." strip
	// so it doesn't flash away too fast on fast loads (matches TA_UI behavior)
	if ( overlay ) {
		vec4_t overlayBg = { 0, 0, 0, 0.6f };
		SCR_FillRect( 0, 440, SCREEN_WIDTH, 40, overlayBg );
		SCR_DrawStringExt( 8, 448, 8, "Loading...", white, qtrue, qfalse );

		// show server message if present
		if ( clc.serverMessage[0] ) {
			SCR_DrawStringExt( 8, 460, 8, clc.serverMessage, dim, qtrue, qfalse );
		}
		return;
	}

	// full-screen connection dialog
	menu = WiredUI_FindMenu( "connect" );
	if ( menu ) {
		WiredUI_RenderMenuOverlay( menu, cls.realtime );
	} else {
		// fallback: dark background if connect.menu isn't loaded
		vec4_t bg = { 0.05f, 0.05f, 0.1f, 1.0f };
		SCR_FillRect( 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, bg );
	}

	// dynamic status text — drawn on top of the menu
	y = 260;

	// server name
	if ( cls.servername[0] ) {
		if ( !Q_stricmp( cls.servername, "localhost" ) ) {
			info = "Starting local server...";
		} else {
			info = va( "Connecting to %s", cls.servername );
		}
		SCR_DrawStringExt( 200, (int)y, 8, info, white, qtrue, qfalse );
		y += 16;
	}

	// connection state
	switch ( cls.state ) {
		case CA_CONNECTING:
			status = va( "Awaiting connection... %d", clc.connectPacketCount );
			break;
		case CA_CHALLENGING:
			status = va( "Awaiting challenge... %d", clc.connectPacketCount );
			break;
		case CA_CONNECTED:
			// check for download in progress
			if ( clc.downloadName[0] ) {
				int pct = 0;
				if ( clc.downloadSize > 0 ) {
					pct = (int)( (float)clc.downloadCount * 100.0f / (float)clc.downloadSize );
				}
				status = va( "Downloading %s... %d%%", clc.downloadName, pct );
			} else {
				status = "Awaiting gamestate...";
			}
			break;
		default:
			status = "Connecting...";
			break;
	}
	SCR_DrawStringExt( 200, (int)y, 8, status, dim, qtrue, qfalse );
	y += 16;

	// server error message
	if ( clc.serverMessage[0] ) {
		vec4_t errColor = { 1, 0.4f, 0.4f, 1 };
		SCR_DrawStringExt( 200, (int)y, 8, clc.serverMessage, errColor, qtrue, qfalse );
		y += 16;
	}

	// MOTD from master server
	Cvar_VariableStringBuffer( "cl_motdString", buf, sizeof( buf ) );
	if ( buf[0] ) {
		SCR_DrawStringExt( 200, (int)y, 8, buf, dim, qtrue, qfalse );
	}
}

// ── overlay renderer (for scoreboard, vote panel, etc.) ──────────────
// Renders a menu's background + items without cursor/focus/tooltip logic.
// Called from WiredHud_Routine() for in-game overlays that need .menu layout.

void WiredUI_RenderMenuOverlay( wiredMenuDef_t *menu, int realtime ) {
	int i;

	if ( !menu ) return;

	float menuX = menu->fullscreen ? 0 : menu->rect.x;
	float menuY = menu->fullscreen ? 0 : menu->rect.y;
	float menuW = menu->fullscreen ? SCREEN_WIDTH : menu->rect.w;
	float menuH = menu->fullscreen ? SCREEN_HEIGHT : menu->rect.h;

	// render background (skip when height=0 — scoreboard draws its own bg)
	if ( menuH > 0 ) {
		float bgX = menu->fullscreen ? 0 : menu->rect.x;
		float bgY = menu->fullscreen ? 0 : menu->rect.y;
		float bgW = menu->fullscreen ? SCREEN_WIDTH : menu->rect.w;
		float bgH = menu->fullscreen ? SCREEN_HEIGHT : menu->rect.h;

		if ( menu->style == WINDOW_STYLE_SHADER && menu->background[0] ) {
			qhandle_t bgShader = re.RegisterShaderNoMip( menu->background );
			if ( bgShader ) {
				re.SetColor( NULL );
				SCR_DrawPic( bgX, bgY, bgW, bgH, bgShader );
			}
		} else if ( menu->style == WINDOW_STYLE_GRADIENT && menu->backcolor[3] > 0.0f ) {
			SCR_FillRect( bgX, bgY, bgW, bgH, menu->backcolor );
			if ( wired_gradientBarShader ) {
				vec4_t gc;
				Vector4Copy( menu->backcolor, gc );
				gc[3] *= 0.5f;
				re.SetColor( gc );
				SCR_DrawPic( bgX, bgY, bgW, bgH, wired_gradientBarShader );
				re.SetColor( NULL );
			}
		} else if ( menu->style == WINDOW_STYLE_FILLED && menu->backcolor[3] > 0.0f ) {
			SCR_FillRect( bgX, bgY, bgW, bgH, menu->backcolor );
		}
	}

	// render items
	for ( i = 0; i < menu->itemCount; i++ ) {
		wiredItemDef_t *item = menu->items[i];

		if ( !item->visible ) continue;

		// cvarTest + showCvar/hideCvar conditional visibility
		if ( item->cvarTest[0] ) {
			char testBuf[256];
			Cvar_VariableStringBuffer( item->cvarTest, testBuf, sizeof( testBuf ) );

			if ( item->showCvar[0] ) {
				if ( !strstr( item->showCvar, testBuf ) ) continue;
			}
			if ( item->hideCvar[0] ) {
				if ( strstr( item->hideCvar, testBuf ) ) continue;
			}
		}

		float itemX = menuX + item->rect.x;
		float itemY = menuY + item->rect.y;

		// clip items outside menu bounds (skip when height=0, meaning auto-size)
		if ( menuH > 0 && ( itemY + item->rect.h < menuY || itemY > menuY + menuH ) ) continue;

		// draw item background
		if ( item->style == WINDOW_STYLE_SHADER && item->background[0] ) {
			qhandle_t itemBg = re.RegisterShaderNoMip( item->background );
			if ( itemBg ) {
				re.SetColor( NULL );
				SCR_DrawPic( itemX, itemY, item->rect.w, item->rect.h, itemBg );
			}
		} else if ( item->style == WINDOW_STYLE_FILLED && item->backcolor[3] > 0.0f ) {
			SCR_FillRect( itemX, itemY, item->rect.w, item->rect.h, item->backcolor );
		}

		// draw LISTBOX items (feeder-driven)
		if ( item->type == ITEM_TYPE_LISTBOX && item->feeder != 0 ) {
			int feederID = (int)item->feeder;
			int totalRows = WiredUI_FeederCount( feederID );
			float rowH = item->elementheight > 0 ? item->elementheight : 16.0f;
			int visibleRows = (int)( item->rect.h / rowH );
			int row, col;
			float charSize = item->textscale >= 0.7f ? 16.0f : 8.0f;
			vec4_t rowColor;
			float contentW = item->rect.w;

			// draw list background
			if ( item->backcolor[3] > 0 ) {
				SCR_FillRect( itemX, itemY, item->rect.w, item->rect.h, item->backcolor );
			}

			// draw rows
			for ( row = 0; row < visibleRows && ( item->listScrollOffset + row ) < totalRows; row++ ) {
				int dataRow = item->listScrollOffset + row;
				float rowY = itemY + row * rowH;

				// draw columns
				Vector4Copy( item->forecolor, rowColor );
				float colX = itemX + 4;
				for ( col = 0; col < ( item->columns > 0 ? item->columns : 1 ); col++ ) {
					const char *text = WiredUI_FeederItemText( feederID, dataRow, col );
					float colW = ( col < item->columns && item->columnWidths[col] > 0 )
						? item->columnWidths[col] : contentW;
					if ( text && text[0] ) {
						int maxChars = (int)( ( colW - 4 ) / charSize );
						if ( maxChars < 1 ) maxChars = 1;
						if ( (int)strlen( text ) > maxChars ) {
							char clipped[128];
							Q_strncpyz( clipped, text, sizeof( clipped ) );
							if ( maxChars < (int)sizeof( clipped ) ) {
								clipped[maxChars] = '\0';
							}
							SCR_DrawStringExt( (int)colX, (int)( rowY + 2 ), charSize,
								clipped, rowColor, qfalse, qfalse );
						} else {
							SCR_DrawStringExt( (int)colX, (int)( rowY + 2 ), charSize,
								text, rowColor, qfalse, qfalse );
						}
					}
					colX += colW;
				}
			}

			continue;
		}

		// draw SCORELIST widget — rich scoreboard with per-cell coloring
		if ( item->type == ITEM_TYPE_SCORELIST && item->feeder != 0 ) {
			extern void WiredHud_DrawScorelistWidget( float x, float y, float w, float h,
				int feederID, const vec4_t textColor );
			WiredHud_DrawScorelistWidget( itemX, itemY, item->rect.w, item->rect.h,
				(int)item->feeder, item->forecolor );
			continue;
		}

		// draw text items (static text or cvar-bound) using modern font system
		{
			const char *drawText = NULL;
			char cvarBuf[256];

			if ( item->text[0] ) {
				drawText = item->text;
			} else if ( item->cvar[0] && item->type == ITEM_TYPE_TEXT ) {
				Cvar_VariableStringBuffer( item->cvar, cvarBuf, sizeof( cvarBuf ) );
				if ( cvarBuf[0] ) drawText = cvarBuf;
			}

			if ( drawText ) {
				float charW, charH;
				int flags = DS_PROPORTIONAL | DS_SHADOW;

				// textscale maps to charW/charH (same mapping as main menu renderer)
				charW = item->textscale >= 0.7f ? 16.0f : ( item->textscale >= 0.3f ? 10.0f : 8.0f );
				charH = charW * 1.4f;

				if ( item->textalign == ITEM_ALIGN_CENTER ) {
					flags |= DS_HCENTER;
				} else if ( item->textalign == ITEM_ALIGN_RIGHT ) {
					flags |= DS_HRIGHT;
				} else {
					flags |= DS_HLEFT;
				}

				// position: centered items use widget midpoint, others use left edge
				float x, y;
				if ( item->textalign == ITEM_ALIGN_CENTER && item->rect.w > 0 ) {
					x = itemX + item->rect.w * 0.5f;
				} else if ( item->textalign == ITEM_ALIGN_RIGHT && item->rect.w > 0 ) {
					x = itemX + item->rect.w;
				} else {
					x = itemX;
				}
				y = itemY + item->textaligny;

				CG_FontSelect( 2 );
				CG_ModernDrawString( x, y, drawText, item->forecolor,
					charW, charH, (int)item->rect.w, flags, NULL );
			}
		}
	}
}

void WiredUI_ReloadHud( void ) {
	const char *manifest = WiredUI_GetManifestPath();
	Com_Printf( "WiredUI: reloading HUD from '%s'...\n", manifest );

	// destroy all active elements (frees Z_Malloc'd contexts)
	WiredHud_DestroyAllElements();

	// two-phase safe reload
	WiredUI_SafeReload( manifest );

	// recreate HUD elements from hudOverlay menus
	WiredHud_LoadFromMenus();

	Com_Printf( "WiredUI: HUD reloaded, %d elements active\n", WiredHud_GetElementCount() );
}

void WiredUI_ReloadMenus( void ) {
	const char *manifest = WiredUI_GetManifestPath();
	char theme[64];
	Cvar_VariableStringBuffer( "ui_theme", theme, sizeof( theme ) );

	Com_Printf( "WiredUI: reloading menus%s%s%s...\n",
		theme[0] ? " (theme: " : "",
		theme[0] ? theme : "",
		theme[0] ? ")" : "" );

	// stop all cinematics before reload
	{
		int i;
		for ( i = 0; i < wired_menuStackDepth; i++ ) {
			wiredMenuDef_t *m = WiredUI_FindMenu( wired_menuStack[i] );
			if ( m && m->cinematicHandle >= 0 ) {
				CIN_StopCinematic( m->cinematicHandle );
				m->cinematicHandle = -1;
			}
		}
	}

	// save current menu name for re-open after reload
	char currentMenu[64] = {0};
	if ( wired_menuStackDepth > 0 ) {
		Q_strncpyz( currentMenu, wired_menuStack[wired_menuStackDepth - 1], sizeof( currentMenu ) );
	}
	wired_menuStackDepth = 0;
	wired_focusItem = -1;

	// destroy HUD elements (they'll be recreated after reload)
	WiredHud_DestroyAllElements();

	// two-phase safe reload: parse new → swap, or keep old on failure
	if ( WiredUI_SafeReload( manifest ) ) {
		if ( theme[0] ) {
			Com_Printf( "Theme switched to '%s'. Menus reloaded.\n", theme );
		} else {
			Com_Printf( "Menus reloaded successfully.\n" );
		}
	}
	// on failure, SafeReload already restored old menus + printed error

	// recreate HUD elements from (possibly new) hudOverlay menus
	WiredHud_LoadFromMenus();

	// re-open the menu that was active before reload
	if ( currentMenu[0] && WiredUI_FindMenu( currentMenu ) ) {
		WiredUI_PushMenu( currentMenu );
	}
}

#endif // FEAT_WIRED_UI
