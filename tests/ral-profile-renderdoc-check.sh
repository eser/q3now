#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Independent-tool receipt for the semantic RAL debug labels.
#
# The sibling gate (ral-profile-markers-check.sh) proves the ENGINE reports it
# emitted five balanced debug-utils scopes. That is the engine grading its own
# homework: it counts its own vkCmdBegin/EndDebugUtilsLabelEXT calls and writes
# the tally to qconsole. This gate answers the other half — does an INDEPENDENT
# GPU tool actually resolve those labels as nested scopes in a real capture?
#
# That question is what acceptance #12 of the Phase 7.12 workstream originally
# aimed at. It was written as "Optick capture loads with semantic naming", on
# the assumption that Optick reads the labels an application already emits. It
# does not: Optick's Vulkan path needs OPTICK_GPU_EVENT macros and its own
# timestamp query pools, compiled into the process. RenderDoc, Nsight, RGP and
# the validation layers DO read plain debug-utils labels, so the criterion is
# tool-proven here instead of brand-bound there. See the wiki:
# realms/wired-engine/concepts/abandoned-directions.md (§9, 2026-08-20).
#
# Platform: RenderDoc supports Windows/Linux/Android — NOT macOS. On a host
# without renderdoccmd/qrenderdoc this gate SKIPs (77) rather than failing, and
# --self-test still exercises the whole analyzer offline against hostile
# fixtures, so the parsing contract is proven on every platform.

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
VR_DIR="$(cd "$SCRIPT_DIR/../tools/visual_regression" 2>/dev/null && pwd)"

# The five scopes the current minimal frame must publish, in recorded order.
# Kept identical to the sibling gate's expected_names so the two receipts
# cannot silently drift apart.
EXPECTED_NAMES='wired.main,wired.tonemap,wired.ui,wired.scene-depth-resume,wired.present'

analyze_contract() {
python3 - "$1" "$2" <<'PYEOF'
import hashlib,json,re,sys
capture_path,manifest_path=sys.argv[1:]
manifest=[]
for number,line in enumerate(open(manifest_path,encoding="utf-8",errors="strict"),1):
 try:item=json.loads(line)
 except Exception as exc:raise SystemExit(f"FAIL renderdoc manifest JSON {number}: {exc}")
 if not isinstance(item,dict):raise SystemExit("FAIL renderdoc manifest schema")
 manifest.append(item)
if not manifest:raise SystemExit("FAIL renderdoc empty manifest")
scenario=manifest[0]
if set(scenario)!={"kind","schema","name","map","backend","tool","expected_names"} or scenario.get("kind")!="scenario" or scenario.get("schema")!=1 or scenario.get("name")!="ral-profile-renderdoc" or scenario.get("backend")!="vulkan" or scenario.get("tool")!="renderdoc":
 raise SystemExit("FAIL renderdoc scenario")
if scenario.get("map") not in ("arena1","arena7"):raise SystemExit("FAIL renderdoc scenario map")
expected=scenario.get("expected_names")
if not isinstance(expected,list) or len(expected)!=5 or len(set(expected))!=5:raise SystemExit("FAIL renderdoc scenario names")

# Provenance: every input that can change the answer is hashed, so a receipt
# cannot be replayed against a different binary/capture than it claims.
roles=[x.get("role") for x in manifest[1:-1]]
if roles!=["wired","renderer","capture","harness"]:raise SystemExit("FAIL renderdoc provenance roles")
for item in manifest[1:-1]:
 if set(item)!={"kind","role","path","bytes","sha256"} or item.get("kind")!="provenance" or not isinstance(item.get("bytes"),int) or item["bytes"]<=0 or re.fullmatch(r"[0-9a-f]{64}",str(item.get("sha256",""))) is None:
  raise SystemExit("FAIL renderdoc provenance schema")
 data=open(item["path"],"rb").read()
 if len(data)!=item["bytes"] or hashlib.sha256(data).hexdigest()!=item["sha256"]:raise SystemExit("FAIL renderdoc provenance rehash")
result=manifest[-1]
if set(result)!={"kind","rc","timeout","forced"} or result.get("kind")!="result" or result.get("rc")!=0 or result.get("timeout") is not False or result.get("forced") is not False:
 raise SystemExit("FAIL renderdoc result")

# The capture readout: what qrenderdoc's replay controller actually resolved.
try:readout=json.load(open(capture_path,encoding="utf-8",errors="strict"))
except Exception as exc:raise SystemExit(f"FAIL renderdoc readout JSON: {exc}")
if not isinstance(readout,dict):raise SystemExit("FAIL renderdoc readout schema")
if readout.get("ok") is not True:raise SystemExit(f"FAIL renderdoc readout not ok: {readout.get('error')!r}")
if readout.get("api")!="Vulkan":raise SystemExit(f"FAIL renderdoc api {readout.get('api')!r}")
scopes=readout.get("scopes")
if not isinstance(scopes,list):raise SystemExit("FAIL renderdoc scopes schema")
for scope in scopes:
 if not isinstance(scope,dict) or set(scope)!={"name","eventId","depth","children","actions"}:raise SystemExit("FAIL renderdoc scope schema")
 if not isinstance(scope.get("name"),str) or not scope["name"]:raise SystemExit("FAIL renderdoc scope name")
 for key in ("eventId","depth","children","actions"):
  if not isinstance(scope.get(key),int) or scope[key]<0:raise SystemExit(f"FAIL renderdoc scope {key}")

# 1. The five semantic names are present, exactly once each, in recorded order.
named=[s for s in scopes if s["name"] in expected]
seen=[s["name"] for s in named]
if seen!=expected:raise SystemExit(f"FAIL renderdoc scope order/cardinality: {seen}")

# 2. They are real markers in the tool's tree, not flat siblings smuggled in as
#    plain action names: a resolved debug scope carries nested events.
for scope in named:
 if scope["children"]<=0:raise SystemExit(f"FAIL renderdoc scope not nested: {scope['name']}")

# 3. Event ids strictly increase — the tool placed them along the frame
#    timeline, which is what makes the labels useful for attributing GPU time.
ids=[s["eventId"] for s in named]
if ids!=sorted(ids) or len(set(ids))!=len(ids):raise SystemExit(f"FAIL renderdoc eventId order: {ids}")

# 4. The frame actually drew inside the labelled scopes. A capture where every
#    scope is empty would satisfy naming while proving nothing about coverage.
if sum(s["actions"] for s in named)<=0:raise SystemExit("FAIL renderdoc no actions inside scopes")

# 5. No unbalanced/dangling scope survived into the capture.
if readout.get("unbalanced") not in (0,False):raise SystemExit("FAIL renderdoc unbalanced scope")

# 6. The engine-side tally and the tool-side readout must agree. This is the
#    join that makes the two gates one claim instead of two opinions.
if readout.get("labelSource")!="vulkan-debug-utils":raise SystemExit("FAIL renderdoc label source")
print(f"PASS independent RenderDoc receipt: {len(named)} semantic scopes, {sum(s['actions'] for s in named)} actions")
PYEOF
}

