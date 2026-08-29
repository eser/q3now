#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors
# Local listen-server pause longer than cl_timeout must preserve one live match.

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
. "$SCRIPT_DIR/lib/wired_paths.sh"
TIMEOUT_RUNNER="$SCRIPT_DIR/run-with-timeout.py"

analyze_contract() {
python3 - "$1" "$2" "$3" "$4" <<'PYEOF'
import datetime,hashlib,json,os,re,sys
log_path,game_path,manifest_path,port=sys.argv[1:]
rows=[]
for number,line in enumerate(open(log_path,encoding="utf-8",errors="strict"),1):
 if not line.strip():continue
 try:row=json.loads(line)
 except Exception as exc:raise SystemExit(f"FAIL local-timeout JSON {number}: {exc}")
 if not isinstance(row,dict) or not all(isinstance(row.get(k),str) for k in ("ts","sev","cat","msg")):raise SystemExit("FAIL local-timeout log schema")
 rows.append(row)
if not rows:raise SystemExit("FAIL local-timeout empty evidence")
game=[]
for number,line in enumerate(open(game_path,encoding="utf-8",errors="strict"),1):
 value=line.rstrip("\r\n")
 if not value:continue
 match=re.fullmatch(r"\s*[0-9]+:[0-9]{2} (.*)",value)
 if not match:raise SystemExit(f"FAIL local-timeout games.log physical row {number}: {value}")
 game.append(match.group(1))
if not game:raise SystemExit("FAIL local-timeout empty games.log evidence")
def norm(value):return value[:-1] if value.endswith("\n") and not value.endswith("\n\n") else value
vals=[norm(row["msg"]) for row in rows]
claimed=("Q0_LOCAL_TIMEOUT_","WiredUI: ingame state ","WiredUI: SetActiveMenu ","WiredUI: pop menu ","WiredUI: close all ","wui_test_keydown:","wui_menu_nav focus:","wui_menu_nav: K_","in-mem game:","in-mem: app ","SV_OnPlayerConnect:","cls.state: -> CA_ACTIVE (FIRST GAMEPLAY FRAME","Server: ","ListenWitness: ","----- Server Shutdown ","==== ShutdownGame ====",'"cl_timeout" is:','"cl_paused" is:','"sv_paused" is:')
for value in vals:
 parts=re.split(r"[\r\n]",value)
 if len(parts)>1 and any(part.startswith(claimed) for part in parts):raise SystemExit("FAIL local-timeout claimed logical-line smuggling")
if any(row["sev"].upper() in ("ERROR","FATAL") for row in rows):raise SystemExit("FAIL local-timeout severity")
for value in vals:
 if ("Server connection timed out" in value or value.startswith("Connect failed ") or
     value.startswith("Server disconnected") or "Server crashed" in value):
  raise SystemExit(f"FAIL local-timeout disconnect/crash marker: {value}")
def exact(prefix,pattern,sev,cat,count=1):
 found=[(i,row,value) for i,(row,value) in enumerate(zip(rows,vals)) if value.startswith(prefix)]
 if len(found)!=count:raise SystemExit(f"FAIL local-timeout {prefix} cardinality {len(found)}")
 for _,row,value in found:
  if row["sev"].upper()!=sev or row["cat"].lower()!=cat or re.fullmatch(pattern,value) is None:
   raise SystemExit(f"FAIL local-timeout {prefix} body/metadata: {value}")
 return found
def gexact(prefix,pattern,count=1):
 found=[(i,value) for i,value in enumerate(game) if value.startswith(prefix)]
 if len(found)!=count:raise SystemExit(f"FAIL local-timeout games.log {prefix} cardinality {len(found)}")
 for _,value in found:
  if re.fullmatch(pattern,value) is None:raise SystemExit(f"FAIL local-timeout games.log {prefix} body: {value}")
 return found
def stamp(item):
 value=item[1]["ts"]
 try:
  parsed=datetime.datetime.fromisoformat(value[:-1]+"+00:00" if value.endswith("Z") else value)
 except Exception as exc:raise SystemExit(f"FAIL local-timeout timestamp {value}: {exc}")
 if parsed.tzinfo is None:raise SystemExit(f"FAIL local-timeout timestamp lacks timezone {value}")
 return int(parsed.timestamp()*1000)
markers=exact("Q0_LOCAL_TIMEOUT_",r"Q0_LOCAL_TIMEOUT_(?:PRE|PAUSE_BEGIN|PAUSE_END|RESUMED|COMPLETE|QUIT_REQUESTED)","INFO","system",6)
if [value for _,_,value in markers] != ["Q0_LOCAL_TIMEOUT_PRE","Q0_LOCAL_TIMEOUT_PAUSE_BEGIN","Q0_LOCAL_TIMEOUT_PAUSE_END","Q0_LOCAL_TIMEOUT_RESUMED","Q0_LOCAL_TIMEOUT_COMPLETE","Q0_LOCAL_TIMEOUT_QUIT_REQUESTED"]:raise SystemExit("FAIL local-timeout marker vector")
server=exact("Server: ",r"Server: arena7","INFO","server")[0]
init=exact("InitGame:",r"InitGame: .*\\mapname\\arena7(?:\\.*)?","INFO","game")[0]
listen=exact("WiredNet: listening on port ",rf"WiredNet: listening on port {re.escape(port)} \(IPv4\), ALPN: .+, max clients: [1-9][0-9]*, cert=.+ key=.+","INFO","network")[0]
alloc=exact("in-mem game: allocated conn slot ",r"in-mem game: allocated conn slot 0 \(handle 110\) for app 0","DEBUG","network")[0]
inmem=exact("in-mem: app ",r"in-mem: app 0 connected \(client handle 100, server handle 110\)","INFO","network")[0]
svconnect=exact("SV_OnPlayerConnect: conn=",r"SV_OnPlayerConnect: conn=110","DEBUG","server")[0]
assigned=exact("SV_OnPlayerConnect: slot ",r"SV_OnPlayerConnect: slot 0 assigned to conn=110 \(loopback\)","DEBUG","server")[0]
first=exact("cls.state: -> CA_ACTIVE (FIRST GAMEPLAY FRAME",r"cls\.state: -> CA_ACTIVE \(FIRST GAMEPLAY FRAME mapname=maps/arena7\.bsp serverTime=[0-9]+ numEntities=[0-9]+ framecount=[0-9]+\)","INFO","client")[0]
setactive=exact("WiredUI: SetActiveMenu ",r"WiredUI: SetActiveMenu 2 \(depth 0\)","DEBUG","ui")[0]
keydown=exact("wui_test_keydown:",r"wui_test_keydown: dispatched keycode 27","DEBUG","ui")[0]
focus=exact("wui_menu_nav focus:",r"wui_menu_nav focus: focused item 'btn_resume' \(top index -1\)","DEBUG","ui")[0]
enter=exact("wui_menu_nav: K_",r"wui_menu_nav: K_ENTER dispatched","DEBUG","ui")[0]
if any(value.startswith("WiredUI: pop menu ") for value in vals):raise SystemExit("FAIL local-timeout unexpected pop")
closeall=exact("WiredUI: close all ",r"WiredUI: close all postcondition depth=0 active=none catcher_ui=0 paused=0","DEBUG","ui")[0]
timeout_value=exact('"cl_timeout" is:',r'"cl_timeout" is:"1(?:\^7)?"',"INFO","system")[0]
paused_values=exact('"cl_paused" is:',r'"cl_paused" is:"[01](?:\^7)?"',"INFO","system",3)
server_paused_values=exact('"sv_paused" is:',r'"sv_paused" is:"[01](?:\^7)?"',"INFO","system",3)
def cvar_bit(value):return int(re.search(r'is:"([01])',value).group(1))
if [cvar_bit(x[2]) for x in paused_values]!=[1,1,0] or [cvar_bit(x[2]) for x in server_paused_values[:2]]!=[1,1]:raise SystemExit("FAIL local-timeout pause cvar vector")
say_client=exact("ListenWitness:",r"ListenWitness: Q0_LISTEN_RESUME_ROUNDTRIP","INFO","cgame")[0]
shutdown=exact("----- Server Shutdown ",r"----- Server Shutdown \(Server quit\) -----","INFO","server")[0]
shutdown_game=exact("==== ShutdownGame ====",r"==== ShutdownGame ====","INFO","game")[0]
trace=exact("WiredUI: ingame state ",r"WiredUI: ingame state active=1 demo=0 catcher_ui=[01] paused=[01] connection_present=1 address=([^ ]+) server_time=([0-9]+) top=(?:none|ingame) depth=0 focused=(?:none|btn_resume)","DEBUG","ui",5)
parsed=[]
for _,_,value in trace:
 m=re.fullmatch(r"WiredUI: ingame state active=1 demo=0 catcher_ui=([01]) paused=([01]) connection_present=1 address=([^ ]+) server_time=([0-9]+) top=(none|ingame) depth=0 focused=(none|btn_resume)",value)
 parsed.append((int(m.group(1)),int(m.group(2)),m.group(3),int(m.group(4)),m.group(5),m.group(6)))
if [(x[0],x[1],x[4]) for x in parsed] != [(0,0,"none"),(1,1,"ingame"),(1,1,"ingame"),(0,0,"none"),(0,0,"none")]:raise SystemExit("FAIL local-timeout state vector")
if parsed[0][2] in ("none","") or len({x[2] for x in parsed})!=1:raise SystemExit("FAIL local-timeout connection address identity")
times=[x[3] for x in parsed]
if times[2]!=times[1]:raise SystemExit(f"FAIL local-timeout paused server time advanced {times[1]}->{times[2]}")
if times[3]<times[2] or times[4]<=times[3]:raise SystemExit(f"FAIL local-timeout post-resume progress {times}")
stamps=[stamp((i,row,value)) for i,(row,value) in enumerate(zip(rows,vals))]
if any(a>b for a,b in zip(stamps,stamps[1:])):raise SystemExit("FAIL local-timeout non-monotonic timestamps")
pause_elapsed=stamp(markers[2])-stamp(markers[1])
if pause_elapsed<2300 or pause_elapsed>=10000:raise SystemExit(f"FAIL local-timeout wall pause {pause_elapsed}ms")
quit_index=markers[5][0]
if any("TLV ACCEPT" in value for value in vals):raise SystemExit("FAIL local-timeout unexpected network ACCEPT")
if any(("CL_Disconnect" in value or value.startswith("Server disconnected") or value.startswith("WiredNet game: freed conn")) for value in vals[first[0]+1:quit_index]):raise SystemExit("FAIL local-timeout active-match disconnect")
order=(listen[0],server[0],closeall[0],alloc[0],inmem[0],init[0],svconnect[0],assigned[0],first[0],markers[0][0],trace[0][0],timeout_value[0],setactive[0],keydown[0],markers[1][0],trace[1][0],paused_values[0][0],server_paused_values[0][0],markers[2][0],trace[2][0],paused_values[1][0],server_paused_values[1][0],focus[0],enter[0],markers[3][0],trace[3][0],paused_values[2][0],server_paused_values[2][0],say_client[0],markers[4][0],trace[4][0],markers[5][0],shutdown[0],shutdown_game[0])
if any(a>=b for a,b in zip(order,order[1:])):raise SystemExit(f"FAIL local-timeout causal order {order}")
ginit=gexact("InitGame:",r"InitGame: .*\\mapname\\arena7(?:\\.*)?")[0]
gconnect=gexact("ClientConnect: ",r"ClientConnect: 0")[0]
gbegin=gexact("ClientBegin: ",r"ClientBegin: 0")[0]
gsay=gexact("say: ",r"say: ListenWitness: Q0_LISTEN_RESUME_ROUNDTRIP")[0]
gshutdown=gexact("ShutdownGame:",r"ShutdownGame:")[0]
if not ginit[0]<gconnect[0]<gbegin[0]<gsay[0]<gshutdown[0]:raise SystemExit("FAIL local-timeout games.log order")
manifest=[]
for number,line in enumerate(open(manifest_path,encoding="utf-8",errors="strict"),1):
 try:item=json.loads(line)
 except Exception as exc:raise SystemExit(f"FAIL local-timeout manifest JSON {number}: {exc}")
 if not isinstance(item,dict):raise SystemExit("FAIL local-timeout manifest schema")
 manifest.append(item)
scenario={"kind":"scenario","schema":1,"name":"local-listen-timeout","map":"arena7","cl_timeout_seconds":1,"pause_wait_ms":2500,"progress_wait_ms":500}
if not manifest or manifest[0]!=scenario:raise SystemExit("FAIL local-timeout scenario")
if [x.get("role") for x in manifest[1:-1]] != ["gui","current-archive","content-archive","harness","active-cfg","bootstrap-cfg"]:raise SystemExit("FAIL local-timeout provenance roles")
for item in manifest[1:-1]:
 if set(item)!={"kind","role","path","bytes","sha256"} or item.get("kind")!="provenance" or not isinstance(item.get("bytes"),int) or item["bytes"]<=0 or re.fullmatch(r"[0-9a-f]{64}",str(item.get("sha256",""))) is None:raise SystemExit("FAIL local-timeout provenance schema")
 data=open(item["path"],"rb").read()
 if len(data)!=item["bytes"] or hashlib.sha256(data).hexdigest()!=item["sha256"]:raise SystemExit("FAIL local-timeout provenance rehash")
result=manifest[-1]
if set(result)!={"kind","controller_pid","rc","timeout","forced"} or result.get("kind")!="result" or not isinstance(result.get("controller_pid"),int) or result["controller_pid"]<=0 or result.get("rc")!=0 or result.get("timeout") is not False or result.get("forced") is not False:raise SystemExit("FAIL local-timeout result")
print("PASS local listen-server pause exceeds cl_timeout and resumes same active match")
PYEOF
}

