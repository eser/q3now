#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#
# Real-map advanced-fog acceptance. The licensed arena7 BSP remains external:
# the harness extracts only its canonical sfx.shader into an isolated home and
# changes fog_intel from legacy fogParms to an explicit exp2 declaration. Both
# Vulkan captures the same fixed camera with the advanced gate ON and OFF
# (classic fogCollapse), at an explicit 1280x720 window. Future RAL backends
# reuse this fixture through their own conformance gates; legacy GL is excluded.

set -u

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
# shellcheck source=lib/wired_paths.sh
. "$SCRIPT_DIR/lib/wired_paths.sh"

TIMEOUT_RUNNER="$SCRIPT_DIR/run-with-timeout.py"
SW3Z="$REPO_ROOT/tools/sw3z-archiver/cmd/sw3z/sw3z"
PNG2RAW="$REPO_ROOT/tools/png2raw/png2raw"
[ -x "$PNG2RAW" ] || PNG2RAW="$PNG2RAW.exe"

WIRED="${1:-}"
[ -n "$WIRED" ] && [ -x "$WIRED" ] || {
	echo "usage: $0 /absolute/path/to/wired" >&2
	exit 64
}
[ -x "$SW3Z" ] || { echo "SKIP: sw3z archiver unavailable"; exit 77; }
[ -x "$PNG2RAW" ] || { echo "SKIP: png2raw unavailable"; exit 77; }
[ -f "$TIMEOUT_RUNNER" ] || { echo "SKIP: timeout runner unavailable"; exit 77; }

WIRED="$(cd "$(dirname "$WIRED")" && pwd)/$(basename "$WIRED")"
BIN_DIR="$(dirname "$WIRED")"

find_required() {
	local name="$1" candidate
	shift
	for candidate in "$@"; do
		[ -f "$candidate/$name" ] && { printf '%s\n' "$candidate/$name"; return 0; }
	done
	return 1
}

VULKAN="$(find_required wired_vulkan_arm64.dylib "$BIN_DIR" "$BIN_DIR/../MacOS")" || {
	echo "SKIP: current Vulkan renderer unavailable"; exit 77;
}
HEADLESS="$(find_required wired-headless.arm64 "$BIN_DIR" "$BIN_DIR/../MacOS")" || {
	echo "SKIP: current headless binary unavailable"; exit 77;
}
GAMECL="$(find_required gameclarm64.dylib "$BIN_DIR/base" "$BIN_DIR/../Resources/base" "$BIN_DIR/../../../base")" || {
	echo "SKIP: current native gamecl unavailable"; exit 77;
}
GAMESV="$(find_required gamesvarm64.dylib "$BIN_DIR/base" "$BIN_DIR/../Resources/base" "$BIN_DIR/../../../base")" || {
	echo "SKIP: current native gamesv unavailable"; exit 77;
}
PAX21="$(find_required pax21.sw3z "$BIN_DIR/base" "$BIN_DIR/../Resources/base" "$BIN_DIR/../../../base")" || {
	echo "SKIP: current pax21 unavailable"; exit 77;
}

PAX01=""
for candidate in "${WIRED_CONTENT_ROOT:-}" "$WIRED_HOME" "$BIN_DIR/../../.."; do
	[ -n "$candidate" ] || continue
	if [ -f "$candidate/base/pax01.sw3z" ]; then
		PAX01="$(cd "$candidate/base" && pwd)/pax01.sw3z"
		break
	fi
done
[ -n "$PAX01" ] || {
	echo "SKIP: set WIRED_CONTENT_ROOT to a root containing base/pax01.sw3z"
	exit 77
}

