#!/usr/bin/env bash
# wiredui-corner-check.sh — computed check for the SMAA corner-squares fix.
#
# Symptom: four small squares in the screen corners over the menu that DIFFER
# from the blessed-clean menu (originally near-black mis-cleared voids). The real
# menu background is NOT uniform — it carries art/gradient/dither that varies
# corner-to-corner — so "corner vs an inboard fill sample" has false positives
# (a legit panel reaching a corner reads as a square), and an absolute brightness
# threshold can't catch a black square on a near-black menu. The robust invariant
# is therefore "each corner matches the BLESSED-CLEAN menu corner" — a per-corner
# golden, the same model the smoke gate uses for the world render.
#
# Each corner is a 24x24 block reduced to its mean RGB; the golden holds the four
# blessed triples. PASS = every live corner is within CORNER_DELTA of its golden.
# A corner square shifts that corner's mean away from the golden → FAIL, on a menu
# of ANY brightness, with no false positive from legit (stable) menu art. Corners
# are byte-stable across launches (verified), so the golden is exact.
#
# Bless:      SMOKE_UPDATE_GOLDEN=1 tests/wiredui-corner-check.sh <wired>
# Self-test:  tests/wiredui-corner-check.sh --self-test   (gate-has-teeth, no engine)
#
# Usage:  tests/wiredui-corner-check.sh [path-to-wired]
# Exit:   0 PASS   1 FAIL   77 SKIP
#
# On macOS the binary defaults to the installed .app — a flat build/ tree cannot
# run the engine at all (see the WIRED_DEFAULT note below). To capture a menu
# built from working-tree content:
#
#   WIRED_CONTENT_ROOT="$PWD/build" WIRED_KEEP_ARTIFACTS=1 \
#     CORNER_MENU=main tests/wiredui-corner-check.sh
#
# For attract panels use CORNER_ATTRACT=<n> instead of CORNER_MENU — pushing them
# captures a blank frame by design. See the CORNER_ATTRACT note below.

set -uo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
# shellcheck source=tests/lib/wired_paths.sh
. "$SCRIPT_DIR/lib/wired_paths.sh"

W="${VR_W:-1280}"; H="${VR_H:-720}"
BLK_N="${BLK_N:-24}"
# Max allowed mean-per-channel divergence (0-255) of a live corner from its
# blessed golden corner. Corners are byte-stable launch-to-launch, so the only
# spread is SMAA/dither sub-unit jitter; 6 sits clear above that and well below a
# real square (which shifts a corner by tens of units — even a near-black square
# on a near-black menu differs by the fill's own brightness, ~7+).
CORNER_DELTA="${CORNER_DELTA:-6.0}"
# Corner goldens are PER-PLATFORM: the menu renders through different Vulkan
# stacks (native vs MoltenVK) at different backing scales, so one platform's
# blessed corners are not another's baseline. The legacy unsuffixed file is
# the pre-split Windows bless and stays the Windows fallback.
case "$(uname -s)" in
    Darwin)               GOLDEN_PLAT="darwin-$(uname -m)" ;;
    MINGW*|MSYS*|CYGWIN*) GOLDEN_PLAT="windows-$(uname -m)" ;;
    *)                    GOLDEN_PLAT="linux-$(uname -m)" ;;
esac
GOLDEN_DEFAULT="$REPO_ROOT/tests/golden/wiredui_corners.$GOLDEN_PLAT.txt"
if [ ! -e "$GOLDEN_DEFAULT" ] && [ -e "$REPO_ROOT/tests/golden/wiredui_corners.txt" ]; then
    case "$GOLDEN_PLAT" in windows-*) GOLDEN_DEFAULT="$REPO_ROOT/tests/golden/wiredui_corners.txt" ;; esac
fi
GOLDEN="${GOLDEN:-$GOLDEN_DEFAULT}"

