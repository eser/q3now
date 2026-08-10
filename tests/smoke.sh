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
#   Q3DIR   path to game installation with content paks in base/
#           (q3now: pax*.sw3z; legacy Q3A: pak0.pk3). Default: /Applications/q3now
#
# Exit codes:
#   0  PASS — no VM errors detected
#   1  FAIL — VM_Create failure or ERROR: line in output
#   77 SKIP — no content paks (*.sw3z / pak0.pk3) available (asset-free CI environment)

set -euo pipefail

DED="${1:-wired-headless}"
Q3DIR="${Q3DIR:-/Applications/q3now}"

# Graceful skip: CI runners don't have content paks. q3now ships .sw3z
# content paks (pax*.sw3z); legacy Q3A used pak0.pk3. Accept either, and
# skip only when neither is present. compgen short-circuits cleanly under
# `set -e` (the `if` consumes its nonzero exit on no-match).
if ! compgen -G "$Q3DIR/base/pa[xk]*.sw3z" >/dev/null 2>&1 \
   && ! compgen -G "$Q3DIR/base/pak*.pk3" >/dev/null 2>&1; then
  echo "SKIP: no content paks (*.sw3z / pak0.pk3) found at $Q3DIR/base/ — skipping smoke test in asset-free environment"
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

echo "PASS: smoke test complete — no VM errors"
exit 0
