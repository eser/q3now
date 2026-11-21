#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors

"""Baseline report for the runtime workload telemetry (TASK-130).

Reads the qconsole.jsonl of fps-perf-gate evidence directories that were run with
`FPS_PERF_EXTRA_CFG='set com_workloadStats 1'`, merges the repeats of each load
level and prints per-workload cost, tail and the go/no-go verdict against
tests/workload-thresholds.json.

Usage:
  tests/workload-baseline-report.py --load 0 DIR [DIR...] --load 6 DIR [DIR...] ...
Each --load N starts a group of evidence directories measured at N bots.
"""

import argparse
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from lib.workload_evidence import EvidenceError, load_run, verdicts  # noqa: E402


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--thresholds", default=os.path.join(os.path.dirname(os.path.abspath(__file__)), "workload-thresholds.json"))
    parser.add_argument("--json", action="store_true", help="machine-readable output")
    parser.add_argument("--load", action="append", nargs="+", metavar=("BOTS", "DIR"), required=True,
                        help="a load level (bots) followed by the evidence directories measured at it")
    args = parser.parse_args(argv)
    loads = {}
    for group in args.load:
        if len(group) < 2:
            parser.error("--load needs a bot count and at least one directory")
        level = int(group[0])
        loads.setdefault(level, [])
        for directory in group[1:]:
            try:
                loads[level].append(load_run(os.path.join(directory, "qconsole.jsonl")))
            except (OSError, EvidenceError) as error:
                print("skip %s: %s" % (directory, error), file=sys.stderr)
    loads = {k: v for k, v in loads.items() if v}
    if not loads:
        print("no usable evidence", file=sys.stderr)
        return 2
    with open(args.thresholds) as handle:
        thresholds = json.load(handle)
    result = verdicts(loads, thresholds)
    if args.json:
        print(json.dumps(result, indent=2, default=str, sort_keys=True))
        return 0
    reference = max(loads)
    print("loads (bots): %s   repeats: %s   frame budget %.2f ms" % (
        sorted(loads), {k: len(v) for k, v in sorted(loads.items())}, thresholds["frame_budget_ms"]))
    for load, runs in sorted(loads.items()):
        ctx = [(sum(r.context["clients"]) / max(1, len(r.context["clients"])),
                sum(r.context["bots"]) / max(1, len(r.context["bots"])),
                sum(r.context["entities"]) / max(1, len(r.context["entities"]))) for r in runs]
        print("  load %-3d humans/bots/entities (mean over runs): %s" % (
            load, ["%.1f/%.1f/%.0f" % c for c in ctx]))
    header = "%-18s %-14s" % ("slot", "verdict") + "".join("  ms/B@%-3d(min-max)" % l for l in sorted(loads)) + "   p99 ms   extrap@64"
    print(header)
    for name, info in sorted(result.items()):
        cells = "".join("  %6.3f(%5.3f-%5.3f)" % (info["cost_ms_per_frame"][l], *info["spread_ms_per_frame"][l]) for l in sorted(loads))
        extrap = info["extrapolated_ms_per_frame"]
        print("%-18s %-14s%s   %6.3f   %s" % (name, info["verdict"], cells, info["p99_ms"], "n/a" if extrap is None else "%.3f" % extrap))
        for reason in info["reasons"]:
            print("    - " + reason)
    print("reference load: %d bots" % reference)
    return 0


if __name__ == "__main__":
    sys.exit(main())
