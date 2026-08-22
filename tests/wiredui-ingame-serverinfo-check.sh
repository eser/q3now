#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors
# Active-match Server Info authority and implicit-root return-focus gate.

set -uo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
FIXTURE="$SCRIPT_DIR/wiredui-ingame-serverinfo-fixture.py"
TIMEOUT_RUNNER="$SCRIPT_DIR/run-with-timeout.py"

analyze() {
python3 - "$1" "$2" "$3" "$4" "$5" "$6" <<'PYEOF'
import json,re,sys
pp,sp,fp,lp,stale_port,active_port=sys.argv[1:]
stale=f"127.0.0.1:{stale_port}"; active=f"127.0.0.1:{active_port}"; ea=re.escape(active)
def load(path):
 rows=[]
 for n,line in enumerate(open(path,encoding="utf-8",errors="replace"),1):
  try:r=json.loads(line)
  except Exception as e:raise SystemExit(f"FAIL {path}:{n}: {e}")
  if not isinstance(r,dict):raise SystemExit(f"FAIL {path}:{n}: schema")
  rows.append(r)
 return rows
p,s,f,layout=map(load,(pp,sp,fp,lp))
if not p or not s or not f or not layout:raise SystemExit("FAIL empty evidence")
if any(not all(isinstance(r.get(k),str) for k in ("sev","cat","msg")) for r in p+s):raise SystemExit("FAIL log schema")
if any(r["sev"].upper() in ("ERROR","FATAL") for r in p+s):raise SystemExit("FAIL severity")
def norm(v):return v[:-1] if v.endswith("\n") and not v.endswith("\n\n") else v
vals=[norm(r["msg"]) for r in p]
claimed=("WiredUI: connected server status ","WiredUI: server status request ","WiredUI: server status state=", "WiredUI: server status loaded ","WiredUI: server status row=", "WiredUI: server status cancelled ","WiredUI: server fixture installed ","WiredUI: server selection ","WiredUI: ingame state ","WiredUI: SetActiveMenu ","WiredUI: push menu '","WiredUI: pop menu ","WiredUI: pointer phase=","wui_pointer_item:","wui_menu_nav focus:","wui_menu_nav: K_","wui_test_keydown:","Browser connect attempt armed ","WiredUI: started validated connect ","WiredUI: close all ","Connect failed ","cls.state: -> CA_ACTIVE ","QUIC client:")
for v in vals:
 lines=re.split(r"[\r\n]",v)
 if len(lines)>1 and any(line.startswith(claimed) or "FIRST GAMEPLAY FRAME" in line for line in lines):raise SystemExit("FAIL claimed multiline")
def exact(prefix,pats,sev="DEBUG",cat="ui"):
 hit=[(i,p[i],v) for i,v in enumerate(vals) if v.startswith(prefix)]
 if len(hit)!=len(pats):raise SystemExit(f"FAIL {prefix} cardinality {len(hit)}")
 for (_,r,v),pat in zip(hit,pats):
  if r["sev"].upper()!=sev or r["cat"].lower()!=cat or re.fullmatch(pat,v) is None:raise SystemExit(f"FAIL {prefix} metadata/body")
 return hit
openr=exact("WiredUI: connected server status open ",[rf"WiredUI: connected server status open owner_generation=1 address={ea}"])
owner="1"; first_generation="6"; retry_generation="7"
status_generations=(first_generation,retry_generation)
req=exact("WiredUI: connected server status request ",[rf"WiredUI: connected server status request generation={g} owner_generation={owner} address={ea}" for g in status_generations])
pending=exact("WiredUI: connected server status state=",[rf"WiredUI: connected server status state=pending generation={g} owner_generation={owner} address={ea} rows=1" for g in status_generations])
controls=exact("WiredUI: connected server status controls ",[r"WiredUI: connected server status controls back=0 connect=0 retry=1 close=1"])
loaded=exact("WiredUI: server status loaded ",[rf"WiredUI: server status loaded generation={g} selection_generation=-1 address={ea} rows=9" for g in status_generations])
row_patterns=[rf"WiredUI: server status row=0 key=Address value={ea}",r"WiredUI: server status row=1 key=Server value=ACTIVE B",r"WiredUI: server status row=2 key=Map value=arena7",r"WiredUI: server status row=3 key=Players value=1/[1-9][0-9]*",r"WiredUI: server status row=4 key=Game type value=0",r"WiredUI: server status row=5 key=Game value=q3now",r"WiredUI: server status row=6 key=Protocol value=74",r"WiredUI: server status row=7 key=Version value=.+",r"WiredUI: server status row=8 key=Player 1 value=.+"]
rows=exact("WiredUI: server status row=",row_patterns*2)
ready=exact("WiredUI: connected server status ready ",[rf"WiredUI: connected server status ready generation={g} owner_generation={owner} address={ea} rows=9" for g in status_generations])
retry=exact("WiredUI: connected server status retry ",[rf"WiredUI: connected server status retry owner_generation={owner} prior_generation={first_generation} address={ea}"])
cancel=exact("WiredUI: connected server status cancel ",[rf"WiredUI: connected server status cancel owner_generation={owner} generation={retry_generation} address={ea}"])
cancelled_all=exact("WiredUI: server status cancelled ",[rf"WiredUI: server status cancelled generation={n} rows=0" for n in (1,2,3,4,5,8,9,10)])
setactive=exact("WiredUI: SetActiveMenu ",[r"WiredUI: SetActiveMenu 2 \(depth 0\)"])
push=exact("WiredUI: push menu '",[r"WiredUI: push menu 'servers' \(depth 1\)",r"WiredUI: push menu 'serverinfo' \(depth 1\)"])
fixture_install=exact("WiredUI: server fixture installed ",[rf"WiredUI: server fixture installed sentinel=127\.0\.0\.1:[1-9][0-9]* target={re.escape(stale)} raw_order=target,sentinel"])
selection=exact("WiredUI: server selection ",[rf"WiredUI: server selection display_row=0 raw=1 source=0 list_generation=([1-9][0-9]*) selection_generation=([1-9][0-9]*) address=127\.0\.0\.1:[1-9][0-9]* name=A0 WIRED Q0 SENTINEL map=arena1",rf"WiredUI: server selection display_row=1 raw=0 source=0 list_generation=([1-9][0-9]*) selection_generation=([1-9][0-9]*) address={re.escape(stale)} name=Z0 WIRED Q0 TARGET map=arena7"])
pop=exact("WiredUI: pop menu ",[r"WiredUI: pop menu \(depth 0\)"]*2)
exact("wui_menu_nav focus:",[r"wui_menu_nav focus: focused item 'serverlist' \(top index -1\)"])
nav=exact("wui_menu_nav: K_",[r"wui_menu_nav: K_DOWNARROW dispatched",r"wui_menu_nav: K_ESCAPE dispatched"]+[r"wui_menu_nav: K_DOWNARROW dispatched"]*10+[r"wui_menu_nav: K_ENTER dispatched"]+[r"wui_menu_nav: K_UPARROW dispatched"]*9+[r"wui_menu_nav: K_ENTER dispatched"])
hidden=exact("wui_pointer_item:",[r"wui_pointer_item: 'btn_back' is not an interactive item",r"wui_pointer_item: 'btn_connect' is not an interactive item"],"WARN","ui")
pointer=exact("WiredUI: pointer phase=",[r"WiredUI: pointer phase=release reason=reload was_down=0 pointer_down=0",r"WiredUI: pointer phase=release reason=shutdown was_down=0 pointer_down=0",r"WiredUI: pointer phase=release reason=reload was_down=0 pointer_down=0",r"WiredUI: pointer phase=release reason=push was_down=0 pointer_down=0",r"WiredUI: pointer phase=release reason=pop was_down=0 pointer_down=0",r"WiredUI: pointer phase=release reason=push was_down=0 pointer_down=0",r"WiredUI: pointer phase=moved ingress=CL_MouseEvent menu=serverinfo item=btn_retry x=([0-9]+) y=([0-9]+)",r"WiredUI: pointer phase=click ingress=CL_KeyEvent key=K_MOUSE1 x=([0-9]+) y=([0-9]+)",r"WiredUI: pointer phase=moved ingress=CL_MouseEvent menu=serverinfo item=btn_close x=([0-9]+) y=([0-9]+)",r"WiredUI: pointer phase=click ingress=CL_KeyEvent key=K_MOUSE1 x=([0-9]+) y=([0-9]+)",r"WiredUI: pointer phase=release reason=pop was_down=1 pointer_down=0",r"WiredUI: pointer phase=release reason=pop was_down=0 pointer_down=0",r"WiredUI: pointer phase=release reason=shutdown was_down=0 pointer_down=0"])
for moved,clicked in ((pointer[6],pointer[7]),(pointer[8],pointer[9])):
 mm=re.search(r"x=([0-9]+) y=([0-9]+)",moved[2]);cm=re.search(r"x=([0-9]+) y=([0-9]+)",clicked[2])
 if mm.groups()!=cm.groups():raise SystemExit("FAIL pointer move/click identity")
trace=exact("WiredUI: ingame state ",[rf"WiredUI: ingame state active=1 demo=0 catcher_ui=1 paused=1 connection_present=1 address={ea} server_time=([0-9]+) top=ingame depth=0 focused=none",rf"WiredUI: ingame state active=1 demo=0 catcher_ui=1 paused=1 connection_present=1 address={ea} server_time=([0-9]+) top=ingame depth=0 focused=btn_serverinfo",rf"WiredUI: ingame state active=1 demo=0 catcher_ui=1 paused=1 connection_present=1 address={ea} server_time=([0-9]+) top=serverinfo depth=1 focused=(none|statuslist|btn_retry|btn_close)",rf"WiredUI: ingame state active=1 demo=0 catcher_ui=1 paused=1 connection_present=1 address={ea} server_time=([0-9]+) top=ingame depth=0 focused=btn_serverinfo",rf"WiredUI: ingame state active=1 demo=0 catcher_ui=1 paused=1 connection_present=1 address={ea} server_time=([0-9]+) top=ingame depth=0 focused=btn_resume",rf"WiredUI: ingame state active=1 demo=0 catcher_ui=0 paused=0 connection_present=1 address={ea} server_time=([0-9]+) top=none depth=0 focused=none",rf"WiredUI: ingame state active=1 demo=0 catcher_ui=0 paused=0 connection_present=1 address={ea} server_time=([0-9]+) top=none depth=0 focused=none"])
times=[]
for _,_,v in trace:times.append(int(re.search(r"server_time=([0-9]+)",v).group(1)))
if times[-1] <= times[-2]:raise SystemExit("FAIL server time did not progress after resume")
first=exact("cls.state: -> CA_ACTIVE ",[r"cls\.state: -> CA_ACTIVE \(FIRST GAMEPLAY FRAME mapname=maps/arena7\.bsp serverTime=[0-9]+ numEntities=[0-9]+ framecount=[0-9]+\)"],"INFO","client")
accept=exact("QUIC client: TLV ACCEPT received",[r"QUIC client: TLV ACCEPT received"],"DEBUG","network")
if not accept[0][0]<first[0][0]<setactive[0][0]<trace[0][0]<fixture_install[0][0]<push[0][0]<selection[0][0]<selection[1][0]<pop[0][0]<trace[1][0]<openr[0][0]<req[0][0]<pending[0][0]<controls[0][0]<push[1][0]<loaded[0][0]<rows[8][0]<ready[0][0]<hidden[0][0]<hidden[1][0]<pointer[6][0]<pointer[7][0]<retry[0][0]<req[1][0]<pending[1][0]<loaded[1][0]<rows[-1][0]<ready[1][0]<trace[2][0]<pointer[8][0]<pointer[9][0]<cancel[0][0]<cancelled_all[5][0]<pop[1][0]<trace[3][0]<trace[4][0]<trace[5][0]<trace[6][0]:raise SystemExit("FAIL causal order")
for forbidden in ("WiredUI: server status request ","WiredUI: server status failed ","WiredUI: server status refused ","WiredUI: server status dispose ","WiredUI: server status snapshot ","WiredUI: connected server status invalidated ","WiredUI: connected server status refused ","WiredUI: connected server status retry refused ","Browser connect attempt armed ","WiredUI: started validated connect ","WiredUI: close all ","Connect failed "):
 if any(v.startswith(forbidden) for v in vals):raise SystemExit(f"FAIL forbidden {forbidden}")
if any(stale in v for v in vals if v.startswith(("WiredUI: connected server status ","WiredUI: server status loaded ","WiredUI: server status row="))):raise SystemExit("FAIL stale browser authority")
if f[0]!={"kind":"ready","elapsed_ms":0,"address":stale,"port":int(stale_port)}:raise SystemExit("FAIL stale fixture ready")
if set(f[1])!={"kind","elapsed_ms","reason"} or f[1].get("kind")!="stopped" or f[1].get("reason")!="signal" or not isinstance(f[1].get("elapsed_ms"),int) or f[1]["elapsed_ms"]<=0:raise SystemExit("FAIL stale fixture stopped")
serverinfo=[r for r in layout if r.get("menu")=="serverinfo"]
for region in ("serverinfo","statuslist","btn_retry","btn_close"):
 hit=[r for r in serverinfo if r.get("region")==region]
 if not hit or any(r.get("w",0)<=0 or r.get("h",0)<=0 for r in hit):raise SystemExit(f"FAIL layout {region}")
sv=[norm(r["msg"]) for r in s]
server_claimed=("WiredNet: listening on port ","Server: ","SV_OnPlayerConnect: conn=","SV_OnPlayerConnect: slot ","ClientConnect: ","ClientBegin: ","Q0_INGAME_INFO_PHASE_COMPLETE","Q0_INGAME_INFO_QUIT_REQUESTED","----- Server Shutdown ","QUIC transport shut down")
for v in sv:
 lines=re.split(r"[\r\n]",v)
 if len(lines)>1 and any(line.startswith(server_claimed) for line in lines):raise SystemExit("FAIL server claimed multiline")
def sexact(prefix,pats,sev,cat):
 hit=[(i,s[i],v) for i,v in enumerate(sv) if v.startswith(prefix)]
 if len(hit)!=len(pats):raise SystemExit(f"FAIL server {prefix} cardinality {len(hit)}")
 for (_,r,v),pat in zip(hit,pats):
  if r["sev"].upper()!=sev or r["cat"].lower()!=cat or re.fullmatch(pat,v) is None:raise SystemExit(f"FAIL server {prefix} metadata/body")
 return hit
listen=sexact("WiredNet: listening on port ",[rf"WiredNet: listening on port {active_port} \(IPv4\).*"],"INFO","network")
arena=sexact("Server: ",[r"Server: arena7"],"INFO","server")
svconnect=sexact("SV_OnPlayerConnect: conn=",[r"SV_OnPlayerConnect: conn=1"],"DEBUG","server")
clientconnect=sexact("ClientConnect: ",[r"ClientConnect: 1"],"INFO","game")
assigned=sexact("SV_OnPlayerConnect: slot ",[r"SV_OnPlayerConnect: slot 1 assigned to conn=1 \(.*\)"],"DEBUG","server")
clientbegin=sexact("ClientBegin: ",[r"ClientBegin: 1"],"INFO","game")
boundary=sexact("Q0_INGAME_INFO_PHASE_COMPLETE",[r"Q0_INGAME_INFO_PHASE_COMPLETE"],"INFO","system")
quitreq=sexact("Q0_INGAME_INFO_QUIT_REQUESTED",[r"Q0_INGAME_INFO_QUIT_REQUESTED"],"INFO","system")
shutdown=sexact("----- Server Shutdown ",[r"----- Server Shutdown \(Server quit\) -----"],"INFO","server")
transport=sexact("QUIC transport shut down",[r"QUIC transport shut down\."],"INFO","network")
if not listen[0][0]<arena[0][0]<svconnect[0][0]<clientconnect[0][0]<assigned[0][0]<clientbegin[0][0]<boundary[0][0]<quitreq[0][0]<shutdown[0][0]<transport[0][0]:raise SystemExit("FAIL active server causal lifecycle")
print("PASS in-game Server Info: exact active B authority and implicit-root pause/focus/resume")
PYEOF
}

