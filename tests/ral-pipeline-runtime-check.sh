#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors
# Exact-process authority for the historical `ral_pipeline_test` command.

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
TIMEOUT_RUNNER="$SCRIPT_DIR/run-with-timeout.py"

analyze_contract() {
python3 - "$1" "$2" <<'PYEOF'
import hashlib,json,os,re,sys
log_path,manifest_path=sys.argv[1:]
rows=[]
for number,line in enumerate(open(log_path,encoding="utf-8",errors="strict"),1):
 if not line.strip():continue
 try:row=json.loads(line)
 except Exception as exc:raise SystemExit(f"FAIL ral-pipeline JSON {number}: {exc}")
 if not isinstance(row,dict) or not all(isinstance(row.get(k),str) for k in ("ts","sev","cat","msg")):raise SystemExit("FAIL ral-pipeline log schema")
 rows.append(row)
if not rows:raise SystemExit("FAIL ral-pipeline empty evidence")
def norm(value):return value[:-1] if value.endswith("\n") and not value.endswith("\n\n") else value
vals=[norm(row["msg"]) for row in rows]
claimed=("ral_pipeline_test:","===== RAL dump ","Vulkan backend ready:","===== RAL pipeline test ","  pipelineCache present:","  layout-cache share:","  draw:","  compute:","  residency acquire:","  residency view:","  pipeline cache:","  teardown:","===== end RAL pipeline test =====","Ral_DestroyBackend:","===== end RAL dump =====","Q0_RAL_PIPELINE_","----- Server Shutdown ","==== ShutdownGame ====")
for value in vals:
 parts=re.split(r"[\r\n]",value)
 if len(parts)>1 and any(part.startswith(claimed) for part in parts):raise SystemExit("FAIL ral-pipeline claimed logical-line smuggling")
if any(row["sev"].upper() in ("ERROR","FATAL") for row in rows):raise SystemExit("FAIL ral-pipeline severity")
for value in vals:
 if ("VUID-" in value or "Rendering is now exclusively on the legacy VkPipeline path" in value or
     value.startswith("Ral_CreateBackend failed") or "VK_KHR_surface entry points unavailable" in value or
     value.startswith("  draw: readback map failed") or value.startswith("  draw: resource creation failed") or
     value.startswith("  draw: command buffer / fence acquisition failed") or
     value.startswith("  compute: ") and ("mismatch" in value or "failed" in value) or
     value.startswith("  residency view: ") and "setup failed" in value or
     value.startswith("  residency view: readback map failed")):
  raise SystemExit(f"FAIL ral-pipeline forbidden marker: {value}")
def exact(prefix,pattern,sev="INFO",cat="renderer.ral",count=1):
 found=[(i,row,value) for i,(row,value) in enumerate(zip(rows,vals)) if value.startswith(prefix)]
 if len(found)!=count:raise SystemExit(f"FAIL ral-pipeline {prefix} cardinality {len(found)}")
 for _,row,value in found:
  if row["sev"].upper()!=sev or row["cat"].lower()!=cat or re.fullmatch(pattern,value) is None:
   raise SystemExit(f"FAIL ral-pipeline {prefix} body/metadata: {value}")
 return found
first=exact("cls.state: -> CA_ACTIVE (FIRST GAMEPLAY FRAME",r"cls\.state: -> CA_ACTIVE \(FIRST GAMEPLAY FRAME mapname=maps/arena7\.bsp serverTime=[0-9]+ numEntities=[0-9]+ framecount=[0-9]+\)","INFO","client")[0]
receipt=exact("ral_pipeline_test:",r"ral_pipeline_test: running exact offscreen RAL pipeline exercise")[0]
dump=exact("===== RAL dump ",r"===== RAL dump \(Phase 7\.3c Vulkan: instance/device/resources/async/pipeline\) =====")[0]
backend=exact("Vulkan backend ready:",r"Vulkan backend ready: .+ \[Vulkan [0-9]+\.[0-9]+\.[0-9]+\] \(queue families gfx/cmp/xfer = [0-9]+/[0-9]+/[0-9]+; debugUtils=(?:yes|no) memBudget=(?:yes|no) descriptorIndexing=(?:yes|no) sync2=yes timeline=yes drawIndirectCount=(?:yes|no) anisotropy=[0-9]+x\)")[0]
start=exact("===== RAL pipeline test ",r"===== RAL pipeline test \(Phase 7\.3c\) =====")[0]
present=exact("  pipelineCache present:",r"  pipelineCache present: yes; layoutCache slots in use: 0")[0]
share=exact("  layout-cache share:",r"(?:  layout-cache share: 100/100 pipelines created; distinct layout-cache entries grew [0-9]+ → [0-9]+ \(expect \+1\); shared-slot refCount = 100 \(expect == created\)|  layout-cache share: after destroy, shared-slot refCount = 0 \(expect 0; layout VkHandle = VK_NULL_HANDLE \(defer-destroyed\)\))",count=2)
share_create,share_destroy=share
m=re.fullmatch(r"  layout-cache share: 100/100 pipelines created; distinct layout-cache entries grew ([0-9]+) → ([0-9]+) \(expect \+1\); shared-slot refCount = 100 \(expect == created\)",share_create[2])
if not m or int(m.group(2))!=int(m.group(1))+1:raise SystemExit("FAIL ral-pipeline layout growth")
if re.fullmatch(r"  layout-cache share: after destroy, shared-slot refCount = 0 \(expect 0; layout VkHandle = VK_NULL_HANDLE \(defer-destroyed\)\)",share_destroy[2]) is None:raise SystemExit("FAIL ral-pipeline layout destroy")
draw=exact("  draw:",r"  draw: pixel\(32,32\) RGBA = ([0-9]+) ([0-9]+) ([0-9]+) ([0-9]+) \(expect non-grey: triangle interior\); pixel\(0,0\) = ([0-9]+) ([0-9]+) ([0-9]+) ([0-9]+) \(expect ~26 = 0\.1×255 clear\); bright \(>0\.125\) pixels = ([0-9]+)/4096 \(expect a substantial fraction inside the centred triangle\)")[0]
dm=re.fullmatch(r"  draw: pixel\(32,32\) RGBA = ([0-9]+) ([0-9]+) ([0-9]+) ([0-9]+) \(expect non-grey: triangle interior\); pixel\(0,0\) = ([0-9]+) ([0-9]+) ([0-9]+) ([0-9]+) \(expect ~26 = 0\.1×255 clear\); bright \(>0\.125\) pixels = ([0-9]+)/4096 \(expect a substantial fraction inside the centred triangle\)",draw[2])
nums=list(map(int,dm.groups()));center=nums[:4];clear=nums[4:8];bright=nums[8]
if center[3]!=255 or len(set(center[:3]))<2 or clear[3]!=255 or any(x<24 or x>28 for x in clear[:3]) or bright<128 or bright>=4096:raise SystemExit(f"FAIL ral-pipeline draw values {nums}")
compute=exact("  compute:",r"  compute: 256 elements all match idx\*3\+7 — data\[10\]=37 \(expect 37\), data\[200\]=607 \(expect 607\), data\[255\]=772 \(expect 772\)")[0]
acquire=exact("  residency acquire:",r"  residency acquire: baseMip=2 mipCount=1 baseLayer=0 layerCount=1 readySemaphore=1 result=ok")[0]
residency=exact("  residency view:",r"  residency view: baseMip=2 sample RGBA = 0 255 0 255 \(expect coarse green; mip0 is red\)")[0]
cache=exact("  pipeline cache:",r"  pipeline cache: saved 'ral_pipeline_cache\.bin' \(([0-9]+) bytes, looks plausible: 32-byte header \+ driver-specific blob\)")[0]
cm=re.fullmatch(r"  pipeline cache: saved 'ral_pipeline_cache\.bin' \(([0-9]+) bytes, looks plausible: 32-byte header \+ driver-specific blob\)",cache[2])
if int(cm.group(1))<32:raise SystemExit("FAIL ral-pipeline cache size")
teardown=exact("  teardown:",r"  teardown: 0 pending destroys, 0 live allocations, 0 live layout-cache slots \(([1-9][0-9]*) high-water\)")[0]
end=exact("===== end RAL pipeline test =====",r"===== end RAL pipeline test =====")[0]
destroy=exact("Ral_DestroyBackend:",r"Ral_DestroyBackend: ok")[0]
dump_end=exact("===== end RAL dump =====",r"===== end RAL dump =====")[0]
complete=exact("Q0_RAL_PIPELINE_",r"Q0_RAL_PIPELINE_COMPLETE","INFO","system")[0]
shutdown=exact("----- Server Shutdown ",r"----- Server Shutdown \(Server quit\) -----","INFO","server")[0]
shutdown_game=exact("==== ShutdownGame ====",r"==== ShutdownGame ====","INFO","game")[0]
order=(first[0],receipt[0],dump[0],backend[0],start[0],present[0],share_create[0],share_destroy[0],draw[0],compute[0],acquire[0],residency[0],cache[0],teardown[0],end[0],destroy[0],dump_end[0],complete[0],shutdown[0],shutdown_game[0])
if any(a>=b for a,b in zip(order,order[1:])):raise SystemExit(f"FAIL ral-pipeline causal order {order}")
manifest=[]
for number,line in enumerate(open(manifest_path,encoding="utf-8",errors="strict"),1):
 try:item=json.loads(line)
 except Exception as exc:raise SystemExit(f"FAIL ral-pipeline manifest JSON {number}: {exc}")
 if not isinstance(item,dict):raise SystemExit("FAIL ral-pipeline manifest schema")
 manifest.append(item)
scenario={"kind":"scenario","schema":1,"name":"ral-pipeline-runtime","map":"arena7","backend":"vulkan","command":"ral_pipeline_test"}
if not manifest or manifest[0]!=scenario:raise SystemExit("FAIL ral-pipeline scenario")
if [x.get("role") for x in manifest[1:-1]] != ["gui","renderer","moltenvk","pax21","base","harness","bootstrap-cfg"]:raise SystemExit("FAIL ral-pipeline provenance roles")
for item in manifest[1:-1]:
 if set(item)!={"kind","role","path","bytes","sha256"} or item.get("kind")!="provenance" or not isinstance(item.get("bytes"),int) or item["bytes"]<=0 or re.fullmatch(r"[0-9a-f]{64}",str(item.get("sha256",""))) is None:raise SystemExit("FAIL ral-pipeline provenance schema")
 data=open(item["path"],"rb").read()
 if len(data)!=item["bytes"] or hashlib.sha256(data).hexdigest()!=item["sha256"]:raise SystemExit("FAIL ral-pipeline provenance rehash")
result=manifest[-1]
if set(result)!={"kind","controller_pid","rc","timeout","forced"} or result.get("kind")!="result" or not isinstance(result.get("controller_pid"),int) or result["controller_pid"]<=0 or result.get("rc")!=0 or result.get("timeout") is not False or result.get("forced") is not False:raise SystemExit("FAIL ral-pipeline result")
print("PASS exact offscreen RAL layout/draw/compute/cache pipeline exercise")
PYEOF
}

