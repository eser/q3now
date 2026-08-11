#!/usr/bin/env bash
# wiredui-errordialog-check.sh — computed check for the error-dialog fix (#3).
#
# Drives the wui_showerror_test console command (a headless affordance that
# replicates the real cl_main.c caller: Com_SetLastError then
# CL_WiredUI_ShowError) under both com_automated settings and asserts the
# contract from the logged "wui_showerror_test: ..." line:
#
#   com_automated 1 (automated/headless):
#     stackTop must NOT be 'error_popup'  → the blocking dialog is SUPPRESSED.
#   com_automated 0 (interactive):
#     stackTop MUST be 'error_popup'      → the dialog DOES show, and
#     errMsg   MUST equal the message     → text is populated (not empty).
#
# The lifecycle half below drives the real ESC/Back path and proves the popup
# exposes its main-menu parent, consumes com_errorMessage, then remains clear
# across two exact gameplay transitions.
#
# Usage:  tests/wiredui-errordialog-check.sh [path-to-wired]
# Exit:   0 PASS   1 FAIL   77 SKIP

set -uo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
TIMEOUT_RUNNER="$SCRIPT_DIR/run-with-timeout.py"
LIFECYCLE_CFG_SOURCE="$SCRIPT_DIR/fixtures/wiredui-error-lifecycle.cfg"
LIFECYCLE_ANALYZER="$SCRIPT_DIR/wiredui-errordialog-analyze.py"

TEST_MSG="probe_error_payload_12345"
LIFECYCLE_TIMEOUT="${WIRED_ERROR_LIFECYCLE_TIMEOUT:-240}"

# ── the assertion contract, factored out so --self-test can feed it synthetic
# lines (proving the gate FAILs on a contract violation). Args: <auto_line>
# <interactive_line>. Returns 0 if BOTH contract halves hold, 1 otherwise.
# Quiet unless VERBOSE=1.
assert_contract() {
    local LINE_AUTO="$1" LINE_INTERACTIVE="$2" fail=0
    [ "${VERBOSE:-0}" = 1 ] && { echo "  automated=1 -> ${LINE_AUTO:-<no line>}"; echo "  automated=0 -> ${LINE_INTERACTIVE:-<no line>}"; }
    # automated=1: must be SUPPRESSED while preserving the known parent menu.
    # Merely checking "not error_popup" would accept a broken UI init whose
    # stack is empty, so this half requires the exact main-menu owner.
    if [ -z "$LINE_AUTO" ]; then
        [ "${VERBOSE:-0}" = 1 ] && echo "  FAIL #3a: no wui_showerror_test line under com_automated 1"; fail=1
    elif ! echo "$LINE_AUTO" | grep -q "stackTop='main'"; then
        [ "${VERBOSE:-0}" = 1 ] && echo "  FAIL #3a: automated suppression did not preserve stackTop='main'"; fail=1
    else
        [ "${VERBOSE:-0}" = 1 ] && echo "  PASS #3a: error_popup suppressed under com_automated 1"
    fi
    # automated=0: must SHOW with populated message
    if [ -z "$LINE_INTERACTIVE" ]; then
        [ "${VERBOSE:-0}" = 1 ] && echo "  FAIL #3b: no wui_showerror_test line under com_automated 0"; fail=1
    else
        local shown=0 msgok=0
        echo "$LINE_INTERACTIVE" | grep -q "stackTop='error_popup'" && shown=1
        echo "$LINE_INTERACTIVE" | grep -q "errMsg='$TEST_MSG'" && msgok=1
        if [ "$shown" -eq 1 ] && [ "$msgok" -eq 1 ]; then
            [ "${VERBOSE:-0}" = 1 ] && echo "  PASS #3b: interactive error_popup shown AND text populated (errMsg='$TEST_MSG')"
        else
            [ "${VERBOSE:-0}" = 1 ] && { [ "$shown" -ne 1 ] && echo "  FAIL #3b: error_popup NOT shown under com_automated 0"; [ "$msgok" -ne 1 ] && echo "  FAIL #3b: errMsg not populated (empty-dialog bug)"; }
            fail=1
        fi
    fi
    return $fail
}

