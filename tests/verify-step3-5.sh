#!/usr/bin/env bash
# verify-step3-5.sh — one-shot sequential verification of
# legacy-mainpath-retire STEPS 3/4/5.
#
# Runs the smoke-map-transition harness ONCE with a cold pipeline cache and
# reports the verdict. No background tasks, no parallelism — strictly
# sequential. Single call site.
set -u
. "$(cd "$(dirname "$0")" && pwd)/lib/wired_paths.sh"

# Scratch home: this script only clears a stale pipeline cache before
# delegating to smoke-map-transition.sh, which runs in its own isolated
# home — so there is no reason for the deletion to land in the player's.
PRODUCT_DIR="${PRODUCT_DIR:-$( wired_isolated_home verify-step3-5-home )}"
WIRED="build/wired.x64.exe"

echo "=== Cold-cache state ==="
rm -f "$PRODUCT_DIR/base/pipelinecache_v1_vulkan.bin"
find "$PRODUCT_DIR/base/screenshots" -name '*.png' -delete 2>/dev/null || true
echo "  pipeline cache: removed"
echo "  screenshot dir: cleared"

echo
echo "=== Source-grep gates ==="
echo "SHADER_MODULE_BL active refs (want 0):"
grep -rn "SHADER_MODULE_BL" code/render/ral/backends/vulkan/renderer/ 2>&1 \
  | grep -v "// \|retired\|retire" \
  | grep -c "SHADER_MODULE_BL" || true
echo "vk.useBindlessMainPath active refs (want 0):"
grep -rn "vk\.useBindlessMainPath" code/render/ral/backends/vulkan/renderer/ 2>&1 \
  | grep -vE "^[^:]+: *//|^[^:]+: *\* |legacy-mainpath-retire" \
  | grep -c useBindlessMainPath || true
echo "r_bindlessMainPath active refs in source (want 0):"
grep -rn "r_bindlessMainPath" code/render/ral/backends/vulkan/renderer/ 2>&1 \
  | grep -vE "^[^:]+: *//|legacy-mainpath-retire" \
  | grep -c r_bindlessMainPath || true
echo "qboolean useBindlessMainPath field (want 0):"
grep -c "qboolean useBindlessMainPath" code/render/ral/backends/vulkan/renderer/ -r 2>&1 || true
echo "USE_BINDLESS in templates (want 0):"
grep -c USE_BINDLESS code/render/ral/backends/vulkan/renderer/shaders/gen_frag.tmpl code/render/ral/backends/vulkan/renderer/shaders/light_frag.tmpl 2>&1 || true
echo "Legacy non-_bindless gen_frag/light_frag manifest entries (want 0):"
grep -nE "stage: 'frag', source: '(gen_frag|light_frag)\.tmpl'" \
  code/render/ral/backends/vulkan/renderer/shaders/shaders.manifest.mjs \
  | grep -vc _bindless || true

echo
echo "=== Cold-cache smoke run ==="
bash tests/smoke-map-transition.sh "$WIRED"
SMOKE_EXIT=$?
echo
echo "=== Smoke exit code: $SMOKE_EXIT ==="
exit $SMOKE_EXIT
