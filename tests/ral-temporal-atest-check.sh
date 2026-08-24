#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Deterministic native-gamecl Vulkan admission gate for tx0 ATEST world draws.

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
TIMEOUT_RUNNER="$SCRIPT_DIR/run-with-timeout.py"
BOOT_CONTENT='set r_fullscreen 0
set r_mode -1
set r_customwidth 1280
set r_customheight 720
log renderer.temporal info
log renderer.ral info
set vm_game 0
set vm_cgame 0
set sv_cheats 1
set sv_pure 0
set activeAction "cg_thirdPerson 0; cg_drawGun 0; sv_fps 20; timescale 0.025; fixedtime 1; wait 512; noclip; setviewpos 888 -768 434 180 0; wait 64; echo Q3_RAL_ATEST_REQUESTED; r_temporalInputTest 1; wait 1; wait 1; ral_dump live temporal-motion-arm; wait 1; ral_dump live temporal; wait 1; ral_dump live temporal; wait 1; echo Q3_RAL_ATEST_ENABLED; r_temporalInputTest 0; fixedtime 0; timescale 1; echo Q3_RAL_ATEST_COMPLETE; quit"
map arena6
'
MEASURE_BOOT_CONTENT='set r_fullscreen 0
set r_mode -1
set r_customwidth 1280
set r_customheight 720
log renderer.temporal info
log renderer.ral info
log renderer.vk info
log renderer.timing debug
set vm_game 0
set vm_cgame 0
set sv_cheats 1
set sv_pure 0
set atestSetup "cg_thirdPerson 0; cg_drawGun 0; sv_fps 20; timescale 0.025; fixedtime 1; wait 512; noclip; setviewpos 888 -768 434 180 0; wait 64; vstr atestProof1"
set atestProof1 "echo Q3_RAL_ATEST_REQUESTED; r_temporalInputTest 1; wait 1; wait 1; ral_dump live temporal-motion-arm; wait 1; vstr atestProof2"
set atestProof2 "ral_dump live temporal; wait 1; ral_dump live temporal; wait 1; echo Q3_RAL_ATEST_ENABLED; set r_pinShaderTime 2.0; set r_pinFrameTime 2.0; r_temporalInputTest 2; wait 64; vstr atestClosed1"
set atestClosed1 "set r_gpuSpeeds 0; wait 4; set r_gpuSpeeds 1; echo Q3_ATEST_PERF_CLOSED_1_BEGIN; wait 220; echo Q3_ATEST_PERF_CLOSED_1_END; set r_gpuSpeeds 0; wait 16; vstr atestOpen1"
set atestOpen1 "r_temporalInputTest 1; wait 64; set r_gpuSpeeds 1; echo Q3_ATEST_PERF_OPEN_1_BEGIN; wait 220; echo Q3_ATEST_PERF_OPEN_1_END; set r_gpuSpeeds 0; wait 16; vstr atestOpen2"
set atestOpen2 "wait 64; set r_gpuSpeeds 1; echo Q3_ATEST_PERF_OPEN_2_BEGIN; wait 220; echo Q3_ATEST_PERF_OPEN_2_END; set r_gpuSpeeds 0; wait 16; vstr atestClosed2"
set atestClosed2 "r_temporalInputTest 2; wait 64; set r_gpuSpeeds 1; echo Q3_ATEST_PERF_CLOSED_2_BEGIN; wait 220; echo Q3_ATEST_PERF_CLOSED_2_END; set r_gpuSpeeds 0; r_temporalInputTest 1; fixedtime 0; timescale 1; echo Q3_RAL_ATEST_COMPLETE; quit"
set activeAction "vstr atestSetup"
map arena6
'

