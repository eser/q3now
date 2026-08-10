// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
cl_wired_ui.h — Wired UI: unified menu/HUD system (client-side)
*/

#ifndef CL_WIRED_UI_H
#define CL_WIRED_UI_H

#include "../../../qcommon/q_feats.h"

#if FEAT_WIRED_UI

#include "../../../qcommon/q_shared.h"
#include "../../../qcommon/wired/core/scripting/wired_scripting.h"
#include "cl_wired_layout.h"
#include "cl_wired_fonts.h"
#include "cl_wired_bg.h"        /* wuiBgIntent_t (WiredUI_PushMenu background intent) */

// ── public API (called from cl_ui.c, cl_keys.c, cl_scrn.c, etc.) ─────

/* V-25/V-26 (2026-05-25): WiredUI_Init returns qboolean now. qtrue on
 * success, qfalse if a non-recoverable subsystem (Clay arena alloc,
 * essential cvar/registry setup) failed. The boot path branches on the
 * `wui_required` cvar: when 1 (default UI mode) a qfalse return is
 * fatal (Sys_Error); when 0 (default dedicated mode) the engine
 * continues headless with SEV_WARN. WiredUI_RenderFrame's
 * `!wui_clay_initialized` early-return keeps the frame loop a no-op when
 * init failed and wui_required was 0. */
qboolean WiredUI_Init( qboolean inGameUI );
void     WiredUI_Shutdown( void );

/* V-15 (2026-05-25): WiredUI_Refresh renamed → WiredUI_TickFrame.
 * Per-frame non-render housekeeping (hot-reload + attract + ui_testall +
 * server-browser pings + HUD state sync + animation tick). Called from
 * WiredUI_RenderFrame BEFORE the compositor walk so the walk sees a fully
 * up-to-date state snapshot. */
void     WiredUI_TickFrame ( int realtime );

/* V-15 (2026-05-25): single dispatch authority. Replaces the legacy
 * SCR_DrawScreenField 5-dispatch (Con_DrawConsole / WiredHud_Routine /
 * CompositorEmitFrame / WiredUI_Refresh / SCR_Draw* helpers) with one
 * compositor-driven walk. Called from cl_scrn.c::SCR_DrawScreenField,
 * which now collapses to re.BeginFrame + this. */
void     WiredUI_RenderFrame( void );
void     WiredUI_KeyEvent( int key, qboolean down );
void     WiredUI_MouseEvent( float dx, float dy );
void     WiredUI_SetActiveMenu( int menu );       // UIMENU_NONE, UIMENU_MAIN, UIMENU_INGAME
qboolean WiredUI_IsFullscreen( void );
/* C-15: WiredUI_DrawConnectScreen retired — compositor sole path. */

// ── health / recovery ─────────────────────────────────────────────────
// See cl_wired_ui.c for the three-layer model and recovery semantics.

int      WiredUI_GetMenuStackDepth( void );       // 0 = nothing on stack

/* ── source-attribution context ───────────────────────────────────────
 * Parse-context: filled by cl_wired_parse.c while a .wmenu / .whud is
 * being parsed. Read by CL_ForwardCommandToServer to annotate "Unknown
 * command" log lines (parser keyword tokens that fall through to the
 * console buffer). NULL outside any parse — production logs unchanged.
 *
 * Emit-context: filled by cl_wired_clay.c while iterating visible
 * panels. Read by wui_clay_error_handler so Clay layout errors blame
 * the actual source menu/item instead of the anonymous registry. NULL
 * outside any emit pass — production logs unchanged.
 *
 * Both are single-threaded engine-state singletons. Cleared on exit
 * from the enclosing scope so a later un-annotated emission can't
 * pick up stale context from a prior parse/emit. */
extern const char *WiredUI_ParseContextFile  ( void );
extern int         WiredUI_ParseContextLine  ( void );
extern const char *WiredUI_ParseContextMenu  ( void );
extern const char *WiredUI_ParseContextItem  ( void );
extern const char *WiredUI_EmitContextMenu   ( void );
extern const char *WiredUI_EmitContextItem   ( void );
extern int         WiredUI_EmitContextLayer  ( void );
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
// HUD elements are the ModernHUD element types (fps, weaponlist, etc.)
// that .hud files reference via the hudElement keyword.
//
// Example:
//   WiredUI_RegisterElement("fps", &CG_ModernHUDElementFPSCreate,
//                                   &CG_ModernHUDElementFPSRoutine,
//                                   &CG_ModernHUDElementFPSDestroy);
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
void     WiredUI_RegisterCoreElements( void );    // batch: all 167 ModernHUD element types

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

