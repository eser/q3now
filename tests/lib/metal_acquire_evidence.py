# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors
"""Same-submission acquire/scene diagnostics, never a full-frame FPS gate."""

import re
from .frame_interval_evidence import EvidenceError

ACQUIRE = re.compile(
    r"^metal acquire scene v1: frames=(\d+) samples=(\d+) unavailable=(\d+) "
    r"acquire_ns=(\d+) before_scene_ns=(\d+) during_scene_ns=(\d+) after_scene_ns=(\d+)$"
)
SPLIT = re.compile(
    r"^metal gpu split v1: frames=(\d+) samples=(\d+) scene_ns=(\d+) "
    r"output_ns=(\d+) overlap_ns=(\d+) gap_ns=(\d+)$"
)


def acquire_scene_timing(messages):
    rows, splits = [], []
    for message in messages:
        for prefix, pattern, destination in (
                ('metal acquire scene', ACQUIRE, rows), ('metal gpu split', SPLIT, splits)):
            if message.startswith(prefix):
                match = pattern.fullmatch(message)
                if not match:
                    raise EvidenceError('malformed Metal acquire/scene timing')
                destination.append(tuple(map(int, match.groups())))
    if not rows:
        return None  # Historical logs have no correlation, not zero latency.
    if len(rows) != len(splits):
        raise EvidenceError('incomplete Metal acquire/scene timing pairs')
    for row, split in zip(rows, splits):
        frames, samples, unavailable, duration, before, during, after = row
        if (frames != 200 or frames != split[0] or samples + unavailable != split[1]
                or samples + unavailable > frames
                or abs(before + during + after - duration) > 2 * samples
                or (not samples and any((duration, before, during, after)))
                or during > split[2] + 2 * samples):
            raise EvidenceError('inconsistent Metal acquire/scene partition')
    count = sum(row[1] for row in rows)
    return {
        'blocks': len(rows), 'samples': count,
        'unavailable': sum(row[2] for row in rows),
        **{name: sum(row[index] for row in rows) / count / 1e6 if count else None
           for index, name in enumerate(('acquire_mean_ms', 'before_scene_mean_ms',
                                         'during_scene_mean_ms', 'after_scene_mean_ms'), 3)},
        'scope': 'same completed split-submission samples only; acquire-call CPU interval partitioned '
                 'against its own scene GPU interval. After-scene is NOT global GPU idle, compositor '
                 'time or onscreen latency. Delayed completion windows can include prior-window submissions.',
    }
