#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors

"""Analyze paired Release native-Metal and MoltenVK performance evidence."""

from __future__ import annotations

import argparse
import datetime as dt
import json
import math
import os
import re
import statistics
import sys
import tempfile
from pathlib import Path
from typing import Dict, List, Mapping, Sequence, Tuple


BACKENDS = ("metal", "vulkan")
TRACE = re.compile(
    r"frame trace \(200f\): bucket=(\d+) count=(\d+) valid=(\d+) "
    r"cpu_work_total=(\d+)us scr_end_total=(\d+)us"
)
DIST = re.compile(
    r"frame work distribution \(200f\): bucket=(\d+) "
    r"lt1ms=(\d+) lt2ms=(\d+) lt4ms=(\d+) lt8ms=(\d+) "
    r"lt12ms=(\d+) lt16_667ms=(\d+) lt24ms=(\d+) "
    r"lt33_334ms=(\d+) lt50ms=(\d+) ge50ms=(\d+) max_us=(\d+)"
)
GPU_HEADER = re.compile(r"gpu \((\d+)f avg, ms\):")
GPU_TOTAL = re.compile(r"^\s*total=([0-9.]+)")
METAL_READY = re.compile(
    r"Wired native Metal RAL: registration ready logical=(\d+)x(\d+) "
    r"pixels=(\d+)x(\d+) swapInterval=(-?\d+) presentMode=(\d+) displaySync=(\d+)"
)
VK_PERF = re.compile(r"vk perf v2: (.+)$")
FORBIDDEN = re.compile(
    r"lifecycle failure|advancing cl_renderer|failed to initialize|renderer fallback|"
    r"Sys_Error|\bFATAL\b|\bVUID-|validation error|VM_Create.*failed|"
    r"VM syscall error|died on signal",
    re.IGNORECASE,
)
HISTOGRAM_UPPER_US = (1000, 2000, 4000, 8000, 12000, 16667, 24000, 33334, 50000)


class EvidenceError(RuntimeError):
    pass


def parse_time(value: str) -> dt.datetime:
    return dt.datetime.fromisoformat(value.replace("Z", "+00:00"))


def receipt_fields(messages: Sequence[str], backend: str) -> Dict[str, int]:
    marker = (
        "Wired native Metal RAL: content receipt"
        if backend == "metal"
        else "Wired Vulkan RAL: content receipt"
    )
    lines = [message for message in messages if marker in message]
    if not lines:
        raise EvidenceError(f"{backend}: semantic content receipt missing")
    fields: Dict[str, int] = {}
    for key, value in re.findall(r"\b([A-Za-z][A-Za-z0-9]*)=([0-9a-fA-F]+)", lines[-1]):
        fields[key] = int(value, 16 if any(c in "abcdefABCDEF" for c in value) else 10)
    required = (
        ("worldBatches", "patchWorld", "texturedWorld", "lightmappedWorld", "modelEntities", "entities", "ui", "loweredUi")
        if backend == "metal"
        else ("worldBatches", "patchWorld", "materialWorld", "lightmappedWorld", "modelAssets", "entities", "ui")
    )
    missing = [key for key in required if fields.get(key, 0) <= 0]
    if missing:
        raise EvidenceError(f"{backend}: non-positive semantic cohorts: {', '.join(missing)}")
    if backend == "metal" and fields.get("unresolvedEntities", -1) != 0:
        raise EvidenceError("metal: unresolvedEntities must be zero")
    return fields


def parse_vk_perf(payload: str) -> Mapping[str, object]:
    fields: Dict[str, str] = {}
    for token in payload.split():
        key, separator, value = token.partition("=")
        if separator != "=" or not key or not value or key in fields:
            raise EvidenceError("malformed vk perf v2 payload")
        fields[key] = value
    required = ("mode", "r_swapInterval", "extent", "count", "valid", "identity_stable")
    if any(key not in fields for key in required):
        raise EvidenceError("incomplete vk perf v2 payload")
    extent = fields["extent"].split("x")
    if len(extent) != 2:
        raise EvidenceError("invalid Vulkan perf extent")
    return {
        "mode": fields["mode"],
        "swap_interval": int(fields["r_swapInterval"]),
        "extent": (int(extent[0]), int(extent[1])),
        "count": int(fields["count"]),
        "valid": int(fields["valid"]),
        "identity_stable": int(fields["identity_stable"]),
    }


