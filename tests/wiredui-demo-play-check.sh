#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Two-process Q0 contract: record current-protocol demos, then select and play
# the target through the real WiredUI demo browser until natural EOF.

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
TIMEOUT_RUNNER="$SCRIPT_DIR/run-with-timeout.py"
PROTOCOL=74

analyze_contract() {
    python3 - "$1" "$2" "$3" "$4" "$5" <<'PYEOF'
import hashlib, json, re, struct, sys
producer_path, consumer_path, layout_path, decoy_path, target_path = sys.argv[1:]

def rows(path):
    out=[]
    with open(path, encoding="utf-8", errors="replace") as f:
        for n,line in enumerate(f,1):
            if not line.strip(): continue
            try: row=json.loads(line)
            except ValueError as e: raise SystemExit(f"FAIL {path}:{n}: invalid JSON: {e}")
            if not isinstance(row,dict): raise SystemExit(f"FAIL {path}:{n}: JSON record is not an object")
            out.append(row)
    if not out: raise SystemExit(f"FAIL: empty evidence {path}")
    return out

prod, cons, layout = rows(producer_path), rows(consumer_path), rows(layout_path)
pm=[str(r.get("msg","")) for r in prod]
cm=[str(r.get("msg","")) for r in cons]

for label,data in (("producer",prod),("consumer",cons)):
    bad=[r for r in data if str(r.get("sev","")).upper() in {"ERROR","FATAL"}
         or (str(r.get("sev","")).upper()=="WARN" and str(r.get("cat","")).lower()=="ui"
             and str(r.get("msg",""))!="WiredUI: no demo selected\n")]
    if bad: raise SystemExit(f"FAIL severity: {label} has {len(bad)} unexpected ERROR/FATAL/UI-WARN")
if sum(m=="WiredUI: no demo selected\n" for m in cm)!=1:
    raise SystemExit("FAIL: expected exactly one deliberate no-selection refusal")

def ordered(messages, steps):
    cursor=0
    for name,pat in steps:
        for i in range(cursor,len(messages)):
            if re.search(pat,messages[i]): cursor=i+1; break
        else: raise SystemExit(f"FAIL contract: missing/out-of-order {name}")

ordered(pm, [
 ("producer FIRST",r"FIRST GAMEPLAY FRAME mapname=(?:maps/)?arena7(?:\.bsp)?"),
 ("decoy record",r"recording to demos/a0_decoy\."),
 ("decoy stop",r"Stopped demo recording\."),
 ("target record",r"recording to demos/q0_target\."),
 ("target stop",r"Stopped demo recording\."),
 ("producer marker",r"Q0_DEMOS_RECORDED"),
])

ordered(cm, [
 ("ignored stale state",r"WiredUI: loaded 0 UI state entries"),
 ("demos menu",r"WiredUI: push menu 'demos' \(depth 1\)"),
 ("inventory",r"WiredUI: demos loaded protocol=74 count=2 generation=2\b"),
 ("decoy row",r"demo feeder row=0 name=a0_decoy generation=2\b"),
 ("target row",r"demo feeder row=1 name=q0_target generation=2\b"),
 ("preselection Play",r"focused item 'btn_play_demo'"),
 ("refusal",r"WiredUI: no demo selected"),
 ("refusal Enter",r"wui_menu_nav: K_ENTER dispatched"),
 ("row0 callback",r"demo feeder selection row=0 name=a0_decoy generation=2\b"),
 ("list focus",r"focused item 'demolist'"),
 ("row1 callback",r"demo feeder selection row=1 name=q0_target generation=2\b"),
 ("real Down",r"wui_menu_nav: K_DOWNARROW dispatched"),
 ("target Play",r"focused item 'btn_play_demo'"),
 ("validated queue",r"queued validated demo playback name=q0_target\b"),
 ("close all",r"close all postcondition depth=0 active=none catcher_ui=0 paused=0"),
 ("Play Enter",r"wui_menu_nav: K_ENTER dispatched"),
 ("exact file",r"Demo file: demos/q0_target\.dm_74\b"),
 ("playback FIRST",r"FIRST GAMEPLAY FRAME mapname=(?:maps/)?arena7(?:\.bsp)?.*numEntities=[1-9][0-9]*"),
])

traces=[]
rx=re.compile(r"demo playback trace state=8 demoplaying=1 name=q0_target\.dm_74 sequence=([0-9]+) serverTime=([0-9]+)")
for i,m in enumerate(cm):
    q=rx.search(m)
    if q: traces.append((i,int(q.group(1)),int(q.group(2))))
if len(traces)!=2: raise SystemExit(f"FAIL: expected two exact active playback traces, got {len(traces)}")
if not (traces[1][1]>traces[0][1] and traces[1][2]>traces[0][2]):
    raise SystemExit("FAIL: demo message sequence/serverTime did not advance")
next_i=next((i for i,m in enumerate(cm) if re.search(r"CL_NextDemo: exec q0-demo-done\.cfg",m)),None)
done_i=next((i for i,m in enumerate(cm) if "Q0_DEMO_COMPLETED" in m),None)
if next_i is None or done_i is None or not (traces[-1][0] < next_i < done_i):
    raise SystemExit("FAIL: natural EOF/nextdemo completion order missing")
if any("QUIC client: TLV ACCEPT" in m or "SV_OnPlayerConnect:" in m for m in cm):
    raise SystemExit("FAIL: playback unexpectedly used a live network server")
if any(x in "\n".join(cm) for x in ("Demo file was truncated", "Protocol 74 not supported", "couldn't open demos/")):
    raise SystemExit("FAIL: demo backend reported playback failure")

focus=[]
for row in layout:
    menu=str(row.get("menu",row.get("menuName","")))
    if menu!="demos": continue
    if row.get("kind")=="item" and bool(row.get("focused",0)):
        item=str(row.get("region",""))
    else:
        item=str(row.get("focusedItem",""))
        if not item and isinstance(row.get("focused"),str):
            item=row["focused"]
    if item: focus.append(item)
for wanted in ("btn_play_demo","demolist","btn_play_demo"):
    try: idx=focus.index(wanted); focus=focus[idx+1:]
    except ValueError: raise SystemExit(f"FAIL layout: missing ordered demos/{wanted}")

def inspect_demo(path):
    data=open(path,"rb").read()
    if len(data)<1024: raise SystemExit(f"FAIL: demo too small: {path} ({len(data)})")
    pos=0; messages=0
    while pos+8<=len(data):
        seq,size=struct.unpack_from("<ii",data,pos); pos+=8
        if seq==-1 and size==-1:
            if pos!=len(data): raise SystemExit(f"FAIL: trailing bytes after demo EOF: {path}")
            if messages<2: raise SystemExit(f"FAIL: too few demo messages: {path}")
            return hashlib.sha256(data).hexdigest()
        if size<0 or pos+size>len(data): raise SystemExit(f"FAIL: malformed demo envelope: {path}")
        pos+=size; messages+=1
    raise SystemExit(f"FAIL: missing terminal -1/-1: {path}")

decoy_hash=inspect_demo(decoy_path); target_hash=inspect_demo(target_path)
if decoy_hash==target_hash: raise SystemExit("FAIL: decoy and target demo hashes unexpectedly match")
print(f"PASS demo-play contract demo_sha256={target_hash}")
PYEOF
}

