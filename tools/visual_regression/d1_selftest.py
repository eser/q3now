#!/usr/bin/env python3
"""d1_selftest.py — prove the RenderDoc visual-regression pipeline works
end-to-end AND can catch a wrong render (Dimension 1 of the visual-verify
infrastructure audit).

It drives the REAL pipeline modules in sequence:
  1. capture_driver.run_capture()  → launches wired.x64 under renderdoccmd,
     fires the in-app WN_RDOC trigger, writes a .rdc
  2. readback.read_pixels()        → opens the .rdc via qrenderdoc --python,
     picks the presented swapchain image, reads centre + sample pixels
  3. diff_harness.run_diff()        → the verdict module

The decisive teeth-test: with the SAME captured pixels we run the verdict
twice —
  * CLEAN  : expected == actual            → must PASS  (pipeline agrees)
  * DEFECT : expected = actual XOR a big delta on every channel → must FAIL
             (a deliberately wrong "expected render" the pipeline must reject)

If the clean run PASSes and the defect run FAILs, the pipeline can both verify
a correct frame and catch a wrong one — it is not a rubber stamp.

Usage:
    python tools/visual_regression/d1_selftest.py [--map arena1]
Exit: 0 PASS (pipeline works + has teeth)   1 FAIL   77 SKIP (no tools)
"""

from __future__ import annotations

import argparse
import os
import sys

_THIS_DIR = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, _THIS_DIR)

import capture_driver  # noqa: E402
import diff_harness    # noqa: E402
import readback        # noqa: E402


# Fixed sample points (well inside a 1280x720 frame; centre + 4 inboard taps).
_SAMPLES = [(640, 360), (320, 180), (960, 180), (320, 540), (960, 540)]


def _perturb(byte_rgba):
    """A deliberately-wrong 'expected' value: shove every colour channel far
    from the actual (clamped to [0,255]); keep alpha. Guarantees a delta well
    over any sane tolerance, so a working verdict MUST flag it."""
    r, g, b, a = byte_rgba
    def flip(c):
        return 0 if c >= 128 else 255   # push to the opposite extreme
    return [flip(r), flip(g), flip(b), a]


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description="D1 RenderDoc-pipeline end-to-end + teeth self-test.")
    ap.add_argument("--map", default="arena1", help="map to capture (default arena1)")
    ap.add_argument("--binary", default=None)
    ap.add_argument("--timeout", type=int, default=150)
    args = ap.parse_args(argv)

    print("==> D1 RenderDoc pipeline end-to-end + teeth self-test")

    # ── 1. capture ──
    scene_cfg = {"name": "d1_selftest", "map": args.map}
    try:
        cap = capture_driver.run_capture(scene_cfg, binary=args.binary,
                                         timeout=args.timeout, verbose=True)
    except RuntimeError as e:
        print(f"SKIP: capture tooling unavailable: {e}")
        return 77
    rdc = cap.get("rdc")
    print(f"  capture: rdc={rdc}  layoutdump={cap.get('layoutdump')}  "
          f"rc={cap.get('returncode')}  elapsed={cap.get('elapsed_s', 0):.1f}s")
    if not rdc or not os.path.isfile(rdc):
        print("  FAIL: no .rdc produced — capture step broken "
              "(renderdoccmd args / WN_RDOC trigger / engine launch).")
        return 1

    # ── 2. readback ──
    try:
        rb = readback.read_pixels(rdc, _SAMPLES, texture="backbuffer",
                                  timeout=max(args.timeout, 120))
    except RuntimeError as e:
        print(f"  FAIL: readback step broke: {e}")
        return 1
    if not rb.get("ok"):
        print(f"  FAIL: readback returned not-ok: {rb.get('error')}")
        return 1
    tex = rb.get("texture") or {}
    print(f"  readback texture: {tex.get('name')} ({tex.get('width')}x{tex.get('height')})")
    actual = {}
    for i, pix in enumerate(rb["pixels"]):
        b = pix.get("byte")
        if b is None:
            print(f"  FAIL: pixel {i} {_SAMPLES[i]} had no byte value: {pix}")
            return 1
        actual[f"s{i}"] = b
        print(f"    s{i} {_SAMPLES[i]}: byte={b}")

    # A frame where every sampled pixel is pure black would make the teeth-test
    # vacuous (flip(0)=255 still differs, so it still works — but a non-black
    # frame is stronger evidence the capture is real). Note it, don't fail.
    nonblack = sum(1 for b in actual.values() if sum(b[:3]) > 12)
    print(f"  non-black samples: {nonblack}/{len(actual)} "
          f"({'real rendered content' if nonblack else 'all-dark frame — teeth still valid'})")

    cfg = diff_harness.load_tolerance()

    # ── 3a. CLEAN verdict: expected == actual → must PASS ──
    clean_regions = [{"region": r, "expected": actual[r], "actual": actual[r]} for r in actual]
    clean_ok, _ = diff_harness.run_diff(clean_regions, cfg)
    print(f"  CLEAN verdict (expected==actual): {'PASS' if clean_ok else 'FAIL'}")

    # ── 3b. DEFECT verdict: expected = perturbed actual → must FAIL ──
    defect_regions = [{"region": r, "expected": _perturb(actual[r]), "actual": actual[r]} for r in actual]
    defect_ok, defect_results = diff_harness.run_diff(defect_regions, cfg)
    print(f"  DEFECT verdict (expected=opposite of actual): {'PASS' if defect_ok else 'FAIL'}")
    # show one failing region as evidence
    for res in defect_results:
        if not res.get("pass", True):
            print(f"    e.g. {res.get('region')}: expected={res.get('expected')} "
                  f"actual={res.get('actual')} delta={res.get('delta')} tol={res.get('tolerance')}")
            break

    # ── verdict ──
    if clean_ok and not defect_ok:
        print("==> D1 PASS: pipeline runs end-to-end (capture→readback→verdict) AND "
              "catches a wrong render (clean PASS, defect FAIL) — it has teeth.")
        return 0
    print(f"==> D1 FAIL: clean_ok={clean_ok} (want True), defect_ok={defect_ok} (want False) "
          "— the pipeline cannot reliably distinguish correct from wrong.")
    return 1


if __name__ == "__main__":
    sys.exit(main())