/* ── 6-layer modality model ──────────────────────────
 * Per docs/wiredui-clay-design.md §10.1. Panel layer ordering:
 *   0 BG_ATTRACT     — attract-mode background (idle splash)
 *   1 LOADING        — map loading screen (wires .wmenu migration)
 *   2 HUD            — in-game HUD elements (active during CA_ACTIVE no-menu)
 *   3 MENU_STACK     — modal menu stack (top of stack receives input)
 *   4 POPUP          — confirmation / error dialogs (blocks MENU_STACK input)
 *   5 DEBUG_OVERLAY  — dev-only overlays (gated by wired_ui_debug; non-modal)
 *
 * Default for `menuDef` without explicit `layer` keyword is MENU_STACK
 * (backward-compatible with all earlier panels). This generalizes the
 * older WiredUI_GetActiveMenu single-panel narrow to a per-layer walk
 * with activation rules per cls.state. */
typedef enum {
	WUI_LAYER_BG_ATTRACT      = 0,
	WUI_LAYER_LOADING         = 1,
	/* 3D world viewport — apps register render callbacks via
	 * WiredUI_RegisterViewportProvider; the compositor's emit walk
	 * invokes them from within this layer. Painter's
	 * order: world geometry under HUD + menus + console. */
	WUI_LAYER_WORLD_VIEWPORT  = 2,
	WUI_LAYER_HUD             = 3,
	/* Modal menu stack. Renamed from WUI_LAYER_MENU_STACK (the _STACK
	 * suffix is retired — there's only one menu semantic). */
	WUI_LAYER_MENU            = 4,
	WUI_LAYER_POPUP           = 5,
	WUI_LAYER_DEBUG_OVERLAY   = 6,
	/* Cursor sprite + hover tooltip + transient floating overlays
	 * (multi-dropdown). Gated on Key_GetCatcher() & KEYCATCH_UI. */
	WUI_LAYER_OVERLAY         = 7,
	/* Console panel — KEYCATCH_CONSOLE or con_immediate. Highest Z,
	 * draws above debug overlays + cursor + tooltip. */
	WUI_LAYER_CONSOLE         = 8,
	WUI_LAYER_COUNT
} wuiLayer_t;

/* Default font size used as a fallback when wiredItemDef_t.fontPointSize <= 0.
 * Shared between the legacy SCR text path (cl_wired_ui.c) and the compositor
 * emit path (cl_wired_clay.c) so the same fallback applies in both routes. */
#define WUI_DEFAULT_FONT_SIZE  14.0f
/* Minimum single-line row height as a multiple of font size. The MSDF draw path
 * top-aligns glyphs with zero leading, so a fixed-height row shorter than the
 * glyph line box (~1.0-1.32 em across shipped faces) lets the next row overlap
 * the current row's descenders. 1.4 clears every face's natural lineHeight plus
 * a little leading, so single-line rows breathe without per-face branching. */
#define WUI_LINE_HEIGHT_FACTOR 1.4f

// multi-choice data (for ITEM_TYPE_MULTI)
typedef struct {
	char    labels[WIRED_MAX_MULTI_CHOICES][64];   // display labels
	char    strValues[WIRED_MAX_MULTI_CHOICES][64]; // string values (for cvarStrList)
	float   floatValues[WIRED_MAX_MULTI_CHOICES];   // float values (for cvarFloatList)
	int     count;
	qboolean isStringList;                          // qtrue = cvarStrList, qfalse = cvarFloatList
} wiredMultiDef_t;

// slider data (for ITEM_TYPE_SLIDER and ITEM_TYPE_SPINNER)
typedef struct {
	float   defVal;
	float   minVal;
	float   maxVal;
	float   step;   // SPINNER: per-tick increment (0 => derived range/20 fallback)
} wiredSliderDef_t;

/* Snapshot of the transient multi-dropdown overlay panel needed by the
 * compositor's Clay emit. Filled by WiredUI_QueryMultiDropdownRender from
 * file-static state in cl_wired_ui.c — only the .open=qtrue case has
 * meaningful geometry/data fields. Allows the renderer to stay agnostic
 * of the option-source plumbing (cvarFloatList / cvarStrList / populate
 * callback). */
typedef struct {
	qboolean      open;
	float         x, y, w, h;
	float         rowH;
	int           visibleRows;
	int           optionCount;
	const char   *labels[WIRED_MAX_MULTI_CHOICES];
	int           hoverRow;     /* -1 when no hover */
	int           selectedRow;  /* current cvar value index, -1 when no match */
	int           scrollOffset;
} wuiMultiDropdownRender_t;

