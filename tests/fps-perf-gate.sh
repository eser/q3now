#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#
# fps-perf-gate.sh -- pinned representative-scene FPS + bottleneck diagnostic.
# FPS_PERF_GAMEPLAY_SECONDS=32 selects the native-resolution moving/combat lane.
# One run is not the full N>=3 arena1/7/17 TASK-86 acceptance matrix.
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
HOLD_FRAMES="${FPS_PERF_HOLD_FRAMES:-600}" # exact 3x200 epochs; no unmeasured tail
TARGET_FPS="${FPS_TARGET_FPS:-250}"
RENDER_WIDTH=1280
RENDER_HEIGHT=720
GAMEPLAY_SECONDS="${FPS_PERF_GAMEPLAY_SECONDS:-0}" # opt-in wall-clock movement/combat lane
BACKEND="${FPS_PERF_BACKEND:-vulkan}"
RENDER_SCALE=1
GAMEPLAY_ARGS=()

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

sha256_file() {
    if command -v sha256sum >/dev/null 2>&1; then
        sha256sum "$1" | awk '{print $1}'
    elif command -v shasum >/dev/null 2>&1; then
        shasum -a 256 "$1" | awk '{print $1}'
    else
        printf 'unavailable\n'
    fi
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
case "$HOLD_FRAMES" in ''|*[!0-9]*) echo "FAIL: FPS_PERF_HOLD_FRAMES must be a positive integer" >&2; exit 2 ;; esac
case "$GAMEPLAY_SECONDS" in ''|*[!0-9]*) echo "FAIL: FPS_PERF_GAMEPLAY_SECONDS must be an integer" >&2; exit 2 ;; esac
if [ "$GAMEPLAY_SECONDS" -ne 0 ]; then
    if [ "$GAMEPLAY_SECONDS" -lt 32 ] || [ "$GAMEPLAY_SECONDS" -gt 120 ]; then
        echo "FAIL: gameplay duration must be 32..120 seconds" >&2; exit 2
    fi
    case "$BACKEND" in vulkan|metal) : ;; *) echo "FAIL: gameplay backend must be vulkan or metal" >&2; exit 2 ;; esac
    RENDER_SCALE=0 # native drawable-sized scene, never internal720p on Retina
    case "$MAP" in
        arena1) VIEWPOS='1052 1432 90 135' ;;
        arena7) VIEWPOS='-96 -1040 176 45' ;;
        arena17) VIEWPOS='488 1096 378 -90' ;;
        *) echo "FAIL: gameplay route not defined for $MAP" >&2; exit 2 ;;
    esac
    GAMEPLAY_ARGS=(+set cl_renderer "$BACKEND" +set r_hdr 1 +set r_hdrDisplay 0
        +set r_hdrAutoExposure 0 +set r_ext_supersample 0)
fi
if [ "$HOLD_FRAMES" -lt 200 ]; then
    echo "FAIL: FPS_PERF_HOLD_FRAMES must cover at least one 200-frame diagnostic epoch" >&2
    exit 2
fi
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
RUN_ENGINE="$ENGINE"

