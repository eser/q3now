#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# TAA must actually resolve in real play, not only in the stripped, time-pinned test scenes. Runs the gameplay lane
# (arena1, bots, default features, r_aa 2 switched on at run time) and requires that almost every temporal frame is a resolved
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
	FPS_PERF_EXTRA_CFG="set r_aa 2" bash "$SCRIPT_DIR/fps-perf-gate.sh" --engine "$ENGINE" --map "${TG_MAP:-arena1}" --bots "${TG_BOTS:-6}" --tag "$TAG" >/tmp/temporal-gameplay-$TAG.out 2>&1 || true
dir="$(find "$SCRIPT_DIR/../build/test-results/fps-gate" -maxdepth 1 -name "run-$TAG-*" 2>/dev/null | head -1)"
[ -n "$dir" ] && [ -f "$dir/qconsole.jsonl" ] || { echo "FAIL temporal gameplay: no run evidence ($TAG)"; exit 1; }
python3 - "$dir/qconsole.jsonl" "$MIN_RATIO" <<'PYEOF'
import json, sys
path, min_ratio = sys.argv[1], float(sys.argv[2])
copy = resolved = 0; poisoned = None
for line in open(path, encoding="utf-8"):
    try: msg = json.loads(line)["msg"]
    except Exception: continue
    if "temporal-resolved-hdr schema=2" in msg: copy += 1
    elif "temporal-resolved-hdr schema=3" in msg: resolved += 1
    elif msg.startswith("temporal motion receipts so far:"):
        poisoned = int(msg.split("poisoned=")[1].split()[0])
total = copy + resolved
if total == 0:
    raise SystemExit("FAIL temporal gameplay: no temporal frames logged (is r_aa 2 active?)")
ratio = resolved / total
print(f"temporal gameplay: resolved={resolved} bootstrap-copy={copy} ratio={ratio:.4f} (needs >= {min_ratio}) poisoned={poisoned}")
if ratio < min_ratio:
    raise SystemExit("FAIL temporal gameplay: the resolve is not running in real play")
if poisoned not in (None, 0):
    raise SystemExit("FAIL temporal gameplay: temporal motion poisoned")
print("PASS temporal gameplay")
PYEOF
