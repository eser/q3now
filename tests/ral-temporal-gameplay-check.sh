#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# TAA must actually resolve in real play, not only in the stripped, time-pinned test scenes. Runs the gameplay lane
# (arena1, bots, default features, r_aa 1 switched on at run time) and requires that almost every temporal frame is a resolved
# one. A frame whose history cannot be written (no resolved log line, only the bootstrap copy) loops back to the bootstrap on the
# next frame through the frame-gap reset, so this ratio collapses to zero when something in the frame breaks the history store.
# Regression guard for the atmosphere pass-close bug (0 resolved frames in 11000+).
set -uo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ENGINE="${1:-}"
[ -n "$ENGINE" ] || { echo "usage: $0 <engine binary>" >&2; exit 2; }
MIN_RATIO="${TG_MIN_RATIO:-0.95}"
TAG="tgc$$"
FPS_PERF_REGIME_MIN_FPS="${FPS_PERF_REGIME_MIN_FPS:-0}" FPS_PERF_GAMEPLAY_SECONDS="${FPS_PERF_GAMEPLAY_SECONDS:-32}" \
	FPS_PERF_EXTRA_CFG="set r_aa 1" bash "$SCRIPT_DIR/fps-perf-gate.sh" --engine "$ENGINE" --map "${TG_MAP:-arena1}" --bots "${TG_BOTS:-6}" --tag "$TAG" >/tmp/temporal-gameplay-$TAG.out 2>&1 || true
dir="$(find "$SCRIPT_DIR/../build/test-results/fps-gate" -maxdepth 1 -name "run-$TAG-*" 2>/dev/null | head -1)"
[ -n "$dir" ] && [ -f "$dir/qconsole.jsonl" ] || { echo "FAIL temporal gameplay: no run evidence ($TAG)"; exit 1; }
python3 - "$dir/qconsole.jsonl" "$MIN_RATIO" <<'PYEOF'
import json, sys
path, min_ratio = sys.argv[1], float(sys.argv[2])
copy = resolved = 0; poisoned = None
uncovered = []   # per 1000 draws: ordinary-route draws that are NOT blended (an opaque surface without a temporal pipeline)
exact_share = []
for line in open(path, encoding="utf-8"):
    try: msg = json.loads(line)["msg"]
    except Exception: continue
    if "temporal-resolved-hdr schema=2" in msg: copy += 1
    elif "temporal-resolved-hdr schema=3" in msg: resolved += 1
    elif msg.startswith("temporal ordinary-route by shader type"):
        head, _, tail = msg.partition("):")
        blended = int(head.split("blended")[1].strip().rstrip(")"))
        total_ord = sum(int(t.split(":")[1]) for t in tail.split())
        uncovered.append(max(0, total_ord - blended))
    elif msg.startswith("temporal route sequence:"):
        exact_share.append(int(msg.split("exact")[1].split()[0]))
    elif msg.startswith("temporal motion receipts so far:"):
        poisoned = int(msg.split("poisoned=")[1].split()[0])
total = copy + resolved
if total == 0:
    raise SystemExit("FAIL temporal gameplay: no temporal frames logged (is r_aa 1 active?)")
ratio = resolved / total
print(f"temporal gameplay: resolved={resolved} bootstrap-copy={copy} ratio={ratio:.4f} (needs >= {min_ratio}) poisoned={poisoned}")
if ratio < min_ratio:
    raise SystemExit("FAIL temporal gameplay: the resolve is not running in real play")
if poisoned not in (None, 0):
    raise SystemExit("FAIL temporal gameplay: temporal motion poisoned")
# Pixel coverage. A pixel only reaches temporal accumulation if an exact-route draw wrote its velocity and validity, so what
# must hold is that no OPAQUE draw is left on the ordinary route: every ordinary draw in default scenes is a blended light or
# effect overlay (which keeps the validity of the surface beneath it, or invalidates it on purpose). Measured 0 per 1000 draws
# at defaults in arena1 with six bots; a regression that drops a pipeline family from the catalog shows here at once.
MAX_UNCOVERED = 30
if uncovered:
    worst = max(uncovered)
    print(f"temporal coverage: non-blended ordinary-route draws per 1000: max={worst} over {len(uncovered)} windows "
          f"(needs <= {MAX_UNCOVERED}); exact-route share per 1000 draws: {min(exact_share) if exact_share else 'n/a'}.."
          f"{max(exact_share) if exact_share else 'n/a'}")
    if worst > MAX_UNCOVERED:
        raise SystemExit("FAIL temporal gameplay: opaque draws are left on the ordinary (history-less) route")
else:
    print("temporal coverage: no route-by-type windows logged (run shorter than 1000 draws)")
print("PASS temporal gameplay")
PYEOF
