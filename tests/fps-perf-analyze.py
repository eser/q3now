#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors

"""Fail-closed analyzer and synthetic classifier tests for fps-perf-gate.sh."""

from __future__ import annotations

import argparse
import datetime as dt
import json
import math
import os
import platform
import re
import statistics
import tempfile
from dataclasses import dataclass, replace
from pathlib import Path

from lib.frame_interval_evidence import EvidenceError, INTERVALS, completed_frame_intervals, full_frame_intervals
from lib.metal_acquire_evidence import acquire_scene_timing


RE_ALL = re.compile(
    r"frame:(\d+)\s+all:\s*(\d+)\s+sv:\s*(\d+)\s+ev:\s*(\d+)"
    r"\s+cl:\s*(\d+)\s+gm:\s*(\d+)\s+rf:\s*(\d+)\s+bk:\s*(\d+)"
)
RE_END = re.compile(r"\bend:(\d+)")
RE_GPU_HDR = re.compile(r"^gpu \((\d+)f avg, ms\):")
RE_METAL_GPU_HDR = re.compile(
    r"^metal perf v1: frames=(\d+) acquire_total_us=(\d+) submit_total_us=(\d+) "
    r"gpu_samples=(\d+) gpu_total_ns=(\d+) gpu_unavailable=(\d+)$"
)
RE_GPU_LABEL = re.compile(r"^\s*([A-Za-z_]+)=([\d.]+)")
RE_GPU_TOTAL = re.compile(r"^\s*total=([\d.]+)")
RE_PACING_SPIKE = re.compile(r"^vkPacing v1: frames=(\d+) over4ms=(\d+) dominant=(\S+) max_us=(\S+)$")
RE_GPU_SPIKE = re.compile(r"^gpuSpike v1: frames=(\d+) over4ms=(\d+) max_frame_ms=([\d.]+) dominant=(\S+) lane_max=(\S+)$")
RE_VKT = re.compile(
    r"vk timing \((\d+)f avg\): fence=(\d+)ms/f\s+ft_fence=(\d+)ms/f"
    r"\s+acquire=(\d+)ms/f\s+submit=(\d+)ms/f\s+present=(\d+)ms/f"
    r"\s+draws=(\d+)/f\(msdf=\d+\)\s+pipebinds=\d+/f\(msdf=\d+\)"
)
RE_VK_PACING = re.compile(
    r"vk pacing \((\d+)f avg\): bucket=(\d+) slots=(\d+) main_slot_wait=(\d+)us/f"
    r" worker_queue=(\d+)us/work worker_fence=(\d+)us/work"
    r" worker_count=(\d+) max_queue=(\d+)"
)
RE_VK_PERF_V2 = re.compile(r"^vk perf v2: (.+)$")
RE_FRAME_PACING = re.compile(
    r"frame trace \((\d+)f\): bucket=(\d+) count=(\d+) valid=(\d+)"
    r" cpu_work_total=(\d+)us scr_end_total=(\d+)us"
)
FPS_SETTINGS = {"com_maxfps": 0, "com_maxfpsUnfocused": 1000, "com_maxfpsMinimized": 30}
RE_FPS_SETTING = re.compile(r'^"(com_maxfps(?:Unfocused|Minimized)?)"\s+is:\s*"([+\-0-9.eE]+)(?:\^7)?"')
RE_GAMEPLAY_MAP = re.compile(r"FIRST GAMEPLAY FRAME mapname=maps/([^\s.]+)\.bsp")
RE_CAMERA = re.compile(r"^(-?\d+) (-?\d+) (-?\d+) (-?\d+) (-?\d+)$")
RE_DLIGHT = re.compile(r"dlight srf:(\d+)\s+culled:(\d+)\s+verts:(\d+)\s+tris:(\d+)")
RE_PARTICLES = re.compile(r'"r_particles"\s+is:\s*"?1"?')
RE_WEATHER = re.compile(r'"g_envWeather"\s+is:\s*"rain"')
RE_DLIGHT_CANDIDATES = re.compile(r"dlight-shadow-candidates: (\d+) peak: (\d+)")
RE_DLIGHT_PROFILE = re.compile(
    r"dlightShadowProfile: (\d+) lights x 6 = (\d+) passes/frame, ([\d.]+) us/frame CPU-submit \(\d+ frames\)"
)
RE_MD3 = re.compile(
    r"\(md3\) (\d+) sin (\d+) sclip\s+(\d+) sout (\d+) bin (\d+) bclip (\d+) bout"
)
RE_BOT_PLACED = re.compile(
    r"bot_teleport: .+ placed at \(989 1575 69\) yaw=295"
)
RE_WORLD = re.compile(
    r"(\d+)/(\d+) shaders/surfs (\d+) leafs (\d+) verts (\d+)/(\d+) tris [\d.]+ mtex"
)
RE_RENDER_EXTENT = re.compile(
    r"^RENDER: (\d+) x (\d+), MODE: -?\d+, (\d+) x (\d+) (?:windowed|fullscreen) hz:"
)
RE_PARTICLE_RUNTIME = re.compile(
    r"^particle emitters:(\d+) particles:(\d+) requests:(\d+) "
    r"dropped:(\d+)/(\d+) computes:(\d+) draws:(\d+)"
    r"(?: child-events:\d+ child-particles:\d+ child-dropped:\d+/\d+)?$"
)
RE_ATMOSPHERE_RUNTIME = re.compile(
    r"^atmosphere particles:(\d+) computes:(\d+) draws:(\d+) froxels:(\d+) "
    r"dispatches:(\d+) clouds:(\d+) heightgrid:(\d+) depth:(\d+) "
    r"temporal:(\d+)/(\d+) surface-milli:(\d+)/(\d+)/(\d+)/(\d+)"
    r"(?: sky-work pending:\d+ inflight:\d+ admitted:\d+ completed:\d+ cancelled:\d+)?$"
)

VK_PERF_V2_KEYS = {
    "epoch", "bucket", "count", "valid", "acquire_total_us", "submit_total_us",
    "present_call_total_us", "main_slot_wait_total_us", "worker_queue_total_us",
    "worker_wait_total_us", "worker_reset_total_us", "worker_count", "maxq",
    "generation", "mode", "r_swapInterval", "policy_intent", "requested_images",
    "actual_images", "frames_in_flight", "vrr_preferred", "slots", "extent",
    "platform", "worker_mode", "draws_total", "image_hits",
    "identity_stable",
}
VK_PERF_V2_INTEGER_KEYS = VK_PERF_V2_KEYS - {
    "mode", "extent", "platform", "worker_mode", "image_hits",
}


def parse_vk_perf_v2(payload: str) -> dict | None:
    fields: dict[str, str] = {}
    for token in payload.split():
        if token.count("=") != 1:
            return None
        key, value = token.split("=", 1)
        if not key or not value or key in fields:
            return None
        fields[key] = value
    if set(fields) != VK_PERF_V2_KEYS:
        return None
    try:
        parsed: dict = {key: int(fields[key]) for key in VK_PERF_V2_INTEGER_KEYS}
        parsed.update({key: fields[key] for key in ("mode", "platform", "worker_mode")})
        extent = fields["extent"].split("x")
        if len(extent) != 2:
            return None
        parsed["extent"] = (int(extent[0]), int(extent[1]))
        hits: dict[int, int] = {}
        if fields["image_hits"] == "none":
            parsed["image_hits"] = hits
            return parsed
        for item in fields["image_hits"].split(","):
            index_text, separator, count_text = item.partition(":")
            if separator != ":":
                return None
            index, count = int(index_text), int(count_text)
            if index < 0 or count < 0 or index in hits:
                return None
            hits[index] = count
        parsed["image_hits"] = hits
        return parsed
    except (TypeError, ValueError):
        return None


@dataclass(frozen=True)
class Contract:
    tag: str
    map_name: str
    bots: int
    target_fps: float
    hold_frames: int
    viewpos: tuple[int, int, int, int, int]
    render_width: int = 1280
    render_height: int = 720
    draw_floor: int = 30
    engine_sha256: str = "unknown"
    pax21_sha256: str = "unknown"
    particle_changed: int = 100000
    particle_mad: float = 0.1
    particle_pixels: int = 921600
    particle_control_changed: int = 0
    particle_control_mad: float = 0.0


def finite(value: float | None) -> bool:
    return value is not None and math.isfinite(value)


def percentile(values: list[int], fraction: float) -> float | None:
    if not values:
        return None
    ordered = sorted(values)
    index = max(0, min(len(ordered) - 1, math.ceil(len(ordered) * fraction) - 1))
    return float(ordered[index])