/* ── TABLE widget column definition ──────────────────── */

#define WUI_TABLE_MAX_COLUMNS   16

typedef struct {
	char        field[64];          /* store key suffix for cell text (e.g. "name", "score") */
	char        header[64];         /* column header text (e.g. "PLAYER", "SCORE") */
	float       width;              /* column width as fraction of table width (0.0-1.0) */
	int         align;              /* 0=left, 1=center, 2=right */
	char        colorfield[64];     /* store key suffix for per-cell color override */
	char        iconfield[64];      /* store key suffix for per-cell icon */
} wuiTableColumn_t;

/* ── .wmenu format additions ──────────────────
 *
 * wiredRepeatBlock_t: backs a `repeat { source / countbind / as / itemDef }`
 * block. The template subtree is parsed once into a sub-itemDef; per-frame
 * expansion clones it N times into the compositor scratch arena, substituting
 * `{{ row }}` / `{{ row.<field> }}` placeholders in text strings.
 *
 * Source can be a literal store-prefix path (existing TABLE convention,
 * resolved via WiredStore_Get) or a Lua expression with "lua:" prefix
 * (compiled once at parse time via WiredScript_CompileChunk; chunk ref
 * cached on this struct). Lua results are constrained to dense 1..N
 * tables (validated at expansion time).
 */
struct wiredItemDef_s;

typedef struct {
	char        source[256];        /* store prefix path OR "lua:..." literal */
	qboolean    sourceIsLua;        /* parse-time detected from "lua:" prefix */
	int         sourceLuaChunk;     /* WIRED_CHUNK_NOREF or chunk ref */
	char        countBind[128];     /* optional store key for explicit count (store source only) */
	char        asName[64];         /* iteration variable name (required, default "row") */

	struct wiredItemDef_s *templateItem;  /* parsed template subtree (singleton) */

	qboolean    sourceWarned;       /* once-only Lua source error log gate */
} wiredRepeatBlock_t;

/* wiredIfBlock_t: backs an `if { test "lua:..." / itemDef ... }` block.
 * The per-frame test eval + child emission is wired separately. Inline child
 * array sized for typical use (≤8 conditional items per block); deeper
 * conditions can nest if-blocks. */
#define WIRED_MAX_IF_CHILDREN  8

typedef struct {
	char        testExpr[256];       /* lua: expression literal */
	int         testLuaChunk;        /* WIRED_CHUNK_NOREF until compiled */
	qboolean    testWarned;
	struct wiredItemDef_s *children[ WIRED_MAX_IF_CHILDREN ];
	int         childCount;
} wiredIfBlock_t;

/* Compositing space for a widget's fill/background.
 *
 *   INHERIT — take the enclosing panel's mode. Zero so a memset-cleared item
 *   defaults to inheriting; the panel resolves it to a concrete mode at emit.
 *   A panel that is itself INHERIT resolves to DIEGETIC (the world default).
 *
 *   DIEGETIC — blended into the scene's linear HDR buffer before the gamma
 *   present pass. Alpha reads against world-lit pixels, so the widget feels
 *   like part of the rendered world (a heads-up image seen through the
 *   player's visor). This is the world default and what the lighting acts
 *   on. The fill rgb is linearised before compositing.
 *
 *   OVERLAY — drawn after the gamma present pass, directly in display/sRGB
 *   space on the swapchain image. Alpha reads perceptually and is independent
 *   of how bright or dark the scene behind it is, so the widget stays legible
 *   under any lighting. The fill rgb is kept perceptual (not linearised).
 */
typedef enum {
	WUI_COMPOSITE_INHERIT  = 0,
	WUI_COMPOSITE_DIEGETIC = 1,
	WUI_COMPOSITE_OVERLAY  = 2
} wuiCompositeMode_t;

/* Optional per-item interaction-state colours (component-library F1). Pointed to
 * by wiredItemDef_t.stateColors, allocated on demand only when a state colour
 * keyword is authored. Each has* flag gates whether the variant was set; unset
 * variants fall back through the resolver chain (pressed→focused→hover→base). */
