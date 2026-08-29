#!/usr/bin/env bash
# wiredui-layout-checks.sh — pixel-free computed checks for two WiredUI fixes
# that the smoke golden gate cannot see (it captures in-game, never a menu):
#
#   #2 (focus highlight): exactly one — or zero — items per menu may be
#      "focused" (the single highlight-emit predicate item ==
#      WiredUI_GetFocusedItem() in cl_wired_clay.c). Two focused items = the
#      "two highlights" symptom. The check sums the per-item "focused" flag the
#      layout dump now records (cl_wired_layout_dump.c) and fails if any menu
#      has > 1.
#
#   #4 (DPI font scaling): dpiScale must be wired from the platform layer
#      (physical/logical), and the rendered glyph px size = fontPointSize *
#      dpiScale. The layout dump records both fontPointSize and dpiScale per
#      item; this check asserts dpiScale is a sane positive ratio (>= 1.0) that
#      MATCHES the engine's reported physical/logical sizes, proving the stub
#      1.0f was replaced by a real value AND that the text path multiplies by it
#      (the emit path and the dump read the same WiredUI_GetDpiScale()).
#
#   #8 (rem root scale): rem-sized lengths are value * rootScale * dpiScale,
#      where rootScale is ui_rootSize — the user's "how big should the UI be",
#      as distinct from dpiScale's "how many physical pixels is a logical one".
#      The dump records the root next to the ratio, so this check asserts it is
#      present, positive and the SAME for every item. Set EXPECT_ROOT_SCALE to
#      also assert a specific value — that is how a caller proves a change of
#      ui_rootSize actually reached the resolvers. Absent from pre-rem dumps,
#      which are tolerated rather than failed.
#
# All three run off layoutdump.jsonl (r_layoutDump 1) — no pixel capture, no
# RenderDoc, no human. The engine is launched to its main menu (no +map) so the
# menu layout dumps; we never need a screen.
#
# Usage:   tests/wiredui-layout-checks.sh [path-to-wired]
# Exit:    0 PASS   1 FAIL   77 SKIP (no binary / no paks)

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
TIMEOUT_RUNNER="$SCRIPT_DIR/run-with-timeout.py"

# ── #7 DPI gate self-test (gate-has-teeth, no engine) ──────────────────────────
# Proves the #4 DPI assertion FAILS when the engine's computed dpiScale is wrong
# (e.g. the old stub returning 1.0 while physical/logical = 2.0) and PASSES when
# it's right. Feeds the SAME analyzer two synthetic layout dumps. The analyzer
# asserts dpiScale == expected (physical/logical), so a 1.0 vs 2.0 mismatch must
# trip it — i.e. a broken real DPI wiring cannot slip through.
if [ "${1:-}" = "--dpi-self-test" ]; then
    echo "==> WiredUI DPI gate SELF-TEST (gate-has-teeth)"
    ST="$(mktemp -d -t wired-dpist-XXXXXX 2>/dev/null || mktemp -d)"
    trap 'rm -rf "$ST"' EXIT INT TERM

    # dump <file> <vidWidthPx|-> <dpiScale...>  — one menu row + one item row per
    # dpiScale given (two values = the "inconsistent across items" shape).
    dump() {
        local f="$1" vid="$2"; shift 2
        local vidfield=""; [ "$vid" != "-" ] && vidfield=",\"vidWidthPx\":$vid"
        : > "$f"
        local first=1
        for d in "$@"; do
            [ "$first" = 1 ] && printf '{"region":"main","kind":"menu","dpiScale":%s,"frame":1,"menu":"main"%s}\n' "$d" "$vidfield" >> "$f"
            printf '{"region":"row","kind":"item","fontPointSize":14,"dpiScale":%s,"focused":1,"activeCvar":"","hasActiveBackcolor":0,"frame":1,"menu":"main"%s}\n' "$d" "$vidfield" >> "$f"
            first=0
        done
    }

    # Each case: name | logical | fallback | want-rc | vidWidthPx | dpiScale(s)
    # want-rc 0 = the gate must ACCEPT; 1 = the gate must REJECT.
    #
    # The dpi4-retina case is the regression that made the LIVE gate fail a
    # CORRECT engine (wiki 2026-08-11 / fix d00a6afd): 5120 backing over a 1280
    # logical request is a genuine dpiScale=4, and a gate that assumed 2 was
    # wrong. Deriving from vidWidthPx is exactly what this fixture pins.
    #
    # The supersample-* pair pins the SAME rule one step further out, where the
    # ratio climbs past 4 for a second, equally genuine reason.
    #
    # r_ext_supersample doubles glConfig.vidWidth/Height, and WiredUI lays Clay
    # out on exactly that canvas (cl_wired_clay.c: WiredUI_ClayFrame is fed
    # cls.glconfig.vidWidth/Height). For the UI to keep its share of the screen,
    # everything authored in points has to grow with the canvas — so dpiScale is
    # SUPPOSED to carry the supersample factor on top of the display's own. A
    # 1280-point window on a 2x panel with supersampling on backs at 5120 and
    # wants 4. Anything that "corrects" this back down to the display ratio
    # leaves the whole UI at half size on the enlarged canvas.
    #
    #   supersample-on-2x-display        5120 backing / 1280 logical → 4.0 ACCEPT
    #   supersample-reports-display-only same session claiming 2.0   → REJECT
    #
    # SCOPE, stated honestly: these fixtures pin the ANALYZER's decision, not the
    # engine's wiring — they are synthetic dumps and no engine runs here. What
    # they buy is a guard against "fixing" dpiScale in the wrong direction, which
    # is a real temptation: the ratio looks inflated until you notice the canvas
    # grew with it. Catching the engine half needs a live capture with
    # r_ext_supersample 1, which belongs to the owner-gated capture path above.
    cases="
