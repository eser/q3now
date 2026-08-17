#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026-present Wired Engine contributors
"""Read the menu-background arms out of qconsole.jsonl and report per-arm cost.

Consumes the SAME authorities fps-perf-analyze.py does, for the same reasons:

  com_perfTrace ("frame trace (200f)")  -- the CPU authority. Full-population:
      every frame in the window is counted, so unlike com_speeds' thresholded
      rows it cannot be biased by only sampling slow frames.
  r_gpuSpeeds   ("gpu (Nf avg, ms)")    -- GPU per-pass breakdown and total.
  vk perf v2                            -- host-side pacing plus the draw count.

Each arm is delimited by MENUBG_ARM_BEGIN_<name> / MENUBG_ARM_END_<name> echo
markers, and only blocks fully contained between them count -- a block that
straddles a boundary carries samples from two different backdrop states and is
dropped rather than attributed to either.

Reported per arm: median and full spread across the contained buckets, never a
single sample.
"""

import argparse
import json
import re
import statistics
import sys

RE_ARM_BEGIN = re.compile(r"^MENUBG_ARM_BEGIN_([A-Z0-9_]+)$")
RE_ARM_END = re.compile(r"^MENUBG_ARM_END_([A-Z0-9_]+)$")
RE_FRAME_TRACE = re.compile(
    r"^frame trace \((\d+)f\): bucket=(\d+) count=(\d+) valid=(\d+)"
    r" cpu_work_total=(\d+)us scr_end_total=(\d+)us$"
)
RE_GPU_HDR = re.compile(r"^gpu \((\d+)f avg, ms\):")
RE_GPU_LABEL = re.compile(r"^\s*([A-Za-z_]+)=([\d.]+)")
RE_GPU_TOTAL = re.compile(r"^\s*total=([\d.]+)")
RE_VK_PERF_V2 = re.compile(r"^vk perf v2: (.+)$")
RE_EXTENT = re.compile(r"^\s*extent=(\d+)x(\d+)")

ARMS = ("BG_ON", "BG_OFF", "BG_ON2")


def load(path):
    rows = []
    with open(path, "r", encoding="utf-8", errors="replace") as handle:
        for line in handle:
            line = line.strip()
            if not line:
                continue
            try:
                obj = json.loads(line)
            except json.JSONDecodeError:
                continue
            if isinstance(obj, dict):
                rows.append(obj)
    return rows


def parse(rows):
    """Split the log into arms and collect each arm's contained blocks."""
    arms = {name: {"cpu": [], "scr": [], "gpu": [], "gpu_passes": [], "draws": []}
            for name in ARMS}
    current = None
    gpu_open = None  # (label->ms dict) while inside a gpu block
    extent = None
    failures = []

    for row in rows:
        msg = str(row.get("msg", "")).rstrip("\n")

        begin = RE_ARM_BEGIN.fullmatch(msg)
        if begin:
            name = begin.group(1)
            if name not in arms:
                failures.append(f"unknown arm marker {name}")
            elif current is not None:
                failures.append(f"arm {name} began while {current} still open")
            else:
                current = name
            gpu_open = None
            continue

        end = RE_ARM_END.fullmatch(msg)
        if end:
            name = end.group(1)
            if current != name:
                failures.append(f"arm end {name} does not match open arm {current}")
            current = None
            gpu_open = None
            continue

        if RE_EXTENT.match(msg) and extent is None:
            m = RE_EXTENT.match(msg)
            extent = (int(m.group(1)), int(m.group(2)))

        # ---- GPU block: header, then labels, then total ----
        if RE_GPU_HDR.match(msg):
            gpu_open = {}
            continue
        if gpu_open is not None:
            total = RE_GPU_TOTAL.match(msg)
            if total:
                if current:
                    arms[current]["gpu"].append(float(total.group(1)))
                    arms[current]["gpu_passes"].append(gpu_open)
                gpu_open = None
                continue
            label = RE_GPU_LABEL.match(msg)
            if label:
                gpu_open[label.group(1)] = float(label.group(2))
                continue

        trace = RE_FRAME_TRACE.fullmatch(msg)
        if trace:
            frames, _bucket, count, valid, cpu_us, scr_us = (int(g) for g in trace.groups())
            if count != valid:
                failures.append(f"frame trace count={count} != valid={valid}")
            elif current:
                arms[current]["cpu"].append(cpu_us / count)
                arms[current]["scr"].append(scr_us / count)
            continue

        v2 = RE_VK_PERF_V2.fullmatch(msg)
        if v2 and current:
            fields = {}
            for token in v2.group(1).split():
                if "=" in token:
                    key, _, value = token.partition("=")
                    fields[key] = value
            draws = fields.get("draws_total")
            count = fields.get("count")
            if draws and count and count.isdigit() and int(count) > 0 and draws.isdigit():
                arms[current]["draws"].append(int(draws) / int(count))
            continue

    if current is not None:
        failures.append(f"arm {current} never closed")
    return arms, extent, failures


