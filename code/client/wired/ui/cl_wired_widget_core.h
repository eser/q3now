/*
 * cl_wired_widget_core.h — WiredUI component-library FRAMEWORK CORE (F1)
 *
 * Single home for the generic, type-agnostic interaction behaviour shared by
 * every WiredUI control: focus-vs-hover provenance, enable/disable policy, the
 * 5-state visual-state resolver, state→colour resolution, the keyboard focus
 * ring, and the key→intent + per-type-arm dispatch tables.
 *
 * DESIGN (ratified): hybrid-state-core. This module OWNS framework policy but
 * emits no Clay and reads no keys directly. It is consulted by the input path
 * (cl_wired_ui.c) and the render path (cl_wired_clay.c). The renderer keeps its
 * bespoke per-type Clay switch (each control's geometry is legitimately
 * hand-authored); only the INPUT switch dissolves into the arm table.
 *
 * Storage discipline: NO new focus/hover pointers — the authoritative
 * wui_focusedItemPtr / wui_hoveredItemPtr already exist in cl_wired_ui.c and are
 * reached through accessors (WiredUI_GetFocusedItem / WiredUI_GetHoveredItem).
 * Visual state is DERIVED per-frame, never stored. Per-item state colours live
 * in an OPTIONAL side-struct allocated only when a hover/pressed/disabled colour
 * keyword is authored — existing menus cost zero extra bytes.
 */

#ifndef CL_WIRED_WIDGET_CORE_H
#define CL_WIRED_WIDGET_CORE_H

#include "../../../qcommon/q_shared.h"

struct wiredItemDef_s;
struct wiredMenuDef_s;

/* ───────────────────────────────────────────────────────────────────────────
 * Interaction state (NEW storage only). Focus/hover authorities stay in
 * cl_wired_ui.c; this adds the press channel + keyboard-focus provenance so the
 * ring draws only on keyboard focus and PRESSED paints between down and up.
 * ─────────────────────────────────────────────────────────────────────────── */
typedef struct {
	struct wiredItemDef_s *pressTarget;       /* item armed between down and up */
	int                    pressKey;          /* K_MOUSE1 | K_SPACE | K_ENTER that armed it */
	qboolean               focusFromKeyboard; /* qtrue => keyboard focus => draw RING */

	/* Type-to-jump (listbox incremental search). A run of printable keys
	 * typed within WUI_TYPEAHEAD_RESET_MS of each other accumulates into
	 * typeBuf; the listbox jumps its selection to the first row whose leading
	 * text matches the buffer. A gap longer than the window resets the buffer
	 * so a fresh keystroke starts a new search. Bound to the focused item so a
	 * focus change clears it. */
	struct wiredItemDef_s *typeTarget;        /* listbox the buffer is searching */
	char                   typeBuf[32];       /* accumulated prefix (lowercased) */
	int                    typeLen;           /* chars in typeBuf */
	int                    typeLastMs;        /* cls.realtime of last typed key */
} wuiInteractionState_t;

/* Type-to-jump: keystrokes further apart than this reset the search buffer. */
#define WUI_TYPEAHEAD_RESET_MS 900

extern wuiInteractionState_t wui_ix;

void WiredUI_WidgetCoreReset( void );  /* clears wui_ix (menu teardown / init) */

/* ───────────────────────────────────────────────────────────────────────────
 * Enable/disable — orthogonal to visibility. WiredUI_ItemIsEnabled tests ONLY
 * the enable/disableCvar rules (the boolean formerly computed inside
 * WiredUI_ItemVisibleByCvarRules and thrown away as `return qfalse`).
 * ─────────────────────────────────────────────────────────────────────────── */
qboolean WiredUI_ItemIsEnabled( const struct wiredItemDef_s *item );

/* ───────────────────────────────────────────────────────────────────────────
 * 5-state model — derived per-frame, precedence DISABLED > PRESSED > FOCUSED >
 * HOVER > RESTING. `isActive` (cvar-selected) is a SEPARATE orthogonal axis kept
 * in the renderer; interaction state only adds ring/hover/press on top.
 * ─────────────────────────────────────────────────────────────────────────── */
typedef enum {
	WUI_STATE_RESTING = 0,
	WUI_STATE_HOVER,      /* pointer over item, not keyboard-focused */
	WUI_STATE_FOCUSED,    /* keyboard focus (draws ring), not pressed */
	WUI_STATE_PRESSED,    /* pressTarget == item (mouse-down / Space held) */
	WUI_STATE_DISABLED,   /* enableMode dim + !IsEnabled */
} wuiVisualState_t;

