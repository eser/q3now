// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026-present Wired Engine contributors
//
// policy/bg_attract.c — WUI_LAYER_BG_ATTRACT activation predicate.
//
// Active in two cases:
//   (1) client is disconnected AND no menu is open — the idle attract panels
//       (brand splash, leaderboard) live here. Attract and the menu are
//       EXCLUSIVE surfaces (Eser 2026-07-02): opening the menu hides attract
//       rather than layering over it. The earlier Q1/Doom "demo keeps playing
//       behind the menu" model was dropped as clutter.
//   (2) attract is playing a demo it owns — the demo runs at CA_CONNECTED (not
//       DISCONNECTED), so this predicate keys on the demo-overlay window to keep
//       the bg_attract layer active and render the poster strip/index overlay
//       ON TOP of the match. This is attract's OWN chrome, not the user menu —
//       the exclusive-with-menu rule (case 1) is unchanged.
// Loading and in-game states fall out automatically (state != CA_DISCONNECTED
// and no attract-owned demo).

#include "../../../client.h"
#include "../cl_wired_ui.h"
#include "../cl_wired_attract.h"
#include "wui_layer_policy.h"

#if FEAT_WIRED_UI

qboolean bg_attract_policy_isActive( void )
{
	// Demo-overlay window: attract owns a playing demo → keep the layer up so
	// the overlay panel renders over the match (state is CA_CONNECTED here).
	if ( WiredAttract_IsDemoOverlayActive() )
		return qtrue;

	// Pull-model: read connection state from the active app (client-tier
	// accessor), not the global cls — see 5.2.4.1 tier note. Single-app:
	// CL_ActiveApp() == &clientApps[0], byte-identical to the former cls.state.
	if ( CL_ActiveApp()->state != CA_DISCONNECTED )
		return qfalse;
	// Menu is up → attract hides (exclusive swap, not layered backdrop).
	if ( menu_policy_isActive() )
		return qfalse;
	return qtrue;
}

#endif /* FEAT_WIRED_UI */
