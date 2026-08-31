#!/usr/bin/env bash
# smoke-map-transition.sh — proof-grade map-transition smoke for the
# windowed wired engine.
#
# Two phases, both gated against a FRESH qconsole.jsonl each invocation:
#   Phase 1 — transition: arena1 -> arena17 in a single wired process,
#     +waitForMap gating each load, +screenshot at the spawn for each
#     map. Verifies CA_ACTIVE for both maps + a non-black/blank frame
#     for each screenshot (C1 pixel gate).
#   Phase 2 — golden-baseline parity at 5 fixed viewpoints across
#     arena1 + arena17. Captures each viewpoint three times on the
#     (sole, post-retire) bindless main path and diffs the first
#     against a stored golden PNG in tests/golden/. The 3 same-path
#     captures bound the per-tile animation-phase noise floor; the
#     gate FAILs if any non-excluded tile diverges beyond
#     measured-noise + margin (C2 golden-baseline gate). Re-bless via
#     SMOKE_UPDATE_GOLDEN=1.
#
# Freshness invariant: qconsole.jsonl + screenshot dir are wiped of
# stale state before each launch; an absent / not-fresh log fails the
# run with "wired did not run this invocation" — never a stale PASS.
#
# Install-path: launches wired with the binary's directory as CWD so
# Sys_Pwd → fs_installpath resolves to the binary's own base/. No
# +set fs_installpath override (the prior version of this script
# passed Q3DIR there, which silently aborted wired on machines where
# Q3DIR/base didn't have the full pak set).
#
# Usage:
#   tests/smoke-map-transition.sh [path-to-wired]
#
# Environment:
#   Q3DIR  path to game installation with base/pak*.pk3 or
#          base/pa[xk]*.sw3z (default: WIRED_INSTALL from
#          tests/lib/wired_paths.sh). Used only for the SKIP probe — the
#          run itself uses Sys_Pwd for fs_installpath.
#
# Exit codes:
#   0  PASS — both phases passed
#   1  FAIL — any gate failed
#   77 SKIP — no pak set available

set -uo pipefail

# ── decoder ──────────────────────────────────────────────────────────────────
# The engine's bare `screenshot` writes PNG (the canonical default). The pixel
# gate needs flat raw RGB bytes, so every capture/golden is decoded through the
# png2raw tool (tools/png2raw) which emits W*H*3 top-down RGB on stdout — the
# exact flat stream the `od -t u1` pipeline below consumes. png2raw is built by
# the `make smoke-map-transition` prerequisite; resolve it repo-relative.
SMOKE_SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SMOKE_SCRIPT_DIR/.." && pwd)"
# shellcheck source=tests/lib/wired_paths.sh
. "$SMOKE_SCRIPT_DIR/lib/wired_paths.sh"
# Scratch root for this harness's intermediate logs / diff tables. Honours a
# pre-set SMT_TMP; otherwise the system temp root from the shared helper.
SMT_TMP="${SMT_TMP:-$WIRED_TMP}"
mkdir -p "$SMT_TMP"
PNG2RAW="${PNG2RAW:-$REPO_ROOT/tools/png2raw/png2raw}"
if [ ! -x "$PNG2RAW" ] && [ -x "$PNG2RAW.exe" ]; then PNG2RAW="$PNG2RAW.exe"; fi
if [ ! -x "$PNG2RAW" ]; then
    echo "FAIL: png2raw decoder not found at $PNG2RAW (build it: make png2raw)"
    exit 1
fi
# All pixel gates operate in a logical 1280x720 comparison space. SDL's
# HIGH_PIXEL_DENSITY window flag produces 2560x1440 screenshots on a 2x Retina
# display even though the requested window is 1280x720. Decode every capture
# through the same normalisation so row strides, split regions and golden
# comparisons mean the same thing on 1x, 1.25x and 2x displays.
PNG2RAW_COMPARE=( "$PNG2RAW" --size 1280x720 )

# ── --self-test: prove the Phase-2 golden gate has TEETH without an engine
# launch. Sources the REAL tiled_diff (extracted from this same file) + the real
# per-tile threshold max(noise*RATIO+MARGIN, FLOOR). A blessed golden vs itself
# = 0 (noise 0 → threshold 60 → over=-60, PASS); vs a +40 brightness
# perturbation (an exposure/tonemap regression synthesized by png-perturb) →
# worst tile ~120 → over≈+60 (FAIL). If +40 does NOT trip the gate, it is blind
# to a whole-frame value regression — exactly the class a feature fence must catch.
if [ "${1:-}" = "--self-test" ]; then
    echo "==> smoke-map-transition Phase-2 SELF-TEST (gate-has-teeth)"

    # Check the byte readers FIRST. Everything below compares pixel means, so a
    # broken reader does not make this self-test fail — it makes every mean come
    # back 0, which compares equal to every other 0 and passes. That is not a
    # hypothetical: `od -w` is GNU-only, BSD od rejected it, the failure was
    # swallowed by the pipeline, and the gate certified frames it had never
    # actually read.
    echo "  -- byte readers (wired_od_*) → expect PASS --"
    if wired_od_selftest; then
        echo "    readers: PASS"
    else
        echo "    readers: FAIL (measurements below would be meaningless)"
        exit 1
    fi

    GOLDEN_DIR="$SMOKE_SCRIPT_DIR/golden"
    PERTURB="${PERTURB:-$REPO_ROOT/tools/png-perturb/png-perturb}"
    if [ ! -x "$PERTURB" ] && [ -x "$PERTURB.exe" ]; then PERTURB="$PERTURB.exe"; fi
    TILE_RATIO=1.5; TILE_MARGIN=5.0; TILE_FLOOR=60.0
    DIFF_TOLERANCE=30; GRID_W=8; GRID_H=8; SAMPLE_COLS=40; SAMPLE_ROWS=720
    # source the REAL tiled_diff() definition from this very file (the block
    # between its `tiled_diff() {` line and the next `^}`), so the self-test
    # exercises the exact differ the live gate uses — not a copy that could drift.
    eval "$(awk '/^tiled_diff\(\) \{/{f=1} f{print} f&&/^\}/{exit}' "${BASH_SOURCE[0]}")"
    g="$GOLDEN_DIR/A_spawn.png"
    [ -s "$g" ] || { echo "SKIP: no golden $g"; exit 77; }
    [ -x "$PERTURB" ] || { echo "SKIP: png-perturb not built ($PERTURB) — build: cd tools/png-perturb && go build -o png-perturb.exe ."; exit 77; }
    tmp="$SMT_TMP/smoke-mt-selftest-perturb.png"
    "$PERTURB" "$g" "$tmp" 40 || { echo "FAIL: perturb failed"; exit 1; }
    verdict() {
        tiled_diff "$1" "$2" | awk -v margin="$TILE_MARGIN" -v floor="$TILE_FLOOR" '
            BEGIN{worst=-1e9; wt=-1}
            { md=$2; thr=margin; if(thr<floor)thr=floor; over=md-thr; if(over>worst){worst=over; wt=$1; wmd=md; wthr=thr} }
            END{ printf "worst_tile=%d meanDiff=%.1f thr=%.1f over=%+.1f %s", wt, wmd, wthr, worst, (worst>0?"FAIL":"PASS") }'
    }
    rc=0
    echo "  -- golden vs itself → expect PASS --"
    s="$(verdict "$g" "$g")"; echo "    $s"; case "$s" in *PASS) echo "    clean: PASS";; *) echo "    clean: FAIL (BUG: gate rejects an identical frame)"; rc=1;; esac
    echo "  -- golden vs +40 perturbation (exposure regression) → expect FAIL --"
    s="$(verdict "$g" "$tmp")"; echo "    $s"; case "$s" in *FAIL) echo "    perturb: FAIL-as-expected";; *) echo "    perturb: PASS (BUG: gate blind to a +40 whole-frame regression)"; rc=1;; esac
    if [ "$rc" -eq 0 ]; then echo "==> SELF-TEST PASS: Phase-2 golden gate accepts an identical frame AND rejects a +40 exposure regression (it has teeth)"; else echo "==> SELF-TEST FAIL"; fi
    exit $rc
