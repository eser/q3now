// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
// Automated verification for the focus-driven audio mute + refocus flush.
//
// The engine mixer (snd_mix.c) no longer polls gw_active to decide whether the
// window is unfocused: the platform focus events drive S_Base_FocusChanged
// (snd_dma.c), which records the mute state the mixer reads through
// S_FocusUnmuted, and flushes the one-shot channels queued while unfocused so
// they don't burst on refocus.
//
// This test reproduces the exact focus state machine, the refocus flush
// predicate, and the mixer mute decision, then asserts:
//   T1  focused => mixer never mutes for the focus reason (byte-identity safety)
//   T2  flush clears one-shots started during the unfocused window
//   T3  flush preserves one-shots that were already playing before unfocus
//   T4  looping channels are never touched by the flush
//   T5  the new event-driven path diverges from the old gw_active poll (teeth)
//   T6  with no focus events delivered, S_FocusUnmuted falls back to gw_active
//       (the native Win32 backend's behaviour is unchanged)
//
// A source-consistency guard (run-focus-mute-test.sh) greps the real engine
// source for the decisive expressions so this test fails if the predicates are
// ever edited away from what is asserted here.

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

typedef enum { qfalse, qtrue } qboolean;

// --- channel model, mirroring the fields the flush touches (snd_local.h) ---
#define MAX_CHANNELS 96
typedef struct {
	void	*thesfx;	// NULL == free channel
	int		allocTime;	// s_soundtime when the sound was started
	int		looping;	// bookkeeping only; loop_channels is a separate array
} channel_t;

// --- engine state reproduced from snd_dma.c ---
static channel_t	s_channels[MAX_CHANNELS];
static channel_t	loop_channels[MAX_CHANNELS];	// rebuilt every frame in the engine
static qboolean		s_soundStarted = qtrue;
static int			s_soundtime;

static qboolean		s_focusUnmuted = qtrue;
static qboolean		s_focusEventsSeen = qfalse;
static int			s_focusMuteStart;

// gw_active stands in for the window-active flag the Win32 wndproc owns.
static qboolean		gw_active = qtrue;
static qboolean		gw_minimized = qfalse;

// ---- S_Base_FocusChanged: verbatim from snd_dma.c ----
static void S_Base_FocusChanged( qboolean focused ) {
	s_focusEventsSeen = qtrue;

	if ( !s_soundStarted ) {
		s_focusUnmuted = focused ? qtrue : qfalse;
		return;
	}

	if ( !focused ) {
		if ( s_focusUnmuted ) {
			s_focusMuteStart = s_soundtime;
		}
		s_focusUnmuted = qfalse;
		return;
	}

	if ( !s_focusUnmuted ) {
		channel_t *ch = s_channels;
		for ( int i = 0; i < MAX_CHANNELS; i++, ch++ ) {
			if ( ch->thesfx == NULL ) {
				continue;
			}
			// allocTime - s_focusMuteStart >= 0  ==  started at/after unfocus
			if ( ch->allocTime - s_focusMuteStart >= 0 ) {
				memset( ch, 0, sizeof( *ch ) );
			}
		}
	}
	s_focusUnmuted = qtrue;
}

// ---- S_FocusUnmuted: verbatim from snd_dma.c ----
static qboolean S_FocusUnmuted( void ) {
	if ( !s_focusEventsSeen ) {
		return gw_active;
	}
	return s_focusUnmuted;
}

// ---- mixer mute decision for the focus term: verbatim from snd_mix.c ----
// Returns qtrue when the mixer would mute purely because the window is
// unfocused (s_autoMute==0 legacy path, s_muteWhenUnfocused on).
static qboolean MixerWantsMute_Focus( void ) {
	qboolean wantMute = qfalse;
	qboolean unfocused = !S_FocusUnmuted();
	if ( unfocused && !gw_minimized /* && s_muteWhenUnfocused */ ) {
		wantMute = qtrue;
	}
	return wantMute;
}

// The OLD behaviour the change replaced: poll gw_active each mix.
static qboolean MixerWantsMute_Focus_OLD( void ) {
	qboolean wantMute = qfalse;
	if ( !gw_active && !gw_minimized ) {
		wantMute = qtrue;
	}
	return wantMute;
}

// ----------------------------------------------------------------------------

static int failures;
#define CHECK(cond, msg) do { \
		if ( cond ) { printf("  PASS  %s\n", msg); } \
		else { printf("  FAIL  %s\n", msg); failures++; } \
	} while (0)

static void reset_state( void ) {
	memset( s_channels, 0, sizeof( s_channels ) );
	memset( loop_channels, 0, sizeof( loop_channels ) );
	s_soundStarted = qtrue;
	s_soundtime = 0;
	s_focusUnmuted = qtrue;
	s_focusEventsSeen = qfalse;
	s_focusMuteStart = 0;
	gw_active = qtrue;
	gw_minimized = qfalse;
}

