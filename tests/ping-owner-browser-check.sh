#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors
# Direct/global same-address ping ownership and browser consumer/cache lifecycle.
set -uo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
FIXTURE="$SCRIPT_DIR/ping-owner-fixture.py"
TIMEOUT_RUNNER="$SCRIPT_DIR/run-with-timeout.py"

analyze() {
python3 - "$1" "$2" <<'PYEOF'
import hashlib,json,re,socket,struct,sys
pp,fp=sys.argv[1:3]
def load(path):
 rows=[]
 for n,line in enumerate(open(path,encoding="utf-8",errors="replace"),1):
  try:r=json.loads(line)
  except Exception as e:raise SystemExit(f"FAIL {path}:{n}: {e}")
  if not isinstance(r,dict):raise SystemExit("FAIL schema")
  rows.append(r)
 return rows
p,f=load(pp),load(fp)
if not p or not f:raise SystemExit("FAIL empty evidence")
if any(not all(isinstance(r.get(k),str) for k in ("sev","cat","msg")) for r in p):raise SystemExit("FAIL product schema")
if any(r["sev"] in ("ERROR","FATAL") or r["sev"]=="WARN" and r["cat"]=="ui" for r in p):raise SystemExit("FAIL severity")
ready=[r for r in f if r.get("event")=="ready"]
if len(ready)!=1 or ready[0].get("protocol")!=74:raise SystemExit("FAIL ready")
q=ready[0]
if any(not isinstance(q.get(k),int) or not 0<q[k]<65536 for k in ("master_port","endpoint_port")):raise SystemExit("FAIL ports")
ep=f"127.0.0.1:{q['endpoint_port']}";ee=re.escape(ep);mp=f"127.0.0.1:{q['master_port']}";em=re.escape(mp)
events=[r.get("event") for r in f]
expected_events=["ready","request","direct_held","request","response","response","request","response","request","response","request","response","stopped"]
if events!=expected_events or "unexpected" in events:raise SystemExit("FAIL fixture lifecycle")
requests=[r for r in f if r.get("event")=="request"]
if [r.get("role") for r in requests] != ["endpoint","master","endpoint","master","endpoint"]:raise SystemExit("FAIL fixture request order")
if [r.get("ordinal") for r in requests] != [1,1,2,2,3]:raise SystemExit("FAIL fixture request ordinals")
if requests[0].get("command","").split(" ",1)[0]!="getinfo" or requests[1].get("command")!="getservers q3now 74" or requests[2].get("command","").split(" ",1)[0]!="getinfo" or requests[3].get("command")!="getservers q3now 74" or requests[4].get("command","").split(" ",1)[0]!="getinfo":raise SystemExit("FAIL fixture commands")
peer={r.get("peer_port") for r in requests}
if len(peer)!=1 or not all(isinstance(x,int) and 0<x<65536 for x in peer):raise SystemExit("FAIL request peer")
responses=[r for r in f if r.get("event")=="response"]
if [r.get("scenario") for r in responses] != ["master_1","direct_before_global","global_1","master_2","global_2"]:raise SystemExit("FAIL response order")
OOB=b"\xff"*4
def master():return OOB+b"getserversResponse\\"+socket.inet_aton("127.0.0.1")+struct.pack("!H",q["endpoint_port"])+b"\\EOT"
def info(ch,name):return OOB+b"infoResponse\n"+(f"\\challenge\\{ch}\\protocol\\74\\hostname\\{name}\\mapname\\arena7\\clients\\1\\sv_maxclients\\8\\game\\q3now\\gametype\\0\\minping\\0\\maxping\\0").encode()
ch=[r["command"].split(" ",1)[1] for r in requests if r["role"]=="endpoint"]
if len(ch)!=3 or len(set(ch))!=3 or any(re.fullmatch(r"[0-9a-f]{32}",x) is None for x in ch):raise SystemExit("FAIL fixture challenges")
expected=[master(),info(ch[0],"DIRECT MUST NOT ENTER CACHE"),info(ch[1],"GLOBAL OWNER 1"),master(),info(ch[2],"GLOBAL OWNER 2")]
for index,(row,wire) in enumerate(zip(responses,expected)):
 try:observed=bytes.fromhex(row.get("hex",""))
 except ValueError:raise SystemExit("FAIL response hex")
 if observed!=wire or row.get("length")!=len(wire) or row.get("sha256")!=hashlib.sha256(wire).hexdigest():raise SystemExit("FAIL response bytes")
 expected_port=q["master_port"] if row["scenario"].startswith("master_") else q["endpoint_port"]
 if row.get("source_port")!=expected_port or row.get("peer_port") not in peer or not isinstance(row.get("elapsed_ms"),int) or not isinstance(row.get("rtt_ms"),int) or row["rtt_ms"]<0:raise SystemExit("FAIL response identity")
times=[r["elapsed_ms"] for r in responses]
if times!=sorted(times) or responses[1]["rtt_ms"]>=800 or not times[1]<times[2]:raise SystemExit("FAIL response timing/order")
stopped=[r for r in f if r.get("event")=="stopped"][0]
if stopped.get("reason")!="signal" or stopped.get("master_count")!=2 or stopped.get("info_count")!=3 or stopped.get("direct_sent") is not True:raise SystemExit("FAIL fixture stop")

def norm(s):
 if s.endswith("\n"):s=s[:-1]
 return s
critical=("Ping transaction started ","Ping request generation=","Accepted ping infoResponse ","Ping result consumed ","Ping transaction cleared ","Ping transaction retired ","Ping owner refresh ","Ping browser scheduling deferred ","Local discovery request ","Master query generation=","WiredUI: server roster generation=","WiredUI: server roster row=","WiredUI: server selection ","wui_menu_nav focus: ","wui_menu_nav: K_")
for r in p:
 raw=r["msg"]
 for line in re.split(r"[\r\n]",raw):
  if line and any(line.startswith(x) for x in critical) and line!=norm(raw):raise SystemExit("FAIL claimed multiline smuggle")
def fam(prefix):return [(i,r,norm(r["msg"])) for i,r in enumerate(p) if norm(r["msg"]).startswith(prefix)]
def exact(prefix,pats,sev="DEBUG",cat="client"):
 rows=fam(prefix)
 if len(rows)!=len(pats):raise SystemExit(f"FAIL {prefix} cardinality")
 for (_,r,m),pat in zip(rows,pats):
  if r["sev"]!=sev or r["cat"]!=cat or re.fullmatch(pat,m) is None:raise SystemExit(f"FAIL {prefix} exact")
 return rows
started=exact("Ping transaction started ",[rf"Ping transaction started owner=manual generation=([0-9]+) address={ee}",rf"Ping transaction started owner=browser-global generation=([0-9]+) address={ee}",rf"Ping transaction started owner=browser-global generation=([0-9]+) address={ee}"])
sm=[re.fullmatch(r"Ping transaction started owner=(?:manual|browser-global) generation=([0-9]+) address=.*",x[2]) for x in started];gens=[int(x.group(1)) for x in sm]
req=exact("Ping request generation=",[rf"Ping request generation={g} challenge={c} timeout=800ms address={ee}" for g,c in zip(gens,ch)])
masterq=exact("Master query generation=",[rf"Master query generation=([0-9]+) address={em} extended=0 timeout=3000ms"]*2,"INFO","client")
retired=exact("Ping transaction retired ",[])
refresh=exact("Ping owner refresh ",[r"Ping owner refresh owner=browser-local reason=local-refresh retired=0 retained_direct=0",r"Ping owner refresh owner=browser-local reason=local-refresh retired=0 retained_direct=1",r"Ping owner refresh owner=browser-global reason=global-refresh retired=0 retained_direct=1",r"Ping owner refresh owner=browser-local reason=local-refresh retired=0 retained_direct=1",r"Ping owner refresh owner=browser-global reason=global-refresh retired=0 retained_direct=1"])
local=exact("Local discovery request ",[r"Local discovery request generation=([0-9]+) challenge=([0-9a-f]{32}) timeout=3000ms"]*3)
local_gen=[int(re.fullmatch(r"Local discovery request generation=([0-9]+).*",x[2]).group(1)) for x in local]
if gens != [local_gen[0]+1,local_gen[0]+3,local_gen[0]+5] or local_gen != [local_gen[0],local_gen[0]+2,local_gen[0]+4]:raise SystemExit("FAIL shared generation chain")
exact("Ping browser scheduling deferred ",[])
accepted=exact("Accepted ping infoResponse ",[rf"Accepted ping infoResponse generation={gens[0]} time=([1-9][0-9]{{0,2}})ms address={ee}",rf"Accepted ping infoResponse generation={gens[1]} time=([1-9][0-9]{{0,2}})ms address={ee}",rf"Accepted ping infoResponse generation={gens[2]} time=([1-9][0-9]{{0,2}})ms address={ee}"])
accepted_time=[int(re.fullmatch(r"Accepted ping infoResponse generation=[0-9]+ time=([0-9]+)ms address=.*",x[2]).group(1)) for x in accepted]
if any(not 1<=x<800 for x in accepted_time):raise SystemExit("FAIL accepted deadline")
consumed=exact("Ping result consumed ",[rf"Ping result consumed slot=[0-9]+ owner=browser-global generation={gens[1]} outcome=completed time={accepted_time[1]}ms cache_action=publish matched=1 consumer=current address={ee}",rf"Ping result consumed slot=[0-9]+ owner=browser-global generation={gens[2]} outcome=completed time={accepted_time[2]}ms cache_action=publish matched=1 consumer=current address={ee}"])
cleared=exact("Ping transaction cleared ",[rf"Ping transaction cleared slot=[0-9]+ owner=browser-global generation={gens[1]} state=completed age=[0-9]+ms address={ee}",rf"Ping transaction cleared slot=[0-9]+ owner=browser-global generation={gens[2]} state=completed age=[0-9]+ms address={ee}"])
if any("owner=manual" in x[2] for x in consumed+cleared+retired):raise SystemExit("FAIL manual consumed/retired")
rows=exact("WiredUI: server roster row=",[rf"WiredUI: server roster row=0 raw=0 address={ee} name=GLOBAL OWNER 1 map=arena7",rf"WiredUI: server roster row=0 raw=0 address={ee} name=GLOBAL OWNER 2 map=arena7"],"DEBUG","ui")
rosters=exact("WiredUI: server roster generation=",[r"WiredUI: server roster generation=([0-9]+) source=0 displayed=0 raw=0",r"WiredUI: server roster generation=([0-9]+) source=1 displayed=0 raw=0",r"WiredUI: server roster generation=([0-9]+) source=1 displayed=0 raw=1",r"WiredUI: server roster generation=([0-9]+) source=1 displayed=1 raw=1",r"WiredUI: server roster generation=([0-9]+) source=1 displayed=0 raw=0",r"WiredUI: server roster generation=([0-9]+) source=1 displayed=0 raw=1",r"WiredUI: server roster generation=([0-9]+) source=1 displayed=1 raw=1"],"DEBUG","ui")
roster_gen=[int(re.fullmatch(r"WiredUI: server roster generation=([0-9]+).*",x[2]).group(1)) for x in rosters]
if roster_gen!=list(range(roster_gen[0],roster_gen[0]+7)):raise SystemExit("FAIL roster generation chain")
if any("DIRECT MUST NOT ENTER CACHE" in r["msg"] for r in p):raise SystemExit("FAIL direct metadata cached")
selections=exact("WiredUI: server selection ",[rf"WiredUI: server selection display_row=0 raw=0 source=1 list_generation={roster_gen[-1]} selection_generation=[0-9]+ address={ee} name=GLOBAL OWNER 2 map=arena7"],"DEBUG","ui")
focus=exact("wui_menu_nav focus: focused item ",[r"wui_menu_nav focus: focused item 'filter_source' \(top index -?[0-9]+\)",r"wui_menu_nav focus: focused item 'btn_refresh' \(top index -?[0-9]+\)",r"wui_menu_nav focus: focused item 'btn_refresh' \(top index -?[0-9]+\)",r"wui_menu_nav focus: focused item 'serverlist' \(top index -?[0-9]+\)"],"DEBUG","ui")
nav_enter=exact("wui_menu_nav: K_ENTER dispatched",[r"wui_menu_nav: K_ENTER dispatched"]*3,"DEBUG","ui")
nav_down=exact("wui_menu_nav: K_DOWNARROW dispatched",[r"wui_menu_nav: K_DOWNARROW dispatched"],"DEBUG","ui")
nav_back=exact("wui_menu_nav: K_ESCAPE dispatched",[r"wui_menu_nav: K_ESCAPE dispatched"],"DEBUG","ui")
if not (refresh[0][0]<local[0][0]<rosters[0][0]<focus[0][0]<nav_enter[0][0]<focus[1][0]<started[0][0]<req[0][0]<nav_enter[1][0]<refresh[1][0]<local[1][0]<refresh[2][0]<masterq[0][0]<rosters[1][0]<accepted[0][0]<rosters[2][0]<started[1][0]<req[1][0]<accepted[1][0]<consumed[0][0]<cleared[0][0]<rosters[3][0]<rows[0][0]<focus[2][0]<nav_enter[2][0]<refresh[3][0]<local[2][0]<refresh[4][0]<masterq[1][0]<rosters[4][0]<rosters[5][0]<started[2][0]<req[2][0]<accepted[2][0]<consumed[1][0]<cleared[1][0]<rosters[6][0]<rows[1][0]<selections[0][0]<focus[3][0]<nav_down[0][0]<nav_back[0][0]):raise SystemExit("FAIL causal order")
print("  PASS ping owner: direct survives global refresh; browser consumer solely publishes source cache")
PYEOF
}

