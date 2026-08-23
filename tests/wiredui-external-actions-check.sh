#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Q0 external-data/action gate — MODS slice.
#
# An isolated home contributes two paired mod-list fixtures.  The product UI
# selects both through the normal focused-listbox keyboard path, activates the
# named Load Mod button for the second one, performs the full game-directory
# restart, and finally resolves a marker from the mounted mod archive with the
# product `which` command.
#
# Usage: tests/wiredui-external-actions-check.sh --self-test
#        WIRED_CONTENT_ROOT=/installed/q3now-preview \
#          tests/wiredui-external-actions-check.sh /assembled/wired
# Exit: 0 PASS, 1 FAIL, 77 unavailable binary/current pack/base content.

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
TIMEOUT_RUNNER="$SCRIPT_DIR/run-with-timeout.py"

analyze_contract() {
    python3 - "$1" <<'PYEOF'
import json
import re
import sys

path = sys.argv[1]
rows = []
try:
    with open(path, "r", encoding="utf-8", errors="replace") as stream:
        for line_no, line in enumerate(stream, 1):
            line = line.strip()
            if not line:
                continue
            try:
                row = json.loads(line)
            except ValueError as exc:
                print(f"FAIL: {path}:{line_no}: invalid JSON: {exc}")
                raise SystemExit(1)
            if not isinstance(row, dict):
                print(f"FAIL: {path}:{line_no}: JSON record is not an object")
                raise SystemExit(1)
            rows.append(row)
except OSError as exc:
    print(f"FAIL: cannot read {path}: {exc}")
    raise SystemExit(1)

bad = [row for row in rows
       if str(row.get("sev", "")).upper() in {"ERROR", "FATAL"}
       or (str(row.get("sev", "")).upper() == "WARN"
           and str(row.get("cat", "")).lower() == "ui")]
if bad:
    print(f"FAIL severity: {len(bad)} ERROR/FATAL or cat=ui WARN record(s)")
    for row in bad[:8]:
        print(f"  {row.get('sev', '?')}: {row.get('msg', '')}")
    raise SystemExit(1)

messages = [str(row.get("msg", "")) for row in rows]
steps = [
    ("isolated three-row mod feeder", r"WiredUI: feeders registered \(demos=[0-9]+, mods=3\)"),
    ("initial UI init", r"WiredUI: initialized \([1-9][0-9]* menus loaded\)"),
    ("mods menu", r"WiredUI: push menu 'mods' \(depth 1\)"),
    ("real list focus", r"wui_menu_nav focus: focused item 'modlist'"),
    ("pair sentinel selection",
     r"WiredUI: mod feeder selection row=1 dir=a0pairprobe description=A0 Pair Probe"),
    ("first real keyboard selection", r"wui_menu_nav: K_DOWNARROW dispatched"),
    ("paired q0mod selection",
     r"WiredUI: mod feeder selection row=2 dir=q0mod description=Q0 Fixture Mod"),
    ("second real keyboard selection", r"wui_menu_nav: K_DOWNARROW dispatched"),
    ("named load button focus", r"wui_menu_nav focus: focused item 'btn_load_mod'"),
    ("RunMod closed UI",
     r"WiredUI: close all postcondition depth=0 active=none catcher_ui=0 paused=0"),
    ("real Enter activation", r"wui_menu_nav: K_ENTER dispatched"),
    ("restart shutdown", r"WiredUI: shutdown"),
    ("file log resumed with session durability", r"log_sink_file: resumed append sync=1"),
    ("post-restart UI init", r"WiredUI: initialized \([1-9][0-9]* menus loaded\)"),
    ("selected fs_game", r'"fs_game" is:"q0mod(?:\^7)?"'),
    ("marker mounted from mod archive",
     r'File "q0mod_marker\.txt" found in ".*[/\\]q0mod[/\\]pak0\.pk3"'),
    ("clean post-restart menu lifecycle", r"WiredUI: push menu 'mods' \(depth 1\)"),
    ("clean final UI shutdown", r"WiredUI: shutdown"),
]

cursor = 0
for name, pattern in steps:
    for index in range(cursor, len(messages)):
        if re.search(pattern, messages[index]):
            cursor = index + 1
            break
    else:
        print(f"FAIL contract: missing/out-of-order step: {name}")
        raise SystemExit(1)

print("  PASS severity: zero ERROR/FATAL and zero cat=ui WARN records")
print(f"  PASS mods contract: {len(steps)} ordered product events")
PYEOF
}

