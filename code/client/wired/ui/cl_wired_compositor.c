// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
cl_wired_compositor.c — WiredUI compositor module.

Initial scope: lifecycle + WiredUI_Arena ownership + per-frame
Clay reset. Does NOT drive rendering yet. Later passes add the panel walk,
tree-to-Clay conversion, modality, and SCR_DrawScreenField retirement.

Clay implementation, font indirection table, and measure-callback wiring live
in the sibling file cl_wired_clay.c (the single TU that defines
CLAY_IMPLEMENTATION). This file is Clay-agnostic in its public surface.
*/

#include "../../client.h"
#include "cl_wired_compositor.h"

LOG_DECLARE_CHANNEL( ch_ui, "ui" );

#if FEAT_WIRED_UI

/* Forward declarations for the Clay glue in cl_wired_clay.c (kept here so
 * neither file pulls clay.h transitively into the rest of the client). */
void WiredUI_ClayInit             ( arena_t *parent );
void WiredUI_ClayShutdown         ( void );
void WiredUI_ClayFrame            ( int widthPx, int heightPx );
void WiredUI_ClayRefreshFontTable ( void );
void WiredUI_CompositorEmitFrame  ( void );

/* ── module state ─────────────────────────────────────────────────── */
static arena_t        *wui_arena         = NULL;
static cvar_t         *wired_ui_arena_mb = NULL;
static wuiWindowRect_t wui_windowRect;
static qboolean        wui_compositor_initialized = qfalse;

/* Resolve the logical (DPI-independent) window size in points. Normally the
 * platform layer publishes it via cls.glconfig.vidWidthLogical/Height (SDL_Get-
 * WindowSize / GetDpiForWindow); when unpublished it falls back to the physical
 * pixel size (→ dpiScale 1.0).
 *
 * Test seam (W-67): r_uiLogicalWidthTest / r_uiLogicalHeightTest, when > 0,
 * stand in for the platform-published logical size — exactly the value a HiDPI
 * SDL_GetWindowSize would yield. This drives the REAL dpiScale = physicalPx /
 * logical computation below (NOT the r_uiDpiScaleTest final-value override),
 * so the headless gate can verify the genuine ratio math on a non-HiDPI display.
 * 0-default; inert in the ship path. */
static void wui_logical_size( int wPx, int hPx, int *outWLog, int *outHLog )
{
	int wTest = (int) Cvar_VariableValue( "r_uiLogicalWidthTest" );
	int hTest = (int) Cvar_VariableValue( "r_uiLogicalHeightTest" );
	int wLog  = ( wTest > 0 ) ? wTest
	          : ( cls.glconfig.vidWidthLogical  > 0 ? cls.glconfig.vidWidthLogical  : wPx );
	int hLog  = ( hTest > 0 ) ? hTest
	          : ( cls.glconfig.vidHeightLogical > 0 ? cls.glconfig.vidHeightLogical : hPx );
	*outWLog = wLog;
	*outHLog = hLog;
}

/* ── accessors ────────────────────────────────────────────────────── */
arena_t *WiredUI_GetArena( void )
{
	return wui_arena;
}

/* DPI: physical/logical ratio (>=1.0; 1.0 on a non-HiDPI display or
 * before the compositor is initialised / the platform layer has published a
 * logical size). Consumed by the text-emit path in cl_wired_clay.c to scale
 * font sizes so glyphs render at a consistent physical size across displays.
 *
 * r_uiDpiScaleTest (default 0) forces a synthetic ratio so the DPI font-scaling
 * path can be verified on a non-HiDPI display (where the real ratio is 1.0 and
 * the multiply would be an unobservable identity). It is a test affordance —
 * the headless layout-check sets it to confirm glyph px == fontPointSize *
 * dpiScale with a non-unity factor — and is inert at its 0 default. */
float WiredUI_GetDpiScale( void )
{
	float testOverride = Cvar_VariableValue( "r_uiDpiScaleTest" );
	if ( testOverride > 0.0f ) {
		return testOverride;
	}
	if ( !wui_compositor_initialized || wui_windowRect.dpiScale <= 0.0f ) {
		return 1.0f;
	}
	return wui_windowRect.dpiScale;
}

