#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors
# WASM game-VM recreation and server-owned bot identity across map_restart.

set -uo pipefail

analyze_contract() {
python3 - "$1" "$2" "$3" <<'PYEOF'
import hashlib,json,os,re,sys
path,manifest_path,controller_path=sys.argv[1:];rows=[];physical=[]
for number,line in enumerate(open(path,encoding="utf-8",errors="strict"),1):
 if not line.strip():continue
 try:row=json.loads(line)
 except Exception as exc:raise SystemExit(f"FAIL wasm-restart log {number}: {exc}")
 if not isinstance(row,dict) or not all(isinstance(row.get(k),str) for k in ("sev","cat","msg")):
  raise SystemExit("FAIL wasm-restart log schema")
 rows.append(row);physical.append(number)
if not rows:raise SystemExit("FAIL wasm-restart empty evidence")
def norm(value):return value[:-1] if value.endswith("\n") and not value.endswith("\n\n") else value
vals=[norm(row["msg"]) for row in rows]
claimed=("Server: ","vm/gamesv.","InitGame:","==== ShutdownGame ====","ShutdownGame:",
 "----- Server Shutdown ","ClientConnect: ","ClientBegin: ","broadcast: print ",
 "botkick:","Q0_WASM_RESTART_","VM_Restart:","WASM module cannot restart",
 "[NAV] navmesh ready for ","VM_Create policy module=gamesv ","WASM: failed to load vm/gamesv.aot")
for value in vals:
 lines=re.split(r"[\r\n]",value)
 if len(lines)>1 and any(line.startswith(claimed) for line in lines):
  raise SystemExit("FAIL wasm-restart claimed logical-line smuggling")
def family(prefix):return [(i,rows[i],value) for i,value in enumerate(vals) if value.startswith(prefix)]
def exact(prefix,patterns,sev,cat):
 found=family(prefix)
 if len(found)!=len(patterns):raise SystemExit(f"FAIL wasm-restart {prefix} cardinality {len(found)}")
 for (_,row,value),pattern in zip(found,patterns):
  if row["sev"].upper()!=sev or row["cat"].lower()!=cat or re.fullmatch(pattern,value) is None:
   raise SystemExit(f"FAIL wasm-restart {prefix} body/metadata {value}")
 return found

servers=exact("Server: ",[r"Server: arena7"],"INFO","server")
loads=exact("vm/gamesv.",[r"vm/gamesv\.wasm loaded as WASM interpreter \(\d+ KB memory, \d+ ms\)"]*3,"INFO","system")
policy=exact("VM_Create policy module=gamesv ",[r"VM_Create policy module=gamesv requested=1 effective=1 backend=wasm-interpreter"]*3,"DEBUG","system")
init=exact("InitGame:",[r"InitGame: .*\\mapname\\arena7(?:\\.*)?"]*3,"INFO","game")
nav=exact("[NAV] navmesh ready for ",[r"\[NAV\] navmesh ready for 'arena7' \((?:built|from cache)\)"],"INFO","nav")
shutdown_game=exact("==== ShutdownGame ====",[r"==== ShutdownGame ===="]*3,"INFO","game")
shutdown_detail=exact("ShutdownGame:",[r"ShutdownGame:"]*3,"INFO","game")
shutdown=exact("----- Server Shutdown ",[r"----- Server Shutdown \(Server quit\) -----"],"INFO","server")
phase=exact("Q0_WASM_RESTART_",[
 r"Q0_WASM_RESTART_INITIAL_READY",r"Q0_WASM_RESTART_ONE_REQUESTED",r"Q0_WASM_RESTART_ONE_COMPLETE",
 r"Q0_WASM_RESTART_TWO_REQUESTED",r"Q0_WASM_RESTART_TWO_COMPLETE",r"Q0_WASM_RESTART_IDENTITY_PRESERVED",
 r"Q0_WASM_RESTART_REPLACEMENT_READY",r"Q0_WASM_RESTART_COMPLETE",r"Q0_WASM_RESTART_QUIT_REQUESTED"],"INFO","system")
kicks=exact("botkick:",[
 r"botkick: refused stale bot identity client=1 expected=999 actual=1",
 r"botkick: refused stale bot identity client=1 expected=2 actual=1",
 r"botkick: refused stale bot identity client=1 expected=2 actual=1",
 r"botkick: removed bot client=1 allocation=1 name=(?:\^[0-9])?RestartBot",
 r"botkick: refused stale bot identity client=1 expected=1 actual=2",
 r"botkick: removed bot client=1 allocation=2 name=(?:\^[0-9])?RestartBot"],"INFO","server")
connect=exact("ClientConnect: ",[r"ClientConnect: 1"]*4,"INFO","game")
begin=exact("ClientBegin: ",[r"ClientBegin: 1"]*4,"INFO","game")
entered=[(i,rows[i],value) for i,value in enumerate(vals) if value.startswith("broadcast: print ") and "RestartBot" in value and "has entered the game" in value]
if len(entered)!=4:raise SystemExit(f"FAIL wasm-restart entered cardinality {len(entered)}")
for _,row,value in entered:
 if row["sev"].upper()!="INFO" or row["cat"].lower()!="server" or re.fullmatch(r'broadcast: print "(?:\^[0-9])?RestartBot(?:\^7)? has entered the game\\n"',value) is None:
  raise SystemExit("FAIL wasm-restart entered body/metadata")
for cohort in range(4):
 if not connect[cohort][0]<entered[cohort][0]<begin[cohort][0]:raise SystemExit("FAIL wasm-restart cohort order")
if not (servers[0][0]<loads[0][0]<policy[0][0]<init[0][0] and servers[0][0]<nav[0][0] and max(init[0][0],nav[0][0])<connect[0][0]<entered[0][0]<begin[0][0]<kicks[0][0]<phase[0][0]<phase[1][0]
 <shutdown_game[0][0]<shutdown_detail[0][0]<loads[1][0]<policy[1][0]<init[1][0]<connect[1][0]<begin[1][0]<phase[2][0]<kicks[1][0]<phase[3][0]
 <shutdown_game[1][0]<shutdown_detail[1][0]<loads[2][0]<policy[2][0]<init[2][0]<connect[2][0]<begin[2][0]<phase[4][0]<kicks[2][0]
 <phase[5][0]<kicks[3][0]<connect[3][0]<begin[3][0]<phase[6][0]<kicks[4][0]<kicks[5][0]<phase[7][0]<phase[8][0]
 <shutdown[0][0]<shutdown_game[2][0]<shutdown_detail[2][0]):raise SystemExit("FAIL wasm-restart causal order")
if any(row["sev"].upper() in ("ERROR","FATAL") for row in rows):raise SystemExit("FAIL wasm-restart severity")
for prefix in ("VM_Restart:","WASM module cannot restart","botkick: inactive ","botkick: refused non-bot ","botkick: invalid "):
 if family(prefix):raise SystemExit(f"FAIL wasm-restart forbidden {prefix}")
if any("VM_Restart on game failed" in value or "Server crashed" in value for value in vals):raise SystemExit("FAIL wasm-restart crash marker")
if family("WASM: failed to load vm/gamesv.aot"):raise SystemExit("FAIL wasm-restart AOT sentinel attempted")

manifest=[]
for number,line in enumerate(open(manifest_path,encoding="utf-8",errors="strict"),1):
 try:item=json.loads(line)
 except Exception as exc:raise SystemExit(f"FAIL wasm-restart manifest {number}: {exc}")
 if not isinstance(item,dict):raise SystemExit("FAIL wasm-restart manifest schema")
 manifest.append(item)
if not manifest or manifest[0]!={"kind":"scenario","schema":1,"name":"wasm-map-restart","map":"arena7","restarts":2}:raise SystemExit("FAIL wasm-restart scenario")
if [item.get("role") for item in manifest[1:-1]] != ["headless","pax21","base","aot-sentinel","harness"]:raise SystemExit("FAIL wasm-restart provenance roles")
for item in manifest[1:-1]:
 if set(item)!={"kind","role","path","bytes","sha256"} or item.get("kind")!="provenance" or not isinstance(item.get("bytes"),int) or item["bytes"]<=0 or re.fullmatch(r"[0-9a-f]{64}",str(item.get("sha256",""))) is None:raise SystemExit("FAIL wasm-restart provenance schema")
 try:data=open(item["path"],"rb").read()
 except OSError as exc:raise SystemExit(f"FAIL wasm-restart provenance inaccessible: {exc}")
 if len(data)!=item["bytes"] or hashlib.sha256(data).hexdigest()!=item["sha256"]:raise SystemExit("FAIL wasm-restart provenance rehash")
result=manifest[-1]
if set(result)!={"kind","pid","rc","timeout","forced"} or result.get("kind")!="result" or not isinstance(result.get("pid"),int) or result["pid"]<=0 or result.get("rc")!=0 or result.get("timeout") is not False or result.get("forced") is not False:raise SystemExit("FAIL wasm-restart result")

controller=[]
for number,line in enumerate(open(controller_path,encoding="utf-8",errors="strict"),1):
 try:item=json.loads(line)
 except Exception as exc:raise SystemExit(f"FAIL wasm-restart controller {number}: {exc}")
 if not isinstance(item,dict):raise SystemExit("FAIL wasm-restart controller schema")
 controller.append(item)
commands=["addbot grunt 4 free 0 RestartBot","botkick 1 999",
 "echo Q0_WASM_RESTART_INITIAL_READY; echo Q0_WASM_RESTART_ONE_REQUESTED; map_restart 0; echo Q0_WASM_RESTART_ONE_COMPLETE",
 "botkick 1 2","echo Q0_WASM_RESTART_TWO_REQUESTED; map_restart 0; echo Q0_WASM_RESTART_TWO_COMPLETE",
 "botkick 1 2","echo Q0_WASM_RESTART_IDENTITY_PRESERVED; botkick 1 1","addbot grunt 4 free 0 RestartBot",
 "echo Q0_WASM_RESTART_REPLACEMENT_READY; botkick 1 1","botkick 1 2",
 "echo Q0_WASM_RESTART_COMPLETE; echo Q0_WASM_RESTART_QUIT_REQUESTED; quit"]
triggers=[max(init[0][0],nav[0][0]),entered[0][0],kicks[0][0],phase[2][0],kicks[1][0],phase[4][0],kicks[2][0],kicks[3][0],entered[3][0],kicks[4][0],kicks[5][0]]
if len(controller)!=len(commands):raise SystemExit("FAIL wasm-restart controller cardinality")
for seq,(event,command,trigger) in enumerate(zip(controller,commands,triggers),1):
 expected={"kind":"command","seq":seq,"trigger_line":physical[trigger],"command":command}
 if event!=expected:raise SystemExit("FAIL wasm-restart controller event")
print("PASS WASM game VM survives two map_restart cycles with stable server bot identity")
PYEOF
}

