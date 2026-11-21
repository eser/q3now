# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors

"""Parse the engine's 'workload v1' telemetry rows and apply the pre-registered
go/no-go thresholds (backlog TASK-130, capacity roadmap doc-1 Phase 0).

The rows come from wired/workload_stats.c, one per active slot per 200-frame
window plus one context row. Counts are exact; time is exact for coarse slots and
sampled for per-call hot paths, so a slot's cost is scaled from its samples.
"""

import json
import math
import re
from typing import Dict, Iterable, List, Mapping, Optional, Sequence, Tuple

# Must match code/qcommon/wired/workload_stats.h.
MIN_EXP = 7
SUB_BITS = 2
SUBS = 1 << SUB_BITS
OCTAVES = 24
BINS = 1 + OCTAVES * SUBS + 1

ROW = re.compile(
    r"^workload v1: bucket=(?P<bucket>\d+) frames=(?P<frames>\d+) slot=(?P<slot>\w+) "
    r"items_are=(?P<items_are>\w+) calls=(?P<calls>\d+) items=(?P<items>\d+) "
    r"samples=(?P<samples>\d+) sample_ns=(?P<sample_ns>\d+) sample_items=(?P<sample_items>\d+) "
    r"max_ns=(?P<max_ns>\d+) over=(?P<over>\d+) budget_ns=(?P<budget_ns>\d+) "
    r"p50_ns=\d+ p95_ns=\d+ p99_ns=\d+ bins=(?P<bins>[0-9:,]*)$"
)
CONTEXT = re.compile(
    r"^workload context v1: bucket=(?P<bucket>\d+) frames=(?P<frames>\d+) "
    r"clients=(?P<clients>-?\d+) bots=(?P<bots>-?\d+) entities=(?P<entities>-?\d+)$"
)


INTERVALS = re.compile(
    r"^frame intervals v1: bucket=(?P<bucket>\d+) count=(?P<count>\d+) invalid=(?P<invalid>\d+) "
    r"total_ns=(?P<total_ns>\d+) max_ns=\d+ gt4ms=\d+ bins=[0-9,]+$"
)


class EvidenceError(RuntimeError):
    pass


def bin_lower_ns(b: int) -> int:
    if b == 0:
        return 0
    if b >= BINS - 1:
        return 1 << (MIN_EXP + OCTAVES)
    octave, sub = divmod(b - 1, SUBS)
    return (SUBS + sub) << (MIN_EXP + octave - SUB_BITS)


