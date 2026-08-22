#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Wired Engine contributors
# Native Vulkan proof that one RAL readback produces byte-identical TGA/PNG
# screenshots without ever publishing a non-16:9 test window.

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
TIMEOUT_RUNNER="$SCRIPT_DIR/run-with-timeout.py"

analyze_capture() {
python3 - "$1" "$2" "$3" "$4" <<'PYEOF'
import binascii
import hashlib
import json
import re
import struct
import sys
import zlib

qconsole_path, stdout_path, tga_path, png_path = sys.argv[1:]

rows = []
with open(qconsole_path, encoding="utf-8", errors="strict") as source:
    for number, line in enumerate(source, 1):
        if not line.strip():
            continue
        try:
            row = json.loads(line)
        except Exception as exc:
            raise SystemExit(f"FAIL ral-readback-runtime JSON {number}: {exc}")
        if not isinstance(row, dict) or not all(
            isinstance(row.get(key), str) for key in ("sev", "cat", "msg")
        ):
            raise SystemExit("FAIL ral-readback-runtime log schema")
        rows.append(row)

messages = [row["msg"].rstrip("\n") for row in rows]
if any(row["sev"].upper() in ("ERROR", "FATAL") for row in rows):
    raise SystemExit("FAIL ral-readback-runtime ERROR/FATAL log severity")
if any("VUID-" in message for message in messages):
    raise SystemExit("FAIL ral-readback-runtime Vulkan validation error")
if messages.count("Q0_RAL_READBACK_COMPLETE") != 1:
    raise SystemExit("FAIL ral-readback-runtime completion cardinality")

window_pattern = re.compile(
    r"window-extent schema=2 requested=1280x720 logical=1280x720 "
    r"pixels=([0-9]+)x([0-9]+) exact16x9=1 publish-ready=1"
)
window_receipts = [window_pattern.fullmatch(message) for message in messages]
window_receipts = [match for match in window_receipts if match is not None]
if len(window_receipts) != 1:
    raise SystemExit("FAIL ral-readback-runtime exact 1280x720 window receipt")
pixel_width, pixel_height = map(int, window_receipts[0].groups())
if pixel_width <= 0 or pixel_height <= 0 or pixel_width * 9 != pixel_height * 16:
    raise SystemExit("FAIL ral-readback-runtime physical window is not exact 16:9")

with open(stdout_path, encoding="utf-8", errors="replace") as source:
    stdout = source.read()
if re.search(r"\[(?:ERROR|FATAL)\]|(?:^|\n)(?:ERROR|FATAL):|VUID-", stdout, re.I):
    raise SystemExit("FAIL ral-readback-runtime stdout failure/VUID")

def decode_tga(path):
    data = open(path, "rb").read()
    if len(data) < 18:
        raise SystemExit("FAIL ral-readback-runtime truncated TGA")
    identifier, cmap, image_type = data[0], data[1], data[2]
    width, height = struct.unpack_from("<HH", data, 12)
    depth, descriptor = data[16], data[17]
    if cmap != 0 or image_type != 2 or depth != 24 or width <= 0 or height <= 0:
        raise SystemExit("FAIL ral-readback-runtime noncanonical TGA")
    if descriptor & 0x10:
        raise SystemExit("FAIL ral-readback-runtime right-to-left TGA")
    payload = data[18 + identifier:]
    row_bytes = width * 3
    if len(payload) != row_bytes * height:
        raise SystemExit("FAIL ral-readback-runtime TGA byte count")
    rows = []
    for y in range(height):
        row = payload[y * row_bytes:(y + 1) * row_bytes]
        rgb = bytearray(row_bytes)
        rgb[0::3], rgb[1::3], rgb[2::3] = row[2::3], row[1::3], row[0::3]
        rows.append(bytes(rgb))
    if not descriptor & 0x20:
        rows.reverse()
    return width, height, b"".join(rows)

def paeth(a, b, c):
    value = a + b - c
    da, db, dc = abs(value - a), abs(value - b), abs(value - c)
    return a if da <= db and da <= dc else b if db <= dc else c

def decode_png(path):
    data = open(path, "rb").read()
    if not data.startswith(b"\x89PNG\r\n\x1a\n"):
        raise SystemExit("FAIL ral-readback-runtime PNG signature")
    offset = 8
    width = height = None
    compressed = bytearray()
    saw_end = False
    while offset + 12 <= len(data):
        length = struct.unpack_from(">I", data, offset)[0]
        kind = data[offset + 4:offset + 8]
        payload = data[offset + 8:offset + 8 + length]
        crc = struct.unpack_from(">I", data, offset + 8 + length)[0]
        if offset + 12 + length > len(data):
            raise SystemExit("FAIL ral-readback-runtime truncated PNG chunk")
        if binascii.crc32(kind + payload) & 0xFFFFFFFF != crc:
            raise SystemExit("FAIL ral-readback-runtime PNG CRC")
        if kind == b"IHDR":
            width, height, depth, color, compression, filtering, interlace = struct.unpack(
                ">IIBBBBB", payload
            )
            if (depth, color, compression, filtering, interlace) != (8, 2, 0, 0, 0):
                raise SystemExit("FAIL ral-readback-runtime noncanonical PNG")
        elif kind == b"IDAT":
            compressed.extend(payload)
        elif kind == b"IEND":
            saw_end = True
            break
        offset += 12 + length
    if width is None or height is None or not saw_end or not compressed:
        raise SystemExit("FAIL ral-readback-runtime incomplete PNG")
    raw = zlib.decompress(bytes(compressed))
    stride = width * 3
    if len(raw) != (stride + 1) * height:
        raise SystemExit("FAIL ral-readback-runtime PNG byte count")
    result = bytearray(stride * height)
    previous = bytearray(stride)
    source_offset = 0
    for y in range(height):
        filter_type = raw[source_offset]
        filtered = raw[source_offset + 1:source_offset + 1 + stride]
        source_offset += stride + 1
        current = bytearray(stride)
        for x, byte in enumerate(filtered):
            left = current[x - 3] if x >= 3 else 0
            up = previous[x]
            up_left = previous[x - 3] if x >= 3 else 0
            if filter_type == 0:
                predictor = 0
            elif filter_type == 1:
                predictor = left
            elif filter_type == 2:
                predictor = up
            elif filter_type == 3:
                predictor = (left + up) // 2
            elif filter_type == 4:
                predictor = paeth(left, up, up_left)
            else:
                raise SystemExit("FAIL ral-readback-runtime unknown PNG filter")
            current[x] = (byte + predictor) & 0xFF
        result[y * stride:(y + 1) * stride] = current
        previous = current
    return width, height, bytes(result)

tga_width, tga_height, tga_rgb = decode_tga(tga_path)
png_width, png_height, png_rgb = decode_png(png_path)
if (tga_width, tga_height) != (png_width, png_height):
    raise SystemExit("FAIL ral-readback-runtime TGA/PNG extent drift")
if tga_width * 9 != tga_height * 16:
    raise SystemExit("FAIL ral-readback-runtime screenshot is not exact 16:9")
if tga_rgb != png_rgb:
    raise SystemExit("FAIL ral-readback-runtime TGA/PNG RGB mismatch")
if not tga_rgb:
    raise SystemExit("FAIL ral-readback-runtime empty pixels")
mean = sum(tga_rgb) / len(tga_rgb)
span = max(tga_rgb) - min(tga_rgb)
lit = sum(1 for index in range(0, len(tga_rgb), 3) if max(tga_rgb[index:index + 3]) > 16)
if mean <= 5.0 or span <= 16 or lit * 100 < tga_width * tga_height:
    raise SystemExit("FAIL ral-readback-runtime dark/degenerate capture")
digest = hashlib.sha256(tga_rgb).hexdigest()
print(
    f"PASS native RAL readback: {tga_width}x{tga_height} "
    f"rgb-sha256={digest} mean={mean:.2f} span={span}"
)
PYEOF
}