if [ "${1:-}" = --self-test ]; then
 ROOT="$(mktemp -d -t ping-owner-self-XXXXXX 2>/dev/null || mktemp -d)"; trap 'rm -rf "$ROOT"' EXIT
 P="$ROOT/p" F="$ROOT/f"
 python3 - "$P" "$F" <<'PYEOF'
import hashlib,json,socket,struct,sys
p,f=sys.argv[1:3];ep="127.0.0.1:30002";mp="127.0.0.1:30001";ch=[x*32 for x in "123"]
msgs=["Ping owner refresh owner=browser-local reason=local-refresh retired=0 retained_direct=0","Local discovery request generation=7 challenge="+"4"*32+" timeout=3000ms","WiredUI: server roster generation=10 source=0 displayed=0 raw=0","WiredUI: server roster generation=11 source=1 displayed=0 raw=0",f"Ping transaction started owner=manual generation=8 address={ep}",f"Ping request generation=8 challenge={ch[0]} timeout=800ms address={ep}","Ping owner refresh owner=browser-local reason=local-refresh retired=0 retained_direct=1","Local discovery request generation=9 challenge="+"5"*32+" timeout=3000ms","Ping owner refresh owner=browser-global reason=global-refresh retired=0 retained_direct=1",f"Master query generation=2 address={mp} extended=0 timeout=3000ms",f"Accepted ping infoResponse generation=8 time=25ms address={ep}","WiredUI: server roster generation=12 source=1 displayed=0 raw=1",f"Ping transaction started owner=browser-global generation=10 address={ep}",f"Ping request generation=10 challenge={ch[1]} timeout=800ms address={ep}",f"Accepted ping infoResponse generation=10 time=20ms address={ep}",f"Ping result consumed slot=1 owner=browser-global generation=10 outcome=completed time=20ms cache_action=publish matched=1 consumer=current address={ep}",f"Ping transaction cleared slot=1 owner=browser-global generation=10 state=completed age=30ms address={ep}","WiredUI: server roster generation=13 source=1 displayed=1 raw=1",f"WiredUI: server roster row=0 raw=0 address={ep} name=GLOBAL OWNER 1 map=arena7","Ping owner refresh owner=browser-local reason=local-refresh retired=0 retained_direct=1","Local discovery request generation=11 challenge="+"6"*32+" timeout=3000ms","Ping owner refresh owner=browser-global reason=global-refresh retired=0 retained_direct=1",f"Master query generation=3 address={mp} extended=0 timeout=3000ms","WiredUI: server roster generation=14 source=1 displayed=0 raw=0","WiredUI: server roster generation=15 source=1 displayed=0 raw=1",f"Ping transaction started owner=browser-global generation=12 address={ep}",f"Ping request generation=12 challenge={ch[2]} timeout=800ms address={ep}",f"Accepted ping infoResponse generation=12 time=20ms address={ep}",f"Ping result consumed slot=1 owner=browser-global generation=12 outcome=completed time=20ms cache_action=publish matched=1 consumer=current address={ep}",f"Ping transaction cleared slot=1 owner=browser-global generation=12 state=completed age=30ms address={ep}","WiredUI: server roster generation=16 source=1 displayed=1 raw=1",f"WiredUI: server roster row=0 raw=0 address={ep} name=GLOBAL OWNER 2 map=arena7",f"WiredUI: server selection display_row=0 raw=0 source=1 list_generation=16 selection_generation=3 address={ep} name=GLOBAL OWNER 2 map=arena7"]
# The active-source zero roster is published by the frame after the authored
# Local->Global refresh, not by the preceding source-filter focus action.
initial_global_zero=next(x for x in msgs if x=="WiredUI: server roster generation=11 source=1 displayed=0 raw=0")
msgs.remove(initial_global_zero)
msgs.insert(msgs.index(f"Master query generation=2 address={mp} extended=0 timeout=3000ms")+1,initial_global_zero)
def before(prefix,*values):
 i=next(i for i,x in enumerate(msgs) if x.startswith(prefix));msgs[i:i]=values
