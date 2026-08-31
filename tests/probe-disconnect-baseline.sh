#!/usr/bin/env bash
# probe-disconnect-baseline.sh — server-client-decoupling D-fork BASELINE probe.
#
# Purpose-built minimal server-lifecycle probe (NOT the render-regression smoke
# harness). ONE foreground launch, throwaway isolated home, auto-quit. Measures
# whether a CLIENT-only `disconnect` at CA_ACTIVE on a listen server whose
# fs_game != base triggers the historical global gamedir restore/restart coupling
# and its "Game directory changed" server shutdown. MEASURES ONLY.
#
# Desktop discipline: exactly one window, briefly, then +quit (+ a timeout guard).
# Never touches the user's real homepath/install/paks.
set -uo pipefail
. "$(cd "$(dirname "$0")" && pwd)/lib/wired_paths.sh"

ROOT="$WIRED_SOURCE"
WIRED_DIR="$ROOT/build"
WIRED_NAME="wired.x64.exe"
[ -x "$WIRED_DIR/$WIRED_NAME" ] || { echo "FAIL: $WIRED_DIR/$WIRED_NAME not found"; exit 1; }

# Pak/module source: the user's real homepath base (read-only; we hard-link OUT).
case "$(uname -s)" in
  MINGW*|MSYS*|CYGWIN*) HOME_WIRED="$(cygpath -u "$USERPROFILE" 2>/dev/null || echo "$HOME")/wired";;
  *) HOME_WIRED="$HOME/wired";;
esac
PRODUCT_DIR=""
for d in "$HOME_WIRED"/q3now-preview "$HOME_WIRED"/q3now*; do
  [ -d "$d/base" ] && { PRODUCT_DIR="$d"; break; }
done
[ -n "$PRODUCT_DIR" ] || { echo "FAIL: no PRODUCT_DIR with base/ under $HOME_WIRED"; exit 1; }

# Throwaway isolated home (trap-cleaned). NEVER the user's real home.
HOME_PARENT="$(mktemp -d -t wired-probe-XXXXXX)"
PROBE_HOME="$HOME_PARENT/q3now-preview"
trap 'rm -rf "$HOME_PARENT"' EXIT INT TERM
mkdir -p "$PROBE_HOME/base" "$PROBE_HOME/testmod"
case "$(uname -s)" in
  MINGW*|MSYS*|CYGWIN*) PROBE_HOME_NATIVE="$(cygpath -w "$PROBE_HOME")";;
  *) PROBE_HOME_NATIVE="$PROBE_HOME";;
esac

link_paks_into() {
  local src="$1" dst="$2"
  shopt -s nullglob
  for pak in "$src"/pak*.pk3 "$src"/pa[xk]*.sw3z; do
    [ -f "$pak" ] || continue
    [ -e "$dst/$(basename "$pak")" ] && continue
    ln "$pak" "$dst/" 2>/dev/null || cp "$pak" "$dst/"
  done
  shopt -u nullglob
}

# Content paks into base (BSPs) and into testmod (so maps resolve under the mod gamedir).
link_paks_into "$PRODUCT_DIR/base" "$PROBE_HOME/base"
link_paks_into "$PRODUCT_DIR/base" "$PROBE_HOME/testmod"
# Native game modules into testmod so VM_Create succeeds under fs_game=testmod
# (the probe uses +set vm_game 0 / vm_cgame 0). Use the freshly-built debug modules.
for m in qagamex86_64.dll cgamex86_64.dll; do
  [ -f "$WIRED_DIR/base/$m" ] && cp "$WIRED_DIR/base/$m" "$PROBE_HOME/testmod/$m"
done
echo "  testmod staged : $(ls "$PROBE_HOME/testmod" | tr '\n' ' ')"

JSONL="$PROBE_HOME/qconsole.jsonl"
LOG="${LOG:-$WIRED_TMP/probe-disconnect.log}"
mkdir -p "$(dirname "$LOG")"
rm -f "$LOG" "$JSONL"

echo "  launching ONE foreground window (auto-quits)…"
(
  cd "$WIRED_DIR"
  timeout 120 "./$WIRED_NAME" \
    +set fs_game testmod \
    +set fs_homepath "$PROBE_HOME_NATIVE" \
    +set sv_pure 0 +set sv_cheats 1 +set r_brightness 1 \
    +set vm_game 0 +set vm_cgame 0 \
    +set log_file_mode overwrite_synced \
    +set r_mode -1 +set r_customwidth 1280 +set r_customheight 720 +set r_fullscreen 0 \
    +map arena1 +waitForMap \
    +wait 240 +echo PROBE_AT_ACTIVE +path \
    +echo PROBE_PRE +disconnect \
    +wait 120 +echo PROBE_POST \
    +wait 30 +quit \
    >"$LOG" 2>&1 || true
)
sleep 1

echo
echo "==> RESULT (isolated qconsole.jsonl)"
J="$JSONL"
[ -s "$J" ] || { echo "FAIL: no qconsole.jsonl produced ($J)"; exit 1; }

# 1) fs_game runtime value — search the FS path / serverinfo for testmod evidence.
echo "  --- fs_game evidence ---"
grep -anE 'fs_game|testmod|gameSwitch|game folder|Current search' "$J" | head -12 | sed 's/^/    /'

# 2) CA_ACTIVE reached?
echo "  --- CA_ACTIVE ---"
echo "    FIRST GAMEPLAY FRAME count: $(grep -ac 'FIRST GAMEPLAY FRAME' "$J")"
grep -aoE 'FIRST GAMEPLAY FRAME[^"]*' "$J" | head -1 | sed 's/^/    /'
echo "    VM_Create failures: $(grep -ac 'VM_Create on game failed' "$J")"

# 3) disconnect window + shutdown classification.
echo "  --- disconnect sequence ---"
grep -anE 'PROBE_PRE|PROBE_POST|CL_Disconnect: state|Server Shutdown' "$J" | sed 's/^/    /'
echo
echo "  >>> Game-directory-changed shutdown on client disconnect: $(grep -ac 'Server Shutdown (Game directory changed)' "$J") <<<"
echo "  (the trailing 'Server Shutdown (Server quit)' is the +quit teardown, expected, NOT the disconnect)"
echo
echo "  log: $LOG    (isolated home rm -rf'd on exit)"
