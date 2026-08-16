#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors
# SPDX-License-Identifier: GPL-3.0-or-later
# Paired GUI/headless acceptance for public VMI_BYTECODE (.wasm-only) policy.

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
TIMEOUT_RUNNER="$SCRIPT_DIR/run-with-timeout.py"

analyze_contract() {
python3 - "$1" "$2" "$3" <<'PYEOF'
import hashlib,json,os,re,sys
client_path,server_path,manifest_path=sys.argv[1:]
def read(path,label):
 rows=[]
 for number,line in enumerate(open(path,encoding="utf-8",errors="strict"),1):
  if not line.strip(): continue
  try: row=json.loads(line)
  except Exception as exc: raise SystemExit(f"FAIL {label} JSON {number}: {exc}")
  if not isinstance(row,dict) or not all(isinstance(row.get(k),str) for k in ("sev","cat","msg")):
   raise SystemExit(f"FAIL {label} schema {number}")
  rows.append(row)
 if not rows: raise SystemExit(f"FAIL empty {label} evidence")
 return rows
client,server=read(client_path,"client"),read(server_path,"server")
def norm(value): return value[:-1] if value.endswith("\n") and not value.endswith("\n\n") else value
def check(rows,label):
 vals=[norm(r["msg"]) for r in rows]
 claimed=("vm/gamesv.","vm/gamecl.","VM_Create policy module=gamesv ",
  "VM_Create policy module=gamecl ","WASM: failed to load vm/gamesv",
  "WASM: failed to load vm/gamecl","QUIC client: TLV ACCEPT received",
  "cls.state: -> CA_ACTIVE (FIRST GAMEPLAY FRAME","Server: ","ClientConnect: ","ClientBegin: ")
 for value in vals:
  parts=re.split(r"[\r\n]",value)
  if len(parts)>1 and any(part.startswith(claimed) for part in parts):
   raise SystemExit(f"FAIL {label} claimed logical-line smuggling")
 if any(r["sev"].upper() in ("ERROR","FATAL") for r in rows): raise SystemExit(f"FAIL {label} severity")
 return vals
cv,sv=check(client,"client"),check(server,"server")
def exact(rows,vals,prefix,pattern,sev,cat,label):
 found=[(i,r,v) for i,(r,v) in enumerate(zip(rows,vals)) if v.startswith(prefix)]
 if len(found)!=1: raise SystemExit(f"FAIL {label} cardinality {len(found)}")
 i,row,value=found[0]
 if row["sev"].upper()!=sev or row["cat"].lower()!=cat or re.fullmatch(pattern,value) is None:
  raise SystemExit(f"FAIL {label} body/metadata: {value}")
 return i
gs_load=exact(server,sv,"vm/gamesv.",r"vm/gamesv\.wasm loaded as WASM interpreter \(\d+ KB memory, \d+ ms\)","INFO","system","gamesv load")
gs_policy=exact(server,sv,"VM_Create policy module=gamesv ",r"VM_Create policy module=gamesv requested=1 effective=1 backend=wasm-interpreter","DEBUG","system","gamesv policy")
cg_load=exact(client,cv,"vm/gamecl.",r"vm/gamecl\.wasm loaded as WASM interpreter \(\d+ KB memory, \d+ ms\)","INFO","system","gamecl load")
cg_policy=exact(client,cv,"VM_Create policy module=gamecl ",r"VM_Create policy module=gamecl requested=1 effective=1 backend=wasm-interpreter","DEBUG","system","gamecl policy")
server_map=exact(server,sv,"Server: ",r"Server: arena7","INFO","server","server map")
init=exact(server,sv,"InitGame:",r"InitGame: .*\\mapname\\arena7(?:\\.*)?","INFO","game","InitGame")
listen=exact(server,sv,"WiredNet: listening on port ",r"WiredNet: listening on port [1-9][0-9]* \(IPv4\), ALPN: .+","INFO","network","listener")
connect=exact(server,sv,"ClientConnect: ",r"ClientConnect: [0-9]+","INFO","game","ClientConnect")
begin=exact(server,sv,"ClientBegin: ",r"ClientBegin: [0-9]+","INFO","game","ClientBegin")
accept=exact(client,cv,"QUIC client: TLV ACCEPT received",r"QUIC client: TLV ACCEPT received","DEBUG","network","ACCEPT")
first=exact(client,cv,"cls.state: -> CA_ACTIVE (FIRST GAMEPLAY FRAME",r"cls\.state: -> CA_ACTIVE \(FIRST GAMEPLAY FRAME mapname=maps/arena7\.bsp serverTime=[0-9]+ numEntities=[0-9]+ framecount=[0-9]+\)","INFO","client","FIRST")
if not server_map<gs_load<gs_policy<init: raise SystemExit("FAIL gamesv causal order")
if not listen<connect<begin: raise SystemExit("FAIL server connection order")
if not cg_load<cg_policy<first or not accept<first: raise SystemExit("FAIL gamecl/FIRST causal order")
for label,vals in (("client",cv),("server",sv)):
 for value in vals:
  lower=value.lower()
  if "gamesv.aot" in lower or "gamecl.aot" in lower or "wasm aot" in lower or "backend=wasm-aot" in lower:
   raise SystemExit(f"FAIL {label} AOT path observed: {value}")
  if value.startswith("VM_Restart:") or "WASM module cannot restart" in value or "Server crashed" in value:
   raise SystemExit(f"FAIL {label} restart/crash marker")
manifest=[]
for number,line in enumerate(open(manifest_path,encoding="utf-8",errors="strict"),1):
 try:item=json.loads(line)
 except Exception as exc:raise SystemExit(f"FAIL manifest JSON {number}: {exc}")
 if not isinstance(item,dict):raise SystemExit("FAIL manifest schema")
 manifest.append(item)
if not manifest or manifest[0]!={"kind":"scenario","schema":1,"name":"vmi-bytecode-gui","map":"arena7","vm_game":1,"vm_cgame":1}:raise SystemExit("FAIL scenario")
roles=[item.get("role") for item in manifest[1:-1]]
if roles != ["gui","headless","pax21","base","gamesv-aot-sentinel","gamecl-aot-sentinel","harness"]:raise SystemExit("FAIL provenance roles")
for item in manifest[1:-1]:
 if set(item)!={"kind","role","path","bytes","sha256"} or item.get("kind")!="provenance" or not isinstance(item.get("bytes"),int) or item["bytes"]<=0 or re.fullmatch(r"[0-9a-f]{64}",str(item.get("sha256",""))) is None:raise SystemExit("FAIL provenance schema")
 try:data=open(item["path"],"rb").read()
 except OSError as exc:raise SystemExit(f"FAIL provenance inaccessible: {exc}")
 if len(data)!=item["bytes"] or hashlib.sha256(data).hexdigest()!=item["sha256"]:raise SystemExit("FAIL provenance rehash")
result=manifest[-1]
if set(result)!={"kind","client_controller_pid","server_pid","client_rc","server_rc","timeout","forced"} or result.get("kind")!="result" or not all(isinstance(result.get(k),int) and result[k]>0 for k in ("client_controller_pid","server_pid")) or result["client_controller_pid"]==result["server_pid"] or result.get("client_rc")!=0 or result.get("server_rc")!=0 or result.get("timeout") is not False or result.get("forced") is not False:raise SystemExit("FAIL result")
print("PASS VMI_BYTECODE GUI/headless: gamesv+gamecl wasm interpreter -> arena7 FIRST")
PYEOF
}

