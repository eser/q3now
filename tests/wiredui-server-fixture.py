#!/usr/bin/env python3
"""Bounded loopback OOB fixture for the WiredUI server-browser contract."""

import argparse
import hashlib
import json
import select
import signal
import socket
import time


OOB = b"\xff\xff\xff\xff"


def info_payload(name, mapname, clients, port, protocol, challenge, malformed=False):
    max_clients = "eight" if malformed else "8"
    info = (
        f"\\challenge\\{challenge}\\protocol\\{protocol}\\hostname\\{name}\\mapname\\{mapname}"
        f"\\clients\\{clients}\\sv_maxclients\\{max_clients}\\game\\q3now"
        "\\gametype\\0\\minping\\0\\maxping\\0"
    )
    return OOB + b"infoResponse\n" + info.encode("ascii")


def status_payload(port, protocol, challenge, malformed=False):
    if malformed:
        # The challenge is current and therefore must pass the transport-level
        # correlation check.  The invalid max-client token makes this a status
        # envelope failure owned by the WiredUI feeder, not a stale-packet case.
        info = (
            f"\\challenge\\{challenge}\\sv_hostname\\Z0 WIRED Q0 TARGET"
            "\\mapname\\arena7\\sv_maxclients\\eight\\g_gametype\\0"
            f"\\gamename\\q3now\\protocol\\{protocol}\\version\\Q0 malformed {port}"
        )
        return OOB + b"statusResponse\n" + info.encode("ascii") + b"\n"
    info = (
        f"\\challenge\\{challenge}\\sv_hostname\\Z0 WIRED Q0 TARGET\\mapname\\arena7"
        "\\sv_maxclients\\8\\g_gametype\\0\\gamename\\q3now"
        f"\\protocol\\{protocol}\\version\\Q0 fixture {port}"
    )
    players = '7 23 "StatusBot"\n'
    return OOB + b"statusResponse\n" + info.encode("ascii") + b"\n" + players.encode("ascii")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--client-port", type=int, required=True)
    parser.add_argument("--sentinel-port", type=int, required=True)
    parser.add_argument("--target-port", type=int, required=True)
    parser.add_argument("--target-external", action="store_true")
    parser.add_argument("--lan-discovery", action="store_true")
    parser.add_argument("--upstream-port", type=int)
    parser.add_argument("--serverinfo-connect", action="store_true")
    parser.add_argument("--protocol", type=int, default=74)
    parser.add_argument("--events", required=True)
    parser.add_argument("--timeout", type=float, default=120.0)
    args = parser.parse_args()

    stop = False

    def on_signal(_signum, _frame):
        nonlocal stop
        stop = True

    signal.signal(signal.SIGTERM, on_signal)
    signal.signal(signal.SIGINT, on_signal)

    client = ("127.0.0.1", args.client_port)
    sockets = {}
    bindings = [("sentinel", args.sentinel_port)]
    if not args.target_external or args.upstream_port:
        bindings.append(("target", args.target_port))
    for role, port in bindings:
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.bind(("127.0.0.1", port))
        sock.setblocking(False)
        sockets[sock] = role
    if args.lan_discovery:
        # localservers is intentionally hard-wired to the four canonical Q3
        # LAN ports.  Own all four exclusively: sharing would let an unrelated
        # local server contaminate this acceptance fixture.
        for offset in range(4):
            sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            sock.bind(("0.0.0.0", 27960 + offset))
            sock.setblocking(False)
            sockets[sock] = f"lan{offset}"

    with open(args.events, "w", encoding="utf-8", buffering=1) as events:
        def emit(event, **fields):
            events.write(json.dumps({"event": event, **fields}, sort_keys=True) + "\n")

        emit(
            "ready",
            client_port=args.client_port,
            sentinel_port=args.sentinel_port,
            target_port=args.target_port,
            protocol=args.protocol,
        )
        target_socket = next((sock for sock, role in sockets.items() if role == "target"), None)
        probe_socket = target_socket or next(iter(sockets))
        probe_port = args.upstream_port or args.target_port
        if (args.target_external or args.upstream_port) and not args.serverinfo_connect:
            probe_socket.sendto(OOB + b"rcon q0browser_probe", ("127.0.0.1", probe_port))
            emit("disallowed_probe", command="rcon", target_port=probe_port)
        held_response = None
        timeout_response = None
        malformed_response = None
        lan_socket = next((sock for sock, role in sockets.items() if role == "lan0"), None)
        lan_scan_challenges = []
        lan_scan_a_requests = 0
        lan_scan_started = False
        lan_unsolicited_attempts = 0
        lan_unsolicited_next = time.monotonic() + 0.25
        lan_a_faults_sent = False
        lan_b_sequence_sent = False
        client_peer = None
        status_requests = 0
        challenges = []
        upstream_ordinal = 0
        serverinfo_connect_response = None
        serverinfo_connect_due = None
        started = time.monotonic()
        while not stop and time.monotonic() - started < args.timeout:
            now = time.monotonic()
            if (serverinfo_connect_response is not None
                    and serverinfo_connect_due is not None
                    and client_peer is not None
                    and now >= serverinfo_connect_due):
                target_socket.sendto(serverinfo_connect_response, client_peer)
                emit("serverinfo_connect_late_response",
                     source_port=args.target_port, peer_port=client_peer[1],
                     elapsed_ms=int((now - started) * 1000),
                     length=len(serverinfo_connect_response),
                     packet_hex=serverinfo_connect_response.hex(),
                     sha256=hashlib.sha256(serverinfo_connect_response).hexdigest())
                serverinfo_connect_response = None
                serverinfo_connect_due = None
            if (lan_socket is not None and not lan_scan_started
                    and lan_unsolicited_attempts < 80 and now >= lan_unsolicited_next):
                lan_socket.sendto(
                    info_payload("UNSOLICITED PRE-SCAN", "arena1", 1,
                                 27960, args.protocol, "pre-scan"), client)
                lan_unsolicited_attempts += 1
                if lan_unsolicited_attempts == 1:
                    emit("lan_unsolicited_pre_scan", address=f"127.0.0.1:{27960}")
                lan_unsolicited_next = now + 0.05
            readable, _, _ = select.select(list(sockets), [], [], 0.05)
            for sock in readable:
                role = sockets[sock]
                data, peer = sock.recvfrom(65535)
                body = data[4:] if data.startswith(OOB) else data
                command = body.split(None, 1)[0].decode("ascii", "replace") if body else ""
                if (args.serverinfo_connect and role == "target"
                        and peer[1] != args.upstream_port
                        and (not data.startswith(OOB) or command != "getstatus")):
                    # The proxy phase proves only the status ownership/lifetime
                    # boundary.  Ignore attempted transport datagrams without
                    # introducing generic request evidence.
                    continue
                if role == "target" and args.upstream_port and peer[1] == args.upstream_port:
                    if command != "statusResponse":
                        emit("unexpected_upstream", command=command, peer_port=peer[1])
                        continue
                    if args.serverinfo_connect and client_peer is not None:
                        target_socket.sendto(data, client_peer)
                        serverinfo_connect_response = data
                        serverinfo_connect_due = time.monotonic() + 0.75
                        emit("serverinfo_connect_current_response",
                             source_port=args.target_port, peer_port=client_peer[1], length=len(data),
                             elapsed_ms=int((time.monotonic() - started) * 1000),
                             packet_hex=data.hex(),
                             sha256=hashlib.sha256(data).hexdigest())
                    elif upstream_ordinal == 1 and held_response is None:
                        held_response = data
                        emit("held_stale_response", request_number=status_requests)
                    elif upstream_ordinal == 2 and client_peer is not None:
                        sock.sendto(held_response, client_peer)
                        emit("sent_stale_response", request_number=1)
                        sock.sendto(data, client_peer)
                        emit("sent_current_response", request_number=status_requests)
                    elif upstream_ordinal == 5 and client_peer is not None:
                        if malformed_response is not None:
                            sock.sendto(malformed_response, client_peer)
                            emit("sent_malformed_stale_response", stale_ordinal=4, current_ordinal=5)
                        sock.sendto(data, client_peer)
                        emit("sent_retry_valid_response", current_ordinal=5)
                    else:
                        emit("unexpected_upstream_ordinal", ordinal=upstream_ordinal)
                    continue
                emit("request", role=role, command=command, peer_port=peer[1])
                if command == "getinfo":
                    parts = body.decode("ascii", "replace").split()
                    challenge = parts[1] if len(parts) > 1 else ""
                    if role.startswith("lan"):
                        if role != "lan0":
                            emit("lan_scan_ignored_port", role=role, challenge=challenge)
                            continue
                        lan_scan_started = True
                        if challenge not in lan_scan_challenges:
                            lan_scan_challenges.append(challenge)
                            emit("lan_scan_seen", ordinal=len(lan_scan_challenges),
                                 challenge=challenge, address=f"{peer[0]}:27960")
                        ordinal = lan_scan_challenges.index(challenge) + 1
                        if ordinal == 1:
                            emit("lan_warmup_missing", challenge=challenge)
                        elif ordinal == 2:
                            lan_scan_a_requests += 1
                            if lan_scan_a_requests == 1:
                                emit("lan_scan_a_missing", challenge=challenge)
                            elif not lan_a_faults_sent:
                                wrong = "f" * max(1, len(challenge))
                                if wrong == challenge:
                                    wrong = "e" * max(1, len(challenge))
                                sock.sendto(info_payload(
                                    "WRONG CHALLENGE", "arena1", 1, 27960,
                                    args.protocol, wrong), peer)
                                emit("lan_scan_a_wrong_sent", challenge=wrong,
                                     expected=challenge, address=f"{peer[0]}:27960")
                                sock.sendto(info_payload(
                                    "MALFORMED CURRENT A", "arena1", 1, 27960,
                                    args.protocol, challenge, malformed=True), peer)
                                emit("lan_scan_a_malformed_sent", challenge=challenge,
                                     address=f"{peer[0]}:27960")
                                lan_a_faults_sent = True
                        elif ordinal == 3:
                            if not lan_b_sequence_sent:
                                old = lan_scan_challenges[1]
                                address = f"{peer[0]}:27960"
                                sock.sendto(info_payload(
                                    "LATE VALID A", "arena1", 1, 27960,
                                    args.protocol, old), peer)
                                emit("lan_scan_b_late_a_sent", stale_challenge=old,
                                     current_challenge=challenge, address=address)
                                current = info_payload(
                                    "LAN WIRED Q0 TARGET", "arena7", 1, 27960,
                                    args.protocol, challenge)
                                sock.sendto(current, peer)
                                emit("lan_scan_b_current_sent", challenge=challenge,
                                     address=address)
                                sock.sendto(current, peer)
                                emit("lan_scan_b_duplicate_sent", challenge=challenge,
                                     address=address)
                                lan_b_sequence_sent = True
                            else:
                                # Local discovery deliberately sends every
                                # port request twice; do not emit responses for
                                # the repeated B request.
                                emit("lan_scan_b_repeat_ignored", challenge=challenge)
                        else:
                            emit("lan_scan_unexpected_ordinal", ordinal=ordinal,
                                 challenge=challenge)
                    else:
                        if role == "sentinel":
                            reply = info_payload("A0 WIRED Q0 SENTINEL", "arena1", 0,
                                                 args.sentinel_port, args.protocol, challenge)
                        else:
                            reply = info_payload("Z0 WIRED Q0 TARGET", "arena7", 1,
                                                 args.target_port, args.protocol, challenge)
                        sock.sendto(reply, peer)
                        emit("info_response", role=role, peer_port=peer[1], challenge=challenge)
                elif command == "getstatus" and role == "target":
                    parts = body.decode("ascii", "replace").split()
                    challenge = parts[1] if len(parts) > 1 else ""
                    if challenge not in challenges:
                        challenges.append(challenge)
                        emit("challenge_seen", ordinal=len(challenges), challenge=challenge)
                    ordinal = challenges.index(challenge) + 1
                    if args.upstream_port:
                        status_requests += 1
                        client_peer = peer
                        if args.serverinfo_connect:
                            upstream_ordinal = ordinal
                            sock.sendto(data, ("127.0.0.1", args.upstream_port))
                            emit("serverinfo_connect_upstream_request",
                                 request_number=status_requests, ordinal=ordinal,
                                 target_port=args.target_port, peer_port=peer[1],
                                 elapsed_ms=int((time.monotonic() - started) * 1000),
                                 upstream_port=args.upstream_port)
                        elif ordinal in (1, 2):
                            upstream_ordinal = ordinal
                            sock.sendto(data, ("127.0.0.1", args.upstream_port))
                            emit("upstream_request", request_number=status_requests,
                                 ordinal=ordinal, upstream_port=args.upstream_port)
                        elif ordinal == 3:
                            emit("timeout_request", ordinal=ordinal, challenge=challenge)
                            if timeout_response is None:
                                timeout_response = status_payload(
                                    args.target_port, args.protocol, challenge)
                                emit("held_timeout_response", ordinal=ordinal)
                        elif ordinal == 4:
                            emit("malformed_retry_request", ordinal=ordinal, challenge=challenge)
                            if malformed_response is None:
                                if timeout_response is not None:
                                    sock.sendto(timeout_response, peer)
                                    emit("sent_timeout_stale_response",
                                         stale_ordinal=3, current_ordinal=4)
                                malformed_response = status_payload(
                                    args.target_port, args.protocol, challenge, malformed=True)
                                sock.sendto(malformed_response, peer)
                                emit("sent_malformed_response", current_ordinal=4)
                        elif ordinal == 5:
                            emit("valid_retry_request", ordinal=ordinal, challenge=challenge)
                            upstream_ordinal = ordinal
                            sock.sendto(data, ("127.0.0.1", args.upstream_port))
                            emit("upstream_retry_request", ordinal=ordinal,
                                 upstream_port=args.upstream_port)
                        else:
                            emit("unexpected_status_ordinal", ordinal=ordinal)
                    else:
                        sock.sendto(status_payload(
                            args.target_port, args.protocol, challenge), peer)
                        emit("status_response", role=role, peer_port=peer[1])
                elif command == "getstatus":
                    emit("wrong_target_status", role=role, peer_port=peer[1])
                elif args.serverinfo_connect and role == "target" and args.upstream_port:
                    # This fixture owns only the OOB status lifecycle.  QUIC is
                    # deliberately not relayed: the companion direct-endpoint
                    # phase proves transport/gameplay admission separately.
                    pass

        emit("stopped", reason="signal" if stop else "timeout")

    for sock in sockets:
        sock.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
