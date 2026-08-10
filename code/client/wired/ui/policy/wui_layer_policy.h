// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026-present Wired Engine contributors
//
// wui_layer_policy.h — V-21 per-layer activation predicates.
//
// The compositor's wui_layer_active() dispatcher in cl_wired_clay.c routes
// each WUI_LAYER_* to the matching <layer>_policy_isActive() predicate
// declared here. Each predicate is owned by one small module in this
// directory; that keeps cross-layer rules (CA_ACTIVE, KEYCATCH_*, viewport
// provider count, R-1 fail-fast gate) isolated to the module that owns the
// rule rather than buried in the dispatcher.
//
// The dev force-override mask (wui_layer_test_single / _stack) is applied
// in the dispatcher itself BEFORE the policy call; predicates only encode
// the production rule.

#ifndef WUI_LAYER_POLICY_H
#define WUI_LAYER_POLICY_H

#include "../../../../qcommon/q_shared.h"
#include "../../../../qcommon/q_feats.h"

#if FEAT_WIRED_UI

qboolean bg_attract_policy_isActive    ( void );
qboolean loading_policy_isActive       ( void );
qboolean world_viewport_policy_isActive( void );
qboolean hud_policy_isActive           ( void );
qboolean menu_policy_isActive          ( void );
qboolean popup_policy_isActive         ( void );
qboolean debug_overlay_policy_isActive ( void );
qboolean overlay_policy_isActive       ( void );
qboolean console_policy_isActive       ( void );

#endif /* FEAT_WIRED_UI */
#endif /* WUI_LAYER_POLICY_H */