def after(prefix,*values):
 i=next(i for i,x in enumerate(msgs) if x.startswith(prefix));msgs[i+1:i+1]=values
after("WiredUI: server roster generation=10","wui_menu_nav focus: focused item 'filter_source' (top index -1)","wui_menu_nav: K_ENTER dispatched")
before("Ping transaction started owner=manual","wui_menu_nav focus: focused item 'btn_refresh' (top index -1)")
after("Ping request generation=8","wui_menu_nav: K_ENTER dispatched")
refresh_indexes=[i for i,x in enumerate(msgs) if x.startswith("Ping owner refresh owner=browser-local reason=local-refresh retired=0 retained_direct=1")]
msgs[refresh_indexes[1]:refresh_indexes[1]]=["wui_menu_nav focus: focused item 'btn_refresh' (top index -1)","wui_menu_nav: K_ENTER dispatched"]
msgs.extend(["wui_menu_nav focus: focused item 'serverlist' (top index -1)","wui_menu_nav: K_DOWNARROW dispatched","wui_menu_nav: K_ESCAPE dispatched"])
with open(p,"w") as o:
 for m in msgs:o.write(json.dumps({"sev":"INFO" if m.startswith("Master query") else "DEBUG","cat":"ui" if m.startswith(("WiredUI:","wui_")) else "client","msg":m})+"\n")
