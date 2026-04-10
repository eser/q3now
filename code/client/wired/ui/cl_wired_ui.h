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

#include "../../../qcommon/q_feats.h"

#if FEAT_WIRED_UI

#include "../../../qcommon/q_shared.h"
#include "cl_wired_layout.h"
#include "cl_wired_fonts.h"

// ── public API (called from cl_ui.c, cl_keys.c, cl_scrn.c, etc.) ─────

void     WiredUI_Init( qboolean inGameUI );
void     WiredUI_Shutdown( void );
void     WiredUI_Refresh( int realtime );
void     WiredUI_KeyEvent( int key, qboolean down );
void     WiredUI_MouseEvent( int dx, int dy );
void     WiredUI_SetActiveMenu( int menu );       // UIMENU_NONE, UIMENU_MAIN, UIMENU_INGAME
qboolean WiredUI_IsFullscreen( void );
void     WiredUI_DrawConnectScreen( qboolean overlay );

// ── health / recovery ─────────────────────────────────────────────────
// See cl_wired_ui.c for the three-layer model and recovery semantics.

int      WiredUI_GetMenuStackDepth( void );       // 0 = nothing on stack
const char *WiredUI_GetMenuStackTop( void );      // "" if stack is empty
qboolean WiredUI_IsHealthy( void );               // pool + uiStarted + menus loaded
qboolean WiredUI_EnsureLoaded( void );            // idempotent re-init; rate-limited
void     WiredUI_Activate( void );                // brings compositor to foreground
int      WiredUI_GetLastRecoveryFailTime( void ); // ms timestamp of last failed EnsureLoaded

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

// ── populate callbacks (for dynamicMulti / runtime-populated MULTI) ───
//
// A populate callback fills in an option list at menu open / render
// time. .wmenu files reference one via:
//
//   itemDef {
//       type 12                            // MULTI
//       cvar "s_device"
//       populateCallback "audio_devices"
//       ...
//   }
//
// At render time the MULTI item invokes the named callback once per
// frame; the callback returns option labels/values and a state describing
// the result (loading / empty / error / success / partial). The renderer
// dispatches per-state visuals (e.g. "No audio devices detected" + Retry
// in the empty state, normal dropdown in the success state).
//
// All option strings are owned by the callback (typically a small static
// buffer). The renderer never frees them and must finish reading before
// the next callback invocation — which is fine because rendering is
// fully synchronous.

typedef enum {
	WUI_POPULATE_LOADING = 0,   // populate is in progress (sync APIs skip this)
	WUI_POPULATE_EMPTY   = 1,   // callback succeeded but found zero options
	WUI_POPULATE_ERROR   = 2,   // callback failed (e.g. driver enumeration failed)
	WUI_POPULATE_SUCCESS = 3,   // callback returned ≥1 option
	WUI_POPULATE_PARTIAL = 4    // some options returned, some failed/marked
} wuiPopulateState_t;

typedef struct {
	int             state;          // wuiPopulateState_t
	int             count;          // number of valid entries in names[]/values[]
	const char    **names;          // option labels (callback-owned static storage)
	const char    **values;         // cvar values (often the same pointers as names)
} wuiPopulateResult_t;

typedef int (*wuiPopulateCallback_t)( wuiPopulateResult_t *out );

void                     WiredUI_RegisterPopulateCallback( const char *name,
                                                            wuiPopulateCallback_t fn );
wuiPopulateCallback_t    WiredUI_GetPopulateCallback( const char *name );
void                     WiredUI_RegisterCorePopulateCallbacks( void );

// ── cgame batch registration ──────────────────────────────────────────
//
// Called once from CG_Init to register all core symbols and elements.
// After this call, the client can safely parse .hud files.

void     WiredUI_RegisterCoreSymbols( void );     // batch: health, armor, ammo, etc.
void     WiredUI_RegisterCoreElements( void );    // batch: all 167 SuperHUD element types

// ── types ─────────────────────────────────────────────────────────────

