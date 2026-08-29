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
BACKENDS="${RAL_PARITY_BACKENDS:-metal vulkan}"
MAPS="${RAL_PARITY_MAPS:-arena1 arena17}"
RENDER_WIDTH="${RAL_PARITY_RENDER_WIDTH:-1280}"
RENDER_HEIGHT="${RAL_PARITY_RENDER_HEIGHT:-720}"
PIN_FRAME_TIME="${RAL_PARITY_PIN_FRAME_TIME:-5.0}"
BRIGHTNESS="${RAL_PARITY_BRIGHTNESS:-1}"
GAMMA="${RAL_PARITY_GAMMA:-1}"
LIGHTMAP_BOOST="${RAL_PARITY_LIGHTMAP_BOOST:-4.6}"
TONEMAP="${RAL_PARITY_TONEMAP:-3}"
ANALYZE="${RAL_PARITY_ANALYZE:-1}"
DRAW_GUN="${RAL_PARITY_DRAW_GUN:-1}"
LENS="${RAL_PARITY_LENS:-1}"
CGAME_LENS="${RAL_PARITY_CGAME_LENS:-1}"
POWERUP_FLARES="${RAL_PARITY_POWERUP_FLARES:-0}"
HALOS="${RAL_PARITY_HALOS:-0}"
BLOOM="${RAL_PARITY_BLOOM:-}"
FLARES="${RAL_PARITY_FLARES:-0}"
DRAW_SKY="${RAL_PARITY_DRAW_SKY:-1}"
DRAW_2D="${RAL_PARITY_DRAW_2D:-1}"

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
  local home cfg log marker shot meta rc arg startup_commands=0
  local bloom_commands=()
  local engine_args=()
  home="$(wired_isolated_home "$backend-$map")"
  cfg="$home/base/ral-parity.cfg"
  log="$OUT/$backend/$map.log"
  meta="$OUT/$backend/$map.meta.json"
  marker="$(wired_shot_marker)"
  if [ -n "$BLOOM" ]; then
    bloom_commands=("set r_bloom $BLOOM" 'vid_restart' 'wait 60')
  fi
  printf '%s\n' \
    "${bloom_commands[@]}" \
	"set sv_cheats 1" \
	"set g_spawnProtect 0" \
	"set r_brightness $BRIGHTNESS" \
	"set r_gamma $GAMMA" \
	"set r_lightmapBoost $LIGHTMAP_BOOST" \
	"set r_tonemap $TONEMAP" \
	"set r_pinShaderTime 1.0" \
	"set r_pinFrameTime 0" \
	"set cg_drawGun $DRAW_GUN" \
	"set cg_lensFlare $CGAME_LENS" \
	"set r_lens $LENS" \
	"set r_dither 0" \
	"set r_chromaticAberration 0" \
	"set con_notifytime 0" \
	"log renderer.init info" \
    "set r_flares $FLARES" \
    "set r_drawSky $DRAW_SKY" \
    "set cg_draw2D $DRAW_2D" \
	"set cg_powerupFlares $POWERUP_FLARES" \
	"set cg_halo $HALOS" \
    "map $map" \
    'waitForMap' \
    'wait 60' \
    'cmd noclip' \
    'wait 20' \
    "cmd setviewpos $x $y $z $yaw" \
    'wait 60' \
    "cmd setviewpos $x $y $z $yaw" \
    'wait 60' \
	"set r_pinFrameTime $PIN_FRAME_TIME" \
	'wait 30' \
	'cmd where' \
    'log cgame info; viewpos' \
	'r_bloom; r_drawSunRays; r_flares; r_lens; cg_lensFlare; cg_powerupFlares; cg_halo' \
    "screenshot ral_parity_${map}_${backend} png silent" \
    'wait 30' \
    'quit' >"$cfg"

  echo "==> parity capture: $backend/$map (1280x720)"
  engine_args=(
    +set fs_installpath "$Q3DIR"
    +set fs_homepath "$home"
    +set cl_renderer "$backend"
    +set net_port 0
    +set com_automated 1
    +set com_noHardReboot 1
    +set s_initsound 0
    +set sv_pure 0
    +set vm_game 0
    +set vm_cgame 0
    +set r_fullscreen 0
    +set r_mode -1
    +set r_customwidth 1280
    +set r_customheight 720
    +set r_hdr 0
    +set r_hdrDisplay 0
    +set r_hdrAutoExposure 0
    +set r_renderScale 1
    +set r_renderWidth "$RENDER_WIDTH"
    +set r_renderHeight "$RENDER_HEIGHT"
    +exec ral-parity.cfg
  )
  # Com_ParseCommandLine owns a fixed 32-entry +command table.  Count the
  # actual argv recipe so future parity controls fail here instead of silently
  # truncating the trailing +exec and idling at the menu until the watchdog.
  for arg in "${engine_args[@]}"; do
    case "$arg" in +*) startup_commands=$(( startup_commands + 1 ));; esac
  done
  [ "$startup_commands" -le 32 ] || {
    echo "FAIL: parity startup uses $startup_commands +commands (engine limit 32)" >&2
    exit 1
  }
  set +e
  python3 "$WATCHDOG" --timeout 120 --kill-after 15 --cwd "$(dirname "$ENGINE")" --stdout "$log" -- \
    "$ENGINE" "${engine_args[@]}"
  rc=$?
  set -e
  if [ "$rc" -ne 0 ]; then
    echo "FAIL: $backend/$map exited with status $rc"; tail -n 80 "$log"; exit 1
  fi
  shot="$(wired_newest_shot "$marker" "$home/base/screenshots")" || {
    echo "FAIL: $backend/$map produced no fresh screenshot"; tail -n 80 "$log"; exit 1;
  }
  cp "$shot" "$OUT/$backend/$map.png"
  python3 - "$shot" "$meta" "$ENGINE_SHA" "$RUN_ID" "$backend" "$map" "$x" "$y" "$z" "$yaw" "$RENDER_WIDTH" "$RENDER_HEIGHT" "$PIN_FRAME_TIME" <<'PY'
