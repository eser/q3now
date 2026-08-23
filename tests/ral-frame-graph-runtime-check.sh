#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Native Vulkan proof for the opt-in above-RAL frame-graph diagnostic.

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
TIMEOUT_RUNNER="$SCRIPT_DIR/run-with-timeout.py"

analyze_contract() {
python3 - "$1" "$2" "$3" <<'PYEOF'
import hashlib,json,os,re,sys
log_path,manifest_path,expected_map=sys.argv[1:]
rows=[]
for number,line in enumerate(open(log_path,encoding="utf-8",errors="strict"),1):
 if not line.strip():continue
 try:row=json.loads(line)
 except Exception as exc:raise SystemExit(f"FAIL ral-frame-graph-runtime JSON {number}: {exc}")
 if not isinstance(row,dict) or not all(isinstance(row.get(k),str) for k in ("ts","sev","cat","msg")):raise SystemExit("FAIL ral-frame-graph-runtime log schema")
 rows.append(row)
if not rows:raise SystemExit("FAIL ral-frame-graph-runtime empty evidence")
def norm(value):return value[:-1] if value.endswith("\n") and not value.endswith("\n\n") else value
vals=[norm(row["msg"]) for row in rows]
claimed=("ral-frame-graph-native ","ral-frame-graph-native-failure ","Q0_RAL_FRAME_GRAPH_","cls.state: -> CA_ACTIVE (FIRST GAMEPLAY FRAME")
for value in vals:
 if len(re.split(r"[\r\n]",value))>1 and any(part.startswith(claimed) for part in re.split(r"[\r\n]",value)):raise SystemExit("FAIL ral-frame-graph-runtime claimed logical-line smuggling")
if any(row["sev"].upper() in ("ERROR","FATAL") for row in rows):raise SystemExit("FAIL ral-frame-graph-runtime severity")
if any("VUID-" in value or value.startswith("ral-frame-graph-native-failure ") for value in vals):raise SystemExit("FAIL ral-frame-graph-runtime failure/VUID")
def exact(prefix,pattern,sev,cat,count=1):
 found=[(i,row,value) for i,(row,value) in enumerate(zip(rows,vals)) if value.startswith(prefix)]
 if len(found)!=count:raise SystemExit(f"FAIL ral-frame-graph-runtime {prefix} cardinality {len(found)}")
 for _,row,value in found:
  if row["sev"].upper()!=sev or row["cat"].lower()!=cat or re.fullmatch(pattern,value) is None:raise SystemExit(f"FAIL ral-frame-graph-runtime {prefix} body/metadata: {value}")
 return found
first=exact("cls.state: -> CA_ACTIVE (FIRST GAMEPLAY FRAME",rf"cls\.state: -> CA_ACTIVE \(FIRST GAMEPLAY FRAME mapname=maps/{re.escape(expected_map)}\.bsp serverTime=[0-9]+ numEntities=[0-9]+ framecount=[0-9]+\)","INFO","client")[0]
requested=exact("Q0_RAL_FRAME_GRAPH_REQUESTED",r"Q0_RAL_FRAME_GRAPH_REQUESTED","INFO","system")[0]
complete=exact("Q0_RAL_FRAME_GRAPH_COMPLETE",r"Q0_RAL_FRAME_GRAPH_COMPLETE","INFO","system")[0]
pattern=r"ral-frame-graph-native schema=([0-9]+) generation=([0-9]+) graph=([0-9]+) material=([0-9]+) batch=([0-9]+) recording=([0-9]+) submission=([0-9]+) native-submission=([0-9]+) textures=([0-9]+) allocations=([0-9]+) passes=([0-9]+) submits=([0-9]+) disjoint=([0-9]+) physical=([0-9]+) saved=([0-9]+) permille=([0-9]+) timeline=([0-9]+):([0-9]+) completed=([01]) retired=([01]) ready=([01])"
receipt=exact("ral-frame-graph-native ",pattern,"INFO","renderer.ral")[0]
m=re.fullmatch(pattern,receipt[2]);v=list(map(int,m.groups()))
(schema,generation,graph,material,batch,recording,submission,native_submission,textures,allocations,passes,submits,disjoint,physical,saved,permille,timeline_base,timeline_final,completed,retired,ready)=v
if schema!=1 or generation!=1 or graph!=generation or material!=generation or recording!=generation or submission!=generation:raise SystemExit("FAIL ral-frame-graph-runtime generation cohort")
if batch<=0 or native_submission<=0:raise SystemExit("FAIL ral-frame-graph-runtime child generations")
if (textures,allocations,passes,submits)!=(3,1,6,1):raise SystemExit("FAIL ral-frame-graph-runtime shape")
if physical<=0 or disjoint<=physical or saved!=disjoint-physical or saved<=0:raise SystemExit("FAIL ral-frame-graph-runtime physical saving")
if permille!=(saved*1000)//disjoint or permille<=0:raise SystemExit("FAIL ral-frame-graph-runtime saving ratio")
if timeline_final!=timeline_base+1 or (completed,retired,ready)!=(1,1,1):raise SystemExit("FAIL ral-frame-graph-runtime completion")
if not first[0]<requested[0]<receipt[0]<complete[0]:raise SystemExit("FAIL ral-frame-graph-runtime causal order")
manifest=[]
for number,line in enumerate(open(manifest_path,encoding="utf-8",errors="strict"),1):
 try:item=json.loads(line)
 except Exception as exc:raise SystemExit(f"FAIL ral-frame-graph-runtime manifest JSON {number}: {exc}")
 if not isinstance(item,dict):raise SystemExit("FAIL ral-frame-graph-runtime manifest schema")
 manifest.append(item)
scenario={"kind":"scenario","schema":1,"name":"ral-frame-graph-runtime","map":expected_map,"backend":"vulkan","snapshot_command":"ral_dump live framegraph","expected_textures":3,"expected_allocations":1,"expected_passes":6,"expected_submits":1}
if not manifest or manifest[0]!=scenario:raise SystemExit("FAIL ral-frame-graph-runtime scenario")
if [x.get("role") for x in manifest[1:-1]] != ["gui","renderer","gamecl","gamesv","moltenvk","pax21","base","harness","bootstrap-cfg"]:raise SystemExit("FAIL ral-frame-graph-runtime provenance roles")
for item in manifest[1:-1]:
 if set(item)!={"kind","role","path","bytes","sha256"} or item.get("kind")!="provenance" or not isinstance(item.get("bytes"),int) or item["bytes"]<=0 or re.fullmatch(r"[0-9a-f]{64}",str(item.get("sha256",""))) is None:raise SystemExit("FAIL ral-frame-graph-runtime provenance schema")
 data=open(item["path"],"rb").read()
 if len(data)!=item["bytes"] or hashlib.sha256(data).hexdigest()!=item["sha256"]:raise SystemExit("FAIL ral-frame-graph-runtime provenance rehash")
result=manifest[-1]
if set(result)!={"kind","controller_pid","rc","timeout","forced"} or result.get("kind")!="result" or not isinstance(result.get("controller_pid"),int) or result["controller_pid"]<=0 or result.get("rc")!=0 or result.get("timeout") is not False or result.get("forced") is not False:raise SystemExit("FAIL ral-frame-graph-runtime result")
print(f"PASS native Vulkan frame graph {expected_map}: saved={saved} permille={permille} timeline={timeline_base}:{timeline_final}")
PYEOF
}

