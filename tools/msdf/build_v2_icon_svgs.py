#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors
"""
build_v2_icon_svgs.py — emit 21 v2 design SVG glyphs into
assets/_sources/fonts/wui-icons/ as filled silhouettes (the MSDF atlas
pipeline produces a single signed-distance silhouette per glyph; strokes
and opacity layers from the v2 JSX cannot survive that conversion, so we
render each primitive as a filled outline).

Source of truth: docs/wui-claude-design-canonical-v2/project/qw-shared.jsx
(QWSigil, Rune, PlayerBadge helmet) + qw-screens.jsx (WeaponIcon × 9).
Coordinates rescaled to a 0..100 viewBox to match the existing
build_wui_icons.py expectation.

Re-run after editing this script (it is idempotent — overwrites only the
21 v2 SVG files; existing 0xE0-0xE8 icons untouched).
"""

from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
SVG_DIR   = REPO_ROOT / "assets/_sources/fonts/wui-icons"

SVG_HEADER = (
    '<?xml version="1.0" encoding="UTF-8"?>\n'
    '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 100 100">\n'
    '  <!-- {comment} -->\n'
    '  <path d="{d}" fill="black" fill-rule="evenodd"/>\n'
    '</svg>\n'
)

def emit(name: str, comment: str, d: str) -> None:
    path = SVG_DIR / f"{name}.svg"
    path.write_text(SVG_HEADER.format(comment=comment, d=d), encoding="utf-8")
    print(f"  + {path.relative_to(REPO_ROOT)}")

# ──────────────────────────────────────────────────────────────────────
# QWSigil — outer ring (octagon approx) + Q diamond wedge + W chevrons.
# Source: qw-shared.jsx QWSigil (64-unit viewBox; rescaled ×1.5625 to 100).
# Pure stroke geometry collapsed to filled ribbon shapes via inner+outer
# octagon (evenodd) and chevron strip.
# ──────────────────────────────────────────────────────────────────────

def qw_sigil() -> None:
    # Outer ring ribbon: outer octagon (r≈47) minus inner octagon (r≈40).
    def octa(r: float, cx: float = 50, cy: float = 50) -> str:
        import math
        pts = []
        for i in range(8):
            a = (i / 8.0) * 2 * math.pi - math.pi / 2
            pts.append((cx + r * math.cos(a), cy + r * math.sin(a)))
        return "M " + " L ".join(f"{x:.2f} {y:.2f}" for x, y in pts) + " Z"

    ring_outer = octa(47)
    ring_inner = octa(40)
    # Q diamond wedge ribbon (outer diamond minus inner diamond).
    q_outer = "M 50 24 L 76 50 L 50 76 L 24 50 Z"
    q_inner = "M 50 32 L 68 50 L 50 68 L 32 50 Z"
    # W chevrons as a triangular strip (filled).
    w_chev = ("M 31 46 L 38 46 L 41 60 L 50 50 L 59 60 L 62 46 L 69 46 "
              "L 65 64 L 56 64 L 50 58 L 44 64 L 35 64 Z")
    # Center dot.
    dot = "M 47 47 L 53 47 L 53 53 L 47 53 Z"
    d = " ".join([ring_outer, ring_inner, q_outer, q_inner, w_chev, dot])
    emit("qw_sigil", "QWSigil (U+E9): ring + Q wedge + W chevrons + dot", d)

# ──────────────────────────────────────────────────────────────────────
# Rune × 10 — qw-shared.jsx Rune[i] stroke paths converted to filled
# silhouettes by widening each stroke to a 2-unit ribbon (input viewBox
# was 16; rescaled ×6.25 yields 100-unit, with strokes ~12 units wide).
# Glyphs read as solid geometric runes rather than thin outlines.
# ──────────────────────────────────────────────────────────────────────