if [ "${1:-}" = "--self-test" ]; then
    SELF_ROOT="$(mktemp -d -t wired-q0mods-self-XXXXXX 2>/dev/null || mktemp -d)"
    trap 'rm -rf "$SELF_ROOT"' EXIT INT TERM
    CLEAN="$SELF_ROOT/clean.jsonl"
    python3 - "$CLEAN" <<'PYEOF'
import json
import sys

messages = [
    ("DEBUG", "ui", "WiredUI: feeders registered (demos=0, mods=3)"),
    ("INFO", "ui", "WiredUI: initialized (38 menus loaded)"),
    ("DEBUG", "ui", "WiredUI: push menu 'mods' (depth 1)"),
    ("DEBUG", "ui", "wui_menu_nav focus: focused item 'modlist' (top index 2)"),
    ("DEBUG", "ui", "WiredUI: mod feeder selection row=1 dir=a0pairprobe description=A0 Pair Probe"),
    ("DEBUG", "ui", "wui_menu_nav: K_DOWNARROW dispatched"),
    ("DEBUG", "ui", "WiredUI: mod feeder selection row=2 dir=q0mod description=Q0 Fixture Mod"),
    ("DEBUG", "ui", "wui_menu_nav: K_DOWNARROW dispatched"),
    ("DEBUG", "ui", "wui_menu_nav focus: focused item 'btn_load_mod' (top index -1)"),
    ("DEBUG", "ui", "WiredUI: close all postcondition depth=0 active=none catcher_ui=0 paused=0"),
    ("DEBUG", "ui", "wui_menu_nav: K_ENTER dispatched"),
    ("INFO", "ui", "WiredUI: shutdown"),
    ("INFO", "system", "log_sink_file: resumed append sync=1"),
    ("INFO", "ui", "WiredUI: initialized (38 menus loaded)"),
    ("INFO", "console", '"fs_game" is:"q0mod^7" default:""'),
    ("INFO", "filesystem", 'File "q0mod_marker.txt" found in "/tmp/home/q0mod/pak0.pk3"'),
    ("DEBUG", "ui", "WiredUI: push menu 'mods' (depth 1)"),
    ("INFO", "ui", "WiredUI: shutdown"),
]
with open(sys.argv[1], "w", encoding="utf-8") as stream:
    for sev, cat, msg in messages:
        stream.write(json.dumps({"sev": sev, "cat": cat, "msg": msg}) + "\n")
PYEOF
    if ! analyze_contract "$CLEAN" >/dev/null; then
        echo "FAIL self-test: clean fixture rejected"
        exit 1
    fi

    for defect in wrong_pair wrong_selection no_close wrong_sync wrong_fs_game no_restart no_marker severity malformed_json non_object; do
        BAD="$SELF_ROOT/$defect.jsonl"
        python3 - "$CLEAN" "$BAD" "$defect" <<'PYEOF'
import json
import sys

src, dst, defect = sys.argv[1:4]
rows = [json.loads(line) for line in open(src, encoding="utf-8")]
if defect == "wrong_pair":
    # Model the prior one-NUL advance exactly: the first description is
    # misinterpreted as the next directory, so q0mod disappears from row 2.
    rows[6]["msg"] = "WiredUI: mod feeder selection row=2 dir=A0 Pair Probe description=A0 Pair Probe"
elif defect == "wrong_selection":
    rows[6]["msg"] = "WiredUI: mod feeder selection row=0 dir=base description=Wired (base game)"
elif defect == "no_close":
    rows = [row for i, row in enumerate(rows) if i != 9]
elif defect == "wrong_sync":
    rows[12]["msg"] = "log_sink_file: resumed append sync=0"
elif defect == "wrong_fs_game":
    rows[14]["msg"] = '"fs_game" is:"base^7" default:""'
elif defect == "no_restart":
    rows = [row for i, row in enumerate(rows) if i not in (11, 12, 13)]
elif defect == "no_marker":
    rows = [row for i, row in enumerate(rows) if i != 15]