O=b"\xff"*4
def master():return O+b"getserversResponse\\"+socket.inet_aton("127.0.0.1")+struct.pack("!H",30002)+b"\\EOT"
def info(c,n):return O+b"infoResponse\n"+(f"\\challenge\\{c}\\protocol\\74\\hostname\\{n}\\mapname\\arena7\\clients\\1\\sv_maxclients\\8\\game\\q3now\\gametype\\0\\minping\\0\\maxping\\0").encode()
req=[("endpoint",f"getinfo {ch[0]}",1),("master","getservers q3now 74",1),("endpoint",f"getinfo {ch[1]}",2),("master","getservers q3now 74",2),("endpoint",f"getinfo {ch[2]}",3)]
wires=[("master_1",master(),"",1,30001),("direct_before_global",info(ch[0],"DIRECT MUST NOT ENTER CACHE"),ch[0],0,30002),("global_1",info(ch[1],"GLOBAL OWNER 1"),ch[1],2,30002),("master_2",master(),"",2,30001),("global_2",info(ch[2],"GLOBAL OWNER 2"),ch[2],3,30002)]
with open(f,"w") as o:
 o.write(json.dumps({"event":"ready","protocol":74,"master_port":30001,"endpoint_port":30002})+"\n")
 def write_req(index):
  role,cmd,n=req[index];o.write(json.dumps({"event":"request","role":role,"command":cmd,"peer_port":30100,"ordinal":n})+"\n")
 def write_wire(index):
  scenario,wire,c,n,port=wires[index];o.write(json.dumps({"event":"response","scenario":scenario,"challenge":c,"ordinal":n,"source_port":port,"peer_port":30100,"length":len(wire),"sha256":hashlib.sha256(wire).hexdigest(),"hex":wire.hex(),"elapsed_ms":100+index,"rtt_ms":20})+"\n")
 write_req(0);o.write(json.dumps({"event":"direct_held","challenge":ch[0],"peer_port":30100})+"\n")
 write_req(1);write_wire(0);write_wire(1);write_req(2);write_wire(2);write_req(3);write_wire(3);write_req(4);write_wire(4)
 o.write(json.dumps({"event":"stopped","reason":"signal","master_count":2,"info_count":3,"direct_sent":True})+"\n")
