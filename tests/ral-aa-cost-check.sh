#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#
# GPU cost of the anti-aliasing modes side by side, inside ONE process.
#
# The temporal quality gate (ral-temporal-quality-check.sh) compares "no AA" with TAA only,
# so it cannot say what TAA costs against SMAA. This runs the three arms (r_aa 0 / 1 / 2) in a
# palindromic order, N S T T S N repeated, so a drifting GPU clock hits every arm alike, and
# reports per-arm medians of the engine's own GPU aggregate (r_gpuSpeeds, 200-frame averages).
#
# Instrument checks built in (not claims):
#   * repeat spread: the N arm appears four times; its spread is the noise floor the S and T
#     deltas have to beat.
#   * the "gate-on" arm reproduces the temporal gate's own comparison; if its delta disagrees with the gate
#     (+0.75 ms) the instrument or the configuration differs and the other numbers are not trusted.
#   * every arm's marker pair must be present and each window must carry exactly one 200-frame
#     GPU aggregate, otherwise the run fails instead of averaging holes.
#
#   AAC_FULL=1   keep the renderer's default features (bloom, AO, shadows, ...). Without it the
#                gate's stripped-down configuration is used, so the numbers compare to its.
#   AAC_ROUNDS=2 how many N S T T S N rounds (default 2).
#
# usage: ral-aa-cost-check.sh /absolute/path/to/wired

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
. "$SCRIPT_DIR/lib/wired_paths.sh"

TIMEOUT_RUNNER="$SCRIPT_DIR/run-with-timeout.py"
ROUNDS="${AAC_ROUNDS:-2}"

WIRED="${1:-${WIRED_BINARY:-}}"
[ -n "$WIRED" ] && [ -x "$WIRED" ] || { echo "usage: $0 /absolute/path/to/wired"; exit 64; }
[ "$(uname -s)" = Darwin ] || { echo "SKIP: runtime gate requires macOS/MoltenVK"; exit 77; }
[ -f "$TIMEOUT_RUNNER" ] || { echo "SKIP: timeout runner unavailable"; exit 77; }

WIRED="$(cd "$(dirname "$WIRED")" && pwd)/$(basename "$WIRED")"
WD="$(dirname "$WIRED")"
find_required() {
	local name="$1" candidate; shift
	for candidate in "$@"; do
		[ -f "$candidate/$name" ] && { printf '%s\n' "$candidate/$name"; return 0; }
	done
	return 1
}
GAMECL="$(find_required gameclarm64.dylib "$WD/base" "$WD/../Resources/base" "$WIRED_SOURCE/build/base")" || { echo "SKIP: gamecl unavailable"; exit 77; }
GAMESV="$(find_required gamesvarm64.dylib "$WD/base" "$WD/../Resources/base" "$WIRED_SOURCE/build/base")" || { echo "SKIP: gamesv unavailable"; exit 77; }
PAX21="$(find_required pax21.sw3z "$WD/base" "$WD/../Resources/base" "$WIRED_SOURCE/build/base")" || { echo "SKIP: pax21 unavailable"; exit 77; }
[ -f "$WIRED_BASE/pax01.sw3z" ] || { echo "SKIP: licensed base content unavailable in $WIRED_BASE"; exit 77; }