write_self() {
python3 - "$1" "$2" "$3" "$4" <<'PYEOF'
import hashlib,json,os,sys
client_path,server_path,manifest_path,mode=sys.argv[1:]
client=[
 {"sev":"DEBUG","cat":"network","msg":"QUIC client: TLV ACCEPT received\n"},
 {"sev":"INFO","cat":"system","msg":"vm/gamecl.wasm loaded as WASM interpreter (4096 KB memory, 2 ms)\n"},
 {"sev":"DEBUG","cat":"system","msg":"VM_Create policy module=gamecl requested=1 effective=1 backend=wasm-interpreter\n"},
 {"sev":"INFO","cat":"client","msg":"cls.state: -> CA_ACTIVE (FIRST GAMEPLAY FRAME mapname=maps/arena7.bsp serverTime=100 numEntities=2 framecount=3)\n"}]
server=[
 {"sev":"INFO","cat":"server","msg":"Server: arena7\n"},
 {"sev":"INFO","cat":"system","msg":"vm/gamesv.wasm loaded as WASM interpreter (4096 KB memory, 3 ms)\n"},
 {"sev":"DEBUG","cat":"system","msg":"VM_Create policy module=gamesv requested=1 effective=1 backend=wasm-interpreter\n"},
 {"sev":"INFO","cat":"game","msg":"InitGame: \\mapname\\arena7\\protocol\\74\n"},
 {"sev":"INFO","cat":"network","msg":"WiredNet: listening on port 27960 (IPv4), ALPN: q3now\n"},
 {"sev":"INFO","cat":"game","msg":"ClientConnect: 0\n"},
 {"sev":"INFO","cat":"game","msg":"ClientBegin: 0\n"}]
def find(rows,text):return next(i for i,r in enumerate(rows) if text in r["msg"])
if mode=="missing-gamecl":client.pop(find(client,"vm/gamecl"))
elif mode=="additive-gamecl":client.insert(2,dict(client[1]))
elif mode=="wrong-gamecl-policy":client[2]["msg"]="VM_Create policy module=gamecl requested=2 effective=2 backend=wasm-aot\n"
elif mode=="wrong-gamesv-policy":server[2]["msg"]="VM_Create policy module=gamesv requested=1 effective=2 backend=wasm-aot\n"
elif mode=="gamesv-aot":server[1]["msg"]="vm/gamesv.aot loaded as WASM AOT (4096 KB memory, 3 ms)\n"
elif mode=="gamecl-aot-attempt":client.insert(1,{"sev":"WARN","cat":"system","msg":"WASM: failed to load vm/gamecl.aot: invalid\n"})
elif mode=="wrong-first-map":client[3]["msg"]="cls.state: -> CA_ACTIVE (FIRST GAMEPLAY FRAME mapname=maps/arena1.bsp serverTime=100 numEntities=2 framecount=3)\n"
elif mode=="first-suffix":client[3]["msg"]="cls.state: -> CA_ACTIVE (FIRST GAMEPLAY FRAME mapname=maps/arena7.bsp serverTime=100 numEntities=2 framecount=3) extra\n"
elif mode=="no-accept":client.pop(0)
elif mode=="bad-metadata":client[1]["cat"]="client"
elif mode=="bad-order":client[1],client[3]=client[3],client[1]
elif mode=="server-order":server[1],server[3]=server[3],server[1]
elif mode=="error":client.append({"sev":"ERROR","cat":"system","msg":"synthetic failure\n"})
elif mode=="smuggle":client.append({"sev":"INFO","cat":"system","msg":"benign\rVM_Create policy module=gamecl requested=1 effective=1 backend=wasm-interpreter"})
for path,rows in ((client_path,client),(server_path,server)):
 with open(path,"w") as out:
  for row in rows:out.write(json.dumps(row)+"\n")
items=[{"kind":"scenario","schema":1,"name":"vmi-bytecode-gui","map":"arena7","vm_game":1,"vm_cgame":1}]
for role in ("gui","headless","pax21","base","gamesv-aot-sentinel","gamecl-aot-sentinel","harness"):
 path=manifest_path+"."+role;data=("fixture-"+role).encode();open(path,"wb").write(data);items.append({"kind":"provenance","role":role,"path":os.path.abspath(path),"bytes":len(data),"sha256":hashlib.sha256(data).hexdigest()})
items.append({"kind":"result","client_controller_pid":101,"server_pid":102,"client_rc":0,"server_rc":0,"timeout":False,"forced":False})
if mode=="manifest":items[2]["sha256"]="0"*64
elif mode=="result":items[-1]["client_rc"]=1
with open(manifest_path,"w") as out:
 for item in items:out.write(json.dumps(item,sort_keys=True)+"\n")
PYEOF
}

