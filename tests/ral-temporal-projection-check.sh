#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Default-off Phase 7.10 native-gamecl temporal continuity process contract.

set -uo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
TIMEOUT_RUNNER="$SCRIPT_DIR/run-with-timeout.py"
BOOT_CONTENT=$'log cgame info\nlog renderer.temporal info\nlog renderer.ral info\nset activeAction "cg_thirdPerson 1; cg_thirdPersonAlpha 255; cg_thirdPersonRange 100; noclip; +forward; echo Q3_RAL_TEMPORAL_REQUESTED; r_temporalInputTest 1; wait 1; ral_dump live temporal; wait 1; ral_dump live temporal; wait 1; ral_dump live temporal; -forward; echo Q3_RAL_TEMPORAL_ENABLED; r_temporalInputTest 0; wait 1; ral_dump live temporal; echo Q3_RAL_TEMPORAL_DISABLED; echo Q3_RAL_TEMPORAL_COMPLETE; quit"\nmap arena7\n'

analyze_contract() {
python3 - "$1" "$2" "$BOOT_CONTENT" <<'PYEOF'
import hashlib,json,math,re,sys
log_path,manifest_path,boot_expected=sys.argv[1:]
rows=[]
for number,line in enumerate(open(log_path,encoding="utf-8",errors="strict"),1):
 if not line.strip():continue
 try: row=json.loads(line)
 except Exception as exc: raise SystemExit(f"FAIL temporal JSON {number}: {exc}")
 if not isinstance(row,dict) or not all(isinstance(row.get(k),str) for k in ("ts","sev","cat","msg")):raise SystemExit("FAIL temporal log schema")
 rows.append(row)
if not rows:raise SystemExit("FAIL temporal empty evidence")
def norm(v):return v[:-1] if v.endswith("\n") and not v.endswith("\n\n") else v
vals=[norm(r["msg"]) for r in rows]
claimed=("temporal-projection ","temporal-continuity ","temporal-entity-","VM_Create policy module=gamecl ","Q3_RAL_TEMPORAL_","cls.state: -> CA_ACTIVE (FIRST GAMEPLAY FRAME")
for value in vals:
 if len(re.split(r"[\r\n]",value))>1 and any(part.startswith(claimed) for part in re.split(r"[\r\n]",value)):raise SystemExit("FAIL temporal logical-line smuggling")
if any(r["sev"].upper() in ("ERROR","FATAL") for r in rows):raise SystemExit("FAIL temporal severity")
if any("VUID-" in v for v in vals):raise SystemExit("FAIL temporal VUID")
def exact(prefix,pattern,sev,cat,count=1):
 found=[(i,r,v) for i,(r,v) in enumerate(zip(rows,vals)) if v.startswith(prefix)]
 if len(found)!=count:raise SystemExit(f"FAIL temporal {prefix} cardinality {len(found)}")
 for _,row,value in found:
  if row["sev"].upper()!=sev or row["cat"].lower()!=cat or re.fullmatch(pattern,value) is None:raise SystemExit(f"FAIL temporal {prefix} body/metadata: {value}")
 return found
policy=exact("VM_Create policy module=gamecl ",r"VM_Create policy module=gamecl requested=0 effective=0 backend=native","DEBUG","system")[0]
gamesv_load=exact("Sys_LoadLibrary(gamesvarm64.dylib): ",r"Sys_LoadLibrary\(gamesvarm64\.dylib\): loaded","INFO","filesystem")[0]
gamecl_load=exact("Sys_LoadLibrary(gameclarm64.dylib): ",r"Sys_LoadLibrary\(gameclarm64\.dylib\): loaded","INFO","filesystem")[0]
gamecl_vm=exact("VM_LoadDll(gamecl): ",r"VM_LoadDll\(gamecl\): loaded, vmMain @ 0x[0-9a-fA-F]+","INFO","system")[0]
cap_pattern=r"temporal-entity-capability glconfig-generation=([0-9]+) key=trap_R_AddRefEntityToSceneTemporal expected=232 discovered=232 route=native-syscall export=1"
cap=exact("temporal-entity-capability ",cap_pattern,"INFO","cgame")[0]
slot_pat=r"temporal-entity-slot glconfig-generation=([0-9]+) trap=232 owner=([0-9]+) entity-generation=([1-9][0-9]*) role=([1-9][0-9]*) accepted=1 export=1"
slots=[]
for i,(row,value) in enumerate(zip(rows,vals)):
 if value.startswith("temporal-entity-slot "):
  m=re.fullmatch(slot_pat,value)
  if row["sev"].upper()!="INFO" or row["cat"].lower()!="cgame" or not m:raise SystemExit("FAIL temporal slot232 metadata/body")
  slots.append((i,tuple(map(int,m.groups()))))
if not slots:raise SystemExit("FAIL temporal no validated slot232 ingress")
cap_gen=int(re.fullmatch(cap_pattern,cap[2]).group(1))
if any(s[1][0]!=cap_gen for s in slots):raise SystemExit("FAIL temporal capability/slot generation mismatch")
first=exact("cls.state: -> CA_ACTIVE (FIRST GAMEPLAY FRAME",r"cls\.state: -> CA_ACTIVE \(FIRST GAMEPLAY FRAME mapname=maps/arena7\.bsp serverTime=[0-9]+ numEntities=[0-9]+ framecount=[0-9]+\)","INFO","client")[0]
markers=exact("Q3_RAL_TEMPORAL_",r"Q3_RAL_TEMPORAL_(?:REQUESTED|ENABLED|DISABLED|COMPLETE)","INFO","system",4)
if [x[2] for x in markers] != ["Q3_RAL_TEMPORAL_REQUESTED","Q3_RAL_TEMPORAL_ENABLED","Q3_RAL_TEMPORAL_DISABLED","Q3_RAL_TEMPORAL_COMPLETE"]:raise SystemExit("FAIL temporal marker vector")
rebuilds=exact("scene-depth live rebuild ",r"scene-depth live rebuild active=([01]) deferred-drained=1 attachments-rebound=1 temporal-store-reset=1","INFO","renderer.ral",2)
if [re.search(r"active=([01])",x[2]).group(1) for x in rebuilds] != ["1","0"]:raise SystemExit("FAIL temporal rebuild vector")
num=r"-?[0-9]+\.[0-9]+"
proj_pat=(r"temporal-projection world=([0-3]) frame=([1-9][0-9]*) enabled=([01]) queued=1 recorded=([01]) previous-read=([01]) committed=1 camera-valid=([01]) camera-previous=([01]) entities=([0-9]+)/([0-9]+) previous=([0-9]+) rejected=([0-9]+) entity-commit=([01]) generation=([1-9][0-9]*) reset=0x([0-9a-f]+) history-valid=([01]) read=([01]) write=([01]) phase=([0-7]) extent=([1-9][0-9]*)x([1-9][0-9]*) resources=(ready|none) resource-generation=([1-9][0-9]*) color-slots=([02]) depth-slots=([02]) jitter-px=("+num+r"),("+num+r") jitter-uv=("+num+r"),("+num+r") ui-jitter=("+num+r"),("+num+r") ndc=("+num+r"),("+num+r") projection=("+num+r"),("+num+r")->("+num+r"),("+num+r")")
projections=exact("temporal-projection ",proj_pat,"INFO","renderer.temporal",4)
pm=[re.fullmatch(proj_pat,x[2]) for x in projections]
if any(x is None for x in pm):raise SystemExit("FAIL temporal projection parse")
enabled_pm=pm[:3]; disabled_pm=pm[3]
frames=[int(x.group(2)) for x in enabled_pm]
if any(int(x.group(1))!=0 for x in pm):raise SystemExit("FAIL temporal world binding")
if frames[1]!=frames[0]+1 or frames[2]!=frames[1]+1:raise SystemExit("FAIL temporal dumps not adjacent")
for n,m in enumerate(enabled_pm):
 vals_i=list(map(int,m.groups()[:13]))
 if vals_i[2]!=1 or vals_i[3]!=1 or vals_i[5]!=1 or vals_i[7]<=0 or vals_i[8]<=0 or vals_i[10]!=0 or vals_i[11]!=1:raise SystemExit("FAIL temporal enabled projection receipt")
 if n==0 and int(m.group(14),16)==0:raise SystemExit("FAIL temporal activation reset")
if int(disabled_pm.group(3))!=0 or int(disabled_pm.group(4))!=0:raise SystemExit("FAIL temporal disabled projection")
bits352=r"[0-9a-f]{352}";bits184=r"[0-9a-f]{184}"
cont_pat=(r"temporal-continuity schema=1 world=([0-3]) frame=([1-9][0-9]*) committed=([01]) camera-valid=([01]) camera-previous=([01]) camera-previous-frame=([0-9]+) camera-current=("+bits352+r") camera-previous-fields=("+bits352+r") entity-valid=([01]) entity-owner=([0-9]+) entity-generation=([0-9]+) entity-role=([0-9]+) entity-previous=([01]) entity-previous-frame=([0-9]+) entity-current=("+bits184+r") entity-previous-fields=("+bits184+r") entity-committed=([01]) attempts=([0-9]+) scans=([0-9]+) drawsurfs=([0-9]+) visible-temporal=([0-9]+) accepted=([0-9]+) rejected=([0-9]+)")
continuity=exact("temporal-continuity ",cont_pat,"INFO","renderer.temporal",4)
cm=[re.fullmatch(cont_pat,x[2]) for x in continuity]
if [int(x.group(2)) for x in cm] != [int(x.group(2)) for x in pm]:raise SystemExit("FAIL temporal projection/continuity frame bind")
slot_tuples={(owner,generation,role) for _,(_,owner,generation,role) in slots}
for n,m in enumerate(cm[:3]):
 frame=int(m.group(2));owner,generation,role=map(int,(m.group(10),m.group(11),m.group(12)))
 if int(m.group(1))!=0:raise SystemExit("FAIL temporal continuity world")
 if int(m.group(3))!=1 or int(m.group(4))!=1 or int(m.group(9))!=1 or int(m.group(17))!=1:raise SystemExit("FAIL temporal continuity commit/valid")
 if int(m.group(18))<1 or int(m.group(19))!=1 or int(m.group(20))<1 or int(m.group(21))<1 or int(m.group(22))<1 or int(m.group(23))!=0:raise SystemExit("FAIL temporal continuity attempt/scan counts")
 if int(m.group(22))>int(m.group(21)) or int(m.group(21))>int(m.group(20)):raise SystemExit("FAIL temporal continuity count relation")
 projection=enabled_pm[n]
 if (int(projection.group(8)),int(projection.group(9)),int(projection.group(11)))!=(int(m.group(22)),int(m.group(21)),int(m.group(23))):raise SystemExit("FAIL temporal projection/continuity counts")
 if (owner,generation,role) not in slot_tuples:raise SystemExit("FAIL temporal sampled entity not bound to slot232 ingress")
 if n==0:
  if int(m.group(5)) or int(m.group(6)) or int(m.group(13)) or int(m.group(14)):raise SystemExit("FAIL temporal activation predecessor")
 else:
  prior=cm[n-1]
  if int(m.group(5))!=1 or int(m.group(6))!=frame-1 or m.group(8)!=prior.group(7):raise SystemExit("FAIL temporal camera prior/previous")
  if int(m.group(13))!=1 or int(m.group(14))!=frame-1 or m.group(16)!=prior.group(15):raise SystemExit("FAIL temporal entity prior/previous")
  if (m.group(10),m.group(11),m.group(12))!=(prior.group(10),prior.group(11),prior.group(12)):raise SystemExit("FAIL temporal sampled tuple drift")
if len({m.group(7)[128:] for m in cm[:3]})!=3 or len({m.group(15)[56:] for m in cm[:3]})!=3:raise SystemExit("FAIL temporal moving camera/entity pose witness")
owner0,generation0,role0=map(int,(cm[0].group(10),cm[0].group(11),cm[0].group(12)))
if owner0!=0 or role0 not in (3,4,5,6):raise SystemExit("FAIL temporal witness is not local-player body part")
entity0=cm[0].group(15)
if int(entity0[0:8],16)==0 or int(entity0[40:48],16)==0 or int(entity0[48:56],16)==0:raise SystemExit("FAIL temporal witness model identity/topology")
matching_slot_indices=[i for i,(_,owner,generation,role) in slots if (owner,generation,role)==(owner0,generation0,role0)]
if not matching_slot_indices:raise SystemExit("FAIL temporal sampled witness has no exact slot232 ingress")
shutdown=exact("----- Server Shutdown ",r"----- Server Shutdown \(Server quit\) -----","INFO","server")[0]
shutdown_game=exact("==== ShutdownGame ====",r"==== ShutdownGame ====","INFO","game")[0]
if not gamesv_load[0]<gamecl_load[0]<gamecl_vm[0]<policy[0]<first[0]<markers[0][0]:raise SystemExit("FAIL temporal native load order")
if not policy[0]<cap[0]<min(matching_slot_indices)<markers[0][0]:raise SystemExit("FAIL temporal authority order")
if not markers[0][0]<rebuilds[0][0]<projections[0][0]<continuity[0][0]<projections[1][0]<continuity[1][0]<projections[2][0]<continuity[2][0]<markers[1][0]<rebuilds[1][0]<projections[3][0]<continuity[3][0]<markers[2][0]<markers[3][0]<shutdown[0]<shutdown_game[0]:raise SystemExit("FAIL temporal causal order")
manifest=[json.loads(line) for line in open(manifest_path,encoding="utf-8",errors="strict") if line.strip()]
scenario={"kind":"scenario","schema":5,"name":"ral-temporal-projection","map":"arena7","backend":"vulkan","cgame":"native-vm_cgame-0","toggle":"isolated-r_temporalInputTest:0->1->0","consumer":"primary-world-projection-history-and-submit-bound-camera-entity-cpu-continuity","nonclaims":["gpu-content-readback","validation-backed-vuid-cleanliness","taa-resolve","motion-vectors","quality","letterboxed-or-multiview-history","screenmap-replay-runtime","split-screen-runtime","wasm-cgame-runtime"]}
if not manifest or manifest[0]!=scenario:raise SystemExit("FAIL temporal scenario")
roles=["gui","renderer","gamecl-native","gamesv-native","moltenvk","pax21","base","harness","bootstrap-cfg"]
if [x.get("role") for x in manifest[1:-1]]!=roles:raise SystemExit("FAIL temporal provenance roles")
for item in manifest[1:-1]:
 if set(item)!={"kind","role","path","bytes","sha256"} or item.get("kind")!="provenance" or not isinstance(item.get("bytes"),int) or item["bytes"]<=0 or re.fullmatch(r"[0-9a-f]{64}",str(item.get("sha256",""))) is None:raise SystemExit("FAIL temporal provenance schema")
 data=open(item["path"],"rb").read()
 if len(data)!=item["bytes"] or hashlib.sha256(data).hexdigest()!=item["sha256"]:raise SystemExit("FAIL temporal provenance rehash")
 if item["role"]=="bootstrap-cfg" and data.decode()!=boot_expected:raise SystemExit("FAIL temporal bootstrap content")
result=manifest[-1]
if set(result)!={"kind","controller_pid","rc","timeout","forced"} or result.get("kind")!="result" or not isinstance(result.get("controller_pid"),int) or result["controller_pid"]<=0 or result.get("rc")!=0 or result.get("timeout") is not False or result.get("forced") is not False:raise SystemExit("FAIL temporal result")
print("PASS native slot232 temporal camera/entity continuity")
PYEOF
}

