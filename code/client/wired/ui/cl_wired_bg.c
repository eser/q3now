// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
cl_wired_bg.c — Wired UI 6-layered background extension.

All six layers emit as Clay primitives so the existing dispatch (rect /
border / image) handles them without renderer-side additions. Layer
shapes:
  base        — solid colour fill (token $bg)
  grid        — double-stride cyan grid (120 px major + 24 px minor),
                breathing drift offset driven by a LOOP_LINEAR anim
  scanlines   — single 2 px horizontal cyan line sweeping top-to-bottom
                via a LOOP_LINEAR anim
  glow_rays   — 4 corner radial-feel rectangles tinted $primary_cyan
  noise       — sparse cyan dots distributed pseudo-randomly
  vignette    — 4 corner radius-quad darken pieces

Painter's order matches WUI_BG_LAYER_* bitmask order so the bottom
layer (base) lands first and vignette wins on top.
*/

#include "../../client.h"
#include "cl_wired_bg.h"
#include "cl_wired_anim.h"

#if FEAT_WIRED_UI

#include "../../../qcommon/q_shared.h"
#include "clay.h"
#include <math.h>
#include <stdio.h>     /* sscanf for token colour parsing */

LOG_DECLARE_CHANNEL( ch_ui, "ui" );

/* Live token table lookup. Defined in cl_wired_parse.c; the v2 design
 * palette overlay (15.2 cl_wired_palette + themes/<mode>/<accent>
 * _tokens.wui chain) rewrites these entries on cvar change, so reading
 * at emit time picks up the active mode×accent values per frame. */
extern const char *WiredToken_Find( const char *name );

/* Parse a token value string into a Clay_Color. Accepts:
 *   "#rrggbb"             — opaque hex
 *   "#rrggbbaa"           — hex with alpha byte
 *   "rgba(r,g,b,a)"       — decimal triple + 0..1 float alpha
 * Returns qfalse on unrecognised form so callers fall back to a
 * built-in default. */
static qboolean wui_bg_parse_color( const char *value, Clay_Color *out ) {
	unsigned r = 0, g = 0, b = 0, a = 255;
	float    af = 1.0f;
	int      n;
	if ( !value || !*value || !out ) return qfalse;
	if ( value[0] == '#' ) {
		n = sscanf( value + 1, "%2x%2x%2x%2x", &r, &g, &b, &a );
		if ( n < 3 ) return qfalse;
		out->r = (uint8_t) r;
		out->g = (uint8_t) g;
		out->b = (uint8_t) b;
		out->a = ( n == 4 ) ? (uint8_t) a : 255;
		return qtrue;
	}
	if ( !Q_stricmpn( value, "rgba(", 5 ) ) {
		n = sscanf( value + 5, "%u , %u , %u , %f", &r, &g, &b, &af );
		if ( n == 4 ) {
			out->r = (uint8_t) r;
			out->g = (uint8_t) g;
			out->b = (uint8_t) b;
			out->a = (uint8_t) ( af * 255.0f );
			return qtrue;
		}
	}
	return qfalse;
}

/* Resolve a palette token into a Clay_Color, falling back to a baked
 * default when the token is missing or unparseable. The wrapper keeps
 * the per-renderer site readable (`wui_bg_resolve("demoSky1", FALLBACK)`)
 * and the alpha-with-multiplier pattern works because the result is
 * passed straight into wui_bg_emit_rect. */
static Clay_Color wui_bg_resolve( const char *name, Clay_Color fallback ) {
	const char *value = WiredToken_Find( name );
	Clay_Color  out;
	if ( value && wui_bg_parse_color( value, &out ) ) return out;
	return fallback;
}

/* Stride values intentionally coarse so the ambient pattern density
 * stays low enough for foreground UI text to read across all target
 * resolutions. The grid emits a double pass: a major stride for the
 * primary weave, a minor stride at one-fifth the size for fine
 * detail. */
#define WUI_BG_GRID_STRIDE_MAJOR_PX  120
#define WUI_BG_GRID_STRIDE_MINOR_PX   24
#define WUI_BG_GRID_THICK_PX           1
#define WUI_BG_SCANLINE_THICK_PX       2
#define WUI_BG_GLOW_CORNER_FRACTION    0.22f   /* fraction of min(w,h) */
#define WUI_BG_NOISE_DOT_COUNT        64
#define WUI_BG_VIGNETTE_FRACTION       0.30f   /* corner quad reach toward centre */

/* Drift amplitude as fraction of min(w,h). Keeps motion subtle at every
 * viewport size; clamped to ±1.5%. */
#define WUI_BG_GRID_DRIFT_FRACTION     0.015f
#define WUI_BG_GRID_DRIFT_DURATION_MS  20000
#define WUI_BG_SCANLINE_DURATION_MS     5000

// ── Canonical layer colours ─────────────────────────────────────────
/* Layer alphas kept low so foreground cyan/white text reads cleanly
 * over the composed background even when grid + scanlines are both
 * enabled. The major grid carries the visible weave; the minor grid
 * is a fine high-frequency overlay that adds texture without
 * dominating. Scanline alpha is bumped relative to the legacy
 * many-rows implementation since a single sweeping line is far less
 * visually dense than a static stripe field. */
static const Clay_Color WUI_BG_COLOR_BASE        = {   8,  12,  16, 255 };  /* #080c10 — opaque base */
static const Clay_Color WUI_BG_COLOR_GRID_MAJOR  = {   0, 180, 216,  12 };  /* primary_cyan @ ~5% */
static const Clay_Color WUI_BG_COLOR_GRID_MINOR  = {   0, 180, 216,   0 };  /* minor stride disabled — single weave reads cleaner than overlapped double stride */
static const Clay_Color WUI_BG_COLOR_SCANLINES   = {   0, 180, 216,  20 };  /* primary_cyan @ ~8% */
static const Clay_Color WUI_BG_COLOR_GLOW        = {   0, 180, 216,   8 };  /* primary_cyan @ ~3% */
static const Clay_Color WUI_BG_COLOR_NOISE       = { 232, 244, 253,   4 };  /* text @ ~1.5% */
static const Clay_Color WUI_BG_COLOR_VIGNETTE    = {   0,   0,   0,  16 };  /* #000 @ ~6% — the corner quads sit on top of any bg colour, so the alpha must stay low enough that they read as faint corner darkening rather than the "4 opaque black corner rectangles" they appeared as in light-mode palettes at the previous ~25% alpha. */