def percentile_upper(histogram: Sequence[int], fraction: float, maximum: int) -> int:
    target = math.ceil(sum(histogram) * fraction)
    count = 0
    for index, value in enumerate(histogram):
        count += value
        if count >= target:
            return HISTOGRAM_UPPER_US[index] if index < len(HISTOGRAM_UPPER_US) else maximum
    raise EvidenceError("empty frame histogram")


def analyze_run(qconsole: Path, stdout: Path, backend: str, expected_frames: int) -> Mapping[str, object]:
    stdout_text = stdout.read_text(encoding="utf-8", errors="replace")
    if (match := FORBIDDEN.search(stdout_text)):
        raise EvidenceError(f"{backend}: forbidden stdout evidence: {match.group(0)}")
    records: List[Mapping[str, str]] = []
    for line_number, line in enumerate(qconsole.read_text(encoding="utf-8", errors="replace").splitlines(), 1):
        try:
            record = json.loads(line)
        except json.JSONDecodeError as exc:
            raise EvidenceError(f"{backend}: malformed qconsole line {line_number}: {exc}") from exc
        if not isinstance(record, dict) or not all(isinstance(record.get(key), str) for key in ("ts", "msg")):
            raise EvidenceError(f"{backend}: malformed qconsole record {line_number}")
        records.append(record)
    messages = [record["msg"].strip() for record in records]
    joined = "\n".join(messages)
    if (match := FORBIDDEN.search(joined)):
        raise EvidenceError(f"{backend}: forbidden runtime evidence: {match.group(0)}")
    receipt_fields(messages, backend)

    begin = [index for index, message in enumerate(messages) if message == "RAL_BACKEND_PERF_BEGIN"]
    end = [index for index, message in enumerate(messages) if message == "RAL_BACKEND_PERF_END"]
    if len(begin) != 1 or len(end) != 1 or begin[0] >= end[0]:
        raise EvidenceError(f"{backend}: invalid measurement marker pair")
    active = records[begin[0] + 1 : end[0]]
    traces: Dict[int, Tuple[int, int, int]] = {}
    distributions: Dict[int, Tuple[List[int], int]] = {}
    gpu_totals: List[float] = []
    gpu_pending = False
    vk_blocks: List[Mapping[str, object]] = []
    for record in active:
        message = record["msg"].strip()
        if match := TRACE.search(message):
            bucket, count, valid, cpu_total, scr_total = map(int, match.groups())
            if bucket in traces:
                raise EvidenceError(f"{backend}: duplicate frame-trace bucket {bucket}")
            traces[bucket] = (count, cpu_total, scr_total)
            if count != 200 or valid != count:
                raise EvidenceError(f"{backend}: incomplete frame-trace bucket {bucket}")
        if match := DIST.search(message):
            values = list(map(int, match.groups()))
            bucket, bins, maximum = values[0], values[1:11], values[11]
            if bucket in distributions or sum(bins) != 200:
                raise EvidenceError(f"{backend}: invalid distribution bucket {bucket}")
            distributions[bucket] = (bins, maximum)
        if GPU_HEADER.search(message):
            gpu_pending = True
        elif gpu_pending and (match := GPU_TOTAL.search(message)):
            gpu_totals.append(float(match.group(1)))
            gpu_pending = False
        if match := VK_PERF.search(message):
            vk_blocks.append(parse_vk_perf(match.group(1)))

    if sorted(traces) != sorted(distributions) or len(traces) * 200 != expected_frames:
        raise EvidenceError(f"{backend}: trace/distribution coverage mismatch")
    all_bins = [0] * 10
    maxima: List[int] = []
    cpu_total = scr_total = 0
    for bucket in sorted(traces):
        count, cpu, scr = traces[bucket]
        bins, maximum = distributions[bucket]
        cpu_total += cpu
        scr_total += scr
        maxima.append(maximum)
        for index, value in enumerate(bins):
            all_bins[index] += value
    if sum(all_bins) != expected_frames:
        raise EvidenceError(f"{backend}: histogram frame count mismatch")

    begin_time = parse_time(records[begin[0]]["ts"])
    end_time = parse_time(records[end[0]]["ts"])
    wall_seconds = (end_time - begin_time).total_seconds()
    if wall_seconds <= 0:
        raise EvidenceError(f"{backend}: invalid marker duration")

    pixel_extent: Tuple[int, int]
    present_mode: str
    if backend == "metal":
        ready = [METAL_READY.search(message) for message in messages]
        ready = [match for match in ready if match]
        if not ready:
            raise EvidenceError("metal: actual presentation receipt missing")
        logical_w, logical_h, pixel_w, pixel_h, swap, mode, sync = map(int, ready[-1].groups())
        if (logical_w, logical_h, swap, mode, sync) != (1280, 720, 0, 2, 0):
            raise EvidenceError("metal: output/present policy mismatch")
        pixel_extent = (pixel_w, pixel_h)
        present_mode = "IMMEDIATE"
    else:
        if len(vk_blocks) < len(traces):
            raise EvidenceError("vulkan: incomplete vk perf v2 coverage")
        if any(
            block["mode"] != "IMMEDIATE"
            or block["swap_interval"] != 0
            or block["identity_stable"] != 1
            or block["count"] != block["valid"]
            for block in vk_blocks
        ):
            raise EvidenceError("vulkan: output/present identity drift")
        extents = {block["extent"] for block in vk_blocks}
        if len(extents) != 1:
            raise EvidenceError("vulkan: render extent drift")
        pixel_extent = next(iter(extents))
        present_mode = "IMMEDIATE"

    maximum = max(maxima)
    return {
        "frames": expected_frames,
        "cpu_work_mean_us": cpu_total / expected_frames,
        "scr_end_mean_us": scr_total / expected_frames,
        "p50_upper_us": percentile_upper(all_bins, 0.50, maximum),
        "p95_upper_us": percentile_upper(all_bins, 0.95, maximum),
        "p99_upper_us": percentile_upper(all_bins, 0.99, maximum),
        "max_us": maximum,
        "hitch_ge24ms_ratio": sum(all_bins[7:]) / expected_frames,
        "histogram": all_bins,
        "wall_seconds": wall_seconds,
        "wall_fps": expected_frames / wall_seconds,
        "gpu_total_median_ms": statistics.median(gpu_totals) if gpu_totals else None,
        "gpu_blocks": len(gpu_totals),
        "present_mode": present_mode,
        "pixel_extent": list(pixel_extent),
    }


