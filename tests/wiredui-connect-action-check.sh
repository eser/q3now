#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Q0 WiredUI external action gate — Specify Server connect slice.
#
# The rejection phase types an injection-shaped address through the real
# editfield character path and proves that validation leaves the menu open and
# the sentinel untouched.  The acceptance phase types a loopback endpoint,
# activates the authored Connect button, and must reach arena7 on a separately
# launched wired-headless server.
#
# Usage: bash tests/wiredui-connect-action-check.sh --self-test
#        WIRED_CONTENT_ROOT=/installed/q3now-preview \
#          bash tests/wiredui-connect-action-check.sh /assembled/wired
# Exit: 0 PASS, 1 FAIL, 77 unavailable binary/headless/content/tooling.

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
TIMEOUT_RUNNER="$SCRIPT_DIR/run-with-timeout.py"

analyze_contract() {
    python3 - "$1" "$2" "$3" "$4" <<'PYEOF'
import json
import re
import sys

invalid_path, valid_path, server_path, endpoint = sys.argv[1:5]

def fnv1a32(text):
    value = 2166136261
    for byte in text.encode("utf-8"):
        value ^= byte
        value = (value * 16777619) & 0xffffffff
    return value

def read_rows(path):
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
                    raise SystemExit(f"FAIL: {path}:{line_no}: invalid JSON: {exc}")
                if not isinstance(row, dict):
                    raise SystemExit(f"FAIL: {path}:{line_no}: JSON record is not an object")
                for field in ("sev", "cat", "msg"):
                    if not isinstance(row.get(field), str):
                        raise SystemExit(f"FAIL: {path}:{line_no}: {field} is not a string")
                rows.append(row)
    except OSError as exc:
        raise SystemExit(f"FAIL: cannot read {path}: {exc}")
    if not rows:
        raise SystemExit(f"FAIL: empty JSONL evidence: {path}")
    return rows

invalid = read_rows(invalid_path)
valid = read_rows(valid_path)
server = read_rows(server_path)

expected_reject = "WiredUI: rejected invalid server address origin=specify"
for phase, rows in (("invalid", invalid), ("valid", valid), ("server", server)):
    bad = []
    for row in rows:
        sev = row["sev"].upper()
        cat = row["cat"].lower()
        msg = row["msg"].strip()
        if phase == "invalid" and sev == "WARN" and cat == "ui" and msg == expected_reject:
            continue
        if sev in {"ERROR", "FATAL"} or (sev == "WARN" and cat == "ui"):
            bad.append(row)
    if bad:
        print(f"FAIL severity ({phase}): {len(bad)} unexpected record(s)")
        for row in bad[:8]:
            print(f"  {row['sev']}/{row['cat']}: {row['msg']}")
        raise SystemExit(1)

def ordered(messages, steps, phase):
    cursor = 0
    for name, pattern in steps:
        for index in range(cursor, len(messages)):
            if re.search(pattern, messages[index]):
                cursor = index + 1
                break
        else:
            raise SystemExit(f"FAIL contract ({phase}): missing/out-of-order step: {name}")

invalid_messages = [row["msg"] for row in invalid]
valid_messages = [row["msg"] for row in valid]
server_messages = [row["msg"] for row in server]
escaped = re.escape(endpoint)
injection = endpoint + ";set q0_injected 1"
fingerprint = rf"WiredStore fingerprint: key=ui_specifyAddress length={len(injection)} fnv1a32={fnv1a32(injection):08x}"

ordered(invalid_messages, [
    ("specify menu", r"WiredUI: push menu 'specifyserver' \(depth 1\)"),
    ("editfield focus", r"wui_menu_nav focus: focused item 'ip_entry'"),
    ("edit mode", r"wui_menu_nav: K_ENTER dispatched"),
    ("typed injection-shaped input", r"wui_menu_nav: typed [1-9][0-9]* printable character\(s\)"),
    ("exact Store fingerprint", fingerprint),
    ("edit commit", r"wui_menu_nav: K_ENTER dispatched"),
    ("connect focus", r"wui_menu_nav focus: focused item 'btn_connect'"),
    ("validation reject", r"WiredUI: rejected invalid server address origin=specify"),
    ("connect Enter returns", r"wui_menu_nav: K_ENTER dispatched"),
    ("menu retained", r"wui_menu_nav focus: focused item 'btn_cancel'"),
    ("sentinel remains zero", r'"q0_injected" is:"0(?:\^7)?"'),
], "invalid")

if sum(row["msg"].strip() == expected_reject for row in invalid) != 1:
    raise SystemExit("FAIL invalid: expected exactly one validation rejection")
if any("queued validated connect" in msg for msg in invalid_messages):
    raise SystemExit("FAIL invalid: rejected input still queued a connect")
if any("usage: connect" in msg for msg in invalid_messages):
    raise SystemExit("FAIL invalid: Specify action fell through to bare connect usage")
if any(re.search(r'"q0_injected" is:"1', msg) for msg in invalid_messages):
    raise SystemExit("FAIL invalid: command-injection sentinel changed")

ordered(valid_messages, [
    ("specify menu", r"WiredUI: push menu 'specifyserver' \(depth 1\)"),
    ("editfield focus", r"wui_menu_nav focus: focused item 'ip_entry'"),
    ("edit mode", r"wui_menu_nav: K_ENTER dispatched"),
    ("typed endpoint", r"wui_menu_nav: typed [1-9][0-9]* printable character\(s\)"),
    ("Store key", r"  key:\s+ui_specifyAddress"),
    ("Store endpoint", rf'  text:\s+"{escaped}"'),
    ("edit commit", r"wui_menu_nav: K_ENTER dispatched"),
    ("connect focus", r"wui_menu_nav focus: focused item 'btn_connect'"),
    ("validated queue", rf"WiredUI: queued validated connect origin=specify address={escaped}"),
    ("close all", r"WiredUI: close all postcondition depth=0 active=none catcher_ui=0 paused=0"),
    ("connect Enter returns", r"wui_menu_nav: K_ENTER dispatched"),
    ("engine resolution", rf"{escaped} resolved to {escaped}"),
    ("QUIC accept", r"QUIC client: TLV ACCEPT received"),
    ("first gameplay", r"FIRST GAMEPLAY FRAME mapname=maps/arena7\.bsp\b"),
    ("clean shutdown", r"WiredUI: shutdown"),
], "valid")

if any("usage: connect" in msg for msg in valid_messages):
    raise SystemExit("FAIL valid: Specify action emitted bare connect usage")
if sum("queued validated connect origin=specify" in msg for msg in valid_messages) != 1:
    raise SystemExit("FAIL valid: expected exactly one validated connect queue")

ordered(server_messages, [
    ("loopback listener", rf"WiredNet: listening on port {re.escape(endpoint.rsplit(':', 1)[1])} \(IPv4\)"),
    ("arena7 server", r"Server: arena7\b"),
    ("remote accept", r"SV_OnPlayerConnect: conn="),
    ("console-quit shutdown", r"----- Server Shutdown \(Server quit\) -----"),
    ("transport shutdown", r"QUIC transport shut down\."),
    ("nav shutdown", r"\[NAV\] Nav_Shutdown"),
    ("final core shutdown", r"WiredCore/Scripting: shutdown"),
], "server")
if any("Signal caught" in msg for msg in server_messages):
    raise SystemExit("FAIL server: signal-based teardown/crash path observed")

print("  PASS invalid: unsafe editfield input rejected; menu retained; sentinel=0")
print("  PASS valid: editfield Store -> validated queue -> QUIC ACCEPT -> arena7 FIRST")
print("  PASS server: loopback listener -> arena7 -> remote client accept")
PYEOF
}

