#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Wired Engine contributors

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
TIMEOUT_RUNNER="$SCRIPT_DIR/run-with-timeout.py"

analyze_contract() {
python3 - "$1" "$2" <<'PYEOF'
import hashlib,json,os,re,sys
stdout_path,manifest_path=sys.argv[1:]
lines=[line.rstrip("\n") for line in open(stdout_path,encoding="utf-8",errors="strict") if line.strip()]
if not lines:raise SystemExit("FAIL profile-host empty stdout")
if any("VUID-" in line or "ERROR" in line or "FATAL" in line or re.match(r"^RAL_PROFILE_HOST_LOG severity=(?:17|21)(?: |$)",line) for line in lines):
 raise SystemExit("FAIL profile-host forbidden diagnostic")
claimed=("RAL_PROFILE_HOST recreate=","RAL_PROFILE_HOST imgui=","RAL_PROFILE_HOST frame=","RAL_PROFILE_HOST event=","RAL_PROFILE_HOST action=")
def family(prefix):return [(i,line) for i,line in enumerate(lines) if line.startswith(prefix)]
def exact(prefix,pattern,count=1):
 found=family(prefix)
 if len(found)!=count:raise SystemExit(f"FAIL profile-host {prefix} cardinality {len(found)}")
 for _,line in found:
  if re.fullmatch(pattern,line) is None:raise SystemExit(f"FAIL profile-host {prefix} body {line}")
 return found
ready=exact("RAL_PROFILE_HOST recreate=",r"RAL_PROFILE_HOST recreate=ready generation=[1-9][0-9]* extent=[1-9][0-9]*x[1-9][0-9]* images=[1-9][0-9]* usage=0x[0-9a-f]+",3)
ready_values=[]
for _,line in ready:
 m=re.fullmatch(r"RAL_PROFILE_HOST recreate=ready generation=([1-9][0-9]*) extent=([1-9][0-9]*)x([1-9][0-9]*) images=([1-9][0-9]*) usage=0x([0-9a-f]+)",line)
 value=tuple(map(int,m.groups()[:4]))+(int(m.group(5),16),)
 if (value[4]&0x4)==0:raise SystemExit("FAIL profile-host missing color usage")
 ready_values.append(value)
if not (ready_values[0][0] < ready_values[1][0] < ready_values[2][0]):raise SystemExit("FAIL profile-host generation monotonicity")
if ready_values[0][1:3] == ready_values[1][1:3]:raise SystemExit("FAIL profile-host physical resize unchanged")
if ready_values[1][1:3] != ready_values[2][1:3]:raise SystemExit("FAIL profile-host restore extent drift")
frames=exact("RAL_PROFILE_HOST frame=",r"RAL_PROFILE_HOST frame=presented phase=(?:initial|resized|restored) generation=[1-9][0-9]* image=[0-9]+ acquire=(?:success|suboptimal) target=canonical prepare=success submit=success present=success",3)
imgui=exact("RAL_PROFILE_HOST imgui=",r"RAL_PROFILE_HOST imgui=drawn phase=(?:initial|resized|restored) generation=[1-9][0-9]* framebuffer=[1-9][0-9]*x[1-9][0-9]* lists=[1-9][0-9]* vertices=[1-9][0-9]* indices=[1-9][0-9]* draws=[1-9][0-9]* scissors=[1-9][0-9]* texture-binds=[1-9][0-9]* font-id=[1-9][0-9]* font=[1-9][0-9]*x[1-9][0-9]* vertex-cap=[1-9][0-9]* index-cap=[1-9][0-9]*",3)
frame_values=[]
for _,line in frames:
 m=re.fullmatch(r"RAL_PROFILE_HOST frame=presented phase=(initial|resized|restored) generation=([1-9][0-9]*) image=([0-9]+) acquire=(success|suboptimal) target=canonical prepare=success submit=success present=success",line)
 frame_values.append((m.group(1),int(m.group(2)),int(m.group(3))))
if [v[0] for v in frame_values] != ["initial","resized","restored"]:raise SystemExit("FAIL profile-host phase vector")
for frame,ready_value in zip(frame_values,ready_values):
 if frame[1]!=ready_value[0] or frame[2]>=ready_value[3]:raise SystemExit("FAIL profile-host frame generation/image binding")
imgui_values=[]
for _,line in imgui:
 m=re.fullmatch(r"RAL_PROFILE_HOST imgui=drawn phase=(initial|resized|restored) generation=([1-9][0-9]*) framebuffer=([1-9][0-9]*)x([1-9][0-9]*) lists=([1-9][0-9]*) vertices=([1-9][0-9]*) indices=([1-9][0-9]*) draws=([1-9][0-9]*) scissors=([1-9][0-9]*) texture-binds=([1-9][0-9]*) font-id=([1-9][0-9]*) font=([1-9][0-9]*)x([1-9][0-9]*) vertex-cap=([1-9][0-9]*) index-cap=([1-9][0-9]*)",line)
 imgui_values.append((m.group(1),)+tuple(map(int,m.groups()[1:])))
if [v[0] for v in imgui_values] != ["initial","resized","restored"]:raise SystemExit("FAIL profile-host ImGui phase vector")
for ui,ready_value in zip(imgui_values,ready_values):
 if ui[1]!=ready_value[0] or ui[2:4]!=ready_value[1:3]:raise SystemExit("FAIL profile-host ImGui generation/framebuffer binding")
 if ui[4]>ui[5] or ui[6]<ui[7]*3 or ui[8]!=ui[7] or ui[9]!=3:raise SystemExit("FAIL profile-host ImGui count relationship")
 if ui[13] < ui[5]*20 or ui[14] < ui[6]*2:raise SystemExit("FAIL profile-host ImGui buffer capacity")
if len({v[10] for v in imgui_values})!=1 or len({v[11:13] for v in imgui_values})!=1:raise SystemExit("FAIL profile-host font identity drift")
pixel=exact("RAL_PROFILE_HOST event=pixel-size-changed",r"RAL_PROFILE_HOST event=pixel-size-changed physical=[1-9][0-9]*x[1-9][0-9]*")[0]
suspended=exact("RAL_PROFILE_HOST event=suspended",r"RAL_PROFILE_HOST event=suspended acquire-delta=0 submit-delta=0 present-delta=0 imgui-delta=0")[0]
restored=exact("RAL_PROFILE_HOST event=restored",r"RAL_PROFILE_HOST event=restored physical=[1-9][0-9]*x[1-9][0-9]*")[0]
def event_extent(line):
 m=re.search(r"physical=([1-9][0-9]*)x([1-9][0-9]*)$",line);return tuple(map(int,m.groups()))
if event_extent(pixel[1])!=ready_values[1][1:3] or event_extent(restored[1])!=ready_values[2][1:3]:raise SystemExit("FAIL profile-host event/swapchain extent binding")
complete=exact("RAL_PROFILE_HOST action=",r"RAL_PROFILE_HOST action=complete acquire=3 submit=3 present=3")[0]
order=[ready[0][0],imgui[0][0],frames[0][0],pixel[0],ready[1][0],imgui[1][0],frames[1][0],suspended[0],restored[0],ready[2][0],imgui[2][0],frames[2][0],complete[0]]
if any(a>=b for a,b in zip(order,order[1:])):raise SystemExit(f"FAIL profile-host causal order {order}")
for line in lines:
 if line.startswith(claimed) and not any(line==x[1] for x in ready+imgui+frames+[pixel,suspended,restored,complete]):
  raise SystemExit(f"FAIL profile-host additive semantic line {line}")
manifest=[json.loads(line) for line in open(manifest_path,encoding="utf-8",errors="strict") if line.strip()]
scenario={"kind":"scenario","schema":2,"name":"ral-profile-host-lifecycle","backend":"vulkan","window":"sdl3-high-pixel-density","command":"--lifecycle-proof"}
if not manifest or manifest[0]!=scenario:raise SystemExit("FAIL profile-host scenario")
if [x.get("role") for x in manifest[1:-1]] != ["tool","sdl3","moltenvk","harness"]:raise SystemExit("FAIL profile-host provenance roles")
for item in manifest[1:-1]:
 if set(item)!={"kind","role","path","bytes","sha256"} or item.get("kind")!="provenance" or not isinstance(item.get("bytes"),int) or item["bytes"]<=0 or re.fullmatch(r"[0-9a-f]{64}",str(item.get("sha256",""))) is None:raise SystemExit("FAIL profile-host provenance schema")
 data=open(item["path"],"rb").read()
 if len(data)!=item["bytes"] or hashlib.sha256(data).hexdigest()!=item["sha256"]:raise SystemExit("FAIL profile-host provenance rehash")
result=manifest[-1]
if set(result)!={"kind","controller_pid","rc","timeout","forced"} or result.get("kind")!="result" or not isinstance(result.get("controller_pid"),int) or result["controller_pid"]<=0 or result.get("rc")!=0 or result.get("timeout") is not False or result.get("forced") is not False:raise SystemExit("FAIL profile-host result")
print("PASS standalone SDL3/RAL physical resize, suspend and restore lifecycle")
PYEOF
}

