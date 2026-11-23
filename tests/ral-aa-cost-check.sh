#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#
# GPU cost of the anti-aliasing modes side by side, inside ONE process.
#
# TAA (r_aa 1) is the default technique; SMAA (r_aa 2) is an option. This runs the arms none (r_aa 0), TAA (r_aa 1) and the
# temporal gate's own "on" arm in a palindromic order, N T G G T N repeated, so a drifting GPU
# clock hits every arm alike, and reports per-arm medians of the engine's own GPU aggregate
# (r_gpuSpeeds, 200-frame averages).
#
# Instrument checks built in (not claims):
#   * repeat spread: the N arm appears four times; its spread is the noise floor the T and G
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
ROUNDS="${AAC_ROUNDS:-1}"   # more than one round (32 live mode switches) exhausts the descriptor pool, see the AA-switch bug

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

# Arms, run in a palindromic order and repeated. 0 none (r_aa 0), 2 TAA (r_aa 1),
# 3 the temporal gate's own "on" arm (r_aa 0 + r_temporalInputTest 1),
# 6 none + 4 redundant pass splits, 7 none + 16 redundant pass splits (r_dbgExtraSplit), 8 TAA with the older
# per-draw exact pass (r_dbgExactHold 0; the default is the held exact pass). Arms 6-7 are dose
# responses: they say whether a cost grows with the number of render passes. The arm numbers
# are stable identifiers: the retired SMAA arms (1, 4, 5) are simply not assigned.
ORDER=()
# AAC_ARMS picks the arms (default 0 2 3: none, TAA, gate). The dose arms 6-7 answered their question and
# every live mode switch eats descriptor-pool memory (TASK-258), so they are opt-in.
ARMS=(${AAC_ARMS:-0 2 3})
for a in "${ARMS[@]}"; do
	case "$a" in
		0|1|2|3|4|5|6|7|8|9|10|11|12|13|14|15|16|17) ;;
		*) echo "FAIL unknown arm '$a' in AAC_ARMS"; exit 64 ;;
	esac