if [ "${1:-}" = "--self-test" ]; then
    SELF_ROOT="$(mktemp -d -t wired-q0connect-self-XXXXXX 2>/dev/null || mktemp -d)"
    trap 'rm -rf "$SELF_ROOT"' EXIT INT TERM
    ENDPOINT="127.0.0.1:28765"
    INVALID="$SELF_ROOT/invalid.jsonl"
    VALID="$SELF_ROOT/valid.jsonl"
    SERVER="$SELF_ROOT/server.jsonl"
    python3 - "$INVALID" "$VALID" "$SERVER" "$ENDPOINT" <<'PYEOF'
import json
import sys

invalid_path, valid_path, server_path, endpoint = sys.argv[1:5]

def write(path, rows):
    with open(path, "w", encoding="utf-8") as stream:
        for sev, cat, msg in rows:
            stream.write(json.dumps({"sev": sev, "cat": cat, "msg": msg}) + "\n")

common = [("INFO", "ui", "WiredUI: initialized (38 menus loaded)")]
write(invalid_path, common + [
    ("DEBUG", "ui", "WiredUI: push menu 'specifyserver' (depth 1)"),
    ("DEBUG", "ui", "wui_menu_nav focus: focused item 'ip_entry' (top index 2)"),
    ("DEBUG", "ui", "wui_menu_nav: K_ENTER dispatched"),
    ("DEBUG", "ui", "wui_menu_nav: typed 35 printable character(s)"),
    ("INFO", "client", "WiredStore fingerprint: key=ui_specifyAddress length=33 fnv1a32=f4778cec"),
    ("DEBUG", "ui", "wui_menu_nav: K_ENTER dispatched"),
    ("DEBUG", "ui", "wui_menu_nav focus: focused item 'btn_connect' (top index -1)"),
    ("WARN", "ui", "WiredUI: rejected invalid server address origin=specify"),
    ("DEBUG", "ui", "wui_menu_nav: K_ENTER dispatched"),
    ("DEBUG", "ui", "wui_menu_nav focus: focused item 'btn_cancel' (top index -1)"),
    ("INFO", "console", '"q0_injected" is:"0^7" default:"0"'),
    ("INFO", "ui", "WiredUI: shutdown"),
])
write(valid_path, common + [
    ("DEBUG", "ui", "WiredUI: push menu 'specifyserver' (depth 1)"),
    ("DEBUG", "ui", "wui_menu_nav focus: focused item 'ip_entry' (top index 2)"),
    ("DEBUG", "ui", "wui_menu_nav: K_ENTER dispatched"),
    ("DEBUG", "ui", "wui_menu_nav: typed 15 printable character(s)"),
    ("INFO", "client", "  key:   ui_specifyAddress"),
    ("INFO", "client", f'  text:  "{endpoint}"'),
    ("DEBUG", "ui", "wui_menu_nav: K_ENTER dispatched"),
    ("DEBUG", "ui", "wui_menu_nav focus: focused item 'btn_connect' (top index -1)"),
    ("DEBUG", "ui", f"WiredUI: queued validated connect origin=specify address={endpoint}"),
    ("DEBUG", "ui", "WiredUI: close all postcondition depth=0 active=none catcher_ui=0 paused=0"),
    ("DEBUG", "ui", "wui_menu_nav: K_ENTER dispatched"),
    ("INFO", "client", f"{endpoint} resolved to {endpoint}"),
    ("INFO", "network", "QUIC client: TLV ACCEPT received slot=0 sv_fps=20"),
    ("INFO", "client", "cls.state: -> CA_ACTIVE (FIRST GAMEPLAY FRAME mapname=maps/arena7.bsp serverTime=100 framecount=1)"),
    ("INFO", "ui", "WiredUI: shutdown"),
])
port = endpoint.rsplit(":", 1)[1]
write(server_path, [
    ("INFO", "network", f"WiredNet: listening on port {port} (IPv4), ALPN: q3now"),
    ("INFO", "server", "Server: arena7"),
    ("DEBUG", "server", "SV_OnPlayerConnect: conn=17"),
    ("INFO", "server", "----- Server Shutdown (Server quit) -----"),
    ("INFO", "network", "QUIC transport shut down."),
    ("DEBUG", "nav", "[NAV] Nav_Shutdown"),
    ("INFO", "scripting", "WiredCore/Scripting: shutdown"),
])
PYEOF
    if ! analyze_contract "$INVALID" "$VALID" "$SERVER" "$ENDPOINT" >/dev/null; then
        echo "FAIL self-test: clean fixture rejected"
        exit 1
    fi

    for defect in no_type bad_invalid_store unsafe_queued injected menu_closed wrong_endpoint no_close no_accept no_first wrong_map no_server_accept no_server_shutdown severity malformed_json non_object; do
        BAD_ROOT="$SELF_ROOT/$defect"
        mkdir -p "$BAD_ROOT"
        python3 - "$INVALID" "$VALID" "$SERVER" "$BAD_ROOT" "$defect" <<'PYEOF'
