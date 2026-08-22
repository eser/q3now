#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors
# Real 34-request directed-ping queue retirement and oldest-eviction contract.

set -uo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
FIXTURE="$SCRIPT_DIR/directed-ping-queue-fixture.py"
TIMEOUT_RUNNER="$SCRIPT_DIR/run-with-timeout.py"

analyze() {
python3 - "$1" "$2" <<'PYEOF'
import hashlib,json,re,sys
product_path,fixture_path=sys.argv[1:]
def load(path):
 rows=[]
 for line_no,line in enumerate(open(path,encoding="utf-8",errors="replace"),1):
  try:row=json.loads(line)
  except Exception as exc:raise SystemExit(f"FAIL {path}:{line_no}: {exc}")
  if not isinstance(row,dict):raise SystemExit(f"FAIL {path}:{line_no}: schema")
  rows.append(row)
 return rows
p=load(product_path);f=load(fixture_path)
if not p or not f:raise SystemExit("FAIL empty evidence")
if any(not all(isinstance(row.get(key),str) for key in ("sev","cat","msg")) for row in p):raise SystemExit("FAIL product schema")
if any(not isinstance(row.get("event"),str) for row in f):raise SystemExit("FAIL fixture schema")
if any(row["sev"].upper() in ("ERROR","FATAL") or row["sev"].upper()=="WARN" and row["cat"].lower()=="ui" for row in p):raise SystemExit("FAIL product severity")
def norm(value):
 if value.endswith("\n"):value=value[:-1]
 return value
m=[norm(row["msg"]) for row in p]
critical=("Ping queue allocation ","Ping request generation=","CL packet ",
          "Accepted ping infoResponse ","Ignored unsolicited infoResponse ")
for value in m:
 if ("\n" in value or "\r" in value) and any(line.startswith(critical) for line in re.split(r"[\r\n]",value)):
  raise SystemExit("FAIL critical newline")
ready=[row for row in f if row["event"]=="ready"]
stopped=[row for row in f if row["event"]=="stopped"]
if len(ready)!=1 or f[0] is not ready[0] or ready[0].get("protocol")!=74:raise SystemExit("FAIL ready")
ports=ready[0].get("ports")
if not isinstance(ports,list) or len(ports)!=34 or len(set(ports))!=34 or any(not isinstance(x,int) or not 0<x<65536 for x in ports):raise SystemExit("FAIL ports")
if len(stopped)!=1 or f[-1] is not stopped[0] or stopped[0].get("reason")!="signal" or stopped[0].get("request_count")!=34:raise SystemExit("FAIL fixture stop")
if any(row["event"] in ("unexpected_request","duplicate_request","fixture_error") for row in f):raise SystemExit("FAIL fixture error")
requests=[row for row in f if row["event"]=="request"]
responses=[row for row in f if row["event"]=="response"]
if len(requests)!=34 or [row.get("request_ordinal") for row in requests]!=list(range(1,35)):raise SystemExit("FAIL request inventory/order")
if len(responses)!=3 or [(row.get("scenario"),row.get("request_ordinal")) for row in responses] != [("completed_1",1),("stale_2_after_evict",2),("current_34",34)]:raise SystemExit("FAIL response inventory")
OOB=b"\xff"*4;by_ordinal={row["request_ordinal"]:row for row in requests}
client_port=requests[0].get("source_port")
if not isinstance(client_port,int) or any(row.get("source")!=f"127.0.0.1:{client_port}" or row.get("source_port")!=client_port for row in requests):raise SystemExit("FAIL client identity")
challenges=[]
def check_wire(row,expected,label):
 try:observed=bytes.fromhex(row.get("hex",""))
 except ValueError:raise SystemExit(f"FAIL {label} hex")
 if observed!=expected or row.get("length")!=len(expected) or row.get("sha256")!=hashlib.sha256(expected).hexdigest():raise SystemExit(f"FAIL {label} bytes")
for ordinal in range(1,35):
 row=by_ordinal[ordinal];challenge=row.get("challenge");challenges.append(challenge)
 if re.fullmatch(r"[0-9a-f]{32}",challenge or "") is None:raise SystemExit("FAIL request challenge")
 if row.get("peer")!=f"127.0.0.1:{ports[ordinal-1]}" or row.get("peer_port")!=ports[ordinal-1]:raise SystemExit("FAIL request peer")
 if not isinstance(row.get("elapsed_ms"),int) or row["elapsed_ms"]<0:raise SystemExit("FAIL request timing")
 check_wire(row,OOB+b"getinfo "+challenge.encode(),f"request{ordinal}")
if len(set(challenges))!=34:raise SystemExit("FAIL challenge freshness")
timed=[row["elapsed_ms"] for row in f if row["event"] in ("request","response")]
if timed != sorted(timed):raise SystemExit("FAIL fixture elapsed chronology")
if not by_ordinal[2]["elapsed_ms"] < min(by_ordinal[n]["elapsed_ms"] for n in range(3,33)):raise SystemExit("FAIL request2 oldest-live authority")
def response(challenge,hostname):
 info=(f"\\challenge\\{challenge}\\protocol\\74\\hostname\\{hostname}\\mapname\\arena7\\clients\\1\\sv_maxclients\\8\\game\\q3now\\gametype\\0\\minping\\0\\maxping\\0")
 return OOB+b"infoResponse\n"+info.encode()
expected_responses=[response(challenges[0],"QUEUE COMPLETED ONE"),response(challenges[1],"QUEUE STALE TWO"),response(challenges[33],"QUEUE CURRENT THIRTY FOUR")]
for row,wire in zip(responses,expected_responses):
 ordinal=row["request_ordinal"]
 if row.get("source")!=f"127.0.0.1:{ports[ordinal-1]}" or row.get("source_port")!=ports[ordinal-1] or row.get("peer")!=f"127.0.0.1:{client_port}" or row.get("peer_port")!=client_port:raise SystemExit("FAIL response identity")
 if not isinstance(row.get("elapsed_ms"),int):raise SystemExit("FAIL response timing")
 check_wire(row,wire,row["scenario"])
event_index={id(row):index for index,row in enumerate(f)}
if not event_index[id(by_ordinal[1])]<event_index[id(responses[0])]<event_index[id(by_ordinal[2])]<event_index[id(by_ordinal[33])]<event_index[id(by_ordinal[34])]<event_index[id(responses[1])]<event_index[id(responses[2])]:raise SystemExit("FAIL fixture causal order")
if not 500 <= responses[0]["elapsed_ms"]-by_ordinal[1]["elapsed_ms"] < 999:raise SystemExit("FAIL completed1 deadline")
if not 0 < responses[2]["elapsed_ms"]-by_ordinal[34]["elapsed_ms"] < 999:raise SystemExit("FAIL current34 deadline")
def exact(prefix,patterns,sev="DEBUG",cat="client"):
 rows=[(i,row,value) for i,(row,value) in enumerate(zip(p,m)) if value.startswith(prefix)]
 if len(rows)!=len(patterns):raise SystemExit(f"FAIL {prefix} cardinality")
 for (_,row,value),pattern in zip(rows,patterns):
  if row["sev"]!=sev or row["cat"]!=cat or re.fullmatch(pattern,value) is None:raise SystemExit(f"FAIL {prefix} format/metadata")
 return rows
addresses=[f"127.0.0.1:{port}" for port in ports]
ea=[re.escape(value) for value in addresses]
allocation_patterns=[]
for slot in range(32):allocation_patterns.append(rf"Ping queue allocation slot={slot} reason=free previous_generation=0 previous_state=empty age=0ms address=none")
allocation_patterns += [rf"Ping queue allocation slot=0 reason=completed previous_generation=([1-9][0-9]*) previous_state=completed age=([0-9]+)ms address={ea[0]}",rf"Ping queue allocation slot=1 reason=oldest-pending previous_generation=([1-9][0-9]*) previous_state=pending age=([0-9]+)ms address={ea[1]}"]
alloc=exact("Ping queue allocation ",allocation_patterns)
request_pattern=[rf"Ping request generation=([1-9][0-9]*) challenge=([0-9a-f]{{32}}) timeout=999ms address={address}" for address in ea]
req=exact("Ping request generation=",request_pattern)
generations=[int(re.fullmatch(pattern,row[2]).group(1)) for row,pattern in zip(req,request_pattern)]
if generations!=list(range(generations[0],generations[0]+34)):raise SystemExit("FAIL generation sequence")
if [re.fullmatch(pattern,row[2]).group(2) for row,pattern in zip(req,request_pattern)]!=challenges:raise SystemExit("FAIL challenge binding")
done_match=re.fullmatch(allocation_patterns[32],alloc[32][2]);old_match=re.fullmatch(allocation_patterns[33],alloc[33][2])
if int(done_match.group(1))!=generations[0] or int(old_match.group(1))!=generations[1]:raise SystemExit("FAIL previous generation")
if not 500<=int(done_match.group(2)) or not 0<int(old_match.group(2))<999:raise SystemExit("FAIL allocation age")
for index in range(34):
 if not alloc[index][0]<req[index][0]:raise SystemExit("FAIL allocation/request order")
for index in range(33):
 if not req[index][0]<alloc[index+1][0]:raise SystemExit("FAIL request/next-allocation order")
packets=exact("CL packet ",[rf"CL packet {ea[0]}: infoResponse",rf"CL packet {ea[1]}: infoResponse",rf"CL packet {ea[33]}: infoResponse"])
accepted=exact("Accepted ping infoResponse ",[rf"Accepted ping infoResponse generation={generations[0]} time=([1-9][0-9]*)ms address={ea[0]}",rf"Accepted ping infoResponse generation={generations[33]} time=([1-9][0-9]*)ms address={ea[33]}"])
accepted_ms=[int(re.fullmatch(pattern,row[2]).group(1)) for row,pattern in zip(accepted,[rf"Accepted ping infoResponse generation={generations[0]} time=([1-9][0-9]*)ms address={ea[0]}",rf"Accepted ping infoResponse generation={generations[33]} time=([1-9][0-9]*)ms address={ea[33]}"])]
if not 500<=accepted_ms[0]<999 or not 0<accepted_ms[1]<999:raise SystemExit("FAIL accepted deadline")
inactive=exact("Ignored unsolicited infoResponse ",[rf"Ignored unsolicited infoResponse from {ea[1]}"])
if not req[0][0]<packets[0][0]<accepted[0][0]<alloc[1][0]<req[1][0]<alloc[32][0]<req[32][0]<alloc[33][0]<req[33][0]<packets[1][0]<inactive[0][0]<packets[2][0]<accepted[1][0]:raise SystemExit("FAIL product causal order")
for prefix in ("Ignored expired ping infoResponse","Ignored infoResponse challenge mismatch","Accepted local discovery infoResponse","Rejected overlong infoResponse","Rejected full infoResponse","Rejected invalid infoResponse"):
 if any(value.startswith(prefix) for value in m):raise SystemExit(f"FAIL forbidden {prefix}")
if any(token in "\n".join(m) for token in ("WiredUI: server roster row=","WiredUI: server selection ","Browser connect attempt armed ","QUIC client:","SV_OnPlayerConnect:","FIRST GAMEPLAY FRAME")):raise SystemExit("FAIL forbidden UI/connect")
print("PASS directed ping queue: >=500ms valid completed result retained while free, reclaimed before live under saturation; stale rejected; current accepted")
PYEOF
}

