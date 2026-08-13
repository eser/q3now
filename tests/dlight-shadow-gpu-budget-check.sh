#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors
# SPDX-License-Identifier: GPL-3.0-or-later
#
# RAL-native GPU timestamp authority for the dynamic-light omni-shadow budget.
# Five fresh processes measure OFF plus K=1..4 under one fixed arena1 camera
# and four synthetic lights. This gate measures; it does not choose the ship
# default. That readability/default decision remains Eser-owned.

set -uo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

analyze_contract() {
python3 - "$@" <<'PYEOF'
import hashlib,json,math,os,re,statistics,sys
if len(sys.argv)!=7:raise SystemExit("usage: analyzer off k1 k2 k3 k4 manifest")
log_paths=sys.argv[1:6];manifest_path=sys.argv[6]
cases=("off","k1","k2","k3","k4")
expected={"off":(0,0,0),"k1":(1,1,6),"k2":(1,2,12),"k3":(1,3,18),"k4":(1,4,24)}
claimed=("DLS_GPU_","dlightShadowGpuProfile:","dlightShadowProfile:","cls.state: -> CA_ACTIVE (FIRST GAMEPLAY FRAME","----- Server Shutdown ","==== ShutdownGame ====")
def read(path,label):
 rows=[]
 for number,line in enumerate(open(path,encoding="utf-8",errors="strict"),1):
  if not line.strip():continue
  try:row=json.loads(line)
  except Exception as exc:raise SystemExit(f"FAIL dlight-gpu {label} JSON {number}: {exc}")
  if not isinstance(row,dict) or not all(isinstance(row.get(k),str) for k in ("sev","cat","msg")):raise SystemExit(f"FAIL dlight-gpu {label} schema")
  value=row["msg"][:-1] if row["msg"].endswith("\n") and not row["msg"].endswith("\n\n") else row["msg"]
  parts=re.split(r"[\r\n]",value)
  if len(parts)>1 and any(p.startswith(claimed) for p in parts):raise SystemExit(f"FAIL dlight-gpu {label} logical-line smuggling")
  rows.append((row,value))
 if not rows:raise SystemExit(f"FAIL dlight-gpu empty {label}")
 return rows
def exact(rows,prefix,pattern,sev,cat,label,count=1):
 found=[(i,r,v) for i,(r,v) in enumerate(rows) if v.startswith(prefix)]
 if len(found)!=count:raise SystemExit(f"FAIL dlight-gpu {label} cardinality {len(found)}")
 for _,r,v in found:
  if r["sev"].upper()!=sev or r["cat"].lower()!=cat or re.fullmatch(pattern,v) is None:raise SystemExit(f"FAIL dlight-gpu {label} body/metadata: {v}")
 return found
measurements={}
for case,path in zip(cases,log_paths):
 rows=read(path,case);vals=[v for _,v in rows]
 if any(r["sev"].upper() in ("ERROR","FATAL") for r,_ in rows):raise SystemExit(f"FAIL dlight-gpu {case} severity")
 if any("VUID" in v or "Validation Error" in v or "timed out" in v.lower() for v in vals):raise SystemExit(f"FAIL dlight-gpu {case} validation/timeout")
 first=exact(rows,"cls.state: -> CA_ACTIVE (FIRST GAMEPLAY FRAME",r"cls\.state: -> CA_ACTIVE \(FIRST GAMEPLAY FRAME mapname=maps/arena1\.bsp serverTime=\d+ numEntities=\d+ framecount=\d+\)","INFO","client",case+" FIRST")[0][0]
 markers=exact(rows,"DLS_GPU_",rf"DLS_GPU_{case.upper()}_(?:BEGIN|COMPLETE)","INFO","system",case+" marker vector",2)
 if [x[2] for x in markers] != [f"DLS_GPU_{case.upper()}_BEGIN",f"DLS_GPU_{case.upper()}_COMPLETE"]:raise SystemExit(f"FAIL dlight-gpu {case} marker order")
 begin,complete=markers[0][0],markers[1][0]
 shutdown=exact(rows,"----- Server Shutdown ",r"----- Server Shutdown \(Server quit\) -----","INFO","server",case+" shutdown")[0][0]
 game_shutdown=exact(rows,"==== ShutdownGame ====",r"==== ShutdownGame ====","INFO","game",case+" game shutdown")[0][0]
 a,k,p=expected[case]
 profiles=exact(rows,"dlightShadowGpuProfile:",rf"dlightShadowGpuProfile: source=ral-native-query samples=200 active={a} k={k} passes={p} gpu-ms=(?:0|[1-9]\d*)\.\d{{3}}","INFO","renderer.timing",case+" GPU profiles",3)
 if not first<begin<profiles[0][0]<profiles[-1][0]<complete<shutdown<game_shutdown:raise SystemExit(f"FAIL dlight-gpu {case} order")
 if case=="off":
  if any(v.startswith("dlightShadowProfile:") for v in vals):raise SystemExit("FAIL dlight-gpu OFF ran producer")
 else:
  cpu=[(i,r,v) for i,(r,v) in enumerate(rows) if v.startswith("dlightShadowProfile:")]
  if not cpu:raise SystemExit(f"FAIL dlight-gpu {case} missing CPU producer")
  if any(r["sev"].upper()!="WARN" or r["cat"].lower()!="renderer.timing" or re.fullmatch(rf"dlightShadowProfile: {k} lights x 6 = {p} passes/frame, \d+(?:\.\d+)? us/frame CPU-submit \(\d+ frames\)",v) is None for _,r,v in cpu):raise SystemExit(f"FAIL dlight-gpu {case} CPU producer body/metadata")
  if not first<cpu[0][0]<complete:raise SystemExit(f"FAIL dlight-gpu {case} CPU producer order")
 values=[float(re.search(r"gpu-ms=([0-9]+\.[0-9]{3})$",v).group(1)) for _,_,v in profiles]
 if any(not math.isfinite(v) or v<0 for v in values):raise SystemExit(f"FAIL dlight-gpu {case} non-finite result")
 measurements[case]=statistics.median(values)
off=measurements["off"]
if measurements["k1"] < off+0.020:raise SystemExit(f"FAIL dlight-gpu K1 not above OFF: {measurements}")
if measurements["k4"] < measurements["k1"]*1.5:raise SystemExit(f"FAIL dlight-gpu K4 does not expose 24-pass scaling: {measurements}")
manifest=[]
for number,line in enumerate(open(manifest_path,encoding="utf-8",errors="strict"),1):
 try:item=json.loads(line)
 except Exception as exc:raise SystemExit(f"FAIL dlight-gpu manifest JSON {number}: {exc}")
 if not isinstance(item,dict):raise SystemExit("FAIL dlight-gpu manifest schema")
 manifest.append(item)
if not manifest or manifest[0]!={"kind":"scenario","schema":1,"name":"dlight-shadow-gpu-budget","map":"arena1","cases":["off",1,2,3,4],"samples_per_window":200}:raise SystemExit("FAIL dlight-gpu scenario")
if [x.get("role") for x in manifest[1:-1]] != ["engine","renderer","pax01","pax21","harness"]:raise SystemExit("FAIL dlight-gpu provenance roles")
for item in manifest[1:-1]:
 if set(item)!={"kind","role","path","bytes","sha256"} or item.get("kind")!="provenance" or not isinstance(item.get("bytes"),int) or item["bytes"]<=0 or re.fullmatch(r"[0-9a-f]{64}",str(item.get("sha256",""))) is None:raise SystemExit("FAIL dlight-gpu provenance schema")
 data=open(item["path"],"rb").read()
 if len(data)!=item["bytes"] or hashlib.sha256(data).hexdigest()!=item["sha256"]:raise SystemExit("FAIL dlight-gpu provenance rehash")
result=manifest[-1]
if set(result)!={"kind","rc","controller_pid","timeout","forced"} or result.get("kind")!="result":raise SystemExit("FAIL dlight-gpu result schema")
if result["rc"]!={c:0 for c in cases} or any(not isinstance(result["controller_pid"].get(c),int) or result["controller_pid"][c]<=0 for c in cases) or len(set(result["controller_pid"].values()))!=5 or result["timeout"] is not False or result["forced"] is not False:raise SystemExit("FAIL dlight-gpu result")
print("PASS dlight-shadow RAL GPU budget " + " ".join(f"{c}={measurements[c]:.3f}ms" for c in cases))
PYEOF
}

