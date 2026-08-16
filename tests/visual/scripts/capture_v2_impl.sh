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
. "$ROOT/lib/wired_paths.sh"
PREVIEW_BASE="${PREVIEW_BASE:-$WIRED_BASE}"
ENGINE_BINARY="${ENGINE_BINARY:-$WIRED_BINARY}"
# Artboard-native — must stay in lockstep with regen_baseline.sh.
# 2026-08-16: the 1280x720 move was reverted; the artboard is fixed-pixel
# 1440x900 with overflow:hidden (qw-screens.jsx:1043), so a smaller root crops
# the HUD instead of reflowing it. See TASK-70.
W="${WIDTH:-1440}"
H="${HEIGHT:-900}"
MAP="${MAP:-arena1}"

mkdir -p "$RESULTS_DIR"

SHOTS_DIR="$PREVIEW_BASE/screenshots"
mkdir -p "$SHOTS_DIR"
SHOT_MARKER="$(wired_shot_marker)"

CFG_PATH="$PREVIEW_BASE/dispatch_visual_${ARTBOARD}_${MODE}_${ACCENT}.cfg"
cat > "$CFG_PATH" <<EOF
set r_mode -1
set r_customwidth $W
set r_customheight $H
set r_fullscreen 0
// Pin the CAPTURED pixel size to exactly WxH, independent of display density.
// Screenshots read vk.capture.image when it exists and the swapchain otherwise
// (tr_backend.c:2698-2699); vk.capture.image is supersample-gated (vk.c:17321)
// and is sized by gls.captureWidth/Height rather than by the window's physical
// pixels. Without this a 2.5x display returns 3600x2196 for a 1440x900 request
// and vcompare rejects it on size. All CVAR_LATCH, so vid_restart is required.
// Kept in lockstep with capture_impl.sh so both artboard families capture alike.
set r_fbo 1
set r_renderWidth $W
set r_renderHeight $H
set r_renderScale 1
set r_ext_supersample 1
vid_restart
wait 300
set sv_pure 0
set sv_cheats 1
set attract_delay 0
set bot_minplayers 0
set g_gametype 0
set cg_drawCrosshair 1
set cg_crosshairHealth 0
set cg_crosshairColor "0.957 0.627 0.227"
set cg_crosshairSize 48
// Hide the first-person weapon model for HUD scoring.
//
// The gun is drawn bottom-right, straight through the armor_panel and
// holdables_strip windows, and it is world geometry rather than HUD: it moves
// with view bob and weapon selection, so it injects large, non-deterministic
// structural noise into regions that are supposed to score the PANELS.
//
// Measured 2026-08-16 on an otherwise identical capture — the only change was
// this cvar:
//     armor_panel      0.2435 -> 0.3159  (threshold 0.26)  FAIL -> PASS
//     holdables_strip  0.2555 -> 0.4824  (threshold 0.36)  FAIL -> PASS
//     health_panel     0.2803 -> 0.2910  (left panel, gun-free either way)
// Both regions were previously diagnosed as panel defects ("clip-path corner
// missing", "player owns 0 holdables"). Neither was: the panels render fine,
// the gun was simply sitting on top of them.
set cg_drawGun 0
set hud ${HUD:-modern}
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
  "$ENGINE_BINARY" +exec "$(basename "$CFG_PATH")" >/dev/null 2>&1 || true
) || true

NEW="$(wired_newest_shot "$SHOT_MARKER" "$SHOTS_DIR")" || NEW=""
if [ -z "$NEW" ]; then
  echo "no new screenshot found after boot" >&2
  exit 1
fi
cp "$NEW" "$RESULTS_DIR/impl.png"

echo "impl: $RESULTS_DIR/impl.png"