write_self() {
python3 - "$1" "$2" "$3" "$4" <<'PYEOF'
import hashlib,json,os,sys
log,manifest,controller,mode=sys.argv[1:];rows=[]
def add(sev,cat,msg):rows.append({"sev":sev,"cat":cat,"msg":msg+"\n"})
def cohort():
 add("INFO","game","ClientConnect: 1");add("INFO","server",'broadcast: print "RestartBot has entered the game\\n"');add("INFO","game","ClientBegin: 1")
add("INFO","server","Server: arena7");add("INFO","system","vm/gamesv.wasm loaded as WASM interpreter (4096 KB memory, 3 ms)");add("DEBUG","system","VM_Create policy module=gamesv requested=1 effective=1 backend=wasm-interpreter");add("INFO","game","InitGame: \\mapname\\arena7\\protocol\\74");add("INFO","nav","[NAV] navmesh ready for 'arena7' (built)");cohort()
add("INFO","server","botkick: refused stale bot identity client=1 expected=999 actual=1");add("INFO","system","Q0_WASM_RESTART_INITIAL_READY");add("INFO","system","Q0_WASM_RESTART_ONE_REQUESTED")
add("INFO","game","==== ShutdownGame ====");add("INFO","game","ShutdownGame:");add("INFO","system","vm/gamesv.wasm loaded as WASM interpreter (4096 KB memory, 2 ms)");add("DEBUG","system","VM_Create policy module=gamesv requested=1 effective=1 backend=wasm-interpreter");add("INFO","game","InitGame: \\mapname\\arena7\\protocol\\74");cohort();add("INFO","system","Q0_WASM_RESTART_ONE_COMPLETE");add("INFO","server","botkick: refused stale bot identity client=1 expected=2 actual=1");add("INFO","system","Q0_WASM_RESTART_TWO_REQUESTED")
add("INFO","game","==== ShutdownGame ====");add("INFO","game","ShutdownGame:");add("INFO","system","vm/gamesv.wasm loaded as WASM interpreter (4096 KB memory, 2 ms)");add("DEBUG","system","VM_Create policy module=gamesv requested=1 effective=1 backend=wasm-interpreter");add("INFO","game","InitGame: \\mapname\\arena7\\protocol\\74");cohort();add("INFO","system","Q0_WASM_RESTART_TWO_COMPLETE");add("INFO","server","botkick: refused stale bot identity client=1 expected=2 actual=1");add("INFO","system","Q0_WASM_RESTART_IDENTITY_PRESERVED");add("INFO","server","botkick: removed bot client=1 allocation=1 name=RestartBot");cohort();add("INFO","system","Q0_WASM_RESTART_REPLACEMENT_READY");add("INFO","server","botkick: refused stale bot identity client=1 expected=1 actual=2");add("INFO","server","botkick: removed bot client=1 allocation=2 name=RestartBot");add("INFO","system","Q0_WASM_RESTART_COMPLETE");add("INFO","system","Q0_WASM_RESTART_QUIT_REQUESTED");add("INFO","server","----- Server Shutdown (Server quit) -----");add("INFO","game","==== ShutdownGame ====");add("INFO","game","ShutdownGame:")
def find(text,n=0):return [i for i,row in enumerate(rows) if text in row["msg"]][n]
triggers=[max(find("InitGame:",0),find("navmesh ready",0)),find("has entered",0),find("expected=999"),find("ONE_COMPLETE"),find("expected=2 actual=1",0),find("TWO_COMPLETE"),find("expected=2 actual=1",1),find("allocation=1"),find("has entered",3),find("expected=1 actual=2"),find("allocation=2")]
if mode=="missing-load":rows.pop(find("vm/gamesv.",1))
elif mode=="backend-change":rows[find("vm/gamesv.",2)]["msg"]="vm/gamesv.aot loaded as WASM AOT (4096 KB memory, 2 ms)\n"
elif mode=="wrong-policy":rows[find("VM_Create policy",1)]["msg"]="VM_Create policy module=gamesv requested=2 effective=2 backend=wasm-aot\n"
elif mode=="aot-attempt":rows.insert(find("InitGame:",1),{"sev":"WARN","cat":"system","msg":"WASM: failed to load vm/gamesv.aot: invalid sentinel\n"})
elif mode=="missing-init":rows.pop(find("InitGame:",2))
elif mode=="init-before-load":
 a=find("InitGame:",0);b=find("vm/gamesv.",0);rows[a],rows[b]=rows[b],rows[a]
elif mode=="nav-before-server":
 row=rows.pop(find("navmesh ready"));rows.insert(find("Server: arena7"),row)
elif mode=="additive-nav":rows.insert(find("ClientConnect: 1"),{"sev":"INFO","cat":"nav","msg":"[NAV] navmesh ready for 'arena7' (from cache)\n"})
elif mode=="late-nav":
 row=rows.pop(find("navmesh ready"));rows.insert(find("ClientConnect: 1")+1,row)
elif mode=="nav-metadata":rows[find("navmesh ready")]["cat"]="server"
elif mode=="missing-shutdown":rows.pop(find("==== ShutdownGame ====",1))
elif mode=="server-shutdown":rows.insert(find("TWO_COMPLETE"),{"sev":"INFO","cat":"server","msg":"----- Server Shutdown (stopserver) -----\n"})
elif mode=="identity-changed":rows[find("expected=2 actual=1",1)]["msg"]="botkick: refused stale bot identity client=1 expected=2 actual=2\n"
elif mode=="same-replacement":rows[find("expected=1 actual=2")]["msg"]="botkick: refused stale bot identity client=1 expected=1 actual=1\n"
elif mode=="cohort-order":
 a=find("ClientBegin: 1",2);b=find("has entered",2);rows[a],rows[b]=rows[b],rows[a]
elif mode=="kick-before-begin":
 row=rows.pop(find("expected=999"));rows.insert(find("ClientBegin: 1",0),row)
elif mode=="restart-order":
 a=find("ONE_COMPLETE");b=find("InitGame:",1);rows[a],rows[b]=rows[b],rows[a]
elif mode=="missing-phase":rows.pop(find("TWO_REQUESTED"))
elif mode=="suffix":rows[find("TWO_COMPLETE")]["msg"]="Q0_WASM_RESTART_TWO_COMPLETE extra\n"
elif mode=="metadata":rows[find("vm/gamesv.",1)]["cat"]="game"
elif mode=="smuggle":rows.insert(find("TWO_COMPLETE"),{"sev":"INFO","cat":"system","msg":"benign\rQ0_WASM_RESTART_TWO_COMPLETE"})
elif mode=="error":rows.append({"sev":"ERROR","cat":"system","msg":"VM_Restart on game failed\n"})
elif mode=="crash":rows.append({"sev":"INFO","cat":"server","msg":"----- Server Shutdown (Server crashed) -----\n"})
with open(log,"w") as out:
 for row in rows:out.write(json.dumps(row)+"\n")
items=[{"kind":"scenario","schema":1,"name":"wasm-map-restart","map":"arena7","restarts":2}]
for role in ("headless","pax21","base","aot-sentinel","harness"):
 path=manifest+"."+role;data=("fixture-"+role).encode();open(path,"wb").write(data);items.append({"kind":"provenance","role":role,"path":os.path.abspath(path),"bytes":len(data),"sha256":hashlib.sha256(data).hexdigest()})
items.append({"kind":"result","pid":4242,"rc":0,"timeout":False,"forced":False})
if mode=="manifest":items[-1]["rc"]=1
with open(manifest,"w") as out:
 for item in items:out.write(json.dumps(item)+"\n")
commands=["addbot grunt 4 free 0 RestartBot","botkick 1 999","echo Q0_WASM_RESTART_INITIAL_READY; echo Q0_WASM_RESTART_ONE_REQUESTED; map_restart 0; echo Q0_WASM_RESTART_ONE_COMPLETE","botkick 1 2","echo Q0_WASM_RESTART_TWO_REQUESTED; map_restart 0; echo Q0_WASM_RESTART_TWO_COMPLETE","botkick 1 2","echo Q0_WASM_RESTART_IDENTITY_PRESERVED; botkick 1 1","addbot grunt 4 free 0 RestartBot","echo Q0_WASM_RESTART_REPLACEMENT_READY; botkick 1 1","botkick 1 2","echo Q0_WASM_RESTART_COMPLETE; echo Q0_WASM_RESTART_QUIT_REQUESTED; quit"]
events=[{"kind":"command","seq":i+1,"trigger_line":triggers[i]+1,"command":command} for i,command in enumerate(commands)]
if mode=="controller":events[4]["trigger_line"]+=1
with open(controller,"w") as out:
 for event in events:out.write(json.dumps(event)+"\n")
PYEOF
}

