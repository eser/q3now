#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Deterministic 1280x720 Vulkan/RAL continuous r_brightness visual gate.

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
TIMEOUT_RUNNER="$SCRIPT_DIR/run-with-timeout.py"
PNG2RAW="$SCRIPT_DIR/../tools/png2raw/png2raw"
VALUES=(0 0.5 1 1.37 2.6 5.2)

WIRED="${1:-}"
CONTENT="${WIRED_CONTENT_ROOT:-}"
[ -n "$WIRED" ] && [ -x "$WIRED" ] || { echo "usage: WIRED_CONTENT_ROOT=/path $0 /absolute/path/to/wired"; exit 64; }
[ "$(uname -s)" = Darwin ] || { echo "SKIP: runtime gate requires macOS/MoltenVK"; exit 77; }
[ -x "$PNG2RAW" ] || { echo "SKIP: missing png2raw"; exit 77; }
[ -f "$CONTENT/base/pax01.sw3z" ] || { echo "SKIP: WIRED_CONTENT_ROOT lacks base/pax01.sw3z"; exit 77; }

WIRED="$(cd "$(dirname "$WIRED")" && pwd)/$(basename "$WIRED")"
WD="$(dirname "$WIRED")"
find_required() { local n="$1" d; shift; for d in "$@"; do [ -f "$d/$n" ] && { printf '%s\n' "$d/$n"; return 0; }; done; return 1; }
RENDERER="$(find_required wired_vulkan_arm64.dylib "$WD" "$WD/../MacOS")" || { echo "SKIP: Vulkan renderer unavailable"; exit 77; }
GAMECL="$(find_required gameclarm64.dylib "$WD/base" "$WD/../Resources/base" "$WD/../../../base")" || { echo "SKIP: gamecl unavailable"; exit 77; }
GAMESV="$(find_required gamesvarm64.dylib "$WD/base" "$WD/../Resources/base" "$WD/../../../base")" || { echo "SKIP: gamesv unavailable"; exit 77; }
PAX21="$(find_required pax21.sw3z "$WD/base" "$WD/../Resources/base" "$WD/../../../base")" || { echo "SKIP: pax21 unavailable"; exit 77; }

ROOT="$(mktemp -d -t ral-brightness-sweep-XXXXXX 2>/dev/null || mktemp -d)"
HOME_DIR="$ROOT/home"
cleanup() { local s=$?; trap - EXIT INT TERM; [ "${WIRED_KEEP_ARTIFACTS:-0}" = 1 ] || rm -rf "$ROOT"; exit "$s"; }
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM
mkdir -p "$HOME_DIR/base"
cp "$CONTENT/base/pax01.sw3z" "$PAX21" "$GAMECL" "$GAMESV" "$HOME_DIR/base/" || exit 1

CFG="$HOME_DIR/base/ral-brightness-sweep.cfg"
{
	printf '%s\n' 'set r_fullscreen 0' 'set r_mode -1' 'set r_customwidth 1280' 'set r_customheight 720'
	printf '%s\n' 'set vm_game 0' 'set vm_cgame 0' 'set sv_cheats 1' 'set sv_pure 0'
	printf '%s\n' 'set fixedtime 16' 'set r_pinShaderTime 1.0' 'set cg_draw2D 0' 'set cg_drawGun 0' 'set r_hdr 1' 'set r_fbo 1' 'set r_hdrAutoExposure 0'
	printf '%s\n' 'set r_dither 0' 'set r_bloom 0' 'set r_ssao 0' 'set r_smaa 0' 'set r_forwardPlus 0' 'set r_dynamiclight 0'
	printf '%s\n' 'set r_gpuSpeeds 0' 'log renderer.timing debug'
	printf '%s\n' 'set r_drawSunRays 0' 'set r_shadows 0' 'set r_lens 0' 'set r_gpuDecals 0' 'set r_particles 0'
	printf '%s\n' 'set scalarSweep "wait 180; noclip; setviewpos 1052 1432 50 135; wait 90; vstr scalar0"'
	for i in "${!VALUES[@]}"; do
		next=$((i + 1)); value="${VALUES[$i]}"; tag="${value//./_}"
		if [ "$next" -lt "${#VALUES[@]}" ]; then tail="vstr scalar$next"; else tail='echo Q3_BRIGHTNESS_SWEEP_COMPLETE; fixedtime 0; quit'; fi
		printf 'set scalar%s "r_gpuSpeeds 0; r_brightness %s; wait 30; echo Q3_BRIGHTNESS_PERF_%s_BEGIN; r_gpuSpeeds 1; wait 440; r_gpuSpeeds 0; echo Q3_BRIGHTNESS_PERF_%s_END; screenshot ral_brightness_%s png silent; wait 20; %s"\n' "$i" "$value" "$tag" "$tag" "$tag" "$tail"
	done
	printf '%s\n' 'set activeAction "vstr scalarSweep"' 'map arena1'
} > "$CFG"

STDOUT="$ROOT/stdout.log"
python3 "$TIMEOUT_RUNNER" --timeout 90 --kill-after 10 --cwd "$WD" --stdout "$STDOUT" -- "$WIRED" \
	+set fs_basepath "$HOME_DIR" +set fs_homepath "$HOME_DIR" +set fs_game base \
	+set com_automated 1 +set com_noHardReboot 1 +set s_initsound 0 \
	+set r_fullscreen 0 +set r_mode -1 +set r_customwidth 1280 +set r_customheight 720 \
	+set r_vkValidate 0 +set log_file_mode overwrite_synced +exec ral-brightness-sweep.cfg
