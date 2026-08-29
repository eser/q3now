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
GENERATOR="${IRRADIANCE_FIXTURE_GENERATOR:-$REPO_ROOT/build/render_submission_test}"
WATCHDOG="$SCRIPT_DIR/run-with-timeout.py"
ANALYZER="$SCRIPT_DIR/irradiance-probe-visual-analyze.py"
PNG2RAW="${PNG2RAW:-$REPO_ROOT/tools/png2raw/png2raw}"
OUT="${IRRADIANCE_PROBE_VISUAL_OUT:-$REPO_ROOT/build/irradiance-probe-visual}"

[ -x "$ENGINE" ] || { echo "FAIL: engine not executable: $ENGINE" >&2; exit 1; }
ENGINE="$(cd "$(dirname "$ENGINE")" && pwd)/$(basename "$ENGINE")"
[ -x "$GENERATOR" ] || { echo "FAIL: fixture generator missing: $GENERATOR" >&2; exit 1; }
[ -f "$WATCHDOG" ] || { echo "FAIL: watchdog missing: $WATCHDOG" >&2; exit 1; }
[ -f "$ANALYZER" ] || { echo "FAIL: analyzer missing: $ANALYZER" >&2; exit 1; }
[ -x "$PNG2RAW" ] || { echo "FAIL: png2raw missing: $PNG2RAW (make png2raw)" >&2; exit 1; }

run_root="$(mktemp -d "$WIRED_TMP/q3now-irradiance-probe-XXXXXX")"
if [ "${IRRADIANCE_KEEP_HOME:-0}" = 1 ]; then
  trap 'echo "retained probe capture home: $run_root"' EXIT
else
  trap 'rm -rf "$run_root"' EXIT
fi
WIRED_TMP="$run_root"
home="$(wired_isolated_home probe-overlay)"
mkdir -p "$home/base/maps" "$home/base/screenshots" "$OUT"
"$GENERATOR" --write-irradiance-fixture "$home/base/maps/arena7.wprobe"

cfg="$home/base/irradiance-probe-visual.cfg"
log="$OUT/vulkan.log"
printf '%s\n' \
  'map arena7' \
  'waitForMap' \
  'wait 60' \
  'cmd noclip' \
  'wait 20' \
  'set cg_drawGun 0' \
  'set cg_thirdPerson 0' \
  'cmd setviewpos 1740 -804 50 0' \
  'wait 60' \
  'testmodel models/weapons2/grenadel/grenadel.md3 0' \
  'wait 30' \
  'cmd setviewpos 1716 -804 50 0' \
  'wait 60' \
  'testmodel models/weapons2/grenadel/grenadel.md3 0' \
  'wait 30' \
  'screenshot irradiance_probe_warm png silent' \
  'wait 30' \
  'cmd setviewpos 1764 -804 50 0' \
  'wait 60' \
  'testmodel models/weapons2/grenadel/grenadel.md3 0' \
  'wait 30' \
  'screenshot irradiance_probe_cool png silent' \
  'wait 30' \
  'cmd setviewpos 1740 -804 50 0' \
  'wait 60' \
  'testmodel models/weapons2/grenadel/grenadel.md3 0' \
  'wait 30' \
  'set r_showIrradianceProbes 0' \
  'wait 30' \
  'screenshot irradiance_probe_off png silent' \
  'wait 30' \
  'set r_showIrradianceProbes 1' \
  'wait 60' \
  'screenshot irradiance_probe_on png silent' \
  'wait 30' \
  'quit' >"$cfg"

set +e
python3 "$WATCHDOG" --timeout 120 --kill-after 15 --cwd "$(dirname "$ENGINE")" --stdout "$log" -- \
  "$ENGINE" \
  +set fs_installpath "$Q3DIR" \
  +set fs_homepath "$home" \
  +set cl_renderer vulkan \
  +set net_port 0 \
  +set com_automated 1 \
  +set com_noHardReboot 1 \
  +set s_initsound 0 \
  +set sv_pure 0 \
  +set sv_cheats 1 \
  +set vm_game 0 \
  +set vm_cgame 0 \
  +set r_fullscreen 0 \
  +set r_mode -1 \
  +set r_customwidth 1280 \
  +set r_customheight 720 \
  +set r_renderScale 1 \
  +set r_dynamiclight 0 \
  +set r_pinShaderTime 1.0 \
  +set r_pinFrameTime 1.0 \
  +exec irradiance-probe-visual.cfg
rc=$?
set -e
if [ "$rc" -ne 0 ]; then
  echo "FAIL: probe capture engine exited with status $rc" >&2
  tail -n 100 "$log" >&2
  exit 1
fi

on="$home/base/screenshots/irradiance_probe_on.png"
off="$home/base/screenshots/irradiance_probe_off.png"
warm="$home/base/screenshots/irradiance_probe_warm.png"
cool="$home/base/screenshots/irradiance_probe_cool.png"
[ -s "$on" ] || { echo "FAIL: probe-on screenshot missing" >&2; tail -n 80 "$log"; exit 1; }
[ -s "$off" ] || { echo "FAIL: probe-off screenshot missing" >&2; tail -n 80 "$log"; exit 1; }
[ -s "$warm" ] || { echo "FAIL: warm crossing screenshot missing" >&2; tail -n 80 "$log"; exit 1; }
[ -s "$cool" ] || { echo "FAIL: cool crossing screenshot missing" >&2; tail -n 80 "$log"; exit 1; }
cp "$off" "$OUT/off.png"
cp "$on" "$OUT/on.png"
cp "$warm" "$OUT/warm.png"
cp "$cool" "$OUT/cool.png"
[ "$(wc -c <"$OUT/off.png")" -gt 1024 ] && [ "$(wc -c <"$OUT/on.png")" -gt 1024 ] || {
  echo "FAIL: implausibly small probe capture" >&2; exit 1;
}
cmp -s "$OUT/off.png" "$OUT/on.png" && {
  echo "FAIL: probe debug view produced no rendered difference" >&2; exit 1;
}
cmp -s "$OUT/warm.png" "$OUT/cool.png" && {
  echo "FAIL: moving entity crossing produced no rendered difference" >&2; exit 1;
}
python3 "$ANALYZER" --artifacts "$OUT" --png2raw "$PNG2RAW"
echo "PASS: 1280x720 irradiance probe overlay rendered; artifacts: $OUT"
