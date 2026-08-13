#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Real /ping timeout and same-address fresh-generation contract.

set -uo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
FIXTURE="$SCRIPT_DIR/directed-ping-fixture.py"
TIMEOUT_RUNNER="$SCRIPT_DIR/run-with-timeout.py"

analyze() {
python3 - "$1" "$2" <<'PYEOF'
import hashlib,json,re,socket,sys
pp,fp=sys.argv[1:]
def load(path):
 out=[]
 for n,line in enumerate(open(path,errors="replace"),1):
  try:r=json.loads(line)
  except Exception as e:raise SystemExit(f"FAIL {path}:{n}: {e}")
  if not isinstance(r,dict):raise SystemExit(f"FAIL {path}:{n}: schema")
  out.append(r)
 return out
p=load(pp);f=load(fp)
if not p or not f:raise SystemExit("FAIL empty evidence")
if any(not all(isinstance(r.get(k),str) for k in ("sev","cat","msg")) for r in p):raise SystemExit("FAIL product schema")
if any(not isinstance(r.get("event"),str) for r in f):raise SystemExit("FAIL fixture schema")
if any(r["sev"].upper() in ("ERROR","FATAL") or r["sev"].upper()=="WARN" and r["cat"].lower()=="ui" for r in p):raise SystemExit("FAIL product severity")
ready=[r for r in f if r["event"]=="ready"]
if len(ready)!=1 or not isinstance(ready[0].get("responder_port"),int):raise SystemExit("FAIL ready")
port=ready[0]["responder_port"];address=f"127.0.0.1:{port}";ea=re.escape(address);OOB=b"\xff"*4
requests=[r for r in f if r["event"]=="request"]
if len(requests)!=2 or [r.get("ordinal") for r in requests]!=[1,2]:raise SystemExit("FAIL request cardinality")
challenges=[r.get("challenge") for r in requests]
if any(re.fullmatch(r"[0-9a-f]{32}",x or "") is None for x in challenges) or challenges[0]==challenges[1]:raise SystemExit("FAIL request challenges")
client_port=requests[0].get("source_port")
if not isinstance(client_port,int) or any(r.get("source")!=f"127.0.0.1:{client_port}" or r.get("peer")!=address or r.get("peer_port")!=port for r in requests):raise SystemExit("FAIL request identity")
for ordinal,row in enumerate(requests,1):
 wire=OOB+f"getinfo {challenges[ordinal-1]}".encode()
 try:observed=bytes.fromhex(row.get("hex",""))
 except ValueError:raise SystemExit("FAIL request hex")
 if observed!=wire or row.get("length")!=len(wire) or row.get("sha256")!=hashlib.sha256(wire).hexdigest():raise SystemExit("FAIL request bytes")
 if not isinstance(row.get("elapsed_ms"),int) or row["elapsed_ms"]<0:raise SystemExit("FAIL request timing")
def response(challenge):
 info=(f"\\challenge\\{challenge}\\protocol\\74\\hostname\\EXPIRED A\\mapname\\arena7\\clients\\1\\sv_maxclients\\8\\game\\q3now\\gametype\\0\\minping\\0\\maxping\\0")
 return OOB+b"infoResponse\n"+info.encode()
expected=[("expired_a",1,response(challenges[0])),("stale_a_under_b",2,response(challenges[0])),("current_b",2,response(challenges[1]))]
sent=[r for r in f if r["event"]=="packet_sent"]
if len(sent)!=3 or [(r.get("scenario"),r.get("request_ordinal")) for r in sent]!=[(x[0],x[1]) for x in expected]:raise SystemExit("FAIL response order")
for row,(scenario,ordinal,wire) in zip(sent,expected):
 try:observed=bytes.fromhex(row.get("hex",""))
 except ValueError:raise SystemExit(f"FAIL {scenario} hex")
 if observed!=wire or row.get("length")!=len(wire) or row.get("sha256")!=hashlib.sha256(wire).hexdigest():raise SystemExit(f"FAIL {scenario} bytes")
 if row.get("source")!=address or row.get("source_port")!=port or row.get("peer")!=f"127.0.0.1:{client_port}" or row.get("peer_port")!=client_port:raise SystemExit(f"FAIL {scenario} identity")
 if not isinstance(row.get("elapsed_ms"),int):raise SystemExit(f"FAIL {scenario} timing")
if not requests[0]["elapsed_ms"]<sent[0]["elapsed_ms"]<requests[1]["elapsed_ms"]<sent[1]["elapsed_ms"]<sent[2]["elapsed_ms"]:raise SystemExit("FAIL fixture causal timing")
if sent[0]["hex"]!=sent[1]["hex"]:raise SystemExit("FAIL stale A not byte-identical")
m=[r["msg"] for r in p]
def single_line(msg):
 if msg.endswith("\n"):msg=msg[:-1]
 if "\n" in msg or "\r" in msg:raise SystemExit("FAIL critical newline")
 return msg
def exact(prefix,patterns,sev="DEBUG",cat="client"):
 rows=[(i,r,single_line(msg)) for i,(r,msg) in enumerate(zip(p,m)) if msg.startswith(prefix)]
 if len(rows)!=len(patterns):raise SystemExit(f"FAIL {prefix} cardinality")
 for (_,r,msg),pat in zip(rows,patterns):
  if r["sev"]!=sev or r["cat"]!=cat or re.fullmatch(pat,msg) is None:raise SystemExit(f"FAIL {prefix} metadata")
 return rows
reqpat=rf"Ping request generation=([0-9]+) challenge=([0-9a-f]{{32}}) timeout=100ms address={ea}"
pr=exact("Ping request generation=",[reqpat,reqpat]);pm=[re.fullmatch(reqpat,x[2]) for x in pr]
ga,gb=(int(x.group(1)) for x in pm)
if gb!=ga+1 or [x.group(2) for x in pm]!=challenges:raise SystemExit("FAIL product request identity")
alloc=exact("Ping queue allocation ",[
 r"Ping queue allocation slot=0 reason=free previous_generation=0 previous_state=empty age=0ms address=none",
 r"Ping queue allocation slot=1 reason=free previous_generation=0 previous_state=empty age=0ms address=none",
])
timeout=int(re.search(r"timeout=([0-9]+)ms",pr[0][2]).group(1))
if timeout!=100 or sent[0]["elapsed_ms"]-requests[0]["elapsed_ms"]<timeout or sent[2]["elapsed_ms"]-requests[1]["elapsed_ms"]>=timeout:raise SystemExit("FAIL response timeout windows")
expired=exact("Ignored expired ping infoResponse",[rf"Ignored expired ping infoResponse generation={ga} from {ea}"]*2)
exact("Ignored infoResponse challenge mismatch",[])
accepted=exact("Accepted ping infoResponse",[rf"Accepted ping infoResponse generation={gb} time=[1-9][0-9]*ms address={ea}"])
packets=exact("CL packet ",[rf"CL packet {ea}: infoResponse.*"]*3)
marker=exact("DIRECTED_PING_A_EXPIRED",[r"DIRECTED_PING_A_EXPIRED"],"INFO","system")
if not alloc[0][0]<pr[0][0]<packets[0][0]<expired[0][0]<marker[0][0]<alloc[1][0]<pr[1][0]<packets[1][0]<expired[1][0]<packets[2][0]<accepted[0][0]:raise SystemExit("FAIL product causal order")
for prefix in ("Ignored unsolicited infoResponse","Ignored non-LAN local discovery infoResponse","Rejected invalid infoResponse","Rejected overlong infoResponse","Rejected full infoResponse"):
 if any(x.startswith(prefix) for x in m):raise SystemExit(f"FAIL forbidden semantic {prefix}")
if any(x.startswith("Accepted local discovery infoResponse") for x in m):raise SystemExit("FAIL local discovery acceptance")
if any(x in "\n".join(m) for x in ("WiredUI: server roster row=","WiredUI: server selection ","QUIC client:","SV_OnPlayerConnect:","FIRST GAMEPLAY FRAME","queued validated")):raise SystemExit("FAIL forbidden admission/connect")
stopped=[r for r in f if r["event"]=="stopped"]
if len(stopped)!=1 or stopped[0].get("reason")!="signal" or any(r["event"]=="unexpected_request" for r in f):raise SystemExit("FAIL fixture lifecycle")
print("PASS directed ping: expired A -> fresh B rejects stale A -> accepts current B")
PYEOF
}

