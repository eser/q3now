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
#                   ~/wired/<product><channel>/       (q_shared.h:19 — same
#                   shape on every platform; only $HOME differs)
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

WIRED_HOME="${WIRED_HOME:-$HOME/wired/$WIRED_APP}"
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
