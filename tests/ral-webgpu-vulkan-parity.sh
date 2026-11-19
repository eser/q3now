#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
# shellcheck source=tests/lib/wired_paths.sh
. "$SCRIPT_DIR/lib/wired_paths.sh"

ENGINE="${1:-${WIRED_BINARY:-}}"
SOURCE_SHA="${2:-${RAL_PARITY_SOURCE_SHA:-}}"
Q3DIR="${Q3DIR:-$WIRED_INSTALL}"
OUT="${RAL_PARITY_OUT:-$REPO_ROOT/build/ral-webgpu-vulkan-parity}"
WATCHDOG="$SCRIPT_DIR/run-with-timeout.py"
PNG2RAW="${PNG2RAW:-$REPO_ROOT/tools/png2raw/png2raw}"
BASE_CONTENT="${WEB_BASE_CONTENT:-$HOME/wired/q3now-preview/base/pax01.sw3z}"

[ -x "$ENGINE" ] || { echo "FAIL: engine not executable: $ENGINE"; exit 1; }
[ -f "$BASE_CONTENT" ] || { echo "FAIL: pax01 content missing: $BASE_CONTENT"; exit 1; }
case "$SOURCE_SHA" in
  [0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f]*)
    [ "${#SOURCE_SHA}" -eq 64 ] || { echo "FAIL: source SHA must have 64 digits"; exit 1; } ;;
  *) echo "FAIL: lowercase source SHA-256 is required"; exit 1 ;;
esac
[ -f "$OUT/webgpu/arena17.png" ] && [ -f "$OUT/webgpu/arena17.meta.json" ] || {
  echo "FAIL: fresh WebGPU browser capture is required first"; exit 1;
}

mkdir -p "$OUT/vulkan"
RUN_ROOT="$(mktemp -d "$WIRED_TMP/q3now-webgpu-vulkan-parity-XXXXXX")"
trap 'rm -rf "$RUN_ROOT"' EXIT
WIRED_TMP="$RUN_ROOT"
HOME_PATH="$(wired_isolated_home vulkan-arena17)"
CFG="$HOME_PATH/base/ral-webgpu-vulkan-parity.cfg"
LOG="$OUT/vulkan/arena17.log"
META="$OUT/vulkan/arena17.meta.json"
MARKER="$(wired_shot_marker)"

printf '%s\n' \
  'map arena17' \
  'waitForMap' \
  'wait 180' \
  'screenshot ral_webgpu_vulkan_parity_arena17 png silent' \
  'wait 30' \
  'quit' >"$CFG"

echo "==> Vulkan same-source capture: arena17 default spawn (1280x720)"
python3 "$WATCHDOG" --timeout 120 --kill-after 15 --cwd "$(dirname "$ENGINE")" --stdout "$LOG" -- \
  "$ENGINE" \
  +set fs_installpath "$Q3DIR" \
  +set fs_homepath "$HOME_PATH" \
  +set cl_renderer vulkan \
  +set net_port 0 \
  +set com_automated 1 \
  +set com_noHardReboot 1 \
  +set s_initsound 0 \
  +set sv_pure 0 \
  +set vm_game 0 \
  +set vm_cgame 0 \
  +set r_fullscreen 0 \
  +set r_mode -1 \
  +set r_customwidth 1280 \
  +set r_customheight 720 \
  +set r_hdr 0 \
  +set r_hdrDisplay 0 \
  +set r_hdrAutoExposure 0 \
  +set r_renderScale 1 \
  +set r_pinShaderTime 1.0 \
  +set r_pinFrameTime 1.0 \
  +set r_dither 0 \
  +set r_chromaticAberration 0 \
  +set con_notifytime 0 \
  +log renderer.init info \
  +exec ral-webgpu-vulkan-parity.cfg

