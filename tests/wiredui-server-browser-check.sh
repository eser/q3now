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
. "$SCRIPT_DIR/lib/wired_paths.sh"
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
product_cats = [row["cat"].lower() for row in product]
product_sevs = [row["sev"].upper() for row in product]
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
           or "Browser connect attempt armed " in msg
           or "started validated connect origin=browser" in msg
           or re.fullmatch(rf"{re.escape(target)} resolved to {re.escape(target)}\n?", msg)
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
    ("positive armed", rf"Browser connect attempt armed target={e_target} selection_generation=[1-9][0-9]* credential_present=0"),
    ("positive resolution", rf"{e_target} resolved to {e_target}"),
    ("positive started", rf"WiredUI: started validated connect origin=browser address={e_target} selection_generation=[1-9][0-9]* credential_present=0"),
    ("terminal pointer release", r"WiredUI: pointer phase=release reason=close-all was_down=0 pointer_down=0"),
    ("positive close", r"WiredUI: close all postcondition depth=0 active=none catcher_ui=0 paused=0"),
    ("positive end", r"^Q0_DBL_POSITIVE_END$"),
    ("positive accept", r"QUIC client: TLV ACCEPT received"),
    ("positive FIRST", r"FIRST GAMEPLAY FRAME mapname=maps/arena7\.bsp\b"),
    ("shutdown", r"WiredUI: shutdown"),
])

if sum("listbox click phase=double" in msg for msg in messages) != 1:
    raise SystemExit("FAIL doubleclick: expected exactly one accepted gesture")
armed_rows=[(i,re.fullmatch(rf"Browser connect attempt armed target={e_target} selection_generation=([1-9][0-9]*) credential_present=0\n?",msg))
            for i,msg in enumerate(messages)]
armed_rows=[row for row in armed_rows if row[1]]
started_rows=[(i,re.fullmatch(rf"WiredUI: started validated connect origin=browser address={e_target} selection_generation=([1-9][0-9]*) credential_present=0\n?",msg))
              for i,msg in enumerate(messages)]
started_rows=[row for row in started_rows if row[1]]
resolve_rows=[i for i,msg in enumerate(messages)
              if re.fullmatch(rf"{e_target} resolved to {e_target}\n?",msg)]
if len(armed_rows)!=1 or len(started_rows)!=1 or len(resolve_rows)!=1 \
        or armed_rows[0][1].group(1)!=started_rows[0][1].group(1) \
        or (product_sevs[armed_rows[0][0]],product_cats[armed_rows[0][0]]) != ("DEBUG","client") \
        or (product_sevs[started_rows[0][0]],product_cats[started_rows[0][0]]) != ("DEBUG","ui") \
        or (product_sevs[resolve_rows[0]],product_cats[resolve_rows[0]]) != ("INFO","client"):
    raise SystemExit("FAIL doubleclick: current browser connect ABI identity/metadata")
for prefix,count in (("Browser connect attempt armed ",1),
                     ("WiredUI: started validated connect ",1)):
    if sum(msg.startswith(prefix) for msg in messages)!=count:
        raise SystemExit(f"FAIL doubleclick: additive current marker {prefix}")
if sum(" resolved to " in msg for msg in messages)!=1:
    raise SystemExit("FAIL doubleclick: additive resolution marker")
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
if armed_rows[0][1].group(1) != doubles[0].group(3):
    raise SystemExit("FAIL doubleclick positive: connect attempt not bound to accepted gesture")

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
if any("game rejected connection" in msg for msg in segment):
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

def normalized_message(value):
    return value[:-1] if value.endswith("\n") else value

def claimed_candidate_indices(values, prefixes=(), contains=(), label="semantic"):
    candidates=[]
    for index,value in enumerate(values):
        lines=re.split(r"[\r\n]",value)
        claimed=any(line.startswith(prefixes) for line in lines) \
            or any(token in line for line in lines for token in contains)
        if not claimed: continue
        if "\r" in value or "\n" in value:
            raise SystemExit(f"FAIL {label}: claimed marker embedded in multiline record")
        candidates.append(index)
    return candidates

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

def current_connect_abi(rows,label):
    normalized=[]
    for row in rows:
        value=row["msg"][:-1] if row["msg"].endswith("\n") else row["msg"]
        normalized.append(value)
    patterns=(
      ("armed",rf"Browser connect attempt armed target={e_connect} selection_generation=([1-9][0-9]*) credential_present=1","DEBUG","client"),
      ("resolve",rf"{e_connect} resolved to {e_connect}","INFO","client"),
      ("started",rf"WiredUI: started validated connect origin=browser address={e_connect} selection_generation=([1-9][0-9]*) credential_present=1","DEBUG","ui"),
    )
    found={}
    for name,pattern,sev,cat in patterns:
        matches=[(i,re.fullmatch(pattern,value)) for i,value in enumerate(normalized)]
        matches=[item for item in matches if item[1]]
        if len(matches)!=1 or rows[matches[0][0]]["sev"].upper()!=sev \
                or rows[matches[0][0]]["cat"].lower()!=cat:
            raise SystemExit(f"FAIL {label}: current connect {name} cardinality/metadata")
        found[name]=matches[0]
    if found["armed"][1].group(1)!=found["started"][1].group(1) \
            or not found["armed"][0] < found["resolve"][0] < found["started"][0]:
        raise SystemExit(f"FAIL {label}: current connect identity/order")
    if len(claimed_candidate_indices(normalized,
            prefixes=("Browser connect attempt armed ",),label=label))!=1 \
            or len(claimed_candidate_indices(normalized,
                prefixes=("WiredUI: started validated connect ",),label=label))!=1 \
            or len(claimed_candidate_indices(normalized,
                contains=(" resolved to ",),label=label))!=1:
        raise SystemExit(f"FAIL {label}: additive current connect family")
    return found["armed"][1].group(1)

wrong_connect_generation=current_connect_abi(wrong,"wrong-password")
product_connect_generation=current_connect_abi(product,"protected browser")
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
    ("wrong-password armed",
     rf"Browser connect attempt armed target={e_connect} selection_generation=[0-9]+ credential_present=1"),
    ("wrong-password resolution", rf"{e_connect} resolved to {e_connect}"),
    ("wrong-password started",
     rf"WiredUI: started validated connect origin=browser address={e_connect} selection_generation=[0-9]+ credential_present=1"),
    ("wrong-password CloseAll",
     r"WiredUI: close all postcondition depth=0 active=none catcher_ui=0 paused=0"),
    ("wrong-password submit Enter", r"wui_menu_nav: K_ENTER dispatched"),
    ("wrong-password credential disposal",
     rf"Browser connect credential disposed target={e_connect} stage=client-handoff"),
    ("wrong-password transport refusal",
     r"QUIC connect refused class=2 deferred_disconnect=1"),
    ("wrong-password deferred failure",
     r"Connect failed kind=2 browser_retry=1"),
    ("wrong-password retry popup", r"WiredUI: push menu 'password' \(depth 3\)"),
    ("wrong-password retry identity",
     rf"WiredUI: authentication retry opened address={e_connect} selection_generation=[0-9]+"),
])
wrong_normalized = [msg[:-1] if msg.endswith("\n") else msg for msg in wrong_messages]
def exact_wrong_family(prefix, pattern, severity, category):
    candidates = [(index, wrong_normalized[index]) for index in
                  claimed_candidate_indices(wrong_normalized,prefixes=(prefix,),
                                            label="wrong-password")]
    exact = [(index, re.fullmatch(pattern, value)) for index, value in candidates]
    exact = [item for item in exact if item[1]]
    if len(candidates) != 1 or len(exact) != 1:
        raise SystemExit(f"FAIL wrong-password: {prefix} family cardinality/format")
    index, match = exact[0]
    if wrong[index]["sev"].upper() != severity or wrong[index]["cat"].lower() != category:
        raise SystemExit(f"FAIL wrong-password: {prefix} metadata")
    return index, match

wrong_started = next(index for index, value in enumerate(wrong_normalized)
                     if re.fullmatch(
                         rf"WiredUI: started validated connect origin=browser address={e_connect} "
                         rf"selection_generation={wrong_connect_generation} credential_present=1",
                         value))
wrong_dispose, _ = exact_wrong_family(
    "Browser connect credential disposed ",
    rf"Browser connect credential disposed target={e_connect} stage=client-handoff",
    "DEBUG", "client")
wrong_refuse, _ = exact_wrong_family(
    "QUIC connect refused ", r"QUIC connect refused class=2 deferred_disconnect=1",
    "WARN", "network")
wrong_failure, _ = exact_wrong_family(
    "Connect failed kind=", r"Connect failed kind=2 browser_retry=1",
    "WARN", "client")
wrong_retry, wrong_retry_match = exact_wrong_family(
    "WiredUI: authentication retry opened ",
    rf"WiredUI: authentication retry opened address={e_connect} selection_generation=([1-9][0-9]*)",
    "DEBUG", "ui")
if wrong_retry_match.group(1) != wrong_connect_generation \
        or not wrong_started < wrong_dispose < wrong_refuse < wrong_failure < wrong_retry:
    raise SystemExit("FAIL wrong-password: refusal/retry identity or causal order")
if any("TLV ACCEPT" in msg or "FIRST GAMEPLAY FRAME" in msg for msg in wrong_messages):
    raise SystemExit("FAIL wrong-password: invalid credential was accepted")
def password_ingress_identity(rows, label, connect_generation):
    values = [normalized_message(row["msg"]) for row in rows]
    preflight_candidates=claimed_candidate_indices(
        values,prefixes=("WiredUI: password required origin=browser ",),label=label)
    preflight = [(index, match) for index, value in enumerate(values)
                 if (match := re.fullmatch(
                     rf"WiredUI: password required origin=browser address={e_connect} "
                     r"selection_generation=([1-9][0-9]*)", value))]
    if len(preflight_candidates) != 1 or len(preflight) != 1 \
            or preflight_candidates[0] != preflight[0][0]:
        raise SystemExit(f"FAIL {label}: password preflight cardinality")
    preflight_index, preflight_match = preflight[0]
    if rows[preflight_index]["sev"].upper() != "DEBUG" \
            or rows[preflight_index]["cat"].lower() != "ui":
        raise SystemExit(f"FAIL {label}: password preflight metadata")
    selections = [(index, match) for index, value in enumerate(values[:preflight_index])
                  if (match := re.fullmatch(
                      rf"WiredUI: server selection display_row=1 raw=[01] source=0 "
                      rf"list_generation=[1-9][0-9]* selection_generation=([1-9][0-9]*) "
                      rf"address={e_connect} name=Z0 WIRED Q0 TARGET map=arena7", value))]
    if not selections:
        raise SystemExit(f"FAIL {label}: selected target identity unavailable")
    selection_index, selection_match = selections[-1]
    if rows[selection_index]["sev"].upper() != "DEBUG" \
            or rows[selection_index]["cat"].lower() != "ui" \
            or selection_match.group(1) != preflight_match.group(1) \
            or preflight_match.group(1) != connect_generation:
        raise SystemExit(f"FAIL {label}: selection/preflight/connect generation drift")
    armed = [index for index, value in enumerate(values)
             if re.fullmatch(
                 rf"Browser connect attempt armed target={e_connect} "
                 rf"selection_generation={connect_generation} credential_present=1", value)]
    typed = [index for index, value in enumerate(values)
             if value == "wui_menu_nav: typed 7 printable character(s)"]
    masked = [index for index, value in enumerate(values)
              if value == "WiredUI: password render trace length=7 masked=1"]
    if len(armed) != 1 or len(typed) != 1 or len(masked) != 1:
        raise SystemExit(f"FAIL {label}: password input/armed cardinality")
    submit_focus = [index for index, value in enumerate(values[:armed[0]])
                    if value == "wui_menu_nav focus: focused item 'btn_connect' (top index -1)"]
    action_enter = [index for index, value in enumerate(values[armed[0] + 1:], armed[0] + 1)
                    if value == "wui_menu_nav: K_ENTER dispatched"]
    if not submit_focus or not action_enter \
            or not preflight_index < typed[0] < masked[0] < submit_focus[-1] \
                   < armed[0] < action_enter[0]:
        raise SystemExit(f"FAIL {label}: password input/focus/action causal order")
    semantic = (typed[0], masked[0], submit_focus[-1], action_enter[0])
    if any(rows[index]["sev"].upper() != "DEBUG"
           or rows[index]["cat"].lower() != "ui" for index in semantic):
        raise SystemExit(f"FAIL {label}: password input/focus/action metadata")
    return selection_index, preflight_index

wrong_selection, wrong_preflight = password_ingress_identity(
    wrong, "wrong-password", wrong_connect_generation)
if not wrong_selection < wrong_preflight < wrong_started:
    raise SystemExit("FAIL wrong-password: selection/preflight/connect order")
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
    ("browser attempt armed", rf"Browser connect attempt armed target={e_connect} selection_generation=[0-9]+ credential_present=1"),
    ("engine resolution", rf"{e_connect} resolved to {e_connect}"),
    ("validated browser start", rf"WiredUI: started validated connect origin=browser address={e_connect} selection_generation=[0-9]+ credential_present=1"),
    ("close all", r"WiredUI: close all postcondition depth=0 active=none catcher_ui=0 paused=0"),
    ("password submit Enter returns", r"wui_menu_nav: K_ENTER dispatched"),
    ("QUIC accept", r"QUIC client: TLV ACCEPT received"),
    ("first gameplay", r"FIRST GAMEPLAY FRAME mapname=maps/arena7\.bsp\b"),
    ("clean UI shutdown", r"WiredUI: shutdown"),
])