analyze_contract() {
	python3 - "$1" "$2" <<'PYEOF'
import json,re,sys
log_path,manifest_path=sys.argv[1:]
rows=[]
for number,line in enumerate(open(log_path,encoding="utf-8",errors="strict"),1):
 if not line.strip(): continue
 try: row=json.loads(line)
 except Exception as exc: raise SystemExit(f"FAIL ATEST JSON line {number}: {exc}")
 if not isinstance(row,dict) or not all(isinstance(row.get(k),str) for k in ("sev","cat","msg")):
  raise SystemExit("FAIL ATEST log schema")
 rows.append(row)
messages=[r["msg"].removesuffix("\n") for r in rows]
if any(r["sev"].upper() in ("ERROR","FATAL") for r in rows):
 raise SystemExit("FAIL ATEST ERROR/FATAL severity")
if any("VUID-" in m for m in messages): raise SystemExit("FAIL ATEST Vulkan VUID")
def exact(prefix,pattern,cat=None,count=None):
 found=[(i,r,m) for i,(r,m) in enumerate(zip(rows,messages)) if m.startswith(prefix)]
 if count is not None and len(found)!=count: raise SystemExit(f"FAIL ATEST {prefix} cardinality {len(found)}")
 for _,row,msg in found:
  if row["sev"].upper()!="INFO" or (cat and row["cat"].lower()!=cat) or not re.fullmatch(pattern,msg):
   raise SystemExit(f"FAIL ATEST {prefix} body/metadata: {msg}")
 return found
window=exact("window-extent ",r"window-extent schema=3 requested=1280x720 logical=1280x720 pixels=([1-9][0-9]*)x([1-9][0-9]*) publish-ready=1","client",1)[0]
wm=re.fullmatch(r"window-extent schema=3 requested=1280x720 logical=1280x720 pixels=([1-9][0-9]*)x([1-9][0-9]*) publish-ready=1",window[2])
pw,ph=map(int,wm.groups())
if pw<1280 or ph<720 or pw*9!=ph*16: raise SystemExit("FAIL ATEST window is not >=1280x720 16:9")
exact("cls.state: -> CA_ACTIVE (FIRST GAMEPLAY FRAME",r"cls\.state: -> CA_ACTIVE \(FIRST GAMEPLAY FRAME mapname=maps/arena6\.bsp serverTime=[0-9]+ numEntities=[0-9]+ framecount=[0-9]+\)","client",1)
markers=exact("Q3_RAL_ATEST_",r"Q3_RAL_ATEST_(?:REQUESTED|ENABLED|COMPLETE)","system",3)
if [x[2] for x in markers] != ["Q3_RAL_ATEST_REQUESTED","Q3_RAL_ATEST_ENABLED","Q3_RAL_ATEST_COMPLETE"]:
 raise SystemExit("FAIL ATEST marker order")
pat=re.compile(r"temporal-atest-admission schema=1 frame=([1-9][0-9]*) slot=([0-9]+) geometry=([12]) state=0x([0-9a-f]{8}) depth=1 outcome=write-valid")
writes=[]
for i,(row,msg) in enumerate(zip(rows,messages)):
 if not msg.startswith("temporal-atest-admission "): continue
 match=pat.fullmatch(msg)
 if match:
  if row["sev"].upper()!="INFO" or row["cat"].lower()!="renderer.ral": raise SystemExit("FAIL ATEST admission metadata")
  frame,slot,geometry,state=match.groups()
  writes.append((i,int(frame),int(slot),int(geometry),int(state,16)))
if len(writes)<2: raise SystemExit("FAIL ATEST needs two write-valid receipts")
cohorts={}
for receipt in writes: cohorts.setdefault(receipt[2],set()).add(receipt[1])
if not any(any(frame+1 in frames for frame in frames) for frames in cohorts.values()):
 raise SystemExit("FAIL ATEST no adjacent-frame stable slot witness")
activation_pat=re.compile(r"temporal-main-activation schema=1 token=[1-9][0-9]* frame=([1-9][0-9]*) world=0 extent=([1-9][0-9]*)x([1-9][0-9]*) topology=[1-9][0-9]* plan=[1-9][0-9]* materialization=[1-9][0-9]* target=[1-9][0-9]* layout=[1-9][0-9]* table=[1-9][0-9]* slot=[01] serial=[1-9][0-9]* segments=[1-9][0-9]* written=([1-9][0-9]*) invalidated=[0-9]+ preserved=[0-9]+ sequence=[0-9a-f]{16}:[0-9a-f]{16}:[1-9][0-9]* submit=1")
activations=[]
for row,msg in zip(rows,messages):
 match=activation_pat.fullmatch(msg)
 if match:
  if row["sev"].upper()!="INFO" or row["cat"].lower()!="renderer.ral": raise SystemExit("FAIL ATEST activation metadata")
  frame,w,h,written=map(int,match.groups())
  if w<1280 or h<720 or w*9!=h*16 or written<1: raise SystemExit("FAIL ATEST activation extent/write count")
  activations.append(frame)
write_frames={x[1] for x in writes}
if len(write_frames.intersection(activations))<2: raise SystemExit("FAIL ATEST admission/activation frame join")
manifest=[json.loads(line) for line in open(manifest_path,encoding="utf-8") if line.strip()]
results=[row for row in manifest if row.get("kind")=="result"]
if len(results)!=1 or results[0].get("rc")!=0 or results[0].get("timeout") is not False:
 raise SystemExit("FAIL ATEST process result")
print(f"PASS temporal ATEST runtime: {len(writes)} write-valid receipts, {pw}x{ph} pixels")
PYEOF
}

