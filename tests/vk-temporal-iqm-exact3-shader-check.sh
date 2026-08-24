#!/bin/sh
set -eu
root=${1:?source root required}
shader_dir="$root/code/render/ral/backends/vulkan/renderer/shaders"
tmp=$(mktemp -d "${TMPDIR:-/tmp}/wired-iqm-exact3-shader.XXXXXX")
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
command -v glslangValidator >/dev/null 2>&1 || exit 77
command -v spirv-val >/dev/null 2>&1 || exit 77
command -v spirv-dis >/dev/null 2>&1 || exit 77
cp -R "$shader_dir" "$tmp/check-shaders"
(cd "$tmp/check-shaders" && node compile.mjs --check)
awk '/^#version / { print; print "#extension GL_GOOGLE_include_directive : require"; next } { print }' \
	"$shader_dir/iqm_temporal_exact3.frag" > "$tmp/exact3.frag"
glslangValidator -S vert -V -o "$tmp/v.spv" "$shader_dir/iqm_temporal_exact3.vert" >/dev/null
glslangValidator -I"$shader_dir" -S frag -V -o "$tmp/w.spv" "$tmp/exact3.frag" >/dev/null
glslangValidator -I"$shader_dir" -S frag -V -DUSE_TEMPORAL_INVALIDATE \
	-o "$tmp/i.spv" "$tmp/exact3.frag" >/dev/null
for spv in "$tmp/v.spv" "$tmp/w.spv" "$tmp/i.spv"; do
	spirv-val "$spv"
	spirv-dis "$spv" -o "$spv.dis"
