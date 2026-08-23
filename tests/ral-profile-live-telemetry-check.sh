#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Wired Engine contributors

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SOURCE_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
TIMEOUT_RUNNER="$SCRIPT_DIR/run-with-timeout.py"

analyze_contract() {
python3 - "$1" "$2" "$3" <<'PYEOF'
import hashlib,json,os,re,sys
host_path,product_path,manifest_path=sys.argv[1:]
host=[x.rstrip("\n") for x in open(host_path,encoding="utf-8",errors="strict") if x.strip()]
product=[x.rstrip("\n") for x in open(product_path,encoding="utf-8",errors="strict") if x.strip()]
if not host or not product:raise SystemExit("FAIL live-profile empty runtime log")
if any("VUID-" in x or re.match(r"^RAL_PROFILE_HOST_LOG severity=(?:17|21)(?: |$)",x) for x in host):
 raise SystemExit("FAIL live-profile host diagnostic")
if any("[FATAL]" in x or "[ERROR]" in x or "Sys_Error:" in x for x in product):
 raise SystemExit("FAIL live-profile product diagnostic")
listener=[(i,x) for i,x in enumerate(host) if x.startswith("RAL_PROFILE_HOST telemetry=listening ")]
if len(listener)!=1 or re.fullmatch(r"RAL_PROFILE_HOST telemetry=listening address=127\.0\.0\.1 port=[1-9][0-9]*",listener[0][1]) is None:
 raise SystemExit("FAIL live-profile listener")
accepted=[]
pat=re.compile(r"RAL_PROFILE_HOST telemetry=accepted session-changes=([1-9][0-9]*) generation=([1-9][0-9]*) sequence=([1-9][0-9]*) topology=([1-9][0-9]*) lanes=([1-9][0-9]*) history=([1-9][0-9]*) labels=([A-Za-z0-9_.>-]+)")
for i,x in enumerate(host):
 m=pat.fullmatch(x)
 if m:accepted.append((i,)+tuple(map(int,m.groups()[:6]))+(m.group(7),))
if len(accepted)<6:raise SystemExit(f"FAIL live-profile accepted sample count {len(accepted)}")
sessions=sorted({x[1] for x in accepted})
if sessions!=[1,2]:raise SystemExit(f"FAIL live-profile producer sessions {sessions}")
for session in sessions:
 rows=[x for x in accepted if x[1]==session]
 seq=[x[3] for x in rows]
 if any(a>=b for a,b in zip(seq,seq[1:])):raise SystemExit("FAIL live-profile sequence monotonicity")
 if rows[0][6]>3:raise SystemExit("FAIL live-profile generation history did not reset")
 for row in rows:
  labels=row[7].split(">")
  if row[5]!=len(labels) or len(set(labels))!=len(labels) or row[7]!="dlight_shadow_start>dlight_shadow>present_prep":
   raise SystemExit(f"FAIL live-profile semantic labels {row[7]}")
presented=[]
ppat=re.compile(r"RAL_PROFILE_HOST telemetry=presented session-changes=([1-9][0-9]*) producer-generation=([1-9][0-9]*) sequence=([1-9][0-9]*) host-generation=([1-9][0-9]*) lists=([1-9][0-9]*) vertices=([1-9][0-9]*) indices=([1-9][0-9]*) draws=([1-9][0-9]*) present=success")
for i,x in enumerate(host):
 m=ppat.fullmatch(x)
 if m:presented.append((i,)+tuple(map(int,m.groups())))
if len(presented)!=len(accepted):raise SystemExit("FAIL live-profile accepted/presented cardinality")
for a,p in zip(accepted,presented):
 if not (a[0]<p[0]) or (a[1],a[2],a[3])!=(p[1],p[2],p[3]):raise SystemExit("FAIL live-profile sample/present binding")
 if p[7]<p[8]*3:raise SystemExit("FAIL live-profile indexed draw relationship")
inactive=[(i,x) for i,x in enumerate(host) if x.startswith("RAL_PROFILE_HOST telemetry=inactive ")]
if len(inactive)!=2 or any(re.fullmatch(r"RAL_PROFILE_HOST telemetry=inactive status=4 stale-transitions=0",x[1]) is None for x in inactive):
 raise SystemExit("FAIL live-profile terminal receipt")
complete=[(i,x) for i,x in enumerate(host) if x.startswith("RAL_PROFILE_HOST action=complete ")]
session1_last=max(x[0] for x in presented if x[1]==1);session2_first=min(x[0] for x in accepted if x[1]==2)
if len(complete)!=1 or not (session1_last<inactive[0][0]<session2_first and presented[-1][0]<inactive[1][0]<complete[0][0]):raise SystemExit("FAIL live-profile terminal order")
ready=[x for x in product if "profileTelemetry: action=sender-ready destination=127.0.0.1:" in x]
first=[x for x in product if "profileTelemetry: action=first-sample generation=" in x]
if len(ready)!=1 or len(first)!=2:raise SystemExit("FAIL live-profile sender/restart receipts")
if not any("----- Client Shutdown (Client quit) -----" in x for x in product):raise SystemExit("FAIL live-profile product shutdown")
manifest=[json.loads(x) for x in open(manifest_path,encoding="utf-8",errors="strict") if x.strip()]
scenario={"backend":"vulkan","kind":"scenario","name":"ral-profile-live-telemetry","renderer_restart":True,"schema":1,"transport":"udp-loopback-latest-value"}
if not manifest or manifest[0]!=scenario:raise SystemExit("FAIL live-profile scenario")
roles=["tool","gui","renderer","sdl3","moltenvk","pax21","base","harness","bootstrap-cfg"]
if [x.get("role") for x in manifest[1:-1]]!=roles:raise SystemExit("FAIL live-profile provenance roles")
for item in manifest[1:-1]:
 if set(item)!={"kind","role","path","bytes","sha256"} or item.get("kind")!="provenance":raise SystemExit("FAIL live-profile provenance schema")
 data=open(item["path"],"rb").read()
 if len(data)!=item["bytes"] or hashlib.sha256(data).hexdigest()!=item["sha256"]:raise SystemExit("FAIL live-profile provenance rehash")
result=manifest[-1]
if set(result)!={"kind","controller_pid","host_rc","product_rc","timeout","forced"} or result.get("kind")!="result" or result.get("host_rc")!=0 or result.get("product_rc")!=0 or result.get("timeout") is not False or result.get("forced") is not False:
 raise SystemExit("FAIL live-profile result")
print("PASS live renderer profile telemetry -> standalone RAL/ImGui tool across renderer restart")
PYEOF
}

