#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Automated next-playback file-adapter fault through the authored Demos route.

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
TIMEOUT_RUNNER="$SCRIPT_DIR/run-with-timeout.py"
CFG_SOURCE="$SCRIPT_DIR/fixtures/wiredui-demo-io-fault.cfg"
DEMO_HEX="0100000005000000aabfaa9200ffffffffffffffff"
DEMO_SHA="3c4c57e19d889d35b3e913e40bd2c7afd0382b13eb6946aee28271d902208faf"

analyze_contract() {
    python3 - "$1" "$2" "$DEMO_HEX" "$DEMO_SHA" <<'PYEOF'
import hashlib,json,re,sys
log_path,demo_path,wanted_hex,wanted_sha=sys.argv[1:]
rows=[]
for number,line in enumerate(open(log_path,encoding="utf-8",errors="replace"),1):
    if not line.strip(): continue
    try: row=json.loads(line)
    except ValueError as exc: raise SystemExit(f"FAIL io-fault {number}: invalid JSON: {exc}")
    if not isinstance(row,dict): raise SystemExit(f"FAIL io-fault {number}: row is not an object")
    rows.append(row)
if not rows: raise SystemExit("FAIL io-fault: empty product evidence")
messages=[str(row.get("msg","")) for row in rows]

def normalized(message):
    return message[:-1] if message.endswith("\n") else message

claimed=(
 "WiredUI: push menu ","WiredUI: pop menu ","WiredUI: demos loaded ",
 "WiredUI: demo feeder row=","WiredUI: demo feeder selection ",
 "WiredUI: queued validated demo playback ","WiredUI: close all postcondition ",
 "wui_menu_nav:","wui_menu_nav focus:","Demo file: demos/",
 "Demo read fault ","Demo playback rejected ","Demo playback recovery ",
 "Demo semantic parse recovered ","Error: The demo ","CL_ParseSnapshot: Invalid size",
 "FIRST GAMEPLAY FRAME","QUIC client: TLV ACCEPT","SV_OnPlayerConnect:",
 "snapshot committed","Q0_DEMO_IO_FAULT_COMPLETE",
)
for row,message in zip(rows,messages):
    clean=normalized(message)
    parts=re.split(r"[\r\n]",clean)
    semantic=[part for part in parts if any(part.startswith(prefix) for prefix in claimed)]
    if semantic and (len(parts)!=1 or clean!=semantic[0]):
        raise SystemExit("FAIL io-fault: claimed logical-line smuggling")

def family(prefix):
    return [(i,row,normalized(message)) for i,(row,message) in enumerate(zip(rows,messages))
            if normalized(message).startswith(prefix)]
def exact(prefix,wanted,sev,cat,label):
    found=family(prefix)
    if len(found)!=1: raise SystemExit(f"FAIL io-fault: {label} cardinality {len(found)}")
    i,row,message=found[0]
    if message!=wanted: raise SystemExit(f"FAIL io-fault: {label} exact message")
    if str(row.get("sev","")).upper()!=sev or str(row.get("cat","")).lower()!=cat:
        raise SystemExit(f"FAIL io-fault: {label} metadata")
    return i

main=family("WiredUI: push menu 'main'")
demos=family("WiredUI: push menu 'demos'")
popups=family("WiredUI: push menu 'error_popup'")
if [x[2] for x in main] != ["WiredUI: push menu 'main' (depth 1)","WiredUI: push menu 'main' (depth 1)"]:
    raise SystemExit("FAIL io-fault: main push inventory")
if any(str(x[1].get("sev","")).upper()!="DEBUG" or str(x[1].get("cat","")).lower()!="ui" for x in main):
    raise SystemExit("FAIL io-fault: main push metadata")
if [x[2] for x in demos] != ["WiredUI: push menu 'demos' (depth 2)","WiredUI: push menu 'demos' (depth 2)"]:
    raise SystemExit("FAIL io-fault: demos push inventory")
if len(popups)!=1 or popups[0][2]!="WiredUI: push menu 'error_popup' (depth 3)":
    raise SystemExit("FAIL io-fault: popup inventory")
pushes=family("WiredUI: push menu ")
if [x[2] for x in pushes] != [main[0][2],demos[0][2],main[1][2],demos[1][2],popups[0][2]]:
    raise SystemExit("FAIL io-fault: global push inventory")
if any(str(x[1].get("sev","")).upper()!="DEBUG" or str(x[1].get("cat","")).lower()!="ui"
       for x in demos+popups+pushes):
    raise SystemExit("FAIL io-fault: push metadata")

loads=family("WiredUI: demos loaded ")
if [x[2] for x in loads] != [
    "WiredUI: demos loaded protocol=74 count=1 generation=1",
    "WiredUI: demos loaded protocol=74 count=1 generation=2",
    "WiredUI: demos loaded protocol=74 count=1 generation=3",
]: raise SystemExit("FAIL io-fault: inventory generations")
feeder=family("WiredUI: demo feeder row=")
if [x[2] for x in feeder] != [
    "WiredUI: demo feeder row=0 name=io.fault generation=1",
    "WiredUI: demo feeder row=0 name=io.fault generation=2",
    "WiredUI: demo feeder row=0 name=io.fault generation=3",
]: raise SystemExit("FAIL io-fault: feeder row inventory")
if any(str(x[1].get("sev","")).upper()!="DEBUG" or str(x[1].get("cat","")).lower()!="ui"
       for x in loads+feeder):
    raise SystemExit("FAIL io-fault: inventory metadata")

selection=exact("WiredUI: demo feeder selection ",
 "WiredUI: demo feeder selection row=0 name=io.fault generation=2","DEBUG","ui","selection")
queue=exact("WiredUI: queued validated demo playback ",
 "WiredUI: queued validated demo playback name=io.fault","DEBUG","ui","queue")
close=exact("WiredUI: close all postcondition ",
 "WiredUI: close all postcondition depth=0 active=none catcher_ui=0 paused=0",
 "DEBUG","ui","CloseAll")
arm=exact("Demo read fault armed ","Demo read fault armed after_bytes=6","DEBUG","client","fault arm")
injected=exact("Demo read fault injected ",
 "Demo read fault injected after_bytes=6 requested=2","DEBUG","client","fault injection")
file_index=exact("Demo file: demos/","Demo file: demos/io.fault.dm_74","INFO","client","demo file")
reject=exact("Demo playback rejected ",
 "Demo playback rejected reason=io-error continuation=0","INFO","client","typed rejection")
error=exact("Error: The demo ","Error: The demo could not be read.","ERROR","system","controlled error")
recovery=exact("Demo playback recovery ",
 "Demo playback recovery reason=io-error depth=3 popup=1 automated=0","INFO","client","recovery")
complete=exact("Q0_DEMO_IO_FAULT_COMPLETE","Q0_DEMO_IO_FAULT_COMPLETE","INFO","system","completion")

nav=family("wui_menu_nav:")
wanted_nav=["wui_menu_nav: K_DOWNARROW dispatched"]*3+["wui_menu_nav: K_ENTER dispatched"]*3
if [x[2] for x in nav]!=wanted_nav: raise SystemExit("FAIL io-fault: exact nav inventory")
focus=family("wui_menu_nav focus:")
wanted_focus=("focused item 'demolist'","focused item 'btn_play_demo'","focused item 'btn_back'")
if len(focus)!=3 or any(token not in focus[i][2] for i,token in enumerate(wanted_focus)):
    raise SystemExit("FAIL io-fault: exact focus inventory")
if any(str(x[1].get("sev","")).upper()!="DEBUG" or str(x[1].get("cat","")).lower()!="ui" for x in nav+focus):
    raise SystemExit("FAIL io-fault: navigation metadata")

pops=family("WiredUI: pop menu ")
if len(pops)!=1 or pops[0][2]!="WiredUI: pop menu (depth 2)":
    raise SystemExit("FAIL io-fault: exact popup dismissal")
if str(pops[0][1].get("sev","")).upper()!="DEBUG" or str(pops[0][1].get("cat","")).lower()!="ui":
    raise SystemExit("FAIL io-fault: popup dismissal metadata")
system_values=[normalized(m) for m in messages if normalized(m).startswith('"com_errorMessage" is:')]
if system_values!=['"com_errorMessage" is:"The demo could not be read."','"com_errorMessage" is:""']:
    raise SystemExit("FAIL io-fault: error state lifecycle")
retry=[normalized(m) for m in messages if normalized(m).startswith('"ui_errorRetry" is:')]
if retry!=['"ui_errorRetry" is:"0"']: raise SystemExit("FAIL io-fault: retry state")

ordered_indices=[loads[0][0],feeder[0][0],main[0][0],demos[0][0],loads[1][0],feeder[1][0],nav[3][0],
                 selection,focus[0][0],focus[1][0],arm,queue,close,
                 nav[4][0],file_index,injected,reject,error,main[1][0],demos[1][0],loads[2][0],
                 feeder[2][0],popups[0][0],recovery,focus[2][0],pops[0][0],nav[5][0],complete]
if ordered_indices!=sorted(ordered_indices) or len(set(ordered_indices))!=len(ordered_indices):
    raise SystemExit("FAIL io-fault: causal order")

zero_prefixes=(
 "Demo read fault refused ","Demo read fault rejected ","demo_read_fault <",
 "Demo semantic parse recovered ","CL_ParseSnapshot: Invalid size","snapshot committed",
 "FIRST GAMEPLAY FRAME","QUIC client: TLV ACCEPT","SV_OnPlayerConnect:",
 "Demo file was truncated","CL_ReadDemoMessage:","CL_NextDemo:",
)
for prefix in zero_prefixes:
    if family(prefix): raise SystemExit(f"FAIL io-fault: forbidden family {prefix}")
if any(str(row.get("sev","")).upper()=="FATAL" for row in rows):
    raise SystemExit("FAIL io-fault: FATAL")
if sum(str(row.get("sev","")).upper()=="ERROR" for row in rows)!=1:
    raise SystemExit("FAIL io-fault: global ERROR cardinality")
if any(str(row.get("sev","")).upper()=="WARN" and str(row.get("cat","")).lower()=="ui" for row in rows):
    raise SystemExit("FAIL io-fault: UI WARN")
fault_family=family("Demo read fault ")
if [x[2] for x in fault_family] != [
    "Demo read fault armed after_bytes=6",
    "Demo read fault injected after_bytes=6 requested=2",
]: raise SystemExit("FAIL io-fault: global fault family inventory")

data=open(demo_path,"rb").read()
if data.hex()!=wanted_hex or len(data)!=21: raise SystemExit("FAIL io-fault: demo bytes/length")
if hashlib.sha256(data).hexdigest()!=wanted_sha: raise SystemExit("FAIL io-fault: demo hash")
print(f"PASS demo I/O fault contract fixture_sha256={wanted_sha}")
PYEOF
}

