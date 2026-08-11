#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Q0 WiredUI server-browser/status/action gate.  An automated-only two-row
# cache proves display-to-raw keyboard selection; a loopback proxy delays an
# actual status response across cancel/reopen.  A fresh pointer client and its
# own isolated unprotected headless prove Browser double-click semantics through
# real CL_MouseEvent + CL_KeyEvent ingress and arena7 admission; that server must
# shut down cleanly before a fresh protected headless starts.  A wrong-password
# client is rejected there before the final fresh client proves footer Connect
# -> masked password edit -> QUIC -> FIRST recovery.  The wrong client ends by
# bounded watchdog rc124 because game rejection leaves no product-owned quit.

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
TIMEOUT_RUNNER="$SCRIPT_DIR/run-with-timeout.py"
FIXTURE="$SCRIPT_DIR/wiredui-server-fixture.py"

analyze_doubleclick_contract() {
    python3 - "$1" "$2" "$3" "$4" "$5" <<'PYEOF'
import json
import re
import sys

product_path, layout_path, server_path, sentinel_port, server_port = sys.argv[1:6]
sentinel = f"127.0.0.1:{sentinel_port}"
target = f"127.0.0.1:{server_port}"

def json_rows(path, required):
    rows = []
    with open(path, encoding="utf-8", errors="strict") as stream:
        for line_no, line in enumerate(stream, 1):
            if not line.strip():
                continue
            try:
                row = json.loads(line)
            except ValueError as exc:
                raise SystemExit(f"FAIL doubleclick {path}:{line_no}: invalid JSON: {exc}")
            if not isinstance(row, dict) or any(not isinstance(row.get(key), kind)
                                                for key, kind in required):
                raise SystemExit(f"FAIL doubleclick {path}:{line_no}: malformed record")
            rows.append(row)
    if not rows:
        raise SystemExit(f"FAIL doubleclick: empty evidence {path}")
    return rows

product = json_rows(product_path, (("sev", str), ("cat", str), ("msg", str)))
server = json_rows(server_path, (("sev", str), ("cat", str), ("msg", str)))
layout = json_rows(layout_path, (("region", str), ("menu", str),
                                 ("frame", int), ("focused", int)))
for row in layout:
    for field in ("x", "y", "w", "h"):
        if not isinstance(row.get(field), (int, float)):
            raise SystemExit(f"FAIL doubleclick layout: {field} is not numeric")

bad = [row for row in product
       if row["sev"].upper() in {"ERROR", "FATAL"}
       or (row["sev"].upper() == "WARN" and row["cat"].lower() == "ui")]
if bad:
    raise SystemExit(f"FAIL doubleclick severity: {len(bad)} product record(s)")
if any(row["sev"].upper() in {"ERROR", "FATAL"} for row in server):
    raise SystemExit("FAIL doubleclick severity: server ERROR/FATAL")

messages = [row["msg"] for row in product]
server_messages = [row["msg"] for row in server]
e_sentinel = re.escape(sentinel)
e_target = re.escape(target)

def ordered(steps):
    cursor = 0
    for name, pattern in steps:
        for index in range(cursor, len(messages)):
            if re.search(pattern, messages[index]):
                cursor = index + 1
                break
        else:
            raise SystemExit(f"FAIL doubleclick contract: missing/out-of-order {name}")

phase_names = ("ENTER", "SINGLE", "MISSING_RELEASE", "TIMEOUT",
               "DIFFERENT_ROW", "LIFECYCLE", "GENERATION", "POSITIVE")
phase_ranges = {}
last_end = -1
for phase in phase_names:
    begins = [i for i, msg in enumerate(messages)
              if msg.strip() == f"Q0_DBL_{phase}_BEGIN"]
    ends = [i for i, msg in enumerate(messages)
            if msg.strip() == f"Q0_DBL_{phase}_END"]
    if len(begins) != 1 or len(ends) != 1 or not (last_end < begins[0] < ends[0]):
        raise SystemExit(
            f"FAIL doubleclick phases: invalid {phase} begin/end {begins}/{ends}")
    phase_ranges[phase] = (begins[0], ends[0])
    last_end = ends[0]

for phase in phase_names[:-1]:
    begin, end = phase_ranges[phase]
    phase_messages = messages[begin:end + 1]
    if any("listbox click phase=double" in msg
           or "queued validated connect origin=browser" in msg
           or "close all postcondition" in msg for msg in phase_messages):
        raise SystemExit(f"FAIL doubleclick negative {phase}: action escaped phase")

enter_messages = messages[phase_ranges["ENTER"][0]:phase_ranges["ENTER"][1] + 1]
if sum(re.search(r"WiredUI: listbox activate input=K_ENTER menu=servers item=serverlist feeder=2 row=0$", msg) is not None
       for msg in enter_messages) != 2:
    raise SystemExit("FAIL doubleclick Enter: expected two authoritative keyboard activations")
if any("server selection display_row=" in msg for msg in enter_messages) \
        or any("listbox click phase=armed" in msg for msg in enter_messages):
    raise SystemExit("FAIL doubleclick Enter: keyboard activation used cursor/feeder authority")
generation_messages = messages[phase_ranges["GENERATION"][0]:phase_ranges["GENERATION"][1] + 1]
generation_moves = [int(match.group(1)) for msg in generation_messages
                    if (match := re.search(
                        r"pointer phase=moved .* row=1 list_generation=([1-9][0-9]*)", msg))]
if len(generation_moves) != 2 or generation_moves[0] == generation_moves[1]:
    raise SystemExit(
        f"FAIL doubleclick generation: same-count rebuild did not change identity {generation_moves}")

ordered([
    ("fixture", rf"WiredUI: server fixture installed sentinel={e_sentinel} target={e_target} raw_order=target,sentinel"),
    ("servers", r"WiredUI: push menu 'servers' \(depth 1\)"),
    ("sorted sentinel", rf"WiredUI: server roster row=0 raw=[01] address={e_sentinel} name=A0 WIRED Q0 SENTINEL map=arena1"),
    ("sorted target", rf"WiredUI: server roster row=1 raw=[01] address={e_target} name=Z0 WIRED Q0 TARGET map=arena7"),
    ("Enter begin", r"^Q0_DBL_ENTER_BEGIN$"),
    ("Enter one", r"WiredUI: listbox activate input=K_ENTER menu=servers item=serverlist feeder=2 row=0"),
    ("Enter two", r"WiredUI: listbox activate input=K_ENTER menu=servers item=serverlist feeder=2 row=0"),
    ("Enter end", r"^Q0_DBL_ENTER_END$"),
    ("single begin", r"^Q0_DBL_SINGLE_BEGIN$"),
    ("single move", r"WiredUI: pointer phase=moved ingress=CL_MouseEvent menu=servers item=serverlist feeder=2 row=1 list_generation=[1-9][0-9]* x=[0-9]+ y=[0-9]+"),
    ("single click", r"WiredUI: pointer phase=click ingress=CL_KeyEvent key=K_MOUSE1"),
    ("single callback", rf"WiredUI: server selection display_row=1 .* address={e_target} name=Z0 WIRED Q0 TARGET map=arena7"),
    ("single arm", r"WiredUI: listbox click phase=armed input=K_MOUSE1 menu=servers item=serverlist feeder=2 row=1 "),
    ("single release", r"WiredUI: listbox click phase=release input=K_MOUSE1 menu=servers item=serverlist feeder=2 row=1 .* pointer_down=0"),
    ("single end", r"^Q0_DBL_SINGLE_END$"),
    ("single lifecycle reset", r"WiredUI: listbox click phase=reset reason=push .* row=1 "),
    ("missing release begin", r"^Q0_DBL_MISSING_RELEASE_BEGIN$"),
    ("edge move", r"WiredUI: pointer phase=moved ingress=CL_MouseEvent menu=servers item=serverlist feeder=2 row=1 "),
    ("first down edge", r"WiredUI: pointer phase=button ingress=CL_KeyEvent key=K_MOUSE1 edge=down "),
    ("edge first arm", r"WiredUI: listbox click phase=armed input=K_MOUSE1 menu=servers item=serverlist feeder=2 row=1 "),
    ("second down edge", r"WiredUI: pointer phase=button ingress=CL_KeyEvent key=K_MOUSE1 edge=down "),
    ("missing release reset", r"WiredUI: listbox click phase=reset reason=missing-release .* row=1 "),
    ("edge second arm", r"WiredUI: listbox click phase=armed input=K_MOUSE1 menu=servers item=serverlist feeder=2 row=1 "),
    ("up edge", r"WiredUI: pointer phase=button ingress=CL_KeyEvent key=K_MOUSE1 edge=up "),
    ("edge release", r"WiredUI: listbox click phase=release input=K_MOUSE1 menu=servers item=serverlist feeder=2 row=1 .* pointer_down=0"),
    ("missing release end", r"^Q0_DBL_MISSING_RELEASE_END$"),
    ("edge lifecycle reset", r"WiredUI: listbox click phase=reset reason=push .* row=1 "),
    ("timeout begin", r"^Q0_DBL_TIMEOUT_BEGIN$"),
    ("timeout first arm", r"WiredUI: listbox click phase=armed input=K_MOUSE1 menu=servers item=serverlist feeder=2 row=1 "),
    ("timeout reset", r"WiredUI: listbox click phase=reset reason=timeout .* row=1 "),
    ("timeout second arm", r"WiredUI: listbox click phase=armed input=K_MOUSE1 menu=servers item=serverlist feeder=2 row=1 "),
    ("timeout end", r"^Q0_DBL_TIMEOUT_END$"),
    ("different row begin", r"^Q0_DBL_DIFFERENT_ROW_BEGIN$"),
    ("row0 arm", r"WiredUI: listbox click phase=armed input=K_MOUSE1 menu=servers item=serverlist feeder=2 row=0 "),
    ("different row reset", r"WiredUI: listbox click phase=reset reason=different-row .* row=0 "),
    ("row1 arm", r"WiredUI: listbox click phase=armed input=K_MOUSE1 menu=servers item=serverlist feeder=2 row=1 "),
    ("different row end", r"^Q0_DBL_DIFFERENT_ROW_END$"),
    ("lifecycle begin", r"^Q0_DBL_LIFECYCLE_BEGIN$"),
    ("lifecycle first arm", r"WiredUI: listbox click phase=armed input=K_MOUSE1 menu=servers item=serverlist feeder=2 row=1 "),
    ("pop reset", r"WiredUI: listbox click phase=reset reason=pop .* row=1 "),
    ("browser pop", r"WiredUI: pop menu \(depth 0\)"),
    ("browser reopen", r"WiredUI: push menu 'servers' \(depth 1\)"),
    ("lifecycle second arm", r"WiredUI: listbox click phase=armed input=K_MOUSE1 menu=servers item=serverlist feeder=2 row=1 "),
    ("lifecycle end", r"^Q0_DBL_LIFECYCLE_END$"),
    ("generation begin", r"^Q0_DBL_GENERATION_BEGIN$"),
    ("generation first arm", r"WiredUI: listbox click phase=armed input=K_MOUSE1 menu=servers item=serverlist feeder=2 row=1 "),
    ("roster reset", r"WiredUI: listbox click phase=reset reason=server-roster .* row=1 "),
    ("generation second arm", r"WiredUI: listbox click phase=armed input=K_MOUSE1 menu=servers item=serverlist feeder=2 row=1 "),
    ("generation end", r"^Q0_DBL_GENERATION_END$"),
    ("positive begin", r"^Q0_DBL_POSITIVE_BEGIN$"),
    ("positive double", r"WiredUI: listbox click phase=double input=K_MOUSE1 menu=servers item=serverlist feeder=2 row=1 "),
    ("positive queue", rf"WiredUI: queued validated connect origin=browser address={e_target}"),
    ("terminal pointer release", r"WiredUI: pointer phase=release reason=close-all was_down=1 pointer_down=0"),
    ("positive close", r"WiredUI: close all postcondition depth=0 active=none catcher_ui=0 paused=0"),
    ("positive end", r"^Q0_DBL_POSITIVE_END$"),
    ("positive resolution", rf"{e_target} resolved to {e_target}"),
    ("positive accept", r"QUIC client: TLV ACCEPT received"),
    ("positive FIRST", r"FIRST GAMEPLAY FRAME mapname=maps/arena7\.bsp\b"),
    ("shutdown", r"WiredUI: shutdown"),
])

if sum("listbox click phase=double" in msg for msg in messages) != 1:
    raise SystemExit("FAIL doubleclick: expected exactly one accepted gesture")
if sum("queued validated connect origin=browser" in msg for msg in messages) != 1:
    raise SystemExit("FAIL doubleclick: expected exactly one browser queue")
if sum("close all postcondition" in msg for msg in messages) != 1:
    raise SystemExit("FAIL doubleclick: expected exactly one CloseAll")
if sum("FIRST GAMEPLAY FRAME" in msg for msg in messages) != 1:
    raise SystemExit("FAIL doubleclick: expected exactly one FIRST")
if any("password required" in msg or "127.0.0.1:" + str(sentinel_port) + " resolved" in msg
       for msg in messages):
    raise SystemExit("FAIL doubleclick: protected/sentinel path contaminated unprotected gesture")

positive = messages[phase_ranges["POSITIVE"][0]:phase_ranges["POSITIVE"][1] + 1]
move_re = re.compile(r"pointer phase=moved ingress=CL_MouseEvent menu=servers item=serverlist feeder=2 row=1 list_generation=([1-9][0-9]*) x=([0-9]+) y=([0-9]+)")
callback_re = re.compile(rf"server selection display_row=1 raw=([01]) source=0 list_generation=([1-9][0-9]*) selection_generation=([1-9][0-9]*) address={e_target} name=Z0 WIRED Q0 TARGET map=arena7")
armed_re = re.compile(r"listbox click phase=armed input=K_MOUSE1 menu=servers item=serverlist feeder=2 row=1 raw=([01]) source=0 list_generation=([1-9][0-9]*) selection_generation=([1-9][0-9]*)")
double_re = re.compile(r"listbox click phase=double input=K_MOUSE1 menu=servers item=serverlist feeder=2 row=1 raw=([01]) source=0 list_generation=([1-9][0-9]*) selection_generation=([1-9][0-9]*) elapsed=([0-9]+)")
moves = [move_re.search(msg) for msg in positive if move_re.search(msg)]
callbacks = [callback_re.search(msg) for msg in positive if callback_re.search(msg)]
arms = [armed_re.search(msg) for msg in positive if armed_re.search(msg)]
doubles = [double_re.search(msg) for msg in positive if double_re.search(msg)]
if len(moves) != 2 or len(callbacks) != 2 or len(arms) != 1 or len(doubles) != 1:
    raise SystemExit(
        f"FAIL doubleclick positive cardinality moves={len(moves)} callbacks={len(callbacks)} arms={len(arms)} doubles={len(doubles)}")
if sum("pointer phase=click ingress=CL_KeyEvent key=K_MOUSE1" in msg for msg in positive) != 2:
    raise SystemExit("FAIL doubleclick positive: not exactly two paired pointer helpers")
if sum("listbox click phase=release input=K_MOUSE1" in msg for msg in positive) != 1:
    raise SystemExit("FAIL doubleclick positive: first click release proof missing/duplicated")
list_generation = moves[0].group(1)
if any(match.group(1) != list_generation for match in moves) \
        or any(match.group(2) != list_generation for match in callbacks) \
        or arms[0].group(2) != list_generation or doubles[0].group(2) != list_generation:
    raise SystemExit("FAIL doubleclick positive: click identity generation drifted")
if arms[0].group(1) != callbacks[0].group(1) \
        or doubles[0].group(1) != callbacks[1].group(1) \
        or arms[0].group(3) != callbacks[0].group(3) \
        or doubles[0].group(3) != callbacks[1].group(3):
    raise SystemExit("FAIL doubleclick positive: feeder callback identity not bound to latch")
if int(callbacks[1].group(3)) <= int(callbacks[0].group(3)) \
        or not 0 <= int(doubles[0].group(4)) < 300:
    raise SystemExit("FAIL doubleclick positive: selection epoch/elapsed contract broken")

serverlist = [row for row in layout
              if row["menu"] == "servers" and row["region"] == "serverlist"
              and row["w"] > 0 and row["h"] > 0]
if not serverlist:
    raise SystemExit("FAIL doubleclick layout: no positive serverlist rendered rect")
for match in moves:
    x, y = float(match.group(2)), float(match.group(3))
    if not any(row["x"] <= x <= row["x"] + row["w"]
               and row["y"] <= y <= row["y"] + row["h"] for row in serverlist):
        raise SystemExit("FAIL doubleclick layout: pointer target outside rendered serverlist")

if sum(msg.startswith("WiredNet: listening on port ") for msg in server_messages) != 1 \
        or any(msg.strip() in {"Q0_PASSWORD_SERVER_READY",
                               "Q0_WRONG_PASSWORD_PHASE_COMPLETE"}
               for msg in server_messages):
    raise SystemExit("FAIL doubleclick server: evidence is not one isolated unprotected process")
boundaries = [i for i, msg in enumerate(server_messages)
              if msg.strip() == "Q0_DOUBLECLICK_PHASE_COMPLETE"]
if len(boundaries) != 1:
    raise SystemExit(f"FAIL doubleclick server: boundary count {boundaries}")
segment = server_messages[:boundaries[0]]
shutdown = [i for i, msg in enumerate(server_messages)
            if msg.strip() == "----- Server Shutdown (Server quit) -----"]
transport_down = [i for i, msg in enumerate(server_messages)
                  if msg.strip() == "QUIC transport shut down."]
quit_requested = [i for i, msg in enumerate(server_messages)
                  if msg.strip() == "Q0_DOUBLECLICK_QUIT_REQUESTED"]
if len(quit_requested) != 1 or len(shutdown) != 1 or len(transport_down) != 1 \
        or not boundaries[0] < quit_requested[0] < shutdown[0] < transport_down[0]:
    raise SystemExit(
        "FAIL doubleclick server: boundary/console-quit/clean-shutdown order broken, "
        f"boundary={boundaries} quit={quit_requested} shutdown={shutdown} transport={transport_down}")
if sum(re.search(r"^g_needpass\s+0$", msg.strip()) is not None for msg in segment) != 1:
    raise SystemExit("FAIL doubleclick server: missing authoritative unprotected g_needpass=0")
conn = [i for i, msg in enumerate(segment) if "SV_OnPlayerConnect: conn=" in msg]
assigned = [(i, int(match.group(1))) for i, msg in enumerate(segment)
            if (match := re.search(r"SV_OnPlayerConnect: slot ([1-9][0-9]*) assigned to conn=", msg))]
if len(conn) != 1 or len(assigned) != 1:
    raise SystemExit(f"FAIL doubleclick server: conn/assignment counts {conn}/{assigned}")
assigned_index, slot = assigned[0]
connect = [i for i, msg in enumerate(segment) if re.search(rf"ClientConnect: {slot}\b", msg)]
begin = [i for i, msg in enumerate(segment) if re.search(rf"ClientBegin: {slot}\b", msg)]
if len(connect) != 1 or len(begin) != 1 \
        or not conn[0] < connect[0] < assigned_index < begin[0]:
    raise SystemExit("FAIL doubleclick server: post-VM admission incomplete/out of order")
if any("Invalid password" in msg or "game rejected connection" in msg for msg in segment):
    raise SystemExit("FAIL doubleclick server: unprotected admission rejected")

print("  PASS doubleclick negatives: Enter/single/missing-release/timeout/row/lifecycle/generation queue nothing")
print("  PASS doubleclick positive: rendered pointer -> paired mouse clicks -> authored action -> arena7 FIRST")
PYEOF
}

