#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors

# Real CA_PRIMED loading-frame gate for the runtime-capable macOS backends.
# OpenGL 4.6 has no macOS context and is covered by its supported-host runtime
# lane; this host exercises Metal and Vulkan. The debug-only hold captures the
# compositor while loading is still active; this is not a pushed-menu mock.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
. "$SCRIPT_DIR/lib/wired_paths.sh"

ENGINE="${1:-${WIRED_BINARY:-}}"
Q3DIR="${Q3DIR:-$WIRED_INSTALL}"
OUT="${LOADING_UI_OUT:-$REPO_ROOT/build/loading-ui}"
WATCHDOG="$SCRIPT_DIR/run-with-timeout.py"
PNG2RAW="${PNG2RAW:-$REPO_ROOT/tools/png2raw/png2raw}"

[ "$(uname -s)" = Darwin ] || { echo "SKIP: native loading backend matrix requires macOS"; exit 77; }
[ -x "$ENGINE" ] || { echo "FAIL: engine not executable: $ENGINE"; exit 1; }
[ -x "$PNG2RAW" ] || { echo "FAIL: png2raw not executable: $PNG2RAW"; exit 1; }

if rg -n '^\s*rect\b|\bposition\s+absolute\b' "$REPO_ROOT/modfiles/ui" --glob '*.wui'; then
  echo "FAIL: WiredUI source still contains viewport-position literals"
  exit 1
fi
[ ! -e "$REPO_ROOT/modfiles/ui/default.wui" ] || { echo "FAIL: superseded default.wui still ships"; exit 1; }
grep -q 'Q_stricmp( hudName, "default" )' "$REPO_ROOT/code/client/wired/ui/cl_wired_ui.c" || {
  echo "FAIL: legacy hud default alias is missing"; exit 1;
}

mkdir -p "$OUT"
RUN_ROOT="$(mktemp -d "$WIRED_TMP/q3now-loading-ui-XXXXXX")"
trap 'rm -rf "$RUN_ROOT"' EXIT
WIRED_TMP="$RUN_ROOT"

for backend in metal vulkan; do
  case "$backend" in
    metal) module="wired_metal_arm64.dylib" ;;
    vulkan) module="wired_vulkan_arm64.dylib" ;;
  esac
  [ -f "$(dirname "$ENGINE")/$module" ] || { echo "FAIL: missing renderer: $module"; exit 1; }

  HOME_DIR="$(wired_isolated_home "loading-ui-$backend")"
  mkdir -p "$HOME_DIR/base/screenshots"
  CFG="$HOME_DIR/base/loading-ui.cfg"
  LOG="$OUT/$backend.log"
  LAYOUT_SRC="$(dirname "$ENGINE")/layoutdump.jsonl"
  rm -f "$LAYOUT_SRC" "$HOME_DIR/base/screenshots/wn_loadshot.png"
  printf '%s\n' \
    'set ui_palette_mode dark' \
    'set r_atmosphericGPU 0' \
    'set r_flares 0' \
    'set r_lens 0' \
    'set r_halos 0' \
    'set r_sunRayIntensity 0' \
    'set debug_hold_loading 4' \
    'set r_layoutDump 1' \
    'map arena1' \
    'waitForMap' \
    'set r_layoutDump 0' \
    'wait 15' \
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
    +exec loading-ui.cfg

  SHOT="$HOME_DIR/base/screenshots/wn_loadshot.png"
  [ -s "$SHOT" ] || { echo "FAIL: missing $backend loading screenshot"; exit 1; }
  [ -s "$LAYOUT_SRC" ] || { echo "FAIL: missing $backend loading layout dump"; exit 1; }
  cp "$SHOT" "$OUT/$backend.png"
  cp "$LAYOUT_SRC" "$OUT/$backend-layout.jsonl"

  grep -q 'cls.state: -> CA_PRIMED' "$LOG" || { echo "FAIL: $backend never reached CA_PRIMED"; exit 1; }
  grep -q 'cls.state: -> CA_ACTIVE' "$LOG" || { echo "FAIL: $backend never completed loading"; exit 1; }
  if grep -Eiq 'VUID-|validation error|lifecycle failure|renderer .*failed to initialize|failed to initialize renderer|Sys_Error|\bFATAL\b|died on signal' "$LOG"; then
    echo "FAIL: forbidden $backend runtime diagnostic"
    exit 1
  fi
done

python3 - "$OUT" "$PNG2RAW" <<'PY'
import json,struct,subprocess,sys
from pathlib import Path