def stats(values):
    if not values:
        return None
    return {
        "n": len(values),
        "median": statistics.median(values),
        "min": min(values),
        "max": max(values),
        "spread_pct": (max(values) - min(values)) / statistics.median(values) * 100.0
        if statistics.median(values)
        else 0.0,
    }


def fmt(label, unit, s):
    if s is None:
        return f"    {label:<22} (no samples)"
    return (
        f"    {label:<22} median {s['median']:8.3f} {unit}"
        f"   min {s['min']:8.3f}  max {s['max']:8.3f}"
        f"   n={s['n']}  spread {s['spread_pct']:5.1f}%"
    )


def analyze(rows, verbose=True, warmup_blocks=1):
    """warmup_blocks: leading blocks dropped per arm.

    The first block after an arm boundary is not steady state. The menu
    transition that selected this arm is still settling (Clay relayout, the
    anim pool re-arming its loop timers, first-touch of the backdrop's glyph
    and token lookups), and on the GPU side the first block also carries
    pipeline warm-up. Averaging that into the arm reports transition cost as
    if it were per-frame cost. The fps gate drops its first GPU block for the
    same reason; here it is dropped on both authorities because the CPU side
    has a transition too.
    """
    arms, extent, failures = parse(rows)

    if verbose:
        print(f"  render extent: {extent[0]}x{extent[1]}" if extent else "  render extent: unknown")
        print(f"  warm-up policy: first {warmup_blocks} block(s) per arm discarded")

    def steady(values):
        return values[warmup_blocks:] if len(values) > warmup_blocks else []

    summary = {}
    for name in ARMS:
        data = arms[name]
        summary[name] = {
            "cpu": stats(steady(data["cpu"])),
            "scr": stats(steady(data["scr"])),
            "gpu": stats(steady(data["gpu"])),
            "draws": stats(steady(data["draws"])),
            "gpu_passes": data["gpu_passes"],
            "cpu_raw": data["cpu"],
        }
        if verbose:
            print(f"\n  [{name}]")
            print(fmt("CPU work / frame", "us", summary[name]["cpu"]))
            print(fmt("SCR_UpdateScreen /f", "us", summary[name]["scr"]))
            print(fmt("GPU total / frame", "ms", summary[name]["gpu"]))
            print(fmt("draws / frame", "", summary[name]["draws"]))

    # Every arm must have produced at least one contained CPU block, or the
    # comparison below is between a measurement and nothing.
    for name in ARMS:
        if not summary[name]["cpu"]:
            failures.append(
                f"arm {name} produced no steady-state frame-trace block"
                f" (raw blocks: {len(summary[name]['cpu_raw'])},"
                f" need > {warmup_blocks})"
            )

    if verbose and not failures:
        print("\n  ── backdrop cost (BG_ON minus BG_OFF) ──")
        for key, unit in (("cpu", "us"), ("scr", "us"), ("gpu", "ms")):
            a, b = summary["BG_ON"][key], summary["BG_OFF"][key]
            if a and b:
                d = a["median"] - b["median"]
                pct = (d / b["median"] * 100.0) if b["median"] else 0.0
                print(f"    {key:<4} {d:+9.3f} {unit}  ({pct:+6.1f}%)")

        print("\n  ── drift control (BG_ON2 minus BG_ON, same state) ──")
        for key, unit in (("cpu", "us"), ("gpu", "ms")):
            a, b = summary["BG_ON2"][key], summary["BG_ON"][key]
            if a and b:
                d = a["median"] - b["median"]
                print(f"    {key:<4} {d:+9.3f} {unit}")

        # If repeating the SAME state moved more than the two states differ,
        # the run measured the machine, not the backdrop. Say so rather than
        # letting the delta above be read as a backdrop result.
        for key, unit in (("cpu", "us"), ("gpu", "ms")):
            on, off, on2 = (summary[a][key] for a in ("BG_ON", "BG_OFF", "BG_ON2"))
            if on and off and on2:
                effect = abs(on["median"] - off["median"])
                drift = abs(on2["median"] - on["median"])
                verdict = "usable" if drift < effect else "DRIFT EXCEEDS EFFECT"
                print(f"    {key}: effect {effect:.3f} {unit} vs drift"
                      f" {drift:.3f} {unit} -> {verdict}")

        for name in ("BG_ON", "BG_OFF"):
            s = summary[name]["cpu"]
            if s and s["median"]:
                print(f"    implied CPU-bound FPS {name:<7} {1e6 / s['median']:8.1f}")
        for name in ("BG_ON", "BG_OFF"):
            s = summary[name]["gpu"]
            if s and s["median"]:
                print(f"    implied GPU-bound FPS {name:<7} {1e3 / s['median']:8.1f}")

    return summary, failures


