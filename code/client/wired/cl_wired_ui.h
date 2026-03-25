/*
===========================================================================
cl_wired_ui.h — Wired UI: unified menu/HUD system (client-side)

When FEAT_WIRED_UI is enabled, this replaces the traditional UI VM (q3_ui
or Team Arena ui) with an embedded menu parser that runs directly in the
client. Menus are defined in .menu files, HUD in .hud files, and (Phase 5)
in-game surface GUIs in .gui files — all sharing the same parser.

Architecture:
  ┌────────────────────────────┐     ┌──────────────────────────┐
  │  CLIENT (engine)           │     │  CGAME (mod-replaceable)  │
  │  ├─ Wired parser+renderer  │     │                          │
  │  ├─ data binding engine    │◄────│  WiredUI_RegisterSymbol() │
  │  │   {{ name }} → callback │     │  WiredUI_RegisterElement()│
  │  └─ hot reload             │     │                          │
  └────────────────────────────┘     └──────────────────────────┘

cgame registers data symbols (health, armor, etc.) and HUD element types
(fps, weaponlist, etc.) at init. The client parser resolves {{ symbol }}
references and calls registered element routines during rendering.
===========================================================================
*/

#ifndef CL_WIRED_UI_H
#define CL_WIRED_UI_H

#include "../../game/q_feats.h"

#if FEAT_WIRED_UI

#include "../../qcommon/q_shared.h"

// ── public API (called from cl_ui.c, cl_keys.c, cl_scrn.c, etc.) ─────

void     WiredUI_Init( qboolean inGameUI );
void     WiredUI_Shutdown( void );
void     WiredUI_Refresh( int realtime );
void     WiredUI_KeyEvent( int key, qboolean down );
void     WiredUI_MouseEvent( int dx, int dy );
void     WiredUI_SetActiveMenu( int menu );       // UIMENU_NONE, UIMENU_MAIN, UIMENU_INGAME
qboolean WiredUI_IsFullscreen( void );
qboolean WiredUI_ConsoleCommand( int realtime );
void     WiredUI_DrawConnectScreen( qboolean overlay );

// ── hot reload ────────────────────────────────────────────────────────

void     WiredUI_ReloadHud( void );               // /hud_reload console command
void     WiredUI_ReloadMenus( void );             // /menu_reload console command

// ── symbol registration (called from cgame via trap) ──────────────────
//
// Symbols are named data sources that .hud files reference via {{ name }}.
// cgame registers them at CG_Init time (batch) and optionally later (late).
//
// Example:
//   WiredUI_RegisterSymbol("health", CG_GetPlayerHealth, NULL);
//   WiredUI_RegisterSymbol("armor",  CG_GetPlayerArmor,  NULL);
//
// In a .hud file:
//   itemDef { text "{{ health }}" ... }

typedef const char *(*wiredSymbolCallback_t)( void *userData );

void     WiredUI_RegisterSymbol( const char *name,
                                  wiredSymbolCallback_t callback,
                                  void *userData );
void     WiredUI_UnregisterSymbol( const char *name );
const char *WiredUI_ResolveSymbol( const char *name ); // returns "???" if not found

// ── element registration (called from cgame via trap) ─────────────────
//
// HUD elements are the SuperHUD element types (fps, weaponlist, etc.)
// that .hud files reference via the hudElement keyword.
//
// Example:
//   WiredUI_RegisterElement("fps", &CG_SHUDElementFPSCreate,
//                                   &CG_SHUDElementFPSRoutine,
//                                   &CG_SHUDElementFPSDestroy);
//
// In a .hud file:
//   itemDef { hudElement "fps" rect 620 2 0 0 ... }

typedef void *(*wiredElementCreate_t)( const void *config );
typedef void  (*wiredElementRoutine_t)( void *context );
typedef void  (*wiredElementDestroy_t)( void *context );

void     WiredUI_RegisterElement( const char *name,
                                   wiredElementCreate_t create,
                                   wiredElementRoutine_t routine,
                                   wiredElementDestroy_t destroy );

// ── cgame batch registration ──────────────────────────────────────────
//
// Called once from CG_Init to register all core symbols and elements.
// After this call, the client can safely parse .hud files.

