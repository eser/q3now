// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026-present Wired Engine contributors
//
// policy/hud.c — WUI_LAYER_HUD activation predicate.
//
// V-11 R-1 fail-fast gate retained: cgame must have pushed at least one
// state frame (wiredHud_state_valid) before the HUD layer activates,
// preventing the predecorate+empty-text phantom frames that appeared
// between CA_ACTIVE and the first WiredHud_ReceiveState.

#include "../../../client.h"
#include "../cl_wired_ui.h"
#include "../cl_wired_ui_hud_state.h"
#include "wui_layer_policy.h"

#if FEAT_WIRED_UI

qboolean hud_policy_isActive( void )
{
	// Pull-model active-app state (5.2.4.1 tier note); single-app == clientApps[0].
	// WiredHud_DrawEnabled() folds in cl_drawHud: cl_drawHud 0 hides the HUD at
	// this draw gate (the tick still runs, so re-enabling shows fresh data).
	return ( CL_ActiveApp()->state == CA_ACTIVE
	      && WiredUI_GetMenuStackDepth() == 0
	      && wiredHud_state_valid
	      && WiredHud_DrawEnabled() )
	     ? qtrue : qfalse;
}

#endif /* FEAT_WIRED_UI */
