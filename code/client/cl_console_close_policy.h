// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef CL_CONSOLE_CLOSE_POLICY_H
#define CL_CONSOLE_CLOSE_POLICY_H

/*
The console-close decision for a client connection-state edge, isolated from
the client so it is testable without the engine. The console is a separate top
layer (WUI_LAYER_CONSOLE) the user opens and closes deliberately; only two
things may take it away: the user pressing `~`, or entering a session because
the user asked for one. Everything else at most collapses it visually while
leaving KEYCATCH_CONSOLE with the user.

Three outcomes:
  CL_CONCLOSE_NONE  — not a session-entry edge; do not touch the console.
  CL_CONCLOSE_SOFT  — collapse visually, PRESERVE KEYCATCH_CONSOLE. The user
                      keeps ownership: an attract reel advancing itself, or a
                      local map load whose "watch the log wall" idiom
                      (CL_MapLoading) deliberately keeps the console attached.
  CL_CONCLOSE_HARD  — drop the console entirely (Con_Close). The user typed
                      \connect / \demo, or a remote server pushed us into a new
                      map; in both cases the session replaces the console.
*/
typedef enum {
	CL_CONCLOSE_NONE = 0,
	CL_CONCLOSE_SOFT,
	CL_CONCLOSE_HARD
} clConsoleCloseAction_t;

/* The transition facts the decision needs. Deliberately not the client's
 * connstate_t: this seam must compile with no engine headers. */
typedef struct {
	int			sessionEntryEdge;	/* newState is CA_CONNECTING/CONNECTED/LOADING */
	int			attractActive;		/* an attract reel is driving the transition */
	int			localMapLoad;		/* a local \map load owns this transition */
} clConsoleCloseInput_t;

clConsoleCloseAction_t CL_ConsoleCloseDecision( const clConsoleCloseInput_t *in );

#endif /* CL_CONSOLE_CLOSE_POLICY_H */