write_self() {
python3 - "$@" <<'PYEOF'
import hashlib,json,os,re,sys
paths=sys.argv[1:6];manifest_path=sys.argv[6];mode=sys.argv[7]
cases=("off","k1","k2","k3","k4");values={"off":.010,"k1":.180,"k2":.340,"k3":.510,"k4":.680}
def row(sev,cat,msg):return {"sev":sev,"cat":cat,"msg":msg+"\n"}
logs={}
for case in cases:
 k=0 if case=="off" else int(case[1:]);p=k*6;a=0 if case=="off" else 1
 r=[row("INFO","client","cls.state: -> CA_ACTIVE (FIRST GAMEPLAY FRAME mapname=maps/arena1.bsp serverTime=600 numEntities=17 framecount=8)"),row("INFO","system",f"DLS_GPU_{case.upper()}_BEGIN")]
 if case!="off":r.append(row("WARN","renderer.timing",f"dlightShadowProfile: {k} lights x 6 = {p} passes/frame, 5.0 us/frame CPU-submit (60 frames)"))
 for d in (-.005,0,.005):r.append(row("INFO","renderer.timing",f"dlightShadowGpuProfile: source=ral-native-query samples=200 active={a} k={k} passes={p} gpu-ms={values[case]+d:.3f}"))
 r += [row("INFO","system",f"DLS_GPU_{case.upper()}_COMPLETE"),row("INFO","server","----- Server Shutdown (Server quit) -----"),row("INFO","game","==== ShutdownGame ====")]
 logs[case]=r
def find(case,text,n=0):return [i for i,r in enumerate(logs[case]) if text in r["msg"]][n]
if mode=="wrong-source":logs["k1"][find("k1","GpuProfile")]["msg"]=logs["k1"][find("k1","GpuProfile")]["msg"].replace("ral-native-query","legacy-query-bridge")
elif mode=="wrong-k":logs["k2"][find("k2","GpuProfile")]["msg"]=logs["k2"][find("k2","GpuProfile")]["msg"].replace("k=2 passes=12","k=1 passes=6")
elif mode=="wrong-passes":logs["k4"][find("k4","GpuProfile")]["msg"]=logs["k4"][find("k4","GpuProfile")]["msg"].replace("passes=24","passes=18")
elif mode=="missing-sample":logs["k3"].pop(find("k3","GpuProfile",2))
elif mode=="profile-early":
 i=find("k1","GpuProfile");logs["k1"].insert(1,logs["k1"].pop(i))
elif mode=="metadata":logs["k1"][find("k1","GpuProfile")]["cat"]="renderer.cmd"
elif mode=="vuid":logs["k2"].append(row("ERROR","renderer.ral","Validation Error: VUID-synthetic"))
elif mode=="off-producer":logs["off"].insert(3,row("WARN","renderer.timing","dlightShadowProfile: 1 lights x 6 = 6 passes/frame, 1.0 us/frame CPU-submit (60 frames)"))
elif mode=="no-scaling":
 for r in logs["k4"]:
  if "GpuProfile" in r["msg"]:r["msg"]=re.sub(r"gpu-ms=[0-9.]+", "gpu-ms=0.200", r["msg"])
elif mode=="duplicate-marker":logs["k1"].insert(3,row("INFO","system","DLS_GPU_K1_BEGIN"))
elif mode=="smuggle":logs["k2"].append(row("INFO","system","benign\rDLS_GPU_K2_BEGIN"))
elif mode=="wrong-first":
 i=find("k3","FIRST GAMEPLAY FRAME");logs["k3"][i]["msg"]=logs["k3"][i]["msg"].replace("arena1","arena7")
for path,case in zip(paths,cases):
 with open(path,"w") as out:
  for item in logs[case]:out.write(json.dumps(item)+"\n")
items=[{"kind":"scenario","schema":1,"name":"dlight-shadow-gpu-budget","map":"arena1","cases":["off",1,2,3,4],"samples_per_window":200}]
for role in ("engine","renderer","pax01","pax21","harness"):
 path=manifest_path+"."+role;data=("fixture-"+role).encode();open(path,"wb").write(data);items.append({"kind":"provenance","role":role,"path":os.path.abspath(path),"bytes":len(data),"sha256":hashlib.sha256(data).hexdigest()})
items.append({"kind":"result","rc":{c:0 for c in cases},"controller_pid":{c:100+i for i,c in enumerate(cases)},"timeout":False,"forced":False})
if mode=="manifest":items[2]["sha256"]="0"*64
elif mode=="result":items[-1]["rc"]["k4"]=1
with open(manifest_path,"w") as out:
 for item in items:out.write(json.dumps(item,sort_keys=True)+"\n")
PYEOF
}