typedef enum {
	ANCHOR_NONE = 0,        // use absolute/normalized position as-is
	ANCHOR_TOP_LEFT,
	ANCHOR_TOP_CENTER,
	ANCHOR_TOP_RIGHT,
	ANCHOR_CENTER_LEFT,
	ANCHOR_CENTER,
	ANCHOR_CENTER_RIGHT,
	ANCHOR_BOTTOM_LEFT,
	ANCHOR_BOTTOM_CENTER,
	ANCHOR_BOTTOM_RIGHT
} wiredAnchor_t;

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

/* ── TABLE widget column definition (Phase 4) ──────────────────── */

#define WUI_TABLE_MAX_COLUMNS   16

typedef struct {
	char        field[64];          /* store key suffix for cell text (e.g. "name", "score") */
	char        header[64];         /* column header text (e.g. "PLAYER", "SCORE") */
	float       width;              /* column width as fraction of table width (0.0-1.0) */
	int         align;              /* 0=left, 1=center, 2=right */
	char        colorfield[64];     /* store key suffix for per-cell color override */
	char        iconfield[64];      /* store key suffix for per-cell icon */
} wuiTableColumn_t;

typedef struct wiredItemDef_s {
	char            name[64];
	char            text[256];
	char            group[64];
	int             type;                   // ITEM_TYPE_*
	wiredRect_t     rect;
	wuiRect_t       wuiRect;                // unit-aware source rect (Layer 1)
	wuiPosition_t   position;               // POSITION_STATIC / ABSOLUTE / VIEWPORT
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

	// Wired native format extensions (.wmenu/.whud)
	wiredAnchor_t   anchor;                 // screen anchor position
	float           textoffsetX;            // normalized text offset X (replaces textalignx)
	float           textoffsetY;            // normalized text offset Y (replaces textaligny)
	float           fontPointSize;          // font size in points (native format "font" keyword)
	int             fontWeight;             // requested weight (fontweight)
	float           letterSpacing;          // extra pixels between glyphs ("letterspacing")

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

	// Legacy ITEM_TYPE_MODEL support (Phase 2 rendering parity)
	char            assetModel[MAX_QPATH];
	char            assetShader[MAX_QPATH];
	vec3_t          modelOrigin;
	float           modelFovX;
	float           modelFovY;
	float           modelRotation;
	float           modelAngle;
	int             modelWidescreen;
	qhandle_t       modelHandle;
	qhandle_t       modelShaderHandle;

	/* Wired Store data bindings (Phase 4) */
	char            storeBind[128];         /* store key for text override (e.g. "player.health.text") */
	char            storeBindColor[128];    /* store key for color override */
	char            storeBindIcon[128];     /* store key for icon override */
	char            storeBindValue[128];    /* store key for numeric value */
	char            showBind[128];          /* show item when store key is truthy */
	char            hideBind[128];          /* hide item when store key is truthy */
	qboolean        bindWarned;             /* dev-mode: already warned about missing binding */

	/* ── TABLE widget properties (Phase 4) ────────────────────── */
	char            tableSource[128];       /* store key prefix for row data (e.g. "game.scores") */
	char            tableCountBind[128];    /* store key for row count (e.g. "game.scores.count") */
	int             tableTeamFilter;        /* -1=all, 0=none, 1=red side, 2=blue side */
	wuiTableColumn_t tableColumns[WUI_TABLE_MAX_COLUMNS];
	int             numTableColumns;

	// cvar binding data
	wiredMultiDef_t *multiData;             // for ITEM_TYPE_MULTI (allocated from pool)
	wiredSliderDef_t sliderData;            // for ITEM_TYPE_SLIDER (cvarFloat min/max)
	int             maxChars;               // for ITEM_TYPE_EDITFIELD
	int             maxPaintChars;          // visible chars in edit field

	// Dynamic MULTI: when populateCallback is set on an ITEM_TYPE_MULTI, the
	// option list is filled at render time by the named callback (registered
	// via WiredUI_RegisterPopulateCallback) instead of via cvarFloatList /
	// cvarStrList. Empty string = static MULTI (legacy behaviour).
	char            populateCallback[64];

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

	// fade animation state (TA compat: fadein/fadeout script commands)
	float           fadeAlphaItem;          // current item alpha multiplier (0.0 = invisible, 1.0 = opaque)
	float           fadeTargetAlpha;        // target alpha (0.0 for fadeout, 1.0 for fadein)
	int             fadeStartTime;          // cls.realtime when fade started (0 = inactive)
	int             fadeDurationItem;       // fade duration in ms

	// Flex child properties (Layer 2)
	wuiFlexChild_t      flexChild;
	wuiAspect_t         aspect;

	// If this item is a flex container (has "layout" keyword)
	wuiFlexContainer_t  flexContainer;
	qboolean            isFlexContainer;

	// Nested children (for flex containers)
	struct wiredItemDef_s  *children[WIRED_MAX_ITEMS_PER_MENU];
	int                     childCount;

	// Animated rect transition (Layer 5)
	wuiTransition_t     wuiTransition;

	// Responsive breakpoints (Layer 5)
	wuiBreakpoint_t     breakpoints[WUI_MAX_BREAKPOINTS];
	int                 breakpointCount;

	// Resolved pixel rect (filled by layout engine)
	wuiPixelRect_t  resolvedRect;
} wiredItemDef_t;

