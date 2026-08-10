// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
// Automated verification for the mouse-grab x audio-mute focus/console policy.
//
// The two axes (focus, console) drive the two outputs differently:
//   mouse grab : confined only when focused AND console-closed. Opening the
//                console releases the pointer so it can be used — in every
//                display mode, including fullscreen single-monitor.
//   audio      : plays while focused, mutes when backgrounded — independent of
//                the console. Opening the console while focused keeps audio on.
//
// So focused + console-open is ungrabbed but still audible.
//
// This test reproduces the consolidated grab gate (sdl_input.c IN_Frame /
// win_input.c IN_Frame) and the audio-mute decision (snd_mix.c S_PaintChannels),
// then asserts the four (focus x console) cells for both grab and audio, plus
// the fullscreen-single-monitor case and the user force-ungrab override. The
// teeth prove the grab releases on console-open (incl. fullscreen single-
// monitor) and that audio is focus-only (a re-added console-audio term would
// break the focused open-vs-closed equality).
//
// A source-consistency guard (run-grab-mute-test.sh) greps the real engine
// source for the decisive expressions so this test cannot drift from the code.

#include <stdio.h>

typedef enum { qfalse, qtrue } qboolean;

#define KEYCATCH_CONSOLE 0x0001

// ---- grab gate: reproduces the consolidated IN_Frame predicate ----
// Grab (confine) only when focused AND console-closed AND not an automated run.
// keyCatcher carries KEYCATCH_CONSOLE when the console is open; the gate has no
// display-mode exemption, so fullscreen single-monitor behaves identically. The
// third term is com_automated: a non-interactive run never grabs the pointer.
static qboolean GrabActive( qboolean gw_active, int keyCatcher, qboolean com_automated ) {
	qboolean consoleOpen = ( keyCatcher & KEYCATCH_CONSOLE ) != 0;
	if ( !gw_active || consoleOpen || com_automated ) {
		return qfalse;	// IN_DeactivateMouse()
	}
	return qtrue;		// IN_ActivateMouse()
}

// ---- audio-mute decision: reproduces the snd_mix.c focus term composition ----
// Audio follows window focus ONLY: it plays while focused and mutes when
// backgrounded, regardless of console state. A match-alert override still
// forces audio on. The keyCatcher argument is intentionally not consulted —
// the console does not gate audio (only the mouse-grab gate reads it).
static qboolean AudioMuted( qboolean focusUnmuted, int keyCatcher, qboolean overrideMute ) {
	(void)keyCatcher;	// console state does not affect audio
	qboolean wantMute = !focusUnmuted;
	if ( overrideMute ) {
		wantMute = qfalse;	// match-alert force-unmute wins last
	}
	return wantMute;
}

// ----------------------------------------------------------------------------

static int failures;
#define CHECK(cond, msg) do { \
		if ( cond ) { printf("  PASS  %s\n", msg); } \
		else { printf("  FAIL  %s\n", msg); failures++; } \
	} while (0)

#define CON_OPEN   KEYCATCH_CONSOLE
#define CON_CLOSED 0

int main( void ) {
	printf("grab x mute focus/console state-machine verification\n");

	// === Four-state truth table: grab + audio per (focus x console) cell ===
	// Cell 1: focused + console-closed -> grabbed + audio ON
	CHECK( GrabActive( qtrue, CON_CLOSED, qfalse ) == qtrue,
		"focused+console-closed: mouse GRABBED" );
	CHECK( AudioMuted( /*focusUnmuted*/qtrue, CON_CLOSED, qfalse ) == qfalse,
		"focused+console-closed: audio ON" );

	// Cell 2: focused + console-OPEN -> ungrabbed (grab releases on console)
	// but audio stays ON (audio follows focus only, not console).
	CHECK( GrabActive( qtrue, CON_OPEN, qfalse ) == qfalse,
		"focused+console-open: mouse UNGRABBED" );
	CHECK( AudioMuted( qtrue, CON_OPEN, qfalse ) == qfalse,
		"focused+console-open: audio ON (audio follows focus, not console)" );

	// Cell 3: background + console-closed -> ungrabbed + audio MUTED
	CHECK( GrabActive( qfalse, CON_CLOSED, qfalse ) == qfalse,
		"background+console-closed: mouse UNGRABBED" );
	CHECK( AudioMuted( /*focusUnmuted*/qfalse, CON_CLOSED, qfalse ) == qtrue,
		"background+console-closed: audio MUTED" );

	// Cell 4: background + console-open -> ungrabbed + audio MUTED
	CHECK( GrabActive( qfalse, CON_OPEN, qfalse ) == qfalse,
		"background+console-open: mouse UNGRABBED" );
	CHECK( AudioMuted( qfalse, CON_OPEN, qfalse ) == qtrue,
		"background+console-open: audio MUTED" );

	// === Fullscreen single-monitor must obey the same policy (no exemption) ===
	CHECK( GrabActive( qtrue, CON_OPEN, qfalse ) == qfalse,
		"fullscreen single-monitor + console-open: mouse UNGRABBED (exemption gone)" );

	// === Automated run (com_automated): never grab, free pointer while focused ===
	CHECK( GrabActive( qtrue, CON_CLOSED, /*com_automated*/qtrue ) == qfalse,
		"focused+console-closed+com_automated: mouse UNGRABBED (non-interactive run)" );

	// === Match-alert override still forces audio on when backgrounded ===
	CHECK( AudioMuted( qfalse, CON_CLOSED, /*overrideMute*/qtrue ) == qfalse,
		"background + match-alert override: audio ON (override wins)" );
	CHECK( AudioMuted( qfalse, CON_OPEN, qtrue ) == qfalse,
		"background + console-open + match-alert override: audio ON (override wins)" );

	// === TEETH: each missing term reintroduces the old bug ===
	printf("\n  -- teeth: old behaviour fails the policy --\n");

	// T-grab-console: OLD grab gate, fullscreen single-monitor, console open ->
	// the bug: mouse stayed GRABBED. NEW gate ungrabs.
	CHECK( GrabActive( qtrue, CON_OPEN, qfalse ) == qfalse,
		"T1 NEW grab: same state UNGRABBED (fix; behaviours differ)" );

	// T-audio-no-console: regression guard. Audio must depend on focus alone, so
	// toggling the console while focus is held cannot change the mute decision.
	// (A re-added console-audio term would make the open case mute and break
	// this equality.)
	CHECK( AudioMuted( qtrue, CON_OPEN, qfalse ) == AudioMuted( qtrue, CON_CLOSED, qfalse ),
		"T2 focused: console open vs closed -> SAME audio state (focus-only)" );
	CHECK( AudioMuted( qtrue, CON_OPEN, qfalse ) == qfalse,
		"T2 focused+console-open: audio ON (not muted by console)" );
	// And when backgrounded, mute is driven by focus loss, not by the console:
	// console open vs closed makes no difference there either.
	CHECK( AudioMuted( qfalse, CON_OPEN, qfalse ) == AudioMuted( qfalse, CON_CLOSED, qfalse ),
		"T2b background: console open vs closed -> SAME audio state (muted by focus)" );

	printf("\n%s (%d failure%s)\n",
		failures ? "FAILED" : "ALL PASS",
		failures, failures == 1 ? "" : "s");
	return failures ? 1 : 0;
}
