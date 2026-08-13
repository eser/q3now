#!/usr/bin/env bash
# visual-feature-bless-verify.sh — fully-automated, ZERO-human-in-the-loop
# bless+verify for a render-feature visual gate (GTAO or Forward+).
#
# Two-step (mirrors tests/rebless-and-verify.sh): (1) capture → run the COMPUTED
# sanity assertions → bless the golden ONLY when the assertions pass; (2) cold
# re-run to verify the current build matches the just-blessed golden. The "is it
# sane?" decision is the computed assertion inside visual-render-features.sh
# (AO open≈1.0 / corners<1.0 / spread for GTAO; tile-equivalence for Forward+),
# NEVER a human look. Eser is terminal-only.
#
# For mode=gtao this builds a dedicated FEAT_SSAO=1 verify DLL (USE_SSAO=ON)
# into a separate build dir and swaps it into the run dir, so the current
# product renderer is never clobbered; it restores that renderer on exit.
# mode=fwdplus uses the current product build.
#
# Usage: visual-feature-bless-verify.sh <gtao|fwdplus> <run-build-dir> <verify-build-dir>
set -u

MODE="${1:?mode (gtao|fwdplus) required}"
RUN_DIR="${2:?run build dir required}"
SSAO_DIR="${3:-build/ssao-verify}"
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

ENGINE="$RUN_DIR/wired.x64.exe"
SHIP_DLL="$RUN_DIR/wired_vulkan_x86_64.dll"
DLL_BAK="$SHIP_DLL.shipbak"
RESTORE_DLL=0

cleanup() {
    if [ "$RESTORE_DLL" = "1" ] && [ -f "$DLL_BAK" ]; then
        cp "$DLL_BAK" "$SHIP_DLL" && rm -f "$DLL_BAK"
        echo "==== restored current product renderer DLL ===="
    fi
}
trap cleanup EXIT

if [ "$MODE" = "gtao" ]; then
    echo "==== build the FEAT_SSAO=1 verify renderer DLL (USE_SSAO=ON) ===="
    cmake -S . -B "$SSAO_DIR" -G Ninja -DCMAKE_BUILD_TYPE=Debug -DUSE_SSAO=ON -DUSE_RENDERER_DLOPEN=ON >/dev/null 2>&1
    ninja -C "$SSAO_DIR" wired_vulkan_x86_64 || { echo "FAIL: FEAT_SSAO=1 DLL build failed"; exit 1; }
    # swap the dedicated FEAT_SSAO=1 DLL into the run dir (only the renderer DLL needs SSAO;
    # the engine + game are FEAT_SSAO-agnostic, so the proven run-dir engine is reused).
    cp "$SHIP_DLL" "$DLL_BAK" && cp "$SSAO_DIR/wired_vulkan_x86_64.dll" "$SHIP_DLL"
    RESTORE_DLL=1
    echo "    swapped dedicated FEAT_SSAO=1 DLL into $RUN_DIR (product DLL backed up)"
fi

echo
echo "==== STEP 1: capture + assert + bless-if-sane (mode=$MODE) ===="
SMOKE_UPDATE_GOLDEN=1 bash tests/visual-render-features.sh --mode "$MODE" --engine "$ENGINE"
BLESS_EXIT=$?
echo "==== bless exit: $BLESS_EXIT ===="
if [ "$BLESS_EXIT" -ne 0 ]; then
    echo "FAIL: the computed sanity assertions did not pass — goldens NOT blessed (a real bug; see numbers above)"
    exit 1
fi

echo
echo "==== STEP 2: cold re-verify against the just-blessed goldens ===="
bash tests/visual-render-features.sh --mode "$MODE" --engine "$ENGINE"
VERIFY_EXIT=$?
echo "==== verify exit: $VERIFY_EXIT ===="
exit $VERIFY_EXIT
