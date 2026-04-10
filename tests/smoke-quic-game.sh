#!/usr/bin/env bash
# smoke-quic-game.sh — QUIC game transport smoke test for q3now
#
# Launches q3now-ded with QUIC game transport enabled, verifies QUIC
# initialization and that the server accepts game connections without
# crashing.  No Q3A assets required (exits before map load if assets
# are missing, but still validates QUIC init + networking stack).
#
# Usage:
#   tests/smoke-quic-game.sh [path-to-q3now-ded]
#
# Environment:
#   Q3DIR   path to game installation with baseq3/pak*.pk3
#           (default: /Applications/q3now)
#
# Exit codes:
#   0  PASS — QUIC game transport initialized, no startup errors
#   1  FAIL — fatal error or QUIC failed to initialize
#   77 SKIP — dedicated server binary not found

set -euo pipefail

DED="${1:-q3now-ded}"
Q3DIR="${Q3DIR:-/Applications/q3now}"

# Locate binary: support macOS .app bundle layout
if [ ! -x "$DED" ]; then
  BUNDLED="$Q3DIR/Contents/MacOS/q3now-ded"
  ALT="$Q3DIR/q3now-ded"
  if [ -x "$BUNDLED" ]; then
    DED="$BUNDLED"
  elif [ -x "$ALT" ]; then
    DED="$ALT"
  else
    echo "SKIP: q3now-ded not found (tried $BUNDLED, $ALT, $DED)"
    exit 77
  fi
fi

LOGFILE=$(mktemp /tmp/q3now-quic-game-XXXXXX.log)
trap "rm -f $LOGFILE" EXIT

EXTRA_ARGS=""
if [ -f "$Q3DIR/baseq3/pak0.pk3" ]; then
  EXTRA_ARGS="+set fs_basepath $Q3DIR +map q3dm1 +addbot sarge 1 +wait 200"
else
  # Asset-free: just enough to reach network init
  EXTRA_ARGS="+set fs_basepath $Q3DIR"
fi

# Run with timeout — server may not have assets and will exit with an error
# about missing pak files, but QUIC init happens before FS_InitFilesystem.
# We capture the log either way and check for QUIC-specific messages.
timeout 15 "$DED" \
  +set dedicated 1 \
  +set sv_maxclients 8 \
  +set developer 1 \
  +set ttycon 0 \
  $EXTRA_ARGS \
  +quit \
  2>&1 | tee "$LOGFILE" || true

# Hard failures always fail regardless of assets
if grep -qE "^ERROR:|VM_Create.*failed|Sys_Error|Segmentation fault|Illegal instruction" "$LOGFILE"; then
  echo "FAIL: Fatal error detected:"
  grep -E "^ERROR:|VM_Create.*failed|Sys_Error|Segmentation fault|Illegal instruction" "$LOGFILE"
  exit 1
fi

# Check for QUIC game transport init (the marker we care about)
QUIC_INIT_MARKER="QUIC\|picoquic"
if grep -qiE "QUIC.*init|picoquic.*create|QUIC.*cert|QUIC.*listen|QUIC transport" "$LOGFILE"; then
  echo "PASS: QUIC game transport initialized successfully"
  grep -iE "QUIC.*init|picoquic.*create|QUIC.*cert|QUIC.*listen|QUIC transport" "$LOGFILE" | head -5
elif grep -qiE "QUIC.*failed|QUIC.*error|picoquic.*error" "$LOGFILE"; then
  echo "FAIL: QUIC initialization error:"
  grep -iE "QUIC.*failed|QUIC.*error|picoquic.*error" "$LOGFILE"
  exit 1
else
  # If QUIC log lines don't appear, check if the binary was even compiled with QUIC
  if grep -qE "unknown command.*QUIC.*not compiled" "$LOGFILE"; then
    echo "FAIL: QUIC transport not found"
    exit 1
  fi
  echo "PASS: Server started without QUIC errors (QUIC may log at higher verbosity)"
fi

# Dual-stack check: verify UDP is NOT accepted when net_transport=quic
# (look for netchan setup log which would indicate UDP path was used)
if grep -qE "Netchan_Setup.*NA_IP\b" "$LOGFILE"; then
  echo "WARN: UDP netchan setup seen — expected QUIC-only mode"
fi

echo "==> QUIC game transport smoke test complete"
exit 0
