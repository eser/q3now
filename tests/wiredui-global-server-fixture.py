#!/usr/bin/env python3
"""Deterministic loopback master and getinfo responders for WiredUI."""

import argparse
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
        pending_master = None
        while not stop and time.monotonic() - began < args.timeout:
            if pending_master is not None and time.monotonic() >= pending_master[0]:
                _, master_sock, master_peer = pending_master
                master_sock.sendto(master_response([
                    ("127.0.0.1", ports["target"]),
                    ("127.0.0.1", ports["sentinel"]),
                ]), master_peer)
                emit("authorized_master_response", address=f"127.0.0.1:{ports['master']}", raw="target,sentinel")
                pending_master = None
            readable, _, _ = select.select(list(sockets), [], [], 0.05)
            for sock in readable:
                data, peer = sock.recvfrom(65535)
                role = sockets[sock]
                body = data[4:] if data.startswith(OOB) else data
                command = body.split(None, 1)[0].decode("ascii", "replace") if body else ""
                emit("request", role=role, command=body.decode("ascii", "replace"), peer_port=peer[1])
                if role == "master" and command in ("getservers", "getserversExt"):
                    rogue = next(s for s, r in sockets.items() if r == "rogue")
                    rogue.sendto(master_response([("127.0.0.1", ports["rogue"])]), peer)
                    emit("rogue_master_response", address=f"127.0.0.1:{ports['rogue']}")
                    pending_master = (time.monotonic() + 0.2, sock, peer)
                elif role in ("target", "sentinel") and command == "getinfo":
                    parts = body.decode("ascii", "replace").split()
                    challenge = parts[1] if len(parts) > 1 else ""
                    if role in served:
                        emit("duplicate_getinfo", role=role, challenge=challenge)
                        continue
                    name = "Z0 WIRED GLOBAL TARGET" if role == "target" else "A0 WIRED GLOBAL SENTINEL"
                    mapname = "arena7" if role == "target" else "arena1"
                    clients = 1 if role == "target" else 0
                    wrong = "f" * max(1, len(challenge))
                    if wrong == challenge:
                        wrong = "e" * max(1, len(challenge))
                    sock.sendto(info_response(wrong, f"BAD {role.upper()}", "arena9", clients, args.protocol), peer)
                    emit("wrong_info_response", role=role, challenge=wrong, expected=challenge,
                         address=f"127.0.0.1:{ports[role]}")
                    sock.sendto(info_response(challenge, name, mapname, clients, args.protocol), peer)
                    emit("current_info_response", role=role, challenge=challenge,
                         address=f"127.0.0.1:{ports[role]}")
                    served.add(role)
                elif role == "rogue" and command == "getinfo":
                    emit("decoy_ping", challenge=body.decode("ascii", "replace"))
        emit("stopped", reason="signal" if stop else "timeout")
    for sock in sockets:
        sock.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
