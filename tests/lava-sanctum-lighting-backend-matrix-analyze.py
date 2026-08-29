#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Wired Engine contributors

import hashlib
import json
import pathlib
import struct
import subprocess
import sys

if len(sys.argv) != 5:
    raise SystemExit("usage: analyzer OUT PNG2RAW OPENGL_STATUS WEBGPU_STATUS")

root = pathlib.Path(sys.argv[1])
decoder = pathlib.Path(sys.argv[2])
matrix = {}
model_roi = (550, 780, 230, 475)
for backend in ("vulkan", "metal"):
    image = root / backend / "arena7.png"
    log = root / backend / "arena7.log"
    data = image.read_bytes() if image.is_file() else b""
    if len(data) < 24 or data[:8] != b"\x89PNG\r\n\x1a\n" or data[12:16] != b"IHDR":
        raise SystemExit(f"FAIL: {backend} capture missing or malformed")
    if struct.unpack(">II", data[16:24]) != (1280, 720):
        raise SystemExit(f"FAIL: {backend} capture is not 1280x720")
    raw = subprocess.run([str(decoder), str(image)], check=True,
                         stdout=subprocess.PIPE).stdout
    if len(raw) != 1280 * 720 * 3:
        raise SystemExit(f"FAIL: {backend} decoded byte count")
    luma = [(raw[i] * 54 + raw[i + 1] * 183 + raw[i + 2] * 19) >> 8
            for i in range(0, len(raw), 3)]
    levels = len(set(luma))
    dynamic_range = max(luma) - min(luma)
    if levels < 32 or dynamic_range < 48:
        raise SystemExit(f"FAIL: {backend} capture is blank/degenerate")
    lava_pixels = []
    for y in range(360, 720):
        for x in range(1280):
            offset = (y * 1280 + x) * 3
            red, green, blue = raw[offset:offset + 3]
            if red > 80 and red * 4 > green * 5 and red * 5 > blue * 8:
                lava_pixels.append((x, y))
    if len(lava_pixels) < 50000:
        raise SystemExit(f"FAIL: {backend} camera ROI misses the authored lava pool")
    lava_bounds = [min(x for x, _ in lava_pixels), min(y for _, y in lava_pixels),
                   max(x for x, _ in lava_pixels), max(y for _, y in lava_pixels)]
    if lava_bounds[2] - lava_bounds[0] < 500 or lava_bounds[3] - lava_bounds[1] < 150:
        raise SystemExit(f"FAIL: {backend} lava ROI is clipped or obstructed")
    model_values = []
    for y in range(model_roi[2], model_roi[3]):
        for x in range(model_roi[0], model_roi[1]):
            offset = (y * 1280 + x) * 3
            model_values.extend(raw[offset:offset + 3])
    model_mean = sum(model_values) / len(model_values)
    if model_mean < 8.0:
        raise SystemExit(f"FAIL: {backend} moving-entity ROI is unreadably dark")
    text = log.read_text(errors="replace") if log.is_file() else ""
    forbidden = ("failed to initialize", "renderer fallback", "VUID-", "validation error",
                 "lifecycle failure", "can't register model", "model rejected")
    if any(term.lower() in text.lower() for term in forbidden):
        raise SystemExit(f"FAIL: {backend} runtime log contains failure evidence")
    receipts = [line for line in text.splitlines() if "irradiance receipt" in line]
    if not receipts or not any("volumes=1" in line and "entities=0" not in line
                               for line in receipts):
        raise SystemExit(f"FAIL: {backend} lacks positive local SH receipt evidence")
    matrix[backend] = {
        "status": "pass", "fixture": "lava-sanctum-v1", "map": "arena7",
        "camera": [650, -500, -250, 225, 10], "extent": [1280, 720],
        "sha256": hashlib.sha256(data).hexdigest(), "lumaLevels": levels,
        "dynamicRange": dynamic_range, "lavaPixels": len(lava_pixels),
        "lavaBounds": lava_bounds, "movingEntityRoiMean": model_mean,
    }

if abs(matrix["vulkan"]["movingEntityRoiMean"] -
       matrix["metal"]["movingEntityRoiMean"]) > 5.0:
    raise SystemExit("FAIL: Vulkan/Metal moving-entity probe ROI parity drift")

for backend, value in (("opengl46", sys.argv[3]), ("webgpu", sys.argv[4])):
    if not value.startswith("skip:") or len(value) <= 5:
        raise SystemExit(f"FAIL: {backend} skip must carry a concrete host reason")
    matrix[backend] = {"status": "skip", "fixture": "lava-sanctum-v1",
                       "reason": value[5:]}

result = {"schemaVersion": 1, "fixture": "lava-sanctum-v1",
          "authoredFixtureShared": True, "backends": matrix}
(root / "matrix.json").write_text(json.dumps(result, indent=2, sort_keys=True) + "\n")
print("PASS: lava-sanctum backend matrix: Vulkan+Metal captured; OpenGL46+WebGPU explicit host skips")