if [ "${1:-}" = --analyze ]; then analyze_contract "$2" "$3" "$4"; exit $?; fi
if [ "${1:-}" = --self-test ]; then
	ROOT="$(mktemp -d -t ral-profile-live-self-XXXXXX 2>/dev/null || mktemp -d)"; trap 'rm -rf "$ROOT"' EXIT
	python3 - "$ROOT" <<'PYEOF'
import hashlib,json,os,sys
r=sys.argv[1];host=[
"RAL_PROFILE_HOST telemetry=listening address=127.0.0.1 port=45000",
"RAL_PROFILE_HOST telemetry=accepted session-changes=1 generation=1 sequence=2 topology=1 lanes=3 history=2 labels=dlight_shadow_start>dlight_shadow>present_prep",
"RAL_PROFILE_HOST telemetry=presented session-changes=1 producer-generation=1 sequence=2 host-generation=2 lists=2 vertices=120 indices=180 draws=4 present=success",
"RAL_PROFILE_HOST telemetry=accepted session-changes=1 generation=1 sequence=3 topology=1 lanes=3 history=3 labels=dlight_shadow_start>dlight_shadow>present_prep",
"RAL_PROFILE_HOST telemetry=presented session-changes=1 producer-generation=1 sequence=3 host-generation=2 lists=2 vertices=120 indices=180 draws=4 present=success",
"RAL_PROFILE_HOST telemetry=accepted session-changes=1 generation=1 sequence=4 topology=1 lanes=3 history=4 labels=dlight_shadow_start>dlight_shadow>present_prep",
"RAL_PROFILE_HOST telemetry=presented session-changes=1 producer-generation=1 sequence=4 host-generation=2 lists=2 vertices=120 indices=180 draws=4 present=success",
"RAL_PROFILE_HOST telemetry=inactive status=4 stale-transitions=0",
"RAL_PROFILE_HOST telemetry=accepted session-changes=2 generation=1 sequence=2 topology=1 lanes=3 history=2 labels=dlight_shadow_start>dlight_shadow>present_prep",
"RAL_PROFILE_HOST telemetry=presented session-changes=2 producer-generation=1 sequence=2 host-generation=2 lists=2 vertices=120 indices=180 draws=4 present=success",
"RAL_PROFILE_HOST telemetry=accepted session-changes=2 generation=1 sequence=3 topology=1 lanes=3 history=3 labels=dlight_shadow_start>dlight_shadow>present_prep",
"RAL_PROFILE_HOST telemetry=presented session-changes=2 producer-generation=1 sequence=3 host-generation=2 lists=2 vertices=120 indices=180 draws=4 present=success",
"RAL_PROFILE_HOST telemetry=accepted session-changes=2 generation=1 sequence=4 topology=1 lanes=3 history=4 labels=dlight_shadow_start>dlight_shadow>present_prep",
"RAL_PROFILE_HOST telemetry=presented session-changes=2 producer-generation=1 sequence=4 host-generation=2 lists=2 vertices=120 indices=180 draws=4 present=success",
"RAL_PROFILE_HOST telemetry=inactive status=4 stale-transitions=0","RAL_PROFILE_HOST action=complete acquire=50 submit=50 present=50"]
product=["[INFO ] profileTelemetry: action=sender-ready destination=127.0.0.1:45000","[INFO ] profileTelemetry: action=first-sample generation=1 sequence=1 lanes=3","[INFO ] profileTelemetry: action=first-sample generation=1 sequence=1 lanes=3","[INFO ] ----- Client Shutdown (Client quit) -----"]
open(r+"/host","w").write("\n".join(host)+"\n");open(r+"/product","w").write("\n".join(product)+"\n")
files=[]
for i,role in enumerate(("tool","gui","renderer","sdl3","moltenvk","pax21","base","harness","bootstrap-cfg")):
 p=f"{r}/{role}";open(p,"wb").write((role+"\n").encode());d=open(p,"rb").read();files.append({"kind":"provenance","role":role,"path":p,"bytes":len(d),"sha256":hashlib.sha256(d).hexdigest()})