write_self() {
python3 - "$1" "$2" "$3" <<'PYEOF'
import hashlib,json,os,sys
out_path,manifest_path,mode=sys.argv[1:]
R=[
"RAL_PROFILE_HOST_LOG severity=9 queue families: graphics=0 compute=0 transfer=0 (compute aliases graphics) (transfer aliases graphics)",
"RAL_PROFILE_HOST_LOG severity=9 Vulkan backend ready: Test GPU [Vulkan 1.3.0] (queue families gfx/cmp/xfer = 0/0/0; debugUtils=yes memBudget=yes descriptorIndexing=no sync2=yes timeline=yes drawIndirectCount=no anisotropy=1x)",
"RAL_PROFILE_HOST recreate=ready generation=1 extent=1280x720 images=3 usage=0x4",
"RAL_PROFILE_HOST imgui=drawn phase=initial generation=1 framebuffer=1280x720 lists=2 vertices=120 indices=180 draws=4 scissors=4 texture-binds=3 font-id=1 font=512x128 vertex-cap=4096 index-cap=1024",
"RAL_PROFILE_HOST frame=presented phase=initial generation=1 image=0 acquire=success target=canonical prepare=success submit=success present=success",
"RAL_PROFILE_HOST event=pixel-size-changed physical=1600x900",
"RAL_PROFILE_HOST recreate=ready generation=2 extent=1600x900 images=3 usage=0x4",
"RAL_PROFILE_HOST imgui=drawn phase=resized generation=2 framebuffer=1600x900 lists=2 vertices=120 indices=180 draws=4 scissors=4 texture-binds=3 font-id=1 font=512x128 vertex-cap=4096 index-cap=1024",
"RAL_PROFILE_HOST frame=presented phase=resized generation=2 image=0 acquire=success target=canonical prepare=success submit=success present=success",
"RAL_PROFILE_HOST event=suspended acquire-delta=0 submit-delta=0 present-delta=0 imgui-delta=0",
"RAL_PROFILE_HOST event=restored physical=1600x900",
"RAL_PROFILE_HOST recreate=ready generation=3 extent=1600x900 images=3 usage=0x4",
"RAL_PROFILE_HOST imgui=drawn phase=restored generation=3 framebuffer=1600x900 lists=2 vertices=120 indices=180 draws=4 scissors=4 texture-binds=3 font-id=1 font=512x128 vertex-cap=4096 index-cap=1024",
"RAL_PROFILE_HOST frame=presented phase=restored generation=3 image=0 acquire=success target=canonical prepare=success submit=success present=success",
"RAL_PROFILE_HOST action=complete acquire=3 submit=3 present=3"]
def find(text):return next(i for i,x in enumerate(R) if text in x)
if mode=="missing":R.pop(find("frame=presented phase=resized"))
elif mode=="duplicate":R.insert(find("frame=presented phase=resized"),R[find("frame=presented phase=resized")])
elif mode=="generation":R[find("generation=3")]=R[find("generation=3")].replace("generation=3","generation=2")
elif mode=="same-extent":R[find("generation=1")]=R[find("generation=1")].replace("1280x720","1600x900")
elif mode=="restore-drift":R[find("generation=3")]=R[find("generation=3")].replace("1600x900","1440x810")
elif mode=="image-oob":R[find("frame=presented phase=restored")]=R[find("frame=presented phase=restored")].replace("image=0","image=3")
elif mode=="target":R[find("frame=presented phase=resized")]=R[find("frame=presented phase=resized")].replace("target=canonical","target=duplicate")
elif mode=="prepare":R[find("frame=presented phase=resized")]=R[find("frame=presented phase=resized")].replace("prepare=success","prepare=failed")
elif mode=="submit":R[find("frame=presented phase=resized")]=R[find("frame=presented phase=resized")].replace("submit=success","submit=issued")
elif mode=="present":R[find("frame=presented phase=resized")]=R[find("frame=presented phase=resized")].replace("present=success","present=out-of-date")
elif mode=="gpu-while-suspended":R[find("event=suspended")]=R[find("event=suspended")].replace("acquire-delta=0","acquire-delta=1")
elif mode=="event-extent":R[find("event=pixel")]=R[find("event=pixel")].replace("1600x900","1500x844")
elif mode=="usage":R[find("generation=1")]=R[find("generation=1")].replace("usage=0x4","usage=0x0")
elif mode=="imgui-missing":R.pop(find("imgui=drawn phase=resized"))
elif mode=="imgui-empty":R[find("imgui=drawn phase=resized")]=R[find("imgui=drawn phase=resized")].replace("lists=2","lists=0")
elif mode=="imgui-framebuffer":R[find("imgui=drawn phase=resized")]=R[find("imgui=drawn phase=resized")].replace("1600x900","1500x844",1)
elif mode=="imgui-font":R[find("imgui=drawn phase=restored")]=R[find("imgui=drawn phase=restored")].replace("font-id=1","font-id=2")
elif mode=="imgui-count":R[find("imgui=drawn phase=resized")]=R[find("imgui=drawn phase=resized")].replace("draws=4 scissors=4","draws=5 scissors=4")
elif mode=="imgui-texture-map":R[find("imgui=drawn phase=resized")]=R[find("imgui=drawn phase=resized")].replace("texture-binds=3","texture-binds=1")
elif mode=="imgui-texture-overbind":R[find("imgui=drawn phase=resized")]=R[find("imgui=drawn phase=resized")].replace("texture-binds=3","texture-binds=4")
elif mode=="imgui-triangles":R[find("imgui=drawn phase=resized")]=R[find("imgui=drawn phase=resized")].replace("indices=180","indices=2")
elif mode=="imgui-capacity":R[find("imgui=drawn phase=resized")]=R[find("imgui=drawn phase=resized")].replace("vertex-cap=4096 index-cap=1024","vertex-cap=1 index-cap=1")
elif mode=="imgui-suspended":R[find("event=suspended")]=R[find("event=suspended")].replace("imgui-delta=0","imgui-delta=1")
elif mode=="additive":R.append("RAL_PROFILE_HOST recreate=result-failed code=7 retained=1 pixels=1600x900")
elif mode=="vuid":R.append("VUID-synthetic")
elif mode=="severity":R.append("RAL_PROFILE_HOST_LOG severity=17 synthetic backend failure")
open(out_path,"w").write("\n".join(R)+"\n")
M=[{"kind":"scenario","schema":2,"name":"ral-profile-host-lifecycle","backend":"vulkan","window":"sdl3-high-pixel-density","command":"--lifecycle-proof"}]
for role in ("tool","sdl3","moltenvk","harness"):
 path=manifest_path+"."+role;data=("fixture-"+role).encode();open(path,"wb").write(data)
 M.append({"kind":"provenance","role":role,"path":os.path.abspath(path),"bytes":len(data),"sha256":hashlib.sha256(data).hexdigest()})
M.append({"kind":"result","controller_pid":101,"rc":0,"timeout":False,"forced":False})
if mode=="manifest":M[1]["sha256"]="0"*64
elif mode=="result":M[-1]["rc"]=1
with open(manifest_path,"w") as out:
 for item in M:out.write(json.dumps(item,sort_keys=True)+"\n")
PYEOF
}