SHOT="$(wired_newest_shot "$MARKER" "$HOME_PATH/base/screenshots")" || {
  echo "FAIL: Vulkan produced no fresh screenshot"; tail -n 80 "$LOG"; exit 1;
}
cp "$SHOT" "$OUT/vulkan/arena17.png"
# HiDPI hosts present the 1280x720 logical window at native pixel density
# (SDL_WINDOW_HIGH_PIXEL_DENSITY), so the screenshot is 2560x1440. The browser
# capture is the logical 1280x720 canvas; box-average an exact 2x native
# capture down to the logical extent (the native file is retained) so both
# sides carry the same extent for the coarse-metric comparison.
python3 - "$OUT/vulkan/arena17.png" "$PNG2RAW" <<'PY'
import struct, subprocess, sys, zlib
path, png2raw = sys.argv[1:3]
head = open(path, "rb").read(24)
width, height = struct.unpack(">II", head[16:24])
if (width, height) == (1280, 720):
    sys.exit(0)
if (width, height) != (2560, 1440):
    sys.exit("FAIL: Vulkan capture is %dx%d, expected 1280x720 logical or 2560x1440 native" % (width, height))
raw = subprocess.run([png2raw, "--size", "%dx%d" % (width, height), path], check=True, capture_output=True).stdout
channels = len(raw) // (width * height)
if channels not in (3, 4):
    sys.exit("FAIL: unexpected png2raw channel count %d" % channels)
native = path[:-4] + ".native.png"
open(native, "wb").write(open(path, "rb").read())
rows = []
for y in range(720):
    row = bytearray([0])
    r0 = (2 * y) * width * channels
    r1 = (2 * y + 1) * width * channels
    for x in range(1280):
        i0 = r0 + 2 * x * channels
        i1 = r1 + 2 * x * channels
        for c in range(3):
            row.append((raw[i0 + c] + raw[i0 + channels + c] + raw[i1 + c] + raw[i1 + channels + c] + 2) // 4)
    rows.append(bytes(row))
def chunk(kind, payload):
    return struct.pack(">I", len(payload)) + kind + payload + struct.pack(">I", zlib.crc32(kind + payload) & 0xffffffff)
data = zlib.compress(b"".join(rows), 9)
open(path, "wb").write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", 1280, 720, 8, 2, 0, 0, 0)) + chunk(b"IDAT", data) + chunk(b"IEND", b""))
print("Vulkan capture box-averaged from native 2560x1440 to logical 1280x720; native retained as", native)
PY
ENGINE_SHA="$(shasum -a 256 "$ENGINE" | awk '{print $1}')"
RENDERER_SHA="$(shasum -a 256 "$(dirname "$ENGINE")/wired_vulkan_arm64.dylib" | awk '{print $1}')"
CAPTURE_SHA="$(shasum -a 256 "$OUT/vulkan/arena17.png" | awk '{print $1}')"
CONTENT_SHA="$(shasum -a 256 "$BASE_CONTENT" | awk '{print $1}')"
HOST="$(uname -srvmp)"
COMPILER="$(cc --version | head -n 1)"
python3 - "$META" "$SOURCE_SHA" "$ENGINE_SHA" "$RENDERER_SHA" "$CAPTURE_SHA" "$CONTENT_SHA" "$HOST" "$COMPILER" <<'PY'
import json,sys
path,source,engine,renderer,capture,content,host,compiler=sys.argv[1:]
data={
  "schemaVersion":1,"backend":"vulkan","map":"arena17",
  "source_sha256":source,"binary_sha256":engine,"renderer_sha256":renderer,
  "capture_sha256":capture,"width":1280,"height":720,"render_scale":1,
  "content_sha256":content,
  "color_policy":"sdr-srgb","camera":"default-spawn","warmup_frames":180,
  "shader_time":1.0,"frame_time":1.0,"host":host,"compiler":compiler
}
open(path,"w",encoding="utf-8").write(json.dumps(data,sort_keys=True)+"\n")
PY

echo "PASS: retained Vulkan capture and metadata in $OUT/vulkan"
