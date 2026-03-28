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
[ -n "$Q3_FRAGLIMIT" ]    && ARGS="$ARGS +set fraglimit $Q3_FRAGLIMIT"
[ -n "$Q3_TIMELIMIT" ]    && ARGS="$ARGS +set timelimit $Q3_TIMELIMIT"
[ -n "$Q3_GAMETYPE" ]     && ARGS="$ARGS +set g_gametype $Q3_GAMETYPE"
[ -n "$Q3_MAPROTATION" ]  && ARGS="$ARGS +set g_maprotation \"$Q3_MAPROTATION\""

# Engine tuning
[ -n "$Q3_HUNKMEGS" ]     && ARGS="$ARGS +set com_hunkmegs $Q3_HUNKMEGS"

# QUIC transport
[ -n "$Q3_QUIC" ]         && ARGS="$ARGS +set sv_quic $Q3_QUIC"

# Downloads
[ -n "$Q3_DOWNLOAD_URL" ] && ARGS="$ARGS +set sv_dlURL \"$Q3_DOWNLOAD_URL\""

# Config execution
[ -n "$Q3_EXEC" ]         && ARGS="$ARGS +exec $Q3_EXEC"

# Escape hatch: pass arbitrary engine arguments
[ -n "$Q3_EXTRA_ARGS" ]   && ARGS="$ARGS $Q3_EXTRA_ARGS"

# CMD args (e.g., +map q3dm17) are appended via "$@"
# exec replaces shell so the engine is PID 1 (receives SIGTERM on docker stop)
exec /opt/q3now/q3now-ded $ARGS "$@"
