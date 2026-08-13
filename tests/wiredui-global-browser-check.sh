#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors
# SPDX-License-Identifier: GPL-3.0-or-later
# Deterministic real global-browser discovery. No cache injection or Connect.

set -uo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
FIXTURE="$SCRIPT_DIR/wiredui-global-server-fixture.py"
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
  if not isinstance(r,dict):raise SystemExit(f"FAIL {path}:{n}: schema")
  rows.append(r)
 if not rows:raise SystemExit(f"FAIL empty {path}")
 return rows
p=load(pp);f=load(fp)
if any(not all(isinstance(r.get(k),str) for k in ("sev","cat","msg")) for r in p):raise SystemExit("FAIL product schema")
if any(not isinstance(r.get("event"),str) for r in f):raise SystemExit("FAIL fixture schema")
bad=[r for r in p if r["sev"].upper() in {"ERROR","FATAL"} or r["sev"].upper()=="WARN" and r["cat"].lower()=="ui"]
if bad:raise SystemExit("FAIL severity")
m=[r["msg"] for r in p];ev=[r["event"] for r in f]
ready=[r for r in f if r["event"]=="ready"]
if len(ready)!=1:raise SystemExit("FAIL fixture ready")
q=ready[0];roles=("master","rogue","target","sentinel")
if any(not isinstance(q.get(k+"_port"),int) or not 0<q[k+"_port"]<65536 for k in roles):raise SystemExit("FAIL fixture ports")
addr={k:f"127.0.0.1:{q[k+'_port']}" for k in roles};E={k:re.escape(v) for k,v in addr.items()}

# Reconstruct the wire bytes independently. Fixture labels/hashes are evidence,
# not the authority used to decide whether a packet is canonical.
OOB=b"\xff"*4;CMD=b"getserversResponse"
def rec(port):return b"\\"+socket.inet_aton("127.0.0.1")+struct.pack("!H",port)
canonical=OOB+CMD+rec(q["target_port"])+rec(q["sentinel_port"])+b"\\EOT"
expected={"rogue_authority":OOB+CMD+rec(q["rogue_port"])+b"\\EOT","authorized_bare":OOB+CMD,"authorized_whitespace":OOB+CMD+b" ","authorized_poison_then_zero_port":OOB+CMD+rec(q["rogue_port"])+rec(0),"expired_canonical":canonical,"inactive_replay":canonical,"recovery_canonical":canonical}
packets=[r for r in f if r["event"]=="packet_sent"]
if [r.get("scenario") for r in packets]!=list(expected):raise SystemExit("FAIL packet scenario cardinality/order")
queries=[r for r in f if r["event"]=="master_query"]
requests_from_master=[r for r in f if r["event"]=="request" and r.get("role")=="master"]
if len(queries)!=2 or [r.get("query") for r in queries]!=[1,2] or any(r.get("command")!="getservers q3now 74" for r in queries):raise SystemExit("FAIL fixture master queries")
if len(requests_from_master)!=2 or any(r.get("command")!="getservers q3now 74" for r in requests_from_master):raise SystemExit("FAIL exact getservers")
for row in packets:
 scenario=row["scenario"];wire=expected[scenario];source="rogue" if scenario=="rogue_authority" else "master";query=2 if scenario=="recovery_canonical" else 1
 try:observed=bytes.fromhex(row.get("hex",""))
 except ValueError:raise SystemExit(f"FAIL packet hex {scenario}")
 if observed!=wire or row.get("length")!=len(wire) or row.get("sha256")!=hashlib.sha256(wire).hexdigest():raise SystemExit(f"FAIL packet bytes {scenario}")
 if row.get("source")!=source or row.get("source_port")!=q[source+"_port"] or row.get("source_address")!=addr[source] or row.get("query")!=query:raise SystemExit(f"FAIL packet identity {scenario}")
 if row.get("peer_port")!=queries[query-1].get("peer_port") or row.get("peer")!=f"127.0.0.1:{row.get('peer_port')}":raise SystemExit(f"FAIL packet peer {scenario}")
 if not isinstance(row.get("elapsed_ms"),int) or row["elapsed_ms"]<0:raise SystemExit(f"FAIL packet timing schema {scenario}")
times={r["scenario"]:r["elapsed_ms"] for r in packets}
if not (times["rogue_authority"]<=times["authorized_bare"]<=times["authorized_whitespace"]<=times["authorized_poison_then_zero_port"]<1000):raise SystemExit("FAIL early packet timing")
if times["expired_canonical"]<3000 or not times["expired_canonical"]<=times["inactive_replay"]<=times["expired_canonical"]+500:raise SystemExit("FAIL expiry packet timing")
if not 0<=times["recovery_canonical"]<3000:raise SystemExit("FAIL recovery packet timing")
if ev.count("stopped")!=1 or any(x in ev for x in ("poison_ping","unexpected_master_query","duplicate_getinfo")):raise SystemExit("FAIL fixture lifecycle")

def single_line(msg):
 if msg.endswith("\n"):msg=msg[:-1]
 if "\n" in msg or "\r" in msg:raise SystemExit("FAIL critical embedded/additional newline")
 return msg
def family(prefix):return [(i,r,single_line(r["msg"])) for i,r in enumerate(p) if r["msg"].startswith(prefix)]
def exact_family(prefix,patterns,sev,cat):
 rows=family(prefix)
 if len(rows)!=len(patterns):raise SystemExit(f"FAIL {prefix} cardinality")
 for (i,row,msg),pat in zip(rows,patterns):
  if row["sev"]!=sev or row["cat"]!=cat or re.fullmatch(pat,msg) is None:raise SystemExit(f"FAIL {prefix} exact metadata")
 return rows

