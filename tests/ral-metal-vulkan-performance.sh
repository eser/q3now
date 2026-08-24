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
ANALYZER="$SCRIPT_DIR/ral-metal-vulkan-performance-analyze.py"
CONFIG="$SCRIPT_DIR/ral-metal-vulkan-performance.json"
WATCHDOG="$SCRIPT_DIR/run-with-timeout.py"
RUN_ID="$(date -u +%Y%m%dT%H%M%SZ)-$$"
WARMUP_FRAMES=300
MEASURE_FRAMES=1200

[ "$(uname -s)" = Darwin ] || { echo "SKIP: Metal/MoltenVK benchmark requires macOS"; exit 77; }
[ -x "$ENGINE" ] || { echo "FAIL: engine not executable: $ENGINE"; exit 1; }
[ -f "$ANALYZER" ] && [ -f "$CONFIG" ] && [ -f "$WATCHDOG" ] || {
  echo "FAIL: performance analyzer/config/watchdog missing"; exit 1;
}
for renderer in wired_metal_arm64.dylib wired_vulkan_arm64.dylib; do
  [ -f "$(dirname "$ENGINE")/$renderer" ] || { echo "FAIL: packaged renderer missing: $renderer"; exit 1; }
done

mkdir -p "$OUT/metal" "$OUT/vulkan"
touch "$OUT/run.marker"
RUN_ROOT="$(mktemp -d "$WIRED_TMP/q3now-ral-backend-perf-XXXXXX")"
trap 'rm -rf "$RUN_ROOT"' EXIT
WIRED_TMP="$RUN_ROOT"
ENGINE_SHA="$(shasum -a 256 "$ENGINE" | awk '{print $1}')"

capture() {
  local backend="$1" map="$2" repetition="$3" x="$4" y="$5" z="$6" yaw="$7"
  local home cfg stdout qconsole meta renderer renderer_sha rc
  home="$(wired_isolated_home "$backend-$map")"
  cfg="$home/base/ral-backend-perf.cfg"
  stdout="$OUT/$backend/$map-r$repetition.stdout.log"
  qconsole="$OUT/$backend/$map-r$repetition.qconsole.jsonl"
  meta="$OUT/$backend/$map-r$repetition.meta.json"
  renderer="$(dirname "$ENGINE")/wired_${backend}_arm64.dylib"
  renderer_sha="$(shasum -a 256 "$renderer" | awk '{print $1}')"
  printf '%s\n' \
    'set sv_cheats 1' \
    'set sv_pure 0' \
    'set com_maxfps 0' \
    'set com_maxfpsUnfocused 0' \
    'set com_maxfpsMinimized 0' \
    'set r_swapInterval 0' \
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
    'log renderer.init info' \
    'log renderer.ral info' \
    'log renderer.timing debug' \
    'set r_gpuSpeeds 1' \
    'set r_vkDebugTiming 0' \
    'wait 1' \
    'set r_vkDebugTiming 1' \
    'set com_perfTrace 1' \
    'echo RAL_BACKEND_PERF_BEGIN' \
    "wait $MEASURE_FRAMES" \
    'echo RAL_BACKEND_PERF_END' \
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
    +set com_automated 1 \
    +set com_noHardReboot 1 \
    +set s_initsound 0 \
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
    +set r_renderScale 1 \
    +set r_hdr 0 \
    +set r_hdrDisplay 0 \
    +set r_hdrAutoExposure 0 \
    +set r_dither 0 \
    +set r_chromaticAberration 0 \
    +exec ral-backend-perf.cfg
  rc=$?
  set -e
  if [ "$rc" -ne 0 ]; then
    echo "FAIL: $backend/$map exited with status $rc"; tail -n 80 "$stdout"; exit 1
  fi
  [ -s "$home/qconsole.jsonl" ] || { echo "FAIL: $backend/$map qconsole missing"; exit 1; }
  cp "$home/qconsole.jsonl" "$qconsole"
  python3 - "$meta" "$ENGINE_SHA" "$renderer_sha" "$RUN_ID" "$backend" "$map" "$repetition" "$x" "$y" "$z" "$yaw" <<'PY'
import json,sys
path,engine_sha,renderer_sha,run_id,backend,map_name,repetition,x,y,z,yaw=sys.argv[1:]
data={
  "engine_sha256":engine_sha,"renderer_sha256":renderer_sha,"run_id":run_id,
  "backend":backend,"map":map_name,"repetition":int(repetition),
  "camera":[int(x),int(y),int(z),int(yaw)],
  "build_profile":"Release","width":1280,"height":720,"render_scale":1,
  "swap_interval":0,"warmup_frames":300,"measurement_frames":1200
}
open(path,"w",encoding="utf-8").write(json.dumps(data,sort_keys=True)+"\n")
PY
}

# Counterbalanced M/V/V/M scene ordering limits backend-order bias without
# repeatedly tearing down eight independent Metal/MoltenVK process owners.
capture metal  arena1  1 1052 1432 50 135
capture vulkan arena1  1 1052 1432 50 135
capture vulkan arena17 1 488 1096 378 -90
capture metal  arena17 1 488 1096 378 -90

python3 "$ANALYZER" --artifacts "$OUT" --config "$CONFIG"
echo "PASS retained artifacts: $OUT"