write_clean() {
    python3 - "$1" "$2" "$DEMO_HEX" <<'PYEOF'
import json,sys
log_path,demo_path,demo_hex=sys.argv[1:]
rows=[]
def add(sev,cat,msg): rows.append({"sev":sev,"cat":cat,"msg":msg+"\n"})
add("DEBUG","ui","WiredUI: demos loaded protocol=74 count=1 generation=1")
add("DEBUG","ui","WiredUI: demo feeder row=0 name=io.fault generation=1")
add("DEBUG","ui","WiredUI: push menu 'main' (depth 1)")
for _ in range(3): add("DEBUG","ui","wui_menu_nav: K_DOWNARROW dispatched")
add("DEBUG","ui","WiredUI: push menu 'demos' (depth 2)")
add("DEBUG","ui","WiredUI: demos loaded protocol=74 count=1 generation=2")
add("DEBUG","ui","WiredUI: demo feeder row=0 name=io.fault generation=2")
add("DEBUG","ui","wui_menu_nav: K_ENTER dispatched")
add("DEBUG","ui","WiredUI: demo feeder selection row=0 name=io.fault generation=2")
add("DEBUG","ui","wui_menu_nav focus: focused item 'demolist' (top index -1)")
add("DEBUG","ui","wui_menu_nav focus: focused item 'btn_play_demo' (top index -1)")
add("DEBUG","client","Demo read fault armed after_bytes=6")
add("DEBUG","ui","WiredUI: queued validated demo playback name=io.fault")
add("DEBUG","ui","WiredUI: close all postcondition depth=0 active=none catcher_ui=0 paused=0")
add("DEBUG","ui","wui_menu_nav: K_ENTER dispatched")
add("INFO","client","Demo file: demos/io.fault.dm_74")
add("DEBUG","client","Demo read fault injected after_bytes=6 requested=2")
add("INFO","client","Demo playback rejected reason=io-error continuation=0")
add("ERROR","system","Error: The demo could not be read.")
add("DEBUG","ui","WiredUI: push menu 'main' (depth 1)")
add("DEBUG","ui","WiredUI: push menu 'demos' (depth 2)")
add("DEBUG","ui","WiredUI: demos loaded protocol=74 count=1 generation=3")
add("DEBUG","ui","WiredUI: demo feeder row=0 name=io.fault generation=3")
add("DEBUG","ui","WiredUI: push menu 'error_popup' (depth 3)")
add("INFO","client","Demo playback recovery reason=io-error depth=3 popup=1 automated=0")
add("INFO","system",'"com_errorMessage" is:"The demo could not be read."')
add("INFO","system",'"ui_errorRetry" is:"0"')
add("DEBUG","ui","wui_menu_nav focus: focused item 'btn_back' (top index -1)")
add("DEBUG","ui","WiredUI: pop menu (depth 2)")
add("DEBUG","ui","wui_menu_nav: K_ENTER dispatched")
add("INFO","system",'"com_errorMessage" is:""')
add("INFO","system","Q0_DEMO_IO_FAULT_COMPLETE")
with open(log_path,"w") as f:
    for row in rows: f.write(json.dumps(row)+"\n")
open(demo_path,"wb").write(bytes.fromhex(demo_hex))
PYEOF
}