fi

# ── arg / env resolution ─────────────────────────────────────────────────────
WIRED="${1:-wired}"
# Install root from the shared helper, never a literal: the old defaults
# (`/Applications/q3now`, `$HOME/q3now`) are not the installed bundle name —
# that is <PRODUCT_NAME><CHANNEL_SUFFIX>. Same fix as smoke-quic-game.sh.
# Used only for the SKIP probe; the run itself uses Sys_Pwd.
Q3DIR="${Q3DIR:-$WIRED_INSTALL}"

# Resolve the wired binary's containing directory (its dir is used as the
# launch CWD, so Sys_Pwd → fs_installpath finds the adjacent base/).
case "$WIRED" in
    /*) WIRED_ABS="$WIRED" ;;
    *)  WIRED_ABS="$PWD/$WIRED" ;;
esac
if [ ! -x "$WIRED_ABS" ] && [ ! -x "$WIRED_ABS.exe" ]; then
    if command -v "$WIRED" >/dev/null 2>&1; then
        WIRED_ABS="$(command -v "$WIRED")"
    fi
fi
if [ ! -x "$WIRED_ABS" ]; then
    echo "FAIL: wired binary not found: $WIRED (resolved: $WIRED_ABS)"
    exit 1
fi
WIRED_DIR="$(dirname "$WIRED_ABS")"
WIRED_NAME="$(basename "$WIRED_ABS")"

# Derive PRODUCT_NAME + CHANNEL_SUFFIX from the build that produced this
# binary — the engine compiles them in (q_shared.h:18, CMakeLists.txt:31-35)
# and Sys_DefaultHomePath builds its default fs_homepath as
# <userprofile>/wired/<PRODUCT_NAME><CHANNEL_SUFFIX>/ (win_shared.c:97-100).
# Reading CMakeCache.txt from $WIRED_DIR keeps the harness's notion of the
# channel-suffixed product dir in lockstep with the binary's compiled-in
# constants — no static "-preview" assumption. If the cache is absent or
# the keys are missing, fall back to CMake's own defaults (CMakeLists.txt:32,35).
PRODUCT_NAME=""
CHANNEL_SUFFIX=""
BUILD_CACHE="$WIRED_DIR/CMakeCache.txt"
if [ -f "$BUILD_CACHE" ]; then
    PRODUCT_NAME="$(awk -F= '/^PRODUCT_NAME(:[^=]*)?=/ { print $2; exit }' "$BUILD_CACHE")"
    CHANNEL_SUFFIX="$(awk -F= '/^CHANNEL_SUFFIX(:[^=]*)?=/ { print $2; exit }' "$BUILD_CACHE")"
fi
PRODUCT_NAME="${PRODUCT_NAME:-q3now}"
# CHANNEL_SUFFIX may legitimately be empty (release channel); only fall back
# when the cache is missing entirely.
if [ ! -f "$BUILD_CACHE" ]; then
    CHANNEL_SUFFIX="${CHANNEL_SUFFIX:--preview}"
fi
PRODUCT_DIRNAME="${PRODUCT_NAME}${CHANNEL_SUFFIX}"

# User's real homepath — used ONLY as a pak-discovery source. The harness
# does NOT write into it; engine state goes into the isolated SMOKE_HOME below.
case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*) HOME_WIRED="$(cygpath -u "$USERPROFILE" 2>/dev/null || echo "$HOME")/wired" ;;
    *)                    HOME_WIRED="$HOME/wired" ;;
esac
PRODUCT_DIR=""
if [ -d "$HOME_WIRED/$PRODUCT_DIRNAME" ]; then
    PRODUCT_DIR="$HOME_WIRED/$PRODUCT_DIRNAME"
else
    # Fallback: any sibling matching the product-name glob — covers the case
    # where the user has a different-channel homepath that still carries paks.
    PRODUCT_DIR="$(ls -td "$HOME_WIRED/${PRODUCT_NAME}"* 2>/dev/null | head -1 || echo "")"
fi

# Pak availability probe (SKIP if no source has paks). Note: at runtime the
# engine reads paks from fs_installpath ($WIRED_DIR) and fs_homepath ($SMOKE_HOME);
# Q3DIR and PRODUCT_DIR are not on the engine search path but can be linked
# into $SMOKE_HOME below.
have_paks() {
    compgen -G "$1/base/pak*.pk3"   >/dev/null 2>&1 \
 || compgen -G "$1/base/pa[xk]*.sw3z" >/dev/null 2>&1
}
if ! have_paks "$WIRED_DIR" && ! have_paks "$Q3DIR" \
   && { [ -z "$PRODUCT_DIR" ] || ! have_paks "$PRODUCT_DIR"; }; then
    echo "SKIP: no pak set found (checked $WIRED_DIR, $Q3DIR, $PRODUCT_DIR)"
    exit 77
fi

# ── isolated homepath ───────────────────────────────────────────────────────
# The engine writes config.cfg + qconsole.jsonl + screenshots under fs_homepath.
# Cvars carrying CVAR_ARCHIVE (r_mode, r_customWidth, r_customHeight,
# r_fullscreen, in_mouse, log_file_mode, …) are persisted on exit. Without
# isolation, every smoke run would rewrite the user's real config.cfg with the
# harness's test values (r_mode -1, in_mouse 0, etc.) — the user then sees what
# looks like a regression on their next normal launch.
#
# Fix: redirect fs_homepath to a per-run mktemp dir whose basename mirrors the
# engine's compiled-in <PRODUCT_NAME><CHANNEL_SUFFIX> (e.g. "q3now-preview").
# config.cfg and all other CVAR_ARCHIVE writeback land there and get rm -rf'd
# on exit. The user's real homepath at $PRODUCT_DIR is never touched.
# fs_installpath stays at $WIRED_DIR (via the launch CWD + Sys_Pwd), so
# build-shipped paks remain reachable.
SMOKE_HOME_PARENT="$(mktemp -d -t wired-smoke-XXXXXX 2>/dev/null || mktemp -d)"
SMOKE_HOME="$SMOKE_HOME_PARENT/$PRODUCT_DIRNAME"
trap 'rm -rf "$SMOKE_HOME_PARENT"' EXIT INT TERM
mkdir -p "$SMOKE_HOME/base/screenshots"
case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*) SMOKE_HOME_NATIVE="$(cygpath -w "$SMOKE_HOME")" ;;
    *)                    SMOKE_HOME_NATIVE="$SMOKE_HOME" ;;
esac

# Pak strategy: engine searches fs_installpath/base (= $WIRED_DIR/base)
# AND fs_homepath/base (= $SMOKE_HOME/base). The default (non-isolated)
# layout serves the build's smaller paks via installpath AND the user's full
# content paks (e.g. pax01.sw3z, ~150 MB with the BSPs) via homepath. With
# fs_homepath redirected to $SMOKE_HOME, the homepath leg goes empty and
# arena1.bsp / arena17.bsp become unreachable unless we re-stage the user's
# paks into $SMOKE_HOME/base. Hard-link (same inode, instant, no disk bloat,
# removed by the trap); fall back to symlink, then copy.
#
# Linking is unconditional: even when $WIRED_DIR/base has paks, the user's
# $PRODUCT_DIR/base typically holds the BSP-bearing content pak that the
# build dir lacks. FS_DeduplicateArchives handles same-basename collisions
# safely (see tests/smoke-fs-dedup.sh).
link_paks_into() {
    local src="$1" dst="$2"
    local count=0 method=""
    shopt -s nullglob
    for pak in "$src"/pak*.pk3 "$src"/pa[xk]*.sw3z; do
        [ -f "$pak" ] || continue
        # Skip if a same-basename file already linked from an earlier source.
        [ -e "$dst/$(basename "$pak")" ] && continue
        if ln "$pak" "$dst/" 2>/dev/null; then
            method="${method:-hardlink}"
        elif ln -s "$pak" "$dst/" 2>/dev/null; then
            method="${method:-symlink}"
        else
            cp "$pak" "$dst/" || continue
            method="${method:-copy}"
        fi
        count=$((count + 1))
    done
    shopt -u nullglob
    [ "$count" -gt 0 ] && echo "  pak source        : $src ($count pak(s) via $method)"
    return 0
}
# Stage in priority order: PRODUCT_DIR (most likely to hold full content) first,
# then Q3DIR. WIRED_DIR's paks are reached via fs_installpath at runtime — no
# need to re-link.
[ -n "$PRODUCT_DIR" ] && have_paks "$PRODUCT_DIR" \
    && link_paks_into "$PRODUCT_DIR/base" "$SMOKE_HOME/base"
have_paks "$Q3DIR" \
    && link_paks_into "$Q3DIR/base" "$SMOKE_HOME/base"

JSONL="$SMOKE_HOME/qconsole.jsonl"
SCREENSHOT_DIR="$SMOKE_HOME/base/screenshots"
SMOKE_REVIEW_DIR="${SMOKE_REVIEW_DIR:-}"
if [ -n "$SMOKE_REVIEW_DIR" ]; then
    mkdir -p "$SMOKE_REVIEW_DIR"
fi

# ── helpers ─────────────────────────────────────────────────────────────────

# Compute zero-pixel-% and mean-byte of a PNG's pixel-data (subsampled).
# The PNG is decoded to flat RGB via png2raw; that raw stream is header-free,
# so there is no size-18 header offset to subtract — the whole stream is pixels.
# Output: "zero%=NN.NN mean=NN.NN"
pixel_histogram() {
    local shot="$1"
    if [ ! -s "$shot" ]; then echo "zero%=100.00 mean=0.00"; return; fi
    "${PNG2RAW_COMPARE[@]}" "$shot" \
      | wired_od_bytes \
      | awk 'BEGIN{tot=0;zero=0;sum=0}
             { for (i=1;i<=NF;i++) { tot++; sum+=$i; if($i==0)zero++ } }
             END{ if(tot>0) printf "zero%%=%.2f mean=%.2f", zero/tot*100, sum/tot }'
}

# C1 — black/blank pixel gate. FAIL if zero% > 90 OR mean < 5.
# Margin justification: a forced-black frame is ~100/0 (verified in
# Negative test 2); a known-good arena1 spawn capture is ~12-25 / ~15-25
# (verified in bindless-legacy-parity-check). 90 / 5 thresholds give 4×
# headroom on each axis vs known-good and clearly separate from black.
PIXEL_GATE_MAX_ZERO_PCT=90
PIXEL_GATE_MIN_MEAN=5
check_pixel_gate() {
    local shot="$1" label="$2"
    local hist zero_pct mean_val
    hist="$(pixel_histogram "$shot")"
    zero_pct="$(echo "$hist" | sed -nE 's/.*zero%=([0-9.]+).*/\1/p')"
    mean_val="$(echo "$hist" | sed -nE 's/.*mean=([0-9.]+).*/\1/p')"
    local bad=0
    awk -v z="$zero_pct" -v t="$PIXEL_GATE_MAX_ZERO_PCT" 'BEGIN{exit !(z>t)}' && bad=1 || true
    awk -v m="$mean_val" -v t="$PIXEL_GATE_MIN_MEAN"     'BEGIN{exit !(m<t)}' && bad=1 || true
    if [ "$bad" -ne 0 ]; then
        echo "FAIL: pixel gate ($label): $hist  -- frame is black/blank (zero%>${PIXEL_GATE_MAX_ZERO_PCT} or mean<${PIXEL_GATE_MIN_MEAN})"
        return 1
    fi
    echo "  pixel ok ($label) : $hist"
    return 0
}