write_self_fixture() {
python3 - "$1" "$2" "$3" <<'PYEOF'
import hashlib,json,sys
p,f,mode=sys.argv[1:];port=30001;client=40000;a=f"127.0.0.1:{port}";c=f"127.0.0.1:{client}";ca="1"*32;cb="2"*32;O=b"\xff"*4
def response(ch):
 info=f"\\challenge\\{ch}\\protocol\\74\\hostname\\EXPIRED A\\mapname\\arena7\\clients\\1\\sv_maxclients\\8\\game\\q3now\\gametype\\0\\minping\\0\\maxping\\0"
 return O+b"infoResponse\n"+info.encode()
def wire(event,scenario,ordinal,data,elapsed,source,peer):return {"event":event,"scenario":scenario,"request_ordinal":ordinal,"ordinal":ordinal,"challenge":ca if ordinal==1 else cb,"source":source,"source_port":int(source.rsplit(":",1)[1]),"peer":peer,"peer_port":int(peer.rsplit(":",1)[1]),"elapsed_ms":elapsed,"length":len(data),"sha256":hashlib.sha256(data).hexdigest(),"hex":data.hex()}
F=[{"event":"ready","responder_port":port},wire("request","",1,O+b"getinfo "+ca.encode(),10,c,a),wire("packet_sent","expired_a",1,response(ca),170,a,c),wire("request","",2,O+b"getinfo "+cb.encode(),230,c,a),wire("packet_sent","stale_a_under_b",2,response(ca),240,a,c),wire("packet_sent","current_b",2,response(cb),270,a,c),{"event":"stopped","reason":"signal"}]
msgs=["Ping queue allocation slot=0 reason=free previous_generation=0 previous_state=empty age=0ms address=none",f"Ping request generation=7 challenge={ca} timeout=100ms address={a}",f"CL packet {a}: infoResponse A",f"Ignored expired ping infoResponse generation=7 from {a}","DIRECTED_PING_A_EXPIRED","Ping queue allocation slot=1 reason=free previous_generation=0 previous_state=empty age=0ms address=none",f"Ping request generation=8 challenge={cb} timeout=100ms address={a}",f"CL packet {a}: infoResponse STALE",f"Ignored expired ping infoResponse generation=7 from {a}",f"CL packet {a}: infoResponse CURRENT",f"Accepted ping infoResponse generation=8 time=40ms address={a}"]
P=[{"sev":"INFO" if x=="DIRECTED_PING_A_EXPIRED" else "DEBUG","cat":"system" if x=="DIRECTED_PING_A_EXPIRED" else "client","msg":x} for x in msgs]
def drop(s):
 global P;P=[r for r in P if s not in r["msg"]]
if mode=="same_challenge":F[3]["challenge"]=ca;F[3].update({"hex":(O+b"getinfo "+ca.encode()).hex(),"length":44,"sha256":hashlib.sha256(O+b"getinfo "+ca.encode()).hexdigest()})
elif mode=="early_a":F[2]["elapsed_ms"]=90
elif mode=="missing_expired":drop("Ignored expired")
elif mode=="missing_second_expired":
 seen=0;kept=[]
 for r in P:
  if r["msg"].startswith("Ignored expired ping infoResponse"):
   seen+=1
   if seen==2:continue
  kept.append(r)
 P=kept
elif mode=="unexpected_mismatch":P.insert(9,{"sev":"DEBUG","cat":"client","msg":f"Ignored infoResponse challenge mismatch generation=8 from {a}"})
elif mode=="missing_accept":drop("Accepted ping")
elif mode=="stale_accepted":P[8]["msg"]=f"Accepted ping infoResponse generation=8 time=10ms address={a}"
elif mode=="wrong_generation":P[6]["msg"]=P[6]["msg"].replace("generation=8","generation=9")
elif mode=="extra_request":P.insert(7,P[6].copy())
elif mode=="missing_allocation":drop("slot=1 reason=free")
elif mode=="wrong_allocation_slot":P[5]["msg"]=P[5]["msg"].replace("slot=1","slot=0")
elif mode=="allocation_order":P[5],P[6]=P[6],P[5]
elif mode=="packet_hash":F[4]["sha256"]="0"*64
elif mode=="packet_order":F[4],F[5]=F[5],F[4]
elif mode=="wrong_source":F[4]["source"]="127.0.0.1:39999"
elif mode=="forbidden_row":P.append({"sev":"DEBUG","cat":"ui","msg":"WiredUI: server roster row=0 raw=0 address=127.0.0.1:30001 name=STALE A map=arena7"})
elif mode=="challenge_order":P[8],P[10]=P[10],P[8]
elif mode=="missing_marker":drop("DIRECTED_PING_A_EXPIRED")
elif mode=="marker_category":P[4]["cat"]="client"
elif mode=="wrong_category":P[3]["cat"]="ui"
elif mode=="cl_source":P[2]["msg"]=P[2]["msg"].replace("127.0.0.1:30001","127.0.0.1:39999")
elif mode=="semantic_suffix":P[3]["msg"]+=" trailing"
elif mode=="extra_lf":P[3]["msg"]+="\n\n"
elif mode=="embedded_cr":P[3]["msg"]=P[3]["msg"].replace(" ping ","\rping ")
elif mode=="terminal_lf":
 for r in P:r["msg"]+="\n"
elif mode=="ui_warn":P.append({"sev":"WARN","cat":"ui","msg":"unexpected"})
elif mode=="local_accept":P.append({"sev":"DEBUG","cat":"client","msg":f"Accepted local discovery infoResponse generation=8 rtt=40ms address={a}"})
elif mode=="timeout_stop":F[-1]["reason"]="timeout"
for path,rows in ((p,P),(f,F)):
 with open(path,"w") as o:
  for r in rows:o.write(json.dumps(r)+"\n")
PYEOF
}

