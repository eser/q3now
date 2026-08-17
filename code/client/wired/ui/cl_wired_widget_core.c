/*
 * cl_wired_widget_core.c — WiredUI component-library FRAMEWORK CORE (F1)
 *
 * See cl_wired_widget_core.h for the design rationale. This TU owns:
 *   - wui_ix (press channel + keyboard-focus provenance)
 *   - WiredUI_ItemIsEnabled       (enable/disable policy, orthogonal to hide)
 *   - WiredUI_ItemVisualState     (5-state resolver, derived per-frame)
 *   - WiredUI_ResolveStateColors  (state → fg/bg/border, fallback-preserving)
 *   - WiredUI_FocusRingFor        (keyboard-focus ring, distinct from hover fill)
 *   - WiredUI_KeyToIntent         (Table A: raw key → intent)
 *   - WiredUI_ArmForType          (Table B: per-type value arm, no vtable)
 *
 * It emits no Clay and reads no keys. Focus/hover authorities live in
 * cl_wired_ui.c and are reached via WiredUI_GetFocusedItem/GetHoveredItem.
 */

#include "../../client.h"
#include "cl_wired_ui.h"
#include "cl_wired_widget_core.h"
#include "cl_wired_compositor.h"      /* WiredUI_GetDpiScale */
#include "../../../qcommon/menudef.h"
#include "../../keycodes.h"
#include <stdio.h>                    /* sscanf for token colour parsing */

/* Live theme token table lookup (defined in cl_wired_parse.c). */
extern const char *WiredToken_Find( const char *name );

/* Set outColor RGB from a theme token (alpha preserved by caller) so the focus
 * ring stays theme-driven and matches the menu focuscolor default (also
 * $accent — see WiredUI_ParseMenu).
 *
 * FIX 2026-08-17: this read $primary_cyan, a v1 token no v2 accent overlay
 * rewrites, so the keyboard focus ring stayed cyan under every accent. Worse,
 * the menu focuscolor default it claims to match had already been repointed at
 * $accent, so the two drifted apart: a menu that authored no focuscolor drew an
 * amber wash behind a cyan ring. Read $accent and fall back to the shipped
 * amber default. */
static void wui_widget_focus_ring_color( vec4_t outColor ) {
	const char *v = WiredToken_Find( "accent" );
	unsigned    r, g, b;
	/* baked $accent #f4a03a fallback */
	outColor[0] = 0.957f; outColor[1] = 0.627f; outColor[2] = 0.227f; outColor[3] = 1.0f;
	if ( v && v[0] == '#' && ( strlen( v ) == 7 || strlen( v ) == 9 )
	     && sscanf( v + 1, "%2x%2x%2x", &r, &g, &b ) == 3 ) {
		outColor[0] = (float) r / 255.0f;
		outColor[1] = (float) g / 255.0f;
		outColor[2] = (float) b / 255.0f;
	}
}

/* ── interaction state (the only NEW framework storage) ───────────────────── */
wuiInteractionState_t wui_ix = { NULL, 0, qfalse };

void WiredUI_WidgetCoreReset( void ) {
	wui_ix.pressTarget       = NULL;
	wui_ix.pressKey          = 0;
	wui_ix.focusFromKeyboard = qfalse;
	wui_ix.typeTarget        = NULL;
	wui_ix.typeBuf[0]        = '\0';
	wui_ix.typeLen           = 0;
	wui_ix.typeLastMs        = 0;
}

/* ── enable/disable ───────────────────────────────────────────────────────────
 * The exact boolean formerly computed inside WiredUI_ItemVisibleByCvarRules
 * (cl_wired_ui.c) and discarded as `return qfalse`. Extracted verbatim so the
 * enable/disableCvar semantics are byte-identical; visibility keeps ONLY
 * show/hide after the extraction. Uses the WiredUI_StateGetString +
 * WiredUI_StateListContainsValue helpers exposed from cl_wired_ui.c.
 * ─────────────────────────────────────────────────────────────────────────── */