typedef struct wuiStateColors_s {
	vec4_t   hoverFg,    hoverBg,    hoverBorder;
	vec4_t   pressedFg,  pressedBg,  pressedBorder;
	vec4_t   focusedFg,  focusedBg,  focusedBorder;
	vec4_t   disabledFg, disabledBg, disabledBorder;
	qboolean hasHover;
	qboolean hasPressed;
	qboolean hasFocused;
	qboolean hasDisabled;
} wuiStateColors_t;

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
	char            hudElement[64];

	// v6 additions
	vec4_t          outlinecolor;
	float           special;                // ownerdraw spacing parameter
	int             align;                  // HUD_VERTICAL(0) / HUD_HORIZONTAL(1)
	qboolean        notselectable;
	/* WiredUI F4 (modal/dialog behaviour): item authored as the dialog's
	 * DEFAULT control. On menu open, WiredUI_SetInitialFocus routes initial
	 * keyboard focus here (so Enter activates it without a prior Tab), and the
	 * menu-level Enter handler falls back to firing this item's action[] when
	 * the currently-focused item has no Enter behaviour of its own. Zero-init
	 * (memset) => not a default button — every existing item keeps today's
	 * behaviour. Keyword: `defaultButton` (bare flag). */
	qboolean        defaultButton;

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

	// ModernHUD-specific properties (hudElement items)
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
	char            image[MAX_QPATH];       // image/shader name (ModernHUD "image" keyword)
	char            bind[32];               // data binding name ("health", "armor", "ammo")

	// Legacy ITEM_TYPE_MODEL support (rendering parity)
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

	/* Wired Store data bindings */
	char            storeBind[128];         /* store key for text override (e.g. "player.health.text") */
	char            storeBindColor[128];    /* store key for color override */
	char            storeBindIcon[128];     /* store key for icon override */
	char            storeBindValue[128];    /* store key for numeric value */
	char            showBind[128];          /* show item when store key is truthy */
	char            hideBind[128];          /* hide item when store key is truthy */
	qboolean        bindWarned;             /* dev-mode: already warned about missing binding */

	/* ── TABLE widget properties ────────────────────── */
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
	// Engine-drawn multi-column header band (§7.1 list-view). When
	// columnHeaderCount > 0 the listbox paints its own header row above the
	// body rows using the SAME per-column geometry (wui_listbox_column_geom),
	// so header cells and body cells are guaranteed to share one column model.
	// A header cell click fires "<columnSortCmd> <col>" (e.g. "MapSort 2").
	char            columnHeaders[8][32];   // per-column header labels
	int             columnHeaderCount;      // number of authored header labels (0 = no header band)
	char            columnSortCmd[32];      // sort script command fired on header click (empty = headers not clickable)
	int             listScrollOffset;       // first visible row (vertical) or first visible column (horizontal)
	int             listSelectedRow;        // selected row index (-1 = none)
	int             listScrollFadeTime;     // cls.realtime when last scrolled (for scrollbar fade)
	int             elementtype;            // LISTBOX_TEXT or LISTBOX_IMAGE — picks how each row is drawn
	qboolean        horizontalScroll;       // axis-flip: items flow left-to-right instead of top-to-bottom

	// transition animation state
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

	// CSS-style per-side anchor + margin (parser keywords
	// top/left/right/bottom/margin/marginTop/...). Auto-promotes to
	// POSITION_ABSOLUTE in Clay-emit when wuiOffset has any side declared.
	wuiOffset_t         wuiOffset;
	wuiMargin_t         wuiMargin;

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

	/* ── repeat block ────────────────────────────────
	 * When non-NULL the item is a CONTAINER whose children are produced by
	 * per-frame expansion of repeatBlock->templateItem. Original
	 * children[] / childCount remain authoritative for static items;
	 * repeat-expansion happens on top in the compositor scratch arena. */
	wiredRepeatBlock_t *repeatBlock;

	/* ── if block — parser recognition only here;
	 * per-frame test evaluation lands separately. */
	wiredIfBlock_t     *ifBlock;

	/* ── Lua chunk refs ───────────────────────
	 * Compiled at parse time when bind/visible carry the "lua:" prefix.
	 * The per-frame eval is wired separately; here we only store the ref. */
	int                 luaBindChunk;
	int                 luaVisibleChunk;

	/* ── unified custom-draw registry plumbing ─────────
	 * customDrawName carries the sigil-prefixed registry key the parser
	 * wrote from `ownerdraw "x"` → "od:x", `hudElement "x"` → "hud:x",
	 * `custom "x"` → "custom:x". Empty (NUL-first-byte) means the item
	 * has no custom-draw — dispatch skipped.
	 *
	 * customDrawContext caches the per-item stateful context returned by
	 * the registry's create() callback. Allocated from s_hudArena at item
	 * bind time, reclaimed via Arena_Reset on menu teardown. Stateless
	 * entries leave this NULL. */
	char                customDrawName[ 64 ];
	void               *customDrawContext;

	/* ── declarative animated rect width via store ─────
	 * When non-empty, the compositor emit scales item->resolvedRect.w by
	 * the [0..1]-clamped float read from the named store key. Authors
	 * write `rect <x> <y> <fullWidth> <h> ... bindwidth "store_key"` in
	 * .wmenu; reading the key at emit time yields the animated width.
	 * Pure-static rects leave this empty — emit uses the original width. */
	char                storeBindWidth[ 128 ];

	/* viewport-provider registry id. Populated by the
	 * parser when itemDef declares `type viewport id "<id>"`. Compositor's
	 * WUI_LAYER_WORLD_VIEWPORT walk resolves this via
	 * WiredUI_FindViewportProvider() and invokes provider->render(). Empty
	 * on non-viewport items. */
	char                viewportId[ 64 ];

	/* WUI Flexbox Authoring Migration — visual
	 * keywords. `radius v` sets the uniform value; `radius4 tl tr br bl`
	 * populates the per-corner array (uniform sets all four to `radius`).
	 * Spec order is clockwise from TL: TL, TR, BR, BL. Clay's
	 * Clay_CornerRadius struct is `{ topLeft, topRight, bottomLeft,
	 * bottomRight }` — emit remaps. */
	float               cornerRadius;          /* uniform fallback */
	float               cornerRadius4[ 4 ];    /* TL, TR, BR, BL (spec order) */

	/* 6-layered background extension. Parser
	 * sets bgLayerFlags via `background "layered" effects "<flags>"`
	 * keyword; compositor emit dispatches to WUI_DrawBackgroundLayered
	 * if non-zero. Zero = legacy single-color/shader path. */
	int                 bgLayerFlags;
	/* compositing space for this widget's fill.
	 * Parser sets it from the `composite diegetic|overlay` keyword;
	 * zero-init (memset in WiredUI_Alloc) defaults to DIEGETIC. The
	 * compositor tags the emitted rectangle so the dispatch can route
	 * overlay fills to the post-gamma display-space draw path. */
	wuiCompositeMode_t  compositeMode;
	/* per-side border widths so borderX / borderY
	 * can target left+right vs top+bottom separately. Order matches
	 * Clay_BorderWidth: { left, right, top, bottom }. Zero on a side
	 * means "no border on that side". `border` keyword sets all four
	 * uniformly to `bordersize`; `borderX` / `borderY` populate the
	 * relevant pair only. */
	float               bordersize4[ 4 ];      /* L, R, T, B (Clay order) */

	/* wuiAnim binding. The parser stores the
	 * animation name (built-in or future Lua-registered). The compositor
	 * lazily creates a wuiAnim_t on first emit, latches the id, and
	 * reads the per-frame eased value out of the corresponding scalar
	 * field below. `animOffsetX/Y` are pixel-space additive offsets
	 * applied during Clay emit; `animAlphaMul` is a [0..1] multiplier
	 * on emit color alpha. */
	char                animationName[ 64 ];
	char                animationCurve[ 32 ];   /* override built-in curve name */
	int                 animationDurationMs;    /* override built-in duration */
	qboolean            animationLoop;          /* override built-in loop flag */
	int                 animationSpeed;         /* `speed v` form alias for duration */
	int                 animationId;            /* runtime: WUI_AnimCreate result */
	float               animOffsetX;            /* compositor: scroll-x output */
	float               animOffsetY;            /* compositor: slide-up/down output */
	float               animAlphaMul;           /* compositor: fade-in/out output */

	/* active state binding. `active <cvar> <value>` records
	 * the cvar/value pair; per-frame the compositor compares the cvar's
	 * string against `value` and routes per-property emit through the
	 * `.active` variants when matched. The `hasActive*` flags gate which
	 * properties have authored overrides so emit can skip the comparison
	 * for items that don't use the feature. */
	char                activeCvar[ 64 ];
	char                activeValue[ 64 ];
	vec4_t              forecolorActive;
	vec4_t              backcolorActive;
	vec4_t              bordercolorActive;
	float               fontPointSizeActive;
	float               cornerRadiusActive;
	qboolean            hasActiveForecolor;
	qboolean            hasActiveBackcolor;
	qboolean            hasActiveBordercolor;
	qboolean            hasActiveFontSize;
	qboolean            hasActiveCornerRadius;

	/* ── component-library framework (F1) ────────────────────────────────
	 * enableMode: how a disabled (enable/disableCvar-failed) item renders.
	 *   qfalse (default, keyword `enableMode hide` or unset) — LEGACY: the
	 *          visibility path culls it exactly as before. Every existing menu
	 *          keeps today's behaviour with ZERO init (memset-0 => hide).
	 *   qtrue  (keyword `enableMode dim`) — the new component path: the item is
	 *          rendered greyed + non-interactive (dropped from focus traversal
	 *          and mouse-accept, greyed by the state resolver).
	 * Stored as `dim` (not `hide`) so the memset-0 default is the legacy path —
	 * every alloc site inherits backward-compat without an explicit init. */
	qboolean            enableModeDim;

	/* Optional per-item interaction-state colours. NULL unless a hover/pressed/
	 * disabled/focused colour keyword was authored — allocated on demand from the
	 * menu pool by the parser, so existing items cost zero extra bytes. */
	struct wuiStateColors_s *stateColors;
} wiredItemDef_t;