write_fixture() {
    python3 - "$1" "$2" "$3" "$4" "$5" "$6" <<'PYEOF'
import json,struct,sys
mode,p,c,l,d,t=sys.argv[1:]
def rec(msg,sev="DEBUG",cat="ui"): return {"ts":"2026-08-11T00:00:00Z","sev":sev,"cat":cat,"msg":msg}
prod=[rec("cls.state: -> CA_ACTIVE (FIRST GAMEPLAY FRAME mapname=maps/arena7.bsp serverTime=100 numEntities=4 framecount=1)","INFO","client"),rec("recording to demos/a0_decoy.\n","INFO","client"),rec("Stopped demo recording.\n","INFO","client"),rec("recording to demos/q0_target.\n","INFO","client"),rec("Stopped demo recording.\n","INFO","client"),rec("Q0_DEMOS_RECORDED","INFO","system")]
cons_msgs=["WiredUI: loaded 0 UI state entries (ignored transient server selection 0)\n","WiredUI: push menu 'demos' (depth 1)","WiredUI: demos loaded protocol=74 count=2 generation=2\n","WiredUI: demo feeder row=0 name=a0_decoy generation=2\n","WiredUI: demo feeder row=1 name=q0_target generation=2\n","wui_menu_nav focus: focused item 'btn_play_demo'","WiredUI: no demo selected\n","wui_menu_nav: K_ENTER dispatched","WiredUI: demo feeder selection row=0 name=a0_decoy generation=2\n","wui_menu_nav focus: focused item 'demolist'","WiredUI: demo feeder selection row=1 name=q0_target generation=2\n","wui_menu_nav: K_DOWNARROW dispatched","wui_menu_nav focus: focused item 'btn_play_demo'","WiredUI: queued validated demo playback name=q0_target\n","WiredUI: close all postcondition depth=0 active=none catcher_ui=0 paused=0","wui_menu_nav: K_ENTER dispatched","Demo file: demos/q0_target.dm_74\n","cls.state: -> CA_ACTIVE (FIRST GAMEPLAY FRAME mapname=maps/arena7.bsp serverTime=200 numEntities=4 framecount=2)","WiredUI: demo playback trace state=8 demoplaying=1 name=q0_target.dm_74 sequence=10 serverTime=220\n","WiredUI: demo playback trace state=8 demoplaying=1 name=q0_target.dm_74 sequence=14 serverTime=300\n","CL_NextDemo: exec q0-demo-done.cfg\n","Q0_DEMO_COMPLETED"]
cons=[rec(m,"WARN" if m=="WiredUI: no demo selected\n" else ("INFO" if "FIRST" in m or "Demo file:" in m else "DEBUG"),"client" if "FIRST" in m or "Demo file:" in m or "CL_NextDemo" in m else "ui") for m in cons_msgs]
layout=[{"menu":"demos","kind":"item","region":"btn_play_demo","focused":1},
        {"menu":"demos","kind":"item","region":"demolist","focused":1},
        {"menu":"demos","kind":"item","region":"btn_play_demo","focused":1}]
def remove(s):
    nonlocal_dummy=None
    for arr in (prod,cons): arr[:]=[r for r in arr if s not in r["msg"]]
def replace(a,b):
    for arr in (prod,cons):
        for r in arr: r["msg"]=r["msg"].replace(a,b)
if mode=="missing-record": remove("recording to demos/q0_target")
elif mode=="missing-stop": prod=[r for i,r in enumerate(prod) if not ("Stopped demo" in r["msg"] and i>2)]
elif mode=="wrong-count": replace("count=2","count=3")
elif mode=="wrong-row": replace("row=1 name=q0_target","row=1 name=a0_decoy")
elif mode=="missing-down": remove("K_DOWNARROW")
elif mode=="missing-queue": remove("queued validated")
elif mode=="bad-close": replace("catcher_ui=0","catcher_ui=1")
elif mode=="wrong-file": replace("Demo file: demos/q0_target","Demo file: demos/a0_decoy")
elif mode=="no-first": cons=[r for r in cons if "FIRST GAMEPLAY" not in r["msg"]]
elif mode=="zero-entities": replace("numEntities=4 framecount=2","numEntities=0 framecount=2")
elif mode=="one-trace": cons=[r for r in cons if "sequence=14" not in r["msg"]]
elif mode=="stale-trace": replace("sequence=14 serverTime=300","sequence=10 serverTime=220")
elif mode=="early-eof":
    x=next(r for r in cons if "CL_NextDemo" in r["msg"]); cons.remove(x); cons.insert(18,x)
elif mode=="network": cons.insert(-1,rec("QUIC client: TLV ACCEPT received","INFO","client"))
elif mode=="ui-warn": cons.append(rec("unexpected ui warn\n","WARN","ui"))
elif mode=="renderer-error": cons.append(rec("renderer failed\n","ERROR","renderer"))
elif mode=="missing-layout": layout=layout[:-1]
elif mode=="malformed-json": pass
for path,arr in ((p,prod),(c,cons),(l,layout)):
    with open(path,"w") as f:
        for r in arr: f.write(json.dumps(r)+"\n")
if mode=="malformed-json": open(c,"a").write("{broken\n")
def demo(path,seed,bad=False):
    payload=(bytes([seed])*700)
    data=struct.pack("<ii",1,len(payload))+payload+struct.pack("<ii",2,len(payload))+payload
    if not bad: data+=struct.pack("<ii",-1,-1)
    open(path,"wb").write(data)
demo(d,1,mode=="bad-demo")
demo(t,2,mode=="bad-demo")
PYEOF
}

