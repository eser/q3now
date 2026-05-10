#!/bin/bash
# run-quic-server.sh — 12-factor dedicated server for QUIC testing
#
# Usage:
#   ./tests/run-quic-server.sh
#
# All configuration via environment variables (WIRED_* prefix).
# Override any variable before running:
#   WIRED_MAP=q3dm6 WIRED_SV_MAXCLIENTS=4 ./tests/run-quic-server.sh

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$PROJECT_DIR/build/quic-test"
BINARY="$BUILD_DIR/wired-ded.arm64.app/Contents/MacOS/wired-ded.arm64"
ASSETS_DIR="${WIRED_BASEPATH:-/Users/eser/q3now/old-q3}"

if [ ! -f "$BINARY" ]; then
    echo "ERROR: Binary not found at $BINARY"
    echo "Build first: cd build/quic-test && cmake ../.. -DUSE_WASM=OFF && make -j$(sysctl -n hw.ncpu)"
    exit 1
fi

if [ ! -d "$ASSETS_DIR/baseq3" ]; then
    echo "ERROR: Game assets not found at $ASSETS_DIR/baseq3"
    echo "Set WIRED_BASEPATH to the directory containing baseq3/pak0.pk3"
    exit 1
fi

# ── 12-factor defaults ────────────────────────────────────────────
export WIRED_SV_HOSTNAME="${WIRED_SV_HOSTNAME:-q3now QUIC Test Server}"
export WIRED_SV_MAXCLIENTS="${WIRED_SV_MAXCLIENTS:-8}"
export WIRED_G_GAMETYPE="${WIRED_G_GAMETYPE:-0}"
export WIRED_SV_QUIC="${WIRED_SV_QUIC:-1}"
export WIRED_SV_QUICAUTHTOKEN="${WIRED_SV_QUICAUTHTOKEN:-observer:member:user:testtoken,observer:leader:admin:admintoken}"
export WIRED_SV_QUICMAXCLIENTS="${WIRED_SV_QUICMAXCLIENTS:-8}"
export WIRED_MAP="${WIRED_MAP:-q3dm17}"
export WIRED_DEDICATED="${WIRED_DEDICATED:-1}"
export WIRED_NET_PORT="${WIRED_NET_PORT:-27960}"
export WIRED_COM_HUNKMEGS="${WIRED_COM_HUNKMEGS:-128}"

echo "════════════════════════════════════════════════════════════"
echo " q3now Dedicated Server (QUIC Transport Enabled)"
echo "════════════════════════════════════════════════════════════"
echo " Hostname:    $WIRED_SV_HOSTNAME"
echo " Map:         $WIRED_MAP"
echo " Port:        $WIRED_NET_PORT"
echo " QUIC:        $WIRED_SV_QUIC"
echo " Auth tokens: $(echo $WIRED_SV_QUICAUTHTOKEN | tr ',' '\n' | wc -l | tr -d ' ') configured"
echo " Assets:      $ASSETS_DIR"
echo "════════════════════════════════════════════════════════════"

exec "$BINARY" \
    +set fs_installpath "$ASSETS_DIR" \
    +set fs_homepath "$PROJECT_DIR" \
    +set dedicated 1 \
    +set com_hunkmegs "${WIRED_COM_HUNKMEGS}" \
    +set net_port "${WIRED_NET_PORT}" \
    "$@"