# ── --self-test: prove the contract assertion has teeth (FAILs on each
# violation, PASSes on the clean pair) without launching the engine ──
if [ "${1:-}" = "--self-test" ]; then
    echo "==> WiredUI error-dialog check SELF-TEST (gate-has-teeth)"
    CLEAN_AUTO="wui_showerror_test: automated=1 stackTop='main' errMsg='$TEST_MSG'"
    CLEAN_INT="wui_showerror_test: automated=0 stackTop='error_popup' errMsg='$TEST_MSG'"
    rc=0
    echo "  -- CLEAN pair (suppressed when automated, shown+populated when interactive) → expect PASS --"
    assert_contract "$CLEAN_AUTO" "$CLEAN_INT" && echo "    clean: PASS" || { echo "    clean: FAIL (BUG: gate rejects a correct contract)"; rc=1; }
    # Defect 1: popup NOT suppressed under automated.
    echo "  -- DEFECT 1: error_popup shown under com_automated 1 → expect FAIL --"
    assert_contract "wui_showerror_test: automated=1 stackTop='error_popup' errMsg='$TEST_MSG'" "$CLEAN_INT" \
        && { echo "    defect1: PASS (BUG: gate blind to un-suppressed popup)"; rc=1; } || echo "    defect1: FAIL-as-expected"
    # Defect 2: popup NOT shown under interactive.
    echo "  -- DEFECT 2: error_popup NOT shown under com_automated 0 → expect FAIL --"
    assert_contract "$CLEAN_AUTO" "wui_showerror_test: automated=0 stackTop='main' errMsg='$TEST_MSG'" \
        && { echo "    defect2: PASS (BUG: gate blind to missing dialog)"; rc=1; } || echo "    defect2: FAIL-as-expected"
    # Defect 3: empty message (the original empty-dialog bug).
    echo "  -- DEFECT 3: interactive dialog shown but errMsg empty → expect FAIL --"
    assert_contract "$CLEAN_AUTO" "wui_showerror_test: automated=0 stackTop='error_popup' errMsg=''" \
        && { echo "    defect3: PASS (BUG: gate blind to empty dialog)"; rc=1; } || echo "    defect3: FAIL-as-expected"
    if ! python3 "$LIFECYCLE_ANALYZER" --self-test; then rc=1; fi
    if [ "$rc" -eq 0 ]; then echo "==> SELF-TEST PASS: gate accepts the correct contract AND rejects each violation (it has teeth)"; else echo "==> SELF-TEST FAIL"; fi
    exit $rc
fi

