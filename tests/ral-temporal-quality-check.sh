#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# 1280x720 temporal quality + full-path GPU budget acceptance gate.
# The in-run baseline is the AA-off frame at the same camera (SMAA is no longer needed as a reference: a still frame without AA is as
# stable as a still SMAA frame, and AA off is a legitimate readability baseline).

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
. "$SCRIPT_DIR/lib/wired_paths.sh"

TIMEOUT_RUNNER="$SCRIPT_DIR/run-with-timeout.py"
PNG2RAW="${PNG2RAW:-$WIRED_SOURCE/tools/png2raw/png2raw}"

analyze() {
	python3 - "$1" "$2" "$PNG2RAW" <<'PYEOF'
import json, math, os, re, statistics, struct, subprocess, sys
from datetime import datetime

log_path, shot_dir, decoder = sys.argv[1:]
rows = [json.loads(line) for line in open(log_path, encoding="utf-8") if line.strip()]
messages = [row["msg"].removesuffix("\n") for row in rows]
if any(row.get("sev", "").upper() in ("ERROR", "FATAL") for row in rows):
    raise SystemExit("FAIL temporal-quality ERROR/FATAL severity")
if any("VUID-" in message for message in messages):
    raise SystemExit("FAIL temporal-quality Vulkan VUID")

expected_markers = [
    "Q3_TEMPORAL_QUALITY_BEGIN", "Q3_TEMPORAL_QUALITY_CAPTURED",
    "Q3_TEMPORAL_PERF_VIEW",
    "Q3_TEMPORAL_PERF_OFF_1_BEGIN", "Q3_TEMPORAL_PERF_OFF_1_END",
    "Q3_TEMPORAL_PERF_ON_1_BEGIN", "Q3_TEMPORAL_PERF_ON_1_END",
    "Q3_TEMPORAL_PERF_SMAA_1_BEGIN", "Q3_TEMPORAL_PERF_SMAA_1_END",
    "Q3_TEMPORAL_PERF_SMAA_2_BEGIN", "Q3_TEMPORAL_PERF_SMAA_2_END",
    "Q3_TEMPORAL_PERF_ON_2_BEGIN", "Q3_TEMPORAL_PERF_ON_2_END",
    "Q3_TEMPORAL_PERF_OFF_2_BEGIN", "Q3_TEMPORAL_PERF_OFF_2_END",
    "Q3_TEMPORAL_QUALITY_COMPLETE",
]
markers = [(i, message) for i, message in enumerate(messages) if message.startswith("Q3_TEMPORAL_")]
if [message for _, message in markers] != expected_markers:
    raise SystemExit(f"FAIL temporal-quality marker vector: {[message for _, message in markers]}")

window_pattern = re.compile(
    r"window-extent schema=3 requested=1280x720 logical=1280x720 "
    r"pixels=([1-9][0-9]*)x([1-9][0-9]*) publish-ready=1")
windows = [window_pattern.fullmatch(message) for message in messages]
windows = [match for match in windows if match]
if len(windows) != 1:
    raise SystemExit(f"FAIL temporal-quality window receipt cardinality {len(windows)}")
pixel_w, pixel_h = map(int, windows[0].groups())
if pixel_w < 1280 or pixel_h < 720 or pixel_w * 9 != pixel_h * 16:
    raise SystemExit("FAIL temporal-quality window is not >=1280x720 16:9")
if not any(message.startswith("temporal-resolved-hdr schema=3 ") for message in messages):
    raise SystemExit("FAIL temporal-quality has no resolved temporal output")
if not any(message.startswith("temporal-history-feedback ") for message in messages):
    raise SystemExit("FAIL temporal-quality has no recursive history feedback")
# The static TAA view must actually take the history branch. Its motion validity is read back over the centre ROI (world
# geometry only); a run whose validity is zero there resolves every pixel without history (reconstruction plus the spatial
# fallback) and measures nothing about TAA. This is what a teleport-in effect around the camera did to every capture.
# The performance view is read back the same way (once, before its first TAA window): a resolve without history costs about
# half, so a budget measured on it would flatter TAA.
content_pattern = re.compile(r"temporal-motion-content schema=1 .* pixels=([1-9][0-9]*) validity-zero=([0-9]+) ")
contents = [content_pattern.match(message) for message in messages]
contents = [match for match in contents if match]
if not contents:
    raise SystemExit("FAIL temporal-quality no motion-target readback (history in use is unproven)")
history_share = min((int(m.group(1)) - int(m.group(2))) / int(m.group(1)) for m in contents)
if history_share < 0.90:
    raise SystemExit(f"FAIL temporal-quality history not in use: motion validity non-zero on {history_share:.1%} of the readback ROI (< 90%)")

names = (
    "temporal_off_a", "temporal_off_a_2", "temporal_off_a_3", "temporal_off_b", "temporal_taa_a_1",
    "temporal_taa_a_2", "temporal_taa_a_3", "temporal_taa_a_4", "temporal_taa_a_5", "temporal_taa_a_6", "temporal_taa_a_7",
    "temporal_taa_a_8", "temporal_taa_a_9", "temporal_taa_a_10", "temporal_taa_b_first", "temporal_taa_b_steady",
)

def load(name):
    path = f"{shot_dir}/{name}.png"
    with open(path, "rb") as handle:
        header = handle.read(24)
    if len(header) != 24 or header[:8] != b"\x89PNG\r\n\x1a\n":
        raise SystemExit(f"FAIL temporal-quality missing/invalid {name}.png")
    width, height = struct.unpack(">II", header[16:24])
    raw = subprocess.check_output([decoder, path])
    if len(raw) != width * height * 3:
        raise SystemExit(f"FAIL temporal-quality decoded size {name}")
    if (width, height) != (pixel_w, pixel_h):
        raise SystemExit(f"FAIL temporal-quality extent drift {name}={width}x{height}")
    return width, height, raw

images = {name: load(name) for name in names}
width, height, _ = images[names[0]]
stride = max(1, width // 640)
# Competitive-scene region only: console notify text is top-authored UI and the
# status cards are bottom-authored UI. Both are intentionally composed after
# temporal resolve and must not contaminate a world-quality measurement.
xs = range(width // 20, width * 19 // 20, stride)
ys = range(height // 5, height * 3 // 4, stride)

def mae(a, b):
    ar, br = images[a][2], images[b][2]
    total = count = 0
    for y in ys:
        row = y * width * 3
        for x in xs:
            i = row + x * 3
            total += abs(ar[i] - br[i]) + abs(ar[i+1] - br[i+1]) + abs(ar[i+2] - br[i+2])
            count += 3
    return total / count

def edge_energy(name):
    raw = images[name][2]
    total = count = 0
    for y in range(height // 5, height * 3 // 4 - stride, stride):
        row = y * width * 3
        next_row = (y + stride) * width * 3
        for x in range(width // 20, width * 19 // 20 - stride, stride):
            i = row + x * 3
            right = i + stride * 3
            down = next_row + x * 3
            l = (54 * raw[i] + 183 * raw[i+1] + 19 * raw[i+2]) / 256.0
            lr = (54 * raw[right] + 183 * raw[right+1] + 19 * raw[right+2]) / 256.0
            ld = (54 * raw[down] + 183 * raw[down+1] + 19 * raw[down+2]) / 256.0
            total += abs(l - lr) + abs(l - ld)
            count += 2
    return total / count

old = images["temporal_taa_a_2"][2]
first = images["temporal_taa_b_first"][2]
settled = images["temporal_taa_b_steady"][2]
changed = ghosts = 0
for y in ys:
    row = y * width * 3
    for x in xs:
        i = row + x * 3
        axis = [old[i+c] - settled[i+c] for c in range(3)]
        axis2 = sum(value * value for value in axis)
        if axis2 < 3 * 18 * 18:
            continue
        changed += 1
        residual = [first[i+c] - settled[i+c] for c in range(3)]
        projection = sum(residual[c] * axis[c] for c in range(3)) / axis2
        residual2 = sum(value * value for value in residual)
        if 0.15 < projection < 1.25 and residual2 > 3 * 10 * 10:
            ghosts += 1
if changed < 1000:
    raise SystemExit(f"FAIL temporal-quality camera pan witness too small ({changed})")
ghost_ratio = ghosts / changed

static_baseline_delta = mae("temporal_off_a", "temporal_off_a_2")
static_delta = mae("temporal_taa_a_1", "temporal_taa_a_2")
static_excess = max(0.0, static_delta - static_baseline_delta)
# a_1 and a_2 are 8 frames apart, one full jitter cycle (RAL_TEMPORAL_JITTER_PHASES): they share a jitter phase, so that delta cannot
# see an output that moves with the jitter. a_3 is the very next frame after a_2; report its change (MAE and the share of sampled
# pixels changing by more than 8/255) as the frame-to-frame stability a player sees.
consecutive_delta = mae("temporal_taa_a_2", "temporal_taa_a_3")
def changed_share(a, b, threshold=8):
    ar, br = images[a][2], images[b][2]
    changed = total = 0
    for y in ys:
        row = y * width * 3
        for x in xs:
            i = row + x * 3
            total += 1
            if max(abs(ar[i] - br[i]), abs(ar[i+1] - br[i+1]), abs(ar[i+2] - br[i+2])) > threshold:
                changed += 1
    return changed / total if total else 0.0
consecutive_share = changed_share("temporal_taa_a_2", "temporal_taa_a_3")
# a_2 .. a_10 are nine consecutive frames, so their eight transitions cover every jitter phase once: the mean is the stable
# frame-to-frame figure (a single pair depends on which two phases it happened to catch).
phase_pairs = [(f"temporal_taa_a_{i}", f"temporal_taa_a_{i + 1}") for i in range(2, 10)]
cycle_delta = statistics.mean(mae(a, b) for a, b in phase_pairs)
cycle_share = statistics.mean(changed_share(a, b) for a, b in phase_pairs)
consecutive_baseline = mae("temporal_off_a_2", "temporal_off_a_3")
motion_delta = mae("temporal_taa_a_2", "temporal_taa_b_steady")
first_settled_delta = mae("temporal_taa_b_first", "temporal_taa_b_steady")
reference_delta = mae("temporal_off_b", "temporal_taa_b_steady")
edge_off = edge_energy("temporal_off_b")
edge_taa = edge_energy("temporal_taa_b_steady")
edge_ratio = edge_taa / edge_off if edge_off else 0.0

if static_excess > 2.0:
    raise SystemExit(
        f"FAIL temporal-quality excess static shimmer {static_excess:.3f} > 2.000 "
        f"(AA-off baseline={static_baseline_delta:.3f}, TAA={static_delta:.3f})")
if motion_delta < 4.0:
    raise SystemExit(f"FAIL temporal-quality camera pan is not visually distinct ({motion_delta:.3f})")
if ghost_ratio > 0.05:
    raise SystemExit(f"FAIL temporal-quality old-frame residual {ghost_ratio:.2%} > 5.00%")
if not 0.65 <= edge_ratio <= 1.25:
    raise SystemExit(f"FAIL temporal-quality readability edge ratio {edge_ratio:.3f} outside [0.65,1.25]")

marker_pattern = re.compile(r"Q3_TEMPORAL_PERF_(OFF|ON|SMAA)_([12])_(BEGIN|END)")
header_pattern = re.compile(r"gpu \(([0-9]+)f avg, ms\):")
label_pattern = re.compile(r"  ([A-Za-z_]+)=([0-9.]+)")
buckets = {}
active = current = None
for row, message in zip(rows, messages):
    match = marker_pattern.fullmatch(message)
    if match:
        key = (match.group(1).lower(), int(match.group(2)))
        if match.group(3) == "BEGIN":
            if active is not None or key in buckets:
                raise SystemExit("FAIL temporal-quality performance marker nesting")
            active, current = key, {"begin": datetime.fromisoformat(row["ts"]), "gpu": []}
        else:
            if active != key or current is None:
                raise SystemExit("FAIL temporal-quality performance marker order")
            current["end"] = datetime.fromisoformat(row["ts"])
            buckets[key] = current
            active = current = None
        continue
    if active is None or row.get("cat", "").lower() != "renderer.timing":
        continue
    match = header_pattern.fullmatch(message)
    if match:
        current["gpu"].append({"frames": int(match.group(1))})
        continue
    if current["gpu"]:
        match = label_pattern.fullmatch(message)
        if match:
            current["gpu"][-1][match.group(1)] = float(match.group(2))

# Palindromic order (off, TAA, SMAA, SMAA, TAA, off) at one fixed, pinned view: TAA and SMAA are measured next to each other, so
# a GPU clock/presentation regime that drifts during the run hits both alike. The view used to be wherever the quality pan
# stopped (real-time +right with fixedtime 0), a different yaw on every run.
# The TAA arms use the product setting (r_aa 1), not the r_temporalInputTest hook the quality captures use: after a live switch
# from SMAA that hook's TAA window read 0.2-0.7 ms more resolve time than the one before it (5 of 6 launches), while r_aa 1 reads
# the same after SMAA as after TAA.
expected = [("off", 1), ("on", 1), ("smaa", 1), ("smaa", 2), ("on", 2), ("off", 2)]
if list(buckets) != expected:
    raise SystemExit(f"FAIL temporal-quality performance bucket order {list(buckets)}")
for key, bucket in buckets.items():
    complete = [sample for sample in bucket["gpu"] if sample.get("frames") == 200 and "total" in sample]
    if len(complete) != 1:
        raise SystemExit(f"FAIL temporal-quality GPU aggregate {key}: {complete}")
    bucket["lanes"] = complete[0]
# Budget lanes. Totals are paced by presentation on this machine (present_prep, the interval that ends at presentation, absorbs the
# wait for the next drawable: 3.4 ms of a ~4.1 ms AA-off total), so the budget compares the anti-aliasing work itself, each mode
# measured in the same run: its own passes plus what it adds to the scene lanes over AA off (TAA's motion-target passes).
#   TAA:  temporal_resolve + temporal_store (the "temporal_pre" lane is the post chain before it, not AA work)
#   SMAA: smaa_passes (copy, edges, blend weights, resolve; the "smaa" lane before it is the post chain)
scene_lanes = ("particle_compute", "atmosphere_compute", "dlight_shadow_start", "dlight_shadow", "opaque_to_depth",
    "depth_transfer", "gtao_denoise", "lens_depth")
def lane_values(mode, names):
    values = []
    for i in (1, 2):
        lanes = buckets[(mode, i)]["lanes"]
        missing = [name for name in names if name not in lanes]
        if missing:
            raise SystemExit(f"FAIL temporal-quality GPU aggregate {mode}{i} misses lanes {missing}")
        values.append(sum(lanes[name] for name in names))
    return values
def lane_mean(mode, names):
    return statistics.mean(lane_values(mode, names))
scene_off = lane_mean("off", scene_lanes)
on_ms = lane_mean("on", ("temporal_resolve", "temporal_store")) + lane_mean("on", scene_lanes) - scene_off
smaa_ms = lane_mean("smaa", ("smaa_passes",)) + lane_mean("smaa", scene_lanes) - scene_off
off_ms = scene_off
delta_ms = on_ms - smaa_ms
ratio = on_ms / smaa_ms if smaa_ms > 0 else float("inf")
# Same-regime check: the two windows of every arm must agree. On this machine the same binary switches between GPU
# clock/presentation regimes on a minutes timescale and every lane scales with it (TAA about 2x, SMAA about 1.6x); a run whose
# arms straddle a switch measures the switch, not the anti-aliasing, and is reported as inconclusive (exit 75), not PASS/FAIL.
regime_limit = float(os.environ.get("TQ_REGIME_SPREAD", "0.20"))
spreads = {}
for mode, names in (("off", scene_lanes), ("on", ("temporal_resolve", "temporal_store") + scene_lanes),
        ("smaa", ("smaa_passes",) + scene_lanes)):
    a, b = lane_values(mode, names)
    spreads[mode] = abs(a - b) / max(1e-6, 0.5 * (a + b))
# The quality criteria above already passed (each raises on failure); report them even when the budget below fails.
print(
    f"PASS temporal quality: static AA-off={static_baseline_delta:.3f} TAA={static_delta:.3f} "
    f"excess={static_excess:.3f}, consecutive AA-off={consecutive_baseline:.3f} TAA={consecutive_delta:.3f} "
    f"({consecutive_share:.2%} px >8/255), jitter-cycle mean TAA={cycle_delta:.3f} ({cycle_share:.2%} px >8/255), "
    f"pan={motion_delta:.3f}, "
    f"first-settled={first_settled_delta:.3f}, old-frame-residual={ghost_ratio:.2%}, history-roi={history_share:.1%}, "
    f"edge-ratio={edge_ratio:.3f}, AA-off-reference-delta={reference_delta:.3f}")
regime_text = " ".join(f"{mode}={spread:.1%}" for mode, spread in spreads.items())
consistent = 1 if max(spreads.values()) <= regime_limit else 0
# One launch's budget sample; the verdict is taken over several launches (budget() below).
print(f"TQ_BUDGET_SAMPLE on={on_ms:.4f} smaa={smaa_ms:.4f} off={off_ms:.4f} ratio={ratio:.4f} consistent={consistent} "
    f"spread={regime_text}")
PYEOF
}

# Budget (owner, 2026-11-24 / 2026-11-26): TAA's anti-aliasing work must not exceed SMAA's by more than TQ_SMAA_MARGIN_MS, judged
# on the median over launches. One launch cannot decide it: the same binary at the same pinned view gives TAA-SMAA from -0.24 to
# +0.61 ms on different launches while the route, the exact-draw share and the history are identical in every launch (the
# variation is per-launch GPU state; 8 launches, TASK-272). A launch whose arms straddle a GPU regime change (its two windows of
# one arm differ by more than TQ_REGIME_SPREAD) is not counted; with fewer than half of the launches (and at least one) left,
# the gate is inconclusive (exit 75), not PASS/FAIL.
budget() {
	python3 - "$@" <<'PYEOF'
import os, re, statistics, sys
pattern = re.compile(r"TQ_BUDGET_SAMPLE on=([0-9.]+) smaa=([0-9.]+) off=([0-9.]+) ratio=([0-9.inf]+) consistent=([01]) spread=(.*)")
samples = []
for path in sys.argv[1:]:
    found = [pattern.fullmatch(line.strip()) for line in open(path, encoding="utf-8")]
    found = [m for m in found if m]
    if len(found) != 1:
        raise SystemExit(f"FAIL temporal-quality budget sample missing in {path}")
    m = found[0]
    samples.append({"on": float(m.group(1)), "smaa": float(m.group(2)), "off": float(m.group(3)),
        "consistent": m.group(5) == "1", "spread": m.group(6)})
for i, x in enumerate(samples, 1):
    print(f"  launch {i}: TAA={x['on']:.3f}ms SMAA={x['smaa']:.3f}ms TAA-SMAA={x['on'] - x['smaa']:+.3f}ms "
        f"AA-off scene lanes {x['off']:.3f}ms window spread {x['spread']}{'' if x['consistent'] else ' (regime change: not counted)'}")
used = [x for x in samples if x["consistent"]]
if not used or len(used) * 2 < len(samples):
    print(f"INCONCLUSIVE temporal-quality GPU budget: {len(used)} of {len(samples)} launches are regime-consistent; rerun")
    sys.exit(75)
margin = float(os.environ.get("TQ_SMAA_MARGIN_MS", "0.35"))
delta = statistics.median(x["on"] - x["smaa"] for x in used)
on = statistics.median(x["on"] for x in used); smaa = statistics.median(x["smaa"] for x in used)
ratio = statistics.median(x["on"] / x["smaa"] for x in used if x["smaa"] > 0)
summary = (f"median over {len(used)} of {len(samples)} launches: TAA={on:.3f}ms SMAA={smaa:.3f}ms TAA-SMAA={delta:+.3f}ms "
    f"ratio={ratio:.3f} (margin {margin:.2f}ms)")
if delta > margin:
    raise SystemExit(f"FAIL temporal-quality GPU budget (anti-aliasing work), {summary}")
print(f"PASS temporal GPU budget (anti-aliasing work), {summary}")
PYEOF
}

if [ "${1:-}" = --analyze ]; then
	[ -f "${2:-}" ] && [ -d "${3:-}" ] || {
		echo "usage: $0 --analyze qconsole.jsonl screenshots-dir"; exit 64; }
	SAMPLE="$(mktemp -t ral-temporal-quality-sample-XXXXXX)"
	analyze "$2" "$3" | tee "$SAMPLE"
	RC=${PIPESTATUS[0]}
	[ "$RC" -eq 0 ] && { budget "$SAMPLE"; RC=$?; }
	rm -f "$SAMPLE"
	exit "$RC"
fi

WIRED="${1:-${WIRED_BINARY:-}}"
[ -n "$WIRED" ] && [ -x "$WIRED" ] || {
	echo "usage: $0 /absolute/path/to/wired"; exit 64; }
[ "$(uname -s)" = Darwin ] || {
	echo "SKIP: runtime gate requires macOS/MoltenVK"; exit 77; }
[ -x "$PNG2RAW" ] || { echo "SKIP: png2raw unavailable"; exit 77; }
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
RENDERER="$(find_required wired_vulkan_arm64.dylib "$WD" "$WD/../MacOS")" || {
	echo "SKIP: current Vulkan renderer unavailable"; exit 77; }
GAMECL="$(find_required gameclarm64.dylib "$WD/base" "$WD/../Resources/base" "$WIRED_SOURCE/build/base")" || {
	echo "SKIP: current native gamecl unavailable"; exit 77; }
GAMESV="$(find_required gamesvarm64.dylib "$WD/base" "$WD/../Resources/base" "$WIRED_SOURCE/build/base")" || {
	echo "SKIP: current native gamesv unavailable"; exit 77; }
PAX21="$(find_required pax21.sw3z "$WD/base" "$WD/../Resources/base" "$WIRED_SOURCE/build/base")" || {
	echo "SKIP: current pax21 unavailable"; exit 77; }
[ -f "$WIRED_BASE/pax01.sw3z" ] || {
	echo "SKIP: licensed base content unavailable in $WIRED_BASE"; exit 77; }

LAUNCHES="${TQ_LAUNCHES:-6}"
case "$LAUNCHES" in ''|*[!0-9]*|0) echo "usage: TQ_LAUNCHES must be a positive integer"; exit 64 ;; esac
ROOT="$(mktemp -d -t ral-temporal-quality-XXXXXX 2>/dev/null || mktemp -d)"
cleanup() {
	local status=$?; trap - EXIT INT TERM
	if [ "${WIRED_KEEP_ARTIFACTS:-0}" = 1 ] || [ "$status" -ne 0 ]; then
		echo "retained root: $ROOT"
	else
		rm -rf "$ROOT"
	fi
	exit "$status"
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM

# One launch: its own home, the quality capture, the six budget windows and the analysis (quality verdict + budget sample).
launch() {
HOME_DIR="$ROOT/launch-$1/home"
SHOT_DIR="$HOME_DIR/base/screenshots"
mkdir -p "$SHOT_DIR"
wired_link_content_into_home "$HOME_DIR" "$WIRED_BASE" || return 1
ln -sfn "$PAX21" "$HOME_DIR/base/pax21.sw3z"
ln -sfn "$GAMECL" "$HOME_DIR/base/gameclarm64.dylib"
ln -sfn "$GAMESV" "$HOME_DIR/base/gamesvarm64.dylib"

# setviewpos spawns cgame's teleport-in effect (models/misc/telep.md3, 1200 ms) around the camera. r_pinFrameTime freezes cg.time,
# so under the pin that local entity never expires; each setviewpos is therefore followed by a short unpinned stretch of fixed
# 20 ms game frames (1.8 s) that lets it expire, then the same pin again (the captured frame stays byte-stable).
BOOT="$HOME_DIR/base/ral-temporal-quality.cfg"
cat >"$BOOT" <<'CFGEOF'
set r_fullscreen 0
set r_mode -1
set r_customwidth 1280
set r_customheight 720
log renderer.temporal info
log renderer.ral info
log renderer.timing debug
set vm_game 0
set vm_cgame 0
set sv_cheats 1
set sv_pure 0
set tqSetup "cg_draw2D 0; cg_drawGun 0; cg_thirdPerson 0; con_notifytime 0; cl_yawspeed 90; sv_fps 125; timescale 1; fixedtime 0; r_pinShaderTime 2.0; r_pinFrameTime 2.0; wait 512; noclip; wait 150; echo Q3_TEMPORAL_QUALITY_BEGIN; setviewpos 888 -768 434 180 0; r_temporalInputTest 0; r_pinFrameTime 0; fixedtime 20; wait 90; fixedtime 0; r_pinFrameTime 2.0; wait 96; screenshot temporal_off_a png silent; wait 8; screenshot temporal_off_a_2 png silent; wait 1; screenshot temporal_off_a_3 png silent; wait 4; +right; wait 12; -right; wait 8; screenshot temporal_off_b png silent; wait 4; vstr tqTemporalA"
set tqTemporalA "r_temporalInputTest 1; setviewpos 888 -768 434 180 0; r_pinFrameTime 0; fixedtime 20; wait 90; fixedtime 0; r_pinFrameTime 2.0; wait 96; screenshot temporal_taa_a_1 png silent; wait 8; screenshot temporal_taa_a_2 png silent; wait 1; screenshot temporal_taa_a_3 png silent; wait 1; screenshot temporal_taa_a_4 png silent; wait 1; screenshot temporal_taa_a_5 png silent; wait 1; screenshot temporal_taa_a_6 png silent; wait 1; screenshot temporal_taa_a_7 png silent; wait 1; screenshot temporal_taa_a_8 png silent; wait 1; screenshot temporal_taa_a_9 png silent; wait 1; screenshot temporal_taa_a_10 png silent; ral_dump live temporal-motion-arm; wait 4; +right; wait 12; -right; wait 4; screenshot temporal_taa_b_first png silent; wait 96; screenshot temporal_taa_b_steady png silent; wait 4; echo Q3_TEMPORAL_QUALITY_CAPTURED; vstr tqPerfView"
set tqPerfView "set r_aa 0; r_temporalInputTest 0; setviewpos 888 -768 434 180 0; r_pinFrameTime 0; fixedtime 20; wait 90; fixedtime 0; r_pinFrameTime 2.0; wait 16; echo Q3_TEMPORAL_PERF_VIEW; vstr tqPerfOff1"
set tqPerfOff1 "set r_aa 0; r_temporalInputTest 0; wait 96; set r_gpuSpeeds 0; wait 4; set r_gpuSpeeds 1; echo Q3_TEMPORAL_PERF_OFF_1_BEGIN; wait 220; echo Q3_TEMPORAL_PERF_OFF_1_END; set r_gpuSpeeds 0; wait 16; vstr tqPerfOn1"
set tqPerfOn1 "r_temporalInputTest 0; set r_aa 1; wait 96; ral_dump live temporal-motion-arm; wait 16; set r_gpuSpeeds 1; echo Q3_TEMPORAL_PERF_ON_1_BEGIN; wait 220; echo Q3_TEMPORAL_PERF_ON_1_END; set r_gpuSpeeds 0; wait 16; vstr tqPerfSmaa1"
set tqPerfSmaa1 "r_temporalInputTest 0; set r_aa 2; wait 96; set r_gpuSpeeds 1; echo Q3_TEMPORAL_PERF_SMAA_1_BEGIN; wait 220; echo Q3_TEMPORAL_PERF_SMAA_1_END; set r_gpuSpeeds 0; wait 16; vstr tqPerfSmaa2"
set tqPerfSmaa2 "r_temporalInputTest 0; set r_aa 2; wait 96; set r_gpuSpeeds 1; echo Q3_TEMPORAL_PERF_SMAA_2_BEGIN; wait 220; echo Q3_TEMPORAL_PERF_SMAA_2_END; set r_gpuSpeeds 0; wait 16; vstr tqPerfOn2"
set tqPerfOn2 "r_temporalInputTest 0; set r_aa 1; wait 96; set r_gpuSpeeds 1; echo Q3_TEMPORAL_PERF_ON_2_BEGIN; wait 220; echo Q3_TEMPORAL_PERF_ON_2_END; set r_gpuSpeeds 0; wait 16; vstr tqPerfOff2"
set tqPerfOff2 "set r_aa 0; r_temporalInputTest 0; wait 96; set r_gpuSpeeds 1; echo Q3_TEMPORAL_PERF_OFF_2_BEGIN; wait 220; echo Q3_TEMPORAL_PERF_OFF_2_END; set r_gpuSpeeds 0; echo Q3_TEMPORAL_QUALITY_COMPLETE; quit"
set activeAction "vstr tqSetup"
map arena6
CFGEOF

QCONSOLE="$HOME_DIR/qconsole.jsonl"
STDOUT="$ROOT/launch-$1/stdout.log"
python3 "$TIMEOUT_RUNNER" --timeout 180 --kill-after 15 --cwd "$WD" --stdout "$STDOUT" -- \
	"$WIRED" +set fs_basepath "$HOME_DIR" +set fs_homepath "$HOME_DIR" +set fs_game base \
	+set com_automated 1 +set com_noHardReboot 1 +set s_initsound 0 \
	+set r_fullscreen 0 +set r_mode -1 +set r_customwidth 1280 +set r_customheight 720 \
	+set r_vkValidate 0 +set r_temporalInputTest 0 +set r_aa 0 \
	+set r_bloom 0 +set r_ao 0 +set r_forwardPlus 0 +set r_drawSunRays 0 \
	+set r_shadows 0 +set r_depthFade 0 +set r_lens 0 +set r_gpuDecals 0 \
	+set r_particles 0 +set r_atmosphericGPU 0 ${RAL_TEMPORAL_EXTRA_ARGS:-} +set log_severity DEBUG \
	+set log_file_severity DEBUG +set log_file_mode overwrite_synced \
	+exec ral-temporal-quality.cfg
RC=$?
[ "$RC" -eq 0 ] || { echo "FAIL temporal-quality process rc=$RC (launch $1)"; return 1; }
[ -f "$QCONSOLE" ] || { echo "FAIL temporal-quality missing qconsole (launch $1)"; return 1; }
analyze "$QCONSOLE" "$SHOT_DIR" > "$ROOT/launch-$1/analysis.log"
RC=$?
cat "$ROOT/launch-$1/analysis.log"
return "$RC"
}

SAMPLES=()
for (( n = 1; n <= LAUNCHES; n++ )); do
	echo "==> temporal quality launch $n/$LAUNCHES"
	launch "$n" || exit 1
	SAMPLES+=("$ROOT/launch-$n/analysis.log")
done
budget "${SAMPLES[@]}"
