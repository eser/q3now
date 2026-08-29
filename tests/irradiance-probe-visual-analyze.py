#!/usr/bin/env python3
"""Objective 1280x720 gate for the authored SH probe visual fixture."""

from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path


WIDTH = 1280
HEIGHT = 720
MODEL_ROI = (615, 665, 320, 410)


def decode(decoder: Path, image: Path) -> bytes:
    result = subprocess.run(
        [str(decoder), str(image)], capture_output=True, check=False
    )
    if result.returncode:
        detail = result.stderr.decode("utf-8", errors="replace").strip()
        raise ValueError(f"png2raw failed for {image}: {detail}")
    expected = WIDTH * HEIGHT * 3
    if len(result.stdout) != expected:
        raise ValueError(
            f"{image} decoded to {len(result.stdout)} bytes; expected {expected}"
        )
    return result.stdout


def pixels(raw: bytes, roi: tuple[int, int, int, int]):
    x0, x1, y0, y1 = roi
    for y in range(y0, y1):
        for x in range(x0, x1):
            offset = (y * WIDTH + x) * 3
            yield raw[offset], raw[offset + 1], raw[offset + 2]


def mean_rgb(raw: bytes, roi: tuple[int, int, int, int]) -> float:
    samples = list(pixels(raw, roi))
    return sum(sum(sample) for sample in samples) / (3.0 * len(samples))


def cyan_count(raw: bytes) -> int:
    return sum(1 for r, g, b in pixels(raw, (0, WIDTH, 0, HEIGHT))
               if r <= 80 and g >= 150 and b >= 190)


def analyze(artifacts: Path, decoder: Path) -> None:
    captures = {
        name: decode(decoder, artifacts / f"{name}.png")
        for name in ("off", "on", "warm", "cool")
    }
    off_cyan = cyan_count(captures["off"])
    on_cyan = cyan_count(captures["on"])
    if on_cyan - off_cyan < 1000:
        raise ValueError(
            f"probe overlay cyan delta too small: off={off_cyan} on={on_cyan}"
        )

    warm_mean = mean_rgb(captures["warm"], MODEL_ROI)
    cool_mean = mean_rgb(captures["cool"], MODEL_ROI)
    warm_pixels = list(pixels(captures["warm"], MODEL_ROI))
    cool_pixels = list(pixels(captures["cool"], MODEL_ROI))
    changed = sum(
        1
        for warm, cool in zip(warm_pixels, cool_pixels)
        if sum(abs(warm[channel] - cool[channel]) for channel in range(3)) / 3.0 >= 12.0
    )
    warm_bright = sum(1 for sample in warm_pixels if sum(sample) >= 180)
    cool_bright = sum(1 for sample in cool_pixels if sum(sample) >= 180)
    if cool_mean - warm_mean < 5.0 or changed < 500:
        raise ValueError(
            "fixed model ROI did not cross the authored SH field: "
            f"warm={warm_mean:.2f} cool={cool_mean:.2f} changed={changed}"
        )
    if warm_bright >= 50 or cool_bright < 100:
        raise ValueError(
            "fixed model silhouette lacks the expected warm/cool contrast: "
            f"warm-bright={warm_bright} cool-bright={cool_bright}"
        )
    print(
        "PASS: probe visual metrics "
        f"cyan-delta={on_cyan - off_cyan} "
        f"model-mean={warm_mean:.2f}->{cool_mean:.2f} "
        f"changed={changed} bright={warm_bright}->{cool_bright}"
    )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--artifacts", type=Path, required=True)
    parser.add_argument("--png2raw", type=Path, required=True)
    args = parser.parse_args()
    try:
        analyze(args.artifacts, args.png2raw)
    except (OSError, ValueError) as error:
        print(f"FAIL: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
