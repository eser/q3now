#!/usr/bin/env bash
# Dispatch 5.27 S3(b): kill_feed functional smoke.
#
# Scenario (documented & reproducible): arena1 deathmatch with g_forcerespawn so
# the local player auto-respawns to a live HUD. We deterministically generate
# real obituaries by suiciding the local player (`kill`) twice. Each `kill`
# drives an EV_OBITUARY event → cgame CG_Obituary (cg_event.c, self/suicide
# site) → name-resolved push "<player> <verb>" → the client
# WIRED_EVENT_OBITUARY ring (cl_wired_ui_hud_state.c) → hud.feed.<N>.text →
# the qw_hud_kill_feed widget rows. After the second forced respawn the player
# is alive again and frozen at the canonical V2 viewpoint (same controlled
# frame the deterministic layout capture uses), so the screenshot shows the
# normal in-game HUD with >=1 populated kill_feed row at top-left.
#
# Why suicide and not bots: arena1 ships no .aas nav data, so bots cannot
# navigate or frag reliably. Suicide exercises the identical obituary code path
# (EV_OBITUARY → CG_Obituary) with a guaranteed, reproducible result.
#
# PASS = cgame prints >=1 obituary to the console (proves the obituary path ran
# on the rebuilt cgame) AND the screenshot shows >=1 populated kill_feed row.
set -euo pipefail
ARTBOARD="${ARTBOARD:-v2_hud_active}"
MODE="${MODE:-dark}"
ACCENT="${ACCENT:-cyan}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
TIMESTAMP="${TIMESTAMP:-$(date +%Y%m%d_%H%M%S)}"
RESULTS_DIR="$ROOT/visual/results/${ARTBOARD}_${MODE}_${ACCENT}/${TIMESTAMP}_killscenario"
PREVIEW_BASE="${PREVIEW_BASE:-/c/Users/eser/wired/q3now-preview/base}"
ENGINE_EXE="${ENGINE_EXE:-$ROOT/../build/debug/wired.x64.exe}"
W="${WIDTH:-1440}"
H="${HEIGHT:-900}"
MAP="${MAP:-arena1}"

mkdir -p "$RESULTS_DIR"
SHOTS_DIR="$PREVIEW_BASE/screenshots"
mkdir -p "$SHOTS_DIR"
BEFORE=$(ls -t "$SHOTS_DIR" 2>/dev/null | head -1 || echo "")

CFG_PATH="$PREVIEW_BASE/dispatch_smoke_killfeed_${MODE}_${ACCENT}.cfg"
cat > "$CFG_PATH" <<EOF
set r_mode -1
set r_customwidth $W
set r_customheight $H
set r_fullscreen 0
set sv_pure 0
set sv_cheats 1
set attract_delay 0
set bot_minplayers 0
set g_gametype 0
set g_forcerespawn 2
set cg_drawCrosshair 1
set hud ${HUD:-qw_hud_active}
set ui_palette_mode $MODE
set ui_palette_accent $ACCENT
menu_reload
attract_restart
wait 500
map $MAP
wait 3000
kill
wait 3000
kill
wait 3000
setviewpos ${VIEWPOS:-216 1328 24 90}
wait 300
screenshot
wait 200
quit
EOF

(
  cd "$PREVIEW_BASE/.."
  "$ENGINE_EXE" +set fs_installpath "$PREVIEW_BASE/.." +set fs_homepath "$PREVIEW_BASE/.." +exec "$(basename "$CFG_PATH")" >/dev/null 2>&1 || true
) || true

AFTER=$(ls -t "$SHOTS_DIR" 2>/dev/null | head -5)
NEW=$(echo "$AFTER" | grep -v "^$BEFORE\$" | head -1)
if [ -z "$NEW" ]; then
  echo "no new screenshot found after boot" >&2
  exit 1
fi
cp "$SHOTS_DIR/$NEW" "$RESULTS_DIR/impl.png"
echo "impl: $RESULTS_DIR/impl.png"
