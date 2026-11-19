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

from lib.frame_interval_evidence import EvidenceError, INTERVALS, full_frame_intervals
from lib.metal_acquire_evidence import acquire_scene_timing


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
METAL_PERF = re.compile(
    r"^metal perf v1: frames=(\d+) acquire_total_us=(\d+) submit_total_us=(\d+) "
    r"gpu_samples=(\d+) gpu_total_ns=(\d+) gpu_unavailable=(\d+)$"
)
METAL_PHASES = re.compile(
    r"^metal cpu phases v1: frames=(\d+) prepare_ns=(\d+) encode_pre_ns=(\d+) "
    r"acquire_ns=(\d+) encode_post_ns=(\d+) submit_ns=(\d+) retire_ns=(\d+) total_ns=(\d+)$"
)
METAL_TIMELINE = re.compile(
    r"^metal gpu timeline v1: frames=(\d+) samples=(\d+) commit_to_start_ns=(\d+) "
    r"execution_ns=(\d+) unavailable=(\d+)$"
)
METAL_SPLIT = re.compile(
    r"^metal gpu split v1: frames=(\d+) samples=(\d+) scene_ns=(\d+) "
    r"output_ns=(\d+) overlap_ns=(\d+) gap_ns=(\d+)$"
)
VK_PERF = re.compile(r"vk perf v2: (.+)$")
FORBIDDEN = re.compile(
    r"lifecycle failure|advancing cl_renderer|failed to initialize|renderer fallback|"
    r"Sys_Error|\bFATAL\b|\bVUID-|validation error|VM_Create.*failed|"
    r"VM syscall error|died on signal",
    re.IGNORECASE,
)
HISTOGRAM_UPPER_US = (1000, 2000, 4000, 8000, 12000, 16667, 24000, 33334, 50000)
BLOOM_SETTINGS = {"r_bloom": 1, "r_bloomPasses": 2, "r_bloomThresholdMode": 0,
                  "r_bloomThreshold": .32, "r_bloomIntensity": .5}
FPS_SETTINGS = {"com_maxfps": 0, "com_maxfpsUnfocused": 1000, "com_maxfpsMinimized": 30}
WINDOW_CONTEXT = re.compile(r"^frame window context v1: bucket=(\d+) count=(\d+) focused=(\d+) minimized=(\d+) automated=([01])$")


class PerformanceFailure(EvidenceError):
    """Valid measurements that fail the ratified gate, not missing evidence."""

    def __init__(self, report: Mapping[str, object]):
        self.report = report
        super().__init__("; ".join(report["failures"]))


def metal_timing(blocks: Sequence[Sequence[int]], phases: Sequence[Sequence[int]],
                 timelines: Sequence[Sequence[int]],
                 expected_frames: int) -> Mapping[str, object]:
    if not blocks or sum(b[0] for b in blocks) != expected_frames:
        raise EvidenceError("metal: incomplete acquire/submit timing coverage")
    for frames, acquire, submit, samples, duration, unavailable in blocks:
        if frames != 200 or samples + unavailable > frames or (samples == 0) != (duration == 0):
            raise EvidenceError("metal: invalid acquire/submit/GPU timing block")
    samples = sum(b[3] for b in blocks)
    if len(timelines) != len(blocks):
        raise EvidenceError("metal: incomplete GPU timeline coverage")
    for timeline, block in zip(timelines, blocks):
        frames, paired, delay, execution, unavailable = timeline
        if (frames != block[0] or paired > block[3]
                or paired + unavailable != block[3] + block[5]
                or execution > block[4] or (paired == 0) != (execution == 0)
                or (paired == 0 and delay != 0)
                or (paired == block[3] and execution != block[4])):
            raise EvidenceError("metal: inconsistent paired GPU timeline")
    paired = sum(t[1] for t in timelines)
    if len(phases) != len(blocks):
        raise EvidenceError("metal: incomplete CPU phase timing coverage")
    for phase, block in zip(phases, blocks):
        if phase[0] != block[0] or phase[-1] == 0 or sum(phase[1:7]) != phase[7]:
            raise EvidenceError("metal: invalid CPU phase partition")
        # v1 acquire totals round each frame down to microseconds; phases keep ns.
        if not 0 <= phase[3] - block[1] * 1000 < phase[0] * 1000:
            raise EvidenceError("metal: acquire phase disagrees with outer timing")
    phase_names = ("prepare", "encode_pre_acquire", "acquire", "encode_post_acquire",
                   "present_call_and_commit", "retire", "total")
    return {
        "frames": expected_frames,
        "acquire_mean_us": sum(b[1] for b in blocks) / expected_frames,
        "submit_excluding_acquire_mean_us": sum(b[2] for b in blocks) / expected_frames,
        "gpu_samples": samples,
        "gpu_unavailable": sum(b[5] for b in blocks),
        "gpu_sample_coverage": samples / expected_frames,
        "gpu_sample_mean_ms": sum(b[4] for b in blocks) / samples / 1e6 if samples else None,
        "gpu_timeline": {
            "samples": paired,
            "unavailable": sum(t[4] for t in timelines),
            "commit_to_start_mean_ms": sum(t[2] for t in timelines) / paired / 1e6 if paired else None,
            "execution_mean_ms": sum(t[3] for t in timelines) / paired / 1e6 if paired else None,
            "scope": "paired completed-submission samples, mach absolute seconds; split submissions use first commit/start and the union of both GPU execution intervals (exclude drawable idle gap); commit-to-GPU-start includes driver/scheduler delay, not pure queue time or onscreen latency; completion windows can include prior-window submissions",
        },
        "cpu_phase_mean_us": {name: sum(p[i + 1] for p in phases) / expected_frames / 1000
                              for i, name in enumerate(phase_names)},
        "cpu_phase_scope": "disjoint same-submission CPU spans; prepare includes cache/upload/readback reservation; encode_pre includes an early scene commit when split; submit is final present/commit API calls, NOT onscreen latency; retire includes explicit capture/first-readback waits and cleanup",
        "scope": "200-frame renderer windows within measurement markers; GPU is sampled, not all frames",
    }


