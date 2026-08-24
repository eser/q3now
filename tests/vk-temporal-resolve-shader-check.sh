#!/usr/bin/env bash
set -eu

root=${1:-.}
command -v glslangValidator >/dev/null 2>&1 || exit 77
command -v spirv-val >/dev/null 2>&1 || exit 77
command -v spirv-dis >/dev/null 2>&1 || exit 77

tmp=$(mktemp -d "${TMPDIR:-/tmp}/wired-h3-reflect.XXXXXX")
trap 'rm -rf "$tmp"' EXIT
spv="$tmp/temporal-resolve.spv"
dis="$tmp/temporal-resolve.spv.dis"
glslangValidator -S comp -V -o "$spv" \
	"$root/code/render/ral/backends/vulkan/renderer/shaders/temporal_resolve.comp" >/dev/null
spirv-val "$spv"
spirv-dis "$spv" -o "$dis"

python3 - "$dis" <<'PYEOF'
import re,sys
lines=open(sys.argv[1],encoding="utf-8").read().splitlines()
decorated=set();opcode={};stored={}
for line in lines:
    m=re.fullmatch(r"\s*OpDecorate\s+(%\S+)\s+NoContraction",line)
    if m:decorated.add(m.group(1));continue
    m=re.fullmatch(r"\s*(%\S+)\s*=\s+(Op\S+).*$",line)
    if m:opcode[m.group(1)]=m.group(2);continue
    m=re.fullmatch(r"\s*OpStore\s+(%\S+)\s+(%\S+)",line)
    if m:stored.setdefault(m.group(1),[]).append(m.group(2))
required={
 "%range":"OpFSub","%scaled":"OpFMul","%denominator":"OpFAdd",
 "%numerator":"OpFMul","%result":"OpFDiv",
 "%oneMinus":"OpFSub","%left":"OpFMul","%right":"OpFMul","%result_0":"OpFAdd",
 "%oneMinus_0":"OpFSub","%left_0":"OpVectorTimesScalar",
 "%right_0":"OpVectorTimesScalar","%result_1":"OpFAdd",
 "%oneMinus_1":"OpFSub","%left_1":"OpVectorTimesScalar",
 "%right_1":"OpVectorTimesScalar","%result_2":"OpFAdd",
 "%f":"OpFSub","%f_0":"OpFSub",
 "%jitterDelta":"OpFSub","%jitterTexel":"OpFMul","%motionTexel":"OpFMul",
 "%unjitteredPreviousTexel":"OpFSub","%previousTexel":"OpFAdd",
 "%previousCenter":"OpFAdd",
 # scalePrecise: history weight scaled by the pixel's motion confidence.
 # glslang renames the second `scaled` to %scaled_0 (the first belongs to
 # linearizeDepth). Pinned because `precise` does NOT survive being written as
 # clamp(a,0,1) * b — that form compiles to an undecorated OpFMul, which is why
 # the multiply lives in its own function taking pre-clamped parameters.
 "%scaled_0":"OpFMul",
}
for variable,want in required.items():
    values=stored.get(variable,[])
    if len(values)!=1 or opcode.get(values[0])!=want or values[0] not in decorated:
        raise SystemExit(f"missing exact NoContraction {variable} {want}: {values}")
text="\n".join(lines)
if " FMix " in text or " Fract " in text:
    raise SystemExit("contracted/opaque temporal interpolation survived")
divisions=[result for result,op in opcode.items() if op=="OpFDiv"]
if divisions!=stored.get("%result",[]):
    raise SystemExit(f"unexpected color-path OpFDiv: {divisions}")

# Motion validity is a CONTINUOUS confidence, not a flag.
#
# It used to gate reprojection with `valid > 0.5` and then blend with a constant
# historyWeight, which collapsed the whole R8 channel to two states. Alpha-tested
# cut-outs are the motivating case: a pixel's coverage flips between frames, so
# its history is partly trustworthy — all-in ghosts, all-out loses the AA. The
# admission test is therefore `> 0` (zero still means "no usable history", where
# the velocity is meaningless), and the surviving confidence scales the blend.
#
# Both halves are pinned here, because either one silently reverts on its own:
# restore the 0.5 threshold and intermediate confidences never reach the blend;
# drop the scale and they reach it but change nothing.
if not re.search(r"%\S+\s*=\s*OpFOrdGreaterThan\s+%bool\s+%\S+\s+%float_0\b",text):
    raise SystemExit("motion validity is not admitted as a continuous > 0 test")
