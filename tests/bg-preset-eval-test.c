// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026-present Wired Engine contributors
//
// Contract for the background preset evaluator. Compiles the production
// translation unit (code/client/wired/ui/policy/wui_bg_preset.c) directly, so
// this checks the real table rather than a copy that can drift away from it.
//
// The case that matters most is `pop re-applies the parent`. The model this
// replaces stored an intent in the menu-stack slot at push time and read it
// back only for the stack-top panel, so any route that showed a menu without
// pushing left the slot unwritten — returning from a map put the main menu up
// on black, and entering any submenu repaired it because that path did push.
// Evaluating from the stack top every frame has no write to skip: after a pop
// the parent's own declaration is simply asked again. That property is what
// the sequence case below pins.

#include <stdio.h>
#include <string.h>

#include "policy/wui_bg_preset.h"

static int failures = 0;

static void CheckState( const char *name, const wuiBgLayerState_t *got,
                        int dark, int animated, int attractVis,
                        int attractPaused, int scrim )
{
	int ok = ( got->darkVisible     == dark )
	      && ( got->animatedVisible == animated )
	      && ( got->attractVisible  == attractVis )
	      && ( got->attractPaused   == attractPaused )
	      && ( got->menuScrim       == scrim );

	if ( ok ) {
		printf( "  ok   %-42s dark=%d anim=%d attract=%d/p%d scrim=%d\n",
			name, dark, animated, attractVis, attractPaused, scrim );
	} else {
		printf( "  FAIL %-42s got dark=%d anim=%d attract=%d/p%d scrim=%d,"
			" want %d/%d/%d/p%d/%d\n",
			name, got->darkVisible, got->animatedVisible, got->attractVisible,
			got->attractPaused, got->menuScrim,
			dark, animated, attractVis, attractPaused, scrim );
		failures++;
	}
}

static void CheckInt( const char *name, int got, int want )
{
	if ( got == want ) {
		printf( "  ok   %-42s -> %d\n", name, got );
	} else {
		printf( "  FAIL %-42s -> %d, want %d\n", name, got, want );
		failures++;
	}
}

