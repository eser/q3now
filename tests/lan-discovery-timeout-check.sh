#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Concurrent directed ping and three authored LAN discovery generations.

set -uo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
. "$SCRIPT_DIR/lib/wired_paths.sh"
FIXTURE="$SCRIPT_DIR/lan-discovery-timeout-fixture.py"
TIMEOUT_RUNNER="$SCRIPT_DIR/run-with-timeout.py"

analyze() {
python3 - "$1" "$2" <<'PYEOF'
import hashlib,json,re,sys
product_path,fixture_path=sys.argv[1:]
def load(path):
 rows=[]
 for number,line in enumerate(open(path,encoding="utf-8",errors="strict"),1):
  try:row=json.loads(line)
  except Exception as exc:raise SystemExit(f"FAIL {path}:{number}: {exc}")
  if not isinstance(row,dict):raise SystemExit(f"FAIL {path}:{number}: schema")
  rows.append(row)
 return rows
p=load(product_path);f=load(fixture_path)
if not p or not f:raise SystemExit("FAIL empty evidence")
if any(not all(isinstance(r.get(k),str) for k in ("sev","cat","msg")) for r in p):raise SystemExit("FAIL product schema")
if any(not isinstance(r.get("event"),str) for r in f):raise SystemExit("FAIL fixture schema")
if any(r["sev"].upper() in ("ERROR","FATAL") or r["sev"].upper()=="WARN" and r["cat"].lower()=="ui" for r in p):raise SystemExit("FAIL severity")
listener_ready=[r for r in f if r["event"]=="listener_ready"]
if len(listener_ready)!=1 or listener_ready[0].get("ports")!=[27960,27961,27962,27963] or listener_ready[0].get("protocol")!=74 or re.fullmatch(r"(?:[0-9]{1,3}\.){3}[0-9]{1,3}",str(listener_ready[0].get("lan_address",""))) is None:raise SystemExit("FAIL listener ready")
ready=[r for r in f if r["event"]=="ready"]
if len(ready)!=1 or ready[0].get("ports")!=[27960,27961,27962,27963] or ready[0].get("protocol")!=74 or ready[0].get("lan_address")!=listener_ready[0]["lan_address"] or type(ready[0].get("client_port")) is not int:raise SystemExit("FAIL ready")
ready_index=next(i for i,r in enumerate(f) if r["event"]=="ready")
if any(i<=ready_index for i,r in enumerate(f) if r["event"] in ("request","response")):raise SystemExit("FAIL ready evidence order")
lan_address=ready[0]["lan_address"]
requests=[r for r in f if r["event"]=="request"]
if len(requests)!=25:raise SystemExit("FAIL request cardinality")
challenges=[]
for row in requests:
 challenge=row.get("challenge")
 if challenge not in challenges:challenges.append(challenge)
if len(challenges)!=4 or any(re.fullmatch(r"[0-9a-f]{32}",c or "") is None for c in challenges) or len(set(challenges))!=4:raise SystemExit("FAIL challenge uniqueness")
manual=[r for r in requests if r["challenge"]==challenges[0]]
if len(manual)!=1 or manual[0].get("listener_port")!=27960:raise SystemExit("FAIL manual request")
client_port=ready[0]["client_port"]
if type(manual[0].get("source_port")) is not int or manual[0].get("source_port")!=client_port:raise SystemExit("FAIL client port")
OOB=b"\xff"*4
for generation,challenge in enumerate(challenges):
 rows=[r for r in requests if r.get("challenge")==challenge]
 if generation:
  if len(rows)!=8 or sorted(r.get("listener_port") for r in rows)!=[27960,27960,27961,27961,27962,27962,27963,27963]:raise SystemExit(f"FAIL request ports generation={generation}")
  if sorted(r.get("request_ordinal") for r in rows)!=list(range(1,9)):raise SystemExit(f"FAIL request ordinals generation={generation}")
 elif len(rows)!=1 or rows[0].get("listener_port")!=27960:
  raise SystemExit("FAIL manual request port")
 for ordinal,row in enumerate(rows,1):
  wire=OOB+f"getinfo {challenge}".encode()
  try:actual=bytes.fromhex(row.get("hex",""))
  except ValueError:raise SystemExit("FAIL request hex")
  if actual!=wire or row.get("length")!=len(wire) or row.get("sha256")!=hashlib.sha256(wire).hexdigest():raise SystemExit("FAIL request bytes")
  if type(row.get("generation")) is not int or row.get("generation")!=generation:raise SystemExit("FAIL request fixture generation")
  if not isinstance(row.get("listener_port"),int) or row.get("listener")!=f"0.0.0.0:{row['listener_port']}":raise SystemExit("FAIL listener identity")
  if row.get("source_port")!=client_port or row.get("source")!=f"{lan_address}:{client_port}":raise SystemExit("FAIL request identity")
def response(challenge,name):
 info=(f"\\challenge\\{challenge}\\protocol\\74\\hostname\\{name}\\mapname\\arena7\\clients\\1\\sv_maxclients\\8\\game\\q3now\\gametype\\0\\minping\\0\\maxping\\0")
 return OOB+b"infoResponse\n"+info.encode()
wanted=[("local_a_current",1,response(challenges[1],"LAN CURRENT A")),("manual_p_current",0,response(challenges[0],"MANUAL P")),("local_b_expired",2,response(challenges[2],"LAN EXPIRED B")),("local_b_inactive_replay",2,response(challenges[2],"LAN EXPIRED B")),("local_b_stale_under_c",3,response(challenges[2],"LAN EXPIRED B")),("local_c_current",3,response(challenges[3],"LAN CURRENT C")),("local_c_duplicate",3,response(challenges[3],"LAN CURRENT C"))]
sent=[r for r in f if r["event"]=="response"]
if [(r.get("scenario"),r.get("generation")) for r in sent]!=[(a,b) for a,b,_ in wanted]:raise SystemExit("FAIL response sequence")
for row,(_,_,wire) in zip(sent,wanted):
 try:actual=bytes.fromhex(row.get("hex",""))
 except ValueError:raise SystemExit("FAIL response hex")
 if actual!=wire or row.get("length")!=len(wire) or row.get("sha256")!=hashlib.sha256(wire).hexdigest():raise SystemExit("FAIL response bytes")
 if type(row.get("source_port")) is not int or row.get("source_port")!=27960 or type(row.get("peer_port")) is not int or row.get("peer_port")!=client_port or row.get("source")!=f"{lan_address}:27960" or row.get("peer")!=f"{lan_address}:{client_port}":raise SystemExit("FAIL response identity")
if sent[2]["hex"]!=sent[3]["hex"] or sent[2]["hex"]!=sent[4]["hex"] or sent[5]["hex"]!=sent[6]["hex"]:raise SystemExit("FAIL replay identity")
first={c:min(r["elapsed_ms"] for r in requests if r["challenge"]==c) for c in challenges}
timeline=[r for r in f if r["event"] in ("request","response")]
if any(not isinstance(r.get("elapsed_ms"),int) for r in timeline) or any(a["elapsed_ms"]>b["elapsed_ms"] for a,b in zip(timeline,timeline[1:])):raise SystemExit("FAIL unified fixture timeline")
if not first[challenges[0]]<first[challenges[1]]<sent[0]["elapsed_ms"]<sent[1]["elapsed_ms"]<first[challenges[2]]<sent[2]["elapsed_ms"]<sent[3]["elapsed_ms"]<first[challenges[3]]<sent[4]["elapsed_ms"]<sent[5]["elapsed_ms"]<sent[6]["elapsed_ms"]:raise SystemExit("FAIL fixture causal timeline")
if not 0<sent[0]["elapsed_ms"]-first[challenges[1]]<3000 or not 0<sent[1]["elapsed_ms"]-first[challenges[0]]<800 or not 3000<=sent[2]["elapsed_ms"]-first[challenges[2]]<4000 or not 0<sent[5]["elapsed_ms"]-first[challenges[3]]<3000:raise SystemExit("FAIL response deadline windows")
m=[r["msg"] for r in p]
def clean(msg):
 if msg.endswith("\n"):msg=msg[:-1]
 if "\n" in msg or "\r" in msg:raise SystemExit("FAIL critical newline")
 return msg
def family(prefix,patterns,sev="DEBUG",cat="client"):
 rows=[(i,r,clean(msg)) for i,(r,msg) in enumerate(zip(p,m)) if msg.startswith(prefix)]
 if len(rows)!=len(patterns):raise SystemExit(f"FAIL {prefix} cardinality")
 for (_,row,msg),pattern in zip(rows,patterns):
  if row["sev"].upper()!=sev or row["cat"].lower()!=cat or re.fullmatch(pattern,msg) is None:raise SystemExit(f"FAIL {prefix} metadata")
 return rows
ep=re.escape(f"{lan_address}:27960")
ping=family("Ping request generation=",[rf"Ping request generation=([0-9]+) challenge={challenges[0]} timeout=800ms address={ep}"])
local=family("Local discovery request generation=",[rf"Local discovery request generation=([0-9]+) challenge={c} timeout=3000ms" for c in challenges[1:]])
generations=[int(re.fullmatch(r"Local discovery request generation=([0-9]+).*",x[2]).group(1)) for x in local]
ping_generation=int(re.fullmatch(r"Ping request generation=([0-9]+).*",ping[0][2]).group(1))
if generations!=[ping_generation+1,ping_generation+2,ping_generation+3]:raise SystemExit("FAIL request generation chain")
accepted_local=family("Accepted local discovery infoResponse generation=",[rf"Accepted local discovery infoResponse generation={generations[0]} address={ep} rtt=[1-9][0-9]*ms name=LAN CURRENT A map=arena7 clients=1/8 gametype=0",rf"Accepted local discovery infoResponse generation={generations[2]} address={ep} rtt=[1-9][0-9]*ms name=LAN CURRENT C map=arena7 clients=1/8 gametype=0"],"INFO","client")
accepted_local_rtts=[int(re.search(r" rtt=([0-9]+)ms ",x[2]).group(1)) for x in accepted_local]
if any(not 0<rtt<3000 for rtt in accepted_local_rtts):raise SystemExit("FAIL accepted local deadline")
accepted_ping=family("Accepted ping infoResponse generation=",[rf"Accepted ping infoResponse generation=([0-9]+) time=[1-9][0-9]*ms address={ep}"])
if ping_generation!=int(re.fullmatch(r"Accepted ping infoResponse generation=([0-9]+).*",accepted_ping[0][2]).group(1)):raise SystemExit("FAIL ping generation")
accepted_ping_ms=int(re.search(r" time=([0-9]+)ms ",accepted_ping[0][2]).group(1))
if not 0<accepted_ping_ms<800:raise SystemExit("FAIL accepted ping deadline")
expired=family("Ignored expired local discovery infoResponse",[rf"Ignored expired local discovery infoResponse generation={generations[1]} elapsed=3[0-9]{{3}}ms from {ep}"])
expired_ms=int(re.fullmatch(r"Ignored expired local discovery infoResponse generation=[0-9]+ elapsed=([0-9]+)ms.*",expired[0][2]).group(1))
if not 3000<=expired_ms<4000:raise SystemExit("FAIL product expired deadline")
inactive=family("Ignored unsolicited infoResponse",[rf"Ignored unsolicited infoResponse from {ep}"])
mismatch=family("Ignored infoResponse challenge mismatch",[rf"Ignored infoResponse challenge mismatch generation={generations[2]} from {ep}"])
markers=family("LAN_DISCOVERY_",["LAN_DISCOVERY_B_INACTIVE","LAN_DISCOVERY_COMPLETE"],"INFO","system")
scans=family("Scanning for servers on the local network",[r"Scanning for servers on the local network\.\.\."]*3,"INFO","client")
nav=family("wui_menu_nav: K_",[r"wui_menu_nav: K_DOWNARROW dispatched",r"wui_menu_nav: K_ENTER dispatched",r"wui_menu_nav: K_ENTER dispatched",r"wui_menu_nav: K_ENTER dispatched",r"wui_menu_nav: K_DOWNARROW dispatched",r"wui_menu_nav: K_ESCAPE dispatched"],"DEBUG","ui")
packets=family("CL packet ",[rf"CL packet {ep}: infoResponse"]*7,"DEBUG","client")
pushes=family("WiredUI: push menu ",[r"WiredUI: push menu 'main' \(depth 1\)",r"WiredUI: push menu 'servers' \(depth 2\)"],"DEBUG","ui")
pops=family("WiredUI: pop menu",[r"WiredUI: pop menu \(depth 1\)"],"DEBUG","ui")
focus=family("wui_menu_nav focus:",[r"wui_menu_nav focus: focused item 'btn_refresh' \(top index -1\)",r"wui_menu_nav focus: focused item 'btn_refresh' \(top index -1\)",r"wui_menu_nav focus: focused item 'serverlist' \(top index -1\)"],"DEBUG","ui")
selection=family("WiredUI: server selection ",[rf"WiredUI: server selection display_row=0 raw=0 source=0 list_generation=([0-9]+) selection_generation=[0-9]+ address={ep} name=LAN CURRENT C map=arena7"],"DEBUG","ui")
summaries=family("WiredUI: server roster generation=",[r"WiredUI: server roster generation=([0-9]+) source=0 displayed=0 raw=0",r"WiredUI: server roster generation=([0-9]+) source=0 displayed=1 raw=1",r"WiredUI: server roster generation=([0-9]+) source=0 displayed=0 raw=0",r"WiredUI: server roster generation=([0-9]+) source=0 displayed=1 raw=1"],"DEBUG","ui")
summary_generations=[int(re.fullmatch(r"WiredUI: server roster generation=([0-9]+).*",x[2]).group(1)) for x in summaries]
if summary_generations!=list(range(summary_generations[0],summary_generations[0]+4)):raise SystemExit("FAIL roster generations")
selection_list_generation=int(re.fullmatch(r"WiredUI: server selection .*list_generation=([0-9]+).*",selection[0][2]).group(1))
if selection_list_generation!=summary_generations[-1]:raise SystemExit("FAIL selection roster epoch")
rows=family("WiredUI: server roster row=",[rf"WiredUI: server roster row=0 raw=0 address={ep} name=LAN CURRENT A map=arena7",rf"WiredUI: server roster row=0 raw=0 address={ep} name=LAN CURRENT C map=arena7"],"DEBUG","ui")
if any("name=MANUAL P map=arena7" in x for x in m):raise SystemExit("FAIL manual ping browser-cache fanout")
if not pushes[0][0]<nav[0][0]<pushes[1][0]<nav[1][0]<scans[0][0]<local[0][0]<summaries[0][0]<packets[0][0]<accepted_local[0][0]<summaries[1][0]<rows[0][0]<packets[1][0]<accepted_ping[0][0]<focus[0][0]<nav[2][0]<scans[1][0]<local[1][0]<summaries[2][0]<packets[2][0]<expired[0][0]<packets[3][0]<inactive[0][0]<markers[0][0]<focus[1][0]<nav[3][0]<scans[2][0]<local[2][0]<packets[4][0]<mismatch[0][0]<packets[5][0]<accepted_local[1][0]<summaries[3][0]<rows[1][0]<selection[0][0]<focus[2][0]<nav[4][0]<markers[1][0]<pops[0][0]<nav[5][0]:raise SystemExit("FAIL causal order")
if not accepted_local[1][0]<packets[6][0]<selection[0][0]:raise SystemExit("FAIL duplicate packet causal window")
for forbidden in ("LAN EXPIRED B map=", "Accepted local discovery infoResponse generation="+str(generations[1]), "QUIC client:", "FIRST GAMEPLAY FRAME", "queued validated"):
 if any(forbidden in x for x in m):raise SystemExit(f"FAIL forbidden {forbidden}")
for prefix in ("Ignored expired ping infoResponse", "Ignored non-LAN local discovery infoResponse", "Rejected overlong infoResponse", "Rejected full infoResponse", "Rejected invalid infoResponse", "MAX_OTHER_SERVERS hit", "Master query generation=", "Accepted master response", "Accepted getserversResponse"):
 if any(x.startswith(prefix) for x in m):raise SystemExit(f"FAIL additive resolver family {prefix}")
stopped=[r for r in f if r["event"]=="stopped"]
if len(stopped)!=1 or stopped[0].get("reason")!="signal" or any(r["event"] in ("unexpected_request","fixture_error") for r in f):raise SystemExit("FAIL fixture lifecycle")
print("PASS LAN discovery: concurrent ping preserved; B expires/inactivates; C recovers")
PYEOF
}

