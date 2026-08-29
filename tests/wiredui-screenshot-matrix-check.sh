#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors

# Widescreen-only frozen screenshot gate for every independently reachable
# MENU and POPUP panel. Other compositor layers have stateful product gates
# (loading, attract, HUD/scoreboards, console/overlay/world viewport); the
# source catalog still accounts for their authored roots and every include.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
. "$SCRIPT_DIR/lib/wired_paths.sh"

ENGINE="${1:-${WIRED_BINARY:-}}"
Q3DIR="${Q3DIR:-$WIRED_INSTALL}"
OUT="${WIREDUI_SCREENSHOT_MATRIX_OUT:-$REPO_ROOT/build/wiredui-screenshot-matrix}"
WATCHDOG="$SCRIPT_DIR/run-with-timeout.py"
PNG2RAW="${PNG2RAW:-$REPO_ROOT/tools/png2raw/png2raw}"
BASELINE="$REPO_ROOT/tests/golden/wiredui_screenshot_matrix.darwin-arm64.json"

[ "$(uname -s)" = Darwin ] || { echo "SKIP: Metal/Vulkan screenshot matrix requires macOS"; exit 77; }
[ -x "$ENGINE" ] || { echo "FAIL: engine not executable: $ENGINE"; exit 1; }
[ -x "$PNG2RAW" ] || { echo "FAIL: png2raw not executable: $PNG2RAW"; exit 1; }

mkdir -p "$OUT"
RUN_ROOT="$(mktemp -d "$WIRED_TMP/q3now-wiredui-matrix-XXXXXX")"
WIRED_TMP="$RUN_ROOT"
cleanup_runtime() {
  if [ "${KEEP_ARTIFACTS:-0}" = 1 ]; then
    echo "kept matrix runtime: $RUN_ROOT"
  else
    rm -rf "$RUN_ROOT"
  fi
}
trap cleanup_runtime EXIT
CATALOG="$RUN_ROOT/catalog.tsv"
node "$SCRIPT_DIR/wiredui-source-catalog-test.mjs" >"$CATALOG"

PANEL_COUNT="$(awk -F '\t' '$3 == "menu" || $3 == "popup" { count++ } END { print count + 0 }' "$CATALOG")"
[ "$PANEL_COUNT" -ge 20 ] || { echo "FAIL: suspiciously small MENU/POPUP catalog: $PANEL_COUNT"; exit 1; }

for backend in ${BACKENDS:-metal vulkan}; do
  case "$backend" in
    metal) module="wired_metal_arm64.dylib" ;;
    vulkan) module="wired_vulkan_arm64.dylib" ;;
  esac
  [ -f "$(dirname "$ENGINE")/$module" ] || { echo "FAIL: missing renderer: $module"; exit 1; }

  HOME_DIR="$(wired_isolated_home "wiredui-matrix-$backend")"
  mkdir -p "$HOME_DIR/base/screenshots" "$OUT/$backend"
  CFG="$HOME_DIR/base/wiredui-screenshot-matrix.cfg"
  LOG="$OUT/$backend.log"
  LAYOUT_SRC="$(dirname "$ENGINE")/layoutdump.jsonl"
  rm -f "$LAYOUT_SRC"

  {
    printf '%s\n' \
      'set ui_palette_mode dark' \
      'set r_atmosphericGPU 0' \
      'set r_flares 0' \
      'set r_lens 0' \
      'set r_halos 0' \
      'set r_sunRayIntensity 0' \
      'wait 120'
    while IFS=$'\t' read -r source name layer; do
      case "$layer" in
        menu)
          printf 'wui_push %s animated\n' "$name"
          ;;
        popup)
          printf 'wui_push main animated\nwait 8\nwui_popup %s\n' "$name"
          ;;
        *) continue ;;
      esac
      printf 'wait 18\nset r_layoutDump 1\nwait 3\nscreenshot matrix_%s_%s png silent\nset r_layoutDump 0\nwait 3\n' "$backend" "$name"
      if [ "$layer" = popup ]; then
        printf 'wui_popup dismiss\nwait 4\nwui_menu_nav back\nwait 4\n'
      else
        printf 'wui_menu_nav back\nwait 4\n'
      fi
    done <"$CATALOG"
    printf '%s\n' 'quit'
  } >"$CFG"

  python3 "$WATCHDOG" --timeout 180 --kill-after 15 \
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
    +set r_fullscreen 0 \
    +set r_mode -1 \
    +set r_customwidth 1280 \
    +set r_customheight 720 \
    +set r_renderScale 1 \
    +set r_vkValidate 1 \
    +set r_hdr 0 \
    +set r_hdrDisplay 0 \
    +set r_hdrAutoExposure 0 \
    +exec wiredui-screenshot-matrix.cfg

  [ -s "$LAYOUT_SRC" ] || { echo "FAIL: missing $backend layout dump"; exit 1; }
  cp "$LAYOUT_SRC" "$OUT/$backend-layout.jsonl"
  while IFS=$'\t' read -r source name layer; do
    case "$layer" in menu|popup) ;; *) continue ;; esac
    shot="$HOME_DIR/base/screenshots/matrix_${backend}_${name}.png"
    [ -s "$shot" ] || { echo "FAIL: missing $backend/$name screenshot"; exit 1; }
    cp "$shot" "$OUT/$backend/$name.png"
  done <"$CATALOG"
  if grep -Eiq 'VUID-|validation error|lifecycle failure|renderer .*failed to initialize|failed to initialize renderer|Sys_Error|\bFATAL\b|died on signal' "$LOG"; then
    echo "FAIL: forbidden $backend runtime diagnostic"
    exit 1
  fi
