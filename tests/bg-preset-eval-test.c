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
//
// The second thing pinned here is the LOADING screen, and it is the case an
// earlier version of this file got wrong by testing a shape production never
// produces. Its two loading cases both passed hasMenu=qtrue; a real load never
// has a menu, because CL_MapLoading drains the stack before it starts. Every
// frame of every load is hasMenu=qfalse + isLoading=qtrue, which went untested,
// and behind the !hasMenu early-out the isLoading branch was unreachable — the
// loading screen drew over bare attract with no backdrop at all. A test whose
// inputs cannot occur cannot catch anything, so the cases below use the shape
// the engine actually passes.

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

	/* dim darkens whatever is behind, not specifically the game: the scrim
	 * lives on the menu layer, so the same preset serves the in-game menu
	 * (match underneath) and the main menu (attract underneath). Attract is
	 * left running because seeing it through the scrim is the point. */
	WUI_BgPresetEval( WUI_BG_PRESET_DIM, qtrue, qfalse, &s );
	CheckState( "dim: whatever is behind, scrimmed", &s, 0, 0, 1, 0, 1 );

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

	/* ── 🔴 loading with NO menu: the only shape this ever has ─────── */

	/* The two cases above pass hasMenu=qtrue, which never happens during a
	 * real load and is why they went on passing while the loading screen drew
	 * on bare attract. CL_MapLoading calls WiredUI_CloseAllMenus() before the
	 * load — it has to, or a surviving stack re-asserts KEYCATCH_UI and trips
	 * the cgame's "KEYCATCH_UI is 0 at CA_LOADING" invariant — and that drain
	 * empties the stack AND sets the root to UIMENU_NONE. So every frame of
	 * every load arrives here as hasMenu=qfalse, isLoading=qtrue.
	 *
	 * Measured on this exact combination before the fix (map arena1, release
	 * build, WIRED_BGPROBE in wui_clay_resolve_layer_states):
	 *
	 *   state=6(CA_LOADING) isLoading=1 top=<null> dark=0 anim=0 attract=1/p0
	 *   bgCmds=SKIPPED (no bg layer visible)
	 *
	 * and after:
	 *
	 *   state=6(CA_LOADING) isLoading=1 top=<null> dark=1 anim=1 attract=0/p1
	 *   bgCmds=190
	 *
	 * The fix is purely one of ORDER — isLoading is asked before !hasMenu.
	 * Swapping the two branches back makes this case, and only this case,
	 * fail. */
	WUI_BgPresetEval( WUI_BG_PRESET_ANIMATED, qfalse, qtrue, &s );
	CheckState( "loading with no menu: composed backdrop", &s, 1, 1, 0, 1, 0 );

	/* The preset argument is dead during a load whatever it holds — the value
	 * left over from the menu that launched the map must not leak through.
	 * INVISIBLE is the one that matters: it is what `main` declares, so it is
	 * the stale value a real load actually carries, and it is the one preset
	 * whose own answer (attract visible, no backdrop) is exactly the broken
	 * behaviour measured above. */
	{
		int           i;
		wuiBgLayerState_t want;
		int           ok = 1;

		WUI_BgPresetEval( WUI_BG_PRESET_ANIMATED, qfalse, qtrue, &want );
		for ( i = 0; i < WUI_BG_PRESET_COUNT; i++ ) {
			WUI_BgPresetEval( (wuiBgPreset_t) i, qfalse, qtrue, &s );
			if ( memcmp( &s, &want, sizeof( s ) ) != 0 ) ok = 0;
			WUI_BgPresetEval( (wuiBgPreset_t) i, qtrue,  qtrue, &s );
			if ( memcmp( &s, &want, sizeof( s ) ) != 0 ) ok = 0;
		}
		CheckInt( "loading ignores preset and hasMenu alike", ok, 1 );
	}

	/* Attract must be hidden AND paused behind the loading screen. An attract
	 * reel still advancing behind an opaque backdrop burns a demo playback for
	 * pixels nobody sees, and the reel would be mid-slide when the map lands. */
	WUI_BgPresetEval( WUI_BG_PRESET_INVISIBLE, qfalse, qtrue, &s );
	CheckInt( "loading hides attract", !s.attractVisible, 1 );
	CheckInt( "loading pauses attract", s.attractPaused, 1 );

	/* Nothing scrims the loading screen: the composed backdrop is the
	 * loading look, not a dimmed something-else. */
	CheckInt( "loading is never scrimmed", !s.menuScrim, 1 );

	/* A load with no menu must differ from bare attract with no menu — the
	 * assertion that isLoading is actually consulted on this path rather than
	 * being shadowed by the !hasMenu early-out, which is the whole defect. */
	{
		wuiBgLayerState_t bare, loading;

		WUI_BgPresetEval( WUI_BG_PRESET_INVISIBLE, qfalse, qfalse, &bare );
		WUI_BgPresetEval( WUI_BG_PRESET_INVISIBLE, qfalse, qtrue,  &loading );

		CheckInt( "no-menu load differs from no-menu attract",
			memcmp( &bare, &loading, sizeof( bare ) ) != 0, 1 );
	}

	/* The whole load, state by state. loading_policy_isActive() answers qtrue
	 * for CONNECTING / CHALLENGING / CONNECTED / LOADING / PRIMED, and the
	 * caller passes that same predicate as isLoading — so the backdrop must be
	 * identical across the run rather than appearing only in the middle of it.
	 * The earlier caller derived isLoading from `state == CA_LOADING` alone,
	 * which left CONNECTING and PRIMED (measured: bgCmds=SKIPPED at state=3
	 * and state=7) showing the loading panel over bare attract. */
	{
		wuiBgLayerState_t seq[ 5 ];
		int i, ok = 1;

		for ( i = 0; i < 5; i++ )
			WUI_BgPresetEval( WUI_BG_PRESET_INVISIBLE, qfalse, qtrue, &seq[ i ] );
		for ( i = 1; i < 5; i++ )
			if ( memcmp( &seq[ 0 ], &seq[ i ], sizeof( seq[ 0 ] ) ) != 0 ) ok = 0;

		CheckInt( "backdrop is stable across the whole load", ok, 1 );
		CheckInt( "and it is the composed one",
			seq[ 0 ].darkVisible && seq[ 0 ].animatedVisible, 1 );
	}

	/* Leaving the load restores bare attract with nothing carried over — the
	 * loading backdrop must not outlive the load it belonged to. */
	{
		wuiBgLayerState_t coldBare, afterLoad;

		WUI_BgPresetEval( WUI_BG_PRESET_INVISIBLE, qfalse, qfalse, &coldBare );
		WUI_BgPresetEval( WUI_BG_PRESET_INVISIBLE, qfalse, qtrue,  &s );
		WUI_BgPresetEval( WUI_BG_PRESET_INVISIBLE, qfalse, qfalse, &afterLoad );

		CheckInt( "load leaves nothing behind",
			memcmp( &coldBare, &afterLoad, sizeof( coldBare ) ) == 0, 1 );
	}

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