analyze_measurement() {
	python3 - "$1" <<'PYEOF'
import json,re,statistics,sys
from datetime import datetime
rows=[json.loads(line) for line in open(sys.argv[1],encoding="utf-8") if line.strip()]
msgs=[row["msg"].removesuffix("\n") for row in rows]
opaque=[]; atest=[]
for msg in msgs:
 m=re.fullmatch(r"shadow caster: ([0-9]+) verts / ([0-9]+) tris from ([0-9]+) world surfaces",msg)
 if m: opaque.append(tuple(map(int,m.groups())))
 m=re.fullmatch(r"shadow caster \(alpha-tested\): ([0-9]+) verts / ([0-9]+) tris from ([0-9]+) world surfaces",msg)
 if m: atest.append(tuple(map(int,m.groups())))
opaque=set(opaque); atest=set(atest)
if len(opaque)!=1 or len(atest)!=1: raise SystemExit(f"FAIL ATEST prevalence identity opaque={opaque} atest={atest}")
marker=re.compile(r"Q3_ATEST_PERF_(CLOSED|OPEN)_([12])_(BEGIN|END)")
header=re.compile(r"gpu \(([0-9]+)f avg, ms\):")
label=re.compile(r"  ([A-Za-z_]+)=([0-9.]+)")
buckets={}; active=None; current=None
for row,msg in zip(rows,msgs):
 mm=marker.fullmatch(msg)
 if mm:
  key=(mm.group(1).lower(),int(mm.group(2)))
  if mm.group(3)=="BEGIN":
   if active is not None or key in buckets: raise SystemExit("FAIL ATEST perf begin order")
   active=key; current={"begin":datetime.fromisoformat(row["ts"]),"gpu":[]}
  else:
   if active!=key or current is None: raise SystemExit("FAIL ATEST perf end order")
   current["end"]=datetime.fromisoformat(row["ts"]); buckets[key]=current
   active=None; current=None
  continue
 if active is None or row.get("cat","").lower()!="renderer.timing": continue
 hm=header.fullmatch(msg)
 if hm:
  current["gpu"].append({"frames":int(hm.group(1))})
  continue
 if current["gpu"]:
  lm=label.fullmatch(msg)
  if lm: current["gpu"][-1][lm.group(1)]=float(lm.group(2))
expected=[("closed",1),("open",1),("open",2),("closed",2)]
if list(buckets)!=expected: raise SystemExit(f"FAIL ATEST perf marker order {list(buckets)}")
for key,bucket in buckets.items():
 complete=[g for g in bucket["gpu"] if g.get("frames")==200 and "total" in g]
 if len(complete)!=1: raise SystemExit(f"FAIL ATEST perf GPU aggregate {key}: {complete}")
 bucket["total"]=complete[0]["total"]
 seconds=(bucket["end"]-bucket["begin"]).total_seconds()
 if seconds<=0: raise SystemExit("FAIL ATEST perf non-positive interval")
 bucket["fps"]=220.0/seconds
closed=[buckets[("closed",i)] for i in (1,2)]
opened=[buckets[("open",i)] for i in (1,2)]
cg=statistics.mean(x["total"] for x in closed); og=statistics.mean(x["total"] for x in opened)
cf=statistics.mean(x["fps"] for x in closed); of=statistics.mean(x["fps"] for x in opened)
gpu_delta=(og-cg)/cg*100.0; fps_delta=(of-cf)/cf*100.0
ov,ot,os=next(iter(opaque)); av,at,ass=next(iter(atest))
share=ass/(os+ass)*100.0
print(f"PASS temporal ATEST measurement: prevalence={ass}/{os+ass} surfaces ({share:.2f}%), tris={at}/{ot+at}; GPU closed={cg:.3f}ms open={og:.3f}ms delta={gpu_delta:+.2f}%; wall-FPS closed={cf:.1f} open={of:.1f} delta={fps_delta:+.2f}%")
PYEOF
}