if [ "${1:-}" = --analyze ]; then
	[ "$#" -eq 5 ] || {
		echo "usage: $0 --analyze <qconsole.jsonl> <stdout.log> <capture.tga> <capture.png>"
		exit 64
	}
	analyze_capture "$2" "$3" "$4" "$5"
	exit $?
fi

WIRED="${1:-}"
[ -n "$WIRED" ] && [ -x "$WIRED" ] || {
	echo "usage: $0 /absolute/path/to/wired"
	exit 64
}
[ -f "$TIMEOUT_RUNNER" ] || { echo "SKIP: missing timeout runner"; exit 77; }
WIRED="$(cd "$(dirname "$WIRED")" && pwd)/$(basename "$WIRED")"
WD="$(dirname "$WIRED")"

find_required() {
	local name="$1" candidate
	shift
	for candidate in "$@"; do
		[ -f "$candidate/$name" ] && { printf '%s\n' "$candidate/$name"; return 0; }
	done
	return 1
}

RENDERER="${WIRED_RENDERER:-}"
[ -f "$RENDERER" ] || RENDERER="$(find_required wired_vulkan_arm64.dylib \
	"$WD" "$WD/Contents/MacOS" "$WD/q3now-preview.arm64.app/Contents/MacOS" "$WD/../MacOS")" || {
	echo "SKIP: set WIRED_RENDERER to the current Vulkan renderer"
	exit 77
}
GAMECL="${WIRED_GAMECL:-}"
[ -f "$GAMECL" ] || GAMECL="$(find_required gameclarm64.dylib \
	"$WD/base" "$WD/Debug/base" "$WD/Release/base" "$WD/Contents/Resources/base")" || {
	echo "SKIP: set WIRED_GAMECL to the current game module"
	exit 77
}
GAMESV="${WIRED_GAMESV:-}"
[ -f "$GAMESV" ] || GAMESV="$(find_required gamesvarm64.dylib \
	"$WD/base" "$WD/Debug/base" "$WD/Release/base" "$WD/Contents/Resources/base")" || {
	echo "SKIP: set WIRED_GAMESV to the current game module"
	exit 77
}
MOLTEN="${WIRED_MOLTENVK:-}"
[ -f "$MOLTEN" ] || MOLTEN="$(find_required libMoltenVK.dylib \
	"$WD" "$WD/Contents/MacOS" "$WD/../MacOS" /opt/homebrew/opt/molten-vk/lib /usr/local/lib)" || {
	echo "SKIP: set WIRED_MOLTENVK to MoltenVK"
	exit 77
}