typedef struct wiredMenuDef_s {
	char              name[64];
	// Relative source path+ext the menu was loaded from (e.g.
	// "ui/loading_screen.wui"). The menu's path-identity, distinct from the
	// short `name` keyword. Stamped at parse time from the WiredUI_LoadMenuFile
	// filename; used by WiredUI_FindMenuByPath for state→named-UI binding
	// (the LOADING layer's by-identity emit). Empty for menus parsed outside
	// a file load.
	char              sourcePath[MAX_QPATH];
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
	wuiCompositeMode_t compositeMode;       // panel-fill compositing space; items inherit this default
	qboolean          hudOverlay;           // passive HUD overlay
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

	/* ── panel-level VM selection ─────────────
	 * Set by parsing `vm "system"` or `vm "user"` on the menuDef. Empty
	 * string = infer from loader (engine-side WiredUI_LoadMenuFile →
	 * System; cgame-side trap-loaded → User). The compiled
	 * Lua chunks on this menu's items resolve against this VM at
	 * compile + invoke time; the parser refactor makes the
	 * compile-time dispatch effective. */
	char              vm[16];

	/* ── Path A / Path B classification ──────────────────────
	 * qtrue when the panel uses any wuiFlexContainer feature Clay v0.14
	 * does NOT natively support: wrap, shrink > 1.0, or justify
	 * space-between. Such panels stay on the CLAY_FLOATING +
	 * resolvedRect pinning strategy (Path B); the converter handles them
	 * via WUI_LayoutFlex's resolved rects. A later wired-
	 * side polyfill pre-resolution pass migrates them to native
	 * Clay declarations. */
	qboolean          pathBKind;

	/* ── layer assignment ───────────────────────────
	 * Set by parsing `layer "<name>"` on the menuDef. Defaults to
	 * WUI_LAYER_MENU when omitted (backward-compatible with all
	 * earlier panels). Drives the per-layer emit walk + modality
	 * input gating in WiredUI_CompositorEmitFrame. */
	wuiLayer_t        layer;
	/* parse-end check uses this to emit a SEV_WARN
	 * (escalating to SEV_ERROR) when a menuDef omits the layer
	 * keyword. Defaults qfalse; set qtrue when the `layer` keyword
	 * handler fires. */
	qboolean          layerSeen;
} wiredMenuDef_t;