def parse_time(value: str) -> dt.datetime:
    return dt.datetime.fromisoformat(value.replace("Z", "+00:00"))


def split_timing(splits: Sequence[Sequence[int]], blocks: Sequence[Sequence[int]]) -> Mapping[str, object]:
    if len(splits) != len(blocks) or not splits:
        raise EvidenceError("metal: missing split GPU timing coverage")
    for (frames, n, scene, output, overlap, gap), block in zip(splits, blocks):
        union = scene + output - overlap
        if (frames != block[0] or n > block[3]
                or overlap > min(scene, output)
                or (n == 0 and any((scene, output, overlap, gap)))
                or (n > 0 and (scene <= 0 or output <= 0))
                or union > block[4] + 3*n
                or (n == block[3] and abs(union - block[4]) > 3*n)):
            raise EvidenceError("metal: inconsistent split GPU intervals")
    count = sum(s[1] for s in splits)
    return {
        "samples": count,
        **{name: sum(s[index] for s in splits) / count / 1e6 if count else None
           for index, name in enumerate(("scene_mean_ms", "output_mean_ms",
                                          "overlap_mean_ms", "gap_mean_ms"), 2)},
        "scope": "split-command subset only; scene includes bloom/exposure, output includes tonemap/UI/lens query; subtract overlap from scene+output. Gap is between these submissions, NOT GPU work, pure drawable wait or physical presentation latency. Not per-pass bloom timing.",
    }


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


VK_WAIT_TOTALS = ("acquire_total_us", "submit_total_us", "present_call_total_us",
                  "main_slot_wait_total_us", "worker_queue_total_us",
                  "worker_wait_total_us", "worker_reset_total_us")
VK_WAIT_KEYS = VK_WAIT_TOTALS + ("bucket", "worker_count")


