// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
// Contract for the attract-state first-input promotion (cl_keys.c). Over
// attract, the first keypress should raise the main menu. Two conditions in
// that test used to be wrong in ways that only appear after a round of play,
// and both produced the same symptom — keys stop working, silently:
//
//   1. It excluded KEYCATCH_CONSOLE. A SOFT console close collapses the console
//      visually but deliberately leaves the catcher with the user
//      (cl_console_close_policy.h). So after opening and closing the console
//      once over attract, no keypress ever raised the menu again.
//
//   2. It required !com_sv_running. Disconnect is client-only and leaves the
//      local server up on purpose (cl_main.c:2119), so after loading one map
//      the clause was permanently false.
//
// The predicate is pure, so this runs with no engine, no window and no packs.

#include <stdio.h>
#include <string.h>

/* Mirrors the live conditions in cl_keys.c CL_KeyDownEvent. Keep in sync. */
typedef struct {
	int   isEscape;         /* key == K_ESCAPE                      */
	int   disconnected;     /* state == CA_DISCONNECTED             */
	int   catcherUI;        /* KEYCATCH_UI                          */
	int   catcherConsole;   /* KEYCATCH_CONSOLE (may outlive a soft close) */
	int   catcherCgame;     /* KEYCATCH_CGAME                       */
	int   catcherMessage;   /* KEYCATCH_MESSAGE                     */
	float consoleFrac;      /* Con_GetDisplayFrac(): >0 == on screen */
	int   uiHealthy;        /* WiredUI_IsHealthy()                  */
	int   svRunning;        /* com_sv_running                       */
} promotionInput_t;

static int ShouldPromote( const promotionInput_t *in )
{
	/* isEscape is deliberately NOT consulted: ESC promotes like any other
	 * key. It already closes the menu back to attract, so excluding it made
	 * it the single key that could not perform the opposite move. The pair
	 * is meant to read as a toggle. */
	if ( !in->disconnected )            return 0;
	if ( in->catcherUI )                return 0;
	if ( in->catcherCgame )             return 0;
	if ( in->catcherMessage )           return 0;
	if ( in->consoleFrac > 0.0f )       return 0;   /* on screen, not merely owned */
	/* Health, not the old UI_VM_ACTIVE — that macro is a literal 1 and
	 * guarded nothing. It matters now that ESC is admitted: this block
	 * returns before the ESC handler runs, so promoting a dead UI would
	 * shadow the recovery branch that revives WiredUI from the fullscreen
	 * fallback console. */
	if ( !in->uiHealthy )               return 0;
	return 1;                                        /* svRunning deliberately unused */
}

static int failures = 0;

static void Check( const char *name, int got, int want )
{
	if ( got == want ) {
		printf( "  ok   %-44s -> %d\n", name, got );
	} else {
		printf( "  FAIL %-44s -> %d, want %d\n", name, got, want );
		failures++;
	}
}

int main( void )
{
	promotionInput_t in;

	/* Baseline: sitting on attract, nothing owns input. */
	memset( &in, 0, sizeof( in ) );
	in.disconnected = 1;
	in.uiHealthy    = 1;
	Check( "plain attract keypress promotes", ShouldPromote( &in ), 1 );

	/* 🔴 The console regression. Console was opened and closed; the soft close
	 * left KEYCATCH_CONSOLE with the user but took it off the screen. */
	in.catcherConsole = 1;
	in.consoleFrac    = 0.0f;
	Check( "after soft console close, still promotes", ShouldPromote( &in ), 1 );

	/* While it IS on screen the console owns the key, catcher or not. */
	in.consoleFrac = 1.0f;
	Check( "console on screen does not promote", ShouldPromote( &in ), 0 );
	in.consoleFrac = 0.5f;
	Check( "console mid-slide does not promote", ShouldPromote( &in ), 0 );
	in.consoleFrac    = 0.0f;
	in.catcherConsole = 0;

	/* 🔴 The local-server regression. One map was loaded, disconnect left the
	 * server up, and the old clause turned this off for good. */
	in.svRunning = 1;
	Check( "server still up after map, promotes", ShouldPromote( &in ), 1 );

	/* Both regressions at once — console used AND a map played. */
	in.catcherConsole = 1;
	Check( "console used and map played, promotes", ShouldPromote( &in ), 1 );
	in.catcherConsole = 0;
	in.svRunning      = 0;

	/* 🔴 ESC is a toggle, not a dead key. Pressing it over attract raises the
	 * menu; pressing it in the menu drops back to attract. It used to be the
	 * one key excluded here, which left the return leg working and the
	 * outbound leg silently doing nothing. */
	in.isEscape = 1;
	Check( "escape promotes like any other key", ShouldPromote( &in ), 1 );

	/* The other half of the toggle: with the menu up, ESC must NOT promote —
	 * it belongs to the menu's own close path. catcherUI already covers this,
	 * but pin it with ESC specifically so a future ESC special-case cannot
	 * reintroduce a promote-on-top-of-the-menu. */
	in.catcherUI = 1;
	Check( "escape with menu up does not promote", ShouldPromote( &in ), 0 );
	in.catcherUI = 0;
	in.isEscape  = 0;

	in.catcherUI = 1;
	Check( "menu already up does not promote", ShouldPromote( &in ), 0 );
	in.catcherUI = 0;

	in.catcherCgame = 1;
	Check( "cgame owns input, no promote", ShouldPromote( &in ), 0 );
	in.catcherCgame = 0;

	in.catcherMessage = 1;
	Check( "chat open does not promote", ShouldPromote( &in ), 0 );
	in.catcherMessage = 0;

	in.disconnected = 0;
	Check( "in a session does not promote", ShouldPromote( &in ), 0 );
	in.disconnected = 1;

	/* A dead UI has no menu to promote to. This matters more than it reads:
	 * because this block returns before the ESC handler, promoting here with
	 * a dead UI would swallow the ESC that is supposed to revive WiredUI from
	 * the fullscreen fallback console. */
	in.uiHealthy = 0;
	Check( "dead UI does not promote", ShouldPromote( &in ), 0 );
	in.isEscape  = 1;
	Check( "dead UI leaves escape for recovery", ShouldPromote( &in ), 0 );
	in.isEscape  = 0;
	in.uiHealthy = 1;

	if ( failures ) {
		printf( "==> FAIL: %d case(s)\n", failures );
		return 1;
	}
	printf( "==> PASS: attract input promotion contract\n" );
	return 0;
}
