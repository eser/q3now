#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Deterministic M1 WiredUI behavior gate.  Two clean engine processes prove:
#   1. representative real behavior plus lifecycle coverage for all 15
#      alpha-inventory menus; and
#   2. main -> Host -> Start Server -> a feeder-selected real map -> the first
#      active gameplay frame.
#
# No user config or install is modified.  The current pax21 is copied into an
# isolated home while licensed base content is mounted read-only via
# fs_basepath.
#
# Usage: bash tests/wiredui-menu-functional-check.sh --self-test
#        WIRED_CONTENT_ROOT=/installed/q3now-preview \
#          bash tests/wiredui-menu-functional-check.sh /assembled/wired
# Exit: 0 PASS, 1 FAIL, 77 unavailable binary/current pack/base content.

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
TIMEOUT_RUNNER="$SCRIPT_DIR/run-with-timeout.py"
EXPECTED_MAP="${EXPECTED_MAP:-arena17}"

analyze_contract() {
    python3 - "$1" "$2" "$3" "$4" "$5" <<'PYEOF'
import json
import re
import sys

expected_map, inv_log_path, inv_layout_path, game_log_path, game_layout_path = sys.argv[1:6]

def load_jsonl(path):
    rows = []
    try:
        with open(path, "r", encoding="utf-8", errors="replace") as stream:
            for line_no, line in enumerate(stream, 1):
                line = line.strip()
                if not line:
                    continue
                try:
                    rows.append(json.loads(line))
                except ValueError as exc:
                    print(f"FAIL: {path}:{line_no}: invalid JSON: {exc}")
                    raise SystemExit(1)
    except OSError as exc:
        print(f"FAIL: cannot read {path}: {exc}")
        raise SystemExit(1)
    return rows

inventory = load_jsonl(inv_log_path)
inv_layout = load_jsonl(inv_layout_path)
gameplay = load_jsonl(game_log_path)
game_layout = load_jsonl(game_layout_path)
fail = False

def check_severity(label, rows):
    global fail
    errors = [row for row in rows if str(row.get("sev", "")).upper() in {"ERROR", "FATAL"}]
    # Product runs can legitimately report non-UI content diagnostics (for
    # example optional character icons) and renderer PIPE-PIN warnings.  The
    # M1 contract owns every ERROR/FATAL plus every warning emitted by the
    # authoritative UI category, independent of message wording.
    ui_warn = [row for row in rows
               if str(row.get("sev", "")).upper() == "WARN"
               and str(row.get("cat", "")).lower() == "ui"]
    bad = errors + ui_warn
    if bad:
        print(f"FAIL {label}: observed {len(errors)} ERROR/FATAL and {len(ui_warn)} cat=ui WARN record(s)")
        for row in bad[:8]:
            print(f"  {row.get('sev', '?')}: {row.get('msg', '')}")
        fail = True
    else:
        print(f"  PASS {label}: zero ERROR/FATAL and zero cat=ui WARN records")

def ordered_trace(label, rows, steps):
    global fail
    messages = [str(row.get("msg", "")) for row in rows]
    cursor = 0
    for step_name, pattern in steps:
        for index in range(cursor, len(messages)):
            if re.search(pattern, messages[index]):
                cursor = index + 1
                break
        else:
            print(f"FAIL {label}: missing/out-of-order step: {step_name}")
            fail = True
            return
    print(f"  PASS {label}: {len(steps)} ordered behavior steps")

def ordered_layout(label, rows, steps):
    global fail
    focused = sorted(
        (int(row.get("frame", 0)), str(row.get("menu", "")), str(row.get("region", "")))
        for row in rows
        if row.get("kind") == "item" and int(row.get("focused", 0)) == 1
    )
    cursor = 0
    last_frame = -1
    matched = []
    for wanted in steps:
        for index in range(cursor, len(focused)):
            frame, menu, region = focused[index]
            if frame > last_frame and (menu, region) == wanted:
                cursor = index + 1
                last_frame = frame
                matched.append(frame)
                break
        else:
            print(f"FAIL {label}: missing ordered focus evidence {wanted[0]}/{wanted[1]}")
            fail = True
            return
    print(f"  PASS {label}: {len(steps)} focused surfaces at frames " + ", ".join(map(str, matched)))

check_severity("inventory logs", inventory)
check_severity("gameplay logs", gameplay)

inventory_steps = [
    ("main root", r"WiredUI: push menu 'main' \(depth 1\)"),
    ("preferences open", r"WiredUI: push menu 'preferences' \(depth 2\)"),
    ("preferences checkbox focus", r"focused item 'row_simple_items'"),
    ("preferences mutation", r'"cg_simpleItems" is:"1(?:\^7)?"'),
    ("preferences return", r"WiredUI: pop menu \(depth 1\)"),
    ("controls open", r"WiredUI: push menu 'controls' \(depth 2\)"),
    ("controls binding focus", r"focused item 'row_forward'"),
    ("controls capture", r"wui_menu_nav: K_ENTER dispatched"),
    ("controls key injection", r"wui_menu_nav: K_DOWNARROW dispatched"),
    ("controls binding mutation", r'"DOWNARROW" = "\+forward"'),
    ("controls return", r"WiredUI: pop menu \(depth 1\)"),
    ("display open", r"WiredUI: push menu 'display' \(depth 2\)"),
    ("display focus", r"focused item 'row_ingame_video'"),
    ("display mutation", r'"r_inGameVideo" is:"1(?:\^7)?"'),
    ("display return", r"WiredUI: pop menu \(depth 1\)"),
    ("sound open", r"WiredUI: push menu 'sound' \(depth 2\)"),
    ("sound focus", r"focused item 'row_master'"),
    ("sound mutation", r'"s_volume" is:"0\.55(?:0+)?(?:\^7)?"'),
    ("sound return", r"WiredUI: pop menu \(depth 1\)"),
    ("video open", r"WiredUI: push menu 'video' \(depth 2\)"),
    ("video focus", r"focused item 'row_fastsky'"),
    ("video mutation", r'"r_fastsky" is:"1(?:\^7)?"'),
    ("video return", r"WiredUI: pop menu \(depth 1\)"),
    ("network open", r"WiredUI: push menu 'network' \(depth 2\)"),
    ("network focus", r"focused item 'row_autodl'"),
    ("network mutation", r'"cl_allowDownload" is:"1(?:\^7)?"'),
    ("network return", r"WiredUI: pop menu \(depth 1\)"),
    ("playersettings open", r"WiredUI: push menu 'playersettings' \(depth 2\)"),
    ("playersettings segmented focus", r"focused item 'row_crosshair'"),
    ("playersettings segmented mutation", r'"cg_crosshairAlpha" is:"30(?:\.0+)?(?:\^7)?"'),
    ("playersettings feeder focus", r"focused item 'charlist'"),
    ("playersettings feeder mutation", r"char is set to (?!sarge(?:\s|$))[^\s]+"),
    ("playersettings return", r"WiredUI: pop menu \(depth 1\)"),
    ("servers open", r"WiredUI: push menu 'servers' \(depth 2\)"),
    ("servers filter focus", r"focused item 'filter_showfull'"),
    ("servers filter Enter/action", r"wui_menu_nav: K_ENTER dispatched"),
    ("servers specify action", r"focused item 'btn_specify'"),
    ("specifyserver open", r"WiredUI: push menu 'specifyserver' \(depth 3\)"),
    ("specifyserver edit focus", r"focused item 'ip_entry'"),
    ("specifyserver cancel focus", r"focused item 'btn_cancel'"),
    ("specifyserver return", r"WiredUI: pop menu \(depth 2\)"),
    ("servers return", r"WiredUI: pop menu \(depth 1\)"),
    ("startserver open", r"WiredUI: push menu 'startserver' \(depth 2\)"),
    ("startserver dropdown focus", r"focused item 'row_skill'"),
    ("startserver dropdown mutation", r'"g_skill" is:"2(?:\.0+)?(?:\^7)?"'),
    ("startserver addbots action", r"focused item 'btn_add_bots'"),
    ("addbots open", r"WiredUI: push menu 'addbots' \(depth 3\)"),
    ("addbots dropdown focus", r"focused item 'row_skill'"),
    ("addbots dropdown mutation", r'"g_skill" is:"2(?:\.0+)?(?:\^7)?"'),
    ("addbots return", r"WiredUI: pop menu \(depth 2\)"),
    ("startserver return", r"WiredUI: pop menu \(depth 1\)"),
    ("removebots open", r"WiredUI: push menu 'removebots' \(depth 2\)"),
    ("removebots list focus", r"focused item 'botlist'"),
    ("removebots list input", r"wui_menu_nav: K_DOWNARROW dispatched"),
    ("removebots return", r"WiredUI: pop menu \(depth 1\)"),
    ("demos open", r"WiredUI: push menu 'demos' \(depth 2\)"),
    ("demos list focus", r"focused item 'demolist'"),
    ("demos list input", r"wui_menu_nav: K_DOWNARROW dispatched"),
    ("demos return", r"WiredUI: pop menu \(depth 1\)"),
    ("mods open", r"WiredUI: push menu 'mods' \(depth 2\)"),
    ("mods list focus", r"focused item 'modlist'"),
    ("mods list input", r"wui_menu_nav: K_DOWNARROW dispatched"),
    ("mods return", r"WiredUI: pop menu \(depth 1\)"),
    ("serverinfo open", r"WiredUI: push menu 'serverinfo' \(depth 2\)"),
    ("serverinfo parent action", r"focused item 'btn_back'"),
    ("serverinfo close", r"WiredUI: pop menu \(depth 1\)"),
    ("serverinfo returns to servers", r"WiredUI: push menu 'servers' \(depth 2\)"),
    ("returned servers close", r"WiredUI: pop menu \(depth 1\)"),
    ("main closes", r"WiredUI: pop menu \(depth 0\)"),
    ("main reopens", r"WiredUI: push menu 'main' \(depth 1\)"),
    ("reopened main focus", r"focused item 'menu_host'"),
    ("reopened main closes", r"WiredUI: pop menu \(depth 0\)"),
]
ordered_trace("inventory trace", inventory, inventory_steps)

inventory_layout_steps = [
    ("preferences", "row_simple_items"),
    ("controls", "row_forward"),
    ("display", "row_ingame_video"),
    ("sound", "row_master"),
    ("video", "row_fastsky"),
    ("network", "row_autodl"),
    ("playersettings", "charlist"),
    ("servers", "filter_showfull"),
    ("specifyserver", "ip_entry"),
    ("startserver", "row_skill"),
    ("addbots", "row_skill"),
    ("removebots", "botlist"),
    ("demos", "demolist"),
    ("mods", "modlist"),
    ("serverinfo", "btn_back"),
    ("main", "menu_host"),
]
ordered_layout("inventory layout", inv_layout, inventory_layout_steps)

escaped_map = re.escape(expected_map)
gameplay_steps = [
    ("gameplay main root", r"WiredUI: push menu 'main' \(depth 1\)"),
    ("host focus", r"focused item 'menu_host'"),
    ("host action opens startserver", r"WiredUI: push menu 'startserver' \(depth 2\)"),
    ("map list focus", r"focused item 'maplist'"),
    ("map row0 to row1 callback", r"WiredUI: map feeder selection filtered_row=1 map=arena13"),
    ("map row0 to row1 input returns", r"wui_menu_nav: K_DOWNARROW dispatched"),
    ("map row1 to row2 callback", r"WiredUI: map feeder selection filtered_row=2 map=arena17"),
    ("map row1 to row2 input returns", r"wui_menu_nav: K_DOWNARROW dispatched"),
    ("start focus", r"focused item 'btn_start'"),
    ("close-all postcondition", r"WiredUI: close all postcondition depth=0 active=none catcher_ui=0 paused=0"),
    ("start Enter returns", r"wui_menu_nav: K_ENTER dispatched"),
    ("selected map reaches gameplay", rf"FIRST GAMEPLAY FRAME mapname=(?:maps/)?{escaped_map}(?:\.bsp)?(?:\s|$)"),
]
ordered_trace("gameplay trace", gameplay, gameplay_steps)
ordered_layout("gameplay layout", game_layout, [
    ("main", "menu_host"),
    ("startserver", "maplist"),
    ("startserver", "btn_start"),
])

raise SystemExit(1 if fail else 0)
PYEOF
}

