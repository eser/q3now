// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
cl_wired_clay.c — Clay layout engine integration for the WiredUI compositor.

This is the SINGLE TU that defines CLAY_IMPLEMENTATION. No other TU may
define it (Clay's single-header convention; double-definition would
duplicate-symbol at link time).

Initial integration scope:
  - Carve a 4 MB Clay arena slice from WiredUI_Arena (parent arena).
  - Call Clay_Initialize once at WiredUI_CompositorInit.
  - Bind Clay_SetMeasureTextFunction → wui_clay_measure_text, which routes
    through MSDF_MeasureStringBytes so the measure metric matches the eventual
    draw metric (cl_wired_msdf.c + cl_wired_text.c stays the sole text path).
  - Populate the font indirection table from the existing fontFace_t /
    msdfFont_t registry: wui_font_table[N] → msdfFont_t * (index 0 reserved).
  - Per-frame Clay_Reset (currently a no-op contract — Clay v0.14 manages
    its own per-frame reset through Clay_BeginLayout; we call SetLayoutDimensions
    when the window-pixel size changes).

This initial scope explicitly does NOT walk the panel tree, emit Clay
declarations, or consume Clay_RenderCommandArray. cl_scrn.c::SCR_DrawScreenField
still drives rendering during the parallel-run period; a later step retires it
together with the modality + loading-screen migration.
*/

#include "../../client.h"
#include "cl_wired_compositor.h"
#ifdef WIRED_WEB_UI_NATIVE
#include "../../../web/web_authored_content.h"
#endif
#include "cl_wired_ui.h"
#include "policy/wui_bg_preset.h"
#include "cl_wired_widget_core.h"   /* 5-state resolver, focus-ring, state colours */
#include "cl_wired_anim.h"
#include "cl_wired_bg.h"
#include "cl_wired_msdf.h"
#include "cl_wired_fonts.h"
#include "cl_wired_text.h"   /* Text_ShadowEnabled — dispatch gate */
#include "cl_wired_customdraw.h"  /* unified custom-draw registry */
#include "cl_wired_ui_hud_state.h"   /* WiredHud_SE_Visible + wiredHud_state_valid */
#include "../store/cl_wired_store.h"
#include "../../../qcommon/menudef.h"

/* WiredUI_OwnerDrawVisible exposed from cl_wired_ownerdraw.c
 * so the compositor's CUSTOM dispatch can mirror the legacy ownerdrawFlag
 * gating. Declared inline rather than added to cl_wired_ui.h to keep the
 * dependency narrow. */
extern qboolean WiredUI_OwnerDrawVisible( int flags );

#include "policy/wui_layer_policy.h"

/* viewport-element renderer lives in elements/viewport.c. Resolves
 * the provider registered against viewportId and invokes its render
 * callback; emits a red placeholder rect on miss. */
extern void WiredUI_RenderViewport( float x, float y, float w, float h,
                                     const char *viewportId );

/* scorelist_widget element renderer lives in
 * elements/scorelist_widget.c. Dispatches to the relocated
 * WiredHud_DrawScorelistWidget / WiredHud_DrawDuelBoard backends by
 * subtype string. */
extern void WiredUI_RenderScorelistWidget( float x, float y, float w, float h,
                                            const vec4_t color, const char *subtype );

/* console_view element renderer lives in elements/console.c
 * (relocated from panels/console.c). Wraps the existing Con_DrawConsole
 * full-frame draw so the WUI_LAYER_CONSOLE walk drives console rendering
 * without a SCR_DrawScreenField direct call. */
extern void Con_DrawConsole( void );

/* Per-frame command struct threaded through Clay's customData channel.
 * Allocated from wui_clay_scratch_arena at emit time (bump-pointer,
 * rewound each frame). Carries everything the dispatch needs to invoke
 * the registered routine — name (for re-lookup at dispatch time), rect,
 * color, ownerdrawFlag, and the per-item stateful context (if any). */
typedef struct {
	char        name[ 64 ];          /* full sigil-prefixed key */
	float       rect[ 4 ];           /* x, y, w, h pixels */
	vec4_t      color;
	int         ownerdrawFlag;
	int         familyIndex;
	const wiredItemDef_t *item;      /* stable parsed item; stateful lazy-create input */
	const wiredItemDef_t *perspectiveOwner; /* lexical flex ancestor owning paint transform */
	void       *context;             /* item->customDrawContext (may be NULL) */
} wuiCustomDrawCommand_t;
#include "../../../qcommon/wired/core/scripting/user_vm.h"

LOG_DECLARE_CHANNEL( ch_ui, "ui" );

#if FEAT_WIRED_UI

#define CLAY_IMPLEMENTATION
#include "clay.h"

/* ── Clay arena slice ─────────────────────────────────────────────── *
 * The design spec (§5.3) quoted 4 MB — that estimate predates the v0.14
 * vendor and the v0.14 default Clay_MaxElementCount (8192) / Clay_Max-
 * MeasureTextCacheWordCount (16384) settings, whose combined size table
 * makes Clay_MinMemorySize() return ~6 MB before we've allocated a single
 * element. Bump the floor to 8 MB to give modest headroom (covers Clay's
 * stated min + ~2 MB), and runtime-promote past that if a future Clay rev
 * or a Clay_SetMaxElementCount tweak demands more. Surfaced as a deviation
 * from spec — Eser to ratify the new floor.
 */
#define WIRED_CLAY_ARENA_BYTES_FLOOR    ( 8u * 1024u * 1024u )

/* per-frame scratch arena for repeat-block expansion.
 * Carved from WiredUI_Arena at compositor init, reset (bump-pointer rewind)
 * at top of every EmitFrame. 1 MB is the spec-mandated size. */
#define WIRED_CLAY_SCRATCH_BYTES        ( 1u * 1024u * 1024u )
static arena_t       *wui_clay_scratch_arena = NULL;

/* CUSTOM carriers live in a reused per-frame arena. Arena_Alloc deliberately
 * does not clear recycled bytes, so every carrier must start from a fully
 * initialized record. This matters whenever the record grows: perspectiveOwner
 * was added after the viewport/scene/console emitters were written, and their
 * recycled bytes were then interpreted as an item pointer during map-loading
 * composition. Centralizing zero-init makes that class of ABI-within-a-struct
 * lifetime bug impossible for future fields as well. */
static wuiCustomDrawCommand_t *wui_clay_alloc_custom_command( void )
{
	wuiCustomDrawCommand_t *cmd;

	if ( !wui_clay_scratch_arena ) return NULL;
	cmd = (wuiCustomDrawCommand_t *) Arena_Alloc(
		wui_clay_scratch_arena, sizeof( *cmd ), sizeof( void * ) );
	if ( !cmd ) return NULL;
	memset( cmd, 0, sizeof( *cmd ) );
	return cmd;
}

/* D3 fix helper: copy a transient (stack) string into the per-frame scratch
 * arena so the pointer survives until Clay's deferred render-command walk
 * (Clay_EndLayout) — Clay stores the Clay_String char pointer, not a copy, and
 * draws it AFTER emit_item returns. Returns "" (a persistent literal) if the
 * arena is unavailable/full, never NULL, so the caller's displayText[0] guard
 * is always safe. */
static const char *wui_clay_arena_strdup( const char *src ) {
	size_t  n;
	char   *dst;
	if ( !src ) return "";
	if ( !wui_clay_scratch_arena ) return "";
	n   = strlen( src ) + 1;
	dst = (char *) Arena_Alloc( wui_clay_scratch_arena, n, 1 );
	if ( !dst ) return "";
	memcpy( dst, src, n );
	return dst;
}

static Clay_Arena     wui_clay_arena;
static Clay_Context  *wui_clay_context     = NULL;
static qboolean       wui_clay_initialized = qfalse;
static int            wui_clay_lastWidth   = -1;
static int            wui_clay_lastHeight  = -1;

/* debug-overlay gate cvar. Registered in WiredUI_ClayInit;
 * the debug_overlay_policy_isActive predicate reads it
 * via Cvar_VariableValue("wired_ui_debug"). The legacy `developer` cvar
 * compound gate was retired per Memory K16 (diagnostic-cvar dependency
 * ban) — `wired_ui_debug` is the sole functional gate. */
static cvar_t        *wired_ui_debug = NULL;

/* ── font indirection table ─────────────────────────────────
 * Clay's Clay_TextElementConfig.fontId is a uint16_t index into this table.
 * The same table is consulted by the measure callback and later by the
 * TEXT-command interpreter in the compositor backend, guaranteeing the
 * measure path and draw path use byte-identical font handles. Slot 0 is
 * reserved as a "no font" sentinel — measure returns (0,0) for it.
 */
#define MAX_WUI_FONTS  16
static msdfFont_t *wui_font_table[ MAX_WUI_FONTS ];
static int         wui_font_count = 0;   /* slot 0 reserved → first real slot is 1 */

/* Register an MSDF font in the indirection table. Idempotent — repeat
 * registrations of the same atlas return the existing slot. Returns the
 * 0-based slot index (1..MAX_WUI_FONTS-1) or 0 if registration failed. */
static int wui_clay_register_font( msdfFont_t *atlas )
{
	int i;

	if ( !atlas ) return 0;

	for ( i = 1; i < wui_font_count; i++ ) {
		if ( wui_font_table[ i ] == atlas ) return i;
	}

	if ( wui_font_count >= MAX_WUI_FONTS ) {
		Com_Log( SEV_WARN, LOG_CH(ch_ui),
			"WiredUI/Clay: font indirection table full (MAX_WUI_FONTS=%d)\n",
			MAX_WUI_FONTS );
		return 0;
	}

	if ( wui_font_count == 0 ) {
		/* lazy-init: reserve slot 0 for "no font" */
		wui_font_table[ 0 ] = NULL;
		wui_font_count = 1;
	}

	wui_font_table[ wui_font_count ] = atlas;
	return wui_font_count++;
}

/* Populate the table from WiredUI's registered fontFace_t atlases.
 *
 * Called from WiredUI_ClayRefreshFontTable, which the compositor lifecycle
 * (now process-lifetime) does NOT invoke directly — the compositor
 * comes up at CL_Init before any fonts are loaded. WiredUI_Init runs
 * Text_Init() and then asks the compositor to refresh; subsequent map loads
 * (which call WiredUI_Init again) re-populate idempotently.
 *
 * Re-population is safe because msdfFont_t storage lives in the persistent
 * "Font" arena (cl_wired_msdf.c:30), separate from WiredUI_Arena, so the
 * font pointers stored in wui_font_table never go stale between WiredUI_Init
 * cycles. */
static void wui_clay_populate_font_table( void )
{
	/* Reserve slot 0 even if no fonts resolve (defensive). */
	wui_font_table[ 0 ] = NULL;
	wui_font_count = 1;

	/* Resolve the canonical Wired UI fonts. Names match
	 * wiredui-clay-design.md §5.2 / docs/wired-ui.md. WiredFont_ResolveByName
	 * returns NULL if the atlas wasn't loaded; register_font silently skips. */
	{
		const char *names[] = {
			"sansman",
			"sansman-italic",
			"oxanium",
			"oxanium-medium",
			"sharetechmono",
			"jetbrainsmono",  /* canonical $font_mono target (_tokens.wui) */
			"wui_icons",  /* MSDF icon atlas */
		};
		size_t i;
		for ( i = 0; i < ARRAY_LEN( names ); i++ ) {
			const fontFace_t *face = WiredFont_ResolveByName( names[ i ] );
			if ( face && face->atlas ) {
				wui_clay_register_font( face->atlas );
			}
		}
	}

	Com_Log( SEV_INFO, LOG_CH(ch_ui),
		"WiredUI/Clay: font indirection table populated (%d slot%s, slot 0 reserved)\n",
		wui_font_count, wui_font_count == 1 ? "" : "s" );
}

/* ── measure callback ──────────────────────────────────────
 * Clay invokes this on every TEXT element to size it, splitting the string
 * into space/newline-delimited WORD slices and summing their widths (see
 * Clay__MeasureTextCached in clay.h). Each slice Clay hands us is a
 * non-NUL-terminated pointer into the shared source buffer plus a BYTE
 * length — so the measure MUST bound on that byte window, not on a glyph
 * budget. Routing through MSDF_MeasureStringBytes does exactly that: it walks
 * the slice's bytes, skipping ^N colour codes for width (same as the draw
 * path), and stops at the byte boundary. A glyph budget (the old
 * MSDF_MeasureString(text.length) call) over-read past the slice by one glyph
 * per embedded ^N code — because text.length is a BYTE count but the loop
 * spent it as a GLYPH count — inflating the summed width proportional to the
 * colour-code count and stranding right-aligned text left of its edge
 * (the obituary ragged-right bug). The draw dispatch uses the twin
 * MSDF_DrawStringBytes on the same byte window → measure metric == draw metric
 * for any colour-coded string, no per-caller colour-split workaround needed.
 */
static Clay_Dimensions wui_clay_measure_text( Clay_StringSlice text,
                                              Clay_TextElementConfig *cfg,
                                              void *userData )
{
	Clay_Dimensions out = { 0.0f, 0.0f };
	msdfFont_t     *font;
	float           letterSpacing;
	float           pxFontSize;

	(void) userData;

	if ( !cfg || cfg->fontId >= MAX_WUI_FONTS ) return out;

	font = wui_font_table[ cfg->fontId ];
	if ( !font || !font->loaded ) return out;
	if ( !text.chars || text.length <= 0 ) return out;

	letterSpacing = (float) cfg->letterSpacing;

	/* DPI: cfg->fontSize is the logical point size. Clay lays out in
	 * physical-pixel space (Clay_SetLayoutDimensions is fed the physical size),
	 * so convert logical points → physical pixels here. The draw dispatch
	 * applies the SAME scale, keeping the measured layout box and the rendered
	 * glyphs consistent on HiDPI displays. dpiScale is 1.0 on non-HiDPI. */
	pxFontSize = (float) cfg->fontSize * WiredUI_GetDpiScale();

	out.width  = MSDF_MeasureStringBytes( font,
	                                      pxFontSize,
	                                      text.chars,
	                                      text.length,
	                                      letterSpacing );
	out.height = pxFontSize * font->lineHeight;
	return out;
}

/* ── Clay error handler ───────────────────────────────────────────
 * Routes Clay's internal errors (arena overflow, missing measure callback,
 * etc.) into the system log channel. During bring-up this only triggers if
 * our init is wrong; later work may extend it with graceful-degradation
 * behaviour (spec R-9).
 */
/* source-attribution: emit context — filled by the per-frame
 * panel walk in WiredUI_CompositorEmitFrame so Clay layout errors blame
 * the source menu/item/layer instead of the anonymous registry.
 * Production-default values are NULL/-1 → log emit unchanged when no
 * emit pass is active. */
static const char *s_wui_emit_menu  = NULL;
static const char *s_wui_emit_item  = NULL;
static int         s_wui_emit_layer = -1;

const char *WiredUI_EmitContextMenu ( void ) { return s_wui_emit_menu;  }
const char *WiredUI_EmitContextItem ( void ) { return s_wui_emit_item;  }
int         WiredUI_EmitContextLayer( void ) { return s_wui_emit_layer; }

static const char *wui_layer_name_of( int layer )
{
	switch ( layer ) {
	case WUI_LAYER_BG_DARK:        return "bg_dark";
	case WUI_LAYER_BG_ANIMATED:    return "bg_animated";
	case WUI_LAYER_BG_ATTRACT:     return "bg_attract";
	case WUI_LAYER_LOADING:        return "loading";
	case WUI_LAYER_WORLD_VIEWPORT: return "world_viewport";
	case WUI_LAYER_HUD:            return "hud";
	case WUI_LAYER_MENU:           return "menu";
	case WUI_LAYER_POPUP:          return "popup";
	case WUI_LAYER_DEBUG_OVERLAY:  return "debug_overlay";
	case WUI_LAYER_OVERLAY:        return "overlay";
	case WUI_LAYER_CONSOLE:        return "console";
	default:                      return "(none)";
	}
}

static void wui_clay_error_handler( Clay_ErrorData err )
{
	if ( s_wui_emit_menu ) {
		Com_Log( SEV_WARN, LOG_CH(ch_ui),
			"WiredUI/Clay error during emit of menu '%s' (item '%s', layer '%s'): "
			"type=%d message='%.*s'\n",
			s_wui_emit_menu,
			s_wui_emit_item  ? s_wui_emit_item  : "(none)",
			wui_layer_name_of( s_wui_emit_layer ),
			(int) err.errorType,
			(int) err.errorText.length, err.errorText.chars );
	} else {
		Com_Log( SEV_WARN, LOG_CH(ch_ui),
			"WiredUI/Clay error: type=%d message='%.*s'\n",
			(int) err.errorType,
			(int) err.errorText.length, err.errorText.chars );
	}
}

/* ── lifecycle ────────────────────────────────────────────────── */
void WiredUI_ClayInit( arena_t *parent )
{
	void              *clayMem;
	uint32_t           clayMinSize;
	uint32_t           clayBytes;
	Clay_Dimensions    dims;
	Clay_ErrorHandler  errHandler = { wui_clay_error_handler, NULL };

	if ( wui_clay_initialized ) return;
	if ( !parent ) {
		Com_Log( SEV_ERROR, LOG_CH(ch_ui),
			"WiredUI/Clay: WiredUI_ClayInit called with NULL parent arena\n" );
		return;
	}

	/* Honour the higher of {FLOOR, Clay_MinMemorySize()} so a future Clay
	 * upgrade or Max-Element bump auto-promotes without re-touching code. */
	clayMinSize = Clay_MinMemorySize();
	clayBytes   = ( clayMinSize > WIRED_CLAY_ARENA_BYTES_FLOOR )
	            ?   clayMinSize
	            :   WIRED_CLAY_ARENA_BYTES_FLOOR;

	clayMem = Arena_Alloc( parent, clayBytes, 16 );
	if ( !clayMem ) {
		Com_Log( SEV_ERROR, LOG_CH(ch_ui),
			"WiredUI/Clay: Arena_Alloc %u bytes from WiredUI_Arena failed\n",
			(unsigned) clayBytes );
		return;
	}

	wui_clay_arena = Clay_CreateArenaWithCapacityAndMemory(
		clayBytes, clayMem );

	/* carve the per-frame scratch arena. Arena_Create
	 * gets its own registration in /meminfo so the scratch usage is
	 * visible alongside the WiredUI parent. Bump-pointer reset is O(1)
	 * inside EmitFrame. */
	if ( !wui_clay_scratch_arena ) {
		wui_clay_scratch_arena = Arena_Create( "WiredUI_ClayScratch", WIRED_CLAY_SCRATCH_BYTES );
	}

	/* Seed layout dimensions with the current window pixel size. The
	 * value is allowed to be approximate at init since no Clay
	 * declarations are emitted yet; WiredUI_ClayFrame keeps it in sync
	 * each frame. */
	dims.width  = (float) (cls.glconfig.vidWidth  > 0 ? cls.glconfig.vidWidth  : 1280);
	dims.height = (float) (cls.glconfig.vidHeight > 0 ? cls.glconfig.vidHeight :  720);

	wui_clay_context = Clay_Initialize( wui_clay_arena, dims, errHandler );

	/* Font table stays empty at compositor init: WiredUI fonts haven't
	 * been loaded yet at CL_Init time. WiredUI_Init calls Text_Init followed
	 * by WiredUI_ClayRefreshFontTable to fill it in. Until then the measure
	 * callback returns (0,0) for slot 0 — fine since no Clay declarations
	 * are emitted before the panel walk lands. */
	memset( wui_font_table, 0, sizeof( wui_font_table ) );
	wui_font_count = 0;

	Clay_SetMeasureTextFunction( wui_clay_measure_text, NULL );

	wui_clay_lastWidth  = (int) dims.width;
	wui_clay_lastHeight = (int) dims.height;
	wui_clay_initialized = qtrue;

	/* debug-overlay gate cvar. The layer-specific `wired_ui_debug` is the
	 * SOLE functional gate; the legacy `developer` cvar compound test was
	 * retired per Memory K16 (diagnostic-cvar dependency ban). Modders
	 * flip wired_ui_debug per-session to surface the layer. */
	if ( !wired_ui_debug ) {
		wired_ui_debug = Cvar_Get( "wired_ui_debug", "0", CVAR_TEMP );
	}

	Com_Log( SEV_INFO, LOG_CH(ch_ui),
		"WiredUI/Clay: initialized (arena %u bytes / %u MB, min required %u bytes, "
		"layout %dx%d)\n",
		(unsigned) clayBytes,
		(unsigned) (clayBytes / (1024u * 1024u)),
		clayMinSize,
		wui_clay_lastWidth, wui_clay_lastHeight );
}

/* public predicate; see cl_wired_compositor.h. */
qboolean WiredUI_IsClayInitialized( void )
{
	return wui_clay_initialized;
}

/* Public — called by WiredUI_Init after Text_Init, and again on hot-reload.
 * The compositor itself lives across map loads, so this is the
 * subsystem-level binding-refresh hook. */
void WiredUI_ClayRefreshFontTable( void )
{
	if ( !wui_clay_initialized ) {
		/* Defensive — refresh before init is a bug elsewhere; ignore. */
		return;
	}
	wui_clay_populate_font_table();
}

void WiredUI_ClayShutdown( void )
{
	if ( !wui_clay_initialized ) return;

	/* Clay's arena memory is owned by WiredUI_Arena; Arena_Destroy in
	 * WiredUI_CompositorShutdown releases it. Nothing to free here.
	 *
	 * BUT — clear Clay's process-static context pointer. It still references
	 * the arena memory we're about to free, and Clay_MinMemorySize() at the
	 * NEXT WiredUI_ClayInit (vid_restart / map-load reset) reads
	 * currentContext->maxElementCount through that stale pointer to size the
	 * new arena (clay.h:3873-3877). Without this reset, the next init reads
	 * garbage from freed memory and Arena_Alloc terminates the process
	 * (observed: Clay_MinMemorySize → 1.7 GB on the post-shutdown re-init).
	 */
	Clay_SetCurrentContext( NULL );

	memset( wui_font_table, 0, sizeof( wui_font_table ) );
	wui_font_count       = 0;
	wui_clay_context     = NULL;
	wui_clay_lastWidth   = -1;
	wui_clay_lastHeight  = -1;
	wui_clay_initialized = qfalse;
}

/* Per-frame tick. Clay's actual per-frame reset happens inside
 * Clay_BeginLayout (v0.14 manages its scratch internally); we only need to
 * update layout dimensions when they change. WiredUI_CompositorEmitFrame is
 * called from the per-frame entry to walk panels + emit Clay declarations +
 * dispatch render commands; a later step retires SCR_DrawScreenField. */
void WiredUI_ClayFrame( int widthPx, int heightPx )
{
	if ( !wui_clay_initialized ) return;

	if ( widthPx != wui_clay_lastWidth || heightPx != wui_clay_lastHeight ) {
		Clay_Dimensions dims = { (float) widthPx, (float) heightPx };
		Clay_SetLayoutDimensions( dims );
		wui_clay_lastWidth  = widthPx;
		wui_clay_lastHeight = heightPx;
	}
}

/* ───────────────────────────────────────────────────────────────────
 * tree-to-Clay converter + backend dispatch
 *
 * Strategy: Clay is the sole tree-layout and render-command authority. Static
 * items participate in native Clay flow; explicitly positioned items map to
 * Clay floating declarations. The resolvedRect fields are only a synchronized
 * compatibility snapshot for input/provider code that cannot query Clay yet.
 *
 * Attribute mapping coverage:
 *
 *   wiredItemDef field            Clay slot                            status
 *   ────────────────────────      ─────────────────────────────        ────────
 *   resolvedRect.{x,y}            .floating.offset                     ✓
 *   resolvedRect.{w,h}            .layout.sizing fixed                 ✓
 *   position                      .floating.attachTo (always ROOT)     ✓ (simplified)
 *   visible                       early-out prune                      ✓
 *   backcolor                     .backgroundColor                     ✓
 *   bordercolor                   .border.color                        ✓
 *   border + bordersize           .border.width                        ✓
 *   background[64] (shader)       IMAGE child element                  ✓
 *   text[256]                     CLAY_TEXT child element              ✓
 *   forecolor                     .textColor on CLAY_TEXT              ✓
 *   fontName + fontPointSize      fontId via slot lookup + fontSize    ✓
 *   letterSpacing                 .letterSpacing on CLAY_TEXT          ✓
 *   storeBind / storeBindColor    resolve via WiredStore (text/color)  ✓ (no Lua yet)
 *   showBind / hideBind           resolve via WiredStore, prune        ✓
 *   decoration                    pointerCaptureMode = passthrough     ✓ (foundation for hit-test)
 *   childCount + children[]       recursive native-flow emit           ✓
 *   layout/gap/padding/grow       Clay sizing/alignment                 ✓
 *   breakpoint                    effective authored rect selection     ✓
 *   scroll                        Clay clip + compositor scroll state   ✓
 */

/* ── module state ─────────────────────────────────────────────────── */

/* Internal switch — when qtrue the backend's re.* calls actually emit
 * draw commands. Spec mandates "no cvar"; toggle by code edit + rebuild.
 *
 * Parallel-run default: qfalse. The contract is "BOTH run, SCR overlays
 * on top" — but the compositor's per-frame entry fires from
 * CL_Frame BEFORE SCR_UpdateScreen, i.e. before RE_BeginFrame, so re.*
 * calls would hit the renderer outside its valid frame scope (the
 * NVIDIA GL driver's nvoglv64.dll asserts on this with an ACCESS_VIOLATION
 * — verified during implementation). A later step wires the dispatch
 * into the proper SCR_DrawScreenField slot (alongside the legacy walk's
 * retirement) so the switch can flip back on.
 *
 * Until then: the converter still walks every visible panel + item, the
 * backend dispatch still counts emissions (wui_compositor_*_emitted), and
 * the Clay render commands still get produced — but actual draw calls are
 * elided, so the existing SCR_DrawScreenField output remains the sole
 * visible UI. Validates the converter + backend wiring end-to-end without
 * risking the renderer scope violation. */
static qboolean wui_compositor_emit_to_swapchain = qtrue;

/* Panel-alpha pass-through hook. Currently always 1.0 (no fade); a later
 * step wires this to the modality state machine as a one-line
 * change at the point where wui_compositor_panel_alpha gets multiplied
 * into emitted colors below. */
static float wui_compositor_panel_alpha = 1.0f;

/* Scissor stack. Clay emits SCISSOR_START/END pairs around clip elements;
 * the backend pushes/pops a Wired-side stack so we can intersect nested
 * clips and pass the resolved rect through re.SetClipRegion. Depth 8 is
 * the spec-mandated cap; overflow drops the push with a once-only log. */
#define WUI_COMPOSITOR_SCISSOR_DEPTH 8
typedef struct {
	float x, y, w, h;
} wui_scissor_rect_t;
static wui_scissor_rect_t wui_scissor_stack[ WUI_COMPOSITOR_SCISSOR_DEPTH ];
static int                wui_scissor_depth = 0;
static qboolean           wui_scissor_overflow_warned = qfalse;

/* backend statistics — printf-debug only, not exposed as a cvar.
 * Useful during implementation to confirm the dispatch is being exercised
 * the expected number of times per frame. */
static int wui_compositor_rect_emitted = 0;
static int wui_compositor_text_emitted = 0;
static int wui_compositor_image_emitted = 0;
static int wui_compositor_border_emitted = 0;
static int wui_compositor_custom_emitted = 0;

/* ── hit-test state + visible-only iteration ──
 *
 * Pointer state: WiredUI_MouseEvent forwards the (clamped) cursor through
 * WiredUI_CompositorPointerMoved, mouse buttons through CompositorMouseButton.
 * Read by Clay_SetPointerState at the top of each visible panel's BeginLayout
 * inside the per-frame walk (so per-panel localPointer is just the global
 * cursor for now — non-identity per-panel transforms are later work).
 *
 * Dispatch gate: similar pattern to wui_compositor_emit_to_swapchain — the
 * compositor records hover + down-target + would-fire data, but actual
 * action dispatch via WiredUI_RunScript is gated by wui_compositor_input_active
 * (default OFF). Legacy WiredUI_KeyEvent path remains the sole
 * action source while both paths run in parallel; a later step retires legacy
 * and flips this on, matching the wui_compositor_emit_to_swapchain story.
 */
static float    wui_compositor_pointer_x   = -1.0f;
static float    wui_compositor_pointer_y   = -1.0f;
static qboolean wui_compositor_mouse_down  = qfalse;
static uint32_t wui_compositor_down_target = 0;    /* Clay element id captured on mouse-down */
static qboolean wui_compositor_input_active = qfalse;  /* gate: action fire from Clay path */

/* Per-panel persistent state — survives across frames so focus/hover resume
 * when a panel re-enters the visible_panels[] list. Keyed by wiredMenuDef_t*. */
typedef struct {
	const wiredMenuDef_t *panel;
	uint32_t              hoveredId;
	uint32_t              focusedId;
} wui_panel_state_t;

static wui_panel_state_t wui_panel_states[ WIRED_MAX_MENUS ];
static int               wui_panel_state_count = 0;

static wui_panel_state_t *wui_panel_state_for( const wiredMenuDef_t *panel )
{
	int i;
	if ( !panel ) return NULL;
	for ( i = 0; i < wui_panel_state_count; i++ ) {
		if ( wui_panel_states[ i ].panel == panel ) return &wui_panel_states[ i ];
	}
	if ( wui_panel_state_count >= (int) ARRAY_LEN( wui_panel_states ) ) {
		return NULL;   /* defensive; the array is sized for max menus */
	}
	wui_panel_states[ wui_panel_state_count ].panel     = panel;
	wui_panel_states[ wui_panel_state_count ].hoveredId = 0;
	wui_panel_states[ wui_panel_state_count ].focusedId = 0;
	return &wui_panel_states[ wui_panel_state_count++ ];
}

/* ID → wiredItemDef_t reverse-lookup map. Rebuilt every frame inside
 * EmitFrame. Linear-search lookup; capacity sized to cover the largest
 * realistic panel set on screen at once (5 panels × 256 items + headroom). */
#define WUI_ID_MAP_CAPACITY  2048
typedef struct {
	uint32_t                clayId;
	const wiredItemDef_t   *item;
	const wiredMenuDef_t   *panel;
} wui_id_map_entry_t;

static wui_id_map_entry_t wui_id_map[ WUI_ID_MAP_CAPACITY ];
static int                wui_id_map_count       = 0;
static qboolean           wui_id_map_full_warned = qfalse;

static void wui_id_map_record( uint32_t clayId,
                               const wiredItemDef_t *item,
                               const wiredMenuDef_t *panel )
{
	if ( !clayId || !item ) return;
	if ( wui_id_map_count >= WUI_ID_MAP_CAPACITY ) {
		if ( !wui_id_map_full_warned ) {
			Com_Log( SEV_WARN, LOG_CH(ch_ui),
				"WiredUI/Clay hit-test: id-map full at %d entries — later items uncaptured\n",
				WUI_ID_MAP_CAPACITY );
			wui_id_map_full_warned = qtrue;
		}
		return;
	}
	wui_id_map[ wui_id_map_count ].clayId = clayId;
	wui_id_map[ wui_id_map_count ].item   = item;
	wui_id_map[ wui_id_map_count ].panel  = panel;
	wui_id_map_count++;
}

static qboolean wui_id_map_lookup( uint32_t clayId,
                                   const wiredItemDef_t **outItem,
                                   const wiredMenuDef_t **outPanel )
{
	int i;
	if ( !clayId ) return qfalse;
	for ( i = 0; i < wui_id_map_count; i++ ) {
		if ( wui_id_map[ i ].clayId == clayId ) {
			if ( outItem  ) *outItem  = wui_id_map[ i ].item;
			if ( outPanel ) *outPanel = wui_id_map[ i ].panel;
			return qtrue;
		}
	}
	return qfalse;
}

/* Visible-panels list — built at the top of each EmitFrame. Sized for the
 * worst case (WIRED_MAX_MENUS) but typically holds 1-5 entries. */
static const wiredMenuDef_t *wui_visible_panels[ WIRED_MAX_MENUS ];
static int                   wui_visible_panel_count = 0;

/* (wired_ui_debug cvar pointer declared earlier — see top-of-file statics.) */

/* per-layer activation predicate.
 *
 * Decides whether layer L should be emitted this frame based on cls.state,
 * menu-stack depth, popup queue, and the debug cvar pair. Called once per
 * layer from the visible-panel build pass at the top of EmitFrame.
 *
 * Rules:
 *   BG_ATTRACT     — CA_DISCONNECTED + no menu stack pushed
 *   LOADING        — CA_CONNECTING / CHALLENGING / CONNECTED / LOADING / PRIMED
 *   HUD            — CA_ACTIVE + no menu stack pushed
 *   MENU_STACK     — menu stack non-empty (root main/ingame also counts via
 *                    WiredUI_GetActiveMenu, see emit-pass note below)
 *   POPUP          — popup queue non-empty
 *   DEBUG_OVERLAY  — wired_ui_debug != 0 (dropped the
 *                    legacy `developer` cvar compound gate per Memory K16
 *                    diagnostic-cvar dependency ban; the layer-specific
 *                    wired_ui_debug is the sole opt-in)
 */
static qboolean wui_layer_active( wuiLayer_t layer )
{
	/* per-layer policy predicates live in
	 * code/client/wired/ui/policy modules. Each module owns its rule
	 * (CA_ACTIVE gate, KEYCATCH_*, viewport provider count, etc) so the
	 * dispatcher stays a thin route table. The dev force-override mask
	 * still gates here so the production rule and the dev override share
	 * a single hot-path predicate. */

	/* dev force-override mask. When the dev commands
	 * wui_layer_test_single / _stack flip a layer's bit, return active
	 * immediately and skip the production policy below. Mask is driven
	 * exclusively by the dev commands; production default is 0 so this is
	 * a single AND + branch on a zero word on the hot path.
	 *
	 * exclusive-mode toggle: when
	 * the dev command was invoked with the `exclusive` keyword, the
	 * predicate suppresses unmasked layers instead of falling through to
	 * production policy. Three outcomes:
	 *   - mask=0                       → fall through (production path)
	 *   - mask!=0, exclusive=qfalse    → masked bits force-on, unmasked
	 *                                     fall through (additive)
	 *   - mask!=0, exclusive=qtrue     → masked bits force-on, unmasked
	 *                                     suppressed (exclusive) */
	{
		uint32_t mask = WiredUI_LayerForceOverrideMask();
		if ( mask != 0 ) {
			qboolean masked = ( mask & ( 1U << (uint32_t) layer ) ) != 0;
			if ( WiredUI_LayerForceExclusiveMode() ) {
				return masked ? qtrue : qfalse;
			}
			if ( masked ) {
				return qtrue;
			}
		}
	}

	switch ( layer ) {
	case WUI_LAYER_BG_DARK:        return bg_dark_policy_isActive();
	case WUI_LAYER_BG_ANIMATED:    return bg_animated_policy_isActive();
	case WUI_LAYER_BG_ATTRACT:     return bg_attract_policy_isActive();
	case WUI_LAYER_LOADING:        return loading_policy_isActive();
	case WUI_LAYER_WORLD_VIEWPORT: return world_viewport_policy_isActive();
	case WUI_LAYER_HUD:            return hud_policy_isActive();
	case WUI_LAYER_MENU:           return menu_policy_isActive();
	case WUI_LAYER_POPUP:          return popup_policy_isActive();
	case WUI_LAYER_DEBUG_OVERLAY:  return debug_overlay_policy_isActive();
	case WUI_LAYER_OVERLAY:        return overlay_policy_isActive();
	case WUI_LAYER_CONSOLE:        return console_policy_isActive();

	default:
		return qfalse;
	}
}

/* Per-emit-walk path string used to build per-item IDs.
 * "<panelName>/<itemName>" for named items; "<panelName>/i<index>" for
 * anonymous items (deterministic across frames as long as tree shape is
 * stable — panels are static .wmenu definitions, so stability
 * holds). */
static int wui_emit_anon_counter = 0;

static uint32_t wui_clay_id_for_item( const wiredMenuDef_t *panel,
                                       const wiredItemDef_t *item )
{
	char        path[ 196 ];
	Clay_String s;

	if ( !panel || !item ) return 0;

	/* Source-attribution: include the item pointer as a stable disambiguator even
	 * for named items. Legacy TA-style wmenus (preferences.wmenu has 20+
	 * items all sharing `name "prefsBasic"` for show/hide grouping) would
	 * otherwise collide on the (panel, itemName) key and trip Clay's
	 * "element with this ID was already previously declared" check every
	 * frame. wiredItemDef_t pointers are arena-allocated at parse time
	 * and never moved, so the pointer hashes stably across frames. */
	if ( item->name[0] ) {
		Com_sprintf( path, sizeof( path ), "%s/%s@%p",
			panel->name, item->name, (const void *) item );
	} else {
		Com_sprintf( path, sizeof( path ), "%s/i%d", panel->name, wui_emit_anon_counter++ );
	}

	s.isStaticallyAllocated = qfalse;
	s.length                = (int32_t) strlen( path );
	s.chars                 = path;
	return Clay_GetElementId( s ).id;
}

/* Stable Clay id for a slider's TRACK sub-element. Pointer-derived (independent
 * of item->name / the per-frame anon counter) so the same id is emitted every
 * frame and can be resolved back to the track's rendered bounding box for the
 * drag/click hit-test. This is the single-source-geometry contract for the
 * slider: the render emits the track WITH this id and positions the thumb from
 * the track's rect; the input path hit-tests the SAME rect — draw-rect and
 * hit-rect can never diverge (cf. wui_listbox_column_geom for columns). */
static uint32_t wui_clay_slider_track_id_for_item( const wiredItemDef_t *item )
{
	char        path[ 64 ];
	Clay_String s;

	if ( !item ) return 0;
	Com_sprintf( path, sizeof( path ), "sliderTrack@%p", (const void *) item );
	s.isStaticallyAllocated = qfalse;
	s.length                = (int32_t) strlen( path );
	s.chars                 = path;
	return Clay_GetElementId( s ).id;
}

/* Stable Clay ids for a SPINNER's decrement / increment BUTTON sub-elements.
 * Same pointer-derived, single-source-geometry contract as the slider track:
 * the render emits each stepper button WITH this id; the input path resolves the
 * id back to the button's rendered bounding box (Clay_GetElementData) to hit-test
 * clicks and drive click-and-hold repeat. Draw-rect == hit-rect by construction,
 * honouring the HiDPI/Clay-rect invariant (never item->rect / resolvedRect). */
static uint32_t wui_clay_spinner_btn_id_for_item( const wiredItemDef_t *item, qboolean inc )
{
	char        path[ 64 ];
	Clay_String s;

	if ( !item ) return 0;
	Com_sprintf( path, sizeof( path ), "spinner%s@%p", inc ? "Inc" : "Dec", (const void *) item );
	s.isStaticallyAllocated = qfalse;
	s.length                = (int32_t) strlen( path );
	s.chars                 = path;
	return Clay_GetElementId( s ).id;
}

/* ── helpers ───────────────────────────────────────────────────────── */

/* Forward decl — theme-token → RGB resolver (defined below near the listbox
 * tint helpers). Used by widget emit paths (radio/header/slider) above its
 * definition to make accent colours follow ui_palette_accent per frame. */
static void wui_clay_token_rgb( const char *tokenName, float outRGB[3] );

/* Translate an engine RGBA vec4 (0..1) to a Clay_Color (0..255 per channel).
 * The alpha multiplier is the panel-alpha hook from above. */
static Clay_Color wui_clay_color_of( const vec4_t v, float alphaMul )
{
	Clay_Color c;
	c.r = v[0] * 255.0f;
	c.g = v[1] * 255.0f;
	c.b = v[2] * 255.0f;
	c.a = v[3] * 255.0f * alphaMul;
	return c;
}

/* Bit flags packed into a Clay element's .userData and forwarded verbatim onto
   the render command's rc->userData (clay.h propagates it through layout). The
   dispatch reads these back to vary how the command is drawn. TEXT commands use
   the shadow bit; RECTANGLE commands use the overlay bit; they never collide on
   the same command type, but distinct bits keep a future combined use safe. */
#define WUI_TEXT_TAG_SHADOW   ((uintptr_t)0x1)
#define WUI_RECT_TAG_OVERLAY  ((uintptr_t)0x2)
#define WUI_COMMAND_TAG_MASK  ( WUI_TEXT_TAG_SHADOW | WUI_RECT_TAG_OVERLAY )

/* Clay may detach CUSTOM carriers into floating roots and hashes TEXT ids away
 * from their source item. Preserve lexical paint ancestry explicitly in the
 * command userData instead of trying to reconstruct it from command order. */
static void *wui_clay_command_tag( const wiredItemDef_t *perspectiveOwner,
		uintptr_t flags ) {
	uintptr_t owner = (uintptr_t)perspectiveOwner;
	if ( owner & WUI_COMMAND_TAG_MASK ) {
		Com_Log( SEV_WARN, LOG_CH(ch_ui),
			"WiredUI/Clay perspective owner alignment rejected\n" );
		owner = 0u;
	}
	return (void *)( owner | ( flags & WUI_COMMAND_TAG_MASK ) );
}

static const wiredItemDef_t *wui_clay_command_perspective_owner(
		const void *userData ) {
	return (const wiredItemDef_t *)( (uintptr_t)userData
		& ~(uintptr_t)WUI_COMMAND_TAG_MASK );
}

/* Resolve a fill's effective compositing mode and return the .userData tag for
   its emitted rectangle. An item that left its mode INHERIT takes the panel's;
   a panel that is itself INHERIT falls through to DIEGETIC (the world default).
   Only OVERLAY produces a tag — DIEGETIC is the untagged default path. */
static void *wui_clay_composite_tag( wuiCompositeMode_t itemMode, wuiCompositeMode_t panelMode )
{
	wuiCompositeMode_t mode = ( itemMode == WUI_COMPOSITE_INHERIT ) ? panelMode : itemMode;
	if ( mode == WUI_COMPOSITE_OVERLAY )
		return (void *)WUI_RECT_TAG_OVERLAY;
	return NULL;
}

/* Composite tag for a per-ITEM container BACKGROUND fill. Unlike the menu-backdrop
   tag above, a background does NOT inherit the panel's overlay mode: the panel's
   CUSTOM children (statusbar value/icon/bar, crosshair, …) draw INLINE in the UI
   pass, but an overlay-tagged rect is deferred to the post-gamma present pass and
   would replay AFTER — i.e. ON TOP OF — its own readouts (emit order ≠ draw order),
   occluding them. Keeping the background inline preserves painter's order (fill
   under content). Only an item that EXPLICITLY asked for overlay still gets it —
   a full-menu backdrop that wants the display-space blend uses the menu backcolor
   (tagged via the inheriting helper), not a per-widget panel fill. */
static void *wui_clay_composite_tag_item_bg( wuiCompositeMode_t itemMode )
{
	if ( itemMode == WUI_COMPOSITE_OVERLAY )
		return (void *)WUI_RECT_TAG_OVERLAY;
	return NULL;
}

/* Inverse — Clay's 0..255 colors back to engine 0..1 vec4 (verbatim, perceptual). */
static void wui_clay_color_to_vec4( const Clay_Color *src, vec4_t out, float alphaMul )
{
	out[0] = src->r * (1.0f / 255.0f);
	out[1] = src->g * (1.0f / 255.0f);
	out[2] = src->b * (1.0f / 255.0f);
	out[3] = src->a * (1.0f / 255.0f) * alphaMul;
}

/* Resolve the font name on a wiredItemDef_t to a slot in wui_font_table.
 * Returns 0 if the name doesn't match any registered atlas (Clay's measure
 * callback handles slot 0 → (0,0) dimensions). */
static uint16_t wui_clay_font_slot_for_face( const char *fontName )
{
	const fontFace_t *face;
	msdfFont_t       *atlas;
	int               i;

	if ( !fontName || !*fontName ) return 1;   /* default to first real slot */
	face = WiredFont_ResolveByName( fontName );
	if ( !face ) return 1;

	/* Load a lazy face's atlas on first use (block-until-resident) and register it
	 * into the font table now, so its first draw shows correct glyphs rather than the
	 * default font. Boot-loaded faces already have their atlas + table slot. */
	atlas = WiredFont_EnsureAtlas( face );
	if ( !atlas ) return 1;

	for ( i = 1; i < wui_font_count; i++ ) {
		if ( wui_font_table[ i ] == atlas ) return (uint16_t) i;
	}
	/* register_font already returns the slot it placed the atlas in (existing or
	 * new); use it directly instead of re-scanning the table. 0 = table full →
	 * fall back to the default slot. */
	{
		int slot = wui_clay_register_font( atlas );
		return slot ? (uint16_t) slot : 1;
	}
}

/* ── VM dispatcher (System / User) ─────────────────────
 *
 * Compile dispatch happens at parse time in cl_wired_parse.c; today
 * always routes to System VM (chunks live in WiredScript's registry).
 * The User VM chunk-cache API is symmetric and ready, but the parser
 * doesn't yet carry menu context to item-parse sites, so a vm "user"
 * menu's chunks still compile against System VM for now. A later step
 * threads the menu pointer through item-parse + flips the compile site.
 *
 * Eval dispatch goes through these helpers: they look at menu->vm and
 * route to the right VM's chunk-cache API. There are only System
 * VM refs in practice, so the User VM branch is exercised only via a
 * test path (in-engine command); production menus always hit System. */

typedef enum {
	WUI_VM_SYSTEM = 0,
	WUI_VM_USER   = 1
} wui_vm_kind_t;

static wui_vm_kind_t wui_clay_vm_for_panel( const wiredMenuDef_t *menu )
{
	if ( menu && menu->vm[0] && !Q_stricmp( menu->vm, "user" ) ) {
		return WUI_VM_USER;
	}
	/* Default + explicit "system" + unknown = System VM. */
	return WUI_VM_SYSTEM;
}

static qboolean wui_clay_chunk_call_string( wui_vm_kind_t vm, int ref,
                                             char *out, size_t outSize )
{
	if ( vm == WUI_VM_USER )  return UserVM_CallChunkString  ( ref, out, outSize );
	return                          WiredScript_CallChunkString( ref, out, outSize );
}

static qboolean wui_clay_chunk_call_bool( wui_vm_kind_t vm, int ref, qboolean defaultVal )
{
	if ( vm == WUI_VM_USER )  return UserVM_CallChunkBool  ( ref, defaultVal );
	return                          WiredScript_CallChunkBool( ref, defaultVal );
}

static int wui_clay_chunk_call_array_len( wui_vm_kind_t vm, int ref )
{
	if ( vm == WUI_VM_USER )  return UserVM_CallChunkArrayLen  ( ref );
	return                          WiredScript_CallChunkArrayLen( ref );
}

static qboolean wui_clay_chunk_array_item_string( wui_vm_kind_t vm,
                                                    int index, char *out, size_t outSize )
{
	if ( vm == WUI_VM_USER )  return UserVM_ChunkArrayItemAsString  ( index, out, outSize );
	return                          WiredScript_ChunkArrayItemAsString( index, out, outSize );
}

static qboolean wui_clay_chunk_array_item_field( wui_vm_kind_t vm,
                                                   int index, const char *field,
                                                   char *out, size_t outSize )
{
	if ( vm == WUI_VM_USER )  return UserVM_ChunkArrayItemFieldAsString  ( index, field, out, outSize );
	return                          WiredScript_ChunkArrayItemFieldAsString( index, field, out, outSize );
}

static void wui_clay_chunk_array_release( wui_vm_kind_t vm )
{
	if ( vm == WUI_VM_USER )  UserVM_ChunkArrayRelease  ();
	else                            WiredScript_ChunkArrayRelease();
}

static void wui_clay_chunk_release( wui_vm_kind_t vm, int ref )
{
	if ( vm == WUI_VM_USER )  UserVM_ReleaseChunk  ( ref );
	else                            WiredScript_ReleaseChunk( ref );
}

/* ── public dispatch — parser + hot-reload-purge call sites ──
 * Same VM selection as wui_clay_vm_for_panel but exposed for use outside
 * cl_wired_clay.c. The parser previously hardcoded WiredScript_CompileChunk;
 * this routes through the same dispatcher the eval path
 * uses so vm "user" panels actually compile against the User VM. */
int WiredUI_CompositorCompileChunkForMenu( const wiredMenuDef_t *menu,
                                            const char *text,
                                            const char *chunkName )
{
	wui_vm_kind_t vm = wui_clay_vm_for_panel( menu );
	if ( vm == WUI_VM_USER ) {
		return UserVM_CompileChunk( text, chunkName );
	}
	return WiredScript_CompileChunk( text, chunkName );
}

void WiredUI_CompositorReleaseChunkForMenu( const wiredMenuDef_t *menu,
                                             int chunkRef )
{
	wui_clay_chunk_release( wui_clay_vm_for_panel( menu ), chunkRef );
}

/* Read a WiredStore string by key into out. Returns qfalse if the key is
 * empty or not found. Used by bind/showbind/hidebind resolution; the Lua
 * "bind=lua:..." extension lands later. */
static qboolean wui_clay_store_read_text( const char *key, char *out, size_t outSize )
{
	const wuiStoreEntry_t *e;
	if ( !key || !*key ) return qfalse;
	e = WiredStore_Get( key );
	if ( !e || !e->text[0] ) return qfalse;
	Q_strncpyz( out, e->text, outSize );
	return qtrue;
}

/* Read a WiredStore truthiness for show/hide. An entry is "true" if it has
 * a non-empty text or a non-zero value. */
static qboolean wui_clay_store_truthy( const char *key )
{
	const wuiStoreEntry_t *e;
	if ( !key || !*key ) return qfalse;
	e = WiredStore_Get( key );
	if ( !e ) return qfalse;
	return ( e->text[0] != '\0' ) || ( e->value != 0.0f );
}

/* ── converter ──────────────────────────────────────── */

/* `parentIsContainer` flag threads through
 * the recursive emit walk so leaf items inside a flex container parent
 * stay on Clay native flex (useFloating=qfalse). Previously they hit the
 * `!isFlexContainer → useFloating=qtrue` branch and rendered as
 * CLAY_FLOATING pinned to root, which dropped them onto the
 * stale compatibility resolvedRect — competing with Clay's own
 * flex resolution and producing the LEFT-region "Single Player overlap
 * with subtitle" anomaly the multimodal review flagged.
 *
 * `parentDirection` joins the thread. The
 * `grow N` shorthand (and the `width/height GROW` keyword) needs to land
 * on the parent's main axis: column parent → height GROW; row parent →
 * width GROW. Previously the emit unconditionally mapped grow→width,
 * collapsing column-direction grow children onto the cross axis and
 * leaving the main axis to fall back to a PERCENT cascade that shrank
 * leaf text items to sub-fontsize heights. */
static void wui_clay_emit_item( const wiredMenuDef_t *panel,
                                 const wiredItemDef_t *item,
                                 qboolean parentIsContainer,
                                 wuiLayoutDir_t parentDirection,
								 const wiredItemDef_t *perspectiveOwner );

/* forward-declare the repeat-block expansion so
 * wui_clay_emit_item can call it before doing its own per-item emit. */
static void wui_clay_emit_repeat_block( const wiredMenuDef_t *panel,
                                         const wiredItemDef_t *containerItem,
										 const wiredItemDef_t *perspectiveOwner );

/* per-panel Path A/B mode set by wui_clay_emit_panel before
 * recursing items. Static so emit_item + emit_repeat_row + emit_repeat_block
 * can read it without threading through every signature. Reset per-panel. */
static qboolean wui_clay_current_pathB = qfalse;

/* Focus-highlight gradient as an in-tree row background. The focused row paints
 * its highlight as the row element's OWN background (image + tint), which Clay
 * emits before recursing the row's children — so paint order is
 * row-highlight -> caption text, with the caption on top and the highlight bar
 * behind it but in front of the menu background. (A floating element can't sit
 * there: it becomes its own tree root and sorts entirely above or entirely
 * below the main tree.) wui_clay_focus_gradient_for resolves the shader + tint
 * for the focused item; both row-emit paths feed them into the row's CLAY
 * .image / .backgroundColor when set. */
static qboolean wui_clay_focus_gradient_for( const wiredMenuDef_t *panel,
                                             const wiredItemDef_t *item,
                                             qhandle_t *outShader,
                                             Clay_Color *outTint )
{
	if ( !panel || item != WiredUI_GetFocusedItem() ) {
		return qfalse;
	}
	*outTint   = wui_clay_color_of( panel->focuscolor, wui_compositor_panel_alpha );
	*outShader = WiredUI_GradientBarShader();
	return qtrue;
}

/* unit resolution for converter-time placement.
 * Mirrors WUI_Resolve (cl_wired_layout.c:22) but inlines for the
 * compositor's per-frame path. parentSize is the relevant dimension
 * (width for X/W, height for Y/H) of the panel rect. */
static float wui_resolve_unit( wuiValue_t v, float parentSize )
{
	switch ( v.unit ) {
	/* Authored px are LOGICAL points; scale by dpiScale so gap/padding/min-max/
	 * basis track the physical-pixel layout exactly like WUI_Resolve's UNIT_PX
	 * (cl_wired_layout.c:26). Without this the emit pass under-sized these on
	 * HiDPI while the layout pass scaled them — the two disagreed. */
	case UNIT_PX:    return v.value * WiredUI_GetDpiScale();
	/* Must match WUI_Resolve's UNIT_REM exactly — the two resolvers disagreeing
	 * is the bug the UNIT_PX note above records. */
	case UNIT_REM:   return v.value * WiredUI_GetRootScale() * WiredUI_GetDpiScale();
	case UNIT_VW:    return v.value * (float) wui_clay_lastWidth  / 100.0f;
	case UNIT_VH:    return v.value * (float) wui_clay_lastHeight / 100.0f;
	case UNIT_NORM:  return v.value * parentSize;
	case UNIT_AUTO:  /* fall through */
	default:         return 0.0f;   /* AUTO → let Clay's FIT compute it */
	}
}

/* Resolve only the panel's viewport-relative root box. This is deliberately
 * not a second item-tree layout pass: Clay remains the sole authority for all
 * descendant flex geometry. The retained work here is equivalent to CSS's
 * containing-block selection (viewport + fullscreen/anchor), which Clay needs
 * before the panel root declaration can be opened. */
static void wui_clay_resolve_panel_root( wiredMenuDef_t *menu )
{
	wuiPixelRect_t viewport = {
		0.0f, 0.0f,
		(float) wui_clay_lastWidth,
		(float) wui_clay_lastHeight
	};
	float mw, mh;

	if ( !menu ) return;
	menu->resolvedRect = WUI_ResolveRect( &menu->wuiRect, &viewport,
		viewport.w, viewport.h );

	if ( menu->fullscreen ) {
		menu->resolvedRect = viewport;
		return;
	}

	/* An omitted root dimension is a viewport-filling canvas. Content-sized
	 * boxes belong inside that canvas and are measured by Clay FIT. */
	if ( menu->resolvedRect.w <= 0.0f ) menu->resolvedRect.w = viewport.w;
	if ( menu->resolvedRect.h <= 0.0f ) menu->resolvedRect.h = viewport.h;
	mw = menu->resolvedRect.w;
	mh = menu->resolvedRect.h;

	switch ( menu->anchor ) {
	case ANCHOR_TOP_CENTER:
		menu->resolvedRect.x = ( viewport.w - mw ) * 0.5f;
		menu->resolvedRect.y = 0.0f;
		break;
	case ANCHOR_TOP_RIGHT:
		menu->resolvedRect.x = viewport.w - mw;
		menu->resolvedRect.y = 0.0f;
		break;
	case ANCHOR_CENTER_LEFT:
		menu->resolvedRect.x = 0.0f;
		menu->resolvedRect.y = ( viewport.h - mh ) * 0.5f;
		break;
	case ANCHOR_CENTER:
		menu->resolvedRect.x = ( viewport.w - mw ) * 0.5f;
		menu->resolvedRect.y = ( viewport.h - mh ) * 0.5f;
		break;
	case ANCHOR_CENTER_RIGHT:
		menu->resolvedRect.x = viewport.w - mw;
		menu->resolvedRect.y = ( viewport.h - mh ) * 0.5f;
		break;
	case ANCHOR_BOTTOM_LEFT:
		menu->resolvedRect.x = 0.0f;
		menu->resolvedRect.y = viewport.h - mh;
		break;
	case ANCHOR_BOTTOM_CENTER:
		menu->resolvedRect.x = ( viewport.w - mw ) * 0.5f;
		menu->resolvedRect.y = viewport.h - mh;
		break;
	case ANCHOR_BOTTOM_RIGHT:
		menu->resolvedRect.x = viewport.w - mw;
		menu->resolvedRect.y = viewport.h - mh;
		break;
	case ANCHOR_NONE:
	case ANCHOR_TOP_LEFT:
	default:
		break;
	}
}

/* Copy the just-computed Clay boxes back into the compatibility snapshot.
 * Input, popup anchoring and layoutdump already query Clay directly when they
 * can; keeping resolvedRect synchronized removes their first-frame fallback
 * divergence without reviving a second layout engine. */
static void wui_clay_sync_panel_rects( const wiredMenuDef_t *panel )
{
	int i;
	for ( i = 0; i < wui_id_map_count; i++ ) {
		Clay_ElementId   id;
		Clay_ElementData data;
		wiredItemDef_t  *item;

		if ( wui_id_map[ i ].panel != panel || !wui_id_map[ i ].item ) continue;
		id.id = wui_id_map[ i ].clayId;
		data = Clay_GetElementData( id );
		if ( !data.found ) continue;
		item = (wiredItemDef_t *) wui_id_map[ i ].item;
		item->resolvedRect.x = data.boundingBox.x;
		item->resolvedRect.y = data.boundingBox.y;
		item->resolvedRect.w = data.boundingBox.width;
		item->resolvedRect.h = data.boundingBox.height;
	}
}

/* Preserve the stable .wui breakpoint contract after retirement of the legacy
 * tree pre-pass. The selected rectangle feeds Clay's sizing/floating mapping;
 * no second layout pass is involved. */
static const wuiRect_t *wui_clay_effective_item_rect( const wiredItemDef_t *item )
{
	const wuiRect_t *breakpointRect;
	if ( !item || item->breakpointCount <= 0 ) return item ? &item->wuiRect : NULL;
	breakpointRect = WUI_FindBreakpointRect( item->breakpoints,
		item->breakpointCount, wui_clay_lastWidth );
	return breakpointRect ? breakpointRect : &item->wuiRect;
}

/* A static top-level container with an omitted axis is the panel canvas, not a
 * content-hugging box. Seed only that containing block before Clay emission;
 * descendants still use their authored FIT/GROW/PERCENT rules and Clay remains
 * their sole layout authority. Re-seeding every frame is intentional because
 * the post-layout snapshot below replaces resolvedRect with measured boxes. */
static void wui_clay_seed_panel_child( const wiredMenuDef_t *panel,
                                       wiredItemDef_t *item )
{
	const wuiRect_t *layoutRect;
	if ( !panel || !item || !item->isFlexContainer
	  || item->position != POSITION_STATIC ) return;
	layoutRect = wui_clay_effective_item_rect( item );

	if ( layoutRect->w.unit == UNIT_NORM && layoutRect->w.value <= 0.0f ) {
		item->resolvedRect.w = panel->resolvedRect.w;
	}
	if ( layoutRect->h.unit == UNIT_NORM && layoutRect->h.value <= 0.0f ) {
		item->resolvedRect.h = panel->resolvedRect.h;
	}
}

/* Translate (direction, align, justify) to Clay's Clay_ChildAlignment.
 * align is cross-axis, justify is main-axis. For ROW direction, X is
 * main + Y is cross; for COLUMN direction, Y is main + X is cross. */
static Clay_ChildAlignment wui_align_to_clay( wuiLayoutDir_t direction,
                                                wuiAlign_t align,
                                                wuiJustify_t justify )
{
	Clay_ChildAlignment ca;
	Clay_LayoutAlignmentX justifyX = CLAY_ALIGN_X_LEFT;
	Clay_LayoutAlignmentY alignY   = CLAY_ALIGN_Y_TOP;
	Clay_LayoutAlignmentX alignX   = CLAY_ALIGN_X_LEFT;
	Clay_LayoutAlignmentY justifyY = CLAY_ALIGN_Y_TOP;

	switch ( align ) {
	case WUI_ALIGN_START:   alignY = CLAY_ALIGN_Y_TOP;    alignX = CLAY_ALIGN_X_LEFT;  break;
	case WUI_ALIGN_CENTER:  alignY = CLAY_ALIGN_Y_CENTER; alignX = CLAY_ALIGN_X_CENTER; break;
	case WUI_ALIGN_END:     alignY = CLAY_ALIGN_Y_BOTTOM; alignX = CLAY_ALIGN_X_RIGHT; break;
	case WUI_ALIGN_STRETCH: /* Clay has no stretch alignment; treat as start. */
	                        alignY = CLAY_ALIGN_Y_TOP;    alignX = CLAY_ALIGN_X_LEFT;  break;
	}
	switch ( justify ) {
	case WUI_JUSTIFY_START:         justifyX = CLAY_ALIGN_X_LEFT;   justifyY = CLAY_ALIGN_Y_TOP;    break;
	case WUI_JUSTIFY_CENTER:        justifyX = CLAY_ALIGN_X_CENTER; justifyY = CLAY_ALIGN_Y_CENTER; break;
	case WUI_JUSTIFY_END:           justifyX = CLAY_ALIGN_X_RIGHT;  justifyY = CLAY_ALIGN_Y_BOTTOM; break;
	case WUI_JUSTIFY_SPACE_BETWEEN: /* Path B polyfill target; fallback to center
	                                 * to keep something sensible if Path A
	                                 * ever sees this (shouldn't — pathBKind
	                                 * detection sends such panels to B). */
	                                justifyX = CLAY_ALIGN_X_CENTER; justifyY = CLAY_ALIGN_Y_CENTER; break;
	}

	if ( direction == WUI_LAYOUT_COLUMN ) {
		/* main-axis = Y, cross-axis = X */
		ca.x = alignX;
		ca.y = justifyY;
	} else {
		/* ROW (or NONE): main-axis = X, cross-axis = Y */
		ca.x = justifyX;
		ca.y = alignY;
	}
	return ca;
}

/* Map wiredItemDef_t.textalign
 * (ITEM_ALIGN_LEFT/CENTER/RIGHT) to Clay v0.14's native text alignment.
 *
 * NB: CLAY_TEXT_CONFIG.textAlignment is a no-op for single-line text in
 * Clay v0.14 (clay.h:2493-2495 collapses non-wrapped lines to the parent's
 * full width, making the per-line alignment offset always 0). For single-
 * line items we instead set the parent CLAY block's childAlignment.x to
 * the same value so Clay's flex distribution puts the text in the right
 * place. We pass textAlignment to the text config too for completeness
 * (it kicks in if the text ever wraps). */
static Clay_TextAlignment wui_clay_textalign_from_item( int textalign )
{
	switch ( textalign ) {
	case ITEM_ALIGN_CENTER: return CLAY_TEXT_ALIGN_CENTER;
	case ITEM_ALIGN_RIGHT:  return CLAY_TEXT_ALIGN_RIGHT;
	case ITEM_ALIGN_LEFT:
	default:                return CLAY_TEXT_ALIGN_LEFT;
	}
}

/* Companion mapping for the parent CLAY block's childAlignment.x —
 * this is what actually centers/right-aligns single-line text in v0.14. */
static Clay_LayoutAlignmentX wui_clay_alignx_from_item( int textalign )
{
	switch ( textalign ) {
	case ITEM_ALIGN_CENTER: return CLAY_ALIGN_X_CENTER;
	case ITEM_ALIGN_RIGHT:  return CLAY_ALIGN_X_RIGHT;
	case ITEM_ALIGN_LEFT:
	default:                return CLAY_ALIGN_X_LEFT;
	}
}

/* Classify a wiredItemDef_t.textstyle into
 * "needs dropshadow at MSDF dispatch time". Legacy cl_wired_ui.c:5268-5274
 * checks SHADOWED + SHADOWEDMORE; OUTLINESHADOWED is also dropshadow-style
 * in name though legacy doesn't currently treat it as such — we mirror
 * legacy exactly to avoid introducing a new shadow trigger. */
static qboolean wui_textstyle_has_dropshadow( int textstyle )
{
	return ( textstyle == ITEM_TEXTSTYLE_SHADOWED
	      || textstyle == ITEM_TEXTSTYLE_SHADOWEDMORE ) ? qtrue : qfalse;
}

/* Emit a CLAY_FLOATING child block carrying a CUSTOM
 * render command whose customData points to a per-frame
 * wuiCustomDrawCommand_t allocated from the scratch arena. Caller must be
 * inside an open Clay parent (the item's own block).
 *
 * Stateful create is intentionally deferred to CUSTOM command consumption:
 * only then does Clay expose the final bounding box for a native flex child.
 * The command carries the stable parsed-item pointer needed by the create
 * adapter; the resulting context is cached back on that item. */
static void wui_clay_emit_customdraw_for_item( const wiredItemDef_t *item,
                                                float x, float y, float w, float h,
                                                const vec4_t color,
												const wiredItemDef_t *perspectiveOwner )
{
	const wuiCustomDrawDef_t *def;
	wuiCustomDrawCommand_t   *cmd;
	int                       familyIdx = -1;

	if ( !item->customDrawName[ 0 ] ) return;

	def = WiredUI_FindCustomDraw( item->customDrawName, &familyIdx );
	if ( !def ) return;

	cmd = wui_clay_alloc_custom_command();
	if ( !cmd ) return;
	Q_strncpyz( cmd->name, item->customDrawName, sizeof( cmd->name ) );
	cmd->rect[ 0 ]      = x;
	cmd->rect[ 1 ]      = y;
	cmd->rect[ 2 ]      = w;
	cmd->rect[ 3 ]      = h;
	Vector4Copy( color, cmd->color );
	cmd->ownerdrawFlag  = item->ownerdrawFlag;
	cmd->familyIndex    = familyIdx;
	cmd->item           = item;
	cmd->perspectiveOwner = perspectiveOwner;
	cmd->context        = item->customDrawContext;

	/* Anchor-only routines (crosshair and the zero-rect HUD text family) own
	 * their geometry. A tiny native-flex child is valid authoring, but Clay
	 * culls its nested CUSTOM carrier while resolving the panel tree. Emit that
	 * carrier as a root-attached, input-transparent command at the already
	 * resolved pixel anchor. This is not authored absolute layout: the item
	 * itself remains in flex flow and supplies x/y. Content-sized custom items
	 * retain the normal in-tree GROW carrier, including parent clipping. */
	{
		float carrierMax = WiredUI_GetDpiScale();
		if ( carrierMax < 1.0f ) carrierMax = 1.0f;
		if ( w <= carrierMax && h <= carrierMax ) {
			CLAY({
				.layout = { .sizing = {
					CLAY_SIZING_FIXED( w > 0.0f ? w : carrierMax ),
					CLAY_SIZING_FIXED( h > 0.0f ? h : carrierMax )
				} },
				.floating = {
					.attachTo = CLAY_ATTACH_TO_ROOT,
					.offset = { x, y },
					.attachPoints = {
						CLAY_ATTACH_POINT_LEFT_TOP,
						CLAY_ATTACH_POINT_LEFT_TOP
					},
					.pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH
				},
				.custom = { .customData = cmd }
			}) {}
		} else {
			/* A custom command paints the item's box; it is not an additional
			 * layout child. Keeping this carrier in normal flow made a custom on
			 * a container compete with the container's authored children for GROW
			 * space (the loading backdrop consumed the first 36% of the column and
			 * pushed its top bar halfway down the screen). A parent-attached
			 * floating carrier fills the resolved item without affecting its child
			 * layout. Leaf custom items retain the same final bounding box. */
			CLAY({
				.layout = { .sizing = {
					CLAY_SIZING_GROW( 0 ), CLAY_SIZING_GROW( 0 )
				} },
				.floating = {
					.attachTo = CLAY_ATTACH_TO_PARENT,
					.attachPoints = {
						CLAY_ATTACH_POINT_LEFT_TOP,
						CLAY_ATTACH_POINT_LEFT_TOP
					},
					.pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH,
					.clipTo = CLAY_CLIP_TO_ATTACHED_PARENT
				},
				.custom = { .customData = cmd }
			}) {}
		}
	}
}

/* Emit a CLAY CUSTOM child for an ITEM_TYPE_VIEWPORT itemDef. Same
 * shape as wui_clay_emit_customdraw_for_item — bump-alloc a per-frame
 * command struct, write "viewport:<id>" into cmd->name as the discriminator
 * the dispatch consume case (CLAY_RENDER_COMMAND_TYPE_CUSTOM) keys off. */
static void wui_clay_emit_viewport_for_item( const wiredItemDef_t *item,
                                              float x, float y, float w, float h,
                                              const vec4_t color )
{
	// World viewport: this panel hosts the 3D world render and must stay full-rect.
	// The CLAY_SIZING_GROW(0) sizing below (and the absence of padding / border /
	// cornerRadius / aspect / anim) is load-bearing — the world render derives its
	// fov and clip from this rect, so insetting or animating the panel would regress
	// the world's projection. Keep it a bare full-grow custom element.
	wuiCustomDrawCommand_t *cmd;

	cmd = wui_clay_alloc_custom_command();
	if ( !cmd ) return;
	Com_sprintf( cmd->name, sizeof( cmd->name ), "viewport:%s", item->viewportId );
	cmd->rect[ 0 ]     = x;
	cmd->rect[ 1 ]     = y;
	cmd->rect[ 2 ]     = w;
	cmd->rect[ 3 ]     = h;
	Vector4Copy( color, cmd->color );
	cmd->ownerdrawFlag = 0;
	cmd->context       = NULL;

	CLAY({
		.layout = { .sizing = { CLAY_SIZING_GROW( 0 ), CLAY_SIZING_GROW( 0 ) } },
		.custom = { .customData = cmd }
	}) {}
}

/* R1: emit the WiredUI SCENE procedural backdrop as a single FLOATING custom
 * command at the backmost z-index (WUI_BG_SCENE_ZINDEX, -10), matching where the
 * retired rect-band scene sat. Called from WUI_DrawBackgroundScene (cl_wired_bg.c)
 * during the SCENE bg resolve. The dispatch ("menubg:" sigil) reads the live
 * scene params and calls re.DrawMenuBackdrop, which draws the menubg.frag
 * fullscreen quad blended into the open 2D UI pass — so the menu content queued
 * after it composites on top. One custom command replaces ~200 rect emits. */
void WUI_EmitSceneBackdrop( float x, float y, float w, float h ) {
	wuiCustomDrawCommand_t *cmd;

	if ( w <= 0.0f || h <= 0.0f ) return;
	cmd = wui_clay_alloc_custom_command();
	if ( !cmd ) return;
	Q_strncpyz( cmd->name, "menubg:", sizeof( cmd->name ) );
	cmd->rect[ 0 ]     = x;
	cmd->rect[ 1 ]     = y;
	cmd->rect[ 2 ]     = w;
	cmd->rect[ 3 ]     = h;
	Vector4Set( cmd->color, 1.0f, 1.0f, 1.0f, 1.0f );
	cmd->ownerdrawFlag = 0;
	cmd->context       = NULL;

	/* FLOATING + attach-to-root + zIndex -10 so it escapes the layout flow and
	 * sits behind every menu child — the exact placement wui_bg_emit_rect used
	 * for the old scene rects (WUI_BG_SCENE_ZINDEX). */
	CLAY({
		.layout = { .sizing = { CLAY_SIZING_FIXED( w ), CLAY_SIZING_FIXED( h ) } },
		.floating = {
			.attachTo     = CLAY_ATTACH_TO_ROOT,
			.offset       = { x, y },
			.attachPoints = { CLAY_ATTACH_POINT_LEFT_TOP, CLAY_ATTACH_POINT_LEFT_TOP },
			.zIndex       = WUI_BG_SCENE_ZINDEX
		},
		.custom = { .customData = cmd }
	}) {}
}

/* Emit a CLAY CUSTOM child for an ITEM_TYPE_CONSOLE_VIEW
 * itemDef. The dispatch sigil is "console:" with no per-itemDef payload —
 * Con_DrawConsole draws the entire console UI (scrollback + input prompt +
 * autocomplete + search) using its own layout against the swapchain. */
static void wui_clay_emit_console_view_for_item( const wiredItemDef_t *item,
                                                  float x, float y, float w, float h,
                                                  const vec4_t color )
{
	wuiCustomDrawCommand_t *cmd;
	(void) item;

	cmd = wui_clay_alloc_custom_command();
	if ( !cmd ) return;
	Q_strncpyz( cmd->name, "console:", sizeof( cmd->name ) );
	cmd->rect[ 0 ]     = x;
	cmd->rect[ 1 ]     = y;
	cmd->rect[ 2 ]     = w;
	cmd->rect[ 3 ]     = h;
	Vector4Copy( color, cmd->color );
	cmd->ownerdrawFlag = 0;
	cmd->context       = NULL;

	CLAY({
		.layout = { .sizing = { CLAY_SIZING_GROW( 0 ), CLAY_SIZING_GROW( 0 ) } },
		.custom = { .customData = cmd }
	}) {}
}

/* Emit a CLAY CUSTOM child for an ITEM_TYPE_SCORELIST_WIDGET itemDef.
 * Subtype string comes from item->group ("duel" / "red" / "blue" / ""). */
static void wui_clay_emit_scorelist_widget_for_item( const wiredItemDef_t *item,
                                                      float x, float y, float w, float h,
                                                      const vec4_t color )
{
	wuiCustomDrawCommand_t *cmd;
	const char             *subtype;

	cmd = wui_clay_alloc_custom_command();
	if ( !cmd ) return;
	subtype = item->group[ 0 ] ? item->group : "";
	Com_sprintf( cmd->name, sizeof( cmd->name ), "scorelist:%s", subtype );
	cmd->rect[ 0 ]     = x;
	cmd->rect[ 1 ]     = y;
	cmd->rect[ 2 ]     = w;
	cmd->rect[ 3 ]     = h;
	Vector4Copy( color, cmd->color );
	cmd->ownerdrawFlag = 0;
	cmd->context       = NULL;

	CLAY({
		.layout = { .sizing = { CLAY_SIZING_GROW( 0 ), CLAY_SIZING_GROW( 0 ) } },
		.custom = { .customData = cmd }
	}) {}
}

/* Open the floating wrapper for an item and emit its content + children.
 *
 * Hit-test extension: each item gets an explicit Clay ID computed
 * from "<panelName>/<itemName>" (named) or "<panelName>/i<index>" (anon),
 * and the (ID, item, panel) tuple is recorded into wui_id_map for click-
 * time reverse-lookup. Stable across frames as long as tree shape is
 * stable — fine for static .wmenu definitions.
 *
 * Repeat-block extension: if the item carries a repeatBlock, it
 * expands into N rows in place of its own emission. The container item
 * itself doesn't draw — the template subtree is emitted N times with
 * per-row substitution. */

/* ── ITEM_TYPE_RADIOBUTTON segmented-control geometry (single source) ─────────
 *
 * A radio/segmented control is a MULTI drawn inline as N equal-width segments.
 * The row splits into a left label region and a right segment band; the band is
 * divided equally across the segment count. The RENDERER uses Clay relative
 * sizing (label = PERCENT(1-frac), band = PERCENT(frac), segments = GROW) so the
 * split follows the ACTUAL Clay-resolved row width — the compatibility
 * resolvedRect may be stale/zero for flex GROW rows,
 * so a pixel split off it would drift. The click HIT-TEST reproduces the SAME
 * fraction split on the item's ACTUAL rendered rect (WiredUI_ClayItemRenderedRect),
 * so draw-rect and hit-rect agree without either trusting resolvedRect. */
#define WUI_RADIO_BAND_FRACTION   0.68f   /* right portion given to the segments */

void WiredUI_RadioSegmentBand( wiredItemDef_t *item, float rowW,
                               float *outBandX, float *outBandW, float *outSegW ) {
	float bandX, bandW, segW;
	int   n;

	n = WiredUI_RadioSegmentCount( item );
	if ( n < 1 ) n = 1;

	/* Empty-label controls take the full width; label rows reserve the left
	 * region so the caption stays legible. Must match the CLAY sizing below:
	 * label = PERCENT(1-frac), band = PERCENT(frac). */
	if ( item && item->text[0] ) {
		bandX = rowW * ( 1.0f - WUI_RADIO_BAND_FRACTION );
		bandW = rowW * WUI_RADIO_BAND_FRACTION;
	} else {
		bandX = 0.0f;
		bandW = rowW;
	}
	if ( bandW < (float) n ) bandW = (float) n;   /* degenerate guard */
	segW  = bandW / (float) n;

	if ( outBandX ) *outBandX = bandX;
	if ( outBandW ) *outBandW = bandW;
	if ( outSegW  ) *outSegW  = segW;
}

int WiredUI_RadioSegmentAtX( wiredItemDef_t *item, float rowX, float rowW, float cursorX ) {
	float bandX, bandW, segW, local;
	int   n, seg;

	n = WiredUI_RadioSegmentCount( item );
	if ( n < 1 ) return -1;

	WiredUI_RadioSegmentBand( item, rowW, &bandX, &bandW, &segW );
	local = cursorX - ( rowX + bandX );
	if ( local < 0.0f || local >= bandW ) return -1;
	if ( segW <= 0.0f ) return -1;
	seg = (int)( local / segW );
	if ( seg < 0 ) seg = 0;
	if ( seg >= n ) seg = n - 1;
	return seg;
}

/* ── ITEM_TYPE_RADIOBUTTON segmented-control render ──────────────────────────
 *
 * Emits the row (label + a right-aligned horizontal band of N segment boxes).
 * Reuses the F1 framework for state: the whole control gets a keyboard focus
 * ring (WiredUI_FocusRingFor) and a DISABLED dim; per-segment the SELECTED
 * option is filled with the accent colour, the segment under the cursor gets a
 * hover fill, others rest. Physical-px sizing (× dpiScale) so segments, borders
 * and text stay crisp on HiDPI. Self-contained floating block anchored to the
 * item's clayId (mirrors wui_clay_emit_listbox) so the click hit-test can query
 * the actual rendered rect. */
static void wui_clay_emit_radio( const wiredItemDef_t *item,
                                 uint32_t clayId,
                                 float x, float y, float w, float h,
                                 Clay_Color bg, Clay_Color borderColor,
                                 uint16_t borderW,
                                 wuiVisualState_t vstate,
                                 const char *fontName,
                                 qboolean parentIsContainer ) {
	float        dpi        = WiredUI_GetDpiScale();
	int          n          = WiredUI_RadioSegmentCount( (wiredItemDef_t *) item );
	int          selIdx     = WiredUI_RadioSelectedIndex( (wiredItemDef_t *) item );
	uint16_t     fontSlot   = wui_clay_font_slot_for_face( fontName );
	uint16_t     fontSize   = (uint16_t)( item->fontPointSize > 0.0f
	                                     ? item->fontPointSize
	                                     : (uint16_t) WUI_DEFAULT_FONT_SIZE );
	qboolean     hasShadow  = wui_textstyle_has_dropshadow( item->textstyle );
	qboolean     disabled   = ( vstate == WUI_STATE_DISABLED );
	int          hoverSeg   = -1;
	int          i;
	uint16_t     segFontSize;
	int          maxLabelLen = 0;

	/* label = PERCENT(1-frac), band = PERCENT(frac): a Clay-relative split so it
	 * follows the actual resolved row width (resolvedRect is stale for flex GROW
	 * rows). The hit-test reproduces the SAME fraction on the rendered rect. */
	float        labelPct   = item->text[0] ? ( 1.0f - WUI_RADIO_BAND_FRACTION ) : 0.0f;
	float        bandPct    = item->text[0] ? WUI_RADIO_BAND_FRACTION : 1.0f;

	vec4_t   accentVec = { 0.957f, 0.627f, 0.227f, 1.0f };  /* $accent #f4a03a fallback */
	vec4_t   restVec   = { 0.16f, 0.16f, 0.18f, 0.85f };
	vec4_t   hoverVec  = { 0.28f, 0.28f, 0.32f, 0.9f };
	vec4_t   segBdVec  = { 0.45f, 0.45f, 0.5f, 0.9f };
	vec4_t   selFgVec  = { 0.06f, 0.07f, 0.09f, 1.0f };    /* dark ink on accent pill */
	Clay_Color accentBg, restBg, hoverBg, segBd, selFg;
	Clay_Color labelFg  = wui_clay_color_of( item->forecolor, wui_compositor_panel_alpha );

	vec4_t     ringCol;
	float      ringPx    = 0.0f;
	qboolean   haveRing;

	/* Selected-segment pill fill = the live theme accent; re-read per frame so
	 * it follows ui_palette_accent.
	 *
	 * FIX 2026-08-17: this read "primary_cyan" while the comment claimed it
	 * followed ui_palette_accent. It did not — the v2 accent overlays
	 * (ui/themes/<accent>/_tokens.wui) only rewrite accent/accentDim/
	 * accentSoft/accentWash, so primary_cyan stayed v1 #00b4d8 under every
	 * accent and the selected pill rendered cyan even on amber. Read `accent`,
	 * the token the overlay chain actually rewrites, and fall back to the
	 * shipped amber default rather than back to v1 cyan. */
	wui_clay_token_rgb( "accent", accentVec );
	uint16_t   ringW     = 0;
	Clay_Color ringColor = borderColor;

	wuiPixelRect_t rr;

	Clay_FloatingAttachToElement attachMode;
	Clay_SizingAxis              sizeW;

	if ( dpi <= 0.0f ) dpi = 1.0f;
	if ( n < 1 ) return;

	/* Segment text auto-shrink: N equal segments split the (fixed-fraction) band,
	 * so a control with many long-worded options (e.g. Default/Quality/Performance/
	 * Competition) would clip each label at the row's segment width. Estimate the
	 * per-segment character budget from the segment count and step the font down
	 * when the longest label overflows it, so long-label controls stay legible
	 * without shrinking the short ones (Off/2x/4x/8x keep full size). */
	for ( i = 0; i < n; i++ ) {
		const char *l = WiredUI_RadioSegmentLabel( (wiredItemDef_t *) item, i );
		int         ln = l ? (int) strlen( l ) : 0;
		if ( ln > maxLabelLen ) maxLabelLen = ln;
	}
	segFontSize = fontSize;
	/* Rough budget: at ~0.55em/char, a segment fits ~ (segWidthEm/0.55) chars.
	 * Rather than resolve the live pixel width here (stale for flex rows), gate on
	 * the count×length product that empirically clips: 4+ segments with a >7-char
	 * label, or 3 segments with a >9-char label. Two steps keep it bounded. */
	if ( ( n >= 4 && maxLabelLen > 7 ) || ( n >= 3 && maxLabelLen > 10 ) ) {
		segFontSize = (uint16_t)( fontSize > 3 ? fontSize - 2 : fontSize );
	} else if ( ( n >= 4 && maxLabelLen > 5 ) || ( n >= 3 && maxLabelLen > 8 ) ) {
		segFontSize = (uint16_t)( fontSize > 2 ? fontSize - 1 : fontSize );
	}

	/* disabled dim: fade every segment + label so the control reads greyed. */
	if ( disabled ) {
		accentVec[3] *= 0.4f; restVec[3] *= 0.4f; hoverVec[3] *= 0.4f;
		segBdVec[3] *= 0.4f;  selFgVec[3] *= 0.4f; labelFg.a *= 0.4f;
	}
	accentBg = wui_clay_color_of( accentVec, wui_compositor_panel_alpha );
	restBg   = wui_clay_color_of( restVec,   wui_compositor_panel_alpha );
	hoverBg  = wui_clay_color_of( hoverVec,  wui_compositor_panel_alpha );
	segBd    = wui_clay_color_of( segBdVec,  wui_compositor_panel_alpha );
	selFg    = wui_clay_color_of( selFgVec,  wui_compositor_panel_alpha );

	/* hover segment: hit-test the pointer against the ACTUAL rendered rect (the
	 * previous frame's Clay layout), via the SAME fraction geometry the click
	 * path uses — so hover-highlight and click select the same segment. Only when
	 * not disabled. Falls back to no hover on the very first frame. */
	if ( !disabled
	  && wui_compositor_pointer_x >= 0.0f && wui_compositor_pointer_y >= 0.0f
	  && WiredUI_ClayItemRenderedRect( NULL, item, &rr )
	  && wui_compositor_pointer_x >= rr.x && wui_compositor_pointer_x < rr.x + rr.w
	  && wui_compositor_pointer_y >= rr.y && wui_compositor_pointer_y < rr.y + rr.h ) {
		hoverSeg = WiredUI_RadioSegmentAtX( (wiredItemDef_t *) item, rr.x, rr.w, wui_compositor_pointer_x );
	}

	/* focus ring for the whole control (keyboard focus only) */
	haveRing = WiredUI_FocusRingFor( NULL, item, ringCol, &ringPx );
	if ( haveRing && ringPx > 0.0f ) {
		ringW = (uint16_t)( ringPx + 0.5f );
		if ( ringW < 1 ) ringW = 1;
		ringColor = wui_clay_color_of( ringCol, wui_compositor_panel_alpha );
	}

	attachMode = parentIsContainer ? CLAY_ATTACH_TO_NONE : CLAY_ATTACH_TO_ROOT;
	sizeW      = parentIsContainer ? CLAY_SIZING_GROW( 0 ) : CLAY_SIZING_FIXED( w );

	CLAY({
		.id     = { .id = clayId },
		.layout = {
			.sizing          = { sizeW, CLAY_SIZING_FIXED( h ) },
			.layoutDirection = CLAY_LEFT_TO_RIGHT,
			.childAlignment  = { .x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER }
		},
		.floating = {
			.attachTo     = attachMode,
			.offset       = { x, y },
			.attachPoints = { CLAY_ATTACH_POINT_LEFT_TOP, CLAY_ATTACH_POINT_LEFT_TOP },
			.pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_CAPTURE
		},
		.backgroundColor = bg,
		.border = {
			.color = ringW ? ringColor : borderColor,
			.width = { ringW ? ringW : borderW, ringW ? ringW : borderW,
			           ringW ? ringW : borderW, ringW ? ringW : borderW, 0 }
		}
	}) {
		/* Label (left region), sized PERCENT(1-frac). A clip child keeps the
		 * caption from spilling over the segment band on narrow rows. */
		if ( item->text[0] ) {
			Clay_String labelStr;
			labelStr.isStaticallyAllocated = qfalse;
			labelStr.length                = (int32_t) strlen( item->text );
			labelStr.chars                 = item->text;
			CLAY({
				.layout = {
					.sizing = { CLAY_SIZING_PERCENT( labelPct ), CLAY_SIZING_FIXED( h ) },
					.childAlignment = { .x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER },
					.padding = { (uint16_t)( 2.0f * dpi ), (uint16_t)( 6.0f * dpi ), 0, 0 }
				},
				.clip = { .horizontal = true }
			}) {
				CLAY_TEXT( labelStr, CLAY_TEXT_CONFIG({
					.userData      = hasShadow ? (void*)(uintptr_t)1 : NULL,
					.fontId        = fontSlot,
					.fontSize      = fontSize,
					.letterSpacing = 0,
					.textColor     = labelFg,
					.wrapMode      = CLAY_TEXT_WRAP_NONE,
					.textAlignment = CLAY_TEXT_ALIGN_LEFT
				}) );
			}
		}

		/* Segment band: PERCENT(frac), N equal-width GROW pills with a hairline
		 * gap so the boundaries read. Each segment GROWs to an equal share of the
		 * band — matching the WUI_RADIO_BAND_FRACTION split the hit-test uses. */
		CLAY({
			.layout = {
				.sizing          = { CLAY_SIZING_PERCENT( bandPct ), CLAY_SIZING_FIXED( h ) },
				.layoutDirection = CLAY_LEFT_TO_RIGHT,
				.childAlignment  = { .x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER },
				.padding         = { 0, 0, (uint16_t)( 4.0f * dpi ), (uint16_t)( 4.0f * dpi ) },
				.childGap        = (uint16_t)( 2.0f * dpi )
			}
		}) {
			for ( i = 0; i < n; i++ ) {
				const char *lbl     = WiredUI_RadioSegmentLabel( (wiredItemDef_t *) item, i );
				qboolean    isSel   = ( i == selIdx );
				qboolean    isHover = ( i == hoverSeg );
				Clay_Color  segBg   = isSel ? accentBg : ( isHover ? hoverBg : restBg );
				Clay_Color  segTxt  = isSel ? selFg : labelFg;
				CLAY({
					.layout = {
						.sizing          = { CLAY_SIZING_GROW( 0 ), CLAY_SIZING_GROW( 0 ) },
						.childAlignment  = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER }
					},
					.backgroundColor = segBg,
					.cornerRadius    = CLAY_CORNER_RADIUS( 4.0f * dpi ),
					.border = { .color = segBd, .width = { 1, 1, 1, 1, 0 } },
					.clip = { .horizontal = true }
				}) {
					if ( lbl && lbl[0] ) {
						Clay_String segStr;
						segStr.isStaticallyAllocated = qfalse;
						segStr.length                = (int32_t) strlen( lbl );
						segStr.chars                 = lbl;
						CLAY_TEXT( segStr, CLAY_TEXT_CONFIG({
							.userData      = hasShadow ? (void*)(uintptr_t)1 : NULL,
							.fontId        = fontSlot,
							.fontSize      = segFontSize,
							.letterSpacing = 0,
							.textColor     = segTxt,
							.wrapMode      = CLAY_TEXT_WRAP_NONE,
							.textAlignment = CLAY_TEXT_ALIGN_CENTER
						}) );
					}
				}
			}
		}
	}
}

/* ── ITEM_TYPE_LISTBOX feeder-driven render ─────────────────────────
 *
 * Floating outer container pinned to the item's rect; emits only the
 * visible cells/rows as floating children at computed offsets, with an
 * optional fading scrollbar on the scroll axis. Read-only on the item's
 * scroll/selection state — the input handlers (cl_wired_ui.c) keep
 * updating those fields. Hover is recomputed inline each frame from
 * wui_compositor_pointer_x/y vs the item rect.
 *
 * Floating-cell emission (vs Clay's native clip+childOffset scroll) is
 * deliberate: Clay's clip element with FIXED-overflowing content
 * positions children at the bottom of the clip (the default childAlignment
 * fall-through is bottom-equivalent for overflow), and forcing TOP did
 * not change the behavior. Emitting only the visible slice sidesteps
 * the issue. */

#define WUI_LISTBOX_SCROLLBAR_THICKNESS   4.0f
#define WUI_LISTBOX_SCROLLBAR_MIN_THUMB  16.0f
#define WUI_LISTBOX_SCROLL_HOLD_MS      1500
#define WUI_LISTBOX_SCROLL_FADE_MS       500
#define WUI_LISTBOX_ROW_PADDING_PX        4.0f
#define WUI_LISTBOX_COL_GAP_FALLBACK_PX   6.0f
/* Extra vertical breathing room for listbox rows on top of the shared line-box
 * factor — list rows want more air than a dense text block (tailwind: my-2).
 * Applied only to the listbox row height, not the global line-height. */
#define WUI_LISTBOX_ROW_SPACING           1.28f

/* Slider geometry (LOGICAL px, ×WiredUI_GetDpiScale() at emit — WiredUI is
 * physical-pixel Clay space). The track bar stays thin; the thumb is a taller,
 * wider handle centred on the value so the grab target is obvious and the fill
 * boundary reads clearly. The track element is sized to the thumb height so the
 * handle isn't clipped. */
#define WUI_SLIDER_TRACK_H_PX             8.0f
#define WUI_SLIDER_THUMB_W_PX            12.0f
#define WUI_SLIDER_THUMB_H_PX            18.0f

/* Live design-token table lookup (defined in cl_wired_parse.c). Read at
 * emit time so a mod/theme override of `listbox_col_gap` is picked up per
 * frame. */
extern const char *WiredToken_Find( const char *name );

/* Resolve the inter-column gap token into logical px, falling back to the
 * baked default when the token is missing/unparseable. atof() tolerates the
 * `px` suffix used in _tokens.wui (e.g. "6px" → 6). */
static float wui_clay_listbox_col_gap_px( void )
{
	const char *value = WiredToken_Find( "listbox_col_gap" );
	if ( value && *value ) {
		float px = (float) atof( value );
		if ( px >= 0.0f ) return px;
	}
	return WUI_LISTBOX_COL_GAP_FALLBACK_PX;
}

/* Resolve a theme token's colour by name into an RGB vec (alpha untouched).
 * Reads the token table the same way wui_clay_listbox_col_gap_px reads its
 * numeric token, then parses the "#rrggbb" (or "#rrggbbaa") string. Falls back
 * to the passed-in default RGB if the token is missing/unparseable. This lets
 * C-side render code stay theme-driven: change ui_palette_accent and every
 * consumer of the token follows, no recompile. */
static void wui_clay_token_rgb( const char *tokenName, float outRGB[3] )
{
	const char *v = WiredToken_Find( tokenName );
	unsigned    r, g, b;
	if ( v && v[0] == '#' && ( strlen( v ) == 7 || strlen( v ) == 9 )
	     && sscanf( v + 1, "%2x%2x%2x", &r, &g, &b ) == 3 ) {
		outRGB[0] = (float) r / 255.0f;
		outRGB[1] = (float) g / 255.0f;
		outRGB[2] = (float) b / 255.0f;
	}
	/* else: leave the caller's fallback RGB in place */
}

/* Selection + hover tints are the theme accent ($accent) at two alphas so the
 * two states read as distinct. RGB comes from the token (theme-driven); only
 * the alpha is authored here. Was an off-theme purple/grey hardcode.
 *
 * FIX 2026-08-17: the RGB fallbacks and the token name were v1 $primary_cyan,
 * which no v2 accent overlay rewrites — so both tints stayed cyan under every
 * accent. Fallbacks are now the shipped amber default (#f4a03a). */
static vec4_t wui_listbox_sel_color   = { 0.957f, 0.627f, 0.227f, 0.35f };
static vec4_t wui_listbox_hover_color = { 0.957f, 0.627f, 0.227f, 0.12f };

/* Refresh the accent-derived listbox tints from the current theme token.
 * Called each frame before the listbox emits (cheap: one token lookup ×2). */
static void wui_clay_refresh_listbox_theme_colors( void )
{
	wui_clay_token_rgb( "accent", wui_listbox_sel_color );
	wui_clay_token_rgb( "accent", wui_listbox_hover_color );
}

/* ── checkbox geometry (single source of truth) ──────────────────────
 *
 * Component-library F2. The checkbox box is a fixed LOGICAL size scaled to
 * PHYSICAL px by dpiScale (WiredUI is physical-pixel Clay space — every px that
 * goes to Clay must be ×dpiScale). One helper feeds the emit so the box, its
 * border, and the inner check glyph all derive from the same numbers. The box
 * self-positions as a Clay flex child in the widget row (no manual rect math /
 * hit-test needed: the row-level click resolves the item, like yesno). */
#define WUI_CHECKBOX_LOGICAL_PX   16.0f   /* box edge, logical */
#define WUI_CHECKBOX_BORDER_PX     1.5f   /* box outline, logical */
#define WUI_CHECKBOX_INSET_FRAC    0.28f  /* check-glyph inset as fraction of box */

/* Spinner stepper geometry (logical px, ×dpiScale at emit). The −/+ buttons are
 * square; the numeric value field sits between them with a fixed logical width so
 * the two buttons flank a stable-width readout. */
#define WUI_SPINNER_BTN_LOGICAL_PX    22.0f   /* stepper button edge, logical */
#define WUI_SPINNER_VALUE_LOGICAL_PX  56.0f   /* value field width, logical */
#define WUI_SPINNER_BORDER_PX          1.5f   /* button/field outline, logical */
#define WUI_SPINNER_RADIUS_PX          4.0f   /* corner radius, logical */

typedef struct {
	float boxPx;      /* physical box edge */
	float borderPx;   /* physical outline width */
	float insetPx;    /* physical inset of the inner check mark */
	float checkPx;    /* physical inner check-mark edge */
} wuiCheckboxGeom_t;

static void wui_checkbox_geom( float dpi, wuiCheckboxGeom_t *out ) {
	if ( dpi <= 0.0f ) dpi = 1.0f;
	out->boxPx    = WUI_CHECKBOX_LOGICAL_PX * dpi;
	out->borderPx = WUI_CHECKBOX_BORDER_PX  * dpi;
	if ( out->borderPx < 1.0f ) out->borderPx = 1.0f;
	out->insetPx  = out->boxPx * WUI_CHECKBOX_INSET_FRAC;
	out->checkPx  = out->boxPx - 2.0f * out->insetPx;
	if ( out->checkPx < 1.0f ) out->checkPx = 1.0f;
}

/* ── unified multi-column geometry (single source of truth) ──────────
 *
 * One column model shared by the header band, every body row, the full-row
 * highlight span, the col-gap spacers, and the scrollbar's right edge — so a
 * real list-view: header cell #i and body cell #i always share the same left
 * edge and width. Every consumer that used to recompute per-column widths
 * inline now calls this instead.
 *
 * All values are PHYSICAL pixels (Clay space). The caller passes contentW
 * (already physical) and the physical col-gap; the returned x[] offsets are
 * relative to the listbox's inner origin (0 = left of the row block). The
 * leading WUI_LISTBOX_ROW_PADDING_PX indent and the inter-column gaps are
 * baked into the offsets so both header and body honour them identically.
 *
 * Returns the row span (right edge of the last column incl. the leading
 * indent) BEFORE clamping to contentW — the caller clamps for the highlight /
 * scrollbar anchor. Fills colX[]/colW[] for `numColumns` columns. */
typedef struct {
	float x;   /* left edge (physical px, relative to row block origin) */
	float w;   /* width (physical px) */
} wuiColGeom_t;

static float wui_listbox_column_geom( const wiredItemDef_t *item,
                                      int numColumns,
                                      float contentW,
                                      float colGap,
                                      wuiColGeom_t out[8] )
{
	float cursor = WUI_LISTBOX_ROW_PADDING_PX;
	int   c;
	int   hasFill = 0;   /* any width-0 "fill remaining" column authored? */
	float fixedSum = 0;  /* sum of the authored fixed column widths */
	float scale    = 1.0f;

	if ( numColumns > 8 ) numColumns = 8;

	/* Pre-pass: classify columns. A width-0 column is an explicit "fill the
	 * remainder" flex column (handled inline below). If NO column asks to
	 * fill but the fixed columns together span LESS than the available
	 * contentW, scale every fixed column up proportionally so the row span
	 * fills the listbox — otherwise the columns (and the selected-row
	 * highlight + header band, all of which read this geometry) hug the left
	 * edge and leave dead space on the right. This is the general list-view
	 * stretch: every listbox fills its box whether or not the .wui author
	 * left a width-0 fill column (maplist authors fixed pixel widths; the
	 * servers browser leaves a width-0 column and takes the inline path). */
	for ( c = 0; c < numColumns; c++ ) {
		if ( c < item->columns && item->columnWidths[ c ] > 0 )
			fixedSum += (float) item->columnWidths[ c ];
		else
			hasFill = 1;
	}
	if ( !hasFill && fixedSum > 0.0f ) {
		float gapSum   = ( numColumns > 1 ) ? colGap * (float)( numColumns - 1 ) : 0.0f;
		float availFor = contentW - WUI_LISTBOX_ROW_PADDING_PX - gapSum;
		if ( availFor > fixedSum )
			scale = availFor / fixedSum;
	}

	for ( c = 0; c < numColumns; c++ ) {
		float cellW = ( c < item->columns && item->columnWidths[ c ] > 0 )
		            ? (float) item->columnWidths[ c ] * scale
		            : ( contentW - cursor );
		if ( cellW < 1.0f ) cellW = 1.0f;

		if ( out ) {
			out[ c ].x = cursor;
			out[ c ].w = cellW;
		}

		cursor += cellW;
		if ( c + 1 < numColumns ) cursor += colGap;
	}

	return cursor;   /* = row span before contentW clamp */
}

/* ── header band public helpers (used by the click hit-test) ─────────
 *
 * These mirror the emit-side header geometry so a header click resolves to the
 * exact column the label is drawn over. Physical-px throughout. */
float WiredUI_ListboxHeaderHeight( const wiredItemDef_t *item )
{
	float dpi, charSize, rowHFloor, rowH;

	if ( !item || item->columnHeaderCount <= 0 ) return 0.0f;

	dpi       = WiredUI_GetDpiScale();
	charSize  = item->fontPointSize > 0.0f ? item->fontPointSize : WUI_DEFAULT_FONT_SIZE;
	rowHFloor = charSize * WUI_LINE_HEIGHT_FACTOR * WUI_LISTBOX_ROW_SPACING * dpi;
	rowH      = item->elementheight > 0 ? item->elementheight * dpi : rowHFloor;
	if ( rowH < rowHFloor ) rowH = rowHFloor;
	return rowH;   /* header band == one row tall (matches emit) */
}

int WiredUI_ListboxHeaderColumnAtX( const wiredItemDef_t *item,
                                    float listX, float listW, float cursorX )
{
	int          numColumns = item->columns > 0 ? item->columns : 1;
	float        dpi        = WiredUI_GetDpiScale();
	float        colGap     = wui_clay_listbox_col_gap_px() * dpi;
	float        local      = cursorX - listX;
	wuiColGeom_t geom[8];
	int          c;

	if ( numColumns > 8 ) numColumns = 8;

	/* Same geometry the header band is drawn with. contentW only affects a
	 * 0-width column's fallback; the header columns carry explicit widths, so
	 * listW is a safe contentW here. */
	wui_listbox_column_geom( item, numColumns, listW, colGap, geom );

	for ( c = 0; c < numColumns; c++ ) {
		if ( local >= geom[ c ].x && local < geom[ c ].x + geom[ c ].w + colGap ) {
			return c;
		}
	}
	return -1;
}

/* Single-source vertical-listbox scrollbar-thumb geometry (physical px).
 *
 * Recomputes the SAME thumb + track band the vertical listbox scrollbar is
 * DRAWN from (wui_clay_emit_listbox, "macOS-style: pin the scrollbar to the FAR
 * RIGHT" block), given the listbox's on-screen rect (listX/listY/listW/listH,
 * from WiredUI_ClayItemRenderedRect) and its feeder item count. Returns qtrue
 * and fills *outThumb (absolute screen rect) + *outTrackTop / *outTrackTravel
 * (the thumb's vertical range: trackTop..trackTop+travel maps scroll 0..max)
 * when the list overflows and a thumb is shown; qfalse otherwise.
 *
 * Mirrored formulas (kept in lockstep with the emit by cross-reference, exactly
 * as the row-click hit-test recomputes rowH independently — grep
 * WUI_LISTBOX_SCROLLBAR in both places when touching either). Horizontal-scroll
 * listboxes are not draggable here (they use a bottom-edge bar); returns qfalse.
 */
qboolean WiredUI_ListboxScrollbarGeom( const wiredItemDef_t *item,
                                       float listX, float listY,
                                       float listW, float listH,
                                       wuiPixelRect_t *outThumb,
                                       float *outTrackTop, float *outTrackTravel )
{
	float dpi, charSize, rowHFloor, rowH, headerH, contentW, colGap, rowSpan;
	int   numColumns, totalItems, visibleRows, maxScrollI, scrollOff;
	float trackH, thumbH, thumbY, travel;
	wuiColGeom_t geom[8];

	if ( !item || item->type != ITEM_TYPE_LISTBOX || item->feeder == 0 ) return qfalse;
	if ( item->horizontalScroll ) return qfalse;   /* vertical thumbs only */

	totalItems = WiredUI_FeederCount( (int) item->feeder );
	if ( totalItems <= 0 ) return qfalse;

	dpi       = WiredUI_GetDpiScale();
	charSize  = item->fontPointSize > 0.0f ? item->fontPointSize : WUI_DEFAULT_FONT_SIZE;
	rowHFloor = charSize * WUI_LINE_HEIGHT_FACTOR * WUI_LISTBOX_ROW_SPACING * dpi;
	rowH      = item->elementheight > 0 ? item->elementheight * dpi : rowHFloor;
	if ( rowH < rowHFloor ) rowH = rowHFloor;

	headerH     = WiredUI_ListboxHeaderHeight( item );
	visibleRows = rowH > 0.0f ? (int)( ( listH - headerH ) / rowH ) : 0;
	if ( visibleRows < 1 ) visibleRows = 1;
	if ( totalItems <= visibleRows ) return qfalse;   /* no overflow, no thumb */

	/* contentW/rowSpan reproduce the emit's right-edge anchor (rows draw out to
	 * rowSpan, the "listbox draws wider than its flex box" quirk). */
	numColumns = item->columns > 0 ? item->columns : 1;
	if ( numColumns > 8 ) numColumns = 8;
	contentW   = listW - ( WUI_LISTBOX_SCROLLBAR_THICKNESS + 2.0f );
	if ( contentW < 1.0f ) contentW = 1.0f;
	colGap     = wui_clay_listbox_col_gap_px() * dpi;
	rowSpan    = wui_listbox_column_geom( item, numColumns, contentW, colGap, geom );
	if ( rowSpan < contentW ) rowSpan = contentW;

	maxScrollI = totalItems - visibleRows;
	if ( maxScrollI < 0 ) maxScrollI = 0;
	scrollOff  = Com_Clampi( 0, maxScrollI, item->listScrollOffset );

	trackH = listH - headerH - 2.0f;
	if ( trackH < 1.0f ) trackH = 1.0f;
	thumbH = trackH * ( (float) visibleRows / (float) totalItems );
	if ( thumbH < WUI_LISTBOX_SCROLLBAR_MIN_THUMB ) thumbH = WUI_LISTBOX_SCROLLBAR_MIN_THUMB;
	travel = trackH - thumbH;
	if ( travel < 0.0f ) travel = 0.0f;
	thumbY = headerH + 1.0f;
	if ( maxScrollI > 0 ) thumbY += travel * ( (float) scrollOff / (float) maxScrollI );

	if ( outThumb ) {
		outThumb->x = listX + ( rowSpan - WUI_LISTBOX_SCROLLBAR_THICKNESS - 1.0f );
		outThumb->y = listY + thumbY;
		outThumb->w = WUI_LISTBOX_SCROLLBAR_THICKNESS;
		outThumb->h = thumbH;
	}
	if ( outTrackTop )    *outTrackTop    = listY + ( headerH + 1.0f );
	if ( outTrackTravel ) *outTrackTravel = travel;
	return qtrue;
}

static float wui_clay_listbox_fade_alpha( int fadeTimeMs )
{
	int   elapsed;
	float alpha;

	if ( fadeTimeMs <= 0 ) return 0.0f;
	elapsed = cls.realtime - fadeTimeMs;
	if ( elapsed < WUI_LISTBOX_SCROLL_HOLD_MS ) return 1.0f;
	alpha = 1.0f - (float)( elapsed - WUI_LISTBOX_SCROLL_HOLD_MS ) / (float) WUI_LISTBOX_SCROLL_FADE_MS;
	return alpha < 0.0f ? 0.0f : alpha;
}

static void wui_clay_emit_floating_rect( float ox, float oy, float w, float h, Clay_Color color )
{
	CLAY({
		.layout = { .sizing = { CLAY_SIZING_FIXED( w ), CLAY_SIZING_FIXED( h ) } },
		.floating = {
			.attachTo     = CLAY_ATTACH_TO_PARENT,
			.offset       = { ox, oy },
			.attachPoints = { CLAY_ATTACH_POINT_LEFT_TOP, CLAY_ATTACH_POINT_LEFT_TOP },
			.pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH
		},
		.backgroundColor = color
	}) {}
}

/* Timestamp (cls.realtime ms) of the last scroll activity on any flex scroll
 * container; drives the macOS-style thumb fade. Set by the wheel path
 * (WiredUI_CompositorMouseWheel) and by hover so the affordance surfaces on
 * approach. Shared across containers — only one panel_content scrolls at a
 * time in the settings UI; a per-container map is later work if multiple
 * simultaneous scroll viewports appear. */
static int wui_clay_flexScrollFadeTime = 0;

/* Pending vertical wheel delta accumulated between frames. The wheel arrives
 * during input handling (WiredUI_CompositorMouseWheel), OUTSIDE the layout
 * frame. Applying it to Clay right there is a bug: Clay_UpdateScrollContainers
 * evicts any scroll container whose openThisFrame flag is false (clay.h
 * ~4053), and that flag is only set during layout — so an out-of-frame update
 * removes the container and discards the delta. Instead we stash the delta
 * here and feed it to the SINGLE in-frame Clay_UpdateScrollContainers call
 * (right after Clay_SetPointerState, with fresh pointerOverIds). */
static float wui_clay_pendingWheelDeltaY = 0.0f;

/* Pending ABSOLUTE scroll-position request for a specific container, set by the
 * scrollbar drag + PageUp/PageDown keyboard handlers (which know exactly where
 * they want the content, not a wheel delta). clayId==0 = none. reqScrollY is a
 * target scrollPosition.y (<= 0, Clay convention); it is clamped to the
 * container's range and applied in the SAME in-frame Clay_UpdateScrollContainers
 * window as the wheel delta (out-of-frame mutation races Clay's eviction). */
static uint32_t wui_clay_pendingScrollId  = 0;
static float    wui_clay_pendingScrollY   = 0.0f;

/* Per-scroll-container state carried across frames. Clay only computes a
 * container's contentSize during Clay_EndLayout, so it is NOT available during
 * the emit pass that lays the container out. We therefore snapshot each
 * container's extents AFTER EndLayout (wui_clay_refresh_scroll_cache) and read
 * that ONE-FRAME-STALE snapshot when emitting the scrollbar and when the wheel
 * handler decides overflow — imperceptible, and it sidesteps the emit-time
 * contentSize==0 trap. Also holds the container's screen rect for wheel hit-
 * testing. Small fixed cap; the settings UI has a single scroll viewport. */
#define WUI_MAX_SCROLL_RECTS 8
typedef struct {
	uint32_t clayId;
	float    x, y, w, h;   /* viewport screen rect (physical px) */
	float    contentH;     /* content extent (from last EndLayout) */
	float    viewH;        /* viewport height  (from last EndLayout) */
	float    viewW;        /* viewport width   (from last EndLayout) — Clay's
	                        * computed box width, which can differ from the emit
	                        * resolvedRect width; the thumb hugs THIS edge */
	float    scrollY;      /* cached scroll position (<= 0); authoritative copy
	                        * because Clay_GetScrollContainerData is unreadable
	                        * mid-emit (its layoutElement is reset by BeginLayout
	                        * until the element re-registers) */
	qboolean overflow;     /* contentH > viewH */
	/* Absolute screen rect (physical px) of the scrollbar THUMB, recorded each
	 * frame by the scrollbar emit. Used by the drag hit-test (thumb click) and
	 * by the drag update (cursorY→scroll mapping). thumbH is 0 until the thumb
	 * has painted at least once (drag start requires thumbH>0). */
	float    thumbX, thumbY, thumbW, thumbH;
	float    trackY, trackH;   /* thumb travel band, absolute screen px */
} wui_scroll_entry_t;
static wui_scroll_entry_t wui_scroll_rects[ WUI_MAX_SCROLL_RECTS ];
static int                wui_scroll_rect_count = 0;

/* Active flex-scroll-container thumb drag. clayId==0 = not dragging. grabDY is
 * the cursor's offset within the thumb at grab time (so the thumb does not jump
 * its top to the cursor). Mirrors the wui_sliderDragging pattern in
 * cl_wired_ui.c — the drag lives here because the scroll state does. */
static uint32_t wui_scroll_drag_clayId = 0;
static float    wui_scroll_drag_grabDY = 0.0f;

/* Find (or create) the cache entry for a scroll container by id. */
static wui_scroll_entry_t *wui_scroll_entry_for( uint32_t clayId )
{
	int i;
	for ( i = 0; i < wui_scroll_rect_count; i++ ) {
		if ( wui_scroll_rects[ i ].clayId == clayId ) return &wui_scroll_rects[ i ];
	}
	if ( wui_scroll_rect_count < WUI_MAX_SCROLL_RECTS ) {
		wui_scroll_entry_t *e = &wui_scroll_rects[ wui_scroll_rect_count++ ];
		memset( e, 0, sizeof( *e ) );
		e->clayId = clayId;
		return e;
	}
	return NULL;
}

/* Refresh every registered scroll container's extents from Clay AFTER
 * Clay_EndLayout (when contentSize is valid). Called once per frame from the
 * emit driver. Leaves the screen rect (set during emit) intact. */
static void wui_clay_refresh_scroll_cache( void )
{
	int i;
	for ( i = 0; i < wui_scroll_rect_count; i++ ) {
		wui_scroll_entry_t      *e = &wui_scroll_rects[ i ];
		Clay_ElementId           eid = { .id = e->clayId };
		Clay_ScrollContainerData scd = Clay_GetScrollContainerData( eid );
		if ( scd.found ) {
			e->contentH = scd.contentDimensions.height;
			e->viewH    = scd.scrollContainerDimensions.height;
			e->viewW    = scd.scrollContainerDimensions.width;
			e->overflow = ( e->contentH > e->viewH + 0.5f );
			if ( scd.scrollPosition ) e->scrollY = scd.scrollPosition->y;
		} else {
			e->overflow = qfalse;
		}
	}
}

/* Live scroll offset for a scroll container by id, for the `.clip.childOffset`
 * config. Must be looked up by id (not Clay_GetScrollOffset(), which reads the
 * currently-open element whose id is still 0 at config-eval time). */
static Clay_Vector2 wui_clay_scroll_offset_for( uint32_t clayId )
{
	/* Read the cached scroll position, NOT Clay_GetScrollContainerData: mid-emit
	 * (before this element's ConfigureOpenElement re-registers it) the
	 * container's layoutElement is stale from Clay_BeginLayout's ephemeral
	 * reset, so Clay_GetScrollContainerData reports found=false and 0. The cache
	 * carries the authoritative scrollY snapshotted after the last EndLayout /
	 * updated by the wheel mutation. */
	int i;
	for ( i = 0; i < wui_scroll_rect_count; i++ ) {
		if ( wui_scroll_rects[ i ].clayId == clayId ) {
			return (Clay_Vector2){ 0.0f, wui_scroll_rects[ i ].scrollY };
		}
	}
	return (Clay_Vector2){ 0.0f, 0.0f };
}

/* macOS-style right-edge scrollbar for a flex `.clip` scroll container. Must
 * be called from INSIDE the scroll container's CLAY block so the floating
 * thumb attaches to it (and, being floating with the default CLAY_CLIP_TO_NONE,
 * is neither scissored nor shifted by childOffset — it stays pinned to the
 * viewport while the content scrolls beneath it).
 *
 * Reads the container's extents AND scroll position from the cache (both are
 * unreadable via Clay_GetScrollContainerData mid-emit — contentSize is only set
 * at EndLayout, and the container's layoutElement is reset by BeginLayout until
 * it re-registers). Registers the container so the cache refresh + wheel hit-
 * test can find it. Emits nothing unless the content overflows. */
static void wui_clay_emit_flex_scrollbar( uint32_t clayId, float x, float y, float w, float h )
{
	wui_scroll_entry_t      *e;
	float                    contentH, viewH, viewportOffY;
	float                    trackH, thumbH, thumbY, maxScroll, scrollPos, alpha;
	vec4_t                   thumbVec;

	e = wui_scroll_entry_for( clayId );
	if ( !e ) return;
	/* Record this frame's screen rect for wheel hit-testing regardless of
	 * overflow (cheap; overflow gates the actual capture below). */
	e->x = x; e->y = y; e->w = w; e->h = h;

	/* Overflow decision uses the stale-but-correct cached extents. On the very
	 * first frame the cache is empty (overflow=false) → no scrollbar for one
	 * frame, then it appears. */
	if ( !e->overflow ) return;

	contentH = e->contentH;
	viewH    = e->viewH > 0.0f ? e->viewH : h;
	/* Thumb hugs Clay's computed right edge (viewW), which can be narrower than
	 * the emit resolvedRect width (w) — using w would push the thumb past the
	 * rendered box. Fall back to w if Clay's width is not yet cached. */
	{
		float barW = e->viewW > 0.0f ? e->viewW : w;
		w = barW;
	}

	/* macOS-style visibility: a dim resting thumb whenever content overflows
	 * (so the user sees there is more), brightening to full on recent scroll
	 * then easing back to the resting alpha. */
	{
		float active = wui_clay_listbox_fade_alpha( wui_clay_flexScrollFadeTime );
		float rest   = 0.35f;
		alpha = rest + ( 1.0f - rest ) * active;
	}

	/* Thumb proportional to visible fraction, positioned by scroll progress.
	 * cached scrollY is <= 0 (Clay convention: content shifted up). */
	maxScroll = contentH - viewH;
	scrollPos = -e->scrollY;
	if ( scrollPos < 0.0f )        scrollPos = 0.0f;
	if ( scrollPos > maxScroll )   scrollPos = maxScroll;

	trackH = viewH - 4.0f;
	thumbH = trackH * ( viewH / contentH );
	if ( thumbH < WUI_LISTBOX_SCROLLBAR_MIN_THUMB ) thumbH = WUI_LISTBOX_SCROLLBAR_MIN_THUMB;
	thumbY = ( maxScroll > 0.0f )
	       ? 2.0f + ( trackH - thumbH ) * ( scrollPos / maxScroll )
	       : 2.0f;

	/* Floating offsets are relative to this element's top-left. The clip
	 * viewport's own offset is applied by Clay; a floating child measured from
	 * the parent origin lands at the viewport edge regardless of childOffset. */
	viewportOffY = 0.0f;

	/* macOS look: no visible track, thin translucent light thumb hugging the
	 * right edge. THICKNESS matches the listbox scrollbar for visual
	 * consistency. */
	thumbVec[0] = 0.85f; thumbVec[1] = 0.85f; thumbVec[2] = 0.85f;
	thumbVec[3] = 0.55f * alpha;

	wui_clay_emit_floating_rect(
		w - WUI_LISTBOX_SCROLLBAR_THICKNESS - 2.0f,
		viewportOffY + thumbY,
		WUI_LISTBOX_SCROLLBAR_THICKNESS,
		thumbH,
		wui_clay_color_of( thumbVec, wui_compositor_panel_alpha ) );

	/* Record the absolute (screen-px) thumb + track band so the drag hit-test
	 * (WiredUI_CompositorScrollbarDragStart) and drag update read the SAME
	 * geometry the thumb is drawn from — single-source, no draw/hit drift. The
	 * floating rect above is relative to the element origin (e->x, e->y); add it
	 * to get screen coords. A wider hit pad on X is applied at hit-test time. */
	e->thumbX = e->x + ( w - WUI_LISTBOX_SCROLLBAR_THICKNESS - 2.0f );
	e->thumbY = e->y + viewportOffY + thumbY;
	e->thumbW = WUI_LISTBOX_SCROLLBAR_THICKNESS;
	e->thumbH = thumbH;
	e->trackY = e->y + viewportOffY + 2.0f;
	e->trackH = trackH;
}

static void wui_clay_emit_listbox( const wiredItemDef_t *item,
                                    uint32_t clayId,
                                    float x, float y, float w, float h,
                                    Clay_Color bg, Clay_Color borderColor,
                                    uint16_t borderW,
                                    qboolean parentIsContainer )
{
	const wuiRect_t *layoutRect = wui_clay_effective_item_rect( item );
	int          feederID    = (int) item->feeder;
	int          totalItems  = WiredUI_FeederCount( feederID );
	float        charSize    = item->fontPointSize > 0.0f ? item->fontPointSize : WUI_DEFAULT_FONT_SIZE;
	int          numColumns  = item->columns > 0 ? item->columns : 1;
	/* Floor to the glyph line box (font size × line-height factor) so listbox
	 * rows don't overlap — the previous 16px floor was below the ~16.2px line
	 * box for the default 14px face, causing vertical collision. Rows are laid
	 * out in physical-pixel space and the row text draws at fontSize × dpiScale
	 * (see MSDF measure/draw path), so scale the authored/derived row height by
	 * the same factor — otherwise on HiDPI the 2× glyphs overflow un-scaled rows
	 * and the map/server lists overlap. Mirrors the multidropdown popup path. */
	float        dpi         = WiredUI_GetDpiScale();
	float        rowHFloor   = charSize * WUI_LINE_HEIGHT_FACTOR * dpi;
	float        rowH        = item->elementheight > 0 ? item->elementheight * dpi : rowHFloor;
	if ( rowH < rowHFloor ) rowH = rowHFloor;
	float        colW        = item->elementwidth  > 0 ? item->elementwidth  : 64.0f;
	int          scrollOff   = item->listScrollOffset;
	int          selectedRow = item->listSelectedRow;
	float        cursorX     = wui_compositor_pointer_x;
	float        cursorY     = wui_compositor_pointer_y;
	uint16_t     fontSlot;
	Clay_Color   forecolor;
	Clay_Color   selColor, hoverColor, transparent;
	int          k, c;

	/* When this ListBox sits inside a Clay flex
	 * container parent (e.g. the settings_panel pattern or the
	 * new servers.wui browser_list), switch attachTo to ATTACH_TO_NONE
	 * so Clay flex positions the ListBox inside its parent. Legacy
	 * positioning via ATTACH_TO_ROOT + offset={x,y} from resolvedRect
	 * stays the path for Path B menus (playersettings model browser,
	 * etc.) that haven't migrated to the flex shell yet. Mirrors the
	 * label-and-value branch fix.
	 *
	 * Sizing: in the flex path, resolvedRect.{w,h} cascade to
	 * zero whenever an ancestor authored `height GROW` (UNIT_AUTO →
	 * WUI_Resolve returns 0), so FIXED(w,h) here would emit a 0-size
	 * outer block and the empty-state BG / border / "no servers" copy
	 * would all vanish. GROW(0) lets the parent flex flow determine
	 * the actual size; resolvedRect.{w,h} are still used below for the
	 * row + column inner layout (which the listbox internally manages
	 * with its own pixel arithmetic — a later step can revisit if the
	 * Clay-vs-internal sizing divergence becomes visible). */
	Clay_FloatingAttachToElement attachMode = parentIsContainer
	                                        ? CLAY_ATTACH_TO_NONE
	                                        : CLAY_ATTACH_TO_ROOT;
	/* Native-flow listboxes must preserve the same authored sizing contract as
	 * generic items. Bounding GROW by the previous frame's resolvedRect made a
	 * width:PERCENT(1) picker permanently inherit a stale, narrower maximum
	 * after its parent changed size. Let Clay resolve percentages against the
	 * current parent; AUTO/GROW remains native flex growth. */
	Clay_SizingAxis sizeW;
	Clay_SizingAxis sizeH;
	if ( !parentIsContainer ) {
		sizeW = CLAY_SIZING_FIXED( w );
		sizeH = CLAY_SIZING_FIXED( h );
	} else {
		if ( layoutRect->w.unit == UNIT_NORM && layoutRect->w.value > 0.0f ) {
			sizeW = CLAY_SIZING_PERCENT( layoutRect->w.value );
		} else if ( layoutRect->w.unit == UNIT_PX ) {
			sizeW = CLAY_SIZING_FIXED( layoutRect->w.value * dpi );
		} else {
			sizeW = CLAY_SIZING_GROW( 0 );
		}

		if ( layoutRect->h.unit == UNIT_NORM && layoutRect->h.value > 0.0f ) {
			sizeH = CLAY_SIZING_PERCENT( layoutRect->h.value );
		} else if ( layoutRect->h.unit == UNIT_PX ) {
			sizeH = CLAY_SIZING_FIXED( layoutRect->h.value * dpi );
		} else {
			sizeH = CLAY_SIZING_GROW( 0 );
		}
	}
	CLAY({
		/* Tag the outer listbox block with the item's stable Clay id so the
		 * row hit-test (cl_wired_ui.c click path) can query Clay_GetElementData
		 * for the ACTUAL rendered rect. In the flex path (ATTACH_TO_NONE) the
		 * listbox is positioned by its parent's flow, so the legacy resolvedRect
		 * y/x diverge from where rows actually paint — the same legacy-vs-Clay
		 * divergence the multidropdown-anchor fix documents. */
		.id     = { .id = clayId },
		.layout = {
			.sizing = { sizeW, sizeH },
			.layoutDirection = CLAY_TOP_TO_BOTTOM,
			/* Explicit TOP alignment: Clay's default childAlignment.y
			 * falls through to bottom-equivalent for overflowing content. */
			.childAlignment  = { .x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_TOP }
		},
		.floating = {
			.attachTo     = attachMode,
			.offset       = { x, y },
			.attachPoints = { CLAY_ATTACH_POINT_LEFT_TOP, CLAY_ATTACH_POINT_LEFT_TOP },
			.pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH
		},
		.backgroundColor = bg,
		.border = { .color = borderColor, .width = { borderW, borderW, borderW, borderW, 0 } }
	}) {
		/* Never return from inside a CLAY scope: the macro's loop epilogue
		 * closes the layout element.  Call-vote can legitimately expose three
		 * empty feeders (players, teams and maps); returning here leaked one
		 * open element per empty list and corrupted Clay's layout stack.  Keep
		 * the authored empty listbox frame, and only omit its row contents. */
		if ( totalItems > 0 ) {

		/* Hoisted per-item constants — item->fontName + item->forecolor
		 * are stable across the row/cell loop. wui_clay_font_slot_for_face
		 * walks the font family registry; pulling it out saves ~N*M stricmps
		 * per frame. */
		fontSlot    = wui_clay_font_slot_for_face( item->fontName[0] ? item->fontName : NULL );
		wui_clay_refresh_listbox_theme_colors();   /* pull accent RGB from the live theme token */
		forecolor   = wui_clay_color_of( item->forecolor,        wui_compositor_panel_alpha );
		selColor    = wui_clay_color_of( wui_listbox_sel_color,   wui_compositor_panel_alpha );
		hoverColor  = wui_clay_color_of( wui_listbox_hover_color, wui_compositor_panel_alpha );
		transparent = wui_clay_color_of( colorBlack,              0.0f );

		if ( item->horizontalScroll ) {
			int   visibleCols = colW > 0.0f ? (int)( w / colW ) : 0;
			float contentH    = h;
			int   firstVis, lastVis;
			int   hoverCol    = -1;
			int   maxScrollI;

			if ( visibleCols < 1 ) visibleCols = 1;
			if ( totalItems > visibleCols ) {
				contentH -= WUI_LISTBOX_SCROLLBAR_THICKNESS + 2.0f;
				if ( contentH < 1.0f ) contentH = 1.0f;
			}

			/* Clamp scroll into range — input handlers can race past
			 * limits when totalItems shrinks (e.g. characters reload). */
			maxScrollI = totalItems - visibleCols;
			if ( maxScrollI < 0 ) maxScrollI = 0;
			scrollOff = Com_Clampi( 0, maxScrollI, scrollOff );
			firstVis  = scrollOff;
			lastVis   = firstVis + visibleCols;
			if ( lastVis > totalItems ) lastVis = totalItems;

			if ( cursorX >= x && cursorX < x + w &&
			     cursorY >= y && cursorY < y + contentH ) {
				hoverCol = (int)( ( cursorX - x ) / colW );
				if ( hoverCol < 0 || hoverCol + scrollOff >= totalItems ) hoverCol = -1;
			}

			for ( k = firstVis; k < lastVis; k++ ) {
				float       cellX  = (float)( k - firstVis ) * colW;
				Clay_Color  cellBg = transparent;
				qhandle_t   icon   = 0;
				const char *text   = NULL;

				if ( k == selectedRow )                   cellBg = selColor;
				else if ( ( k - scrollOff ) == hoverCol ) cellBg = hoverColor;

				if ( item->elementtype == LISTBOX_IMAGE ) {
					icon = WiredUI_FeederItemIcon( feederID, k );
				}
				if ( !icon ) {
					text = WiredUI_FeederItemText( feederID, k, 0 );
				}

				CLAY({
					.layout = {
						.sizing = { CLAY_SIZING_FIXED( colW ),
						            CLAY_SIZING_FIXED( contentH ) },
						.layoutDirection = CLAY_LEFT_TO_RIGHT,
						.childAlignment  = { .x = CLAY_ALIGN_X_CENTER,
						                     .y = CLAY_ALIGN_Y_CENTER }
					},
					.floating = {
						.attachTo     = CLAY_ATTACH_TO_PARENT,
						.offset       = { cellX, 0.0f },
						.attachPoints = { CLAY_ATTACH_POINT_LEFT_TOP, CLAY_ATTACH_POINT_LEFT_TOP },
						.pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH
					},
					.backgroundColor = cellBg
				}) {
					if ( icon ) {
						float pad  = 2.0f;
						float side = colW < contentH ? colW : contentH;
						side -= pad * 2.0f;
						if ( side < 1.0f ) side = 1.0f;
						CLAY({
							.layout = { .sizing = { CLAY_SIZING_FIXED( side ),
							                         CLAY_SIZING_FIXED( side ) } },
							.image = { .imageData = (void *)(uintptr_t) icon },
							.backgroundColor = forecolor
						}) {}
					} else if ( text && text[ 0 ] ) {
						Clay_String s;
						s.isStaticallyAllocated = qfalse;
						s.length = (int32_t) strlen( text );
						s.chars  = text;
						CLAY_TEXT( s, CLAY_TEXT_CONFIG({
							.fontId        = fontSlot,
							.fontSize      = (uint16_t) charSize,
							.textColor     = forecolor,
							.wrapMode      = CLAY_TEXT_WRAP_NONE,
							.textAlignment = CLAY_TEXT_ALIGN_CENTER
						}) );
					}
				}
			}

			if ( totalItems > visibleCols ) {
				float alpha = wui_clay_listbox_fade_alpha( item->listScrollFadeTime );
				if ( alpha > 0.0f ) {
					float trackW      = w - 2.0f;
					float visibleFrac = (float) visibleCols / (float) totalItems;
					float thumbW      = trackW * visibleFrac;
					float thumbX      = 1.0f;
					float trackY      = h - WUI_LISTBOX_SCROLLBAR_THICKNESS - 1.0f;
					vec4_t trackVec   = { 0.30f, 0.30f, 0.30f, 0.15f * alpha };
					vec4_t thumbVec   = { 0.70f, 0.70f, 0.70f, 0.50f * alpha };

					if ( thumbW < WUI_LISTBOX_SCROLLBAR_MIN_THUMB ) thumbW = WUI_LISTBOX_SCROLLBAR_MIN_THUMB;
					if ( maxScrollI > 0 ) {
						thumbX += ( trackW - thumbW ) * ( (float) scrollOff / (float) maxScrollI );
					}

					wui_clay_emit_floating_rect( 1.0f,   trackY, trackW, WUI_LISTBOX_SCROLLBAR_THICKNESS,
					                              wui_clay_color_of( trackVec, wui_compositor_panel_alpha ) );
					wui_clay_emit_floating_rect( thumbX, trackY, thumbW, WUI_LISTBOX_SCROLLBAR_THICKNESS,
					                              wui_clay_color_of( thumbVec, wui_compositor_panel_alpha ) );
				}
			}
		} else {
			/* Engine-drawn header band (§7.1 list-view): when the listbox
			 * authors columnHeaders, paint a header row above the body using
			 * the SAME wui_listbox_column_geom column model, so header cell #i
			 * and body cell #i share one left edge + width. Header clicks are
			 * hit-tested in cl_wired_ui.c (map cursorX→column, fire the sort).
			 * The band reserves rowH of vertical space; body rows shift down by
			 * it and one fewer row is visible. */
			qboolean hasHeader = ( item->columnHeaderCount > 0 );
			float    headerH   = hasHeader ? rowH : 0.0f;
			/* Row count comes from the RENDERED height, not the legacy rect.
			 * In the flex path Clay grows the box to fill its parent, so `h`
			 * is only a starting guess — on the start-server map list Clay
			 * resolved 888px while `h` still said 403, and the list drew 9
			 * rows in a box with space for 22, leaving most of it empty down
			 * to the Clear Pool button. Same previous-frame bounding box the
			 * width already trusts, so the two axes now agree on which
			 * geometry is authoritative. */
			float contentH    = h;
			{
				Clay_ElementId  _hid; Clay_ElementData _hd;
				_hid.id = clayId;
				_hd = Clay_GetElementData( _hid );
				if ( _hd.found ) {
					float innerH = _hd.boundingBox.height - 2.0f * (float) borderW;
					if ( innerH > 1.0f ) contentH = innerH;
				}
			}
			int   visibleRows = rowH > 0.0f ? (int)( ( contentH - headerH ) / rowH ) : 0;
			float contentW    = w;
			int   firstVis, lastVis;

			/* The emit `w` can be the previous frame's compatibility snapshot
			 * while Clay's current layout grows the element to fill its parent.
			 * The columns / header band /
			 * selected-row highlight / scrollbar all derive their span from
			 * contentW, so trusting the stale `w` makes them hug the left edge
			 * and leave dead space on the right. Source the TRUE inner width from
			 * the element's previous-frame Clay bounding box (the same GROW(0)
			 * block emitted below, keyed by clayId) whenever it is wider — this
			 * is the actual rendered listbox width, so the row content fills it. */
			{
				Clay_ElementId  _eid; Clay_ElementData _ed;
				_eid.id = clayId;
				_ed = Clay_GetElementData( _eid );
				if ( _ed.found ) {
					/* boundingBox.width spans border-to-border; the inner content
					 * area is inset by borderW on each side.
					 *
					 * Adopt it in BOTH directions. This used to widen only
					 * (`if ( inner > contentW )`), which fixed rows hugging the
					 * left edge but left the opposite case broken: when Clay
					 * resolves the box NARROWER than the stale `w` — which is
					 * what happens once the grow is bounded — the rows kept the
					 * old wider span and painted past the panel edge onto the
					 * one beside it. The rendered box is the authority either
					 * way; `w` is only a starting guess. */
					float inner = _ed.boundingBox.width - 2.0f * (float) borderW;
					if ( inner > 1.0f ) contentW = inner;
				}
			}
			int   maxScrollI;
			int   hoverRow = -1;
			float colGap;
			float rowSpan;
			wuiColGeom_t geom[8];

			if ( visibleRows < 1 ) visibleRows = 1;
			if ( totalItems > visibleRows ) {
				contentW -= WUI_LISTBOX_SCROLLBAR_THICKNESS + 2.0f;
				if ( contentW < 1.0f ) contentW = 1.0f;
			}

			maxScrollI = totalItems - visibleRows;
			if ( maxScrollI < 0 ) maxScrollI = 0;
			scrollOff = Com_Clampi( 0, maxScrollI, scrollOff );
			firstVis  = scrollOff;
			lastVis   = firstVis + visibleRows;
			if ( lastVis > totalItems ) lastVis = totalItems;

			/* Inter-column gap (design token `listbox_col_gap`, logical px)
			 * scaled to physical pixels so it matches the row-height / font
			 * scaling on HiDPI. */
			colGap  = wui_clay_listbox_col_gap_px() * dpi;

			/* SINGLE source of truth for column geometry — the header band,
			 * every body row, the full-row highlight span, the col-gap spacers
			 * and the scrollbar's right edge all read geom[]/rowSpan from here.
			 * The per-column cells sum to a span that can exceed the listbox
			 * flex box (contentW) — the known "listbox draws wider than its
			 * flex box" quirk — so the highlight/scrollbar clamp to the wider
			 * of the span and contentW. */
			rowSpan = wui_listbox_column_geom( item, numColumns, contentW, colGap, geom );
			if ( rowSpan < contentW ) rowSpan = contentW;

			/* Per-row HOVER (vertical multi-column body). Mirrors the
			 * horizontal path's hoverCol computation on the vertical axis:
			 * the body region begins below the header band (y+headerH) and
			 * each visible row occupies rowH. Rows draw out to rowSpan (the
			 * full column span, the known "listbox draws wider than its flex
			 * box" quirk), so hit-test X against rowSpan — matching where the
			 * row backgrounds actually paint. cursorX/cursorY are the
			 * compositor pointer in physical px, same space as x/y/rowH.
			 * Selected row wins over hover below (selectedRow paints last),
			 * so a hovered-but-unselected row lights faintly and the selected
			 * row keeps its stronger fill. */
			/* Hit-test against the RENDERED box, not the legacy rect. Rows are
			 * positioned relative to their container (rowY is an offset inside
			 * the CLAY block), so the drawing lives in Clay's coordinate space
			 * while `x`/`y` are the pre-layout guess. On the start-server map
			 * list the two were 493px apart vertically — about twelve rows —
			 * so the highlight tracked a row nowhere near the cursor. */
			{
				float           hitX = x, hitY = y, hitH = h;
				Clay_ElementId  _hvid; Clay_ElementData _hvd;
				_hvid.id = clayId;
				_hvd = Clay_GetElementData( _hvid );
				if ( _hvd.found && _hvd.boundingBox.height > 1.0f ) {
					hitX = _hvd.boundingBox.x + (float) borderW;
					hitY = _hvd.boundingBox.y + (float) borderW;
					hitH = _hvd.boundingBox.height - 2.0f * (float) borderW;
				}
				if ( cursorX >= hitX && cursorX < hitX + rowSpan &&
				     cursorY >= hitY + headerH && cursorY < hitY + hitH ) {
					int hv = firstVis + (int)( ( cursorY - ( hitY + headerH ) ) / rowH );
					if ( hv >= firstVis && hv < lastVis && hv < totalItems ) {
						hoverRow = hv;
					}
				}
			}

			/* ── header band ─────────────────────────────────────────── */
			if ( hasHeader ) {
				int   activeSortCol = -1, activeSortDir = 0;
				vec4_t hdrBgVec   = { 1.0f, 1.0f, 1.0f, 0.05f };
				/* Active-sort column header text = the theme accent ($accent),
				 * the same active-state hue the segmented control uses.
				 * Readable on the dark header band. NOT selColor: that is the
				 * selected-ROW fill (a low-alpha wash of the same accent) and
				 * reads as invisible dark-on-dark on the header band.
				 *
				 * FIX 2026-08-17: was $primary_cyan, a v1 token no accent
				 * overlay rewrites — the active-sort header stayed cyan under
				 * every accent. Fallback is the shipped amber default. */
				vec4_t     hdrAccentVec = { 0.957f, 0.627f, 0.227f, 1.0f };  /* $accent #f4a03a fallback */
				wui_clay_token_rgb( "accent", hdrAccentVec );                /* theme-driven; follows ui_palette_accent */
				Clay_Color hdrBg     = wui_clay_color_of( hdrBgVec, wui_compositor_panel_alpha );
				Clay_Color hdrAccent = wui_clay_color_of( hdrAccentVec, wui_compositor_panel_alpha );

				WiredFeeder_ActiveSort( feederID, &activeSortCol, &activeSortDir );

				CLAY({
					.layout = {
						.sizing = { CLAY_SIZING_FIXED( rowSpan ),
						            CLAY_SIZING_FIXED( headerH ) },
						.layoutDirection = CLAY_LEFT_TO_RIGHT,
						.childAlignment  = { .x = CLAY_ALIGN_X_LEFT,
						                     .y = CLAY_ALIGN_Y_CENTER }
					},
					.floating = {
						.attachTo     = CLAY_ATTACH_TO_PARENT,
						.offset       = { 0.0f, 0.0f },
						.attachPoints = { CLAY_ATTACH_POINT_LEFT_TOP, CLAY_ATTACH_POINT_LEFT_TOP },
						.pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH
					},
					.backgroundColor = hdrBg
				}) {
					for ( c = 0; c < numColumns; c++ ) {
						const char *label = ( c < item->columnHeaderCount )
						                  ? item->columnHeaders[ c ] : "";
						float cellW = geom[ c ].w;

						CLAY({
							.layout = {
								.sizing = { CLAY_SIZING_FIXED( cellW ),
								            CLAY_SIZING_FIXED( headerH ) },
								.layoutDirection = CLAY_LEFT_TO_RIGHT,
								.childAlignment  = { .x = CLAY_ALIGN_X_LEFT,
								                     .y = CLAY_ALIGN_Y_CENTER }
							}
						}) {
							if ( label && label[ 0 ] ) {
								/* "<label> ^" for the active-sort column; the
								 * ^/v indicator sits in the active column. */
								const char *arrow = ( c == activeSortCol )
								                  ? ( activeSortDir ? " v" : " ^" ) : "";
								const char *composed = va( "%s%s", label, arrow );
								int   copyLen = (int) strlen( composed );
								char *buf     = (char *) Arena_Alloc( wui_clay_scratch_arena,
								                                     (size_t)( copyLen + 1 ),
								                                     sizeof( void * ) );
								Clay_String s;
								if ( !buf ) continue;
								Q_strncpyz( buf, composed, copyLen + 1 );
								s.isStaticallyAllocated = qfalse;
								s.length = (int32_t) copyLen;
								s.chars  = buf;
								CLAY_TEXT( s, CLAY_TEXT_CONFIG({
									.fontId        = fontSlot,
									.fontSize      = (uint16_t) charSize,
									.textColor     = ( c == activeSortCol ) ? hdrAccent : forecolor,
									.wrapMode      = CLAY_TEXT_WRAP_NONE,
									.textAlignment = CLAY_TEXT_ALIGN_LEFT
								}) );
							}
						}

						if ( colGap > 0.0f && c + 1 < numColumns ) {
							CLAY({
								.layout = { .sizing = {
									CLAY_SIZING_FIXED( colGap ),
									CLAY_SIZING_FIXED( headerH ) } }
							}) {}
						}
					}
				}
			}

			{
			for ( k = firstVis; k < lastVis; k++ ) {
				float       rowY  = headerH + (float)( k - firstVis ) * rowH;
				Clay_Color  rowBg = ( k == selectedRow ) ? selColor
				                  : ( k == hoverRow )     ? hoverColor
				                  :                         transparent;

				CLAY({
					.layout = {
						.sizing = { CLAY_SIZING_FIXED( rowSpan ),
						            CLAY_SIZING_FIXED( rowH ) },
						.layoutDirection = CLAY_LEFT_TO_RIGHT,
						.childAlignment  = { .x = CLAY_ALIGN_X_LEFT,
						                     .y = CLAY_ALIGN_Y_CENTER }
					},
					.floating = {
						.attachTo     = CLAY_ATTACH_TO_PARENT,
						.offset       = { 0.0f, rowY },
						.attachPoints = { CLAY_ATTACH_POINT_LEFT_TOP, CLAY_ATTACH_POINT_LEFT_TOP },
						.pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH
					},
					.backgroundColor = rowBg
				}) {
					/* Leading indent spacer — geom[0].x already includes the
					 * WUI_LISTBOX_ROW_PADDING_PX start offset, so the body's
					 * first cell lines up with the header's first cell. */
					CLAY({
						.layout = { .sizing = { CLAY_SIZING_FIXED( geom[ 0 ].x ),
						                         CLAY_SIZING_FIXED( rowH ) } }
					}) {}

					for ( c = 0; c < numColumns; c++ ) {
						float       cellW = geom[ c ].w;
						const char *raw = WiredUI_FeederItemText( feederID, k, c );

						CLAY({
							.layout = {
								.sizing = { CLAY_SIZING_FIXED( cellW ),
								            CLAY_SIZING_FIXED( rowH ) },
								.layoutDirection = CLAY_LEFT_TO_RIGHT,
								.childAlignment  = { .x = CLAY_ALIGN_X_LEFT,
								                     .y = CLAY_ALIGN_Y_CENTER }
							}
						}) {
							if ( raw && raw[ 0 ] ) {
								int   maxChars = (int)( ( cellW - WUI_LISTBOX_ROW_PADDING_PX ) / charSize );
								int   srcLen   = (int) strlen( raw );
								int   copyLen  = ( srcLen > maxChars ) ? maxChars : srcLen;
								char *buf;
								Clay_String s;

								if ( maxChars < 1 ) maxChars = 1;
								if ( copyLen < 0 ) copyLen = 0;
								buf = (char *) Arena_Alloc( wui_clay_scratch_arena,
								                             (size_t)( copyLen + 1 ),
								                             sizeof( void * ) );
								if ( !buf ) break;
								Q_strncpyz( buf, raw, copyLen + 1 );
								s.isStaticallyAllocated = qfalse;
								s.length = (int32_t) copyLen;
								s.chars  = buf;
								CLAY_TEXT( s, CLAY_TEXT_CONFIG({
									.fontId        = fontSlot,
									.fontSize      = (uint16_t) charSize,
									.textColor     = forecolor,
									.wrapMode      = CLAY_TEXT_WRAP_NONE,
									.textAlignment = CLAY_TEXT_ALIGN_LEFT
								}) );
							}
						}

						/* Inter-column gap spacer (empty flex leaf), inserted
						 * between adjacent columns only - not after the last -
						 * so the columns no longer touch ("#Map", "Map|Name").
						 * Same colGap the header band + geom use. */
						if ( colGap > 0.0f && c + 1 < numColumns ) {
							CLAY({
								.layout = { .sizing = {
									CLAY_SIZING_FIXED( colGap ),
									CLAY_SIZING_FIXED( rowH ) } }
							}) {}
						}
					}
				}
			}
			}

			if ( totalItems > visibleRows ) {
				float alpha = wui_clay_listbox_fade_alpha( item->listScrollFadeTime );
				if ( alpha > 0.0f ) {
					float trackH      = h - headerH - 2.0f;
					float visibleFrac = (float) visibleRows / (float) totalItems;
					float thumbH      = trackH * visibleFrac;
					float thumbY      = headerH + 1.0f;
					/* macOS-style: pin the scrollbar to the FAR RIGHT of the
					 * whole list control. The rows draw out to `rowSpan` (the
					 * full column span, wider than the flex box `w` - the known
					 * "listbox draws wider than its flex box" quirk), so anchor
					 * the track/thumb to rowSpan's right edge rather than to the
					 * narrow `w`, which placed it just right of the "Map" column. */
					float trackX      = rowSpan - WUI_LISTBOX_SCROLLBAR_THICKNESS - 1.0f;
					vec4_t trackVec   = { 0.30f, 0.30f, 0.30f, 0.15f * alpha };
					vec4_t thumbVec   = { 0.70f, 0.70f, 0.70f, 0.50f * alpha };

					if ( trackH < 1.0f ) trackH = 1.0f;
					if ( thumbH < WUI_LISTBOX_SCROLLBAR_MIN_THUMB ) thumbH = WUI_LISTBOX_SCROLLBAR_MIN_THUMB;
					if ( maxScrollI > 0 ) {
						thumbY += ( trackH - thumbH ) * ( (float) scrollOff / (float) maxScrollI );
					}

					wui_clay_emit_floating_rect( trackX, headerH + 1.0f, WUI_LISTBOX_SCROLLBAR_THICKNESS, trackH,
					                              wui_clay_color_of( trackVec, wui_compositor_panel_alpha ) );
					wui_clay_emit_floating_rect( trackX, thumbY, WUI_LISTBOX_SCROLLBAR_THICKNESS, thumbH,
					                              wui_clay_color_of( thumbVec, wui_compositor_panel_alpha ) );
				}
			}
		}
		}
	}
}

/* ── transient multi-dropdown overlay ───────────────────────────────
 *
 * Restore of the multi-select dropdown panel render. It previously
 * lived in WiredUI_DrawMultiDropdown (deleted as a dead helper);
 * state machine + input handlers stayed alive in cl_wired_ui.c. This
 * helper emits the floating panel as its own Clay layout pass (called
 * from WiredUI_CompositorEmitFrame after the per-panel walk so it lands
 * above every visible panel).
 *
 * Snapshot data — geometry, options, hover, selected, scroll — comes
 * from WiredUI_QueryMultiDropdownRender. Floating-cell pattern parallel
 * to wui_clay_emit_listbox: only the visible row slice is emitted, each
 * row a floating Clay element pinned to the outer at computed Y. */
static void wui_clay_emit_multidropdown( void )
{
	wuiMultiDropdownRender_t info;
	float                    contentW;
	int                      firstVis, lastVis;
	int                      k;
	int                      maxScrollI;
	int                      scrollOff;

	const vec4_t bgVec     = { 0.04f, 0.04f, 0.06f, 0.95f };
	const vec4_t borderVec = { 0.50f, 0.50f, 0.50f, 1.00f };
	const vec4_t fgVec     = { 1.00f, 1.00f, 1.00f, 1.00f };

	WiredUI_QueryMultiDropdownRender( &info );
	if ( !info.open ) return;
	if ( info.rowH <= 0.0f || info.visibleRows <= 0 ) return;

	contentW = info.w;
	if ( info.optionCount > info.visibleRows ) {
		contentW -= WUI_LISTBOX_SCROLLBAR_THICKNESS + 2.0f;
		if ( contentW < 1.0f ) contentW = 1.0f;
	}

	maxScrollI = info.optionCount - info.visibleRows;
	if ( maxScrollI < 0 ) maxScrollI = 0;
	scrollOff = Com_Clampi( 0, maxScrollI, info.scrollOffset );
	firstVis  = scrollOff;
	lastVis   = firstVis + info.visibleRows;
	if ( lastVis > info.optionCount ) lastVis = info.optionCount;

	{
		Clay_Color bg          = wui_clay_color_of( bgVec,     wui_compositor_panel_alpha );
		Clay_Color borderColor = wui_clay_color_of( borderVec, wui_compositor_panel_alpha );
		Clay_Color forecolor   = wui_clay_color_of( fgVec,     wui_compositor_panel_alpha );
		Clay_Color selColor;
		Clay_Color hoverColor;
		Clay_Color transparent = wui_clay_color_of( colorBlack,              0.0f );

		wui_clay_refresh_listbox_theme_colors();   /* accent RGB from the live theme token */
		selColor   = wui_clay_color_of( wui_listbox_sel_color,   wui_compositor_panel_alpha );
		hoverColor = wui_clay_color_of( wui_listbox_hover_color, wui_compositor_panel_alpha );
		uint16_t   fontSlot    = wui_clay_font_slot_for_face( NULL );

		/* The overlay is emitted in its OWN Clay_BeginLayout/EndLayout pass,
		 * separate from the settings panel that declared the clicked row. A Clay
		 * floating element may only ATTACH_TO_ELEMENT_WITH_ID an id declared in
		 * the SAME pass. A settings cvar row is a NAMED item, so its id still
		 * resolves in Clay's persistent hashmap across passes — but it points at
		 * a layoutElement from the PRIOR pass, so Clay's clip-id lookup indexes
		 * this pass's (much shorter) clip array out of bounds → error type=7
		 * (clay.h:2088) the moment the dropdown opens. So attach to ROOT and
		 * place the popup with the CPU-computed, screen-clamped info.x/info.y
		 * (WiredUI_GetMultiDropdownRect already positions the rect under the row
		 * and clamps it to the viewport, including flip-up-if-offscreen). */
		CLAY({
			.layout = {
				.sizing = { CLAY_SIZING_FIXED( info.w ), CLAY_SIZING_FIXED( info.h ) },
				.layoutDirection = CLAY_TOP_TO_BOTTOM,
				.childAlignment  = { .x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_TOP }
			},
			.floating = {
				.attachTo     = CLAY_ATTACH_TO_ROOT,
				.parentId     = 0,
				.offset       = { info.x, info.y },
				.attachPoints = { CLAY_ATTACH_POINT_LEFT_TOP, CLAY_ATTACH_POINT_LEFT_TOP },
				.pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH
			},
			.backgroundColor = bg,
			.border = { .color = borderColor, .width = { 1, 1, 1, 1, 0 } }
		}) {
			for ( k = firstVis; k < lastVis; k++ ) {
				float       rowY  = (float)( k - firstVis ) * info.rowH;
				Clay_Color  rowBg = transparent;
				const char *label;

				if ( k == info.selectedRow )    rowBg = selColor;
				else if ( k == info.hoverRow )  rowBg = hoverColor;

				label = info.labels[ k ];

				CLAY({
					.layout = {
						.sizing = { CLAY_SIZING_FIXED( contentW ),
						            CLAY_SIZING_FIXED( info.rowH ) },
						.layoutDirection = CLAY_LEFT_TO_RIGHT,
						.childAlignment  = { .x = CLAY_ALIGN_X_LEFT,
						                     .y = CLAY_ALIGN_Y_CENTER },
						.padding = { (uint16_t) WUI_LISTBOX_ROW_PADDING_PX, 0, 0, 0 }
					},
					.floating = {
						.attachTo     = CLAY_ATTACH_TO_PARENT,
						.offset       = { 0.0f, rowY },
						.attachPoints = { CLAY_ATTACH_POINT_LEFT_TOP, CLAY_ATTACH_POINT_LEFT_TOP },
						.pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH
					},
					.backgroundColor = rowBg
				}) {
					if ( label && label[ 0 ] ) {
						Clay_String s;
						s.isStaticallyAllocated = qfalse;
						s.length = (int32_t) strlen( label );
						s.chars  = label;
						CLAY_TEXT( s, CLAY_TEXT_CONFIG({
							.fontId        = fontSlot,
							.fontSize      = (uint16_t) WUI_DEFAULT_FONT_SIZE,
							.textColor     = forecolor,
							.wrapMode      = CLAY_TEXT_WRAP_NONE,
							.textAlignment = CLAY_TEXT_ALIGN_LEFT
						}) );
					}
				}
			}

			if ( info.optionCount > info.visibleRows ) {
				float visibleFrac = (float) info.visibleRows / (float) info.optionCount;
				float thumbH      = info.h * visibleFrac;
				float thumbY      = 1.0f;
				float trackX      = info.w - WUI_LISTBOX_SCROLLBAR_THICKNESS - 1.0f;
				float trackH      = info.h - 2.0f;
				vec4_t trackVec   = { 0.30f, 0.30f, 0.30f, 0.30f };
				vec4_t thumbVec   = { 0.70f, 0.70f, 0.70f, 0.70f };

				if ( thumbH < WUI_LISTBOX_SCROLLBAR_MIN_THUMB ) thumbH = WUI_LISTBOX_SCROLLBAR_MIN_THUMB;
				if ( maxScrollI > 0 ) {
					thumbY += ( trackH - thumbH ) * ( (float) scrollOff / (float) maxScrollI );
				}

				wui_clay_emit_floating_rect( trackX, 1.0f,   WUI_LISTBOX_SCROLLBAR_THICKNESS, trackH,
				                              wui_clay_color_of( trackVec, wui_compositor_panel_alpha ) );
				wui_clay_emit_floating_rect( trackX, thumbY, WUI_LISTBOX_SCROLLBAR_THICKNESS, thumbH,
				                              wui_clay_color_of( thumbVec, wui_compositor_panel_alpha ) );
			}
		}
	}
}

/* Absolute-positioning classification used by the sole Clay emit path. It
 * decides whether an authored item leaves native flex flow and maps to a Clay
 * floating declaration.
 *
 * "Absolute" = explicit non-static position, OR a `decoration` leaf that authored
 * a concrete non-AUTO size (w,h>0) at a concrete non-zero position (x||y!=0) and
 * is not a grower. The three decoration guards keep legitimate flex children that
 * also author a rect (menu cards `rect 0 0 W H` + `width PERCENT`/`height FIT`)
 * on the flex path. */
static qboolean wui_clay_item_is_absolute( const wiredItemDef_t *item )
{
	const wuiRect_t *layoutRect = wui_clay_effective_item_rect( item );
	return item->position != POSITION_STATIC
	    || ( item->decoration
	         && item->flexChild.grow == 0.0f
	         && layoutRect->w.unit != UNIT_AUTO && layoutRect->w.value > 0.0f
	         && layoutRect->h.unit != UNIT_AUTO && layoutRect->h.value > 0.0f
	         && ( layoutRect->x.value != 0.0f || layoutRect->y.value != 0.0f ) );
}

static void wui_clay_emit_item( const wiredMenuDef_t *panel,
                                 const wiredItemDef_t *item,
                                 qboolean parentIsContainer,
                                 wuiLayoutDir_t parentDirection,
								 const wiredItemDef_t *perspectiveOwner )
{
	float                  x, y, w, h;
	Clay_Color             bg, borderColor;
	uint16_t               borderW;
	qboolean               hasBackground;
	/* focus-highlight gradient painted as the row element's own background. */
	qboolean               hasFocusGradient;
	qhandle_t              focusGradShader = 0;
	Clay_Color             focusGradTint = { 0, 0, 0, 0 };
	const char            *displayText = item->text;
	const char            *fontName    = item->fontName[0] ? item->fontName : NULL;
	char                   boundText[ 256 ];
	vec4_t                 effectiveFg;
	const wuiRect_t       *layoutRect;
	uint32_t               clayId;
	int                    i;

	if ( !item->visible ) return;
	if ( item->isFlexContainer && fabsf( item->perspective ) >= 0.001f ) {
		perspectiveOwner = item;
	}
	layoutRect = wui_clay_effective_item_rect( item );

	/* source-attribution: surface this item for Clay error
	 * blame. Not stack-restored across nested children — leakage to
	 * the parent's continuation names a real child, acceptable for the
	 * blame purpose. The caller's per-panel walk overwrites on the
	 * next item, so leakage is bounded. */
	s_wui_emit_item = item->name[ 0 ] ? item->name : "(itemDef)";

	/* repeat container — replace this item's emission
	 * with N row emissions of the template. The container itself is
	 * an organisational concept; nothing is drawn at the container's rect. */
	if ( item->repeatBlock ) {
		wui_clay_emit_repeat_block( panel, item, perspectiveOwner );
		return;
	}

	/* if-block container — evaluate testLuaChunk via the
	 * panel's VM; if true, emit each child; if false, prune entirely. The
	 * container itself doesn't draw. */
	if ( item->ifBlock ) {
		const wiredIfBlock_t *ib  = item->ifBlock;
		wiredIfBlock_t       *ibm = (wiredIfBlock_t *) ib;
		wui_vm_kind_t         vm  = wui_clay_vm_for_panel( panel );
		qboolean              cond;
		int                   ci;

		if ( ib->testLuaChunk == WIRED_CHUNK_NOREF ) return;

		cond = wui_clay_chunk_call_bool( vm, ib->testLuaChunk, qfalse );
		if ( !cond ) {
			/* (Errors inside CallChunkBool already log once via the VM's
			 * error path; if a chunk consistently errors, the panel sees
			 * "false" and the children stay pruned.) */
			(void) ibm;
			return;
		}

		for ( ci = 0; ci < ib->childCount; ci++ ) {
			if ( ib->children[ ci ] ) {
				wui_clay_emit_item( panel, ib->children[ ci ], parentIsContainer,
					parentDirection, perspectiveOwner );
			}
		}
		return;
	}

	/* showBind / hideBind: resolve via WiredStore. These cull at emit
	 * time so neither the rectangle nor the text are drawn. */
	if ( item->showBind[0] && !wui_clay_store_truthy( item->showBind ) ) return;
	if ( item->hideBind[0] &&  wui_clay_store_truthy( item->hideBind ) ) return;

	/* cvarTest / showCvar / hideCvar: declarative cvar-driven visibility.
	 * The legacy render path culls these via WiredUI_ItemShouldRender; the
	 * Clay emit pass must do the same, otherwise a cvar-hidden item still
	 * emits its rect (and, for a viewport item, still invokes its provider).
	 * Culling here keeps the rect and any provider dormant when hidden. */
	if ( !WiredUI_ItemVisibleByCvarRules( item ) ) return;

	/* visible="lua:..." per-frame evaluation. The Lua
	 * chunk is compiled at parse time; failure here treats the item as
	 * invisible. Eval errors share the existing item->bindWarned flag for
	 * once-only logging. */
	if ( item->luaVisibleChunk != WIRED_CHUNK_NOREF ) {
		wui_vm_kind_t vm = wui_clay_vm_for_panel( panel );
		qboolean      vis = wui_clay_chunk_call_bool( vm, item->luaVisibleChunk, qfalse );
		if ( !vis ) return;
	}

	x = item->resolvedRect.x;
	y = item->resolvedRect.y;
	w = item->resolvedRect.w;
	h = item->resolvedRect.h;
	/* The old tree pre-pass used to seed these values before Clay ran. On the
	 * first frame (or immediately after a viewport change) derive a bounded
	 * authored fallback from the panel containing block. Native-flow items are
	 * still positioned and finally sized by Clay; this only supplies fixed-size
	 * custom/listbox helpers until their authoritative box is synchronized at
	 * EndLayout below. */
	if ( w <= 0.0f || h <= 0.0f ) {
		wuiPixelRect_t authored = WUI_ResolveRect( layoutRect,
			&panel->resolvedRect,
			(float) wui_clay_lastWidth,
			(float) wui_clay_lastHeight );
		if ( w <= 0.0f ) w = authored.w;
		if ( h <= 0.0f ) h = authored.h;
		if ( x == 0.0f ) x = authored.x;
		if ( y == 0.0f ) y = authored.y;
	}

	/* CSS-style per-side offset support. When the item's
	 * wuiOffset carries any declared side, override the resolvedRect
	 * coordinates relative to the parent's resolved rect. Auto-promote to
	 * POSITION_ABSOLUTE so the emit pass takes the floating path below. */
	{
		const wuiOffset_t *off = &item->wuiOffset;
		if ( off->hasTop || off->hasLeft || off->hasRight || off->hasBottom ) {
			float parentX = panel->resolvedRect.x;
			float parentY = panel->resolvedRect.y;
			float parentW = panel->resolvedRect.w > 0 ? panel->resolvedRect.w : (float)cls.glconfig.vidWidth;
			float parentH = panel->resolvedRect.h > 0 ? panel->resolvedRect.h : (float)cls.glconfig.vidHeight;
			float vpW = (float)cls.glconfig.vidWidth;
			float vpH = (float)cls.glconfig.vidHeight;
			if ( off->hasLeft ) {
				x = parentX + WUI_Resolve( off->left, parentW, vpW, vpH );
			}
			if ( off->hasTop ) {
				y = parentY + WUI_Resolve( off->top, parentH, vpW, vpH );
			}
			if ( off->hasRight ) {
				float rightPx = WUI_Resolve( off->right, parentW, vpW, vpH );
				if ( off->hasLeft ) {
					/* top + left + right + (maybe bottom) — width spans the gap */
					w = ( parentX + parentW - rightPx ) - x;
				} else {
					/* right + width (or default w from rect) — anchor item's right edge */
					if ( w <= 0 ) w = parentW * 0.25f;
					x = parentX + parentW - rightPx - w;
				}
			}
			if ( off->hasBottom ) {
				float bottomPx = WUI_Resolve( off->bottom, parentH, vpW, vpH );
				if ( off->hasTop ) {
					h = ( parentY + parentH - bottomPx ) - y;
				} else {
					if ( h <= 0 ) h = parentH * 0.25f;
					y = parentY + parentH - bottomPx - h;
				}
			}
			/* Default to parent's remaining extent when only one side is
			 * declared on the relevant axis — mirrors CSS "auto" sizing
			 * (top without bottom → height stretches to parent bottom). */
			if ( h <= 0 && off->hasTop ) {
				h = parentY + parentH - y;
			}
			if ( w <= 0 && off->hasLeft ) {
				w = parentX + parentW - x;
			}
			/* Auto-promotion to floating happens via the wui_useFloating
			 * test below; item->position is const here, the offset-set
			 * condition feeds the same decision without mutation. */
		}
	}

	/* per-side margin (Clay v0.14 has no per-child margin
	 * primitive; emulate by shrinking resolvedRect inward on each side so
	 * the item sits at its parent's flex flow position offset by margin). */
	if ( item->wuiMargin.hasAny ) {
		const wuiMargin_t *m = &item->wuiMargin;
		float parentW = panel->resolvedRect.w > 0 ? panel->resolvedRect.w : (float)cls.glconfig.vidWidth;
		float parentH = panel->resolvedRect.h > 0 ? panel->resolvedRect.h : (float)cls.glconfig.vidHeight;
		float mT = WUI_Resolve( m->top,    parentH, (float)cls.glconfig.vidWidth, (float)cls.glconfig.vidHeight );
		float mR = WUI_Resolve( m->right,  parentW, (float)cls.glconfig.vidWidth, (float)cls.glconfig.vidHeight );
		float mB = WUI_Resolve( m->bottom, parentH, (float)cls.glconfig.vidWidth, (float)cls.glconfig.vidHeight );
		float mL = WUI_Resolve( m->left,   parentW, (float)cls.glconfig.vidWidth, (float)cls.glconfig.vidHeight );
		x += mL;
		y += mT;
		w -= ( mL + mR );
		h -= ( mT + mB );
	}

	/* declarative animated rect width. When the item
	 * carries bindwidth "<store_key>", scale the emitted width by the
	 * [0..1]-clamped float read from the store. Used for progress bars
	 * (e.g. loading-screen overall bar bound to cl_loadProgress) without
	 * needing a custom-draw wrapper. Production default storeBindWidth[0]
	 * is empty → zero cost on the normal path. */
	if ( item->storeBindWidth[ 0 ] ) {
		const wuiStoreEntry_t *e = WiredStore_Get( item->storeBindWidth );
		if ( e ) {
			float frac = e->value;
			if ( frac < 0.0f ) frac = 0.0f;
			if ( frac > 1.0f ) frac = 1.0f;
			w = w * frac;
		}
	}

	/* Zero-size items don't produce a meaningful Clay element; skip.
	 *
	 * Exception (2026-05-25 visual-regression fix): items that carry a
	 * customDrawName own their own content-aware sizing — the .wui
	 * authoring convention for text-emit HUD elements (statusbar_value,
	 * score, name, ammomessage, itempickup, crosshair) is `rect X Y 0 0`,
	 * meaning "(x,y) is the anchor; the routine sizes itself from fontsize
	 * + measured text." Skipping these here drops their CUSTOM emit, which
	 * cascades into the lazy-create never firing → routine never
	 * dispatched → no text rendered. The routine reads its own
	 * config.rect (still zero-sized) for placement but uses fontsize for
	 * text height, so the Clay-side bb has informational value only.
	 *
	 * The resolvedRect-based gate is only
	 * meaningful for FLOATING items (useFloating=true), where the floating
	 * sizing uses resolvedRect.{w,h} directly. Path A native flex items
	 * derive their size from Clay's own layout pass via layoutCfg.sizing;
	 * their resolvedRect cascades to 0 whenever an ancestor authored
	 * `height GROW` (UNIT_AUTO collapses to 0 in WUI_Resolve at
	 * cl_wired_layout.c:16). Skipping the check for Path A keeps the
	 * recursive emit walk reaching deeply nested children — without this
	 * fix `right_region` and its 3 cards never entered emit because
	 * main_content.resolvedRect.h cascaded a 0 through left_region's
	 * sibling and right_region itself. */
	qboolean wui_hasOffset = ( item->wuiOffset.hasTop || item->wuiOffset.hasLeft
	                        || item->wuiOffset.hasRight || item->wuiOffset.hasBottom );
	/* A static flex-container that is a real flex child (parentIsContainer) takes
	 * the native-flex path even inside a Path B (wrap / space-between) panel, so
	 * its PERCENT sizing resolves against its real Clay parent. Without the
	 * `&& !parentIsContainer` guard, Path B force-floated every item, making a
	 * native-flex menu root emit FIXED(resolvedRect) off a child-flex-resolved
	 * rect that cascades small — the "PERCENT height collapses to a few px" bug
	 * a menu root hits when it drops `position absolute`. Genuinely floating
	 * items (position != static, or top/left/right/bottom offsets, or non-
	 * container leaves at the top level) still float via the other disjuncts.
	 *
	 * The wui_clay_item_is_absolute() disjunct MIRRORS the layout resolver's
	 * absolute-partition test so emit and layout agree: a `decoration` leaf that
	 * authored a concrete non-zero rect (grow 0, fixed w/h, non-zero x||y) was
	 * placed OUT of flex flow by the resolver (resolvedRect = authored rect), so
	 * emit must float it at that rect. Without this the item took the native-flex
	 * path and Clay re-flowed it, ignoring the authored position — the loading
	 * screen's map-title / streaming-rows / wireframe customs landed off-screen or
	 * were dropped. (The `position != POSITION_STATIC` arm is subsumed by the
	 * shared predicate; kept explicit above for readability.) */
	qboolean wui_useFloating = ( wui_clay_current_pathB && !parentIsContainer )
	                         || item->position != POSITION_STATIC
	                         || wui_hasOffset
	                         || wui_clay_item_is_absolute( item )
	                         || ( !item->isFlexContainer && !parentIsContainer );
	if ( wui_useFloating && ( w <= 0.0f || h <= 0.0f ) ) {
		if ( !item->customDrawName[ 0 ] ) {
			return;
		}
	}

	/* Active-state evaluation. Reads the bound cvar's string
	 * once per frame and compares against the authored value; downstream
	 * property selection (forecolor / backcolor / bordercolor / radius /
	 * fontSize) reads this single flag. Items without activeCvar take an
	 * immediate qfalse short-circuit. */
	qboolean isActive = qfalse;
	if ( item->activeCvar[ 0 ] ) {
		/* Store-backed keys resolve through the store, everything else through
		 * the cvar system. Without this branch a key that lives in the store
		 * reads as the empty string here — no row matches and the highlight
		 * sticks on whichever row happens to be first, which is exactly how
		 * the settings rail failed. Keeping one predicate that knows both
		 * homes means an author writes `active <key> <value>` without caring
		 * which side the key is on. */
		char        buf[ MAX_CVAR_VALUE_STRING ];
		const char *cv;
		if ( WiredUI_IsStoreStateKey( item->activeCvar ) ) {
			WiredUI_StateGetString( item->activeCvar, buf, sizeof( buf ) );
			cv = buf;
		} else {
			cv = Cvar_VariableString( item->activeCvar );
		}
		if ( cv && !strcmp( cv, item->activeValue ) ) {
			isActive = qtrue;
		}
	}

	/* Selection-state unification (2026-06): focus is the single source of
	 * truth for "which menu row is selected" (keyboard nav AND mouse hover
	 * both drive wui_focusItem). The menu rows used to express this with a
	 * separate hover-driven `active ui_currentMenuItem` cvar — a misuse of
	 * the `active` PERSISTENT-selection mechanism for what is really a
	 * TRANSIENT cursor. That cvar binding is gone from the rows; instead the
	 * focused row (and its direct text children — the ">" chevron + LABEL)
	 * pick up the same `.active` colour variants (forecolor.active) so the
	 * focused item still reads emphasized, now sourced from focus alone.
	 *
	 * STRICT separation (Eser-ratified): this focus-derived activation applies
	 * ONLY to items that do NOT use the cvar-`active` mechanism (empty
	 * activeCvar). Items that DO — the settings tabs (active ui_settingsSection
	 * "video" etc.) — keep cvar-ONLY semantics: a focused-but-not-selected tab
	 * must NOT show its selected (.active) styling just because the cursor is on
	 * it (that would make two tabs read selected). So the focus path is gated on
	 * activeCvar[0]=='\0' for both the item itself and the focused parent whose
	 * children we light up. Menu rows (cvar removed) qualify; tabs do not.
	 *
	 * Only direct children are checked (menu rows are flat: container -> leaf
	 * text children); the focused container matches via item == foc.
	 * backcolor.active was removed from the rows, so a focused row gets no cyan
	 * fill — its gold highlight comes from the separate focus-gradient pass. */
	if ( !isActive && item->activeCvar[ 0 ] == '\0' ) {
		const wiredItemDef_t *foc = WiredUI_GetFocusedItem();
		if ( foc ) {
			if ( item == foc ) {
				isActive = qtrue;
			} else if ( foc->activeCvar[ 0 ] == '\0' ) {
				/* light up the focused container's direct children only when the
				 * container itself is a focus-driven (non-cvar) row */
				int fc;
				for ( fc = 0; fc < foc->childCount; fc++ ) {
					if ( foc->children[ fc ] == item ) { isActive = qtrue; break; }
				}
			}
		}
	}

	/* Component-library F1: resolve the INTERACTION visual state (resting /
	 * hover / focused / pressed / disabled). Orthogonal to the cvar-`active`
	 * (isActive) axis above — interaction state only layers ring/hover/press/dim
	 * ON TOP of the selection colour. Derived per-frame, never stored. */
	wuiVisualState_t vstate = WiredUI_ItemVisualState( panel, item );

	/* Pick the active variants when the binding matches.
	 * The active flags were authored explicitly; absence falls through
	 * to the base colour without per-property comparison cost. */
	{
		const vec_t *bcSrc = ( isActive && item->hasActiveBackcolor )
		                     ? item->backcolorActive : item->backcolor;
		const vec_t *bdSrc = ( isActive && item->hasActiveBordercolor )
		                     ? item->bordercolorActive : item->bordercolor;
		bg          = wui_clay_color_of( bcSrc, wui_compositor_panel_alpha );
		borderColor = wui_clay_color_of( bdSrc, wui_compositor_panel_alpha );
	}

	/* Overlay authored state colours (hover/pressed/focused/disabled) on the
	 * bg + border. Fallback-preserving: WiredUI_ResolveStateColors returns qfalse
	 * (leaving bg/border untouched) unless the item authored the relevant state
	 * keyword — so every existing menu is byte-identical here. When it returns
	 * qtrue it has written the (fallback-resolved) fg/bg/border for the matched
	 * variant into the out params. */
	if ( vstate != WUI_STATE_RESTING ) {
		vec4_t stBg, stBd;
		Vector4Copy( item->backcolor, stBg );
		Vector4Copy( item->bordercolor, stBd );
		if ( WiredUI_ResolveStateColors( item, vstate, NULL, stBg, stBd ) ) {
			bg          = wui_clay_color_of( stBg, wui_compositor_panel_alpha );
			borderColor = wui_clay_color_of( stBd, wui_compositor_panel_alpha );
		}
	}
	/* Scale the authored px border by dpiScale so a `1px` border is 1 LOGICAL px
	 * (physically dpiScale px) — matching the UNIT_PX box geometry (WUI_Resolve /
	 * wui_resolve_unit). Every border in the corpus is authored UNIT_PX (`1px`,
	 * `$border_med`=2px); a UNIT_NORM border is already baked to physical px at
	 * parse (×vidHeight) and none exists, so this does not double-scale. */
	borderW     = ( item->border != WINDOW_BORDER_NONE && item->bordersize > 0.0f )
	            ? (uint16_t) ( item->bordersize * WiredUI_GetDpiScale() )
	            : 0;
	hasBackground = ( item->background[0] != '\0' );

	/* The focused item paints the highlight gradient as its own background
	 * (image + tint), behind its children — see wui_clay_focus_gradient_for. */
	hasFocusGradient = wui_clay_focus_gradient_for( panel, item,
	                                                &focusGradShader, &focusGradTint );

	/* Per-side border widths from spec keywords
	 * `border` / `borderX` / `borderY`. Uniform `border w` populates all
	 * four; `borderX w` left+right only; `borderY w` top+bottom only.
	 * Fall back to the legacy uniform `bordersize` when no per-side
	 * entry was set (covers legacy itemDefs that write `bordersize` via
	 * the descriptor table). */
	/* Per-side widths, dpiScale'd like borderW above (all UNIT_PX at parse). */
	float    borderDpi = WiredUI_GetDpiScale();
	uint16_t borderL = (uint16_t) ( item->bordersize4[ 0 ] * borderDpi );
	uint16_t borderR = (uint16_t) ( item->bordersize4[ 1 ] * borderDpi );
	uint16_t borderT = (uint16_t) ( item->bordersize4[ 2 ] * borderDpi );
	uint16_t borderB = (uint16_t) ( item->bordersize4[ 3 ] * borderDpi );
	if ( borderL == 0 && borderR == 0 && borderT == 0 && borderB == 0 ) {
		borderL = borderR = borderT = borderB = borderW;
	}

	/* A custom-draw item (hudElement leaf) owns ALL of its pixels via its
	 * draw routine, which reads backcolor/border/color2 from config AND is
	 * gated by its own state (e.g. the weapon carousel's weaponSelectTime
	 * show/fade window). The compositor must NOT also paint this item's
	 * backcolor/border as a Clay rect — that box would be drawn every frame,
	 * bypassing the routine's gate (it left a persistent empty bordered box at
	 * the carousel slot even while the carousel was hidden). Suppress the
	 * compositor-level fill/border here; the routine still gets the values via
	 * WiredHud_ItemToConfig and draws its own (gated) per-slot chrome. */
	if ( item->customDrawName[ 0 ] ) {
		bg          = (Clay_Color){ 0, 0, 0, 0 };
		borderColor = (Clay_Color){ 0, 0, 0, 0 };
		borderW     = 0;
		borderL = borderR = borderT = borderB = 0;
	}

	/* Component-library F1: keyboard-focus RING. Distinct from the hover FILL
	 * gradient — a real border on the row element (in-tree, via the existing
	 * .border machinery below; never a floating root). Only draws when the item
	 * is the keyboard-focused item AND has no authored border of its own (so we
	 * don't stomp a control that already draws its own outline). Thickness is
	 * physical px (WiredUI_FocusRingFor already ×dpiScale). Custom-draw items are
	 * excluded (their borders were zeroed just above). */
	if ( !item->customDrawName[ 0 ]
	  && borderL == 0 && borderR == 0 && borderT == 0 && borderB == 0 ) {
		vec4_t ringCol;
		float  ringPx = 0.0f;
		if ( WiredUI_FocusRingFor( panel, item, ringCol, &ringPx ) && ringPx > 0.0f ) {
			uint16_t rw = (uint16_t)( ringPx + 0.5f );
			if ( rw < 1 ) rw = 1;
			borderColor = wui_clay_color_of( ringCol, wui_compositor_panel_alpha );
			borderL = borderR = borderT = borderB = rw;
			borderW = rw;
		}
	}

	/* cornerRadius from `radius v` / `radius4 tl tr br bl`.
	 * Spec order is clockwise from TL; Clay_CornerRadius is
	 * `{topLeft, topRight, bottomLeft, bottomRight}` so the BR/BL pair
	 * flips relative to the spec.
	 *
	 * When the active variant is set and bound matches, all
	 * four corners get the active scalar uniformly (the spec deferred a
	 * per-corner `.active` variant; rare enough to wait until modder
	 * demand surfaces). */
	Clay_CornerRadius cornerR;
	if ( isActive && item->hasActiveCornerRadius ) {
		cornerR.topLeft     = item->cornerRadiusActive;
		cornerR.topRight    = item->cornerRadiusActive;
		cornerR.bottomLeft  = item->cornerRadiusActive;
		cornerR.bottomRight = item->cornerRadiusActive;
	} else {
		cornerR.topLeft     = item->cornerRadius4[ 0 ];   /* TL */
		cornerR.topRight    = item->cornerRadius4[ 1 ];   /* TR */
		cornerR.bottomLeft  = item->cornerRadius4[ 3 ];   /* BL */
		cornerR.bottomRight = item->cornerRadius4[ 2 ];   /* BR */
	}

	/* TEXT items may source their complete caption from a cvar/Store-state key
	 * (for example password.wui's ui_password_server_name).  The generic
	 * label-and-value branch deliberately excludes ITEM_TYPE_TEXT, so resolve
	 * that binding into the single-text path here.  Arena-copy it for the same
	 * deferred Clay render lifetime required by storeBind and Lua output below.
	 * storeBind and Lua remain the explicit higher-precedence overrides. */
	if ( item->type == ITEM_TYPE_TEXT && item->cvar[0] ) {
		WiredUI_BoundValueText( item, boundText, sizeof( boundText ) );
		displayText = wui_clay_arena_strdup( boundText );
	}

	/* storeBind: text override from WiredStore (literal key lookup).
	 *
	 * D3 fix (2026-06-30): Clay stores the Clay_String's char POINTER and
	 * renders it later (Clay_EndLayout render-command walk), AFTER emit_item
	 * returns. boundText is a STACK local, so a bound caption pointed Clay at
	 * freed stack → garbled glyphs (the rocket ammo panel's weapon name).
	 * Copy the resolved string into the per-frame scratch arena (lives until
	 * end-of-frame render) so the pointer survives. Static item->text is a
	 * persistent item buffer and was never affected (which is why only BOUND
	 * text nodes garbled). */
	if ( item->storeBind[0] ) {
		if ( wui_clay_store_read_text( item->storeBind, boundText, sizeof( boundText ) ) ) {
			displayText = wui_clay_arena_strdup( boundText );
		}
	}

	/* bind="lua:..." computed text. Overrides storeBind /
	 * .text fallback when present. Lua return value: string → displayText;
	 * number → tostring → displayText; nil/error → falls through to whatever
	 * displayText already is (storeBind result or item->text). Errors logged
	 * once via item->bindWarned. */
	if ( item->luaBindChunk != WIRED_CHUNK_NOREF ) {
		wui_vm_kind_t vm = wui_clay_vm_for_panel( panel );
		char          luaOut[ 256 ];
		if ( wui_clay_chunk_call_string( vm, item->luaBindChunk, luaOut, sizeof( luaOut ) ) ) {
			/* D3: same use-after-scope fix as storeBind — arena-copy so the
			 * pointer outlives emit_item (Clay renders it later). */
			displayText = wui_clay_arena_strdup( luaOut );
		}
	}

	/* wuiAnim lazy-create. The first frame this item enters
	 * emit, allocate a pool slot and latch the id; subsequent frames just
	 * read the eased value the per-frame tick has already written. Use
	 * a const-cast to write the latch + scalar outputs (the rest of
	 * emit_item treats item as read-only). */
	if ( item->animationName[ 0 ] && item->animationId == 0 ) {
		const wuiAnimBuiltin_t *bi = WUI_AnimFindBuiltin( item->animationName );
		if ( bi ) {
			wiredItemDef_t *mut = (wiredItemDef_t *) item;
			int             duration = item->animationDurationMs > 0
			                         ? item->animationDurationMs
			                         : bi->durationMs;
			wuiAnimCurve_t  curve    = bi->curve;
			int             flags    = bi->flags | ( item->animationLoop ? WUI_ANIM_FLAG_LOOP : 0 );
			float          *targetRef;

			/* Route the built-in name to the relevant scalar field on
			 * the item. Compositor emit below reads the same field.
			 * `pulse` shares the alpha-multiplier slot with fade-in/out
			 * so the same forecolor[3] multiplication block applies. */
			if ( !Q_stricmp( item->animationName, "scroll-x" ) ) {
				targetRef = &mut->animOffsetX;
			} else if ( !Q_stricmp( item->animationName, "fade-in" )
			         || !Q_stricmp( item->animationName, "fade-out" )
			         || !Q_stricmp( item->animationName, "pulse" )
			         || !Q_stricmp( item->animationName, "blink" ) ) {
				targetRef = &mut->animAlphaMul;
				mut->animAlphaMul = bi->from;
			} else {
				targetRef = &mut->animOffsetY;
			}
			mut->animationId = WUI_AnimCreate( item->animationName, targetRef,
			                                   bi->from, bi->to, duration, curve, flags );
		} else {
			/* Unknown name — log once via the bindWarned gate to avoid
			 * per-frame spam, then leave animationName latched so we
			 * don't retry every frame. */
			if ( !item->bindWarned ) {
				Com_Log( SEV_WARN, LOG_CH(ch_ui),
					"WUI_Anim: unknown animation '%s' on item '%s' — built-in registry has scroll-x/fade-in/fade-out/slide-up/slide-down/pulse/blink\n",
					item->animationName,
					item->name[ 0 ] ? item->name : "(unnamed)" );
				( (wiredItemDef_t *) item )->bindWarned = qtrue;
			}
		}
	}

	/* storeBindColor: replace forecolor for this frame.
	 * Apply forecolor.active variant before storeBindColor
	 * so explicit per-cell colours from the store still win. */
	if ( isActive && item->hasActiveForecolor ) {
		Vector4Copy( item->forecolorActive, effectiveFg );
	} else {
		Vector4Copy( item->forecolor, effectiveFg );
	}
	if ( item->storeBindColor[0] ) {
		const wuiStoreEntry_t *e = WiredStore_Get( item->storeBindColor );
		if ( e ) Vector4Copy( e->color, effectiveFg );
	}

	/* Component-library F1: state foreground overlay + DISABLED dim. An authored
	 * hover/pressed/focused/disabled *color* replaces effectiveFg; a DISABLED
	 * item with no explicit disabled colour is dimmed (alpha ×0.4 on fg + bg +
	 * border) so it reads greyed. Fallback-preserving: no state keyword => no
	 * change (ResolveStateColors returns qfalse and the dim only fires for the
	 * DISABLED interaction state, which requires `enableMode dim`). */
	if ( vstate != WUI_STATE_RESTING ) {
		vec4_t stFg;
		Vector4Copy( effectiveFg, stFg );
		if ( WiredUI_ResolveStateColors( item, vstate, stFg, NULL, NULL ) ) {
			Vector4Copy( stFg, effectiveFg );
		} else if ( vstate == WUI_STATE_DISABLED ) {
			/* no explicit disabled colour — dim the resolved colours. */
			effectiveFg[ 3 ] *= 0.4f;
			bg.a             *= 0.4f;
			borderColor.a    *= 0.4f;
		}
	}

	/* Alpha-multiplier built-ins (fade-in / fade-out / pulse / blink)
	 * apply at the forecolor stage so the modulation carries through
	 * to text dispatch. Lower bound is >=0 so blink's 0.0-opacity half
	 * actually hides the element; upper bound <1 skips the no-op case
	 * when the multiplier has settled at full opacity. */
	if ( item->animAlphaMul >= 0.0f && item->animAlphaMul < 1.0f
	  && ( !Q_stricmp( item->animationName, "fade-in" )
	    || !Q_stricmp( item->animationName, "fade-out" )
	    || !Q_stricmp( item->animationName, "pulse" )
	    || !Q_stricmp( item->animationName, "blink" ) ) ) {
		effectiveFg[ 3 ] *= item->animAlphaMul;
	}

	/* Assign a stable Clay element ID + record the (id, item, panel)
	 * tuple in the reverse-lookup map so a future click can resolve the
	 * Clay_Hovered() id back to the source wiredItemDef_t. */
	clayId = wui_clay_id_for_item( panel, item );
	wui_id_map_record( clayId, item, panel );

	/* Focus-highlight gradient was emitted here as a CLAY_ATTACH_TO_ROOT
	 * floating element. That made it its own layout tree root, which Clay can
	 * only sort entirely above the main tree (covering the caption) or entirely
	 * below it (behind the menu background, so the bar vanished) — a floating
	 * root cannot slot between the row background and the caption text, because
	 * both live in the main tree. The gradient is now emitted in-tree as the
	 * focused row's first child (wui_clay_emit_focus_gradient, called from
	 * inside the row's CLAY block below), so normal paint order gives
	 * row-background -> gradient -> caption. */

	/* ITEM_TYPE_LISTBOX feeder-driven render. Previously lived
	 * inline in WiredUI_Refresh body; restored via compositor sole-source
	 * Clay tree (see wui_clay_emit_listbox above). Affects 7 wmenus:
	 * playersettings (characters + skins), servers, demos, mods,
	 * startserver, serverinfo, removebots. Returns early — the listbox
	 * encapsulates its own outer box / border / content. */
	if ( item->type == ITEM_TYPE_LISTBOX && item->feeder != 0 ) {
		wui_clay_emit_listbox( item, clayId, x, y, w, h, bg, borderColor, borderW, parentIsContainer );
		return;
	}

	/* ITEM_TYPE_RADIOBUTTON segmented control: a MULTI whose options render
	 * inline as N horizontal segments (single-click select, arrow-key move,
	 * hover + keyboard focus-ring + disabled dim from the F1 framework). Returns
	 * early — it emits its own self-contained row, before the generic label-and-
	 * value branch below (which would otherwise draw it as a single value cell). */
	if ( item->type == ITEM_TYPE_RADIOBUTTON && item->cvar[0] ) {
		wui_clay_emit_radio( item, clayId, x, y, w, h, bg, borderColor, borderW,
		                     vstate, fontName, parentIsContainer );
		return;
	}

	/* label-and-value branch. Cvar-bound items that aren't
	 * TEXT/BUTTON render as "label left + value right" — mirrors the
	 * legacy SCR shape (cl_wired_ui.c label-and-value branch). Parent
	 * CLAY block with LEFT_TO_RIGHT layout + childGap, two CLAY_TEXT
	 * children, plus SLIDER track + fill floating children at legacy
	 * coords. Returns early so the standard text-emit path below stays
	 * single-text. Legacy SCR fires alongside during the transition
	 * cycle; both draw at the same coords, deterministic overdraw.
	 *
	 * Bindwidth note: production label-and-value items use cvar, not
	 * storeBindWidth. The bindwidth scaling above (line ~1159)
	 * runs before this branch; if both are set on the same item, the
	 * branch inherits the scaled width.
	 *
	 * Alignment note: parent uses LEFT_TO_RIGHT with per-child alignment, so
	 * the single-text textalign/childAlignment threading does not
	 * apply here. */
	if ( item->cvar[0]
	  && item->type != ITEM_TYPE_TEXT
	  && item->type != ITEM_TYPE_BUTTON ) {
		Clay_Color fg          = wui_clay_color_of( effectiveFg, wui_compositor_panel_alpha );
		uint16_t   fontSlot    = wui_clay_font_slot_for_face( fontName );
		uint16_t   fontSize    = (uint16_t)( item->fontPointSize > 0.0f
		                                    ? item->fontPointSize
		                                    : (uint16_t) WUI_DEFAULT_FONT_SIZE );
		qboolean   hasShadow   = wui_textstyle_has_dropshadow( item->textstyle );
		uint16_t   padLeft     = (uint16_t)( item->textalignx > 0.0f ? item->textalignx : 0.0f );
		char      *valueBuf    = NULL;

		/* Allocate the value buffer from the per-frame scratch arena so the
		 * .chars pointer Clay stores survives until dispatch. The scratch
		 * arena resets each frame in WiredUI_CompositorEmitFrame. */
		if ( wui_clay_scratch_arena ) {
			valueBuf = (char *) Arena_Alloc( wui_clay_scratch_arena, 256, sizeof( void * ) );
		}
		if ( valueBuf ) {
			WiredUI_BoundValueText( item, valueBuf, 256 );
		}

		/* when this cvar-bearing widget sits
		 * inside a Clay flex container parent, switch attachTo to
		 * CLAY_ATTACH_TO_NONE so Clay flex-positions the widget's
		 * wrapper block within the parent panel. resolvedRect.{x,y}
		 * for items inside flexbox-first .wui panels otherwise pin to
		 * absolute viewport coords and ignore the panel — the
		 * sound.wui canary surfaced this as all widget rows overlapping
		 * at the resolvedRect top-left (5px, 5px). Legacy items
		 * (parentIsContainer=qfalse) keep the historical
		 * ATTACH_TO_ROOT pin so playersettings / startserver /
		 * preferences continue to lay out as before. */
		Clay_FloatingAttachToElement attachMode = parentIsContainer
		                                        ? CLAY_ATTACH_TO_NONE
		                                        : CLAY_ATTACH_TO_ROOT;
		/* Width sizing: flex-container children (settings rows) GROW to fill
		 * the parent box's inner width, so the label/value spacer split spans
		 * the box exactly and the right-aligned value hugs the box's right edge
		 * rather than the row's unreliable resolvedRect.w (which is 0/stale for
		 * nested flex items and would let the value overflow the box). Legacy
		 * ATTACH_TO_ROOT rows keep their historical FIXED(w) from resolvedRect. */
		Clay_SizingAxis rowWidthSizing = parentIsContainer
		                               ? CLAY_SIZING_GROW( 0 )
		                               : CLAY_SIZING_FIXED( w );
		/* Focus/hover highlight for cvar rows: the general path (line ~2738)
		 * paints the focused row with the gold focus-gradient bar, but cvar rows
		 * take THIS branch and returned before reaching it — so a hovered toggle/
		 * dropdown/slider showed no highlight and you couldn't tell which row you
		 * were about to click. Consume the same gradient here (already resolved at
		 * line ~2104) so every focused control row gets the same bar. */
		/* Focus highlight: a FLAT solid fill across the whole row (no gradient
		 * bar, no side fade). The accent tint at a subtle alpha so the label
		 * stays readable. `.image` is left NULL — dropping the gradient shader
		 * removes the left→right fade the user disliked; the fill spans the
		 * full row width because the row element's background covers its box. */
		Clay_Color rowGradBg    = bg;
		if ( hasFocusGradient ) {
			rowGradBg     = focusGradTint;
			rowGradBg.a  *= 0.30f;   /* solid wash, not an opaque bar */
		}
		CLAY({
			.id     = { .id = clayId },
			.layout = {
				.sizing          = { rowWidthSizing, CLAY_SIZING_FIXED( h ) },
				.layoutDirection = CLAY_LEFT_TO_RIGHT,
				.childAlignment  = { .x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER },
				.padding         = { padLeft, 0, 0, 0 },
				.childGap        = 12
			},
			.floating = {
				.attachTo     = attachMode,
				.offset       = { x, y },
				.attachPoints = { CLAY_ATTACH_POINT_LEFT_TOP, CLAY_ATTACH_POINT_LEFT_TOP },
				.pointerCaptureMode = item->decoration
				                    ? CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH
				                    : CLAY_POINTER_CAPTURE_MODE_CAPTURE
			},
			.image = { .imageData = NULL },
			.backgroundColor = rowGradBg,
			.border = {
				.color = borderColor,
				.width = { borderW, borderW, borderW, borderW, 0 }
			}
		}) {
			/* Label (left) */
			if ( item->text[ 0 ] ) {
				Clay_String labelStr;
				labelStr.isStaticallyAllocated = qfalse;
				labelStr.length                = (int32_t) strlen( item->text );
				labelStr.chars                 = item->text;
				CLAY_TEXT( labelStr, CLAY_TEXT_CONFIG({
					.userData      = hasShadow ? (void*)(uintptr_t)1 : NULL,
					.fontId        = fontSlot,
					.fontSize      = fontSize,
					.letterSpacing = 0,
					.textColor     = fg,
					.wrapMode      = CLAY_TEXT_WRAP_NONE,
					.textAlignment = CLAY_TEXT_ALIGN_LEFT
				}) );
			}

			/* Spacer: a zero-content GROW child that absorbs all the row's slack,
			 * pushing the slider track (and then the FIT-sized value) to the
			 * right edge. Emitted for ALL row types. The spacer (min 0) is the
			 * single grow-absorber, so the value cell can stay content-sized
			 * (FIT) — see the value block below. An earlier version made the
			 * VALUE cell GROW instead; that regressed multi-word values ("r or
			 * MWHEELDOWN") because Clay's compress path shrinks a GROW cell
			 * toward its widest-word min-width when label+value overflow the row,
			 * landing the cell narrower than its text so the right-aligned value
			 * was pushed left of the edge. A GROW spacer + FIT value never
			 * compresses the value, so both single- and multi-word values sit
			 * flush right. */
			CLAY({
				.layout = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED( 0 ) } }
			}) {}

			/* SLIDER: track + fill as Clay native flex children of the
			 * widget block (which has LEFT_TO_RIGHT layout). This was
			 * previously emitted as floating
			 * CLAY_ATTACH_TO_PARENT children with absolute offsets
			 * (barXoff=w*0.5, barYoff=h*0.4). That worked when the
			 * widget block itself was floating (Path B), but the widget block
			 * switched to ATTACH_TO_NONE when sitting
			 * inside a Clay flex container parent (settings panels),
			 * which left the track + fill orphaned (Clay's attach-to-
			 * parent resolver needs a floating ancestor for the
			 * relative-position math to work). Making track + fill
			 * native flex children lets them flow as part of the
			 * widget's LEFT_TO_RIGHT row: label - track - value,
			 * with the value text auto-pushed to the right by
			 * childGap. Composes cleanly with both ATTACH_TO_NONE
			 * and ATTACH_TO_ROOT (legacy) widget wrappers. */
			if ( item->type == ITEM_TYPE_SLIDER ) {
				float      dpi       = WiredUI_GetDpiScale();
				float      frac      = WiredUI_SliderFraction( item );
				float      thumbWpx  = WUI_SLIDER_THUMB_W_PX * dpi;
				float      thumbHpx  = WUI_SLIDER_THUMB_H_PX * dpi;
				float      trackHpx  = WUI_SLIDER_TRACK_H_PX * dpi;
				uint32_t   trackId   = wui_clay_slider_track_id_for_item( item );
				vec4_t     trackVec  = { 0.3f, 0.3f, 0.3f, 0.6f };
				/* Filled portion of the slider track = the theme accent.
				 * FIX 2026-08-17: was $primary_cyan, a v1 token no accent
				 * overlay rewrites — every slider fill stayed cyan under every
				 * accent. Fallback is the shipped amber default. */
				vec4_t     fillVec   = { 0.957f, 0.627f, 0.227f, 1.0f };  /* $accent #f4a03a fallback */
				vec4_t     thumbVec  = { 0.85f, 0.92f, 0.96f, 1.0f };     /* light handle */
				wui_clay_token_rgb( "accent", fillVec );                  /* theme-driven; follows ui_palette_accent */
				Clay_Color trackBg   = wui_clay_color_of( trackVec, wui_compositor_panel_alpha );
				Clay_Color fillBg    = wui_clay_color_of( fillVec,  wui_compositor_panel_alpha );
				Clay_Color thumbBg   = wui_clay_color_of( thumbVec, wui_compositor_panel_alpha );
				float      usableX   = 0.0f, usableW = 0.0f, gThumbW = thumbWpx;
				qboolean   haveGeom;
				float      fillPx;

				/* Fill length up to the thumb's LEFT edge, in px. The exact value
				 * needs the track's rendered width (PERCENT sizing is resolved by
				 * Clay, not known here), so read the PREVIOUS frame's track rect via
				 * WiredUI_SliderTrackGeom (same 1-frame lag the drag hit-test already
				 * accepts). This is the SAME geometry the input path uses, so the
				 * drawn thumb sits exactly under the drag hit-zone. Before the first
				 * layout pass usableW is 0 -> fill falls back to PERCENT(frac) and the
				 * thumb rides its right edge; it snaps to the pixel-exact position on
				 * the next frame. */
				haveGeom = WiredUI_SliderTrackGeom( item, &usableX, &usableW, &gThumbW, NULL );
				fillPx   = haveGeom ? ( frac * usableW ) : -1.0f;

				/* Track: ~45% of the widget row's width. Its element height is the
				 * thin bar (trackHpx); the taller thumb overflows it vertically (Clay
				 * does not clip children), reading as a handle straddling the bar.
				 * childAlignment.y = CENTER keeps the fill and thumb on the mid-line.
				 * The track carries the stable pointer-derived id so the input path
				 * resolves this exact rect (single-source geometry: draw==hit). */
				CLAY({
					.id     = { .id = trackId },
					.layout = {
						.sizing = { CLAY_SIZING_PERCENT( 0.45f ), CLAY_SIZING_FIXED( trackHpx ) },
						.layoutDirection = CLAY_LEFT_TO_RIGHT,
						.childAlignment = { .x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER }
					},
					.cornerRadius = CLAY_CORNER_RADIUS( 4.0f ),
					.backgroundColor = trackBg
				}) {
					/* Fill: from track start to the thumb's left edge. FIXED(px) once
					 * the geometry is known (pixel-exact, matches input), else
					 * PERCENT(frac) fallback on the first frame. */
					if ( fillPx >= 0.0f ) {
						if ( fillPx > 0.0f ) {
							CLAY({
								.layout = { .sizing = { CLAY_SIZING_FIXED( fillPx ), CLAY_SIZING_GROW(0) } },
								.cornerRadius = CLAY_CORNER_RADIUS( 4.0f ),
								.backgroundColor = fillBg
							}) {}
						}
					} else if ( frac > 0.0f ) {
						CLAY({
							.layout = { .sizing = { CLAY_SIZING_PERCENT( frac ), CLAY_SIZING_GROW(0) } },
							.cornerRadius = CLAY_CORNER_RADIUS( 4.0f ),
							.backgroundColor = fillBg
						}) {}
					}
					/* Thumb: fixed handle, taller/wider than the bar, drawn right after
					 * the fill so it sits at the value position. */
					CLAY({
						.layout = { .sizing = { CLAY_SIZING_FIXED( thumbWpx ), CLAY_SIZING_FIXED( thumbHpx ) } },
						.cornerRadius = CLAY_CORNER_RADIUS( 3.0f ),
						.backgroundColor = thumbBg
					}) {}
				}
			}

			/* CHECKBOX (component-library F2): a real box + check glyph in the
			 * value cell, replacing the faked "yesno" booleans' text. The box is
			 * a Clay flex child (self-positioned by the row's LEFT_TO_RIGHT
			 * layout, vertically centred), sized in PHYSICAL px via
			 * wui_checkbox_geom (x dpiScale -- WiredUI is physical-pixel Clay
			 * space). Framework state (hover/focus/pressed/disabled) is consulted
			 * through `vstate`: DISABLED dims, and the outline/check colour is
			 * lifted from the item's state colours when authored (fallback: the
			 * row's effective fg). The row-level click / Space / arrows toggle the
			 * bound 0/1 cvar (cl_wired_ui.c); the box is a pure value display, so
			 * no separate hit-test rect is needed (mirrors yesno). */
			if ( item->type == ITEM_TYPE_CHECKBOX ) {
				wuiCheckboxGeom_t cbg;
				float             dpi    = WiredUI_GetDpiScale();
				float             dpiCr  = ( dpi > 0.0f ? dpi : 1.0f );
				qboolean          on     = qfalse;
				vec4_t            markVec;
				vec4_t            boxVec  = { 0.30f, 0.30f, 0.30f, 0.55f };  /* empty-box fill */
				Clay_Color        boxBg;
				Clay_Color        markBg;
				Clay_Color        boxBorder;

				wui_checkbox_geom( dpi, &cbg );

				if ( item->cvar[ 0 ] ) {
					char cbBuf[ 64 ];
					WiredUI_StateGetString( item->cvar, cbBuf, sizeof( cbBuf ) );
					on = ( atof( cbBuf ) != 0.0 );
				}

				/* Outline + check colour: start from the row's effective fg, then
				 * let authored state colours override for the current interaction
				 * state (hover/focused/pressed/disabled). */
				Vector4Copy( effectiveFg, markVec );
				{
					vec4_t stFg;
					Vector4Copy( effectiveFg, stFg );
					if ( vstate != WUI_STATE_RESTING &&
					     WiredUI_ResolveStateColors( item, vstate, stFg, NULL, NULL ) ) {
						Vector4Copy( stFg, markVec );
					}
					/* DISABLED: dim both box and check so a greyed checkbox reads inert. */
					if ( vstate == WUI_STATE_DISABLED ) {
						markVec[ 3 ] *= 0.4f;
						boxVec[ 3 ]  *= 0.4f;
					}
				}

				boxBg     = wui_clay_color_of( boxVec,  wui_compositor_panel_alpha );
				markBg    = wui_clay_color_of( markVec, wui_compositor_panel_alpha );
				boxBorder = markBg;   /* outline shares the fg/state colour */

				CLAY({
					.layout = {
						.sizing = { CLAY_SIZING_FIXED( cbg.boxPx ), CLAY_SIZING_FIXED( cbg.boxPx ) },
						.childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER }
					},
					.cornerRadius    = CLAY_CORNER_RADIUS( 3.0f * dpiCr ),
					.backgroundColor = boxBg,
					.border = {
						.color = boxBorder,
						.width = { (uint16_t) cbg.borderPx, (uint16_t) cbg.borderPx,
						           (uint16_t) cbg.borderPx, (uint16_t) cbg.borderPx, 0 }
					}
				}) {
					/* Checked: a filled inner square (the check glyph). A solid square
					 * is atlas-independent (a unicode check may not be in the MSDF
					 * font) and reads crisply at every dpiScale. */
					if ( on ) {
						CLAY({
							.layout = { .sizing = { CLAY_SIZING_FIXED( cbg.checkPx ),
							                        CLAY_SIZING_FIXED( cbg.checkPx ) } },
							.cornerRadius    = CLAY_CORNER_RADIUS( 2.0f * dpiCr ),
							.backgroundColor = markBg
						}) {}
					}
				}
			}

			/* SPINNER (component-library): a numeric stepper — [-] value [+].
			 * Three Clay flex children (self-positioned by the row's LEFT_TO_RIGHT
			 * layout, vertically centred): decrement button, fixed-width numeric
			 * readout, increment button. Everything is PHYSICAL px (× dpiScale —
			 * WiredUI is physical-pixel Clay space). The two buttons carry stable
			 * pointer-derived ids (wui_clay_spinner_btn_id_for_item) so the input
			 * path resolves the exact drawn rect for click + click-and-hold
			 * (single-source geometry: draw-rect == hit-rect, never resolvedRect).
			 * Framework state (hover/focus/pressed/disabled) is consulted via
			 * `vstate`: DISABLED dims. The value text is the bound cvar, clamped to
			 * [min,max]. */
			if ( item->type == ITEM_TYPE_SPINNER ) {
				float      dpi     = WiredUI_GetDpiScale();
				float      dpiCr   = ( dpi > 0.0f ? dpi : 1.0f );
				float      btnPx   = WUI_SPINNER_BTN_LOGICAL_PX   * dpiCr;
				float      valPx   = WUI_SPINNER_VALUE_LOGICAL_PX * dpiCr;
				float      bordPx  = WUI_SPINNER_BORDER_PX        * dpiCr;
				float      radPx   = WUI_SPINNER_RADIUS_PX        * dpiCr;
				uint32_t   decId   = wui_clay_spinner_btn_id_for_item( item, qfalse );
				uint32_t   incId   = wui_clay_spinner_btn_id_for_item( item, qtrue );
				float      curVal  = WiredUI_SpinnerValue( item );
				qboolean   atMin   = ( curVal <= item->sliderData.minVal + 1e-4f );
				qboolean   atMax   = ( curVal >= item->sliderData.maxVal - 1e-4f );
				/* Value string backing: the per-frame scratch arena, NOT a stack
				 * buffer. Clay stores Clay_String.chars as a bare pointer and defers
				 * text rendering to Clay_EndLayout/dispatch — long after this emit
				 * block's stack frame is gone. A stack buffer here measured correctly
				 * (measure runs inline during layout) but drew from freed/overwritten
				 * memory, so multi-digit readouts rendered garbage (a "24" that stayed
				 * "2" on screen). The scratch arena resets each frame in
				 * WiredUI_CompositorEmitFrame, so the pointer stays valid through
				 * dispatch — the same lifetime the general value-cell path already
				 * relies on (valueBuf above). */
				char      *valStr  = wui_clay_scratch_arena
				                   ? (char *) Arena_Alloc( wui_clay_scratch_arena, 32, sizeof( void * ) )
				                   : NULL;
				vec4_t     fgVec, btnFillVec = { 0.30f, 0.30f, 0.30f, 0.55f };
				vec4_t     fieldVec = { 0.16f, 0.16f, 0.18f, 0.55f };
				Clay_Color btnBg, fieldBg, glyphBg, btnBorder;
				Clay_Color decGlyphBg, incGlyphBg;
				Clay_String decStr, incStr, valClayStr;

				if ( bordPx < 1.0f ) bordPx = 1.0f;

				/* Readout text from the clamped cvar value. %g prints integer
				 * ranges cleanly (50, not 50.000000) and trims float noise. */
				Com_sprintf( valStr, 32, "%g", curVal );

				/* Glyph / outline colour: row fg, overridden by authored state
				 * colours for the current interaction state. */
				Vector4Copy( effectiveFg, fgVec );
				if ( vstate != WUI_STATE_RESTING ) {
					vec4_t stFg;
					Vector4Copy( effectiveFg, stFg );
					if ( WiredUI_ResolveStateColors( item, vstate, stFg, NULL, NULL ) ) {
						Vector4Copy( stFg, fgVec );
					}
				}
				if ( vstate == WUI_STATE_DISABLED ) {
					fgVec[ 3 ]      *= 0.4f;
					btnFillVec[ 3 ] *= 0.4f;
					fieldVec[ 3 ]   *= 0.4f;
				}

				btnBg      = wui_clay_color_of( btnFillVec, wui_compositor_panel_alpha );
				fieldBg    = wui_clay_color_of( fieldVec,   wui_compositor_panel_alpha );
				glyphBg    = wui_clay_color_of( fgVec,      wui_compositor_panel_alpha );
				btnBorder  = glyphBg;
				/* Grey out the glyph on a button that can't move further (at a range
				 * end), leaving the field + other button live. */
				decGlyphBg = glyphBg; incGlyphBg = glyphBg;
				if ( atMin ) { decGlyphBg.a *= 0.35f; }
				if ( atMax ) { incGlyphBg.a *= 0.35f; }

				decStr.isStaticallyAllocated = qtrue;
				decStr.length = 1; decStr.chars = "-";
				incStr.isStaticallyAllocated = qtrue;
				incStr.length = 1; incStr.chars = "+";
				valClayStr.isStaticallyAllocated = qfalse;
				valClayStr.length = (int32_t) strlen( valStr );
				valClayStr.chars  = valStr;

				/* Decrement button (-). Carries decId for the click/hold hit-test. */
				CLAY({
					.id     = { .id = decId },
					.layout = {
						.sizing = { CLAY_SIZING_FIXED( btnPx ), CLAY_SIZING_FIXED( btnPx ) },
						.childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER }
					},
					.cornerRadius    = CLAY_CORNER_RADIUS( radPx ),
					.backgroundColor = btnBg,
					.border = {
						.color = btnBorder,
						.width = { (uint16_t) bordPx, (uint16_t) bordPx,
						           (uint16_t) bordPx, (uint16_t) bordPx, 0 }
					}
				}) {
					CLAY_TEXT( decStr, CLAY_TEXT_CONFIG({
						.fontId = fontSlot, .fontSize = fontSize,
						.textColor = decGlyphBg, .wrapMode = CLAY_TEXT_WRAP_NONE,
						.textAlignment = CLAY_TEXT_ALIGN_CENTER
					}) );
				}

				/* Numeric readout field (fixed width so both buttons stay put). */
				CLAY({
					.layout = {
						.sizing = { CLAY_SIZING_FIXED( valPx ), CLAY_SIZING_FIXED( btnPx ) },
						.childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER }
					},
					.cornerRadius    = CLAY_CORNER_RADIUS( radPx ),
					.backgroundColor = fieldBg
				}) {
					CLAY_TEXT( valClayStr, CLAY_TEXT_CONFIG({
						.userData  = hasShadow ? (void*)(uintptr_t)1 : NULL,
						.fontId = fontSlot, .fontSize = fontSize,
						.textColor = glyphBg, .wrapMode = CLAY_TEXT_WRAP_NONE,
						.textAlignment = CLAY_TEXT_ALIGN_CENTER
					}) );
				}

				/* Increment button (+). Carries incId. */
				CLAY({
					.id     = { .id = incId },
					.layout = {
						.sizing = { CLAY_SIZING_FIXED( btnPx ), CLAY_SIZING_FIXED( btnPx ) },
						.childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER }
					},
					.cornerRadius    = CLAY_CORNER_RADIUS( radPx ),
					.backgroundColor = btnBg,
					.border = {
						.color = btnBorder,
						.width = { (uint16_t) bordPx, (uint16_t) bordPx,
						           (uint16_t) bordPx, (uint16_t) bordPx, 0 }
					}
				}) {
					CLAY_TEXT( incStr, CLAY_TEXT_CONFIG({
						.fontId = fontSlot, .fontSize = fontSize,
						.textColor = incGlyphBg, .wrapMode = CLAY_TEXT_WRAP_NONE,
						.textAlignment = CLAY_TEXT_ALIGN_CENTER
					}) );
				}
			}

			/* Value (right). A single CLAY_TEXT for the whole (possibly
			 * colour-coded) value, right-aligned so it hugs the row's right
			 * edge. Embedded ^N codes stay in the string and the MSDF draw path
			 * switches colour per-glyph, so the colour emphasis on multi-key
			 * binds ("r ^7or MWHEELDOWN") is preserved without splitting.
			 *
			 * This used to be a per-run colour split (one CLAY_TEXT per ^-run)
			 * that existed ONLY to sidestep a measure/draw skew: the measure
			 * callback bounded on a glyph budget while Clay handed it byte
			 * slices, so an embedded ^N over-read past the slice and inflated
			 * the summed width, stranding the value left of the edge. That skew
			 * is now fixed at the source — the measure/draw contract is
			 * byte-bounded and colour-code-aware (MSDF_MeasureStringBytes /
			 * MSDF_DrawStringBytes), so a single colour-coded CLAY_TEXT measures
			 * exactly what it draws and right-aligns flush. The split was a dead
			 * workaround and is gone (one canonical text path). */
			if ( valueBuf && valueBuf[ 0 ] ) {
				Clay_String valStr;
				valStr.isStaticallyAllocated = qfalse;
				valStr.length                = (int32_t) strlen( valueBuf );
				valStr.chars                 = valueBuf;
				CLAY_TEXT( valStr, CLAY_TEXT_CONFIG({
					.userData      = hasShadow ? (void*)(uintptr_t)1 : NULL,
					.fontId        = fontSlot,
					.fontSize      = fontSize,
					.letterSpacing = (uint16_t) item->letterSpacing,
					.textColor     = fg,
					.wrapMode      = CLAY_TEXT_WRAP_NONE,
					.textAlignment = CLAY_TEXT_ALIGN_RIGHT
				}) );
			}
		}
		return;
	}

	/* choose Path A (native Clay layout) vs Path B (CLAY_FLOATING +
	 * resolvedRect pinning). Path B always wins for explicitly-positioned
	 * items (position ABSOLUTE / VIEWPORT) and for panels that use polyfill-
	 * target features (wrap / shrink>1 / space-between, detected at parse
	 * time as menu->pathBKind). Path A items emit native Clay sizing + the
	 * parent flex layout positions them; Path B items pin via floating.
	 *
	 * For Path A items that are themselves flex containers, emit the
	 * .layout config (layoutDirection, childGap, padding, childAlignment)
	 * so their children flow correctly. */
	{
		/* Non-flex top-level items pin via floating with offset from their
		 * authored/last-Clay snapshot. Native descendants stay in parent flow.
		 *
		 * Flex containers stay on Clay flex (useFloating=false unless they
		 * have explicit POSITION or pathBKind), so their internal layout
		 * continues to flow as before. */
		/* leaf items inside a flex container
		 * parent stay on Clay native flex even when they themselves are
		 * not flex containers. The previous `!isFlexContainer →
		 * useFloating=qtrue` rule dropped them onto ATTACH_TO_ROOT
		 * floating, where a stale resolvedRect competed with Clay's own
		 * resolution and produced the
		 * LEFT-region vertical-stack overlap the multimodal review
		 * flagged. With parentIsContainer threaded through the recursive
		 * emit walk, leaf children of a container now flow naturally
		 * inside the parent CLAY block.
		 *
		 * useFloating is computed identically at the top of
		 * the function (the zero-rect gate at line 1781 needs it); reuse
		 * that value here to keep the two sites in lockstep. */
		qboolean useFloating = wui_useFloating;

		/* text alignment + dropshadow
		 * inputs threaded through the CLAY_TEXT_CONFIG / parent CLAY block.
		 * Resolved once per item and reused by both Path A / Path B branches.
		 *
		 * Pick the .active fontSize variant when the binding
		 * matches; falls back to the base fontPointSize / default. The
		 * single resolved value flows through both CLAY_TEXT_CONFIG sites
		 * (Path A floating + Path B native flex) so they stay in lockstep. */
		float    effectiveFontPointSize = ( isActive && item->hasActiveFontSize && item->fontPointSizeActive > 0.0f )
		                                ? item->fontPointSizeActive
		                                : ( item->fontPointSize > 0.0f ? item->fontPointSize : (float) WUI_DEFAULT_FONT_SIZE );
		float    textCharSize = effectiveFontPointSize;
		qboolean textHasShadow = wui_textstyle_has_dropshadow( item->textstyle );
		Clay_TextAlignment textHAlign = wui_clay_textalign_from_item( item->textalign );
		/* Auto-vertical-center gate.
		 *
		 * The gate must test the box height Clay will ACTUALLY draw, not the
		 * layout-pass resolvedRect.h. A type-1 leaf authored `height FIXED Npx`
		 * (e.g. the ESC-menu QW_BUTTON_BLOCK pills) emits its Clay box at
		 * wuiRect.h.value*dpiScale (see the height-sizing cascade below), but
		 * its resolvedRect.h can lag at a small/stale value (runtime-measured
		 * at 8.5px while the emitted box was tens of px). Testing the stale h
		 * left the gate false, so the label fell back to TOP align and drew
		 * top-anchored with empty space below the caption. Mirror the emit's
		 * own PX/PERCENT height decision here so the gate sees the real box. */
		float effectiveBoxH = h;
		{
			float parentVHc = wui_clay_lastHeight > 0 ? (float) wui_clay_lastHeight : (float) cls.glconfig.vidHeight;
			if ( layoutRect->h.unit == UNIT_PX ) {
				effectiveBoxH = layoutRect->h.value * WiredUI_GetDpiScale();
			} else if ( layoutRect->h.unit == UNIT_NORM && layoutRect->h.value > 0 ) {
				effectiveBoxH = layoutRect->h.value * parentVHc;
			}
		}
		qboolean textAutoVCenter = ( item->textaligny == 0.0f
		                          && effectiveBoxH > textCharSize ) ? qtrue : qfalse;
		Clay_LayoutAlignmentY textVAlign = textAutoVCenter
		                                 ? CLAY_ALIGN_Y_CENTER
		                                 : CLAY_ALIGN_Y_TOP;
		uint16_t textPadLeft = (uint16_t)( item->textalignx > 0.0f
		                                  ? item->textalignx : 0.0f );
		uint16_t textPadTop  = (uint16_t)( ( !textAutoVCenter && item->textaligny > 0.0f )
		                                  ? item->textaligny : 0.0f );

		Clay_LayoutConfig layoutCfg = { 0 };

		if ( useFloating ) {
			layoutCfg.sizing.width  = CLAY_SIZING_FIXED( w );
			layoutCfg.sizing.height = CLAY_SIZING_FIXED( h );
		} else {
			/* Path A static flex child — sizing decision tree:
			 *   grow > 0          → GROW (min from flexChild.minW/H if set)
			 *   UNIT_PX           → FIXED at pixel value
			 *   UNIT_NORM, val>0  → PERCENT
			 *   UNIT_AUTO         → FIT (content-driven; spec §4 `width FIT`)
			 *   else              → FIXED at resolvedRect px (legacy fallback)
			 * Min/Max from flexChild.{min,max}{Width,Height} are honored
			 * when set; default 0 / CLAY_LAYOUT_DEFAULT_MAX otherwise. */
			float parentVW = wui_clay_lastWidth  > 0 ? (float) wui_clay_lastWidth  : (float) cls.glconfig.vidWidth;
			float parentVH = wui_clay_lastHeight > 0 ? (float) wui_clay_lastHeight : (float) cls.glconfig.vidHeight;
			float minW = wui_resolve_unit( item->flexChild.minWidth,  parentVW );
			float maxW = wui_resolve_unit( item->flexChild.maxWidth,  parentVW );
			float minH = wui_resolve_unit( item->flexChild.minHeight, parentVH );
			float maxH = wui_resolve_unit( item->flexChild.maxHeight, parentVH );
			/* basis→ GROW min hint. When the modder
			 * authored a non-zero `basis v`, treat that as the GROW
			 * range's minimum (= Clay starts at basis and expands from
			 * there). minWidth keeps precedence if explicitly set. */
			float basisPx = wui_resolve_unit( item->flexChild.basis, parentVW );
			if ( basisPx > 0.0f && minW <= 0.0f ) minW = basisPx;

			/* shrink Option 1 deviation: Clay v0.14 has
			 * no native flex-shrink field, so the keyword is parser-
			 * accepted but emit is no-op. One-time SEV_WARN on first
			 * non-default shrink to surface the deviation; charter §UI
			 * declarative format unification documents it. */
			static qboolean s_warned_shrink = qfalse;
			if ( !s_warned_shrink && item->flexChild.shrink > 0.0f
			  && item->flexChild.shrink != 1.0f ) {
				Com_Log( SEV_WARN, LOG_CH(ch_ui),
					"WUI: `shrink %.2f` on item '%s' has no Clay v0.14 "
					"native mapping — parser-accepted, emit no-op (charter "
					"§UI declarative format unification documented deviation)\n",
					item->flexChild.shrink,
					item->name[0] ? item->name : "(unnamed)" );
				s_warned_shrink = qtrue;
			}

			/* direction-aware grow disambiguation.
			 * `width GROW`  keyword: parser sets grow>0 + w.unit=AUTO   → growW
			 * `height GROW` keyword: parser sets grow>0 + h.unit=AUTO   → growH
			 * `grow N`      shorthand: grow>0, both units unchanged     → main axis
			 *   (parentDirection == COLUMN ? growH : growW)
			 * Previously emit unconditionally mapped grow→width, so column-
			 * parent grow children collapsed onto the cross axis and the
			 * main axis fell through to PERCENT/FIXED cascade. */
			qboolean growW = qfalse;
			qboolean growH = qfalse;
			if ( item->flexChild.grow > 0.0f ) {
				qboolean explicitW = ( layoutRect->w.unit == UNIT_AUTO );
				qboolean explicitH = ( layoutRect->h.unit == UNIT_AUTO );
				if ( explicitW || explicitH ) {
					growW = explicitW;
					growH = explicitH;
				} else {
					if ( parentDirection == WUI_LAYOUT_COLUMN ) growH = qtrue;
					else                                          growW = qtrue;
				}
			}

			if ( growW ) {
				layoutCfg.sizing.width = ( maxW > 0.0f )
					? CLAY_SIZING_GROW( minW, maxW )
					: CLAY_SIZING_GROW( minW );
			} else if ( layoutRect->w.unit == UNIT_PX ) {
				/* Scale authored px by dpiScale to match WUI_Resolve (UNIT_PX)
				 * and the font path — otherwise a flex container authored in
				 * logical px stays 1× while its rows (routed through resolvedRect)
				 * scale 2×, so the box is short and its contents overflow. */
				layoutCfg.sizing.width = CLAY_SIZING_FIXED( layoutRect->w.value * WiredUI_GetDpiScale() );
			} else if ( layoutRect->w.unit == UNIT_VW
			         || layoutRect->w.unit == UNIT_VH
			         || layoutRect->w.unit == UNIT_REM ) {
				/* Viewport/rem units are concrete lengths, not percentages of
				 * the immediate Clay parent. Resolve them against the current
				 * presentation extent. */
				layoutCfg.sizing.width = CLAY_SIZING_FIXED(
					wui_resolve_unit( layoutRect->w, parentVW ) );
			} else if ( layoutRect->w.unit == UNIT_NORM && layoutRect->w.value > 0 ) {
				layoutCfg.sizing.width = CLAY_SIZING_PERCENT( layoutRect->w.value );
			} else if ( layoutRect->w.unit == UNIT_AUTO ) {
				/* Reuse the preceding Clay frame when available; FIT handles the
				 * first/unmeasured frame. */
				if ( item->resolvedRect.w > 0 ) {
					layoutCfg.sizing.width = CLAY_SIZING_FIXED( item->resolvedRect.w );
				} else {
					layoutCfg.sizing.width = ( maxW > 0.0f )
						? CLAY_SIZING_FIT( minW, maxW )
						: CLAY_SIZING_FIT( minW );
				}
			} else {
				/* An omitted width is the flex cross-axis default, not a literal
				 * zero-width box.  The legacy resolver necessarily leaves `w` at
				 * zero for this case; pinning Clay to FIXED(0) collapsed column
				 * children such as the in-game menu's form and two-column rows.
				 * Their percent-width descendants then inherited zero, leaving only
				 * border slivers while MSDF labels overflowed outside the panel.
				 * Match flexbox stretch on a column parent's cross axis.  Row
				 * children keep their measured/resolved width (or FIT when there is
				 * no measurement) because width is their main axis. */
				if ( w > 0.0f ) {
					layoutCfg.sizing.width = CLAY_SIZING_FIXED( w );
				} else if ( parentDirection == WUI_LAYOUT_COLUMN ) {
					layoutCfg.sizing.width = ( maxW > 0.0f )
						? CLAY_SIZING_GROW( minW, maxW )
						: CLAY_SIZING_GROW( minW );
				} else {
					layoutCfg.sizing.width = ( maxW > 0.0f )
						? CLAY_SIZING_FIT( minW, maxW )
						: CLAY_SIZING_FIT( minW );
				}
			}

			if ( growH ) {
				layoutCfg.sizing.height = ( maxH > 0.0f )
					? CLAY_SIZING_GROW( minH, maxH )
					: CLAY_SIZING_GROW( minH );
			} else if ( layoutRect->h.unit == UNIT_PX ) {
				/* Scale authored px by dpiScale — see the width branch above.
				 * This keeps a FIXED-px flex box in the same 2× space as its
				 * dpi-scaled rows so it holds them without overflow. */
				layoutCfg.sizing.height = CLAY_SIZING_FIXED( layoutRect->h.value * WiredUI_GetDpiScale() );
			} else if ( layoutRect->h.unit == UNIT_VW
			         || layoutRect->h.unit == UNIT_VH
			         || layoutRect->h.unit == UNIT_REM ) {
				layoutCfg.sizing.height = CLAY_SIZING_FIXED(
					wui_resolve_unit( layoutRect->h, parentVH ) );
			} else if ( layoutRect->h.unit == UNIT_NORM && layoutRect->h.value > 0 ) {
				layoutCfg.sizing.height = CLAY_SIZING_PERCENT( layoutRect->h.value );
			} else if ( layoutRect->h.unit == UNIT_AUTO ) {
				/* `height FIT` on a flex container with children lets Clay
				 * measure the true content height so the box hugs its rows.
				 *
				 * For AUTO-height LEAVES / text (no children Clay can measure),
				 * keep the preceding Clay snapshot — FIT would collapse
				 * them to 0. The `childCount` gate preserves the deeply-nested
				 * emit fix (resolvedRect fallback for unmeasurable items). */
				if ( item->childCount > 0 ) {
					layoutCfg.sizing.height = ( maxH > 0.0f )
						? CLAY_SIZING_FIT( minH, maxH )
						: CLAY_SIZING_FIT( minH );
				} else if ( item->resolvedRect.h > 0 ) {
					layoutCfg.sizing.height = CLAY_SIZING_FIXED( item->resolvedRect.h );
				} else {
					layoutCfg.sizing.height = ( maxH > 0.0f )
						? CLAY_SIZING_FIT( minH, maxH )
						: CLAY_SIZING_FIT( minH );
				}
			} else {
				layoutCfg.sizing.height = CLAY_SIZING_FIXED( h );
			}
		}

		/* Flex container properties — emitted for ALL items (Path A or B)
		 * because Clay applies them to descendants regardless of how this
		 * item itself was positioned. */
		if ( item->isFlexContainer ) {
			float parentW = w > 0 ? w : (float) wui_clay_lastWidth;
			float parentH = h > 0 ? h : (float) wui_clay_lastHeight;
			layoutCfg.layoutDirection = ( item->flexContainer.direction == WUI_LAYOUT_COLUMN )
			                          ? CLAY_TOP_TO_BOTTOM : CLAY_LEFT_TO_RIGHT;
			layoutCfg.childGap        = (uint16_t) wui_resolve_unit( item->flexContainer.gap, parentW );
			layoutCfg.padding.top     = (uint16_t) wui_resolve_unit( item->flexContainer.padding[0], parentH );
			layoutCfg.padding.right   = (uint16_t) wui_resolve_unit( item->flexContainer.padding[1], parentW );
			layoutCfg.padding.bottom  = (uint16_t) wui_resolve_unit( item->flexContainer.padding[2], parentH );
			layoutCfg.padding.left    = (uint16_t) wui_resolve_unit( item->flexContainer.padding[3], parentW );
			layoutCfg.childAlignment  = wui_align_to_clay(
				item->flexContainer.direction,
				item->flexContainer.align,
				item->flexContainer.justify );
		}
		else if ( !item->hudElement[0] && displayText[0] ) {
			/* text-bearing non-flex
			 * items carry their alignment + offset padding on the parent
			 * CLAY block. Horizontal alignment via childAlignment.x is
			 * what actually positions single-line text in Clay v0.14 (the
			 * CLAY_TEXT_CONFIG.textAlignment only takes effect once text
			 * wraps — see helper-fn comment).
			 *
			 * Mirrors legacy cl_wired_ui.c:2591-2594 + 2635-2641:
			 *   textalign  CENTER/RIGHT → childAlignment.x = CENTER/RIGHT
			 *   textaligny==0 + h>charSize → auto-vert-center (.y = CENTER)
			 *   textaligny!=0              → explicit Y offset (padding.top)
			 *   textalignx                  → explicit X offset (padding.left) */
			layoutCfg.childAlignment.x = wui_clay_alignx_from_item( item->textalign );
			layoutCfg.childAlignment.y = textVAlign;
			layoutCfg.padding.left     = textPadLeft;
			layoutCfg.padding.top      = textPadTop;
		}

		/* emit the 6-layered background BEFORE
		 * opening the item's main CLAY block. Clay v0.14 floating elements
		 * render ABOVE non-floating siblings in z-order; emitting the bg
		 * layers as separate floating elements (attach-to-root) BEFORE the
		 * item's CLAY block opens places them earlier in the command stream
		 * so the item's nested non-floating children render ON TOP. This
		 * fixes the visual obstruction where the layered bg
		 * blanketed the foreground UI. */
		/* Resolve the effective background from the push-time INTENT (how this
		 * menu was opened) against the item's .wui-authored bgLayerFlags. The
		 * intent applies only to the stack-top MENU panel; every other panel
		 * (popup/loading/multi-layer) resolves to INHERIT, so its authored
		 * background stays byte-identical. */
		/* Only the background-owning item (the one that authored a
		 * `background "layered"` line → bgLayerFlags != 0, i.e. the root
		 * container) emits a background. wui_clay_emit_item recurses over the
		 * WHOLE tree, so gating on bgLayerFlags is what keeps the emit to a
		 * single pass — without it, DIM/SCENE would fire for every nested item
		 * (thousands of scene emits → Clay element-count overflow). */
		/* The item-level background hook is gone. It let whichever menu item
		 * happened to carry `background "layered"` decide what the whole
		 * screen looked like, which is how the backdrop ended up owned by the
		 * menu tree rather than by a layer — and how two emitters wound up on
		 * the same zIndex arguing about precedence.
		 *
		 * Backgrounds now belong to WUI_LAYER_BG_DARK / _BG_ANIMATED and are
		 * requested by the stack-top menu's `backdrop` preset. The authored
		 * bgLayerFlags remain parsed and are still read by the layer emit for
		 * their content; nothing draws from here. */

		/* Two CLAY emit blocks because Clay's CLAY({...}) macro is a for-loop
		 * that opens + configures + closes around its body — there's no
		 * clean way to conditionally include `.floating` without splitting
		 * the macro invocation. The content block (IMAGE / TEXT / recursive
		 * children) is identical between the two; minor duplication is
		 * accepted vs the alternative of restructuring with Clay__OpenElement
		 * directly. */
#ifdef _DEBUG
		Com_Log( SEV_TRACE, LOG_CH(ch_ui),
			"WUI_TRACE EMIT item='%s' xywh=(%.1f,%.1f,%.1f,%.1f) floating=%d parentIsContainer=%d "
			"sizing.w=(t%d,min%.1f,max%.1f) sizing.h=(t%d,min%.1f,max%.1f) dir=%d gap=%d\n",
			item->name[ 0 ] ? item->name : "<anon>",
			x, y, w, h,
			(int) useFloating, (int) parentIsContainer,
			(int) layoutCfg.sizing.width.type,  layoutCfg.sizing.width.size.minMax.min,  layoutCfg.sizing.width.size.minMax.max,
			(int) layoutCfg.sizing.height.type, layoutCfg.sizing.height.size.minMax.min, layoutCfg.sizing.height.size.minMax.max,
			(int) layoutCfg.layoutDirection, (int) layoutCfg.childGap );
#endif

		/* apply wuiAnim offsets to the floating x/y. scroll-x
		 * multiplies the normalized offset by the parent's width (use
		 * resolvedRect.w from this emit context as the "container"). For
		 * slide-up/down the y offset is normalized against viewport height
		 * — sufficient for the marquee/transition canary; modder can
		 * refine via `speed` / `duration` overrides. */
		float animX = x;
		float animY = y;
		if ( item->animationName[ 0 ] ) {
			if ( !Q_stricmp( item->animationName, "scroll-x" ) ) {
				/* Use the parent panel width (last-resolved viewport
				 * width is good enough; the marquee container is
				 * full-width by convention). */
				animX += item->animOffsetX * (float) wui_clay_lastWidth;
			} else if ( !Q_stricmp( item->animationName, "slide-up" )
			         || !Q_stricmp( item->animationName, "slide-down" )
			         || !Q_stricmp( item->animationName, "scan-y" ) ) {
				animY += item->animOffsetY * (float) wui_clay_lastHeight;
			}
		}
		/* When focused, the row paints a FLAT solid highlight as its own
		 * background — no gradient bar, no left→right fade (the user disliked
		 * the effect). `.image` stays NULL; the accent tint at a subtle alpha
		 * fills the full row width. The caption renders on top. */
		void      *rowImageData = NULL;
		Clay_Color rowBg        = bg;
		if ( hasFocusGradient ) {
			rowBg     = focusGradTint;
			rowBg.a  *= 0.30f;
		}

		if ( useFloating ) {
			CLAY({
				.id = { .id = clayId },
				.layout = layoutCfg,
				.floating = {
					.attachTo    = CLAY_ATTACH_TO_ROOT,
					.offset      = { animX, animY },
					.attachPoints = { CLAY_ATTACH_POINT_LEFT_TOP, CLAY_ATTACH_POINT_LEFT_TOP },
					.pointerCaptureMode = item->decoration
					                    ? CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH
					                    : CLAY_POINTER_CAPTURE_MODE_CAPTURE
				},
				.image = { .imageData = rowImageData },
				.backgroundColor = rowBg,
				.userData = wui_clay_command_tag( perspectiveOwner,
					(uintptr_t)wui_clay_composite_tag_item_bg( item->compositeMode ) ),
				.cornerRadius = cornerR,
				.border = {
					.color = borderColor,
					.width = { borderL, borderR, borderT, borderB, 0 }
				}
			}) {
				if ( hasBackground ) {
					qhandle_t hShader = re.RegisterShader( item->background );
					if ( hShader ) {
						CLAY({
							.layout = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) } },
							.image  = { .imageData = (void*)(uintptr_t) hShader },
							.userData = wui_clay_command_tag( perspectiveOwner, 0u ),
							.backgroundColor = { 255, 255, 255, 255.0f * wui_compositor_panel_alpha }
						}) {}
					}
				}
				/* layered background now emitted BEFORE the
				 * CLAY block opens (see above) to fix z-order. Removed
				 * from inside-block sites. */
				/* unified custom-draw emit. Placed AFTER the
				 * background image (so custom-draw paints over a textured
				 * panel) and BEFORE displayText (so text labels overlay the
				 * custom-draw output — playersettings.wmenu's effectfield
				 * has both `text "Effect:"` and `ownerdraw "effects"`). */
				wui_clay_emit_customdraw_for_item( item, x, y, w, h, effectiveFg,
					perspectiveOwner );
				/* viewport itemDef — provider-driven world/preview
				 * render dispatched alongside the custom-draw channel. */
				if ( item->type == ITEM_TYPE_VIEWPORT ) {
					wui_clay_emit_viewport_for_item( item, x, y, w, h, effectiveFg );
				}
				/* scorelist_widget itemDef — declarative scoreboard
				 * dispatch through the same CUSTOM channel. */
				else if ( item->type == ITEM_TYPE_SCORELIST_WIDGET ) {
					wui_clay_emit_scorelist_widget_for_item( item, x, y, w, h, effectiveFg );
				}
				/* console_view itemDef — WUI_LAYER_CONSOLE walk's
				 * dispatch into elements/console.c::Con_DrawConsole. */
				else if ( item->type == ITEM_TYPE_CONSOLE_VIEW ) {
					wui_clay_emit_console_view_for_item( item, x, y, w, h, effectiveFg );
				}
				if ( !item->hudElement[0] && displayText[0] ) {
					uint16_t    fontSlot = wui_clay_font_slot_for_face( fontName );
					Clay_String s;
					Clay_Color  fg = wui_clay_color_of( effectiveFg, wui_compositor_panel_alpha );
					s.isStaticallyAllocated = qfalse;
					s.length                = (int32_t) strlen( displayText );
					s.chars                 = displayText;
					CLAY_TEXT( s, CLAY_TEXT_CONFIG({
						/* sub-gap 4: textstyle plumbed via .userData; the
						 * render command picks it up at dispatch time. The
						 * cast intentionally narrows to the dropshadow bit
						 * we care about — full textstyle isn't needed
						 * downstream and a plain qboolean is cheaper to
						 * round-trip through void*. */
						.userData      = wui_clay_command_tag( perspectiveOwner,
							textHasShadow ? WUI_TEXT_TAG_SHADOW : 0u ),
						.fontId        = fontSlot,
						.fontSize      = (uint16_t) effectiveFontPointSize,
						.letterSpacing = (uint16_t) item->letterSpacing,
						.textColor     = fg,
						.wrapMode      = CLAY_TEXT_WRAP_NONE,
						.textAlignment = textHAlign
					}) );
				}
				for ( i = 0; i < item->childCount; i++ ) {
					if ( item->children[ i ] ) {
						/* pass item->isFlexContainer so
						 * nested leaf items stay on Clay native flex
						 * instead of falling through to ATTACH_TO_ROOT
						 * floating.
						 * pass this container's direction
						 * so children's `grow N` shorthand lands on the
						 * correct main axis. */
						wui_clay_emit_item( panel, item->children[ i ], item->isFlexContainer,
						                    item->isFlexContainer ? item->flexContainer.direction : parentDirection,
											perspectiveOwner );
					}
				}
			}
		} else {
			/* Path A native flex child — no .floating, sizing from
			 * layoutCfg.sizing (set above from flexChild/wuiRect). The focus
			 * highlight paints as this element's own image/background (see the
			 * useFloating branch above for the rationale). */
			CLAY({
				.id = { .id = clayId },
				.layout = layoutCfg,
				.image = { .imageData = rowImageData },
				.backgroundColor = rowBg,
				.userData = wui_clay_command_tag( perspectiveOwner,
					(uintptr_t)wui_clay_composite_tag_item_bg( item->compositeMode ) ),
				.cornerRadius = cornerR,
				/* A `scroll` flex container becomes a vertical scroll viewport:
				 * Clay clips children to this box and childOffset (driven by the
				 * wheel via Clay_UpdateScrollContainers) scrolls them. The stable
				 * .id lets Clay_GetScrollContainerData track the offset. */
				.clip = {
					.vertical    = ( item->isFlexContainer && item->flexContainer.scroll ),
					/* fetch by KNOWN clayId, not Clay_GetScrollOffset(): this
					 * config is evaluated BEFORE Clay attaches the element's id,
					 * so the open element's id is still 0 and Clay_GetScrollOffset()
					 * would read {0,0} and freeze the scroll. */
					.childOffset = ( item->isFlexContainer && item->flexContainer.scroll )
					             ? wui_clay_scroll_offset_for( clayId ) : (Clay_Vector2){ 0, 0 }
				},
				.border = {
					.color = borderColor,
					.width = { borderL, borderR, borderT, borderB, 0 }
				}
			}) {
				if ( hasBackground ) {
					qhandle_t hShader = re.RegisterShader( item->background );
					if ( hShader ) {
						CLAY({
							.layout = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) } },
							.image  = { .imageData = (void*)(uintptr_t) hShader },
							.userData = wui_clay_command_tag( perspectiveOwner, 0u ),
							.backgroundColor = { 255, 255, 255, 255.0f * wui_compositor_panel_alpha }
						}) {}
					}
				}
				/* layered background now emitted BEFORE the
				 * CLAY block opens (see above) to fix z-order. Removed
				 * from inside-block sites. */
				/* unified custom-draw emit. Placed AFTER the
				 * background image (so custom-draw paints over a textured
				 * panel) and BEFORE displayText (so text labels overlay the
				 * custom-draw output — playersettings.wmenu's effectfield
				 * has both `text "Effect:"` and `ownerdraw "effects"`). */
				wui_clay_emit_customdraw_for_item( item, x, y, w, h, effectiveFg,
					perspectiveOwner );
				/* viewport itemDef — provider-driven world/preview
				 * render dispatched alongside the custom-draw channel. */
				if ( item->type == ITEM_TYPE_VIEWPORT ) {
					wui_clay_emit_viewport_for_item( item, x, y, w, h, effectiveFg );
				}
				/* scorelist_widget itemDef — declarative scoreboard
				 * dispatch through the same CUSTOM channel. */
				else if ( item->type == ITEM_TYPE_SCORELIST_WIDGET ) {
					wui_clay_emit_scorelist_widget_for_item( item, x, y, w, h, effectiveFg );
				}
				/* console_view itemDef — WUI_LAYER_CONSOLE walk's
				 * dispatch into elements/console.c::Con_DrawConsole. */
				else if ( item->type == ITEM_TYPE_CONSOLE_VIEW ) {
					wui_clay_emit_console_view_for_item( item, x, y, w, h, effectiveFg );
				}
				if ( !item->hudElement[0] && displayText[0] ) {
					uint16_t    fontSlot = wui_clay_font_slot_for_face( fontName );
					Clay_String s;
					Clay_Color  fg = wui_clay_color_of( effectiveFg, wui_compositor_panel_alpha );
					s.isStaticallyAllocated = qfalse;
					s.length                = (int32_t) strlen( displayText );
					s.chars                 = displayText;
					CLAY_TEXT( s, CLAY_TEXT_CONFIG({
						/* sub-gap 4: textstyle plumbed via .userData; the
						 * render command picks it up at dispatch time. The
						 * cast intentionally narrows to the dropshadow bit
						 * we care about — full textstyle isn't needed
						 * downstream and a plain qboolean is cheaper to
						 * round-trip through void*. */
						.userData      = wui_clay_command_tag( perspectiveOwner,
							textHasShadow ? WUI_TEXT_TAG_SHADOW : 0u ),
						.fontId        = fontSlot,
						.fontSize      = (uint16_t) effectiveFontPointSize,
						.letterSpacing = (uint16_t) item->letterSpacing,
						.textColor     = fg,
						.wrapMode      = CLAY_TEXT_WRAP_NONE,
						.textAlignment = textHAlign
					}) );
				}
				for ( i = 0; i < item->childCount; i++ ) {
					if ( item->children[ i ] ) {
						/* pass item->isFlexContainer so
						 * nested leaf items stay on Clay native flex
						 * instead of falling through to ATTACH_TO_ROOT
						 * floating.
						 * pass this container's direction. */
						wui_clay_emit_item( panel, item->children[ i ], item->isFlexContainer,
						                    item->isFlexContainer ? item->flexContainer.direction : parentDirection,
											perspectiveOwner );
					}
				}
				/* macOS-style scrollbar for a `scroll` flex container, emitted
				 * as the last (floating) child so it attaches to this element
				 * and rides above the content. No-op unless content overflows. */
				if ( item->isFlexContainer && item->flexContainer.scroll ) {
					wui_clay_emit_flex_scrollbar( clayId, x, y, w, h );
				}
			}
		}
	}
}

/* ── Repeat-block expansion (field substitution) ──────
 *
 * Resolve source (store prefix walk or compiled Lua chunk), emit N row
 * instances of the template. Per-row data accessors are VM-routed via
 * wui_clay_chunk_array_item_string / _field. Substitution
 * recognises both `{{ row }}` (whole scalar) and `{{ row.<field> }}`
 * (table-of-tables field access). Per-row Clay IDs use
 * "<panel>/<template>_i<rowIdx>" for hit-test reverse-lookup stability.
 *
 * Field name parse: alphanumeric + underscore characters only (rejects
 * dots, so `row.nested.field` is treated as no-match and substituted as
 * empty; nested access is later work). */

/* Row context for substitution. Carries enough info to resolve either
 * `{{ row }}` (scalar via cached rowScalar) or `{{ row.<field> }}` (via
 * chunk-array field accessor + current rowIdx). Lua source paths set vm
 * + rowIdx + isLua=qtrue; store source paths set isLua=qfalse + use a
 * store prefix instead. */
typedef struct {
	wui_vm_kind_t   vm;
	int             rowIdx;        /* 1-based row index */
	qboolean        isLua;         /* qtrue: query chunk-array; qfalse: store-prefix */
	const char     *rowScalar;     /* used by scalar {{ row }} when isLua */
	const char     *storePrefix;   /* used when !isLua: e.g. "game.scores.0" */
} wui_row_ctx_t;

static void wui_clay_substitute_row_text( const char *src,
                                            const wui_row_ctx_t *ctx,
                                            char *dst, size_t dstSize )
{
	const char *p   = src;
	char       *out = dst;
	size_t      rem = dstSize > 0 ? dstSize - 1 : 0;

	while ( *p && rem > 0 ) {
		if ( p[0] == '{' && p[1] == '{' ) {
			const char *q = p + 2;
			while ( *q == ' ' || *q == '\t' ) q++;
			if ( q[0] == 'r' && q[1] == 'o' && q[2] == 'w' ) {
				const char *r = q + 3;
				char        field[ 64 ];
				int         fieldLen = 0;
				char        valueBuf[ 128 ];
				const char *value     = NULL;
				size_t      vlen;

				/* Optional ".field" — alphanumeric + underscore only. */
				if ( *r == '.' ) {
					r++;
					while ( ( ( *r >= 'a' && *r <= 'z' ) ||
					          ( *r >= 'A' && *r <= 'Z' ) ||
					          ( *r >= '0' && *r <= '9' ) ||
					            *r == '_' ) &&
					        fieldLen < (int) sizeof( field ) - 1 ) {
						field[ fieldLen++ ] = *r++;
					}
					field[ fieldLen ] = '\0';
				}

				/* Trailing whitespace + `}}`. */
				while ( *r == ' ' || *r == '\t' ) r++;
				if ( r[0] == '}' && r[1] == '}' ) {
					if ( ctx ) {
						if ( fieldLen > 0 ) {
							/* {{ row.<field> }} */
							if ( ctx->isLua ) {
								if ( wui_clay_chunk_array_item_field( ctx->vm, ctx->rowIdx, field,
								                                       valueBuf, sizeof( valueBuf ) ) ) {
									value = valueBuf;
								}
							} else if ( ctx->storePrefix ) {
								char storeKey[ 256 ];
								const wuiStoreEntry_t *e;
								Com_sprintf( storeKey, sizeof( storeKey ),
									"%s.%s", ctx->storePrefix, field );
								e = WiredStore_Get( storeKey );
								if ( e && e->text[0] ) value = e->text;
							}
						} else {
							/* {{ row }} */
							if ( ctx->isLua ) {
								value = ctx->rowScalar;
							} else if ( ctx->storePrefix ) {
								char storeKey[ 256 ];
								const wuiStoreEntry_t *e;
								Com_sprintf( storeKey, sizeof( storeKey ),
									"%s.name", ctx->storePrefix );
								e = WiredStore_Get( storeKey );
								if ( e && e->text[0] ) value = e->text;
							}
						}
					}

					vlen = value ? strlen( value ) : 0;
					if ( vlen > rem ) vlen = rem;
					if ( vlen ) memcpy( out, value, vlen );
					out += vlen;
					rem -= vlen;
					p = r + 2;
					continue;
				}
			}
		}
		*out++ = *p++;
		rem--;
	}
	*out = '\0';
}

/* Per-row text buffer pool. Each row's substituted text needs to live
 * past `wui_clay_emit_repeat_row`'s return because Clay defers reading
 * the bytes until its render pass — a sibling stack-local would be
 * reused across iterations and collapse all rows to the last write.
 * Fixed cap at WUI_REPEAT_MAX_ROWS per emit dispatch; counter resets
 * inside `wui_clay_emit_repeat_block`. */
#define WUI_REPEAT_MAX_ROWS         64
#define WUI_REPEAT_CLONE_MAX_DEPTH  3
static char wui_repeat_row_text_pool[ WUI_REPEAT_MAX_ROWS ][ 256 ];

/* Recursive deep-copy of a template subtree into a Z_Malloc'd scratch tree,
 * with per-row Mustache substitution into string fields that authors may
 * template (`text`, `background`, `storeBindIcon`). The clone then enters
 * the existing wui_clay_emit_item recursive pipeline unchanged — no
 * dual-path branching in the emit code, one pipeline for all items.
 *
 * Pointer fields that must NOT be shared with the template (or that would
 * cause re-entry / double-free) are cleared on the clone: multiData,
 * ifBlock, repeatBlock, customDrawContext, animationId. Depth-bounded to
 * WUI_REPEAT_CLONE_MAX_DEPTH so a malformed template can't recurse without
 * bound.
 *
 * Clones come from the per-frame scratch arena (bump-allocated, reset once per
 * frame at Arena_Reset in the compositor emit) — NOT Z_Malloc — so an N-row x
 * M-item list no longer does N*M heap alloc/free every frame. There is no
 * matching free: the arena reset reclaims all clones at once. Arena_Alloc
 * returns NULL on overflow, which the existing NULL handling skips gracefully. */
static wiredItemDef_t *wui_clay_clone_template( const wiredItemDef_t *templ,
                                                  const wui_row_ctx_t *ctx,
                                                  int rowIdx,
                                                  int depth )
{
	wiredItemDef_t *clone;
	int             i;

	if ( !templ || depth > WUI_REPEAT_CLONE_MAX_DEPTH ) return NULL;
	if ( !wui_clay_scratch_arena ) return NULL;

	clone = (wiredItemDef_t *) Arena_Alloc( wui_clay_scratch_arena,
	                                        sizeof( *clone ), sizeof( void * ) );
	if ( !clone ) return NULL;

	memcpy( clone, templ, sizeof( *clone ) );
	clone->multiData         = NULL;
	clone->ifBlock           = NULL;
	clone->repeatBlock       = NULL;
	clone->customDrawContext = NULL;
	clone->animationId       = 0;
	clone->childCount        = 0;

	/* Per-row name suffix so Clay-ID derivation (which hashes item->name)
	 * produces a distinct id per row. Without this the second row's clone
	 * collides with the first and Clay rejects the duplicate declaration. */
	{
		size_t nameLen = strlen( clone->name );
		Com_sprintf( clone->name + nameLen,
		             sizeof( clone->name ) - nameLen,
		             "_i%d", rowIdx );
	}

	if ( clone->text[0] ) {
		char buf[ sizeof( clone->text ) ];
		wui_clay_substitute_row_text( clone->text, ctx, buf, sizeof( buf ) );
		Q_strncpyz( clone->text, buf, sizeof( clone->text ) );
	}
	if ( clone->background[0] ) {
		char buf[ sizeof( clone->background ) ];
		wui_clay_substitute_row_text( clone->background, ctx, buf, sizeof( buf ) );
		Q_strncpyz( clone->background, buf, sizeof( clone->background ) );
	}
	if ( clone->storeBindIcon[0] ) {
		char buf[ sizeof( clone->storeBindIcon ) ];
		wui_clay_substitute_row_text( clone->storeBindIcon, ctx, buf, sizeof( buf ) );
		Q_strncpyz( clone->storeBindIcon, buf, sizeof( clone->storeBindIcon ) );
	}

	for ( i = 0; i < templ->childCount && i < WIRED_MAX_ITEMS_PER_MENU; i++ ) {
		wiredItemDef_t *ch;
		if ( !templ->children[ i ] ) continue;
		ch = wui_clay_clone_template( templ->children[ i ], ctx, rowIdx, depth + 1 );
		if ( ch ) {
			clone->children[ clone->childCount++ ] = ch;
		}
	}

	return clone;
}

static void wui_clay_emit_repeat_row( const wiredMenuDef_t *panel,
                                       const wiredItemDef_t *templ,
                                       int rowIdx,
                                       const wui_row_ctx_t *ctx,
									   const wiredItemDef_t *perspectiveOwner )
{
	float       x, y, w, h;
	Clay_Color  bg, borderColor;
	uint16_t    borderW;
	const char *fontName = templ->fontName[0] ? templ->fontName : NULL;
	char       *rowText;
	uint32_t    rowId;
	char        rowPath[ 192 ];
	Clay_String idStr;

	if ( !templ->visible ) return;
	if ( rowIdx < 0 || rowIdx >= WUI_REPEAT_MAX_ROWS ) return;

	/* Per-row rect: stack rows vertically from the template's resolvedRect.
	 * The template's resolvedRect comes from the legacy layout engine;
	 * each row offsets by its height. */
	x = templ->resolvedRect.x;
	y = templ->resolvedRect.y + rowIdx * ( templ->resolvedRect.h > 0 ? templ->resolvedRect.h : 24.0f );
	w = templ->resolvedRect.w;
	h = templ->resolvedRect.h > 0 ? templ->resolvedRect.h : 24.0f;
	if ( w <= 0.0f ) w = 200.0f;   /* sensible default for unresolved rows */

	/* Container / nested template: per-row scratch-arena clone tree. The clone
	 * reuses the standard wui_clay_emit_item recursive pipeline (no dual-path
	 * branching); image leaves resolve via the existing background-shader path,
	 * text leaves via the existing displayText path. Per-row Mustache
	 * substitution runs at clone time. No free — the clone lives in the per-frame
	 * scratch arena, reclaimed wholesale at the next frame's Arena_Reset. */
	if ( templ->childCount > 0 ) {
		wiredItemDef_t *clone = wui_clay_clone_template( templ, ctx, rowIdx, 0 );
		if ( clone ) {
			clone->resolvedRect.x = x;
			clone->resolvedRect.y = y;
			clone->resolvedRect.w = w;
			clone->resolvedRect.h = h;
			wui_clay_emit_item( panel, clone, qfalse, WUI_LAYOUT_ROW,
				perspectiveOwner );
		}
		return;
	}

	rowText = wui_repeat_row_text_pool[ rowIdx ];

	bg          = wui_clay_color_of( templ->backcolor,   wui_compositor_panel_alpha );
	borderColor = wui_clay_color_of( templ->bordercolor, wui_compositor_panel_alpha );
	borderW     = ( templ->border != WINDOW_BORDER_NONE && templ->bordersize > 0.0f )
	            ? (uint16_t) templ->bordersize : 0;

	/* Substitute placeholders in template text against the row context.
	 * `{{ row }}` and `{{ row.<field> }}` both supported. Output lands
	 * in the per-row pool slot above so it survives Clay's deferred
	 * read at render time. */
	wui_clay_substitute_row_text( templ->text, ctx, rowText, 256 );

	/* Per-row Clay ID: "<panel>/<template>_i<rowIdx>". */
	Com_sprintf( rowPath, sizeof( rowPath ), "%s/%s_i%d",
		panel->name,
		templ->name[0] ? templ->name : "anon",
		rowIdx );
	idStr.isStaticallyAllocated = qfalse;
	idStr.length                = (int32_t) strlen( rowPath );
	idStr.chars                 = rowPath;
	rowId = Clay_GetElementId( idStr ).id;
	wui_id_map_record( rowId, templ, panel );

	CLAY({
		.id     = { .id = rowId },
		.layout = {
			.sizing = { CLAY_SIZING_FIXED( w ), CLAY_SIZING_FIXED( h ) }
		},
		.floating = {
			.attachTo    = CLAY_ATTACH_TO_ROOT,
			.offset      = { x, y },
			.attachPoints = { CLAY_ATTACH_POINT_LEFT_TOP, CLAY_ATTACH_POINT_LEFT_TOP }
		},
		.backgroundColor = bg,
		.userData = wui_clay_command_tag( perspectiveOwner, 0u ),
		.border = {
			.color = borderColor,
			.width = { borderW, borderW, borderW, borderW, 0 }
		}
	}) {
		if ( rowText[0] ) {
			uint16_t    fontSlot = wui_clay_font_slot_for_face( fontName );
			Clay_String s;
			Clay_Color  fg = wui_clay_color_of( templ->forecolor, wui_compositor_panel_alpha );

			s.isStaticallyAllocated = qfalse;
			s.length                = (int32_t) strlen( rowText );
			s.chars                 = rowText;

			CLAY_TEXT( s, CLAY_TEXT_CONFIG({
				.userData      = wui_clay_command_tag( perspectiveOwner, 0u ),
				.fontId        = fontSlot,
				.fontSize      = (uint16_t)( templ->fontPointSize > 0 ? templ->fontPointSize : 16 ),
				.letterSpacing = (uint16_t) templ->letterSpacing,
				.textColor     = fg,
				.wrapMode      = CLAY_TEXT_WRAP_NONE
			}) );
		}
	}
}

static void wui_clay_emit_repeat_block( const wiredMenuDef_t *panel,
                                         const wiredItemDef_t *containerItem,
										 const wiredItemDef_t *perspectiveOwner )
{
	const wiredRepeatBlock_t *rb = containerItem->repeatBlock;
	wiredRepeatBlock_t       *rbMut;   /* for once-only warn flag mutation */
	int                       n = 0;
	int                       i;

	if ( !rb || !rb->templateItem ) return;
	rbMut = (wiredRepeatBlock_t *) rb;   /* discard const for warn flag */

	if ( rb->sourceIsLua ) {
		/* Lua source: call chunk, expect dense 1..N array. */
		wui_vm_kind_t vm = wui_clay_vm_for_panel( panel );
		if ( rb->sourceLuaChunk == WIRED_CHUNK_NOREF ) return;
		n = wui_clay_chunk_call_array_len( vm, rb->sourceLuaChunk );
		if ( n <= 0 ) {
			if ( !rbMut->sourceWarned ) {
				Com_Log( SEV_WARN, LOG_CH(ch_ui),
					"WiredUI/repeat: Lua source for '%s' returned no rows (or non-dense table) — Q-3=a\n",
					containerItem->name[0] ? containerItem->name : "<anon>" );
				rbMut->sourceWarned = qtrue;
			}
			return;
		}

		for ( i = 0; i < n; i++ ) {
			char           rowScalar[ 64 ];
			wui_row_ctx_t  ctx;

			/* Pre-read the scalar form into the buffer so substitute_row_text
			 * sees the bytes even after release.  Field-form access goes
			 * directly to the held chunk array. */
			if ( !wui_clay_chunk_array_item_string( vm, i + 1, rowScalar, sizeof( rowScalar ) ) ) {
				rowScalar[0] = '\0';
			}

			ctx.vm          = vm;
			ctx.rowIdx      = i + 1;
			ctx.isLua       = qtrue;
			ctx.rowScalar   = rowScalar;
			ctx.storePrefix = NULL;

			wui_clay_emit_repeat_row( panel, rb->templateItem, i, &ctx,
				perspectiveOwner );
		}

		wui_clay_chunk_array_release( vm );
	}
	else if ( rb->source[0] ) {
		/* Store source: walk prefix.<i>.<field> with countBind for explicit
		 * count or scan-until-null for autodetect. `{{ row.<field> }}`
		 * resolves to a prefix.<i>.<field> store lookup. */
		if ( rb->countBind[0] ) {
			const wuiStoreEntry_t *cnt = WiredStore_Get( rb->countBind );
			n = cnt ? (int) cnt->value : 0;
		} else {
			char k[ 256 ];
			for ( n = 0; n < 64; n++ ) {
				Com_sprintf( k, sizeof( k ), "%s.%d.name", rb->source, n );
				if ( !WiredStore_Get( k ) ) break;
			}
		}
		if ( n > WUI_REPEAT_MAX_ROWS ) n = WUI_REPEAT_MAX_ROWS;

		for ( i = 0; i < n; i++ ) {
			char           rowPrefix[ 256 ];
			wui_row_ctx_t  ctx;
			Com_sprintf( rowPrefix, sizeof( rowPrefix ), "%s.%d", rb->source, i );

			ctx.vm          = WUI_VM_SYSTEM;   /* irrelevant — store source */
			ctx.rowIdx      = i + 1;
			ctx.isLua       = qfalse;
			ctx.rowScalar   = NULL;
			ctx.storePrefix = rowPrefix;

			wui_clay_emit_repeat_row( panel, rb->templateItem, i, &ctx,
				perspectiveOwner );
		}
	}
}

/* Emit a single panel: open the root container (full-screen, no background),
 * then walk all items. The root itself doesn't draw — only its children's
 * floating elements produce render commands. */
static void wui_clay_emit_panel( const wiredMenuDef_t *menu )
{
	int i;

	/* Resolve only the viewport-relative panel containing block. Clay owns the
	 * complete descendant tree; there is no hand-written flex pre-pass. */
	wui_clay_resolve_panel_root( (wiredMenuDef_t *) menu );

	/* select Path A (native Clay flex) vs Path B (CLAY_FLOATING +
	 * resolvedRect pinning) for this panel's items. Set ONCE per panel
	 * walk; emit_item reads the static flag to choose its CLAY declaration
	 * shape. Reset is implicit on the next emit_panel call. */
	wui_clay_current_pathB = menu->pathBKind;

	/* the !visible && !fullscreen early-return that existed in
	 * earlier revisions is redundant once the panel is in visible_panels[]
	 * (the visible-only narrow already filters by WiredUI_GetActiveMenu). Some
	 * test fixtures author menuDef.visible=0 so the panel only appears
	 * when explicitly pushed; rejecting them here would mean active
	 * pushed panels never emit. */

	/* Panel-level backcolor fill emitted as a CLAY_FLOATING rectangle
	 * pinned to the panel's resolvedRect. Mirrors the menu->background
	 * image block below in shape (sizing + floating attachment) and the
	 * item-level .backgroundColor pattern for color conversion. No
	 * explicit alpha gate at emit time — the dispatch's `color[3] <= 0`
	 * check drops zero-alpha rects cheaply.
	 *
	 * Painter's order: backcolor fill (this) → background shader (below) →
	 * items (after) — z-order the compositor enforces as sole
	 * renderer. */
	{
		float x = menu->resolvedRect.x;
		float y = menu->resolvedRect.y;
		float w = menu->resolvedRect.w;
		float h = menu->resolvedRect.h;
		if ( w > 0 && h > 0 ) {
			Clay_Color bg = wui_clay_color_of( menu->backcolor,
			                                    wui_compositor_panel_alpha );
			void *compositeTag = wui_clay_composite_tag( WUI_COMPOSITE_INHERIT,
			                                             menu->compositeMode );
			CLAY({
				.layout = { .sizing = { CLAY_SIZING_FIXED( w ), CLAY_SIZING_FIXED( h ) } },
				.floating = {
					.attachTo = CLAY_ATTACH_TO_ROOT,
					.offset   = { x, y },
					.attachPoints = { CLAY_ATTACH_POINT_LEFT_TOP, CLAY_ATTACH_POINT_LEFT_TOP }
				},
				.backgroundColor = bg,
				.userData = compositeTag
			}) {}
		}
	}

	/* Optional menu background shader (drawn as a full-panel IMAGE behind
	 * everything). */
	if ( menu->background[0] ) {
		qhandle_t hShader = re.RegisterShader( menu->background );
		if ( hShader ) {
			float x = menu->resolvedRect.x;
			float y = menu->resolvedRect.y;
			float w = menu->resolvedRect.w;
			float h = menu->resolvedRect.h;
			if ( w > 0 && h > 0 ) {
				CLAY({
					.layout = { .sizing = { CLAY_SIZING_FIXED( w ), CLAY_SIZING_FIXED( h ) } },
					.floating = {
						.attachTo = CLAY_ATTACH_TO_ROOT,
						.offset   = { x, y },
						.attachPoints = { CLAY_ATTACH_POINT_LEFT_TOP, CLAY_ATTACH_POINT_LEFT_TOP }
					},
					.image = { .imageData = (void*)(uintptr_t) hShader },
					.backgroundColor = { 255, 255, 255, 255.0f * wui_compositor_panel_alpha }
				}) {}
			}
		}
	}

	/* Implicit full-screen flex root. Historically each top-level item was
	 * emitted with parentIsContainer=qfalse, which forced a `type container`
	 * menu root to carry `position absolute` (its only way to fill the screen,
	 * since with no flex parent Clay had nowhere to size it from). To let menu
	 * roots drop `position absolute` and lay out as native flex, wrap the
	 * top-level walk in a screen-sized flex container: static (non-floating)
	 * top-level items now flow inside it with parentIsContainer=qtrue and a
	 * no-size / `rect 0 0 1 1` root fills it. Items that are still floating —
	 * legacy absolute-rect leaves, HUD widgets pinned to screen coords, and any
	 * root still authored `position absolute` — are lifted out of this flex by
	 * Clay's floating semantics (a floating element attaches to root, not its
	 * lexical parent), so their placement is unchanged. Fully backward
	 * compatible: absolute roots keep working, and absolute-free roots now flex. */
	CLAY({
		.id = { .id = Clay_GetElementId( CLAY_STRING( "wui_menu_flex_root" ) ).id },
		.layout = {
			.sizing = { CLAY_SIZING_FIXED( menu->resolvedRect.w ),
			            CLAY_SIZING_FIXED( menu->resolvedRect.h ) },
			.layoutDirection = CLAY_TOP_TO_BOTTOM
		},
		.floating = {
			.attachTo     = CLAY_ATTACH_TO_ROOT,
			.offset       = { menu->resolvedRect.x, menu->resolvedRect.y },
			.attachPoints = { CLAY_ATTACH_POINT_LEFT_TOP, CLAY_ATTACH_POINT_LEFT_TOP },
			.pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH
		}
	}) {
		for ( i = 0; i < menu->itemCount; i++ ) {
			if ( menu->items[ i ] ) {
				/* parentIsContainer=qtrue: static top-level items flow as flex
				 * children of the screen root; floating items ignore it. */
				wui_clay_seed_panel_child( menu, menu->items[ i ] );
				wui_clay_emit_item( menu, menu->items[ i ], qtrue,
					WUI_LAYOUT_COLUMN, NULL );
			}
		}
	}
}

/* ── backend ───────────────────────────────────────────────── */

/* Push a scissor rect onto the stack, intersecting with the existing top
 * if any. Sets the active clip region through re.SetClipRegion. */
static void wui_clay_scissor_push( const Clay_BoundingBox *bb )
{
	wui_scissor_rect_t  r;
	float               region[ 4 ];

	r.x = bb->x;
	r.y = bb->y;
	r.w = bb->width;
	r.h = bb->height;

	if ( wui_scissor_depth >= WUI_COMPOSITOR_SCISSOR_DEPTH ) {
		if ( !wui_scissor_overflow_warned ) {
			Com_Log( SEV_WARN, LOG_CH(ch_ui),
				"WiredUI/Clay backend: scissor stack overflow at depth %d — dropping push\n",
				WUI_COMPOSITOR_SCISSOR_DEPTH );
			wui_scissor_overflow_warned = qtrue;
		}
		return;
	}

	if ( wui_scissor_depth > 0 ) {
		/* Intersect with current top. */
		wui_scissor_rect_t *top = &wui_scissor_stack[ wui_scissor_depth - 1 ];
		float x0 = MAX( r.x, top->x );
		float y0 = MAX( r.y, top->y );
		float x1 = MIN( r.x + r.w, top->x + top->w );
		float y1 = MIN( r.y + r.h, top->y + top->h );
		r.x = x0;
		r.y = y0;
		r.w = MAX( 0.0f, x1 - x0 );
		r.h = MAX( 0.0f, y1 - y0 );
	}

	wui_scissor_stack[ wui_scissor_depth++ ] = r;

	if ( !wui_compositor_emit_to_swapchain ) return;

	region[ 0 ] = r.x;
	region[ 1 ] = r.y;
	region[ 2 ] = r.w;
	region[ 3 ] = r.h;
	re.SetClipRegion( region );
}

/* Pop the top scissor; restore the new top (or NULL) via re.SetClipRegion. */
static void wui_clay_scissor_pop( void )
{
	float region[ 4 ];

	if ( wui_scissor_depth == 0 ) return;
	wui_scissor_depth--;

	if ( !wui_compositor_emit_to_swapchain ) return;

	if ( wui_scissor_depth == 0 ) {
		re.SetClipRegion( NULL );
		return;
	}

	region[ 0 ] = wui_scissor_stack[ wui_scissor_depth - 1 ].x;
	region[ 1 ] = wui_scissor_stack[ wui_scissor_depth - 1 ].y;
	region[ 2 ] = wui_scissor_stack[ wui_scissor_depth - 1 ].w;
	region[ 3 ] = wui_scissor_stack[ wui_scissor_depth - 1 ].h;
	re.SetClipRegion( region );
}

#ifdef _DEBUG
/* scripted Clay render-command-array → JSON dump for
 * structural diff against Chrome DOM extraction. Tree shape mirrors DOM
 * dump: SCISSOR_START / _END pairs delimit subtrees, every other command
 * is a leaf under the current scissor depth. _DEBUG-gated (matches the
 * wui_test_keydown gating used by the harness). */
static char     wui_clay_dump_path[ MAX_QPATH ];
static qboolean wui_clay_dump_pending;

void WiredUI_ClayDumpNext( const char *filename )
{
	if ( !filename || !*filename ) { wui_clay_dump_pending = qfalse; return; }
	Q_strncpyz( wui_clay_dump_path, filename, sizeof( wui_clay_dump_path ) );
	wui_clay_dump_pending = qtrue;
}

static void wui_clay_dump_indent( fileHandle_t f, int depth )
{
	int i; char pad[ 64 ];
	if ( depth > 31 ) depth = 31;
	for ( i = 0; i < depth; i++ ) { pad[ i*2 ] = ' '; pad[ i*2 + 1 ] = ' '; }
	pad[ depth * 2 ] = '\0';
	FS_Write( pad, depth * 2, f );
}

static const char *wui_clay_cmd_type_str( Clay_RenderCommandType t )
{
	switch ( t ) {
		case CLAY_RENDER_COMMAND_TYPE_RECTANGLE:     return "rect";
		case CLAY_RENDER_COMMAND_TYPE_BORDER:        return "border";
		case CLAY_RENDER_COMMAND_TYPE_TEXT:          return "text";
		case CLAY_RENDER_COMMAND_TYPE_IMAGE:         return "image";
		case CLAY_RENDER_COMMAND_TYPE_SCISSOR_START: return "container";
		case CLAY_RENDER_COMMAND_TYPE_SCISSOR_END:   return "_end";
		default:                                     return "custom";
	}
}

static void wui_clay_dump_color_hex( fileHandle_t f, Clay_Color c )
{
	char buf[ 32 ];
	Com_sprintf( buf, sizeof( buf ), "\"#%02x%02x%02x\"", (int)c.r & 0xff, (int)c.g & 0xff, (int)c.b & 0xff );
	FS_Write( buf, strlen( buf ), f );
}

static void wui_clay_dump_node_open( fileHandle_t f, Clay_RenderCommand *rc, int depth )
{
	char buf[ 256 ];
	Clay_BoundingBox *bb = &rc->boundingBox;
	wui_clay_dump_indent( f, depth );
	Com_sprintf( buf, sizeof( buf ),
		"{\"node_id\":\"%u\",\"type\":\"%s\",\"rect\":{\"x\":%.2f,\"y\":%.2f,\"w\":%.2f,\"h\":%.2f}",
		rc->id, wui_clay_cmd_type_str( rc->commandType ), bb->x, bb->y, bb->width, bb->height );
	FS_Write( buf, strlen( buf ), f );

	if ( rc->commandType == CLAY_RENDER_COMMAND_TYPE_RECTANGLE ) {
		FS_Write( ",\"style\":{\"bg\":", 15, f );
		wui_clay_dump_color_hex( f, rc->renderData.rectangle.backgroundColor );
		FS_Write( "}", 1, f );
	} else if ( rc->commandType == CLAY_RENDER_COMMAND_TYPE_BORDER ) {
		FS_Write( ",\"style\":{\"color\":", 18, f );
		wui_clay_dump_color_hex( f, rc->renderData.border.color );
		FS_Write( "}", 1, f );
	} else if ( rc->commandType == CLAY_RENDER_COMMAND_TYPE_TEXT ) {
		Clay_StringSlice s = rc->renderData.text.stringContents;
		size_t n = s.length < 200 ? (size_t)s.length : 200;
		size_t i;
		FS_Write( ",\"style\":{\"color\":", 18, f );
		wui_clay_dump_color_hex( f, rc->renderData.text.textColor );
		FS_Write( "},\"text\":\"", 10, f );
		for ( i = 0; i < n; i++ ) {
			char c = s.chars[ i ];
			if ( c == '"' || c == '\\' ) { FS_Write( "\\", 1, f ); FS_Write( &c, 1, f ); }
			else if ( c >= 0x20 ) { FS_Write( &c, 1, f ); }
		}
		FS_Write( "\"", 1, f );
	}
}

static void wui_clay_dump_render_commands( Clay_RenderCommandArray *cmds, const char *path )
{
	fileHandle_t f;
	int j, depth = 0;
	qboolean firstAtDepth[ 32 ];
	int i;
	for ( i = 0; i < 32; i++ ) firstAtDepth[ i ] = qtrue;

	f = FS_FOpenFileWrite( path );
	if ( !f ) return;

	FS_Write( "{\"node_id\":\"root\",\"type\":\"container\",\"rect\":{\"x\":0,\"y\":0,\"w\":0,\"h\":0},\"children\":[\n", 89, f );
	depth = 1;

	for ( j = 0; j < cmds->length; j++ ) {
		Clay_RenderCommand *rc = Clay_RenderCommandArray_Get( cmds, j );
		if ( !rc ) continue;

		if ( rc->commandType == CLAY_RENDER_COMMAND_TYPE_SCISSOR_END ) {
			FS_Write( "]}\n", 3, f );
			if ( depth > 0 ) depth--;
			continue;
		}

		if ( !firstAtDepth[ depth ] ) FS_Write( ",\n", 2, f );
		firstAtDepth[ depth ] = qfalse;

		wui_clay_dump_node_open( f, rc, depth );

		if ( rc->commandType == CLAY_RENDER_COMMAND_TYPE_SCISSOR_START ) {
			FS_Write( ",\"children\":[\n", 14, f );
			if ( depth < 31 ) depth++;
			firstAtDepth[ depth ] = qtrue;
		} else {
			FS_Write( "}", 1, f );
		}
	}

	while ( depth > 1 ) { FS_Write( "]}\n", 3, f ); depth--; }
	FS_Write( "\n]}\n", 4, f );
	FS_FCloseFile( f );
}
#endif

/* CSS-like paint transform for a Clay flex subtree. Layout and hit testing stay
 * axis aligned, while every renderer primitive emitted between the container's
 * background and border inherits one projective transform. This includes text
 * glyphs, images, bars, nested containers and custom HUD draws. amount is
 * normalized and signed: <0 recedes left, >0 recedes right. */
typedef struct {
	Clay_BoundingBox bounds;
	float            amount;
} wui_perspective_paint_t;

static const wiredItemDef_t *wui_applied_perspective_owner;

static void wui_clay_apply_perspective_paint( const wui_perspective_paint_t *paint ) {
	refUiTransform_t transform;
	if ( !wui_compositor_emit_to_swapchain || !re.SetUiTransform ) return;
	if ( !paint || fabsf( paint->amount ) < 0.001f ) {
		re.SetUiTransform( NULL );
		return;
	}
	memset( &transform, 0, sizeof( transform ) );
	transform.schemaVersion = REF_UI_TRANSFORM_SCHEMA_VERSION;
	transform.x = paint->bounds.x;
	transform.y = paint->bounds.y;
	transform.width = paint->bounds.width;
	transform.height = paint->bounds.height;
	transform.perspective = paint->amount;
	re.SetUiTransform( &transform );
}

static void wui_clay_apply_perspective_owner( const wiredItemDef_t *owner ) {
	wui_perspective_paint_t paint;
	if ( owner == wui_applied_perspective_owner ) return;
	wui_applied_perspective_owner = owner;
	if ( !owner || fabsf( owner->perspective ) < 0.001f ) {
		wui_clay_apply_perspective_paint( NULL );
		return;
	}
	paint.bounds.x = owner->resolvedRect.x;
	paint.bounds.y = owner->resolvedRect.y;
	paint.bounds.width = owner->resolvedRect.w;
	paint.bounds.height = owner->resolvedRect.h;
	paint.amount = owner->perspective;
	wui_clay_apply_perspective_paint( &paint );
}

/* Dispatch a single Clay render command. */
static void wui_clay_dispatch_command( Clay_RenderCommand *rc )
{
	Clay_BoundingBox *bb = &rc->boundingBox;
	vec4_t            color;
	const wiredItemDef_t *perspectiveOwner =
		wui_clay_command_perspective_owner( rc->userData );
	if ( rc->commandType == CLAY_RENDER_COMMAND_TYPE_CUSTOM ) {
		const wuiCustomDrawCommand_t *custom =
			(const wuiCustomDrawCommand_t *)rc->renderData.custom.customData;
		if ( custom ) perspectiveOwner = custom->perspectiveOwner;
	}
	wui_clay_apply_perspective_owner( perspectiveOwner );

	switch ( rc->commandType ) {
	case CLAY_RENDER_COMMAND_TYPE_RECTANGLE: {
		Clay_RectangleRenderData *rd = &rc->renderData.rectangle;
		wui_clay_color_to_vec4( &rd->backgroundColor, color, wui_compositor_panel_alpha );
		wui_compositor_rect_emitted++;
		if ( !wui_compositor_emit_to_swapchain ) break;
		if ( color[3] <= 0.0f ) break;
		if ( (uintptr_t) rc->userData & WUI_RECT_TAG_OVERLAY ) {
			/* Overlay fill: drawn after the gamma present pass in display/sRGB
			   space, so the rgb stays perceptual (no linearise) and alpha reads
			   light-independently. The renderer buffers these to a post-gamma
			   subpass on the swapchain image. */
			re.SetColor( color );
			re.DrawStretchPicOverlay( bb->x, bb->y, bb->width, bb->height,
			                          0.0f, 0.0f, 0.0f, 0.0f, cls.whiteShader );
			re.SetColor( NULL );
			break;
		}
		/* The renderer contract receives authored sRGB vertex colour. Each
		 * backend decodes it exactly once before blending in its linear target. */
		re.SetColor( color );
		re.DrawStretchPic( bb->x, bb->y, bb->width, bb->height,
		                   0.0f, 0.0f, 0.0f, 0.0f, cls.whiteShader );
		re.SetColor( NULL );
		break;
	}

	case CLAY_RENDER_COMMAND_TYPE_TEXT: {
		Clay_TextRenderData *td = &rc->renderData.text;
		msdfFont_t          *font;
		qboolean             wantShadow;

		wui_compositor_text_emitted++;

		if ( td->fontId >= MAX_WUI_FONTS ) break;
		font = wui_font_table[ td->fontId ];
		if ( !font || !font->loaded ) break;
		if ( !td->stringContents.chars || td->stringContents.length <= 0 ) break;

		wui_clay_color_to_vec4( &td->textColor, color, wui_compositor_panel_alpha );

		if ( !wui_compositor_emit_to_swapchain ) break;

		/* dropshadow plumbing. wui_clay_emit_item
		 * packed item->textstyle's "needs shadow" bit into the CLAY_TEXT_CONFIG's
		 * .userData; Clay forwarded it onto rc->userData on the TEXT render
		 * command (clay.h:689 + impl line 2862). Mirror legacy gating
		 * (cl_wired_text.c:131-146 + :174-180) — cvar gate, color-aware alpha,
		 * size-proportional offset. */
		/* DPI: td->fontSize is the logical point size; scale to
		 * physical pixels with the SAME factor wui_clay_measure_text used so
		 * the rendered glyphs fill the layout box Clay computed. The glyph
		 * positions (bb->x/y) are already in physical-pixel space (Clay laid
		 * out at the physical canvas size). dpiScale is 1.0 on non-HiDPI. */
		float pxFontSize = (float) td->fontSize * WiredUI_GetDpiScale();

		wantShadow = ( rc->userData != NULL ) && Text_ShadowEnabled();
		if ( wantShadow ) {
			vec4_t shadowColor = { 0.0f, 0.0f, 0.0f, color[3] * 0.8f };
			float  offset      = pxFontSize * 0.06f;
			if ( offset < 1.0f ) offset = 1.0f;
			re.SetMSDFShadow( offset, offset, shadowColor );
		}

		/* Byte-bounded draw (twin of the MSDF_MeasureStringBytes measure
		 * callback): td->stringContents is Clay's byte slice of the run, not
		 * necessarily NUL-terminated at the slice end, so bound on its byte
		 * length. Using the byte-bounded pair on both sides is what makes the
		 * laid-out width equal the rendered width for colour-coded text — the
		 * root fix that retired the per-run colour-split workaround. */
		MSDF_DrawStringBytes( font,
		                      bb->x, bb->y,
		                      pxFontSize,
		                      color,
		                      td->stringContents.chars,
		                      td->stringContents.length,
		                      (float) td->letterSpacing,
		                      qfalse );

		if ( wantShadow ) {
			re.SetMSDFShadow( 0.0f, 0.0f, NULL );
		}
		break;
	}

	case CLAY_RENDER_COMMAND_TYPE_IMAGE: {
		Clay_ImageRenderData *id = &rc->renderData.image;
		qhandle_t hShader = (qhandle_t)(uintptr_t) id->imageData;

		wui_compositor_image_emitted++;

		if ( !hShader ) break;
		wui_clay_color_to_vec4( &id->backgroundColor, color, wui_compositor_panel_alpha );

		if ( !wui_compositor_emit_to_swapchain ) break;
		re.SetColor( color );
		re.DrawStretchPic( bb->x, bb->y, bb->width, bb->height,
		                   0.0f, 0.0f, 1.0f, 1.0f, hShader );
		re.SetColor( NULL );
		break;
	}

	case CLAY_RENDER_COMMAND_TYPE_BORDER: {
		Clay_BorderRenderData *bd = &rc->renderData.border;
		float top    = bd->width.top;
		float right  = bd->width.right;
		float bottom = bd->width.bottom;
		float left   = bd->width.left;

		wui_compositor_border_emitted++;
		wui_clay_color_to_vec4( &bd->color, color, wui_compositor_panel_alpha );
		if ( color[3] > 0.0f && wui_compositor_emit_to_swapchain ) {
			re.SetColor( color );
		/* Up to 4 strip draws — one per non-zero edge. */
		if ( top > 0.0f ) {
			re.DrawStretchPic( bb->x, bb->y, bb->width, top,
			                   0,0,0,0, cls.whiteShader );
		}
		if ( bottom > 0.0f ) {
			re.DrawStretchPic( bb->x, bb->y + bb->height - bottom, bb->width, bottom,
			                   0,0,0,0, cls.whiteShader );
		}
		if ( left > 0.0f ) {
			re.DrawStretchPic( bb->x, bb->y, left, bb->height,
			                   0,0,0,0, cls.whiteShader );
		}
		if ( right > 0.0f ) {
			re.DrawStretchPic( bb->x + bb->width - right, bb->y, right, bb->height,
			                   0,0,0,0, cls.whiteShader );
		}
			re.SetColor( NULL );
		}
		break;
	}

	case CLAY_RENDER_COMMAND_TYPE_SCISSOR_START:
		wui_clay_scissor_push( bb );
		break;

	case CLAY_RENDER_COMMAND_TYPE_SCISSOR_END:
		wui_clay_scissor_pop();
		break;

	case CLAY_RENDER_COMMAND_TYPE_CUSTOM: {
		/* unified custom-draw dispatch. The per-frame
		 * wuiCustomDrawCommand_t was allocated from the scratch arena at
		 * emit time and threaded through Clay's customData channel.
		 * Look up the def by name, gate via ownerdrawFlag (preserves
		 * legacy ownerdraw visibility) + defaultVisibility (preserves
		 * hudElement SE_* visibility), then dispatch the appropriate
		 * union arm.
		 *
		 * This is the SOLE custom-draw emit path. The legacy
		 * SCR ownerdraw dispatch and WiredHud_RenderElements path have
		 * been retired. */
		wuiCustomDrawCommand_t   *cmd = (wuiCustomDrawCommand_t *) rc->renderData.custom.customData;
		const wuiCustomDrawDef_t *def;

		wui_compositor_custom_emitted++;
		if ( !cmd ) break;
		if ( !wui_compositor_emit_to_swapchain ) break;

		/* viewport itemDefs encode their id behind a "viewport:"
		 * sigil prefix at emit time. Route here before the custom-draw
		 * registry lookup; the post-':' tail is the registry key for the
		 * provider lookup that lives in elements/viewport.c. */
		if ( cmd->name[ 0 ] == 'v'
		  && cmd->name[ 1 ] == 'i'
		  && cmd->name[ 2 ] == 'e'
		  && cmd->name[ 3 ] == 'w'
		  && cmd->name[ 4 ] == 'p'
		  && cmd->name[ 5 ] == 'o'
		  && cmd->name[ 6 ] == 'r'
		  && cmd->name[ 7 ] == 't'
		  && cmd->name[ 8 ] == ':' )
		{
			WiredUI_RenderViewport( bb->x, bb->y, bb->width, bb->height,
				cmd->name + 9 );
			break;
		}

		/* scorelist_widget itemDefs encode their subtype behind a
		 * "scorelist:" sigil. Same dispatch shape as viewport: route to
		 * the dedicated element renderer before the custom-draw lookup. */
		if ( cmd->name[ 0 ] == 's'
		  && cmd->name[ 1 ] == 'c'
		  && cmd->name[ 2 ] == 'o'
		  && cmd->name[ 3 ] == 'r'
		  && cmd->name[ 4 ] == 'e'
		  && cmd->name[ 5 ] == 'l'
		  && cmd->name[ 6 ] == 'i'
		  && cmd->name[ 7 ] == 's'
		  && cmd->name[ 8 ] == 't'
		  && cmd->name[ 9 ] == ':' )
		{
			WiredUI_RenderScorelistWidget( bb->x, bb->y, bb->width, bb->height,
				cmd->color, cmd->name + 10 );
			break;
		}

		/* R1: the WiredUI SCENE procedural backdrop carries the bare "menubg:"
		 * sigil (no payload — the rect is bb). Gather the live continuous
		 * time / smoothed cursor / transition and hand them to
		 * re.DrawMenuBackdrop, which draws the menubg.frag fullscreen quad
		 * blended into this open 2D UI pass. Emitted backmost (zIndex -10), so
		 * every menu child dispatches after it and composites on top. */
		if ( cmd->name[ 0 ] == 'm'
		  && cmd->name[ 1 ] == 'e'
		  && cmd->name[ 2 ] == 'n'
		  && cmd->name[ 3 ] == 'u'
		  && cmd->name[ 4 ] == 'b'
		  && cmd->name[ 5 ] == 'g'
		  && cmd->name[ 6 ] == ':' )
		{
			float bgTime = 0.0f, bgMouseX = 0.0f, bgMouseY = 0.0f, bgTrans = 0.0f;
			/* Procedural menu backdrops are an optional renderer capability. A
			 * level restart may render the loading UI after a backend reload, and
			 * legacy/partial adapters legitimately leave this export NULL. Keep the
			 * UI command a no-op there instead of calling address zero. */
			if ( re.DrawMenuBackdrop ) {
				WUI_SceneBackdropParams( &bgTime, &bgMouseX, &bgMouseY, &bgTrans );
				re.DrawMenuBackdrop( bb->x, bb->y, bb->width, bb->height,
					bgTime, bgMouseX, bgMouseY, bgTrans );
			}
			break;
		}

		/* console_view itemDefs carry the bare "console:" sigil.
		 * Con_DrawConsole renders the full console UI against the swap-
		 * chain; the bb rect is informational only (the element panel's
		 * .wui sizing exists so the layer participates in policy + input
		 * claim, not so Con_DrawConsole respects it). */
		if ( cmd->name[ 0 ] == 'c'
		  && cmd->name[ 1 ] == 'o'
		  && cmd->name[ 2 ] == 'n'
		  && cmd->name[ 3 ] == 's'
		  && cmd->name[ 4 ] == 'o'
		  && cmd->name[ 5 ] == 'l'
		  && cmd->name[ 6 ] == 'e'
		  && cmd->name[ 7 ] == ':' )
		{
			Con_DrawConsole();
			break;
		}

		def = WiredUI_FindCustomDraw( cmd->name, NULL );
		if ( !def ) break;

		if ( cmd->ownerdrawFlag != 0 && !WiredUI_OwnerDrawVisible( cmd->ownerdrawFlag ) ) {
			break;
		}
		/* cg_draw2D is the master game-HUD visibility switch. Hide every
		   hud:<name> custom draw, including subtitles, while it is disabled.

		   Cinematic director: hide EVERY game-HUD element (hud:<name>) while a
		   scene is playing with its HUD flag off — including always-visible ones
		   (defaultVisibility == 0) that skip the SE predicate below, e.g. the
		   scoreboard bar. Only HUD custom-draws are gated; console / viewport /
		   menu custom-draws are untouched. Client-view-only; re-derived each
		   frame, so the HUD returns on scene-end.
		   EXCEPTION: hud:subtitle is a scene-presentation surface (the localized
		   cinematic caption), so it is NOT hidden by the HUD-off flag — a cutscene
		   hides the HUD but the subtitle stays visible. It gates itself on the
		   active-caption state instead. */
		if ( wiredHud && wiredHud_state_valid
		  && cmd->name[0] == 'h' && cmd->name[1] == 'u' && cmd->name[2] == 'd' && cmd->name[3] == ':' ) {
			if ( wiredHud->hud2DHidden ) {
				break;
			}
			if ( wiredHud->sceneHudHidden
			  && Q_stricmp( cmd->name, "hud:subtitle" ) != 0 ) {
				break;
			}
		}
		if ( def->defaultVisibility != 0 && !WiredHud_SE_Visible( def->defaultVisibility ) ) {
			break;
		}

		if ( def->isStateful ) {
			/* First resolved dispatch owns stateful creation.  Emit-time x/y/w/h
			 * are pre-layout estimates for native flex children; rc->boundingBox
			 * is Clay's authoritative physical-pixel box. */
			if ( !cmd->context && def->create && cmd->item ) {
				wuiCustomDrawConfig_t cfg;
				const char *colon = strchr( cmd->name, ':' );
				memset( &cfg, 0, sizeof( cfg ) );
				cfg.rect[ 0 ]      = bb->x;
				cfg.rect[ 1 ]      = bb->y;
				cfg.rect[ 2 ]      = bb->width;
				cfg.rect[ 3 ]      = bb->height;
				Vector4Copy( cmd->color, cfg.forecolor );
				cfg.ownerdrawFlag  = cmd->ownerdrawFlag;
				cfg.textstyle      = cmd->item->textstyle;
				cfg.familyIndex    = cmd->familyIndex;
				cfg.unprefixedName = colon ? colon + 1 : cmd->name;
				cfg.item           = cmd->item;
				cmd->context = def->create( &cfg );
				( (wiredItemDef_t *) cmd->item )->customDrawContext = cmd->context;
			}
			if ( def->routine.stateful && cmd->context ) {
				def->routine.stateful( cmd->context,
				                       bb->x, bb->y, bb->width, bb->height,
				                       cmd->color );
			}
		} else {
			if ( def->routine.stateless ) {
				def->routine.stateless( bb->x, bb->y, bb->width, bb->height,
				                        cmd->color );
			}
		}
		break;
	}

	case CLAY_RENDER_COMMAND_TYPE_NONE:
	default:
		break;
	}
}

/* WiredUI F4 (consistent backdrop dim): draw a full-viewport dim scrim behind a
 * modal dialog that does NOT already paint its own full-screen backdrop overlay.
 * The centered dialogs (confirm/callvote/error_popup) are authored `fullScreen 1`
 * with a `popup_overlay` container that fills the screen and carries the layered
 * vignette — those already dim the backdrop, so re-scrimming them would double-
 * darken. The small `fullScreen 0` popups (popup_message) have no backdrop of
 * their own; this gives them the same dimmed backing so every modal reads with a
 * consistent scrim. The backend performs the shared sRGB-to-linear decode. */
static void wui_clay_emit_modal_scrim( const wiredMenuDef_t *menu )
{
	vec4_t col;
	if ( !menu || !menu->modal || menu->fullscreen ) return;
	if ( !wui_compositor_emit_to_swapchain ) return;
	/* ~55% black — enough to separate the dialog from the surface behind it
	 * without hiding it, matching the vignette weight the fullscreen dialogs use. */
	col[0] = 0.0f;
	col[1] = 0.0f;
	col[2] = 0.0f;
	col[3] = 0.55f;
	re.SetColor( col );
	re.DrawStretchPic( 0.0f, 0.0f,
		(float) cls.glconfig.vidWidth, (float) cls.glconfig.vidHeight,
		0.0f, 0.0f, 0.0f, 0.0f, cls.whiteShader );
	re.SetColor( NULL );
}

/* ── per-frame entry (called from SCR_DrawScreenField) ─────────────── */

/* Per-frame walk:
 *   1. Build the visible_panels[] list (visible-only narrow) — only menus
 *      with .visible or .fullscreen enter further work this frame.
 *   2. Reset per-frame state (id-map, scissor, anon counter, counters).
 *   3. For each visible panel:
 *        a. Feed Clay the pointer state (cursor + mouse-down).
 *        b. Clay_BeginLayout → emit panel root + walk items (records
 *           into id-map alongside).
 *        c. Clay_EndLayout → capture Clay_Hovered() into panel state.
 *        d. Walk the render command array → dispatch through backend.
 */
/* Does the stack-top menu want a scrim over the game? Resolved with the rest
 * of the layer state and read by the MENU layer's emit. */
static qboolean wui_clay_menu_scrim = qfalse;
/* Per-frame latch so a multi-panel menu layer gets one scrim, not one per
 * panel — stacking them would darken by 1-(1-a)^n instead of a. */
static qboolean wui_clay_scrim_drawn = qfalse;
/* Per-frame latch for the dropdown sub-pass, so it dispatches once even when
 * several overlay-or-higher panels are visible. */
static qboolean wui_clay_dropdown_drawn = qfalse;

/* Per-frame layer-state resolve. Split out so the emit walk reads state rather
 * than deciding it, and so the background family's preset lookup has one home. */
static void wui_clay_resolve_layer_states( void )
{
	const wiredMenuDef_t *top = WiredUI_GetActiveMenu();
	wuiBgLayerState_t     bg;
	qboolean              isLoading;
	int                   L;

	/* A menu is "on top" for preset purposes whenever one is showing at all —
	 * including the implicit-root case where main is active without ever
	 * having been pushed. That path is precisely what the old slot model
	 * could not see. */
	/* The loading layer owns the whole connection lifecycle, including the
	 * CA_PRIMED hold used for the last loading frame. Keying only on CA_LOADING
	 * let the attract reel reappear above the world during CA_PRIMED while the
	 * loading panel was still active. Resolve the backdrop from the same policy
	 * as the layer itself so those two answers cannot diverge. */
	isLoading = loading_policy_isActive();

	WUI_BgPresetEval( top ? top->bgPreset : WUI_BG_PRESET_ANIMATED,
	                  top ? qtrue : qfalse, isLoading, &bg );

	/* Publish "a menu is up" for .wui gating. One writer, derived from the same
	 * stack top the presets read, so it cannot drift out of sync the way seven
	 * scattered onOpen writes did. The attract wordmark hides on it. */
	WiredUI_StateSetString( "ui_menuUp", top ? "1" : "0" );

	wui_clay_menu_scrim  = bg.menuScrim;
	wui_clay_scrim_drawn    = qfalse;
	wui_clay_dropdown_drawn = qfalse;
	WiredUI_LayerStateSet( WUI_LAYER_BG_DARK,     bg.darkVisible,     qfalse );
	WiredUI_LayerStateSet( WUI_LAYER_BG_ANIMATED, bg.animatedVisible, !bg.animatedVisible );
	WiredUI_LayerStateSet( WUI_LAYER_BG_ATTRACT,  bg.attractVisible,  bg.attractPaused );

	/* The rest keep their own predicates; recording the answer here gives
	 * every layer a uniform queryable state and a clock. */
	for ( L = 0; L < WUI_LAYER_COUNT; L++ ) {
		if ( L == WUI_LAYER_BG_DARK || L == WUI_LAYER_BG_ANIMATED
		  || L == WUI_LAYER_BG_ATTRACT ) continue;
		WiredUI_LayerStateSet( (wuiLayer_t) L, wui_layer_active( (wuiLayer_t) L ), qfalse );
	}
}

/* Transient multi-dropdown sub-pass. Dispatched from inside the panel walk,
 * immediately BEFORE the overlay layer, so the popup covers every panel
 * beneath it while the cursor sprite — which lives on the overlay — still
 * draws on top. Running it after the whole walk put it above the overlay
 * too, and the cursor vanished behind any open dropdown. */
static void wui_clay_dispatch_multidropdown( void )
	{
		Clay_RenderCommandArray ddCmds;
		int                     j;
		Clay_Vector2            ddPointer;

		if ( wui_compositor_pointer_x >= 0.0f && wui_compositor_pointer_y >= 0.0f ) {
			ddPointer.x = wui_compositor_pointer_x;
			ddPointer.y = wui_compositor_pointer_y;
			Clay_SetPointerState( ddPointer, wui_compositor_mouse_down );
		}

		s_wui_emit_menu  = "multidropdown";
		s_wui_emit_layer = (int) WUI_LAYER_OVERLAY;
		s_wui_emit_item  = NULL;
		wui_compositor_panel_alpha = 1.0f;

		Clay_BeginLayout();
		wui_clay_emit_multidropdown();
		ddCmds = Clay_EndLayout();

		wui_scissor_depth = 0;
		for ( j = 0; j < ddCmds.length; j++ ) {
			Clay_RenderCommand *rc = Clay_RenderCommandArray_Get( &ddCmds, j );
			if ( !rc ) continue;
			wui_clay_dispatch_command( rc );
		}
		if ( wui_scissor_depth > 0 && wui_compositor_emit_to_swapchain ) {
			re.SetClipRegion( NULL );
			wui_scissor_depth = 0;
		}
	}


void WiredUI_CompositorEmitFrame( void )
{
	int                       i;
	Clay_RenderCommandArray   cmds;
	const wiredMenuDef_t     *menu;

	if ( !wui_clay_initialized ) return;
	wui_applied_perspective_owner = NULL;
	if ( wui_compositor_emit_to_swapchain && re.SetUiTransform )
		re.SetUiTransform( NULL );

	/* Resolve every layer's visible/paused state for this frame before any
	 * emit reads it. The background family answers to the stack-top menu's
	 * declared preset; everything else keeps its own connection-state
	 * predicate. Asking here, once, is what replaces the old write-on-push
	 * intent slot — there is no transition to miss because there is no
	 * transition, only a fresh answer each frame. */
	wui_clay_resolve_layer_states();

	/* (1) Build the visible-panels list. This replaces the
	 * single-active-menu hook with a 6-layer walk (back-to-front z-order):
	 * BG_ATTRACT → LOADING → HUD → MENU_STACK → POPUP → DEBUG_OVERLAY.
	 *
	 * For each layer L:
	 *   - Check wui_layer_active(L) against cls.state / stack / popup / cvar.
	 *   - If active, append the layer's menus to wui_visible_panels in order.
	 *
	 * Layer iteration kinds (matches the directive's split between
	 * "many panels per layer" vs "single active panel"):
	 *
	 *   MENU_STACK : single panel — WiredUI_GetActiveMenu() (stack top OR
	 *                root main/ingame) so existing flows route through here.
	 *   POPUP      : single panel — front-of-queue popup name resolved via
	 *                WiredUI_FindMenu.
	 *   all others : multi-panel — walk every defined menu, emit those
	 *                tagged with .layer == L AND .visible.
	 *
	 * The visible-only narrow still holds: .visible defaults true
	 * at parse time and stays as the "definition enabled" gate. Per-panel
	 * persistent state (hoveredId, focusedId) is held in wui_panel_states
	 * across frames so it resumes when the panel re-enters the list. */
	wui_visible_panel_count = 0;
	{
		int            layerIdx;
		int            menuCount = WiredUI_GetMenuCount();
		int            menuIdx;
		const char    *popupName;
		wiredMenuDef_t *m;

		for ( layerIdx = 0; layerIdx < WUI_LAYER_COUNT; layerIdx++ ) {
			wuiLayer_t L = (wuiLayer_t) layerIdx;
			/* Read the state resolved at the top of the frame rather than
			 * re-asking the predicate: the background family's answer comes
			 * from the stack-top preset, which wui_layer_active() cannot see. */
			if ( !WiredUI_LayerVisible( L ) ) continue;

			/* The two background layers own their content directly instead of
			 * riding on whichever menu item happened to carry bgLayerFlags.
			 * That indirection is what let two different emitters — the
			 * layered scene rects and the procedural backdrop — end up on the
			 * same zIndex competing on emit order; as layers they simply
			 * stack. */

			if ( L == WUI_LAYER_MENU ) {
				m = WiredUI_GetActiveMenu();
				if ( m && m->visible
				  && wui_visible_panel_count < WIRED_MAX_MENUS ) {
					wui_visible_panels[ wui_visible_panel_count++ ] = m;
				}
				continue;
			}

			if ( L == WUI_LAYER_POPUP ) {
				popupName = WiredUI_FrontPopupName();
				if ( popupName && *popupName ) {
					m = WiredUI_FindMenu( popupName );
					if ( m && m->visible
					  && wui_visible_panel_count < WIRED_MAX_MENUS ) {
						wui_visible_panels[ wui_visible_panel_count++ ] = m;
					}
				}
				continue;
			}

			/* LOADING: state→named-UI binding (the React `return
			 * <LoadingComponent/>` model). The connstate transition set the
			 * relative path of the menu to show (connect.wui / loading_screen.wui);
			 * resolve it by path-identity instead of blind-scanning the registry
			 * for anything tagged `layer "loading"`. A blind scan comes up empty
			 * SILENTLY when the menu was dropped from the registry (e.g. by
			 * WiredUI_SafeReload) → black screen; a by-identity lookup fails LOUD
			 * with a warning instead. The activation predicate
			 * (loading_policy_isActive, checked by wui_layer_active above) is the
			 * `props.loading` half; this is the missing `which UI` half. */
			if ( L == WUI_LAYER_LOADING ) {
				const char *path = WiredUI_GetLoadingMenuPath();
				m = ( path && *path ) ? WiredUI_FindMenuByPath( path ) : NULL;
				if ( m && m->visible
				  && wui_visible_panel_count < WIRED_MAX_MENUS ) {
					wui_visible_panels[ wui_visible_panel_count++ ] = m;
				} else if ( path && *path && !m ) {
					/* fail-loud: the loading UI was named but is not in the
					 * registry — the drop-bug surfacing as a diagnosable error
					 * instead of a silent black screen. */
					COM_WARN( LOG_CH(ch_ui),
						"WiredUI: loading menu '%s' set but not in registry "
						"(state=%d) — loading screen will be blank\n",
						path, (int)clientActiveApp->state );
				}
				continue;   /* no blind-scan for LOADING */
			}

			/* Multi-panel layers: walk the full menu registry, filter by
			 * .layer == L AND .visible. Definition order is preserved
			 * inside a layer (parse-order = registry-order). */
			for ( menuIdx = 0; menuIdx < menuCount; menuIdx++ ) {
				m = WiredUI_GetMenuByIndex( menuIdx );
				if ( !m ) continue;
				if ( m->layer != L ) continue;
				if ( !m->visible ) continue;
				if ( wui_visible_panel_count >= WIRED_MAX_MENUS ) break;
				wui_visible_panels[ wui_visible_panel_count++ ] = m;
			}
		}
	}

	/* (2) Per-frame state reset. Counters + id-map rebuilt every frame.
	 * Scratch arena bump-pointer rewind for repeat expansion. */
	wui_compositor_rect_emitted   = 0;
	wui_compositor_text_emitted   = 0;
	wui_compositor_image_emitted  = 0;
	wui_compositor_border_emitted = 0;
	wui_compositor_custom_emitted = 0;
	wui_scissor_depth             = 0;
	wui_id_map_count              = 0;
	wui_emit_anon_counter         = 0;
	if ( wui_clay_scratch_arena ) {
		Arena_Reset( wui_clay_scratch_arena );
	}

	/* (2a) tick the animation pool BEFORE per-panel
	 * emission so store-key writebacks land this frame — bindings on the
	 * target key refresh via the existing store-dirty pattern when the
	 * panel walk below resolves them. */
	Anim_FrameUpdate( Sys_Milliseconds() );

	/* (2b) pure-pull world render. The engine-side
	 * direct CL_CGameRendering(STEREO_CENTER) call is RETIRED. The world
	 * scene now renders through the cgame provider callback
	 * (CG_RenderScene_cb), invoked by the per-panel walk below when it
	 * reaches the WORLD_VIEWPORT panel (world_main.wui)'s `type viewport
	 * id "main_scene"` itemDef → WiredUI_RenderViewport → prov->render().
	 * The callback pulls per-frame context via trap_GetSceneFrameContext
	 * (slot 224). WORLD_VIEWPORT (layer 2) walks before HUD/MENU/CONSOLE,
	 * so the world still draws at the bottom of painter's order.
	 *
	 * The cinematic blit stays here: it is NOT a WORLD_VIEWPORT/viewport-
	 * itemDef path (no provider, no panel), but the dispatch unification
	 * still has to drive it before any UI overlay to preserve the v2
	 * "single dispatch authority" goal. */
	if ( clientActiveApp->state == CA_CINEMATIC ) {
		SCR_DrawCinematic();
	}

	/* Re-sync the Clay layout dimensions to the CURRENT render size right before
	 * the panel walk lays anything out. WiredUI_CompositorFrame ticks this earlier
	 * in CL_Frame, but an early/transitional render (notably the connect dialog,
	 * which draws before the compositor tick has caught up to the post-supersample
	 * cls.glconfig.vidWidth) could otherwise lay out against the stale init-seeded
	 * window width — landing the panel in a fraction of the larger ORTHO (the
	 * connecting-screen top-left-quarter bug). WiredUI_ClayFrame is a no-op when
	 * the width is already current (its !=-gate), so this never perturbs the 1:1 /
	 * non-supersample case (1280 == 1280). Must run BEFORE Clay_BeginLayout below —
	 * Clay_SetLayoutDimensions inside an open layout bracket would not re-resolve. */
	WiredUI_ClayFrame( cls.glconfig.vidWidth, cls.glconfig.vidHeight );

	/* (2c) Background layers, in a layout pass of their own, before any panel.
	 *
	 * These cannot ride along inside a panel's layout. Each panel opens its own
	 * Clay_BeginLayout/EndLayout and dispatches that list immediately, so a rect
	 * emitted into panel N's list is drawn in panel N's turn and nowhere else.
	 * Attaching the backdrop to the first panel therefore tied it to whichever
	 * panel happened to be first — always `attract_brand` — with two
	 * consequences: a menu emitted in a later pass (preferences, any submenu)
	 * got no backdrop at all, and when a preset hid attract the backdrop
	 * vanished with the panel that was carrying it. That is exactly the
	 * "options has no animated background" report.
	 *
	 * Its own pass has no such coupling: it is dispatched first, so every panel
	 * lands on top of it regardless of which panels exist this frame.
	 *
	 * Sized from cls.glconfig, the same pair fed to WiredUI_ClayFrame just
	 * above — Clay's coordinate space here is the physical framebuffer, so with
	 * r_ext_supersample this is 2560x1440 and not the 1280x720 of the
	 * screenshot. */
	if ( WiredUI_LayerVisible( WUI_LAYER_BG_DARK )
	  || WiredUI_LayerVisible( WUI_LAYER_BG_ANIMATED )
	  || wui_clay_menu_scrim ) {
		float                    bw = (float) cls.glconfig.vidWidth;
		float                    bh = (float) cls.glconfig.vidHeight;
		Clay_RenderCommandArray  bgCmds;
		int                      j;

		Clay_BeginLayout();
		if ( WiredUI_LayerVisible( WUI_LAYER_BG_DARK ) )
			WUI_DrawBackgroundLayered( 0.0f, 0.0f, bw, bh, WUI_BG_LAYER_BASE );
		if ( WiredUI_LayerVisible( WUI_LAYER_BG_ANIMATED ) )
			/* The ANIMATED layer is the procedural SCENE pass (menubg.frag), NOT
			 * the WUI_BG_DEMO_BACKDROP rect stack. That rect stack was retired
			 * when the scene became one shader (see WUI_DrawBackgroundScene) —
			 * but when the background layers were given their own content the
			 * retired constant was wired in here instead of the live emitter, so
			 * every `backdrop animated` menu got ~190 near-black Clay rects and
			 * no animation at all. The rect stack has no clock: sky/fog/floor are
			 * static bands, and only the 14 embers move, at alpha ~4/255 over a
			 * #0a0403 underpaint — which is why the composed backdrop measured as
			 * an empty black screen while the shader that was supposed to draw it
			 * had zero callers. */
			WUI_DrawBackgroundScene( 0.0f, 0.0f, bw, bh );
		bgCmds = Clay_EndLayout();

		wui_scissor_depth = 0;
		for ( j = 0; j < bgCmds.length; j++ ) {
			Clay_RenderCommand *rc = Clay_RenderCommandArray_Get( &bgCmds, j );
			if ( rc ) wui_clay_dispatch_command( rc );
		}
	}

	/* (3) Per-panel work. */
	for ( i = 0; i < wui_visible_panel_count; i++ ) {
		Clay_Vector2          pointer;
		wui_panel_state_t    *pstate;
		int                   j;

		menu = wui_visible_panels[ i ];

		/* The scrim darkens whatever is BEHIND the menu, so it has to be
		 * dispatched between that content and the menu itself — not with the
		 * background layers. Those are emitted in their own pass before the
		 * panel walk, and attract is a panel, so a scrim drawn back there
		 * lands underneath the reel and darkens nothing: the main menu read
		 * as its text tangled with a full-brightness attract poster. Emitting
		 * it here, once, immediately before the first panel at or above the
		 * menu layer, puts it over attract and under the menu. */
		if ( wui_clay_menu_scrim && !wui_clay_scrim_drawn
		  && menu && (int) menu->layer >= (int) WUI_LAYER_MENU ) {
			Clay_RenderCommandArray scrimCmds;
			int                     k;

			wui_clay_scrim_drawn = qtrue;
			Clay_BeginLayout();
			WUI_DrawBackgroundDim( 0.0f, 0.0f,
				(float) cls.glconfig.vidWidth, (float) cls.glconfig.vidHeight );
			scrimCmds = Clay_EndLayout();

			wui_scissor_depth = 0;
			for ( k = 0; k < scrimCmds.length; k++ ) {
				Clay_RenderCommand *rc = Clay_RenderCommandArray_Get( &scrimCmds, k );
				if ( rc ) wui_clay_dispatch_command( rc );
			}
		}

		/* source-attribution: surface the menu currently being
		 * emitted so wui_clay_error_handler can blame Clay layout errors
		 * on it. Item-level context updates inside wui_clay_emit_item. */
		s_wui_emit_menu  = menu ? menu->name : NULL;
		s_wui_emit_layer = menu ? (int) menu->layer : -1;
		s_wui_emit_item  = NULL;

		/* Panel alpha — always 1.0 for now; a later change reads from the
		 * per-menu fade state machine here. */
		wui_compositor_panel_alpha = 1.0f;

		/* (3a) Pointer state — feeds the global cursor; per-panel
		 * localPointer differs only if the panel has a non-identity
		 * transform (a later modality scope). Negative coords mean "no
		 * pointer feed yet" (boot before first mouse motion). */
		if ( wui_compositor_pointer_x >= 0.0f && wui_compositor_pointer_y >= 0.0f ) {
			pointer.x = wui_compositor_pointer_x;
			pointer.y = wui_compositor_pointer_y;
			Clay_SetPointerState( pointer, wui_compositor_mouse_down );
		}

		/* (3a') Advance scroll containers ONCE per frame (first visible panel
		 * only). Clay_UpdateScrollContainers is global — it flips every scroll
		 * container's openThisFrame flag to false and EVICTS any container whose
		 * flag is already false. Calling it per-panel is fatal for scroll: a
		 * later panel's call clears the menu panel's container flag, and next
		 * frame's first call then evicts the container (losing its scroll
		 * position) before the menu panel re-registers it. Gating to i==0 keeps
		 * a single, layout-consistent update per frame.
		 *
		 * A zero delta here just initialises + clamps; the real wheel delta is
		 * applied by direct mutation below (Clay's pointer-over matching can't
		 * see a container that isn't in the last-laid-out panel). */
		if ( i == 0 ) {
			Clay_Vector2 zeroDelta = { 0.0f, 0.0f };
			Clay_UpdateScrollContainers( qfalse, zeroDelta, 0.016f );

			/* Apply the accumulated wheel delta by DIRECTLY mutating the
			 * container's scroll position via the pointer Clay exposes for this
			 * purpose (Clay_ScrollContainerData.scrollPosition — "Intended for
			 * use with external functionality that modifies scroll position,
			 * such as scroll bars"). We hit-test the cursor against the cached
			 * viewport rect ourselves and clamp exactly like Clay's wheel path
			 * (delta*10, [-(overflow),0]). Read this frame by the childOffset
			 * emit (wui_clay_scroll_offset_for). */
			if ( wui_clay_pendingWheelDeltaY != 0.0f
			  && wui_compositor_pointer_x >= 0.0f && wui_compositor_pointer_y >= 0.0f ) {
				int s;
				for ( s = 0; s < wui_scroll_rect_count; s++ ) {
					wui_scroll_entry_t      *r = &wui_scroll_rects[ s ];
					Clay_ElementId           eid;
					Clay_ScrollContainerData scd;
					float                    maxScroll;
					if ( !r->overflow ) continue;
					if ( !( wui_compositor_pointer_x >= r->x && wui_compositor_pointer_x <= r->x + r->w
					     && wui_compositor_pointer_y >= r->y && wui_compositor_pointer_y <= r->y + r->h ) )
						continue;
					eid.id = r->clayId;
					scd = Clay_GetScrollContainerData( eid );
					if ( !scd.found || !scd.scrollPosition ) continue;
					maxScroll = r->contentH - r->viewH;
					if ( maxScroll < 0.0f ) maxScroll = 0.0f;
					scd.scrollPosition->y += wui_clay_pendingWheelDeltaY * 10.0f;
					if ( scd.scrollPosition->y > 0.0f )         scd.scrollPosition->y = 0.0f;
					if ( scd.scrollPosition->y < -maxScroll )   scd.scrollPosition->y = -maxScroll;
					/* Mirror into the cache so this frame's childOffset emit reads
					 * the new position (Clay_GetScrollContainerData is unreadable
					 * mid-emit). */
					r->scrollY = scd.scrollPosition->y;
					break;
				}
				wui_clay_pendingWheelDeltaY = 0.0f;
			}

			/* Apply a pending ABSOLUTE scroll target (from scrollbar drag or
			 * PageUp/Down) to the addressed container. Same in-frame window +
			 * clamp as the wheel path; keyed by clayId (drag/keys already know
			 * their container, no cursor hit-test needed). */
			if ( wui_clay_pendingScrollId != 0 ) {
				int s;
				for ( s = 0; s < wui_scroll_rect_count; s++ ) {
					wui_scroll_entry_t      *r = &wui_scroll_rects[ s ];
					Clay_ElementId           eid;
					Clay_ScrollContainerData scd;
					float                    maxScroll, target;
					if ( r->clayId != wui_clay_pendingScrollId ) continue;
					eid.id = r->clayId;
					scd = Clay_GetScrollContainerData( eid );
					if ( !scd.found || !scd.scrollPosition ) break;
					maxScroll = r->contentH - r->viewH;
					if ( maxScroll < 0.0f ) maxScroll = 0.0f;
					target = wui_clay_pendingScrollY;
					if ( target > 0.0f )         target = 0.0f;
					if ( target < -maxScroll )   target = -maxScroll;
					scd.scrollPosition->y = target;
					r->scrollY            = target;
					break;
				}
				wui_clay_pendingScrollId = 0;
			}
		}

		/* WiredUI F4: dim scrim behind a modal popup that lacks its own
		 * full-screen backdrop, drawn BEFORE this panel's render commands so it
		 * sits under the dialog. No-op for non-modal / fullscreen-overlay menus. */
		wui_clay_emit_modal_scrim( menu );

		/* An open dropdown belongs above every panel EXCEPT the overlay, which
		 * carries the cursor sprite. Dispatch it once, just before the first
		 * overlay-or-higher panel, so it covers everything beneath while the
		 * cursor still lands on top.
		 *
		 * It has to happen BEFORE this panel opens its layout. The sub-pass
		 * runs its own Clay_BeginLayout/EndLayout, and starting a new layout
		 * invalidates the command array from the previous one — calling it
		 * between the panel's EndLayout and its dispatch left the overlay
		 * holding a dead `cmds` and the cursor stopped drawing entirely. */
		if ( !wui_clay_dropdown_drawn && menu
		  && (int) menu->layer >= (int) WUI_LAYER_OVERLAY ) {
			wui_clay_dropdown_drawn = qtrue;
			wui_clay_dispatch_multidropdown();
		}

		/* (3b) Emit panel + record (id, item, panel) tuples. */
		Clay_BeginLayout();

			wui_clay_emit_panel( menu );
			cmds = Clay_EndLayout();
		wui_clay_sync_panel_rects( menu );
		WUI_DumpLayout( menu );

		/* (3b') Snapshot scroll-container extents now that EndLayout computed
		 * contentSize; next frame's scrollbar emit + wheel handler read this. */
		wui_clay_refresh_scroll_cache();

		/* (3b'') Re-derive the pointer-over set against THIS panel's just-completed
		 * tree. Clay_SetPointerState populates pointerOverIds by walking the CURRENT
		 * layoutElementTreeRoots (from the most recent EndLayout) — but the pre-
		 * BeginLayout SetPointerState above ran while those roots still held the
		 * PREVIOUS panel's tree, so its pointerOverIds describe the previous panel,
		 * not this one. In a single-panel-per-frame Clay app that is the intended
		 * one-frame lag; in this multi-panel-per-frame walk it cross-contaminates
		 * every panel's hover with its predecessor's geometry (e.g. in-game the
		 * ESC menu panel resolved against the HUD panel emitted just before it, so
		 * no menu button ever registered as hovered while the main menu — which has
		 * no panel between it and the overlay — worked by adjacency). Calling
		 * SetPointerState again HERE, after this panel's EndLayout, recomputes
		 * pointerOverIds against this panel's own tree so Clay_GetPointerOverIds
		 * below is panel-correct. Pure read of the freshly-laid-out tree + hashmap
		 * boxes; no layout mutation. Same pointer-valid gate as the pre-layout call. */
		if ( wui_compositor_pointer_x >= 0.0f && wui_compositor_pointer_y >= 0.0f ) {
			Clay_Vector2 p2 = { wui_compositor_pointer_x, wui_compositor_pointer_y };
			Clay_SetPointerState( p2, wui_compositor_mouse_down );
		}

		/* (3c) Capture the hovered element into the per-panel persistent state.
		 * The real hover result is computed below via Clay_GetPointerOverIds()
		 * (Clay auto-generates IDs for CLAY_TEXT children + emit-internal
		 * wrappers, so the topmost pointer-over ID needs filtering). */
		pstate = wui_panel_state_for( menu );
		if ( pstate ) {
			/* The Clay v0.14 API exposes Clay_GetPointerOverIds() which
			 * returns an array of element IDs the pointer is over. Clay
			 * auto-generates IDs for CLAY_TEXT children + emit-internal
			 * wrappers, so the topmost is often an unregistered child
			 * node. Walk back-to-front (innermost first) preferring the
			 * first ID whose wired item carries an `action[]` — this
			 * surfaces action-bearing containers when the pointer is
			 * over their decorative nested leaves (num + label + sub +
			 * hot + arrow composition). Falls back to the first ID
			 * with any wui_id_map entry if no action-bearing item is
			 * in the over-stack. */
			Clay_ElementIdArray over = Clay_GetPointerOverIds();
			uint32_t resolvedAction = 0;
			uint32_t resolvedAny    = 0;
			int k;
			for ( k = (int) over.length - 1; k >= 0; k-- ) {
				uint32_t cand = over.internalArray[ k ].id;
				const wiredItemDef_t *it = NULL;
				const wiredMenuDef_t *pn = NULL;
				if ( wui_id_map_lookup( cand, &it, &pn ) ) {
					/* A `decoration` item is authored passthrough (its emit sets
					 * pointerCaptureMode = PASSTHROUGH). Clay still lists it in the
					 * pointer-over set, but it must never be REPORTED as hovered —
					 * otherwise the always-on overlay layer (cursor + tooltip stubs,
					 * both decoration) and full-rect dim backdrops win the panel's
					 * hoveredId and shadow the real button underneath. Skip it so a
					 * passthrough leaf is transparent to hover, matching its capture
					 * mode. */
					if ( it && it->decoration ) continue;
					if ( resolvedAny == 0 ) resolvedAny = cand;
					if ( it && it->action[0] ) {
						resolvedAction = cand;
						break;
					}
				}
			}
			pstate->hoveredId = resolvedAction ? resolvedAction : resolvedAny;
		}

		/* (3d) Dispatch render commands. */
		wui_scissor_depth = 0;
		for ( j = 0; j < cmds.length; j++ ) {
			Clay_RenderCommand *rc = Clay_RenderCommandArray_Get( &cmds, j );
			if ( !rc ) continue;
			wui_clay_dispatch_command( rc );
		}

#ifdef _DEBUG
		if ( wui_clay_dump_pending ) {
			wui_clay_dump_render_commands( &cmds, wui_clay_dump_path );
			wui_clay_dump_pending = qfalse;
		}
#endif

		/* Defensive: clear any unmatched scissor. */
		if ( wui_scissor_depth > 0 && wui_compositor_emit_to_swapchain ) {
			re.SetClipRegion( NULL );
			wui_scissor_depth = 0;
		}
		/* A malformed/no-border authored perspective container must not leak
		 * its paint transform into the next independently laid-out panel. */
		if ( wui_compositor_emit_to_swapchain && re.SetUiTransform )
			re.SetUiTransform( NULL );
		wui_applied_perspective_owner = NULL;
	}

	/* (4) OVERLAY layer sub-pass split.
	 *
	 * The WUI_LAYER_OVERLAY emit is conceptually three sub-passes in
	 * painter's order — cursor (lowest z), tooltip (mid), transient
	 * (highest z, including multi-dropdown and future floating modals).
	 * The cursor + tooltip pieces are emitted by overlay.wui as standard
	 * panel children (Path A custom-draw items); the transient sub-pass
	 * fires below as its own Clay pass so a floating popover renders
	 * above the cursor sprite.
	 *
	 * Physical refactor into 3 explicit sub-pass functions is deferred to
	 * a follow-up; the comment block here documents the
	 * sub-pass z-ordering contract that the future split must preserve.
	 *
	 * Transient multi-dropdown sub-pass. It normally dispatches inside the
	 * panel walk, just before the overlay layer, so the popup sits above every
	 * panel but under the cursor. This tail call is ONLY the fallback for a
	 * frame with no overlay-or-higher panel.
	 *
	 * The latch matters: without it this ran a SECOND time after the walk, on
	 * top of everything — including the console — so the popup was always the
	 * last thing painted and the cursor stayed buried no matter where the
	 * in-loop call was moved. Two dispatches of the same popup also meant the
	 * work was done twice. */
	if ( !wui_clay_dropdown_drawn ) {
		wui_clay_dropdown_drawn = qtrue;
		wui_clay_dispatch_multidropdown();
	}
#ifdef WIRED_WEB_UI_NATIVE
	if ( wui_visible_panel_count > 0 ) WiredWebAuthored_MarkMenuRendered();
#endif
}

/* ── public hit-test entry points (called from WiredUI_*Event hooks) ── */

/* Forward the cursor position from WiredUI_MouseEvent (post-clamp). */
void WiredUI_CompositorPointerMoved( float x, float y )
{
	wui_compositor_pointer_x = x;
	wui_compositor_pointer_y = y;
}

/* Track mouse-button transitions. On down-edge: capture the currently
 * hovered Clay ID across all visible panels as wui_down_target. On up-
 * edge: if cursor still hovers wui_down_target, resolve to wiredItemDef_t
 * via id-map and (gated by wui_compositor_input_active) fire its action.
 * Returns qtrue iff the compositor consumed the event — for now this always
 * returns qfalse so the legacy WiredUI_KeyEvent path remains authoritative. */
qboolean WiredUI_CompositorMouseButton( qboolean down )
{
	uint32_t hovered = 0;
	int      i;

	/* Aggregate "any visible panel reports hovered" — the topmost wins
	 * on overlap. The single-layer model means whichever visible
	 * panel was emitted last has authority. */
	for ( i = 0; i < wui_visible_panel_count; i++ ) {
		const wui_panel_state_t *ps = wui_panel_state_for( wui_visible_panels[ i ] );
		if ( ps && ps->hoveredId ) hovered = ps->hoveredId;
	}

	if ( down ) {
		wui_compositor_mouse_down  = qtrue;
		wui_compositor_down_target = hovered;
		return qfalse;
	} else {
		const wiredItemDef_t *item  = NULL;
		const wiredMenuDef_t *panel = NULL;
		qboolean              fired = qfalse;

		if ( wui_compositor_down_target != 0
		  && wui_compositor_down_target == hovered
		  && wui_id_map_lookup( wui_compositor_down_target, &item, &panel ) ) {
			if ( wui_compositor_input_active && item && item->action[0] ) {
				/* Gated path — gate OFF, so legacy WiredUI_KeyEvent
				 * remains the sole action source. The action-fire entry
				 * point (WiredUI_RunScript) is static to cl_wired_ui.c
				 * today; promoting it to a public wrapper lands together
				 * with the gate flip (legacy retirement).
				 * With the gate OFF by default, the static linkage
				 * keeps the code from accidentally firing the action twice. */
				fired = qtrue;
			}
		}
		wui_compositor_mouse_down  = qfalse;
		wui_compositor_down_target = 0;
		return fired;
	}
}

/* Forward a mouse wheel event to Clay's scroll containers. Returns qtrue
 * iff Clay consumed the scroll (i.e. cursor over a scroll container);
 * caller (WiredUI_KeyEvent) should then suppress the virtual K_MWHEEL*
 * key fall-through. Simplified for now — calls Clay_UpdateScrollContainers
 * unconditionally and reports whether any panel is currently being scrolled
 * by inspecting the over-IDs array. Full container-found detection is
 * later work once scroll containers are exercised by real .wmenu content. */
const wiredItemDef_t *WiredUI_CompositorHoveredItem( void );   /* defined below */

qboolean WiredUI_CompositorMouseWheel( float deltaY )
{
	int      i;
	qboolean overScroll = qfalse;

	if ( !wui_clay_initialized ) return qfalse;
	if ( wui_visible_panel_count == 0 ) return qfalse;

	/* Wheel over a slider adjusts its value instead of scrolling — handled HERE
	 * (not in the K_MWHEEL switch) so it wins even when the slider sits inside an
	 * overflowing scroll viewport: the early CompositorMouseWheel forward in
	 * WiredUI_KeyEvent returns before that switch, so a switch-based branch is
	 * unreachable on a scrolling panel. Runs for every wheel event → covers both
	 * scrolling and non-scrolling panels. Step mirrors the keyboard left/right
	 * slider step (range/20, 0.01 floor). deltaY sign: MWHEELUP passes -1.0,
	 * MWHEELDOWN +1.0 → wheel up (deltaY<0) increments. */
	{
		const wiredItemDef_t *hov = WiredUI_CompositorHoveredItem();
		if ( hov && hov->type == ITEM_TYPE_SLIDER && hov->cvar[0] ) {
			char  sbuf[64];
			float val, step;
			WiredUI_StateGetString( hov->cvar, sbuf, sizeof( sbuf ) );
			val  = atof( sbuf );
			step = ( hov->sliderData.maxVal - hov->sliderData.minVal ) / 20.0f;
			if ( step < 0.01f ) step = 0.01f;
			if ( deltaY < 0.0f ) val += step;   /* MWHEELUP   -> increment */
			else                 val -= step;   /* MWHEELDOWN -> decrement */
			if ( val < hov->sliderData.minVal ) val = hov->sliderData.minVal;
			if ( val > hov->sliderData.maxVal ) val = hov->sliderData.maxVal;
			WiredUI_StateSetString( hov->cvar, va( "%g", val ) );
			return qtrue;   /* consume: no panel scroll, no switch fall-through */
		}
		/* Spinner wheel-adjust: same compositor path as the slider (wins over
		 * panel scroll even inside an overflowing viewport). One step per notch
		 * via the shared core helper so wheel == keyboard == +/- button. The
		 * const cast is safe: WiredUI_SpinnerAdjust only writes the bound cvar,
		 * exactly like the slider branch above mutates hov->cvar in place. */
		if ( hov && hov->type == ITEM_TYPE_SPINNER && hov->cvar[0] ) {
			WiredUI_SpinnerAdjust( (wiredItemDef_t *) hov,
				( deltaY < 0.0f ) ? +1 : -1, 1.0f );   /* MWHEELUP -> increment */
			return qtrue;
		}
	}

	/* Accumulate for the next in-frame Clay_UpdateScrollContainers. Applying it
	 * here would race Clay's openThisFrame eviction and lose the delta
	 * (see wui_clay_pendingWheelDeltaY).
	 *
	 * Sign: callers pass deltaY>0 for MWHEELDOWN (intent: reveal lower content)
	 * and deltaY<0 for MWHEELUP. Clay's scroll convention is the opposite —
	 * scrollPosition.y is <= 0 and grows MORE negative to reveal lower content
	 * (childOffset shifts content up), and Clay_UpdateScrollContainers does
	 * scrollPosition.y += delta.y*10 then clamps to [-(overflow), 0]. So a
	 * positive delta clamps straight back to 0 (no movement — the original
	 * bug). Negate here so MWHEELDOWN drives scrollPosition negative. */
	wui_clay_pendingWheelDeltaY += -deltaY;

	/* Surface the fade-in scrollbar on scroll. */
	wui_clay_flexScrollFadeTime = cls.realtime;

	/* Consume (suppress the legacy K_MWHEEL fall-through) only when the cursor
	 * is over a scrollable (overflowing) viewport from the last emitted frame. */
	for ( i = 0; i < wui_scroll_rect_count; i++ ) {
		const wui_scroll_entry_t *r = &wui_scroll_rects[ i ];
		if ( !r->overflow ) continue;
		if ( wui_compositor_pointer_x >= r->x && wui_compositor_pointer_x <= r->x + r->w
		  && wui_compositor_pointer_y >= r->y && wui_compositor_pointer_y <= r->y + r->h ) {
			overScroll = qtrue;
			break;
		}
	}

	return overScroll;
}

/* ── flex scroll-container scrollbar DRAG ─────────────────────────────────────
 *
 * The macOS-style thumb (wui_clay_emit_flex_scrollbar) was paint-only. These
 * three entry points add click+drag: on a mouse-down over a thumb we latch the
 * container id and the grab offset; on motion we map the cursor's Y within the
 * track to an absolute scroll target (applied in-frame like the wheel path); on
 * up we release. Called from WiredUI_KeyEvent (K_MOUSE1 down/up) and
 * WiredUI_MouseEvent (motion) in cl_wired_ui.c, gated BEFORE the slider/hover
 * paths so a thumb grab wins over whatever sits under it. Uses the same cached
 * absolute thumb/track geometry the thumb is DRAWN from (single-source). */

/* True while a flex-scroll thumb drag is in progress (suppresses hover/focus
 * churn in the motion handler, mirroring wui_sliderDragging). */
qboolean WiredUI_CompositorScrollbarDragging( void )
{
	return wui_scroll_drag_clayId != 0;
}

/* Try to begin a thumb drag at the current cursor. Returns qtrue iff the cursor
 * is over a (visible, overflowing) container's thumb — caller then suppresses
 * the normal click. A small horizontal pad widens the thin (4px) thumb into a
 * comfortable grab target; vertically the hit is the exact thumb extent. */
qboolean WiredUI_CompositorScrollbarDragStart( float cx, float cy )
{
	const float padX = 6.0f;   /* physical-px grab pad around the thin thumb */
	int i;

	if ( !wui_clay_initialized ) return qfalse;

	for ( i = 0; i < wui_scroll_rect_count; i++ ) {
		wui_scroll_entry_t *r = &wui_scroll_rects[ i ];
		if ( !r->overflow || r->thumbH <= 0.0f ) continue;
		if ( cx >= r->thumbX - padX && cx <= r->thumbX + r->thumbW + padX
		  && cy >= r->thumbY        && cy <= r->thumbY + r->thumbH ) {
			wui_scroll_drag_clayId = r->clayId;
			wui_scroll_drag_grabDY = cy - r->thumbY;   /* keep grab point steady */
			wui_clay_flexScrollFadeTime = cls.realtime;
			return qtrue;
		}
	}
	return qfalse;
}

/* Update the in-progress thumb drag to the current cursor Y. Maps the thumb's
 * TOP (cursorY - grabOffset) along the track [trackY .. trackY+trackH-thumbH]
 * to a scroll fraction, then to an absolute scrollPosition.y (<= 0), queued for
 * the next in-frame apply. No-op if not dragging. */
void WiredUI_CompositorScrollbarDragUpdate( float cy )
{
	int i;
	if ( wui_scroll_drag_clayId == 0 ) return;

	for ( i = 0; i < wui_scroll_rect_count; i++ ) {
		wui_scroll_entry_t *r = &wui_scroll_rects[ i ];
		float travel, thumbTop, frac, maxScroll;
		if ( r->clayId != wui_scroll_drag_clayId ) continue;
		if ( !r->overflow || r->thumbH <= 0.0f ) return;

		travel = r->trackH - r->thumbH;                 /* thumb's vertical range */
		if ( travel <= 0.0f ) return;
		thumbTop = cy - wui_scroll_drag_grabDY;         /* desired thumb top (px) */
		frac = ( thumbTop - r->trackY ) / travel;       /* 0=top .. 1=bottom */
		if ( frac < 0.0f ) frac = 0.0f;
		if ( frac > 1.0f ) frac = 1.0f;

		maxScroll = r->contentH - r->viewH;
		if ( maxScroll < 0.0f ) maxScroll = 0.0f;
		wui_clay_pendingScrollId = r->clayId;
		wui_clay_pendingScrollY  = -frac * maxScroll;   /* Clay: content shifts up */
		wui_clay_flexScrollFadeTime = cls.realtime;
		return;
	}
}

/* End any in-progress thumb drag (mouse-up). */
void WiredUI_CompositorScrollbarDragEnd( void )
{
	wui_scroll_drag_clayId = 0;
	wui_scroll_drag_grabDY = 0.0f;
}

/* Page-scroll the scroll container under the cursor by ±one viewport height
 * (minus a small overlap), for PageUp (dir<0) / PageDown (dir>0). Returns qtrue
 * iff a scrollable container was under the cursor and a scroll was queued — the
 * caller (keyboard handler) then suppresses the default menu page-scroll.
 * Applied in-frame like the wheel/drag paths. */
qboolean WiredUI_CompositorScrollPage( float cx, float cy, int dir )
{
	int i;
	if ( !wui_clay_initialized || dir == 0 ) return qfalse;

	for ( i = 0; i < wui_scroll_rect_count; i++ ) {
		wui_scroll_entry_t *r = &wui_scroll_rects[ i ];
		float maxScroll, page, target;
		if ( !r->overflow ) continue;
		if ( !( cx >= r->x && cx <= r->x + r->w && cy >= r->y && cy <= r->y + r->h ) )
			continue;

		maxScroll = r->contentH - r->viewH;
		if ( maxScroll < 0.0f ) maxScroll = 0.0f;
		/* One viewport minus ~12% overlap so the reader keeps a line of context. */
		page   = r->viewH * 0.88f;
		if ( page < 1.0f ) page = 1.0f;
		/* r->scrollY is <= 0; PageDown (dir>0) reveals lower content = MORE
		 * negative; PageUp (dir<0) = LESS negative. */
		target = r->scrollY - (float) dir * page;
		if ( target > 0.0f )        target = 0.0f;
		if ( target < -maxScroll )  target = -maxScroll;
		wui_clay_pendingScrollId    = r->clayId;
		wui_clay_pendingScrollY     = target;
		wui_clay_flexScrollFadeTime = cls.realtime;
		return qtrue;
	}
	return qfalse;
}

/* Public accessor for the focused widget id on the active panel, for
 * future use by the focus-chain Tab walker. Returns 0 if no
 * panel has been focused yet. */
uint32_t WiredUI_CompositorGetFocusedId( void )
{
	int i;
	for ( i = 0; i < wui_visible_panel_count; i++ ) {
		const wui_panel_state_t *ps = wui_panel_state_for( wui_visible_panels[ i ] );
		if ( ps && ps->focusedId ) return ps->focusedId;
	}
	return 0;
}

/* Fetch the ACTUAL Clay-rendered rect of an item from the previous frame's
 * layout (Clay's persistent element hashmap). Settings rows inside flexbox
 * panels are positioned by Clay's flex flow (ATTACH_TO_NONE), so their true
 * on-screen rect is what Clay actually laid out, not the compatibility
 * resolvedRect snapshot. Returns qtrue and fills *out (physical px) when the
 * element was found in the last layout, qfalse otherwise. Same 1-frame-lag
 * pattern as the focus-highlight bbox lookup — harmless for a popup that only
 * opens after the row has already rendered at least once. */
qboolean WiredUI_ClayItemRenderedRect( const wiredMenuDef_t *panel,
                                       const wiredItemDef_t *item,
                                       wuiPixelRect_t *out )
{
	uint32_t          clayId;
	Clay_ElementId    eid;
	Clay_ElementData  ed;

	if ( !panel || !item || !out ) return qfalse;
	clayId = wui_clay_id_for_item( panel, item );
	if ( !clayId ) return qfalse;
	eid.id = clayId;
	ed = Clay_GetElementData( eid );
	if ( !ed.found ) return qfalse;
	out->x = ed.boundingBox.x;
	out->y = ed.boundingBox.y;
	out->w = ed.boundingBox.width;
	out->h = ed.boundingBox.height;
	return qtrue;
}

/* Single-source slider geometry (physical px). Resolves the slider's TRACK
 * element (by its stable pointer-derived id) to the previous frame's rendered
 * bounding box, then returns the USABLE value range — the track inset by half a
 * thumb on each side so the thumb centre can travel the full [min..max] without
 * overhanging either end. Both the render (thumb placement) and the input
 * (cursorX → fraction) consume this, so the drawn thumb and the drag hit-zone
 * are guaranteed identical. Returns qtrue and fills *usableX / *usableW /
 * *thumbW / *trackRect when the track has rendered at least once; qfalse before
 * the first layout pass (caller falls back to the legacy resolvedRect math). */
qboolean WiredUI_SliderTrackGeom( const wiredItemDef_t *item,
                                  float *usableX, float *usableW,
                                  float *thumbW,  wuiPixelRect_t *trackRect )
{
	uint32_t          trackId;
	Clay_ElementId    eid;
	Clay_ElementData  ed;
	float             dpi, tw, halfThumb, ux, uw;

	if ( !item ) return qfalse;
	trackId = wui_clay_slider_track_id_for_item( item );
	if ( !trackId ) return qfalse;
	eid.id = trackId;
	ed = Clay_GetElementData( eid );
	if ( !ed.found || ed.boundingBox.width <= 0.0f ) return qfalse;

	dpi       = WiredUI_GetDpiScale();
	tw        = WUI_SLIDER_THUMB_W_PX * dpi;
	halfThumb = tw * 0.5f;
	ux        = ed.boundingBox.x + halfThumb;
	uw        = ed.boundingBox.width - tw;
	if ( uw < 1.0f ) { uw = ed.boundingBox.width; ux = ed.boundingBox.x; }

	if ( usableX )   *usableX = ux;
	if ( usableW )   *usableW = uw;
	if ( thumbW )    *thumbW  = tw;
	if ( trackRect ) {
		trackRect->x = ed.boundingBox.x;
		trackRect->y = ed.boundingBox.y;
		trackRect->w = ed.boundingBox.width;
		trackRect->h = ed.boundingBox.height;
	}
	return qtrue;
}

/* Single-source SPINNER button geometry (physical px). Resolves the −/+ button
 * sub-element (by its stable pointer-derived id) to the previous frame's rendered
 * bounding box. The render emits the button WITH this id; the input path hit-tests
 * the SAME rect — click zone and drawn box can never diverge, and it is Clay-rect
 * (post-flex) not the stale resolvedRect. `inc` picks + (qtrue) vs − (qfalse).
 * Returns qfalse before the first layout pass (caller ignores the click). */
qboolean WiredUI_SpinnerButtonRect( const wiredItemDef_t *item, qboolean inc,
                                    wuiPixelRect_t *out )
{
	uint32_t          btnId;
	Clay_ElementId    eid;
	Clay_ElementData  ed;

	if ( !item || !out ) return qfalse;
	btnId = wui_clay_spinner_btn_id_for_item( item, inc );
	if ( !btnId ) return qfalse;
	eid.id = btnId;
	ed = Clay_GetElementData( eid );
	if ( !ed.found || ed.boundingBox.width <= 0.0f ) return qfalse;
	out->x = ed.boundingBox.x;
	out->y = ed.boundingBox.y;
	out->w = ed.boundingBox.width;
	out->h = ed.boundingBox.height;
	return qtrue;
}

/* Public accessor for the currently hovered widget id (the latest
 * Clay_GetPointerOverIds query result). Used by verification
 * logging + the click path; a later change will surface this through a debug
 * HUD overlay. */
uint32_t WiredUI_CompositorGetHoveredId( void )
{
	int i;
	uint32_t hovered = 0;
	for ( i = 0; i < wui_visible_panel_count; i++ ) {
		const wui_panel_state_t *ps = wui_panel_state_for( wui_visible_panels[ i ] );
		if ( ps && ps->hoveredId ) hovered = ps->hoveredId;
	}
	return hovered;
}

/* resolve the currently hovered Clay element ID back
 * to its wiredItemDef_t. The Clay-side hover state is populated post-
 * layout (Clay_GetPointerOverIds query), so this is a prior-frame lookup
 * — same 1-frame lag pattern as the focus-highlight bbox lookup, and
 * imperceptible for user-driven mouse motion. Returns NULL when nothing
 * is hovered (cursor in empty space) or before the first layout pass. */
const wiredItemDef_t *WiredUI_CompositorHoveredItem( void )
{
	uint32_t              clayId = WiredUI_CompositorGetHoveredId();
	const wiredItemDef_t *item   = NULL;
	const wiredMenuDef_t *panel  = NULL;
	if ( !clayId ) return NULL;
	if ( wui_id_map_lookup( clayId, &item, &panel ) ) return item;
	return NULL;
}

/* Compositor-side visible-panel count, exposed for instrumentation
 * (the visible-only narrow check). */
int WiredUI_CompositorGetVisiblePanelCount( void )
{
	return wui_visible_panel_count;
}

/* Tab / Shift-Tab focus cycle within the currently active panel.
 *
 * The compositor-side bookkeeping: walk the active
 * panel's wiredItemDef_t list, find the next/prev focusable item (skipping
 * .notselectable + .decoration), record its Clay element id into the
 * panel's persistent state. Legacy WiredUI_KeyEvent's Tab handler still
 * drives the visible focus (wui_focusItem in cl_wired_ui.c) — the
 * compositor's tracking runs in parallel so a later change can flip authority
 * with one gate change.
 *
 * Returns qtrue iff the compositor consumed the event (currently never —
 * legacy path retains authority). */
qboolean WiredUI_CompositorTabFocus( qboolean forward )
{
	const wiredMenuDef_t *panel;
	wui_panel_state_t    *ps;
	int                   currentIdx = -1;
	int                   n;
	int                   i;
	int                   step;
	uint32_t              currentId;

	if ( wui_visible_panel_count == 0 ) return qfalse;
	panel = wui_visible_panels[ 0 ];
	if ( !panel || panel->itemCount <= 0 ) return qfalse;

	ps = wui_panel_state_for( panel );
	if ( !ps ) return qfalse;
	currentId = ps->focusedId;
	n         = panel->itemCount;
	step      = forward ? 1 : -1;

	/* Find current focused index (if any) by walking the panel's items
	 * + matching against the cached focusedId. The id-map populated by
	 * the emit walk also stores ids, but matching by panel pointer is
	 * cheaper here. */
	if ( currentId ) {
		for ( i = 0; i < n; i++ ) {
			const wiredItemDef_t *it = panel->items[ i ];
			if ( !it ) continue;
			if ( wui_clay_id_for_item( panel, it ) == currentId ) {
				currentIdx = i;
				break;
			}
		}
	}

	/* Cycle to next/prev focusable item — skip notselectable + decoration. */
	for ( i = 0; i < n; i++ ) {
		const wiredItemDef_t *it;
		int                   probe = (currentIdx + step * (i + 1) + n * (n + 1)) % n;
		it = panel->items[ probe ];
		if ( !it ) continue;
		if ( it->notselectable || it->decoration ) continue;
		ps->focusedId = wui_clay_id_for_item( panel, it );
		break;
	}

	return qfalse;   /* legacy retains authority */
}

#endif /* FEAT_WIRED_UI */