good-2x|1280|2.0|0|2560|2.0
dpi4-retina|1280|2.0|0|5120|4.0
non-hidpi-1x|1280|1.0|0|1280|1.0
stub-1x-on-2x-display|1280|2.0|1|2560|1.0
stub-2x-on-dpi4-display|1280|2.0|1|5120|2.0
inconsistent-across-items|1280|2.0|1|2560|2.0 4.0
zero-dpi|1280|2.0|1|2560|0.0
negative-dpi|1280|2.0|1|2560|-2.0
legacy-dump-no-vidpx-good|1280|2.0|0|-|2.0
legacy-dump-no-vidpx-broken|1280|2.0|1|-|1.0
supersample-on-2x-display|1280|4.0|0|5120|4.0
supersample-reports-display-only|1280|4.0|1|5120|2.0
"
    fails=0; ran=0
    while IFS='|' read -r name logical fallback want vid dpis; do
        [ -n "$name" ] || continue
        ran=$((ran+1))
        # shellcheck disable=SC2086 # dpis is an intentional word-split list
        dump "$ST/$name.jsonl" "$vid" $dpis
        rc=0
        bash "$0" --dpi-analyze "$ST/$name.jsonl" "$logical" "$fallback" >"$ST/$name.out" 2>&1 || rc=$?
        [ "$rc" -ne 0 ] && rc=1
        if [ "$rc" -eq "$want" ]; then
            printf '  ok   %-28s rc=%s (want %s)\n' "$name" "$rc" "$want"
        else
            printf '  FAIL %-28s rc=%s (want %s)\n' "$name" "$rc" "$want"
            sed 's/^/         | /' "$ST/$name.out"
            fails=$((fails+1))
        fi
    done <<EOF
$cases
EOF

    if [ "$fails" -eq 0 ]; then
        echo "==> DPI SELF-TEST PASS: $ran fixtures — gate accepts correct dpiScale (incl. the Retina dpiScale=4 case) AND rejects broken wiring (it has teeth)"
        exit 0
    fi
    echo "==> DPI SELF-TEST FAIL: $fails/$ran fixtures behaved wrongly"
    exit 1
fi

# Sub-invocation used by --dpi-self-test: run ONLY the #4 dpiScale assertion on a
# given dump. Exit 0 PASS / 1 FAIL.
#
#   $2  layout dump (.jsonl)
#   $3  LOGICAL width the session was launched with
#   $4  fallback expected ratio, used only when the dump carries no vidWidthPx
#
# Expectation derivation mirrors the live gate (see the "HiDPI correction" block
# below): when the dump records vidWidthPx — the backing width the engine really
# got — the expectation is vidWidthPx/LOGICAL_W. That is the half the live gate
# got WRONG before d00a6afd: on Retina a 1280 logical request backs at 2560 (or
# 5120 for the dpiScale=4 case), so assuming physical/logical == 2 FAILED a
# CORRECT engine. Deriving from the dump is what makes this fixture-checkable.
# ── #8 rem gate self-test (gate-has-teeth, no engine) ─────────────────────────
# Proves the rem assertion rejects the ways a root scale can be wrong: missing
# from some items, zero (which collapses every rem length), disagreeing between
# items, and not matching what the caller asked for (ui_rootSize did not reach
# the resolvers). Same analyzer, synthetic dumps — no engine.
if [ "${1:-}" = "--rem-self-test" ]; then
    echo "==> WiredUI rem gate SELF-TEST (gate-has-teeth)"
    ST="$(mktemp -d -t wired-remst-XXXXXX 2>/dev/null || mktemp -d)"
    trap 'rm -rf "$ST"' EXIT INT TERM

    # remdump <file> <rootScale...> — one row per root ("-" omits the field).
    remdump() {
        local f="$1"; shift
        : > "$f"
        local first=1
        for r in "$@"; do
            local rf=""; [ "$r" != "-" ] && rf=",\"rootScale\":$r"
            [ "$first" = 1 ] && printf '{"region":"main","kind":"menu","dpiScale":2,"vidWidthPx":2560,"frame":1,"menu":"main"%s}\n' "$rf" >> "$f"
            printf '{"region":"row","kind":"item","fontPointSize":14,"dpiScale":2,"vidWidthPx":2560,"focused":1,"activeCvar":"","hasActiveBackcolor":0,"frame":1,"menu":"main"%s}\n' "$rf" >> "$f"
            first=0
        done
    }

    # name | want-rc | EXPECT_ROOT_SCALE | rootScale(s)
    # want-rc 0 = must ACCEPT, 1 = must REJECT.
    cases='