RUNES = [
    # 0 — triangle
    ("rune_0", "Rune 0 — triangle",
     "M 12 12 L 88 12 L 50 88 Z"),
    # 1 — diamond
    ("rune_1", "Rune 1 — diamond",
     "M 12 50 L 50 12 L 88 50 L 50 88 Z"),
    # 2 — three vertical bars
    ("rune_2", "Rune 2 — three bars",
     "M 10 10 L 22 10 L 22 90 L 10 90 Z "
     "M 44 10 L 56 10 L 56 90 L 44 90 Z "
     "M 78 10 L 90 10 L 90 90 L 78 90 Z"),
    # 3 — divided square (square + center vertical bar)
    ("rune_3", "Rune 3 — divided square",
     "M 14 14 L 86 14 L 86 86 L 14 86 Z "
     "M 22 22 L 46 22 L 46 78 L 22 78 Z "
     "M 54 22 L 78 22 L 78 78 L 54 78 Z"),
    # 4 — central cross with double horizontals
    ("rune_4", "Rune 4 — cross-and-bars",
     "M 44 10 L 56 10 L 56 90 L 44 90 Z "
     "M 10 28 L 90 28 L 90 38 L 10 38 Z "
     "M 18 64 L 82 64 L 82 74 L 18 74 Z"),
    # 5 — H-cross (two horizontals + central vertical)
    ("rune_5", "Rune 5 — H-cross",
     "M 10 22 L 90 22 L 90 32 L 10 32 Z "
     "M 10 68 L 90 68 L 90 78 L 10 78 Z "
     "M 44 10 L 56 10 L 56 90 L 44 90 Z"),
    # 6 — X (two crossed bars)
    ("rune_6", "Rune 6 — X",
     "M 14 22 L 22 14 L 86 78 L 78 86 Z "
     "M 78 14 L 86 22 L 22 86 L 14 78 Z"),
    # 7 — H letter (two verticals + horizontal)
    ("rune_7", "Rune 7 — H",
     "M 18 10 L 30 10 L 30 90 L 18 90 Z "
     "M 70 10 L 82 10 L 82 90 L 70 90 Z "
     "M 24 44 L 76 44 L 76 56 L 24 56 Z"),
    # 8 — peaked triangle with crossbar
    ("rune_8", "Rune 8 — peaked-triangle-bar",
     "M 12 88 L 50 12 L 88 88 Z "
     "M 30 60 L 70 60 L 70 70 L 30 70 Z"),
    # 9 — bowtie (two triangles)
    ("rune_9", "Rune 9 — bowtie",
     "M 12 12 L 88 12 L 50 50 Z "
     "M 12 88 L 50 50 L 88 88 Z"),
]

# ──────────────────────────────────────────────────────────────────────
# helmet (PlayerBadge avatar) — qw-shared.jsx PlayerBadge (60-unit
# viewBox; rescaled ×1.667 to 100). Composite: head silhouette + visor
# slit cutout + jaw plate edge + crown ridge.
# ──────────────────────────────────────────────────────────────────────

HELMET = (
    "helmet", "Helmet (U+F4): avatar silhouette + visor + jaw + crown",
    # head outer
    "M 20 40 Q 20 17 50 17 Q 80 17 80 40 L 80 70 Q 80 83 70 87 "
    "L 63 83 L 63 93 L 37 93 L 37 83 L 30 87 Q 20 83 20 70 Z "
    # visor slit cutout (evenodd hole)
    "M 27 43 L 73 43 L 73 53 L 27 53 Z "
    # jaw plate (dark accent across mouth)
    "M 33 67 L 67 67 L 67 80 L 33 80 Z "
    # crown ridge cutout
    "M 36 20 L 50 13 L 64 20 L 60 23 L 50 18 L 40 23 Z"
)

# ──────────────────────────────────────────────────────────────────────
# WeaponIcon × 9 — qw-screens.jsx WeaponIcon (24-unit viewBox; rescaled
# ×4.167 to 100). Each weapon is a simplified filled silhouette of the
# corresponding Quake 3 weapon (the original SVGs used mixed stroke +
# fill; here we collapse to a single recognizable silhouette).
# ──────────────────────────────────────────────────────────────────────

