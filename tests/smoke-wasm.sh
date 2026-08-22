#!/usr/bin/env bash
# smoke-wasm.sh — WASM VM backend smoke test
#
# Runs a headless gamesv check and a finite client/server gamesv+gamecl check
# in supported VM mode 2 on arena7 (Temple of Retribution). arena7 is in the
# Q3 demo PAK (`demoq3/pak0.pk3`,
# verified via launcher/internal/pipeline/proc_q3copy_entries_pax01.go:102),
# so this smoke runs against redistributable demo assets — no full Q3
# install required.
#
# Asset bootstrap (single canonical path):
#   The launcher CLI (`q3now-launcher assets download/import`) is the
#   one and only bootstrap mechanism. Smoke detects the launcher binary
#   in the build tree, then invokes it with HOME (Linux/macOS) and
#   USERPROFILE (Windows) redirected to a temp dir so the launcher's
#   default `$HOME/wired-preview/` resolves to a smoke-local path
#   instead of polluting the user's real install.
#
#   The launcher's q3copy pipeline converts raw demo PAKs (TGA→PNG,
#   WAV→Opus, repackage) into q3now's SW3Z format under
#   `<work>/wired-preview/base/pax01.sw3z`. The q3now engine accepts
#   `.sw3z` natively (code/qcommon/sw3z.c), so smoke launches against
#   that directory as `fs_installpath`.
#
#   If the launcher binary is missing, smoke fails with a clear pointer
#   at the build instructions. There is no manual fallback — a single
#   known-good path beats two paths where one is broken.
#
# Map choice: arena7. Reasoning: shipped in demoq3/pak0.pk3 and provides a
# stable full client gameplay transition. Override via the second positional
# arg if needed.
#
# Usage:
#   bash tests/smoke-wasm.sh [path-to-ded] [map] [path-to-basepath]
#
# Env overrides:
#   BASEPATH=...               Skip the launcher bootstrap entirely. Must
#                              point at a directory containing
#                              base/{pak0.pk3,*.sw3z}. Useful when you
#                              already have a full Q3 install or a
#                              previously-imported q3now base.
#   Q3NOW_DEMO_PAK_DIR=...     Working dir used as HOME/USERPROFILE for the
#                              launcher invocation. Persists between runs
#                              for idempotency. Default: <repo>/base-demo.
#   Q3NOW_LAUNCHER=...         Override the launcher binary path. Default:
#                              auto-detect <repo>/launcher/build/bin/...
#   WIRED_CLIENT=...           Override the GUI client binary used to prove
#                              gamecl + FIRST GAMEPLAY FRAME.
#   WASM_SMOKE_OUTPUT_DIR=...  Persistent evidence root. Default:
#                              build/test-results/wasm-smoke.
#   Q3NOW_CHANNEL=...          Channel suffix the launcher was built with.
#                              Default: "-preview" (matches Makefile default).
#                              Smoke uses this to find the launcher's output
#                              dir (HOME/wired${CHANNEL}/base/).

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

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

# ── Locate headless server binary ──────────────────────────────────────────
DED_OVERRIDE="${1:-}"
MAP="${2:-arena7}"
BASEPATH_OVERRIDE="${3:-${BASEPATH:-}}"

if [ -n "$DED_OVERRIDE" ]; then
    DED="$DED_OVERRIDE"
else
    for candidate in \
        "${PROJECT_DIR}/build/release/wired-headless.arm64" \
        "${PROJECT_DIR}/build/release/wired-headless.aarch64" \
        "${PROJECT_DIR}/build/release/wired-headless.x86_64" \
        "${PROJECT_DIR}/build/release/wired-headless.x64.exe" \
        "${PROJECT_DIR}/build/release/wired-headless.exe" \
        "${PROJECT_DIR}/build/release/wired-headless" \
    ; do
        if [ -x "$candidate" ]; then DED="$candidate"; break; fi
    done
fi

if [ -z "${DED:-}" ] || [ ! -x "$DED" ]; then
    echo "ERROR: headless server not found. Tried:"
    echo "  build/release/wired-headless.{x86_64,x64.exe,arm64,aarch64}"
    echo "Override: bash $0 path-to-ded [map] [basepath]"
    exit 1