if [ "${1:-}" = --self-test ]; then
 ROOT="$(mktemp -d -t directed-ping-self-XXXXXX 2>/dev/null || mktemp -d)"; trap 'rm -rf "$ROOT"' EXIT
 write_self_fixture "$ROOT/p" "$ROOT/f" clean; analyze "$ROOT/p" "$ROOT/f" >/dev/null || exit 1
 for d in same_challenge early_a missing_expired missing_second_expired unexpected_mismatch missing_accept stale_accepted wrong_generation extra_request missing_allocation wrong_allocation_slot allocation_order packet_hash packet_order wrong_source forbidden_row challenge_order missing_marker marker_category wrong_category cl_source semantic_suffix extra_lf embedded_cr terminal_lf ui_warn local_accept timeout_stop; do
  write_self_fixture "$ROOT/p-$d" "$ROOT/f-$d" "$d"
  if [ "$d" = terminal_lf ]; then
   analyze "$ROOT/p-$d" "$ROOT/f-$d" >/dev/null || { echo "FAIL self $d"; exit 1; }
  elif analyze "$ROOT/p-$d" "$ROOT/f-$d" >/dev/null 2>&1; then echo "FAIL self $d"; exit 1
  fi
 done
 echo '==> directed-ping analyzer self-test: PASS'; exit 0