def self_test():
    """Mutation tests: a clean log passes, each defect is rejected."""

    def row(msg):
        return {"ts": 0, "sev": "INFO", "cat": "system", "msg": msg}

    def build(arm_specs, blocks=4):
        out = [row("    extent=1280x720")]
        for name, cpu_us, gpu_ms in arm_specs:
            out.append(row(f"MENUBG_ARM_BEGIN_{name}"))
            for bucket in range(blocks):
                out.append(row(
                    f"frame trace (200f): bucket={bucket + 1} count=200 valid=200"
                    f" cpu_work_total={cpu_us * 200}us scr_end_total={cpu_us * 100}us"
                ))
                out.append(row("gpu (200f avg, ms):"))
                out.append(row("  ui=0.100"))
                out.append(row(f"  total={gpu_ms:.3f}"))
                out.append(row(
                    "vk perf v2: epoch=1 bucket=1 count=200 valid=200"
                    " draws_total=2000 identity_stable=1"
                ))
            out.append(row(f"MENUBG_ARM_END_{name}"))
        return out

    clean = build([("BG_ON", 900, 0.9), ("BG_OFF", 600, 0.5), ("BG_ON2", 880, 0.88)])

    cases = []
    cases.append(("clean", clean, True))
    # An arm whose markers never close: the trailing block cannot be attributed.
    cases.append(("unclosed-arm", [r for r in clean if r["msg"] != "MENUBG_ARM_END_BG_ON2"], False))
    # A missing arm entirely -- the comparison would silently become 2-way.
    cases.append((
        "missing-arm",
        build([("BG_ON", 900, 0.9), ("BG_OFF", 600, 0.5)]),
        False,
    ))
    # count != valid means the bucket did not fully populate; averaging it
    # would divide a partial total by a full frame count.
    cases.append((
        "partial-bucket",
        [row("MENUBG_ARM_BEGIN_BG_ON"),
         row("frame trace (200f): bucket=1 count=137 valid=200"
             " cpu_work_total=100000us scr_end_total=50000us"),
         row("MENUBG_ARM_END_BG_ON")] + clean,
        False,
    ))
    # Blocks emitted outside any arm must not be attributed to a neighbour.
    orphan = [r for r in clean if not r["msg"].startswith("MENUBG_ARM_BEGIN_BG_ON2")]
    cases.append(("orphan-blocks", orphan, False))
    # Nested arms: a begin while another is open is a harness bug, not data.
    cases.append((
        "nested-arms",
        [row("MENUBG_ARM_BEGIN_BG_ON"), row("MENUBG_ARM_BEGIN_BG_OFF")] + clean,
        False,
    ))
    # An arm with nothing left after the warm-up drop must fail rather than
    # report a median over an empty list or silently reuse the warm-up block.
    cases.append((
        "warmup-consumes-every-block",
        build([("BG_ON", 900, 0.9), ("BG_OFF", 600, 0.5), ("BG_ON2", 880, 0.88)], blocks=1),
        False,
    ))

    failed = 0
    for name, rows, want_ok in cases:
        _summary, failures = analyze(rows, verbose=False)
        ok = not failures
        status = "ok  " if ok == want_ok else "FAIL"
        if ok != want_ok:
            failed += 1
        print(f"  {status} {name:<18} clean={ok} want_clean={want_ok} {failures[:1]}")

    print(f"\n{'PASS' if not failed else 'FAIL'}: {len(cases) - failed}/{len(cases)} self-test cases")
    return 1 if failed else 0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--jsonl")
    ap.add_argument("--hold", type=int, default=650)
    ap.add_argument("--self-test", action="store_true")
    args = ap.parse_args()

    if args.self_test:
        return self_test()

    if not args.jsonl:
        print("FAIL: --jsonl required")
        return 2

    rows = load(args.jsonl)
    print(f"  parsed {len(rows)} JSONL rows")
    _summary, failures = analyze(rows)

    if failures:
        print("\nFAIL:")
        for f in failures:
            print(f"  - {f}")
        return 1
    print("\nPASS: every arm produced contained, fully-populated measurement blocks")
    return 0


if __name__ == "__main__":
    sys.exit(main())