analyze_contract() {
    python3 - "$1" "$2" "$3" "$4" "$5" "$6" "$7" "$8" <<'PYEOF'
import json
import re
import sys

product_path, fixture_path, server_path, wrong_path, layout_path, sentinel_port, target_port, server_port = sys.argv[1:9]
sentinel = f"127.0.0.1:{sentinel_port}"
target = f"127.0.0.1:{target_port}"
connect_target = f"127.0.0.1:{server_port}"
password_secrets = ("q0Wrong", "q0Pass7")

def objects(path, required):
    rows = []
    with open(path, encoding="utf-8", errors="replace") as stream:
        for line_no, line in enumerate(stream, 1):
            if not line.strip():
                continue
            try:
                row = json.loads(line)
            except ValueError as exc:
                raise SystemExit(f"FAIL {path}:{line_no}: invalid JSON: {exc}")
            if not isinstance(row, dict):
                raise SystemExit(f"FAIL {path}:{line_no}: JSON record is not an object")
            for field in required:
                if not isinstance(row.get(field), str):
                    raise SystemExit(f"FAIL {path}:{line_no}: {field} is not a string")
            rows.append(row)
    if not rows:
        raise SystemExit(f"FAIL: empty evidence {path}")
    return rows

product = objects(product_path, ("sev", "cat", "msg"))
fixture = objects(fixture_path, ("event",))
server = objects(server_path, ("sev", "cat", "msg"))
wrong = objects(wrong_path, ("sev", "cat", "msg"))

layout = []
with open(layout_path, encoding="utf-8", errors="strict") as stream:
    for line_no, line in enumerate(stream, 1):
        if not line.strip():
            continue
        try:
            row = json.loads(line)
        except ValueError as exc:
            raise SystemExit(f"FAIL {layout_path}:{line_no}: invalid layout JSON: {exc}")
        if not isinstance(row, dict) or not isinstance(row.get("region"), str) \
                or not isinstance(row.get("menu"), str) \
                or not isinstance(row.get("frame"), int) \
                or not isinstance(row.get("focused"), int):
            raise SystemExit(f"FAIL {layout_path}:{line_no}: malformed layout record")
        for field in ("x", "y", "w", "h"):
            if not isinstance(row.get(field), (int, float)):
                raise SystemExit(f"FAIL {layout_path}:{line_no}: layout {field} is not numeric")
        layout.append(row)
if not layout:
    raise SystemExit("FAIL: empty password layout evidence")

for evidence_path in (product_path, wrong_path, server_path, layout_path):
    with open(evidence_path, "rb") as stream:
        evidence = stream.read()
    for password_secret in password_secrets:
        if password_secret.encode("ascii") in evidence:
            raise SystemExit(f"FAIL secrecy: password bytes leaked into {evidence_path}")

lan_current = [row for row in fixture if row["event"] == "lan_scan_b_current_sent"]
if len(lan_current) != 1 or not isinstance(lan_current[0].get("address"), str):
    raise SystemExit("FAIL fixture: missing unique LAN B current-response address")
lan_address = lan_current[0]["address"]

bad = [row for row in product
       if row["sev"].upper() in {"ERROR", "FATAL"}
       or (row["sev"].upper() == "WARN" and row["cat"].lower() == "ui")]
if bad:
    raise SystemExit(f"FAIL: {len(bad)} unexpected ERROR/FATAL or cat=ui WARN record(s)")
wrong_bad = [row for row in wrong
             if row["sev"].upper() in {"ERROR", "FATAL"}
             or (row["sev"].upper() == "WARN" and row["cat"].lower() == "ui")]
if wrong_bad:
    raise SystemExit(f"FAIL: {len(wrong_bad)} wrong-password ERROR/FATAL or cat=ui WARN record(s)")
server_bad = [row for row in server if row["sev"].upper() in {"ERROR", "FATAL"}]
if server_bad:
    raise SystemExit(f"FAIL: {len(server_bad)} server ERROR/FATAL record(s)")

messages = [row["msg"] for row in product]
wrong_messages = [row["msg"] for row in wrong]

def ordered(steps):
    cursor = 0
    for name, pattern in steps:
        for index in range(cursor, len(messages)):
            if re.search(pattern, messages[index]):
                cursor = index + 1
                break
        else:
            raise SystemExit(f"FAIL contract: missing/out-of-order step: {name}")

def ordered_wrong(steps):
    cursor = 0
    for name, pattern in steps:
        for index in range(cursor, len(wrong_messages)):
            if re.search(pattern, wrong_messages[index]):
                cursor = index + 1
                break
        else:
            raise SystemExit(f"FAIL wrong-password contract: missing/out-of-order step: {name}")

e_sentinel = re.escape(sentinel)
e_target = re.escape(target)
e_connect = re.escape(connect_target)
e_lan = re.escape(lan_address)
ordered_wrong([
    ("wrong-password browser", r"WiredUI: push menu 'servers' \(depth 1\)"),
    ("wrong-password protected fixture",
     rf"WiredUI: server fixture installed sentinel={e_sentinel} target={e_connect} target_needpass=1 raw_order=target,sentinel"),
    ("wrong-password list focus", r"wui_menu_nav focus: focused item 'serverlist'"),
    ("wrong-password target callback",
     rf"WiredUI: server selection display_row=1 .* address={e_connect} name=Z0 WIRED Q0 TARGET map=arena7"),
    ("wrong-password real Down", r"wui_menu_nav: K_DOWNARROW dispatched"),
    ("wrong-password footer focus", r"wui_menu_nav focus: focused item 'btn_connect'"),
    ("wrong-password preflight",
     rf"WiredUI: password required origin=browser address={e_connect} selection_generation=[0-9]+"),
    ("wrong-password popup", r"WiredUI: push menu 'password' \(depth 2\)"),
    ("wrong-password footer Enter", r"wui_menu_nav: K_ENTER dispatched"),
    ("wrong-password edit focus", r"wui_menu_nav focus: focused item 'row_password'"),
    ("wrong-password edit mode", r"wui_menu_nav: K_ENTER dispatched"),
    ("wrong-password typed", r"wui_menu_nav: typed 7 printable character\(s\)"),
    ("wrong-password edit commit", r"wui_menu_nav: K_ENTER dispatched"),
    ("wrong-password masked render", r"WiredUI: password render trace length=7 masked=1"),
    ("wrong-password submit focus", r"wui_menu_nav focus: focused item 'btn_connect'"),
    ("wrong-password accepted by UI validator",
     rf"WiredUI: password submit accepted address={e_connect} selection_generation=[0-9]+"),
    ("wrong-password canonical queue",
     rf"WiredUI: queued validated connect origin=browser address={e_connect}"),
    ("wrong-password CloseAll",
     r"WiredUI: close all postcondition depth=0 active=none catcher_ui=0 paused=0"),
    ("wrong-password submit Enter", r"wui_menu_nav: K_ENTER dispatched"),
    ("wrong-password resolution", rf"{e_connect} resolved to {e_connect}"),
    ("wrong-password transport accepted", r"QUIC client: TLV ACCEPT received"),
])
if sum("queued validated connect origin=browser" in msg for msg in wrong_messages) != 1:
    raise SystemExit("FAIL wrong-password: expected exactly one canonical UI queue")
if any("FIRST GAMEPLAY FRAME" in msg for msg in wrong_messages):
    raise SystemExit("FAIL wrong-password: invalid credential reached gameplay")
if any("reconnect" in msg.lower() for msg in wrong_messages) \
        or any(re.search(rf"{e_sentinel} resolved to {e_sentinel}", msg)
               for msg in wrong_messages):
    raise SystemExit("FAIL wrong-password: submit consumed reconnect state")

ordered([
    ("persisted stale state rejected", r"WiredUI: loaded 0 UI state entries \(ignored transient server selection 2\)"),
    ("transient selection reset", r"WiredUI: reset transient server selection after state load"),
    ("unsolicited LAN response rejected",
     r"Ignored unsolicited infoResponse from 127\.0\.0\.1:27960"),
    ("preselection browser", r"WiredUI: push menu 'servers' \(depth 1\)"),
    ("preselection connect focus", r"wui_menu_nav focus: focused item 'btn_connect'"),
    ("stale connect refused", r"WiredUI: browser connect refused without current selection"),
    ("preselection browser pop", r"WiredUI: pop menu \(depth 0\)"),
    ("LAN scan A browser", r"WiredUI: push menu 'servers' \(depth 1\)"),
    ("LAN scan A issued", r"Scanning for servers on the local network\.\.\."),
    ("LAN scan A wrong challenge rejected",
     rf"Ignored infoResponse challenge mismatch generation=[0-9]+ from {e_lan}"),
    ("LAN scan A malformed current rejected", rf"Rejected invalid infoResponse from {e_lan}"),
    ("LAN zero-row filter focus", r"wui_menu_nav focus: focused item 'btn_filter'"),
    ("LAN zero-row proof", r"WiredUI: server roster generation=[0-9]+ source=0 displayed=0 raw=0"),
    ("LAN zero-row filter activation", r"wui_menu_nav: K_ENTER dispatched"),
    ("LAN Refresh B focus", r"wui_menu_nav focus: focused item 'btn_refresh'"),
    ("LAN Refresh B activation", r"wui_menu_nav: K_ENTER dispatched"),
    ("LAN scan B issued", r"Scanning for servers on the local network\.\.\."),
    ("LAN late valid A rejected",
     rf"Ignored infoResponse challenge mismatch generation=[0-9]+ from {e_lan}"),
    ("LAN current B accepted",
     rf"Accepted local discovery infoResponse generation=[0-9]+ address={e_lan} rtt=[1-9][0-9]*ms name=LAN WIRED Q0 TARGET map=arena7 clients=1/8 gametype=0"),
    ("LAN one-row roster", r"WiredUI: server roster generation=[0-9]+ source=0 displayed=1 raw=1"),
    ("LAN populated row",
     rf"WiredUI: server roster row=0 raw=0 address={e_lan} name=LAN WIRED Q0 TARGET map=arena7"),
    ("LAN row callback",
     rf"WiredUI: server selection display_row=0 raw=0 source=0 .* address={e_lan} name=LAN WIRED Q0 TARGET map=arena7"),
    ("LAN list focus", r"wui_menu_nav focus: focused item 'serverlist'"),
    ("LAN real keyboard selection", r"wui_menu_nav: K_DOWNARROW dispatched"),
    ("LAN browser pop", r"WiredUI: pop menu \(depth 0\)"),
    ("fixture installed", r"WiredUI: server fixture installed "),
    ("servers menu", r"WiredUI: push menu 'servers' \(depth 1\)"),
    ("two-row roster", r"WiredUI: server roster generation=[0-9]+ source=0 displayed=2 raw=2"),
    ("sorted sentinel", rf"WiredUI: server roster row=0 raw=[01] address={e_sentinel} name=A0 WIRED Q0 SENTINEL map=arena1"),
    ("sorted target", rf"WiredUI: server roster row=1 raw=[01] address={e_target} name=Z0 WIRED Q0 TARGET map=arena7"),
    ("server list focus", r"wui_menu_nav focus: focused item 'serverlist'"),
    ("target callback", rf"WiredUI: server selection display_row=1 .* address={e_target} name=Z0 WIRED Q0 TARGET map=arena7"),
    ("real keyboard Down", r"wui_menu_nav: K_DOWNARROW dispatched"),
    ("info button focus", r"wui_menu_nav focus: focused item 'btn_info'"),
    ("first target status request", rf"WiredUI: server status request generation=[0-9]+ selection_generation=[0-9]+ address={e_target}"),
    ("first serverinfo push", r"WiredUI: push menu 'serverinfo' \(depth 2\)"),
    ("first Info activation", r"wui_menu_nav: K_ENTER dispatched"),
    ("first back focus", r"wui_menu_nav focus: focused item 'btn_back'"),
    ("first status cancel", r"WiredUI: server status cancelled generation=[0-9]+ rows=0"),
    ("first serverinfo pop", r"WiredUI: pop menu \(depth 1\)"),
    ("first collapsed browser return", r"WiredUI: push menu 'servers' collapsed \(already on top, depth 1\)"),
    ("second info button focus", r"wui_menu_nav focus: focused item 'btn_info'"),
    ("second target status request", rf"WiredUI: server status request generation=[0-9]+ selection_generation=[0-9]+ address={e_target}"),
    ("second serverinfo push", r"WiredUI: push menu 'serverinfo' \(depth 2\)"),
    ("second Info activation", r"wui_menu_nav: K_ENTER dispatched"),
    ("stale challenge rejected", rf"CL_ServerStatusResponse: ignored stale challenge from {e_target}"),
    ("current challenge accepted", rf"CL_ServerStatusResponse: stored response from {e_target}"),
    ("status loaded", rf"WiredUI: server status loaded .* address={e_target} rows=9"),
    ("registry count", r"WiredUI: server status registry feeder=13 count=9"),
    ("address registry row", rf"WiredUI: server status registry row=0 key=Address value={e_target}"),
    ("server registry row", r"WiredUI: server status registry row=1 key=Server value=Z0 WIRED Q0 TARGET"),
    ("map registry row", r"WiredUI: server status registry row=2 key=Map value=arena7"),
    ("players registry row", r"WiredUI: server status registry row=3 key=Players value=1/8"),
    ("game type registry row", r"WiredUI: server status registry row=4 key=Game type value=0"),
    ("game registry row", r"WiredUI: server status registry row=5 key=Game value=q3now"),
    ("protocol registry row", r"WiredUI: server status registry row=6 key=Protocol value=74"),
    ("version registry row", r"WiredUI: server status registry row=7 key=Version value=Wired 0\.80\.[0-9]+ [A-Za-z0-9_-]+ .+"),
    ("player registry row", r"WiredUI: server status registry row=8 key=Player 1 value=StatusBot . score 0, ping 0"),
    ("status list focus", r"wui_menu_nav focus: focused item 'statuslist'"),
    ("valid status back focus", r"wui_menu_nav focus: focused item 'btn_back'"),
    ("valid status cancel", r"WiredUI: server status cancelled generation=[0-9]+ rows=0"),
    ("valid serverinfo pop", r"WiredUI: pop menu \(depth 1\)"),
    ("valid collapsed browser return", r"WiredUI: push menu 'servers' collapsed \(already on top, depth 1\)"),
    ("timeout retry Info focus", r"wui_menu_nav focus: focused item 'btn_info'"),
    ("timeout request", rf"WiredUI: server status request generation=[0-9]+ selection_generation=[0-9]+ address={e_target}"),
    ("timeout pending state",
     rf"WiredUI: server status state=pending generation=[0-9]+ selection_generation=[0-9]+ address={e_target} rows=1"),
    ("timeout serverinfo push", r"WiredUI: push menu 'serverinfo' \(depth 2\)"),
    ("timeout Info activation", r"wui_menu_nav: K_ENTER dispatched"),
    ("timeout pending registry", r"WiredUI: server status registry feeder=13 count=1"),
    ("timeout pending row", r"WiredUI: server status registry row=0 key=Status value=Contacting server\.\.\."),
    ("timeout terminal",
     rf"WiredUI: server status failed generation=[0-9]+ selection_generation=[0-9]+ address={e_target} state=no-response reason=timeout rows=2"),
    ("timeout registry count", r"WiredUI: server status registry feeder=13 count=2"),
    ("timeout address row", rf"WiredUI: server status registry row=0 key=Address value={e_target}"),
    ("timeout status row", r"WiredUI: server status registry row=1 key=Status value=No response from server\."),
    ("timeout list focus", r"wui_menu_nav focus: focused item 'statuslist'"),
    ("timeout Retry focus", r"wui_menu_nav focus: focused item 'btn_retry'"),
    ("malformed retry request", rf"WiredUI: server status request generation=[0-9]+ selection_generation=[0-9]+ address={e_target}"),
    ("malformed retry pending state",
     rf"WiredUI: server status state=pending generation=[0-9]+ selection_generation=[0-9]+ address={e_target} rows=1"),
    ("malformed retry activation", r"wui_menu_nav: K_ENTER dispatched"),
    ("timeout-A stale response rejected", rf"CL_ServerStatusResponse: ignored stale challenge from {e_target}"),
    ("malformed-B response stored", rf"CL_ServerStatusResponse: stored response from {e_target}"),
    ("malformed terminal",
     rf"WiredUI: server status failed generation=[0-9]+ selection_generation=[0-9]+ address={e_target} state=malformed reason=invalid-sv_maxclients rows=2"),
    ("malformed registry count", r"WiredUI: server status registry feeder=13 count=2"),
    ("malformed address row", rf"WiredUI: server status registry row=0 key=Address value={e_target}"),
    ("malformed status row", r"WiredUI: server status registry row=1 key=Status value=Invalid server response\."),
    ("malformed list focus", r"wui_menu_nav focus: focused item 'statuslist'"),
    ("malformed Retry focus", r"wui_menu_nav focus: focused item 'btn_retry'"),
    ("valid retry request", rf"WiredUI: server status request generation=[0-9]+ selection_generation=[0-9]+ address={e_target}"),
    ("valid retry pending state",
     rf"WiredUI: server status state=pending generation=[0-9]+ selection_generation=[0-9]+ address={e_target} rows=1"),
    ("valid retry activation", r"wui_menu_nav: K_ENTER dispatched"),
    ("malformed-B stale response rejected", rf"CL_ServerStatusResponse: ignored stale challenge from {e_target}"),
    ("valid-C response stored", rf"CL_ServerStatusResponse: stored response from {e_target}"),
    ("valid retry loaded", rf"WiredUI: server status loaded .* address={e_target} rows=9"),
    ("valid retry registry count", r"WiredUI: server status registry feeder=13 count=9"),
    ("valid retry address", rf"WiredUI: server status registry row=0 key=Address value={e_target}"),
    ("valid retry server", r"WiredUI: server status registry row=1 key=Server value=Z0 WIRED Q0 TARGET"),
    ("valid retry map", r"WiredUI: server status registry row=2 key=Map value=arena7"),
    ("valid retry players", r"WiredUI: server status registry row=3 key=Players value=1/8"),
    ("valid retry game type", r"WiredUI: server status registry row=4 key=Game type value=0"),
    ("valid retry game", r"WiredUI: server status registry row=5 key=Game value=q3now"),
    ("valid retry protocol", r"WiredUI: server status registry row=6 key=Protocol value=74"),
    ("valid retry version", r"WiredUI: server status registry row=7 key=Version value=Wired 0\.80\.[0-9]+ [A-Za-z0-9_-]+ .+"),
    ("valid retry player", r"WiredUI: server status registry row=8 key=Player 1 value=StatusBot . score 0, ping 0"),
    ("valid retry list focus", r"wui_menu_nav focus: focused item 'statuslist'"),
    ("valid retry back focus", r"wui_menu_nav focus: focused item 'btn_back'"),
    ("valid retry cancel", r"WiredUI: server status cancelled generation=[0-9]+ rows=0"),
    ("valid retry serverinfo pop", r"WiredUI: pop menu \(depth 1\)"),
    ("collapsed browser return", r"WiredUI: push menu 'servers' collapsed \(already on top, depth 1\)"),
    ("returned browser boundary", r"wui_menu_nav focus: focused item 'btn_connect'"),
    ("connect roster", r"WiredUI: server roster generation=[0-9]+ source=0 displayed=2 raw=2"),
    ("connect sentinel row", rf"WiredUI: server roster row=0 raw=[01] address={e_sentinel} name=A0 WIRED Q0 SENTINEL map=arena1"),
    ("connect target row", rf"WiredUI: server roster row=1 raw=[01] address={e_connect} name=Z0 WIRED Q0 TARGET map=arena7"),
    ("password-protected connect cache installed",
     rf"WiredUI: server fixture installed sentinel={e_sentinel} target={e_connect} target_needpass=1 raw_order=target,sentinel"),
    ("connect list focus", r"wui_menu_nav focus: focused item 'serverlist'"),
    ("sentinel callback", rf"WiredUI: server selection display_row=0 .* address={e_sentinel} name=A0 WIRED Q0 SENTINEL map=arena1"),
    ("real keyboard Up", r"wui_menu_nav: K_UPARROW dispatched"),
    ("connect target callback", rf"WiredUI: server selection display_row=1 .* address={e_connect} name=Z0 WIRED Q0 TARGET map=arena7"),
    ("real keyboard Down to connect target", r"wui_menu_nav: K_DOWNARROW dispatched"),
    ("browser connect button focus", r"wui_menu_nav focus: focused item 'btn_connect'"),
    ("password preflight",
     rf"WiredUI: password required origin=browser address={e_connect} selection_generation=[0-9]+"),
    ("password popup", r"WiredUI: push menu 'password' \(depth 2\)"),
    ("browser Connect Enter returns", r"wui_menu_nav: K_ENTER dispatched"),
    ("empty password submit focus", r"wui_menu_nav focus: focused item 'btn_connect'"),
    ("empty password refused", r"WiredUI: password submit refused reason=invalid-credential"),
    ("empty password Enter returns", r"wui_menu_nav: K_ENTER dispatched"),
    ("password editfield focus", r"wui_menu_nav focus: focused item 'row_password'"),
    ("password edit mode", r"wui_menu_nav: K_ENTER dispatched"),
    ("password typed through real character path", r"wui_menu_nav: typed 7 printable character\(s\)"),
    ("password edit commit", r"wui_menu_nav: K_ENTER dispatched"),
    ("password render masking", r"WiredUI: password render trace length=7 masked=1"),
    ("password submit focus", r"wui_menu_nav focus: focused item 'btn_connect'"),
    ("password submit accepted",
     rf"WiredUI: password submit accepted address={e_connect} selection_generation=[0-9]+"),
    ("validated browser queue", rf"WiredUI: queued validated connect origin=browser address={e_connect}"),
    ("close all", r"WiredUI: close all postcondition depth=0 active=none catcher_ui=0 paused=0"),
    ("password submit Enter returns", r"wui_menu_nav: K_ENTER dispatched"),
    ("engine resolution", rf"{e_connect} resolved to {e_connect}"),
    ("QUIC accept", r"QUIC client: TLV ACCEPT received"),
    ("first gameplay", r"FIRST GAMEPLAY FRAME mapname=maps/arena7\.bsp\b"),
    ("clean UI shutdown", r"WiredUI: shutdown"),
])

password_rows = [row for row in layout if row["menu"] == "password"]
if not password_rows:
    raise SystemExit("FAIL layout: password popup never rendered")
for region in ("password", "password_root", "subheader", "row_password", "btn_connect"):
    matches = [row for row in password_rows if row["region"] == region]
    if not matches or any(row["w"] <= 0 or row["h"] <= 0 for row in matches):
        raise SystemExit(f"FAIL layout: missing/non-positive password region {region}")
frames = {}
for row in password_rows:
    frames.setdefault(row["frame"], []).append(row)
for frame, rows in frames.items():
    if sum(row["focused"] == 1 for row in rows) > 1:
        raise SystemExit(f"FAIL layout: multiple focused password items in frame {frame}")
row_focus_frames = [row["frame"] for row in password_rows
                    if row["region"] == "row_password" and row["focused"] == 1]
submit_focus_frames = [row["frame"] for row in password_rows
                       if row["region"] == "btn_connect" and row["focused"] == 1]
if not row_focus_frames or not submit_focus_frames \
        or min(row_focus_frames) >= max(submit_focus_frames):
    raise SystemExit("FAIL layout: password editfield focus did not precede submit focus")

if sum("queued validated connect origin=browser" in msg for msg in messages) != 1:
    raise SystemExit("FAIL: expected exactly one validated browser Connect queue")
if sum("password required origin=browser" in msg for msg in messages) != 1:
    raise SystemExit("FAIL: expected exactly one password preflight")
if sum("password submit refused reason=invalid-credential" in msg for msg in messages) != 1:
    raise SystemExit("FAIL: expected exactly one empty-password refusal")
if sum("password submit accepted address=" in msg for msg in messages) != 1:
    raise SystemExit("FAIL: expected exactly one password submit acceptance")
if sum(msg.strip() == "WiredUI: password render trace length=7 masked=1"
       for msg in messages) != 1:
    raise SystemExit("FAIL: expected exactly one masked password render trace")
preflight_match = re.search(
    rf"password required origin=browser address={e_connect} selection_generation=([0-9]+)",
    "\n".join(messages))
accepted_match = re.search(
    rf"password submit accepted address={e_connect} selection_generation=([0-9]+)",
    "\n".join(messages))
if not preflight_match or not accepted_match \
        or preflight_match.group(1) != accepted_match.group(1):
    raise SystemExit("FAIL: password submit was not bound to the preflight selection generation")
if any("reconnect" in msg.lower() for msg in messages):
    raise SystemExit("FAIL: password path used reconnect state")
if any(re.search(rf"{e_sentinel} resolved to {e_sentinel}", msg) for msg in messages):
    raise SystemExit("FAIL: password submit consumed the seeded reconnect sentinel")
if sum("QUIC client: TLV ACCEPT received" in msg for msg in messages) != 1:
    raise SystemExit("FAIL: expected exactly one client QUIC ACCEPT")
if sum("FIRST GAMEPLAY FRAME" in msg for msg in messages) != 1:
    raise SystemExit("FAIL: expected exactly one FIRST GAMEPLAY FRAME")
if sum("state=no-response reason=timeout rows=2" in msg for msg in messages) != 1:
    raise SystemExit("FAIL: expected exactly one timeout terminal")
if sum("state=malformed reason=invalid-sv_maxclients rows=2" in msg for msg in messages) != 1:
    raise SystemExit("FAIL: expected exactly one malformed terminal")
if sum("server status loaded" in msg and "rows=9" in msg for msg in messages) != 2:
    raise SystemExit("FAIL: expected initial and recovered nine-row status loads")
if sum(msg.startswith("Accepted local discovery infoResponse generation=")
       and "name=LAN WIRED Q0 TARGET" in msg
       for msg in messages) != 1:
    raise SystemExit("FAIL: LAN current B did not create exactly one discovered row")
if any(token in msg for msg in messages for token in (
        "name=UNSOLICITED PRE-SCAN", "name=WRONG CHALLENGE",
        "name=MALFORMED CURRENT A", "name=LATE VALID A")):
    raise SystemExit("FAIL: rejected LAN metadata reached product authority")
if sum("Ignored infoResponse challenge mismatch generation=" in msg
       and lan_address in msg for msg in messages) != 2:
    raise SystemExit("FAIL: expected wrong-A and late-A challenge rejection exactly once each")

events = [row["event"] for row in fixture]
lan_challenges = [row for row in fixture if row["event"] == "lan_scan_seen"]
if [row.get("ordinal") for row in lan_challenges] != [1, 2, 3]:
    raise SystemExit(f"FAIL fixture: expected warmup/A/B LAN scans, got {lan_challenges}")
lan_values = [row.get("challenge") for row in lan_challenges]
if any(not isinstance(value, str) or not value for value in lan_values) \
   or len(set(lan_values)) != 3:
    raise SystemExit("FAIL fixture: LAN scan challenges are missing or reused")
for event, count in (("lan_unsolicited_pre_scan", 1), ("lan_scan_a_missing", 1),
                     ("lan_scan_a_wrong_sent", 1), ("lan_scan_a_malformed_sent", 1),
                     ("lan_scan_b_late_a_sent", 1), ("lan_scan_b_current_sent", 1),
                     ("lan_scan_b_duplicate_sent", 1)):
    if events.count(event) != count:
        raise SystemExit(f"FAIL fixture: expected {event} exactly once")
if events.count("lan_warmup_missing") < 1 or "lan_scan_unexpected_ordinal" in events:
    raise SystemExit("FAIL fixture: LAN missing-response warmup or bounded scan sequence broken")
if "wrong_target_status" in events:
    raise SystemExit("FAIL fixture: product sent getstatus to decoy")
if events.count("disallowed_probe") != 1:
    raise SystemExit("FAIL fixture: disallowed OOB probe was not sent exactly once")
challenge_rows = [row for row in fixture if row["event"] == "challenge_seen"]
if [row.get("ordinal") for row in challenge_rows] != [1, 2, 3, 4, 5]:
    raise SystemExit(f"FAIL fixture: expected challenge ordinals 1..5, got {challenge_rows}")
challenge_values = [row.get("challenge") for row in challenge_rows]
if any(not isinstance(value, str) or not value for value in challenge_values) \
   or len(set(challenge_values)) != 5:
    raise SystemExit("FAIL fixture: status challenges are missing or reused")
timeout_requests = [row for row in fixture
                    if row["event"] == "timeout_request" and row.get("ordinal") == 3]
if len(timeout_requests) < 2 or any(row.get("challenge") != challenge_values[2]
                                    for row in timeout_requests):
    raise SystemExit("FAIL fixture: timeout A did not drop at least two same-challenge requests")
expected_fixture = [
    "ready", "disallowed_probe", "challenge_seen", "upstream_request",
    "held_stale_response", "challenge_seen", "upstream_request",
    "sent_stale_response", "sent_current_response", "challenge_seen",
    "held_timeout_response", "challenge_seen", "sent_timeout_stale_response",
    "sent_malformed_response", "challenge_seen", "upstream_retry_request",
    "sent_malformed_stale_response", "sent_retry_valid_response", "stopped",
]
cursor = 0
for event in events:
    if cursor < len(expected_fixture) and event == expected_fixture[cursor]:
        cursor += 1
if cursor != len(expected_fixture):
    raise SystemExit("FAIL fixture: stale/current proxy lifecycle incomplete or out of order")
server_messages = [row["msg"] for row in server]
if any(re.search(r"SV packet .* : rcon\b", msg) for msg in server_messages):
    raise SystemExit("FAIL network boundary: legacy rcon OOB reached server dispatch")
boundary_indices = [index for index, msg in enumerate(server_messages)
                    if msg.strip() == "Q0_WRONG_PASSWORD_PHASE_COMPLETE"]
if len(boundary_indices) != 1:
    raise SystemExit(
        f"FAIL server contract: expected one wrong/correct boundary, got {boundary_indices}")
boundary_index = boundary_indices[0]
wrong_server = server_messages[:boundary_index]
correct_server = server_messages[boundary_index + 1:]

arm_indices = [index for index, msg in enumerate(wrong_server)
               if msg.strip() == "Q0_PASSWORD_SERVER_READY"]
needpass_indices = [index for index, msg in enumerate(wrong_server)
                    if re.search(r"^g_needpass\s+1$", msg.strip())]
if len(arm_indices) != 1 or len(needpass_indices) != 1 \
        or needpass_indices[0] >= arm_indices[0]:
    raise SystemExit(
        "FAIL protected server: expected authoritative g_needpass=1 before readiness marker, "
        f"got needpass={needpass_indices} arm={arm_indices}")
wrong_conn = [index for index, msg in enumerate(wrong_server)
              if "SV_OnPlayerConnect: conn=" in msg]
wrong_reject = [index for index, msg in enumerate(wrong_server)
                if re.search(r"QUIC: game rejected connection .*: Invalid password$", msg)]
if not wrong_conn or len(wrong_reject) != len(wrong_conn):
    raise SystemExit(
        "FAIL wrong-password server contract: every attempted connection must be "
        f"rejected by the game (conn={len(wrong_conn)} reject={len(wrong_reject)})")
if wrong_conn[0] <= arm_indices[0]:
    raise SystemExit("FAIL protected server: wrong-password attempt preceded readiness marker")
for ordinal, conn_index in enumerate(wrong_conn):
    next_conn = wrong_conn[ordinal + 1] if ordinal + 1 < len(wrong_conn) else len(wrong_server)
    if not any(conn_index < reject_index < next_conn for reject_index in wrong_reject):
        raise SystemExit(
            f"FAIL wrong-password server contract: attempt {ordinal + 1} lacks ordered rejection")
wrong_attempt_tail = wrong_server[wrong_conn[0]:]
if any("SV_OnPlayerConnect: slot " in msg for msg in wrong_attempt_tail) \
        or any(re.search(r"Client(?:Connect|Begin): [1-9][0-9]*\b", msg)
               for msg in wrong_attempt_tail):
    raise SystemExit("FAIL wrong-password server contract: rejected client reached admission")

if any("Invalid password" in msg or "game rejected connection" in msg
       for msg in correct_server):
    raise SystemExit("FAIL password recovery: correct credential was rejected")
assigned = []
for index, msg in enumerate(correct_server):
    match = re.search(r"SV_OnPlayerConnect: slot ([1-9][0-9]*) assigned to conn=", msg)
    if match:
        assigned.append((index, int(match.group(1))))
if len(assigned) != 1:
    raise SystemExit(f"FAIL server contract: expected one assigned remote slot, got {assigned}")
assigned_index, assigned_slot = assigned[0]
connect_indices = [index for index, msg in enumerate(correct_server)
                   if re.search(rf"ClientConnect: {assigned_slot}\b", msg)]
begin_indices = [index for index, msg in enumerate(correct_server)
                 if re.search(rf"ClientBegin: {assigned_slot}\b", msg)]
conn_indices = [index for index, msg in enumerate(correct_server)
                if "SV_OnPlayerConnect: conn=" in msg]
if len(conn_indices) != 1 or len(connect_indices) != 1 or len(begin_indices) != 1:
    raise SystemExit(
        f"FAIL server contract: slot {assigned_slot} admission counts "
        f"conn={len(conn_indices)} connect={len(connect_indices)} begin={len(begin_indices)}")
if not (conn_indices[0] < connect_indices[0] < assigned_index < begin_indices[0]):
    raise SystemExit(
        f"FAIL server contract: slot {assigned_slot} admission order is not "
        "OnPlayerConnect -> ClientConnect -> assigned -> ClientBegin")

server_steps = [
    rf"WiredNet: listening on port {server_port} \(IPv4\)",
    r"Server: arena7\b",
    r"StatusBot has entered the game",
    r"SVC_Status: request from 127\.0\.0\.1:[0-9]+",
    r"SVC_Status: request from 127\.0\.0\.1:[0-9]+",
    r"SVC_Status: request from 127\.0\.0\.1:[0-9]+",
    r"SV_OnPlayerConnect: conn=",
    r"----- Server Shutdown \(Server quit\) -----",
    r"QUIC transport shut down\.",
]
cursor = 0
for pattern in server_steps:
    for index in range(cursor, len(server_messages)):
        if re.search(pattern, server_messages[index]):
            cursor = index + 1
            break
    else:
        raise SystemExit(f"FAIL server contract: missing/out-of-order {pattern}")

print("  PASS browser: automated-only exact two-row cache and real keyboard row0 -> row1 selection")
print("  PASS status: valid -> timeout A -> malformed B -> valid C recovery")
print("  PASS password-negative: wrong credential -> game rejection -> no admission/FIRST (bounded watchdog)")
print("  PASS password-recovery: fresh correct client -> masked edit -> accepted game admission")
print("  PASS connect: protected cache -> canonical queue -> CloseAll -> QUIC ACCEPT -> arena7 FIRST")
print("  PASS lifecycle: status cancel/return -> gameplay -> controlled client/server shutdown")
PYEOF
}