write_self() {
python3 - "$1" "$2" "$3" "$4" <<'PYEOF'
import hashlib,json,os,sys
log_path,manifest_path,expected_map,mode=sys.argv[1:]
def row(sev,cat,msg):return {"ts":"2026-08-21T12:00:00.000+03:00","sev":sev,"cat":cat,"msg":msg+"\n"}
receipt="ral-frame-graph-native schema=1 generation=1 graph=1 material=1 batch=1 recording=1 submission=1 native-submission=1 textures=3 allocations=1 passes=6 submits=1 disjoint=12288 physical=4096 saved=8192 permille=666 timeline=0:1 completed=1 retired=1 ready=1"
R=[row("INFO","client",f"cls.state: -> CA_ACTIVE (FIRST GAMEPLAY FRAME mapname=maps/{expected_map}.bsp serverTime=1 numEntities=2 framecount=3)"),row("INFO","system","Q0_RAL_FRAME_GRAPH_REQUESTED"),row("INFO","renderer.ral",receipt),row("INFO","system","Q0_RAL_FRAME_GRAPH_COMPLETE")]
def find(text):return next(i for i,x in enumerate(R) if text in x["msg"])
replacements={
 "schema":("schema=1","schema=2"),"generation":("generation=1","generation=2"),"graph":("graph=1","graph=2"),"material":("material=1","material=2"),"batch":("batch=1","batch=0"),"recording":("recording=1","recording=2"),"submission":("submission=1","submission=2"),"native-submission":("native-submission=1","native-submission=0"),"textures":("textures=3","textures=2"),"allocations":("allocations=1","allocations=2"),"passes":("passes=6","passes=5"),"submits":("submits=1","submits=2"),"disjoint":("disjoint=12288","disjoint=4096"),"physical":("physical=4096","physical=0"),"saved":("saved=8192","saved=8191"),"permille":("permille=666","permille=667"),"timeline":("timeline=0:1","timeline=0:2"),"completed":("completed=1","completed=0"),"retired":("retired=1","retired=0"),"ready":("ready=1","ready=0")}
if mode in replacements:
 old,new=replacements[mode];R[2]["msg"]=R[2]["msg"].replace(old,new)
elif mode=="missing":R.pop(2)
elif mode=="duplicate":R.insert(2,dict(R[2]))
elif mode=="failure":R.insert(2,row("WARN","renderer.ral","ral-frame-graph-native-failure generation=1"))
elif mode=="metadata":R[2]["cat"]="renderer"
elif mode=="suffix":R[2]["msg"]=R[2]["msg"].rstrip("\n")+" extra\n"
elif mode=="order":R[1],R[2]=R[2],R[1]
elif mode=="smuggle":R.append(row("INFO","system","benign\rral-frame-graph-native "+receipt))
elif mode=="vuid":R.append(row("WARN","renderer.ral","VUID-synthetic"))
elif mode=="severity":R.append(row("ERROR","renderer.ral","synthetic"))
elif mode=="map":R[0]["msg"]=R[0]["msg"].replace(expected_map,"arena0")
with open(log_path,"w") as out:
 for item in R:out.write(json.dumps(item)+"\n")
roles=("gui","renderer","gamecl","gamesv","moltenvk","pax21","base","harness","bootstrap-cfg")
M=[{"kind":"scenario","schema":1,"name":"ral-frame-graph-runtime","map":expected_map,"backend":"vulkan","snapshot_command":"ral_dump live framegraph","expected_textures":3,"expected_allocations":1,"expected_passes":6,"expected_submits":1}]
for role in roles:
 path=manifest_path+"."+role;data=("fixture-"+role).encode();open(path,"wb").write(data);M.append({"kind":"provenance","role":role,"path":os.path.abspath(path),"bytes":len(data),"sha256":hashlib.sha256(data).hexdigest()})
M.append({"kind":"result","controller_pid":101,"rc":0,"timeout":False,"forced":False})
if mode=="manifest":M[2]["sha256"]="0"*64
elif mode=="scenario":M[0]["expected_allocations"]=2
elif mode=="result":M[-1]["rc"]=1
with open(manifest_path,"w") as out:
 for item in M:out.write(json.dumps(item,sort_keys=True)+"\n")
PYEOF
}

