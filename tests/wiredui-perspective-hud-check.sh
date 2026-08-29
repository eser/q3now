#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors

# Selectable perspective HUD: native parser/Clay geometry plus Metal/Vulkan
# screenshot parity at the project-mandated 1280x720 logical presentation.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
. "$SCRIPT_DIR/lib/wired_paths.sh"

ENGINE="${1:-${WIRED_BINARY:-}}"
Q3DIR="${Q3DIR:-$WIRED_INSTALL}"
OUT="${WUI_PERSPECTIVE_OUT:-$REPO_ROOT/build/wiredui-perspective-hud}"
WATCHDOG="$SCRIPT_DIR/run-with-timeout.py"
PNG2RAW="${PNG2RAW:-$REPO_ROOT/tools/png2raw/png2raw}"

[ "$(uname -s)" = Darwin ] || { echo "SKIP: perspective HUD matrix requires macOS"; exit 77; }
[ -x "$ENGINE" ] || { echo "FAIL: engine not executable: $ENGINE"; exit 1; }
[ -x "$PNG2RAW" ] || { echo "FAIL: png2raw not executable: $PNG2RAW"; exit 1; }

mkdir -p "$OUT"
RUN_ROOT="$(mktemp -d "$WIRED_TMP/q3now-wui-perspective-XXXXXX")"
trap 'rm -rf "$RUN_ROOT"' EXIT
WIRED_TMP="$RUN_ROOT"

for backend in metal vulkan; do
  case "$backend" in
    metal) module="wired_metal_arm64.dylib" ;;
    vulkan) module="wired_vulkan_arm64.dylib" ;;
  esac
  [ -f "$(dirname "$ENGINE")/$module" ] || { echo "FAIL: missing renderer: $module"; exit 1; }

  HOME_DIR="$(wired_isolated_home "wui-perspective-$backend")"
  mkdir -p "$HOME_DIR/base/screenshots"
  CFG="$HOME_DIR/base/wui-perspective.cfg"
  LOG="$OUT/$backend.log"
  INSPECTOR_SRC="$(dirname "$ENGINE")/wiredui-inspector.jsonl"
  SHOT="$HOME_DIR/base/screenshots/perspective_${backend}.png"
  rm -f "$INSPECTOR_SRC" "$SHOT"

  printf '%s\n' \
    'set con_notifytime 0' \
    'set hud perspective' \
    'map arena1' \
    'waitForMap' \
    'wait 120' \
    'setviewpos 1052 1432 50 135' \
    'wait 60' \
    'wui_inspect perspective *' \
    'wait 10' \
    "screenshot perspective_${backend} png silent" \
    'wait 10' \
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
    +exec wui-perspective.cfg

  [ -s "$SHOT" ] || { echo "FAIL: missing $backend perspective screenshot"; tail -n 100 "$LOG"; exit 1; }
  [ -s "$INSPECTOR_SRC" ] || { echo "FAIL: missing $backend perspective inspector output"; exit 1; }
  if grep -Eiq 'WiredUI: unexpected token.*ui/perspective|WiredUI:.*failed.*perspective|lifecycle failure|renderer .*failed to initialize|failed to initialize renderer|Sys_Error|\bFATAL\b|died on signal' "$LOG"; then
    echo "FAIL: forbidden $backend perspective HUD diagnostic"; tail -n 100 "$LOG"; exit 1
  fi
  cp "$SHOT" "$OUT/$backend.png"
  cp "$INSPECTOR_SRC" "$OUT/$backend-inspector.jsonl"
done

python3 - "$OUT" "$PNG2RAW" <<'PY'
import json, struct, subprocess, sys
from pathlib import Path