write_self() {
python3 - "$1" "$2" "$3" "$4" "$5" <<'PYEOF'
import json,sys
pp,sp,fp,lp,mode=sys.argv[1:];a="127.0.0.1:30102";st="127.0.0.1:30101";P=[]
def add(msg,sev="DEBUG",cat="ui"):P.append({"sev":sev,"cat":cat,"msg":msg})
add("WiredUI: server status cancelled generation=1 rows=0");add("WiredUI: pointer phase=release reason=reload was_down=0 pointer_down=0");add("QUIC client: TLV ACCEPT received","DEBUG","network");add("WiredUI: pointer phase=release reason=shutdown was_down=0 pointer_down=0");add("WiredUI: server status cancelled generation=2 rows=0");add("WiredUI: server status cancelled generation=3 rows=0");add("WiredUI: pointer phase=release reason=reload was_down=0 pointer_down=0");add("cls.state: -> CA_ACTIVE (FIRST GAMEPLAY FRAME mapname=maps/arena7.bsp serverTime=100 numEntities=12 framecount=1)","INFO","client");add("WiredUI: SetActiveMenu 2 (depth 0)");
def state(t,top,depth,focus,catch=1,pause=1):add(f"WiredUI: ingame state active=1 demo=0 catcher_ui={catch} paused={pause} connection_present=1 address={a} server_time={t} top={top} depth={depth} focused={focus}")
state(110,"ingame",0,"none")
add("WiredUI: server fixture installed sentinel=127.0.0.1:30103 target="+st+" raw_order=target,sentinel");add("WiredUI: pointer phase=release reason=push was_down=0 pointer_down=0");add("WiredUI: push menu 'servers' (depth 1)");add("WiredUI: server status cancelled generation=4 rows=0");add("WiredUI: server selection display_row=0 raw=1 source=0 list_generation=3 selection_generation=3 address=127.0.0.1:30103 name=A0 WIRED Q0 SENTINEL map=arena1");add("wui_menu_nav focus: focused item 'serverlist' (top index -1)");add("WiredUI: server status cancelled generation=5 rows=0");add("WiredUI: server selection display_row=1 raw=0 source=0 list_generation=3 selection_generation=4 address="+st+" name=Z0 WIRED Q0 TARGET map=arena7");add("wui_menu_nav: K_DOWNARROW dispatched");add("WiredUI: pointer phase=release reason=pop was_down=0 pointer_down=0");add("WiredUI: pop menu (depth 0)");add("wui_menu_nav: K_ESCAPE dispatched")
for _ in range(10):add("wui_menu_nav: K_DOWNARROW dispatched")
status_values=["Address value="+a,"Server value=ACTIVE B","Map value=arena7","Players value=1/8","Game type value=0","Game value=q3now","Protocol value=74","Version value=test","Player 1 value=Human — score 0, ping 0"]
def emit_rows():
 for i,x in enumerate(status_values):add(f"WiredUI: server status row={i} key={x}")
state(120,"ingame",0,"btn_serverinfo");add("WiredUI: connected server status open owner_generation=1 address="+a);add("WiredUI: connected server status request generation=6 owner_generation=1 address="+a);add("WiredUI: connected server status state=pending generation=6 owner_generation=1 address="+a+" rows=1");add("WiredUI: connected server status controls back=0 connect=0 retry=1 close=1");add("WiredUI: pointer phase=release reason=push was_down=0 pointer_down=0");add("WiredUI: push menu 'serverinfo' (depth 1)");add("wui_menu_nav: K_ENTER dispatched");add("WiredUI: server status loaded generation=6 selection_generation=-1 address="+a+" rows=9");emit_rows();add("WiredUI: connected server status ready generation=6 owner_generation=1 address="+a+" rows=9");add("wui_pointer_item: 'btn_back' is not an interactive item","WARN","ui");add("wui_pointer_item: 'btn_connect' is not an interactive item","WARN","ui");add("WiredUI: pointer phase=moved ingress=CL_MouseEvent menu=serverinfo item=btn_retry x=500 y=650");add("WiredUI: pointer phase=click ingress=CL_KeyEvent key=K_MOUSE1 x=500 y=650");add("WiredUI: connected server status retry owner_generation=1 prior_generation=6 address="+a);add("WiredUI: connected server status request generation=7 owner_generation=1 address="+a);add("WiredUI: connected server status state=pending generation=7 owner_generation=1 address="+a+" rows=1");add("WiredUI: server status loaded generation=7 selection_generation=-1 address="+a+" rows=9");emit_rows();add("WiredUI: connected server status ready generation=7 owner_generation=1 address="+a+" rows=9");state(130,"serverinfo",1,"btn_retry");add("WiredUI: pointer phase=moved ingress=CL_MouseEvent menu=serverinfo item=btn_close x=700 y=650");add("WiredUI: pointer phase=click ingress=CL_KeyEvent key=K_MOUSE1 x=700 y=650");add("WiredUI: connected server status cancel owner_generation=1 generation=7 address="+a);add("WiredUI: server status cancelled generation=8 rows=0");add("WiredUI: pointer phase=release reason=pop was_down=1 pointer_down=0");add("WiredUI: pop menu (depth 0)");state(140,"ingame",0,"btn_serverinfo")
for _ in range(9):add("wui_menu_nav: K_UPARROW dispatched")
state(150,"ingame",0,"btn_resume");add("WiredUI: pointer phase=release reason=pop was_down=0 pointer_down=0");add("wui_menu_nav: K_ENTER dispatched");state(150,"none",0,"none",0,0);state(180,"none",0,"none",0,0);add("WiredUI: server status cancelled generation=9 rows=0");add("WiredUI: server status cancelled generation=10 rows=0");add("WiredUI: pointer phase=release reason=shutdown was_down=0 pointer_down=0")
S=[{"sev":"INFO","cat":"network","msg":"WiredNet: listening on port 30102 (IPv4), ALPN: q3now"},{"sev":"INFO","cat":"server","msg":"Server: arena7"},{"sev":"DEBUG","cat":"server","msg":"SV_OnPlayerConnect: conn=1"},{"sev":"INFO","cat":"game","msg":"ClientConnect: 1"},{"sev":"DEBUG","cat":"server","msg":"SV_OnPlayerConnect: slot 1 assigned to conn=1 ()"},{"sev":"INFO","cat":"game","msg":"ClientBegin: 1"},{"sev":"INFO","cat":"system","msg":"Q0_INGAME_INFO_PHASE_COMPLETE"},{"sev":"INFO","cat":"system","msg":"Q0_INGAME_INFO_QUIT_REQUESTED"},{"sev":"INFO","cat":"server","msg":"----- Server Shutdown (Server quit) -----"},{"sev":"INFO","cat":"network","msg":"QUIC transport shut down."}]
F=[{"kind":"ready","elapsed_ms":0,"address":st,"port":30101},{"kind":"stopped","elapsed_ms":50,"reason":"signal"}]
L=[]
for frame in (1,2):
 for region,w,h in (("serverinfo",1280,720),("statuslist",600,400),("btn_retry",80,30),("btn_close",80,30)):L.append({"menu":"serverinfo","frame":frame,"region":region,"x":10,"y":10,"w":w,"h":h,"focused":region=="statuslist"})
if mode=="stale":
 next(r for r in P if r["msg"].startswith("WiredUI: connected server status open "))["msg"]=next(r for r in P if r["msg"].startswith("WiredUI: connected server status open "))["msg"].replace(a,st)
elif mode=="browser":P.insert(20,{"sev":"DEBUG","cat":"ui","msg":"WiredUI: server status request generation=7 selection_generation=9 address="+st})
elif mode=="connect":P.insert(-1,{"sev":"DEBUG","cat":"ui","msg":"Browser connect attempt armed target="+a+" selection_generation=9 credential_present=0"})
elif mode=="wrong_identity":
 next(r for r in P if r["msg"].startswith("WiredUI: connected server status request "))["msg"]=next(r for r in P if r["msg"].startswith("WiredUI: connected server status request "))["msg"].replace("owner_generation=1","owner_generation=2")
elif mode=="no_return":P=[r for r in P if "focused=btn_serverinfo" not in r["msg"]]
elif mode=="fixture_hit":F.insert(1,{"kind":"unexpected","elapsed_ms":25,"peer":"127.0.0.1:9","length":4,"sha256":"0"*64,"hex":"00000000"})
elif mode=="browser_control":
 next(r for r in P if r["msg"].startswith("WiredUI: connected server status controls "))["msg"]=next(r for r in P if r["msg"].startswith("WiredUI: connected server status controls "))["msg"].replace("connect=0","connect=1")
elif mode=="no_progress":
 [r for r in P if r["msg"].startswith("WiredUI: ingame state ")][-1]["msg"]=[r for r in P if r["msg"].startswith("WiredUI: ingame state ")][-1]["msg"].replace("server_time=180","server_time=150")
elif mode=="multiline":P.insert(2,{"sev":"INFO","cat":"client","msg":"benign\nWiredUI: connected server status open owner_generation=99 address="+st})
elif mode=="hidden_back":P=[r for r in P if r["msg"]!="wui_pointer_item: 'btn_back' is not an interactive item"]
elif mode=="retry_pointer":P=[r for r in P if not (r["msg"].startswith("WiredUI: pointer phase=moved") and "item=btn_retry" in r["msg"])]
elif mode=="close_pointer":P=[r for r in P if not (r["msg"].startswith("WiredUI: pointer phase=click") and "x=700 " in r["msg"])]
elif mode=="pointer_coords":
 [r for r in P if r["msg"].startswith("WiredUI: pointer phase=click") and "x=500 " in r["msg"]][0]["msg"]=[r for r in P if r["msg"].startswith("WiredUI: pointer phase=click") and "x=500 " in r["msg"]][0]["msg"].replace("x=500","x=1")
elif mode=="refused":P.insert(-3,{"sev":"WARN","cat":"ui","msg":"WiredUI: connected server status refused without active network connection"})
elif mode=="status_failed":P.insert(-3,{"sev":"DEBUG","cat":"ui","msg":"WiredUI: server status failed generation=7 selection_generation=-1 address="+a+" state=malformed reason=forged rows=2"})
elif mode=="status_dispose":P.insert(-3,{"sev":"DEBUG","cat":"ui","msg":"WiredUI: server status dispose reason=forged prior_state=ready generation=7 selection_generation=-1 address="+a+" rows=9"})
elif mode=="first_metadata":
 next(r for r in P if r["msg"].startswith("cls.state: -> CA_ACTIVE "))["cat"]="ui"
elif mode=="server_shutdown":S=[r for r in S if not r["msg"].startswith("----- Server Shutdown ")]
for path,rows in ((pp,P),(sp,S),(fp,F),(lp,L)):
 with open(path,"w",encoding="utf-8") as out:
  for row in rows:out.write(json.dumps(row)+"\n")
PYEOF
}