if [ "${1:-}" = --analyze ];then
	[ "$#" -eq 4 ] || { echo "usage: $0 --analyze <qconsole.jsonl> <manifest.jsonl> <map>";exit 64; }
	analyze_contract "$2" "$3" "$4";exit $?
fi
if [ "${1:-}" = --self-test ];then
	ROOT="$(mktemp -d -t ral-frame-graph-runtime-self-XXXXXX 2>/dev/null || mktemp -d)";trap 'rm -rf "$ROOT"' EXIT
	write_self "$ROOT/clean.log" "$ROOT/clean.manifest" arena1 clean || exit 1
	analyze_contract "$ROOT/clean.log" "$ROOT/clean.manifest" arena1 >/dev/null || exit 1
	defects=(missing duplicate failure schema generation graph material batch recording submission native-submission textures allocations passes submits disjoint physical saved permille timeline completed retired ready metadata suffix order smuggle vuid severity map manifest scenario result)
	for defect in "${defects[@]}";do
		write_self "$ROOT/$defect.log" "$ROOT/$defect.manifest" arena1 "$defect" || exit 1
		if analyze_contract "$ROOT/$defect.log" "$ROOT/$defect.manifest" arena1 >/dev/null 2>&1;then echo "FAIL accepted $defect";exit 1;fi
	done
	echo "PASS ral-frame-graph-runtime analyzer self-test (${#defects[@]} mutations)";exit 0