/* ── lifecycle ────────────────────────────────────────────────────── */
void WiredUI_CompositorInit( void )
{
	size_t bytes;

	if ( wui_compositor_initialized ) {
		/* vid_restart / hot-reload re-entry: keep arena + Clay state alive.
		 * The legacy WiredUI_Init machinery may be re-run; the compositor
		 * stays through the cycle. */
		return;
	}

	/* Cvar — process-lifetime arena size in MB. CVAR_LATCH because changing
	 * the size means tearing down and recreating the arena (no live resize). */
	{
		static const cvarDesc_t d = CVAR_INT( "wired_ui_arena_mb", "32",
			CVAR_ARCHIVE | CVAR_LATCH,
			"WiredUI compositor arena size in MB (process-lifetime). "
			"Hosts Clay arena slice (8 MB), per-frame scratch arena for repeat-block expansion (1 MB), font indirection table, anim state.",
			4, 256 );
		wired_ui_arena_mb = Cvar_Register( &d );
	}

	bytes = (size_t) wired_ui_arena_mb->integer * 1024u * 1024u;
	wui_arena = Arena_Create( "WiredUI", bytes );
	/* Arena_Create Terminate()s on failure — no NULL check required. */

	Com_Log( SEV_INFO, LOG_CH(ch_ui),
		"WiredUI compositor: arena allocated (%zu MB, /meminfo visible)\n",
		Arena_Size( wui_arena ) / (1024u * 1024u) );

	/* Hand the arena to the Clay glue. It carves a 4 MB slice for Clay's
	 * own arena, calls Clay_Initialize + Clay_SetMeasureTextFunction, and
	 * populates the font indirection table from WiredUI's existing
	 * fontFace_t registry. */
	WiredUI_ClayInit( wui_arena );

	/* Window-rect seed. A later pass wires the publish path from platform
	 * resize hooks (SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED, Win32 WM_SIZE,
	 * GLimp_SetMode completion) and restores cls.glconfig.vidWidth to its
	 * canonical window-pixel meaning. For now we seed from the overloaded
	 * glConfig — same source the rest of the engine reads today, so the
	 * compositor is consistent with cl_scrn.c and cl_wired_ui.c through
	 * the migration cycle. */
	memset( &wui_windowRect, 0, sizeof( wui_windowRect ) );
	wui_windowRect.widthPx   = cls.glconfig.vidWidth  > 0 ? cls.glconfig.vidWidth  : 1280;
	wui_windowRect.heightPx  = cls.glconfig.vidHeight > 0 ? cls.glconfig.vidHeight : 720;
	/* DPI: logical point size comes from the platform layer
	 * (SDL_GetWindowSize → glconfig.vidWidthLogical), or the r_uiLogicalWidthTest
	 * seam. Falls back to the physical size when neither is set (dpiScale 1.0). */
	wui_logical_size( wui_windowRect.widthPx, wui_windowRect.heightPx,
	                  &wui_windowRect.widthLog, &wui_windowRect.heightLog );
	/* Take the VERTICAL ratio, not the horizontal one. The two only agree when
	 * the logical size keeps the physical aspect, and on macOS it often does
	 * not: a 1600x900 request backed by a 2880x1800 drawable gives scaleX 1.80
	 * against scaleY 2.00. Font size is one scalar, so feeding it the narrower
	 * ratio set glyphs 10% short of the pixel density they were rasterised
	 * into — text came out horizontally squeezed with MSDF sampled off its
	 * intended rate, which reads as furry edges. Type is laid out down the
	 * page and its rasterisation follows the vertical density, so that is the
	 * axis to track. */
	wui_windowRect.dpiScale  = wui_windowRect.heightLog > 0
	                         ? (float) wui_windowRect.heightPx / (float) wui_windowRect.heightLog
	                         : ( wui_windowRect.widthLog > 0
	                           ? (float) wui_windowRect.widthPx / (float) wui_windowRect.widthLog
	                           : 1.0f );
	wui_windowRect.generation = 0;
	wui_windowRect.valid     = qfalse;

	wui_compositor_initialized = qtrue;
}

void WiredUI_CompositorShutdown( void )
{
	if ( !wui_compositor_initialized ) {
		return;
	}

	WiredUI_ClayShutdown();

	if ( wui_arena ) {
		Arena_Destroy( wui_arena );
		wui_arena = NULL;
	}

	memset( &wui_windowRect, 0, sizeof( wui_windowRect ) );
	wui_compositor_initialized = qfalse;
}

/* ── per-frame tick (stub) ────────────────────────────────── */
void WiredUI_CompositorFrame( int realtimeMs )
{
	int wPx, hPx;

	(void) realtimeMs;

	if ( !wui_compositor_initialized ) {
		return;
	}

	/* Window-pixel size — same source the rest of the engine reads today
	 * (overloaded glConfig). A later pass swaps this to the canonical value
	 * published from the platform-layer resize hooks. */
	wPx = cls.glconfig.vidWidth  > 0 ? cls.glconfig.vidWidth  : wui_windowRect.widthPx;
	hPx = cls.glconfig.vidHeight > 0 ? cls.glconfig.vidHeight : wui_windowRect.heightPx;

	{
		/* DPI: logical point size from the platform layer (or the
		 * r_uiLogicalWidthTest seam); falls back to physical (dpiScale 1.0). */
		int wLog, hLog;
		wui_logical_size( wPx, hPx, &wLog, &hLog );

		if ( wPx != wui_windowRect.widthPx || hPx != wui_windowRect.heightPx
		  || wLog != wui_windowRect.widthLog || hLog != wui_windowRect.heightLog ) {
			wui_windowRect.widthPx   = wPx;
			wui_windowRect.heightPx  = hPx;
			wui_windowRect.widthLog  = wLog;
			wui_windowRect.heightLog = hLog;
			/* Vertical ratio — see the rationale at the init-time assignment.
			 * This is the live path that runs every frame, so a fix applied
			 * only at init would be overwritten on the first resize. */
			wui_windowRect.dpiScale  = hLog > 0 ? (float) hPx / (float) hLog
			                         : ( wLog > 0 ? (float) wPx / (float) wLog : 1.0f );
			wui_windowRect.generation++;
		}
	}

	WiredUI_ClayFrame( wPx, hPx );

	/* The per-frame Clay tree walk + render-command
	 * dispatch lives in SCR_DrawScreenField (cl_scrn.c), NOT here. That is
	 * because re.* calls from the backend (DrawStretchPic, SetColor,
	 * SetClipRegion, MSDF_DrawString) are only valid between re.BeginFrame
	 * and re.EndFrame, and SCR_DrawScreenField is the function that
	 * brackets those. CL_Frame fires before re.BeginFrame, so dispatching
	 * here would Access-Violate the renderer (NVIDIA GL driver was the
	 * first to catch it during the compositor implementation). A later pass retires
	 * SCR_DrawScreenField; the dispatch then moves to a clean compositor-
	 * owned per-frame slot inside the renderer scope. */
}