password_rows = [row for row in layout if row["menu"] == "password"]
if not password_rows:
    raise SystemExit("FAIL layout: password popup never rendered")
for region in ("password", "password_root", "row_password", "btn_connect"):
    matches = [row for row in password_rows if row["region"] == region]
    if not matches or any(row["w"] <= 0 or row["h"] <= 0 for row in matches):
        raise SystemExit(f"FAIL layout: missing/non-positive password region {region}")
subheaders = [row for row in password_rows if row["region"] == "subheader"]
if not subheaders or not any(row["w"] > 0 and row["h"] > 0 for row in subheaders):
    raise SystemExit("FAIL layout: password subheader never reached positive authored state")
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

if sum("password required origin=browser" in msg for msg in messages) != 1:
    raise SystemExit("FAIL: expected exactly one password preflight")
if sum("password submit refused reason=invalid-credential" in msg for msg in messages) != 1:
    raise SystemExit("FAIL: expected exactly one empty-password refusal")
if sum(msg.strip() == "WiredUI: password render trace length=7 masked=1"
       for msg in messages) != 1:
    raise SystemExit("FAIL: expected exactly one masked password render trace")
product_selection, product_preflight = password_ingress_identity(
    product, "protected browser", product_connect_generation)
product_started = next(index for index, value in enumerate(messages)
                       if re.fullmatch(
                           rf"WiredUI: started validated connect origin=browser address={e_connect} "
                           rf"selection_generation={product_connect_generation} credential_present=1",
                           value[:-1] if value.endswith("\n") else value))
if not product_selection < product_preflight < product_started:
    raise SystemExit("FAIL: protected selection/preflight/connect order")
for label, rows in (("protected browser", product), ("wrong-password", wrong)):
    logical_lines = [line for row in rows
                     for line in re.split(r"[\r\n]", row["msg"][:-1]
                                          if row["msg"].endswith("\n") else row["msg"])]
    if any(line.startswith("WiredUI: password submit accepted ") for line in logical_lines):
        raise SystemExit(f"FAIL {label}: obsolete password submit marker present")
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
server_values = [msg[:-1] if msg.endswith("\n") else msg for msg in server_messages]
server_severities = [row["sev"].upper() for row in server]
server_categories = [row["cat"].lower() for row in server]
wrong_reject_family = [index for index, value in enumerate(server_values[:boundary_index])
                       if any(line.startswith("QUIC: game rejected connection ")
                              for line in re.split(r"[\r\n]", value))]
wrong_reject = [index for index in wrong_reject_family
                if re.fullmatch(r"QUIC: game rejected connection from .* class=2",
                                server_values[index])]
if not wrong_conn or len(wrong_reject) != len(wrong_conn):
    raise SystemExit(
        "FAIL wrong-password server contract: every attempted connection must be "
        f"rejected by the game (conn={len(wrong_conn)} reject={len(wrong_reject)})")
if len(wrong_reject_family) != len(wrong_reject) \
        or any(server_severities[index] != "INFO" or server_categories[index] != "server"
               for index in wrong_reject):
    raise SystemExit("FAIL wrong-password server contract: refusal family format/metadata")
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