/* v2 design DemoBackdrop colours — dark-mode defaults baked from
 * 15.2 _tokens.wui ($demoBg, $demoSky1/2, $demoFog, $demoFloor, etc.).
 * The 15.2 cl_wired_palette overlay rewrites the _tokens.wui table for
 * the .wui parser; engine modules that read at boot (here) snapshot
 * the dark+amber defaults. Future palette-aware extension can repaint
 * these from the live token table on cvar change. */
static const Clay_Color WUI_BG_V2_SKY_1          = {  58,  24,   8, 180 };  /* $demoSky1 #3a1808 */
static const Clay_Color WUI_BG_V2_SKY_2          = {  26,   9,   5, 140 };  /* $demoSky2 #1a0905 */
static const Clay_Color WUI_BG_V2_FOG            = {  26,  12,   6, 120 };  /* $demoFog  #1a0c06 */
static const Clay_Color WUI_BG_V2_FLOOR          = {  42,  18,   8, 255 };  /* $demoFloor #2a1208 */
static const Clay_Color WUI_BG_V2_TOWER_A        = {  10,   6,   5, 200 };  /* $demoTowerA */
static const Clay_Color WUI_BG_V2_TOWER_B        = {  14,   7,   6, 200 };  /* $demoTowerB */
static const Clay_Color WUI_BG_V2_ARCH           = {  20,   9,   6, 128 };  /* $demoArchA */
static const Clay_Color WUI_BG_V2_EMBER          = { 244, 160,  58, 200 };  /* $accent (amber default) */
static const Clay_Color WUI_BG_V2_PLASMA_HOT     = { 200,  90,  20,  46 };  /* plasma core glow */
static const Clay_Color WUI_BG_V2_PLASMA_COOL    = {  40,  16,   8,  60 };  /* plasma surround */

/* Every background quad is a floating attach-to-root element, so Clay sorts it
 * as its own tree root. Menu content lives in wui_menu_flex_root (also floating,
 * zIndex 0) opened BEFORE the item walk; the background is emitted later during
 * the bg-owning item's walk, so at equal zIndex Clay's stable sort would leave
 * the background painting AFTER (over) the content — and the opaque underpaint
 * would hide it. A negative zIndex pins every background quad strictly behind
 * the zIndex-0 content root, so the scene is always the backmost layer regardless
 * of emit order. This is the single choke point: all v2 scene layers route
 * through wui_bg_emit_rect, so setting it here fixes SCENE/DIM/loading uniformly.
 * (WUI_BG_SCENE_ZINDEX is defined in cl_wired_bg.h so cl_wired_clay.c's SCENE
 * backdrop emit pins the procedural pass to the same backmost layer.) */

/* Anim state. `grid_drift_t` and `scanline_sweep_t` are written by
 * WUI_AnimTick each frame (LOOP_LINEAR sweep across [0, 1)). The bg
 * emit reads them per call. Both are registered once via lazy init —
 * the WUI_AnimStopAll path (shutdown / menu hot-reload) clears the
 * registration, so we re-arm on the next emit. The id values are
 * tracked so we don't double-register on every frame. */
static float wui_bg_grid_drift_t      = 0.0f;
static float wui_bg_scanline_sweep_t  = 0.0f;
static int   wui_bg_grid_drift_id     = 0;
static int   wui_bg_scanline_sweep_id = 0;

/* v2 DemoBackdrop layer state. `parallax_t` LOOP_LINEAR over 12 s
 * drives the X offset for sky / arena_silhouette / embers when the
 * MODIFIER_PARALLAX flag is set; the ember table is a fixed 14-slot
 * BSS array seeded once via wui_bg_init_embers (deterministic xorshift
 * keeps the layout stable across boots). */
static float wui_bg_parallax_t        = 0.0f;
static int   wui_bg_parallax_id       = 0;

#define WUI_BG_EMBER_COUNT             14
#define WUI_BG_PARALLAX_DURATION_MS   12000
#define WUI_BG_PARALLAX_AMP_FRACTION   0.03f   /* X offset = ±3% of width */

/* ── F2 depth-parallax + input response (SCENE path only) ──────────────
 * baseOffset = ambient sinusoid + smoothed mouse + one-shot menu-transition
 * nudge; each layer's shift = baseOffset * its depth factor (far → near).
 * The ambient amplitude is trimmed vs the flat ±3% so the mouse motion leads
 * without the two fighting. All in FRACTION-of-width/height until multiplied. */
#define WUI_BG_SCENE_AMBIENT_AMP_FRAC  0.018f  /* idle breathing, ±1.8% width */
#define WUI_BG_MOUSE_PARALLAX_AMP_X    0.040f  /* ±4.0% width at screen edge */
#define WUI_BG_MOUSE_PARALLAX_AMP_Y    0.025f  /* ±2.5% height at screen edge */
#define WUI_BG_MOUSE_SMOOTH            0.12f   /* lerp toward target / frame (glide) */
#define WUI_BG_TRANSITION_AMP_FRAC     0.035f  /* menu-change nudge, ±3.5% width */
#define WUI_BG_TRANSITION_DURATION_MS  320     /* ease-out settle time */

/* Per-layer depth factor [0 far .. 1 near]. Scales baseOffset per layer so
 * distant layers barely move and near layers lead. Sky is nearly pinned;
 * embers/plasma visibly lead. Floor stays pinned (ground plane). */
#define WUI_BG_DEPTH_SKY     0.10f
#define WUI_BG_DEPTH_FOG     0.18f
#define WUI_BG_DEPTH_ARENA   0.35f
#define WUI_BG_DEPTH_EMBERS  0.70f
#define WUI_BG_DEPTH_PLASMA  0.85f

/* Smoothed mouse-parallax state (fraction of width/height, lerped each frame
 * toward the live normalized cursor so the scene glides rather than snaps). */
static float wui_bg_mouse_x_smoothed  = 0.0f;
static float wui_bg_mouse_y_smoothed  = 0.0f;

/* One-shot menu-transition nudge: a WUI_Anim eases wui_bg_transition_t 1→0
 * (ease-out) after WiredUI_NotifyBgTransition; its sign alternates so
 * successive menu changes push opposite directions for a livelier feel. */
static float wui_bg_transition_t      = 0.0f;
static int   wui_bg_transition_id     = 0;
static float wui_bg_transition_sign   = 1.0f;

typedef struct {
	float x_norm;          /* 0..1 viewport X */
	float y_base_norm;     /* 0..1 viewport Y baseline */
	float amplitude;       /* 0..1 fraction of height; sin oscillation */
	float phase;           /* radians; per-particle phase offset */
	float opacity_phase;
	float period_s;        /* full oscillation period, 4..8 s */
} wuiEmber_t;

static wuiEmber_t wui_bg_embers[ WUI_BG_EMBER_COUNT ];
static qboolean   wui_bg_embers_ready = qfalse;

