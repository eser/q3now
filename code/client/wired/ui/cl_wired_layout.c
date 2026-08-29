// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
cl_wired_layout.c — Wired UI coordinate and authored-value helpers

Clay is the sole descendant layout engine. This file intentionally contains no
tree walk or flex algorithm; it only resolves authored units/rectangles plus
the format-level transition and breakpoint value helpers.
*/

#include "cl_wired_layout.h"
#include "cl_wired_compositor.h"

float WUI_Resolve( wuiValue_t val, float parentSizePx, float vpWidth, float vpHeight ) {
	switch ( val.unit ) {
		case UNIT_VW:   return ( val.value / 100.0f ) * vpWidth;
		case UNIT_VH:   return ( val.value / 100.0f ) * vpHeight;
		/* Authored px are logical points; Clay and MSDF operate in physical
		 * pixels, so geometry and text use the same DPI multiplier. */
		case UNIT_PX:   return val.value * WiredUI_GetDpiScale();
		case UNIT_REM:  return val.value * WiredUI_GetRootScale() * WiredUI_GetDpiScale();
		case UNIT_AUTO: return 0.0f;  /* Clay FIT/GROW resolves AUTO. */
		case UNIT_NORM:
		default:        return val.value * parentSizePx;
	}
}

wuiPixelRect_t WUI_ResolveRect( const wuiRect_t *rect, const wuiPixelRect_t *parent,
								 float vpWidth, float vpHeight ) {
	wuiPixelRect_t out;
	out.x = parent->x + WUI_Resolve( rect->x, parent->w, vpWidth, vpHeight );
	out.y = parent->y + WUI_Resolve( rect->y, parent->h, vpWidth, vpHeight );
	out.w = WUI_Resolve( rect->w, parent->w, vpWidth, vpHeight );
	out.h = WUI_Resolve( rect->h, parent->h, vpWidth, vpHeight );
	return out;
}

static float WUI_Ease( float t, wuiEasing_t easing ) {
	switch ( easing ) {
		case WUI_EASE_IN:     return t * t;
		case WUI_EASE_OUT:    return t * ( 2.0f - t );
		case WUI_EASE_IN_OUT: return t < 0.5f ? 2 * t * t : -1 + ( 4 - 2 * t ) * t;
		case WUI_EASE_LINEAR:
		default:              return t;
	}
}

wuiRect_t WUI_TransitionEval( const wuiTransition_t *tr, int currentTime ) {
	wuiRect_t result;
	float t, e;

	if ( !tr->startTime || !tr->duration ) return tr->to;
	t = (float)( currentTime - tr->startTime ) / (float)tr->duration;
	if ( t <= 0.0f ) return tr->from;
	if ( t >= 1.0f ) return tr->to;
	e = WUI_Ease( t, tr->easing );

	result.x.value = tr->from.x.value + ( tr->to.x.value - tr->from.x.value ) * e;
	result.x.unit  = tr->to.x.unit;
	result.y.value = tr->from.y.value + ( tr->to.y.value - tr->from.y.value ) * e;
	result.y.unit  = tr->to.y.unit;
	result.w.value = tr->from.w.value + ( tr->to.w.value - tr->from.w.value ) * e;
	result.w.unit  = tr->to.w.unit;
	result.h.value = tr->from.h.value + ( tr->to.h.value - tr->from.h.value ) * e;
	result.h.unit  = tr->to.h.unit;
	return result;
}

const wuiRect_t *WUI_FindBreakpointRect( const wuiBreakpoint_t *bps, int count, int vpWidth ) {
	int i;
	for ( i = count - 1; i >= 0; i-- ) {
		if ( !bps[i].active ) continue;
		if ( bps[i].minWidth && vpWidth < bps[i].minWidth ) continue;
		if ( bps[i].maxWidth && vpWidth > bps[i].maxWidth ) continue;
		return &bps[i].rect;
	}
	return NULL;
}