if any("game rejected connection" in msg
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

analyze_serverinfo_connect_contract() {
    python3 - "$1" "$2" "$3" "$4" "$5" "$6" "$7" <<'PYEOF'
import hashlib,json,math,re,sys
product_path,fixture_path,server_path,layout_path,sentinel_port,target_port,mode=sys.argv[1:8]
if mode not in {"proxy","direct"}: raise SystemExit("FAIL serverinfo-connect: invalid analyzer mode")
sentinel=f"127.0.0.1:{sentinel_port}"
target=f"127.0.0.1:{target_port}"

def read(path, fields):
    rows=[]
    with open(path,encoding="utf-8",errors="strict") as stream:
        for number,line in enumerate(stream,1):
            if not line.strip(): continue
            try: row=json.loads(line)
            except ValueError as exc: raise SystemExit(f"FAIL serverinfo-connect {path}:{number}: {exc}")
            if not isinstance(row,dict) or any(field not in row for field in fields):
                raise SystemExit(f"FAIL serverinfo-connect {path}:{number}: malformed row")
            rows.append(row)
    if not rows: raise SystemExit(f"FAIL serverinfo-connect: empty {path}")
    return rows

product=read(product_path,("sev","cat","msg"))
fixture=read(fixture_path,("event",)) if mode=="proxy" else []
server=read(server_path,("sev","cat","msg")) if mode=="direct" else []
layout=read(layout_path,("menu","region","frame","focused","x","y","w","h"))
if any(row["sev"].upper() in {"ERROR","FATAL"}
       or (row["sev"].upper()=="WARN" and row["cat"].lower()=="ui") for row in product):
    raise SystemExit("FAIL serverinfo-connect: bad product severity")
if any(row["sev"].upper() in {"ERROR","FATAL"} for row in server):
    raise SystemExit("FAIL serverinfo-connect: bad server severity")

def msg(row):
    value=str(row["msg"])
    if value.endswith("\n"): value=value[:-1]
    return value

def claimed_candidate_indices(values,prefixes=(),contains=(),label="semantic"):
    candidates=[]
    for index,value in enumerate(values):
        lines=re.split(r"[\r\n]",value)
        claimed=any(line.startswith(prefixes) for line in lines) \
            or any(token in line for line in lines for token in contains)
        if not claimed: continue
        if "\r" in value or "\n" in value:
            raise SystemExit(f"FAIL serverinfo-connect: {label} embedded in multiline record")
        candidates.append(index)
    return candidates

messages=[msg(row) for row in product]
cats=[str(row["cat"]).lower() for row in product]
sevs=[str(row["sev"]).upper() for row in product]
e_target=re.escape(target); e_sentinel=re.escape(sentinel)

families={
 "push_servers": (r"WiredUI: push menu 'servers' \(depth 1\)","ui"),
 "selection_sentinel": (rf"WiredUI: server selection display_row=0 raw=[01] source=0 list_generation=([1-9][0-9]*) selection_generation=([1-9][0-9]*) address={e_sentinel} name=A0 WIRED Q0 SENTINEL map=arena1","ui"),
 "selection": (rf"WiredUI: server selection display_row=1 raw=[01] source=0 list_generation=([1-9][0-9]*) selection_generation=([1-9][0-9]*) address={e_target} name=Z0 WIRED Q0 TARGET map=arena7","ui"),
 "serverlist_focus": (r"wui_menu_nav focus: focused item 'serverlist' \(top index [0-9-]+\)","ui"),
 "info_focus": (r"wui_menu_nav focus: focused item 'btn_info' \(top index -1\)","ui"),
 "request": (rf"WiredUI: server status request generation=([1-9][0-9]*) selection_generation=([1-9][0-9]*) address={e_target}","ui"),
 "pending": (rf"WiredUI: server status state=pending generation=([1-9][0-9]*) selection_generation=([1-9][0-9]*) address={e_target} rows=1","ui"),
 "push_info": (r"WiredUI: push menu 'serverinfo' \(depth 2\)","ui"),
 "stored": (rf"CL_ServerStatusResponse: stored response from {e_target} bytes=([1-9][0-9]*)","client"),
 "ready": (rf"WiredUI: server status loaded generation=([1-9][0-9]*) selection_generation=([1-9][0-9]*) address={e_target} rows=9","ui"),
 "loaded_0": (rf"WiredUI: server status row=0 key=Address value={e_target}","ui"),
 "loaded_1": (r"WiredUI: server status row=1 key=Server value=Z0 WIRED Q0 TARGET","ui"),
 "loaded_2": (r"WiredUI: server status row=2 key=Map value=arena7","ui"),
 "loaded_3": (r"WiredUI: server status row=3 key=Players value=1/8","ui"),
 "loaded_4": (r"WiredUI: server status row=4 key=Game type value=0","ui"),
 "loaded_5": (r"WiredUI: server status row=5 key=Game value=q3now","ui"),
 "loaded_6": (r"WiredUI: server status row=6 key=Protocol value=74","ui"),
 "loaded_7": (r"WiredUI: server status row=7 key=Version value=.+","ui"),
 "loaded_8": (r"WiredUI: server status row=8 key=Player 1 value=StatusBot . score 0, ping 0","ui"),
 "registry": (r"WiredUI: server status registry feeder=13 count=9","ui"),
 "address": (rf"WiredUI: server status registry row=0 key=Address value={e_target}","ui"),
 "registry_1": (r"WiredUI: server status registry row=1 key=Server value=Z0 WIRED Q0 TARGET","ui"),
 "registry_2": (r"WiredUI: server status registry row=2 key=Map value=arena7","ui"),
 "registry_3": (r"WiredUI: server status registry row=3 key=Players value=1/8","ui"),
 "registry_4": (r"WiredUI: server status registry row=4 key=Game type value=0","ui"),
 "registry_5": (r"WiredUI: server status registry row=5 key=Game value=q3now","ui"),
 "registry_6": (r"WiredUI: server status registry row=6 key=Protocol value=74","ui"),
 "registry_7": (r"WiredUI: server status registry row=7 key=Version value=.+","ui"),
 "registry_8": (r"WiredUI: server status registry row=8 key=Player 1 value=StatusBot . score 0, ping 0","ui"),
 "list_focus": (r"wui_menu_nav focus: focused item 'statuslist' \(top index [0-9-]+\)","ui"),
 "connect_focus": (r"wui_menu_nav focus: focused item 'btn_connect' \(top index -1\)","ui"),
 "armed": (rf"Browser connect attempt armed target={e_target} selection_generation=([1-9][0-9]*) credential_present=0","client"),
 "started": (rf"WiredUI: started validated connect origin=browser address={e_target} selection_generation=([1-9][0-9]*) credential_present=0","ui"),
 "dispose": (rf"WiredUI: server status dispose reason=close-all prior_state=ready generation=([1-9][0-9]*) selection_generation=([1-9][0-9]*) address={e_target}","ui"),
 "close": (r"WiredUI: close all postcondition depth=0 active=none catcher_ui=0 paused=0","ui"),
 "late": (rf"CL_ServerStatusResponse: ignored unrequested response from {e_target}","client"),
 "resolve": (rf"{e_target} resolved to {e_target}","client"),
 "accept": (r"QUIC client: TLV ACCEPT received","network"),
 "first": (r"cls\.state: -> CA_ACTIVE \(FIRST GAMEPLAY FRAME mapname=maps/arena7\.bsp serverTime=[0-9]+ numEntities=[0-9]+ framecount=[1-9][0-9]*\)","client"),
}
if mode=="proxy":
    for name in ("accept","first"): families.pop(name)
else:
    families.pop("late")
found={}
for name,(pattern,category) in families.items():
    matches=[]
    for index,value in enumerate(messages):
        match=re.fullmatch(pattern,value)
        if match:
            expected_severity="INFO" if name in {"resolve","first"} else "DEBUG"
            if cats[index] != category or sevs[index] != expected_severity:
                raise SystemExit(f"FAIL serverinfo-connect: {name} metadata")
            matches.append((index,match))
    if len(matches)!=1: raise SystemExit(f"FAIL serverinfo-connect: {name} cardinality {len(matches)}")
    found[name]=matches[0]
expected_cancel_generation=int(found["dispose"][1].group(1))+1
cancel_matches=[]
for index,value in enumerate(messages):
    match=re.fullmatch(rf"WiredUI: server status cancelled generation=({expected_cancel_generation}) rows=0",value)
    if match and cats[index]=="ui" and sevs[index]=="DEBUG": cancel_matches.append((index,match))
if len(cancel_matches)!=1: raise SystemExit(f"FAIL serverinfo-connect: owner cancel cardinality {len(cancel_matches)}")
found["cancel"]=cancel_matches[0]

order=["push_servers","selection_sentinel","serverlist_focus","selection","info_focus","request","pending",
       "push_info","stored","ready","loaded_0","loaded_1","loaded_2","loaded_3","loaded_4",
       "loaded_5","loaded_6","loaded_7","loaded_8","registry","address","registry_1","registry_2",
       "registry_3","registry_4","registry_5","registry_6","registry_7","registry_8","list_focus",
       "connect_focus","armed"]
order += ["resolve"]
order += ["started","dispose","cancel","close"]
if mode=="proxy":
    # The delayed proxy packet is deliberately delivered after CloseAll.
    order += ["late"]
else:
    order += ["accept","first"]
if [found[name][0] for name in order] != sorted(found[name][0] for name in order):
    raise SystemExit("FAIL serverinfo-connect: causal order")
nav=[(i,value) for i,value in enumerate(messages) if value.startswith("wui_menu_nav: K_")]
expected=["wui_menu_nav: K_DOWNARROW dispatched","wui_menu_nav: K_ENTER dispatched",
          "wui_menu_nav: K_ENTER dispatched"]
if [value for _,value in nav] != expected or any(cats[i]!="ui" or sevs[i]!="DEBUG" for i,_ in nav):
    raise SystemExit("FAIL serverinfo-connect: authored key inventory")
if not (found["selection_sentinel"][0] < found["serverlist_focus"][0] < found["selection"][0]
        < nav[0][0] < found["info_focus"][0]
        < found["request"][0] < found["push_info"][0] < nav[1][0]
        < found["ready"][0] < found["connect_focus"][0] < found["armed"][0]
        < found["close"][0] < nav[2][0]):
    raise SystemExit("FAIL serverinfo-connect: authored Enter action order")
selection_generation=found["selection"][1].group(2)
if found["selection_sentinel"][1].group(1) != found["selection"][1].group(1) \
        or int(found["selection_sentinel"][1].group(2))+1 != int(selection_generation):
    raise SystemExit("FAIL serverinfo-connect: selection epoch drift")
if any(found[name][1].group(group) != selection_generation
	   for name,group in (("request",2),("pending",2),("ready",2),("armed",1),("started",1),("dispose",2))):
    raise SystemExit("FAIL serverinfo-connect: selection generation drift")
if found["request"][1].group(1) != found["ready"][1].group(1) \
	   or found["request"][1].group(1) != found["pending"][1].group(1) \
	   or found["request"][1].group(1) != found["dispose"][1].group(1):
    raise SystemExit("FAIL serverinfo-connect: status generation drift")
if int(found["cancel"][1].group(1)) != int(found["request"][1].group(1))+1:
    raise SystemExit("FAIL serverinfo-connect: cancel generation drift")
closed_prefixes={
 "WiredUI: push menu ": {found["push_servers"][0],found["push_info"][0]},
 "WiredUI: server selection ": {found["selection_sentinel"][0],found["selection"][0]},
 "wui_menu_nav focus:": {found[name][0] for name in ("serverlist_focus","info_focus","list_focus","connect_focus")},
 "WiredUI: server status row=": {found[f"loaded_{i}"][0] for i in range(9)},
 "WiredUI: server status request ": {found["request"][0]},
 "WiredUI: server status state=": {found["pending"][0]},
 "WiredUI: server status loaded ": {found["ready"][0]},
 "WiredUI: server status registry": {found["registry"][0],found["address"][0],
     *(found[f"registry_{i}"][0] for i in range(1,9))},
 "Browser connect attempt armed ": {found["armed"][0]},
 "WiredUI: started validated connect ": {found["started"][0]},
 "WiredUI: server status dispose ": {found["dispose"][0]},
 "WiredUI: close all postcondition ": {found["close"][0]},
}
closed_prefixes["CL_ServerStatusResponse:"]={found["stored"][0]} \
    | ({found["late"][0]} if mode=="proxy" else set())
if mode=="direct":
    closed_prefixes.update({
      f"{target} resolved to ": {found["resolve"][0]},
      "QUIC client: TLV ACCEPT": {found["accept"][0]},
      "cls.state: -> CA_ACTIVE (FIRST GAMEPLAY FRAME": {found["first"][0]},
    })
if [i for i,value in enumerate(messages) if " resolved to " in value] != [found["resolve"][0]]:
    raise SystemExit("FAIL serverinfo-connect: additive resolve family")
if any(value.startswith("WiredUI: pop menu ") for value in messages):
    raise SystemExit("FAIL serverinfo-connect: popup escaped CloseAll ownership")
claimed_prefixes=tuple(closed_prefixes) + ("wui_menu_nav: K_",)
claimed_candidate_indices(messages,prefixes=claimed_prefixes,
                          contains=(" resolved to ",),label="claimed semantic marker")
for prefix,expected_indices in closed_prefixes.items():
    actual={index for index,value in enumerate(messages) if value.startswith(prefix)}
    if actual != expected_indices: raise SystemExit(f"FAIL serverinfo-connect: additive semantic family {prefix}")
if any(re.fullmatch(rf"CL_ServerStatusResponse: stored response from {e_target} bytes=[1-9][0-9]*",value)
       and index > found["cancel"][0] for index,value in enumerate(messages)):
    raise SystemExit("FAIL serverinfo-connect: late response stored after cancel")
if any("password required origin=browser" in value or "credential_present=1" in value
       or re.search(rf"{e_sentinel} resolved to {e_sentinel}",value) for value in messages):
    raise SystemExit("FAIL serverinfo-connect: credential/sentinel path contamination")

if mode=="proxy":
    events=[row["event"] for row in fixture]
    expected_events=["ready","request","challenge_seen",
                     "serverinfo_connect_upstream_request","serverinfo_connect_current_response",
                     "serverinfo_connect_late_response","stopped"]
    if events != expected_events:
        raise SystemExit(f"FAIL serverinfo-connect fixture: exact event inventory/order {events}")
    for event in ("ready","challenge_seen","serverinfo_connect_upstream_request",
                  "serverinfo_connect_current_response","serverinfo_connect_late_response","stopped"):
        if events.count(event)!=1: raise SystemExit(f"FAIL serverinfo-connect fixture: {event}")
    ready=next(row for row in fixture if row["event"]=="ready")
    generic=next(row for row in fixture if row["event"]=="request")
    challenge=next(row for row in fixture if row["event"]=="challenge_seen")
    request=next(row for row in fixture if row["event"]=="serverinfo_connect_upstream_request")
    current=next(row for row in fixture if row["event"]=="serverinfo_connect_current_response")
    late=next(row for row in fixture if row["event"]=="serverinfo_connect_late_response")
    stopped=next(row for row in fixture if row["event"]=="stopped")
    if stopped.get("reason")!="signal": raise SystemExit("FAIL serverinfo-connect fixture: uncontrolled stop")
    expected_ready={"client_port":current.get("peer_port"),"sentinel_port":int(sentinel_port),
                    "target_port":int(target_port),"protocol":74}
    if any(ready.get(key)!=value for key,value in expected_ready.items()) \
            or generic.get("role")!="target" or generic.get("command")!="getstatus" \
            or generic.get("peer_port")!=ready.get("client_port") \
            or challenge.get("ordinal")!=1 or not re.fullmatch(r"[0-9a-f]{16}",str(challenge.get("challenge",""))) \
            or request.get("request_number")!=1 or request.get("ordinal")!=1 \
            or request.get("target_port")!=int(target_port) or request.get("peer_port")!=ready.get("client_port") \
            or not isinstance(request.get("upstream_port"),int) or request["upstream_port"]<=0 \
            or any(not isinstance(row.get("elapsed_ms"),int) or row["elapsed_ms"]<0
                   for row in (request,current,late)) \
            or not request["elapsed_ms"] <= current["elapsed_ms"] < late["elapsed_ms"] \
            or late["elapsed_ms"]-current["elapsed_ms"] < 700:
        raise SystemExit("FAIL serverinfo-connect fixture: ready/request authority")
    fixture_order=[events.index(name) for name in ("ready","request","challenge_seen","serverinfo_connect_upstream_request",
                                                    "serverinfo_connect_current_response",
                                                    "serverinfo_connect_late_response","stopped")]
    if fixture_order != sorted(fixture_order): raise SystemExit("FAIL serverinfo-connect fixture: causal order")
    if not isinstance(current.get("length"),int) or current["length"]<=0 \
            or not re.fullmatch(r"[0-9a-f]{64}",str(current.get("sha256",""))) \
            or not re.fullmatch(r"[0-9a-f]+",str(current.get("packet_hex",""))) \
            or current.get("packet_hex") != late.get("packet_hex") \
            or (current["length"],current["sha256"],current.get("source_port"),current.get("peer_port")) != \
               (late.get("length"),late.get("sha256"),late.get("source_port"),late.get("peer_port")) \
            or len(bytes.fromhex(current["packet_hex"])) != current["length"] \
            or hashlib.sha256(bytes.fromhex(current["packet_hex"])).hexdigest() != current["sha256"] \
            or current.get("source_port")!=int(target_port):
        raise SystemExit("FAIL serverinfo-connect fixture: late packet not byte-identical/current-peer bound")
    packet=bytes.fromhex(current["packet_hex"])
    challenge_bytes=str(challenge["challenge"]).encode("ascii")
    if not packet.startswith(b"\xff\xff\xff\xffstatusResponse\n") \
            or b"\\challenge\\"+challenge_bytes not in packet \
            or b"\\sv_hostname\\Z0 WIRED Q0 TARGET" not in packet \
            or b"\\mapname\\arena7" not in packet or b"\\sv_maxclients\\8" not in packet \
            or b"\\sv_gamename\\q3now" not in packet or b"\\protocol\\74" not in packet:
        raise SystemExit("FAIL serverinfo-connect fixture: status packet semantic envelope")

info_rows=[row for row in layout if row["menu"]=="serverinfo"]
viewport_rows=[row for row in info_rows if row["region"]=="serverinfo"]
viewports={(row["frame"],float(row["w"]),float(row["h"])) for row in viewport_rows
           if row["x"]==0 and row["y"]==0 and row["w"]>0 and row["h"]>0}
viewport_by_frame={frame:(width,height) for frame,width,height in viewports}
if not viewport_by_frame or len(viewport_by_frame)!=len(viewport_rows):
    raise SystemExit("FAIL serverinfo-connect layout: viewport authority")
if any(not isinstance(row.get("frame"),int) or row["frame"]<0
       or any(not isinstance(row[field],(int,float)) or not math.isfinite(row[field])
              for field in ("x","y","w","h"))
       or row["x"]<0 or row["y"]<0 or row["w"]<=0 or row["h"]<=0
       or row["frame"] not in viewport_by_frame
       or row["x"]+row["w"]>viewport_by_frame[row["frame"]][0]+.01
       or row["y"]+row["h"]>viewport_by_frame[row["frame"]][1]+.01 for row in info_rows):
    raise SystemExit("FAIL serverinfo-connect layout: nonfinite/outside viewport")
for region in ("serverinfo","serverinfo_root","statuslist","btn_connect"):
    rows=[row for row in info_rows if row["region"]==region]
    if not rows or any(not isinstance(row[field],(int,float)) for row in rows for field in ("x","y","w","h")) \
            or any(row["w"]<=0 or row["h"]<=0 for row in rows):
        raise SystemExit(f"FAIL serverinfo-connect layout: {region}")
focus=[row for row in info_rows if row["region"]=="btn_connect" and row["focused"]==1]
if not focus or any(not all(any(candidate["frame"]==row["frame"] and candidate["region"]==region
                                for candidate in info_rows)
                            for region in ("serverinfo","serverinfo_root","statuslist"))
                    or sum(1 for candidate in info_rows
                        if candidate["frame"]==row["frame"] and candidate["focused"]==1)!=1 for row in focus):
    raise SystemExit("FAIL serverinfo-connect layout: unique btn_connect focus")

if mode=="proxy":
    if any("TLV ACCEPT" in value or "FIRST GAMEPLAY FRAME" in value for value in messages):
        raise SystemExit("FAIL serverinfo-connect proxy: gameplay contamination")
    print("  PASS serverinfo proxy: READY popup -> validated attempt -> CloseAll cancellation -> byte-identical late no-store")
else:
    server_messages=[msg(row) for row in server]
    server_cats=[str(row["cat"]).lower() for row in server]
    server_sevs=[str(row["sev"]).upper() for row in server]
    def server_family(pattern,severity,category):
        rows=[i for i,value in enumerate(server_messages) if re.fullmatch(pattern,value)]
        if len(rows)!=1 or server_sevs[rows[0]]!=severity or server_cats[rows[0]]!=category:
            raise SystemExit(f"FAIL serverinfo-connect server family: {pattern}")
        return rows
    listening=server_family(rf"WiredNet: listening on port {target_port} \(IPv4\), ALPN: [^,]+, max clients: [1-9][0-9]*, cert=.+ key=.+","INFO","network")
    arena=server_family(r"Server: arena7","INFO","server")
    needpass=server_family(r"g_needpass\s+0","INFO","system")
    conn=[(i,match.group(1)) for i,value in enumerate(server_messages)
          if (match:=re.fullmatch(r"SV_OnPlayerConnect: conn=([1-9][0-9]*)",value))]
    assigned=[(i,int(match.group(1)),match.group(2)) for i,value in enumerate(server_messages)
              if (match:=re.fullmatch(r"SV_OnPlayerConnect: slot ([1-9][0-9]*) assigned to conn=([1-9][0-9]*) \(127\.0\.0\.1\)",value))]
    if len(conn)!=1 or len(assigned)!=1: raise SystemExit("FAIL serverinfo-connect server: admission cardinality")
    assigned_i,slot,assigned_conn=assigned[0]
    all_connect=[(i,int(match.group(1))) for i,value in enumerate(server_messages)
                 if (match:=re.fullmatch(r"ClientConnect: ([0-9]+)",value))]
    all_begin=[(i,int(match.group(1))) for i,value in enumerate(server_messages)
               if (match:=re.fullmatch(r"ClientBegin: ([0-9]+)",value))]
    if sum(value.startswith("ClientConnect:") for value in server_messages)!=2 \
            or sum(value.startswith("ClientBegin:") for value in server_messages)!=2:
        raise SystemExit("FAIL serverinfo-connect server: additive ClientConnect/ClientBegin family")
    connect=[i for i,value in enumerate(server_messages) if value==f"ClientConnect: {slot}"]
    begin=[i for i,value in enumerate(server_messages) if value==f"ClientBegin: {slot}"]
    shutdown=[i for i,value in enumerate(server_messages) if value=="----- Server Shutdown (Server quit) -----"]
    transport=[i for i,value in enumerate(server_messages) if value=="QUIC transport shut down."]
    boundary=[i for i,value in enumerate(server_messages) if value=="Q0_SERVERINFO_DIRECT_PHASE_COMPLETE"]
    quit_request=[i for i,value in enumerate(server_messages) if value=="Q0_SERVERINFO_DIRECT_QUIT_REQUESTED"]
    if any(len(rows)!=1 for rows in (listening,arena,needpass,connect,begin,shutdown,transport,boundary,quit_request)) \
            or len(all_connect)!=2 or len(all_begin)!=2 \
            or all_connect[0][1] != all_begin[0][1] or all_connect[0][1] == slot \
            or all_connect[1][1] != slot or all_begin[1][1] != slot \
            or conn[0][1] != assigned_conn \
            or not listening[0] < arena[0] < all_connect[0][0] < all_begin[0][0] < needpass[0] \
            < conn[0][0] < connect[0] < assigned_i \
            < begin[0] < boundary[0] < quit_request[0] < shutdown[0] < transport[0]:
        raise SystemExit("FAIL serverinfo-connect server: VM admission/shutdown order")
    if any("game rejected connection" in value for value in server_messages):
        raise SystemExit("FAIL serverinfo-connect server: unexpected rejection")
    expected_metadata={
      conn[0][0]:("DEBUG","server"), assigned_i:("DEBUG","server"),
      all_connect[0][0]:("INFO","game"), all_begin[0][0]:("INFO","game"),
      connect[0]:("INFO","game"), begin[0]:("INFO","game"),
      boundary[0]:("INFO","system"), quit_request[0]:("INFO","system"),
      shutdown[0]:("INFO","server"), transport[0]:("INFO","network"),
    }
    if any((server_sevs[index],server_cats[index])!=metadata for index,metadata in expected_metadata.items()):
        raise SystemExit("FAIL serverinfo-connect server: semantic metadata")
    prefix_counts={"WiredNet: listening on port ":1,"SV_OnPlayerConnect: conn=":1,
                   "SV_OnPlayerConnect: slot ":1,"Q0_SERVERINFO_DIRECT_PHASE_COMPLETE":1,
                   "Q0_SERVERINFO_DIRECT_QUIT_REQUESTED":1,"----- Server Shutdown (":1,
                   "QUIC transport shut down.":1}
    for prefix,count in prefix_counts.items():
        if sum(value.startswith(prefix) for value in server_messages)!=count:
            raise SystemExit(f"FAIL serverinfo-connect server: additive family {prefix}")
    print("  PASS serverinfo direct: READY popup -> CloseAll cancellation -> same endpoint QUIC/VM/FIRST -> controlled shutdown")
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
 "Browser connect attempt armed target=127.0.0.1:28003 selection_generation=5 credential_present=1",
 "127.0.0.1:28003 resolved to 127.0.0.1:28003",
 "WiredUI: started validated connect origin=browser address=127.0.0.1:28003 selection_generation=5 credential_present=1",
 "WiredUI: close all postcondition depth=0 active=none catcher_ui=0 paused=0",
 "wui_menu_nav: K_ENTER dispatched",
 "QUIC client: TLV ACCEPT received",
 "cls.state: -> CA_ACTIVE (FIRST GAMEPLAY FRAME mapname=maps/arena7.bsp serverTime=100 numEntities=3 framecount=1)",
 "WiredUI: shutdown",
]
with open(product, "w", encoding="utf-8") as out:
    for msg in msgs:
        sev,cat="DEBUG","ui"
        if msg.startswith("Browser connect attempt armed "): cat="client"
        elif " resolved to " in msg: sev,cat="INFO","client"
        elif msg.startswith("QUIC client: TLV ACCEPT"): cat="network"
        elif "FIRST GAMEPLAY FRAME" in msg: sev,cat="INFO","client"
        out.write(json.dumps({"sev":sev,"cat":cat,"msg":msg})+"\n")
wrong_msgs = [
 "Unrelated startup banner line one\nline two",
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
 "Browser connect attempt armed target=127.0.0.1:28003 selection_generation=2 credential_present=1",
 "127.0.0.1:28003 resolved to 127.0.0.1:28003",
 "WiredUI: started validated connect origin=browser address=127.0.0.1:28003 selection_generation=2 credential_present=1",
 "WiredUI: close all postcondition depth=0 active=none catcher_ui=0 paused=0",
 "wui_menu_nav: K_ENTER dispatched",
 "Browser connect credential disposed target=127.0.0.1:28003 stage=client-handoff",
 "QUIC connect refused class=2 deferred_disconnect=1",
 "Connect failed kind=2 browser_retry=1",
 "WiredUI: push menu 'main' (depth 1)",
 "WiredUI: push menu 'servers' (depth 2)",
 "WiredUI: push menu 'password' (depth 3)",
 "WiredUI: authentication retry opened address=127.0.0.1:28003 selection_generation=2",
]
with open(wrong, "w", encoding="utf-8") as out:
    for msg in wrong_msgs:
        sev,cat="DEBUG","ui"
        if msg.startswith("Browser connect attempt armed "): cat="client"
        elif msg.startswith("Browser connect credential disposed "): cat="client"
        elif " resolved to " in msg: sev,cat="INFO","client"
        elif msg.startswith("QUIC connect refused "): sev,cat="WARN","network"
        elif msg.startswith("Connect failed kind="): sev,cat="WARN","client"
        out.write(json.dumps({"sev":sev,"cat":cat,"msg":msg})+"\n")
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
 "QUIC: game rejected connection from 127.0.0.1:28000 class=2",
 "Q0_WRONG_PASSWORD_PHASE_COMPLETE",
 "SV_OnPlayerConnect: conn=17",
 "  0:10 ClientConnect: 2",
 "SV_OnPlayerConnect: slot 2 assigned to conn=17 (127.0.0.1)",
 "  0:10 ClientBegin: 2",
 "----- Server Shutdown (Server quit) -----",
 "QUIC transport shut down.",
]
with open(server, "w", encoding="utf-8") as out:
    for msg in server_msgs:
        sev = "INFO" if msg.startswith("QUIC: game rejected connection ") else "DEBUG"
        out.write(json.dumps({"sev":sev,"cat":"server","msg":msg})+"\n")
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
    CLEAN_INFO_PRODUCT="$ROOT/serverinfo-product.jsonl"
    CLEAN_INFO_PROXY_PRODUCT="$ROOT/serverinfo-proxy-product.jsonl"
    CLEAN_INFO_DIRECT_PRODUCT="$ROOT/serverinfo-direct-product.jsonl"
    CLEAN_INFO_FIXTURE="$ROOT/serverinfo-fixture.jsonl"
    CLEAN_INFO_SERVER="$ROOT/serverinfo-server.jsonl"
    CLEAN_INFO_LAYOUT="$ROOT/serverinfo-layout.jsonl"
    python3 - "$CLEAN_INFO_PRODUCT" "$CLEAN_INFO_FIXTURE" "$CLEAN_INFO_SERVER" "$CLEAN_INFO_LAYOUT" <<'PYEOF'
