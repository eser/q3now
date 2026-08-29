#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Wired Engine contributors

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
# shellcheck source=tests/lib/wired_paths.sh
. "$SCRIPT_DIR/lib/wired_paths.sh"

ENGINE="${1:-${WIRED_BINARY:-}}"
Q3DIR="${Q3DIR:-$WIRED_INSTALL}"
WATCHDOG="$SCRIPT_DIR/run-with-timeout.py"
OUT="${LAVA_SANCTUM_CAMERA_OUT:-$REPO_ROOT/build/lava-sanctum-camera-probe}"

[ -x "$ENGINE" ] || { echo "FAIL: engine not executable: $ENGINE" >&2; exit 1; }
ENGINE="$(cd "$(dirname "$ENGINE")" && pwd)/$(basename "$ENGINE")"
[ -f "$WATCHDOG" ] || { echo "FAIL: watchdog missing: $WATCHDOG" >&2; exit 1; }

run_root="$(mktemp -d "$WIRED_TMP/q3now-lava-camera-XXXXXX")"
trap 'rm -rf "$run_root"' EXIT
WIRED_TMP="$run_root"
home="$(wired_isolated_home lava-camera)"
mkdir -p "$home/base/screenshots" "$OUT"
cfg="$home/base/lava-sanctum-camera-probe.cfg"
log="$OUT/vulkan.log"

printf '%s\n' \
  'map arena7' \
  'waitForMap' \
  'wait 60' \
  'cmd noclip' \
  'set cg_drawGun 0' \
  'set cg_thirdPerson 0' \
  'wait 20' \
  'cmd setviewpos 1400 -200 100 180 30' \
  'wait 60' \
  'screenshot lava_camera_a png silent' \
  'wait 30' \
  'cmd setviewpos 1300 -400 120 135 25' \
  'wait 60' \
  'screenshot lava_camera_b png silent' \
  'wait 30' \
  'cmd setviewpos 700 -800 -100 180 30' \
  'wait 60' \
  'screenshot lava_camera_c png silent' \
  'wait 30' \
  'cmd setviewpos 700 -400 -100 225 30' \
  'wait 60' \
  'screenshot lava_camera_d png silent' \
  'wait 30' \
  'cmd setviewpos 1250 -158 -150 180 35' \
  'wait 60' \
  'screenshot lava_camera_e png silent' \
  'wait 30' \
  'cmd setviewpos 1400 -500 0 135 35' \
  'wait 60' \
  'screenshot lava_camera_f png silent' \
  'wait 30' \
  'cmd setviewpos 1400 200 0 225 35' \
  'wait 60' \
  'screenshot lava_camera_g png silent' \
  'wait 30' \
  'cmd setviewpos 650 -500 -250 225 10' \
  'wait 60' \
  'screenshot lava_camera_h png silent' \
  'wait 30' \
  'cmd setviewpos 620 -700 -220 180 10' \
  'wait 60' \
  'screenshot lava_camera_i png silent' \
  'wait 30' \
  'cmd setviewpos 700 -300 -180 225 20' \
  'wait 60' \
  'screenshot lava_camera_j png silent' \
  'wait 30' \
  'quit' >"$cfg"

python3 "$WATCHDOG" --timeout 90 --kill-after 15 --cwd "$(dirname "$ENGINE")" --stdout "$log" -- \
  "$ENGINE" \
  +set fs_installpath "$Q3DIR" \
  +set fs_homepath "$home" \
  +set cl_renderer vulkan \
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
  +set r_renderScale 1 \
  +set r_pinShaderTime 1.0 \
  +set r_pinFrameTime 1.0 \
  +set con_notifytime 0 \
  +set cg_draw2D 0 \
  +exec lava-sanctum-camera-probe.cfg

for id in a b c d e f g h i j; do
  capture="$home/base/screenshots/lava_camera_${id}.png"
  [ -s "$capture" ] || { echo "FAIL: camera $id capture missing" >&2; tail -n 80 "$log"; exit 1; }
  cp "$capture" "$OUT/$id.png"
done
echo "PASS: 1280x720 lava-sanctum camera candidates captured: $OUT"