qboolean WiredUI_ItemIsEnabled( const wiredItemDef_t *item ) {
	if ( !item ) return qtrue;

	if ( item->enableCvar[0] || item->disableCvar[0] ) {
		char     testBuf[256];
		qboolean enabled = qtrue;

		if ( item->cvarTest[0] )    WiredUI_StateGetString( item->cvarTest, testBuf, sizeof( testBuf ) );
		else if ( item->cvar[0] )   WiredUI_StateGetString( item->cvar, testBuf, sizeof( testBuf ) );
		else                        testBuf[0] = '\0';

		if ( item->enableCvar[0] ) {
			enabled = WiredUI_StateListContainsValue( item->enableCvar, testBuf );
		}
		if ( item->disableCvar[0] ) {
			if ( WiredUI_StateListContainsValue( item->disableCvar, testBuf ) ) {
				enabled = qfalse;
			}
		}
		return enabled;
	}

	return qtrue;
}

/* ── 5-state resolver ─────────────────────────────────────────────────────────
 * Precedence: DISABLED > PRESSED > FOCUSED > HOVER > RESTING. The cvar-selected
 * `.active` axis is ORTHOGONAL and stays in the renderer — this returns only the
 * interaction axis (the renderer folds in isActive on top).
 * ─────────────────────────────────────────────────────────────────────────── */
wuiVisualState_t WiredUI_ItemVisualState( const wiredMenuDef_t *panel,
                                          const wiredItemDef_t *item ) {
	const wiredItemDef_t *foc;
	const wiredItemDef_t *hov;
	(void) panel;

	if ( !item ) return WUI_STATE_RESTING;

	/* DISABLED wins outright — a disabled control never lights under the cursor.
	 * Only `dim`-mode items reach the renderer at all; `hide`-mode disabled
	 * items are culled by visibility upstream, so they never get here. */
	if ( item->enableModeDim && !WiredUI_ItemIsEnabled( item ) ) {
		return WUI_STATE_DISABLED;
	}

	if ( wui_ix.pressTarget == item ) {
		return WUI_STATE_PRESSED;
	}

	foc = WiredUI_GetFocusedItem();
	if ( foc == item && wui_ix.focusFromKeyboard ) {
		return WUI_STATE_FOCUSED;
	}

	hov = WiredUI_GetHoveredItem();
	if ( hov == item ) {
		return WUI_STATE_HOVER;
	}

	return WUI_STATE_RESTING;
}

/* ── state → colours ─────────────────────────────────────────────────────────
 * Fallback chain preserves every existing menu: a control with no stateColors
 * side-struct returns qfalse for every non-disabled state (caller keeps its own
 * base / .active colour). DISABLED dims the caller's base when no explicit
 * disabled colour was authored.
 * ─────────────────────────────────────────────────────────────────────────── */
qboolean WiredUI_ResolveStateColors( const wiredItemDef_t *item,
                                     wuiVisualState_t st,
                                     vec4_t outFg, vec4_t outBg, vec4_t outBorder ) {
	const wuiStateColors_t *sc;
	qboolean applied = qfalse;

	if ( !item ) return qfalse;
	sc = item->stateColors;

	/* DISABLED: explicit colour if authored, else caller keeps base and we
	 * signal the dim (caller multiplies alpha). We express dim by not touching
	 * the colour but returning qtrue so the caller can apply the alpha dim. The
	 * renderer applies the 0.4 alpha multiply for DISABLED regardless. */
	if ( st == WUI_STATE_DISABLED ) {
		if ( sc && sc->hasDisabled ) {
			if ( outFg )     Vector4Copy( sc->disabledFg, outFg );
			if ( outBg )     Vector4Copy( sc->disabledBg, outBg );
			if ( outBorder ) Vector4Copy( sc->disabledBorder, outBorder );
			return qtrue;
		}
		return qfalse; /* caller keeps base; renderer applies alpha dim */
	}

	if ( !sc ) return qfalse;

	/* PRESSED → FOCUSED → HOVER fallback: pick the most specific authored
	 * variant at or below the requested state. */
	switch ( st ) {
	case WUI_STATE_PRESSED:
		if ( sc->hasPressed ) {
			if ( outFg )     Vector4Copy( sc->pressedFg, outFg );
			if ( outBg )     Vector4Copy( sc->pressedBg, outBg );
			if ( outBorder ) Vector4Copy( sc->pressedBorder, outBorder );
			return qtrue;
		}
		/* fall through to FOCUSED */
	case WUI_STATE_FOCUSED:
		/* FOCUSED colour is optional — the ring is the primary focus signal.
		 * If a focused colour is authored use it, else fall to hover, else base. */
		if ( sc->hasFocused ) {
			if ( outFg )     Vector4Copy( sc->focusedFg, outFg );
			if ( outBg )     Vector4Copy( sc->focusedBg, outBg );
			if ( outBorder ) Vector4Copy( sc->focusedBorder, outBorder );
			return qtrue;
		}
		/* fall through to HOVER */
	case WUI_STATE_HOVER:
		if ( sc->hasHover ) {
			if ( outFg )     Vector4Copy( sc->hoverFg, outFg );
			if ( outBg )     Vector4Copy( sc->hoverBg, outBg );
			if ( outBorder ) Vector4Copy( sc->hoverBorder, outBorder );
			return qtrue;
		}
		break;
	default:
		break;
	}

	return applied;
}

