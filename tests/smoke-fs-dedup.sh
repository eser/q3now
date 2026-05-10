#!/usr/bin/env bash
# smoke-fs-dedup.sh — regression test for FS_DeduplicateArchives SW3Z double-free
#
# Reproduces the bug pattern: two same-basename SW3Z archives in different
# search-path roots (basepath/baseq3 vs homepath/baseq3). Pre-fix: engine
# crashes during FS_Startup with "Z_Free: freed a freed pointer" because the
# SW3Z dedup branch called SW3Z_CloseArchive() (which Z_Free's the pack
# internally) AND then redundantly Z_Free'd s->pack.
#
# This smoke fails (non-zero exit) on a binary built before the fix, passes
# on a fixed binary. See git log for the FS_DeduplicateArchives commit.
#
# Usage:
#   tests/smoke-fs-dedup.sh [path-to-wired-ded]
#
# Exit codes:
#   0  PASS — engine boots cleanly, dedup log line emitted
#   1  FAIL — engine crashed, missing dedup line, or other failure
#  77  SKIP — wired-ded binary or sw3z archiver not built

set -euo pipefail

DED="${1:-wired-ded}"
SW3Z_TOOL="${SW3Z_TOOL:-tools/sw3z-archiver/cmd/sw3z/sw3z}"

# ── locate binaries ─────────────────────────────────────────────────────────
if [ ! -x "$DED" ] && ! command -v "$DED" >/dev/null 2>&1; then
  # Try common build locations
  for candidate in \
    "build/release/wired-ded.x64.exe" \
    "build/release/wired-ded.x86_64" \
    "build/release/wired-ded"; do
    if [ -x "$candidate" ]; then DED="$candidate"; break; fi
  done
fi
if [ ! -x "$DED" ] && ! command -v "$DED" >/dev/null 2>&1; then
  echo "SKIP: wired-ded binary not found (tried: $1, build/release/wired-ded*)"
  exit 77
fi

if [ ! -x "$SW3Z_TOOL" ]; then
  echo "SKIP: sw3z archiver not built at $SW3Z_TOOL (run: cd tools/sw3z-archiver && go build ./cmd/sw3z)"
  exit 77
fi

# ── build fixture under build/release/test-fixtures/dedup/ ──────────────────
FIXTURE_ROOT="${FIXTURE_ROOT:-build/release/test-fixtures/dedup}"
BASEPATH="$FIXTURE_ROOT/basepath"
HOMEPATH="$FIXTURE_ROOT/homepath"
SEED="$FIXTURE_ROOT/seed"

rm -rf "$FIXTURE_ROOT"
mkdir -p "$BASEPATH/baseq3" "$HOMEPATH/baseq3" "$SEED"

# Minimal SW3Z content: needs default.cfg so FS_Restart's
# `FS_ReadFile("default.cfg")` post-check passes; otherwise engine
# Com_Terminates on TERM_UNRECOVERABLE before reaching +quit.
# The dedup code path runs BEFORE that check, so even a no-default.cfg
# fixture exercises the bug — but exit code wouldn't be 0.
echo "// regression fixture marker — empty default.cfg" > "$SEED/default.cfg"
echo "regression fixture for FS_DeduplicateArchives" > "$SEED/dummy.txt"
"$SW3Z_TOOL" a "$BASEPATH/baseq3/regression_dup.sw3z" "$SEED" >/dev/null
cp "$BASEPATH/baseq3/regression_dup.sw3z" "$HOMEPATH/baseq3/regression_dup.sw3z"

# ── convert to engine-readable paths ────────────────────────────────────────
# Engine on Windows expects Windows-native paths; on Unix accepts as-is.
to_native() {
  local p="$1"
  if command -v cygpath >/dev/null 2>&1; then
    cygpath -w "$p"
  else
    # Resolve to absolute path even without cygpath
    ( cd "$p" && pwd )
  fi
}

BASEPATH_NATIVE=$(to_native "$BASEPATH")
HOMEPATH_NATIVE=$(to_native "$HOMEPATH")

# ── run engine, capture output ──────────────────────────────────────────────
LOGFILE=$(mktemp -t q3now-fs-dedup-XXXXXX.log)
trap "rm -f $LOGFILE" EXIT

echo "==> Running FS dedup smoke"
echo "    basepath: $BASEPATH_NATIVE"
echo "    homepath: $HOMEPATH_NATIVE"

set +e
"$DED" \
  +set fs_installpath "$BASEPATH_NATIVE" \
  +set fs_homepath "$HOMEPATH_NATIVE" \
  +set fs_basegame baseq3 \
  +set dedicated 2 \
  +quit \
  >"$LOGFILE" 2>&1
EC=$?
set -e

# ── assertions ──────────────────────────────────────────────────────────────
fail() {
  echo "FAIL: $*"
  echo "── engine output (last 40 lines) ──"
  tail -40 "$LOGFILE" || true
  echo "── (full log: $LOGFILE)"
  exit 1
}

# (a) clean exit
if [ "$EC" -ne 0 ]; then
  fail "engine exited with code $EC (expected 0)"
fi

# Z_Free crash signature must NOT be present (pre-fix produced this).
if grep -q "Z_Free: freed a freed pointer" "$LOGFILE"; then
  fail "Z_Free double-free detected in output"
fi

# (a-extended) dedup log line MUST be present — proves we exercised the
# FS_DeduplicateArchives code path with an actual duplicate.
# Exact format from files.c: "FS_Precedence: removed N duplicate archives, M unique remain"
if ! grep -qE "FS_Precedence: removed 1 duplicate archives" "$LOGFILE"; then
  fail "expected 'FS_Precedence: removed 1 duplicate archives' log line not found"
fi

echo "PASS: FS dedup smoke — clean exit, dedup line present, no Z_Free crash"
exit 0