if [ "${1:-}" = "--self-test" ]; then
 root="$(mktemp -d -t wired-ingame-info-self-XXXXXX 2>/dev/null || mktemp -d)"; trap 'rm -rf "$root"' EXIT INT TERM
 write_self "$root/p" "$root/s" "$root/f" "$root/l" clean
 analyze "$root/p" "$root/s" "$root/f" "$root/l" 30101 30102 >/dev/null || { echo "FAIL clean fixture"; exit 1; }
 n=0
 for defect in stale browser connect wrong_identity no_return fixture_hit browser_control no_progress multiline hidden_back retry_pointer close_pointer pointer_coords refused status_failed status_dispose first_metadata server_shutdown; do
  mkdir -p "$root/$defect"; write_self "$root/$defect/p" "$root/$defect/s" "$root/$defect/f" "$root/$defect/l" "$defect"
  if analyze "$root/$defect/p" "$root/$defect/s" "$root/$defect/f" "$root/$defect/l" 30101 30102 >/dev/null 2>&1; then echo "FAIL accepted $defect"; exit 1; fi
  n=$((n+1)); echo "  PASS rejects $defect"
 done
 echo "PASS in-game Server Info analyzer self-test ($n mutations)"; exit 0
fi

if [ "${1:-}" = "--analyze" ]; then
 [ "$#" -eq 7 ] || { echo "usage: $0 --analyze <product.jsonl> <server.jsonl> <fixture.jsonl> <layout.jsonl> <stale-port> <active-port>"; exit 2; }
 analyze "$2" "$3" "$4" "$5" "$6" "$7"
 exit $?
