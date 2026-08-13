#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Q0 WiredUI bot-action product gate. One isolated GUI listen-server proves
# that the real in-game Add Bot / Remove Bot controls create two independently
# packaged profiles (Visor and Grunt), render Grunt through its declared shared
# asset root, expose the bot-only feeder, reject a human slot, remove only the
# selected Grunt, and leave every human plus Visor alive.

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
TIMEOUT_RUNNER="$SCRIPT_DIR/run-with-timeout.py"

analyze_contract() {
    python3 - "$1" "$2" "$3" <<'PYEOF'
import json
import re
import sys

log_path, layout_path, game_path = sys.argv[1:4]

def json_objects(path):
    rows = []
    try:
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
                rows.append(row)
    except OSError as exc:
        raise SystemExit(f"FAIL: cannot read {path}: {exc}")
    if not rows:
        raise SystemExit(f"FAIL: empty evidence {path}")
    return rows

product = json_objects(log_path)
layout = json_objects(layout_path)
try:
    game = open(game_path, encoding="utf-8", errors="replace").read()
except OSError as exc:
    raise SystemExit(f"FAIL: cannot read {game_path}: {exc}")
if not game.strip():
    raise SystemExit(f"FAIL: empty evidence {game_path}")

bad = [row for row in product
       if str(row.get("sev", "")).upper() in {"ERROR", "FATAL"}
       or (str(row.get("sev", "")).upper() == "WARN"
           and str(row.get("cat", "")).lower() == "ui")]
if bad:
    raise SystemExit(f"FAIL severity: {len(bad)} ERROR/FATAL or cat=ui WARN record(s)")

messages = [str(row.get("msg", "")).rstrip("\r\n") for row in product]

def exact_family(prefix, patterns, severity, category):
    matches = [(index, row, messages[index]) for index, row in enumerate(product)
               if messages[index].startswith(prefix)]
    if len(matches) != len(patterns):
        raise SystemExit(f"FAIL exact family {prefix}: cardinality {len(matches)}")
    for (_, row, message), pattern in zip(matches, patterns):
        if str(row.get("sev", "")).upper() != severity \
                or str(row.get("cat", "")).lower() != category \
                or re.fullmatch(pattern, message) is None:
            raise SystemExit(f"FAIL exact family {prefix}: metadata/message {message}")
    return matches

def require(name, pattern):
    if not any(re.search(pattern, message) for message in messages):
        raise SystemExit(f"FAIL contract: missing {name}")

def ordered(steps):
    cursor = 0
    for name, pattern in steps:
        for index in range(cursor, len(messages)):
            if re.search(pattern, messages[index]):
                cursor = index + 1
                break
        else:
            raise SystemExit(f"FAIL contract: missing/out-of-order {name}")

require("arena7 first gameplay frame",
        r"FIRST GAMEPLAY FRAME mapname=(?:maps/)?arena7(?:\.bsp)?(?:\)|\s|$)")
require("arena7 navmesh readiness",
        r"\[NAV\] navmesh ready for 'arena7' \((?:built|from cache)\)")
require("Grunt eligibility and resolved primary asset",
        r"CL_Characters: bot profile 'grunt' eligible "
        r"primary=characters/visor/models/lower\.md3\b")

roster_events = exact_family("WiredUI: bot feeder roster generation=", [
    r"WiredUI: bot feeder roster generation=\d+ count=2",
    r"WiredUI: bot feeder roster generation=\d+ count=1",
    r"WiredUI: bot feeder roster generation=\d+ count=2",
    r"WiredUI: bot feeder roster generation=\d+ count=1",
    r"WiredUI: bot feeder roster generation=\d+ count=2",
    r"WiredUI: bot feeder roster generation=\d+ count=1",
], "DEBUG", "ui")
roster_generations = [int(re.fullmatch(
    r"WiredUI: bot feeder roster generation=(\d+) count=[12]", event[2]).group(1))
    for event in roster_events]
if roster_generations != list(range(roster_generations[0], roster_generations[0] + 6)):
    raise SystemExit(f"FAIL bot roster generations: {roster_generations}")
row_events = exact_family("WiredUI: bot feeder row=", [
    r"WiredUI: bot feeder row=0 client=2 allocation=1 name=(?:\^3)?Visor",
    r"WiredUI: bot feeder row=1 client=3 allocation=2 name=(?:\^1)?Grunt",
    r"WiredUI: bot feeder row=0 client=2 allocation=1 name=(?:\^3)?Visor",
    r"WiredUI: bot feeder row=0 client=2 allocation=1 name=(?:\^3)?Visor",
    r"WiredUI: bot feeder row=1 client=3 allocation=3 name=CustomGrunt",
    r"WiredUI: bot feeder row=0 client=2 allocation=1 name=(?:\^3)?Visor",
    r"WiredUI: bot feeder row=1 client=3 allocation=3 name=CustomGrunt",
    r"WiredUI: bot feeder row=0 client=2 allocation=1 name=(?:\^3)?Visor",
], "DEBUG", "ui")
selection_events = exact_family("WiredUI: bot feeder selection row=", [
    r"WiredUI: bot feeder selection row=0 client=2 allocation=1 generation=\d+ name=(?:\^3)?Visor",
    r"WiredUI: bot feeder selection row=1 client=3 allocation=2 generation=\d+ name=(?:\^1)?Grunt",
    r"WiredUI: bot feeder selection row=1 client=3 allocation=3 generation=\d+ name=CustomGrunt",
    r"WiredUI: bot feeder selection row=1 client=3 allocation=4 generation=\d+ name=CustomGrunt",
], "DEBUG", "ui")
selection_generations = [int(re.search(r" generation=(\d+) ", event[2]).group(1))
                         for event in selection_events]
if selection_generations != [roster_generations[0], roster_generations[0],
                             roster_generations[2], roster_generations[4]]:
    raise SystemExit(f"FAIL bot selection generations: {selection_generations}")
trace_events = exact_family("WiredUI: bot feeder trace generation=", [
    r"WiredUI: bot feeder trace generation=\d+ count=2 selected_client=-1 selected_allocation=0",
    r"WiredUI: bot feeder trace generation=\d+ count=1 selected_client=-1 selected_allocation=0",
    r"WiredUI: bot feeder trace generation=\d+ count=2 selected_client=-1 selected_allocation=0",
    r"WiredUI: bot feeder trace generation=\d+ count=2 selected_client=-1 selected_allocation=0",
    r"WiredUI: bot feeder trace generation=\d+ count=1 selected_client=-1 selected_allocation=0",
], "DEBUG", "ui")
trace_generations = [int(re.search(r"generation=(\d+)", event[2]).group(1))
                     for event in trace_events]
if trace_generations != [roster_generations[0], roster_generations[1],
                         roster_generations[2], roster_generations[2],
                         roster_generations[5]]:
    raise SystemExit(f"FAIL bot trace generations: {trace_generations}")
queue_events = exact_family("WiredUI: queued verified bot kick", [
    r"WiredUI: queued verified bot kick client=3 allocation=2",
    r"WiredUI: queued verified bot kick client=3 allocation=4",
], "DEBUG", "ui")
ui_kick_refusals = exact_family("WiredUI: bot kick refused", [
    r"WiredUI: bot kick refused without a current bot selection",
], "DEBUG", "ui")
botkick_events = exact_family("botkick:", [
    r"botkick: refused non-bot client=1 name=.*",
    r"botkick: removed bot client=3 allocation=2 name=(?:\^1)?Grunt",
    r"botkick: removed bot client=3 allocation=3 name=CustomGrunt",
    r"botkick: refused stale bot identity client=3 expected=3 actual=4",
    r"botkick: removed bot client=3 allocation=4 name=CustomGrunt",
], "INFO", "server")
custom_add_queue_index = next(i for i, message in enumerate(messages)
                              if message == "WiredUI: queued validated custom bot add "
                              "profile=grunt name=CustomGrunt skill=4 team=free")
custom_server_enters = [(i, row, messages[i]) for i, row in enumerate(product)
                        if str(row.get("cat", "")).lower() == "server"
                        and "CustomGrunt" in messages[i]
                        and "has entered the game" in messages[i]]
custom_cgame_enters = [(i, row, messages[i]) for i, row in enumerate(product)
                       if str(row.get("cat", "")).lower() == "cgame"
                       and "CustomGrunt" in messages[i]
                       and "has entered the game" in messages[i]]
if len(custom_server_enters) != 2 or len(custom_cgame_enters) != 2:
    raise SystemExit(f"FAIL CustomGrunt enter cardinality: "
                     f"server={custom_server_enters} cgame={custom_cgame_enters}")
for _, row, message in custom_server_enters:
    if str(row.get("sev", "")).upper() != "INFO" or re.fullmatch(
            r'broadcast: print "(?:\^2)?CustomGrunt(?:\^7)? has entered the game\\n"',
            message) is None:
        raise SystemExit(f"FAIL CustomGrunt server-enter metadata/message: {message}")
for _, row, message in custom_cgame_enters:
    if str(row.get("sev", "")).upper() != "INFO" or re.fullmatch(
            r"(?:\^2)?CustomGrunt(?:\^7)? has entered the game", message) is None:
        raise SystemExit(f"FAIL CustomGrunt cgame-enter metadata/message: {message}")
stale_button_focus = next(i for i in range(roster_events[4][0] + 1, len(messages))
                          if "focused item 'btn_kick_bot'" in messages[i])
stale_enter = next(i for i in range(stale_button_focus + 1, len(messages))
                   if messages[i] == "wui_menu_nav: K_ENTER dispatched")
fresh_botlist_focus = next(i for i in range(botkick_events[3][0] + 1, len(messages))
                           if "focused item 'botlist'" in messages[i])
fresh_down = next(i for i in range(fresh_botlist_focus + 1, len(messages))
                  if messages[i] == "wui_menu_nav: K_DOWNARROW dispatched")
fresh_button_focus = next(i for i in range(fresh_down + 1, len(messages))
                          if "focused item 'btn_kick_bot'" in messages[i])
fresh_enter = next(i for i in range(queue_events[1][0] + 1, len(messages))
                   if messages[i] == "wui_menu_nav: K_ENTER dispatched")
aba_botlist_focus = next(i for i in range(roster_events[2][0] + 1, len(messages))
                         if "focused item 'botlist'" in messages[i])
aba_down = next(i for i in range(selection_events[2][0] + 1, len(messages))
                if messages[i] == "wui_menu_nav: K_DOWNARROW dispatched")
if not (queue_events[0][0] < botkick_events[1][0]
        < custom_add_queue_index < custom_server_enters[0][0]
        < custom_cgame_enters[0][0] < roster_events[2][0]
        < selection_events[2][0] < aba_botlist_focus < aba_down
        < botkick_events[2][0]
        < roster_events[3][0] < custom_server_enters[1][0]
        < custom_cgame_enters[1][0] < roster_events[4][0]
        < stale_button_focus < ui_kick_refusals[0][0] < stale_enter
        < botkick_events[3][0] < selection_events[3][0]
        < fresh_botlist_focus < fresh_down < fresh_button_focus
        < queue_events[1][0] < fresh_enter < botkick_events[4][0]
        < roster_events[5][0]):
    raise SystemExit("FAIL bot ABA authority cursor")

grunt_loads = [message for message in messages
               if "CG_LoadCharacter: loaded profile=grunt" in message]
if not grunt_loads:
    raise SystemExit("FAIL render: missing Grunt character-load success")
grunt_load_re = re.compile(
    r"^CG_LoadCharacter: loaded profile=grunt parts=(\d+) legs=(\d+) "
    r"torso=(\d+) head=(\d+) icon=(\d+) skin=(\d+)$")
for message in grunt_loads:
    match = grunt_load_re.fullmatch(message)
    if not match:
        raise SystemExit(f"FAIL render: malformed Grunt success marker: {message}")
    parts, legs, torso, head, icon, skin = map(int, match.groups())
    if parts != 3 or min(legs, torso, head, icon, skin) <= 0:
        raise SystemExit(f"FAIL render: non-renderable Grunt handles: {message}")

grunt_failure = re.compile(
    r"(?:CG_LoadCharacter: 'grunt' (?:part .* not found|no usable body model found)|"
    r"Failed to load model file .*grunt|CG_RegisterClientModelname\([^\n]*grunt|"
    r"CL_Characters: bot profile 'grunt' (?:ineligible|rejected)|"
    r"(?:grunt.*fallback|fallback.*grunt))",
    re.IGNORECASE)
if any(grunt_failure.search(message) for message in messages):
    raise SystemExit("FAIL render: Grunt missing-part, fallback, or eligibility failure observed")

ordered([
    ("navmesh ready before bot actions",
     r"\[NAV\] navmesh ready for 'arena7' \((?:built|from cache)\)"),
    ("headless human slot",
     r"spawn_headless_client: client slot 1 connecting\b"),
    ("human botkick refusal",
     r"botkick: refused non-bot client=1\b"),
    ("in-game menu", r"WiredUI: push menu 'ingame' \(depth 1\)"),
    ("add-bot button", r"focused item 'btn_addbot'"),
    ("first add dialog", r"WiredUI: push menu 'addbots' \(depth 2\)"),
    ("Visor quick action focus", r"focused item 'btn_quick_visor'"),
    ("validated Visor add",
     r"WiredUI: queued validated quick bot add profile=visor skill=3 team=free"),
    ("first add dialog close", r"WiredUI: pop menu \(depth 1\)"),
    ("Visor Enter return", r"wui_menu_nav: K_ENTER dispatched"),
    ("Visor entered", r"(?:\^3)?Visor(?:\^7)? has entered the game"),
    ("second add-bot button", r"focused item 'btn_addbot'"),
    ("second add dialog", r"WiredUI: push menu 'addbots' \(depth 2\)"),
    ("Grunt quick action focus", r"focused item 'btn_quick_grunt'"),
    ("validated Grunt add",
     r"WiredUI: queued validated quick bot add profile=grunt skill=3 team=free"),
    ("second add dialog close", r"WiredUI: pop menu \(depth 1\)"),
    ("Grunt Enter return", r"wui_menu_nav: K_ENTER dispatched"),
    ("Grunt entered", r"(?:\^1)?Grunt(?:\^7)? has entered the game"),
    ("two-bot trace count", r"WiredUI: bot feeder trace .*count=2\b"),
    ("Visor trace row", r"WiredUI: bot feeder row=0 client=2 allocation=1 name=(?:\^3)?Visor\b"),
    ("Grunt trace row", r"WiredUI: bot feeder row=1 client=3 allocation=2 name=(?:\^1)?Grunt\b"),
    ("remove-bot button", r"focused item 'btn_removebot'"),
    ("remove dialog", r"WiredUI: push menu 'removebots' \(depth 2\)"),
    ("row-zero callback",
     r"WiredUI: bot feeder selection row=0 client=2 allocation=1 .*name=(?:\^3)?Visor\b"),
    ("bot list focus", r"focused item 'botlist'"),
    ("row-one callback",
     r"WiredUI: bot feeder selection row=1 client=3 allocation=2 .*name=(?:\^1)?Grunt\b"),
    ("real Down return", r"wui_menu_nav: K_DOWNARROW dispatched"),
    ("kick button", r"focused item 'btn_kick_bot'"),
    ("verified kick queued", r"WiredUI: queued verified bot kick client=3 allocation=2\b"),
    ("kick Enter", r"wui_menu_nav: K_ENTER dispatched"),
    ("server removed selected Grunt", r"botkick: removed bot client=3 allocation=2 name=(?:\^1)?Grunt\b"),
    ("selected Grunt kicked broadcast", r"(?:\^1)?Grunt(?:\^7)? was kicked"),
    ("post-kick bot count", r"WiredUI: bot feeder trace .*count=1\b"),
    ("post-kick surviving Visor", r"WiredUI: bot feeder row=0 client=2 allocation=1 name=(?:\^3)?Visor\b"),
    ("remove dialog Back", r"WiredUI: pop menu \(depth 1\)"),
    ("remove dialog Back dispatch", r"wui_menu_nav: K_ESCAPE dispatched"),
    ("custom add button", r"focused item 'btn_addbot'"),
    ("custom add dialog", r"WiredUI: push menu 'addbots' \(depth 2\)"),
    ("custom initial Visor selection",
     r"WiredUI: custom bot profile selected profile=visor generation=\d+"),
    ("custom add open Enter", r"wui_menu_nav: K_ENTER dispatched"),
    ("profile field", r"focused item 'row_profile'"),
    ("profile dropdown open", r"wui_menu_nav: K_ENTER dispatched"),
    ("profile dropdown real Down", r"wui_menu_nav: K_DOWNARROW dispatched"),
    ("custom Grunt selection",
     r"WiredUI: custom bot profile selected profile=grunt generation=\d+"),
    ("profile dropdown select", r"wui_menu_nav: K_ENTER dispatched"),
    ("name field", r"focused item 'row_name'"),
    ("name edit begin", r"wui_menu_nav: K_ENTER dispatched"),
    ("custom name typed", r"wui_menu_nav: typed 11 printable character\(s\)"),
    ("name edit commit", r"wui_menu_nav: K_ENTER dispatched"),
    ("skill field", r"focused item 'row_skill'"),
    ("skill dropdown open", r"wui_menu_nav: K_ENTER dispatched"),
    ("skill 3 to 4", r"wui_menu_nav: K_DOWNARROW dispatched"),
    ("skill dropdown select", r"wui_menu_nav: K_ENTER dispatched"),
    ("team field", r"focused item 'row_team'"),
    ("team dropdown open", r"wui_menu_nav: K_ENTER dispatched"),
    ("team free to red", r"wui_menu_nav: K_DOWNARROW dispatched"),
    ("team red to free", r"wui_menu_nav: K_UPARROW dispatched"),
    ("team dropdown select", r"wui_menu_nav: K_ENTER dispatched"),
    ("custom submit", r"focused item 'btn_add_custom'"),
    ("validated custom add",
     r"WiredUI: queued validated custom bot add profile=grunt name=CustomGrunt skill=4 team=free"),
    ("custom success pop", r"WiredUI: pop menu \(depth 1\)"),
    ("custom submit Enter", r"wui_menu_nav: K_ENTER dispatched"),
    ("CustomGrunt entered", r"CustomGrunt(?:\^7)? has entered the game"),
    ("custom final bot count", r"WiredUI: bot feeder trace .*count=2\b"),
    ("custom final Visor row", r"WiredUI: bot feeder row=0 client=2 allocation=1 name=(?:\^3)?Visor\b"),
    ("custom final row", r"WiredUI: bot feeder row=1 client=3 allocation=3 name=CustomGrunt\b"),
    ("negative add button", r"focused item 'btn_addbot'"),
    ("negative add dialog", r"WiredUI: push menu 'addbots' \(depth 2\)"),
    ("negative initial Visor selection",
     r"WiredUI: custom bot profile selected profile=visor generation=\d+"),
    ("negative profile field", r"focused item 'row_profile'"),
    ("negative Grunt selection",
     r"WiredUI: custom bot profile selected profile=grunt generation=\d+"),
    ("negative name field", r"focused item 'row_name'"),
    ("injection suffix typed", r"wui_menu_nav: typed 5 printable character\(s\)"),
    ("negative submit", r"focused item 'btn_add_custom'"),
    ("invalid display name rejected",
     r"WiredUI: custom bot add rejected invalid display name"),
    ("invalid-name menu retained", r"focused item 'btn_add_custom'"),
    ("name restore field", r"focused item 'row_name'"),
    ("name restore edit", r"wui_menu_nav: K_ENTER dispatched"),
    ("name restore backspace", r"wui_menu_nav: K_BACKSPACE dispatched"),
    ("name restore commit", r"wui_menu_nav: K_ENTER dispatched"),
    ("stale submit focus", r"focused item 'btn_add_custom'"),
    ("character registry reloaded", r"CL_Characters: reloaded \d+ character\(s\)"),
    ("stale profile rejected",
     r"WiredUI: custom bot add rejected stale profile selection"),
    ("stale-selection menu retained", r"focused item 'btn_add_custom'"),
    ("ABA remove dialog", r"WiredUI: push menu 'removebots' \(depth 2\)"),
    ("ABA selected original allocation",
     r"WiredUI: bot feeder selection row=1 client=3 allocation=3 .*name=CustomGrunt"),
    ("ABA original removed",
     r"botkick: removed bot client=3 allocation=3 name=CustomGrunt"),
    ("ABA replacement entered", r"CustomGrunt(?:\^7)? has entered the game"),
    ("stale UI selection refused",
     r"WiredUI: bot kick refused without a current bot selection"),
    ("stale recycled-slot kick rejected",
     r"botkick: refused stale bot identity client=3 expected=3 actual=4"),
    ("ABA selected replacement allocation",
     r"WiredUI: bot feeder selection row=1 client=3 allocation=4 .*name=CustomGrunt"),
    ("ABA fresh kick queued",
     r"WiredUI: queued verified bot kick client=3 allocation=4"),
    ("ABA replacement removed",
     r"botkick: removed bot client=3 allocation=4 name=CustomGrunt"),
    ("ABA final survivor count", r"WiredUI: bot feeder trace .*count=1\b"),
])

if any(re.search(r"\bclientkick\b", message, re.IGNORECASE) for message in messages):
    raise SystemExit("FAIL boundary: legacy clientkick appeared in product evidence")

post_kick_count = next(i for i, message in enumerate(messages)
                       if re.search(r"WiredUI: bot feeder trace .*count=1\b", message))
valid_custom_queue = next(i for i, message in enumerate(messages[post_kick_count + 1:],
                                                        post_kick_count + 1)
                          if "queued validated custom bot add" in message)
selection_re = re.compile(
    r"WiredUI: custom bot profile selected profile=(\w+) generation=(\d+)$")
custom_selections = []
for index, message in enumerate(messages[post_kick_count + 1:valid_custom_queue],
                                post_kick_count + 1):
    match = selection_re.fullmatch(message)
    if match:
        custom_selections.append((index, match.group(1), int(match.group(2))))
if len(custom_selections) != 2:
    raise SystemExit(f"FAIL custom selection: expected two authoritative selections, got {custom_selections}")
if [row[1] for row in custom_selections] != ["visor", "grunt"]:
    raise SystemExit(f"FAIL custom selection: wrong profile order {custom_selections}")
if custom_selections[0][2] != custom_selections[1][2]:
    raise SystemExit(f"FAIL custom selection: generation changed {custom_selections}")

validated_custom_queues = [message for message in messages
                           if "queued validated custom bot add" in message]
if validated_custom_queues != [
        "WiredUI: queued validated custom bot add profile=grunt "
        "name=CustomGrunt skill=4 team=free"]:
    raise SystemExit(f"FAIL custom validation: unexpected accepted queue(s) "
                     f"{validated_custom_queues}")

valid_result_count = next(i for i, message in enumerate(messages[valid_custom_queue + 1:],
                                                         valid_custom_queue + 1)
                          if re.search(r"WiredUI: bot feeder trace .*count=2\b", message))
custom_final_count = max(i for i, message in enumerate(messages)
                         if re.search(r"WiredUI: bot feeder trace .*count=2\b", message))
aba_remove_dialog = next(i for i, message in enumerate(messages[custom_final_count + 1:],
                                                         custom_final_count + 1)
                         if message == "WiredUI: push menu 'removebots' (depth 2)")
invalid_reject = next(i for i, message in enumerate(messages)
                      if message == "WiredUI: custom bot add rejected invalid display name")
reload_index = next(i for i, message in enumerate(messages)
                    if re.fullmatch(r"CL_Characters: reloaded \d+ character\(s\)", message))
stale_reject = next(i for i, message in enumerate(messages)
                    if message == "WiredUI: custom bot add rejected stale profile selection")
negative_selections = []
for index, message in enumerate(messages[valid_result_count + 1:invalid_reject],
                                valid_result_count + 1):
    match = selection_re.fullmatch(message)
    if match:
        negative_selections.append((index, match.group(1), int(match.group(2))))
if [row[1] for row in negative_selections] != ["visor", "grunt"]:
    raise SystemExit(f"FAIL negative selection: wrong profile order {negative_selections}")
if negative_selections[0][2] != negative_selections[1][2]:
    raise SystemExit(f"FAIL negative selection: generation changed before reload "
                     f"{negative_selections}")
if not (invalid_reject < reload_index < stale_reject):
    raise SystemExit("FAIL negative selection: invalid/reload/stale order is not causal")
backspaces = [i for i, message in enumerate(messages[invalid_reject + 1:reload_index],
                                            invalid_reject + 1)
              if message == "wui_menu_nav: K_BACKSPACE dispatched"]
if len(backspaces) != 5:
    raise SystemExit(f"FAIL negative selection: expected five real backspaces, got {backspaces}")

final_rows = []
for message in messages[custom_final_count + 1:aba_remove_dialog]:
    match = re.fullmatch(r"WiredUI: bot feeder row=(\d+) client=(\d+) allocation=(\d+) name=(.*)", message)
    if match:
        final_rows.append((int(match.group(1)), int(match.group(2)),
                           int(match.group(3)), match.group(4)))
if len(final_rows) != 2:
    raise SystemExit(f"FAIL result: custom final trace has unexpected rows {final_rows}")
if not re.fullmatch(r"(?:\^3)?Visor", final_rows[0][3]) or final_rows[0][:3] != (0, 2, 1):
    raise SystemExit(f"FAIL result: custom final Visor row mismatch {final_rows[0]}")
if final_rows[1] != (1, 3, 3, "CustomGrunt"):
    raise SystemExit(f"FAIL result: custom final row mismatch {final_rows[1]}")
if not any(re.fullmatch(r"WiredUI: bot feeder row=0 client=2 allocation=1 name=(?:\^3)?Visor", message)
           for message in messages[aba_remove_dialog:]):
    raise SystemExit("FAIL ABA result: final Visor survivor row missing")
if any(re.search(r"botkick: removed bot client=(?:0|1|2)\b", message) for message in messages):
    raise SystemExit("FAIL safety: host, headless human, or Visor was removed")

game_steps = [
    ("headless human connect", r"ClientConnect: 1\b"),
    ("headless human begin", r"ClientBegin: 1\b"),
    ("Visor connect", r"ClientConnect: 2\b"),
    ("Visor profile", r"ClientUserinfoChanged: 2 .*\\char\\visor\\skin\\default.*\\skill\\3\.00\b"),
    ("Visor begin", r"ClientBegin: 2\b"),
    ("Grunt connect", r"ClientConnect: 3\b"),
    ("Grunt profile", r"ClientUserinfoChanged: 3 .*\\char\\grunt\\skin\\default.*\\skill\\3\.00\b"),
    ("Grunt begin", r"ClientBegin: 3\b"),
    ("selected bot disconnect", r"ClientDisconnect: 3\b"),
    ("custom reconnect", r"ClientConnect: 3\b"),
    ("custom profile",
     r"ClientUserinfoChanged: 3 .*n\\CustomGrunt\\t\\0\\char\\grunt\\skin\\default.*\\skill\\4\.00\b"),
    ("custom begin", r"ClientBegin: 3\b"),
    ("ABA original disconnect", r"ClientDisconnect: 3\b"),
    ("ABA replacement connect", r"ClientConnect: 3\b"),
    ("ABA replacement profile",
     r"ClientUserinfoChanged: 3 .*n\\CustomGrunt\\t\\0\\char\\grunt\\skin\\default.*\\skill\\4\.00\b"),
    ("ABA replacement begin", r"ClientBegin: 3\b"),
    ("ABA replacement disconnect", r"ClientDisconnect: 3\b"),
]
cursor = 0
for name, pattern in game_steps:
    match = re.search(pattern, game[cursor:])
    if not match:
        raise SystemExit(f"FAIL game log: missing/out-of-order {name}")
    cursor += match.end()
if re.search(r"ClientDisconnect: (?:0|1|2)\b", game):
    raise SystemExit("FAIL game log: a protected/surviving client disconnected")
if len(re.findall(r"ClientConnect: 3\b", game)) != 3 \
        or len(re.findall(r"ClientBegin: 3\b", game)) != 3 \
        or len(re.findall(r"ClientDisconnect: 3\b", game)) != 3:
    raise SystemExit("FAIL game log: rejected custom attempts changed the client lifecycle")

focused = sorted(
    (int(row.get("frame", 0)), str(row.get("menu", "")), str(row.get("region", "")))
    for row in layout
    if row.get("kind") == "item" and int(row.get("focused", 0)) == 1
)
layout_steps = [
    ("addbots", "btn_quick_visor"),
    ("addbots", "btn_quick_grunt"),
    ("removebots", "botlist"),
    ("removebots", "btn_kick_bot"),
    ("addbots", "row_profile"),
    ("addbots", "row_name"),
    ("addbots", "row_skill"),
    ("addbots", "row_team"),
    ("addbots", "btn_add_custom"),
]
cursor = 0
last_frame = -1
for wanted in layout_steps:
    for index in range(cursor, len(focused)):
        frame, menu, region = focused[index]
        if frame > last_frame and (menu, region) == wanted:
            cursor = index + 1
            last_frame = frame
            break
    else:
        raise SystemExit(f"FAIL layout: missing ordered focus {wanted[0]}/{wanted[1]}")

print("  PASS add: named quick actions created Visor client2 and Grunt client3")
print("  PASS render: Grunt eligibility resolved Visor assets and all handles are nonzero")
print("  PASS remove: real bot-feeder row0 -> Down -> row1 removed only Grunt client3")
print("  PASS custom: Visor -> Grunt selection created CustomGrunt at skill4/free")
print("  PASS ABA: stale Grunt allocation could not remove same-slot CustomGrunt")
print("  PASS negative: invalid name and stale registry generation retained the form")
print("  PASS safety: host/client1/client2 survived; no legacy clientkick")
PYEOF
}

