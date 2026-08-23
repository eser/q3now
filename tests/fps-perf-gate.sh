#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#
# fps-perf-gate.sh -- deterministic real-gameplay FPS + bottleneck benchmark.
#
# A repeatable regression gate for render performance. Launches a current,
# assembled engine against an ISOLATED home, loads a representative scene (map +
# dlights + several bots + effects) with a PINNED camera and PINNED frame time so
# the per-frame render work is identical every run, then collects — from the
# structured qconsole.jsonl log, parsed as JSON — the total FPS, the
# SCR_UpdateScreen ("end") time, the GPU per-pass breakdown, the fence/present
# split, stable swapchain identity, and the draw count. Vulkan CPU attribution
# is consumed only from the authoritative 200-attempt `vk perf v2` aggregate;
# the older rounded timing lines are informational and are never added together.
#
# It reuses the existing timing infrastructure (com_speeds / r_gpuSpeeds /
# r_vkDebugTiming, the qconsole.jsonl sink, the deterministic-camera recipe from
# visual-render-features.sh + the addbot/sv_seed recipe from nav-trace-gate.sh) —
# it does NOT introduce any new diagnostic cvar or a parallel harness.
#
# Determinism: cg.time is frozen (r_pinFrameTime 2.0), so the CLIENT render is
# pinned regardless of ongoing server bot AI — the same interpolated scene, the
# same draws/dlights in frustum, every frame. sv_seed pins the spawn RNG,
# fixedtime pins the sim delta, noclip removes the gravity settle so the camera
# origin is identical. com_maxfps 0 uncaps the frame rate so the measured number
# is the true render ceiling, not the 250 cap.
#
# Usage:
#   WIRED_CONTENT_ROOT=/path/to/content \
#     tests/fps-perf-gate.sh --engine build/wired.arm64 [--map arena1] [--bots 6] [--tag head]
#
# Re-run it on any build to compare; the before/after regression answer just
# needs the same script run against a second (pre-regression) build.

set -euo pipefail

# ── args ──────────────────────────────────────────────────────────────────
ENGINE=""
HEADLESS="${WIRED_BINARY_HEADLESS:-}"
MAP="arena1"
BOTS=6
TAG="head"
VIEWPOS="1052 1432 90 135"   # interior lit spot in arena1, action in frustum
SEED="12345"
HOLD_FRAMES=650             # fresh V2 epoch yields exactly 3x200 contained attempts (+50 tail frames)
TARGET_FPS="${FPS_TARGET_FPS:-250}"
RENDER_WIDTH=1280
RENDER_HEIGHT=720

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

PYTHON=()
for candidate in python3 python; do
    if command -v "$candidate" >/dev/null 2>&1 \
        && "$candidate" -c 'import sys; raise SystemExit(sys.version_info < (3, 8))' >/dev/null 2>&1; then
        PYTHON=("$candidate")
        break
    fi
done
if [ "${#PYTHON[@]}" -eq 0 ] && command -v py >/dev/null 2>&1 \
    && py -3 -c 'import sys; raise SystemExit(sys.version_info < (3, 8))' >/dev/null 2>&1; then
    PYTHON=(py -3)
fi
[ "${#PYTHON[@]}" -gt 0 ] || { echo "SKIP: Python 3.8+ is required"; exit 77; }
PYTHON_OS="$("${PYTHON[@]}" -c 'import os; print(os.name)')"

python_path() {
    case "$(uname -s):$PYTHON_OS" in
        MINGW*:nt|MSYS*:nt|CYGWIN*:nt) cygpath -w "$1" ;;
        *) printf '%s\n' "$1" ;;
    esac
}

if [ "${1:-}" = "--self-test" ] && [ "$#" -eq 1 ]; then
    "${PYTHON[@]}" "$(python_path "$SCRIPT_DIR/fps-perf-analyze.py")" --self-test
    "${PYTHON[@]}" "$(python_path "$SCRIPT_DIR/run-with-timeout.py")" --self-test
    exit 0
fi

shell_path() {
    case "$(uname -s)" in
        MINGW*|MSYS*|CYGWIN*) cygpath -u "$1" ;;
        *) printf '%s\n' "$1" ;;
    esac
}

