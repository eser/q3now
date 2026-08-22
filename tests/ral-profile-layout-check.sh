#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Exact-process proof that semantic GPU timing accumulation resets on lane-layout changes.

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
 except Exception as exc:raise SystemExit(f"FAIL ral-profile-layout JSON {number}: {exc}")
 if not isinstance(row,dict) or not all(isinstance(row.get(k),str) for k in ("ts","sev","cat","msg")):raise SystemExit("FAIL ral-profile-layout log schema")
 rows.append(row)
if not rows:raise SystemExit("FAIL ral-profile-layout empty evidence")
def norm(value):return value[:-1] if value.endswith("\n") and not value.endswith("\n\n") else value
vals=[norm(row["msg"]) for row in rows]
claimed=("gpuProfile: action=epoch-","RAL profile snapshot:","RAL profile lane:","Q0_RAL_PROFILE_LAYOUT_","cls.state: -> CA_ACTIVE (FIRST GAMEPLAY FRAME","----- Server Shutdown ","==== ShutdownGame ====")
for value in vals:
 parts=re.split(r"[\r\n]",value)
 if len(parts)>1 and any(part.startswith(claimed) for part in parts):raise SystemExit("FAIL ral-profile-layout claimed logical-line smuggling")
if any(row["sev"].upper() in ("ERROR","FATAL") for row in rows):raise SystemExit("FAIL ral-profile-layout severity")
for value in vals:
 if "VUID-" in value or value.startswith("gpuProfile: action=sample-rejected"):
  raise SystemExit(f"FAIL ral-profile-layout forbidden row: {value}")
def exact(prefix,pattern,sev,cat,count=1):
 found=[(i,row,value) for i,(row,value) in enumerate(zip(rows,vals)) if value.startswith(prefix)]
 if len(found)!=count:raise SystemExit(f"FAIL ral-profile-layout {prefix} cardinality {len(found)}")
 for _,row,value in found:
  if row["sev"].upper()!=sev or row["cat"].lower()!=cat or re.fullmatch(pattern,value) is None:
   raise SystemExit(f"FAIL ral-profile-layout {prefix} body/metadata: {value}")
 return found
first=exact("cls.state: -> CA_ACTIVE (FIRST GAMEPLAY FRAME",r"cls\.state: -> CA_ACTIVE \(FIRST GAMEPLAY FRAME mapname=maps/arena7\.bsp serverTime=[0-9]+ numEntities=[0-9]+ framecount=[0-9]+\)","INFO","client")[0]
markers=exact("Q0_RAL_PROFILE_LAYOUT_",r"Q0_RAL_PROFILE_LAYOUT_(?:REQUESTED|BASE_READY|SMAA_ON_REQUESTED|SMAA_ON_READY|SMAA_OFF_REQUESTED|COMPLETE)","INFO","system",6)
expected_markers=["Q0_RAL_PROFILE_LAYOUT_REQUESTED","Q0_RAL_PROFILE_LAYOUT_BASE_READY","Q0_RAL_PROFILE_LAYOUT_SMAA_ON_REQUESTED","Q0_RAL_PROFILE_LAYOUT_SMAA_ON_READY","Q0_RAL_PROFILE_LAYOUT_SMAA_OFF_REQUESTED","Q0_RAL_PROFILE_LAYOUT_COMPLETE"]
if [item[2] for item in markers]!=expected_markers:raise SystemExit("FAIL ral-profile-layout marker vector")
epochs=exact("gpuProfile: action=epoch-",r"gpuProfile: action=(?:epoch-start epoch=1|epoch-reset reason=semantic-layout-change epoch=[23]) lanes=[1-9][0-9]* labels=[a-z][a-z0-9_]*(?:,[a-z][a-z0-9_]*)*","INFO","renderer.timing",3)
patterns=(
 r"gpuProfile: action=epoch-start epoch=1 lanes=([1-9][0-9]*) labels=(.+)",
 r"gpuProfile: action=epoch-reset reason=semantic-layout-change epoch=2 lanes=([1-9][0-9]*) labels=(.+)",
 r"gpuProfile: action=epoch-reset reason=semantic-layout-change epoch=3 lanes=([1-9][0-9]*) labels=(.+)")
