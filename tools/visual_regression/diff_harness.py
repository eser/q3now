#!/usr/bin/env python3
"""diff_harness.py — compare expected vs actual byte colours, per region.

Given a list of regions, each with an expected byte RGBA (from
expected_calc) and an actual byte RGBA (from a swapchain readback), flag
any channel whose absolute delta exceeds the region's tolerance.

Tolerance resolution order (per region):
  1. explicit per-region entry in tolerance.yaml `regions:`
  2. `shoulder` default if the region is tagged shoulder-prone
  3. `default`

CLI:
    python diff_harness.py --expected exp.json --actual act.json [--tolerance tolerance.yaml] [--json]
    python diff_harness.py --selftest

The JSON inputs are {region_name: [r,g,b,a]} maps. Exit code 0 if every
region passes, 1 otherwise (CI-friendly).
"""

from __future__ import annotations

import argparse
import json
import os
import sys

_THIS_DIR = os.path.dirname(os.path.abspath(__file__))
DEFAULT_TOLERANCE = os.path.join(_THIS_DIR, "tolerance.yaml")

_DEFAULT_TOL = 5
_DEFAULT_SHOULDER_TOL = 15


def load_tolerance(path=None):
    """Return {'default': int, 'shoulder': int, 'regions': {name: int}}.
    Falls back to built-in defaults if PyYAML or the file is missing."""
    cfg = {"default": _DEFAULT_TOL, "shoulder": _DEFAULT_SHOULDER_TOL,
           "regions": {}, "shoulder_regions": []}
    path = path or DEFAULT_TOLERANCE
    if not path or not os.path.isfile(path):
        return cfg
    try:
        import yaml  # type: ignore
    except ImportError:
        sys.stderr.write("diff_harness: PyYAML not available; using built-in tolerance defaults\n")
        return cfg
    with open(path, "r", encoding="utf-8") as fh:
        data = yaml.safe_load(fh) or {}
    if "default" in data:
        cfg["default"] = int(data["default"])
    if "shoulder" in data:
        cfg["shoulder"] = int(data["shoulder"])
    if isinstance(data.get("regions"), dict):
        cfg["regions"] = {str(k): int(v) for k, v in data["regions"].items()}
    if isinstance(data.get("shoulder_regions"), list):
        cfg["shoulder_regions"] = [str(x) for x in data["shoulder_regions"]]
    return cfg


def tolerance_for(region, cfg, *, override=None, shoulder=False):
    if override is not None:
        return int(override)
    if region in cfg.get("regions", {}):
        return cfg["regions"][region]
    if shoulder or region in cfg.get("shoulder_regions", []):
        return cfg.get("shoulder", _DEFAULT_SHOULDER_TOL)
    return cfg.get("default", _DEFAULT_TOL)


def _as_rgba(v):
    v = list(v)
    if len(v) == 3:
        v = v + [255]
    return [int(round(x)) for x in v[:4]]


def diff_region(region, expected, actual, tol):
    e = _as_rgba(expected)
    a = _as_rgba(actual)
    delta = [abs(e[i] - a[i]) for i in range(4)]
    worst = max(delta)
    return {
        "region": region,
        "expected": e,
        "actual": a,
        "delta": delta,
        "worst": worst,
        "tolerance": int(tol),
        "pass": worst <= tol,
    }


def run_diff(regions, cfg=None, *, per_region_overrides=None, shoulder_set=None):
    """`regions` is a list of dicts: {region, expected:[..], actual:[..]}.
    Returns (overall_ok: bool, results: list)."""
    cfg = cfg or load_tolerance()
    per_region_overrides = per_region_overrides or {}
    shoulder_set = set(shoulder_set or ())
    results = []
    for r in regions:
        name = r["region"]
        tol = tolerance_for(name, cfg, override=per_region_overrides.get(name),
                            shoulder=name in shoulder_set)
        results.append(diff_region(name, r["expected"], r["actual"], tol))
    overall_ok = all(x["pass"] for x in results)
    return overall_ok, results