write_self() {
python3 - "$1" "$2" "$3" <<'PYEOF'
import hashlib,json,os,sys
log_path,manifest_path,mode=sys.argv[1:]
def row(sev,cat,msg):return {"ts":"2026-08-13T12:00:00.000+03:00","sev":sev,"cat":cat,"msg":msg+"\n"}
R=[row("INFO","client","cls.state: -> CA_ACTIVE (FIRST GAMEPLAY FRAME mapname=maps/arena7.bsp serverTime=1 numEntities=2 framecount=3)"),row("INFO","renderer.ral","ral_pipeline_test: running exact offscreen RAL pipeline exercise"),row("INFO","renderer.ral","===== RAL dump (Phase 7.3c Vulkan: instance/device/resources/async/pipeline) ====="),row("INFO","renderer.ral","Vulkan backend ready: Test GPU [Vulkan 1.3.0] (queue families gfx/cmp/xfer = 0/0/0; debugUtils=no memBudget=yes descriptorIndexing=yes sync2=yes timeline=yes drawIndirectCount=no anisotropy=16x)"),row("INFO","renderer.ral","===== RAL pipeline test (Phase 7.3c) ====="),row("INFO","renderer.ral","  pipelineCache present: yes; layoutCache slots in use: 0"),row("INFO","renderer.ral","  layout-cache share: 100/100 pipelines created; distinct layout-cache entries grew 0 → 1 (expect +1); shared-slot refCount = 100 (expect == created)"),row("INFO","renderer.ral","  layout-cache share: after destroy, shared-slot refCount = 0 (expect 0; layout VkHandle = VK_NULL_HANDLE (defer-destroyed))"),row("INFO","renderer.ral","  draw: pixel(32,32) RGBA = 124 70 62 255 (expect non-grey: triangle interior); pixel(0,0) = 26 26 26 255 (expect ~26 = 0.1×255 clear); bright (>0.125) pixels = 512/4096 (expect a substantial fraction inside the centred triangle)"),row("INFO","renderer.ral","  compute: 256 elements all match idx*3+7 — data[10]=37 (expect 37), data[200]=607 (expect 607), data[255]=772 (expect 772)"),row("INFO","renderer.ral","  residency acquire: baseMip=2 mipCount=1 baseLayer=0 layerCount=1 readySemaphore=1 result=ok"),row("INFO","renderer.ral","  residency view: baseMip=2 sample RGBA = 0 255 0 255 (expect coarse green; mip0 is red)"),row("INFO","renderer.ral","  pipeline cache: saved 'ral_pipeline_cache.bin' (4096 bytes, looks plausible: 32-byte header + driver-specific blob)"),row("INFO","renderer.ral","  teardown: 0 pending destroys, 0 live allocations, 0 live layout-cache slots (1 high-water)"),row("INFO","renderer.ral","===== end RAL pipeline test ====="),row("INFO","renderer.ral","Ral_DestroyBackend: ok"),row("INFO","renderer.ral","===== end RAL dump ====="),row("INFO","system","Q0_RAL_PIPELINE_COMPLETE"),row("INFO","server","----- Server Shutdown (Server quit) -----"),row("INFO","game","==== ShutdownGame ====")]
def find(text):return next(i for i,x in enumerate(R) if text in x["msg"])
if mode=="old-claim":R.insert(2,row("INFO","renderer.ral","Rendering is now exclusively on the legacy VkPipeline path."))
elif mode=="missing":R.pop(find("  compute:"))
elif mode=="duplicate":R.insert(find("  draw:")+1,dict(R[find("  draw:")]))
elif mode=="suffix":R[find("Q0_RAL_PIPELINE_COMPLETE")]["msg"]="Q0_RAL_PIPELINE_COMPLETE extra\n"
elif mode=="metadata":R[find("  teardown:")]["cat"]="renderer"
elif mode=="smuggle":R.append(row("INFO","system","benign\r  compute: 256 elements all match idx*3+7 — data[10]=37 (expect 37), data[200]=607 (expect 607), data[255]=772 (expect 772)"))
elif mode=="layout":R[find("100/100")]["msg"]=R[find("100/100")]["msg"].replace("0 → 1","0 → 2")
elif mode=="destroy":R[find("after destroy")]["msg"]=R[find("after destroy")]["msg"].replace("refCount = 0","refCount = 1")
elif mode=="draw-grey":R[find("  draw:")]["msg"]=R[find("  draw:")]["msg"].replace("124 70 62 255","70 70 70 255")
elif mode=="draw-clear":R[find("  draw:")]["msg"]=R[find("  draw:")]["msg"].replace("26 26 26 255","0 0 0 255")
elif mode=="compute":R[find("  compute:")]["msg"]=R[find("  compute:")]["msg"].replace("data[255]=772","data[255]=771")
elif mode=="acquire-missing":R.pop(find("  residency acquire:"))
elif mode=="acquire-range":R[find("  residency acquire:")]["msg"]=R[find("  residency acquire:")]["msg"].replace("baseMip=2","baseMip=1")
elif mode=="acquire-failed":R[find("  residency acquire:")]["msg"]=R[find("  residency acquire:")]["msg"].replace("result=ok","result=failed")
elif mode=="residency-missing":R.pop(find("  residency view:"))
elif mode=="residency-wrong":R[find("  residency view:")]["msg"]=R[find("  residency view:")]["msg"].replace("0 255 0 255","255 0 0 255")
elif mode=="residency-additive":R.insert(find("  pipeline cache:"),row("INFO","renderer.ral","  residency view: baseMip=0 sample RGBA = 255 0 0 255"))
elif mode=="cache":R[find("  pipeline cache:")]["msg"]=R[find("  pipeline cache:")]["msg"].replace("4096 bytes","12 bytes")
elif mode=="teardown":R[find("  teardown:")]["msg"]=R[find("  teardown:")]["msg"].replace("0 live allocations","1 live allocations")
elif mode=="vuid":R.append(row("WARN","renderer.vk","VUID-synthetic"))
elif mode=="order":a=find("  draw:");b=find("  compute:");R[a],R[b]=R[b],R[a]
elif mode=="backend-fail":R.insert(find("===== RAL pipeline test"),row("WARN","renderer.ral","Ral_CreateBackend failed (see [RAL] warnings above)"))
with open(log_path,"w") as out:
 for item in R:out.write(json.dumps(item)+"\n")
roles=("gui","renderer","moltenvk","pax21","base","harness","bootstrap-cfg")
M=[{"kind":"scenario","schema":1,"name":"ral-pipeline-runtime","map":"arena7","backend":"vulkan","command":"ral_pipeline_test"}]
for role in roles:
 path=manifest_path+"."+role;data=("fixture-"+role).encode();open(path,"wb").write(data);M.append({"kind":"provenance","role":role,"path":os.path.abspath(path),"bytes":len(data),"sha256":hashlib.sha256(data).hexdigest()})
M.append({"kind":"result","controller_pid":101,"rc":0,"timeout":False,"forced":False})
if mode=="manifest":M[2]["sha256"]="0"*64
elif mode=="result":M[-1]["rc"]=1
with open(manifest_path,"w") as out:
 for item in M:out.write(json.dumps(item,sort_keys=True)+"\n")
PYEOF
}

