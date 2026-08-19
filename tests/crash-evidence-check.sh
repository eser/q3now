#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#
# Crash evidence validator (TASK-81 #4).
#
# Answers one question about a crashed session: is the evidence it left behind
# actually usable? A crash that produces a file is not the same as a crash that
# produces evidence — a truncated minidump, a report with no build identity, or
# a report whose cvar block leaks the operator's home directory all look like
# success from the outside.
#
# Two modes, and the self-test is not optional:
#
#   --analyze <homepath>   Validate the artefacts a real crash left in a home.
#   --self-test            Prove this validator can REJECT bad evidence.
#
# The self-test exists because a gate that cannot fail certifies everything,
# including the artefacts that quietly lost their contents. It builds corrupt
# artefacts on purpose and requires each one to be refused. (Same discipline as
# --playtest-self-test in tests/headless-map-transition-zonecheck.sh.)

set -u

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=lib/wired_paths.sh
. "$SCRIPT_DIR/lib/wired_paths.sh"

MODE=""
TARGET=""
while [ "$#" -gt 0 ]; do
    case "$1" in
        --analyze)   MODE="analyze"; TARGET="${2:-}"; shift 2 ;;
        --self-test) MODE="selftest"; shift ;;
        *) echo "usage: $0 --analyze <homepath> | --self-test" >&2; exit 64 ;;
    esac
done
[ -n "$MODE" ] || { echo "usage: $0 --analyze <homepath> | --self-test" >&2; exit 64; }

# ── the checks ──────────────────────────────────────────────────────────────
# Emits FAIL: lines for defects and INFO: lines for facts. The caller decides
# the exit code from whether any FAIL appeared, so every check can run and the
# report is complete rather than stopping at the first problem.
crash_report_checks() {
    local report="$1"
    python3 - "$report" <<'PYEOF'
import json, sys

path = sys.argv[1]
try:
    with open(path) as fh:
        d = json.load(fh)
except json.JSONDecodeError as e:
    print(f"FAIL: report is not valid JSON ({e}) — nothing downstream can read it")
    sys.exit(0)
except OSError as e:
    print(f"FAIL: report unreadable ({e})")
    sys.exit(0)

# Build identity. Without it a report cannot be matched to a commit, to a
# wired_playtest.jsonl trail, or to a minidump — it describes a crash in an
# unidentifiable binary, which is close to describing nothing.
for field in ("engine_build_id", "engine_source_revision"):
    if not d.get(field):
        print(f"FAIL: '{field}' missing or empty — the report cannot be tied to a build")

# The fault itself.
if not d.get("reason"):
    print("FAIL: 'reason' missing — the report does not say what went wrong")

cvars = d.get("cvars")
if not isinstance(cvars, dict):
    print("FAIL: 'cvars' missing or not an object — no engine state was captured")
    cvars = {}

# PRIVACY. A crash report is sent to someone else, so a value that identifies
# the operator is a disclosure defect, not a cosmetic one. Absolute paths are
# the common carrier: they embed the account name.
for name, value in cvars.items():
    if not isinstance(value, str):
        continue
    if value.startswith("/") or (len(value) > 2 and value[1] == ":" and value[2] in "\\/"):
        print(f"FAIL: cvar '{name}' discloses an absolute path ('{value}') — "
              f"absolute paths carry the operator's account name")

# Known-bad field that was removed from the allowlist. Named explicitly so a
# re-introduction is caught by name rather than only by the shape rule above.
if "fs_installpath" in cvars:
    print("FAIL: 'fs_installpath' is back in the cvar allowlist — it leaks the home directory")

# Duplicate JSON keys are undefined across parsers; json.load silently keeps the
# last, so this is checked on the raw text instead.
with open(path) as fh:
    raw = fh.read()
for key in cvars:
    if raw.count(f'"{key}"') > 1:
        print(f"FAIL: key '{key}' appears more than once — repeated JSON keys are parser-dependent")

print(f"INFO: build={d.get('engine_build_id')} rev={d.get('engine_source_revision')} "
      f"reason={d.get('reason')} map={cvars.get('mapname', '?')}")
dumpdir = d.get("minidump_database")
print(f"INFO: minidump_database={dumpdir!r}" if dumpdir
      else "INFO: no minidump database recorded (out-of-process backend inactive)")
PYEOF
}