root=Path(sys.argv[1]); decoder=sys.argv[2]
rects={}
for backend in ("metal", "vulkan"):
    shot=root/f"{backend}.png"
    header=shot.read_bytes()[:24]
    if len(header)!=24 or header[:8]!=b"\x89PNG\r\n\x1a\n" or header[12:16]!=b"IHDR":
        raise SystemExit(f"FAIL: invalid {backend} perspective PNG")
    width,height=struct.unpack(">II",header[16:24])
    if width%1280 or height%720 or width//1280!=height//720:
        raise SystemExit(f"FAIL: {backend} perspective extent is not 1280x720 presentation scale: {width}x{height}")
    pixels=subprocess.check_output([decoder,str(shot)])
    mean=sum(pixels)/(len(pixels) or 1)
    if mean<2.0 or mean>245.0:
        raise SystemExit(f"FAIL: {backend} perspective screenshot blank/washed out: mean={mean:.2f}")

    rows=[json.loads(line) for line in (root/f"{backend}-inspector.jsonl").read_text().splitlines() if line.strip()]
    items={row.get("name"):row for row in rows if row.get("kind")=="item" and row.get("menu")=="perspective"}
    required=("hud_perspective_grid", "hud_perspective_status", "hud_perspective_ammo", "hud_active_crosshair")
    if any(name not in items for name in required):
        raise SystemExit(f"FAIL: {backend} perspective tree incomplete: {sorted(items)}")
    status=items["hud_perspective_status"]; ammo=items["hud_perspective_ammo"]
    if abs(status.get("layout",{}).get("perspective",0)-0.20)>1e-4:
        raise SystemExit(f"FAIL: {backend} left projection missing")
    if abs(ammo.get("layout",{}).get("perspective",0)+0.20)>1e-4:
        raise SystemExit(f"FAIL: {backend} right projection missing")
    sr=status["rect"]; ar=ammo["rect"]
    if sr.get("source")!="clay" or ar.get("source")!="clay" or sr["x"]>=width/4 or ar["x"]<=3*width/4:
        raise SystemExit(f"FAIL: {backend} instruments are not in mirrored corners: {sr} / {ar}")
    if min(sr["w"],sr["h"],ar["w"],ar["h"])<=0 or sr["y"]+sr["h"]>height+0.5 or ar["y"]+ar["h"]>height+0.5:
        raise SystemExit(f"FAIL: {backend} perspective instruments out of bounds")

    # The previous regression projected only the panel chrome: the inspector
    # still reported a perspective value while the HEALTH number and bar were
    # horizontal. Prove that a descendant custom bar is actually painted on
    # the same plane by fitting the blue health-bar pixels. Its authored rect
    # remains axis-aligned by design, so a visible negative slope is paint-time
    # inheritance evidence rather than layout deformation.
    bar_path="hud_perspective_status/hud_perspective_health_row/child3"
    bars=[row for row in rows if row.get("kind")=="item" and row.get("path","").endswith(bar_path)]
    if not bars:
        raise SystemExit(f"FAIL: {backend} health-bar descendant missing from inspector")
    br=bars[-1]["rect"]
    x0=max(0,int(br["x"])); x1=min(width,int(br["x"]+br["w"]+8))
    y0=max(0,int(br["y"]-br["h"]*5)); y1=min(height,int(br["y"]+br["h"]*2))
    blue=[]
    for y in range(y0,y1):
        for x in range(x0,x1):
            off=(y*width+x)*3; r,g,b=pixels[off:off+3]
            if b>140 and b>r*1.35 and b>g*1.08:
                blue.append((x,y))
    if len(blue)<200:
        raise SystemExit(f"FAIL: {backend} projected health-bar pixels missing: {len(blue)}")
    mean_x=sum(x for x,_ in blue)/len(blue); mean_y=sum(y for _,y in blue)/len(blue)
    denom=sum((x-mean_x)**2 for x,_ in blue)
    slope=sum((x-mean_x)*(y-mean_y) for x,y in blue)/denom if denom else 0.0
    if slope>=-0.035:
        raise SystemExit(f"FAIL: {backend} health-bar descendant stayed flat: slope={slope:.4f}")
    rects[backend]=(sr,ar)
    print(f"  PASS {backend}: projected child bar slope={slope:.4f} + crosshair + {width}x{height}, mean={mean:.2f}")

for i,name in enumerate(("status","ammo")):
    a=rects["metal"][i]; b=rects["vulkan"][i]
    if any(abs(a[k]-b[k])>0.51 for k in ("x","y","w","h")):
        raise SystemExit(f"FAIL: Metal/Vulkan {name} layout drift: {a} / {b}")
PY

echo "PASS WiredUI perspective HUD: Metal/Vulkan 1280x720"