if [ "${1:-}" = --analyze ];then
 [ "$#" -eq 3 ] || { echo "usage: $0 --analyze <qconsole.jsonl> <manifest.jsonl>";exit 64; }
 analyze_contract "$2" "$3";exit $?
fi
if [ "${1:-}" = --self-test ];then
 ROOT="$(mktemp -d -t ral-pipeline-self-XXXXXX 2>/dev/null || mktemp -d)";trap 'rm -rf "$ROOT"' EXIT
 write_self "$ROOT/clean.log" "$ROOT/clean.manifest" clean || exit 1
 analyze_contract "$ROOT/clean.log" "$ROOT/clean.manifest" >/dev/null || exit 1
 defects=(old-claim missing duplicate suffix metadata smuggle layout destroy draw-grey draw-clear compute acquire-missing acquire-range acquire-failed residency-missing residency-wrong residency-additive cache teardown vuid order backend-fail manifest result)
 for defect in "${defects[@]}";do
  write_self "$ROOT/$defect.log" "$ROOT/$defect.manifest" "$defect" || exit 1
  if analyze_contract "$ROOT/$defect.log" "$ROOT/$defect.manifest" >/dev/null 2>&1;then echo "FAIL accepted $defect";exit 1;fi
 done
 echo "PASS ral-pipeline-runtime analyzer self-test (${#defects[@]} mutations)";exit 0