write_fixture() {
    python3 - "$1" "$2" "$3" "$4" <<'PYEOF'
import json
import sys

mode, log_path, layout_path, game_path = sys.argv[1:5]
messages = [
    "cls.state: -> CA_ACTIVE (FIRST GAMEPLAY FRAME mapname=maps/arena7.bsp)",
    "[NAV] navmesh ready for 'arena7' (built)",
    "CL_Characters: bot profile 'grunt' eligible primary=characters/visor/models/lower.md3",
    "spawn_headless_client: client slot 1 connecting (handle 8)",
    "botkick: refused non-bot client=1 name=HeadlessClient",
    "WiredUI: push menu 'ingame' (depth 1)",
    "wui_menu_nav focus: focused item 'btn_addbot' (top index -1)",
    "WiredUI: push menu 'addbots' (depth 2)",
    "wui_menu_nav focus: focused item 'btn_quick_visor' (top index -1)",
    "WiredUI: queued validated quick bot add profile=visor skill=3 team=free",
    "WiredUI: pop menu (depth 1)",
    "wui_menu_nav: K_ENTER dispatched",
    "broadcast: print \"^2^3Visor^7 has entered the game\\n\"",
    "wui_menu_nav focus: focused item 'btn_addbot' (top index -1)",
    "WiredUI: push menu 'addbots' (depth 2)",
    "wui_menu_nav focus: focused item 'btn_quick_grunt' (top index -1)",
    "WiredUI: queued validated quick bot add profile=grunt skill=3 team=free",
    "WiredUI: pop menu (depth 1)",
    "wui_menu_nav: K_ENTER dispatched",
    "broadcast: print \"^2^1Grunt^7 has entered the game\\n\"",
    "CG_LoadCharacter: loaded profile=grunt parts=3 legs=31 torso=32 head=33 icon=34 skin=35",
    "WiredUI: bot feeder roster generation=3 count=2",
    "WiredUI: bot feeder trace generation=3 count=2 selected_client=-1 selected_allocation=0",
    "WiredUI: bot feeder row=0 client=2 allocation=1 name=^3Visor",
    "WiredUI: bot feeder row=1 client=3 allocation=2 name=^1Grunt",
    "wui_menu_nav focus: focused item 'btn_removebot' (top index -1)",
    "WiredUI: push menu 'removebots' (depth 2)",
    "WiredUI: bot feeder selection row=0 client=2 allocation=1 generation=3 name=^3Visor",
    "wui_menu_nav focus: focused item 'botlist' (top index -1)",
    "WiredUI: bot feeder selection row=1 client=3 allocation=2 generation=3 name=^1Grunt",
    "wui_menu_nav: K_DOWNARROW dispatched",
    "wui_menu_nav focus: focused item 'btn_kick_bot' (top index -1)",
    "WiredUI: queued verified bot kick client=3 allocation=2",
    "wui_menu_nav: K_ENTER dispatched",
    "botkick: removed bot client=3 allocation=2 name=^1Grunt",
    "broadcast: print \"^1Grunt^7 was kicked\\n\"",
    "WiredUI: bot feeder roster generation=4 count=1",
    "WiredUI: bot feeder trace generation=4 count=1 selected_client=-1 selected_allocation=0",
    "WiredUI: bot feeder row=0 client=2 allocation=1 name=^3Visor",
    "WiredUI: pop menu (depth 1)",
    "wui_menu_nav: K_ESCAPE dispatched",
    "wui_menu_nav focus: focused item 'btn_addbot' (top index -1)",
    "WiredUI: push menu 'addbots' (depth 2)",
    "WiredUI: custom bot profile selected profile=visor generation=7",
    "wui_menu_nav: K_ENTER dispatched",
    "wui_menu_nav focus: focused item 'row_profile' (top index -1)",
    "wui_menu_nav: K_ENTER dispatched",
    "wui_menu_nav: K_DOWNARROW dispatched",
    "WiredUI: custom bot profile selected profile=grunt generation=7",
    "wui_menu_nav: K_ENTER dispatched",
    "wui_menu_nav focus: focused item 'row_name' (top index -1)",
    "wui_menu_nav: K_ENTER dispatched",
    "wui_menu_nav: typed 11 printable character(s)",
    "wui_menu_nav: K_ENTER dispatched",
    "wui_menu_nav focus: focused item 'row_skill' (top index -1)",
    "wui_menu_nav: K_ENTER dispatched",
    "wui_menu_nav: K_DOWNARROW dispatched",
    "wui_menu_nav: K_ENTER dispatched",
    "wui_menu_nav focus: focused item 'row_team' (top index -1)",
    "wui_menu_nav: K_ENTER dispatched",
    "wui_menu_nav: K_DOWNARROW dispatched",
    "wui_menu_nav: K_UPARROW dispatched",
    "wui_menu_nav: K_ENTER dispatched",
    "wui_menu_nav focus: focused item 'btn_add_custom' (top index -1)",
    "WiredUI: queued validated custom bot add profile=grunt name=CustomGrunt skill=4 team=free",
    "WiredUI: pop menu (depth 1)",
    "wui_menu_nav: K_ENTER dispatched",
    "broadcast: print \"CustomGrunt has entered the game\\n\"",
    "^2CustomGrunt^7 has entered the game",
    "WiredUI: bot feeder roster generation=5 count=2",
    "WiredUI: bot feeder trace generation=5 count=2 selected_client=-1 selected_allocation=0",
    "WiredUI: bot feeder row=0 client=2 allocation=1 name=^3Visor",
    "WiredUI: bot feeder row=1 client=3 allocation=3 name=CustomGrunt",
    "wui_menu_nav focus: focused item 'btn_addbot' (top index -1)",
    "WiredUI: push menu 'addbots' (depth 2)",
    "WiredUI: custom bot profile selected profile=visor generation=7",
    "wui_menu_nav: K_ENTER dispatched",
    "wui_menu_nav focus: focused item 'row_profile' (top index -1)",
    "wui_menu_nav: K_ENTER dispatched",
    "wui_menu_nav: K_DOWNARROW dispatched",
    "WiredUI: custom bot profile selected profile=grunt generation=7",
    "wui_menu_nav: K_ENTER dispatched",
    "wui_menu_nav focus: focused item 'row_name' (top index -1)",
    "wui_menu_nav: K_ENTER dispatched",
    "wui_menu_nav: typed 5 printable character(s)",
    "wui_menu_nav: K_ENTER dispatched",
    "wui_menu_nav focus: focused item 'btn_add_custom' (top index -1)",
    "WiredUI: custom bot add rejected invalid display name",
    "wui_menu_nav: K_ENTER dispatched",
    "wui_menu_nav focus: focused item 'btn_add_custom' (top index -1)",
    "wui_menu_nav focus: focused item 'row_name' (top index -1)",
    "wui_menu_nav: K_ENTER dispatched",
    "wui_menu_nav: K_BACKSPACE dispatched",
    "wui_menu_nav: K_BACKSPACE dispatched",
    "wui_menu_nav: K_BACKSPACE dispatched",
    "wui_menu_nav: K_BACKSPACE dispatched",
    "wui_menu_nav: K_BACKSPACE dispatched",
    "wui_menu_nav: K_ENTER dispatched",
    "wui_menu_nav focus: focused item 'btn_add_custom' (top index -1)",
    "CL_Characters: reloaded 13 character(s)",
    "WiredUI: custom bot add rejected stale profile selection",
    "wui_menu_nav: K_ENTER dispatched",
    "wui_menu_nav focus: focused item 'btn_add_custom' (top index -1)",
    "WiredUI: bot feeder trace generation=5 count=2 selected_client=-1 selected_allocation=0",
    "WiredUI: bot feeder row=0 client=2 allocation=1 name=^3Visor",
    "WiredUI: bot feeder row=1 client=3 allocation=3 name=CustomGrunt",
    "WiredUI: pop menu (depth 1)",
    "wui_menu_nav: K_ESCAPE dispatched",
    "wui_menu_nav focus: focused item 'btn_removebot' (top index -1)",
    "WiredUI: push menu 'removebots' (depth 2)",
    "WiredUI: bot feeder selection row=1 client=3 allocation=3 generation=5 name=CustomGrunt",
    "wui_menu_nav focus: focused item 'botlist' (top index -1)",
    "wui_menu_nav: K_DOWNARROW dispatched",
    "botkick: removed bot client=3 allocation=3 name=CustomGrunt",
    "broadcast: print \"CustomGrunt was kicked\\n\"",
    "WiredUI: bot feeder roster generation=6 count=1",
    "broadcast: print \"CustomGrunt has entered the game\\n\"",
    "^2CustomGrunt^7 has entered the game",
    "WiredUI: bot feeder roster generation=7 count=2",
    "wui_menu_nav focus: focused item 'btn_kick_bot' (top index -1)",
    "WiredUI: bot kick refused without a current bot selection",
    "wui_menu_nav: K_ENTER dispatched",
    "botkick: refused stale bot identity client=3 expected=3 actual=4",
    "WiredUI: bot feeder selection row=1 client=3 allocation=4 generation=7 name=CustomGrunt",
    "wui_menu_nav focus: focused item 'botlist' (top index -1)",
    "wui_menu_nav: K_DOWNARROW dispatched",
    "wui_menu_nav focus: focused item 'btn_kick_bot' (top index -1)",
    "WiredUI: queued verified bot kick client=3 allocation=4",
    "wui_menu_nav: K_ENTER dispatched",
    "botkick: removed bot client=3 allocation=4 name=CustomGrunt",
    "broadcast: print \"CustomGrunt was kicked\\n\"",
    "WiredUI: bot feeder roster generation=8 count=1",
    "WiredUI: bot feeder trace generation=8 count=1 selected_client=-1 selected_allocation=0",
    "WiredUI: bot feeder row=0 client=2 allocation=1 name=^3Visor",
    "WiredUI: shutdown",
]
game = "\n".join([
    "  0:00 ClientConnect: 1", "  0:00 ClientBegin: 1",
    "  0:01 ClientConnect: 2",
    r"  0:01 ClientUserinfoChanged: 2 n\^3Visor\t\0\char\visor\skin\default\c1\4\skill\3.00",
    "  0:01 ClientBegin: 2",
    "  0:02 ClientConnect: 3",
    r"  0:02 ClientUserinfoChanged: 3 n\^1Grunt\t\0\char\grunt\skin\default\c1\4\skill\3.00",
    "  0:02 ClientBegin: 3",
    "  0:04 ClientDisconnect: 3",
    "  0:05 ClientConnect: 3",
    r"  0:05 ClientUserinfoChanged: 3 n\CustomGrunt\t\0\char\grunt\skin\default\c1\4\skill\4.00",
    "  0:05 ClientBegin: 3", "",
    "  0:06 ClientDisconnect: 3",
    "  0:07 ClientConnect: 3",
    r"  0:07 ClientUserinfoChanged: 3 n\CustomGrunt\t\0\char\grunt\skin\default\c1\4\skill\4.00",
    "  0:07 ClientBegin: 3",
    "  0:08 ClientDisconnect: 3", "",
])
layout = []
for frame, (menu, region) in enumerate([
    ("addbots", "btn_quick_visor"),
    ("addbots", "btn_quick_grunt"),
    ("removebots", "botlist"),
    ("removebots", "btn_kick_bot"),
    ("addbots", "row_profile"),
    ("addbots", "row_name"),
    ("addbots", "row_skill"),
    ("addbots", "row_team"),
    ("addbots", "btn_add_custom"),
], 10):
    layout.append({"frame": frame, "menu": menu, "region": region,
                   "kind": "item", "focused": 1})

def remove(text):
    for index, message in enumerate(messages):
        if text in message:
            messages.pop(index)
            return

def remove_last(text):
    for index in range(len(messages) - 1, -1, -1):
        if text in messages[index]:
            messages.pop(index)
            return

if mode == "missing-first": remove("FIRST GAMEPLAY FRAME")
elif mode == "wrong-map": messages[0] = messages[0].replace("arena7", "arena1")
elif mode == "missing-nav": remove("navmesh ready")
elif mode == "missing-headless": remove("spawn_headless_client")
elif mode == "missing-human-begin": game = game.replace("  0:00 ClientBegin: 1\n", "")
elif mode == "missing-refusal": remove("refused non-bot")
elif mode == "missing-visor-action": remove("btn_quick_visor")
elif mode == "missing-visor-queue": remove("validated quick bot add profile=visor")
elif mode == "missing-grunt-action": remove("btn_quick_grunt")
elif mode == "missing-grunt-queue": remove("validated quick bot add profile=grunt")
elif mode == "wrong-grunt-profile":
    idx = messages.index("WiredUI: queued validated quick bot add profile=grunt skill=3 team=free")
    messages[idx] = "WiredUI: queued validated quick bot add profile=visor skill=3 team=free"
elif mode == "missing-second-enter":
    remove("Grunt^7 has entered")
elif mode == "missing-grunt-eligibility": remove("bot profile 'grunt' eligible")
elif mode == "wrong-grunt-primary":
    idx = next(i for i, message in enumerate(messages) if "bot profile 'grunt' eligible" in message)
    messages[idx] = messages[idx].replace("characters/visor/models/lower.md3",
                                         "characters/grunt/models/lower.md3")
elif mode == "missing-grunt-render": remove("loaded profile=grunt")
elif mode == "zero-grunt-render":
    idx = next(i for i, message in enumerate(messages) if "loaded profile=grunt" in message)
    messages[idx] = messages[idx].replace("legs=31", "legs=0")
elif mode == "wrong-grunt-parts":
    idx = next(i for i, message in enumerate(messages) if "loaded profile=grunt" in message)
    messages[idx] = messages[idx].replace("parts=3", "parts=2")
elif mode == "grunt-missing-part":
    messages.append("CG_LoadCharacter: 'grunt' part 'lower' not found (tried .iqm/.md3/.mdl)")
elif mode == "grunt-fallback":
    messages.append("CG_LoadCharacter: grunt fallback to DEFAULT_MODEL visor")
elif mode == "wrong-grunt-char":
    game = game.replace(r"\char\grunt\skin\default", r"\char\visor\skin\default")
elif mode == "wrong-grunt-skill":
    line = next(line for line in game.splitlines() if "ClientUserinfoChanged: 3" in line)
    game = game.replace(line, line.replace(r"\skill\3.00", r"\skill\4.00"))
elif mode == "wrong-trace-client":
    messages[messages.index("WiredUI: bot feeder row=0 client=2 allocation=1 name=^3Visor")] = \
        "WiredUI: bot feeder row=0 client=1 allocation=1 name=human"
elif mode == "missing-row0": remove("bot feeder selection row=0")
elif mode == "missing-down": remove("K_DOWNARROW")
elif mode == "wrong-row1":
    messages[messages.index("WiredUI: bot feeder selection row=1 client=3 allocation=2 generation=3 name=^1Grunt")] = \
        "WiredUI: bot feeder selection row=1 client=2 allocation=2 generation=3 name=^1Grunt"
elif mode == "wrong-initial-visor-selection":
    idx = messages.index("WiredUI: bot feeder selection row=0 client=2 allocation=1 generation=3 name=^3Visor")
    messages[idx] = "WiredUI: bot feeder selection row=0 client=3 allocation=1 generation=3 name=^3Visor"
elif mode == "wrong-grunt-trace":
    messages[messages.index("WiredUI: bot feeder row=1 client=3 allocation=2 name=^1Grunt")] = \
        "WiredUI: bot feeder row=1 client=3 allocation=2 name=^3Visor"
elif mode == "missing-queued": remove("queued verified bot kick")
elif mode == "wrong-queued-allocation":
    idx = next(i for i, message in enumerate(messages) if "queued verified bot kick" in message)
    messages[idx] = messages[idx].replace("allocation=2", "allocation=1")
elif mode == "legacy-clientkick": messages.insert(-1, "clientkick 3")
elif mode == "missing-removed": remove("botkick: removed")
elif mode == "missing-stale-bot-refusal": remove("refused stale bot identity")
elif mode == "wrong-stale-actual":
    idx = next(i for i, message in enumerate(messages) if "refused stale bot identity" in message)
    messages[idx] = messages[idx].replace("actual=4", "actual=2")
elif mode == "stale-accepted":
    idx = next(i for i, message in enumerate(messages) if "refused stale bot identity" in message)
    messages[idx] = "botkick: removed bot client=3 allocation=2 name=CustomGrunt"
    game += "  0:06 ClientDisconnect: 3\n"
elif mode == "missing-disconnect": game = game.replace("  0:04 ClientDisconnect: 3\n", "")
elif mode == "human-disconnect": game += "  0:05 ClientDisconnect: 1\n"
elif mode == "residual-client3": messages.insert(-1, "WiredUI: bot feeder row=1 client=3 allocation=2 name=^1Grunt")
elif mode == "ui-warn":
    pass
elif mode == "renderer-error":
    pass
elif mode == "missing-layout": layout = [row for row in layout if row["region"] != "btn_kick_bot"]
elif mode == "missing-grunt-layout": layout = [row for row in layout if row["region"] != "btn_quick_grunt"]
elif mode == "missing-custom-init": remove("custom bot profile selected profile=visor")
elif mode == "missing-custom-select": remove("custom bot profile selected profile=grunt")
elif mode == "wrong-custom-generation":
    idx = next(i for i, message in enumerate(messages) if "selected profile=grunt" in message)
    messages[idx] = messages[idx].replace("generation=7", "generation=8")
elif mode == "missing-custom-type": remove("typed 11 printable")
elif mode == "wrong-custom-queue-profile":
    idx = next(i for i, message in enumerate(messages) if "queued validated custom bot" in message)
    messages[idx] = messages[idx].replace("profile=grunt", "profile=visor")
elif mode == "wrong-custom-queue-name":
    idx = next(i for i, message in enumerate(messages) if "queued validated custom bot" in message)
    messages[idx] = messages[idx].replace("name=CustomGrunt", "name=Wrong")
elif mode == "wrong-custom-queue-skill":
    idx = next(i for i, message in enumerate(messages) if "queued validated custom bot" in message)
    messages[idx] = messages[idx].replace("skill=4", "skill=5")
elif mode == "wrong-custom-queue-team":
    idx = next(i for i, message in enumerate(messages) if "queued validated custom bot" in message)
    messages[idx] = messages[idx].replace("team=free", "team=red")
elif mode == "missing-custom-reconnect":
    game = game.replace("  0:05 ClientConnect: 3\n", "")
elif mode == "wrong-custom-userinfo":
    game = game.replace(r"n\CustomGrunt\t\0\char\grunt", r"n\Wrong\t\0\char\visor")
elif mode == "missing-custom-begin":
    game = game.replace("  0:05 ClientBegin: 3\n", "")
elif mode == "missing-custom-entered": remove("CustomGrunt has entered")
elif mode == "missing-b-cgame-enter":
    idx = max(i for i, message in enumerate(messages)
              if message == "^2CustomGrunt^7 has entered the game")
    del messages[idx]
elif mode == "additive-b-enter":
    idx = max(i for i, message in enumerate(messages)
              if message == "^2CustomGrunt^7 has entered the game")
    messages.insert(idx + 1, messages[idx])
elif mode == "wrong-custom-final-count":
    idx = next(i for i, message in enumerate(messages) if "generation=5 count=2" in message)
    messages[idx] = messages[idx].replace("count=2", "count=1")
elif mode == "wrong-custom-final-name":
    idx = max(i for i, message in enumerate(messages)
              if message == "WiredUI: bot feeder row=1 client=3 allocation=3 name=CustomGrunt")
    messages[idx] = "WiredUI: bot feeder row=1 client=3 allocation=3 name=Grunt"
elif mode == "wrong-custom-final-allocation":
    idx = max(i for i, message in enumerate(messages)
              if message == "WiredUI: bot feeder row=1 client=3 allocation=3 name=CustomGrunt")
    messages[idx] = "WiredUI: bot feeder row=1 client=3 allocation=2 name=CustomGrunt"
elif mode == "additive-bot-row":
    messages.insert(-1, "WiredUI: bot feeder row=1 client=3 allocation=99 name=CustomGrunt")
elif mode == "additive-bot-selection":
    messages.insert(-1, "WiredUI: bot feeder selection row=1 client=3 allocation=4 generation=8 name=CustomGrunt")
elif mode == "additive-botkick":
    messages.insert(-1, "botkick: refused stale bot identity client=3 expected=3 actual=4")
elif mode == "additive-bot-trace":
    messages.insert(-1, "WiredUI: bot feeder trace generation=8 count=1 selected_client=-1 selected_allocation=0")
elif mode == "duplicate-ui-kick-refusal":
    idx = next(i for i, message in enumerate(messages)
               if message == "WiredUI: bot kick refused without a current bot selection")
    messages.insert(idx + 1, messages[idx])
elif mode == "wrong-aba-selection-id":
    idx = next(i for i, message in enumerate(messages)
               if "selection row=1 client=3 allocation=3 generation=5" in message)
    messages[idx] = messages[idx].replace("allocation=3", "allocation=99")
elif mode == "wrong-aba-selection-generation":
    idx = next(i for i, message in enumerate(messages)
               if "selection row=1 client=3 allocation=3 generation=5" in message)
    messages[idx] = messages[idx].replace("generation=5", "generation=4")
elif mode == "missing-aba-focus":
    indices = [i for i, message in enumerate(messages) if "focused item 'btn_kick_bot'" in message]
    del messages[indices[-2]]
elif mode == "missing-aba-enter":
    refusal = messages.index("WiredUI: bot kick refused without a current bot selection")
    idx = next(i for i in range(refusal + 1, len(messages))
               if messages[i] == "wui_menu_nav: K_ENTER dispatched")
    del messages[idx]
elif mode == "missing-fresh-down":
    idx = max(i for i, message in enumerate(messages)
              if message == "wui_menu_nav: K_DOWNARROW dispatched")
    del messages[idx]
elif mode == "focus-before-selection-a":
    selection = next(i for i, message in enumerate(messages)
                     if "selection row=1 client=3 allocation=3 generation=5" in message)
    focus = next(i for i in range(selection + 1, len(messages))
                 if "focused item 'botlist'" in messages[i])
    messages[selection], messages[focus] = messages[focus], messages[selection]
elif mode == "focus-before-selection-b":
    selection = next(i for i, message in enumerate(messages)
                     if "selection row=1 client=3 allocation=4 generation=7" in message)
    focus = next(i for i in range(selection + 1, len(messages))
                 if "focused item 'botlist'" in messages[i])
    messages[selection], messages[focus] = messages[focus], messages[selection]
elif mode == "missing-custom-layout": layout = [row for row in layout if row["region"] != "btn_add_custom"]
elif mode == "missing-negative-type": remove("typed 5 printable")
elif mode == "missing-invalid-name-reject": remove("rejected invalid display name")
elif mode == "missing-invalid-retain":
    idx = next(i for i, message in enumerate(messages) if "rejected invalid display name" in message)
    del messages[idx + 2]
elif mode == "missing-name-restore": remove("K_BACKSPACE dispatched")
elif mode == "missing-character-reload": remove("CL_Characters: reloaded")
elif mode == "missing-stale-reject": remove("rejected stale profile selection")
elif mode == "missing-stale-retain": remove_last("focused item 'btn_add_custom'")
elif mode == "extra-custom-accept":
    idx = next(i for i, message in enumerate(messages) if "rejected invalid display name" in message)
    messages.insert(idx, "WiredUI: queued validated custom bot add profile=grunt name=Injected skill=4 team=free")
elif mode == "extra-custom-client": game += "  0:06 ClientConnect: 3\n  0:06 ClientBegin: 3\n"

rows = []
for message in messages:
    category = "cgame" if message.startswith("CG_LoadCharacter:") \
        or message.startswith("^2CustomGrunt^7 has entered") else \
        ("client" if message.startswith("CL_Characters:") else
         ("server" if message.startswith("botkick:")
          or message.startswith("broadcast:") else "ui"))
    severity = "WARN" if mode == "grunt-missing-part" and "part 'lower' not found" in message else \
        ("INFO" if message.startswith("botkick:") or message.startswith("broadcast:")
         or message.startswith("^2CustomGrunt^7 has entered") else "DEBUG")
    rows.append({"sev": severity, "cat": category, "msg": message})
if mode == "ui-warn": rows.append({"sev": "WARN", "cat": "ui", "msg": "generic warning"})
if mode == "renderer-error": rows.append({"sev": "ERROR", "cat": "renderer", "msg": "renderer failed"})
if mode == "wrong-botkick-category":
    idx = next(i for i, row in enumerate(rows) if row["msg"].startswith("botkick:"))
    rows[idx]["cat"] = "ui"
if mode == "wrong-ui-refusal-category":
    idx = next(i for i, row in enumerate(rows)
               if row["msg"] == "WiredUI: bot kick refused without a current bot selection")
    rows[idx]["cat"] = "server"
if mode == "wrong-b-enter-category":
    indices = [i for i, row in enumerate(rows)
               if row["msg"] == "^2CustomGrunt^7 has entered the game"]
    rows[indices[-1]]["cat"] = "ui"
with open(log_path, "w", encoding="utf-8") as out:
    for row in rows: out.write(json.dumps(row) + "\n")
with open(layout_path, "w", encoding="utf-8") as out:
    for row in layout: out.write(json.dumps(row) + "\n")
with open(game_path, "w", encoding="utf-8") as out: out.write(game)
PYEOF
}