if [ "${1:-}" = "--self-test" ]; then
    ROOT="$(mktemp -d -t wired-q0browser-self-XXXXXX 2>/dev/null || mktemp -d)"
    trap 'rm -rf "$ROOT"' EXIT INT TERM
    CLEAN_PRODUCT="$ROOT/product.jsonl"
    CLEAN_FIXTURE="$ROOT/fixture.jsonl"
    CLEAN_SERVER="$ROOT/server.jsonl"
    CLEAN_DOUBLE_SERVER="$ROOT/double-server.jsonl"
    CLEAN_WRONG="$ROOT/wrong.jsonl"
    CLEAN_LAYOUT="$ROOT/layout.jsonl"
    python3 - "$CLEAN_PRODUCT" "$CLEAN_FIXTURE" "$CLEAN_SERVER" "$CLEAN_DOUBLE_SERVER" "$CLEAN_WRONG" "$CLEAN_LAYOUT" <<'PYEOF'
import json, sys
product, fixture, server, double_server, wrong, layout_path = sys.argv[1:7]
s, t = "127.0.0.1:28001", "127.0.0.1:28002"
msgs = [
 "WiredUI: loaded 0 UI state entries (ignored transient server selection 2)",
 "WiredUI: reset transient server selection after state load",
 "Ignored unsolicited infoResponse from 127.0.0.1:27960",
 "WiredUI: push menu 'servers' (depth 1)",
 "wui_menu_nav focus: focused item 'btn_connect' (top index -1)",
 "WiredUI: browser connect refused without current selection",
 "WiredUI: pop menu (depth 0)",
 "WiredUI: push menu 'servers' (depth 1)",
 "Scanning for servers on the local network...",
 "Ignored infoResponse challenge mismatch generation=2 from 192.0.2.10:27960",
 "Rejected invalid infoResponse from 192.0.2.10:27960",
 "wui_menu_nav focus: focused item 'btn_filter' (top index -1)",
 "WiredUI: server roster generation=2 source=0 displayed=0 raw=0",
 "wui_menu_nav: K_ENTER dispatched",
 "wui_menu_nav focus: focused item 'btn_refresh' (top index -1)",
 "wui_menu_nav: K_ENTER dispatched",
 "Scanning for servers on the local network...",
 "Ignored infoResponse challenge mismatch generation=3 from 192.0.2.10:27960",
 "Accepted local discovery infoResponse generation=3 address=192.0.2.10:27960 rtt=17ms name=LAN WIRED Q0 TARGET map=arena7 clients=1/8 gametype=0",
 "WiredUI: server roster generation=3 source=0 displayed=1 raw=1",
 "WiredUI: server roster row=0 raw=0 address=192.0.2.10:27960 name=LAN WIRED Q0 TARGET map=arena7",
 "WiredUI: server selection display_row=0 raw=0 source=0 list_generation=3 selection_generation=2 address=192.0.2.10:27960 name=LAN WIRED Q0 TARGET map=arena7",
 "wui_menu_nav focus: focused item 'serverlist' (top index 1)",
 "wui_menu_nav: K_DOWNARROW dispatched",
 "WiredUI: pop menu (depth 0)",
 "WiredUI: server fixture installed sentinel=127.0.0.1:28001 target=127.0.0.1:28002 raw_order=target,sentinel",
 "WiredUI: push menu 'servers' (depth 1)",
 "WiredUI: server roster generation=4 source=0 displayed=2 raw=2",
 f"WiredUI: server roster row=0 raw=1 address={s} name=A0 WIRED Q0 SENTINEL map=arena1",
 f"WiredUI: server roster row=1 raw=0 address={t} name=Z0 WIRED Q0 TARGET map=arena7",
 "wui_menu_nav focus: focused item 'serverlist' (top index 1)",
 f"WiredUI: server selection display_row=1 raw=0 source=0 list_generation=4 selection_generation=3 address={t} name=Z0 WIRED Q0 TARGET map=arena7",
 "wui_menu_nav: K_DOWNARROW dispatched",
 "wui_menu_nav focus: focused item 'btn_info' (top index -1)",
 f"WiredUI: server status request generation=5 selection_generation=3 address={t}",
 "WiredUI: push menu 'serverinfo' (depth 2)",
 "wui_menu_nav: K_ENTER dispatched",
 "wui_menu_nav focus: focused item 'btn_back' (top index -1)",
 "WiredUI: server status cancelled generation=6 rows=0",
 "WiredUI: pop menu (depth 1)",
 "WiredUI: push menu 'servers' collapsed (already on top, depth 1)",
 "wui_menu_nav focus: focused item 'btn_info' (top index -1)",
 f"WiredUI: server status request generation=7 selection_generation=3 address={t}",
 "WiredUI: push menu 'serverinfo' (depth 2)",
 "wui_menu_nav: K_ENTER dispatched",
 f"CL_ServerStatusResponse: ignored stale challenge from {t}",
 f"CL_ServerStatusResponse: stored response from {t} bytes=600",
 f"WiredUI: server status loaded generation=5 selection_generation=3 address={t} rows=9",
 "WiredUI: server status registry feeder=13 count=9",
 f"WiredUI: server status registry row=0 key=Address value={t}",
 "WiredUI: server status registry row=1 key=Server value=Z0 WIRED Q0 TARGET",
 "WiredUI: server status registry row=2 key=Map value=arena7",
 "WiredUI: server status registry row=3 key=Players value=1/8",
 "WiredUI: server status registry row=4 key=Game type value=0",
 "WiredUI: server status registry row=5 key=Game value=q3now",
 "WiredUI: server status registry row=6 key=Protocol value=74",
 "WiredUI: server status registry row=7 key=Version value=Wired 0.80.77 macos-arm64 Aug 11 2026",
 "WiredUI: server status registry row=8 key=Player 1 value=StatusBot — score 0, ping 0",
 "wui_menu_nav focus: focused item 'statuslist' (top index 1)",
 "wui_menu_nav focus: focused item 'btn_back' (top index -1)",
 "WiredUI: server status cancelled generation=6 rows=0",
 "WiredUI: pop menu (depth 1)",
 "WiredUI: push menu 'servers' collapsed (already on top, depth 1)",
 "wui_menu_nav focus: focused item 'btn_info' (top index -1)",
 f"WiredUI: server status request generation=9 selection_generation=3 address={t}",
 f"WiredUI: server status state=pending generation=9 selection_generation=3 address={t} rows=1",
 "WiredUI: push menu 'serverinfo' (depth 2)",
 "wui_menu_nav: K_ENTER dispatched",
 "WiredUI: server status registry feeder=13 count=1",
 "WiredUI: server status registry row=0 key=Status value=Contacting server...",
 f"WiredUI: server status failed generation=9 selection_generation=3 address={t} state=no-response reason=timeout rows=2",
 "WiredUI: server status registry feeder=13 count=2",
 f"WiredUI: server status registry row=0 key=Address value={t}",
 "WiredUI: server status registry row=1 key=Status value=No response from server.",
 "wui_menu_nav focus: focused item 'statuslist' (top index 1)",
 "wui_menu_nav focus: focused item 'btn_retry' (top index -1)",
 f"WiredUI: server status request generation=10 selection_generation=3 address={t}",
 f"WiredUI: server status state=pending generation=10 selection_generation=3 address={t} rows=1",
 "wui_menu_nav: K_ENTER dispatched",
 f"CL_ServerStatusResponse: ignored stale challenge from {t}",
 f"CL_ServerStatusResponse: stored response from {t} bytes=180",
 f"WiredUI: server status failed generation=10 selection_generation=3 address={t} state=malformed reason=invalid-sv_maxclients rows=2",
 "WiredUI: server status registry feeder=13 count=2",
 f"WiredUI: server status registry row=0 key=Address value={t}",
 "WiredUI: server status registry row=1 key=Status value=Invalid server response.",
 "wui_menu_nav focus: focused item 'statuslist' (top index 1)",
 "wui_menu_nav focus: focused item 'btn_retry' (top index -1)",
 f"WiredUI: server status request generation=11 selection_generation=3 address={t}",
 f"WiredUI: server status state=pending generation=11 selection_generation=3 address={t} rows=1",
 "wui_menu_nav: K_ENTER dispatched",
 f"CL_ServerStatusResponse: ignored stale challenge from {t}",
 f"CL_ServerStatusResponse: stored response from {t} bytes=600",
 f"WiredUI: server status loaded generation=11 selection_generation=3 address={t} rows=9",
 "WiredUI: server status registry feeder=13 count=9",
 f"WiredUI: server status registry row=0 key=Address value={t}",
 "WiredUI: server status registry row=1 key=Server value=Z0 WIRED Q0 TARGET",
 "WiredUI: server status registry row=2 key=Map value=arena7",
 "WiredUI: server status registry row=3 key=Players value=1/8",
 "WiredUI: server status registry row=4 key=Game type value=0",
 "WiredUI: server status registry row=5 key=Game value=q3now",
 "WiredUI: server status registry row=6 key=Protocol value=74",
 "WiredUI: server status registry row=7 key=Version value=Wired 0.80.77 macos-arm64 Aug 11 2026",
 "WiredUI: server status registry row=8 key=Player 1 value=StatusBot — score 0, ping 0",
 "wui_menu_nav focus: focused item 'statuslist' (top index 1)",
 "wui_menu_nav focus: focused item 'btn_back' (top index -1)",
 "WiredUI: server status cancelled generation=12 rows=0",
 "WiredUI: pop menu (depth 1)",
 "WiredUI: push menu 'servers' collapsed (already on top, depth 1)",
 "wui_menu_nav focus: focused item 'btn_connect' (top index -1)",
 "WiredUI: server roster generation=8 source=0 displayed=2 raw=2",
 f"WiredUI: server roster row=0 raw=1 address={s} name=A0 WIRED Q0 SENTINEL map=arena1",
 "WiredUI: server roster row=1 raw=0 address=127.0.0.1:28003 name=Z0 WIRED Q0 TARGET map=arena7",
 "WiredUI: server fixture installed sentinel=127.0.0.1:28001 target=127.0.0.1:28003 target_needpass=1 raw_order=target,sentinel",
 "wui_menu_nav focus: focused item 'serverlist' (top index 1)",
 f"WiredUI: server selection display_row=0 raw=1 source=0 list_generation=8 selection_generation=4 address={s} name=A0 WIRED Q0 SENTINEL map=arena1",
 "wui_menu_nav: K_UPARROW dispatched",
 "WiredUI: server selection display_row=1 raw=0 source=0 list_generation=8 selection_generation=5 address=127.0.0.1:28003 name=Z0 WIRED Q0 TARGET map=arena7",
 "wui_menu_nav: K_DOWNARROW dispatched",
 "wui_menu_nav focus: focused item 'btn_connect' (top index -1)",
 "WiredUI: password required origin=browser address=127.0.0.1:28003 selection_generation=5",
 "WiredUI: push menu 'password' (depth 2)",
 "wui_menu_nav: K_ENTER dispatched",
 "wui_menu_nav focus: focused item 'btn_connect' (top index -1)",
 "WiredUI: password submit refused reason=invalid-credential",
 "wui_menu_nav: K_ENTER dispatched",
 "wui_menu_nav focus: focused item 'row_password' (top index -1)",
 "wui_menu_nav: K_ENTER dispatched",
 "wui_menu_nav: typed 7 printable character(s)",
 "wui_menu_nav: K_ENTER dispatched",
 "WiredUI: password render trace length=7 masked=1",
 "wui_menu_nav focus: focused item 'btn_connect' (top index -1)",
 "WiredUI: password submit accepted address=127.0.0.1:28003 selection_generation=5",
 "WiredUI: queued validated connect origin=browser address=127.0.0.1:28003",
 "WiredUI: close all postcondition depth=0 active=none catcher_ui=0 paused=0",
 "wui_menu_nav: K_ENTER dispatched",
 "127.0.0.1:28003 resolved to 127.0.0.1:28003",
 "QUIC client: TLV ACCEPT received slot=0 sv_fps=20",
 "cls.state: -> CA_ACTIVE (FIRST GAMEPLAY FRAME mapname=maps/arena7.bsp serverTime=100 numEntities=3 framecount=1)",
 "WiredUI: shutdown",
]
with open(product, "w", encoding="utf-8") as out:
    for msg in msgs: out.write(json.dumps({"sev":"DEBUG","cat":"ui","msg":msg})+"\n")
