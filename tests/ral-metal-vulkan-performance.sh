#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
# shellcheck source=tests/lib/wired_paths.sh
. "$SCRIPT_DIR/lib/wired_paths.sh"

ENGINE="${1:-${WIRED_BINARY:-}}"
Q3DIR="${Q3DIR:-$WIRED_INSTALL}"
OUT="${RAL_BACKEND_PERF_OUT:-$REPO_ROOT/build/ral-metal-vulkan-performance}"
MATERIAL_COOK_DIR="${RAL_BACKEND_PERF_MATERIAL_COOK_DIR:-}"
ANALYZER="$SCRIPT_DIR/ral-metal-vulkan-performance-analyze.py"
CONFIG="$SCRIPT_DIR/ral-metal-vulkan-performance.json"
WATCHDOG="$SCRIPT_DIR/run-with-timeout.py"
RUN_ID="$(date -u +%Y%m%dT%H%M%SZ)-$$"
WARMUP_FRAMES=300
MEASURE_FRAMES=1200
AUTOMATED="${RAL_BACKEND_PERF_AUTOMATED:-1}"
case "$AUTOMATED" in 0|1) ;; *) echo "FAIL: RAL_BACKEND_PERF_AUTOMATED must be 0 or 1"; exit 1;; esac

[ "$(uname -s)" = Darwin ] || { echo "SKIP: Metal/MoltenVK benchmark requires macOS"; exit 77; }
[ -x "$ENGINE" ] || { echo "FAIL: engine not executable: $ENGINE"; exit 1; }
[ -f "$ANALYZER" ] && [ -f "$CONFIG" ] && [ -f "$WATCHDOG" ] || {
  echo "FAIL: performance analyzer/config/watchdog missing"; exit 1;
}
for renderer in wired_metal_arm64.dylib wired_vulkan_arm64.dylib; do
  [ -f "$(dirname "$ENGINE")/$renderer" ] || { echo "FAIL: packaged renderer missing: $renderer"; exit 1; }
done

REPETITIONS="$(python3 -c 'import json,sys; n=json.load(open(sys.argv[1]))["repetitions"]; assert type(n) is int and 1 <= n <= 4; print(n)' "$CONFIG")"

mkdir -p "$OUT/metal" "$OUT/vulkan"
touch "$OUT/run.marker"
RUN_ROOT="$(mktemp -d "$WIRED_TMP/q3now-ral-backend-perf-XXXXXX")"
trap 'rm -rf "$RUN_ROOT"' EXIT
WIRED_TMP="$RUN_ROOT"
ENGINE_SHA="$(shasum -a 256 "$ENGINE" | awk '{print $1}')"

