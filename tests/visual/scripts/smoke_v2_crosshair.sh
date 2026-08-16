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
. "$ROOT/lib/wired_paths.sh"
PREVIEW_BASE="${PREVIEW_BASE:-$WIRED_BASE}"
ENGINE_BINARY="${ENGINE_BINARY:-$WIRED_BINARY}"
mkdir -p "$RESULTS_DIR"
SHOTS_DIR="$PREVIEW_BASE/screenshots"
mkdir -p "$SHOTS_DIR"
SHOT_MARKER="$(wired_shot_marker)"
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
set hud modern
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
  "$ENGINE_BINARY" +exec "$(basename "$CFG_PATH")" >/dev/null 2>&1 || true
) || true
NEW="$(wired_newest_shot "$SHOT_MARKER" "$SHOTS_DIR")" || NEW=""
if [ -z "$NEW" ]; then echo "no new screenshot" >&2; exit 1; fi
cp "$NEW" "$RESULTS_DIR/impl.png"
echo "impl: $RESULTS_DIR/impl.png"