query_pat=rf"Master query generation=([0-9]+) address={E['master']} extended=0 timeout=3000ms"
query_rows=exact_family("Master query generation=",[query_pat,query_pat],"INFO","client")
query_match=[re.fullmatch(query_pat,msg) for _,row,msg in query_rows]
g1,g2=(int(x.group(1)) for x in query_match)
if g2!=g1+1:raise SystemExit("FAIL product query generations")
unauthorized=exact_family("Ignored unauthorized getserversResponse",[rf"Ignored unauthorized getserversResponse from {E['rogue']}"],"DEBUG","client")
source_kind=exact_family("Ignored master response reason=source-or-kind",[rf"Ignored master response reason=source-or-kind generation={g1} source={E['rogue']} kind=classic"],"DEBUG","client")
malformed=exact_family("Ignored master response reason=malformed",[rf"Ignored master response reason=malformed generation={g1} source={E['master']} kind=classic"],"DEBUG","client")
expired=exact_family("Ignored master response reason=expired",[rf"Ignored master response reason=expired generation={g1} source={E['master']} kind=classic elapsed=(?:[3-9][0-9]{{3}}|[1-9][0-9]{{4,}})ms"],"DEBUG","client")
inactive=exact_family("Ignored master response reason=inactive",[rf"Ignored master response reason=inactive source={E['master']} kind=classic"],"DEBUG","client")
accepted_summary=exact_family("Accepted getserversResponse",[rf"Accepted getserversResponse generation={g2} address={E['master']} parsed=2 total=2"],"INFO","client")
accepted_detail=exact_family("Accepted master response",[rf"Accepted master response generation={g2} source_index=0 source={E['master']} kind=classic parsed=2 added=2 total=2 elapsed=[0-9]+ms"],"INFO","client")

semantic=family("CL packet ")
semantic_patterns=[rf"CL packet {E['rogue']}: getserversResponse.*"]+[rf"CL packet {E['master']}: getserversResponse.*"]*4+[rf"CL packet {E['target']}: infoResponse.*"]*2+[rf"CL packet {E['sentinel']}: infoResponse.*"]*2
if len(semantic)!=9 or any(r["sev"]!="DEBUG" or r["cat"]!="client" or re.fullmatch(pat,msg) is None for (_,r,msg),pat in zip(semantic,semantic_patterns)):raise SystemExit("FAIL CL packet exact source/token/order")
if family("Unknown connectionless packet"):raise SystemExit("FAIL bare packet routed")

rosters=family("WiredUI: server roster generation=")
roster_pat=r"WiredUI: server roster generation=([0-9]+) source=([0-9]+) displayed=([0-9]+) raw=([0-9]+)"
if len(rosters)!=8 or any(r["sev"]!="DEBUG" or r["cat"]!="ui" or re.fullmatch(roster_pat,msg) is None for _,r,msg in rosters):raise SystemExit("FAIL roster exact metadata/cardinality")
roster_values=[tuple(map(int,re.fullmatch(roster_pat,msg).groups())) for _,_,msg in rosters]
roster_base=roster_values[0][0]
roster_states=[(0,0,0)]+[(1,0,0)]*4+[(1,0,2)]+[(1,2,2)]*2
if roster_values!=[(roster_base+i,*state) for i,state in enumerate(roster_states)]:raise SystemExit("FAIL roster exact generation/state sequence")
sorts=exact_family("wui_listbox_sort: sorted feeder",[r"wui_listbox_sort: sorted feeder 2 on item 'serverlist' by column 0"]*3,"DEBUG","ui")
markers=exact_family("GLOBAL_BROWSER_ZERO_",[r"GLOBAL_BROWSER_ZERO_1",r"GLOBAL_BROWSER_ZERO_2"],"INFO","system")
for marker,(mi,_,_) in zip(("GLOBAL_BROWSER_ZERO_1","GLOBAL_BROWSER_ZERO_2"),markers):
 prior_sort=max((i for i,_,_ in sorts if i<mi),default=-1);prior_roster=max(((i,r,msg) for i,r,msg in rosters if i<mi),default=(-1,None,None),key=lambda x:x[0])
 if not (prior_roster[0]>=0 and prior_roster[0]<prior_sort<mi and re.fullmatch(r"WiredUI: server roster generation=[0-9]+ source=1 displayed=0 raw=0",prior_roster[2])):raise SystemExit(f"FAIL explicit zero roster {marker}")

ping_req_pat=r"Ping request generation=([0-9]+) challenge=([0-9a-f]{32}) timeout=[0-9]+ms address=(\S+)"
ping_rows=exact_family("Ping request generation=",[ping_req_pat,ping_req_pat],"DEBUG","client")
requests={}
for _,row,msg in ping_rows:
 x=re.fullmatch(ping_req_pat,msg);requests[x.group(3)]=(x.group(1),x.group(2))
identities=[requests.get(addr[r]) for r in ("target","sentinel")]
if len(requests)!=2 or any(not x for x in identities) or len({x[0] for x in identities})!=2 or len({x[1] for x in identities})!=2:raise SystemExit("FAIL challenge identities")
mismatch_patterns=[];accept_patterns=[]
for role in ("target","sentinel"):
 generation,challenge=requests[addr[role]];a=re.escape(addr[role])
 mismatch_patterns.append(rf"Ignored infoResponse challenge mismatch generation={generation} from {a}")
 accept_patterns.append(rf"Accepted ping infoResponse generation={generation} time=[1-9][0-9]*ms address={a}")
mismatch_rows=family("Ignored infoResponse challenge mismatch");ping_accept_rows=family("Accepted ping infoResponse")
if len(mismatch_rows)!=2 or any(r["sev"]!="DEBUG" or r["cat"]!="client" for _,r,_ in mismatch_rows) or any(not any(re.fullmatch(pat,msg) for pat in mismatch_patterns) for _,r,msg in mismatch_rows) or len({msg for _,r,msg in mismatch_rows})!=2:raise SystemExit("FAIL mismatch lifecycle exact metadata")
if len(ping_accept_rows)!=2 or any(r["sev"]!="DEBUG" or r["cat"]!="client" for _,r,_ in ping_accept_rows) or any(not any(re.fullmatch(pat,msg) for pat in accept_patterns) for _,r,msg in ping_accept_rows) or len({re.sub(r"time=[0-9]+ms", "time=Xms", msg) for _,r,msg in ping_accept_rows})!=2:raise SystemExit("FAIL accepted ping lifecycle exact metadata")
consume_patterns=[];clear_patterns=[]
for role in ("target","sentinel"):
 generation,_=requests[addr[role]];a=re.escape(addr[role])
 consume_patterns.append(rf"Ping result consumed slot=[0-9]+ owner=browser-global generation={generation} outcome=completed time=([1-9][0-9]*)ms cache_action=publish matched=1 consumer=current address={a}")
 clear_patterns.append(rf"Ping transaction cleared slot=[0-9]+ owner=browser-global generation={generation} state=completed age=[1-9][0-9]*ms address={a}")
