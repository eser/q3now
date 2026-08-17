#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#
# smoke-launcher.sh — launcher PROCESS smoke (no GUI, no network, no assets).
#
# The launcher is one binary with two personalities (launcher/main.go:21-42):
# a bare invocation opens the Wails window, a recognized subcommand runs
# headlessly and exits. Only the second personality is testable without a
# display, and it is the one that matters for release evidence: the CLI is the
# single canonical asset-bootstrap path (tests/smoke-wasm.sh header), so if
# `assets download` stops dispatching, nothing can be bootstrapped.
#
# WHAT THIS PROVES, AND WHY IT NEEDS NO NETWORK:
#   `assets download` is gated behind EULA acceptance
#   (launcher/internal/subcommands/shared.go:requireEulaAccepted). With no
#   acceptance recorded and no --accept-eula, the command MUST refuse before
#   any HTTP work — the refusal is the observable, and it is also a licence
#   contract: the id-quakepack bundle must never be fetched without recorded
#   acceptance. So the smoke asserts the fail-CLOSED half, which is fully
#   offline and fully deterministic.
#
# 🔴 HOME REDIRECTION IS MANDATORY, NOT COSMETIC. config.ResolvePaths() derives
# settings.json from the user's home. Running this against the real home would
# read Eser's genuine EULA acceptance — the gate would pass, the command would
# proceed to DOWNLOAD, and the smoke would both hang on network and mutate the
# user's install. Every invocation below redirects HOME (Unix) and USERPROFILE
# (Windows) into a scratch dir, so the "not accepted yet" state is guaranteed.
#
# Usage:
#   tests/smoke-launcher.sh [path-to-q3now-launcher]
#
# Exit codes:
#   0  PASS
#   1  FAIL — a contract broke
#  77  SKIP — launcher binary not built/installed

set -uo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=tests/lib/wired_paths.sh
. "$SCRIPT_DIR/lib/wired_paths.sh"

# Resolve the launcher: explicit arg, then the self-contained wails build dir,
# then the install root. Never hardcode — see GAME-DATA.md §4.
LAUNCHER="${1:-}"
if [ -z "$LAUNCHER" ]; then
    for c in \
        "$WIRED_SOURCE/launcher/build/bin/q3now.app/Contents/MacOS/q3now-launcher" \
        "$WIRED_SOURCE/launcher/build/bin/q3now-launcher.exe" \
        "$WIRED_SOURCE/launcher/build/bin/q3now-launcher" \
        "$WIRED_BINDIR/q3now-launcher.exe" \
        "$WIRED_BINDIR/q3now-launcher"; do
        [ -x "$c" ] && { LAUNCHER="$c"; break; }
    done
fi
[ -n "$LAUNCHER" ] && [ -x "$LAUNCHER" ] || {
    echo "SKIP: q3now-launcher not found (build it: make create-launcher)"
    exit 77
}

echo "==> launcher process smoke"
echo "    binary: $LAUNCHER"

SCRATCH="$(mktemp -d -t q3now-launcher-smoke-XXXXXX 2>/dev/null || mktemp -d)"
trap 'rm -rf "$SCRATCH"' EXIT INT TERM

fails=0
OUT="$SCRATCH/out"

# run <expected-rc> <label> <args...>
run() {
    local want="$1" label="$2"; shift 2
    local rc=0
    HOME="$SCRATCH/home" USERPROFILE="$SCRATCH/home" \
        "$LAUNCHER" "$@" >"$OUT" 2>&1 || rc=$?
    if [ "$rc" -ne "$want" ]; then
        echo "  FAIL $label — exit $rc, want $want"
        sed 's/^/        /' "$OUT" | head -10
        fails=$((fails + 1))
        return 1
    fi
    printf '  ok   %-34s exit %d\n' "$label" "$rc"
    return 0
}

mkdir -p "$SCRATCH/home"

# 1. The binary dispatches at all, and reports a version. A launcher that
#    cannot even print --version is not going to bootstrap anything.
if run 0 "--version dispatches" --version; then
    if ! grep -qi "q3now-launcher version" "$OUT"; then
        echo "  FAIL --version printed no version banner"
        sed 's/^/        /' "$OUT" | head -5
        fails=$((fails + 1))
    fi
fi

# 2. The CLI tree still exposes the asset subcommands. `assets download` is
#    the documented bootstrap entry point; if help stops listing it, the
#    subcommand tree was restructured out from under every asset flow.
if run 0 "--help lists subcommands" --help; then
    for want in assets; do
        grep -qE "^[[:space:]]+$want[[:space:]]" "$OUT" || {
            echo "  FAIL --help does not list the '$want' command"
            fails=$((fails + 1))
        }
    done
fi
run 0 "assets --help dispatches" assets --help && {
    for want in download import; do
        grep -qE "^[[:space:]]+$want[[:space:]]" "$OUT" || {
            echo "  FAIL 'assets --help' does not list '$want'"
            fails=$((fails + 1))
        }
    done
}

# 3. 🔴 The licence gate. With a virgin home and no --accept-eula,
#    `assets download` must refuse with a NON-ZERO exit, and must not have
#    started fetching. A zero exit here would mean the bundle can be pulled
#    without recorded acceptance.
if run 1 "assets download refuses w/o EULA" assets download; then
    grep -qi "EULA has not been accepted" "$OUT" || {
        echo "  FAIL refusal did not name the EULA gate"
        sed 's/^/        /' "$OUT" | head -6
        fails=$((fails + 1))
    }
fi
# Nothing may have been written under the redirected home: no settings.json
# recording an acceptance nobody gave, and no partial download cache.
if [ -n "$(find "$SCRATCH/home" -type f 2>/dev/null)" ]; then
    echo "  FAIL refused run still wrote files under the scratch home:"
    find "$SCRATCH/home" -type f 2>/dev/null | sed 's/^/        /' | head -10
    fails=$((fails + 1))
else
    printf '  ok   %-34s no files written\n' "refusal is side-effect free"
fi

# 4. An unknown subcommand must fail rather than silently opening the GUI.
#    On a headless CI box a GUI fallback would hang until the timeout.
run 1 "unknown subcommand rejected" definitely-not-a-command

if [ "$fails" -eq 0 ]; then
    echo "==> PASS launcher process smoke"
    exit 0
fi
echo "==> FAIL launcher process smoke ($fails contract(s) broken)"
exit 1
