#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors
# Same-process stopserver/map bot slot-allocation identity lifecycle.

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
. "$SCRIPT_DIR/lib/wired_paths.sh"

analyze_contract() {
python3 - "$1" "$2" "$3" <<'PYEOF'
import hashlib,json,os,re,sys
path,manifest_path,controller_path=sys.argv[1:];rows=[];physical=[]
for n,line in enumerate(open(path,encoding="utf-8",errors="replace"),1):
 if not line.strip():continue
 try:row=json.loads(line)
 except Exception as exc:raise SystemExit(f"FAIL bot-restart {n}: {exc}")
 if not isinstance(row,dict):raise SystemExit("FAIL bot-restart schema")
 rows.append(row);physical.append(n)
if not rows:raise SystemExit("FAIL bot-restart empty evidence")
if any(not all(isinstance(r.get(k),str) for k in ("sev","cat","msg")) for r in rows):
 raise SystemExit("FAIL bot-restart log schema")
def norm(value):return value[:-1] if value.endswith("\n") and not value.endswith("\n\n") else value
vals=[norm(r["msg"]) for r in rows]
claimed=("Server: ","----- Server Shutdown ","botkick:","ClientConnect: ",
 "ClientBegin: ","SV_OnPlayerConnect:","broadcast: print ","Q0_BOT_RESTART_",
 "InitGame:","==== ShutdownGame ====","ShutdownGame:")
for value in vals:
 lines=re.split(r"[\r\n]",value)
 if len(lines)>1 and any(line.startswith(claimed) for line in lines):
  raise SystemExit("FAIL bot-restart claimed logical-line smuggling")
def family(prefix):return [(i,rows[i],v) for i,v in enumerate(vals) if v.startswith(prefix)]
def exact(prefix,patterns,sev,cat):
 found=family(prefix)
 if len(found)!=len(patterns):raise SystemExit(f"FAIL bot-restart {prefix} cardinality {len(found)}")
 for (_,row,value),pattern in zip(found,patterns):
  if str(row["sev"]).upper()!=sev or str(row["cat"]).lower()!=cat or re.fullmatch(pattern,value) is None:
   raise SystemExit(f"FAIL bot-restart {prefix} body/metadata {value}")
 return found

servers=exact("Server: ",[r"Server: arena7",r"Server: arena7"],"INFO","server")
nav=exact("[NAV] navmesh ready for ",[
 r"\[NAV\] navmesh ready for 'arena7' \((?:built|from cache)\)",
 r"\[NAV\] navmesh ready for 'arena7' \((?:built|from cache)\)"],"INFO","nav")
init=exact("InitGame:",[r"InitGame: .*\\mapname\\arena7(?:\\.*)?"]*2,"INFO","game")
shutdown_game=exact("==== ShutdownGame ====",[r"==== ShutdownGame ===="]*2,"INFO","game")
shutdown_detail=exact("ShutdownGame:",[r"ShutdownGame:"]*2,"INFO","game")
shutdown=exact("----- Server Shutdown ",[
 r"----- Server Shutdown \(stopserver\) -----",
 r"----- Server Shutdown \(Server quit\) -----"],"INFO","server")
phase=exact("Q0_BOT_RESTART_",[
 r"Q0_BOT_RESTART_FIRST_READY",
 r"Q0_BOT_RESTART_STOP_REQUESTED",
 r"Q0_BOT_RESTART_STOPPED",
 r"Q0_BOT_RESTART_SECOND_READY",
 r"Q0_BOT_RESTART_COMPLETE",
 r"Q0_BOT_RESTART_QUIT_REQUESTED"],"INFO","system")
kicks=exact("botkick:",[
 r"botkick: refused stale bot identity client=1 expected=999 actual=1",
 r"botkick: refused stale bot identity client=1 expected=1 actual=2",
 r"botkick: removed bot client=1 allocation=2 name=(?:\^[0-9])?RestartBot"],"INFO","server")
connect=exact("ClientConnect: ",[r"ClientConnect: 1"]*2,"INFO","game")
begin=exact("ClientBegin: ",[r"ClientBegin: 1"]*2,"INFO","game")
entered=[(i,rows[i],v) for i,v in enumerate(vals)
         if v.startswith("broadcast: print ") and "RestartBot" in v and "has entered the game" in v]
if len(entered)!=2:raise SystemExit(f"FAIL bot-restart enter cardinality {len(entered)}")
for _,row,value in entered:
 if str(row["sev"]).upper()!="INFO" or str(row["cat"]).lower()!="server" or re.fullmatch(
   r'broadcast: print "(?:\^[0-9])?RestartBot(?:\^7)? has entered the game\\n"',value) is None:
  raise SystemExit(f"FAIL bot-restart enter body/metadata {value}")

if not (servers[0][0]<init[0][0]<connect[0][0] and servers[0][0]<nav[0][0]<connect[0][0]
        and connect[0][0]<entered[0][0]<begin[0][0]<kicks[0][0]
        <phase[0][0]<phase[1][0]<shutdown[0][0]<shutdown_game[0][0]<shutdown_detail[0][0]
        <phase[2][0]<servers[1][0]<init[1][0]<connect[1][0]
        and servers[1][0]<nav[1][0]<connect[1][0]
        and connect[1][0]<entered[1][0]<begin[1][0]<kicks[1][0]<kicks[2][0]
        <phase[3][0]<phase[4][0]<phase[5][0]<shutdown[1][0]
        <shutdown_game[1][0]<shutdown_detail[1][0]):
 raise SystemExit("FAIL bot-restart causal order")
if any(str(r["sev"]).upper() in ("ERROR","FATAL") for r in rows):
 raise SystemExit("FAIL bot-restart severity")
for prefix in ("botkick: inactive ","botkick: refused non-bot ","botkick: invalid ",
               "SV_BotAllocateClient: bot allocation identity exhausted"):
 if family(prefix):raise SystemExit(f"FAIL bot-restart forbidden {prefix}")

manifest=[]
for n,line in enumerate(open(manifest_path,encoding="utf-8",errors="replace"),1):
 try:item=json.loads(line)
 except Exception as exc:raise SystemExit(f"FAIL bot-restart manifest {n}: {exc}")
 if not isinstance(item,dict):raise SystemExit("FAIL bot-restart manifest schema")
 manifest.append(item)
if not manifest or manifest[0] != {"kind":"scenario","schema":1,"name":"bot-slot-restart","map":"arena7"}:
 raise SystemExit("FAIL bot-restart manifest scenario")
result=manifest[-1]
if set(result)!={"kind","pid","rc","timeout","forced"} or result.get("kind")!="result" \
  or not isinstance(result.get("pid"),int) or result["pid"]<=0 or result.get("rc")!=0 \
  or result.get("timeout") is not False or result.get("forced") is not False:
 raise SystemExit("FAIL bot-restart manifest result")
provenance=manifest[1:-1]
if [item.get("role") for item in provenance] != ["headless","current-archive","content-archive","harness"]:
 raise SystemExit("FAIL bot-restart manifest roles")
for item in provenance:
 if set(item)!={"kind","role","path","bytes","sha256"} or item.get("kind")!="provenance" \
   or not isinstance(item.get("path"),str) or not item["path"] \
   or not isinstance(item.get("bytes"),int) or item["bytes"]<=0 \
   or re.fullmatch(r"[0-9a-f]{64}",str(item.get("sha256",""))) is None:
  raise SystemExit("FAIL bot-restart manifest provenance")
 try:
  data=open(item["path"],"rb").read()
 except OSError as exc:raise SystemExit(f"FAIL bot-restart provenance inaccessible: {exc}")
 if len(data)!=item["bytes"] or hashlib.sha256(data).hexdigest()!=item["sha256"]:
  raise SystemExit("FAIL bot-restart provenance rehash")

controller=[]
for n,line in enumerate(open(controller_path,encoding="utf-8",errors="replace"),1):
 try:item=json.loads(line)
 except Exception as exc:raise SystemExit(f"FAIL bot-restart controller {n}: {exc}")
 if not isinstance(item,dict):raise SystemExit("FAIL bot-restart controller schema")
 controller.append(item)
wanted_commands=[
 "addbot grunt 4 free 0 RestartBot",
 "botkick 1 999",
 "echo Q0_BOT_RESTART_FIRST_READY; echo Q0_BOT_RESTART_STOP_REQUESTED; stopserver; echo Q0_BOT_RESTART_STOPPED; map arena7",
 "addbot grunt 4 free 0 RestartBot",
 "botkick 1 1",
 "botkick 1 2",
 "echo Q0_BOT_RESTART_SECOND_READY; echo Q0_BOT_RESTART_COMPLETE; echo Q0_BOT_RESTART_QUIT_REQUESTED; quit"]
if len(controller)!=len(wanted_commands):raise SystemExit("FAIL bot-restart controller cardinality")
expected_trigger_indices=[max(init[0][0],nav[0][0]),entered[0][0],kicks[0][0],
 max(init[1][0],nav[1][0]),entered[1][0],kicks[1][0],kicks[2][0]]
expected_trigger_lines=[physical[index] for index in expected_trigger_indices]
for seq,(event,command,line) in enumerate(zip(controller,wanted_commands,expected_trigger_lines),1):
 if event != {"kind":"command","seq":seq,"trigger_line":line,"command":command}:
  raise SystemExit("FAIL bot-restart controller event")
print("PASS bot slot identity survives full same-process server shutdown/restart")
PYEOF
}