consume_rows=family("Ping result consumed ");clear_rows=family("Ping transaction cleared ")
if len(consume_rows)!=2 or any(r["sev"]!="DEBUG" or r["cat"]!="client" or not any(re.fullmatch(pat,msg) for pat in consume_patterns) for _,r,msg in consume_rows):raise SystemExit("FAIL consumer lifecycle")
if len(clear_rows)!=2 or any(r["sev"]!="DEBUG" or r["cat"]!="client" or not any(re.fullmatch(pat,msg) for pat in clear_patterns) for _,r,msg in clear_rows):raise SystemExit("FAIL clear lifecycle")
for role in ("target","sentinel"):
 a=re.escape(addr[role]);generation,challenge=requests[addr[role]]
 request_i=next(i for i,_,msg in ping_rows if re.fullmatch(rf"Ping request generation={generation} challenge={challenge} timeout=[0-9]+ms address={a}",msg))
 mismatch_i=next(i for i,_,msg in mismatch_rows if re.fullmatch(rf"Ignored infoResponse challenge mismatch generation={generation} from {a}",msg))
 accept_i=next(i for i,_,msg in ping_accept_rows if re.fullmatch(rf"Accepted ping infoResponse generation={generation} time=[1-9][0-9]*ms address={a}",msg))
 consume_i,consume_msg=next((i,msg) for i,_,msg in consume_rows if f"generation={generation} " in msg)
 clear_i=next(i for i,_,msg in clear_rows if f"generation={generation} " in msg)
 accepted_rtt=int(re.fullmatch(rf"Accepted ping infoResponse generation={generation} time=([1-9][0-9]*)ms address={a}",next(msg for _,_,msg in ping_accept_rows if f"generation={generation} " in msg)).group(1))
 consumed_rtt=int(re.fullmatch(next(pat for pat in consume_patterns if f"generation={generation}" in pat),consume_msg).group(1))
 if accepted_rtt!=consumed_rtt or not request_i<mismatch_i<accept_i<consume_i<clear_i:raise SystemExit(f"FAIL challenge causal order {role}")
master_i=accepted_summary[0][0];first_ping_i=ping_accept_rows[0][0]
if any(i<=master_i for i,_,_ in ping_rows+mismatch_rows+ping_accept_rows):raise SystemExit("FAIL pre-generation2 ping lifecycle")

feeder_rows=family("WiredUI: server roster row=");selections=family("WiredUI: server selection ")
if any(i<first_ping_i for i,_,_ in feeder_rows+selections):raise SystemExit("FAIL pre-hydration feeder authority")
target_initial=rf"WiredUI: server roster row=0 raw=0 address={E['target']} name=Z0 WIRED GLOBAL TARGET map=arena7"
sentinel_initial=rf"WiredUI: server roster row=1 raw=1 address={E['sentinel']} name=A0 WIRED GLOBAL SENTINEL map=arena1"
sentinel_sorted=rf"WiredUI: server roster row=0 raw=1 address={E['sentinel']} name=A0 WIRED GLOBAL SENTINEL map=arena1"
target_sorted=rf"WiredUI: server roster row=1 raw=0 address={E['target']} name=Z0 WIRED GLOBAL TARGET map=arena7"
feeder_patterns=[target_initial,sentinel_initial,sentinel_sorted,target_sorted]
feeder_rows=exact_family("WiredUI: server roster row=",feeder_patterns,"DEBUG","ui")
if not (clear_rows[0][0]<clear_rows[1][0]<rosters[6][0]<feeder_rows[0][0]<feeder_rows[1][0]<rosters[7][0]<feeder_rows[2][0]<feeder_rows[3][0]<sorts[2][0]):raise SystemExit("FAIL roster row epoch binding")
selection_pat=rf"WiredUI: server selection display_row=([01]) raw=([01]) source=1 list_generation=([0-9]+) selection_generation=([0-9]+) address=({E['sentinel']}|{E['target']}) name=(A0 WIRED GLOBAL SENTINEL|Z0 WIRED GLOBAL TARGET) map=(arena1|arena7)"
selection_rows=exact_family("WiredUI: server selection ",[selection_pat,selection_pat],"DEBUG","ui")
selection_values=[re.fullmatch(selection_pat,msg).groups() for _,_,msg in selection_rows]
if selection_values[0][0:2]!=("0","1") or selection_values[0][4:]!=(addr["sentinel"],"A0 WIRED GLOBAL SENTINEL","arena1"):raise SystemExit("FAIL sentinel selection authority")
if selection_values[1][0:2]!=("1","0") or selection_values[1][4:]!=(addr["target"],"Z0 WIRED GLOBAL TARGET","arena7"):raise SystemExit("FAIL target selection authority")
if int(selection_values[0][2])!=roster_base+7 or selection_values[0][2]!=selection_values[1][2] or int(selection_values[1][3])!=int(selection_values[0][3])+1:raise SystemExit("FAIL selection generation continuity")
if not (sorts[2][0]<selection_rows[0][0]<selection_rows[1][0]):raise SystemExit("FAIL selection roster order")
for role in ("target","sentinel"):
 a=addr[role];rows=[r for r in f if r["event"]=="current_info_response" and r.get("query")==2 and r.get("role")==role];wrong=[r for r in f if r["event"]=="wrong_info_response" and r.get("query")==2 and r.get("role")==role]
 if len(rows)!=1 or len(wrong)!=1 or rows[0].get("challenge")!=requests[a][1] or wrong[0].get("expected")!=requests[a][1]:raise SystemExit(f"FAIL challenge correlation {role}")