write_self_fixture() {
python3 - "$1" "$2" "$3" <<'PYEOF'
import hashlib,json,sys
pp,fp,mode=sys.argv[1:];O=b"\xff"*4;ports=list(range(30001,30035));client=40000;ch=[f"{i:032x}" for i in range(1,35)]
def wf(w):return {"length":len(w),"sha256":hashlib.sha256(w).hexdigest(),"hex":w.hex()}
def response(c,name):return O+b"infoResponse\n"+(f"\\challenge\\{c}\\protocol\\74\\hostname\\{name}\\mapname\\arena7\\clients\\1\\sv_maxclients\\8\\game\\q3now\\gametype\\0\\minping\\0\\maxping\\0").encode()
F=[{"event":"ready","protocol":74,"ports":ports}]
def request(n,t):
 w=O+b"getinfo "+ch[n-1].encode();return {"event":"request","request_ordinal":n,"challenge":ch[n-1],"source":f"127.0.0.1:{client}","source_port":client,"peer":f"127.0.0.1:{ports[n-1]}","peer_port":ports[n-1],"elapsed_ms":t,**wf(w)}
def sent(s,n,t,name):
 w=response(ch[n-1],name);return {"event":"response","scenario":s,"request_ordinal":n,"source":f"127.0.0.1:{ports[n-1]}","source_port":ports[n-1],"peer":f"127.0.0.1:{client}","peer_port":client,"elapsed_ms":t,**wf(w)}
F += [request(1,10),sent("completed_1",1,610,"QUEUE COMPLETED ONE")]
F += [request(2,710)] + [request(n,740+n) for n in range(3,33)] + [request(33,930),request(34,950),sent("stale_2_after_evict",2,970,"QUEUE STALE TWO"),sent("current_34",34,1010,"QUEUE CURRENT THIRTY FOUR"),{"event":"stopped","reason":"signal","request_count":34}]
P=[]
def add(msg,sev="DEBUG",cat="client"):P.append({"sev":sev,"cat":cat,"msg":msg})
for n in range(1,35):
 if n<=32:add(f"Ping queue allocation slot={n-1} reason=free previous_generation=0 previous_state=empty age=0ms address=none")
 elif n==33:add(f"Ping queue allocation slot=0 reason=completed previous_generation=7 previous_state=completed age=920ms address=127.0.0.1:{ports[0]}")
 else:add(f"Ping queue allocation slot=1 reason=oldest-pending previous_generation=8 previous_state=pending age=240ms address=127.0.0.1:{ports[1]}")
 add(f"Ping request generation={n+6} challenge={ch[n-1]} timeout=999ms address=127.0.0.1:{ports[n-1]}")
 if n==1:
  add(f"CL packet 127.0.0.1:{ports[0]}: infoResponse")
  add(f"Accepted ping infoResponse generation=7 time=600ms address=127.0.0.1:{ports[0]}")
add(f"CL packet 127.0.0.1:{ports[1]}: infoResponse");add(f"Ignored unsolicited infoResponse from 127.0.0.1:{ports[1]}")
add(f"CL packet 127.0.0.1:{ports[33]}: infoResponse");add(f"Accepted ping infoResponse generation=40 time=60ms address=127.0.0.1:{ports[33]}")
def row(prefix):return next(r for r in P if r["msg"].startswith(prefix))
if mode=="completed_magic500":row("Ping queue allocation slot=0 reason=completed")["msg"]=row("Ping queue allocation slot=0 reason=completed")["msg"].replace("reason=completed","reason=oldest-pending").replace("previous_state=completed","previous_state=pending")
elif mode=="wrong_oldest":row("Ping queue allocation slot=1 reason=oldest-pending")["msg"]=row("Ping queue allocation slot=1 reason=oldest-pending")["msg"].replace("slot=1","slot=2")
elif mode=="extra_alloc":P.insert(2,dict(P[0]))
elif mode=="missing_alloc":P.remove(row("Ping queue allocation slot=0 reason=completed"))
elif mode=="alloc_category":row("Ping queue allocation slot=1 reason=oldest-pending")["cat"]="ui"
elif mode=="alloc_suffix":row("Ping queue allocation slot=1 reason=oldest-pending")["msg"]+=" EXTRA"
elif mode=="alloc_newline":row("Ping queue allocation slot=1 reason=oldest-pending")["msg"]+="\nEXTRA"
elif mode=="bad_previous_generation":row("Ping queue allocation slot=1 reason=oldest-pending")["msg"]=row("Ping queue allocation slot=1 reason=oldest-pending")["msg"].replace("previous_generation=8","previous_generation=9")
elif mode=="bad_completed_age":row("Ping queue allocation slot=0 reason=completed")["msg"]=row("Ping queue allocation slot=0 reason=completed")["msg"].replace("age=920ms","age=499ms")
elif mode=="bad_oldest_age":row("Ping queue allocation slot=1 reason=oldest-pending")["msg"]=row("Ping queue allocation slot=1 reason=oldest-pending")["msg"].replace("age=240ms","age=0ms")
elif mode=="stale_accepted":row("Ignored unsolicited infoResponse")["msg"]=f"Accepted ping infoResponse generation=8 time=260ms address=127.0.0.1:{ports[1]}"
elif mode=="missing_current":P.remove(row("Accepted ping infoResponse generation=40"))
elif mode=="duplicate_current":P.append(dict(row("Accepted ping infoResponse generation=40")))
elif mode=="wrong_request_slot":row("Ping request generation=40")["msg"]=row("Ping request generation=40")["msg"].replace(str(ports[33]),str(ports[32]))
elif mode=="generation_gap":row("Ping request generation=20")["msg"]=row("Ping request generation=20")["msg"].replace("generation=20","generation=21")
elif mode=="challenge_duplicate":F[35]["challenge"]=ch[0];w=O+b"getinfo "+ch[0].encode();F[35].update(wf(w))
elif mode=="request_hash":F[10]["sha256"]="0"*64
elif mode=="response_hash":next(r for r in F if r.get("scenario")=="current_34")["sha256"]="0"*64
elif mode=="wrong_peer":F[10]["peer_port"]=1
elif mode=="bad_protocol":F[0]["protocol"]=75
elif mode=="bad_stop":F[-1]["reason"]="timeout"
elif mode=="response_order":F[-3],F[-2]=F[-2],F[-3]
elif mode=="early_completed":next(r for r in F if r.get("scenario")=="completed_1")["elapsed_ms"]=500
elif mode=="completed_at_500":
 next(r for r in F if r.get("scenario")=="completed_1")["elapsed_ms"]=510
 row("Accepted ping infoResponse generation=7")["msg"]=row("Accepted ping infoResponse generation=7")["msg"].replace("time=600ms","time=500ms")
elif mode=="completed_at_499":
 next(r for r in F if r.get("scenario")=="completed_1")["elapsed_ms"]=509
 row("Accepted ping infoResponse generation=7")["msg"]=row("Accepted ping infoResponse generation=7")["msg"].replace("time=600ms","time=499ms")
elif mode=="late_current":next(r for r in F if r.get("scenario")=="current_34")["elapsed_ms"]=2000
elif mode=="alloc_after_request":
 ai=next(i for i,r in enumerate(P) if r["msg"].startswith("Ping queue allocation slot=19 "))
 ri=next(i for i,r in enumerate(P) if r["msg"].startswith("Ping request generation=26 "))
 P[ai],P[ri]=P[ri],P[ai]
elif mode=="next_alloc_before_request":
 ri=next(i for i,r in enumerate(P) if r["msg"].startswith("Ping request generation=25 "))
 ai=next(i for i,r in enumerate(P) if r["msg"].startswith("Ping queue allocation slot=19 "))
 P[ri],P[ai]=P[ai],P[ri]
elif mode=="request_event_order":
 a=next(i for i,r in enumerate(F) if r.get("event")=="request" and r.get("request_ordinal")==10)
 b=next(i for i,r in enumerate(F) if r.get("event")=="request" and r.get("request_ordinal")==11)
 F[a],F[b]=F[b],F[a]
elif mode=="request3_older_than_2":
 next(r for r in F if r.get("event")=="request" and r.get("request_ordinal")==3)["elapsed_ms"]=700
elif mode=="forbidden_row":add("WiredUI: server roster row=0 raw=0 address=127.0.0.1:30001 name=LEAK map=arena7",cat="ui")
elif mode=="unsolicited_suffix":row("Ignored unsolicited infoResponse")["msg"]+=" EXTRA"
elif mode=="packet_extra":add(f"CL packet 127.0.0.1:{ports[0]}: infoResponse")
elif mode=="terminal_lf":
 for r in P:r["msg"]+="\n"
for path,rows in ((pp,P),(fp,F)):
 with open(path,"w",encoding="utf-8") as out:
  for value in rows:out.write(json.dumps(value)+"\n")
PYEOF
}