minidump_checks() {
    local dump="$1"
    local size magic

    size=$( wc -c < "$dump" | tr -d '[:space:]' )
    # A minidump always begins with the literal 'MDMP'. Checking the magic
    # separates "a file exists" from "a debugger can open it" — a truncated or
    # zero-length dump is the failure this catches, and it is exactly what a
    # handler that died mid-write leaves behind.
    magic=$( head -c 4 "$dump" | tr -d '\0' )
    if [ "$magic" != "MDMP" ]; then
        echo "FAIL: '$dump' does not start with the MDMP signature (got '$magic')"
        return
    fi
    # Header alone is 32 bytes; anything near that carries no thread or module
    # data and would open in a debugger showing nothing.
    if [ "$size" -lt 4096 ]; then
        echo "FAIL: '$dump' is only $size bytes — too small to contain thread or module data"
        return
    fi
    echo "INFO: minidump $(basename "$dump"): ${size} bytes, MDMP signature present"
}

# ── analyze ─────────────────────────────────────────────────────────────────
if [ "$MODE" = "analyze" ]; then
    HOME_PATH="${TARGET:-$WIRED_HOME}"
    [ -d "$HOME_PATH" ] || { echo "FAIL: home '$HOME_PATH' does not exist"; exit 1; }

    echo "==> crash evidence in $HOME_PATH"
    fail=0

    reports=$( find "$HOME_PATH" -maxdepth 1 -name 'crash_*.json' 2>/dev/null | sort )
    if [ -z "$reports" ]; then
        echo "FAIL: no crash_*.json — a crashed session left no structured report"
        fail=1
    else
        while IFS= read -r r; do
            [ -n "$r" ] || continue
            echo "  report: $(basename "$r")"
            out=$( crash_report_checks "$r" )
            printf '%s\n' "$out" | sed 's/^/    /'
            printf '%s' "$out" | grep -q '^FAIL:' && fail=1
        done <<< "$reports"
    fi

    dumps=$( find "$HOME_PATH/crashdb" -maxdepth 1 -name '*.dmp' 2>/dev/null | sort )
    if [ -z "$dumps" ]; then
        # Not a failure on its own: the out-of-process backend is optional
        # (USE_SENTRY_CRASH=OFF is the default), and TASK-81 #5 explicitly
        # accepts an absent artefact provided the reason is explicit.
        echo "  INFO: no minidump present — out-of-process capture not active for this session"
    else
        while IFS= read -r dmp; do
            [ -n "$dmp" ] || continue
            out=$( minidump_checks "$dmp" )
            printf '%s\n' "$out" | sed 's/^/    /'
            printf '%s' "$out" | grep -q '^FAIL:' && fail=1
        done <<< "$dumps"
    fi

    [ "$fail" = 0 ] && echo "==> CRASH EVIDENCE PASS" || echo "==> CRASH EVIDENCE FAIL"
    exit "$fail"
fi