# The same immutable offline corpus must reach every isolated capture home.
# Do not regenerate content or expose the player's writable cache to a run.
if [ -n "$MATERIAL_COOK_DIR" ]; then
  [ -d "$MATERIAL_COOK_DIR" ] || {
    echo "FAIL: material cook directory missing: $MATERIAL_COOK_DIR"; exit 1;
  }
  mkdir -p "$RUN_ROOT/material-corpus"
  for extension in wrmat wrmatpkg wrmatref; do
    count=0
    for product in "$MATERIAL_COOK_DIR"/*."$extension"; do
      [ -f "$product" ] || continue
      cp "$product" "$RUN_ROOT/material-corpus/"
      count=$((count + 1))
    done
    [ "$count" -gt 0 ] || {
      echo "FAIL: incomplete material corpus: no .$extension products"; exit 1;
    }
  done
  (cd "$RUN_ROOT/material-corpus" && shasum -a 256 ./*) > "$OUT/material-corpus.sha256"
fi

capture() {
  local backend="$1" map="$2" repetition="$3" x="$4" y="$5" z="$6" yaw="$7"
  local home cfg stdout qconsole meta renderer renderer_sha rc diagnostic
  diagnostic="echo RAL_BACKEND_NATIVE_SCENE"
  [ "$backend" != vulkan ] || diagnostic="gfxinfo"
  home="$(wired_isolated_home "$backend-$map")"
  if [ -n "$MATERIAL_COOK_DIR" ]; then
    mkdir -p "$home/base/materials/cooked"
    cp "$RUN_ROOT/material-corpus/"* "$home/base/materials/cooked/"
  fi
  cfg="$home/base/ral-backend-perf.cfg"
  stdout="$OUT/$backend/$map-r$repetition.stdout.log"
  qconsole="$OUT/$backend/$map-r$repetition.qconsole.jsonl"
  meta="$OUT/$backend/$map-r$repetition.meta.json"
  renderer="$(dirname "$ENGINE")/wired_${backend}_arm64.dylib"
  renderer_sha="$(shasum -a 256 "$renderer" | awk '{print $1}')"
  # Pin valid limiter values and query them around measurement. Minimized=0
  # is invalid (range 1..30); analyzer rejects any minimized measurement frame.
  printf '%s\n' \
    'set sv_cheats 1' \
    'set sv_pure 0' \
    'set com_maxfps 0' \
    'set com_maxfpsUnfocused 1000' \
    'set com_maxfpsMinimized 30' \
    'set r_swapInterval 0' \
    'set r_textureMode GL_LINEAR_MIPMAP_LINEAR' \
    'set r_ext_texture_filter_anisotropic 1' \
    'set r_ext_max_anisotropy 8' \
    "map $map" \
    'waitForMap' \
    'wait 80' \
    'cmd noclip' \
    'wait 20' \
    "cmd setviewpos $x $y $z $yaw" \
    'wait 80' \
    "cmd setviewpos $x $y $z $yaw" \
    'set r_pinShaderTime 1.0' \
    'set r_pinFrameTime 1.0' \
    "wait $WARMUP_FRAMES" \
    'log renderer.hdr info' \
    "$diagnostic" \
    'log renderer.init info' \
    'log renderer.ral info' \
    'log renderer.timing debug' \
    'set r_gpuSpeeds 1' \
    'set r_vkDebugTiming 0' \
    'wait 1' \
    'set r_vkDebugTiming 1' \
    'set com_perfTrace 1' \
    'com_maxfps; com_maxfpsUnfocused; com_maxfpsMinimized' \
    'r_bloom; r_bloomPasses; r_bloomThresholdMode; r_bloomThreshold; r_bloomIntensity' \
    'echo RAL_BACKEND_PERF_BEGIN' \
    "wait $MEASURE_FRAMES" \
    'echo RAL_BACKEND_PERF_END' \
    'com_maxfps; com_maxfpsUnfocused; com_maxfpsMinimized' \
    'r_bloom; r_bloomPasses; r_bloomThresholdMode; r_bloomThreshold; r_bloomIntensity' \
    'set com_perfTrace 0' \
    'set r_gpuSpeeds 0' \
    'set r_vkDebugTiming 0' \
    'wait 2' \
    'quit' >"$cfg"

  echo "==> Release backend benchmark: $backend/$map repetition=$repetition (1280x720 logical, swapInterval=0)"
  set +e
  python3 "$WATCHDOG" --timeout 90 --kill-after 15 --cwd "$(dirname "$ENGINE")" --stdout "$stdout" -- \
    "$ENGINE" \
    +set fs_installpath "$Q3DIR" \
    +set fs_homepath "$home" \
    +set cl_renderer "$backend" \
    +set net_port 0 \
    +set com_automated "$AUTOMATED" \
    +set com_noHardReboot 1 \
    +set s_initsound 1 \
    +set vm_game 0 \
    +set vm_cgame 0 \
    +set log_file_mode overwrite_synced \
    +set log_file_severity DEBUG \
    +log renderer.init info \
    +log renderer.ral info \
    +set r_fullscreen 0 \
    +set r_mode -1 \
    +set r_customwidth 1280 \
    +set r_customheight 720 \
    +set r_renderScale 0 \
    +set r_ext_supersample 0 \
    +set r_fbo 1 \
    +set r_hdr 1 \
    +set r_hdrDisplay 0 \
    +set r_hdrAutoExposure 0 \
    +set r_bloom 1 \
    +set r_bloomPasses 2 \
    +set r_bloomThresholdMode 0 \
    +set r_bloomThreshold 0.32 \
    +set r_bloomIntensity 0.5 \
    +set r_dither 0 \
    +set r_chromaticAberration 0 \
    +exec ral-backend-perf.cfg
  rc=$?
  set -e
  # Preserve diagnostics before the isolated-home cleanup, even on native crash.
  if [ -s "$home/qconsole.jsonl" ]; then cp "$home/qconsole.jsonl" "$qconsole"; fi
  if [ "$rc" -ne 0 ]; then
    echo "FAIL: $backend/$map exited with status $rc"; tail -n 80 "$stdout"; exit 1
  fi
  [ -s "$home/qconsole.jsonl" ] || { echo "FAIL: $backend/$map qconsole missing"; exit 1; }
  cp "$home/qconsole.jsonl" "$qconsole"
  python3 - "$meta" "$ENGINE_SHA" "$renderer_sha" "$RUN_ID" "$backend" "$map" "$repetition" "$x" "$y" "$z" "$yaw" "$AUTOMATED" <<'PY'
import json,sys
path,engine_sha,renderer_sha,run_id,backend,map_name,repetition,x,y,z,yaw,automated=sys.argv[1:]
data={
  "engine_sha256":engine_sha,"renderer_sha256":renderer_sha,"run_id":run_id,
  "backend":backend,"map":map_name,"repetition":int(repetition),
  "automated":int(automated),
  "camera":[int(x),int(y),int(z),int(yaw)],
  "build_profile":"Release","width":1280,"height":720,
  "legacy_render_scale":0,"render_extent_policy":"native-presentation",
  "swap_interval":0,"warmup_frames":300,"measurement_frames":1200,
  "scene_format":"R16G16B16A16_SFLOAT"
}
open(path,"w",encoding="utf-8").write(json.dumps(data,sort_keys=True)+"\n")
PY
}

# Reverse the paired order on alternate repetitions. All repetitions share
# one engine/corpus and freshness epoch; the analyzer rejects missing runs.
for (( repetition=1; repetition<=REPETITIONS; repetition++ )); do
  if (( repetition % 2 )); then
    capture metal  arena1  "$repetition" 1052 1432 50 135
    capture vulkan arena1  "$repetition" 1052 1432 50 135
    capture vulkan arena17 "$repetition" 488 1096 378 -90
    capture metal  arena17 "$repetition" 488 1096 378 -90
  else
    capture vulkan arena1  "$repetition" 1052 1432 50 135
    capture metal  arena1  "$repetition" 1052 1432 50 135
    capture metal  arena17 "$repetition" 488 1096 378 -90
    capture vulkan arena17 "$repetition" 488 1096 378 -90
  fi
done

python3 "$ANALYZER" --artifacts "$OUT" --config "$CONFIG"
echo "PASS retained artifacts: $OUT"
