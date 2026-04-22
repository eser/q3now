/*
cl_wired_layout.c — Wired UI: resolution-independent coordinate resolver
*/

#include "cl_wired_layout.h"
#include "cl_wired_ui.h"

float WUI_Resolve( wuiValue_t val, float parentSizePx, float vpWidth, float vpHeight ) {
	switch ( val.unit ) {
		case UNIT_VW:   return ( val.value / 100.0f ) * vpWidth;
		case UNIT_VH:   return ( val.value / 100.0f ) * vpHeight;
		case UNIT_PX:   return val.value;
		case UNIT_AUTO: return 0.0f;  // resolved later by layout engine from content
		case UNIT_NORM:
		default:        return val.value * parentSizePx;
	}
}

wuiPixelRect_t WUI_ResolveRect( const wuiRect_t *rect, const wuiPixelRect_t *parent,
                                 float vpWidth, float vpHeight ) {
	wuiPixelRect_t out;
	out.x = WUI_Resolve( rect->x, parent->w, vpWidth, vpHeight );
	out.y = WUI_Resolve( rect->y, parent->h, vpWidth, vpHeight );
	out.w = WUI_Resolve( rect->w, parent->w, vpWidth, vpHeight );
	out.h = WUI_Resolve( rect->h, parent->h, vpWidth, vpHeight );
	// Offset by parent origin (caller passes the correct parent:
	// viewport for POSITION_VIEWPORT items, menu rect for others)
	out.x += parent->x;
	out.y += parent->y;
	return out;
}

void WUI_ApplyAspect( wuiPixelRect_t *rect, const wuiAspect_t *aspect ) {
	if ( !aspect->active ) return;
	// contain: fit within the given rect maintaining aspect ratio
	float desiredW = rect->h * aspect->ratio;
	float desiredH = rect->w / aspect->ratio;
	if ( desiredW <= rect->w ) {
		// height is the constraint — center horizontally
		rect->x += ( rect->w - desiredW ) * 0.5f;
		rect->w = desiredW;
	} else {
		// width is the constraint — center vertically
		rect->y += ( rect->h - desiredH ) * 0.5f;
		rect->h = desiredH;
	}
}

void WUI_ApplyMinMax( wuiPixelRect_t *rect, const wuiFlexChild_t *child,
                      float vpWidth, float vpHeight ) {
	// Only apply if value > 0 (0 = no constraint)
	if ( child->minWidth.value > 0 ) {
		float minW = WUI_Resolve( child->minWidth, rect->w, vpWidth, vpHeight );
		if ( rect->w < minW ) rect->w = minW;
	}
	if ( child->maxWidth.value > 0 ) {
		float maxW = WUI_Resolve( child->maxWidth, rect->w, vpWidth, vpHeight );
		if ( rect->w > maxW ) rect->w = maxW;
	}
	if ( child->minHeight.value > 0 ) {
		float minH = WUI_Resolve( child->minHeight, rect->h, vpWidth, vpHeight );
		if ( rect->h < minH ) rect->h = minH;
	}
	if ( child->maxHeight.value > 0 ) {
		float maxH = WUI_Resolve( child->maxHeight, rect->h, vpWidth, vpHeight );
		if ( rect->h > maxH ) rect->h = maxH;
	}
}