fi

# The client run is authoritative for gamecl: a headless server can only prove
# gamesv. Keep discovery beside the headless binary so both use one build.
CLIENT="${WIRED_CLIENT:-}"
if [ -z "$CLIENT" ]; then
    for candidate in \
        "${PROJECT_DIR}/build/release/q3now-preview.arm64.app/Contents/MacOS/wired.arm64" \
        "${PROJECT_DIR}/build/release/wired.arm64" \
        "${PROJECT_DIR}/build/release/wired.aarch64" \
        "${PROJECT_DIR}/build/release/wired.x86_64" \
        "${PROJECT_DIR}/build/release/wired.x64.exe" \
        "${PROJECT_DIR}/build/release/wired.exe" \
        "${PROJECT_DIR}/build/release/wired" \
    ; do
        if [ -x "$candidate" ]; then CLIENT="$candidate"; break; fi
    done
fi
if [ -z "$CLIENT" ] || [ ! -x "$CLIENT" ]; then
    echo "ERROR: client engine not found; gamecl WASM cannot be proven."
    echo "Override with WIRED_CLIENT=/path/to/wired."
    exit 1
fi

# ── Locate launcher binary ──────────────────────────────────────────────────
LAUNCHER="${Q3NOW_LAUNCHER:-}"
if [ -z "$LAUNCHER" ]; then
    for candidate in \
        "${PROJECT_DIR}/launcher/build/bin/q3now-launcher.exe" \
        "${PROJECT_DIR}/launcher/build/bin/q3now-launcher" \
    ; do
        if [ -x "$candidate" ]; then LAUNCHER="$candidate"; break; fi
    done
fi

# ── Helper: does a basepath contain usable game assets? ─────────────────────
# Engine accepts pak0.pk3 (vanilla layout) or *.sw3z (q3now/launcher layout).
has_basegame_assets() {
    local dir="$1"
    [ -f "${dir}/base/pak0.pk3" ] && return 0
    if compgen -G "${dir}/base/*.sw3z" > /dev/null; then return 0; fi
    if compgen -G "${dir}/base/*.pk3" > /dev/null; then return 0; fi
    return 1
}

# ── Resolve basepath: explicit override > already-imported > launcher CLI ──
WORK_DIR="${Q3NOW_DEMO_PAK_DIR:-${PROJECT_DIR}/base-demo}"
CHANNEL="${Q3NOW_CHANNEL:--preview}"
LAUNCHER_OUTPUT="${WORK_DIR}/wired${CHANNEL}"

if [ -n "$BASEPATH_OVERRIDE" ] && has_basegame_assets "$BASEPATH_OVERRIDE"; then
    Q3DIR="$BASEPATH_OVERRIDE"
    echo "  -- Using basepath override: $Q3DIR"
elif has_basegame_assets "$LAUNCHER_OUTPUT"; then
    Q3DIR="$LAUNCHER_OUTPUT"
    echo "  -- Reusing previously-imported assets: $Q3DIR"
elif [ -n "$LAUNCHER" ]; then
    echo "  -- Bootstrapping demo assets via $LAUNCHER"
    echo "  -- WORK_DIR: $WORK_DIR (will be HOME/USERPROFILE for launcher)"
    mkdir -p "$WORK_DIR"

    # Redirect HOME (Linux/macOS) and USERPROFILE (Windows) so the launcher's
    # `os.UserHomeDir()` resolves to the smoke work dir instead of the user's
    # real home. Tracked as a CLI followup in docs/health.md "launcher CLI:
    # --home-dir flag" — proper flag would replace this env-var workaround.
    HOME="$WORK_DIR" USERPROFILE="$WORK_DIR" \
        "$LAUNCHER" assets download --accept-eula
    HOME="$WORK_DIR" USERPROFILE="$WORK_DIR" \
        "$LAUNCHER" assets import

    if ! has_basegame_assets "$LAUNCHER_OUTPUT"; then
        echo "ERROR: launcher import completed but no base assets found at"
        echo "  $LAUNCHER_OUTPUT/base/"
        ls -la "$LAUNCHER_OUTPUT/base/" 2>&1 || true
        exit 1
    fi

    Q3DIR="$LAUNCHER_OUTPUT"
    echo "  -- Bootstrap complete: $Q3DIR"
