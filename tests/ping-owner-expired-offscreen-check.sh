#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors
# An expired Global transaction is cleared while Local is active; a fresh
# authored Global generation then recovers the row through the current owner.
set -uo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
. "$SCRIPT_DIR/lib/wired_paths.sh"
FIXTURE="$SCRIPT_DIR/ping-owner-lifecycle-fixture.py"
TIMEOUT_RUNNER="$SCRIPT_DIR/run-with-timeout.py"

analyze() {
python3 - "$1" "$2" <<'PYEOF'
import hashlib,json,re,socket,struct,sys
pp,fp=sys.argv[1:3]
def load(path):
 out=[]
 for n,line in enumerate(open(path,encoding="utf-8",errors="replace"),1):
  try:r=json.loads(line)
  except Exception as e:raise SystemExit(f"FAIL {path}:{n}: {e}")
  if not isinstance(r,dict):raise SystemExit("FAIL schema")
  out.append(r)
 return out
p,f=load(pp),load(fp)
if not p or not f or any(not all(isinstance(r.get(k),str) for k in ("sev","cat","msg")) for r in p):raise SystemExit("FAIL evidence")
if any(r["sev"] in ("ERROR","FATAL") or r["sev"]=="WARN" and r["cat"]=="ui" for r in p):raise SystemExit("FAIL severity")
ready=[r for r in f if r.get("event")=="ready"]
if len(ready)!=1 or ready[0].get("mode")!="expired" or ready[0].get("protocol")!=74:raise SystemExit("FAIL ready")
q=ready[0]
for key in ("master_port","browser_port","fill_port"):
 if not isinstance(q.get(key),int) or not 0<q[key]<65536:raise SystemExit("FAIL port")
expected_events=["ready","request","response","request","browser_held","request","response","request","response","stopped"]
if [r.get("event") for r in f]!=expected_events:raise SystemExit("FAIL fixture lifecycle")
req=[r for r in f if r.get("event")=="request"]
if [(r.get("role"),r.get("ordinal")) for r in req] != [("master",1),("browser",1),("master",2),("browser",2)]:raise SystemExit("FAIL request roles")
if req[0].get("command")!="getservers q3now 74" or req[2].get("command")!="getservers q3now 74":raise SystemExit("FAIL master command")
if not all(re.fullmatch(r"getinfo [0-9a-f]{32}",req[i].get("command","")) for i in (1,3)):raise SystemExit("FAIL info command")
client_ports={r.get("source_port") for r in req}
if len(client_ports)!=1 or not all(r.get("peer_port")==q[f"{r['role']}_port"] for r in req):raise SystemExit("FAIL request identity")
if not all(isinstance(r.get("elapsed_ms"),int) for r in req) or [r["elapsed_ms"] for r in req]!=sorted(r["elapsed_ms"] for r in req):raise SystemExit("FAIL request chronology")
g1=req[1]["command"].split(" ",1)[1];g2=req[3]["command"].split(" ",1)[1]
if g1==g2:raise SystemExit("FAIL challenge freshness")
generation_gap=req[2]["elapsed_ms"]-req[1]["elapsed_ms"]
if not 999<=generation_gap<3000:raise SystemExit("FAIL fixture generation deadline")
held=[r for r in f if r.get("event")=="browser_held"]
if len(held)!=1 or held[0].get("challenge")!=g1 or held[0].get("peer_port") not in client_ports:raise SystemExit("FAIL held identity")
responses=[r for r in f if r.get("event")=="response"]
if [r.get("scenario") for r in responses] != ["master","master","browser_recovery"]:raise SystemExit("FAIL responses")
O=b"\xff"*4
master=O+b"getserversResponse\\"+socket.inet_aton("127.0.0.1")+struct.pack("!H",q["browser_port"])+b"\\EOT"
info=O+b"infoResponse\n"+(f"\\challenge\\{g2}\\protocol\\74\\hostname\\EXPIRED RECOVERY RESULT\\mapname\\arena7\\clients\\1\\sv_maxclients\\8\\game\\q3now\\gametype\\0\\minping\\0\\maxping\\0").encode()
for row,wire,port in zip(responses,(master,master,info),(q["master_port"],q["master_port"],q["browser_port"])):
 try:observed=bytes.fromhex(row.get("hex",""))
 except ValueError:raise SystemExit("FAIL response hex")
 if observed!=wire or row.get("length")!=len(wire) or row.get("sha256")!=hashlib.sha256(wire).hexdigest() or row.get("source_port")!=port or row.get("peer_port") not in client_ports:raise SystemExit("FAIL response authority")
if not 0<=responses[2].get("rtt_ms",-1)<999 or responses[1].get("elapsed_ms",-1)>responses[2].get("elapsed_ms",-1):raise SystemExit("FAIL recovery timing")
stop=f[-1]
if stop.get("reason")!="signal" or stop.get("master_count")!=2 or stop.get("browser_count")!=2 or stop.get("fill_count")!=0 or stop.get("browser_sent") is not True:raise SystemExit("FAIL stop")

def norm(s):return s[:-1] if s.endswith("\n") else s
critical=("Ping transaction started ","Ping request generation=","Accepted ping infoResponse ","Ping result consumed ","Ping transaction cleared ","Ping transaction retired ","Ping owner refresh ","Ping queue allocation ","Ping browser scheduling deferred ","Local discovery request ","Ignored expired ping infoResponse ","Ignored infoResponse challenge mismatch ","Ignored unsolicited infoResponse ","Ignored non-LAN local discovery infoResponse ","Rejected overlong infoResponse ","Rejected full infoResponse ","Rejected invalid infoResponse ","Accepted local discovery infoResponse ","Master query generation=","WiredUI: server ping tick ","WiredUI: server roster generation=","WiredUI: server roster row=","WiredUI: server selection ","WiredUI: push menu ","WiredUI: pop menu ","wui_menu_nav focus: ","wui_menu_nav: K_")
for r in p:
 raw=r["msg"]
 for line in re.split(r"[\r\n]",raw):
  if line and any(line.startswith(x) for x in critical) and line!=norm(raw):raise SystemExit("FAIL multiline claim")
def fam(prefix):return [(i,r,norm(r["msg"])) for i,r in enumerate(p) if norm(r["msg"]).startswith(prefix)]
def exact(prefix,pats,sev="DEBUG",cat="client"):
 rows=fam(prefix)
 if len(rows)!=len(pats):raise SystemExit(f"FAIL {prefix} cardinality")
 for (_,r,m),pat in zip(rows,pats):
  if r["sev"]!=sev or r["cat"]!=cat or re.fullmatch(pat,m) is None:raise SystemExit(f"FAIL {prefix} exact")
 return rows
eb=re.escape(f"127.0.0.1:{q['browser_port']}");em=re.escape(f"127.0.0.1:{q['master_port']}")
started=exact("Ping transaction started ",[rf"Ping transaction started owner=browser-global generation=([1-9][0-9]*) address={eb}"]*2)
gens=[int(re.fullmatch(r".*generation=([0-9]+) address=.*",x[2]).group(1)) for x in started]
local=exact("Local discovery request ",[r"Local discovery request generation=([1-9][0-9]*) challenge=[0-9a-f]{32} timeout=3000ms"]*3)
local_gen=[int(re.fullmatch(r"Local discovery request generation=([0-9]+).*",x[2]).group(1)) for x in local]
if local_gen!=[local_gen[0],local_gen[0]+1,local_gen[0]+3] or gens!=[local_gen[0]+2,local_gen[0]+4]:raise SystemExit("FAIL shared generation chain")
requests=exact("Ping request generation=",[rf"Ping request generation={gens[0]} challenge={g1} timeout=999ms address={eb}",rf"Ping request generation={gens[1]} challenge={g2} timeout=999ms address={eb}"])
masterq=exact("Master query generation=",[rf"Master query generation=([1-9][0-9]*) address={em} extended=0 timeout=3000ms"]*2,"INFO")
master_gen=[int(re.fullmatch(r"Master query generation=([0-9]+).*",x[2]).group(1)) for x in masterq]
if master_gen[1]!=master_gen[0]+1:raise SystemExit("FAIL master generation")
dispatch=exact("WiredUI: server ping tick dispatch ",[r"WiredUI: server ping tick dispatch source=1 engine_source=2",r"WiredUI: server ping tick dispatch source=0 engine_source=0",r"WiredUI: server ping tick dispatch source=1 engine_source=2",r"WiredUI: server ping tick dispatch source=1 engine_source=2"],"DEBUG","ui")
ticks=exact("WiredUI: server ping tick source=",[r"WiredUI: server ping tick source=1 engine_source=2 work=1",r"WiredUI: server ping tick source=0 engine_source=0 work=0",r"WiredUI: server ping tick source=1 engine_source=2 work=1",r"WiredUI: server ping tick source=1 engine_source=2 work=1"],"DEBUG","ui")
accepted=exact("Accepted ping infoResponse ",[rf"Accepted ping infoResponse generation={gens[1]} time=([1-9][0-9]{{0,2}})ms address={eb}"])
at=int(re.fullmatch(r".*time=([0-9]+)ms address=.*",accepted[0][2]).group(1))
if at>=999:raise SystemExit("FAIL recovery deadline")
consumed=exact("Ping result consumed ",[rf"Ping result consumed slot=0 owner=browser-global generation={gens[0]} outcome=expired time=([1-9][0-9]{{2,}})ms cache_action=clear matched=1 consumer=offscreen address={eb}",rf"Ping result consumed slot=0 owner=browser-global generation={gens[1]} outcome=completed time={at}ms cache_action=publish matched=1 consumer=current address={eb}"])
expired_time=int(re.fullmatch(r".*outcome=expired time=([0-9]+)ms.*",consumed[0][2]).group(1))
if expired_time<999:raise SystemExit("FAIL expired deadline")
cleared=exact("Ping transaction cleared ",[rf"Ping transaction cleared slot=0 owner=browser-global generation={gens[0]} state=expired age=([1-9][0-9]{{2,}})ms address={eb}",rf"Ping transaction cleared slot=0 owner=browser-global generation={gens[1]} state=completed age=([1-9][0-9]*)ms address={eb}"])
ages=[int(re.fullmatch(r".* age=([0-9]+)ms address=.*",x[2]).group(1)) for x in cleared]
if expired_time>=3000 or ages[0]<expired_time or ages[0]>=3000 or ages[1]<at:raise SystemExit("FAIL clear age")
exact("Ping transaction retired ",[]);exact("Ping queue allocation ",[]);exact("Ping browser scheduling deferred ",[])
for forbidden in ("Ignored expired ping infoResponse ","Ignored infoResponse challenge mismatch ","Ignored unsolicited infoResponse ","Ignored non-LAN local discovery infoResponse ","Rejected overlong infoResponse ","Rejected full infoResponse ","Rejected invalid infoResponse ","Accepted local discovery infoResponse "):
 exact(forbidden,[])
refresh=exact("Ping owner refresh ",[r"Ping owner refresh owner=browser-local reason=local-refresh retired=0 retained_direct=0",r"Ping owner refresh owner=browser-local reason=local-refresh retired=0 retained_direct=0",r"Ping owner refresh owner=browser-global reason=global-refresh retired=0 retained_direct=0",r"Ping owner refresh owner=browser-local reason=local-refresh retired=0 retained_direct=0",r"Ping owner refresh owner=browser-global reason=global-refresh retired=0 retained_direct=0"])
rosters=exact("WiredUI: server roster generation=",[r"WiredUI: server roster generation=([1-9][0-9]*) source=0 displayed=0 raw=0",r"WiredUI: server roster generation=([1-9][0-9]*) source=1 displayed=0 raw=0",r"WiredUI: server roster generation=([1-9][0-9]*) source=1 displayed=0 raw=1",r"WiredUI: server roster generation=([1-9][0-9]*) source=1 displayed=0 raw=1",r"WiredUI: server roster generation=([1-9][0-9]*) source=1 displayed=0 raw=0",r"WiredUI: server roster generation=([1-9][0-9]*) source=1 displayed=0 raw=1",r"WiredUI: server roster generation=([1-9][0-9]*) source=1 displayed=1 raw=1"],"DEBUG","ui")
rg=[int(re.fullmatch(r"WiredUI: server roster generation=([0-9]+).*",x[2]).group(1)) for x in rosters]
if rg!=list(range(rg[0],rg[0]+7)):raise SystemExit("FAIL roster generations")
rows=exact("WiredUI: server roster row=",[rf"WiredUI: server roster row=0 raw=0 address={eb} name=EXPIRED RECOVERY RESULT map=arena7"],"DEBUG","ui")
selection=exact("WiredUI: server selection ",[rf"WiredUI: server selection display_row=0 raw=0 source=1 list_generation={rg[-1]} selection_generation=[1-9][0-9]* address={eb} name=EXPIRED RECOVERY RESULT map=arena7"],"DEBUG","ui")
focus=exact("wui_menu_nav focus: focused item ",[r"wui_menu_nav focus: focused item 'filter_source' \(top index -?[0-9]+\)",r"wui_menu_nav focus: focused item 'btn_refresh' \(top index -?[0-9]+\)",r"wui_menu_nav focus: focused item 'filter_source' \(top index -?[0-9]+\)",r"wui_menu_nav focus: focused item 'filter_source' \(top index -?[0-9]+\)",r"wui_menu_nav focus: focused item 'btn_refresh' \(top index -?[0-9]+\)",r"wui_menu_nav focus: focused item 'serverlist' \(top index -?[0-9]+\)"],"DEBUG","ui")
enters=exact("wui_menu_nav: K_ENTER dispatched",[r"wui_menu_nav: K_ENTER dispatched"]*5,"DEBUG","ui")
down=exact("wui_menu_nav: K_DOWNARROW dispatched",[r"wui_menu_nav: K_DOWNARROW dispatched"],"DEBUG","ui")
back=exact("wui_menu_nav: K_ESCAPE dispatched",[r"wui_menu_nav: K_ESCAPE dispatched"]*2,"DEBUG","ui")
push=exact("WiredUI: push menu ",[r"WiredUI: push menu 'servers' \(depth 1\)",r"WiredUI: push menu 'main' \(depth 2\)"],"DEBUG","ui")
pop=exact("WiredUI: pop menu ",[r"WiredUI: pop menu \(depth 1\)",r"WiredUI: pop menu \(depth 0\)"],"DEBUG","ui")
if not (push[0][0]<refresh[0][0]<local[0][0]<rosters[0][0]<focus[0][0]<enters[0][0]<focus[1][0]<enters[1][0]<refresh[1][0]<local[1][0]<refresh[2][0]<masterq[0][0]<rosters[1][0]<rosters[2][0]<dispatch[0][0]<started[0][0]<requests[0][0]<ticks[0][0]<focus[2][0]<enters[2][0]<push[1][0]<dispatch[1][0]<consumed[0][0]<cleared[0][0]<ticks[1][0]<pop[0][0]<back[0][0]<focus[3][0]<enters[3][0]<rosters[3][0]<focus[4][0]<enters[4][0]<refresh[3][0]<local[2][0]<refresh[4][0]<masterq[1][0]<rosters[4][0]<rosters[5][0]<dispatch[2][0]<started[1][0]<requests[1][0]<ticks[2][0]<accepted[0][0]<dispatch[3][0]<consumed[1][0]<cleared[1][0]<ticks[3][0]<rosters[6][0]<rows[0][0]<selection[0][0]<focus[5][0]<down[0][0]<pop[1][0]<back[1][0]):raise SystemExit("FAIL causal order")
print("  PASS ping owner expired offscreen: expired Global cache is cleared offscreen and a fresh generation recovers")
PYEOF
}

