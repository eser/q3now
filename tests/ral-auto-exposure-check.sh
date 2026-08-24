#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Deterministic native Vulkan/RAL auto-exposure rate and bright-transition gate.

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
TIMEOUT_RUNNER="$SCRIPT_DIR/run-with-timeout.py"
BOOT_CONTENT='set r_fullscreen 0
set r_mode -1
set r_customwidth 1280
set r_customheight 720
log renderer.ral warn
set vm_game 0
set vm_cgame 0
set sv_cheats 1
set sv_pure 0
set aeSetup "cg_thirdPerson 0; cg_drawGun 0; com_maxfps 60; fixedtime 16; wait 180; noclip; setviewpos 1052 1432 50 135; vstr aeLegacySettle"
set aeLegacySettle "r_hdrAdaptionRateUp 3.0; r_hdrAdaptionRateDown 1.0; wait 360; echo Q3_AE_LEGACY_FLICKER_BEGIN; wait 300; echo Q3_AE_LEGACY_FLICKER_END; vstr aeLegacyAB"
set aeLegacyAB "r_pinShaderTime 2.0; wait 120; echo Q3_AE_LEGACY_AB_BEGIN; setviewpos 1052 1432 300 0; wait 300; echo Q3_AE_LEGACY_AB_END; vstr aeLegacyBA"
set aeLegacyBA "echo Q3_AE_LEGACY_BA_BEGIN; setviewpos 1052 1432 50 135; wait 300; echo Q3_AE_LEGACY_BA_END; r_pinShaderTime 0; vstr aeMiddleSettle"
set aeMiddleSettle "r_hdrAdaptionRateUp 2.0; r_hdrAdaptionRateDown 0.75; wait 360; echo Q3_AE_MIDDLE_FLICKER_BEGIN; wait 300; echo Q3_AE_MIDDLE_FLICKER_END; vstr aeMiddleAB"
set aeMiddleAB "r_pinShaderTime 2.0; wait 120; echo Q3_AE_MIDDLE_AB_BEGIN; setviewpos 1052 1432 300 0; wait 300; echo Q3_AE_MIDDLE_AB_END; vstr aeMiddleBA"
set aeMiddleBA "echo Q3_AE_MIDDLE_BA_BEGIN; setviewpos 1052 1432 50 135; wait 300; echo Q3_AE_MIDDLE_BA_END; r_pinShaderTime 0; vstr aeCurrentSettle"
set aeCurrentSettle "r_hdrAdaptionRateUp 1.5; r_hdrAdaptionRateDown 0.5; wait 360; echo Q3_AE_CURRENT_FLICKER_BEGIN; wait 300; echo Q3_AE_CURRENT_FLICKER_END; vstr aeCurrentAB"
set aeCurrentAB "r_pinShaderTime 2.0; wait 120; echo Q3_AE_CURRENT_AB_BEGIN; setviewpos 1052 1432 300 0; wait 300; echo Q3_AE_CURRENT_AB_END; vstr aeCurrentBA"
set aeCurrentBA "echo Q3_AE_CURRENT_BA_BEGIN; setviewpos 1052 1432 50 135; wait 300; echo Q3_AE_CURRENT_BA_END; fixedtime 0; echo Q3_AE_COMPLETE; quit"
set activeAction "vstr aeSetup"
map arena1
'

