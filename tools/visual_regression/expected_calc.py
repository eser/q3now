#!/usr/bin/env python3
"""expected_calc.py — predict the swapchain byte value for an authored colour.

Models the Wired colour pipeline that an authored display-referred sRGB
colour traverses on its way to the swapchain:

    authored sRGB (0-1)
      -> sRGBToLinear                       (m8 / sampleColorTex decode)
      -> * exposure_bias  (r_brightness)    [only if it flows through tonemap.frag]
      -> * tonemap_exposure (r_tonemapExposure) [inside applyTonemap]
      -> tonemap operator                   [identity if UI bypasses tonemap]
      -> linearToSRGB                        (gamma.frag / B8G8R8A8_SRGB store)
      -> byte = round(clamp01 * 255)

The sRGB transfer functions and the four tonemap operators are ports of
`code/renderervk/shaders/tonemap.frag` (PBR Neutral / AgX / Lottes /
Reinhard) and `gen_frag.tmpl` (sRGBToLinear / linearToSRGB) — kept in
sync by hand. If those shaders change, update this file.

Wired UI fullscreen overlays (menu / HUD / console) bypass the tonemap
operator by design (post-Block-5b: UI renders into the post-tonemap LDR
intermediate) — so VR scenes use operator="identity" for UI/HUD regions.
The PBR-Neutral / AgX / Lottes / Reinhard operators are provided for
world-surface checks and for `--operator` "what if it went through
tonemap" diagnostics.

CLI:
    python expected_calc.py --rgba 0.08 0.08 0.12 0.92 --operator identity
    python expected_calc.py --rgba 0.5 0.5 0.5 1 --operator pbr_neutral
    python expected_calc.py --selftest
"""

from __future__ import annotations

import argparse
import json
import math
import sys

# ── sRGB transfer functions (mirror gen_frag.tmpl) ─────────────────────
# Standard IEC 61966-2-1 piecewise curve. Cutoffs / exponents must match
# the shader (`sRGBToLinear` / `linearToSRGB` in gen_frag.tmpl).

_SRGB_TO_LIN_CUTOFF = 0.04045
_LIN_TO_SRGB_CUTOFF = 0.0031308


def srgb_to_linear(c: float) -> float:
    c = max(c, 0.0)
    if c <= _SRGB_TO_LIN_CUTOFF:
        return c / 12.92
    return ((c + 0.055) / 1.055) ** 2.4


def linear_to_srgb(c: float) -> float:
    c = max(c, 0.0)
    if c <= _LIN_TO_SRGB_CUTOFF:
        return c * 12.92
    return 1.055 * (c ** (1.0 / 2.4)) - 0.055


def _v3(fn, rgb):
    return (fn(rgb[0]), fn(rgb[1]), fn(rgb[2]))


def _clamp01(x: float) -> float:
    return 0.0 if x < 0.0 else (1.0 if x > 1.0 else x)


# ── tonemap operators (mirror tonemap.frag) ────────────────────────────


def tonemap_identity(rgb):
    return tuple(rgb)


def tonemap_pbr_neutral(rgb):
    """Khronos glTF PBR Neutral (2024). Hue-preserving, mid-tones pass through."""
    start_compression = 0.8 - 0.04   # 0.76
    desaturation = 0.15

    r, g, b = rgb
    x = min(r, g, b)
    offset = (x - 6.25 * x * x) if x < 0.08 else 0.04
    r, g, b = r - offset, g - offset, b - offset

    peak = max(r, g, b)
    if peak < start_compression:
        return (r, g, b)

    d = 1.0 - start_compression      # 0.24
    new_peak = 1.0 - d * d / (peak + d - start_compression)
    scale = new_peak / peak
    r, g, b = r * scale, g * scale, b * scale

    gmix = 1.0 - 1.0 / (desaturation * (peak - new_peak) + 1.0)
    # mix(color, new_peak*vec3(1), gmix)
    return (r + (new_peak - r) * gmix,
            g + (new_peak - g) * gmix,
            b + (new_peak - b) * gmix)


# AgX matrices — column-major in GLSL: `agxInset * color` == M @ color where
# M's *columns* are the rows written in the shader source.
_AGX_INSET_COLS = (
    (0.856627153315983, 0.0951212405381588, 0.0482516061458583),
    (0.137318972929847, 0.761241990602591, 0.101439036467562),
    (0.11189821299995, 0.0767994186031903, 0.811302368396859),
)
_AGX_OUTSET_COLS = (
    (1.1271005818144368, -0.11060664309660323, -0.016493938717834573),
    (-0.1413297634984383, 1.157823702216272, -0.016493938717834257),
    (-0.1413297634984383, -0.11060664309660324, 1.2519364065950405),
)
_AGX_MIN_EV = -12.47393
_AGX_MAX_EV = 4.026069