m=[{"backend":"vulkan","kind":"scenario","name":"ral-profile-live-telemetry","renderer_restart":True,"schema":1,"transport":"udp-loopback-latest-value"}]+files+[{"kind":"result","controller_pid":1,"host_rc":0,"product_rc":0,"timeout":False,"forced":False}]
open(r+"/manifest","w").write("\n".join(json.dumps(x,sort_keys=True) for x in m)+"\n")
PYEOF
	analyze_contract "$ROOT/host" "$ROOT/product" "$ROOT/manifest" >/dev/null || exit 1
	defects=(session terminal labels presented draw restart diagnostic result)
	for defect in "${defects[@]}"; do
		cp "$ROOT/host" "$ROOT/host.$defect"; cp "$ROOT/product" "$ROOT/product.$defect"; cp "$ROOT/manifest" "$ROOT/manifest.$defect"
		case "$defect" in
			session) sed -i.bak 's/session-changes=2/session-changes=1/g' "$ROOT/host.$defect";;
			terminal) sed -i.bak '/telemetry=inactive/d' "$ROOT/host.$defect";;
			labels) sed -i.bak 's/dlight_shadow_start>dlight_shadow>present_prep/final>scene>color/g' "$ROOT/host.$defect";;
			presented) sed -i.bak '/sequence=3 host-generation=2/d' "$ROOT/host.$defect";;
			draw) sed -i.bak 's/indices=180 draws=4/indices=9 draws=4/' "$ROOT/host.$defect";;
			restart) sed -i.bak '/action=first-sample/d' "$ROOT/product.$defect";;
			diagnostic) printf '%s\n' 'RAL_PROFILE_HOST_LOG severity=17 failure' >>"$ROOT/host.$defect";;
			result) sed -i.bak 's/"host_rc": 0/"host_rc": 1/' "$ROOT/manifest.$defect";;
		esac
		if analyze_contract "$ROOT/host.$defect" "$ROOT/product.$defect" "$ROOT/manifest.$defect" >/dev/null 2>&1; then echo "FAIL accepted $defect"; exit 1; fi
	done
	echo "PASS ral-profile-live-telemetry analyzer self-test (${#defects[@]} mutations)"; exit 0