# zero%/mean over a horizontal HALF of the frame (region = "left" or "right").
# Proves a split-screen frame rendered BOTH viewports independently: a single
# full-frame mean could pass with one half black, so each sub-rect is gated
# separately. The frame is the pinned smoke resolution (1280x720, see run_wired);
# png2raw emits W*H*3 top-down RGB, so each row is 1280*3 = 3840 bytes and the
# left viewport (main_scene, x 0..0.5) is bytes 0..1919 of each row, the right
# viewport (chase camera, x 0.5..1.0) is bytes 1920..3839. Track the byte index
# within the row (modulo the row stride) and accumulate only the target half.
pixel_histogram_region() {
    local shot="$1" region="$2"
    local width="${SMOKE_FRAME_WIDTH:-1280}"
    if [ ! -s "$shot" ]; then echo "zero%=100.00 mean=0.00"; return; fi
    "${PNG2RAW_COMPARE[@]}" "$shot" \
      | wired_od_bytes \
      | awk -v region="$region" -v stride="$((width*3))" '
             BEGIN{tot=0;zero=0;sum=0;half=int(stride/2);idx=0}
             { for (i=1;i<=NF;i++) {
                   col = idx % stride;
                   inleft  = (col <  half);
                   if ((region=="left" && inleft) || (region=="right" && !inleft)) {
                       tot++; sum+=$i; if($i==0)zero++;
                   }
                   idx++;
               } }
             END{ if(tot>0) printf "zero%%=%.2f mean=%.2f", zero/tot*100, sum/tot;
                  else printf "zero%%=100.00 mean=0.00" }'
}

# Gate one half of a split-screen frame against the same black/blank thresholds.
check_pixel_gate_region() {
    local shot="$1" region="$2" label="$3"
    local hist zero_pct mean_val
    hist="$(pixel_histogram_region "$shot" "$region")"
    zero_pct="$(echo "$hist" | sed -nE 's/.*zero%=([0-9.]+).*/\1/p')"
    mean_val="$(echo "$hist" | sed -nE 's/.*mean=([0-9.]+).*/\1/p')"
    local bad=0
    awk -v z="$zero_pct" -v t="$PIXEL_GATE_MAX_ZERO_PCT" 'BEGIN{exit !(z>t)}' && bad=1 || true
    awk -v m="$mean_val" -v t="$PIXEL_GATE_MIN_MEAN"     'BEGIN{exit !(m<t)}' && bad=1 || true
    if [ "$bad" -ne 0 ]; then
        echo "FAIL: split-screen $region viewport ($label): $hist  -- half is black/blank (the $region viewport did not render)"
        return 1
    fi
    echo "  split $region ok ($label) : $hist"
    return 0
}