if [ "${1:-}" = "--analyze" ]; then
    [ "$#" -eq 3 ] || { echo "usage: $0 --analyze qconsole.jsonl demo.dm_74"; exit 64; }
    analyze_contract "$2" "$3"
    exit $?
fi

if [ "${1:-}" = "--self-test" ]; then
    ROOT="$(mktemp -d -t wired-demo-io-self-XXXXXX 2>/dev/null || mktemp -d)"
    trap 'rm -rf "$ROOT"' EXIT
    write_clean "$ROOT/clean.jsonl" "$ROOT/io.fault.dm_74"
    analyze_contract "$ROOT/clean.jsonl" "$ROOT/io.fault.dm_74" >/dev/null || exit 1
    defects=(missing-arm duplicate-arm wrong-budget missing-injection duplicate-injection wrong-request wrong-injected-budget injection-category injection-suffix injection-cr-smuggle additive-fault wrong-reason structural-reason missing-error wrong-error second-error missing-popup wrong-recovery missing-clear retry-visible missing-complete semantic-marker generic-error snapshot-publication truncated-log first-frame accept admission wrong-selection wrong-queue wrong-file extra-enter extra-focus extra-push wrong-pop invalid-command refusal-command bad-fixture)
    for defect in "${defects[@]}"; do
        cp "$ROOT/clean.jsonl" "$ROOT/$defect.jsonl"
        python3 - "$ROOT/$defect.jsonl" "$defect" <<'PYEOF'
