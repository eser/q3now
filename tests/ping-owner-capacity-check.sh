#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors
# A completed browser result is published before shared-capacity reuse while
# its browser menu is covered and no frame consumer can reap it first.
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
if len(ready)!=1 or ready[0].get("mode")!="capacity" or ready[0].get("protocol")!=74:raise SystemExit("FAIL ready")
q=ready[0]
for key in ("master_port","browser_port","fill_port"):
 if not isinstance(q.get(key),int) or not 0<q[key]<65536:raise SystemExit("FAIL port")
events=[r.get("event") for r in f]
expected=["ready","request","response","request","browser_held","request","response"]+["request"]*31+["stopped"]
if events!=expected:raise SystemExit("FAIL fixture lifecycle")
req=[r for r in f if r.get("event")=="request"]
if [r.get("role") for r in req[:2]] != ["master","browser"] or any(r.get("role")!="fill" for r in req[2:]) or [r.get("ordinal") for r in req[2:]]!=list(range(1,33)):raise SystemExit("FAIL request roles")
if req[0].get("command")!="getservers q3now 74" or any(re.fullmatch(r"getinfo [0-9a-f]{32}",r.get("command","")) is None for r in req[1:]):raise SystemExit("FAIL request commands")
challenges=[r["command"].split(" ",1)[1] for r in req[1:]]
if len(set(challenges))!=33:raise SystemExit("FAIL challenge uniqueness")
client_ports={r.get("source_port") for r in req}
if len(client_ports)!=1 or any(r.get("peer_port")!=q[f"{r['role']}_port"] for r in req):raise SystemExit("FAIL request identity")
responses=[r for r in f if r.get("event")=="response"]
if [r.get("scenario") for r in responses] != ["master","browser_capacity"]:raise SystemExit("FAIL responses")
O=b"\xff"*4;ch=challenges[0]
master=O+b"getserversResponse\\"+socket.inet_aton("127.0.0.1")+struct.pack("!H",q["browser_port"])+b"\\EOT"
info=O+b"infoResponse\n"+(f"\\challenge\\{ch}\\protocol\\74\\hostname\\CAPACITY PUBLISHED RESULT\\mapname\\arena7\\clients\\1\\sv_maxclients\\8\\game\\q3now\\gametype\\0\\minping\\0\\maxping\\0").encode()
for row,wire,port in zip(responses,(master,info),(q["master_port"],q["browser_port"])):
 try:observed=bytes.fromhex(row.get("hex",""))
 except ValueError:raise SystemExit("FAIL response hex")
 if observed!=wire or row.get("length")!=len(wire) or row.get("sha256")!=hashlib.sha256(wire).hexdigest() or row.get("source_port")!=port or row.get("peer_port") not in client_ports:raise SystemExit("FAIL response authority")
if not 0<=responses[1].get("rtt_ms",-1)<999:raise SystemExit("FAIL response deadline")
held=[r for r in f if r.get("event")=="browser_held"]
if len(held)!=1 or held[0].get("challenge")!=challenges[0] or held[0].get("peer_port") not in client_ports:raise SystemExit("FAIL held identity")
elapsed=[r.get("elapsed_ms") for r in req]
if any(not isinstance(x,int) for x in elapsed) or elapsed!=sorted(elapsed) or req[2]["elapsed_ms"]>responses[1]["elapsed_ms"] or elapsed[-1]-elapsed[2]>=999:raise SystemExit("FAIL fixture chronology")
stop=f[-1]
if stop.get("reason")!="signal" or stop.get("master_count")!=1 or stop.get("browser_count")!=1 or stop.get("fill_count")!=32 or stop.get("browser_sent") is not True:raise SystemExit("FAIL stop")

