#!/bin/sh
# compile.sh — macOS port of compile.bat
# Compiles VK GLSL shader templates → SPIR-V → C hex arrays

set -e

DIR="$(cd "$(dirname "$0")" && pwd)"
BH="$DIR/bin2hex"
CL="$(which glslangValidator)"
TMPF="$DIR/spirv/data.spv"
OUTF="+spirv/shader_data.c"

if [ ! -x "$CL" ]; then
    echo "ERROR: glslangValidator not found. Install with: brew install glslang"
    exit 1
fi

if [ ! -x "$BH" ]; then
    echo "Building bin2hex..."
    cc -o "$BH" "$DIR/bin2hex.c"
fi

mkdir -p "$DIR/spirv"
rm -f "$DIR/spirv/shader_data.c"

cd "$DIR"

compile() {
    "$CL" -S "$1" -V -o "$TMPF" $2 && "$BH" "$TMPF" $OUTF "$3" && rm -f "$TMPF"
}

echo "==> Compiling individual shaders..."
for f in *.vert; do
    [ -f "$f" ] || continue
    case "$f" in smaa_*|rail_*) continue;; esac  # compiled separately below
    name="${f%.vert}_vert_spv"
    compile vert "$f" "$name"
done
for f in *.frag; do
    [ -f "$f" ] || continue
    case "$f" in smaa_*|rail_*) continue;; esac  # compiled separately below
    name="${f%.frag}_frag_spv"
    compile frag "$f" "$name"
done

echo "==> Compiling gamma post-process variants..."
compile frag "gamma.frag -DUSE_SSAO" gamma_ssao_frag_spv
compile frag "gamma.frag -DUSE_TONEMAP" gamma_tonemap_frag_spv
compile frag "gamma.frag -DUSE_COLOR_GRADING" gamma_colorgrade_frag_spv
compile frag "gamma.frag -DUSE_SSAO -DUSE_TONEMAP" gamma_ssao_tonemap_frag_spv
compile frag "gamma.frag -DUSE_SSAO -DUSE_TONEMAP -DUSE_COLOR_GRADING" gamma_full_frag_spv
compile frag "gamma.frag -DUSE_TONEMAP -DUSE_COLOR_GRADING" gamma_tonemap_cg_frag_spv
compile frag "gamma.frag -DUSE_FXAA" gamma_fxaa_frag_spv
compile frag "gamma.frag -DUSE_FXAA -DUSE_SSAO" gamma_fxaa_ssao_frag_spv
compile frag "gamma.frag -DUSE_FXAA -DUSE_SSAO -DUSE_TONEMAP -DUSE_COLOR_GRADING" gamma_all_frag_spv
compile frag "gamma.frag -DUSE_GODRAYS" gamma_godrays_frag_spv
compile frag "gamma.frag -DUSE_SSAO -DUSE_GODRAYS" gamma_ssao_godrays_frag_spv
compile frag "gamma.frag -DUSE_SSAO -DUSE_GODRAYS -DUSE_TONEMAP" gamma_ssao_godrays_tm_frag_spv
compile frag "gamma.frag -DUSE_FXAA -DUSE_SSAO -DUSE_GODRAYS -DUSE_TONEMAP -DUSE_COLOR_GRADING" gamma_ultimate_frag_spv