fi

command -v python3 >/dev/null 2>&1 && [ -f "$FIXTURE" ] && [ -f "$TIMEOUT_RUNNER" ] || { echo 'SKIP: Python unavailable'; exit 77; }
WIRED="${1:-}"; [ -n "$WIRED" ] && [ -x "$WIRED" ] || { echo 'SKIP: pass assembled Wired GUI binary'; exit 77; }
WIRED="$(cd "$(dirname "$WIRED")" && pwd)/$(basename "$WIRED")"; WD="$(dirname "$WIRED")"
PACK=""; for c in "$WD" "$WD/../Resources" "$WD/q3now-preview.arm64.app/Contents/Resources"; do [ -f "$c/base/pax21.sw3z" ] && PACK="$c" && break; done
[ -n "$PACK" ] || { echo 'SKIP: current pax21 unavailable'; exit 77; }
CONTENT="${WIRED_CONTENT_ROOT:-$PACK}"
if [ -f "$CONTENT/base/pax01.sw3z" ]; then BASE="$CONTENT/base/pax01.sw3z"; elif [ -f "$CONTENT/base/pak0.pk3" ]; then BASE="$CONTENT/base/pak0.pk3"; else echo 'SKIP: set WIRED_CONTENT_ROOT'; exit 77; fi
ROOT="$(mktemp -d -t directed-ping-XXXXXX 2>/dev/null || mktemp -d)"; EVENTS="$ROOT/fixture.jsonl"; HOME_DIR="$ROOT/q3now-preview"; PID=""
cleanup(){ [ -n "$PID" ] && kill -TERM "$PID" 2>/dev/null || true; [ -n "$PID" ] && wait "$PID" 2>/dev/null || true; [ "${WIRED_KEEP_ARTIFACTS:-0}" = 1 ] || rm -rf "$ROOT"; }; trap cleanup EXIT INT TERM
mkdir -p "$HOME_DIR/base"
cp "$PACK/base/pax21.sw3z" "$HOME_DIR/base/pax21.sw3z" || exit 1
cp "$BASE" "$HOME_DIR/base/" || exit 1
python3 "$FIXTURE" --events "$EVENTS" --timeout 30 & PID=$!
for _ in $(seq 1 100); do [ -s "$EVENTS" ] && break; sleep .05; done
PORT="$(python3 - "$EVENTS" <<'PYEOF'
import json,sys
print(json.loads(open(sys.argv[1]).readline())["responder_port"])
PYEOF
)"
cat >"$HOME_DIR/base/directed-ping.cfg" <<CFGEOF
set cl_maxPing 100
wait 100
ping 127.0.0.1:$PORT
waitms 230
echo DIRECTED_PING_A_EXPIRED
ping 127.0.0.1:$PORT
waitms 300
quit
CFGEOF
case "$(uname -s)" in Darwin) NATIVE="$HOME_DIR"; ARGS=(-ApplePersistenceIgnoreState YES);; *) NATIVE="$HOME_DIR"; ARGS=();; esac
python3 "$TIMEOUT_RUNNER" --timeout 30 --kill-after 10 --cwd "$WD" --stdout "$ROOT/stdout" -- "$WIRED" "${ARGS[@]}" +set fs_homepath "$NATIVE" +set com_automated 1 +set com_noHardReboot 1 +set net_enabled 1 +set net_ip 127.0.0.1 +set net_port 0 +set s_initsound 0 +set r_fullscreen 0 +set log_severity DEBUG +set log_file_severity DEBUG +set log_file_mode overwrite_synced +exec directed-ping.cfg
rc=$?; [ "$rc" -eq 0 ] && [ -s "$HOME_DIR/qconsole.jsonl" ] || { echo "FAIL product rc=$rc"; exit 1; }
kill -TERM "$PID" 2>/dev/null || true; wait "$PID" || exit 1; PID=""
analyze "$HOME_DIR/qconsole.jsonl" "$EVENTS"
