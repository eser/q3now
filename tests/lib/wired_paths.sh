#!/usr/bin/env bash
# Single source of truth for the paths test scripts need. Source it:
#
#   . "$(dirname "$0")/../lib/wired_paths.sh"     # adjust depth as needed
#
# Everything here is derived, never hardcoded. Before this existed, ~14 scripts
# each invented their own answer and most baked in one developer's Windows
# layout (/c/Users/<name>/...), which broke every non-Windows run and leaked a
# personal directory structure into a public repo.
#
# The layout itself is fixed and documented, so there is nothing to guess:
#
#   WIRED_SOURCE    this source checkout              (git rev-parse)
#   WIRED_HOME      engine home: config, screenshots, qconsole.jsonl, navmesh
#                   <home-root>/wired/<product><channel>/  (q_shared.h:19 — same
#                   shape on every platform; the ROOT differs, and on Windows it
#                   is USERPROFILE rather than $HOME. See WIRED_HOME_ROOT below.)
#   WIRED_BASE      $WIRED_HOME/base — where the engine actually writes
#   WIRED_INSTALL   install root: engine binary + paks
#                   macOS   /Applications/<app>.app
#                   Windows $LOCALAPPDATA/Programs/<app>
#                   Linux   $HOME/.local/share/<app>
#                   (mirrors Makefile:137-145 Q3DIR)
#   WIRED_BINDIR    binaries      (macOS: Contents/MacOS)   — Makefile:151-157
#   WIRED_GAMEDATA   paks          (macOS: Contents/Resources/base)
#   WIRED_BINARY    GUI engine binary, arch-suffixed
#   WIRED_BINARY_HEADLESS  headless binary
#   WIRED_TMP       system temp root, asked of the system (POSIX), never a literal
#
# Every value honours a pre-set environment variable, so callers can still
# point a run somewhere else without editing scripts.

WIRED_PRODUCT="${WIRED_PRODUCT:-q3now}"
WIRED_CHANNEL="${WIRED_CHANNEL:--preview}"
WIRED_APP="${WIRED_APP:-${WIRED_PRODUCT}${WIRED_CHANNEL}}"

WIRED_SOURCE="${WIRED_SOURCE:-$(git rev-parse --show-toplevel 2>/dev/null)}"

# System temp root — asked of the system, not spelled out. `mktemp -d` is the POSIX
# way to find where temporary files belong: it honours TMPDIR, falls back to the
# platform default, and is correct on MSYS (where /tmp maps into the MSYS root) as
# well as macOS (where TMPDIR is a per-user directory, not /tmp). Scripts that need a
# STABLE location across runs — capture homes, warmup caches — should use
# "$WIRED_TMP/<fixed-name>"; scripts that need a throwaway should keep calling
# mktemp directly. Deriving the root once here is what keeps literals like
# /c/msys64/tmp out of individual scripts; that particular literal silently produced
# an empty, pak-free home on every non-Windows host, so captures came back black and
# read as rendering defects.
if [ -z "${WIRED_TMP:-}" ]; then
    _wired_tmp_probe="$(mktemp -d 2>/dev/null)" || _wired_tmp_probe=""
    if [ -n "$_wired_tmp_probe" ]; then
        WIRED_TMP="$(dirname "$_wired_tmp_probe")"
        rmdir "$_wired_tmp_probe" 2>/dev/null || true
    else
        WIRED_TMP="${TMPDIR:-/tmp}"
    fi
    unset _wired_tmp_probe
fi
WIRED_TMP="${WIRED_TMP%/}"

