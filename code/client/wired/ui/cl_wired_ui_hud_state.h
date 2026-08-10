// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
cl_wired_ui_hud_state.h — Wired UI HUD: state + receivers + tick.
*
* TRANSITION SHIM (V-13 2026-05-25)
* =================================
* Relocated from code/client/wired/hud/cl_wired_hud.h. WiredHud_* names survive in
* this header for one cycle: cl_cgame.c trap handler + 47 element files
* still reference them. Retire in [[wiredui-hud-element-relocation]].
*/

#ifndef CL_WIRED_UI_HUD_STATE_H
#define CL_WIRED_UI_HUD_STATE_H

#include "../../../cgame/cg_public.h"

#if FEAT_WIRED_UI

// global HUD state — written by cgame trap handler, read by element routines
extern wiredHudState_t  wired_hudStateStorage;
extern wiredHudState_t *wiredHud;

/* V-11 (2026-05-25): renamed from `wiredHud->valid` so the engine-tier
 * fail-fast gate has an explicit symbol. Set qtrue by WiredHud_ReceiveState
 * once cgame pushes its first state frame; queried by the compositor's HUD
 * layer activation predicate (skips the entire HUD-layer walk when qfalse,
 * preventing phantom predecorate+empty-text frames). */
extern qboolean wiredHud_state_valid;

// qfalse when the cl_drawHud cvar is 0 (HUD toggled off). The HUD-layer
// activation predicate (policy/hud.c) ANDs this so cl_drawHud 0 hides the
// HUD at the DRAW gate; the per-frame tick still runs so state stays fresh.
qboolean WiredHud_DrawEnabled( void );

// called from cl_cgame.c when cgame pushes state
void WiredHud_ReceiveState( wiredHudState_t *state );

// called from cl_cgame.c when cgame pushes an event (chat, frag, etc.)
void WiredHud_ReceiveEvent( int type, const char *data );

/* V-11 (2026-05-25): per-frame HUD tick (formerly WiredHud_Routine).
 * Called from cl_scrn.c::SCR_DrawScreenField when CA_ACTIVE +
 * wiredHud_state_valid; lazy-loads elements + syncs cg/cgs compat
 * structures + drives the scoreboard menu-visibility flip. */
void WiredUI_HudTick( int realtime );

// init / shutdown
void WiredHud_Init( void );
void WiredHud_Shutdown( void );

// data binding lookup (returns NULL if not found)
const wiredHudBinding_t *WiredHud_FindBinding( const char *name );

/* Permanent C-13 HUD registration entry point (formerly the transitionally-
 * named WiredUI_RegisterHudElementsLegacy / WiredHud_RegisterAll). Registers
 * wiredHudElementDefs[] + wiredHudFamilyDefs[] into the unified custom-draw
 * registry under the "hud:<name>" sigil. Called from WiredUI_Init after the
 * unified registry initialises and BEFORE the first .wui parse (so V-09
 * strict validation sees a populated registry). The table is load-bearing at
 * per-item-create time via the config-bridge adapter (signature mismatch
 * between the registry's wuiCustomDrawConfig_t-create and the elements'
 * modernhudConfig_t-create) — it is NOT retireable; see
 * cl_wired_ui_hud_register.c's header for the full rationale. */
void WiredHud_RegisterElements( void );

// visibility predicate for SE_* flag combinations.
// Used by the compositor's CUSTOM dispatch (cl_wired_clay.c) so the
// unified registry mirrors existing hudElement gating.
qboolean WiredHud_SE_Visible( int vflags );

#endif // FEAT_WIRED_UI
#endif // CL_WIRED_UI_HUD_STATE_H
