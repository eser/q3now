#!/bin/bash
# run-quic-server.sh — 12-factor dedicated server for QUIC testing
#
# Usage:
#   ./tests/run-quic-server.sh
#
# All configuration via environment variables (Q3_* prefix).
# Override any variable before running:
#   Q3_MAP=q3dm6 Q3_SV_MAXCLIENTS=4 ./tests/run-quic-server.sh

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$PROJECT_DIR/build/quic-test"
BINARY="$BUILD_DIR/q3now-ded.arm64.app/Contents/MacOS/q3now-ded.arm64"
ASSETS_DIR="${Q3_BASEPATH:-/Users/eser/q3now/old-q3}"

if [ ! -f "$BINARY" ]; then
    echo "ERROR: Binary not found at $BINARY"
    echo "Build first: cd build/quic-test && cmake ../.. -DUSE_WASM=OFF && make -j$(sysctl -n hw.ncpu)"
    exit 1
fi

if [ ! -d "$ASSETS_DIR/baseq3" ]; then
    echo "ERROR: Game assets not found at $ASSETS_DIR/baseq3"
    echo "Set Q3_BASEPATH to the directory containing baseq3/pak0.pk3"
    exit 1
fi

# ── 12-factor defaults ────────────────────────────────────────────
export Q3_SV_HOSTNAME="${Q3_SV_HOSTNAME:-q3now QUIC Test Server}"
export Q3_SV_MAXCLIENTS="${Q3_SV_MAXCLIENTS:-8}"
export Q3_G_GAMETYPE="${Q3_G_GAMETYPE:-0}"
export Q3_SV_QUIC="${Q3_SV_QUIC:-1}"
export Q3_SV_QUICAUTHTOKEN="${Q3_SV_QUICAUTHTOKEN:-observer:member:user:testtoken,observer:leader:admin:admintoken}"
export Q3_SV_QUICMAXCLIENTS="${Q3_SV_QUICMAXCLIENTS:-8}"
export Q3_MAP="${Q3_MAP:-q3dm17}"
export Q3_DEDICATED="${Q3_DEDICATED:-1}"
export Q3_NET_PORT="${Q3_NET_PORT:-27960}"
export Q3_COM_HUNKMEGS="${Q3_COM_HUNKMEGS:-128}"

echo "════════════════════════════════════════════════════════════"
echo " q3now Dedicated Server (QUIC Transport Enabled)"
echo "════════════════════════════════════════════════════════════"
echo " Hostname:    $Q3_SV_HOSTNAME"
echo " Map:         $Q3_MAP"
echo " Port:        $Q3_NET_PORT"
echo " QUIC:        $Q3_SV_QUIC"
echo " Auth tokens: $(echo $Q3_SV_QUICAUTHTOKEN | tr ',' '\n' | wc -l | tr -d ' ') configured"
echo " Assets:      $ASSETS_DIR"
echo "════════════════════════════════════════════════════════════"

exec "$BINARY" \
    +set fs_installpath "$ASSETS_DIR" \
    +set fs_homepath "$PROJECT_DIR" \
    +set dedicated 1 \
    +set com_hunkmegs "${Q3_COM_HUNKMEGS}" \
    +set net_port "${Q3_NET_PORT}" \
    "$@"