write_self() {
python3 - "$1" "$2" "$3" "$4" <<'PYEOF'
import datetime,hashlib,json,os,sys
log_path,game_path,manifest_path,mode=sys.argv[1:]
base=datetime.datetime(2026,8,12,tzinfo=datetime.timezone.utc);clock=0
def row(sev,cat,msg,advance=10):
 global clock
 stamp=(base+datetime.timedelta(milliseconds=clock)).isoformat(timespec="milliseconds").replace("+00:00","Z");clock+=advance
 return {"ts":stamp,"sev":sev,"cat":cat,"msg":msg+"\n"}
R=[row("INFO","network","WiredNet: listening on port 30123 (IPv4), ALPN: q3now, max clients: 64, cert=memory key=memory"),row("INFO","server","Server: arena7"),row("DEBUG","ui","WiredUI: close all postcondition depth=0 active=none catcher_ui=0 paused=0"),row("DEBUG","network","in-mem game: allocated conn slot 0 (handle 110) for app 0"),row("INFO","network","in-mem: app 0 connected (client handle 100, server handle 110)"),row("INFO","game","InitGame: \\mapname\\arena7\\protocol\\74"),row("DEBUG","server","SV_OnPlayerConnect: conn=110"),row("INFO","game","ClientConnect: 0"),row("DEBUG","server","SV_OnPlayerConnect: slot 0 assigned to conn=110 (loopback)"),row("INFO","game","ClientBegin: 0"),row("INFO","client","cls.state: -> CA_ACTIVE (FIRST GAMEPLAY FRAME mapname=maps/arena7.bsp serverTime=100 numEntities=2 framecount=3)"),row("INFO","system","Q0_LOCAL_TIMEOUT_PRE"),row("DEBUG","ui","WiredUI: ingame state active=1 demo=0 catcher_ui=0 paused=0 connection_present=1 address=loopback server_time=100 top=none depth=0 focused=none"),row("INFO","system",'"cl_timeout" is:"1^7"'),row("INFO","system",' default:"200^7"'),row("DEBUG","ui","WiredUI: SetActiveMenu 2 (depth 0)"),row("DEBUG","ui","wui_test_keydown: dispatched keycode 27"),row("INFO","system","Q0_LOCAL_TIMEOUT_PAUSE_BEGIN"),row("DEBUG","ui","WiredUI: ingame state active=1 demo=0 catcher_ui=1 paused=1 connection_present=1 address=loopback server_time=110 top=ingame depth=0 focused=none"),row("INFO","system",'"cl_paused" is:"1^7"'),row("INFO","system",'"sv_paused" is:"1^7"',2500),row("INFO","system","Q0_LOCAL_TIMEOUT_PAUSE_END"),row("DEBUG","ui","WiredUI: ingame state active=1 demo=0 catcher_ui=1 paused=1 connection_present=1 address=loopback server_time=110 top=ingame depth=0 focused=none"),row("INFO","system",'"cl_paused" is:"1^7"'),row("INFO","system",'"sv_paused" is:"1^7"'),row("DEBUG","ui","wui_menu_nav focus: focused item 'btn_resume' (top index -1)"),row("DEBUG","ui","wui_menu_nav: K_ENTER dispatched"),row("INFO","system","Q0_LOCAL_TIMEOUT_RESUMED"),row("DEBUG","ui","WiredUI: ingame state active=1 demo=0 catcher_ui=0 paused=0 connection_present=1 address=loopback server_time=110 top=none depth=0 focused=none"),row("INFO","system",'"cl_paused" is:"0^7"'),row("INFO","system",'"sv_paused" is:"1^7"'),row("INFO","game","say: ListenWitness: Q0_LISTEN_RESUME_ROUNDTRIP"),row("INFO","cgame","ListenWitness: Q0_LISTEN_RESUME_ROUNDTRIP",500),row("INFO","system","Q0_LOCAL_TIMEOUT_COMPLETE"),row("DEBUG","ui","WiredUI: ingame state active=1 demo=0 catcher_ui=0 paused=0 connection_present=1 address=loopback server_time=610 top=none depth=0 focused=none"),row("INFO","system","Q0_LOCAL_TIMEOUT_QUIT_REQUESTED"),row("INFO","server","----- Server Shutdown (Server quit) -----"),row("INFO","game","==== ShutdownGame ====")]
G=["  0:00 InitGame: \\mapname\\arena7\\protocol\\74","  0:01 ClientConnect: 0","  0:02 ClientBegin: 0","  0:05 say: ListenWitness: Q0_LISTEN_RESUME_ROUNDTRIP","  0:06 ShutdownGame:"]
R=[item for item in R if not (item["msg"].startswith("ClientConnect: ") or item["msg"].startswith("ClientBegin: ") or item["msg"].startswith("say: "))]
def find(text,n=0):return [i for i,x in enumerate(R) if text in x["msg"]][n]
if mode=="timeout":R.insert(-2,row("INFO","client","Server connection timed out."))
elif mode=="reconnect":G.insert(2,"  0:01 ClientConnect: 1")
elif mode=="address":R[find("Q0_LOCAL_TIMEOUT_PAUSE_END")+1]["msg"]=R[find("Q0_LOCAL_TIMEOUT_PAUSE_END")+1]["msg"].replace("loopback","127.0.0.1:30123")
elif mode=="pause-progress":R[find("Q0_LOCAL_TIMEOUT_PAUSE_END")+1]["msg"]=R[find("Q0_LOCAL_TIMEOUT_PAUSE_END")+1]["msg"].replace("server_time=110","server_time=111")
elif mode=="no-progress":R[find("Q0_LOCAL_TIMEOUT_COMPLETE")+1]["msg"]=R[find("Q0_LOCAL_TIMEOUT_COMPLETE")+1]["msg"].replace("server_time=610","server_time=110")
elif mode=="missing-pause":R.pop(find("Q0_LOCAL_TIMEOUT_PAUSE_BEGIN"))
elif mode=="wrong-state":R[find("Q0_LOCAL_TIMEOUT_PAUSE_BEGIN")+1]["msg"]=R[find("Q0_LOCAL_TIMEOUT_PAUSE_BEGIN")+1]["msg"].replace("paused=1","paused=0")
elif mode=="order":a=find("Q0_LOCAL_TIMEOUT_PAUSE_END");b=find("WiredUI: ingame state",2);R[a],R[b]=R[b],R[a]
elif mode=="slot":G[2]="  0:02 ClientBegin: 1"
elif mode=="suffix":R[find("Q0_LOCAL_TIMEOUT_COMPLETE")]["msg"]="Q0_LOCAL_TIMEOUT_COMPLETE extra\n"
elif mode=="metadata":R[find("Q0_LOCAL_TIMEOUT_PAUSE_BEGIN")]["cat"]="client"
elif mode=="smuggle":R.append(row("INFO","system","benign\rWiredUI: ingame state active=1 demo=0 catcher_ui=0 paused=0 connection_present=1 address=loopback server_time=999 top=none depth=0 focused=none"))
elif mode=="error":R.append(row("ERROR","system","synthetic failure"))
elif mode=="transport":R[find("in-mem: app")]["msg"]="in-mem: app 1 connected (client handle 101, server handle 111)\n"
elif mode=="allocation":R[find("in-mem game:")]["msg"]="in-mem game: allocated conn slot 1 (handle 111) for app 0\n"
elif mode=="timeout-cvar":R[find('"cl_timeout"')]["msg"]='"cl_timeout" is:"2^7"\n'
elif mode=="timeout-combined":R[find('"cl_timeout"')]["msg"]='"cl_timeout" is:"1^7" default:"200^7"\n'
elif mode=="timeout-additive":R.insert(find('"cl_timeout"')+1,dict(R[find('"cl_timeout"')]))
elif mode=="pause-cvar":R[find('"sv_paused"')]["msg"]='"sv_paused" is:"0^7"\n'
elif mode=="short-wall":R[find("Q0_LOCAL_TIMEOUT_PAUSE_END")]["ts"]=(datetime.datetime.fromisoformat(R[find("Q0_LOCAL_TIMEOUT_PAUSE_BEGIN")]["ts"].replace("Z","+00:00"))+datetime.timedelta(milliseconds=1000)).isoformat(timespec="milliseconds").replace("+00:00","Z")
elif mode=="accept":
 at=find("Q0_LOCAL_TIMEOUT_PRE");item=row("DEBUG","network","QUIC client: TLV ACCEPT received");item["ts"]=R[at]["ts"];R.insert(at,item)
elif mode=="pop":
 at=find("Q0_LOCAL_TIMEOUT_RESUMED");item=row("DEBUG","ui","WiredUI: pop menu (depth 0)");item["ts"]=R[at]["ts"];R.insert(at,item)
elif mode=="closeall-late":
 item=R.pop(find("WiredUI: close all "));at=find("Q0_LOCAL_TIMEOUT_RESUMED");item["ts"]=R[at]["ts"];R.insert(at,item)
elif mode=="closeall-extra":R.insert(find("Q0_LOCAL_TIMEOUT_PRE"),dict(R[find("WiredUI: close all ")]))
elif mode=="roundtrip-missing":R.pop(find("ListenWitness:"))
elif mode=="roundtrip-order":a=find("Q0_LOCAL_TIMEOUT_RESUMED");b=find("ListenWitness:");R[a],R[b]=R[b],R[a]
elif mode=="roundtrip-metadata":R[find("ListenWitness:")]["cat"]="client"
elif mode=="roundtrip-body":R[find("ListenWitness:")]["msg"]="ListenWitness: Q0_LISTEN_OTHER\n"
elif mode=="prequit-disconnect":
 at=find("Q0_LOCAL_TIMEOUT_COMPLETE");item=row("DEBUG","network","WiredNet game: freed conn slot 0");item["ts"]=R[at]["ts"];R.insert(at,item)
elif mode=="timestamp-backward":R[find("Q0_LOCAL_TIMEOUT_COMPLETE")]["ts"]="2026-08-11T00:00:00.000Z"
elif mode=="game-missing":G.pop(1)
elif mode=="game-extra":G.insert(2,"  0:01 ClientConnect: 0")
elif mode=="game-order":G[1],G[2]=G[2],G[1]
elif mode=="game-say":G[3]="  0:05 say: Other: Q0_LISTEN_RESUME_ROUNDTRIP"
with open(log_path,"w") as out:
 for item in R:out.write(json.dumps(item)+"\n")
with open(game_path,"w") as out:
 for item in G:out.write(item+"\n")
roles=("gui","current-archive","content-archive","harness","active-cfg","bootstrap-cfg")
M=[{"kind":"scenario","schema":1,"name":"local-listen-timeout","map":"arena7","cl_timeout_seconds":1,"pause_wait_ms":2500,"progress_wait_ms":500}]
for role in roles:
 path=manifest_path+"."+role;data=("fixture-"+role).encode();open(path,"wb").write(data);M.append({"kind":"provenance","role":role,"path":os.path.abspath(path),"bytes":len(data),"sha256":hashlib.sha256(data).hexdigest()})
M.append({"kind":"result","controller_pid":101,"rc":0,"timeout":False,"forced":False})
if mode=="manifest":M[2]["sha256"]="0"*64
elif mode=="result":M[-1]["rc"]=1
with open(manifest_path,"w") as out:
 for item in M:out.write(json.dumps(item,sort_keys=True)+"\n")
PYEOF
}

