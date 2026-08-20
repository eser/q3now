#!/usr/bin/env bash
# visual-supersample-parity.sh — the captured frame must be the SAME PICTURE
# whether or not supersampling is on, and it must not be blank.
#
# Why this gate exists
# --------------------
# Screenshots come from one of two buffers (tr_backend.c:2698-2699):
#
#   r_ext_supersample 1 -> vk.capture.image, sized by gls.captureWidth/Height
#   r_ext_supersample 0 -> the swapchain, sized by the window's PHYSICAL pixels
#
# Only the first is display-independent. On a HiDPI display the second inherits
# the scale factor, because sdl_glimp.c:233 always requests
# SDL_WINDOW_HIGH_PIXEL_DENSITY (correct for gameplay — it renders at native
# resolution). Measured on a 2.5x Mac: 1440x900 requested, 3600x2196 captured.
#
# That divergence is exactly the kind of thing that silently invalidates a
# visual baseline, so the two paths are pinned against each other here.
#
# The blankness check is not decorative. During 2026-08-16 bring-up every macOS
# capture came back the right SIZE and completely BLACK, and the size alone was
# briefly mistaken for success. A capture gate that only measures dimensions
# will happily bless an empty frame forever.
#
# Usage:   tests/visual-supersample-parity.sh              # full run
#          tests/visual-supersample-parity.sh --self-test  # analyzer only, no engine
# Exit:    0 PASS   1 FAIL   77 SKIP (no engine / no display)

set -uo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
. "$SCRIPT_DIR/lib/wired_paths.sh"

W="${WIDTH:-1440}"
H="${HEIGHT:-900}"

# ── analyzer ────────────────────────────────────────────────────────────────
# Pure function of two PNGs, so it is testable without an engine or a display.

png_dims() {   # -> "WxH"
    python3 - "$1" <<'PY'
import struct,sys
d=open(sys.argv[1],'rb').read()
w,h=struct.unpack('>II',d[16:24]); print(f"{w}x{h}")
PY
}

