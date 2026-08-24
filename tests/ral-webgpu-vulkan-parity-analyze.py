#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later

"""Fail-closed WebGPU/Vulkan W1 geometry and presentation parity analyzer.

This gate deliberately does not claim material or pixel parity: the W1 WebGPU
path currently lowers frontend vertex color while Vulkan also samples shipped
materials. It proves that the same source/content corpus reaches a real browser
adapter and native Vulkan, produces populated 1280x720 arena17 frames, and
retains bounded coarse scene structure with exact lifecycle/toolchain receipts.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import re
import struct
import subprocess
import sys
import tempfile
import zlib
from pathlib import Path
from typing import Dict, List, Mapping, Tuple


SHA256 = re.compile(r"[0-9a-f]{64}")
CHROME = re.compile(r"Chrome/([0-9]+(?:\.[0-9]+){0,3})")
VULKAN_REQUIRED = (
    "worldBatches", "patchWorld", "materialWorld", "lightmappedWorld",
    "modelAssets", "entities", "ui",
)
FORBIDDEN = re.compile(
    r"lifecycle failure|advancing cl_renderer|failed to initialize|"
    r"renderer fallback|Sys_Error|\bFATAL\b|\bVUID-|validation error|"
    r"VM_Create.*failed|VM syscall error|died on signal",
    re.IGNORECASE,
)


class EvidenceError(RuntimeError):
    pass


def require(condition: bool, message: str) -> None:
    if not condition:
        raise EvidenceError(message)


def load_json(path: Path) -> Mapping[str, object]:
    require(path.is_file() and path.stat().st_size > 0, f"missing/empty metadata: {path}")
    value = json.loads(path.read_text(encoding="utf-8"))
    require(isinstance(value, Mapping), f"metadata must be an object: {path}")
    return value


def png_dimensions(path: Path) -> Tuple[int, int]:
    require(path.is_file() and path.stat().st_size > 0, f"missing/empty PNG: {path}")
    header = path.read_bytes()[:24]
    require(len(header) == 24 and header[:8] == b"\x89PNG\r\n\x1a\n"
            and header[12:16] == b"IHDR", f"invalid PNG header: {path}")
    return struct.unpack(">II", header[16:24])


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def decode_rgb(path: Path, decoder: Path) -> bytes:
    command = [str(decoder), str(path)]
    if decoder.suffix == ".py":
        command.insert(0, sys.executable)
    completed = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if completed.returncode != 0:
        detail = completed.stderr.decode("utf-8", "replace").strip()
        raise EvidenceError(f"png2raw failed for {path}: {detail}")
    return completed.stdout


def require_sha(meta: Mapping[str, object], field: str, label: str) -> str:
    value = str(meta.get(field, ""))
    require(SHA256.fullmatch(value) is not None, f"{label}: invalid {field}")
    return value


def require_vulkan_receipt(log_path: Path) -> None:
    require(log_path.is_file() and log_path.stat().st_size > 0,
            "vulkan: content receipt log missing")
    log = log_path.read_text(encoding="utf-8", errors="replace")
    forbidden = FORBIDDEN.search(log)
    require(forbidden is None, f"vulkan: forbidden runtime evidence: {forbidden.group(0) if forbidden else ''}")
    marker = "Wired Vulkan RAL: content receipt"
    lines = [line for line in log.splitlines() if marker in line]
    require(bool(lines), "vulkan: content receipt missing")
    fields: Dict[str, int] = {}
    for key, value in re.findall(r"\b([A-Za-z][A-Za-z0-9]*)=([0-9a-fA-F]+)", lines[-1]):
        base = 16 if any(character in "abcdefABCDEF" for character in value) else 10
        fields[key] = int(value, base)
    missing = [field for field in VULKAN_REQUIRED if fields.get(field, 0) <= 0]
    require(not missing, f"vulkan: missing/non-positive cohorts: {', '.join(missing)}")


def require_webgpu(meta: Mapping[str, object]) -> str:
    require(meta.get("backend") == "webgpu" and meta.get("map") == "arena17",
            "webgpu: backend/map mismatch")
    require(meta.get("schemaVersion") == 1 and meta.get("width") == 1280
            and meta.get("height") == 720, "webgpu: schema/extent mismatch")
    for generation in ("first", "second"):
        require(meta.get(f"{generation}State") == "running",
                f"webgpu: {generation} lifecycle did not run")
        require(meta.get(f"{generation}ContentFlags") == 7,
                f"webgpu: {generation} content receipt mismatch")
        require(meta.get(f"{generation}ArenaFlags") == 31,
                f"webgpu: {generation} arena receipt mismatch")
        require(meta.get(f"{generation}AuthoredFlags") == 31,
                f"webgpu: {generation} authored-content receipt mismatch")
    require(meta.get("resizeSequence") == [[1280, 720], [1600, 900], [1280, 720]],
            "webgpu: resize sequence mismatch")
    require(meta.get("deterministicReload") is True,
            "webgpu: deterministic reload receipt missing")
    require(meta.get("lifecycle") == ["Com_Init", "Com_Frame", "Com_Shutdown"],
            "webgpu: lifecycle receipt mismatch")
    require(meta.get("renderer") == "WebGPU RAL", "webgpu: renderer identity mismatch")
    require(isinstance(meta.get("emscripten"), str) and bool(meta.get("emscripten")),
            "webgpu: Emscripten metadata missing")
    require(CHROME.search(str(meta.get("userAgent", ""))) is not None,
            "webgpu: exact Chrome user-agent metadata missing")
    require(1 <= int(meta.get("captureAttempts", 0)) <= 120,
            "webgpu: bounded capture attempts missing")
    require(int(meta.get("capturePasses", 0)) > 0,
            "webgpu: draw-bearing capture pass receipt missing")
    require(int(meta.get("captureDynamicRange", 0)) >= 16
            and int(meta.get("captureNonBackgroundPixels", 0)) >= 1280 * 720 // 200,
            "webgpu: populated capture receipt missing")
    host = meta.get("hostReceipt")
    require(isinstance(host, Mapping), "webgpu: host receipt missing")
    require(host.get("schemaVersion") == 1 and host.get("generation") == 2
            and host.get("status") == "ready" and host.get("ready") is True,
            "webgpu: host readiness/generation mismatch")
    require(host.get("error") == "" and host.get("lost") is None,
            "webgpu: host error/device-loss evidence present")
    require(all(int(host.get(field, 0)) > 0 for field in
                ("adapterHandle", "deviceHandle", "queueHandle")),
            "webgpu: opaque host handles missing")
    adapter = host.get("adapterInfo")
    require(isinstance(adapter, Mapping)
            and any(bool(adapter.get(field)) for field in
                    ("vendor", "architecture", "device", "description")),
            "webgpu: adapter metadata missing")
    content = meta.get("content")
    require(isinstance(content, Mapping) and content.get("schemaVersion") == 1,
            "webgpu: content metadata missing")
    assets = content.get("assets")
    require(isinstance(assets, list), "webgpu: content asset list missing")
    pax01 = next((asset for asset in assets
                  if isinstance(asset, Mapping) and asset.get("id") == "pax01"), None)
    require(isinstance(pax01, Mapping), "webgpu: pax01 asset receipt missing")
    value = str(pax01.get("sha256", ""))
    require(SHA256.fullmatch(value) is not None, "webgpu: pax01 SHA-256 missing")
    return value


def require_vulkan(meta: Mapping[str, object]) -> str:
    require(meta.get("backend") == "vulkan" and meta.get("map") == "arena17",
            "vulkan: backend/map mismatch")
    require(meta.get("schemaVersion") == 1 and meta.get("width") == 1280
            and meta.get("height") == 720, "vulkan: schema/extent mismatch")
    require(meta.get("camera") == "default-spawn" and meta.get("color_policy") == "sdr-srgb",
            "vulkan: camera/color policy mismatch")
    require(meta.get("render_scale") == 1 and meta.get("warmup_frames") == 180
            and meta.get("shader_time") == 1.0 and meta.get("frame_time") == 1.0,
            "vulkan: deterministic capture pins missing")
    for field in ("binary_sha256", "renderer_sha256", "capture_sha256"):
        require_sha(meta, field, "vulkan")
    require(bool(meta.get("host")) and bool(meta.get("compiler")),
            "vulkan: exact host/compiler metadata missing")
    value = str(meta.get("content_sha256", ""))
    require(SHA256.fullmatch(value) is not None, "vulkan: pax01 SHA-256 missing")
    return value


def frame_metrics(raw: bytes, width: int, height: int) -> Mapping[str, float]:
    require(len(raw) == width * height * 3, "decoded RGB byte count mismatch")
    luma = [(raw[index] * 54 + raw[index + 1] * 183 + raw[index + 2] * 19) >> 8
            for index in range(0, len(raw), 3)]
    mean = sum(luma) / len(luma)
    deviation = math.sqrt(sum((value - mean) ** 2 for value in luma) / len(luma))
    step = 1 if width <= 16 else 4
    edges = 0
    total = 0
    for y in range(0, height - step, step):
        for x in range(0, width - step, step):
            value = luma[y * width + x]
            if (abs(value - luma[y * width + x + step]) > 16
                    or abs(value - luma[(y + step) * width + x]) > 16):
                edges += 1
            total += 1
    top = luma[: width * max(1, height // 4)]
    bottom = luma[width * (height * 3 // 4):]
    return {
        "minimum": float(min(luma)), "maximum": float(max(luma)),
        "mean": mean, "deviation": deviation, "levels": float(len(set(luma))),
        "edge_density": edges / max(1, total),
        "vertical_separation": abs(sum(top) / len(top) - sum(bottom) / len(bottom)),
    }


def analyze(root: Path, decoder: Path) -> None:
    web_png = root / "webgpu" / "arena17.png"
    vk_png = root / "vulkan" / "arena17.png"
    web_meta = load_json(root / "webgpu" / "arena17.meta.json")
    vk_meta = load_json(root / "vulkan" / "arena17.meta.json")
    require(png_dimensions(web_png) == (1280, 720), "webgpu: screenshot must be 1280x720")
    require(png_dimensions(vk_png) == (1280, 720), "vulkan: screenshot must be 1280x720")
    require(digest(web_png) == require_sha(web_meta, "capture_sha256", "webgpu"),
            "webgpu: capture SHA-256 mismatch")
    require(digest(vk_png) == require_sha(vk_meta, "capture_sha256", "vulkan"),
            "vulkan: capture SHA-256 mismatch")
    source_web = require_sha(web_meta, "source_sha256", "webgpu")
    source_vk = require_sha(vk_meta, "source_sha256", "vulkan")
    require(source_web == source_vk, "source SHA-256 mismatch")
    require(require_webgpu(web_meta) == require_vulkan(vk_meta),
            "pax01 content SHA-256 mismatch")
    require_vulkan_receipt(root / "vulkan" / "arena17.log")
    frames = {
        "webgpu": frame_metrics(decode_rgb(web_png, decoder), 1280, 720),
        "vulkan": frame_metrics(decode_rgb(vk_png, decoder), 1280, 720),
    }
    for backend, metrics in frames.items():
        require(metrics["maximum"] - metrics["minimum"] >= 32,
                f"{backend}: capture is blank/low-dynamic-range")
        require(metrics["levels"] >= 16 and metrics["deviation"] >= 8,
                f"{backend}: capture lacks populated scene variation")
        require(0.002 <= metrics["edge_density"] <= 0.35,
                f"{backend}: edge density outside bounded scene range")
        require(metrics["vertical_separation"] >= 12,
                f"{backend}: coarse vertical scene structure missing")
        print(f"{backend}: mean={metrics['mean']:.3f} sd={metrics['deviation']:.3f} "
              f"edges={metrics['edge_density']:.5f} vertical={metrics['vertical_separation']:.3f}")
    edge_ratio = min(frames["webgpu"]["edge_density"], frames["vulkan"]["edge_density"]) / max(
        frames["webgpu"]["edge_density"], frames["vulkan"]["edge_density"])
    require(edge_ratio >= 0.10, "cross-backend coarse edge-density ratio diverged")
    print(f"cross-backend W1 edge-density ratio={edge_ratio:.5f} (material/pixel parity not claimed)")


def _chunk(kind: bytes, payload: bytes) -> bytes:
    return struct.pack(">I", len(payload)) + kind + payload + struct.pack(">I", zlib.crc32(kind + payload))


def _write_stub_png(path: Path, tag: bytes = b"good") -> None:
    header = struct.pack(">IIBBBBB", 1280, 720, 8, 2, 0, 0, 0)
    path.write_bytes(b"\x89PNG\r\n\x1a\n" + _chunk(b"IHDR", header) + _chunk(b"IEND", b"") + tag)


def _fixture(root: Path, defect: str = "") -> Path:
    (root / "webgpu").mkdir(parents=True)
    (root / "vulkan").mkdir()
    decoder = root / "fake_png2raw.py"
    decoder.write_text(
        "import pathlib,sys\n"
        "p=pathlib.Path(sys.argv[1]); blank=b'blank' in p.read_bytes(); web='webgpu' in str(p)\n"
        "out=bytearray()\n"
        "for y in range(720):\n"
        " for x in range(1280):\n"
        "  v=0 if blank else ((220+(x+y)%24) if web and y<360 else "
        "((24+(x*3+y)%96) if web else ((12+(x+y)%32) if y<360 else 48+(x*5+y)%128)))\n"
        "  out.extend((v,v,v))\n"
        "sys.stdout.buffer.write(out)\n", encoding="utf-8")
    source = "a" * 64
    content = "b" * 64
    common_web: Dict[str, object] = {
        "schemaVersion": 1, "backend": "webgpu", "map": "arena17", "width": 1280,
        "height": 720, "source_sha256": source, "firstState": "running",
        "secondState": "running", "firstContentFlags": 7, "secondContentFlags": 7,
        "firstArenaFlags": 31, "secondArenaFlags": 31, "firstAuthoredFlags": 31,
        "secondAuthoredFlags": 31, "resizeSequence": [[1280, 720], [1600, 900], [1280, 720]],
        "deterministicReload": True, "lifecycle": ["Com_Init", "Com_Frame", "Com_Shutdown"],
        "renderer": "WebGPU RAL", "emscripten": "6.0.8-git",
        "userAgent": "Mozilla/5.0 Chrome/151.0.0.0 Safari/537.36", "captureAttempts": 2,
        "capturePasses": 1, "captureDynamicRange": 200, "captureNonBackgroundPixels": 200000,
        "hostReceipt": {"schemaVersion": 1, "generation": 2, "status": "ready", "ready": True,
            "error": "", "lost": None, "adapterHandle": 1, "deviceHandle": 2, "queueHandle": 3,
            "adapterInfo": {"vendor": "fixture", "architecture": "test"}},
        "content": {"schemaVersion": 1, "assets": [{"id": "pax01", "sha256": content}]},
    }
    common_vk: Dict[str, object] = {
        "schemaVersion": 1, "backend": "vulkan", "map": "arena17", "width": 1280,
        "height": 720, "source_sha256": source, "content_sha256": content,
        "camera": "default-spawn", "color_policy": "sdr-srgb", "render_scale": 1,
        "warmup_frames": 180, "shader_time": 1.0, "frame_time": 1.0,
        "binary_sha256": "c" * 64, "renderer_sha256": "d" * 64,
        "host": "fixture host", "compiler": "fixture compiler",
    }
    for backend, meta in (("webgpu", common_web), ("vulkan", common_vk)):
        png = root / backend / "arena17.png"
        _write_stub_png(png, b"blank" if defect == "blank" and backend == "webgpu" else b"good")
        meta["capture_sha256"] = digest(png)
        (root / backend / "arena17.meta.json").write_text(json.dumps(meta), encoding="utf-8")
    receipt = "Wired Vulkan RAL: content receipt " + " ".join(
        f"{field}={0 if defect == 'receipt' and field == 'patchWorld' else 1}"
        for field in VULKAN_REQUIRED)
    (root / "vulkan" / "arena17.log").write_text(receipt + "\n", encoding="utf-8")
    if defect == "source":
        common_vk["source_sha256"] = "e" * 64
    elif defect == "content":
        common_vk["content_sha256"] = "e" * 64
    elif defect == "capture_hash":
        common_web["capture_sha256"] = "f" * 64
    elif defect == "resize":
        common_web["resizeSequence"] = [[1280, 720], [1280, 720]]
    elif defect == "host_error":
        common_web["hostReceipt"]["error"] = "validation error"
    if defect in {"source", "content"}:
        (root / "vulkan" / "arena17.meta.json").write_text(json.dumps(common_vk), encoding="utf-8")
    if defect in {"capture_hash", "resize", "host_error"}:
        (root / "webgpu" / "arena17.meta.json").write_text(json.dumps(common_web), encoding="utf-8")
    return decoder


def self_test() -> int:
    failures: List[str] = []
    with tempfile.TemporaryDirectory(prefix="ral-webgpu-vulkan-parity-") as temp:
        base = Path(temp)
        decoder = _fixture(base / "pass")
        try:
            analyze(base / "pass", decoder)
        except EvidenceError as error:
            failures.append(f"valid fixture rejected: {error}")
        for defect in ("source", "content", "capture_hash", "resize", "host_error", "receipt", "blank"):
            decoder = _fixture(base / defect, defect)
            try:
                analyze(base / defect, decoder)
            except EvidenceError:
                continue
            failures.append(f"{defect} defect was accepted")
    if failures:
        for failure in failures:
            print(f"FAIL: {failure}")
        return 1
    print("PASS: WebGPU/Vulkan W1 parity analyzer fails closed (7 mutations)")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--artifacts", type=Path)
    parser.add_argument("--png2raw", type=Path)
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        return self_test()
    if not args.artifacts or not args.png2raw:
        parser.error("--artifacts and --png2raw are required")
    try:
        analyze(args.artifacts, args.png2raw)
    except (EvidenceError, OSError, ValueError, json.JSONDecodeError) as error:
        print(f"FAIL: {error}", file=sys.stderr)
        return 1
    print("PASS: same-SHA WebGPU/Vulkan W1 geometry and presentation parity evidence is bounded")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