while [ $# -gt 0 ]; do
    case "$1" in
        --engine|--headless|--map|--bots|--tag|--viewpos|--seed|--target-fps)
            [ "$#" -ge 2 ] || { echo "FAIL: $1 requires a value" >&2; exit 2; }
            option="$1"; value="$2"; shift 2
            case "$option" in
                --engine) ENGINE="$value" ;;
                --headless) HEADLESS="$value" ;;
                --map) MAP="$value" ;;
                --bots) BOTS="$value" ;;
                --tag) TAG="$value" ;;
                --viewpos) VIEWPOS="$value" ;;
                --seed) SEED="$value" ;;
                --target-fps) TARGET_FPS="$value" ;;
            esac
            ;;
        --help)
            echo "usage: $0 --engine PATH [--headless PATH] [--map NAME] [--bots N] [--tag TAG] [--viewpos 'X Y Z YAW'] [--target-fps N]"
            echo "       $0 --self-test"
            exit 0
            ;;
        --) shift; break ;;
        *) echo "unknown arg: $1" >&2; exit 2 ;;
    esac
done

[ -n "$ENGINE" ] || { echo "FAIL: --engine <wired-binary> required" >&2; exit 2; }
ENGINE="$(shell_path "$ENGINE")"
[ -x "$ENGINE" ] || { echo "FAIL: engine not found or not executable: $ENGINE" >&2; exit 2; }
case "$ENGINE" in /*) : ;; *) ENGINE="$PWD/$ENGINE" ;; esac
case "$BOTS" in ''|*[!0-9]*) echo "FAIL: --bots must be a non-negative integer" >&2; exit 2 ;; esac
case "$TAG" in ''|*[!A-Za-z0-9_.-]*) echo "FAIL: --tag must use only letters, numbers, dot, underscore, or dash" >&2; exit 2 ;; esac
case "$MAP" in ''|*[!A-Za-z0-9_.-]*) echo "FAIL: --map must use only letters, numbers, dot, underscore, or dash" >&2; exit 2 ;; esac
case "$SEED" in ''|*[!0-9-]*|-) echo "FAIL: --seed must be an integer" >&2; exit 2 ;; esac
if ! printf '%s\n' "$VIEWPOS" | grep -Eq '^-?[0-9]+([.][0-9]+)? +-?[0-9]+([.][0-9]+)? +-?[0-9]+([.][0-9]+)? +-?[0-9]+([.][0-9]+)?$'; then
    echo "FAIL: --viewpos must contain exactly four numeric values: x y z yaw" >&2
    exit 2
fi
if ! "${PYTHON[@]}" -c 'import math,sys; value=float(sys.argv[1]); raise SystemExit(not math.isfinite(value) or value <= 0)' "$TARGET_FPS"; then
    echo "FAIL: --target-fps must be a positive finite number" >&2
    exit 2
fi

ENGINE_DIR="$(cd "$(dirname "$ENGINE")" && pwd)"
ENGINE="$ENGINE_DIR/$(basename "$ENGINE")"
if [ -n "$HEADLESS" ]; then
    HEADLESS="$(shell_path "$HEADLESS")"
    case "$HEADLESS" in /*) : ;; *) HEADLESS="$PWD/$HEADLESS" ;; esac
else
    engine_base="$(basename "$ENGINE")"
    engine_suffix="${engine_base#wired}"
    for candidate in \
        "$ENGINE_DIR/wired-headless$engine_suffix" \
        "$ENGINE_DIR/wired-headless.arm64" \
        "$ENGINE_DIR/wired-headless.aarch64" \
        "$ENGINE_DIR/wired-headless.x86_64" \
        "$ENGINE_DIR/wired-headless.x64.exe" \
        "$ENGINE_DIR/wired-headless.exe" \
        "$ENGINE_DIR/wired-headless"; do
        if [ -x "$candidate" ]; then
            HEADLESS="$candidate"
            break
        fi
    done
fi
if [ -z "$HEADLESS" ]; then
    echo "SKIP: a sibling wired-headless binary is required to warm the isolated navmesh cache"
    exit 77
fi

# Resolve the current product pack separately from licensed base content. Both
# are staged into the isolated home: the engine does not mount launcher-home
# pax01 through fs_basepath alone, while the current pax21 must win over any old
# installed module. The user's content remains read-only.
PACK_ROOT=""
WIRED_PACK_ROOT_POSIX=""
if [ -n "${WIRED_PACK_ROOT:-}" ]; then
    WIRED_PACK_ROOT_POSIX="$(shell_path "$WIRED_PACK_ROOT")"
fi
for candidate in "$WIRED_PACK_ROOT_POSIX" "$ENGINE_DIR" "$ENGINE_DIR/../Resources" "$ENGINE_DIR/../../.."; do
    [ -n "$candidate" ] || continue
    if [ -f "$candidate/base/pax21.sw3z" ]; then
        PACK_ROOT="$(cd "$candidate" && pwd)"
        break
    fi
done
if [ -z "$PACK_ROOT" ]; then
    echo "SKIP: no current base/pax21.sw3z found beside bundle/build for $ENGINE"
    exit 77
fi

CONTENT_ROOT=""
WIRED_CONTENT_ROOT_POSIX=""
if [ -n "${WIRED_CONTENT_ROOT:-}" ]; then
    WIRED_CONTENT_ROOT_POSIX="$(shell_path "$WIRED_CONTENT_ROOT")"
fi
for candidate in "$WIRED_CONTENT_ROOT_POSIX" "$PACK_ROOT"; do
    [ -n "$candidate" ] || continue
    if [ -f "$candidate/base/pax01.sw3z" ] || [ -f "$candidate/base/pak0.pk3" ]; then
        CONTENT_ROOT="$(cd "$candidate" && pwd)"
        break
    fi
done
if [ -z "$CONTENT_ROOT" ]; then
    echo "SKIP: licensed base content missing; set WIRED_CONTENT_ROOT to a root containing base/pax01.sw3z or base/pak0.pk3"
    exit 77
fi

RUN_PARENT="$(mktemp -d -t wired-fpsgate-XXXXXX 2>/dev/null || mktemp -d)"
HOME_DIR="$RUN_PARENT/q3now-preview"

cleanup_runtime() {
    if [ "${KEEP_ARTIFACTS:-0}" = "1" ]; then
        echo "    kept isolated runtime: $RUN_PARENT"
    else
        rm -rf "$RUN_PARENT"
    fi
}
trap cleanup_runtime EXIT INT TERM

mkdir -p "$HOME_DIR/base"
if ! cp "$PACK_ROOT/base/pax21.sw3z" "$HOME_DIR/base/pax21.sw3z"; then
    echo "FAIL: could not stage current pax21.sw3z into isolated home"
    exit 1
fi
content_staged=0
for content_pack in "$CONTENT_ROOT"/base/pax0*.sw3z "$CONTENT_ROOT"/base/pak*.pk3; do
    [ -f "$content_pack" ] || continue
    if ! cp "$content_pack" "$HOME_DIR/base/"; then
        echo "FAIL: could not stage base content pack: $content_pack"
        exit 1
    fi
    content_staged=$((content_staged + 1))
done
if [ "$content_staged" -eq 0 ]; then
    echo "FAIL: content root resolved but no base content pack could be staged"
    exit 1
fi
case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*) HOME_NATIVE="$(cygpath -w "$HOME_DIR")" ;;
    *)                    HOME_NATIVE="$HOME_DIR" ;;
esac

OUTDIR_RAW="${FPS_OUTPUT_DIR:-$PROJECT_ROOT/build/test-results/fps-gate}"
OUTDIR="$(shell_path "$OUTDIR_RAW")"
mkdir -p "$OUTDIR"
RUN_OUTDIR="$OUTDIR/run-$TAG-$$"
mkdir -p "$RUN_OUTDIR"
STDOUT="$RUN_OUTDIR/stdout.log"
WARM_STDOUT="$RUN_OUTDIR/warm.log"
QSNAP="$RUN_OUTDIR/qconsole.jsonl"
QCONSOLE="$HOME_DIR/qconsole.jsonl"
CONFIG="$HOME_DIR/base/fps-perf-gate.cfg"
WARM_CONFIG="$HOME_DIR/base/fps-perf-warm.cfg"

echo "==> fps-perf-gate: $TAG  map=$MAP bots=$BOTS seed=$SEED viewpos=($VIEWPOS)"
echo "    current pack : $PACK_ROOT/base/pax21.sw3z"
echo "    base content : $CONTENT_ROOT/base ($content_staged pack(s), staged from read-only source)"
echo "    fs_homepath  : $HOME_NATIVE (isolated)"
echo "    evidence     : $RUN_OUTDIR"

# A fresh isolated home has no nav cache. Warm it with the sibling headless
# binary before the measured client run; otherwise addbot self-defers while the
# async bake runs and a nominal six-bot benchmark silently measures zero bots.
printf '%s\n' \
    'set sv_pure 0' \
    'set fixedtime 1' \
    'set com_maxfps 85' \
    "map $MAP" \
    'wait 450' \
    'quit' > "$WARM_CONFIG"
warm_port=$((27960 + ($$ % 1000)))
set +e
"${PYTHON[@]}" "$(python_path "$SCRIPT_DIR/run-with-timeout.py")" \
    --timeout 120 --kill-after 15 \
    --cwd "$(python_path "$(dirname "$HEADLESS")")" \
    --stdout "$(python_path "$WARM_STDOUT")" -- \
    "$(python_path "$HEADLESS")" \
    +set fs_homepath "$HOME_NATIVE" \
    +set com_noHardReboot 1 \
    +set net_port "$warm_port" \
    +exec fps-perf-warm.cfg
warm_rc=$?
set -e
if [ "$warm_rc" -ne 0 ] || ! grep -aFq "navmesh ready for '$MAP'" "$WARM_STDOUT"; then
    echo "FAIL: isolated navmesh warm pass did not complete (status=$warm_rc)"
    [ -s "$WARM_STDOUT" ] && { echo "----- warm stdout tail -----"; tail -30 "$WARM_STDOUT"; }
    exit 1
fi

# One cfg avoids the engine command-line's finite '+' token budget. The marker
# pair makes the parser consume only the pinned, steady measurement window.
{
    printf '%s\n' \
        'set sv_cheats 1' \
        'set sv_pure 0' \
        'set bot_enable 1' \
        'set g_gametype 0' \
        'set sv_maxclients 16' \
        'set g_warmup 0' \
        'set g_doWarmup 0' \
        "set sv_seed $SEED" \
        'set fixedtime 1' \
        'set com_maxfps 0' \
        'set com_maxfpsUnfocused 0' \
        'set com_maxfpsMinimized 0' \
        'set r_swapInterval 0' \
        'set com_automated 0' \
        'set log_severity DEBUG' \
        'set log_file_severity DEBUG' \
        'set sv_floodProtect 0' \
        "map $MAP" \
        'waitForMap' \
        'wait 100' \
        'cmd noclip' \
        'wait 20' \
        "cmd setviewpos $VIEWPOS" \
        'wait 300' \
        "cmd setviewpos $VIEWPOS 1000000" \
        'wait 100' \
        'echo FPS_GATE_CAMERA_CHECK' \
        'viewpos' \
        'wait 20' \
        'viewpos' \
        'wait 20' \
        'viewpos' \
        'log renderer.cmd info' \
        'log renderer.init info' \
        'gfxinfo' \
        'echo FPS_GATE_BOT_VIS_PRE_BEGIN' \
        'set r_speeds 2' \
        'wait 10' \
        'set r_speeds 0' \
        'echo FPS_GATE_BOT_VIS_PRE_END'
    i=0
    while [ "$i" -lt "$BOTS" ]; do
        printf '%s\n' 'addbot visor 5' 'wait 8'
        i=$((i+1))
    done
    printf '%s\n' \
        'wait 80' \
        'bot_teleport visor 989 1575 68 295' \
        'wait 20' \
        'echo FPS_GATE_BOT_VIS_POST_BEGIN' \
        'set r_speeds 2' \
        'wait 10' \
        'set r_speeds 0' \
        'echo FPS_GATE_BOT_VIS_POST_END' \
        'set r_dynamiclight 1' \
        'set r_dlightShadowTest 300' \
        'set r_dlightShadowTestN 1' \
        'set r_dlightShadows 1' \
        'set r_dlightShadowK 1' \
        'set r_dlightShadowCount 1' \
        'set r_dlightShadowProfile 1' \
        'set r_particles 1' \
        'set r_dither 0' \
        'set con_notifytime 0' \
        'set cg_debugevents 1' \
        'cmd give all' \
        'cmd god' \
        'weapon 7' \
        'wait 300' \
        'cmd noclip' \
        'wait 100' \
        'set r_speeds 7' \
        '+attack' \
        'wait 10' \
        '-attack' \
        'wait 20' \
        'cmd noclip' \
        'wait 100' \
        'set r_speeds 0' \
        "cmd setviewpos $VIEWPOS" \
        'wait 300' \
        "cmd setviewpos $VIEWPOS 1000000" \
        'set r_pinShaderTime 2.0' \
        'set r_pinFrameTime 2.0' \
        'wait 100' \
        'echo FPS_GATE_CAMERA_CHECK' \
        'viewpos' \
        'wait 20' \
        'viewpos' \
        'wait 20' \
        'viewpos' \
        'set r_particles 0' \
        'wait 5' \
        'screenshot fps_particles_off_a png silent' \
        'wait 5' \
        'screenshot fps_particles_off_b png silent' \
        'set r_particles 1' \
        'wait 5' \
        'screenshot fps_particles_on png silent' \
        'set cg_debugevents 0' \
        'set g_envWeather rain' \
        'echo FPS_GATE_SCENE_BEGIN' \
        'print r_particles' \
        'print g_envWeather' \
        'set r_speeds 1' \
        'wait 10' \
        'set r_speeds 4' \
        'wait 20' \
        'set r_speeds 0' \
        'wait 20' \
        'echo FPS_GATE_SCENE_END' \
        'set r_vkDebugTiming 1' \
        'log renderer.timing debug' \
        'set r_atmosphericGPU 1' \
        'wait 200' \
        'set r_dlightShadowCount 0' \
        'set r_dlightShadowProfile 0' \
        'set com_speeds 0' \
        'set r_gpuSpeeds 1' \
        'set r_vkDebugTiming 0' \
        'wait 1' \
        'set com_perfTrace 1' \
        'set r_vkDebugTiming 1' \
        'echo FPS_GATE_MEASURE_BEGIN' \
        "wait $HOLD_FRAMES" \
        'echo FPS_GATE_MEASURE_END' \
        'set com_perfTrace 0' \
        'set r_gpuSpeeds 0' \
        'set r_vkDebugTiming 0' \
        'wait 2' \
        'quit'
} > "$CONFIG"

# ── launch ──────────────────────────────────────────────────────────────────
#  sv_cheats 1  : CVAR_LATCH — active only AFTER map spawn. The CVAR_CHEAT timing
#    cvars (r_gpuSpeeds / r_vkDebugTiming / r_pinShaderTime / r_pinFrameTime) are
#    therefore set AFTER +map, not at startup (a startup set is "cheat protected").
#  com_maxfps 0 : uncap — measure the true render ceiling, not the 250 cap.
#  r_swapInterval 0 : vsync off (present-mode IMMEDIATE).
#  com_perfTrace 1 : aggregate-only full-population CPU/SCR micro-timing;
#    com_speeds stays 0 so threshold-dependent JSON writes cannot bias wall FPS.
#  r_gpuSpeeds 1 / r_vkDebugTiming 1 : 200-frame GPU per-pass plus authoritative
#    200-attempt Vulkan totals, swapchain identity, image histogram, and draws.
#    The forced 0/wait/1 edge immediately before MEASURE_BEGIN starts a fresh
#    Vulkan diagnostic epoch; all three emitted buckets are measurement-contained.
#  log_file_severity DEBUG + `log renderer.timing debug` : route the SEV_DEBUG
#    renderer.timing lines to the qconsole.jsonl file sink (after renderer init).
set +e
"${PYTHON[@]}" "$(python_path "$SCRIPT_DIR/run-with-timeout.py")" \
    --timeout 200 --kill-after 15 \
    --cwd "$(python_path "$ENGINE_DIR")" \
    --stdout "$(python_path "$STDOUT")" -- \
    "$(python_path "$ENGINE")" \
    +set fs_homepath "$HOME_NATIVE" \
    +set com_noHardReboot 1 \
    +set r_mode -1 \
    +set r_customwidth "$RENDER_WIDTH" \
    +set r_customheight "$RENDER_HEIGHT" \
    +set r_fullscreen 0 \
    +set r_fbo 1 \
    +set r_renderScale 1 \
    +set r_renderWidth "$RENDER_WIDTH" \
    +set r_renderHeight "$RENDER_HEIGHT" \
    +exec fps-perf-gate.cfg
engine_rc=$?
set -e

if [ "$engine_rc" -ne 0 ]; then
    echo "FAIL: engine exited with status $engine_rc"
    [ -s "$STDOUT" ] && { echo "----- stdout tail -----"; tail -30 "$STDOUT"; }
    exit 1
fi
if grep -aEq 'died on signal|Sys_Error|FATAL|^ERROR:' "$STDOUT"; then
    echo "FAIL: engine stdout contains a fatal/crash signature"
    grep -aE 'died on signal|Sys_Error|FATAL|^ERROR:' "$STDOUT" | tail -20
    exit 1
fi

# Snapshot qconsole so a later run doesn't clobber the parsed evidence.
if [ ! -s "$QCONSOLE" ]; then
    echo "FAIL: no qconsole.jsonl at $QCONSOLE" >&2
    exit 1
fi
cp "$QCONSOLE" "$QSNAP"

PNG2RAW="${PNG2RAW:-$PROJECT_ROOT/tools/png2raw/png2raw}"
if [ ! -x "$PNG2RAW" ] && [ -x "$PNG2RAW.exe" ]; then
    PNG2RAW="$PNG2RAW.exe"
fi
[ -x "$PNG2RAW" ] || { echo "FAIL: png2raw not found (build: make png2raw)" >&2; exit 1; }
SHOT_OFF_A="$HOME_DIR/base/screenshots/fps_particles_off_a.png"
SHOT_OFF_B="$HOME_DIR/base/screenshots/fps_particles_off_b.png"
SHOT_ON="$HOME_DIR/base/screenshots/fps_particles_on.png"
if [ ! -s "$SHOT_OFF_A" ] || [ ! -s "$SHOT_OFF_B" ] || [ ! -s "$SHOT_ON" ]; then
    echo "FAIL: particle control/A/B screenshots missing" >&2
    exit 1
fi
RAW_OFF_A="$RUN_OUTDIR/particles-off-a.rgb"
RAW_OFF_B="$RUN_OUTDIR/particles-off-b.rgb"
RAW_ON="$RUN_OUTDIR/particles-on.rgb"
"$PNG2RAW" "$SHOT_OFF_A" > "$RAW_OFF_A"
"$PNG2RAW" "$SHOT_OFF_B" > "$RAW_OFF_B"
"$PNG2RAW" "$SHOT_ON" > "$RAW_ON"
read -r PARTICLE_PIXELS PARTICLE_CHANGED PARTICLE_MAD PARTICLE_CONTROL_CHANGED PARTICLE_CONTROL_MAD <<EOF
$("${PYTHON[@]}" -c 'import pathlib,sys; a=pathlib.Path(sys.argv[1]).read_bytes(); b=pathlib.Path(sys.argv[2]).read_bytes(); c=pathlib.Path(sys.argv[3]).read_bytes(); assert len(a)==len(b)==len(c) and len(a)%3==0, (len(a),len(b),len(c)); metric=lambda x,y: (sum(any(d) for d in zip(*(iter([abs(i-j) for i,j in zip(x,y)]),)*3)), sum(abs(i-j) for i,j in zip(x,y))/len(x)); changed,mad=metric(b,c); control_changed,control_mad=metric(a,b); print(len(a)//3,changed,mad,control_changed,control_mad)' "$(python_path "$RAW_OFF_A")" "$(python_path "$RAW_OFF_B")" "$(python_path "$RAW_ON")")
EOF

# ── parse (JSON, not string-grep) ─────────────────────────────────────────
sha256_file() {
    if command -v sha256sum >/dev/null 2>&1; then
        sha256sum "$1" | awk '{print $1}'
    elif command -v shasum >/dev/null 2>&1; then
        shasum -a 256 "$1" | awk '{print $1}'
    else
        printf 'unavailable\n'
    fi
}
ENGINE_SHA256="$(sha256_file "$ENGINE")"
PAX21_SHA256="$(sha256_file "$PACK_ROOT/base/pax21.sw3z")"

set +e
"${PYTHON[@]}" "$(python_path "$SCRIPT_DIR/fps-perf-analyze.py")" \
    --input "$(python_path "$QSNAP")" \
    --tag "$TAG" \
    --map "$MAP" \
    --bots "$BOTS" \
    --target-fps "$TARGET_FPS" \
    --hold-frames "$HOLD_FRAMES" \
    --viewpos "$VIEWPOS" \
    --render-width "$RENDER_WIDTH" \
    --render-height "$RENDER_HEIGHT" \
    --draw-floor 30 \
    --particle-changed "$PARTICLE_CHANGED" \
    --particle-mad "$PARTICLE_MAD" \
    --particle-pixels "$PARTICLE_PIXELS" \
    --particle-control-changed "$PARTICLE_CONTROL_CHANGED" \
    --particle-control-mad "$PARTICLE_CONTROL_MAD" \
    --engine-sha256 "$ENGINE_SHA256" \
    --pax21-sha256 "$PAX21_SHA256"
parse_rc=$?
set -e

echo "==> stdout: $STDOUT   qconsole: $QSNAP"
exit "$parse_rc"