done
v="$tmp/v.spv.dis"; w="$tmp/w.spv.dis"; i="$tmp/i.spv.dis"
grep -Eq 'OpMemberDecorate %TemporalIqmRecord 0 Offset 0' "$v"
grep -Eq 'OpMemberDecorate %TemporalIqmRecord 1 Offset 6144' "$v"
grep -Eq 'OpMemberDecorate %TemporalIqmRecord 2 Offset 12288' "$v"
grep -Eq 'OpMemberDecorate %TemporalIqmRecord 3 Offset 12352' "$v"
grep -Eq 'OpMemberDecorate %TemporalIqmRecord 4 Offset 12416' "$v"
test "$(grep -Ec 'OpMemberDecorate %TemporalIqmRecord [234] MatrixStride 16' "$v")" -eq 3
test "$(grep -Ec 'OpMemberDecorate %TemporalIqmRecord [234] ColMajor' "$v")" -eq 3
grep -Eq 'OpDecorate %_runtimearr_TemporalIqmRecord ArrayStride 12480' "$v"
grep -Eq '%TemporalIqmRecord = OpTypeStruct %_arr_v4float_uint_384 %_arr_v4float_uint_384_0 %mat4v4float %mat4v4float %mat4v4float' "$v"
grep -Eq '%_arr_v4float_uint_384 = OpTypeArray %v4float %uint_384' "$v"
grep -Eq '%_arr_v4float_uint_384_0 = OpTypeArray %v4float %uint_384' "$v"
grep -Eq 'OpMemberDecorate %TemporalIqmPayload 0 NonWritable' "$v"
grep -Eq 'OpDecorate %temporalIqmPayload NonWritable' "$v"
grep -Eq 'OpDecorate %temporalIqmPayload DescriptorSet 0' "$v"
grep -Eq 'OpDecorate %temporalIqmPayload Binding 0' "$v"
grep -Eq 'OpDecorate %gl_InstanceIndex BuiltIn InstanceIndex' "$v"
! grep -Eq 'OpLoad %TemporalIqmRecord|OpCopyMemory' "$v"
! grep -Eq 'OpFunctionParameter %_ptr_Function__arr_v4float_uint_384' "$v"
grep -Eq 'OpDecorate %in_position Location 0' "$v"
grep -Eq 'OpDecorate %in_normal Location 1' "$v"
grep -Eq 'OpDecorate %in_tex_coord Location 2' "$v"
grep -Eq 'OpDecorate %in_tangent Location 3' "$v"
grep -Eq 'OpDecorate %in_bone_weights Location 4' "$v"
grep -Eq 'OpDecorate %in_bone_indices Location 5' "$v"
grep -Eq '%in_position = OpVariable %_ptr_Input_v3float Input' "$v"
grep -Eq '%in_normal = OpVariable %_ptr_Input_v3float Input' "$v"
grep -Eq '%in_tex_coord = OpVariable %_ptr_Input_v2float Input' "$v"
grep -Eq '%in_tangent = OpVariable %_ptr_Input_v4float Input' "$v"
grep -Eq '%in_bone_weights = OpVariable %_ptr_Input_v4float Input' "$v"
grep -Eq '%in_bone_indices = OpVariable %_ptr_Input_v4uint Input' "$v"
grep -Eq 'OpDecorate %temporalCurrentClip Location 10' "$v"
grep -Eq 'OpDecorate %temporalPreviousClip Location 11' "$v"
grep -Eq '%temporalCurrentClip = OpVariable %_ptr_Output_v4float Output' "$v"
grep -Eq '%temporalPreviousClip = OpVariable %_ptr_Output_v4float Output' "$v"
! grep -Eq 'OpDecorate %(temporalCurrentClip|temporalPreviousClip) (Flat|NoPerspective|Noperspective)' "$v"
for f in "$w" "$i"; do
	grep -Eq 'OpDecorate %wired_bindless_images DescriptorSet 1' "$f"
	grep -Eq 'OpDecorate %wired_bindless_images Binding 0' "$f"
	grep -Eq 'OpDecorate %wired_bindless_samplers DescriptorSet 1' "$f"
	grep -Eq 'OpDecorate %wired_bindless_samplers Binding 1' "$f"
	grep -Eq 'OpMemberDecorate %TemporalIqmSurfacePush 0 Offset 0' "$f"
	grep -Eq 'OpMemberDecorate %TemporalIqmSurfacePush 1 Offset 4' "$f"
	grep -Eq '%TemporalIqmSurfacePush = OpTypeStruct %uint %uint' "$f"
	grep -Eq 'OpDecorate %out_color Location 0' "$f"
	grep -Eq '%out_color = OpVariable %_ptr_Output_v4float Output' "$f"
	grep -Eq 'OpDecorate %out_temporal_velocity Location 1' "$f"
	grep -Eq 'OpDecorate %out_temporal_validity Location 2' "$f"
	grep -Eq 'OpDecorate %temporalCurrentClip Location 10' "$f"
	grep -Eq 'OpDecorate %temporalPreviousClip Location 11' "$f"
	grep -Eq '%temporalCurrentClip = OpVariable %_ptr_Input_v4float Input' "$f"
	grep -Eq '%temporalPreviousClip = OpVariable %_ptr_Input_v4float Input' "$f"
	grep -Eq '%out_temporal_velocity = OpVariable %_ptr_Output_v2float Output' "$f"
	grep -Eq '%out_temporal_validity = OpVariable %_ptr_Output_float Output' "$f"
	! grep -Eq 'OpDecorate %(temporalCurrentClip|temporalPreviousClip) (Flat|NoPerspective|Noperspective)' "$f"
	test "$(grep -Ec 'OpDecorate %[^ ]+ NonUniform' "$f")" -ge 6
	test "$(grep -Ec 'OpStore %out_color ' "$f")" -eq 1
done
test "$(grep -Ec 'OpStore %out_temporal_validity %float_0' "$w")" -eq 1
test "$(grep -Ec 'OpStore %out_temporal_validity %float_1' "$w")" -eq 1
test "$(grep -Ec 'OpStore %out_temporal_velocity ' "$w")" -eq 2
test "$(grep -Ec 'OpStore %out_temporal_validity %float_0' "$i")" -eq 1
test "$(grep -Ec 'OpStore %out_temporal_validity %float_1' "$i")" -eq 0
test "$(grep -Ec 'OpStore %out_temporal_velocity ' "$i")" -eq 1
echo 'vk temporal IQM exact3 shader reflection contract: PASS'
