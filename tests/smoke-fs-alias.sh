#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors

set -euo pipefail

. "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/lib/wired_paths.sh"

DED="${1:-wired-headless}"
SW3Z_TOOL="${SW3Z_TOOL:-$WIRED_SOURCE/tools/sw3z-archiver/cmd/sw3z/sw3z}"
if [ ! -x "$DED" ] || [ ! -x "$SW3Z_TOOL" ]; then
  echo "SKIP: wired-headless or sw3z archiver unavailable"
  exit 77
fi

FIXTURE_ROOT="$(mktemp -d -t q3now-fs-alias)"
trap 'rm -rf "$FIXTURE_ROOT"' EXIT
BASEPATH="$FIXTURE_ROOT/basepath"
HOMEPATH="$FIXTURE_ROOT/homepath"
SEED="$FIXTURE_ROOT/seed"
SCAFFOLD="$FIXTURE_ROOT/scaffold"
PK3_SCOPE_SEED="$FIXTURE_ROOT/pk3-scope-seed"
SW3Z_SCOPE_SEED="$FIXTURE_ROOT/sw3z-scope-seed"
ROLE_SCOPE_SEED="$FIXTURE_ROOT/role-scope-seed"
PRECEDENCE_INSTALL_SEED="$FIXTURE_ROOT/precedence-install-seed"
PRECEDENCE_HOME_SEED="$FIXTURE_ROOT/precedence-home-seed"
PAK0_SEED="$FIXTURE_ROOT/pak0-seed"
PAK8_SEED="$FIXTURE_ROOT/pak8-seed"
SAME_PK3_SEED="$FIXTURE_ROOT/same-pk3-seed"
SAME_SW3Z_SEED="$FIXTURE_ROOT/same-sw3z-seed"
ROOT_INSTALL_SEED="$FIXTURE_ROOT/root-install-seed"
ROOT_HOME_SEED="$FIXTURE_ROOT/root-home-seed"
INSTALL_RESOURCE="$BASEPATH"
case "$(uname -s)" in
  Darwin) INSTALL_RESOURCE="$BASEPATH/Contents/Resources" ;;
esac
mkdir -p "$INSTALL_RESOURCE/base" "$HOMEPATH/base/fixtures" \
  "$HOMEPATH/scope-pk3" "$HOMEPATH/scope-sw3z" \
	"$HOMEPATH/scope-role" \
	"$SEED/fixtures" "$SCAFFOLD" "$PK3_SCOPE_SEED/scope" \
	"$SW3Z_SCOPE_SEED/scope" "$ROLE_SCOPE_SEED/scope" \
	"$INSTALL_RESOURCE/scope-precedence" "$HOMEPATH/scope-precedence" \
	"$PRECEDENCE_INSTALL_SEED/scope" "$PRECEDENCE_HOME_SEED/scope" \
	"$PAK0_SEED/scope" "$PAK8_SEED/scope" \
	"$SAME_PK3_SEED/scope" "$SAME_SW3Z_SEED/scope" \
	"$ROOT_INSTALL_SEED/scope" "$ROOT_HOME_SEED/scope" \
	"$HOMEPATH/scope-precedence/scope"

printf '%s\n' '// VFS alias smoke fixture' > "$SEED/default.cfg"
printf '%s\n' 'pack-origin' > "$SEED/fixtures/canonical.bin"
printf '%s\n' 'return {' '  files = {' \
  '    ["legacy/resource.bin"] = "fixtures/canonical.bin",' \
  '  },' '}' > "$SEED/fs-aliases.lua"
ARCHIVE="$FIXTURE_ROOT/alias_fixture.sw3z"
"$SW3Z_TOOL" a "$ARCHIVE" "$SEED" >/dev/null
printf '%s\n' 'fixture-scaffold' > "$SCAFFOLD/scaffold.txt"
"$SW3Z_TOOL" a "$INSTALL_RESOURCE/base/alias_fixture.sw3z" \
  "$SCAFFOLD" >/dev/null
printf '%s\n' 'scope-pk3-allowed' > "$PK3_SCOPE_SEED/scope/allow.bin"
( cd "$PK3_SCOPE_SEED" && cmake -E tar cf \
  "$HOMEPATH/scope-pk3/scope_policy.pk3" --format=zip scope/allow.bin )
printf '%s\n' 'scope-sw3z-denied' > "$SW3Z_SCOPE_SEED/scope/deny.bin"
"$SW3Z_TOOL" a "$HOMEPATH/scope-sw3z/scope_policy.sw3z" \
  "$SW3Z_SCOPE_SEED" >/dev/null