int main( void )
{
	wuiBgLayerState_t s;
	wuiBgPreset_t     p;

	/* ── the four presets, exactly ─────────────────────────────────── */

	WUI_BgPresetEval( WUI_BG_PRESET_INVISIBLE, qtrue, qfalse, &s );
	CheckState( "invisible: attract shows through", &s, 0, 0, 1, 0, 0 );

	WUI_BgPresetEval( WUI_BG_PRESET_ANIMATED, qtrue, qfalse, &s );
	CheckState( "animated: backdrop on, attract paused", &s, 1, 1, 0, 1, 0 );

	WUI_BgPresetEval( WUI_BG_PRESET_DIM, qtrue, qfalse, &s );
	CheckState( "dim: game visible under a scrim", &s, 0, 0, 0, 1, 1 );

	WUI_BgPresetEval( WUI_BG_PRESET_NONE, qtrue, qfalse, &s );
	CheckState( "none: nothing behind", &s, 0, 0, 0, 1, 0 );

	/* A hidden attract must never be left running. Checked across every
	 * preset so a future row cannot quietly reintroduce the waste. */
	{
		int i;
		for ( i = 0; i < WUI_BG_PRESET_COUNT; i++ ) {
			WUI_BgPresetEval( (wuiBgPreset_t) i, qtrue, qfalse, &s );
			if ( !s.attractVisible && !s.attractPaused ) {
				printf( "  FAIL %-42s preset '%s' hides attract but leaves it running\n",
					"hidden attract is always paused", WUI_BgPresetName( (wuiBgPreset_t) i ) );
				failures++;
			}
		}
		if ( !failures )
			printf( "  ok   %-42s all %d presets\n",
				"hidden attract is always paused", WUI_BG_PRESET_COUNT );
	}

	/* ── no menu at all: bare attract, preset ignored ──────────────── */

	WUI_BgPresetEval( WUI_BG_PRESET_ANIMATED, qfalse, qfalse, &s );
	CheckState( "no menu: attract runs alone", &s, 0, 0, 1, 0, 0 );

	/* ── loading owns the backdrop whatever was on top ─────────────── */

	WUI_BgPresetEval( WUI_BG_PRESET_INVISIBLE, qtrue, qtrue, &s );
	CheckState( "loading overrides invisible", &s, 1, 1, 0, 1, 0 );

	WUI_BgPresetEval( WUI_BG_PRESET_DIM, qtrue, qtrue, &s );
	CheckState( "loading overrides dim", &s, 1, 1, 0, 1, 0 );

	/* ── 🔴 the regression this design exists to make impossible ───── */

	/* main (invisible) -> deep menu (animated) -> ESC pops back to main.
	 * Nothing was stored on the way in, so nothing has to be restored on the
	 * way out: asking the new stack top gives main's own declaration back.
	 * Under the old slot model this is precisely where the scene was lost. */
	{
		wuiBgLayerState_t before, during, after;

		WUI_BgPresetEval( WUI_BG_PRESET_INVISIBLE, qtrue, qfalse, &before );
		WUI_BgPresetEval( WUI_BG_PRESET_ANIMATED,  qtrue, qfalse, &during );
		WUI_BgPresetEval( WUI_BG_PRESET_INVISIBLE, qtrue, qfalse, &after );

		CheckInt( "pop re-applies the parent's declaration",
			memcmp( &before, &after, sizeof( before ) ) == 0, 1 );
		CheckInt( "the submenu really did differ",
			memcmp( &before, &during, sizeof( before ) ) != 0, 1 );
	}

	/* Returning from a map: the menu comes back with no push in between.
	 * There is no separate "was pushed" input to the evaluator at all, which
	 * is the structural reason the black-menu bug cannot recur. */
	{
		wuiBgLayerState_t fresh, returned;

		WUI_BgPresetEval( WUI_BG_PRESET_INVISIBLE, qtrue, qfalse, &fresh );
		WUI_BgPresetEval( WUI_BG_PRESET_INVISIBLE, qtrue, qtrue,  &s );       /* map loads */
		WUI_BgPresetEval( WUI_BG_PRESET_INVISIBLE, qtrue, qfalse, &returned ); /* disconnect */

		CheckInt( "menu after a map matches a cold boot",
			memcmp( &fresh, &returned, sizeof( fresh ) ) == 0, 1 );
	}

	/* ── parsing ───────────────────────────────────────────────────── */

	CheckInt( "parse 'invisible'",
		WUI_BgPresetParse( "invisible", &p ) && p == WUI_BG_PRESET_INVISIBLE, 1 );
	CheckInt( "parse 'animated'",
		WUI_BgPresetParse( "animated", &p ) && p == WUI_BG_PRESET_ANIMATED, 1 );
	CheckInt( "parse 'dim'",
		WUI_BgPresetParse( "dim", &p ) && p == WUI_BG_PRESET_DIM, 1 );
	CheckInt( "parse 'none'",
		WUI_BgPresetParse( "none", &p ) && p == WUI_BG_PRESET_NONE, 1 );
	CheckInt( "parse is case-insensitive",
		WUI_BgPresetParse( "ANIMATED", &p ) && p == WUI_BG_PRESET_ANIMATED, 1 );
	CheckInt( "unknown name is rejected",
		WUI_BgPresetParse( "layer1", &p ), 0 );
	CheckInt( "NULL name is rejected",
		WUI_BgPresetParse( NULL, &p ), 0 );

	/* Round-trip: every preset's canonical name parses back to itself, so a
	 * renamed row cannot desync the table from the parser. */
	{
		int i, ok = 1;
		for ( i = 0; i < WUI_BG_PRESET_COUNT; i++ ) {
			wuiBgPreset_t back;
			if ( !WUI_BgPresetParse( WUI_BgPresetName( (wuiBgPreset_t) i ), &back )
			  || back != (wuiBgPreset_t) i ) ok = 0;
		}
		CheckInt( "every name round-trips", ok, 1 );
	}

	/* Malformed input falls back to the deep-menu look rather than crashing
	 * or drawing nothing. */
	WUI_BgPresetEval( (wuiBgPreset_t) 999, qtrue, qfalse, &s );
	CheckState( "out-of-range falls back to animated", &s, 1, 1, 0, 1, 0 );

	if ( failures ) {
		printf( "==> FAIL: %d case(s)\n", failures );
		return 1;
	}
	printf( "==> PASS: background preset contract\n" );
	return 0;
}
