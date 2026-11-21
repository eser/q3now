# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors

import json
import os
import tempfile
import unittest

from lib import workload_evidence as we

THRESHOLDS = {
    "frame_budget_ms": 4.17,
    "go_share_of_budget_at_reference_load": 0.10,
    "go_single_call_p99_share_of_budget": 0.25,
    "go_extrapolated_share_of_budget": 0.25,
    "extrapolation_agents": 64,
}


def row(slot, calls, ns_each, samples=None, bucket=1, frames=200, items=0, budget=0, items_are="none"):
    samples = calls if samples is None else samples
    bins = [0] * we.BINS
    b = 0
    # same mapping as wired_wl_bin_for
    if ns_each >= 128:
        e = ns_each.bit_length() - 1
        octave = e - we.MIN_EXP
        b = we.BINS - 1 if octave >= we.OCTAVES else 1 + octave * we.SUBS + ((ns_each >> (e - we.SUB_BITS)) & (we.SUBS - 1))
    bins[b] = samples
    packed = ",".join("%d:%d" % (i, c) for i, c in enumerate(bins) if c)
    return ("workload v1: bucket=%d frames=%d slot=%s items_are=%s calls=%d items=%d samples=%d "
            "sample_ns=%d sample_items=0 max_ns=%d over=0 budget_ns=%d p50_ns=0 p95_ns=0 p99_ns=0 bins=%s"
            % (bucket, frames, slot, items_are, calls, items, samples, samples * ns_each, ns_each, budget, packed))


def ctx(bucket=1, frames=200, clients=1, bots=0, entities=100):
    return "workload context v1: bucket=%d frames=%d clients=%d bots=%d entities=%d" % (bucket, frames, clients, bots, entities)


def interval(count=199, total_ms=1000.0):
    return "frame intervals v1: bucket=1 count=%d invalid=0 total_ns=%d max_ns=5000000 gt4ms=0 bins=%s" % (
        count, int(total_ms * 1e6), ",".join(["0"] * 33))


def write_run(lines, with_markers=True):
    handle = tempfile.NamedTemporaryFile("w", suffix=".jsonl", delete=False)
    body = (["warm-up row outside the window"] + ["echo FPS_GAMEPLAY_BEGIN"] if with_markers else []) + lines + (["echo FPS_GAMEPLAY_END", "workload context v1: bucket=9 frames=200 clients=1 bots=0 entities=1"] if with_markers else [])
    for msg in body:
        handle.write(json.dumps({"msg": msg + "\n"}) + "\n")
    handle.close()
    return handle.name