printf '%s\n' 'scope-role-shadowed' > "$ROLE_SCOPE_SEED/scope/role.bin"
"$SW3Z_TOOL" a "$HOMEPATH/scope-role/role_policy.sw3z" \
  "$ROLE_SCOPE_SEED" >/dev/null
cp "$HOMEPATH/scope-role/role_policy.sw3z" "$HOMEPATH/base/role_policy.sw3z"
printf '%s\n' 'pax21-wins' > "$PRECEDENCE_INSTALL_SEED/scope/precedence.bin"
printf '%s\n' 'archive-wins' > "$PRECEDENCE_INSTALL_SEED/scope/archive-over-loose.bin"
printf '%s\n' 'pax-wins' > "$PRECEDENCE_INSTALL_SEED/scope/pax-over-pak.bin"
printf '%s\n' 'pax01-loses' > "$PRECEDENCE_HOME_SEED/scope/precedence.bin"
"$SW3Z_TOOL" a "$INSTALL_RESOURCE/scope-precedence/pax21.sw3z" \
  "$PRECEDENCE_INSTALL_SEED" >/dev/null
"$SW3Z_TOOL" a "$HOMEPATH/scope-precedence/pax01.sw3z" \
  "$PRECEDENCE_HOME_SEED" >/dev/null
printf '%s\n' 'pak0-loses' > "$PAK0_SEED/scope/q3-pak-order.bin"
( cd "$PAK0_SEED" && cmake -E tar cf \
  "$INSTALL_RESOURCE/scope-precedence/pak0.pk3" --format=zip scope/q3-pak-order.bin )
printf '%s\n' 'pak8-wins' > "$PAK8_SEED/scope/q3-pak-order.bin"
printf '%s\n' 'pak-loses' > "$PAK8_SEED/scope/pax-over-pak.bin"
( cd "$PAK8_SEED" && cmake -E tar cf \
  "$INSTALL_RESOURCE/scope-precedence/pak8.pk3" --format=zip \
  scope/q3-pak-order.bin scope/pax-over-pak.bin )
printf '%s\n' 'pk3-loses' > "$SAME_PK3_SEED/scope/container-order.bin"
( cd "$SAME_PK3_SEED" && cmake -E tar cf \
  "$HOMEPATH/scope-precedence/same.pk3" --format=zip scope/container-order.bin )
printf '%s\n' 'sw3z-wins' > "$SAME_SW3Z_SEED/scope/container-order.bin"
"$SW3Z_TOOL" a "$HOMEPATH/scope-precedence/same.sw3z" \
  "$SAME_SW3Z_SEED" >/dev/null
printf '%s\n' 'install-loses' > "$ROOT_INSTALL_SEED/scope/root-order.bin"
( cd "$ROOT_INSTALL_SEED" && cmake -E tar cf \
  "$INSTALL_RESOURCE/scope-precedence/zzroot.pk3" --format=zip scope/root-order.bin )
printf '%s\n' 'home-wins' > "$ROOT_HOME_SEED/scope/root-order.bin"
( cd "$ROOT_HOME_SEED" && cmake -E tar cf \
  "$HOMEPATH/scope-precedence/zzroot.pk3" --format=zip scope/root-order.bin )
printf '%s\n' 'loose-loses' > \
  "$HOMEPATH/scope-precedence/scope/archive-over-loose.bin"

to_native() {
  local path="$1"
  if command -v cygpath >/dev/null 2>&1; then
    cygpath -w "$path"
  else
    ( cd "$path" && pwd )
  fi
}

BASEPATH_NATIVE="$(to_native "$BASEPATH")"
HOMEPATH_NATIVE="$(to_native "$HOMEPATH")"
LOOSE_LOG="$FIXTURE_ROOT/loose.log"
PACK_LOG="$FIXTURE_ROOT/pack.log"

