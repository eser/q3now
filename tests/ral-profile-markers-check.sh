#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Exact-process receipt for semantic RAL dynamic-rendering GPU debug labels.

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
TIMEOUT_RUNNER="$SCRIPT_DIR/run-with-timeout.py"

analyze_contract() {
python3 - "$1" "$2" <<'PYEOF'
import hashlib,json,os,re,sys
log_path,manifest_path=sys.argv[1:]
manifest=[]
for number,line in enumerate(open(manifest_path,encoding="utf-8",errors="strict"),1):
 try:item=json.loads(line)
 except Exception as exc:raise SystemExit(f"FAIL ral-profile manifest JSON {number}: {exc}")
 if not isinstance(item,dict):raise SystemExit("FAIL ral-profile manifest schema")
 manifest.append(item)
if not manifest or manifest[0].get("map") not in ("arena1","arena7"):raise SystemExit("FAIL ral-profile scenario map")
scenario_map=manifest[0]["map"]
rows=[]
for number,line in enumerate(open(log_path,encoding="utf-8",errors="strict"),1):
 if not line.strip():continue
 try:row=json.loads(line)
 except Exception as exc:raise SystemExit(f"FAIL ral-profile JSON {number}: {exc}")
 if not isinstance(row,dict) or not all(isinstance(row.get(k),str) for k in ("ts","sev","cat","msg")):raise SystemExit("FAIL ral-profile log schema")
 rows.append(row)
if not rows:raise SystemExit("FAIL ral-profile empty evidence")
def norm(value):return value[:-1] if value.endswith("\n") and not value.endswith("\n\n") else value
vals=[norm(row["msg"]) for row in rows]
claimed=("RAL profile markers:","Q0_RAL_PROFILE_MARKERS_","cls.state: -> CA_ACTIVE (FIRST GAMEPLAY FRAME","----- Server Shutdown ","==== ShutdownGame ====")
for value in vals:
 parts=re.split(r"[\r\n]",value)
 if len(parts)>1 and any(part.startswith(claimed) for part in parts):raise SystemExit("FAIL ral-profile claimed logical-line smuggling")
if any(row["sev"].upper() in ("ERROR","FATAL") for row in rows):raise SystemExit("FAIL ral-profile severity")
for value in vals:
 if "VUID-" in value or value.startswith("Ral_BeginRendering: prior debug-label") or value.startswith("Ral_EndRendering: active debug-label"):
  raise SystemExit(f"FAIL ral-profile forbidden marker: {value}")
def exact(prefix,pattern,sev,cat,count=1):
 found=[(i,row,value) for i,(row,value) in enumerate(zip(rows,vals)) if value.startswith(prefix)]
 if len(found)!=count:raise SystemExit(f"FAIL ral-profile {prefix} cardinality {len(found)}")
 for _,row,value in found:
  if row["sev"].upper()!=sev or row["cat"].lower()!=cat or re.fullmatch(pattern,value) is None:
   raise SystemExit(f"FAIL ral-profile {prefix} body/metadata: {value}")
 return found
first=exact("cls.state: -> CA_ACTIVE (FIRST GAMEPLAY FRAME",rf"cls\.state: -> CA_ACTIVE \(FIRST GAMEPLAY FRAME mapname=maps/{re.escape(scenario_map)}\.bsp serverTime=[0-9]+ numEntities=[0-9]+ framecount=[0-9]+\)","INFO","client")[0]
family=exact("RAL profile markers:",r"RAL profile markers: action=(?:armed|complete source=vulkan-debug-utils requested=5 begin=5 end=5 unique=5 mask=0x201d)","INFO","renderer.ral",2)
if [item[2] for item in family] != ["RAL profile markers: action=armed","RAL profile markers: action=complete source=vulkan-debug-utils requested=5 begin=5 end=5 unique=5 mask=0x201d"]:raise SystemExit("FAIL ral-profile decision vector")
markers=exact("Q0_RAL_PROFILE_MARKERS_",r"Q0_RAL_PROFILE_MARKERS_(?:REQUESTED|COMPLETE)","INFO","system",2)
if [item[2] for item in markers] != ["Q0_RAL_PROFILE_MARKERS_REQUESTED","Q0_RAL_PROFILE_MARKERS_COMPLETE"]:raise SystemExit("FAIL ral-profile marker vector")
shutdown=exact("----- Server Shutdown ",r"----- Server Shutdown \(Server quit\) -----","INFO","server")[0]
shutdown_game=exact("==== ShutdownGame ====",r"==== ShutdownGame ====","INFO","game")[0]
if not first[0]<markers[0][0]<family[0][0]<family[1][0]<markers[1][0]<shutdown[0]<shutdown_game[0]:raise SystemExit("FAIL ral-profile causal order")
scenario={"kind":"scenario","schema":1,"name":"ral-profile-markers","map":scenario_map,"backend":"vulkan","profile_markers":1,"expected_mask":"0x201d","expected_names":["wired.main","wired.tonemap","wired.ui","wired.scene-depth-resume","wired.present"]}
if not manifest or manifest[0]!=scenario:raise SystemExit("FAIL ral-profile scenario")
if [x.get("role") for x in manifest[1:-1]] != ["gui","renderer","moltenvk","pax21","base","harness","bootstrap-cfg"]:raise SystemExit("FAIL ral-profile provenance roles")
for item in manifest[1:-1]:
 if set(item)!={"kind","role","path","bytes","sha256"} or item.get("kind")!="provenance" or not isinstance(item.get("bytes"),int) or item["bytes"]<=0 or re.fullmatch(r"[0-9a-f]{64}",str(item.get("sha256",""))) is None:raise SystemExit("FAIL ral-profile provenance schema")
 data=open(item["path"],"rb").read()
 if len(data)!=item["bytes"] or hashlib.sha256(data).hexdigest()!=item["sha256"]:raise SystemExit("FAIL ral-profile provenance rehash")
result=manifest[-1]
if set(result)!={"kind","controller_pid","rc","timeout","forced"} or result.get("kind")!="result" or not isinstance(result.get("controller_pid"),int) or result["controller_pid"]<=0 or result.get("rc")!=0 or result.get("timeout") is not False or result.get("forced") is not False:raise SystemExit("FAIL ral-profile result")
print("PASS exact semantic RAL Vulkan debug-label receipt")
PYEOF
}