CONTENT="${WIRED_CONTENT_ROOT:-}"
PACK=""
if [ -n "$CONTENT" ] && [ -f "$CONTENT/base/pax21.sw3z" ]; then
	PACK="$(cd "$CONTENT" && pwd)"
else
	for candidate in "$WD" "$WD/../Resources" "$WD/../../.."; do
		[ -f "$candidate/base/pax21.sw3z" ] && PACK="$(cd "$candidate" && pwd)" && break
	done
fi
[ -n "$PACK" ] || { echo "SKIP: set WIRED_CONTENT_ROOT"; exit 77; }
if [ -f "$PACK/base/pax01.sw3z" ]; then
	BASE="$PACK/base/pax01.sw3z"
elif [ -f "$PACK/base/pak0.pk3" ]; then
	BASE="$PACK/base/pak0.pk3"
else
	echo "SKIP: base content unavailable"
	exit 77
fi

ROOT="$(mktemp -d -t ral-readback-runtime-XXXXXX 2>/dev/null || mktemp -d)"
HOME_DIR="$ROOT/home"
RUN="$ROOT/runtime"
mkdir -p "$HOME_DIR/base" "$RUN/Contents/MacOS"
cp "$PACK/base/pax21.sw3z" "$HOME_DIR/base/" || exit 1
cp "$BASE" "$HOME_DIR/base/" || exit 1
cp "$GAMECL" "$HOME_DIR/base/gameclarm64.dylib" || exit 1
cp "$GAMESV" "$HOME_DIR/base/gamesvarm64.dylib" || exit 1
cp "$WIRED" "$RUN/wired" || exit 1
chmod +x "$RUN/wired"
cp "$RENDERER" "$RUN/Contents/MacOS/wired_vulkan_arm64.dylib" || exit 1
cp "$MOLTEN" "$RUN/Contents/MacOS/libMoltenVK.dylib" || exit 1

if command -v otool >/dev/null 2>&1; then
	otool -L "$WIRED" 2>/dev/null | sed -n 's|^[[:space:]]*@executable_path/\([^ ]*\).*|\1|p' | while read -r dependency; do
		[ -n "$dependency" ] || continue
		for candidate in "$WD" "$WD/Contents/MacOS" "$WD/../MacOS"; do
			[ -f "$candidate/$dependency" ] && { cp "$candidate/$dependency" "$RUN/"; break; }
		done
	done
fi

BOOT="$HOME_DIR/base/ral-readback-runtime.cfg"
printf '%s\n' \
	'log renderer.ral debug' \
	'set activeAction "wait 120; screenshot ral_readback tga silent; screenshot ral_readback png silent; wait 30; echo Q0_RAL_READBACK_COMPLETE; quit"' \
	'map arena1' >"$BOOT"

QCONSOLE="$HOME_DIR/qconsole.jsonl"
STDOUT="$ROOT/stdout.log"
python3 "$TIMEOUT_RUNNER" --timeout 90 --kill-after 15 --cwd "$RUN" --stdout "$STDOUT" -- \
	"$RUN/wired" \
	+set fs_basepath "$HOME_DIR" +set fs_homepath "$HOME_DIR" +set fs_game base \
	+set vm_game 0 +set vm_cgame 0 +set sv_cheats 1 +set sv_pure 0 \
	+set com_automated 1 +set com_noHardReboot 1 +set s_initsound 0 \
	+set r_fullscreen 0 +set r_mode -1 +set r_customwidth 1280 +set r_customheight 720 \
	+set r_vkValidate 1 +set r_bloom 0 +set r_ssao 0 +set r_smaa 0 \
	+set r_forwardPlus 0 +set r_drawSunRays 0 +set r_shadows 0 \
	+set log_severity DEBUG +set log_file_severity DEBUG +set log_file_mode overwrite_synced \
	+exec ral-readback-runtime.cfg
RC=$?

TGA="$HOME_DIR/base/screenshots/ral_readback.tga"
PNG="$HOME_DIR/base/screenshots/ral_readback.png"
if [ "$RC" -ne 0 ] || [ ! -f "$QCONSOLE" ] || [ ! -s "$TGA" ] || [ ! -s "$PNG" ] \
	|| ! analyze_capture "$QCONSOLE" "$STDOUT" "$TGA" "$PNG"; then
	echo "FAIL retained root: $ROOT"
	exit 1
fi

echo "PASS retained root: $ROOT"
if [ "${WIRED_KEEP_ARTIFACTS:-0}" != 1 ]; then
	rm -rf "$ROOT"
fi
