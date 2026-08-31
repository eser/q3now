#!/usr/bin/env bash
# Static exact-extension compatibility gate for imported id shaders, embedded
# MD3 material slots, and all shipped physical maps. Imported sources remain
# immutable; Wired-owned shaders must use canonical paths directly. Imported
# references with no packaged alternative are reported; runtime smoke owns the
# selected-definition availability gate.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BASE_ARCHIVE="${WIRED_BASE_CONTENT:-${HOME}/wired/q3now-preview/base/pax01.sw3z}"
MOD_ARCHIVE="${WIRED_MOD_CONTENT:-${ROOT}/build/base/pax21.sw3z}"
SW3Z="${WIRED_SW3Z:-${ROOT}/tools/sw3z-archiver/cmd/sw3z/sw3z}"

if [[ ! -f "$BASE_ARCHIVE" || ! -f "$MOD_ARCHIVE" || ! -x "$SW3Z" ]]; then
	echo "SKIP: shader audit needs pax01, pax21 and the sw3z tool" >&2
	exit 77
fi

AUDIT_DIR="$(mktemp -d "${TMPDIR:-/tmp}/q3now-shader-audit.XXXXXX")"
trap 'rm -rf "$AUDIT_DIR"' EXIT
mkdir -p "$AUDIT_DIR/pax01" "$AUDIT_DIR/pax21"
"$SW3Z" x "$BASE_ARCHIVE" "$AUDIT_DIR/pax01" >/dev/null
"$SW3Z" x "$MOD_ARCHIVE" "$AUDIT_DIR/pax21" >/dev/null

node "$ROOT/tools/shader-resource-audit.mjs" \
	--imported-root "$AUDIT_DIR/pax01" \
	--owned-root "$ROOT/modfiles" \
	--content-root "$AUDIT_DIR/pax01" \
	--content-root "$AUDIT_DIR/pax21" \
	--content-root "$ROOT/modfiles" \
	--map-root "$AUDIT_DIR/pax01" \
	--aliases "$ROOT/modfiles/fs-aliases.lua"
