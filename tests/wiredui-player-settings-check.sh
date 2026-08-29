#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors

# Real Player Settings composition gate. Captures the same menu on Metal and
# Vulkan at a 1280x720 logical window, then checks Clay-authoritative geometry,
# listbox containment/non-overlap, screenshot readability and backend parity.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
. "$SCRIPT_DIR/lib/wired_paths.sh"

ENGINE="${1:-${WIRED_BINARY:-}}"
Q3DIR="${Q3DIR:-$WIRED_INSTALL}"
OUT="${PLAYER_SETTINGS_UI_OUT:-$REPO_ROOT/build/player-settings-ui}"
WATCHDOG="$SCRIPT_DIR/run-with-timeout.py"
PNG2RAW="${PNG2RAW:-$REPO_ROOT/tools/png2raw/png2raw}"

[ "$(uname -s)" = Darwin ] || { echo "SKIP: native Player Settings backend matrix requires macOS"; exit 77; }
[ -x "$ENGINE" ] || { echo "FAIL: engine not executable: $ENGINE"; exit 1; }
[ -x "$PNG2RAW" ] || { echo "FAIL: png2raw not executable: $PNG2RAW"; exit 1; }

mkdir -p "$OUT"
RUN_ROOT="$(mktemp -d "$WIRED_TMP/q3now-player-settings-ui-XXXXXX")"
trap 'rm -rf "$RUN_ROOT"' EXIT
WIRED_TMP="$RUN_ROOT"

for backend in metal vulkan; do
  case "$backend" in
    metal) module="wired_metal_arm64.dylib" ;;
    vulkan) module="wired_vulkan_arm64.dylib" ;;
  esac
  [ -f "$(dirname "$ENGINE")/$module" ] || { echo "FAIL: missing renderer: $module"; exit 1; }

  HOME_DIR="$(wired_isolated_home "player-settings-$backend")"
  mkdir -p "$HOME_DIR/base/screenshots"
  CFG="$HOME_DIR/base/player-settings-ui.cfg"
  LOG="$OUT/$backend.log"
  LAYOUT_SRC="$(dirname "$ENGINE")/layoutdump.jsonl"
  CLAY_SRC="$HOME_DIR/base/player_settings_${backend}_clay.json"
  rm -f "$LAYOUT_SRC" "$CLAY_SRC" "$HOME_DIR/base/screenshots/player_settings_${backend}.png"
  printf '%s\n' \
    'set ui_palette_mode dark' \
    'set r_atmosphericGPU 0' \
    'set r_flares 0' \
    'set r_lens 0' \
    'set r_halos 0' \
    'set r_sunRayIntensity 0' \
	'set skin original' \
    'wait 120' \
    'wui_push playersettings' \
    'wait 45' \
    'set r_layoutDump 1' \
    'wait 5' \
    "wui_test_dump_clay player_settings_${backend}_clay.json" \
    'wait 5' \
    "screenshot player_settings_${backend} png silent" \
    'set r_layoutDump 0' \
    'wait 5' \
    'quit' >"$CFG"

  python3 "$WATCHDOG" --timeout 75 --kill-after 15 \
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
    +exec player-settings-ui.cfg

  SHOT="$HOME_DIR/base/screenshots/player_settings_${backend}.png"
  [ -s "$SHOT" ] || { echo "FAIL: missing $backend Player Settings screenshot"; exit 1; }
  [ -s "$LAYOUT_SRC" ] || { echo "FAIL: missing $backend Player Settings layout dump"; exit 1; }
  [ -s "$CLAY_SRC" ] || { echo "FAIL: missing $backend Player Settings Clay command dump"; exit 1; }
  cp "$SHOT" "$OUT/$backend.png"
  cp "$LAYOUT_SRC" "$OUT/$backend-layout.jsonl"
  cp "$CLAY_SRC" "$OUT/$backend-clay.json"
  grep -q "WiredUI: push menu 'playersettings'" "$LOG" || { echo "FAIL: $backend did not open Player Settings"; exit 1; }
  if grep -Eiq 'VUID-|validation error|lifecycle failure|renderer .*failed to initialize|failed to initialize renderer|Sys_Error|\bFATAL\b|died on signal' "$LOG"; then
    echo "FAIL: forbidden $backend runtime diagnostic"
    exit 1
  fi
