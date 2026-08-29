#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors

# Native inspector + last-good hot-reload gate. A malformed loose menus.lua is
# injected only after the live main menu has produced inspector evidence. The
# failed reload must restore that menu, including its Lua-backed behavior, on
# both native RAL backends. Every window is 1280x720 logical.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
. "$SCRIPT_DIR/lib/wired_paths.sh"

ENGINE="${1:-${WIRED_BINARY:-}}"
Q3DIR="${Q3DIR:-$WIRED_INSTALL}"
OUT="${WUI_INSPECTOR_OUT:-$REPO_ROOT/build/wiredui-inspector-hot-reload}"
WATCHDOG="$SCRIPT_DIR/run-with-timeout.py"
PNG2RAW="${PNG2RAW:-$REPO_ROOT/tools/png2raw/png2raw}"

[ "$(uname -s)" = Darwin ] || { echo "SKIP: Metal/Vulkan inspector matrix requires macOS"; exit 77; }
[ -x "$ENGINE" ] || { echo "FAIL: engine not executable: $ENGINE"; exit 1; }
[ -x "$PNG2RAW" ] || { echo "FAIL: png2raw not executable: $PNG2RAW"; exit 1; }

mkdir -p "$OUT"
RUN_ROOT="$(mktemp -d "$WIRED_TMP/q3now-wui-inspector-XXXXXX")"
trap 'rm -rf "$RUN_ROOT"' EXIT
WIRED_TMP="$RUN_ROOT"

for backend in metal vulkan; do
  case "$backend" in
    metal) module="wired_metal_arm64.dylib" ;;
    vulkan) module="wired_vulkan_arm64.dylib" ;;
  esac
  [ -f "$(dirname "$ENGINE")/$module" ] || { echo "FAIL: missing renderer: $module"; exit 1; }

  HOME_DIR="$(wired_isolated_home "wui-inspector-$backend")"
  MOD_NAME="wui_reload_test"
  MOD_DIR="$HOME_DIR/$MOD_NAME"
  mkdir -p "$MOD_DIR/screenshots"
  CFG="$MOD_DIR/wui-inspector.cfg"
  LOG="$OUT/$backend.log"
  INSPECTOR_SRC="$(dirname "$ENGINE")/wiredui-inspector.jsonl"
  INJECTED="$HOME_DIR/malformed-injected"
  rm -f "$INSPECTOR_SRC" "$INJECTED" "$MOD_DIR/screenshots/wui_reload_${backend}.png"

  printf '%s\n' \
    'wait 90' \
    'wui_push main' \
    'wui_inspect main main_root' \
    'waitms 1000' \
    'set wired_ui_manifest scripts/wui_reload_invalid.lua' \
    'menu_reload' \
    'waitms 1000' \
    'wui_inspect main menu_host' \
    'wait 10' \
    "screenshot wui_reload_${backend} png silent" \
    'wait 5' \
    'quit' >"$CFG"

  # The first inspector frame proves normal startup completed. Only then place
  # a higher-priority loose manifest in the isolated home so menu_reload sees
  # an invalid replacement without perturbing the player's install or config.
  (
    for _ in $(seq 1 500); do
      if [ -s "$INSPECTOR_SRC" ]; then
        mkdir -p "$MOD_DIR/scripts"
        printf '%s\n' 'this is deliberately invalid Lua !!!' >"$MOD_DIR/scripts/wui_reload_invalid.lua"
        : >"$INJECTED"
        exit 0
      fi
      sleep 0.02
    done
    exit 1
  ) &
  injector_pid=$!

  python3 "$WATCHDOG" --timeout 75 --kill-after 15 \
    --cwd "$(dirname "$ENGINE")" --stdout "$LOG" -- \
    "$ENGINE" \
    +set fs_installpath "$Q3DIR" \
    +set fs_homepath "$HOME_DIR" \
    +set fs_game "$MOD_NAME" \
    +set cl_renderer "$backend" \
    +set net_port 0 \
    +set com_automated 1 \
    +set com_noHardReboot 1 \
    +set s_initsound 0 \
    +set sv_pure 0 \
    +set r_fullscreen 0 \
    +set r_mode -1 \
    +set r_customwidth 1280 \
    +set r_customheight 720 \
    +set r_renderScale 1 \
    +set r_hdr 0 \
    +set r_hdrDisplay 0 \
    +set r_hdrAutoExposure 0 \
    +exec wui-inspector.cfg
  wait "$injector_pid" || { echo "FAIL: $backend manifest injector did not run"; exit 1; }

  SHOT="$MOD_DIR/screenshots/wui_reload_${backend}.png"
  [ -e "$INJECTED" ] || { echo "FAIL: $backend malformed manifest was not injected"; exit 1; }
  [ -s "$SHOT" ] || { echo "FAIL: $backend fallback screenshot missing"; exit 1; }
  [ -s "$INSPECTOR_SRC" ] || { echo "FAIL: $backend fallback inspector output missing"; exit 1; }
  grep -q 'Menu reload failed.*keeping old menus' "$LOG" || {
    echo "FAIL: $backend did not report last-good fallback"; exit 1;
  }
  if grep -Eiq 'lifecycle failure|renderer .*failed to initialize|failed to initialize renderer|Sys_Error|\bFATAL\b|died on signal' "$LOG"; then
    echo "FAIL: forbidden $backend runtime diagnostic"; exit 1
  fi
  cp "$SHOT" "$OUT/$backend.png"
  cp "$INSPECTOR_SRC" "$OUT/$backend-inspector.jsonl"