echo "==> Compiling lighting shader variations..."
compile vert "light_vert.tmpl" vert_light
compile vert "light_vert.tmpl -DUSE_FOG" vert_light_fog
compile frag "light_frag.tmpl" frag_light
compile frag "light_frag.tmpl -DUSE_FOG" frag_light_fog
compile frag "light_frag.tmpl -DUSE_LINE" frag_light_line
compile frag "light_frag.tmpl -DUSE_LINE -DUSE_FOG" frag_light_line_fog
# parallax mapping variants
compile vert "light_vert.tmpl -DUSE_PARALLAX" vert_light_parallax
compile vert "light_vert.tmpl -DUSE_PARALLAX -DUSE_FOG" vert_light_parallax_fog
compile frag "light_frag.tmpl -DUSE_PARALLAX" frag_light_parallax
compile frag "light_frag.tmpl -DUSE_PARALLAX -DUSE_FOG" frag_light_parallax_fog
compile frag "light_frag.tmpl -DUSE_PARALLAX -DUSE_LINE" frag_light_parallax_line
compile frag "light_frag.tmpl -DUSE_PARALLAX -DUSE_LINE -DUSE_FOG" frag_light_parallax_line_fog
# shadow mapping variants
compile vert "light_vert.tmpl -DUSE_SHADOWMAP" vert_light_shadow
compile vert "light_vert.tmpl -DUSE_SHADOWMAP -DUSE_FOG" vert_light_shadow_fog
compile frag "light_frag.tmpl -DUSE_SHADOWMAP" frag_light_shadow
compile frag "light_frag.tmpl -DUSE_SHADOWMAP -DUSE_FOG" frag_light_shadow_fog
compile frag "light_frag.tmpl -DUSE_SHADOWMAP -DUSE_LINE" frag_light_shadow_line
compile frag "light_frag.tmpl -DUSE_SHADOWMAP -DUSE_LINE -DUSE_FOG" frag_light_shadow_line_fog
# PBR material variants
compile frag "light_frag.tmpl -DUSE_PBR" frag_light_pbr
compile frag "light_frag.tmpl -DUSE_PBR -DUSE_FOG" frag_light_pbr_fog
compile frag "light_frag.tmpl -DUSE_PBR -DUSE_LINE" frag_light_pbr_line
compile frag "light_frag.tmpl -DUSE_PBR -DUSE_LINE -DUSE_FOG" frag_light_pbr_line_fog

echo "==> Compiling generic vertex shaders..."
# single-texture vertex
compile vert "gen_vert.tmpl" vert_tx0
compile vert "gen_vert.tmpl -DUSE_FOG" vert_tx0_fog
compile vert "gen_vert.tmpl -DUSE_ENV" vert_tx0_env
compile vert "gen_vert.tmpl -DUSE_FOG -DUSE_ENV" vert_tx0_env_fog
# single-texture vertex, identity colors
compile vert "gen_vert.tmpl -DUSE_CLX_IDENT" vert_tx0_ident1
compile vert "gen_vert.tmpl -DUSE_CLX_IDENT -DUSE_FOG" vert_tx0_ident1_fog
compile vert "gen_vert.tmpl -DUSE_CLX_IDENT -DUSE_ENV" vert_tx0_ident1_env
compile vert "gen_vert.tmpl -DUSE_CLX_IDENT -DUSE_FOG -DUSE_ENV" vert_tx0_ident1_env_fog
# single-texture vertex, fixed colors
compile vert "gen_vert.tmpl -DUSE_FIXED_COLOR" vert_tx0_fixed
compile vert "gen_vert.tmpl -DUSE_FIXED_COLOR -DUSE_FOG" vert_tx0_fixed_fog
compile vert "gen_vert.tmpl -DUSE_FIXED_COLOR -DUSE_ENV" vert_tx0_fixed_env
compile vert "gen_vert.tmpl -DUSE_FIXED_COLOR -DUSE_FOG -DUSE_ENV" vert_tx0_fixed_env_fog
# double-texture vertex
compile vert "gen_vert.tmpl -DUSE_TX1" vert_tx1
compile vert "gen_vert.tmpl -DUSE_TX1 -DUSE_FOG" vert_tx1_fog
compile vert "gen_vert.tmpl -DUSE_TX1 -DUSE_ENV" vert_tx1_env
compile vert "gen_vert.tmpl -DUSE_TX1 -DUSE_FOG -DUSE_ENV" vert_tx1_env_fog
# double-texture vertex, identity colors
compile vert "gen_vert.tmpl -DUSE_CLX_IDENT -DUSE_TX1" vert_tx1_ident1
compile vert "gen_vert.tmpl -DUSE_CLX_IDENT -DUSE_TX1 -DUSE_FOG" vert_tx1_ident1_fog
compile vert "gen_vert.tmpl -DUSE_CLX_IDENT -DUSE_TX1 -DUSE_ENV" vert_tx1_ident1_env
compile vert "gen_vert.tmpl -DUSE_CLX_IDENT -DUSE_TX1 -DUSE_FOG -DUSE_ENV" vert_tx1_ident1_env_fog
# double-texture vertex, fixed colors
compile vert "gen_vert.tmpl -DUSE_FIXED_COLOR -DUSE_TX1" vert_tx1_fixed
compile vert "gen_vert.tmpl -DUSE_FIXED_COLOR -DUSE_TX1 -DUSE_FOG" vert_tx1_fixed_fog
compile vert "gen_vert.tmpl -DUSE_FIXED_COLOR -DUSE_TX1 -DUSE_ENV" vert_tx1_fixed_env
compile vert "gen_vert.tmpl -DUSE_FIXED_COLOR -DUSE_TX1 -DUSE_FOG -DUSE_ENV" vert_tx1_fixed_env_fog
# double-texture vertex, non-identical colors
compile vert "gen_vert.tmpl -DUSE_CL1 -DUSE_TX1" vert_tx1_cl
compile vert "gen_vert.tmpl -DUSE_CL1 -DUSE_TX1 -DUSE_FOG" vert_tx1_cl_fog
compile vert "gen_vert.tmpl -DUSE_CL1 -DUSE_TX1 -DUSE_ENV" vert_tx1_cl_env
compile vert "gen_vert.tmpl -DUSE_CL1 -DUSE_TX1 -DUSE_ENV -DUSE_FOG" vert_tx1_cl_env_fog
# triple-texture vertex
compile vert "gen_vert.tmpl -DUSE_TX2" vert_tx2
compile vert "gen_vert.tmpl -DUSE_TX2 -DUSE_FOG" vert_tx2_fog
compile vert "gen_vert.tmpl -DUSE_TX2 -DUSE_ENV" vert_tx2_env
compile vert "gen_vert.tmpl -DUSE_TX2 -DUSE_ENV -DUSE_FOG" vert_tx2_env_fog
# triple-texture vertex, non-identical colors
compile vert "gen_vert.tmpl -DUSE_CL2 -DUSE_TX2" vert_tx2_cl
compile vert "gen_vert.tmpl -DUSE_CL2 -DUSE_TX2 -DUSE_FOG" vert_tx2_cl_fog
compile vert "gen_vert.tmpl -DUSE_CL2 -DUSE_TX2 -DUSE_ENV" vert_tx2_cl_env
compile vert "gen_vert.tmpl -DUSE_CL2 -DUSE_TX2 -DUSE_ENV -DUSE_FOG" vert_tx2_cl_env_fog

