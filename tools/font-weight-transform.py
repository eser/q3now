#!/usr/bin/env python3
"""
Transform font weight using FontForge's changeWeight algorithm.
Usage: python3 font-weight-transform.py <input.ttf> <output.ttf> <weight_delta>
  weight_delta: negative = thinner (e.g. -30), positive = thicker (e.g. +20)
"""
import fontforge
import sys

if len(sys.argv) != 4:
    print(f"Usage: {sys.argv[0]} <input.ttf> <output.ttf> <weight_delta>")
    sys.exit(1)

input_ttf = sys.argv[1]
output_ttf = sys.argv[2]
weight_delta = int(sys.argv[3])

font = fontforge.open(input_ttf)
font.selection.all()
font.changeWeight(weight_delta, "auto", 0, 0, "auto")
font.simplify()
font.round()
font.removeOverlap()
font.autoHint()
font.simplify()
font.generate(output_ttf)
font.close()
print(f"Generated {output_ttf} with weight delta {weight_delta}")