void WUI_LayoutFlex(
    const wuiRect_t *items, wuiPixelRect_t *resolved, int count,
    const wuiPixelRect_t *container, const wuiFlexContainer_t *flex,
    const wuiFlexChild_t *childProps, const wuiAspect_t *aspects,
    float vpWidth, float vpHeight )
{
	if ( count <= 0 ) return;

	// 1. Resolve padding
	float padTop   = WUI_Resolve( flex->padding[0], container->h, vpWidth, vpHeight );
	float padRight = WUI_Resolve( flex->padding[1], container->w, vpWidth, vpHeight );
	float padBot   = WUI_Resolve( flex->padding[2], container->h, vpWidth, vpHeight );
	float padLeft  = WUI_Resolve( flex->padding[3], container->w, vpWidth, vpHeight );

	float innerX = container->x + padLeft;
	float innerY = container->y + padTop;
	float innerW = container->w - padLeft - padRight;
	float innerH = container->h - padTop - padBot;

	if ( innerW < 0 ) innerW = 0;
	if ( innerH < 0 ) innerH = 0;

	// Determine main axis and cross axis sizes
	float mainSize   = ( flex->direction == WUI_LAYOUT_ROW ) ? innerW : innerH;
	float crossTotal = ( flex->direction == WUI_LAYOUT_ROW ) ? innerH : innerW;
	float gapPx = WUI_Resolve( flex->gap, mainSize, vpWidth, vpHeight );

	// Process items in lines (for wrap support)
	float lineCrossOffset = 0;
	float lineCrossMax = 0;
	int lineStart = 0;

	while ( lineStart < count ) {
		float *mainSizes;
		float totalUsed, mainRemaining;
		int lineEnd, lineCount;

		// 2. Determine which items fit in this line
		float lineMainUsed = 0;
		lineCrossMax = 0;
		lineEnd = lineStart;

		for ( int i = lineStart; i < count; i++ ) {
			float naturalMain;
			float itemGap;
			float naturalCross;

			// Resolve natural size of child
			if ( childProps[i].basis.value > 0 ) {
				naturalMain = WUI_Resolve( childProps[i].basis,
					( flex->direction == WUI_LAYOUT_ROW ) ? innerW : innerH,
					vpWidth, vpHeight );
			} else {
				naturalMain = WUI_Resolve(
					( flex->direction == WUI_LAYOUT_ROW ) ? items[i].w : items[i].h,
					( flex->direction == WUI_LAYOUT_ROW ) ? innerW : innerH,
					vpWidth, vpHeight );
			}

			itemGap = ( i > lineStart ) ? gapPx : 0;

			if ( flex->wrap && lineEnd > lineStart && lineMainUsed + itemGap + naturalMain > mainSize ) {
				break; // wrap to next line
			}

			lineMainUsed += itemGap + naturalMain;
			lineEnd = i + 1;

			// Track cross size for this line
			naturalCross = WUI_Resolve(
				( flex->direction == WUI_LAYOUT_ROW ) ? items[i].h : items[i].w,
				( flex->direction == WUI_LAYOUT_ROW ) ? innerH : innerW,
				vpWidth, vpHeight );
			if ( naturalCross > lineCrossMax ) lineCrossMax = naturalCross;
		}

		lineCount = lineEnd - lineStart;
		if ( lineCount <= 0 ) break;

		// If not wrapping, use full cross size
		if ( !flex->wrap ) lineCrossMax = crossTotal;

		// 3. Distribute space (grow/shrink)
		float totalGaps = gapPx * ( lineCount - 1 );
		float available = mainSize - totalGaps;
		float totalNatural = 0;
		float totalGrow = 0;
		float totalShrink = 0;

		// First pass: compute natural sizes and totals
		for ( int i = lineStart; i < lineEnd; i++ ) {
			float natural;
			if ( childProps[i].basis.value > 0 ) {
				natural = WUI_Resolve( childProps[i].basis,
					( flex->direction == WUI_LAYOUT_ROW ) ? innerW : innerH,
					vpWidth, vpHeight );
			} else {
				natural = WUI_Resolve(
					( flex->direction == WUI_LAYOUT_ROW ) ? items[i].w : items[i].h,
					( flex->direction == WUI_LAYOUT_ROW ) ? innerW : innerH,
					vpWidth, vpHeight );
			}
			totalNatural += natural;
			totalGrow += childProps[i].grow;
			totalShrink += childProps[i].shrink * natural;
		}

		float excess = available - totalNatural;

		// Second pass: compute final sizes
		mainSizes = (float *)alloca( lineCount * sizeof( float ) );
		for ( int i = 0; i < lineCount; i++ ) {
			int idx = lineStart + i;
			float natural;
			if ( childProps[idx].basis.value > 0 ) {
				natural = WUI_Resolve( childProps[idx].basis,
					( flex->direction == WUI_LAYOUT_ROW ) ? innerW : innerH,
					vpWidth, vpHeight );
			} else {
				natural = WUI_Resolve(
					( flex->direction == WUI_LAYOUT_ROW ) ? items[idx].w : items[idx].h,
					( flex->direction == WUI_LAYOUT_ROW ) ? innerW : innerH,
					vpWidth, vpHeight );
			}

			if ( excess > 0 && totalGrow > 0 ) {
				mainSizes[i] = natural + excess * ( childProps[idx].grow / totalGrow );
			} else if ( excess < 0 && totalShrink > 0 ) {
				float shrinkFactor = ( childProps[idx].shrink * natural ) / totalShrink;
				mainSizes[i] = natural + excess * shrinkFactor;
			} else {
				mainSizes[i] = natural;
			}
			if ( mainSizes[i] < 0 ) mainSizes[i] = 0;
		}

		// 4. Apply justify (position on main axis)
		totalUsed = totalGaps;
		for ( int i = 0; i < lineCount; i++ ) totalUsed += mainSizes[i];
		mainRemaining = mainSize - totalUsed;
		if ( mainRemaining < 0 ) mainRemaining = 0;

		float cursor;
		switch ( flex->justify ) {
			case WUI_JUSTIFY_CENTER:
				cursor = mainRemaining * 0.5f;
				break;
			case WUI_JUSTIFY_END:
				cursor = mainRemaining;
				break;
			case WUI_JUSTIFY_SPACE_BETWEEN:
				cursor = 0;
				if ( lineCount > 1 ) {
					gapPx = mainRemaining / ( lineCount - 1 );
				}
				break;
			case WUI_JUSTIFY_START:
			default:
				cursor = 0;
				break;
		}

		// 5. Position each child
		for ( int i = 0; i < lineCount; i++ ) {
			int idx = lineStart + i;
			float childMain = mainSizes[i];
			float childCross;
			float crossOffset;
			wuiAlign_t align;

			// Resolve cross size
			childCross = WUI_Resolve(
				( flex->direction == WUI_LAYOUT_ROW ) ? items[idx].h : items[idx].w,
				( flex->direction == WUI_LAYOUT_ROW ) ? innerH : innerW,
				vpWidth, vpHeight );

			// Apply align (cross axis positioning)
			crossOffset = 0;
			// Use parent align unless child overrides with non-START value
			align = flex->align;
			if ( childProps[idx].alignSelf != WUI_ALIGN_START ) {
				align = childProps[idx].alignSelf;
			}

			switch ( align ) {
				case WUI_ALIGN_CENTER:
					crossOffset = ( lineCrossMax - childCross ) * 0.5f;
					break;
				case WUI_ALIGN_END:
					crossOffset = lineCrossMax - childCross;
					break;
				case WUI_ALIGN_STRETCH:
					childCross = lineCrossMax;
					crossOffset = 0;
					break;
				case WUI_ALIGN_START:
				default:
					crossOffset = 0;
					break;
			}

			// Build the resolved rect
			if ( flex->direction == WUI_LAYOUT_ROW ) {
				resolved[idx].x = innerX + cursor;
				resolved[idx].y = innerY + lineCrossOffset + crossOffset;
				resolved[idx].w = childMain;
				resolved[idx].h = childCross;
			} else {
				resolved[idx].x = innerX + lineCrossOffset + crossOffset;
				resolved[idx].y = innerY + cursor;
				resolved[idx].w = childCross;
				resolved[idx].h = childMain;
			}

			// Apply min/max constraints
			WUI_ApplyMinMax( &resolved[idx], &childProps[idx], vpWidth, vpHeight );

			// Apply aspect ratio
			if ( aspects && aspects[idx].active ) {
				WUI_ApplyAspect( &resolved[idx], &aspects[idx] );
			}

			cursor += childMain + gapPx;
		}

		lineCrossOffset += lineCrossMax;
		if ( flex->wrap ) lineCrossOffset += gapPx; // gap between lines
		lineStart = lineEnd;
	}
}