echo "==> Compiling generic fragment shaders..."
# single-texture fragment, generic
compile frag "gen_frag.tmpl -DUSE_ATEST" frag_tx0
compile frag "gen_frag.tmpl -DUSE_ATEST -DUSE_FOG" frag_tx0_fog
# single-texture fragment, identity color
compile frag "gen_frag.tmpl -DUSE_CLX_IDENT -DUSE_ATEST" frag_tx0_ident1
compile frag "gen_frag.tmpl -DUSE_CLX_IDENT -DUSE_ATEST -DUSE_FOG" frag_tx0_ident1_fog
# single-texture fragment, fixed color
compile frag "gen_frag.tmpl -DUSE_FIXED_COLOR -DUSE_ATEST" frag_tx0_fixed
compile frag "gen_frag.tmpl -DUSE_FIXED_COLOR -DUSE_ATEST -DUSE_FOG" frag_tx0_fixed_fog
# single-texture fragment, entity color
compile frag "gen_frag.tmpl -DUSE_ENT_COLOR -DUSE_ATEST" frag_tx0_ent
compile frag "gen_frag.tmpl -DUSE_ENT_COLOR -DUSE_ATEST -DUSE_FOG" frag_tx0_ent_fog
# single-texture fragment, depth-fragment
compile frag "gen_frag.tmpl -DUSE_CLX_IDENT -DUSE_ATEST -DUSE_DF" frag_tx0_df
# double-texture fragment
compile frag "gen_frag.tmpl -DUSE_TX1" frag_tx1
compile frag "gen_frag.tmpl -DUSE_TX1 -DUSE_FOG" frag_tx1_fog
# double-texture fragment, identity colors
compile frag "gen_frag.tmpl -DUSE_CLX_IDENT -DUSE_TX1" frag_tx1_ident1
compile frag "gen_frag.tmpl -DUSE_CLX_IDENT -DUSE_TX1 -DUSE_FOG" frag_tx1_ident1_fog
# double-texture fragment, fixed colors
compile frag "gen_frag.tmpl -DUSE_FIXED_COLOR -DUSE_TX1" frag_tx1_fixed
compile frag "gen_frag.tmpl -DUSE_FIXED_COLOR -DUSE_TX1 -DUSE_FOG" frag_tx1_fixed_fog
# double-texture fragment, non-identical colors
compile frag "gen_frag.tmpl -DUSE_CL1 -DUSE_TX1" frag_tx1_cl
compile frag "gen_frag.tmpl -DUSE_CL1 -DUSE_TX1 -DUSE_FOG" frag_tx1_cl_fog
# triple-texture fragment
compile frag "gen_frag.tmpl -DUSE_TX2" frag_tx2
compile frag "gen_frag.tmpl -DUSE_TX2 -DUSE_FOG" frag_tx2_fog
# triple-texture fragment, non-identical colors
compile frag "gen_frag.tmpl -DUSE_CL2 -DUSE_TX2" frag_tx2_cl
compile frag "gen_frag.tmpl -DUSE_CL2 -DUSE_TX2 -DUSE_FOG" frag_tx2_cl_fog