ROOT="$(mktemp -d -t wired-advanced-fog-XXXXXX 2>/dev/null || mktemp -d)"
cleanup() {
	local status=$?
	trap - EXIT INT TERM
	if [ "${WIRED_KEEP_ARTIFACTS:-0}" = 1 ]; then
		echo "advanced-fog artifacts: $ROOT"
	else
		rm -rf "$ROOT"
	fi
	exit "$status"
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM

EXTRACT="$ROOT/extracted"
mkdir -p "$EXTRACT"
"$SW3Z" x "$PAX01" "$EXTRACT" >/dev/null || {
	echo "FAIL: could not extract canonical pax01 fixture source" >&2
	exit 1
}
[ -f "$EXTRACT/scripts/sfx.shader" ] || {
	echo "FAIL: pax01 has no canonical scripts/sfx.shader" >&2
	exit 1
}
python3 - "$EXTRACT/scripts/sfx.shader" <<'PYEOF'
import pathlib, sys
path = pathlib.Path(sys.argv[1])
source = path.read_text(encoding="utf-8", errors="strict")
start = source.find("textures/sfx/fog_intel")
if start < 0:
    raise SystemExit("FAIL: canonical fog_intel shader missing")
end = source.find("\n}", start)
if end < 0:
    raise SystemExit("FAIL: canonical fog_intel block is unterminated")
block = source[start:end]
legacy = "fogparms ( .75 .38 0 ) 800"
explicit = "fogparms ( .75 .38 0 ) 800 exp2 0.006 1000"
if block.count(legacy) != 1:
    raise SystemExit("FAIL: canonical fog_intel declaration drifted")
path.write_text(source[:start] + block.replace(legacy, explicit, 1) + source[end:],
                encoding="utf-8")
PYEOF
FIXTURE_PAX01="$ROOT/pax01-advanced-fog.sw3z"
"$SW3Z" a "$FIXTURE_PAX01" "$EXTRACT" >/dev/null || {
	echo "FAIL: could not build isolated advanced-fog fixture archive" >&2
	exit 1
}

make_home() {
	local name="$1" home="$ROOT/$1"
	mkdir -p "$home/base/screenshots"
	ln -s "$FIXTURE_PAX01" "$home/base/pax01.sw3z"
	ln -s "$PAX21" "$home/base/pax21.sw3z"
	ln -s "$GAMECL" "$home/base/$(basename "$GAMECL")"
	ln -s "$GAMESV" "$home/base/$(basename "$GAMESV")"
	printf '%s\n' "$home"
}

write_capture_cfg() {
	local cfg="$1" renderer="$2"
	printf '%s\n' \
		'cmd noclip' \
		'wait 10' \
		'cmd setviewpos 1360 0 -230 0' \
		'wait 120' \
		"screenshot advanced_fog_${renderer}_on" \
		'wait 30' \
		'set r_useGlFog 0' \
		'wait 120' \
		"screenshot advanced_fog_${renderer}_classic" \
		'wait 30' \
		'quit' > "$cfg"
}

write_runtime_config() {
	local cfg="$1"
	printf '%s\n' \
		'set vm_game 0' 'set vm_cgame 0' 'set sv_cheats 1' 'set sv_pure 0' \
		'set com_automated 1' 'set com_noHardReboot 1' 'set s_initsound 0' \
		'set r_fullscreen 0' 'set r_mode -1' \
		'set r_customwidth 1280' 'set r_customheight 720' \
		'set r_useGlFog 1' 'set r_defaultFogParmsType -1' \
		'set r_vkValidate 1' 'set r_hdr 2' 'set r_bloom 0' 'set r_ssao 0' \
		'set r_smaa 0' 'set r_forwardPlus 0' 'set r_drawSunRays 0' 'set r_shadows 0' \
		'set r_dither 0' 'set r_depthFade 0' 'set r_lens 0' \
		'set r_pinShaderTime 1' 'set r_pinFrameTime 1' \
		'set log_severity DEBUG' 'set log_file_severity DEBUG' \
		'set log_file_mode overwrite_synced' > "$cfg"
}

analyze_pair() {
	local renderer="$1" on="$2" classic="$3"
	"$PNG2RAW" "$on" > "$ROOT/${renderer}-on.raw"
	"$PNG2RAW" "$classic" > "$ROOT/${renderer}-classic.raw"
	python3 - "$renderer" "$ROOT/${renderer}-on.raw" "$ROOT/${renderer}-classic.raw" <<'PYEOF'
import pathlib, sys
renderer, on_path, classic_path = sys.argv[1:]
on = pathlib.Path(on_path).read_bytes()
classic = pathlib.Path(classic_path).read_bytes()
if len(on) != len(classic) or len(on) not in (1280*720*3, 2560*1440*3):
    raise SystemExit(f"FAIL {renderer}: unexpected/mismatched RGB byte count {len(on)} vs {len(classic)}")
if max(on, default=0) < 24 or max(classic, default=0) < 24:
    raise SystemExit(f"FAIL {renderer}: black/void capture")
deltas = [abs(a-b) for a,b in zip(on, classic)]
mean = sum(deltas) / len(deltas)
significant = sum(d > 4 for d in deltas) / len(deltas)
if mean < 0.35 or significant < 0.005:
    raise SystemExit(f"FAIL {renderer}: advanced/classic signal too weak mean={mean:.4f} significant={significant:.4%}")
print(f"MEASURE {renderer}: advanced/classic mean_abs_delta={mean:.4f} significant_channels={significant:.2%}")
PYEOF
}

run_renderer() {
	local renderer="$1" home cfg stdout qconsole on classic rc
	home="$(make_home "$renderer")" || exit 1
	cfg="$home/base/advanced-fog-runtime.cfg"
	write_runtime_config "$home/base/config.cfg"
	write_capture_cfg "$cfg" "$renderer"
	stdout="$ROOT/${renderer}.stdout.log"
	qconsole="$home/qconsole.jsonl"
	python3 "$TIMEOUT_RUNNER" --timeout 180 --kill-after 15 --cwd "$BIN_DIR" --stdout "$stdout" -- "$WIRED" \
		+set fs_basepath "$home" +set fs_homepath "$home" +set fs_game base \
		+set cl_renderer "$renderer" \
		+log renderer.assets info +log renderer.shaders info \
		+map arena7 +waitForMap +wait 100 +exec advanced-fog-runtime.cfg
	rc=$?
	[ "$rc" -eq 0 ] || { echo "FAIL $renderer: process rc=$rc (see $stdout)" >&2; exit 1; }
	[ -f "$qconsole" ] || { echo "FAIL $renderer: qconsole missing" >&2; exit 1; }
	grep -q 'FIRST GAMEPLAY FRAME.*arena7' "$stdout" "$qconsole" || {
		echo "FAIL $renderer: arena7 never reached gameplay" >&2; exit 1;
	}
	grep -q 'Advanced fog volume: shader=textures/sfx/fog_intel type=3 density=0.006 farClip=1000 depthForOpaque=800' "$stdout" "$qconsole" || {
		echo "FAIL $renderer: explicit exp2 map receipt missing" >&2; exit 1;
	}
	if grep -Eqi 'VUID|Validation Error|shader.*(compile|link).*(fail|error)|renderer .*failed to initialize|Failed to load renderer|Sys_Error|FATAL|^ERROR:' "$stdout" "$qconsole"; then
		echo "FAIL $renderer: renderer/validation error evidence" >&2
		grep -Ei 'VUID|Validation Error|shader.*(compile|link).*(fail|error)|renderer .*failed to initialize|Failed to load renderer|Sys_Error|FATAL|^ERROR:' "$stdout" "$qconsole" | head -8 >&2
		exit 1
	fi
	on="$home/base/screenshots/advanced_fog_${renderer}_on.png"
	classic="$home/base/screenshots/advanced_fog_${renderer}_classic.png"
	[ -s "$on" ] && [ -s "$classic" ] || {
		echo "FAIL $renderer: expected screenshot pair missing" >&2; exit 1;
	}
	analyze_pair "$renderer" "$on" "$classic"
}

run_headless() {
	local home stdout rc
	home="$(make_home headless)" || exit 1
	stdout="$ROOT/headless.stdout.log"
	python3 "$TIMEOUT_RUNNER" --timeout 90 --kill-after 15 --cwd "$BIN_DIR" --stdout "$stdout" -- "$HEADLESS" \
		+set fs_basepath "$home" +set fs_homepath "$home" +set fs_game base \
		+set vm_game 0 +set sv_pure 0 +set com_automated 1 +set com_noHardReboot 1 \
		+map arena7 +wait 40 +quit
	rc=$?
	[ "$rc" -eq 0 ] || { echo "FAIL headless: process rc=$rc" >&2; exit 1; }
	grep -q 'Server: arena7' "$stdout" || { echo "FAIL headless: arena7 did not load" >&2; exit 1; }
	if grep -Eqi 'Sys_Error|FATAL|^ERROR:' "$stdout"; then
		echo "FAIL headless: fatal/error evidence" >&2; exit 1
	fi
	echo "PASS headless: arena7 loaded and shut down cleanly"
}

echo "advanced-fog fixture: arena7 fog_intel -> exp2 density=0.006 farClip=1000"
echo "window: 1280x720 (explicit harness request)"
run_renderer vulkan
run_headless
echo "PASS advanced-fog runtime: Vulkan explicit-map A/B and headless smoke"
