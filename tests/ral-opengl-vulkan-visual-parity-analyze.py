#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors

"""Fail-closed native OpenGL 4.6 versus Vulkan visual evidence analyzer."""

from __future__ import annotations

import argparse
import json
import math
import os
import re
import struct
import subprocess
import sys
import tempfile
import zlib
from pathlib import Path
from typing import Dict, List, Mapping, Sequence, Tuple


BACKENDS = ("opengl", "vulkan")
OPENGL_REQUIRED = (
    "surfaces", "vertices", "indices", "assets", "materials",
    "resolvedMaterials", "materialBytes", "entities", "ui", "loweredWorld",
    "worldBatches", "loweredEntity", "modelEntities", "loweredUi",
    "texturedUi", "nativeDraws",
)
VULKAN_REQUIRED = (
    "worldBatches", "patchWorld", "materialWorld", "lightmappedWorld",
    "modelAssets", "entities", "ui",
)
FORBIDDEN = re.compile(
    r"lifecycle failure|advancing cl_renderer|failed to initialize|"
    r"renderer fallback|Sys_Error|\bFATAL\b(?!\s*=0)|\bVUID-|validation error|"
    r"VM_Create.*failed|VM syscall error|died on signal|Initializing Metal|MoltenVK",
    re.IGNORECASE,
)


class EvidenceError(RuntimeError):
    pass


def png_dimensions(path: Path) -> Tuple[int, int]:
    data = path.read_bytes()[:24]
    if len(data) != 24 or data[:8] != b"\x89PNG\r\n\x1a\n" or data[12:16] != b"IHDR":
        raise EvidenceError(f"invalid PNG header: {path}")
    return struct.unpack(">II", data[16:24])


def decode_rgb(path: Path, decoder: Path) -> bytes:
    command = [str(decoder), str(path)]
    if decoder.suffix == ".py":
        command.insert(0, sys.executable)
    completed = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if completed.returncode != 0:
        detail = completed.stderr.decode("utf-8", "replace").strip()
        raise EvidenceError(f"png2raw failed for {path}: {detail}")
    return completed.stdout


def receipt_fields(log: str, backend: str) -> Dict[str, int]:
    marker = (
        "Wired native OpenGL RAL: content receipt"
        if backend == "opengl"
        else "Wired Vulkan RAL: content receipt"
    )
    lines = [line for line in log.splitlines() if marker in line]
    if not lines:
        raise EvidenceError(f"{backend}: content receipt missing")
    fields: Dict[str, int] = {}
    for key, value in re.findall(r"\b([A-Za-z][A-Za-z0-9]*)=([0-9a-fA-F]+)", lines[-1]):
        base = 16 if any(c in "abcdefABCDEF" for c in value) else 10
        fields[key] = int(value, base)
    return fields


def require_receipt(log: str, backend: str) -> None:
    match = FORBIDDEN.search(log)
    if match:
        raise EvidenceError(f"{backend}: forbidden runtime evidence: {match.group(0)}")
    fields = receipt_fields(log, backend)
    required = OPENGL_REQUIRED if backend == "opengl" else VULKAN_REQUIRED
    missing = [field for field in required if fields.get(field, 0) <= 0]
    if missing:
        raise EvidenceError(f"{backend}: missing/non-positive cohorts: {', '.join(missing)}")
    if backend == "opengl":
        for field in ("unresolved", "fallback", "fatal"):
            if fields.get(field, -1) != 0:
                raise EvidenceError(f"opengl: {field} must be zero")
    foreign = (
        "Wired Vulkan RAL: content receipt"
        if backend == "opengl"
        else "Wired native OpenGL RAL: content receipt"
    )
    if foreign in log:
        raise EvidenceError(f"{backend}: foreign backend receipt present")


def roi_metrics(a: bytes, b: bytes, width: int, height: int, rect: Sequence[float]) -> Tuple[float, float, float]:
    x0 = max(0, min(width, int(math.floor(rect[0] * width))))
    y0 = max(0, min(height, int(math.floor(rect[1] * height))))
    x1 = max(x0 + 1, min(width, int(math.ceil(rect[2] * width))))
    y1 = max(y0 + 1, min(height, int(math.ceil(rect[3] * height))))
    diffs: List[int] = []
    for y in range(y0, y1):
        first = (y * width + x0) * 3
        last = (y * width + x1) * 3
        diffs.extend(abs(x - yv) for x, yv in zip(a[first:last], b[first:last]))
    diffs.sort()
    return (
        sum(diffs) / (len(diffs) * 255.0),
        diffs[max(0, math.ceil(len(diffs) * 0.90) - 1)] / 255.0,
        sum(value > 64 for value in diffs) / len(diffs),
    )