fixture_challenge_order=[(r.get("event"),r.get("role")) for r in f if r.get("event") in ("wrong_info_response","current_info_response")]
if fixture_challenge_order!=[("wrong_info_response","target"),("current_info_response","target"),("wrong_info_response","sentinel"),("current_info_response","sentinel")]:raise SystemExit("FAIL fixture challenge order")
if any("BAD SENTINEL" in x or "BAD TARGET" in x for x in m):raise SystemExit("FAIL rejected metadata admitted")
if any(addr["rogue"] in msg for _,r,msg in ping_rows+feeder_rows):raise SystemExit("FAIL poison admitted")
exact_family("WiredUI: push menu 'servers'",[r"WiredUI: push menu 'servers' \(depth 1\)"],"DEBUG","ui")
focus_patterns=[r"wui_menu_nav focus: focused item 'filter_source' \(top index -?[0-9]+\)",r"wui_menu_nav focus: focused item 'btn_refresh' \(top index -?[0-9]+\)",r"wui_menu_nav focus: focused item 'btn_refresh' \(top index -?[0-9]+\)",r"wui_menu_nav focus: focused item 'serverlist' \(top index -?[0-9]+\)"]
exact_family("wui_menu_nav focus: focused item ",focus_patterns,"DEBUG","ui")
exact_family("wui_menu_nav: K_ENTER dispatched",[r"wui_menu_nav: K_ENTER dispatched"]*3,"DEBUG","ui")
exact_family("wui_menu_nav: K_DOWNARROW dispatched",[r"wui_menu_nav: K_DOWNARROW dispatched"],"DEBUG","ui")
exact_family("WiredUI: pop menu",[r"WiredUI: pop menu \(depth 0\)"],"DEBUG","ui")
exact_family("WiredUI: shutdown",[r"WiredUI: shutdown"],"INFO","ui")
if not (query_rows[0][0]<unauthorized[0][0]<source_kind[0][0]<malformed[0][0]<markers[0][0]<expired[0][0]<inactive[0][0]<markers[1][0]<query_rows[1][0]<master_i<accepted_detail[0][0]<first_ping_i):raise SystemExit("FAIL authority order")
print("  PASS global discovery: atomic reject -> deadline expiry -> fresh authorized recovery")
PYEOF
}

if [ "${1:-}" = --self-test ]; then
 ROOT="$(mktemp -d -t wired-global-self-XXXXXX 2>/dev/null || mktemp -d)"; trap 'rm -rf "$ROOT"' EXIT
 P="$ROOT/p"; F="$ROOT/f"
 python3 - "$P" "$F" <<'PYEOF'
import hashlib,json,socket,struct,sys
p,f=sys.argv[1:3];ports={"master":30001,"rogue":30002,"target":30003,"sentinel":30004};a={k:f"127.0.0.1:{v}" for k,v in ports.items()};one="1"*32;two="2"*32
msgs=["WiredUI: push menu 'servers' (depth 1)","wui_menu_nav focus: focused item 'filter_source' (top index -1)","wui_menu_nav: K_ENTER dispatched","wui_menu_nav focus: focused item 'btn_refresh' (top index -1)","wui_menu_nav: K_ENTER dispatched",f"Master query generation=7 address={a['master']} extended=0 timeout=3000ms",f"CL packet {a['rogue']}: getserversResponse",f"Ignored unauthorized getserversResponse from {a['rogue']}",f"Ignored master response reason=source-or-kind generation=7 source={a['rogue']} kind=classic",f"CL packet {a['master']}: getserversResponse",f"Ignored master response reason=malformed generation=7 source={a['master']} kind=classic","WiredUI: server roster generation=2 source=1 displayed=0 raw=0","wui_listbox_sort: sorted feeder 2 on item 'serverlist' by column 0","GLOBAL_BROWSER_ZERO_1",f"CL packet {a['master']}: getserversResponse",f"Ignored master response reason=expired generation=7 source={a['master']} kind=classic elapsed=3201ms",f"CL packet {a['master']}: getserversResponse",f"Ignored master response reason=inactive source={a['master']} kind=classic","WiredUI: server roster generation=3 source=1 displayed=0 raw=0","wui_listbox_sort: sorted feeder 2 on item 'serverlist' by column 0","GLOBAL_BROWSER_ZERO_2","wui_menu_nav focus: focused item 'btn_refresh' (top index -1)","wui_menu_nav: K_ENTER dispatched",f"Master query generation=8 address={a['master']} extended=0 timeout=3000ms",f"CL packet {a['master']}: getserversResponse",f"Accepted getserversResponse generation=8 address={a['master']} parsed=2 total=2",f"Accepted master response generation=8 source_index=0 source={a['master']} kind=classic parsed=2 added=2 total=2 elapsed=201ms","WiredUI: server roster generation=4 source=1 displayed=0 raw=2",f"Ping request generation=11 challenge={one} timeout=800ms address={a['target']}",f"CL packet {a['target']}: infoResponse BAD",f"Ignored infoResponse challenge mismatch generation=11 from {a['target']}",f"CL packet {a['target']}: infoResponse GOOD",f"Accepted ping infoResponse generation=11 time=17ms address={a['target']}",f"Ping request generation=12 challenge={two} timeout=800ms address={a['sentinel']}",f"CL packet {a['sentinel']}: infoResponse BAD",f"Ignored infoResponse challenge mismatch generation=12 from {a['sentinel']}",f"CL packet {a['sentinel']}: infoResponse GOOD",f"Accepted ping infoResponse generation=12 time=19ms address={a['sentinel']}","WiredUI: server roster generation=5 source=1 displayed=2 raw=2",f"WiredUI: server roster row=0 raw=0 address={a['target']} name=Z0 WIRED GLOBAL TARGET map=arena7",f"WiredUI: server roster row=1 raw=1 address={a['sentinel']} name=A0 WIRED GLOBAL SENTINEL map=arena1",f"WiredUI: server roster row=0 raw=1 address={a['sentinel']} name=A0 WIRED GLOBAL SENTINEL map=arena1",f"WiredUI: server roster row=1 raw=0 address={a['target']} name=Z0 WIRED GLOBAL TARGET map=arena7","wui_listbox_sort: sorted feeder 2 on item 'serverlist' by column 0",f"WiredUI: server roster row=0 raw=1 address={a['sentinel']} name=A0 WIRED GLOBAL SENTINEL map=arena1",f"WiredUI: server roster row=1 raw=0 address={a['target']} name=Z0 WIRED GLOBAL TARGET map=arena7","wui_menu_nav focus: focused item 'serverlist' (top index 1)",f"WiredUI: server selection display_row=0 raw=1 source=1 list_generation=5 selection_generation=2 address={a['sentinel']} name=A0 WIRED GLOBAL SENTINEL map=arena1",f"WiredUI: server selection display_row=1 raw=0 source=1 list_generation=5 selection_generation=3 address={a['target']} name=Z0 WIRED GLOBAL TARGET map=arena7","wui_menu_nav: K_DOWNARROW dispatched","WiredUI: pop menu (depth 0)","WiredUI: shutdown"]
# Model the consumer-owned eight-publication cadence explicitly while keeping
# the long fixture above readable as the semantic event transcript.
msgs=[x for x in msgs if not x.startswith("WiredUI: server roster generation=")]
def before(prefix,text,occurrence=0):
 indexes=[i for i,x in enumerate(msgs) if x.startswith(prefix)];msgs.insert(indexes[occurrence],text)