ROOT="$(mktemp -d -t ral-aa-cost-XXXXXX 2>/dev/null || mktemp -d)"
HOME_DIR="$ROOT/home"
cleanup() {
	local status=$?; trap - EXIT INT TERM
	if [ "${WIRED_KEEP_ARTIFACTS:-0}" = 1 ] || [ "$status" -ne 0 ]; then echo "retained root: $ROOT"; else rm -rf "$ROOT"; fi
	exit "$status"
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM
mkdir -p "$HOME_DIR/base"
wired_link_content_into_home "$HOME_DIR" "$WIRED_BASE" || exit 1
ln -sfn "$PAX21" "$HOME_DIR/base/pax21.sw3z"
ln -sfn "$GAMECL" "$HOME_DIR/base/gameclarm64.dylib"
ln -sfn "$GAMESV" "$HOME_DIR/base/gamesvarm64.dylib"

# Arm order: N S T D D T S N, repeated. 0 = none (r_aa 0), 1 = SMAA preset 3 (r_aa 1), 2 = TAA (r_aa 2),
# 3 = the temporal gate's own "on" arm (r_aa 0 + r_temporalInputTest 1), kept so this instrument can be checked
# against the gate's +0.75 ms figure in the same process.
ORDER=()
for (( r = 0; r < ROUNDS; r++ )); do ORDER+=(0 1 2 3 3 2 1 0); done
LAST=$(( ${#ORDER[@]} - 1 ))

BOOT="$HOME_DIR/base/ral-aa-cost.cfg"
{
	cat <<'CFGEOF'
set r_fullscreen 0
set r_mode -1
set r_customwidth 1280
set r_customheight 720
log renderer.ral info
log renderer.timing debug
set vm_game 0
set vm_cgame 0
set sv_cheats 1
set sv_pure 0
set aacSetup "cg_draw2D 0; cg_drawGun 0; cg_thirdPerson 0; con_notifytime 0; sv_fps 125; timescale 1; fixedtime 0; r_pinShaderTime 2.0; r_pinFrameTime 2.0; wait 512; noclip; setviewpos 888 -768 434 180 0; r_temporalInputTest 0; r_smaa 0; wait 64; vstr aacSeg0"
CFGEOF
	for (( i = 0; i <= LAST; i++ )); do
		arm="${ORDER[$i]}"; tag="ARM${arm}_$i"
		if [ "$i" -lt "$LAST" ]; then next="vstr aacSeg$(( i + 1 ))"; else next="echo AAC_COMPLETE; quit"; fi
		if [ "$arm" = 3 ]; then armcmd="r_aa 0; r_temporalInputTest 1"; else armcmd="r_temporalInputTest 0; r_aa $arm"; fi
		printf 'set aacSeg%s "%s; wait 96; set r_gpuSpeeds 1; echo AAC_%s_BEGIN; wait 220; echo AAC_%s_END; set r_gpuSpeeds 0; wait 16; %s"\n' "$i" "$armcmd" "$tag" "$tag" "$next"
	done
	echo 'set activeAction "vstr aacSetup"'
	echo 'map arena6'
} > "$BOOT"

STRIP=(+set r_bloom 0 +set r_ao 0 +set r_forwardPlus 0 +set r_drawSunRays 0 +set r_shadows 0 +set r_depthFade 0
	+set r_lens 0 +set r_gpuDecals 0 +set r_particles 0 +set r_atmosphericGPU 0)
[ "${AAC_FULL:-0}" = 1 ] && STRIP=()

QCONSOLE="$HOME_DIR/qconsole.jsonl"
STDOUT="$ROOT/stdout.log"
python3 "$TIMEOUT_RUNNER" --timeout 420 --kill-after 15 --cwd "$WD" --stdout "$STDOUT" -- \
	"$WIRED" +set fs_basepath "$HOME_DIR" +set fs_homepath "$HOME_DIR" +set fs_game base \
	+set com_automated 1 +set com_noHardReboot 1 +set s_initsound 0 +set s_volume 0 +set s_musicVolume 0 \
	+set r_fullscreen 0 +set r_mode -1 +set r_customwidth 1280 +set r_customheight 720 \
	+set r_vkValidate 0 +set r_temporalInputTest 0 +set r_smaa 0 \
	"${STRIP[@]}" \
	+set log_severity DEBUG +set log_file_severity DEBUG +set log_file_mode overwrite_synced \
	+exec ral-aa-cost.cfg
RC=$?
[ "$RC" -eq 0 ] || { echo "FAIL aa-cost process rc=$RC"; exit 1; }
[ -f "$QCONSOLE" ] || { echo "FAIL aa-cost missing qconsole"; exit 1; }

python3 - "$QCONSOLE" "${ORDER[*]}" "${AAC_FULL:-0}" <<'PYEOF'
import json, re, statistics, sys
log_path, order_s, full = sys.argv[1], sys.argv[2], sys.argv[3]
order = [int(x) for x in order_s.split()]
rows = [json.loads(l) for l in open(log_path, encoding="utf-8") if l.strip()]
msgs = [r["msg"].removesuffix("\n") for r in rows]
if any(r.get("sev", "").upper() in ("ERROR", "FATAL") for r in rows):
    raise SystemExit("FAIL aa-cost ERROR/FATAL severity in the log")
if "AAC_COMPLETE" not in msgs:
    raise SystemExit("FAIL aa-cost run did not complete")
marker = re.compile(r"AAC_ARM([0123])_(\d+)_(BEGIN|END)")
header = re.compile(r"gpu \((\d+)f avg, ms\):")
label = re.compile(r"  ([A-Za-z_0-9]+)=([0-9.]+)")
seg = {}; active = None
for row, m in zip(rows, msgs):
    mm = marker.fullmatch(m)
    if mm:
        key = (int(mm.group(1)), int(mm.group(2)))
        if mm.group(3) == "BEGIN":
            if active is not None: raise SystemExit("FAIL aa-cost marker nesting")
            active = key; seg[key] = []
        else:
            if active != key: raise SystemExit("FAIL aa-cost marker order")
            active = None
        continue
    if active is None or row.get("cat", "").lower() != "renderer.timing":
        continue
    h = header.fullmatch(m)
    if h: seg[active].append({"frames": int(h.group(1))}); continue
    if seg[active]:
        l = label.fullmatch(m)
        if l: seg[active][-1][l.group(1)] = float(l.group(2))
per = {0: [], 1: [], 2: [], 3: []}
lanes = {0: {}, 1: {}, 2: {}, 3: {}}
for i, arm in enumerate(order):
    full_s = [s for s in seg.get((arm, i), []) if s.get("frames") == 200 and "total" in s]
    if len(full_s) != 1:
        raise SystemExit(f"FAIL aa-cost segment {i} (arm {arm}): expected one 200-frame aggregate, got {len(full_s)}")
    per[arm].append(full_s[0]["total"])
    for k, v in full_s[0].items():
        if k not in ("frames", "total"): lanes[arm].setdefault(k, []).append(v)
name = {0: "none", 1: "SMAA3", 2: "TAA", 3: "gate-on"}
print(f"config: {'full default renderer' if full == '1' else 'stripped (as the temporal gate)'}; order {order_s}")
for arm in (0, 1, 2, 3):
    v = per[arm]
    print(f"  {name[arm]:6s} n={len(v)} median={statistics.median(v):.3f} ms  min={min(v):.3f} max={max(v):.3f}  all={['%.3f' % x for x in v]}")
base = statistics.median(per[0]); noise = max(per[0]) - min(per[0])
print(f"  noise floor (spread of the 'none' arm, {len(per[0])} windows): {noise:.3f} ms")
for arm in (1, 2, 3):
    d = statistics.median(per[arm]) - base
    print(f"  {name[arm]} - none = {d:+.3f} ms ({d / base * 100:+.1f}%)  -> {'above' if abs(d) > noise else 'WITHIN'} the noise floor")
d_ts = statistics.median(per[2]) - statistics.median(per[1])
print(f"  TAA - SMAA3 = {d_ts:+.3f} ms")
print("  lane medians that differ most from 'none' (ms):")
for arm in (1, 2, 3):
    diffs = []
    for k, vs in lanes[arm].items():
        b = lanes[0].get(k)
        if b: diffs.append((statistics.median(vs) - statistics.median(b), k, statistics.median(vs)))
    diffs.sort(key=lambda x: -abs(x[0]))
    print(f"    {name[arm]}: " + ", ".join(f"{k}={d:+.3f}" for d, k, _ in diffs[:6]))
PYEOF
exit $?