write_self() {
python3 - "$1" "$2" "$3" "$4" <<'PYEOF'
import hashlib,json,os,sys
path,manifest_path,controller_path,mode=sys.argv[1:];rows=[]
def add(sev,cat,msg):rows.append({"sev":sev,"cat":cat,"msg":msg+"\n"})
add("INFO","server","Server: arena7")
add("INFO","game","InitGame: \\mapname\\arena7\\protocol\\74")
add("INFO","nav","[NAV] navmesh ready for 'arena7' (built)")
add("INFO","game","ClientConnect: 1")
add("INFO","server",'broadcast: print "RestartBot has entered the game\\n"')
add("INFO","game","ClientBegin: 1")
add("INFO","server","botkick: refused stale bot identity client=1 expected=999 actual=1")
add("INFO","system","Q0_BOT_RESTART_FIRST_READY")
add("INFO","system","Q0_BOT_RESTART_STOP_REQUESTED")
add("INFO","server","----- Server Shutdown (stopserver) -----")
add("INFO","game","==== ShutdownGame ====")
add("INFO","game","ShutdownGame:")
add("INFO","system","Q0_BOT_RESTART_STOPPED")
add("INFO","server","Server: arena7")
add("INFO","game","InitGame: \\mapname\\arena7\\protocol\\74")
add("INFO","nav","[NAV] navmesh ready for 'arena7' (from cache)")
add("INFO","game","ClientConnect: 1")
add("INFO","server",'broadcast: print "RestartBot has entered the game\\n"')
add("INFO","game","ClientBegin: 1")
add("INFO","server","botkick: refused stale bot identity client=1 expected=1 actual=2")
add("INFO","server","botkick: removed bot client=1 allocation=2 name=RestartBot")
add("INFO","system","Q0_BOT_RESTART_SECOND_READY")
add("INFO","system","Q0_BOT_RESTART_COMPLETE")
add("INFO","system","Q0_BOT_RESTART_QUIT_REQUESTED")
add("INFO","server","----- Server Shutdown (Server quit) -----")
add("INFO","game","==== ShutdownGame ====")
add("INFO","game","ShutdownGame:")
def find(text,n=0):return [i for i,r in enumerate(rows) if text in r["msg"]][n]
canonical_trigger_lines=[
 max(find("InitGame:",0),find("navmesh ready",0))+1,
 find("RestartBot has entered",0)+1,
 find("expected=999 actual=1")+1,
 max(find("InitGame:",1),find("navmesh ready",1))+1,
 find("RestartBot has entered",1)+1,
 find("expected=1 actual=2")+1,
 find("removed bot")+1]
if mode=="same-id":rows[find("expected=1 actual=2")]["msg"]="botkick: refused stale bot identity client=1 expected=1 actual=1\n"
elif mode=="wrong-slot":rows[find("expected=1 actual=2")]["msg"]="botkick: refused stale bot identity client=2 expected=1 actual=2\n"
elif mode=="wrong-name":rows[find("allocation=2")]["msg"]="botkick: removed bot client=1 allocation=2 name=OtherBot\n"
elif mode=="early-remove":
 row=rows.pop(find("removed bot"));rows.insert(find("expected=1 actual=2"),row)
elif mode=="missing-first":rows.pop(find("expected=999"))
elif mode=="missing-second":rows.pop(find("expected=1 actual=2"))
elif mode=="duplicate-kick":
 i=find("expected=1 actual=2");rows.insert(i,dict(rows[i]))
elif mode=="no-stop":rows.pop(find("Shutdown (stopserver)"))
elif mode=="wrong-shutdown":rows[find("Shutdown (stopserver)")]["msg"]="----- Server Shutdown (Server quit) -----\n"
elif mode=="one-map":rows.pop(find("Server: arena7",1))
elif mode=="wrong-second-slot":rows[find("ClientConnect: 1",1)]["msg"]="ClientConnect: 2\n"
elif mode=="wrong-second-name":rows[find("RestartBot has entered",1)]["msg"]='broadcast: print "OtherBot has entered the game\\n"\n'
elif mode=="missing-complete":rows.pop(find("Q0_BOT_RESTART_COMPLETE"))
elif mode=="phase-order":
 a=find("SECOND_READY");b=find("removed bot");rows[a],rows[b]=rows[b],rows[a]
elif mode=="metadata":rows[find("expected=1 actual=2")]["cat"]="game"
elif mode=="suffix":rows[find("expected=1 actual=2")]["msg"]="botkick: refused stale bot identity client=1 expected=1 actual=2 EXTRA\n"
elif mode=="multiline":rows.insert(find("expected=1 actual=2"),{"sev":"INFO","cat":"server","msg":"benign\rbotkick: removed bot client=1 allocation=2 name=RestartBot"})
elif mode=="error":rows.append({"sev":"ERROR","cat":"server","msg":"forged\n"})
elif mode=="inactive":rows.insert(find("Q0_BOT_RESTART_COMPLETE"),{"sev":"INFO","cat":"server","msg":"botkick: inactive client slot=1\n"})
elif mode=="missing-nav":rows.pop(find("navmesh ready",1))
elif mode=="missing-stopped":rows.pop(find("Q0_BOT_RESTART_STOPPED"))
elif mode=="missing-init":rows.pop(find("InitGame:",1))
elif mode=="missing-shutdown-game":rows.pop(find("==== ShutdownGame ====",0))
elif mode=="lifecycle-order":
 a=find("ShutdownGame:",0);b=find("Q0_BOT_RESTART_STOPPED");rows[a],rows[b]=rows[b],rows[a]
elif mode=="begin-before-entered":
 a=find("ClientBegin: 1",1);b=find("RestartBot has entered",1);rows[a],rows[b]=rows[b],rows[a]
with open(path,"w") as out:
 for row in rows:out.write(json.dumps(row)+"\n")
manifest=[{"kind":"scenario","schema":1,"name":"bot-slot-restart","map":"arena7"}]
for role in ("headless","current-archive","content-archive","harness"):
 fixture=manifest_path+"."+role
 data=("fixture-"+role).encode();open(fixture,"wb").write(data)
 manifest.append({"kind":"provenance","role":role,"path":os.path.abspath(fixture),"bytes":len(data),"sha256":hashlib.sha256(data).hexdigest()})
manifest.append({"kind":"result","pid":4242,"rc":0,"timeout":False,"forced":False})
if mode=="manifest-role":manifest[-2]["role"]="other"
elif mode=="manifest-hash":manifest[-2]["sha256"]="0"*64
elif mode=="manifest-result":manifest[-1]["forced"]=True
with open(manifest_path,"w") as out:
 for row in manifest:out.write(json.dumps(row)+"\n")
commands=[
 "addbot grunt 4 free 0 RestartBot","botkick 1 999",
 "echo Q0_BOT_RESTART_FIRST_READY; echo Q0_BOT_RESTART_STOP_REQUESTED; stopserver; echo Q0_BOT_RESTART_STOPPED; map arena7",
 "addbot grunt 4 free 0 RestartBot","botkick 1 1","botkick 1 2",
 "echo Q0_BOT_RESTART_SECOND_READY; echo Q0_BOT_RESTART_COMPLETE; echo Q0_BOT_RESTART_QUIT_REQUESTED; quit"]
controller=[{"kind":"command","seq":i+1,"trigger_line":canonical_trigger_lines[i],"command":command} for i,command in enumerate(commands)]
if mode=="controller-command":controller[2]["command"]="stopserver"
elif mode=="controller-line":controller[3]["trigger_line"]+=1
elif mode=="forged-line":controller[0]["trigger_line"]=find("Server: arena7")+1
with open(controller_path,"w") as out:
 for row in controller:out.write(json.dumps(row)+"\n")
PYEOF
}