PYEOF
 analyze "$P" "$F" >/dev/null || { echo 'FAIL clean'; exit 1; }
 defects=(owner_zero direct_consumed direct_retired eager_direct_cache missing_consume wrong_cache_action missing_clear refresh_manual retained_direct local_generation roster_missing roster_additive roster_source roster_order wrong_row extra_row wrong_selection generation_gap request_timeout accepted_deadline consumed_time request_order fixture_protocol fixture_bytes fixture_order fixture_peer master_source_port info_source_port direct_deadline timing_order accepted_order causal_smuggle)
 for d in "${defects[@]}"; do
  D="$ROOT/$d"; mkdir "$D"; cp "$P" "$D/p"; cp "$F" "$D/f"
  python3 - "$D/p" "$D/f" "$d" <<'PYEOF'
import json,re,sys
pp,fp,d=sys.argv[1:];p=[json.loads(x) for x in open(pp)];f=[json.loads(x) for x in open(fp)]
def rows(prefix):return [r for r in p if r["msg"].startswith(prefix)]
def row(prefix,n=0):return rows(prefix)[n]
if d=="owner_zero":row("Ping transaction started owner=manual")["msg"]=row("Ping transaction started owner=manual")["msg"].replace("owner=manual","owner=none")
elif d=="direct_consumed":p.insert(-1,{"sev":"DEBUG","cat":"client","msg":"Ping result consumed slot=0 owner=manual generation=7 outcome=completed time=50ms cache_action=none matched=0 consumer=browser address=127.0.0.1:30002"})
elif d=="direct_retired":p.insert(10,{"sev":"DEBUG","cat":"client","msg":"Ping transaction retired slot=0 owner=manual generation=7 reason=global-refresh state=pending age=40ms address=127.0.0.1:30002"})
elif d=="eager_direct_cache":p.append({"sev":"DEBUG","cat":"ui","msg":"DIRECT MUST NOT ENTER CACHE"})
elif d=="missing_consume":p.remove(row("Ping result consumed "))
elif d=="wrong_cache_action":row("Ping result consumed ")["msg"]=row("Ping result consumed ")["msg"].replace("publish","none")
elif d=="missing_clear":p.remove(row("Ping transaction cleared "))
elif d=="refresh_manual":p.insert(10,{"sev":"DEBUG","cat":"client","msg":"Ping transaction retired slot=0 owner=manual generation=7 reason=global-refresh state=pending age=40ms address=127.0.0.1:30002"})
elif d=="retained_direct":row("Ping owner refresh ",1)["msg"]=row("Ping owner refresh ",1)["msg"].replace("retained_direct=1","retained_direct=0")
elif d=="local_generation":row("Local discovery request ",1)["msg"]=re.sub(r"generation=[0-9]+","generation=99",row("Local discovery request ",1)["msg"])
elif d=="roster_missing":p.remove(row("WiredUI: server roster generation=",2))
elif d=="roster_additive":p.append(dict(row("WiredUI: server roster generation=")))
elif d=="roster_source":row("WiredUI: server roster generation=",3)["msg"]=row("WiredUI: server roster generation=",3)["msg"].replace("source=1","source=0")
elif d=="roster_order":
 a=p.index(row("WiredUI: server roster generation=",2));b=p.index(row("WiredUI: server roster generation=",3));p[a],p[b]=p[b],p[a]