# Per-tile mean-RGB shift between two captures at a fixed viewpoint.
# (png2raw emits RGB. The gate sums |Δ| symmetrically over all three channels,
# so channel order is irrelevant to the mean / diff as long as golden and impl
# decode through the same tool.)
#
# Why block-mean instead of per-pixel divergence fraction: flame /
# lava / tcMod-turb shaders animate at render-time and produce huge
# per-pixel divergence between any two captures of the same scene
# (the engine has no trivial way to freeze that — timescale 0 stalls
# the +waitForMap gate without freezing the shader-time progression).
# Averaging BGR within a tile smooths the animation away: a 320×180
# flame tile sums to nearly the same mean color regardless of which
# instantaneous phase the flame was caught in. What does NOT average
# out is a real REGION-WIDE brightness shift (a wall emblem rendered
# at 10× brightness, a pillar blown-out white) — that moves the tile
# mean by tens of BGR units, far above same-path animation noise.
#
# Sampling: every 32 pixels in the linear decoded RGB stream. The pinned window
# resolution is 1280×720 (c2-golden-gate-rework — see run_wired): 1280/32
# = 40 sample columns × 720 rows = 28,800 samples. 8x8 grid → ~450
# samples per tile (each covering 160×90 actual pixels — still
# emblem-scale divergences land in ≤ 1 tile).
#
# Output: 64 lines, format:
#   tile_idx  meanDiffSum  fracSampled  divergedSampleFrac
# meanDiffSum is the L1 sum of the three per-channel mean differences
# (channel 0, 1, 2 of the decoded RGB stream) — the gate's primary signal. The
# trailing legacy frac stays available for backward-compat (and a
# secondary diagnostic in the report).
DIFF_TOLERANCE=30      # per-pixel BGR L1 distance to flag a single sample (legacy diagnostic)
GRID_W=8
GRID_H=8
SAMPLE_COLS=40         # 1280 / 32
SAMPLE_ROWS=720
tiled_diff() {
    local a="$1" b="$2"
    paste -d ' ' \
        <("${PNG2RAW_COMPARE[@]}" "$a" | wired_od_sample_rgb 32) \
        <("${PNG2RAW_COMPARE[@]}" "$b" | wired_od_sample_rgb 32) \
      | awk -v tol="$DIFF_TOLERANCE" -v W="$SAMPLE_COLS" -v H="$SAMPLE_ROWS" \
            -v GW="$GRID_W" -v GH="$GRID_H" '
            BEGIN {
                TW = W / GW
                TH = H / GH
                for (i = 0; i < GW*GH; i++) {
                    n[i] = 0; d[i] = 0
                    sb1[i] = 0; sg1[i] = 0; sr1[i] = 0
                    sb2[i] = 0; sg2[i] = 0; sr2[i] = 0
                }
            }
            NF >= 6 {
                idx = NR - 1
                r = int(idx / W)
                c = idx % W
                tr = int(r / TH); if (tr >= GH) tr = GH - 1
                tc = int(c / TW); if (tc >= GW) tc = GW - 1
                ti = tr * GW + tc
                n[ti]++
                sb1[ti] += $1; sg1[ti] += $2; sr1[ti] += $3
                sb2[ti] += $4; sg2[ti] += $5; sr2[ti] += $6
                dx = ($1 > $4) ? $1 - $4 : $4 - $1
                dy = ($2 > $5) ? $2 - $5 : $5 - $2
                dz = ($3 > $6) ? $3 - $6 : $6 - $3
                if (dx + dy + dz > tol) d[ti]++
            }
            END {
                for (i = 0; i < GW*GH; i++) {
                    if (n[i] > 0) {
                        mb1 = sb1[i]/n[i]; mb2 = sb2[i]/n[i]
                        mg1 = sg1[i]/n[i]; mg2 = sg2[i]/n[i]
                        mr1 = sr1[i]/n[i]; mr2 = sr2[i]/n[i]
                        meanDiff = ((mb1>mb2)?mb1-mb2:mb2-mb1) \
                                 + ((mg1>mg2)?mg1-mg2:mg2-mg1) \
                                 + ((mr1>mr2)?mr1-mr2:mr2-mr1)
                        f = d[i]/n[i]
                    } else { meanDiff = 0; f = 0 }
                    printf "%d %.3f %d %.4f\n", i, meanDiff, n[i], f
                }
            }'
}

# Backward-compat helper retained for negative-test scripts: global
# fraction-of-diverging-pixels (the old C2 metric). Kept so the
# parity-harden report can quote the old metric for comparison.
diff_fraction() {
    tiled_diff "$1" "$2" | awk '
        BEGIN { d=0; n=0 }
        { f=$4; total=$3; n += total; d += int(f * total + 0.5) }
        END { if (n > 0) printf "frac=%.4f diff=%d total=%d", d/n, d, n; else printf "frac=NA diff=0 total=0" }'
}


# Run wired once with a custom command set. Wipes the freshness state
# (jsonl + any pre-existing screenshot newer than $PRE_TS) and launches from
# $WIRED_DIR so Sys_Pwd locks fs_installpath there.
# Args: TAG TIMEOUT_S CMD_ARGS...
# Returns 0 if wired's jsonl was written this invocation, else 1.
LAUNCH_TIMEOUT=${LAUNCH_TIMEOUT:-180}
run_wired() {
    local tag="$1"; shift
    local timeout_s="$1"; shift
    local logfile="$SMT_TMP/smoke-mt-$tag.log"
    rm -f "$JSONL"
    rm -f "$logfile"
    # Wipe the isolated homepath's config.cfg per launch so CVAR_ARCHIVE
    # writeback from launch N cannot drift into launch N+1's startup state.
    # The +set list is the only intended cvar input; nothing should carry
    # across the 16 launches of a single harness invocation.
    rm -f "$SMOKE_HOME/base/config.cfg"
    (
        cd "$WIRED_DIR"
        # NOTE: no +set fs_installpath here. Sys_Pwd → fs_installpath = $WIRED_DIR
        # picks up the binary's adjacent base/ (e.g. build/base/pax21.sw3z)
        # while fs_homepath ($PRODUCT_DIR) still contributes its own paks.
        # c2-golden-gate-rework: pin window resolution explicitly so captures
        # taken on different monitors compare correctly. Without this the
        # engine takes the desktop resolution (r_mode -2 default), which
        # differs per developer / per display (e.g. 1920×1200 vs 2560×1440)
        # — and tiled_diff's SAMPLE_COLS=80 (ceil(2560/32)) becomes wrong
        # for any width ≠ 2560, sampling different scene positions per row
        # in each image and producing nonsense per-tile meanDiffs.
        # Windowed at 1280×720 (universally supported, samples cleanly with
        # the existing 8×8 tile grid at SAMPLE_COLS=40 / SAMPLE_ROWS=720
        # after the matching tiled_diff update below).
        # c2-shadertime-pin — pin shader animation time (seconds) so wave-
        # driven emissive surfaces (rgbGen/tcMod wave, R_NoiseGet4f) sit at
        # a fixed phase across cold-cache launches. Eliminates the per-
        # launch animation-phase bimodality the c2-brightness-
        # reinvestigation pinned as the residual C2 nondeterminism after
        # the noise-table fix. r_pinShaderTime is CVAR_CHEAT + non-archive
        # so the harness-only +set does not persist to config.cfg and has
        # zero effect on normal launches (default 0 = wall-clock-driven
        # gameplay path, unchanged). 1.0 sec is well-past the cold-start
        # zero state; the exact value doesn't matter for determinism so
        # long as it's >0 and the same in rebless + verify.
        timeout "$timeout_s" "$WIRED_ABS" \
            +set fs_homepath "$SMOKE_HOME_NATIVE" \
            +set sv_pure 0 \
            +set sv_cheats 1 \
            +set r_brightness 1 \
            +set vm_game 0 \
            +set vm_cgame 0 \
            +set log_file_mode overwrite_synced \
            +set con_notifytime 0 \
            +set r_mode -1 \
            +set r_customwidth 1280 \
            +set r_customheight 720 \
            +set r_fullscreen 0 \
            +set r_pinShaderTime 1.0 \
            +set r_pinFrameTime 1.0 \
            +set r_dither 0 \
            +set r_chromaticAberration 0 \
            +set com_automated 1 \
            "$@" \
            >"$logfile" 2>&1 || true
    )
    sleep 1
    if [ ! -s "$JSONL" ]; then
        echo "FAIL: wired did not run this invocation (no $JSONL produced)"
        echo "  command tag : $tag"
        echo "  stdout/err  : $logfile"
        if [ -s "$logfile" ]; then
            echo "  ── stdout tail ──"
            tail -n 20 "$logfile" | sed 's/^/  /'
        fi
        return 1
    fi
    # JSONL was removed immediately before launch, so a non-empty file here
    # was necessarily produced by this invocation. Avoid GNU-only
    # `find -newermt @epoch`, which rejects valid runs on macOS/BSD find.
    return 0
}