import json,sys
path,mode=sys.argv[1:]; rows=[json.loads(x) for x in open(path) if x.strip()]
def find(text): return next(i for i,r in enumerate(rows) if text in r["msg"])
if mode=="missing-arm": rows.pop(find("fault armed"))
elif mode=="duplicate-arm":
    i=find("fault armed"); rows.insert(i,dict(rows[i]))
elif mode=="wrong-budget": rows[find("fault armed")]["msg"]="Demo read fault armed after_bytes=7\n"
elif mode=="missing-injection": rows.pop(find("fault injected"))
elif mode=="duplicate-injection":
    i=find("fault injected"); rows.insert(i,dict(rows[i]))
elif mode=="wrong-request": rows[find("fault injected")]["msg"]="Demo read fault injected after_bytes=6 requested=4\n"
elif mode=="wrong-injected-budget": rows[find("fault injected")]["msg"]="Demo read fault injected after_bytes=5 requested=2\n"
elif mode=="injection-category": rows[find("fault injected")]["cat"]="system"
elif mode=="injection-suffix": rows[find("fault injected")]["msg"]="Demo read fault injected after_bytes=6 requested=2 EXTRA\n"
elif mode=="injection-cr-smuggle":
    i=find("fault injected"); rows.insert(i,{"sev":"DEBUG","cat":"client","msg":"benign\r"+rows[i]["msg"]})
