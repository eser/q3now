// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "cl_console_close_policy.h"
#include <stdio.h>

static int failures;
#define CHECK(expr) do { if (!(expr)) { fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#expr); failures++; } } while (0)

static clConsoleCloseAction_t decide( int sessionEntry, int attract, int localMap ) {
	clConsoleCloseInput_t in;
	in.sessionEntryEdge = sessionEntry;
	in.attractActive    = attract;
	in.localMapLoad     = localMap;
	return CL_ConsoleCloseDecision( &in );
}

int main( void ) {
	/* Non-session-entry edges (PRIMED / ACTIVE / CINEMATIC / DISCONNECTED)
	 * never touch the console, whatever else is true. */
	CHECK( decide( 0, 0, 0 ) == CL_CONCLOSE_NONE );
	CHECK( decide( 0, 1, 0 ) == CL_CONCLOSE_NONE );
	CHECK( decide( 0, 0, 1 ) == CL_CONCLOSE_NONE );
	CHECK( decide( 0, 1, 1 ) == CL_CONCLOSE_NONE );

	/* Flow (a) \connect and flow (b) \demo typed by the user: the session
	 * replaces the console, so it is dropped outright. */
	CHECK( decide( 1, 0, 0 ) == CL_CONCLOSE_HARD );

	/* Flow (b') attract reel advancing its own demo: not a user action, so the
	 * console layer survives with KEYCATCH_CONSOLE still held by the user. */
	CHECK( decide( 1, 1, 0 ) == CL_CONCLOSE_SOFT );

	/* Flow (c) local \map: CL_MapLoading's deliberate soft-close must survive
	 * the CA_CONNECTING/CA_CONNECTED edge CL_MapLoading itself triggers. This is
	 * the regression the observer introduced — a hard close here silently kills
	 * the "watch the log wall" idiom. */
	CHECK( decide( 1, 0, 1 ) == CL_CONCLOSE_SOFT );

	/* Attract and local map load together stay SOFT (attract outranks, same
	 * answer either way) — no path turns two user-preserving reasons into a
	 * hard close. */
	CHECK( decide( 1, 1, 1 ) == CL_CONCLOSE_SOFT );

	/* Flow (d) REMOTE server-pushed map change: no command typed, no local
	 * spawn in flight, no attract — must still hard-close. Encoded as the same
	 * input shape the observer builds for CA_ACTIVE -> CA_LOADING. */
	CHECK( decide( 1, 0, 0 ) == CL_CONCLOSE_HARD );

	/* Defensive: a NULL input decides nothing rather than closing. */
	CHECK( CL_ConsoleCloseDecision( NULL ) == CL_CONCLOSE_NONE );

	return failures ? 1 : 0;
}
