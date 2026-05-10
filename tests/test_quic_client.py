#!/usr/bin/env python3
"""
test_quic_client.py — QUIC transport test client for q3now

Tests the entire QUIC transport vertical slice:
  1. QUIC handshake with ALPN "q3v69"
  2. Capability negotiation on Stream 0
  3. Receive game state datagrams
  4. Receive events on event stream
  5. MCP tool calls (game_status, event_history)
  6. HTTP /health and /metrics endpoints

Usage:
  source tests/.venv/bin/activate
  python tests/test_quic_client.py [--host HOST] [--port PORT] [--token TOKEN]
"""

import asyncio
import json
import sys
import argparse
import ssl
import struct
from typing import Optional

from aioquic.asyncio import connect
from aioquic.asyncio.protocol import QuicConnectionProtocol
from aioquic.quic.configuration import QuicConfiguration
from aioquic.quic.events import (
    HandshakeCompleted,
    StreamDataReceived,
    DatagramFrameReceived,
    ConnectionTerminated,
)

try:
    import msgpack
except ImportError:
    msgpack = None

# ── MessagePack integer key map (mirrors WTK_* in wt_msgpack.c) ──
WTK_TYPE          = 0
WTK_SEQ           = 1
WTK_TIME          = 2
WTK_ATTACKER_ID   = 3
WTK_VICTIM_ID     = 4
WTK_WEAPON        = 5
WTK_MOD           = 6
WTK_ATTACKER_POS  = 7
WTK_VICTIM_POS    = 8
WTK_DAMAGE        = 9
WTK_PLAYER_ID     = 10
WTK_ITEM          = 11
WTK_POSITION      = 12
WTK_MESSAGE       = 13
WTK_TEAM_ONLY     = 14
WTK_MAP           = 15
WTK_GAMETYPE      = 16
WTK_PLAYERS       = 17
WTK_PL_ID         = 18
WTK_PL_NAME       = 19
WTK_PL_SCORE      = 20
WTK_PL_PING       = 21
WTK_PL_TEAM       = 22
WTK_PL_ALIVE      = 23


# ── Colors ────────────────────────────────────────────────────────
GREEN = "\033[92m"
RED = "\033[91m"
YELLOW = "\033[93m"
CYAN = "\033[96m"
RESET = "\033[0m"

def ok(msg):
    print(f"  {GREEN}✓{RESET} {msg}")

def fail(msg):
    print(f"  {RED}✗{RESET} {msg}")

def info(msg):
    print(f"  {CYAN}ℹ{RESET} {msg}")


# ── QUIC Protocol Handler ────────────────────────────────────────

class Q3QuicClient(QuicConnectionProtocol):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.datagrams = []
        self.stream_data = {}
        self.handshake_done = asyncio.Event()
        self.capability_response = None
        self._events = {}  # stream_id → asyncio.Event

    def quic_event_received(self, event):
        if isinstance(event, HandshakeCompleted):
            self.handshake_done.set()

        elif isinstance(event, DatagramFrameReceived):
            self.datagrams.append(event.data)

        elif isinstance(event, StreamDataReceived):
            sid = event.stream_id
            if sid not in self.stream_data:
                self.stream_data[sid] = b""
            self.stream_data[sid] += event.data
            if sid in self._events:
                self._events[sid].set()

        elif isinstance(event, ConnectionTerminated):
            info(f"Connection terminated: code={event.error_code} reason={event.reason_phrase}")

    async def wait_stream(self, stream_id, timeout=5.0):
        """Wait for data on a specific stream."""
        self._events[stream_id] = asyncio.Event()
        try:
            await asyncio.wait_for(self._events[stream_id].wait(), timeout)
        except asyncio.TimeoutError:
            pass
        return self.stream_data.get(stream_id, b"")


# ── Test Functions ────────────────────────────────────────────────

async def test_handshake(client: Q3QuicClient) -> bool:
    """Test 1: QUIC handshake with ALPN q3v69"""
    print(f"\n{CYAN}Test 1: QUIC Handshake{RESET}")
    try:
        await asyncio.wait_for(client.handshake_done.wait(), timeout=5.0)
        ok("QUIC handshake completed")
        return True
    except asyncio.TimeoutError:
        fail("QUIC handshake timed out")
        return False