if [ "${1:-}" = --analyze ]; then
	[ "$#" -eq 3 ] || { echo "usage: $0 --analyze <stdout.log> <manifest.jsonl>"; exit 64; }
	analyze_contract "$2" "$3"; exit $?
fi
if [ "${1:-}" = --self-test ]; then
	ROOT="$(mktemp -d -t ral-profile-host-self-XXXXXX 2>/dev/null || mktemp -d)"; trap 'rm -rf "$ROOT"' EXIT
	write_self "$ROOT/clean.log" "$ROOT/clean.manifest" clean || exit 1
	analyze_contract "$ROOT/clean.log" "$ROOT/clean.manifest" >/dev/null || exit 1
	defects=(missing duplicate generation same-extent restore-drift image-oob target prepare submit present gpu-while-suspended event-extent usage imgui-missing imgui-empty imgui-framebuffer imgui-font imgui-count imgui-texture-map imgui-texture-overbind imgui-triangles imgui-capacity imgui-suspended additive vuid severity manifest result)
	for defect in "${defects[@]}"; do
		write_self "$ROOT/$defect.log" "$ROOT/$defect.manifest" "$defect" || exit 1
		if analyze_contract "$ROOT/$defect.log" "$ROOT/$defect.manifest" >/dev/null 2>&1; then echo "FAIL accepted $defect"; exit 1; fi
	done
	echo "PASS ral-profile-host lifecycle analyzer self-test (${#defects[@]} mutations)"; exit 0