#include "cl_wired_ui.h"
#include "cl_wired_customdraw.h"   /* dev cmd register/unregister */
#include "../store/cl_wired_store.h"
#include "../../../qcommon/wired/core/scripting/wired_scripting.h"
#include "../../../qcommon/wired/core/scripting/user_vm.h"
#include <lua.h>
#include <lauxlib.h>
#include <math.h>

/* ───────────────────────────────────────────────────────────────────
 * popup queue
 * ─────────────────────────────────────────────────────────────────── */

#define WUI_POPUP_QUEUE_MAX 8

typedef struct {
	char     menuName[ 64 ];
	qboolean active;
	int      pushTimeMs;
} wui_popup_slot_t;

static wui_popup_slot_t wui_popup_queue[ WUI_POPUP_QUEUE_MAX ];
static int              wui_popup_head = 0;   /* dismiss pops from head */
static int              wui_popup_tail = 0;   /* push appends at tail */
static int              wui_popup_count = 0;

void WiredUI_PushPopup( const char *menuName )
{
	if ( !menuName || !*menuName ) return;
	if ( wui_popup_count >= WUI_POPUP_QUEUE_MAX ) {
		Com_Log( SEV_WARN, LOG_CH(ch_ui),
			"WiredUI/popup: queue full (max %d) — dropping push of '%s'\n",
			WUI_POPUP_QUEUE_MAX, menuName );
		return;
	}
	Q_strncpyz( wui_popup_queue[ wui_popup_tail ].menuName, menuName,
		sizeof( wui_popup_queue[ wui_popup_tail ].menuName ) );
	wui_popup_queue[ wui_popup_tail ].active     = qtrue;
	wui_popup_queue[ wui_popup_tail ].pushTimeMs = Sys_Milliseconds();
	wui_popup_tail = ( wui_popup_tail + 1 ) % WUI_POPUP_QUEUE_MAX;
	wui_popup_count++;
}

void WiredUI_DismissPopup( void )
{
	if ( wui_popup_count == 0 ) return;
	wui_popup_queue[ wui_popup_head ].active = qfalse;
	wui_popup_queue[ wui_popup_head ].menuName[ 0 ] = '\0';
	wui_popup_head = ( wui_popup_head + 1 ) % WUI_POPUP_QUEUE_MAX;
	wui_popup_count--;
}

qboolean WiredUI_HasActivePopup( void )
{
	return wui_popup_count > 0;
}

const char *WiredUI_FrontPopupName( void )
{
	if ( wui_popup_count == 0 ) return NULL;
	return wui_popup_queue[ wui_popup_head ].menuName;
}

/* ───────────────────────────────────────────────────────────────────
 * layer force-override mask
 *
 * Bit N set means layer enum value N is force-active regardless of
 * cls.state. wui_layer_active() (cl_wired_clay.c) consults this BEFORE
 * the production gating switch via WiredUI_LayerForceOverrideTest;
 * when the mask is 0 (production default) the predicate's behaviour is
 * byte-identical to the prior behaviour — zero cost on the production path
 * (single AND against zero + branch).
 *
 * Owned by the dev commands wui_layer_test_single / _stack / _clear.
 * No cvar surface (Memory: no diagnostic cvars).
 * ─────────────────────────────────────────────────────────────────── */

static uint32_t wui_layer_force_override_mask  = 0;

/* anomaly 11A polish (Path P, ratified 2026-05-23): exclusive
 * mode toggle. When set alongside a non-zero override mask, wui_layer_active
 * suppresses layers NOT in the mask (instead of falling through to the
 * production cls.state gating for unmasked layers). Drives the
 * `wui_layer_test_single <layer> exclusive` and `_stack exclusive` dev
 * command variants — additive (no `exclusive` keyword) preserves
 * backward-compatible behavior. */
static qboolean wui_layer_force_exclusive_mode = qfalse;

qboolean WiredUI_LayerForceOverrideTest( int layer )
{
	if ( layer < 0 || layer >= 32 ) return qfalse;
	return ( wui_layer_force_override_mask & ( 1U << (uint32_t) layer ) )
	     ? qtrue : qfalse;
}

/* ── per-layer state ──────────────────────────────────────────────────
 *
 * One record per layer, recomputed once per frame at the top of the emit walk.
 * Nothing here is written at a transition; the values are answers to "what is
 * true right now", which is what makes a missed transition impossible to
 * express. The background family gets its answer from the stack-top menu's
 * preset (policy/wui_bg_preset.h); every other layer keeps its own
 * connection-state predicate.
 *
 * `clockMs` advances only while a layer is visible and unpaused, so a hidden
 * backdrop genuinely stops rather than merely being skipped at draw time —
 * that is the difference between saving the draw and saving the work. */