/* WiredUI_RenderMenuOverlay retired — compositor sole path. */

// ── layer keyword parser helper ────────────────────
// Case-insensitive string → wuiLayer_t mapping. Returns WUI_LAYER_COUNT
// (sentinel) on unknown name; callers fall back to WUI_LAYER_MENU + SEV_WARN.
// Valid names: bg_attract | loading | world_viewport | hud | menu |
// menu_stack (legacy alias) | popup | debug_overlay | overlay | console.
wuiLayer_t WiredUI_ParseLayerName( const char *str );

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

// cvarTest/showCvar/hideCvar visibility: returns qfalse when the item's
// cvarTest value is excluded by showCvar (or matched by hideCvar). Exposed
// so the Clay compositor's emit walk can cull cvar-hidden items the same way
// the legacy render path does (WiredUI_ItemShouldRender). Read-only on item.
qboolean WiredUI_ItemVisibleByCvarRules( const wiredItemDef_t *item );

// Space-separated list membership test (matches the value literally and as an
// integer). Exposed so the framework core (cl_wired_widget_core.c) can share the
// exact enable/disable-cvar comparison the visibility path uses.
qboolean WiredUI_StateListContainsValue( const char *list, const char *value );

// ── label-and-value resolution ────────────────────────────────
// Shared between legacy SCR renderer (cl_wired_ui.c) and compositor
// (cl_wired_clay.c) during the transition. Resolves the right-side
// value text for a cvar-bound item per legacy semantics (YESNO/MULTI/
// BIND/EDITFIELD/NUMERICFIELD/SLIDER/default). Always writes into `out`;
// caller owns lifetime. Returns `out` for chain-style use.
const char *WiredUI_BoundValueText( const wiredItemDef_t *item, char *out, int outSize );
// [0..1] clamped slider fraction from item's cvar reading.
float       WiredUI_SliderFraction( const wiredItemDef_t *item );