import json
import pathlib
import sys

invalid_path, valid_path, server_path, out_root, defect = sys.argv[1:6]
out_root = pathlib.Path(out_root)
rows = {}
for key, path in (("invalid", invalid_path), ("valid", valid_path), ("server", server_path)):
    rows[key] = [json.loads(line) for line in open(path, encoding="utf-8")]

if defect == "no_type":
    rows["valid"] = [r for r in rows["valid"] if "typed " not in r["msg"]]
elif defect == "bad_invalid_store":
    for r in rows["invalid"]:
        if "WiredStore fingerprint:" in r["msg"]:
            r["msg"] = "WiredStore fingerprint: key=ui_specifyAddress length=0 fnv1a32=811c9dc5"
elif defect == "unsafe_queued":
    rows["invalid"].insert(8, {"sev":"DEBUG","cat":"ui","msg":"WiredUI: queued validated connect origin=specify address=127.0.0.1:28765"})
elif defect == "injected":
    rows["invalid"][-2]["msg"] = '"q0_injected" is:"1^7" default:"0"'
elif defect == "menu_closed":
    rows["invalid"] = [r for r in rows["invalid"] if "focused item 'btn_cancel'" not in r["msg"]]
elif defect == "wrong_endpoint":
    for r in rows["valid"]:
        if "queued validated connect" in r["msg"]:
            r["msg"] = "WiredUI: queued validated connect origin=specify address=127.0.0.1:1"