def norm(s):return s[:-1] if s.endswith("\n") else s
critical=("Ping transaction started ","Ping request generation=","Accepted ping infoResponse ","Ping result consumed ","Ping transaction cleared ","Ping transaction retired ","Ping owner refresh ","Ping queue allocation ","Ping browser scheduling deferred ","Local discovery request ","Master query generation=","WiredUI: server ping tick ","WiredUI: server roster generation=","WiredUI: server roster row=","WiredUI: server selection ","WiredUI: push menu ","WiredUI: pop menu ","wui_menu_nav focus: ","wui_menu_nav: K_")
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
eb=re.escape(f"127.0.0.1:{q['browser_port']}");ef=re.escape(f"127.0.0.1:{q['fill_port']}");em=re.escape(f"127.0.0.1:{q['master_port']}")
started=exact("Ping transaction started ",[rf"Ping transaction started owner=browser-global generation=([1-9][0-9]*) address={eb}"]+[rf"Ping transaction started owner=manual generation=([1-9][0-9]*) address={ef}"]*32)
gens=[int(re.fullmatch(r".*generation=([0-9]+) address=.*",x[2]).group(1)) for x in started]
local=exact("Local discovery request ",[r"Local discovery request generation=([1-9][0-9]*) challenge=[0-9a-f]{32} timeout=3000ms"]*2)
local_gen=[int(re.fullmatch(r"Local discovery request generation=([0-9]+).*",x[2]).group(1)) for x in local]
if local_gen!=[local_gen[0],local_gen[0]+1] or gens!=list(range(local_gen[0]+2,local_gen[0]+35)):raise SystemExit("FAIL shared generation chain")
requests=exact("Ping request generation=",[rf"Ping request generation={g} challenge={c} timeout=999ms address={eb if i==0 else ef}" for i,(g,c) in enumerate(zip(gens,challenges))])
masterq=exact("Master query generation=",[rf"Master query generation=[1-9][0-9]* address={em} extended=0 timeout=3000ms"],"INFO")
tick_dispatch=exact("WiredUI: server ping tick dispatch ",[r"WiredUI: server ping tick dispatch source=1 engine_source=2"],"DEBUG","ui")
tick=exact("WiredUI: server ping tick source=",[r"WiredUI: server ping tick source=1 engine_source=2 work=1"],"DEBUG","ui")
accepted=exact("Accepted ping infoResponse ",[rf"Accepted ping infoResponse generation={gens[0]} time=([1-9][0-9]{{0,2}})ms address={eb}"])
at=int(re.fullmatch(r".*time=([0-9]+)ms address=.*",accepted[0][2]).group(1))
if at>=999:raise SystemExit("FAIL accepted deadline")
alloc_patterns=[rf"Ping queue allocation slot={i} reason=free previous_generation=0 previous_state=empty age=0ms address=none" for i in range(1,32)]
alloc_patterns.append(rf"Ping queue allocation slot=0 reason=completed previous_generation={gens[0]} previous_state=completed age=([1-9][0-9]{{0,2}})ms address={eb}")
alloc=exact("Ping queue allocation ",alloc_patterns)
age=int(re.fullmatch(r".* age=([0-9]+)ms address=.*",alloc[-1][2]).group(1))
if not at<=age<999:raise SystemExit("FAIL capacity age")
consumed=exact("Ping result consumed ",[rf"Ping result consumed slot=0 owner=browser-global generation={gens[0]} outcome=completed time={at}ms cache_action=publish matched=1 consumer=capacity address={eb}"])
retired=exact("Ping transaction retired ",[rf"Ping transaction retired slot=0 owner=browser-global generation={gens[0]} reason=capacity-completed state=completed age={age}ms address={eb}"])
exact("Ping transaction cleared ",[]);exact("Ping browser scheduling deferred ",[])
refresh=exact("Ping owner refresh ",[r"Ping owner refresh owner=browser-local reason=local-refresh retired=0 retained_direct=0",r"Ping owner refresh owner=browser-local reason=local-refresh retired=0 retained_direct=0",r"Ping owner refresh owner=browser-global reason=global-refresh retired=0 retained_direct=0"])
rosters=exact("WiredUI: server roster generation=",[r"WiredUI: server roster generation=([1-9][0-9]*) source=0 displayed=0 raw=0",r"WiredUI: server roster generation=([1-9][0-9]*) source=1 displayed=0 raw=0",r"WiredUI: server roster generation=([1-9][0-9]*) source=1 displayed=0 raw=1",r"WiredUI: server roster generation=([1-9][0-9]*) source=1 displayed=1 raw=1"],"DEBUG","ui")
roster_gen=[int(re.fullmatch(r"WiredUI: server roster generation=([0-9]+).*",x[2]).group(1)) for x in rosters]
if roster_gen!=list(range(roster_gen[0],roster_gen[0]+4)):raise SystemExit("FAIL roster generation chain")
rows=exact("WiredUI: server roster row=",[rf"WiredUI: server roster row=0 raw=0 address={eb} name=CAPACITY PUBLISHED RESULT map=arena7"],"DEBUG","ui")
selection=exact("WiredUI: server selection ",[rf"WiredUI: server selection display_row=0 raw=0 source=1 list_generation={roster_gen[-1]} selection_generation=[1-9][0-9]* address={eb} name=CAPACITY PUBLISHED RESULT map=arena7"],"DEBUG","ui")
push=exact("WiredUI: push menu ",[r"WiredUI: push menu 'servers' \(depth 1\)",r"WiredUI: push menu 'main' \(depth 2\)"],"DEBUG","ui")
pop=exact("WiredUI: pop menu ",[r"WiredUI: pop menu \(depth 1\)",r"WiredUI: pop menu \(depth 0\)"],"DEBUG","ui")
focus_server=exact("wui_menu_nav focus: focused item 'serverlist'",[r"wui_menu_nav focus: focused item 'serverlist' \(top index -?[0-9]+\)"],"DEBUG","ui")
focus=exact("wui_menu_nav focus: focused item ",[r"wui_menu_nav focus: focused item 'filter_source' \(top index -?[0-9]+\)",r"wui_menu_nav focus: focused item 'btn_refresh' \(top index -?[0-9]+\)",r"wui_menu_nav focus: focused item 'serverlist' \(top index -?[0-9]+\)"],"DEBUG","ui")
enters=exact("wui_menu_nav: K_ENTER dispatched",[r"wui_menu_nav: K_ENTER dispatched"]*2,"DEBUG","ui")
down=exact("wui_menu_nav: K_DOWNARROW dispatched",[r"wui_menu_nav: K_DOWNARROW dispatched"],"DEBUG","ui")
back=exact("wui_menu_nav: K_ESCAPE dispatched",[r"wui_menu_nav: K_ESCAPE dispatched"]*2,"DEBUG","ui")
if not (push[0][0]<refresh[0][0]<local[0][0]<rosters[0][0]<focus[0][0]<enters[0][0]<focus[1][0]<enters[1][0]<refresh[1][0]<local[1][0]<refresh[2][0]<masterq[0][0]<rosters[1][0]<rosters[2][0]<tick_dispatch[0][0]<started[0][0]<requests[0][0]<tick[0][0]<push[1][0]<alloc[0][0]<started[1][0]<requests[1][0]<accepted[0][0]<alloc[-1][0]<consumed[0][0]<retired[0][0]<started[-1][0]<requests[-1][0]<pop[0][0]<back[0][0]<rosters[3][0]<rows[0][0]<selection[0][0]<focus_server[0][0]<down[0][0]<pop[1][0]<back[1][0]):raise SystemExit("FAIL causal order")
for i in range(32):
 ai=alloc[i][0];si=started[i+1][0];ri=requests[i+1][0]
 if not ai<si<ri or i<31 and not ri<alloc[i+1][0]:raise SystemExit("FAIL allocation/request interleave")