import json,sys
product,fixture,server,layout=sys.argv[1:5]
s,t="127.0.0.1:28001","127.0.0.1:28005"
challenge_value="0123456789abcdef"
packet=(b"\xff\xff\xff\xffstatusResponse\n"
        + f"\\challenge\\{challenge_value}\\sv_hostname\\Z0 WIRED Q0 TARGET\\mapname\\arena7"
          "\\sv_maxclients\\8\\g_gametype\\0\\sv_gamename\\q3now\\protocol\\74"
          "\\version\\Q0 fixture 28005\n7 23 \"StatusBot\"\n".encode("ascii"))
packet_hex=packet.hex()
packet_hash=__import__("hashlib").sha256(packet).hexdigest()
packet_len=len(packet)
rows=[
 ("DEBUG","ui",f"WiredUI: server fixture installed sentinel={s} target={t} raw_order=target,sentinel"),
 ("DEBUG","ui","WiredUI: push menu 'servers' (depth 1)"),
 ("DEBUG","ui",f"WiredUI: server selection display_row=0 raw=1 source=0 list_generation=4 selection_generation=4 address={s} name=A0 WIRED Q0 SENTINEL map=arena1"),
 ("DEBUG","ui","wui_menu_nav focus: focused item 'serverlist' (top index 0)"),
 ("DEBUG","ui",f"WiredUI: server selection display_row=1 raw=0 source=0 list_generation=4 selection_generation=5 address={t} name=Z0 WIRED Q0 TARGET map=arena7"),
 ("DEBUG","ui","wui_menu_nav: K_DOWNARROW dispatched"),
 ("DEBUG","ui","wui_menu_nav focus: focused item 'btn_info' (top index -1)"),
 ("DEBUG","ui",f"WiredUI: server status request generation=7 selection_generation=5 address={t}"),
 ("DEBUG","ui",f"WiredUI: server status state=pending generation=7 selection_generation=5 address={t} rows=1"),
 ("DEBUG","ui","WiredUI: push menu 'serverinfo' (depth 2)"),
 ("DEBUG","ui","wui_menu_nav: K_ENTER dispatched"),
 ("DEBUG","client",f"CL_ServerStatusResponse: stored response from {t} bytes={packet_len}"),
 ("DEBUG","ui",f"WiredUI: server status loaded generation=7 selection_generation=5 address={t} rows=9"),
 ("DEBUG","ui",f"WiredUI: server status row=0 key=Address value={t}"),
 ("DEBUG","ui","WiredUI: server status row=1 key=Server value=Z0 WIRED Q0 TARGET"),
 ("DEBUG","ui","WiredUI: server status row=2 key=Map value=arena7"),
 ("DEBUG","ui","WiredUI: server status row=3 key=Players value=1/8"),
 ("DEBUG","ui","WiredUI: server status row=4 key=Game type value=0"),
 ("DEBUG","ui","WiredUI: server status row=5 key=Game value=q3now"),
 ("DEBUG","ui","WiredUI: server status row=6 key=Protocol value=74"),
 ("DEBUG","ui","WiredUI: server status row=7 key=Version value=Wired 0.80.77 macos-arm64 Aug 11 2026"),
 ("DEBUG","ui","WiredUI: server status row=8 key=Player 1 value=StatusBot — score 0, ping 0"),
 ("DEBUG","ui","WiredUI: server status registry feeder=13 count=9"),
 ("DEBUG","ui",f"WiredUI: server status registry row=0 key=Address value={t}"),
 ("DEBUG","ui","WiredUI: server status registry row=1 key=Server value=Z0 WIRED Q0 TARGET"),
 ("DEBUG","ui","WiredUI: server status registry row=2 key=Map value=arena7"),
 ("DEBUG","ui","WiredUI: server status registry row=3 key=Players value=1/8"),
 ("DEBUG","ui","WiredUI: server status registry row=4 key=Game type value=0"),
 ("DEBUG","ui","WiredUI: server status registry row=5 key=Game value=q3now"),
 ("DEBUG","ui","WiredUI: server status registry row=6 key=Protocol value=74"),
 ("DEBUG","ui","WiredUI: server status registry row=7 key=Version value=Wired 0.80.77 macos-arm64 Aug 11 2026"),
 ("DEBUG","ui","WiredUI: server status registry row=8 key=Player 1 value=StatusBot — score 0, ping 0"),
 ("DEBUG","ui","wui_menu_nav focus: focused item 'statuslist' (top index 1)"),
 ("DEBUG","ui","wui_menu_nav focus: focused item 'btn_connect' (top index -1)"),
 ("DEBUG","client",f"Browser connect attempt armed target={t} selection_generation=5 credential_present=0"),
 ("INFO","client",f"{t} resolved to {t}"),
 ("DEBUG","ui",f"WiredUI: started validated connect origin=browser address={t} selection_generation=5 credential_present=0"),
 ("DEBUG","ui",f"WiredUI: server status dispose reason=close-all prior_state=ready generation=7 selection_generation=5 address={t}"),
 ("DEBUG","ui","WiredUI: server status cancelled generation=8 rows=0"),
 ("DEBUG","ui","WiredUI: close all postcondition depth=0 active=none catcher_ui=0 paused=0"),
 ("DEBUG","ui","wui_menu_nav: K_ENTER dispatched"),
 ("DEBUG","client",f"CL_ServerStatusResponse: ignored unrequested response from {t}"),
 ("DEBUG","network","QUIC client: TLV ACCEPT received"),
 ("INFO","client","cls.state: -> CA_ACTIVE (FIRST GAMEPLAY FRAME mapname=maps/arena7.bsp serverTime=100 numEntities=3 framecount=1)"),
]
with open(product,"w",encoding="utf-8") as out:
    for sev,cat,msg in rows: out.write(json.dumps({"sev":sev,"cat":cat,"msg":msg})+"\n")