typedef struct wiredMenuDef_s {
	char              name[64];
	wiredRect_t       rect;
	wuiRect_t         wuiRect;              // unit-aware source rect (Layer 1)

	// Flex container properties (Layer 2)
	wuiFlexContainer_t  flexContainer;
	qboolean            isFlexContainer;

	// Resolved pixel rect (filled by layout engine)
	wuiPixelRect_t      resolvedRect;

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
	wiredAnchor_t     anchor;               // menu-level anchor
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

	// cinematic background (WINDOW_STYLE_CINEMATIC / TA compat)
	char              cinematic[MAX_QPATH]; // ROQ file path (parsed from "cinematic" keyword)
	int               cinematicHandle;      // CIN handle (-1 = none)
} wiredMenuDef_t;

// ── overlay rendering (for in-game overlays like scoreboard) ──────────
void     WiredUI_RenderMenuOverlay( wiredMenuDef_t *menu, int realtime );

// ── UI state access (Wired Store-backed for ui_* keys) ───────────────
qboolean WiredUI_IsStoreStateKey( const char *key );
void     WiredUI_StateGetString( const char *key, char *out, int outSize );
int      WiredUI_StateGetInt( const char *key );
float    WiredUI_StateGetFloat( const char *key );
void     WiredUI_StateSetString( const char *key, const char *value );
void     WiredUI_StateSetInt( const char *key, int value );
void     WiredUI_StateSetFloat( const char *key, float value );
void     WiredUI_SaveState( void );
void     WiredUI_LoadState( void );

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

// ── error dialog ─────────────────────────────────────────────────────
//
// Show the error_popup.wmenu modal.  Writes ui_errorTitle and ui_errorRetry
// state keys, then pushes the menu.  com_errorMessage must be set by the
// caller before this is invoked (it is the dialog's text source).
//
// retryable: if qfalse or the last connect target was localhost, the Retry
// button is hidden (ui_errorRetry = "0").
//
// Safe to call with cls.uiStarted == qfalse — falls back to Com_Printf.

void     CL_WiredUI_ShowError( const char *title,
                                const char *message,
                                qboolean    retryable );

// ── asset globals (assetGlobalDef from .menu files) ──────────────────
//
// These values are set by parsing the assetGlobalDef {} block in menus.txt.
// They provide global defaults for cursor, fonts, colors, and sounds.
// If not specified, sensible defaults are used (TA-style).

