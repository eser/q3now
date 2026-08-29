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
PNG2RAW="${PNG2RAW:-$WIRED_SOURCE/tools/png2raw/png2raw}"

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
    "$PNG2RAW" "$1" | python3 -c '
import sys
raw=sys.stdin.buffer.read()
samples=20000
pixels=len(raw)//3
step=max(1,pixels//samples)
print(len({raw[i*3:i*3+3] for i in range(0,pixels,step)}))'
}

analyze_menu_geometry() {
    python3 - "$1" "$2" "$PNG2RAW" <<'PY'
import struct,subprocess,sys

def load(path, decoder):
    data=open(path,'rb').read(24)
    w,h=struct.unpack('>II',data[16:24])
    raw=subprocess.check_output([decoder,path])
    return w,h,raw

def authored_fill_box(image):
    w,h,raw=image
    target=(58,50,42)
    xs=[]; ys=[]
    for y in range(h//10, h*9//10):
        row=y*w*3
        # The authored pause panel occupies the central third.  Restricting the
        # probe to it prevents similarly coloured world texels outside the
        # opaque panel from widening the measured bounding box.
        for x in range(w//3, w*2//3):
            i=row+x*3
            if max(abs(raw[i+j]-target[j]) for j in range(3)) <= 5:
                xs.append(x); ys.append(y)
    if len(xs) < 1000:
        raise SystemExit(f"FAIL: authored menu fill not measurable ({len(xs)} pixels)")
    return (min(xs),min(ys),max(xs),max(ys),len(xs))

a=authored_fill_box(load(sys.argv[1],sys.argv[3]))
b=authored_fill_box(load(sys.argv[2],sys.argv[3]))
edge_delta=max(abs(a[i]-b[i]) for i in range(4))
count_delta=abs(a[4]-b[4])/max(a[4],b[4])
if edge_delta > 4 or count_delta > .05:
    raise SystemExit(f"FAIL: supersample changed menu geometry: ss={a} noss={b}")
print(f"    menu authored-fill geometry: ss={a} noss={b}")
PY
}

analyze_parity() {
    local ss="$1" noss="$2" want="$3" mode="${4:-generic}" rc=0
    echo "==> supersample parity"

    for f in "$ss" "$noss"; do
        [ -s "$f" ] || { echo "  FAIL: missing/empty capture: $f"; return 1; }
    done

    local dss dnoss
    dss="$(png_dims "$ss")"; dnoss="$(png_dims "$noss")"
    echo "    supersample=1 : $dss"
    echo "    supersample=0 : $dnoss"

    # 1. Both paths are presentation artifacts. They must use the same output
    #    extent, which may be an integer HiDPI multiple of the logical window.
    local got_w got_h want_w want_h
    got_w="${dss%x*}"; got_h="${dss#*x}"
    want_w="${want%x*}"; want_h="${want#*x}"
    if [ "$dss" != "$dnoss" ] || [ $((got_w % want_w)) -ne 0 ] \
       || [ $((got_h % want_h)) -ne 0 ] \
       || [ $((got_w / want_w)) -ne $((got_h / want_h)) ]; then
        echo "  FAIL: capture extents ss=$dss noss=$dnoss are not one shared presentation multiple of $want"
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

    if [ "$mode" = menu ] && ! analyze_menu_geometry "$ss" "$noss"; then
        rc=1
    fi

    [ "$rc" = 0 ] && echo "  PASS: both paths populated at shared presentation extent $dss"
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
for n,w,h,m in [('good_ss.png',64,40,'rich'),('good_noss.png',64,40,'rich'),
                ('blank_ss.png',64,40,'blank'),('blank_noss.png',64,40,'blank'),
                ('small_ss.png',32,20,'rich'),('poor_noss.png',64,40,'poor')]:
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

# WIRED_KEEP_ARTIFACTS=1 keeps the two captures and prints where they are, the
# same affordance the ral-*-check.sh gates offer. A size/richness verdict tells
# you THAT two frames differ; investigating WHY needs the frames themselves.
# Never write archived cvars or screenshots into the player's real home.  Stage
# the currently selected game's paks into an isolated home and pass it to the
# engine explicitly; merely changing cwd does not change fs_homepath.
RUN_ROOT="$(mktemp -d "$WIRED_TMP/q3now-supersample-ui-XXXXXX")"
WIRED_TMP="$RUN_ROOT"
WIRED_HOME="$(wired_isolated_home supersample-ui)"
WIRED_BASE="$WIRED_HOME/base"
OUT="$RUN_ROOT/evidence"
mkdir -p "$OUT"
if [ "${WIRED_KEEP_ARTIFACTS:-0}" = 1 ]; then
    trap 'echo "  retained: $OUT"' EXIT
else
    trap 'rm -rf "$RUN_ROOT"' EXIT
fi

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

# RENDER_W / RENDER_H — decouple the render target from the window.
#
# Everything above keeps them equal, because parity means "same picture from
# two capture paths". Setting them smaller asks a DIFFERENT question, and it is
# the one TASK-200 needs answered:
#
#   WiredUI derives dpiScale = vidHeight / vidHeightLogical. vidHeight follows
#   the RENDER TARGET (tr_init.c:693-696 assigns it from r_renderHeight);
#   vidHeightLogical follows the WINDOW (SDL_GetWindowSize). While the two
#   match, so do the two sizing worlds inside the loading screen — position
#   from the .wui rect, type and padding from cls.glconfig. Shrink the render
#   target alone and they must come apart: authored positions hold, glyphs and
#   padding shrink with the target.
#
#   SCENE=loading RENDER_W=720 RENDER_H=450 tests/visual-supersample-parity.sh
#
# Left at the window size by default so the parity gate's own meaning is
# unchanged; this is an investigation lever, not a new assertion.

capture_at() {  # $1 = supersample 0|1, $2 = destination png
    local ss="$1" dest="$2"
    local shots="$WIRED_BASE/screenshots"
    mkdir -p "$shots"
    local marker; marker="$(wired_shot_marker)"

    local scene_cmds
    case "$SCENE" in
        loading)
            # Shoot DURING the load: `map` returns once loading starts, so the
            # screenshot lands while the loading screen is what is being drawn.
            # The wait is a settle, not a completion barrier.
            scene_cmds=$'map arena1\nwait 40\nscreenshot\nwait 200\nquit' ;;
        attract)
            scene_cmds=$'set attract_delay 0\nattract_restart\nwait 200\nwui_test_keydown 32\nwait 400\nscreenshot\nwait 200\nquit' ;;
        menu)
            # Stable in-game menu: unlike the attract reel this has no
            # phase-dependent panel transition, so off/on captures prove UI
            # geometry rather than accidentally sampling two animation times.
            scene_cmds=$'map arena1\nwaitForMap\nwait 90\nwui_push ingame\nwait 90\nscreenshot\nwait 30\nquit' ;;
        *)
            echo "unknown SCENE '$SCENE' (want attract|loading|menu)" >&2; return 1 ;;
    esac

    # RENDER_W/H default to the window size, which is what the parity check
    # wants: same picture, two capture paths. Setting them SMALLER is the
    # separate question this gate can also answer — see RENDER_W below.
    local rw="${RENDER_W:-$W}" rh="${RENDER_H:-$H}"

    local cfg="$WIRED_BASE/supersample_parity_${ss}.cfg"
    cat > "$cfg" <<EOF
set r_mode -1
set r_customwidth $W
set r_customheight $H
set r_fullscreen 0
set r_fbo 1
set r_renderWidth $rw
set r_renderHeight $rh
set r_renderScale 1
set r_ext_supersample $ss
vid_restart
wait 300
$scene_cmds
EOF
    ( cd "$WIRED_BASE/.." && "$WIRED_BINARY" \
        +set fs_homepath "$WIRED_HOME" \
        +set fs_installpath "$WIRED_INSTALL" \
        +exec "$(basename "$cfg")" ) >/dev/null 2>&1 || true

    local new; new="$(wired_newest_shot "$marker" "$shots")" || return 1
    cp "$new" "$dest"
}

capture_at 1 "$OUT/ss.png"   || { echo "SKIP: no screenshot with supersample on";  exit 77; }
capture_at 0 "$OUT/noss.png" || { echo "SKIP: no screenshot with supersample off"; exit 77; }

analyze_parity "$OUT/ss.png" "$OUT/noss.png" "${W}x${H}" "$SCENE" || exit 1
exit 0