root=Path(sys.argv[1]); decoder=sys.argv[2]
required=("loading_root","loading_topbar","loading_body","loading_left_panel",
          "loading_wireframe","loading_divider","loading_right_panel",
          "loading_maptitle_block","loading_mapinfo_stats","loading_streaming_rows",
          "loading_phase_text","loading_overall_bar","loading_bottom_strip")
extents=set()
for backend in ("metal","vulkan"):
    path=root/f"{backend}.png"
    data=path.read_bytes()[:24]
    if len(data)!=24 or data[:8]!=b"\x89PNG\r\n\x1a\n" or data[12:16]!=b"IHDR":
        raise SystemExit(f"FAIL: invalid PNG: {path}")
    width,height=struct.unpack(">II",data[16:24]); extents.add((width,height))
    if width%1280 or height%720 or width//1280!=height//720:
        raise SystemExit(f"FAIL: non-presentation extent {width}x{height}: {backend}")

    entries=[json.loads(line) for line in (root/f"{backend}-layout.jsonl").read_text().splitlines()]
    frames={}
    for e in entries:
        frames.setdefault(e["frame"],[]).append(e)
    candidates=[]
    for frame,items in frames.items():
        regions={e["region"]:e for e in items if e.get("menu")=="loading_screen"}
        if all(name in regions for name in required): candidates.append((frame,items,regions))
    if not candidates:
        raise SystemExit(f"FAIL: {backend} has no complete loading layout frame")
    if len(candidates)<2:
        raise SystemExit(f"FAIL: {backend} needs at least two complete loading frames, got {len(candidates)}")
    phase_samples=[]
    for sample_frame,_,sample_regions in candidates:
        phase=sample_regions["loading_phase_text"]
        # The first emitted frame legitimately has no preceding completed Clay
        # layout and is marked compat. Drift is measured only once the runtime
        # ground truth exists; at least two such frames are mandatory below.
        if phase.get("rectSource")=="clay":
            phase_samples.append((sample_frame,phase["y"],phase["h"]))
    if len(phase_samples)<2:
        raise SystemExit(f"FAIL: {backend} needs at least two Clay-authoritative phase frames, got {phase_samples}")
    y_values=[v[1] for v in phase_samples]
    h_values=[v[2] for v in phase_samples]
    if max(y_values)-min(y_values)>0.5 or max(h_values)-min(h_values)>0.5:
        raise SystemExit(f"FAIL: {backend} loading phase-text drifted across frames: {phase_samples}")
    frame,items,regions=candidates[-1]
    menus={e.get("menu") for e in items}
    if "attract_brand" in menus or "attract_leaderboard" in menus:
        raise SystemExit(f"FAIL: {backend} loading frame {frame} also emitted attract UI: {sorted(menus)}")
    rootbox=regions["loading_root"]
    if abs(rootbox["w"]-width)>1 or abs(rootbox["h"]-height)>1:
        raise SystemExit(f"FAIL: {backend} loading root is not presentation-sized: {rootbox}")
    for name in required[1:]:
        item=regions[name]
        if item["w"]<=0 or item["h"]<=0:
            raise SystemExit(f"FAIL: {backend} collapsed loading region {name}: {item}")
        if item["x"] < -1 or item["y"] < -1 or item["x"]+item["w"] > width+1 or item["y"]+item["h"] > height+1:
            raise SystemExit(f"FAIL: {backend} out-of-bounds loading region {name}: {item}")

    pixels=subprocess.check_output([decoder,str(path)])
    def metrics(x0,y0,x1,y1):
        bright=edge=0; total=(x1-x0)*(y1-y0)
        for y in range(y0,y1):
            for x in range(x0,x1):
                i=(y*width+x)*3
                lum=(pixels[i]*54+pixels[i+1]*183+pixels[i+2]*19)//256
                bright += lum>120
                if x+1<x1:
                    edge += max(abs(pixels[i+c]-pixels[i+3+c]) for c in range(3))>28
        return bright/total,edge/total
    top=metrics(0,0,width,height//10)
    left=metrics(0,height//5,width//2,height*9//10)
    right=metrics(width//2,height//10,width,height*9//10)
    if top[1]<0.001 or left[1]<0.003 or right[1]<0.003:
        raise SystemExit(f"FAIL: {backend} loading content unreadable: top={top}, left={left}, right={right}")
    print(f"  PASS {backend} loading: {width}x{height}, frame={frame}, phase-y={y_values[-1]:.2f} stable/{len(phase_samples)}f, top/left/right edges={top[1]:.2%}/{left[1]:.2%}/{right[1]:.2%}")
if len(extents)!=1:
    raise SystemExit(f"FAIL: loading backend extents differ: {sorted(extents)}")
print(f"PASS macOS loading UI backend matrix: {root}")
PY
