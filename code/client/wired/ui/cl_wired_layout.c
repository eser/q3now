// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
cl_wired_layout.c — Wired UI: resolution-independent coordinate resolver
*/

#include "cl_wired_layout.h"
#include "cl_wired_ui.h"
#include "cl_wired_compositor.h"   /* WiredUI_GetDpiScale (UNIT_PX scaling) */

#ifdef _DEBUG
LOG_DECLARE_CHANNEL( ch_ui, "ui" );
#endif

float WUI_Resolve( wuiValue_t val, float parentSizePx, float vpWidth, float vpHeight ) {
	switch ( val.unit ) {
		case UNIT_VW:   return ( val.value / 100.0f ) * vpWidth;
		case UNIT_VH:   return ( val.value / 100.0f ) * vpHeight;
		/* Authored pixel lengths are LOGICAL points, but the Clay layout runs
		 * in PHYSICAL pixels and the text path multiplies font size by
		 * WiredUI_GetDpiScale() (cl_wired_clay.c). A fixed-px box height that
		 * skipped that scale stayed logical-sized while its text grew 2× on
		 * HiDPI, so glyphs overflowed their rows (cramped line spacing). Scale
		 * UNIT_PX by the same factor so box geometry tracks the font. */
		case UNIT_PX:   return val.value * WiredUI_GetDpiScale();
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

/* Dispatch 5.7 S1: bottom-up content measurement for UNIT_AUTO-sized flex
 * children. Recursion invariants:
 *   - Leaf with explicit size on the axis: return WUI_Resolve of that value
 *   - Flex container, main axis matches: sum of children main + (n-1)*gap +
 *     padding[main-side-1] + padding[main-side-2]
 *   - Flex container, cross axis: max of children cross + cross-padding
 *   - Fallback (no resolvable size, no children): rect's stored value
 *     interpreted as NORM against parent (the dispatch-5.7 parser change
 *     preserves the rect's pre-FIT value as a hint for exactly this fallback)
 *   - Depth-bounded by tree termination; recursion follows existing
 *     childCount walk.
 */
static float wui_measure_natural( const wiredItemDef_t *item, qboolean isHeight,
                                  float parentMain, float parentCross,
                                  float vpW, float vpH );

static float wui_measure_natural( const wiredItemDef_t *item, qboolean isHeight,
                                  float parentMain, float parentCross,
                                  float vpW, float vpH )
{
	const wuiValue_t *axis = isHeight ? &item->wuiRect.h : &item->wuiRect.w;
	if ( axis->unit != UNIT_AUTO ) {
		float v = WUI_Resolve( *axis, parentMain, vpW, vpH );
		if ( v > 0 ) return v;
	}
	if ( item->isFlexContainer && item->childCount > 0 ) {
		qboolean mainIsHeight = ( item->flexContainer.direction == WUI_LAYOUT_COLUMN );
		float padA = WUI_Resolve( item->flexContainer.padding[ isHeight ? 0 : 3 ], parentMain, vpW, vpH );
		float padB = WUI_Resolve( item->flexContainer.padding[ isHeight ? 2 : 1 ], parentMain, vpW, vpH );
		float gap = WUI_Resolve( item->flexContainer.gap, parentMain, vpW, vpH );
		float total = 0;
		float childMaxCross = 0;
		int n = 0;
		for ( int i = 0; i < item->childCount; i++ ) {
			if ( !item->children[i] || !item->children[i]->visible ) continue;
			float childM = wui_measure_natural( item->children[i], isHeight, parentMain, parentCross, vpW, vpH );
			float childC = wui_measure_natural( item->children[i], !isHeight, parentCross, parentMain, vpW, vpH );
			if ( ( isHeight == mainIsHeight ) ) {
				total += childM;
				n++;
			} else {
				if ( childM > childMaxCross ) childMaxCross = childM;
			}
			(void) childC;
		}
		if ( isHeight == mainIsHeight ) {
			float gaps = ( n > 1 ) ? gap * ( n - 1 ) : 0;
			return padA + padB + total + gaps;
		}
		return padA + padB + childMaxCross;
	}
	/* Leaf fallback: rect's preserved pre-FIT value (or 0 if never set). */
	return WUI_Resolve( (wuiValue_t){ axis->value, UNIT_NORM }, parentMain, vpW, vpH );
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

#ifdef _DEBUG
	Com_Log( SEV_TRACE, LOG_CH(ch_ui),
		"WUI_TRACE FLEX_ENTRY container=(%.1f,%.1f,%.1f,%.1f) inner=(%.1f,%.1f,%.1f,%.1f) "
		"dir=%d count=%d mainSize=%.1f gapPx=%.1f align=%d justify=%d wrap=%d\n",
		container->x, container->y, container->w, container->h,
		innerX, innerY, innerW, innerH,
		(int) flex->direction, count, mainSize, gapPx,
		(int) flex->align, (int) flex->justify, (int) flex->wrap );
#endif

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
#ifdef _DEBUG
			Com_Log( SEV_TRACE, LOG_CH(ch_ui),
				"WUI_TRACE FLEX_CHILD idx=%d item.h=(%.3f:%d) item.w=(%.3f:%d) natural=%.2f mainSize=%.2f "
				"grow=%.2f shrink=%.2f totalNat=%.1f excess=%.1f\n",
				idx, items[idx].h.value, items[idx].h.unit,
				items[idx].w.value, items[idx].w.unit,
				natural, mainSizes[i],
				childProps[idx].grow, childProps[idx].shrink, totalNatural, excess );
#endif
		}

		// 4. Apply justify (position on main axis)
		totalUsed = totalGaps;
		for ( int i = 0; i < lineCount; i++ ) totalUsed += mainSizes[i];
		mainRemaining = mainSize - totalUsed;
		if ( mainRemaining < 0 ) mainRemaining = 0;

		float cursor;
		// Per-line positioning gap. Seeded from the function-scope gapPx each
		// iteration so the SPACE_BETWEEN override below cannot leak into the next
		// wrapped line (the function-scope gapPx stays the authored flex->gap,
		// which line 377's inter-line spacing still needs).
		float layoutGap = gapPx;
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
					layoutGap = mainRemaining / ( lineCount - 1 );
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

			cursor += childMain + layoutGap;
		}

		lineCrossOffset += lineCrossMax;
		if ( flex->wrap ) lineCrossOffset += gapPx; // gap between lines
		lineStart = lineEnd;
	}
}

