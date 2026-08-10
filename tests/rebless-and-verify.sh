#!/usr/bin/env bash
# rebless-and-verify.sh — c2-golden-gate-rework completion.
# One bash invocation that (1) re-blesses goldens against the now-resolution-
# pinned harness, then (2) runs a verification cold-cache smoke. Sequential,
# no parallelism. Single call site so the harness only spawns one outer task.
set -u

PRODUCT_DIR="/c/Users/eser/wired/q3now-preview"
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