done

python3 - "$OUT" "$PNG2RAW" <<'PY'
import json,struct,subprocess,sys
from pathlib import Path

root=Path(sys.argv[1]); decoder=sys.argv[2]
required=("settings_root","settings_content","settings_nav","settings_panel",
          "panel_content","grid_row_1","group_identity","model_browser",
          "model_preview","model_pickers","char_frame","charlist",
          "skin_frame","skinlist","group_crosshair")
geometry={}
decoded={}

def contains(outer,inner,tol=1.5):
    return (inner["x"]>=outer["x"]-tol and inner["y"]>=outer["y"]-tol and
            inner["x"]+inner["w"]<=outer["x"]+outer["w"]+tol and
            inner["y"]+inner["h"]<=outer["y"]+outer["h"]+tol)

def overlaps(a,b,tol=1.0):
    return (min(a["x"]+a["w"],b["x"]+b["w"])-max(a["x"],b["x"])>tol and
            min(a["y"]+a["h"],b["y"]+b["h"])-max(a["y"],b["y"])>tol)

def same_rect(a,b,tol=0.5):
    return all(abs(a[k]-b[k])<=tol for k in ("x","y","w","h"))

for backend in ("metal","vulkan"):
    path=root/f"{backend}.png"
    header=path.read_bytes()[:24]
    if len(header)!=24 or header[:8]!=b"\x89PNG\r\n\x1a\n" or header[12:16]!=b"IHDR":
        raise SystemExit(f"FAIL: invalid PNG: {path}")
    width,height=struct.unpack(">II",header[16:24])
    if width%1280 or height%720 or width//1280!=height//720:
        raise SystemExit(f"FAIL: non-presentation extent {width}x{height}: {backend}")

    entries=[json.loads(line) for line in (root/f"{backend}-layout.jsonl").read_text().splitlines()]
    frames={}
    for e in entries: frames.setdefault(e["frame"],[]).append(e)
    candidates=[]
    for frame,items in frames.items():
        regions={e["region"]:e for e in items if e.get("menu")=="playersettings"}
        if all(name in regions and regions[name].get("rectSource")=="clay" for name in required):
            candidates.append((frame,regions))
    if not candidates:
        raise SystemExit(f"FAIL: {backend} has no complete Clay-authoritative Player Settings frame")
    frame,regions=candidates[-1]
    for name in required:
        item=regions[name]
        if item["w"]<=0 or item["h"]<=0:
            raise SystemExit(f"FAIL: {backend} collapsed {name}: {item}")
        if item["x"]<-1 or item["y"]<-1 or item["x"]+item["w"]>width+1 or item["y"]+item["h"]>height+1:
            raise SystemExit(f"FAIL: {backend} out-of-bounds {name}: {item}")

    for child in ("model_browser","model_preview","model_pickers","char_frame","charlist","skin_frame","skinlist"):
        if not contains(regions["group_identity"],regions[child]):
            raise SystemExit(f"FAIL: {backend} {child} bleeds outside identity panel")
    if overlaps(regions["group_identity"],regions["group_crosshair"]):
        raise SystemExit(f"FAIL: {backend} identity/crosshair panels overlap")
    if overlaps(regions["model_preview"],regions["model_pickers"]):
        raise SystemExit(f"FAIL: {backend} preview/picker columns overlap")
    if overlaps(regions["charlist"],regions["skinlist"]):
        raise SystemExit(f"FAIL: {backend} character/skin lists overlap")
    # TASK-142/#8: the historical 5-25 px outer-container bleed came from a
    # floating listbox using stale rounded dimensions while its Clay frame had
    # already resolved to a different box.  Pin the actual regression shape:
    # each listbox must exactly cover (and therefore cannot escape) the frame
    # that owns it.  Half a physical pixel permits serialized float noise, not
    # a visible seam or background strip.
    for frame_name,list_name in (("char_frame","charlist"),("skin_frame","skinlist")):
        if not same_rect(regions[frame_name],regions[list_name]):
            raise SystemExit(
                f"FAIL: {backend} listbox outer-container bleed: "
                f"{frame_name}={regions[frame_name]} {list_name}={regions[list_name]}")

    pixels=subprocess.check_output([decoder,str(path)])
    decoded[backend]=(pixels,width,height)
    def edge_density(x0,y0,x1,y1):
        edges=0; total=max(1,(x1-x0)*(y1-y0))
        for y in range(y0,y1):
            for x in range(x0,x1-1):
                i=(y*width+x)*3
                if max(abs(pixels[i+c]-pixels[i+3+c]) for c in range(3))>26: edges+=1
        return edges/total

    def region_edge_density(name,inset=4):
        region=regions[name]
        x0=max(0,int(region["x"])+inset)
        y0=max(0,int(region["y"])+inset)
        x1=min(width,int(region["x"]+region["w"])-inset)
        y1=min(height,int(region["y"]+region["h"])-inset)
        return edge_density(x0,y0,x1,y1)
    left=edge_density(0,height//8,width//2,height*7//8)
    right=edge_density(width//2,height//8,width,height*7//8)
    if left<0.002 or right<0.002:
        raise SystemExit(f"FAIL: {backend} Player Settings content unreadable: left={left:.3%}, right={right:.3%}")
    mean_luma=sum(pixels)/(len(pixels) or 1)
    if mean_luma>120.0:
        raise SystemExit(f"FAIL: {backend} backdrop is washed out/fallback-white: mean={mean_luma:.1f}")

    # A valid rectangle is not sufficient: the original Vulkan regression
    # preserved the list boxes' geometry while dropping every feeder glyph,
    # and both backends once accepted an empty 3D preview.  Require local
    # high-frequency content inside each semantic region so a blank panel can
    # never satisfy this visual gate again.
    local_edges={name:region_edge_density(name) for name in
                 ("model_preview","charlist","skinlist")}
    if local_edges["model_preview"]<0.001:
        raise SystemExit(
            f"FAIL: {backend} player-model preview is blank: "
            f"edges={local_edges['model_preview']:.3%}")
    for name in ("charlist","skinlist"):
        if local_edges[name]<0.003:
            raise SystemExit(
                f"FAIL: {backend} {name} has no visible feeder content: "
                f"edges={local_edges[name]:.3%}")

    geometry[backend]={name:tuple(round(regions[name][k],2) for k in ("x","y","w","h")) for name in required}
    print(f"  PASS {backend} Player Settings: {width}x{height}, frame={frame}, "
          f"edges={left:.2%}/{right:.2%}, local="
          f"{local_edges['model_preview']:.2%}/"
          f"{local_edges['charlist']:.2%}/"
          f"{local_edges['skinlist']:.2%}")

for name in required:
    a=geometry["metal"][name]; b=geometry["vulkan"][name]
    if max(abs(x-y) for x,y in zip(a,b))>1.0:
        raise SystemExit(f"FAIL: backend geometry drift {name}: Metal={a}, Vulkan={b}")
metal,mw,mh=decoded["metal"]; vulkan,vw,vh=decoded["vulkan"]
if (mw,mh)!=(vw,vh) or len(metal)!=len(vulkan):
    raise SystemExit("FAIL: backend screenshot extent drift")
absolute=sum(abs(a-b) for a,b in zip(metal,vulkan))/max(1,len(metal))
large=sum(abs(a-b)>64 for a,b in zip(metal,vulkan))/max(1,len(metal))
if absolute>12.0 or large>0.08:
    raise SystemExit(f"FAIL: backend visual drift mean={absolute:.2f}, >64={large:.2%}")
print(f"  PASS backend pixels: mean delta={absolute:.2f}, >64={large:.2%}")
print(f"PASS Player Settings Metal/Vulkan composition parity: {root}")
PY
