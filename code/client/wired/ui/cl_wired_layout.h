// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
cl_wired_layout.h — Wired UI: resolution-independent coordinate types
*/

#ifndef CL_WIRED_LAYOUT_H
#define CL_WIRED_LAYOUT_H

#include "../../../qcommon/q_shared.h"

// -- Positioning mode (like CSS position) ---------------------------------
typedef enum {
	POSITION_STATIC = 0,   // default — participates in flex layout
	POSITION_ABSOLUTE,     // positioned relative to parent, skipped by flex
	POSITION_VIEWPORT      // positioned relative to viewport, skipped by flex
} wuiPosition_t;

// -- Coordinate unit types ------------------------------------------------
typedef enum {
	UNIT_NORM = 0,   // fraction of parent (0.0-1.0)
	UNIT_VW,         // fraction of viewport width (0-100)
	UNIT_VH,         // fraction of viewport height (0-100)
	UNIT_PX,         // real device pixels
	UNIT_AUTO,       // size determined by content (children or text)
	// Multiple of the UI root scale (WiredUI_GetRootScale), then dpiScale —
	// the model CSS rem uses. Appended last on purpose: the values above are
	// what already-authored menus carry, and inserting ahead of them would
	// silently reinterpret existing content.
	UNIT_REM
} wuiUnit_t;

// -- Unit-aware value -----------------------------------------------------
typedef struct {
	float       value;
	wuiUnit_t   unit;
} wuiValue_t;

// -- Unit-aware rectangle -------------------------------------------------
typedef struct {
	wuiValue_t  x, y, w, h;
} wuiRect_t;

// -- CSS-style per-side anchor offsets ------------------------------------
// Dispatch 5.5: standalone top/left/right/bottom on an item. Auto-promotes
// the item to POSITION_ABSOLUTE in Clay-emit when any side is declared. Sides
// not declared (hasX = qfalse) fall through to rect-based positioning.
typedef struct {
	wuiValue_t  top, right, bottom, left;
	qboolean    hasTop, hasRight, hasBottom, hasLeft;
} wuiOffset_t;

// -- CSS-style per-side margin (outside the box) --------------------------
// Dispatch 5.5: margin on a flex-child item. Contributes to parent's flex
// gap on the relevant axis (Clay v0.14 has no per-child margin primitive;
// we emulate by adjusting the item's resolvedRect when emitted).
typedef struct {
	wuiValue_t  top, right, bottom, left;
	qboolean    hasAny;
} wuiMargin_t;

// -- Aspect ratio constraint ----------------------------------------------
typedef struct {
	float       ratio;      // w/h (e.g. 1.0 for square, 1.777 for 16:9)
	qboolean    active;
} wuiAspect_t;

// -- Flex container properties --------------------------------------------
typedef enum {
	WUI_LAYOUT_NONE = 0,
	WUI_LAYOUT_ROW,
	WUI_LAYOUT_COLUMN
} wuiLayoutDir_t;

typedef enum {
	WUI_ALIGN_START = 0,
	WUI_ALIGN_CENTER,
	WUI_ALIGN_END,
	WUI_ALIGN_STRETCH
} wuiAlign_t;

typedef enum {
	WUI_JUSTIFY_START = 0,
	WUI_JUSTIFY_CENTER,
	WUI_JUSTIFY_END,
	WUI_JUSTIFY_SPACE_BETWEEN
} wuiJustify_t;

typedef struct {
	wuiLayoutDir_t  direction;
	wuiValue_t      gap;
	wuiValue_t      padding[4];  // top, right, bottom, left
	wuiAlign_t      align;
	wuiJustify_t    justify;
	qboolean        wrap;
	qboolean        scroll;   // clip children to the box + wheel/drag scroll-offset (Clay .clip.vertical)
} wuiFlexContainer_t;

