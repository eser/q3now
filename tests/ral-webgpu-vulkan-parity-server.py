#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later

"""Strict localhost static/capture server for the opt-in WebGPU parity gate."""

from __future__ import annotations

import argparse
import hashlib
import json
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path


class CaptureHandler(SimpleHTTPRequestHandler):
    server_version = "WiredParity/1"

    def end_headers(self) -> None:
        self.send_header("Cross-Origin-Opener-Policy", "same-origin")
        self.send_header("Cross-Origin-Embedder-Policy", "require-corp")
        self.send_header("Cross-Origin-Resource-Policy", "same-origin")
        self.send_header("Cache-Control", "no-store")
        super().end_headers()

    def do_POST(self) -> None:
        prefix = "/__wired_parity__/webgpu/"
        name = self.path.removeprefix(prefix)
        if not self.path.startswith(prefix) or name not in {
            "arena17.png", "arena17.meta.json", "menu.png", "menu.meta.json",
            "servers.png", "servers.meta.json", "loading.png", "loading.meta.json",
            "console.png", "console.meta.json"
        }:
            self.send_error(404)
            return
        try:
            size = int(self.headers.get("Content-Length", "0"))
        except ValueError:
            self.send_error(400)
            return
        if size <= 0 or size > 8 * 1024 * 1024:
            self.send_error(413)
            return
        payload = self.rfile.read(size)
        target = self.server.output / "webgpu" / name
        target.parent.mkdir(parents=True, exist_ok=True)
        if name.endswith(".png"):
            if not payload.startswith(b"\x89PNG\r\n\x1a\n"):
                self.send_error(400)
                return
            target.write_bytes(payload)
        else:
            try:
                metadata = json.loads(payload)
            except (UnicodeDecodeError, json.JSONDecodeError):
                self.send_error(400)
                return
            stem = name.removesuffix(".meta.json")
            metadata.update({
                "source_sha256": self.server.source_sha,
                "emscripten": self.server.emscripten,
                "capture_sha256": hashlib.sha256(
                    (self.server.output / "webgpu" / f"{stem}.png").read_bytes()
                ).hexdigest(),
            })
            target.write_text(json.dumps(metadata, sort_keys=True) + "\n", encoding="utf-8")
        self.send_response(204)
        self.end_headers()

    def log_message(self, fmt: str, *args: object) -> None:
        print(f"parity-server: {fmt % args}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--source-sha", required=True)
    parser.add_argument("--emscripten", required=True)
    parser.add_argument("--port", type=int, default=8777)
    args = parser.parse_args()
    if len(args.source_sha) != 64 or any(c not in "0123456789abcdef" for c in args.source_sha):
        parser.error("--source-sha must be lowercase SHA-256")
    handler = lambda *values, **kwargs: CaptureHandler(
        *values, directory=str(args.root), **kwargs
    )
    server = ThreadingHTTPServer(("127.0.0.1", args.port), handler)
    server.output = args.output.resolve()
    server.source_sha = args.source_sha
    server.emscripten = args.emscripten
    print(f"parity-server: http://127.0.0.1:{args.port} -> {server.output}")
    server.serve_forever()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