void     WiredUI_RegisterCoreSymbols( void );     // batch: health, armor, ammo, etc.
void     WiredUI_RegisterCoreElements( void );    // batch: all 167 SuperHUD element types

// ── types ─────────────────────────────────────────────────────────────

typedef struct {
	float x, y, w, h;
} wiredRect_t;

#define WIRED_MAX_ITEMS_PER_MENU   128
#define WIRED_MAX_MENUS            64
#define WIRED_MAX_SCRIPT_LEN       1024
#define WIRED_MAX_MULTI_CHOICES    32

// multi-choice data (for ITEM_TYPE_MULTI)
typedef struct {
	char    labels[WIRED_MAX_MULTI_CHOICES][64];   // display labels
	char    strValues[WIRED_MAX_MULTI_CHOICES][64]; // string values (for cvarStrList)
	float   floatValues[WIRED_MAX_MULTI_CHOICES];   // float values (for cvarFloatList)
	int     count;
	qboolean isStringList;                          // qtrue = cvarStrList, qfalse = cvarFloatList
} wiredMultiDef_t;

// slider data (for ITEM_TYPE_SLIDER)
typedef struct {
	float   defVal;
	float   minVal;
	float   maxVal;
} wiredSliderDef_t;

typedef struct wiredItemDef_s {
	char            name[64];
	char            text[256];
	char            group[64];
	int             type;                   // ITEM_TYPE_*
	wiredRect_t     rect;
	int             textalign;              // ITEM_ALIGN_*
	float           textalignx;
	float           textaligny;
	float           textscale;
	int             textstyle;              // ITEM_TEXTSTYLE_*
	vec4_t          forecolor;
	vec4_t          backcolor;
	vec4_t          bordercolor;
	int             style;                  // WINDOW_STYLE_*
	int             border;                 // WINDOW_BORDER_*
	float           bordersize;
	char            background[64];
	// scripts (v6 + ET:Legacy per-item events)
	char            action[WIRED_MAX_SCRIPT_LEN];
	char            onFocus[WIRED_MAX_SCRIPT_LEN];
	char            leaveFocus[WIRED_MAX_SCRIPT_LEN];
	char            mouseEnter[WIRED_MAX_SCRIPT_LEN];
	char            mouseExit[WIRED_MAX_SCRIPT_LEN];
	char            onEsc[WIRED_MAX_SCRIPT_LEN];    // ET:Legacy: per-item ESC handler
	char            onEnter[WIRED_MAX_SCRIPT_LEN];   // ET:Legacy: per-item Enter handler
	char            onTab[WIRED_MAX_SCRIPT_LEN];     // ET:Legacy: per-item Tab handler
	char            doubleClick[WIRED_MAX_SCRIPT_LEN]; // v6: on double-click

	// execKey (ET:Legacy): bind action to specific key
	int             execKeyCode;                      // key code (0 = none)
	char            execKeyAction[WIRED_MAX_SCRIPT_LEN];

	// tooltip (ET:Legacy + QL)
	char            tooltip[256];

	char            cvar[64];
	qboolean        visible;
	qboolean        decoration;
	char            cvarTest[64];
	char            showCvar[256];
	char            hideCvar[256];
	char            enableCvar[256];        // v6: enable when cvarTest matches
	char            disableCvar[256];       // v6: disable when cvarTest matches
	int             ownerdraw;
	int             ownerdrawFlag;
	float           feeder;
	float           elementwidth;
	float           elementheight;
	char            hudElement[64];         // Phase 3

	// v6 + Phase 2.5 additions
	vec4_t          outlinecolor;
	float           special;                // ownerdraw spacing parameter
	int             align;                  // HUD_VERTICAL(0) / HUD_HORIZONTAL(1)
	qboolean        notselectable;

	// addColorRange — dynamic coloring by value (health/armor bars)
	#define WIRED_MAX_COLOR_RANGES  4
	struct {
		float   low, high;
		vec4_t  color;
	} colorRanges[WIRED_MAX_COLOR_RANGES];
	int             numColorRanges;

	// SuperHUD-specific properties (Phase 3: hudElement items)
	char            fontName[MAX_QPATH];    // font name ("sansman", "id", etc.)
	vec2_t          fontSize;               // fontsize W H (separate from textscale)
	int             direction;              // 0=L2R, 1=R2L, 2=T2B, 3=B2T
	qboolean        fillFlag;              // fill background
	qboolean        monospace;
	vec4_t          color2;                 // secondary color (e.g. active weapon)
	int             alignV;                 // 0=top, 1=center, 2=bottom
	vec4_t          fadeColor;              // fade target color
	int             fadeDelay;              // ms before fade starts
	int             timeMs;                 // element display duration (ms)
	char            image[MAX_QPATH];       // image/shader name (SuperHUD "image" keyword)
	char            bind[32];               // data binding name ("health", "armor", "ammo")

	// cvar binding data
	wiredMultiDef_t *multiData;             // for ITEM_TYPE_MULTI (allocated from pool)
	wiredSliderDef_t sliderData;            // for ITEM_TYPE_SLIDER (cvarFloat min/max)
	int             maxChars;               // for ITEM_TYPE_EDITFIELD
	int             maxPaintChars;          // visible chars in edit field

	// listbox data (for ITEM_TYPE_LISTBOX)
	int             columns;                // number of columns
	int             columnWidths[8];        // width per column (max 8)
	int             listScrollOffset;       // first visible row
	int             listSelectedRow;        // selected row index (-1 = none)
	int             listScrollFadeTime;     // cls.realtime when last scrolled (for scrollbar fade)

	// transition animation state (Phase 2.5)
	wiredRect_t     transFrom;              // start rect
	wiredRect_t     transTo;                // target rect
	int             transStartTime;         // cls.realtime when transition started (0 = inactive)
	int             transDuration;          // total duration in ms
} wiredItemDef_t;