if [ "${1:-}" = --analyze ]; then
 [ "$#" -eq 4 ] || { echo "usage: $0 --analyze <server-qconsole.jsonl> <manifest.jsonl> <controller.jsonl>"; exit 64; }
 analyze_contract "$2" "$3" "$4"; exit $?
fi
if [ "${1:-}" = --self-test ]; then
 ROOT="$(mktemp -d -t bot-slot-restart-self-XXXXXX 2>/dev/null || mktemp -d)"; trap 'rm -rf "$ROOT"' EXIT
 python3 - "$0" <<'PYEOF'
import sys
source=open(sys.argv[1],encoding="utf-8").read()
required=("local status=$?","trap - EXIT INT TERM","exit \"$status\"",
          "trap cleanup EXIT","trap 'exit 130' INT","trap 'exit 143' TERM",
          'analyze_contract "$LOG" "$MANIFEST" "$CONTROLLER" || exit 1')
if any(token not in source for token in required):raise SystemExit("FAIL cleanup status preservation")
PYEOF
 write_self "$ROOT/clean" "$ROOT/clean.manifest" "$ROOT/clean.controller" clean; analyze_contract "$ROOT/clean" "$ROOT/clean.manifest" "$ROOT/clean.controller" >/dev/null || exit 1
 defects=(same-id wrong-slot wrong-name early-remove missing-first missing-second duplicate-kick no-stop wrong-shutdown one-map wrong-second-slot wrong-second-name missing-complete phase-order metadata suffix multiline error inactive missing-nav missing-stopped missing-init missing-shutdown-game lifecycle-order begin-before-entered manifest-role manifest-hash manifest-result controller-command controller-line forged-line)
 for defect in "${defects[@]}"; do
  write_self "$ROOT/$defect" "$ROOT/$defect.manifest" "$ROOT/$defect.controller" "$defect"
  if analyze_contract "$ROOT/$defect" "$ROOT/$defect.manifest" "$ROOT/$defect.controller" >/dev/null 2>&1; then echo "FAIL accepted $defect"; exit 1; fi
 done
 echo "PASS bot-slot-restart analyzer self-test (${#defects[@]} mutations)"; exit 0