typedef struct {
	qboolean visible;
	qboolean paused;
	int      clockMs;      /* accumulated running time, ms */
	int      lastTickMs;   /* cls.realtime at the previous advance */
} wuiLayerState_t;

static wuiLayerState_t wui_layer_state[ WUI_LAYER_COUNT ];

void WiredUI_LayerStateSet( int layer, qboolean visible, qboolean paused )
{
	wuiLayerState_t *st;

	if ( layer < 0 || layer >= WUI_LAYER_COUNT ) return;
	st = &wui_layer_state[ layer ];

	/* Advance the clock for the interval that just elapsed, using the state
	 * it was in during that interval — then adopt the new state. Sampling
	 * after the change would credit or skip a frame at every transition. */
	if ( st->lastTickMs > 0 && st->visible && !st->paused ) {
		int delta = cls.realtime - st->lastTickMs;
		if ( delta > 0 && delta < 1000 )   /* ignore hitches and rewinds */
			st->clockMs += delta;
	}
	st->lastTickMs = cls.realtime;
	st->visible    = visible;
	st->paused     = paused;
}

qboolean WiredUI_LayerVisible( int layer )
{
	if ( layer < 0 || layer >= WUI_LAYER_COUNT ) return qfalse;
	return wui_layer_state[ layer ].visible;
}

qboolean WiredUI_LayerPaused( int layer )
{
	if ( layer < 0 || layer >= WUI_LAYER_COUNT ) return qfalse;
	return wui_layer_state[ layer ].paused;
}

/* Milliseconds this layer has actually been running. Content animated off this
 * instead of cls.realtime freezes when the layer is paused and resumes where
 * it left off, rather than jumping to wherever wall-clock got to meanwhile. */
int WiredUI_LayerClockMs( int layer )
{
	if ( layer < 0 || layer >= WUI_LAYER_COUNT ) return 0;
	return wui_layer_state[ layer ].clockMs;
}

/* Whole-mask accessor — used by the SCR_DrawScreenField CA_DISCONNECTED
 * carve-out. Production default mask=0 makes the carve-out
 * a no-op; when any dev-command bit is set, SCR skips the legacy main-menu
 * force-set + UI_CALL_REFRESH so the compositor's emit becomes visible.
 * Half-life: deletes alongside SCR_DrawScreenField in a later pass. */
uint32_t WiredUI_LayerForceOverrideMask( void )
{
	return wui_layer_force_override_mask;
}

/* Exclusive-mode accessor — used by wui_layer_active (cl_wired_clay.c) to
 * decide whether unmasked layers fall through to production gating
 * (additive, default) or are unconditionally suppressed (exclusive).
 * Returns qfalse in production (mask defaults to 0 + exclusive defaults
 * to qfalse → predicate behaves identically to current implementation). */
qboolean WiredUI_LayerForceExclusiveMode( void )
{
	return wui_layer_force_exclusive_mode;
}

/* ───────────────────────────────────────────────────────────────────
 * animation module (narrow v1)
 * ─────────────────────────────────────────────────────────────────── */

#define WUI_TRANSITION_POOL_SIZE 64

typedef struct {
	qboolean    active;
	char        storeKey[ 128 ];
	float       startValue;
	float       endValue;
	int         startTimeMs;
	int         durationMs;
	wuiEasing_t ease;
} wui_transition_slot_t;

static wui_transition_slot_t wui_transitions[ WUI_TRANSITION_POOL_SIZE ];
static int                   wui_transition_active_count = 0;
static qboolean              wui_transition_pool_full_warned = qfalse;

/* Easing function — t ∈ [0,1] → eased fraction ∈ [0,1]. */
static float wui_ease_apply( wuiEasing_t ease, float t )
{
	if ( t <= 0.0f ) return 0.0f;
	if ( t >= 1.0f ) return 1.0f;
	switch ( ease ) {
	case WUI_EASE_LINEAR:    return t;
	case WUI_EASE_IN:        return t * t;
	case WUI_EASE_OUT:       return 1.0f - ( 1.0f - t ) * ( 1.0f - t );
	case WUI_EASE_IN_OUT: {
		if ( t < 0.5f ) return 2.0f * t * t;
		{ float u = -2.0f * t + 2.0f;
		  return 1.0f - ( u * u ) * 0.5f; }
	}
	default:                 return t;
	}
}

int Anim_EaseFromString( const char *name )
{
	if ( !name ) return WUI_EASE_UNKNOWN;
	if ( !Q_stricmp( name, "linear" ) )       return WUI_EASE_LINEAR;
	if ( !Q_stricmp( name, "ease-in" ) )      return WUI_EASE_IN;
	if ( !Q_stricmp( name, "ease-out" ) )     return WUI_EASE_OUT;
	if ( !Q_stricmp( name, "ease-in-out" ) )  return WUI_EASE_IN_OUT;
	return WUI_EASE_UNKNOWN;
}