// ── Layout tree resolution ───────────────────────────────────────────

/* sizeAlreadyResolved: the caller (a parent flex pass) has ALREADY assigned this
   item's final box into item->resolvedRect and passes that same box as `parent`.
   In that case the item's own w/h/x/y must NOT be re-resolved against the parent
   — doing so applies a PERCENT/NORM size a SECOND time (0.4×parent when the flex
   pass already made the item 0.4×grandparent), starving the box. Preserve the
   flex-assigned rect and only lay out the item's children. */
static void WUI_LayoutItemImpl( wiredItemDef_t *item, const wuiPixelRect_t *parent,
                                float vpWidth, float vpHeight,
                                qboolean sizeAlreadyResolved ) {
	const wuiRect_t *srcRect;

	// Check for responsive breakpoint override
	srcRect = &item->wuiRect;
	if ( item->breakpointCount > 0 ) {
		const wuiRect_t *bpRect = WUI_FindBreakpointRect(
			item->breakpoints, item->breakpointCount, (int)vpWidth );
		if ( bpRect ) srcRect = bpRect;
	}

	// Resolve this item's rect relative to parent — UNLESS the parent's flex pass
	// already assigned the final box (then re-resolving would double-apply size).
	if ( !sizeAlreadyResolved ) {
		item->resolvedRect = WUI_ResolveRect( srcRect, parent, vpWidth, vpHeight );
	}

	/* No-size flex container → fill parent. A flex container that authored no
	 * size on an axis (the default `{value:0, unit:UNIT_NORM}`, or an explicit
	 * UNIT_AUTO) resolves that axis to 0 via WUI_Resolve — collapsing the whole
	 * subtree. The loading-screen decoration group wrappers (loading_backdrop_
	 * root, loading_topbar_root, loading_maptitle_root, …: `type container /
	 * direction column / decoration` with no rect) are exactly this case, and
	 * their absolutely-placed children then resolve against a 0×0 box. A flex
	 * container with no authored extent is, by intent, a layout wrapper that
	 * should span its parent (so its children — flex-flowed or absolutely
	 * placed via the partition below — have a real coordinate space). Mirror
	 * that: default each unauthored axis to the parent's size. An authored size
	 * (any non-zero value, or a non-NORM/AUTO unit like VW/VH/PX) is left
	 * untouched, so this never overrides an explicit container size. */
	if ( item->isFlexContainer && !sizeAlreadyResolved ) {
		if ( srcRect->w.value == 0.0f
		     && ( srcRect->w.unit == UNIT_NORM || srcRect->w.unit == UNIT_AUTO ) ) {
			item->resolvedRect.w = parent->w;
		}
		if ( srcRect->h.value == 0.0f
		     && ( srcRect->h.unit == UNIT_NORM || srcRect->h.unit == UNIT_AUTO ) ) {
			item->resolvedRect.h = parent->h;
		}
	}

	// Dispatch 5.6 S1: CSS-style per-side anchor offset overrides. When
	// wuiOffset has any side declared, override resolvedRect.x/y/w/h so the
	// child recursion below resolves grandchildren against the correct
	// floating-parent rect. Reference frame is the viewport (mirrors
	// cl_wired_clay.c dispatch 5.5 S2 use of panel->resolvedRect; both must
	// agree on the offset's reference frame for emit and layout to align).
	// CSS-positioned ancestor lookup not implemented — V1_Monolith's
	// main_root is the menu root which IS the viewport so this matches
	// mockup intent; future artboards with nested position:relative
	// ancestors will need that lookup.
	{
		const wuiOffset_t *off = &item->wuiOffset;
		if ( off->hasTop || off->hasLeft || off->hasRight || off->hasBottom ) {
			float pX = 0.0f;
			float pY = 0.0f;
			float pW = vpWidth;
			float pH = vpHeight;
			if ( off->hasLeft ) {
				item->resolvedRect.x = pX + WUI_Resolve( off->left, pW, vpWidth, vpHeight );
			}
			if ( off->hasTop ) {
				item->resolvedRect.y = pY + WUI_Resolve( off->top, pH, vpWidth, vpHeight );
			}
			if ( off->hasRight ) {
				float rPx = WUI_Resolve( off->right, pW, vpWidth, vpHeight );
				if ( off->hasLeft ) {
					item->resolvedRect.w = ( pX + pW - rPx ) - item->resolvedRect.x;
				} else {
					if ( item->resolvedRect.w <= 0 ) item->resolvedRect.w = pW * 0.25f;
					item->resolvedRect.x = pX + pW - rPx - item->resolvedRect.w;
				}
			}
			if ( off->hasBottom ) {
				float bPx = WUI_Resolve( off->bottom, pH, vpWidth, vpHeight );
				if ( off->hasTop ) {
					item->resolvedRect.h = ( pY + pH - bPx ) - item->resolvedRect.y;
				} else {
					if ( item->resolvedRect.h <= 0 ) item->resolvedRect.h = pH * 0.25f;
					item->resolvedRect.y = pY + pH - bPx - item->resolvedRect.h;
				}
			}
			// CSS-auto stretch: top without bottom → height fills to parent
			// bottom; left without right → width fills to parent right.
			if ( item->resolvedRect.h <= 0 && off->hasTop ) {
				item->resolvedRect.h = pY + pH - item->resolvedRect.y;
			}
			if ( item->resolvedRect.w <= 0 && off->hasLeft ) {
				item->resolvedRect.w = pX + pW - item->resolvedRect.x;
			}
		}
	}

	// Apply aspect + min/max — but ONLY on a fresh resolve. A flex-assigned child
	// (sizeAlreadyResolved) had these applied to its box by the parent's flex pass;
	// re-running them here is non-idempotent (a NORM min re-grows against the
	// already-clamped width, aspect re-centers), so under the guard the assigned
	// box is left fully final.
	if ( !sizeAlreadyResolved ) {
		// Apply aspect ratio constraint
		if ( item->aspect.active ) {
			WUI_ApplyAspect( &item->resolvedRect, &item->aspect );
		}

		// Apply min/max constraints
		WUI_ApplyMinMax( &item->resolvedRect, &item->flexChild, vpWidth, vpHeight );
	}

#ifdef _DEBUG
	Com_Log( SEV_TRACE, LOG_CH(ch_ui),
		"WUI_TRACE LAYOUTITEM item='%s' parent=(%.1f,%.1f,%.1f,%.1f) "
		"resolved=(%.1f,%.1f,%.1f,%.1f) flex=%d childCount=%d\n",
		item->name[ 0 ] ? item->name : "<anon>",
		parent->x, parent->y, parent->w, parent->h,
		item->resolvedRect.x, item->resolvedRect.y,
		item->resolvedRect.w, item->resolvedRect.h,
		(int) item->isFlexContainer, item->childCount );
#endif

	// Recursively resolve children
	if ( item->isFlexContainer && item->childCount > 0 ) {
		/* Partition children the same way WUI_LayoutMenu's root loop does
		 * (the menu-root split at the bottom of this file): an absolutely-
		 * positioned child is resolved OUTSIDE the flex flow against this
		 * container's rect (honoring its authored x/y/w/h), and is excluded
		 * from the flex participant set so it is not column/row-stacked.
		 * Static children flow through the flex path unchanged.
		 *
		 * "Absolutely-positioned" = explicit `position absolute|viewport`
		 * OR a `decoration` leaf that authored an explicit non-AUTO rect
		 * (e.g. loading_screen's loading_backdrop_root grid/divider/glow,
		 * which are POSITION_STATIC by default but carry `rect X Y W H`).
		 * The `decoration` qualifier is load-bearing: it distinguishes an
		 * absolutely-placed visual from an interactive flex child that also
		 * happens to author a rect (options/network/video controls), which
		 * must keep flex-stacking. Without this split a flex container with
		 * no authored size collapsed its absolute children to (0,0,0,0).
		 * (Nested `position viewport` does not occur in the corpus — connect
		 * .wui's viewport items are menu-root, handled by the root loop — so
		 * VIEWPORT is resolved against the container here like ABSOLUTE.) */
		int *flexIdx   = (int *)alloca( item->childCount * sizeof( int ) );
		int  flexCount = 0;

		for ( int i = 0; i < item->childCount; i++ ) {
			wiredItemDef_t *c = item->children[i];
			/* A nested flex child is resolved at its authored rect (pulled
			 * OUT of flex flow) iff it is explicitly non-static, OR it is an
			 * absolutely-placed decoration leaf. The decoration arm needs
			 * THREE guards to avoid sweeping in legitimate flex children that
			 * also author a rect (the main-menu cards/rows in qw_*_card.wui /
			 * qw_menu_item.wui, which are `decoration` with `width PERCENT` /
			 * `height FIT` that resolve to non-AUTO units — so a w/h-only test
			 * is not enough):
			 *   - flexChild.grow == 0 : a grower is, by definition, flexed.
			 *   - w & h are non-AUTO and > 0 : it authored a concrete size.
			 *   - x != 0 || y != 0 : it authored a concrete POSITION. The card
			 *     leaves author `rect 0 0 W H` and rely on flex to place them;
			 *     the loading backdrop leaves author non-zero x/y (grid y=0.04…,
			 *     dividers x=0.518…, glows x=0.10/0.50 y=0.30). This authored-
			 *     position test is the load-bearing discriminator. */
			qboolean absolute =
				c->position != POSITION_STATIC
				|| ( c->decoration
				     && c->flexChild.grow == 0.0f
				     && c->wuiRect.w.unit != UNIT_AUTO && c->wuiRect.w.value > 0.0f
				     && c->wuiRect.h.unit != UNIT_AUTO && c->wuiRect.h.value > 0.0f
				     && ( c->wuiRect.x.value != 0.0f || c->wuiRect.y.value != 0.0f ) );
			if ( absolute ) {
				// Resolve at the authored rect against this container; this
				// call recurses into the child's own grandchildren internally.
				WUI_LayoutItemImpl( c, &item->resolvedRect, vpWidth, vpHeight, qfalse );
			} else {
				flexIdx[ flexCount++ ] = i;
			}
		}

		if ( flexCount > 0 ) {
		// Use flexbox layout for the static participants only.
		wuiRect_t     *childRects  = (wuiRect_t *)alloca( flexCount * sizeof( wuiRect_t ) );
		wuiPixelRect_t *childResolved = (wuiPixelRect_t *)alloca( flexCount * sizeof( wuiPixelRect_t ) );
		wuiFlexChild_t *childProps = (wuiFlexChild_t *)alloca( flexCount * sizeof( wuiFlexChild_t ) );
		wuiAspect_t    *childAspects = (wuiAspect_t *)alloca( flexCount * sizeof( wuiAspect_t ) );

		for ( int k = 0; k < flexCount; k++ ) {
			wiredItemDef_t *c = item->children[ flexIdx[k] ];
			childRects[k]   = c->wuiRect;
			childProps[k]   = c->flexChild;
			childAspects[k] = c->aspect;

			/* Dispatch 5.7 S1: pre-resolve UNIT_AUTO sizes via the
			 * parser-preserved rect-declared hint OR via bottom-up
			 * measurement when no hint exists. WUI_LayoutFlex itself sees
			 * the substituted UNIT_PX value and routes through the
			 * existing fixed-size path. Without this, AUTO children
			 * resolve to 0 via WUI_Resolve's UNIT_AUTO branch and all
			 * stack at cursor=0. */
			if ( childRects[k].h.unit == UNIT_AUTO ) {
				float hint = childRects[k].h.value * item->resolvedRect.h;
				float measured = hint > 0 ? hint : wui_measure_natural(
					c, qtrue,
					item->resolvedRect.h, item->resolvedRect.w, vpWidth, vpHeight );
				if ( measured > 0 ) {
					childRects[k].h.unit = UNIT_PX;
					childRects[k].h.value = measured;
				}
			}
			if ( childRects[k].w.unit == UNIT_AUTO ) {
				float hint = childRects[k].w.value * item->resolvedRect.w;
				float measured = hint > 0 ? hint : wui_measure_natural(
					c, qfalse,
					item->resolvedRect.w, item->resolvedRect.h, vpWidth, vpHeight );
				if ( measured > 0 ) {
					childRects[k].w.unit = UNIT_PX;
					childRects[k].w.value = measured;
				}
			}
		}

		WUI_LayoutFlex( childRects, childResolved, flexCount,
			&item->resolvedRect, &item->flexContainer, childProps, childAspects,
			vpWidth, vpHeight );

		for ( int k = 0; k < flexCount; k++ ) {
			wiredItemDef_t *c = item->children[ flexIdx[k] ];
			c->resolvedRect = childResolved[k];
#ifdef _DEBUG
			Com_Log( SEV_TRACE, LOG_CH(ch_ui),
				"WUI_TRACE FLEXCHILD parent='%s' child='%s' assigned=(%.1f,%.1f,%.1f,%.1f) recurse=%d\n",
				item->name[ 0 ] ? item->name : "<anon>",
				c->name[ 0 ] ? c->name : "<anon>",
				childResolved[k].x, childResolved[k].y,
				childResolved[k].w, childResolved[k].h,
				(int)( c->isFlexContainer && c->childCount > 0 ) );
#endif
			// Recurse into grandchildren. The child's box was ASSIGNED by the
			// flex pass above (c->resolvedRect = childResolved[k]) — pass that as
			// parent AND flag it resolved so the recursion lays out grandchildren
			// without re-resolving (and double-applying) the child's own size.
			if ( c->isFlexContainer && c->childCount > 0 ) {
				WUI_LayoutItemImpl( c, &childResolved[k], vpWidth, vpHeight, qtrue );
			}
		}
		}
	} else {
		// Absolute positioning: resolve children relative to this item
		for ( int i = 0; i < item->childCount; i++ ) {
			WUI_LayoutItemImpl( item->children[i], &item->resolvedRect, vpWidth, vpHeight, qfalse );
		}
	}
}

// Public entry: resolve an item and its subtree against `parent` from scratch.
void WUI_LayoutItem( wiredItemDef_t *item, const wuiPixelRect_t *parent,
                     float vpWidth, float vpHeight ) {
	WUI_LayoutItemImpl( item, parent, vpWidth, vpHeight, qfalse );
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
				// Menu-root flex child: box already assigned (flexResolved[i]) —
				// recurse in size-resolved mode so the child's own PERCENT size is
				// not applied a second time against its just-assigned box.
				if ( menu->items[idx]->isFlexContainer && menu->items[idx]->childCount > 0 ) {
					WUI_LayoutItemImpl( menu->items[idx], &flexResolved[i], vpWidth, vpHeight, qtrue );
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

	// Visual-regression instrumentation — no-op unless r_layoutDump != 0.
	WUI_DumpLayout( menu );
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