wrong_msgs = [
 "WiredUI: push menu 'servers' (depth 1)",
 "WiredUI: server fixture installed sentinel=127.0.0.1:28001 target=127.0.0.1:28003 target_needpass=1 raw_order=target,sentinel",
 "wui_menu_nav focus: focused item 'serverlist' (top index 1)",
 "WiredUI: server selection display_row=1 raw=0 source=0 list_generation=1 selection_generation=2 address=127.0.0.1:28003 name=Z0 WIRED Q0 TARGET map=arena7",
 "wui_menu_nav: K_DOWNARROW dispatched",
 "wui_menu_nav focus: focused item 'btn_connect' (top index -1)",
 "WiredUI: password required origin=browser address=127.0.0.1:28003 selection_generation=2",
 "WiredUI: push menu 'password' (depth 2)",
 "wui_menu_nav: K_ENTER dispatched",
 "wui_menu_nav focus: focused item 'row_password' (top index -1)",
 "wui_menu_nav: K_ENTER dispatched",
 "wui_menu_nav: typed 7 printable character(s)",
 "wui_menu_nav: K_ENTER dispatched",
 "WiredUI: password render trace length=7 masked=1",
 "wui_menu_nav focus: focused item 'btn_connect' (top index -1)",
 "WiredUI: password submit accepted address=127.0.0.1:28003 selection_generation=2",
 "WiredUI: queued validated connect origin=browser address=127.0.0.1:28003",
 "WiredUI: close all postcondition depth=0 active=none catcher_ui=0 paused=0",
 "wui_menu_nav: K_ENTER dispatched",
 "127.0.0.1:28003 resolved to 127.0.0.1:28003",
 "QUIC client: TLV ACCEPT received slot=0 sv_fps=20",
]
with open(wrong, "w", encoding="utf-8") as out:
    for msg in wrong_msgs: out.write(json.dumps({"sev":"DEBUG","cat":"ui","msg":msg})+"\n")
layout = []
for frame, focused in ((800, "row_password"), (801, "row_password"),
                       (802, "btn_connect"), (803, "btn_connect")):
    for region, kind, rect in (
            ("password", "menu", (0, 0, 1280, 720)),
            ("password_root", "item", (384, 252, 512, 216)),
            ("subheader", "item", (404, 290, 472, 24)),
            ("row_password", "item", (404, 330, 460, 36)),
            ("btn_connect", "item", (720, 400, 160, 36))):
        x, y, w, h = rect
        layout.append({"region":region, "kind":kind, "x":x, "y":y,
                       "w":w, "h":h, "focused":int(region == focused),
                       "frame":frame, "menu":"password"})
with open(layout_path, "w", encoding="utf-8") as out:
    for row in layout: out.write(json.dumps(row)+"\n")
events = [
 {"event":"ready"},
 {"event":"lan_unsolicited_pre_scan","address":"127.0.0.1:27960"},
 {"event":"lan_scan_seen","ordinal":1,"challenge":"1000000000000001","address":"192.0.2.10:27960"},
 {"event":"lan_warmup_missing","challenge":"1000000000000001"},
 {"event":"lan_scan_seen","ordinal":2,"challenge":"2000000000000002","address":"192.0.2.10:27960"},
 {"event":"lan_scan_a_missing","challenge":"2000000000000002"},
 {"event":"lan_scan_a_wrong_sent","challenge":"ffffffffffffffff","expected":"2000000000000002","address":"192.0.2.10:27960"},
 {"event":"lan_scan_a_malformed_sent","challenge":"2000000000000002","address":"192.0.2.10:27960"},
 {"event":"lan_scan_seen","ordinal":3,"challenge":"3000000000000003","address":"192.0.2.10:27960"},
 {"event":"lan_scan_b_late_a_sent","stale_challenge":"2000000000000002","current_challenge":"3000000000000003","address":"192.0.2.10:27960"},
 {"event":"lan_scan_b_current_sent","challenge":"3000000000000003","address":"192.0.2.10:27960"},
 {"event":"lan_scan_b_duplicate_sent","challenge":"3000000000000003","address":"192.0.2.10:27960"},
 {"event":"disallowed_probe","command":"rcon"},
 {"event":"challenge_seen","ordinal":1,"challenge":"0000000100000001"},
 {"event":"request","role":"target","command":"getstatus"},
 {"event":"upstream_request","ordinal":1},
 {"event":"held_stale_response"},
 {"event":"challenge_seen","ordinal":2,"challenge":"0000000200000002"},
 {"event":"request","role":"target","command":"getstatus"},
 {"event":"upstream_request","ordinal":2},
 {"event":"sent_stale_response"},
 {"event":"sent_current_response"},
 {"event":"challenge_seen","ordinal":3,"challenge":"0000000300000003"},
 {"event":"request","role":"target","command":"getstatus"},
 {"event":"timeout_request","ordinal":3,"challenge":"0000000300000003"},
 {"event":"held_timeout_response","ordinal":3},
 {"event":"request","role":"target","command":"getstatus"},
 {"event":"timeout_request","ordinal":3,"challenge":"0000000300000003"},
 {"event":"challenge_seen","ordinal":4,"challenge":"0000000400000004"},
 {"event":"request","role":"target","command":"getstatus"},
 {"event":"malformed_retry_request","ordinal":4,"challenge":"0000000400000004"},
 {"event":"sent_timeout_stale_response","stale_ordinal":3,"current_ordinal":4},
 {"event":"sent_malformed_response","current_ordinal":4},
 {"event":"challenge_seen","ordinal":5,"challenge":"0000000500000005"},
 {"event":"request","role":"target","command":"getstatus"},
 {"event":"valid_retry_request","ordinal":5,"challenge":"0000000500000005"},
 {"event":"upstream_retry_request","ordinal":5},
 {"event":"sent_malformed_stale_response","stale_ordinal":4,"current_ordinal":5},
 {"event":"sent_retry_valid_response","current_ordinal":5},
 {"event":"stopped"},
]
with open(fixture, "w", encoding="utf-8") as out:
    for row in events: out.write(json.dumps(row)+"\n")
server_msgs = [
 "WiredNet: listening on port 28003 (IPv4), ALPN: q3v69",
 "Server: arena7",
 "broadcast: print \"StatusBot has entered the game\\n\"",
 "Server info settings:",
 "g_needpass           1",
 "Q0_PASSWORD_SERVER_READY",
 "SVC_Status: request from 127.0.0.1:28003",
 "SVC_Status: request from 127.0.0.1:28003",
 "SVC_Status: request from 127.0.0.1:28003",
 "SV_OnPlayerConnect: conn=11",
 "QUIC: game rejected connection from 127.0.0.1:28000: Invalid password",
 "Q0_WRONG_PASSWORD_PHASE_COMPLETE",
 "SV_OnPlayerConnect: conn=17",
 "  0:10 ClientConnect: 2",
 "SV_OnPlayerConnect: slot 2 assigned to conn=17 (127.0.0.1)",
 "  0:10 ClientBegin: 2",
 "----- Server Shutdown (Server quit) -----",
 "QUIC transport shut down.",
]
with open(server, "w", encoding="utf-8") as out:
    for msg in server_msgs: out.write(json.dumps({"sev":"DEBUG","cat":"server","msg":msg})+"\n")
