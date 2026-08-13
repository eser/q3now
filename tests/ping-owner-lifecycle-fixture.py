#!/usr/bin/env python3
"""Loopback authority for offscreen, expired, and capacity owner gates."""

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
    return (OOB + b"getserversResponse\\" + socket.inet_aton("127.0.0.1")
            + struct.pack("!H", port) + b"\\EOT")


def info_response(challenge, hostname):
    info = (f"\\challenge\\{challenge}\\protocol\\74\\hostname\\{hostname}"
            "\\mapname\\arena7\\clients\\1\\sv_maxclients\\8"
            "\\game\\q3now\\gametype\\0\\minping\\0\\maxping\\0")
    return OOB + b"infoResponse\n" + info.encode("ascii")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--events", required=True)
    parser.add_argument("--mode", choices=("offscreen", "expired", "capacity"), required=True)
    parser.add_argument("--timeout", type=float, default=45)
    args = parser.parse_args()
    stopped = False

    def stop_now(_sig, _frame):
        nonlocal stopped
        stopped = True

    signal.signal(signal.SIGTERM, stop_now)
    signal.signal(signal.SIGINT, stop_now)
    master = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    browser = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    fill = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    for sock in (master, browser, fill):
        sock.bind(("127.0.0.1", 0))
        sock.setblocking(False)
    sockets = {master: "master", browser: "browser", fill: "fill"}
    ports = {f"{role}_port": sock.getsockname()[1] for sock, role in sockets.items()}

    with open(args.events, "w", encoding="utf-8", buffering=1) as evidence:
        began = time.monotonic()
        counts = {role: 0 for role in sockets.values()}
        browser_request = None
        browser_sent = False

        def emit(event, **fields):
            evidence.write(json.dumps({"event": event, **fields}, sort_keys=True) + "\n")

        def send(sock, peer, scenario, wire, challenge, request_started):
            sock.sendto(wire, peer)
            emit("response", scenario=scenario, challenge=challenge,
                 source_port=sock.getsockname()[1], peer_port=peer[1],
                 length=len(wire), sha256=hashlib.sha256(wire).hexdigest(),
                 hex=wire.hex(), elapsed_ms=int((time.monotonic() - began) * 1000),
                 rtt_ms=int((time.monotonic() - request_started) * 1000))

        emit("ready", mode=args.mode, protocol=74, **ports)
        while not stopped and time.monotonic() - began < args.timeout:
            readable, _, _ = select.select(list(sockets), [], [], 0.05)
            for sock in readable:
                data, peer = sock.recvfrom(65535)
                role = sockets[sock]
                body = data[4:] if data.startswith(OOB) else data
                command = body.decode("ascii", "replace")
                counts[role] += 1
                emit("request", role=role, ordinal=counts[role], command=command,
                     source_port=peer[1], peer_port=sock.getsockname()[1],
                     elapsed_ms=int((time.monotonic() - began) * 1000))
                if role == "master" and command == "getservers q3now 74" and (
                        counts[role] == 1 or
                        (args.mode == "expired" and counts[role] == 2)):
                    send(master, peer, "master", master_response(ports["browser_port"]),
                         "", time.monotonic())
                elif role == "browser" and command.startswith("getinfo ") and counts[role] == 1:
                    challenge = command.split(" ", 1)[1]
                    browser_request = (challenge, peer, time.monotonic())
                    emit("browser_held", challenge=challenge, peer_port=peer[1])
                elif (args.mode == "expired" and role == "browser"
                      and command.startswith("getinfo ") and counts[role] == 2):
                    challenge = command.split(" ", 1)[1]
                    send(browser, peer, "browser_recovery",
                         info_response(challenge, "EXPIRED RECOVERY RESULT"),
                         challenge, time.monotonic())
                    browser_sent = True
                elif role == "fill" and command.startswith("getinfo "):
                    if browser_request is not None and not browser_sent:
                        challenge, browser_peer, request_started = browser_request
                        hostname = ("OFFSCREEN GLOBAL RESULT" if args.mode == "offscreen"
                                    else "CAPACITY PUBLISHED RESULT")
                        send(browser, browser_peer, f"browser_{args.mode}",
                             info_response(challenge, hostname), challenge, request_started)
                        browser_sent = True
                else:
                    emit("unexpected", role=role, ordinal=counts[role], command=command)
        emit("stopped", reason="signal" if stopped else "timeout",
             master_count=counts["master"], browser_count=counts["browser"],
             fill_count=counts["fill"], browser_sent=browser_sent)
    for sock in (master, browser, fill):
        sock.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