fi

TOOL="${1:-}"; WIRED="${2:-}"
[ -x "$TOOL" ] && [ -x "$WIRED" ] || { echo "usage: $0 /absolute/wired_profile_host /absolute/wired"; exit 64; }
[ "$(uname -s)" = Darwin ] || { echo "SKIP live profile telemetry runtime currently requires macOS/MoltenVK"; exit 77; }
[ -f "$TIMEOUT_RUNNER" ] || { echo "FAIL missing timeout runner"; exit 1; }
TOOL="$(cd "$(dirname "$TOOL")" && pwd)/$(basename "$TOOL")"; WIRED="$(cd "$(dirname "$WIRED")" && pwd)/$(basename "$WIRED")"
WD="$(dirname "$WIRED")"; RENDERER=""
for p in "$WD/wired_vulkan_arm64.dylib" "$WD/Contents/MacOS/wired_vulkan_arm64.dylib"; do [ -f "$p" ] && { RENDERER="$p"; break; }; done
SDL3_PATH="$(otool -L "$TOOL" | awk '/libSDL3.*[.]dylib/{print $1; exit}')"; MOLTEN=""
for p in "$WD/libMoltenVK.dylib" "$WD/Contents/MacOS/libMoltenVK.dylib" /opt/homebrew/opt/molten-vk/lib/libMoltenVK.dylib; do [ -f "$p" ] && { MOLTEN="$p"; break; }; done
CONTENT="${WIRED_CONTENT_ROOT:-}"
if [ -z "$CONTENT" ]; then for p in "$WD" "$WD/../Resources" "$SOURCE_ROOT/build/dmg-staging/q3now-preview.app/Contents/Resources"; do [ -f "$p/base/pax21.sw3z" ] && { CONTENT="$p"; break; }; done; fi
[ -f "$RENDERER" ] && [ -f "$SDL3_PATH" ] && [ -f "$MOLTEN" ] && [ -f "$CONTENT/base/pax21.sw3z" ] || { echo "SKIP missing renderer/SDL3/MoltenVK/content"; exit 77; }
if [ -f "$CONTENT/base/pax01.sw3z" ]; then BASE="$CONTENT/base/pax01.sw3z"; else BASE="$CONTENT/base/pak0.pk3"; fi
[ -f "$BASE" ] || { echo "SKIP missing base content"; exit 77; }
ROOT="$(mktemp -d -t ral-profile-live-XXXXXX 2>/dev/null || mktemp -d)"; FORCED=false
cleanup(){ local s=$?; trap - EXIT INT TERM; [ "${WIRED_KEEP_ARTIFACTS:-0}" = 1 ] || rm -rf "$ROOT"; exit "$s"; }; trap cleanup EXIT; trap 'exit 130' INT; trap 'exit 143' TERM
mkdir -p "$ROOT/runtime" "$ROOT/install/Contents/Resources/base" "$ROOT/install/Contents/MacOS" "$ROOT/home/base"
cp "$TOOL" "$ROOT/runtime/wired_profile_host"; cp "$WIRED" "$ROOT/runtime/wired"; chmod +x "$ROOT/runtime/wired_profile_host" "$ROOT/runtime/wired"
cp "$RENDERER" "$ROOT/install/Contents/MacOS/"; cp "$MOLTEN" "$ROOT/install/Contents/MacOS/libMoltenVK.dylib"; cp "$CONTENT/base/pax21.sw3z" "$ROOT/install/Contents/Resources/base/"; cp "$BASE" "$ROOT/install/Contents/Resources/base/"
cp "$0" "$ROOT/harness.sh"
BOOT="$ROOT/home/base/profile-live.cfg"
cat >"$BOOT" <<'EOF'
r_gpuSpeeds 1
wait 120
vid_restart
wait 180
quit
EOF
PORT="$(python3 - <<'PYEOF'
import socket
s=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);s.bind(('127.0.0.1',0));print(s.getsockname()[1]);s.close()
PYEOF
)"; TOKEN="$(python3 - <<'PYEOF'
import secrets;print(secrets.token_hex(16))
PYEOF
)"
HOST_OUT="$ROOT/host.stdout"; PRODUCT_OUT="$ROOT/product.stdout"; MANIFEST="$ROOT/manifest.jsonl"
python3 - "$MANIFEST" "$ROOT/runtime/wired_profile_host" "$ROOT/runtime/wired" "$ROOT/install/Contents/MacOS/$(basename "$RENDERER")" "$SDL3_PATH" "$ROOT/install/Contents/MacOS/libMoltenVK.dylib" "$ROOT/install/Contents/Resources/base/pax21.sw3z" "$ROOT/install/Contents/Resources/base/$(basename "$BASE")" "$ROOT/harness.sh" "$BOOT" <<'PYEOF'
import hashlib,json,os,sys
roles=("tool","gui","renderer","sdl3","moltenvk","pax21","base","harness","bootstrap-cfg")
with open(sys.argv[1],"w") as out:
 out.write(json.dumps({"backend":"vulkan","kind":"scenario","name":"ral-profile-live-telemetry","renderer_restart":True,"schema":1,"transport":"udp-loopback-latest-value"},sort_keys=True)+"\n")
 for role,path in zip(roles,sys.argv[2:]):
  data=open(path,"rb").read();out.write(json.dumps({"kind":"provenance","role":role,"path":os.path.abspath(path),"bytes":len(data),"sha256":hashlib.sha256(data).hexdigest()},sort_keys=True)+"\n")