echo "==> Compiling depth fade fragment shaders..."
# depth fade variants for soft particles (single-texture only)
compile frag "gen_frag.tmpl -DUSE_DEPTH_FADE -DUSE_ATEST" frag_tx0_dfade
compile frag "gen_frag.tmpl -DUSE_DEPTH_FADE -DUSE_ATEST -DUSE_FOG" frag_tx0_dfade_fog
compile frag "gen_frag.tmpl -DUSE_DEPTH_FADE -DUSE_CLX_IDENT -DUSE_ATEST" frag_tx0_ident1_dfade
compile frag "gen_frag.tmpl -DUSE_DEPTH_FADE -DUSE_CLX_IDENT -DUSE_ATEST -DUSE_FOG" frag_tx0_ident1_dfade_fog
compile frag "gen_frag.tmpl -DUSE_DEPTH_FADE -DUSE_FIXED_COLOR -DUSE_ATEST" frag_tx0_fixed_dfade
compile frag "gen_frag.tmpl -DUSE_DEPTH_FADE -DUSE_FIXED_COLOR -DUSE_ATEST -DUSE_FOG" frag_tx0_fixed_dfade_fog
compile frag "gen_frag.tmpl -DUSE_DEPTH_FADE -DUSE_ENT_COLOR -DUSE_ATEST" frag_tx0_ent_dfade
compile frag "gen_frag.tmpl -DUSE_DEPTH_FADE -DUSE_ENT_COLOR -DUSE_ATEST -DUSE_FOG" frag_tx0_ent_dfade_fog

echo "==> Compiling SMAA shaders..."
compile vert "smaa_edge.vert" smaa_edge_vert_spv
compile frag "smaa_edge.frag" smaa_edge_frag_spv
compile vert "smaa_blend.vert" smaa_blend_vert_spv
compile frag "smaa_blend.frag" smaa_blend_frag_spv
compile vert "smaa_resolve.vert" smaa_resolve_vert_spv
compile frag "smaa_resolve.frag" smaa_resolve_frag_spv

echo "==> Compiling rail trail compute + render shaders..."
# compile with rail_common.glsl prepended (shared SSBO struct definitions)
compile_rail() {
    cat rail_common.glsl "$2" > "$DIR/spirv/_rail_tmp.$1" && \
    "$CL" -S "$1" -V -o "$TMPF" "$DIR/spirv/_rail_tmp.$1" && \
    "$BH" "$TMPF" $OUTF "$3" && \
    rm -f "$TMPF" "$DIR/spirv/_rail_tmp.$1"
}
compile_rail comp rail_helix.comp rail_helix_comp_spv
compile_rail comp rail_debris.comp rail_debris_comp_spv
compile_rail comp rail_sparks.comp rail_sparks_comp_spv
compile_rail vert rail_helix.vert rail_helix_vert_spv
compile_rail frag rail_helix.frag rail_helix_frag_spv

echo "==> Compiling IQM GPU skinning shaders..."
compile vert iqm_skinning.vert iqm_skinning_vert_spv
compile frag iqm_skinning.frag iqm_skinning_frag_spv

rm -f "$TMPF"
echo "==> Done. shader_data.c regenerated."