analyze() {
	python3 - "$1" "$2" <<'PYEOF'
import json,re,statistics,sys

log_path,manifest_path=sys.argv[1:]
rows=[]
for number,line in enumerate(open(log_path,encoding="utf-8",errors="strict"),1):
    if not line.strip(): continue
    try: row=json.loads(line)
    except Exception as exc: raise SystemExit(f"FAIL exposure JSON line {number}: {exc}")
    if not isinstance(row,dict) or not all(isinstance(row.get(k),str) for k in ("sev","cat","msg")):
        raise SystemExit("FAIL exposure log schema")
    rows.append(row)
messages=[row["msg"].removesuffix("\n") for row in rows]
if any(row["sev"].upper() in ("ERROR","FATAL") for row in rows):
    raise SystemExit("FAIL exposure ERROR/FATAL severity")
if any("VUID-" in msg for msg in messages):
    raise SystemExit("FAIL exposure Vulkan VUID")

window=re.compile(r"window-extent schema=3 requested=1280x720 logical=1280x720 pixels=([1-9][0-9]*)x([1-9][0-9]*) publish-ready=1")
windows=[window.fullmatch(msg) for msg in messages if msg.startswith("window-extent ")]
if len(windows)!=1: raise SystemExit(f"FAIL exposure window receipt count {len(windows)}")
pw,ph=map(int,windows[0].groups())
if pw<1280 or ph<720 or pw*9!=ph*16:
    raise SystemExit(f"FAIL exposure window {pw}x{ph} is not >=1280x720 16:9")

series_re=re.compile(r"EXPSERIES frame=([0-9]+) exposure_bias=([0-9]+(?:\.[0-9]+)?)")
marker_re=re.compile(r"Q3_AE_(LEGACY|MIDDLE|CURRENT)_(FLICKER|AB|BA)_(BEGIN|END)|Q3_AE_COMPLETE")
series=[]; markers=[]
for index,(row,msg) in enumerate(zip(rows,messages)):
    match=series_re.fullmatch(msg)
    if match:
        if row["cat"].lower()!="renderer.ral" or row["sev"].upper()!="WARN":
            raise SystemExit("FAIL exposure telemetry metadata")
        series.append((index,int(match.group(1)),float(match.group(2))))
    match=marker_re.fullmatch(msg)
    if match:
        if row["cat"].lower()!="system" or row["sev"].upper()!="INFO":
            raise SystemExit("FAIL exposure marker metadata")
        markers.append((index,msg))
if not series: raise SystemExit("FAIL exposure telemetry missing (Debug renderer required)")
if not markers or markers[-1][1]!="Q3_AE_COMPLETE": raise SystemExit("FAIL exposure completion marker")
expected=[]
for rate in ("LEGACY","MIDDLE","CURRENT"):
    for phase in ("FLICKER","AB","BA"):
        expected.extend((f"Q3_AE_{rate}_{phase}_BEGIN",f"Q3_AE_{rate}_{phase}_END"))
expected.append("Q3_AE_COMPLETE")
if [msg for _,msg in markers] != expected: raise SystemExit("FAIL exposure marker order/cardinality")
if any(b[1] != a[1]+1 for a,b in zip(series,series[1:])):
    raise SystemExit("FAIL exposure frame sequence is not contiguous")

marker_at={msg:index for index,msg in markers}
def values(rate,phase):
    begin=f"Q3_AE_{rate}_{phase}_BEGIN"; end=f"Q3_AE_{rate}_{phase}_END"
    if begin not in marker_at or end not in marker_at or marker_at[begin]>=marker_at[end]:
        raise SystemExit(f"FAIL exposure marker pair {rate}/{phase}")
    vals=[value for index,_,value in series if marker_at[begin] < index < marker_at[end]]
    if len(vals)<250: raise SystemExit(f"FAIL exposure short sample {rate}/{phase}: {len(vals)}")
    return vals

def quantile(vals,p):
    ordered=sorted(vals); pos=(len(ordered)-1)*p; lo=int(pos); hi=min(lo+1,len(ordered)-1)
    return ordered[lo]+(ordered[hi]-ordered[lo])*(pos-lo)
def metrics(vals):
    median=statistics.median(vals)
    return {"n":len(vals),"median":median,"swing":quantile(vals,.95)-quantile(vals,.05),
            "relative":(quantile(vals,.95)-quantile(vals,.05))/median if median else 0.0,
            "start":statistics.median(vals[:16]),"end":statistics.median(vals[-64:]),
            "minimum":min(vals),"maximum":max(vals)}

results={}
for rate in ("LEGACY","MIDDLE","CURRENT"):
    results[rate]={phase:metrics(values(rate,phase)) for phase in ("FLICKER","AB","BA")}

for rate,phases in results.items():
    for phase,metric in phases.items():
        if metric["minimum"] < 0.25 or metric["maximum"] > 8.0:
            raise SystemExit(f"FAIL exposure clamp {rate}/{phase}: {metric['minimum']}..{metric['maximum']}")
current=results["CURRENT"]; legacy=results["LEGACY"]
if current["FLICKER"]["relative"] > 0.008:
    raise SystemExit(f"FAIL exposure current flicker {current['FLICKER']['relative']*100:.2f}% > 0.80%")
if legacy["FLICKER"]["relative"] < current["FLICKER"]["relative"] * 1.25:
    raise SystemExit("FAIL exposure current defaults do not improve legacy flicker by >=20%")

def progress(metric,target):
    step=target-metric["start"]
    return (metric["end"]-metric["start"])/step if abs(step)>1e-6 else 1.0
target_b=legacy["AB"]["end"]; target_a=legacy["BA"]["end"]
if not (0.90 <= target_b <= 1.02 and 1.65 <= target_a <= 1.75):
    raise SystemExit(f"FAIL exposure pinned endpoint golden B={target_b:.5f} A={target_a:.5f}")
if progress(current["AB"],target_b) < 0.85 or progress(current["BA"],target_a) < 0.95:
    raise SystemExit("FAIL exposure current transition convergence")
for phase,target in (("AB",target_b),("BA",target_a)):
    metric=current[phase]; margin=abs(target-metric["start"])*0.03
    lo=min(metric["start"],target)-margin; hi=max(metric["start"],target)+margin
    if metric["minimum"] < lo or metric["maximum"] > hi:
        raise SystemExit(f"FAIL exposure current {phase} overshoot/ringing")

manifest=[json.loads(line) for line in open(manifest_path,encoding="utf-8") if line.strip()]
process=[row for row in manifest if row.get("kind")=="result"]
if len(process)!=1 or process[0].get("rc")!=0 or process[0].get("timeout") is not False:
    raise SystemExit("FAIL exposure process result")

for rate in ("LEGACY","MIDDLE","CURRENT"):
    flick=results[rate]["FLICKER"]
    ab=results[rate]["AB"]; ba=results[rate]["BA"]
    print(f"MEASURE {rate.lower()}: flicker p90={flick['swing']:.5f} ({flick['relative']*100:.2f}%) "
          f"AB={ab['start']:.5f}->{ab['end']:.5f} range={ab['minimum']:.5f}..{ab['maximum']:.5f} "
          f"BA={ba['start']:.5f}->{ba['end']:.5f} range={ba['minimum']:.5f}..{ba['maximum']:.5f}")
print(f"PASS auto-exposure runtime: {pw}x{ph} pixels; current flicker={current['FLICKER']['relative']*100:.2f}% "
      f"legacy={legacy['FLICKER']['relative']*100:.2f}%; pinned B={target_b:.5f} A={target_a:.5f}")
PYEOF
}

