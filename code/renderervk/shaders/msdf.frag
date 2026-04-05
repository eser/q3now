#version 450

layout(set = 1, binding = 0) uniform sampler2D texture0;

layout(location = 0) in vec4 frag_color0;
layout(location = 1) centroid in vec2 frag_tex_coord0;

layout(location = 0) out vec4 out_color;

// MSDF distance range passed via specialization constant
layout (constant_id = 0) const float msdf_distance_range = 4.0;

float median(float r, float g, float b) {
    return max(min(r, g), min(max(r, g), b));
}

void main() {
    vec3 msd = texture(texture0, frag_tex_coord0).rgb;
    float sd = median(msd.r, msd.g, msd.b);

    // Compute screen-space pixel range using fwidth for precise antialiasing
    vec2 msdfUnit = vec2(msdf_distance_range) / vec2(textureSize(texture0, 0));
    vec2 screenTexSize = vec2(1.0) / fwidth(frag_tex_coord0);
    float screenPxRange = max(0.5 * dot(msdfUnit, screenTexSize), 1.0);

    float screenPxDistance = screenPxRange * (sd - 0.5);
    float opacity = clamp(screenPxDistance + 0.5, 0.0, 1.0);

    out_color = vec4(frag_color0.rgb, frag_color0.a * opacity);
}
