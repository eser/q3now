#!/usr/bin/env python3
# Procedural generator for the modern HDR flare sprite (flare_modern.png).
#
# 256x256 RGBA. RGB is white (255,255,255) everywhere so the engine's
# `rgbGen vertex` tints the flare with the light's colour; all structure
# lives in the ALPHA channel so `blendFunc GL_SRC_ALPHA GL_ONE` shapes a
# crisp core under the exposure-invariant additive pipeline.
#
# Alpha = max-composite of three layers:
#   1. bright defined core      — flat-top out to ~4% radius, near-255
#   2. inverse-square halo       — 1/(1+(r/r0)^2), the physical light falloff
#   3. subtle anamorphic streak  — thin horizontal accent, peak ~35% of core
#   (4. faint lens-ghost ring     — barely-visible thin ring at ~45% radius)
#
# Deterministic: no randomness. Regenerate with `python3 flare_modern_gen.py`.

import math, struct, zlib, os

N = 256
CX = (N - 1) / 2.0
CY = (N - 1) / 2.0
R_MAX = N / 2.0                       # 128 px = radius 1.0 in normalised terms

# --- layer tuning (normalised radius, 0..1 across the half-extent) ---
CORE_FLAT   = 0.04                    # alpha holds ~flat inside this
CORE_EDGE   = 0.06                    # core fully handed off to halo by here
# inverse-square r0 chosen so alpha ~= 0.5 at r~0.15 (see calibration below):
#   a = 1/(1+(r/r0)^2) = 0.5  =>  (r/r0)^2 = 1  =>  r0 = r  => r0 = 0.15
HALO_R0     = 0.15
HALO_CUT    = 0.80                    # hard taper to 0 by here (clean edge)
STREAK_HALF = 0.70                    # streak reaches 70% of half-width each side
STREAK_THICK= 0.022                   # ~2.2% sprite-height thick at centre
STREAK_PEAK = 0.36                    # peak streak alpha as fraction of full (subtle)
RING_R      = 0.45                    # faint ghost ring radius
RING_W      = 0.018                   # ring thickness (normalised)
RING_PEAK   = 0.06                    # very faint


def smoothstep(e0, e1, x):
    if e1 == e0:
        return 0.0 if x < e0 else 1.0
    t = (x - e0) / (e1 - e0)
    t = max(0.0, min(1.0, t))
    return t * t * (3.0 - 2.0 * t)


def core_alpha(r):
    # flat near-full inside CORE_FLAT, smooth handoff to the halo value at CORE_EDGE
    if r <= CORE_FLAT:
        return 1.0
    if r >= CORE_EDGE:
        return 0.0
    # 1 -> 0 across the thin shell so the halo takes over continuously
    return 1.0 - smoothstep(CORE_FLAT, CORE_EDGE, r)


def halo_alpha(r):
    a = 1.0 / (1.0 + (r / HALO_R0) ** 2)
    # smooth taper to 0 over the last stretch so there is no hard ring at the edge
    a *= (1.0 - smoothstep(HALO_CUT - 0.20, HALO_CUT, r))
    return max(0.0, a)


def streak_alpha(nx, ny):
    # nx, ny normalised to half-extent (-1..1). Horizontal accent.
    ax = abs(nx)
    if ax > STREAK_HALF:
        return 0.0
    # vertical gaussian-ish profile (thin), horizontal taper to the tips
    vert = math.exp(-(ny / STREAK_THICK) ** 2 * 0.5)
    horiz = 1.0 - smoothstep(0.0, STREAK_HALF, ax)      # 1 at centre -> 0 at tip
    horiz = horiz ** 1.5                                  # bias brightness toward centre
    return STREAK_PEAK * vert * horiz


def ring_alpha(r):
    d = abs(r - RING_R)
    if d > RING_W:
        return 0.0
    return RING_PEAK * (1.0 - smoothstep(0.0, RING_W, d))


def build_rgba():
    buf = bytearray(N * N * 4)
    for y in range(N):
        for x in range(N):
            dx = (x - CX) / R_MAX        # normalised -1..1
            dy = (y - CY) / R_MAX
            r = math.hypot(dx, dy)        # 0..~1.41 (corner)

            a_core = core_alpha(r)
            a_halo = halo_alpha(r)
            a_ring = ring_alpha(r)
            a_radial = max(a_core, a_halo, a_ring)        # max-composite radial parts
            a_streak = streak_alpha(dx, dy)

            # streak adds on top of the radial halo (additive accent), clamped
            a = min(1.0, a_radial + a_streak)

            o = (y * N + x) * 4
            buf[o + 0] = 255            # R
            buf[o + 1] = 255            # G
            buf[o + 2] = 255            # B
            buf[o + 3] = int(round(a * 255.0))
    return buf


def write_png(path, rgba, w, h):
    def chunk(typ, data):
        c = struct.pack(">I", len(data)) + typ + data
        c += struct.pack(">I", zlib.crc32(typ + data) & 0xFFFFFFFF)
        return c
    sig = b"\x89PNG\r\n\x1a\n"
    ihdr = struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0)   # 8-bit, colour type 6 (RGBA)
    # filter byte 0 (None) per scanline
    raw = bytearray()
    stride = w * 4
    for y in range(h):
        raw.append(0)
        raw += rgba[y * stride:(y + 1) * stride]
    idat = zlib.compress(bytes(raw), 9)
    with open(path, "wb") as f:
        f.write(sig)
        f.write(chunk(b"IHDR", ihdr))
        f.write(chunk(b"IDAT", idat))
        f.write(chunk(b"IEND", b""))


if __name__ == "__main__":
    # repo-root/tools/flare-sprite/ -> repo-root/modfiles/gfx/misc/flare_modern.png
    here = os.path.dirname(os.path.abspath(__file__))
    root = os.path.abspath(os.path.join(here, "..", ".."))
    out = os.path.join(root, "modfiles", "gfx", "misc", "flare_modern.png")
    rgba = build_rgba()
    write_png(out, rgba, N, N)
    print("wrote", out, os.path.getsize(out), "bytes")
