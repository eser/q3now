// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026-present Wired Engine contributors
//
// policy/loading.c — WUI_LAYER_LOADING activation predicate.
//
// Active for the entire connection lifecycle below CA_ACTIVE so the loading
// panel covers the wire-protocol stages users perceive as "loading the map".

#include "../../../client.h"
#include "wui_layer_policy.h"

#if FEAT_WIRED_UI

qboolean loading_policy_isActive( void )
{
	// Pull-model active-app state (5.2.4.1 tier note); single-app == clientApps[0].
	switch ( CL_ActiveApp()->state ) {
	case CA_CONNECTING:
	case CA_CHALLENGING:
	case CA_CONNECTED:
	case CA_LOADING:
	case CA_PRIMED:
		return qtrue;
	default:
		return qfalse;
	}
}

#endif /* FEAT_WIRED_UI */