if [ "${1:-}" = "--self-test" ]; then
    echo "==> WiredUI bot-action gate SELF-TEST"
    ROOT="$(mktemp -d -t wired-botst-XXXXXX 2>/dev/null || mktemp -d)"
    trap 'rm -rf "$ROOT"' EXIT INT TERM
    rc=0
    write_fixture clean "$ROOT/clean.jsonl" "$ROOT/clean-layout.jsonl" "$ROOT/clean-games.log"
    analyze_contract "$ROOT/clean.jsonl" "$ROOT/clean-layout.jsonl" "$ROOT/clean-games.log" \
        || { echo "FAIL: clean fixture rejected"; rc=1; }
    defects="missing-first wrong-map missing-nav missing-headless missing-human-begin missing-refusal missing-visor-action missing-visor-queue missing-grunt-action missing-grunt-queue wrong-grunt-profile missing-second-enter missing-grunt-eligibility wrong-grunt-primary missing-grunt-render zero-grunt-render wrong-grunt-parts grunt-missing-part grunt-fallback wrong-grunt-char wrong-grunt-skill wrong-grunt-trace wrong-trace-client missing-row0 missing-down wrong-row1 wrong-initial-visor-selection missing-queued wrong-queued-allocation legacy-clientkick missing-removed missing-stale-bot-refusal wrong-stale-actual stale-accepted missing-disconnect human-disconnect residual-client3 ui-warn renderer-error missing-layout missing-grunt-layout missing-custom-init missing-custom-select wrong-custom-generation missing-custom-type wrong-custom-queue-profile wrong-custom-queue-name wrong-custom-queue-skill wrong-custom-queue-team missing-custom-reconnect wrong-custom-userinfo missing-custom-begin missing-custom-entered missing-b-cgame-enter additive-b-enter wrong-b-enter-category wrong-custom-final-count wrong-custom-final-name wrong-custom-final-allocation additive-bot-row additive-bot-selection additive-botkick additive-bot-trace duplicate-ui-kick-refusal wrong-aba-selection-id wrong-aba-selection-generation wrong-botkick-category wrong-ui-refusal-category missing-aba-focus missing-aba-enter missing-fresh-down focus-before-selection-a focus-before-selection-b missing-custom-layout missing-negative-type missing-invalid-name-reject missing-invalid-retain missing-name-restore missing-character-reload missing-stale-reject missing-stale-retain extra-custom-accept extra-custom-client"
    for defect in $defects; do
        write_fixture "$defect" "$ROOT/$defect.jsonl" "$ROOT/$defect-layout.jsonl" "$ROOT/$defect-games.log"
        if analyze_contract "$ROOT/$defect.jsonl" "$ROOT/$defect-layout.jsonl" "$ROOT/$defect-games.log" >/dev/null 2>&1; then
            echo "FAIL: analyzer accepted defect $defect"
            rc=1
        else
            echo "  PASS rejected $defect"
        fi
    done
    [ "$rc" -eq 0 ] && echo "==> SELF-TEST PASS: clean accepted; eighty-three defects rejected"
    exit "$rc"