before("wui_menu_nav focus: focused item 'filter_source'","WiredUI: server roster generation=1 source=0 displayed=0 raw=0")
before("Master query generation=7","WiredUI: server roster generation=2 source=1 displayed=0 raw=0")
before("wui_listbox_sort: sorted feeder", "WiredUI: server roster generation=3 source=1 displayed=0 raw=0",0)
before("wui_listbox_sort: sorted feeder", "WiredUI: server roster generation=4 source=1 displayed=0 raw=0",1)
before("Master query generation=8","WiredUI: server roster generation=5 source=1 displayed=0 raw=0")
before("Ping request generation=11","WiredUI: server roster generation=6 source=1 displayed=0 raw=2")
target_accept=next(i for i,x in enumerate(msgs) if x.startswith("Accepted ping infoResponse generation=11"))
msgs[target_accept+1:target_accept+1]=[f"Ping result consumed slot=0 owner=browser-global generation=11 outcome=completed time=17ms cache_action=publish matched=1 consumer=current address={a['target']}",f"Ping transaction cleared slot=0 owner=browser-global generation=11 state=completed age=1017ms address={a['target']}"]
sentinel_accept=next(i for i,x in enumerate(msgs) if x.startswith("Accepted ping infoResponse generation=12"))
msgs[sentinel_accept+1:sentinel_accept+1]=[f"Ping result consumed slot=1 owner=browser-global generation=12 outcome=completed time=19ms cache_action=publish matched=1 consumer=current address={a['sentinel']}",f"Ping transaction cleared slot=1 owner=browser-global generation=12 state=completed age=1019ms address={a['sentinel']}"]
msgs=[x for x in msgs if not x.startswith("WiredUI: server roster row=")]
sentinel_clear=next(i for i,x in enumerate(msgs) if x.startswith("Ping transaction cleared ") and "generation=12 " in x)
msgs[sentinel_clear+1:sentinel_clear+1]=["WiredUI: server roster generation=7 source=1 displayed=2 raw=2",f"WiredUI: server roster row=0 raw=0 address={a['target']} name=Z0 WIRED GLOBAL TARGET map=arena7",f"WiredUI: server roster row=1 raw=1 address={a['sentinel']} name=A0 WIRED GLOBAL SENTINEL map=arena1"]
server_focus=next(i for i,x in enumerate(msgs) if x.startswith("wui_menu_nav focus: focused item 'serverlist'"))
final_sort=max(i for i,x in enumerate(msgs) if x.startswith("wui_listbox_sort: sorted feeder"))
msgs[final_sort:final_sort]=["WiredUI: server roster generation=8 source=1 displayed=2 raw=2",f"WiredUI: server roster row=0 raw=1 address={a['sentinel']} name=A0 WIRED GLOBAL SENTINEL map=arena1",f"WiredUI: server roster row=1 raw=0 address={a['target']} name=Z0 WIRED GLOBAL TARGET map=arena7"]
msgs=[x.replace("list_generation=5","list_generation=8") if x.startswith("WiredUI: server selection ") else x for x in msgs]
selection_tail=[x for x in msgs if x.startswith("WiredUI: server selection ")]
selection_focus=next(x for x in msgs if x.startswith("wui_menu_nav focus: focused item 'serverlist'"))
msgs=[x for x in msgs if x not in selection_tail and x!=selection_focus]
final_sort=max(i for i,x in enumerate(msgs) if x.startswith("wui_listbox_sort: sorted feeder"))
msgs[final_sort+1:final_sort+1]=[selection_focus,*selection_tail]
with open(p,"w") as o:
 for x in msgs:
  if x.startswith(("Master query generation=","Accepted getserversResponse","Accepted master response")):sev,cat="INFO","client"
  elif x.startswith(("CL packet ","Ignored unauthorized ","Ignored master response ","Ping request generation=","Ignored infoResponse challenge mismatch","Accepted ping infoResponse","Ping result consumed ","Ping transaction cleared ")):sev,cat="DEBUG","client"
  elif x.startswith("GLOBAL_BROWSER_ZERO_"):sev,cat="INFO","system"
  elif x=="WiredUI: shutdown":sev,cat="INFO","ui"
  else:sev,cat="DEBUG","ui"
  o.write(json.dumps({"sev":sev,"cat":cat,"msg":x})+"\n")
OOB=b"\xff"*4;CMD=b"getserversResponse"
def rec(port):return b"\\"+socket.inet_aton("127.0.0.1")+struct.pack("!H",port)
canonical=OOB+CMD+rec(ports["target"])+rec(ports["sentinel"])+b"\\EOT"
spec=[("rogue_authority","rogue",1,10,OOB+CMD+rec(ports["rogue"])+b"\\EOT"),("authorized_bare","master",1,30,OOB+CMD),("authorized_whitespace","master",1,50,OOB+CMD+b" "),("authorized_poison_then_zero_port","master",1,80,OOB+CMD+rec(ports["rogue"])+rec(0)),("expired_canonical","master",1,3201,canonical),("inactive_replay","master",1,3251,canonical),("recovery_canonical","master",2,201,canonical)]
rows=[{"event":"ready",**{k+"_port":v for k,v in ports.items()}},{"event":"request","role":"master","command":"getservers q3now 74","peer_port":40000},{"event":"master_query","query":1,"command":"getservers q3now 74","peer_port":40000}]
for scenario,source,query,elapsed,wire in spec:
 if query==2:rows += [{"event":"request","role":"master","command":"getservers q3now 74","peer_port":40000},{"event":"master_query","query":2,"command":"getservers q3now 74","peer_port":40000}]
 rows.append({"event":"packet_sent","query":query,"scenario":scenario,"source":source,"source_port":ports[source],"source_address":a[source],"peer":"127.0.0.1:40000","peer_port":40000,"elapsed_ms":elapsed,"length":len(wire),"sha256":hashlib.sha256(wire).hexdigest(),"hex":wire.hex()})
