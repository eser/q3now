#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Current-protocol malformed demo gate: exact oversize envelope through the
# authored Main -> Demos -> feeder -> Play route, controlled popup recovery.

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
. "$SCRIPT_DIR/lib/wired_paths.sh"
TIMEOUT_RUNNER="$SCRIPT_DIR/run-with-timeout.py"
CFG_SOURCE="$SCRIPT_DIR/fixtures/wiredui-demo-malformed.cfg"
MATRIX_CFG_SOURCE="$SCRIPT_DIR/fixtures/wiredui-demo-malformed-matrix.cfg"
EXPECTED_SHA="c2817cad5cd157cf2bee911e4eda0d101b45b5a2e080d2c97deaffb912251782"

analyze_contract() {
    python3 - "$1" "$2" "$EXPECTED_SHA" <<'PYEOF'
import hashlib,json,re,struct,sys
log_path,demo_path,wanted_sha=sys.argv[1:]
rows=[]
with open(log_path,encoding="utf-8",errors="replace") as f:
    for number,line in enumerate(f,1):
        if not line.strip(): continue
        try: row=json.loads(line)
        except ValueError as exc: raise SystemExit(f"FAIL {log_path}:{number}: invalid JSON: {exc}")
        if not isinstance(row,dict): raise SystemExit(f"FAIL {log_path}:{number}: JSON is not an object")
        rows.append(row)
if not rows: raise SystemExit("FAIL malformed: empty product evidence")
messages=[str(row.get("msg","")) for row in rows]

expected_error="Error: The demo contains a message larger than the protocol limit."
errors=[row for row in rows if str(row.get("sev","")).upper()=="ERROR"]
if len(errors)!=1 or str(errors[0].get("cat","")).lower()!="system" or str(errors[0].get("msg","")).strip()!=expected_error:
    raise SystemExit(f"FAIL malformed: expected one exact system ERROR, got {len(errors)}")
bad=[row for row in rows
     if str(row.get("sev","")).upper()=="FATAL"
     or (str(row.get("sev","")).upper()=="WARN"
         and str(row.get("cat","")).lower()=="ui"
         and str(row.get("msg",""))!="WiredUI: no demo selected\n")]
if bad: raise SystemExit(f"FAIL malformed: {len(bad)} unexpected FATAL/UI-WARN")
if sum(m=="WiredUI: no demo selected\n" for m in messages)!=1:
    raise SystemExit("FAIL malformed: deliberate preselection refusal cardinality")

def ordered(steps):
    cursor=0
    for label,pattern in steps:
        for index in range(cursor,len(messages)):
            if re.search(pattern,messages[index]): cursor=index+1; break
        else: raise SystemExit(f"FAIL malformed: missing/out-of-order {label}")

ordered([
 ("main",r"WiredUI: push menu 'main' \(depth 1\)"),
 ("down1",r"wui_menu_nav: K_DOWNARROW dispatched"),
 ("down2",r"wui_menu_nav: K_DOWNARROW dispatched"),
 ("down3",r"wui_menu_nav: K_DOWNARROW dispatched"),
 ("demos",r"WiredUI: push menu 'demos' \(depth 2\)"),
 ("inventory",r"WiredUI: demos loaded protocol=74 count=1 generation=2\b"),
 ("row",r"demo feeder row=0 name=broken\.current generation=2\b"),
 ("route-enter",r"wui_menu_nav: K_ENTER dispatched"),
 ("refusal",r"WiredUI: no demo selected"),
 ("selection",r"demo feeder selection row=0 name=broken\.current generation=2\b"),
 ("queue",r"queued validated demo playback name=broken\.current\b"),
 ("close",r"close all postcondition depth=0 active=none catcher_ui=0 paused=0"),
 ("play-enter",r"wui_menu_nav: K_ENTER dispatched"),
 ("file",r"Demo file: demos/broken\.current\.dm_75\n?$"),
 ("reject",r"Demo playback rejected reason=oversize continuation=0\b"),
 ("error",re.escape(expected_error)),
 ("main-recovery",r"WiredUI: push menu 'main' \(depth 1\)"),
 ("demos-recovery",r"WiredUI: push menu 'demos' \(depth 2\)"),
 ("reload",r"WiredUI: demos loaded protocol=74 count=1 generation=3\b"),
 ("reload-row",r"demo feeder row=0 name=broken\.current generation=3\b"),
 ("popup",r"WiredUI: push menu 'error_popup' \(depth 3\)"),
 ("recovery",r"Demo playback recovery reason=oversize depth=3 popup=1 automated=0\b"),
 ("message-live",r'"com_errorMessage" is:"(?:\^7)?The demo contains a message larger than the protocol limit\."'),
 ("retry-key",r"  key:   ui_errorRetry"),
 ("retry-hidden",r'  text:  "0"'),
 ("Back focus",r"wui_menu_nav focus: focused item 'btn_back'"),
 ("popup-dismiss",r"WiredUI: pop menu \(depth 2\)"),
 ("dismiss-enter",r"wui_menu_nav: K_ENTER dispatched"),
 ("message-cleared",r'"com_errorMessage" is:"(?:\^7)?"'),
 ("complete",r"Q0_MALFORMED_DEMO_COMPLETE"),
])

main_push=[i for i,m in enumerate(messages) if re.search(r"push menu 'main' \(depth 1\)",m)]
demos_push=[i for i,m in enumerate(messages) if re.search(r"push menu 'demos' \(depth 2\)",m)]
popup_push=[i for i,m in enumerate(messages) if re.search(r"push menu 'error_popup' \(depth 3\)",m)]
if len(main_push)!=2 or len(demos_push)!=2 or len(popup_push)!=1:
    raise SystemExit("FAIL malformed: recovery stack cardinality")
route=messages[main_push[0]+1:demos_push[0]]
if sum("K_DOWNARROW dispatched" in m for m in route)!=3:
    raise SystemExit("FAIL malformed: authored route must contain exactly three Downs")
selections=[m for m in messages if "demo feeder selection" in m]
if len(selections)!=1 or not re.search(r"row=0 name=broken\.current generation=2\b",selections[0]):
    raise SystemExit("FAIL malformed: exact selection cardinality")
queues=[m for m in messages if "queued validated demo playback" in m]
if len(queues)!=1 or not re.search(r"name=broken\.current\b",queues[0]):
    raise SystemExit("FAIL malformed: exact queue cardinality")
feeder_rows=[m for m in messages if "demo feeder row=" in m]
if any(not re.search(r"row=0 name=broken\.current generation=[0-9]+\b",m) for m in feeder_rows):
    raise SystemExit("FAIL malformed: unexpected feeder row identity")
for generation in (2,3):
    if sum(re.search(rf"row=0 name=broken\.current generation={generation}\b",m) is not None for m in feeder_rows)!=1:
        raise SystemExit(f"FAIL malformed: generation {generation} row cardinality")
for forbidden in ("FIRST GAMEPLAY FRAME","demo playback trace state=8",
                  "QUIC client: TLV ACCEPT","SV_OnPlayerConnect:",
                  "Q0_BAD_MALFORMED_ACTIVEACTION","CL_NextDemo:"):
    if any(forbidden in m for m in messages):
        raise SystemExit(f"FAIL malformed: forbidden outcome {forbidden}")

data=open(demo_path,"rb").read()
if len(data)!=8: raise SystemExit(f"FAIL malformed: fixture length {len(data)} != 8")
if data.hex()!="df9b571301400000": raise SystemExit("FAIL malformed: fixture bytes changed")
sequence,length=struct.unpack("<ii",data)
if sequence!=0x13579bdf or length!=16385:
    raise SystemExit("FAIL malformed: fixture fields changed")
actual_sha=hashlib.sha256(data).hexdigest()
if actual_sha!=wanted_sha: raise SystemExit("FAIL malformed: fixture hash changed")
print(f"PASS malformed demo contract fixture_sha256={actual_sha}")
PYEOF
}

analyze_attract_contract() {
    python3 - "$1" <<'PYEOF'
import json,re,sys
rows=[json.loads(x) for x in open(sys.argv[1],errors="replace") if x.strip()]
messages=[str(r.get("msg","")) for r in rows]
expected="Error: The demo contains a message larger than the protocol limit."
errors=[r for r in rows if str(r.get("sev","")).upper()=="ERROR"]
if (len(errors)!=1 or str(errors[0].get("cat","")).lower()!="system" or
        str(errors[0].get("msg","")).strip()!=expected):
    raise SystemExit("FAIL attract malformed: exact ERROR cardinality")
cursor=0
for label,pattern in (
    ("file",r"Demo file: demos/broken\.current\.dm_75\n?$"),
    ("reject",r"Demo playback rejected reason=oversize continuation=1\b"),
    ("error",re.escape(expected)),
    ("status",r"attract_status:"),
    ("playing",r"  state         : PLAYING"),
    ("index",r"  currentIndex  : 1\b"),
    ("ownership",r"  ownsDemo      : 0 \(demoplaying=0\)"),
	("panel",r"  pushedPanel   : attract_brand\b"),
	("message-cleared",r'"com_errorMessage" is:"(?:\^7)?"'),
    ("done",r"Q0_MALFORMED_ATTRACT_COMPLETE"),
):
    for i in range(cursor,len(messages)):
        if re.search(pattern,messages[i]): cursor=i+1; break
    else: raise SystemExit(f"FAIL attract malformed: missing/out-of-order {label}")
for forbidden in ("push menu 'error_popup'","FIRST GAMEPLAY FRAME","CL_NextDemo:"):
    if any(forbidden in m for m in messages): raise SystemExit(f"FAIL attract malformed: forbidden {forbidden}")
print("PASS malformed attract continuation contract")
PYEOF
}