layouts=[]
for item,pattern in zip(epochs,patterns):
 match=re.fullmatch(pattern,item[2]);
 if match is None:raise SystemExit("FAIL ral-profile-layout epoch vector")
 labels=match.group(2).split(",")
 if len(labels)!=int(match.group(1)) or len(labels)!=len(set(labels)):raise SystemExit("FAIL ral-profile-layout lane cardinality/uniqueness")
 layouts.append(labels)
base,enabled,reverted=layouts
if "smaa" in base or "present_prep" not in base:raise SystemExit("FAIL ral-profile-layout baseline semantic anchors")
expected_enabled=list(base);expected_enabled.insert(expected_enabled.index("present_prep"),"smaa")
if enabled!=expected_enabled:raise SystemExit("FAIL ral-profile-layout SMAA relation")
if reverted!=base:raise SystemExit("FAIL ral-profile-layout reverted topology drift")
snapshots=exact("RAL profile snapshot:",r"RAL profile snapshot: source=ral-native-query epoch=[123] lanes=[1-9][0-9]* samples=(?:[1-9]|[1-9][0-9]|1[01][0-9]|120) labels=[a-z][a-z0-9_]*(?:,[a-z][a-z0-9_]*)*","INFO","renderer.timing",3)
snapshot_layouts=[]
snapshot_samples=[]
for expected_epoch,item in enumerate(snapshots,1):
 match=re.fullmatch(r"RAL profile snapshot: source=ral-native-query epoch=([123]) lanes=([1-9][0-9]*) samples=([1-9][0-9]*) labels=(.+)",item[2])
 if match is None or int(match.group(1))!=expected_epoch:raise SystemExit("FAIL ral-profile-layout snapshot epoch")
 labels=match.group(4).split(",")
 if len(labels)!=int(match.group(2)) or int(match.group(3))>120:raise SystemExit("FAIL ral-profile-layout snapshot bounds")
 snapshot_layouts.append(labels);snapshot_samples.append(int(match.group(3)))
if snapshot_layouts!=layouts:raise SystemExit("FAIL ral-profile-layout snapshot topology binding")
lane_rows=exact("RAL profile lane:",r"RAL profile lane: epoch=[123] index=[0-9]+ name=[a-z][a-z0-9_]* latest-ms=[0-9]+\.[0-9]{3} average-ms=[0-9]+\.[0-9]{3} min-ms=[0-9]+\.[0-9]{3} max-ms=[0-9]+\.[0-9]{3}","INFO","renderer.timing",sum(map(len,layouts)))
lane_cursor=0
for epoch_index,(snapshot_item,labels) in enumerate(zip(snapshots,layouts),1):
 for index,label in enumerate(labels):
  lane=lane_rows[lane_cursor];lane_cursor+=1
  match=re.fullmatch(r"RAL profile lane: epoch=([123]) index=([0-9]+) name=([a-z][a-z0-9_]*) latest-ms=([0-9]+\.[0-9]{3}) average-ms=([0-9]+\.[0-9]{3}) min-ms=([0-9]+\.[0-9]{3}) max-ms=([0-9]+\.[0-9]{3})",lane[2])
  if match is None or int(match.group(1))!=epoch_index or int(match.group(2))!=index or match.group(3)!=label:raise SystemExit("FAIL ral-profile-layout lane identity")
  latest,average,minimum,maximum=map(float,match.groups()[3:])
  if not minimum<=latest<=maximum or not minimum<=average<=maximum:raise SystemExit("FAIL ral-profile-layout lane statistics")
  if lane[0] != snapshot_item[0]+1+index:raise SystemExit("FAIL ral-profile-layout snapshot/lane adjacency")
shutdown=exact("----- Server Shutdown ",r"----- Server Shutdown \(Server quit\) -----","INFO","server")[0]
shutdown_game=exact("==== ShutdownGame ====",r"==== ShutdownGame ====","INFO","game")[0]
if not first[0]<markers[0][0]<epochs[0][0]<markers[1][0]<snapshots[0][0]<markers[2][0]<epochs[1][0]<markers[3][0]<snapshots[1][0]<markers[4][0]<epochs[2][0]<snapshots[2][0]<markers[5][0]<shutdown[0]<shutdown_game[0]:raise SystemExit("FAIL ral-profile-layout causal order")
manifest=[]
for number,line in enumerate(open(manifest_path,encoding="utf-8",errors="strict"),1):
 try:item=json.loads(line)
 except Exception as exc:raise SystemExit(f"FAIL ral-profile-layout manifest JSON {number}: {exc}")
 if not isinstance(item,dict):raise SystemExit("FAIL ral-profile-layout manifest schema")
 manifest.append(item)