elif d=="wrong_row":row("WiredUI: server roster row=")["msg"]=row("WiredUI: server roster row=")["msg"].replace("GLOBAL OWNER 1","DIRECT MUST NOT ENTER CACHE")
elif d=="extra_row":p.append(dict(row("WiredUI: server roster row=")))
elif d=="wrong_selection":row("WiredUI: server selection ")["msg"]=row("WiredUI: server selection ")["msg"].replace("GLOBAL OWNER 2","GLOBAL OWNER 1")
elif d=="generation_gap":row("Ping transaction started owner=browser-global",1)["msg"]=re.sub(r"generation=[0-9]+","generation=99",row("Ping transaction started owner=browser-global",1)["msg"])
elif d=="request_timeout":row("Ping request generation=")["msg"]=row("Ping request generation=")["msg"].replace("timeout=800ms","timeout=9999ms")
elif d=="accepted_deadline":row("Accepted ping infoResponse ")["msg"]=re.sub(r"time=[0-9]+ms","time=9999ms",row("Accepted ping infoResponse ")["msg"])
elif d=="consumed_time":row("Ping result consumed ")["msg"]=re.sub(r"time=[0-9]+ms","time=777ms",row("Ping result consumed ")["msg"])
elif d=="request_order":f[1],f[2]=f[2],f[1]
elif d=="fixture_protocol":f[0]["protocol"]=75
elif d=="fixture_bytes":next(x for x in f if x.get("event")=="response")["hex"]="00"
elif d=="fixture_order":
 rows=[x for x in f if x.get("event")=="response"];rows[0]["scenario"]="master_2"