analyze_loop_contract() {
    python3 - "$1" <<'PYEOF'
import json,re,sys
rows=[json.loads(x) for x in open(sys.argv[1],errors="replace") if x.strip()]
messages=[str(r.get("msg","")) for r in rows]
expected="Error: The demo contains a message larger than the protocol limit."
errors=[r for r in rows if str(r.get("sev","")).upper()=="ERROR"]
if (len(errors)!=4 or any(str(r.get("cat","")).lower()!="system" or
        str(r.get("msg","")).strip()!=expected for r in errors)):
    raise SystemExit("FAIL attract loop: expected four exact system errors")
rejects=[m for m in messages if re.search(r"Demo playback rejected reason=oversize continuation=1\b",m)]
if len(rejects)!=4: raise SystemExit("FAIL attract loop: bounded retry cardinality")
for pattern in (r"attract_status:",r"  state         : STOPPED",r"  currentIndex  : 0\b",
                r"  ownsDemo      : 0 \(demoplaying=0\)",r"  pushedPanel   : none\b",
                r'"com_errorMessage" is:"(?:\^7)?"',r"Q0_MALFORMED_LOOP_COMPLETE"):
    if not any(re.search(pattern,m) for m in messages):
        raise SystemExit(f"FAIL attract loop: missing {pattern}")
for forbidden in ("push menu 'error_popup'","FIRST GAMEPLAY FRAME","CL_NextDemo:"):
    if any(forbidden in m for m in messages): raise SystemExit(f"FAIL attract loop: forbidden {forbidden}")
print("PASS malformed attract loop non-recursion contract")
PYEOF
}

