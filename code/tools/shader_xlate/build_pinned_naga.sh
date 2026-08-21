#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors

set -eu

NAGA_VERSION=30.0.0
WIRED_VERSION=30.0.0+wired-portable-v1

if [ "$#" -ne 1 ]; then
	echo "usage: $0 <output-naga-path>" >&2
	exit 2
fi

OUTPUT=$1
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PATCH_FILE=$SCRIPT_DIR/patches/naga-30.0.0-wired-portable-v1.patch
WORK_DIR=$(mktemp -d "${TMPDIR:-/tmp}/wired-naga.XXXXXX")
trap 'rm -rf "$WORK_DIR"' EXIT HUP INT TERM

command -v cargo >/dev/null 2>&1 || {
	echo "ERROR: cargo is required to build the pinned Naga translator" >&2
	exit 1
}
command -v patch >/dev/null 2>&1 || {
	echo "ERROR: patch is required to build the pinned Naga translator" >&2
	exit 1
}
test -f "$PATCH_FILE" || {
	echo "ERROR: missing Wired Naga patch: $PATCH_FILE" >&2
	exit 1
}

mkdir -p "$WORK_DIR/fetch/src" "$WORK_DIR/cargo-home"
printf '%s\n' \
	'[package]' \
	'name = "wired-naga-fetch"' \
	'version = "0.0.0"' \
	'edition = "2021"' \
	'' \
	'[dependencies]' \
	"naga = \"=$NAGA_VERSION\"" \
	"naga-cli = \"=$NAGA_VERSION\"" \
	> "$WORK_DIR/fetch/Cargo.toml"
printf '%s\n' 'fn main() {}' > "$WORK_DIR/fetch/src/main.rs"

# The first fetch obtains the exact top-level crate archives. The naga-cli
# crate's published Cargo.lock then pins every transitive build dependency.
CARGO_HOME="$WORK_DIR/cargo-home" cargo fetch \
	--manifest-path "$WORK_DIR/fetch/Cargo.toml"

set -- "$WORK_DIR"/cargo-home/registry/src/*/naga-$NAGA_VERSION
test "$#" -eq 1 && test -d "$1" || {
	echo "ERROR: expected one naga-$NAGA_VERSION source directory" >&2
	exit 1
}
NAGA_SOURCE=$1

set -- "$WORK_DIR"/cargo-home/registry/src/*/naga-cli-$NAGA_VERSION
test "$#" -eq 1 && test -d "$1" || {
	echo "ERROR: expected one naga-cli-$NAGA_VERSION source directory" >&2
	exit 1
}
NAGA_CLI_SOURCE=$1
REGISTRY_SOURCE=$(dirname -- "$NAGA_SOURCE")

CARGO_HOME="$WORK_DIR/cargo-home" cargo fetch \
	--manifest-path "$NAGA_CLI_SOURCE/Cargo.toml" --locked
(cd "$REGISTRY_SOURCE" && patch -p1 < "$PATCH_FILE")
CARGO_HOME="$WORK_DIR/cargo-home" cargo build \
	--manifest-path "$NAGA_CLI_SOURCE/Cargo.toml" --release --locked --offline

BUILT_NAGA=$NAGA_CLI_SOURCE/target/release/naga
if [ ! -x "$BUILT_NAGA" ] && [ -x "$BUILT_NAGA.exe" ]; then
	BUILT_NAGA=$BUILT_NAGA.exe
fi
test -x "$BUILT_NAGA" || {
	echo "ERROR: pinned Naga build did not produce an executable" >&2
	exit 1
}

ACTUAL_VERSION=$($BUILT_NAGA --version)
test "$ACTUAL_VERSION" = "$WIRED_VERSION" || {
	echo "ERROR: patched Naga identity mismatch: $ACTUAL_VERSION" >&2
	exit 1
}

mkdir -p "$(dirname -- "$OUTPUT")"
cp "$BUILT_NAGA" "$OUTPUT"
chmod 0755 "$OUTPUT"
echo "built $OUTPUT ($WIRED_VERSION)"
