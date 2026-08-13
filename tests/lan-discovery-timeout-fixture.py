#!/usr/bin/env python3
"""Real loopback packets for concurrent directed-ping/LAN discovery lifecycles."""

import argparse
import hashlib
import ipaddress
import json
import select
import signal
import socket
import time

OOB = b"\xff" * 4
PORTS = tuple(range(27960, 27964))


def info_response(challenge, name):
    info = (f"\\challenge\\{challenge}\\protocol\\74\\hostname\\{name}"
            "\\mapname\\arena7\\clients\\1\\sv_maxclients\\8"
            "\\game\\q3now\\gametype\\0\\minping\\0\\maxping\\0")
    return OOB + b"infoResponse\n" + info.encode("ascii")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--events", required=True)
    parser.add_argument("--timeout", type=float, default=30.0)
    args = parser.parse_args()
    stop = False

    def on_signal(_signum, _frame):
        nonlocal stop
        stop = True

    signal.signal(signal.SIGTERM, on_signal)
    signal.signal(signal.SIGINT, on_signal)
    probe = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        probe.connect(("192.0.2.1", 9))
        lan_address = probe.getsockname()[0]
    except OSError:
        lan_address = ""
    finally:
        probe.close()
    try:
        usable_address = bool(lan_address) and not ipaddress.ip_address(lan_address).is_loopback \
            and not ipaddress.ip_address(lan_address).is_unspecified
    except ValueError:
        usable_address = False
    if not usable_address:
        with open(args.events, "w", encoding="utf-8") as output:
            output.write(json.dumps({"event": "skip", "reason": "no-nonloopback-ipv4"}) + "\n")
        return 77
    sockets = {}
    for port in PORTS:
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.bind(("0.0.0.0", port))
        sock.setblocking(False)
        sockets[sock] = port

    with open(args.events, "w", encoding="utf-8", buffering=1) as output:
        started = time.monotonic()
        client = None
        ready_emitted = False
        challenges = []
        counts = {}
        scheduled = []

        def elapsed_ms():
            return int((time.monotonic() - started) * 1000)

        def emit(event, **fields):
            output.write(json.dumps({"event": event, **fields}, sort_keys=True) + "\n")

        def wire_fields(wire):
            return {"hex": wire.hex(), "length": len(wire),
                    "sha256": hashlib.sha256(wire).hexdigest()}

        def schedule(delay, scenario, challenge, name, generation):
            wire = info_response(challenge, name)
            scheduled.append((time.monotonic() + delay, scenario, wire, generation))
            scheduled.sort(key=lambda item: item[0])

        def send(scenario, wire, generation):
            if client is None:
                emit("fixture_error", reason="missing-client", scenario=scenario)
                return
            sender = next(sock for sock, port in sockets.items() if port == 27960)
            sender.sendto(wire, client)
            emit("response", scenario=scenario, generation=generation,
                 source=f"{lan_address}:27960", source_port=27960,
                 peer=f"{client[0]}:{client[1]}", peer_port=client[1],
                 elapsed_ms=elapsed_ms(), **wire_fields(wire))

        emit("listener_ready", ports=list(PORTS), protocol=74, lan_address=lan_address)
        while not stop and time.monotonic() - started < args.timeout:
            now = time.monotonic()
            while scheduled and scheduled[0][0] <= now:
                _, scenario, wire, generation = scheduled.pop(0)
                send(scenario, wire, generation)
            readable, _, _ = select.select(list(sockets), [], [], 0.01)
            for sock in readable:
                wire, peer = sock.recvfrom(65535)
                port = sockets[sock]
                body = wire[4:] if wire.startswith(OOB) else wire
                parts = body.decode("ascii", "replace").split()
                challenge = parts[1] if len(parts) == 2 and parts[0] == "getinfo" else ""
                client = peer
                if not ready_emitted:
                    emit("ready", ports=list(PORTS), protocol=74,
                         lan_address=lan_address, client_port=peer[1])
                    ready_emitted = True
                if challenge and challenge not in challenges:
                    challenges.append(challenge)
                ordinal = challenges.index(challenge) if challenge in challenges else -1
                counts[challenge] = counts.get(challenge, 0) + 1
                emit("request", generation=ordinal, request_ordinal=counts[challenge],
                     challenge=challenge, listener=f"0.0.0.0:{port}", listener_port=port,
                     source=f"{peer[0]}:{peer[1]}", source_port=peer[1],
                     elapsed_ms=elapsed_ms(), **wire_fields(wire))
                if not challenge or ordinal < 0 or ordinal > 3:
                    emit("unexpected_request", generation=ordinal, challenge=challenge)
                    continue
                if ordinal == 0 and counts[challenge] == 1:
                    # Manual P stays alive across authored LAN A.
                    continue
                if ordinal == 1 and counts[challenge] == 1:
                    schedule(.04, "local_a_current", challenge, "LAN CURRENT A", 1)
                    schedule(.12, "manual_p_current", challenges[0], "MANUAL P", 0)
                elif ordinal == 2 and counts[challenge] == 1:
                    schedule(3.10, "local_b_expired", challenge, "LAN EXPIRED B", 2)
                    schedule(3.16, "local_b_inactive_replay", challenge, "LAN EXPIRED B", 2)
                elif ordinal == 3 and counts[challenge] == 1:
                    schedule(.03, "local_b_stale_under_c", challenges[2], "LAN EXPIRED B", 3)
                    schedule(.07, "local_c_current", challenge, "LAN CURRENT C", 3)
                    schedule(.10, "local_c_duplicate", challenge, "LAN CURRENT C", 3)
        emit("stopped", reason="signal" if stop else "timeout")

    for sock in sockets:
        sock.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
