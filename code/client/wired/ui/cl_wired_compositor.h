// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
cl_wired_compositor.h — WiredUI compositor frame loop + canonical window rect.

The initial scaffold provides lifecycle stubs +
WiredUI_Arena ownership + per-frame Clay reset. Does NOT yet replace
SCR_DrawScreenField — rendering continues to flow through the legacy
dispatcher. Later stages layer panels / Clay tree emission / modality /
loading migration on top of this scaffold.

See docs/wiredui-compositor-spec.md for the authoritative migration spec.
*/

#ifndef CL_WIRED_COMPOSITOR_H
#define CL_WIRED_COMPOSITOR_H

#include "../../../qcommon/q_feats.h"

#if FEAT_WIRED_UI

#include "../../../qcommon/q_shared.h"
#include "../../../qcommon/arena.h"
#include "cl_wired_layout.h"   /* wuiEasing_t — used by Anim_* API */

struct wiredMenuDef_s;
struct wiredItemDef_s;

/* ── canonical window rect ────────────────────────────────────────────
 * Owned by the compositor. Initially seeds from cls.glconfig.vidWidth/Height
 * (still overloaded at this stage — a later stage restores it to canonical
 * window-pixel size and wires the platform-layer publish path). Consumers
 * read via WiredUI_GetWindowRect.
 */
typedef struct {
	int      widthPx,  heightPx;   /* device-pixel size — FBO + layout canonical */
	int      widthLog, heightLog;  /* logical-point size — input coords / DPI-relative */
	float    dpiScale;             /* heightPx / heightLog; 1.0 on non-HiDPI */
	int      generation;           /* bumped per publish */
	qboolean valid;                /* false until platform layer's first publish */
} wuiWindowRect_t;

/* ── arena accessor ───────────────────────────────────────────────────
 * WiredUI_Arena is process-lifetime per docs/memory-architecture-handoff.md
 * "Idealized Arena Map" (future target → landed this cycle). Hosts the menu
 * pool, Clay arena slice (4 MB), per-frame scratch arena, anim state, attract
 * scheduler state. Sized by wired_ui_arena_mb cvar (default 20).
 */
arena_t *WiredUI_GetArena( void );

/* ── DPI scale accessor ───────────────────────────────────────────────
 * Physical/logical window-size ratio (vidWidth / vidWidthLogical), >=1.0;
 * 1.0 on a non-HiDPI display or before the platform layer has published a
 * logical size. The text-emit path multiplies font sizes by this so glyphs
 * render at a consistent physical size across displays. */
float WiredUI_GetDpiScale( void );

/* ── UI root scale accessor ───────────────────────────────────────────
 * The logical size that `1rem` means, before dpiScale.
 *
 * This is the ONE quantity the UI is sized against. Authoring in rem instead of
 * px is what makes "scale the whole interface" a single number rather than an
 * edit to every string — the case a handheld needs, where a 7" 1280x800 panel
 * and an external 4K want different UI sizes at the same dpiScale.
 *
 * It is not the same question as dpiScale. dpiScale answers "how many physical
 * pixels is a logical one" (a property of the display); the root answers "how
 * big should the UI be" (a property of the user's preference). Physical size is
 * the product of the two, so both still apply:  px = rem x root x dpiScale.
 *
 * Backed by ui_rootSize (registered with bounds in WiredUI_Init), defaulting to
 * WUI_DEFAULT_FONT_SIZE so unauthored text is exactly 1rem and the shipped look
 * does not move. */
float WiredUI_GetRootScale( void );

/* ── lifecycle ────────────────────────────────────────────────────────
 * Called from WiredUI_Init / WiredUI_Shutdown (cl_wired_ui.c:1697 / 1835).
 * Idempotent on Init (vid_restart safe — compositor survives via the
 * existing WiredUI_Init/Shutdown lifecycle).
 */
void WiredUI_CompositorInit( void );
void WiredUI_CompositorShutdown( void );