analyze_matrix_contract() {
    python3 - "$1" "$2" <<'PYEOF'
import hashlib,json,re,sys
log_path,fixture_dir=sys.argv[1:]
cases=[
 ("00.missing","missing-terminator","The demo ended without its required terminator.",b""),
 ("01.shortseq","truncated-sequence","The demo has a truncated message sequence.",bytes.fromhex("0100")),
 ("02.shortlen","truncated-length","The demo has a truncated message length.",bytes.fromhex("010000000400")),
 ("03.badterm","invalid-terminator","The demo has an invalid end marker.",bytes.fromhex("ffffffff00000000")),
 ("04.neglen","invalid-length","The demo has an invalid message length.",bytes.fromhex("01000000feffffff")),
 ("05.oversize","oversize","The demo contains a message larger than the protocol limit.",bytes.fromhex("df9b571301400000")),
 ("06.shortpayload","truncated-payload","The demo has a truncated message payload.",bytes.fromhex("0100000004000000aabb")),
 ("07.emptypayload","empty-payload","The demo contains an empty message payload.",bytes.fromhex("0100000000000000ffffffffffffffff")),
 ("08.badmessage","semantic-payload","The demo contains an invalid server message.",bytes.fromhex("0100000002000000aa24ffffffffffffffff")),
 ("09.badsnapshot","semantic-payload","The demo contains an invalid server message.",bytes.fromhex("0100000005000000aabfaa9200ffffffffffffffff")),
]
empty_fixture=cases[7][3]
if len(empty_fixture)!=16 or hashlib.sha256(empty_fixture).hexdigest()!="db0550d553e2a146e34164d19cd55f006c38d700d8f9a4e3ba2c889a1d7c26b2":
    raise SystemExit("FAIL matrix: empty-payload fixture identity")
semantic_fixture=cases[8][3]
if len(semantic_fixture)!=18 or hashlib.sha256(semantic_fixture).hexdigest()!="604e67307bf038c66916e9ee52ae6edec6a636e52923870c5d6c0cb17bd09ff2":
    raise SystemExit("FAIL matrix: semantic-payload fixture identity")
snapshot_fixture=cases[9][3]
if len(snapshot_fixture)!=21 or hashlib.sha256(snapshot_fixture).hexdigest()!="3c4c57e19d889d35b3e913e40bd2c7afd0382b13eb6946aee28271d902208faf":
    raise SystemExit("FAIL matrix: snapshot-areamask fixture identity")
rows=[]
for number,line in enumerate(open(log_path,encoding="utf-8",errors="replace"),1):
    if not line.strip(): continue
    try: row=json.loads(line)
    except ValueError as exc: raise SystemExit(f"FAIL matrix {number}: invalid JSON: {exc}")
    if not isinstance(row,dict): raise SystemExit(f"FAIL matrix {number}: JSON is not an object")
    rows.append(row)
messages=[str(r.get("msg","")) for r in rows]

errors=[r for r in rows if str(r.get("sev","")).upper()=="ERROR"]
if len(errors)!=len(cases): raise SystemExit("FAIL matrix: exact ERROR cardinality")
if any(str(r.get("cat","")).lower()!="system" for r in errors):
    raise SystemExit("FAIL matrix: ERROR category")
error_messages=[(r,m) for r,m in zip(rows,messages) if m.startswith("Error:")]
wanted_errors=[f"Error: {text}\n" for _,_,text,_ in cases]
if [m for _,m in error_messages]!=wanted_errors or any(
        str(r.get("sev","")).upper()!="ERROR" or str(r.get("cat","")).lower()!="system"
        for r,_ in error_messages):
    raise SystemExit("FAIL matrix: global controlled-error sequence")
if any(str(r.get("sev","")).upper()=="FATAL" or
       (str(r.get("sev","")).upper()=="WARN" and str(r.get("cat","")).lower()=="ui") for r in rows):
    raise SystemExit("FAIL matrix: unexpected FATAL/UI-WARN")

cursor=0
def expect_exact(message,label,sev=None,cat=None):
    global cursor
    for pos in range(cursor,len(messages)):
        if messages[pos] != message: continue
        if sev is not None and str(rows[pos].get("sev","")).upper()!=sev:
            raise SystemExit(f"FAIL matrix: {label} severity")
        if cat is not None and str(rows[pos].get("cat","")).lower()!=cat:
            raise SystemExit(f"FAIL matrix: {label} category")
        cursor=pos+1
        return
    raise SystemExit(f"FAIL matrix: missing/out-of-order {label}")

expect_exact("WiredUI: demos loaded protocol=74 count=10 generation=2\n","initial inventory","DEBUG","ui")
for row,(row_name,_,_,_) in enumerate(cases):
    expect_exact(f"WiredUI: demo feeder row={row} name={row_name} generation=2\n",f"initial row {row}","DEBUG","ui")

for index,(name,reason,text,wanted) in enumerate(cases):
    generation=index+2
    if index:
        previous=cases[index-1][0]
        expect_exact(f"WiredUI: demo feeder selection row={index-1} name={previous} generation={generation}\n",f"case {index} prior cursor selection","DEBUG","ui")
    expect_exact(f"WiredUI: demo feeder selection row={index} name={name} generation={generation}\n",f"case {index} selection","DEBUG","ui")
    expect_exact(f"WiredUI: queued validated demo playback name={name}\n",f"case {index} queue","DEBUG","ui")
    expect_exact("WiredUI: close all postcondition depth=0 active=none catcher_ui=0 paused=0\n",f"case {index} CloseAll","DEBUG","ui")
    expect_exact("wui_menu_nav: K_ENTER dispatched\n",f"case {index} Play Enter","DEBUG","ui")
    expect_exact(f"Demo file: demos/{name}.dm_75\n",f"case {index} file","INFO","client")
    if name=="08.badmessage":
        expect_exact("Demo semantic parse recovered command=255 generic_teardown=0\n",
                     "semantic local recovery marker","DEBUG","client")
    elif name=="09.badsnapshot":
        expect_exact("Demo semantic parse recovered command=7 detail=snapshot-areamask areabytes=255 generic_teardown=0\n",
                     "snapshot areamask local recovery marker","DEBUG","client")
    expect_exact(f"Demo playback rejected reason={reason} continuation=0\n",f"case {index} reject","INFO","client")
    expect_exact(f"Error: {text}\n",f"case {index} error","ERROR","system")
    expect_exact("WiredUI: push menu 'main' (depth 1)\n",f"case {index} main recovery","DEBUG","ui")
    expect_exact("WiredUI: push menu 'demos' (depth 2)\n",f"case {index} demos recovery","DEBUG","ui")
    expect_exact(f"WiredUI: demos loaded protocol=74 count=10 generation={generation+1}\n",f"case {index} reload","DEBUG","ui")
    for row,(row_name,_,_,_) in enumerate(cases):
        expect_exact(f"WiredUI: demo feeder row={row} name={row_name} generation={generation+1}\n",f"case {index} reload row {row}","DEBUG","ui")
    expect_exact("WiredUI: push menu 'error_popup' (depth 3)\n",f"case {index} popup","DEBUG","ui")
    expect_exact(f"Demo playback recovery reason={reason} depth=3 popup=1 automated=0\n",f"case {index} recovery","INFO","client")
    expect_exact(f'"com_errorMessage" is:"{text}"',f"case {index} message live","INFO","system")
    expect_exact("  key:   ui_errorRetry\n",f"case {index} retry key","INFO","client")
    expect_exact('  text:  "0"\n',f"case {index} retry hidden","INFO","client")
    expect_exact("wui_menu_nav focus: focused item 'btn_back' (top index -1)\n",f"case {index} Back focus","DEBUG","ui")
    expect_exact("WiredUI: pop menu (depth 2)\n",f"case {index} popup dismiss","DEBUG","ui")
    expect_exact("wui_menu_nav: K_ENTER dispatched\n",f"case {index} Back Enter","DEBUG","ui")
    expect_exact('"com_errorMessage" is:""',f"case {index} message clear","INFO","system")
    expect_exact(f"Q0_MALFORMED_MATRIX_CASE_{index}\n",f"case {index} marker","INFO","system")
    path=f"{fixture_dir}/{name}.dm_75"
    actual=open(path,"rb").read()
    if actual!=wanted: raise SystemExit(f"FAIL matrix case {index}: fixture bytes")
    if hashlib.sha256(actual).hexdigest()!=hashlib.sha256(wanted).hexdigest():
        raise SystemExit(f"FAIL matrix case {index}: fixture hash")

expect_exact("Q0_MALFORMED_MATRIX_COMPLETE\n","completion","INFO","system")

if sum("Q0_MALFORMED_MATRIX_CASE_" in m for m in messages)!=len(cases):
    raise SystemExit("FAIL matrix: case marker cardinality")
if sum("queued validated demo playback" in m for m in messages)!=len(cases):
    raise SystemExit("FAIL matrix: queue cardinality")
if sum("demo feeder selection" in m for m in messages)!=len(cases)*2-1:
    raise SystemExit("FAIL matrix: selection cardinality")
if sum("Demo playback rejected reason=" in m for m in messages)!=len(cases):
    raise SystemExit("FAIL matrix: rejection cardinality")
semantic_markers=[(r,m) for r,m in zip(rows,messages) if m.startswith("Demo semantic parse recovered command=")]
wanted_semantic_markers=[
    "Demo semantic parse recovered command=255 generic_teardown=0\n",
    "Demo semantic parse recovered command=7 detail=snapshot-areamask areabytes=255 generic_teardown=0\n",
]
if [m for _,m in semantic_markers]!=wanted_semantic_markers or any(
        str(r.get("sev","")).upper()!="DEBUG" or
        str(r.get("cat","")).lower()!="client" for r,_ in semantic_markers):
    raise SystemExit("FAIL matrix: semantic local recovery marker")
for message in messages:
    normalized=message[:-1] if message.endswith("\n") else message
    logical_lines=re.split(r"[\r\n]",normalized)
    for logical in logical_lines:
        if logical.startswith("Demo semantic parse recovered command=") and normalized!=logical:
            raise SystemExit("FAIL matrix: embedded semantic recovery marker")
popup_messages=[(r,m) for r,m in zip(rows,messages) if m.startswith("WiredUI: push menu 'error_popup'")]
if len(popup_messages)!=len(cases) or any(m!="WiredUI: push menu 'error_popup' (depth 3)\n" or
        str(r.get("sev","")).upper()!="DEBUG" or str(r.get("cat","")).lower()!="ui"
        for r,m in popup_messages):
    raise SystemExit("FAIL matrix: popup cardinality")
if sum(m.startswith("Demo file: demos/") for m in messages)!=len(cases):
    raise SystemExit("FAIL matrix: file cardinality")
if sum(m.startswith("Demo playback recovery reason=") for m in messages)!=len(cases):
    raise SystemExit("FAIL matrix: recovery cardinality")
if sum(m.startswith('"com_errorMessage" is:') for m in messages)!=len(cases)*2:
    raise SystemExit("FAIL matrix: error-message cvar cardinality")
retry_messages=[]
for i,message in enumerate(messages[:-1]):
    if message=="  key:   ui_errorRetry\n":retry_messages.append((rows[i],rows[i+1],messages[i+1]))
if len(retry_messages)!=len(cases) or any(m!='  text:  "0"\n' or
        any(str(r.get("sev","")).upper()!="INFO" or str(r.get("cat","")).lower()!="client" for r in pair)
        for *pair,m in retry_messages):
    raise SystemExit("FAIL matrix: retry cardinality")
pop_messages=[(r,m) for r,m in zip(rows,messages) if m.startswith("WiredUI: pop menu")]
if len(pop_messages)!=len(cases) or any(m!="WiredUI: pop menu (depth 2)\n" or
        str(r.get("sev","")).upper()!="DEBUG" or str(r.get("cat","")).lower()!="ui"
        for r,m in pop_messages):
    raise SystemExit("FAIL matrix: pop cardinality")
close_messages=[(r,m) for r,m in zip(rows,messages) if m.startswith("WiredUI: close all postcondition")]
if len(close_messages)!=len(cases) or any(
        m!="WiredUI: close all postcondition depth=0 active=none catcher_ui=0 paused=0\n" or
        str(r.get("sev","")).upper()!="DEBUG" or str(r.get("cat","")).lower()!="ui"
        for r,m in close_messages):
    raise SystemExit("FAIL matrix: CloseAll cardinality")
if sum(m=="wui_menu_nav: K_ENTER dispatched\n" for m in messages)!=len(cases)*2+1:
    raise SystemExit("FAIL matrix: Enter cardinality")
enter_messages=[(r,m) for r,m in zip(rows,messages) if m.startswith("wui_menu_nav: K_ENTER dispatched")]
if len(enter_messages)!=len(cases)*2+1 or any(
        m!="wui_menu_nav: K_ENTER dispatched\n" or
        str(r.get("sev","")).upper()!="DEBUG" or str(r.get("cat","")).lower()!="ui"
        for r,m in enter_messages):
    raise SystemExit("FAIL matrix: global Enter inventory")
focus_messages=[(r,m) for r,m in zip(rows,messages) if m.startswith("wui_menu_nav focus:")]
wanted_focus=[message for _ in cases for message in (
    "wui_menu_nav focus: focused item 'demolist' (top index -1)\n",
    "wui_menu_nav focus: focused item 'btn_play_demo' (top index -1)\n",
    "wui_menu_nav focus: focused item 'btn_back' (top index -1)\n")]
if [m for _,m in focus_messages]!=wanted_focus or any(
        str(r.get("sev","")).upper()!="DEBUG" or str(r.get("cat","")).lower()!="ui"
        for r,_ in focus_messages):
    raise SystemExit("FAIL matrix: global focus sequence")
nav_messages=[(r,m) for r,m in zip(rows,messages) if m.startswith((
    "wui_menu_nav: K_DOWNARROW dispatched", "wui_menu_nav: K_ENTER dispatched",
    "wui_menu_nav focus:"))]
wanted_nav=["wui_menu_nav: K_DOWNARROW dispatched\n"]*3
wanted_nav.append("wui_menu_nav: K_ENTER dispatched\n")
for index,_ in enumerate(cases):
    wanted_nav.append("wui_menu_nav focus: focused item 'demolist' (top index -1)\n")
    if index:
        wanted_nav.append("wui_menu_nav: K_DOWNARROW dispatched\n")
    wanted_nav.extend((
        "wui_menu_nav focus: focused item 'btn_play_demo' (top index -1)\n",
        "wui_menu_nav: K_ENTER dispatched\n",
        "wui_menu_nav focus: focused item 'btn_back' (top index -1)\n",
        "wui_menu_nav: K_ENTER dispatched\n"))
if [m for _,m in nav_messages]!=wanted_nav or any(
        str(r.get("sev","")).upper()!="DEBUG" or str(r.get("cat","")).lower()!="ui"
        for r,_ in nav_messages):
    raise SystemExit("FAIL matrix: global keyboard/focus interleaving")
main_pushes=[(r,m) for r,m in zip(rows,messages) if m.startswith("WiredUI: push menu 'main'")]
if len(main_pushes)!=len(cases)+1 or any(m!="WiredUI: push menu 'main' (depth 1)\n" or
        str(r.get("sev","")).upper()!="DEBUG" or str(r.get("cat","")).lower()!="ui"
        for r,m in main_pushes):
    raise SystemExit("FAIL matrix: main push cardinality")
demos_pushes=[(r,m) for r,m in zip(rows,messages) if m.startswith("WiredUI: push menu 'demos'")]
if len(demos_pushes)!=len(cases)+1 or any(m!="WiredUI: push menu 'demos' (depth 2)\n" or
        str(r.get("sev","")).upper()!="DEBUG" or str(r.get("cat","")).lower()!="ui"
        for r,m in demos_pushes):
    raise SystemExit("FAIL matrix: demos push cardinality")
inventory_messages=[(r,m) for r,m in zip(rows,messages) if m.startswith("WiredUI: demos loaded protocol=")]
wanted_inventory=[f"WiredUI: demos loaded protocol=74 count=10 generation={generation}\n" for generation in range(1,len(cases)+3)]
if [m for _,m in inventory_messages]!=wanted_inventory or any(
        str(r.get("sev","")).upper()!="DEBUG" or str(r.get("cat","")).lower()!="ui"
        for r,_ in inventory_messages):
    raise SystemExit("FAIL matrix: global inventory generation sequence")
all_feeder_rows=[(r,m) for r,m in zip(rows,messages) if m.startswith("WiredUI: demo feeder row=")]
wanted_feeder_rows=[f"WiredUI: demo feeder row={row} name={name} generation={generation}\n"
                    for generation in range(1,len(cases)+3) for row,(name,_,_,_) in enumerate(cases)]
if [m for _,m in all_feeder_rows]!=wanted_feeder_rows or any(
        str(r.get("sev","")).upper()!="DEBUG" or str(r.get("cat","")).lower()!="ui"
        for r,_ in all_feeder_rows):
    raise SystemExit("FAIL matrix: global feeder row sequence")
for generation in range(1,len(cases)+3):
    if sum(re.search(rf"demos loaded protocol=74 count=10 generation={generation}\b",m) is not None for m in messages)!=1:
        raise SystemExit(f"FAIL matrix: generation {generation} inventory cardinality")
    rows_for_generation=[m for m in messages if re.search(rf"demo feeder row=\d+ name=.* generation={generation}\b",m)]
    if len(rows_for_generation)!=len(cases): raise SystemExit(f"FAIL matrix: generation {generation} rows")
    actual=[]
    for row in rows_for_generation:
        match=re.search(r"row=(\d+) name=([^ ]+) generation=",row)
        actual.append((int(match.group(1)),match.group(2)))
    wanted_rows=[(i,c[0]) for i,c in enumerate(cases)]
    if actual!=wanted_rows: raise SystemExit(f"FAIL matrix: generation {generation} row identity/order")
for forbidden in ("FIRST GAMEPLAY FRAME","demo playback trace state=8",
                  "QUIC client: TLV ACCEPT","SV_OnPlayerConnect:",
                  "Q0_BAD_MALFORMED_ACTIVEACTION","CL_NextDemo:",
                  "Illegible server message","Server crashed:",
                  "CL_ParseSnapshot: Invalid size", "snapshot committed"):
    if any(forbidden in m for m in messages): raise SystemExit(f"FAIL matrix: forbidden {forbidden}")
if sum(m=="Q0_MALFORMED_MATRIX_COMPLETE\n" for m in messages)!=1:
    raise SystemExit("FAIL matrix: completion cardinality")
print("PASS malformed structural framing + semantic-payload recovery matrix")
PYEOF
}