fi

WIRED="${1:-}";[ -n "$WIRED" ] && [ -x "$WIRED" ] || { echo "usage: $0 /absolute/path/to/wired";exit 64; }
[ -f "$TIMEOUT_RUNNER" ] || { echo "SKIP: missing timeout runner";exit 77; }
WIRED="$(cd "$(dirname "$WIRED")" && pwd)/$(basename "$WIRED")";WD="$(dirname "$WIRED")"
find_required(){ local name="$1" candidate;shift;for candidate in "$@";do [ -f "$candidate/$name" ] && { printf '%s\n' "$candidate/$name";return 0;};done;return 1;}
RENDERER="$(find_required wired_vulkan_arm64.dylib "$WD" "$WD/Contents/MacOS" "$WD/q3now-preview.arm64.app/Contents/MacOS" "$WD/../MacOS")" || { echo "SKIP: current Vulkan renderer unavailable";exit 77; }
MOLTEN="$(find_required libMoltenVK.dylib "$WD" "$WD/Contents/MacOS" "$WD/q3now-preview.arm64.app/Contents/MacOS" "$WD/../MacOS")" || { echo "SKIP: current MoltenVK unavailable";exit 77; }
PACK="";for candidate in "$WD" "$WD/../Resources" "$WD/../../.." "$WD/q3now-preview.arm64.app/Contents/Resources";do [ -f "$candidate/base/pax21.sw3z" ] && PACK="$(cd "$candidate" && pwd)" && break;done;[ -n "$PACK" ] || { echo "SKIP: current pax21 unavailable";exit 77; }
CONTENT="${WIRED_CONTENT_ROOT:-$PACK}";if [ -f "$CONTENT/base/pax01.sw3z" ];then BASE="$CONTENT/base/pax01.sw3z";elif [ -f "$CONTENT/base/pak0.pk3" ];then BASE="$CONTENT/base/pak0.pk3";else echo "SKIP: set WIRED_CONTENT_ROOT";exit 77;fi
ROOT="$(mktemp -d -t ral-pipeline-runtime-XXXXXX 2>/dev/null || mktemp -d)";HOME_DIR="$ROOT/home";RUN="$ROOT/runtime";FORCED=0
cleanup(){ local status=$?;trap - EXIT INT TERM;[ "${WIRED_KEEP_ARTIFACTS:-0}" = 1 ] || rm -rf "$ROOT";exit "$status";};trap cleanup EXIT;trap 'exit 130' INT;trap 'exit 143' TERM
mkdir -p "$HOME_DIR/base" "$RUN/Contents/MacOS";cp "$PACK/base/pax21.sw3z" "$HOME_DIR/base/" || exit 1;cp "$BASE" "$HOME_DIR/base/" || exit 1;cp "$WIRED" "$RUN/wired" || exit 1;chmod +x "$RUN/wired";cp "$RENDERER" "$MOLTEN" "$RUN/Contents/MacOS/" || exit 1
BOOT="$HOME_DIR/base/ral-pipeline-runtime.cfg";printf '%s\n' 'log renderer.ral info' 'set activeAction "ral_pipeline_test ; wait 30 ; echo Q0_RAL_PIPELINE_COMPLETE ; quit"' 'map arena7' >"$BOOT"
MANIFEST="$ROOT/manifest.jsonl";python3 - "$MANIFEST" "$WIRED" "$RENDERER" "$MOLTEN" "$PACK/base/pax21.sw3z" "$BASE" "$0" "$BOOT" <<'PYEOF'
import hashlib,json,os,sys
with open(sys.argv[1],"w") as out:
 out.write(json.dumps({"kind":"scenario","schema":1,"name":"ral-pipeline-runtime","map":"arena7","backend":"vulkan","command":"ral_pipeline_test"},sort_keys=True)+"\n")
 for role,path in zip(("gui","renderer","moltenvk","pax21","base","harness","bootstrap-cfg"),sys.argv[2:]):
  data=open(path,"rb").read();out.write(json.dumps({"kind":"provenance","role":role,"path":os.path.abspath(path),"bytes":len(data),"sha256":hashlib.sha256(data).hexdigest()},sort_keys=True)+"\n")