// allocate a one-shot channel in s_channels with the given allocTime
static channel_t *start_oneshot( int slot, int allocTime ) {
	s_channels[slot].thesfx = (void *)1;
	s_channels[slot].allocTime = allocTime;
	s_channels[slot].looping = 0;
	return &s_channels[slot];
}

int main( void ) {
	printf("focus-mute state-machine / flush verification\n");

	// === T1: focused => mixer never mutes for the focus reason ===
	reset_state();
	S_Base_FocusChanged( qtrue );		// initial focus event
	CHECK( S_FocusUnmuted() == qtrue, "T1a focused: S_FocusUnmuted() == qtrue" );
	CHECK( MixerWantsMute_Focus() == qfalse,
		"T1b focused: mixer does NOT mute (buffer stays dma.buffer)" );

	// === unfocus then refocus, with a mix of pre-mute and during-mute sounds ===
	reset_state();
	S_Base_FocusChanged( qtrue );		// focused
	s_soundtime = 1000;
	start_oneshot( 0, 200 );			// started long before unfocus
	start_oneshot( 1, 999 );			// started one sample before unfocus
	loop_channels[0].thesfx = (void *)1;	// an active looping sound
	loop_channels[0].allocTime = 200;

	S_Base_FocusChanged( qfalse );		// LOSE FOCUS at s_soundtime==1000
	CHECK( s_focusMuteStart == 1000, "unfocus records s_focusMuteStart == 1000" );
	CHECK( S_FocusUnmuted() == qfalse, "unfocused: S_FocusUnmuted() == qfalse" );
	CHECK( MixerWantsMute_Focus() == qtrue, "unfocused: mixer mutes" );

	// sounds queued WHILE unfocused (allocTime >= s_focusMuteStart)
	s_soundtime = 1500;
	start_oneshot( 2, 1500 );			// queued during the mute window
	s_soundtime = 1800;
	start_oneshot( 3, 1800 );			// queued during the mute window
	start_oneshot( 4, 1000 );			// queued exactly at unfocus boundary

	S_Base_FocusChanged( qtrue );		// REGAIN FOCUS -> flush

	// === T2: during-mute one-shots flushed ===
	CHECK( s_channels[2].thesfx == NULL, "T2a during-mute sound @1500 flushed" );
	CHECK( s_channels[3].thesfx == NULL, "T2b during-mute sound @1800 flushed" );
	CHECK( s_channels[4].thesfx == NULL, "T2c boundary sound @1000 (==start) flushed" );

	// === T3: pre-mute one-shots preserved ===
	CHECK( s_channels[0].thesfx != NULL, "T3a pre-mute sound @200 preserved" );
	CHECK( s_channels[1].thesfx != NULL, "T3b pre-mute sound @999 preserved" );

	// === T4: looping channels never touched ===
	CHECK( loop_channels[0].thesfx != NULL, "T4 looping channel preserved" );

	// === refocused: mixer unmutes ===
	CHECK( S_FocusUnmuted() == qtrue, "refocused: S_FocusUnmuted() == qtrue" );
	CHECK( MixerWantsMute_Focus() == qfalse, "refocused: mixer unmutes" );

	// === T5: TEETH — event-driven path diverges from the old gw_active poll ===
	// Construct a state the old and new code disagree on: window reports active
	// (gw_active==qtrue) but a FOCUS_LOST event has set the mute state. The old
	// poll would NOT mute (gw_active true); the new path mutes.
	reset_state();
	S_Base_FocusChanged( qtrue );
	S_Base_FocusChanged( qfalse );		// event says unfocused
	gw_active = qtrue;					// but the window-active poll says active
	CHECK( MixerWantsMute_Focus_OLD() == qfalse,
		"T5a OLD poll(gw_active) would NOT mute here" );
	CHECK( MixerWantsMute_Focus() == qtrue,
		"T5b NEW event-driven path DOES mute here (behaviours differ)" );

	// === T6: no focus events => fall back to gw_active (Win32 unchanged) ===
	reset_state();				// s_focusEventsSeen == qfalse
	gw_active = qtrue;
	CHECK( S_FocusUnmuted() == qtrue,  "T6a no events, gw_active=1 -> unmuted" );
	gw_active = qfalse;
	CHECK( S_FocusUnmuted() == qfalse, "T6b no events, gw_active=0 -> muted (poll)" );
	CHECK( MixerWantsMute_Focus() == MixerWantsMute_Focus_OLD(),
		"T6c no events: NEW path == OLD poll (Win32 byte-identical)" );

	printf("\n%s (%d failure%s)\n",
		failures ? "FAILED" : "ALL PASS",
		failures, failures == 1 ? "" : "s");
	return failures ? 1 : 0;
}
