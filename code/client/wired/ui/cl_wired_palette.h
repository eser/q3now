// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
cl_wired_palette.h — Wired UI palette (mode × accent) overlay driver.

Distinct from cl_wired_theme.{c,h} (which maps semantic state labels —
critical / warning / normal — to colour values). This module drives the
runtime `_tokens.wui` overlay chain for the v2 design's `dark`/`light`
modes crossed with five accents (`amber`, `blood`, `toxic`, `cyan`,
`violet`).

Engine surface: two CVAR_ARCHIVE cvars (`ui_palette_mode`,
`ui_palette_accent`) registered with CVT_ENUM validation + onChange
callback that re-applies the overlay chain. Overlays load from
`ui/themes/<mode>/_tokens.wui` then `ui/themes/<accent>/_tokens.wui`;
the base `ui/_tokens.wui` declares every palette token first
(allowNew=qtrue) so the overlay's allowNew=qfalse path only sets
existing keys to new values.
*/

#ifndef CL_WIRED_PALETTE_H
#define CL_WIRED_PALETTE_H

#include "../../../qcommon/q_feats.h"

#if FEAT_WIRED_UI

void        WiredPalette_Init( void );

/* Re-apply the active mode + accent overlay chain. Called by the cvar
 * onChange path when `ui_palette_mode` or `ui_palette_accent` changes,
 * and once from WiredPalette_Init to seed the initial state. */
void        WiredPalette_Reload( void );

/* Dispatch 5.6 S3: poll-and-fire deferred reload. Called from the UI per-
 * frame tick. No-op unless a prior accent/mode change attempted reload
 * while cls.uiStarted=qfalse; then fires the queued reload once
 * uiStarted=qtrue. */
void        WiredPalette_TickPending( void );

const char *WiredPalette_CurrentMode( void );      /* "dark" | "light" */
const char *WiredPalette_CurrentAccent( void );    /* "amber"...   */

#endif /* FEAT_WIRED_UI */
#endif /* CL_WIRED_PALETTE_H */
