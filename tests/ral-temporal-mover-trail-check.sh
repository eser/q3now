#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#
# TAA under moving entities, per backend (owner request, TASK-257). The parity harness is run unmodified on its e1m5 view: straight
# down onto two of the four looping Quake 1 func_trains (*7, *8), which move since the train core links q1_path_corner and follows
# the Quake 1 train rules, and which the RAL renderers draw since inline brush models register again. Time is NOT pinned: a mover's position comes from a server-time trajectory leg, so a pinned cg.time
# outside the current leg clamps it and nothing moves. The harness's fixedtime advances client and server time by the same amount every
# frame, which makes an unpinned capture reproducible (two runs differ only in the HUD counters). Three captures per backend:
#   taa   r_aa 1 throughout, captured STEP frames after the harness's settle point, so its history follows the movers' motion.
#   off   r_aa 0, the same frame (the reference image of the same pose).
#   base  r_aa 0, STEP frames earlier: |off - base| > 8 segments the region that moved (the mover region).
# The report is the mean abs deviation of the TAA frame from the AA-off frame at the same pose, inside the mover region and over the
# frame. History dragged along a wrong vector shows as a mover-region excess over the frame value (a trail). This detects trails;
# it does not rank TAA against SMAA (AA must change edges, so a deviation from AA off is not a quality loss by itself).
#
# Usage: tests/ral-temporal-mover-trail-check.sh            (backends: RAL_MOVER_BACKENDS, default "metal vulkan")
# Needs RAL_PARITY_MATERIAL_COOK_DIR holding e1m5's cooked materials (native Release loads cooked materials only).

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
. "$SCRIPT_DIR/lib/wired_paths.sh"

BACKENDS="${RAL_MOVER_BACKENDS:-metal vulkan}"
COOK="${RAL_PARITY_MATERIAL_COOK_DIR:-}"
[ -n "$COOK" ] || { echo "SKIP: set RAL_PARITY_MATERIAL_COOK_DIR (see tests/ral-metal-vulkan-visual-parity.sh)"; exit 0; }
PNG2RAW="${PNG2RAW:-$WIRED_SOURCE/tools/png2raw/png2raw}"
OUT="${RAL_MOVER_OUT:-$(mktemp -d "${WIRED_TMP:-${TMPDIR:-/tmp}}/ral-mover-trail-XXXXXX")}"
mkdir -p "$OUT"
STEP="${RAL_MOVER_STEP_FRAMES:-8}"
MAP="${RAL_MOVER_MAP:-e1m5}"

FAIL=0
for backend in $BACKENDS; do
	for spec in "taa:1:wait $STEP" "off:0:wait $STEP" "base:0:"; do
		name=${spec%%:*}; rest=${spec#*:}; aa=${rest%%:*}; pre=${rest#*:}
		RAL_PARITY_BACKENDS="$backend" RAL_PARITY_MAPS="$MAP" RAL_PARITY_MATERIAL_COOK_DIR="$COOK" RAL_PARITY_INITSOUND=0 \
			RAL_PARITY_ANALYZE=0 RAL_PARITY_REPEAT=0 RAL_PARITY_PIN_FRAME_TIME=0 RAL_PARITY_EXTRA_CFG="set r_aa $aa" \
			RAL_PARITY_PRE_SHOT="$pre" RAL_PARITY_OUT="$OUT/$backend-$name" \
			bash "$SCRIPT_DIR/ral-metal-vulkan-visual-parity.sh" >"$OUT/$backend-$name.log" 2>&1 \
			|| { echo "FAIL mover-trail $backend/$name capture (log: $OUT/$backend-$name.log)"; FAIL=1; }
	done
	python3 - "$OUT" "$backend" "$PNG2RAW" "$MAP" <<'PYEOF' || FAIL=1
import struct, subprocess, sys
out, backend, decoder, map_name = sys.argv[1:]
def load(name):
    path = f"{out}/{backend}-{name}/{backend}/{map_name}.png"
    w, h = struct.unpack(">II", open(path, "rb").read(24)[16:24])
    return w, h, subprocess.check_output([decoder, path])
w, h, taa = load("taa"); _, _, off = load("off"); _, _, base = load("base")
mover = frame = 0.0; mover_n = frame_n = 0
for y in range(h // 10, h * 7 // 10, 2):          # the world, above the HUD and the gun
    row = y * w * 3
    for x in range(w // 20, w * 13 // 20, 2):
        i = row + x * 3
        d = max(abs(taa[i + c] - off[i + c]) for c in range(3))
        frame += d; frame_n += 1
        if max(abs(off[i + c] - base[i + c]) for c in range(3)) > 8:
            mover += d; mover_n += 1
if mover_n < 200:
    raise SystemExit(f"FAIL mover-trail {backend}: mover region too small ({mover_n} px): the step moved nothing in frame")
print(f"PASS mover-trail {backend}: mover px {mover_n}, TAA vs AA off mean abs: mover region {mover / mover_n:.2f}, "
      f"frame {frame / frame_n:.2f}, excess {mover / mover_n - frame / frame_n:+.2f}")
PYEOF
done
echo "artifacts: $OUT"
exit $FAIL
