#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors

"""Fail-closed native Metal versus Vulkan visual-parity evidence analyzer."""

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
from typing import Callable, Dict, Iterable, List, Mapping, Sequence, Tuple


BACKENDS = ("metal", "vulkan")
METAL_REQUIRED = (
    "worldBatches",
    "patchWorld",
    "texturedWorld",
    "lightmappedWorld",
    "modelEntities",
    "entities",
    "ui",
    "loweredUi",
    "texturedUi",
    "msdfUi",
)
VULKAN_REQUIRED = (
    "worldBatches",
    "patchWorld",
    "materialWorld",
    "lightmappedWorld",
    "modelAssets",
    "entities",
    "ui",
)
FORBIDDEN = re.compile(
    r"lifecycle failure|advancing cl_renderer|failed to initialize|"
    r"renderer fallback|Sys_Error|\bFATAL\b|\bVUID-|validation error|"
    r"VM_Create.*failed|VM syscall error|died on signal|"
    r"\[ERROR\s*\]|disabling (?:particles|decals|ribbon/rail/beam)|"
    r"(?:particles|decals) remain disabled|"
    r"cooked material catalog rejected|rejected profile [0-9]+[^\n]*load/validation failure",
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
        "Wired native Metal RAL: content receipt"
        if backend == "metal"
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
    # Equal missing content is not parity. Ignore intentional BSP sentinels
    # and nodraw volumes; scripted materials may resolve through stage images
    # rather than their primary image, so their diagnostic is not sufficient.
    for line in log.splitlines():
        unresolved = re.search(r"unresolved material name=(\S+).*scripted=0.*noDraw=0\b", line)
        if unresolved and unresolved.group(1).lower() != "noshader":
            raise EvidenceError(f"{backend}: unresolved implicit world material: {unresolved.group(1)}")
    fields = receipt_fields(log, backend)
    required = METAL_REQUIRED if backend == "metal" else VULKAN_REQUIRED
    missing = [field for field in required if fields.get(field, 0) <= 0]
    if missing:
        raise EvidenceError(f"{backend}: missing/non-positive cohorts: {', '.join(missing)}")
    if backend == "metal" and fields.get("unresolvedEntities", -1) != 0:
        raise EvidenceError("metal: unresolvedEntities must be zero")
    foreign = (
        "Wired Vulkan RAL: content receipt"
        if backend == "metal"
        else "Wired native Metal RAL: content receipt"
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
    mae = sum(diffs) / (len(diffs) * 255.0)
    p90 = diffs[max(0, math.ceil(len(diffs) * 0.90) - 1)] / 255.0
    changed = sum(value > 64 for value in diffs) / len(diffs)
    return mae, p90, changed


def ensure_populated(raw: bytes, label: str) -> None:
    sample_step = max(3, (len(raw) // 20000 // 3) * 3)
    colors = {raw[index : index + 3] for index in range(0, len(raw) - 2, sample_step)}
    if len(colors) < 4 or sum(raw) <= len(raw):
        raise EvidenceError(f"{label}: screenshot is blank or near-blank")


def analyze(root: Path, config: Mapping[str, object], decoder: Path) -> None:
    marker = root / "run.marker"
    if not marker.is_file():
        raise EvidenceError("run freshness marker missing")
    marker_time = marker.stat().st_mtime_ns
    logical_width = int(config["width"])
    logical_height = int(config["height"])
    width = height = 0
    scenes = config["scenes"]
    if not isinstance(scenes, Mapping) or not scenes:
        raise EvidenceError("no parity scenes configured")

    metadata: List[Mapping[str, object]] = []
    frames: Dict[Tuple[str, str], bytes] = {}
    for scene in scenes:
        for backend in BACKENDS:
            stem = root / backend / str(scene)
            png = stem.with_suffix(".png")
            log_path = stem.with_suffix(".log")
            meta_path = stem.with_suffix(".meta.json")
            for artifact in (png, log_path, meta_path):
                if not artifact.is_file() or artifact.stat().st_size == 0:
                    raise EvidenceError(f"missing/empty artifact: {artifact}")
                if artifact.stat().st_mtime_ns < marker_time:
                    raise EvidenceError(f"stale artifact: {artifact}")
            frame_width, frame_height = png_dimensions(png)
            if frame_width % logical_width or frame_height % logical_height:
                raise EvidenceError(
                    f"{backend}/{scene}: screenshot {frame_width}x{frame_height} is not an "
                    f"integer-DPI multiple of {logical_width}x{logical_height}"
                )
            scale_x = frame_width // logical_width
            scale_y = frame_height // logical_height
            if scale_x != scale_y or scale_x < 1 or scale_x > 4:
                raise EvidenceError(
                    f"{backend}/{scene}: invalid presentation scale {scale_x}x{scale_y}"
                )
            if width == 0:
                width, height = frame_width, frame_height
            elif (frame_width, frame_height) != (width, height):
                raise EvidenceError(
                    f"{backend}/{scene}: screenshot extent differs from the matrix "
                    f"{width}x{height}"
                )
            log = log_path.read_text(encoding="utf-8", errors="replace")
            require_receipt(log, backend)
            meta = json.loads(meta_path.read_text(encoding="utf-8"))
            if meta.get("backend") != backend or meta.get("map") != scene:
                raise EvidenceError(f"{backend}/{scene}: metadata backend/map mismatch")
            if (meta.get("width"), meta.get("height")) != (frame_width, frame_height):
                raise EvidenceError(f"{backend}/{scene}: metadata screenshot extent mismatch")
            if (meta.get("logical_width"), meta.get("logical_height")) not in (
                (None, None), (logical_width, logical_height)
            ):
                raise EvidenceError(f"{backend}/{scene}: metadata logical extent mismatch")
            metadata.append(meta)
            raw = decode_rgb(png, decoder)
            if len(raw) != width * height * 3:
                raise EvidenceError(f"{backend}/{scene}: decoded RGB byte count mismatch")
            ensure_populated(raw, f"{backend}/{scene}")
            frames[(backend, str(scene))] = raw

    pinned = (
        "binary_sha256",
        "width",
        "height",
        "render_scale",
        "render_width",
        "render_height",
        "scene_format",
        "color_policy",
        "warmup_frames",
        "shader_time",
        "frame_time",
        "run_id",
    )
    for field in pinned:
        values = {json.dumps(meta.get(field), sort_keys=True) for meta in metadata}
        if len(values) != 1:
            raise EvidenceError(f"capture metadata is not pinned for {field}: {sorted(values)}")
    if not re.fullmatch(r"[0-9a-f]{64}", str(metadata[0].get("binary_sha256", ""))):
        raise EvidenceError("capture metadata lacks a valid binary SHA-256")
    for meta in metadata:
        if (meta.get("scene_format") != "R16G16B16A16_SFLOAT"
                or any(type(meta.get(k)) is not int or meta[k] <= 0
                       for k in ("render_width", "render_height"))):
            raise EvidenceError("capture metadata lacks verified HDR/render extent")

    # Same-backend repeat determinism (N=1 self-identity): when repeat captures
    # are present or required, each backend/scene must reproduce itself within a
    # near-identity tolerance over the whole frame. This is independent of, and
    # much tighter than, the cross-backend parity tolerances below.
    repeat_cfg = config.get("repeat")
    if repeat_cfg is not None:
        if not isinstance(repeat_cfg, Mapping):
            raise EvidenceError("invalid repeat configuration")
        limits = (float(repeat_cfg["mae"]), float(repeat_cfg["p90"]), float(repeat_cfg["changed64"]))
        required = bool(repeat_cfg.get("required", False))
        pinned_repeat = ("binary_sha256", "width", "height", "render_scale", "render_width",
                         "render_height", "scene_format", "camera", "frame_time", "shader_time")
        for scene in scenes:
            for backend in BACKENDS:
                # Explicit names: Path.with_suffix would strip the ".repeat" tag.
                png = root / backend / f"{scene}.repeat.png"
                if not png.is_file():
                    if required:
                        raise EvidenceError(f"{backend}/{scene}: repeat capture missing")
                    continue
                log_path = root / backend / f"{scene}.repeat.log"
                meta_path = root / backend / f"{scene}.repeat.meta.json"
                for artifact in (png, log_path, meta_path):
                    if not artifact.is_file() or artifact.stat().st_size == 0:
                        raise EvidenceError(f"missing/empty repeat artifact: {artifact}")
                    if artifact.stat().st_mtime_ns < marker_time:
                        raise EvidenceError(f"stale repeat artifact: {artifact}")
                if png_dimensions(png) != (width, height):
                    raise EvidenceError(f"{backend}/{scene}: repeat screenshot extent differs")
                require_receipt(log_path.read_text(encoding="utf-8", errors="replace"), backend)
                meta = json.loads(meta_path.read_text(encoding="utf-8"))
                first = next(m for m in metadata if m.get("backend") == backend and m.get("map") == scene)
                for field in pinned_repeat:
                    if json.dumps(meta.get(field), sort_keys=True) != json.dumps(first.get(field), sort_keys=True):
                        raise EvidenceError(f"{backend}/{scene}: repeat metadata differs for {field}")
                raw = decode_rgb(png, decoder)
                if len(raw) != width * height * 3:
                    raise EvidenceError(f"{backend}/{scene}: repeat decoded RGB byte count mismatch")
                ensure_populated(raw, f"{backend}/{scene}.repeat")
                metrics = roi_metrics(frames[(backend, str(scene))], raw, width, height, [0.0, 0.0, 1.0, 1.0])
                print(
                    f"{scene}/{backend}/repeat: mae={metrics[0]:.4f}/{limits[0]:.4f} "
                    f"p90={metrics[1]:.4f}/{limits[1]:.4f} "
                    f"changed64={metrics[2]:.4f}/{limits[2]:.4f}"
                )
                if any(value > limit for value, limit in zip(metrics, limits)):
                    raise EvidenceError(f"{backend}/{scene}: same-backend repeat determinism exceeded")

    for scene, scene_cfg_raw in scenes.items():
        scene_cfg = scene_cfg_raw
        if not isinstance(scene_cfg, Mapping):
            raise EvidenceError(f"invalid scene configuration: {scene}")
        camera_values = {
            json.dumps(meta.get("camera")) for meta in metadata if meta.get("map") == scene
        }
        if len(camera_values) != 1 or next(iter(camera_values)) != json.dumps(scene_cfg.get("camera")):
            raise EvidenceError(f"{scene}: camera metadata mismatch")
        metal = frames[("metal", str(scene))]
        vulkan = frames[("vulkan", str(scene))]
        for region in scene_cfg.get("regions", []):
            metrics = roi_metrics(metal, vulkan, width, height, region["rect"])
            limits = (float(region["mae"]), float(region["p90"]), float(region["changed64"]))
            print(
                f"{scene}/{region['name']}: mae={metrics[0]:.4f}/{limits[0]:.4f} "
                f"p90={metrics[1]:.4f}/{limits[1]:.4f} "
                f"changed64={metrics[2]:.4f}/{limits[2]:.4f}"
            )
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
    config: Mapping[str, object] = {
        "width": 4,
        "height": 4,
        "repeat": {"required": True, "mae": 0.002, "p90": 0.004, "changed64": 0.001},
        "scenes": {
            "arena1": {
                "camera": [1, 2, 3, 4],
                "regions": [{"name": "all", "rect": [0, 0, 1, 1], "mae": 0.1, "p90": 0.1, "changed64": 0.1}],
            }
        },
    }
    decoder = root / "fake_png2raw.py"
    decoder.write_text(
        "import pathlib,sys\n"
        "d=pathlib.Path(sys.argv[1]).read_bytes()\n"
        "base=bytes((32,64,96,96,64,32,16,32,48,48,32,16)*4)\n"
        "sys.stdout.buffer.write(bytes(255-x for x in base) if b'bad_pixels' in d else base)\n",
        encoding="utf-8",
    )
    common = {
        "binary_sha256": "a" * 64,
        "width": 4,
        "height": 4,
        "render_scale": 1,
        "render_width": 4,
        "render_height": 4,
        "scene_format": "R16G16B16A16_SFLOAT",
        "color_policy": "sdr-srgb",
        "warmup_frames": 180,
        "shader_time": 1.0,
        "frame_time": 1.0,
        "run_id": "selftest",
        "map": "arena1",
        "camera": [1, 2, 3, 4],
    }
    metal_receipt = "Wired native Metal RAL: content receipt " + " ".join(
        f"{key}={0 if bad == 'cohort' and key == 'patchWorld' else 1}" for key in METAL_REQUIRED
    ) + " unresolvedEntities=0\n"
    vulkan_receipt = "Wired Vulkan RAL: content receipt " + " ".join(f"{key}=1" for key in VULKAN_REQUIRED) + "\n"
    for backend, receipt in (("metal", metal_receipt), ("vulkan", vulkan_receipt)):
        if bad == "cooked_catalog" and backend == "vulkan":
            receipt += "RAL cooked material catalog rejected world=0 reason=5\n"
        if bad == "fx_profile" and backend == "metal":
            receipt += "rejected profile 1 'scripts/effects/rocket-explosion.lua': load/validation failure\n"
        if backend == "vulkan":
            fx_failures = {
                "particle_disabled": "[WARN ] particle render RAL bind-group cohort unavailable; disabling particles\n",
                "decal_disabled": "[WARN ] decal render RAL bind-group cohort unavailable; disabling decals\n",
                "primitive_disabled": "[ERROR] primitive RAL bind-group cohort publication failed; disabling ribbon/rail/beam\n",
                "runtime_error": "[ERROR] unexpected renderer resource failure\n",
            }
            receipt += fx_failures.get(bad, "")
        if backend == "metal":
            receipt += "unresolved material name=noshader image=noshader scripted=0 sky=0 noDraw=0\n"
            receipt += "unresolved material name=textures/common/clip image= scripted=0 sky=0 noDraw=1\n"
            receipt += "unresolved material name=test/staged image= scripted=1 sky=0 noDraw=0\n"
            if bad == "implicit_material":
                receipt += "unresolved material name=textures/test/missing image=textures/test/missing scripted=0 sky=0 noDraw=0\n"
        directory = root / backend
        directory.mkdir()
        png = directory / "arena1.png"
        _write_stub_png(png, b"bad_pixels" if bad == "pixels" and backend == "vulkan" else b"good")
        (directory / "arena1.log").write_text(receipt, encoding="utf-8")
        meta = dict(common, backend=("vulkan" if bad == "backend" and backend == "metal" else backend))
        (directory / "arena1.meta.json").write_text(json.dumps(meta), encoding="utf-8")
        if bad != "repeat_missing":
            _write_stub_png(directory / "arena1.repeat.png",
                            b"bad_pixels" if bad == "repeat_pixels" and backend == "metal" else b"good")
            (directory / "arena1.repeat.log").write_text(receipt, encoding="utf-8")
            repeat_meta = dict(meta, camera=([9, 9, 9, 9] if bad == "repeat_meta" and backend == "metal" else meta["camera"]))
            (directory / "arena1.repeat.meta.json").write_text(json.dumps(repeat_meta), encoding="utf-8")
    if bad == "stale":
        stale = root / "metal" / "arena1.png"
        marker_time = (root / "run.marker").stat().st_mtime - 10
        os.utime(stale, (marker_time, marker_time))
    return config, decoder


def self_test() -> int:
    failures: List[str] = []
    with tempfile.TemporaryDirectory(prefix="ral-parity-selftest-") as temp:
        root = Path(temp)
        config, decoder = _fixture(root / "pass")
        try:
            analyze(root / "pass", config, decoder)
        except EvidenceError as exc:
            failures.append(f"valid fixture rejected: {exc}")
        for defect in ("cohort", "backend", "stale", "pixels", "cooked_catalog", "fx_profile", "implicit_material",
                       "particle_disabled", "decal_disabled", "primitive_disabled", "runtime_error",
                       "repeat_missing", "repeat_pixels", "repeat_meta"):
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
    print("PASS: Metal/Vulkan visual parity analyzer fails closed")
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
    print("PASS: native Metal/Vulkan visual parity evidence is within the documented tolerances")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
