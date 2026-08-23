#!/usr/bin/env bash
# Boots the engine at the requested artboard state, captures both the
# screenshot (`screenshot`) and the Clay layout tree (`wui_test_dump_clay`),
# then copies both artifacts into the per-run results dir as impl.png +
# impl_clay.json. Both feed vcompare.
set -euo pipefail
ARTBOARD="${ARTBOARD:?ARTBOARD required}"
MODE="${MODE:-dark}"
ACCENT="${ACCENT:-amber}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
TIMESTAMP="${TIMESTAMP:-$(date +%Y%m%d_%H%M%S)}"
RESULTS_DIR="$ROOT/visual/results/${ARTBOARD}_${MODE}_${ACCENT}/$TIMESTAMP"
. "$ROOT/lib/wired_paths.sh"
PREVIEW_BASE="${PREVIEW_BASE:-$WIRED_BASE}"
ENGINE_BINARY="${ENGINE_BINARY:-$WIRED_BINARY}"
# Artboard-native capture resolution — must match the baseline exactly.
#
# 2026-08-16: this was briefly moved to 1280x720 on the premise that the mockup
# was resolution-independent. That premise was WRONG and the change is reverted.
# Only the backdrop <svg> uses viewBox + width:'100%'; the artboard container
# itself is fixed pixels with overflow:hidden — qw-screens.jsx:1043 reads
# `width:1440, height:900, overflow:'hidden'`, and every HUD element is
# absolutely positioned against that box (bottom:36, right:24, ...). Shrinking
# the root does not reflow the layout, it CROPS it: at 1280x720 the health and
# armor panels, all 9 weapon-carousel entries, the telemetry row and the ammo
# value fall outside the frame entirely (measured: ammo region drops from 741
# distinct colours to 34, with zero accent pixels).
#
# Reconciling this 16:10 artboard with other capture targets remains an open
# visual-test design decision — see TASK-70.
W="${WIDTH:-1440}"
H="${HEIGHT:-900}"

mkdir -p "$RESULTS_DIR"

SHOTS_DIR="$PREVIEW_BASE/screenshots"
mkdir -p "$SHOTS_DIR"
SHOT_MARKER="$(wired_shot_marker)"

CLAY_REL="dispatch5b_impl_clay_${ARTBOARD}_${MODE}_${ACCENT}_${TIMESTAMP}.json"

CFG_PATH="$PREVIEW_BASE/dispatch_visual_${ARTBOARD}_${MODE}_${ACCENT}.cfg"
cat > "$CFG_PATH" <<EOF
set r_mode -1
set r_customwidth $W
set r_customheight $H
set r_fullscreen 0
// Pin the CAPTURED pixel size to exactly WxH.
//
// r_customwidth/height are LOGICAL (point) dimensions. On a HiDPI display the
// window is backed at full physical density — sdl_glimp.c:233 always sets
// SDL_WINDOW_HIGH_PIXEL_DENSITY, correctly, so gameplay renders at native
// resolution — and a plain swapchain readback then yields the scaled size
// (measured on a 2.5x display: 1440x900 requested, 3600x2196 captured), which
// vcompare rejects against a fixed-pixel 1440x900 baseline.
//
// The screenshot path reads vk.capture.image when that image exists, and the
// swapchain otherwise (tr_backend.c:2698-2699). vk.capture.image is
// SUPERSAMPLE-GATED (vk.c:17321-17324 says so outright), so r_ext_supersample
// is what decides which buffer the capture comes from — without it the readback
// is the swapchain and inherits the display's scale factor.
//
// gls.captureWidth/Height are pinned to glConfig.vid* BEFORE the supersample
// doubling (tr_init.c:699-709), and r_renderScale overrides glConfig.vid* with
// r_renderWidth/Height (tr_init.c:693-696). Together they fix the captured
// image at exactly WxH regardless of display density: the same bytes on a
// Retina Mac and a 1x Windows box.
//
// r_renderScale 1 = nearest, no resampling — a filtered mode would blur the diff.
// All three are CVAR_LATCH (tr_init.c:2396, 2407-2415), so the vid_restart is
// required, not decorative. r_fbo 1 is a documented prerequisite of
// r_ext_supersample and is the default, set explicitly so a stale user config
// cannot silently disable the whole path.
set r_fbo 1
set r_renderWidth $W
set r_renderHeight $H
set r_renderScale 1
set r_ext_supersample 1
vid_restart
wait 300
set attract_delay 0
attract_restart
wait 200
// Open the main menu.
//
// `wui_test_keydown 32` does NOT do this. Attract is a timed reel of WiredUI
// panels and the menu composites ON TOP of it (cl_wired_ui_compositor layer
// order: error dialog > menu panels > attract content > game viewport). A
// keypress just advances the reel — measured 2026-08-16: one press showed the
// "QUAKE WIRED" slide, two showed "TOP TIMES", never the menu. So every
// v1_monolith run compared an ATTRACT SLIDE against a MENU baseline and failed
// all 13 regions (global SSIM 0.1857) for want of the right scene.
//
// `attract_stop` is also wrong here: it halts the reel without pushing a menu,
// leaving a black frame (measured: 0.0% ink).
wui_push main
wait 600
set ui_palette_mode $MODE
set ui_palette_accent $ACCENT
wait 400
wui_test_dump_clay $CLAY_REL
// 🔴 wait 100 here produced a fully BLACK screenshot, every time, at the right
// dimensions — the single most misleading failure in this harness, because a
// gate that only checks size blesses it. Bisected 2026-08-16: identical config,
// only this delay varied — 100ms black, 200ms and 400ms populated. The dump
// walks the Clay tree and costs at least one frame; screenshotting too soon
// grabs a target that has not been drawn into yet. 400ms is 2x the measured
// threshold. tests/visual-supersample-parity.sh guards the blank-frame class.
wait 400
screenshot
wait 200
quit
EOF

(
  cd "$PREVIEW_BASE/.."
  "$ENGINE_BINARY" +exec "$(basename "$CFG_PATH")" >/dev/null 2>&1 || true
) || true

# Locate the new screenshot.
NEW="$(wired_newest_shot "$SHOT_MARKER" "$SHOTS_DIR")" || NEW=""
if [ -z "$NEW" ]; then
  echo "no new screenshot found after boot" >&2
  exit 1
fi
cp "$NEW" "$RESULTS_DIR/impl.png"

# Locate the Clay dump. FS_FOpenFileWrite writes under fs_homepath/base (or
# fs_installpath/base when home == base). Check both.
CLAY_OUT_1="$PREVIEW_BASE/$CLAY_REL"
CLAY_OUT_2="$PREVIEW_BASE/../base/$CLAY_REL"
if [ -s "$CLAY_OUT_1" ]; then
  cp "$CLAY_OUT_1" "$RESULTS_DIR/impl_clay.json"
elif [ -s "$CLAY_OUT_2" ]; then
  cp "$CLAY_OUT_2" "$RESULTS_DIR/impl_clay.json"
else
  echo "clay dump missing — wui_test_dump_clay may not be _DEBUG-gated build, or write path differs: $CLAY_REL" >&2
fi

echo "impl: $RESULTS_DIR/impl.png"
[ -f "$RESULTS_DIR/impl_clay.json" ] && echo "clay: $RESULTS_DIR/impl_clay.json"