RC=$?
[ "$RC" -eq 0 ] || { echo "FAIL runtime rc=$RC; retained root: $ROOT"; WIRED_KEEP_ARTIFACTS=1; exit 1; }

QCONSOLE="$HOME_DIR/qconsole.jsonl"
python3 - "$PNG2RAW" "$QCONSOLE" "$HOME_DIR/base/screenshots" "${VALUES[@]}" <<'PYEOF'
import json, pathlib, re, statistics, struct, subprocess, sys
png2raw, log_path, shot_dir, *values = sys.argv[1:]
messages=[]
for line in open(log_path, encoding="utf-8"):
    row=json.loads(line); messages.append(row.get("msg","").rstrip("\n"))
receipt=re.compile(r"window-extent schema=3 requested=1280x720 logical=1280x720 pixels=([1-9][0-9]*)x([1-9][0-9]*) publish-ready=1")
windows=[receipt.fullmatch(m) for m in messages if m.startswith("window-extent ")]
if len(windows)!=1: raise SystemExit(f"FAIL window receipt count={len(windows)}")
pw,ph=map(int,windows[0].groups())
if pw < 1280 or ph < 720 or pw*9 != ph*16: raise SystemExit(f"FAIL non-widescreen window {pw}x{ph}")
if "Q3_BRIGHTNESS_SWEEP_COMPLETE" not in messages: raise SystemExit("FAIL completion marker missing")
perf={}
for value in values:
    tag=value.replace(".","_"); begin=f"Q3_BRIGHTNESS_PERF_{tag}_BEGIN"; end=f"Q3_BRIGHTNESS_PERF_{tag}_END"
    if messages.count(begin)!=1 or messages.count(end)!=1: raise SystemExit(f"FAIL timing markers {tag}")
    lo=messages.index(begin); hi=messages.index(end)
    totals=[]; armed=False
    for msg in messages[lo+1:hi]:
        if msg.startswith("gpu (") and msg.endswith("avg, ms):"): armed=True
        elif armed and msg.startswith("  total="):
            totals.append(float(msg.split("=",1)[1])); armed=False
    if len(totals)<2: raise SystemExit(f"FAIL insufficient Release GPU samples {tag}: {totals}")
    perf[float(value)]=statistics.median(totals)
captures=[]
for value in values:
    tag=value.replace(".","_"); path=pathlib.Path(shot_dir)/f"ral_brightness_{tag}.png"
    if not path.is_file(): raise SystemExit(f"FAIL missing {path.name}")
    data=path.read_bytes()
    width,height=struct.unpack(">II",data[16:24])
    if (width,height) not in ((1280,720),(2560,1440)): raise SystemExit(f"FAIL {path.name} dimensions {width}x{height}")
    raw=subprocess.run([png2raw,str(path)],check=True,stdout=subprocess.PIPE).stdout
    if len(raw)!=width*height*3: raise SystemExit(f"FAIL raw byte count {path.name}")
    captures.append((float(value),width,height,raw))
identity=next(c for c in captures if c[0] == 1.0)
_,width,height,identity_raw=identity
def luminance(raw,pixel):
    i=pixel*3; r,g,b=raw[i:i+3]; return .2126*r+.7152*g+.0722*b
# Freeze the cohort from the authored-identity image. Conditional filtering per
# sample biases a darker image upward by dropping pixels that crossed black.
crop=[p for p in range(width*height)
      if width//10 <= p%width < width*17//20
      and height*3//20 <= p//width < height*17//20]
shadow_pixels=[p for p in crop if 2.0 <= luminance(identity_raw,p) <= 80.0]
if len(shadow_pixels) < len(crop)//4: raise SystemExit("FAIL insufficient fixed shadow cohort")
rows=[]
for value,w,h,raw in captures:
    if (w,h)!=(width,height): raise SystemExit("FAIL screenshot extent drift")
    luma=sorted(luminance(raw,p) for p in shadow_pixels); n=len(luma)
    clipped=sum(max(raw[p*3:p*3+3])>=254 for p in crop)/len(crop)
    rows.append((value,statistics.fmean(luma),luma[n//4],luma[n//2],clipped))
for value,mean,q1,median,clip in rows:
    print(f"MEASURE r_brightness={value:g} mean={mean:.3f} shadow_q1={q1:.3f} median={median:.3f} clipped={clip*100:.3f}%")
for a,b in zip(rows,rows[1:]):
    if not (b[1] > a[1] and b[2] > a[2] and b[3] >= a[3]):
        raise SystemExit(f"FAIL non-monotonic visibility {a} -> {b}")
if rows[-1][4] > .35 or rows[-1][4]-rows[2][4] > .20:
    raise SystemExit(f"FAIL highlight clipping identity={rows[2][4]:.4f} high={rows[-1][4]:.4f}")
identity_gpu=perf[1.0]; high_gpu=perf[5.2]
if high_gpu > identity_gpu * 1.12 + .10:
    raise SystemExit(f"FAIL Release GPU cost identity={identity_gpu:.3f}ms high={high_gpu:.3f}ms")
print(f"MEASURE Release GPU total: identity={identity_gpu:.3f}ms r_brightness=5.2={high_gpu:.3f}ms delta={high_gpu-identity_gpu:+.3f}ms")
print(f"PASS continuous scalar sweep: logical=1280x720 pixels={pw}x{ph}; samples={len(rows)}")
PYEOF
STATUS=$?
[ "$STATUS" -eq 0 ] || { echo "FAIL retained root: $ROOT"; WIRED_KEEP_ARTIFACTS=1; exit "$STATUS"; }
echo "PASS retained root: $ROOT"