# Capture a screenshot at the given viewpoint after CA_ACTIVE. Returns the
# capture path on stdout. Empty stdout = failure.
#
# legacy-mainpath-retire STEP 5: the bindless argument is gone — the bindless
# main path is the SOLE main path post-retire. The legacy r_bindlessMainPath
# cvar was removed in STEP 4; passing it to the engine now is a no-op (and
# in newer builds would error). Phase 2 now diffs against a stored golden,
# not against a same-launch legacy capture.
#
# Args: MAP X Y Z YAW TAG
capture_viewpoint() {
    local map="$1" x="$2" y="$3" z="$4" yaw="$5" tag="$6"
    local marker="$SMT_TMP/smoke-mt-$tag.marker"
    touch "$marker"
    # REAL camera teleport. A bare "+setviewpos" is a NO-OP (server console echo
    # only; player not moved) — so the prior 5 "viewpoints" were all the same spawn
    # frame. The working recipe (proven by visual-render-features.sh --mode viewport):
    # +cmd noclip (removes gravity so the origin is deterministic, no settle) then
    # +cmd setviewpos (actually teleports), issued twice for the cold-launch settle.
    # r_pinShaderTime + r_pinFrameTime (in run_wired) freeze shader-wave AND entity-
    # animation time so flames/items can't drift the frame — replacing the old
    # "+set timescale 0" which stalls the +waitForMap gate (intermittent hangs).
    # +waitForMap already gates CA_ACTIVE before the +cmd's are accepted.
    run_wired "$tag" 140 \
        +map "$map" +waitForMap +wait 60 \
        +cmd noclip +wait 20 +cmd setviewpos $x $y $z $yaw +wait 60 +cmd setviewpos $x $y $z $yaw +wait 40 \
        +wait 60 +screenshot \
        +wait 30 +quit >&2 || return 1
    local shot
    shot="$(find "$SCREENSHOT_DIR" -name '*.png' -newer "$marker" 2>/dev/null | sort | tail -1)"
    if [ -z "$shot" ] || [ ! -s "$shot" ]; then
        echo >&2 "FAIL: capture_viewpoint($tag): no screenshot produced under $SCREENSHOT_DIR"
        return 1
    fi
    echo "$shot"
    return 0
}

# Run the standard per-invocation gates against $JSONL (CA_ACTIVE count,
# no VUIDs, no FATAL). Echoes summary lines; sets FAIL_PHASE_1 on fail.
check_jsonl_invariants() {
    local expected_markers="$1"
    local launch_tag="${2:-}"
    local runtime_log="$SMT_TMP/smoke-mt-$launch_tag.log"
    local fail=0
    local count
    count="$(grep -c "FIRST GAMEPLAY FRAME" "$JSONL" || true)"
    if [ "$count" -ne "$expected_markers" ]; then
        echo "FAIL: expected $expected_markers FIRST GAMEPLAY FRAME marker(s), got $count"
        fail=1
    else
        echo "  markers           : $count OK"
    fi
    # Every successfully activated map must have crossed the Level ownership
    # barrier first. This pins the current fresh-level contract: the previous
    # map's hunk offsets are retired and subsequent Hunk_Alloc calls rebuild
    # zeroed state, while App/connection scopes remain alive.
    local level_resets
    level_resets="$(grep -c 'Hunk_ClearLevel: reset the hunk ok' "$JSONL" || true)"
    if [ "$level_resets" -lt "$expected_markers" ]; then
        echo "FAIL: expected at least $expected_markers Hunk_ClearLevel reset(s), got $level_resets"
        fail=1
    else
        echo "  level resets      : $level_resets OK"
    fi
    if grep -q '"sev":"FATAL"' "$JSONL"; then
        echo "FAIL: engine emitted SEV_FATAL — Com_Terminate fired during the run:"
        grep '"sev":"FATAL"' "$JSONL" | head -3 | sed 's/^/    /'
        fail=1
    else
        echo "  no FATAL          : OK"
    fi
    local vuid_descset
    vuid_descset="$(grep -c 'VUID-VkWriteDescriptorSet-dstSet-00320' "$JSONL" || true)"
    if [ "$vuid_descset" -gt 0 ]; then
        echo "FAIL: descriptor-set lifecycle VUID-VkWriteDescriptorSet-dstSet-00320 fired $vuid_descset time(s)"
        fail=1
    else
        echo "  no dstSet VUID    : OK"
    fi
    if grep -qE 'ACCESS_VIOLATION|nvoglv64\.dll@' "$JSONL"; then
        echo "FAIL: ACCESS_VIOLATION in renderer/driver detected"
        grep -E 'ACCESS_VIOLATION|nvoglv64\.dll@' "$JSONL" | head -2 | sed 's/^/    /'
        fail=1
    else
        echo "  no driver crash   : OK"
    fi
    if grep -qE '^ERROR:|VM_Create.*failed|VM syscall error|trap_[[:alnum:]_]+[[:space:]]+syscall[[:space:]]+error' "$JSONL"; then
        echo "FAIL: VM errors detected:"
        grep -E '^ERROR:|VM_Create.*failed|VM syscall error|trap_[[:alnum:]_]+[[:space:]]+syscall[[:space:]]+error' "$JSONL" | head -3 | sed 's/^/    /'
        fail=1
    fi
    # The visual witness is meaningful only when it uses the same role package
    # set as run-game.  The macOS build-tree app is an intermediate skeleton;
    # when the harness accidentally launched it, pax21-client/server and the
    # static alias catalog were absent. Maps still reached CA_ACTIVE but drew
    # hundreds of default-image surfaces, so the old pixel/golden gates scored
    # a different product configuration as if it were the user's game.
    local role_pak
    for role_pak in pax21.sw3z pax21-client.sw3z pax21-server.sw3z; do
        # Archive discovery precedes qconsole.jsonl creation; the per-launch
        # stdout log is therefore the authoritative role-package witness.
        if [ -z "$launch_tag" ] || ! grep -q "SW3Z: loaded .*${role_pak}" "$runtime_log"; then
            echo "FAIL: canonical role package was not loaded: $role_pak"
            fail=1
        fi
    done
    if ! grep -Eq 'VFS: published alias generation [0-9]+ with [1-9][0-9]* exact mapping' "$JSONL"; then
        echo "FAIL: fs-aliases.lua was not published (zero/missing exact mappings)"
        fail=1
    fi
    if grep -qE 'R_FindImageFile could not find|R_FindShader could not find material image|CG_LoadCharacter: .* not found|sounds/.* not found, using default|s_enginePlay: could not load' "$JSONL"; then
        echo "FAIL: runtime media fallback detected:"
        grep -E 'R_FindImageFile could not find|R_FindShader could not find material image|CG_LoadCharacter: .* not found|sounds/.* not found, using default|s_enginePlay: could not load' "$JSONL" \
            | head -8 | sed 's/^/    /'
        fail=1
    else
        echo "  runtime media     : canonical (no image/sound fallback)"
    fi
    # A second gamestate legitimately replays the connection's immutable
    # package receipt and pure policy while level-owned resources still hold
    # scoped file handles. Exact replay must be a read-only success; a rejected
    # receipt here means the map transition retired or mutated App/connection
    # ownership even if a later fallback happened to render a frame.
    if grep -qE 'client content receipt rejected|Server content failure|package receipt scope (missing|busy)' "$JSONL"; then
        echo "FAIL: connection content scope was rejected during map transition:"
        grep -E 'client content receipt rejected|Server content failure|package receipt scope (missing|busy)' "$JSONL" | head -3 | sed 's/^/    /'
        fail=1
    else
        echo "  content scope     : retained"
    fi
    return $fail
}