print("  PASS ping owner capacity: completed browser result is published before shared-capacity reuse")
PYEOF
}

if [ "${1:-}" = --self-test ]; then
 ROOT="$(mktemp -d -t ping-owner-capacity-self-XXXXXX 2>/dev/null || mktemp -d)"; trap 'rm -rf "$ROOT"' EXIT
 P="$ROOT/p" F="$ROOT/f"
 python3 - "$P" "$F" <<'PYEOF'
import hashlib,json,socket,struct,sys
p,f=sys.argv[1:];bp=30002;fp=30003;mp=30001;ep=f"127.0.0.1:{bp}";fill=f"127.0.0.1:{fp}";chs=[f"{i+1:032x}" for i in range(33)];M=[]
def add(msg,sev="DEBUG",cat="client"):M.append({"sev":sev,"cat":cat,"msg":msg})
add("WiredUI: push menu 'servers' (depth 1)",cat="ui");add("Ping owner refresh owner=browser-local reason=local-refresh retired=0 retained_direct=0");add("Local discovery request generation=8 challenge="+"a"*32+" timeout=3000ms");add("WiredUI: server roster generation=20 source=0 displayed=0 raw=0",cat="ui");add("wui_menu_nav focus: focused item 'filter_source' (top index -1)",cat="ui");add("wui_menu_nav: K_ENTER dispatched",cat="ui");add("wui_menu_nav focus: focused item 'btn_refresh' (top index -1)",cat="ui");add("wui_menu_nav: K_ENTER dispatched",cat="ui");add("Ping owner refresh owner=browser-local reason=local-refresh retired=0 retained_direct=0");add("Local discovery request generation=9 challenge="+"b"*32+" timeout=3000ms");add("Ping owner refresh owner=browser-global reason=global-refresh retired=0 retained_direct=0");add(f"Master query generation=4 address=127.0.0.1:{mp} extended=0 timeout=3000ms","INFO");add("WiredUI: server roster generation=21 source=1 displayed=0 raw=0",cat="ui");add("WiredUI: server roster generation=22 source=1 displayed=0 raw=1",cat="ui");add("WiredUI: server ping tick dispatch source=1 engine_source=2",cat="ui");add(f"Ping transaction started owner=browser-global generation=10 address={ep}");add(f"Ping request generation=10 challenge={chs[0]} timeout=999ms address={ep}");add("WiredUI: server ping tick source=1 engine_source=2 work=1",cat="ui");add("WiredUI: push menu 'main' (depth 2)",cat="ui")
for i in range(32):
 slot=i+1 if i<31 else 0
 if i<31:add(f"Ping queue allocation slot={slot} reason=free previous_generation=0 previous_state=empty age=0ms address=none")
 else:
  add(f"Ping queue allocation slot=0 reason=completed previous_generation=10 previous_state=completed age=140ms address={ep}");add(f"Ping result consumed slot=0 owner=browser-global generation=10 outcome=completed time=25ms cache_action=publish matched=1 consumer=capacity address={ep}");add(f"Ping transaction retired slot=0 owner=browser-global generation=10 reason=capacity-completed state=completed age=140ms address={ep}")
 add(f"Ping transaction started owner=manual generation={11+i} address={fill}");add(f"Ping request generation={11+i} challenge={chs[i+1]} timeout=999ms address={fill}")
 if i==0:add(f"Accepted ping infoResponse generation=10 time=25ms address={ep}")