write_fixture() {
    python3 - "$1" "$2" <<'PYEOF'
import json,struct,sys
log_path,demo_path=sys.argv[1:]
msgs=[
("DEBUG","ui","WiredUI: push menu 'main' (depth 1)\n"),
("DEBUG","ui","wui_menu_nav: K_DOWNARROW dispatched\n"),
("DEBUG","ui","wui_menu_nav: K_DOWNARROW dispatched\n"),
("DEBUG","ui","wui_menu_nav: K_DOWNARROW dispatched\n"),
("DEBUG","ui","WiredUI: push menu 'demos' (depth 2)\n"),
("DEBUG","ui","WiredUI: demos loaded protocol=74 count=1 generation=2\n"),
("DEBUG","ui","WiredUI: demo feeder row=0 name=broken.current generation=2\n"),
("DEBUG","ui","wui_menu_nav: K_ENTER dispatched\n"),
("WARN","ui","WiredUI: no demo selected\n"),
("DEBUG","ui","WiredUI: demo feeder selection row=0 name=broken.current generation=2\n"),
("DEBUG","ui","WiredUI: queued validated demo playback name=broken.current\n"),
("DEBUG","ui","WiredUI: close all postcondition depth=0 active=none catcher_ui=0 paused=0 pointer_down=0\n"),
("DEBUG","ui","wui_menu_nav: K_ENTER dispatched\n"),
("INFO","client","Demo file: demos/broken.current.dm_75\n"),
("INFO","client","Demo playback rejected reason=oversize continuation=0\n"),
("ERROR","system","Error: The demo contains a message larger than the protocol limit.\n"),
("DEBUG","ui","WiredUI: push menu 'main' (depth 1)\n"),
("DEBUG","ui","WiredUI: push menu 'demos' (depth 2)\n"),
("DEBUG","ui","WiredUI: demos loaded protocol=74 count=1 generation=3\n"),
("DEBUG","ui","WiredUI: demo feeder row=0 name=broken.current generation=3\n"),
("DEBUG","ui","WiredUI: push menu 'error_popup' (depth 3)\n"),
("INFO","client","Demo playback recovery reason=oversize depth=3 popup=1 automated=0\n"),
("INFO","system",'"com_errorMessage" is:"The demo contains a message larger than the protocol limit."\n'),
("INFO","client","  key:   ui_errorRetry\n"),
("INFO","client",'  text:  "0"\n'),
("DEBUG","ui","wui_menu_nav focus: focused item 'btn_back' (top index -1)\n"),
("DEBUG","ui","WiredUI: pop menu (depth 2)\n"),
("DEBUG","ui","wui_menu_nav: K_ENTER dispatched\n"),
("INFO","system",'"com_errorMessage" is:""\n'),
("INFO","system","Q0_MALFORMED_DEMO_COMPLETE\n"),
]
with open(log_path,"w") as f:
    for sev,cat,msg in msgs: f.write(json.dumps({"sev":sev,"cat":cat,"msg":msg})+"\n")
open(demo_path,"wb").write(struct.pack("<ii",0x13579bdf,16385))
PYEOF
}