if [ "${1:-}" = "--self-test" ]; then
    ROOT="$(mktemp -d -t wired-demoplay-self-XXXXXX 2>/dev/null || mktemp -d)"; trap 'rm -rf "$ROOT"' EXIT
    write_fixture clean "$ROOT/p.jsonl" "$ROOT/c.jsonl" "$ROOT/l.jsonl" "$ROOT/a.dm_74" "$ROOT/q.dm_74"
    analyze_contract "$ROOT/p.jsonl" "$ROOT/c.jsonl" "$ROOT/l.jsonl" "$ROOT/a.dm_74" "$ROOT/q.dm_74" >/dev/null || { echo "FAIL: clean fixture rejected"; exit 1; }
    rc=0
    for defect in missing-record missing-stop wrong-count wrong-row missing-down missing-queue bad-close wrong-file no-first zero-entities one-trace stale-trace early-eof network ui-warn renderer-error missing-layout malformed-json bad-demo; do
        write_fixture "$defect" "$ROOT/p-$defect.jsonl" "$ROOT/c-$defect.jsonl" "$ROOT/l-$defect.jsonl" "$ROOT/a-$defect.dm_74" "$ROOT/q-$defect.dm_74"
        if analyze_contract "$ROOT/p-$defect.jsonl" "$ROOT/c-$defect.jsonl" "$ROOT/l-$defect.jsonl" "$ROOT/a-$defect.dm_74" "$ROOT/q-$defect.dm_74" >/dev/null 2>&1; then echo "FAIL: accepted defect $defect"; rc=1; else echo "  PASS rejected $defect"; fi
    done
    [ "$rc" -eq 0 ] && echo "==> SELF-TEST PASS: clean accepted; nineteen defects rejected"
    exit "$rc"
