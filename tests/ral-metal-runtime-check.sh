#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=tests/lib/wired_paths.sh
. "$SCRIPT_DIR/lib/wired_paths.sh"

ENGINE="${1:-${WIRED_BINARY:-}}"
Q3DIR="${Q3DIR:-$WIRED_INSTALL}"
Q3DATADIR="${Q3DATADIR:-$Q3DIR/Contents/Resources/base}"
METAL_RENDERER="$(dirname "$ENGINE")/wired_metal_arm64.dylib"
MAPS=( arena1 arena17 )

[ "$(uname -s)" = Darwin ] || {
  echo "SKIP: native Metal product runtime requires macOS"
  exit 77
}
[ -x "$ENGINE" ] || { echo "FAIL: engine not executable: $ENGINE"; exit 1; }
[ -f "$METAL_RENDERER" ] || {
  echo "FAIL: packaged native Metal renderer missing: $METAL_RENDERER"
  exit 1
}

if otool -L "$METAL_RENDERER" | grep -Eq 'MoltenVK|Vulkan'; then
  echo "FAIL: native Metal renderer links Vulkan or MoltenVK"
  otool -L "$METAL_RENDERER"
  exit 1
fi

have_content=false
for root in "$Q3DATADIR" "$WIRED_BASE"; do
  if compgen -G "$root/pa[xk]*.sw3z" >/dev/null 2>&1 \
      || compgen -G "$root/pak*.pk3" >/dev/null 2>&1; then
    have_content=true
    break
  fi
done
if [ "$have_content" != true ]; then
  echo "SKIP: arena content unavailable under $Q3DATADIR or $WIRED_BASE"
  exit 77
fi

tmp_dir="$(mktemp -d "$WIRED_TMP/q3now-metal-runtime-XXXXXX")"
trap 'rm -rf "$tmp_dir"' EXIT

for map in "${MAPS[@]}"; do
  log="$tmp_dir/$map.log"
  echo "==> native Metal product smoke: $map (1280x720)"
  set +e
  timeout 45 "$ENGINE" \
    +set fs_installpath "$Q3DIR" \
    +set cl_renderer metal \
    +set r_fullscreen 0 \
    +set r_mode -1 \
    +set r_customwidth 1280 \
    +set r_customheight 720 \
    +set sv_pure 0 \
    +set vm_game 0 \
    +set vm_cgame 0 \
    +log renderer.init info \
    +map "$map" \
    +wait 120 \
    +quit >"$log" 2>&1
  rc=$?
  set -e

  if [ "$rc" -ne 0 ]; then
    echo "FAIL: $map process exited with status $rc"
    tail -n 80 "$log"
    exit 1
  fi
  if grep -Eq "advancing cl_renderer|renderer '.*' failed to initialize|Failed to load renderer|Initializing Vulkan|MoltenVK|Sys_Error|FATAL|^ERROR:|VM_Create.*failed|VM syscall error|died on signal" "$log"; then
    echo "FAIL: $map reported renderer fallback, non-Metal activation or a fatal runtime error"
    grep -E "advancing cl_renderer|failed to initialize|Failed to load renderer|Initializing Vulkan|MoltenVK|Sys_Error|FATAL|^ERROR:|VM_Create.*failed|VM syscall error|died on signal" "$log" || true
    exit 1
  fi
  grep -q "Wired native Metal RAL: registration ready logical=1280x720" "$log" || {
    echo "FAIL: $map lacks exact native Metal 1280x720 initialization evidence"
    tail -n 80 "$log"
    exit 1
  }
  grep -q "Wired native Metal RAL: registration frame presented" "$log" || {
    echo "FAIL: $map lacks a native Metal presented-frame receipt"
    tail -n 80 "$log"
    exit 1
  }
  content_line="$(grep -m1 "Wired native Metal RAL: content receipt" "$log" || true)"
  if [ -z "$content_line" ]; then
    echo "FAIL: $map lacks a native Metal frontend/content/readback receipt"
    tail -n 80 "$log"
    exit 1
  fi
  if ! grep -Eq 'asset=[0-9a-f]{16} world=[0-9a-f]{16} frame=[0-9a-f]{16} readback=[0-9a-f]{16} bytes=(4|8) readbackXY=[0-9]+,[0-9]+ surfaces=[1-9][0-9]* vertices=[1-9][0-9]* indices=[1-9][0-9]* assets=[1-9][0-9]* materials=[1-9][0-9]* resolvedMaterials=[1-9][0-9]* materialBytes=[1-9][0-9]* models=[0-9]+ modelBytes=[0-9]+ entities=[0-9]+ temporalEntities=[0-9]+ ui=[1-9][0-9]* loweredWorld=[1-9][0-9]* worldBatches=[1-9][0-9]* texturedWorld=[0-9]+ lightmappedWorld=[0-9]+ patchWorld=[0-9]+ maskedWorld=[0-9]+ blendedWorld=[0-9]+ depthWriteWorld=[0-9]+ loweredEntity=[0-9]+ entityBatches=[0-9]+ modelEntities=[0-9]+ primitiveEntities=[0-9]+ unresolvedEntities=[0-9]+ loweredUi=[1-9][0-9]* texturedUi=[1-9][0-9]* msdfUi=[1-9][0-9]*' <<<"$content_line"; then
    echo "FAIL: $map published an incomplete native Metal content receipt"
    echo "$content_line"
    exit 1
  fi
  if ! grep -Eq 'loweredEntity=[1-9][0-9]* .*modelEntities=[1-9][0-9]* .*unresolvedEntities=0' <<<"$content_line"; then
    echo "FAIL: $map lacks resolved native Metal model/entity draw evidence"
    echo "$content_line"
    exit 1
  fi
  echo "$content_line"
  grep -q "Server Initialization" "$log" || {
    echo "FAIL: $map never reached server initialization"
    tail -n 80 "$log"
    exit 1
  }
  grep -qi "$map" "$log" || {
    echo "FAIL: $map lacks positive map evidence"
    tail -n 80 "$log"
    exit 1
  }
  grep -q "CA_PRIMED.*maps/$map.bsp" "$log" || {
    echo "FAIL: $map never reached CA_PRIMED"
    tail -n 80 "$log"
    exit 1
  }
  grep -q "Wired native Metal RAL: shutdown complete" "$log" || {
    echo "FAIL: $map lacks native Metal shutdown evidence"
    tail -n 80 "$log"
    exit 1
  }
done

echo "PASS: native Metal product booted arena1 and arena17 at 1280x720"