scenario={"kind":"scenario","schema":1,"name":"ral-profile-layout","map":"arena7","backend":"vulkan","gpu_speeds":1,"toggle":"r_smaa:0->1->0","expected_relation":"baseline + smaa -> baseline","history_capacity":120,"snapshot_command":"ral_dump live profile"}
if not manifest or manifest[0]!=scenario:raise SystemExit("FAIL ral-profile-layout scenario")
if [x.get("role") for x in manifest[1:-1]] != ["gui","renderer","moltenvk","pax21","base","harness","bootstrap-cfg"]:raise SystemExit("FAIL ral-profile-layout provenance roles")
for item in manifest[1:-1]:
 if set(item)!={"kind","role","path","bytes","sha256"} or item.get("kind")!="provenance" or not isinstance(item.get("bytes"),int) or item["bytes"]<=0 or re.fullmatch(r"[0-9a-f]{64}",str(item.get("sha256",""))) is None:raise SystemExit("FAIL ral-profile-layout provenance schema")
 data=open(item["path"],"rb").read()
 if len(data)!=item["bytes"] or hashlib.sha256(data).hexdigest()!=item["sha256"]:raise SystemExit("FAIL ral-profile-layout provenance rehash")
result=manifest[-1]
if set(result)!={"kind","controller_pid","rc","timeout","forced"} or result.get("kind")!="result" or not isinstance(result.get("controller_pid"),int) or result["controller_pid"]<=0 or result.get("rc")!=0 or result.get("timeout") is not False or result.get("forced") is not False:raise SystemExit("FAIL ral-profile-layout result")
print("PASS exact semantic GPU profile topology epochs")
PYEOF
}