fi

WIRED="${1:-}";[ -n "$WIRED" ] && [ -x "$WIRED" ] || { echo "usage: $0 /absolute/path/to/wired";exit 64; }
[ -f "$TIMEOUT_RUNNER" ] || { echo "SKIP: missing timeout runner";exit 77; }
WIRED="$(cd "$(dirname "$WIRED")" && pwd)/$(basename "$WIRED")";WD="$(dirname "$WIRED")"
find_required(){ local name="$1" candidate;shift;for candidate in "$@";do [ -f "$candidate/$name" ] && { printf '%s\n' "$candidate/$name";return 0;};done;return 1;}
RENDERER="$(find_required wired_vulkan_arm64.dylib "$WD" "$WD/Contents/MacOS" "$WD/q3now-preview.arm64.app/Contents/MacOS" "$WD/../MacOS")" || { echo "SKIP: current Vulkan renderer unavailable";exit 77; }
GAMECL="${WIRED_GAMECL:-}";[ -f "$GAMECL" ] || GAMECL="$(find_required gameclarm64.dylib "$WD/base" "$WD/Contents/Resources/base" "$WD/q3now-preview.arm64.app/Contents/Resources/base")" || { echo "SKIP: set WIRED_GAMECL to current gameclarm64.dylib";exit 77; }
GAMESV="${WIRED_GAMESV:-}";[ -f "$GAMESV" ] || GAMESV="$(find_required gamesvarm64.dylib "$WD/base" "$WD/Contents/Resources/base" "$WD/q3now-preview.arm64.app/Contents/Resources/base")" || { echo "SKIP: set WIRED_GAMESV to current gamesvarm64.dylib";exit 77; }
MOLTEN="$(find_required libMoltenVK.dylib "$WD" "$WD/Contents/MacOS" "$WD/q3now-preview.arm64.app/Contents/MacOS" "$WD/../MacOS" /opt/homebrew/opt/molten-vk/lib /usr/local/lib)" || { echo "SKIP: current MoltenVK unavailable";exit 77; }
CONTENT="${WIRED_CONTENT_ROOT:-}";PACK=""
if [ -n "$CONTENT" ] && [ -f "$CONTENT/base/pax21.sw3z" ];then PACK="$(cd "$CONTENT" && pwd)";else
	for candidate in "$WD" "$WD/../Resources" "$WD/../../.." "$WD/q3now-preview.arm64.app/Contents/Resources";do [ -f "$candidate/base/pax21.sw3z" ] && PACK="$(cd "$candidate" && pwd)" && break;done
fi
[ -n "$PACK" ] || { echo "SKIP: current pax21 unavailable";exit 77; }
CONTENT="${CONTENT:-$PACK}";if [ -f "$CONTENT/base/pax01.sw3z" ];then BASE="$CONTENT/base/pax01.sw3z";elif [ -f "$CONTENT/base/pak0.pk3" ];then BASE="$CONTENT/base/pak0.pk3";else echo "SKIP: set WIRED_CONTENT_ROOT";exit 77;fi

