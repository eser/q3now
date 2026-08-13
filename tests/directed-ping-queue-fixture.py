#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors
"""Thirty-four-endpoint responder for directed-ping queue reuse evidence."""

import argparse
import hashlib
import json
import select
import signal
import socket
import time

OOB = b"\xff" * 4
PROTOCOL = 74


def response(challenge, hostname):
    info = (f"\\challenge\\{challenge}\\protocol\\{PROTOCOL}"
            f"\\hostname\\{hostname}\\mapname\\arena7\\clients\\1"
            "\\sv_maxclients\\8\\game\\q3now\\gametype\\0"
            "\\minping\\0\\maxping\\0")
    return OOB + b"infoResponse\n" + info.encode("ascii")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--events", required=True)
    parser.add_argument("--timeout", type=float, default=30.0)
    args = parser.parse_args()
    stopped = False

    def request_stop(_signum, _frame):
        nonlocal stopped
        stopped = True

    signal.signal(signal.SIGTERM, request_stop)
    signal.signal(signal.SIGINT, request_stop)
    sockets = []
    for _ in range(34):
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.bind(("127.0.0.1", 0))
        sock.setblocking(False)
        sockets.append(sock)
    ports = [sock.getsockname()[1] for sock in sockets]
    by_socket = {sock: index + 1 for index, sock in enumerate(sockets)}

    with open(args.events, "w", buffering=1, encoding="utf-8") as events:
        began = time.monotonic()
        requests = {}
        scheduled = []

        def emit(event, **fields):
            events.write(json.dumps({"event": event, **fields}, sort_keys=True) + "\n")

        def wire_fields(wire):
            return {"length": len(wire), "sha256": hashlib.sha256(wire).hexdigest(),
                    "hex": wire.hex()}

        def schedule(delay, scenario, ordinal, wire, peer):
            scheduled.append((time.monotonic() + delay, scenario, ordinal, wire, peer))
            scheduled.sort(key=lambda item: item[0])

        def send(scenario, ordinal, wire, peer):
            sockets[ordinal - 1].sendto(wire, peer)
            emit("response", scenario=scenario, request_ordinal=ordinal,
                 source=f"127.0.0.1:{ports[ordinal - 1]}",
                 source_port=ports[ordinal - 1], peer=f"{peer[0]}:{peer[1]}",
                 peer_port=peer[1], elapsed_ms=int((time.monotonic() - began) * 1000),
                 **wire_fields(wire))

        emit("ready", protocol=PROTOCOL, ports=ports)
        while not stopped and time.monotonic() - began < args.timeout:
            now = time.monotonic()
            while scheduled and scheduled[0][0] <= now:
                _, scenario, ordinal, wire, peer = scheduled.pop(0)
                send(scenario, ordinal, wire, peer)
            readable, _, _ = select.select(sockets, [], [], 0.02)
            for sock in readable:
                wire, peer = sock.recvfrom(4096)
                ordinal = by_socket[sock]
                parts = wire.split(b" ", 1)
                if len(parts) != 2 or parts[0] != OOB + b"getinfo":
                    emit("unexpected_request", request_ordinal=ordinal, **wire_fields(wire))
                    continue
                try:
                    challenge = parts[1].decode("ascii")
                except UnicodeDecodeError:
                    emit("unexpected_request", request_ordinal=ordinal, **wire_fields(wire))
                    continue
                if ordinal in requests:
                    emit("duplicate_request", request_ordinal=ordinal, challenge=challenge,
                         **wire_fields(wire))
                    continue
                requests[ordinal] = (challenge, peer)
                emit("request", request_ordinal=ordinal, challenge=challenge,
                     source=f"{peer[0]}:{peer[1]}", source_port=peer[1],
                     peer=f"127.0.0.1:{ports[ordinal - 1]}",
                     peer_port=ports[ordinal - 1],
                     elapsed_ms=int((time.monotonic() - began) * 1000),
                     **wire_fields(wire))
                if ordinal == 1:
                    schedule(0.600, "completed_1", 1,
                             response(challenge, "QUEUE COMPLETED ONE"), peer)
                elif ordinal == 34:
                    if 2 not in requests:
                        emit("fixture_error", reason="request34-before-request2")
                        stopped = True
                        break
                    old_challenge, old_peer = requests[2]
                    schedule(0.020, "stale_2_after_evict", 2,
                             response(old_challenge, "QUEUE STALE TWO"), old_peer)
                    schedule(0.060, "current_34", 34,
                             response(challenge, "QUEUE CURRENT THIRTY FOUR"), peer)
        emit("stopped", reason="signal" if stopped else "timeout",
             request_count=len(requests))
    for sock in sockets:
        sock.close()


if __name__ == "__main__":
    main()
