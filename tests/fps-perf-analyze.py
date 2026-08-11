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


RE_ALL = re.compile(
    r"frame:(\d+)\s+all:\s*(\d+)\s+sv:\s*(\d+)\s+ev:\s*(\d+)"
    r"\s+cl:\s*(\d+)\s+gm:\s*(\d+)\s+rf:\s*(\d+)\s+bk:\s*(\d+)"
)
RE_END = re.compile(r"\bend:(\d+)")
RE_GPU_HDR = re.compile(r"^gpu \((\d+)f avg, ms\):")
RE_GPU_LABEL = re.compile(r"^\s*([A-Za-z_]+)=([\d.]+)")
RE_GPU_TOTAL = re.compile(r"^\s*total=([\d.]+)")
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
RE_FRAME_PACING = re.compile(
    r"frame trace \((\d+)f\): bucket=(\d+) count=(\d+) valid=(\d+)"
    r" cpu_work_total=(\d+)us scr_end_total=(\d+)us"
)
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
    r"^particle emitters:(\d+) particles:(\d+) computes:(\d+) draws:(\d+)$"
)


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
    raw_frame_pacing: list[tuple[str, ...]] = []
    vk_pacing_pair_errors = 0
    pending_vk_pacing = False
    raw_gpu: list[dict[str, float]] = []
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
    particle_computes = 0
    particle_draws = 0

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
                particle_computes = max(particle_computes, int(particle_runtime.group(3)))
                particle_draws = max(particle_draws, int(particle_runtime.group(4)))

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
            frame_pacing = RE_FRAME_PACING.fullmatch(msg) if category == "system" else None
            if frame_pacing:
                raw_frame_pacing.append(frame_pacing.groups())
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

    # The first diagnostic buckets can straddle the pre-roll/marker boundary.
    # They are presence evidence only; every reported metric uses clean buckets.
    gpu_blocks = raw_gpu[1:]
    vkt_lines = raw_vkt[1:]
    pacing_lines = raw_vk_pacing[1:]
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

    draws = [int(values[6]) for values in vkt_lines]
    draws_median = int(statistics.median(draws)) if draws else None
    vkt_metric_names = ("fence", "ft_fence", "acquire", "submit", "present")
    vkt_medians = {
        name: statistics.median(int(values[index]) for values in vkt_lines)
        for index, name in enumerate(vkt_metric_names, 1)
        if vkt_lines
    }
    pacing_metric_names = ("main_slot_wait", "worker_queue", "worker_fence", "worker_count", "max_queue")
    pacing_medians = {
        name: statistics.median(int(values[index]) for values in pacing_lines)
        for index, name in enumerate(pacing_metric_names, 3)
        if pacing_lines
    }
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
    if min(particle_emitters, particle_emitted, particle_computes, particle_draws) < 1:
        failures.append(
            "particle runtime contract missing "
            f"(emitters={particle_emitters}, particles={particle_emitted}, "
            f"computes={particle_computes}, draws={particle_draws})"
        )
    expected_render = (contract.render_width, contract.render_height)
    render_observed = render_observations[-1] if render_observations else None
    if render_observed is None or render_observed[:2] != expected_render:
        failures.append(
            f"render extent mismatch (expected={expected_render}, observed={render_observations})"
        )
    elif contract.particle_pixels != render_observed[2] * render_observed[3]:
        failures.append(
            "capture extent does not match renderer window "
            f"(pixels={contract.particle_pixels}, window={render_observed[2]}x{render_observed[3]})"
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
    vkt_sizes_ok = all(int(values[0]) == 200 for values in raw_vkt)
    if len(raw_vkt) < 3 or len(vkt_lines) < 2 or not vkt_sizes_ok:
        failures.append(f"vk timing blocks invalid (raw={len(raw_vkt)}, clean={len(vkt_lines)})")
    pacing_sizes_ok = all(int(values[0]) == 200 for values in raw_vk_pacing)
    pacing_buckets = [int(values[1]) for values in raw_vk_pacing]
    pacing_order_ok = len(pacing_buckets) == len(set(pacing_buckets)) and all(
        ((current - previous) & 0xFFFFFFFF) == 1
        for previous, current in zip(pacing_buckets, pacing_buckets[1:])
    )
    pacing_aligned = len(raw_vk_pacing) == len(raw_vkt) and vk_pacing_pair_errors == 0
    if (
        len(raw_vk_pacing) < 3
        or len(pacing_lines) < 2
        or not pacing_sizes_ok
        or not pacing_order_ok
        or not pacing_aligned
    ):
        failures.append(
            "vk pacing blocks invalid "
            f"(raw={len(raw_vk_pacing)}, clean={len(pacing_lines)}, "
            f"ordered={pacing_order_ok}, aligned={pacing_aligned})"
        )
    else:
        slots = [int(values[2]) for values in pacing_lines]
        worker_queues = [int(values[4]) for values in pacing_lines]
        worker_fences = [int(values[5]) for values in pacing_lines]
        worker_counts = [int(values[6]) for values in pacing_lines]
        max_queues = [int(values[7]) for values in pacing_lines]
        if any(slot <= 0 or slot != slots[0] for slot in slots):
            failures.append(f"vk pacing slot cardinality invalid ({slots})")
        elif platform.system() == "Windows":
            if any(worker_queues) or any(worker_fences) or any(worker_counts) or any(max_queues):
                failures.append(
                    "vk pacing Windows worker attribution must be zero "
                    f"(queue={worker_queues}, fence={worker_fences}, count={worker_counts}, max={max_queues})"
                )
        elif any(
            count < 200 - slots[index] or count > 200 + slots[index]
            for index, count in enumerate(worker_counts)
        ) or any(depth < 1 or depth > slots[index] for index, depth in enumerate(max_queues)):
            failures.append(
                "vk pacing worker attribution invalid "
                f"(slots={slots}, counts={worker_counts}, max_queue={max_queues})"
            )
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
            f"{render_observed[2]}x{render_observed[3]}" if render_observed else None
        ),
        "target_fps": contract.target_fps,
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
        "particle_computes": particle_computes,
        "particle_draws": particle_draws,
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
        "vk_blocks_raw": len(raw_vkt),
        "vk_blocks_clean": len(vkt_lines),
        "vk_fence_median_ms": vkt_medians.get("fence"),
        "vk_frame_time_fence_median_ms": vkt_medians.get("ft_fence"),
        "vk_acquire_median_ms": vkt_medians.get("acquire"),
        "vk_submit_median_ms": vkt_medians.get("submit"),
        "vk_present_median_ms": vkt_medians.get("present"),
        "vk_pacing_blocks_raw": len(raw_vk_pacing),
        "vk_pacing_blocks_clean": len(pacing_lines),
        "vk_pacing_pair_errors": vk_pacing_pair_errors,
        "vk_pacing_bucket_first": pacing_buckets[0] if pacing_buckets else None,
        "vk_pacing_bucket_last": pacing_buckets[-1] if pacing_buckets else None,
        "vk_main_slot_wait_median_us": pacing_medians.get("main_slot_wait"),
        "vk_worker_queue_median_us": pacing_medians.get("worker_queue"),
        "vk_worker_fence_median_us": pacing_medians.get("worker_fence"),
        "vk_worker_count_median": pacing_medians.get("worker_count"),
        "vk_max_queue_median": pacing_medians.get("max_queue"),
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
        print(f"  dlight/weather/particles: surfaces={dlight_frames} shadow_passes={dlight_profile_passes} candidates={dlight_candidates} rain={weather_rain} particles={particles_enabled} runtime={particle_emitters}/{particle_emitted}/{particle_computes}/{particle_draws}")
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
        print(f"  vk blocks/draws         : raw={len(raw_vkt)} clean={len(vkt_lines)} median={draws_median}")
        print(
            "  vk pacing medians (ms)  : "
            f"fence={vkt_medians.get('fence')} ft_fence={vkt_medians.get('ft_fence')} "
            f"acquire={vkt_medians.get('acquire')} submit={vkt_medians.get('submit')} "
            f"present={vkt_medians.get('present')}"
        )
        print(
            "  vk pacing attribution   : "
            f"raw={len(raw_vk_pacing)} clean={len(pacing_lines)} "
            f"slot={pacing_medians.get('main_slot_wait')}us/f "
            f"queue={pacing_medians.get('worker_queue')}us/work "
            f"fence={pacing_medians.get('worker_fence')}us/work "
            f"work={pacing_medians.get('worker_count')} maxq={pacing_medians.get('max_queue')}"
        )
        print(f"  wall FPS                : {wall_fps:.1f}" if finite(wall_fps) else "  wall FPS                : unavailable")
        print(f"  EFFECTIVE FPS           : {effective_fps:.1f}" if finite(effective_fps) else "  EFFECTIVE FPS           : unavailable")
        print(f"  verdict                 : {summary['status']}")
        print(f"=== END RESULT [{contract.tag}] ===\n")
    return summary