/* ── focus ring ───────────────────────────────────────────────────────────────
 * True iff item is the keyboard-focused item (hover never draws a ring). Colour
 * defaults from menu->focuscolor; thickness is physical px (caller multiplies by
 * dpiScale, or we do it here for a single source of truth).
 * ─────────────────────────────────────────────────────────────────────────── */
qboolean WiredUI_FocusRingFor( const wiredMenuDef_t *panel,
                               const wiredItemDef_t *item,
                               vec4_t outColor, float *outThicknessPx ) {
	const wiredItemDef_t *foc;

	if ( !item ) return qfalse;
	if ( !wui_ix.focusFromKeyboard ) return qfalse;

	foc = WiredUI_GetFocusedItem();
	if ( foc != item ) return qfalse;

	if ( outColor ) {
		if ( panel ) {
			Vector4Copy( panel->focuscolor, outColor );
		} else {
			/* theme accent ($accent), theme-driven */
			wui_widget_focus_ring_color( outColor );
		}
		/* focuscolor may be authored as {0,0,0,0}; give a sane default so the
		 * ring is visible even on menus that never set focuscolor. */
		if ( outColor[3] <= 0.0f ) {
			/* theme accent ($accent), theme-driven */
			wui_widget_focus_ring_color( outColor );
		}
	}
	if ( outThicknessPx ) {
		float dpi = WiredUI_GetDpiScale();
		if ( dpi <= 0.0f ) dpi = 1.0f;
		*outThicknessPx = 2.0f * dpi;   /* physical px */
	}
	return qtrue;
}

/* ── spinner value math (single source) ──────────────────────────────────────
 * SPINNER shares wiredSliderDef_t (def/min/max) and adds `step`. These helpers
 * are the ONE place the step + clamp semantics live, consumed by the input path
 * (keyboard INC/DEC/HOME/END, +/- button click, click-and-hold repeat) AND the
 * wheel-adjust compositor path — so every adjustment route agrees. `step<=0`
 * falls back to range/20 (mirrors the slider's derived step), 0.01 floor.
 * ─────────────────────────────────────────────────────────────────────────── */
float WiredUI_SpinnerStep( const wiredItemDef_t *item ) {
	float step;
	if ( !item ) return 1.0f;
	step = item->sliderData.step;
	if ( step <= 0.0f ) {
		step = ( item->sliderData.maxVal - item->sliderData.minVal ) / 20.0f;
		if ( step < 0.01f ) step = 0.01f;
	}
	return step;
}

/* Clamp v to [min,max]. */
float WiredUI_SpinnerClamp( const wiredItemDef_t *item, float v ) {
	if ( !item ) return v;
	if ( v < item->sliderData.minVal ) v = item->sliderData.minVal;
	if ( v > item->sliderData.maxVal ) v = item->sliderData.maxVal;
	return v;
}

/* Current cvar value read back + clamped to range. */
float WiredUI_SpinnerValue( const wiredItemDef_t *item ) {
	char buf[64];
	if ( !item || !item->cvar[0] ) return 0.0f;
	WiredUI_StateGetString( item->cvar, buf, sizeof( buf ) );
	return WiredUI_SpinnerClamp( item, atof( buf ) );
}

/* Fraction 0..1 of value across [min,max] — for a progress underlay if desired. */
float WiredUI_SpinnerFraction( const wiredItemDef_t *item ) {
	float range;
	if ( !item ) return 0.0f;
	range = item->sliderData.maxVal - item->sliderData.minVal;
	if ( range <= 0.0f ) return 0.0f;
	return ( WiredUI_SpinnerValue( item ) - item->sliderData.minVal ) / range;
}