default-root|0||14
large-root|0||21
absent-is-tolerated|0||-
expectation-met|0|21|21
expectation-missed|1|14|21
zero-root|1||0
negative-root|1||-3
inconsistent-across-items|1||14 21
'
    fails=0; n=0
    printf '%s\n' "$cases" | while IFS='|' read -r name want expect roots; do
        [ -z "$name" ] && continue
        n=$((n+1))
        # shellcheck disable=SC2086
        remdump "$ST/$name.jsonl" $roots
        rc=0
        EXPECT_ROOT_SCALE="$expect" bash "$0" --dpi-analyze "$ST/$name.jsonl" 1280 >"$ST/$name.out" 2>&1 || rc=$?
        if [ "$rc" = "$want" ]; then
            printf '  ok   %-28s rc=%s (want %s)\n' "$name" "$rc" "$want"
        else
            printf '  FAIL %-28s rc=%s (want %s)\n' "$name" "$rc" "$want"
            sed 's/^/       /' "$ST/$name.out"
            fails=$((fails+1))
        fi
        echo "$fails" > "$ST/.fails"
    done
    fails="$(cat "$ST/.fails" 2>/dev/null || echo 0)"
    if [ "${fails:-0}" = 0 ]; then
        echo "==> REM SELF-TEST PASS: gate accepts a sane root AND rejects zero/inconsistent/unmet-expectation (it has teeth)"
        exit 0
    fi
    echo "==> REM SELF-TEST FAIL: $fails wrong"; exit 1
fi

if [ "${1:-}" = "--dpi-analyze" ]; then
    python3 - "$2" "$3" "${4:-}" <<'PYEOF'
import json, os, sys
path = sys.argv[1]
logical = float(sys.argv[2])
fallback = sys.argv[3]
rows=[json.loads(l) for l in open(path) if l.strip()]
last=max(o.get("frame",0) for o in rows)
fr=[o for o in rows if o.get("frame")==last]

# Derive the expectation the same way the live gate does: prefer the engine's
# own reported backing width, fall back to the caller's ratio for older dumps.
vid=[int(o["vidWidthPx"]) for o in fr if "vidWidthPx" in o and int(o.get("vidWidthPx",0))>0]
if vid:
    if len(set(vid))!=1:
        print(f"  FAIL #4: vidWidthPx inconsistent {sorted(set(vid))}"); sys.exit(1)
    if logical<=0.0:
        print(f"  FAIL #4: logical width non-positive {logical:g}"); sys.exit(1)
    exp=vid[0]/logical
    src=f"vidWidthPx {vid[0]} / logical {logical:g}"
elif fallback:
    exp=float(fallback); src=f"caller fallback (no vidWidthPx in dump)"
else:
    print("  FAIL #4: dump has no vidWidthPx and no fallback ratio given"); sys.exit(1)

dpis=sorted({round(float(o.get("dpiScale",-1.0)),6) for o in fr if "dpiScale" in o})
print(f"  [#4 dpi] dpiScale observed = {dpis} (expected {exp:g} from {src})")
if not dpis or any(d<=0.0 for d in dpis):
    print(f"  FAIL #4: dpiScale missing/non-positive {dpis}"); sys.exit(1)
if len(dpis)!=1:
    print(f"  FAIL #4: dpiScale inconsistent {dpis}"); sys.exit(1)
if abs(dpis[0]-exp)>1e-3:
    print(f"  FAIL #4: dpiScale={dpis[0]} != real {exp:g} — broken DPI wiring"); sys.exit(1)

# #8 rem: the root must be present, positive and consistent. It is a separate
# quantity from dpiScale — the display sets the ratio, the user sets the root —
# so a dump carrying one but not the other cannot answer "did ui_rootSize take".
# Older dumps predate the field; absent is tolerated, present-but-broken is not.
roots=sorted({round(float(o["rootScale"]),6) for o in fr if "rootScale" in o})
if roots:
    print(f"  [#8 rem] rootScale observed = {roots}")
    if any(r<=0.0 for r in roots):
        print(f"  FAIL #8: rootScale non-positive {roots} — rem lengths collapse"); sys.exit(1)
    if len(roots)!=1:
        print(f"  FAIL #8: rootScale inconsistent across items {roots}"); sys.exit(1)
    want=float(os.environ.get("EXPECT_ROOT_SCALE","") or 0.0)
    if want>0.0 and abs(roots[0]-want)>1e-3:
        print(f"  FAIL #8: rootScale={roots[0]:g} != expected {want:g} — ui_rootSize did not take"); sys.exit(1)
    print(f"  PASS #8: rootScale={roots[0]:g}"
          + (f" == expected {want:g}" if want>0.0 else " (consistent, no expectation set)"))