PYEOF
python3 "$TIMEOUT_RUNNER" --timeout 40 --kill-after 5 --cwd "$ROOT/runtime" --stdout "$HOST_OUT" -- "$ROOT/runtime/wired_profile_host" --profile-listen "$PORT" --profile-token "$TOKEN" --frames 6 --profile-generations 2 --profile-wait-terminal & HOST_CTL=$!
for _ in {1..100}; do grep -q 'telemetry=listening' "$HOST_OUT" 2>/dev/null && break; kill -0 "$HOST_CTL" 2>/dev/null || break; sleep 0.05; done
python3 "$TIMEOUT_RUNNER" --timeout 35 --kill-after 5 --cwd "$ROOT/runtime" --stdout "$PRODUCT_OUT" -- "$ROOT/runtime/wired" +set fs_basepath "$ROOT/install" +set fs_installpath "$ROOT/install" +set fs_homepath "$ROOT/home" +set fs_game base +set com_automated 1 +set com_noHardReboot 1 +set s_initsound 0 +set r_fullscreen 0 +set r_mode -1 +set r_customwidth 1280 +set r_customheight 720 +set cl_profileTelemetryPort "$PORT" +set cl_profileTelemetryToken "$TOKEN" +set r_gpuSpeeds 1 +set r_smaa 0 +exec profile-live.cfg
PRODUCT_RC=$?; wait "$HOST_CTL"; HOST_RC=$?; TIMEOUT=false; { [ "$PRODUCT_RC" -eq 124 ] || [ "$HOST_RC" -eq 124 ]; } && TIMEOUT=true
python3 - "$MANIFEST" "$$" "$HOST_RC" "$PRODUCT_RC" "$TIMEOUT" <<'PYEOF'
import json,sys
with open(sys.argv[1],"a") as out:out.write(json.dumps({"kind":"result","controller_pid":int(sys.argv[2]),"host_rc":int(sys.argv[3]),"product_rc":int(sys.argv[4]),"timeout":sys.argv[5]=="true","forced":False},sort_keys=True)+"\n")
PYEOF
if ! analyze_contract "$HOST_OUT" "$PRODUCT_OUT" "$MANIFEST"; then echo "FAIL retained root: $ROOT"; WIRED_KEEP_ARTIFACTS=1; exit 1; fi
echo "PASS retained root: $ROOT"
