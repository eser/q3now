#!/usr/bin/env bash
# smoke.sh — headless smoke test for q3now headless server
#
# Launches wired-headless, loads arena1 with a bot for 300 ticks, and exits.
# Parses console output for hard VM errors.
#
# Usage:
#   tests/smoke.sh [path-to-wired-headless]
#
# Environment:
#   Q3DIR   path to game installation. Default: /Applications/q3now
#           Content paks (q3now: pax*.sw3z; legacy Q3A: pak0.pk3) live in
#           base/ — except on macOS, where Q3DIR is an .app bundle and data
#           lives in Contents/Resources/base/ per bundle conventions. This
#           mirrors the Makefile's Q3DATADIR split; keep the two in step.
#   Q3DATADIR  override the resolved data directory directly.
#
# Exit codes:
#   0  PASS — no VM errors detected
#   1  FAIL — VM_Create failure or ERROR: line in output
#   77 SKIP — no content paks (*.sw3z / pak0.pk3) available (asset-free CI environment)

set -euo pipefail

DED="${1:-wired-headless}"
Q3DIR="${Q3DIR:-/Applications/q3now}"

# Resolve the data directory. A macOS install is an .app bundle with data
# under Contents/Resources/base/, everything else uses base/ next to the
# binary. Detect by layout rather than by `uname` so an explicitly staged
# non-bundle tree still works on a Mac.
if [ -z "${Q3DATADIR:-}" ]; then
  if [ -d "$Q3DIR/Contents/Resources/base" ]; then
    Q3DATADIR="$Q3DIR/Contents/Resources/base"
  else
    Q3DATADIR="$Q3DIR/base"
  fi
fi

# Graceful skip: CI runners don't have content paks. q3now ships .sw3z
# content paks (pax*.sw3z); legacy Q3A used pak0.pk3. Accept either, and
# skip only when neither is present. compgen short-circuits cleanly under
# `set -e` (the `if` consumes its nonzero exit on no-match).
if ! compgen -G "$Q3DATADIR/pa[xk]*.sw3z" >/dev/null 2>&1 \
   && ! compgen -G "$Q3DATADIR/pak*.pk3" >/dev/null 2>&1; then
  echo "SKIP: no content paks (*.sw3z / pak0.pk3) found at $Q3DATADIR/ — skipping smoke test in asset-free environment"
  exit 77
fi

if ! command -v "$DED" >/dev/null 2>&1 && [ ! -x "$DED" ]; then
  echo "FAIL: headless server not found: $DED"
  exit 1
fi

LOGFILE=$(mktemp /tmp/q3now-smoke-XXXXXX.log)
trap "rm -f $LOGFILE" EXIT

echo "==> Starting smoke test: $DED +map arena1"
timeout 30 "$DED" \
  +set fs_installpath "$Q3DIR" \
  +set bot_enable 1 \
  +set g_gametype 0 \
  +set sv_maxclients 4 \
  +map arena1 \
  +addbot visor 1 \
  +wait 300 \
  +quit \
  2>&1 | tee "$LOGFILE" || true

# Hard failures: fatal aborts (Sys_Error / FATAL) and VM loader errors.
# Sys_Error / FATAL catch an early startup abort that exits before any VM
# work — otherwise a dead engine would be reported as PASS.
if grep -qE "Sys_Error|FATAL|^ERROR:|VM_Create.*failed|VM syscall error|trap_.*syscall" "$LOGFILE"; then
  echo "FAIL: fatal/VM errors detected in output:"
  grep -E "Sys_Error|FATAL|^ERROR:|VM_Create.*failed|VM syscall error|trap_.*syscall" "$LOGFILE"
  exit 1
fi

# A crash caught by the parent watchdog produces none of the above. Sys_RunWatchdog
# forks, and when the child dies on a signal it just prints a relaunch notice and
# starts another one — so a hard fault (a misaligned pthread_mutex_t faulting with
# SIGBUS on AArch64 was the real case) yields a log full of relaunch lines, zero
# error lines, and a clean exit status.
if grep -q "died on signal" "$LOGFILE"; then
  echo "FAIL: engine crashed and was relaunched by the watchdog:"
  grep -m 3 "died on signal" "$LOGFILE"
  exit 1
fi

# Positive evidence that the run actually happened. Absence of error lines proves
# nothing on its own: an engine that dies before producing any output has no error
# lines either. Require a marker the server can only print once it got there.
if ! grep -q "Server Initialization" "$LOGFILE"; then
  echo "FAIL: server never reached initialization — expected '------ Server Initialization ------'"
  echo "---- last 20 lines of output ----"
  tail -20 "$LOGFILE"
  exit 1
fi

echo "PASS: smoke test complete — server initialized, no fatal/VM errors"
exit 0
