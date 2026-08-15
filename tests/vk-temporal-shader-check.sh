#!/bin/sh
set -eu
root=${1:?source root required}
shader_dir="$root/code/renderervk/shaders"
tmp=$(mktemp -d "${TMPDIR:-/tmp}/wired-temporal-shader.XXXXXX")
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
command -v glslangValidator >/dev/null 2>&1 || exit 77
command -v spirv-val >/dev/null 2>&1 || exit 77
command -v spirv-dis >/dev/null 2>&1 || exit 77
# compile.mjs uses a fixed local scratch name while checking all committed
# outputs. Run freshness validation from a private copy so read-only source
# mounts and concurrent CI jobs never contend for shaders/spirv/data.spv.
check_shader_dir="$tmp/check-shaders"
cp -R "$shader_dir" "$check_shader_dir"
(cd "$check_shader_dir" && node compile.mjs --check)
awk 'NR==1 { print; print "#extension GL_GOOGLE_include_directive : require"; next } { print }' \
	"$shader_dir/gen_frag.tmpl" > "$tmp/gen_frag.glsl"
glslangValidator -S vert -V -DUSE_TX1 -DUSE_TEMPORAL_MOTION -o "$tmp/v.spv" "$shader_dir/gen_vert.tmpl" >/dev/null
glslangValidator -I"$shader_dir" -S frag -V -DUSE_TX1 -DUSE_TEMPORAL_MOTION -o "$tmp/f.spv" "$tmp/gen_frag.glsl" >/dev/null
glslangValidator -I"$shader_dir" -S frag -V -DUSE_TX1 -DUSE_TEMPORAL_MOTION -DUSE_TEMPORAL_INVALIDATE -o "$tmp/i.spv" "$tmp/gen_frag.glsl" >/dev/null
glslangValidator -S frag -V -DUSE_TEMPORAL_INVALIDATE -o "$tmp/q.spv" "$shader_dir/iqm_skinning.frag" >/dev/null
for spv in "$tmp"/*.spv; do spirv-val "$spv"; spirv-dis "$spv" -o "$spv.dis"; done
vert="$tmp/v.spv.dis"; frag="$tmp/f.spv.dis"; inv="$tmp/i.spv.dis"; iqm="$tmp/q.spv.dis"
grep -Eq 'OpDecorate %_runtimearr_TemporalPayloadRecord[^ ]* ArrayStride 144' "$vert"
grep -Eq '%_runtimearr_TemporalPayloadRecord[^ ]* = OpTypeRuntimeArray %TemporalPayloadRecord[^ ]*' "$vert"
grep -Eq 'OpMemberDecorate %TemporalPayloadRecord[^ ]* 0 Offset 0' "$vert"
grep -Eq 'OpMemberDecorate %TemporalPayloadRecord[^ ]* 1 Offset 64' "$vert"
grep -Eq 'OpMemberDecorate %TemporalPayloadRecord[^ ]* 2 Offset 128' "$vert"
test "$(grep -Ec 'OpMemberDecorate %TemporalPayloadRecord[^ ]* [01] MatrixStride 16' "$vert")" -eq 2
test "$(grep -Ec 'OpMemberDecorate %TemporalPayloadRecord[^ ]* [01] ColMajor' "$vert")" -eq 2
grep -Eq 'OpDecorate .* DescriptorSet 3' "$vert"
grep -Eq 'OpDecorate %temporalPayloads DescriptorSet 3' "$vert"
grep -Eq 'OpDecorate %temporalPayloads Binding 1' "$vert"
for loc in 10 11 12; do grep -Eq "OpDecorate .* Location $loc" "$vert"; grep -Eq "OpDecorate .* Location $loc" "$frag"; done
grep -Eq 'OpDecorate %out_color Location 0' "$frag"
grep -Eq 'OpDecorate %out_temporal_velocity Location 1' "$frag"
grep -Eq 'OpDecorate %out_temporal_validity Location 2' "$frag"
grep -Eq '%temporalCurrentClip = OpVariable %_ptr_Output_v4float Output' "$vert"
grep -Eq '%temporalPreviousClip = OpVariable %_ptr_Output_v4float Output' "$vert"
grep -Eq '%temporalCurrentClip = OpVariable %_ptr_Input_v4float Input' "$frag"
grep -Eq '%temporalPreviousClip = OpVariable %_ptr_Input_v4float Input' "$frag"
grep -Eq '%out_temporal_velocity = OpVariable %_ptr_Output_v2float Output' "$frag"
grep -Eq '%out_temporal_validity = OpVariable %_ptr_Output_float Output' "$frag"
! grep -Eq 'OpDecorate %(temporalCurrentClip|temporalPreviousClip) (Flat|NoPerspective|Noperspective)' "$vert" "$frag"
grep -Eq 'OpDecorate %temporalOutcome Flat' "$vert"
grep -Eq 'OpDecorate %temporalOutcome Flat' "$frag"
for file in "$inv" "$iqm"; do
	grep -Eq 'OpDecorate %out_color Location 0' "$file"
	grep -Eq 'OpDecorate %out_temporal_velocity Location 1' "$file"
	grep -Eq 'OpDecorate %out_temporal_validity Location 2' "$file"
	grep -Eq '%out_color = OpVariable %_ptr_Output_v4float Output' "$file"
	grep -Eq '%out_temporal_velocity = OpVariable %_ptr_Output_v2float Output' "$file"
	grep -Eq '%out_temporal_validity = OpVariable %_ptr_Output_float Output' "$file"
done

# Exhaust every manifest-owned temporal generic blob, rather than treating the
# representative pair above as proof for the generated 40-pair catalog.
(cd "$shader_dir" && node --input-type=module -e '
	const m=await import("./shaders.manifest.mjs");
	const byOutput=new Map(m.default.map(e=>[e.output,e])); const emitted=new Set();
	for(const p of m.temporalGenericPairs) for(const [output,ordinary] of [
		[p.temporalVertex,p.ordinaryVertex],[p.temporalWriteFragment,p.ordinaryFragment],
		[p.temporalInvalidateFragment,p.ordinaryFragment]]) {
		if(emitted.has(output)) continue; emitted.add(output);
		const e=byOutput.get(output), o=byOutput.get(ordinary);
		if(!e||!o) throw new Error(`missing temporal/ordinary manifest entry ${output}/${ordinary}`);
		console.log([e.stage,e.output,(e.defines||[]).join(","),o.output,(o.defines||[]).join(",")].join("|"));
	}
') > "$tmp/temporal-variants.txt"
variant_count=0
vertex_count=0
write_count=0
invalidate_count=0
while IFS='|' read -r stage output defines ordinary_output ordinary_defines; do
	variant_count=$((variant_count + 1))
	case "$stage" in vert) source="$shader_dir/gen_vert.tmpl";; frag) source="$tmp/gen_frag.glsl";; *) exit 1;; esac
	define_args=
	old_ifs=$IFS; IFS=,
	for define in $defines; do define_args="$define_args -D$define"; done
	IFS=$old_ifs
	ordinary_define_args=
	old_ifs=$IFS; IFS=,
	for define in $ordinary_defines; do ordinary_define_args="$ordinary_define_args -D$define"; done
	IFS=$old_ifs
	# Manifest defines are fixed identifier tokens; intentional word splitting.
	# shellcheck disable=SC2086
	glslangValidator -I"$shader_dir" -S "$stage" -V $define_args -o "$tmp/$variant_count.spv" "$source" >/dev/null
	# shellcheck disable=SC2086
	glslangValidator -I"$shader_dir" -S "$stage" -V $ordinary_define_args -o "$tmp/$variant_count.ordinary.spv" "$source" >/dev/null
	spirv-val "$tmp/$variant_count.spv"
	spirv-val "$tmp/$variant_count.ordinary.spv"
	spirv-dis "$tmp/$variant_count.spv" -o "$tmp/$variant_count.dis"
	spirv-dis "$tmp/$variant_count.ordinary.spv" -o "$tmp/$variant_count.ordinary.dis"
	# Exact ordinary scene ABI is preserved. Only the payload descriptor and
	# loc10/11/12 + auxiliary outputs are admitted temporal additions.
	grep --color=never -E 'OpDecorate .* (DescriptorSet|Binding) ' "$tmp/$variant_count.dis" | grep --color=never -v temporalPayloads | sort > "$tmp/current.desc"
	grep --color=never -E 'OpDecorate .* (DescriptorSet|Binding) ' "$tmp/$variant_count.ordinary.dis" | sort > "$tmp/ordinary.desc"
	cmp "$tmp/current.desc" "$tmp/ordinary.desc"
	grep --color=never -E 'OpDecorate .* Location ' "$tmp/$variant_count.dis" | grep --color=never -Ev '(temporalCurrentClip|temporalPreviousClip|temporalOutcome|out_temporal_)' | sort > "$tmp/current.loc"
	grep --color=never -E 'OpDecorate .* Location ' "$tmp/$variant_count.ordinary.dis" | sort > "$tmp/ordinary.loc"
	cmp "$tmp/current.loc" "$tmp/ordinary.loc"
	awk '/OpDecorate %[A-Za-z0-9_]+ Location / { print $2 }' "$tmp/$variant_count.ordinary.dis" | sort -u > "$tmp/ordinary.interface-ids"
	while read -r interface_id; do
		grep --color=never -E "^[[:space:]]*${interface_id} = OpVariable " "$tmp/$variant_count.dis" > "$tmp/current.var"
		grep --color=never -E "^[[:space:]]*${interface_id} = OpVariable " "$tmp/$variant_count.ordinary.dis" > "$tmp/ordinary.var"
		cmp "$tmp/current.var" "$tmp/ordinary.var"
	done < "$tmp/ordinary.interface-ids"
	awk '/OpDecorate %[A-Za-z0-9_]+ (DescriptorSet|Binding) / { print $2 }' "$tmp/$variant_count.ordinary.dis" | sort -u > "$tmp/ordinary.descriptor-ids"
	while read -r descriptor_id; do
		grep --color=never -E "^[[:space:]]*${descriptor_id} = OpVariable " "$tmp/$variant_count.dis" | awk '{print $NF}' > "$tmp/current.storage"
		grep --color=never -E "^[[:space:]]*${descriptor_id} = OpVariable " "$tmp/$variant_count.ordinary.dis" | awk '{print $NF}' > "$tmp/ordinary.storage"
		cmp "$tmp/current.storage" "$tmp/ordinary.storage"
	done < "$tmp/ordinary.descriptor-ids"
	grep --color=never -E 'Op(Member)?Decorate .* (Block|Offset|MatrixStride|ArrayStride)' "$tmp/$variant_count.dis" | grep --color=never -v TemporalPayload | sort > "$tmp/current.layout"
	grep --color=never -E 'Op(Member)?Decorate .* (Block|Offset|MatrixStride|ArrayStride)' "$tmp/$variant_count.ordinary.dis" | sort > "$tmp/ordinary.layout"
	cmp "$tmp/current.layout" "$tmp/ordinary.layout"
	grep --color=never -E '^[[:space:]]*%(UBO|_arr_[A-Za-z0-9_]+) = OpType(Struct|Array)' "$tmp/$variant_count.dis" | sort > "$tmp/current.types"
	grep --color=never -E '^[[:space:]]*%(UBO|_arr_[A-Za-z0-9_]+) = OpType(Struct|Array)' "$tmp/$variant_count.ordinary.dis" | sort > "$tmp/ordinary.types"
	cmp "$tmp/current.types" "$tmp/ordinary.types"
	grep --color=never -E 'OpExecutionMode ' "$tmp/$variant_count.dis" | sort > "$tmp/current.mode"
	grep --color=never -E 'OpExecutionMode ' "$tmp/$variant_count.ordinary.dis" | sort > "$tmp/ordinary.mode"
	cmp "$tmp/current.mode" "$tmp/ordinary.mode"
	grep --color=never -E 'OpDecorate .* SpecId ' "$tmp/$variant_count.dis" | sort > "$tmp/current.spec"
	grep --color=never -E 'OpDecorate .* SpecId ' "$tmp/$variant_count.ordinary.dis" | sort > "$tmp/ordinary.spec"
	case "$ordinary_output" in
		frag_tx0*_bindless)
			grep -Eq 'SpecId 0$' "$tmp/ordinary.spec"
			grep -Eq 'SpecId 1$' "$tmp/ordinary.spec"
			! grep -Eq 'SpecId [01]$' "$tmp/current.spec"
			grep --color=never -Ev 'SpecId [01]$' "$tmp/ordinary.spec" > "$tmp/ordinary.spec.passive" || true
			cmp "$tmp/current.spec" "$tmp/ordinary.spec.passive";;
		*) cmp "$tmp/current.spec" "$tmp/ordinary.spec";;
	esac
	if [ "$stage" = frag ]; then
		grep --color=never -E '%out_color = OpVariable %_ptr_Output_' "$tmp/$variant_count.dis" > "$tmp/current.scene-type"
		grep --color=never -E '%out_color = OpVariable %_ptr_Output_' "$tmp/$variant_count.ordinary.dis" > "$tmp/ordinary.scene-type"
		cmp "$tmp/current.scene-type" "$tmp/ordinary.scene-type"
	fi
	case "$output" in
		vert_*_temporal_motion)
			vertex_count=$((vertex_count + 1))
			grep -Eq 'OpDecorate %temporalPayloads DescriptorSet 3' "$tmp/$variant_count.dis"
			grep -Eq 'OpDecorate %temporalPayloads Binding 1' "$tmp/$variant_count.dis"
			grep -Eq 'OpDecorate %temporalCurrentClip Location 10' "$tmp/$variant_count.dis"
			grep -Eq 'OpDecorate %temporalPreviousClip Location 11' "$tmp/$variant_count.dis"
			! grep -Eq 'OpDecorate %(temporalCurrentClip|temporalPreviousClip) (Flat|NoPerspective)' "$tmp/$variant_count.dis"
			;;
		frag_*_temporal_write_bindless)
			write_count=$((write_count + 1))
			grep -Eq 'OpDecorate %temporalCurrentClip Location 10' "$tmp/$variant_count.dis"
			grep -Eq 'OpDecorate %temporalPreviousClip Location 11' "$tmp/$variant_count.dis"
			grep -Eq 'OpDecorate %out_color Location 0' "$tmp/$variant_count.dis"
			grep -Eq 'OpDecorate %out_temporal_velocity Location 1' "$tmp/$variant_count.dis"
			grep -Eq 'OpDecorate %out_temporal_validity Location 2' "$tmp/$variant_count.dis"
			;;
		frag_*_temporal_invalidate_bindless)
			invalidate_count=$((invalidate_count + 1))
			grep -Eq 'OpDecorate %out_color Location 0' "$tmp/$variant_count.dis"
			grep -Eq 'OpDecorate %out_temporal_velocity Location 1' "$tmp/$variant_count.dis"
			grep -Eq 'OpDecorate %out_temporal_validity Location 2' "$tmp/$variant_count.dis"
			;;
		*) exit 1;;
	esac
done < "$tmp/temporal-variants.txt"
test "$variant_count" -eq 76
test "$vertex_count" -eq 36
test "$write_count" -eq 20
test "$invalidate_count" -eq 20
echo 'vk temporal shader reflection contract: PASS'