# Offline fixture writer: builds a clean readout+manifest, then injects one
# named defect. Every branch here must be rejected by analyze_contract.
write_self() {
python3 - "$1" "$2" "$3" <<'PYEOF'
import hashlib,json,os,sys
capture_path,manifest_path,mode=sys.argv[1:]
names=["wired.main","wired.tonemap","wired.ui","wired.scene-depth-resume","wired.present"]
scopes=[{"name":n,"eventId":10+i*10,"depth":1,"children":4,"actions":3} for i,n in enumerate(names)]
readout={"ok":True,"api":"Vulkan","labelSource":"vulkan-debug-utils","unbalanced":0,"scopes":scopes}
if mode=="missing":readout["scopes"]=[s for s in scopes if s["name"]!="wired.ui"]
elif mode=="duplicate":readout["scopes"]=scopes+[dict(scopes[0])]
elif mode=="renamed":readout["scopes"][2]["name"]="wired.hud"
elif mode=="order":readout["scopes"][1],readout["scopes"][3]=readout["scopes"][3],readout["scopes"][1]
elif mode=="name-order":
 # Swap only the NAMES, leaving eventIds ascending in place. Without this the
 # eventId check masks a broken name-order check: proven by disabling the
 # order assertion and watching the suite still pass on `order` alone.
 readout["scopes"][1]["name"],readout["scopes"][3]["name"]=readout["scopes"][3]["name"],readout["scopes"][1]["name"]
elif mode=="flat":readout["scopes"][0]["children"]=0
elif mode=="empty-actions":
 for s in readout["scopes"]:s["actions"]=0
elif mode=="eventid-order":readout["scopes"][4]["eventId"]=1
elif mode=="eventid-dup":readout["scopes"][4]["eventId"]=readout["scopes"][3]["eventId"]
elif mode=="unbalanced":readout["unbalanced"]=1
elif mode=="not-ok":readout["ok"]=False;readout["error"]="synthetic"
elif mode=="wrong-api":readout["api"]="D3D12"
elif mode=="label-source":readout["labelSource"]="nvtx"
elif mode=="scope-schema":readout["scopes"][0].pop("actions")
elif mode=="scope-name-type":readout["scopes"][0]["name"]=""
elif mode=="negative-children":readout["scopes"][0]["children"]=-1
with open(capture_path,"w") as out:json.dump(readout,out)
roles=("wired","renderer","capture","harness")
M=[{"kind":"scenario","schema":1,"name":"ral-profile-renderdoc","map":"arena7","backend":"vulkan","tool":"renderdoc","expected_names":list(names)}]
for role in roles:
 path=manifest_path+"."+role;data=("fixture-"+role).encode();open(path,"wb").write(data)
 M.append({"kind":"provenance","role":role,"path":os.path.abspath(path),"bytes":len(data),"sha256":hashlib.sha256(data).hexdigest()})
M.append({"kind":"result","rc":0,"timeout":False,"forced":False})
if mode=="manifest":M[2]["sha256"]="0"*64
elif mode=="scenario-name":M[0]["expected_names"].remove("wired.present")
elif mode=="scenario-map":M[0]["map"]="arena99"
elif mode=="scenario-tool":M[0]["tool"]="optick"
elif mode=="scenario-backend":M[0]["backend"]="d3d12"
elif mode=="provenance-role":M[1]["role"]="gui"
elif mode=="result":M[-1]["rc"]=1
elif mode=="result-timeout":M[-1]["timeout"]=True
with open(manifest_path,"w") as out:
 for item in M:out.write(json.dumps(item,sort_keys=True)+"\n")
PYEOF
}