write_self_fixture() {
python3 - "$1" "$2" "$3" <<'PYEOF'
import hashlib,json,sys
pp,fp,mode=sys.argv[1:];O=b"\xff"*4;cs=["1"*32,"2"*32,"3"*32,"4"*32];client=40000;lan="192.0.2.10"
def wf(w):return {"hex":w.hex(),"length":len(w),"sha256":hashlib.sha256(w).hexdigest()}
F=[{"event":"listener_ready","ports":[27960,27961,27962,27963],"protocol":74,"lan_address":lan},{"event":"ready","ports":[27960,27961,27962,27963],"protocol":74,"lan_address":lan,"client_port":client}]
def request(g,c,n,port,t):
 w=O+f"getinfo {c}".encode();return {"event":"request","generation":g,"request_ordinal":n,"challenge":c,"listener":f"0.0.0.0:{port}","listener_port":port,"source":f"{lan}:{client}","source_port":client,"elapsed_ms":t,**wf(w)}
F.append(request(0,cs[0],1,27960,10))
for g,c,t in ((1,cs[1],30),(2,cs[2],300),(3,cs[3],3600)):
 for n,port in enumerate([27960,27961,27962,27963]*2,1):F.append(request(g,c,n,port,t+n))
def response(c,name):return O+b"infoResponse\n"+(f"\\challenge\\{c}\\protocol\\74\\hostname\\{name}\\mapname\\arena7\\clients\\1\\sv_maxclients\\8\\game\\q3now\\gametype\\0\\minping\\0\\maxping\\0").encode()
for scenario,g,w,t in [("local_a_current",1,response(cs[1],"LAN CURRENT A"),80),("manual_p_current",0,response(cs[0],"MANUAL P"),160),("local_b_expired",2,response(cs[2],"LAN EXPIRED B"),3405),("local_b_inactive_replay",2,response(cs[2],"LAN EXPIRED B"),3465),("local_b_stale_under_c",3,response(cs[2],"LAN EXPIRED B"),3640),("local_c_current",3,response(cs[3],"LAN CURRENT C"),3680),("local_c_duplicate",3,response(cs[3],"LAN CURRENT C"),3710)]:F.append({"event":"response","scenario":scenario,"generation":g,"source":f"{lan}:27960","source_port":27960,"peer":f"{lan}:{client}","peer_port":client,"elapsed_ms":t,**wf(w)})
F.append({"event":"stopped","reason":"signal"})
F=F[:2]+sorted(F[2:-1],key=lambda row:row["elapsed_ms"])+[F[-1]]
ep=f"{lan}:27960";P=[]
def add(sev,cat,msg):P.append({"sev":sev,"cat":cat,"msg":msg})
add("DEBUG","ui","WiredUI: push menu 'main' (depth 1)")
add("DEBUG","client",f"Ping request generation=5 challenge={cs[0]} timeout=800ms address={ep}")
add("DEBUG","ui","wui_menu_nav: K_DOWNARROW dispatched")
add("DEBUG","ui","WiredUI: push menu 'servers' (depth 2)")
add("DEBUG","ui","wui_menu_nav: K_ENTER dispatched")
add("INFO","client","Scanning for servers on the local network...")
add("DEBUG","client",f"Local discovery request generation=6 challenge={cs[1]} timeout=3000ms")
add("DEBUG","ui","WiredUI: server roster generation=20 source=0 displayed=0 raw=0")
add("DEBUG","client",f"CL packet {ep}: infoResponse")
add("INFO","client",f"Accepted local discovery infoResponse generation=6 address={ep} rtt=50ms name=LAN CURRENT A map=arena7 clients=1/8 gametype=0")
add("DEBUG","ui","WiredUI: server roster generation=21 source=0 displayed=1 raw=1")
add("DEBUG","ui",f"WiredUI: server roster row=0 raw=0 address={ep} name=LAN CURRENT A map=arena7")
add("DEBUG","client",f"CL packet {ep}: infoResponse")
add("DEBUG","client",f"Accepted ping infoResponse generation=5 time=150ms address={ep}")
add("DEBUG","ui","wui_menu_nav focus: focused item 'btn_refresh' (top index -1)")
add("DEBUG","ui","wui_menu_nav: K_ENTER dispatched")
add("INFO","client","Scanning for servers on the local network...")
add("DEBUG","client",f"Local discovery request generation=7 challenge={cs[2]} timeout=3000ms")
add("DEBUG","ui","WiredUI: server roster generation=22 source=0 displayed=0 raw=0")
add("DEBUG","client",f"CL packet {ep}: infoResponse")
add("DEBUG","client",f"Ignored expired local discovery infoResponse generation=7 elapsed=3105ms from {ep}")
add("DEBUG","client",f"CL packet {ep}: infoResponse")
add("DEBUG","client",f"Ignored unsolicited infoResponse from {ep}")
add("INFO","system","LAN_DISCOVERY_B_INACTIVE")
add("DEBUG","ui","wui_menu_nav focus: focused item 'btn_refresh' (top index -1)")
add("DEBUG","ui","wui_menu_nav: K_ENTER dispatched")
add("INFO","client","Scanning for servers on the local network...")
add("DEBUG","client",f"Local discovery request generation=8 challenge={cs[3]} timeout=3000ms")
add("DEBUG","client",f"CL packet {ep}: infoResponse")
add("DEBUG","client",f"Ignored infoResponse challenge mismatch generation=8 from {ep}")
add("DEBUG","client",f"CL packet {ep}: infoResponse")
add("INFO","client",f"Accepted local discovery infoResponse generation=8 address={ep} rtt=80ms name=LAN CURRENT C map=arena7 clients=1/8 gametype=0")
add("DEBUG","client",f"CL packet {ep}: infoResponse")
add("DEBUG","ui","WiredUI: server roster generation=23 source=0 displayed=1 raw=1")
add("DEBUG","ui",f"WiredUI: server roster row=0 raw=0 address={ep} name=LAN CURRENT C map=arena7")
add("DEBUG","ui",f"WiredUI: server selection display_row=0 raw=0 source=0 list_generation=23 selection_generation=2 address={ep} name=LAN CURRENT C map=arena7")
add("DEBUG","ui","wui_menu_nav focus: focused item 'serverlist' (top index -1)")
add("DEBUG","ui","wui_menu_nav: K_DOWNARROW dispatched")
add("INFO","system","LAN_DISCOVERY_COMPLETE")
add("DEBUG","ui","WiredUI: pop menu (depth 1)")
add("DEBUG","ui","wui_menu_nav: K_ESCAPE dispatched")
if mode=="blanket_clear":P=[r for r in P if not r["msg"].startswith("Accepted ping infoResponse")]
elif mode=="address_shadow":
 i=next(i for i,r in enumerate(P) if r["msg"].startswith("Accepted local discovery"));P[i]["msg"]=f"Ignored infoResponse challenge mismatch generation=5 from {ep}"
elif mode=="collision":
 for r in F:
  if r.get("generation")==1:r["challenge"]=cs[0]
elif mode=="missing_expired":P=[r for r in P if not r["msg"].startswith("Ignored expired local")]
elif mode=="missing_inactive":P=[r for r in P if not r["msg"].startswith("Ignored unsolicited")]
elif mode=="duplicate_commit":
 i=next(i for i,r in enumerate(P) if "name=LAN CURRENT C" in r["msg"] and r["msg"].startswith("Accepted"));P.insert(i,dict(P[i]))
elif mode=="wrong_metadata":next(r for r in P if r["msg"].startswith("Ignored expired local"))["cat"]="ui"
elif mode=="suffix":next(r for r in P if r["msg"].startswith("Ignored expired local"))["msg"]+=" EXTRA"
elif mode=="request_hash":next(r for r in F if r.get("event")=="request")["sha256"]="0"*64
elif mode=="request_count":F.pop(10)
elif mode=="wrong_port_multiplicity":
 rows=[r for r in F if r.get("event")=="request" and r.get("generation")==2]
 rows[0]["listener_port"]=27961;rows[0]["listener"]="0.0.0.0:27961"
elif mode=="additive_packet":P.append({"sev":"DEBUG","cat":"client","msg":f"CL packet {ep}: infoResponse EXTRA"})
elif mode=="roster_raw_leak":P.append({"sev":"DEBUG","cat":"ui","msg":f"WiredUI: server roster row=1 raw=1 address={ep} name=LEAK map=arena1"})
elif mode=="focus_drift":P.insert(4,{"sev":"DEBUG","cat":"ui","msg":"wui_menu_nav focus: focused item 'btn_filter' (top index -1)"})
elif mode=="main_focus":P.insert(1,{"sev":"DEBUG","cat":"ui","msg":"wui_menu_nav focus: focused item 'menu_campaign' (top index -1)"})
elif mode=="missing_escape":P=[r for r in P if r["msg"]!="wui_menu_nav: K_ESCAPE dispatched"]
elif mode=="extra_escape":P.append({"sev":"DEBUG","cat":"ui","msg":"wui_menu_nav: K_ESCAPE dispatched"})
elif mode=="escape_suffix":next(r for r in P if r["msg"]=="wui_menu_nav: K_ESCAPE dispatched")["msg"]+=" EXTRA"
elif mode=="nav_additive":P.insert(5,{"sev":"DEBUG","cat":"ui","msg":"wui_menu_nav: K_DOWNARROW dispatched"})
elif mode.startswith("local_before_enter_"):
 generation={"local_before_enter_a":"6","local_before_enter_b":"7","local_before_enter_c":"8"}[mode]
 li=next(i for i,r in enumerate(P) if r["msg"].startswith(f"Local discovery request generation={generation} "))
 ni=max(i for i,r in enumerate(P[:li]) if r["msg"]=="wui_menu_nav: K_ENTER dispatched")
 P.insert(ni,P.pop(li))
elif mode=="focus_before_selection":
 si=next(i for i,r in enumerate(P) if r["msg"].startswith("WiredUI: server selection "))
 fi=next(i for i,r in enumerate(P) if r["msg"].startswith("wui_menu_nav focus: focused item 'serverlist'"))
 P.insert(si,P.pop(fi))
elif mode=="duplicate_packet_before_accept":
 packet_indices=[i for i,r in enumerate(P) if r["msg"].startswith("CL packet ")]
 ci,di=packet_indices[5],packet_indices[6]
 P.insert(ci,P.pop(di))
elif mode=="duplicate_packet_after_complete":
 di=[i for i,r in enumerate(P) if r["msg"].startswith("CL packet ")][6]
 mi=next(i for i,r in enumerate(P) if r["msg"]=="LAN_DISCOVERY_COMPLETE")
 P.insert(mi+1,P.pop(di))
elif mode=="response_source_port":
 for r in F:
  if r.get("event")=="response":r["source_port"]=1
elif mode=="response_peer_port":
 for r in F:
  if r.get("event")=="response":r["peer_port"]=1
elif mode=="cl_suffix":next(r for r in P if r["msg"].startswith("CL packet "))["msg"]+=" EXTRA"
elif mode=="protocol75":next(r for r in F if r.get("event")=="ready")["protocol"]=75
elif mode=="request_generation99":next(r for r in F if r.get("event")=="request")["generation"]=99
elif mode=="p_at_deadline":next(r for r in P if r["msg"].startswith("Accepted ping infoResponse"))["msg"]=next(r for r in P if r["msg"].startswith("Accepted ping infoResponse"))["msg"].replace("time=150ms","time=800ms")
elif mode=="a_at_deadline":next(r for r in P if "Accepted local discovery" in r["msg"] and "CURRENT A" in r["msg"])["msg"]=next(r for r in P if "Accepted local discovery" in r["msg"] and "CURRENT A" in r["msg"])["msg"].replace("rtt=50ms","rtt=3000ms")
elif mode=="c_at_deadline":next(r for r in P if "Accepted local discovery" in r["msg"] and "CURRENT C" in r["msg"])["msg"]=next(r for r in P if "Accepted local discovery" in r["msg"] and "CURRENT C" in r["msg"])["msg"].replace("rtt=80ms","rtt=3000ms")
elif mode=="wrong_stack":next(r for r in P if "push menu 'servers'" in r["msg"])["msg"]="WiredUI: push menu 'servers' (depth 1)"
elif mode=="early_b":next(r for r in F if r.get("scenario")=="local_b_expired")["elapsed_ms"]=1000
elif mode=="timeout_stop":F[-1]["reason"]="timeout"
elif mode=="generation_gap":next(r for r in P if "Local discovery request generation=7" in r["msg"])["msg"]=f"Local discovery request generation=9 challenge={cs[2]} timeout=3000ms"
elif mode=="deadline_low":next(r for r in P if r["msg"].startswith("Ignored expired local"))["msg"]=f"Ignored expired local discovery infoResponse generation=7 elapsed=2999ms from {ep}"
elif mode=="selection_stale":next(r for r in P if r["msg"].startswith("WiredUI: server selection"))["msg"]=next(r for r in P if r["msg"].startswith("WiredUI: server selection"))["msg"].replace("list_generation=23","list_generation=22")
elif mode=="additive_resolver":P.append({"sev":"DEBUG","cat":"client","msg":f"Rejected invalid infoResponse from {ep}"})
elif mode=="summary_gap":next(r for r in P if "server roster generation=22" in r["msg"])["msg"]="WiredUI: server roster generation=25 source=0 displayed=0 raw=0"
elif mode=="listener_type":next(r for r in F if r.get("event")=="request" and r.get("generation")==2)["listener_port"]="27960"
for path,rows in ((pp,P),(fp,F)):
 with open(path,"w") as out:
  for row in rows:out.write(json.dumps(row)+"\n")
PYEOF
}

