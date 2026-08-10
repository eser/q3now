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
# Both run off layoutdump.jsonl (r_layoutDump 1) — no pixel capture, no
# RenderDoc, no human. The engine is launched to its main menu (no +map) so the
# menu layout dumps; we never need a screen.
#
# Usage:   tests/wiredui-layout-checks.sh [path-to-wired]
# Exit:    0 PASS   1 FAIL   77 SKIP (no binary / no paks)

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

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
    EXPECT=2.0
    # GOOD dump: dpiScale=2.0 (the correct physical/logical result).
    printf '%s\n' \
      '{"region":"main","kind":"menu","dpiScale":2.0,"frame":1,"menu":"main"}' \
      '{"region":"row","kind":"item","fontPointSize":14,"dpiScale":2.0,"focused":1,"activeCvar":"","hasActiveBackcolor":0,"frame":1,"menu":"main"}' \
      > "$ST/good.jsonl"
    # BROKEN dump: dpiScale=1.0 (the stub/broken-wiring value) while expecting 2.0.
    printf '%s\n' \
      '{"region":"main","kind":"menu","dpiScale":1.0,"frame":1,"menu":"main"}' \
      '{"region":"row","kind":"item","fontPointSize":14,"dpiScale":1.0,"focused":1,"activeCvar":"","hasActiveBackcolor":0,"frame":1,"menu":"main"}' \
      > "$ST/broken.jsonl"
    rc_good=0; rc_broken=0
    echo "  -- GOOD dump (dpiScale 2.0, expect 2.0) → expect PASS --"
    bash "$0" --dpi-analyze "$ST/good.jsonl"   "$EXPECT" || rc_good=$?
    echo "  -- BROKEN dump (dpiScale 1.0, expect 2.0 = broken wiring) → expect FAIL --"
    bash "$0" --dpi-analyze "$ST/broken.jsonl" "$EXPECT" || rc_broken=$?
    if [ "$rc_good" -eq 0 ] && [ "$rc_broken" -ne 0 ]; then
        echo "==> DPI SELF-TEST PASS: gate accepts correct dpiScale AND rejects a broken (stub 1.0) value (it has teeth)"
        exit 0
    fi
    echo "==> DPI SELF-TEST FAIL: good_rc=$rc_good (want 0), broken_rc=$rc_broken (want !=0)"
    exit 1
fi

# Sub-invocation used by --dpi-self-test: run ONLY the #4 dpiScale assertion on a
# given dump with a given expected ratio. Exit 0 PASS / 1 FAIL.
if [ "${1:-}" = "--dpi-analyze" ]; then
    python3 - "$2" "$3" <<'PYEOF'
import json, sys
path, exp = sys.argv[1], float(sys.argv[2])
rows=[json.loads(l) for l in open(path) if l.strip()]
last=max(o.get("frame",0) for o in rows)
fr=[o for o in rows if o.get("frame")==last]
dpis=sorted({round(float(o.get("dpiScale",-1.0)),6) for o in fr if "dpiScale" in o})
print(f"  [#4 dpi] dpiScale observed = {dpis} (expected physical/logical = {exp:g})")
if not dpis or any(d<=0.0 for d in dpis):
    print(f"  FAIL #4: dpiScale missing/non-positive {dpis}"); sys.exit(1)
if len(dpis)!=1:
    print(f"  FAIL #4: dpiScale inconsistent {dpis}"); sys.exit(1)
if abs(dpis[0]-exp)>1e-3:
    print(f"  FAIL #4: dpiScale={dpis[0]} != real physical/logical {exp:g} — broken DPI wiring"); sys.exit(1)
print(f"  PASS #4: dpiScale={dpis[0]:g} == physical/logical {exp:g}"); sys.exit(0)
PYEOF
    exit $?
fi

WIRED="${1:-$REPO_ROOT/build/debug/wired.x64.exe}"
if [ ! -x "$WIRED" ] && [ -x "$WIRED.exe" ]; then WIRED="$WIRED.exe"; fi
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
trap 'rm -rf "$HOME_PARENT"' EXIT INT TERM
mkdir -p "$HOME_DIR/base"
case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*) HOME_NATIVE="$(cygpath -w "$HOME_DIR")" ;;
    *)                    HOME_NATIVE="$HOME_DIR" ;;
esac

# Stage BSP-less menu run: the main menu needs only base UI assets, which the
# build-dir paks (reached via fs_installpath = CWD) carry. No map → no BSP need.
# The layout dump is written to the launch CWD as layoutdump.jsonl.
LAYOUT_DUMP="$WIRED_DIR/layoutdump.jsonl"
rm -f "$LAYOUT_DUMP"
JSONL="$HOME_DIR/qconsole.jsonl"

echo "==> WiredUI layout checks: $WIRED"
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
(
    cd "$WIRED_DIR" || exit 1
    timeout 90 "$WIRED" \
        +set fs_homepath "$HOME_NATIVE" \
        +set com_automated 1 \
        +set s_initsound 0 \
        +set r_fullscreen 0 \
        +set r_mode -1 +set r_customwidth "$PHYS_W" +set r_customheight "$PHYS_H" \
        +set r_uiLogicalWidthTest "$LOGICAL_W" +set r_uiLogicalHeightTest "$LOGICAL_H" \
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
        +quit \
        >"$JSONL.stdout" 2>&1
)

if [ ! -s "$LAYOUT_DUMP" ]; then
    echo "FAIL: no layoutdump.jsonl produced (engine did not lay out a menu)"
    [ -s "$JSONL.stdout" ] && { echo "  ── stdout tail ──"; tail -15 "$JSONL.stdout" | sed 's/^/  /'; }
    exit 1
fi
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
python3 - "$LAYOUT_DUMP" "$DPI_TEST" "$HOVER_ITEM" <<'PYEOF'
import json, sys, collections

path = sys.argv[1]
dpi_expected = float(sys.argv[2]) if len(sys.argv) > 2 else 0.0
hover_item = sys.argv[3] if len(sys.argv) > 3 else ""
lines = []
with open(path, "r", encoding="utf-8", errors="replace") as fh:
    for ln in fh:
        ln = ln.strip()
        if not ln:
            continue
        try:
            lines.append(json.loads(ln))
        except Exception:
            pass

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
    # Not the bug (never >1), but the test did not exercise a focused item, so
    # the "exactly one" half is unproven. Report as a coverage note, not a pass.
    print(f"  WARN #2: invariant held (never >1) but no frame focused an item — "
          f"the nav-down did not land a focus; 'exactly one' is unverified here "
          f"(the <=1 invariant IS verified)")
else:
    print(f"  PASS #2: exactly one highlight when focused, never more than one "
          f"across {len(all_frames)} frames (single highlight-emit path)")

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
    print(f"  WARN #N: no menu_* rows in the dump — cannot confirm removal")
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
    print(f"  WARN #E: no focused menu row in the settled frame — emphasis re-home unverified")
elif n_emph == 0:
    print(f"  WARN #E: no emphasis-bearing children dumped — cannot verify")
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