write_matrix_fixture() {
    python3 - "$1" "$2" <<'PYEOF'
import json,os,sys
log_path,fixture_dir=sys.argv[1:]
cases=[
 ("00.missing","missing-terminator","The demo ended without its required terminator.",b""),
 ("01.shortseq","truncated-sequence","The demo has a truncated message sequence.",bytes.fromhex("0100")),
 ("02.shortlen","truncated-length","The demo has a truncated message length.",bytes.fromhex("010000000400")),
 ("03.badterm","invalid-terminator","The demo has an invalid end marker.",bytes.fromhex("ffffffff00000000")),
 ("04.neglen","invalid-length","The demo has an invalid message length.",bytes.fromhex("01000000feffffff")),
 ("05.oversize","oversize","The demo contains a message larger than the protocol limit.",bytes.fromhex("df9b571301400000")),
 ("06.shortpayload","truncated-payload","The demo has a truncated message payload.",bytes.fromhex("0100000004000000aabb")),
 ("07.emptypayload","empty-payload","The demo contains an empty message payload.",bytes.fromhex("0100000000000000ffffffffffffffff")),
 ("08.badmessage","semantic-payload","The demo contains an invalid server message.",bytes.fromhex("0100000002000000aa24ffffffffffffffff")),
 ("09.badsnapshot","semantic-payload","The demo contains an invalid server message.",bytes.fromhex("0100000005000000aabfaa9200ffffffffffffffff")),
]
rows=[]
def add(sev,cat,msg): rows.append({"sev":sev,"cat":cat,"msg":msg+"\n"})
def add_raw(sev,cat,msg): rows.append({"sev":sev,"cat":cat,"msg":msg})
add("DEBUG","ui","WiredUI: demos loaded protocol=74 count=10 generation=1")
for row,(row_name,_,_,_) in enumerate(cases):
    add("DEBUG","ui",f"WiredUI: demo feeder row={row} name={row_name} generation=1")
add("DEBUG","ui","WiredUI: push menu 'main' (depth 1)")
add("DEBUG","ui","wui_menu_nav: K_DOWNARROW dispatched")
add("DEBUG","ui","wui_menu_nav: K_DOWNARROW dispatched")
add("DEBUG","ui","wui_menu_nav: K_DOWNARROW dispatched")
add("DEBUG","ui","WiredUI: push menu 'demos' (depth 2)")
add("DEBUG","ui","WiredUI: demos loaded protocol=74 count=10 generation=2")
for row,(row_name,_,_,_) in enumerate(cases):
    add("DEBUG","ui",f"WiredUI: demo feeder row={row} name={row_name} generation=2")
add("DEBUG","ui","wui_menu_nav: K_ENTER dispatched")
for index,(name,reason,text,data) in enumerate(cases):
    generation=index+2
    add("DEBUG","ui","wui_menu_nav focus: focused item 'demolist' (top index -1)")
    if index:
        add("DEBUG","ui","wui_menu_nav: K_DOWNARROW dispatched")
    add("DEBUG","ui","wui_menu_nav focus: focused item 'btn_play_demo' (top index -1)")
    if index:
        add("DEBUG","ui",f"WiredUI: demo feeder selection row={index-1} name={cases[index-1][0]} generation={generation}")
    add("DEBUG","ui",f"WiredUI: demo feeder selection row={index} name={name} generation={generation}")
    add("DEBUG","ui",f"WiredUI: queued validated demo playback name={name}")
    add("DEBUG","ui","WiredUI: close all postcondition depth=0 active=none catcher_ui=0 paused=0")
    add("DEBUG","ui","wui_menu_nav: K_ENTER dispatched")
    add("INFO","client",f"Demo file: demos/{name}.dm_75")
    if name=="08.badmessage":
        add("DEBUG","client","Demo semantic parse recovered command=255 generic_teardown=0")
    elif name=="09.badsnapshot":
        add("DEBUG","client","Demo semantic parse recovered command=7 detail=snapshot-areamask areabytes=255 generic_teardown=0")
    add("INFO","client",f"Demo playback rejected reason={reason} continuation=0")
    add("ERROR","system",f"Error: {text}")
    add("DEBUG","ui","WiredUI: push menu 'main' (depth 1)")
    add("DEBUG","ui","WiredUI: push menu 'demos' (depth 2)")
    add("DEBUG","ui",f"WiredUI: demos loaded protocol=74 count=10 generation={generation+1}")
    for row,(row_name,_,_,_) in enumerate(cases):
        add("DEBUG","ui",f"WiredUI: demo feeder row={row} name={row_name} generation={generation+1}")
    add("DEBUG","ui","WiredUI: push menu 'error_popup' (depth 3)")
    add("INFO","client",f"Demo playback recovery reason={reason} depth=3 popup=1 automated=0")
    add_raw("INFO","system",f'"com_errorMessage" is:"{text}"')
    add("INFO","client","  key:   ui_errorRetry")
    add("INFO","client",'  text:  "0"')
    add("DEBUG","ui","wui_menu_nav focus: focused item 'btn_back' (top index -1)")
    add("DEBUG","ui","WiredUI: pop menu (depth 2)")
    add("DEBUG","ui","wui_menu_nav: K_ENTER dispatched")
    add_raw("INFO","system",'"com_errorMessage" is:""')
    add("INFO","system",f"Q0_MALFORMED_MATRIX_CASE_{index}")
    open(os.path.join(fixture_dir,name+".dm_75"),"wb").write(data)
add("INFO","system","Q0_MALFORMED_MATRIX_COMPLETE")
with open(log_path,"w") as f:
    for row in rows: f.write(json.dumps(row)+"\n")
PYEOF
}

if [ "${1:-}" = "--analyze-matrix" ]; then
    [ "$#" -eq 3 ] || { echo "usage: $0 --analyze-matrix /path/qconsole.jsonl /path/demos"; exit 64; }
    analyze_matrix_contract "$2" "$3"
    exit $?
fi

if [ "${1:-}" = "--self-test" ]; then
    ROOT="$(mktemp -d -t wired-demobad-self-XXXXXX 2>/dev/null || mktemp -d)"
    trap 'rm -rf "$ROOT"' EXIT
    write_fixture "$ROOT/clean.jsonl" "$ROOT/broken.current.dm_75"
    analyze_contract "$ROOT/clean.jsonl" "$ROOT/broken.current.dm_75" >/dev/null || exit 1
    defects=(missing-error duplicate-error wrong-reason missing-popup retry-visible missing-back-focus missing-clear first-frame network active-action nextdemo extra-down wrong-row stale-selection wrong-selection duplicate-queue wrong-queue wrong-file bad-envelope)
    for defect in "${defects[@]}"; do
        python3 - "$ROOT/clean.jsonl" "$ROOT/$defect.jsonl" "$defect" <<'PYEOF'
import json,sys
src,dst,mode=sys.argv[1:]; rows=[json.loads(x) for x in open(src) if x.strip()]
def find(text): return next(i for i,r in enumerate(rows) if text in r["msg"])
if mode=="missing-error": rows.pop(find("Error: The demo contains"))
elif mode=="duplicate-error": rows.insert(find("Error: The demo contains"),dict(rows[find("Error: The demo contains")]))
elif mode=="wrong-reason": rows[find("reason=oversize")]["msg"]=rows[find("reason=oversize")]["msg"].replace("oversize","truncated-payload")
elif mode=="missing-popup": rows.pop(find("push menu 'error_popup'"))
elif mode=="retry-visible": rows[find('text:  "0"')]["msg"]='  text:  "1"\n'
elif mode=="missing-back-focus": rows.pop(find("focused item 'btn_back'"))
elif mode=="missing-clear": rows[find('"com_errorMessage" is:""')]["msg"]='"com_errorMessage" is:"still live"\n'
elif mode=="first-frame": rows.append({"sev":"INFO","cat":"client","msg":"FIRST GAMEPLAY FRAME mapname=arena7 numEntities=1\n"})
elif mode=="network": rows.append({"sev":"INFO","cat":"network","msg":"QUIC client: TLV ACCEPT\n"})
elif mode=="active-action": rows.append({"sev":"INFO","cat":"system","msg":"Q0_BAD_MALFORMED_ACTIVEACTION\n"})
elif mode=="nextdemo": rows.append({"sev":"INFO","cat":"client","msg":"CL_NextDemo: exec bad.cfg\n"})
elif mode=="extra-down": rows.insert(find("push menu 'demos'"),{"sev":"DEBUG","cat":"ui","msg":"wui_menu_nav: K_DOWNARROW dispatched\n"})
elif mode=="wrong-row": rows.insert(find("demo feeder row=0"),{"sev":"DEBUG","cat":"ui","msg":"WiredUI: demo feeder row=1 name=other generation=2\n"})
elif mode=="stale-selection": rows.insert(find("demo feeder selection"),{"sev":"DEBUG","cat":"ui","msg":"WiredUI: demo feeder selection row=0 name=broken.current generation=1\n"})
elif mode=="wrong-selection": rows.insert(find("demo feeder selection"),{"sev":"DEBUG","cat":"ui","msg":"WiredUI: demo feeder selection row=0 name=other generation=2\n"})
elif mode=="duplicate-queue": rows.insert(find("queued validated demo"),dict(rows[find("queued validated demo")]))
elif mode=="wrong-queue": rows.insert(find("queued validated demo"),{"sev":"DEBUG","cat":"ui","msg":"WiredUI: queued validated demo playback name=other\n"})
elif mode=="wrong-file": rows[find("Demo file:")]["msg"]="Demo file: demos/broken.current.dm_75.tmp\n"
with open(dst,"w") as f:
    for row in rows: f.write(json.dumps(row)+"\n")
