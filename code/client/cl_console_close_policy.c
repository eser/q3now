// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "cl_console_close_policy.h"

/*
====================
CL_ConsoleCloseDecision

Ranking, strongest claim first:

1. Not a session-entry edge  -> NONE. PRIMED / ACTIVE / CINEMATIC /
   DISCONNECTED are not session entries and must never disturb the console.

2. Attract reel active       -> SOFT. Attract advancing its own reel is not a
   user action and must not steal a console the user opened
   (Eser 2026-07-03: "console is a different layer, never interrupted by
   attract screen changes").

3. Local map load            -> SOFT. CL_MapLoading deliberately soft-closes so
   the user watches the log wall across the async spawn phases. That intent has
   to survive the CA_CONNECTING/CA_CONNECTED edge CL_MapLoading itself causes —
   otherwise the observer hard-closes over a soft-close issued microseconds
   earlier and the "log wall" idiom silently dies. Attract outranks this: an
   attract-driven map load is still not the user's doing, and both answers are
   SOFT anyway.

4. Otherwise                 -> HARD. A typed \connect / \demo, or a REMOTE
   server-pushed map change (CA_ACTIVE -> CA_LOADING with no local server and no
   command typed). Both replace what the user is looking at, so the console goes.
====================
*/
clConsoleCloseAction_t CL_ConsoleCloseDecision( const clConsoleCloseInput_t *in ) {
	if ( !in || !in->sessionEntryEdge ) {
		return CL_CONCLOSE_NONE;
	}
	if ( in->attractActive || in->localMapLoad ) {
		return CL_CONCLOSE_SOFT;
	}
	return CL_CONCLOSE_HARD;
}