def vulkan_wait_timing(blocks, buckets, expected_frames):
    timings = [block["wait_timing"] for block in blocks]
    if not any(row is not None for row in timings):
        return None  # Historical minimal records are unavailable, not zero wait.
    if any(row is None for row in timings):
        raise EvidenceError("vulkan: partial wait timing coverage")
    if sorted(row["bucket"] for row in timings) != sorted(buckets):
        raise EvidenceError("vulkan: wait timing bucket mismatch")
    if sum(block["count"] for block in blocks) != expected_frames:
        raise EvidenceError("vulkan: wait timing frame mismatch")
    workers = sum(row["worker_count"] for row in timings)
    result = {"frames": expected_frames, "worker_samples": workers}
    for key in VK_WAIT_TOTALS:
        denominator = workers if key.startswith("worker_") else expected_frames
        result[key.replace("_total_us", "_mean_us")] = (
            sum(row[key] for row in timings) / denominator if denominator else None)
    result["scope"] = ("CPU wall spans from matched vk-perf frame buckets; acquire/submit/"
                       "present-call/main-slot-wait use attempted frames, worker queue/fence/reset "
                       "use worker samples. Worker spans overlap main-thread spans: do not sum "
                       "them into frame time. Fence wait is not pure GPU execution or onscreen latency.")
    return result


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
    timing = None
    if any(key in fields for key in VK_WAIT_KEYS):
        if any(key not in fields for key in VK_WAIT_KEYS):
            raise EvidenceError("incomplete Vulkan wait timing payload")
        try:
            timing = {key: int(fields[key]) for key in VK_WAIT_KEYS}
        except ValueError as exc:
            raise EvidenceError("non-integer Vulkan wait timing") from exc
        if (any(value < 0 for value in timing.values()) or timing["bucket"] == 0
                or timing["worker_count"] > int(fields["count"])
                or (not timing["worker_count"] and any(
                    timing[key] for key in VK_WAIT_TOTALS if key.startswith("worker_")))):
            raise EvidenceError("invalid Vulkan wait timing")
    return {
        "wait_timing": timing,
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
        if record.get("sev") in ("ERROR", "FATAL"):
            raise EvidenceError(f"{backend}: runtime {record['sev']}: {record['msg'].strip()}")
        records.append(record)
    messages = [record["msg"].strip() for record in records]
    joined = "\n".join(messages)
    if (match := FORBIDDEN.search(joined)):
        raise EvidenceError(f"{backend}: forbidden runtime evidence: {match.group(0)}")
    receipt_fields(messages, backend)
    # Scene precision is independent of swapchain/display HDR. Previously
    # r_hdr=0 made Vulkan LDR while native Metal still rendered RGBA16F.
    format_pattern = (r"Wired native Metal RAL: scene format=(\S+)"
                      if backend == "metal" else r"color \(main FBO\)\s*:\s*VK_FORMAT_(\S+)")
    scene_formats = set(re.findall(format_pattern, joined))
    if scene_formats != {"R16G16B16A16_SFLOAT"}:
        raise EvidenceError(f"{backend}: absent or mismatched HDR scene format: {scene_formats}")

    # Require runtime effective descriptors, not metadata/cvar intent alone.
    sampling_pattern = rf"Wired {backend} material sampler: min=(\d+) mag=(\d+) mip=(\d+) aniso=([\d.]+) maxLod=([\d.]+)"

    begin = [index for index, message in enumerate(messages) if message == "RAL_BACKEND_PERF_BEGIN"]
    end = [index for index, message in enumerate(messages) if message == "RAL_BACKEND_PERF_END"]
    if len(begin) != 1 or len(end) != 1 or begin[0] >= end[0]:
        raise EvidenceError(f"{backend}: invalid measurement marker pair")
    # Runtime cvar reads bracket the interval, not just launcher intent. This
    # proves enabled configuration; the native GPU oracle separately proves
    # execution. Missing bloom must never masquerade as backend speed.
    # Validate actual limiter values too: minimized=0 is outside the current
    # engine range. An unfocused 1000 cap is valid; minimized runs are rejected.
    for name, expected in (BLOOM_SETTINGS | FPS_SETTINGS).items():
        pattern = rf'"{name}" is:"([+\-0-9.eE]+)(?:\^7)?"'
        before = re.findall(pattern, "\n".join(messages[:begin[0]]))
        after = re.findall(pattern, "\n".join(messages[end[0]+1:]))
        observed = re.findall(pattern, joined)
        if (not before or not after or not observed
                or any(float(value) != expected for value in observed)):
            raise EvidenceError(f"{backend}: absent or mismatched runtime setting {name}")
    sampling = re.findall(sampling_pattern, "\n".join(messages[:begin[0]]))
    active_sampling = re.findall(sampling_pattern, "\n".join(messages[begin[0]+1:end[0]]))
    if (not sampling or tuple(map(float, sampling[-1])) != (1, 1, 1, 8, 0)
            or any(tuple(map(float, row)) != (1, 1, 1, 8, 0) for row in active_sampling)):
        raise EvidenceError(f"{backend}: absent, mismatched or changing trilinear/aniso8 material sampling")
    active = records[begin[0] + 1 : end[0]]
    traces: Dict[int, Tuple[int, int, int]] = {}
    distributions: Dict[int, Tuple[List[int], int]] = {}
    intervals: Dict[int, Sequence[int]] = {}
    window_context: Dict[int, Sequence[int]] = {}
    gpu_totals: List[float] = []
    metal_blocks: List[Sequence[int]] = []
    metal_phases: List[Sequence[int]] = []
    metal_timelines: List[Sequence[int]] = []
    metal_splits: List[Sequence[int]] = []
    gpu_pending = False
    vk_blocks: List[Mapping[str, object]] = []
    for record in active:
        message = record["msg"].strip()
        if message.startswith("frame window context"):
            match = WINDOW_CONTEXT.fullmatch(message)
            if not match:
                raise EvidenceError("malformed window context")
            bucket, count, focused, minimized, automated = map(int, match.groups())
            if bucket in window_context or count != 200 or focused > count or minimized != 0:
                raise EvidenceError("invalid or duplicate window context")
            window_context[bucket] = (count, focused, minimized, automated)
        if message.startswith("frame intervals"):
            match = INTERVALS.fullmatch(message)
            if not match:
                raise EvidenceError("malformed full-frame interval payload")
            values = list(map(int, match.groups()[:6]))
            bucket = values[0]
            if bucket in intervals:
                raise EvidenceError("duplicate full-frame interval bucket")
            intervals[bucket] = values[1:] + list(map(int, match.group(7).split(",")))
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
        if message.startswith("metal perf"):
            match = METAL_PERF.fullmatch(message)
            if not match or backend != "metal":
                raise EvidenceError("malformed or misplaced metal perf v1 payload")
            metal_blocks.append(tuple(map(int, match.groups())))
        if message.startswith("metal cpu phases"):
            match = METAL_PHASES.fullmatch(message)
            if not match or backend != "metal":
                raise EvidenceError("malformed or misplaced metal CPU phase payload")
            metal_phases.append(tuple(map(int, match.groups())))
        if message.startswith("metal gpu timeline"):
            match = METAL_TIMELINE.fullmatch(message)
            if not match or backend != "metal":
                raise EvidenceError("malformed or misplaced metal GPU timeline payload")
            metal_timelines.append(tuple(map(int, match.groups())))
        if message.startswith("metal gpu split"):
            match = METAL_SPLIT.fullmatch(message)
            if not match or backend != "metal":
                raise EvidenceError("malformed or misplaced metal GPU split payload")
            metal_splits.append(tuple(map(int, match.groups())))
        if match := VK_PERF.search(message):
            vk_blocks.append(parse_vk_perf(match.group(1)))

    if sorted(traces) != sorted(distributions) or len(traces) * 200 != expected_frames:
        raise EvidenceError(f"{backend}: trace/distribution coverage mismatch")
    if sorted(window_context) != sorted(traces) or len({v[3] for v in window_context.values()}) != 1:
        raise EvidenceError("missing or changing window context")
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
    # Swapchain extent alone does not establish equal work. Vulkan's legacy
    # r_renderScale=1 enables an independent (default 1280x720) scene target.
    if backend == "metal":
        scene_extents = set(tuple(map(int, pair)) for pair in re.findall(
            r"Wired native Metal RAL: scene format=\S+ extent=(\d+)x(\d+)", joined))
    else:
        scene_extents = set()
        for render_w, render_h, output_w, output_h in re.findall(
                r"(?:RENDER: (\d+) x (\d+), )?MODE: -?\d+, (\d+) x (\d+) (?:windowed|fullscreen)", joined):
            scene_extents.add((int(render_w or output_w), int(render_h or output_h)))
    if scene_extents != {pixel_extent}:
        raise EvidenceError(f"{backend}: missing or scaled scene extent {scene_extents}; presentation={pixel_extent}")
    return {
        "frames": expected_frames,
        "frame_intervals": full_frame_intervals(intervals, sorted(traces), expected_frames),
        "bloom_settings": dict(BLOOM_SETTINGS),
        "fps_settings": dict(FPS_SETTINGS),
        "window_context": {
            "automated": next(iter(window_context.values()))[3],
            "focused_frames": sum(v[1] for v in window_context.values()),
            "minimized_frames": sum(v[2] for v in window_context.values()),
            "frames": expected_frames,
            "scope": "engine OS-focus/minimize event state at frame end; not occlusion, GPU power state or physical presentation evidence",
        },
        "cpu_work_mean_us": cpu_total / expected_frames,
        "scr_end_mean_us": scr_total / expected_frames,
        "p50_upper_us": percentile_upper(all_bins, 0.50, maximum),
        "p95_upper_us": percentile_upper(all_bins, 0.95, maximum),
        "p99_upper_us": percentile_upper(all_bins, 0.99, maximum),
        "max_us": maximum,
        "work_ge4ms_count": sum(all_bins[3:]),
        "work_timing_scope": "main-thread work span including renderer waits, excludes pacing/outer-frame work; histogram percentiles are upper bounds, not exact full-frame intervals",
        "hitch_ge24ms_ratio": sum(all_bins[7:]) / expected_frames,
        "histogram": all_bins,
        "wall_seconds": wall_seconds,
        "wall_fps": expected_frames / wall_seconds,
        "wall_fps_statistic": "per-run average throughput, not minimum frame rate",
        "gpu_total_median_ms": statistics.median(gpu_totals) if gpu_totals else None,
        "gpu_blocks": len(gpu_totals),
        "metal_timing": metal_timing(metal_blocks, metal_phases, metal_timelines, expected_frames) if backend == "metal" else None,
        "metal_gpu_split": split_timing(metal_splits, metal_blocks) if backend == "metal" else None,
        "metal_acquire_scene": acquire_scene_timing(messages[begin[0]+1:end[0]]) if backend == "metal" else None,
        "vulkan_wait_timing": vulkan_wait_timing(vk_blocks, traces, expected_frames) if backend == "vulkan" else None,
        "present_mode": present_mode,
        "pixel_extent": list(pixel_extent),
        "scene_extent": list(next(iter(scene_extents))),
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
                if runs[backend][str(scene)][-1]["window_context"]["automated"] != meta.get("automated"):
                    raise EvidenceError("runtime/metadata automated context mismatch")

    pinned = (
        "engine_sha256", "build_profile", "width", "height", "legacy_render_scale", "render_extent_policy",
        "swap_interval", "warmup_frames", "measurement_frames", "run_id", "scene_format", "automated",
    )
    for field in pinned:
        values = {json.dumps(meta.get(field), sort_keys=True) for meta in metadata}
        if len(values) != 1:
            raise EvidenceError(f"capture configuration drift: {field}")
    first = metadata[0]
    if first.get("scene_format") != "R16G16B16A16_SFLOAT":
        raise EvidenceError("benchmark scene-format policy drift")
    if first.get("build_profile") != "Release" or first.get("width") != 1280 or first.get("height") != 720:
        raise EvidenceError("benchmark is not the pinned Release 1280x720 profile")
    if (first.get("swap_interval") != 0 or first.get("legacy_render_scale") != 0
            or first.get("render_extent_policy") != "native-presentation"):
        raise EvidenceError("benchmark presentation/render-scale policy drift")
    if not re.fullmatch(r"[0-9a-f]{64}", str(first.get("engine_sha256", ""))):
        raise EvidenceError("invalid engine SHA-256")
    for backend in BACKENDS:
        identities = {str(meta.get("renderer_sha256", ""))
                      for meta in metadata if meta["backend"] == backend}
        if len(identities) != 1 or not re.fullmatch(r"[0-9a-f]{64}", next(iter(identities))):
            raise EvidenceError(f"{backend}: invalid or changed renderer SHA-256")

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
    failures: List[str] = []
    if metal_corpus > limit:
        failures.append(
            f"native Metal corpus CPU-work geomean regressed: {metal_corpus:.1f}us > {limit:.1f}us"
        )
    # Presentation-regime stability: macOS windowed presentation flips between a
    # compositor-locked regime (Metal 120 / MoltenVK 240 FPS) and a decoupled
    # regime (400+ FPS) on a minutes timescale. Runs of one backend must share a
    # regime, otherwise the matrix silently mixes regimes and every average,
    # ratio and minimum below is meaningless. Reported and gated per backend.
    regime_spread_limit = float(config.get("regime_fps_spread_limit", 1.35))
    regime: Dict[str, object] = {"fps_spread_limit": regime_spread_limit, "backends": {}}
    for backend in BACKENDS:
        fps_values = [float(run["wall_fps"]) for scene in scenes for run in results[backend][scene]["runs"]]
        spread = max(fps_values) / min(fps_values) if min(fps_values) > 0 else float("inf")
        regime["backends"][backend] = {
            "wall_fps": [round(v, 1) for v in fps_values],
            "fps_spread": round(spread, 4),
            "classification": "decoupled" if min(fps_values) >= 400 else (
                "locked" if max(fps_values) < 260 else "mixed-or-transitional"),
        }
        if spread > regime_spread_limit:
            failures.append(
                f"{backend}: presentation regime unstable across the matrix "
                f"(wall FPS spread {spread:.2f} > {regime_spread_limit:g})"
            )
    max_hitch_delta = float(config["max_hitch_ratio_delta"])
    minimum_fps = float(config.get("minimum_wall_fps", 0))
    interval_budget = int(config.get("maximum_frame_interval_ns", 4000000))
    if interval_budget != 4000000:
        raise EvidenceError("full-frame interval budget must remain 4ms")
    for scene in scenes:
        metal_hitch = float(results["metal"][scene]["hitch_ge24ms_ratio_max"])
        vulkan_hitch = float(results["vulkan"][scene]["hitch_ge24ms_ratio_max"])
        if metal_hitch > vulkan_hitch + max_hitch_delta:
            failures.append(f"{scene}: native Metal hitch ratio regressed")
        for backend in BACKENDS:
            if any(float(run["wall_fps"]) < minimum_fps
                   for run in results[backend][scene]["runs"]):
                failures.append(f"{backend}/{scene}: wall FPS below {minimum_fps:g}")
            if any(run["frame_intervals"]["max_ns"] > interval_budget
                   for run in results[backend][scene]["runs"]):
                failures.append(f"{backend}/{scene}: full-frame intervals exceed 4ms")

    report = {
        "schema": 1,
        "presentation_regime": regime,
        "engine_sha256": first["engine_sha256"],
        "build_profile": first["build_profile"],
        "configuration": {
            "logical_output": [first["width"], first["height"]],
            "legacy_render_scale": first["legacy_render_scale"],
            "render_extent_policy": first["render_extent_policy"],
            "swap_interval": first["swap_interval"],
            "warmup_frames": first["warmup_frames"],
            "measurement_frames": first["measurement_frames"],
            "repetitions": repetitions,
            "scene_format": first["scene_format"],
            "automated": first["automated"],
            "minimum_wall_fps": minimum_fps,
            "maximum_frame_interval_ns": interval_budget,
            "acceptance_scope": "static arena1/17 diagnostic subset; not the >=30s arena1/7/17 gameplay acceptance",
        },
        "backends": results,
        "ratified_statistic": "arena1/arena17 geometric mean of counterbalanced per-scene aggregate CPU-work usec/frame",
        "native_metal_corpus_us": round(metal_corpus, 3),
        "moltenvk_corpus_us": round(vulkan_corpus, 3),
        "native_to_moltenvk_ratio": round(metal_corpus / vulkan_corpus, 5),
        "ratio_limit": ratio_limit,
        "fixed_allowance_us": allowance,
        "status": "FAIL" if failures else "PASS",
        "failures": failures,
    }
    if failures:
        raise PerformanceFailure(report)
    return report


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
        fps_lines = [f'"{name}" is:"{60 if defect == "fps_capped" and name == "com_maxfpsUnfocused" else value}"'
                     for name, value in FPS_SETTINGS.items()]
        if defect != "fps_missing":
            lines.extend(_record("2026-08-24T00:00:00+00:00", line) for line in fps_lines)
        bloom_lines = [f'"{name}" is:"{0 if defect == "bloom_disabled" and name == "r_bloom" else value}"'
                       for name, value in BLOOM_SETTINGS.items()]
        if defect != "bloom_missing":
            lines.extend(_record("2026-08-24T00:00:00+00:00", line) for line in bloom_lines)
        if defect != "sampler_missing":
            mip = 0 if defect == "sampler_mip" else 1
            aniso = 1 if defect == "sampler_aniso" else 8
            lines.append(_record("2026-08-24T00:00:00+00:00",
                f"Wired {backend} material sampler: min=1 mag=1 mip={mip} aniso={aniso} maxLod=0"))
        scene_format = "R8G8B8A8_UNORM" if defect == "scene_format" else "R16G16B16A16_SFLOAT"
        if defect != "scene_format_missing":
            prefix = ("Wired native Metal RAL: scene format=" if backend == "metal"
                      else "color (main FBO) : VK_FORMAT_")
            extent = "" if defect == "scene_extent_missing" else " extent=2560x1440"
            lines.append(_record("2026-08-24T00:00:00+00:00", prefix + scene_format + (extent if backend == "metal" else "")))
        if backend == "vulkan" and defect != "scene_extent_missing":
            render_prefix = "RENDER: 1280 x 720, " if defect == "scene_extent_scaled" else ""
            lines.append(_record("2026-08-24T00:00:00+00:00", render_prefix + "MODE: -1, 2560 x 1440 windowed hz:"))
        if defect == "fx_rejected":
            lines.append(json.dumps({
                "ts": "2026-08-24T00:00:00+00:00", "sev": "ERROR",
                "cat": "wired-fx", "msg": "rejected profile 1: load/validation failure",
            }))
        if backend == "metal":
            mode = 0 if defect == "present" else 2
            lines.append(_record("2026-08-24T00:00:00+00:00", f"Wired native Metal RAL: registration ready logical=1280x720 pixels=2560x1440 swapInterval=0 presentMode={mode} displaySync=0"))
        lines.append(_record("2026-08-24T00:00:01+00:00", "RAL_BACKEND_PERF_BEGIN"))
        for bucket in range(1, 4):
            if defect != "window_missing":
                focus = 201 if defect == "window_invalid" else 0
                minimized = 1 if defect == "window_minimized" else 0
                context = f"frame window context v1: bucket={bucket} count=200 focused={focus} minimized={minimized} automated=1"
                lines.append(_record("2026-08-24T00:00:02+00:00", context))
                if defect == "window_duplicate":
                    lines.append(_record("2026-08-24T00:00:02+00:00", context))
            if defect != "interval_missing":
                n = 199 if bucket == 1 else 200
                duration_ns = 4000001 if defect == "interval_slow" else mean * 1000
                bins = [0] * 33
                bins[min(32, duration_ns // 250000)] = n
                gt4 = n if duration_ns > 4000000 else 0
                if defect == "interval_gt4": gt4 = n
                invalid = 1 if defect == "interval_clock" else 0
                count = n + 1 if defect == "interval_count" else n
                if defect == "interval_histogram": bins[0] += 1
                interval_payload = f"frame intervals v1: bucket={bucket} count={count} invalid={invalid} total_ns={n * duration_ns} max_ns={duration_ns} gt4ms={gt4} bins={','.join(map(str, bins))}"
                if defect == "interval_malformed": interval_payload += " extra=1"
                lines.append(_record("2026-08-24T00:00:02+00:00", interval_payload))
                if defect == "interval_duplicate":
                    lines.append(_record("2026-08-24T00:00:02+00:00", interval_payload))
            lines.append(_record("2026-08-24T00:00:02+00:00", f"frame trace (200f): bucket={bucket} count=200 valid=200 cpu_work_total={mean * 200}us scr_end_total={mean * 100}us"))
            total = 199 if defect == "histogram" and backend == "metal" and bucket == 1 else 200
            lines.append(_record("2026-08-24T00:00:02+00:00", f"frame work distribution (200f): bucket={bucket} lt1ms=0 lt2ms={total} lt4ms=0 lt8ms=0 lt12ms=0 lt16_667ms=0 lt24ms=0 lt33_334ms=0 lt50ms=0 ge50ms=0 max_us={mean}"))
            if backend == "metal" and defect != "metal_missing":
                block_frames = 199 if defect == "metal_frames" else 200
                samples = 201 if defect == "metal_samples" else 10
                duration = 0 if defect == "metal_duration" else 10000000
                payload = f"metal perf v1: frames={block_frames} acquire_total_us=80000 submit_total_us=100000 gpu_samples={samples} gpu_total_ns={duration} gpu_unavailable=0"
                if defect == "metal_malformed":
                    payload += " extra=1"
                lines.append(_record("2026-08-24T00:00:02+00:00", payload))
                if defect != "metal_timeline_missing":
                    paired = 11 if defect == "metal_timeline_samples" else 10
                    paired_duration = 9000000 if defect == "metal_timeline_duration" else 10000000
                    timeline = f"metal gpu timeline v1: frames=200 samples={paired} commit_to_start_ns=2000000 execution_ns={paired_duration} unavailable=0"
                    if defect == "metal_timeline_malformed":
                        timeline += " extra=1"
                    lines.append(_record("2026-08-24T00:00:02+00:00", timeline))
                    if defect == "metal_timeline_duplicate":
                        lines.append(_record("2026-08-24T00:00:02+00:00", timeline))
                if defect != "split_missing":
                    overlap = 9000000 if defect == "split_overlap" else 1000000
                    split = f"metal gpu split v1: frames=200 samples=10 scene_ns=8000000 output_ns=3000000 overlap_ns={overlap} gap_ns=2000000"
                    if defect == "split_malformed": split += " extra=1"
                    lines.append(_record("2026-08-24T00:00:02+00:00", split))
                    if defect == "split_duplicate":
                        lines.append(_record("2026-08-24T00:00:02+00:00", split))
                lines.append(_record("2026-08-24T00:00:02+00:00",
                    "metal acquire scene v1: frames=200 samples=10 unavailable=0 acquire_ns=4000000 before_scene_ns=1000000 during_scene_ns=1000000 after_scene_ns=2000000"))
                if defect != "metal_phases_missing":
                    phase_total = 181000000 if defect == "metal_phases_sum" else 180000000
                    phase_acquire = 81000000 if defect == "metal_phases_acquire" else 80000000
                    # Keep the partition valid while disagreeing with outer acquire timing.
                    phase_prepare = 40000000 - (phase_acquire - 80000000)
                    phase_payload = f"metal cpu phases v1: frames={block_frames} prepare_ns={phase_prepare} encode_pre_ns=40000000 acquire_ns={phase_acquire} encode_post_ns=10000000 submit_ns=5000000 retire_ns=5000000 total_ns={phase_total}"
                    lines.append(_record("2026-08-24T00:00:02+00:00", phase_payload))
                    if defect == "metal_phases_duplicate":
                        lines.append(_record("2026-08-24T00:00:02+00:00", phase_payload))
                if defect == "metal_duplicate":
                    lines.append(_record("2026-08-24T00:00:02+00:00", payload))
            if backend == "vulkan":
                mode = "FIFO" if defect == "present" else "IMMEDIATE"
                lines.append(_record("2026-08-24T00:00:02+00:00", f"vk perf v2: mode={mode} r_swapInterval=0 extent=2560x1440 count=200 valid=200 identity_stable=1"))
        lines.append(_record("2026-08-24T00:00:04+00:00", "RAL_BACKEND_PERF_END"))
        if defect not in ("fps_missing", "fps_end_missing"):
            lines.extend(_record("2026-08-24T00:00:04+00:00",
                line.replace('is:"1000"', 'is:"60"') if defect == "fps_drift" else line)
                for line in fps_lines)
        if defect not in ("bloom_missing", "bloom_end_missing"):
            lines.extend(_record("2026-08-24T00:00:04+00:00",
                line.replace('is:"1"', 'is:"0"') if defect == "bloom_drift" else line)
                for line in bloom_lines)
        stem = directory / "arena1-r1"
        stem.with_suffix(".qconsole.jsonl").write_text("\n".join(lines) + "\n", encoding="utf-8")
        stem.with_suffix(".stdout.log").write_text("clean runtime\n", encoding="utf-8")
        meta = {
            "engine_sha256": ("b" * 64 if defect == "sha" and backend == "metal" else "a" * 64),
            "renderer_sha256": ("c" if backend == "metal" else "d") * 64,
            "build_profile": "Release", "width": 1280, "height": 720, "automated": 1,
            "legacy_render_scale": 0, "render_extent_policy": "native-presentation",
            "swap_interval": 0, "warmup_frames": 300,
            "measurement_frames": 600, "run_id": "selftest", "backend": backend,
            "map": "arena1", "repetition": 1, "camera": [1, 2, 3, 4],
            "scene_format": "R16G16B16A16_SFLOAT",
        }
        stem.with_suffix(".meta.json").write_text(json.dumps(meta), encoding="utf-8")
    if defect == "stale":
        stale = root / "metal" / "arena1-r1.qconsole.jsonl"
        old = (root / "run.marker").stat().st_mtime - 10
        os.utime(stale, (old, old))
    return config


def self_test() -> int:
    failures: List[str] = []
    minimal_vk = "mode=IMMEDIATE r_swapInterval=0 extent=2560x1440 count=200 valid=200 identity_stable=1"
    assert vulkan_wait_timing([parse_vk_perf(minimal_vk)], [1], 200) is None
    wait_vk = (minimal_vk + " bucket=1 worker_count=100 acquire_total_us=2000 "
               "submit_total_us=400 present_call_total_us=600 main_slot_wait_total_us=8000 "
               "worker_queue_total_us=5000 worker_wait_total_us=7000 worker_reset_total_us=100")
    wait_row = parse_vk_perf(wait_vk)
    wait_result = vulkan_wait_timing([wait_row], [1], 200)
    assert wait_result["acquire_mean_us"] == 10 and wait_result["worker_wait_mean_us"] == 70
    no_workers = dict(wait_row)
    no_workers["wait_timing"] = dict(wait_row["wait_timing"])
    for key in VK_WAIT_KEYS:
        if key.startswith("worker_"):
            no_workers["wait_timing"][key] = 0
    assert vulkan_wait_timing([no_workers], [1], 200)["worker_wait_mean_us"] is None
    for defect in ("worker_count=201", "worker_count=0", "worker_count=-1", "worker_count=nan"):
        try:
            parse_vk_perf(wait_vk.replace("worker_count=100", defect))
        except EvidenceError:
            pass
        else:
            raise AssertionError("invalid Vulkan worker coverage accepted")
    for malformed in (wait_vk.replace("acquire_total_us=2000", ""),
                      wait_vk.replace("acquire_total_us=2000", "acquire_total_us=-1")):
        try:
            parse_vk_perf(malformed)
        except EvidenceError:
            pass
        else:
            raise AssertionError("invalid Vulkan wait payload accepted")
    for rows, buckets, frames in (([wait_row, wait_row], [1,2], 400),
                                  ([wait_row], [2], 200), ([wait_row], [1], 400),
                                  ([wait_row, parse_vk_perf(minimal_vk)], [1,2], 400)):
        try:
            vulkan_wait_timing(rows, buckets, frames)
        except EvidenceError:
            pass
        else:
            raise AssertionError("invalid Vulkan wait window accepted")
    empty_split = split_timing([(200,0,0,0,0,0)], [(200,0,0,0,0,10)])
    assert empty_split["samples"] == 0 and empty_split["scene_mean_ms"] is None
    partial_split = split_timing([(200,1,700000,400000,100000,0)], [(200,0,0,2,2000000,0)])
    assert partial_split["samples"] == 1 and partial_split["scene_mean_ms"] == .7
    for invalid in ((200,0,1,0,0,0), (200,3,1,1,0,0), (200,2,100,100,0,0)):
        try:
            split_timing([invalid], [(200,0,0,2,2000000,0)])
        except EvidenceError:
            pass
        else:
            raise AssertionError("invalid split attribution accepted")
    # Unavailable timing is not a zero-latency sample; partial pairs must keep
    # their own execution denominator, not borrow the full GPU population.
    phase = [(200, 40000000, 40000000, 80000000, 10000000, 5000000, 5000000, 180000000)]
    missing = metal_timing([(200, 80000, 100000, 0, 0, 10)], phase,
                           [(200, 0, 0, 0, 10)], 200)["gpu_timeline"]
    assert missing["samples"] == 0 and missing["commit_to_start_mean_ms"] is None
    partial = metal_timing([(200, 80000, 100000, 10, 10000000, 0)], phase,
                           [(200, 5, 0, 4000000, 5)], 200)["gpu_timeline"]
    assert partial["samples"] == 5 and partial["execution_mean_ms"] == 0.8
    try:
        metal_timing([(200, 80000, 100000, 0, 0, 10)], phase,
                     [(200, 0, 1, 0, 10)], 200)
    except EvidenceError:
        pass
    else:
        raise AssertionError("unavailable samples cannot claim elapsed queue time")
    with tempfile.TemporaryDirectory(prefix="ral-backend-perf-selftest-") as temp:
        base = Path(temp)
        config = _fixture(base / "pass")
        try:
            report = analyze(base / "pass", config)
            split = report["backends"]["metal"]["arena1"]["runs"][0]["metal_gpu_split"]
            assert split["samples"] == 30 and split["scene_mean_ms"] == .8
            assert split["output_mean_ms"] == .3 and split["overlap_mean_ms"] == .1
            acquire = report["backends"]["metal"]["arena1"]["runs"][0]["metal_acquire_scene"]
            assert acquire["samples"] == 30 and acquire["acquire_mean_ms"] == .4
            assert acquire["during_scene_mean_ms"] == .1 and acquire["after_scene_mean_ms"] == .2
            timing = report["backends"]["metal"]["arena1"]["runs"][0]["metal_timing"]
            assert timing["acquire_mean_us"] == 400 and timing["submit_excluding_acquire_mean_us"] == 500
            assert timing["gpu_samples"] == 30 and timing["gpu_sample_mean_ms"] == 1
            assert timing["gpu_timeline"]["samples"] == 30
            assert timing["gpu_timeline"]["commit_to_start_mean_ms"] == 0.2
            assert timing["gpu_timeline"]["execution_mean_ms"] == 1
            interval = report["backends"]["metal"]["arena1"]["runs"][0]["frame_intervals"]
            assert interval["count"] == 599 and interval["gt4ms_count"] == 0
            assert interval["mean_ns"] == 1000000 and interval["max_ns"] == 1000000
            assert interval["p99_lower_ns"] == 1000000 and interval["p99_upper_ns"] == 1000000
            assert timing["gpu_sample_coverage"] == .05
            assert timing["cpu_phase_mean_us"]["prepare"] == 200
            assert timing["cpu_phase_mean_us"]["total"] == 900
        except EvidenceError as exc:
            failures.append(f"valid fixture rejected: {exc}")
        floor_config = dict(config, minimum_wall_fps=250)
        try:
            analyze(base / "pass", floor_config)  # fixture: 600 frames / 3s
            failures.append("sub-250 FPS fixture accepted")
        except PerformanceFailure as exc:
            assert any("wall FPS below" in reason for reason in exc.report["failures"])
        # A real N=3 epoch must include all runs and retain each backend binary.
        repeated = base / "repeated"
        repeat_config = dict(_fixture(repeated))
        repeat_config["repetitions"] = 3
        for backend in BACKENDS:
            for repetition in (2, 3):
                source = repeated / backend / "arena1-r1"
                target = repeated / backend / f"arena1-r{repetition}"
                for suffix in (".qconsole.jsonl", ".stdout.log", ".meta.json"):
                    target.with_suffix(suffix).write_bytes(source.with_suffix(suffix).read_bytes())
                meta_path = target.with_suffix(".meta.json")
                meta = json.loads(meta_path.read_text())
                meta["repetition"] = repetition
                meta_path.write_text(json.dumps(meta))
        assert analyze(repeated, repeat_config)["configuration"]["repetitions"] == 3
        changed = repeated / "metal" / "arena1-r3.meta.json"
        original = changed.read_text()
        for identity in ("e" * 64, "not-a-digest"):
            meta = json.loads(original)
            meta["renderer_sha256"] = identity
            changed.write_text(json.dumps(meta))
            try:
                analyze(repeated, repeat_config)
                failures.append("changed/invalid repeated renderer identity accepted")
            except EvidenceError:
                pass
        changed.write_text(original)
        missing = repeated / "vulkan" / "arena1-r3.stdout.log"
        missing.unlink()
        try:
            analyze(repeated, repeat_config)
            failures.append("incomplete N=3 accepted")
        except EvidenceError:
            pass
        for defect in ("fps_missing", "fps_end_missing", "fps_capped", "fps_drift",
                       "window_missing", "window_invalid", "window_duplicate", "window_minimized",
                       "split_missing", "split_overlap", "split_malformed", "split_duplicate",
                       "bloom_missing", "bloom_disabled", "bloom_end_missing", "bloom_drift",
                       "regression", "histogram", "present", "sha", "stale", "fx_rejected",
                       "metal_missing", "metal_frames", "metal_samples", "metal_duration",
                       "metal_malformed", "metal_duplicate", "scene_format", "scene_format_missing",
                       "metal_phases_missing", "metal_phases_sum", "metal_phases_acquire", "metal_phases_duplicate",
                       "metal_timeline_missing", "metal_timeline_samples", "metal_timeline_duration",
                       "metal_timeline_malformed", "metal_timeline_duplicate",
                       "interval_missing", "interval_malformed", "interval_duplicate",
                       "interval_count", "interval_clock", "interval_histogram", "interval_gt4", "interval_slow",
                       "scene_extent_scaled", "scene_extent_missing", "sampler_missing", "sampler_mip", "sampler_aniso"):
            config = _fixture(base / defect, defect)
            try:
                analyze(base / defect, config)
            except PerformanceFailure as exc:
                assert defect in ("regression", "interval_slow") and exc.report["status"] == "FAIL"
                assert exc.report["backends"]["metal"]["arena1"]["runs"][0]["metal_timing"]
                continue
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
    except PerformanceFailure as exc:
        # Retain validated diagnostics on FAIL too. Exit code and thresholds
        # remain authoritative; this is not a relaxed acceptance mode.
        try:
            (args.artifacts / "summary.json").write_text(
                json.dumps(exc.report, indent=2, sort_keys=True) + "\n", encoding="utf-8"
            )
        except OSError as error:
            print(f"FAIL: cannot write failure diagnostics: {error}", file=sys.stderr)
        print(f"FAIL: {exc}", file=sys.stderr)
        return 1
    except (EvidenceError, OSError, ValueError, json.JSONDecodeError) as exc:
        print(f"FAIL: {exc}", file=sys.stderr)
        return 1
    print(json.dumps(summary, indent=2, sort_keys=True))
    print("PASS: native Metal meets the ratified MoltenVK performance baseline")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