int Anim_StartTween( const char *storeKey, float endValue, int durationMs, wuiEasing_t ease )
{
	int i;
	wuiStoreEntry_t *e;
	int             nowMs;

	if ( !storeKey || !*storeKey ) return 0;
	if ( durationMs <= 0 ) durationMs = 1;
	if ( ease != WUI_EASE_LINEAR && ease != WUI_EASE_IN
	  && ease != WUI_EASE_OUT && ease != WUI_EASE_IN_OUT ) {
		ease = WUI_EASE_LINEAR;
	}

	/* Find a free slot. */
	for ( i = 0; i < WUI_TRANSITION_POOL_SIZE; i++ ) {
		if ( !wui_transitions[ i ].active ) break;
	}
	if ( i >= WUI_TRANSITION_POOL_SIZE ) {
		if ( !wui_transition_pool_full_warned ) {
			Com_Log( SEV_WARN, LOG_CH(ch_ui),
				"WiredUI/anim: transition pool full (max %d) — tween for '%s' dropped\n",
				WUI_TRANSITION_POOL_SIZE, storeKey );
			wui_transition_pool_full_warned = qtrue;
		}
		return 0;
	}

	/* Seed startValue from current store value (0 if absent). */
	e = WiredStore_Get( storeKey );
	wui_transitions[ i ].startValue   = e ? e->value : 0.0f;
	wui_transitions[ i ].endValue     = endValue;
	wui_transitions[ i ].ease         = ease;
	wui_transitions[ i ].durationMs   = durationMs;
	nowMs = Sys_Milliseconds();
	wui_transitions[ i ].startTimeMs  = nowMs;
	Q_strncpyz( wui_transitions[ i ].storeKey, storeKey, sizeof( wui_transitions[ i ].storeKey ) );
	wui_transitions[ i ].active       = qtrue;
	wui_transition_active_count++;
	wui_transition_pool_full_warned   = qfalse;   /* reset latch on success */

	return i + 1;   /* handle = slot index + 1 (0 reserved for error) */
}

void Anim_Cancel( int handle )
{
	int idx;
	if ( handle <= 0 || handle > WUI_TRANSITION_POOL_SIZE ) return;
	idx = handle - 1;
	if ( !wui_transitions[ idx ].active ) return;
	wui_transitions[ idx ].active = qfalse;
	wui_transition_active_count--;
}

void Anim_FrameUpdate( int nowMs )
{
	int i;
	for ( i = 0; i < WUI_TRANSITION_POOL_SIZE; i++ ) {
		wui_transition_slot_t *s = &wui_transitions[ i ];
		float t;
		float eased;
		float value;
		wuiStoreEntry_t *e;

		if ( !s->active ) continue;

		/* t = elapsed / duration, both in ms. (The former elapsedSec = ms/1000
		 * then *1000 round-trip cancelled out and only lost precision.) */
		t = (float)( nowMs - s->startTimeMs ) / (float) s->durationMs;
		if ( t >= 1.0f ) t = 1.0f;
		eased = wui_ease_apply( s->ease, t );
		value = s->startValue + ( s->endValue - s->startValue ) * eased;

		/* Write back to the store. WiredStore_Set returns the entry; we
		 * write the .value field. Existing bind="storekey" path picks up
		 * the change next frame via the store-dirty pattern. */
		e = WiredStore_Set( s->storeKey );
		if ( e ) e->value = value;

		if ( t >= 1.0f ) {
			s->active = qfalse;
			wui_transition_active_count--;
		}
	}
}

/* ───────────────────────────────────────────────────────────────────
 * anim.tween Lua binding
 *
 * Registered in BOTH System VM and User VM. Signature in Lua:
 *
 *   handle = anim.tween( storeKey, endValue, durationMs [, easeName] )
 *
 * Returns the integer handle on success, or nil on error (invalid args,
 * pool full, unknown ease name). easeName defaults to "linear" when
 * omitted. Errors logged once via SEV_WARN.
 * ─────────────────────────────────────────────────────────────────── */

static int wui_lua_anim_tween( lua_State *L )
{
	const char *storeKey = luaL_checkstring( L, 1 );
	float       endValue = (float) luaL_checknumber( L, 2 );
	int         durationMs = (int) luaL_checkinteger( L, 3 );
	const char *easeName = ( lua_gettop( L ) >= 4 && !lua_isnil( L, 4 ) )
	                     ? luaL_checkstring( L, 4 ) : "linear";
	int         easeOrErr;
	int         handle;

	easeOrErr = Anim_EaseFromString( easeName );
	if ( easeOrErr == WUI_EASE_UNKNOWN ) {
		static qboolean s_warned = qfalse;
		if ( !s_warned ) {
			Com_Log( SEV_WARN, LOG_CH(ch_ui),
				"anim.tween: unknown ease name '%s' (valid: linear, ease-in, ease-out, ease-in-out); falling back to linear\n",
				easeName );
			s_warned = qtrue;
		}
		easeOrErr = WUI_EASE_LINEAR;
	}

	handle = Anim_StartTween( storeKey, endValue, durationMs, (wuiEasing_t) easeOrErr );
	if ( handle == 0 ) {
		lua_pushnil( L );
		return 1;
	}

	lua_pushinteger( L, handle );
	return 1;
}

static int wui_lua_anim_cancel( lua_State *L )
{
	int handle = (int) luaL_checkinteger( L, 1 );
	Anim_Cancel( handle );
	return 0;
}

static const luaL_Reg wui_anim_lib[] = {
	{ "tween",  wui_lua_anim_tween },
	{ "cancel", wui_lua_anim_cancel },
	{ NULL, NULL }
};