typedef struct wiredMenuDef_s {
	char              name[64];
	wiredRect_t       rect;
	qboolean          fullscreen;
	qboolean          visible;
	int               style;
	vec4_t            forecolor;
	vec4_t            backcolor;
	vec4_t            focuscolor;
	char              background[64];
	char              soundLoop[64];
	char              onOpen[WIRED_MAX_SCRIPT_LEN];
	char              onClose[WIRED_MAX_SCRIPT_LEN];
	char              onESC[WIRED_MAX_SCRIPT_LEN];
	wiredItemDef_t   *items[WIRED_MAX_ITEMS_PER_MENU];
	int               itemCount;

	// Wired UI extensions
	qboolean          hudOverlay;           // Phase 3: passive HUD overlay
	qboolean          modal;                // ET:Legacy: captures all input
	qboolean          alwaysOnTop;          // ET:Legacy: z-order override
	qboolean          popup;                // v6: popup menu
	qboolean          outOfBoundsClick;     // v6: close on click outside
	int               border;               // WINDOW_BORDER_*
	float             bordersize;
	vec4_t            bordercolor;
	vec4_t            disablecolor;
	char              font[64];             // per-menu font override
	float             fadeClamp;            // max fade alpha
	int               fadeCycle;            // fade cycle time (ms)
	float             fadeAmount;           // fade step per cycle

	// menu-level scrolling (Wired UI innovation — HTML DIV-style overflow)
	float             scrollOffset;         // current scroll Y offset (pixels)
	float             contentHeight;        // computed: bottom-most item Y + H
	float             scrollVelocity;       // for smooth scrolling (momentum)
	int               scrollBarFadeTime;    // timestamp for scrollbar fade-out

	// fade animation (v6 assetGlobalDef: fadeClamp/fadeCycle/fadeAmount)
	int               openTime;             // cls.realtime when menu was opened
	float             fadeAlpha;            // current fade alpha (0..fadeClamp)
} wiredMenuDef_t;

// ── overlay rendering (for in-game overlays like scoreboard) ──────────
void     WiredUI_RenderMenuOverlay( wiredMenuDef_t *menu, int realtime );

// ── feeder system ─────────────────────────────────────────────────────
//
// Feeders provide data for ITEM_TYPE_LISTBOX items. Each feeder has an ID
// (matching FEEDER_* constants from menudef.h) and callbacks for count,
// item text, and selection handling.
//
// Unlike v6 (which routes feeders through displayContextDef_t callbacks),
// Wired UI stores feeder callbacks directly — no VM indirection.