elif defect == "no_close":
    rows["valid"] = [r for r in rows["valid"] if "close all postcondition" not in r["msg"]]
elif defect == "no_accept":
    rows["valid"] = [r for r in rows["valid"] if "TLV ACCEPT" not in r["msg"]]
elif defect == "no_first":
    rows["valid"] = [r for r in rows["valid"] if "FIRST GAMEPLAY FRAME" not in r["msg"]]
elif defect == "wrong_map":
    for r in rows["valid"]:
        if "FIRST GAMEPLAY FRAME" in r["msg"]:
            r["msg"] = "cls.state: -> CA_ACTIVE (FIRST GAMEPLAY FRAME mapname=maps/arena1.bsp serverTime=100 framecount=1)"
elif defect == "no_server_accept":
    rows["server"] = [r for r in rows["server"] if "SV_OnPlayerConnect" not in r["msg"]]
elif defect == "no_server_shutdown":
    rows["server"] = [r for r in rows["server"] if "Server Shutdown" not in r["msg"]]
elif defect == "severity":
    rows["valid"].insert(5, {"sev":"ERROR","cat":"renderer","msg":"synthetic failure"})

for key in rows:
    with open(out_root / f"{key}.jsonl", "w", encoding="utf-8") as stream:
        for row in rows[key]:
            stream.write(json.dumps(row) + "\n")
        if defect == "malformed_json" and key == "valid":
            stream.write("{not-json\n")
        if defect == "non_object" and key == "server":
            stream.write(json.dumps(["not", "object"]) + "\n")
PYEOF
        if analyze_contract "$BAD_ROOT/invalid.jsonl" "$BAD_ROOT/valid.jsonl" "$BAD_ROOT/server.jsonl" "$ENDPOINT" >/dev/null 2>&1; then
            echo "FAIL self-test: defect '$defect' was accepted"
            exit 1
        fi
        echo "  PASS self-test: rejects $defect"
    done
    echo "==> WiredUI Specify connect analyzer self-test: PASS"
    exit 0
fi

if ! command -v python3 >/dev/null 2>&1 || ! python3 -c 'import sys; raise SystemExit(sys.version_info < (3,8))'; then
    echo "SKIP: Python >=3.8 is required"
    exit 77
fi
if [ ! -f "$TIMEOUT_RUNNER" ]; then
    echo "SKIP: missing portable watchdog: $TIMEOUT_RUNNER"
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
    echo "SKIP: Wired GUI binary unavailable"
    exit 77
fi
WIRED="$(cd "$(dirname "$WIRED")" && pwd)/$(basename "$WIRED")"
WIRED_DIR="$(dirname "$WIRED")"