done
for (( r = 0; r < ROUNDS; r++ )); do
	for a in "${ARMS[@]}"; do ORDER+=("$a"); done
	for (( k = ${#ARMS[@]} - 1; k >= 0; k-- )); do ORDER+=("${ARMS[$k]}"); done
done
LAST=$(( ${#ORDER[@]} - 1 ))

BOOT="$HOME_DIR/base/ral-aa-cost.cfg"
{
	cat <<CFGEOF
set r_fullscreen 0
set r_mode -1
set r_customwidth ${AAC_W:-1280}
set r_customheight ${AAC_H:-720}
log renderer.ral info
log renderer.timing debug
set vm_game 0
set vm_cgame 0
set sv_cheats 1
set sv_pure 0
set aacSetup "cg_draw2D 0; cg_drawGun 0; cg_thirdPerson 0; con_notifytime 0; sv_fps 125; timescale 1; fixedtime 0; r_pinShaderTime 2.0; r_pinFrameTime 2.0; wait 512; noclip; wait 150; setviewpos 888 -768 434 180 0; r_temporalInputTest 0; ${AAC_PAN:+cl_yawspeed 60; +right; }wait 64; vstr aacSeg0"
CFGEOF
	for (( i = 0; i <= LAST; i++ )); do
		arm="${ORDER[$i]}"; tag="ARM${arm}_$i"
		if [ "$i" -lt "$LAST" ]; then next="vstr aacSeg$(( i + 1 ))"; else next="echo AAC_COMPLETE; quit"; fi
		case "$arm" in
			0) armcmd="r_temporalInputTest 0; r_aa 0; r_dbgExtraSplit 0; r_dbgExactHold 0" ;;
			1) armcmd="r_temporalInputTest 0; r_aa 2; r_smaa 0; r_dbgExtraSplit 0; r_dbgExactHold 0" ;;   # SMAA, preset 3 (high)
			2) armcmd="r_temporalInputTest 0; r_aa 1; r_dbgExtraSplit 0; r_dbgExactHold 1; r_dbgTemporalRepeatResolve 1; r_dbgTemporalRepeatStore 1; r_dbgTemporalFold 1; r_dbgTemporalQuality 7" ;;
			3) armcmd="r_aa 0; r_dbgExtraSplit 0; r_dbgExactHold 1; r_temporalInputTest 1" ;;
			4) armcmd="r_temporalInputTest 0; r_aa 2; r_smaa 1; r_dbgExtraSplit 0" ;;   # SMAA preset 1 (low)
			5) armcmd="r_temporalInputTest 0; r_aa 2; r_smaa 4; r_dbgExtraSplit 0" ;;   # SMAA preset 4 (ultra)
			6) armcmd="r_temporalInputTest 0; r_aa 0; r_dbgExtraSplit 4" ;;
			7) armcmd="r_temporalInputTest 0; r_aa 0; r_dbgExtraSplit 16" ;;
			8) armcmd="r_temporalInputTest 0; r_aa 1; r_dbgExtraSplit 0; r_dbgExactHold 0" ;;
			9) armcmd="r_temporalInputTest 0; r_aa 1; r_dbgExtraSplit 0; r_dbgExactHold 1; r_dbgTemporalRepeatResolve 4; r_dbgTemporalRepeatStore 1" ;;
			10) armcmd="r_temporalInputTest 0; r_aa 1; r_dbgExtraSplit 0; r_dbgExactHold 1; r_dbgTemporalRepeatResolve 1; r_dbgTemporalRepeatStore 4" ;;
			11) armcmd="r_temporalInputTest 0; r_aa 1; r_dbgExtraSplit 0; r_dbgExactHold 1; r_dbgTemporalRepeatResolve -1; r_dbgTemporalRepeatStore 1" ;;
			12) armcmd="r_temporalInputTest 0; r_aa 1; r_dbgExtraSplit 0; r_dbgExactHold 1; r_dbgTemporalRepeatResolve -1; r_dbgTemporalRepeatStore -1" ;;
			13) armcmd="r_temporalInputTest 0; r_aa 1; r_dbgExtraSplit 0; r_dbgExactHold 1; r_dbgTemporalRepeatResolve 1; r_dbgTemporalRepeatStore 1; r_dbgTemporalFold 0" ;;
			14) armcmd="r_temporalInputTest 0; r_aa 1; r_dbgExtraSplit 0; r_dbgExactHold 1; r_dbgTemporalRepeatResolve 1; r_dbgTemporalRepeatStore 1; r_dbgTemporalFold 1; r_dbgTemporalQuality 0" ;;
			15) armcmd="r_temporalInputTest 0; r_aa 1; r_dbgExtraSplit 0; r_dbgExactHold 1; r_dbgTemporalRepeatResolve 1; r_dbgTemporalRepeatStore 1; r_dbgTemporalFold 1; r_dbgTemporalQuality 3" ;;
			16) armcmd="r_temporalInputTest 0; r_aa 1; r_dbgExtraSplit 0; r_dbgExactHold 1; r_dbgTemporalRepeatResolve 1; r_dbgTemporalRepeatStore 1; r_dbgTemporalFold 1; r_dbgTemporalQuality 4" ;;
			17) armcmd="r_temporalInputTest 0; r_aa 1; r_dbgExtraSplit 0; r_dbgExactHold 2; r_dbgTemporalRepeatResolve 1; r_dbgTemporalRepeatStore 1; r_dbgTemporalFold 1; r_dbgTemporalQuality 7" ;;   # leak control: ordinary draws issue into the held exact pass, so no draw closes it (upper bound of what pass splits cost)
		esac
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
python3 "$TIMEOUT_RUNNER" --timeout 900 --kill-after 15 --cwd "$WD" --stdout "$STDOUT" -- \
	"$WIRED" +set fs_basepath "$HOME_DIR" +set fs_homepath "$HOME_DIR" +set fs_game base \
	+set com_automated 1 +set com_noHardReboot 1 +set s_initsound 0 +set s_volume 0 +set s_musicVolume 0 \
	+set r_fullscreen 0 +set r_mode -1 +set r_customwidth "${AAC_W:-1280}" +set r_customheight "${AAC_H:-720}" \
	+set r_vkValidate 0 +set r_temporalInputTest 0 \
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
marker = re.compile(r"AAC_ARM(\d+)_(\d+)_(BEGIN|END)")
header = re.compile(r"gpu \((\d+)f avg, ms\):")
label = re.compile(r"  ([A-Za-z_0-9]+)=([0-9.]+)")
route_pat = re.compile(r"temporal route sequence: (\d+) draws: exact (\d+) in (\d+) runs, ordinary (\d+); plans write (\d+) invalidate (\d+) preserve (\d+)")
route_seg = {}
pass_pat = re.compile(r"render passes per frame \(100 frames\): ([0-9.]+) ")
seg = {}; active = None; pas_seg = {}
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
    if active is not None:
        pm = pass_pat.match(m)
        if pm: pas_seg.setdefault(active, []).append(float(pm.group(1)))
        rm = route_pat.match(m)
        if rm: route_seg.setdefault(active, []).append(tuple(int(x) for x in rm.groups()))
    if active is None or row.get("cat", "").lower() != "renderer.timing":
        continue
    h = header.fullmatch(m)
    if h: seg[active].append({"frames": int(h.group(1))}); continue
    if seg[active]:
        l = label.fullmatch(m)
        if l: seg[active][-1][l.group(1)] = float(l.group(2))
