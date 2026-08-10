#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors
"""
build_wui_icons.py — SVG icon → TTF → MSDF atlas pipeline for Phase 3e.

Reads monochrome SVG glyphs from assets/_sources/fonts/wui-icons/<name>.svg,
assigns each a PUA codepoint (U+E000+i in deterministic name order), assembles
them into assets/fonts/wui_icons.ttf, writes the matching charset file at
assets/fonts/wui_icons_charset.txt, and invokes the vendored
tools/msdf-atlas-gen build to produce modfiles/fonts/wui_icons.{png,json}.

The PUA codepoint chart is sourced from
docs/wui-authoring-guide.md §5.3 — adding a new SVG requires bumping that
chart so modders see a single canonical name→codepoint mapping.
"""

import os
import re
import sys
import subprocess
from pathlib import Path
from xml.etree import ElementTree as ET

from fontTools.fontBuilder import FontBuilder
from fontTools.pens.ttGlyphPen import TTGlyphPen
from fontTools.pens.cu2quPen import Cu2QuPen
from fontTools.pens.recordingPen import RecordingPen
from fontTools.svgLib.path import parse_path
from fontTools.ttLib import TTFont
from fontTools.misc.transform import DecomposedTransform

REPO_ROOT       = Path(__file__).resolve().parents[2]
SVG_DIR         = REPO_ROOT / "assets/_sources/fonts/wui-icons"
TTF_OUT         = REPO_ROOT / "assets/fonts/wui_icons.ttf"
CHARSET_OUT     = REPO_ROOT / "assets/fonts/wui_icons_charset.txt"
ATLAS_PNG_OUT   = REPO_ROOT / "modfiles/fonts/wui_icons.png"
ATLAS_JSON_OUT  = REPO_ROOT / "modfiles/fonts/wui_icons.json"
MSDF_ATLAS_BIN  = REPO_ROOT / "tools/msdf-atlas-gen/build/bin/msdf-atlas-gen.exe"

# Canonical icon chart — must match docs/wui-authoring-guide.md §5.3.
#
# Single-byte codepoints in the 0xE0-0xE7 range (Latin-1 supplement). The
# Wired engine's MSDF text path (MSDF_NextRenderableChar at
# cl_wired_msdf.c:736) reads one byte at a time via `(unsigned char)*p` and
# has no UTF-8 multibyte decoding; PUA codepoints in the U+E000+ range
# (3-byte UTF-8 sequences) would be read as three independent Latin-1 chars
# and miss the lookup. Using single-byte codepoints keeps the renderer
# untouched while still leaving the icon atlas isolated from text fonts
# (regular text doesn't reference these byte values via $font_icon; other
# fonts don't ship glyphs at these codepoints so cross-atlas confusion is
# nil). The "0xE0+i" pattern preserves the original PUA naming spirit.
CANONICAL_ICONS = [
    ("card_featured",      0xE0),
    ("card_recent",        0xE1),
    ("card_status",        0xE2),
    ("status_connection",  0xE3),
    ("nav_play",           0xE4),
    ("nav_multi",          0xE5),
    ("nav_settings",       0xE6),
    ("nav_quit",           0xE7),
    ("arrow_right",        0xE8),
    # v2 design primitive glyphs (qw-shared.jsx + qw-screens.jsx)
    ("qw_sigil",           0xE9),
    ("rune_0",             0xEA),
    ("rune_1",             0xEB),
    ("rune_2",             0xEC),
    ("rune_3",             0xED),
    ("rune_4",             0xEE),
    ("rune_5",             0xEF),
    ("rune_6",             0xF0),
    ("rune_7",             0xF1),
    ("rune_8",             0xF2),
    ("rune_9",             0xF3),
    ("helmet",             0xF4),
    ("weapon_gnt",         0xF5),
    ("weapon_mg",          0xF6),
    ("weapon_sg",          0xF7),
    ("weapon_gl",          0xF8),
    ("weapon_rl",          0xF9),
    ("weapon_lg",          0xFA),
    ("weapon_rg",          0xFB),
    ("weapon_pg",          0xFC),
    ("weapon_bfg",         0xFD),
    ("diamond",            0xFE),
]

# SVG viewBox is 0..100 with Y growing down. TTF glyphs use Y growing up
# from baseline, units-per-em scale. Use a 1000 upem grid (FontTools default
# advice) and flip Y at conversion time.
UPEM = 1000
SVG_VIEWBOX = 100

# Glyph metrics — square cell, ascent at the top edge, descent below baseline.
ASCENT  = 800
DESCENT = 200   # so total = 1000 = UPEM