def ensure_populated(raw: bytes, label: str) -> None:
    sample_step = max(3, (len(raw) // 20000 // 3) * 3)
    colors = {raw[index:index + 3] for index in range(0, len(raw) - 2, sample_step)}
    if len(colors) < 4 or sum(raw) <= len(raw):
        raise EvidenceError(f"{label}: screenshot is blank or near-blank")


def analyze(root: Path, config: Mapping[str, object], decoder: Path) -> None:
    marker = root / "run.marker"
    if not marker.is_file():
        raise EvidenceError("run freshness marker missing")
    marker_time = marker.stat().st_mtime_ns
    width, height = int(config["width"]), int(config["height"])
    scenes = config["scenes"]
    if not isinstance(scenes, Mapping) or not scenes:
        raise EvidenceError("no parity scenes configured")
    metadata: List[Mapping[str, object]] = []
    frames: Dict[Tuple[str, str], bytes] = {}
    for scene in scenes:
        for backend in BACKENDS:
            stem = root / backend / str(scene)
            png, log_path, meta_path = stem.with_suffix(".png"), stem.with_suffix(".log"), stem.with_suffix(".meta.json")
            for artifact in (png, log_path, meta_path):
                if not artifact.is_file() or artifact.stat().st_size == 0:
                    raise EvidenceError(f"missing/empty artifact: {artifact}")
                if artifact.stat().st_mtime_ns < marker_time:
                    raise EvidenceError(f"stale artifact: {artifact}")
            if png_dimensions(png) != (width, height):
                raise EvidenceError(f"{backend}/{scene}: screenshot must be {width}x{height}")
            require_receipt(log_path.read_text(encoding="utf-8", errors="replace"), backend)
            meta = json.loads(meta_path.read_text(encoding="utf-8"))
            if meta.get("backend") != backend or meta.get("map") != scene:
                raise EvidenceError(f"{backend}/{scene}: metadata backend/map mismatch")
            metadata.append(meta)
            raw = decode_rgb(png, decoder)
            if len(raw) != width * height * 3:
                raise EvidenceError(f"{backend}/{scene}: decoded RGB byte count mismatch")
            ensure_populated(raw, f"{backend}/{scene}")
            frames[(backend, str(scene))] = raw
    for field in ("binary_sha256", "width", "height", "render_scale", "color_policy", "warmup_frames", "shader_time", "frame_time", "run_id"):
        values = {json.dumps(meta.get(field), sort_keys=True) for meta in metadata}
        if len(values) != 1:
            raise EvidenceError(f"capture metadata is not pinned for {field}: {sorted(values)}")
    if not re.fullmatch(r"[0-9a-f]{64}", str(metadata[0].get("binary_sha256", ""))):
        raise EvidenceError("capture metadata lacks a valid binary SHA-256")
    for scene, scene_cfg_raw in scenes.items():
        if not isinstance(scene_cfg_raw, Mapping):
            raise EvidenceError(f"invalid scene configuration: {scene}")
        camera_values = {json.dumps(meta.get("camera")) for meta in metadata if meta.get("map") == scene}
        if len(camera_values) != 1 or next(iter(camera_values)) != json.dumps(scene_cfg_raw.get("camera")):
            raise EvidenceError(f"{scene}: camera metadata mismatch")
        opengl, vulkan = frames[("opengl", str(scene))], frames[("vulkan", str(scene))]
        for region in scene_cfg_raw.get("regions", []):
            metrics = roi_metrics(opengl, vulkan, width, height, region["rect"])
            limits = (float(region["mae"]), float(region["p90"]), float(region["changed64"]))
            print(f"{scene}/{region['name']}: mae={metrics[0]:.4f}/{limits[0]:.4f} p90={metrics[1]:.4f}/{limits[1]:.4f} changed64={metrics[2]:.4f}/{limits[2]:.4f}")
            if any(value > limit for value, limit in zip(metrics, limits)):
                raise EvidenceError(f"{scene}/{region['name']}: visual tolerance exceeded")


def _chunk(kind: bytes, payload: bytes) -> bytes:
    return struct.pack(">I", len(payload)) + kind + payload + struct.pack(">I", zlib.crc32(kind + payload))


def _write_stub_png(path: Path, tag: bytes = b"good") -> None:
    ihdr = struct.pack(">IIBBBBB", 4, 4, 8, 2, 0, 0, 0)
    path.write_bytes(b"\x89PNG\r\n\x1a\n" + _chunk(b"IHDR", ihdr) + _chunk(b"IEND", b"") + tag)


def _fixture(root: Path, bad: str = "") -> Tuple[Mapping[str, object], Path]:
    root.mkdir(parents=True)
    (root / "run.marker").touch()
    config: Mapping[str, object] = {"width": 4, "height": 4, "scenes": {"arena1": {"camera": [1, 2, 3, 4], "regions": [{"name": "all", "rect": [0, 0, 1, 1], "mae": 0.1, "p90": 0.1, "changed64": 0.1}]}}}
    decoder = root / "fake_png2raw.py"
    decoder.write_text("import pathlib,sys\nd=pathlib.Path(sys.argv[1]).read_bytes()\nbase=bytes((32,64,96,96,64,32,16,32,48,48,32,16)*4)\nsys.stdout.buffer.write(bytes(255-x for x in base) if b'bad_pixels' in d else base)\n", encoding="utf-8")
    common = {"binary_sha256": "a" * 64, "width": 4, "height": 4, "render_scale": 1, "color_policy": "sdr-srgb", "warmup_frames": 180, "shader_time": 1.0, "frame_time": 1.0, "run_id": "selftest", "map": "arena1", "camera": [1, 2, 3, 4]}
    opengl_receipt = "Wired native OpenGL RAL: content receipt " + " ".join(f"{key}={0 if bad == 'cohort' and key == 'loweredWorld' else 1}" for key in OPENGL_REQUIRED) + " unresolved=0 fallback=0 fatal=0\n"
    vulkan_receipt = "Wired Vulkan RAL: content receipt " + " ".join(f"{key}=1" for key in VULKAN_REQUIRED) + "\n"
    for backend, receipt in (("opengl", opengl_receipt), ("vulkan", vulkan_receipt)):
        directory = root / backend
        directory.mkdir()
        png = directory / "arena1.png"
        _write_stub_png(png, b"bad_pixels" if bad == "pixels" and backend == "vulkan" else b"good")
        (directory / "arena1.log").write_text(receipt, encoding="utf-8")
        meta = dict(common, backend=("vulkan" if bad == "backend" and backend == "opengl" else backend))
        (directory / "arena1.meta.json").write_text(json.dumps(meta), encoding="utf-8")
    if bad == "stale":
        stale = root / "opengl" / "arena1.png"
        marker_time = (root / "run.marker").stat().st_mtime - 10
        os.utime(stale, (marker_time, marker_time))
    return config, decoder


def self_test() -> int:
    failures: List[str] = []
    with tempfile.TemporaryDirectory(prefix="ral-opengl-vulkan-parity-selftest-") as temp:
        root = Path(temp)
        config, decoder = _fixture(root / "pass")
        try:
            analyze(root / "pass", config, decoder)
        except EvidenceError as exc:
            failures.append(f"valid fixture rejected: {exc}")
        for defect in ("cohort", "backend", "stale", "pixels"):
            config, decoder = _fixture(root / defect, defect)
            try:
                analyze(root / defect, config, decoder)
            except EvidenceError:
                continue
            failures.append(f"{defect} defect was accepted")
    if failures:
        for failure in failures:
            print(f"FAIL: {failure}")
        return 1
    print("PASS: OpenGL/Vulkan visual parity analyzer fails closed")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--artifacts", type=Path)
    parser.add_argument("--config", type=Path)
    parser.add_argument("--png2raw", type=Path)
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        return self_test()
    if not args.artifacts or not args.config or not args.png2raw:
        parser.error("--artifacts, --config and --png2raw are required")
    try:
        config = json.loads(args.config.read_text(encoding="utf-8"))
        analyze(args.artifacts, config, args.png2raw)
    except (EvidenceError, OSError, ValueError, json.JSONDecodeError) as exc:
        print(f"FAIL: {exc}", file=sys.stderr)
        return 1
    print("PASS: native OpenGL/Vulkan visual parity evidence is within the documented tolerances")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
