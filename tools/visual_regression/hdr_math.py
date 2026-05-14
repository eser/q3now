"""hdr_math — Phase 6B3'-d8 HDR10 colour-space helpers for the visual-regression kit.

Mirrors the host-side math in code/renderervk/shaders/gamma.frag (the HDR10
output branch) and code/renderervk/shaders/tonemap.frag (the HDR tonemap
shoulder).  expected_calc.py and readback.py both consume these so the
"expected" side and the "captured" side share one source of truth.

Pipeline (HDR10 / r_hdrDisplay 1):

    scene-linear (BT.709, after tonemap)
      -> tonemap operator HDR shoulder  (graphics white = 1.0, peak = hdr_peak_norm)
      -> gamma pass: linear *= GRAPHICS_WHITE_NITS (graphics white -> ~100 nits)
                     linear  = BT709_TO_BT2020 @ linear     (primaries, still linear)
                     code    = pq_encode(linear / 10000.0)  (ST.2084 EOTF^-1, [0,1])
      -> A2B10G10R10_UNORM_PACK32 swapchain (10-bit code value per RGB channel)

readback runs that in reverse: 10-bit code -> code/1023 -> pq_decode -> *10000
nits -> BT2020_TO_BT709 -> /GRAPHICS_WHITE_NITS -> scene-linear-ish, for
comparison against the SDR-equivalent expected linear value (looser tolerance).

Constants per Rec. ITU-R BT.2100 (PQ system) and BT.2020 (primaries).
"""

from __future__ import annotations

# --- SMPTE ST.2084 / Rec. BT.2100 PQ (Perceptual Quantizer) -----------------

_PQ_M1 = 2610.0 / 16384.0          # 0.1593017578125
_PQ_M2 = 2523.0 / 4096.0 * 128.0   # 78.84375
_PQ_C1 = 3424.0 / 4096.0           # 0.8359375
_PQ_C2 = 2413.0 / 4096.0 * 32.0    # 18.8515625
_PQ_C3 = 2392.0 / 4096.0 * 32.0    # 18.6875

#: graphics white ("paper white") in nits — the luminance the tonemapped value
#: 1.0 (diffuse white / UI) maps to.  Must match GRAPHICS_WHITE_NITS in gamma.frag.
GRAPHICS_WHITE_NITS = 100.0

#: PQ encodes luminance normalised to this peak.
PQ_PEAK_NITS = 10000.0


def pq_encode(luma_norm: float) -> float:
    """ST.2084 EOTF^-1.  Input: luminance normalised to [0,1] (i.e. nits/10000),
    clamped.  Output: PQ code value in [0,1]."""
    L = min(max(luma_norm, 0.0), 1.0)
    Lm1 = L ** _PQ_M1
    return ((_PQ_C1 + _PQ_C2 * Lm1) / (1.0 + _PQ_C3 * Lm1)) ** _PQ_M2


def pq_decode(code: float) -> float:
    """ST.2084 EOTF.  Input: PQ code value in [0,1].  Output: luminance
    normalised to [0,1] (multiply by 10000 for nits)."""
    V = min(max(code, 0.0), 1.0)
    Vm2 = V ** (1.0 / _PQ_M2)
    num = max(Vm2 - _PQ_C1, 0.0)
    den = _PQ_C2 - _PQ_C3 * Vm2
    if den <= 0.0:
        return 1.0
    return (num / den) ** (1.0 / _PQ_M1)


# --- BT.709 <-> BT.2020 primary conversion (linear light, D65) ---------------
# Row-major 3x3 (apply as out[i] = sum_j M[i][j] * in[j]).  BT.709's gamut is a
# subset of BT.2020's, so 709->2020 never clips; 2020->709 can produce small
# negatives for very saturated colours (caller should clamp for display, but for
# regression comparison the raw value is fine).

BT709_TO_BT2020 = (
    (0.627403895934699,  0.329283038377884, 0.043313065687417),
    (0.069097289358232,  0.919540395075459, 0.011362315566309),
    (0.016391438875150,  0.088013307877226, 0.895595253247624),
)

BT2020_TO_BT709 = (
    ( 1.660491,   -0.587641,  -0.072850),
    (-0.124550,    1.132900,  -0.008349),
    (-0.018151,   -0.100579,   1.118730),
)


