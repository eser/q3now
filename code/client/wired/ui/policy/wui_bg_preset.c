// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026-present Wired Engine contributors
//
// wui_bg_preset.c — the single preset -> background-layer table.
//
// Deliberately free of engine headers so tests/bg-preset-eval-test.c can
// compile this exact translation unit and check the real table rather than a
// copy of it.

#include "wui_bg_preset.h"

#include <ctype.h>
#include <string.h>

#if FEAT_WIRED_UI

/* Local case-insensitive compare rather than Q_stricmp: that lives in the
 * engine's shared library, and linking it here would defeat the point of
 * keeping this TU engine-free so the contract test can compile it directly. */
static int wui_bg_stricmp( const char *a, const char *b )
{
	while ( *a && *b ) {
		int ca = tolower( (unsigned char) *a++ );
		int cb = tolower( (unsigned char) *b++ );
		if ( ca != cb ) return ca - cb;
	}
	return (unsigned char) *a - (unsigned char) *b;
}

/* One row per preset. This is the only place in the codebase that knows which
 * background layers exist; .wui files name presets, never layers. */
typedef struct {
	const char        *name;
	wuiBgLayerState_t  state;
} wuiBgPresetRow_t;

static const wuiBgPresetRow_t wui_bg_preset_table[ WUI_BG_PRESET_COUNT ] = {
	/* NONE — nothing behind. Attract is stopped, not merely hidden: a
	 * fullscreen console or an error dialog means the reel has no business
	 * still advancing behind it. */
	[ WUI_BG_PRESET_NONE ] = { "none", {
		/* dark */ qfalse, /* animated */ qfalse,
		/* attractVisible */ qfalse, /* attractPaused */ qtrue,
		/* menuScrim */ qfalse } },

	/* INVISIBLE — the attract reel shows through the menu. Both backdrop
	 * layers stay off precisely so it can. */
	[ WUI_BG_PRESET_INVISIBLE ] = { "invisible", {
		qfalse, qfalse,
		qtrue,  qfalse,
		qfalse } },

	/* ANIMATED — the composed backdrop: dark base under the animated scene.
	 * Attract is hidden AND paused; leaving it running behind an opaque
	 * backdrop burns a demo playback and a scheduler tick for pixels nobody
	 * sees. */
	[ WUI_BG_PRESET_ANIMATED ] = { "animated", {
		qtrue,  qtrue,
		qfalse, qtrue,
		qfalse } },

	/* DIM — whatever is behind, with a scrim over it. Deliberately not
	 * "gameplay with a scrim": the scrim lives on the MENU layer and darkens
	 * whatever it lands on, so the same preset covers the in-game menu (match
	 * underneath) and the main menu (attract underneath). Making it specific
	 * to one of those would have meant two presets that differ only in which
	 * layer happens to be lit.
	 *
	 * Attract is left VISIBLE and RUNNING here — the point is to see it
	 * through the scrim. The backdrop layers stay off so they do not cover
	 * whatever we are dimming. */
	[ WUI_BG_PRESET_DIM ] = { "dim", {
		qfalse, qfalse,
		qtrue,  qfalse,
		qtrue } },
};

qboolean WUI_BgPresetParse( const char *name, wuiBgPreset_t *out )
{
	int i;

	if ( !name || !out ) return qfalse;

	for ( i = 0; i < WUI_BG_PRESET_COUNT; i++ ) {
		if ( !wui_bg_stricmp( name, wui_bg_preset_table[ i ].name ) ) {
			*out = (wuiBgPreset_t) i;
			return qtrue;
		}
	}
	return qfalse;
}

const char *WUI_BgPresetName( wuiBgPreset_t preset )
{
	if ( preset < 0 || preset >= WUI_BG_PRESET_COUNT ) return "?";
	return wui_bg_preset_table[ preset ].name;
}

void WUI_BgPresetEval( wuiBgPreset_t preset, qboolean hasMenu,
                       qboolean isLoading, wuiBgLayerState_t *out )
{
	if ( !out ) return;

	/* A map load owns the backdrop, and it is asked FIRST — before the no-menu
	 * case, not after it.
	 *
	 * Ordering these the other way round is what left the loading screen on
	 * bare attract with no backdrop at all. CL_MapLoading calls
	 * WiredUI_CloseAllMenus() before the load — it must, or a surviving stack
	 * re-asserts KEYCATCH_UI on the next frame and trips the cgame's
	 * "KEYCATCH_UI is 0 at CA_LOADING" invariant. That drain empties the stack
	 * AND sets the root to UIMENU_NONE, so WiredUI_GetActiveMenu() returns
	 * NULL and hasMenu is ALWAYS false while a map loads. An isLoading branch
	 * sitting behind the !hasMenu early-out is therefore unreachable in
	 * production: it can only fire for a caller that has a menu and a load at
	 * once, which the drain makes impossible.
	 *
	 * The loading panel is not a menu-stack entry — it lives on its own layer
	 * (WUI_LAYER_LOADING, above the world viewport, below the menu). That is
	 * exactly why "is a menu up" is the wrong question to ask ahead of "is a
	 * map loading": the thing that needs the backdrop is not a menu. */
	if ( isLoading ) {
		*out = wui_bg_preset_table[ WUI_BG_PRESET_ANIMATED ].state;
		return;
	}

	/* No menu and no load — bare attract. The preset argument is whatever the
	 * last menu happened to hold and is deliberately ignored, so a stale value
	 * can never leak into the no-menu case. */
	if ( !hasMenu ) {
		out->darkVisible     = qfalse;
		out->animatedVisible = qfalse;
		out->attractVisible  = qtrue;
		out->attractPaused   = qfalse;
		out->menuScrim       = qfalse;
		return;
	}

	/* Out-of-range guard rather than an assert: a malformed .wui should fall
	 * back to the deep-menu look, not take the client down. */
	if ( preset < 0 || preset >= WUI_BG_PRESET_COUNT )
		preset = WUI_BG_PRESET_ANIMATED;

	*out = wui_bg_preset_table[ preset ].state;
}

#endif /* FEAT_WIRED_UI */
