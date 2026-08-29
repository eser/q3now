#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors

# Lua-authored, native-authority UI behavior gate: main-menu interaction plus
# active and input-driven HUD flows on Metal and Vulkan at 1280x720 logical.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
. "$SCRIPT_DIR/lib/wired_paths.sh"

ENGINE="${1:-${WIRED_BINARY:-}}"
Q3DIR="${Q3DIR:-$WIRED_INSTALL}"
OUT="${WUI_LUA_HARNESS_OUT:-$REPO_ROOT/build/wiredui-lua-harness}"
WATCHDOG="$SCRIPT_DIR/run-with-timeout.py"
PNG2RAW="${PNG2RAW:-$REPO_ROOT/tools/png2raw/png2raw}"

[ "$(uname -s)" = Darwin ] || { echo "SKIP: Metal/Vulkan Lua UI matrix requires macOS"; exit 77; }
[ -x "$ENGINE" ] || { echo "FAIL: engine not executable: $ENGINE"; exit 1; }
[ -x "$PNG2RAW" ] || { echo "FAIL: png2raw not executable: $PNG2RAW"; exit 1; }

mkdir -p "$OUT"
RUN_ROOT="$(mktemp -d "$WIRED_TMP/q3now-wui-lua-XXXXXX")"
trap 'rm -rf "$RUN_ROOT"' EXIT
WIRED_TMP="$RUN_ROOT"

for backend in metal vulkan; do
  case "$backend" in
    metal) module="wired_metal_arm64.dylib" ;;
    vulkan) module="wired_vulkan_arm64.dylib" ;;
  esac
  [ -f "$(dirname "$ENGINE")/$module" ] || { echo "FAIL: missing renderer: $module"; exit 1; }

  HOME_DIR="$(wired_isolated_home "wui-lua-$backend")"
  mkdir -p "$HOME_DIR/base/screenshots"
  CFG="$HOME_DIR/base/wui-lua-harness.cfg"
  LOG="$OUT/$backend.log"
  rm -f "$HOME_DIR/base/screenshots/lua_hud_active.png" \
        "$HOME_DIR/base/screenshots/lua_hud_transient.png"

  printf '%s\n' \
    'wait 120' \
    'wui_test_run scripts/ui_tests/main.lua' \
    'wait 120' \
    'set hud classic' \
    'map arena1' \
    'waitForMap' \
    'wait 90' \
    'wui_test_run scripts/ui_tests/hud-active.lua' \
    'wait 90' \
    'wui_test_run scripts/ui_tests/hud-transient.lua' \
    'wait 90' \
    'quit' >"$CFG"

  python3 "$WATCHDOG" --timeout 90 --kill-after 15 \
    --cwd "$(dirname "$ENGINE")" --stdout "$LOG" -- \
    "$ENGINE" \
    +set fs_installpath "$Q3DIR" \
    +set fs_homepath "$HOME_DIR" \
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
    +set r_renderScale 1 \
    +set r_hdr 0 \
    +set r_hdrDisplay 0 \
    +set r_hdrAutoExposure 0 \
    +exec wui-lua-harness.cfg

  for script in main hud-active hud-transient; do
    grep -q "WiredUI Lua harness: PASS script=scripts/ui_tests/$script.lua" "$LOG" || {
      echo "FAIL: $backend Lua harness did not pass $script"; tail -n 80 "$LOG"; exit 1;
    }
  done
  if grep -Eiq 'WiredUI Lua harness: FAIL|lifecycle failure|renderer .*failed to initialize|failed to initialize renderer|Sys_Error|\bFATAL\b|died on signal' "$LOG"; then
    echo "FAIL: forbidden $backend Lua UI diagnostic"; exit 1
  fi
  for flow in active transient; do
    shot="$HOME_DIR/base/screenshots/lua_hud_${flow}.png"
    [ -s "$shot" ] || { echo "FAIL: missing $backend/$flow HUD screenshot"; exit 1; }
    cp "$shot" "$OUT/$backend-$flow.png"
  done
done

python3 - "$OUT" "$PNG2RAW" <<'PY'
import struct, subprocess, sys
from pathlib import Path

root=Path(sys.argv[1]); decoder=sys.argv[2]
for backend in ("metal", "vulkan"):
    for flow in ("active", "transient"):
        path=root/f"{backend}-{flow}.png"
        header=path.read_bytes()[:24]
        if len(header)!=24 or header[:8]!=b"\x89PNG\r\n\x1a\n" or header[12:16]!=b"IHDR":
            raise SystemExit(f"FAIL: invalid PNG: {path}")
        width,height=struct.unpack(">II",header[16:24])
        if width%1280 or height%720 or width//1280!=height//720:
            raise SystemExit(f"FAIL: non-presentation extent {width}x{height}: {path}")
        pixels=subprocess.check_output([decoder,str(path)])
        mean=sum(pixels)/(len(pixels) or 1)
        if mean<2.0 or mean>245.0:
            raise SystemExit(f"FAIL: blank/washed-out HUD screenshot {path}: mean={mean:.2f}")
        print(f"  PASS {backend}/{flow}: {width}x{height}, mean={mean:.2f}")
PY

echo "PASS WiredUI Lua harness: main + two HUD flows (Metal/Vulkan)"
