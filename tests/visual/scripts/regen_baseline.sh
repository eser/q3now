#!/usr/bin/env bash
# Renders the React mockup for ARTBOARD/MODE/ACCENT into baselines/:
#   - <artboard>_<mode>_<accent>.png      via --screenshot
#   - <artboard>_<mode>_<accent>_dom.json via --dump-dom + script-tag extraction
# Both inputs feed vcompare; the render resolution must match capture_impl.sh
# exactly or every comparison is meaningless.
#
# !!! WARNING — REGENERATING BASELINES INVALIDATES THE CALIBRATED THRESHOLDS !!!
# The per-region ssim/deltaE thresholds in tests/visual/regions/*.json were all
# measured against 1440x900 (16:10) captures. Re-rendering baselines at any
# other resolution makes those numbers stale: they no longer describe the
# cross-renderer floor of the new baseline/impl pair. After a regen the
# thresholds MUST be re-measured and re-calibrated (measure run -> per-region
# floor -> recalibrate) before any PASS from this gate can be trusted. A PASS
# against freshly regenerated baselines with the old thresholds is not
# evidence — treat it as UNCALIBRATED.
set -euo pipefail
ARTBOARD="${ARTBOARD:?ARTBOARD required}"
MODE="${MODE:-dark}"
ACCENT="${ACCENT:-amber}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
HARNESS="$ROOT/visual/scripts/artboard_${ARTBOARD}.html"
OUT_PNG="$ROOT/visual/baselines/${ARTBOARD}_${MODE}_${ACCENT}.png"
OUT_DOM="$ROOT/visual/baselines/${ARTBOARD}_${MODE}_${ACCENT}_dom.json"
# Artboard-native render resolution — must match capture_impl.sh exactly.
#
# 2026-08-16: briefly moved to 1280x720 on the false premise that the mockup was
# resolution-independent; reverted. The artboard container is fixed pixels with
# overflow:hidden (qw-screens.jsx:1043 `width:1440, height:900`), so a smaller
# root crops the HUD rather than reflowing it. Reconciling this 16:10 artboard
# with other capture targets remains open — see TASK-70.
W="${WIDTH:-1440}"
H="${HEIGHT:-900}"
. "$(cd "$(dirname "$0")/../.." && pwd)/lib/wired_paths.sh"
CHROME="${CHROME:-$(wired_find_chrome)}"

[ -f "$HARNESS" ] || { echo "harness not found: $HARNESS" >&2; exit 2; }
mkdir -p "$(dirname "$OUT_PNG")"

URL="file://$HARNESS?mode=$MODE&accent=$ACCENT"

# Single Chrome invocation: --screenshot writes PNG to file; --dump-dom writes
# serialized HTML to stdout. Both produced in one render pass.
DOM_HTML="$(mktemp "$WIRED_TMP/qw_dom_html.XXXXXX")"
"$CHROME" --headless=new --disable-gpu --no-sandbox --hide-scrollbars \
  --window-size="$W,$H" --virtual-time-budget=30000 --allow-file-access-from-files \
  --force-device-scale-factor=1 \
  --screenshot="$OUT_PNG" \
  --dump-dom \
  "$URL" > "$DOM_HTML" 2>/dev/null

[ -s "$OUT_PNG" ] || { echo "baseline PNG render failed: $OUT_PNG" >&2; rm -f "$DOM_HTML"; exit 1; }

# Extract <script id="qw_dom_dump" type="application/json">…</script> body.
awk '
  /<script[^>]*id="qw_dom_dump"/ { dom = 1; sub(/.*<script[^>]*>/, ""); }
  dom {
    if (match($0, /<\/script>/)) {
      print substr($0, 1, RSTART - 1); exit;
    }
    print;
  }
' "$DOM_HTML" > "$OUT_DOM"

if [ ! -s "$OUT_DOM" ]; then
  echo "baseline DOM extract failed (script tag missing — emitDump never ran?): $OUT_DOM" >&2
  echo "  inspect: $DOM_HTML" >&2
  exit 1
fi
rm -f "$DOM_HTML"

PNG_SIZE=$(stat -c%s "$OUT_PNG")
DOM_SIZE=$(stat -c%s "$OUT_DOM")
echo "baseline: $OUT_PNG (${PNG_SIZE} bytes); $OUT_DOM (${DOM_SIZE} bytes)"