else
    echo "ERROR: no launcher binary found and no \$BASEPATH override."
    echo ""
    echo "  Build the launcher first:    make create-launcher"
    echo "  Or set BASEPATH=/path/to/q3 if you have an existing install."
    echo ""
    echo "  Searched for launcher at:"
    echo "    ${PROJECT_DIR}/launcher/build/bin/q3now-launcher.exe"
    echo "    ${PROJECT_DIR}/launcher/build/bin/q3now-launcher"
    exit 1
fi

# ── Stage build artifacts into an isolated writable home ──────────────────
# Two distinct artifact groups, both produced by `make` / `make create-packs`:
#   1. WASM modules (gamesv.wasm, gamecl.wasm) from <build>/<config>/base/vm/
#      → engine searches fs_homepath/base/vm/ before the read-only install root
#   2. Mod pack (pax21.sw3z) from <build>/base/  — this is what ships
#      modfiles/default.cfg and other q3now overrides; without it the engine
#      hard-fails with "Couldn't load default.cfg". The user's install/content
#      root is never mutated by the smoke.
RESULT_ROOT="${WASM_SMOKE_OUTPUT_DIR:-${PROJECT_DIR}/build/test-results/wasm-smoke}"
RUN_DIR="${RESULT_ROOT}/run-$(date +%Y%m%d-%H%M%S)-$$"
HOME_PATH="${RUN_DIR}/home"
mkdir -p "${HOME_PATH}/base/vm"