WIRED="${1:-$REPO_ROOT/build/debug/wired.x64.exe}"
if [ ! -x "$WIRED" ] && [ -x "$WIRED.exe" ]; then WIRED="$WIRED.exe"; fi
if [ ! -x "$WIRED" ]; then echo "SKIP: wired binary not found: $WIRED"; exit 77; fi
case "$WIRED" in /*) : ;; *) WIRED="$PWD/$WIRED" ;; esac
WIRED_DIR="$(cd "$(dirname "$WIRED")" && pwd)"
WIRED="$WIRED_DIR/$(basename "$WIRED")"

if [ ! -f "$TIMEOUT_RUNNER" ] ||
   ! python3 -c 'import sys; raise SystemExit(0 if sys.version_info >= (3,8) else 1)' 2>/dev/null; then
    echo "SKIP: Python >=3.8 and tests/run-with-timeout.py are required"
    exit 77
fi

PACK_ROOT=""
for candidate in "$WIRED_DIR" "$WIRED_DIR/../Resources" "$WIRED_DIR/../../.."; do
    if [ -f "$candidate/base/pax21.sw3z" ]; then
        PACK_ROOT="$(cd "$candidate" && pwd)"
        break
    fi
done
if [ -z "$PACK_ROOT" ]; then
    echo "SKIP: no current base/pax21.sw3z found beside bundle/install for $WIRED"
    exit 77
fi

CONTENT_ROOT=""
BASE_ARCHIVE=""
for candidate in "${WIRED_CONTENT_ROOT:-}" "$PACK_ROOT"; do
    [ -n "$candidate" ] || continue
    if [ -f "$candidate/base/pax01.sw3z" ]; then
        CONTENT_ROOT="$(cd "$candidate" && pwd)"
        BASE_ARCHIVE="$CONTENT_ROOT/base/pax01.sw3z"
        break
    fi
    if [ -f "$candidate/base/pak0.pk3" ]; then
        CONTENT_ROOT="$(cd "$candidate" && pwd)"
        BASE_ARCHIVE="$CONTENT_ROOT/base/pak0.pk3"
        break
    fi
done
if [ -z "$BASE_ARCHIVE" ]; then
    echo "SKIP: canonical base content missing; set WIRED_CONTENT_ROOT to a root containing base/pax01.sw3z or base/pak0.pk3"
    exit 77
fi

stage_exact_content() {
    local target_home="$1"
    mkdir -p "$target_home/base"
    cp "$BASE_ARCHIVE" "$target_home/base/$(basename "$BASE_ARCHIVE")" &&
        cp "$PACK_ROOT/base/pax21.sw3z" "$target_home/base/pax21.sw3z"
}

# Run the showerror test once at a given com_automated value; echo the logged
# "wui_showerror_test: ..." line.
run_case() {
    local automated="$1"
    local home_parent home_dir home_native jsonl stdout rc analyze_rc line
    home_parent="$(mktemp -d -t wired-errchk-XXXXXX 2>/dev/null || mktemp -d)"
    home_dir="$home_parent/q3now-preview"
    if ! stage_exact_content "$home_dir"; then
        echo "FAIL: could not stage exact content for error-dialog case" >&2
        rm -rf "$home_parent"
        return 1
    fi
    jsonl="$home_dir/qconsole.jsonl"
    stdout="$home_parent/wired.stdout"
    home_native="$home_dir"
    case "$(uname -s)" in MINGW*|MSYS*|CYGWIN*) home_native="$(cygpath -w "$home_dir")" ;; esac

    python3 "$TIMEOUT_RUNNER" \
        --timeout 90 \
        --kill-after 15 \
        --cwd "$home_parent" \
        --stdout "$stdout" \
        -- "$WIRED" \
        +set fs_homepath "$home_native" \
        +set com_automated "$automated" \
        +set s_initsound 0 \
        +set r_fullscreen 0 +set r_mode -1 +set r_customwidth 1280 +set r_customheight 720 \
        +set log_renderer_filter 0 \
        +wait 100 \
        +wui_push main \
        +wait 20 \
        +wui_showerror_test "$TEST_MSG" \
        +wait 60 \
        +quit
    rc=$?
    if [ "$rc" -ne 0 ]; then
        echo "FAIL: error-dialog case automated=$automated exited with status $rc" >&2
        [ -s "$stdout" ] && tail -15 "$stdout" | sed 's/^/  /' >&2
    fi
    # Strictly parse the phase log.  The real test path emits one intentional
    # probe ERROR; allow exactly that record and reject every other ERROR/FATAL
    # or UI warning.  Diagnostics go to stderr so command substitution receives
    # only the authoritative result line.
    analyze_rc=0
    line="$(python3 - "$jsonl" "$automated" "$TEST_MSG" <<'CASEPY'
import json, re, sys
path, automated, payload = sys.argv[1:4]
rows = []
errors = 0
try:
    with open(path, encoding="utf-8", errors="replace") as stream:
        for raw in stream:
            if not raw.strip():
                continue
            try:
                row = json.loads(raw)
            except ValueError:
                errors += 1
                continue
            if not isinstance(row, dict):
                errors += 1
            else:
                rows.append(row)
except OSError as exc:
    print(f"FAIL: cannot read error-dialog phase log: {exc}", file=sys.stderr)
    raise SystemExit(1)
if errors:
    print(f"FAIL: error-dialog phase has {errors} malformed/non-object JSON record(s)", file=sys.stderr)
    raise SystemExit(1)
expected_error = f"Error: {payload}"
expected_suppressed = f"Connect error (dialog suppressed, automated): {payload}"
probe_errors = [row for row in rows
                if str(row.get("sev", "")).upper() == "ERROR"
                and str(row.get("msg", "")).strip() == expected_error]
suppressed_errors = [row for row in rows
                     if str(row.get("sev", "")).upper() == "ERROR"
                     and str(row.get("msg", "")).strip() == expected_suppressed]
allowed_messages = {expected_error}
if automated == "1":
    allowed_messages.add(expected_suppressed)
bad = [row for row in rows
       if (str(row.get("sev", "")).upper() in {"ERROR", "FATAL"}
           and not (str(row.get("sev", "")).upper() == "ERROR"
                    and str(row.get("msg", "")).strip() in allowed_messages))
       or (str(row.get("sev", "")).upper() == "WARN"
           and str(row.get("cat", "")).lower() == "ui")]
expected_suppressed_count = 1 if automated == "1" else 0
if (len(probe_errors) != 1 or len(suppressed_errors) != expected_suppressed_count
        or bad):
    print(f"FAIL: probe ERROR={len(probe_errors)} (want 1), suppressed ERROR="
          f"{len(suppressed_errors)} (want {expected_suppressed_count}), "
          f"unexpected severity={len(bad)}",
          file=sys.stderr)
    raise SystemExit(1)
pattern = re.compile(
    rf"wui_showerror_test: automated={re.escape(automated)} "
    rf"stackTop='[^']*' errMsg='{re.escape(payload)}'")
matches = [str(row.get("msg", "")).strip() for row in rows
           if pattern.fullmatch(str(row.get("msg", "")).strip())]
if len(matches) != 1:
    print(f"FAIL: expected one authoritative wui_showerror_test result, got {len(matches)}",
          file=sys.stderr)
    raise SystemExit(1)
print(matches[0])
CASEPY
)" || analyze_rc=$?
    [ -n "$line" ] && printf '%s\n' "$line"
    rm -rf "$home_parent"
    [ "$rc" -ne 0 ] && return "$rc"
    return "$analyze_rc"
}

echo "==> WiredUI error-dialog check: $WIRED"
echo "    current pack: $PACK_ROOT/base/pax21.sw3z"
echo "    base archive: $BASE_ARCHIVE (read-only source)"

LINE_AUTO="$(run_case 1)"; RC_AUTO=$?
LINE_INTERACTIVE="$(run_case 0)"; RC_INTERACTIVE=$?

fail=0
[ "$RC_AUTO" -eq 0 ] || { echo "  FAIL #3a: automated phase runner/analyzer rc=$RC_AUTO"; fail=1; }
[ "$RC_INTERACTIVE" -eq 0 ] || { echo "  FAIL #3b: interactive phase runner/analyzer rc=$RC_INTERACTIVE"; fail=1; }
VERBOSE=1 assert_contract "$LINE_AUTO" "$LINE_INTERACTIVE" || fail=1

# ── #5/#7 lifecycle case (one interactive run) ────────────────────────────────
# Closes the coverage gap the old #3c NOTE used to flag. A single
# com_automated=0 run with r_layoutDump exercises the full dialog LIFECYCLE:
#   show → dismiss (wui_menu_nav back = the real ESC path) → map load →
#   show AGAIN over the running game → re-map.
# Computed assertions (no eyes):
#   #5a  some dump frame CONTAINS the error_popup region (it really showed)
#   #5b  popup-free frames FOLLOW the last popup frame (dismiss/map-load
#        clears it — the stuck-dialog symptom would keep it to the end)
#   #7a  both maps reach load and the run exits cleanly with zero
#        FATAL/Z_Free lines (popup → re-map does not crash)
#   #7b  the FINAL dump frame carries no error_popup
# Maps need game data: the same explicit canonical base archive + exact current
# pax21 allowlist used above is copied into this lifecycle's isolated home.
lifecycle_case() {
    local home_parent home_dir home_native jsonl dump stdout rc
    home_parent="$(mktemp -d -t wired-errlc-XXXXXX 2>/dev/null || mktemp -d)"
    home_dir="$home_parent/q3now-preview"
    if ! stage_exact_content "$home_dir"; then
        echo "  FAIL #5/#7 lifecycle: could not stage exact content"
        rm -rf "$home_parent"
        return 1
    fi
    if ! cp "$LIFECYCLE_CFG_SOURCE" "$home_dir/base/wiredui-error-lifecycle.cfg"; then
        echo "  FAIL #5/#7 lifecycle: could not stage lifecycle cfg"
        rm -rf "$home_parent"
        return 1
    fi
    jsonl="$home_dir/qconsole.jsonl"
    dump="$home_parent/layoutdump.jsonl"
    stdout="$home_parent/wired.stdout"
    home_native="$home_dir"
    case "$(uname -s)" in MINGW*|MSYS*|CYGWIN*) home_native="$(cygpath -w "$home_dir")" ;; esac
    python3 "$TIMEOUT_RUNNER" \
        --timeout "$LIFECYCLE_TIMEOUT" \
        --kill-after 15 \
        --cwd "$home_parent" \
        --stdout "$stdout" \
        -- "$WIRED" \
        +set fs_homepath "$home_native" \
        +set com_automated 0 \
        +set com_noHardReboot 1 \
        +set s_initsound 0 \
        +set r_fullscreen 0 +set r_mode -1 +set r_customwidth 1280 +set r_customheight 720 \
        +set r_layoutDump 1 \
        +set log_severity DEBUG \
        +set log_file_severity DEBUG \
        +exec wiredui-error-lifecycle.cfg
    rc=$?
    local lfail=0 maps fatals
    maps="$(grep -c "Server: arena" "$jsonl" 2>/dev/null | tr -cd '0-9')"
    fatals="$(grep -ciE "FATAL|Z_Free|crashed" "$jsonl" 2>/dev/null | tr -cd '0-9')"
    if [ "$rc" -eq 0 ] && [ "${maps:-0}" -ge 2 ] && [ "${fatals:-0}" -eq 0 ]; then
        echo "  PASS #7a: popup → dismiss → map → popup-over-game → re-map: ${maps} map loads, 0 FATAL, clean exit"
    else
        echo "  FAIL #7a: rc=$rc maps=${maps:-0} fatal-lines=${fatals:-0} (popup/re-map lifecycle broke the run)"
        lfail=1
    fi
    # Causal lifecycle trace: the first popup is authoritatively popped to its
    # main-menu parent before arena1 becomes active; the second popup is shown
    # over gameplay before the independent arena7 first frame.  Presence-only
    # map counts cannot substitute for this order/map identity contract.
    python3 - "$jsonl" "$TEST_MSG" <<'TRACEPY' || lfail=1
import json, re, sys
path, payload = sys.argv[1:3]
rows = []
parse_errors = 0
try:
    with open(path, encoding="utf-8", errors="replace") as stream:
        for line in stream:
            if not line.strip():
                continue
            try:
                row = json.loads(line)
                if not isinstance(row, dict):
                    parse_errors += 1
                else:
                    rows.append(row)
            except ValueError:
                parse_errors += 1
except OSError as exc:
    print(f"  FAIL #7c: cannot read lifecycle log: {exc}")
    raise SystemExit(1)
if parse_errors:
    print(f"  FAIL #7c: lifecycle log has {parse_errors} malformed/non-object JSON record(s)")
    raise SystemExit(1)
messages = [str(row.get("msg", "")) for row in rows]
# wui_showerror_test intentionally enters the real Com_Error/UI path, which
# emits one exact ERROR record per requested popup.  Permit only those two
# causal probe records; every other ERROR/FATAL and every UI WARN stays fatal.
expected_error = f"Error: {payload}"
probe_errors = [row for row in rows
                if str(row.get("sev", "")).upper() == "ERROR"
                and str(row.get("msg", "")).strip() == expected_error]
bad = [row for row in rows
       if (str(row.get("sev", "")).upper() in {"ERROR", "FATAL"}
           and not (str(row.get("sev", "")).upper() == "ERROR"
                    and str(row.get("msg", "")).strip() == expected_error))
       or (str(row.get("sev", "")).upper() == "WARN"
           and str(row.get("cat", "")).lower() == "ui")]
if len(probe_errors) != 2:
    print(f"  FAIL #7c: expected exactly two causal probe ERROR records, got {len(probe_errors)}")
    raise SystemExit(1)
if bad:
    print(f"  FAIL #7c: lifecycle log has {len(bad)} ERROR/FATAL or cat=ui WARN record(s)")
    for row in bad[:5]:
        print(f"    {row.get('sev', '?')}: {row.get('msg', '')}")
    raise SystemExit(1)

steps = [
    ("first popup shown", rf"wui_showerror_test: automated=0 stackTop='error_popup' errMsg='{re.escape(payload)}'"),
    ("Back exposed main parent", r"WiredUI: pop menu \(depth 1\)"),
    ("Back dispatch returned", r"wui_menu_nav: K_ESCAPE dispatched"),
    ("Back consumed error payload", r'"com_errorMessage" is:"(?:\^7)?"'),
    ("arena1 first frame", r"FIRST GAMEPLAY FRAME mapname=(?:maps/)?arena1(?:\.bsp)?(?:\s|$)"),
    ("second popup shown", rf"wui_showerror_test: automated=0 stackTop='error_popup' errMsg='{re.escape(payload)}'"),
    ("arena7 first frame", r"FIRST GAMEPLAY FRAME mapname=(?:maps/)?arena7(?:\.bsp)?(?:\s|$)"),
    ("arena7 cleared error payload", r'"com_errorMessage" is:"(?:\^7)?"'),
]
cursor = 0
for label, pattern in steps:
    for index in range(cursor, len(messages)):
        if re.search(pattern, messages[index]):
            cursor = index + 1
            break
    else:
        print(f"  FAIL #7c: missing/out-of-order lifecycle step: {label}")
        raise SystemExit(1)
print("  PASS #7c: popup → Back main/depth1 + consumed payload → arena1 FIRST → popup → arena7 FIRST + clear ordered")
TRACEPY
    if [ -s "$dump" ]; then
        python3 - "$dump" <<'LCPY' || lfail=1
import json, sys, collections
frames = collections.defaultdict(bool)
frame_menus = collections.defaultdict(set)
parse_errors = 0
for line in open(sys.argv[1], errors="ignore"):
    if not line.strip():
        continue
    try: o = json.loads(line)
    except ValueError:
        parse_errors += 1
        continue
    if not isinstance(o, dict):
        parse_errors += 1
        continue
    fr = o.get("frame", 0)
    if o.get("menu"):
        frame_menus[fr].add(str(o.get("menu")))
    if o.get("menu") == "error_popup" or o.get("region") == "error_popup":
        frames[fr] = True
    else:
        frames.setdefault(fr, False)
order = sorted(frames)
shown = [f for f in order if frames[f]]
fail = 0
if parse_errors:
    print(f"  FAIL #5: layout dump has {parse_errors} malformed/non-object JSON record(s)")
    fail = 1
if shown:
    print(f"  PASS #5a: error_popup present in dump frames {shown[0]}..{shown[-1]} (dialog really showed)")
else:
    print("  FAIL #5a: error_popup never appeared in the layout dump"); fail = 1
# Two popup presentations must be separated by at least one represented frame
# without error_popup.  This ties the first dismissal to the real Back/pop
# trace instead of letting the later popup mask a stuck first dialog.
segments = []
for f in shown:
    if not segments or f > segments[-1][-1] + 1:
        segments.append([f])
    else:
        segments[-1].append(f)
if len(segments) >= 2 and any(not frames[f] and "main" in frame_menus[f] for f in order
                              if segments[0][-1] < f < segments[1][0]):
    print("  PASS #5b: popup-free main-menu frames separate the first Back dismissal from the second popup")
else:
    print("  FAIL #5b: no popup-free main-menu frame separates the two popup presentations"); fail = 1
if order and not frames[order[-1]]:
    print("  PASS #7b: final frame carries no error_popup")
elif order:
    print("  FAIL #7b: final frame still shows error_popup"); fail = 1
sys.exit(fail)
LCPY
    else
        echo "  FAIL #5: no layout dump produced"; lfail=1
    fi
    python3 "$LIFECYCLE_ANALYZER" --lifecycle "$jsonl" "$dump" "$TEST_MSG" || lfail=1
    if [ "$lfail" -ne 0 ] || [ "${KEEP_ARTIFACTS:-0}" = "1" ]; then
        echo "  kept lifecycle artifacts: $home_parent"
        if [ -s "$stdout" ]; then
            echo "  -- lifecycle stdout tail --"
            tail -30 "$stdout" | sed 's/^/  /'
        fi
    else
        rm -rf "$home_parent"
    fi
    return $lfail
}
lifecycle_case || fail=1

if [ "$fail" -eq 0 ]; then echo "==> WiredUI error-dialog check: PASS"; else echo "==> WiredUI error-dialog check: FAIL"; fi
exit $fail