fi

WIRED="${1:-}"
if [ -z "$WIRED" ]; then echo "SKIP: pass a fully assembled Wired GUI binary"; exit 77; fi
if [ ! -x "$WIRED" ] && [ -x "$WIRED.exe" ]; then WIRED="$WIRED.exe"; fi
if [ ! -x "$WIRED" ]; then echo "SKIP: binary not executable: $WIRED"; exit 77; fi
case "$WIRED" in /*) : ;; *) WIRED="$PWD/$WIRED" ;; esac
WIRED_DIR="$(cd "$(dirname "$WIRED")" && pwd)"
WIRED="$WIRED_DIR/$(basename "$WIRED")"
if [ ! -f "$TIMEOUT_RUNNER" ] || ! python3 -c 'import sys; raise SystemExit(0 if sys.version_info >= (3,8) else 1)' 2>/dev/null; then
    echo "SKIP: Python >=3.8 and tests/run-with-timeout.py are required"; exit 77
fi

PACK_ROOT=""
for candidate in "$WIRED_DIR" "$WIRED_DIR/../Resources" "$WIRED_DIR/../../.."; do
    if [ -f "$candidate/base/pax21.sw3z" ]; then PACK_ROOT="$(cd "$candidate" && pwd)"; break; fi
done
if [ -z "$PACK_ROOT" ]; then echo "SKIP: current base/pax21.sw3z not found beside assembled product"; exit 77; fi

CONTENT_ROOT=""
for candidate in "${WIRED_CONTENT_ROOT:-}" "$PACK_ROOT"; do
    [ -n "$candidate" ] || continue
    if [ -f "$candidate/base/pax01.sw3z" ] || [ -f "$candidate/base/pak0.pk3" ]; then
        CONTENT_ROOT="$(cd "$candidate" && pwd)"; break
    fi
done
if [ -z "$CONTENT_ROOT" ]; then echo "SKIP: set WIRED_CONTENT_ROOT to canonical pax01.sw3z or pak0.pk3"; exit 77; fi
if [ -f "$CONTENT_ROOT/base/pax01.sw3z" ]; then BASE_ARCHIVE="$CONTENT_ROOT/base/pax01.sw3z"
else BASE_ARCHIVE="$CONTENT_ROOT/base/pak0.pk3"; fi

RUN_ROOT="$(mktemp -d -t wired-botchk-XXXXXX 2>/dev/null || mktemp -d)"
HOME_ROOT="$RUN_ROOT/q3now-preview"
mkdir -p "$HOME_ROOT/base"
cleanup() {
    if [ "${WIRED_KEEP_ARTIFACTS:-0}" = 1 ]; then echo "    kept artifacts: $RUN_ROOT"; else rm -rf "$RUN_ROOT"; fi
}
trap cleanup EXIT INT TERM
cp "$BASE_ARCHIVE" "$HOME_ROOT/base/" || exit 1
cp "$PACK_ROOT/base/pax21.sw3z" "$HOME_ROOT/base/pax21.sw3z" || exit 1

cat >"$HOME_ROOT/base/wiredui-bot-actions.cfg" <<'CFGEOF'
set g_log wiredui-bot-actions-games.log
set g_logsync 1
set g_autoBots 0
set bot_enable 1
set sv_maxclients 8
set cg_deferPlayers 0
set activeAction "exec wiredui-bot-actions-live.cfg"
map arena7
CFGEOF
cat >"$HOME_ROOT/base/wiredui-bot-actions-live.cfg" <<'CFGEOF'
# A cold isolated home builds arena7's navmesh asynchronously.  Bot add commands
# issued while that bake is in flight are deferred, so make readiness an explicit
# precondition of this identity/ABA contract rather than racing the bake.
waitms 10000
spawn_headless_client
wait 300
botkick 1 1
wait 30
wui_push ingame
wait 30
wui_menu_nav focus btn_addbot
wui_menu_nav enter
wait 25
wui_menu_nav focus btn_quick_visor
set r_layoutDump 1
wait 2
set r_layoutDump 0
wui_menu_nav enter
wait 30
wui_menu_nav focus btn_addbot
wui_menu_nav enter
wait 25
wui_menu_nav focus btn_quick_grunt
set r_layoutDump 1
wait 2
set r_layoutDump 0
wui_menu_nav enter
wait 300
wui_bot_trace
wui_menu_nav focus btn_removebot
wui_menu_nav enter
wait 100
wui_menu_nav focus botlist
set r_layoutDump 1
wait 2
set r_layoutDump 0
wui_menu_nav down
wait 10
wui_menu_nav focus btn_kick_bot
set r_layoutDump 1
wait 2
set r_layoutDump 0
wui_menu_nav enter
wait 100
wui_bot_trace
wait 20
wui_menu_nav back
wait 20
wui_menu_nav focus btn_addbot
wui_menu_nav enter
wait 25
wui_menu_nav focus row_profile
set r_layoutDump 1
wait 2
set r_layoutDump 0
wui_menu_nav enter
wui_menu_nav down
wui_menu_nav enter
wait 10
wui_menu_nav focus row_name
set r_layoutDump 1
wait 2
set r_layoutDump 0
wui_menu_nav enter
wui_menu_nav type "CustomGrunt"
wui_menu_nav enter
wui_menu_nav focus row_skill
set r_layoutDump 1
wait 2
set r_layoutDump 0
wui_menu_nav enter
wui_menu_nav down
wui_menu_nav enter
wui_menu_nav focus row_team
set r_layoutDump 1
wait 2
set r_layoutDump 0
wui_menu_nav enter
wui_menu_nav down
wui_menu_nav up
wui_menu_nav enter
wui_menu_nav focus btn_add_custom
set r_layoutDump 1
wait 2
set r_layoutDump 0
wui_menu_nav enter
wait 300
wui_bot_trace
wait 20
wui_menu_nav focus btn_addbot
wui_menu_nav enter
wait 25
wui_menu_nav focus row_profile
wui_menu_nav enter
wui_menu_nav down
wui_menu_nav enter
wait 10
wui_menu_nav focus row_name
wui_menu_nav enter
wui_menu_nav type ";quit"
wui_menu_nav enter
wui_menu_nav focus btn_add_custom
wui_menu_nav enter
wait 10
wui_menu_nav focus btn_add_custom
wui_menu_nav focus row_name
wui_menu_nav enter
wui_menu_nav backspace
wui_menu_nav backspace
wui_menu_nav backspace
wui_menu_nav backspace
wui_menu_nav backspace
wui_menu_nav enter
wui_menu_nav focus btn_add_custom
reload_characters
wait 20
wui_menu_nav enter
wait 10
wui_menu_nav focus btn_add_custom
wui_bot_trace
wait 10
wui_menu_nav back
wait 20
wui_menu_nav focus btn_removebot
wui_menu_nav enter
wait 100
wui_menu_nav focus botlist
wui_menu_nav down
wait 10
botkick 3 3
wait 100
addbot "grunt" 4 free 0 "CustomGrunt"
wait 300
wui_menu_nav focus btn_kick_bot
wui_menu_nav enter
wait 20
botkick 3 3
wait 20
wui_menu_nav focus botlist
wui_menu_nav down
wui_menu_nav focus btn_kick_bot
wui_menu_nav enter
wait 100
wui_bot_trace
wait 10
quit
CFGEOF

case "$(uname -s)" in
    Darwin) NATIVE_HOME="$HOME_ROOT"; PLATFORM_ARGS=( -ApplePersistenceIgnoreState YES ) ;;
    MINGW*|MSYS*|CYGWIN*) NATIVE_HOME="$(cygpath -w "$HOME_ROOT")"; PLATFORM_ARGS=() ;;
    *) NATIVE_HOME="$HOME_ROOT"; PLATFORM_ARGS=() ;;
esac
PRODUCT_JSON="$HOME_ROOT/qconsole.jsonl"
GAME_LOG="$HOME_ROOT/base/wiredui-bot-actions-games.log"
LAYOUT_JSON="$RUN_ROOT/layoutdump.jsonl"
STDOUT_LOG="$RUN_ROOT/wired.stdout"

echo "==> WiredUI bot actions: arena7, headless human slot1, bots slots2/3"
python3 "$TIMEOUT_RUNNER" --timeout 240 --kill-after 15 --cwd "$RUN_ROOT" --stdout "$STDOUT_LOG" -- \
    "$WIRED" "${PLATFORM_ARGS[@]}" +set fs_homepath "$NATIVE_HOME" +set com_automated 1 \
    +set com_noHardReboot 1 +set s_initsound 0 +set r_fullscreen 0 \
    +set r_mode -1 +set r_customwidth 1280 +set r_customheight 720 +set r_layoutDump 0 \
    +set log_severity DEBUG +set log_file_severity DEBUG +set log_file_mode overwrite_synced \
    +exec wiredui-bot-actions.cfg
run_rc=$?
if [ "$run_rc" -ne 0 ]; then
    echo "FAIL: product exited with status $run_rc"
    [ -s "$STDOUT_LOG" ] && tail -40 "$STDOUT_LOG"
    exit 1
fi
for artifact in "$PRODUCT_JSON" "$LAYOUT_JSON" "$GAME_LOG"; do
    [ -s "$artifact" ] || { echo "FAIL: required artifact missing: $artifact"; exit 1; }
done
analyze_contract "$PRODUCT_JSON" "$LAYOUT_JSON" "$GAME_LOG" || exit 1
echo "==> WiredUI bot-action gate: PASS"