done

python3 - "$OUT" "$CATALOG" "$PNG2RAW" "$BASELINE" "${BLESS:-0}" <<'PY'
import json, struct, subprocess, sys
from pathlib import Path

root, catalog_path, decoder, baseline_path, bless = Path(sys.argv[1]), Path(sys.argv[2]), sys.argv[3], Path(sys.argv[4]), sys.argv[5] == "1"
panels=[]
for line in catalog_path.read_text().splitlines():
    source,name,layer=line.split("\t")
    if layer in {"menu","popup"}: panels.append((source,name,layer))

layouts={}
for backend in ("metal","vulkan"):
    rows=[json.loads(line) for line in (root/f"{backend}-layout.jsonl").read_text().splitlines() if line.strip()]
    layouts[backend]=rows

settings_focus={
    "sound":"nav_sound", "video":"nav_video", "display":"nav_display",
    "controls":"nav_controls", "network":"nav_network",
    "playersettings":"nav_player", "preferences":"nav_preferences",
}

def signature(pixels,w,h):
    result=[]
    for gy in range(9):
        y0=gy*h//9; y1=(gy+1)*h//9
        for gx in range(16):
            x0=gx*w//16; x1=(gx+1)*w//16
            sums=[0,0,0]; count=max(1,(x1-x0)*(y1-y0))
            for y in range(y0,y1):
                row=y*w*3
                for x in range(x0,x1):
                    i=row+x*3
                    sums[0]+=pixels[i]; sums[1]+=pixels[i+1]; sums[2]+=pixels[i+2]
            result.extend(round(value/count) for value in sums)
    return result

current={"resolution":"1280x720 logical; native backing permitted","panels":{}}
for source,name,layer in panels:
    decoded={}
    for backend in ("metal","vulkan"):
        path=root/backend/f"{name}.png"
        header=path.read_bytes()[:24]
        if len(header)!=24 or header[:8]!=b"\x89PNG\r\n\x1a\n" or header[12:16]!=b"IHDR":
            raise SystemExit(f"FAIL: invalid PNG: {path}")
        width,height=struct.unpack(">II",header[16:24])
        if width%1280 or height%720 or width//1280!=height//720:
            raise SystemExit(f"FAIL: non-widescreen presentation extent {width}x{height}: {backend}/{name}")
        pixels=subprocess.check_output([decoder,str(path)])
        decoded[backend]=(pixels,width,height)

        menu_rows=[row for row in layouts[backend] if row.get("menu")==name and row.get("rectSource")=="clay"]
        if not menu_rows:
            raise SystemExit(f"FAIL: no Clay-authoritative layout evidence: {backend}/{name}")
        if not any(row.get("w",0)>0 and row.get("h",0)>0 for row in menu_rows):
            raise SystemExit(f"FAIL: every emitted rect collapsed: {backend}/{name}")
        if name in settings_focus:
            latest=max(row.get("frame",0) for row in menu_rows)
            focused=[row.get("region") for row in menu_rows
                     if row.get("frame")==latest and row.get("focused")==1]
            if focused != [settings_focus[name]]:
                raise SystemExit(
                    f"FAIL: settings focus/state mismatch {backend}/{name}: "
                    f"expected {settings_focus[name]}, got {focused}")
        edges=sum(1 for i in range(0,len(pixels)-3,3) if max(abs(pixels[i+c]-pixels[i+3+c]) for c in range(3))>24)
        density=edges/max(1,width*height)
        if density<0.00015:
            raise SystemExit(f"FAIL: screenshot has no readable UI edges: {backend}/{name} density={density:.4%}")

    metal,mw,mh=decoded["metal"]; vulkan,vw,vh=decoded["vulkan"]
    if (mw,mh)!=(vw,vh): raise SystemExit(f"FAIL: backend extent drift: {name}")
    mean=sum(abs(a-b) for a,b in zip(metal,vulkan))/max(1,len(metal))
    large=sum(abs(a-b)>64 for a,b in zip(metal,vulkan))/max(1,len(metal))
    if mean>12.0 or large>0.08:
        raise SystemExit(f"FAIL: backend visual drift {name}: mean={mean:.2f}, >64={large:.2%}")
    current["panels"][name]={"source":source,"layer":layer,"signature":signature(metal,mw,mh)}
    print(f"  PASS {name}: Metal/Vulkan mean={mean:.2f}, >64={large:.2%}")

if bless:
    baseline_path.write_text(json.dumps(current,indent=2,sort_keys=True)+"\n")
    print(f"BLESSED widescreen screenshot baseline: {baseline_path}")
else:
    if not baseline_path.is_file():
        raise SystemExit(f"FAIL: frozen baseline missing: {baseline_path} (review captures, then rerun with BLESS=1)")
    baseline=json.loads(baseline_path.read_text())
    if set(baseline.get("panels",{}))!=set(current["panels"]):
        raise SystemExit("FAIL: frozen baseline panel inventory drift")
    for name,item in current["panels"].items():
        old=baseline["panels"][name]["signature"]; new=item["signature"]
        delta=sum(abs(a-b) for a,b in zip(old,new))/max(1,len(new))
        peak=max(abs(a-b) for a,b in zip(old,new))
        if delta>4.0 or peak>36:
            raise SystemExit(f"FAIL: frozen screenshot drift {name}: mean={delta:.2f}, peak={peak}")
    print(f"PASS frozen widescreen baseline: {len(panels)} MENU/POPUP panels")
PY

echo "PASS WiredUI screenshot matrix: $PANEL_COUNT panels × Metal/Vulkan at 1280x720"