// ── focus accessors ────────────────────────────────────────────
// During the transition, legacy retains focus authority (wui_focusItem)
// for mouse / arrow / setfocus paths; compositor's parallel
// wui_panel_state.focusedId only tracks Tab cycling. Compositor reads via
// this accessor for the focus-highlight gradient; flips to focusedId
// once mouse/arrow paths also propagate into the compositor state.
// Returns NULL if no active menu or no focusable item is currently focused.
const wiredItemDef_t *WiredUI_GetFocusedItem( void );

// Authoritative pointer-hover accessor (formalizes the direct wui_hoveredItemPtr
// read the tooltip path uses). Distinct from focus: mouse hover updates this,
// keyboard nav updates focus. Returns NULL when the cursor is over no hoverable
// item. Framework core reads this for the HOVER visual state.
const wiredItemDef_t *WiredUI_GetHoveredItem( void );

// Lazy accessor for the asset-globals gradient bar shader handle (loaded
// during WiredUI_Init from wui_assetGlobals.gradientBar). Compositor uses
// this for the focus highlight emit; returns 0 if the shader is unloaded.
qhandle_t            WiredUI_GradientBarShader( void );

// Snapshot the transient multi-dropdown overlay's geometry + option list
// for the compositor's Clay floating-panel emit. Caller-allocated struct,
// .open == qfalse means no overlay this frame (renderer should no-op).
// All string pointers in .labels[] are owned by the trigger item's
// option source; valid for the duration of the current frame.
void                 WiredUI_QueryMultiDropdownRender( wuiMultiDropdownRender_t *out );
wiredItemDef_t *     WiredUI_GetMultiDropdownItem( void );

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
typedef qhandle_t   (*wiredFeederItemIcon_t)( int feederID, int index );

#define WIRED_MAX_FEEDERS  32

// Register a feeder. The symbolic `name` (e.g. "characters", "skins") is what
// .wmenu files quote in `feeder "name"`; pass NULL/"" if the feeder is only
// addressable by numeric ID.
void     WiredUI_RegisterFeeder( int feederID, const char *name,
                                  wiredFeederCount_t count,
                                  wiredFeederItemText_t itemText,
                                  wiredFeederSelection_t selection );
// Optional: attach an icon callback to a feeder so listboxes with
// elementtype LISTBOX_IMAGE can render thumbnails per row. Call AFTER
// WiredUI_RegisterFeeder so the feeder slot already exists.
void     WiredUI_RegisterFeederIcon( int feederID, wiredFeederItemIcon_t icon );
// Look up a feeder ID from its registered name. Returns 0 if not found.
int      WiredUI_FeederIDByName( const char *name );
// Look up an ownerdraw ID from its registered script name. Returns 0 if not found.
int      WiredUI_OwnerDrawIDByName( const char *name );
int      WiredUI_FeederCount( int feederID );
const char *WiredUI_FeederItemText( int feederID, int index, int column );
qhandle_t WiredUI_FeederItemIcon( int feederID, int index );
void     WiredUI_FeederSelection( int feederID, int index );
/* Active sort column + direction for a feeder (cl_wired_feeders.c), used by
 * the engine-drawn listbox header band to draw the ^/v indicator. Returns
 * qtrue + fills *col/*dir (dir 0=asc,1=desc) for feeders with a known sort. */
qboolean WiredFeeder_ActiveSort( int feederID, int *col, int *dir );

// ── feeder data loading (cl_wired_feeders.c) ──────────────────────────

void     WiredUI_RegisterCoreFeeders( void );
void     WiredFeeder_LoadMaps( void );
void     WiredFeeder_LoadDemos( void );
void     WiredFeeder_LoadMods( void );

// ── menu stack ────────────────────────────────────────────────────────

#define WIRED_MENU_STACK_DEPTH  8

/* bgIntent decides how the pushed menu's background is emitted
 * (WUI_BG_INTENT_INHERIT reproduces today's authored-flags behavior). */
void     WiredUI_PushMenu( const char *name, wuiBgIntent_t bgIntent );
void     WiredUI_PopMenu( void );
void     WiredUI_CloseAllMenus( void );
wiredMenuDef_t *WiredUI_GetActiveMenu( void );