def analyze(root: Path, config: Mapping[str, object]) -> Mapping[str, object]:
    marker = root / "run.marker"
    if not marker.is_file():
        raise EvidenceError("freshness marker missing")
    marker_time = marker.stat().st_mtime_ns
    scenes = config["scenes"]
    frames = int(config["measurement_frames"])
    repetitions = int(config.get("repetitions", 1))
    if repetitions < 1 or repetitions > 4:
        raise EvidenceError("repetition count outside bounded range 1..4")
    metadata: List[Mapping[str, object]] = []
    runs: Dict[str, Dict[str, List[Mapping[str, object]]]] = {
        backend: {str(scene): [] for scene in scenes} for backend in BACKENDS
    }
    for scene in scenes:
        for backend in BACKENDS:
            for repetition in range(1, repetitions + 1):
                stem = root / backend / f"{scene}-r{repetition}"
                qconsole = stem.with_suffix(".qconsole.jsonl")
                stdout = stem.with_suffix(".stdout.log")
                meta_path = stem.with_suffix(".meta.json")
                for artifact in (qconsole, stdout, meta_path):
                    if not artifact.is_file() or artifact.stat().st_size == 0:
                        raise EvidenceError(f"missing/empty artifact: {artifact}")
                    if artifact.stat().st_mtime_ns < marker_time:
                        raise EvidenceError(f"stale artifact: {artifact}")
                meta = json.loads(meta_path.read_text(encoding="utf-8"))
                if (
                    meta.get("backend") != backend
                    or meta.get("map") != scene
                    or meta.get("repetition") != repetition
                ):
                    raise EvidenceError(f"{backend}/{scene}/r{repetition}: metadata identity mismatch")
                if meta.get("camera") != scenes[scene]["camera"]:
                    raise EvidenceError(f"{backend}/{scene}/r{repetition}: camera drift")
                metadata.append(meta)
                runs[backend][str(scene)].append(
                    analyze_run(qconsole, stdout, backend, frames)
                )

    pinned = (
        "engine_sha256", "build_profile", "width", "height", "render_scale",
        "swap_interval", "warmup_frames", "measurement_frames", "run_id",
    )
    for field in pinned:
        values = {json.dumps(meta.get(field), sort_keys=True) for meta in metadata}
        if len(values) != 1:
            raise EvidenceError(f"capture configuration drift: {field}")
    first = metadata[0]
    if first.get("build_profile") != "Release" or first.get("width") != 1280 or first.get("height") != 720:
        raise EvidenceError("benchmark is not the pinned Release 1280x720 profile")
    if first.get("swap_interval") != 0 or first.get("render_scale") != 1:
        raise EvidenceError("benchmark presentation/render-scale policy drift")
    if not re.fullmatch(r"[0-9a-f]{64}", str(first.get("engine_sha256", ""))):
        raise EvidenceError("invalid engine SHA-256")

    results: Dict[str, Dict[str, Mapping[str, object]]] = {backend: {} for backend in BACKENDS}
    for backend in BACKENDS:
        for scene in scenes:
            scene_runs = runs[backend][str(scene)]
            extents = {tuple(run["pixel_extent"]) for run in scene_runs}
            modes = {str(run["present_mode"]) for run in scene_runs}
            if len(extents) != 1 or modes != {"IMMEDIATE"}:
                raise EvidenceError(f"{backend}/{scene}: repetition output identity drift")
            results[backend][str(scene)] = {
                "cpu_work_median_us": statistics.median(
                    float(run["cpu_work_mean_us"]) for run in scene_runs
                ),
                "hitch_ge24ms_ratio_max": max(
                    float(run["hitch_ge24ms_ratio"]) for run in scene_runs
                ),
                "pixel_extent": list(next(iter(extents))),
                "present_mode": "IMMEDIATE",
                "runs": scene_runs,
            }

    for scene in scenes:
        metal_extent = results["metal"][scene]["pixel_extent"]
        vulkan_extent = results["vulkan"][scene]["pixel_extent"]
        if metal_extent != vulkan_extent:
            raise EvidenceError(f"{scene}: backend pixel/render extent mismatch {metal_extent} != {vulkan_extent}")

    metal_means = [float(results["metal"][scene]["cpu_work_median_us"]) for scene in scenes]
    vulkan_means = [float(results["vulkan"][scene]["cpu_work_median_us"]) for scene in scenes]
    metal_corpus = math.exp(sum(math.log(value) for value in metal_means) / len(metal_means))
    vulkan_corpus = math.exp(sum(math.log(value) for value in vulkan_means) / len(vulkan_means))
    ratio_limit = float(config["native_to_moltenvk_ratio_limit"])
    allowance = float(config["fixed_allowance_us"])
    limit = vulkan_corpus * ratio_limit + allowance
    if metal_corpus > limit:
        raise EvidenceError(
            f"native Metal corpus CPU-work geomean regressed: {metal_corpus:.1f}us > {limit:.1f}us"
        )
    max_hitch_delta = float(config["max_hitch_ratio_delta"])
    for scene in scenes:
        metal_hitch = float(results["metal"][scene]["hitch_ge24ms_ratio_max"])
        vulkan_hitch = float(results["vulkan"][scene]["hitch_ge24ms_ratio_max"])
        if metal_hitch > vulkan_hitch + max_hitch_delta:
            raise EvidenceError(f"{scene}: native Metal hitch ratio regressed")

    return {
        "schema": 1,
        "engine_sha256": first["engine_sha256"],
        "build_profile": first["build_profile"],
        "configuration": {
            "logical_output": [first["width"], first["height"]],
            "render_scale": first["render_scale"],
            "swap_interval": first["swap_interval"],
            "warmup_frames": first["warmup_frames"],
            "measurement_frames": first["measurement_frames"],
            "repetitions": repetitions,
        },
        "backends": results,
        "ratified_statistic": "arena1/arena17 geometric mean of counterbalanced per-scene aggregate CPU-work usec/frame",
        "native_metal_corpus_us": round(metal_corpus, 3),
        "moltenvk_corpus_us": round(vulkan_corpus, 3),
        "native_to_moltenvk_ratio": round(metal_corpus / vulkan_corpus, 5),
        "ratio_limit": ratio_limit,
        "fixed_allowance_us": allowance,
        "status": "PASS",
    }