cp "$SEED/default.cfg" "$HOMEPATH/base/default.cfg"
cp "$SEED/fs-aliases.lua" "$HOMEPATH/base/fs-aliases.lua"
printf '%s\n' 'loose-origin' > "$HOMEPATH/base/fixtures/canonical.bin"
"$DED" +set fs_installpath "$BASEPATH_NATIVE" +set fs_homepath "$HOMEPATH_NATIVE" \
	+set fs_debug 1 \
  +fs_resolve legacy/resource.bin \
  +fs_alias_verify legacy/resource.bin fixtures/canonical.bin \
  +fs_scope_verify home base default.cfg \
	+fs_scope_verify install scope-precedence scope/precedence.bin \
	+fs_scope_verify install scope-precedence scope/q3-pak-order.bin \
	+fs_scope_verify install scope-precedence scope/pax-over-pak.bin \
	+fs_scope_verify install scope-precedence scope/container-order.bin \
	+fs_scope_verify install scope-precedence scope/root-order.bin \
	+fs_scope_verify install scope-precedence scope/archive-over-loose.bin \
  +fs_scope_pure_verify home scope-pk3 scope/allow.bin scope-sw3z scope/deny.bin \
  +fs_scope_role_verify home scope-role scope/role.bin \
  +cvar_scope_verify \
  +fs_alias_stats +quit >"$LOOSE_LOG" 2>&1

if ! grep -q "published alias generation .* with 1 exact mapping" "$LOOSE_LOG" ||
   ! grep -q "requested=legacy/resource.bin canonical=fixtures/canonical.bin source=loose" "$LOOSE_LOG" ||
   ! grep -q "VFS alias verify: PASS.*independent_cursors=1" "$LOOSE_LOG" ||
   ! grep -q "VFS scope verify: PASS root=home dir=base qpath=default.cfg layers=2" "$LOOSE_LOG" ||
	 ! grep -q "FS_FOpenFileRead: scope/precedence.bin.*pax21.sw3z" "$LOOSE_LOG" ||
	 ! grep -q "FS_FOpenFileRead: scope/q3-pak-order.bin.*pak8.pk3" "$LOOSE_LOG" ||
	 ! grep -q "FS_FOpenFileRead: scope/pax-over-pak.bin.*pax21.sw3z" "$LOOSE_LOG" ||
	 ! grep -q "FS_FOpenFileRead: scope/container-order.bin.*same.sw3z" "$LOOSE_LOG" ||
	 ! grep -q "FS_FOpenFileRead: scope/root-order.bin.*homepath.*zzroot.pk3" "$LOOSE_LOG" ||
	 ! grep -q "FS_FOpenFileRead: scope/archive-over-loose.bin.*pax21.sw3z" "$LOOSE_LOG" ||
   ! grep -q "VFS scope pure verify: PASS.*containers=pk3+sw3z" "$LOOSE_LOG" ||
   ! grep -q "VFS scope role verify: PASS.*initial_scoped=1.*server_denied=1.*global_restored=1" "$LOOSE_LOG" ||
   ! grep -q "Cvar scope verify: PASS.*collision=closed.*global_preserved=1" "$LOOSE_LOG"; then
  tail -80 "$LOOSE_LOG"
  echo "FAIL: loose overlay alias contract"
  exit 1
fi

rm "$HOMEPATH/base/default.cfg" "$HOMEPATH/base/fs-aliases.lua" \
  "$HOMEPATH/base/fixtures/canonical.bin"
cp "$ARCHIVE" "$INSTALL_RESOURCE/base/alias_fixture.sw3z"
"$DED" +set fs_installpath "$BASEPATH_NATIVE" +set fs_homepath "$HOMEPATH_NATIVE" \
  +fs_resolve legacy/resource.bin \
  +fs_alias_verify legacy/resource.bin fixtures/canonical.bin \
  +fs_alias_stats +quit >"$PACK_LOG" 2>&1

if ! grep -q "requested=legacy/resource.bin canonical=fixtures/canonical.bin source=sw3z" "$PACK_LOG" ||
   ! grep -q "VFS alias verify: PASS.*independent_cursors=1" "$PACK_LOG"; then
  tail -80 "$PACK_LOG"
  echo "FAIL: SW3Z alias contract"
  exit 1
fi

LOOSE_ID="$(sed -n 's/.*requested=legacy\/resource.bin.*source_id=\([0-9][0-9]*\).*/\1/p' "$LOOSE_LOG" | head -1)"
PACK_ID="$(sed -n 's/.*requested=legacy\/resource.bin.*source_id=\([0-9][0-9]*\).*/\1/p' "$PACK_LOG" | head -1)"
if [ -z "$LOOSE_ID" ] || [ -z "$PACK_ID" ] || [ "$LOOSE_ID" = "$PACK_ID" ]; then
  echo "FAIL: same qpath from different origins did not produce distinct source identity"
  exit 1
fi

echo "PASS: alias identity/cursors, scoped VFS precedence/retirement/pure/role policy and cvar ownership"