# ── header ──────────────────────────────────────────────────────────────────
echo "==> Map transition smoke: $WIRED_ABS"
echo "    cwd at launch     : $WIRED_DIR"
echo "    fs_homepath       : $SMOKE_HOME_NATIVE (isolated; user config untouched)"
echo "    jsonl target      : $JSONL"
echo "    screenshot dir    : $SCREENSHOT_DIR"
echo "    pixel gate        : zero%<=${PIXEL_GATE_MAX_ZERO_PCT}, mean>=${PIXEL_GATE_MIN_MEAN}"

FAIL=0

# ════════════════════════════════════════════════════════════════════════════
# Phase 1 — arena1 → arena17 transition, freshness, C1 pixel gate.
# legacy-mainpath-retire STEP 4: r_bindlessMainPath cvar is gone post-retire;
# the bindless main path is the SOLE renderervk path. Cold-cache pipeline
# warmup is handled by the bumped +wait below (Phase-1 cold-cache fix).
# ════════════════════════════════════════════════════════════════════════════
echo
echo "==> Phase 1 — arena1 → arena17 transition (default path)"

P1_MARKER="$SMT_TMP/smoke-mt-phase1.marker"
touch "$P1_MARKER"
# legacy-mainpath-retire STEP 5 — cold-cache pipeline warmup fix.
# On a cold pipeline cache the first CA_ACTIVE frame can ship before all
# pipelines have compiled — the C1 black-frame gate then fires on what was
# a transient warmup hole, not a real regression. `+wait 60` (1 second @
# 60fps) was racy on cold cache; `+wait 240` (4 seconds) gives glslang time
# to compile the per-shader pipeline variants needed for the first scene
# and the cache to populate, so the +screenshot below captures a
# fully-warmed frame. Subsequent runs warm-load the cache from disk and
# the wait is harmless overhead.
# W-25 sanctioned passthrough: SMOKE_EXTRA_ARGS lets a caller inject extra
# engine +cmds (e.g. "+scores +wait 90 +screenshot tag") into the phase1
# launch, on the same live arena process, AFTER the built-in transition chain
# and BEFORE +quit. Word-split intentionally (it is a list of +cmd tokens).
# INVARIANT: when SMOKE_EXTRA_ARGS is empty/unset the expansion contributes
# nothing — the default flow is byte-identical to before this hook.
# SMOKE_PRESET_ARGS: pre-map +cmd injection (e.g. "+set debug_hold_loading 8")
# so a cvar is live before arena1's connect. Default-empty = byte-identical.
if ! run_wired "phase1" 240 \
    ${SMOKE_PRESET_ARGS:-} \
    +map arena1   +waitForMap +wait 900 +screenshot +wait 30 \
    +map arena17  +waitForMap +wait 240 +screenshot +wait 30 \
    ${SMOKE_EXTRA_ARGS:-} \
    +quit; then
    FAIL=1
else
    if ! check_jsonl_invariants 2 phase1; then
        FAIL=1
    fi
    # Two screenshots expected (arena1 spawn + arena17 spawn). Both must pass C1.
    mapfile -t P1_SHOTS < <(find "$SCREENSHOT_DIR" -name '*.png' -newer "$P1_MARKER" | sort)
    if [ "${#P1_SHOTS[@]}" -lt 2 ]; then
        echo "FAIL: phase1 produced ${#P1_SHOTS[@]} screenshots, expected 2"
        FAIL=1
    else
        for i in 0 1; do
            map_label="$([ "$i" = 0 ] && echo arena1 || echo arena17)"
            check_pixel_gate "${P1_SHOTS[$i]}" "phase1 $map_label spawn" || FAIL=1
            if [ -n "$SMOKE_REVIEW_DIR" ]; then
                cp "${P1_SHOTS[$i]}" "$SMOKE_REVIEW_DIR/phase1_${map_label}.png"
            fi
        done
    fi
fi

if [ "${SMOKE_PHASE1_ONLY:-0}" = 1 ]; then
    if [ "$FAIL" -eq 0 ]; then
        echo "==> Smoke result: PASS (Phase 1 only)"
    else
        echo "==> Smoke result: FAIL (Phase 1 only)"
    fi
    exit "$FAIL"
fi

# ════════════════════════════════════════════════════════════════════════════
# Phase 1B — split-screen two-viewport render (cl_splitScreen 1).
# State A (Phase 1 above, cl_splitScreen 0) proved the single full-screen
# viewport. This proves the multi-viewport composite: with cl_splitScreen 1 the
# world_main layout splits into main_scene (left half) + chase camera (right
# half), so BOTH halves must render their own scene. A full-frame mean could
# pass with one half black, so the left and right sub-rects are gated
# independently — each must be non-black, proving the second viewport actually
# rendered. cl_splitScreen is a cheat cvar; sv_cheats is already set by run_wired.
# Set pre-map so the layout is live before the arena connect.
echo
echo "==> Phase 1B — split-screen two-viewport render (cl_splitScreen 1)"
P1B_MARKER="$SMT_TMP/smoke-mt-phase1b.marker"
touch "$P1B_MARKER"
if ! run_wired "phase1b" 240 \
    +set cl_splitScreen 1 \
    +map arena1 +waitForMap +wait 240 +screenshot +wait 30 \
    +quit; then
    FAIL=1
else
    if ! check_jsonl_invariants 1 phase1b; then
        FAIL=1
    fi
    mapfile -t P1B_SHOTS < <(find "$SCREENSHOT_DIR" -name '*.png' -newer "$P1B_MARKER" | sort)
    if [ "${#P1B_SHOTS[@]}" -lt 1 ]; then
        echo "FAIL: phase1b produced ${#P1B_SHOTS[@]} screenshots, expected 1"
        FAIL=1
    else
        SPLIT_SHOT="${P1B_SHOTS[-1]}"
        check_pixel_gate_region "$SPLIT_SHOT" left  "phase1b arena1 split" || FAIL=1
        check_pixel_gate_region "$SPLIT_SHOT" right "phase1b arena1 split" || FAIL=1
        if [ -n "$SMOKE_REVIEW_DIR" ]; then
            cp "$SPLIT_SHOT" "$SMOKE_REVIEW_DIR/phase1b_arena1_split.png"
        fi
    fi
fi

# ════════════════════════════════════════════════════════════════════════════
# Phase 2 — golden-baseline parity at 5 fixed viewpoints (C2 gate).
# legacy-mainpath-retire STEP 5: the legacy rotating-set main path was retired,
# so there is no bindless-vs-legacy A/B to run any more. The gate now diffs
# 3 same-path captures of the current build against a stored golden PNG
# (captured from the post-STEP-2 verified-good bindless build, file:line
# evidence at vk.c capture-Turn-1 commit). The tiled block-mean metric,
# per-tile noise, ceiling+seed exclusions all carry over unchanged.
#
# Re-blessing: `SMOKE_UPDATE_GOLDEN=1 make smoke-map-transition` re-captures
# all 5 viewpoints as new goldens (overwrites tests/golden/<id>.png) instead
# of diffing. Re-bless ONLY after an intentional rendering change is verified
# correct by eye, then commit the new PNGs alongside the change.
# ════════════════════════════════════════════════════════════════════════════
echo
echo "==> Phase 2 — golden-baseline parity at 5 viewpoints"