static void wui_bg_init_embers( void ) {
	int      i;
	unsigned seed = 0xE17BE12Eu;   /* deterministic seed; arbitrary */
	if ( wui_bg_embers_ready ) return;
	for ( i = 0; i < WUI_BG_EMBER_COUNT; i++ ) {
		seed = seed * 1664525u + 1013904223u;
		wui_bg_embers[ i ].x_norm        = 0.05f + ( ( seed >> 8 ) & 0xFFFF ) / 65535.0f * 0.90f;
		seed = seed * 1664525u + 1013904223u;
		/* Full-viewport y distribution: previous [0.60, 0.90] confined
		 * embers to the lower 30% of the screen. v2 spec scatters
		 * particles across the whole viewport with sin Y oscillation. */
		wui_bg_embers[ i ].y_base_norm   = 0.05f + ( ( seed >> 8 ) & 0xFFFF ) / 65535.0f * 0.90f;
		seed = seed * 1664525u + 1013904223u;
		wui_bg_embers[ i ].amplitude     = 0.04f + ( ( seed >> 8 ) & 0xFFFF ) / 65535.0f * 0.10f;
		seed = seed * 1664525u + 1013904223u;
		wui_bg_embers[ i ].phase         = ( ( seed >> 8 ) & 0xFFFF ) / 65535.0f * 2.0f * (float) M_PI;
		seed = seed * 1664525u + 1013904223u;
		wui_bg_embers[ i ].opacity_phase = ( ( seed >> 8 ) & 0xFFFF ) / 65535.0f * 2.0f * (float) M_PI;
		seed = seed * 1664525u + 1013904223u;
		wui_bg_embers[ i ].period_s      = 4.0f + ( ( seed >> 8 ) & 0xFFFF ) / 65535.0f * 4.0f;
	}
	wui_bg_embers_ready = qtrue;
}

static void wui_bg_ensure_anims( void ) {
	if ( !WUI_AnimIsLive( wui_bg_grid_drift_id ) ) {
		wui_bg_grid_drift_id = WUI_AnimCreate(
			"bg_grid_drift", &wui_bg_grid_drift_t,
			0.0f, 1.0f, WUI_BG_GRID_DRIFT_DURATION_MS,
			WUI_ANIM_CURVE_LOOP_LINEAR, WUI_ANIM_FLAG_LOOP );
	}
	if ( !WUI_AnimIsLive( wui_bg_scanline_sweep_id ) ) {
		wui_bg_scanline_sweep_id = WUI_AnimCreate(
			"bg_scanline_sweep", &wui_bg_scanline_sweep_t,
			0.0f, 1.0f, WUI_BG_SCANLINE_DURATION_MS,
			WUI_ANIM_CURVE_LOOP_LINEAR, WUI_ANIM_FLAG_LOOP );
	}
}

// ── Internal layer emit helpers ─────────────────────────────────────

static void wui_bg_emit_base( float x, float y, float w, float h ) {
	CLAY({
		.layout = { .sizing = { CLAY_SIZING_FIXED( w ), CLAY_SIZING_FIXED( h ) } },
		.floating = {
			.attachTo    = CLAY_ATTACH_TO_ROOT,
			.offset      = { x, y },
			.attachPoints = { CLAY_ATTACH_POINT_LEFT_TOP, CLAY_ATTACH_POINT_LEFT_TOP }
		},
		.backgroundColor = wui_bg_resolve( "bg", WUI_BG_COLOR_BASE )
	}) {}
}

/* Emit one full grid pass at the given stride + colour, shifted by
 * (offX, offY). The shift is applied to every line so the whole
 * lattice breathes. Lines crossing the panel boundary are clipped by
 * Clay's emit (we just don't bother to skip them — empty regions
 * cost nothing). */
static void wui_bg_emit_grid_pass( float x, float y, float w, float h,
                                    int stride, int thick, Clay_Color col,
                                    float offX, float offY )
{
	int cols = (int) ( w / stride ) + 2;
	int rows = (int) ( h / stride ) + 2;
	int i;
	/* Vertical lines — each a thin tall rectangle at column boundaries.
	 * Range extends one stride beyond the panel so the drift offset
	 * never reveals a bare edge. */
	for ( i = 0; i <= cols; i++ ) {
		float gx = x + i * stride + offX;
		CLAY({
			.layout = { .sizing = { CLAY_SIZING_FIXED( (float) thick ), CLAY_SIZING_FIXED( h ) } },
			.floating = {
				.attachTo    = CLAY_ATTACH_TO_ROOT,
				.offset      = { gx, y },
				.attachPoints = { CLAY_ATTACH_POINT_LEFT_TOP, CLAY_ATTACH_POINT_LEFT_TOP }
			},
			.backgroundColor = col
		}) {}
	}
	for ( i = 0; i <= rows; i++ ) {
		float gy = y + i * stride + offY;
		CLAY({
			.layout = { .sizing = { CLAY_SIZING_FIXED( w ), CLAY_SIZING_FIXED( (float) thick ) } },
			.floating = {
				.attachTo    = CLAY_ATTACH_TO_ROOT,
				.offset      = { x, gy },
				.attachPoints = { CLAY_ATTACH_POINT_LEFT_TOP, CLAY_ATTACH_POINT_LEFT_TOP }
			},
			.backgroundColor = col
		}) {}
	}
}

static void wui_bg_emit_grid( float x, float y, float w, float h ) {
	float minDim = ( w < h ) ? w : h;
	float amp    = minDim * WUI_BG_GRID_DRIFT_FRACTION;
	/* Two-axis breathing via sin/cos of the anim phase. The phase
	 * ramps linearly 0 → 1 over WUI_BG_GRID_DRIFT_DURATION_MS, then
	 * wraps; sin(2πt) gives smooth back-and-forth. */
	float phase = wui_bg_grid_drift_t * 2.0f * (float) M_PI;
	float offX  = sinf( phase ) * amp;
	float offY  = cosf( phase ) * amp;

	/* Resolve the grid tint from the live `primary_cyan` token so it
	 * follows ui_palette_accent, but keep each layer's baked alpha (the
	 * #rrggbb token form is opaque; wui_bg_resolve would clobber a→255). */
	Clay_Color major = wui_bg_resolve( "primary_cyan", WUI_BG_COLOR_GRID_MAJOR );
	Clay_Color minor = wui_bg_resolve( "primary_cyan", WUI_BG_COLOR_GRID_MINOR );
	major.a = WUI_BG_COLOR_GRID_MAJOR.a;
	minor.a = WUI_BG_COLOR_GRID_MINOR.a;

	/* Minor pass first (denser, fainter) then major on top so the
	 * primary weave wins at intersections. Skip the minor pass when
	 * its alpha is 0 to avoid emitting hundreds of invisible rects. */
	if ( minor.a > 0.0f ) {
		wui_bg_emit_grid_pass( x, y, w, h,
			WUI_BG_GRID_STRIDE_MINOR_PX, WUI_BG_GRID_THICK_PX,
			minor, offX, offY );
	}
	wui_bg_emit_grid_pass( x, y, w, h,
		WUI_BG_GRID_STRIDE_MAJOR_PX, WUI_BG_GRID_THICK_PX,
		major, offX, offY );
}

