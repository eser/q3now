// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026-present Wired Engine contributors
//
// policy/overlay.c — WUI_LAYER_OVERLAY activation predicate.
//
// Active whenever a menu is on screen. Topmost non-console layer — the
// cursor sprite + hover tooltip + transient floating modals (multi-dropdown)
// render here so they stay visible across every other menu/popup/debug
// state. V-24 splits this layer's walk into 3 sub-passes inside the
// compositor (cursor → tooltip → transient).
//
// Gated on "a menu is active", NOT on KEYCATCH_UI. The settings nav buttons
// transition via `close ; open` (options.wui:159 et al), which runs
// synchronously: `close` pops the stack to empty and WiredUI_PopMenu releases
// KEYCATCH_UI on the empty-stack edge (cl_wired_ui.c:4291-4310), then `open`
// re-pushes but never restores the catcher. Gating the cursor layer on the
// catcher made it blink out for that transition — the cursor vanished until the
// next click re-set KEYCATCH_UI. Vanilla Q3's invariant is "the cursor is drawn
// whenever a menu is up", which is exactly menu_policy_isActive(); mirroring it
// here severs the accidental cursor⇔catcher coupling. Inert in gameplay/attract
// (no menu active), same as before.

#include "../../../client.h"
#include "../cl_wired_ui.h"
#include "wui_layer_policy.h"

#if FEAT_WIRED_UI

qboolean overlay_policy_isActive( void )
{
	return ( WiredUI_GetActiveMenu() != NULL ) ? qtrue : qfalse;
}

#endif /* FEAT_WIRED_UI */
