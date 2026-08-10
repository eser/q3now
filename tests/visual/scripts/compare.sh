#!/usr/bin/env bash
set -euo pipefail
ARTBOARD="${ARTBOARD:?ARTBOARD required}"
MODE="${MODE:-dark}"
ACCENT="${ACCENT:-amber}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
TIMESTAMP="${TIMESTAMP:-$(date +%Y%m%d_%H%M%S)}"
BASELINE="$ROOT/visual/baselines/${ARTBOARD}_${MODE}_${ACCENT}.png"
# Per-mode regions JSON preferred (e.g. v1_monolith_light.json), fallback
# to mode-agnostic v1_monolith.json. Light palettes carry larger
# cross-renderer delta against bright bg so thresholds differ per-mode.
if [ -f "$ROOT/visual/regions/${ARTBOARD}_${MODE}.json" ]; then
  REGIONS="$ROOT/visual/regions/${ARTBOARD}_${MODE}.json"
else
  REGIONS="$ROOT/visual/regions/${ARTBOARD}.json"
fi
RESULTS_DIR="$ROOT/visual/results/${ARTBOARD}_${MODE}_${ACCENT}/$TIMESTAMP"
IMPL="$RESULTS_DIR/impl.png"
DIFF="$RESULTS_DIR/diff.png"
SUMMARY="$RESULTS_DIR/result.json"
VDIFF="$ROOT/../tools/visual-diff/vdiff.exe"

[ -f "$VDIFF" ] || { echo "vdiff binary missing: $VDIFF" >&2; exit 2; }
[ -f "$BASELINE" ] || { echo "baseline missing (run regen first): $BASELINE" >&2; exit 2; }
[ -f "$REGIONS" ] || { echo "regions JSON missing: $REGIONS" >&2; exit 2; }
[ -f "$IMPL" ] || { echo "impl screenshot missing (run capture first): $IMPL" >&2; exit 2; }

"$VDIFF" -baseline "$BASELINE" -impl "$IMPL" -regions "$REGIONS" \
  -out-diff "$DIFF" -out-result "$SUMMARY"