static void wui_bg_emit_scanlines( float x, float y, float w, float h ) {
	/* Single horizontal sweep line driven by the wuiAnim sweep value
	 * (LOOP_LINEAR 0 → 1 wraps). The line traverses the full panel
	 * height each cycle. */
	float gy = y + wui_bg_scanline_sweep_t * h;
	Clay_Color scan = wui_bg_resolve( "primary_cyan", WUI_BG_COLOR_SCANLINES );
	scan.a = WUI_BG_COLOR_SCANLINES.a;
	CLAY({
		.layout = { .sizing = { CLAY_SIZING_FIXED( w ), CLAY_SIZING_FIXED( (float) WUI_BG_SCANLINE_THICK_PX ) } },
		.floating = {
			.attachTo    = CLAY_ATTACH_TO_ROOT,
			.offset      = { x, gy },
			.attachPoints = { CLAY_ATTACH_POINT_LEFT_TOP, CLAY_ATTACH_POINT_LEFT_TOP }
		},
		.backgroundColor = scan
	}) {}
}

static void wui_bg_emit_glow_rays( float x, float y, float w, float h ) {
	float side = ( ( w < h ) ? w : h ) * WUI_BG_GLOW_CORNER_FRACTION;
	Clay_Color glow = wui_bg_resolve( "primary_cyan", WUI_BG_COLOR_GLOW );
	glow.a = WUI_BG_COLOR_GLOW.a;
	float positions[ 4 ][ 2 ] = {
		{ x,             y             },
		{ x + w - side,  y             },
		{ x,             y + h - side  },
		{ x + w - side,  y + h - side  }
	};
	int i;
	for ( i = 0; i < 4; i++ ) {
		CLAY({
			.layout = { .sizing = { CLAY_SIZING_FIXED( side ), CLAY_SIZING_FIXED( side ) } },
			.floating = {
				.attachTo    = CLAY_ATTACH_TO_ROOT,
				.offset      = { positions[i][0], positions[i][1] },
				.attachPoints = { CLAY_ATTACH_POINT_LEFT_TOP, CLAY_ATTACH_POINT_LEFT_TOP }
			},
			.backgroundColor = glow,
			.cornerRadius    = { side * 0.5f, side * 0.5f, side * 0.5f, side * 0.5f }
		}) {}
	}
}

static void wui_bg_emit_noise( float x, float y, float w, float h ) {
	/* Deterministic pseudo-random scatter — same seed every frame so the
	 * pattern doesn't shimmer. Tiny squares; the dot count is sparse so
	 * subsequent layers (vignette) still read. */
	int        i;
	unsigned   seed = 0xC0FFEE; /* fixed */
	const int  N    = WUI_BG_NOISE_DOT_COUNT;
	Clay_Color noise = wui_bg_resolve( "text", WUI_BG_COLOR_NOISE );
	noise.a = WUI_BG_COLOR_NOISE.a;
	for ( i = 0; i < N; i++ ) {
		float dx, dy;
		seed = seed * 1664525u + 1013904223u;
		dx = ( seed & 0xFFFF ) / 65535.0f;
		seed = seed * 1664525u + 1013904223u;
		dy = ( seed & 0xFFFF ) / 65535.0f;
		CLAY({
			.layout = { .sizing = { CLAY_SIZING_FIXED( 1.0f ), CLAY_SIZING_FIXED( 1.0f ) } },
			.floating = {
				.attachTo    = CLAY_ATTACH_TO_ROOT,
				.offset      = { x + dx * w, y + dy * h },
				.attachPoints = { CLAY_ATTACH_POINT_LEFT_TOP, CLAY_ATTACH_POINT_LEFT_TOP }
			},
			.backgroundColor = noise
		}) {}
	}
}

static void wui_bg_emit_vignette( float x, float y, float w, float h ) {
	/* Vignette resolved from the palette `vignette` token rather than a
	 * hardcoded black; the token form `rgba(r,g,b,a)` palette-swaps
	 * tone (light mode warm brown, dark mode black) so the corner
	 * falloff matches the active mode's atmosphere instead of showing
	 * as four opaque dark squares against a light cream background. */
	Clay_Color  base = wui_bg_resolve( "vignette", WUI_BG_V2_FOG );
	float       side = ( ( w < h ) ? w : h ) * WUI_BG_VIGNETTE_FRACTION;
	float       positions[ 4 ][ 2 ] = {
		{ x,             y             },
		{ x + w - side,  y             },
		{ x,             y + h - side  },
		{ x + w - side,  y + h - side  }
	};
	int i;
	/* Token alpha (e.g. 0.55 = 140 in dark, 0.35 = 89 in light) is too
	 * strong for corner falloff once the renderer applies it as a
	 * single soft-edged quad per corner — scale it down so the corners
	 * read as faint ambient haze rather than warm-brown / dark squares
	 * against the cream sky in light palettes. */
	base.a = (uint8_t)( base.a / 16 );
	for ( i = 0; i < 4; i++ ) {
		CLAY({
			.layout = { .sizing = { CLAY_SIZING_FIXED( side ), CLAY_SIZING_FIXED( side ) } },
			.floating = {
				.attachTo    = CLAY_ATTACH_TO_ROOT,
				.offset      = { positions[i][0], positions[i][1] },
				.attachPoints = { CLAY_ATTACH_POINT_LEFT_TOP, CLAY_ATTACH_POINT_LEFT_TOP }
			},
			.backgroundColor = base,
			.cornerRadius    = { side * 0.5f, side * 0.5f, side * 0.5f, side * 0.5f }
		}) {}
	}
}

// ── v2 layer emit helpers ───────────────────────────────────────────

/* Floating rectangle helper — reduces emit boilerplate for v2 layers
 * (the legacy six each open this inline; the v2 layers stack many
 * small emits and benefit from the shorter call). */