# A macOS product binary normally lives inside an .app bundle, but invoking it
# directly from Contents/MacOS is not equivalent to the flat test-runtime shape
# used by the native renderer harnesses. Normalize the measured client to that
# proven shape: executable at runtime/wired, renderer/MoltenVK under
# runtime/Contents/MacOS, and @executable_path dependencies beside the binary.
# Source artifacts remain read-only and continue to identify the build receipt.
if [ "$(uname -s)" = "Darwin" ]; then
    RUN_DIR="$RUN_PARENT/runtime"
    mkdir -p "$RUN_DIR/Contents/MacOS"
    cp "$ENGINE" "$RUN_DIR/wired"
    chmod +x "$RUN_DIR/wired"
    RUN_ENGINE="$RUN_DIR/wired"

    dylib_count=0
    for dylib in "$ENGINE_DIR"/*.dylib; do
        [ -f "$dylib" ] || continue
		# MoltenVK has a dedicated, overridable source-selection path below.
		# Copying it here first preserves the bundle's read-only mode and makes
		# the second copy fail before the benchmark can launch.
		[ "$(basename "$dylib")" = "libMoltenVK.dylib" ] && continue
        cp "$dylib" "$RUN_DIR/Contents/MacOS/"
        dylib_count=$((dylib_count + 1))
    done
    if [ "$dylib_count" -eq 0 ]; then
        echo "FAIL: no renderer/runtime dylibs found beside macOS engine: $ENGINE_DIR"
        exit 1
    fi

    molten_source="${WIRED_MOLTENVK:-}"
    if [ ! -f "$molten_source" ]; then
        molten_source=""
        for candidate in \
            "$ENGINE_DIR/libMoltenVK.dylib" \
            /opt/homebrew/opt/molten-vk/lib/libMoltenVK.dylib \
            /usr/local/lib/libMoltenVK.dylib; do
            if [ -f "$candidate" ]; then
                molten_source="$candidate"
                break
            fi
        done
    fi
    if [ -z "$molten_source" ]; then
        echo "FAIL: MoltenVK unavailable; set WIRED_MOLTENVK to libMoltenVK.dylib"
        exit 1
    fi
    cp "$molten_source" "$RUN_DIR/Contents/MacOS/libMoltenVK.dylib"

    if command -v otool >/dev/null 2>&1; then
        otool -L "$ENGINE" 2>/dev/null \
            | sed -n 's|^[[:space:]]*@executable_path/\([^ ]*\).*|\1|p' \
            | while read -r dependency; do
                [ -n "$dependency" ] || continue
                [ -f "$ENGINE_DIR/$dependency" ] && cp "$ENGINE_DIR/$dependency" "$RUN_DIR/"
            done
    fi
fi

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

# Match the normal `make run-game` product policy: native game/cgame modules
# are the default path, while VM=1 is an explicit WASM/AOT launch mode. The
# isolated benchmark must stage those loose modules itself because pax21 owns
# portable VM artifacts, not platform dylibs/DLLs.
native_module_count=0
for native_module in \
    "$PACK_ROOT"/base/gamecl*.dylib "$PACK_ROOT"/base/gamesv*.dylib \
    "$PACK_ROOT"/base/gamecl*.so "$PACK_ROOT"/base/gamesv*.so \
    "$PACK_ROOT"/base/gamecl*.dll "$PACK_ROOT"/base/gamesv*.dll; do
    [ -f "$native_module" ] || continue
    cp "$native_module" "$HOME_DIR/base/"
    native_module_count=$((native_module_count + 1))
done
if [ "$native_module_count" -lt 2 ]; then
    echo "FAIL: default native game/cgame modules missing under $PACK_ROOT/base"
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

# Release consumes offline products, just like the normal installed game.
# Copy the existing corpus into this isolated home; never cook or modify the
# player's cache during a benchmark.
MATERIAL_COOK_DIR="${FPS_PERF_MATERIAL_COOK_DIR:-$CONTENT_ROOT/base/materials/cooked}"
[ -d "$MATERIAL_COOK_DIR" ] || {
    echo "FAIL: existing cooked material corpus missing: $MATERIAL_COOK_DIR"
    exit 1
}
mkdir -p "$HOME_DIR/base/materials/cooked"
for extension in wrmat wrmatpkg wrmatref; do
    product_count=0
    for product in "$MATERIAL_COOK_DIR"/*."$extension"; do
        [ -f "$product" ] || continue
        cp "$product" "$HOME_DIR/base/materials/cooked/"
        product_count=$((product_count + 1))
    done
    [ "$product_count" -gt 0 ] || {
        echo "FAIL: incomplete cooked material corpus: no .$extension products"
        exit 1
    }
done
(cd "$HOME_DIR/base/materials/cooked" && shasum -a 256 ./*) > "$RUN_OUTDIR/material-corpus.sha256"

echo "==> fps-perf-gate: $TAG  map=$MAP bots=$BOTS seed=$SEED viewpos=($VIEWPOS)"
echo "    current pack : $PACK_ROOT/base/pax21.sw3z"
echo "    base content : $CONTENT_ROOT/base ($content_staged pack(s), staged from read-only source)"
echo "    fs_homepath  : $HOME_NATIVE (isolated)"
echo "    evidence     : $RUN_OUTDIR"

# A fresh isolated home has no nav cache. Warm it with the sibling headless
# binary before the measured client run; otherwise addbot self-defers while the
# async bake runs and a nominal six-bot benchmark silently measures zero bots.
# Cold arena7 baking can exceed the former 450-frame preparation window.
# This unmeasured headless pass remains bounded by the 120-second watchdog;
# the ready-marker check below is still mandatory before measuring gameplay.
printf '%s\n' \
    'set sv_pure 0' \
    'set fixedtime 1' \
    'set com_maxfps 85' \
    "map $MAP" \
    'wait 1800' \
    'quit' > "$WARM_CONFIG"
warm_port=$((27960 + ($$ % 1000)))
set +e
"${PYTHON[@]}" "$(python_path "$SCRIPT_DIR/run-with-timeout.py")" \
    --timeout 120 --kill-after 15 \
    --cwd "$(python_path "$(dirname "$HEADLESS")")" \
    --stdout "$(python_path "$WARM_STDOUT")" -- \
    "$(python_path "$HEADLESS")" \
    +set fs_homepath "$HOME_NATIVE" +set s_initsound 0 \
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

# Optional extra cvar line for A/B experiments in the gameplay lane, e.g.
# FPS_PERF_EXTRA_CFG='set r_vkSplitPresent 1' (one console command, applied
# with the pinned settings before the map loads; recorded in recipe.cfg).
# Elevated hold position used while bots spawn (see the telefrag note in the recipe).
HOLD_VIEWPOS="$(echo "$VIEWPOS" | awk '{printf "%s %s %d %s", $1, $2, $3 + 256, $4}')"

# One cfg avoids the engine command-line's finite '+' token budget. The marker
# pair makes the parser consume only the pinned, steady measurement window.
{
    if [ "$GAMEPLAY_SECONDS" -gt 0 ]; then
        printf '%s\n' 'set sv_cheats 1' 'set sv_pure 0' 'set sv_floodProtect 0' 'set bot_enable 1' \
            'set sv_maxclients 16' 'set g_gametype 0' 'set g_warmup 0' 'set g_doWarmup 0' \
            'set g_scorelimit 0' 'set g_timelimit 0' "set sv_seed $SEED" \
            'set fixedtime 0' 'set timescale 1' 'set com_maxfps 0' \
            'set com_maxfpsUnfocused 1000' 'set com_maxfpsMinimized 30' \
            'set r_swapInterval 0' 'set com_automated 0' 'set con_notifytime 0' 'set s_volume 0' 'set s_musicVolume 0' \
            'set r_textureMode GL_LINEAR_MIPMAP_LINEAR' \
            'set r_ext_texture_filter_anisotropic 1' 'set r_ext_max_anisotropy 8' \
            'set r_bloom 1' 'set r_bloomPasses 2' 'set r_bloomThresholdMode 0' \
            ${FPS_PERF_EXTRA_CFG:+"$FPS_PERF_EXTRA_CFG"} \
            'set r_bloomThreshold 0.32' 'set r_bloomIntensity 0.5' \
            'set g_envWeather rain' 'set r_particles 1' 'set r_dynamiclight 1' \
            'set r_atmosphericGPU 1' 'set r_dither 0' \
            'log renderer.init info' 'log renderer.cmd info' 'log renderer.ral info' \
            'log renderer.timing debug' "map $MAP" 'waitForMap' \
            'cmd god' 'wait 8' 'cmd give all' 'wait 8' \
            "cmd setviewpos $VIEWPOS" 'wait 8' \
            'cmd noclip' 'wait 2' "cmd setviewpos $HOLD_VIEWPOS" 'wait 8' \
            'set g_envWeather rain' 'waitms 2000'
        for ((i=0; i<BOTS; i++)); do printf '%s\n' 'addbot visor 5' 'wait 8'; done
        # Bots spawn through G_KillBox: a player standing on the chosen spawn is
        # telefragged even in god mode, which the analyzer rejects. Hold the
        # player 256 units above the route position (noclip) while bots join,
        # then return and restore normal movement for the measured phases.
        printf '%s\n' 'wait 16' "cmd setviewpos $VIEWPOS" 'wait 2' 'cmd noclip' 'wait 8'
        printf '%s\n' 'set r_pinFrameTime 0' \
            'set r_pinShaderTime 0' 'set cg_debugevents 0' 'set cl_yawspeed 30' \
            'waitms 3000'
        # First-use weapon/material compilation belongs to the separately
        # recorded warmup, not hidden inside the steady interval population.
        printf '%s\n' 'echo FPS_GAMEPLAY_WEAPON_WARMUP_BEGIN'
        for weapon in 2 3 4 5; do
            printf '%s\n' "weapon $weapon" '+attack' 'waitms 1500' '-attack' 'waitms 500'
        done
        printf '%s\n' 'echo FPS_GAMEPLAY_WEAPON_WARMUP_END' 'cmd give ammo' \
            'waitms 2000'
        # Metal publishes native scene/presentation receipts at registration;
        # gfxinfo belongs to the legacy Vulkan frontend, not every backend.
        if [ "$BACKEND" = vulkan ]; then printf '%s\n' 'gfxinfo'; fi
        printf '%s\n' 'echo FPS_GAMEPLAY_SETTINGS_BEFORE'
        gameplay_settings='com_maxfps; com_maxfpsUnfocused; com_maxfpsMinimized; fixedtime; timescale; r_pinFrameTime; r_pinShaderTime; r_renderScale; r_hdr; r_hdrAutoExposure; r_bloom; r_bloomPasses; r_bloomThresholdMode; r_bloomThreshold; r_bloomIntensity; r_particles; g_envWeather; cl_renderer'
        printf '%s\n' "$gameplay_settings" 'echo FPS_GAMEPLAY_BEGIN' \
            'set com_perfTrace 1' 'set r_gpuSpeeds 1' 'set r_vkDebugTiming 1'
        # Wall-clock waits run real simulation, unlike fixedtime/frame waits.
        # Sampling only two frames per phase keeps diagnostic writes bounded;
        # their cost remains in the measured interval population.
        phase=0
        for weapon in 2 3 4 5; do
            phase=$((phase+1))
            printf '%s\n' "echo FPS_GAMEPLAY_PHASE_$phase" 'viewpos' \
                'cmd give ammo' "weapon $weapon" '+attack' '+forward' '+right' \
                "waitms $((GAMEPLAY_SECONDS * 250))" \
                'set r_speeds 7' 'wait 2' 'set r_speeds 0' \
                '-attack' '-forward' '-right' 'viewpos'
        done
        printf '%s\n' 'set com_perfTrace 0' 'echo FPS_GAMEPLAY_END' \
            'wait 2' 'echo FPS_GAMEPLAY_SETTINGS_AFTER' "$gameplay_settings" \
            'set r_gpuSpeeds 0' 'set r_vkDebugTiming 0' \
            'screenshot fps_gameplay_end png silent' 'wait 2' 'quit'
    else
    # g_envWeather is serverinfo-owned. Seed it before map startup so the
    # cgame receives the authored rain preset during its initial
    # CG_ParseServerinfo/CG_AtmosphericInit pass; changing only the client cvar
    # after the map has loaded does not publish atmosphere work.
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
        'set com_maxfpsUnfocused 1000' \
        'set com_maxfpsMinimized 30' \
        'set r_swapInterval 0' \
        'set com_automated 0' \
        'set log_severity DEBUG' \
        'set log_file_severity DEBUG' \
        'set sv_floodProtect 0' \
        'set g_envWeather rain' \
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
        'set con_notifytime 0' 'set s_volume 0' 'set s_musicVolume 0' \
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
		'give all' \
		'weapon 5' \
		'wait 20' \
        'set r_particles 0' \
        'wait 5' \
        'screenshot fps_particles_off_a png silent' \
        'wait 5' \
        'screenshot fps_particles_off_b png silent' \
        'set r_particles 1' \
		'set r_speeds 7' \
		'+attack' \
		'wait 10' \
		'-attack' \
		'wait 5' \
        'screenshot fps_particles_on png silent' \
		'set r_speeds 0' \
        'set cg_debugevents 0' \
        'set g_envWeather rain' \
        'wait 100' \
        'echo FPS_GATE_SCENE_BEGIN' \
        'print r_particles' \
        'print g_envWeather' \
        'set r_speeds 7' \
        'wait 20' \
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
		'+attack' \
        'com_maxfps; com_maxfpsUnfocused; com_maxfpsMinimized' \
        'echo FPS_GATE_MEASURE_BEGIN' \
        "wait $HOLD_FRAMES" \
        'echo FPS_GATE_MEASURE_END' \
        'com_maxfps; com_maxfpsUnfocused; com_maxfpsMinimized' \
		'-attack' \
        'set com_perfTrace 0' \
        'set r_gpuSpeeds 0' \
        'set r_vkDebugTiming 0' \
        'wait 2' \
        'quit'
    fi
} > "$CONFIG"
cp "$CONFIG" "$RUN_OUTDIR/recipe.cfg"

# ── presentation-regime pin (gameplay lane) ─────────────────────────────────
# macOS windowed presentation alternates on a minutes timescale between a
# compositor-locked regime (native Metal 120 FPS, MoltenVK 240 with 3 images)
# and a decoupled regime (GPU-bound, 400+ FPS) with no engine change
# (2026-09-11 evidence: the same binary measured 119.8 and 497.9 FPS three
# minutes apart). A 250-FPS acceptance is meaningless in the locked regime, so
# the gameplay lane first runs a short static probe of the same backend/map and
# only proceeds once the probe throughput proves the decoupled regime; each
# attempt is recorded so the report can never silently mix regimes.
#   FPS_PERF_REGIME_MIN_FPS   probe threshold (0 disables the pin; default 400)
#   FPS_PERF_REGIME_ATTEMPTS  probe attempts before FAIL (default 6)
#   FPS_PERF_REGIME_WAIT      seconds between attempts (default 45)
REGIME_MIN_FPS="${FPS_PERF_REGIME_MIN_FPS:-400}"
REGIME_ATTEMPTS="${FPS_PERF_REGIME_ATTEMPTS:-6}"
REGIME_WAIT="${FPS_PERF_REGIME_WAIT:-45}"
case "$REGIME_MIN_FPS$REGIME_ATTEMPTS$REGIME_WAIT" in *[!0-9]*) echo "FAIL: FPS_PERF_REGIME_* must be integers" >&2; exit 2 ;; esac
REGIME_JSON="$RUN_OUTDIR/regime-probe.json"
if [ "$GAMEPLAY_SECONDS" -gt 0 ] && [ "$REGIME_MIN_FPS" -gt 0 ]; then
    PROBE_CFG="$HOME_DIR/base/fps-regime-probe.cfg"
    printf '%s\n' 'set sv_cheats 1' 'set sv_pure 0' 'set com_maxfps 0' \
        'set com_maxfpsUnfocused 1000' 'set com_maxfpsMinimized 30' 'set r_swapInterval 0' \
        'set con_notifytime 0' 'set s_volume 0' "map $MAP" 'waitForMap' 'wait 80' \
        'cmd noclip' 'wait 20' "cmd setviewpos $VIEWPOS" 'wait 200' \
        'echo FPS_REGIME_PROBE_BEGIN' 'wait 600' 'echo FPS_REGIME_PROBE_END' 'wait 2' 'quit' > "$PROBE_CFG"
    regime_pinned=0
    printf '{"schema":1,"threshold_fps":%s,"attempts":[' "$REGIME_MIN_FPS" > "$REGIME_JSON"
    for (( attempt=1; attempt<=REGIME_ATTEMPTS; attempt++ )); do
        rm -f "$QCONSOLE"
        set +e
        "${PYTHON[@]}" "$(python_path "$SCRIPT_DIR/run-with-timeout.py")" \
            --timeout 120 --kill-after 15 \
            --cwd "$(python_path "$(dirname "$RUN_ENGINE")")" \
            --stdout "$(python_path "$RUN_OUTDIR/regime-probe-$attempt.stdout.log")" -- \
            "$(python_path "$RUN_ENGINE")" \
            +set fs_homepath "$HOME_NATIVE" +set s_initsound 0 +set com_noHardReboot 1 +set vm_game 0 +set vm_cgame 0 \
            +set log_file_severity DEBUG +set r_mode -1 +set r_customwidth "$RENDER_WIDTH" \
            +set r_customheight "$RENDER_HEIGHT" +set r_fullscreen 0 +set r_fbo 1 \
            +set r_renderScale "$RENDER_SCALE" +set r_renderWidth "$RENDER_WIDTH" \
            +set r_renderHeight "$RENDER_HEIGHT" "${GAMEPLAY_ARGS[@]}" +exec fps-regime-probe.cfg
        probe_rc=$?
        set -e
        probe_fps="$("${PYTHON[@]}" - "$QCONSOLE" <<'PY' 2>/dev/null || echo 0
import json,sys
from datetime import datetime
b=e=None
for line in open(sys.argv[1],encoding="utf-8",errors="replace"):
    try: r=json.loads(line)
    except Exception: continue
    m=r.get("msg","")
    t=r.get("ts")
    if "FPS_REGIME_PROBE_BEGIN" in m and t: b=datetime.fromisoformat(t.replace("Z","+00:00")).timestamp()
    if "FPS_REGIME_PROBE_END" in m and t: e=datetime.fromisoformat(t.replace("Z","+00:00")).timestamp()
print(f"{600.0/(e-b):.1f}" if b and e and e>b else "0")
PY
)"
        # No early 'exit' in awk: under pipefail a SIGPIPE'd ps would abort the lane (141).
        ws_cpu="$(ps -eo pcpu,comm 2>/dev/null | awk '/WindowServer/ {v=$1} END {print v}')"
        [ "$attempt" -eq 1 ] || printf ',' >> "$REGIME_JSON"
        printf '{"attempt":%s,"probe_fps":%s,"engine_rc":%s,"windowserver_cpu":"%s","utc":"%s"}' \
            "$attempt" "${probe_fps:-0}" "$probe_rc" "${ws_cpu:-}" "$(date -u +%Y-%m-%dT%H:%M:%SZ)" >> "$REGIME_JSON"
        echo "==> regime probe $attempt/$REGIME_ATTEMPTS: ${probe_fps:-0} FPS (threshold $REGIME_MIN_FPS, WindowServer ${ws_cpu:-?}% CPU)"
        if [ "$probe_rc" -eq 0 ] && [ "$("${PYTHON[@]}" -c "print(int(float('${probe_fps:-0}') >= $REGIME_MIN_FPS))")" = 1 ]; then
            regime_pinned=1; break
        fi
        [ "$attempt" -lt "$REGIME_ATTEMPTS" ] && sleep "$REGIME_WAIT"
    done
    printf '],"pinned":%s}\n' "$regime_pinned" >> "$REGIME_JSON"
    rm -f "$QCONSOLE" "$PROBE_CFG"
    if [ "$regime_pinned" -ne 1 ]; then
        echo "FAIL: presentation regime locked (probe never reached $REGIME_MIN_FPS FPS in $REGIME_ATTEMPTS attempts); see $REGIME_JSON" >&2
        exit 3
    fi
fi

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
    --timeout "$((200 + GAMEPLAY_SECONDS))" --kill-after 15 \
    --cwd "$(python_path "$(dirname "$RUN_ENGINE")")" \
    --stdout "$(python_path "$STDOUT")" -- \
    "$(python_path "$RUN_ENGINE")" \
    +set fs_homepath "$HOME_NATIVE" +set s_initsound 0 \
    +set com_noHardReboot 1 \
    +set vm_game 0 \
    +set vm_cgame 0 \
    +set log_severity DEBUG \
    +set log_file_severity DEBUG \
    +set r_mode -1 \
    +set r_customwidth "$RENDER_WIDTH" \
    +set r_customheight "$RENDER_HEIGHT" \
    +set r_fullscreen 0 \
    +set r_fbo 1 \
    +set r_renderScale "$RENDER_SCALE" \
    +set r_renderWidth "$RENDER_WIDTH" \
    +set r_renderHeight "$RENDER_HEIGHT" \
    "${GAMEPLAY_ARGS[@]}" \
    +exec fps-perf-gate.cfg
engine_rc=$?
set -e

# Keep the native failure evidence before isolated-home cleanup too.
[ ! -s "$QCONSOLE" ] || cp "$QCONSOLE" "$QSNAP"

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

if [ "$GAMEPLAY_SECONDS" -gt 0 ]; then
    gameplay_shot="$HOME_DIR/base/screenshots/fps_gameplay_end.png"
    [ ! -s "$gameplay_shot" ] || cp "$gameplay_shot" "$RUN_OUTDIR/gameplay-end.png"
    "${PYTHON[@]}" "$(python_path "$SCRIPT_DIR/fps-perf-analyze.py")" \
        --input "$(python_path "$QSNAP")" --tag "$TAG" --map "$MAP" --bots "$BOTS" \
        --gameplay-seconds "$GAMEPLAY_SECONDS" --backend "$BACKEND" \
        --engine-sha256 "$(sha256_file "$RUN_ENGINE")" \
        --pax21-sha256 "$(sha256_file "$PACK_ROOT/base/pax21.sw3z")"
    exit $?
fi

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
"$PNG2RAW" --size "${RENDER_WIDTH}x${RENDER_HEIGHT}" "$SHOT_OFF_A" > "$RAW_OFF_A"
"$PNG2RAW" --size "${RENDER_WIDTH}x${RENDER_HEIGHT}" "$SHOT_OFF_B" > "$RAW_OFF_B"
"$PNG2RAW" --size "${RENDER_WIDTH}x${RENDER_HEIGHT}" "$SHOT_ON" > "$RAW_ON"
read -r PARTICLE_PIXELS PARTICLE_CHANGED PARTICLE_MAD PARTICLE_CONTROL_CHANGED PARTICLE_CONTROL_MAD <<EOF
$("${PYTHON[@]}" -c 'import pathlib,sys; a=pathlib.Path(sys.argv[1]).read_bytes(); b=pathlib.Path(sys.argv[2]).read_bytes(); c=pathlib.Path(sys.argv[3]).read_bytes(); assert len(a)==len(b)==len(c) and len(a)%3==0, (len(a),len(b),len(c)); metric=lambda x,y: (sum(any(d) for d in zip(*(iter([abs(i-j) for i,j in zip(x,y)]),)*3)), sum(abs(i-j) for i,j in zip(x,y))/len(x)); changed,mad=metric(b,c); control_changed,control_mad=metric(a,b); print(len(a)//3,changed,mad,control_changed,control_mad)' "$(python_path "$RAW_OFF_A")" "$(python_path "$RAW_OFF_B")" "$(python_path "$RAW_ON")")
EOF

# ── parse (JSON, not string-grep) ─────────────────────────────────────────
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