events=[
 {"event":"ready","client_port":28000,"sentinel_port":28001,"target_port":28005,"protocol":74},
 {"event":"request","role":"target","command":"getstatus","peer_port":28000},
 {"event":"challenge_seen","ordinal":1,"challenge":challenge_value},
 {"event":"serverinfo_connect_upstream_request","request_number":1,"ordinal":1,"target_port":28005,"peer_port":28000,"upstream_port":28006,"elapsed_ms":100},
 {"event":"serverinfo_connect_current_response","source_port":28005,"peer_port":28000,"elapsed_ms":110,"length":packet_len,"packet_hex":packet_hex,"sha256":packet_hash},
 {"event":"serverinfo_connect_late_response","source_port":28005,"peer_port":28000,"elapsed_ms":860,"length":packet_len,"packet_hex":packet_hex,"sha256":packet_hash},
 {"event":"stopped","reason":"signal"},
]
with open(fixture,"w",encoding="utf-8") as out:
    for row in events: out.write(json.dumps(row)+"\n")
server_rows=[
 ("INFO","network","WiredNet: listening on port 28005 (IPv4), ALPN: q3v69, max clients: 8, cert=test-cert key=test-key"),
 ("INFO","server","Server: arena7"),
 ("INFO","game","ClientConnect: 0"),
 ("INFO","game","ClientBegin: 0"),
 ("INFO","system","g_needpass           0"),
 ("DEBUG","server","SV_OnPlayerConnect: conn=7"),
 ("INFO","game","ClientConnect: 2"),
 ("DEBUG","server","SV_OnPlayerConnect: slot 2 assigned to conn=7 (127.0.0.1)"),
 ("INFO","game","ClientBegin: 2"),
 ("INFO","system","Q0_SERVERINFO_DIRECT_PHASE_COMPLETE"),
 ("INFO","system","Q0_SERVERINFO_DIRECT_QUIT_REQUESTED"),
 ("INFO","server","----- Server Shutdown (Server quit) -----"),
 ("INFO","network","QUIC transport shut down."),
]
with open(server,"w",encoding="utf-8") as out:
    for sev,cat,msg in server_rows: out.write(json.dumps({"sev":sev,"cat":cat,"msg":msg})+"\n")
layout_rows=[]
for region,rect in (("serverinfo",(0,0,1280,720)),("serverinfo_root",(256,108,768,504)),
                    ("statuslist",(280,180,720,320)),("btn_connect",(720,530,160,36))):
    x,y,w,h=rect
    layout_rows.append({"menu":"serverinfo","region":region,"frame":500,
                        "focused":int(region=="btn_connect"),"x":x,"y":y,"w":w,"h":h})
with open(layout,"w",encoding="utf-8") as out:
    for row in layout_rows: out.write(json.dumps(row)+"\n")
PYEOF
    python3 - "$CLEAN_INFO_PRODUCT" "$CLEAN_INFO_PROXY_PRODUCT" "$CLEAN_INFO_DIRECT_PRODUCT" <<'PYEOF'
import json,sys
source,proxy,direct=sys.argv[1:4]
rows=[json.loads(line) for line in open(source,encoding="utf-8")]
with open(proxy,"w",encoding="utf-8") as out:
    for row in rows:
        if not any(token in row["msg"] for token in ("TLV ACCEPT","FIRST GAMEPLAY FRAME")):
            out.write(json.dumps(row)+"\n")
with open(direct,"w",encoding="utf-8") as out:
    for row in rows:
        if "ignored unrequested response" not in row["msg"]:
            out.write(json.dumps(row)+"\n")
PYEOF
    analyze_serverinfo_connect_contract "$CLEAN_INFO_PROXY_PRODUCT" "$CLEAN_INFO_FIXTURE" \
        "$CLEAN_INFO_SERVER" "$CLEAN_INFO_LAYOUT" 28001 28005 proxy >/dev/null || {
        echo "FAIL self-test: clean serverinfo proxy fixture rejected"; exit 1; }
    analyze_serverinfo_connect_contract "$CLEAN_INFO_DIRECT_PRODUCT" "$CLEAN_INFO_FIXTURE" \
        "$CLEAN_INFO_SERVER" "$CLEAN_INFO_LAYOUT" 28001 28005 direct >/dev/null || {
        echo "FAIL self-test: clean serverinfo direct fixture rejected"; exit 1; }
    for defect in info_missing_dispose info_dispose_early info_wrong_state info_wrong_address \
            info_duplicate_cancel info_late_store info_missing_late info_hash_mismatch \
            info_missing_focus info_generation_drift info_pending_generation info_cancel_generation \
            info_additive_started info_missing_enter \
            info_additive_enter info_wrong_enter_category info_missing_status_row \
            info_additive_registry info_fixture_protocol info_fixture_peer info_fixture_stop_timeout \
            info_fixture_packet_semantic info_fixture_bad_challenge info_fixture_additive_event \
            info_additive_resolve info_multiline_claimed info_cr_smuggle_dispose \
            info_lf_smuggle_stored info_popup_escape info_layout_outside; do
        DIR="$ROOT/$defect"; mkdir -p "$DIR"
        python3 - "$CLEAN_INFO_PROXY_PRODUCT" "$CLEAN_INFO_FIXTURE" "$CLEAN_INFO_SERVER" \
            "$CLEAN_INFO_LAYOUT" "$DIR/product" "$DIR/fixture" "$DIR/server" "$DIR/layout" "$defect" <<'PYEOF'
import hashlib,json,sys
ps,fs,ss,ls,pd,fd,sd,ld,defect=sys.argv[1:10]
p=[json.loads(x) for x in open(ps,encoding="utf-8")]
f=[json.loads(x) for x in open(fs,encoding="utf-8")]
s=[json.loads(x) for x in open(ss,encoding="utf-8")]
l=[json.loads(x) for x in open(ls,encoding="utf-8")]
if defect=="info_missing_dispose": p=[r for r in p if "status dispose reason=close-all" not in r["msg"]]
elif defect=="info_dispose_early":
    i=next(i for i,r in enumerate(p) if "status dispose reason=close-all" in r["msg"]); row=p.pop(i)
    i=next(i for i,r in enumerate(p) if "started validated connect" in r["msg"]); p.insert(i,row)
elif defect=="info_wrong_state":
    next(r for r in p if "status dispose reason=close-all" in r["msg"])["msg"] = next(r["msg"] for r in p if "status dispose reason=close-all" in r["msg"]).replace("prior_state=ready","prior_state=pending")
elif defect=="info_wrong_address":
    r=next(r for r in p if "status dispose reason=close-all" in r["msg"]); r["msg"]=r["msg"].replace("127.0.0.1:28005","127.0.0.1:28001")
elif defect=="info_duplicate_cancel":
    i=next(i for i,r in enumerate(p) if "server status cancelled" in r["msg"]); p.insert(i,p[i].copy())
elif defect=="info_late_store":
    i=next(i for i,r in enumerate(p) if "ignored unrequested response" in r["msg"]); p[i]["msg"]="CL_ServerStatusResponse: stored response from 127.0.0.1:28005 bytes=180"
elif defect=="info_missing_late": f=[r for r in f if r["event"]!="serverinfo_connect_late_response"]
elif defect=="info_hash_mismatch": next(r for r in f if r["event"]=="serverinfo_connect_late_response")["sha256"]="2"*64
elif defect=="info_missing_focus": l=[r for r in l if r["region"]!="btn_connect"]
elif defect=="info_generation_drift":
    r=next(r for r in p if "started validated connect" in r["msg"]); r["msg"]=r["msg"].replace("selection_generation=5","selection_generation=4")
elif defect=="info_pending_generation":
    r=next(r for r in p if "server status state=pending" in r["msg"])
    r["msg"]=r["msg"].replace("selection_generation=5","selection_generation=999")
elif defect=="info_cancel_generation":
    r=next(r for r in p if "server status cancelled" in r["msg"]); r["msg"]="WiredUI: server status cancelled generation=999 rows=0"
elif defect=="info_additive_started":
    i=next(i for i,r in enumerate(p) if "started validated connect" in r["msg"]); p.insert(i,p[i].copy())
elif defect=="info_missing_enter":
    indices=[i for i,r in enumerate(p) if r["msg"]=="wui_menu_nav: K_ENTER dispatched"]; p.pop(indices[-1])
elif defect=="info_additive_enter":
    i=next(i for i,r in enumerate(p) if r["msg"]=="wui_menu_nav: K_ENTER dispatched"); p.insert(i,p[i].copy())
elif defect=="info_wrong_enter_category":
    next(r for r in p if r["msg"]=="wui_menu_nav: K_ENTER dispatched")["cat"]="client"
elif defect=="info_missing_status_row": p=[r for r in p if "server status row=4 " not in r["msg"]]
elif defect=="info_additive_registry":
    i=next(i for i,r in enumerate(p) if "server status registry row=8 " in r["msg"]); p.insert(i,p[i].copy())
elif defect=="info_fixture_protocol": next(r for r in f if r["event"]=="ready")["protocol"]=75
elif defect=="info_fixture_peer": next(r for r in f if r["event"]=="serverinfo_connect_upstream_request")["peer_port"]=1
elif defect=="info_fixture_stop_timeout": next(r for r in f if r["event"]=="stopped")["reason"]="timeout"
elif defect=="info_fixture_packet_semantic":
    current=next(r for r in f if r["event"]=="serverinfo_connect_current_response")
    packet=bytes.fromhex(current["packet_hex"]).replace(b"\\mapname\\arena7",b"\\mapname\\arena8")
    for row in f:
        if row["event"] in {"serverinfo_connect_current_response","serverinfo_connect_late_response"}:
            row["packet_hex"]=packet.hex(); row["length"]=len(packet); row["sha256"]=hashlib.sha256(packet).hexdigest()
elif defect=="info_fixture_bad_challenge": next(r for r in f if r["event"]=="challenge_seen")["challenge"]="0"*32
elif defect=="info_fixture_additive_event": f.insert(-1,{"event":"unexpected_upstream"})
elif defect=="info_additive_resolve":
    i=next(i for i,r in enumerate(p) if " resolved to " in r["msg"]); p.insert(i,p[i].copy())
elif defect=="info_multiline_claimed":
    r=next(r for r in p if "server status loaded" in r["msg"]); r["msg"] += "\nforged"
elif defect=="info_cr_smuggle_dispose":
    p.append({"sev":"DEBUG","cat":"ui",
              "msg":"benign\rWiredUI: server status dispose reason=close-all prior_state=ready generation=7 selection_generation=5 address=127.0.0.1:28005"})
elif defect=="info_lf_smuggle_stored":
    i=next(i for i,r in enumerate(p) if "server status cancelled" in r["msg"])
    p.insert(i+1,{"sev":"DEBUG","cat":"client",
                  "msg":"benign\nCL_ServerStatusResponse: stored response from 127.0.0.1:28005 bytes=180"})
elif defect=="info_popup_escape":
    i=next(i for i,r in enumerate(p) if "close all postcondition" in r["msg"]); p.insert(i,{"sev":"DEBUG","cat":"ui","msg":"WiredUI: pop menu (depth 1)"})
elif defect=="info_layout_outside": next(r for r in l if r["region"]=="btn_connect")["x"]=1270
for path,rows in ((pd,p),(fd,f),(sd,s),(ld,l)):
    with open(path,"w",encoding="utf-8") as out:
        for row in rows: out.write(json.dumps(row)+"\n")
PYEOF
        if analyze_serverinfo_connect_contract "$DIR/product" "$DIR/fixture" "$DIR/server" \
                "$DIR/layout" 28001 28005 proxy >/dev/null 2>&1; then
            echo "FAIL self-test: serverinfo Connect defect '$defect' accepted"; exit 1
        fi
        echo "  PASS self-test: rejects $defect"
    done
    for defect in info_direct_missing_accept info_direct_conn_mismatch info_direct_wrong_listen \
            info_direct_missing_shutdown info_direct_wrong_metadata info_direct_resolve_after_close \
            info_direct_missing_startup_bot info_direct_extra_clientconnect \
            info_direct_connect_suffix info_direct_begin_suffix \
            info_direct_wrong_accept_metadata info_direct_wrong_first_metadata \
            info_direct_late_response; do
        DIR="$ROOT/$defect"; mkdir -p "$DIR"
        python3 - "$CLEAN_INFO_DIRECT_PRODUCT" "$CLEAN_INFO_FIXTURE" "$CLEAN_INFO_SERVER" \
            "$CLEAN_INFO_LAYOUT" "$DIR/product" "$DIR/fixture" "$DIR/server" "$DIR/layout" "$defect" <<'PYEOF'
import json,sys
ps,fs,ss,ls,pd,fd,sd,ld,defect=sys.argv[1:10]
p=[json.loads(x) for x in open(ps,encoding="utf-8")]
f=[json.loads(x) for x in open(fs,encoding="utf-8")]
s=[json.loads(x) for x in open(ss,encoding="utf-8")]
l=[json.loads(x) for x in open(ls,encoding="utf-8")]
if defect=="info_direct_missing_accept": p=[r for r in p if "TLV ACCEPT" not in r["msg"]]
elif defect=="info_direct_conn_mismatch":
    r=next(r for r in s if "slot 2 assigned" in r["msg"]); r["msg"]=r["msg"].replace("conn=7","conn=8")
elif defect=="info_direct_wrong_listen":
    r=next(r for r in s if "WiredNet: listening" in r["msg"]); r["msg"]=r["msg"].replace("port 28005","port 28006")