elif mode=="additive-fault": rows.insert(find("fault injected"),{"sev":"DEBUG","cat":"client","msg":"Demo read fault other=1\n"})
elif mode=="wrong-reason": rows[find("playback rejected")]["msg"]="Demo playback rejected reason=truncated-length continuation=0\n"
elif mode=="structural-reason": rows.insert(find("playback rejected"),{"sev":"INFO","cat":"client","msg":"Demo playback rejected reason=truncated-length continuation=0\n"})
elif mode=="missing-error": rows.pop(find("Error: The demo"))
elif mode=="wrong-error": rows[find("Error: The demo")]["msg"]="Error: The demo has a truncated message length.\n"
elif mode=="second-error": rows.append({"sev":"ERROR","cat":"system","msg":"unrelated error\n"})
elif mode=="missing-popup": rows.pop(find("error_popup"))
elif mode=="wrong-recovery": rows[find("playback recovery")]["msg"]="Demo playback recovery reason=io-error depth=2 popup=1 automated=0\n"
elif mode=="missing-clear": rows[find('com_errorMessage\" is:\"\"')]["msg"]='"com_errorMessage" is:"still live"\n'
elif mode=="retry-visible": rows[find("ui_errorRetry")]["msg"]='"ui_errorRetry" is:"1"\n'
elif mode=="missing-complete": rows.pop(find("Q0_DEMO_IO_FAULT_COMPLETE"))
elif mode=="semantic-marker": rows.insert(find("playback rejected"),{"sev":"DEBUG","cat":"client","msg":"Demo semantic parse recovered command=7 generic_teardown=0\n"})
elif mode=="generic-error": rows.insert(find("playback rejected"),{"sev":"ERROR","cat":"system","msg":"CL_ParseSnapshot: Invalid size 255 for areamask\n"})
elif mode=="snapshot-publication": rows.insert(find("playback rejected"),{"sev":"DEBUG","cat":"client","msg":"snapshot committed message=1\n"})
elif mode=="truncated-log": rows.insert(find("playback rejected"),{"sev":"INFO","cat":"client","msg":"Demo file was truncated.\n"})
elif mode=="first-frame": rows.append({"sev":"INFO","cat":"client","msg":"FIRST GAMEPLAY FRAME mapname=arena7 numEntities=1\n"})
elif mode=="accept": rows.append({"sev":"DEBUG","cat":"network","msg":"QUIC client: TLV ACCEPT received\n"})
elif mode=="admission": rows.append({"sev":"DEBUG","cat":"server","msg":"SV_OnPlayerConnect: conn=1\n"})
elif mode=="wrong-selection": rows[find("demo feeder selection")]["msg"]="WiredUI: demo feeder selection row=0 name=other generation=2\n"
elif mode=="wrong-queue": rows[find("queued validated demo")]["msg"]="WiredUI: queued validated demo playback name=other\n"
elif mode=="wrong-file": rows[find("Demo file:")]["msg"]="Demo file: demos/other.dm_74\n"
elif mode=="extra-enter": rows.insert(find("fault armed"),{"sev":"DEBUG","cat":"ui","msg":"wui_menu_nav: K_ENTER dispatched\n"})
elif mode=="extra-focus": rows.insert(find("fault armed"),{"sev":"DEBUG","cat":"ui","msg":"wui_menu_nav focus: focused item 'demolist' (top index -1)\n"})
elif mode=="extra-push": rows.insert(find("fault armed"),{"sev":"DEBUG","cat":"ui","msg":"WiredUI: push menu 'other' (depth 3)\n"})
elif mode=="wrong-pop": rows[find("WiredUI: pop menu")]["msg"]="WiredUI: pop menu (depth 1)\n"
elif mode=="invalid-command": rows.insert(find("fault armed"),{"sev":"WARN","cat":"client","msg":"Demo read fault rejected invalid_budget=1\n"})
elif mode=="refusal-command": rows.insert(find("fault armed"),{"sev":"WARN","cat":"client","msg":"Demo read fault refused automated=0\n"})
with open(path,"w") as f:
    for row in rows: f.write(json.dumps(row)+"\n")
PYEOF
        demo="$ROOT/io.fault.dm_74"
        if [ "$defect" = bad-fixture ]; then printf '\001' >>"$demo"; fi
        if analyze_contract "$ROOT/$defect.jsonl" "$demo" >/dev/null 2>&1; then
            echo "FAIL: I/O analyzer accepted defect $defect"; exit 1
        fi
        if [ "$defect" = bad-fixture ]; then python3 - "$demo" "$DEMO_HEX" <<'PYEOF'
import sys
open(sys.argv[1],"wb").write(bytes.fromhex(sys.argv[2]))
PYEOF
        fi
    done
    echo "==> SELF-TEST PASS: demo I/O clean accepted; ${#defects[@]} defects rejected"
    exit 0
fi

WIRED="${1:-}"
[ -n "$WIRED" ] && [ -x "$WIRED" ] || { echo "usage: $0 /absolute/path/to/wired"; exit 64; }
WIRED="$(cd "$(dirname "$WIRED")" && pwd)/$(basename "$WIRED")"
WIRED_DIR="$(dirname "$WIRED")"
PACK_ROOT=""
for candidate in "$WIRED_DIR" "$WIRED_DIR/../Resources" "$WIRED_DIR/../../.."; do
    [ -f "$candidate/base/pax21.sw3z" ] && { PACK_ROOT="$(cd "$candidate" && pwd)"; break; }
