#!/usr/bin/env python3
"""Objective rendered-delta gate for the emissive authority inspection view."""

from __future__ import annotations

import argparse
import subprocess
from pathlib import Path

WIDTH, HEIGHT = 1280, 720


def decode(decoder: Path, image: Path) -> bytes:
    result = subprocess.run([str(decoder), str(image)], capture_output=True, check=False)
    if result.returncode:
        raise ValueError(result.stderr.decode(errors="replace").strip())
    expected = WIDTH * HEIGHT * 3
    if len(result.stdout) != expected:
        raise ValueError(f"{image} decoded to {len(result.stdout)} bytes, expected {expected}")
    return result.stdout


def pixels(raw: bytes):
    return zip(raw[0::3], raw[1::3], raw[2::3])


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--artifacts", type=Path, required=True)
    parser.add_argument("--png2raw", type=Path, required=True)
    args = parser.parse_args()
    try:
        off = decode(args.png2raw, args.artifacts / "off.png")
        on = decode(args.png2raw, args.artifacts / "on.png")
        off_pixels = list(pixels(off))
        on_pixels = list(pixels(on))
        changed = sum(
            1 for a, b in zip(off_pixels, on_pixels)
            if max(abs(a[channel] - b[channel]) for channel in range(3)) >= 4
        )
        off_rejected = sum(1 for r, g, b in pixels(off) if r >= 180 and g <= 80 and b <= 100)
        on_rejected = sum(1 for r, g, b in pixels(on) if r >= 180 and g <= 80 and b <= 100)
        off_cyan = sum(1 for r, g, b in pixels(off) if r <= 80 and g >= 150 and b >= 190)
        on_cyan = sum(1 for r, g, b in pixels(on) if r <= 80 and g >= 150 and b >= 190)
        if changed < 600 or on_rejected - off_rejected < 200 or on_cyan - off_cyan < 200:
            raise ValueError(
                "emissive overlay is vacuous: "
                f"changed={changed} rejected-delta={on_rejected-off_rejected} "
                f"cyan-delta={on_cyan-off_cyan}"
            )
        print(
            f"PASS: emissive authority visual changed={changed} "
            f"rejected-delta={on_rejected-off_rejected} cyan-delta={on_cyan-off_cyan}"
        )
        return 0
    except (OSError, ValueError) as error:
        print(f"FAIL: {error}")
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