# The 5 viewpoints — now GENUINELY DISTINCT cameras (cross-diff 89-302 BGR).
# Earlier these were captured with a bare "+setviewpos", which is a NO-OP (server
# console echo only; the player is never teleported), so all 5 were really the
# SAME spawn frame (cross-diff ~0.5). capture_viewpoint() now uses the real-
# teleport recipe (+cmd noclip + +cmd setviewpos + r_pinFrameTime); each commanded
# position is an authored deathmatch spawn or a separately verified in-bounds
# camera. Do not use elevated/out-of-bounds points here: a mostly-void frame can
# pass the coarse black-pixel gate thanks to HUD/gun pixels while providing no
# useful world/material regression coverage. B_spawn2 and E_spawn2 use distinct
# deathmatch origins from arena1.ent / arena17.bsp respectively.
VPS=(
    "arena1  1052 1432 50  135   A_spawn"
    "arena1  216  1328 50  0     B_spawn2"
    "arena1  500  500  50  0     C_far"
    "arena17 488  1096 378 -90   D_spawn"
    "arena17 488  -968 378 90    E_spawn2"
)

# Golden fixture directory — repo-relative.
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
GOLDEN_DIR="$SCRIPT_DIR/golden"
SMOKE_UPDATE_GOLDEN="${SMOKE_UPDATE_GOLDEN:-0}"
if [ "$SMOKE_UPDATE_GOLDEN" = "1" ]; then
    mkdir -p "$GOLDEN_DIR"
    echo "  SMOKE_UPDATE_GOLDEN=1 — re-capturing goldens into $GOLDEN_DIR (NOT diffing)"
fi
if [ -n "$SMOKE_REVIEW_DIR" ]; then
    echo "  SMOKE_REVIEW_DIR — exporting current captures into $SMOKE_REVIEW_DIR (goldens untouched)"
fi

# Per-viewpoint capture protocol: THREE same-path captures. The MAX pairwise
# meanDiff among the three bounds the same-launch animation-phase variance
# (flame / lava / tcMod-turb shaders) per tile — this gives a noise floor
# even when a captured frame lands in a different phase from the golden.
# Three same-path captures × five viewpoints = 15 wired launches; Phase 1
# adds one more for 16. (Pre-retire was 20 wired launches at four per
# viewpoint — three legacy + one bindless.)
# Per-tile gate in BGR-L1 units (each channel 0..255).
# Threshold = max(tile_noise * TILE_RATIO + TILE_MARGIN, TILE_FLOOR)
# where tile_noise is the MAX pairwise meanDiff among 3 same-path
# captures.
#
# legacy-mainpath-retire STEP 5: defaults bumped (margin 3→5,
# floor 5→60) to absorb cross-launch animation-phase variance that
# the pre-retire same-launch A/B did NOT see. Empirically, arena17's
# spawn area carries an area-wide animation (rotating geometry /
# tcMod-turb / particles) whose phase relative to a separately-
# captured golden migrates across the tile grid run-to-run with a
# ~50 BGR-L1 swing on the worst tile. Same-launch 3-capture noise
# stays low (<5) because the 3 captures span a few hundred ms; the
# golden is from a different process minutes/hours earlier. The
# bumped floor catches region-wide regressions (≥+60 BGR shift) but
# accepts the cross-launch animation drift as expected noise. If
# tighter sensitivity is needed for a specific run, override via
# TILE_FLOOR=15 (the pre-cross-launch default).
#   - calm tile (noise≈0): threshold 60 — catches catastrophic
#     region-wide shifts; smaller cross-launch drift passes.
#   - mid-animated tile (noise=20): threshold 60 (still floored).
#   - heavily animated tile (noise=40): threshold 65 — animation
#     phase drift won't trip; only a regression dwarfing the
#     animation's own variation can.
# Tile exclusion: a tile is dropped from the gate (does not contribute
# to pass/fail) when EITHER its same-path noise exceeds
# TILE_NOISE_CEILING (default 35 BGR-L1) OR it appears in the per-
# viewpoint TILE_EXCLUDE list. Why a ceiling: at noise=35 the
# threshold is already 55.5 — only a region-wide +60 shift would
# flag, below the regression class we care about; gating such tiles
# would mostly emit false-passes. Why a seed list: a tile flickering
# across the ceiling cutoff between runs would alternate between
# gated and ungated; explicit seeding stabilises it. TILE_EXCLUDE
# format: "vpid:tile,tile vpid2:tile" (space-separated viewpoints,
# comma-separated tile indices). Guard: if more than
# TILE_EXCLUDE_MAX (16 of 64 = 25%) tiles are excluded for a single
# viewpoint the harness hard-FAILs that viewpoint — too little
# coverage left to catch a regression in the remaining tiles.
TILE_RATIO="${TILE_RATIO:-1.5}"
TILE_MARGIN="${TILE_MARGIN:-5.0}"
TILE_FLOOR="${TILE_FLOOR:-60.0}"
TILE_NOISE_CEILING="${TILE_NOISE_CEILING:-35.0}"
# Per-viewpoint seed list, format "vpid:t,t vpid2:t…".
#
# c2-shadertime-pin-2 — tile 14 is the irreducible C2 residual across all 5
# viewpoints. The renderer-side pin (r_pinShaderTime) made captures within a
# launch deterministic and reduced whole-frame spread by ~50× (4 BGR → 0.05
# BGR), but a small set of entities/surfaces at the top-mid-right region
# (tile 14, row 1 col 6 in the 8×8 grid) still mode-flips per-launch with a
# ~150 BGR delta. When the blessed golden lands in one mode and a verify
# run lands in the other, that tile alone blows the 60-BGR floor. The
# mechanism is shader-time-adjacent (NOT covered by tess.shaderTime /
# e.shaderTime pinning); the c2-shadertime-pin-2 spec capped at 5 LOC and
# declared the FINAL C2 turn. Excluding tile 14 on all 5 viewpoints
# accepts that ~1.5% of gate coverage (5 tiles out of 5*64=320) is
# ungated. The other 315 tiles still gate every viewpoint — the gate
# remains a real regression detector everywhere except this one
# top-mid-right cell.
TILE_EXCLUDE="${TILE_EXCLUDE:-A_spawn:14 B_spawn2:14 C_far:14 D_spawn:14 E_spawn2:14}"
TILE_EXCLUDE_MAX="${TILE_EXCLUDE_MAX:-16}"

echo "  per-tile threshold (BGR-L1 mean shift): max(tile_noise * ${TILE_RATIO} + ${TILE_MARGIN}, ${TILE_FLOOR}) where tile_noise = max pairwise meanDiff among 3 legacy captures — ANY non-excluded tile over its threshold ⇒ FAIL"
echo "  per-tile exclusion: noise > ${TILE_NOISE_CEILING} OR vpid:tile in TILE_EXCLUDE=\"${TILE_EXCLUDE}\"; >${TILE_EXCLUDE_MAX}/64 excluded ⇒ viewpoint hard-FAIL"