elif d=="fixture_peer":next(x for x in f if x.get("event")=="request")["peer_port"]=9
elif d=="master_source_port":next(x for x in f if x.get("scenario")=="master_1")["source_port"]=30002
elif d=="info_source_port":next(x for x in f if x.get("scenario")=="global_1")["source_port"]=30001
elif d=="direct_deadline":next(x for x in f if x.get("scenario")=="direct_before_global")["rtt_ms"]=800
elif d=="timing_order":next(x for x in f if x.get("scenario")=="master_2")["elapsed_ms"]=50
elif d=="accepted_order":
 a=p.index(row("Accepted ping infoResponse "));b=p.index(row("Ping result consumed "));p[a],p[b]=p[b],p[a]
elif d=="causal_smuggle":p.append({"sev":"INFO","cat":"system","msg":"benign\rPing transaction cleared slot=1 owner=browser-global generation=9 state=completed age=30ms address=127.0.0.1:30002"})
open(pp,"w").writelines(json.dumps(x)+"\n" for x in p);open(fp,"w").writelines(json.dumps(x)+"\n" for x in f)
PYEOF
  if analyze "$D/p" "$D/f" >/dev/null 2>&1; then echo "FAIL self $d"; exit 1; fi
 done
 echo "==> ping owner analyzer self-test: PASS (${#defects[@]} mutations)"; exit 0
fi

