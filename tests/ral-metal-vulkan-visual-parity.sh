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
RENDER_SCALE="${RAL_PARITY_RENDER_SCALE:-0}"
PIN_FRAME_TIME="${RAL_PARITY_PIN_FRAME_TIME:-5.0}"
BRIGHTNESS="${RAL_PARITY_BRIGHTNESS:-1}"
MATERIAL_COOK_DIR="${RAL_PARITY_MATERIAL_COOK_DIR:-}"
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
SKY_MODE="${RAL_PARITY_SKY_MODE:-}"
FILTER_RELOAD="${RAL_PARITY_FILTER_RELOAD:-0}"
REPEAT="${RAL_PARITY_REPEAT:-1}"
EXTRA_CFG="${RAL_PARITY_EXTRA_CFG:-}"
# Deterministic game clock: fixedtime advances cg.time by a constant per frame,
# so HUD timers, weather/sky timelines and game-time animations reach the same
# state at the capture frame regardless of wall-clock FPS (same-backend repeat
# determinism would otherwise track the presentation regime, not the renderer).
FIXED_TIME_MS="${RAL_PARITY_FIXED_TIME_MS:-8}"   # one optional console command for A/B experiments (recorded in the cfg)
case "$REPEAT" in 0|1) ;; *) echo "FAIL: invalid RAL_PARITY_REPEAT"; exit 1 ;; esac
case "$FILTER_RELOAD" in 0|1) ;; *) echo "FAIL: invalid RAL_PARITY_FILTER_RELOAD"; exit 1 ;; esac
case "$SKY_MODE" in
  ''|authored|physical|hybrid) ;;
  *) echo "FAIL: invalid RAL_PARITY_SKY_MODE: $SKY_MODE"; exit 1 ;;
esac
# An explicit sky experiment is retained evidence, not the default ROI gate.
if [ -n "$SKY_MODE" ] && [ "$ANALYZE" = 1 ]; then
  echo "FAIL: explicit sky-mode capture requires RAL_PARITY_ANALYZE=0"; exit 1
fi

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