if [ "${1:-}" = --analyze ]; then
 [ "$#" -eq 3 ] || { echo 'usage: --analyze <product.jsonl> <fixture.jsonl>'; exit 2; }
 analyze "$2" "$3"
 exit $?
fi

if [ "${1:-}" = --self-test ]; then
 ROOT="$(mktemp -d -t ping-owner-expired-self-XXXXXX 2>/dev/null || mktemp -d)"; trap 'rm -rf "$ROOT"' EXIT
 P="$ROOT/p" F="$ROOT/f"
 python3 - "$P" "$F" <<'PYEOF'
import hashlib,json,socket,struct,sys
p,f=sys.argv[1:];mp=30001;bp=30002;fp=30003;ep=f"127.0.0.1:{bp}";g1="1"*32;g2="2"*32
M=[]
def add(msg,sev="DEBUG",cat="client"):M.append({"sev":sev,"cat":cat,"msg":msg})
add("WiredUI: push menu 'servers' (depth 1)",cat="ui");add("Ping owner refresh owner=browser-local reason=local-refresh retired=0 retained_direct=0");add("Local discovery request generation=6 challenge="+"a"*32+" timeout=3000ms");add("WiredUI: server roster generation=20 source=0 displayed=0 raw=0",cat="ui");add("wui_menu_nav focus: focused item 'filter_source' (top index -1)",cat="ui");add("wui_menu_nav: K_ENTER dispatched",cat="ui");add("wui_menu_nav focus: focused item 'btn_refresh' (top index -1)",cat="ui");add("wui_menu_nav: K_ENTER dispatched",cat="ui");add("Ping owner refresh owner=browser-local reason=local-refresh retired=0 retained_direct=0");add("Local discovery request generation=7 challenge="+"b"*32+" timeout=3000ms");add("Ping owner refresh owner=browser-global reason=global-refresh retired=0 retained_direct=0");add(f"Master query generation=4 address=127.0.0.1:{mp} extended=0 timeout=3000ms","INFO");add("WiredUI: server roster generation=21 source=1 displayed=0 raw=0",cat="ui");add("WiredUI: server roster generation=22 source=1 displayed=0 raw=1",cat="ui");add("WiredUI: server ping tick dispatch source=1 engine_source=2",cat="ui");add(f"Ping transaction started owner=browser-global generation=8 address={ep}");add(f"Ping request generation=8 challenge={g1} timeout=999ms address={ep}");add("WiredUI: server ping tick source=1 engine_source=2 work=1",cat="ui");add("wui_menu_nav focus: focused item 'filter_source' (top index -1)",cat="ui");add("wui_menu_nav: K_ENTER dispatched",cat="ui");add("WiredUI: push menu 'main' (depth 2)",cat="ui");add("WiredUI: server ping tick dispatch source=0 engine_source=0",cat="ui");add(f"Ping result consumed slot=0 owner=browser-global generation=8 outcome=expired time=1100ms cache_action=clear matched=1 consumer=offscreen address={ep}");add(f"Ping transaction cleared slot=0 owner=browser-global generation=8 state=expired age=1101ms address={ep}");add("WiredUI: server ping tick source=0 engine_source=0 work=0",cat="ui");add("WiredUI: pop menu (depth 1)",cat="ui");add("wui_menu_nav: K_ESCAPE dispatched",cat="ui");add("wui_menu_nav focus: focused item 'filter_source' (top index -1)",cat="ui");add("wui_menu_nav: K_ENTER dispatched",cat="ui");add("WiredUI: server roster generation=23 source=1 displayed=0 raw=1",cat="ui");add("wui_menu_nav focus: focused item 'btn_refresh' (top index -1)",cat="ui");add("wui_menu_nav: K_ENTER dispatched",cat="ui");add("Ping owner refresh owner=browser-local reason=local-refresh retired=0 retained_direct=0");add("Local discovery request generation=9 challenge="+"c"*32+" timeout=3000ms");add("Ping owner refresh owner=browser-global reason=global-refresh retired=0 retained_direct=0");add(f"Master query generation=5 address=127.0.0.1:{mp} extended=0 timeout=3000ms","INFO");add("WiredUI: server roster generation=24 source=1 displayed=0 raw=0",cat="ui");add("WiredUI: server roster generation=25 source=1 displayed=0 raw=1",cat="ui");add("WiredUI: server ping tick dispatch source=1 engine_source=2",cat="ui");add(f"Ping transaction started owner=browser-global generation=10 address={ep}");add(f"Ping request generation=10 challenge={g2} timeout=999ms address={ep}");add("WiredUI: server ping tick source=1 engine_source=2 work=1",cat="ui");add(f"Accepted ping infoResponse generation=10 time=25ms address={ep}");add("WiredUI: server ping tick dispatch source=1 engine_source=2",cat="ui");add(f"Ping result consumed slot=0 owner=browser-global generation=10 outcome=completed time=25ms cache_action=publish matched=1 consumer=current address={ep}");add(f"Ping transaction cleared slot=0 owner=browser-global generation=10 state=completed age=110ms address={ep}");add("WiredUI: server ping tick source=1 engine_source=2 work=1",cat="ui");add("WiredUI: server roster generation=26 source=1 displayed=1 raw=1",cat="ui");add(f"WiredUI: server roster row=0 raw=0 address={ep} name=EXPIRED RECOVERY RESULT map=arena7",cat="ui");add(f"WiredUI: server selection display_row=0 raw=0 source=1 list_generation=26 selection_generation=3 address={ep} name=EXPIRED RECOVERY RESULT map=arena7",cat="ui");add("wui_menu_nav focus: focused item 'serverlist' (top index -1)",cat="ui");add("wui_menu_nav: K_DOWNARROW dispatched",cat="ui");add("WiredUI: pop menu (depth 0)",cat="ui");add("wui_menu_nav: K_ESCAPE dispatched",cat="ui")
open(p,"w").writelines(json.dumps(x)+"\n" for x in M)
O=b"\xff"*4;mw=O+b"getserversResponse\\"+socket.inet_aton("127.0.0.1")+struct.pack("!H",bp)+b"\\EOT";iw=O+b"infoResponse\n"+(f"\\challenge\\{g2}\\protocol\\74\\hostname\\EXPIRED RECOVERY RESULT\\mapname\\arena7\\clients\\1\\sv_maxclients\\8\\game\\q3now\\gametype\\0\\minping\\0\\maxping\\0").encode()
def resp(s,w,port,elapsed,rtt=0):return {"event":"response","scenario":s,"challenge":g2 if s=="browser_recovery" else "","source_port":port,"peer_port":30100,"length":len(w),"sha256":hashlib.sha256(w).hexdigest(),"hex":w.hex(),"elapsed_ms":elapsed,"rtt_ms":rtt}
E=[{"event":"ready","mode":"expired","protocol":74,"master_port":mp,"browser_port":bp,"fill_port":fp},{"event":"request","role":"master","ordinal":1,"command":"getservers q3now 74","source_port":30100,"peer_port":mp,"elapsed_ms":10},resp("master",mw,mp,11),{"event":"request","role":"browser","ordinal":1,"command":f"getinfo {g1}","source_port":30100,"peer_port":bp,"elapsed_ms":20},{"event":"browser_held","challenge":g1,"peer_port":30100},{"event":"request","role":"master","ordinal":2,"command":"getservers q3now 74","source_port":30100,"peer_port":mp,"elapsed_ms":1400},resp("master",mw,mp,1401),{"event":"request","role":"browser","ordinal":2,"command":f"getinfo {g2}","source_port":30100,"peer_port":bp,"elapsed_ms":1450},resp("browser_recovery",iw,bp,1451,1),{"event":"stopped","reason":"signal","master_count":2,"browser_count":2,"fill_count":0,"browser_sent":True}]
open(f,"w").writelines(json.dumps(x)+"\n" for x in E)
PYEOF
 analyze "$P" "$F" >/dev/null || { echo 'FAIL clean'; exit 1; }
 defects=(expired_current expired_publish expired_unmatched expired_early expired_late clear_late clear_slot accepted_g1 accepted_missing recovery_offscreen recovery_unmatched recovery_deadline stale_row extra_row missing_row wrong_row missing_selection roster_missing roster_additive roster_source roster_order roster_newline tick_missing tick_source refresh_late request_peer response_bytes fixture_mode fixture_order generation_early generation_late same_challenge extra_enter missing_focus missing_cover extra_pop ignored_expired challenge_mismatch unsolicited non_lan rejected_overlong rejected_full rejected_invalid accepted_local ingress_smuggle suffix smuggle retired)
 for d in "${defects[@]}"; do
  D="$ROOT/$d";mkdir "$D";cp "$P" "$D/p";cp "$F" "$D/f"
  python3 - "$D/p" "$D/f" "$d" <<'PYEOF'