static void wui_anim_register_for_lua( lua_State *L )
{
	if ( !L ) return;
	luaL_newlib( L, wui_anim_lib );
	lua_setglobal( L, "anim" );
}

void WiredAnimLua_Init( void )
{
	/* Register the same binding set in BOTH VMs (dispatcher
	 * symmetry). System VM panels and User VM panels alike can call
	 * anim.tween; the animation pool is engine-side and shared. */
	WiredScript_RegisterBindings( wui_anim_register_for_lua );
	UserVM_RegisterBindings     ( wui_anim_register_for_lua );
}

/* ───────────────────────────────────────────────────────────────────
 * dev commands — kept in tree as engineering utilities
 *
 * Migrated from `developer` cvar gating (Memory
 * K16 retired-cvar) to `#ifdef _DEBUG` compile-time gating.
 * Release builds strip the handlers + their Cmd_Add/Remove
 * registrations entirely; DEBUG builds expose the commands without
 * needing any runtime cvar flip.
 * ─────────────────────────────────────────────────────────────────── */

#ifdef _DEBUG

static void WiredUI_PopupTest_f( void )
{
	const char *name;
	if ( Cmd_Argc() < 2 ) {
		Com_Log( SEV_INFO, LOG_CH(ch_ui),
			"Usage: wui_popup_test <menuName>\n"
			"       wui_popup_test --dismiss\n" );
		return;
	}
	name = Cmd_Argv( 1 );
	if ( !Q_stricmp( name, "--dismiss" ) || !Q_stricmp( name, "-d" ) ) {
		WiredUI_DismissPopup();
		Com_Log( SEV_INFO, LOG_CH(ch_ui),
			"wui_popup_test: dismissed front popup (count=%s)\n",
			WiredUI_HasActivePopup() ? "non-zero" : "0" );
		return;
	}
	WiredUI_PushPopup( name );
	Com_Log( SEV_INFO, LOG_CH(ch_ui),
		"wui_popup_test: pushed '%s' (front='%s')\n",
		name,
		WiredUI_FrontPopupName() ? WiredUI_FrontPopupName() : "(empty)" );
}

static void WiredUI_AnimTest_f( void )
{
	const char *key;
	float       endValue;
	int         durationMs;
	const char *easeName = "linear";
	int         ease;
	int         handle;

	if ( Cmd_Argc() < 4 ) {
		Com_Log( SEV_INFO, LOG_CH(ch_ui),
			"Usage: wui_anim_test <storeKey> <endValue> <durationMs> [ease]\n"
			"  ease : linear (default) | ease-in | ease-out | ease-in-out\n" );
		return;
	}
	key        = Cmd_Argv( 1 );
	endValue   = atof( Cmd_Argv( 2 ) );
	durationMs = atoi( Cmd_Argv( 3 ) );
	if ( Cmd_Argc() >= 5 ) easeName = Cmd_Argv( 4 );

	ease = Anim_EaseFromString( easeName );
	if ( ease == WUI_EASE_UNKNOWN ) {
		Com_Log( SEV_WARN, LOG_CH(ch_ui),
			"wui_anim_test: unknown ease '%s' (using linear)\n", easeName );
		ease = WUI_EASE_LINEAR;
	}

	handle = Anim_StartTween( key, endValue, durationMs, (wuiEasing_t) ease );
	Com_Log( SEV_INFO, LOG_CH(ch_ui),
		"wui_anim_test: tween('%s' → %.3f, %d ms, %s) → handle=%d\n",
		key, endValue, durationMs, easeName, handle );
}

/* ───────────────────────────────────────────────────────────────────
 * visual z-order verification harness
 *
 * Three dev commands that drive the 6 layer fixtures in
 * base/scripts/wmenu/test/layer_*.wmenu. The fixtures load lazily on
 * first invocation; the force-override mask above gets flipped to
 * activate the requested layer(s) regardless of cls.state.
 *
 * Layer activation paths:
 *   bg_attract / loading / hud / debug_overlay : multi-panel walk picks
 *     up the fixture via menu->layer + .visible once the override mask
 *     bit is set. No explicit menu push.
 *   menu_stack : MENU_STACK emission uses WiredUI_GetActiveMenu(), so
 *     the command does an explicit WiredUI_PushMenu in addition to
 *     setting the bit.
 *   popup      : POPUP emission uses WiredUI_FrontPopupName(), so the
 *     command does an explicit WiredUI_PushPopup in addition to setting
 *     the bit.
 *
 * Bookkeeping for wui_layer_test_clear: tracks whether menu_stack /
 * popup fixtures were pushed by *this harness*, so clear can dismiss
 * them symmetrically without touching unrelated stack/popup state.
 * ─────────────────────────────────────────────────────────────────── */

typedef struct {
	const char *name;        /* layer keyword as accepted by parser */
	int         enumValue;   /* wuiLayer_t enum value */
	const char *fixtureName; /* menuDef name authored in the fixture */
	const char *fixturePath; /* path passed to WiredUI_LoadMenuFile */
} wui_layer_fixture_t;