def _mat3_apply(m, v):
    return tuple(m[i][0] * v[0] + m[i][1] * v[1] + m[i][2] * v[2] for i in range(3))


def bt709_to_bt2020(rgb):
    return _mat3_apply(BT709_TO_BT2020, rgb)


def bt2020_to_bt709(rgb):
    return _mat3_apply(BT2020_TO_BT709, rgb)


# --- forward: scene-linear BT.709 -> HDR10 10-bit code values ----------------

def linear_bt709_to_hdr10_code10(rgb_lin, *, graphics_white_nits: float = GRAPHICS_WHITE_NITS):
    """Mirror gamma.frag's hdr_mode==1 output branch.  Input: scene-linear
    BT.709 (post-tonemap, graphics white ~= 1.0).  Output: tuple of three
    10-bit integer code values [0,1023] (the A2B10G10R10 RGB channels)."""
    nits = tuple(max(c, 0.0) * graphics_white_nits for c in rgb_lin)
    nits = bt709_to_bt2020(nits)
    code = tuple(pq_encode(c / PQ_PEAK_NITS) for c in nits)
    return tuple(int(round(min(max(c, 0.0), 1.0) * 1023.0)) for c in code)


# --- reverse: HDR10 10-bit code values -> scene-linear BT.709 ----------------

def hdr10_code10_to_linear_bt709(code10, *, graphics_white_nits: float = GRAPHICS_WHITE_NITS):
    """Inverse of linear_bt709_to_hdr10_code10 — for readback.py.  Input: three
    10-bit code values [0,1023].  Output: scene-linear BT.709 floats (graphics
    white ~= 1.0).  Note: not exactly invertible at low nits due to 10-bit PQ
    quantisation — HDR scenes use a looser comparison tolerance accordingly."""
    norm = tuple(min(max(c, 0), 1023) / 1023.0 for c in code10)
    nits = tuple(pq_decode(c) * PQ_PEAK_NITS for c in norm)
    nits709 = bt2020_to_bt709(nits)
    return tuple(c / graphics_white_nits for c in nits709)


# --- HDR tonemap shoulder (mirror tonemap.frag) ------------------------------
# Only PBR Neutral has a true peak-aware HDR shoulder in tonemap.frag this turn;
# AgX / Lottes / Reinhard use their SDR curves under HDR10 (documented there).
# expected_calc.py picks the matching operator function; the peak parameter is
# hdr_peak_norm = r_hdrPeakLuminance / 100.

def pbr_neutral_hdr(rgb, peak: float):
    """Peak-aware PBR Neutral — mirror tonemapPBRNeutralHDR() in tonemap.frag.
    peak == 1.0 reduces to the SDR PBR Neutral curve."""
    start_compression = (0.8 - 0.04) * peak
    desaturation = 0.15
    c = list(rgb)
    x = min(c)
    offset = (x - 6.25 * x * x) if x < 0.08 else 0.04
    c = [v - offset for v in c]
    pk = max(c)
    if pk < start_compression:
        return tuple(c)
    d = peak - start_compression
    new_peak = peak - d * d / (pk + d - start_compression)
    scale = new_peak / pk
    c = [v * scale for v in c]
    g = 1.0 - 1.0 / (desaturation * (pk - new_peak) + 1.0)
    return tuple((1.0 - g) * v + g * new_peak for v in c)


if __name__ == "__main__":
    # quick self-check / sanity dump
    for nits in (0.0, 0.01, 0.1, 1.0, 18.0, 100.0, 203.0, 1000.0, 4000.0, 10000.0):
        c = pq_encode(nits / PQ_PEAK_NITS)
        back = pq_decode(c) * PQ_PEAK_NITS
        print(f"{nits:>9.2f} nits -> PQ code {c:.6f} ({int(round(c*1023))}/1023) -> {back:>9.2f} nits")
    wcode = linear_bt709_to_hdr10_code10((1.0, 1.0, 1.0))
    print(f"graphics white (linear 1,1,1) -> HDR10 10-bit code {wcode}  (= {GRAPHICS_WHITE_NITS:.0f} nits, BT.2020)")
    pkcode = linear_bt709_to_hdr10_code10((10.0, 10.0, 10.0))
    print(f"scene highlight (linear 10,10,10) -> HDR10 10-bit code {pkcode}  (= {10*GRAPHICS_WHITE_NITS:.0f} nits)")