if [ "${1:-}" = --analyze ]; then
 [ "$#" -eq 5 ] || { echo "usage: $0 --analyze <qconsole.jsonl> <games.log> <manifest.jsonl> <port>";exit 64; }
 analyze_contract "$2" "$3" "$4" "$5";exit $?
fi
if [ "${1:-}" = --self-test ]; then
 ROOT="$(mktemp -d -t local-timeout-self-XXXXXX 2>/dev/null || mktemp -d)";trap 'rm -rf "$ROOT"' EXIT
 write_self "$ROOT/log" "$ROOT/game" "$ROOT/manifest" clean || exit 1;analyze_contract "$ROOT/log" "$ROOT/game" "$ROOT/manifest" 30123 >/dev/null || exit 1
 defects=(timeout reconnect address pause-progress no-progress missing-pause wrong-state order slot suffix metadata smuggle error transport allocation timeout-cvar timeout-combined timeout-additive pause-cvar short-wall accept pop closeall-late closeall-extra roundtrip-missing roundtrip-order roundtrip-metadata roundtrip-body prequit-disconnect timestamp-backward game-missing game-extra game-order game-say manifest result)
 for defect in "${defects[@]}";do write_self "$ROOT/$defect.log" "$ROOT/$defect.game" "$ROOT/$defect.manifest" "$defect" || exit 1;if analyze_contract "$ROOT/$defect.log" "$ROOT/$defect.game" "$ROOT/$defect.manifest" 30123 >/dev/null 2>&1;then echo "FAIL accepted $defect";exit 1;fi;done
 echo "PASS local-listen-timeout analyzer self-test (${#defects[@]} mutations)";exit 0
