// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026-present Wired Engine contributors
//
// policy/popup.c — WUI_LAYER_POPUP activation predicate.
//
// Active whenever the popup queue has at least one entry. Stacks above the
// menu layer so a popup overlays whatever main menu / ingame panel is open.

#include "../../../client.h"
#include "../cl_wired_ui.h"
#include "../cl_wired_compositor.h"
#include "wui_layer_policy.h"

#if FEAT_WIRED_UI

qboolean popup_policy_isActive( void )
{
	return WiredUI_HasActivePopup();
}

#endif /* FEAT_WIRED_UI */