fi

HEADLESS="${1:-}"; [ -n "$HEADLESS" ] && [ -x "$HEADLESS" ] || { echo "usage: $0 /absolute/path/to/wired-headless"; exit 64; }
HEADLESS="$(cd "$(dirname "$HEADLESS")" && pwd)/$(basename "$HEADLESS")"; WD="$(dirname "$HEADLESS")"
PACK="$(wired_find_archive_root "$WD" "$WD/../Resources" 2>/dev/null || true)"
[ -n "$PACK" ] || { echo "SKIP: current VFS archives unavailable"; exit 77; }
CONTENT="$(wired_find_archive_root "${WIRED_CONTENT_ROOT:-}" "$WIRED_HOME" "$PACK" 2>/dev/null || true)"
[ -n "$CONTENT" ] || { echo "SKIP: set WIRED_CONTENT_ROOT"; exit 77; }
CURRENT_ARCHIVE="$(wired_first_archive "$PACK/base")" || exit 77
CONTENT_ARCHIVE="$(wired_first_archive "$CONTENT/base")" || exit 77
ROOT="$(mktemp -d -t bot-slot-restart-XXXXXX 2>/dev/null || mktemp -d)"; HOME_DIR="$ROOT/home/q3now-preview"; FIFO="$ROOT/control"; PID=""; OPEN=0; CONTROLLER="$ROOT/controller.jsonl"; CONTROLLER_SEQ=0; WAIT_LINE=0
cleanup(){
 local status=$?
 trap - EXIT INT TERM
 [ "$OPEN" -eq 1 ] && exec 8>&- || true
 [ -n "$PID" ] && kill -TERM "$PID" 2>/dev/null || true
 [ -n "$PID" ] && wait "$PID" 2>/dev/null || true
 [ "${WIRED_KEEP_ARTIFACTS:-0}" = 1 ] || rm -rf "$ROOT"
 exit "$status"
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM
mkdir -p "$HOME_DIR/base"; wired_link_content_into_home "$HOME_DIR" "$CONTENT/base" "$PACK/base" || exit 1
MANIFEST="$ROOT/manifest.jsonl"
python3 - "$MANIFEST" "$HEADLESS" "$CURRENT_ARCHIVE" "$CONTENT_ARCHIVE" "$0" <<'PYEOF'
import hashlib,json,os,sys
out=sys.argv[1];roles=("headless","current-archive","content-archive","harness")
with open(out,"w",encoding="utf-8") as stream:
 stream.write(json.dumps({"kind":"scenario","schema":1,"name":"bot-slot-restart","map":"arena7"},sort_keys=True)+"\n")
 for role,path in zip(roles,sys.argv[2:]):
  digest=hashlib.sha256()
  with open(path,"rb") as source:
   for chunk in iter(lambda:source.read(1024*1024),b""):digest.update(chunk)
  stream.write(json.dumps({"kind":"provenance","role":role,"path":os.path.abspath(path),
   "bytes":os.path.getsize(path),"sha256":digest.hexdigest()},sort_keys=True)+"\n")
PYEOF
PORT="$(python3 - <<'PYEOF'
import socket
s=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);s.bind(("127.0.0.1",0))
print(s.getsockname()[1]);s.close()
PYEOF
)"
mkfifo "$FIFO"; exec 8<>"$FIFO"; OPEN=1
(cd "$WD" && exec "$HEADLESS" +set fs_homepath "$HOME_DIR" +set com_automated 1 +set com_noHardReboot 1 +set net_ip 127.0.0.1 +set net_port "$PORT" +set g_autoBots 0 +set sv_pure 0 +set log_severity DEBUG +set log_file_severity DEBUG +set log_file_mode overwrite_synced +map arena7 <&8) >"$ROOT/stdout" 2>&1 & PID=$!
RUN_PID="$PID"
LOG="$HOME_DIR/qconsole.jsonl"
wait_row(){
 local pattern="$1" wanted="$2" sev="$3" cat="$4" limit="${5:-600}" result
 for _ in $(seq 1 "$limit"); do
  result="$(python3 - "$LOG" "$pattern" "$wanted" "$sev" "$cat" <<'PYEOF'
import json,re,sys
path,pattern,wanted,sev,cat=sys.argv[1:];hits=[]
try:
 for number,line in enumerate(open(path,encoding="utf-8",errors="strict"),1):
  if not line.strip():continue
  row=json.loads(line)
  if not isinstance(row,dict) or not all(isinstance(row.get(k),str) for k in ("sev","cat","msg")):raise ValueError("schema")
  message=row["msg"][:-1] if row["msg"].endswith("\n") and not row["msg"].endswith("\n\n") else row["msg"]
  if row["sev"].upper()==sev and row["cat"].lower()==cat and re.fullmatch(pattern,message):hits.append(number)
except (OSError,UnicodeError,ValueError,json.JSONDecodeError):raise SystemExit(1)
count=int(wanted)
if len(hits)<count:raise SystemExit(1)
print(hits[count-1])
PYEOF
)" && { WAIT_LINE="$result"; return 0; }
  kill -0 "$PID" 2>/dev/null || return 1
  sleep .1
 done
 return 1
}
send_command(){
 local command="$1"
 CONTROLLER_SEQ=$((CONTROLLER_SEQ+1))
 python3 - "$CONTROLLER" "$CONTROLLER_SEQ" "$WAIT_LINE" "$command" <<'PYEOF'
import json,sys
path,seq,line,command=sys.argv[1:]
with open(path,"a",encoding="utf-8") as out:out.write(json.dumps({"kind":"command","seq":int(seq),"trigger_line":int(line),"command":command},sort_keys=True)+"\n")
PYEOF
 printf '%s\n' "$command" >&8
}
wait_row 'Server: arena7' 1 INFO server || { echo "FAIL first map"; exit 1; }
wait_row 'InitGame: .*\\mapname\\arena7(?:\\.*)?' 1 INFO game 1200 || { echo "FAIL first InitGame"; exit 1; }
INIT_LINE="$WAIT_LINE"
wait_row "\\[NAV\\] navmesh ready for 'arena7' \\((?:built|from cache)\\)" 1 INFO nav 1200 || { echo "FAIL first navmesh"; exit 1; }
[ "$INIT_LINE" -gt "$WAIT_LINE" ] && WAIT_LINE="$INIT_LINE"
send_command 'addbot grunt 4 free 0 RestartBot'
wait_row 'broadcast: print "(?:\^[0-9])?RestartBot(?:\^7)? has entered the game\\n"' 1 INFO server || { echo "FAIL first bot"; exit 1; }
send_command 'botkick 1 999'
wait_row 'botkick: refused stale bot identity client=1 expected=999 actual=1' 1 INFO server || { echo "FAIL first identity"; exit 1; }
send_command 'echo Q0_BOT_RESTART_FIRST_READY; echo Q0_BOT_RESTART_STOP_REQUESTED; stopserver; echo Q0_BOT_RESTART_STOPPED; map arena7'
wait_row '----- Server Shutdown \(stopserver\) -----' 1 INFO server || { echo "FAIL stopserver"; exit 1; }
wait_row 'Q0_BOT_RESTART_STOPPED' 1 INFO system || { echo "FAIL compound stop marker"; exit 1; }
wait_row 'Server: arena7' 2 INFO server || { echo "FAIL second map"; exit 1; }
wait_row 'InitGame: .*\\mapname\\arena7(?:\\.*)?' 2 INFO game 1200 || { echo "FAIL second InitGame"; exit 1; }
INIT_LINE="$WAIT_LINE"
wait_row "\\[NAV\\] navmesh ready for 'arena7' \\((?:built|from cache)\\)" 2 INFO nav 1200 || { echo "FAIL second navmesh"; exit 1; }
[ "$INIT_LINE" -gt "$WAIT_LINE" ] && WAIT_LINE="$INIT_LINE"
send_command 'addbot grunt 4 free 0 RestartBot'
wait_row 'broadcast: print "(?:\^[0-9])?RestartBot(?:\^7)? has entered the game\\n"' 2 INFO server || { echo "FAIL second bot"; exit 1; }
send_command 'botkick 1 1'
wait_row 'botkick: refused stale bot identity client=1 expected=1 actual=2' 1 INFO server || { echo "FAIL stale identity"; exit 1; }
send_command 'botkick 1 2'
wait_row 'botkick: removed bot client=1 allocation=2 name=(?:\^[0-9])?RestartBot' 1 INFO server || { echo "FAIL current identity"; exit 1; }
send_command 'echo Q0_BOT_RESTART_SECOND_READY; echo Q0_BOT_RESTART_COMPLETE; echo Q0_BOT_RESTART_QUIT_REQUESTED; quit'
for _ in $(seq 1 150); do kill -0 "$PID" 2>/dev/null || break; sleep .1; done
kill -0 "$PID" 2>/dev/null && { echo "FAIL forced teardown required"; exit 1; }
wait "$PID"; RC=$?; PID=""; exec 8>&-; OPEN=0
python3 - "$MANIFEST" "$RUN_PID" "$RC" <<'PYEOF'
import json,sys
with open(sys.argv[1],"a",encoding="utf-8") as out:out.write(json.dumps({"kind":"result","pid":int(sys.argv[2]),"rc":int(sys.argv[3]),"timeout":False,"forced":False},sort_keys=True)+"\n")
PYEOF
[ "$RC" -eq 0 ] || { echo "FAIL headless nonzero exit"; exit 1; }
analyze_contract "$LOG" "$MANIFEST" "$CONTROLLER" || exit 1
echo "PASS headless bot-slot restart gate"