write_fixture() {
    python3 - "$1" "$2" "$3" "$4" "$5" "$EXPECTED_MAP" <<'PYEOF'
import json
import sys

mode, inv_log, inv_layout, game_log, game_layout, expected_map = sys.argv[1:7]

inventory_messages = [
    "WiredUI: push menu 'main' (depth 1)",
    "WiredUI: push menu 'preferences' (depth 2)", "wui_menu_nav focus: focused item 'row_simple_items'", '"cg_simpleItems" is:"1"', "WiredUI: pop menu (depth 1)",
    "WiredUI: push menu 'controls' (depth 2)", "wui_menu_nav focus: focused item 'row_forward'", "wui_menu_nav: K_ENTER dispatched", "wui_menu_nav: K_DOWNARROW dispatched", '"DOWNARROW" = "+forward"', "WiredUI: pop menu (depth 1)",
    "WiredUI: push menu 'display' (depth 2)", "wui_menu_nav focus: focused item 'row_ingame_video'", '"r_inGameVideo" is:"1"', "WiredUI: pop menu (depth 1)",
    "WiredUI: push menu 'sound' (depth 2)", "wui_menu_nav focus: focused item 'row_master'", '"s_volume" is:"0.55"', "WiredUI: pop menu (depth 1)",
    "WiredUI: push menu 'video' (depth 2)", "wui_menu_nav focus: focused item 'row_fastsky'", '"r_fastsky" is:"1"', "WiredUI: pop menu (depth 1)",
    "WiredUI: push menu 'network' (depth 2)", "wui_menu_nav focus: focused item 'row_autodl'", '"cl_allowDownload" is:"1"', "WiredUI: pop menu (depth 1)",
    "WiredUI: push menu 'playersettings' (depth 2)", "wui_menu_nav focus: focused item 'row_crosshair'", '"cg_crosshairAlpha" is:"30"', "wui_menu_nav focus: focused item 'charlist'", "char is set to skulk", "WiredUI: pop menu (depth 1)",
    "WiredUI: push menu 'servers' (depth 2)", "wui_menu_nav focus: focused item 'filter_showfull'", "wui_menu_nav: K_ENTER dispatched", "wui_menu_nav focus: focused item 'btn_specify'", "WiredUI: push menu 'specifyserver' (depth 3)", "wui_menu_nav focus: focused item 'ip_entry'", "wui_menu_nav focus: focused item 'btn_cancel'", "WiredUI: pop menu (depth 2)", "WiredUI: pop menu (depth 1)",
    "WiredUI: push menu 'startserver' (depth 2)", "wui_menu_nav focus: focused item 'row_skill'", '"g_skill" is:"2"', "wui_menu_nav focus: focused item 'btn_add_bots'", "WiredUI: push menu 'addbots' (depth 3)", "wui_menu_nav focus: focused item 'row_skill'", '"g_skill" is:"2"', "WiredUI: pop menu (depth 2)", "WiredUI: pop menu (depth 1)",
    "WiredUI: push menu 'removebots' (depth 2)", "wui_menu_nav focus: focused item 'botlist'", "wui_menu_nav: K_DOWNARROW dispatched", "WiredUI: pop menu (depth 1)",
    "WiredUI: push menu 'demos' (depth 2)", "wui_menu_nav focus: focused item 'demolist'", "wui_menu_nav: K_DOWNARROW dispatched", "WiredUI: pop menu (depth 1)",
    "WiredUI: push menu 'mods' (depth 2)", "wui_menu_nav focus: focused item 'modlist'", "wui_menu_nav: K_DOWNARROW dispatched", "WiredUI: pop menu (depth 1)",
    "WiredUI: push menu 'serverinfo' (depth 2)", "wui_menu_nav focus: focused item 'btn_back'", "WiredUI: pop menu (depth 1)", "WiredUI: push menu 'servers' (depth 2)", "WiredUI: pop menu (depth 1)",
    "WiredUI: pop menu (depth 0)", "WiredUI: push menu 'main' (depth 1)", "wui_menu_nav focus: focused item 'menu_host'", "WiredUI: pop menu (depth 0)",
]

layout_items = [
    ("preferences", "row_simple_items"), ("controls", "row_forward"),
    ("display", "row_ingame_video"), ("sound", "row_master"),
    ("video", "row_fastsky"), ("network", "row_autodl"),
    ("playersettings", "charlist"), ("servers", "filter_showfull"),
    ("specifyserver", "ip_entry"), ("startserver", "row_skill"),
    ("addbots", "row_skill"), ("removebots", "botlist"),
    ("demos", "demolist"), ("mods", "modlist"),
    ("serverinfo", "btn_back"), ("main", "menu_host"),
]

game_messages = [
    "WiredUI: push menu 'main' (depth 1)",
    "wui_menu_nav focus: focused item 'menu_host'",
    "WiredUI: push menu 'startserver' (depth 2)",
    "wui_menu_nav focus: focused item 'maplist'",
    "WiredUI: map feeder selection filtered_row=1 map=arena13",
    "wui_menu_nav: K_DOWNARROW dispatched",
    "WiredUI: map feeder selection filtered_row=2 map=arena17",
    "wui_menu_nav: K_DOWNARROW dispatched",
    "wui_menu_nav focus: focused item 'btn_start'",
    "WiredUI: close all postcondition depth=0 active=none catcher_ui=0 paused=0",
    "wui_menu_nav: K_ENTER dispatched",
    f"cls.state: -> CA_ACTIVE (FIRST GAMEPLAY FRAME mapname=maps/{expected_map}.bsp serverTime=100 numEntities=7 framecount=9)",
]
game_layout_items = [("main", "menu_host"), ("startserver", "maplist"), ("startserver", "btn_start")]

if mode == "missing-menu":
    inventory_messages.remove("WiredUI: push menu 'mods' (depth 2)")
elif mode == "unchanged-binding":
    inventory_messages[inventory_messages.index('"DOWNARROW" = "+forward"')] = '"DOWNARROW" is not bound'
elif mode == "missing-map-down":
    game_messages.remove("wui_menu_nav: K_DOWNARROW dispatched")
elif mode == "missing-feeder-callback":
    game_messages.remove("WiredUI: map feeder selection filtered_row=2 map=arena17")
elif mode == "wrong-feeder-callback":
    index = game_messages.index("WiredUI: map feeder selection filtered_row=2 map=arena17")
    game_messages[index] = "WiredUI: map feeder selection filtered_row=2 map=arena13"
elif mode == "missing-close":
    game_messages.remove("WiredUI: close all postcondition depth=0 active=none catcher_ui=0 paused=0")
elif mode == "bad-close-catcher":
    index = game_messages.index("WiredUI: close all postcondition depth=0 active=none catcher_ui=0 paused=0")
    game_messages[index] = "WiredUI: close all postcondition depth=0 active=none catcher_ui=1 paused=0"
elif mode == "first-before-start":
    marker = game_messages.pop()
    game_messages.insert(6, marker)
elif mode == "wrong-map":
    wrong_map = "arena13" if expected_map != "arena13" else "arena17"
    game_messages[-1] = f"cls.state: -> CA_ACTIVE (FIRST GAMEPLAY FRAME mapname=maps/{wrong_map}.bsp serverTime=100 numEntities=7 framecount=9)"
elif mode == "warn":
    pass
elif mode == "fatal":
    pass
elif mode == "error":
    pass
elif mode == "missing-layout":
    layout_items.remove(("mods", "modlist"))
elif mode == "missing-reopen":
    del inventory_messages[-3:]

def dump_log(path, messages, add_warn=False, add_fatal=False, add_error=False):
    with open(path, "w", encoding="utf-8") as stream:
        for msg in messages:
            stream.write(json.dumps({"sev": "DEBUG", "cat": "ui", "msg": msg}) + "\n")
        # Expected non-UI diagnostics must not make a behavior gate red.
        stream.write(json.dumps({"sev": "WARN", "cat": "client", "msg": "character icon is optional"}) + "\n")
        stream.write(json.dumps({"sev": "WARN", "cat": "renderer", "msg": "PIPE-PIN informational"}) + "\n")
        if add_warn:
            stream.write(json.dumps({"sev": "WARN", "cat": "ui", "msg": "generic synthetic warning"}) + "\n")
        if add_fatal:
            stream.write(json.dumps({"sev": "FATAL", "cat": "renderer", "msg": "synthetic fatal outside UI"}) + "\n")
        if add_error:
            stream.write(json.dumps({"sev": "ERROR", "cat": "renderer", "msg": "synthetic renderer error"}) + "\n")

def dump_layout(path, items):
    with open(path, "w", encoding="utf-8") as stream:
        for frame, (menu, region) in enumerate(items, 10):
            stream.write(json.dumps({"frame": frame, "menu": menu, "region": region, "kind": "item", "focused": 1}) + "\n")

dump_log(inv_log, inventory_messages, mode == "warn", mode == "fatal", mode == "error")
dump_layout(inv_layout, layout_items)
dump_log(game_log, game_messages)
dump_layout(game_layout, game_layout_items)
PYEOF
}