def analyze(path: Path, contract: Contract, *, emit: bool = True) -> dict:
    alls: list[int] = []
    ends: list[int] = []
    frame_numbers: list[int] = []
    pending_frame: tuple[int, int] | None = None
    pair_errors = 0
    raw_vkt: list[tuple[str, ...]] = []
    raw_vk_pacing: list[tuple[str, ...]] = []
    raw_vk_v2: list[dict] = []
    vk_v2_parse_errors = 0
    raw_frame_pacing: list[tuple[str, ...]] = []
    intervals: dict[int, list[int]] = {}
    interval_errors: list[str] = []
    fps_settings: dict[str, list[tuple[str, float]]] = {name: [] for name in FPS_SETTINGS}
    vk_pacing_pair_errors = 0
    pending_vk_pacing = False
    raw_gpu: list[dict[str, float]] = []
    raw_gpu_spikes: list[dict[str, object]] = []
    cur_gpu: dict[str, float] | None = None
    gpu_headers = 0
    partial_gpu = 0
    measurement_state = "before"
    scene_state = "before"
    begin_markers = end_markers = 0
    scene_begin_markers = scene_end_markers = 0
    begin_time: str | None = None
    end_time: str | None = None
    marker_errors: list[str] = []
    malformed_active = 0
    gameplay_maps: list[str] = []
    entrant_broadcasts = 0
    lifecycle_errors: list[str] = []
    camera_armed = False
    camera_observations: list[tuple[int, int, int, int, int]] = []
    noclip_on = False
    bot_vis_state = "none"
    bot_vis_pre: list[int] = []
    bot_vis_post: list[int] = []
    bot_placements = 0
    dlight_frames = 0
    dlight_candidates = 0
    dlight_profile_passes = 0
    world_frames = 0
    particles_enabled = False
    weather_rain = False
    render_observations: list[tuple[int, int, int, int]] = []
    particle_emitters = 0
    particle_emitted = 0
    particle_requests = 0
    particle_dropped_requests = 0
    particle_dropped_particles = 0
    particle_computes = 0
    particle_draws = 0
    atmosphere_particles = 0
    atmosphere_computes = 0
    atmosphere_draws = 0
    atmosphere_froxels = 0
    atmosphere_dispatches = 0
    atmosphere_clouds = 0
    atmosphere_heightgrid = 0
    atmosphere_depth = 0
    atmosphere_temporal_reuse = 0
    atmosphere_temporal_reject = 0
    atmosphere_surface_milli = (0, 0, 0, 0)
    epoch_rows: list[str] = []
    epoch_end_seen = False

    with path.open(encoding="utf-8", errors="replace") as stream:
        for line_number, raw_line in enumerate(stream, 1):
            line = raw_line.strip()
            if not line:
                continue
            try:
                record = json.loads(line)
            except (ValueError, TypeError):
                if measurement_state == "active":
                    malformed_active += 1
                continue
            if not isinstance(record, dict) or not all(
                isinstance(record.get(key), str) for key in ("ts", "sev", "cat", "msg")
            ):
                if measurement_state == "active":
                    malformed_active += 1
                continue
            msg = record["msg"].rstrip("\r\n")
            category = record["cat"]
            # Disable flushes the final partial CPU bucket after MEASURE_END.
            # Collect only explicit frame evidence, never extend the GPU window
            # or silently count post-roll rendering as steady gameplay.
            if measurement_state != "before" and category == "system" and msg.startswith(
                ("frame intervals ", "frame trace (", "frame trace end ")
            ):
                epoch_rows.append(msg)
                epoch_end_seen |= msg.startswith("frame trace end ")
            setting = RE_FPS_SETTING.match(msg) if category == "system" else None
            if setting:
                try:
                    value = float(setting.group(2))
                except ValueError:
                    value = math.nan  # rejected below, without crashing the analyzer
                fps_settings[setting.group(1)].append((measurement_state, value))

            gameplay = RE_GAMEPLAY_MAP.search(msg)
            if gameplay and category == "client":
                gameplay_maps.append(gameplay.group(1))
            if category == "server" and msg.startswith('broadcast: print "') and "entered the game" in msg:
                entrant_broadcasts += 1
            if category == "renderer.init":
                render_match = RE_RENDER_EXTENT.match(msg)
                if render_match:
                    render_observations.append(tuple(int(value) for value in render_match.groups()))
            if (
                "Can't find map" in msg
                or "waitForMap: TIMEOUT" in msg
                or 'Unknown command "addbot"' in msg
                or "Cheats are not enabled" in msg
                or "usage: setviewpos" in msg
                or "bot_teleport: no active bot" in msg
                or "has no live entity" in msg
            ):
                lifecycle_errors.append(msg.strip())
            if msg == "noclip ON":
                noclip_on = True
            elif msg == "noclip OFF":
                noclip_on = False

            if category == "system" and msg == "FPS_GATE_CAMERA_CHECK":
                camera_armed = True
                continue
            if camera_armed and category == "cgame":
                camera_match = RE_CAMERA.match(msg)
                if camera_match:
                    camera_observations.append(tuple(int(v) for v in camera_match.groups()))

            if category == "system" and msg == "FPS_GATE_BOT_VIS_PRE_BEGIN":
                camera_armed = False
                bot_vis_state = "pre"
                continue
            if category == "system" and msg == "FPS_GATE_BOT_VIS_PRE_END":
                bot_vis_state = "none"
                continue
            if category == "system" and msg == "FPS_GATE_BOT_VIS_POST_BEGIN":
                bot_vis_state = "post"
                continue
            if category == "system" and msg == "FPS_GATE_BOT_VIS_POST_END":
                bot_vis_state = "none"
                continue
            if bot_vis_state != "none" and category == "renderer.cmd":
                md3 = RE_MD3.fullmatch(msg)
                if md3:
                    visible = int(md3.group(1)) + int(md3.group(2))
                    (bot_vis_pre if bot_vis_state == "pre" else bot_vis_post).append(visible)
            if category == "game" and RE_BOT_PLACED.fullmatch(msg):
                bot_placements += 1
            particle_runtime = RE_PARTICLE_RUNTIME.fullmatch(msg) if category == "renderer.cmd" else None
            if particle_runtime:
                particle_emitters = max(particle_emitters, int(particle_runtime.group(1)))
                particle_emitted = max(particle_emitted, int(particle_runtime.group(2)))
                particle_requests = max(particle_requests, int(particle_runtime.group(3)))
                particle_dropped_requests = max(particle_dropped_requests, int(particle_runtime.group(4)))
                particle_dropped_particles = max(particle_dropped_particles, int(particle_runtime.group(5)))
                particle_computes = max(particle_computes, int(particle_runtime.group(6)))
                particle_draws = max(particle_draws, int(particle_runtime.group(7)))
            atmosphere_runtime = RE_ATMOSPHERE_RUNTIME.fullmatch(msg) if category == "renderer.cmd" else None
            if atmosphere_runtime:
                values = tuple(int(atmosphere_runtime.group(i)) for i in range(1, 15))
                atmosphere_particles = max(atmosphere_particles, values[0])
                atmosphere_computes = max(atmosphere_computes, values[1])
                atmosphere_draws = max(atmosphere_draws, values[2])
                atmosphere_froxels = max(atmosphere_froxels, values[3])
                atmosphere_dispatches = max(atmosphere_dispatches, values[4])
                atmosphere_clouds = max(atmosphere_clouds, values[5])
                atmosphere_heightgrid = max(atmosphere_heightgrid, values[6])
                atmosphere_depth = max(atmosphere_depth, values[7])
                atmosphere_temporal_reuse = max(atmosphere_temporal_reuse, values[8])
                atmosphere_temporal_reject = max(atmosphere_temporal_reject, values[9])
                atmosphere_surface_milli = tuple(
                    max(atmosphere_surface_milli[i], values[10 + i]) for i in range(4)
                )

            if category == "system" and msg == "FPS_GATE_SCENE_BEGIN":
                scene_begin_markers += 1
                if scene_state != "before":
                    marker_errors.append(f"scene begin out of order at line {line_number}")
                scene_state = "active"
                continue
            if category == "system" and msg == "FPS_GATE_SCENE_END":
                scene_end_markers += 1
                if scene_state != "active":
                    marker_errors.append(f"scene end out of order at line {line_number}")
                scene_state = "after"
                continue
            if scene_state == "active":
                world = RE_WORLD.fullmatch(msg) if category == "renderer.cmd" else None
                if world and int(world.group(2)) > 0 and int(world.group(4)) > 0 and int(world.group(5)) > 0:
                    world_frames += 1
                dlight = RE_DLIGHT.search(msg)
                if category == "renderer.cmd" and dlight and int(dlight.group(3)) > 0 and int(dlight.group(4)) > 0:
                    dlight_frames += 1
                if category == "system" and RE_PARTICLES.search(msg):
                    particles_enabled = True
                if category == "system" and RE_WEATHER.search(msg):
                    weather_rain = True

            candidate = RE_DLIGHT_CANDIDATES.fullmatch(msg) if category == "renderer.timing" else None
            if candidate:
                dlight_candidates = max(dlight_candidates, int(candidate.group(1)))
            profile = RE_DLIGHT_PROFILE.fullmatch(msg) if category == "renderer.timing" else None
            if profile and int(profile.group(1)) > 0:
                dlight_profile_passes = max(dlight_profile_passes, int(profile.group(2)))

            if category == "system" and msg == "FPS_GATE_MEASURE_BEGIN":
                begin_markers += 1
                if measurement_state != "before":
                    marker_errors.append(f"measurement begin out of order at line {line_number}")
                measurement_state = "active"
                begin_time = record.get("ts")
                cur_gpu = None
                continue
            if category == "system" and msg == "FPS_GATE_MEASURE_END":
                end_markers += 1
                if measurement_state != "active":
                    marker_errors.append(f"measurement end out of order at line {line_number}")
                if cur_gpu is not None:
                    partial_gpu += 1
                if pending_frame is not None:
                    pair_errors += 1
                    pending_frame = None
                if pending_vk_pacing:
                    vk_pacing_pair_errors += 1
                    pending_vk_pacing = False
                cur_gpu = None
                measurement_state = "after"
                end_time = record.get("ts")
                continue
            if measurement_state != "active":
                continue

            if category == "system" and msg.startswith("frame intervals"):
                interval = INTERVALS.fullmatch(msg)
                if interval is None:
                    interval_errors.append("malformed full-frame interval payload")
                else:
                    values = list(map(int, interval.groups()[:6]))
                    bucket = values[0]
                    if bucket in intervals:
                        interval_errors.append("duplicate full-frame interval bucket")
                    intervals[bucket] = values[1:] + list(map(int, interval.group(7).split(",")))

            match = RE_ALL.fullmatch(msg) if category == "system" else None
            if match:
                if pending_frame is not None:
                    pair_errors += 1
                pending_frame = (int(match.group(1)), int(match.group(2)))
            micro = RE_END.search(msg) if category == "system" else None
            if micro and msg.lstrip().startswith("cl:"):
                if pending_frame is None:
                    pair_errors += 1
                else:
                    frame_numbers.append(pending_frame[0])
                    alls.append(pending_frame[1])
                    ends.append(int(micro.group(1)))
                    pending_frame = None
            vkt = RE_VKT.fullmatch(msg) if category == "renderer.timing" else None
            if vkt:
                if pending_vk_pacing:
                    vk_pacing_pair_errors += 1
                raw_vkt.append(vkt.groups())
                pending_vk_pacing = True
            pacing = RE_VK_PACING.fullmatch(msg) if category == "renderer.timing" else None
            if pacing:
                if not pending_vk_pacing:
                    vk_pacing_pair_errors += 1
                raw_vk_pacing.append(pacing.groups())
                pending_vk_pacing = False
            vk_v2_match = RE_VK_PERF_V2.fullmatch(msg) if category == "renderer.timing" else None
            if vk_v2_match:
                vk_v2 = parse_vk_perf_v2(vk_v2_match.group(1))
                if vk_v2 is None:
                    vk_v2_parse_errors += 1
                else:
                    raw_vk_v2.append(vk_v2)
            frame_pacing = RE_FRAME_PACING.fullmatch(msg) if category == "system" else None
            if frame_pacing:
                raw_frame_pacing.append(frame_pacing.groups())
            spike = RE_GPU_SPIKE.fullmatch(msg) if category == "renderer.timing" else None
            if spike:
                raw_gpu_spikes.append({
                    "frames": int(spike.group(1)), "over4ms": int(spike.group(2)),
                    "max_frame_ms": float(spike.group(3)),
                    "dominant": {} if spike.group(4) == "-" else {
                        k: int(v) for k, v in (item.split(":") for item in spike.group(4).split(","))},
                    "lane_max_ms": {} if spike.group(5) == "-" else {
                        k: float(v) for k, v in (item.split(":") for item in spike.group(5).split(","))},
                })
                continue
            header = RE_GPU_HDR.fullmatch(msg) if category == "renderer.timing" else None
            if header:
                if cur_gpu is not None:
                    partial_gpu += 1
                gpu_headers += 1
                cur_gpu = {"frames": float(header.group(1))}
                continue
            if cur_gpu is not None and category == "renderer.timing":
                total = RE_GPU_TOTAL.match(msg)
                if total:
                    cur_gpu["total"] = float(total.group(1))
                    raw_gpu.append(cur_gpu)
                    cur_gpu = None
                    continue
                label = RE_GPU_LABEL.match(msg)
                if label:
                    cur_gpu[label.group(1)] = float(label.group(2))
                else:
                    partial_gpu += 1
                    cur_gpu = None

    if measurement_state == "active" and cur_gpu is not None:
        partial_gpu += 1

    # GPU timestamps have their own pre-roll cadence, so only their first block
    # can straddle the marker and is discarded. V2 starts a fresh measurement
    # epoch at the marker boundary and uses all three contained 200-attempt blocks.
    gpu_blocks = raw_gpu[1:]
    vk_v2_blocks = raw_vk_v2
    steady_all = alls[50:]
    p95_all = percentile(steady_all, 0.95)
    p99_all = percentile(steady_all, 0.99)
    frame_counts = [int(values[2]) for values in raw_frame_pacing]
    frame_valid = [int(values[3]) for values in raw_frame_pacing]
    frame_coverage = sum(frame_counts)
    frame_work_total = sum(int(values[4]) for values in raw_frame_pacing)
    frame_scr_total = sum(int(values[5]) for values in raw_frame_pacing)
    frame_work_mean = frame_work_total / frame_coverage if frame_coverage else None
    scr_end_mean = frame_scr_total / frame_coverage if frame_coverage else None
    mean_all = frame_work_mean / 1000.0 if finite(frame_work_mean) else None
    fps_mean = (
        1_000_000.0 / frame_work_mean
        if finite(frame_work_mean) and frame_work_mean > 0
        else None
    )
    scr_end_implied_fps = (
        1_000_000.0 / scr_end_mean if finite(scr_end_mean) and scr_end_mean > 0 else None
    )

    passes = list(dict.fromkeys(key for block in gpu_blocks for key in block if key != "frames"))
    med_passes: dict[str, float] = {}
    for key in passes:
        values = [block[key] for block in gpu_blocks if key in block and finite(block[key])]
        if values:
            med_passes[key] = statistics.median(values)
    dominant = max(
        ((key, value) for key, value in med_passes.items() if key != "total"),
        key=lambda pair: pair[1],
        default=(None, None),
    )
    gpu_totals = [block.get("total") for block in gpu_blocks]
    gpu_totals = [value for value in gpu_totals if finite(value) and value > 0]
    gpu_total_median = statistics.median(gpu_totals) if gpu_totals else None
    gpu_implied_fps = (
        1000.0 / gpu_total_median
        if finite(gpu_total_median) and gpu_total_median > 0
        else None
    )

    vk_attempt_coverage = sum(block["count"] for block in vk_v2_blocks)
    vk_worker_coverage = sum(block["worker_count"] for block in vk_v2_blocks)

    def vk_attempt_mean(key: str) -> float | None:
        return (
            sum(block[key] for block in vk_v2_blocks) / vk_attempt_coverage
            if vk_attempt_coverage else None
        )

    def vk_worker_mean(key: str) -> float | None:
        return (
            sum(block[key] for block in vk_v2_blocks) / vk_worker_coverage
            if vk_worker_coverage else None
        )

    vk_acquire_mean_us = vk_attempt_mean("acquire_total_us")
    vk_submit_mean_us = vk_attempt_mean("submit_total_us")
    vk_present_call_mean_us = vk_attempt_mean("present_call_total_us")
    vk_main_slot_wait_mean_us = vk_attempt_mean("main_slot_wait_total_us")
    vk_worker_queue_mean_us = vk_worker_mean("worker_queue_total_us")
    vk_worker_wait_mean_us = vk_worker_mean("worker_wait_total_us")
    vk_worker_reset_mean_us = vk_worker_mean("worker_reset_total_us")
    draws_mean = vk_attempt_mean("draws_total")
    draws_median = int(draws_mean) if finite(draws_mean) else None
    bots_entered = max(0, entrant_broadcasts - 1)
    wall_seconds = wall_fps = None
    timestamp_error = None
    try:
        if not isinstance(begin_time, str) or not isinstance(end_time, str):
            raise ValueError("missing timestamp")
        wall_seconds = (dt.datetime.fromisoformat(end_time) - dt.datetime.fromisoformat(begin_time)).total_seconds()
        if not finite(wall_seconds) or wall_seconds <= 0:
            raise ValueError("non-positive window")
        wall_fps = contract.hold_frames / wall_seconds
    except (TypeError, ValueError) as error:
        timestamp_error = str(error)
        wall_seconds = wall_fps = None

    effective_values = (fps_mean, gpu_implied_fps, wall_fps)
    effective_fps = min(effective_values) if all(finite(value) for value in effective_values) else None
    failures: list[str] = []
    for name, expected in FPS_SETTINGS.items():
        observations = fps_settings[name]
        if (not any(phase == "before" for phase, _ in observations)
                or not any(phase == "after" for phase, _ in observations)
                or any(value != expected for _, value in observations)):
            failures.append(f"absent or mismatched runtime setting {name}")
    if begin_markers != 1 or end_markers != 1 or marker_errors:
        failures.append(
            f"measurement markers invalid (begin={begin_markers}, end={end_markers}, order_errors={len(marker_errors)})"
        )
    if scene_begin_markers != 1 or scene_end_markers != 1:
        failures.append(
            f"scene markers invalid (begin={scene_begin_markers}, end={scene_end_markers})"
        )
    if malformed_active:
        failures.append(f"malformed JSON inside measurement window ({malformed_active})")
    if pair_errors or len(frame_numbers) != len(set(frame_numbers)) or any(
        current <= previous for previous, current in zip(frame_numbers, frame_numbers[1:])
    ):
        failures.append(
            f"frame/end pairing invalid (pairs={len(alls)}, pair_errors={pair_errors}, monotonic={len(frame_numbers) == len(set(frame_numbers)) and all(current > previous for previous, current in zip(frame_numbers, frame_numbers[1:]))})"
        )
    if contract.map_name not in gameplay_maps:
        failures.append(
            f"FIRST GAMEPLAY FRAME map mismatch (expected={contract.map_name}, observed={gameplay_maps})"
        )
    camera_observed = camera_observations[-1] if camera_observations else None
    camera_stable = (
        len(camera_observations) >= 3
        and camera_observations[-3:] == [contract.viewpos] * 3
    )
    if not noclip_on:
        failures.append("missing noclip ON evidence")
    if not camera_stable:
        failures.append(
            f"camera mismatch/unstable (expected={contract.viewpos}, observed={camera_observations})"
        )
    if bots_entered < contract.bots:
        failures.append(f"bot population incomplete ({bots_entered} entered < {contract.bots} requested)")
    pre_visible = max(bot_vis_pre, default=-1)
    post_visible = max(bot_vis_post, default=-1)
    if bot_placements < 1 or pre_visible < 0 or post_visible <= pre_visible:
        failures.append(
            f"active/visible bot evidence missing (placements={bot_placements}, md3_pre={pre_visible}, md3_post={post_visible})"
        )
    if dlight_frames < 1 and dlight_profile_passes < 6:
        failures.append("missing dynamic-light render-work evidence")
    if dlight_candidates < 1:
        failures.append("missing dynamic-light candidate evidence")
    if not particles_enabled:
        failures.append("GPU particle pass is not proven enabled")
    particle_ratio = (
        contract.particle_changed / contract.particle_pixels
        if contract.particle_pixels > 0 else 0.0
    )
    particle_control_ratio = (
        contract.particle_control_changed / contract.particle_pixels
        if contract.particle_pixels > 0 else 1.0
    )
    particle_causal = (
        contract.particle_pixels > 0
        and particle_ratio >= 0.01
        and particle_ratio >= particle_control_ratio + 0.01
        and contract.particle_mad >= contract.particle_control_mad + 0.1
        and contract.particle_mad >= contract.particle_control_mad * 1.25
    )
    particle_workload = (
        particle_emitters >= 1 and particle_emitted >= 1
    ) or atmosphere_particles >= 1 or particle_causal
    if not particle_workload or max(particle_computes, atmosphere_computes) < 1 \
            or max(particle_draws, atmosphere_draws) < 1:
        failures.append(
            "particle runtime contract missing "
            f"(emitters={particle_emitters}, particles={particle_emitted}, "
            f"computes={particle_computes}, draws={particle_draws}, "
            f"weather-particles={atmosphere_particles}, "
            f"weather-computes={atmosphere_computes}, weather-draws={atmosphere_draws})"
        )
    expected_render = (contract.render_width, contract.render_height)
    render_observed = render_observations[-1] if render_observations else None
    if render_observed is None or render_observed[:2] != expected_render:
        failures.append(
            f"render extent mismatch (expected={expected_render}, observed={render_observations})"
        )
    elif contract.particle_pixels != render_observed[0] * render_observed[1]:
        failures.append(
            "capture extent does not match render resolution "
            f"(pixels={contract.particle_pixels}, render={render_observed[0]}x{render_observed[1]})"
        )
    if not weather_rain:
        failures.append("representative rain effect is not configured")
    if world_frames < 1:
        failures.append("missing representative world-geometry evidence")
    if lifecycle_errors:
        failures.append("lifecycle errors: " + " | ".join(lifecycle_errors[:3]))
    # Thresholded com_speeds rows remain useful tail samples, but CPU authority
    # comes from the complete 200-frame aggregate above.  Requiring hundreds of
    # threshold hits biases fast runs toward their slowest frames.
    frame_pacing_sizes_ok = all(
        int(values[0]) == 200 and count == 200 and valid == 200
        for values, count, valid in zip(raw_frame_pacing, frame_counts, frame_valid)
    )
    frame_pacing_buckets = [int(values[1]) for values in raw_frame_pacing]
    frame_pacing_order_ok = len(frame_pacing_buckets) == len(set(frame_pacing_buckets)) and all(
        ((current - previous) & 0xFFFFFFFF) == 1
        for previous, current in zip(frame_pacing_buckets, frame_pacing_buckets[1:])
    )
    expected_frame_blocks = contract.hold_frames // 200
    expected_frame_coverage = expected_frame_blocks * 200
    interval_summary = None
    interval_coverage = expected_frame_coverage
    try:
        interval_summary = full_frame_intervals(
            intervals, frame_pacing_buckets, expected_frame_coverage)
        if epoch_end_seen:
            interval_summary = completed_frame_intervals(epoch_rows)
            interval_coverage = int(interval_summary["frames"])
            if interval_coverage != contract.hold_frames:
                raise EvidenceError("completed frame epoch differs from requested frame count")
    except EvidenceError as exc:
        interval_errors.append(str(exc))
    failures.extend(interval_errors)
    # Keep low-target historical diagnostics available, but never let their
    # averaged throughput stand in for the ratified minimum-250 requirement.
    if contract.target_fps >= 250:
        if interval_summary and interval_summary["gt4ms_count"]:
            failures.append(f"strict 4ms budget exceeded ({interval_summary['gt4ms_count']} intervals)")
        if interval_coverage != contract.hold_frames:
            failures.append("strict full-frame coverage incomplete: unmeasured tail frames")
    if (
        len(raw_frame_pacing) != expected_frame_blocks
        or frame_coverage != expected_frame_coverage
        or not frame_pacing_sizes_ok
        or not frame_pacing_order_ok
    ):
        failures.append(
            "frame pacing blocks invalid "
            f"(raw={len(raw_frame_pacing)}/{expected_frame_blocks}, "
            f"coverage={frame_coverage}/{expected_frame_coverage}, "
            f"sized={frame_pacing_sizes_ok}, ordered={frame_pacing_order_ok})"
        )
    gpu_header_sizes_ok = all(block.get("frames") == 200.0 for block in raw_gpu)
    if gpu_headers < 3 or len(raw_gpu) < 3 or len(gpu_blocks) < 2 or partial_gpu or not gpu_header_sizes_ok:
        failures.append(
            f"GPU timing blocks invalid (headers={gpu_headers}, complete={len(raw_gpu)}, clean={len(gpu_blocks)}, partial={partial_gpu})"
        )
    if any(not any(label not in {"frames", "total"} for label in block) for block in gpu_blocks):
        failures.append("GPU timing blocks contain no non-total pass")
    vk_v2_buckets = [block["bucket"] for block in raw_vk_v2]
    vk_v2_epochs = [block["epoch"] for block in raw_vk_v2]
    vk_v2_order_ok = (
        vk_v2_buckets == list(range(1, expected_frame_blocks + 1))
        and bool(vk_v2_epochs)
        and vk_v2_epochs[0] > 0
        and all(epoch == vk_v2_epochs[0] for epoch in vk_v2_epochs)
    )
    vk_v2_sizes_ok = all(
        block["count"] == 200 and block["valid"] == 200
        for block in raw_vk_v2
    )
    identity_keys = (
        "generation", "mode", "r_swapInterval", "policy_intent", "requested_images",
        "actual_images", "frames_in_flight", "vrr_preferred", "slots", "extent",
        "platform", "worker_mode",
    )
    vk_v2_identity_ok = bool(raw_vk_v2) and all(
        block["identity_stable"] == 1
        and all(block[key] == raw_vk_v2[0][key] for key in identity_keys)
        for block in raw_vk_v2
    )
    expected_vk_platform = {
        "Windows": "windows", "Darwin": "macos", "Linux": "linux",
    }.get(platform.system(), "other")
    vk_v2_contract_ok = all(
        block["slots"] == 2
        and render_observed is not None
        and block["extent"] == render_observed[2:4]
        and block["platform"] == expected_vk_platform
        and block["r_swapInterval"] == 0
        and block["policy_intent"] == 0
        and block["frames_in_flight"] == block["slots"]
        and block["vrr_preferred"] == 0
        and block["mode"] in {
            "IMMEDIATE", "MAILBOX", "FIFO", "FIFO_RELAXED", "FIFO_LATEST_READY",
        }
        and block["generation"] > 0
        and block["requested_images"] >= 2
        and block["actual_images"] >= block["requested_images"]
        and block["actual_images"] <= 8
        and all(block[key] >= 0 for key in VK_PERF_V2_INTEGER_KEYS)
        and set(block["image_hits"]) == set(range(block["actual_images"]))
        and sum(block["image_hits"].values()) == block["valid"]
        and block["acquire_total_us"] > 0
        and block["submit_total_us"] > 0
        and block["present_call_total_us"] > 0
        and block["main_slot_wait_total_us"] > 0
        for block in raw_vk_v2
    )
    if (
        vk_v2_parse_errors
        or len(raw_vk_v2) != expected_frame_blocks
        or vk_attempt_coverage != expected_frame_coverage
        or sum(block["valid"] for block in raw_vk_v2) != expected_frame_coverage
        or vk_attempt_coverage != frame_coverage
        or not vk_v2_sizes_ok
        or not vk_v2_order_ok
        or not vk_v2_identity_ok
        or not vk_v2_contract_ok
    ):
        failures.append(
            "vk perf v2 blocks invalid "
            f"(raw={len(raw_vk_v2)}/{expected_frame_blocks}, coverage={vk_attempt_coverage}/{expected_frame_coverage}, "
            f"frame_coverage={frame_coverage}, parse={vk_v2_parse_errors}, "
            f"sized={vk_v2_sizes_ok}, ordered={vk_v2_order_ok}, "
            f"identity={vk_v2_identity_ok}, contract={vk_v2_contract_ok})"
        )
    elif expected_vk_platform == "windows":
        if any(
            block[key] != 0
            for block in raw_vk_v2
            for key in ("worker_queue_total_us", "worker_wait_total_us", "worker_reset_total_us", "worker_count", "maxq")
        ) or any(block["worker_mode"] != "sync" for block in raw_vk_v2):
            failures.append("vk perf v2 Windows worker attribution invalid")
    elif any(
        block["worker_mode"] != "pthread"
        or block["worker_count"] < 200 - block["slots"]
        or block["worker_count"] > 200 + block["slots"]
        or block["maxq"] < 1
        or block["maxq"] > block["slots"]
        or block["worker_wait_total_us"] + block["worker_reset_total_us"] <= 0
        for block in raw_vk_v2
    ):
        failures.append("vk perf v2 worker attribution invalid")
    if draws_median is None or draws_median < contract.draw_floor:
        failures.append(
            f"representative draw floor not met ({draws_median} < {contract.draw_floor})"
        )
    if mean_all is None or mean_all <= 0 or fps_mean is None:
        failures.append("invalid total frame-time mean")
    if timestamp_error or wall_fps is None:
        failures.append(f"invalid wall-clock measurement window ({timestamp_error})")
    if gpu_implied_fps is None:
        failures.append("invalid GPU total")
    if effective_fps is None:
        failures.append("effective FPS could not be derived")
    elif effective_fps < contract.target_fps:
        failures.append(
            f"effective FPS target not met ({effective_fps:.1f} < {contract.target_fps:.0f})"
        )

    summary = {
        'gpu_spikes': raw_gpu_spikes,  # per-200-frame GPU single-frame spike attribution (gpuSpike v1 rows)
        "tag": contract.tag,
        "map": contract.map_name,
        "camera": list(camera_observed) if camera_observed else None,
        "bots": contract.bots,
        "platform": platform.system(),
        "machine": platform.machine(),
        "resolution": f"{contract.render_width}x{contract.render_height}",
        "render_width": contract.render_width,
        "render_height": contract.render_height,
        "render_extent_observations": [list(value) for value in render_observations],
        "capture_resolution": (
            f"{render_observed[0]}x{render_observed[1]}" if render_observed else None
        ),
        "output_resolution": (
            f"{render_observed[2]}x{render_observed[3]}" if render_observed else None
        ),
        "target_fps": contract.target_fps,
        "fps_settings": FPS_SETTINGS.copy(),
        "frame_intervals": interval_summary,
        "unmeasured_tail_frames": contract.hold_frames - interval_coverage,
        "frame_epoch_ended": epoch_end_seen,
        "acceptance_scope": "pinned-scene diagnostic; not TASK-86 long movement/combat corpus acceptance",
        "status": "FAIL" if failures else "PASS",
        "failure_reasons": failures,
        "marker_errors": marker_errors,
        "malformed_active_json": malformed_active,
        "gameplay_maps": gameplay_maps,
        "bots_entered": bots_entered,
        "noclip_on": noclip_on,
        "camera_observations": [list(value) for value in camera_observations],
        "bot_placements": bot_placements,
        "bot_md3_pre": pre_visible,
        "bot_md3_post": post_visible,
        "dlight_frames": dlight_frames,
        "dlight_candidates": dlight_candidates,
        "dlight_profile_passes": dlight_profile_passes,
        "world_frames": world_frames,
        "particles_enabled": particles_enabled,
        "particle_changed_pixels": contract.particle_changed,
        "particle_changed_ratio": round(particle_ratio, 8),
        "particle_mad": contract.particle_mad,
        "particle_total_pixels": contract.particle_pixels,
        "particle_control_changed_pixels": contract.particle_control_changed,
        "particle_control_changed_ratio": round(particle_control_ratio, 8),
        "particle_control_mad": contract.particle_control_mad,
        "particle_visual_above_noise": particle_causal,
        "particle_emitters": particle_emitters,
        "particle_emitted": particle_emitted,
        "particle_requests": particle_requests,
        "particle_dropped_requests": particle_dropped_requests,
        "particle_dropped_particles": particle_dropped_particles,
        "particle_computes": particle_computes,
        "particle_draws": particle_draws,
        "atmosphere_particles": atmosphere_particles,
        "atmosphere_computes": atmosphere_computes,
        "atmosphere_draws": atmosphere_draws,
        "atmosphere_froxels": atmosphere_froxels,
        "atmosphere_dispatches": atmosphere_dispatches,
        "atmosphere_clouds": atmosphere_clouds,
        "atmosphere_heightgrid": atmosphere_heightgrid,
        "atmosphere_depth": atmosphere_depth,
        "atmosphere_temporal_reuse": atmosphere_temporal_reuse,
        "atmosphere_temporal_reject": atmosphere_temporal_reject,
        "atmosphere_surface_milli": list(atmosphere_surface_milli),
        "weather_rain": weather_rain,
        "frames": len(alls),
        "frame_pacing_blocks": len(raw_frame_pacing),
        "frame_pacing_bucket_first": frame_pacing_buckets[0] if frame_pacing_buckets else None,
        "frame_pacing_bucket_last": frame_pacing_buckets[-1] if frame_pacing_buckets else None,
        "cpu_work_mean_us": round(frame_work_mean, 1) if finite(frame_work_mean) else None,
        "frame_pacing_coverage": frame_coverage,
        "frames_requested": contract.hold_frames,
        "frames_not_profiled": max(0, contract.hold_frames - len(alls)),
        "steady_frames": len(steady_all),
        "all_mean_ms": round(mean_all, 3) if finite(mean_all) else None,
        "all_p95_ms": p95_all,
        "all_p99_ms": p99_all,
        "fps_mean": round(fps_mean, 1) if finite(fps_mean) else None,
        "scr_end_mean_us": scr_end_mean,
        "scr_end_implied_fps": round(scr_end_implied_fps, 1) if finite(scr_end_implied_fps) else None,
        "wall_seconds": round(wall_seconds, 6) if finite(wall_seconds) else None,
        "wall_fps": round(wall_fps, 1) if finite(wall_fps) else None,
        "sample_pair_errors": pair_errors,
        "gpu_blocks_raw": len(raw_gpu),
        "gpu_blocks_clean": len(gpu_blocks),
        "gpu_total_ms": gpu_total_median,
        "gpu_implied_fps": round(gpu_implied_fps, 1) if finite(gpu_implied_fps) else None,
        "gpu_dominant_pass": dominant[0],
        "gpu_dominant_ms": dominant[1],
        "vk_perf_v2_blocks_raw": len(raw_vk_v2),
        "vk_perf_v2_blocks_clean": len(vk_v2_blocks),
        "vk_perf_v2_parse_errors": vk_v2_parse_errors,
        "vk_perf_v2_bucket_first": vk_v2_buckets[0] if vk_v2_buckets else None,
        "vk_perf_v2_bucket_last": vk_v2_buckets[-1] if vk_v2_buckets else None,
        "vk_perf_v2_attempt_coverage": vk_attempt_coverage,
        "vk_acquire_mean_us": vk_acquire_mean_us,
        "vk_submit_mean_us": vk_submit_mean_us,
        "vk_present_call_mean_us": vk_present_call_mean_us,
        "vk_main_slot_wait_mean_us": vk_main_slot_wait_mean_us,
        "vk_worker_queue_mean_us": vk_worker_queue_mean_us,
        "vk_worker_wait_mean_us": vk_worker_wait_mean_us,
        "vk_worker_reset_mean_us": vk_worker_reset_mean_us,
        "vk_worker_count": vk_worker_coverage,
        "vk_max_queue": max((block["maxq"] for block in vk_v2_blocks), default=None),
        "vk_swapchain_identity": {
            key: (list(raw_vk_v2[0][key]) if key == "extent" else raw_vk_v2[0][key])
            for key in identity_keys
        } if raw_vk_v2 else None,
        "effective_fps": round(effective_fps, 1) if finite(effective_fps) else None,
        "draws_median": draws_median,
        "draw_floor": contract.draw_floor,
        "engine_sha256": contract.engine_sha256,
        "pax21_sha256": contract.pax21_sha256,
    }

    if emit:
        print(f"\n=== FPS-PERF-GATE RESULT [{contract.tag}] ===")
        print(f"  scene contract          : {contract.render_width}x{contract.render_height} map={gameplay_maps} camera={camera_observed} bots={bots_entered}")
        print(f"  bot visibility          : placements={bot_placements} md3={pre_visible}->{post_visible}")
        print(f"  dlight/weather/particles: surfaces={dlight_frames} shadow_passes={dlight_profile_passes} candidates={dlight_candidates} rain={weather_rain} particles={particles_enabled} runtime={particle_emitters}/{particle_emitted}/{particle_computes}/{particle_draws} atmosphere={atmosphere_particles}/{atmosphere_computes}/{atmosphere_draws}")
        print(f"  threshold frame samples : {len(alls)} (p95={p95_all}ms p99={p99_all}ms)")
        print(
            f"  full-frame CPU aggregate: blocks={len(raw_frame_pacing)} "
            f"coverage={frame_coverage} work={frame_work_mean:.0f}us/f scr={scr_end_mean:.0f}us/f"
            if finite(frame_work_mean) and finite(scr_end_mean)
            else "  full-frame CPU aggregate: unavailable"
        )
        print(f"  CPU FPS                 : {fps_mean:.1f}" if finite(fps_mean) else "  CPU FPS                 : unavailable")
        print(f"  GPU blocks              : raw={len(raw_gpu)} clean={len(gpu_blocks)}")
        print(f"  GPU total/FPS           : {gpu_total_median:.3f}ms / {gpu_implied_fps:.1f}" if finite(gpu_implied_fps) else "  GPU total/FPS           : unavailable")
        print(f"  vk perf v2/draws        : raw={len(raw_vk_v2)} clean={len(vk_v2_blocks)} mean={draws_mean}")
        print(
            "  vk attempt means (us)   : "
            f"acquire={vk_acquire_mean_us} submit={vk_submit_mean_us} "
            f"present_call={vk_present_call_mean_us} slot={vk_main_slot_wait_mean_us}"
        )
        print(
            "  vk worker means (us)    : "
            f"queue={vk_worker_queue_mean_us} wait={vk_worker_wait_mean_us} "
            f"reset={vk_worker_reset_mean_us} work={vk_worker_coverage} "
            f"maxq={max((block['maxq'] for block in vk_v2_blocks), default=None)}"
        )
        print(f"  wall FPS                : {wall_fps:.1f}" if finite(wall_fps) else "  wall FPS                : unavailable")
        if interval_summary:
            print(f"  frame intervals         : count={interval_summary['count']} >4ms={interval_summary['gt4ms_count']} max={interval_summary['max_ns']}ns P99=[{interval_summary['p99_lower_ns']},{interval_summary['p99_upper_ns']}]ns tail-unmeasured={contract.hold_frames - interval_coverage}")
        print(f"  EFFECTIVE FPS           : {effective_fps:.1f}" if finite(effective_fps) else "  EFFECTIVE FPS           : unavailable")
        print(f"  verdict                 : {summary['status']}")
        print(f"=== END RESULT [{contract.tag}] ===\n")
    return summary


