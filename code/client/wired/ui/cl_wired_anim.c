// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
cl_wired_anim.c — Wired UI animation primitive implementation.
*/

#include "../../client.h"
#include "cl_wired_anim.h"

#if FEAT_WIRED_UI

#include <math.h>

LOG_DECLARE_CHANNEL( ch_ui, "ui" );

// ── Pool storage ────────────────────────────────────────────────────
static wuiAnim_t s_anims[ WUI_ANIM_POOL_MAX ];
static int       s_next_id  = 1;        /* monotonic id; 0 reserved for "none" */
static int       s_active   = 0;

// ── Curves ──────────────────────────────────────────────────────────
static float wui_curve_apply( wuiAnimCurve_t curve, float t ) {
	switch ( curve ) {
	case WUI_ANIM_CURVE_LINEAR:
		return t;
	case WUI_ANIM_CURVE_EASE_IN:
		return t * t;
	case WUI_ANIM_CURVE_EASE_OUT: {
		float u = 1.0f - t;
		return 1.0f - u * u;
	}
	case WUI_ANIM_CURVE_EASE_IN_OUT:
		return ( t < 0.5f ) ? ( 2.0f * t * t )
		                    : ( 1.0f - ( -2.0f * t + 2.0f ) * ( -2.0f * t + 2.0f ) * 0.5f );
	case WUI_ANIM_CURVE_LOOP_LINEAR:
		/* Caller has already wrapped t via fmod; this is the post-wrap
		 * identity. Separate enum entry exists so the curve dispatch
		 * matches the named-builtin registry semantics. */
		return t;
	case WUI_ANIM_CURVE_HALFSINE:
		/* Half-sine: t∈[0,1] → sin(t·π) ∈ [0,1,0]. Smooth ramp up to
		 * the midpoint and symmetric ramp back down. Two canonical
		 * uses: (a) LOOP_FLAG on → continuous brighten-dim pulse
		 * with no sawtooth seam (indicator dots); (b) LOOP_FLAG off →
		 * one-shot scale-in/out envelope (medal / popup / award
		 * elements). See enum header docs for the full rationale. */
		return (float) sin( t * M_PI );
	case WUI_ANIM_CURVE_STEP:
		/* Step-end: held at the start value (eased == 0) until the 50%
		 * mark, then snaps to the end value (eased == 1). The caller
		 * lerps `from + (to-from)*eased` over this so a blink animation
		 * with from=1, to=0 reads 1.0 for the first half of the cycle
		 * and 0.0 for the second. Paired with LOOP_FLAG = square wave. */
		return ( t < 0.5f ) ? 0.0f : 1.0f;
	}
	return t;
}

// ── Built-in named animations ───────────────────────────────────────
/* `scroll-x` LOOP_LINEAR — text horizontal scroll. from=1.0 means "start
 * at right edge of container", to=-1.0 "off left edge". Compositor
 * multiplies by container width at emit time. Default 12s loop period.
 *
 * `fade-in` / `fade-out` EASE_IN_OUT — alpha multiplier 0→1 / 1→0.
 *
 * `slide-up` / `slide-down` EASE_OUT — vertical pixel offset; from is
 * the off-screen origin (positive=below, negative=above), to=0 = final
 * position. Compositor adds the value to the item's emit y. */
static const wuiAnimBuiltin_t s_builtins[] = {
	{ "scroll-x",     1.0f, -1.0f, 12000, WUI_ANIM_CURVE_LOOP_LINEAR, WUI_ANIM_FLAG_LOOP },
	{ "fade-in",      0.0f,  1.0f,   400, WUI_ANIM_CURVE_EASE_IN_OUT, 0 },
	{ "fade-out",     1.0f,  0.0f,   400, WUI_ANIM_CURVE_EASE_IN_OUT, 0 },
	{ "slide-up",     0.10f, 0.0f,   500, WUI_ANIM_CURVE_EASE_OUT,    0 },
	{ "slide-down",  -0.10f, 0.0f,   500, WUI_ANIM_CURVE_EASE_OUT,    0 },
	/* `pulse` SINE LOOP — forecolor alpha modulation 0.3 → 1.0 → 0.3
	 * over a 2 s cycle. Bound by routing into item->animAlphaMul
	 * (same target slot as fade-in/out); see cl_wired_clay.c
	 * builtin-dispatch routing. */
	{ "pulse",        0.3f,  1.0f,  2000, WUI_ANIM_CURVE_HALFSINE,    WUI_ANIM_FLAG_LOOP },
	/* `blink` STEP LOOP — opacity 1.0 → 0.0 square wave over a 600ms
	 * cycle (300ms visible, 300ms hidden). Same animAlphaMul routing
	 * slot as fade-in/out/pulse; the alpha-gate site in cl_wired_clay
	 * recognises the name and applies the multiplier through to the
	 * forecolor alpha. */
	{ "blink",        1.0f,  0.0f,   600, WUI_ANIM_CURVE_STEP,        WUI_ANIM_FLAG_LOOP },
	/* `scan-y` LOOP_LINEAR — vertical sweep of a decoration strip across
	 * its container: the Claude-Design-v2 `qwscan` keyframe (translateY
	 * -100% → 100%, 3.5s linear infinite; qw-screens.jsx:891, the scanning
	 * beam over the ScreenCampaign map preview). Routes through the same
	 * animOffsetY emit slot as slide-up/slide-down, so from/to are
	 * normalized against viewport height and a -1 → 1 sweep crosses the
	 * full screen; a consumer scopes the beam to a panel by overriding
	 * `duration` and clamping the strip inside that panel. LOOP because
	 * the beam never settles. The three sibling v2 keyframes (qwgscan /
	 * qwglitch / qwdash) have NO engine analogue and are deliberately not
	 * modelled — see modfiles/docs/wired-ui-v2.md §"Animation curves". */
	{ "scan-y",      -1.0f,  1.0f,  3500, WUI_ANIM_CURVE_LOOP_LINEAR, WUI_ANIM_FLAG_LOOP },
	{ NULL, 0, 0, 0, 0, 0 }
};