// ── Layout tree resolution ───────────────────────────────────────────

void WUI_LayoutItem( wiredItemDef_t *item, const wuiPixelRect_t *parent,
                     float vpWidth, float vpHeight ) {
	const wuiRect_t *srcRect;

	// Check for responsive breakpoint override
	srcRect = &item->wuiRect;
	if ( item->breakpointCount > 0 ) {
		const wuiRect_t *bpRect = WUI_FindBreakpointRect(
			item->breakpoints, item->breakpointCount, (int)vpWidth );
		if ( bpRect ) srcRect = bpRect;
	}

	// Resolve this item's rect relative to parent
	item->resolvedRect = WUI_ResolveRect( srcRect, parent, vpWidth, vpHeight );

	// Apply aspect ratio constraint
	if ( item->aspect.active ) {
		WUI_ApplyAspect( &item->resolvedRect, &item->aspect );
	}

	// Apply min/max constraints
	WUI_ApplyMinMax( &item->resolvedRect, &item->flexChild, vpWidth, vpHeight );

	// Recursively resolve children
	if ( item->isFlexContainer && item->childCount > 0 ) {
		// Use flexbox layout for children
		wuiRect_t     *childRects  = (wuiRect_t *)alloca( item->childCount * sizeof( wuiRect_t ) );
		wuiPixelRect_t *childResolved = (wuiPixelRect_t *)alloca( item->childCount * sizeof( wuiPixelRect_t ) );
		wuiFlexChild_t *childProps = (wuiFlexChild_t *)alloca( item->childCount * sizeof( wuiFlexChild_t ) );
		wuiAspect_t    *childAspects = (wuiAspect_t *)alloca( item->childCount * sizeof( wuiAspect_t ) );

		for ( int i = 0; i < item->childCount; i++ ) {
			childRects[i]   = item->children[i]->wuiRect;
			childProps[i]   = item->children[i]->flexChild;
			childAspects[i] = item->children[i]->aspect;
		}

		WUI_LayoutFlex( childRects, childResolved, item->childCount,
			&item->resolvedRect, &item->flexContainer, childProps, childAspects,
			vpWidth, vpHeight );

		for ( int i = 0; i < item->childCount; i++ ) {
			item->children[i]->resolvedRect = childResolved[i];
			// Recurse into grandchildren
			if ( item->children[i]->isFlexContainer && item->children[i]->childCount > 0 ) {
				WUI_LayoutItem( item->children[i], &childResolved[i], vpWidth, vpHeight );
			}
		}
	} else {
		// Absolute positioning: resolve children relative to this item
		for ( int i = 0; i < item->childCount; i++ ) {
			WUI_LayoutItem( item->children[i], &item->resolvedRect, vpWidth, vpHeight );
		}
	}
}