elif defect=="info_direct_missing_shutdown": s=[r for r in s if "Server Shutdown" not in r["msg"]]
elif defect=="info_direct_wrong_metadata": next(r for r in s if "QUIC transport shut down" in r["msg"])["cat"]="server"
elif defect=="info_direct_resolve_after_close":
    i=next(i for i,r in enumerate(p) if " resolved to " in r["msg"]); row=p.pop(i)
    i=next(i for i,r in enumerate(p) if "close all postcondition" in r["msg"]); p.insert(i+1,row)
elif defect=="info_direct_missing_startup_bot":
    s=[r for r in s if r["msg"] not in {"ClientConnect: 0","ClientBegin: 0"}]
elif defect=="info_direct_extra_clientconnect":
    i=next(i for i,r in enumerate(s) if "ClientConnect:" in r["msg"]); row=s[i].copy(); row["msg"]="ClientConnect: 3"; s.insert(i+1,row)
elif defect=="info_direct_connect_suffix": next(r for r in s if r["msg"]=="ClientConnect: 2")["msg"] += " trailing"
elif defect=="info_direct_begin_suffix": next(r for r in s if r["msg"]=="ClientBegin: 2")["msg"] += " trailing"
elif defect=="info_direct_wrong_accept_metadata": next(r for r in p if "TLV ACCEPT" in r["msg"])["cat"]="client"
elif defect=="info_direct_wrong_first_metadata": next(r for r in p if "FIRST GAMEPLAY FRAME" in r["msg"])["sev"]="DEBUG"
elif defect=="info_direct_late_response":
    i=next(i for i,r in enumerate(p) if "TLV ACCEPT" in r["msg"])
    p.insert(i,{"sev":"DEBUG","cat":"client","msg":"CL_ServerStatusResponse: ignored unrequested response from 127.0.0.1:28005"})
for path,rows in ((pd,p),(fd,f),(sd,s),(ld,l)):
    with open(path,"w",encoding="utf-8") as out:
        for row in rows: out.write(json.dumps(row)+"\n")
PYEOF
        if analyze_serverinfo_connect_contract "$DIR/product" "$DIR/fixture" "$DIR/server" \
                "$DIR/layout" 28001 28005 direct >/dev/null 2>&1; then
            echo "FAIL self-test: serverinfo direct defect '$defect' accepted"; exit 1
        fi
        echo "  PASS self-test: rejects $defect"
    done
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
 f"Browser connect attempt armed target={t} selection_generation=14 credential_present=0",
 f"{t} resolved to {t}",
 f"WiredUI: started validated connect origin=browser address={t} selection_generation=14 credential_present=0",
 "WiredUI: pointer phase=release reason=close-all was_down=0 pointer_down=0",
 "WiredUI: close all postcondition depth=0 active=none catcher_ui=0 paused=0",
 "Q0_DBL_POSITIVE_END",
 "QUIC client: TLV ACCEPT received",
 "cls.state: -> CA_ACTIVE (FIRST GAMEPLAY FRAME mapname=maps/arena7.bsp serverTime=100 numEntities=3 framecount=1)",
 "WiredUI: shutdown",
]
with open(product, "w", encoding="utf-8") as out:
    for msg in msgs:
        sev,cat="DEBUG","ui"
        if msg.startswith("Browser connect attempt armed "): cat="client"
        elif " resolved to " in msg: sev,cat="INFO","client"
        elif msg.startswith("QUIC client: TLV ACCEPT"): cat="network"
        elif "FIRST GAMEPLAY FRAME" in msg: sev,cat="INFO","client"
        out.write(json.dumps({"sev":sev, "cat":cat, "msg":msg}) + "\n")
with open(layout, "w", encoding="utf-8") as out:
    out.write(json.dumps({"region":"serverlist", "menu":"servers", "frame":100,
                          "focused":1, "x":80, "y":220, "w":1120, "h":320}) + "\n")
PYEOF
    analyze_doubleclick_contract "$CLEAN_DOUBLE" "$CLEAN_DOUBLE_LAYOUT" "$CLEAN_DOUBLE_SERVER" 28001 28004 >/dev/null || { echo "FAIL self-test: clean doubleclick fixture rejected"; exit 1; }
    for defect in dbl_missing_enter dbl_enter_callback dbl_enter_arm dbl_missing_move dbl_fake_move dbl_missing_click dbl_fake_click dbl_missing_callback dbl_wrong_callback dbl_missing_arm dbl_missing_release dbl_missing_edge_down dbl_missing_release_reset dbl_missing_timeout_reset dbl_missing_row_reset dbl_missing_pop_reset dbl_missing_roster_reset dbl_reused_generation dbl_missing_double dbl_duplicate_double dbl_bad_elapsed dbl_early_connect dbl_duplicate_connect dbl_connect_generation dbl_missing_terminal_release dbl_bad_pointer_down dbl_no_close dbl_no_first dbl_wrong_first dbl_missing_server_conn dbl_missing_server_assign dbl_missing_server_begin dbl_missing_boundary dbl_missing_unprotected dbl_missing_quit_request dbl_missing_server_shutdown dbl_missing_transport_shutdown dbl_shutdown_before_boundary dbl_shared_server dbl_server_reject dbl_severity dbl_malformed dbl_missing_layout dbl_pointer_outside; do
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
elif defect == "dbl_early_connect":
    index = next(i for i,r in enumerate(p) if "Browser connect attempt armed" in r["msg"])
    row = p.pop(index)
    begin = next(i for i,r in enumerate(p) if r["msg"] == "Q0_DBL_SINGLE_BEGIN")
    p.insert(begin + 1, row)
elif defect == "dbl_duplicate_connect":
    index = next(i for i,r in enumerate(p) if "Browser connect attempt armed" in r["msg"])
    p.insert(index, p[index].copy())
elif defect == "dbl_connect_generation":
    row = next(r for r in p if "started validated connect" in r["msg"])
    row["msg"] = row["msg"].replace("selection_generation=14", "selection_generation=13")
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
    server.insert(boundary, {"sev":"INFO","cat":"server","msg":"QUIC: game rejected connection from 127.0.0.1 class=2"})
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
    for defect in dirty_roster wrong_order no_down wrong_target no_request no_status count_zero missing_row no_cancel no_return stale_store_accepted stale_response_accepted missing_lan_unsolicited missing_lan_unsolicited_event reused_lan_challenge missing_lan_a_missing missing_lan_wrong_packet missing_lan_wrong_reject missing_lan_malformed_packet missing_lan_malformed_reject false_lan_zero missing_lan_refresh_focus missing_lan_late_packet missing_lan_late_reject missing_lan_current_packet zero_lan_rtt duplicate_lan_discovery missing_lan_duplicate wrong_lan_row missing_lan_callback missing_lan_down extra_lan_scan missing_timeout_pending wrong_pending_row missing_timeout_terminal wrong_timeout_rows wrong_timeout_status missing_timeout_focus missing_timeout_retry timeout_single_drop reused_challenge missing_timeout_stale missing_malformed_pending missing_malformed_terminal malformed_loaded wrong_malformed_reason wrong_malformed_status missing_malformed_focus missing_malformed_retry missing_malformed_stale missing_valid_pending missing_recovery recovery_count_two missing_recovery_focus missing_retry_valid wrong_connect_target no_connect_up no_connect_down no_connect_focus missing_needpass missing_preflight duplicate_preflight direct_connect_before_prompt missing_password_popup missing_empty_refusal missing_password_focus missing_password_type missing_password_commit missing_password_mask bad_password_mask missing_password_submit_focus missing_password_action_enter obsolete_password_submit_marker reconnect_path sentinel_reconnect secret_leak missing_wrong_preflight wrong_cr_smuggle_preflight missing_wrong_type missing_wrong_mask bad_wrong_mask missing_wrong_action_enter wrong_obsolete_password_submit_marker missing_wrong_connect missing_wrong_started wrong_password_first wrong_secret_leak wrong_severity missing_wrong_dispose missing_wrong_transport_refusal bad_wrong_transport_refusal wrong_transport_refusal_metadata wrong_transport_refusal_suffix missing_wrong_failure bad_wrong_failure wrong_failure_metadata wrong_failure_suffix missing_wrong_retry bad_wrong_retry_target bad_wrong_retry_generation wrong_retry_metadata wrong_retry_suffix wrong_retry_order wrong_arm_cr_smuggle wrong_refusal_lf_smuggle missing_wrong_rejection bad_wrong_rejection wrong_server_refusal_metadata wrong_server_refusal_suffix wrong_assigned wrong_begin missing_attempt_boundary missing_password_arm bad_password_arm_order wrong_password_needpass password_attempt_before_arm missing_layout_popup wrong_layout_order multiple_layout_focus missing_client_connect missing_client_begin rejected_password missing_connect missing_started bad_connect_generation wrong_connect_metadata duplicate_connect bad_close no_accept no_first wrong_first missing_server_connect missing_oob_probe accepted_oob severity server_severity malformed_product malformed_fixture malformed_wrong malformed_layout; do
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
elif defect == "direct_connect_before_prompt":
    marker = next(i for i,r in enumerate(p) if "Browser connect attempt armed" in r["msg"])
    row = p.pop(marker)
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
    armed = next(i for i,r in enumerate(p) if "Browser connect attempt armed" in r["msg"])
    focus = max(i for i in range(armed) if "focused item 'btn_connect'" in p[i]["msg"])
    p.pop(focus)
elif defect == "missing_password_action_enter":
    armed = next(i for i,r in enumerate(p) if "Browser connect attempt armed" in r["msg"])
    enter = next(i for i in range(armed + 1, len(p)) if "K_ENTER dispatched" in p[i]["msg"])
    p.pop(enter)
elif defect == "obsolete_password_submit_marker":
    armed = next(i for i,r in enumerate(p) if "Browser connect attempt armed" in r["msg"])
    p.insert(armed, {"sev":"DEBUG", "cat":"ui",
                     "msg":"WiredUI: password submit accepted address=127.0.0.1:28003 selection_generation=5"})
elif defect == "reconnect_path":
    index = next(i for i,r in enumerate(p) if "Browser connect attempt armed" in r["msg"])
    p.insert(index, {"sev":"DEBUG", "cat":"ui", "msg":"password action queued reconnect"})
elif defect == "sentinel_reconnect":
    index = next(i for i,r in enumerate(p) if "127.0.0.1:28003 resolved" in r["msg"])
    p.insert(index, {"sev":"INFO", "cat":"client",
                     "msg":"127.0.0.1:28001 resolved to 127.0.0.1:28001"})
elif defect == "secret_leak":
    p.append({"sev":"DEBUG", "cat":"ui", "msg":"credential=q0Pass7"})
elif defect == "missing_wrong_preflight":
    w = [r for r in w if "password required origin=browser" not in r["msg"]]
elif defect == "wrong_cr_smuggle_preflight":
    w.append({"sev":"DEBUG","cat":"ui",
              "msg":"benign\rWiredUI: password required origin=browser address=127.0.0.1:28003 selection_generation=2"})
elif defect == "missing_wrong_type":
    w = [r for r in w if "typed 7 printable character(s)" not in r["msg"]]
elif defect == "missing_wrong_mask":
    w = [r for r in w if "password render trace" not in r["msg"]]
elif defect == "bad_wrong_mask":
    row = next(r for r in w if "password render trace" in r["msg"])
    row["msg"] = row["msg"].replace("masked=1", "masked=0")
elif defect == "missing_wrong_action_enter":
    armed = next(i for i,r in enumerate(w) if "Browser connect attempt armed" in r["msg"])
    enter = next(i for i in range(armed + 1, len(w)) if "K_ENTER dispatched" in w[i]["msg"])
    w.pop(enter)
elif defect == "wrong_obsolete_password_submit_marker":
    armed = next(i for i,r in enumerate(w) if "Browser connect attempt armed" in r["msg"])
    w.insert(armed, {"sev":"DEBUG", "cat":"ui",
                     "msg":"WiredUI: password submit accepted address=127.0.0.1:28003 selection_generation=2"})
elif defect == "missing_wrong_connect":
    w = [r for r in w if "Browser connect attempt armed" not in r["msg"]]
elif defect == "missing_wrong_started":
    w = [r for r in w if "started validated connect" not in r["msg"]]
elif defect == "wrong_password_first":
    w.append({"sev":"DEBUG", "cat":"client",
              "msg":"cls.state: -> CA_ACTIVE (FIRST GAMEPLAY FRAME mapname=maps/arena7.bsp)"})
elif defect == "wrong_secret_leak":
    w.append({"sev":"DEBUG", "cat":"ui", "msg":"credential=q0Wrong"})
elif defect == "wrong_severity":
    w.append({"sev":"WARN", "cat":"ui", "msg":"synthetic"})
elif defect == "missing_wrong_dispose":
    w = [r for r in w if not r["msg"].startswith("Browser connect credential disposed ")]
elif defect == "missing_wrong_transport_refusal":
    w = [r for r in w if not r["msg"].startswith("QUIC connect refused ")]
elif defect == "bad_wrong_transport_refusal":
    row = next(r for r in w if r["msg"].startswith("QUIC connect refused "))
    row["msg"] = row["msg"].replace("class=2", "class=1")
elif defect == "wrong_transport_refusal_metadata":
    row = next(r for r in w if r["msg"].startswith("QUIC connect refused ")); row["cat"]="client"