elif defect == "severity":
    rows.insert(6, {"sev": "WARN", "cat": "ui", "msg": "synthetic UI warning"})
with open(dst, "w", encoding="utf-8") as stream:
    for row in rows:
        stream.write(json.dumps(row) + "\n")
    if defect == "malformed_json":
        stream.write("{not-json\n")
    elif defect == "non_object":
        stream.write(json.dumps(["not", "an", "object"]) + "\n")
PYEOF
        if analyze_contract "$BAD" >/dev/null 2>&1; then
            echo "FAIL self-test: defect '$defect' was accepted"
            exit 1
        fi
        echo "  PASS self-test: rejects $defect"
    done
    echo "==> WiredUI external-actions MODS analyzer self-test: PASS"
    exit 0
fi

if [ ! -f "$TIMEOUT_RUNNER" ]; then
    echo "SKIP: timeout runner unavailable: $TIMEOUT_RUNNER"
    exit 77
fi
if ! command -v python3 >/dev/null 2>&1; then
    echo "SKIP: Python 3 is required for the watchdog and strict analyzer"
    exit 77
fi
if ! python3 -c 'import sys; raise SystemExit(0 if sys.version_info >= (3, 8) else 1)' >/dev/null 2>&1; then
    echo "SKIP: Python 3.8 or newer is required"
    exit 77
fi

WIRED="${1:-}"
if [ -z "$WIRED" ]; then
    for candidate in \
        "$SCRIPT_DIR/../build/q3now-preview.arm64.app/Contents/MacOS/wired.arm64" \
        "$SCRIPT_DIR/../build/q3now-preview.x86_64.app/Contents/MacOS/wired.x86_64" \
        "$SCRIPT_DIR/../build/wired"; do
        if [ -x "$candidate" ]; then WIRED="$candidate"; break; fi
    done
fi
if [ -z "$WIRED" ] || [ ! -x "$WIRED" ]; then
    echo "SKIP: Wired engine binary unavailable (pass its path as argv[1])"
    exit 77
fi
WIRED="$(cd "$(dirname "$WIRED")" && pwd)/$(basename "$WIRED")"
WIRED_DIR="$(dirname "$WIRED")"

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
    echo "SKIP: licensed base content missing; set WIRED_CONTENT_ROOT"
    exit 77
fi

RUN_ROOT="$(mktemp -d -t wired-q0mods-XXXXXX 2>/dev/null || mktemp -d)"
HOME_PARENT="$RUN_ROOT/home"
HOME_DIR="$HOME_PARENT/q3now-preview"
mkdir -p "$HOME_DIR/base" "$HOME_DIR/a0pairprobe" "$HOME_DIR/q0mod"

cleanup_runtime() {
    if [ "${WIRED_KEEP_ARTIFACTS:-0}" = "1" ]; then
        echo "    kept artifacts: $RUN_ROOT"
    else
        rm -rf "$RUN_ROOT"
    fi
}
trap cleanup_runtime EXIT INT TERM

# Stage exactly one canonical licensed base archive and then the exact current
# module/UI pack.  Broad globs can silently contaminate the fixture with stale
# or unrelated product packs, so pax01 wins and pak0 is only its legacy fallback.
if [ -f "$CONTENT_ROOT/base/pax01.sw3z" ]; then
    BASE_ARCHIVE="$CONTENT_ROOT/base/pax01.sw3z"
elif [ -f "$CONTENT_ROOT/base/pak0.pk3" ]; then
    BASE_ARCHIVE="$CONTENT_ROOT/base/pak0.pk3"
else
    echo "FAIL: canonical pax01.sw3z or pak0.pk3 missing from $CONTENT_ROOT/base"
    exit 1
fi
cp "$BASE_ARCHIVE" "$HOME_DIR/base/" || {
    echo "FAIL: could not stage $BASE_ARCHIVE"
    exit 1
}
cp "$PACK_ROOT/base/pax21.sw3z" "$HOME_DIR/base/pax21.sw3z" || {
    echo "FAIL: could not stage current pax21.sw3z"
    exit 1
}