run_map() {
	local MAP="$1" ROOT HOME_DIR RUN BOOT MANIFEST QCONSOLE STDOUT RC TIMEOUT FORCED=0 dep candidate
	ROOT="$(mktemp -d -t "ral-frame-graph-$MAP-XXXXXX" 2>/dev/null || mktemp -d)";HOME_DIR="$ROOT/home";RUN="$ROOT/runtime"
	mkdir -p "$HOME_DIR/base" "$RUN/Contents/MacOS";cp "$PACK/base/pax21.sw3z" "$HOME_DIR/base/" || return 1;cp "$BASE" "$HOME_DIR/base/" || return 1;cp "$GAMECL" "$HOME_DIR/base/gameclarm64.dylib" || return 1;cp "$GAMESV" "$HOME_DIR/base/gamesvarm64.dylib" || return 1;cp "$WIRED" "$RUN/wired" || return 1;chmod +x "$RUN/wired";cp "$RENDERER" "$MOLTEN" "$RUN/Contents/MacOS/" || return 1
	if command -v otool >/dev/null 2>&1;then
		otool -L "$WIRED" 2>/dev/null | sed -n 's|^[[:space:]]*@executable_path/\([^ ]*\).*|\1|p' | while read -r dep;do
			[ -n "$dep" ] || continue
			for candidate in "$WD" "$WD/Contents/MacOS" "$WD/q3now-preview.arm64.app/Contents/MacOS" "$WD/../MacOS";do [ -f "$candidate/$dep" ] && { cp "$candidate/$dep" "$RUN/";break; };done
		done
	fi
	BOOT="$HOME_DIR/base/ral-frame-graph-runtime.cfg";printf '%s\n' 'log renderer.ral info' 'set activeAction "wait 60; echo Q0_RAL_FRAME_GRAPH_REQUESTED; ral_dump live framegraph; echo Q0_RAL_FRAME_GRAPH_COMPLETE; quit"' "map $MAP" >"$BOOT"
	MANIFEST="$ROOT/manifest.jsonl";python3 - "$MANIFEST" "$MAP" "$WIRED" "$RENDERER" "$GAMECL" "$GAMESV" "$MOLTEN" "$PACK/base/pax21.sw3z" "$BASE" "$0" "$BOOT" <<'PYEOF'
import hashlib,json,os,sys
with open(sys.argv[1],"w") as out:
 out.write(json.dumps({"kind":"scenario","schema":1,"name":"ral-frame-graph-runtime","map":sys.argv[2],"backend":"vulkan","snapshot_command":"ral_dump live framegraph","expected_textures":3,"expected_allocations":1,"expected_passes":6,"expected_submits":1},sort_keys=True)+"\n")
 for role,path in zip(("gui","renderer","gamecl","gamesv","moltenvk","pax21","base","harness","bootstrap-cfg"),sys.argv[3:]):
  data=open(path,"rb").read();out.write(json.dumps({"kind":"provenance","role":role,"path":os.path.abspath(path),"bytes":len(data),"sha256":hashlib.sha256(data).hexdigest()},sort_keys=True)+"\n")
PYEOF
	QCONSOLE="$HOME_DIR/qconsole.jsonl";STDOUT="$ROOT/stdout.log"
	python3 "$TIMEOUT_RUNNER" --timeout 90 --kill-after 15 --cwd "$RUN" --stdout "$STDOUT" -- "$RUN/wired" +set fs_basepath "$HOME_DIR" +set fs_homepath "$HOME_DIR" +set fs_game base +set vm_game 0 +set vm_cgame 0 +set sv_cheats 1 +set sv_pure 0 +set com_automated 1 +set com_noHardReboot 1 +set s_initsound 0 +set r_fullscreen 0 +set r_mode -1 +set r_customwidth 1280 +set r_customheight 720 +set r_vkValidate 1 +set r_bloom 0 +set r_ssao 0 +set r_smaa 0 +set r_forwardPlus 0 +set r_drawSunRays 0 +set r_shadows 0 +set log_severity DEBUG +set log_file_severity DEBUG +set log_file_mode overwrite_synced +exec ral-frame-graph-runtime.cfg
	RC=$?;TIMEOUT=false;[ "$RC" -eq 124 ] && TIMEOUT=true;python3 - "$MANIFEST" "$$" "$RC" "$TIMEOUT" "$FORCED" <<'PYEOF'
import json,sys
with open(sys.argv[1],"a") as out:out.write(json.dumps({"kind":"result","controller_pid":int(sys.argv[2]),"rc":int(sys.argv[3]),"timeout":sys.argv[4]=="true","forced":sys.argv[5]=="1"},sort_keys=True)+"\n")
PYEOF
	if [ ! -f "$QCONSOLE" ] || ! analyze_contract "$QCONSOLE" "$MANIFEST" "$MAP";then echo "FAIL retained root: $ROOT";return 1;fi
	echo "PASS retained root: $ROOT"
	[ "${WIRED_KEEP_ARTIFACTS:-0}" = 1 ] || rm -rf "$ROOT"
}

run_map arena1 || exit 1
run_map arena17 || exit 1
echo "PASS native Vulkan frame graph arena1 + arena17"