if [ "${1:-}" = --analyze ]; then
 [ "$#" -eq 4 ] || { echo "usage: $0 --analyze <qconsole> <manifest> <controller>"; exit 64; }
 analyze_contract "$2" "$3" "$4"; exit $?
fi
if [ "${1:-}" = --self-test ]; then
 ROOT="$(mktemp -d -t wasm-map-restart-self-XXXXXX 2>/dev/null || mktemp -d)"; trap 'rm -rf "$ROOT"' EXIT
 write_self "$ROOT/clean" "$ROOT/manifest" "$ROOT/controller" clean; analyze_contract "$ROOT/clean" "$ROOT/manifest" "$ROOT/controller" >/dev/null || exit 1
 defects=(missing-load backend-change wrong-policy aot-attempt missing-init init-before-load nav-before-server additive-nav late-nav nav-metadata missing-shutdown server-shutdown identity-changed same-replacement cohort-order kick-before-begin restart-order missing-phase suffix metadata smuggle error crash manifest controller)
 for defect in "${defects[@]}"; do write_self "$ROOT/$defect" "$ROOT/$defect.manifest" "$ROOT/$defect.controller" "$defect"; if analyze_contract "$ROOT/$defect" "$ROOT/$defect.manifest" "$ROOT/$defect.controller" >/dev/null 2>&1; then echo "FAIL accepted $defect"; exit 1; fi; done
 echo "PASS wasm-map-restart analyzer self-test (${#defects[@]} mutations)"; exit 0