if [ "${1:-}" = --self-test ]; then
 ROOT="$(mktemp -d -t lan-discovery-self-XXXXXX 2>/dev/null || mktemp -d)"; trap 'rm -rf "$ROOT"' EXIT
 write_self_fixture "$ROOT/p" "$ROOT/f" clean; analyze "$ROOT/p" "$ROOT/f" >/dev/null || exit 1
 for defect in blanket_clear address_shadow collision missing_expired missing_inactive duplicate_commit wrong_metadata suffix request_hash request_count wrong_port_multiplicity additive_packet roster_raw_leak focus_drift main_focus missing_escape extra_escape escape_suffix nav_additive local_before_enter_a local_before_enter_b local_before_enter_c focus_before_selection duplicate_packet_before_accept duplicate_packet_after_complete response_source_port response_peer_port cl_suffix protocol75 request_generation99 p_at_deadline a_at_deadline c_at_deadline wrong_stack early_b timeout_stop generation_gap deadline_low selection_stale additive_resolver summary_gap listener_type; do
  write_self_fixture "$ROOT/p-$defect" "$ROOT/f-$defect" "$defect"
  if analyze "$ROOT/p-$defect" "$ROOT/f-$defect" >/dev/null 2>&1; then echo "FAIL self $defect"; exit 1; fi
 done
 echo "==> LAN discovery analyzer self-test: PASS (42 defects rejected)"; exit 0
