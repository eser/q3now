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

set -uo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

W="${VR_W:-1280}"; H="${VR_H:-720}"
BLK_N="${BLK_N:-24}"
# Max allowed mean-per-channel divergence (0-255) of a live corner from its
# blessed golden corner. Corners are byte-stable launch-to-launch, so the only
# spread is SMAA/dither sub-unit jitter; 6 sits clear above that and well below a
# real square (which shifts a corner by tens of units — even a near-black square
# on a near-black menu differs by the fill's own brightness, ~7+).
CORNER_DELTA="${CORNER_DELTA:-6.0}"
GOLDEN="${GOLDEN:-$REPO_ROOT/tests/golden/wiredui_corners.txt}"

# Analysis program written to a temp .py (NOT a `python3 -` heredoc): the heredoc
# would BE stdin, leaving nothing for the raw image the program reads from stdin.
ASSERT_PY="$(mktemp -t wired-corner-XXXXXX.py 2>/dev/null || echo /tmp/wired-corner.py)"
SELF_TMP=""
cleanup() { rm -f "$ASSERT_PY"; [ -n "$SELF_TMP" ] && rm -rf "$SELF_TMP"; }
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
WIRED="${1:-$REPO_ROOT/build/debug/wired.x64.exe}"
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
HOME_NATIVE="$HOME_DIR"
case "$(uname -s)" in MINGW*|MSYS*|CYGWIN*) HOME_NATIVE="$(cygpath -w "$HOME_DIR")" ;; esac

MODE="verify"; [ "${SMOKE_UPDATE_GOLDEN:-0}" = "1" ] && MODE="bless"
echo "==> WiredUI corner check (SMAA corner-squares, corner-vs-golden, mode=$MODE): $WIRED"

(
    cd "$WIRED_DIR" || exit 1
    timeout 90 "$WIRED" \
        +set fs_homepath "$HOME_NATIVE" \
        +set com_automated 1 +set s_initsound 0 \
        +set r_fullscreen 0 +set r_mode -1 +set r_customwidth "$W" +set r_customheight "$H" \
        +set r_smaa 1 \
        +wait 80 +wui_push main +wait 60 +screenshot cornercap +wait 30 +quit \
        >/dev/null 2>&1
)

SHOT="$(ls -t "$HOME_DIR/base/screenshots/"*.png 2>/dev/null | head -1)"
if [ -z "$SHOT" ]; then echo "FAIL: no menu screenshot captured"; exit 1; fi

mkdir -p "$(dirname "$GOLDEN")"
"$PNG2RAW" "$SHOT" | python3 "$ASSERT_PY" "$W" "$H" "$BLK_N" "$CORNER_DELTA" "$MODE" "$GOLDEN"
RC=$?
if [ "$MODE" = "bless" ]; then
    [ "$RC" -eq 0 ] && echo "==> WiredUI corner golden blessed -> $GOLDEN" || echo "==> bless FAILED"
else
    [ "$RC" -eq 0 ] && echo "==> WiredUI corner check: PASS" || echo "==> WiredUI corner check: FAIL"
fi
exit $RC