write_self() {
python3 - "$1" "$2" "$3" "$BOOT_CONTENT" <<'PYEOF'
import hashlib,json,os,sys
log_path,manifest_path,mode,boot=sys.argv[1:]
def row(sev,cat,msg):return {"ts":"2026-08-14T12:00:00.000+03:00","sev":sev,"cat":cat,"msg":msg+"\n"}
def proj(frame,enabled,reset,history,read,write,phase,cam_prev):
 rec=1 if enabled else 0;ready="ready" if enabled else "none";slots=2 if enabled else 0;accepted=2 if enabled else 0;visible=3 if enabled else 0;commit=1 if enabled else 0
 return row("INFO","renderer.temporal",f"temporal-projection world=0 frame={frame} enabled={enabled} queued=1 recorded={rec} previous-read=0 committed=1 camera-valid=1 camera-previous={cam_prev} entities={accepted}/{visible} previous={accepted if cam_prev else 0} rejected=0 entity-commit={commit} generation={1 if enabled else 2} reset=0x{reset:x} history-valid={history} read={read} write={write} phase={phase} extent=1280x720 resources={ready} resource-generation=1 color-slots={slots} depth-slots={slots} jitter-px=0.000000,-0.166667 jitter-uv=0.000000000,-0.000231481 ui-jitter=0.0,0.0 ndc=0.000000000,0.000462963 projection=0.100000000,-0.200000000->0.100000000,-0.200462963")
def cont(frame,n,enabled=True):
 cam=("%x"%n)*352;ent=("%x"%(n+3))*184;pcam=("0"*352 if n==1 else ("%x"%(n-1))*352);pent=("0"*184 if n==1 else ("%x"%(n+2))*184);prev=0 if n==1 else 1;pf=0 if n==1 else frame-1
 return row("INFO","renderer.temporal",f"temporal-continuity schema=1 world=0 frame={frame} committed=1 camera-valid=1 camera-previous={prev} camera-previous-frame={pf} camera-current={cam} camera-previous-fields={pcam} entity-valid={1 if enabled else 0} entity-owner=0 entity-generation=1 entity-role=3 entity-previous={prev if enabled else 0} entity-previous-frame={pf if enabled else 0} entity-current={ent if enabled else '0'*184} entity-previous-fields={pent if enabled else '0'*184} entity-committed={1 if enabled else 0} attempts=1 scans=1 drawsurfs=40 visible-temporal={3 if enabled else 0} accepted={2 if enabled else 0} rejected=0")
R=[row("INFO","filesystem","Sys_LoadLibrary(gamesvarm64.dylib): loaded"),row("INFO","filesystem","Sys_LoadLibrary(gameclarm64.dylib): loaded"),row("INFO","system","VM_LoadDll(gamecl): loaded, vmMain @ 0x1234"),row("DEBUG","system","VM_Create policy module=gamecl requested=0 effective=0 backend=native"),row("INFO","client","cls.state: -> CA_ACTIVE (FIRST GAMEPLAY FRAME mapname=maps/arena7.bsp serverTime=1 numEntities=2 framecount=3)"),row("INFO","cgame","temporal-entity-capability glconfig-generation=7 key=trap_R_AddRefEntityToSceneTemporal expected=232 discovered=232 route=native-syscall export=1"),row("INFO","cgame","temporal-entity-slot glconfig-generation=7 trap=232 owner=0 entity-generation=1 role=3 accepted=1 export=1"),row("INFO","system","Q3_RAL_TEMPORAL_REQUESTED"),row("INFO","renderer.ral","scene-depth live rebuild active=1 deferred-drained=1 attachments-rebound=1 temporal-store-reset=1")]
for f,n in ((10,1),(11,2),(12,3)):R += [proj(f,1,1 if n==1 else 0,0 if n==1 else 1,(n-1)&1,n&1,n-1,0 if n==1 else 1),cont(f,n)]
R += [row("INFO","system","Q3_RAL_TEMPORAL_ENABLED"),row("INFO","renderer.ral","scene-depth live rebuild active=0 deferred-drained=1 attachments-rebound=1 temporal-store-reset=1"),proj(13,0,2,0,0,0,0,1),cont(13,4,False),row("INFO","system","Q3_RAL_TEMPORAL_DISABLED"),row("INFO","system","Q3_RAL_TEMPORAL_COMPLETE"),row("INFO","server","----- Server Shutdown (Server quit) -----"),row("INFO","game","==== ShutdownGame ====")]
def find(prefix,n=1):return [i for i,x in enumerate(R) if x["msg"].startswith(prefix)][n-1]
if mode=="cap-missing":R.pop(find("temporal-entity-capability"))
elif mode=="cap-trap":R[find("temporal-entity-capability")]["msg"]=R[find("temporal-entity-capability")]["msg"].replace("discovered=232","discovered=231")
elif mode=="slot-missing":R.pop(find("temporal-entity-slot"))
elif mode=="slot-export":R[find("temporal-entity-slot")]["msg"]=R[find("temporal-entity-slot")]["msg"].replace("export=1","export=0")
elif mode=="vm":R[3]["msg"]=R[3]["msg"].replace("requested=0 effective=0 backend=native","requested=1 effective=1 backend=wasm-interpreter")
elif mode=="continuity-missing":R.pop(find("temporal-continuity",2))
elif mode=="adjacent":R[find("temporal-continuity",2)]["msg"]=R[find("temporal-continuity",2)]["msg"].replace("frame=11","frame=14")
elif mode=="camera-previous":R[find("temporal-continuity",2)]["msg"]=R[find("temporal-continuity",2)]["msg"].replace("camera-previous=1","camera-previous=0")
elif mode=="camera-blob":R[find("temporal-continuity",2)]["msg"]=R[find("temporal-continuity",2)]["msg"].replace("camera-previous-fields="+"1"*352,"camera-previous-fields="+"f"*352)
elif mode=="entity-previous":R[find("temporal-continuity",2)]["msg"]=R[find("temporal-continuity",2)]["msg"].replace("entity-previous=1","entity-previous=0")
elif mode=="entity-blob":R[find("temporal-continuity",2)]["msg"]=R[find("temporal-continuity",2)]["msg"].replace("entity-previous-fields="+"4"*184,"entity-previous-fields="+"f"*184)
elif mode=="tuple":R[find("temporal-continuity",2)]["msg"]=R[find("temporal-continuity",2)]["msg"].replace("entity-role=3","entity-role=4")
elif mode=="stationary":R[find("temporal-continuity",3)]["msg"]=R[find("temporal-continuity",3)]["msg"].replace("entity-current="+"6"*184,"entity-current="+"5"*184)
elif mode=="commit":R[find("temporal-continuity",2)]["msg"]=R[find("temporal-continuity",2)]["msg"].replace("entity-committed=1","entity-committed=0")
elif mode=="counts":R[find("temporal-continuity",2)]["msg"]=R[find("temporal-continuity",2)]["msg"].replace("scans=1","scans=0")
elif mode=="world":R[find("temporal-continuity",2)]["msg"]=R[find("temporal-continuity",2)]["msg"].replace("world=0","world=1")
elif mode=="load":R.pop(find("Sys_LoadLibrary(gameclarm64.dylib):"))
elif mode=="late-slot":R.append(R.pop(find("temporal-entity-slot")))
elif mode=="non-player":R[find("temporal-continuity",1)]["msg"]=R[find("temporal-continuity",1)]["msg"].replace("entity-role=3","entity-role=7")
elif mode=="zero-hmodel":R[find("temporal-continuity",1)]["msg"]=R[find("temporal-continuity",1)]["msg"].replace("entity-current="+"4"*184,"entity-current="+"0"*8+"4"*176)
elif mode=="zero-topology":R[find("temporal-continuity",1)]["msg"]=R[find("temporal-continuity",1)]["msg"].replace("entity-current="+"4"*184,"entity-current="+"4"*48+"0"*8+"4"*128)
elif mode=="severity":R.append(row("ERROR","renderer.temporal","synthetic"))
elif mode=="smuggle":R.append(row("INFO","system","benign\rtemporal-continuity schema=1"))
with open(log_path,"w") as out:
 for x in R:out.write(json.dumps(x)+"\n")
scenario={"kind":"scenario","schema":5,"name":"ral-temporal-projection","map":"arena7","backend":"vulkan","cgame":"native-vm_cgame-0","toggle":"isolated-r_temporalInputTest:0->1->0","consumer":"primary-world-projection-history-and-submit-bound-camera-entity-cpu-continuity","nonclaims":["gpu-content-readback","validation-backed-vuid-cleanliness","taa-resolve","motion-vectors","quality","letterboxed-or-multiview-history","screenmap-replay-runtime","split-screen-runtime","wasm-cgame-runtime"]}
M=[scenario]
for role in ("gui","renderer","gamecl-native","gamesv-native","moltenvk","pax21","base","harness","bootstrap-cfg"):
 path=manifest_path+"."+role;data=boot.encode() if role=="bootstrap-cfg" else ("fixture-"+role).encode();open(path,"wb").write(data);M.append({"kind":"provenance","role":role,"path":os.path.abspath(path),"bytes":len(data),"sha256":hashlib.sha256(data).hexdigest()})
M.append({"kind":"result","controller_pid":101,"rc":0,"timeout":False,"forced":False})
if mode=="manifest":M[2]["sha256"]="0"*64
elif mode=="scenario":M[0]["schema"]=4
elif mode=="result":M[-1]["rc"]=1
elif mode=="gamecl-role":M[3]["role"]="gamecl"
elif mode=="bootstrap":
 data=b"wrong\n";open(M[-2]["path"],"wb").write(data);M[-2]["bytes"]=len(data);M[-2]["sha256"]=hashlib.sha256(data).hexdigest()
with open(manifest_path,"w") as out:
 for x in M:out.write(json.dumps(x,sort_keys=True)+"\n")
PYEOF
}