double_server_msgs = [
 "WiredNet: listening on port 28004 (IPv4), ALPN: q3v69",
 "Server: arena7",
 "broadcast: print \"StatusBot has entered the game\\n\"",
 "Server info settings:",
 "g_needpass           0",
 "SV_OnPlayerConnect: conn=7",
 "  0:05 ClientConnect: 2",
 "SV_OnPlayerConnect: slot 2 assigned to conn=7 (127.0.0.1)",
 "  0:05 ClientBegin: 2",
 "Q0_DOUBLECLICK_PHASE_COMPLETE",
 "Q0_DOUBLECLICK_QUIT_REQUESTED",
 "----- Server Shutdown (Server quit) -----",
 "QUIC transport shut down.",
]
with open(double_server, "w", encoding="utf-8") as out:
    for msg in double_server_msgs:
        out.write(json.dumps({"sev":"DEBUG","cat":"server","msg":msg})+"\n")
PYEOF
    CLEAN_DOUBLE="$ROOT/double.jsonl"
    CLEAN_DOUBLE_LAYOUT="$ROOT/double-layout.jsonl"
    python3 - "$CLEAN_DOUBLE" "$CLEAN_DOUBLE_LAYOUT" <<'PYEOF'
import json, sys
product, layout = sys.argv[1:3]
s, t = "127.0.0.1:28001", "127.0.0.1:28004"
msgs = [
 f"WiredUI: server fixture installed sentinel={s} target={t} raw_order=target,sentinel",
 "WiredUI: push menu 'servers' (depth 1)",
 f"WiredUI: server roster row=0 raw=1 address={s} name=A0 WIRED Q0 SENTINEL map=arena1",
 f"WiredUI: server roster row=1 raw=0 address={t} name=Z0 WIRED Q0 TARGET map=arena7",
 "Q0_DBL_ENTER_BEGIN",
 "WiredUI: listbox activate input=K_ENTER menu=servers item=serverlist feeder=2 row=0",
 "WiredUI: listbox activate input=K_ENTER menu=servers item=serverlist feeder=2 row=0",
 "Q0_DBL_ENTER_END",
]
def move(row, generation, y):
    msgs.append(f"WiredUI: pointer phase=moved ingress=CL_MouseEvent menu=servers item=serverlist feeder=2 row={row} list_generation={generation} x=640 y={y}")
def helper_click():
    msgs.append("WiredUI: pointer phase=click ingress=CL_KeyEvent key=K_MOUSE1 x=640 y=340")
def callback(row, raw, generation, selection):
    addr, name, mapname = (t, "Z0 WIRED Q0 TARGET", "arena7") if row == 1 else (s, "A0 WIRED Q0 SENTINEL", "arena1")
    msgs.append(f"WiredUI: server selection display_row={row} raw={raw} source=0 list_generation={generation} selection_generation={selection} address={addr} name={name} map={mapname}")
def arm(row, raw, generation, selection):
    msgs.append(f"WiredUI: listbox click phase=armed input=K_MOUSE1 menu=servers item=serverlist feeder=2 row={row} raw={raw} source=0 list_generation={generation} selection_generation={selection}")
def release(row, generation):
    msgs.append(f"WiredUI: listbox click phase=release input=K_MOUSE1 menu=servers item=serverlist feeder=2 row={row} list_generation={generation} pointer_down=0")
def reset(reason, row, generation):
    msgs.append(f"WiredUI: listbox click phase=reset reason={reason} menu=servers item=serverlist feeder=2 row={row} list_generation={generation}")

msgs.append("Q0_DBL_SINGLE_BEGIN")
move(1, 4, 340); helper_click(); callback(1, 0, 4, 2); arm(1, 0, 4, 2); release(1, 4)
msgs += ["Q0_DBL_SINGLE_END"]
reset("push", 1, 4)

msgs.append("Q0_DBL_MISSING_RELEASE_BEGIN")
move(1, 4, 340)
msgs.append("WiredUI: pointer phase=button ingress=CL_KeyEvent key=K_MOUSE1 edge=down x=640 y=340")
callback(1, 0, 4, 3); arm(1, 0, 4, 3)
msgs.append("WiredUI: pointer phase=button ingress=CL_KeyEvent key=K_MOUSE1 edge=down x=640 y=340")
callback(1, 0, 4, 4); reset("missing-release", 1, 4); arm(1, 0, 4, 4)
msgs.append("WiredUI: pointer phase=button ingress=CL_KeyEvent key=K_MOUSE1 edge=up x=640 y=340")
release(1, 4); msgs.append("Q0_DBL_MISSING_RELEASE_END"); reset("push", 1, 4)

msgs.append("Q0_DBL_TIMEOUT_BEGIN")
move(1, 4, 340); helper_click(); callback(1, 0, 4, 5); arm(1, 0, 4, 5); release(1, 4)
move(1, 4, 340); helper_click(); callback(1, 0, 4, 6); reset("timeout", 1, 4); arm(1, 0, 4, 6); release(1, 4)
msgs.append("Q0_DBL_TIMEOUT_END"); reset("push", 1, 4)

msgs.append("Q0_DBL_DIFFERENT_ROW_BEGIN")
move(0, 4, 310); helper_click(); callback(0, 1, 4, 7); arm(0, 1, 4, 7); release(0, 4)
move(1, 4, 340); helper_click(); callback(1, 0, 4, 8); reset("different-row", 0, 4); arm(1, 0, 4, 8); release(1, 4)
msgs.append("Q0_DBL_DIFFERENT_ROW_END"); reset("push", 1, 4)

msgs.append("Q0_DBL_LIFECYCLE_BEGIN")
move(1, 4, 340); helper_click(); callback(1, 0, 4, 9); arm(1, 0, 4, 9); release(1, 4)
reset("pop", 1, 4); msgs += ["WiredUI: pop menu (depth 0)", "WiredUI: push menu 'servers' (depth 1)"]
move(1, 5, 340); helper_click(); callback(1, 0, 5, 10); arm(1, 0, 5, 10); release(1, 5)
msgs.append("Q0_DBL_LIFECYCLE_END"); reset("push", 1, 5)

msgs.append("Q0_DBL_GENERATION_BEGIN")
move(1, 5, 340); helper_click(); callback(1, 0, 5, 11); arm(1, 0, 5, 11); release(1, 5)
reset("server-roster", 1, 5)
msgs += [f"WiredUI: server fixture installed sentinel={s} target={t} raw_order=target,sentinel",
         f"WiredUI: server roster row=0 raw=1 address={s} name=A0 WIRED Q0 SENTINEL map=arena1",
         f"WiredUI: server roster row=1 raw=0 address={t} name=Z0 WIRED Q0 TARGET map=arena7"]
move(1, 6, 340); helper_click(); callback(1, 0, 6, 12); arm(1, 0, 6, 12); release(1, 6)
msgs.append("Q0_DBL_GENERATION_END"); reset("push", 1, 6)

msgs.append("Q0_DBL_POSITIVE_BEGIN")
move(1, 6, 340); helper_click(); callback(1, 0, 6, 13); arm(1, 0, 6, 13); release(1, 6)
move(1, 6, 340); helper_click(); callback(1, 0, 6, 14)
msgs += [
 "WiredUI: listbox click phase=double input=K_MOUSE1 menu=servers item=serverlist feeder=2 row=1 raw=0 source=0 list_generation=6 selection_generation=14 elapsed=33",
 f"WiredUI: queued validated connect origin=browser address={t}",
 "WiredUI: pointer phase=release reason=close-all was_down=1 pointer_down=0",
 "WiredUI: close all postcondition depth=0 active=none catcher_ui=0 paused=0",
 "Q0_DBL_POSITIVE_END",
 f"{t} resolved to {t}",
 "QUIC client: TLV ACCEPT received slot=0 sv_fps=20",
 "cls.state: -> CA_ACTIVE (FIRST GAMEPLAY FRAME mapname=maps/arena7.bsp serverTime=100 numEntities=3 framecount=1)",
 "WiredUI: shutdown",
]
with open(product, "w", encoding="utf-8") as out:
    for msg in msgs:
        out.write(json.dumps({"sev":"DEBUG", "cat":"ui", "msg":msg}) + "\n")
with open(layout, "w", encoding="utf-8") as out:
    out.write(json.dumps({"region":"serverlist", "menu":"servers", "frame":100,
                          "focused":1, "x":80, "y":220, "w":1120, "h":320}) + "\n")
PYEOF
    analyze_doubleclick_contract "$CLEAN_DOUBLE" "$CLEAN_DOUBLE_LAYOUT" "$CLEAN_DOUBLE_SERVER" 28001 28004 >/dev/null || { echo "FAIL self-test: clean doubleclick fixture rejected"; exit 1; }
    for defect in dbl_missing_enter dbl_enter_callback dbl_enter_arm dbl_missing_move dbl_fake_move dbl_missing_click dbl_fake_click dbl_missing_callback dbl_wrong_callback dbl_missing_arm dbl_missing_release dbl_missing_edge_down dbl_missing_release_reset dbl_missing_timeout_reset dbl_missing_row_reset dbl_missing_pop_reset dbl_missing_roster_reset dbl_reused_generation dbl_missing_double dbl_duplicate_double dbl_bad_elapsed dbl_early_queue dbl_duplicate_queue dbl_missing_terminal_release dbl_bad_pointer_down dbl_no_close dbl_no_first dbl_wrong_first dbl_missing_server_conn dbl_missing_server_assign dbl_missing_server_begin dbl_missing_boundary dbl_missing_unprotected dbl_missing_quit_request dbl_missing_server_shutdown dbl_missing_transport_shutdown dbl_shutdown_before_boundary dbl_shared_server dbl_server_reject dbl_severity dbl_malformed dbl_missing_layout dbl_pointer_outside; do
        DIR="$ROOT/$defect"; mkdir -p "$DIR"
        python3 - "$CLEAN_DOUBLE" "$CLEAN_DOUBLE_LAYOUT" "$CLEAN_DOUBLE_SERVER" \
            "$DIR/product" "$DIR/layout" "$DIR/server" "$defect" <<'PYEOF'
import json, re, sys
product_source, layout_source, server_source, product_path, layout_path, server_path, defect = sys.argv[1:8]
p = [json.loads(line) for line in open(product_source, encoding="utf-8")]
layout = [json.loads(line) for line in open(layout_source, encoding="utf-8")]
server = [json.loads(line) for line in open(server_source, encoding="utf-8")]

def remove_nth(rows, needle, ordinal=1):
    seen = 0
    for index, row in enumerate(rows):
        if needle in row.get("msg", ""):
            seen += 1
            if seen == ordinal:
                rows.pop(index)
                return

if defect == "dbl_missing_enter": remove_nth(p, "listbox activate input=K_ENTER")
elif defect == "dbl_enter_callback":
    end = next(i for i,r in enumerate(p) if r["msg"] == "Q0_DBL_ENTER_END")
    p.insert(end, {"sev":"DEBUG","cat":"ui","msg":"WiredUI: server selection display_row=1 raw=0 source=0 list_generation=4 selection_generation=1 address=127.0.0.1:28004 name=Z0 WIRED Q0 TARGET map=arena7"})
elif defect == "dbl_enter_arm":
    end = next(i for i,r in enumerate(p) if r["msg"] == "Q0_DBL_ENTER_END")
    p.insert(end, {"sev":"DEBUG","cat":"ui","msg":"WiredUI: listbox click phase=armed input=K_MOUSE1 menu=servers item=serverlist feeder=2 row=0 raw=1 source=0 list_generation=4 selection_generation=1"})
elif defect == "dbl_missing_move":
    begin = next(i for i,r in enumerate(p) if r["msg"] == "Q0_DBL_POSITIVE_BEGIN")
    for index in range(begin + 1, len(p)):
        if "pointer phase=moved" in p[index]["msg"]: p.pop(index); break
elif defect == "dbl_fake_move":
    row = next(r for r in p if "pointer phase=moved" in r["msg"])
    row["msg"] = row["msg"].replace("ingress=CL_MouseEvent", "ingress=direct")
elif defect == "dbl_missing_click":
    begin = next(i for i,r in enumerate(p) if r["msg"] == "Q0_DBL_POSITIVE_BEGIN")
    for index in range(begin + 1, len(p)):
        if "pointer phase=click" in p[index]["msg"]: p.pop(index); break
elif defect == "dbl_fake_click":
    row = next(r for r in p if "pointer phase=click" in r["msg"])
    row["msg"] = row["msg"].replace("ingress=CL_KeyEvent", "ingress=direct")
elif defect == "dbl_missing_callback":
    begin = next(i for i,r in enumerate(p) if r["msg"] == "Q0_DBL_POSITIVE_BEGIN")
    for index in range(begin + 1, len(p)):
        if "server selection display_row=1" in p[index]["msg"]: p.pop(index); break
elif defect == "dbl_wrong_callback":
    begin = next(i for i,r in enumerate(p) if r["msg"] == "Q0_DBL_POSITIVE_BEGIN")
    row = next(r for r in p[begin:] if "server selection display_row=1" in r["msg"])
    row["msg"] = row["msg"].replace("address=127.0.0.1:28004", "address=127.0.0.1:28001")
elif defect == "dbl_missing_arm":
    begin = next(i for i,r in enumerate(p) if r["msg"] == "Q0_DBL_POSITIVE_BEGIN")
    for index in range(begin + 1, len(p)):
        if "listbox click phase=armed" in p[index]["msg"]: p.pop(index); break
elif defect == "dbl_missing_release":
    begin = next(i for i,r in enumerate(p) if r["msg"] == "Q0_DBL_POSITIVE_BEGIN")
    for index in range(begin + 1, len(p)):
        if "listbox click phase=release" in p[index]["msg"]: p.pop(index); break
elif defect == "dbl_missing_edge_down": remove_nth(p, "pointer phase=button ingress=CL_KeyEvent key=K_MOUSE1 edge=down", 2)
elif defect == "dbl_missing_release_reset": p = [r for r in p if "reason=missing-release" not in r["msg"]]
elif defect == "dbl_missing_timeout_reset": p = [r for r in p if "reason=timeout" not in r["msg"]]
elif defect == "dbl_missing_row_reset": p = [r for r in p if "reason=different-row" not in r["msg"]]
elif defect == "dbl_missing_pop_reset": p = [r for r in p if "reason=pop" not in r["msg"]]
elif defect == "dbl_missing_roster_reset": p = [r for r in p if "reason=server-roster" not in r["msg"]]
elif defect == "dbl_reused_generation":
    begin = next(i for i,r in enumerate(p) if r["msg"] == "Q0_DBL_GENERATION_BEGIN")
    end = next(i for i,r in enumerate(p) if r["msg"] == "Q0_DBL_GENERATION_END")
    moves = [r for r in p[begin:end] if "pointer phase=moved" in r["msg"]]
    moves[-1]["msg"] = moves[-1]["msg"].replace("list_generation=6", "list_generation=5")
elif defect == "dbl_missing_double": p = [r for r in p if "listbox click phase=double" not in r["msg"]]
elif defect == "dbl_duplicate_double":
    index = next(i for i,r in enumerate(p) if "listbox click phase=double" in r["msg"])
    p.insert(index, p[index].copy())
elif defect == "dbl_bad_elapsed":
    row = next(r for r in p if "listbox click phase=double" in r["msg"])
    row["msg"] = row["msg"].replace("elapsed=33", "elapsed=300")
elif defect == "dbl_early_queue":
    index = next(i for i,r in enumerate(p) if "queued validated connect" in r["msg"])
    row = p.pop(index)
    begin = next(i for i,r in enumerate(p) if r["msg"] == "Q0_DBL_SINGLE_BEGIN")
    p.insert(begin + 1, row)
elif defect == "dbl_duplicate_queue":
    index = next(i for i,r in enumerate(p) if "queued validated connect" in r["msg"])
    p.insert(index, p[index].copy())
elif defect == "dbl_missing_terminal_release": p = [r for r in p if "pointer phase=release reason=close-all" not in r["msg"]]
elif defect == "dbl_bad_pointer_down":
    row = next(r for r in p if "pointer phase=release reason=close-all" in r["msg"])
    row["msg"] = row["msg"].replace("pointer_down=0", "pointer_down=1")
elif defect == "dbl_no_close": p = [r for r in p if "close all postcondition" not in r["msg"]]
elif defect == "dbl_no_first": p = [r for r in p if "FIRST GAMEPLAY FRAME" not in r["msg"]]
elif defect == "dbl_wrong_first":
    row = next(r for r in p if "FIRST GAMEPLAY FRAME" in r["msg"])
    row["msg"] = row["msg"].replace("arena7.bsp", "arena1.bsp")
elif defect == "dbl_missing_server_conn":
    server = [r for r in server if "SV_OnPlayerConnect: conn=7" not in r["msg"]]
elif defect == "dbl_missing_server_assign":
    server = [r for r in server if "slot 2 assigned to conn=7" not in r["msg"]]
elif defect == "dbl_missing_server_begin":
    remove_nth(server, "ClientBegin: 2")
elif defect == "dbl_missing_boundary":
    server = [r for r in server if r["msg"] != "Q0_DOUBLECLICK_PHASE_COMPLETE"]
elif defect == "dbl_missing_unprotected":
    server = [r for r in server if not re.fullmatch(r"g_needpass\s+0", r["msg"].strip())]
elif defect == "dbl_missing_quit_request":
    server = [r for r in server if r["msg"] != "Q0_DOUBLECLICK_QUIT_REQUESTED"]
elif defect == "dbl_missing_server_shutdown":
    server = [r for r in server if "Server Shutdown (Server quit)" not in r["msg"]]
elif defect == "dbl_missing_transport_shutdown":
    server = [r for r in server if r["msg"] != "QUIC transport shut down."]
elif defect == "dbl_shutdown_before_boundary":
    shutdown = next(i for i,r in enumerate(server) if "Server Shutdown (Server quit)" in r["msg"])
    row = server.pop(shutdown)
    boundary = next(i for i,r in enumerate(server) if r["msg"] == "Q0_DOUBLECLICK_PHASE_COMPLETE")
    server.insert(boundary, row)
elif defect == "dbl_shared_server":
    server += [
        {"sev":"DEBUG","cat":"server","msg":"WiredNet: listening on port 28003 (IPv4), ALPN: q3v69"},
        {"sev":"DEBUG","cat":"server","msg":"Q0_PASSWORD_SERVER_READY"},
    ]
elif defect == "dbl_server_reject":
    boundary = next(i for i,r in enumerate(server) if r["msg"] == "Q0_DOUBLECLICK_PHASE_COMPLETE")
    server.insert(boundary, {"sev":"DEBUG","cat":"server","msg":"QUIC: game rejected connection from 127.0.0.1: Invalid password"})
elif defect == "dbl_severity": p.append({"sev":"WARN","cat":"ui","msg":"synthetic"})
elif defect == "dbl_missing_layout": layout = []
elif defect == "dbl_pointer_outside":
    layout[0]["x"] = 900; layout[0]["w"] = 10

with open(product_path, "w", encoding="utf-8") as out:
    for row in p: out.write(json.dumps(row) + "\n")
    if defect == "dbl_malformed": out.write("{bad\n")
