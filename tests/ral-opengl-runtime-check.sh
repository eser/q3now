#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=tests/lib/wired_paths.sh
. "$SCRIPT_DIR/lib/wired_paths.sh"

ENGINE="${1:-${WIRED_BINARY:-}}"
Q3DIR="${Q3DIR:-$WIRED_INSTALL}"
Q3DATADIR="${Q3DATADIR:-$Q3DIR/base}"
MAPS=( arena1 arena17 )

[ "$(uname -s)" != Darwin ] || {
  echo "SKIP: macOS system OpenGL is 4.1; canonical adapter requires exact 4.6 Core"
  exit 77
}
[ -x "$ENGINE" ] || { echo "FAIL: engine not executable: $ENGINE"; exit 1; }
renderer_dir="$(dirname "$ENGINE")"
OPENGL_RENDERER="$(find "$renderer_dir" -maxdepth 1 -type f \
  \( -name 'wired_opengl_*.so' -o -name 'wired_opengl_*.dll' \) \
  ! -name '*legacy*' | head -n 1)"
[ -n "$OPENGL_RENDERER" ] || {
  echo "FAIL: canonical wired_opengl_<arch> module missing beside $ENGINE"
  exit 1
}

have_content=false
for root in "$Q3DATADIR" "$WIRED_BASE"; do
  if compgen -G "$root/pa[xk]*.sw3z" >/dev/null 2>&1 \
      || compgen -G "$root/pak*.pk3" >/dev/null 2>&1; then
    have_content=true
    break
  fi
done
[ "$have_content" = true ] || {
  echo "SKIP: arena content unavailable under $Q3DATADIR or $WIRED_BASE"
  exit 77
}

tmp_dir="$(mktemp -d "$WIRED_TMP/q3now-opengl-runtime-XXXXXX")"
trap 'rm -rf "$tmp_dir"' EXIT

for map in "${MAPS[@]}"; do
  log="$tmp_dir/$map.log"
  home="$tmp_dir/home-$map"
  shot="$home/base/screenshots/ral-opengl-$map.png"
  mkdir -p "$home/base/screenshots"
  echo "==> native OpenGL 4.6 RAL product smoke: $map (1280x720)"
  set +e
  timeout 45 "$ENGINE" \
    +set fs_installpath "$Q3DIR" \
    +set fs_homepath "$home" \
    +set cl_renderer opengl \
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
    +screenshot "ral-opengl-$map" \
    +wait 10 \
    +quit >"$log" 2>&1
  rc=$?
  set -e
  [ "$rc" -eq 0 ] || { echo "FAIL: $map exited $rc"; tail -n 100 "$log"; exit 1; }
  if grep -Eq "advancing cl_renderer|renderer '.*' failed to initialize|Failed to load renderer|Initializing Vulkan|MoltenVK|Sys_Error|FATAL|^ERROR:|VM_Create.*failed|VM syscall error|died on signal" "$log"; then
    echo "FAIL: $map reported renderer fallback or fatal runtime error"
    grep -E "advancing cl_renderer|failed to initialize|Failed to load renderer|Initializing Vulkan|MoltenVK|Sys_Error|FATAL|^ERROR:|VM_Create.*failed|VM syscall error|died on signal" "$log" || true
    exit 1
  fi
  grep -q "Wired native OpenGL RAL: registration ready logical=1280x720 version=4.6" "$log" || {
    echo "FAIL: $map lacks exact OpenGL 4.6 1280x720 registration evidence"; tail -n 100 "$log"; exit 1;
  }
  content_line="$(grep -m1 "Wired native OpenGL RAL: content receipt map=maps/$map.bsp" "$log" || true)"
  [ -n "$content_line" ] || { echo "FAIL: $map lacks product content receipt"; tail -n 100 "$log"; exit 1; }
  if ! grep -Eq 'surfaces=[1-9][0-9]* vertices=[1-9][0-9]* indices=[1-9][0-9]* assets=[1-9][0-9]* materials=[1-9][0-9]* resolvedMaterials=[1-9][0-9]* materialBytes=[1-9][0-9]* .*loweredWorld=[1-9][0-9]* worldBatches=[1-9][0-9]* .*nativeDraws=[1-9][0-9]* unresolved=0 fallback=0 fatal=0' <<<"$content_line"; then
    echo "FAIL: $map published incomplete/non-exact OpenGL content receipt"; echo "$content_line"; exit 1
  fi
  grep -q "CA_PRIMED.*maps/$map.bsp" "$log" || { echo "FAIL: $map never reached CA_PRIMED"; tail -n 100 "$log"; exit 1; }
  [ -s "$shot" ] || { echo "FAIL: $map produced no PNG capture at $shot"; tail -n 100 "$log"; exit 1; }
  [ "$(wc -c <"$shot")" -gt 1024 ] || { echo "FAIL: $map PNG capture is implausibly small"; exit 1; }
  grep -q "Screenshot saved as screenshots/ral-opengl-$map.png" "$log" || { echo "FAIL: $map lacks screenshot completion evidence"; tail -n 100 "$log"; exit 1; }
  grep -q "Wired native OpenGL RAL: shutdown complete" "$log" || { echo "FAIL: $map lacks clean shutdown"; tail -n 100 "$log"; exit 1; }
  echo "$content_line"
done

echo "PASS: native OpenGL 4.6 RAL product booted arena1 and arena17 at 1280x720"
