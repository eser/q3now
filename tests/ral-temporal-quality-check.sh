#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# 1280x720 temporal quality + full-path GPU budget acceptance gate.

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
. "$SCRIPT_DIR/lib/wired_paths.sh"

TIMEOUT_RUNNER="$SCRIPT_DIR/run-with-timeout.py"
PNG2RAW="${PNG2RAW:-$WIRED_SOURCE/tools/png2raw/png2raw}"

analyze() {
	python3 - "$1" "$2" "$PNG2RAW" <<'PYEOF'
import json, math, re, statistics, struct, subprocess, sys
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
    "Q3_TEMPORAL_PERF_OFF_1_BEGIN", "Q3_TEMPORAL_PERF_OFF_1_END",
    "Q3_TEMPORAL_PERF_ON_1_BEGIN", "Q3_TEMPORAL_PERF_ON_1_END",
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

names = (
    "temporal_smaa_a", "temporal_smaa_a_2", "temporal_smaa_b", "temporal_taa_a_1",
    "temporal_taa_a_2", "temporal_taa_b_first", "temporal_taa_b_steady",
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

static_reference_delta = mae("temporal_smaa_a", "temporal_smaa_a_2")
static_delta = mae("temporal_taa_a_1", "temporal_taa_a_2")
static_excess = max(0.0, static_delta - static_reference_delta)
motion_delta = mae("temporal_taa_a_2", "temporal_taa_b_steady")
first_settled_delta = mae("temporal_taa_b_first", "temporal_taa_b_steady")
reference_delta = mae("temporal_smaa_b", "temporal_taa_b_steady")
edge_smaa = edge_energy("temporal_smaa_b")
edge_taa = edge_energy("temporal_taa_b_steady")
edge_ratio = edge_taa / edge_smaa if edge_smaa else 0.0

if static_excess > 2.0:
    raise SystemExit(
        f"FAIL temporal-quality excess static shimmer {static_excess:.3f} > 2.000 "
        f"(SMAA={static_reference_delta:.3f}, TAA={static_delta:.3f})")
if motion_delta < 4.0:
    raise SystemExit(f"FAIL temporal-quality camera pan is not visually distinct ({motion_delta:.3f})")
if ghost_ratio > 0.05:
    raise SystemExit(f"FAIL temporal-quality old-frame residual {ghost_ratio:.2%} > 5.00%")
if not 0.65 <= edge_ratio <= 1.25:
    raise SystemExit(f"FAIL temporal-quality readability edge ratio {edge_ratio:.3f} outside [0.65,1.25]")

marker_pattern = re.compile(r"Q3_TEMPORAL_PERF_(OFF|ON)_([12])_(BEGIN|END)")
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

expected = [("off", 1), ("on", 1), ("on", 2), ("off", 2)]
if list(buckets) != expected:
    raise SystemExit(f"FAIL temporal-quality performance bucket order {list(buckets)}")
for key, bucket in buckets.items():
    complete = [sample for sample in bucket["gpu"] if sample.get("frames") == 200 and "total" in sample]
    if len(complete) != 1:
        raise SystemExit(f"FAIL temporal-quality GPU aggregate {key}: {complete}")
    bucket["total"] = complete[0]["total"]
off_ms = statistics.mean(buckets[("off", i)]["total"] for i in (1, 2))
on_ms = statistics.mean(buckets[("on", i)]["total"] for i in (1, 2))
delta_ms = on_ms - off_ms
delta_percent = delta_ms / off_ms * 100.0
if delta_ms > 0.75 or on_ms > 4.0:
    raise SystemExit(
        f"FAIL temporal-quality GPU budget off={off_ms:.3f}ms on={on_ms:.3f}ms "
        f"delta={delta_ms:+.3f}ms/{delta_percent:+.2f}%")

print(
    f"PASS temporal quality: static SMAA={static_reference_delta:.3f} TAA={static_delta:.3f} "
    f"excess={static_excess:.3f}, pan={motion_delta:.3f}, "
    f"first-settled={first_settled_delta:.3f}, old-frame-residual={ghost_ratio:.2%}, "
    f"edge-ratio={edge_ratio:.3f}, SMAA-reference-delta={reference_delta:.3f}")
print(
    f"PASS temporal GPU budget: off={off_ms:.3f}ms on={on_ms:.3f}ms "
    f"delta={delta_ms:+.3f}ms/{delta_percent:+.2f}%")
PYEOF
}

if [ "${1:-}" = --analyze ]; then
	[ -f "${2:-}" ] && [ -d "${3:-}" ] || {
		echo "usage: $0 --analyze qconsole.jsonl screenshots-dir"; exit 64; }
	analyze "$2" "$3"
	exit $?
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

ROOT="$(mktemp -d -t ral-temporal-quality-XXXXXX 2>/dev/null || mktemp -d)"
HOME_DIR="$ROOT/home"
SHOT_DIR="$HOME_DIR/base/screenshots"
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
mkdir -p "$SHOT_DIR"
wired_link_content_into_home "$HOME_DIR" "$WIRED_BASE" || exit 1
ln -sfn "$PAX21" "$HOME_DIR/base/pax21.sw3z"
ln -sfn "$GAMECL" "$HOME_DIR/base/gameclarm64.dylib"
ln -sfn "$GAMESV" "$HOME_DIR/base/gamesvarm64.dylib"

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
set tqSetup "cg_draw2D 0; cg_drawGun 0; cg_thirdPerson 0; con_notifytime 0; cl_yawspeed 90; sv_fps 125; timescale 1; fixedtime 0; r_pinShaderTime 2.0; r_pinFrameTime 2.0; wait 512; noclip; echo Q3_TEMPORAL_QUALITY_BEGIN; setviewpos 888 -768 434 180 0; r_temporalInputTest 0; r_smaa 3; wait 96; screenshot temporal_smaa_a png silent; wait 8; screenshot temporal_smaa_a_2 png silent; +right; wait 12; -right; wait 8; screenshot temporal_smaa_b png silent; vstr tqTemporalA"
set tqTemporalA "r_smaa 0; r_temporalInputTest 1; setviewpos 888 -768 434 180 0; wait 96; screenshot temporal_taa_a_1 png silent; wait 8; screenshot temporal_taa_a_2 png silent; +right; wait 12; -right; wait 4; screenshot temporal_taa_b_first png silent; wait 96; screenshot temporal_taa_b_steady png silent; echo Q3_TEMPORAL_QUALITY_CAPTURED; vstr tqPerfOff1"
set tqPerfOff1 "r_temporalInputTest 0; wait 96; set r_gpuSpeeds 0; wait 4; set r_gpuSpeeds 1; echo Q3_TEMPORAL_PERF_OFF_1_BEGIN; wait 220; echo Q3_TEMPORAL_PERF_OFF_1_END; set r_gpuSpeeds 0; wait 16; vstr tqPerfOn1"
set tqPerfOn1 "r_temporalInputTest 1; wait 96; set r_gpuSpeeds 1; echo Q3_TEMPORAL_PERF_ON_1_BEGIN; wait 220; echo Q3_TEMPORAL_PERF_ON_1_END; set r_gpuSpeeds 0; wait 16; vstr tqPerfOn2"
set tqPerfOn2 "wait 96; set r_gpuSpeeds 1; echo Q3_TEMPORAL_PERF_ON_2_BEGIN; wait 220; echo Q3_TEMPORAL_PERF_ON_2_END; set r_gpuSpeeds 0; wait 16; vstr tqPerfOff2"
set tqPerfOff2 "r_temporalInputTest 0; wait 96; set r_gpuSpeeds 1; echo Q3_TEMPORAL_PERF_OFF_2_BEGIN; wait 220; echo Q3_TEMPORAL_PERF_OFF_2_END; set r_gpuSpeeds 0; echo Q3_TEMPORAL_QUALITY_COMPLETE; quit"
set activeAction "vstr tqSetup"
map arena6
CFGEOF

QCONSOLE="$HOME_DIR/qconsole.jsonl"
STDOUT="$ROOT/stdout.log"
python3 "$TIMEOUT_RUNNER" --timeout 180 --kill-after 15 --cwd "$WD" --stdout "$STDOUT" -- \
	"$WIRED" +set fs_basepath "$HOME_DIR" +set fs_homepath "$HOME_DIR" +set fs_game base \
	+set com_automated 1 +set com_noHardReboot 1 +set s_initsound 0 \
	+set r_fullscreen 0 +set r_mode -1 +set r_customwidth 1280 +set r_customheight 720 \
	+set r_vkValidate 0 +set r_entitySSBO 1 +set r_temporalInputTest 0 +set r_smaa 0 \
	+set r_bloom 0 +set r_ssao 0 +set r_forwardPlus 0 +set r_drawSunRays 0 \
	+set r_shadows 0 +set r_depthFade 0 +set r_lens 0 +set r_gpuDecals 0 \
	+set r_particles 0 +set r_atmosphericGPU 0 +set log_severity DEBUG \
	+set log_file_severity DEBUG +set log_file_mode overwrite_synced \
	+exec ral-temporal-quality.cfg
RC=$?
[ "$RC" -eq 0 ] || { echo "FAIL temporal-quality process rc=$RC"; exit 1; }
[ -f "$QCONSOLE" ] || { echo "FAIL temporal-quality missing qconsole"; exit 1; }
analyze "$QCONSOLE" "$SHOT_DIR"
