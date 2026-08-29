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
OUT="${LAVA_SANCTUM_PERF_OUT:-$REPO_ROOT/build/lava-sanctum-lighting-performance}"
WATCHDOG="$SCRIPT_DIR/run-with-timeout.py"
FIXTURE_WRITER="$REPO_ROOT/build/render_submission_test"
[ -x "$ENGINE" ] || { echo "FAIL: engine not executable: $ENGINE" >&2; exit 1; }
ENGINE="$(cd "$(dirname "$ENGINE")" && pwd)/$(basename "$ENGINE")"
[ -x "$FIXTURE_WRITER" ] || { echo "FAIL: fixture writer missing" >&2; exit 1; }
grep -q '^CMAKE_BUILD_TYPE:STRING=Release$' "$REPO_ROOT/build/CMakeCache.txt" || {
  echo "FAIL: canonical build/ is not Release" >&2; exit 1;
}

run_root="$(mktemp -d "$WIRED_TMP/q3now-lava-perf-XXXXXX")"
trap 'rm -rf "$run_root"' EXIT
WIRED_TMP="$run_root"
home="$(wired_isolated_home lava-sanctum-perf)"
mkdir -p "$home/base/maps" "$OUT"
"$FIXTURE_WRITER" --write-irradiance-fixture "$home/base/maps/arena7.wprobe" 512 -640 -384 >/dev/null
cfg="$home/base/lava-sanctum-performance.cfg"
printf '%s\n' \
  'map arena7' 'waitForMap' 'wait 90' 'cmd noclip' \
  'cmd setviewpos 650 -500 -250 225 10' 'wait 120' \
  'set r_pinShaderTime 1.0' 'set r_pinFrameTime 1.0' \
  'set r_forwardPlus 1' 'set r_dynamiclight 2' 'set r_dlightShadowTest 500' \
  'set r_dlightShadowTestN 1' 'set r_dlightShadows 1' 'set r_dlightShadowK 1' \
  'set r_lightingReferenceFixture 1' \
  'testmodel models/weapons2/grenadel/grenadel.md3 0' 'wait 240' \
  'set r_gpuSpeeds 0' 'set r_vkDebugTiming 0' 'set com_perfTrace 0' 'wait 2' \
  'echo LAVA_LIGHTING_PERF_BEGIN' 'set r_gpuSpeeds 1' 'set r_vkDebugTiming 1' 'set com_perfTrace 1' \
  'wait 650' 'set r_gpuSpeeds 0' 'set r_vkDebugTiming 0' 'set com_perfTrace 0' \
  'echo LAVA_LIGHTING_PERF_END' 'wait 2' 'quit' >"$cfg"

python3 "$WATCHDOG" --timeout 120 --kill-after 15 --cwd "$(dirname "$ENGINE")" --stdout "$OUT/stdout.log" -- \
  "$ENGINE" +set fs_installpath "$Q3DIR" +set fs_homepath "$home" +set cl_renderer vulkan \
  +set net_port 0 +set com_automated 1 +set com_noHardReboot 1 +set s_initsound 0 \
  +set sv_pure 0 +set sv_cheats 1 +set vm_game 0 +set vm_cgame 0 \
  +set r_fullscreen 0 +set r_mode -1 +set r_customwidth 1280 +set r_customheight 720 \
  +set r_renderScale 1 +set r_swapInterval 0 +set com_maxfps 0 \
  +set r_bloom 0 +set r_ssao 1 +set r_smaa 0 +set r_forwardPlus 1 \
  +set log_severity DEBUG +set log_file_severity DEBUG +log renderer.timing debug \
  +exec lava-sanctum-performance.cfg

[ -s "$home/qconsole.jsonl" ] || { echo "FAIL: qconsole missing" >&2; exit 1; }
cp "$home/qconsole.jsonl" "$OUT/qconsole.jsonl"
python3 "$SCRIPT_DIR/lava-sanctum-lighting-performance-analyze.py" \
  "$OUT/qconsole.jsonl" "$OUT/summary.json"
