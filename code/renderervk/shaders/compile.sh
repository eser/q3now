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
    name="${f%.vert}_vert_spv"
    compile vert "$f" "$name"
done
for f in *.frag; do
    [ -f "$f" ] || continue
    name="${f%.frag}_frag_spv"
    compile frag "$f" "$name"
done

echo "==> Compiling lighting shader variations..."
compile vert "light_vert.tmpl" vert_light
compile vert "light_vert.tmpl -DUSE_FOG" vert_light_fog
compile frag "light_frag.tmpl" frag_light
compile frag "light_frag.tmpl -DUSE_FOG" frag_light_fog
compile frag "light_frag.tmpl -DUSE_LINE" frag_light_line
compile frag "light_frag.tmpl -DUSE_LINE -DUSE_FOG" frag_light_line_fog

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

rm -f "$TMPF"
echo "==> Done. shader_data.c regenerated."