fi

HEADLESS="${1:-}"; [ -n "$HEADLESS" ] && [ -x "$HEADLESS" ] || { echo "usage: $0 /absolute/path/to/wired-headless"; exit 64; }
HEADLESS="$(cd "$(dirname "$HEADLESS")" && pwd)/$(basename "$HEADLESS")"; WD="$(dirname "$HEADLESS")"
PACK=""; for candidate in "$WD" "$WD/../Resources"; do [ -f "$candidate/base/pax21.sw3z" ] && PACK="$candidate" && break; done
[ -n "$PACK" ] || { echo "SKIP: current pax21 unavailable"; exit 77; }
CONTENT="${WIRED_CONTENT_ROOT:-$PACK}"; if [ -f "$CONTENT/base/pax01.sw3z" ]; then BASE="$CONTENT/base/pax01.sw3z"; elif [ -f "$CONTENT/base/pak0.pk3" ]; then BASE="$CONTENT/base/pak0.pk3"; else echo "SKIP: set WIRED_CONTENT_ROOT"; exit 77; fi
ROOT="$(mktemp -d -t wasm-map-restart-XXXXXX 2>/dev/null || mktemp -d)"; HOME_DIR="$ROOT/home/q3now-preview"; FIFO="$ROOT/control"; PID=""; OPEN=0; CONTROLLER="$ROOT/controller.jsonl"; CONTROLLER_SEQ=0; WAIT_LINE=0; AOT_SENTINEL="$ROOT/gamesv.aot"
cleanup(){ local status=$?; trap - EXIT INT TERM; [ "$OPEN" -eq 1 ] && exec 8>&- || true; [ -n "$PID" ] && kill -TERM "$PID" 2>/dev/null || true; [ -n "$PID" ] && wait "$PID" 2>/dev/null || true; [ "${WIRED_KEEP_ARTIFACTS:-0}" = 1 ] || rm -rf "$ROOT"; exit "$status"; }
trap cleanup EXIT; trap 'exit 130' INT; trap 'exit 143' TERM
mkdir -p "$HOME_DIR/base/vm"; cp "$PACK/base/pax21.sw3z" "$HOME_DIR/base/" || exit 1; cp "$BASE" "$HOME_DIR/base/" || exit 1
printf 'WIRED_INVALID_AOT_SENTINEL_BYTECODE_MUST_NOT_OPEN\n' >"$AOT_SENTINEL" || exit 1
cp "$AOT_SENTINEL" "$HOME_DIR/base/vm/gamesv.aot" || exit 1
MANIFEST="$ROOT/manifest.jsonl"
python3 - "$MANIFEST" "$HEADLESS" "$PACK/base/pax21.sw3z" "$BASE" "$AOT_SENTINEL" "$0" <<'PYEOF'
import hashlib,json,os,sys
with open(sys.argv[1],"w") as out:
 out.write(json.dumps({"kind":"scenario","schema":1,"name":"wasm-map-restart","map":"arena7","restarts":2},sort_keys=True)+"\n")
 for role,path in zip(("headless","pax21","base","aot-sentinel","harness"),sys.argv[2:]):
  digest=hashlib.sha256()
  with open(path,"rb") as source:
   for chunk in iter(lambda:source.read(1024*1024),b""):digest.update(chunk)
  out.write(json.dumps({"kind":"provenance","role":role,"path":os.path.abspath(path),"bytes":os.path.getsize(path),"sha256":digest.hexdigest()},sort_keys=True)+"\n")
