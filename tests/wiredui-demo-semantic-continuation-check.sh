#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Exact snapshot semantic failure through attract-owned and scripted continuations.

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
TIMEOUT_RUNNER="$SCRIPT_DIR/run-with-timeout.py"
DEMO_HEX="0100000005000000aabfaa9200ffffffffffffffff"
DEMO_SHA="3c4c57e19d889d35b3e913e40bd2c7afd0382b13eb6946aee28271d902208faf"

analyze_mode() {
    python3 - "$1" "$2" "$3" "$DEMO_HEX" "$DEMO_SHA" <<'PYEOF'
import hashlib,json,re,sys
mode,log_path,demo_path,wanted_hex,wanted_sha=sys.argv[1:]
rows=[]
for number,line in enumerate(open(log_path,encoding="utf-8",errors="replace"),1):
    if not line.strip(): continue
    try: row=json.loads(line)
    except ValueError as exc: raise SystemExit(f"FAIL semantic-{mode} {number}: invalid JSON: {exc}")
    if not isinstance(row,dict): raise SystemExit(f"FAIL semantic-{mode}: non-object row")
    rows.append(row)
if not rows: raise SystemExit(f"FAIL semantic-{mode}: empty evidence")
messages=[str(row.get("msg","")) for row in rows]
def norm(message): return message[:-1] if message.endswith("\n") else message
def family(prefix):
    return [(i,row,norm(message)) for i,(row,message) in enumerate(zip(rows,messages)) if norm(message).startswith(prefix)]
def exact(prefix,wanted,sev,cat,label):
    found=family(prefix)
    if len(found)!=1: raise SystemExit(f"FAIL semantic-{mode}: {label} cardinality {len(found)}")
    i,row,message=found[0]
    if message!=wanted: raise SystemExit(f"FAIL semantic-{mode}: {label} message")
    if str(row.get("sev","")).upper()!=sev or str(row.get("cat","")).lower()!=cat:
        raise SystemExit(f"FAIL semantic-{mode}: {label} metadata")
    return i

claimed=(
 "Demo file: demos/","Demo semantic parse recovered ","Demo playback rejected ",
 "Demo playback recovery ","CL_NextDemo:","Error: The demo ",
 "WiredUI: push menu ","WiredUI: close all postcondition ",
 "WiredUI: queued validated demo playback ","WiredUI: demo feeder selection ",
 "wui_menu_nav:","attract_status:","  state         :","  currentIndex  :",
 "  ownsDemo      :","  pushedPanel   :","Q0_SEMANTIC_",
 "FIRST GAMEPLAY FRAME","QUIC client: TLV ACCEPT","SV_OnPlayerConnect:",
)
for row,message in zip(rows,messages):
    clean=norm(message); parts=re.split(r"[\r\n]",clean)
    semantic=[part for part in parts if any(part.startswith(prefix) for prefix in claimed)]
    if semantic and (len(parts)!=1 or clean!=semantic[0]):
        raise SystemExit(f"FAIL semantic-{mode}: logical-line smuggling")

file_index=exact("Demo file: demos/","Demo file: demos/semantic.current.dm_74","INFO","client","file")
marker=exact("Demo semantic parse recovered ",
 "Demo semantic parse recovered command=7 detail=snapshot-areamask areabytes=255 generic_teardown=0",
 "DEBUG","client","typed local marker")
reject=exact("Demo playback rejected ",
 "Demo playback rejected reason=semantic-payload continuation=1","INFO","client","rejection")
error=exact("Error: The demo ","Error: The demo contains an invalid server message.","ERROR","system","error")
if not file_index < marker < reject < error:
    raise SystemExit(f"FAIL semantic-{mode}: core causal order")
if sum(str(row.get("sev","")).upper()=="ERROR" for row in rows)!=1:
    raise SystemExit(f"FAIL semantic-{mode}: global ERROR cardinality")
if any(str(row.get("sev","")).upper()=="FATAL" for row in rows):
    raise SystemExit(f"FAIL semantic-{mode}: FATAL")
if any(str(row.get("sev","")).upper()=="WARN" and str(row.get("cat","")).lower()=="ui" for row in rows):
    raise SystemExit(f"FAIL semantic-{mode}: UI WARN")
for prefix in ("FIRST GAMEPLAY FRAME","QUIC client: TLV ACCEPT","SV_OnPlayerConnect:",
               "CL_ParseSnapshot: Invalid size","snapshot committed"):
    if family(prefix): raise SystemExit(f"FAIL semantic-{mode}: forbidden {prefix}")

errors=[norm(message) for message in messages if norm(message).startswith('"com_errorMessage" is:')]
if errors!=['"com_errorMessage" is:""']:
    raise SystemExit(f"FAIL semantic-{mode}: error state not cleared")

if mode=="nextdemo":
    selection=exact("WiredUI: demo feeder selection ",
      "WiredUI: demo feeder selection row=0 name=semantic.current generation=2","DEBUG","ui","selection")
    queue=exact("WiredUI: queued validated demo playback ",
      "WiredUI: queued validated demo playback name=semantic.current","DEBUG","ui","queue")
    close=exact("WiredUI: close all postcondition ",
      "WiredUI: close all postcondition depth=0 active=none catcher_ui=0 paused=0","DEBUG","ui","CloseAll")
    next_index=exact("CL_NextDemo:","CL_NextDemo: echo Q0_SEMANTIC_NEXTDEMO_ADVANCED","DEBUG","client","nextdemo dispatch")
    advanced=exact("Q0_SEMANTIC_NEXTDEMO_ADVANCED","Q0_SEMANTIC_NEXTDEMO_ADVANCED","INFO","system","nextdemo continuation")
    complete=exact("Q0_SEMANTIC_NEXTDEMO_COMPLETE","Q0_SEMANTIC_NEXTDEMO_COMPLETE","INFO","system","completion")
    nav=family("wui_menu_nav:")
    if [x[2] for x in nav] != ["wui_menu_nav: K_DOWNARROW dispatched"]*3+["wui_menu_nav: K_ENTER dispatched"]*2:
        raise SystemExit("FAIL semantic-nextdemo: nav inventory")
    if not selection < queue < close < nav[-1][0] < file_index < next_index < advanced < complete:
        raise SystemExit("FAIL semantic-nextdemo: continuation causal order")
    if family("Demo playback recovery ") or family("WiredUI: push menu 'error_popup'"):
        raise SystemExit("FAIL semantic-nextdemo: popup recovery substituted")
    pushes=family("WiredUI: push menu ")
    if [x[2] for x in pushes] != ["WiredUI: push menu 'main' (depth 1)","WiredUI: push menu 'demos' (depth 2)"]:
        raise SystemExit("FAIL semantic-nextdemo: push inventory")
    if any(str(x[1].get("sev","")).upper()!="DEBUG" or str(x[1].get("cat","")).lower()!="ui" for x in pushes):
        raise SystemExit("FAIL semantic-nextdemo: push metadata")
elif mode=="attract":
    status=exact("attract_status:","attract_status:","INFO","ui","status")
    state=exact("  state         :","  state         : PLAYING","INFO","ui","state")
    index=exact("  currentIndex  :","  currentIndex  : 1","INFO","ui","index")
    owner=exact("  ownsDemo      :","  ownsDemo      : 0 (demoplaying=0)","INFO","ui","ownership")
    panel=exact("  pushedPanel   :","  pushedPanel   : attract_brand","INFO","ui","successor panel")
    complete=exact("Q0_SEMANTIC_ATTRACT_COMPLETE","Q0_SEMANTIC_ATTRACT_COMPLETE","INFO","system","completion")
    if not error < status < state < index < owner < panel < complete:
        raise SystemExit("FAIL semantic-attract: scheduler causal order")
    if family("CL_NextDemo:") or family("Demo playback recovery ") or family("WiredUI: push menu 'error_popup'"):
        raise SystemExit("FAIL semantic-attract: wrong continuation owner")
else:
    raise SystemExit("FAIL: unknown analyzer mode")

data=open(demo_path,"rb").read()
if len(data)!=21 or data.hex()!=wanted_hex or hashlib.sha256(data).hexdigest()!=wanted_sha:
    raise SystemExit(f"FAIL semantic-{mode}: fixture identity")
print(f"PASS semantic {mode} continuation fixture_sha256={wanted_sha}")
PYEOF
}