def _mat3_mul_cols(cols, v):
    # cols[i] is column i. result = sum_i cols[i] * v[i]
    return tuple(
        cols[0][r] * v[0] + cols[1][r] * v[1] + cols[2][r] * v[2]
        for r in range(3)
    )


def _agx_sigmoid_scalar(x: float) -> float:
    x2 = x * x
    x4 = x2 * x2
    return (15.5 * x4 * x2
            - 40.14 * x4 * x
            + 31.96 * x4
            - 6.868 * x2 * x
            + 0.4298 * x2
            + 0.1191 * x
            - 0.00232)


def tonemap_agx(rgb):
    """Troy Sobotka AgX (2023), 6th-order polynomial approximation."""
    c = _mat3_mul_cols(_AGX_INSET_COLS, rgb)
    c = tuple(math.log2(max(v, 1e-10)) for v in c)
    c = tuple(_clamp01((v - _AGX_MIN_EV) / (_AGX_MAX_EV - _AGX_MIN_EV)) for v in c)
    c = tuple(_agx_sigmoid_scalar(v) for v in c)
    c = _mat3_mul_cols(_AGX_OUTSET_COLS, c)
    return tuple(max(v, 0.0) for v in c)


# Lottes (GDC 2016) — defaults match the shader's spec-constant defaults.
_LOTTES_CONTRAST = 1.6
_LOTTES_SHOULDER = 0.977
_LOTTES_MID_IN = 0.18
_LOTTES_MID_OUT = 0.267
_LOTTES_HDR_MAX = 8.0


def tonemap_lottes(rgb, *, contrast=_LOTTES_CONTRAST, shoulder=_LOTTES_SHOULDER,
                   mid_in=_LOTTES_MID_IN, mid_out=_LOTTES_MID_OUT, hdr_max=_LOTTES_HDR_MAX):
    a, d = contrast, shoulder
    # b and c are scalars when the params are scalars (they are, host-side).
    denom = (hdr_max ** (a * d) - mid_in ** (a * d)) * mid_out
    b = (-(mid_in ** a) + (hdr_max ** a) * mid_out) / denom
    c_const = ((hdr_max ** (a * d)) * (mid_in ** a)
               - (hdr_max ** a) * (mid_in ** (a * d)) * mid_out) / denom

    def f(x: float) -> float:
        x = max(x, 0.0)   # pow of negative base is NaN in GLSL; UI colours are >= 0
        return (x ** a) / ((x ** (a * d)) * b + c_const)

    return tuple(f(v) for v in rgb)


def tonemap_reinhard(rgb):
    return tuple(v / (1.0 + v) for v in rgb)


_OPERATORS = {
    "identity": tonemap_identity,
    "passthrough": tonemap_identity,
    "pbr_neutral": tonemap_pbr_neutral,
    "pbrneutral": tonemap_pbr_neutral,
    "agx": tonemap_agx,
    "lottes": tonemap_lottes,
    "reinhard": tonemap_reinhard,
}

# r_tonemap integer -> operator name (matches tonemap.frag: 1=PBR Neutral,
# 2=AgX, 3=Lottes, 4=Reinhard; 0/out-of-range = identity).
TONEMAP_MODE_NAMES = {0: "identity", 1: "pbr_neutral", 2: "agx", 3: "lottes", 4: "reinhard"}


def operator_by_name(name):
    key = str(name).strip().lower().replace(" ", "_").replace("-", "_")
    if key not in _OPERATORS:
        raise ValueError(f"unknown tonemap operator {name!r}; known: {sorted(set(_OPERATORS))}")
    return _OPERATORS[key]


# ── full pipeline ──────────────────────────────────────────────────────


