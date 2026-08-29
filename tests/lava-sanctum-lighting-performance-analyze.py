#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Wired Engine contributors

import json
import pathlib
import re
import statistics
import sys

if len(sys.argv) != 3:
    raise SystemExit("usage: analyzer QCONSOLE OUTPUT_JSON")

rows = []
for number, line in enumerate(pathlib.Path(sys.argv[1]).read_text().splitlines(), 1):
    try:
        row = json.loads(line)
    except Exception as exc:
        raise SystemExit(f"FAIL: qconsole JSON line {number}: {exc}")
    if isinstance(row, dict) and isinstance(row.get("msg"), str):
        rows.append(row)

messages = [row["msg"].strip() for row in rows]
try:
    begin = messages.index("LAVA_LIGHTING_PERF_BEGIN")
    end = messages.index("LAVA_LIGHTING_PERF_END")
except ValueError as exc:
    raise SystemExit(f"FAIL: performance marker missing: {exc}")
if begin >= end:
    raise SystemExit("FAIL: performance marker order")
window = rows[begin + 1:end]
if any(row.get("sev", "").upper() in ("ERROR", "FATAL") or "VUID" in row["msg"] for row in window):
    raise SystemExit("FAIL: validation/error evidence in performance window")

perf_re = re.compile(r"vk perf v2: epoch=(\d+) bucket=(\d+) count=200 valid=200 .*draws_total=(\d+) .*identity_stable=1")
frame_re = re.compile(r"frame trace \(200f\): bucket=(\d+) count=200 valid=200 cpu_work_total=(\d+)us scr_end_total=(\d+)us")
lane_re = re.compile(r"([a-z0-9_]+)=([0-9]+\.[0-9]{3})")
perf = [perf_re.fullmatch(row["msg"].strip()) for row in window]
perf = [match for match in perf if match]
frames = [frame_re.fullmatch(row["msg"].strip()) for row in window]
frames = [match for match in frames if match]
if len(perf) != 3 or len(frames) != 3:
    raise SystemExit(f"FAIL: Release N=3 cardinality perf={len(perf)} frame={len(frames)}")
if len({match.group(1) for match in perf}) != 1:
    raise SystemExit("FAIL: topology epoch drift")
if [int(match.group(2)) for match in perf] != [int(match.group(1)) for match in frames]:
    raise SystemExit("FAIL: perf/frame bucket join")

gpu_runs = []
current = None
for row in window:
    message = row["msg"].strip()
    if message == "gpu (200f avg, ms):":
        if current is not None:
            raise SystemExit("FAIL: nested GPU profile")
        current = {}
        continue
    if current is None:
        continue
    match = lane_re.fullmatch(message)
    if not match:
        continue
    current[match.group(1)] = float(match.group(2))
    if match.group(1) == "total":
        gpu_runs.append(current)
        current = None
if len(gpu_runs) != 3 or any("present_prep" not in run or len(run) < 3 for run in gpu_runs):
    raise SystemExit(f"FAIL: per-pass GPU N=3 cardinality/lanes: {gpu_runs}")

cpu_ms = [int(match.group(2)) / 200000.0 for match in frames]
fps = [1000.0 / value for value in cpu_ms]
gpu_ms = [run["total"] for run in gpu_runs]
if min(fps) < 250.0 or max(cpu_ms) > 4.0:
    raise SystemExit(f"FAIL: 250 FPS / 4 ms CPU gate: fps={fps} cpu_ms={cpu_ms}")
if max(gpu_ms) > 4.0:
    raise SystemExit(f"FAIL: 4 ms GPU gate: {gpu_ms}")

result = {
    "schemaVersion": 1,
    "fixture": "lava-sanctum",
    "build": "Release",
    "runs": 3,
    "fps": fps,
    "cpuFrameMilliseconds": cpu_ms,
    "gpuFrameMilliseconds": gpu_ms,
    "medianFps": statistics.median(fps),
    "perPassGpuMilliseconds": gpu_runs,
}
pathlib.Path(sys.argv[2]).write_text(json.dumps(result, indent=2, sort_keys=True) + "\n")
print(f"PASS: lava-sanctum Release N=3 fps={[round(v, 1) for v in fps]} gpu-ms={gpu_ms}")
