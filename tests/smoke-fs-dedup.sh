#!/usr/bin/env bash
# smoke-fs-dedup.sh — regression test for FS_DeduplicateArchives SW3Z double-free
#
# Reproduces the bug pattern: two same-basename SW3Z archives in different
# search-path roots (basepath/base vs homepath/base). Pre-fix: engine
# crashes during FS_Startup with "Z_Free: freed a freed pointer" because the
# SW3Z dedup branch called SW3Z_CloseArchive() (which Z_Free's the pack
# internally) AND then redundantly Z_Free'd s->pack.
#
# This smoke fails (non-zero exit) on a binary built before the fix, passes
# on a fixed binary. See git log for the FS_DeduplicateArchives commit.
#
# Usage:
#   tests/smoke-fs-dedup.sh [path-to-wired-headless]
#
# Exit codes:
#   0  PASS — engine boots cleanly, dedup log line emitted
#   1  FAIL — engine crashed, missing dedup line, or other failure
#  77  SKIP — wired-headless binary or sw3z archiver not built

set -euo pipefail

# shellcheck source=tests/lib/wired_paths.sh
. "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/lib/wired_paths.sh"

DED="${1:-wired-headless}"
SW3Z_TOOL="${SW3Z_TOOL:-$WIRED_SOURCE/tools/sw3z-archiver/cmd/sw3z/sw3z}"

# ── locate binaries ─────────────────────────────────────────────────────────
if [ ! -x "$DED" ] && ! command -v "$DED" >/dev/null 2>&1; then
  # Try common build locations
  for candidate in \
    "build/wired-headless.x64.exe" \
    "build/wired-headless.x86_64" \
    "build/wired-headless"; do
    if [ -x "$candidate" ]; then DED="$candidate"; break; fi
  done
fi
if [ ! -x "$DED" ] && ! command -v "$DED" >/dev/null 2>&1; then
  echo "SKIP: wired-headless binary not found (tried: $1, build/wired-headless*)"
  exit 77
fi

if [ ! -x "$SW3Z_TOOL" ]; then
  echo "SKIP: sw3z archiver not built at $SW3Z_TOOL (run: cd tools/sw3z-archiver && go build ./cmd/sw3z)"
  exit 77
fi

# ── build fixture under build/test-fixtures/dedup/ ──────────────────
FIXTURE_ROOT="${FIXTURE_ROOT:-$WIRED_SOURCE/build/test-fixtures/dedup}"
BASEPATH="$FIXTURE_ROOT/basepath"
HOMEPATH="$FIXTURE_ROOT/homepath"
SEED="$FIXTURE_ROOT/seed"
PAK0_SEED="$FIXTURE_ROOT/pak0-seed"
PAK8_SEED="$FIXTURE_ROOT/pak8-seed"
PAX01_SEED="$FIXTURE_ROOT/pax01-seed"
PAX21_SEED="$FIXTURE_ROOT/pax21-seed"
SAME_PK3_SEED="$FIXTURE_ROOT/same-pk3-seed"
SAME_SW3Z_SEED="$FIXTURE_ROOT/same-sw3z-seed"
ROOT_INSTALL_SEED="$FIXTURE_ROOT/root-install-seed"
ROOT_HOME_SEED="$FIXTURE_ROOT/root-home-seed"

# fs_installpath is the application root. On macOS the VFS intentionally scans
# its bundle resource root, not <install>/base; mirroring the real product
# layout is required for the install-side duplicate to enter the search path.
INSTALL_RESOURCE="$BASEPATH"
case "$(uname -s)" in
  Darwin) INSTALL_RESOURCE="$BASEPATH/Contents/Resources" ;;
esac

rm -rf "$FIXTURE_ROOT"
mkdir -p "$INSTALL_RESOURCE/base" "$HOMEPATH/base/order" "$SEED" \
  "$PAK0_SEED/order" "$PAK8_SEED/order" \
  "$PAX01_SEED/order" "$PAX21_SEED/order" \
  "$SAME_PK3_SEED/order" "$SAME_SW3Z_SEED/order" \
  "$ROOT_INSTALL_SEED/order" "$ROOT_HOME_SEED/order"

# Minimal SW3Z content: needs default.cfg so FS_Restart's
# `FS_ReadFile("default.cfg")` post-check passes; otherwise engine
# Com_Terminates on TERM_UNRECOVERABLE before reaching +quit.
# The dedup code path runs BEFORE that check, so even a no-default.cfg
# fixture exercises the bug — but exit code wouldn't be 0.
echo "// regression fixture marker — empty default.cfg" > "$SEED/default.cfg"
echo "regression fixture for FS_DeduplicateArchives" > "$SEED/dummy.txt"
"$SW3Z_TOOL" a "$INSTALL_RESOURCE/base/regression_dup.sw3z" "$SEED" >/dev/null
cp "$INSTALL_RESOURCE/base/regression_dup.sw3z" "$HOMEPATH/base/regression_dup.sw3z"