HEADLESS="${WIRED_BINARY_HEADLESS:-}"
if [ -z "$HEADLESS" ]; then
    gui_name="$(basename "$WIRED")"
    suffix="${gui_name#wired}"
    for candidate in \
        "$WIRED_DIR/wired-headless$suffix" \
        "$WIRED_DIR/wired-headless.arm64" \
        "$WIRED_DIR/wired-headless.aarch64" \
        "$WIRED_DIR/wired-headless.x86_64" \
        "$WIRED_DIR/wired-headless.x64.exe" \
        "$WIRED_DIR/../../../wired-headless$suffix" \
        "$WIRED_DIR/../../../wired-headless.arm64" \
        "$WIRED_DIR/../../../wired-headless.x86_64"; do
        if [ -x "$candidate" ]; then HEADLESS="$candidate"; break; fi
    done
fi
if [ -z "$HEADLESS" ] || [ ! -x "$HEADLESS" ]; then
    echo "SKIP: sibling wired-headless binary unavailable (or set WIRED_BINARY_HEADLESS)"
    exit 77
fi
HEADLESS="$(cd "$(dirname "$HEADLESS")" && pwd)/$(basename "$HEADLESS")"

PACK_ROOT=""
for candidate in "$WIRED_DIR" "$WIRED_DIR/../Resources" "$WIRED_DIR/../../.."; do
    if [ -f "$candidate/base/pax21.sw3z" ]; then
        PACK_ROOT="$(cd "$candidate" && pwd)"
        break
    fi
done
if [ -z "$PACK_ROOT" ]; then
    echo "SKIP: current base/pax21.sw3z unavailable"
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
if [ -f "$CONTENT_ROOT/base/pax01.sw3z" ]; then
    BASE_ARCHIVE="$CONTENT_ROOT/base/pax01.sw3z"
else
    BASE_ARCHIVE="$CONTENT_ROOT/base/pak0.pk3"
fi

RUN_ROOT="$(mktemp -d -t wired-q0connect-XXXXXX 2>/dev/null || mktemp -d)"
INVALID_HOME="$RUN_ROOT/invalid/q3now-preview"
VALID_HOME="$RUN_ROOT/valid/q3now-preview"
SERVER_HOME="$RUN_ROOT/server/q3now-preview"
SERVER_PID=""
SERVER_CONTROL="$RUN_ROOT/server.stdin"
SERVER_CONTROL_OPEN=0
SERVER_EXIT_RC=""
SERVER_FORCED=0

stop_server() {
    if [ -n "$SERVER_PID" ] && kill -0 "$SERVER_PID" 2>/dev/null; then
        if [ "$SERVER_CONTROL_OPEN" -eq 1 ]; then
            printf '%s\n' quit >&9 2>/dev/null || true
        fi
        # Nav teardown can be joining an in-flight bake after the console quit
        # has already begun a clean Server Shutdown.  Give that owned shutdown
        # the same 15 s grace as the process-tree watchdog before escalation.
        for _ in $(seq 1 150); do
            kill -0 "$SERVER_PID" 2>/dev/null || break
            sleep 0.1
        done
        if kill -0 "$SERVER_PID" 2>/dev/null; then
            SERVER_FORCED=1
            kill -TERM "$SERVER_PID" 2>/dev/null || true
            sleep 0.25
        fi
        if kill -0 "$SERVER_PID" 2>/dev/null; then
            SERVER_FORCED=1
            kill -KILL "$SERVER_PID" 2>/dev/null || true
        fi
        wait "$SERVER_PID" 2>/dev/null
        SERVER_EXIT_RC=$?
    fi
    if [ "$SERVER_CONTROL_OPEN" -eq 1 ]; then
        exec 9>&-
        SERVER_CONTROL_OPEN=0
    fi
    SERVER_PID=""
}

cleanup_runtime() {
    stop_server
    if [ "${WIRED_KEEP_ARTIFACTS:-0}" = "1" ]; then
        echo "    kept artifacts: $RUN_ROOT"
    else
        rm -rf "$RUN_ROOT"
    fi
}
trap cleanup_runtime EXIT INT TERM

stage_home() {
    local target="$1"
    mkdir -p "$target/base"
    cp "$BASE_ARCHIVE" "$target/base/" || return 1
    cp "$PACK_ROOT/base/pax21.sw3z" "$target/base/pax21.sw3z" || return 1
}
stage_home "$INVALID_HOME" || { echo "FAIL: invalid-phase staging failed"; exit 1; }
stage_home "$VALID_HOME" || { echo "FAIL: valid-phase staging failed"; exit 1; }
stage_home "$SERVER_HOME" || { echo "FAIL: server staging failed"; exit 1; }