static const wui_layer_fixture_t wui_layer_fixtures[] = {
	{ "bg_attract",     WUI_LAYER_BG_ATTRACT,     "layer_bg_attract",     "scripts/wmenu/test/layer_bg_attract.wui"     },
	{ "loading",        WUI_LAYER_LOADING,        "layer_loading",        "scripts/wmenu/test/layer_loading.wui"        },
	{ "world_viewport", WUI_LAYER_WORLD_VIEWPORT, "layer_world_viewport", "scripts/wmenu/test/layer_world_viewport.wui" },
	{ "hud",            WUI_LAYER_HUD,            "layer_hud",            "scripts/wmenu/test/layer_hud.wui"            },
	{ "menu",           WUI_LAYER_MENU,           "layer_menu",           "scripts/wmenu/test/layer_menu.wui"           },
	{ "popup",          WUI_LAYER_POPUP,          "layer_popup",          "scripts/wmenu/test/layer_popup.wui"          },
	{ "debug_overlay",  WUI_LAYER_DEBUG_OVERLAY,  "layer_debug_overlay",  "scripts/wmenu/test/layer_debug_overlay.wui"  },
	{ "overlay",        WUI_LAYER_OVERLAY,        "overlay",              "ui/overlay.wui"                              },
	{ "console",        WUI_LAYER_CONSOLE,        "layer_console",        "scripts/wmenu/test/layer_console.wui"        },
};

/* Tracks harness-pushed state so wui_layer_test_clear can dismiss only
 * what the harness itself pushed. */
static qboolean wui_layer_test_menu_pushed = qfalse;
static qboolean wui_layer_test_popup_pushed     = qfalse;

static void wui_layer_test_ensure_loaded( const wui_layer_fixture_t *f )
{
	if ( !f ) return;
	if ( WiredUI_FindMenu( f->fixtureName ) ) return;
	if ( !WiredUI_LoadMenuFile( f->fixturePath ) ) {
		Com_Log( SEV_WARN, LOG_CH(ch_ui),
			"wui_layer_test: failed to load fixture '%s'\n", f->fixturePath );
	}
}

static const wui_layer_fixture_t *wui_layer_test_lookup( const char *name )
{
	size_t i;
	if ( !name ) return NULL;
	for ( i = 0; i < ARRAY_LEN( wui_layer_fixtures ); i++ ) {
		if ( !Q_stricmp( name, wui_layer_fixtures[ i ].name ) ) {
			return &wui_layer_fixtures[ i ];
		}
	}
	return NULL;
}

static void wui_layer_test_activate_one( const wui_layer_fixture_t *f )
{
	/* Flip the bit BEFORE pushing so any same-frame emit pass sees the
	 * override + the freshly pushed menu_stack/popup state together. */
	wui_layer_force_override_mask |= ( 1U << (uint32_t) f->enumValue );

	if ( f->enumValue == WUI_LAYER_MENU ) {
		WiredUI_PushMenu( f->fixtureName, WUI_BG_INTENT_INHERIT );
		wui_layer_test_menu_pushed = qtrue;
	} else if ( f->enumValue == WUI_LAYER_POPUP ) {
		WiredUI_PushPopup( f->fixtureName );
		wui_layer_test_popup_pushed = qtrue;
	}
	/* Other 4 layers: multi-panel walk in WiredUI_CompositorEmitFrame
	 * picks the fixture up automatically via menu->layer matching. */
}

/* anomaly 11B polish (Path S, ratified 2026-05-23): unconditional
 * menu-state reset on every dev-command entry. Clears the legacy stack +
 * wui_activeMenu (set by SCR_DrawScreenField's CA_DISCONNECTED branch
 * during boot before the carve-out engages) so the compositor's natural
 * MENU_STACK layer activation rule no longer fires on a leaked "main".
 * Popup queue is separate from the menu stack — dismiss the harness-pushed
 * one too. Returns the pop count for transparency in the log marker.
 *
 * Unconditional (fires in additive AND exclusive modes) — backward-compat
 * shift relative to pre-polish behavior is documented in the report. */
static int wui_layer_test_reset_menu_state( void )
{
	int popCount = WiredUI_GetMenuStackDepth();
	WiredUI_CloseAllMenus();              /* clears stack + wui_activeMenu  */
	wui_layer_test_menu_pushed = qfalse;  /* tracking out of sync now */
	if ( wui_layer_test_popup_pushed ) {
		WiredUI_DismissPopup();
		wui_layer_test_popup_pushed = qfalse;
	}
	return popCount;
}

/* Returns qtrue iff Cmd_Argv(argIdx) is the case-insensitive "exclusive"
 * keyword (the anomaly 11A polish opt-in for layer-isolation
 * mode). Absence (or any other token) → additive, backward-compatible. */
static qboolean wui_layer_test_parse_exclusive( int argIdx )
{
	if ( Cmd_Argc() <= argIdx ) return qfalse;
	return ( !Q_stricmp( Cmd_Argv( argIdx ), "exclusive" ) ) ? qtrue : qfalse;
}