wuiVisualState_t WiredUI_ItemVisualState( const struct wiredMenuDef_s *panel,
                                          const struct wiredItemDef_s *item );

/* Resolve fg/bg/border for a given interaction state. Fallback chain
 * PRESSED→FOCUSED→HOVER→base preserves every existing menu: a menu authoring
 * only base + `.active` renders identically until it opts into a state keyword.
 * outFg/outBg/outBorder may be NULL to skip. Returns qtrue if any override was
 * applied (caller may keep its own base otherwise). */
qboolean WiredUI_ResolveStateColors( const struct wiredItemDef_s *item,
                                     wuiVisualState_t st,
                                     vec4_t outFg, vec4_t outBg, vec4_t outBorder );

/* ───────────────────────────────────────────────────────────────────────────
 * Focus ring — real border emit, distinct from the hover FILL gradient.
 * Returns qtrue iff item is the keyboard-focused item (hover never rings).
 * ─────────────────────────────────────────────────────────────────────────── */
qboolean WiredUI_FocusRingFor( const struct wiredMenuDef_s *panel,
                               const struct wiredItemDef_s *item,
                               vec4_t outColor, float *outThicknessPx );

/* ───────────────────────────────────────────────────────────────────────────
 * SPINNER value math — single source for the numeric-stepper step/clamp/adjust
 * semantics. SPINNER reuses wiredSliderDef_t (def/min/max) + its `step` field.
 * Consumed by the input path (keyboard/button/hold) AND the wheel compositor so
 * every adjustment route agrees. step<=0 falls back to range/20 (0.01 floor).
 * ─────────────────────────────────────────────────────────────────────────── */
float WiredUI_SpinnerStep    ( const struct wiredItemDef_s *item );
float WiredUI_SpinnerClamp   ( const struct wiredItemDef_s *item, float v );
float WiredUI_SpinnerValue   ( const struct wiredItemDef_s *item );
float WiredUI_SpinnerFraction( const struct wiredItemDef_s *item );
void  WiredUI_SpinnerAdjust  ( struct wiredItemDef_s *item, int dir, float mult );
void  WiredUI_SpinnerSetExtreme( struct wiredItemDef_s *item, qboolean toMax );

/* ───────────────────────────────────────────────────────────────────────────
 * Keyboard-nav: key→intent (Table A, resolved once) + per-type arm (Table B,
 * a plain function table keyed by ITEM_TYPE_* — NO per-item vtable).
 * ─────────────────────────────────────────────────────────────────────────── */
typedef enum {
	WUI_INTENT_NONE = 0,
	WUI_INTENT_ACTIVATE,
	WUI_INTENT_INC,
	WUI_INTENT_DEC,
	WUI_INTENT_PAGE_INC,
	WUI_INTENT_PAGE_DEC,
	WUI_INTENT_HOME,
	WUI_INTENT_END,
	WUI_INTENT_NEXT,
	WUI_INTENT_PREV,
	WUI_INTENT_CONTEXT,
	WUI_INTENT_CANCEL,
} wuiIntent_t;

/* Raw key → intent. `shift` selects PREV for K_TAB. Returns WUI_INTENT_NONE for
 * keys with no framework meaning (caller keeps its legacy handling). */
wuiIntent_t WiredUI_KeyToIntent( int key, qboolean shift );

/* Per-type arm. onIntent handles the VALUE intents (ACTIVATE/INC/DEC/CONTEXT)
 * for one control type; returns qtrue if it consumed the intent. */
typedef struct {
	int      type;
	qboolean (*onIntent)( struct wiredMenuDef_s *, struct wiredItemDef_s *, wuiIntent_t );
	qboolean firesOnRelease;   /* ACTIVATE fires on key-UP (button/checkbox/radio) */
} wuiWidgetArm_t;

/* Registry lookup. NULL => the type has no value-arm (nav-only item). */
const wuiWidgetArm_t *WiredUI_ArmForType( int type );

/* Register (or replace) a per-type arm. Called from cl_wired_ui.c at init — the
 * arm bodies live there because they need the file-static value-mutation helpers
 * (WiredUI_StateSetString, multi-dropdown machinery, …). */
void WiredUI_RegisterArm( int type,
                          qboolean (*onIntent)( struct wiredMenuDef_s *,
                                                struct wiredItemDef_s *, wuiIntent_t ),
                          qboolean firesOnRelease );

#endif /* CL_WIRED_WIDGET_CORE_H */