typedef int         (*wiredFeederCount_t)( int feederID );
typedef const char *(*wiredFeederItemText_t)( int feederID, int index, int column );
typedef void        (*wiredFeederSelection_t)( int feederID, int index );

#define WIRED_MAX_FEEDERS  32

void     WiredUI_RegisterFeeder( int feederID,
                                  wiredFeederCount_t count,
                                  wiredFeederItemText_t itemText,
                                  wiredFeederSelection_t selection );
int      WiredUI_FeederCount( int feederID );
const char *WiredUI_FeederItemText( int feederID, int index, int column );
void     WiredUI_FeederSelection( int feederID, int index );

// ── feeder data loading (cl_wired_feeders.c) ──────────────────────────

void     WiredUI_RegisterCoreFeeders( void );
void     WiredFeeder_LoadMaps( void );
void     WiredFeeder_LoadDemos( void );
void     WiredFeeder_LoadMods( void );

// ── menu stack ────────────────────────────────────────────────────────

#define WIRED_MENU_STACK_DEPTH  8

void     WiredUI_PushMenu( const char *name );
void     WiredUI_PopMenu( void );
void     WiredUI_CloseAllMenus( void );
wiredMenuDef_t *WiredUI_GetActiveMenu( void );

// ── parser (cl_wired_parse.c) ─────────────────────────────────────────

qboolean WiredUI_LoadMenus( const char *manifestFile );
qboolean WiredUI_LoadMenuFile( const char *filename );
int      WiredUI_GetMenuCount( void );
wiredMenuDef_t *WiredUI_FindMenu( const char *name );
void     WiredUI_ClearMenus( void );
void     WiredUI_ResetPool( void );

// ── memory pool ───────────────────────────────────────────────────────

#define WIRED_MENU_POOL_SIZE   (8 * 1024 * 1024)  // 8MB for menus
#define WIRED_HUD_POOL_SIZE    (512 * 1024)        // 512KB for HUD overlays

// ── dispatch macros ───────────────────────────────────────────────────
//
// These replace VM_Call(uivm, ...) throughout the client code.
// When FEAT_WIRED_UI=1, they call the embedded WiredUI functions directly.
// When FEAT_WIRED_UI=0, the old VM_Call path is used (macros not defined).

#define UI_VM_ACTIVE            (1)  // always active — no VM to check
#define UI_CALL_REFRESH(rt)     WiredUI_Refresh(rt)
#define UI_CALL_KEY_EVENT(k,d)  WiredUI_KeyEvent(k,d)
#define UI_CALL_MOUSE_EVENT(x,y) WiredUI_MouseEvent(x,y)
#define UI_CALL_SET_ACTIVE(m)   WiredUI_SetActiveMenu(m)
#define UI_CALL_IS_FULLSCREEN() WiredUI_IsFullscreen()
#define UI_CALL_CONNECT(o)      WiredUI_DrawConnectScreen(o)
#define UI_CALL_CONSOLE_CMD(rt) WiredUI_ConsoleCommand(rt)

#else // !FEAT_WIRED_UI

// ── fallback: traditional UI VM path ──────────────────────────────────

extern vm_t *uivm;

#define UI_VM_ACTIVE            (uivm != NULL)
#define UI_CALL_REFRESH(rt)     VM_Call(uivm, 1, UI_REFRESH, rt)
#define UI_CALL_KEY_EVENT(k,d)  VM_Call(uivm, 2, UI_KEY_EVENT, k, d)
#define UI_CALL_MOUSE_EVENT(x,y) VM_Call(uivm, 2, UI_MOUSE_EVENT, x, y)
#define UI_CALL_SET_ACTIVE(m)   VM_Call(uivm, 1, UI_SET_ACTIVE_MENU, m)
#define UI_CALL_IS_FULLSCREEN() VM_Call(uivm, 0, UI_IS_FULLSCREEN)
#define UI_CALL_CONNECT(o)      VM_Call(uivm, 1, UI_DRAW_CONNECT_SCREEN, o)
#define UI_CALL_CONSOLE_CMD(rt) VM_Call(uivm, 1, UI_CONSOLE_COMMAND, rt)

#endif // FEAT_WIRED_UI
#endif // CL_WIRED_UI_H
