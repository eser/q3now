// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026-present Wired Engine contributors
//
// policy/console.c — WUI_LAYER_CONSOLE activation predicate.
//
// V-20/V-23 (2026-05-25): active when KEYCATCH_CONSOLE is on (`~` toggle).
// The console layer claims input exclusively while active (V-23) — Enter /
// Tab / Up / Down / PgUp / PgDn / Esc route to elements/console.c without
// falling through to other catchers. The `~` toggle key itself remains
// engine-reserved (cl_keys.c handles it before any catcher walks).
//
// (2026-06-10) the layer must ALSO activate every frame for the two
// always-on surfaces the deleted panels/console.c drew unconditionally —
// otherwise Con_DrawNotify (in-game notify lines + messagemode prompt) and
// the fullscreen fallback console (the recovery surface shown when WiredUI
// is unhealthy and no other catcher owns the screen) never render.
//   - SOLID/full console stays gated on KEYCATCH_CONSOLE — Con_DrawConsole
//     itself only draws the solid panel when the slide fraction is > 0
//     (driven by Con_RunConsole + KEYCATCH_CONSOLE), so the broader policy
//     does NOT force the full console on.
//   - The console_view itemDef is `decoration` only (no action / no focus),
//     so widening the policy adds no spurious input/action claim — input
//     still routes through the legacy KEYCATCH_CONSOLE path.

#include "../../../client.h"
#include "../cl_wired_ui.h"
#include "wui_layer_policy.h"

#if FEAT_WIRED_UI

qboolean console_policy_isActive( void )
{
	connstate_t state = CL_ActiveApp()->state;

	/* (1) Explicit `~` toggle / immediate console — the solid console. */
	if ( ( Key_GetCatcher() & KEYCATCH_CONSOLE ) != 0 ) {
		return qtrue;
	}

	/* (2) Fullscreen fallback recovery surface, shown ONLY when WiredUI is
	 * unhealthy (no other catcher owns the screen, no passive loading bar).
	 * When WiredUI is healthy the disconnected screen is the attract base
	 * layer, not the console — attract is what shows, and `~` opens the
	 * console over it. The recovery console is the offline safety net. */
	if ( state < CA_ACTIVE
	  && !( Key_GetCatcher() & ( KEYCATCH_UI | KEYCATCH_CGAME ) )
	  && cl_loadProgress.startTime <= 0
	  && !WiredUI_IsHealthy() ) {
		return qtrue;
	}

	/* (3) In-game notify lines + messagemode prompt overlay (Con_DrawNotify,
	 * fires only when the slide fraction is 0 and the state is CA_ACTIVE). */
	if ( state == CA_ACTIVE ) {
		return qtrue;
	}

	return qfalse;
}

#endif /* FEAT_WIRED_UI */