# The engine's home root, resolved the way the ENGINE resolves it — not the way
# the shell does. On Windows Sys_DefaultHomePath (win32/win_shared.c) reads
# USERPROFILE, and under MSYS that is NOT $HOME: $HOME is the MSYS root
# (/home/<user>) while the engine writes to C:/Users/<user>. Deriving this from
# $HOME sent every Windows harness looking for artefacts in a directory the
# engine never writes to, and the isolated-home helper below then linked no paks
# because it found no base/ to link from — a map that exists reads as "Can't find
# map". POSIX hosts have no such split, so $HOME stays correct there.
if [ -z "${WIRED_HOME_ROOT:-}" ]; then
    case "$(uname -s)" in
        MINGW*|MSYS*|CYGWIN*)
            _up="${USERPROFILE:-}"
            if [ -n "$_up" ] && command -v cygpath >/dev/null 2>&1; then
                WIRED_HOME_ROOT="$(cygpath -u "$_up")"
            else
                WIRED_HOME_ROOT="${_up:-$HOME}"
            fi
            unset _up
            ;;
        *)
            WIRED_HOME_ROOT="$HOME"
            ;;
    esac
fi
WIRED_HOME_ROOT="${WIRED_HOME_ROOT%/}"

WIRED_HOME="${WIRED_HOME:-$WIRED_HOME_ROOT/wired/$WIRED_APP}"
WIRED_BASE="${WIRED_BASE:-$WIRED_HOME/base}"

case "$(uname -s)" in
    Darwin)
        WIRED_INSTALL="${WIRED_INSTALL:-/Applications/${WIRED_APP}.app}"
        WIRED_BINDIR="${WIRED_BINDIR:-$WIRED_INSTALL/Contents/MacOS}"
        WIRED_GAMEDATA="${WIRED_GAMEDATA:-$WIRED_INSTALL/Contents/Resources/base}"
        ;;
    MINGW*|MSYS*|CYGWIN*)
        # $LOCALAPPDATA arrives as a native path under MSYS; cygpath makes it
        # POSIX so it can be concatenated with the rest.
        _lad="${LOCALAPPDATA:-$HOME/AppData/Local}"
        command -v cygpath >/dev/null 2>&1 && _lad="$(cygpath -u "$_lad")"
        WIRED_INSTALL="${WIRED_INSTALL:-$_lad/Programs/$WIRED_APP}"
        WIRED_BINDIR="${WIRED_BINDIR:-$WIRED_INSTALL}"
        WIRED_GAMEDATA="${WIRED_GAMEDATA:-$WIRED_INSTALL/base}"
        unset _lad
        ;;
    *)
        WIRED_INSTALL="${WIRED_INSTALL:-$HOME/.local/share/$WIRED_APP}"
        WIRED_BINDIR="${WIRED_BINDIR:-$WIRED_INSTALL}"
        WIRED_GAMEDATA="${WIRED_GAMEDATA:-$WIRED_INSTALL/base}"
        ;;
esac

# Binaries carry an arch suffix (wired.arm64 / wired.x64[.exe]). Resolve by
# looking, rather than assuming which host this is.
wired_find_bin() {
    # $1 = base name ("wired" | "wired-headless")
    local n="$1" c
    for c in "$WIRED_BINDIR/$n.arm64" "$WIRED_BINDIR/$n.x64.exe" \
             "$WIRED_BINDIR/$n.x64"   "$WIRED_BINDIR/$n.exe" "$WIRED_BINDIR/$n"; do
        [ -x "$c" ] && { printf '%s' "$c"; return 0; }
    done
    return 1
}

WIRED_BINARY="${WIRED_BINARY:-$(wired_find_bin wired || true)}"
WIRED_BINARY_HEADLESS="${WIRED_BINARY_HEADLESS:-$(wired_find_bin wired-headless || true)}"

# Repo-built helper tools (tools/png2raw, tools/visual-diff/vdiff, ...). Same
# problem as the engine binaries: Windows appends .exe, nothing else does, and
# several scripts each hand-rolled the same two-line "try .exe too" dance.
#
#   PNG2RAW="$(wired_find_tool tools/png2raw/png2raw)"
wired_find_tool() {
    # $1 = path relative to the repo root, without extension
    local p="$WIRED_SOURCE/$1"
    for c in "$p" "$p.exe"; do
        [ -x "$c" ] && { printf '%s' "$c"; return 0; }
    done
    return 1
}