static void wui_bg_emit_rect( float x, float y, float w, float h, Clay_Color c ) {
	if ( w <= 0.0f || h <= 0.0f ) return;
	CLAY({
		.layout = { .sizing = { CLAY_SIZING_FIXED( w ), CLAY_SIZING_FIXED( h ) } },
		.floating = {
			.attachTo    = CLAY_ATTACH_TO_ROOT,
			.offset      = { x, y },
			.attachPoints = { CLAY_ATTACH_POINT_LEFT_TOP, CLAY_ATTACH_POINT_LEFT_TOP },
			.zIndex      = WUI_BG_SCENE_ZINDEX
		},
		.backgroundColor = c
	}) {}
}

/* SKY_GRADIENT_RADIAL — Clay has no native radial gradient primitive,
 * so we approximate via N horizontal bands ramping from sky1 (top) to
 * sky2 (lower) and a faint accent ambient pool near the top-mid. The
 * total cost is N emits (N=12 here, ~12 floating quads). */
static void wui_bg_emit_v2_sky( float x, float y, float w, float h, float px ) {
	/* 32-band ramp with rounded uint8 quantisation. The previous 12-band
	 * truncation produced visible seams in dark mode because
	 * (sky2 - sky1) / 11 in the B + G channels was < 1 per band, so
	 * adjacent bands quantised to the same color until the accumulated
	 * float crossed an integer boundary — making the ramp read as 3-4
	 * distinct stripes rather than a continuous gradient. 32 bands with
	 * (int)( value + 0.5f ) rounding gives sub-band channel resolution. */
	const int N = 64;
	int i;
	float bandH = ( h * 0.55f ) / (float) N;
	Clay_Color sky1 = wui_bg_resolve( "demoSky1", WUI_BG_V2_SKY_1 );
	Clay_Color sky2 = wui_bg_resolve( "demoSky2", WUI_BG_V2_SKY_2 );
	for ( i = 0; i < N; i++ ) {
		float t = (float) i / (float) ( N - 1 );
		float r = sky1.r + ( (float) sky2.r - (float) sky1.r ) * t;
		float g = sky1.g + ( (float) sky2.g - (float) sky1.g ) * t;
		float b = sky1.b + ( (float) sky2.b - (float) sky1.b ) * t;
		float a = sky1.a + ( (float) sky2.a - (float) sky1.a ) * t;
		Clay_Color c;
		c.r = (uint8_t)( r + 0.5f );
		c.g = (uint8_t)( g + 0.5f );
		c.b = (uint8_t)( b + 0.5f );
		c.a = (uint8_t)( a + 0.5f );
		/* +1px overlap absorbs sub-pixel rounding so the band edges
		 * always cover the boundary even when bandH is fractional. */
		wui_bg_emit_rect( x + px, y + (float) i * bandH, w, bandH + 1.0f, c );
	}
}

/* ARENA_SILHOUETTE — far towers + mid arches + floor slab, approximated
 * as solid rectangles per qw-variants.jsx DemoBackdrop SVG. Coordinates
 * are normalised against the viewport and shifted by the parallax X
 * offset when MODIFIER_PARALLAX is set on the layer. */
static void wui_bg_emit_v2_arena_silhouette( float x, float y, float w, float h, float px ) {
	float floorY = y + h * 0.78f;
	float floorH = h - ( floorY - y );
	Clay_Color floorC  = wui_bg_resolve( "demoFloor",  WUI_BG_V2_FLOOR   );
	Clay_Color towerA  = wui_bg_resolve( "demoTowerA", WUI_BG_V2_TOWER_A );
	Clay_Color towerB  = wui_bg_resolve( "demoTowerB", WUI_BG_V2_TOWER_B );
	Clay_Color archA   = wui_bg_resolve( "demoArchA",  WUI_BG_V2_ARCH    );
	Clay_Color archB   = wui_bg_resolve( "demoArchB",  WUI_BG_V2_ARCH    );
	/* Tower/arch token form is opaque #rrggbb; v2 design renders these
	 * as VERY faint silhouettes barely lifted off the dusk bg (the bg
	 * fill comes from a separate base pass in WUI_DrawBackgroundLayered).
	 * Heavy alpha drop turns rectangles into atmospheric haze rather
	 * than visible blocks — Clay can't draw trapezoids/polygons so
	 * subtlety is the only spec-aligning lever available. */
	/* Tower / arch alpha dropped to "barely-there" — light palettes
	 * still saw the 22/18 levels as distinct edge bars because their
	 * RGB delta against light cream bg was large even at 8-10% opacity.
	 * The renderer is constrained to rect primitives (Clay can't draw
	 * gothic polygonal silhouettes) so alpha is the only available
	 * lever to keep them from reading as opaque blocks. */
	towerA.a = 5;
	towerB.a = 5;
	archA.a  = 4;
	archB.a  = 4;
	/* Floor fades from transparent at the top of the floor zone to full
	 * opacity at the viewport bottom. 24 sub-bands give a 4×-tighter
	 * ramp than the previous 6 — at 6 bands each band was ~44 px tall
	 * at 1200-height viewport, which read as a discrete stepped strip
	 * rather than a continuous fade. */
	{
		const int   FN     = 24;
		int         fi;
		float       fbandH = floorH / (float) FN;
		Clay_Color  fadeC  = floorC;
		for ( fi = 0; fi < FN; fi++ ) {
			float t = (float) fi / (float) ( FN - 1 );  /* 0 at top, 1 at bottom */
			fadeC.a = (uint8_t)( (float) floorC.a * t + 0.5f );
			wui_bg_emit_rect( x, floorY + (float) fi * fbandH, w, fbandH + 1.0f, fadeC );
		}
	}
	/* Far towers — left + right edge silhouettes only. The central
	 * tower (was 50% × 20%, 58% tall) + central arch (was 36% × 60%,
	 * 28% wide) were dominating the mid-screen real estate where the
	 * menu hero + cards live. Clay can't draw the gothic polygonal
	 * shapes from the v2 SVG; keeping just the side rectangles reads
	 * as "distant ruins" without obscuring the foreground composition. */
	wui_bg_emit_rect( x + w * 0.04f + px, y + h * 0.36f, w * 0.06f, h * 0.42f, towerA );
	wui_bg_emit_rect( x + w * 0.10f + px, y + h * 0.31f, w * 0.03f, h * 0.47f, towerB );
	wui_bg_emit_rect( x + w * 0.87f + px, y + h * 0.33f, w * 0.06f, h * 0.45f, towerA );
	wui_bg_emit_rect( x + w * 0.83f + px, y + h * 0.38f, w * 0.04f, h * 0.40f, towerB );
	/* Mid arches — keep left + right only, drop the central 28%-wide
	 * arch that obscured the menu region. */
	wui_bg_emit_rect( x + w * 0.14f + px, y + h * 0.53f, w * 0.10f, h * 0.25f, archA );
	wui_bg_emit_rect( x + w * 0.74f + px, y + h * 0.50f, w * 0.12f, h * 0.28f, archA );
}