if [ "${1:-}" = --analyze ]; then
 [ "$#" -eq 4 ] || { echo "usage: $0 --analyze <client-jsonl> <server-jsonl> <manifest>"; exit 64; }
 analyze_contract "$2" "$3" "$4"; exit $?
fi
if [ "${1:-}" = --self-test ]; then
 ROOT="$(mktemp -d -t vmi-bytecode-gui-self-XXXXXX 2>/dev/null || mktemp -d)"; trap 'rm -rf "$ROOT"' EXIT
 write_self "$ROOT/client" "$ROOT/server" "$ROOT/manifest" clean
 analyze_contract "$ROOT/client" "$ROOT/server" "$ROOT/manifest" >/dev/null || exit 1
 defects=(missing-gamecl additive-gamecl wrong-gamecl-policy wrong-gamesv-policy gamesv-aot gamecl-aot-attempt wrong-first-map first-suffix no-accept bad-metadata bad-order server-order error smuggle manifest result)
 for defect in "${defects[@]}"; do
  write_self "$ROOT/$defect.client" "$ROOT/$defect.server" "$ROOT/$defect.manifest" "$defect"
  if analyze_contract "$ROOT/$defect.client" "$ROOT/$defect.server" "$ROOT/$defect.manifest" >/dev/null 2>&1; then echo "FAIL accepted $defect"; exit 1; fi
 done
 echo "PASS vmi-bytecode-gui analyzer self-test (${#defects[@]} mutations)"; exit 0