import json,sys
import struct
shot,path,sha,run_id,backend,map_name,x,y,z,yaw,render_width,render_height,frame_time=sys.argv[1:]
with open(shot,"rb") as stream:
  header=stream.read(24)
if len(header)!=24 or header[:8]!=b"\x89PNG\r\n\x1a\n" or header[12:16]!=b"IHDR":
  raise SystemExit(f"invalid PNG header: {shot}")
width,height=struct.unpack(">II",header[16:24])
data={
  "binary_sha256":sha,"run_id":run_id,"backend":backend,"map":map_name,
  "camera":[int(x),int(y),int(z),int(yaw)],"width":width,"height":height,
  "logical_width":1280,"logical_height":720,
  "render_scale":1,"render_width":int(render_width),"render_height":int(render_height),
  "color_policy":"sdr-srgb","warmup_frames":210,
  "shader_time":1.0,"frame_time":float(frame_time)
}
open(path,"w",encoding="utf-8").write(json.dumps(data,sort_keys=True)+"\n")
PY
}

for backend in $BACKENDS; do
  for map in $MAPS; do
    case "$map" in
      arena1) capture "$backend" arena1 1052 1432 50 135 ;;
      arena17) capture "$backend" arena17 488 1096 378 -90 ;;
      *) echo "FAIL: unsupported RAL_PARITY_MAPS entry: $map"; exit 1 ;;
    esac
  done
done

if [ "$ANALYZE" = 1 ]; then
  [ "$BACKENDS" = "metal vulkan" ] && [ "$MAPS" = "arena1 arena17" ] || {
    echo "FAIL: analysis requires the complete default backend/map matrix"; exit 1;
  }
  python3 "$ANALYZER" --artifacts "$OUT" --config "$CONFIG" --png2raw "$PNG2RAW"
fi
echo "PASS retained artifacts: $OUT"
