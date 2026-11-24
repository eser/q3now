#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#
# TAA under a visible effect, per backend (owner request, TASK-257). The parity harness is run unmodified: arena1 at its fixed view,
# PRE_SHOT re-runs setviewpos so cgame's teleport-in effect surrounds the camera, RAL_PARITY_PIN_FRAME_TIME 3.0 keeps it alive and
# visibly faded (5.0 frees it, 1.0 leaves it invisible), and a short yaw sweep moves the camera while the effect stays. Three captures
# per backend: TAA (r_aa 1) and AA off (r_aa 0) with the effect, and AA off without it (pin 5.0) to segment the effect region
# (|off3 - off5| > 8). The report is the mean abs deviation of the TAA frame from the AA-off frame at the same pose, inside the effect
# region and over the frame: a trail under the effect shows as an effect-region excess over the frame value. The fixture is
# deterministic (repeat runs give identical figures).
#
# Usage: tests/ral-temporal-effect-trail-check.sh            (backends: RAL_TRAIL_BACKENDS, default "metal vulkan")
# Needs RAL_PARITY_MATERIAL_COOK_DIR (native Metal Release loads cooked materials only).
# Reference, 2026-11-24 (2560x1440): Metal 1.71 / 1.45, Vulkan 1.70 / 1.43 (effect region / frame).

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
. "$SCRIPT_DIR/lib/wired_paths.sh"

BACKENDS="${RAL_TRAIL_BACKENDS:-metal vulkan}"
COOK="${RAL_PARITY_MATERIAL_COOK_DIR:-}"
[ -n "$COOK" ] || { echo "SKIP: set RAL_PARITY_MATERIAL_COOK_DIR (see tests/ral-metal-vulkan-visual-parity.sh)"; exit 0; }
PNG2RAW="${PNG2RAW:-$WIRED_SOURCE/tools/png2raw/png2raw}"
OUT="${RAL_TRAIL_OUT:-$(mktemp -d "${WIRED_TMP:-${TMPDIR:-/tmp}}/ral-effect-trail-XXXXXX")}"
PRE="cmd setviewpos 1052 1432 50 135 0; wait 20; +right; wait 12; -right; wait 4"

FAIL=0
for backend in $BACKENDS; do
	for spec in "taa:1:3.0" "off:0:3.0" "clean:0:5.0"; do
		name=${spec%%:*}; rest=${spec#*:}; aa=${rest%%:*}; pin=${rest#*:}
		RAL_PARITY_BACKENDS="$backend" RAL_PARITY_MAPS=arena1 RAL_PARITY_MATERIAL_COOK_DIR="$COOK" RAL_PARITY_INITSOUND=0 \
			RAL_PARITY_ANALYZE=0 RAL_PARITY_REPEAT=0 RAL_PARITY_PIN_FRAME_TIME="$pin" RAL_PARITY_EXTRA_CFG="set r_aa $aa" \
			RAL_PARITY_PRE_SHOT="$PRE" RAL_PARITY_OUT="$OUT/$backend-$name" \
			bash "$SCRIPT_DIR/ral-metal-vulkan-visual-parity.sh" >"$OUT/$backend-$name.log" 2>&1 \
			|| { echo "FAIL effect-trail $backend/$name capture (log: $OUT/$backend-$name.log)"; FAIL=1; }
	done
	python3 - "$OUT" "$backend" "$PNG2RAW" <<'PYEOF' || FAIL=1
import struct, subprocess, sys
out, backend, decoder = sys.argv[1:]
def load(name):
    path = f"{out}/{backend}-{name}/{backend}/arena1.png"
    w, h = struct.unpack(">II", open(path, "rb").read(24)[16:24])
    return w, h, subprocess.check_output([decoder, path])
w, h, taa = load("taa"); _, _, off = load("off"); _, _, clean = load("clean")
effect = frame = 0.0; effect_n = frame_n = 0
for y in range(h // 10, h * 7 // 10, 2):          # the world, above the HUD and the gun
    row = y * w * 3
    for x in range(w // 20, w * 13 // 20, 2):
        i = row + x * 3
        d = max(abs(taa[i + c] - off[i + c]) for c in range(3))
        frame += d; frame_n += 1
        if max(abs(off[i + c] - clean[i + c]) for c in range(3)) > 8:
            effect += d; effect_n += 1
if effect_n < 1000:
    raise SystemExit(f"FAIL effect-trail {backend}: effect region too small ({effect_n} px): the effect is not in frame")
print(f"PASS effect-trail {backend}: effect px {effect_n}, TAA vs AA off mean abs: effect region {effect / effect_n:.2f}, "
      f"frame {frame / frame_n:.2f}, excess {effect / effect_n - frame / frame_n:+.2f}")
PYEOF
done
echo "artifacts: $OUT"
exit $FAIL
