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
# The "won't close" sub-issue was a consequence of empty text (an empty dialog
# took no input); with the message populated and error_popup.wui's back/copy
# buttons dispatching close (verified in source), the dialog dismisses normally.
# Close itself is not separately drivable headlessly — flagged below.
#
# Usage:  tests/wiredui-errordialog-check.sh [path-to-wired]
# Exit:   0 PASS   1 FAIL   77 SKIP

set -uo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

TEST_MSG="probe_error_payload_12345"

# ── the assertion contract, factored out so --self-test can feed it synthetic
# lines (proving the gate FAILs on a contract violation). Args: <auto_line>
# <interactive_line>. Returns 0 if BOTH contract halves hold, 1 otherwise.
# Quiet unless VERBOSE=1.
assert_contract() {
    local LINE_AUTO="$1" LINE_INTERACTIVE="$2" fail=0
    [ "${VERBOSE:-0}" = 1 ] && { echo "  automated=1 -> ${LINE_AUTO:-<no line>}"; echo "  automated=0 -> ${LINE_INTERACTIVE:-<no line>}"; }
    # automated=1: must be SUPPRESSED (stackTop != error_popup)
    if [ -z "$LINE_AUTO" ]; then
        [ "${VERBOSE:-0}" = 1 ] && echo "  FAIL #3a: no wui_showerror_test line under com_automated 1"; fail=1
    elif echo "$LINE_AUTO" | grep -q "stackTop='error_popup'"; then
        [ "${VERBOSE:-0}" = 1 ] && echo "  FAIL #3a: error_popup WAS shown under com_automated 1 (not suppressed)"; fail=1
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
    if [ "$rc" -eq 0 ]; then echo "==> SELF-TEST PASS: gate accepts the correct contract AND rejects each violation (it has teeth)"; else echo "==> SELF-TEST FAIL"; fi
    exit $rc
fi

WIRED="${1:-$REPO_ROOT/build/debug/wired.x64.exe}"
if [ ! -x "$WIRED" ] && [ -x "$WIRED.exe" ]; then WIRED="$WIRED.exe"; fi
if [ ! -x "$WIRED" ]; then echo "SKIP: wired binary not found: $WIRED"; exit 77; fi
case "$WIRED" in /*) : ;; *) WIRED="$PWD/$WIRED" ;; esac
WIRED_DIR="$(cd "$(dirname "$WIRED")" && pwd)"
WIRED="$WIRED_DIR/$(basename "$WIRED")"

# Run the showerror test once at a given com_automated value; echo the logged
# "wui_showerror_test: ..." line.
run_case() {
    local automated="$1"
    local home_parent home_dir jsonl
    home_parent="$(mktemp -d -t wired-errchk-XXXXXX 2>/dev/null || mktemp -d)"
    home_dir="$home_parent/q3now-preview"
    mkdir -p "$home_dir/base"
    jsonl="$home_dir/qconsole.jsonl"
    local home_native="$home_dir"
    case "$(uname -s)" in MINGW*|MSYS*|CYGWIN*) home_native="$(cygpath -w "$home_dir")" ;; esac

    (
        cd "$WIRED_DIR" || exit 1
        timeout 90 "$WIRED" \
            +set fs_homepath "$home_native" \
            +set com_automated "$automated" \
            +set s_initsound 0 \
            +set r_fullscreen 0 +set r_mode -1 +set r_customwidth 1280 +set r_customheight 720 \
            +set log_renderer_filter 0 \
            +wait 100 \
            +wui_showerror_test "$TEST_MSG" \
            +wait 60 \
            +quit >/dev/null 2>&1
    )
    # Emit the result line (last match wins).
    grep -hoE "wui_showerror_test: automated=[0-9]+ stackTop='[^']*' errMsg='[^']*'" "$jsonl" 2>/dev/null | tail -1
    rm -rf "$home_parent"
}

echo "==> WiredUI error-dialog check: $WIRED"

LINE_AUTO="$(run_case 1)"
LINE_INTERACTIVE="$(run_case 0)"

fail=0
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
# Maps need game data: the isolated home receives an APFS clone (cp -c,
# instant) of the user's pax01 when present; the lifecycle SKIPs without it.
lifecycle_case() {
    local home_parent home_dir jsonl dump rc
    home_parent="$(mktemp -d -t wired-errlc-XXXXXX 2>/dev/null || mktemp -d)"
    home_dir="$home_parent/q3now-preview"
    mkdir -p "$home_dir/base"
    jsonl="$home_dir/qconsole.jsonl"
    local pax01="$HOME/wired/q3now-preview/base/pax01.sw3z"
    if [ ! -f "$pax01" ]; then
        echo "  SKIP #5/#7 lifecycle: no pax01 at $pax01 (maps needed)"
        rm -rf "$home_parent"; return 0
    fi
    cp -c "$pax01" "$home_dir/base/" 2>/dev/null || cp "$pax01" "$home_dir/base/"
    dump="$WIRED_DIR/layoutdump.jsonl"
    rm -f "$dump"
    (
        cd "$WIRED_DIR" || exit 1
        timeout 240 "$WIRED" \
            +set fs_homepath "$home_dir" \
            +set com_automated 0 \
            +set com_noHardReboot 1 \
            +set s_initsound 0 \
            +set r_fullscreen 0 +set r_mode -1 +set r_customwidth 1280 +set r_customheight 720 \
            +set r_layoutDump 1 \
            +wait 100 \
            +wui_showerror_test "$TEST_MSG" +wait 40 \
            +wui_menu_nav back +wait 40 \
            +map arena1 +wait 250 \
            +wui_showerror_test "$TEST_MSG" +wait 40 \
            +map arena7 +wait 250 \
            +quit >/dev/null 2>&1
    )
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
    if [ -s "$dump" ]; then
        python3 - "$dump" <<'LCPY' || lfail=1
import json, sys, collections
frames = collections.defaultdict(bool)
for line in open(sys.argv[1], errors="ignore"):
    try: o = json.loads(line)
    except ValueError: continue
    fr = o.get("frame", 0)
    if o.get("menu") == "error_popup" or o.get("region") == "error_popup":
        frames[fr] = True
    else:
        frames.setdefault(fr, False)
order = sorted(frames)
shown = [f for f in order if frames[f]]
fail = 0
if shown:
    print(f"  PASS #5a: error_popup present in dump frames {shown[0]}..{shown[-1]} (dialog really showed)")
else:
    print("  FAIL #5a: error_popup never appeared in the layout dump"); fail = 1
if shown and any(f > shown[-1] for f in order):
    print("  PASS #5b: popup-free frames follow the last popup frame (cleared — not stuck)")
elif shown:
    print("  FAIL #5b: the dialog is present through the FINAL frame (stuck-dialog symptom)"); fail = 1
if order and not frames[order[-1]]:
    print("  PASS #7b: final frame carries no error_popup")
elif order:
    print("  FAIL #7b: final frame still shows error_popup"); fail = 1
sys.exit(fail)
LCPY
    else
        echo "  FAIL #5: no layout dump produced"; lfail=1
    fi
    rm -rf "$home_parent"
    return $lfail
}
lifecycle_case || fail=1

if [ "$fail" -eq 0 ]; then echo "==> WiredUI error-dialog check: PASS"; else echo "==> WiredUI error-dialog check: FAIL"; fi
exit $fail
