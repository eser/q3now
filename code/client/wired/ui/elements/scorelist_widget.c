// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026-present Wired Engine contributors
//
// elements/scorelist_widget.c — WiredUI `type scorelist_widget` itemDef
// render handler (V-13b 2026-05-25, 3c declarative scoreboard migration).
//
// The .wui scoreboard panels (ingame_scoreboard_*, end_scoreboard_*) place a
// single scorelist_widget itemDef inside their menu; the compositor walk
// emits a Clay CUSTOM command with "scorelist:<subtype>" in cmd->name and
// the CUSTOM dispatch routes here.
//
// Subtype selection:
//   - item->group "duel"  → DUELBOARD layout (two fighter panels)
//   - item->group "team"  → SCORELIST team filter via item->feeder
//                            (feederID 0x05/0x06 = red/blue)
//   - default             → full SCORELIST (FFA + spectator block)
//
// The actual rendering lives in cl_wired_ui_hud_scoreboard.c — this element
// is the dispatch glue that lets the existing widget functions run from a
// declarative itemDef instead of the cl_wired_hud.c fallback path that
// V-13 retired.

#include "../../client.h"
#include "../cl_wired_ui.h"
#include "../cl_wired_ui_hud_state.h"

LOG_DECLARE_CHANNEL( ch_ui, "ui" );

#if FEAT_WIRED_UI

extern void WiredHud_DrawScorelistWidget( float ox, float oy, float ow, float oh,
                                          int feederID, const vec4_t textColor );
extern void WiredHud_DrawDuelBoard      ( float ox, float oy, float ow, float oh );

void WiredUI_RenderScorelistWidget( float x, float y, float w, float h,
                                     const vec4_t color, const char *subtype )
{
	if ( w <= 0.0f || h <= 0.0f ) return;
	if ( !wiredHud || !wiredHud_state_valid ) return;

	/* Subtype dispatch. The subtype string follows the "scorelist:" sigil
	 * in cmd->name and is sourced from item->group at parse time. */
	if ( subtype && !Q_stricmp( subtype, "duel" ) ) {
		WiredHud_DrawDuelBoard( x, y, w, h );
		return;
	}
	if ( subtype && !Q_stricmp( subtype, "red" ) ) {
		WiredHud_DrawScorelistWidget( x, y, w, h, 0x05, color );
		return;
	}
	if ( subtype && !Q_stricmp( subtype, "blue" ) ) {
		WiredHud_DrawScorelistWidget( x, y, w, h, 0x06, color );
		return;
	}

	/* Default: full SCORELIST (FFA + spectator block). 0x0b matches the
	 * legacy cl_wired_hud.c fallback's FFA feederID. */
	WiredHud_DrawScorelistWidget( x, y, w, h, 0x0b, color );
}

#endif /* FEAT_WIRED_UI */