PYEOF
PORT="$(python3 - <<'PYEOF'
import socket
s=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);s.bind(("127.0.0.1",0));print(s.getsockname()[1]);s.close()
PYEOF
)"
mkfifo "$FIFO"; exec 8<>"$FIFO"; OPEN=1
(cd "$WD" && exec "$HEADLESS" +set fs_homepath "$HOME_DIR" +set com_automated 1 +set com_noHardReboot 1 +set net_ip 127.0.0.1 +set net_port "$PORT" +set vm_game 1 +set g_autoBots 0 +set g_minPlayers 0 +set sv_pure 0 +set log_severity DEBUG +set log_file_severity DEBUG +set log_file_mode overwrite_synced +map arena7 <&8) >"$ROOT/stdout" 2>&1 & PID=$!; RUN_PID="$PID"; LOG="$HOME_DIR/qconsole.jsonl"
wait_row(){ local pattern="$1" wanted="$2" sev="$3" cat="$4" limit="${5:-600}" result; for _ in $(seq 1 "$limit"); do result="$(python3 - "$LOG" "$pattern" "$wanted" "$sev" "$cat" <<'PYEOF'
import json,re,sys
path,pattern,wanted,sev,cat=sys.argv[1:];hits=[]
try:
 for number,line in enumerate(open(path,encoding="utf-8",errors="strict"),1):
  if not line.strip():continue
  row=json.loads(line);message=row["msg"][:-1] if row["msg"].endswith("\n") and not row["msg"].endswith("\n\n") else row["msg"]
  if row["sev"].upper()==sev and row["cat"].lower()==cat and re.fullmatch(pattern,message):hits.append(number)
except Exception:raise SystemExit(1)
if len(hits)<int(wanted):raise SystemExit(1)
print(hits[int(wanted)-1])
PYEOF
)" && { WAIT_LINE="$result"; return 0; }; kill -0 "$PID" 2>/dev/null || return 1; sleep .1; done; return 1; }
send_command(){ local command="$1"; CONTROLLER_SEQ=$((CONTROLLER_SEQ+1)); python3 - "$CONTROLLER" "$CONTROLLER_SEQ" "$WAIT_LINE" "$command" <<'PYEOF'
import json,sys
with open(sys.argv[1],"a") as out:out.write(json.dumps({"kind":"command","seq":int(sys.argv[2]),"trigger_line":int(sys.argv[3]),"command":sys.argv[4]},sort_keys=True)+"\n")
PYEOF
 printf '%s\n' "$command" >&8; }