/* ── per-frame tick (stub) ────────────────────────────────────────────
 * Called from CL_Frame (cl_main.c:3071) before SCR_UpdateScreen. This
 * does NOT drive rendering: it ticks Clay state (Clay_Reset +
 * SetLayoutDimensions when the window-pixel size changes) so subsequent
 * stages can layer the panel walk + render-command emission on top
 * without churn at the lifecycle layer.
 */
void WiredUI_CompositorFrame( int realtimeMs );

/* ── font-table refresh ────────────────────────────────────────────────
 * Clay's font indirection table is populated from MSDF's font registry.
 * The compositor lives at process scope (CL_Init / CL_Shutdown) but MSDF
 * fonts load later inside WiredUI_Init via Text_Init. This entry point
 * refreshes the indirection table; WiredUI_Init invokes it after Text_Init,
 * and hot-reload re-invokes it when a font atlas is reloaded.
 */
void WiredUI_ClayRefreshFontTable( void );

/* Clay-initialization predicate. Public accessor
 * for the internal wui_clay_initialized flag so the WiredUI_Init failure
 * branch can detect a Clay-arena-alloc failure without poking statics.
 * Returns qtrue iff WiredUI_ClayInit ran to completion (arena alloc +
 * Clay_Initialize + measure-text hook). */
qboolean WiredUI_IsClayInitialized( void );

/* ── per-frame Clay tree walk + render-command dispatch ──
 * Called from SCR_DrawScreenField (cl_scrn.c) AFTER re.BeginFrame and
 * BEFORE the legacy WiredUI_Refresh call. Walks visible panels (only
 * menus with .visible or .fullscreen), emits Clay declarations, calls
 * Clay_BeginLayout/EndLayout per panel, captures Clay_GetPointerOverIds()
 * into per-panel hover state, then dispatches the render command array
 * through re.* / MSDF.
 *
 * Actual draw output is wired behind an internal switch
 * (wui_compositor_emit_to_swapchain in cl_wired_clay.c) defaulting ON so
 * "both paths run; SCR_DrawScreenField overlays on top" per directive.
 * A later stage retires the legacy walk and removes the switch.
 */
void WiredUI_CompositorEmitFrame( void );

/* ── hit-test public entries ───────────────────────────────────────────
 * Clay-driven hover/click tracking runs IN PARALLEL with the
 * legacy WiredUI_KeyEvent / WiredUI_MouseEvent dispatch. The compositor
 * builds a per-frame ID-map from Clay element ids to wiredItemDef_t
 * pointers + records the cursor + button state. Actual action dispatch
 * is gated by wui_compositor_input_active (currently OFF — legacy is
 * authoritative). A later stage flips the gate on alongside SCR_DrawScreenField
 * retirement.
 *
 * Wiring (cl_wired_ui.c):
 *   WiredUI_MouseEvent post-clamp → WiredUI_CompositorPointerMoved(x, y)
 *   WiredUI_KeyEvent (K_MOUSE1)   → WiredUI_CompositorMouseButton(down)
 *   WiredUI_KeyEvent (K_MWHEEL*)  → WiredUI_CompositorMouseWheel(dy)
 */
void     WiredUI_CompositorPointerMoved        ( float x, float y );
qboolean WiredUI_CompositorMouseButton         ( qboolean down );
qboolean WiredUI_CompositorMouseWheel          ( float deltaY );
qboolean WiredUI_CompositorTabFocus            ( qboolean forward );

/* Flex scroll-container scrollbar DRAG (click+drag the macOS-style thumb) +
 * keyboard page-scroll. The scrollbar was paint-only; these add interaction.
 * DragStart returns qtrue iff the cursor grabbed a thumb (caller suppresses the
 * normal click); DragUpdate maps cursorY→scroll while held; DragEnd releases;
 * Dragging() reports in-progress (so the motion handler skips hover churn).
 * ScrollPage pages the container under the cursor by ±one viewport (PageUp
 * dir<0 / PageDown dir>0), returning qtrue when it consumed the key. All use
 * the SAME cached thumb/track geometry the thumb is drawn from (single-source),
 * in physical px, and apply their scroll in-frame like the wheel path. */