per = {a: [] for a in range(24)}
lanes = {a: {} for a in range(24)}
passes = {a: [] for a in range(24)}
pass_pat = re.compile(r"render passes per frame \(100 frames\): ([0-9.]+) ")
for i, arm in enumerate(order):
    full_s = [s for s in seg.get((arm, i), []) if s.get("frames") == 200 and "total" in s]
    if len(full_s) != 1:
        raise SystemExit(f"FAIL aa-cost segment {i} (arm {arm}): expected one 200-frame aggregate, got {len(full_s)}")
    per[arm].append(full_s[0]["total"])
    if pas_seg.get((arm, i)): passes[arm].append(statistics.median(pas_seg[(arm, i)]))
    for k, v in full_s[0].items():
        if k not in ("frames", "total"): lanes[arm].setdefault(k, []).append(v)
RAN = [arm for arm in range(24) if per[arm]]
name = {0: "none", 1: "SMAA3", 2: "TAA", 4: "SMAA1", 5: "SMAA4", 3: "gate-on", 6: "none+4sp", 7: "none+16sp", 8: "TAA-legacy", 9: "TAA resolve x4", 10: "TAA store x4", 11: "TAA resolve skipped", 12: "TAA resolve+store skipped", 13: "TAA store-copy", 14: "TAA quality off", 15: "TAA no fallback", 16: "TAA fallback only", 17: "TAA hold-leak control"}
print(f"config: {'full default renderer' if full == '1' else 'stripped (as the temporal gate)'}; order {order_s}")
for arm in RAN:
    v = per[arm]
    pp = f"  passes/frame={statistics.median(passes[arm]):.1f}" if passes[arm] else "  passes/frame=?"
    print(f"  {name[arm]:9s} n={len(v)} median={statistics.median(v):.3f} ms  min={min(v):.3f} max={max(v):.3f}{pp}")
# Regime check. When the GPU is paced by presentation (240 Hz => 4.16 ms) the "total" of every window reads
# the frame period, not the work, and the present_prep lane absorbs the wait. A window is pacing-bound when it
# sits within 4.10-4.20 ms; if the none arm is, no cost claim from this run is valid.
PERIOD_LO, PERIOD_HI = 4.10, 4.20
bound = {arm: sum(1 for x in per[arm] if PERIOD_LO <= x <= PERIOD_HI) for arm in RAN}
total_w = sum(len(per[arm]) for arm in RAN); bound_w = sum(bound.values())
print(f"  regime: {bound_w} of {total_w} windows read the 240 Hz frame period (4.10-4.20 ms)")
if bound[0] == len(per[0]) or bound_w * 2 > total_w:
    print("  REGIME-LOCKED: the machine was paced by presentation during this run; the numbers above measure the frame period, not the GPU work. NO cost claim is valid. Run again later (the decoupled regime comes and goes).")
    raise SystemExit(75)
if bound_w:
    print("  WARNING: some windows are pacing-bound; deltas that involve them are not valid. Valid windows only are the ones outside 4.10-4.20 ms.")
base = statistics.median(per[0]); noise = max(per[0]) - min(per[0])
print(f"  noise floor (spread of the 'none' arm, {len(per[0])} windows): {noise:.3f} ms")
for arm in RAN[1:]:
    d = statistics.median(per[arm]) - base
    dp = (statistics.median(passes[arm]) - statistics.median(passes[0])) if passes[arm] and passes[0] else None
    per_pass = f"; {d / dp:+.3f} ms per extra pass ({dp:+.1f} passes)" if dp else ""
    print(f"  {name[arm]} - none = {d:+.3f} ms ({d / base * 100:+.1f}%)  -> {'above' if abs(d) > noise else 'WITHIN'} the noise floor{per_pass}")
print("  exact-route structure per arm (a run costs 4 render passes):")
for arm in RAN:
    rs = [t for (a_, i_), v in route_seg.items() if a_ == arm for t in v]
    if rs:
        d, e, r, o, w, inv, pr = (sum(x[k] for x in rs) for k in range(7))
        print(f"    {name[arm]:9s} draws={d} exact={e} ({e / d * 100:.0f}%) in {r} runs ({e / max(r, 1):.1f} draws/run); plans write={w} invalidate={inv} preserve={pr}")
print("  per-arm lane medians above 0.05 ms:")
for arm in RAN:
    big = sorted(((statistics.median(vs), k) for k, vs in lanes[arm].items() if statistics.median(vs) > 0.05), reverse=True)
    print(f"    {name[arm]:8s} " + ", ".join(f"{k}={v:.3f}" for v, k in big))
print("  lane medians that differ most from 'none' (ms):")
for arm in [a for a in (2, 3) if a in RAN]:
    diffs = []
    for k, vs in lanes[arm].items():
        b = lanes[0].get(k)
        if b: diffs.append((statistics.median(vs) - statistics.median(b), k, statistics.median(vs)))
    diffs.sort(key=lambda x: -abs(x[0]))
    print(f"    {name[arm]}: " + ", ".join(f"{k}={d:+.3f}" for d, k, _ in diffs[:6]))
PYEOF
exit $?