fi

WIRED="${1:-}";[ -n "$WIRED" ] && [ -x "$WIRED" ] || { echo "usage: $0 /absolute/path/to/wired";exit 64; }
[ -f "$TIMEOUT_RUNNER" ] || { echo "SKIP: missing timeout runner";exit 77; }
WIRED="$(cd "$(dirname "$WIRED")" && pwd)/$(basename "$WIRED")";WD="$(dirname "$WIRED")"
PACK="$(wired_find_archive_root "$WD" "$WD/../Resources" "$WD/../../.." 2>/dev/null || true)";[ -n "$PACK" ] || { echo "SKIP: current VFS archives unavailable";exit 77; }
CONTENT="$(wired_find_archive_root "${WIRED_CONTENT_ROOT:-}" "$WIRED_HOME" "$PACK" 2>/dev/null || true)";[ -n "$CONTENT" ] || { echo "SKIP: set WIRED_CONTENT_ROOT";exit 77; }
CURRENT_ARCHIVE="$(wired_first_archive "$PACK/base")" || exit 77
CONTENT_ARCHIVE="$(wired_first_archive "$CONTENT/base")" || exit 77
ROOT="$(mktemp -d -t local-listen-timeout-XXXXXX 2>/dev/null || mktemp -d)";HOME_DIR="$ROOT/home/q3now-preview";FORCED=0
cleanup(){ local status=$?;trap - EXIT INT TERM;[ "${WIRED_KEEP_ARTIFACTS:-0}" = 1 ] || rm -rf "$ROOT";exit "$status";};trap cleanup EXIT;trap 'exit 130' INT;trap 'exit 143' TERM
mkdir -p "$HOME_DIR/base";wired_link_content_into_home "$HOME_DIR" "$CONTENT/base" "$PACK/base" || exit 1
PORT="$(python3 - <<'PYEOF'
import socket
s=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);s.bind(("127.0.0.1",0));print(s.getsockname()[1]);s.close()
PYEOF
)"
ACTIVE="$HOME_DIR/base/local-listen-timeout-active.cfg";BOOT="$HOME_DIR/base/local-listen-timeout.cfg"
cat >"$ACTIVE" <<'CFG'
echo Q0_LOCAL_TIMEOUT_PRE
wui_ingame_trace
cl_timeout
wui_test_keydown 27
wait 10
echo Q0_LOCAL_TIMEOUT_PAUSE_BEGIN
wui_ingame_trace
cl_paused
sv_paused
waitms 2500
echo Q0_LOCAL_TIMEOUT_PAUSE_END
wui_ingame_trace
cl_paused
sv_paused
wui_menu_nav focus btn_resume
wui_menu_nav enter
echo Q0_LOCAL_TIMEOUT_RESUMED
wui_ingame_trace
cl_paused
sv_paused
say Q0_LISTEN_RESUME_ROUNDTRIP
waitms 500
echo Q0_LOCAL_TIMEOUT_COMPLETE
wui_ingame_trace
echo Q0_LOCAL_TIMEOUT_QUIT_REQUESTED
quit
CFG
cat >"$BOOT" <<'CFG'
set activeAction "exec local-listen-timeout-active.cfg"
map arena7
CFG
MANIFEST="$ROOT/manifest.jsonl";python3 - "$MANIFEST" "$WIRED" "$CURRENT_ARCHIVE" "$CONTENT_ARCHIVE" "$0" "$ACTIVE" "$BOOT" <<'PYEOF'
import hashlib,json,os,sys
with open(sys.argv[1],"w") as out:
 out.write(json.dumps({"kind":"scenario","schema":1,"name":"local-listen-timeout","map":"arena7","cl_timeout_seconds":1,"pause_wait_ms":2500,"progress_wait_ms":500},sort_keys=True)+"\n")
 for role,path in zip(("gui","current-archive","content-archive","harness","active-cfg","bootstrap-cfg"),sys.argv[2:]):
  data=open(path,"rb").read();out.write(json.dumps({"kind":"provenance","role":role,"path":os.path.abspath(path),"bytes":len(data),"sha256":hashlib.sha256(data).hexdigest()},sort_keys=True)+"\n")
