#!/usr/bin/env python3
"""Deterministic loopback master and getinfo responders for WiredUI."""

import argparse
import hashlib
import json
import select
import signal
import socket
import struct
import time

OOB = b"\xff\xff\xff\xff"


def info_response(challenge, name, mapname, clients, protocol):
    info = (
        f"\\challenge\\{challenge}\\protocol\\{protocol}\\hostname\\{name}"
        f"\\mapname\\{mapname}\\clients\\{clients}\\sv_maxclients\\8"
        "\\game\\q3now\\gametype\\0\\minping\\0\\maxping\\0"
    )
    return OOB + b"infoResponse\n" + info.encode("ascii")


def master_response(addresses):
    payload = bytearray(OOB + b"getserversResponse")
    for host, port in addresses:
        payload += b"\\" + socket.inet_aton(host) + struct.pack("!H", port)
    payload += b"\\EOT"
    return bytes(payload)


def poison_then_zero_port_response(poison_port):
    """Two complete records: poison first, then an invalid zero-port record.

    The second record is not truncated.  Its later validation failure proves
    that the parser rolls the already-decoded poison record back with its
    temporary address array instead of partially publishing it.
    """
    payload = bytearray(OOB + b"getserversResponse")
    payload += b"\\" + socket.inet_aton("127.0.0.1") + struct.pack("!H", poison_port)
    payload += b"\\" + socket.inet_aton("127.0.0.1") + struct.pack("!H", 0)
    return bytes(payload)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--events", required=True)
    parser.add_argument("--protocol", type=int, default=74)
    parser.add_argument("--timeout", type=float, default=90)
    args = parser.parse_args()

    stop = False
    def stopped(_sig, _frame):
        nonlocal stop
        stop = True
    signal.signal(signal.SIGTERM, stopped)
    signal.signal(signal.SIGINT, stopped)

    sockets = {}
    for role in ("master", "rogue", "target", "sentinel"):
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.bind(("127.0.0.1", 0))
        sock.setblocking(False)
        sockets[sock] = role
    ports = {role: sock.getsockname()[1] for sock, role in sockets.items()}

    with open(args.events, "w", encoding="utf-8", buffering=1) as out:
        def emit(event, **fields):
            out.write(json.dumps({"event": event, **fields}, sort_keys=True) + "\n")
        emit("ready", **{f"{key}_port": value for key, value in ports.items()})
        began = time.monotonic()
        served = set()
        scheduled = []
        query_ordinal = 0

        def schedule(delay, source_role, peer, scenario, packet, query):
            scheduled.append((time.monotonic() + delay, source_role, peer,
                              scenario, packet, query))
            scheduled.sort(key=lambda row: row[0])

        def send_packet(source_role, peer, scenario, packet, query, query_started):
            source_sock = next(s for s, role in sockets.items() if role == source_role)
            source_sock.sendto(packet, peer)
            elapsed_ms = int(round((time.monotonic() - query_started[query]) * 1000.0))
            emit("packet_sent", query=query, scenario=scenario,
                 source=source_role, source_port=ports[source_role],
                 source_address=f"127.0.0.1:{ports[source_role]}",
                 peer=f"{peer[0]}:{peer[1]}", peer_port=peer[1],
                 elapsed_ms=elapsed_ms, length=len(packet),
                 sha256=hashlib.sha256(packet).hexdigest(), hex=packet.hex())

        query_started = {}
        while not stop and time.monotonic() - began < args.timeout:
            now = time.monotonic()
            while scheduled and scheduled[0][0] <= now:
                _, source_role, peer, scenario, packet, query = scheduled.pop(0)
                send_packet(source_role, peer, scenario, packet, query, query_started)
            readable, _, _ = select.select(list(sockets), [], [], 0.05)
            for sock in readable:
                data, peer = sock.recvfrom(65535)
                role = sockets[sock]
                body = data[4:] if data.startswith(OOB) else data
                command = body.split(None, 1)[0].decode("ascii", "replace") if body else ""
                emit("request", role=role, command=body.decode("ascii", "replace"), peer_port=peer[1])
                if role == "master" and command in ("getservers", "getserversExt"):
                    query_ordinal += 1
                    query_started[query_ordinal] = time.monotonic()
                    emit("master_query", query=query_ordinal,
                         command=body.decode("ascii", "replace"), peer_port=peer[1])
                    canonical = master_response([
                        ("127.0.0.1", ports["target"]),
                        ("127.0.0.1", ports["sentinel"]),
                    ])
                    if query_ordinal == 1:
                        schedule(0.01, "rogue", peer, "rogue_authority",
                                 master_response([("127.0.0.1", ports["rogue"])]), 1)
                        schedule(0.03, "master", peer, "authorized_bare",
                                 OOB + b"getserversResponse", 1)
                        schedule(0.05, "master", peer, "authorized_whitespace",
                                 OOB + b"getserversResponse ", 1)
                        schedule(0.08, "master", peer, "authorized_poison_then_zero_port",
                                 poison_then_zero_port_response(ports["rogue"]), 1)
                        schedule(3.20, "master", peer, "expired_canonical", canonical, 1)
                        schedule(3.25, "master", peer, "inactive_replay", canonical, 1)
                    elif query_ordinal == 2:
                        schedule(0.20, "master", peer, "recovery_canonical", canonical, 2)
                    else:
                        emit("unexpected_master_query", query=query_ordinal)
                elif role in ("target", "sentinel") and command == "getinfo":
                    parts = body.decode("ascii", "replace").split()
                    challenge = parts[1] if len(parts) > 1 else ""
                    request_key = (query_ordinal, role, challenge)
                    if request_key in served:
                        emit("duplicate_getinfo", role=role, challenge=challenge)
                        continue
                    name = "Z0 WIRED GLOBAL TARGET" if role == "target" else "A0 WIRED GLOBAL SENTINEL"
                    mapname = "arena7" if role == "target" else "arena1"
                    clients = 1 if role == "target" else 0
                    wrong = "f" * max(1, len(challenge))
                    if wrong == challenge:
                        wrong = "e" * max(1, len(challenge))
                    sock.sendto(info_response(wrong, f"BAD {role.upper()}", "arena9", clients, args.protocol), peer)
                    emit("wrong_info_response", query=query_ordinal,
                         role=role, challenge=wrong, expected=challenge,
                         address=f"127.0.0.1:{ports[role]}")
                    sock.sendto(info_response(challenge, name, mapname, clients, args.protocol), peer)
                    emit("current_info_response", query=query_ordinal, role=role, challenge=challenge,
                         address=f"127.0.0.1:{ports[role]}")
                    served.add(request_key)
                elif role == "rogue" and command == "getinfo":
                    emit("poison_ping", query=query_ordinal,
                         challenge=body.decode("ascii", "replace"))
        emit("stopped", reason="signal" if stop else "timeout")
    for sock in sockets:
        sock.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
