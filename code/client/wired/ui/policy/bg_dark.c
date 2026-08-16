// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026-present Wired Engine contributors
//
// policy/bg_dark.c — WUI_LAYER_BG_DARK activation predicate.
//
// Unlike its siblings, this layer's visibility is not a function of the
// connection state: it is declared by whichever menu is on top, through the
// background preset (policy/wui_bg_preset.h). The compositor resolves the
// preset each frame and stores the answer; this predicate only reports it.
//
// That indirection is the point. The model this replaces wrote a background
// intent into the menu-stack slot at push time, so any path that showed a menu
// without pushing left it unwritten and the backdrop silently vanished. A
// predicate that asks rather than remembers has nothing to lose.
//
// Until the compositor wiring lands (step 3 of the layer refactor) this
// reports qfalse, so the layer exists and is addressable by the dev override
// (wui_layer_test_single) without yet changing what anyone sees.

#include "../../../client.h"
#include "../cl_wired_ui.h"
#include "wui_layer_policy.h"

#if FEAT_WIRED_UI

qboolean bg_dark_policy_isActive( void )
{
	return qfalse;
}

#endif /* FEAT_WIRED_UI */
