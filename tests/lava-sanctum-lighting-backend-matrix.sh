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
OUT="${LAVA_SANCTUM_MATRIX_OUT:-$REPO_ROOT/build/lava-sanctum-lighting-matrix}"
WATCHDOG="$SCRIPT_DIR/run-with-timeout.py"
FIXTURE_WRITER="$REPO_ROOT/build/render_submission_test"
PNG2RAW="${PNG2RAW:-$REPO_ROOT/tools/png2raw/png2raw}"

[ -x "$ENGINE" ] || { echo "FAIL: engine not executable: $ENGINE" >&2; exit 1; }
[ -x "$FIXTURE_WRITER" ] || { echo "FAIL: fixture writer missing" >&2; exit 1; }
[ -x "$PNG2RAW" ] || { echo "FAIL: png2raw missing" >&2; exit 1; }
ENGINE="$(cd "$(dirname "$ENGINE")" && pwd)/$(basename "$ENGINE")"

run_root="$(mktemp -d "$WIRED_TMP/q3now-lava-matrix-XXXXXX")"
trap 'rm -rf "$run_root"' EXIT
WIRED_TMP="$run_root"
mkdir -p "$OUT"
module_app="$run_root/renderer.app"
mkdir -p "$module_app/Contents/MacOS" "$module_app/Contents/Resources"
ln -s "$WIRED_GAMEDATA" "$module_app/Contents/Resources/base"
for module in "$(dirname "$ENGINE")"/wired_*_arm64.dylib; do
  [ -f "$module" ] && ln -s "$module" "$module_app/Contents/MacOS/$(basename "$module")"
done
[ -f "$WIRED_BINDIR/libMoltenVK.dylib" ] && \
  ln -s "$WIRED_BINDIR/libMoltenVK.dylib" "$module_app/Contents/MacOS/libMoltenVK.dylib"

capture_native() {
  local backend="$1" home cfg log marker shot renderer
  renderer="$(dirname "$ENGINE")/wired_${backend}_arm64.dylib"
  [ -f "$renderer" ] || { echo "FAIL: packaged $backend renderer missing: $renderer" >&2; exit 1; }
  home="$(wired_isolated_home "lava-matrix-$backend")"
  mkdir -p "$home/base/maps" "$home/base/screenshots" "$OUT/$backend"
  "$FIXTURE_WRITER" --write-irradiance-fixture "$home/base/maps/arena7.wprobe" 512 -640 -384 >/dev/null
  cfg="$home/base/lava-sanctum-matrix.cfg"
  log="$OUT/$backend/arena7.log"
  marker="$(wired_shot_marker)"
  printf '%s\n' \
		'map arena7' 'waitForMap' 'wait 90' 'cmd noclip' \
		'set cg_drawGun 0' 'set cg_thirdPerson 0' \
		'cmd setviewpos 650 -500 -250 225 10' 'wait 180' \
    'testmodel models/weapons2/grenadel/grenadel.md3 0' 'wait 180' \
    "screenshot lava_sanctum_${backend} png silent" 'wait 30' 'quit' >"$cfg"

  python3 "$WATCHDOG" --timeout 120 --kill-after 15 --cwd "$(dirname "$ENGINE")" --stdout "$log" -- \
    "$ENGINE" +set fs_installpath "$module_app" +set fs_homepath "$home" +set cl_renderer "$backend" \
    +set net_port 0 +set com_automated 1 +set com_noHardReboot 1 +set s_initsound 0 \
    +set sv_pure 0 +set sv_cheats 1 +set vm_game 0 +set vm_cgame 0 \
    +set r_fullscreen 0 +set r_mode -1 +set r_customwidth 1280 +set r_customheight 720 \
    +set r_renderScale 1 +set r_pinShaderTime 1.0 +set r_pinFrameTime 1.0 \
    +set r_hdrAutoExposure 0 +set r_bloom 0 +set r_smaa 0 +set con_notifytime 0 \
    +set cg_draw2D 0 +log renderer.init info +exec lava-sanctum-matrix.cfg
  shot="$(wired_newest_shot "$marker" "$home/base/screenshots")" || {
    echo "FAIL: $backend produced no fresh arena7 capture" >&2; tail -n 100 "$log"; exit 1;
  }
  cp "$shot" "$OUT/$backend/arena7.png"
}

capture_native vulkan
if [ "$(uname -s)" = Darwin ]; then
  capture_native metal
  opengl_status="skip:macOS exposes OpenGL 4.1, below the required OpenGL 4.6 adapter"
else
  echo "FAIL: this matrix requires a supported native Metal host or a separately retained explicit skip" >&2
  exit 1
fi

if wired_find_chrome >/dev/null; then
  echo "FAIL: Chrome exists, so WebGPU arena7 evidence must be captured rather than skipped" >&2
  exit 1
else
  webgpu_status="skip:no WebGPU-capable browser executable is available on this host"
fi

python3 "$SCRIPT_DIR/lava-sanctum-lighting-backend-matrix-analyze.py" \
  "$OUT" "$PNG2RAW" "$opengl_status" "$webgpu_status"