# Analysis program written to a temp .py (NOT a `python3 -` heredoc): the heredoc
# would BE stdin, leaving nothing for the raw image the program reads from stdin.
# mktemp needs XXXXXX at the END of the template; `-t wired-corner-XXXXXX.py`
# put a suffix after it, which on BSD/macOS is taken as a literal name — the
# file is created verbatim and the NEXT run refuses because it already exists.
# Generate first, then add the .py suffix (same fix as smoke.sh).
# CORNER_MENU — which menu to push before the shot. The corner golden is blessed
# against "main", so the gate's own assertion only means anything there; this is
# an override for USING the capture machinery on another menu (e.g. to produce a
# before/after of a widget change). Point it elsewhere and the corner verdict is
# noise — read the retained frame, not the deltas.
CORNER_MENU="${CORNER_MENU:-main}"
# CORNER_ATTRACT=<n> — capture the n'th attract playlist item (1-based) instead of
# pushing a menu. wui_push is the WRONG door for attract panels: it makes the panel
# the ACTIVE menu, which sets ui_menuUp, and attract content hides itself on that
# state by design (attract_leaderboard gates six items on it; attract_brand's
# wordmark, stamp and poster copy likewise). Pushed, they capture as bare
# background — a frame that proves nothing.
#
# In the real flow the scheduler sets panel->visible directly and never touches the
# menu stack (cl_wired_attract.c), so no menu is active and the panel draws in
# full. The automated launch may still have the boot menu up, so do not assume
# attract_delay=0 will win that race: call attract_start explicitly, then skip
# forward to the item you want. Item order is modfiles/scripts/attract.lua —
# 1 = attract_brand, 2 = attract_leaderboard.
#
# Panels not on the playlist stay unreachable, attract_demo_overlay among them: it
# needs an attract-owned demo playing (WiredAttract_IsDemoOverlayActive), and the
# demo item is currently commented out of the playlist.
CORNER_ATTRACT="${CORNER_ATTRACT:-}"

ASSERT_PY="$(mktemp "$WIRED_TMP/wired-corner-XXXXXX")".py
SELF_TMP=""
# WIRED_KEEP_ARTIFACTS=1 keeps the captured menu frame and prints where it is.
# A corner delta tells you THAT a corner moved; deciding whether the new corner is
# a legitimate drawing or the very corner-square this gate exists to catch needs
# the frame itself. Same affordance the ral-*-check.sh gates offer.
cleanup() {
	rm -f "$ASSERT_PY"
	if [ -n "$SELF_TMP" ]; then
		if [ "${WIRED_KEEP_ARTIFACTS:-0}" = 1 ]; then
			echo "  retained: $SELF_TMP"
		else
			rm -rf "$SELF_TMP"
		fi
	fi
}
trap cleanup EXIT INT TERM
cat > "$ASSERT_PY" <<'PYEOF'
import sys
# argv: W H blkN delta mode goldenPath ; raw RGB (W*H*3 top-down) on stdin.
W,H,n,delta,mode,golden = int(sys.argv[1]),int(sys.argv[2]),int(sys.argv[3]),float(sys.argv[4]),sys.argv[5],sys.argv[6]
raw = sys.stdin.buffer.read()
if len(raw) < W*H*3:
    print(f"  FAIL: decoded {len(raw)} bytes < {W*H*3} for {W}x{H}"); sys.exit(2)
def blk(x0,y0):
    sr=sg=sb=0; c=0
    for yy in range(max(0,y0), min(H,y0+n)):
        base=yy*W*3
        for xx in range(max(0,x0), min(W,x0+n)):
            i=base+xx*3; sr+=raw[i]; sg+=raw[i+1]; sb+=raw[i+2]; c+=1
    return (sr/c, sg/c, sb/c) if c else (0.0,0.0,0.0)
order = ["TL","TR","BL","BR"]
live = {"TL":blk(0,0), "TR":blk(W-n,0), "BL":blk(0,H-n), "BR":blk(W-n,H-n)}
if mode == "bless":
    with open(golden,"w") as f:
        for k in order:
            r,g,b = live[k]; f.write(f"{k} {r:.3f} {g:.3f} {b:.3f}\n")
    print("  blessed corner golden: " + ", ".join(f"{k}=({live[k][0]:.0f},{live[k][1]:.0f},{live[k][2]:.0f})" for k in order))
    sys.exit(0)
# verify mode: compare each live corner to the golden triple
try:
    gold={}
    for ln in open(golden):
        p=ln.split()
        if len(p)==4: gold[p[0]]=(float(p[1]),float(p[2]),float(p[3]))
except FileNotFoundError:
    print(f"  FAIL: no corner golden at {golden} — bless first (SMOKE_UPDATE_GOLDEN=1)"); sys.exit(2)