PYEOF
        demo="$ROOT/broken.current.dm_75"
        if [ "$defect" = bad-envelope ]; then printf '\000' >"$ROOT/bad.dm_75"; demo="$ROOT/bad.dm_75"; fi
        if analyze_contract "$ROOT/$defect.jsonl" "$demo" >/dev/null 2>&1; then
            echo "FAIL: analyzer accepted defect $defect"; exit 1
        fi
    done
    cat >"$ROOT/attract.jsonl" <<'EOF'
{"sev":"INFO","cat":"client","msg":"Demo file: demos/broken.current.dm_75\n"}
{"sev":"INFO","cat":"client","msg":"Demo playback rejected reason=oversize continuation=1\n"}
{"sev":"ERROR","cat":"system","msg":"Error: The demo contains a message larger than the protocol limit.\n"}
{"sev":"INFO","cat":"ui","msg":"attract_status:\n"}
{"sev":"INFO","cat":"ui","msg":"  state         : PLAYING\n"}
{"sev":"INFO","cat":"ui","msg":"  currentIndex  : 1\n"}
{"sev":"INFO","cat":"ui","msg":"  ownsDemo      : 0 (demoplaying=0)\n"}
{"sev":"INFO","cat":"ui","msg":"  pushedPanel   : attract_brand\n"}
{"sev":"INFO","cat":"system","msg":"\"com_errorMessage\" is:\"\"\n"}
{"sev":"INFO","cat":"system","msg":"Q0_MALFORMED_ATTRACT_COMPLETE\n"}
EOF
    analyze_attract_contract "$ROOT/attract.jsonl" >/dev/null || exit 1
    for defect in silent-stuck retained-owner forged-index wrong-error-category retained-error popup; do
        cp "$ROOT/attract.jsonl" "$ROOT/attract-$defect.jsonl"
        case "$defect" in
        silent-stuck) sed -i.bak 's/continuation=1/continuation=0/' "$ROOT/attract-$defect.jsonl";;
        retained-owner) sed -i.bak 's/ownsDemo      : 0/ownsDemo      : 1/' "$ROOT/attract-$defect.jsonl";;
        forged-index) sed -i.bak 's/pushedPanel   : attract_brand/pushedPanel   : none/' "$ROOT/attract-$defect.jsonl";;
        wrong-error-category) sed -i.bak 's/"cat":"system","msg":"Error:/"cat":"client","msg":"Error:/' "$ROOT/attract-$defect.jsonl";;
        retained-error) sed -i.bak 's/\\"com_errorMessage\\" is:\\"\\"/\\"com_errorMessage\\" is:\\"still live\\"/' "$ROOT/attract-$defect.jsonl";;
        popup) printf '%s\n' '{"sev":"DEBUG","cat":"ui","msg":"WiredUI: push menu '\''error_popup'\'' (depth 2)\\n"}' >>"$ROOT/attract-$defect.jsonl";;
        esac
        if analyze_attract_contract "$ROOT/attract-$defect.jsonl" >/dev/null 2>&1; then
            echo "FAIL: attract analyzer accepted defect $defect"; exit 1
        fi
    done
    mkdir -p "$ROOT/matrix"
    write_matrix_fixture "$ROOT/matrix-clean.jsonl" "$ROOT/matrix"
    analyze_matrix_contract "$ROOT/matrix-clean.jsonl" "$ROOT/matrix" >/dev/null || exit 1
    matrix_defects=(missing-case wrong-status missing-empty-reject wrong-empty-status missing-semantic-reject wrong-semantic-status missing-semantic-marker duplicate-semantic-marker wrong-semantic-command semantic-generic-teardown semantic-marker-suffix missing-snapshot-marker wrong-snapshot-command wrong-snapshot-detail wrong-snapshot-areabytes snapshot-generic-teardown snapshot-generic-error snapshot-publication snapshot-marker-cr-smuggle generic-illegible additive-info-error additive-warn-error duplicate-queue wrong-generation extra-selection wrong-queue missing-popup retained-error first-frame network bad-fixture bad-empty-fixture bad-semantic-fixture bad-snapshot-fixture forged-file error-suffix enter-suffix enter-additive down-suffix down-additive focus-drift missing-dismiss-enter duplicate-recovery retry-visible additive-retry-visible reload-after-popup extra-generation wrong-count-extra-generation extra-generation-row wrong-depth-popup variant-closeall wrong-depth-pop wrong-depth-main wrong-depth-demos)
    for defect in "${matrix_defects[@]}"; do
        cp "$ROOT/matrix-clean.jsonl" "$ROOT/matrix-$defect.jsonl"
        python3 - "$ROOT/matrix-$defect.jsonl" "$defect" <<'PYEOF'
import json,sys
path,mode=sys.argv[1:]
rows=[json.loads(x) for x in open(path) if x.strip()]
def find(text): return next(i for i,r in enumerate(rows) if text in r["msg"])
if mode=="missing-case": rows.pop(find("Q0_MALFORMED_MATRIX_CASE_3"))
elif mode=="wrong-status":
    i=find("reason=invalid-length continuation=0")
    rows[i]["msg"]=rows[i]["msg"].replace("invalid-length","oversize")
elif mode=="missing-empty-reject": rows.pop(find("Demo playback rejected reason=empty-payload"))
elif mode=="wrong-empty-status":
    i=find("Demo playback rejected reason=empty-payload")
    rows[i]["msg"]=rows[i]["msg"].replace("empty-payload","truncated-payload")
elif mode=="missing-semantic-reject": rows.pop(find("Demo playback rejected reason=semantic-payload"))
elif mode=="wrong-semantic-status":
    i=find("Demo playback rejected reason=semantic-payload")
    rows[i]["msg"]=rows[i]["msg"].replace("semantic-payload","invalid-length")
elif mode=="missing-semantic-marker": rows.pop(find("Demo semantic parse recovered command="))
elif mode=="duplicate-semantic-marker":
    i=find("Demo semantic parse recovered command=")
    rows.insert(i,dict(rows[i]))
elif mode=="wrong-semantic-command":
    i=find("Demo semantic parse recovered command=")
    rows[i]["msg"]=rows[i]["msg"].replace("command=255","command=254")
elif mode=="semantic-generic-teardown":
    i=find("Demo semantic parse recovered command=")
    rows[i]["msg"]=rows[i]["msg"].replace("generic_teardown=0","generic_teardown=1")
elif mode=="semantic-marker-suffix":
    i=find("Demo semantic parse recovered command=")
    rows[i]["msg"]=rows[i]["msg"].rstrip("\n")+" EXTRA\n"
elif mode=="missing-snapshot-marker": rows.pop(find("detail=snapshot-areamask"))
elif mode=="wrong-snapshot-command":
    i=find("detail=snapshot-areamask")
    rows[i]["msg"]=rows[i]["msg"].replace("command=7","command=8")
elif mode=="wrong-snapshot-detail":
    i=find("detail=snapshot-areamask")
    rows[i]["msg"]=rows[i]["msg"].replace("snapshot-areamask","snapshot-other")
elif mode=="wrong-snapshot-areabytes":
    i=find("detail=snapshot-areamask")
    rows[i]["msg"]=rows[i]["msg"].replace("areabytes=255","areabytes=33")
elif mode=="snapshot-generic-teardown":
    i=find("detail=snapshot-areamask")
    rows[i]["msg"]=rows[i]["msg"].replace("generic_teardown=0","generic_teardown=1")
elif mode=="snapshot-generic-error":
    i=find("detail=snapshot-areamask")
    rows.insert(i+1,{"sev":"ERROR","cat":"system","msg":"CL_ParseSnapshot: Invalid size 255 for areamask\n"})
elif mode=="snapshot-publication":
    i=find("detail=snapshot-areamask")
    rows.insert(i+1,{"sev":"DEBUG","cat":"client","msg":"snapshot committed message=1\n"})
elif mode=="snapshot-marker-cr-smuggle":
    i=find("detail=snapshot-areamask")
    rows.insert(i,{"sev":"DEBUG","cat":"client","msg":"benign\r"+rows[i]["msg"]})
elif mode=="generic-illegible":
    i=find("Demo playback rejected reason=semantic-payload")
    rows.insert(i,{"sev":"ERROR","cat":"system","msg":"CL_ParseServerMessage: Illegible server message\n"})
elif mode in ("additive-info-error","additive-warn-error"):
    i=find("Error: The demo contains an invalid server message.")
    rows.insert(i+1,{"sev":"INFO" if mode=="additive-info-error" else "WARN",
                     "cat":"system","msg":"Error: The demo contains an invalid server message.\n"})
elif mode=="duplicate-queue":
    i=find("queued validated demo playback name=03.badterm")
    rows.insert(i,dict(rows[i]))
elif mode=="wrong-generation":
    i=find("count=10 generation=5")
    rows[i]["msg"]=rows[i]["msg"].replace("generation=5","generation=6")
elif mode=="extra-selection":
    i=find("demo feeder selection row=3 name=03.badterm generation=5")
    rows.insert(i,{"sev":"DEBUG","cat":"ui","msg":"WiredUI: demo feeder selection row=6 name=06.shortpayload generation=5\n"})
elif mode=="wrong-queue":
    i=find("queued validated demo playback name=02.shortlen")
    rows[i]["msg"]="WiredUI: queued validated demo playback name=other\n"
elif mode=="missing-popup": rows.pop(find("Demo playback recovery reason=truncated-length"))
elif mode=="retained-error":
    indices=[i for i,r in enumerate(rows) if r["msg"]=='"com_errorMessage" is:""']
    rows[indices[4]]["msg"]='"com_errorMessage" is:"still live"'
elif mode=="first-frame": rows.append({"sev":"INFO","cat":"client","msg":"FIRST GAMEPLAY FRAME mapname=arena7 numEntities=1\n"})
elif mode=="network": rows.append({"sev":"INFO","cat":"network","msg":"QUIC client: TLV ACCEPT\n"})
elif mode=="forged-file":
    i=find("Demo file: demos/00.missing.dm_75")
    rows.insert(i+1,{"sev":"INFO","cat":"client","msg":"Demo file: demos/forged.dm_75\n"})