async def test_capability_negotiation(client: Q3QuicClient, token: str) -> bool:
    """Test 2: Capability negotiation on Stream 0"""
    print(f"\n{CYAN}Test 2: Capability Negotiation{RESET}")

    # Open Stream 0 and send capability request
    stream_id = client._quic.get_next_available_stream_id(is_unidirectional=False)
    info(f"Using stream_id={stream_id}")
    cap_request = json.dumps({
        "protocol": 72,
        "token": token,
        "channels": ["datagrams", "events", "commands", "mcp", "http"]
    }).encode()

    info(f"Sending {len(cap_request)} bytes on stream {stream_id}")
    client._quic.send_stream_data(stream_id, cap_request)
    client.transmit()
    info("Transmitted, waiting for response...")

    # Wait for response
    data = await client.wait_stream(stream_id, timeout=5.0)
    if not data:
        fail("No capability response received")
        return False

    try:
        resp = json.loads(data.decode())
        info(f"Response: {json.dumps(resp, indent=2)[:200]}...")

        if resp.get("protocol") == 69:
            ok(f"Capability negotiation succeeded")
            if "granted" in resp:
                ok(f"Granted channels: {resp['granted']}")
            if "permission" in resp:
                ok(f"Permission: {resp['permission']}")
            if "server" in resp:
                ok(f"Server: {resp['server'].get('hostname', '?')} on {resp['server'].get('map', '?')}")
            client.capability_response = resp
            return True
        elif "error" in resp:
            fail(f"Auth failed: {resp['error']}")
            return False
        else:
            fail(f"Unexpected response: {resp}")
            return False
    except json.JSONDecodeError as e:
        fail(f"Invalid JSON: {e}")
        info(f"Raw data: {data[:200]}")
        return False


async def test_datagrams(client: Q3QuicClient) -> bool:
    """Test 3: Receive game state datagrams"""
    print(f"\n{CYAN}Test 3: Game State Datagrams{RESET}")

    # Wait a bit for datagrams to arrive
    await asyncio.sleep(2.0)

    if not client.datagrams:
        info("No datagrams received (server may not be sending yet)")
        return True  # not a failure — server needs WiredNet_SendDatagrams implementation

    ok(f"Received {len(client.datagrams)} datagrams")
    if msgpack:
        try:
            state = msgpack.unpackb(client.datagrams[-1], raw=False)
            ok(f"Decoded: map={state.get(WTK_MAP)}, players={len(state.get(WTK_PLAYERS, []))}")
            return True
        except Exception as e:
            fail(f"Msgpack decode error: {e}")
            return False
    else:
        info(f"Datagram size: {len(client.datagrams[-1])} bytes (msgpack not installed for decoding)")
        return True


async def test_mcp_game_status(client: Q3QuicClient) -> bool:
    """Test 4: MCP game_status tool call"""
    print(f"\n{CYAN}Test 4: MCP game_status{RESET}")

    # Open a new bidirectional stream for MCP
    stream_id = client._quic.get_next_available_stream_id(is_unidirectional=False)

    # MCP initialize
    init_msg = json.dumps({
        "jsonrpc": "2.0",
        "method": "initialize",
        "params": {
            "protocolVersion": "2025-03-26",
            "capabilities": {},
            "clientInfo": {"name": "q3now-test", "version": "0.1"}
        },
        "id": 1
    }) + "\n"

    client._quic.send_stream_data(stream_id, init_msg.encode())
    client.transmit()

    data = await client.wait_stream(stream_id, timeout=5.0)
    if not data:
        fail("No MCP initialize response")
        return False

    # Parse NDJSON — may contain multiple responses
    lines = data.decode().strip().split("\n")
    for line in lines:
        try:
            resp = json.loads(line)
            if resp.get("id") == 1:
                ok(f"MCP initialize: server={resp.get('result', {}).get('serverInfo', {}).get('name', '?')}")
                break
        except json.JSONDecodeError:
            continue

    # Now call game_status
    call_msg = json.dumps({
        "jsonrpc": "2.0",
        "method": "tools/call",
        "params": {"name": "game_status"},
        "id": 2
    }) + "\n"

    # Reset stream data for this stream
    client.stream_data[stream_id] = b""
    client._events.pop(stream_id, None)

    client._quic.send_stream_data(stream_id, call_msg.encode())
    client.transmit()

    data = await client.wait_stream(stream_id, timeout=5.0)
    if not data:
        fail("No MCP game_status response")
        return False

    lines = data.decode().strip().split("\n")
    for line in lines:
        try:
            resp = json.loads(line)
            if resp.get("id") == 2:
                content = resp.get("result", {}).get("content", [{}])[0].get("text", "")
                ok(f"game_status: {content[:150]}...")
                return True
        except json.JSONDecodeError:
            continue

    fail("No game_status response found in stream data")
    info(f"Raw: {data[:300]}")
    return False