if [ "${1:-}" = "--self-test" ]; then
    echo "==> WiredUI menu-functional gate SELF-TEST"
    fixture_root="$(mktemp -d -t wired-menust-XXXXXX 2>/dev/null || mktemp -d)"
    trap 'rm -rf "$fixture_root"' EXIT INT TERM
    rc=0

    write_fixture clean "$fixture_root/inv.jsonl" "$fixture_root/inv-layout.jsonl" "$fixture_root/game.jsonl" "$fixture_root/game-layout.jsonl"
    echo "  -- CLEAN -> expect PASS --"
    analyze_contract "$EXPECTED_MAP" "$fixture_root/inv.jsonl" "$fixture_root/inv-layout.jsonl" "$fixture_root/game.jsonl" "$fixture_root/game-layout.jsonl" \
        || { echo "  FAIL: clean fixture rejected"; rc=1; }

    for defect in missing-menu unchanged-binding missing-map-down missing-feeder-callback wrong-feeder-callback missing-close bad-close-catcher first-before-start wrong-map warn fatal error missing-layout missing-reopen; do
        write_fixture "$defect" "$fixture_root/$defect-inv.jsonl" "$fixture_root/$defect-inv-layout.jsonl" "$fixture_root/$defect-game.jsonl" "$fixture_root/$defect-game-layout.jsonl"
        echo "  -- DEFECT $defect -> expect FAIL --"
        if analyze_contract "$EXPECTED_MAP" "$fixture_root/$defect-inv.jsonl" "$fixture_root/$defect-inv-layout.jsonl" "$fixture_root/$defect-game.jsonl" "$fixture_root/$defect-game-layout.jsonl"; then
            echo "  FAIL: gate accepted $defect"
            rc=1
        else
            echo "  PASS: gate rejected $defect"
        fi
    done

    if [ "$rc" -eq 0 ]; then
        echo "==> SELF-TEST PASS: clean contract accepted; fourteen defects rejected"
    else
        echo "==> SELF-TEST FAIL"
    fi
    exit "$rc"
