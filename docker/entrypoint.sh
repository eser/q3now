#!/bin/sh
set -e

# ── q3now dedicated server entrypoint ─────────────────────────────────────────
# Maps Docker environment variables to engine cvars.
# Unset variables are simply omitted — engine defaults apply.

ARGS="+set dedicated 2"
ARGS="$ARGS +set fs_basepath /opt/q3now"
ARGS="$ARGS +set fs_homepath /home/q3now"
ARGS="$ARGS +set sv_allowDownload 1"

# Server identity
[ -n "$Q3_HOSTNAME" ]     && ARGS="$ARGS +set sv_hostname \"$Q3_HOSTNAME\""
[ -n "$Q3_MAXCLIENTS" ]   && ARGS="$ARGS +set sv_maxclients $Q3_MAXCLIENTS"
[ -n "$Q3_RCONPASSWORD" ] && ARGS="$ARGS +set rconpassword \"$Q3_RCONPASSWORD\""

# Game rules
[ -n "$Q3_SCORELIMIT" ]   && ARGS="$ARGS +set g_scorelimit $Q3_SCORELIMIT"
[ -n "$Q3_TIMELIMIT" ]    && ARGS="$ARGS +set g_timelimit $Q3_TIMELIMIT"
[ -n "$Q3_GAMETYPE" ]     && ARGS="$ARGS +set g_gametype $Q3_GAMETYPE"
[ -n "$Q3_MAPROTATION" ]  && ARGS="$ARGS +set g_maprotation \"$Q3_MAPROTATION\""

# Engine tuning
[ -n "$Q3_HUNKMEGS" ]     && ARGS="$ARGS +set com_hunkmegs $Q3_HUNKMEGS"

# QUIC transport — always enabled
# Override TLS certificate and key paths via env vars (paths relative to fs_homepath).
[ -n "$Q3_QUIC_CERT" ]    && ARGS="$ARGS +set sv_quicCertFile \"$Q3_QUIC_CERT\""
[ -n "$Q3_QUIC_KEY" ]     && ARGS="$ARGS +set sv_quicKeyFile \"$Q3_QUIC_KEY\""

# ── Auto-generate self-signed TLS cert for QUIC if none is present ────────────
# The engine looks for certs/cert.pem and certs/key.pem under fs_homepath.
# If the operator didn't mount a real cert, generate a self-signed one so QUIC
# starts cleanly. Replace with a CA-signed cert for production deployments.
CERT_DIR="/home/q3now/certs"
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
        -subj "/CN=q3now-server" \
        2>/dev/null
    echo "q3now: generated self-signed TLS cert at $CERT_FILE (replace for production)"
fi

# Downloads
[ -n "$Q3_DOWNLOAD_URL" ] && ARGS="$ARGS +set sv_dlURL \"$Q3_DOWNLOAD_URL\""

# Config execution
[ -n "$Q3_EXEC" ]         && ARGS="$ARGS +exec $Q3_EXEC"

# Escape hatch: pass arbitrary engine arguments
[ -n "$Q3_EXTRA_ARGS" ]   && ARGS="$ARGS $Q3_EXTRA_ARGS"

# CMD args (e.g., +map q3dm17) are appended via "$@"
# exec replaces shell so the engine is PID 1 (receives SIGTERM on docker stop)
exec /opt/q3now/q3now-ded $ARGS "$@"