fi

command -v python3 >/dev/null 2>&1 && [ -f "$FIXTURE" ] && [ -f "$TIMEOUT_RUNNER" ] || { echo "SKIP: Python fixture/watchdog unavailable"; exit 77; }
WIRED="${1:-}"; [ -n "$WIRED" ] && [ -x "$WIRED" ] || { echo "SKIP: assembled GUI binary required"; exit 77; }
WIRED="$(cd "$(dirname "$WIRED")" && pwd)/$(basename "$WIRED")"; WD="$(dirname "$WIRED")"
HEADLESS="${WIRED_BINARY_HEADLESS:-}"; [ -n "$HEADLESS" ] || HEADLESS="$WD/wired-headless.arm64"; [ -x "$HEADLESS" ] || { echo "SKIP: sibling wired-headless required"; exit 77; }
PACK=""; for c in "$WD" "$WD/../Resources"; do [ -f "$c/base/pax21.sw3z" ] && PACK="$c" && break; done; [ -n "$PACK" ] || { echo "SKIP: current pax21 unavailable"; exit 77; }
CONTENT="${WIRED_CONTENT_ROOT:-$PACK}"; if [ -f "$CONTENT/base/pax01.sw3z" ]; then BASE="$CONTENT/base/pax01.sw3z"; elif [ -f "$CONTENT/base/pak0.pk3" ]; then BASE="$CONTENT/base/pak0.pk3"; else echo "SKIP: set WIRED_CONTENT_ROOT"; exit 77; fi
ROOT="$(mktemp -d -t wired-ingame-info-XXXXXX 2>/dev/null || mktemp -d)"; GUI_HOME="$ROOT/gui/q3now-preview"; SERVER_HOME="$ROOT/server/q3now-preview"; RUN="$ROOT/run"; EVENTS="$ROOT/stale.jsonl"; LAYOUT="$RUN/layoutdump.jsonl"; SERVER_CONTROL="$ROOT/server-control.fifo"; SPID=""; FPID=""; SERVER_CONTROL_OPEN=0
cleanup(){ [ "$SERVER_CONTROL_OPEN" -eq 1 ] && exec 8>&- || true; [ -n "$FPID" ] && kill -TERM "$FPID" 2>/dev/null || true; [ -n "$SPID" ] && kill -TERM "$SPID" 2>/dev/null || true; [ -n "$FPID" ] && wait "$FPID" 2>/dev/null || true; [ -n "$SPID" ] && wait "$SPID" 2>/dev/null || true; [ "${WIRED_KEEP_ARTIFACTS:-0}" = 1 ] || rm -rf "$ROOT"; }; trap cleanup EXIT INT TERM
mkdir -p "$GUI_HOME/base" "$SERVER_HOME/base" "$RUN"; for h in "$GUI_HOME" "$SERVER_HOME"; do cp "$PACK/base/pax21.sw3z" "$h/base/" || exit 1; cp "$BASE" "$h/base/" || exit 1; done
python3 - "$ROOT/manifest.jsonl" "$WIRED" "$HEADLESS" "$PACK/base/pax21.sw3z" "$BASE" "$0" "$FIXTURE" "$TIMEOUT_RUNNER" <<'PY'
import hashlib,json,os,sys
out,*paths=sys.argv[1:]
with open(out,"w",encoding="utf-8") as f:
 for path in paths:
  h=hashlib.sha256()
  with open(path,"rb") as src:
   for chunk in iter(lambda:src.read(1024*1024),b""):h.update(chunk)
  f.write(json.dumps({"kind":"provenance","name":os.path.basename(path),"path":os.path.abspath(path),"bytes":os.path.getsize(path),"sha256":h.hexdigest()},sort_keys=True)+"\n")
