# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors

"""Shared validation of the engine's complete 200-frame timing epochs."""

import math
import re
from typing import Mapping, Optional, Sequence


INTERVALS = re.compile(
    r"^frame intervals v1: bucket=(\d+) count=(\d+) invalid=(\d+) "
    r"total_ns=(\d+) max_ns=(\d+) gt4ms=(\d+) bins=([0-9,]+)$"
)
FRAME_TRACE = re.compile(
    r"^frame trace \((\d+)f\): bucket=(\d+) count=(\d+) valid=(\d+) "
    r"cpu_work_total=(\d+)us scr_end_total=(\d+)us$"
)
FRAME_TRACE_END = re.compile(r"^frame trace end v1: bucket=(\d+) frames=(\d+)$")


class EvidenceError(RuntimeError):
    pass


def full_frame_intervals(blocks: Mapping[int, Sequence[int]],
                         buckets: Sequence[int], expected_frames: int,
                         frame_counts: Optional[Mapping[int, int]] = None) -> Mapping[str, object]:
    if sorted(blocks) != sorted(buckets):
        raise EvidenceError("full-frame interval coverage missing or mismatched")
    ordered = sorted(buckets)
    if not ordered or ordered != list(range(ordered[0], ordered[0] + len(ordered))):
        raise EvidenceError("non-contiguous full-frame interval buckets")
    if frame_counts is not None and sorted(frame_counts) != ordered:
        raise EvidenceError("frame counts missing or mismatched")
    histogram = [0] * 33
    total = count = maximum = over = 0
    for index, bucket in enumerate(ordered):
        n, invalid, elapsed, peak, gt4, *bins = blocks[bucket]
        endpoints = 200 if frame_counts is None else frame_counts[bucket]
        if not 1 <= endpoints <= 200 or (index < len(ordered) - 1 and endpoints != 200):
            raise EvidenceError("partial frame bucket is not the final bucket")
        expected_count = endpoints - (1 if index == 0 else 0)
        if n == 0 and expected_count == 0 and not any((invalid, elapsed, peak, gt4, *bins)) and len(bins) == 33:
            continue  # A one-endpoint epoch has no interval; rejected below.
        if (n != expected_count or invalid or len(bins) != 33
                or sum(bins) != n or not 0 < peak <= elapsed <= peak * n
                or not 0 <= gt4 <= n or not sum(bins[17:]) <= gt4 <= sum(bins[16:])):
            raise EvidenceError("invalid full-frame interval block")
        if (peak <= 4000000) != (gt4 == 0):
            raise EvidenceError("full-frame peak disagrees with 4ms exceedances")
        peak_bin = min(32, peak // 250000)
        if not bins[peak_bin] or any(bins[peak_bin + 1:]):
            raise EvidenceError("full-frame histogram disagrees with peak")
        lower = sum(i * 250000 * value for i, value in enumerate(bins))
        upper = sum((min(peak, (i + 1) * 250000 - 1) if i < 32 else peak) * value
                    for i, value in enumerate(bins))
        if not lower <= elapsed <= upper:
            raise EvidenceError("full-frame histogram disagrees with elapsed time")
        total += elapsed; count += n; over += gt4; maximum = max(maximum, peak)
        histogram = [a + b for a, b in zip(histogram, bins)]
    if not count or count != expected_frames - 1:
        raise EvidenceError("full-frame interval endpoint count mismatch")
    rank, cumulative, percentile_bin = math.ceil(count * .99), 0, 0
    for percentile_bin, value in enumerate(histogram):
        cumulative += value
        if cumulative >= rank:
            break
    return {
        "count": count, "total_ns": total, "mean_ns": total / count,
        "max_ns": maximum, "gt4ms_count": over,
        "p99_lower_ns": percentile_bin * 250000,
        "p99_upper_ns": min(maximum, (percentile_bin + 1) * 250000 - 1)
                          if percentile_bin < 32 else maximum,
        "histogram": histogram,
        "scope": "consecutive client-frame-end intervals: includes pacing, outer-loop work and instrumentation; excludes first epoch anchor; NOT onscreen presentation times",
        "p99_precision": "250us histogram bounds below 8ms; overflow upper bound is observed maximum, not exact P99",
    }


def completed_frame_intervals(messages: Sequence[str], minimum_seconds: float = 0) -> Mapping[str, object]:
    """Validate ONE explicitly ended epoch, including its final partial bucket.

    Callers delimit the measurement before passing messages. Neither log wall
    timestamps nor delayed GPU averages can fill a gap in frame-end evidence.
    Existing fixed-frame callers retain their exact-200 contract above.
    """
    blocks, frames, ordered = {}, {}, []
    end = None
    if not math.isfinite(minimum_seconds) or minimum_seconds < 0:
        raise EvidenceError("invalid minimum measurement duration")
    for raw in messages:
        msg = raw.strip()
        interval, trace, ending = INTERVALS.fullmatch(msg), FRAME_TRACE.fullmatch(msg), FRAME_TRACE_END.fullmatch(msg)
        if any(msg.startswith(prefix) for prefix in ("frame intervals ", "frame trace (", "frame trace end ")):
            if not (interval or trace or ending):
                raise EvidenceError("malformed frame evidence")
            if end is not None:
                raise EvidenceError("frame evidence after epoch end")
        if interval:
            bucket, n, invalid, total, peak, over = map(int, interval.groups()[:6])
            if bucket in blocks:
                raise EvidenceError("duplicate frame interval bucket")
            blocks[bucket] = (n, invalid, total, peak, over, *map(int, interval.group(7).split(',')))
        elif trace:
            label, bucket, count, valid, work, scr = map(int, trace.groups())
            if bucket in frames or label != count or count != valid or scr > work:
                raise EvidenceError("invalid or duplicate frame trace bucket")
            if bucket not in blocks:
                raise EvidenceError("frame trace precedes interval evidence")
            frames[bucket] = count
            ordered.append(bucket)
        elif ending:
            end = tuple(map(int, ending.groups()))
    if end is None or not ordered or end[0] != ordered[-1] or ordered != sorted(ordered):
        raise EvidenceError("missing or mismatched frame epoch end")
    if sum(frames.values()) != end[1]:
        raise EvidenceError("frame epoch endpoint total mismatch")
    summary = dict(full_frame_intervals(blocks, ordered, end[1], frames))
    if summary["total_ns"] < minimum_seconds * 1e9:
        raise EvidenceError("measured frame interval duration is too short")
    summary.update(frames=end[1], final_bucket_frames=frames[ordered[-1]],
                   duration_seconds=summary["total_ns"] / 1e9)
    return summary