fi

WIRED="${1:-}"
if [ -z "$WIRED" ]; then
    echo "SKIP: pass a fully assembled Wired binary"
    exit 77
fi
if [ ! -x "$WIRED" ] && [ -x "$WIRED.exe" ]; then WIRED="$WIRED.exe"; fi
if [ ! -x "$WIRED" ]; then
    echo "SKIP: wired binary not found or not executable: $WIRED"
    exit 77
fi
case "$WIRED" in /*) : ;; *) WIRED="$PWD/$WIRED" ;; esac
WIRED_DIR="$(cd "$(dirname "$WIRED")" && pwd)"
WIRED="$WIRED_DIR/$(basename "$WIRED")"

if [ ! -f "$TIMEOUT_RUNNER" ] || ! python3 -c 'import sys; raise SystemExit(0 if sys.version_info >= (3,8) else 1)' 2>/dev/null; then
    echo "SKIP: Python >=3.8 and tests/run-with-timeout.py are required"
    exit 77
fi
case "$EXPECTED_MAP" in
    ''|*[!A-Za-z0-9_.-]*) echo "FAIL: EXPECTED_MAP contains unsafe characters: $EXPECTED_MAP"; exit 1 ;;
esac

PACK_ROOT=""
for candidate in "$WIRED_DIR" "$WIRED_DIR/../Resources" "$WIRED_DIR/../../.."; do
    if [ -f "$candidate/base/pax21.sw3z" ]; then
        PACK_ROOT="$(cd "$candidate" && pwd)"
        break
    fi
done
if [ -z "$PACK_ROOT" ]; then
    echo "SKIP: no current base/pax21.sw3z found beside bundle/build for $WIRED"
    exit 77
fi

CONTENT_ROOT=""
for candidate in "${WIRED_CONTENT_ROOT:-}" "$PACK_ROOT"; do
    [ -n "$candidate" ] || continue
    if [ -f "$candidate/base/pax01.sw3z" ] || [ -f "$candidate/base/pak0.pk3" ]; then
        CONTENT_ROOT="$(cd "$candidate" && pwd)"
        break
    fi
done
if [ -z "$CONTENT_ROOT" ]; then
    echo "SKIP: licensed base content missing; set WIRED_CONTENT_ROOT to a root containing base/pax01.sw3z or base/pak0.pk3"
    exit 77
fi

PRODUCT_DIRNAME="q3now-preview"
RUN_ROOT="$(mktemp -d -t wired-menuchk-XXXXXX 2>/dev/null || mktemp -d)"
INV_PARENT="$RUN_ROOT/inventory"
GAME_PARENT="$RUN_ROOT/gameplay"
INV_HOME="$INV_PARENT/$PRODUCT_DIRNAME"
GAME_HOME="$GAME_PARENT/$PRODUCT_DIRNAME"
mkdir -p "$INV_HOME/base" "$GAME_HOME/base"
if ! cp "$PACK_ROOT/base/pax21.sw3z" "$INV_HOME/base/pax21.sw3z" ||
   ! cp "$PACK_ROOT/base/pax21.sw3z" "$GAME_HOME/base/pax21.sw3z"; then
    echo "FAIL: could not stage current pax21.sw3z into isolated homes"
    exit 1
fi

# Launcher installs do not expose pax01/pak0 to a foreign fs_homepath merely
# through fs_basepath.  Stage every licensed base archive into both isolated
# homes (copy only; source remains read-only), then let current pax21 shadow the
# product UI in the same directory.  This mirrors fps-perf-gate's proven mount
# model and makes maps_list[] see the real arena roster.
BASE_ARCHIVE_COUNT=0
for content_archive in "$CONTENT_ROOT"/base/pax0*.sw3z "$CONTENT_ROOT"/base/pak*.pk3; do
    [ -f "$content_archive" ] || continue
    if ! cp "$content_archive" "$INV_HOME/base/" ||
       ! cp "$content_archive" "$GAME_HOME/base/"; then
        echo "FAIL: could not stage licensed base archive: $content_archive"
        exit 1
    fi
    BASE_ARCHIVE_COUNT=$((BASE_ARCHIVE_COUNT + 1))
done
if [ "$BASE_ARCHIVE_COUNT" -eq 0 ]; then
    echo "FAIL: no pax0*.sw3z or pak*.pk3 archives were staged from $CONTENT_ROOT/base"
    exit 1
fi
case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*)
        INV_HOME_NATIVE="$(cygpath -w "$INV_HOME")"
        GAME_HOME_NATIVE="$(cygpath -w "$GAME_HOME")"
        ;;
    *)
        INV_HOME_NATIVE="$INV_HOME"
        GAME_HOME_NATIVE="$GAME_HOME"
        ;;
esac

INV_CFG="$INV_HOME/base/wiredui-menu-inventory.cfg"
GAME_CFG="$GAME_HOME/base/wiredui-menu-gameplay.cfg"
INV_JSONL="$INV_HOME/qconsole.jsonl"
GAME_JSONL="$GAME_HOME/qconsole.jsonl"
INV_LAYOUT="$INV_PARENT/layoutdump.jsonl"
GAME_LAYOUT="$GAME_PARENT/layoutdump.jsonl"
INV_STDOUT="$INV_PARENT/wired.stdout"
GAME_STDOUT="$GAME_PARENT/wired.stdout"

cleanup_runtime() {
    if [ "${KEEP_ARTIFACTS:-0}" = "1" ]; then
        echo "    kept artifacts: $RUN_ROOT"
    else
        rm -rf "$RUN_ROOT"
    fi
}
trap cleanup_runtime EXIT INT TERM

cat >"$INV_CFG" <<'CFGEOF'
set cg_simpleItems 0
set r_inGameVideo 0
set s_volume 0.5
set r_fastsky 0
set cl_allowDownload 0
set cg_crosshairAlpha 0
set char sarge
set g_skill 1
unbind DOWNARROW
wait 100
wui_push main
wait 30
wui_push preferences
wait 25
wui_menu_nav focus row_simple_items
set r_layoutDump 1
wait 2
set r_layoutDump 0
wui_menu_nav enter
cg_simpleItems
wui_menu_nav back
wait 20
wui_push controls
wait 25
wui_menu_nav focus row_forward
set r_layoutDump 1
wait 2
set r_layoutDump 0
wui_menu_nav enter
wui_menu_nav down
bind DOWNARROW
wui_menu_nav back
wait 20
wui_push display
wait 25
wui_menu_nav focus row_ingame_video
set r_layoutDump 1
wait 2
set r_layoutDump 0
wui_menu_nav enter
r_inGameVideo
wui_menu_nav back
wait 20
wui_push sound
wait 25
wui_menu_nav focus row_master
set r_layoutDump 1
wait 2
set r_layoutDump 0
wui_menu_nav enter
s_volume
wui_menu_nav back
wait 20
wui_push video
wait 25
wui_menu_nav focus row_fastsky
set r_layoutDump 1
wait 2
set r_layoutDump 0
wui_menu_nav enter
r_fastsky
wui_menu_nav back
wait 20
wui_push network
wait 25
wui_menu_nav focus row_autodl
set r_layoutDump 1
wait 2
set r_layoutDump 0
wui_menu_nav enter
cl_allowDownload
wui_menu_nav back
wait 20
wui_push playersettings
wait 30
wui_menu_nav focus row_crosshair
wui_menu_nav enter
cg_crosshairAlpha
wui_menu_nav focus charlist
set r_layoutDump 1
wait 2
set r_layoutDump 0
wui_menu_nav down
char
wui_menu_nav back
wait 20
wui_push servers
wait 30
wui_menu_nav focus filter_showfull
set r_layoutDump 1
wait 2
set r_layoutDump 0
wui_menu_nav enter
wui_menu_nav focus btn_specify
wui_menu_nav enter
wait 25
wui_menu_nav focus ip_entry
set r_layoutDump 1
wait 2
set r_layoutDump 0
wui_menu_nav focus btn_cancel
wui_menu_nav enter
wait 20
wui_menu_nav back
wait 20
wui_push startserver
wait 30
set g_skill 1
wui_menu_nav focus row_skill
set r_layoutDump 1
wait 2
set r_layoutDump 0
wui_menu_nav enter
wui_menu_nav down
wui_menu_nav enter
g_skill
wui_menu_nav focus btn_add_bots
wui_menu_nav enter
wait 25
set g_skill 1
wui_menu_nav focus row_skill
set r_layoutDump 1
wait 2
set r_layoutDump 0
wui_menu_nav enter
wui_menu_nav down
wui_menu_nav enter
g_skill
wui_menu_nav back
wait 15
wui_menu_nav back
wait 20
wui_push removebots
wait 25
wui_menu_nav focus botlist
set r_layoutDump 1
wait 2
set r_layoutDump 0
wui_menu_nav down
wui_menu_nav back
wait 20
wui_push demos
wait 25
wui_menu_nav focus demolist
set r_layoutDump 1
wait 2
set r_layoutDump 0
wui_menu_nav down
wui_menu_nav back
wait 20
wui_push mods
wait 25
wui_menu_nav focus modlist
set r_layoutDump 1
wait 2
set r_layoutDump 0
wui_menu_nav down
wui_menu_nav back
wait 20
wui_push serverinfo
wait 25
wui_menu_nav focus btn_back
set r_layoutDump 1
wait 2
set r_layoutDump 0
wui_menu_nav enter
wait 20
wui_menu_nav back
wait 20
wui_menu_nav back
wait 20
wui_push main
wait 25
wui_menu_nav focus menu_host
set r_layoutDump 1
wait 2
set r_layoutDump 0
wui_menu_nav back
wait 20
quit
CFGEOF

cat >"$GAME_CFG" <<'CFGEOF'
set g_maprotation ""
set activeAction "wait 60 ; quit"
wait 100
wui_push main
wait 30
wui_menu_nav focus menu_host
set r_layoutDump 1
wait 2
set r_layoutDump 0
wui_menu_nav enter
wait 40
wui_menu_nav focus maplist
set r_layoutDump 1
wait 2
set r_layoutDump 0
// Item storage starts with listSelectedRow 0.  Canonical pax01's filtered FFA
// roster begins arena1, arena13, arena17, so two real Down events must invoke
// the feeder callback twice and leave ui_selectedMap on arena17.
wui_menu_nav down
wait 10
wui_menu_nav down
wait 20
wui_menu_nav focus btn_start
set r_layoutDump 1
wait 2
set r_layoutDump 0
wui_menu_nav enter
CFGEOF

run_phase() {
    phase="$1"
    phase_parent="$2"
    phase_home="$3"
    phase_cfg="$4"
    phase_stdout="$5"
    phase_timeout="$6"

    echo "==> WiredUI menu-functional $phase phase"
    python3 "$TIMEOUT_RUNNER" \
        --timeout "$phase_timeout" \
        --kill-after 15 \
        --cwd "$phase_parent" \
        --stdout "$phase_stdout" \
        -- "$WIRED" \
        +set fs_basepath "$CONTENT_ROOT" \
        +set fs_homepath "$phase_home" \
        +set com_automated 1 \
        +set com_noHardReboot 1 \
        +set s_initsound 0 \
        +set r_fullscreen 0 \
        +set r_mode -1 +set r_customwidth 1280 +set r_customheight 720 \
        +set r_layoutDump 0 \
        +set log_severity DEBUG \
        +set log_file_severity DEBUG \
        +exec "$(basename "$phase_cfg")"
}

echo "    binary       : $WIRED"
echo "    current pack  : $PACK_ROOT/base/pax21.sw3z"
echo "    base content  : $CONTENT_ROOT/base (read-only)"
echo "    expected map  : $EXPECTED_MAP"

run_phase inventory "$INV_PARENT" "$INV_HOME_NATIVE" "$INV_CFG" "$INV_STDOUT" 180
inv_rc=$?
if [ "$inv_rc" -ne 0 ]; then
    echo "FAIL: inventory engine exited with status $inv_rc"
    [ -s "$INV_STDOUT" ] && { echo "  -- inventory stdout tail --"; tail -25 "$INV_STDOUT" | sed 's/^/  /'; }
    exit 1
fi

run_phase gameplay "$GAME_PARENT" "$GAME_HOME_NATIVE" "$GAME_CFG" "$GAME_STDOUT" 240
game_rc=$?
if [ "$game_rc" -ne 0 ]; then
    echo "FAIL: gameplay engine exited with status $game_rc"
    [ -s "$GAME_STDOUT" ] && { echo "  -- gameplay stdout tail --"; tail -25 "$GAME_STDOUT" | sed 's/^/  /'; }
    exit 1
fi

for artifact in "$INV_JSONL" "$INV_LAYOUT" "$GAME_JSONL" "$GAME_LAYOUT"; do
    if [ ! -s "$artifact" ]; then
        echo "FAIL: required artifact was not produced: $artifact"
        exit 1
    fi
done

if analyze_contract "$EXPECTED_MAP" "$INV_JSONL" "$INV_LAYOUT" "$GAME_JSONL" "$GAME_LAYOUT"; then
    echo "==> WiredUI menu-functional gate: PASS"
    exit 0
fi
echo "==> WiredUI menu-functional gate: FAIL"
exit 1
