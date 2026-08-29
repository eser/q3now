#!/usr/bin/env bash
# TASK-89: measure projection-2D tess flush attribution before optimizing.

set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
. "$SCRIPT_DIR/lib/wired_paths.sh"

ENGINE="${1:-${WIRED_BINARY:-}}"
if [ -z "${Q3DIR:-}" ] && [[ "$ENGINE" == *.app/Contents/MacOS/* ]]; then
  Q3DIR="$(cd "$(dirname "$ENGINE")/../.." && pwd)"
else
  Q3DIR="${Q3DIR:-$WIRED_INSTALL}"
fi
OUT="${WUI_BATCH_OUT:-$REPO_ROOT/build/wiredui-2d-batches}"
WATCHDOG="$SCRIPT_DIR/run-with-timeout.py"

[ -x "$ENGINE" ] || { echo "FAIL: engine not executable: $ENGINE"; exit 1; }
[ -f "$(dirname "$ENGINE")/wired_vulkan_arm64.dylib" ] || {
  echo "FAIL: packaged Vulkan renderer missing"; exit 1;
}

mkdir -p "$OUT"
RUN_ROOT="$(mktemp -d "$WIRED_TMP/q3now-wui-batches-XXXXXX")"
trap 'rm -rf "$RUN_ROOT"' EXIT
WIRED_TMP="$RUN_ROOT"
HOME_DIR="$(wired_isolated_home wiredui-2d-batches)"
CFG="$HOME_DIR/base/wiredui-2d-batches.cfg"
LOG="$OUT/wiredui-2d-batches.log"
REPORT="$OUT/report.json"

phase() {
  local name="$1"
  printf '%s\n' "echo WUI_BATCH_${name}_BEGIN" 'set r_speeds 8' 'wait 24' \
    'set r_speeds 0' "echo WUI_BATCH_${name}_END"
}

{
  printf '%s\n' \
    'set r_atmosphericGPU 0' 'set r_flares 0' 'set r_lens 0' \
    'set r_halos 0' 'set r_sunRayIntensity 0' 'set cg_lensFlare 0' \
    'set cg_missileFlare 0' 'set hud classic' 'map arena1' 'waitForMap' \
    'wait 100' 'cmd noclip' 'cmd setviewpos 1052 1432 50 135' \
    'log renderer.cmd info' 'set cg_draw2D 0' 'set cg_crosshairAlpha 0' 'wait 20'
  phase BASE
  printf '%s\n' 'set cg_draw2D 1' 'set cg_crosshairAlpha 0' 'wait 20'
  phase HUD
  printf '%s\n' 'set cg_crosshairAlpha 1' 'wait 20'
  phase CROSSHAIR
  printf '%s\n' '+scores' 'wait 20'
  phase SCOREBOARD
  printf '%s\n' '-scores' 'set r_speeds 0' 'quit'
} >"$CFG"

python3 "$WATCHDOG" --timeout 90 --kill-after 15 --cwd "$(dirname "$ENGINE")" --stdout "$LOG" -- \
  "$ENGINE" \
  +set fs_installpath "$Q3DIR" +set fs_homepath "$HOME_DIR" \
  +set cl_renderer vulkan +set net_port 0 +set com_automated 1 \
  +set com_noHardReboot 1 +set s_initsound 0 +set sv_pure 0 +set sv_cheats 1 \
  +set vm_game 0 +set vm_cgame 0 +set r_fullscreen 0 +set r_mode -1 \
  +set r_customwidth 1280 +set r_customheight 720 +set r_renderScale 1 \
  +set r_renderWidth 1280 +set r_renderHeight 720 +set r_hdr 0 \
  +set r_hdrDisplay 0 +set r_hdrAutoExposure 0 +set con_notifytime 0 \
  +exec wiredui-2d-batches.cfg

python3 - "$LOG" "$REPORT" <<'PY'
import json,re,statistics,sys
from pathlib import Path
log=Path(sys.argv[1]).read_text(errors="replace").splitlines()
out=Path(sys.argv[2])
phases=("BASE","HUD","CROSSHAIR","SCOREBOARD")
rx=re.compile(r"2d shaders:(\d+) white:(\d+) msdf:(\d+) other:(\d+) white-msdf-transitions:(\d+)")
active=None; rows={p:[] for p in phases}
for line in log:
    for p in phases:
        if f"WUI_BATCH_{p}_BEGIN" in line: active=p
        if f"WUI_BATCH_{p}_END" in line: active=None
    m=rx.search(line)
    if active and m: rows[active].append(tuple(map(int,m.groups())))
for p in phases:
    if len(rows[p])<8: raise SystemExit(f"FAIL: {p} has only {len(rows[p])} 2D samples")
med={p:tuple(int(statistics.median(v[i] for v in rows[p])) for i in range(5)) for p in phases}
base,hud,cross,score=(med[p] for p in phases)
# cg_draw2D=0 removes Wired's game HUD, but r_speeds itself and the common
# presentation path still emit measurable 2D work.  Treat this phase as the
# empirical non-HUD baseline; the contract is about bounded incremental cost.
if hud[0]<=base[0] or hud[2]<=0: raise SystemExit(f"FAIL: HUD phase has no measurable MSDF UI work: {hud}")
if cross[0]<hud[0] or cross[0]-hud[0]>3: raise SystemExit(f"FAIL: crosshair batch delta is not bounded: HUD={hud} CROSSHAIR={cross}")
if score[0]<=cross[0] or score[2]<=cross[2]: raise SystemExit(f"FAIL: scoreboard did not add measurable text work: {score}")
for p,v in med.items():
    if v[4]>max(0,v[0]-1): raise SystemExit(f"FAIL: impossible transition count for {p}: {v}")
increment=hud[0]-base[0]
transitions=hud[4]-base[4]
decision="negligible-close" if increment<=64 and transitions<=32 else "optimization-candidate"
data={"schemaVersion":1,"renderer":"vulkan","map":"arena1","logicalExtent":[1280,720],
      "columns":["total","white","msdf","other","whiteMsdfTransitions"],
      "samplesPerPhase":{p:len(rows[p]) for p in phases},"median":med,
      "baseline":"cg_draw2D=0 with r_speeds instrumentation still active",
      "hudIncrementVsUiOff":increment,"hudWhiteMsdfTransitions":transitions,"decision":decision}
out.write_text(json.dumps(data,indent=2,sort_keys=True)+"\n")
for p in phases: print(f"  {p:10s} total/white/msdf/other/transitions = {med[p]}")
print(f"PASS: HUD adds {increment} 2D batches and {transitions} white<->MSDF transitions; decision={decision}")
PY

echo "PASS retained artifacts: $OUT"
