#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#
# Compile and run the focus-mute state-machine test, then verify the engine
# source still contains the decisive expressions the test reproduces, so the
# test cannot silently drift from the real code.

set -u
cd "$(dirname "$0")/.." || exit 2
ROOT="$(pwd)"
fail=0

echo "== focus-mute: compile + run state-machine test =="
CC="${CC:-cc}"
"$CC" -O2 -Wall -Wextra -Werror "$ROOT/tests/focus_mute_test.c" -o "$ROOT/build/tmp/focus_mute_test" || {
	echo "COMPILE FAILED"; exit 2; }
"$ROOT/build/tmp/focus_mute_test" || fail=1

echo
echo "== focus-mute: source-consistency guard (no mirror drift) =="

guard() {
	# guard <file> <fixed-string> <label>
	if grep -qF "$2" "$ROOT/$1"; then
		echo "  PASS  $3"
	else
		echo "  FAIL  $3 -- '$2' not found in $1"
		fail=1
	fi
}

# snd_dma.c: focus state machine + flush predicate + fallback
guard code/client/snd_dma.c 'void S_Base_FocusChanged( qboolean focused ) {'      'snd_dma: S_Base_FocusChanged present'
guard code/client/snd_dma.c 's_focusMuteStart = s_soundtime;'                      'snd_dma: records mute-start on unfocus'
guard code/client/snd_dma.c 'if ( ch->allocTime - s_focusMuteStart >= 0 ) {'       'snd_dma: flush predicate (allocTime - start >= 0)'
guard code/client/snd_dma.c 'channel_t *ch = s_channels;'                          'snd_dma: flush targets s_channels (one-shots) only'
guard code/client/snd_dma.c 'if ( !s_focusEventsSeen ) {'                          'snd_dma: fallback guard present'
guard code/client/snd_dma.c 'return gw_active;'                                    'snd_dma: fallback returns polled gw_active'

# flush must NOT touch loop_channels (preserve looping/ambient).
if grep -n 'loop_channels' code/client/snd_dma.c | grep -qi 'FocusChanged'; then
	echo "  FAIL  snd_dma: flush must not reference loop_channels"
	fail=1
else
	echo "  PASS  snd_dma: flush leaves loop_channels untouched"
fi

# snd_dma.c: auto-mute predicate reads S_FocusUnmuted, not a fresh gw_active poll
guard code/client/snd_dma.c 'qboolean unfocused = !S_FocusUnmuted();'              'snd_dma: focus term reads S_FocusUnmuted()'
guard code/client/snd_dma.c '(am & 1) && unfocused && !gw_minimized'              'snd_dma: am&1 always-mute path intact'
guard code/client/snd_dma.c 'unfocused && !gw_minimized && s_muteWhenUnfocused->integer' 'snd_dma: s_muteWhenUnfocused semantics intact'
guard code/client/snd_dma.c 'if ( s_autoMute_OverrideMute ) {'                     'snd_dma: match-alert override intact'

# the auto-mute focus term must no longer poll gw_active directly
if grep -E '!gw_active' code/client/snd_dma.c >/dev/null; then
	echo "  FAIL  snd_dma: still polls !gw_active for the focus term"
	fail=1
else
	echo "  PASS  snd_dma: no direct !gw_active poll for the focus term"
fi

# sdl_input.c: focus events drive S_FocusChanged
guard code/sdl/sdl_input.c 'S_FocusChanged( qfalse )'                              'sdl_input: FOCUS_LOST -> S_FocusChanged(qfalse)'
guard code/sdl/sdl_input.c 'S_FocusChanged( qtrue )'                               'sdl_input: FOCUS_GAINED -> S_FocusChanged(qtrue)'

# comment-style guard: no roadmap/phase/milestone tags in the touched code
if grep -nE 'S3|Phase [0-9]|SDL3-migration|milestone' \
		code/client/snd_dma.c code/client/snd_miniaudio.c code/client/snd_main.c \
		code/client/snd_local.h code/client/snd_public.h >/dev/null; then
	echo "  FAIL  comment-style: roadmap/phase tag found in touched snd source"
	fail=1
else
	echo "  PASS  comment-style: no roadmap/phase tags in touched snd source"
fi

echo
if [ "$fail" -eq 0 ]; then echo "focus-mute verification: ALL PASS"; else echo "focus-mute verification: FAILED"; fi
exit "$fail"
