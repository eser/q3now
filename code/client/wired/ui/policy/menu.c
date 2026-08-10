// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026-present Wired Engine contributors
//
// policy/menu.c — WUI_LAYER_MENU activation predicate.
//
// Active whenever WiredUI_GetActiveMenu would return non-NULL — that is,
// when a menu stack entry exists OR when stackDepth is 0 but the engine
// has an implicit root (main/ingame) set via wui_activeMenu. Existing
// root-menu flows route through here without needing explicit push/pop.

#include "../../../client.h"
#include "../cl_wired_ui.h"
#include "wui_layer_policy.h"

#if FEAT_WIRED_UI

qboolean menu_policy_isActive( void )
{
	return ( WiredUI_GetActiveMenu() != NULL ) ? qtrue : qfalse;
}

#endif /* FEAT_WIRED_UI */
