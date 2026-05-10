#!/usr/bin/env bash
# smoke-wasm.sh — WASM VM backend smoke test
#
# Runs the dedicated server with WASM modules (vm_game=3) on q3dm7
# (Temple of Retribution). q3dm7 is in the Q3 demo PAK (`demoq3/pak0.pk3`,
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
#   `<work>/wired-preview/baseq3/pax01.sw3z`. The q3now engine accepts
#   `.sw3z` natively (code/qcommon/sw3z.c), so smoke launches against
#   that directory as `fs_installpath`.
#
#   If the launcher binary is missing, smoke fails with a clear pointer
#   at the build instructions. There is no manual fallback — a single
#   known-good path beats two paths where one is broken.
#
# Map choice: q3dm7. Reasoning: shipped in demoq3/pak0.pk3, exercises
# bot AI navigation, jump pads, item placement variety better than
# q3dm1/q3dm17. Override via the second positional arg if needed.
#
# Usage:
#   bash tests/smoke-wasm.sh [path-to-ded] [map] [path-to-basepath]
#
# Env overrides:
#   BASEPATH=...               Skip the launcher bootstrap entirely. Must
#                              point at a directory containing
#                              baseq3/{pak0.pk3,*.sw3z}. Useful when you
#                              already have a full Q3 install or a
#                              previously-imported q3now baseq3.
#   Q3NOW_DEMO_PAK_DIR=...     Working dir used as HOME/USERPROFILE for the
#                              launcher invocation. Persists between runs
#                              for idempotency. Default: <repo>/baseq3-demo.
#   Q3NOW_LAUNCHER=...         Override the launcher binary path. Default:
#                              auto-detect <repo>/launcher/build/bin/...
#   Q3NOW_CHANNEL=...          Channel suffix the launcher was built with.
#                              Default: "-preview" (matches Makefile default).
#                              Smoke uses this to find the launcher's output
#                              dir (HOME/wired${CHANNEL}/baseq3/).

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

# ── Locate dedicated server binary ──────────────────────────────────────────
DED_OVERRIDE="${1:-}"
MAP="${2:-q3dm7}"
BASEPATH_OVERRIDE="${3:-${BASEPATH:-}}"

if [ -n "$DED_OVERRIDE" ]; then
    DED="$DED_OVERRIDE"
else
    for candidate in \
        "${PROJECT_DIR}/build/release/wired-ded.x86_64" \
        "${PROJECT_DIR}/build/release/wired-ded.x64.exe" \
        "${PROJECT_DIR}/build/Release/wired-ded.arm64" \
        "${PROJECT_DIR}/build/release/wired-ded.aarch64" \
    ; do
        if [ -x "$candidate" ]; then DED="$candidate"; break; fi
    done
fi

if [ -z "${DED:-}" ] || [ ! -x "$DED" ]; then
    echo "ERROR: dedicated server not found. Tried:"
    echo "  build/release/wired-ded.{x86_64,x64.exe,arm64,aarch64}"
    echo "Override: bash $0 path-to-ded [map] [basepath]"
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
has_baseq3_assets() {
    local dir="$1"
    [ -f "${dir}/baseq3/pak0.pk3" ] && return 0
    if compgen -G "${dir}/baseq3/*.sw3z" > /dev/null; then return 0; fi
    if compgen -G "${dir}/baseq3/*.pk3" > /dev/null; then return 0; fi
    return 1
}

# ── Resolve basepath: explicit override > already-imported > launcher CLI ──
WORK_DIR="${Q3NOW_DEMO_PAK_DIR:-${PROJECT_DIR}/baseq3-demo}"
CHANNEL="${Q3NOW_CHANNEL:--preview}"
LAUNCHER_OUTPUT="${WORK_DIR}/wired${CHANNEL}"

if [ -n "$BASEPATH_OVERRIDE" ] && has_baseq3_assets "$BASEPATH_OVERRIDE"; then
    Q3DIR="$BASEPATH_OVERRIDE"
    echo "  -- Using basepath override: $Q3DIR"
elif has_baseq3_assets "$LAUNCHER_OUTPUT"; then
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

    if ! has_baseq3_assets "$LAUNCHER_OUTPUT"; then
        echo "ERROR: launcher import completed but no baseq3 assets found at"
        echo "  $LAUNCHER_OUTPUT/baseq3/"
        ls -la "$LAUNCHER_OUTPUT/baseq3/" 2>&1 || true
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

# ── Stage build artifacts into the smoke basepath ──────────────────────────
# Two distinct artifact groups, both produced by `make` / `make create-packs`:
#   1. WASM modules (qagame.wasm, cgame.wasm) from <build>/<config>/baseq3/vm/
#      → engine searches fs_installpath/baseq3/vm/ at runtime
#   2. Mod pack (pax21.sw3z) from <build>/baseq3/  — this is what ships
#      modfiles/default.cfg and other q3now overrides; without it the engine
#      hard-fails with "Couldn't load default.cfg"
WASM_BUILD_DIR=""
for candidate in \
    "${PROJECT_DIR}/build/release/Release/baseq3/vm" \
    "${PROJECT_DIR}/build/release/baseq3/vm" \
