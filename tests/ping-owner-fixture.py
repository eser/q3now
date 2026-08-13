#!/usr/bin/env python3
"""Loopback master/endpoint for direct+global same-address ping ownership."""

import argparse
import hashlib
import json
import select
import signal
import socket
import struct
import time

OOB = b"\xff" * 4


def master_response(port):
    return OOB + b"getserversResponse\\" + socket.inet_aton("127.0.0.1") + struct.pack("!H", port) + b"\\EOT"


def info_response(challenge, name):
    info = (f"\\challenge\\{challenge}\\protocol\\74\\hostname\\{name}"
            "\\mapname\\arena7\\clients\\1\\sv_maxclients\\8"
            "\\game\\q3now\\gametype\\0\\minping\\0\\maxping\\0")
    return OOB + b"infoResponse\n" + info.encode("ascii")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--events", required=True)
    ap.add_argument("--timeout", type=float, default=90)
    args = ap.parse_args()
    stop = False

    def stop_now(_sig, _frame):
        nonlocal stop
        stop = True

    signal.signal(signal.SIGTERM, stop_now)
    signal.signal(signal.SIGINT, stop_now)
    master = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    endpoint = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    master.bind(("127.0.0.1", 0))
    endpoint.bind(("127.0.0.1", 0))
    master.setblocking(False)
    endpoint.setblocking(False)
    ports = {"master": master.getsockname()[1], "endpoint": endpoint.getsockname()[1]}
    sockets = {master: "master", endpoint: "endpoint"}

    with open(args.events, "w", encoding="utf-8", buffering=1) as out:
        def emit(event, **fields):
            out.write(json.dumps({"event": event, **fields}, sort_keys=True) + "\n")

        def send(sock, peer, scenario, packet, challenge, ordinal, request_started):
            sock.sendto(packet, peer)
            emit("response", scenario=scenario, challenge=challenge, ordinal=ordinal,
                 source_port=sock.getsockname()[1], peer_port=peer[1],
                 length=len(packet), sha256=hashlib.sha256(packet).hexdigest(),
                 hex=packet.hex(), elapsed_ms=int((time.monotonic() - began) * 1000),
                 rtt_ms=int((time.monotonic() - request_started) * 1000))

        emit("ready", protocol=74, **{f"{k}_port": v for k, v in ports.items()})
        began = time.monotonic()
        master_count = 0
        info_count = 0
        direct = None
        direct_sent = False
        info_started = {}
        while not stop and time.monotonic() - began < args.timeout:
            readable, _, _ = select.select(list(sockets), [], [], 0.05)
            for sock in readable:
                data, peer = sock.recvfrom(65535)
                role = sockets[sock]
                body = data[4:] if data.startswith(OOB) else data
                command = body.decode("ascii", "replace")
                emit("request", role=role, command=command, peer_port=peer[1],
                     ordinal=master_count + 1 if role == "master" else info_count + 1)
                if role == "master" and command == "getservers q3now 74":
                    request_started = time.monotonic()
                    master_count += 1
                    packet = master_response(ports["endpoint"])
                    send(master, peer, f"master_{master_count}", packet, "",
                         master_count, request_started)
                    if master_count == 1 and direct and not direct_sent:
                        direct_challenge, direct_peer = direct
                        direct_packet = info_response(direct_challenge, "DIRECT MUST NOT ENTER CACHE")
                        send(endpoint, direct_peer, "direct_before_global", direct_packet,
                             direct_challenge, 0, info_started[direct_challenge])
                        direct_sent = True
                elif role == "endpoint" and command.startswith("getinfo "):
                    info_count += 1
                    challenge = command.split(" ", 1)[1]
                    info_started[challenge] = time.monotonic()
                    if direct is None:
                        direct = (challenge, peer)
                        emit("direct_held", challenge=challenge, peer_port=peer[1])
                    else:
                        packet = info_response(challenge, f"GLOBAL OWNER {info_count - 1}")
                        send(endpoint, peer, f"global_{info_count - 1}", packet,
                             challenge, info_count, info_started[challenge])
                else:
                    emit("unexpected", role=role, command=command)
        emit("stopped", reason="signal" if stop else "timeout",
             master_count=master_count, info_count=info_count,
             direct_sent=direct_sent)
    master.close()
    endpoint.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