if [ "${1:-}" = --analyze ]; then
	[ -f "${2:-}" ] && [ -f "${3:-}" ] || { echo "usage: $0 --analyze qconsole.jsonl manifest.jsonl"; exit 64; }
	analyze "$2" "$3"
	exit $?
fi

WIRED="${1:-}"
[ -n "$WIRED" ] && [ -x "$WIRED" ] || { echo "usage: $0 /absolute/path/to/wired"; exit 64; }
[ "$(uname -s)" = Darwin ] || { echo "SKIP: runtime gate requires macOS/MoltenVK"; exit 77; }
[ -f "$TIMEOUT_RUNNER" ] || { echo "SKIP: missing timeout runner"; exit 77; }
WIRED="$(cd "$(dirname "$WIRED")" && pwd)/$(basename "$WIRED")"
WD="$(dirname "$WIRED")"
find_required() { local name="$1" candidate; shift; for candidate in "$@"; do [ -f "$candidate/$name" ] && { printf '%s\n' "$candidate/$name"; return 0; }; done; return 1; }
RENDERER="$(find_required wired_vulkan_arm64.dylib "$WD" "$WD/../MacOS")" || { echo "SKIP: current Vulkan renderer unavailable"; exit 77; }
GAMECL="$(find_required gameclarm64.dylib "$WD/base" "$WD/../Resources/base" "$WD/../../../base")" || { echo "SKIP: current native gamecl unavailable"; exit 77; }
GAMESV="$(find_required gamesvarm64.dylib "$WD/base" "$WD/../Resources/base" "$WD/../../../base")" || { echo "SKIP: current native gamesv unavailable"; exit 77; }
PAX21="$(find_required pax21.sw3z "$WD/base" "$WD/../Resources/base" "$WD/../../../base")" || { echo "SKIP: current pax21 unavailable"; exit 77; }
CONTENT="${WIRED_CONTENT_ROOT:-}"
[ -f "$CONTENT/base/pax01.sw3z" ] || { echo "SKIP: set WIRED_CONTENT_ROOT to current base content"; exit 77; }
BASE="$CONTENT/base/pax01.sw3z"
ROOT="$(mktemp -d -t ral-auto-exposure-XXXXXX 2>/dev/null || mktemp -d)"
HOME_DIR="$ROOT/home"
cleanup() { local status=$?; trap - EXIT INT TERM; [ "${WIRED_KEEP_ARTIFACTS:-0}" = 1 ] || rm -rf "$ROOT"; exit "$status"; }
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM
mkdir -p "$HOME_DIR/base"
cp "$PAX21" "$BASE" "$GAMECL" "$GAMESV" "$HOME_DIR/base/" || exit 1
BOOT="$HOME_DIR/base/ral-auto-exposure.cfg"
printf '%s' "$BOOT_CONTENT" > "$BOOT"
MANIFEST="$ROOT/manifest.jsonl"
python3 - "$MANIFEST" "$WIRED" "$RENDERER" "$GAMECL" "$GAMESV" "$PAX21" "$BASE" "$0" "$BOOT" <<'PYEOF'
import hashlib,json,os,sys
with open(sys.argv[1],"w") as out:
    out.write(json.dumps({"kind":"scenario","schema":1,"name":"ral-auto-exposure","map":"arena1","window":"1280x720","cameras":["1052 1432 50 135","1052 1432 300 0"]},sort_keys=True)+"\n")
    for role,path in zip(("gui","renderer","gamecl-native","gamesv-native","pax21","base","harness","bootstrap-cfg"),sys.argv[2:]):
        data=open(path,"rb").read(); out.write(json.dumps({"kind":"provenance","role":role,"path":os.path.abspath(path),"bytes":len(data),"sha256":hashlib.sha256(data).hexdigest()},sort_keys=True)+"\n")