if [ "${1:-}" = --analyze ];then [ "$#" -eq 3 ] || exit 64;analyze_contract "$2" "$3";exit $?;fi
if [ "${1:-}" = --self-test ];then
 ROOT="$(mktemp -d -t ral-temporal-self-XXXXXX 2>/dev/null || mktemp -d)";trap 'rm -rf "$ROOT"' EXIT
 write_self "$ROOT/clean.log" "$ROOT/clean.manifest" clean || exit 1;analyze_contract "$ROOT/clean.log" "$ROOT/clean.manifest" >/dev/null || exit 1
 defects=(cap-missing cap-trap slot-missing slot-export vm continuity-missing adjacent camera-previous camera-blob entity-previous entity-blob tuple stationary commit counts world load late-slot non-player zero-hmodel zero-topology severity smuggle manifest scenario result gamecl-role bootstrap)
 for d in "${defects[@]}";do write_self "$ROOT/$d.log" "$ROOT/$d.manifest" "$d" || exit 1;if analyze_contract "$ROOT/$d.log" "$ROOT/$d.manifest" >/dev/null 2>&1;then echo "FAIL accepted $d";exit 1;fi;done
 echo "PASS ral-temporal analyzer self-test (${#defects[@]} mutations)";exit 0
fi

WIRED="${1:-}";[ -n "$WIRED" ] && [ -x "$WIRED" ] || { echo "usage: $0 /absolute/path/to/wired";exit 64; }
[ "$(uname -s)" = Darwin ] || { echo "SKIP: temporal runtime currently requires macOS/MoltenVK";exit 77; }
[ -f "$TIMEOUT_RUNNER" ] || { echo "SKIP: missing timeout runner";exit 77; }
WIRED="$(cd "$(dirname "$WIRED")" && pwd)/$(basename "$WIRED")";WD="$(dirname "$WIRED")"
find_required(){ local name="$1" candidate;shift;for candidate in "$@";do [ -f "$candidate/$name" ] && { printf '%s\n' "$candidate/$name";return 0;};done;return 1;}
RENDERER="$(find_required wired_vulkan_arm64.dylib "$WD" "$WD/Contents/MacOS" "$WD/q3now-preview.arm64.app/Contents/MacOS" "$WD/../MacOS")" || { echo "SKIP: current Vulkan renderer unavailable";exit 77; }
GAMECL="${WIRED_GAMECL:-}";[ -f "$GAMECL" ] || GAMECL="$(find_required gameclarm64.dylib "$WD/base" "$WD/Contents/Resources/base" "$WD/../Resources/base" "$WD/q3now-preview.arm64.app/Contents/Resources/base")" || { echo "SKIP: set WIRED_GAMECL to current native gameclarm64.dylib";exit 77; }
GAMESV="${WIRED_GAMESV:-}";[ -f "$GAMESV" ] || GAMESV="$(find_required gamesvarm64.dylib "$WD/base" "$WD/Contents/Resources/base" "$WD/../Resources/base" "$WD/q3now-preview.arm64.app/Contents/Resources/base")" || { echo "SKIP: set WIRED_GAMESV to current native gamesvarm64.dylib";exit 77; }
MOLTEN="$(find_required libMoltenVK.dylib "$WD" "$WD/Contents/MacOS" "$WD/q3now-preview.arm64.app/Contents/MacOS" "$WD/../MacOS")" || { echo "SKIP: current MoltenVK unavailable";exit 77; }
PACK="";for candidate in "${WIRED_CONTENT_ROOT:-}" "$WD" "$WD/../Resources" "$WD/../../.." "$WD/q3now-preview.arm64.app/Contents/Resources";do [ -n "$candidate" ] && [ -f "$candidate/base/pax21.sw3z" ] && PACK="$(cd "$candidate" && pwd)" && break;done;[ -n "$PACK" ] || { echo "SKIP: current pax21 unavailable";exit 77; }
CONTENT="${WIRED_CONTENT_ROOT:-$PACK}";if [ -f "$CONTENT/base/pax01.sw3z" ];then BASE="$CONTENT/base/pax01.sw3z";elif [ -f "$CONTENT/base/pak0.pk3" ];then BASE="$CONTENT/base/pak0.pk3";else echo "SKIP: set WIRED_CONTENT_ROOT";exit 77;fi
ROOT="$(mktemp -d -t ral-temporal-projection-XXXXXX 2>/dev/null || mktemp -d)";HOME_DIR="$ROOT/home";RUN="$ROOT/runtime";FORCED=0
cleanup(){ local status=$?;trap - EXIT INT TERM;[ "${WIRED_KEEP_ARTIFACTS:-0}" = 1 ] || rm -rf "$ROOT";exit "$status";};trap cleanup EXIT;trap 'exit 130' INT;trap 'exit 143' TERM
mkdir -p "$HOME_DIR/base" "$RUN/Contents/MacOS";cp "$PACK/base/pax21.sw3z" "$HOME_DIR/base/" || exit 1;cp "$BASE" "$HOME_DIR/base/" || exit 1;cp "$GAMECL" "$HOME_DIR/base/gameclarm64.dylib" || exit 1;cp "$GAMESV" "$HOME_DIR/base/gamesvarm64.dylib" || exit 1;cp "$WIRED" "$RUN/wired" || exit 1;chmod +x "$RUN/wired";cp "$RENDERER" "$MOLTEN" "$RUN/Contents/MacOS/" || exit 1
BOOT="$HOME_DIR/base/ral-temporal-projection.cfg";printf '%s' "$BOOT_CONTENT" >"$BOOT"
MANIFEST="$ROOT/manifest.jsonl";python3 - "$MANIFEST" "$RUN/wired" "$RUN/Contents/MacOS/wired_vulkan_arm64.dylib" "$HOME_DIR/base/gameclarm64.dylib" "$HOME_DIR/base/gamesvarm64.dylib" "$RUN/Contents/MacOS/libMoltenVK.dylib" "$HOME_DIR/base/pax21.sw3z" "$HOME_DIR/base/$(basename "$BASE")" "$0" "$BOOT" <<'PYEOF'
import hashlib,json,os,sys
scenario={"kind":"scenario","schema":5,"name":"ral-temporal-projection","map":"arena7","backend":"vulkan","cgame":"native-vm_cgame-0","toggle":"isolated-r_temporalInputTest:0->1->0","consumer":"primary-world-projection-history-and-submit-bound-camera-entity-cpu-continuity","nonclaims":["gpu-content-readback","validation-backed-vuid-cleanliness","taa-resolve","motion-vectors","quality","letterboxed-or-multiview-history","screenmap-replay-runtime","split-screen-runtime","wasm-cgame-runtime"]}
with open(sys.argv[1],"w") as out:
 out.write(json.dumps(scenario,sort_keys=True)+"\n")
 for role,path in zip(("gui","renderer","gamecl-native","gamesv-native","moltenvk","pax21","base","harness","bootstrap-cfg"),sys.argv[2:]):
  data=open(path,"rb").read();out.write(json.dumps({"kind":"provenance","role":role,"path":os.path.abspath(path),"bytes":len(data),"sha256":hashlib.sha256(data).hexdigest()},sort_keys=True)+"\n")