typedef struct {
	char            cursor[MAX_QPATH];         // cursor shader path
	char            gradientBar[MAX_QPATH];    // gradient bar shader path
	float           fadeClamp;                 // max fade alpha (default 1.0)
	int             fadeCycle;                 // fade cycle time ms (default 1)
	float           fadeAmount;                // fade step per cycle (default 0.2)
	vec4_t          shadowColor;               // text shadow color
	char            focusSound[MAX_QPATH];     // item focus sound path
	vec4_t          focusColor;                // focus highlight color

	// Shadow offset
	float           shadowX;                   // horizontal shadow offset (default 1.0)
	float           shadowY;                   // vertical shadow offset (default 1.0)

	// Visual effects
	vec4_t          gradientBarColor;          // gradient bar decoration color
	char            radialGlowShader[MAX_QPATH]; // shader name for radial glow effect

	// Theme font slots (MSDF only)
	char            defaultSerifFontName[MAX_QPATH];        // FONT_DISPLAY / FONT_DISPLAY_BOLD
	char            defaultSerifFontItalicName[MAX_QPATH];  // FONT_DISPLAY_ITALIC
	char            defaultSansFontName[MAX_QPATH];         // FONT_UI
	char            defaultSansFontMediumName[MAX_QPATH];   // FONT_UI_MEDIUM
	char            defaultMonoFontName[MAX_QPATH];         // FONT_MONO
} wiredAssetGlobals_t;

wiredAssetGlobals_t *WiredUI_GetAssetGlobals( void );
void WiredUI_ResetAssetGlobalsDefaults( void );

// ── parser (cl_wired_parse.c) ─────────────────────────────────────────

qboolean WiredUI_LoadMenuFile( const char *filename );
int      WiredUI_GetMenuCount( void );
wiredMenuDef_t *WiredUI_GetMenuByIndex( int index );
wiredMenuDef_t *WiredUI_FindMenu( const char *name );
void     WiredUI_ClearMenus( void );
void     WiredUI_ResetPool( void );
qboolean WiredUI_SafeReload( void );   // two-phase: exec menus.lua → swap or keep old
// Single pre-PostInit Lua binding registration point (call from CL_Init).
// Registers load_menu() and attract.* globals so they are live when
// WiredUI_Init and WiredAttract_Init exec their Lua files.
void     WiredUI_LuaInit( void );
// (WiredUI-internal — called only by WiredUI_LuaInit, defined in cl_wired_parse.c)
void     WiredUI_MenuLuaInit( void );
// Execute scripts/menus.lua to populate the menu pool.
void     WiredUI_LoadMenusFromLua( void );

// ── ownerdraw system (cl_wired_ownerdraw.c) ──────────────────────────

qboolean WiredUI_OwnerDrawVisible( int flags );  // evaluate CG_SHOW_*/UI_SHOW_* flags
void     WiredUI_OwnerDraw( int ownerDraw, float x, float y, float w, float h,
                             vec4_t color, int style );

// ── memory pool ───────────────────────────────────────────────────────

#define WIRED_MENU_POOL_SIZE   (16 * 1024 * 1024) // 16MB for menus
#define WIRED_HUD_POOL_SIZE    (512 * 1024)        // 512KB for HUD overlays

// ── dispatch macros ───────────────────────────────────────────────────
//
// These replace VM_Call(uivm, ...) throughout the client code.
// Wired UI is now the only path — no VM fallback.

#define UI_VM_ACTIVE            (1)  // always active — no VM to check
#define UI_CALL_REFRESH(rt)     WiredUI_Refresh(rt)
#define UI_CALL_KEY_EVENT(k,d)  WiredUI_KeyEvent(k,d)
#define UI_CALL_MOUSE_EVENT(x,y) WiredUI_MouseEvent(x,y)
#define UI_CALL_SET_ACTIVE(m)   WiredUI_SetActiveMenu(m)
#define UI_CALL_IS_FULLSCREEN() WiredUI_IsFullscreen()
#define UI_CALL_CONNECT(o)      WiredUI_DrawConnectScreen(o)

#endif // FEAT_WIRED_UI
#endif // CL_WIRED_UI_H
