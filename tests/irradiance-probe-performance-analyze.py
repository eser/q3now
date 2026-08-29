#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Wired Engine contributors

import json
import pathlib
import re
import statistics
import subprocess
import sys

if len(sys.argv) != 4:
    raise SystemExit("usage: analyzer BENCHMARK LIGHTING_SUMMARY OUTPUT")

benchmark = pathlib.Path(sys.argv[1])
lighting_path = pathlib.Path(sys.argv[2])
output = pathlib.Path(sys.argv[3])
values = []
for _ in range(3):
    result = subprocess.run([str(benchmark), "--benchmark"], check=True,
                            capture_output=True, text=True)
    match = re.search(r"probeSampleNanoseconds=([0-9.]+)", result.stdout)
    if not match:
        raise SystemExit("FAIL: missing probe benchmark result")
    values.append(float(match.group(1)))

lighting = json.loads(lighting_path.read_text())
if lighting.get("build") != "Release" or lighting.get("runs") != 3:
    raise SystemExit("FAIL: probe gate requires Release N=3 lighting evidence")
if min(lighting["fps"]) < 250.0 or max(lighting["gpuFrameMilliseconds"]) > 4.0:
    raise SystemExit("FAIL: probe fixture exceeds 250 FPS / 4 ms budget")
if not all("total" in run and "present_prep" in run
           for run in lighting["perPassGpuMilliseconds"]):
    raise SystemExit("FAIL: per-pass GPU evidence missing")

summary = {
    "schemaVersion": 1,
    "fixture": "lava-sanctum-v1",
    "build": "Release",
    "runs": 3,
    "cpuProbeSampleNanoseconds": values,
    "medianCpuProbeSampleNanoseconds": statistics.median(values),
    "gpuDedicatedProbePassMilliseconds": 0.0,
    "gpuAuthority": "SH L1 is CPU-resolved per entity and consumed inline by the entity material; no dedicated GPU pass exists.",
    "fixtureGpuFrameMilliseconds": lighting["gpuFrameMilliseconds"],
    "fixtureFps": lighting["fps"],
}
output.write_text(json.dumps(summary, indent=2, sort_keys=True) + "\n")
print(f"PASS: Release N=3 SH probe CPU ns={values}; inline GPU path remains within fixture budget")
