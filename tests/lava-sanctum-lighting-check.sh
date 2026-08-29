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
FIXTURE_WRITER="${FIXTURE_WRITER:-$REPO_ROOT/build/render_submission_test}"
OUT="${LAVA_SANCTUM_OUT:-$REPO_ROOT/build/lava-sanctum-lighting}"

[ -x "$ENGINE" ] || { echo "FAIL: engine not executable: $ENGINE" >&2; exit 1; }
ENGINE="$(cd "$(dirname "$ENGINE")" && pwd)/$(basename "$ENGINE")"
[ -x "$PNG2RAW" ] || { echo "FAIL: png2raw not executable: $PNG2RAW" >&2; exit 1; }
[ -x "$FIXTURE_WRITER" ] || { echo "FAIL: fixture writer not executable: $FIXTURE_WRITER" >&2; exit 1; }

run_root="$(mktemp -d "$WIRED_TMP/q3now-lava-lighting-XXXXXX")"
trap 'rm -rf "$run_root"' EXIT
WIRED_TMP="$run_root"
home="$(wired_isolated_home lava-sanctum-lighting)"
mkdir -p "$home/base/maps" "$home/base/screenshots" "$OUT/raw"
"$FIXTURE_WRITER" --write-irradiance-fixture "$home/base/maps/arena7.wprobe" 512 -640 -384 >/dev/null
cfg="$home/base/lava-sanctum-lighting.cfg"
log="$OUT/vulkan.log"

printf '%s\n' \
  'map arena7' 'waitForMap' 'wait 60' 'cmd noclip' \
  'set cg_drawGun 0' 'set cg_thirdPerson 0' \
  'cmd setviewpos 650 -500 -250 225 10' 'wait 90' \
  'testmodel' 'set r_dynamiclight 0' 'set r_dlightShadowTest 0' \
  'set r_ralEffectsSmoke 0' 'set r_lightingReferenceFixture 0' 'set r_lightmap 1' \
  'set r_pinShaderTime 0.25' 'wait 60' 'screenshot lava_emissive_a png silent' \
  'set r_pinShaderTime 0.75' 'wait 60' 'screenshot lava_emissive_b png silent' \
  'set r_pinShaderTime 1.0' 'wait 60' 'screenshot lava_indirect png silent' \
  'set r_lightmap 0' 'set r_shadows 0' 'wait 60' 'screenshot lava_direct_off png silent' \
  'set r_dynamiclight 2' 'set r_dlightShadowTest 500' 'set r_dlightShadows 0' \
  'wait 90' 'screenshot lava_direct_on png silent' \
  'set r_dlightShadows 1' 'wait 120' 'screenshot lava_shadow_on png silent' \
  'set r_dlightShadows 0' 'wait 90' 'screenshot lava_shadow_off png silent' \
  'set r_dynamiclight 0' 'set r_dlightShadowTest 0' 'set r_drawWorld 1' 'testmodel' \
  'wait 90' 'screenshot lava_probe_off png silent' \
  'testmodel models/weapons2/grenadel/grenadel.md3 0' 'wait 90' \
  'screenshot lava_probe_on png silent' \
  'testmodel' 'wait 90' 'screenshot lava_atmosphere_off png silent' \
  'set r_lightingReferenceFixture 1' 'wait 180' 'screenshot lava_atmosphere_on png silent' \
  'set r_dynamiclight 2' 'set r_dlightShadowTest 500' 'set r_dlightShadows 1' \
  'testmodel models/weapons2/grenadel/grenadel.md3 0' 'wait 180' \
  'screenshot lava_final png silent' 'wait 30' 'quit' >"$cfg"

python3 "$WATCHDOG" --timeout 120 --kill-after 15 --cwd "$(dirname "$ENGINE")" --stdout "$log" -- \
  "$ENGINE" \
  +set fs_installpath "$Q3DIR" +set fs_homepath "$home" +set cl_renderer vulkan \
  +set net_port 0 +set com_automated 1 +set com_noHardReboot 1 +set s_initsound 0 \
  +set sv_pure 0 +set sv_cheats 1 +set vm_game 0 +set vm_cgame 0 \
  +set r_fullscreen 0 +set r_mode -1 +set r_customwidth 1280 +set r_customheight 720 \
  +set r_renderScale 1 +set r_pinFrameTime 1.0 +set r_hdrAutoExposure 0 \
  +set r_bloom 0 +set r_ssao 1 +set r_showAO 0 +set r_smaa 0 +set r_forwardPlus 1 \
  +set con_notifytime 0 +set cg_draw2D 0 +exec lava-sanctum-lighting.cfg

capture() { printf '%s' "$home/base/screenshots/$1.png"; }
raw() {
  local id="$1" source
  source="$(capture "$id")"
  [ -s "$source" ] || { echo "FAIL: capture missing: $source" >&2; tail -n 100 "$log"; exit 1; }
  "$PNG2RAW" "$source" >"$OUT/raw/$id.rgb"
  printf '%s' "$OUT/raw/$id.rgb"
}

python3 "$SCRIPT_DIR/lava-sanctum-lighting-analyze.py" "$OUT" \
  "$(raw lava_emissive_a)" "$(raw lava_emissive_b)" \
  "$(raw lava_direct_off)" "$(raw lava_direct_on)" \
  "$(raw lava_indirect)" "$(raw lava_probe_off)" "$(raw lava_probe_on)" \
  "$(raw lava_shadow_off)" "$(raw lava_shadow_on)" \
  "$(raw lava_atmosphere_off)" "$(raw lava_atmosphere_on)" \
  "$(raw lava_final)"

for ppm in "$OUT"/*.ppm; do
  sips -s format png "$ppm" --out "${ppm%.ppm}.png" >/dev/null
done

echo "PASS: canonical 1280x720 lava-sanctum lighting evidence: $OUT"