write_clean() {
    python3 - "$1" "$2" "$3" "$DEMO_HEX" <<'PYEOF'
import json,sys
mode,log_path,demo_path,demo_hex=sys.argv[1:]
rows=[]
def add(sev,cat,msg): rows.append({"sev":sev,"cat":cat,"msg":msg+"\n"})
if mode=="nextdemo":
    add("DEBUG","ui","WiredUI: push menu 'main' (depth 1)")
    for _ in range(3): add("DEBUG","ui","wui_menu_nav: K_DOWNARROW dispatched")
    add("DEBUG","ui","WiredUI: push menu 'demos' (depth 2)")
    add("DEBUG","ui","wui_menu_nav: K_ENTER dispatched")
    add("DEBUG","ui","WiredUI: demo feeder selection row=0 name=semantic.current generation=2")
    add("DEBUG","ui","WiredUI: queued validated demo playback name=semantic.current")
    add("DEBUG","ui","WiredUI: close all postcondition depth=0 active=none catcher_ui=0 paused=0")
    add("DEBUG","ui","wui_menu_nav: K_ENTER dispatched")
add("INFO","client","Demo file: demos/semantic.current.dm_74")
add("DEBUG","client","Demo semantic parse recovered command=7 detail=snapshot-areamask areabytes=255 generic_teardown=0")
add("INFO","client","Demo playback rejected reason=semantic-payload continuation=1")
add("ERROR","system","Error: The demo contains an invalid server message.")
if mode=="nextdemo":
    add("DEBUG","client","CL_NextDemo: echo Q0_SEMANTIC_NEXTDEMO_ADVANCED")
    add("INFO","system","Q0_SEMANTIC_NEXTDEMO_ADVANCED")
    add("INFO","system",'"com_errorMessage" is:""')
    add("INFO","system","Q0_SEMANTIC_NEXTDEMO_COMPLETE")
else:
    add("INFO","ui","attract_status:")
    add("INFO","ui","  state         : PLAYING")
    add("INFO","ui","  currentIndex  : 1")
    add("INFO","ui","  ownsDemo      : 0 (demoplaying=0)")
    add("INFO","ui","  pushedPanel   : attract_brand")
    add("INFO","system",'"com_errorMessage" is:""')
    add("INFO","system","Q0_SEMANTIC_ATTRACT_COMPLETE")
with open(log_path,"w") as f:
    for row in rows: f.write(json.dumps(row)+"\n")
open(demo_path,"wb").write(bytes.fromhex(demo_hex))
PYEOF
}