done
[ -n "$PACK_ROOT" ] || { echo "SKIP: current pax21 not found"; exit 77; }
CONTENT_ROOT=""
for candidate in "${WIRED_CONTENT_ROOT:-}" "$PACK_ROOT"; do
    [ -n "$candidate" ] || continue
    if [ -f "$candidate/base/pax01.sw3z" ] || [ -f "$candidate/base/pak0.pk3" ]; then
        CONTENT_ROOT="$(cd "$candidate" && pwd)"; break
    fi
done
[ -n "$CONTENT_ROOT" ] || { echo "SKIP: set WIRED_CONTENT_ROOT"; exit 77; }
if [ -f "$CONTENT_ROOT/base/pax01.sw3z" ]; then BASE_ARCHIVE="$CONTENT_ROOT/base/pax01.sw3z"; else BASE_ARCHIVE="$CONTENT_ROOT/base/pak0.pk3"; fi

RUN_ROOT="$(mktemp -d -t wired-demo-io-XXXXXX 2>/dev/null || mktemp -d)"
HOME_DIR="$RUN_ROOT/home/q3now-preview"
mkdir -p "$HOME_DIR/base/demos" "$RUN_ROOT/run"
cleanup(){ if [ "${WIRED_KEEP_ARTIFACTS:-0}" = 1 ]; then echo "    kept artifacts: $RUN_ROOT"; else rm -rf "$RUN_ROOT"; fi; }
trap cleanup EXIT INT TERM
cp "$BASE_ARCHIVE" "$HOME_DIR/base/" || exit 1
cp "$PACK_ROOT/base/pax21.sw3z" "$HOME_DIR/base/pax21.sw3z" || exit 1
cp "$CFG_SOURCE" "$HOME_DIR/base/wiredui-demo-io-fault.cfg" || exit 1
python3 - "$HOME_DIR/base/demos/io.fault.dm_74" "$DEMO_HEX" <<'PYEOF'
import sys
open(sys.argv[1],"wb").write(bytes.fromhex(sys.argv[2]))
PYEOF
case "$(uname -s)" in
Darwin) NATIVE_HOME="$HOME_DIR"; PLATFORM_ARGS=( -ApplePersistenceIgnoreState YES );;
MINGW*|MSYS*|CYGWIN*) NATIVE_HOME="$(cygpath -w "$HOME_DIR")"; PLATFORM_ARGS=();;
*) NATIVE_HOME="$HOME_DIR"; PLATFORM_ARGS=();;
esac

echo "==> Demo I/O fault: exact next-playback adapter failure and controlled Demos recovery"
python3 "$TIMEOUT_RUNNER" --timeout 180 --kill-after 15 --cwd "$RUN_ROOT/run" --stdout "$RUN_ROOT/wired.stdout" -- \
    "$WIRED" "${PLATFORM_ARGS[@]}" +set fs_homepath "$NATIVE_HOME" \
    +set com_automated 1 +set com_noHardReboot 1 +set s_initsound 0 \
    +set r_fullscreen 0 +set r_mode -1 +set r_customwidth 1280 +set r_customheight 720 \
    +set log_severity DEBUG +set log_file_severity DEBUG +set log_file_mode overwrite_synced \
    +exec wiredui-demo-io-fault.cfg
[ "$?" -eq 0 ] || { echo "FAIL: demo I/O fault client did not exit cleanly"; exit 1; }
LOG="$HOME_DIR/qconsole.jsonl"; DEMO="$HOME_DIR/base/demos/io.fault.dm_74"
[ -s "$LOG" ] && [ -s "$DEMO" ] || { echo "FAIL: missing demo I/O evidence"; exit 1; }
analyze_contract "$LOG" "$DEMO" || exit 1
python3 - "$WIRED" "$PACK_ROOT/base/pax21.sw3z" "$BASE_ARCHIVE" "$0" "$CFG_SOURCE" <<'PYEOF'
import hashlib,sys
for label,path in zip(("binary","pax21","base","harness","cfg"),sys.argv[1:]):
    print(f"    {label}_sha256={hashlib.sha256(open(path,'rb').read()).hexdigest()}")
PYEOF
echo "==> WiredUI Demo I/O fault gate: PASS"