const wuiAnimBuiltin_t *WUI_AnimFindBuiltin( const char *name ) {
	int i;
	if ( !name || !*name ) return NULL;
	for ( i = 0; s_builtins[ i ].name; i++ ) {
		if ( !Q_stricmp( s_builtins[ i ].name, name ) ) {
			return &s_builtins[ i ];
		}
	}
	return NULL;
}

// ── API ─────────────────────────────────────────────────────────────

int WUI_AnimCreate( const char *name, float *targetRef,
                    float from, float to,
                    int durationMs, wuiAnimCurve_t curve, int flags )
{
	int      slot;
	wuiAnim_t *a;

	if ( !targetRef ) return 0;
	if ( durationMs <= 0 ) durationMs = 1;  /* avoid /0 in tick */

	for ( slot = 0; slot < WUI_ANIM_POOL_MAX; slot++ ) {
		if ( s_anims[ slot ].id == 0 ) break;
	}
	if ( slot >= WUI_ANIM_POOL_MAX ) {
		Com_Log( SEV_WARN, LOG_CH(ch_ui),
			"WUI_AnimCreate: pool full (max %d) — dropping anim '%s'\n",
			WUI_ANIM_POOL_MAX, name ? name : "(unnamed)" );
		return 0;
	}

	a = &s_anims[ slot ];
	a->id          = s_next_id++;
	a->active      = qtrue;
	a->startTick   = cls.realtime;
	a->durationMs  = durationMs;
	a->curve       = curve;
	a->from        = from;
	a->to          = to;
	a->targetRef   = targetRef;
	a->flags       = flags;
	if ( name ) Q_strncpyz( a->name, name, sizeof( a->name ) );
	else        a->name[ 0 ] = '\0';

	/* Initialize target to `from` so the first emit sees a consistent
	 * starting value even if the tick hasn't fired yet. */
	*targetRef = from;

	s_active++;
	return a->id;
}

void WUI_AnimStop( int id )
{
	int i;
	if ( id <= 0 ) return;
	for ( i = 0; i < WUI_ANIM_POOL_MAX; i++ ) {
		if ( s_anims[ i ].id == id ) {
			memset( &s_anims[ i ], 0, sizeof( s_anims[ i ] ) );
			s_active--;
			return;
		}
	}
}

void WUI_AnimStopAll( void )
{
	memset( s_anims, 0, sizeof( s_anims ) );
	s_active = 0;
}

int WUI_AnimActiveCount( void )
{
	return s_active;
}

void WUI_AnimTick( int realtime )
{
	int   i;
	float raw, eased;

	for ( i = 0; i < WUI_ANIM_POOL_MAX; i++ ) {
		wuiAnim_t *a = &s_anims[ i ];
		if ( a->id == 0 ) continue;
		if ( !a->active ) continue;
		if ( a->flags & WUI_ANIM_FLAG_PAUSED ) continue;

		raw = (float)( realtime - a->startTick ) / (float) a->durationMs;

		if ( a->flags & WUI_ANIM_FLAG_LOOP ) {
			/* Wrap t into [0, 1) using fmod. For LOOP_LINEAR this is the
			 * scroll-marquee behaviour. Curves other than LOOP_LINEAR can
			 * still set LOOP — they'll snap back at the cycle boundary. */
			raw = (float) fmod( raw, 1.0 );
			if ( raw < 0 ) raw += 1.0f;
		} else {
			if ( raw >= 1.0f ) {
				/* Snap to final, mark inactive (slot stays allocated until
				 * StopAll or explicit Stop — modder may inspect post-anim
				 * state). */
				*( a->targetRef ) = a->to;
				a->active = qfalse;
				continue;
			}
			if ( raw < 0 ) raw = 0;
		}

		eased = wui_curve_apply( a->curve, raw );
		*( a->targetRef ) = a->from + ( a->to - a->from ) * eased;
	}
}

#endif /* FEAT_WIRED_UI */