fi

command -v python3 >/dev/null 2>&1 && [ -f "$FIXTURE" ] && [ -f "$TIMEOUT_RUNNER" ] || { echo "SKIP: Python unavailable"; exit 77; }
WIRED="${1:-}"; [ -n "$WIRED" ] && [ -x "$WIRED" ] || { echo "SKIP: pass assembled Wired GUI binary"; exit 77; }
WIRED="$(cd "$(dirname "$WIRED")" && pwd)/$(basename "$WIRED")"; WIRED_DIR="$(dirname "$WIRED")"
PACK="$(wired_find_archive_root "$WIRED_DIR" "$WIRED_DIR/../Resources" "$WIRED_DIR/q3now-preview.arm64.app/Contents/Resources" 2>/dev/null || true)"
[ -n "$PACK" ] || { echo "SKIP: current VFS archives unavailable"; exit 77; }
ROOT="$(mktemp -d -t lan-discovery-XXXXXX 2>/dev/null || mktemp -d)"; EVENTS="$ROOT/fixture.jsonl"; HOME_DIR="$ROOT/q3now-preview"; PID=""
cleanup(){ [ -n "$PID" ] && kill -TERM "$PID" 2>/dev/null || true; [ -n "$PID" ] && wait "$PID" 2>/dev/null || true; [ "${WIRED_KEEP_ARTIFACTS:-0}" = 1 ] || rm -rf "$ROOT"; }; trap cleanup EXIT INT TERM
wired_link_content_into_home "$HOME_DIR" "$PACK/base" || exit 1
python3 "$FIXTURE" --events "$EVENTS" --timeout 30 & PID=$!; for _ in $(seq 1 100); do [ -s "$EVENTS" ] && break; sleep .05; done; [ -s "$EVENTS" ] || exit 1
LAN_ADDRESS="$(python3 - "$EVENTS" <<'PYEOF'
import json,sys
row=json.loads(open(sys.argv[1]).readline())
if row.get("event")=="skip": raise SystemExit(77)
print(row["lan_address"])
PYEOF
)"; ready_rc=$?
if [ "$ready_rc" -eq 77 ]; then wait "$PID" 2>/dev/null || true; PID=""; echo "SKIP: no non-loopback IPv4 interface"; exit 77; fi
[ "$ready_rc" -eq 0 ] && [ -n "$LAN_ADDRESS" ] || { echo "FAIL: invalid fixture readiness"; exit 1; }
cat >"$HOME_DIR/base/lan-discovery.cfg" <<CFGEOF
set cl_maxPing 800
wait 100
ping $LAN_ADDRESS:27960
wait 5
wui_push main
wait 30
wui_menu_nav down
wui_menu_nav enter
waitms 300
wui_menu_nav focus btn_refresh
wui_menu_nav enter
waitms 3300
echo LAN_DISCOVERY_B_INACTIVE
wui_menu_nav focus btn_refresh
wui_menu_nav enter
waitms 300
wui_menu_nav focus serverlist
wui_menu_nav down
wait 10
echo LAN_DISCOVERY_COMPLETE
wui_menu_nav back
wait 5
quit
CFGEOF
case "$(uname -s)" in Darwin) NATIVE="$HOME_DIR"; ARGS=(-ApplePersistenceIgnoreState YES);; *) NATIVE="$HOME_DIR"; ARGS=();; esac
python3 "$TIMEOUT_RUNNER" --timeout 30 --kill-after 10 --cwd "$WIRED_DIR" --stdout "$ROOT/stdout" -- "$WIRED" "${ARGS[@]}" +set fs_homepath "$NATIVE" +set com_automated 1 +set com_noHardReboot 1 +set net_enabled 1 +set net_ip 0.0.0.0 +set net_port 0 +set s_initsound 0 +set r_fullscreen 0 +set r_mode -1 +set r_customwidth 1280 +set r_customheight 720 +set log_severity DEBUG +set log_file_severity DEBUG +set log_file_mode overwrite_synced +exec lan-discovery.cfg
rc=$?; [ "$rc" -eq 0 ] && [ -s "$HOME_DIR/qconsole.jsonl" ] || { echo "FAIL product rc=$rc"; exit 1; }
kill -TERM "$PID" 2>/dev/null || true; wait "$PID" || exit 1; PID=""
analyze "$HOME_DIR/qconsole.jsonl" "$EVENTS"