import json,re,sys
pp,fp,d=sys.argv[1:];P=[json.loads(x) for x in open(pp)];F=[json.loads(x) for x in open(fp)]
def rows(s):return [x for x in P if x["msg"].startswith(s)]
def row(s,n=0):return rows(s)[n]
if d=="expired_current":row("Ping result consumed ")["msg"]=row("Ping result consumed ")["msg"].replace("consumer=offscreen","consumer=current")
elif d=="expired_publish":row("Ping result consumed ")["msg"]=row("Ping result consumed ")["msg"].replace("cache_action=clear","cache_action=publish")
elif d=="expired_unmatched":row("Ping result consumed ")["msg"]=row("Ping result consumed ")["msg"].replace("matched=1","matched=0")
elif d=="expired_early":row("Ping result consumed ")["msg"]=row("Ping result consumed ")["msg"].replace("time=1100ms","time=998ms")
elif d=="expired_late":row("Ping result consumed ")["msg"]=row("Ping result consumed ")["msg"].replace("time=1100ms","time=3000ms")
elif d=="clear_late":row("Ping transaction cleared ")["msg"]=row("Ping transaction cleared ")["msg"].replace("age=1101ms","age=3000ms")
elif d=="clear_slot":row("Ping transaction cleared ")["msg"]=row("Ping transaction cleared ")["msg"].replace("slot=0","slot=2")
elif d=="accepted_g1":P.insert(P.index(row("Ping result consumed ")),dict(row("Accepted ping infoResponse ")))
elif d=="accepted_missing":P.remove(row("Accepted ping infoResponse "))
elif d=="recovery_offscreen":row("Ping result consumed ",1)["msg"]=row("Ping result consumed ",1)["msg"].replace("consumer=current","consumer=offscreen")
elif d=="recovery_unmatched":row("Ping result consumed ",1)["msg"]=row("Ping result consumed ",1)["msg"].replace("matched=1","matched=0")
elif d=="recovery_deadline":row("Accepted ping infoResponse ")["msg"]=re.sub(r"time=[0-9]+ms","time=999ms",row("Accepted ping infoResponse ")["msg"])
elif d=="stale_row":P.insert(P.index(row("Ping result consumed ")),dict(row("WiredUI: server roster row=")))
elif d=="extra_row":P.append(dict(row("WiredUI: server roster row=")))
elif d=="missing_row":P.remove(row("WiredUI: server roster row="))
elif d=="wrong_row":row("WiredUI: server roster row=")["msg"]=row("WiredUI: server roster row=")["msg"].replace("EXPIRED RECOVERY RESULT","HELD G1")
elif d=="missing_selection":P.remove(row("WiredUI: server selection "))
elif d=="roster_missing":P.remove(row("WiredUI: server roster generation=",3))
elif d=="roster_additive":P.append(dict(row("WiredUI: server roster generation=")))
elif d=="roster_source":row("WiredUI: server roster generation=",3)["msg"]=row("WiredUI: server roster generation=",3)["msg"].replace("source=1","source=0")
elif d=="roster_order":a=P.index(row("WiredUI: server roster generation=",3));b=P.index(row("WiredUI: server roster generation=",4));P[a],P[b]=P[b],P[a]
elif d=="roster_newline":row("WiredUI: server roster generation=",3)["msg"]+="\rEXTRA"
elif d=="tick_missing":P.remove(row("WiredUI: server ping tick source=",1))
elif d=="tick_source":row("WiredUI: server ping tick dispatch ",1)["msg"]=row("WiredUI: server ping tick dispatch ",1)["msg"].replace("source=0 engine_source=0","source=1 engine_source=2")
elif d=="refresh_late":P.insert(P.index(row("WiredUI: server roster generation=",6)),dict(row("Ping owner refresh ",4)))
elif d=="request_peer":next(x for x in F if x.get("event")=="request")["peer_port"]=1
elif d=="response_bytes":next(x for x in F if x.get("event")=="response")["hex"]="00"
elif d=="fixture_mode":F[0]["mode"]="offscreen"
elif d=="fixture_order":F[5],F[6]=F[6],F[5]
elif d=="generation_early":F[5]["elapsed_ms"]=F[3]["elapsed_ms"]+998
elif d=="generation_late":F[5]["elapsed_ms"]=F[3]["elapsed_ms"]+3000
elif d=="same_challenge":F[7]["command"]=F[3]["command"]
elif d=="extra_enter":P.append(dict(row("wui_menu_nav: K_ENTER")))
elif d=="missing_focus":P.remove(row("wui_menu_nav focus: focused item ",3))
elif d=="missing_cover":P.remove(row("WiredUI: push menu ",1))
elif d=="extra_pop":P.append(dict(row("WiredUI: pop menu ")))
elif d=="ignored_expired":P.append({"sev":"DEBUG","cat":"client","msg":"Ignored expired ping infoResponse generation=8 from 127.0.0.1:30002"})
elif d=="challenge_mismatch":P.append({"sev":"DEBUG","cat":"client","msg":"Ignored infoResponse challenge mismatch generation=8 from 127.0.0.1:30002"})
elif d=="unsolicited":P.append({"sev":"DEBUG","cat":"client","msg":"Ignored unsolicited infoResponse from 127.0.0.1:30002"})
elif d=="non_lan":P.append({"sev":"DEBUG","cat":"client","msg":"Ignored non-LAN local discovery infoResponse from 127.0.0.1:30002"})
elif d=="rejected_overlong":P.append({"sev":"DEBUG","cat":"client","msg":"Rejected overlong infoResponse from 127.0.0.1:30002"})
elif d=="rejected_full":P.append({"sev":"DEBUG","cat":"client","msg":"Rejected full infoResponse from 127.0.0.1:30002"})
elif d=="rejected_invalid":P.append({"sev":"DEBUG","cat":"client","msg":"Rejected invalid infoResponse from 127.0.0.1:30002"})
elif d=="accepted_local":P.append({"sev":"INFO","cat":"client","msg":"Accepted local discovery infoResponse generation=7 address=127.0.0.1:30002 rtt=1ms name=X map=arena7 clients=1/8 gametype=0"})
elif d=="ingress_smuggle":P.append({"sev":"INFO","cat":"system","msg":"benign\rIgnored unsolicited infoResponse from 127.0.0.1:30002"})
elif d=="suffix":row("Ping result consumed ")["msg"]+=" EXTRA"
elif d=="smuggle":P.append({"sev":"INFO","cat":"system","msg":"benign\rPing result consumed slot=0 owner=browser-global generation=8 outcome=expired time=1100ms cache_action=clear matched=1 consumer=offscreen address=127.0.0.1:30002"})
elif d=="retired":P.append({"sev":"DEBUG","cat":"client","msg":"Ping transaction retired slot=0 owner=browser-global generation=8 reason=capacity-expired state=pending age=1100ms address=127.0.0.1:30002"})
open(pp,"w").writelines(json.dumps(x)+"\n" for x in P);open(fp,"w").writelines(json.dumps(x)+"\n" for x in F)
PYEOF
  if analyze "$D/p" "$D/f" >/dev/null 2>&1;then echo "FAIL self $d";exit 1;fi
 done
 echo "==> ping owner expired-offscreen analyzer self-test: PASS (${#defects[@]} mutations)";exit 0