PYEOF
LOG="$HOME_DIR/qconsole.jsonl";GAMELOG="$HOME_DIR/base/games.log";STDOUT="$ROOT/stdout";case "$(uname -s)" in Darwin) PLATFORM=(-ApplePersistenceIgnoreState YES);;*) PLATFORM=();;esac
python3 "$TIMEOUT_RUNNER" --timeout 120 --kill-after 15 --cwd "$WD" --stdout "$STDOUT" -- "$WIRED" "${PLATFORM[@]}" +set fs_homepath "$HOME_DIR" +set com_automated 1 +set com_noHardReboot 1 +set name ListenWitness +set s_initsound 0 +set r_fullscreen 0 +set r_mode -1 +set r_customwidth 1280 +set r_customheight 720 +set net_ip 127.0.0.1 +set net_port "$PORT" +set cl_timeout 1 +set sv_pure 0 +set g_autoBots 0 +set g_minPlayers 0 +set log_severity DEBUG +set log_file_severity DEBUG +set log_file_mode overwrite_synced +exec local-listen-timeout.cfg &CONTROLLER_PID=$!;wait "$CONTROLLER_PID";RC=$?
python3 - "$MANIFEST" "$CONTROLLER_PID" "$RC" <<'PYEOF'
import json,sys
with open(sys.argv[1],"a") as out:out.write(json.dumps({"kind":"result","controller_pid":int(sys.argv[2]),"rc":int(sys.argv[3]),"timeout":int(sys.argv[3])==124,"forced":False},sort_keys=True)+"\n")
PYEOF
[ "$RC" -eq 0 ] && [ -s "$LOG" ] && [ -s "$GAMELOG" ] || { echo "FAIL local-listen-timeout process rc=$RC root=$ROOT";exit 1; }
analyze_contract "$LOG" "$GAMELOG" "$MANIFEST" "$PORT" || exit 1
echo "PASS local listen-server idle-timeout acceptance gate"