if [ "${1:-}" = --analyze ];then [ "$#" -eq 7 ] || exit 64;analyze_contract "$2" "$3" "$4" "$5" "$6" "$7";exit $?;fi
if [ "${1:-}" = --self-test ];then
 ROOT="$(mktemp -d -t dlight-gpu-self-XXXXXX 2>/dev/null || mktemp -d)";trap 'rm -rf "$ROOT"' EXIT
 args=("$ROOT/off" "$ROOT/k1" "$ROOT/k2" "$ROOT/k3" "$ROOT/k4" "$ROOT/manifest")
 write_self "${args[@]}" clean || exit 1;analyze_contract "${args[@]}" >/dev/null || exit 1
 defects=(wrong-source wrong-k wrong-passes missing-sample profile-early metadata vuid off-producer no-scaling duplicate-marker smuggle wrong-first manifest result)
 for defect in "${defects[@]}";do local_args=("$ROOT/$defect.off" "$ROOT/$defect.k1" "$ROOT/$defect.k2" "$ROOT/$defect.k3" "$ROOT/$defect.k4" "$ROOT/$defect.manifest");write_self "${local_args[@]}" "$defect" || exit 1;if analyze_contract "${local_args[@]}" >/dev/null 2>&1;then echo "FAIL accepted $defect";exit 1;fi;done
 echo "PASS dlight-shadow GPU-budget analyzer self-test (${#defects[@]} mutations)";exit 0