elif defect == "wrong_transport_refusal_suffix":
    row = next(r for r in w if r["msg"].startswith("QUIC connect refused ")); row["msg"] += " trailing"
elif defect == "missing_wrong_failure":
    w = [r for r in w if not r["msg"].startswith("Connect failed kind=")]
elif defect == "bad_wrong_failure":
    row = next(r for r in w if r["msg"].startswith("Connect failed kind="))
    row["msg"] = row["msg"].replace("browser_retry=1", "browser_retry=0")
elif defect == "wrong_failure_metadata":
    row = next(r for r in w if r["msg"].startswith("Connect failed kind=")); row["sev"]="INFO"
elif defect == "wrong_failure_suffix":
    row = next(r for r in w if r["msg"].startswith("Connect failed kind=")); row["msg"] += " trailing"
elif defect == "missing_wrong_retry":
    w = [r for r in w if not r["msg"].startswith("WiredUI: authentication retry opened ")]
elif defect == "bad_wrong_retry_target":
    row = next(r for r in w if r["msg"].startswith("WiredUI: authentication retry opened "))
    row["msg"] = row["msg"].replace("127.0.0.1:28003", "127.0.0.1:28001")
elif defect == "bad_wrong_retry_generation":
    row = next(r for r in w if r["msg"].startswith("WiredUI: authentication retry opened "))
    row["msg"] = row["msg"].replace("selection_generation=2", "selection_generation=99")
elif defect == "wrong_retry_metadata":
    row = next(r for r in w if r["msg"].startswith("WiredUI: authentication retry opened ")); row["cat"]="client"
elif defect == "wrong_retry_suffix":
    row = next(r for r in w if r["msg"].startswith("WiredUI: authentication retry opened ")); row["msg"] += " trailing"
elif defect == "wrong_retry_order":
    retry = next(i for i,r in enumerate(w) if r["msg"].startswith("WiredUI: authentication retry opened "))
    row = w.pop(retry)
    failure = next(i for i,r in enumerate(w) if r["msg"].startswith("Connect failed kind="))
    w.insert(failure, row)
elif defect == "wrong_arm_cr_smuggle":
    w.append({"sev":"DEBUG", "cat":"client",
              "msg":"benign\rBrowser connect attempt armed target=127.0.0.1:28003 selection_generation=2 credential_present=1"})
elif defect == "wrong_refusal_lf_smuggle":
    w.append({"sev":"WARN", "cat":"network",
              "msg":"benign\nQUIC connect refused class=2 deferred_disconnect=1"})
elif defect == "missing_wrong_rejection":
    sv = [r for r in sv if "QUIC: game rejected connection" not in r["msg"]]
elif defect == "bad_wrong_rejection":
    row = next(r for r in sv if "QUIC: game rejected connection" in r["msg"])
    row["msg"] = row["msg"].replace("class=2", "class=1")
elif defect == "wrong_server_refusal_metadata":
    row = next(r for r in sv if "QUIC: game rejected connection" in r["msg"]); row["sev"]="DEBUG"
elif defect == "wrong_server_refusal_suffix":
    row = next(r for r in sv if "QUIC: game rejected connection" in r["msg"]); row["msg"] += " trailing"
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
                   "msg":"QUIC: game rejected connection from 127.0.0.1 class=2"})
elif defect == "missing_connect": p = [r for r in p if "Browser connect attempt armed" not in r["msg"]]
elif defect == "missing_started": p = [r for r in p if "started validated connect" not in r["msg"]]
elif defect == "bad_connect_generation":
    row = next(r for r in p if "started validated connect" in r["msg"])
    row["msg"] = row["msg"].replace("selection_generation=5", "selection_generation=4")
elif defect == "wrong_connect_metadata":
    row = next(r for r in p if "Browser connect attempt armed" in r["msg"]); row["cat"]="ui"
elif defect == "duplicate_connect":
    row = next(r.copy() for r in p if "Browser connect attempt armed" in r["msg"])
    p.insert(next(i for i,r in enumerate(p) if "Browser connect attempt armed" in r["msg"]), row)
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
HEADLESS="${WIRED_BINARY_HEADLESS:-}"
if [ -z "$HEADLESS" ]; then
    gui_name="$(basename "$WIRED")"; suffix="${gui_name#wired}"
    for candidate in "$WIRED_DIR/wired-headless$suffix" "$WIRED_DIR/../../../wired-headless$suffix" \
        "$WIRED_DIR/../../../wired-headless.arm64" "$WIRED_DIR/wired-headless.arm64"; do
        if [ -x "$candidate" ]; then HEADLESS="$candidate"; break; fi
    done
fi
[ -n "$HEADLESS" ] && [ -x "$HEADLESS" ] || { echo "SKIP: sibling wired-headless missing"; exit 77; }
HEADLESS="$(cd "$(dirname "$HEADLESS")" && pwd)/$(basename "$HEADLESS")"
PACK_ROOT="$(wired_find_archive_root "$WIRED_DIR" "$WIRED_DIR/../Resources" "$WIRED_DIR/../../.." 2>/dev/null || true)"
[ -n "$PACK_ROOT" ] || { echo "SKIP: current VFS archives not found"; exit 77; }
CONTENT_ROOT="$(wired_find_archive_root "${WIRED_CONTENT_ROOT:-}" "$WIRED_HOME" "$PACK_ROOT" 2>/dev/null || true)"
[ -n "$CONTENT_ROOT" ] || { echo "SKIP: licensed base archives missing; set WIRED_CONTENT_ROOT"; exit 77; }

RUN_ROOT="$(mktemp -d -t wired-q0browser-XXXXXX 2>/dev/null || mktemp -d)"
HOME_ROOT="$RUN_ROOT/q3now-preview"
DOUBLE_HOME="$RUN_ROOT/double/q3now-preview"
DOUBLE_SERVER_HOME="$RUN_ROOT/double-server/q3now-preview"
INFO_HOME="$RUN_ROOT/serverinfo-connect/q3now-preview"
INFO_DIRECT_HOME="$RUN_ROOT/serverinfo-connect-direct/q3now-preview"
INFO_SERVER_HOME="$RUN_ROOT/serverinfo-connect-server/q3now-preview"
WRONG_HOME="$RUN_ROOT/wrong/q3now-preview"
SERVER_HOME="$RUN_ROOT/server/q3now-preview"
PRODUCT_JSON="$HOME_ROOT/qconsole.jsonl"
DOUBLE_JSON="$DOUBLE_HOME/qconsole.jsonl"
DOUBLE_SERVER_JSON="$DOUBLE_SERVER_HOME/qconsole.jsonl"
INFO_JSON="$INFO_HOME/qconsole.jsonl"
INFO_DIRECT_JSON="$INFO_DIRECT_HOME/qconsole.jsonl"
INFO_SERVER_JSON="$INFO_SERVER_HOME/qconsole.jsonl"
WRONG_JSON="$WRONG_HOME/qconsole.jsonl"
SERVER_JSON="$SERVER_HOME/qconsole.jsonl"
PRODUCT_STDOUT="$RUN_ROOT/product.stdout"
DOUBLE_STDOUT="$RUN_ROOT/double.stdout"
DOUBLE_SERVER_STDOUT="$RUN_ROOT/double-server.stdout"
INFO_STDOUT="$RUN_ROOT/serverinfo-connect.stdout"
INFO_DIRECT_STDOUT="$RUN_ROOT/serverinfo-connect-direct.stdout"
INFO_SERVER_STDOUT="$RUN_ROOT/serverinfo-connect-server.stdout"
WRONG_STDOUT="$RUN_ROOT/wrong.stdout"
LAYOUT_JSON="$RUN_ROOT/layoutdump.jsonl"
DOUBLE_LAYOUT_JSON="$RUN_ROOT/double-run/layoutdump.jsonl"
INFO_LAYOUT_JSON="$RUN_ROOT/serverinfo-connect-run/layoutdump.jsonl"
INFO_DIRECT_LAYOUT_JSON="$RUN_ROOT/serverinfo-connect-direct-run/layoutdump.jsonl"
INFO_FIXTURE_JSON="$RUN_ROOT/serverinfo-connect-fixture.jsonl"
FIXTURE_JSON="$RUN_ROOT/fixture.jsonl"
FIXTURE_PID=""
SERVER_PID=""
DOUBLE_SERVER_PID=""
INFO_FIXTURE_PID=""
INFO_SERVER_PID=""
SERVER_CONTROL="$RUN_ROOT/server.stdin"
DOUBLE_SERVER_CONTROL="$RUN_ROOT/double-server.stdin"
INFO_SERVER_CONTROL="$RUN_ROOT/serverinfo-connect-server.stdin"
SERVER_CONTROL_OPEN=0
DOUBLE_SERVER_CONTROL_OPEN=0
INFO_SERVER_CONTROL_OPEN=0
DOUBLE_SERVER_FORCED=0
cleanup() {
    if [ -n "$FIXTURE_PID" ] && kill -0 "$FIXTURE_PID" 2>/dev/null; then kill -TERM "$FIXTURE_PID" 2>/dev/null || true; wait "$FIXTURE_PID" 2>/dev/null || true; fi
    if [ -n "$INFO_FIXTURE_PID" ] && kill -0 "$INFO_FIXTURE_PID" 2>/dev/null; then kill -TERM "$INFO_FIXTURE_PID" 2>/dev/null || true; wait "$INFO_FIXTURE_PID" 2>/dev/null || true; fi
    if [ -n "$INFO_SERVER_PID" ] && kill -0 "$INFO_SERVER_PID" 2>/dev/null; then
        [ "$INFO_SERVER_CONTROL_OPEN" -eq 1 ] && printf '%s\n' quit >&7 2>/dev/null || true
        for _ in $(seq 1 150); do kill -0 "$INFO_SERVER_PID" 2>/dev/null || break; sleep 0.1; done
        kill -0 "$INFO_SERVER_PID" 2>/dev/null && kill -TERM "$INFO_SERVER_PID" 2>/dev/null || true
        wait "$INFO_SERVER_PID" 2>/dev/null || true
    fi
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
    [ "$INFO_SERVER_CONTROL_OPEN" -eq 1 ] && exec 7>&- || true
    if [ "${WIRED_KEEP_ARTIFACTS:-0}" = 1 ]; then echo "    kept artifacts: $RUN_ROOT"; else rm -rf "$RUN_ROOT"; fi
}
trap cleanup EXIT INT TERM
mkdir -p "$HOME_ROOT/base" "$DOUBLE_HOME/base" "$DOUBLE_SERVER_HOME/base" \
    "$INFO_HOME/base" "$INFO_DIRECT_HOME/base" "$INFO_SERVER_HOME/base" "$WRONG_HOME/base" "$SERVER_HOME/base" \
    "$RUN_ROOT/double-run" "$RUN_ROOT/serverinfo-connect-run" "$RUN_ROOT/serverinfo-connect-direct-run" "$RUN_ROOT/wrong-run"
for home in "$HOME_ROOT" "$DOUBLE_HOME" "$DOUBLE_SERVER_HOME" "$INFO_HOME" "$INFO_DIRECT_HOME" "$INFO_SERVER_HOME" "$WRONG_HOME" "$SERVER_HOME"; do
    wired_link_content_into_home "$home" "$CONTENT_ROOT/base" "$PACK_ROOT/base" || exit 1
done
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
waitms 45000
addbot visor 3 free 0 StatusBot
CFGEOF
cat >"$DOUBLE_SERVER_HOME/base/q0browser-double-server.cfg" <<'CFGEOF'
set g_password ""
map arena7
waitms 45000
addbot visor 3 free 0 StatusBot
CFGEOF
cat >"$INFO_SERVER_HOME/base/q0browser-serverinfo-server.cfg" <<'CFGEOF'
set g_password ""
map arena7
waitms 45000
addbot visor 3 free 0 StatusBot
CFGEOF

read -r DOUBLE_CLIENT_PORT DOUBLE_SERVER_PORT INFO_CLIENT_PORT INFO_PROXY_PORT INFO_SERVER_PORT WRONG_CLIENT_PORT CLIENT_PORT SENTINEL_PORT TARGET_PORT SERVER_PORT <<EOF
$(python3 - <<'PYEOF'
import socket
s=[]
for _ in range(10):
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
 MINGW*|MSYS*|CYGWIN*) INFO_SERVER_NATIVE="$(cygpath -w "$INFO_SERVER_HOME")" ;;
 *) INFO_SERVER_NATIVE="$INFO_SERVER_HOME" ;;
esac
mkfifo "$INFO_SERVER_CONTROL"; exec 7<>"$INFO_SERVER_CONTROL"; INFO_SERVER_CONTROL_OPEN=1
( cd "$(dirname "$HEADLESS")" && exec "$HEADLESS" +set fs_homepath "$INFO_SERVER_NATIVE" \
    +set com_automated 1 +set com_noHardReboot 1 +set log_severity DEBUG +set log_file_severity DEBUG \
    +set log_file_mode overwrite_synced +set net_ip 127.0.0.1 +set net_port "$INFO_SERVER_PORT" \
    +set sv_hostname "Z0 WIRED Q0 TARGET" +set sv_pure 0 +set g_autoBots 0 \
    +exec q0browser-serverinfo-server.cfg <&7 ) >"$INFO_SERVER_STDOUT" 2>&1 &