if [ "${1:-}" = "--analyze" ]; then
    [ "$#" -eq 4 ] || { echo "usage: $0 --analyze attract|nextdemo qconsole.jsonl demo.dm_74"; exit 64; }
    analyze_mode "$2" "$3" "$4"
    exit $?
fi

if [ "${1:-}" = "--self-test" ]; then
    ROOT="$(mktemp -d -t wired-demo-semantic-self-XXXXXX 2>/dev/null || mktemp -d)"
    trap 'rm -rf "$ROOT"' EXIT
    defects=(missing-marker duplicate-marker marker-category wrong-command wrong-detail wrong-areabytes generic-teardown marker-cr-smuggle wrong-reason continuation-zero missing-error wrong-error error-category second-error ui-warn first-frame accept admission bad-fixture)
    total=0
    for mode in attract nextdemo; do
        write_clean "$mode" "$ROOT/$mode-clean.jsonl" "$ROOT/$mode.dm_74"
        analyze_mode "$mode" "$ROOT/$mode-clean.jsonl" "$ROOT/$mode.dm_74" >/dev/null || exit 1
        mode_defects=("${defects[@]}")
        if [ "$mode" = attract ]; then
            mode_defects+=(missing-status wrong-state wrong-index retained-owner wrong-panel nextdemo-substitution popup-substitution recovery-substitution retained-error)
        else
            mode_defects+=(missing-next wrong-next missing-advanced missing-complete popup-substitution recovery-substitution retained-error wrong-selection wrong-queue extra-enter extra-push)
        fi
        for defect in "${mode_defects[@]}"; do
            cp "$ROOT/$mode-clean.jsonl" "$ROOT/$mode-$defect.jsonl"
            python3 - "$ROOT/$mode-$defect.jsonl" "$defect" <<'PYEOF'