with open(layout_path, "w", encoding="utf-8") as out:
    for row in layout: out.write(json.dumps(row) + "\n")
with open(server_path, "w", encoding="utf-8") as out:
    for row in server: out.write(json.dumps(row) + "\n")
PYEOF
        if analyze_doubleclick_contract "$DIR/product" "$DIR/layout" "$DIR/server" 28001 28004 >/dev/null 2>&1; then
            echo "FAIL self-test: doubleclick defect '$defect' accepted"; exit 1
        fi
        echo "  PASS self-test: rejects $defect"
    done
    analyze_contract "$CLEAN_PRODUCT" "$CLEAN_FIXTURE" "$CLEAN_SERVER" "$CLEAN_WRONG" "$CLEAN_LAYOUT" 28001 28002 28003 >/dev/null || { echo "FAIL self-test: clean fixture rejected"; exit 1; }
    for defect in dirty_roster wrong_order no_down wrong_target no_request no_status count_zero missing_row no_cancel no_return stale_store_accepted stale_response_accepted missing_lan_unsolicited missing_lan_unsolicited_event reused_lan_challenge missing_lan_a_missing missing_lan_wrong_packet missing_lan_wrong_reject missing_lan_malformed_packet missing_lan_malformed_reject false_lan_zero missing_lan_refresh_focus missing_lan_late_packet missing_lan_late_reject missing_lan_current_packet zero_lan_rtt duplicate_lan_discovery missing_lan_duplicate wrong_lan_row missing_lan_callback missing_lan_down extra_lan_scan missing_timeout_pending wrong_pending_row missing_timeout_terminal wrong_timeout_rows wrong_timeout_status missing_timeout_focus missing_timeout_retry timeout_single_drop reused_challenge missing_timeout_stale missing_malformed_pending missing_malformed_terminal malformed_loaded wrong_malformed_reason wrong_malformed_status missing_malformed_focus missing_malformed_retry missing_malformed_stale missing_valid_pending missing_recovery recovery_count_two missing_recovery_focus missing_retry_valid wrong_connect_target no_connect_up no_connect_down no_connect_focus missing_needpass missing_preflight duplicate_preflight direct_queue_before_prompt missing_password_popup missing_empty_refusal missing_password_focus missing_password_type missing_password_commit missing_password_mask bad_password_mask missing_password_submit_focus missing_password_accept stale_password_accept reconnect_path sentinel_reconnect secret_leak missing_wrong_preflight missing_wrong_type missing_wrong_mask bad_wrong_mask missing_wrong_queue wrong_password_first wrong_secret_leak wrong_severity missing_wrong_rejection bad_wrong_rejection wrong_assigned wrong_begin missing_attempt_boundary missing_password_arm bad_password_arm_order wrong_password_needpass password_attempt_before_arm missing_layout_popup wrong_layout_order multiple_layout_focus missing_client_connect missing_client_begin rejected_password missing_connect duplicate_connect bad_close no_accept no_first wrong_first missing_server_connect missing_oob_probe accepted_oob severity server_severity malformed_product malformed_fixture malformed_wrong malformed_layout; do
        DIR="$ROOT/$defect"; mkdir -p "$DIR"
        cp "$CLEAN_SERVER" "$DIR/server"
        python3 - "$CLEAN_PRODUCT" "$CLEAN_FIXTURE" "$CLEAN_WRONG" "$CLEAN_LAYOUT" "$DIR/product" "$DIR/fixture" "$DIR/server" "$DIR/wrong" "$DIR/layout" "$defect" <<'PYEOF'
import json, sys
ps, fs, ws, ls, pd, fd, sd, wd, ld, defect = sys.argv[1:11]
p = [json.loads(x) for x in open(ps, encoding="utf-8")]
f = [json.loads(x) for x in open(fs, encoding="utf-8")]
sv = [json.loads(x) for x in open(sd, encoding="utf-8")]
w = [json.loads(x) for x in open(ws, encoding="utf-8")]
lay = [json.loads(x) for x in open(ls, encoding="utf-8")]

def remove_nth(rows, needle, ordinal):
    seen = 0
    for index, row in enumerate(rows):
        if needle in row.get("msg", ""):
            seen += 1
            if seen == ordinal:
                rows.pop(index)
                return

def mutate_last(rows, needle, old, new):
    for row in reversed(rows):
        if needle in row.get("msg", ""):
            row["msg"] = row["msg"].replace(old, new)
            return

if defect == "dirty_roster":
    row = next(r for r in p if "server roster generation=4" in r["msg"])
    row["msg"] = row["msg"].replace("displayed=2 raw=2", "displayed=3 raw=3")
elif defect == "wrong_order":
    a = next(i for i,r in enumerate(p) if "roster row=0" in r["msg"] and "A0 WIRED" in r["msg"])
    b = next(i for i,r in enumerate(p) if "roster row=1" in r["msg"] and "Z0 WIRED" in r["msg"])
    p[a]["msg"], p[b]["msg"] = p[b]["msg"], p[a]["msg"]
elif defect == "no_down": p = [r for r in p if "K_DOWNARROW" not in r["msg"]]
elif defect == "wrong_target":
    row = next(r for r in p if "server selection display_row=1" in r["msg"] and "127.0.0.1:28002" in r["msg"])
    row["msg"] = row["msg"].replace("127.0.0.1:28002", "127.0.0.1:28001")
elif defect == "no_request": p = [r for r in p if "server status request generation=" not in r["msg"]]
elif defect == "no_status": p = [r for r in p if "server status loaded generation=" not in r["msg"]]
elif defect == "count_zero":
    for r in p:
        if "registry feeder=13" in r["msg"]: r["msg"] = "WiredUI: server status registry feeder=13 count=0"
elif defect == "missing_row": p = [r for r in p if "registry row=8" not in r["msg"]]
elif defect == "no_cancel": p = [r for r in p if "status cancelled" not in r["msg"]]
elif defect == "no_return": p = [r for r in p if "focused item 'btn_connect'" not in r["msg"]]
elif defect == "stale_store_accepted": p = [r for r in p if "browser connect refused without current selection" not in r["msg"]]
elif defect == "stale_response_accepted": p = [r for r in p if "ignored stale challenge" not in r["msg"]]
elif defect == "missing_lan_unsolicited": p = [r for r in p if "Ignored unsolicited infoResponse" not in r["msg"]]
elif defect == "missing_lan_unsolicited_event": f = [r for r in f if r.get("event") != "lan_unsolicited_pre_scan"]
elif defect == "reused_lan_challenge":
    a = next(r["challenge"] for r in f if r.get("event") == "lan_scan_seen" and r.get("ordinal") == 2)
    next(r for r in f if r.get("event") == "lan_scan_seen" and r.get("ordinal") == 3)["challenge"] = a
elif defect == "missing_lan_a_missing": f = [r for r in f if r.get("event") != "lan_scan_a_missing"]
elif defect == "missing_lan_wrong_packet": f = [r for r in f if r.get("event") != "lan_scan_a_wrong_sent"]
elif defect == "missing_lan_wrong_reject": remove_nth(p, "Ignored infoResponse challenge mismatch", 1)
elif defect == "missing_lan_malformed_packet": f = [r for r in f if r.get("event") != "lan_scan_a_malformed_sent"]
elif defect == "missing_lan_malformed_reject": p = [r for r in p if "Rejected invalid infoResponse from 192.0.2.10:27960" not in r["msg"]]
elif defect == "false_lan_zero": p = [r for r in p if "source=0 displayed=0 raw=0" not in r["msg"]]
elif defect == "missing_lan_refresh_focus": p = [r for r in p if "focused item 'btn_refresh'" not in r["msg"]]
elif defect == "missing_lan_late_packet": f = [r for r in f if r.get("event") != "lan_scan_b_late_a_sent"]
elif defect == "missing_lan_late_reject": remove_nth(p, "Ignored infoResponse challenge mismatch", 2)
elif defect == "missing_lan_current_packet": f = [r for r in f if r.get("event") != "lan_scan_b_current_sent"]
elif defect == "zero_lan_rtt":
    for r in p:
        if "Accepted local discovery infoResponse" in r["msg"]: r["msg"] = r["msg"].replace("rtt=17", "rtt=0")
elif defect == "duplicate_lan_discovery":
    row = next(r.copy() for r in p if r["msg"].startswith("Accepted local discovery infoResponse generation=3"))
    p.insert(next(i for i,r in enumerate(p) if r["msg"].startswith("Accepted local discovery infoResponse generation=3")), row)
elif defect == "missing_lan_duplicate": f = [r for r in f if r.get("event") != "lan_scan_b_duplicate_sent"]
elif defect == "wrong_lan_row":
    for r in p:
        if "server roster row=0 raw=0 address=192.0.2.10:27960" in r["msg"]: r["msg"] = r["msg"].replace("map=arena7", "map=arena1")
elif defect == "missing_lan_callback": p = [r for r in p if "server selection display_row=0 raw=0 source=0" not in r["msg"]]
elif defect == "missing_lan_down": remove_nth(p, "K_DOWNARROW dispatched", 1)
elif defect == "extra_lan_scan":
    f.insert(next(i for i,r in enumerate(f) if r.get("event") == "disallowed_probe"),
             {"event":"lan_scan_seen","ordinal":4,"challenge":"4000000000000004","address":"192.0.2.10:27960"})
elif defect == "missing_timeout_pending": remove_nth(p, "server status state=pending", 1)
elif defect == "wrong_pending_row":
    for r in p:
        if "key=Status value=Contacting server..." in r["msg"]: r["msg"] = r["msg"].replace("Contacting server...", "Idle")
elif defect == "missing_timeout_terminal": p = [r for r in p if "state=no-response reason=timeout rows=2" not in r["msg"]]
elif defect == "wrong_timeout_rows":
    for r in p:
        if "state=no-response reason=timeout rows=2" in r["msg"]: r["msg"] = r["msg"].replace("rows=2", "rows=3")
elif defect == "wrong_timeout_status":
    for r in p:
        if "key=Status value=No response from server." in r["msg"]: r["msg"] = r["msg"].replace("No response from server.", "Waiting")
elif defect == "missing_timeout_focus": remove_nth(p, "focused item 'statuslist'", 2)
elif defect == "missing_timeout_retry": remove_nth(p, "focused item 'btn_retry'", 1)
elif defect == "timeout_single_drop":
    removed = False
    for index in range(len(f) - 1, -1, -1):
        if f[index].get("event") == "timeout_request":
            f.pop(index); removed = True; break
    if not removed: raise SystemExit("bad self fixture")
elif defect == "reused_challenge":
    old = next(r["challenge"] for r in f if r.get("event") == "challenge_seen" and r.get("ordinal") == 3)
    next(r for r in f if r.get("event") == "challenge_seen" and r.get("ordinal") == 4)["challenge"] = old
elif defect == "missing_timeout_stale": f = [r for r in f if r.get("event") != "sent_timeout_stale_response"]
elif defect == "missing_malformed_pending": remove_nth(p, "server status state=pending", 2)
elif defect == "missing_malformed_terminal": p = [r for r in p if "state=malformed reason=invalid-sv_maxclients rows=2" not in r["msg"]]
elif defect == "malformed_loaded":
    for r in p:
        if "state=malformed reason=invalid-sv_maxclients rows=2" in r["msg"]:
            r["msg"] = r["msg"].replace("server status failed", "server status loaded").replace(" state=malformed reason=invalid-sv_maxclients", "").replace("rows=2", "rows=9")
elif defect == "wrong_malformed_reason":
    for r in p:
        if "reason=invalid-sv_maxclients" in r["msg"]: r["msg"] = r["msg"].replace("invalid-sv_maxclients", "invalid-hostname")
elif defect == "wrong_malformed_status":
    for r in p:
        if "key=Status value=Invalid server response." in r["msg"]: r["msg"] = r["msg"].replace("Invalid server response.", "No response")
elif defect == "missing_malformed_focus": remove_nth(p, "focused item 'statuslist'", 3)
elif defect == "missing_malformed_retry": remove_nth(p, "focused item 'btn_retry'", 2)
elif defect == "missing_malformed_stale": f = [r for r in f if r.get("event") != "sent_malformed_stale_response"]
elif defect == "missing_valid_pending": remove_nth(p, "server status state=pending", 3)
elif defect == "missing_recovery":
    indices = [i for i,r in enumerate(p) if "server status loaded" in r["msg"] and "rows=9" in r["msg"]]
    if indices: p.pop(indices[-1])
elif defect == "recovery_count_two": mutate_last(p, "registry feeder=13 count=9", "count=9", "count=2")
elif defect == "missing_recovery_focus": remove_nth(p, "focused item 'statuslist'", 4)
elif defect == "missing_retry_valid": f = [r for r in f if r.get("event") != "sent_retry_valid_response"]
elif defect == "wrong_connect_target":
    for r in p:
        if "server selection display_row=1" in r["msg"] and "list_generation=8" in r["msg"]:
            r["msg"] = r["msg"].replace("127.0.0.1:28003", "127.0.0.1:28001")
elif defect == "no_connect_up": p = [r for r in p if "K_UPARROW" not in r["msg"]]
elif defect == "no_connect_down":
    indices = [i for i,r in enumerate(p) if "K_DOWNARROW" in r["msg"]]
    if indices: p.pop(indices[-1])
elif defect == "no_connect_focus":
    indices = [i for i,r in enumerate(p) if "focused item 'btn_connect'" in r["msg"]]
    if indices: p.pop(indices[-1])
elif defect == "missing_needpass":
    row = next(r for r in p if "target=127.0.0.1:28003 target_needpass=1" in r["msg"])
    row["msg"] = row["msg"].replace("target_needpass=1", "target_needpass=0")
elif defect == "missing_preflight":
    p = [r for r in p if "password required origin=browser" not in r["msg"]]
elif defect == "duplicate_preflight":
    index = next(i for i,r in enumerate(p) if "password required origin=browser" in r["msg"])
    p.insert(index, p[index].copy())
elif defect == "direct_queue_before_prompt":
    queue = next(i for i,r in enumerate(p) if "queued validated connect origin=browser" in r["msg"])
    row = p.pop(queue)
    prompt = next(i for i,r in enumerate(p) if "password required origin=browser" in r["msg"])
    p.insert(prompt, row)
elif defect == "missing_password_popup":
    p = [r for r in p if "push menu 'password'" not in r["msg"]]
elif defect == "missing_empty_refusal":
    p = [r for r in p if "password submit refused reason=invalid-credential" not in r["msg"]]
elif defect == "missing_password_focus":
    p = [r for r in p if "focused item 'row_password'" not in r["msg"]]
elif defect == "missing_password_type":
    p = [r for r in p if "typed 7 printable character(s)" not in r["msg"]]
elif defect == "missing_password_commit":
    typed = next(i for i,r in enumerate(p) if "typed 7 printable character(s)" in r["msg"])
    enter = next(i for i in range(typed + 1, len(p)) if "K_ENTER dispatched" in p[i]["msg"])
    p.pop(enter)
elif defect == "missing_password_mask":
    p = [r for r in p if "password render trace" not in r["msg"]]
elif defect == "bad_password_mask":
    row = next(r for r in p if "password render trace" in r["msg"])
    row["msg"] = row["msg"].replace("masked=1", "masked=0")
elif defect == "missing_password_submit_focus":
    accepted = next(i for i,r in enumerate(p) if "password submit accepted" in r["msg"])
    focus = max(i for i in range(accepted) if "focused item 'btn_connect'" in p[i]["msg"])
    p.pop(focus)
elif defect == "missing_password_accept":
    p = [r for r in p if "password submit accepted" not in r["msg"]]
elif defect == "stale_password_accept":
    row = next(r for r in p if "password submit accepted" in r["msg"])
    row["msg"] = row["msg"].replace("selection_generation=5", "selection_generation=4")
elif defect == "reconnect_path":
    index = next(i for i,r in enumerate(p) if "password submit accepted" in r["msg"])
    p.insert(index, {"sev":"DEBUG", "cat":"ui", "msg":"password action queued reconnect"})
elif defect == "sentinel_reconnect":
    index = next(i for i,r in enumerate(p) if "127.0.0.1:28003 resolved" in r["msg"])
    p.insert(index, {"sev":"INFO", "cat":"client",
                     "msg":"127.0.0.1:28001 resolved to 127.0.0.1:28001"})
elif defect == "secret_leak":
    p.append({"sev":"DEBUG", "cat":"ui", "msg":"credential=q0Pass7"})
elif defect == "missing_wrong_preflight":
    w = [r for r in w if "password required origin=browser" not in r["msg"]]
elif defect == "missing_wrong_type":
    w = [r for r in w if "typed 7 printable character(s)" not in r["msg"]]
elif defect == "missing_wrong_mask":
    w = [r for r in w if "password render trace" not in r["msg"]]
elif defect == "bad_wrong_mask":
    row = next(r for r in w if "password render trace" in r["msg"])
    row["msg"] = row["msg"].replace("masked=1", "masked=0")
elif defect == "missing_wrong_queue":
    w = [r for r in w if "queued validated connect origin=browser" not in r["msg"]]
elif defect == "wrong_password_first":
    w.append({"sev":"DEBUG", "cat":"client",
              "msg":"cls.state: -> CA_ACTIVE (FIRST GAMEPLAY FRAME mapname=maps/arena7.bsp)"})
elif defect == "wrong_secret_leak":
    w.append({"sev":"DEBUG", "cat":"ui", "msg":"credential=q0Wrong"})
elif defect == "wrong_severity":
    w.append({"sev":"WARN", "cat":"ui", "msg":"synthetic"})
elif defect == "missing_wrong_rejection":
    sv = [r for r in sv if "Invalid password" not in r["msg"]]
elif defect == "bad_wrong_rejection":
    row = next(r for r in sv if "Invalid password" in r["msg"])
    row["msg"] = row["msg"].replace("Invalid password", "Server is full")
elif defect == "wrong_assigned":
    boundary = next(i for i,r in enumerate(sv)
                    if r["msg"] == "Q0_WRONG_PASSWORD_PHASE_COMPLETE")
    sv.insert(boundary, {"sev":"DEBUG", "cat":"server",
                         "msg":"SV_OnPlayerConnect: slot 4 assigned to conn=11"})
elif defect == "wrong_begin":
    boundary = next(i for i,r in enumerate(sv)
                    if r["msg"] == "Q0_WRONG_PASSWORD_PHASE_COMPLETE")
    sv.insert(boundary, {"sev":"DEBUG", "cat":"server", "msg":"ClientBegin: 4"})
elif defect == "missing_attempt_boundary":
    sv = [r for r in sv if r["msg"] != "Q0_WRONG_PASSWORD_PHASE_COMPLETE"]