static void WiredUI_LayerTestSingle_f( void )
{
	const wui_layer_fixture_t *f;
	const char                *name;
	qboolean                   exclusive;
	int                        popped;

	if ( Cmd_Argc() < 2 ) {
		Com_Log( SEV_INFO, LOG_CH(ch_ui),
			"Usage: wui_layer_test_single <bg_attract|loading|world_viewport|hud|menu|popup|debug_overlay|overlay|console> [exclusive]\n" );
		return;
	}
	name = Cmd_Argv( 1 );
	f = wui_layer_test_lookup( name );
	if ( !f ) {
		Com_Log( SEV_WARN, LOG_CH(ch_ui),
			"wui_layer_test_single: unknown layer \"%s\"\n", name );
		return;
	}
	exclusive = wui_layer_test_parse_exclusive( 2 );

	/* Path S — wipe legacy menu state before activating the fixture so the
	 * compositor doesn't pick up a leaked "main" through MENU_STACK's natural
	 * activation rule. */
	popped = wui_layer_test_reset_menu_state();

	/* Path P — set/clear exclusive flag from arg; mask reset is single-layer
	 * mode's existing exclusivity inside the harness command set (one bit
	 * at a time). */
	wui_layer_force_exclusive_mode = exclusive;
	wui_layer_force_override_mask  = 0;

	wui_layer_test_ensure_loaded( f );
	wui_layer_test_activate_one( f );

	Com_Log( SEV_INFO, LOG_CH(ch_ui),
		"wui_layer_test_single: %s activated (popped %d, override mask=0x%X, exclusive=%d)\n",
		f->name, popped, wui_layer_force_override_mask, (int) exclusive );
}

static void WiredUI_LayerTestStack_f( void )
{
	size_t   i;
	qboolean exclusive;
	int      popped;

	exclusive = wui_layer_test_parse_exclusive( 1 );

	/* Path S — wipe legacy menu state. */
	popped = wui_layer_test_reset_menu_state();

	/* Path P — exclusive flag + mask reset. */
	wui_layer_force_exclusive_mode = exclusive;
	wui_layer_force_override_mask  = 0;

	/* Ensure all fixtures loaded. */
	for ( i = 0; i < ARRAY_LEN( wui_layer_fixtures ); i++ ) {
		wui_layer_test_ensure_loaded( &wui_layer_fixtures[ i ] );
	}

	/* Set all bits — use the WUI_LAYER_COUNT sentinel so the mask
	 * auto-tracks any future layer additions. */
	wui_layer_force_override_mask = ( 1U << (uint32_t) WUI_LAYER_COUNT ) - 1U;

	/* Activate each fixture in BG_ATTRACT → DEBUG_OVERLAY order. */
	for ( i = 0; i < ARRAY_LEN( wui_layer_fixtures ); i++ ) {
		const wui_layer_fixture_t *f = &wui_layer_fixtures[ i ];

		if ( f->enumValue == WUI_LAYER_MENU ) {
			WiredUI_PushMenu( f->fixtureName, WUI_BG_INTENT_INHERIT );
			wui_layer_test_menu_pushed = qtrue;
		} else if ( f->enumValue == WUI_LAYER_POPUP ) {
			WiredUI_PushPopup( f->fixtureName );
			wui_layer_test_popup_pushed = qtrue;
		}
		Com_Log( SEV_INFO, LOG_CH(ch_ui),
			"wui_layer_test_stack: layer %d (%s) activated\n",
			(int) f->enumValue, f->name );
	}

	Com_Log( SEV_INFO, LOG_CH(ch_ui),
		"wui_layer_test_stack: %d layers active (popped %d, override mask=0x%X, exclusive=%d), capture with screenshot\n",
		(int) WUI_LAYER_COUNT, popped, wui_layer_force_override_mask, (int) exclusive );
}

static void WiredUI_LayerTestClear_f( void )
{
	int popped;

	popped = wui_layer_test_reset_menu_state();
	wui_layer_force_override_mask  = 0;
	wui_layer_force_exclusive_mode = qfalse;

	Com_Log( SEV_INFO, LOG_CH(ch_ui),
		"wui_layer_test_clear: popped %d menus from stack, override mask cleared, exclusive mode cleared\n",
		popped );
}

#endif /* _DEBUG — dev-command compile-time gate */

void WiredUI_CompositorRegisterDevCommands( void )
{
#ifdef _DEBUG
	Cmd_AddCommand( "wui_popup_test",       WiredUI_PopupTest_f );
	Cmd_AddCommand( "wui_anim_test",        WiredUI_AnimTest_f );
	Cmd_AddCommand( "wui_layer_test_single", WiredUI_LayerTestSingle_f );
	Cmd_AddCommand( "wui_layer_test_stack",  WiredUI_LayerTestStack_f );
	Cmd_AddCommand( "wui_layer_test_clear",  WiredUI_LayerTestClear_f );
#endif
	/* Unified custom-draw registry diagnostic. Kept
	 * unconditional for now; the custom-draw registry callers gate
	 * their own dev paths internally. */
	WiredUI_CustomDraw_RegisterDevCommands();
}

void WiredUI_CompositorUnregisterDevCommands( void )
{
#ifdef _DEBUG
	Cmd_RemoveCommand( "wui_popup_test" );
	Cmd_RemoveCommand( "wui_anim_test" );
	Cmd_RemoveCommand( "wui_layer_test_single" );
	Cmd_RemoveCommand( "wui_layer_test_stack" );
	Cmd_RemoveCommand( "wui_layer_test_clear" );
#endif

	WiredUI_CustomDraw_UnregisterDevCommands();
}

#endif /* FEAT_WIRED_UI */