PYEOF
QCONSOLE="$HOME_DIR/qconsole.jsonl";STDOUT="$ROOT/stdout.log"
python3 "$TIMEOUT_RUNNER" --timeout 60 --kill-after 15 --cwd "$RUN" --stdout "$STDOUT" -- "$RUN/wired" +set fs_basepath "$HOME_DIR" +set fs_homepath "$HOME_DIR" +set fs_game base +set vm_game 0 +set vm_cgame 0 +set sv_cheats 1 +set sv_pure 0 +set com_automated 1 +set com_noHardReboot 1 +set s_initsound 0 +set r_fullscreen 0 +set r_mode 3 +set r_vkValidate 0 +set r_temporalInputTest 0 +set r_bloom 0 +set r_ssao 0 +set r_smaa 0 +set r_forwardPlus 0 +set r_drawSunRays 0 +set r_shadows 0 +set r_depthFade 0 +set r_lens 0 +set r_gpuDecals 0 +set r_particles 0 +set r_atmosphericGPU 0 +set log_severity DEBUG +set log_file_severity DEBUG +set log_file_mode overwrite_synced +exec ral-temporal-projection.cfg
RC=$?;TIMEOUT=false;[ "$RC" -eq 124 ] && TIMEOUT=true;python3 - "$MANIFEST" "$$" "$RC" "$TIMEOUT" "$FORCED" <<'PYEOF'
import json,sys
with open(sys.argv[1],"a") as out:out.write(json.dumps({"kind":"result","controller_pid":int(sys.argv[2]),"rc":int(sys.argv[3]),"timeout":sys.argv[4]=="true","forced":sys.argv[5]=="1"},sort_keys=True)+"\n")
PYEOF
[ -f "$QCONSOLE" ] || { echo "FAIL missing qconsole; retained root: $ROOT";WIRED_KEEP_ARTIFACTS=1;exit 1; };analyze_contract "$QCONSOLE" "$MANIFEST" || { echo "FAIL retained root: $ROOT";WIRED_KEEP_ARTIFACTS=1;exit 1; }
echo "PASS retained root: $ROOT"