done

python3 - "$OUT" "$PNG2RAW" <<'PY'
import json, struct, subprocess, sys
from pathlib import Path

root=Path(sys.argv[1]); decoder=sys.argv[2]
for backend in ("metal", "vulkan"):
    shot=root/f"{backend}.png"
    header=shot.read_bytes()[:24]
    if len(header)!=24 or header[:8]!=b"\x89PNG\r\n\x1a\n" or header[12:16]!=b"IHDR":
        raise SystemExit(f"FAIL: invalid {backend} fallback PNG")
    width,height=struct.unpack(">II",header[16:24])
    if width%1280 or height%720 or width//1280!=height//720:
        raise SystemExit(f"FAIL: {backend} fallback extent is not 1280x720 presentation scale: {width}x{height}")
    pixels=subprocess.check_output([decoder,str(shot)])
    mean=sum(pixels)/(len(pixels) or 1)
    if mean<2.0 or mean>245.0:
        raise SystemExit(f"FAIL: {backend} fallback screenshot is blank/washed out: mean={mean:.2f}")

    rows=[json.loads(line) for line in (root/f"{backend}-inspector.jsonl").read_text().splitlines() if line.strip()]
    if not rows or any(row.get("schema")!="wired-ui-inspector/v1" for row in rows):
        raise SystemExit(f"FAIL: {backend} inspector protocol/schema missing")
    menus=[row for row in rows if row.get("kind")=="menu" and row.get("menu")=="main"]
    selected=[row for row in rows if row.get("kind")=="item" and row.get("name")=="menu_host" and row.get("selected")==1]
    if not menus or not selected:
        raise SystemExit(f"FAIL: {backend} last-good main/menu_host tree did not survive reload")
    item=selected[-1]
    rect=item.get("rect",{})
    if rect.get("source")!="clay" or rect.get("w",0)<=0 or rect.get("h",0)<=0:
        raise SystemExit(f"FAIL: {backend} selected node lacks live Clay geometry: {rect}")
    for field in ("source","layer","parent","depth","layout","styleProvenance","bindings"):
        if field not in item:
            raise SystemExit(f"FAIL: {backend} inspector field missing: {field}")
    print(f"  PASS {backend}: transactional fallback + inspector tree + {width}x{height} screenshot")
PY

echo "PASS WiredUI native inspector + transactional hot reload (Metal/Vulkan)"