PYEOF
QCONSOLE="$HOME_DIR/qconsole.jsonl";STDOUT="$ROOT/stdout.log"
python3 "$TIMEOUT_RUNNER" --timeout 90 --kill-after 15 --cwd "$RUN" --stdout "$STDOUT" -- "$RUN/wired" +set fs_basepath "$HOME_DIR" +set fs_homepath "$HOME_DIR" +set fs_game base +set sv_pure 0 +set com_automated 1 +set com_noHardReboot 1 +set s_initsound 0 +set r_fullscreen 0 +set r_mode 3 +set r_vkValidate 1 +set log_severity DEBUG +set log_file_severity DEBUG +set log_file_mode overwrite +exec ral-pipeline-runtime.cfg
RC=$?;TIMEOUT=false;[ "$RC" -eq 124 ] && TIMEOUT=true;python3 - "$MANIFEST" "$$" "$RC" "$TIMEOUT" "$FORCED" <<'PYEOF'
import json,sys
with open(sys.argv[1],"a") as out:out.write(json.dumps({"kind":"result","controller_pid":int(sys.argv[2]),"rc":int(sys.argv[3]),"timeout":sys.argv[4]=="true","forced":sys.argv[5]=="1"},sort_keys=True)+"\n")
PYEOF
[ -f "$QCONSOLE" ] || { echo "FAIL missing qconsole: $ROOT";exit 1; };analyze_contract "$QCONSOLE" "$MANIFEST" || { echo "FAIL retained root: $ROOT";exit 1; }
echo "PASS retained root: $ROOT"