def format_table(results, *, scene=None):
    lines = []
    if scene:
        lines.append(f"=== scene: {scene} ===")
    hdr = f"{'region':28s} {'expected (rgba)':>20s} {'actual (rgba)':>20s} {'delta':>16s} {'tol':>4s}  result"
    lines.append(hdr)
    lines.append("-" * len(hdr))
    for r in results:
        verdict = "PASS" if r["pass"] else "FAIL"
        lines.append(
            f"{r['region']:28.28s} {str(r['expected']):>20s} {str(r['actual']):>20s} "
            f"{str(r['delta']):>16s} {r['tolerance']:>4d}  {verdict}"
        )
    n_fail = sum(1 for r in results if not r["pass"])
    lines.append("-" * len(hdr))
    lines.append(f"{len(results)} region(s): {len(results) - n_fail} pass, {n_fail} fail")
    return "\n".join(lines)


# ── self-test ──────────────────────────────────────────────────────────


def _selftest() -> int:
    regions = [
        {"region": "exact_match", "expected": [20, 20, 31, 235], "actual": [20, 20, 31, 235]},
        {"region": "within_tol", "expected": [128, 128, 128, 255], "actual": [131, 126, 130, 255]},
        {"region": "out_of_tol", "expected": [20, 20, 31, 235], "actual": [1, 1, 3, 235]},
        {"region": "shoulder_loose", "expected": [200, 200, 200, 255], "actual": [212, 190, 205, 255]},
    ]
    cfg = {"default": 5, "shoulder": 15, "regions": {}, "shoulder_regions": ["shoulder_loose"]}
    ok, results = run_diff(regions, cfg)
    by = {r["region"]: r for r in results}
    fails = []
    if not by["exact_match"]["pass"]:
        fails.append("exact_match should pass")
    if not by["within_tol"]["pass"]:
        fails.append("within_tol should pass (max delta 3 <= 5)")
    if by["out_of_tol"]["pass"]:
        fails.append("out_of_tol should FAIL (delta 19/19/28 > 5)")
    if not by["shoulder_loose"]["pass"]:
        fails.append("shoulder_loose should pass (max delta 12 <= 15)")
    if ok:
        fails.append("overall should be False (out_of_tol failed)")

    # tolerance_for resolution order
    if tolerance_for("x", cfg) != 5:
        fails.append("default tolerance lookup")
    if tolerance_for("shoulder_loose", cfg) != 15:
        fails.append("shoulder tolerance lookup")
    cfg2 = dict(cfg, regions={"x": 8})
    if tolerance_for("x", cfg2) != 8:
        fails.append("explicit per-region tolerance lookup")
    if tolerance_for("x", cfg2, override=2) != 2:
        fails.append("override tolerance lookup")

    if fails:
        for f in fails:
            print("FAIL:", f, file=sys.stderr)
        print(format_table(results), file=sys.stderr)
        print(f"\n{len(fails)} self-test failure(s).", file=sys.stderr)
        return 1
    print("diff_harness.py self-test: OK")
    print(format_table(results))
    return 0


# ── CLI ────────────────────────────────────────────────────────────────


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description="Compare expected vs actual byte colours per region.")
    ap.add_argument("--expected", help="JSON file: {region: [r,g,b,a]}")
    ap.add_argument("--actual", help="JSON file: {region: [r,g,b,a]}")
    ap.add_argument("--tolerance", default=DEFAULT_TOLERANCE, help="tolerance.yaml path")
    ap.add_argument("--json", action="store_true", help="emit JSON results")
    ap.add_argument("--selftest", action="store_true", help="run built-in checks and exit")
    args = ap.parse_args(argv)

    if args.selftest:
        return _selftest()
    if not args.expected or not args.actual:
        ap.error("--expected and --actual are required (or use --selftest)")

    with open(args.expected, "r", encoding="utf-8") as fh:
        expected = json.load(fh)
    with open(args.actual, "r", encoding="utf-8") as fh:
        actual = json.load(fh)

    cfg = load_tolerance(args.tolerance)
    regions = []
    for name, exp in expected.items():
        if name not in actual:
            sys.stderr.write(f"diff_harness: region {name!r} missing from --actual; skipping\n")
            continue
        regions.append({"region": name, "expected": exp, "actual": actual[name]})

    ok, results = run_diff(regions, cfg)
    if args.json:
        print(json.dumps({"ok": ok, "results": results}, indent=2))
    else:
        print(format_table(results))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