import json,sys
path,mode=sys.argv[1:]; rows=[json.loads(x) for x in open(path) if x.strip()]
def find(text): return next(i for i,r in enumerate(rows) if text in r["msg"])
if mode=="missing-marker": rows.pop(find("semantic parse recovered"))
elif mode=="duplicate-marker":
    i=find("semantic parse recovered"); rows.insert(i,dict(rows[i]))
elif mode=="marker-category": rows[find("semantic parse recovered")]["cat"]="system"
elif mode=="wrong-command": rows[find("semantic parse recovered")]["msg"]=rows[find("semantic parse recovered")]["msg"].replace("command=7","command=8")
elif mode=="wrong-detail": rows[find("semantic parse recovered")]["msg"]=rows[find("semantic parse recovered")]["msg"].replace("snapshot-areamask","snapshot-other")
elif mode=="wrong-areabytes": rows[find("semantic parse recovered")]["msg"]=rows[find("semantic parse recovered")]["msg"].replace("areabytes=255","areabytes=33")
elif mode=="generic-teardown": rows[find("semantic parse recovered")]["msg"]=rows[find("semantic parse recovered")]["msg"].replace("generic_teardown=0","generic_teardown=1")
elif mode=="marker-cr-smuggle":
    i=find("semantic parse recovered"); rows.insert(i,{"sev":"DEBUG","cat":"client","msg":"benign\r"+rows[i]["msg"]})
elif mode=="wrong-reason": rows[find("playback rejected")]["msg"]=rows[find("playback rejected")]["msg"].replace("semantic-payload","oversize")
elif mode=="continuation-zero": rows[find("playback rejected")]["msg"]=rows[find("playback rejected")]["msg"].replace("continuation=1","continuation=0")
elif mode=="missing-error": rows.pop(find("Error: The demo"))
elif mode=="wrong-error": rows[find("Error: The demo")]["msg"]="Error: The demo could not be read.\n"
elif mode=="error-category": rows[find("Error: The demo")]["cat"]="client"
elif mode=="second-error": rows.append({"sev":"ERROR","cat":"system","msg":"extra\n"})
elif mode=="ui-warn": rows.append({"sev":"WARN","cat":"ui","msg":"unexpected ui warning\n"})
elif mode=="first-frame": rows.append({"sev":"INFO","cat":"client","msg":"FIRST GAMEPLAY FRAME mapname=arena7 numEntities=1\n"})
elif mode=="accept": rows.append({"sev":"DEBUG","cat":"network","msg":"QUIC client: TLV ACCEPT received\n"})
elif mode=="admission": rows.append({"sev":"DEBUG","cat":"server","msg":"SV_OnPlayerConnect: conn=1\n"})
elif mode=="missing-status": rows.pop(find("attract_status:"))
elif mode=="wrong-state": rows[find("state         :")]["msg"]="  state         : STOPPED\n"
elif mode=="wrong-index": rows[find("currentIndex")]["msg"]="  currentIndex  : 0\n"
elif mode=="retained-owner": rows[find("ownsDemo")]["msg"]="  ownsDemo      : 1 (demoplaying=1)\n"
elif mode=="wrong-panel": rows[find("pushedPanel")]["msg"]="  pushedPanel   : none\n"
elif mode=="nextdemo-substitution": rows.insert(find("attract_status"),{"sev":"DEBUG","cat":"client","msg":"CL_NextDemo: echo forged\n"})
elif mode=="popup-substitution": rows.append({"sev":"DEBUG","cat":"ui","msg":"WiredUI: push menu 'error_popup' (depth 1)\n"})
elif mode=="recovery-substitution": rows.append({"sev":"INFO","cat":"client","msg":"Demo playback recovery reason=semantic-payload depth=2 popup=0 automated=1\n"})
elif mode=="retained-error": rows[find('com_errorMessage\" is:\"\"')]["msg"]='"com_errorMessage" is:"still live"\n'
elif mode=="missing-next": rows.pop(find("CL_NextDemo:"))
elif mode=="wrong-next": rows[find("CL_NextDemo:")]["msg"]="CL_NextDemo: echo OTHER\n"
elif mode=="missing-advanced": rows.pop(find("Q0_SEMANTIC_NEXTDEMO_ADVANCED"))
elif mode=="missing-complete": rows.pop(find("Q0_SEMANTIC_NEXTDEMO_COMPLETE"))
elif mode=="wrong-selection": rows[find("demo feeder selection")]["msg"]="WiredUI: demo feeder selection row=0 name=other generation=2\n"
elif mode=="wrong-queue": rows[find("queued validated demo")]["msg"]="WiredUI: queued validated demo playback name=other\n"
elif mode=="extra-enter": rows.insert(find("Demo file:"),{"sev":"DEBUG","cat":"ui","msg":"wui_menu_nav: K_ENTER dispatched\n"})
elif mode=="extra-push": rows.insert(find("Demo file:"),{"sev":"DEBUG","cat":"ui","msg":"WiredUI: push menu 'other' (depth 3)\n"})
with open(path,"w") as f:
    for row in rows: f.write(json.dumps(row)+"\n")