def record(ts: str, msg: str, cat: str = "system") -> str:
    return json.dumps({"ts": ts, "sev": "INFO", "cat": cat, "msg": msg})


def synthetic_fixture(samples: int = 650) -> list[str]:
    base = dt.datetime.fromisoformat("2026-08-11T12:00:00+03:00")
    lines = [
        record(base.isoformat(), "cls.state: -> CA_ACTIVE (FIRST GAMEPLAY FRAME mapname=maps/arena1.bsp serverTime=400 numEntities=17 framecount=8)", "client"),
        record(base.isoformat(), 'broadcast: print "local has entered the game\\n"', "server"),
    ]
    lines.extend(record(base.isoformat(), 'broadcast: print "Visor has entered the game\\n"', "server") for _ in range(2))
    lines.extend(
        [
            record(base.isoformat(), "RENDER: 1280 x 720, MODE: -1, 1280 x 720 windowed hz:N/A", "renderer.init"),
            record(base.isoformat(), "FPS_GATE_CAMERA_CHECK"),
            record(base.isoformat(), "1052 1432 117 135 0", "cgame"),
            record(base.isoformat(), "1052 1432 117 135 0", "cgame"),
            record(base.isoformat(), "1052 1432 117 135 0", "cgame"),
            record(base.isoformat(), "noclip ON", "cgame"),
            record(base.isoformat(), "FPS_GATE_BOT_VIS_PRE_BEGIN"),
            record(base.isoformat(), "(md3) 2 sin 1 sclip  4 sout 0 bin 0 bclip 0 bout", "renderer.cmd"),
            record(base.isoformat(), "FPS_GATE_BOT_VIS_PRE_END"),
            record(base.isoformat(), "bot_teleport: Visor placed at (989 1575 69) yaw=295", "game"),
            record(base.isoformat(), "particle emitters:1 particles:18 computes:1 draws:2", "renderer.cmd"),
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
            record(base.isoformat(), "FPS_GATE_MEASURE_BEGIN"),
        ]
    )
    for index in range(samples):
        ts = (base + dt.timedelta(seconds=(index + 1) / 100.0)).isoformat()
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
            cpu_work = (9000, 4000, 4200)[index // 200]
            scr_end = (8000, 3000, 3200)[index // 200]
            lines.extend(
                [
                    record(ts, f"frame trace (200f): bucket={pacing_bucket} count=200 valid=200 cpu_work_total={cpu_work * 200}us scr_end_total={scr_end * 200}us"),
                    record(ts, "gpu (200f avg, ms):", "renderer.timing"),
                    record(ts, f"  world_done={total - 1:.2f}", "renderer.timing"),
                    record(ts, f"  total={total:.2f}", "renderer.timing"),
                    record(ts, f"vk timing (200f avg): fence={fence}ms/f  ft_fence={frame_time_fence}ms/f  acquire=0ms/f  submit=0ms/f  present=0ms/f  draws={draws}/f(msdf=0)  pipebinds=2/f(msdf=0)", "renderer.timing"),
                    record(ts, f"vk pacing (200f avg): bucket={pacing_bucket} slots=2 main_slot_wait={slot_wait}us/f worker_queue={worker_queue}us/work worker_fence={worker_fence}us/work worker_count=200 max_queue=1", "renderer.timing"),
                ]
            )
    end = base + dt.timedelta(seconds=samples / 100.0)
    lines.append(record(end.isoformat(), "FPS_GATE_MEASURE_END"))
    return lines


def run_self_test() -> int:
    base_contract = Contract("self", "arena1", 2, 90.0, 650, (1052, 1432, 117, 135, 0))
    cases: list[tuple[str, list[str], Contract, bool, str | None]] = []
    clean = synthetic_fixture()
    cases.append(("clean", clean, base_contract, True, None))
    cases.append(("below-target", clean, replace(base_contract, target_fps=250.0), False, "target not met"))

    def without(pattern: str, source: list[str] = clean) -> list[str]:
        return [line for line in source if pattern not in line]

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
    cases.append(("missing-vkt", without("vk timing"), base_contract, False, "vk timing blocks invalid"))
    cases.append(("missing-vk-pacing", without("vk pacing"), base_contract, False, "vk pacing blocks invalid"))
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
    negative_pacing = [line.replace("main_slot_wait=5us/f", "main_slot_wait=-5us/f") for line in clean]
    cases.append(("negative-vk-pacing", negative_pacing, base_contract, False, "vk pacing blocks invalid"))
    zero_worker = [line.replace("worker_count=200", "worker_count=0") for line in clean]
    cases.append(("zero-worker-count", zero_worker, base_contract, False, "worker attribution invalid"))
    tiny_worker = [line.replace("worker_count=200", "worker_count=1") for line in clean]
    cases.append(("tiny-worker-count", tiny_worker, base_contract, False, "worker attribution invalid"))
    huge_queue = [line.replace("max_queue=1", "max_queue=999") for line in clean]
    cases.append(("huge-worker-queue", huge_queue, base_contract, False, "worker attribution invalid"))
    duplicate_bucket = [line.replace("bucket=3 slots", "bucket=2 slots") for line in clean]
    cases.append(("duplicate-pacing-bucket", duplicate_bucket, base_contract, False, "vk pacing blocks invalid"))
    out_of_order_bucket = [
        line.replace("bucket=2 slots", "bucket=3 slots").replace(
            "bucket=3 slots=2 main_slot_wait=7", "bucket=2 slots=2 main_slot_wait=7"
        )
        for line in clean
    ]
    cases.append(("out-of-order-pacing-bucket", out_of_order_bucket, base_contract, False, "vk pacing blocks invalid"))
    skipped_bucket = [line.replace("bucket=3 slots", "bucket=4 slots") for line in clean]
    cases.append(("skipped-pacing-bucket", skipped_bucket, base_contract, False, "vk pacing blocks invalid"))
    pacing_records = [line for line in clean if "vk pacing (" in line]
    separated_pacing = [line for line in clean if "vk pacing (" not in line]
    separated_end = next(i for i, line in enumerate(separated_pacing) if "FPS_GATE_MEASURE_END" in line)
    separated_pacing[separated_end:separated_end] = pacing_records
    cases.append(("separated-vkt-pacing", separated_pacing, base_contract, False, "vk pacing blocks invalid"))
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
            or clean_summary["draws_median"] != 61
            or clean_summary["vk_fence_median_ms"] != 0
            or clean_summary["vk_frame_time_fence_median_ms"] != 0
            or clean_summary["vk_main_slot_wait_median_us"] != 6
            or clean_summary["vk_worker_queue_median_us"] != 12
            or clean_summary["vk_worker_fence_median_us"] != 22
            or not math.isclose(clean_summary["cpu_work_mean_us"], 5733.3, abs_tol=0.05)
            or not math.isclose(clean_summary["scr_end_mean_us"], 4733.3, abs_tol=0.05)
        ):
            failed = True
            print("SELF-TEST FAIL: contaminated first bucket influenced clean medians")
        else:
            print("SELF-TEST PASS: contaminated first bucket discarded")
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
    return 1 if failed else 0


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--self-test", action="store_true")
    parser.add_argument("--input", type=Path)
    parser.add_argument("--tag", default="head")
    parser.add_argument("--map", dest="map_name", default="arena1")
    parser.add_argument("--bots", type=int, default=6)
    parser.add_argument("--target-fps", type=float, default=250.0)
    parser.add_argument("--hold-frames", type=int, default=650)
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


def main() -> int:
    args = parse_args()
    if args.self_test:
        return run_self_test()
    if args.input is None:
        raise SystemExit("--input is required")
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
