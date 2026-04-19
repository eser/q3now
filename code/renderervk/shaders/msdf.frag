#version 450

layout(set = 1, binding = 0) uniform sampler2D texture0;

// Push constants shared with vertex shader (mat4 mvp occupies offset 0-63)
// Layout satisfies std430: vec4 requires 16-byte alignment.
// Floats packed first, then vec4s at 16-byte-aligned offsets.
layout(push_constant) uniform PushConstants {
    layout(offset = 64)  float outlineWidth;   // 0.0 = no outline, SDF units
    layout(offset = 68)  float glowWidth;      // 0.0 = no glow, SDF units
    layout(offset = 72)  vec2  shadowOffset;   // shadow shift in atlas pixels (0,0 = no shadow)
    layout(offset = 80)  vec4  outlineColor;   // RGBA
    layout(offset = 96)  vec4  glowColor;      // RGBA
    layout(offset = 112) vec4  shadowColor;    // RGBA (a=0 disables shadow — branchless no-op)
} pc;

layout(location = 0) in vec4 frag_color0;
layout(location = 1) in vec2 frag_tex_coord0;

layout(location = 0) out vec4 out_color;

// MSDF distance range passed via specialization constant
layout (constant_id = 0) const float msdf_distance_range = 8.0;

float median(float r, float g, float b) {
    return max(min(r, g), min(max(r, g), b));
}

void main() {
    vec3 msd = texture(texture0, frag_tex_coord0).rgb;
    float sd = median(msd.r, msd.g, msd.b);

    // screenPxRange: official msdf-atlas-gen formula
    vec2 unitRange = vec2(msdf_distance_range) / vec2(textureSize(texture0, 0));
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
    vec2 shadowUV  = frag_tex_coord0 - pc.shadowOffset / vec2(textureSize(texture0, 0));
    vec3 shadowMsd = texture(texture0, shadowUV).rgb;
    float shadowSd = median(shadowMsd.r, shadowMsd.g, shadowMsd.b);
    float shadowAlpha = clamp(screenPxRange * (shadowSd - 0.5) + 0.5, 0.0, 1.0) * pc.shadowColor.a;

    // Composite: shadow (backmost) -> glow -> outline -> fill (front)
    vec4 fill    = vec4(frag_color0.rgb, frag_color0.a * fillAlpha);
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