PYEOF
            demo="$ROOT/$mode.dm_74"
            if [ "$defect" = bad-fixture ]; then printf '\001' >>"$demo"; fi
            if analyze_mode "$mode" "$ROOT/$mode-$defect.jsonl" "$demo" >/dev/null 2>&1; then
                echo "FAIL: semantic $mode analyzer accepted $defect"; exit 1
            fi
            if [ "$defect" = bad-fixture ]; then python3 - "$demo" "$DEMO_HEX" <<'PYEOF'
import sys
open(sys.argv[1],"wb").write(bytes.fromhex(sys.argv[2]))
PYEOF
            fi
            total=$((total+1))
        done
    done
    echo "==> SELF-TEST PASS: semantic continuations clean accepted; $total defects rejected"
    exit 0
fi

WIRED="${1:-}"
[ -n "$WIRED" ] && [ -x "$WIRED" ] || { echo "usage: $0 /absolute/path/to/wired"; exit 64; }
WIRED="$(cd "$(dirname "$WIRED")" && pwd)/$(basename "$WIRED")"
WIRED_DIR="$(dirname "$WIRED")"; PACK_ROOT=""
for candidate in "$WIRED_DIR" "$WIRED_DIR/../Resources" "$WIRED_DIR/../../.."; do
    [ -f "$candidate/base/pax21.sw3z" ] && { PACK_ROOT="$(cd "$candidate" && pwd)"; break; }
done
[ -n "$PACK_ROOT" ] || { echo "SKIP: current pax21 not found"; exit 77; }
CONTENT_ROOT=""
for candidate in "${WIRED_CONTENT_ROOT:-}" "$PACK_ROOT"; do
    [ -n "$candidate" ] || continue
    if [ -f "$candidate/base/pax01.sw3z" ] || [ -f "$candidate/base/pak0.pk3" ]; then CONTENT_ROOT="$(cd "$candidate" && pwd)"; break; fi
done
[ -n "$CONTENT_ROOT" ] || { echo "SKIP: set WIRED_CONTENT_ROOT"; exit 77; }
if [ -f "$CONTENT_ROOT/base/pax01.sw3z" ]; then BASE_ARCHIVE="$CONTENT_ROOT/base/pax01.sw3z"; else BASE_ARCHIVE="$CONTENT_ROOT/base/pak0.pk3"; fi

RUN_ROOT="$(mktemp -d -t wired-demo-semantic-XXXXXX 2>/dev/null || mktemp -d)"
ATTRACT_HOME="$RUN_ROOT/attract/q3now-preview"; NEXT_HOME="$RUN_ROOT/next/q3now-preview"
mkdir -p "$ATTRACT_HOME/base/demos" "$NEXT_HOME/base/demos" "$RUN_ROOT/attract-run" "$RUN_ROOT/next-run"
cleanup(){ if [ "${WIRED_KEEP_ARTIFACTS:-0}" = 1 ]; then echo "    kept artifacts: $RUN_ROOT"; else rm -rf "$RUN_ROOT"; fi; }
trap cleanup EXIT INT TERM
for home in "$ATTRACT_HOME" "$NEXT_HOME"; do
    cp "$BASE_ARCHIVE" "$home/base/" || exit 1
    cp "$PACK_ROOT/base/pax21.sw3z" "$home/base/pax21.sw3z" || exit 1
    python3 - "$home/base/demos/semantic.current.dm_74" "$DEMO_HEX" <<'PYEOF'