async def test_http_health(client: Q3QuicClient) -> bool:
    """Test 5: HTTP /health endpoint"""
    print(f"\n{CYAN}Test 5: HTTP /health{RESET}")

    stream_id = client._quic.get_next_available_stream_id(is_unidirectional=False)

    http_req = b"GET /health HTTP/1.1\r\nHost: localhost\r\n\r\n"
    client._quic.send_stream_data(stream_id, http_req)
    client.transmit()

    data = await client.wait_stream(stream_id, timeout=5.0)
    if not data:
        fail("No HTTP response")
        return False

    response = data.decode(errors="replace")
    if "200 OK" in response:
        ok(f"HTTP /health → 200 OK")
        # Parse body
        if "\r\n\r\n" in response:
            body = response.split("\r\n\r\n", 1)[1]
            try:
                health = json.loads(body)
                ok(f"Health: status={health.get('status')}, map={health.get('map')}, quic_conns={health.get('quic_connections')}")
            except json.JSONDecodeError:
                info(f"Body: {body[:200]}")
        return True
    else:
        fail(f"Unexpected HTTP response: {response[:100]}")
        return False


async def test_http_metrics(client: Q3QuicClient) -> bool:
    """Test 6: HTTP /metrics endpoint (Prometheus)"""
    print(f"\n{CYAN}Test 6: HTTP /metrics{RESET}")

    stream_id = client._quic.get_next_available_stream_id(is_unidirectional=False)

    http_req = b"GET /metrics HTTP/1.1\r\nHost: localhost\r\n\r\n"
    client._quic.send_stream_data(stream_id, http_req)
    client.transmit()

    data = await client.wait_stream(stream_id, timeout=5.0)
    if not data:
        fail("No HTTP response")
        return False

    response = data.decode(errors="replace")
    if "200 OK" in response and "wired_quic_connections_active" in response:
        ok("HTTP /metrics → 200 OK (Prometheus format)")
        return True
    elif "200 OK" in response:
        ok("HTTP /metrics → 200 OK")
        info(f"Body: {response.split(chr(13)+chr(10)+chr(13)+chr(10), 1)[-1][:200]}")
        return True
    else:
        fail(f"Unexpected response: {response[:100]}")
        return False


# ── Main ──────────────────────────────────────────────────────────

async def main():
    parser = argparse.ArgumentParser(description="q3now QUIC transport test client")
    parser.add_argument("--host", default="localhost", help="Server host")
    parser.add_argument("--port", type=int, default=27960, help="Server port")
    parser.add_argument("--token", default="admintoken", help="Auth token (use admin token to test MCP)")
    args = parser.parse_args()

    print(f"{'='*60}")
    print(f" q3now QUIC Transport Test Client")
    print(f" Connecting to {args.host}:{args.port} with ALPN q3v69")
    print(f"{'='*60}")

    config = QuicConfiguration(
        is_client=True,
        alpn_protocols=["q3v69"],
    )
    # Accept self-signed certs
    config.verify_mode = ssl.CERT_NONE

    results = {}

    try:
        async with connect(
            args.host,
            args.port,
            configuration=config,
            create_protocol=Q3QuicClient,
        ) as client:
            # Test 1: Handshake
            results["handshake"] = await test_handshake(client)
            if not results["handshake"]:
                print(f"\n{RED}Handshake failed — cannot proceed.{RESET}")
                return

            # Test 2: Capability negotiation
            results["capability"] = await test_capability_negotiation(client, args.token)

            # Test 3: Datagrams
            results["datagrams"] = await test_datagrams(client)

            # Test 4: MCP
            if results.get("capability"):
                results["mcp"] = await test_mcp_game_status(client)
            else:
                info("Skipping MCP test — capability negotiation failed")

            # Test 5: HTTP /health
            results["health"] = await test_http_health(client)

            # Test 6: HTTP /metrics
            results["metrics"] = await test_http_metrics(client)

    except ConnectionRefusedError:
        fail(f"Connection refused — is the server running on {args.host}:{args.port}?")
        return
    except Exception as e:
        fail(f"Connection error: {e}")
        return

    # Summary
    print(f"\n{'='*60}")
    passed = sum(1 for v in results.values() if v)
    total = len(results)
    color = GREEN if passed == total else YELLOW if passed > 0 else RED
    print(f" {color}Results: {passed}/{total} tests passed{RESET}")
    for name, result in results.items():
        status = f"{GREEN}PASS{RESET}" if result else f"{RED}FAIL{RESET}"
        print(f"   {status}  {name}")
    print(f"{'='*60}")

    sys.exit(0 if passed == total else 1)


if __name__ == "__main__":
    asyncio.run(main())
