#!/usr/bin/env bash
# Dispatch 5.30 S2: crosshair live-play functional smoke.
# Boots arena1 and screenshots the LIVE player view (NO setviewpos) so the
# crosshair draws (the frozen setviewpos capture state suppresses it). Confirms
# the 5.29 S2 filled asset + S3 cg_crosshairColor amber actually render.
set -euo pipefail
MODE="${MODE:-dark}"
ACCENT="${ACCENT:-cyan}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
TIMESTAMP="${TIMESTAMP:-$(date +%Y%m%d_%H%M%S)}"
RESULTS_DIR="$ROOT/visual/results/v2_hud_active_${MODE}_${ACCENT}/${TIMESTAMP}_xhairsmoke"
PREVIEW_BASE="${PREVIEW_BASE:-/c/Users/eser/wired/q3now-preview/base}"
ENGINE_EXE="${ENGINE_EXE:-$ROOT/../build/debug/wired.x64.exe}"
mkdir -p "$RESULTS_DIR"
SHOTS_DIR="$PREVIEW_BASE/screenshots"
mkdir -p "$SHOTS_DIR"
BEFORE=$(ls -t "$SHOTS_DIR" 2>/dev/null | head -1 || echo "")
CFG_PATH="$PREVIEW_BASE/dispatch_smoke_xhair_${MODE}_${ACCENT}.cfg"
cat > "$CFG_PATH" <<EOF
set r_mode -1
set r_customwidth 1440
set r_customheight 900
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
set hud qw_hud_active
set ui_palette_mode $MODE
set ui_palette_accent $ACCENT
menu_reload
wait 500
devmap arena1
wait 3500
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
if [ -z "$NEW" ]; then echo "no new screenshot" >&2; exit 1; fi
cp "$SHOTS_DIR/$NEW" "$RESULTS_DIR/impl.png"
echo "impl: $RESULTS_DIR/impl.png"