# ── self-test ───────────────────────────────────────────────────────────────
# Each case builds an artefact with exactly one defect and requires the checks
# to name it. A case that passes here would be a check with no teeth.
if [ "$MODE" = "selftest" ]; then
    TMP="$WIRED_TMP/crash-evidence-selftest.$$"
    mkdir -p "$TMP" || exit 1
    trap 'rm -rf "$TMP"' EXIT
    rc=0

    expect_reject() {
        local name="$1" pattern="$2" out
        out=$( crash_report_checks "$TMP/report.json" )
        if printf '%s' "$out" | grep -q "$pattern"; then
            echo "    reject $name: OK"
        else
            echo "    reject $name: NOT CAUGHT — the check has no teeth here"
            printf '%s\n' "$out" | sed 's/^/      /'
            rc=1
        fi
    }

    echo "==> crash evidence validator self-test"

    # A clean report must PASS, or every rejection below proves nothing.
    cat > "$TMP/report.json" <<'JSON'
{ "engine_build_id": 1, "engine_source_revision": "abc1234", "reason": "SIGSEGV",
  "cvars": { "mapname": "arena1", "cl_renderer": "vulkan" } }
JSON
    out=$( crash_report_checks "$TMP/report.json" )
    if printf '%s' "$out" | grep -q '^FAIL:'; then
        echo "    clean report: REJECTED — the validator fails valid evidence"
        printf '%s\n' "$out" | sed 's/^/      /'
        rc=1
    else
        echo "    clean report: OK"
    fi

    cat > "$TMP/report.json" <<'JSON'
{ "engine_source_revision": "abc1234", "reason": "SIGSEGV", "cvars": {} }
JSON
    expect_reject "missing build id" "engine_build_id"

    cat > "$TMP/report.json" <<'JSON'
{ "engine_build_id": 1, "engine_source_revision": "", "reason": "SIGSEGV", "cvars": {} }
JSON
    expect_reject "empty source revision" "engine_source_revision"

    cat > "$TMP/report.json" <<'JSON'
{ "engine_build_id": 1, "engine_source_revision": "abc1234", "cvars": {} }
JSON
    expect_reject "missing reason" "'reason' missing"

    cat > "$TMP/report.json" <<'JSON'
{ "engine_build_id": 1, "engine_source_revision": "abc1234", "reason": "SIGSEGV",
  "cvars": { "fs_installpath": "/Users/someone/Library/Application Support/q3now" } }
JSON
    expect_reject "posix home-path leak" "absolute path"
    expect_reject "fs_installpath by name" "fs_installpath"

    cat > "$TMP/report.json" <<'JSON'
{ "engine_build_id": 1, "engine_source_revision": "abc1234", "reason": "SIGSEGV",
  "cvars": { "some_path": "C:\\Users\\someone\\AppData" } }
JSON
    expect_reject "windows home-path leak" "absolute path"

    cat > "$TMP/report.json" <<'JSON'
{ "engine_build_id": 1, "engine_source_revision": "abc1234", "reason": "SIGSEGV",
  "cvars": { "mapname": "arena1", "mapname": "arena7" } }
JSON
    expect_reject "duplicate key" "more than once"

    printf 'not json at all' > "$TMP/report.json"
    expect_reject "malformed json" "not valid JSON"

    # Minidump shape.
    printf 'XXXX' > "$TMP/bad.dmp"
    out=$( minidump_checks "$TMP/bad.dmp" )
    printf '%s' "$out" | grep -q 'MDMP signature' \
        && echo "    reject wrong magic: OK" \
        || { echo "    reject wrong magic: NOT CAUGHT"; rc=1; }

    # Correct magic but truncated: the case a real half-written dump produces,
    # and the one a magic-only check would wave through.
    printf 'MDMP' > "$TMP/trunc.dmp"
    head -c 100 /dev/zero >> "$TMP/trunc.dmp"
    out=$( minidump_checks "$TMP/trunc.dmp" )
    printf '%s' "$out" | grep -q 'too small' \
        && echo "    reject truncated dump: OK" \
        || { echo "    reject truncated dump: NOT CAUGHT"; rc=1; }

    if [ "$rc" = 0 ]; then
        echo "==> SELF-TEST PASS: the validator rejects missing identity, missing reason, "
        echo "    path disclosure, duplicate keys, malformed JSON, and unusable minidumps"
    else
        echo "==> SELF-TEST FAIL"
    fi
    exit "$rc"
fi