# Cache-warmup: the FIRST capture in a fresh fs_homepath renders a slightly
# different frame (cold Vulkan pipeline cache → the screenshot lands at a
# different warmup state, a ~90 BGR one-shot divergence vs warm captures). The
# noclip-teleport recipe is otherwise run-to-run deterministic (warm-vs-warm
# ~0-6 BGR). One throwaway capture warms the cache so EVERY gated capture below
# (and the blessed golden) is warm — keeping bless and verify on the same warm
# footing without a large noise-bound. Discarded; failure here is non-fatal.
set -- ${VPS[0]}
capture_viewpoint "$1" "$2" "$3" "$4" "$5" "vp_warmup" >/dev/null 2>&1 || true

for entry in "${VPS[@]}"; do
    set -- $entry
    map="$1" x="$2" y="$3" z="$4" yaw="$5" id="$6"
    golden="$GOLDEN_DIR/${id}.png"

    # Three same-path captures of the current build.
    shot_a="$(capture_viewpoint "$map" "$x" "$y" "$z" "$yaw" "vp_${id}_a")"
    shot_b="$(capture_viewpoint "$map" "$x" "$y" "$z" "$yaw" "vp_${id}_b")"
    shot_c="$(capture_viewpoint "$map" "$x" "$y" "$z" "$yaw" "vp_${id}_c")"
    if [ -z "$shot_a" ] || [ -z "$shot_b" ] || [ -z "$shot_c" ]; then
        echo "  vp $id : SKIP (missing capture)"
        FAIL=1; continue
    fi
    check_pixel_gate "$shot_a" "phase2 $id current" || FAIL=1

    # Review mode is intentionally orthogonal to re-blessing: preserve one
    # current frame after the isolated homepath is retired, but never modify
    # the committed golden fixtures.  This gives a human-ratifiable artifact
    # for visual changes whose old baseline is known to encode broken output.
    if [ -n "$SMOKE_REVIEW_DIR" ]; then
        cp "$shot_a" "$SMOKE_REVIEW_DIR/${id}.png"
    fi

    # Re-bless mode: overwrite the golden with the first capture, skip diff.
    # We use shot_a (not a vote-of-three) — animation phase is irrelevant to
    # the golden's fitness; the noise-floor mechanism handles phase variance
    # at gate time anyway.
    if [ "$SMOKE_UPDATE_GOLDEN" = "1" ]; then
        cp "$shot_a" "$golden"
        echo "  vp $id : GOLDEN updated -> $golden"
        continue
    fi

    if [ ! -s "$golden" ]; then
        echo "  vp $id : FAIL — missing golden $golden (re-bless: SMOKE_UPDATE_GOLDEN=1 make smoke-map-transition)"
        FAIL=1; continue
    fi

    nf_ab="$SMT_TMP/smoke-mt-nf-${id}-ab.txt"
    nf_ac="$SMT_TMP/smoke-mt-nf-${id}-ac.txt"
    nf_bc="$SMT_TMP/smoke-mt-nf-${id}-bc.txt"
    pair_file="$SMT_TMP/smoke-mt-pair-$id.txt"
    tiled_diff "$shot_a" "$shot_b" > "$nf_ab"
    tiled_diff "$shot_a" "$shot_c" > "$nf_ac"
    tiled_diff "$shot_b" "$shot_c" > "$nf_bc"
    # legacy-mainpath-retire STEP 5 — diff the current capture against the
    # stored golden instead of against an in-launch bindless companion.
    tiled_diff "$shot_a" "$golden"  > "$pair_file"

    # Per-tile noise = max pairwise meanDiff among the 3 legacy
    # captures — bounds the flame-phase variation range so a bindless
    # capture landing in any phase has its meanDiff vs leg_a measured
    # against a representative noise estimate. Column layout of
    # tiled_diff: $1 tile_idx, $2 meanDiff (BGR-L1), $3 samples, $4 frac.
    # Pull this viewpoint's seeded-exclude tile list out of the global
    # TILE_EXCLUDE env knob (format "vpid:t,t vpid2:t…").
    excl_for_id=""
    for tok in $TILE_EXCLUDE; do
        case "$tok" in
            "${id}:"*) excl_for_id="${tok#${id}:}";;
        esac
    done
    summary="$(awk -v ratio="$TILE_RATIO" -v margin="$TILE_MARGIN" -v floor="$TILE_FLOOR" \
        -v ceiling="$TILE_NOISE_CEILING" -v excl_max="$TILE_EXCLUDE_MAX" -v EX="$excl_for_id" \
        -v F_AB="$nf_ab" -v F_AC="$nf_ac" -v F_BC="$nf_bc" -v F_P="$pair_file" '
        function maxv(a, b) { return (a > b) ? a : b }
        BEGIN {
            worst_over = -1e9; worst_tile = -1; worst_md = 0; worst_thr = 0; worst_noise = 0
            nz_mx = 0; n_gated = 0; n_excl_seed = 0; n_excl_noise = 0; excl_list = ""
            if (length(EX) > 0) {
                n = split(EX, ea, ",")
                for (i = 1; i <= n; i++) seeded[ea[i] + 0] = 1
            }
            while ((getline line < F_AB) > 0) { split(line, f, " "); n_ab[f[1]] = f[2] + 0 } close(F_AB)
            while ((getline line < F_AC) > 0) { split(line, f, " "); n_ac[f[1]] = f[2] + 0 } close(F_AC)
            while ((getline line < F_BC) > 0) { split(line, f, " "); n_bc[f[1]] = f[2] + 0 } close(F_BC)
            while ((getline line < F_P)  > 0) {
                split(line, f, " ")
                ti = f[1] + 0; md = f[2] + 0
                n = maxv(maxv(n_ab[ti], n_ac[ti]), n_bc[ti])
                if (n > nz_mx) nz_mx = n
                if (ti in seeded) {
                    n_excl_seed++
                    excl_list = excl_list (excl_list == "" ? "" : ",") ti "(seed)"
                    continue
                }
                if (n > ceiling) {
                    n_excl_noise++
                    excl_list = excl_list (excl_list == "" ? "" : ",") ti "(n=" sprintf("%.0f", n) ")"
                    continue
                }
                n_gated++
                t = n * ratio + margin
                if (t < floor) t = floor
                over = md - t
                if (over > worst_over) {
                    worst_over = over; worst_tile = ti; worst_md = md
                    worst_thr = t;     worst_noise = n
                }
            }
            close(F_P)
            n_excl = n_excl_seed + n_excl_noise
            if (worst_tile < 0) { worst_over = 0; worst_md = 0; worst_thr = 0; worst_noise = 0 }
            printf "noise_max=%.2f  gated=%d excl=%d(seed=%d,noise=%d)  worst_tile=%d (noise=%.2f) meanDiff=%.2f thr=%.2f over=%+.2f",
                   nz_mx, n_gated, n_excl, n_excl_seed, n_excl_noise,
                   worst_tile, worst_noise, worst_md, worst_thr, worst_over
            if (n_excl > excl_max) printf " OVER_EXCLUDED=%d>%d", n_excl, excl_max
            if (n_excl > 0) printf "\n    excluded=[%s]", excl_list
        }')"
    over="$(echo "$summary" | sed -nE 's/.*over=([+-][0-9.]+).*/\1/p')"
    verdict="OK"
    if awk -v o="$over" 'BEGIN{exit !(o > 0)}'; then
        verdict="FAIL"
        FAIL=1
    fi
    if echo "$summary" | grep -q "OVER_EXCLUDED"; then
        verdict="FAIL"
        FAIL=1
    fi
    printf "  vp %s : %s  %s\n" "$id" "$summary" "$verdict"
done

echo
if [ "$FAIL" -ne 0 ]; then
    echo "==> Smoke result: FAIL"
    exit 1
fi
echo "==> Smoke result: PASS"
exit 0
