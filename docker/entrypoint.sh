#!/bin/sh
set -e

# ── q3now dedicated server entrypoint ─────────────────────────────────────────
# Maps Docker environment variables to engine cvars.
# Unset variables are simply omitted — engine defaults apply.

ARGS="+set dedicated 2"
ARGS="$ARGS +set fs_installpath /opt/wired"
ARGS="$ARGS +set fs_homepath /home/wired"
ARGS="$ARGS +set sv_allowDownload 1"

# Server identity
[ -n "$WIRED_HOSTNAME" ]     && ARGS="$ARGS +set sv_hostname \"$WIRED_HOSTNAME\""
[ -n "$WIRED_MAXCLIENTS" ]   && ARGS="$ARGS +set sv_maxclients $WIRED_MAXCLIENTS"
[ -n "$WIRED_RCONPASSWORD" ] && ARGS="$ARGS +set rconpassword \"$WIRED_RCONPASSWORD\""

# Game rules
[ -n "$WIRED_SCORELIMIT" ]   && ARGS="$ARGS +set g_scorelimit $WIRED_SCORELIMIT"
[ -n "$WIRED_TIMELIMIT" ]    && ARGS="$ARGS +set g_timelimit $WIRED_TIMELIMIT"
[ -n "$WIRED_GAMETYPE" ]     && ARGS="$ARGS +set g_gametype $WIRED_GAMETYPE"
[ -n "$WIRED_MAPROTATION" ]  && ARGS="$ARGS +set g_maprotation \"$WIRED_MAPROTATION\""

# Engine tuning
[ -n "$WIRED_HUNKMEGS" ]     && ARGS="$ARGS +set com_hunkmegs $WIRED_HUNKMEGS"

# QUIC transport — always enabled
# Override TLS certificate and key paths via env vars (paths relative to fs_homepath).
[ -n "$WIRED_QUIC_CERT" ]    && ARGS="$ARGS +set sv_quicCertFile \"$WIRED_QUIC_CERT\""
[ -n "$WIRED_QUIC_KEY" ]     && ARGS="$ARGS +set sv_quicKeyFile \"$WIRED_QUIC_KEY\""

# ── Auto-generate self-signed TLS cert for QUIC if none is present ────────────
# The engine looks for certs/cert.pem and certs/key.pem under fs_homepath.
# If the operator didn't mount a real cert, generate a self-signed one so QUIC
# starts cleanly. Replace with a CA-signed cert for production deployments.
CERT_DIR="/home/wired/certs"
CERT_FILE="$CERT_DIR/cert.pem"
KEY_FILE="$CERT_DIR/key.pem"
if [ ! -f "$CERT_FILE" ] || [ ! -f "$KEY_FILE" ]; then
    mkdir -p "$CERT_DIR"
    openssl req -x509 -newkey ec \
        -pkeyopt ec_paramgen_curve:P-256 \
        -keyout "$KEY_FILE" \
        -out "$CERT_FILE" \
        -days 3650 \
        -nodes \
        -subj "/CN=wired-server" \
        2>/dev/null
    echo "wired: generated self-signed TLS cert at $CERT_FILE (replace for production)"
fi

# Downloads
[ -n "$WIRED_DOWNLOAD_URL" ] && ARGS="$ARGS +set sv_dlURL \"$WIRED_DOWNLOAD_URL\""

# Config execution
[ -n "$WIRED_EXEC" ]         && ARGS="$ARGS +exec $WIRED_EXEC"

# Escape hatch: pass arbitrary engine arguments
[ -n "$WIRED_EXTRA_ARGS" ]   && ARGS="$ARGS $WIRED_EXTRA_ARGS"

# CMD args (e.g., +map q3dm17) are appended via "$@"
# exec replaces shell so the engine is PID 1 (receives SIGTERM on docker stop)
exec /opt/wired/wired-ded $ARGS "$@"