fi

. "$(cd "$(dirname "$0")" && pwd)/lib/wired_paths.sh"
WIRED="${1:-${WIRED_BINARY:-}}"; [ -n "$WIRED" ] && [ -x "$WIRED" ] || { echo "usage: $0 /absolute/path/to/wired"; exit 64; }
[ -f "$TIMEOUT_RUNNER" ] || { echo "SKIP: missing timeout runner"; exit 77; }
WIRED="$(cd "$(dirname "$WIRED")" && pwd)/$(basename "$WIRED")"; WD="$(dirname "$WIRED")"
HEADLESS="${WIRED_BINARY_HEADLESS:-}"; if [ -z "$HEADLESS" ]; then
 suffix="$(basename "$WIRED")"; suffix="${suffix#wired}"
 for candidate in "$WD/wired-headless$suffix" "$WD/wired-headless.arm64" "$WD/wired-headless.x86_64" "$WD/../../../wired-headless$suffix"; do [ -x "$candidate" ] && HEADLESS="$candidate" && break; done
fi
[ -n "$HEADLESS" ] && [ -x "$HEADLESS" ] || { echo "SKIP: sibling wired-headless unavailable"; exit 77; }
HEADLESS="$(cd "$(dirname "$HEADLESS")" && pwd)/$(basename "$HEADLESS")"
PACK=""; for candidate in "$WD" "$WD/../Resources" "$WD/../../.."; do [ -f "$candidate/base/pax21.sw3z" ] && PACK="$(cd "$candidate" && pwd)" && break; done
[ -n "$PACK" ] || { echo "SKIP: current pax21 unavailable"; exit 77; }
# Search every root that can legitimately hold the content pack, not just one.
# $WIRED_HOME is where the launcher writes pax01.sw3z on all platforms
# (GAME-DATA.md §4); omitting it made this SKIP on machines that had the
# content, and the SKIP was then misread as "needs another platform".
BASE=""
for _c in "${WIRED_CONTENT_ROOT:-}" "$PACK" "${WIRED_HOME:-}" "${WIRED_INSTALL:-}"; do
    [ -n "$_c" ] || continue
    if   [ -f "$_c/base/pax01.sw3z" ]; then BASE="$_c/base/pax01.sw3z"; break
    elif [ -f "$_c/base/pak0.pk3"   ]; then BASE="$_c/base/pak0.pk3";   break; fi