rows += [{"event":"wrong_info_response","query":2,"role":"target","challenge":"f"*32,"expected":one},{"event":"current_info_response","query":2,"role":"target","challenge":one},{"event":"wrong_info_response","query":2,"role":"sentinel","challenge":"f"*32,"expected":two},{"event":"current_info_response","query":2,"role":"sentinel","challenge":two},{"event":"stopped"}]
with open(f,"w") as o:
 for x in rows:o.write(json.dumps(x)+"\n")
PYEOF
 analyze "$P" "$F" >/dev/null || { echo 'FAIL self clean'; exit 1; }
 defects='source_focus source_enter refresh_focus refresh_enter query generation rogue_reject source_kind malformed expired inactive zero1 zero2 master_accept master_detail terminal_lf semantic_suffix extra_lf embedded_newline wrong_category wrong_severity wrong_feeder semantic_leak cl_extra cl_wrong_token cl_wrong_source accepted_g1 pre_g2_ping pre_g2_accepted_ping consume_missing clear_missing consume_owner consume_rtt poison_row arbitrary_row arbitrary_selection roster_additive row_missing row_extra row_order selection_missing selection_extra selection_order selection_list_generation selection_stale_generation selection_generation rebuild_order challenge_target challenge_sentinel challenge_reuse challenge_order wrong_target current_target wrong_sentinel current_sentinel poison_ping roster sentinel_row target_row callback down pop shutdown bad_name duplicate_master severity packet_missing packet_hex packet_sha packet_len packet_order packet_query packet_source packet_peer packet_early packet_expiry packet_replay packet_recovery malformed_product malformed_fixture'
 for d in $defects; do
  D="$ROOT/$d"; mkdir "$D"
  python3 - "$P" "$F" "$D/p" "$D/f" "$d" <<'PYEOF' || { echo "FAIL self fixture $d"; exit 1; }
import json,re,sys
ps,fs,pd,fd,d=sys.argv[1:6]; p=[json.loads(x) for x in open(ps)]; f=[json.loads(x) for x in open(fs)]
keys={"source_focus":"filter_source","rogue_reject":"unauthorized getservers","source_kind":"reason=source-or-kind","malformed":"reason=malformed","expired":"reason=expired","inactive":"reason=inactive","zero1":"GLOBAL_BROWSER_ZERO_1","zero2":"GLOBAL_BROWSER_ZERO_2","master_accept":"Accepted getserversResponse","master_detail":"Accepted master response","roster":"displayed=2 raw=2","sentinel_row":"GLOBAL SENTINEL","target_row":"GLOBAL TARGET","callback":"server selection display_row=1","down":"K_DOWNARROW","pop":"pop menu","shutdown":"WiredUI: shutdown"}
if d in keys:p=[r for r in p if keys[d] not in r["msg"]]
elif d=="source_enter":p.pop(next(i for i,r in enumerate(p) if "K_ENTER" in r["msg"]))
elif d=="refresh_enter":
 i=[i for i,r in enumerate(p) if "K_ENTER" in r["msg"]][1];p.pop(i)
elif d=="refresh_focus":p.pop([i for i,r in enumerate(p) if "btn_refresh" in r["msg"]][1])
elif d=="query":p.pop([i for i,r in enumerate(p) if "Master query" in r["msg"]][1])
elif d=="generation":
 for r in p:
  if "generation=8" in r["msg"]:r["msg"]=r["msg"].replace("generation=8","generation=7")
elif d=="semantic_suffix":
 r=next(r for r in p if r["msg"].startswith("Accepted master response"));r["msg"]+=" trailing"
elif d=="terminal_lf":
 for r in p:
  if r["msg"].startswith(("Master query generation=","Ignored unauthorized ","Ignored master response ","Accepted getserversResponse","Accepted master response","Ping request generation=","Ignored infoResponse challenge mismatch","Accepted ping infoResponse","Ping result consumed ","Ping transaction cleared ","WiredUI: server roster generation=","WiredUI: server roster row=","WiredUI: server selection ","wui_listbox_sort: sorted feeder","GLOBAL_BROWSER_ZERO_")):r["msg"]+="\n"
elif d=="extra_lf":
 r=next(r for r in p if r["msg"].startswith("Accepted master response"));r["msg"]+="\n\n"
elif d=="embedded_newline":
 r=next(r for r in p if r["msg"].startswith("Accepted master response"));r["msg"]=r["msg"].replace(" source_index", "\nsource_index")
elif d=="wrong_category":
 r=next(r for r in p if r["msg"].startswith("Ignored master response reason=malformed"));r["cat"]="ui"
elif d=="wrong_severity":
 r=next(r for r in p if r["msg"].startswith("Accepted getserversResponse"));r["sev"]="DEBUG"
elif d=="wrong_feeder":
 r=next(r for r in p if r["msg"].startswith("wui_listbox_sort: sorted feeder"));r["msg"]=r["msg"].replace("feeder 2", "feeder 1")
elif d=="semantic_leak":p.insert(9,{"sev":"DEBUG","cat":"client","msg":"CL packet 127.0.0.1:30001: getserversResponse"})
elif d=="cl_extra":p.insert(next(i for i,r in enumerate(p) if r["msg"].startswith("Accepted ping infoResponse")),{"sev":"DEBUG","cat":"client","msg":"CL packet 127.0.0.1:39999: infoResponse EXTRA"})
elif d=="cl_wrong_token":
 r=next(r for r in p if r["msg"].startswith("CL packet 127.0.0.1:30003: infoResponse"));r["msg"]=r["msg"].replace("infoResponse","statusResponse")
elif d=="cl_wrong_source":
 r=next(r for r in p if r["msg"].startswith("CL packet 127.0.0.1:30003: infoResponse"));r["msg"]=r["msg"].replace("127.0.0.1:30003","127.0.0.1:39999")
