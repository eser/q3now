#!/usr/bin/env bash
# rebless-and-verify.sh — c2-golden-gate-rework completion.
# One bash invocation that (1) re-blesses goldens against the now-resolution-
# pinned harness, then (2) runs a verification cold-cache smoke. Sequential,
# no parallelism. Single call site so the harness only spawns one outer task.
set -u
. "$(cd "$(dirname "$0")" && pwd)/lib/wired_paths.sh"

# Scratch home: this script only clears a stale pipeline cache before
# delegating to smoke-map-transition.sh, which runs in its own isolated
# home — so there is no reason for the deletion to land in the player's.
PRODUCT_DIR="${PRODUCT_DIR:-$( wired_isolated_home rebless-and-verify-home )}"
WIRED="build/debug/wired.x64.exe"

echo "==== STEP 1: re-bless goldens (SMOKE_UPDATE_GOLDEN=1, cold cache) ===="
rm -f "$PRODUCT_DIR/base/pipelinecache_v1_vulkan.bin"
find "$PRODUCT_DIR/base/screenshots" -name '*.png' -delete 2>/dev/null || true
SMOKE_UPDATE_GOLDEN=1 bash tests/smoke-map-transition.sh "$WIRED" 2>&1
REBLESS_EXIT=$?
echo "==== rebless exit: $REBLESS_EXIT ===="

echo
echo "==== STEP 2: verification smoke (cold cache) ===="
rm -f "$PRODUCT_DIR/base/pipelinecache_v1_vulkan.bin"
find "$PRODUCT_DIR/base/screenshots" -name '*.png' -delete 2>/dev/null || true
bash tests/smoke-map-transition.sh "$WIRED" 2>&1
VERIFY_EXIT=$?
echo "==== verify exit: $VERIFY_EXIT ===="

exit $VERIFY_EXIT