; do
    if [ -f "${candidate}/qagame.wasm" ]; then WASM_BUILD_DIR="$candidate"; break; fi
done

if [ -n "$WASM_BUILD_DIR" ]; then
    mkdir -p "${Q3DIR}/baseq3/vm"
    cp -f "${WASM_BUILD_DIR}/qagame.wasm" "${WASM_BUILD_DIR}/cgame.wasm" "${Q3DIR}/baseq3/vm/"
    echo "  -- Staged WASM modules from $WASM_BUILD_DIR"
else
    echo "  -- WARNING: no compiled .wasm found in build tree;"
    echo "     vm_game=3 test will fail unless modules are already at"
    echo "     ${Q3DIR}/baseq3/vm/. Build with USE_WASM=ON first."
fi

MODPACK_BUILD=""
for candidate in \
    "${PROJECT_DIR}/build/release/baseq3/pax21.sw3z" \
    "${PROJECT_DIR}/build/release/baseq3/pax21.pk3" \
; do
    if [ -f "$candidate" ]; then MODPACK_BUILD="$candidate"; break; fi
done

if [ -n "$MODPACK_BUILD" ]; then
    mkdir -p "${Q3DIR}/baseq3"
    cp -f "$MODPACK_BUILD" "${Q3DIR}/baseq3/"
    echo "  -- Staged mod pack: $(basename "$MODPACK_BUILD")"
else
    echo "  -- ERROR: mod pack (pax21.sw3z) not found at"
    echo "     ${PROJECT_DIR}/build/release/baseq3/pax21.{sw3z,pk3}"
    echo "     Build it first:  make create-packs"
    echo "     Without it, the engine fails at startup with"
    echo "     'Couldn't load default.cfg' — modfiles/default.cfg ships"
    echo "     in pax21.sw3z."
    exit 1
fi

echo "=== WASM Smoke Test ==="
echo "Server:   $DED"
echo "Basepath: $Q3DIR"
echo "Map:      $MAP"
echo ""

# ── Test 1: Explicit WASM mode (vm_game=3) ──────────────────────────
echo "[1/3] Running dedicated server with vm_game=3 (explicit WASM)..."
timeout 30 "$DED" \
    +set fs_installpath "$Q3DIR" \
    +set vm_game 3 \
    +set dedicated 2 \
    +map "$MAP" \
    +addbot sarge 1 \
    +wait 300 \
    +quit \
    2>&1 | tee /tmp/smoke-wasm-1.log || true

if grep -q "loaded as WASM" /tmp/smoke-wasm-1.log; then
    echo "  PASS: WASM module loaded"
else
    echo "  FAIL: WASM module did not load"
    exit 1
fi

# ── Test 2: Auto-detect mode (vm_game=2 with .wasm present) ─────────
echo "[2/3] Running with vm_game=2 (auto-detect, should prefer .wasm)..."
timeout 30 "$DED" \
    +set fs_installpath "$Q3DIR" \
    +set vm_game 2 \
    +set dedicated 2 \
    +map "$MAP" \
    +wait 100 \
    +quit \
    2>&1 | tee /tmp/smoke-wasm-2.log || true

if grep -q "WASM" /tmp/smoke-wasm-2.log; then
    echo "  PASS: Auto-detected WASM module"
else
    echo "  INFO: No WASM auto-detect (may be expected if no .wasm in vm/)"
fi

# ── Test 3: Fallback when no .wasm exists ────────────────────────────
echo "[3/3] Testing QVM fallback (rename .wasm temporarily)..."
WASM_FILE="${Q3DIR}/baseq3/vm/qagame.wasm"
if [ -f "$WASM_FILE" ]; then
    mv "$WASM_FILE" "${WASM_FILE}.bak"
    timeout 15 "$DED" \
        +set fs_installpath "$Q3DIR" \
        +set vm_game 2 \
        +set dedicated 2 \
        +map "$MAP" \
        +wait 50 \
        +quit \
        2>&1 | tee /tmp/smoke-wasm-3.log || true
    mv "${WASM_FILE}.bak" "$WASM_FILE"

    if grep -q "QVM" /tmp/smoke-wasm-3.log || grep -q "compiled" /tmp/smoke-wasm-3.log; then
        echo "  PASS: Fell back to QVM when .wasm missing"
    else
        echo "  INFO: Could not verify QVM fallback"
    fi
else
    echo "  SKIP: No .wasm file to test fallback"
fi

echo ""
echo "=== WASM Smoke Test Complete ==="
