#!/usr/bin/env bash
# smoke-quic-game.sh — QUIC game transport smoke test for q3now
#
# Launches wired-headless with QUIC game transport enabled, verifies QUIC
# initialization and that the server accepts game connections without
# crashing.  No Q3A assets required (exits before map load if assets
# are missing, but still validates QUIC init + networking stack).
#
# Usage:
#   tests/smoke-quic-game.sh [path-to-wired-headless]
#
# Environment:
#   Q3DIR   path to game installation with base/pak*.pk3
#           (default: /Applications/q3now)
#
# Exit codes:
#   0  PASS — QUIC game transport initialized, no startup errors
#   1  FAIL — fatal error or QUIC failed to initialize
#   77 SKIP — headless server binary not found

set -euo pipefail
QUIC_SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=tests/lib/wired_paths.sh
. "$QUIC_SCRIPT_DIR/lib/wired_paths.sh"

DED="${1:-}"
# Install root from the shared helper, never a literal: the old default
# `/Applications/q3now` is not the installed bundle name (that is
# <PRODUCT_NAME><CHANNEL_SUFFIX>.app, CMakeLists.txt:35-40), so it named a
# directory that does not exist. See GAME-DATA.md §4.
Q3DIR="${Q3DIR:-$WIRED_INSTALL}"

# Locate binary: explicit arg wins, else the resolved install.
if [ -z "$DED" ] || { [ ! -x "$DED" ] && ! command -v "$DED" >/dev/null 2>&1; }; then
  if [ -n "${WIRED_BINARY_HEADLESS:-}" ] && [ -x "$WIRED_BINARY_HEADLESS" ]; then
    DED="$WIRED_BINARY_HEADLESS"
  else
    echo "SKIP: wired-headless not found (arg='${1:-}', install='$WIRED_BINDIR')"
    exit 77
  fi
fi

LOGFILE="$(mktemp /tmp/q3now-quic-game-XXXXXX)".log   # suffix after mktemp; see smoke.sh
trap "rm -f $LOGFILE" EXIT

# 🔴 DO NOT force fs_installpath. The previous version passed
# `+set fs_installpath $Q3DIR` unconditionally, including in its "asset-free"
# branch, on the stated assumption that "QUIC init happens before
# FS_InitFilesystem". That assumption is wrong: pointing fs_installpath at a
# directory with no pax21 makes the engine abort at
#   code/qcommon/wired/core/vfs/files.c — Com_Terminate("Couldn't load default.cfg")
# BEFORE the transport comes up, so the run proved nothing about QUIC. It is
# also the exact override GAME-DATA.md §4 warns against. Letting the engine
# resolve its own install path (Sys_Pwd from the binary's own directory) is
# both correct and the documented way to launch.
EXTRA_ARGS=()
if [ -n "${WIRED_MAP:-}" ]; then
  EXTRA_ARGS=( +map "$WIRED_MAP" +addbot visor 1 +wait 200 )
fi

# Launch with the binary's own directory as CWD so Sys_Pwd → fs_installpath
# finds the adjacent base/ — same convention as smoke-map-transition.sh.
DED_ABS="$(cd "$(dirname "$DED")" && pwd)/$(basename "$DED")"
DED_DIR="$(dirname "$DED_ABS")"
( cd "$DED_DIR" && timeout 15 "$DED_ABS" \
  +set sv_maxclients 8 \
  +set developer 1 \
  +set ttycon 0 \
  "${EXTRA_ARGS[@]}" \
  +quit ) 2>&1 | tee "$LOGFILE" || true

# Hard failures always fail regardless of assets
if grep -qE "^ERROR:|VM_Create.*failed|Sys_Error|FATAL|Segmentation fault|Illegal instruction" "$LOGFILE"; then
  echo "FAIL: Fatal error detected:"
  grep -E "^ERROR:|VM_Create.*failed|Sys_Error|FATAL|Segmentation fault|Illegal instruction" "$LOGFILE"
  exit 1
fi

# QUIC bring-up must be PROVEN, not assumed.
#
# 🔴 The previous version ended in an `else` branch that printed
# "PASS: Server started without QUIC errors (QUIC may log at higher verbosity)"
# whenever none of its patterns matched. That is a fail-OPEN verdict: a server
# where the transport never came up logs no QUIC error either, so a completely
# dead transport passed. It was in fact taking that branch on every run here —
# the loose patterns ("QUIC.*init", "QUIC.*listen") do not match the wording the
# engine actually emits, so the smoke had been vacuous rather than green.
#
# The engine prints exactly one line that is reachable ONLY after
# picoquic_create() succeeded, the demux registered and wn.initialized was set:
#   code/qcommon/wired/net/wn_main.c:582
#     "WiredNet: listening on port %d (IPv4), ALPN: %s, max clients: %d, ..."
# Requiring that literal makes the absence of the transport a FAILURE.
QUIC_READY_RE='WiredNet: listening on port [0-9]+ \(IPv4\), ALPN:'
if grep -qiE "QUIC.*failed|QUIC.*error|picoquic.*error" "$LOGFILE"; then
  echo "FAIL: QUIC initialization error:"
  grep -iE "QUIC.*failed|QUIC.*error|picoquic.*error" "$LOGFILE"
  exit 1
fi
if ! grep -qE "$QUIC_READY_RE" "$LOGFILE"; then
  echo "FAIL: QUIC game transport never reached the listening state."
  echo "      Expected a line matching: $QUIC_READY_RE"
  echo "      (emitted at code/qcommon/wired/net/wn_main.c:582, only after"
  echo "       picoquic_create succeeded and the transport was published)."
  echo "---- last 20 lines of output ----"
  tail -20 "$LOGFILE"
  exit 1
fi
echo "PASS: QUIC game transport reached the listening state"
grep -E "$QUIC_READY_RE" "$LOGFILE" | head -3

# Dual-stack check: verify UDP is NOT accepted when net_transport=quic
# (look for netchan setup log which would indicate UDP path was used)
if grep -qE "Netchan_Setup.*NA_IP\b" "$LOGFILE"; then
  echo "WARN: UDP netchan setup seen — expected QUIC-only mode"
fi

echo "==> QUIC game transport smoke test complete"
exit 0