elif mode=="error-suffix":
    i=find("Error: The demo ended without its required terminator.")
    rows[i]["msg"]=rows[i]["msg"].rstrip("\n")+" EXTRA\n"
elif mode=="enter-suffix":
    i=find("wui_menu_nav: K_ENTER dispatched")
    rows[i]["msg"]="wui_menu_nav: K_ENTER dispatched EXTRA\n"
elif mode=="enter-additive":
    i=find("wui_menu_nav: K_ENTER dispatched")
    rows.insert(i,{"sev":"DEBUG","cat":"ui","msg":"wui_menu_nav: K_ENTER dispatched\n"})
elif mode=="down-suffix":
    i=find("wui_menu_nav: K_DOWNARROW dispatched")
    rows[i]["msg"]="wui_menu_nav: K_DOWNARROW dispatched EXTRA\n"
elif mode=="down-additive":
    i=find("wui_menu_nav: K_DOWNARROW dispatched")
    rows.insert(i,{"sev":"DEBUG","cat":"ui","msg":"wui_menu_nav: K_DOWNARROW dispatched\n"})
elif mode=="focus-drift":
    i=find("focused item 'btn_back'")
    rows.insert(i+1,{"sev":"DEBUG","cat":"ui","msg":"wui_menu_nav focus: focused item 'demolist' (top index -1)\n"})
elif mode=="missing-dismiss-enter":
    i=find("WiredUI: pop menu (depth 2)")
    rows.pop(i+1)
elif mode=="duplicate-recovery":
    i=find("Demo playback recovery reason=oversize")
    rows.insert(i,dict(rows[i]))
elif mode=="retry-visible":
    i=find('text:  "0"')
    rows[i]["msg"]='  text:  "1"'
elif mode=="additive-retry-visible":
    i=find('text:  "0"')
    rows.insert(i,{"sev":"INFO","cat":"client","msg":"  text:  \"1\""})
elif mode=="reload-after-popup":
    start=find("demos loaded protocol=74 count=10 generation=3")
    end=start+1
    while end<len(rows) and rows[end]["msg"].startswith("WiredUI: demo feeder row="): end+=1
    block=rows[start:end]
    del rows[start:end]
    popup=find("push menu 'error_popup'")
    rows[popup+1:popup+1]=block
elif mode=="extra-generation":
    rows.append({"sev":"DEBUG","cat":"ui","msg":"WiredUI: demos loaded protocol=74 count=10 generation=99\n"})
elif mode=="wrong-count-extra-generation":
    rows.append({"sev":"DEBUG","cat":"ui","msg":"WiredUI: demos loaded protocol=74 count=9 generation=99\n"})
elif mode=="extra-generation-row":
    rows.append({"sev":"DEBUG","cat":"ui","msg":"WiredUI: demo feeder row=0 name=forged generation=99\n"})
elif mode=="wrong-depth-popup":
    i=find("push menu 'error_popup'")
    rows.insert(i,{"sev":"DEBUG","cat":"ui","msg":"WiredUI: push menu 'error_popup' (depth 2)\n"})
elif mode=="variant-closeall":
    i=find("close all postcondition")
    rows.insert(i,{"sev":"DEBUG","cat":"ui","msg":"WiredUI: close all postcondition depth=0 active=none catcher_ui=0 paused=0 pointer_down=1\n"})
elif mode=="wrong-depth-pop":
    i=find("WiredUI: pop menu")
    rows.insert(i,{"sev":"DEBUG","cat":"ui","msg":"WiredUI: pop menu (depth 1)\n"})
elif mode=="wrong-depth-main":
    i=find("push menu 'main'")
    rows.insert(i,{"sev":"DEBUG","cat":"ui","msg":"WiredUI: push menu 'main' (depth 2)\n"})
elif mode=="wrong-depth-demos":
    i=find("push menu 'demos'")
    rows.insert(i,{"sev":"DEBUG","cat":"ui","msg":"WiredUI: push menu 'demos' (depth 1)\n"})
with open(path,"w") as f:
    for row in rows: f.write(json.dumps(row)+"\n")
PYEOF
        if [ "$defect" = bad-fixture ]; then printf '\001' >"$ROOT/matrix/00.missing.dm_75"; fi
        if [ "$defect" = bad-empty-fixture ]; then printf '\001' >>"$ROOT/matrix/07.emptypayload.dm_75"; fi
        if [ "$defect" = bad-semantic-fixture ]; then printf '\001' >>"$ROOT/matrix/08.badmessage.dm_75"; fi
        if [ "$defect" = bad-snapshot-fixture ]; then printf '\001' >>"$ROOT/matrix/09.badsnapshot.dm_75"; fi
        if analyze_matrix_contract "$ROOT/matrix-$defect.jsonl" "$ROOT/matrix" >/dev/null 2>&1; then
            echo "FAIL: matrix analyzer accepted defect $defect"; exit 1
        fi
        if [ "$defect" = bad-fixture ]; then : >"$ROOT/matrix/00.missing.dm_75"; fi
        if [ "$defect" = bad-empty-fixture ]; then python3 - "$ROOT/matrix/07.emptypayload.dm_75" <<'PYEOF2'
import sys
open(sys.argv[1],"wb").write(bytes.fromhex("0100000000000000ffffffffffffffff"))
PYEOF2
        fi
        if [ "$defect" = bad-semantic-fixture ]; then python3 - "$ROOT/matrix/08.badmessage.dm_75" <<'PYEOF2'
import sys
open(sys.argv[1],"wb").write(bytes.fromhex("0100000002000000aa24ffffffffffffffff"))
PYEOF2
        fi
        if [ "$defect" = bad-snapshot-fixture ]; then python3 - "$ROOT/matrix/09.badsnapshot.dm_75" <<'PYEOF2'
import sys
open(sys.argv[1],"wb").write(bytes.fromhex("0100000005000000aabfaa9200ffffffffffffffff"))
PYEOF2
        fi
    done
    echo "==> SELF-TEST PASS: malformed clean accepted; $((${#defects[@]}+6+${#matrix_defects[@]})) defects rejected"
    exit 0
fi

WIRED="${1:-}"
[ -n "$WIRED" ] && [ -x "$WIRED" ] || { echo "usage: $0 /absolute/path/to/wired"; exit 64; }
WIRED="$(cd "$(dirname "$WIRED")" && pwd)/$(basename "$WIRED")"
WIRED_DIR="$(dirname "$WIRED")"
PACK_ROOT="$(wired_find_archive_root "$WIRED_DIR" "$WIRED_DIR/../Resources" "$WIRED_DIR/../../.." 2>/dev/null || true)"
[ -n "$PACK_ROOT" ] || { echo "SKIP: current VFS archives not found"; exit 77; }
CONTENT_ROOT="$(wired_find_archive_root "${WIRED_CONTENT_ROOT:-}" "$WIRED_HOME" "$PACK_ROOT" 2>/dev/null || true)"
[ -n "$CONTENT_ROOT" ] || { echo "SKIP: set WIRED_CONTENT_ROOT"; exit 77; }
CURRENT_ARCHIVE="$(wired_first_archive "$PACK_ROOT/base")"; BASE_ARCHIVE="$(wired_first_archive "$CONTENT_ROOT/base")"