def expected_pipeline(authored_rgb, *, operator="identity", exposure_bias=1.0,
                      tonemap_exposure=1.0, display_mode="sdr", peak_luminance=1000):
    """Return a dict with the linear, display-encoded, and byte/code values
    for an authored display-referred sRGB RGB triple traversing the pipeline.

    `operator` may be a name ("identity", "pbr_neutral", ...) or callable.
    `exposure_bias` (r_brightness) and `tonemap_exposure` (r_tonemapExposure)
    are applied only when the colour flows through tonemap.frag — i.e. they
    are no-ops for operator="identity" by convention here (UI bypasses
    tonemap.frag entirely, so no exposure stage). For non-identity operators
    they are applied before the operator, matching tonemap.frag's main().

    `display_mode` (Phase 6B3'-d8): "sdr" (default) → linear→sRGB encode, 8-bit
    byte per channel. "hdr10" → linear (BT.709) → BT.2020 + PQ (ST.2084) encode
    via hdr_math, 10-bit code per channel (the A2B10G10R10_UNORM_PACK32 RGB
    channels). Under hdr10 the pbr_neutral operator uses its peak-aware HDR
    shoulder (hdr_math.pbr_neutral_hdr with peak = peak_luminance/100); the
    other operators use their SDR curves under HDR10, matching tonemap.frag.
    """
    op = operator if callable(operator) else operator_by_name(operator)
    op_name = (op.__name__ if callable(operator) else str(operator).strip().lower().replace(" ", "_").replace("-", "_"))
    lin = _v3(srgb_to_linear, authored_rgb)
    if op is not tonemap_identity:
        lin = tuple(v * exposure_bias * tonemap_exposure for v in lin)

    if str(display_mode).lower() == "hdr10":
        import hdr_math
        peak_norm = float(peak_luminance) / 100.0
        if op_name in ("pbr_neutral", "pbrneutral") or op is tonemap_pbr_neutral:
            toned = hdr_math.pbr_neutral_hdr(lin, peak_norm)
        else:
            # AgX / Lottes / Reinhard / identity: SDR curve under HDR10
            # (mirrors tonemap.frag — extended HDR variants of those are a
            # future refinement).
            toned = op(lin)
        codes = hdr_math.linear_bt709_to_hdr10_code10(toned, graphics_white_nits=hdr_math.GRAPHICS_WHITE_NITS)
        return {
            "authored": list(authored_rgb),
            "operator": (op.__name__ if callable(operator) else operator),
            "display_mode": "hdr10",
            "peak_luminance": int(peak_luminance),
            "linear_in": list(lin),
            "linear_toned": list(toned),
            "code10": list(codes),     # A2B10G10R10 RGB code values [0,1023]
            "byte": list(codes),       # alias — diff_harness compares 'byte'; for hdr10 these are 10-bit codes
            "bits": 10,
        }

    toned = op(lin)
    disp = _v3(linear_to_srgb, toned)
    disp = tuple(_clamp01(v) for v in disp)
    byte = tuple(int(round(v * 255.0)) for v in disp)
    return {
        "authored": list(authored_rgb),
        "operator": (op.__name__ if callable(operator) else operator),
        "display_mode": "sdr",
        "linear_in": list(lin),
        "linear_toned": list(toned),
        "display_srgb": list(disp),
        "byte": list(byte),
        "bits": 8,
    }


def expected_byte_rgba(authored_rgba, *, operator="identity", exposure_bias=1.0,
                       tonemap_exposure=1.0, display_mode="sdr", peak_luminance=1000):
    """Convenience: 4-tuple authored -> 4-tuple byte (sdr) or code (hdr10).
    Alpha is *not* sent through the colour pipeline (alpha is never an sRGB
    quantity); it is quantised directly — to 8 bits for sdr, to 2 bits for
    hdr10 (the A2B10G10R10 alpha channel). Alpha-blend-over-scene effects are
    not modelled — looser per-region tolerance covers those (see tolerance.yaml)."""
    rgb = authored_rgba[:3]
    a = authored_rgba[3] if len(authored_rgba) > 3 else 1.0
    res = expected_pipeline(rgb, operator=operator, exposure_bias=exposure_bias,
                            tonemap_exposure=tonemap_exposure,
                            display_mode=display_mode, peak_luminance=peak_luminance)
    if res.get("bits") == 10:
        res["byte"].append(int(round(_clamp01(a) * 3.0)))   # 2-bit alpha
        if "code10" in res:
            res["code10"].append(int(round(_clamp01(a) * 3.0)))
    else:
        res["byte"].append(int(round(_clamp01(a) * 255.0)))
    res["authored"] = list(authored_rgba)
    return res


# ── self-test ──────────────────────────────────────────────────────────