add("WiredUI: pop menu (depth 1)",cat="ui");add("wui_menu_nav: K_ESCAPE dispatched",cat="ui");add("WiredUI: server roster generation=23 source=1 displayed=1 raw=1",cat="ui");add(f"WiredUI: server roster row=0 raw=0 address={ep} name=CAPACITY PUBLISHED RESULT map=arena7",cat="ui");add(f"WiredUI: server selection display_row=0 raw=0 source=1 list_generation=23 selection_generation=3 address={ep} name=CAPACITY PUBLISHED RESULT map=arena7",cat="ui");add("wui_menu_nav focus: focused item 'serverlist' (top index -1)",cat="ui");add("wui_menu_nav: K_DOWNARROW dispatched",cat="ui");add("WiredUI: pop menu (depth 0)",cat="ui");add("wui_menu_nav: K_ESCAPE dispatched",cat="ui")
open(p,"w").writelines(json.dumps(x)+"\n" for x in M)
O=b"\xff"*4;mw=O+b"getserversResponse\\"+socket.inet_aton("127.0.0.1")+struct.pack("!H",bp)+b"\\EOT";iw=O+b"infoResponse\n"+(f"\\challenge\\{chs[0]}\\protocol\\74\\hostname\\CAPACITY PUBLISHED RESULT\\mapname\\arena7\\clients\\1\\sv_maxclients\\8\\game\\q3now\\gametype\\0\\minping\\0\\maxping\\0").encode();E=[{"event":"ready","mode":"capacity","protocol":74,"master_port":mp,"browser_port":bp,"fill_port":fp},{"event":"request","role":"master","ordinal":1,"command":"getservers q3now 74","source_port":30100,"peer_port":mp,"elapsed_ms":10},{"event":"response","scenario":"master","challenge":"","source_port":mp,"peer_port":30100,"length":len(mw),"sha256":hashlib.sha256(mw).hexdigest(),"hex":mw.hex(),"elapsed_ms":11,"rtt_ms":0},{"event":"request","role":"browser","ordinal":1,"command":f"getinfo {chs[0]}","source_port":30100,"peer_port":bp,"elapsed_ms":20},{"event":"browser_held","challenge":chs[0],"peer_port":30100}]
for i in range(32):
 E.append({"event":"request","role":"fill","ordinal":i+1,"command":f"getinfo {chs[i+1]}","source_port":30100,"peer_port":fp,"elapsed_ms":50+i})
 if i==0:E.append({"event":"response","scenario":"browser_capacity","challenge":chs[0],"source_port":bp,"peer_port":30100,"length":len(iw),"sha256":hashlib.sha256(iw).hexdigest(),"hex":iw.hex(),"elapsed_ms":51,"rtt_ms":31})
