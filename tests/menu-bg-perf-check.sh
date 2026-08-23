#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026-present Wired Engine contributors
#
# menu-bg-perf-check.sh -- what the animated menu backdrop costs per frame.
#
# TASK-33 criterion #6 bundles a subjective look-ratify with an objective
# "within the FPS budget" claim. This measures the objective half.
#
# The comparison is between two menus that differ in exactly one thing a user
# can reach: the `backdrop` preset they declare. `preferences` takes the default
# ANIMATED preset -- dark base plus the composed v2 backdrop (sky, arena
# silhouette, embers, fog, noise, vignette, parallax), ~194 floating quads
# emitted in their own Clay layout pass before the panel walk. `main` declares
# `dim`, which leaves both backdrop layers off. Same process, same session, same
# window, back to back, so the machine state is shared and only the backdrop
# moves.
#
# Note what ANIMATED also does: it hides AND pauses the attract reel, while
# `dim` leaves attract visible and running. So the measured delta is the honest
# per-menu difference a user experiences (backdrop added, attract removed) and
# NOT the backdrop's cost in isolation. Isolating it would need both menus to
# hold attract in the same state, which no shipped preset does.
#
# An earlier version of this script had a third arm using
# `wui_layer_test_single menu exclusive` to force every background layer off.
# That arm was silently meaningless: the wui_layer_test_* dev commands are
# registered under `#ifdef _DEBUG` (cl_wired_compositor.c), so in the release
# build being measured the console reported `Unknown command` and the arm
# recorded whatever state was already up. It is removed rather than fixed --
# measuring a release build is the point, and a Debug build's frame times are
# not the shipped ones.
#
# Measurement reuses the existing timing infrastructure exactly as
# fps-perf-gate.sh does -- com_perfTrace for the full-population aggregate CPU
# authority, r_gpuSpeeds for the GPU per-pass breakdown, r_vkDebugTiming for
# the host-side pacing split, all read out of qconsole.jsonl as JSON. No new
# diagnostic cvar is introduced.
#
# Determinism: no map is loaded, so there is no bot AI and no world scene at
# all -- the only per-frame work is the UI. r_pinShaderTime / r_pinFrameTime
# pin the shader and entity clocks. com_maxfps 0 uncaps so the number is the
# render ceiling rather than the 250 cap; r_swapInterval 0 keeps present-mode
# IMMEDIATE so vsync does not quantise the result.
#
# Usage:
#   WIRED_CONTENT_ROOT=/path/to/content \
#     tests/menu-bg-perf-check.sh --engine build/<app>/Contents/MacOS/wired.arm64
#
#   tests/menu-bg-perf-check.sh --self-test     # analyzer mutation tests only

set -euo pipefail

ENGINE=""
HOLD_FRAMES=1250         # 6 contained 200-frame buckets + a controlled tail;
                         # the analyzer drops the first as warm-up, so five
                         # steady-state buckets per arm survive into the median
RENDER_WIDTH=1280        # deterministic harness default
RENDER_HEIGHT=720
KEEP="${KEEP_ARTIFACTS:-0}"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
ANALYZER="$SCRIPT_DIR/menu-bg-perf-analyze.py"

PYTHON=()
for candidate in python3 python; do
    if command -v "$candidate" >/dev/null 2>&1 \
        && "$candidate" -c 'import sys; raise SystemExit(sys.version_info < (3, 8))' >/dev/null 2>&1; then
        PYTHON=("$candidate")
        break
    fi
done
[ "${#PYTHON[@]}" -gt 0 ] || { echo "SKIP: Python 3.8+ is required"; exit 77; }

if [ "${1:-}" = "--self-test" ] && [ "$#" -eq 1 ]; then
    exec "${PYTHON[@]}" "$ANALYZER" --self-test
fi