elif defect == "missing_password_arm":
    sv = [r for r in sv if r["msg"] != "Q0_PASSWORD_SERVER_READY"]
elif defect == "bad_password_arm_order":
    arm = next(i for i,r in enumerate(sv) if r["msg"] == "Q0_PASSWORD_SERVER_READY")
    row = sv.pop(arm)
    needpass = next(i for i,r in enumerate(sv) if r["msg"].strip().startswith("g_needpass") and r["msg"].strip().endswith("1"))
    sv.insert(needpass, row)
elif defect == "wrong_password_needpass":
    row = next(r for r in sv if r["msg"].strip().startswith("g_needpass") and r["msg"].strip().endswith("1"))
    row["msg"] = "g_needpass           0"
elif defect == "password_attempt_before_arm":
    arm = next(i for i,r in enumerate(sv) if r["msg"] == "Q0_PASSWORD_SERVER_READY")
    conn = next(i for i,r in enumerate(sv) if r["msg"] == "SV_OnPlayerConnect: conn=11")
    row = sv.pop(conn)
    sv.insert(arm, row)
elif defect == "missing_layout_popup":
    lay = [r for r in lay if r.get("region") != "password_root"]
elif defect == "wrong_layout_order":
    for row in lay:
        if row.get("region") == "row_password" and row.get("focused") == 1:
            row["frame"] = 900
elif defect == "multiple_layout_focus":
    row = next(r for r in lay if r.get("frame") == 800 and r.get("region") == "btn_connect")
    row["focused"] = 1
elif defect == "missing_client_connect":
    sv = [r for r in sv if "ClientConnect: 2" not in r["msg"]]
elif defect == "missing_client_begin":
    sv = [r for r in sv if "ClientBegin: 2" not in r["msg"]]
elif defect == "rejected_password":
    sv.insert(-2, {"sev":"INFO", "cat":"server",
                   "msg":"QUIC: game rejected connection from 127.0.0.1: Invalid password"})
elif defect == "missing_connect": p = [r for r in p if "queued validated connect origin=browser" not in r["msg"]]
elif defect == "duplicate_connect":
    row = next(r.copy() for r in p if "queued validated connect origin=browser" in r["msg"])
    p.insert(next(i for i,r in enumerate(p) if "queued validated connect origin=browser" in r["msg"]), row)
elif defect == "bad_close":
    for r in p:
        if "close all postcondition" in r["msg"]: r["msg"] = r["msg"].replace("catcher_ui=0", "catcher_ui=1")
elif defect == "no_accept": p = [r for r in p if "QUIC client: TLV ACCEPT" not in r["msg"]]
elif defect == "no_first": p = [r for r in p if "FIRST GAMEPLAY FRAME" not in r["msg"]]
elif defect == "wrong_first":
    for r in p:
        if "FIRST GAMEPLAY FRAME" in r["msg"]: r["msg"] = r["msg"].replace("arena7.bsp", "arena1.bsp")
elif defect == "missing_server_connect": sv = [r for r in sv if "SV_OnPlayerConnect" not in r["msg"]]
elif defect == "missing_oob_probe": f = [r for r in f if r.get("event") != "disallowed_probe"]
elif defect == "accepted_oob": sv.append({"sev":"DEBUG","cat":"server","msg":"SV packet 127.0.0.1 : rcon"})
elif defect == "severity": p.insert(5,{"sev":"ERROR","cat":"renderer","msg":"synthetic"})
elif defect == "server_severity": sv.append({"sev":"FATAL","cat":"server","msg":"synthetic"})
with open(pd,"w",encoding="utf-8") as out:
    for r in p: out.write(json.dumps(r)+"\n")
    if defect == "malformed_product": out.write("{bad\n")
with open(fd,"w",encoding="utf-8") as out:
    for r in f: out.write(json.dumps(r)+"\n")
    if defect == "malformed_fixture": out.write("[]\n")
with open(sd,"w",encoding="utf-8") as out:
    for r in sv: out.write(json.dumps(r)+"\n")
with open(wd,"w",encoding="utf-8") as out:
    for r in w: out.write(json.dumps(r)+"\n")
    if defect == "malformed_wrong": out.write("{bad\n")
with open(ld,"w",encoding="utf-8") as out:
    for r in lay: out.write(json.dumps(r)+"\n")
    if defect == "malformed_layout": out.write("{bad\n")
PYEOF
        if analyze_contract "$DIR/product" "$DIR/fixture" "$DIR/server" "$DIR/wrong" "$DIR/layout" 28001 28002 28003 >/dev/null 2>&1; then
            echo "FAIL self-test: defect '$defect' accepted"; exit 1
        fi
        echo "  PASS self-test: rejects $defect"
    done
    echo "==> WiredUI server-browser analyzer self-test: PASS"
    exit 0
fi

if ! command -v python3 >/dev/null 2>&1 || [ ! -f "$TIMEOUT_RUNNER" ] || [ ! -f "$FIXTURE" ]; then
    echo "SKIP: Python 3 fixture/watchdog unavailable"; exit 77
fi
WIRED="${1:-}"
if [ -z "$WIRED" ] || [ ! -x "$WIRED" ]; then echo "SKIP: pass an assembled Wired GUI binary"; exit 77; fi
WIRED="$(cd "$(dirname "$WIRED")" && pwd)/$(basename "$WIRED")"
WIRED_DIR="$(dirname "$WIRED")"
HEADLESS="${WIRED_HEADLESS:-}"
if [ -z "$HEADLESS" ]; then
    gui_name="$(basename "$WIRED")"; suffix="${gui_name#wired}"
    for candidate in "$WIRED_DIR/wired-headless$suffix" "$WIRED_DIR/../../../wired-headless$suffix" \
        "$WIRED_DIR/../../../wired-headless.arm64" "$WIRED_DIR/wired-headless.arm64"; do
        if [ -x "$candidate" ]; then HEADLESS="$candidate"; break; fi
    done
fi
[ -n "$HEADLESS" ] && [ -x "$HEADLESS" ] || { echo "SKIP: sibling wired-headless missing"; exit 77; }
HEADLESS="$(cd "$(dirname "$HEADLESS")" && pwd)/$(basename "$HEADLESS")"
PACK_ROOT=""
for candidate in "$WIRED_DIR" "$WIRED_DIR/../Resources" "$WIRED_DIR/../../.."; do
    if [ -f "$candidate/base/pax21.sw3z" ]; then PACK_ROOT="$(cd "$candidate" && pwd)"; break; fi
done
[ -n "$PACK_ROOT" ] || { echo "SKIP: current pax21 not found"; exit 77; }
CONTENT_ROOT="${WIRED_CONTENT_ROOT:-}"
for candidate in "$CONTENT_ROOT" "$PACK_ROOT"; do
    if [ -f "$candidate/base/pax01.sw3z" ] || [ -f "$candidate/base/pak0.pk3" ]; then CONTENT_ROOT="$candidate"; break; fi
done
if [ -f "$CONTENT_ROOT/base/pax01.sw3z" ]; then BASE_ARCHIVE="$CONTENT_ROOT/base/pax01.sw3z"
elif [ -f "$CONTENT_ROOT/base/pak0.pk3" ]; then BASE_ARCHIVE="$CONTENT_ROOT/base/pak0.pk3"
else echo "SKIP: licensed base archive missing; set WIRED_CONTENT_ROOT"; exit 77; fi

RUN_ROOT="$(mktemp -d -t wired-q0browser-XXXXXX 2>/dev/null || mktemp -d)"
HOME_ROOT="$RUN_ROOT/q3now-preview"
DOUBLE_HOME="$RUN_ROOT/double/q3now-preview"
DOUBLE_SERVER_HOME="$RUN_ROOT/double-server/q3now-preview"
WRONG_HOME="$RUN_ROOT/wrong/q3now-preview"
SERVER_HOME="$RUN_ROOT/server/q3now-preview"
PRODUCT_JSON="$HOME_ROOT/qconsole.jsonl"
DOUBLE_JSON="$DOUBLE_HOME/qconsole.jsonl"
DOUBLE_SERVER_JSON="$DOUBLE_SERVER_HOME/qconsole.jsonl"
WRONG_JSON="$WRONG_HOME/qconsole.jsonl"
SERVER_JSON="$SERVER_HOME/qconsole.jsonl"
PRODUCT_STDOUT="$RUN_ROOT/product.stdout"
DOUBLE_STDOUT="$RUN_ROOT/double.stdout"
DOUBLE_SERVER_STDOUT="$RUN_ROOT/double-server.stdout"
WRONG_STDOUT="$RUN_ROOT/wrong.stdout"
LAYOUT_JSON="$RUN_ROOT/layoutdump.jsonl"
DOUBLE_LAYOUT_JSON="$RUN_ROOT/double-run/layoutdump.jsonl"
FIXTURE_JSON="$RUN_ROOT/fixture.jsonl"
FIXTURE_PID=""
SERVER_PID=""
DOUBLE_SERVER_PID=""
SERVER_CONTROL="$RUN_ROOT/server.stdin"
DOUBLE_SERVER_CONTROL="$RUN_ROOT/double-server.stdin"
SERVER_CONTROL_OPEN=0
DOUBLE_SERVER_CONTROL_OPEN=0
DOUBLE_SERVER_FORCED=0
cleanup() {
    if [ -n "$FIXTURE_PID" ] && kill -0 "$FIXTURE_PID" 2>/dev/null; then kill -TERM "$FIXTURE_PID" 2>/dev/null || true; wait "$FIXTURE_PID" 2>/dev/null || true; fi
    if [ -n "$DOUBLE_SERVER_PID" ] && kill -0 "$DOUBLE_SERVER_PID" 2>/dev/null; then
        [ "$DOUBLE_SERVER_CONTROL_OPEN" -eq 1 ] && printf '%s\n' quit >&8 2>/dev/null || true
        for _ in $(seq 1 150); do kill -0 "$DOUBLE_SERVER_PID" 2>/dev/null || break; sleep 0.1; done
        kill -0 "$DOUBLE_SERVER_PID" 2>/dev/null && kill -TERM "$DOUBLE_SERVER_PID" 2>/dev/null || true
        wait "$DOUBLE_SERVER_PID" 2>/dev/null || true
    fi
    if [ -n "$SERVER_PID" ] && kill -0 "$SERVER_PID" 2>/dev/null; then
        [ "$SERVER_CONTROL_OPEN" -eq 1 ] && printf '%s\n' quit >&9 2>/dev/null || true
        for _ in $(seq 1 150); do kill -0 "$SERVER_PID" 2>/dev/null || break; sleep 0.1; done
        kill -0 "$SERVER_PID" 2>/dev/null && kill -TERM "$SERVER_PID" 2>/dev/null || true
        wait "$SERVER_PID" 2>/dev/null || true
    fi
    [ "$SERVER_CONTROL_OPEN" -eq 1 ] && exec 9>&- || true
    [ "$DOUBLE_SERVER_CONTROL_OPEN" -eq 1 ] && exec 8>&- || true
    if [ "${WIRED_KEEP_ARTIFACTS:-0}" = 1 ]; then echo "    kept artifacts: $RUN_ROOT"; else rm -rf "$RUN_ROOT"; fi
}
trap cleanup EXIT INT TERM
mkdir -p "$HOME_ROOT/base" "$DOUBLE_HOME/base" "$DOUBLE_SERVER_HOME/base" \
    "$WRONG_HOME/base" "$SERVER_HOME/base" \
    "$RUN_ROOT/double-run" "$RUN_ROOT/wrong-run"
cp "$BASE_ARCHIVE" "$HOME_ROOT/base/" || exit 1
cp "$PACK_ROOT/base/pax21.sw3z" "$HOME_ROOT/base/pax21.sw3z" || exit 1
cp "$BASE_ARCHIVE" "$DOUBLE_HOME/base/" || exit 1
cp "$PACK_ROOT/base/pax21.sw3z" "$DOUBLE_HOME/base/pax21.sw3z" || exit 1
cp "$BASE_ARCHIVE" "$DOUBLE_SERVER_HOME/base/" || exit 1
cp "$PACK_ROOT/base/pax21.sw3z" "$DOUBLE_SERVER_HOME/base/pax21.sw3z" || exit 1
cp "$BASE_ARCHIVE" "$WRONG_HOME/base/" || exit 1
cp "$PACK_ROOT/base/pax21.sw3z" "$WRONG_HOME/base/pax21.sw3z" || exit 1
cp "$BASE_ARCHIVE" "$SERVER_HOME/base/" || exit 1
cp "$PACK_ROOT/base/pax21.sw3z" "$SERVER_HOME/base/pax21.sw3z" || exit 1
python3 - "$HOME_ROOT/base/wired_ui_state.dat" <<'PYEOF'
import struct, sys
entries = [
    (b"ui_selectedServerAddr", b"127.0.0.1:65534"),
    (b"ui_selectedServerName", b"STALE PREVIOUS SESSION"),
]
with open(sys.argv[1], "wb") as out:
    out.write(struct.pack("@iii", 0x57554953, 1, len(entries)))
    for key, value in entries:
        out.write(struct.pack("@HH", len(key), len(value)))
        out.write(key)
        out.write(value)
PYEOF
cat >"$SERVER_HOME/base/q0browser-server.cfg" <<'CFGEOF'
set g_password q0Pass7
map arena7
wait 300
addbot visor 3 free 0 StatusBot
CFGEOF
cat >"$DOUBLE_SERVER_HOME/base/q0browser-double-server.cfg" <<'CFGEOF'
set g_password ""
map arena7
wait 300
addbot visor 3 free 0 StatusBot
CFGEOF

read -r DOUBLE_CLIENT_PORT DOUBLE_SERVER_PORT WRONG_CLIENT_PORT CLIENT_PORT SENTINEL_PORT TARGET_PORT SERVER_PORT <<EOF
$(python3 - <<'PYEOF'
import socket
s=[]
for _ in range(7):
    while True:
        x=socket.socket(socket.AF_INET,socket.SOCK_DGRAM); x.bind(("127.0.0.1",0))
        if not 27960 <= x.getsockname()[1] <= 27963: break
        x.close()
    s.append(x)
print(*(x.getsockname()[1] for x in s))
for x in s: x.close()
PYEOF
)
EOF
case "$(uname -s)" in
 MINGW*|MSYS*|CYGWIN*) DOUBLE_SERVER_NATIVE="$(cygpath -w "$DOUBLE_SERVER_HOME")" ;;
 *) DOUBLE_SERVER_NATIVE="$DOUBLE_SERVER_HOME" ;;
esac
mkfifo "$DOUBLE_SERVER_CONTROL"; exec 8<>"$DOUBLE_SERVER_CONTROL"; DOUBLE_SERVER_CONTROL_OPEN=1
( cd "$(dirname "$HEADLESS")" && exec "$HEADLESS" +set fs_homepath "$DOUBLE_SERVER_NATIVE" \
    +set com_automated 1 +set com_noHardReboot 1 +set log_severity DEBUG +set log_file_severity DEBUG \
    +set log_file_mode overwrite_synced +set net_ip 127.0.0.1 +set net_port "$DOUBLE_SERVER_PORT" \
    +set sv_hostname "Z0 WIRED Q0 TARGET" +set sv_pure 0 +set g_autoBots 0 \
    +exec q0browser-double-server.cfg <&8 ) >"$DOUBLE_SERVER_STDOUT" 2>&1 &
DOUBLE_SERVER_PID=$!
if ! python3 - "$DOUBLE_SERVER_JSON" "$DOUBLE_SERVER_PID" <<'PYEOF'
import json, os, sys, time
path, pid = sys.argv[1], int(sys.argv[2]); deadline=time.monotonic()+60
while time.monotonic()<deadline:
    try: os.kill(pid,0)
    except ProcessLookupError: raise SystemExit("FAIL: double headless exited before arena7")
    try:
        rows=[json.loads(x) for x in open(path,encoding="utf-8") if x.strip()]
    except (OSError,ValueError): rows=[]
    init = any("InitGame:" in str(r.get("msg","")) and "\\mapname\\arena7" in str(r.get("msg","")) for r in rows)
    bot = any("StatusBot has entered the game" in str(r.get("msg","")) for r in rows)
    if init and bot: break
    time.sleep(.1)
else: raise SystemExit("FAIL: double headless arena7/StatusBot readiness timeout")
PYEOF
then exit 1; fi
printf '%s\n' serverinfo >&8
if ! python3 - "$DOUBLE_SERVER_JSON" <<'PYEOF'
import json, re, sys, time
path = sys.argv[1]; deadline = time.monotonic() + 5
while time.monotonic() < deadline:
    try:
        rows = [json.loads(line) for line in open(path, encoding="utf-8") if line.strip()]
    except (OSError, ValueError):
        rows = []
    if any(re.fullmatch(r"g_needpass\s+0", str(row.get("msg", "")).strip())
           for row in rows):
        break
    time.sleep(.05)
else:
    raise SystemExit("FAIL: double headless did not expose authoritative g_needpass=0")
PYEOF
then exit 1; fi

cat >"$DOUBLE_HOME/base/q0browser-double.cfg" <<CFGEOF
set activeAction "wait 60 ; quit"
set com_maxfps 60
set password ""
wait 100
wui_server_fixture $SENTINEL_PORT $DOUBLE_SERVER_PORT
wui_push servers
wait 60
wui_listbox_sort 0
wait 20
set r_layoutDump 1
wait 5
wui_menu_nav focus serverlist
echo Q0_DBL_ENTER_BEGIN
wui_menu_nav enter
wui_menu_nav enter
echo Q0_DBL_ENTER_END

echo Q0_DBL_SINGLE_BEGIN
wui_pointer_listbox serverlist 1
wait 2
wui_pointer_click
echo Q0_DBL_SINGLE_END
wui_push servers
wait 2

echo Q0_DBL_MISSING_RELEASE_BEGIN
wui_pointer_listbox serverlist 1
wait 2
wui_pointer_button down
wait 2
wui_pointer_button down
wait 1
wui_pointer_button up
echo Q0_DBL_MISSING_RELEASE_END
wui_push servers
wait 2

echo Q0_DBL_TIMEOUT_BEGIN
wui_pointer_listbox serverlist 1
wait 2
wui_pointer_click
wait 30
wui_pointer_listbox serverlist 1
wait 2
wui_pointer_click
echo Q0_DBL_TIMEOUT_END
wui_push servers
wait 2

echo Q0_DBL_DIFFERENT_ROW_BEGIN
wui_pointer_listbox serverlist 0
wait 2
wui_pointer_click
wui_pointer_listbox serverlist 1
wait 2
wui_pointer_click
echo Q0_DBL_DIFFERENT_ROW_END
wui_push servers
wait 2

