#!/usr/bin/env bash
# Dispatch 5.19 S5: V2_HUD_Active engine capture. Loads a representative
# arena map (arena1 canonical) via singleplayer/devmap, spawns the
# player, waits for HUD state to stabilize (health/armor populated,
# weapon selected), sets ui_palette_mode + ui_palette_accent, screenshots.
# Mirrors V1 capture_impl.sh pattern but adapted for in-game context.
set -euo pipefail
ARTBOARD="${ARTBOARD:-v2_hud_active}"
MODE="${MODE:-dark}"
ACCENT="${ACCENT:-amber}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
TIMESTAMP="${TIMESTAMP:-$(date +%Y%m%d_%H%M%S)}"
RESULTS_DIR="$ROOT/visual/results/${ARTBOARD}_${MODE}_${ACCENT}/$TIMESTAMP"
PREVIEW_BASE="${PREVIEW_BASE:-/c/Users/eser/wired/q3now-preview/base}"
ENGINE_EXE="${ENGINE_EXE:-$ROOT/../build/debug/wired.x64.exe}"
W="${WIDTH:-1440}"
H="${HEIGHT:-900}"
MAP="${MAP:-arena1}"

mkdir -p "$RESULTS_DIR"

SHOTS_DIR="$PREVIEW_BASE/screenshots"
mkdir -p "$SHOTS_DIR"
BEFORE=$(ls -t "$SHOTS_DIR" 2>/dev/null | head -1 || echo "")

CFG_PATH="$PREVIEW_BASE/dispatch_visual_${ARTBOARD}_${MODE}_${ACCENT}.cfg"
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
set cg_drawCrosshair 1
set cg_crosshairHealth 0
set cg_crosshairColor "0.957 0.627 0.227"
set cg_crosshairSize 48
set hud ${HUD:-qw_hud_active}
set ui_palette_mode $MODE
set ui_palette_accent $ACCENT
menu_reload
attract_restart
wait 500
map $MAP
wait 3000
setviewpos ${VIEWPOS:-216 1328 24 90}
wait 500
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
