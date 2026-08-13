#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors
"""Negative authority fixture for the in-game Server Info gate.

The real active server is a separate wired-headless process.  This socket is
the deliberately stale browser endpoint and must receive no getstatus request
after the connected-origin popup is opened.
"""

import argparse
import hashlib
import json
import signal
import socket
import time


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", type=int, required=True)
    parser.add_argument("--events", required=True)
    args = parser.parse_args()
    if not 1 <= args.port <= 65535:
        raise SystemExit("invalid port")

    running = True

    def stop(_signum, _frame):
        nonlocal running
        running = False

    signal.signal(signal.SIGTERM, stop)
    signal.signal(signal.SIGINT, stop)
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(("127.0.0.1", args.port))
    sock.settimeout(0.1)
    started = time.monotonic()
    with open(args.events, "w", encoding="utf-8", buffering=1) as events:
        def emit(kind: str, **fields):
            events.write(json.dumps({"kind": kind,
                                     "elapsed_ms": int((time.monotonic() - started) * 1000),
                                     **fields}, sort_keys=True) + "\n")

        emit("ready", address=f"127.0.0.1:{args.port}", port=args.port)
        while running:
            try:
                data, peer = sock.recvfrom(65535)
            except socket.timeout:
                continue
            emit("unexpected", peer=f"{peer[0]}:{peer[1]}", length=len(data),
                 sha256=hashlib.sha256(data).hexdigest(), hex=data.hex())
        emit("stopped", reason="signal")
    sock.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