while [ $# -gt 0 ]; do
    case "$1" in
        --engine)
            [ "$#" -ge 2 ] || { echo "FAIL: $1 requires a value" >&2; exit 2; }
            ENGINE="$2"; shift 2 ;;
        --hold)
            [ "$#" -ge 2 ] || { echo "FAIL: $1 requires a value" >&2; exit 2; }
            HOLD_FRAMES="$2"; shift 2 ;;
        --help)
            echo "usage: $0 --engine PATH [--hold FRAMES]"
            echo "       $0 --self-test"
            exit 0 ;;
        *) echo "unknown arg: $1" >&2; exit 2 ;;
    esac
done

[ -n "$ENGINE" ] || { echo "FAIL: --engine <wired-binary> required" >&2; exit 2; }
case "$ENGINE" in /*) : ;; *) ENGINE="$PWD/$ENGINE" ;; esac
[ -x "$ENGINE" ] || { echo "SKIP: engine not found or not executable: $ENGINE"; exit 77; }
ENGINE_DIR="$(cd "$(dirname "$ENGINE")" && pwd)"
ENGINE="$ENGINE_DIR/$(basename "$ENGINE")"
case "$HOLD_FRAMES" in ''|*[!0-9]*) echo "FAIL: --hold must be an integer" >&2; exit 2 ;; esac

# ── stage an isolated home (never touch the user's real one) ───────────────
# Same mount model as fps-perf-gate.sh / wiredui-menu-functional-check.sh: the
# current pax21 from beside the binary, plus whatever licensed base content is
# available. A menu-only run needs no map, but pax01 must still mount or the
# arena roster the menus bind to comes up empty.
PACK_ROOT=""
for candidate in "$ENGINE_DIR" "$ENGINE_DIR/../Resources" "$PROJECT_ROOT/build"; do
    if [ -f "$candidate/base/pax21.sw3z" ]; then
        PACK_ROOT="$(cd "$candidate" && pwd)"; break
    fi
done
[ -n "$PACK_ROOT" ] || { echo "SKIP: no current base/pax21.sw3z beside $ENGINE"; exit 77; }

CONTENT_ROOT=""
for candidate in "${WIRED_CONTENT_ROOT:-}" "$HOME/wired/q3now-preview" "$PACK_ROOT"; do
    [ -n "$candidate" ] || continue
    if [ -f "$candidate/base/pax01.sw3z" ] || [ -f "$candidate/base/pak0.pk3" ]; then
        CONTENT_ROOT="$(cd "$candidate" && pwd)"; break
    fi
done
[ -n "$CONTENT_ROOT" ] || { echo "SKIP: set WIRED_CONTENT_ROOT to a root with base/pax01.sw3z"; exit 77; }

RUN_ROOT="$(mktemp -d -t wired-menubg-XXXXXX)"
HOME_DIR="$RUN_ROOT/q3now-preview"
mkdir -p "$HOME_DIR/base"
cp "$PACK_ROOT/base/pax21.sw3z" "$HOME_DIR/base/" \
    || { echo "FAIL: could not stage pax21.sw3z"; exit 1; }
staged=0
for archive in "$CONTENT_ROOT"/base/pax0*.sw3z "$CONTENT_ROOT"/base/pak*.pk3; do
    [ -f "$archive" ] || continue
    cp "$archive" "$HOME_DIR/base/" || { echo "FAIL: could not stage $archive"; exit 1; }
    staged=$((staged + 1))
done
[ "$staged" -gt 0 ] || { echo "FAIL: no base archives staged from $CONTENT_ROOT/base"; exit 1; }

cleanup() {
    if [ "$KEEP" = "1" ]; then echo "    kept artifacts: $RUN_ROOT"; else rm -rf "$RUN_ROOT"; fi
}
trap cleanup EXIT INT TERM

CONFIG="$HOME_DIR/base/menubgperf.cfg"
JSONL="$HOME_DIR/qconsole.jsonl"
STDOUT_LOG="$RUN_ROOT/wired.stdout"

# ── the run ───────────────────────────────────────────────────────────────
# Two arms, then the first repeated, in ONE process so CPU/GPU/thermal state is
# shared:
#   BG_ON   -- preferences (ANIMATED preset): full composed backdrop
#   BG_OFF  -- main (dim preset): both backdrop layers off, attract running
#   BG_ON2  -- back to preferences
# The repeat is the drift control. These arms run in sequence, so a machine that
# gets busier (or a GPU that clocks down) partway through would show up as an
# arm difference that is really a time difference. BG_ON2 vs BG_ON measures that
# drift directly: when the two disagree by more than the BG_ON/BG_OFF delta, the
# run is telling you about the machine rather than the backdrop.
#
# Each arm gets its own fresh measurement epoch: com_perfTrace and
# r_vkDebugTiming are toggled 0 -> 1 at the arm boundary, which resets the
# renderer bucket and the CPU accumulator, so no arm inherits the previous
# arm's samples.
arm() {
    printf '%s\n' \
        "echo MENUBG_ARM_BEGIN_$1" \
        'set com_perfTrace 0' \
        'set r_gpuSpeeds 0' \
        'set r_vkDebugTiming 0' \
        'wait 20' \
        'set com_perfTrace 1' \
        'set r_gpuSpeeds 1' \
        'set r_vkDebugTiming 1' \
        "wait $HOLD_FRAMES" \
        'set com_perfTrace 0' \
        'set r_gpuSpeeds 0' \
        'set r_vkDebugTiming 0' \
        "echo MENUBG_ARM_END_$1" \
        'wait 10'
}

{
    printf '%s\n' \
        'set sv_cheats 1' \
        'set sv_pure 0' \
        'set com_maxfps 0' \
        'set com_maxfpsUnfocused 0' \
        'set com_maxfpsMinimized 0' \
        'set r_swapInterval 0' \
        'set com_speeds 0' \
        'set log_severity DEBUG' \
        'set log_file_severity DEBUG' \
        'set r_pinShaderTime 1.0' \
        'set r_pinFrameTime 1.0' \
        'set cl_allowDownload 0' \
        'set r_inGameVideo 0' \
        'wait 120' \
        'log renderer.timing debug' \
        'log renderer.init info' \
        'gfxinfo' \
        'wait 20'

    # ---- arm 1: backdrop ON (preferences, ANIMATED preset) ----
    printf '%s\n' 'wui_push main' 'wait 30' 'wui_push preferences' 'wait 90'
    arm BG_ON

    # ---- arm 2: backdrop OFF (main, dim preset) ----
    printf '%s\n' 'wui_menu_nav back' 'wait 90'
    arm BG_OFF

    # ---- arm 3: backdrop ON again (drift control) ----
    printf '%s\n' 'wui_push preferences' 'wait 90'
    arm BG_ON2

    printf '%s\n' 'wait 10' 'quit'
} > "$CONFIG"

echo "==> running (isolated home: $HOME_DIR)"
rm -f "$JSONL"
set +e
"${PYTHON[@]}" "$SCRIPT_DIR/run-with-timeout.py" \
    --timeout 300 --kill-after 15 \
    --cwd "$RUN_ROOT" \
    --stdout "$STDOUT_LOG" -- \
    "$ENGINE" \
    +set fs_basepath "$CONTENT_ROOT" \
    +set fs_homepath "$HOME_DIR" \
    +set com_automated 1 \
    +set com_noHardReboot 1 \
    +set s_initsound 0 \
    +set r_mode -1 \
    +set r_customwidth "$RENDER_WIDTH" \
    +set r_customheight "$RENDER_HEIGHT" \
    +set r_fullscreen 0 \
    +set r_fbo 1 \
    +set r_renderScale 1 \
    +set r_renderWidth "$RENDER_WIDTH" \
    +set r_renderHeight "$RENDER_HEIGHT" \
    +exec menubgperf.cfg
RC=$?
set -e
echo "    engine rc=$RC"

[ -f "$JSONL" ] || { echo "FAIL: no qconsole.jsonl at $JSONL"; exit 1; }

"${PYTHON[@]}" "$ANALYZER" --jsonl "$JSONL" --hold "$HOLD_FRAMES"