def quantile_ns(bins: Sequence[int], max_ns: int, num: int, den: int) -> int:
    """Upper edge of the bin holding the num/den quantile (never understates)."""
    samples = sum(bins)
    if not samples or not den:
        return 0
    target = max(1, -(-samples * num // den))
    seen = 0
    for b, count in enumerate(bins):
        seen += count
        if seen >= target:
            return max_ns if b >= BINS - 1 else bin_lower_ns(b + 1)
    return max_ns


def parse_bins(text: str) -> List[int]:
    bins = [0] * BINS
    if text:
        for pair in text.split(","):
            index, count = pair.split(":")
            if not 0 <= int(index) < BINS:
                raise EvidenceError("histogram bin out of range")
            bins[int(index)] = int(count)
    return bins


class Slot:
    """One slot's rows merged across the windows of a run."""

    def __init__(self, name: str, items_are: str, budget_ns: int):
        self.name, self.items_are, self.budget_ns = name, items_are, budget_ns
        self.calls = self.items = self.samples = self.sample_ns = 0
        self.sample_items = self.max_ns = self.over = 0
        self.bins = [0] * BINS

    def add(self, row: Mapping[str, str]) -> None:
        bins = parse_bins(row["bins"])
        if sum(bins) != int(row["samples"]):
            raise EvidenceError("histogram disagrees with the sample count")
        if int(row["samples"]) > int(row["calls"]):
            raise EvidenceError("more samples than calls")
        self.calls += int(row["calls"])
        self.items += int(row["items"])
        self.samples += int(row["samples"])
        self.sample_ns += int(row["sample_ns"])
        self.sample_items += int(row["sample_items"])
        self.max_ns = max(self.max_ns, int(row["max_ns"]))
        self.over += int(row["over"])
        self.bins = [a + b for a, b in zip(self.bins, bins)]

    def cost_ns(self) -> float:
        """Estimated total on-CPU time of all calls (scaled from the samples)."""
        if not self.samples:
            return 0.0
        return self.sample_ns / self.samples * self.calls

    def quantile(self, num: int, den: int) -> int:
        return quantile_ns(self.bins, self.max_ns, num, den)


class Run:
    def __init__(self):
        self.frames = 0
        self.interval_count = 0      # frame intervals seen in the window
        self.interval_ns = 0         # wall time those intervals add up to
        self.slots: Dict[str, Slot] = {}
        self.context = {"clients": [], "bots": [], "entities": []}
        self.windows = 0

    def wall_ns(self) -> float:
        """Wall time covered by the workload windows: the mean frame interval of
        the same gameplay window times the frames the windows span."""
        if not self.interval_count:
            raise EvidenceError("no frame interval rows: cannot normalise to wall time")
        return self.interval_ns / self.interval_count * self.frames

    def per_frame_ms(self, name: str) -> float:
        """Cost per frame of this run. A diagnostic only: it depends on how many
        frames the presentation regime happened to produce."""
        slot = self.slots.get(name)
        if not slot or not self.frames:
            return 0.0
        return slot.cost_ns() / 1e6 / self.frames

    def share_of_wall(self, name: str) -> float:
        """Fraction of wall time this workload kept the main thread busy. Unlike
        a per-frame cost it does not move with the frame rate, so runs in
        different presentation regimes (the 240 Hz lock vs a free-running loop)
        and at different loads compare."""
        slot = self.slots.get(name)
        if not slot or not self.frames:
            return 0.0
        return slot.cost_ns() / self.wall_ns()

    def ms_per_budget_frame(self, name: str, budget_ms: float) -> float:
        """The share expressed as milliseconds in one frame of the reference
        budget (4.17 ms = 240 Hz): the unit the thresholds are written in."""
        return self.share_of_wall(name) * budget_ms

    def agents(self) -> float:
        """Humans plus bots actually present (mean over the windows). The load a
        run really carried, which can differ from what the fixture asked for."""
        n = len(self.context["clients"])
        if not n:
            return 0.0
        return sum(c + b for c, b in zip(self.context["clients"], self.context["bots"])) / n

    def per_tick_ms(self, name: str) -> float:
        """Cost per server tick, for slots that only run on ticks."""
        ticks = self.slots["sv_tick"].calls if "sv_tick" in self.slots else 0
        slot = self.slots.get(name)
        if not slot or not ticks:
            return 0.0
        return slot.cost_ns() / 1e6 / ticks


def messages(path: str) -> Iterable[str]:
    with open(path, errors="replace") as handle:
        for line in handle:
            try:
                yield json.loads(line)["msg"].strip()
            except (ValueError, KeyError):
                continue


def load_run(jsonl_path: str, begin_marker: str = "FPS_GAMEPLAY_BEGIN",
             end_marker: str = "FPS_GAMEPLAY_END") -> Run:
    """Merge the workload rows of one run. When the gate's gameplay markers are
    present only the rows between them count; otherwise every row does."""
    rows: List[Tuple[str, re.Match]] = []
    inside: Optional[bool] = None
    seen_marker = False
    for msg in messages(jsonl_path):
        if begin_marker in msg and not msg.startswith("workload"):
            inside, seen_marker = True, True
            continue
        if end_marker in msg and not msg.startswith("workload"):
            inside = False
            continue
        if seen_marker and not inside:
            continue
        for kind, rx in (("row", ROW), ("ctx", CONTEXT), ("iv", INTERVALS)):
            match = rx.match(msg)
            if match:
                rows.append((kind, match))
                break
    run = Run()
    frames_by_bucket: Dict[int, int] = {}
    for kind, match in rows:
        if kind == "iv":
            run.interval_count += int(match["count"])
            run.interval_ns += int(match["total_ns"])
            continue
        if kind == "ctx":
            frames_by_bucket[int(match["bucket"])] = int(match["frames"])
            run.windows += 1
            for key in run.context:
                run.context[key].append(int(match[key]))
            continue
        row = match.groupdict()
        slot = run.slots.setdefault(row["slot"], Slot(row["slot"], row["items_are"], int(row["budget_ns"])))
        slot.add(row)
    run.frames = sum(frames_by_bucket.values())
    if run.slots and not run.frames:
        raise EvidenceError("workload rows without a context row")
    return run


def mean(values: Sequence[float]) -> float:
    return sum(values) / len(values) if values else 0.0


def median(values: Sequence[float]) -> float:
    ordered = sorted(values)
    n = len(ordered)
    if not n:
        return 0.0
    return ordered[n // 2] if n % 2 else (ordered[n // 2 - 1] + ordered[n // 2]) / 2


def extrapolate_linear(points: Sequence[Tuple[float, float]], x: float) -> Optional[float]:
    """Least-squares line through (load, cost) points, evaluated at x; None with
    fewer than two distinct loads."""
    if len({p[0] for p in points}) < 2:
        return None
    n = len(points)
    mx, my = sum(p[0] for p in points) / n, sum(p[1] for p in points) / n
    sxx = sum((p[0] - mx) ** 2 for p in points)
    slope = sum((p[0] - mx) * (p[1] - my) for p in points) / sxx
    return my + slope * (x - mx)


def verdicts(loads: Mapping[int, Sequence[Run]], thresholds: Mapping[str, object]) -> Dict[str, Dict[str, object]]:
    """Apply the pre-registered thresholds. `loads` maps a load level (bots) to
    the repeats measured at it. A slot is GO when any criterion holds."""
    budget_ms = float(thresholds["frame_budget_ms"])
    share = float(thresholds["go_share_of_budget_at_reference_load"])
    p99_share = float(thresholds["go_single_call_p99_share_of_budget"])
    scale_share = float(thresholds["go_extrapolated_share_of_budget"])
    scale_to = float(thresholds["extrapolation_agents"])
    reference = max(loads)
    names = sorted({name for runs in loads.values() for run in runs for name in run.slots})
    out: Dict[str, Dict[str, object]] = {}
    for name in names:
        per_load = {load: [run.ms_per_budget_frame(name, budget_ms) for run in runs] for load, runs in loads.items()}
        medians = {load: median(values) for load, values in per_load.items()}
        calls = sum(run.slots[name].calls for runs in loads.values() for run in runs if name in run.slots)
        samples = sum(run.slots[name].samples for runs in loads.values() for run in runs if name in run.slots)
        p99_ns = max((run.slots[name].quantile(99, 100) for run in loads[reference] if name in run.slots), default=0)
        ref_cost = medians[reference]
        # The x axis is the number of agents that were really present, not the
        # nominal load the fixture asked for (15 requested bots gave about 7).
        agents = {load: median([run.agents() for run in runs]) for load, runs in loads.items()}
        extrapolated = extrapolate_linear(sorted((agents[load], cost) for load, cost in medians.items()), scale_to)
        reasons = []
        if ref_cost >= share * budget_ms:
            reasons.append("a: cost at reference load %.3f ms per budget frame >= %.3f" % (ref_cost, share * budget_ms))
        if p99_ns / 1e6 >= p99_share * budget_ms:
            reasons.append("b: single-call p99 %.3f ms >= %.3f" % (p99_ns / 1e6, p99_share * budget_ms))
        if extrapolated is not None and extrapolated >= scale_share * budget_ms:
            reasons.append("c: extrapolation to %d agents %.3f ms per budget frame >= %.3f" % (scale_to, extrapolated, scale_share * budget_ms))
        if not calls:
            verdict = "NOT-MEASURABLE"
        elif not samples:
            verdict = "UNTIMED"
        else:
            verdict = "GO" if reasons else "NO-GO"
        spread = {load: (min(v), max(v)) for load, v in per_load.items()}
        out[name] = {
            "verdict": verdict, "reasons": reasons, "cost_ms_per_frame": medians,
            "spread_ms_per_frame": spread, "extrapolated_ms_per_frame": extrapolated,
            "p99_ms": p99_ns / 1e6, "agents": agents,
        }
    return out
