// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
cl_wired_hud_elem_markerlist.c — Wired UI world-anchored marker-list element (WA-2a)

Generic, STATELESS world-anchored text element: each frame it reads its bound
marker list (WiredStore_GetMarkerList, fed by cgame's WUI_StageMarkers_* via the
PushMarkerList syscall) and draws every marker via Text_Draw at its ABSOLUTE
real-pixel position. cgame computes WHERE (world→screen, CG_WorldToScreenPixels);
this element draws WHAT — the inversion-of-control affordance for MULTI-INSTANCE
world-anchored UI (damage plums in WA-2b, bot directives in WA-3).

No per-marker cache/id: the list churns every frame (count varies 0..N), so the
element re-reads and re-draws from scratch each frame — caching by index would go
stale. The bound listKey comes from the .wui item's `bind` field.
*/

#include "../../../client.h"
#include "cl_wired_ui_hud_compat.h"
#include "cl_wired_ui_hud_private.h"
#include "cl_wired_store.h"
#include "cl_wired_text.h"

#if FEAT_WIRED_UI

/* Real-pixel text size for marker labels (~SMALLCHAR_HEIGHT analog). Fixed for
 * WA-2a; a future phase may derive it from the .wui fontsize if needed. */
#define MARKERLIST_TEXT_SIZE_PX   16.0f

typedef struct {
	modernhudConfig_t config;
} modernHudElementMarkerList_t;

void *CG_ModernHUDElementMarkerListCreate( const modernhudConfig_t *config ) {
	modernHudElementMarkerList_t *element;
	ModernHUD_ELEMENT_INIT( element, config );
	return element;
}

void CG_ModernHUDElementMarkerListRoutine( void *context ) {
	modernHudElementMarkerList_t *element = (modernHudElementMarkerList_t *)context;
	const wuiMarker_t *markers;
	int count = 0;
	int i;

	if ( !element ) {
		return;
	}
	/* the listKey to draw comes from the .wui item's bind (e.g. "markers.plums") */
	if ( !element->config.bind.isSet || !element->config.bind.value[0] ) {
		return;
	}

	markers = WiredStore_GetMarkerList( element->config.bind.value, &count );
	if ( !markers || count <= 0 ) {
		return;
	}

	for ( i = 0; i < count; i++ ) {
		Text_Draw( markers[i].text, markers[i].x, markers[i].y,
		           FONT_UI, MARKERLIST_TEXT_SIZE_PX, markers[i].color,
		           TEXT_ALIGN_CENTER, TEXT_DROPSHADOW );
	}
}

#endif /* FEAT_WIRED_UI */