else:
    print("  [#8 rem] rootScale absent from dump — pre-rem build, skipping")

print(f"  PASS #4: dpiScale={dpis[0]:g} == {exp:g}"); sys.exit(0)
PYEOF
    exit $?
fi

# Default resolved, not hardcoded. This used to default to wired.x64.exe — a
# WINDOWS binary name — so on every other platform the gate SKIPped, and the
# skip was mistaken for "this criterion needs a Windows machine". It does not:
# the DPI wiring under test is platform-independent, and a Retina Mac is a
# BETTER host for it than a 1x display (dpiScale != 1 is the interesting case).
. "$REPO_ROOT/tests/lib/wired_paths.sh"
WIRED="${1:-${WIRED_BINARY:-}}"
if [ ! -x "$WIRED" ]; then
    echo "SKIP: wired binary not found: $WIRED"
    exit 77
fi
# Absolutise so a later `cd "$WIRED_DIR"` does not break a relative binary path.
case "$WIRED" in
    /*) : ;;
    *)  WIRED="$PWD/$WIRED" ;;
esac
WIRED_DIR="$(cd "$(dirname "$WIRED")" && pwd)"
WIRED="$WIRED_DIR/$(basename "$WIRED")"

if [ ! -f "$TIMEOUT_RUNNER" ] ||
   ! python3 -c 'import sys; raise SystemExit(0 if sys.version_info >= (3,8) else 1)' 2>/dev/null; then
    echo "SKIP: Python >=3.8 and tests/run-with-timeout.py are required"
    exit 77
fi

# Resolve the exact current UI pack from either a bare assembled install or a
# macOS .app (Contents/MacOS binary + Contents/Resources content).  Licensed
# base content may come from the same install or an explicit external root.
PACK_ROOT="$(wired_find_archive_root "$WIRED_DIR" "$WIRED_DIR/../Resources" "$WIRED_DIR/../../.." 2>/dev/null || true)"
if [ -z "$PACK_ROOT" ]; then
    echo "SKIP: no current VFS archive found beside bundle/install for $WIRED"
    exit 77
fi

CONTENT_ROOT="$(wired_find_archive_root "${WIRED_CONTENT_ROOT:-}" "$WIRED_HOME" "$PACK_ROOT" "${WIRED_INSTALL:-}" 2>/dev/null || true)"
if [ -z "$CONTENT_ROOT" ]; then
    echo "SKIP: canonical base content missing; set WIRED_CONTENT_ROOT to a root containing base VFS archives"
    exit 77
fi

# Isolated homepath so we never touch the user's config; mirror the smoke
# harness's product-dir naming so CVAR_ARCHIVE writeback lands here.
PRODUCT_DIRNAME="q3now-preview"
BUILD_CACHE="$WIRED_DIR/CMakeCache.txt"
if [ -f "$BUILD_CACHE" ]; then
    pn="$(awk -F= '/^PRODUCT_NAME(:[^=]*)?=/ { print $2; exit }' "$BUILD_CACHE")"
    cs="$(awk -F= '/^CHANNEL_SUFFIX(:[^=]*)?=/ { print $2; exit }' "$BUILD_CACHE")"
    [ -n "$pn" ] && PRODUCT_DIRNAME="${pn}${cs}"
fi

HOME_PARENT="$(mktemp -d -t wired-uichk-XXXXXX 2>/dev/null || mktemp -d)"
HOME_DIR="$HOME_PARENT/$PRODUCT_DIRNAME"
cleanup_layout_check() {
    if [ "${WIRED_KEEP_ARTIFACTS:-0}" = 1 ]; then
        echo "    retained: $HOME_PARENT"
    else
        rm -rf "$HOME_PARENT"
    fi
}
trap cleanup_layout_check EXIT INT TERM
if ! wired_link_content_into_home "$HOME_DIR" "$CONTENT_ROOT/base" "$WIRED_BASE" "$PACK_ROOT/base"; then
    echo "FAIL: could not link VFS archives into isolated home"
    exit 1
fi
case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*) HOME_NATIVE="$(cygpath -w "$HOME_DIR")" ;;
    *)                    HOME_NATIVE="$HOME_DIR" ;;
esac

# The layout dump and stdout live beside the isolated home, never in the app or
# build directory. The current bundle directory is linked last so it wins over
# any same-name archive in the licensed content root.
LAYOUT_DUMP="$HOME_PARENT/layoutdump.jsonl"
JSONL="$HOME_DIR/qconsole.jsonl"
STDOUT="$HOME_PARENT/wired.stdout"

echo "==> WiredUI layout checks: $WIRED"
echo "    current VFS: $PACK_ROOT/base (read-only links)"
echo "    base content: $CONTENT_ROOT/base (read-only links)"
echo "    fs_homepath : $HOME_NATIVE (isolated)"
echo "    layout dump : $LAYOUT_DUMP"

# DPI #4 exercises the REAL dpiScale = vidWidth / vidWidthLogical computation
# (compositor), NOT the r_uiDpiScaleTest final-value override (which short-circuits
# WiredUI_GetDpiScale before the ratio math and so can't catch a broken real
# wiring). We inject a synthetic LOGICAL size at the platform seam
# (r_uiLogicalWidthTest/Height — what a HiDPI SDL_GetWindowSize would publish);
# the compositor then computes the genuine ratio physical/logical. Physical is the
# launch resolution (PHYS_W x PHYS_H), logical is LOGICAL_W x LOGICAL_H, so the
# real dpiScale the gate must observe is PHYS_W/LOGICAL_W.
PHYS_W="${PHYS_W:-1280}"; PHYS_H="${PHYS_H:-720}"
LOGICAL_W="${LOGICAL_W:-640}"; LOGICAL_H="${LOGICAL_H:-360}"
UI_ROOT_SIZE="${UI_ROOT_SIZE:-14}"
# Expected real dpiScale = physical / logical (1280/640 = 2.0).
DPI_TEST="$(awk -v p="$PHYS_W" -v l="$LOGICAL_W" 'BEGIN{printf "%.6g", p/l}')"

# One interactive session that exercises ALL checks across BOTH input modes:
#   * +wui_push main          → open the interactive main menu (focusable rows;
#                               onOpen seeds focus on menu_campaign)
#   * +wui_menu_nav down      → synthesize K_DOWNARROW (KEYBOARD focus move)
#   * +wui_hover_test menu_host → drive the real hover path (MOUSE focus move) to
#                               a DIFFERENT row, proving hover moves the SAME
#                               single highlight (not a second one)
#   * r_uiLogicalWidthTest/Height → inject the platform logical size so the REAL
#                               dpiScale = physical/logical computation runs (#7)
# r_layoutDump 1 records, per item per frame, focused + activeCvar +
# hasActiveBackcolor + dpiScale. The dump spans the keyboard-focus frames AND
# the hover frames, so the "<=1 highlight in every frame" assertion covers both
# input modes. com_automated 1 keeps any error dialog from blocking (also fix
# #3). Windowed, no mouse-grab, sound off; no map (the menu needs no BSP).
HOVER_ITEM="${HOVER_ITEM:-menu_host}"
KEYBOARD_ITEM="${KEYBOARD_ITEM:-menu_join}"
python3 "$TIMEOUT_RUNNER" \
    --timeout 90 \
    --kill-after 15 \
    --cwd "$HOME_PARENT" \
    --stdout "$STDOUT" \
    -- "$WIRED" \
    +set fs_homepath "$HOME_NATIVE" \
    +set com_automated 1 \
    +set s_initsound 0 \
    +set r_fullscreen 0 \
    +set r_mode -1 +set r_customwidth "$PHYS_W" +set r_customheight "$PHYS_H" \
    +set r_uiLogicalWidthTest "$LOGICAL_W" +set r_uiLogicalHeightTest "$LOGICAL_H" \
    +set ui_rootSize "$UI_ROOT_SIZE" \
    +set r_layoutDump 1 \
    +wait 80 \
    +wui_push main \
    +wait 40 \
    +wui_menu_nav down \
    +wait 40 \
    +wui_hover_test "$HOVER_ITEM" \
    +wait 20 \
    +wui_hover_test "$HOVER_ITEM" \
    +wait 60 \
    +quit
engine_rc=$?
if [ "$engine_rc" -ne 0 ]; then
    echo "FAIL: layout engine exited with status $engine_rc"
    [ -s "$STDOUT" ] && { echo "  -- stdout tail --"; tail -15 "$STDOUT" | sed 's/^/  /'; }
    exit 1
fi

if [ ! -s "$LAYOUT_DUMP" ]; then
    echo "FAIL: no layoutdump.jsonl produced (engine did not lay out a menu)"
    [ -s "$STDOUT" ] && { echo "  -- stdout tail --"; tail -15 "$STDOUT" | sed 's/^/  /'; }
    exit 1
fi
# The layout result is invalid if its owning engine phase logged a fatal/error
# or an authoritative UI warning.  Parse JSON strictly; a partial good dump may
# not mask a corrupt/partial qconsole stream.
python3 - "$JSONL" <<'LOGPY' || exit 1
import json, sys
rows = []
errors = 0
try:
    with open(sys.argv[1], encoding="utf-8", errors="replace") as stream:
        for raw in stream:
            if not raw.strip():
                continue
            try:
                row = json.loads(raw)
            except ValueError:
                errors += 1
                continue
            if not isinstance(row, dict):
                errors += 1
            else:
                rows.append(row)
except OSError as exc:
    print(f"FAIL: cannot read layout phase qconsole: {exc}")
    raise SystemExit(1)
bad = [row for row in rows
       if str(row.get("sev", "")).upper() in {"ERROR", "FATAL"}
       or (str(row.get("sev", "")).upper() == "WARN"
           and str(row.get("cat", "")).lower() == "ui")]
if errors or bad:
    print(f"FAIL: layout phase qconsole malformed={errors} unexpected-severity={len(bad)}")
    for row in bad[:10]:
        print("  " + json.dumps(row, sort_keys=True))
    raise SystemExit(1)
print(f"  qconsole: {len(rows)} strict JSON records, zero ERROR/FATAL/cat=ui WARN")
LOGPY
# HiDPI correction: the launch width is the LOGICAL size — on a 2x display a
# 1280 request backs at 2560 physical pixels, so "expected = PHYS_W/LOGICAL_W"
# under-states the genuine ratio and fails a CORRECT engine. The dump now
# records the backing width the engine actually got (vidWidthPx); derive the
# expectation from that. Falls back to the launch-width assumption when the
# field is absent (older dumps / self-test fixtures).
# tr -cd: strip everything non-digit — some environments force grep color even
# into pipes, and the ANSI tail would fail the numeric test silently.
VID_PX="$(grep -o '"vidWidthPx":[0-9]*' "$LAYOUT_DUMP" | head -1 | cut -d: -f2 | tr -cd '0-9')"
if [ -n "$VID_PX" ] && [ "$VID_PX" -gt 0 ] 2>/dev/null; then
    DPI_TEST="$(awk -v p="$VID_PX" -v l="$LOGICAL_W" 'BEGIN{printf "%.6g", p/l}')"
    echo "    dpi REAL path   : backing ${VID_PX}px / logical ${LOGICAL_W} -> expect dpiScale=$DPI_TEST (vidWidthPx from dump)"
else
    echo "    dpi REAL path   : physical ${PHYS_W}x${PHYS_H} / logical ${LOGICAL_W}x${LOGICAL_H} -> expect dpiScale=$DPI_TEST (vidWidth/vidWidthLogical)"
fi

# ── analysis (jq-free awk; tolerant of field order) ───────────────────────────
# Read the LAST frame's worth of lines (a static menu dumps identical lines each
# frame; the last frame is the settled state). Group by menu, sum focused,
# collect fontPointSize/dpiScale.
python3 - "$LAYOUT_DUMP" "$DPI_TEST" "$HOVER_ITEM" "$KEYBOARD_ITEM" <<'PYEOF'
import json, sys, collections

path = sys.argv[1]
dpi_expected = float(sys.argv[2]) if len(sys.argv) > 2 else 0.0
hover_item = sys.argv[3] if len(sys.argv) > 3 else ""
keyboard_item = sys.argv[4] if len(sys.argv) > 4 else ""
lines = []
parse_errors = 0
with open(path, "r", encoding="utf-8", errors="replace") as fh:
    for ln in fh:
        ln = ln.strip()
        if not ln:
            continue
        try:
            row = json.loads(ln)
            if not isinstance(row, dict):
                parse_errors += 1
            else:
                lines.append(row)
        except Exception:
            parse_errors += 1

if parse_errors:
    print(f"FAIL: layoutdump.jsonl contains {parse_errors} malformed/non-object record(s)")
    sys.exit(1)

if not lines:
    print("FAIL: layoutdump.jsonl had no parseable JSON lines")
    sys.exit(1)

# Examine EVERY frame for the focus invariant (the count must never exceed 1 in
# any frame, not just the settled one), but use the LAST frame that actually has
# a focused item for the "exactly one" assertion (focus settles a frame or two
# after the synthesized nav key).
all_frames = sorted({o.get("frame", 0) for o in lines})
last_frame = all_frames[-1]
def items_of(fr):  return [o for o in lines if o.get("frame", 0) == fr and o.get("kind") == "item"]
def menus_of(fr):  return [o for o in lines if o.get("frame", 0) == fr and o.get("kind") == "menu"]

settled = items_of(last_frame)
print(f"  frames dumped: {len(all_frames)} (last={last_frame}); "
      f"last frame {len(menus_of(last_frame))} menu(s), {len(settled)} named item(s)")

fail = False

# ── #2 focus-highlight count ─────────────────────────────────────────────────
# Invariant across ALL frames: no menu ever has > 1 focused item (that is the
# "two highlights" symptom). Plus a liveness check: at least one frame focused
# exactly one item, proving the path produces a single highlight (not zero).
worst_ever = 0; worst_where = ("", 0)
saw_exactly_one = False
for fr in all_frames:
    fb = collections.Counter()
    for it in items_of(fr):
        if int(it.get("focused", 0)):
            fb[it.get("menu", "?")] += 1
    if fb:
        mx = max(fb.values())
        if mx > worst_ever:
            worst_ever = mx
            worst_where = (max(fb, key=fb.get), fr)
        if max(fb.values()) == 1 and sum(fb.values()) == 1:
            saw_exactly_one = True
print(f"  [#2 focus] max focused-per-menu across all {len(all_frames)} frames = {worst_ever}; "
      f"some frame focused exactly one = {saw_exactly_one}")
if worst_ever > 1:
    print(f"  FAIL #2: menu '{worst_where[0]}' (frame {worst_where[1]}) had "
          f"{worst_ever} focused items — the two-highlights bug")
    fail = True
elif not saw_exactly_one:
    print(f"  FAIL #2: no frame focused an item — keyboard/hover behavior was not exercised")
    fail = True
else:
    print(f"  PASS #2: exactly one highlight when focused, never more than one "
          f"across {len(all_frames)} frames (single highlight-emit path)")

# The aggregate <=1 invariant is insufficient by itself: a later mouse hover
# could hide a broken keyboard transition.  Require the authored main onOpen
# focus, the real Down target, and the later hover target as an exact ordered
# sequence on distinct frames.
focus_frames = []
for fr in all_frames:
    focused = [it.get("region", "") for it in items_of(fr) if int(it.get("focused", 0))]
    if len(focused) == 1:
        focus_frames.append((fr, focused[0]))

wanted_focus = ["menu_campaign", keyboard_item, hover_item]
focus_cursor = 0
matched_focus = []
last_focus_frame = -1
for wanted in wanted_focus:
    for index in range(focus_cursor, len(focus_frames)):
        fr, region = focus_frames[index]
        if fr > last_focus_frame and region == wanted:
            matched_focus.append((fr, region))
            last_focus_frame = fr
            focus_cursor = index + 1
            break
    else:
        print(f"  FAIL #K: missing ordered focus transition to '{wanted}'")
        fail = True
        break
if len(matched_focus) == len(wanted_focus):
    print("  PASS #K: ordered initial→keyboard→hover focus = " +
          " -> ".join(f"{region}@{fr}" for fr, region in matched_focus))

# ── #4 DPI font scaling — REAL vidWidth/vidWidthLogical path (#7) ─────────────
# dpi_expected is physical/logical (the launch computed PHYS_W/LOGICAL_W). The
# logical size was injected at the PLATFORM SEAM (r_uiLogicalWidthTest), NOT via
# the r_uiDpiScaleTest final-value override — so the dump's dpiScale is the result
# of the engine's genuine `dpiScale = vidWidth / vidWidthLogical` computation in
# the compositor. Asserting it equals physical/logical proves the REAL ratio math
# is correct (a broken platform-size publish or a broken divide FAILS here), and
# that the same WiredUI_GetDpiScale() the text-emit path multiplies by carries it.
dpis = sorted({round(float(o.get("dpiScale", -1.0)), 6)
               for o in (menus_of(last_frame) + settled) if "dpiScale" in o})
print(f"  [#4 dpi] dpiScale observed = {dpis} (expected physical/logical = {dpi_expected:g}, computed by the engine)")
if not dpis or any(d <= 0.0 for d in dpis):
    print(f"  FAIL #4: dpiScale missing or non-positive {dpis} "
          f"(stub never replaced / divide guard hit)")
    fail = True
elif len(dpis) != 1:
    print(f"  FAIL #4: dpiScale inconsistent across items {dpis}")
    fail = True
elif abs(dpis[0] - dpi_expected) > 1e-3:
    print(f"  FAIL #4: dpiScale={dpis[0]} != real physical/logical {dpi_expected:g} — the "
          f"engine's vidWidth/vidWidthLogical computation is wrong (broken DPI wiring)")
    fail = True
else:
    dpi = dpis[0]
    sample = [it for it in settled if float(it.get("fontPointSize", 0)) > 0][:5]
    print(f"  PASS #4: dpiScale={dpi:g} = physical/logical, computed by the REAL "
          f"vidWidth/vidWidthLogical path; glyph px = fontPointSize * {dpi:g}:")
    for it in sample:
        fp = float(it["fontPointSize"])
        print(f"           {it.get('region','?'):<26} {fp:g}pt -> {fp*dpi:g}px")
    if not sample:
        print(f"           (items used the default; "
              f"WUI_DEFAULT_FONT_SIZE 14pt -> {14.0*dpi:g}px)")

# ── #H hover → focus (selection unification) ─────────────────────────────────
# The session hovered `hover_item` AFTER a keyboard nav-down landed focus on a
# different row. The settled (last) frame must show focus on the hovered item —
# proving mouse hover drives the SAME focus state as the keyboard, and that the
# highlight MOVED (not duplicated). Combined with #2 (never >1 across all frames,
# which spans both the keyboard-focus and hover frames), this is the one-highlight
# guarantee under both input modes.
if hover_item:
    settled_focus = [it["region"] for it in settled if int(it.get("focused", 0))]
    print(f"  [#H hover] after hovering '{hover_item}': settled focus = {settled_focus}")
    if settled_focus == [hover_item]:
        print(f"  PASS #H: mouse hover moved the single focus-highlight to "
              f"'{hover_item}' (same state as keyboard nav; exactly one focused)")
    else:
        print(f"  FAIL #H: hover did not land focus solely on '{hover_item}' "
              f"(got {settled_focus}) — hover/keyboard focus not unified, or a "
              f"second highlight survives")
        fail = True

# ── #N no second highlight system on menu rows ───────────────────────────────
# The hover-driven `active ui_currentMenuItem` cvar + backcolor.active (the cyan
# second highlight) was removed from the menu rows. Assert NO menu_* row carries
# an activeCvar binding or an active-backcolor variant in any frame. The settings
# tabs legitimately keep `active ui_settingsSection` — those are NOT menu_* rows
# and are not present in this menu, so this check is scoped to the menu rows.
menu_rows = [o for o in lines
             if o.get("kind") == "item" and str(o.get("region", "")).startswith("menu_")]
bad_active = sorted({o["region"] for o in menu_rows if o.get("activeCvar")})
bad_backc  = sorted({o["region"] for o in menu_rows if int(o.get("hasActiveBackcolor", 0))})
print(f"  [#N rows] menu_* rows with activeCvar binding = {bad_active or 'none'}; "
      f"with active-backcolor = {bad_backc or 'none'}")
if bad_active or bad_backc:
    print(f"  FAIL #N: the hover-active (ui_currentMenuItem) second-highlight "
          f"system is still present on menu rows "
          f"(activeCvar:{bad_active} backcolor:{bad_backc})")
    fail = True
elif not menu_rows:
    print(f"  FAIL #N: no menu_* rows in the dump — removal is unverified")
    fail = True
else:
    print(f"  PASS #N: no menu row carries activeCvar or active-backcolor — the "
          f"cyan hover-active second highlight is gone; focus is the only state")

# ── #E text emphasis follows focus ───────────────────────────────────────────
# The ">" chevron + LABEL + SUB child itemDefs keep forecolor.active
# (hasActiveForecolor=1). Their `.active` variant must now fire on FOCUS: a
# child of the FOCUSED row must have focusActive=1, and a child of an UNFOCUSED
# row focusActive=0. focusActive mirrors the cl_wired_clay.c isActive-by-focus
# predicate that gates forecolor.active — so this proves the focused item's text
# emphasis is sourced from focus alone (re-homed off the removed hover cvar).
last_items = items_of(last_frame)
foc_rows = [o["region"] for o in last_items
            if str(o.get("region","")).startswith("menu_")
            and "/" not in str(o.get("region",""))
            and int(o.get("focused",0))]
def kids_of(rowname):
    return [o for o in last_items if str(o.get("region","")).startswith(rowname + "/child")]
emph_ok = True
detail = []
for o in last_items:
    reg = str(o.get("region",""))
    if "/child" not in reg or not reg.startswith("menu_"):
        continue
    if not int(o.get("hasActiveForecolor",0)):
        continue  # only the emphasis-bearing children matter
    parent = reg.split("/child")[0]
    parent_focused = parent in foc_rows
    fa = int(o.get("focusActive",0))
    if fa != (1 if parent_focused else 0):
        emph_ok = False
        detail.append(f"{reg}: focusActive={fa} but parent focused={parent_focused}")
n_emph = sum(1 for o in last_items
             if "/child" in str(o.get("region",""))
             and int(o.get("hasActiveForecolor",0)))
print(f"  [#E emphasis] focused row(s)={foc_rows}; checked {n_emph} emphasis-bearing "
      f"child itemDef(s) for focusActive==parent.focused")
if not foc_rows:
    print(f"  FAIL #E: no focused menu row in the settled frame — emphasis re-home unverified")
    fail = True
elif n_emph == 0:
    print(f"  FAIL #E: no emphasis-bearing children dumped — cannot verify")
    fail = True
elif not emph_ok:
    print(f"  FAIL #E: text emphasis does not follow focus: {detail[:4]}")
    fail = True
else:
    print(f"  PASS #E: focused row's text children carry focusActive (->"
          f" forecolor.active); unfocused rows do not — emphasis is sourced "
          f"from focus alone")

sys.exit(1 if fail else 0)
PYEOF
RC=$?

if [ "$RC" -eq 0 ]; then
    echo "==> WiredUI layout checks: PASS"
else
    echo "==> WiredUI layout checks: FAIL"
fi
exit $RC