PYEOF
QCONSOLE="$HOME_DIR/qconsole.jsonl"
STDOUT="$ROOT/stdout.log"
python3 "$TIMEOUT_RUNNER" --timeout 180 --kill-after 15 --cwd "$WD" --stdout "$STDOUT" -- "$WIRED" \
	+set fs_basepath "$HOME_DIR" +set fs_homepath "$HOME_DIR" +set fs_game base \
	+set com_automated 1 +set com_noHardReboot 1 +set s_initsound 0 \
	+set r_fullscreen 0 +set r_mode -1 +set r_customwidth 1280 +set r_customheight 720 \
	+set r_vkValidate 0 +set r_hdr 1 +set r_fbo 1 +set r_hdrAutoExposure 1 \
	+set r_hdrHistogramDebug 2 +set r_brightness 1 +set r_dither 0 \
	+set r_bloom 1 +set r_ssao 0 +set r_smaa 0 +set r_forwardPlus 0 \
	+set r_drawSunRays 0 +set r_shadows 0 +set r_depthFade 0 +set r_lens 0 \
	+set r_gpuDecals 0 +set r_particles 0 +set r_atmosphericGPU 0 \
	+set log_severity DEBUG +set log_file_severity DEBUG +set log_file_mode overwrite_synced \
	+exec ral-auto-exposure.cfg
RC=$?
TIMEOUT=false
[ "$RC" -eq 124 ] && TIMEOUT=true
python3 - "$MANIFEST" "$$" "$RC" "$TIMEOUT" <<'PYEOF'
import json,sys
with open(sys.argv[1],"a") as out: out.write(json.dumps({"kind":"result","controller_pid":int(sys.argv[2]),"rc":int(sys.argv[3]),"timeout":sys.argv[4]=="true"},sort_keys=True)+"\n")
PYEOF
[ -f "$QCONSOLE" ] || { echo "FAIL missing qconsole; retained root: $ROOT"; WIRED_KEEP_ARTIFACTS=1; exit 1; }
analyze "$QCONSOLE" "$MANIFEST" || { echo "FAIL retained root: $ROOT"; WIRED_KEEP_ARTIFACTS=1; exit 1; }
echo "PASS retained root: $ROOT"