qboolean WiredUI_CompositorScrollbarDragStart  ( float cx, float cy );
void     WiredUI_CompositorScrollbarDragUpdate ( float cy );
void     WiredUI_CompositorScrollbarDragEnd    ( void );
qboolean WiredUI_CompositorScrollbarDragging   ( void );
qboolean WiredUI_CompositorScrollPage          ( float cx, float cy, int dir );

/* Accessors for hover/focus state — useful for verification + future
 * focus-chain Tab routing (next step). */
uint32_t WiredUI_CompositorGetHoveredId        ( void );
uint32_t WiredUI_CompositorGetFocusedId        ( void );
int      WiredUI_CompositorGetVisiblePanelCount( void );

/* Fetch the actual Clay-rendered rect (physical px) of an item from the
 * previous frame's layout. Returns qtrue when the element was found. Settings
 * rows inside flexbox panels are flex-positioned by Clay, so this is the
 * authoritative on-screen rect — not the compatibility resolvedRect snapshot. */
qboolean WiredUI_ClayItemRenderedRect( const struct wiredMenuDef_s *panel,
                                       const struct wiredItemDef_s *item,
                                       wuiPixelRect_t *out );

/* Single-source slider geometry (physical px, from the track's Clay-rendered
 * rect). Returns the USABLE value range — the track inset by half a thumb on
 * each side so the thumb centre travels the full [min..max] without overhanging.
 * The render (thumb placement) and the input (cursorX -> fraction) both consume
 * this, so the drawn thumb and the drag hit-zone are identical. Any out-param
 * may be NULL. Returns qtrue once the track has rendered at least once; qfalse
 * before the first layout pass (caller falls back to legacy resolvedRect math).
 * value fraction from an absolute cursorX = clamp01( (cursorX - *usableX)/ *usableW ). */
qboolean WiredUI_SliderTrackGeom( const struct wiredItemDef_s *item,
                                  float *usableX, float *usableW,
                                  float *thumbW,  wuiPixelRect_t *trackRect );

/* Single-source SPINNER stepper-button geometry (physical px, Clay-rect). Fills
 * *out with the −(inc=qfalse) / +(inc=qtrue) button's rendered bounding box from
 * the previous frame. The renderer draws the button WITH the same id, so the
 * click/hold hit-zone matches the drawn box exactly (no resolvedRect drift).
 * Returns qfalse before the button has laid out at least once. */
qboolean WiredUI_SpinnerButtonRect( const struct wiredItemDef_s *item, qboolean inc,
                                    wuiPixelRect_t *out );

/* Item currently under the cursor on the topmost visible panel (or NULL). The
 * click path uses it to adopt mouse-hovered items as the focused item so
 * focusedItem-driven interactions (slider drag, spinner +/- buttons) fire on
 * click, not only after keyboard nav. */
const struct wiredItemDef_s *WiredUI_CompositorHoveredItem( void );

/* Engine-drawn multi-column listbox header band (§7.1 list-view).
 *
 * WiredUI_ListboxHeaderHeight returns the header band's height in physical px
 * (0 when the listbox authors no columnHeaders). The click path uses it to
 * (a) detect header clicks and (b) shift the body-row hit math down by it.
 *
 * WiredUI_ListboxHeaderColumnAtX maps an absolute screen cursorX to the header
 * column under it, using the SAME wui_listbox_column_geom model the header +
 * body are drawn with — so a header click hits the same column the label sits
 * over. listX/listW are the listbox's on-screen rect (physical px). Returns the
 * column index, or -1 when outside any column. */