# Quake 3-compatible global search order matrix. Archive names sort descending;
# equal basenames retain discovery priority (SW3Z over PK3, home over install),
# and archives remain ahead of loose files.
echo "echo pak0-loses" > "$PAK0_SEED/order/q3-pak.cfg"
( cd "$PAK0_SEED" && cmake -E tar cf \
  "$INSTALL_RESOURCE/base/pak0.pk3" --format=zip order/q3-pak.cfg )
echo "echo pak8-wins" > "$PAK8_SEED/order/q3-pak.cfg"
echo "echo pak-loses" > "$PAK8_SEED/order/pax-over-pak.cfg"
( cd "$PAK8_SEED" && cmake -E tar cf \
  "$INSTALL_RESOURCE/base/pak8.pk3" --format=zip \
  order/q3-pak.cfg order/pax-over-pak.cfg )
echo "echo pax01-loses" > "$PAX01_SEED/order/pax.cfg"
"$SW3Z_TOOL" a "$INSTALL_RESOURCE/base/pax01.sw3z" "$PAX01_SEED" >/dev/null
echo "echo pax21-wins" > "$PAX21_SEED/order/pax.cfg"
echo "echo pax-wins" > "$PAX21_SEED/order/pax-over-pak.cfg"
echo "echo archive-wins" > "$PAX21_SEED/order/archive-over-loose.cfg"
"$SW3Z_TOOL" a "$INSTALL_RESOURCE/base/pax21.sw3z" "$PAX21_SEED" >/dev/null
echo "echo pk3-loses" > "$SAME_PK3_SEED/order/container.cfg"
( cd "$SAME_PK3_SEED" && cmake -E tar cf \
  "$HOMEPATH/base/same.pk3" --format=zip order/container.cfg )
echo "echo sw3z-wins" > "$SAME_SW3Z_SEED/order/container.cfg"
"$SW3Z_TOOL" a "$HOMEPATH/base/same.sw3z" "$SAME_SW3Z_SEED" >/dev/null
echo "echo install-loses" > "$ROOT_INSTALL_SEED/order/root.cfg"
"$SW3Z_TOOL" a "$INSTALL_RESOURCE/base/zzroot.sw3z" \
  "$ROOT_INSTALL_SEED" >/dev/null
echo "echo home-wins" > "$ROOT_HOME_SEED/order/root.cfg"
"$SW3Z_TOOL" a "$HOMEPATH/base/zzroot.sw3z" "$ROOT_HOME_SEED" >/dev/null
echo "echo loose-loses" > "$HOMEPATH/base/order/archive-over-loose.cfg"

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
LOGFILE="$(mktemp -t q3now-fs-dedup)".log   # suffix after mktemp; see smoke.sh
trap "rm -f $LOGFILE" EXIT

echo "==> Running FS dedup smoke"
echo "    basepath: $BASEPATH_NATIVE"
echo "    homepath: $HOMEPATH_NATIVE"

set +e
"$DED" \
  +set fs_installpath "$BASEPATH_NATIVE" \
  +set fs_homepath "$HOMEPATH_NATIVE" \
  +set fs_debug 1 \
  +exec order/q3-pak.cfg \
  +exec order/pax.cfg \
  +exec order/pax-over-pak.cfg \
  +exec order/container.cfg \
  +exec order/root.cfg \
  +exec order/archive-over-loose.cfg \
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

# A fatal startup abort (Sys_Error / FATAL) must never pass — the exit-code
# check above usually catches it, but guard explicitly so a dead engine is
# never mistaken for a clean dedup run.
if grep -qE "Sys_Error|FATAL" "$LOGFILE"; then
  fail "fatal error (Sys_Error/FATAL) detected in output"
fi

# (a-extended) dedup log line MUST be present — proves we exercised the
# FS_DeduplicateArchives code path with an actual duplicate.
# Exact format from files.c: "FS_Precedence: removed N duplicate archives, M unique remain"
if ! grep -qE "FS_Precedence: removed [1-9][0-9]* duplicate archives" "$LOGFILE"; then
  fail "expected archive-deduplication log line not found"
fi

for expected in \
  "FS_FOpenFileRead: order/q3-pak.cfg.*pak8.pk3" \
  "FS_FOpenFileRead: order/pax.cfg.*pax21.sw3z" \
  "FS_FOpenFileRead: order/pax-over-pak.cfg.*pax21.sw3z" \
  "FS_FOpenFileRead: order/container.cfg.*same.sw3z" \
  "FS_FOpenFileRead: order/root.cfg.*homepath.*zzroot.sw3z" \
  "FS_FOpenFileRead: order/archive-over-loose.cfg.*pax21.sw3z"; do
  if ! grep -q "$expected" "$LOGFILE"; then
    fail "global archive precedence mismatch: $expected"
  fi
done

echo "PASS: FS global archive order/dedup — Q3 basename, container/root tie and archive precedence"
exit 0
