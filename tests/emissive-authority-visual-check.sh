#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Wired Engine contributors

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
# shellcheck source=tests/lib/wired_paths.sh
. "$SCRIPT_DIR/lib/wired_paths.sh"

ENGINE="${1:-${WIRED_BINARY:-}}"
Q3DIR="${Q3DIR:-$WIRED_INSTALL}"
WATCHDOG="$SCRIPT_DIR/run-with-timeout.py"
PNG2RAW="${PNG2RAW:-$REPO_ROOT/tools/png2raw/png2raw}"
OUT="${EMISSIVE_AUTHORITY_VISUAL_OUT:-$REPO_ROOT/build/emissive-authority-visual}"

[ -x "$ENGINE" ] || { echo "FAIL: engine not executable: $ENGINE" >&2; exit 1; }
[ -x "$PNG2RAW" ] || { echo "FAIL: png2raw missing: $PNG2RAW" >&2; exit 1; }
ENGINE="$(cd "$(dirname "$ENGINE")" && pwd)/$(basename "$ENGINE")"
run_root="$(mktemp -d "$WIRED_TMP/q3now-emissive-authority-XXXXXX")"
trap 'rm -rf "$run_root"' EXIT
WIRED_TMP="$run_root"
home="$(wired_isolated_home emissive-authority)"
mkdir -p "$home/base/screenshots" "$OUT"
cfg="$home/base/emissive-authority.cfg"
log="$OUT/vulkan.log"

printf '%s\n' \
  'map arena7' 'waitForMap' 'set r_emissiveAuthorityFixture 1' 'wait 60' 'cmd noclip' \
  'set cg_drawGun 0' 'set cg_thirdPerson 0' \
  'cmd setviewpos 650 -500 -250 225 10' 'wait 90' \
  'set r_showEmissiveLights 0' 'wait 45' 'screenshot emissive_authority_off png silent' 'wait 2' \
  'set r_showEmissiveLights 1' 'wait 45' 'screenshot emissive_authority_on png silent' \
  'wait 20' 'quit' >"$cfg"

python3 "$WATCHDOG" --timeout 90 --kill-after 15 --cwd "$(dirname "$ENGINE")" --stdout "$log" -- \
  "$ENGINE" +set fs_installpath "$Q3DIR" +set fs_homepath "$home" \
  +set cl_renderer vulkan +set net_port 0 +set com_automated 1 +set com_noHardReboot 1 \
  +set s_initsound 0 +set sv_pure 0 +set sv_cheats 1 +set vm_game 0 +set vm_cgame 0 \
  +set r_fullscreen 0 +set r_mode -1 +set r_customwidth 1280 +set r_customheight 720 \
  +set r_renderScale 1 +set r_emissiveAuthorityFixture 1 +set r_dynamiclight 2 \
  +set r_pinShaderTime 1.0 +set r_pinFrameTime 1.0 +set r_bloom 0 +set r_smaa 0 \
  +set con_notifytime 0 +set cg_draw2D 0 +log renderer.init info \
  +exec emissive-authority.cfg

off="$home/base/screenshots/emissive_authority_off.png"
on="$home/base/screenshots/emissive_authority_on.png"
[ -s "$off" ] && [ -s "$on" ] || { echo "FAIL: emissive debug captures missing" >&2; tail -n 100 "$log"; exit 1; }
cp "$off" "$OUT/off.png"
cp "$on" "$OUT/on.png"
python3 "$SCRIPT_DIR/emissive-authority-visual-analyze.py" --artifacts "$OUT" --png2raw "$PNG2RAW"
echo "PASS: 1280x720 emissive authority overlay rendered; artifacts: $OUT"
