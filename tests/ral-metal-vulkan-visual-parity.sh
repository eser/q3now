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
OUT="${RAL_PARITY_OUT:-$REPO_ROOT/build/ral-metal-vulkan-parity}"
PNG2RAW="${PNG2RAW:-$REPO_ROOT/tools/png2raw/png2raw}"
ANALYZER="$SCRIPT_DIR/ral-metal-vulkan-visual-parity-analyze.py"
CONFIG="$SCRIPT_DIR/ral-metal-vulkan-visual-parity.json"
WATCHDOG="$SCRIPT_DIR/run-with-timeout.py"
RUN_ID="$(date -u +%Y%m%dT%H%M%SZ)-$$"

[ "$(uname -s)" = Darwin ] || { echo "SKIP: Metal/Vulkan parity requires macOS"; exit 77; }
[ -x "$ENGINE" ] || { echo "FAIL: engine not executable: $ENGINE"; exit 1; }
[ -x "$PNG2RAW" ] || { echo "FAIL: png2raw missing: $PNG2RAW"; exit 1; }
[ -f "$ANALYZER" ] && [ -f "$CONFIG" ] && [ -f "$WATCHDOG" ] || {
  echo "FAIL: parity analyzer/config/watchdog missing"; exit 1;
}
for renderer in wired_metal_arm64.dylib wired_vulkan_arm64.dylib; do
  [ -f "$(dirname "$ENGINE")/$renderer" ] || { echo "FAIL: packaged renderer missing: $renderer"; exit 1; }
done

mkdir -p "$OUT/metal" "$OUT/vulkan"
touch "$OUT/run.marker"
RUN_ROOT="$(mktemp -d "$WIRED_TMP/q3now-ral-parity-XXXXXX")"
trap 'rm -rf "$RUN_ROOT"' EXIT
WIRED_TMP="$RUN_ROOT"
ENGINE_SHA="$(shasum -a 256 "$ENGINE" | awk '{print $1}')"

capture() {
  local backend="$1" map="$2" x="$3" y="$4" z="$5" yaw="$6"
  local home cfg log marker shot meta rc
  home="$(wired_isolated_home "$backend-$map")"
  cfg="$home/base/ral-parity.cfg"
  log="$OUT/$backend/$map.log"
  meta="$OUT/$backend/$map.meta.json"
  marker="$(wired_shot_marker)"
  printf '%s\n' \
    "map $map" \
    'waitForMap' \
    'wait 60' \
    'cmd noclip' \
    'wait 20' \
    "cmd setviewpos $x $y $z $yaw" \
    'wait 60' \
    "cmd setviewpos $x $y $z $yaw" \
    'wait 60' \
    "screenshot ral_parity_${map}_${backend} png silent" \
    'wait 30' \
    'quit' >"$cfg"

  echo "==> parity capture: $backend/$map (1280x720)"
  set +e
  python3 "$WATCHDOG" --timeout 120 --kill-after 15 --cwd "$(dirname "$ENGINE")" --stdout "$log" -- \
    "$ENGINE" \
    +set fs_installpath "$Q3DIR" \
    +set fs_homepath "$home" \
    +set cl_renderer "$backend" \
    +set net_port 0 \
    +set com_automated 1 \
    +set com_noHardReboot 1 \
    +set s_initsound 0 \
    +set sv_pure 0 \
    +set sv_cheats 1 \
    +set vm_game 0 \
    +set vm_cgame 0 \
    +set r_fullscreen 0 \
    +set r_mode -1 \
    +set r_customwidth 1280 \
    +set r_customheight 720 \
    +set r_hdr 0 \
    +set r_hdrDisplay 0 \
    +set r_hdrAutoExposure 0 \
    +set r_renderScale 1 \
    +set r_pinShaderTime 1.0 \
    +set r_pinFrameTime 1.0 \
    +set r_dither 0 \
    +set r_chromaticAberration 0 \
    +set con_notifytime 0 \
    +log renderer.init info \
    +exec ral-parity.cfg
  rc=$?
  set -e
  if [ "$rc" -ne 0 ]; then
    echo "FAIL: $backend/$map exited with status $rc"; tail -n 80 "$log"; exit 1
  fi
  shot="$(wired_newest_shot "$marker" "$home/base/screenshots")" || {
    echo "FAIL: $backend/$map produced no fresh screenshot"; tail -n 80 "$log"; exit 1;
  }
  cp "$shot" "$OUT/$backend/$map.png"
  python3 - "$meta" "$ENGINE_SHA" "$RUN_ID" "$backend" "$map" "$x" "$y" "$z" "$yaw" <<'PY'
import json,sys
path,sha,run_id,backend,map_name,x,y,z,yaw=sys.argv[1:]
data={
  "binary_sha256":sha,"run_id":run_id,"backend":backend,"map":map_name,
  "camera":[int(x),int(y),int(z),int(yaw)],"width":1280,"height":720,
  "render_scale":1,"color_policy":"sdr-srgb","warmup_frames":180,
  "shader_time":1.0,"frame_time":1.0
}
open(path,"w",encoding="utf-8").write(json.dumps(data,sort_keys=True)+"\n")
PY
}

for backend in metal vulkan; do
  capture "$backend" arena1 1052 1432 50 135
  capture "$backend" arena17 488 1096 378 -90
done

python3 "$ANALYZER" --artifacts "$OUT" --config "$CONFIG" --png2raw "$PNG2RAW"
echo "PASS retained artifacts: $OUT"
