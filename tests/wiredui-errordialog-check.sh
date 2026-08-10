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

echo "  NOTE #3c (close): the back/copy buttons in error_popup.wui dispatch"
echo "       'close' -> WiredScript_Close -> WiredUI_PopMenu (verified in source)."
echo "       With text now populated the dialog is focusable/dismissable; driving"
echo "       an actual button click headlessly is not covered here (coverage gap)."

if [ "$fail" -eq 0 ]; then echo "==> WiredUI error-dialog check: PASS"; else echo "==> WiredUI error-dialog check: FAIL"; fi
exit $fail