wait_row 'InitGame: .*\\mapname\\arena7(?:\\.*)?' 1 INFO game 1200 || { echo "FAIL initial InitGame"; exit 1; }; INIT_LINE="$WAIT_LINE"; wait_row "\\[NAV\\] navmesh ready for 'arena7' \\((?:built|from cache)\\)" 1 INFO nav 1200 || { echo "FAIL navmesh"; exit 1; }; [ "$INIT_LINE" -gt "$WAIT_LINE" ] && WAIT_LINE="$INIT_LINE"
send_command 'addbot grunt 4 free 0 RestartBot'; wait_row 'broadcast: print "(?:\^[0-9])?RestartBot(?:\^7)? has entered the game\\n"' 1 INFO server || { echo "FAIL initial bot"; exit 1; }
send_command 'botkick 1 999'; wait_row 'botkick: refused stale bot identity client=1 expected=999 actual=1' 1 INFO server || { echo "FAIL initial identity"; exit 1; }
send_command 'echo Q0_WASM_RESTART_INITIAL_READY; echo Q0_WASM_RESTART_ONE_REQUESTED; map_restart 0; echo Q0_WASM_RESTART_ONE_COMPLETE'; wait_row 'Q0_WASM_RESTART_ONE_COMPLETE' 1 INFO system 1200 || { echo "FAIL first restart"; exit 1; }
send_command 'botkick 1 2'; wait_row 'botkick: refused stale bot identity client=1 expected=2 actual=1' 1 INFO server || { echo "FAIL first continuity"; exit 1; }
send_command 'echo Q0_WASM_RESTART_TWO_REQUESTED; map_restart 0; echo Q0_WASM_RESTART_TWO_COMPLETE'; wait_row 'Q0_WASM_RESTART_TWO_COMPLETE' 1 INFO system 1200 || { echo "FAIL second restart"; exit 1; }
send_command 'botkick 1 2'; wait_row 'botkick: refused stale bot identity client=1 expected=2 actual=1' 2 INFO server || { echo "FAIL second continuity"; exit 1; }
send_command 'echo Q0_WASM_RESTART_IDENTITY_PRESERVED; botkick 1 1'; wait_row 'botkick: removed bot client=1 allocation=1 name=(?:\^[0-9])?RestartBot' 1 INFO server || { echo "FAIL current identity"; exit 1; }
send_command 'addbot grunt 4 free 0 RestartBot'; wait_row 'broadcast: print "(?:\^[0-9])?RestartBot(?:\^7)? has entered the game\\n"' 4 INFO server || { echo "FAIL replacement bot"; exit 1; }
send_command 'echo Q0_WASM_RESTART_REPLACEMENT_READY; botkick 1 1'; wait_row 'botkick: refused stale bot identity client=1 expected=1 actual=2' 1 INFO server || { echo "FAIL replacement stale identity"; exit 1; }
send_command 'botkick 1 2'; wait_row 'botkick: removed bot client=1 allocation=2 name=(?:\^[0-9])?RestartBot' 1 INFO server || { echo "FAIL replacement current identity"; exit 1; }
send_command 'echo Q0_WASM_RESTART_COMPLETE; echo Q0_WASM_RESTART_QUIT_REQUESTED; quit'
for _ in $(seq 1 150); do kill -0 "$PID" 2>/dev/null || break; sleep .1; done; kill -0 "$PID" 2>/dev/null && { echo "FAIL forced teardown required"; exit 1; }
wait "$PID"; RC=$?; PID=""; exec 8>&-; OPEN=0
python3 - "$MANIFEST" "$RUN_PID" "$RC" <<'PYEOF'
import json,sys
with open(sys.argv[1],"a") as out:out.write(json.dumps({"kind":"result","pid":int(sys.argv[2]),"rc":int(sys.argv[3]),"timeout":False,"forced":False},sort_keys=True)+"\n")
PYEOF
[ "$RC" -eq 0 ] || { echo "FAIL headless nonzero exit"; exit 1; }
analyze_contract "$LOG" "$MANIFEST" "$CONTROLLER" || exit 1
echo "PASS headless WASM map_restart gate"
