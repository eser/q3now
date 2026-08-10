// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026-present Wired Engine contributors
//
// policy/debug_overlay.c — WUI_LAYER_DEBUG_OVERLAY activation predicate.
//
// Single-cvar functional gate: `wired_ui_debug != 0` activates the layer.
// The legacy `wui_layer_active` predicate also tested the engine-wide
// `developer` cvar; that compound gate violated Memory K16 "no diagnostic
// cvars" (the `developer` cvar is treated as retired for new code). The
// layer-specific `wired_ui_debug` is the only opt-in needed — modders
// flip it on per-session to surface the layer.

#include "../../../client.h"
#include "wui_layer_policy.h"

#if FEAT_WIRED_UI

qboolean debug_overlay_policy_isActive( void )
{
	int wuiDbg = (int) Cvar_VariableValue( "wired_ui_debug" );
	return ( wuiDbg != 0 ) ? qtrue : qfalse;
}

#endif /* FEAT_WIRED_UI */