float WiredUI_ListboxHeaderHeight( const struct wiredItemDef_s *item );
int   WiredUI_ListboxHeaderColumnAtX( const struct wiredItemDef_s *item,
                                      float listX, float listW, float cursorX );

/* Single-source vertical-listbox scrollbar-thumb geometry (physical px). Given
 * the listbox's on-screen rect (from WiredUI_ClayItemRenderedRect), fills the
 * absolute thumb rect + the thumb's vertical travel band (trackTop..trackTop+
 * travel maps scroll offset 0..max). The drag hit-test + drag-to-offset mapping
 * consume this so the grab zone matches the drawn thumb exactly. Returns qfalse
 * for non-overflowing or horizontal-scroll listboxes (no draggable thumb). */
qboolean WiredUI_ListboxScrollbarGeom( const struct wiredItemDef_s *item,
                                       float listX, float listY,
                                       float listW, float listH,
                                       wuiPixelRect_t *outThumb,
                                       float *outTrackTop, float *outTrackTravel );

/* ── radio / segmented control (ITEM_TYPE_RADIOBUTTON) ────────────────────────
 * A segmented control renders a MULTI's options inline as N equal-width
 * horizontal segments. These share the multi option-source + value-match logic
 * so the selected segment and the faked dropdowns agree. Selection is one-write
 * (WiredUI_RadioSelectIndex) so click + keyboard stay in lockstep. */
int         WiredUI_RadioSegmentCount ( struct wiredItemDef_s *item );
const char *WiredUI_RadioSegmentLabel ( struct wiredItemDef_s *item, int index );
int         WiredUI_RadioSelectedIndex( struct wiredItemDef_s *item );
void        WiredUI_RadioSelectIndex  ( struct wiredItemDef_s *item, int index );

/* Map an absolute screen cursorX to the segment index under it, given the
 * control's on-screen rect (physical px, from WiredUI_ClayItemRenderedRect).
 * The value cell occupies the right portion of the row; segments split it
 * equally. Returns -1 when outside the segment band. Single source of segment
 * geometry — the renderer draws with the SAME split. */
int   WiredUI_RadioSegmentAtX( struct wiredItemDef_s *item,
                               float rowX, float rowW, float cursorX );

/* Left edge + width (physical px, relative to the row's left origin) of the
 * value-cell segment band, and each segment's width. The renderer and the
 * hit-test both call this so draw-rect and hit-rect never diverge. */
void  WiredUI_RadioSegmentBand( struct wiredItemDef_s *item, float rowW,
                                float *outBandX, float *outBandW, float *outSegW );

/* ── per-menu Lua chunk compile/release dispatch ──────────────────────
 * Parser-side companions to the eval-time wuiLuaVMOps_t. Routes
 * Compile / Release into System or User VM based on the panel's `vm`
 * keyword (wiredMenuDef_t.vm field). Hot-reload purge uses the same
 * dispatcher so chunks release into the VM that compiled them.
 *
 * Forward-declare wiredMenuDef_s here so callers that include this
 * header don't need cl_wired_ui.h's full struct definitions. */
struct wiredMenuDef_s;

int  WiredUI_CompositorCompileChunkForMenu( const struct wiredMenuDef_s *menu,
                                             const char *text,
                                             const char *chunkName );
void WiredUI_CompositorReleaseChunkForMenu( const struct wiredMenuDef_s *menu,
                                             int chunkRef );

/* ── popup queue API ───────────────────────────────────────────────────
 * Bounded ring queue of named panels gated to LAYER_POPUP. Popups emit
 * above MENU_STACK + block modal input from underlying layers. Push at
 * top of queue, dismiss from front. Designed for confirmation / error
 * dialogs; the loading screen migration uses LAYER_LOADING
 * separately. */
void     WiredUI_PushPopup       ( const char *menuName );
void     WiredUI_DismissPopup    ( void );
qboolean WiredUI_HasActivePopup  ( void );
const char *WiredUI_FrontPopupName( void );    /* NULL if empty */