command -v python3 >/dev/null 2>&1 && [ -f "$FIXTURE" ] && [ -f "$TIMEOUT_RUNNER" ] || { echo 'SKIP: Python unavailable'; exit 77; }
WIRED="${1:-}"; [ -n "$WIRED" ] && [ -x "$WIRED" ] || { echo 'SKIP: pass assembled Wired GUI binary'; exit 77; }
WIRED="$(cd "$(dirname "$WIRED")" && pwd)/$(basename "$WIRED")"; WD="$(dirname "$WIRED")"
PACK=""; for c in "$WD" "$WD/../Resources" "$WD/../../.."; do [ -f "$c/base/pax21.sw3z" ] && PACK="$(cd "$c" && pwd)" && break; done
[ -n "$PACK" ] || { echo 'SKIP: pax21 missing'; exit 77; }; BASE="${WIRED_CONTENT_ROOT:-$PACK}"
if [ -f "$BASE/base/pax01.sw3z" ]; then ARCH="$BASE/base/pax01.sw3z"; elif [ -f "$BASE/base/pak0.pk3" ]; then ARCH="$BASE/base/pak0.pk3"; else echo 'SKIP: base archive missing'; exit 77; fi
ROOT="$(mktemp -d -t ping-owner-XXXXXX 2>/dev/null || mktemp -d)"; HOME_DIR="$ROOT/q3now-preview"; EVENTS="$ROOT/fixture.jsonl"; STDOUT="$ROOT/stdout"; PID=""
cleanup(){ [ -n "$PID" ] && kill -TERM "$PID" 2>/dev/null || true; [ -n "$PID" ] && wait "$PID" 2>/dev/null || true; [ "${WIRED_KEEP_ARTIFACTS:-0}" = 1 ] || rm -rf "$ROOT"; }; trap cleanup EXIT INT TERM
mkdir -p "$HOME_DIR/base"; cp "$ARCH" "$HOME_DIR/base/" || exit 1; cp "$PACK/base/pax21.sw3z" "$HOME_DIR/base/pax21.sw3z" || exit 1
python3 "$FIXTURE" --events "$EVENTS" --timeout 90 & PID=$!; for _ in $(seq 1 100); do [ -s "$EVENTS" ] && break; sleep .05; done; [ -s "$EVENTS" ] || { echo 'FAIL fixture ready'; exit 1; }
read -r MASTER_PORT ENDPOINT_PORT <<EOF
$(python3 - "$EVENTS" <<'PYEOF'
import json,sys
r=json.loads(open(sys.argv[1]).readline());print(r["master_port"],r["endpoint_port"])
PYEOF
)
EOF
cat >"$HOME_DIR/base/ping-owner.cfg" <<CFGEOF
wait 100
wui_push servers
wait 20
wui_menu_nav focus filter_source
wui_menu_nav enter
wui_menu_nav focus btn_refresh
ping 127.0.0.1:$ENDPOINT_PORT
wui_menu_nav enter
waitms 1800
echo PING_OWNER_G1
wui_menu_nav focus btn_refresh
wui_menu_nav enter
waitms 2800
wui_menu_nav focus serverlist
wui_menu_nav down
wait 10
wui_menu_nav back
wait 10
quit
CFGEOF
case "$(uname -s)" in Darwin) NATIVE="$HOME_DIR"; ARGS=(-ApplePersistenceIgnoreState YES);; MINGW*|MSYS*|CYGWIN*) NATIVE="$(cygpath -w "$HOME_DIR")"; ARGS=();; *) NATIVE="$HOME_DIR"; ARGS=();; esac
python3 "$TIMEOUT_RUNNER" --timeout 90 --kill-after 10 --cwd "$WD" --stdout "$STDOUT" -- "$WIRED" "${ARGS[@]}" +set fs_homepath "$NATIVE" +set com_automated 1 +set com_noHardReboot 1 +set net_enabled 1 +set net_ip 127.0.0.1 +set net_port 0 +set sv_master1 "127.0.0.1:$MASTER_PORT" +set s_initsound 0 +set r_fullscreen 0 +set r_mode -1 +set r_customwidth 1280 +set r_customheight 720 +set log_severity DEBUG +set log_file_severity DEBUG +set log_file_mode overwrite_synced +exec ping-owner.cfg
rc=$?; [ "$rc" -eq 0 ] && [ -s "$HOME_DIR/qconsole.jsonl" ] || { echo "FAIL product rc=$rc"; exit 1; }
kill -TERM "$PID" 2>/dev/null || true; wait "$PID" || { echo 'FAIL fixture exit'; exit 1; }; PID=""
analyze "$HOME_DIR/qconsole.jsonl" "$EVENTS" || exit 1
echo '==> ping owner browser gate: PASS'