/* EMBERS — 14 dot particles oscillating in Y via per-particle sin
 * with deterministic seeded phase. Each ember is a 2x2 px square at
 * full $accent colour, alpha-modulated by a slower opacity_phase sin
 * so the swarm appears to glow in and out. */
static void wui_bg_emit_v2_embers( float x, float y, float w, float h, float px, float frame_sec ) {
	int i;
	Clay_Color base = wui_bg_resolve( "accent", WUI_BG_V2_EMBER );
	/* Token alpha is 255 (opaque) for accent; we want embers around 200/255
	 * intensity so the per-particle opacity modulator has visible range. */
	base.a = 200;
	wui_bg_init_embers();
	for ( i = 0; i < WUI_BG_EMBER_COUNT; i++ ) {
		const wuiEmber_t *e = &wui_bg_embers[ i ];
		float y_offset  = e->amplitude * sinf( frame_sec * 2.0f * (float) M_PI / e->period_s + e->phase );
		float opacity   = 0.45f + 0.55f * sinf( frame_sec * 2.0f * (float) M_PI / ( e->period_s * 1.3f ) + e->opacity_phase );
		Clay_Color c    = base;
		float ex        = x + e->x_norm * w + px;
		float ey        = y + ( e->y_base_norm + y_offset ) * h;
		if ( opacity < 0.0f ) opacity = 0.0f;
		c.a = (uint8_t)( c.a * opacity );
		/* Quad bumped from 2 px to 4 px so the embers register against
		 * the dusk gradient at the v2 design viewport scale — the smaller
		 * size disappeared as sub-pixel motes at 1440p/4K. */
		wui_bg_emit_rect( ex, ey, 4.0f, 4.0f, c );
	}
}

/* FOG_GRADIENT — top-to-mid linear fog as N stacked bands (4 here for
 * cheap parallax-independent atmosphere). Bottom band fully opaque,
 * fading to transparent at the top. */
static void wui_bg_emit_v2_fog( float x, float y, float w, float h ) {
	/* 16-band ramp (was 4) with rounded uint8 quantisation. The previous
	 * 4 bands at h*0.45/4 ≈ 11% viewport-height each produced visibly
	 * discrete fog stripes in light-mode palettes; tighter bands give
	 * a continuous fade matching the v2 atmospheric layer intent. */
	const int N = 16;
	int i;
	float bandH = ( h * 0.45f ) / (float) N;
	float topY  = y + h * 0.35f;
	Clay_Color base = wui_bg_resolve( "demoFog", WUI_BG_V2_FOG );
	base.a = 120;
	for ( i = 0; i < N; i++ ) {
		float t = 1.0f - (float) i / (float) ( N - 1 );    /* 1..0 fade-out as we go up */
		Clay_Color c = base;
		c.a = (uint8_t)( (float) base.a * t + 0.5f );
		wui_bg_emit_rect( x, topY + (float) i * bandH, w, bandH + 1.0f, c );
	}
}

/* GLOW_PLASMA (PlasmaBG primitive) — a soft radial bloom approximated by
 * a stack of concentric alpha-ramped rects. With MODIFIER_ANIMATED the
 * core diameter pulses via the parallax_t LOOP_LINEAR phase (re-using one
 * timer rather than registering yet another); without it the plasma stays
 * static. v2's qwplasma composite (translate+scale+hue-rotate) cannot be
 * reproduced with the curve set this dispatch inherits — per 15.4 the
 * qwplasma-class composites are accepted as static / approximated.
 *
 * Clay draws only axis-aligned rects, so the bloom is composited from N
 * nested rects of DECREASING size, each at a LOW per-ring alpha. Under
 * alpha-over compositing a point covered by k rings reaches effective
 * coverage 1-(1-a)^k, so the centre (covered by all N) builds a faint
 * warm core while the outer rim (1 ring) barely tints — the composite
 * reads as a radial falloff that fades to nothing at its edge, NOT the 3
 * hard-edged translucent blocks the previous 3-rect form drew.
 *
 * Colour is the DESIGN WARM plasma palette (WUI_BG_V2_PLASMA_COOL/_HOT),
 * NOT the live accent token. The accent token follows ui_palette_accent,
 * and cool accents (e.g. cyan #46c4d8) turned the bloom into a solid teal
 * rectangle over the menu's central content — a stray blue box that read
 * as a bug rather than atmosphere. A warm dusk-toned bloom harmonises
 * with the sky/floor/fog underneath at every accent theme. */
static void wui_bg_emit_v2_plasma( float x, float y, float w, float h, float px, qboolean animated ) {
	const int   N        = 18;
	/* Per-ring alpha kept very low so ~N overlaps at the centre stay a
	 * faint bloom (peak effective coverage ≈ 1-(1-4/255)^18 ≈ 0.25) while
	 * each individual ring's rim is only ~1.6% — below the threshold where
	 * the nested-rect stepping reads as concentric rectangle outlines, so
	 * the composite dissolves into a soft warm centre-glow with no edge. */
	const float RING_A   = 4.0f;
	int   i;
	float cx = x + w * 0.5f + px;
	float cy = y + h * 0.5f;
	float baseR = ( w < h ? w : h ) * 0.40f;
	float scale = animated
		? 1.0f + 0.08f * sinf( wui_bg_parallax_t * 2.0f * (float) M_PI )
		: 1.0f;
	/* Warm design plasma palette — cool outer surround → hot inner core.
	 * Deliberately NOT accent-driven (see header): a cool accent theme
	 * would repaint this as a blue block over the menu. */
	Clay_Color cool = WUI_BG_V2_PLASMA_COOL;
	Clay_Color hot  = WUI_BG_V2_PLASMA_HOT;
	for ( i = 0; i < N; i++ ) {
		/* t: 0 at the outermost ring → 1 at the innermost core. */
		float t  = (float) i / (float) ( N - 1 );
		float rr = baseR * ( 1.10f - 0.98f * t ) * scale;  /* 1.10·baseR → 0.12·baseR */
		Clay_Color c;
		/* Blend cool→hot toward the core; the last few rings are the hot core. */
		c.r = (uint8_t)( cool.r + ( hot.r - cool.r ) * t );
		c.g = (uint8_t)( cool.g + ( hot.g - cool.g ) * t );
		c.b = (uint8_t)( cool.b + ( hot.b - cool.b ) * t );
		c.a = (uint8_t) RING_A;
		wui_bg_emit_rect( cx - rr, cy - rr * 0.7f, rr * 2.0f, rr * 1.4f, c );
	}
}