# Pick up the screenshot a run just produced.
#
#   marker="$(wired_shot_marker)"
#   ... run the engine ...
#   shot="$(wired_newest_shot "$marker")" || { echo "no screenshot"; exit 1; }
#
# Do NOT do this with `ls -t | grep -v "$BEFORE" | head -1`. That idiom was in
# four capture scripts and it fails OPEN: when `ls -t` errors (a `ls` shim that
# rejects -t, a missing dir) the variable is empty, `grep -v "^$"` then filters
# nothing, and the script silently copies a STALE screenshot instead of
# reporting that none was produced. That is what made every macOS capture look
# like a correctly-sized black frame on 2026-08-16 — the engine was fine; the
# harness was handing back an old file. Parsing `ls` output is fragile anyway
# (spaces in names, locale, shims); a mtime marker is not.
wired_shot_marker() {
    local m; m="$(mktemp "${TMPDIR:-/tmp}/wired_shot_marker.XXXXXX")"
    printf '%s' "$m"
}

wired_newest_shot() {
    # $1 = marker file from wired_shot_marker; $2 = dir (default $WIRED_BASE/screenshots)
    local marker="$1" dir="${2:-$WIRED_BASE/screenshots}" newest=""
    [ -d "$dir" ] || return 1
    # -newer compares mtime against the marker, so only files written after the
    # marker was created qualify. No sorting of filenames involved.
    while IFS= read -r f; do
        [ -z "$f" ] && continue
        if [ -z "$newest" ] || [ "$f" -nt "$newest" ]; then newest="$f"; fi
    done <<EOF
$(find "$dir" -type f -name '*.png' -newer "$marker" 2>/dev/null)
EOF
    rm -f "$marker"
    [ -n "$newest" ] || return 1
    printf '%s' "$newest"
}

# Second-newest screenshot since the marker — for a capture that takes TWO
# shots in one engine session (a HUD-on frame, then a HUD-off reference frame
# of the same pinned viewpoint, so the harness can prove the HUD drew anything
# at all). Returns non-zero when fewer than two shots exist, so a caller that
# expects a pair fails loudly instead of silently comparing a frame with itself.
#
# Ordering is by mtime via `-nt`, never by filename: the screenshot stamp has
# 1-second granularity and two shots taken 200ms apart can collide on name.
# The marker is NOT consumed here — call this BEFORE wired_newest_shot.
wired_prev_shot() {
    # $1 = marker file from wired_shot_marker; $2 = dir (default $WIRED_BASE/screenshots)
    local marker="$1" dir="${2:-$WIRED_BASE/screenshots}" newest="" second=""
    [ -d "$dir" ] || return 1
    while IFS= read -r f; do
        [ -z "$f" ] && continue
        if [ -z "$newest" ] || [ "$f" -nt "$newest" ]; then
            second="$newest"; newest="$f"
        elif [ -z "$second" ] || [ "$f" -nt "$second" ]; then
            second="$f"
        fi
    done <<EOF
$(find "$dir" -type f -name '*.png' -newer "$marker" 2>/dev/null)
EOF
    [ -n "$second" ] || return 1
    printf '%s' "$second"
}

# Tools that live outside the repo. Prefer whatever is on PATH; fall back to
# each platform's usual install spot. Never assume one platform's location.
wired_find_chrome() {
    local c
    for c in "${CHROME:-}" chrome google-chrome chromium; do
        [ -n "$c" ] && command -v "$c" >/dev/null 2>&1 && { command -v "$c"; return 0; }
    done
    for c in "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome" \
             "$HOME/Applications/Google Chrome.app/Contents/MacOS/Google Chrome" \
             "/c/Program Files/Google/Chrome/Application/chrome.exe" \
             "/c/Program Files (x86)/Google/Chrome/Application/chrome.exe"; do
        [ -x "$c" ] && { printf '%s' "$c"; return 0; }
    done
    return 1
}