E.append({"event":"stopped","reason":"signal","master_count":1,"browser_count":1,"fill_count":32,"browser_sent":True});open(f,"w").writelines(json.dumps(x)+"\n" for x in E)
PYEOF
 analyze "$P" "$F" >/dev/null || { echo 'FAIL clean'; exit 1; }
 defects=(missing_capacity_consume wrong_consumer unmatched publish_after_retire wrong_owner missing_retire wrong_reason clear_browser missing_row extra_row wrong_row missing_selection roster_missing roster_additive roster_source roster_order tick_dispatch_missing tick_missing tick_additive tick_source late_global_refresh push_missing pop_suffix completed_age expired_victim missing_fill request_order request_peer response_bytes fixture_mode fixture_count fixture_span accepted_deadline duplicate_challenge suffix smuggle)
 for d in "${defects[@]}"; do
  D="$ROOT/$d";mkdir "$D";cp "$P" "$D/p";cp "$F" "$D/f"
  python3 - "$D/p" "$D/f" "$d" <<'PYEOF'
import json,re,sys
pp,fp,d=sys.argv[1:];P=[json.loads(x) for x in open(pp)];F=[json.loads(x) for x in open(fp)]
def rows(s):return [x for x in P if x["msg"].startswith(s)]
def row(s,n=0):return rows(s)[n]
if d=="missing_capacity_consume":P.remove(row("Ping result consumed "))
elif d=="wrong_consumer":row("Ping result consumed ")["msg"]=row("Ping result consumed ")["msg"].replace("consumer=capacity","consumer=current")
elif d=="unmatched":row("Ping result consumed ")["msg"]=row("Ping result consumed ")["msg"].replace("matched=1","matched=0")
elif d=="publish_after_retire":a=P.index(row("Ping result consumed "));b=P.index(row("Ping transaction retired "));P[a],P[b]=P[b],P[a]
elif d=="wrong_owner":row("Ping result consumed ")["msg"]=row("Ping result consumed ")["msg"].replace("browser-global","manual")
elif d=="missing_retire":P.remove(row("Ping transaction retired "))
elif d=="wrong_reason":row("Ping transaction retired ")["msg"]=row("Ping transaction retired ")["msg"].replace("capacity-completed","capacity-oldest-pending")
elif d=="clear_browser":P.append({"sev":"DEBUG","cat":"client","msg":"Ping transaction cleared slot=0 owner=browser-global generation=10 state=completed age=140ms address=127.0.0.1:30002"})
elif d=="missing_row":P.remove(row("WiredUI: server roster row="))
elif d=="extra_row":P.append(dict(row("WiredUI: server roster row=")))
elif d=="wrong_row":row("WiredUI: server roster row=")["msg"]=row("WiredUI: server roster row=")["msg"].replace("CAPACITY PUBLISHED RESULT","WRONG")
elif d=="missing_selection":P.remove(row("WiredUI: server selection "))
elif d=="roster_missing":P.remove(row("WiredUI: server roster generation=",2))
elif d=="roster_additive":P.append(dict(row("WiredUI: server roster generation=")))
elif d=="roster_source":row("WiredUI: server roster generation=",3)["msg"]=row("WiredUI: server roster generation=",3)["msg"].replace("source=1","source=0")
elif d=="roster_order":a=P.index(row("WiredUI: server roster generation=",2));b=P.index(row("WiredUI: server roster generation=",3));P[a],P[b]=P[b],P[a]
elif d=="tick_dispatch_missing":P.remove(row("WiredUI: server ping tick dispatch "))
elif d=="tick_missing":P.remove(row("WiredUI: server ping tick source="))
elif d=="tick_additive":P.append(dict(row("WiredUI: server ping tick source=")))
elif d=="tick_source":row("WiredUI: server ping tick source=")["msg"]=row("WiredUI: server ping tick source=")["msg"].replace("source=1 engine_source=2","source=0 engine_source=0")
elif d=="late_global_refresh":P.insert(P.index(row("WiredUI: pop menu ")),{"sev":"DEBUG","cat":"client","msg":"Ping owner refresh owner=browser-global reason=global-refresh retired=0 retained_direct=32"})
elif d=="push_missing":P.remove(row("WiredUI: push menu "))
elif d=="pop_suffix":row("WiredUI: pop menu ")["msg"]+=" EXTRA"
elif d=="completed_age":row("Ping queue allocation slot=0")["msg"]=row("Ping queue allocation slot=0")["msg"].replace("age=140ms","age=999ms")
elif d=="expired_victim":row("Ping queue allocation slot=0")["msg"]=row("Ping queue allocation slot=0")["msg"].replace("reason=completed","reason=expired").replace("previous_state=completed","previous_state=pending")
elif d=="missing_fill":F.remove(next(x for x in F if x.get("role")=="fill" and x.get("ordinal")==17))
elif d=="request_order":a=next(i for i,x in enumerate(F) if x.get("role")=="fill" and x.get("ordinal")==2);b=next(i for i,x in enumerate(F) if x.get("role")=="fill" and x.get("ordinal")==3);F[a],F[b]=F[b],F[a]
elif d=="request_peer":next(x for x in F if x.get("event")=="request")["peer_port"]=1
elif d=="response_bytes":next(x for x in F if x.get("event")=="response")["hex"]="00"
elif d=="fixture_mode":F[0]["mode"]="offscreen"
elif d=="fixture_count":F[-1]["fill_count"]=31
elif d=="fixture_span":next(x for x in F if x.get("role")=="fill" and x.get("ordinal")==32)["elapsed_ms"]=2000
elif d=="accepted_deadline":row("Accepted ping infoResponse ")["msg"]=row("Accepted ping infoResponse ")["msg"].replace("time=25ms","time=999ms")
elif d=="duplicate_challenge":next(x for x in F if x.get("role")=="fill" and x.get("ordinal")==2)["command"]=next(x for x in F if x.get("role")=="fill" and x.get("ordinal")==1)["command"]
elif d=="suffix":row("Ping result consumed ")["msg"]+=" EXTRA"
elif d=="smuggle":P.append({"sev":"INFO","cat":"system","msg":"benign\nPing transaction retired slot=0 owner=browser-global generation=10 reason=capacity-completed state=completed age=140ms address=127.0.0.1:30002"})
open(pp,"w").writelines(json.dumps(x)+"\n" for x in P);open(fp,"w").writelines(json.dumps(x)+"\n" for x in F)
PYEOF
  if analyze "$D/p" "$D/f" >/dev/null 2>&1;then echo "FAIL self $d";exit 1;fi
 done
 echo "==> ping owner capacity analyzer self-test: PASS (${#defects[@]} mutations)";exit 0
