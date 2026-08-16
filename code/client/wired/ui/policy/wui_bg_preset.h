// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026-present Wired Engine contributors
//
// wui_bg_preset.h — what a menu asks of the background layers.
//
// A menu declares a preset; it never names a layer. The preset -> layer
// mapping lives in exactly one table (wui_bg_preset.c), so inserting a layer
// changes behaviour without touching a single .wui file.
//
// Presets drive ONLY the background family (dark, animated, attract) plus the
// menu scrim. game / loading / menu / console keep their own state-driven
// policies in this same directory — that is what keeps "menus never name
// layers" true by construction rather than by convention.
//
// EVALUATED EVERY FRAME FROM THE STACK TOP, never written at push time.
// The old model stored an intent in the menu-stack slot
// (wui_menuBgIntent[depth]) and applied it only to the stack-top panel, so any
// path that showed a menu without pushing left the slot unwritten and the
// scene simply vanished — that is exactly what happened returning from a map.
// Asking the top of the stack each frame has no write to skip: popping back to
// a parent re-applies the parent's declaration for free, because nothing was
// ever stored to go stale.

#ifndef WUI_BG_PRESET_H
#define WUI_BG_PRESET_H

#include "../../../../qcommon/q_shared.h"
#include "../../../../qcommon/q_feats.h"

#if FEAT_WIRED_UI

typedef enum {
	/* The composed backdrop — dark base plus the animated scene. Deep menus
	 * use this, and it is the default for any menu that declares nothing.
	 *
	 * Deliberately zero: menuDef is zero-initialised, so a menu that never
	 * says `background` lands here rather than on whatever happened to be
	 * first in the enum. An undeclared field falling into "draw nothing"
	 * would be a black screen that looks like a bug and reads like a
	 * deliberate choice. */
	WUI_BG_PRESET_ANIMATED = 0,
	/* Let the attract reel show through. The main menu's look: Quake 1 drew
	 * its menu straight over whatever the attract demo was doing. */
	WUI_BG_PRESET_INVISIBLE,
	/* Live gameplay with a scrim over it — the in-game menu. */
	WUI_BG_PRESET_DIM,
	/* Nothing behind the menu — fullscreen console, error dialogs. */
	WUI_BG_PRESET_NONE,

	WUI_BG_PRESET_COUNT
} wuiBgPreset_t;

/* Resolved per-layer state for one frame. `paused` is meaningful only when
 * `visible` is false — a hidden layer that keeps animating is wasted work,
 * which is the whole point of tracking it separately. */
typedef struct {
	qboolean darkVisible;
	qboolean animatedVisible;
	qboolean attractVisible;
	qboolean attractPaused;
	qboolean menuScrim;
} wuiBgLayerState_t;

/* Parse a preset name from a .wui menuDef. Returns qfalse for unknown names
 * and leaves *out untouched; the caller reports the parse error with its own
 * file/line context. Exactly one canonical spelling each, no aliases. */
qboolean WUI_BgPresetParse( const char *name, wuiBgPreset_t *out );

/* Canonical name for a preset — diagnostics and the layer-state log. */
const char *WUI_BgPresetName( wuiBgPreset_t preset );

/* The frame evaluation. `hasMenu` is false when no menu is up at all (bare
 * attract), in which case the preset is ignored and attract runs alone.
 * `isLoading` forces the composed backdrop: a map load shows the scene behind
 * its progress panel regardless of which menu was on top when it started. */
void WUI_BgPresetEval( wuiBgPreset_t preset, qboolean hasMenu,
                       qboolean isLoading, wuiBgLayerState_t *out );

#endif /* FEAT_WIRED_UI */
#endif /* WUI_BG_PRESET_H */