PY
read -r CLIENT_PORT STALE_PORT SENTINEL_PORT ACTIVE_PORT <<EOF
$(python3 - <<'PY'
import socket
s=[]
for _ in range(4):
 x=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);x.bind(("127.0.0.1",0));s.append(x)
print(*(x.getsockname()[1] for x in s))
for x in s:x.close()
PY
)
EOF
python3 "$FIXTURE" --port "$STALE_PORT" --events "$EVENTS" & FPID=$!; for _ in $(seq 1 100); do [ -s "$EVENTS" ] && break; sleep .05; done
cat >"$SERVER_HOME/base/ingame-info-server.cfg" <<'CFG'
set g_password ""
set g_autoBots 0
map arena7
CFG
mkfifo "$SERVER_CONTROL"; exec 8<>"$SERVER_CONTROL"; SERVER_CONTROL_OPEN=1
(cd "$(dirname "$HEADLESS")" && exec "$HEADLESS" +set fs_homepath "$SERVER_HOME" +set com_automated 1 +set com_noHardReboot 1 +set net_ip 127.0.0.1 +set net_port "$ACTIVE_PORT" +set sv_hostname "ACTIVE B" +set sv_pure 0 +set log_severity DEBUG +set log_file_severity DEBUG +set log_file_mode overwrite_synced +exec ingame-info-server.cfg <&8) >"$ROOT/server.stdout" 2>&1 & SPID=$!
for _ in $(seq 1 600); do [ -s "$SERVER_HOME/qconsole.jsonl" ] && grep -Fq "Server: arena7" "$SERVER_HOME/qconsole.jsonl" && break; sleep .1; done
cat >"$GUI_HOME/base/ingame-info-active.cfg" <<CFG
wait 60
wui_test_keydown 27
wait 10
wui_ingame_trace
wui_server_fixture $SENTINEL_PORT $STALE_PORT
wui_push servers
wait 10
wui_listbox_sort 0
wui_menu_nav focus serverlist
wui_menu_nav down
wui_menu_nav back
wait 5
$(printf 'wui_menu_nav down\n%.0s' {1..10})
wui_ingame_trace
wui_menu_nav enter
wait 180
wui_pointer_item btn_back
wui_pointer_item btn_connect
wui_pointer_item btn_retry
wait 2
wui_pointer_click
wait 180
set r_layoutDump 1
wait 5
wui_ingame_trace
wui_pointer_item btn_close
wait 2
wui_pointer_click
wui_ingame_trace
$(printf 'wui_menu_nav up\n%.0s' {1..9})
wui_ingame_trace
wui_menu_nav enter
wui_ingame_trace
wait 30
wui_ingame_trace
quit
CFG
cat >"$GUI_HOME/base/ingame-info.cfg" <<CFG
set com_maxfps 60
wait 100
set activeAction "exec ingame-info-active.cfg"
connect 127.0.0.1:$ACTIVE_PORT
CFG
case "$(uname -s)" in Darwin) ARGS=(-ApplePersistenceIgnoreState YES);; *) ARGS=();; esac
python3 "$TIMEOUT_RUNNER" --timeout 90 --kill-after 10 --cwd "$RUN" --stdout "$ROOT/gui.stdout" -- "$WIRED" "${ARGS[@]}" +set fs_homepath "$GUI_HOME" +set com_automated 1 +set com_noHardReboot 1 +set net_ip 127.0.0.1 +set net_port "$CLIENT_PORT" +set s_initsound 0 +set r_fullscreen 0 +set r_mode -1 +set r_customwidth 1280 +set r_customheight 720 +set log_severity DEBUG +set log_file_severity DEBUG +set log_file_mode overwrite_synced +exec ingame-info.cfg || exit 1
kill -TERM "$FPID" 2>/dev/null || true; wait "$FPID" || exit 1; FPID=""
printf '%s\n' 'echo Q0_INGAME_INFO_PHASE_COMPLETE' >&8
for _ in $(seq 1 100); do grep -Fq 'Q0_INGAME_INFO_PHASE_COMPLETE' "$SERVER_HOME/qconsole.jsonl" 2>/dev/null && break; sleep .05; done
grep -Fq 'Q0_INGAME_INFO_PHASE_COMPLETE' "$SERVER_HOME/qconsole.jsonl" || { echo "FAIL: headless missed phase boundary"; exit 1; }
printf '%s\n' 'echo Q0_INGAME_INFO_QUIT_REQUESTED' quit >&8
for _ in $(seq 1 150); do kill -0 "$SPID" 2>/dev/null || break; sleep .1; done
kill -0 "$SPID" 2>/dev/null && { echo "FAIL: headless required forced teardown"; exit 1; }
wait "$SPID" || { echo "FAIL: headless exit was nonzero"; exit 1; }; SPID=""; exec 8>&-; SERVER_CONTROL_OPEN=0
analyze "$GUI_HOME/qconsole.jsonl" "$SERVER_HOME/qconsole.jsonl" "$EVENTS" "$LAYOUT" "$STALE_PORT" "$ACTIVE_PORT"