fi

command -v python3 >/dev/null 2>&1 && [ -f "$FIXTURE" ] && [ -f "$TIMEOUT_RUNNER" ] || { echo 'SKIP: Python unavailable'; exit 77; }
WIRED="${1:-}"; [ -n "$WIRED" ] && [ -x "$WIRED" ] || { echo 'SKIP: pass assembled Wired GUI binary'; exit 77; }
WIRED="$(cd "$(dirname "$WIRED")" && pwd)/$(basename "$WIRED")"; WD="$(dirname "$WIRED")"
PACK="$(wired_find_archive_root "$WD" "$WD/../Resources" "$WD/../../.." 2>/dev/null || true)"
[ -n "$PACK" ] || { echo 'SKIP: current VFS archives missing'; exit 77; }
ROOT="$(mktemp -d -t ping-owner-capacity-XXXXXX 2>/dev/null||mktemp -d)";HOME_DIR="$ROOT/q3now-preview";EVENTS="$ROOT/fixture.jsonl";PID=""
cleanup(){ [ -n "$PID" ]&&kill -TERM "$PID" 2>/dev/null||true;[ -n "$PID" ]&&wait "$PID" 2>/dev/null||true;[ "${WIRED_KEEP_ARTIFACTS:-0}" = 1 ]||rm -rf "$ROOT";};trap cleanup EXIT INT TERM
wired_link_content_into_home "$HOME_DIR" "$PACK/base" || exit 1
python3 "$FIXTURE" --mode capacity --events "$EVENTS" & PID=$!;for _ in $(seq 1 100);do [ -s "$EVENTS" ]&&break;sleep .05;done;[ -s "$EVENTS" ]||{ echo 'FAIL fixture ready';exit 1;}
read -r MASTER_PORT BROWSER_PORT FILL_PORT <<EOF
$(python3 - "$EVENTS" <<'PYEOF'
import json,sys
r=json.loads(open(sys.argv[1]).readline());print(r["master_port"],r["browser_port"],r["fill_port"])
PYEOF
)
EOF
CFG="$HOME_DIR/base/ping-owner-capacity.cfg"
{
 echo 'set cl_maxPing 999';echo 'wait 100';echo 'wui_push servers';echo 'wait 20';echo 'wui_menu_nav focus filter_source';echo 'wui_menu_nav enter';echo 'wui_menu_nav focus btn_refresh';echo 'wui_menu_nav enter';echo 'waitms 200';echo 'wui_server_ping_tick';echo 'wui_push main';echo "ping 127.0.0.1:$FILL_PORT";echo 'waitms 100'
 for _ in $(seq 2 32);do echo "ping 127.0.0.1:$FILL_PORT";done
 echo 'waitms 200';echo 'wui_menu_nav back';echo 'waitms 500';echo 'wui_menu_nav focus serverlist';echo 'wui_menu_nav down';echo 'wait 10';echo 'wui_menu_nav back';echo 'wait 10';echo 'quit'
} >"$CFG"
case "$(uname -s)" in Darwin) NATIVE="$HOME_DIR";ARGS=(-ApplePersistenceIgnoreState YES);;*) NATIVE="$HOME_DIR";ARGS=();;esac
python3 "$TIMEOUT_RUNNER" --timeout 45 --kill-after 10 --cwd "$WD" --stdout "$ROOT/stdout" -- "$WIRED" "${ARGS[@]}" +set fs_homepath "$NATIVE" +set com_automated 1 +set com_noHardReboot 1 +set net_enabled 1 +set net_ip 127.0.0.1 +set net_port 0 +set sv_master1 "127.0.0.1:$MASTER_PORT" +set s_initsound 0 +set r_fullscreen 0 +set r_mode -1 +set r_customwidth 1280 +set r_customheight 720 +set log_severity DEBUG +set log_file_severity DEBUG +set log_file_mode overwrite_synced +exec ping-owner-capacity.cfg
rc=$?;[ "$rc" -eq 0 ]&&[ -s "$HOME_DIR/qconsole.jsonl" ]||{ echo "FAIL product rc=$rc";exit 1;}
kill -TERM "$PID" 2>/dev/null||true;wait "$PID"||{ echo 'FAIL fixture exit';exit 1;};PID=""
analyze "$HOME_DIR/qconsole.jsonl" "$EVENTS"