if [ "${1:-}" = --analyze ] || [ "${1:-}" = --analyze-measure ]; then
	[ -f "${2:-}" ] && [ -f "${3:-}" ] || { echo "usage: $0 --analyze qconsole.jsonl manifest.jsonl"; exit 64; }
	analyze_contract "$2" "$3"
	[ "$1" = --analyze-measure ] && analyze_measurement "$2"
	exit $?
fi

MEASURE=0
if [ "${1:-}" = --measure ]; then MEASURE=1; shift; fi
WIRED="${1:-}"
[ -n "$WIRED" ] && [ -x "$WIRED" ] || { echo "usage: $0 /absolute/path/to/wired"; exit 64; }
[ "$(uname -s)" = Darwin ] || { echo "SKIP: runtime gate requires macOS/MoltenVK"; exit 77; }
[ -f "$TIMEOUT_RUNNER" ] || { echo "SKIP: missing timeout runner"; exit 77; }
WIRED="$(cd "$(dirname "$WIRED")" && pwd)/$(basename "$WIRED")"
WD="$(dirname "$WIRED")"
find_required() { local name="$1" candidate; shift; for candidate in "$@"; do [ -f "$candidate/$name" ] && { printf '%s\n' "$candidate/$name"; return 0; }; done; return 1; }
RENDERER="$(find_required wired_vulkan_arm64.dylib "$WD" "$WD/../MacOS")" || { echo "SKIP: current Vulkan renderer unavailable"; exit 77; }
GAMECL="$(find_required gameclarm64.dylib "$WD/base" "$WD/../Resources/base")" || { echo "SKIP: current native gamecl unavailable"; exit 77; }
GAMESV="$(find_required gamesvarm64.dylib "$WD/base" "$WD/../Resources/base")" || { echo "SKIP: current native gamesv unavailable"; exit 77; }
PAX21="$(find_required pax21.sw3z "$WD/base" "$WD/../Resources/base")" || { echo "SKIP: current pax21 unavailable"; exit 77; }
CONTENT="${WIRED_CONTENT_ROOT:-}"
[ -f "$CONTENT/base/pax01.sw3z" ] || { echo "SKIP: set WIRED_CONTENT_ROOT to current base content"; exit 77; }
BASE="$CONTENT/base/pax01.sw3z"
ROOT="$(mktemp -d -t ral-temporal-atest-XXXXXX 2>/dev/null || mktemp -d)"
HOME_DIR="$ROOT/home"
cleanup() { local status=$?; trap - EXIT INT TERM; [ "${WIRED_KEEP_ARTIFACTS:-0}" = 1 ] || rm -rf "$ROOT"; exit "$status"; }
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM
mkdir -p "$HOME_DIR/base"
cp "$PAX21" "$BASE" "$GAMECL" "$GAMESV" "$HOME_DIR/base/" || exit 1
BOOT="$HOME_DIR/base/ral-temporal-atest.cfg"
if [ "$MEASURE" -eq 1 ]; then printf '%s' "$MEASURE_BOOT_CONTENT" > "$BOOT"
else printf '%s' "$BOOT_CONTENT" > "$BOOT"; fi
MANIFEST="$ROOT/manifest.jsonl"
python3 - "$MANIFEST" "$WIRED" "$RENDERER" "$GAMECL" "$GAMESV" "$PAX21" "$BASE" "$0" "$BOOT" <<'PYEOF'
import hashlib,json,os,sys
with open(sys.argv[1],"w") as out:
 out.write(json.dumps({"kind":"scenario","schema":1,"name":"ral-temporal-atest","map":"arena6","window":"1280x720","camera":"888 -768 434 180 0"},sort_keys=True)+"\n")
 for role,path in zip(("gui","renderer","gamecl-native","gamesv-native","pax21","base","harness","bootstrap-cfg"),sys.argv[2:]):
  data=open(path,"rb").read();out.write(json.dumps({"kind":"provenance","role":role,"path":os.path.abspath(path),"bytes":len(data),"sha256":hashlib.sha256(data).hexdigest()},sort_keys=True)+"\n")