echo Q0_DBL_LIFECYCLE_BEGIN
wui_pointer_listbox serverlist 1
wait 2
wui_pointer_click
wui_menu_nav back
wait 2
wui_push servers
wait 10
wui_listbox_sort 0
wui_listbox_sort 0
wait 5
wui_pointer_listbox serverlist 1
wait 2
wui_pointer_click
echo Q0_DBL_LIFECYCLE_END
wui_push servers
wait 2

echo Q0_DBL_GENERATION_BEGIN
wui_pointer_listbox serverlist 1
wait 2
wui_pointer_click
wui_server_fixture $SENTINEL_PORT $DOUBLE_SERVER_PORT
wui_listbox_sort 0
wui_listbox_sort 0
wait 5
wui_pointer_listbox serverlist 1
wait 2
wui_pointer_click
echo Q0_DBL_GENERATION_END
wui_push servers
wait 2

echo Q0_DBL_POSITIVE_BEGIN
wui_pointer_listbox serverlist 1
wait 2
wui_pointer_click
wui_pointer_listbox serverlist 1
wait 2
wui_pointer_click
echo Q0_DBL_POSITIVE_END
CFGEOF
case "$(uname -s)" in
 Darwin) DOUBLE_NATIVE="$DOUBLE_HOME"; DOUBLE_PLATFORM_ARGS=( -ApplePersistenceIgnoreState YES ) ;;
 MINGW*|MSYS*|CYGWIN*) DOUBLE_NATIVE="$(cygpath -w "$DOUBLE_HOME")"; DOUBLE_PLATFORM_ARGS=() ;;
 *) DOUBLE_NATIVE="$DOUBLE_HOME"; DOUBLE_PLATFORM_ARGS=() ;;
esac
echo "==> WiredUI real Browser double-click: negative matrix then arena7 positive"
python3 "$TIMEOUT_RUNNER" --timeout 75 --kill-after 10 --cwd "$RUN_ROOT/double-run" --stdout "$DOUBLE_STDOUT" -- \
    "$WIRED" "${DOUBLE_PLATFORM_ARGS[@]}" +set fs_homepath "$DOUBLE_NATIVE" +set com_automated 1 \
    +set com_noHardReboot 1 +set net_enabled 1 +set net_ip 0.0.0.0 +set net_port "$DOUBLE_CLIENT_PORT" \
    +set wn_cert_verify 0 \
    +set s_initsound 0 +set r_fullscreen 0 +set r_mode -1 +set r_customwidth 1280 +set r_customheight 720 \
    +set log_severity DEBUG +set log_file_severity DEBUG +set log_file_mode overwrite_synced \
    +exec q0browser-double.cfg
double_rc=$?
if [ "$double_rc" -ne 0 ] || [ ! -s "$DOUBLE_JSON" ] || [ ! -s "$DOUBLE_LAYOUT_JSON" ]; then
    echo "FAIL: double-click product rc=$double_rc or missing JSONL/layout evidence"
    [ -s "$DOUBLE_STDOUT" ] && tail -40 "$DOUBLE_STDOUT"
    exit 1
fi
printf '%s\n' 'echo Q0_DOUBLECLICK_PHASE_COMPLETE' >&8
if ! python3 - "$DOUBLE_SERVER_JSON" <<'PYEOF'
import json, sys, time
path = sys.argv[1]; deadline = time.monotonic() + 5
while time.monotonic() < deadline:
    try:
        rows = [json.loads(line) for line in open(path, encoding="utf-8") if line.strip()]
    except (OSError, ValueError):
        rows = []
    if any(str(row.get("msg", "")).strip() == "Q0_DOUBLECLICK_PHASE_COMPLETE"
           for row in rows):
        break
    time.sleep(.05)
else:
    raise SystemExit("FAIL: double headless did not record phase boundary")
PYEOF
then exit 1; fi
printf '%s\n' 'echo Q0_DOUBLECLICK_QUIT_REQUESTED' quit >&8
for _ in $(seq 1 150); do kill -0 "$DOUBLE_SERVER_PID" 2>/dev/null || break; sleep 0.1; done
if kill -0 "$DOUBLE_SERVER_PID" 2>/dev/null; then
    DOUBLE_SERVER_FORCED=1
    echo "FAIL: double headless required forced teardown"
    exit 1
fi
if ! wait "$DOUBLE_SERVER_PID"; then
    echo "FAIL: double headless exit was nonzero"
    exit 1
fi
DOUBLE_SERVER_PID=""
exec 8>&-
DOUBLE_SERVER_CONTROL_OPEN=0
if [ "$DOUBLE_SERVER_FORCED" -ne 0 ]; then
    echo "FAIL: double headless teardown was forced"
    exit 1
fi
if ! analyze_doubleclick_contract "$DOUBLE_JSON" "$DOUBLE_LAYOUT_JSON" \
        "$DOUBLE_SERVER_JSON" "$SENTINEL_PORT" "$DOUBLE_SERVER_PORT"; then exit 1; fi

case "$(uname -s)" in
 MINGW*|MSYS*|CYGWIN*) SERVER_NATIVE="$(cygpath -w "$SERVER_HOME")" ;;
 *) SERVER_NATIVE="$SERVER_HOME" ;;
esac
mkfifo "$SERVER_CONTROL"; exec 9<>"$SERVER_CONTROL"; SERVER_CONTROL_OPEN=1
( cd "$(dirname "$HEADLESS")" && exec "$HEADLESS" +set fs_homepath "$SERVER_NATIVE" \
    +set com_automated 1 +set com_noHardReboot 1 +set log_severity DEBUG +set log_file_severity DEBUG \
    +set log_file_mode overwrite_synced +set net_ip 127.0.0.1 +set net_port "$SERVER_PORT" \
    +set sv_hostname "Z0 WIRED Q0 TARGET" +set sv_pure 0 +set g_autoBots 0 \
    +exec q0browser-server.cfg <&9 ) >"$RUN_ROOT/server.stdout" 2>&1 &
SERVER_PID=$!
if ! python3 - "$SERVER_JSON" "$SERVER_PID" <<'PYEOF'
import json, os, sys, time
path, pid = sys.argv[1], int(sys.argv[2]); deadline=time.monotonic()+60
while time.monotonic()<deadline:
    try: os.kill(pid,0)
    except ProcessLookupError: raise SystemExit("FAIL: protected headless exited before arena7")
    try:
        rows=[json.loads(x) for x in open(path,encoding="utf-8") if x.strip()]
    except (OSError,ValueError): rows=[]
    init = any("InitGame:" in str(r.get("msg","")) and "\\mapname\\arena7" in str(r.get("msg","")) for r in rows)
    bot = any("StatusBot has entered the game" in str(r.get("msg","")) for r in rows)
    if init and bot: break
    time.sleep(.1)
else: raise SystemExit("FAIL: protected headless arena7/StatusBot readiness timeout")
PYEOF
then exit 1; fi
printf '%s\n' serverinfo 'echo Q0_PASSWORD_SERVER_READY' >&9
if ! python3 - "$SERVER_JSON" <<'PYEOF'
import json, re, sys, time
path = sys.argv[1]; deadline = time.monotonic() + 5
while time.monotonic() < deadline:
    try:
        messages = [str(json.loads(line).get("msg", "")).strip()
                    for line in open(path, encoding="utf-8") if line.strip()]
    except (OSError, ValueError):
        messages = []
    needpass = [i for i,msg in enumerate(messages)
                if re.fullmatch(r"g_needpass\s+1", msg)]
    ready = [i for i,msg in enumerate(messages) if msg == "Q0_PASSWORD_SERVER_READY"]
    if len(needpass) == 1 and len(ready) == 1 and needpass[0] < ready[0]:
        break
    time.sleep(.05)
else:
    raise SystemExit("FAIL: fresh protected server did not expose ordered g_needpass=1 readiness")
PYEOF
then exit 1; fi

cat >"$WRONG_HOME/base/q0browser-wrong.cfg" <<CFGEOF
set com_maxfps 60
set password ""
set cl_reconnectArgs "127.0.0.1:$SENTINEL_PORT"
wait 100
wui_push servers
wait 30
wui_server_fixture $SENTINEL_PORT $SERVER_PORT 1
wait 30
wui_listbox_sort 0
wait 20
wui_menu_nav focus serverlist
wui_menu_nav down
wait 10
wui_menu_nav focus btn_connect
wui_menu_nav enter
wait 10
wui_menu_nav focus row_password
wui_menu_nav enter
wui_menu_nav type "q0Wrong"
wait 5
wui_menu_nav enter
wui_password_trace
wui_menu_nav focus btn_connect
wui_menu_nav enter
CFGEOF
case "$(uname -s)" in
 Darwin) WRONG_NATIVE="$WRONG_HOME"; WRONG_PLATFORM_ARGS=( -ApplePersistenceIgnoreState YES ) ;;
 MINGW*|MSYS*|CYGWIN*) WRONG_NATIVE="$(cygpath -w "$WRONG_HOME")"; WRONG_PLATFORM_ARGS=() ;;
 *) WRONG_NATIVE="$WRONG_HOME"; WRONG_PLATFORM_ARGS=() ;;
esac
echo "==> WiredUI wrong-password negative control: server=$SERVER_PORT watchdog=25s (rc124 expected)"
python3 "$TIMEOUT_RUNNER" --timeout 25 --kill-after 5 --cwd "$RUN_ROOT/wrong-run" --stdout "$WRONG_STDOUT" -- \
    "$WIRED" "${WRONG_PLATFORM_ARGS[@]}" +set fs_homepath "$WRONG_NATIVE" +set com_automated 1 \
    +set com_noHardReboot 1 +set net_enabled 1 +set net_ip 0.0.0.0 +set net_port "$WRONG_CLIENT_PORT" \
    +set wn_cert_verify 0 \
    +set s_initsound 0 +set r_fullscreen 0 +set r_mode -1 +set r_customwidth 1280 +set r_customheight 720 \
    +set log_severity DEBUG +set log_file_severity DEBUG +set log_file_mode overwrite_synced \
    +exec q0browser-wrong.cfg
wrong_rc=$?
if [ "$wrong_rc" -ne 124 ] || [ ! -s "$WRONG_JSON" ]; then
    echo "FAIL: wrong-password negative control rc=$wrong_rc (expected bounded watchdog rc124) or missing JSONL"
    [ -s "$WRONG_STDOUT" ] && tail -40 "$WRONG_STDOUT"
    exit 1
fi
if ! python3 - "$SERVER_JSON" "$WRONG_JSON" <<'PYEOF'
import json, sys
server_path, client_path = sys.argv[1:3]
def messages(path):
    with open(path, encoding="utf-8", errors="replace") as stream:
        return [json.loads(line).get("msg", "") for line in stream if line.strip()]
server = messages(server_path)
client = messages(client_path)
if not any("QUIC: game rejected connection" in msg and msg.strip().endswith(": Invalid password")
           for msg in server):
    raise SystemExit("FAIL: watchdog expired without authoritative Invalid password rejection")
if any("FIRST GAMEPLAY FRAME" in msg for msg in client):
    raise SystemExit("FAIL: wrong-password negative control reached gameplay")
PYEOF
then exit 1; fi
printf '%s\n' 'echo Q0_WRONG_PASSWORD_PHASE_COMPLETE' >&9
if ! python3 - "$SERVER_JSON" <<'PYEOF'
import json, sys, time
path = sys.argv[1]; deadline = time.monotonic() + 5
while time.monotonic() < deadline:
    try:
        rows = [json.loads(line) for line in open(path, encoding="utf-8") if line.strip()]
    except (OSError, ValueError):
        rows = []
    if any(str(row.get("msg", "")).strip() == "Q0_WRONG_PASSWORD_PHASE_COMPLETE"
           for row in rows):
        break
    time.sleep(.05)
else:
    raise SystemExit("FAIL: headless did not record wrong/correct phase boundary")
PYEOF
then exit 1; fi

python3 "$FIXTURE" --client-port "$CLIENT_PORT" --sentinel-port "$SENTINEL_PORT" \
    --target-port "$TARGET_PORT" --upstream-port "$SERVER_PORT" --lan-discovery \
    --protocol 74 --events "$FIXTURE_JSON" --timeout 180 &
FIXTURE_PID=$!
for _ in $(seq 1 50); do [ -s "$FIXTURE_JSON" ] && break; sleep 0.1; done
[ -s "$FIXTURE_JSON" ] || { echo "FAIL: fixture did not become ready"; exit 1; }

cat >"$HOME_ROOT/base/q0browser.cfg" <<CFGEOF
set activeAction "wait 60 ; quit"
set com_maxfps 60
set password ""
set cl_reconnectArgs "127.0.0.1:$SENTINEL_PORT"
wait 100
wui_push servers
wui_menu_nav focus btn_connect
wui_menu_nav enter
wait 5
wui_menu_nav back
wait 5
wui_push servers
wait 180
wui_menu_nav focus btn_filter
wui_menu_nav enter
wait 10
wui_menu_nav focus btn_refresh
wui_menu_nav enter
wait 180
wui_menu_nav focus serverlist
wui_menu_nav down
wait 10
wui_menu_nav back
wait 10
wui_server_fixture $SENTINEL_PORT $TARGET_PORT
wui_push servers
wait 60
wui_listbox_sort 0
wait 30
wui_menu_nav focus serverlist
wui_menu_nav down
wait 10
wui_menu_nav focus btn_info
wui_menu_nav enter
wait 30
wui_menu_nav focus btn_back
wui_menu_nav enter
wait 20
wui_menu_nav focus btn_info
wui_menu_nav enter
wait 300
wui_serverstatus_trace
wui_menu_nav focus statuslist
wait 5
wui_menu_nav focus btn_back
wui_menu_nav enter
wait 20
wui_menu_nav focus btn_info
wui_menu_nav enter
wui_serverstatus_trace
wait 360
wui_serverstatus_trace
wui_menu_nav focus statuslist
wui_menu_nav focus btn_retry
wui_menu_nav enter
wait 120
wui_serverstatus_trace
wui_menu_nav focus statuslist
wui_menu_nav focus btn_retry
wui_menu_nav enter
wait 120
wui_serverstatus_trace
wui_menu_nav focus statuslist
wui_menu_nav focus btn_back
wui_menu_nav enter
wait 20
wui_menu_nav focus btn_connect
wui_server_fixture $SENTINEL_PORT $SERVER_PORT 1
wait 30
wui_menu_nav focus serverlist
wui_menu_nav up
wui_menu_nav down
wait 10
wui_menu_nav focus btn_connect
wui_menu_nav enter
wait 10
set r_layoutDump 1
wait 5
wui_menu_nav focus btn_connect
wui_menu_nav enter
wait 5
wui_menu_nav focus row_password
wui_menu_nav enter
wui_menu_nav type "q0Pass7"
wait 5
wui_menu_nav enter
wui_password_trace
wui_menu_nav focus btn_connect
wait 5
set r_layoutDump 0
wui_menu_nav enter
CFGEOF

case "$(uname -s)" in
 Darwin) NATIVE_HOME="$HOME_ROOT"; PLATFORM_ARGS=( -ApplePersistenceIgnoreState YES ) ;;
 MINGW*|MSYS*|CYGWIN*) NATIVE_HOME="$(cygpath -w "$HOME_ROOT")"; PLATFORM_ARGS=() ;;
 *) NATIVE_HOME="$HOME_ROOT"; PLATFORM_ARGS=() ;;
esac

echo "==> WiredUI live loopback browser/status: sentinel=$SENTINEL_PORT target=$TARGET_PORT upstream=$SERVER_PORT"
python3 "$TIMEOUT_RUNNER" --timeout 180 --kill-after 15 --cwd "$RUN_ROOT" --stdout "$PRODUCT_STDOUT" -- \
    "$WIRED" "${PLATFORM_ARGS[@]}" +set fs_homepath "$NATIVE_HOME" +set com_automated 1 \
    +set com_noHardReboot 1 +set net_enabled 1 +set net_ip 0.0.0.0 +set net_port "$CLIENT_PORT" \
    +set wn_cert_verify 0 \
    +set s_initsound 0 +set r_fullscreen 0 +set r_mode -1 +set r_customwidth 1280 +set r_customheight 720 \
    +set log_severity DEBUG +set log_file_severity DEBUG +set log_file_mode overwrite_synced +exec q0browser.cfg
rc=$?
if [ "$rc" -ne 0 ] || [ ! -s "$PRODUCT_JSON" ] || [ ! -s "$LAYOUT_JSON" ]; then
    echo "FAIL: product rc=$rc or missing JSONL/layout evidence"; [ -s "$PRODUCT_STDOUT" ] && tail -40 "$PRODUCT_STDOUT"; exit 1
fi
kill -TERM "$FIXTURE_PID" 2>/dev/null || true
wait "$FIXTURE_PID" || { echo "FAIL: fixture exit was nonzero"; exit 1; }
FIXTURE_PID=""
printf '%s\n' quit >&9
for _ in $(seq 1 150); do kill -0 "$SERVER_PID" 2>/dev/null || break; sleep 0.1; done
if kill -0 "$SERVER_PID" 2>/dev/null; then echo "FAIL: headless did not exit after console quit"; exit 1; fi
if ! wait "$SERVER_PID"; then echo "FAIL: headless exit was nonzero"; exit 1; fi
SERVER_PID=""; exec 9>&-; SERVER_CONTROL_OPEN=0
for evidence in "$PRODUCT_JSON" "$DOUBLE_JSON" "$WRONG_JSON" "$SERVER_JSON" "$DOUBLE_SERVER_JSON" \
        "$LAYOUT_JSON" "$DOUBLE_LAYOUT_JSON" "$PRODUCT_STDOUT" "$DOUBLE_STDOUT" \
        "$DOUBLE_SERVER_STDOUT" "$WRONG_STDOUT" "$RUN_ROOT/server.stdout"; do
    for secret in q0Wrong q0Pass7; do
        if [ -f "$evidence" ] && LC_ALL=C grep -Fq "$secret" "$evidence"; then
            echo "FAIL: password bytes leaked into evidence: $evidence"; exit 1
        fi
    done
done
if ! analyze_doubleclick_contract "$DOUBLE_JSON" "$DOUBLE_LAYOUT_JSON" "$DOUBLE_SERVER_JSON" \
        "$SENTINEL_PORT" "$DOUBLE_SERVER_PORT"; then exit 1; fi
if ! analyze_contract "$PRODUCT_JSON" "$FIXTURE_JSON" "$SERVER_JSON" "$WRONG_JSON" "$LAYOUT_JSON" \
        "$SENTINEL_PORT" "$TARGET_PORT" "$SERVER_PORT"; then exit 1; fi
echo "==> WiredUI server-browser/status gate: PASS"
