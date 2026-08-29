#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later

"""Fail-closed WiredUI semantic/structural pixel parity analyzer.

The comparison deliberately ignores the animated 3D/background material while
retaining normalized UI regions.  It compares the actual WebGPU framebuffer to
both Metal and Vulkan evidence using resolution-independent edge geometry and a
bounded, contrast-normalized pixel error.  Semantic receipts separately prove
that the expected authored roots, list interaction, layers, HUD and crosshair
were active; a visually plausible placeholder therefore cannot pass alone.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import re
import struct
import subprocess
import sys
from pathlib import Path
from typing import Dict, Iterable, List, Mapping, Sequence, Tuple


SHA256 = re.compile(r"[0-9a-f]{64}")
SURFACES = ("menu", "servers", "loading", "console", "arena17")
ROIS = {
    "menu": ((0.34, 0.16, 0.66, 0.84),),
    "servers": ((0.00, 0.00, 1.00, 0.34), (0.00, 0.84, 1.00, 1.00)),
    "loading": ((0.00, 0.00, 1.00, 1.00),),
    "console": ((0.00, 0.00, 1.00, 0.52),),
    "arena17": (
        (0.00, 0.68, 0.18, 1.00), (0.82, 0.68, 1.00, 1.00),
        (0.46, 0.42, 0.54, 0.58), (0.42, 0.00, 0.58, 0.10),
    ),
}
NATIVE_IMAGES = {
    "menu": ("renderer-switch", "metal-menu.png", "vulkan-menu.png"),
    "servers": ("matrix", "metal/servers.png", "vulkan/servers.png"),
    "loading": ("loading", "metal.png", "vulkan.png"),
    "console": ("renderer-switch", "metal-console.png", "vulkan-console.png"),
    "arena17": ("renderer-switch", "metal-game.png", "vulkan-game.png"),
}
FORBIDDEN_LOG = re.compile(
    r"\bFATAL\b|Sys_Error|died on signal|validation error|\bVUID-|"
    r"renderer fallback|renderer .* failed to initialize", re.IGNORECASE
)


class EvidenceError(RuntimeError):
    pass


def require(condition: bool, message: str) -> None:
    if not condition:
        raise EvidenceError(message)


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def load_json(path: Path) -> Mapping[str, object]:
    require(path.is_file() and path.stat().st_size > 0, f"missing metadata: {path}")
    value = json.loads(path.read_text(encoding="utf-8"))
    require(isinstance(value, Mapping), f"metadata must be an object: {path}")
    return value


def png_dimensions(path: Path) -> Tuple[int, int]:
    require(path.is_file() and path.stat().st_size > 0, f"missing PNG: {path}")
    header = path.read_bytes()[:24]
    require(len(header) == 24 and header[:8] == b"\x89PNG\r\n\x1a\n"
            and header[12:16] == b"IHDR", f"invalid PNG: {path}")
    return struct.unpack(">II", header[16:24])


def decode_rgb(path: Path, decoder: Path) -> Tuple[bytes, int, int]:
    width, height = png_dimensions(path)
    completed = subprocess.run([str(decoder), str(path)], stdout=subprocess.PIPE,
                               stderr=subprocess.PIPE)
    require(completed.returncode == 0,
            f"png2raw failed for {path}: {completed.stderr.decode('utf-8', 'replace').strip()}")
    require(len(completed.stdout) == width * height * 3,
            f"decoded RGB byte count mismatch: {path}")
    return completed.stdout, width, height


def normalized_luma(raw: bytes, width: int, height: int,
                    out_width: int = 320, out_height: int = 180) -> List[float]:
    result: List[float] = []
    for oy in range(out_height):
        y = min(height - 1, (oy * height + height // (2 * out_height)) // out_height)
        for ox in range(out_width):
            x = min(width - 1, (ox * width + width // (2 * out_width)) // out_width)
            offset = (y * width + x) * 3
            result.append((raw[offset] * 54 + raw[offset + 1] * 183
                           + raw[offset + 2] * 19) / (255.0 * 256.0))
    return result


def indices_for_rois(rois: Sequence[Tuple[float, float, float, float]],
                     width: int = 320, height: int = 180) -> List[int]:
    selected = set()
    for left, top, right, bottom in rois:
        x0, x1 = int(left * width), max(int(left * width) + 1, int(right * width))
        y0, y1 = int(top * height), max(int(top * height) + 1, int(bottom * height))
        for y in range(max(0, y0), min(height, y1)):
            selected.update(range(y * width + max(0, x0), y * width + min(width, x1)))
    return sorted(selected)


def edge_set(luma: Sequence[float], selected: Iterable[int],
             width: int = 320, height: int = 180) -> set[int]:
    chosen = set(selected)
    edges = set()
    for index in chosen:
        x, y = index % width, index // width
        if x + 1 < width and index + 1 in chosen and abs(luma[index] - luma[index + 1]) >= 0.10:
            edges.add(index)
        if y + 1 < height and index + width in chosen and abs(luma[index] - luma[index + width]) >= 0.10:
            edges.add(index)
    return edges


def dilate(points: set[int], radius: int = 2, width: int = 320,
           height: int = 180) -> set[int]:
    result = set()
    for index in points:
        x, y = index % width, index // width
        for dy in range(-radius, radius + 1):
            for dx in range(-radius, radius + 1):
                if abs(dx) + abs(dy) > radius:
                    continue
                nx, ny = x + dx, y + dy
                if 0 <= nx < width and 0 <= ny < height:
                    result.add(ny * width + nx)
    return result


def structural_metrics(web: Sequence[float], native: Sequence[float],
                       selected: Sequence[int]) -> Mapping[str, float]:
    require(bool(selected), "empty UI region")
    web_values = [web[index] for index in selected]
    native_values = [native[index] for index in selected]
    web_mean = sum(web_values) / len(web_values)
    native_mean = sum(native_values) / len(native_values)
    web_sd = math.sqrt(sum((value - web_mean) ** 2 for value in web_values) / len(web_values))
    native_sd = math.sqrt(sum((value - native_mean) ** 2 for value in native_values) / len(native_values))
    require(web_sd >= 0.015 and native_sd >= 0.015, "blank/default-material UI region")
    normalized_error = sum(abs((web[index] - web_mean) / max(web_sd, 0.03)
                               - (native[index] - native_mean) / max(native_sd, 0.03))
                           for index in selected) / len(selected)
    web_edges = edge_set(web, selected)
    native_edges = edge_set(native, selected)
    require(len(web_edges) >= max(8, len(selected) // 1000), "WebGPU UI edge set is empty")
    require(len(native_edges) >= max(8, len(selected) // 1000), "native UI edge set is empty")
    web_hit = len(web_edges & dilate(native_edges)) / len(web_edges)
    native_hit = len(native_edges & dilate(web_edges)) / len(native_edges)
    overlap = 2.0 * web_hit * native_hit / max(0.0001, web_hit + native_hit)
    density_ratio = min(len(web_edges), len(native_edges)) / max(len(web_edges), len(native_edges))
    return {"edge_overlap": overlap, "edge_density_ratio": density_ratio,
            "normalized_pixel_error": normalized_error,
            "web_sd": web_sd, "native_sd": native_sd}


def require_web_semantics(surface: str, meta: Mapping[str, object], png: Path) -> str:
    require(meta.get("backend") == "webgpu" and meta.get("surface") == surface,
            f"{surface}: backend/surface receipt mismatch")
    require(meta.get("map") == "arena17", f"{surface}: content map mismatch")
    require(png_dimensions(png) == (1280, 720), f"{surface}: WebGPU extent mismatch")
    source = str(meta.get("source_sha256", ""))
    capture = str(meta.get("capture_sha256", ""))
    require(SHA256.fullmatch(source) is not None, f"{surface}: source SHA missing")
    require(SHA256.fullmatch(capture) is not None and capture == digest(png),
            f"{surface}: capture SHA mismatch")
    require(int(meta.get("capturePasses", 0)) > 0
            and int(meta.get("captureDynamicRange", 0)) >= 16
            and int(meta.get("captureNonBackgroundPixels", 0)) >= 1280 * 720 // 200,
            f"{surface}: blank/default-material capture receipt")
    host = meta.get("hostReceipt")
    require(isinstance(host, Mapping) and host.get("status") == "ready"
            and host.get("error") == "" and host.get("lost") is None,
            f"{surface}: WebGPU host/device-loss receipt")
    if surface == "servers":
        require(meta.get("opened") == 19 and meta.get("scrolled") == 23
                and meta.get("clicked") == 31
                and meta.get("rowCenter") == meta.get("clickedCanvas"),
                "servers: open/scroll/real-pointer selection receipt mismatch")
    elif surface == "loading":
        require(meta.get("loading") == 19, "loading: authored layer receipt mismatch")
    elif surface == "console":
        require(meta.get("console") == 12 and meta.get("loadingClosed") == 0,
                "console: layer receipt mismatch")
    elif surface == "menu":
        trace = meta.get("menuToggleRoundTrip")
        roots = trace.get("authoredRoots") if isinstance(trace, Mapping) else None
        require(isinstance(trace, Mapping) and trace.get("passed") is True
                and isinstance(roots, Mapping) and roots.get("passed") is True
                and roots.get("count") == 25, "menu: 25-root/focus receipt mismatch")
    elif surface == "arena17":
        require(meta.get("hudReceipt") == 7
                and meta.get("initialHudExtent") == [1280, 720]
                and meta.get("resizedHudExtent") == [1600, 900]
                and meta.get("restoredHudExtent") == [1280, 720],
                "arena17: HUD/crosshair/resize receipt mismatch")
    return source


def require_native_log(path: Path, markers: Sequence[str]) -> None:
    require(path.is_file() and path.stat().st_size > 0, f"missing native log: {path}")
    text = path.read_text(encoding="utf-8", errors="replace")
    forbidden = FORBIDDEN_LOG.search(text)
    if forbidden is not None:
        raise EvidenceError(f"native log contains forbidden evidence: {forbidden.group(0)}")
    for marker in markers:
        require(marker in text, f"native log missing receipt {marker!r}: {path}")


def analyze(webgpu: Path, renderer_switch: Path, loading: Path, matrix: Path,
            decoder: Path) -> None:
    require(decoder.is_file(), f"png2raw missing: {decoder}")
    source: str | None = None
    require_native_log(renderer_switch / "renderer-switch-ui.log",
                       ("WiredCrosshair: loaded default crosshair", "cl_renderer"))
    require_native_log(loading / "metal.log", ("WiredCrosshair: loaded default crosshair",))
    require_native_log(loading / "vulkan.log", ("WiredCrosshair: loaded default crosshair",))
    require_native_log(matrix / "metal.log", ("WiredUI: loaded menu 'servers'",))
    require_native_log(matrix / "vulkan.log", ("WiredUI: loaded menu 'servers'",))

    roots = {"renderer-switch": renderer_switch, "loading": loading, "matrix": matrix}
    for surface in SURFACES:
        web_png = webgpu / f"{surface}.png"
        meta = load_json(webgpu / f"{surface}.meta.json")
        current_source = require_web_semantics(surface, meta, web_png)
        require(source is None or current_source == source,
                f"{surface}: mixed WebGPU build identities")
        source = current_source
        web_raw, web_width, web_height = decode_rgb(web_png, decoder)
        web_luma = normalized_luma(web_raw, web_width, web_height)
        selected = indices_for_rois(ROIS[surface])
        group, metal_name, vulkan_name = NATIVE_IMAGES[surface]
        for backend, name in (("metal", metal_name), ("vulkan", vulkan_name)):
            native_png = roots[group] / name
            native_raw, native_width, native_height = decode_rgb(native_png, decoder)
            require(native_width * 9 == native_height * 16,
                    f"{surface}/{backend}: native evidence is not 16:9")
            metrics = structural_metrics(web_luma,
                                         normalized_luma(native_raw, native_width, native_height),
                                         selected)
            require(metrics["edge_overlap"] >= 0.18,
                    f"{surface}/{backend}: semantic edge overlap {metrics['edge_overlap']:.3f} < 0.18")
            require(metrics["edge_density_ratio"] >= 0.20,
                    f"{surface}/{backend}: edge-density ratio {metrics['edge_density_ratio']:.3f} < 0.20")
            require(metrics["normalized_pixel_error"] <= 2.25,
                    f"{surface}/{backend}: normalized pixel error "
                    f"{metrics['normalized_pixel_error']:.3f} > 2.25")
            print(f"{surface}/{backend}: overlap={metrics['edge_overlap']:.3f} "
                  f"density={metrics['edge_density_ratio']:.3f} "
                  f"pixel={metrics['normalized_pixel_error']:.3f}")


def self_test() -> int:
    width, height = 320, 180
    selected = indices_for_rois(((0.2, 0.2, 0.8, 0.8),), width, height)
    base = [0.05] * (width * height)
    for y in range(40, 140):
        for x in (90, 230):
            base[y * width + x] = 0.9
    for x in range(90, 231):
        for y in (40, 140):
            base[y * width + x] = 0.9
    shifted = [0.05] * len(base)
    for index, value in enumerate(base):
        x, y = index % width, index // width
        if value > 0.5 and x + 1 < width:
            shifted[y * width + x + 1] = value
    good = structural_metrics(base, shifted, selected)
    require(good["edge_overlap"] >= 0.80, "self-test rejected bounded one-pixel drift")
    blank = [0.05] * len(base)
    try:
        structural_metrics(base, blank, selected)
    except EvidenceError:
        print("PASS: blank/default-material mutation rejected")
    else:
        print("FAIL: blank/default-material mutation accepted", file=sys.stderr)
        return 1
    unrelated = [0.05] * len(base)
    for y in range(50, 130):
        for x in (66, 252):
            unrelated[y * width + x] = 0.9
    for x in range(66, 253):
        for y in (50, 130):
            unrelated[y * width + x] = 0.9
    bad = structural_metrics(base, unrelated, selected)
    require(bad["edge_overlap"] < 0.18, "self-test accepted unrelated geometry")
    print("PASS: WiredUI WebGPU/native parity analyzer fails closed")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--webgpu", type=Path)
    parser.add_argument("--renderer-switch", type=Path)
    parser.add_argument("--loading", type=Path)
    parser.add_argument("--matrix", type=Path)
    parser.add_argument("--png2raw", type=Path)
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    try:
        if args.self_test:
            return self_test()
        if not all((args.webgpu, args.renderer_switch, args.loading, args.matrix, args.png2raw)):
            parser.error("all evidence paths and --png2raw are required")
        analyze(args.webgpu, args.renderer_switch, args.loading, args.matrix, args.png2raw)
    except (EvidenceError, OSError, ValueError, json.JSONDecodeError) as error:
        print(f"FAIL: {error}", file=sys.stderr)
        return 1
    print("PASS: actual WebGPU matches Metal/Vulkan WiredUI semantic geometry and reviewed pixel tolerances")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