void WUI_LayoutMenu( wiredMenuDef_t *menu, float vpWidth, float vpHeight ) {
	wuiPixelRect_t viewport;

	if ( !menu ) return;

	// Viewport is the root parent
	viewport.x = 0;
	viewport.y = 0;
	viewport.w = vpWidth;
	viewport.h = vpHeight;

	// Resolve menu rect relative to viewport
	menu->resolvedRect = WUI_ResolveRect( &menu->wuiRect, &viewport, vpWidth, vpHeight );

	// Apply anchor: reposition menu origin
	if ( menu->anchor != ANCHOR_NONE && menu->anchor != ANCHOR_TOP_LEFT ) {
		float mw = menu->fullscreen ? vpWidth : menu->resolvedRect.w;
		float mh = menu->fullscreen ? vpHeight : menu->resolvedRect.h;

		switch ( menu->anchor ) {
			case ANCHOR_TOP_CENTER:    menu->resolvedRect.x = ( vpWidth - mw ) * 0.5f;  menu->resolvedRect.y = 0;                     break;
			case ANCHOR_TOP_RIGHT:     menu->resolvedRect.x = vpWidth - mw;              menu->resolvedRect.y = 0;                     break;
			case ANCHOR_CENTER_LEFT:   menu->resolvedRect.x = 0;                         menu->resolvedRect.y = ( vpHeight - mh ) * 0.5f; break;
			case ANCHOR_CENTER:        menu->resolvedRect.x = ( vpWidth - mw ) * 0.5f;   menu->resolvedRect.y = ( vpHeight - mh ) * 0.5f; break;
			case ANCHOR_CENTER_RIGHT:  menu->resolvedRect.x = vpWidth - mw;              menu->resolvedRect.y = ( vpHeight - mh ) * 0.5f; break;
			case ANCHOR_BOTTOM_LEFT:   menu->resolvedRect.x = 0;                         menu->resolvedRect.y = vpHeight - mh;         break;
			case ANCHOR_BOTTOM_CENTER: menu->resolvedRect.x = ( vpWidth - mw ) * 0.5f;   menu->resolvedRect.y = vpHeight - mh;         break;
			case ANCHOR_BOTTOM_RIGHT:  menu->resolvedRect.x = vpWidth - mw;              menu->resolvedRect.y = vpHeight - mh;         break;
			default: break;
		}
	}

	// For fullscreen menus, override resolved rect to full viewport
	if ( menu->fullscreen ) {
		menu->resolvedRect.x = 0;
		menu->resolvedRect.y = 0;
		menu->resolvedRect.w = vpWidth;
		menu->resolvedRect.h = vpHeight;
	}

	// UNIT_AUTO on height: size to content (walk children to find extent)
	if ( menu->wuiRect.h.unit == UNIT_AUTO ) {
		float bottom = 0;
		for ( int i = 0; i < menu->itemCount; i++ ) {
			if ( !menu->items[i] ) continue;
			// Temporarily resolve each item to find its bottom edge
			wuiPixelRect_t childRect = WUI_ResolveRect( &menu->items[i]->wuiRect,
				&menu->resolvedRect, vpWidth, vpHeight );
			float childBottom = ( childRect.y - menu->resolvedRect.y ) + childRect.h;
			if ( childBottom > bottom ) bottom = childBottom;
		}
		if ( bottom > 0 ) {
			menu->resolvedRect.h = bottom;
		} else {
			// No children — fall back to remaining viewport space
			menu->resolvedRect.h = vpHeight - menu->resolvedRect.y;
		}
	}

	// Resolve items using flex layout or absolute positioning
	if ( menu->isFlexContainer && menu->itemCount > 0 ) {
		// Separate items into flex participants and viewport-absolute items.
		// Items with VW/VH on x or y are viewport-absolute (e.g. fullscreen backgrounds)
		// and should not participate in flex flow.
		int *flexIndices = (int *)alloca( menu->itemCount * sizeof( int ) );
		int flexCount = 0;

		for ( int i = 0; i < menu->itemCount; i++ ) {
			wiredItemDef_t *item = menu->items[i];
			if ( item->position != POSITION_STATIC ) {
				// Absolute or viewport-positioned — resolve outside flex flow
				if ( item->position == POSITION_VIEWPORT ) {
					WUI_LayoutItem( item, &viewport, vpWidth, vpHeight );
				} else {
					WUI_LayoutItem( item, &menu->resolvedRect, vpWidth, vpHeight );
				}
			} else {
				flexIndices[flexCount++] = i;
			}
		}

		if ( flexCount > 0 ) {
			wuiRect_t      *flexRects    = (wuiRect_t *)alloca( flexCount * sizeof( wuiRect_t ) );
			wuiPixelRect_t *flexResolved = (wuiPixelRect_t *)alloca( flexCount * sizeof( wuiPixelRect_t ) );
			wuiFlexChild_t *flexProps    = (wuiFlexChild_t *)alloca( flexCount * sizeof( wuiFlexChild_t ) );
			wuiAspect_t    *flexAspects  = (wuiAspect_t *)alloca( flexCount * sizeof( wuiAspect_t ) );

			for ( int i = 0; i < flexCount; i++ ) {
				int idx = flexIndices[i];
				flexRects[i]   = menu->items[idx]->wuiRect;
				flexProps[i]   = menu->items[idx]->flexChild;
				flexAspects[i] = menu->items[idx]->aspect;
			}

			WUI_LayoutFlex( flexRects, flexResolved, flexCount,
				&menu->resolvedRect, &menu->flexContainer, flexProps, flexAspects,
				vpWidth, vpHeight );

			for ( int i = 0; i < flexCount; i++ ) {
				int idx = flexIndices[i];
				menu->items[idx]->resolvedRect = flexResolved[i];
				if ( menu->items[idx]->isFlexContainer && menu->items[idx]->childCount > 0 ) {
					WUI_LayoutItem( menu->items[idx], &flexResolved[i], vpWidth, vpHeight );
				}
			}
		}
	} else {
		// Absolute positioning: resolve each item individually
		for ( int i = 0; i < menu->itemCount; i++ ) {
			if ( menu->items[i] ) {
				WUI_LayoutItem( menu->items[i], &menu->resolvedRect, vpWidth, vpHeight );
			}
		}
	}
}

// ── Layer 5: Animation/Transition system ─────────────────────────────

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

	// Lerp each component's value (keep the unit from 'to')
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

// ── Layer 5: Responsive breakpoints ──────────────────────────────────

const wuiRect_t *WUI_FindBreakpointRect( const wuiBreakpoint_t *bps, int count, int vpWidth ) {
	for ( int i = count - 1; i >= 0; i-- ) {
		if ( !bps[i].active ) continue;
		if ( bps[i].minWidth && vpWidth < bps[i].minWidth ) continue;
		if ( bps[i].maxWidth && vpWidth > bps[i].maxWidth ) continue;
		return &bps[i].rect;
	}
	return NULL; // no match — use default rect
}