write_self() {
python3 - "$1" "$2" "$3" <<'PYEOF'
import hashlib,json,os,sys
log_path,manifest_path,mode=sys.argv[1:]
def row(sev,cat,msg):return {"ts":"2026-08-13T12:00:00.000+03:00","sev":sev,"cat":cat,"msg":msg+"\n"}
R=[row("INFO","client","cls.state: -> CA_ACTIVE (FIRST GAMEPLAY FRAME mapname=maps/arena7.bsp serverTime=1 numEntities=2 framecount=3)"),row("INFO","system","Q0_RAL_PROFILE_MARKERS_REQUESTED"),row("INFO","renderer.ral","RAL profile markers: action=armed"),row("INFO","renderer.ral","RAL profile markers: action=complete source=vulkan-debug-utils requested=5 begin=5 end=5 unique=5 mask=0x201d"),row("INFO","system","Q0_RAL_PROFILE_MARKERS_COMPLETE"),row("INFO","server","----- Server Shutdown (Server quit) -----"),row("INFO","game","==== ShutdownGame ====")]
def find(text):return next(i for i,x in enumerate(R) if text in x["msg"])
if mode=="missing":R.pop(find("action=complete"))
elif mode=="duplicate":R.insert(find("action=complete"),dict(R[find("action=complete")]))
elif mode=="refused":R[find("action=complete")]=row("WARN","renderer.ral","RAL profile markers: action=refused reason=debug-utils-unavailable")
elif mode=="requested":R[find("action=complete")]["msg"]=R[find("action=complete")]["msg"].replace("requested=5","requested=4")
elif mode=="begin":R[find("action=complete")]["msg"]=R[find("action=complete")]["msg"].replace("begin=5","begin=4")
elif mode=="end":R[find("action=complete")]["msg"]=R[find("action=complete")]["msg"].replace("end=5","end=4")
elif mode=="unique":R[find("action=complete")]["msg"]=R[find("action=complete")]["msg"].replace("unique=5","unique=4")
elif mode=="mask":R[find("action=complete")]["msg"]=R[find("action=complete")]["msg"].replace("0x201d","0x200d")
elif mode=="metadata":R[find("action=complete")]["cat"]="renderer.vk"
elif mode=="suffix":R[find("action=complete")]["msg"]=R[find("action=complete")]["msg"].rstrip("\n")+" extra\n"
elif mode=="order":a=find("action=armed");b=find("action=complete");R[a],R[b]=R[b],R[a]
elif mode=="request-order":a=find("Q0_RAL_PROFILE_MARKERS_REQUESTED");b=find("action=armed");R[a],R[b]=R[b],R[a]
elif mode=="smuggle":R.append(row("INFO","system","benign\rRAL profile markers: action=complete source=vulkan-debug-utils requested=5 begin=5 end=5 unique=5 mask=0x201d"))
elif mode=="vuid":R.append(row("WARN","renderer.ral","VUID-synthetic"))
elif mode=="unbalanced":R.append(row("WARN","renderer.ral","Ral_EndRendering: active debug-label scope could not close"))
elif mode=="map-mismatch":R[0]["msg"]=R[0]["msg"].replace("arena7.bsp","arena1.bsp")
with open(log_path,"w") as out:
 for item in R:out.write(json.dumps(item)+"\n")
roles=("gui","renderer","moltenvk","pax21","base","harness","bootstrap-cfg")
M=[{"kind":"scenario","schema":1,"name":"ral-profile-markers","map":"arena7","backend":"vulkan","profile_markers":1,"expected_mask":"0x201d","expected_names":["wired.main","wired.tonemap","wired.ui","wired.scene-depth-resume","wired.present"]}]
for role in roles:
 path=manifest_path+"."+role;data=("fixture-"+role).encode();open(path,"wb").write(data);M.append({"kind":"provenance","role":role,"path":os.path.abspath(path),"bytes":len(data),"sha256":hashlib.sha256(data).hexdigest()})
M.append({"kind":"result","controller_pid":101,"rc":0,"timeout":False,"forced":False})
if mode=="manifest":M[2]["sha256"]="0"*64
elif mode=="scenario":M[0]["expected_mask"]="0x200d"
elif mode=="scenario-name":M[0]["expected_names"].remove("wired.scene-depth-resume")
elif mode=="scenario-map":M[0]["map"]="arena99"
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
 ROOT="$(mktemp -d -t ral-profile-self-XXXXXX 2>/dev/null || mktemp -d)";trap 'rm -rf "$ROOT"' EXIT
 write_self "$ROOT/clean.log" "$ROOT/clean.manifest" clean || exit 1
 analyze_contract "$ROOT/clean.log" "$ROOT/clean.manifest" >/dev/null || exit 1
 defects=(missing duplicate refused requested begin end unique mask metadata suffix order request-order smuggle vuid unbalanced map-mismatch manifest scenario scenario-name scenario-map result)
 for defect in "${defects[@]}";do
  write_self "$ROOT/$defect.log" "$ROOT/$defect.manifest" "$defect" || exit 1
  if analyze_contract "$ROOT/$defect.log" "$ROOT/$defect.manifest" >/dev/null 2>&1;then echo "FAIL accepted $defect";exit 1;fi
 done
 echo "PASS ral-profile-markers analyzer self-test (${#defects[@]} mutations)";exit 0
fi

WIRED="${1:-}";[ -n "$WIRED" ] && [ -x "$WIRED" ] || { echo "usage: $0 /absolute/path/to/wired";exit 64; }
[ -z "${WIRED_MAP:-}" ] || [[ "$WIRED_MAP" =~ ^arena(1|7)$ ]] || { echo "FAIL: WIRED_MAP must be arena1 or arena7";exit 64; }
MAP="${WIRED_MAP:-arena7}"
[ -f "$TIMEOUT_RUNNER" ] || { echo "SKIP: missing timeout runner";exit 77; }
WIRED="$(cd "$(dirname "$WIRED")" && pwd)/$(basename "$WIRED")";WD="$(dirname "$WIRED")"
find_required(){ local name="$1" candidate;shift;for candidate in "$@";do [ -f "$candidate/$name" ] && { printf '%s\n' "$candidate/$name";return 0;};done;return 1;}
RENDERER="$(find_required wired_vulkan_arm64.dylib "$WD" "$WD/Contents/MacOS" "$WD/q3now-preview.arm64.app/Contents/MacOS" "$WD/../MacOS")" || { echo "SKIP: current Vulkan renderer unavailable";exit 77; }
MOLTEN="$(find_required libMoltenVK.dylib "$WD" "$WD/Contents/MacOS" "$WD/q3now-preview.arm64.app/Contents/MacOS" "$WD/../MacOS")" || { echo "SKIP: current MoltenVK unavailable";exit 77; }
PACK="";for candidate in "$WD" "$WD/../Resources" "$WD/../../.." "$WD/q3now-preview.arm64.app/Contents/Resources";do [ -f "$candidate/base/pax21.sw3z" ] && PACK="$(cd "$candidate" && pwd)" && break;done;[ -n "$PACK" ] || { echo "SKIP: current pax21 unavailable";exit 77; }
CONTENT="${WIRED_CONTENT_ROOT:-$PACK}";if [ -f "$CONTENT/base/pax01.sw3z" ];then BASE="$CONTENT/base/pax01.sw3z";elif [ -f "$CONTENT/base/pak0.pk3" ];then BASE="$CONTENT/base/pak0.pk3";else echo "SKIP: set WIRED_CONTENT_ROOT";exit 77;fi
ROOT="$(mktemp -d -t ral-profile-markers-XXXXXX 2>/dev/null || mktemp -d)";HOME_DIR="$ROOT/home";RUN="$ROOT/runtime";FORCED=0
cleanup(){ local status=$?;trap - EXIT INT TERM;[ "${WIRED_KEEP_ARTIFACTS:-0}" = 1 ] || rm -rf "$ROOT";exit "$status";};trap cleanup EXIT;trap 'exit 130' INT;trap 'exit 143' TERM
mkdir -p "$HOME_DIR/base" "$RUN/Contents/MacOS";cp "$PACK/base/pax21.sw3z" "$HOME_DIR/base/" || exit 1;cp "$BASE" "$HOME_DIR/base/" || exit 1;cp "$WIRED" "$RUN/wired" || exit 1;chmod +x "$RUN/wired";cp "$RENDERER" "$MOLTEN" "$RUN/Contents/MacOS/" || exit 1
# Anything the binary resolves through @executable_path must sit beside the copy
# we just made, NOT in Contents/MacOS: the renderer and MoltenVK are dlopen'd on
# a relative search path, but hard-linked dependencies are looked up by dyld
# against the executable's own directory. Ask the binary which ones those are
# instead of hardcoding a list — a shipped bundle links libSDL3/libcrypto, a
# plain devel build may link neither, and guessing gets it wrong in both
# directions. Missing files are left alone; the run then fails loudly on its own.
if command -v otool >/dev/null 2>&1;then
	otool -L "$WIRED" 2>/dev/null | sed -n 's|^[[:space:]]*@executable_path/\([^ ]*\).*|\1|p' | while read -r dep;do
		[ -n "$dep" ] || continue
		for candidate in "$WD" "$WD/Contents/MacOS" "$WD/q3now-preview.arm64.app/Contents/MacOS" "$WD/../MacOS";do
			[ -f "$candidate/$dep" ] && { cp "$candidate/$dep" "$RUN/";break; }
		done
	done
fi
BOOT="$HOME_DIR/base/ral-profile-markers.cfg";printf '%s\n' 'log renderer.ral info' 'set activeAction "echo Q0_RAL_PROFILE_MARKERS_REQUESTED; r_profileMarkers 1; wait 30; echo Q0_RAL_PROFILE_MARKERS_COMPLETE; quit"' "map $MAP" >"$BOOT"
MANIFEST="$ROOT/manifest.jsonl";python3 - "$MANIFEST" "$MAP" "$WIRED" "$RENDERER" "$MOLTEN" "$PACK/base/pax21.sw3z" "$BASE" "$0" "$BOOT" <<'PYEOF'
import hashlib,json,os,sys
with open(sys.argv[1],"w") as out:
 out.write(json.dumps({"kind":"scenario","schema":1,"name":"ral-profile-markers","map":sys.argv[2],"backend":"vulkan","profile_markers":1,"expected_mask":"0x201d","expected_names":["wired.main","wired.tonemap","wired.ui","wired.scene-depth-resume","wired.present"]},sort_keys=True)+"\n")
 for role,path in zip(("gui","renderer","moltenvk","pax21","base","harness","bootstrap-cfg"),sys.argv[3:]):
  data=open(path,"rb").read();out.write(json.dumps({"kind":"provenance","role":role,"path":os.path.abspath(path),"bytes":len(data),"sha256":hashlib.sha256(data).hexdigest()},sort_keys=True)+"\n")
PYEOF
QCONSOLE="$HOME_DIR/qconsole.jsonl";STDOUT="$ROOT/stdout.log"
python3 "$TIMEOUT_RUNNER" --timeout 90 --kill-after 15 --cwd "$RUN" --stdout "$STDOUT" -- "$RUN/wired" +set fs_basepath "$HOME_DIR" +set fs_homepath "$HOME_DIR" +set fs_game base +set sv_pure 0 +set com_automated 1 +set com_noHardReboot 1 +set s_initsound 0 +set r_fullscreen 0 +set r_mode -1 +set r_customwidth 1280 +set r_customheight 720 +set r_vkValidate 1 +set r_profileMarkers 0 +set r_bloom 0 +set r_ssao 0 +set r_smaa 0 +set r_forwardPlus 0 +set r_drawSunRays 0 +set r_shadows 0 +set log_severity DEBUG +set log_file_severity DEBUG +set log_file_mode overwrite_synced +exec ral-profile-markers.cfg
RC=$?;TIMEOUT=false;[ "$RC" -eq 124 ] && TIMEOUT=true;python3 - "$MANIFEST" "$$" "$RC" "$TIMEOUT" "$FORCED" <<'PYEOF'
import json,sys
with open(sys.argv[1],"a") as out:out.write(json.dumps({"kind":"result","controller_pid":int(sys.argv[2]),"rc":int(sys.argv[3]),"timeout":sys.argv[4]=="true","forced":sys.argv[5]=="1"},sort_keys=True)+"\n")
PYEOF
[ -f "$QCONSOLE" ] || { echo "FAIL missing qconsole: $ROOT";exit 1; };analyze_contract "$QCONSOLE" "$MANIFEST" || { echo "FAIL retained root: $ROOT";exit 1; }
echo "PASS retained root: $ROOT"