content_staged=0
for content_pack in "${Q3DIR}"/base/*.sw3z "${Q3DIR}"/base/*.pk3; do
    [ -f "$content_pack" ] || continue
    cp -f "$content_pack" "${HOME_PATH}/base/"
    content_staged=$((content_staged + 1))
done
if [ "$content_staged" -eq 0 ]; then
    echo "  -- ERROR: basepath resolved but no content pack could be staged."
    exit 1
fi
echo "  -- Staged $content_staged read-only content pack(s) from $Q3DIR"

WASM_BUILD_DIR=""
for candidate in \
    "${PROJECT_DIR}/build/release/Release/base/vm" \
    "${PROJECT_DIR}/build/release/base/vm" \
; do
    if [ -f "${candidate}/gamesv.wasm" ]; then WASM_BUILD_DIR="$candidate"; break; fi
done

if [ -n "$WASM_BUILD_DIR" ]; then
    cp -f "${WASM_BUILD_DIR}/gamesv.wasm" "${WASM_BUILD_DIR}/gamecl.wasm" "${HOME_PATH}/base/vm/"
    for module in gamesv gamecl; do
        if [ -f "${WASM_BUILD_DIR}/${module}.aot" ]; then
            cp -f "${WASM_BUILD_DIR}/${module}.aot" "${HOME_PATH}/base/vm/"
        fi
    done
    echo "  -- Staged WASM modules from $WASM_BUILD_DIR"
else
    echo "  -- ERROR: no compiled gamesv.wasm found in build tree."
    echo "     Build with USE_WASM=ON first."
    exit 1
fi

MODPACK_BUILD=""
for candidate in \
    "${PROJECT_DIR}/build/release/base/pax21.sw3z" \
    "${PROJECT_DIR}/build/release/base/pax21.pk3" \
; do
    if [ -f "$candidate" ]; then MODPACK_BUILD="$candidate"; break; fi
done

if [ -n "$MODPACK_BUILD" ]; then
    cp -f "$MODPACK_BUILD" "${HOME_PATH}/base/"
    echo "  -- Staged mod pack: $(basename "$MODPACK_BUILD")"
else
    echo "  -- ERROR: mod pack (pax21.sw3z) not found at"
    echo "     ${PROJECT_DIR}/build/release/base/pax21.{sw3z,pk3}"
    echo "     Build it first:  make create-packs"
    echo "     Without it, the engine fails at startup with"
    echo "     'Couldn't load default.cfg' — modfiles/default.cfg ships"
    echo "     in pax21.sw3z."
    exit 1
fi

echo "=== WASM Smoke Test ==="
echo "Server:   $DED"
echo "Client:   $CLIENT"
echo "Basepath: $Q3DIR"
echo "Homepath: $HOME_PATH"
echo "Map:      $MAP"
echo "Evidence: $RUN_DIR"
echo ""

run_engine() {
    local binary="$1"
    local stdout_file="$2"
    shift 2
    local binary_dir
    binary_dir="$(cd "$(dirname "$binary")" && pwd)"

    set +e
    "${PYTHON[@]}" "$(python_path "$SCRIPT_DIR/run-with-timeout.py")" \
        --timeout 45 --kill-after 10 \
        --cwd "$(python_path "$binary_dir")" \
        --stdout "$(python_path "$stdout_file")" -- \
        "$(python_path "$binary_dir/$(basename "$binary")")" "$@"
    local rc=$?
    set -e
    if [ "$rc" -ne 0 ]; then
        echo "  FAIL: engine exited with status $rc"
        tail -40 "$stdout_file" 2>/dev/null || true
        exit 1
    fi
    if grep -aEq 'died on signal|Sys_Error|FATAL|WASM: failed|WASM[^:]*exception' "$stdout_file"; then
        echo "  FAIL: fatal/WASM error signature in $stdout_file"
        grep -aE 'died on signal|Sys_Error|FATAL|WASM: failed|WASM[^:]*exception' "$stdout_file" | tail -20
        exit 1
    fi
}

HOME_NATIVE="$(python_path "$HOME_PATH")"
SERVER_LOG="$RUN_DIR/server.log"
CLIENT_LOG="$RUN_DIR/client.log"

# ── Test 1: authoritative server-module load ───────────────────────────────
echo "[1/2] Running headless server with vm_game=2..."
run_engine "$DED" "$SERVER_LOG" \
    +set fs_homepath "$HOME_NATIVE" \
    +set com_noHardReboot 1 \
    +set sv_pure 0 \
    +set vm_game 2 \
    +map "$MAP" \
    +wait 300 \
    +quit

if ! grep -aEq 'gamesv\.(wasm|aot) loaded as WASM (interpreter|AOT)' "$SERVER_LOG"; then
    echo "  FAIL: gamesv did not load through WAMR"
    tail -60 "$SERVER_LOG"
    exit 1
fi
echo "  PASS: gamesv loaded through WAMR"

# ── Test 2: client-module load + gameplay transition ───────────────────────
# A headless process cannot instantiate gamecl. This finite window runs a local
# client/server, requires both modules, and proves cgame reached CA_ACTIVE.
echo "[2/2] Running client with vm_game=2 + vm_cgame=2..."
run_engine "$CLIENT" "$CLIENT_LOG" \
    +set fs_homepath "$HOME_NATIVE" \
    +set com_noHardReboot 1 \
    +set com_automated 1 \
    +set r_fullscreen 0 \
    +set r_mode -1 \
    +set r_customwidth 1280 \
    +set r_customheight 720 \
    +set sv_pure 0 \
    +set vm_game 2 \
    +set vm_cgame 2 \
    +map "$MAP" \
    +wait 500 \
    +quit

for module in gamesv gamecl; do
    if ! grep -aEq "${module}\\.(wasm|aot) loaded as WASM (interpreter|AOT)" "$CLIENT_LOG"; then
        echo "  FAIL: $module did not load through WAMR in client run"
        tail -80 "$CLIENT_LOG"
        exit 1
    fi
done
if ! grep -aEq "FIRST GAMEPLAY FRAME mapname=(maps/)?${MAP}(\\.bsp)?([^A-Za-z0-9_.-]|$)" "$CLIENT_LOG"; then
    echo "  FAIL: client did not reach the first gameplay frame on $MAP"
    tail -80 "$CLIENT_LOG"
    exit 1
fi
echo "  PASS: gamesv + gamecl loaded through WAMR; first gameplay frame reached"

echo ""
echo "=== WASM Smoke Test Complete ==="