PORT="$(python3 - <<'PYEOF'
import socket
s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
s.bind(("127.0.0.1", 0))
print(s.getsockname()[1])
s.close()
PYEOF
)"
case "$PORT" in ''|*[!0-9]*) echo "FAIL: could not allocate loopback UDP port"; exit 1;; esac
ENDPOINT="127.0.0.1:$PORT"
INJECTION="$ENDPOINT;set q0_injected 1"

cat >"$INVALID_HOME/base/q0connect-invalid.cfg" <<CFGEOF
set q0_injected 0
wait 100
wui_push specifyserver
wait 25
wui_menu_nav focus ip_entry
wui_menu_nav enter
wui_menu_nav type "$INJECTION"
wui_store_fingerprint ui_specifyAddress
wui_menu_nav enter
wui_menu_nav focus btn_connect
wui_menu_nav enter
wait 20
wui_menu_nav focus btn_cancel
q0_injected
wait 5
quit
CFGEOF

cat >"$VALID_HOME/base/q0connect-valid.cfg" <<CFGEOF
set activeAction "wait 60 ; quit"
wait 100
wui_push specifyserver
wait 25
wui_menu_nav focus ip_entry
wui_menu_nav enter
wui_menu_nav type "$ENDPOINT"
wui_store_get ui_specifyAddress
wui_menu_nav enter
wui_menu_nav focus btn_connect
wui_menu_nav enter
CFGEOF

INVALID_JSON="$INVALID_HOME/qconsole.jsonl"
VALID_JSON="$VALID_HOME/qconsole.jsonl"
SERVER_JSON="$SERVER_HOME/qconsole.jsonl"
INVALID_STDOUT="$RUN_ROOT/invalid.stdout"
VALID_STDOUT="$RUN_ROOT/valid.stdout"
SERVER_STDOUT="$RUN_ROOT/server.stdout"

case "$(uname -s)" in
    Darwin)
        INVALID_NATIVE="$INVALID_HOME"
        VALID_NATIVE="$VALID_HOME"
        SERVER_NATIVE="$SERVER_HOME"
        CLIENT_PLATFORM_ARGS=( -ApplePersistenceIgnoreState YES )
        ;;
    MINGW*|MSYS*|CYGWIN*)
        INVALID_NATIVE="$(cygpath -w "$INVALID_HOME")"
        VALID_NATIVE="$(cygpath -w "$VALID_HOME")"
        SERVER_NATIVE="$(cygpath -w "$SERVER_HOME")"
        CLIENT_PLATFORM_ARGS=()
        ;;
    *)
        INVALID_NATIVE="$INVALID_HOME"
        VALID_NATIVE="$VALID_HOME"
        SERVER_NATIVE="$SERVER_HOME"
        CLIENT_PLATFORM_ARGS=()
        ;;
esac

run_client() {
    local native_home="$1"
    local stdout_path="$2"
    local cfg="$3"
    shift 3
    python3 "$TIMEOUT_RUNNER" \
        --timeout 150 --kill-after 15 \
        --cwd "$WIRED_DIR" --stdout "$stdout_path" \
        -- "$WIRED" "${CLIENT_PLATFORM_ARGS[@]}" \
        +set fs_homepath "$native_home" \
        +set com_automated 1 +set com_noHardReboot 1 \
        +set s_initsound 0 +set r_fullscreen 0 \
        +set r_mode -1 +set r_customwidth 1280 +set r_customheight 720 \
        +set log_severity DEBUG +set log_file_severity DEBUG \
        +set log_file_mode overwrite_synced "$@" +exec "$cfg"
}

echo "==> Specify rejection phase (real editfield input; no network)"
run_client "$INVALID_NATIVE" "$INVALID_STDOUT" q0connect-invalid.cfg +set net_enabled 0
invalid_rc=$?
if [ "$invalid_rc" -ne 0 ] || [ ! -s "$INVALID_JSON" ]; then
    echo "FAIL: rejection phase rc=$invalid_rc or missing JSONL"
    [ -s "$INVALID_STDOUT" ] && tail -30 "$INVALID_STDOUT"
    exit 1
