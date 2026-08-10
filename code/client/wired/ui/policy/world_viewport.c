// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026-present Wired Engine contributors
//
// policy/world_viewport.c — WUI_LAYER_WORLD_VIEWPORT activation predicate.
//
// V-19/V-21 (2026-05-25): active whenever the engine is in a state that
// expects world rendering AND the cgame VM has registered at least one
// viewport provider (typically "main_scene" — see CG_Init). The provider's
// own input_mode + lifetime keep this predicate one boolean; per-provider
// gating happens at the walk handler.

#include "../../../client.h"
#include "../cl_wired_viewport.h"
#include "wui_layer_policy.h"

#if FEAT_WIRED_UI

qboolean world_viewport_policy_isActive( void )
{
	/* Activate when ANY app that owns a registered viewport provider is in a
	 * world-rendering state — not when the input-focused app is. This lets an
	 * additional client app's viewport drive the layer while the focused app
	 * keeps input. The renderable test (in-game, or primed with a cgame VM —
	 * the latter draws the world backdrop under the loading screen) is applied
	 * per owning-app inside the client-tier query; this policy consumes only a
	 * bool and dereferences no per-app state (tier-clean).
	 *
	 * Single-client: the sole owner is the one connected app (the focused app),
	 * so this returns exactly what the focused-app state check returned.
	 *
	 * The per-itemDef provider lookup still happens at the walk handler; a
	 * missing provider there emits the red placeholder rect (elements/viewport.c)
	 * rather than skipping the walk. */
	return CL_AnyViewportAppRenderable();
}

#endif /* FEAT_WIRED_UI */