if re.search(r"OpFOrdGreaterThan\s+%bool\s+%\S+\s+%float_0_5\b",text):
    raise SystemExit("motion validity regressed to a binary 0.5 threshold")
scaled_multiply=stored.get("%scaled_0",[])
if len(scaled_multiply)!=1:
    raise SystemExit(f"history weight is not scaled exactly once: {scaled_multiply}")
PYEOF

grep -Eq '^[[:space:]]*OpExecutionMode %main LocalSize 8 8 1$' "$dis"
for pair in \
	'currentColor 0' 'currentDeviceDepth 1' 'previousColor 2' \
	'previousLinearDepth 3' 'motionVelocity 4' 'motionValidity 5' \
	'nearestSampler 6' 'resolvedColor 7'
do
	name=${pair% *}
	binding=${pair##* }
	grep -Eq "^[[:space:]]*OpName %${name} \"${name}\"$" "$dis"
	grep -Eq "^[[:space:]]*OpDecorate %${name} DescriptorSet 0$" "$dis"
	grep -Eq "^[[:space:]]*OpDecorate %${name} Binding ${binding}$" "$dis"
done

texture_ptr=$(sed -n 's/^[[:space:]]*%currentColor = OpVariable \(%[^ ]*\) UniformConstant$/\1/p' "$dis")
test -n "$texture_ptr"
for name in currentColor currentDeviceDepth previousColor previousLinearDepth \
	motionVelocity motionValidity
do
	grep -Eq "^[[:space:]]*%${name} = OpVariable ${texture_ptr} UniformConstant$" "$dis"
done
texture_type=$(sed -n "s/^[[:space:]]*${texture_ptr} = OpTypePointer UniformConstant \(%[^ ]*\)$/\\1/p" "$dis")
test -n "$texture_type"
grep -Eq "^[[:space:]]*${texture_type} = OpTypeImage %float 2D 0 0 0 1 Unknown$" "$dis"

sampler_ptr=$(sed -n 's/^[[:space:]]*%nearestSampler = OpVariable \(%[^ ]*\) UniformConstant$/\1/p' "$dis")
sampler_type=$(sed -n "s/^[[:space:]]*${sampler_ptr} = OpTypePointer UniformConstant \(%[^ ]*\)$/\\1/p" "$dis")
test -n "$sampler_ptr" && test -n "$sampler_type"
grep -Eq "^[[:space:]]*${sampler_type} = OpTypeSampler$" "$dis"

offset=0
for member in 0 1 2 3 4 5 6 7 8
do
	grep -Eq "^[[:space:]]*OpMemberDecorate %TemporalResolvePush ${member} Offset ${offset}$" "$dis"
	case "$member" in
		0) offset=8 ;;
		1) offset=16 ;;
		2) offset=24 ;;
		*) offset=$((offset + 4)) ;;
	esac
done
test "$offset" -eq 48
grep -Eq '^[[:space:]]*OpDecorate %TemporalResolvePush Block$' "$dis"
grep -Eq '^[[:space:]]*%TemporalResolvePush = OpTypeStruct %v2uint %v2float %v2float %float %float %float %float %float %uint$' "$dis"
output_ptr=$(sed -n 's/^[[:space:]]*%resolvedColor = OpVariable \(%[^ ]*\) UniformConstant$/\1/p' "$dis")
output_type=$(sed -n "s/^[[:space:]]*${output_ptr} = OpTypePointer UniformConstant \(%[^ ]*\)$/\\1/p" "$dis")
test -n "$output_ptr" && test -n "$output_type"
grep -Eq "^[[:space:]]*${output_type} = OpTypeImage %float 2D 0 0 0 2 Rgba16f$" "$dis"
grep -Eq '^[[:space:]]*OpDecorate %resolvedColor NonReadable$' "$dis"

echo 'vk temporal resolve shader reflection: PASS'
