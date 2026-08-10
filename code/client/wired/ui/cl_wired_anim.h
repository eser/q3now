// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
cl_wired_anim.h — Wired UI animation primitive

Engine-side compositor framework for time-driven scalar interpolation.
Author intent: per-itemDef `animation "<name>" speed/duration ...` keyword
binds a named built-in animation to the item; the compositor's per-frame
tick advances the eased value; emit reads the resulting offset/alpha and
applies it during Clay primitive generation.

Storage: fixed-size pool (WUI_ANIM_POOL_MAX slots) keyed by id. Per-frame
WUI_AnimTick walks active entries and writes into `target_ref`. Inactive
slots auto-reclaim when t >= 1.0 on a non-loop curve.

Single source of truth for time-based UI transitions: scroll marquee
(LOOP_LINEAR), fade-in/out (EASE_IN_OUT), slide-up/down (EASE_OUT).
Future expansions (parallax, glow pulse, etc.) plug in via the same
named-anim registry.
*/

#ifndef CL_WIRED_ANIM_H
#define CL_WIRED_ANIM_H

#include "../../../qcommon/q_feats.h"

#if FEAT_WIRED_UI

#include "../../../qcommon/q_shared.h"

// ── Curves ──────────────────────────────────────────────────────────
typedef enum {
	WUI_ANIM_CURVE_LINEAR        = 0,
	WUI_ANIM_CURVE_EASE_IN       = 1,
	WUI_ANIM_CURVE_EASE_OUT      = 2,
	WUI_ANIM_CURVE_EASE_IN_OUT   = 3,
	WUI_ANIM_CURVE_LOOP_LINEAR   = 4,
	/* Half-sine: sin(t·π) maps t∈[0,1] → [0,1,0] — smooth ramp up to the
	 * midpoint and symmetric ramp back down. Two canonical use cases:
	 *   (a) Pulse / indicator dot — paired with WUI_ANIM_FLAG_LOOP to
	 *       produce a continuous brighten-dim cycle. The shipping `pulse`
	 *       built-in routes through this curve for "live" / "active"
	 *       indicator dots in main.wui.
	 *   (b) Scale-in/out animation — the 0→1→0 envelope is the natural
	 *       shape for medal / popup / award elements that pop in, hold
	 *       at full scale at the midpoint, and fade away. No separate
	 *       SCALE curve is needed; route a scale animation through this
	 *       same curve with WUI_ANIM_FLAG_LOOP off and the appropriate
	 *       durationMs. */
	WUI_ANIM_CURVE_HALFSINE      = 5,
	/* Step: discrete on/off (no interpolation). Held at `from` while
	 * t < 0.5, snaps to `to` at t >= 0.5. Standard step-end semantic;
	 * paired with WUI_ANIM_FLAG_LOOP yields a square-wave blink. Maps
	 * cleanly onto the CSS `steps(1)` / step-end timing function used
	 * by qwblink-style indicators. */
	WUI_ANIM_CURVE_STEP          = 6
} wuiAnimCurve_t;

// ── Flags ───────────────────────────────────────────────────────────
#define WUI_ANIM_FLAG_LOOP    0x0001
#define WUI_ANIM_FLAG_PAUSED  0x0002

// ── Pool dimensions ─────────────────────────────────────────────────
#define WUI_ANIM_POOL_MAX     128

typedef struct wuiAnim_s {
	int             id;             /* 0 = unused slot */
	qboolean        active;
	int             startTick;      /* cls.realtime in ms when created */
	int             durationMs;     /* 0 = single-frame snap (avoid /0) */
	wuiAnimCurve_t  curve;
	float           from;
	float           to;
	float          *targetRef;      /* compositor-allocated, owned by caller */
	int             flags;
	char            name[ 64 ];     /* diagnostic / lookup */
} wuiAnim_t;

// ── API ─────────────────────────────────────────────────────────────

/* Allocate + activate an animation. Returns the id (positive int) or
 * 0 on pool exhaustion. `targetRef` must remain valid until the anim
 * completes or is stopped. */
int  WUI_AnimCreate( const char *name, float *targetRef,
                     float from, float to,
                     int durationMs, wuiAnimCurve_t curve, int flags );

/* Stop a specific anim by id. No-op if id invalid. */
void WUI_AnimStop( int id );

/* Stop all active anims — called from WiredUI_Shutdown + reset paths
 * (menu hot-reload). */
void WUI_AnimStopAll( void );

/* Per-frame tick. Called from WiredUI_TickFrame BEFORE the compositor
 * walk so emit sees fresh interpolated values. */
void WUI_AnimTick( int realtime );

/* Diagnostic: count of currently-active anims. Used by smoke / unit
 * tests; cheap O(1). */
int  WUI_AnimActiveCount( void );

/* Built-in named-anim registry — resolve a `.wui` `animation "<name>"`
 * keyword to from/to/duration/curve/flags defaults. Returns qfalse for
 * unknown names; caller may pass through to a Lua-defined registry in
 * a future expansion. */
typedef struct {
	const char     *name;
	float           from;
	float           to;
	int             durationMs;
	wuiAnimCurve_t  curve;
	int             flags;
} wuiAnimBuiltin_t;

const wuiAnimBuiltin_t *WUI_AnimFindBuiltin( const char *name );

#endif /* FEAT_WIRED_UI */
#endif /* CL_WIRED_ANIM_H */
