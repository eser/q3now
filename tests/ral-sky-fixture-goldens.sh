#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#
# TASK-239 AC#10: arena7 sky fixtures (clear noon, sunset, night, storm,
# authored legacy, hybrid) captured on the native macOS backends at the shared
# parity camera with pinned time and auto-exposure off.  Sun elevation is
# driven by a fixture shader override placed in the isolated fs_homepath
# (highest search-path priority) that redeclares arena7's sky material with a
# different q3map_sun; the shipped pak content is never modified.
#
#   bash tests/ral-sky-fixture-goldens.sh <engine> [fixture ...]
#   RAL_SKY_FIXTURE_OUT      output dir (default build/ral-sky-fixture-goldens)
#   RAL_SKY_FIXTURE_BACKENDS backends (default "metal vulkan")
#   RAL_SKY_FIXTURE_MATERIAL_COOK_DIR cooked material corpus for Release Metal
set -uo pipefail
REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"; cd "$REPO_ROOT"
source tests/lib/wired_paths.sh
ENGINE="${1:-}"; shift || true
[ -x "$ENGINE" ] || { echo "usage: $0 <engine> [fixture ...]"; exit 2; }
Q3DIR="${Q3DIR:-$(cd "$(dirname "$ENGINE")/../.." && pwd)}"
OUT="${RAL_SKY_FIXTURE_OUT:-$REPO_ROOT/build/ral-sky-fixture-goldens}"
BACKENDS="${RAL_SKY_FIXTURE_BACKENDS:-metal vulkan}"
COOK="${RAL_SKY_FIXTURE_MATERIAL_COOK_DIR:-$HOME/wired/q3now-preview/base/materials/cooked}"
FIXTURES="${*:-clear-noon sunset night storm authored-legacy hybrid}"
MAP=arena7; X=-96; Y=-1040; Z=176; YAW=45; PITCH=-25
# fixture -> sky mode, weather, sun elevation override (empty = authored q3map_sun)
fixture_params() {
  case "$1" in
    clear-noon)      echo "physical clear 85" ;;
    sunset)          echo "physical clear 6" ;;
    night)           echo "physical clear -12" ;;
    storm)           echo "physical storm" ;;
    authored-legacy) echo "authored clear" ;;
    hybrid)          echo "hybrid clear" ;;
    *) return 1 ;;
  esac
}
mkdir -p "$OUT"; status=0
for backend in $BACKENDS; do
  for fixture in $FIXTURES; do
    params="$(fixture_params "$fixture")" || { echo "FAIL: unknown fixture $fixture"; exit 2; }
    set -- $params; mode="$1"; weather="$2"; elevation="${3:-}"
    tag="sky-fixture-$backend-$fixture"
    home="$(wired_isolated_home "$tag")"
    mkdir -p "$home/base/materials/cooked" "$home/base/screenshots" "$home/base/scripts" "$OUT/$backend"
    [ -d "$COOK" ] && cp "$COOK"/* "$home/base/materials/cooked/" 2>/dev/null
    # Sun elevation reaches the physical sky through g_envSunElevation (serverinfo,
    # read by cgame); the BSP q3map_sun never fed the physical sky.
    cfg="$home/base/capture.cfg"
    printf '%s\n' 'set sv_cheats 1' 'set sv_pure 0' 'set g_spawnProtect 0' 'set fixedtime 8' 'set r_pinShaderTime 1.0' 'set r_pinFrameTime 0' \
      'set r_textureMode GL_LINEAR_MIPMAP_LINEAR' 'set r_ext_texture_filter_anisotropic 1' 'set r_ext_max_anisotropy 8' \
      "set g_envSkyMode $mode" "set g_envWeather $weather" "set g_envSunElevation $elevation" 'set r_particles 1' 'set r_atmosphericGPU 1' 'set cg_draw2D 0' 'set cg_drawGun 0' \
      'set s_volume 0' 'set con_notifytime 0' "map $MAP" 'waitForMap' 'wait 60' "set g_envSkyMode $mode" "set g_envWeather $weather" "set g_envSunElevation $elevation" 'wait 300' \
      'cmd noclip' 'wait 20' "cmd setviewpos $X $Y $Z $YAW $PITCH" 'wait 60' "cmd setviewpos $X $Y $Z $YAW $PITCH" 'wait 60' 'set r_pinFrameTime 1.0' 'wait 40' \
      'cvarlist r_skyPhysicalBlend' "screenshot ${tag} png silent" 'wait 30' 'quit' > "$cfg"
    python3 tests/run-with-timeout.py --timeout 170 --kill-after 15 --cwd "$(dirname "$ENGINE")" --stdout "$OUT/$backend/$fixture.stdout.log" -- "$ENGINE" \
      +set fs_installpath "$Q3DIR" +set fs_homepath "$home" +set cl_renderer "$backend" +set net_port 0 +set com_automated 1 +set com_noHardReboot 1 \
      +set s_initsound 1 +set vm_game 0 +set vm_cgame 0 +set r_fullscreen 0 +set r_mode -1 +set r_customwidth 1280 +set r_customheight 720 \
      +set r_renderScale 0 +set r_ext_supersample 0 +set r_fbo 1 +set r_hdr 1 +set r_hdrDisplay 0 +set r_hdrAutoExposure 0 +set r_bloom 1 +set r_bloomPasses 2 \
      +set r_bloomThresholdMode 0 +set r_bloomThreshold 0.32 +set r_bloomIntensity 0.5 +set r_dither 0 +set r_chromaticAberration 0 +exec capture.cfg >/dev/null 2>&1
    shot="$(ls -t "$home/base/screenshots/"*.png 2>/dev/null | head -1)"
    if [ -n "$shot" ]; then
      cp "$shot" "$OUT/$backend/$fixture.native.png"
      # Logical 1280x720 golden (box-averaged HiDPI native) via sips; the analyzer samples the native file.
      sips -Z 1280 "$shot" --out "$OUT/$backend/$fixture.png" >/dev/null 2>&1 || cp "$shot" "$OUT/$backend/$fixture.png"
      python3 - "$OUT/$backend/$fixture.meta.json" "$backend" "$fixture" "$mode" "$weather" "$elevation" "$MAP" "$X $Y $Z $YAW $PITCH" <<'PY'
import json,sys,datetime
p,backend,fixture,mode,weather,elev,m,cam=sys.argv[1:]
json.dump({"schemaVersion":1,"backend":backend,"fixture":fixture,"skyMode":mode,"weather":weather,
  "sunElevationOverride":(float(elev) if elev else None),"map":m,"camera":cam,"width":1280,"height":720,
  "capturedAt":datetime.datetime.now().isoformat(timespec="seconds"),"protocol":"fixedtime 8 (frame-count waits only), pinned shader/frame time, r_hdrAutoExposure 0, bloom 0.32/0.5"},open(p,"w"),indent=1)
PY
      echo "captured $backend/$fixture"
    else
      echo "FAIL: no screenshot for $backend/$fixture (see $OUT/$backend/$fixture.stdout.log)"; status=1
    fi
  done
done
exit $status