write_self() {
python3 - "$1" "$2" "$3" <<'PYEOF'
import hashlib,json,os,sys
log_path,manifest_path,mode=sys.argv[1:]
def row(sev,cat,msg):return {"ts":"2026-08-13T12:00:00.000+03:00","sev":sev,"cat":cat,"msg":msg+"\n"}
base=["dlight_shadow_start","dlight_shadow","world_done","present_prep"]
enabled=["dlight_shadow_start","dlight_shadow","world_done","smaa","present_prep"]
def profile(epoch,labels,samples):
 out=[row("INFO","renderer.timing",f"RAL profile snapshot: source=ral-native-query epoch={epoch} lanes={len(labels)} samples={samples} labels={','.join(labels)}")]
 for index,label in enumerate(labels):
  value=0.100+index*0.010
  out.append(row("INFO","renderer.timing",f"RAL profile lane: epoch={epoch} index={index} name={label} latest-ms={value:.3f} average-ms={value:.3f} min-ms={value-0.010:.3f} max-ms={value+0.010:.3f}"))
 return out
R=[row("INFO","client","cls.state: -> CA_ACTIVE (FIRST GAMEPLAY FRAME mapname=maps/arena7.bsp serverTime=1 numEntities=2 framecount=3)"),row("INFO","system","Q0_RAL_PROFILE_LAYOUT_REQUESTED"),row("INFO","renderer.timing","gpuProfile: action=epoch-start epoch=1 lanes=4 labels="+",".join(base)),row("INFO","system","Q0_RAL_PROFILE_LAYOUT_BASE_READY"),*profile(1,base,80),row("INFO","system","Q0_RAL_PROFILE_LAYOUT_SMAA_ON_REQUESTED"),row("INFO","renderer.timing","gpuProfile: action=epoch-reset reason=semantic-layout-change epoch=2 lanes=5 labels="+",".join(enabled)),row("INFO","system","Q0_RAL_PROFILE_LAYOUT_SMAA_ON_READY"),*profile(2,enabled,120),row("INFO","system","Q0_RAL_PROFILE_LAYOUT_SMAA_OFF_REQUESTED"),row("INFO","renderer.timing","gpuProfile: action=epoch-reset reason=semantic-layout-change epoch=3 lanes=4 labels="+",".join(base)),*profile(3,base,120),row("INFO","system","Q0_RAL_PROFILE_LAYOUT_COMPLETE"),row("INFO","server","----- Server Shutdown (Server quit) -----"),row("INFO","game","==== ShutdownGame ====")]
def find(text):return next(i for i,x in enumerate(R) if text in x["msg"])
if mode=="missing":R.pop(find("epoch=2"))
elif mode=="duplicate":R.insert(find("epoch=2"),dict(R[find("epoch=2")]))
elif mode=="baseline-smaa":R[find("epoch=1")]["msg"]=R[find("epoch=1")]["msg"].replace("world_done,present","world_done,smaa,present").replace("lanes=4","lanes=5")
elif mode=="enabled-missing":R[find("epoch=2")]["msg"]=R[find("epoch=2")]["msg"].replace(",smaa","").replace("lanes=5","lanes=4")
elif mode=="enabled-extra":R[find("epoch=2")]["msg"]=R[find("epoch=2")]["msg"].replace("smaa,present","smaa,bloom,present").replace("lanes=5","lanes=6")
elif mode=="enabled-order":R[find("epoch=2")]["msg"]=R[find("epoch=2")]["msg"].replace("world_done,smaa","smaa,world_done")
elif mode=="revert-drift":R[find("epoch=3")]["msg"]=R[find("epoch=3")]["msg"].replace("world_done","bloom")
elif mode=="epoch":R[find("epoch=3")]["msg"]=R[find("epoch=3")]["msg"].replace("epoch=3","epoch=4")
elif mode=="reason":R[find("epoch=2")]["msg"]=R[find("epoch=2")]["msg"].replace("semantic-layout-change","manual")
elif mode=="metadata":R[find("epoch=2")]["cat"]="renderer.ral"
elif mode=="suffix":R[find("epoch=2")]["msg"]=R[find("epoch=2")]["msg"].rstrip("\n")+" extra\n"
elif mode=="marker":R.pop(find("SMAA_ON_READY"))
elif mode=="order":a=find("SMAA_ON_REQUESTED");b=find("epoch=2");R[a],R[b]=R[b],R[a]
elif mode=="additive":R.append(row("INFO","renderer.timing","gpuProfile: action=epoch-reset reason=semantic-layout-change epoch=4 lanes=4 labels=dlight_shadow_start,dlight_shadow,world_done,present_prep"))
elif mode=="smuggle":R.append(row("INFO","system","benign\rgpuProfile: action=epoch-start epoch=1 lanes=1 labels=present_prep"))
elif mode=="vuid":R.append(row("WARN","renderer.ral","VUID-synthetic"))
elif mode=="rejected":R.append(row("WARN","renderer.timing","gpuProfile: action=sample-rejected reason=invalid-semantic-layout"))
elif mode=="snapshot-missing":R.pop(find("RAL profile snapshot: source=ral-native-query epoch=2"))
elif mode=="snapshot-topology":R[find("RAL profile snapshot: source=ral-native-query epoch=2")]["msg"]=R[find("RAL profile snapshot: source=ral-native-query epoch=2")]["msg"].replace(",smaa","").replace("lanes=5","lanes=4")
elif mode=="snapshot-epoch":R[find("RAL profile snapshot: source=ral-native-query epoch=2")]["msg"]=R[find("RAL profile snapshot: source=ral-native-query epoch=2")]["msg"].replace("epoch=2","epoch=3")
elif mode=="snapshot-samples":R[find("RAL profile snapshot: source=ral-native-query epoch=2")]["msg"]=R[find("RAL profile snapshot: source=ral-native-query epoch=2")]["msg"].replace("samples=120","samples=121")
elif mode=="snapshot-refused":R[find("RAL profile snapshot: source=ral-native-query epoch=2")]=row("WARN","renderer.timing","RAL profile snapshot: action=refused reason=no-completed-samples")
elif mode=="lane-missing":R.pop(find("RAL profile lane: epoch=2 index=1"))
elif mode=="lane-name":R[find("RAL profile lane: epoch=2 index=1")]["msg"]=R[find("RAL profile lane: epoch=2 index=1")]["msg"].replace("name=dlight_shadow","name=bloom")
elif mode=="lane-index":R[find("RAL profile lane: epoch=2 index=1")]["msg"]=R[find("RAL profile lane: epoch=2 index=1")]["msg"].replace("index=1","index=9")
elif mode=="lane-metadata":R[find("RAL profile lane: epoch=2 index=1")]["cat"]="renderer.ral"
elif mode=="lane-range":R[find("RAL profile lane: epoch=2 index=1")]["msg"]=R[find("RAL profile lane: epoch=2 index=1")]["msg"].replace("average-ms=0.110","average-ms=9.000")
elif mode=="lane-suffix":R[find("RAL profile lane: epoch=2 index=1")]["msg"]=R[find("RAL profile lane: epoch=2 index=1")]["msg"].rstrip("\n")+" extra\n"
with open(log_path,"w") as out:
 for item in R:out.write(json.dumps(item)+"\n")
roles=("gui","renderer","moltenvk","pax21","base","harness","bootstrap-cfg")
M=[{"kind":"scenario","schema":1,"name":"ral-profile-layout","map":"arena7","backend":"vulkan","gpu_speeds":1,"toggle":"r_smaa:0->1->0","expected_relation":"baseline + smaa -> baseline","history_capacity":120,"snapshot_command":"ral_dump live profile"}]
for role in roles:
 path=manifest_path+"."+role;data=("fixture-"+role).encode();open(path,"wb").write(data);M.append({"kind":"provenance","role":role,"path":os.path.abspath(path),"bytes":len(data),"sha256":hashlib.sha256(data).hexdigest()})
M.append({"kind":"result","controller_pid":101,"rc":0,"timeout":False,"forced":False})
if mode=="manifest":M[2]["sha256"]="0"*64
elif mode=="scenario":M[0]["toggle"]="r_smaa:0->1"
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
 ROOT="$(mktemp -d -t ral-profile-layout-self-XXXXXX 2>/dev/null || mktemp -d)";trap 'rm -rf "$ROOT"' EXIT
 write_self "$ROOT/clean.log" "$ROOT/clean.manifest" clean || exit 1
 analyze_contract "$ROOT/clean.log" "$ROOT/clean.manifest" >/dev/null || exit 1
 defects=(missing duplicate baseline-smaa enabled-missing enabled-extra enabled-order revert-drift epoch reason metadata suffix marker order additive smuggle vuid rejected snapshot-missing snapshot-topology snapshot-epoch snapshot-samples snapshot-refused lane-missing lane-name lane-index lane-metadata lane-range lane-suffix manifest scenario result)
 for defect in "${defects[@]}";do
  write_self "$ROOT/$defect.log" "$ROOT/$defect.manifest" "$defect" || exit 1
  if analyze_contract "$ROOT/$defect.log" "$ROOT/$defect.manifest" >/dev/null 2>&1;then echo "FAIL accepted $defect";exit 1;fi
 done
 echo "PASS ral-profile-layout analyzer self-test (${#defects[@]} mutations)";exit 0
fi

WIRED="${1:-}";[ -n "$WIRED" ] && [ -x "$WIRED" ] || { echo "usage: $0 /absolute/path/to/wired";exit 64; }
[ -f "$TIMEOUT_RUNNER" ] || { echo "SKIP: missing timeout runner";exit 77; }
WIRED="$(cd "$(dirname "$WIRED")" && pwd)/$(basename "$WIRED")";WD="$(dirname "$WIRED")"
find_required(){ local name="$1" candidate;shift;for candidate in "$@";do [ -f "$candidate/$name" ] && { printf '%s\n' "$candidate/$name";return 0;};done;return 1;}
RENDERER="$(find_required wired_vulkan_arm64.dylib "$WD" "$WD/Contents/MacOS" "$WD/q3now-preview.arm64.app/Contents/MacOS" "$WD/../MacOS")" || { echo "SKIP: current Vulkan renderer unavailable";exit 77; }
MOLTEN="$(find_required libMoltenVK.dylib "$WD" "$WD/Contents/MacOS" "$WD/q3now-preview.arm64.app/Contents/MacOS" "$WD/../MacOS")" || { echo "SKIP: current MoltenVK unavailable";exit 77; }
PACK="";for candidate in "$WD" "$WD/../Resources" "$WD/../../.." "$WD/q3now-preview.arm64.app/Contents/Resources";do [ -f "$candidate/base/pax21.sw3z" ] && PACK="$(cd "$candidate" && pwd)" && break;done;[ -n "$PACK" ] || { echo "SKIP: current pax21 unavailable";exit 77; }
CONTENT="${WIRED_CONTENT_ROOT:-$PACK}";if [ -f "$CONTENT/base/pax01.sw3z" ];then BASE="$CONTENT/base/pax01.sw3z";elif [ -f "$CONTENT/base/pak0.pk3" ];then BASE="$CONTENT/base/pak0.pk3";else echo "SKIP: set WIRED_CONTENT_ROOT";exit 77;fi
ROOT="$(mktemp -d -t ral-profile-layout-XXXXXX 2>/dev/null || mktemp -d)";HOME_DIR="$ROOT/home";RUN="$ROOT/runtime";FORCED=0
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

BOOT="$HOME_DIR/base/ral-profile-layout.cfg";printf '%s\n' 'log renderer.timing info' 'set activeAction "echo Q0_RAL_PROFILE_LAYOUT_REQUESTED; r_gpuSpeeds 1; wait 90; echo Q0_RAL_PROFILE_LAYOUT_BASE_READY; ral_dump live profile; echo Q0_RAL_PROFILE_LAYOUT_SMAA_ON_REQUESTED; r_smaa 1; wait 180; echo Q0_RAL_PROFILE_LAYOUT_SMAA_ON_READY; ral_dump live profile; echo Q0_RAL_PROFILE_LAYOUT_SMAA_OFF_REQUESTED; r_smaa 0; wait 180; ral_dump live profile; echo Q0_RAL_PROFILE_LAYOUT_COMPLETE; quit"' 'map arena7' >"$BOOT"
MANIFEST="$ROOT/manifest.jsonl";python3 - "$MANIFEST" "$WIRED" "$RENDERER" "$MOLTEN" "$PACK/base/pax21.sw3z" "$BASE" "$0" "$BOOT" <<'PYEOF'
import hashlib,json,os,sys
with open(sys.argv[1],"w") as out:
 out.write(json.dumps({"kind":"scenario","schema":1,"name":"ral-profile-layout","map":"arena7","backend":"vulkan","gpu_speeds":1,"toggle":"r_smaa:0->1->0","expected_relation":"baseline + smaa -> baseline","history_capacity":120,"snapshot_command":"ral_dump live profile"},sort_keys=True)+"\n")
 for role,path in zip(("gui","renderer","moltenvk","pax21","base","harness","bootstrap-cfg"),sys.argv[2:]):
  data=open(path,"rb").read();out.write(json.dumps({"kind":"provenance","role":role,"path":os.path.abspath(path),"bytes":len(data),"sha256":hashlib.sha256(data).hexdigest()},sort_keys=True)+"\n")
PYEOF
QCONSOLE="$HOME_DIR/qconsole.jsonl";STDOUT="$ROOT/stdout.log"
python3 "$TIMEOUT_RUNNER" --timeout 90 --kill-after 15 --cwd "$RUN" --stdout "$STDOUT" -- "$RUN/wired" +set fs_basepath "$HOME_DIR" +set fs_homepath "$HOME_DIR" +set fs_game base +set sv_cheats 1 +set sv_pure 0 +set com_automated 1 +set com_noHardReboot 1 +set s_initsound 0 +set r_fullscreen 0 +set r_mode -1 +set r_customwidth 1280 +set r_customheight 720 +set r_vkValidate 1 +set r_gpuSpeeds 0 +set r_bloom 0 +set r_ssao 0 +set r_smaa 0 +set r_forwardPlus 0 +set r_drawSunRays 0 +set r_shadows 0 +set log_severity DEBUG +set log_file_severity DEBUG +set log_file_mode overwrite_synced +exec ral-profile-layout.cfg
RC=$?;TIMEOUT=false;[ "$RC" -eq 124 ] && TIMEOUT=true;python3 - "$MANIFEST" "$$" "$RC" "$TIMEOUT" "$FORCED" <<'PYEOF'
import json,sys
with open(sys.argv[1],"a") as out:out.write(json.dumps({"kind":"result","controller_pid":int(sys.argv[2]),"rc":int(sys.argv[3]),"timeout":sys.argv[4]=="true","forced":sys.argv[5]=="1"},sort_keys=True)+"\n")
PYEOF
[ -f "$QCONSOLE" ] || { echo "FAIL missing qconsole: $ROOT";exit 1; };analyze_contract "$QCONSOLE" "$MANIFEST" || { echo "FAIL retained root: $ROOT";exit 1; }
echo "PASS retained root: $ROOT"