INFO_SERVER_PID=$!
if ! python3 - "$INFO_SERVER_JSON" "$INFO_SERVER_PID" <<'PYEOF'
import json,os,sys,time
path,pid=sys.argv[1],int(sys.argv[2]); deadline=time.monotonic()+60
while time.monotonic()<deadline:
    try: os.kill(pid,0)
    except ProcessLookupError: raise SystemExit("FAIL: serverinfo headless exited before readiness")
    try: rows=[json.loads(line) for line in open(path,encoding="utf-8") if line.strip()]
    except (OSError,ValueError): rows=[]
    messages=[str(row.get("msg","")) for row in rows]
    if any("InitGame:" in value and "\\mapname\\arena7" in value for value in messages) \
            and any("StatusBot has entered the game" in value for value in messages): break
    time.sleep(.1)
else: raise SystemExit("FAIL: serverinfo headless arena7/StatusBot readiness timeout")
PYEOF
then exit 1; fi
printf '%s\n' serverinfo >&7
if ! python3 - "$INFO_SERVER_JSON" <<'PYEOF'
import json,re,sys,time
path=sys.argv[1]; deadline=time.monotonic()+5
while time.monotonic()<deadline:
    try: messages=[str(json.loads(line).get("msg","")).strip() for line in open(path,encoding="utf-8") if line.strip()]
    except (OSError,ValueError): messages=[]
    if any(re.fullmatch(r"g_needpass\s+0",value) for value in messages): break
    time.sleep(.05)
else: raise SystemExit("FAIL: serverinfo headless did not expose g_needpass=0")
PYEOF
then exit 1; fi

python3 "$FIXTURE" --client-port "$INFO_CLIENT_PORT" --sentinel-port "$SENTINEL_PORT" \
    --target-port "$INFO_PROXY_PORT" --upstream-port "$INFO_SERVER_PORT" --serverinfo-connect \
    --protocol 75 --events "$INFO_FIXTURE_JSON" --timeout 30 &
INFO_FIXTURE_PID=$!
if ! python3 - "$INFO_FIXTURE_JSON" "$INFO_FIXTURE_PID" <<'PYEOF'
import json,os,sys,time
path,pid=sys.argv[1],int(sys.argv[2]); deadline=time.monotonic()+5
while time.monotonic()<deadline:
    try: os.kill(pid,0)
    except ProcessLookupError: raise SystemExit("FAIL: serverinfo proxy fixture exited before ready")
    try: rows=[json.loads(line) for line in open(path,encoding="utf-8") if line.strip()]
    except (OSError,ValueError): rows=[]
    if any(row.get("event")=="ready" for row in rows): break
    time.sleep(.05)
else: raise SystemExit("FAIL: serverinfo proxy fixture readiness timeout")
PYEOF
then exit 1; fi

cat >"$INFO_HOME/base/q0browser-serverinfo-proxy.cfg" <<CFGEOF
set com_maxfps 60
set password ""
wait 100
wui_server_fixture $SENTINEL_PORT $INFO_PROXY_PORT
wui_push servers
wait 30
wui_listbox_sort 0
wait 10
wui_menu_nav focus serverlist
wui_menu_nav down
wait 5
wui_menu_nav focus btn_info
wui_menu_nav enter
wait 18
wui_serverstatus_trace
set r_layoutDump 1
wait 3
wui_menu_nav focus statuslist
wui_menu_nav focus btn_connect
wait 2
wui_menu_nav enter
CFGEOF
case "$(uname -s)" in
 Darwin) INFO_NATIVE="$INFO_HOME"; INFO_PLATFORM_ARGS=( -ApplePersistenceIgnoreState YES ) ;;
 MINGW*|MSYS*|CYGWIN*) INFO_NATIVE="$(cygpath -w "$INFO_HOME")"; INFO_PLATFORM_ARGS=() ;;
 *) INFO_NATIVE="$INFO_HOME"; INFO_PLATFORM_ARGS=() ;;
esac
echo "==> WiredUI Server Info proxy lifecycle: endpoint=$INFO_PROXY_PORT late replay/no-store"
python3 "$TIMEOUT_RUNNER" --timeout 10 --kill-after 5 --cwd "$RUN_ROOT/serverinfo-connect-run" --stdout "$INFO_STDOUT" -- \
    "$WIRED" "${INFO_PLATFORM_ARGS[@]}" +set fs_homepath "$INFO_NATIVE" +set com_automated 1 \
    +set com_noHardReboot 1 +set net_enabled 1 +set net_ip 127.0.0.1 +set net_port "$INFO_CLIENT_PORT" \
    +set wn_cert_verify 0 +set s_initsound 0 +set r_fullscreen 0 +set r_mode -1 \
    +set r_customwidth 1280 +set r_customheight 720 +set log_severity DEBUG \
    +set log_file_severity DEBUG +set log_file_mode overwrite_synced +exec q0browser-serverinfo-proxy.cfg
info_proxy_rc=$?
if [ "$info_proxy_rc" -ne 124 ] || [ ! -s "$INFO_JSON" ] || [ ! -s "$INFO_LAYOUT_JSON" ]; then
    echo "FAIL: serverinfo proxy rc=$info_proxy_rc (expected bounded watchdog rc124) or missing evidence"
    [ -s "$INFO_STDOUT" ] && tail -40 "$INFO_STDOUT"
    exit 1
fi
kill -TERM "$INFO_FIXTURE_PID" 2>/dev/null || true
wait "$INFO_FIXTURE_PID" || { echo "FAIL: serverinfo proxy fixture exit was nonzero"; exit 1; }
INFO_FIXTURE_PID=""
if ! analyze_serverinfo_connect_contract "$INFO_JSON" "$INFO_FIXTURE_JSON" \
        "$INFO_SERVER_JSON" "$INFO_LAYOUT_JSON" "$SENTINEL_PORT" "$INFO_PROXY_PORT" proxy; then exit 1; fi

cat >"$INFO_DIRECT_HOME/base/q0browser-serverinfo-direct.cfg" <<CFGEOF
set activeAction "wait 60 ; quit"
set com_maxfps 60
set password ""
wait 100
wui_server_fixture $SENTINEL_PORT $INFO_SERVER_PORT
wui_push servers
wait 30
wui_listbox_sort 0
wait 10
wui_menu_nav focus serverlist
wui_menu_nav down
wait 5
wui_menu_nav focus btn_info
wui_menu_nav enter
wait 18
wui_serverstatus_trace
set r_layoutDump 1
wait 3
wui_menu_nav focus statuslist
wui_menu_nav focus btn_connect
wait 2
wui_menu_nav enter
CFGEOF
case "$(uname -s)" in
 Darwin) INFO_DIRECT_NATIVE="$INFO_DIRECT_HOME"; INFO_DIRECT_PLATFORM_ARGS=( -ApplePersistenceIgnoreState YES ) ;;
 MINGW*|MSYS*|CYGWIN*) INFO_DIRECT_NATIVE="$(cygpath -w "$INFO_DIRECT_HOME")"; INFO_DIRECT_PLATFORM_ARGS=() ;;
 *) INFO_DIRECT_NATIVE="$INFO_DIRECT_HOME"; INFO_DIRECT_PLATFORM_ARGS=() ;;
esac
echo "==> WiredUI Server Info direct Connect: endpoint=$INFO_SERVER_PORT"
python3 "$TIMEOUT_RUNNER" --timeout 60 --kill-after 10 --cwd "$RUN_ROOT/serverinfo-connect-direct-run" --stdout "$INFO_DIRECT_STDOUT" -- \
    "$WIRED" "${INFO_DIRECT_PLATFORM_ARGS[@]}" +set fs_homepath "$INFO_DIRECT_NATIVE" +set com_automated 1 \
    +set com_noHardReboot 1 +set net_enabled 1 +set net_ip 127.0.0.1 +set net_port "$INFO_CLIENT_PORT" \
    +set wn_cert_verify 0 +set s_initsound 0 +set r_fullscreen 0 +set r_mode -1 \
    +set r_customwidth 1280 +set r_customheight 720 +set log_severity DEBUG \
    +set log_file_severity DEBUG +set log_file_mode overwrite_synced +exec q0browser-serverinfo-direct.cfg
info_direct_rc=$?
if [ "$info_direct_rc" -ne 0 ] || [ ! -s "$INFO_DIRECT_JSON" ] || [ ! -s "$INFO_DIRECT_LAYOUT_JSON" ]; then
    echo "FAIL: serverinfo direct rc=$info_direct_rc or missing evidence"
    [ -s "$INFO_DIRECT_STDOUT" ] && tail -40 "$INFO_DIRECT_STDOUT"
    exit 1
fi
printf '%s\n' 'echo Q0_SERVERINFO_DIRECT_PHASE_COMPLETE' >&7
if ! python3 - "$INFO_SERVER_JSON" <<'PYEOF'
import json,sys,time
path=sys.argv[1]; deadline=time.monotonic()+5
while time.monotonic()<deadline:
    try: messages=[str(json.loads(line).get("msg","")).strip() for line in open(path,encoding="utf-8") if line.strip()]
    except (OSError,ValueError): messages=[]
    if "Q0_SERVERINFO_DIRECT_PHASE_COMPLETE" in messages: break
    time.sleep(.05)
else: raise SystemExit("FAIL: serverinfo headless did not record direct phase boundary")
PYEOF
then exit 1; fi
printf '%s\n' 'echo Q0_SERVERINFO_DIRECT_QUIT_REQUESTED' quit >&7
for _ in $(seq 1 150); do kill -0 "$INFO_SERVER_PID" 2>/dev/null || break; sleep 0.1; done
if kill -0 "$INFO_SERVER_PID" 2>/dev/null; then echo "FAIL: serverinfo headless did not exit after console quit"; exit 1; fi
if ! wait "$INFO_SERVER_PID"; then echo "FAIL: serverinfo headless exit was nonzero"; exit 1; fi
INFO_SERVER_PID=""; exec 7>&-; INFO_SERVER_CONTROL_OPEN=0
if ! analyze_serverinfo_connect_contract "$INFO_DIRECT_JSON" "$INFO_FIXTURE_JSON" \
        "$INFO_SERVER_JSON" "$INFO_DIRECT_LAYOUT_JSON" "$SENTINEL_PORT" "$INFO_SERVER_PORT" direct; then exit 1; fi

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
import json, re, sys
server_path, client_path = sys.argv[1:3]
def rows(path):
    with open(path, encoding="utf-8", errors="replace") as stream:
        return [json.loads(line) for line in stream if line.strip()]
server = rows(server_path)
client = rows(client_path)
def normalized(row):
    value = str(row.get("msg", ""))
    return value[:-1] if value.endswith("\n") else value
server_values = [normalized(row) for row in server]
client_values = [normalized(row) for row in client]
server_refusal = [index for index,value in enumerate(server_values)
                  if any(line.startswith("QUIC: game rejected connection ")
                         for line in re.split(r"[\r\n]", value))]
if len(server_refusal) != 1 \
        or not re.fullmatch(r"QUIC: game rejected connection from .* class=2",
                            server_values[server_refusal[0]]) \
        or server[server_refusal[0]].get("sev", "").upper() != "INFO" \
        or server[server_refusal[0]].get("cat", "").lower() != "server":
    raise SystemExit("FAIL: watchdog expired without authoritative fixed-class authentication refusal")
armed = [(index,match) for index,value in enumerate(client_values)
         if (match := re.fullmatch(
             r"Browser connect attempt armed target=([^ ]+) selection_generation=([1-9][0-9]*) credential_present=1",
             value))]
if len(armed) != 1:
    raise SystemExit("FAIL: wrong-password connect identity unavailable")
target, generation = armed[0][1].group(1), armed[0][1].group(2)
patterns = (
    (rf"Browser connect credential disposed target={re.escape(target)} stage=client-handoff", "DEBUG", "client"),
    (r"QUIC connect refused class=2 deferred_disconnect=1", "WARN", "network"),
    (r"Connect failed kind=2 browser_retry=1", "WARN", "client"),
    (rf"WiredUI: authentication retry opened address={re.escape(target)} selection_generation={generation}", "DEBUG", "ui"),
)
indices = []
for pattern,severity,category in patterns:
    matches = [index for index,value in enumerate(client_values) if re.fullmatch(pattern,value)]
    if len(matches) != 1 or client[matches[0]].get("sev", "").upper() != severity \
            or client[matches[0]].get("cat", "").lower() != category:
        raise SystemExit(f"FAIL: wrong-password current refusal ABI missing: {pattern}")
    indices.append(matches[0])
if not armed[0][0] < indices[0] < indices[1] < indices[2] < indices[3]:
    raise SystemExit("FAIL: wrong-password current refusal ABI out of order")
if any("TLV ACCEPT" in msg or "FIRST GAMEPLAY FRAME" in msg for msg in client_values):
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
    --protocol 75 --events "$FIXTURE_JSON" --timeout 180 &
FIXTURE_PID=$!
for _ in $(seq 1 50); do [ -s "$FIXTURE_JSON" ] && break; sleep 0.1; done
[ -s "$FIXTURE_JSON" ] || { echo "FAIL: fixture did not become ready"; exit 1; }

cat >"$HOME_ROOT/base/q0browser.cfg" <<CFGEOF
set activeAction "wait 60 ; quit"
set com_maxfps 60
set password ""
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
