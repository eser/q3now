# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors

import unittest

from lib.frame_interval_evidence import EvidenceError, completed_frame_intervals


def epoch(counts, interval_ns=3_000_000, tail_hitch=False):
    rows = []
    for index, endpoints in enumerate(counts):
        count = endpoints - (index == 0)
        histogram = [0] * 33
        histogram[min(32, interval_ns // 250000)] = count
        total, peak = count * interval_ns, interval_ns if count else 0
        over = count if interval_ns > 4_000_000 else 0
        if tail_hitch and index == len(counts) - 1:
            histogram[min(32, interval_ns // 250000)] -= 1
            histogram[20] += 1
            total += 5_000_000 - interval_ns
            peak = 5_000_000
            over += 1
        bucket = index + 7  # Epoch need not be the first since engine startup.
        rows.extend([
            f"frame intervals v1: bucket={bucket} count={count} invalid=0 total_ns={total} max_ns={peak} gt4ms={over} bins=" + ','.join(map(str, histogram)),
            f"frame trace ({endpoints}f): bucket={bucket} count={endpoints} valid={endpoints} cpu_work_total={endpoints * 2000}us scr_end_total={endpoints * 1000}us",
        ])
    rows.append(f"frame trace end v1: bucket={6 + len(counts)} frames={sum(counts)}")
    return rows


class CompletedFrameEpochTests(unittest.TestCase):
    def test_full_and_partial_epochs(self):
        for counts in ([200, 200], [200, 37], [200, 1], [37]):
            with self.subTest(counts=counts):
                summary = completed_frame_intervals(epoch(counts))
                self.assertEqual(summary['count'], sum(counts) - 1)
                self.assertEqual(summary['frames'], sum(counts))
                self.assertEqual(summary['final_bucket_frames'], counts[-1])
                self.assertEqual(summary['gt4ms_count'], 0)

    def test_hitch_in_partial_tail_is_preserved(self):
        summary = completed_frame_intervals(epoch([200, 37], tail_hitch=True))
        self.assertEqual(summary['gt4ms_count'], 1)
        self.assertEqual(summary['max_ns'], 5_000_000)

    def test_actual_duration_not_log_or_requested_duration(self):
        completed_frame_intervals(epoch([200] * 51), minimum_seconds=30)
        with self.assertRaisesRegex(EvidenceError, 'too short'):
            completed_frame_intervals(epoch([200] * 50), minimum_seconds=30)

    def test_reject_incomplete_or_corrupt_epoch(self):
        good = epoch([200, 37])
        mutations = [
            [], good[:-1], good[:-2] + good[-1:], good[1:],
            [good[0]] + good, good + [good[-1]], good + [good[0]],
            good[:-1] + [good[-1].replace('frames=237', 'frames=238')],
            good[:-1] + [good[-1].replace('bucket=8', 'bucket=9')],
            [line.replace('valid=37', 'valid=36') for line in good],
            [line.replace('invalid=0', 'invalid=1') for line in good],
            [line.replace('frame trace (37f)', 'frame trace (200f)') for line in good],
            [line.replace('max_ns=3000000', 'max_ns=not-a-time') for line in good],
            epoch([37, 200]), epoch([201]), epoch([1]),
            epoch([200, 37]) + epoch([200, 37]),
        ]
        for rows in mutations:
            with self.subTest(rows=rows):
                with self.assertRaises(EvidenceError):
                    completed_frame_intervals(rows)


if __name__ == '__main__':
    unittest.main()