# An explicit offline producer output is a test input, not a second cooker.
# Snapshot once for both backends; never link the writable player cache into
# a capture home. Runtime still validates project/package/artifact identities.
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
  local backend="$1" map="$2" x="$3" y="$4" z="$5" yaw="$6" pitch="${7:-0}" variant="${8:-}"
  local stem="$map${variant:+.$variant}"
  local home cfg log marker shot meta rc arg startup_commands=0
  local bloom_commands=()
  local sky_commands=()
  local filter_commands=()
  if [ "$FILTER_RELOAD" = 1 ]; then
    # Exercise material-pool resets with a live FX cohort, then replace the
    # world in the same process. Restore identical quality before capture.
    filter_commands=('map arena7' 'waitForMap' 'wait 60'
      'set r_textureMode GL_NEAREST' 'wait 30'
      'set r_textureMode GL_LINEAR_MIPMAP_LINEAR' 'wait 30')
  fi
  if [ -n "$SKY_MODE" ]; then
    sky_commands=("set g_envSkyMode $SKY_MODE" 'wait 120')
  fi
  local engine_args=()
  home="$(wired_isolated_home "$backend-$stem")"
  if [ -n "$MATERIAL_COOK_DIR" ]; then
    mkdir -p "$home/base/materials/cooked"
    cp "$RUN_ROOT/material-corpus/"* "$home/base/materials/cooked/"
  fi
  cfg="$home/base/ral-parity.cfg"
  log="$OUT/$backend/$stem.log"
  meta="$OUT/$backend/$stem.meta.json"
  marker="$(wired_shot_marker)"
  if [ -n "$BLOOM" ]; then
    bloom_commands=("set r_bloom $BLOOM" 'vid_restart' 'wait 60')
  fi
  printf '%s\n' \
    "${bloom_commands[@]}" \
	"set sv_cheats 1" \
	"set r_textureMode GL_LINEAR_MIPMAP_LINEAR" \
	"set r_ext_texture_filter_anisotropic 1" \
	"set r_ext_max_anisotropy 8" \
	"set g_spawnProtect 0" \
	"set r_brightness $BRIGHTNESS" \
	"set r_gamma $GAMMA" \
	"set r_lightmapBoost $LIGHTMAP_BOOST" \
	"set r_tonemap $TONEMAP" \
	"set fixedtime $FIXED_TIME_MS" \
	"set r_pinShaderTime 1.0" \
	"set r_pinFrameTime 0" \
	"set r_aa 0" \
	${EXTRA_CFG:+"$EXTRA_CFG"} \
	"set cg_drawGun $DRAW_GUN" \
	"set cg_lensFlare $CGAME_LENS" \
	"set r_lens $LENS" \
	"set r_dither 0" \
	"set r_chromaticAberration 0" \
	"set con_notifytime 0" \
	"set s_volume 0" \
	"set s_musicVolume 0" \
	"log renderer.init info" \
	"log renderer.ral ${RAL_PARITY_RAL_LOG_LEVEL:-info}" \
	"log renderer.hdr info" \
    "set r_flares $FLARES" \
    "set r_drawSky $DRAW_SKY" \
    "set cg_draw2D $DRAW_2D" \
	"set cg_powerupFlares $POWERUP_FLARES" \
	"set cg_halo $HALOS" \
    "${filter_commands[@]}" \
    "map $map" \
    'waitForMap' \
    "${sky_commands[@]}" \
    'wait 60' \
    'cmd noclip' \
    'wait 20' \
    "cmd setviewpos $x $y $z $yaw $pitch" \
    'wait 60' \
    "cmd setviewpos $x $y $z $yaw $pitch" \
    'wait 60' \
	"set r_pinFrameTime $PIN_FRAME_TIME" \
	'wait 30' \
	'cmd where' \
	"$([ "$backend" = vulkan ] && echo gfxinfo || echo 'echo RAL_METAL_NATIVE_SCENE')" \
    'log cgame info; viewpos' \
	'r_bloom; r_drawSunRays; r_flares; r_lens; cg_lensFlare; cg_powerupFlares; cg_halo' \
    "screenshot ral_parity_${map}_${backend} png silent" \
    'wait 30' \
    'quit' >"$cfg"

  echo "==> parity capture: $backend/$stem (1280x720)"
  engine_args=(
    +set fs_installpath "$Q3DIR"
    +set fs_homepath "$home"
    +set cl_renderer "$backend"
    +set net_port 0
    +set com_automated 1
    +set com_noHardReboot 1
    # Keep registration alive: disabling audio rejects sound-bearing WiredFX
    # profiles and removes their visual actions too. Mute in the cfg instead.
    +set s_initsound 1
    +set sv_pure 0
    +set vm_game 0
    +set vm_cgame 0
    +set r_fullscreen 0
    +set r_mode -1
    +set r_customwidth 1280
    +set r_customheight 720
    +set r_hdr 1
    +set r_hdrDisplay 0
    +set r_hdrAutoExposure 0
    +set r_renderScale "$RENDER_SCALE"
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
  cp "$shot" "$OUT/$backend/$stem.png"
  python3 - "$shot" "$meta" "$ENGINE_SHA" "$RUN_ID" "$backend" "$map" "$x" "$y" "$z" "$yaw" "$RENDER_WIDTH" "$RENDER_HEIGHT" "$PIN_FRAME_TIME" "$pitch" "$SKY_MODE" "$log" "$RENDER_SCALE" "$FILTER_RELOAD" <<'PY'
import json,sys
import struct
import re
shot,path,sha,run_id,backend,map_name,x,y,z,yaw,render_width,render_height,frame_time,pitch,sky_mode,log_path,render_scale,filter_reload=sys.argv[1:]
with open(shot,"rb") as stream:
  header=stream.read(24)
if len(header)!=24 or header[:8]!=b"\x89PNG\r\n\x1a\n" or header[12:16]!=b"IHDR":
  raise SystemExit(f"invalid PNG header: {shot}")
width,height=struct.unpack(">II",header[16:24])
log=open(log_path,encoding="utf-8",errors="replace").read()
samplers=re.findall(rf"Wired {backend} material sampler: min=(\d+) mag=(\d+) mip=(\d+) aniso=([\d.]+) maxLod=([\d.]+)",log)
if not samplers or tuple(map(float,samplers[-1])) != (1,1,1,8,0):
  raise SystemExit(f"{backend}: missing trilinear/aniso8 sampler evidence")
if filter_reload == "1":
  active=re.findall(r"CA_ACTIVE \(FIRST GAMEPLAY FRAME mapname=maps/(\w+)\.bsp",log)
  if active != ["arena7",map_name] or not any(tuple(map(float,s)) == (0,0,0,1,0.25) for s in samplers):
    raise SystemExit(f"{backend}: missing live filter-change / map-reload evidence")
if backend == "metal":
  formats=re.findall(r"Wired native Metal RAL: scene format=(\S+) extent=(\d+)x(\d+)",log)
  if not formats or formats[-1][0] != "R16G16B16A16_SFLOAT":
    raise SystemExit("missing Metal HDR scene evidence")
  actual_width,actual_height=map(int,formats[-1][1:])
else:
  if "VK_FORMAT_R16G16B16A16_SFLOAT" not in log:
    raise SystemExit("missing Vulkan HDR scene evidence")
  extents=re.findall(r"(?:RENDER:\s*(\d+) x (\d+),\s*)?MODE:\s*-?\d+,\s*(\d+) x (\d+)",log)
  if not extents: raise SystemExit("missing Vulkan scene extent evidence")
  rw,rh,dw,dh=extents[-1]
  actual_width,actual_height=int(rw or dw),int(rh or dh)
expected=(width,height) if float(render_scale)==0 else (int(render_width),int(render_height))
if (actual_width,actual_height)!=expected:
  raise SystemExit(f"{backend}: requested/actual render extent mismatch {expected} vs {(actual_width,actual_height)}")
data={
  "binary_sha256":sha,"run_id":run_id,"backend":backend,"map":map_name,
  "camera":[int(x),int(y),int(z),int(yaw)],"width":width,"height":height,
  "logical_width":1280,"logical_height":720,
  "render_scale":float(render_scale),"render_width":actual_width,"render_height":actual_height,
  "scene_format":"R16G16B16A16_SFLOAT",
  "filter_reload":filter_reload == "1",
  "color_policy":"sdr-srgb","warmup_frames":210+(120 if sky_mode else 0),
  "camera_pitch":int(pitch),"requested_sky_mode":sky_mode or "map-authored",
  "shader_time":1.0,"frame_time":float(frame_time)
}
open(path,"w",encoding="utf-8").write(json.dumps(data,sort_keys=True)+"\n")
PY
  # A screenshot and exit 0 are not proof of a published renderer world.
  # Retain diagnostics/images, but reject native material/lifecycle failures
  # even for capture-only experiments (not just the default ROI analysis).
  if grep -En '\[ERROR[[:space:]]*\]|disabling (particles|decals|ribbon/rail/beam)|(particles|decals) remain disabled|material resource rejected|material catalog rejected|material-runtime-catalog|world-material-catalog|present failure stage=|catalog (material|batch)=.*reason=' "$log"; then
    echo "FAIL: $backend/$map rejected renderer resources; artifacts retained in $OUT" >&2
    exit 1
  fi
}