PYEOF
QCONSOLE="$HOME_DIR/qconsole.jsonl"
STDOUT="$ROOT/stdout.log"
python3 "$TIMEOUT_RUNNER" --timeout 120 --kill-after 15 --cwd "$WD" --stdout "$STDOUT" -- "$WIRED" +set fs_basepath "$HOME_DIR" +set fs_homepath "$HOME_DIR" +set fs_game base +set com_automated 1 +set com_noHardReboot 1 +set s_initsound 0 +set r_fullscreen 0 +set r_mode -1 +set r_customwidth 1280 +set r_customheight 720 +set r_vkValidate 0 +set r_entitySSBO 1 +set r_temporalInputTest 0 +set r_bloom 0 +set r_ssao 0 +set r_smaa 0 +set r_forwardPlus 0 +set r_drawSunRays 0 +set r_shadows "$MEASURE" +set r_depthFade 0 +set r_lens 0 +set r_gpuDecals 0 +set r_particles 0 +set r_atmosphericGPU 0 +set log_severity DEBUG +set log_file_severity DEBUG +set log_file_mode overwrite_synced +exec ral-temporal-atest.cfg
RC=$?
TIMEOUT=false
[ "$RC" -eq 124 ] && TIMEOUT=true
python3 - "$MANIFEST" "$$" "$RC" "$TIMEOUT" <<'PYEOF'
import json,sys
with open(sys.argv[1],"a") as out: out.write(json.dumps({"kind":"result","controller_pid":int(sys.argv[2]),"rc":int(sys.argv[3]),"timeout":sys.argv[4]=="true"},sort_keys=True)+"\n")
PYEOF
[ -f "$QCONSOLE" ] || { echo "FAIL missing qconsole; retained root: $ROOT"; WIRED_KEEP_ARTIFACTS=1; exit 1; }
analyze_contract "$QCONSOLE" "$MANIFEST" || { echo "FAIL retained root: $ROOT"; WIRED_KEEP_ARTIFACTS=1; exit 1; }
[ "$MEASURE" -eq 0 ] || analyze_measurement "$QCONSOLE" || { echo "FAIL retained root: $ROOT"; WIRED_KEEP_ARTIFACTS=1; exit 1; }
echo "PASS retained root: $ROOT"