def _selftest() -> int:
    failures = []

    # sRGB round-trip is the identity to ~float precision.
    for c in (0.0, 0.001, 0.0031308, 0.04045, 0.08, 0.12, 0.5, 0.847, 1.0):
        rt = linear_to_srgb(srgb_to_linear(c))
        if abs(rt - c) > 1e-4:
            failures.append(f"sRGB round-trip {c}: got {rt}")

    # identity operator: byte == round(authored * 255) within 1 LSB.
    for c in (0.08, 0.12, 0.5, 0.847, 1.0, 0.0):
        b = expected_pipeline((c, c, c), operator="identity")["byte"][0]
        if abs(b - round(c * 255.0)) > 1:
            failures.append(f"identity byte for {c}: got {b}, want ~{round(c*255.0)}")

    # The famous "menu blue" authored colour, bypassing tonemap (correct
    # post-5b behaviour): 0.08 -> ~20, 0.12 -> ~31.
    r = expected_byte_rgba((0.08, 0.08, 0.12, 0.92), operator="identity")
    if r["byte"][:3] != [20, 20, 31] and not all(
        abs(r["byte"][i] - want) <= 1 for i, want in enumerate((20, 20, 31))
    ):
        failures.append(f"menu-blue identity byte: got {r['byte']}, want ~[20,20,31,235]")

    # Same colour pushed *through* PBR Neutral tonemap (the broken state the
    # diagnose cascade chased): should crush near-black.
    rt = expected_byte_rgba((0.08, 0.08, 0.12, 0.92), operator="pbr_neutral")
    if rt["byte"][0] > 6:
        failures.append(f"menu-blue through PBR Neutral: got {rt['byte']}, expected near-black (<7)")

    # Mid-grey through PBR Neutral passes through nearly unchanged (the
    # operator's design intent): 0.5 sRGB -> linear ~0.214 -> tonemap ~0.214 -> sRGB ~0.5.
    g = expected_pipeline((0.5, 0.5, 0.5), operator="pbr_neutral")["byte"][0]
    if abs(g - 128) > 12:
        failures.append(f"mid-grey through PBR Neutral: got {g}, want ~128")

    # Reinhard compresses: 1.0 linear -> 0.5.
    rein = tonemap_reinhard((1.0, 0.0, 4.0))
    if abs(rein[0] - 0.5) > 1e-9 or abs(rein[2] - 0.8) > 1e-9:
        failures.append(f"reinhard: got {rein}, want (0.5, 0.0, 0.8)")

    if failures:
        for f in failures:
            print("FAIL:", f, file=sys.stderr)
        print(f"\n{len(failures)} self-test failure(s).", file=sys.stderr)
        return 1
    print("expected_calc.py self-test: OK")
    return 0


# ── CLI ────────────────────────────────────────────────────────────────


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description="Predict swapchain byte for an authored sRGB colour.")
    ap.add_argument("--rgba", nargs="+", type=float, metavar="V",
                    help="authored colour: 3 (RGB) or 4 (RGBA) floats in 0-1")
    ap.add_argument("--operator", default="identity",
                    help="tonemap operator: identity | pbr_neutral | agx | lottes | reinhard "
                         "(UI/HUD regions use identity — they bypass tonemap by design)")
    ap.add_argument("--exposure-bias", type=float, default=1.0,
                    help="r_brightness (applied only for non-identity operators)")
    ap.add_argument("--tonemap-exposure", type=float, default=1.0,
                    help="r_tonemapExposure (applied only for non-identity operators)")
    ap.add_argument("--json", action="store_true", help="emit JSON")
    ap.add_argument("--selftest", action="store_true", help="run built-in checks and exit")
    args = ap.parse_args(argv)

    if args.selftest:
        return _selftest()

    if not args.rgba:
        ap.error("--rgba is required (or use --selftest)")
    if len(args.rgba) not in (3, 4):
        ap.error("--rgba needs 3 or 4 values")

    res = expected_byte_rgba(args.rgba, operator=args.operator,
                             exposure_bias=args.exposure_bias,
                             tonemap_exposure=args.tonemap_exposure)
    if args.json:
        print(json.dumps(res))
    else:
        print(f"authored      : {res['authored']}")
        print(f"operator      : {res['operator']}")
        print(f"linear (in)   : {[round(v, 6) for v in res['linear_in']]}")
        print(f"linear (toned): {[round(v, 6) for v in res['linear_toned']]}")
        print(f"display sRGB  : {[round(v, 6) for v in res['display_srgb']]}")
        print(f"byte rgba     : {res['byte']}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