class WorkloadEvidence(unittest.TestCase):
    def test_bin_edges_match_the_engine(self):
        self.assertEqual(we.bin_lower_ns(0), 0)
        self.assertEqual(we.bin_lower_ns(1), 128)
        self.assertEqual(we.bin_lower_ns(2), 160)
        self.assertEqual(we.bin_lower_ns(5), 256)
        self.assertEqual(we.bin_lower_ns(we.BINS - 1), 128 << 24)

    def test_quantile_is_an_upper_edge_and_overflow_reports_max(self):
        bins = [0] * we.BINS
        bins[we.BINS - 1] = 3
        self.assertEqual(we.quantile_ns(bins, 123456789012, 99, 100), 123456789012)
        bins = [0] * we.BINS
        bins[12] = 100
        self.assertEqual(we.quantile_ns(bins, 1000, 99, 100), we.bin_lower_ns(13))

    def test_rows_outside_the_gameplay_window_are_ignored(self):
        path = write_run([ctx(), interval(), row("sv_tick", 10, 1000)])
        run = we.load_run(path)
        self.assertEqual(run.frames, 200)
        self.assertEqual(run.slots["sv_tick"].calls, 10)
        os.unlink(path)

    def test_sampled_cost_is_scaled_by_the_call_count(self):
        # 1600 calls, only 100 timed at 2 us each: estimated 3.2 ms in 200 frames.
        path = write_run([ctx(), interval(), row("sv_trace_tick", 1600, 2000, samples=100)])
        run = we.load_run(path)
        self.assertAlmostEqual(run.per_frame_ms("sv_trace_tick"), 1600 * 2000 / 1e6 / 200)
        # 1000 ms over 199 intervals; the windows span 200 frames.
        self.assertAlmostEqual(run.wall_ns(), 1000e6 / 199 * 200)
        self.assertAlmostEqual(run.share_of_wall("sv_trace_tick"), 3.2e6 / run.wall_ns())
        os.unlink(path)

    def test_inconsistent_histogram_is_rejected(self):
        bad = row("sv_tick", 10, 1000).replace("samples=10", "samples=11")
        path = write_run([ctx(), interval(), bad])
        with self.assertRaises(we.EvidenceError):
            we.load_run(path)
        os.unlink(path)

    def test_verdicts_apply_the_three_criteria(self):
        def run_at(cost_ns_per_call, calls, bots):
            path = write_run([ctx(bots=bots), interval(), row("heavy", calls, cost_ns_per_call), row("light", 100, 500), row("tail", 100, 1500)])
            r = we.load_run(path)
            os.unlink(path)
            return r
        # heavy: 400 calls/frame-window at 5 us = 0.01 ms/frame @0 bots, grows with load.
        loads = {0: [run_at(2000, 20000, 0)], 15: [run_at(2000, 60000, 7)]}   # 15 asked for, 7 present
        result = we.verdicts(loads, THRESHOLDS)
        self.assertEqual(result["light"]["verdict"], "NO-GO")
        self.assertEqual(result["heavy"]["verdict"], "GO")       # 0.6 ms/frame >= 0.417
        self.assertTrue(any(r.startswith("a:") for r in result["heavy"]["reasons"]))
        self.assertIsNotNone(result["heavy"]["extrapolated_ms_per_frame"])
        # the x axis is the agents really present (1 human + 7 bots), not the nominal 15
        self.assertEqual(result["heavy"]["agents"][15], 8.0)

    def test_the_cost_does_not_move_with_the_frame_rate(self):
        # The same work in a run that produced half the frames takes the same
        # share of wall time, which per-frame cost would not show.
        fast = write_run([ctx(frames=200), interval(count=199, total_ms=800), row("w", 4000, 2000)])
        slow = write_run([ctx(frames=100), interval(count=99, total_ms=800), row("w", 4000, 2000)])
        a, b = we.load_run(fast), we.load_run(slow)
        os.unlink(fast); os.unlink(slow)
        self.assertAlmostEqual(a.share_of_wall("w"), b.share_of_wall("w"), places=2)
        self.assertNotAlmostEqual(a.per_frame_ms("w"), b.per_frame_ms("w"), places=2)

    def test_a_run_without_frame_intervals_cannot_be_normalised(self):
        path = write_run([ctx(), row("w", 10, 1000)])
        run = we.load_run(path)
        os.unlink(path)
        with self.assertRaises(we.EvidenceError):
            run.share_of_wall("w")

    def test_a_slot_that_never_ran_is_not_measurable_not_no_go(self):
        path = write_run([ctx(), interval(), row("sv_snap_write", 0, 0, samples=0)])
        r = we.load_run(path)
        os.unlink(path)
        result = we.verdicts({0: [r]}, THRESHOLDS)
        self.assertEqual(result["sv_snap_write"]["verdict"], "NOT-MEASURABLE")

    def test_extrapolation_needs_two_loads(self):
        self.assertIsNone(we.extrapolate_linear([(6, 1.0), (6, 1.2)], 64))
        self.assertAlmostEqual(we.extrapolate_linear([(0, 0.0), (10, 1.0)], 64), 6.4)


if __name__ == "__main__":
    unittest.main()