def d(a,b): return (abs(a[0]-b[0])+abs(a[1]-b[1])+abs(a[2]-b[2]))/3.0
worst=("",-1.0); bad=[]
detail=[]
for k in order:
    if k not in gold:
        print(f"  FAIL: golden missing corner {k}"); sys.exit(2)
    dd=d(live[k],gold[k]); detail.append(f"{k}:Δ{dd:.1f}")
    if dd>worst[1]: worst=(k,dd)
    if dd>delta: bad.append((k,dd))
print(f"  corner-vs-golden deltas: {', '.join(detail)}  (tol={delta:g})")
if bad:
    print("  FAIL: corner(s) diverge from the blessed-clean menu: "
          + ", ".join(f"{k}(Δ{dd:.1f})" for k,dd in bad)
          + " — a corner square the clean menu does not have")
    sys.exit(1)
print(f"  PASS: all 4 corners match the blessed-clean menu (worst Δ{worst[1]:.1f} <= {delta:g}) — no corner squares")
sys.exit(0)
PYEOF

# ── self-test: prove the gate PASSES clean and FAILS on a deliberate corner sq ──
if [ "${1:-}" = "--self-test" ]; then
    echo "==> WiredUI corner check SELF-TEST (gate-has-teeth)"
    SELF_TMP="$(mktemp -d -t wired-cornerst-XXXXXX 2>/dev/null || mktemp -d)"
    # Generator: a DARK menu fill (6,6,8) (the old absolute gate's blind spot),
    # optionally with one corner forced to a black square. Writes raw to a file.
    cat > "$SELF_TMP/gen.py" <<'GENPY'
import sys
W,H,n,mode = int(sys.argv[1]),int(sys.argv[2]),int(sys.argv[3]),sys.argv[4]
buf=bytearray((6,6,8)*(W*H))
if mode=="defect":
    for yy in range(0,n):
        b=yy*W*3
        for xx in range(0,n):
            i=b+xx*3; buf[i]=buf[i+1]=buf[i+2]=0   # black corner square on near-black fill
sys.stdout.buffer.write(bytes(buf))
GENPY
    SG="$SELF_TMP/self_golden.txt"
    python3 "$SELF_TMP/gen.py" "$W" "$H" "$BLK_N" clean > "$SELF_TMP/clean.raw"
    python3 "$SELF_TMP/gen.py" "$W" "$H" "$BLK_N" defect > "$SELF_TMP/defect.raw"
    echo "  -- bless golden from the CLEAN dark frame --"
    python3 "$ASSERT_PY" "$W" "$H" "$BLK_N" "$CORNER_DELTA" bless "$SG" < "$SELF_TMP/clean.raw"
    rc_clean=0; rc_defect=0
    echo "  -- verify CLEAN frame against that golden (expect PASS) --"
    python3 "$ASSERT_PY" "$W" "$H" "$BLK_N" "$CORNER_DELTA" verify "$SG" < "$SELF_TMP/clean.raw" || rc_clean=$?
    echo "  -- verify DEFECT frame (black corner square) against the SAME golden (expect FAIL) --"
    python3 "$ASSERT_PY" "$W" "$H" "$BLK_N" "$CORNER_DELTA" verify "$SG" < "$SELF_TMP/defect.raw" || rc_defect=$?
    if [ "$rc_clean" -eq 0 ] && [ "$rc_defect" -ne 0 ]; then
        echo "==> SELF-TEST PASS: gate accepts clean AND rejects a corner square on a dark menu (it has teeth)"
        exit 0
    fi
    echo "==> SELF-TEST FAIL: clean_rc=$rc_clean (want 0), defect_rc=$rc_defect (want !=0) — gate cannot catch its target bug"
    exit 1
fi

# ── live gate: capture a real menu frame, then bless or verify ─────────────────
# Default binary. On macOS the engine can only run from the INSTALLED .app: the
# renderer is loaded from FS_GetInstallBinaryPath(), which appends Contents/MacOS
# (qcommon.h:951-952), and a flat build/ tree has no such subdirectory — every
# configuration there dies with "Failed to load renderer wired_vulkan_arm64.dylib"
# before reaching a menu, overriding fs_installpath included. Makefile:994 launches
# the same way. Elsewhere the flat build dir IS the install layout, so it stands.
WIRED_DEFAULT="$REPO_ROOT/build/wired.x64.exe"
if [ "$(uname -s)" = "Darwin" ]; then
    for _app in /Applications/q3now-preview.app /Applications/q3now.app; do
        if [ -x "$_app/Contents/MacOS/wired.$(uname -m)" ]; then
            WIRED_DEFAULT="$_app/Contents/MacOS/wired.$(uname -m)"; break
        fi
    done