elif d=="accepted_g1":
 r=next(r for r in p if "Accepted getserversResponse" in r["msg"]);r["msg"]=r["msg"].replace("generation=8","generation=7")
elif d=="pre_g2_ping":p.insert(next(i for i,r in enumerate(p) if "Master query generation=8" in r["msg"]),{"sev":"DEBUG","cat":"client","msg":"Ping request generation=9 challenge="+"9"*32+" timeout=800ms address=127.0.0.1:30002"})
elif d=="pre_g2_accepted_ping":p.insert(next(i for i,r in enumerate(p) if "Master query generation=8" in r["msg"]),{"sev":"DEBUG","cat":"client","msg":"Accepted ping infoResponse generation=9 time=7ms address=127.0.0.1:30003"})
elif d=="consume_missing":p.remove(next(r for r in p if r["msg"].startswith("Ping result consumed ")))
elif d=="clear_missing":p.remove(next(r for r in p if r["msg"].startswith("Ping transaction cleared ")))
elif d=="consume_owner":
 r=next(r for r in p if r["msg"].startswith("Ping result consumed "));r["msg"]=r["msg"].replace("owner=browser-global","owner=manual")
elif d=="consume_rtt":
 r=next(r for r in p if r["msg"].startswith("Ping result consumed "));r["msg"]=r["msg"].replace("time=17ms","time=18ms")
elif d=="poison_row":p.insert(11,{"sev":"DEBUG","cat":"ui","msg":"WiredUI: server roster row=0 raw=0 address=127.0.0.1:30002 name=POISON map=bad"})
elif d=="arbitrary_row":p.insert(next(i for i,r in enumerate(p) if "Master query generation=8" in r["msg"]),{"sev":"DEBUG","cat":"ui","msg":"WiredUI: server roster row=7 raw=9 address=127.0.0.1:39999 name=ADDITIVE map=bad"})
elif d=="arbitrary_selection":p.insert(next(i for i,r in enumerate(p) if "Master query generation=8" in r["msg"]),{"sev":"DEBUG","cat":"ui","msg":"WiredUI: server selection display_row=7 raw=9 source=1 list_generation=2 selection_generation=2 address=127.0.0.1:39999 name=ADDITIVE map=bad"})
elif d=="roster_additive":p.insert(next(i for i,r in enumerate(p) if "generation=6 source=1 displayed=0 raw=2" in r["msg"]),{"sev":"DEBUG","cat":"ui","msg":"WiredUI: server roster generation=99 source=1 displayed=0 raw=1"})
elif d.startswith("row_"):
 rows=[r for r in p if r["msg"].startswith("WiredUI: server roster row=")]
 if d=="row_missing":p.remove(rows[0])
 elif d=="row_extra":p.insert(p.index(rows[1]),rows[1].copy())
 elif d=="row_order":
  ia=p.index(rows[0]);ib=p.index(rows[1]);p[ia],p[ib]=p[ib],p[ia]
elif d.startswith("selection_"):
 rows=[r for r in p if r["msg"].startswith("WiredUI: server selection ")]
 if d=="selection_missing":p.remove(rows[0])
 elif d=="selection_extra":p.insert(p.index(rows[1]),rows[0].copy())
 elif d=="selection_order":
  ia=p.index(rows[0]);ib=p.index(rows[1]);p[ia],p[ib]=p[ib],p[ia]
 elif d=="selection_list_generation":rows[1]["msg"]=rows[1]["msg"].replace("list_generation=8","list_generation=9")
 elif d=="selection_stale_generation":
  for r in rows:r["msg"]=r["msg"].replace("list_generation=8","list_generation=99")
 elif d=="selection_generation":rows[1]["msg"]=rows[1]["msg"].replace("selection_generation=3","selection_generation=4")
elif d=="rebuild_order":
 rebuild=next(r for r in p if r["msg"].startswith("WiredUI: server roster generation=8 "));first_clear=next(r for r in p if r["msg"].startswith("Ping transaction cleared "))
 p.remove(rebuild);p.insert(p.index(first_clear),rebuild)
elif d in ("challenge_target","challenge_sentinel"):
 port="30003" if d.endswith("target") else "30004";p=[r for r in p if not ("Ping request" in r["msg"] and port in r["msg"])]
elif d=="challenge_reuse":
 first=next(re.search(r"challenge=([0-9a-f]+)",r["msg"]).group(1) for r in p if "Ping request" in r["msg"])
 for r in p:
  if "Ping request" in r["msg"] and "30004" in r["msg"]:r["msg"]=re.sub(r"challenge=[0-9a-f]+",f"challenge={first}",r["msg"])
 for r in f:
  if r.get("role")=="sentinel" and r.get("event")=="wrong_info_response":r["expected"]=first
  if r.get("role")=="sentinel" and r.get("event")=="current_info_response":r["challenge"]=first
elif d=="challenge_order":
 mismatch=next(r for r in p if r["msg"].startswith("Ignored infoResponse challenge mismatch") and "30003" in r["msg"]);accepted=next(r for r in p if r["msg"].startswith("Accepted ping infoResponse") and "30003" in r["msg"])
 ia=p.index(mismatch);ib=p.index(accepted);p[ia],p[ib]=p[ib],p[ia]
elif d in ("wrong_target","current_target","wrong_sentinel","current_sentinel"):
 event,role=d.split('_'); en="wrong_info_response" if event=="wrong" else "current_info_response";f=[r for r in f if not(r.get("event")==en and r.get("role")==role)]
elif d=="poison_ping":f.append({"event":"poison_ping"})
elif d=="bad_name":p.append({"sev":"DEBUG","cat":"ui","msg":"BAD TARGET"})
elif d=="duplicate_master":
 r=next(r for r in p if "Accepted getserversResponse" in r["msg"]);p.insert(p.index(r),r.copy())
