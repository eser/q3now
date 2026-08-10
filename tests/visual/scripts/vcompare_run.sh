#!/usr/bin/env bash
# Invokes vcompare for ARTBOARD/MODE/ACCENT/TIMESTAMP. Reads baseline PNG +
# DOM JSON from baselines/; reads impl PNG + Clay JSON from results/<run>/.
# Writes result.json to the same run dir. Exit 0 pass / 1 fail / 2 HALT.
set -euo pipefail
ARTBOARD="${ARTBOARD:?ARTBOARD required}"
MODE="${MODE:-dark}"
ACCENT="${ACCENT:-amber}"
TIMESTAMP="${TIMESTAMP:?TIMESTAMP required}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
BASELINE_PNG="$ROOT/visual/baselines/${ARTBOARD}_${MODE}_${ACCENT}.png"
BASELINE_DOM="$ROOT/visual/baselines/${ARTBOARD}_${MODE}_${ACCENT}_dom.json"
REGIONS="$ROOT/visual/regions/${ARTBOARD}.json"
[ -f "$ROOT/visual/regions/${ARTBOARD}_${MODE}.json" ] && REGIONS="$ROOT/visual/regions/${ARTBOARD}_${MODE}.json"
RUN_DIR="$ROOT/visual/results/${ARTBOARD}_${MODE}_${ACCENT}/$TIMESTAMP"
IMPL_PNG="$RUN_DIR/impl.png"
IMPL_CLAY="$RUN_DIR/impl_clay.json"
VCOMPARE="$ROOT/../tools/visual-compare/vcompare.exe"

[ -f "$VCOMPARE" ]    || { echo "vcompare binary missing: $VCOMPARE" >&2; exit 2; }
[ -f "$BASELINE_PNG" ] || { echo "baseline PNG missing (run regen first): $BASELINE_PNG" >&2; exit 2; }
[ -f "$REGIONS" ]      || { echo "regions JSON missing: $REGIONS" >&2; exit 2; }
[ -f "$IMPL_PNG" ]     || { echo "impl PNG missing (run capture first): $IMPL_PNG" >&2; exit 2; }

ARGS=(
  --baseline "$BASELINE_PNG"
  --impl     "$IMPL_PNG"
  --regions  "$REGIONS"
  --result-dir "$RUN_DIR"
  --artboard "$ARTBOARD"
  --mode     "$MODE"
  --accent   "$ACCENT"
)
if [ -f "$BASELINE_DOM" ] && [ -f "$IMPL_CLAY" ]; then
  ARGS+=( --baseline-tree "$BASELINE_DOM" --impl-tree "$IMPL_CLAY" )
fi
# Opt-in (default off, so the strict W-7.22 threshold-up guard is preserved for
# every other artboard). Set ALLOW_THRESHOLD_UP=1 only for a ratified per-region
# downward calibration pass (V1 5.18 pattern), where lowering SSIM thresholds to
# the measured floor legitimately trips the loosen guard.
if [ -n "${ALLOW_THRESHOLD_UP:-}" ]; then
  ARGS+=( --allow-threshold-up )
fi
"$VCOMPARE" "${ARGS[@]}"
