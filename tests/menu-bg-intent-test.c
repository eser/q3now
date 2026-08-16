// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
// Contract for how a menu's background intent survives being re-activated.
//
// The scene behind the menus is not a property of the panel — it is recorded
// per menu-stack slot at push time (wui_menuBgIntent). SetActiveMenu(MAIN) used
// to push only when the stack was empty:
//
//     if ( menu == UIMENU_MAIN ) {
//         if ( wui_menuStackDepth == 0 ) {
//             WiredUI_PushMenu( "main", WUI_BG_INTENT_SCENE );
//         }
//     }
//
// Coming back from a map the stack is not empty, so the push was skipped: the
// menu still appeared through the stack fallback, but no SCENE intent was ever
// recorded for the top slot and the parallax did not draw. The main menu came
// up on black. Opening any submenu pushed properly and repaired it, which made
// the bug look specific to main.
//
// The guard was never needed — a redundant push collapses on the top-of-stack
// check AND refreshes the intent while collapsing. This models both halves so
// neither can regress: skipping the push must not lose the intent, and a
// redundant push must not duplicate the stack entry.

#include <stdio.h>
#include <string.h>

typedef enum { BG_INHERIT = 0, BG_SCENE, BG_DIM } bgIntent_t;

#define STACK_MAX 8

typedef struct {
	char       name[STACK_MAX][32];
	bgIntent_t intent[STACK_MAX];
	int        depth;
} menuStack_t;

/* Mirrors WiredUI_PushMenu's stack handling (cl_wired_ui.c:5880-5917). */
static void PushMenu( menuStack_t *s, const char *name, bgIntent_t intent )
{
	if ( s->depth > 0 && !strcmp( s->name[s->depth - 1], name ) ) {
		/* Already on top: collapse, but refresh the intent so a re-open with a
		 * different background takes effect. */
		s->intent[s->depth - 1] = intent;
		return;
	}
	if ( s->depth >= STACK_MAX ) return;
	strncpy( s->name[s->depth], name, sizeof( s->name[0] ) - 1 );
	s->name[s->depth][sizeof( s->name[0] ) - 1] = '\0';
	s->intent[s->depth] = intent;
	s->depth++;
}

/* Mirrors WiredUI_GetActiveBgIntent's stack-top rule (cl_wired_ui.c:2602-2606). */
static bgIntent_t ActiveIntent( const menuStack_t *s )
{
	if ( s->depth <= 0 ) return BG_INHERIT;
	return s->intent[s->depth - 1];
}

/* SetActiveMenu(UIMENU_MAIN) as it stands now: always pushes. */
static void SetActiveMain( menuStack_t *s )
{
	PushMenu( s, "main", BG_SCENE );
}

static int failures = 0;

static void Check( const char *name, int got, int want )
{
	if ( got == want ) {
		printf( "  ok   %-46s -> %d\n", name, got );
	} else {
		printf( "  FAIL %-46s -> %d, want %d\n", name, got, want );
		failures++;
	}
}

int main( void )
{
	menuStack_t s;

	/* Cold boot: nothing on the stack, main comes up with its scene. */
	memset( &s, 0, sizeof( s ) );
	SetActiveMain( &s );
	Check( "cold boot: main gets SCENE", ActiveIntent( &s ) == BG_SCENE, 1 );
	Check( "cold boot: one stack entry", s.depth, 1 );

	/* 🔴 The regression. Something is left on the stack — as after a map — and
	 * main is re-activated. The intent must still land. */
	memset( &s, 0, sizeof( s ) );
	PushMenu( &s, "ingame", BG_DIM );          /* residue from the session */
	SetActiveMain( &s );
	Check( "stack not empty: main still gets SCENE", ActiveIntent( &s ) == BG_SCENE, 1 );

	/* Re-activating main while it is already on top must not grow the stack. */
	memset( &s, 0, sizeof( s ) );
	SetActiveMain( &s );
	SetActiveMain( &s );
	SetActiveMain( &s );
	Check( "repeated activation does not stack up", s.depth, 1 );
	Check( "repeated activation keeps SCENE", ActiveIntent( &s ) == BG_SCENE, 1 );

	/* A redundant push with a DIFFERENT intent refreshes rather than ignores —
	 * this is what makes dropping the depth guard safe. */
	memset( &s, 0, sizeof( s ) );
	PushMenu( &s, "main", BG_INHERIT );
	PushMenu( &s, "main", BG_SCENE );
	Check( "redundant push refreshes the intent", ActiveIntent( &s ) == BG_SCENE, 1 );
	Check( "redundant push keeps depth at 1", s.depth, 1 );

	/* Submenus keep their own intent, and popping restores main's. */
	memset( &s, 0, sizeof( s ) );
	SetActiveMain( &s );
	PushMenu( &s, "options", BG_SCENE );
	Check( "submenu on top of main", s.depth, 2 );
	s.depth--;                                  /* pop */
	Check( "after pop, main's SCENE is intact", ActiveIntent( &s ) == BG_SCENE, 1 );

	/* An empty stack has no intent to report. */
	memset( &s, 0, sizeof( s ) );
	Check( "empty stack reports INHERIT", ActiveIntent( &s ) == BG_INHERIT, 1 );

	if ( failures ) {
		printf( "==> FAIL: %d case(s)\n", failures );
		return 1;
	}
	printf( "==> PASS: menu background intent contract\n" );
	return 0;
}