fi

command -v python3 >/dev/null 2>&1 && [ -f "$FIXTURE" ] && [ -f "$TIMEOUT_RUNNER" ] || { echo 'SKIP: Python unavailable'; exit 77; }
WIRED="${1:-}";[ -n "$WIRED" ] && [ -x "$WIRED" ] || { echo 'SKIP: pass assembled Wired GUI binary'; exit 77; }
WIRED="$(cd "$(dirname "$WIRED")" && pwd)/$(basename "$WIRED")";WD="$(dirname "$WIRED")"
PACK="$(wired_find_archive_root "$WD" "$WD/../Resources" "$WD/../../.." 2>/dev/null || true)"
[ -n "$PACK" ] || { echo 'SKIP: current VFS archives missing'; exit 77; }
ROOT="$(mktemp -d -t ping-owner-expired-XXXXXX 2>/dev/null || mktemp -d)";HOME_DIR="$ROOT/q3now-preview";EVENTS="$ROOT/fixture.jsonl";PID=""
cleanup(){ [ -n "$PID" ] && kill -TERM "$PID" 2>/dev/null || true;[ -n "$PID" ] && wait "$PID" 2>/dev/null || true;[ "${WIRED_KEEP_ARTIFACTS:-0}" = 1 ] || rm -rf "$ROOT";};trap cleanup EXIT INT TERM
wired_link_content_into_home "$HOME_DIR" "$PACK/base" || exit 1
python3 "$FIXTURE" --mode expired --events "$EVENTS" & PID=$!;for _ in $(seq 1 100);do [ -s "$EVENTS" ] && break;sleep .05;done;[ -s "$EVENTS" ] || { echo 'FAIL fixture ready';exit 1; }
read -r MASTER_PORT BROWSER_PORT <<EOF
$(python3 - "$EVENTS" <<'PYEOF'
import json,sys
r=json.loads(open(sys.argv[1]).readline());print(r["master_port"],r["browser_port"])
PYEOF
)
EOF
cat >"$HOME_DIR/base/ping-owner-expired.cfg" <<'CFGEOF'
set cl_maxPing 999
wait 100
wui_push servers
wait 20
wui_menu_nav focus filter_source
wui_menu_nav enter
wui_menu_nav focus btn_refresh
wui_menu_nav enter
waitms 200
wui_server_ping_tick
wui_menu_nav focus filter_source
wui_menu_nav enter
wui_push main
waitms 1100
wui_server_ping_tick
wui_menu_nav back
wui_menu_nav focus filter_source
wui_menu_nav enter
waitms 100
wui_menu_nav focus btn_refresh
wui_menu_nav enter
waitms 200
wui_server_ping_tick
waitms 100
wui_server_ping_tick
waitms 300
wui_menu_nav focus serverlist
wui_menu_nav down
wait 10
wui_menu_nav back
wait 10
quit
CFGEOF
case "$(uname -s)" in Darwin) NATIVE="$HOME_DIR";ARGS=(-ApplePersistenceIgnoreState YES);;*) NATIVE="$HOME_DIR";ARGS=();;esac
python3 "$TIMEOUT_RUNNER" --timeout 45 --kill-after 10 --cwd "$WD" --stdout "$ROOT/stdout" -- "$WIRED" "${ARGS[@]}" +set fs_homepath "$NATIVE" +set com_automated 1 +set com_noHardReboot 1 +set net_enabled 1 +set net_ip 127.0.0.1 +set net_port 0 +set sv_master1 "127.0.0.1:$MASTER_PORT" +set s_initsound 0 +set r_fullscreen 0 +set r_mode -1 +set r_customwidth 1280 +set r_customheight 720 +set log_severity DEBUG +set log_file_severity DEBUG +set log_file_mode overwrite_synced +exec ping-owner-expired.cfg
rc=$?;[ "$rc" -eq 0 ] && [ -s "$HOME_DIR/qconsole.jsonl" ] || { echo "FAIL product rc=$rc";exit 1; }
kill -TERM "$PID" 2>/dev/null || true;wait "$PID" || { echo 'FAIL fixture exit';exit 1; };PID=""
analyze "$HOME_DIR/qconsole.jsonl" "$EVENTS"