import sys
open(sys.argv[1],"wb").write(bytes.fromhex(sys.argv[2]))
PYEOF
done
cat >"$ATTRACT_HOME/base/q0-semantic-attract.cfg" <<'EOF'
wait 100
attract_stop
lua_eval "attract.clear(); attract.add('demo','semantic.current',0); attract.add('panel','attract_brand',6000); attract.set_loop(false); attract.set_transition(0)"
attract_restart
wait 100
wait 100
wait 100
attract_status
com_errorMessage
echo Q0_SEMANTIC_ATTRACT_COMPLETE
wait 10
quit
EOF
cat >"$NEXT_HOME/base/q0-semantic-nextdemo.cfg" <<'EOF'
set attract_enabled 0
wait 100
wui_push main
wait 20
wui_menu_nav down
wait 2
wui_menu_nav down
wait 2
wui_menu_nav down
wait 10
wui_menu_nav enter
wait 20
wui_menu_nav focus demolist
wait 10
wui_menu_nav focus btn_play_demo
set nextdemo "echo Q0_SEMANTIC_NEXTDEMO_ADVANCED"
set com_automated 0
wui_menu_nav enter
wait 60
com_errorMessage
echo Q0_SEMANTIC_NEXTDEMO_COMPLETE
wait 10
quit
EOF
case "$(uname -s)" in
Darwin) ATTRACT_NATIVE="$ATTRACT_HOME"; NEXT_NATIVE="$NEXT_HOME"; PLATFORM_ARGS=( -ApplePersistenceIgnoreState YES );;
MINGW*|MSYS*|CYGWIN*) ATTRACT_NATIVE="$(cygpath -w "$ATTRACT_HOME")"; NEXT_NATIVE="$(cygpath -w "$NEXT_HOME")"; PLATFORM_ARGS=();;
*) ATTRACT_NATIVE="$ATTRACT_HOME"; NEXT_NATIVE="$NEXT_HOME"; PLATFORM_ARGS=();;
esac

run_phase() {
    local mode="$1" home="$2" native="$3" cfg="$4" cwd="$5"
    echo "==> Semantic Demo continuation: $mode"
    python3 "$TIMEOUT_RUNNER" --timeout 180 --kill-after 15 --cwd "$cwd" --stdout "$RUN_ROOT/$mode.stdout" -- \
        "$WIRED" "${PLATFORM_ARGS[@]}" +set fs_homepath "$native" \
        +set com_automated 1 +set com_noHardReboot 1 +set s_initsound 0 \
        +set r_fullscreen 0 +set r_mode -1 +set r_customwidth 1280 +set r_customheight 720 \
        +set log_severity DEBUG +set log_file_severity DEBUG +set log_file_mode overwrite_synced \
        +exec "$cfg"
    [ "$?" -eq 0 ] || { echo "FAIL: semantic $mode process did not exit cleanly"; exit 1; }
    analyze_mode "$mode" "$home/qconsole.jsonl" "$home/base/demos/semantic.current.dm_74" || exit 1
}
run_phase attract "$ATTRACT_HOME" "$ATTRACT_NATIVE" q0-semantic-attract.cfg "$RUN_ROOT/attract-run"
run_phase nextdemo "$NEXT_HOME" "$NEXT_NATIVE" q0-semantic-nextdemo.cfg "$RUN_ROOT/next-run"
python3 - "$WIRED" "$PACK_ROOT/base/pax21.sw3z" "$BASE_ARCHIVE" "$0" <<'PYEOF'
import hashlib,sys
for label,path in zip(("binary","pax21","base","harness"),sys.argv[1:]):
    print(f"    {label}_sha256={hashlib.sha256(open(path,'rb').read()).hexdigest()}")
PYEOF
echo "==> WiredUI semantic continuation gate: PASS"
