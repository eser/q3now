#!/usr/bin/env bash
set -eu

root=${1:-.}
command -v glslangValidator >/dev/null 2>&1 || exit 77
command -v spirv-val >/dev/null 2>&1 || exit 77
command -v spirv-dis >/dev/null 2>&1 || exit 77

tmp=$(mktemp -d "${TMPDIR:-/tmp}/wired-h4-store-reflect.XXXXXX")
trap 'rm -rf "$tmp"' EXIT
spv="$tmp/temporal-history-store.spv"
dis="$tmp/temporal-history-store.dis"
glslangValidator -S comp -V -o "$spv" \
	"$root/code/render/ral/backends/vulkan/renderer/shaders/temporal_history_store.comp" >/dev/null
spirv-val "$spv"
spirv-dis "$spv" -o "$dis"

grep -Eq '^[[:space:]]*OpExecutionMode %main LocalSize 8 8 1$' "$dis"
for pair in 'currentColor 0' 'currentDeviceDepth 1' 'nearestSampler 2' \
	'outputColor 3' 'outputLinearDepth 4'
do
	name=${pair% *}; binding=${pair##* }
	grep -Eq "^[[:space:]]*OpName %${name} \"${name}\"$" "$dis"
	grep -Eq "^[[:space:]]*OpDecorate %${name} DescriptorSet 0$" "$dis"
	grep -Eq "^[[:space:]]*OpDecorate %${name} Binding ${binding}$" "$dis"
done
grep -Eq '^[[:space:]]*OpDecorate %outputColor NonReadable$' "$dis"
grep -Eq '^[[:space:]]*OpDecorate %outputLinearDepth NonReadable$' "$dis"
grep -Eq '^[[:space:]]*OpMemberDecorate %TemporalHistoryPush 0 Offset 0$' "$dis"
grep -Eq '^[[:space:]]*OpMemberDecorate %TemporalHistoryPush 1 Offset 8$' "$dis"
grep -Eq '^[[:space:]]*OpMemberDecorate %TemporalHistoryPush 2 Offset 12$' "$dis"
grep -Eq '^[[:space:]]*OpDecorate %TemporalHistoryPush Block$' "$dis"
grep -Eq '^[[:space:]]*%TemporalHistoryPush = OpTypeStruct %v2uint %float %float$' "$dis"
grep -Eq 'OpTypeImage %float 2D 0 0 0 2 Rgba16f$' "$dis"
grep -Eq 'OpTypeImage %float 2D 0 0 0 2 R32f$' "$dis"

python3 - "$dis" <<'PYEOF'
import re,sys
lines=open(sys.argv[1],encoding='utf-8').read().splitlines()
text='\n'.join(lines)
fetches=[line for line in lines if ' OpImageFetch ' in line]
writes=[line for line in lines if re.search(r'\bOpImageWrite\b',line)]
if len(fetches)!=2:
    raise SystemExit(f'exact texelFetch count changed: {len(fetches)}')
if len(writes)!=2:
    raise SystemExit(f'exact imageStore count changed: {len(writes)}')
if ' OpImageSample' in text or ' OpImageSparseSample' in text:
    raise SystemExit('normalized/filtering sample reintroduced into Store')
if sum(' OpFDiv ' in line for line in lines)!=1:
    raise SystemExit('linear-depth division dataflow changed')
PYEOF

echo 'vk temporal history Store shader reflection: PASS'