if [ "${1:-}" = --self-test ]; then
  ROOT="$(mktemp -d -t directed-ping-queue-self-XXXXXX 2>/dev/null || mktemp -d)"
  trap 'rm -rf "$ROOT"' EXIT
  write_self_fixture "$ROOT/product" "$ROOT/fixture" clean
  analyze "$ROOT/product" "$ROOT/fixture" >/dev/null || exit 1
  write_self_fixture "$ROOT/product-500" "$ROOT/fixture-500" completed_at_500
  analyze "$ROOT/product-500" "$ROOT/fixture-500" >/dev/null || { echo "FAIL self completed_at_500"; exit 1; }
  for defect in completed_magic500 wrong_oldest extra_alloc missing_alloc alloc_category alloc_suffix alloc_newline bad_previous_generation bad_completed_age bad_oldest_age stale_accepted missing_current duplicate_current wrong_request_slot generation_gap challenge_duplicate request_hash response_hash wrong_peer bad_protocol bad_stop response_order early_completed completed_at_499 late_current alloc_after_request next_alloc_before_request request_event_order request3_older_than_2 forbidden_row unsolicited_suffix packet_extra terminal_lf; do
    write_self_fixture "$ROOT/product-$defect" "$ROOT/fixture-$defect" "$defect"
    if [ "$defect" = terminal_lf ]; then
      analyze "$ROOT/product-$defect" "$ROOT/fixture-$defect" >/dev/null || { echo "FAIL self $defect"; exit 1; }
    elif analyze "$ROOT/product-$defect" "$ROOT/fixture-$defect" >/dev/null 2>&1; then
      echo "FAIL self $defect"; exit 1
    fi
  done
  echo "==> directed-ping queue analyzer self-test: PASS (33 defects rejected; exact 500ms accepted)"
  exit 0