/* Parallax modifier: time-driven X offset, HALFSINE-equivalent via
 * sin(2π·t) so the panel slides back and forth ±AMP. Mouse-driven
 * parallax is deferred (would need a viewport mouse accumulator the
 * compositor doesn't expose today). */
static float wui_bg_parallax_offset( float w ) {
	return w * WUI_BG_PARALLAX_AMP_FRACTION
	       * sinf( wui_bg_parallax_t * 2.0f * (float) M_PI );
}

// ── Public dispatch ─────────────────────────────────────────────────

void WUI_DrawBackgroundLayered( float x, float y, float w, float h, int flags ) {
	if ( w <= 0 || h <= 0 ) return;

	/* Lazy-arm anim slots on first emit. WUI_AnimStopAll clears the pool but
	 * not the trackers below, so liveness is asked of the pool rather than
	 * inferred from a remembered id. */
	if ( ( flags & WUI_BG_LAYER_GRID ) || ( flags & WUI_BG_LAYER_SCANLINES ) ) {
		wui_bg_ensure_anims();
	}

	/* v2 layers need the parallax LOOP_LINEAR timer (PARALLAX modifier
	 * + PLASMA ANIMATED modifier both read wui_bg_parallax_t). Arming
	 * is idempotent — WUI_AnimCreate returns the existing id when the
	 * pool slot is live. */
	if ( flags & ( WUI_BG_LAYER_SKY | WUI_BG_LAYER_ARENA_SILHOUETTE
	             | WUI_BG_LAYER_EMBERS | WUI_BG_LAYER_PLASMA
	             | WUI_BG_MODIFIER_PARALLAX | WUI_BG_MODIFIER_ANIMATED ) ) {
		if ( !WUI_AnimIsLive( wui_bg_parallax_id ) ) {
			wui_bg_parallax_id = WUI_AnimCreate(
				"bg_parallax", &wui_bg_parallax_t,
				0.0f, 1.0f, WUI_BG_PARALLAX_DURATION_MS,
				WUI_ANIM_CURVE_LOOP_LINEAR, WUI_ANIM_FLAG_LOOP );
		}
	}

	float px = ( flags & WUI_BG_MODIFIER_PARALLAX )
	         ? wui_bg_parallax_offset( w )
	         : 0.0f;
	float frame_sec = (float) cls.realtime * 0.001f;

	/* v2 base underpaint: when any of the v2 atmosphere layers fire,
	 * lay down a full-viewport $demoBg fill BEFORE the sky so the
	 * bottom half (which SKY does not cover) sits on the warm dusk
	 * colour instead of the engine's black screen clear. Without this
	 * the arena silhouettes + floor + fog all blend against pure black
	 * and read as stark dark blocks. */
	if ( flags & ( WUI_BG_LAYER_SKY | WUI_BG_LAYER_ARENA_SILHOUETTE
	             | WUI_BG_LAYER_EMBERS | WUI_BG_LAYER_FOG ) ) {
		Clay_Color v2bg = wui_bg_resolve( "demoBg", WUI_BG_V2_FLOOR );
		wui_bg_emit_rect( x, y, w, h, v2bg );
	}

	/* Painter's order, back to front:
	 *   1. v2 sky / arena_silhouette / embers / fog (DemoBackdrop stack)
	 *   2. legacy base / grid / scanlines / glow_rays / noise / vignette
	 *   3. v2 plasma overlay (on top — bright bloom over everything) */
	if ( flags & WUI_BG_LAYER_SKY              ) wui_bg_emit_v2_sky              ( x, y, w, h, px );
	if ( flags & WUI_BG_LAYER_ARENA_SILHOUETTE ) wui_bg_emit_v2_arena_silhouette ( x, y, w, h, px );
	if ( flags & WUI_BG_LAYER_EMBERS           ) wui_bg_emit_v2_embers           ( x, y, w, h, px, frame_sec );
	if ( flags & WUI_BG_LAYER_FOG              ) wui_bg_emit_v2_fog              ( x, y, w, h );

	if ( flags & WUI_BG_LAYER_BASE      ) wui_bg_emit_base     ( x, y, w, h );
	if ( flags & WUI_BG_LAYER_GRID      ) wui_bg_emit_grid     ( x, y, w, h );
	if ( flags & WUI_BG_LAYER_SCANLINES ) wui_bg_emit_scanlines( x, y, w, h );
	if ( flags & WUI_BG_LAYER_GLOW_RAYS ) wui_bg_emit_glow_rays( x, y, w, h );
	if ( flags & WUI_BG_LAYER_NOISE     ) wui_bg_emit_noise    ( x, y, w, h );
	if ( flags & WUI_BG_LAYER_VIGNETTE  ) wui_bg_emit_vignette ( x, y, w, h );

	if ( flags & WUI_BG_LAYER_PLASMA    ) wui_bg_emit_v2_plasma( x, y, w, h, 0.0f,
		( flags & WUI_BG_MODIFIER_ANIMATED ) != 0 );
}

void WUI_DrawBackgroundDim( float x, float y, float w, float h ) {
	if ( w <= 0.0f || h <= 0.0f ) return;
	/* Single dark scrim, no layered scene — the WUI_BG_INTENT_DIM path for a
	 * menu opened over live gameplay. Clay_Color channels are 0..255; alpha 140
	 * ≈ 0.55 matches the in-game scrim token. Same floating attach-to-root quad
	 * as the layered base pass so it sits behind the menu's foreground children. */
	wui_bg_emit_rect( x, y, w, h, (Clay_Color){ 0.0f, 0.0f, 0.0f, 140.0f } );
}

// ── F2 depth-parallax scene (SCENE intent path) ─────────────────────

/* Normalized cursor accessor — forward-declared here (defined in cl_wired_ui.c)
 * to avoid an include cycle: cl_wired_ui.h includes cl_wired_bg.h. */
void WiredUI_GetCursorNorm( float *nx, float *ny );

void WiredUI_NotifyBgTransition( void ) {
	/* One-shot horizontal nudge on a SCENE menu change: kick wui_bg_transition_t
	 * to 1 and ease it back to 0 (ease-out) so the scene shifts and settles.
	 * Sign alternates each call so consecutive changes push opposite ways. */
	wui_bg_transition_sign = -wui_bg_transition_sign;
	wui_bg_transition_t    = 1.0f;
	if ( wui_bg_transition_id != 0 ) {
		WUI_AnimStop( wui_bg_transition_id );
		wui_bg_transition_id = 0;
	}
	wui_bg_transition_id = WUI_AnimCreate(
		"bg_transition", &wui_bg_transition_t,
		1.0f, 0.0f, WUI_BG_TRANSITION_DURATION_MS,
		WUI_ANIM_CURVE_EASE_OUT, 0 );
}

