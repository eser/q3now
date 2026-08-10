// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#version 450

// legacy-mainpath-retire STEP 6.1 — MSDF was sampling its atlas via a legacy
// set=1 binding fed by the GL_Bind ring write at tr_backend.c:85. 6.1 routes
// the atlas through the RAL-owned bindless 2D table (set 7 of
// vk.pipeline_layout_msdf) and adds a small FS push range at offset 128 that
// carries the atlas's packed (tex<<12|sampler) slot. The new push range
// extends the existing 128-byte MSDF push by 4 bytes — total 132 bytes,
// within typical desktop GPU maxPushConstantsSize (>=256). The GL_Bind ring
// write becomes dead and is removed at tr_backend.c:85 in the same turn.
#extension GL_EXT_nonuniform_qualifier : require

layout(set = 1, binding = 0) uniform texture2D wired_bindless_images[];
layout(set = 1, binding = 1) uniform sampler   wired_bindless_samplers[];

// Per-draw MSDF UBO at set 3 (UNIFORM_BUFFER_DYNAMIC, std140) — migrated off the
// former 132-byte push constant (which exceeded the 128-byte Vulkan floor). Field
// order/offsets match vk_msdf_ubo_t and msdf.vert's block exactly: mvp@0,
// outlineWidth@64, glowWidth@68, shadowOffset@72, outlineColor@80, glowColor@96,
// shadowColor@112, bindless_packed_slot@128 (carries WIRED_BINDLESS_PACK(atlas
// tex, sampler) for the current text draw's atlas). The instance stays named `pc`
// so the read sites below are unchanged. The fragment stage reads everything but
// mvp (read by msdf.vert); mvp is declared here for a matching block layout.
layout(set = 3, binding = 0, std140) uniform MsdfUBO {
    mat4  mvp;
    float outlineWidth;   // 0.0 = no outline, SDF units
    float glowWidth;      // 0.0 = no glow, SDF units
    vec2  shadowOffset;   // shadow shift in atlas pixels (0,0 = no shadow)
    vec4  outlineColor;   // RGBA
    vec4  glowColor;      // RGBA
    vec4  shadowColor;    // RGBA (a=0 disables shadow — branchless no-op)
    uint  bindless_packed_slot;  // atlas slot (tex:12 | sampler:8)
} pc;

// Sample the atlas via the bindless table. The atlas slot is constant for the
// whole draw (one atlas per text string), so a per-fragment uniform read of
// pc.bindless_packed_slot is folded by the driver to a constant.
#define WIRED_MSDF_ATLAS sampler2D( \
    wired_bindless_images  [ nonuniformEXT(   pc.bindless_packed_slot         & 0xFFFu ) ], \
    wired_bindless_samplers[ nonuniformEXT( ( pc.bindless_packed_slot >> 12 ) & 0xFFu  ) ] )

layout(location = 0) in vec4 frag_color0;
layout(location = 1) in vec2 frag_tex_coord0;

layout(location = 0) out vec4 out_color;

// MSDF distance range passed via specialization constant
layout (constant_id = 0) const float msdf_distance_range = 8.0;

float median(float r, float g, float b) {
    return max(min(r, g), min(max(r, g), b));
}

// precise piecewise sRGB <-> linear conversion.
// Duplicated in every fragment shader per the engine-wide
// unconditional linear migration; compile.mjs lacks #include
// support. Matches color.frag verbatim. linearToSRGB is
// unused here — driver DCEs it; kept for migration symmetry.
vec3 sRGBToLinear( vec3 c ) {
    c = max( c, vec3( 0.0 ) );
    bvec3 cutoff = lessThanEqual( c, vec3( 0.04045 ) );
    vec3 lo = c / 12.92;
    vec3 hi = pow( ( c + vec3( 0.055 ) ) / 1.055, vec3( 2.4 ) );
    return mix( hi, lo, vec3( cutoff ) );
}