for backend in $BACKENDS; do
  for map in $MAPS; do
    case "$map" in
      arena1) capture "$backend" arena1 1052 1432 50 135 ;;
      arena7) capture "$backend" arena7 -96 -1040 176 45 -25 ;;
      arena17) capture "$backend" arena17 488 1096 378 -90 ;;
      *) echo "FAIL: unsupported RAL_PARITY_MAPS entry: $map"; exit 1 ;;
    esac
    # Same-backend repeat capture in a fresh isolated home: pinned frame/shader
    # time must reproduce the frame, so the analyzer holds each backend to a
    # near-identity self-tolerance (N=1 determinism), independent of the
    # cross-backend parity tolerances.
    if [ "$REPEAT" = 1 ]; then
      case "$map" in
        arena1) capture "$backend" arena1 1052 1432 50 135 0 repeat ;;
        arena7) capture "$backend" arena7 -96 -1040 176 45 -25 repeat ;;
        arena17) capture "$backend" arena17 488 1096 378 -90 0 repeat ;;
      esac
    fi
  done
done

if [ "$ANALYZE" = 1 ]; then
  [ "$BACKENDS" = "metal vulkan" ] && [ "$MAPS" = "arena1 arena17" ] || {
    echo "FAIL: analysis requires the complete default backend/map matrix"; exit 1;
  }
  python3 "$ANALYZER" --artifacts "$OUT" --config "$CONFIG" --png2raw "$PNG2RAW"
fi
echo "PASS retained artifacts: $OUT"