/* ── animation module — a narrow v1 ───────────────────────────────────
 * Single-tween-per-handle interpolation that writes to a WiredStore numeric
 * key over `durationMs`. Bindings on the target key refresh automatically
 * via the existing store-dirty pattern. 4 easings (wuiEasing_t already
 * defined in cl_wired_layout.h:87 — linear, ease-in, ease-out, ease-in-out);
 * no chained sequences / completion callbacks. Pool size = 64.
 *
 * Sentinel for "unrecognised ease name" returned by Anim_EaseFromString:
 * a negative value (-1). Callers test against this rather than the enum. */
#define WUI_EASE_UNKNOWN  (-1)

int  Anim_StartTween       ( const char *storeKey, float endValue,
                              int durationMs, wuiEasing_t ease );
void Anim_Cancel           ( int handle );
void Anim_FrameUpdate      ( int nowMs );

/* String→ease helper for the Lua + console-command surfaces. Returns
 * WUI_EASE_UNKNOWN (-1) for an unknown name; callers map to a sensible
 * default + log once. */
int  Anim_EaseFromString   ( const char *name );

/* Register the `anim` namespace in both System and User VMs. Called once
 * during client init alongside WiredStoreLua_Init / WiredUI_LuaInit. */
void WiredAnimLua_Init     ( void );

/* Dev commands — wui_popup_test / wui_anim_test. Both gated by
 * `developer 1` inside the handler. Kept in tree as engineering utilities;
 * see the comment above their definitions in cl_wired_compositor.c. */
void WiredUI_CompositorRegisterDevCommands  ( void );
void WiredUI_CompositorUnregisterDevCommands( void );

/* ── layer force-override accessor ─────────────────────────────────────
 * Called from wui_layer_active() in cl_wired_clay.c BEFORE the production
 * cls.state-driven gating switch. Returns qtrue iff the layer's bit is
 * set in the dev-only override mask (driven by wui_layer_test_* commands).
 * Production default is mask=0 — zero-cost on the normal path.
 *
 * Parameter is wuiLayer_t (declared in cl_wired_ui.h) but typed as int here
 * to keep this header free of cl_wired_ui.h's full include cost; callers
 * cast at the call site.
 */
qboolean WiredUI_LayerForceOverrideTest( int layer );

/* ── whole-mask accessor ───────────────────────────────────────────────
 * Read-only access to the dev-override mask for the SCR_DrawScreenField
 * CA_DISCONNECTED carve-out (cl_scrn.c). Returns 0 in production (the
 * carve-out then no-ops, byte-identical to the prior behaviour).
 * Half-life: deletes alongside SCR_DrawScreenField. */
/* Per-layer visible/paused state, recomputed once per frame by the emit walk.
 * Read by content that needs to know whether it is on screen, and by anything
 * animating off WiredUI_LayerClockMs so a paused layer genuinely stops. */
/* int rather than wuiLayer_t: this header is included ahead of cl_wired_ui.h
 * in several TUs, and the sibling override-mask accessors already take the
 * plain type for the same reason. */
void     WiredUI_LayerStateSet( int layer, qboolean visible, qboolean paused );
qboolean WiredUI_LayerVisible ( int layer );
qboolean WiredUI_LayerPaused  ( int layer );
int      WiredUI_LayerClockMs ( int layer );

uint32_t WiredUI_LayerForceOverrideMask( void );

/* ── exclusive-mode accessor ───────────────────────────────────────────
 * Consulted by wui_layer_active(); when qtrue alongside a non-zero
 * override mask, the predicate suppresses unmasked layers instead of
 * falling through to production cls.state gating. Returns qfalse in
 * production (no dev command flipped it). */
qboolean WiredUI_LayerForceExclusiveMode( void );

#endif /* FEAT_WIRED_UI */
#endif /* CL_WIRED_COMPOSITOR_H */
