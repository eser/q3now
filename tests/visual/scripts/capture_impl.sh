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
PREVIEW_BASE="${PREVIEW_BASE:-/c/Users/eser/wired/q3now-preview/base}"
ENGINE_EXE="${ENGINE_EXE:-$ROOT/../build/debug/wired.x64.exe}"
W="${WIDTH:-1440}"
H="${HEIGHT:-900}"

mkdir -p "$RESULTS_DIR"

SHOTS_DIR="$PREVIEW_BASE/screenshots"
mkdir -p "$SHOTS_DIR"
BEFORE=$(ls -t "$SHOTS_DIR" 2>/dev/null | head -1 || echo "")

CLAY_REL="dispatch5b_impl_clay_${ARTBOARD}_${MODE}_${ACCENT}_${TIMESTAMP}.json"

CFG_PATH="$PREVIEW_BASE/dispatch_visual_${ARTBOARD}_${MODE}_${ACCENT}.cfg"
cat > "$CFG_PATH" <<EOF
set r_mode -1
set r_customwidth $W
set r_customheight $H
set r_fullscreen 0
set attract_delay 0
attract_restart
wait 200
wui_test_keydown 32
wait 300
set ui_palette_mode $MODE
set ui_palette_accent $ACCENT
wait 400
wui_test_dump_clay $CLAY_REL
wait 100
screenshot
wait 200
quit
EOF

(
  cd "$PREVIEW_BASE/.."
  "$ENGINE_EXE" +set fs_installpath "$PREVIEW_BASE/.." +set fs_homepath "$PREVIEW_BASE/.." +exec "$(basename "$CFG_PATH")" >/dev/null 2>&1 || true
) || true

# Locate the new screenshot.
AFTER=$(ls -t "$SHOTS_DIR" 2>/dev/null | head -5)
NEW=$(echo "$AFTER" | grep -v "^$BEFORE\$" | head -1)
if [ -z "$NEW" ]; then
  echo "no new screenshot found after boot" >&2
  exit 1
fi
cp "$SHOTS_DIR/$NEW" "$RESULTS_DIR/impl.png"

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
