#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors
"""Remove one loose demo after the authored selection marker is durable."""

import argparse
import hashlib
import json
import os
import time


def emit(stream, started, **row):
    row["elapsed_ms"] = int((time.monotonic() - started) * 1000)
    stream.write(json.dumps(row, sort_keys=True) + "\n")
    stream.flush()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--log", required=True)
    parser.add_argument("--file", required=True)
    parser.add_argument("--events", required=True)
    parser.add_argument("--marker", required=True)
    parser.add_argument("--timeout", type=float, default=20.0)
    args = parser.parse_args()

    started = time.monotonic()
    with open(args.file, "rb") as source:
        payload = source.read()
    digest = hashlib.sha256(payload).hexdigest()
    basename = os.path.basename(args.file)

    with open(args.events, "w", encoding="utf-8") as events:
        emit(events, started, kind="ready", basename=basename,
             bytes=len(payload), sha256=digest)
        deadline = started + args.timeout
        while time.monotonic() < deadline:
            try:
                with open(args.log, "r", encoding="utf-8", errors="replace") as log:
                    rows = []
                    for line_number, line in enumerate(log, 1):
                        try:
                            row = json.loads(line)
                        except (TypeError, ValueError):
                            continue
                        if isinstance(row, dict):
                            rows.append((line_number, row))
                    selection = next(((line_number, row) for line_number, row in rows
                                      if row.get("sev") == "DEBUG"
                                      and row.get("cat") == "ui"
                                      and row.get("msg") == "WiredUI: demo feeder selection row=0 name=stale.loose generation=2\n"), None)
                    marker = next(((line_number, row) for line_number, row in rows
                                   if selection is not None and line_number > selection[0]
                                   and row.get("sev") == "INFO"
                                   and row.get("cat") == "system"
                                   and row.get("msg") == args.marker + "\n"), None)
                    if marker is not None:
                        emit(events, started, kind="marker", marker=args.marker,
                             marker_line=marker[0], selection_line=selection[0],
                             selection_name="stale.loose", selection_generation=2)
                        queue_seen = any(
                            line_number <= marker[0]
                            and row.get("sev") == "DEBUG"
                            and row.get("cat") == "ui"
                            and str(row.get("msg", "")).startswith(
                                "WiredUI: queued validated demo playback ")
                            for line_number, row in rows)
                        os.unlink(args.file)
                        emit(events, started, kind="removed", basename=basename,
                             bytes=len(payload), sha256=digest,
                             exists_after=os.path.exists(args.file),
                             queue_seen_before_remove=queue_seen)
                        emit(events, started, kind="stopped", reason="complete")
                        return 0
            except FileNotFoundError:
                pass
            time.sleep(0.01)

        emit(events, started, kind="stopped", reason="timeout")
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