WEAPONS = [
    ("weapon_gnt", "Weapon gnt — gauntlet (claw)",
     "M 25 58 L 25 92 L 58 92 L 58 75 L 75 75 L 75 58 Z "
     "M 58 58 L 58 33 L 71 33 L 71 21 L 83 21 L 83 33 L 92 33 L 92 50 Z"),
    ("weapon_mg", "Weapon mg — machinegun (barrel + grip)",
     "M 12 54 L 67 54 L 67 46 L 88 46 L 88 58 L 67 58 L 67 67 L 50 67 "
     "L 50 79 L 38 79 L 38 67 L 12 67 Z "
     "M 75 38 L 79 38 L 79 46 L 75 46 Z "
     "M 83 38 L 88 38 L 88 46 L 83 46 Z"),
    ("weapon_sg", "Weapon sg — shotgun (twin barrel)",
     "M 12 46 L 71 46 L 71 58 L 12 58 Z "
     "M 12 58 L 71 58 L 71 71 L 12 71 Z "
     "M 71 46 L 92 46 L 92 71 L 71 71 Z "
     "M 33 71 L 33 88 L 46 88 L 46 71 Z"),
    ("weapon_gl", "Weapon gl — grenade launcher (drum + barrel)",
     "M 17 38 L 58 38 L 58 58 L 17 58 Z "
     "M 58 46 L 92 46 L 92 67 L 58 67 Z "
     "M 33 21 L 33 38 L 46 38 L 46 21 Z"),
    ("weapon_rl", "Weapon rl — rocket launcher (cylinder + fins)",
     "M 12 42 L 58 42 L 67 33 L 92 33 L 92 67 L 67 67 L 58 58 L 12 58 Z "
     "M 29 58 L 29 75 L 42 75 L 42 58 Z"),
    ("weapon_lg", "Weapon lg — lightning gun (Z bolt body)",
     "M 17 50 L 38 50 L 50 33 L 62 71 L 75 33 L 88 50 L 88 58 L 75 58 "
     "L 62 88 L 50 50 L 38 67 L 17 67 Z"),
    ("weapon_rg", "Weapon rg — railgun (long barrel + scope)",
     "M 12 50 L 75 50 L 92 42 L 92 62 L 75 62 L 12 62 Z "
     "M 29 62 L 29 79 L 42 79 L 42 62 Z "
     "M 50 38 L 67 38 L 67 50 L 50 50 Z"),
    ("weapon_pg", "Weapon pg — plasma gun (body + orb)",
     "M 12 46 L 58 46 L 58 67 L 50 67 L 50 79 L 38 79 L 38 67 L 12 67 Z "
     "M 67 38 L 88 38 L 92 50 L 88 62 L 67 62 L 63 50 Z"),
    ("weapon_bfg", "Weapon bfg — BFG (chamber + barrel + glow)",
     "M 17 33 L 71 33 L 79 25 L 92 25 L 92 71 L 79 71 L 71 63 L 17 63 Z "
     "M 29 63 L 29 83 L 50 83 L 50 63 Z "
     "M 38 42 L 50 42 L 50 54 L 38 54 Z"),
]

def main():
    SVG_DIR.mkdir(parents=True, exist_ok=True)
    print("==> build_v2_icon_svgs: emitting 22 v2 design SVGs")
    qw_sigil()
    for name, comment, d in RUNES:
        emit(name, comment, d)
    emit(HELMET[0], HELMET[1], HELMET[2])
    for name, comment, d in WEAPONS:
        emit(name, comment, d)
    # diamond — v2 uses U+25C6 BLACK DIAMOND for "*" bullet decoration in
    # MMC headers ("* LAST MATCH", "* SIGNED IN", etc.). Engine's MSDF
    # path is single-byte so the multi-byte UTF-8 sequence is unreachable;
    # we ship the diamond as a wui_icons atlas glyph at a PUA-style slot.
    emit("diamond", "diamond (U+FE bullet substitute for v2 markers)",
         "M 50 12 L 88 50 L 50 88 L 12 50 Z")
    print("==> done")

if __name__ == "__main__":
    main()