fi

command -v python3 >/dev/null 2>&1 && [ -f "$FIXTURE" ] && [ -f "$TIMEOUT_RUNNER" ] || { echo "SKIP: Python unavailable"; exit 77; }
WIRED="${1:-}"; [ -n "$WIRED" ] && [ -x "$WIRED" ] || { echo "SKIP: pass assembled Wired GUI binary"; exit 77; }
WIRED="$(cd "$(dirname "$WIRED")" && pwd)/$(basename "$WIRED")"; WD="$(dirname "$WIRED")"
PACK=""; for candidate in "$WD" "$WD/../Resources" "$WD/q3now-preview.arm64.app/Contents/Resources"; do [ -f "$candidate/base/pax21.sw3z" ] && PACK="$candidate" && break; done
[ -n "$PACK" ] || { echo "SKIP: current pax21 unavailable"; exit 77; }
CONTENT="${WIRED_CONTENT_ROOT:-$PACK}"; if [ -f "$CONTENT/base/pax01.sw3z" ]; then BASE="$CONTENT/base/pax01.sw3z"; elif [ -f "$CONTENT/base/pak0.pk3" ]; then BASE="$CONTENT/base/pak0.pk3"; else echo "SKIP: set WIRED_CONTENT_ROOT"; exit 77; fi
ROOT="$(mktemp -d -t directed-ping-queue-XXXXXX 2>/dev/null || mktemp -d)"; EVENTS="$ROOT/fixture.jsonl"; HOME_DIR="$ROOT/q3now-preview"; PID=""
cleanup(){ [ -n "$PID" ] && kill -TERM "$PID" 2>/dev/null || true; [ -n "$PID" ] && wait "$PID" 2>/dev/null || true; [ "${WIRED_KEEP_ARTIFACTS:-0}" = 1 ] || rm -rf "$ROOT"; }; trap cleanup EXIT INT TERM
mkdir -p "$HOME_DIR/base"; cp "$PACK/base/pax21.sw3z" "$HOME_DIR/base/pax21.sw3z" || exit 1; cp "$BASE" "$HOME_DIR/base/" || exit 1
python3 "$FIXTURE" --events "$EVENTS" --timeout 30 & PID=$!
for _ in $(seq 1 100); do [ -s "$EVENTS" ] && break; sleep .05; done
[ -s "$EVENTS" ] || { echo "FAIL fixture readiness"; exit 1; }
PORTS="$(python3 - "$EVENTS" <<'PYEOF'
import json,sys
row=json.loads(open(sys.argv[1]).readline())
assert row["event"]=="ready" and row["protocol"]==74 and len(row["ports"])==34
print(" ".join(map(str,row["ports"])))
PYEOF
)" || exit 1
set -- $PORTS
CFG="$HOME_DIR/base/directed-ping-queue.cfg"
{
  echo 'set cl_maxPing 999'
  echo 'wait 100'
  echo "ping 127.0.0.1:$1"
  echo 'waitms 700'
  echo "ping 127.0.0.1:$2"
  echo 'waitms 25'
  ordinal=3
  for port in "${@:3:30}"; do echo "ping 127.0.0.1:$port"; ordinal=$((ordinal+1)); done
  echo 'waitms 150'
  echo "ping 127.0.0.1:${33}"
  echo 'waitms 20'
  echo "ping 127.0.0.1:${34}"
  echo 'waitms 300'
  echo 'quit'
} >"$CFG"
case "$(uname -s)" in Darwin) NATIVE="$HOME_DIR"; ARGS=(-ApplePersistenceIgnoreState YES);; *) NATIVE="$HOME_DIR"; ARGS=();; esac
python3 "$TIMEOUT_RUNNER" --timeout 30 --kill-after 10 --cwd "$WD" --stdout "$ROOT/stdout" -- "$WIRED" "${ARGS[@]}" +set fs_homepath "$NATIVE" +set com_automated 1 +set com_noHardReboot 1 +set net_enabled 1 +set net_ip 127.0.0.1 +set net_port 0 +set s_initsound 0 +set r_fullscreen 0 +set r_mode -1 +set r_customwidth 1280 +set r_customheight 720 +set log_severity DEBUG +set log_file_severity DEBUG +set log_file_mode overwrite_synced +exec directed-ping-queue.cfg
rc=$?; [ "$rc" -eq 0 ] && [ -s "$HOME_DIR/qconsole.jsonl" ] || { echo "FAIL product rc=$rc"; exit 1; }
kill -TERM "$PID" 2>/dev/null || true; wait "$PID" || exit 1; PID=""
analyze "$HOME_DIR/qconsole.jsonl" "$EVENTS"