png_distinct() {  # -> distinct colours over a FIXED-SIZE sample
    python3 - "$1" <<'PY'
import zlib,struct,sys
d=open(sys.argv[1],'rb').read()
idat=b''; i=8
while i < len(d):
    ln=struct.unpack('>I',d[i:i+4])[0]; t=d[i+4:i+8]
    if t==b'IDAT': idat+=d[i+8:i+8+ln]
    i+=12+ln
raw=zlib.decompress(idat)
# Take the SAME NUMBER of samples from both images, not the same stride.
#
# A fixed stride samples a 2880x1800 frame four times as often as a 1440x900
# one, so the larger image reports ~4x the colours purely as an artifact. That
# very artifact was briefly misread here as "the supersample path draws a poorer
# frame" (measured 44 vs 174, a 3.95x ratio that is entirely explained by the
# 4x pixel count). Normalising by sample count makes the comparison mean what
# it claims to.
SAMPLES = 20000
step = max(3, len(raw)//SAMPLES)
print(len({raw[j:j+3] for j in range(0, len(raw), step)}))
PY
}

analyze_parity() {
    local ss="$1" noss="$2" want="$3" rc=0
    echo "==> supersample parity"

    for f in "$ss" "$noss"; do
        [ -s "$f" ] || { echo "  FAIL: missing/empty capture: $f"; return 1; }
    done

    local dss dnoss
    dss="$(png_dims "$ss")"; dnoss="$(png_dims "$noss")"
    echo "    supersample=1 : $dss"
    echo "    supersample=0 : $dnoss"

    # 1. The supersampled capture is the display-independent one; it must be
    #    exactly the requested size on every machine.
    if [ "$dss" != "$want" ]; then
        echo "  FAIL: supersample capture is $dss, expected $want"
        echo "        (vk.capture.image is sized by gls.captureWidth/Height —"
        echo "         tr_init.c:693-709. A mismatch means r_renderScale/"
        echo "         r_renderWidth/Height did not take, or r_fbo is off.)"
        rc=1
    fi

    # 2. NEITHER capture may be blank. This is the check that catches a
    #    correctly-sized black frame.
    local css cnoss
    css="$(png_distinct "$ss")"; cnoss="$(png_distinct "$noss")"
    echo "    distinct(sampled): ss=$css  noss=$cnoss"
    if [ "${css:-0}" -lt 5 ]; then
        echo "  FAIL: supersample capture is blank ($css distinct colours)"
        rc=1
    fi
    if [ "${cnoss:-0}" -lt 5 ]; then
        echo "  FAIL: non-supersample capture is blank ($cnoss distinct colours)"
        rc=1
    fi

    # 3. Both paths must render comparable content. Identical bytes are not
    #    expected — the non-supersampled one may be at a different scale — but a
    #    blank-vs-populated split, or an order-of-magnitude gap in colour
    #    richness, means the two paths disagree about what to draw.
    if [ "${css:-0}" -ge 5 ] && [ "${cnoss:-0}" -ge 5 ]; then
        local lo hi
        if [ "$css" -lt "$cnoss" ]; then lo=$css; hi=$cnoss; else lo=$cnoss; hi=$css; fi
        if [ $(( hi )) -gt $(( lo * 4 )) ]; then
            echo "  FAIL: colour richness differs >4x (ss=$css noss=$cnoss) —"
            echo "        the two capture paths are not drawing the same frame"
            rc=1
        fi
    fi

    [ "$rc" = 0 ] && echo "  PASS: both paths populated, supersample capture is $want"
    return "$rc"
}

# ── self-test ───────────────────────────────────────────────────────────────
if [ "${1:-}" = "--self-test" ]; then
    ST="$(mktemp -d)"; trap 'rm -rf "$ST"' EXIT
    python3 - "$ST" <<'PY'
import zlib,struct,sys,os
d=sys.argv[1]
def png(path,w,h,mode):
    rows=b''
    for y in range(h):
        row=b'\x00'
        for x in range(w):
            if   mode=='blank': row+=bytes((0,0,0))
            elif mode=='rich':  row+=bytes(((x*7+y*13)%256,(x*3)%256,(y*5)%256))
            else:               row+=bytes(((x//64)%2*255,0,0))   # 'poor': 2 colours
        rows+=row
    def chunk(t,b):
        c=t+b; return struct.pack('>I',len(b))+c+struct.pack('>I',zlib.crc32(c))
    open(os.path.join(d,path),'wb').write(
        b'\x89PNG\r\n\x1a\n'
        +chunk(b'IHDR',struct.pack('>IIBBBBB',w,h,8,2,0,0,0))
        +chunk(b'IDAT',zlib.compress(rows))+chunk(b'IEND',b''))
for n,w,h,m in [('good_ss.png',64,40,'rich'),('good_noss.png',96,60,'rich'),
                ('blank_ss.png',64,40,'blank'),('blank_noss.png',96,60,'blank'),
                ('small_ss.png',32,20,'rich'),('poor_noss.png',96,60,'poor')]:
    png(n,w,h,m)
PY
    n=0; fails=0
    check() { # name  ss  noss  want  expect_rc
        n=$((n+1))
        analyze_parity "$ST/$2" "$ST/$3" "$4" >/dev/null 2>&1; local got=$?
        if [ "$got" = "$5" ]; then printf '  ok   %-28s rc=%s\n' "$1" "$got"
        else printf '  FAIL %-28s rc=%s want=%s\n' "$1" "$got" "$5"; fails=$((fails+1)); fi
    }
    check clean-accepted        good_ss.png  good_noss.png  64x40 0
    check both-blank-rejected   blank_ss.png blank_noss.png 64x40 1
    check ss-blank-rejected     blank_ss.png good_noss.png  64x40 1
    check noss-blank-rejected   good_ss.png  blank_noss.png 64x40 1
    check wrong-size-rejected   small_ss.png good_noss.png  64x40 1
    check richness-gap-rejected good_ss.png  poor_noss.png  64x40 1
    check missing-file-rejected nope.png     good_noss.png  64x40 1
    if [ "$fails" = 0 ]; then
        echo "==> SELF-TEST PASS: clean accepted, $((n-1)) mutations rejected"; exit 0
    fi
    echo "==> SELF-TEST FAIL: $fails/$n wrong"; exit 1
fi

# ── product run ─────────────────────────────────────────────────────────────
[ -n "${WIRED_BINARY:-}" ] && [ -x "$WIRED_BINARY" ] || {
    echo "SKIP: GUI engine binary not found (see GAME-DATA.md §4)"; exit 77; }

OUT="$(mktemp -d)"; trap 'rm -rf "$OUT"' EXIT

# SCENE selects what is on screen when the shot is taken.
#
#   attract  (default) the menu/attract screen — pure WiredUI, every size
#            resolved through WUI_Resolve.
#   loading  the map-load screen. The interesting case, because it mixes the
#            two sizing worlds inside ONE element: cl_loading_ui.c passes the
#            .wui rect through for POSITION, but computes type and padding
#            straight off cls.glconfig (LOADING_FONT_* = vidHeight * k,
#            pad = vidWidth * k). Those two agree only while the render target
#            tracks the window — and r_renderScale, which this gate sets, is
#            exactly the case where it does not. See TASK-200.
SCENE="${SCENE:-attract}"

capture_at() {  # $1 = supersample 0|1, $2 = destination png
    local ss="$1" dest="$2"
    local shots="$WIRED_BASE/screenshots"
    mkdir -p "$shots"
    local before; before="$(ls -t "$shots" 2>/dev/null | head -1 || echo "")"

    local scene_cmds
    case "$SCENE" in
        loading)
            # Shoot DURING the load: `map` returns once loading starts, so the
            # screenshot lands while the loading screen is what is being drawn.
            # The wait is a settle, not a completion barrier.
            scene_cmds=$'map arena1\nwait 40\nscreenshot\nwait 200\nquit' ;;
        attract)
            scene_cmds=$'set attract_delay 0\nattract_restart\nwait 200\nwui_test_keydown 32\nwait 400\nscreenshot\nwait 200\nquit' ;;
        *)
            echo "unknown SCENE '$SCENE' (want attract|loading)" >&2; return 1 ;;
    esac

    local cfg="$WIRED_BASE/supersample_parity_${ss}.cfg"
    cat > "$cfg" <<EOF
set r_mode -1
set r_customwidth $W
set r_customheight $H
set r_fullscreen 0
set r_fbo 1
set r_renderWidth $W
set r_renderHeight $H
set r_renderScale 1
set r_ext_supersample $ss
vid_restart
wait 300
$scene_cmds
EOF
    ( cd "$WIRED_BASE/.." && "$WIRED_BINARY" +exec "$(basename "$cfg")" ) >/dev/null 2>&1 || true

    local new; new="$(ls -t "$shots" 2>/dev/null | grep -v "^${before}\$" | head -1)"
    [ -n "$new" ] || return 1
    cp "$shots/$new" "$dest"
}

capture_at 1 "$OUT/ss.png"   || { echo "SKIP: no screenshot with supersample on";  exit 77; }
capture_at 0 "$OUT/noss.png" || { echo "SKIP: no screenshot with supersample off"; exit 77; }

analyze_parity "$OUT/ss.png" "$OUT/noss.png" "${W}x${H}" || exit 1
exit 0