done
[ -n "$BASE" ] || { echo "SKIP: no content pack found (set WIRED_CONTENT_ROOT)"; exit 77; }
ROOT="$(mktemp -d -t vmi-bytecode-gui-XXXXXX 2>/dev/null || mktemp -d)"; CLIENT_HOME="$ROOT/client/q3now-preview"; SERVER_HOME="$ROOT/server/q3now-preview"; FIFO="$ROOT/server.stdin"; SERVER_PID=""; OPEN=0; FORCED=0
cleanup(){ local status=$?; trap - EXIT INT TERM; [ "$OPEN" -eq 1 ] && exec 9>&- || true; [ -n "$SERVER_PID" ] && kill -TERM "$SERVER_PID" 2>/dev/null || true; [ -n "$SERVER_PID" ] && wait "$SERVER_PID" 2>/dev/null || true; [ "${WIRED_KEEP_ARTIFACTS:-0}" = 1 ] || rm -rf "$ROOT"; exit "$status"; }
trap cleanup EXIT; trap 'exit 130' INT; trap 'exit 143' TERM
mkdir -p "$CLIENT_HOME/base/vm" "$SERVER_HOME/base/vm"
for home in "$CLIENT_HOME" "$SERVER_HOME"; do cp "$PACK/base/pax21.sw3z" "$home/base/" || exit 1; cp "$BASE" "$home/base/" || exit 1; done
printf 'WIRED_INVALID_GAMESV_AOT_MUST_NOT_BE_OPENED\n' >"$ROOT/gamesv.aot" || exit 1
printf 'WIRED_INVALID_GAMECL_AOT_MUST_NOT_BE_OPENED\n' >"$ROOT/gamecl.aot" || exit 1
for home in "$CLIENT_HOME" "$SERVER_HOME"; do cp "$ROOT/gamesv.aot" "$home/base/vm/gamesv.aot" || exit 1; cp "$ROOT/gamecl.aot" "$home/base/vm/gamecl.aot" || exit 1; done
PORT="$(python3 - <<'PYEOF'
import socket
s=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);s.bind(("127.0.0.1",0));print(s.getsockname()[1]);s.close()
PYEOF
)"; ENDPOINT="127.0.0.1:$PORT"
printf 'set activeAction "wait 60 ; quit"\nwait 100\nconnect %s\n' "$ENDPOINT" >"$CLIENT_HOME/base/vmi-bytecode-gui.cfg"
MANIFEST="$ROOT/manifest.jsonl"
python3 - "$MANIFEST" "$WIRED" "$HEADLESS" "$PACK/base/pax21.sw3z" "$BASE" "$ROOT/gamesv.aot" "$ROOT/gamecl.aot" "$0" <<'PYEOF'
import hashlib,json,os,sys
with open(sys.argv[1],"w") as out:
 out.write(json.dumps({"kind":"scenario","schema":1,"name":"vmi-bytecode-gui","map":"arena7","vm_game":1,"vm_cgame":1},sort_keys=True)+"\n")
 for role,path in zip(("gui","headless","pax21","base","gamesv-aot-sentinel","gamecl-aot-sentinel","harness"),sys.argv[2:]):
  data=open(path,"rb").read();out.write(json.dumps({"kind":"provenance","role":role,"path":os.path.abspath(path),"bytes":len(data),"sha256":hashlib.sha256(data).hexdigest()},sort_keys=True)+"\n")