# ── isolated capture homes ──────────────────────────────────────────────────
# wired_base_has_archives <base-dir> — true when a VFS content directory has
# at least one supported immutable archive, without assigning semantic roles to
# archive filenames.
wired_base_has_archives() {
    local source="$1" f
    [ -d "$source" ] || return 1
    for f in "$source"/*.sw3z "$source"/*.pk3; do
        [ -e "$f" ] && return 0
    done
    return 1
}

# wired_first_archive <base-dir> — print one supported archive path. This is
# only for provenance records that require a concrete file; VFS staging must
# always use wired_link_content_into_home so it sees the complete archive set.
wired_first_archive() {
    local source="$1" f
    for f in "$source"/*.sw3z "$source"/*.pk3; do
        [ -e "$f" ] && { printf '%s' "$f"; return 0; }
    done
    return 1
}

# wired_find_archive_root <root ...> — print the first root whose base/
# contains a supported archive.
wired_find_archive_root() {
    local root
    for root in "$@"; do
        [ -n "$root" ] || continue
        if wired_base_has_archives "$root/base"; then
            (cd "$root" && pwd)
            return 0
        fi
    done
    return 1
}

# wired_link_content_into_home <home> [content-base ...] — symlink immutable
# archives from one or more base/ directories into an existing scratch home.
# When no content-base is supplied, the installed game data and the player's
# licensed/base content are used. Later directories win on duplicate names.
wired_link_content_into_home() {
    local home="$1" source f
    shift
    mkdir -p "$home/base" || return 1

    if [ "$#" -eq 0 ]; then
        set -- "$WIRED_GAMEDATA" "$WIRED_BASE"
    fi
    for source in "$@"; do
        [ -d "$source" ] || continue
        for f in "$source"/*.sw3z "$source"/*.pk3; do
            [ -e "$f" ] || continue
            ln -sfn "$f" "$home/base/$( basename "$f" )" || return 1
        done
    done
}

# wired_isolated_home <name> — print a scratch fs_homepath with the real paks
# symlinked into base/, creating it if needed.
#
# WHY NO CAPTURE SCRIPT MAY USE THE PLAYER'S HOME
# A capture harness does two things that are harmless in a scratch directory
# and destructive in ~/wired/<product>/:
#
#   1. It deletes config.cfg before a launch to force a known starting state.
#      In the player's home that silently erases their settings.
#   2. It passes CVAR_ARCHIVE cvars (cg_draw2D, cg_drawGun, r_*) as +set. A
#      CLEAN EXIT WRITES THOSE BACK to config.cfg — so the harness leaves the
#      game with the HUD and weapon switched off and nothing on screen saying
#      why. This actually happened, and it is the reason this helper exists.
#
# Per-script "restore afterwards" logic does not fix (2): a run that dies
# before its restore still leaves the player's config altered, and every new
# capture script has to remember to write that logic correctly. An isolated
# home makes the hazard structurally unreachable instead of conditionally
# survivable.
#
# The paks are SYMLINKED, not copied: they are ~14 MB each and read-only to
# the engine, so copying would cost real time per run and add a second, stale
# copy of the game data that could silently diverge from the built one.
wired_isolated_home() {
    local name="${1:-capture}" home
    home="$WIRED_TMP/$name"
    wired_link_content_into_home "$home" || return 1
    printf '%s' "$home"
}

# ── reading pixels out of a decoded frame ───────────────────────────────────
# wired_od_bytes — stream a file as decimal byte values, one whitespace-
# separated run per line, portably.
#
# WHY THIS EXISTS
# The obvious spelling, `od -A n -t u1 -v -w16`, is NOT portable: -w is a GNU
# coreutils extension and BSD od (the macOS default) rejects it outright:
#
#     od: illegal option -- w
#
# None of the capture harnesses used `set -o pipefail`, so that failure was
# swallowed inside the pipeline, awk received empty input, and every band mean
# came out as 0. The run then reported a clean, plausible number — "lower-band
# delta = 0.000" — which reads as "no difference detected" rather than "nothing
# was measured". A check that cannot fail certifies everything, and this one
# failed INTO the passing direction, on the platform it was being run on.
#
# BSD od already defaults to 16 bytes per line, which is exactly what -w16 was
# asking for, so dropping the flag is not a compromise: the two agree.
#
# Callers that need a SAMPLING STRIDE (the old -w96 spelling, i.e. "one sample
# every 32 pixels") must not reach for a width — that was a side effect of the
# line grouping, not a documented feature. Use wired_od_sample_rgb below, which
# states the stride in pixels and computes it from its own index.
wired_od_bytes() {
    if ! od -A n -t u1 -v "$@"; then
        echo "wired_od_bytes: od failed — measurement aborted rather than reported as zero" >&2
        return 1
    fi
}

# wired_od_selftest — prove the byte readers work HERE, on this host's od.
#
# The original defect was not that od failed; it was that it failed SILENTLY and
# the reading came back as a clean zero. So a check that only runs the helper is
# not enough — it has to assert a known answer. Callers run this before trusting
# a measurement; it needs no engine, no capture and no golden.
#
# Returns 0 on success, 1 with a diagnostic on failure.
wired_od_selftest() {
    local probe out want
    probe="$(mktemp)" || return 1
    # 4 pixels, values chosen so a truncated or mis-strided read cannot
    # accidentally produce the expected output.
    printf '\1\2\3\10\20\30\100\120\140\200\220\240' > "$probe"

    # Every byte must come through: 12 values (1 2 3 8 16 24 64 80 96 128 144
    # 160) summing to 726.
    out="$( wired_od_bytes < "$probe" | awk '{for(i=1;i<=NF;i++){n++; s+=$i}} END{print n, s}' )"
    if [ "$out" != "12 726" ]; then
        echo "wired_od_selftest: wired_od_bytes returned '$out', expected '12 726'" >&2
        rm -f "$probe"; return 1
    fi

    # Stride 2 over 4 pixels must yield pixels 0 and 2.
    want="1 2 3
64 80 96"
    out="$( wired_od_sample_rgb 2 < "$probe" )"
    if [ "$out" != "$want" ]; then
        echo "wired_od_selftest: wired_od_sample_rgb stride 2 returned '$out'" >&2
        rm -f "$probe"; return 1
    fi

    rm -f "$probe"
    return 0
}

# wired_od_sample_rgb [stride_px] — print "R G B" for every stride_px-th pixel
# of a decoded RGB byte stream read from STDIN.
#
# Replaces `od ... -w$((stride*3)) | awk '{print $1,$2,$3}'`, which produced the
# same result only because od happened to start each line on a pixel boundary.
# Tracking the byte index directly says what is meant and does not depend on how
# od chooses to group its output.
#
# Reads stdin rather than taking a path, because every caller is a pipeline and
# the obvious `... | wired_od_sample_rgb /dev/stdin` does NOT work on macOS: od
# reports "Bad file descriptor" for /dev/stdin when stdin is a pipe. Same class
# of portability trap as the -w flag this helper exists to remove, and it was
# caught the same way — by running it rather than assuming.
wired_od_sample_rgb() {
    local stride="${1:-32}"
    od -A n -t u1 -v | awk -v stride="$stride" '
        BEGIN { idx = 0; step = stride * 3 }
        {
            for (i = 1; i <= NF; i++) {
                off = idx % step
                if (off == 0)      r = $i
                else if (off == 1) g = $i
                else if (off == 2) { print r, g, $i }
                idx++
            }
        }'
}