def _record(ts: str, msg: str) -> str:
    return json.dumps({"ts": ts, "sev": "INFO", "cat": "system", "msg": msg})


def _fixture(root: Path, defect: str = "") -> Mapping[str, object]:
    root.mkdir(parents=True)
    (root / "run.marker").touch()
    config: Mapping[str, object] = {
        "measurement_frames": 600,
        "repetitions": 1,
        "native_to_moltenvk_ratio_limit": 1.15,
        "fixed_allowance_us": 0,
        "max_hitch_ratio_delta": 0.01,
        "scenes": {"arena1": {"camera": [1, 2, 3, 4]}},
    }
    for backend in BACKENDS:
        directory = root / backend
        directory.mkdir()
        mean = 1000 if backend == "metal" else 1200
        if defect == "regression" and backend == "metal":
            mean = 2000
        receipt = (
            "Wired native Metal RAL: content receipt worldBatches=1 patchWorld=1 texturedWorld=1 lightmappedWorld=1 modelEntities=1 entities=1 ui=1 loweredUi=1 unresolvedEntities=0"
            if backend == "metal"
            else "Wired Vulkan RAL: content receipt worldBatches=1 patchWorld=1 materialWorld=1 lightmappedWorld=1 modelAssets=1 entities=1 ui=1"
        )
        lines = [_record("2026-08-24T00:00:00+00:00", receipt)]
        if backend == "metal":
            mode = 0 if defect == "present" else 2
            lines.append(_record("2026-08-24T00:00:00+00:00", f"Wired native Metal RAL: registration ready logical=1280x720 pixels=2560x1440 swapInterval=0 presentMode={mode} displaySync=0"))
        lines.append(_record("2026-08-24T00:00:01+00:00", "RAL_BACKEND_PERF_BEGIN"))
        for bucket in range(1, 4):
            lines.append(_record("2026-08-24T00:00:02+00:00", f"frame trace (200f): bucket={bucket} count=200 valid=200 cpu_work_total={mean * 200}us scr_end_total={mean * 100}us"))
            total = 199 if defect == "histogram" and backend == "metal" and bucket == 1 else 200
            lines.append(_record("2026-08-24T00:00:02+00:00", f"frame work distribution (200f): bucket={bucket} lt1ms=0 lt2ms={total} lt4ms=0 lt8ms=0 lt12ms=0 lt16_667ms=0 lt24ms=0 lt33_334ms=0 lt50ms=0 ge50ms=0 max_us={mean}"))
            if backend == "vulkan":
                mode = "FIFO" if defect == "present" else "IMMEDIATE"
                lines.append(_record("2026-08-24T00:00:02+00:00", f"vk perf v2: mode={mode} r_swapInterval=0 extent=2560x1440 count=200 valid=200 identity_stable=1"))
        lines.append(_record("2026-08-24T00:00:04+00:00", "RAL_BACKEND_PERF_END"))
        stem = directory / "arena1-r1"
        stem.with_suffix(".qconsole.jsonl").write_text("\n".join(lines) + "\n", encoding="utf-8")
        stem.with_suffix(".stdout.log").write_text("clean runtime\n", encoding="utf-8")
        meta = {
            "engine_sha256": ("b" * 64 if defect == "sha" and backend == "metal" else "a" * 64),
            "renderer_sha256": backend[0] * 64,
            "build_profile": "Release", "width": 1280, "height": 720,
            "render_scale": 1, "swap_interval": 0, "warmup_frames": 300,
            "measurement_frames": 600, "run_id": "selftest", "backend": backend,
            "map": "arena1", "repetition": 1, "camera": [1, 2, 3, 4],
        }
        stem.with_suffix(".meta.json").write_text(json.dumps(meta), encoding="utf-8")
    if defect == "stale":
        stale = root / "metal" / "arena1-r1.qconsole.jsonl"
        old = (root / "run.marker").stat().st_mtime - 10
        os.utime(stale, (old, old))
    return config