fi

echo "==> Starting isolated loopback server: $ENDPOINT"
mkfifo "$SERVER_CONTROL"
exec 9<>"$SERVER_CONTROL"
SERVER_CONTROL_OPEN=1
( cd "$(dirname "$HEADLESS")" && exec "$HEADLESS" \
    +set fs_homepath "$SERVER_NATIVE" \
    +set com_automated 1 +set com_noHardReboot 1 \
    +set log_severity DEBUG +set log_file_severity DEBUG \
    +set log_file_mode overwrite_synced \
    +set net_ip 127.0.0.1 +set net_port "$PORT" \
    +set sv_hostname WIRED_Q0_SPECIFY +set sv_pure 0 \
    +set g_autoBots 0 +map arena7 <&9 ) >"$SERVER_STDOUT" 2>&1 &
SERVER_PID=$!

if ! python3 - "$SERVER_JSON" "$SERVER_PID" "$PORT" <<'PYEOF'
import json
import os
import re
import sys
import time

path, pid, port = sys.argv[1], int(sys.argv[2]), sys.argv[3]
deadline = time.monotonic() + 45.0
while time.monotonic() < deadline:
    try:
        os.kill(pid, 0)
    except ProcessLookupError:
        raise SystemExit("FAIL: server exited before readiness")
    messages = []
    try:
        with open(path, "r", encoding="utf-8", errors="replace") as stream:
            for line_no, line in enumerate(stream, 1):
                line = line.strip()
                if not line:
                    continue
                try:
                    row = json.loads(line)
                except ValueError:
                    continue
                if isinstance(row, dict) and isinstance(row.get("msg"), str):
                    messages.append(row["msg"])
    except OSError:
        pass
    listening = any(re.search(rf"WiredNet: listening on port {re.escape(port)} \(IPv4\)", msg) for msg in messages)
    mapped = any(re.search(r"Server: arena7\b", msg) for msg in messages)
    if listening and mapped:
        print("  PASS server readiness: loopback listener + arena7")
        raise SystemExit(0)
    time.sleep(0.1)
raise SystemExit("FAIL: timed out waiting for server readiness")
PYEOF
then
    [ -s "$SERVER_STDOUT" ] && tail -40 "$SERVER_STDOUT"
    exit 1
fi

echo "==> Specify acceptance phase (real loopback connect)"
run_client "$VALID_NATIVE" "$VALID_STDOUT" q0connect-valid.cfg \
    +set net_ip 127.0.0.1 +set wn_cert_verify 0
valid_rc=$?
stop_server
if [ "$valid_rc" -ne 0 ] || [ "$SERVER_FORCED" -ne 0 ] || [ "${SERVER_EXIT_RC:-1}" -ne 0 ] || [ ! -s "$VALID_JSON" ] || [ ! -s "$SERVER_JSON" ]; then
    echo "FAIL: acceptance rc=$valid_rc server_rc=${SERVER_EXIT_RC:-unset} forced=$SERVER_FORCED or missing JSONL"
    [ -s "$VALID_STDOUT" ] && tail -40 "$VALID_STDOUT"
    [ -s "$SERVER_STDOUT" ] && tail -40 "$SERVER_STDOUT"
    exit 1
fi
if grep -q "CRASH BACKTRACE" "$SERVER_STDOUT"; then
    echo "FAIL: server used signal/crash teardown instead of console quit"
    tail -40 "$SERVER_STDOUT"
    exit 1
fi

if analyze_contract "$INVALID_JSON" "$VALID_JSON" "$SERVER_JSON" "$ENDPOINT"; then
    echo "==> WiredUI Specify connect gate: PASS"
else
    echo "FAIL: Specify connect product contract rejected"
    [ -s "$INVALID_STDOUT" ] && { echo "-- invalid stdout --"; tail -30 "$INVALID_STDOUT"; }
    [ -s "$VALID_STDOUT" ] && { echo "-- valid stdout --"; tail -40 "$VALID_STDOUT"; }
    [ -s "$SERVER_STDOUT" ] && { echo "-- server stdout --"; tail -40 "$SERVER_STDOUT"; }
    exit 1
fi