def record(ts: str, msg: str, cat: str = "system") -> str:
    return json.dumps({"ts": ts, "sev": "INFO", "cat": cat, "msg": msg})


def synthetic_fixture(samples: int = 650, *, frame_ns: int = 10000000) -> list[str]:
    base = dt.datetime.fromisoformat("2026-08-11T12:00:00+03:00")
    fixture_platform = {"Windows": "windows", "Darwin": "macos", "Linux": "linux"}.get(platform.system(), "other")
    fixture_worker_mode = "sync" if fixture_platform == "windows" else "pthread"
    lines = [
        record(base.isoformat(), "cls.state: -> CA_ACTIVE (FIRST GAMEPLAY FRAME mapname=maps/arena1.bsp serverTime=400 numEntities=17 framecount=8)", "client"),
        record(base.isoformat(), 'broadcast: print "local has entered the game\\n"', "server"),
    ]
    lines.extend(record(base.isoformat(), 'broadcast: print "Visor has entered the game\\n"', "server") for _ in range(2))
    lines.extend(
        [
            record(base.isoformat(), "RENDER: 1280 x 720, MODE: -1, 2560 x 1440 windowed hz:N/A", "renderer.init"),
            record(base.isoformat(), "FPS_GATE_CAMERA_CHECK"),
            record(base.isoformat(), "1052 1432 117 135 0", "cgame"),
            record(base.isoformat(), "1052 1432 117 135 0", "cgame"),
            record(base.isoformat(), "1052 1432 117 135 0", "cgame"),
            record(base.isoformat(), "noclip ON", "cgame"),
            record(base.isoformat(), "FPS_GATE_BOT_VIS_PRE_BEGIN"),
            record(base.isoformat(), "(md3) 2 sin 1 sclip  4 sout 0 bin 0 bclip 0 bout", "renderer.cmd"),
            record(base.isoformat(), "FPS_GATE_BOT_VIS_PRE_END"),
            record(base.isoformat(), "bot_teleport: Visor placed at (989 1575 69) yaw=295", "game"),
            record(base.isoformat(), "particle emitters:1 particles:18 requests:1 dropped:0/0 computes:1 draws:2", "renderer.cmd"),
            record(base.isoformat(), "FPS_GATE_BOT_VIS_POST_BEGIN"),
            record(base.isoformat(), "(md3) 3 sin 2 sclip  4 sout 0 bin 0 bclip 0 bout", "renderer.cmd"),
            record(base.isoformat(), "FPS_GATE_BOT_VIS_POST_END"),
            record(base.isoformat(), "FPS_GATE_SCENE_BEGIN"),
            record(base.isoformat(), '"r_particles" is:"1" default:"1"'),
            record(base.isoformat(), '"g_envWeather" is:"rain" default:""'),
            record(base.isoformat(), "3/40 shaders/surfs 20 leafs 4000 verts 2000/2500 tris 8.00 mtex", "renderer.cmd"),
            record(base.isoformat(), "dlight srf:4  culled:1  verts:24  tris:12", "renderer.cmd"),
            record(base.isoformat(), "dlight-shadow-candidates: 1 peak: 1", "renderer.timing"),
            record(base.isoformat(), "dlightShadowProfile: 1 lights x 6 = 6 passes/frame, 2.0 us/frame CPU-submit (126 frames)", "renderer.timing"),
            record(base.isoformat(), "FPS_GATE_SCENE_END"),
        ]
    )
    lines.extend(record(base.isoformat(), f'"{name}" is:"{value}"') for name, value in FPS_SETTINGS.items())
    lines.append(record(base.isoformat(), "FPS_GATE_MEASURE_BEGIN"))
    for index in range(samples):
        ts = (base + dt.timedelta(seconds=(index + 1) * frame_ns / 1e9)).isoformat()
        lines.append(record(ts, f"frame:{index} all:10 sv:0 ev:0 cl:0 gm:0 rf:0 bk:0"))
        lines.append(record(ts, "cl: front:100 end:10000 (us)"))
        if index in (199, 399, 599):
            total = (50.0, 5.0, 6.0)[index // 200]
            draws = (1, 60, 62)[index // 200]
            fence = (99, 0, 0)[index // 200]
            frame_time_fence = (88, 0, 0)[index // 200]
            pacing_bucket = index // 200 + 1
            slot_wait = (999, 5, 7)[index // 200]
            worker_queue = (888, 10, 14)[index // 200]
            worker_fence = (777, 20, 24)[index // 200]
            v2_slot_wait = (3, 5, 7)[index // 200]
            v2_worker_queue = (8, 10, 14)[index // 200]
            v2_worker_wait = (16, 20, 24)[index // 200]
            v2_draws = (60, 60, 62)[index // 200]
            acquire_total = (2000, 4000, 8000)[index // 200]
            submit_total = (1000, 2000, 6000)[index // 200]
            present_total = (3000, 6000, 10000)[index // 200]
            worker_reset_total = (100, 200, 400)[index // 200]
            cpu_work = (9000, 4000, 4200)[index // 200]
            scr_end = (8000, 3000, 3200)[index // 200]
            interval_count = 199 if index == 199 else 200
            bins = [0] * 33
            bins[min(32, frame_ns // 250000)] = interval_count
            lines.append(record(ts,
                f"frame intervals v1: bucket={pacing_bucket} count={interval_count} invalid=0 "
                f"total_ns={frame_ns * interval_count} max_ns={frame_ns} "
                f"gt4ms={interval_count if frame_ns > 4000000 else 0} bins="
                + ",".join(map(str, bins))))
            lines.extend(
                [
                    record(ts, f"frame trace (200f): bucket={pacing_bucket} count=200 valid=200 cpu_work_total={cpu_work * 200}us scr_end_total={scr_end * 200}us"),
                    record(ts, "gpu (200f avg, ms):", "renderer.timing"),
                    record(ts, f"  world_done={total - 1:.2f}", "renderer.timing"),
                    record(ts, f"  total={total:.2f}", "renderer.timing"),
                    record(ts, f"vk timing (200f avg): fence={fence}ms/f  ft_fence={frame_time_fence}ms/f  acquire=0ms/f  submit=0ms/f  present=0ms/f  draws={draws}/f(msdf=0)  pipebinds=2/f(msdf=0)", "renderer.timing"),
                    record(ts, f"vk pacing (200f avg): bucket={pacing_bucket} slots=2 main_slot_wait={slot_wait}us/f worker_queue={worker_queue}us/work worker_fence={worker_fence}us/work worker_count=200 max_queue=1", "renderer.timing"),
                    record(
                        ts,
                        "vk perf v2: "
                        f"epoch=1 bucket={pacing_bucket} count=200 valid=200 "
                        f"acquire_total_us={acquire_total} submit_total_us={submit_total} "
                        f"present_call_total_us={present_total} main_slot_wait_total_us={v2_slot_wait * 200} "
                        f"worker_queue_total_us={0 if fixture_worker_mode == 'sync' else v2_worker_queue * 200} "
                        f"worker_wait_total_us={0 if fixture_worker_mode == 'sync' else v2_worker_wait * 200} "
                        f"worker_reset_total_us={0 if fixture_worker_mode == 'sync' else worker_reset_total} "
                        f"worker_count={0 if fixture_worker_mode == 'sync' else 200} "
                        f"maxq={0 if fixture_worker_mode == 'sync' else 1} generation=1 mode=IMMEDIATE "
                        "r_swapInterval=0 policy_intent=0 requested_images=3 actual_images=3 "
                        "frames_in_flight=2 vrr_preferred=0 slots=2 extent=2560x1440 "
                        f"platform={fixture_platform} worker_mode={fixture_worker_mode} draws_total={v2_draws * 200} "
                        "image_hits=0:67,1:67,2:66 identity_stable=1",
                        "renderer.timing",
                    ),
                ]
            )
    end = base + dt.timedelta(seconds=samples * frame_ns / 1e9)
    lines.append(record(end.isoformat(), "FPS_GATE_MEASURE_END"))
    lines.extend(record(end.isoformat(), f'"{name}" is:"{value}"') for name, value in FPS_SETTINGS.items())
    return lines


def run_self_test() -> int:
    base_contract = Contract(
        "self", "arena1", 2, 90.0, 650, (1052, 1432, 117, 135, 0),
        particle_pixels=1280 * 720,
    )
    cases: list[tuple[str, list[str], Contract, bool, str | None]] = []
    clean = synthetic_fixture()
    cases.append(("clean", clean, base_contract, True, None))
    cases.append(("below-target", clean, replace(base_contract, target_fps=250.0), False, "target not met"))

    def without(pattern: str, source: list[str] = clean) -> list[str]:
        return [line for line in source if pattern not in line]

    cases.append(("missing-runtime-fps", without("com_maxfpsUnfocused"), base_contract,
                  False, "runtime setting com_maxfpsUnfocused"))
    wrong_fps = [line.replace('is:\\"1000', 'is:\\"60') for line in clean]
    cases.append(("capped-unfocused", wrong_fps, base_contract,
                  False, "runtime setting com_maxfpsUnfocused"))
    cases.append(("malformed-runtime-fps", [line.replace('is:\\"1000', 'is:\\"-') for line in clean],
                  base_contract, False, "runtime setting com_maxfpsUnfocused"))
    cases.append(("missing-end-fps", clean[:-3], base_contract,
                  False, "runtime setting com_maxfps"))

    cases.append(("missing-intervals", without("frame intervals"), base_contract, False,
                  "full-frame interval coverage"))
    duplicate_intervals = clean.copy()
    interval_index = next(i for i, line in enumerate(clean) if "frame intervals" in line)
    duplicate_intervals.insert(interval_index, clean[interval_index])
    cases.append(("duplicate-intervals", duplicate_intervals, base_contract, False,
                  "duplicate full-frame interval"))
    cases.append(("malformed-intervals", [line.replace("invalid=0 total_ns", "invalid=no total_ns")
                  for line in clean], base_contract, False, "malformed full-frame interval"))
    cases.append(("wrong-interval-count", [line.replace("count=199 invalid=0", "count=200 invalid=0")
                  for line in clean], base_contract, False, "invalid full-frame interval block"))

    # High average throughput cannot hide a single 5ms hitch. These fixtures
    # have 600 fully covered frames, unlike the historical 650-frame recipe.
    fast = synthetic_fixture(600, frame_ns=3000000)
    fast = [re.sub(r"cpu_work_total=\d+us scr_end_total=\d+us",
                   "cpu_work_total=600000us scr_end_total=400000us", line)
            .replace("world_done=4.00", "world_done=0.50")
            .replace("world_done=5.00", "world_done=0.50")
            .replace("total=5.00", "total=1.00")
            .replace("total=6.00", "total=1.00") for line in fast]
    strict_contract = replace(base_contract, target_fps=250, hold_frames=600)
    cases.append(("strict-fast-complete", fast, strict_contract, True, None))
    hitch = fast.copy()
    at = next(i for i, line in enumerate(hitch) if "frame intervals" in line)
    hitch_record = json.loads(hitch[at])
    bins = [0] * 33; bins[12] = 198; bins[20] = 1
    hitch_record["msg"] = ("frame intervals v1: bucket=1 count=199 invalid=0 "
        "total_ns=599000000 max_ns=5000000 gt4ms=1 bins=" + ",".join(map(str, bins)))
    hitch[at] = json.dumps(hitch_record)
    cases.append(("strict-fast-with-hitch", hitch, strict_contract, False, "strict 4ms budget"))
    cases.append(("strict-unmeasured-tail", fast, replace(strict_contract, hold_frames=650),
                  False, "unmeasured tail frames"))
    tail_bins = [0] * 33; tail_bins[12] = 37
    tail = fast + [
        record("2026-08-11T12:00:04+03:00", "frame intervals v1: bucket=4 count=37 invalid=0 total_ns=111000000 max_ns=3000000 gt4ms=0 bins=" + ','.join(map(str, tail_bins))),
        record("2026-08-11T12:00:04+03:00", "frame trace (37f): bucket=4 count=37 valid=37 cpu_work_total=74000us scr_end_total=37000us"),
        record("2026-08-11T12:00:04+03:00", "frame trace end v1: bucket=4 frames=637"),
    ]
    tail_contract = replace(strict_contract, hold_frames=637)
    cases.append(("strict-completed-partial-tail", tail, tail_contract, True, None))
    cases.append(("strict-wrong-epoch-end", [line.replace('frames=637', 'frames=638') for line in tail],
                  tail_contract, False, "endpoint total mismatch"))
    cases.append(("strict-missing-partial-tail", tail[:-3] + tail[-1:], tail_contract,
                  False, "mismatched frame epoch end"))
    cases.append(("strict-duplicate-epoch-end", tail + tail[-1:], tail_contract,
                  False, "evidence after epoch end"))
    tail_bins[12] -= 1; tail_bins[20] = 1
    tail_hitch = tail.copy()
    tail_hitch[-3] = record("2026-08-11T12:00:04+03:00", "frame intervals v1: bucket=4 count=37 invalid=0 total_ns=113000000 max_ns=5000000 gt4ms=1 bins=" + ','.join(map(str, tail_bins)))
    cases.append(("strict-hitch-in-partial-tail", tail_hitch, tail_contract, False, "strict 4ms budget"))

    malformed = clean.copy()
    malformed.insert(malformed.index(next(line for line in malformed if "frame:0" in line)) + 1, "{broken-json")
    cases.append(("malformed-active-json", malformed, base_contract, False, "malformed JSON"))
    cases.append(("missing-end", without("FPS_GATE_MEASURE_END"), base_contract, False, "markers invalid"))
    reordered = clean.copy()
    reordered.insert(0, record("2026-08-11T11:59:59+03:00", "FPS_GATE_MEASURE_END"))
    cases.append(("reordered-marker", reordered, base_contract, False, "markers invalid"))
    cases.append(("wrong-map", [line.replace("arena1.bsp", "arena17.bsp") for line in clean], base_contract, False, "map mismatch"))
    cases.append(("wrong-camera", [line.replace("1052 1432 117 135 0", "0 0 0 0 0") for line in clean], base_contract, False, "camera mismatch"))
    cases.append(("wrong-render-extent", [line.replace("RENDER: 1280 x 720", "RENDER: 1920 x 1080") for line in clean], base_contract, False, "render extent mismatch"))
    cases.append(("missing-bot", without("Visor has entered"), base_contract, False, "bot population"))
    cases.append(("missing-effect", without("g_envWeather"), base_contract, False, "rain effect"))
    missing_dlight = without("dlight srf:")
    missing_dlight = without("dlightShadowProfile:", missing_dlight)
    cases.append(("missing-dlight", missing_dlight, base_contract, False, "dynamic-light"))
    particles_disabled = clean.copy()
    particle_index = next(i for i, line in enumerate(particles_disabled) if "r_particles" in line)
    particle_record = json.loads(particles_disabled[particle_index])
    particle_record["msg"] = '"r_particles" is:"0" default:"1"'
    particles_disabled[particle_index] = json.dumps(particle_record)
    cases.append(("particles-disabled", particles_disabled, base_contract, False, "particle pass"))
    cases.append(("missing-particle-runtime", without("particle emitters:"), base_contract, False, "particle runtime contract"))
    noclip_off = clean.copy()
    noclip_index = next(i for i, line in enumerate(noclip_off) if '"noclip ON"' in line)
    noclip_off.insert(noclip_index + 1, record("2026-08-11T12:00:00+03:00", "noclip OFF", "cgame"))
    cases.append(("noclip-left-off", noclip_off, base_contract, False, "missing noclip ON"))
    short_contract = Contract("self", "arena1", 2, 90.0, 299, (1052, 1432, 117, 135, 0))
    cases.append(("299-samples", synthetic_fixture(299), short_contract, False, "GPU timing blocks invalid"))
    partial = clean.copy()
    end_index = next(i for i, line in enumerate(partial) if "FPS_GATE_MEASURE_END" in line)
    partial[end_index:end_index] = [
        record("2026-08-11T12:00:04+03:00", "gpu (200f avg, ms):", "renderer.timing"),
        record("2026-08-11T12:00:04+03:00", "  world_done=5.0", "renderer.timing"),
    ]
    cases.append(("partial-gpu", partial, base_contract, False, "GPU timing blocks invalid"))
    cases.append(("total-only-gpu", without("world_done"), base_contract, False, "no non-total pass"))
    cases.append(("missing-gpu", [line for line in clean if "gpu (" not in line and "world_done=" not in line and "total=" not in line], base_contract, False, "GPU timing blocks invalid"))
    cases.append(("legacy-vk-lines-absent", without("vk timing"), base_contract, True, None))
    cases.append(("legacy-vk-pacing-absent", without("vk pacing"), base_contract, True, None))
    cases.append(("missing-vk-perf-v2", without("vk perf v2"), base_contract, False, "vk perf v2 blocks invalid"))
    cases.append(("missing-frame-pacing", without("frame trace"), base_contract, False, "frame pacing blocks invalid"))
    duplicate_frame_bucket = [
        line.replace("frame trace (200f): bucket=3", "frame trace (200f): bucket=2")
        for line in clean
    ]
    cases.append(("duplicate-frame-pacing-bucket", duplicate_frame_bucket, base_contract, False, "frame pacing blocks invalid"))
    skipped_frame_bucket = [
        line.replace("frame trace (200f): bucket=3", "frame trace (200f): bucket=4")
        for line in clean
    ]
    cases.append(("skipped-frame-pacing-bucket", skipped_frame_bucket, base_contract, False, "frame pacing blocks invalid"))
    bad_frame_count = [line.replace("count=200 valid=200 cpu_work_total=800000", "count=199 valid=199 cpu_work_total=800000") for line in clean]
    cases.append(("bad-frame-pacing-count", bad_frame_count, base_contract, False, "frame pacing blocks invalid"))
    bad_frame_valid = [line.replace("count=200 valid=200 cpu_work_total=800000", "count=200 valid=199 cpu_work_total=800000") for line in clean]
    cases.append(("bad-frame-pacing-valid", bad_frame_valid, base_contract, False, "frame pacing blocks invalid"))
    slow_frame_block = [
        line.replace("cpu_work_total=800000us", "cpu_work_total=20000000us")
        for line in clean
    ]
    cases.append(("slow-frame-aggregate", slow_frame_block, base_contract, False, "target not met"))
    bad_v2_count = [line.replace("epoch=1 bucket=2 count=200", "epoch=1 bucket=2 count=199") for line in clean]
    cases.append(("bad-v2-count", bad_v2_count, base_contract, False, "vk perf v2 blocks invalid"))
    bad_v2_valid = [line.replace("epoch=1 bucket=2 count=200 valid=200", "epoch=1 bucket=2 count=200 valid=199") for line in clean]
    cases.append(("bad-v2-valid", bad_v2_valid, base_contract, False, "vk perf v2 blocks invalid"))
    negative_v2_total = [line.replace("acquire_total_us=4000", "acquire_total_us=-1") for line in clean]
    cases.append(("negative-v2-total", negative_v2_total, base_contract, False, "vk perf v2 blocks invalid"))
    for field, value in (
        ("acquire_total_us", "4000"),
        ("submit_total_us", "2000"),
        ("present_call_total_us", "6000"),
        ("main_slot_wait_total_us", "1000"),
    ):
        zero_total = [line.replace(f"{field}={value}", f"{field}=0") for line in clean]
        cases.append((f"zero-v2-{field}", zero_total, base_contract, False, "vk perf v2 blocks invalid"))
    unstable_v2 = [line.replace("identity_stable=1", "identity_stable=0") for line in clean]
    cases.append(("unstable-v2-identity", unstable_v2, base_contract, False, "vk perf v2 blocks invalid"))
    mutated_generation = [
        line.replace("generation=1 mode=IMMEDIATE", "generation=2 mode=IMMEDIATE")
        if "vk perf v2: epoch=1 bucket=3" in line else line
        for line in clean
    ]
    cases.append(("mutated-v2-generation", mutated_generation, base_contract, False, "vk perf v2 blocks invalid"))
    mutated_epoch = [
        line.replace("epoch=1 bucket=3", "epoch=2 bucket=3")
        for line in clean
    ]
    cases.append(("mutated-v2-epoch", mutated_epoch, base_contract, False, "vk perf v2 blocks invalid"))
    wrong_slots = [line.replace("slots=2 extent=", "slots=3 extent=") for line in clean]
    cases.append(("wrong-current-slot-fixture", wrong_slots, base_contract, False, "vk perf v2 blocks invalid"))
    wrong_intent = [line.replace("policy_intent=0", "policy_intent=2") for line in clean]
    cases.append(("wrong-v2-policy-intent", wrong_intent, base_contract, False, "vk perf v2 blocks invalid"))
    wrong_inflight = [line.replace("frames_in_flight=2", "frames_in_flight=3") for line in clean]
    cases.append(("wrong-v2-frames-in-flight", wrong_inflight, base_contract, False, "vk perf v2 blocks invalid"))
    wrong_vrr = [line.replace("vrr_preferred=0", "vrr_preferred=1") for line in clean]
    cases.append(("wrong-v2-vrr-policy", wrong_vrr, base_contract, False, "vk perf v2 blocks invalid"))
    internal_as_swapchain = [
        line.replace("extent=2560x1440", "extent=1280x720")
        for line in clean
    ]
    cases.append(("internal-as-v2-swapchain-extent", internal_as_swapchain, base_contract, False, "vk perf v2 blocks invalid"))
    bad_hits = [line.replace("image_hits=0:67,1:67,2:66", "image_hits=0:67,1:67,2:65") for line in clean]
    cases.append(("bad-v2-image-histogram", bad_hits, base_contract, False, "vk perf v2 blocks invalid"))
    skipped_v2_bucket = [line.replace("epoch=1 bucket=3", "epoch=1 bucket=4") for line in clean]
    cases.append(("skipped-v2-bucket", skipped_v2_bucket, base_contract, False, "vk perf v2 blocks invalid"))
    malformed_v2 = [line.replace("identity_stable=1", "identity_stable=1 unexpected=1") for line in clean]
    cases.append(("unknown-v2-field", malformed_v2, base_contract, False, "vk perf v2 blocks invalid"))
    if platform.system() != "Windows":
        tiny_worker = [line.replace("worker_count=200 maxq=1 generation", "worker_count=1 maxq=1 generation") for line in clean]
        cases.append(("tiny-v2-worker-count", tiny_worker, base_contract, False, "worker attribution invalid"))
        zero_worker_timing = [
            line.replace("worker_queue_total_us=2000 worker_wait_total_us=4000 worker_reset_total_us=200", "worker_queue_total_us=2000 worker_wait_total_us=0 worker_reset_total_us=0")
            for line in clean
        ]
        cases.append(("queue-only-v2-worker-timing", zero_worker_timing, base_contract, False, "worker attribution invalid"))
    invalid_time = clean.copy()
    end_index = next(i for i, line in enumerate(invalid_time) if "FPS_GATE_MEASURE_END" in line)
    end_record = json.loads(invalid_time[end_index])
    end_record["ts"] = "not-a-time"
    invalid_time[end_index] = json.dumps(end_record)
    cases.append(("invalid-timestamps", invalid_time, base_contract, False, "wall-clock"))

    failed = False
    with tempfile.TemporaryDirectory(prefix="fps-analyze-selftest-") as temp_dir:
        for name, lines, contract, expect_pass, reason in cases:
            fixture = Path(temp_dir) / f"{name}.jsonl"
            fixture.write_text("\n".join(lines) + "\n", encoding="utf-8")
            summary = analyze(fixture, contract, emit=False)
            passed = summary["status"] == "PASS"
            reason_found = reason is None or any(reason in failure for failure in summary["failure_reasons"])
            if passed != expect_pass or not reason_found:
                failed = True
                print(f"SELF-TEST FAIL: {name}: {summary['failure_reasons']}")
            else:
                print(f"SELF-TEST {'PASS' if expect_pass else 'REJECT'}: {name}")
        clean_path = Path(temp_dir) / "clean.jsonl"
        clean_path.write_text("\n".join(clean) + "\n", encoding="utf-8")
        clean_summary = analyze(clean_path, base_contract, emit=False)
        if (
            clean_summary["gpu_total_ms"] != 5.5
            or clean_summary["draws_median"] != 60
            or not math.isclose(clean_summary["vk_acquire_mean_us"], 23.3333, abs_tol=0.001)
            or clean_summary["vk_submit_mean_us"] != 15
            or not math.isclose(clean_summary["vk_present_call_mean_us"], 31.6667, abs_tol=0.001)
            or clean_summary["vk_main_slot_wait_mean_us"] != 5
            or (platform.system() != "Windows" and not math.isclose(clean_summary["vk_worker_queue_mean_us"], 10.6667, abs_tol=0.001))
            or (platform.system() != "Windows" and clean_summary["vk_worker_wait_mean_us"] != 20)
            or (platform.system() != "Windows" and not math.isclose(clean_summary["vk_worker_reset_mean_us"], 1.1667, abs_tol=0.001))
            or not math.isclose(clean_summary["cpu_work_mean_us"], 5733.3, abs_tol=0.05)
            or not math.isclose(clean_summary["scr_end_mean_us"], 4733.3, abs_tol=0.05)
        ):
            failed = True
            print("SELF-TEST FAIL: contained v2 totals were not weighted over exact attempt/work coverage")
        else:
            print("SELF-TEST PASS: all three contained v2 buckets contribute to weighted means")
        sparse_legacy: list[str] = []
        legacy_frames = 0
        for line in clean:
            if '"msg": "frame:' in line or '"msg": "cl: front:' in line:
                if legacy_frames >= 214:
                    continue
                legacy_frames += 1
            sparse_legacy.append(line)
        sparse_legacy = [
            line.replace("all:10", "all:999").replace("end:10000", "end:999999")
            for line in sparse_legacy
        ]
        sparse_path = Path(temp_dir) / "sparse-legacy.jsonl"
        sparse_path.write_text("\n".join(sparse_legacy) + "\n", encoding="utf-8")
        sparse_summary = analyze(sparse_path, base_contract, emit=False)
        if (
            sparse_summary["status"] != "PASS"
            or sparse_summary["frames"] != 107
            or sparse_summary["cpu_work_mean_us"] != clean_summary["cpu_work_mean_us"]
            or sparse_summary["effective_fps"] != clean_summary["effective_fps"]
        ):
            failed = True
            print(f"SELF-TEST FAIL: sparse legacy rows affected aggregate authority: {sparse_summary['failure_reasons']}")
        else:
            print("SELF-TEST PASS: sparse legacy rows do not affect aggregate authority")
    return int(failed or run_gameplay_self_test())


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--self-test", action="store_true")
    parser.add_argument("--gameplay-seconds", type=int, default=0)
    parser.add_argument("--backend", choices=("metal", "vulkan"), default="vulkan")
    parser.add_argument("--input", type=Path)
    parser.add_argument("--tag", default="head")
    parser.add_argument("--map", dest="map_name", default="arena1")
    parser.add_argument("--bots", type=int, default=6)
    parser.add_argument("--target-fps", type=float, default=250.0)
    parser.add_argument("--hold-frames", type=int, default=600)
    parser.add_argument("--viewpos", default="1052 1432 90 135 0")
    parser.add_argument("--draw-floor", type=int, default=30)
    parser.add_argument("--render-width", type=int, default=1280)
    parser.add_argument("--render-height", type=int, default=720)
    parser.add_argument("--engine-sha256", default="unknown")
    parser.add_argument("--pax21-sha256", default="unknown")
    parser.add_argument("--particle-changed", type=int, default=0)
    parser.add_argument("--particle-mad", type=float, default=0.0)
    parser.add_argument("--particle-pixels", type=int, default=0)
    parser.add_argument("--particle-control-changed", type=int, default=0)
    parser.add_argument("--particle-control-mad", type=float, default=0.0)
    return parser.parse_args()


def analyze_gameplay(path: Path, *, seconds: int, backend: str, map_name: str, bots: int) -> dict:
    """One native-resolution moving/combat run, not an N3/multi-map verdict.

    The strict gate uses completed frame-end intervals. GPU aggregates are
    separately scoped diagnostics: neither their delayed delivery nor log wall
    time replaces unmeasured application frames.
    """
    failures = []
    records = []
    for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        try:
            row = json.loads(line)
            if not isinstance(row, dict) or not all(isinstance(row.get(k), str) for k in ("msg", "cat", "sev", "ts")):
                raise ValueError("invalid record")
            records.append(row)
        except (ValueError, TypeError):
            failures.append(f"malformed log record {number}")
    messages = [r['msg'].strip() for r in records]
    joined = '\n'.join(messages)
    failures.extend(f"runtime {r['sev']}: {r['msg'].strip()}" for r in records if r['sev'] in ('ERROR', 'FATAL'))
    if re.search(r"failed to initialize; advancing cl_renderer|died on signal|Wired crash", joined):
        failures.append('renderer fallback or crash')
    if re.search(r'You must be alive to use this command|unknown cmd |godmode OFF|UnnamedPlayer was telefragged', joined):
        failures.append('gameplay setup command rejected or godmode lost')
    marker_names = ('FPS_GAMEPLAY_SETTINGS_BEFORE', 'FPS_GAMEPLAY_BEGIN',
                    'FPS_GAMEPLAY_END', 'FPS_GAMEPLAY_SETTINGS_AFTER')
    markers = [[i for i, msg in enumerate(messages) if msg == name] for name in marker_names]
    if any(len(hits) != 1 for hits in markers) or [hits[0] for hits in markers] != sorted(hits[0] for hits in markers):
        return {'status': 'FAIL', 'failure_reasons': failures + ['invalid gameplay measurement markers']}
    before, begin, end, after = [hits[0] for hits in markers]
    if not any('godmode ON' in msg for msg in messages[:begin]):
        failures.append('missing live player/godmode setup acknowledgement')
    settings = dict(FPS_SETTINGS, fixedtime=0, timescale=1, r_pinFrameTime=0,
        r_pinShaderTime=0, r_renderScale=0, r_hdr=1, r_hdrAutoExposure=0,
        r_bloom=1, r_bloomPasses=2, r_bloomThresholdMode=0,
        r_bloomThreshold=.32, r_bloomIntensity=.5, r_particles=1,
        g_envWeather='rain', cl_renderer=backend)
    for name, expected in settings.items():
        pattern = re.compile(r'^"' + re.escape(name) + r'"\s+is:\s*"([^"^]*)(?:\^7)?"')
        for label, subset in (('before', messages[before:begin]), ('after', messages[after:])):
            values = [match.group(1) for msg in subset if (match := pattern.match(msg))]
            try:
                valid = bool(values) and all(value == expected if isinstance(expected, str)
                    else math.isclose(float(value), expected, abs_tol=1e-6) for value in values)
            except ValueError:
                valid = False
            if not valid:
                failures.append(f'{label} runtime setting {name} missing or mismatched: {values}')
    maps = [match.group(1) for msg in messages if (match := RE_GAMEPLAY_MAP.search(msg))]
    if maps != [map_name]:
        failures.append(f'gameplay map mismatch: {maps}')
    entered = sum(r['cat'] == 'server' and r['msg'].startswith('broadcast: print "')
                  and 'entered the game' in r['msg'] for r in records) - 1
    if entered != bots:
        failures.append(f'bot population mismatch: {entered}/{bots}')
    extents = [tuple(map(int, match.groups())) for msg in messages if (match := RE_RENDER_EXTENT.match(msg))]
    # GfxInfo prints only MODE when scene and output extents are equal.
    # This is the actual native branch, not an inference from r_renderScale.
    for msg in messages:
        native = re.match(r'^MODE: -?\d+, (\d+) x (\d+) (?:windowed|fullscreen) hz:', msg)
        if native:
            width, height = map(int, native.groups())
            extents.append((width, height, width, height))
    if backend == 'metal':
        # Independent native receipts: never infer drawable size from a cvar
        # or accept Vulkan gfxinfo as evidence for a different backend.
        ready = [tuple(map(int, values)) for values in re.findall(
            r'Wired native Metal RAL: registration ready logical=(\d+)x(\d+) '
            r'pixels=(\d+)x(\d+) swapInterval=(-?\d+) presentMode=(\d+) displaySync=(\d+)', joined)]
        scenes = set(tuple(map(int, values)) for values in re.findall(
            r'Wired native Metal RAL: scene format=\S+ extent=(\d+)x(\d+)', joined))
        outputs = {(r[2], r[3]) for r in ready}
        if not ready or any((r[0], r[1], *r[4:]) != (1280, 720, 0, 2, 0) for r in ready):
            failures.append('missing or mismatched native Metal presentation policy')
        if len(outputs) != 1 or len(scenes) != 1 or outputs != scenes:
            failures.append('missing, changing or scaled native Metal scene/output receipt')
        extents = [(w, h, dw, dh) for w, h in scenes for dw, dh in outputs]
    if not extents or any(w != dw or h != dh or w < 1280 or h < 720 for w, h, dw, dh in extents):
        failures.append(f'native render/output extent mismatch: {extents}')
    format_pattern = (r'Wired native Metal RAL: scene format=(\S+)' if backend == 'metal'
        else r'(?m)^\s*color(?: \(main FBO\))?\s*:\s*VK_FORMAT_(\S+)')
    formats = set(re.findall(format_pattern, joined))
    if formats != {'R16G16B16A16_SFLOAT'}:
        failures.append(f'FP16 HDR scene evidence mismatch: {sorted(formats)}')
    sampler = re.findall(rf'Wired {backend} material sampler: min=(\d+) mag=(\d+) mip=(\d+) aniso=([\d.]+) maxLod=([\d.]+)', joined)
    if not sampler or any(tuple(map(float, values[:4])) != (1, 1, 1, 8) for values in sampler):
        failures.append('missing or mismatched trilinear/aniso8 sampler')
    window = messages[begin + 1:after]
    intervals = None
    try:
        # END requests disable; the already measured partial bucket is flushed
        # on that frame and precedes SETTINGS_AFTER. It is not post-roll work.
        intervals = completed_frame_intervals(window, minimum_seconds=seconds)
        if intervals['gt4ms_count']:
            failures.append(f"strict 4ms budget exceeded ({intervals['gt4ms_count']} intervals)")
    except EvidenceError as exc:
        failures.append(str(exc))
    phases = [int(msg.rsplit('_', 1)[1]) for msg in messages[begin:end] if re.fullmatch(r'FPS_GAMEPLAY_PHASE_[1-4]', msg)]
    if phases != [1, 2, 3, 4]:
        failures.append(f'missing or reordered movement/combat phases: {phases}')
    poses = [tuple(map(int, match.groups())) for msg in messages[begin:end] if (match := RE_CAMERA.fullmatch(msg))]
    if len(poses) != 8 or len({p[:3] for p in poses}) < 3 or len({p[3:] for p in poses}) < 3:
        failures.append('insufficient observed translation/rotation across four phases')
    particle_rows = [tuple(map(int, match.groups())) for msg in messages[begin:end] if (match := RE_PARTICLE_RUNTIME.fullmatch(msg))]
    atmosphere_rows = [tuple(map(int, match.groups())) for msg in messages[begin:end] if (match := RE_ATMOSPHERE_RUNTIME.fullmatch(msg))]
    metal_effects = [tuple(map(int, values)) for values in re.findall(
        r'(?m)^metal effects submission v1: emitters=(\d+) admitted=(\d+) pool_instances=(\d+) '
        r'weather_particles=(\d+) weather_computes=(\d+) weather_draws=(\d+)$', '\n'.join(messages[begin:end]))]
    particle_work = (any(p[0] > 0 and p[1] > 0 and p[2] > 0 for p in metal_effects)
        if backend == 'metal' else any(p[1] > 0 and p[5] > 0 and p[6] > 0 for p in particle_rows))
    weather_work = (any(p[3] > 0 and p[4] > 0 and p[5] > 0 for p in metal_effects)
        if backend == 'metal' else any(p[0] > 0 and p[1] > 0 and p[2] > 0 for p in atmosphere_rows))
    if not particle_work:
        failures.append('no live particle compute/draw evidence during combat')
    if not weather_work:
        failures.append('no live weather compute/draw evidence during movement')
    metal_collision = [int(value) for value in re.findall(
        r'(?m)^metal weather collision v1: heightgrid=([01])$', '\n'.join(messages[begin:end]))]
    if backend == 'metal' and not any(metal_collision):
        failures.append('no Metal weather heightgrid submission evidence during movement')
    context_pattern = re.compile(r'^frame window context v1: bucket=(\d+) count=(\d+) focused=(\d+) minimized=(\d+) automated=([01])$')
    context = [tuple(map(int, match.groups())) for msg in window if (match := context_pattern.fullmatch(msg))]
    if not intervals or sum(c[1] for c in context) != intervals['frames'] or any(c[3] or c[2] > c[1] for c in context):
        failures.append('missing/invalid window context or minimized measured frames')
    acquire_scene = None
    if backend == 'metal':
        try:
            acquire_scene = acquire_scene_timing(messages[begin:end])
        except EvidenceError as exc:
            failures.append(str(exc))
    return {
        'status': 'FAIL' if failures else 'PASS', 'failure_reasons': failures,
        'acceptance_scope': 'single wall-clock movement/combat run; not N>=3 arena1/7/17 or visual parity acceptance',
        'backend': backend, 'map': map_name, 'bots': entered, 'requested_seconds': seconds,
        'frame_intervals': intervals, 'native_extent_observations': extents,
        'settings': settings, 'poses': poses, 'particle_runtime': particle_rows,
        'atmosphere_runtime': atmosphere_rows, 'window_context': context,
        'metal_effect_submissions': metal_effects,
        'metal_heightgrid_submissions': metal_collision,
        'metal_acquire_scene': acquire_scene,
        'effect_evidence_scope': 'validated submission counts; not GPU completion/readback or visible-pixel proof',
        'gpu_aggregate_headers_in_window': sum(
            (RE_METAL_GPU_HDR if backend == 'metal' else RE_GPU_HDR).fullmatch(msg) is not None
            for msg in messages[begin:end]),
        'gpu_spikes': [
            {'frames': int(m.group(1)), 'over4ms': int(m.group(2)), 'max_frame_ms': float(m.group(3)),
             'dominant': {} if m.group(4) == '-' else {k: int(v) for k, v in (item.split(':') for item in m.group(4).split(','))},
             'lane_max_ms': {} if m.group(5) == '-' else {k: float(v) for k, v in (item.split(':') for item in m.group(5).split(','))}}
            for m in (RE_GPU_SPIKE.fullmatch(msg) for msg in messages[begin:end]) if m],
        'pacing_spikes': [
            {'frames': int(m.group(1)), 'over4ms': int(m.group(2)),
             'dominant': {k: int(v) for k, v in (item.split(':') for item in m.group(3).split(','))},
             'max_us': {k: int(v) for k, v in (item.split(':') for item in m.group(4).split(','))}}
            for m in (RE_PACING_SPIKE.fullmatch(msg) for msg in messages[begin:end]) if m],
        'pacing_spike_scope': 'renderer frame-to-frame intervals >4ms attributed to the previous frame dominant main-thread wait (acquire/slot_wait/submit/present) or other; Vulkan, per 200 attempts (vkPacing v1)',
        'gpu_spike_scope': 'per-200-frame GPU lane-sum spike attribution (gpuSpike v1): frames whose lane sum exceeds 4ms, worst single-frame total, dominant lane per spike frame and per-lane single-frame maxima; Vulkan product only, delayed readback, not matched to wall intervals',
        'gpu_scope': 'delayed aggregate headers only; not full coverage or a matched per-frame latency attribution',
        'excluded_phases': 'initialization/map load/bot warmup before BEGIN; stopping frame, screenshot and shutdown after END',
    }


def run_gameplay_self_test() -> bool:
    settings = dict(FPS_SETTINGS, fixedtime=0, timescale=1, r_pinFrameTime=0,
        r_pinShaderTime=0, r_renderScale=0, r_hdr=1, r_hdrAutoExposure=0,
        r_bloom=1, r_bloomPasses=2, r_bloomThresholdMode=0, r_bloomThreshold=.32,
        r_bloomIntensity=.5, r_particles=1, g_envWeather='rain', cl_renderer='vulkan')
    reads = [f'"{key}" is:"{value}"' for key, value in settings.items()]
    msgs = ['FIRST GAMEPLAY FRAME mapname=maps/arena1.bsp', 'godmode ON',
        'RENDER: 2560 x 1440, MODE: -1, 2560 x 1440 windowed hz:',
        'color (main FBO) : VK_FORMAT_R16G16B16A16_SFLOAT',
        'Wired vulkan material sampler: min=1 mag=1 mip=1 aniso=8 maxLod=16',
        *['broadcast: print "player has entered the game"'] * 3,
        'FPS_GAMEPLAY_SETTINGS_BEFORE', *reads, 'FPS_GAMEPLAY_BEGIN']
    for phase in range(1, 5):
        msgs += [f'FPS_GAMEPLAY_PHASE_{phase}', f'{phase * 30} 0 100 {phase * 25} 0',
            f'{phase * 30 + 10} 0 100 {phase * 25 + 10} 0',
            'particle emitters:3 particles:8 requests:3 dropped:0/0 computes:1 draws:2',
            'atmosphere particles:8192 computes:1 draws:1 froxels:0 dispatches:0 clouds:0 heightgrid:1 depth:1 temporal:0/0 surface-milli:0/0/0/0']
    for bucket in range(1, 55):
        count = 199 if bucket == 1 else 200
        bins = [0] * 33; bins[12] = count
        msgs += [f'frame intervals v1: bucket={bucket} count={count} invalid=0 total_ns={count * 3000000} max_ns=3000000 gt4ms=0 bins=' + ','.join(map(str, bins)),
            f'frame trace (200f): bucket={bucket} count=200 valid=200 cpu_work_total=600000us scr_end_total=400000us',
            f'frame window context v1: bucket={bucket} count=200 focused=0 minimized=0 automated=0']
    msgs += ['FPS_GAMEPLAY_END', 'frame trace end v1: bucket=54 frames=10800',
        'FPS_GAMEPLAY_SETTINGS_AFTER', *reads]
    cases = [
        ('complete', msgs, True),
        ('native-gfxinfo', [s.replace('RENDER: 2560 x 1440, MODE:', 'MODE:').replace('color (main FBO)', 'color') for s in msgs], True),
        ('dead-setup', msgs + ['You must be alive to use this command.'], False),
        ('end-missing', [s for s in msgs if not s.startswith('frame trace end')], False),
        ('bloom-disabled', [s.replace('"r_bloom" is:"1"', '"r_bloom" is:"0"') for s in msgs], False),
        ('pinned-time', [s.replace('"fixedtime" is:"0"', '"fixedtime" is:"1"') for s in msgs], False),
        ('wrong-map', [s.replace('maps/arena1.bsp', 'maps/arena17.bsp') for s in msgs], False),
        ('missing-phase', [s for s in msgs if s != 'FPS_GAMEPLAY_PHASE_3'], False),
        ('missing-motion', [s for s in msgs if not RE_CAMERA.fullmatch(s)], False),
        ('missing-fx', [s for s in msgs if not s.startswith('particle emitters:')], False),
        ('missing-weather', [s for s in msgs if not s.startswith('atmosphere particles:')], False),
        ('minimized', [s.replace('minimized=0', 'minimized=200') for s in msgs], False),
        ('internal720p', [s.replace('RENDER: 2560 x 1440', 'RENDER: 1280 x 720') for s in msgs], False),
        ('wrong-sampler', [s.replace('mip=1 aniso=8', 'mip=0 aniso=8') for s in msgs], False),
    ]
    cases = [(name, source, expected, 'vulkan') for name, source, expected in cases]
    metal = [s.replace('"cl_renderer" is:"vulkan"', '"cl_renderer" is:"metal"')
        .replace('Wired vulkan material sampler:', 'Wired metal material sampler:')
        .replace('RENDER: 2560 x 1440, MODE: -1, 2560 x 1440 windowed hz:',
            'Wired native Metal RAL: registration ready logical=1280x720 pixels=2560x1440 swapInterval=0 presentMode=2 displaySync=0')
        .replace('color (main FBO) : VK_FORMAT_R16G16B16A16_SFLOAT',
            'Wired native Metal RAL: scene format=R16G16B16A16_SFLOAT extent=2560x1440')
        for s in msgs if not s.startswith(('particle emitters:', 'atmosphere particles:'))]
    effect = 'metal effects submission v1: emitters=3 admitted=8 pool_instances=32768 weather_particles=8192 weather_computes=1 weather_draws=1'
    metal.insert(metal.index('FPS_GAMEPLAY_PHASE_2'), effect)
    collision = 'metal weather collision v1: heightgrid=1'
    metal.insert(metal.index('FPS_GAMEPLAY_PHASE_2'), collision)
    metal_gpu = ('metal perf v1: frames=200 acquire_total_us=400000 submit_total_us=120000 '
                 'gpu_samples=10 gpu_total_ns=15000000 gpu_unavailable=0')
    metal.insert(metal.index('FPS_GAMEPLAY_PHASE_2'), metal_gpu)
    # A delayed postroll header and a truncated payload are not in-window evidence.
    metal.append(metal_gpu)
    metal.insert(metal.index('FPS_GAMEPLAY_PHASE_2'), 'metal perf v1: frames=200')
    paired = metal.copy()
    acquire = ('metal acquire scene v1: frames=200 samples=10 unavailable=0 acquire_ns=30000000 '
               'before_scene_ns=1000000 during_scene_ns=9000000 after_scene_ns=20000000')
    split = 'metal gpu split v1: frames=200 samples=10 scene_ns=10000000 output_ns=5000000 overlap_ns=0 gap_ns=1000000'
    paired[paired.index('FPS_GAMEPLAY_PHASE_2'):paired.index('FPS_GAMEPLAY_PHASE_2')] = [acquire, split]
    cases += [
        ('metal-acquire-paired', paired, True, 'metal'),
        ('metal-acquire-invalid-sum', [s.replace('after_scene_ns=20000000', 'after_scene_ns=10000000') for s in paired], False, 'metal'),
        ('metal-acquire-invalid-count', [s.replace('unavailable=0 acquire_ns', 'unavailable=1 acquire_ns') for s in paired], False, 'metal'),
        ('metal-acquire-missing-pair', [s for s in paired if s != split], False, 'metal'),
        ('metal-acquire-postroll', metal + [acquire, split], True, 'metal'),
        ('metal-native', metal, True, 'metal'),
        ('metal-missing-output', [s for s in metal if 'registration ready' not in s], False, 'metal'),
        ('metal-scaled-scene', [s.replace('extent=2560x1440', 'extent=1280x720') for s in metal], False, 'metal'),
        ('metal-sync', [s.replace('displaySync=0', 'displaySync=1') for s in metal], False, 'metal'),
        ('metal-no-admission', [s.replace('admitted=8', 'admitted=0') for s in metal], False, 'metal'),
        ('metal-no-draw', [s.replace('pool_instances=32768', 'pool_instances=0') for s in metal], False, 'metal'),
        ('metal-no-weather', [s.replace('weather_draws=1', 'weather_draws=0') for s in metal], False, 'metal'),
        ('metal-no-heightgrid', [s.replace('heightgrid=1', 'heightgrid=0') for s in metal], False, 'metal'),
        ('metal-heightgrid-postroll', [s for s in metal if s != collision] + [collision], False, 'metal'),
        ('metal-postroll-only', [s for s in metal if s != effect] + [effect], False, 'metal'),
    ]
    failed = False
    with tempfile.TemporaryDirectory(prefix='wired-gameplay-analyzer-') as directory:
        path = Path(directory) / 'qconsole.jsonl'
        for name, source, expected, backend in cases:
            path.write_text('\n'.join(json.dumps(dict(msg=msg, cat='server' if msg.startswith('broadcast:') else 'system',
                sev='INFO', ts='2026-09-07T00:00:00+03:00')) for msg in source), encoding='utf-8')
            result = analyze_gameplay(path, seconds=32, backend=backend, map_name='arena1', bots=2)
            okay = (result['status'] == 'PASS') == expected
            if name == 'metal-native':
                okay &= result['gpu_aggregate_headers_in_window'] == 1
                okay &= result['metal_acquire_scene'] is None
            if name == 'metal-acquire-paired':
                okay &= result['metal_acquire_scene']['samples'] == 10
                okay &= result['metal_acquire_scene']['after_scene_mean_ms'] == 2
            if name == 'metal-acquire-postroll':
                okay &= result['metal_acquire_scene'] is None
            failed |= not okay
            print(f"GAMEPLAY SELF-TEST {'PASS' if okay else 'FAIL'}: {name}")
            if not okay:
                print(result['failure_reasons'])
    return failed


def main() -> int:
    args = parse_args()
    if args.self_test:
        return run_self_test()
    if args.input is None:
        raise SystemExit("--input is required")
    if args.gameplay_seconds:
        if not 32 <= args.gameplay_seconds <= 120:
            raise SystemExit('--gameplay-seconds must be 32..120')
        summary = analyze_gameplay(args.input, seconds=args.gameplay_seconds,
            backend=args.backend, map_name=args.map_name, bots=args.bots)
        for name, value in (('engine_sha256', args.engine_sha256), ('pax21_sha256', args.pax21_sha256)):
            summary[name] = value
            if not re.fullmatch(r'[0-9a-f]{64}', value):
                summary['failure_reasons'].append(f'missing {name}')
                summary['status'] = 'FAIL'
        summary_path = args.input.parent / f'summary-{args.tag}.json'
        summary_path.write_text(json.dumps(summary, indent=2, sort_keys=True) + '\n', encoding='utf-8')
        print('SUMMARY FILE:', summary_path)
        print('FRAME INTERVALS:', json.dumps(summary.get('frame_intervals')))
        for failure in summary['failure_reasons']:
            print('FAIL:', failure)
        return int(summary['status'] != 'PASS')
    origin = tuple(int(float(value)) for value in args.viewpos.split())
    if len(origin) != 4:
        raise SystemExit("--viewpos must contain x y z yaw")
    values = (origin[0], origin[1], origin[2] + 27, origin[3], 0)
    if args.render_width <= 0 or args.render_height <= 0:
        raise SystemExit("--render-width/--render-height must be positive")
    contract = Contract(
        tag=args.tag,
        map_name=args.map_name,
        bots=args.bots,
        target_fps=args.target_fps,
        hold_frames=args.hold_frames,
        viewpos=values,
        render_width=args.render_width,
        render_height=args.render_height,
        draw_floor=args.draw_floor,
        engine_sha256=args.engine_sha256,
        pax21_sha256=args.pax21_sha256,
        particle_changed=args.particle_changed,
        particle_mad=args.particle_mad,
        particle_pixels=args.particle_pixels,
        particle_control_changed=args.particle_control_changed,
        particle_control_mad=args.particle_control_mad,
    )
    summary = analyze(args.input, contract)
    summary_path = args.input.parent / f"summary-{args.tag}.json"
    summary_path.write_text(json.dumps(summary, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print("SUMMARY:", json.dumps(summary, sort_keys=True))
    print("SUMMARY FILE:", summary_path)
    for failure in summary["failure_reasons"]:
        print("FAIL:", failure)
    return 1 if summary["failure_reasons"] else 0


if __name__ == "__main__":
    raise SystemExit(main())