vec3 linearToSRGB( vec3 c ) {
    c = max( c, vec3( 0.0 ) );
    bvec3 cutoff = lessThanEqual( c, vec3( 0.0031308 ) );
    vec3 lo = c * 12.92;
    vec3 hi = pow( c, vec3( 1.0 / 2.4 ) ) * 1.055 - 0.055;
    return mix( hi, lo, vec3( cutoff ) );
}

void main() {
    // colour-domain contract for this shader —
    //   * texture0 is the MSDF distance field (signed-distance data
    //     in RGB, not a colour) — sampled RAW, never sRGB-decoded.
    //   * frag_color0 is the text tint, a normalised sRGB-encoded
    //     byte vertex attribute — decoded to linear below.
    //   * pc.outlineColor / glowColor / shadowColor arrive already
    //     linear (decoded host-side in vk_update_msdf_outline).
    //   * all alphas stay raw (alpha is not sRGB-encoded).
    // The layered composite (col = col*(1-a) + layer*a) is only
    // correct with every colour term in the linear domain.
    vec3 fillColor = sRGBToLinear( frag_color0.rgb );

    vec3 msd = texture(WIRED_MSDF_ATLAS, frag_tex_coord0).rgb;
    float sd = median(msd.r, msd.g, msd.b);

    // screenPxRange: official msdf-atlas-gen formula
    vec2 unitRange = vec2(msdf_distance_range) / vec2(textureSize(WIRED_MSDF_ATLAS, 0));
    vec2 screenTexSize = vec2(1.0) / fwidth(frag_tex_coord0);
    float screenPxRange = max(0.5 * dot(unitRange, screenTexSize), 1.0);

    // Three layers: glow (outermost) -> outline -> fill (innermost)
    float fillDist    = screenPxRange * (sd - 0.5);
    float outlineDist = screenPxRange * (sd - 0.5 + pc.outlineWidth);
    float glowDist    = screenPxRange * (sd - 0.5 + pc.outlineWidth + pc.glowWidth);

    float fillAlpha    = clamp(fillDist + 0.5, 0.0, 1.0);
    float outlineAlpha = clamp(outlineDist + 0.5, 0.0, 1.0);
    float glowAlpha    = clamp(glowDist + 0.5, 0.0, 1.0);

    // Shadow layer: sample atlas shifted opposite to offset direction.
    // shadowColor.a == 0 makes the shadow term a no-op (branchless).
    vec2 shadowUV  = frag_tex_coord0 - pc.shadowOffset / vec2(textureSize(WIRED_MSDF_ATLAS, 0));
    vec3 shadowMsd = texture(WIRED_MSDF_ATLAS, shadowUV).rgb;
    float shadowSd = median(shadowMsd.r, shadowMsd.g, shadowMsd.b);
    float shadowAlpha = clamp(screenPxRange * (shadowSd - 0.5) + 0.5, 0.0, 1.0) * pc.shadowColor.a;

    // Composite: shadow (backmost) -> glow -> outline -> fill (front)
    vec4 fill    = vec4(fillColor, frag_color0.a * fillAlpha);
    vec4 outline = vec4(pc.outlineColor.rgb, pc.outlineColor.a * outlineAlpha);
    vec4 glow    = vec4(pc.glowColor.rgb, pc.glowColor.a * glowAlpha * (1.0 - outlineAlpha));
    vec4 shadow  = vec4(pc.shadowColor.rgb, shadowAlpha);

    // Layer compositing (back to front)
    vec3 col = shadow.rgb * shadow.a;
    float a  = shadow.a;

    col = col * (1.0 - glow.a) + glow.rgb * glow.a;
    a   = a   * (1.0 - glow.a) + glow.a;

    col = col * (1.0 - outline.a) + outline.rgb * outline.a;
    a   = a   * (1.0 - outline.a) + outline.a;

    col = col * (1.0 - fill.a) + fill.rgb * fill.a;
    a   = a   * (1.0 - fill.a) + fill.a;

    out_color = vec4(col, a);
}