// -- Flex child properties ------------------------------------------------
typedef struct {
	float           grow;       // flex-grow (default 0)
	float           shrink;     // flex-shrink (default 1)
	wuiValue_t      basis;      // flex-basis (initial size on main axis)
	wuiAlign_t      alignSelf;  // override parent align
	wuiValue_t      minWidth, maxWidth;
	wuiValue_t      minHeight, maxHeight;
} wuiFlexChild_t;

// -- Animation easing types (Layer 5) ------------------------------------
typedef enum {
	WUI_EASE_LINEAR = 0,
	WUI_EASE_IN,        // ease-in (slow start)
	WUI_EASE_OUT,       // ease-out (slow end)
	WUI_EASE_IN_OUT     // ease-in-out
} wuiEasing_t;

// -- Transition animation on layout rect (Layer 5) -----------------------
typedef struct {
	wuiRect_t   from;       // start rect
	wuiRect_t   to;         // target rect
	int         startTime;  // cls.realtime when started (0 = inactive)
	int         duration;   // ms
	wuiEasing_t easing;
} wuiTransition_t;

// -- Responsive breakpoint (Layer 5) -------------------------------------
#define WUI_MAX_BREAKPOINTS 8

typedef struct {
	int         minWidth;   // viewport width >= this (0 = no min)
	int         maxWidth;   // viewport width <= this (0 = no max)
	wuiRect_t   rect;       // override rect when breakpoint matches
	qboolean    active;
} wuiBreakpoint_t;

// -- Resolved pixel rect (output of layout engine) -----------------------
typedef struct {
	float x, y, w, h;
} wuiPixelRect_t;

// -- Resolve function -----------------------------------------------------
// Convert any wuiValue_t to real pixels
float WUI_Resolve( wuiValue_t val, float parentSizePx, float vpWidth, float vpHeight );

// -- Authored coordinate helpers ------------------------------------------

// Resolve a wuiRect_t to pixel coordinates
wuiPixelRect_t WUI_ResolveRect( const wuiRect_t *rect, const wuiPixelRect_t *parent,
                                 float vpWidth, float vpHeight );

// -- Transition animation functions (Layer 5) -----------------------------

// Interpolate a transition at current time, returning the blended rect
wuiRect_t WUI_TransitionEval( const wuiTransition_t *tr, int currentTime );

// -- Responsive breakpoint functions (Layer 5) ----------------------------

// Find matching breakpoint for current viewport width (last match wins)
const wuiRect_t *WUI_FindBreakpointRect( const wuiBreakpoint_t *bps, int count, int vpWidth );

// -- Clay layout snapshot instrumentation ---------------------------------
// Forward-declare menu type (defined in cl_wired_ui.h)
struct wiredMenuDef_s;

// Visual-regression instrumentation: dump every named item's resolved
// pixel rect + authored colours to layoutdump.jsonl when the r_layoutDump
// cvar is non-zero. No-op (a single cvar lookup) when disabled. Invoked after
// Clay_EndLayout, so every available item rect is Clay-authored. Defined in
// cl_wired_layout_dump.c.
void WUI_DumpLayout( const struct wiredMenuDef_s *menu );

/* Read-only native inspector protocol. `wui_inspect <menu> [item|*]` selects
 * an authored tree; each completed Clay layout appends schema-versioned tree,
 * computed-layout, style, binding-provenance and focus/hit-test records to
 * wiredui-inspector.jsonl. It never owns or mutates UI/store/layout state. */
void WUI_InspectorInit( void );
void WUI_InspectorShutdown( void );

// Convenience: make a wuiValue_t
static ID_INLINE wuiValue_t WUI_Val( float v, wuiUnit_t u ) {
	wuiValue_t r;
	r.value = v;
	r.unit = u;
	return r;
}

// Convenience: make a UNIT_NORM value (most common)
static ID_INLINE wuiValue_t WUI_Norm( float v ) {
	return WUI_Val( v, UNIT_NORM );
}

#endif // CL_WIRED_LAYOUT_H