fi

ENGINE="${1:-}";[ -n "$ENGINE" ] && [ -x "$ENGINE" ] || { echo "usage: $0 /absolute/path/to/wired";exit 64; }
ENGINE="$(cd "$(dirname "$ENGINE")" && pwd)/$(basename "$ENGINE")";ENGINE_DIR="$(dirname "$ENGINE")"
RESOURCE_ROOT="${WIRED_CONTENT_ROOT:-$ENGINE_DIR/Contents/Resources}"
PAX01="$RESOURCE_ROOT/base/pax01.sw3z";PAX21="$RESOURCE_ROOT/base/pax21.sw3z"
[ -s "$PAX01" ] && [ -s "$PAX21" ] || { echo "SKIP: exact pax01+pax21 unavailable under $RESOURCE_ROOT";exit 77; }
RENDERER="${WIRED_RENDERER:-$ENGINE_DIR/Contents/MacOS/wired_vulkan_arm64.dylib}";[ -s "$RENDERER" ] || { echo "SKIP: renderer unavailable: $RENDERER";exit 77; }
ROOT="$(mktemp -d -t dlight-gpu-budget-XXXXXX 2>/dev/null || mktemp -d)";trap '[ "${WIRED_KEEP_ARTIFACTS:-0}" = 1 ] || rm -rf "$ROOT"' EXIT
MANIFEST="$ROOT/manifest.jsonl"
python3 - "$MANIFEST" "$ENGINE" "$RENDERER" "$PAX01" "$PAX21" "$0" <<'PYEOF'
import hashlib,json,os,sys
with open(sys.argv[1],"w") as out:
 out.write(json.dumps({"kind":"scenario","schema":1,"name":"dlight-shadow-gpu-budget","map":"arena1","cases":["off",1,2,3,4],"samples_per_window":200},sort_keys=True)+"\n")
 for role,path in zip(("engine","renderer","pax01","pax21","harness"),sys.argv[2:]):
  path=os.path.abspath(path);data=open(path,"rb").read();out.write(json.dumps({"kind":"provenance","role":role,"path":path,"bytes":len(data),"sha256":hashlib.sha256(data).hexdigest()},sort_keys=True)+"\n")