PYEOF
mkfifo "$FIFO"; exec 9<>"$FIFO"; OPEN=1
(cd "$(dirname "$HEADLESS")" && exec "$HEADLESS" +set fs_homepath "$SERVER_HOME" +set com_automated 1 +set com_noHardReboot 1 +set net_ip 127.0.0.1 +set net_port "$PORT" +set vm_game 1 +set g_autoBots 0 +set g_minPlayers 0 +set sv_pure 0 +set log_severity DEBUG +set log_file_severity DEBUG +set log_file_mode overwrite_synced +map arena7 <&9) >"$ROOT/server.stdout" 2>&1 & SERVER_PID=$!; RUN_SERVER_PID=$SERVER_PID
SERVER_JSON="$SERVER_HOME/qconsole.jsonl"
for _ in $(seq 1 600); do python3 - "$SERVER_JSON" "$PORT" <<'PYEOF' >/dev/null 2>&1 && break
import json,re,sys
rows=[json.loads(x) for x in open(sys.argv[1]) if x.strip()]
msgs=[r.get("msg","") for r in rows]
raise SystemExit(0 if any(re.search(rf"WiredNet: listening on port {sys.argv[2]} \(IPv4\)",m) for m in msgs) and any(m.startswith("Server: arena7") for m in msgs) else 1)
PYEOF
 sleep .1; kill -0 "$SERVER_PID" 2>/dev/null || { echo "FAIL server exited"; exit 1; }; done
python3 - "$SERVER_JSON" "$PORT" <<'PYEOF' >/dev/null 2>&1 || { echo "FAIL server readiness"; exit 1; }
import json,re,sys
msgs=[json.loads(x).get("msg","") for x in open(sys.argv[1]) if x.strip()]
raise SystemExit(0 if any(re.search(rf"WiredNet: listening on port {sys.argv[2]} \(IPv4\)",m) for m in msgs) and any(m.startswith("Server: arena7") for m in msgs) else 1)
PYEOF
CLIENT_STDOUT="$ROOT/client.stdout"; CLIENT_JSON="$CLIENT_HOME/qconsole.jsonl"
case "$(uname -s)" in Darwin) PLATFORM=(-ApplePersistenceIgnoreState YES);; *) PLATFORM=();; esac
python3 "$TIMEOUT_RUNNER" --timeout 150 --kill-after 15 --cwd "$WD" --stdout "$CLIENT_STDOUT" -- "$WIRED" "${PLATFORM[@]}" +set fs_homepath "$CLIENT_HOME" +set com_automated 1 +set com_noHardReboot 1 +set s_initsound 0 +set r_fullscreen 0 +set r_mode -1 +set r_customwidth 1280 +set r_customheight 720 +set net_ip 127.0.0.1 +set wn_cert_verify 0 +set vm_cgame 1 +set sv_pure 0 +set log_severity DEBUG +set log_file_severity DEBUG +set log_file_mode overwrite_synced +exec vmi-bytecode-gui.cfg &
CLIENT_CONTROLLER_PID=$!; wait "$CLIENT_CONTROLLER_PID"; CLIENT_RC=$?
printf 'quit\n' >&9
for _ in $(seq 1 150); do kill -0 "$SERVER_PID" 2>/dev/null || break; sleep .1; done
if kill -0 "$SERVER_PID" 2>/dev/null; then FORCED=1; kill -TERM "$SERVER_PID" 2>/dev/null || true; fi
wait "$SERVER_PID"; SERVER_RC=$?; SERVER_PID=""; exec 9>&-; OPEN=0
python3 - "$MANIFEST" "$CLIENT_CONTROLLER_PID" "$RUN_SERVER_PID" "$CLIENT_RC" "$SERVER_RC" "$FORCED" <<'PYEOF'
import json,sys
with open(sys.argv[1],"a") as out:out.write(json.dumps({"kind":"result","client_controller_pid":int(sys.argv[2]),"server_pid":int(sys.argv[3]),"client_rc":int(sys.argv[4]),"server_rc":int(sys.argv[5]),"timeout":False,"forced":bool(int(sys.argv[6]))},sort_keys=True)+"\n")
PYEOF
[ "$CLIENT_RC" -eq 0 ] && [ "$SERVER_RC" -eq 0 ] && [ "$FORCED" -eq 0 ] || { echo "FAIL process result client=$CLIENT_RC server=$SERVER_RC forced=$FORCED"; exit 1; }
analyze_contract "$CLIENT_JSON" "$SERVER_JSON" "$MANIFEST" || exit 1
echo "PASS VMI_BYTECODE GUI acceptance gate"