/* Apply `dir` (+1/-1) steps of `mult` step-units, clamp, and write the cvar.
 * mult>1 gives the PageUp/Down coarse step. Writes with %g so integer ranges
 * print cleanly ("50", not "50.000000"). */
void WiredUI_SpinnerAdjust( wiredItemDef_t *item, int dir, float mult ) {
	float val, step;
	if ( !item || !item->cvar[0] ) return;
	val  = WiredUI_SpinnerValue( item );
	step = WiredUI_SpinnerStep( item ) * ( mult > 0.0f ? mult : 1.0f );
	val += step * (float) dir;
	val  = WiredUI_SpinnerClamp( item, val );
	WiredUI_StateSetString( item->cvar, va( "%g", val ) );
}

/* Set to min (HOME) / max (END). */
void WiredUI_SpinnerSetExtreme( wiredItemDef_t *item, qboolean toMax ) {
	if ( !item || !item->cvar[0] ) return;
	WiredUI_StateSetString( item->cvar,
		va( "%g", toMax ? item->sliderData.maxVal : item->sliderData.minVal ) );
}

/* ── Table A: raw key → intent ───────────────────────────────────────────────
 * Resolved ONCE for all controls. Closes the Space/Page/Home/End holes in one
 * place. WUI_INTENT_NONE => the key has no framework meaning and the caller
 * keeps its legacy per-key handling.
 * ─────────────────────────────────────────────────────────────────────────── */
wuiIntent_t WiredUI_KeyToIntent( int key, qboolean shift ) {
	switch ( key ) {
	case K_SPACE:
		return WUI_INTENT_ACTIVATE;
	case K_ENTER:
	case K_KP_ENTER:
		return WUI_INTENT_ACTIVATE;
	case K_RIGHTARROW:
	case K_KP_RIGHTARROW:
		return WUI_INTENT_INC;
	case K_LEFTARROW:
	case K_KP_LEFTARROW:
		return WUI_INTENT_DEC;
	case K_DOWNARROW:
	case K_KP_DOWNARROW:
		return WUI_INTENT_NEXT;
	case K_UPARROW:
	case K_KP_UPARROW:
		return WUI_INTENT_PREV;
	case K_PGDN:
		return WUI_INTENT_PAGE_INC;
	case K_PGUP:
		return WUI_INTENT_PAGE_DEC;
	case K_HOME:
		return WUI_INTENT_HOME;
	case K_END:
		return WUI_INTENT_END;
	case K_TAB:
		return shift ? WUI_INTENT_PREV : WUI_INTENT_NEXT;
	case K_MOUSE2:
		return WUI_INTENT_CONTEXT;
	case K_ESCAPE:
		return WUI_INTENT_CANCEL;
	default:
		return WUI_INTENT_NONE;
	}
}

/* ── Table B: per-type arm registry ──────────────────────────────────────────
 * A plain static function table keyed by ITEM_TYPE_* — NOT a per-item vtable.
 * Arm bodies live in cl_wired_ui.c (they need the file-static value-mutation
 * helpers there) and register themselves at init via WiredUI_RegisterArm. The
 * registry stays here so the dispatch policy has one home.
 * ─────────────────────────────────────────────────────────────────────────── */
#define WUI_MAX_ARMS  32
static wuiWidgetArm_t wui_arms[ WUI_MAX_ARMS ];
static int            wui_armCount = 0;

void WiredUI_RegisterArm( int type,
                          qboolean (*onIntent)( wiredMenuDef_t *, wiredItemDef_t *, wuiIntent_t ),
                          qboolean firesOnRelease ) {
	int i;
	for ( i = 0; i < wui_armCount; i++ ) {
		if ( wui_arms[ i ].type == type ) {
			wui_arms[ i ].onIntent       = onIntent;
			wui_arms[ i ].firesOnRelease = firesOnRelease;
			return;
		}
	}
	if ( wui_armCount >= WUI_MAX_ARMS ) return;
	wui_arms[ wui_armCount ].type           = type;
	wui_arms[ wui_armCount ].onIntent       = onIntent;
	wui_arms[ wui_armCount ].firesOnRelease = firesOnRelease;
	wui_armCount++;
}

const wuiWidgetArm_t *WiredUI_ArmForType( int type ) {
	int i;
	for ( i = 0; i < wui_armCount; i++ ) {
		if ( wui_arms[ i ].type == type ) {
			return &wui_arms[ i ];
		}
	}
	return NULL;
}