RUN_ROOT="$(mktemp -d -t wired-demobad-XXXXXX 2>/dev/null || mktemp -d)"
HOME_DIR="$RUN_ROOT/home/q3now-preview"
ATTRACT_HOME="$RUN_ROOT/attract/q3now-preview"
LOOP_HOME="$RUN_ROOT/loop/q3now-preview"
MATRIX_HOME="$RUN_ROOT/matrix/q3now-preview"
mkdir -p "$HOME_DIR/base/demos" "$ATTRACT_HOME/base/demos" "$ATTRACT_HOME/base/scripts" "$LOOP_HOME/base/demos" "$MATRIX_HOME/base/demos"
cleanup(){ if [ "${WIRED_KEEP_ARTIFACTS:-0}" = 1 ]; then echo "    kept artifacts: $RUN_ROOT"; else rm -rf "$RUN_ROOT"; fi; }
trap cleanup EXIT INT TERM
wired_link_content_into_home "$HOME_DIR" "$CONTENT_ROOT/base" "$PACK_ROOT/base" || exit 1
LIVE_CFG="wiredui-demo-malformed-live.cfg"
cp "$CFG_SOURCE" "$HOME_DIR/base/$LIVE_CFG" || exit 1
wired_link_content_into_home "$ATTRACT_HOME" "$CONTENT_ROOT/base" "$PACK_ROOT/base" || exit 1
wired_link_content_into_home "$LOOP_HOME" "$CONTENT_ROOT/base" "$PACK_ROOT/base" || exit 1
wired_link_content_into_home "$MATRIX_HOME" "$CONTENT_ROOT/base" "$PACK_ROOT/base" || exit 1
MATRIX_LIVE_CFG="wiredui-demo-malformed-matrix-live.cfg"
cp "$MATRIX_CFG_SOURCE" "$MATRIX_HOME/base/$MATRIX_LIVE_CFG" || exit 1
python3 - "$HOME_DIR/base/demos/broken.current.dm_75" <<'PYEOF'
import struct,sys
open(sys.argv[1],"wb").write(struct.pack("<ii",0x13579bdf,16385))
PYEOF
cp "$HOME_DIR/base/demos/broken.current.dm_75" "$ATTRACT_HOME/base/demos/broken.current.dm_75"
cp "$HOME_DIR/base/demos/broken.current.dm_75" "$LOOP_HOME/base/demos/broken.current.dm_75"
python3 - "$MATRIX_HOME/base/demos" <<'PYEOF'
import os,sys
root=sys.argv[1]
fixtures={
 "00.missing":b"",
 "01.shortseq":bytes.fromhex("0100"),
 "02.shortlen":bytes.fromhex("010000000400"),
 "03.badterm":bytes.fromhex("ffffffff00000000"),
 "04.neglen":bytes.fromhex("01000000feffffff"),
 "05.oversize":bytes.fromhex("df9b571301400000"),
 "06.shortpayload":bytes.fromhex("0100000004000000aabb"),
 "07.emptypayload":bytes.fromhex("0100000000000000ffffffffffffffff"),
 "08.badmessage":bytes.fromhex("0100000002000000aa24ffffffffffffffff"),
 "09.badsnapshot":bytes.fromhex("0100000005000000aabfaa9200ffffffffffffffff"),
}
for name,data in fixtures.items():
    open(os.path.join(root,name+".dm_75"),"wb").write(data)
PYEOF
cat >"$ATTRACT_HOME/base/q0-demo-malformed-attract.cfg" <<'EOF'
wait 100
attract_stop
lua_eval "attract.clear(); attract.add('demo','broken.current',0); attract.add('panel','attract_brand',6000); attract.set_loop(false); attract.set_transition(0)"
attract_restart
wait 100
wait 100
wait 100
attract_status
com_errorMessage
echo Q0_MALFORMED_ATTRACT_COMPLETE
wait 10
quit
EOF
cat >"$LOOP_HOME/base/q0-demo-malformed-loop.cfg" <<'EOF'
wait 100
attract_stop
lua_eval "attract.clear(); attract.add('demo','broken.current',0); attract.set_loop(true); attract.set_transition(0)"
attract_restart
wait 100
wait 100
wait 100
wait 100
attract_stop
attract_status
com_errorMessage
echo Q0_MALFORMED_LOOP_COMPLETE
wait 10
quit
EOF

case "$(uname -s)" in
Darwin) NATIVE_HOME="$HOME_DIR"; PLATFORM_ARGS=( -ApplePersistenceIgnoreState YES );;
MINGW*|MSYS*|CYGWIN*) NATIVE_HOME="$(cygpath -w "$HOME_DIR")"; PLATFORM_ARGS=();;
*) NATIVE_HOME="$HOME_DIR"; PLATFORM_ARGS=();;
esac
case "$(uname -s)" in MINGW*|MSYS*|CYGWIN*) ATTRACT_NATIVE="$(cygpath -w "$ATTRACT_HOME")";; *) ATTRACT_NATIVE="$ATTRACT_HOME";; esac
case "$(uname -s)" in MINGW*|MSYS*|CYGWIN*) LOOP_NATIVE="$(cygpath -w "$LOOP_HOME")";; *) LOOP_NATIVE="$LOOP_HOME";; esac
case "$(uname -s)" in MINGW*|MSYS*|CYGWIN*) MATRIX_NATIVE="$(cygpath -w "$MATRIX_HOME")";; *) MATRIX_NATIVE="$MATRIX_HOME";; esac

echo "==> Malformed current demo: exact oversize envelope and controlled Demos recovery"
python3 "$TIMEOUT_RUNNER" --timeout 180 --kill-after 15 --cwd "$RUN_ROOT/home" --stdout "$RUN_ROOT/wired.stdout" -- \
    "$WIRED" "${PLATFORM_ARGS[@]}" +set fs_homepath "$NATIVE_HOME" \
    +set com_automated 1 +set com_noHardReboot 1 +set s_initsound 0 \
    +set r_fullscreen 0 +set r_mode -1 +set r_customwidth 1280 +set r_customheight 720 \
    +set log_severity DEBUG +set log_file_severity DEBUG +set log_file_mode overwrite_synced \
    +exec "$LIVE_CFG"
[ "$?" -eq 0 ] || { echo "FAIL: malformed demo client did not exit cleanly"; exit 1; }
LOG="$HOME_DIR/qconsole.jsonl"; DEMO="$HOME_DIR/base/demos/broken.current.dm_75"
[ -s "$LOG" ] && [ -s "$DEMO" ] || { echo "FAIL: missing malformed demo evidence"; exit 1; }
analyze_contract "$LOG" "$DEMO" || exit 1

echo "==> Malformed current demo matrix: structural statuses + semantic payload recovery"
python3 "$TIMEOUT_RUNNER" --timeout 240 --kill-after 15 --cwd "$RUN_ROOT/matrix" --stdout "$RUN_ROOT/matrix.stdout" -- \
    "$WIRED" "${PLATFORM_ARGS[@]}" +set fs_homepath "$MATRIX_NATIVE" \
    +set com_automated 1 +set com_noHardReboot 1 +set s_initsound 0 \
    +set r_fullscreen 0 +set r_mode -1 +set r_customwidth 1280 +set r_customheight 720 \
    +set log_severity DEBUG +set log_file_severity DEBUG +set log_file_mode overwrite_synced \
    +exec "$MATRIX_LIVE_CFG"
[ "$?" -eq 0 ] || { echo "FAIL: malformed matrix client did not exit cleanly"; exit 1; }
MLOG="$MATRIX_HOME/qconsole.jsonl"; [ -s "$MLOG" ] || { echo "FAIL: missing malformed matrix evidence"; exit 1; }
analyze_matrix_contract "$MLOG" "$MATRIX_HOME/base/demos" || exit 1

echo "==> Malformed attract demo: owned STARTING failure advances to next playlist item"
python3 "$TIMEOUT_RUNNER" --timeout 180 --kill-after 15 --cwd "$RUN_ROOT/attract" --stdout "$RUN_ROOT/attract.stdout" -- \
    "$WIRED" "${PLATFORM_ARGS[@]}" +set fs_homepath "$ATTRACT_NATIVE" \
    +set com_automated 1 +set com_noHardReboot 1 +set s_initsound 0 \
    +set r_fullscreen 0 +set r_mode -1 +set r_customwidth 1280 +set r_customheight 720 \
    +set log_severity DEBUG +set log_file_severity DEBUG +set log_file_mode overwrite_synced \
    +exec q0-demo-malformed-attract.cfg
[ "$?" -eq 0 ] || { echo "FAIL: malformed attract client did not exit cleanly"; exit 1; }
ALOG="$ATTRACT_HOME/qconsole.jsonl"; [ -s "$ALOG" ] || { echo "FAIL: missing malformed attract evidence"; exit 1; }
analyze_attract_contract "$ALOG" || exit 1

echo "==> Malformed attract loop: zero-transition retries remain frame-bounded"
python3 "$TIMEOUT_RUNNER" --timeout 180 --kill-after 15 --cwd "$RUN_ROOT/loop" --stdout "$RUN_ROOT/loop.stdout" -- \
    "$WIRED" "${PLATFORM_ARGS[@]}" +set fs_homepath "$LOOP_NATIVE" \
    +set com_automated 1 +set com_noHardReboot 1 +set s_initsound 0 \
    +set r_fullscreen 0 +set r_mode -1 +set r_customwidth 1280 +set r_customheight 720 \
    +set log_severity DEBUG +set log_file_severity DEBUG +set log_file_mode overwrite_synced \
    +exec q0-demo-malformed-loop.cfg
[ "$?" -eq 0 ] || { echo "FAIL: malformed attract loop did not exit cleanly"; exit 1; }
LLOG="$LOOP_HOME/qconsole.jsonl"; [ -s "$LLOG" ] || { echo "FAIL: missing malformed attract loop evidence"; exit 1; }
analyze_loop_contract "$LLOG" || exit 1
python3 - "$WIRED" "$CURRENT_ARCHIVE" "$BASE_ARCHIVE" "$0" <<'PYEOF'
import hashlib,sys
for label,path in zip(("binary","product-archive","base-archive","harness"),sys.argv[1:]):
    print(f"    {label}_sha256={hashlib.sha256(open(path,'rb').read()).hexdigest()}")
PYEOF
echo "==> WiredUI Malformed Demo gate: PASS"
