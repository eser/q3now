#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#
# Compile and run the grab x mute focus/console state-machine test, then verify
# the engine source still contains the decisive expressions the test reproduces
# (so the test cannot silently drift).

set -u
cd "$(dirname "$0")/.." || exit 2
ROOT="$(pwd)"
fail=0

echo "== grab/mute: compile + run state-machine test =="
CC="${CC:-cc}"
"$CC" -O2 -Wall -Wextra -Werror "$ROOT/tests/grab_mute_state_test.c" -o "$ROOT/build/tmp/grab_mute_state_test" || {
	echo "COMPILE FAILED"; exit 2; }
"$ROOT/build/tmp/grab_mute_state_test" || fail=1

echo
echo "== grab/mute: source-consistency guard (no mirror drift) =="

guard() {
	# guard <file> <fixed-string> <label>
	if grep -qF "$2" "$ROOT/$1"; then
		echo "  PASS  $3"
	else
		echo "  FAIL  $3 -- '$2' not found in $1"
		fail=1
	fi
}

# SDL grab gate: consolidated, console term present, no fullscreen exemption.
# Third term is com_automated — a non-interactive run never grabs.
guard code/sdl/sdl_input.c 'if ( !gw_active || consoleOpen || ( com_automated && com_automated->integer ) ) {' \
	'sdl_input: consolidated grab gate (focus||console||com_automated)'
guard code/sdl/sdl_input.c 'Key_GetCatcher() & KEYCATCH_CONSOLE' \
	'sdl_input: grab gate reads console catcher'
# the old fullscreen/multimonitor exemption must be gone from the grab gate
if grep -E 'isFullscreen \|\| glw_state.monitorCount > 1' code/sdl/sdl_input.c >/dev/null; then
	echo "  FAIL  sdl_input: fullscreen-single-monitor console exemption still present"
	fail=1
else
	echo "  PASS  sdl_input: no fullscreen-single-monitor console exemption"
fi

# The native Win32 input backend has been retired; SDL is the sole grab gate.
if [ -e code/win32/win_input.c ]; then
	echo "  FAIL  win_input.c still present (Win32 input backend should be retired)"
	fail=1
else
	echo "  PASS  Win32 input backend retired (SDL is the sole grab gate)"
fi

# audio-mute: focus-only. The audio gate must NOT read the console catcher —
# audio follows focus, not console. (The grab gate reads it; the audio mute
# predicate must not.)
if grep -q 'KEYCATCH_CONSOLE' code/client/snd_dma.c; then
	echo "  FAIL  snd_dma: audio gate references KEYCATCH_CONSOLE (audio must be focus-only)"
	fail=1
else
	echo "  PASS  snd_dma: audio gate is focus-only (no console term)"
fi
guard code/client/snd_dma.c 'qboolean unfocused = !S_FocusUnmuted();' \
	'snd_dma: focus mute (S_FocusUnmuted) preserved'
guard code/client/snd_dma.c 'if ( s_autoMute_OverrideMute ) {' \
	'snd_dma: match-alert override preserved'

# comment-style guard: no roadmap/phase tags in the touched source.
if grep -nE '\bS4\b|Phase [0-9]|SDL3-migration|milestone' \
		code/sdl/sdl_input.c code/client/snd_dma.c \
		code/sdl/sdl_glimp.c >/dev/null; then
	echo "  FAIL  comment-style: roadmap/phase tag in touched source"
	fail=1
else
	echo "  PASS  comment-style: no roadmap/phase tags in touched source"
fi

echo
if [ "$fail" -eq 0 ]; then echo "grab/mute verification: ALL PASS"; else echo "grab/mute verification: FAILED"; fi
exit "$fail"