fi
WIRED="${1:-$WIRED_DEFAULT}"
if [ ! -x "$WIRED" ] && [ -x "$WIRED.exe" ]; then WIRED="$WIRED.exe"; fi
if [ ! -x "$WIRED" ]; then echo "SKIP: wired binary not found: $WIRED"; exit 77; fi
case "$WIRED" in /*) : ;; *) WIRED="$PWD/$WIRED" ;; esac
WIRED_DIR="$(cd "$(dirname "$WIRED")" && pwd)"
WIRED="$WIRED_DIR/$(basename "$WIRED")"

PNG2RAW="${PNG2RAW:-$REPO_ROOT/tools/png2raw/png2raw}"
if [ ! -x "$PNG2RAW" ] && [ -x "$PNG2RAW.exe" ]; then PNG2RAW="$PNG2RAW.exe"; fi
if [ ! -x "$PNG2RAW" ]; then echo "SKIP: png2raw not found at $PNG2RAW"; exit 77; fi

HOME_PARENT="$(mktemp -d -t wired-corner-XXXXXX 2>/dev/null || mktemp -d)"
HOME_DIR="$HOME_PARENT/q3now-preview"
SELF_TMP="$HOME_PARENT"   # fold into the EXIT cleanup
mkdir -p "$HOME_DIR/base/screenshots"

# WIRED_CONTENT_ROOT — run against a pak you just built instead of the installed
# one.
#
# Without this the gate only sets fs_homepath, so the engine loads its content
# from the INSTALL path and a freshly built pak is never seen. That is silent and
# it lies in the worst direction: edit a .wui, rebuild the pak, run the gate, and
# the "before" and "after" frames come out identical — which reads as "the change
# is harmless" when it actually means "the change never loaded". Measured by
# renaming a button's label and watching the old label still render.
#
# Staging the pak into the sandbox home and pointing fs_basepath at it is the
# same recipe the ral-*-check.sh gates use.
#
# Do NOT also override fs_installpath. The renderer is not loaded from basepath:
# CL_InitRef builds its path from FS_GetInstallBinaryPath(), which the engine
# derives from argv[0] and which appends Contents/MacOS on macOS. Left alone
# that self-detection is correct. Pinning fs_installpath to the flat build dir
# makes it look for build/Contents/MacOS/wired_vulkan_arm64.dylib, which
# does not exist, and the engine dies before reaching the menu:
#     Sys_Error: Failed to load renderer wired_vulkan_arm64.dylib
# Measured both ways. Only the content path belongs in the sandbox.
if [ -n "${WIRED_CONTENT_ROOT:-}" ]; then
    for _pak in "$WIRED_CONTENT_ROOT"/base/*.sw3z "$WIRED_CONTENT_ROOT"/base/*.pk3; do
        [ -e "$_pak" ] && cp "$_pak" "$HOME_DIR/base/"
    done
fi

HOME_NATIVE="$HOME_DIR"
case "$(uname -s)" in MINGW*|MSYS*|CYGWIN*) HOME_NATIVE="$(cygpath -w "$HOME_DIR")" ;; esac

MODE="verify"; [ "${SMOKE_UPDATE_GOLDEN:-0}" = "1" ] && MODE="bless"
echo "==> WiredUI corner check (SMAA corner-squares, corner-vs-golden, mode=$MODE): $WIRED"

# Keep the run's output. Discarding it makes a CRASH and a menu-that-never-opened
# report identically as "no menu screenshot captured", which hides the one line
# that says which — e.g. "Sys_Error: Failed to load renderer ...". Kept in the
# sandbox and echoed only on failure, so a passing run stays quiet.
RUN_LOG="$HOME_PARENT/engine.log"

# How the surface is brought up. Menus get pushed; attract items get skipped to.
# Start explicitly after UI initialization; each skip advances one item. Skipping
# beats waiting out the item's own duration (brand is 6s), because +wait counts
# FRAMES, not seconds — how many frames 6s buys varies by machine.
if [ -n "$CORNER_ATTRACT" ]; then
	SURFACE_ARGS="+set attract_enabled 1 +set attract_delay 0 +wait 20 +attract_start +wait 80"
    _n=1
	while [ "$_n" -lt "$CORNER_ATTRACT" ]; do
		# Transition duration is wall-clock based (500 ms by default), while
		# `wait` counts frames. Give even a high-refresh automated run enough
		# frames to leave TRANSITIONING and dispatch the requested panel.
		SURFACE_ARGS="$SURFACE_ARGS +attract_skip +wait 240"
		_n=$(( _n + 1 ))
	done
	SURFACE_ARGS="$SURFACE_ARGS +attract_status"
    echo "  surface: attract playlist item $CORNER_ATTRACT (real attract flow, no wui_push)"
else
    # attract_enabled 0 for the menu path. main declares `backdrop dim`, which
    # deliberately lets the attract reel show through, and the reel's left spine
    # runs into the sampled corners — TL/BL came out Δ28/Δ26 against a golden
    # blessed without it, so the gate failed every run for a reason that had
    # nothing to do with corner squares. Silencing the reel puts the measurement
    # back in the condition the golden was blessed under (TL Δ0.0).
    SURFACE_ARGS="+set attract_enabled 0 +wait 80 +wui_push $CORNER_MENU +wait 60"
fi

(
    cd "$WIRED_DIR" || exit 1
    # shellcheck disable=SC2086  # SURFACE_ARGS is a deliberate argument list
    timeout 90 "$WIRED" \
        +set fs_homepath "$HOME_NATIVE" \
        ${WIRED_CONTENT_ROOT:+ +set fs_basepath "$HOME_NATIVE"} \
        +set com_automated 1 +set s_initsound 0 \
        +set r_fullscreen 0 +set r_mode -1 +set r_customwidth "$W" +set r_customheight "$H" \
        +set r_layoutDump "${WIRED_LAYOUT_DUMP:-0}" \
        +set r_smaa 1 \
        $SURFACE_ARGS +screenshot cornercap +wait 30 +quit \
        >"$RUN_LOG" 2>&1
)

SHOT="$(ls -t "$HOME_DIR/base/screenshots/"*.png 2>/dev/null | head -1)"
if [ -z "$SHOT" ]; then
    echo "FAIL: no menu screenshot captured"
    if grep -qiE 'Sys_Error|FATAL|Assertion failed' "$RUN_LOG" 2>/dev/null; then
        echo "  the engine died before the shot — last lines:"
        grep -iE 'Sys_Error|FATAL|Assertion failed|Failed to load' "$RUN_LOG" | tail -4 | sed 's/^/    /'
    else
        echo "  the engine ran but produced no shot — is '$CORNER_MENU' pushable with wui_push?"
        tail -4 "$RUN_LOG" 2>/dev/null | sed 's/^/    /'
    fi
    exit 1
fi

# On HiDPI the requested WxH is the LOGICAL size — the capture backs at the
# physical pixel size (a 1280 request backs at 2560 on a 2x display). Sampling
# with the requested dims would read the wrong pixels entirely, so take the
# REAL dims from the PNG header (IHDR width/height, bytes 16..24, big-endian).
ACTUAL_DIMS="$(python3 -c 'import struct,sys; d=open(sys.argv[1],"rb").read(26); print(*struct.unpack(">II", d[16:24]))' "$SHOT" 2>/dev/null)"
if [ -n "$ACTUAL_DIMS" ]; then
    W="${ACTUAL_DIMS%% *}"; H="${ACTUAL_DIMS##* }"
    echo "  capture: ${W}x${H} (PNG-derived; golden: $(basename "$GOLDEN"))"
fi

mkdir -p "$(dirname "$GOLDEN")"
if [ -n "$CORNER_ATTRACT" ]; then
	echo "  PASS: attract panel captured (corner golden is main-menu-specific; visual/content checks own this frame)"
	echo "==> WiredUI attract capture: PASS"
	exit 0
fi
"$PNG2RAW" "$SHOT" | python3 "$ASSERT_PY" "$W" "$H" "$BLK_N" "$CORNER_DELTA" "$MODE" "$GOLDEN"
RC=$?
if [ "$MODE" = "bless" ]; then
    [ "$RC" -eq 0 ] && echo "==> WiredUI corner golden blessed -> $GOLDEN" || echo "==> bless FAILED"
else
    [ "$RC" -eq 0 ] && echo "==> WiredUI corner check: PASS" || echo "==> WiredUI corner check: FAIL"
fi
exit $RC