fi

TOOL="${1:-}"
[ -n "$TOOL" ] && [ -x "$TOOL" ] || { echo "usage: $0 /absolute/path/to/wired_profile_host"; exit 64; }
[ "$(uname -s)" = Darwin ] || { echo "SKIP real profile-host lifecycle requires macOS/MoltenVK"; exit 77; }
[ -f "$TIMEOUT_RUNNER" ] || { echo "FAIL missing timeout runner"; exit 1; }
TOOL="$(cd "$(dirname "$TOOL")" && pwd)/$(basename "$TOOL")"
SDL3_PATH="$(otool -L "$TOOL" | awk '/libSDL3.*[.]dylib/{print $1; exit}')"
MOLTENVK_PATH=""
for candidate in /opt/homebrew/opt/molten-vk/lib/libMoltenVK.dylib /usr/local/lib/libMoltenVK.dylib; do
	[ -f "$candidate" ] && { MOLTENVK_PATH="$candidate"; break; }
done
[ -f "$SDL3_PATH" ] || { echo "FAIL linked SDL3 dylib unavailable"; exit 1; }
[ -f "$MOLTENVK_PATH" ] || { echo "FAIL MoltenVK dylib unavailable"; exit 1; }
ROOT="$(mktemp -d -t ral-profile-host-XXXXXX 2>/dev/null || mktemp -d)"
cleanup(){ local status=$?; trap - EXIT INT TERM; [ "${WIRED_KEEP_ARTIFACTS:-0}" = 1 ] || rm -rf "$ROOT"; exit "$status"; }
trap cleanup EXIT; trap 'exit 130' INT; trap 'exit 143' TERM
mkdir -p "$ROOT/runtime"
cp "$TOOL" "$ROOT/runtime/wired_profile_host" || exit 1
chmod +x "$ROOT/runtime/wired_profile_host"
cp "$0" "$ROOT/harness.sh" || exit 1
STDOUT="$ROOT/stdout.log"; MANIFEST="$ROOT/manifest.jsonl"
python3 - "$MANIFEST" "$ROOT/runtime/wired_profile_host" "$SDL3_PATH" "$MOLTENVK_PATH" "$ROOT/harness.sh" <<'PYEOF'
import hashlib,json,os,sys
with open(sys.argv[1],"w") as out:
 out.write(json.dumps({"kind":"scenario","schema":2,"name":"ral-profile-host-lifecycle","backend":"vulkan","window":"sdl3-high-pixel-density","command":"--lifecycle-proof"},sort_keys=True)+"\n")
 for role,path in zip(("tool","sdl3","moltenvk","harness"),sys.argv[2:]):
  data=open(path,"rb").read();out.write(json.dumps({"kind":"provenance","role":role,"path":os.path.abspath(path),"bytes":len(data),"sha256":hashlib.sha256(data).hexdigest()},sort_keys=True)+"\n")
PYEOF
python3 "$TIMEOUT_RUNNER" --timeout 30 --kill-after 5 --cwd "$ROOT/runtime" --stdout "$STDOUT" -- "$ROOT/runtime/wired_profile_host" --lifecycle-proof
RC=$?; TIMEOUT=false; [ "$RC" -eq 124 ] && TIMEOUT=true
python3 - "$MANIFEST" "$$" "$RC" "$TIMEOUT" <<'PYEOF'
import json,sys
with open(sys.argv[1],"a") as out:out.write(json.dumps({"kind":"result","controller_pid":int(sys.argv[2]),"rc":int(sys.argv[3]),"timeout":sys.argv[4]=="true","forced":False},sort_keys=True)+"\n")
PYEOF
if ! analyze_contract "$STDOUT" "$MANIFEST"; then
	if [ "${WIRED_KEEP_ARTIFACTS:-0}" = 1 ]; then echo "FAIL retained root: $ROOT";
	else echo "FAIL lifecycle contract (temporary artifacts removed)"; fi
	exit 1
fi
echo "PASS retained root: $ROOT"