elif d=="severity":p.append({"sev":"ERROR","cat":"renderer","msg":"synthetic"})
elif d.startswith("packet_"):
 rows=[r for r in f if r.get("event")=="packet_sent"]
 if d=="packet_missing":f.remove(rows[2])
 elif d=="packet_hex":rows[3]["hex"]="00"+rows[3]["hex"][2:]
 elif d=="packet_sha":rows[3]["sha256"]="0"*64
 elif d=="packet_len":rows[3]["length"]+=1
 elif d=="packet_order":
  ia=f.index(rows[1]);ib=f.index(rows[2]);f[ia],f[ib]=f[ib],f[ia]
 elif d=="packet_query":rows[-1]["query"]=1
 elif d=="packet_source":rows[3]["source"]="rogue"
 elif d=="packet_peer":rows[3]["peer_port"]+=1
 elif d=="packet_early":rows[2]["elapsed_ms"]=rows[1]["elapsed_ms"]-1
 elif d=="packet_expiry":rows[4]["elapsed_ms"]=2999
 elif d=="packet_replay":rows[5]["elapsed_ms"]=4000
 elif d=="packet_recovery":rows[6]["elapsed_ms"]=3000
with open(pd,"w") as o:
 [o.write(json.dumps(r)+"\n") for r in p]
 if d=="malformed_product":o.write("{bad\n")
with open(fd,"w") as o:
 [o.write(json.dumps(r)+"\n") for r in f]
 if d=="malformed_fixture":o.write("[]\n")
PYEOF
  if [ "$d" = terminal_lf ]; then
   analyze "$D/p" "$D/f" >/dev/null || { echo "FAIL self $d"; exit 1; }
  elif analyze "$D/p" "$D/f" >/dev/null 2>&1; then echo "FAIL self $d"; exit 1
  fi
 done
 echo '==> WiredUI global-browser analyzer self-test: PASS'; exit 0
fi

command -v python3 >/dev/null 2>&1 && [ -f "$FIXTURE" ] && [ -f "$TIMEOUT_RUNNER" ] || { echo 'SKIP: Python unavailable'; exit 77; }
WIRED="${1:-}"; [ -n "$WIRED" ] && [ -x "$WIRED" ] || { echo 'SKIP: pass assembled Wired GUI binary'; exit 77; }
WIRED="$(cd "$(dirname "$WIRED")" && pwd)/$(basename "$WIRED")"; WD="$(dirname "$WIRED")"
PACK=""; for c in "$WD" "$WD/../Resources" "$WD/../../.."; do [ -f "$c/base/pax21.sw3z" ] && PACK="$(cd "$c" && pwd)" && break; done
[ -n "$PACK" ] || { echo 'SKIP: pax21 missing'; exit 77; }
BASE="${WIRED_CONTENT_ROOT:-$PACK}"; if [ -f "$BASE/base/pax01.sw3z" ]; then ARCH="$BASE/base/pax01.sw3z"; elif [ -f "$BASE/base/pak0.pk3" ]; then ARCH="$BASE/base/pak0.pk3"; else echo 'SKIP: base archive missing'; exit 77; fi
ROOT="$(mktemp -d -t wired-global-XXXXXX 2>/dev/null || mktemp -d)"; HOME_DIR="$ROOT/q3now-preview"; EVENTS="$ROOT/fixture.jsonl"; STDOUT="$ROOT/stdout"; PID=""
cleanup(){ [ -n "$PID" ] && kill -TERM "$PID" 2>/dev/null || true; [ -n "$PID" ] && wait "$PID" 2>/dev/null || true; [ "${WIRED_KEEP_ARTIFACTS:-0}" = 1 ] || rm -rf "$ROOT"; }; trap cleanup EXIT INT TERM
mkdir -p "$HOME_DIR/base"; cp "$ARCH" "$HOME_DIR/base/" || exit 1; cp "$PACK/base/pax21.sw3z" "$HOME_DIR/base/pax21.sw3z" || exit 1
python3 "$FIXTURE" --events "$EVENTS" --timeout 90 & PID=$!; for _ in $(seq 1 100); do [ -s "$EVENTS" ] && break; sleep .05; done; [ -s "$EVENTS" ] || { echo 'FAIL fixture ready'; exit 1; }
read -r MASTER_PORT <<EOF
$(python3 - "$EVENTS" <<'PYEOF'
import json,sys
print(json.loads(open(sys.argv[1]).readline())["master_port"])
PYEOF
)
EOF
cat >"$HOME_DIR/base/global-browser.cfg" <<'CFGEOF'
wait 100
wui_push servers
wait 20
wui_menu_nav focus filter_source
wui_menu_nav enter
wui_menu_nav focus btn_refresh
wui_menu_nav enter
waitms 500
wui_listbox_sort 0
echo GLOBAL_BROWSER_ZERO_1
waitms 3000
wui_listbox_sort 0
echo GLOBAL_BROWSER_ZERO_2
wui_menu_nav focus btn_refresh
wui_menu_nav enter
waitms 2800
wui_listbox_sort 0
wait 10
wui_menu_nav focus serverlist
wui_menu_nav down
wait 20
wui_menu_nav back
wait 20
quit
CFGEOF
case "$(uname -s)" in Darwin) NATIVE="$HOME_DIR"; ARGS=(-ApplePersistenceIgnoreState YES);; MINGW*|MSYS*|CYGWIN*) NATIVE="$(cygpath -w "$HOME_DIR")"; ARGS=();; *) NATIVE="$HOME_DIR"; ARGS=();; esac
python3 "$TIMEOUT_RUNNER" --timeout 90 --kill-after 10 --cwd "$WD" --stdout "$STDOUT" -- "$WIRED" "${ARGS[@]}" +set fs_homepath "$NATIVE" +set com_automated 1 +set com_noHardReboot 1 +set net_enabled 1 +set net_ip 127.0.0.1 +set net_port 0 +set sv_master1 "127.0.0.1:$MASTER_PORT" +set s_initsound 0 +set r_fullscreen 0 +set log_severity DEBUG +set log_file_severity DEBUG +set log_file_mode overwrite_synced +exec global-browser.cfg
rc=$?; [ "$rc" -eq 0 ] && [ -s "$HOME_DIR/qconsole.jsonl" ] || { echo "FAIL product rc=$rc"; exit 1; }
kill -TERM "$PID" 2>/dev/null || true; wait "$PID" || { echo 'FAIL fixture exit'; exit 1; }; PID=""
analyze "$HOME_DIR/qconsole.jsonl" "$EVENTS" || exit 1
echo '==> WiredUI global-browser gate: PASS'