PYEOF
declare -A RCS PIDS
TIMEOUT=0;FORCED=0
for case in off k1 k2 k3 k4;do
 home="$ROOT/$case/home";mkdir -p "$home/base";cp "$PAX01" "$PAX21" "$home/base/" || exit 1
 if [ "$case" = off ];then enabled=0;k=1;expected_k=0;else enabled=1;k="${case#k}";expected_k="$k";fi
 cfg="$home/base/dlight-gpu.cfg"
 {
  printf 'log renderer.timing debug\n'
  printf 'set r_gpuSpeeds 0\nset r_forwardPlus 1\nset r_pbr 0\nset r_parallaxMapping 0\nset r_ssao 0\n'
  printf 'set r_dlightShadows %s\nset r_dlightShadowK %s\n' "$enabled" "$k"
  printf 'set r_dlightShadowTest 800\nset r_dlightShadowTestN 4\nset r_dlightShadowCount 0\nset r_dlightShadowProfile 1\n'
  printf 'map arena1\nwaitForMap\nwait 120\ncmd noclip\nwait 20\ncmd setviewpos 900 1432 50 96\nwait 120\n'
  printf 'set r_gpuSpeeds 1\necho DLS_GPU_%s_BEGIN\nwait 650\necho DLS_GPU_%s_COMPLETE\nquit\n' "$(printf '%s' "$case"|tr '[:lower:]' '[:upper:]')" "$(printf '%s' "$case"|tr '[:lower:]' '[:upper:]')"
 } >"$cfg"
 stdout="$ROOT/$case/stdout.log"
 (cd "$ENGINE_DIR" && timeout -s KILL -k 10 120 "$ENGINE" +set fs_homepath "$home" +set sv_cheats 1 +set sv_pure 0 +set vm_game 0 +set vm_cgame 0 +set r_mode -1 +set r_customwidth 1280 +set r_customheight 720 +set r_fullscreen 0 +set con_notifytime 0 +set com_automated 1 +log renderer.ral info +set r_forwardPlus 1 +set r_ssao 0 +set r_dlightShadows "$enabled" +set r_dlightShadowK "$k" +set r_gpuSpeeds 0 +exec dlight-gpu.cfg >"$stdout" 2>&1) &
 pid=$!;PIDS[$case]=$pid;wait "$pid";rc=$?;RCS[$case]=$rc
 [ "$rc" -eq 124 ] || [ "$rc" -eq 137 ] && TIMEOUT=1
 [ "$rc" -eq 137 ] && FORCED=1
 [ -s "$home/qconsole.jsonl" ] || { echo "FAIL $case: no qconsole";exit 1; }
 cp "$home/qconsole.jsonl" "$ROOT/$case/qconsole.jsonl"
 cmp -s "$PAX01" "$home/base/pax01.sw3z" && cmp -s "$PAX21" "$home/base/pax21.sw3z" || { echo "FAIL $case: staged pax drift";exit 1; }
 echo "dlight-shadow GPU case $case (enabled=$enabled K=$expected_k) rc=$rc root=$ROOT/$case"
done
python3 - "$MANIFEST" "${RCS[off]}" "${RCS[k1]}" "${RCS[k2]}" "${RCS[k3]}" "${RCS[k4]}" "${PIDS[off]}" "${PIDS[k1]}" "${PIDS[k2]}" "${PIDS[k3]}" "${PIDS[k4]}" "$TIMEOUT" "$FORCED" <<'PYEOF'
import json,sys
p=sys.argv[1];cases=("off","k1","k2","k3","k4");vals=list(map(int,sys.argv[2:]));item={"kind":"result","rc":dict(zip(cases,vals[:5])),"controller_pid":dict(zip(cases,vals[5:10])),"timeout":bool(vals[10]),"forced":bool(vals[11])}
with open(p,"a") as out:out.write(json.dumps(item,sort_keys=True)+"\n")
PYEOF
analyze_contract "$ROOT/off/qconsole.jsonl" "$ROOT/k1/qconsole.jsonl" "$ROOT/k2/qconsole.jsonl" "$ROOT/k3/qconsole.jsonl" "$ROOT/k4/qconsole.jsonl" "$MANIFEST" || {
	echo "FAIL retained root: $ROOT"
	exit 1
}
echo "retained root: $ROOT"