/* (R1: wui_bg_scene_base_offset retired — the rect-band SCENE it computed
 * per-layer parallax for is replaced by the menubg.frag procedural pass, which
 * takes normalized mouse + transition directly via WUI_SceneBackdropParams and
 * does its own parallax in-shader. The pixel-offset depth math is gone.) */

/* R1: the SCENE is now ONE RAL procedural pass (menubg.frag) — a smooth
 * constellation-over-warm-dusk scene on a single full-viewport quad, replacing
 * the ~200-rect DemoBackdrop. This function advances the continuous scene state
 * (mouse glide, which the dispatch reads) and emits the single backmost custom
 * command; the renderer draws the shader. The old wui_bg_emit_v2_* rect layers
 * are retired (kept as dead helpers only if still referenced elsewhere).
 *
 * The custom-command emit + the re.DrawMenuBackdrop dispatch live in
 * cl_wired_clay.c (where the scratch arena + custom-command channel are); this
 * exports the per-frame scene params it reads at dispatch time. */
void WUI_SceneBackdropParams( float *outTime, float *outMouseX,
                              float *outMouseY, float *outTransition ) {
	float nx = 0.0f, ny = 0.0f;

	/* Glide the smoothed cursor toward the live normalized position each frame.
	 * This runs every SCENE frame regardless of the anim pool, so the parallax
	 * keeps responding across menu changes (bug-a) and on the loading screen. */
	WiredUI_GetCursorNorm( &nx, &ny );
	wui_bg_mouse_x_smoothed += ( nx - wui_bg_mouse_x_smoothed ) * WUI_BG_MOUSE_SMOOTH;
	wui_bg_mouse_y_smoothed += ( ny - wui_bg_mouse_y_smoothed ) * WUI_BG_MOUSE_SMOOTH;

	/* Continuous wallclock seconds. cls.realtime accumulates monotonically and
	 * is zeroed ONLY in CL_Init (never per-menu / per-map), so the shader clock
	 * advances smoothly across menu changes AND on the loading screen — this is
	 * what fixes the frozen-clock loading scene (bug-c) by construction. It is
	 * immune to WUI_AnimStopAll / the resettable anim pool. */
	if ( outTime )       *outTime       = (float) cls.realtime * 0.001f;
	if ( outMouseX )     *outMouseX     = wui_bg_mouse_x_smoothed;
	if ( outMouseY )     *outMouseY     = wui_bg_mouse_y_smoothed;
	/* Signed one-shot menu-change nudge, eased 1→0 by WiredUI_NotifyBgTransition;
	 * the shader offsets the field by it (does NOT hard-reset the scene). */
	if ( outTransition ) *outTransition = wui_bg_transition_sign * wui_bg_transition_t;
}

void WUI_DrawBackgroundScene( float x, float y, float w, float h ) {
	if ( w <= 0.0f || h <= 0.0f ) return;

	/* Emit the single backmost (zIndex -10) procedural-backdrop custom command.
	 * The dispatch (cl_wired_clay.c) reads WUI_SceneBackdropParams and calls
	 * re.DrawMenuBackdrop, which draws the menubg.frag fullscreen quad blended
	 * into the open 2D UI pass so the menu content composites on top. */
	WUI_EmitSceneBackdrop( x, y, w, h );
}

// ── Modder API: flag-string parser ──────────────────────────────────

int WUI_BackgroundParseFlags( const char *spec ) {
	int   flags = 0;
	const char *p;
	char  token[ 32 ];

	if ( !spec || !*spec ) return 0;
	if ( !Q_stricmp( spec, "full" )    ) return WUI_BG_LAYER_ALL;
	if ( !Q_stricmp( spec, "minimal" ) ) return WUI_BG_LAYER_BASE;

	p = spec;
	while ( *p ) {
		int i = 0;
		while ( *p == ' ' || *p == '\t' || *p == '+' || *p == ',' ) p++;
		while ( *p && *p != ' ' && *p != '\t' && *p != '+' && *p != ',' && i < (int) sizeof( token ) - 1 ) {
			token[ i++ ] = *p++;
		}
		token[ i ] = '\0';
		if ( !token[ 0 ] ) break;

		if      ( !Q_stricmp( token, "base"      ) ) flags |= WUI_BG_LAYER_BASE;
		else if ( !Q_stricmp( token, "grid"      ) ) flags |= WUI_BG_LAYER_GRID;
		else if ( !Q_stricmp( token, "scanlines" ) ) flags |= WUI_BG_LAYER_SCANLINES;
		else if ( !Q_stricmp( token, "glow"      )
		       || !Q_stricmp( token, "glow_rays" ) ) flags |= WUI_BG_LAYER_GLOW_RAYS;
		else if ( !Q_stricmp( token, "noise"     )
		       || !Q_stricmp( token, "grain"     ) ) flags |= WUI_BG_LAYER_NOISE;
		else if ( !Q_stricmp( token, "vignette"  ) ) flags |= WUI_BG_LAYER_VIGNETTE;
		else if ( !Q_stricmp( token, "sky"                 )
		       || !Q_stricmp( token, "sky_gradient_radial" ) ) flags |= WUI_BG_LAYER_SKY;
		else if ( !Q_stricmp( token, "arena"            )
		       || !Q_stricmp( token, "arena_silhouette" ) ) flags |= WUI_BG_LAYER_ARENA_SILHOUETTE;
		else if ( !Q_stricmp( token, "embers" ) )     flags |= WUI_BG_LAYER_EMBERS;
		else if ( !Q_stricmp( token, "fog"           )
		       || !Q_stricmp( token, "fog_gradient" ) ) flags |= WUI_BG_LAYER_FOG;
		else if ( !Q_stricmp( token, "plasma" ) )     flags |= WUI_BG_LAYER_PLASMA;
		else if ( !Q_stricmp( token, "parallax" ) )   flags |= WUI_BG_MODIFIER_PARALLAX;
		else if ( !Q_stricmp( token, "animated" ) )   flags |= WUI_BG_MODIFIER_ANIMATED;
		else if ( !Q_stricmp( token, "demo"            )
		       || !Q_stricmp( token, "demo_backdrop"   ) ) flags |= WUI_BG_DEMO_BACKDROP;
		else {
			Com_Log( SEV_WARN, LOG_CH(ch_ui),
				"WUI/bg: unknown layer flag '%s' (accept: base, grid, "
				"scanlines, glow, noise/grain, vignette, sky, arena, "
				"embers, fog, plasma, parallax, animated, full, "
				"minimal, demo)\n", token );
		}
	}
	return flags;
}

#endif /* FEAT_WIRED_UI */