/* Resolve the background intent for the panel currently being emitted.
 * Returns the intent recorded at push time IF `panel` is the stack-top
 * menu; otherwise WUI_BG_INTENT_INHERIT (so popups / loading / multi-panel
 * layers keep their authored background — byte-identical). */
wuiBgIntent_t WiredUI_GetActiveBgIntent( const wiredMenuDef_t *panel );

/* Cursor position normalized to [-1..1] about the viewport centre (for the
 * background parallax layer's mouse response). Pass NULL for an axis to skip. */
void     WiredUI_GetCursorNorm( float *nx, float *ny );

// ── error dialog ─────────────────────────────────────────────────────
//
// Show the error_popup.wmenu modal.  Writes ui_errorTitle and ui_errorRetry
// state keys, then pushes the menu.  com_errorMessage must be set by the
// caller before this is invoked (it is the dialog's text source).
//
// retryable: if qfalse or the last connect target was localhost, the Retry
// button is hidden (ui_errorRetry = "0").
//
// Safe to call with cls.uiStarted == qfalse — falls back to Com_Log.

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
// Path-identity lookup: resolve a menu by its relative source path+ext
// (menu->sourcePath, e.g. "ui/loading_screen.wui"). Companion to the
// name-keyed WiredUI_FindMenu — used by the LOADING layer's state→UI binding.
wiredMenuDef_t *WiredUI_FindMenuByPath( const char *path );

// LOADING-layer state→named-UI binding. Connstate transitions set the
// relative path of the loading menu to emit (NULL/empty clears); the
// compositor's LOADING layer emits exactly that menu by path-identity.
void        WiredUI_SetLoadingMenu( const char *relPath );
const char *WiredUI_GetLoadingMenuPath( void );
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
// Load the system menus that live OUTSIDE menus.lua (loading_screen +
// overlay). Must run after every WiredUI_LoadMenusFromLua — both WiredUI_Init
// and WiredUI_SafeReload call it so these menus survive reloads (the LOADING
// by-path lookup needs them present in the registry).
void     WiredUI_LoadExplicitMenus( void );
// Read g_maprotation cvar into caller-supplied buffer.
void     WiredUI_GetMapRotation( char *buf, int size );

// ── ownerdraw system (cl_wired_ownerdraw.c) ──────────────────────────

qboolean WiredUI_OwnerDrawVisible( int flags );  // evaluate CG_SHOW_*/UI_SHOW_* flags
void     WiredUI_OwnerDraw( int ownerDraw, float x, float y, float w, float h,
                             vec4_t color, int style );

// ── memory pool ───────────────────────────────────────────────────────
//
// WiredUI_Alloc is a single bump-pointer arena (see cl_wired_parse.c).
// Capacity is per-process and shared by every menu the parser walks at
// boot. The 16 MB baseline ran out once the §6.1 settings-cluster menus
// landed (44-itemDef video.wui + 40-itemDef loading_screen + 8 other
// settings + scoreboard + popups exhausted the pool on the late-loading
// overlay.wui parse). 32 MB roughly doubles the headroom and unblocks
// the §6.8 video.wui LEFT-nav restore + the §8 HUD biggest scope. If
// the next-cluster authoring lands close to the new ceiling, consider
// per-subsystem arenas instead of another single-constant bump.

#define WIRED_MENU_POOL_SIZE   (32 * 1024 * 1024) // 32MB for menus
#define WIRED_HUD_POOL_SIZE    (512 * 1024)        // 512KB for HUD overlays

// Diagnostic: current bump-pointer position. Used by the debug-gated
// per-menu pool-head logger in WiredUI_Init + ParseMenu; reading the
// head outside `#ifdef _DEBUG` is fine but is currently only wired
// from debug builds.
int      WiredUI_GetPoolHead( void );
int      WiredUI_GetPoolCapacity( void );

// ── dispatch macros ───────────────────────────────────────────────────
//
// These replace VM_Call(uivm, ...) throughout the client code.
// Wired UI is now the only path — no VM fallback.

#define UI_VM_ACTIVE            (1)  // always active — no VM to check
#define UI_CALL_KEY_EVENT(k,d)  WiredUI_KeyEvent(k,d)
#define UI_CALL_MOUSE_EVENT(x,y) WiredUI_MouseEvent(x,y)
#define UI_CALL_SET_ACTIVE(m)   WiredUI_SetActiveMenu(m)
/* C-15: UI_CALL_REFRESH / UI_CALL_IS_FULLSCREEN / UI_CALL_CONNECT macros
 * retired — SCR_DrawScreenField thinned to compositor passthrough. */

#endif // FEAT_WIRED_UI
#endif // CL_WIRED_UI_H
