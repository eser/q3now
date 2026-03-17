#!/usr/bin/env bash
# smoke.sh — headless smoke test for q3now dedicated server
#
# Launches q3now-ded, loads q3dm1 with a bot for 300 ticks, and exits.
# Parses console output for hard VM errors.
#
# Usage:
#   tests/smoke.sh [path-to-q3now-ded]
#
# Environment:
#   Q3DIR   path to game installation with baseq3/pak*.pk3 (default: /Applications/q3now)
#
# Exit codes:
#   0  PASS — no VM errors detected
#   1  FAIL — VM_Create failure or ERROR: line in output
#   77 SKIP — pak0.pk3 not available (asset-free CI environment)

set -euo pipefail

DED="${1:-q3now-ded}"
Q3DIR="${Q3DIR:-/Applications/q3now}"

# Graceful skip: CI runners don't have proprietary Q3A assets
if [ ! -f "$Q3DIR/baseq3/pak0.pk3" ]; then
  echo "SKIP: pak0.pk3 not found at $Q3DIR/baseq3/ — skipping smoke test in asset-free environment"
  exit 77
fi

if ! command -v "$DED" >/dev/null 2>&1 && [ ! -x "$DED" ]; then
  echo "FAIL: dedicated server not found: $DED"
  exit 1
fi

LOGFILE=$(mktemp /tmp/q3now-smoke-XXXXXX.log)
trap "rm -f $LOGFILE" EXIT

echo "==> Starting smoke test: $DED +map q3dm1"
timeout 30 "$DED" \
  +set fs_basepath "$Q3DIR" \
  +set dedicated 2 \
  +set bot_enable 1 \
  +set g_gametype 0 \
  +set sv_maxclients 4 \
  +map q3dm1 \
  +addbot sarge 1 \
  +wait 300 \
  +quit \
  2>&1 | tee "$LOGFILE" || true

# Hard failures: VM loader errors
if grep -qE "^ERROR:|VM_Create.*failed|VM syscall error|trap_.*syscall" "$LOGFILE"; then
  echo "FAIL: VM errors detected in output:"
  grep -E "^ERROR:|VM_Create.*failed|VM syscall error|trap_.*syscall" "$LOGFILE"
  exit 1
fi

echo "PASS: smoke test complete — no VM errors"
exit 0