def self_test() -> int:
    failures: List[str] = []
    with tempfile.TemporaryDirectory(prefix="ral-backend-perf-selftest-") as temp:
        base = Path(temp)
        config = _fixture(base / "pass")
        try:
            analyze(base / "pass", config)
        except EvidenceError as exc:
            failures.append(f"valid fixture rejected: {exc}")
        for defect in ("regression", "histogram", "present", "sha", "stale"):
            config = _fixture(base / defect, defect)
            try:
                analyze(base / defect, config)
            except EvidenceError:
                continue
            failures.append(f"{defect} defect was accepted")
    if failures:
        for failure in failures:
            print(f"FAIL: {failure}")
        return 1
    print("PASS: Metal/MoltenVK performance analyzer fails closed")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--artifacts", type=Path)
    parser.add_argument("--config", type=Path)
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        return self_test()
    if not args.artifacts or not args.config:
        parser.error("--artifacts and --config are required")
    try:
        config = json.loads(args.config.read_text(encoding="utf-8"))
        summary = analyze(args.artifacts, config)
        (args.artifacts / "summary.json").write_text(
            json.dumps(summary, indent=2, sort_keys=True) + "\n", encoding="utf-8"
        )
    except (EvidenceError, OSError, ValueError, json.JSONDecodeError) as exc:
        print(f"FAIL: {exc}", file=sys.stderr)
        return 1
    print(json.dumps(summary, indent=2, sort_keys=True))
    print("PASS: native Metal meets the ratified MoltenVK performance baseline")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