if [ "${1:-}" = --analyze ];then
 [ "$#" -eq 3 ] || { echo "usage: $0 --analyze <readout.json> <manifest.jsonl>";exit 64; }
 analyze_contract "$2" "$3";exit $?
fi

if [ "${1:-}" = --self-test ];then
 ROOT="$(mktemp -d -t ral-rdoc-self-XXXXXX 2>/dev/null || mktemp -d)";trap 'rm -rf "$ROOT"' EXIT
 write_self "$ROOT/clean.json" "$ROOT/clean.manifest" clean || exit 1
 analyze_contract "$ROOT/clean.json" "$ROOT/clean.manifest" >/dev/null || { echo "FAIL clean fixture rejected";exit 1; }
 defects=(missing duplicate renamed order name-order flat empty-actions eventid-order eventid-dup unbalanced not-ok wrong-api label-source scope-schema scope-name-type negative-children manifest scenario-name scenario-map scenario-tool scenario-backend provenance-role result result-timeout)
 for defect in "${defects[@]}";do
  write_self "$ROOT/$defect.json" "$ROOT/$defect.manifest" "$defect" || exit 1
  if analyze_contract "$ROOT/$defect.json" "$ROOT/$defect.manifest" >/dev/null 2>&1;then echo "FAIL accepted $defect";exit 1;fi
 done
 echo "PASS ral-profile-renderdoc analyzer self-test (${#defects[@]} mutations)";exit 0
fi

# ── real capture path ────────────────────────────────────────────────────
# Everything below needs a RenderDoc install; without one we SKIP, because a
# missing external tool is not a defect in the engine.
WIRED="${1:-}";[ -n "$WIRED" ] && [ -x "$WIRED" ] || { echo "usage: $0 /absolute/path/to/wired  (or --self-test)";exit 64; }
[ -z "${WIRED_MAP:-}" ] || [[ "$WIRED_MAP" =~ ^arena(1|7)$ ]] || { echo "FAIL: WIRED_MAP must be arena1 or arena7";exit 64; }
MAP="${WIRED_MAP:-arena7}"
[ -n "$VR_DIR" ] || { echo "SKIP: tools/visual_regression unavailable";exit 77; }

find_tool(){ local name="$1" explicit="${2:-}" candidate
 [ -n "$explicit" ] && [ -x "$explicit" ] && { printf '%s\n' "$explicit";return 0; }
 command -v "$name" >/dev/null 2>&1 && { command -v "$name";return 0; }
 for candidate in "/c/Program Files/RenderDoc/$name.exe" "C:/Program Files/RenderDoc/$name.exe" "/usr/bin/$name" "/usr/local/bin/$name";do
  [ -x "$candidate" ] && { printf '%s\n' "$candidate";return 0; }
 done
 return 1;}

RENDERDOCCMD="$(find_tool renderdoccmd "${RENDERDOCCMD:-}")" || { echo "SKIP: renderdoccmd not found (RenderDoc supports Windows/Linux, not macOS); analyzer contract still covered by --self-test";exit 77; }
QRENDERDOC="$(find_tool qrenderdoc "${QRENDERDOC:-}")" || { echo "SKIP: qrenderdoc not found; analyzer contract still covered by --self-test";exit 77; }

echo "renderdoccmd: $RENDERDOCCMD"
echo "qrenderdoc:   $QRENDERDOC"
echo "SKIP: live capture orchestration is owned by tools/visual_regression/capture_driver.py on the RenderDoc host; run it there and feed the readout back through --analyze"
exit 77