fi

WIRED="${1:-}"
if [ -z "$WIRED" ]; then echo "SKIP: pass a fully assembled Wired GUI binary"; exit 77; fi
if [ ! -x "$WIRED" ] && [ -x "$WIRED.exe" ]; then WIRED="$WIRED.exe"; fi
if [ ! -x "$WIRED" ]; then echo "SKIP: binary not executable: $WIRED"; exit 77; fi
case "$WIRED" in /*) : ;; *) WIRED="$PWD/$WIRED" ;; esac
WIRED_DIR="$(cd "$(dirname "$WIRED")" && pwd)"; WIRED="$WIRED_DIR/$(basename "$WIRED")"
if [ ! -f "$TIMEOUT_RUNNER" ] || ! python3 -c 'import sys; raise SystemExit(0 if sys.version_info >= (3,8) else 1)' 2>/dev/null; then echo "SKIP: Python >=3.8 and timeout runner required"; exit 77; fi
HEADLESS="${WIRED_HEADLESS:-}"
if [ -z "$HEADLESS" ]; then
    gui_name="$(basename "$WIRED")"; suffix="${gui_name#wired}"
    for candidate in "$WIRED_DIR/wired-headless$suffix" "$WIRED_DIR/wired-headless.arm64" "$WIRED_DIR/wired-headless.aarch64" "$WIRED_DIR/wired-headless.x86_64" "$WIRED_DIR/wired-headless.x64.exe" "$WIRED_DIR/../../../wired-headless$suffix" "$WIRED_DIR/../../../wired-headless.arm64" "$WIRED_DIR/../../../wired-headless.x86_64"; do
        [ -x "$candidate" ] && { HEADLESS="$candidate"; break; }
    done
fi
[ -n "$HEADLESS" ] && [ -x "$HEADLESS" ] || { echo "SKIP: sibling wired-headless unavailable"; exit 77; }
HEADLESS="$(cd "$(dirname "$HEADLESS")" && pwd)/$(basename "$HEADLESS")"
PACK_ROOT=""
for candidate in "$WIRED_DIR" "$WIRED_DIR/../Resources" "$WIRED_DIR/../../.."; do [ -f "$candidate/base/pax21.sw3z" ] && { PACK_ROOT="$(cd "$candidate" && pwd)"; break; }; done
[ -n "$PACK_ROOT" ] || { echo "SKIP: current pax21 not found"; exit 77; }
CONTENT_ROOT=""
for candidate in "${WIRED_CONTENT_ROOT:-}" "$PACK_ROOT"; do [ -n "$candidate" ] || continue; if [ -f "$candidate/base/pax01.sw3z" ] || [ -f "$candidate/base/pak0.pk3" ]; then CONTENT_ROOT="$(cd "$candidate" && pwd)"; break; fi; done
[ -n "$CONTENT_ROOT" ] || { echo "SKIP: set WIRED_CONTENT_ROOT"; exit 77; }
if [ -f "$CONTENT_ROOT/base/pax01.sw3z" ]; then BASE_ARCHIVE="$CONTENT_ROOT/base/pax01.sw3z"; else BASE_ARCHIVE="$CONTENT_ROOT/base/pak0.pk3"; fi

RUN_ROOT="$(mktemp -d -t wired-demoplay-XXXXXX 2>/dev/null || mktemp -d)"; PRODUCER="$RUN_ROOT/producer/q3now-preview"; CONSUMER="$RUN_ROOT/consumer/q3now-preview"; SERVER="$RUN_ROOT/server/q3now-preview"
mkdir -p "$PRODUCER/base" "$CONSUMER/base" "$SERVER/base"
SERVER_PID=""; SERVER_FIFO="$RUN_ROOT/server.stdin"; SERVER_OPEN=0
stop_server(){
    if [ -n "$SERVER_PID" ] && kill -0 "$SERVER_PID" 2>/dev/null; then [ "$SERVER_OPEN" -eq 1 ] && printf '%s\n' quit >&9 2>/dev/null || true; for _ in $(seq 1 150); do kill -0 "$SERVER_PID" 2>/dev/null || break; sleep 0.1; done; fi
    if [ -n "$SERVER_PID" ]; then wait "$SERVER_PID" 2>/dev/null; SERVER_RC=$?; else SERVER_RC=0; fi
    [ "$SERVER_OPEN" -eq 1 ] && { exec 9>&-; SERVER_OPEN=0; }; SERVER_PID=""
}
cleanup(){ stop_server; if [ "${WIRED_KEEP_ARTIFACTS:-0}" = 1 ]; then echo "    kept artifacts: $RUN_ROOT"; else rm -rf "$RUN_ROOT"; fi; }; trap cleanup EXIT INT TERM
for home in "$PRODUCER" "$CONSUMER" "$SERVER"; do cp "$BASE_ARCHIVE" "$home/base/" || exit 1; cp "$PACK_ROOT/base/pax21.sw3z" "$home/base/pax21.sw3z" || exit 1; done
PORT="$(python3 - <<'PYEOF'
import socket
s=socket.socket(socket.AF_INET,socket.SOCK_DGRAM); s.bind(("127.0.0.1",0)); print(s.getsockname()[1]); s.close()
PYEOF
)"
case "$PORT" in ''|*[!0-9]*) echo "FAIL: loopback port allocation failed"; exit 1;; esac

cat >"$PRODUCER/base/q0-demo-producer.cfg" <<CFGEOF
set attract_enabled 0
set activeAction "exec q0-demo-record-live.cfg"
connect 127.0.0.1:$PORT
CFGEOF
cat >"$PRODUCER/base/q0-demo-record-live.cfg" <<'CFGEOF'
wait 60
+forward
record a0_decoy
wait 180
stoprecord
-forward
+moveright
record q0_target
wait 700
stoprecord
-moveright
echo Q0_DEMOS_RECORDED
wait 20
quit
CFGEOF
cat >"$CONSUMER/base/q0-demo-consumer.cfg" <<'CFGEOF'
set q0_injected 0
set attract_enabled 0
set nextdemo "exec q0-demo-done.cfg"
set activeAction "exec q0-demo-playback-live.cfg"
wait 100
wui_push demos
wait 20
wui_menu_nav focus btn_play_demo
set r_layoutDump 1
wait 2
set r_layoutDump 0
wui_menu_nav enter
print q0_injected
wait 10
wui_menu_nav focus demolist
set r_layoutDump 1
wait 2
set r_layoutDump 0
wui_menu_nav down
wait 10
wui_menu_nav focus btn_play_demo
set r_layoutDump 1
wait 2
set r_layoutDump 0
wui_menu_nav enter
CFGEOF
cat >"$CONSUMER/base/q0-demo-playback-live.cfg" <<'CFGEOF'
wait 30
wui_demo_trace
wait 90
wui_demo_trace
CFGEOF
cat >"$CONSUMER/base/q0-demo-done.cfg" <<'CFGEOF'
echo Q0_DEMO_COMPLETED
wait 10
quit
CFGEOF
python3 - "$CONSUMER/base/wired_ui_state.dat" <<'PYEOF'
import struct,sys
k=b"ui_selectedDemo"; v=b"stale;set q0_injected 1"
open(sys.argv[1],"wb").write(struct.pack("@iii",0x57554953,1,1)+struct.pack("@HH",len(k),len(v))+k+v)
PYEOF

case "$(uname -s)" in Darwin) P_HOME="$PRODUCER"; C_HOME="$CONSUMER"; S_HOME="$SERVER"; PLATFORM_ARGS=( -ApplePersistenceIgnoreState YES );; MINGW*|MSYS*|CYGWIN*) P_HOME="$(cygpath -w "$PRODUCER")"; C_HOME="$(cygpath -w "$CONSUMER")"; S_HOME="$(cygpath -w "$SERVER")"; PLATFORM_ARGS=();; *) P_HOME="$PRODUCER"; C_HOME="$CONSUMER"; S_HOME="$SERVER"; PLATFORM_ARGS=();; esac
COMMON=( +set com_automated 1 +set com_noHardReboot 1 +set s_initsound 0 +set r_fullscreen 0 +set r_mode -1 +set r_customwidth 1280 +set r_customheight 720 +set log_severity DEBUG +set log_file_severity DEBUG +set log_file_mode overwrite_synced )

echo "==> Demo producer server: 127.0.0.1:$PORT arena7"
mkfifo "$SERVER_FIFO"; exec 9<>"$SERVER_FIFO"; SERVER_OPEN=1
( cd "$(dirname "$HEADLESS")" && exec "$HEADLESS" +set fs_homepath "$S_HOME" +set com_automated 1 +set com_noHardReboot 1 +set log_severity DEBUG +set log_file_severity DEBUG +set log_file_mode overwrite_synced +set net_ip 127.0.0.1 +set net_port "$PORT" +set sv_public 0 +set sv_pure 0 +set g_autoBots 0 +map arena7 <&9 ) >"$RUN_ROOT/server.stdout" 2>&1 & SERVER_PID=$!
python3 - "$SERVER/qconsole.jsonl" "$SERVER_PID" "$PORT" <<'PYEOF' || exit 1
import json,os,re,sys,time
p,pid,port=sys.argv[1],int(sys.argv[2]),sys.argv[3]; end=time.monotonic()+45
while time.monotonic()<end:
 try: os.kill(pid,0)
 except ProcessLookupError: raise SystemExit("FAIL: demo server exited before ready")
 try:
  msgs=[json.loads(x).get("msg","") for x in open(p,errors="replace") if x.strip()]
 except (OSError,ValueError): msgs=[]
 if any(re.search(rf"WiredNet: listening on port {port}",m) for m in msgs) and any(re.search(r"Server: arena7\b",m) for m in msgs): raise SystemExit(0)
 time.sleep(.1)
raise SystemExit("FAIL: demo server readiness timeout")
PYEOF
echo "==> Demo producer client: remote current-protocol recording"
python3 "$TIMEOUT_RUNNER" --timeout 240 --kill-after 15 --cwd "$RUN_ROOT/producer" --stdout "$RUN_ROOT/producer.stdout" -- "$WIRED" "${PLATFORM_ARGS[@]}" +set fs_homepath "$P_HOME" "${COMMON[@]}" +set net_ip 127.0.0.1 +set wn_cert_verify 0 +exec q0-demo-producer.cfg
[ "$?" -eq 0 ] || { echo "FAIL: producer did not exit cleanly"; exit 1; }
stop_server; [ "${SERVER_RC:-1}" -eq 0 ] || { echo "FAIL: demo server did not exit cleanly rc=${SERVER_RC:-unset}"; exit 1; }
DECOY="$PRODUCER/base/demos/a0_decoy.dm_$PROTOCOL"; TARGET="$PRODUCER/base/demos/q0_target.dm_$PROTOCOL"
[ -s "$DECOY" ] && [ -s "$TARGET" ] || { echo "FAIL: finalized demos missing"; exit 1; }
if find "$PRODUCER/base/demos" -name '*.tmp' -print -quit | grep -q .; then echo "FAIL: temporary demo residue"; exit 1; fi
mkdir -p "$CONSUMER/base/demos"; cp "$DECOY" "$CONSUMER/base/demos/a0_decoy.dm_$PROTOCOL"; cp "$TARGET" "$CONSUMER/base/demos/q0_target.dm_$PROTOCOL"

echo "==> Demo consumer: real feeder selection and natural EOF"
python3 "$TIMEOUT_RUNNER" --timeout 240 --kill-after 15 --cwd "$RUN_ROOT/consumer" --stdout "$RUN_ROOT/consumer.stdout" -- "$WIRED" "${PLATFORM_ARGS[@]}" +set fs_homepath "$C_HOME" "${COMMON[@]}" +set r_layoutDump 0 +exec q0-demo-consumer.cfg
[ "$?" -eq 0 ] || { echo "FAIL: consumer did not exit cleanly"; exit 1; }
PLOG="$PRODUCER/qconsole.jsonl"; CLOG="$CONSUMER/qconsole.jsonl"; LAYOUT="$RUN_ROOT/consumer/layoutdump.jsonl"
for f in "$PLOG" "$CLOG" "$LAYOUT"; do [ -s "$f" ] || { echo "FAIL: missing evidence $f"; exit 1; }; done
cmp -s "$TARGET" "$CONSUMER/base/demos/q0_target.dm_$PROTOCOL" || { echo "FAIL: target changed between phases"; exit 1; }
analyze_contract "$PLOG" "$CLOG" "$LAYOUT" "$CONSUMER/base/demos/a0_decoy.dm_$PROTOCOL" "$CONSUMER/base/demos/q0_target.dm_$PROTOCOL" || exit 1
python3 - "$WIRED" "$PACK_ROOT/base/pax21.sw3z" "$BASE_ARCHIVE" "$TARGET" "$0" <<'PYEOF'
import hashlib,sys
for label,path in zip(("binary","pax21","base","demo","harness"),sys.argv[1:]):
    print(f"    {label}_sha256={hashlib.sha256(open(path,'rb').read()).hexdigest()}")
PYEOF
echo "==> WiredUI Demo Play gate: PASS"
