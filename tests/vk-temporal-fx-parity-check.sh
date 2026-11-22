#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# The spatial fallback filter exists in two shaders (the temporal resolve and the bootstrap resolve) because compile.mjs has no
# #include. Both copies sit between the same two marker lines and must stay byte-identical.
set -eu
root=${1:-.}
dir="$root/code/render/ral/backends/vulkan/renderer/shaders"
extract() {
	awk '/^\/\/ ---- Spatial fallback/{on=1} on{print} /^\/\/ ---- end spatial fallback ----/{on=0}' "$1"
}
a=$(extract "$dir/temporal_resolve.comp")
b=$(extract "$dir/temporal_bootstrap_aa.comp")
[ -n "$a" ] || { echo "FAIL: no spatial fallback block in temporal_resolve.comp"; exit 1; }
[ "$a" = "$b" ] || { echo "FAIL: the spatial fallback block differs between temporal_resolve.comp and temporal_bootstrap_aa.comp"; diff <(printf '%s\n' "$a") <(printf '%s\n' "$b") | head -10; exit 1; }
echo "vk temporal spatial fallback parity: PASS"