printf '%s\n' 'A0 Pair Probe' >"$HOME_DIR/a0pairprobe/description.txt"
printf '%s\n' 'Q0 Fixture Mod' >"$HOME_DIR/q0mod/description.txt"
python3 - "$HOME_DIR/a0pairprobe/pak0.pk3" "$HOME_DIR/q0mod/pak0.pk3" <<'PYEOF'
import sys
import zipfile

archives = {
    sys.argv[1]: {"a0pairprobe.txt": "PAIR_SENTINEL_V1\n"},
    sys.argv[2]: {
        "q0mod_marker.txt": "WIRED_Q0MOD_MARKER_V1\n",
        "q0mod_post.cfg": """wait 80
set log_severity DEBUG
set log_file_severity DEBUG
fs_game
which q0mod_marker.txt
wui_push mods
wait 20
quit
""",
    },
}
for archive_path, members in archives.items():
    with zipfile.ZipFile(archive_path, "w", compression=zipfile.ZIP_DEFLATED) as archive:
        for name in sorted(members):
            info = zipfile.ZipInfo(name, (1980, 1, 1, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o644 << 16
            archive.writestr(info, members[name])
PYEOF

# Missing on the first boot, found from q0mod/pak0.pk3 after the real filesystem
# restart.  `execq` makes the expected first miss informational and quiet.
printf '%s\n' 'execq q0mod_post.cfg' >"$HOME_DIR/base/autoexec.cfg"
cat >"$HOME_DIR/base/q0mod-select.cfg" <<'CFGEOF'
wait 100
wui_push mods
wait 30
wui_menu_nav focus modlist
wait 2
wui_menu_nav down
wait 5
wui_menu_nav down
wait 10
wui_menu_nav focus btn_load_mod
wait 2
wui_menu_nav enter
CFGEOF

case "$(uname -s)" in
    Darwin)
        HOME_NATIVE="$HOME_DIR"
        # A prior intentionally-failed GUI fixture can make AppKit display a
        # saved-state crash-recovery modal before SDL finishes renderer init.
        # Ignore persistence for this process only; never mutate user defaults
        # or delete the application's saved-state directory.
        PLATFORM_ARGS=( -ApplePersistenceIgnoreState YES )
        ;;
    MINGW*|MSYS*|CYGWIN*)
        HOME_NATIVE="$(cygpath -w "$HOME_DIR")"
        PLATFORM_ARGS=()
        ;;
    *)
        HOME_NATIVE="$HOME_DIR"
        PLATFORM_ARGS=()
        ;;
esac

JSONL="$HOME_DIR/qconsole.jsonl"
STDOUT="$RUN_ROOT/wired.stdout"
echo "==> WiredUI external-actions MODS product run"
echo "    binary      : $WIRED"
echo "    base archive : $BASE_ARCHIVE"
echo "    current pack : $PACK_ROOT/base/pax21.sw3z"
echo "    isolated home: $HOME_DIR"
python3 "$TIMEOUT_RUNNER" \
    --timeout 180 \
    --kill-after 15 \
    --cwd "$HOME_PARENT" \
    --stdout "$STDOUT" \
    -- "$WIRED" \
    "${PLATFORM_ARGS[@]}" \
    +set fs_homepath "$HOME_NATIVE" \
    +set com_automated 1 \
    +set com_noHardReboot 1 \
    +set s_initsound 0 \
    +set r_fullscreen 0 \
    +set r_mode -1 +set r_customwidth 1280 +set r_customheight 720 \
    +set log_severity DEBUG \
    +set log_file_severity DEBUG \
    +set log_file_mode overwrite_synced \
    +exec q0mod-select.cfg
run_rc=$?
if [ "$run_rc" -ne 0 ]; then
    echo "FAIL: engine exited with status $run_rc"
    [ -s "$STDOUT" ] && { echo "  -- stdout tail --"; tail -30 "$STDOUT" | sed 's/^/  /'; }
    exit 1
fi
if [ ! -s "$JSONL" ]; then
    echo "FAIL: product JSONL log not produced: $JSONL"
    exit 1
fi

if analyze_contract "$JSONL"; then
    echo "==> WiredUI external-actions MODS gate: PASS"
else
    echo "FAIL: MODS product contract rejected"
    [ -s "$STDOUT" ] && { echo "  -- stdout tail --"; tail -30 "$STDOUT" | sed 's/^/  /'; }
    exit 1
fi