def svg_to_ttglyph(svg_path: Path) -> "Glyph":
    """Parse a single SVG file and return a TT glyph.

    Conventions:
      - Single <path> element, single 'd' attribute (the convention enforced
        by docs/wui-authoring-guide.md §5.2). Multiple <path>s are concatenated.
      - viewBox "0 0 100 100" assumed; we scale by UPEM/100 and flip Y.
    """
    tree = ET.parse(svg_path)
    root = tree.getroot()
    ns   = ""
    if root.tag.startswith("{"):
        ns = root.tag.split("}")[0] + "}"

    # Collect all <path d="..."> values; concatenate as one big subpath stream.
    d_attrs = []
    for elem in root.iter(f"{ns}path"):
        d = elem.attrib.get("d", "").strip()
        if d:
            d_attrs.append(d)
    if not d_attrs:
        raise RuntimeError(f"{svg_path.name}: no <path d=...> elements found")
    big_d = " ".join(d_attrs)

    # SVGPath.fromstring uses an internal pen; record into our pen via a
    # transforming pen that flips Y and scales to UPEM.
    pen = TTGlyphPen(None)

    # Y-flip + scale: y' = (SVG_VIEWBOX - y) * (UPEM/SVG_VIEWBOX) + descent_pad
    #                x' = x * (UPEM/SVG_VIEWBOX)
    scale = UPEM / SVG_VIEWBOX
    class TransformPen:
        def __init__(self, target):
            self.t = target
        def moveTo(self, p):  self.t.moveTo((p[0]*scale,  (SVG_VIEWBOX - p[1])*scale))
        def lineTo(self, p):  self.t.lineTo((p[0]*scale,  (SVG_VIEWBOX - p[1])*scale))
        def curveTo(self, *p):
            self.t.curveTo(*[(q[0]*scale, (SVG_VIEWBOX - q[1])*scale) for q in p])
        def qCurveTo(self, *p):
            self.t.qCurveTo(*[(q[0]*scale, (SVG_VIEWBOX - q[1])*scale) for q in p])
        def closePath(self): self.t.closePath()
        def endPath(self):   self.t.endPath()

    # parse_path emits cubics, lines, moves on the supplied pen; wrap in
    # Cu2QuPen so the TT glyph ends up with quadratic curves (truetype
    # `glyf` requirement). The path data is concatenated above so multi-
    # subpath SVGs (e.g. gear: outline + bore) merge into one glyph.
    cu2qu = Cu2QuPen(TransformPen(pen), max_err=1.0)
    parse_path(big_d, cu2qu)
    return pen.glyph()

def build_ttf() -> dict:
    """Build the TTF font from SVG sources. Returns the name→codepoint map."""
    name_to_cp = dict(CANONICAL_ICONS)

    # Verify each canonical icon has an SVG, no extra SVGs lurking.
    svg_files = sorted(SVG_DIR.glob("*.svg"))
    svg_names = {p.stem for p in svg_files}
    expected  = set(name_to_cp.keys())
    if expected != svg_names:
        missing = expected - svg_names
        extra   = svg_names - expected
        raise RuntimeError(
            f"SVG ↔ chart mismatch: missing={sorted(missing)} extra={sorted(extra)}"
        )

    # FontBuilder requires a .notdef as glyph 0.
    glyph_order = [".notdef"] + [name for name, _ in CANONICAL_ICONS]
    fb = FontBuilder(UPEM, isTTF=True)
    fb.setupGlyphOrder(glyph_order)

    # cmap: codepoint → glyph name
    cmap = {cp: name for name, cp in CANONICAL_ICONS}
    fb.setupCharacterMap(cmap)

    # Glyph table: empty .notdef (square box) + each icon
    glyphs = {".notdef": TTGlyphPen(None).glyph()}
    for name, _ in CANONICAL_ICONS:
        glyphs[name] = svg_to_ttglyph(SVG_DIR / f"{name}.svg")
    fb.setupGlyf(glyphs)

    # Horizontal metrics — every glyph gets advance=UPEM, lsb=0 so msdf-
    # atlas-gen lays them out on a square cell.
    metrics = {name: (UPEM, 0) for name in glyph_order}
    fb.setupHorizontalMetrics(metrics)
    fb.setupHorizontalHeader(ascent=ASCENT, descent=-DESCENT)

    fb.setupNameTable({
        "familyName":     "Wired UI Icons",
        "styleName":      "Regular",
        "uniqueFontIdentifier": "wui_icons-1.0",
        "fullName":       "Wired UI Icons",
        "psName":         "WiredUIIcons-Regular",
        "version":        "Version 1.0",
    })
    fb.setupOS2(sTypoAscender=ASCENT, sTypoDescender=-DESCENT,
                usWinAscent=ASCENT,    usWinDescent=DESCENT)
    fb.setupPost()

    TTF_OUT.parent.mkdir(parents=True, exist_ok=True)
    fb.save(str(TTF_OUT))

    # Charset: one codepoint per line as \uHHHH for msdf-atlas-gen.
    CHARSET_OUT.parent.mkdir(parents=True, exist_ok=True)
    with CHARSET_OUT.open("w", encoding="utf-8") as fp:
        for _, cp in CANONICAL_ICONS:
            fp.write(f"0x{cp:04X};\n")
    return name_to_cp

def run_msdf_atlas_gen():
    if not MSDF_ATLAS_BIN.exists():
        raise RuntimeError(
            f"msdf-atlas-gen not built — expected at {MSDF_ATLAS_BIN}\n"
            "Run: cd tools/msdf-atlas-gen && mkdir -p build && cd build && "
            "cmake -G \"MinGW Makefiles\" -DCMAKE_BUILD_TYPE=Release .. && "
            "mingw32-make -j$(nproc)"
        )
    ATLAS_PNG_OUT.parent.mkdir(parents=True, exist_ok=True)
    cmd = [
        str(MSDF_ATLAS_BIN),
        "-font",      str(TTF_OUT),
        "-charset",   str(CHARSET_OUT),
        "-type",      "msdf",
        "-format",    "png",
        "-size",      "72",
        "-pxrange",   "8",
        "-dimensions","512", "512",
        "-imageout",  str(ATLAS_PNG_OUT),
        "-json",      str(ATLAS_JSON_OUT),
    ]
    print("==> msdf-atlas-gen", " ".join(cmd[1:]))
    subprocess.run(cmd, check=True)
    print(f"==> Generated {ATLAS_PNG_OUT.name} + {ATLAS_JSON_OUT.name}")

def main():
    print("==> build_wui_icons: assembling TTF from SVGs")
    name_to_cp = build_ttf()
    for name, cp in CANONICAL_ICONS:
        print(f"    U+{cp:04X}  {name}")
    print(f"==> {TTF_OUT}")
    print(f"==> {CHARSET_OUT}")
    print("==> Generating MSDF atlas")
    run_msdf_atlas_gen()

if __name__ == "__main__":
    main()
