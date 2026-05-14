// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

uniform sampler2D u_DiffuseMap;
// u_Color.xyz = (distanceRange, atlasWidth, atlasHeight)
uniform vec4      u_Color;
uniform float     u_MsdfOutlineWidth;
uniform vec4      u_MsdfOutlineColor;
uniform float     u_MsdfGlowWidth;
uniform vec4      u_MsdfGlowColor;

varying vec2      var_Tex1;
varying vec4      var_Color;

float median(float r, float g, float b) {
    return max(min(r, g), min(max(r, g), b));
}

void main()
{
    vec3  msd = texture2D(u_DiffuseMap, var_Tex1).rgb;
    float sd  = median(msd.r, msd.g, msd.b);

    vec2  unitRange = vec2(u_Color.x) / vec2(u_Color.y, u_Color.z);
    vec2  screenTex = vec2(1.0) / fwidth(var_Tex1);
    float spr       = max(0.5 * dot(unitRange, screenTex), 1.0);

    float sd05  = sd - 0.5;
    float fillA = clamp(spr * sd05 + 0.5, 0.0, 1.0) * var_Color.a;
    float outA  = clamp(spr * (sd05 + u_MsdfOutlineWidth)  + 0.5, 0.0, 1.0) * u_MsdfOutlineColor.a;
    float glowRaw = clamp(spr * (sd05 + u_MsdfOutlineWidth + u_MsdfGlowWidth) + 0.5, 0.0, 1.0);
    float glowA = glowRaw * u_MsdfGlowColor.a * (1.0 - outA);

    // back-to-front composite: glow -> outline -> fill
    vec3  col = u_MsdfGlowColor.rgb * glowA;
    float a   = glowA;
    col = col * (1.0 - outA) + u_MsdfOutlineColor.rgb * outA;
    a   = a   * (1.0 - outA) + outA;
    col = col * (1.0 - fillA) + var_Color.rgb * fillA;
    a   = a   * (1.0 - fillA) + fillA;

    gl_FragColor = vec4(col, a);
}
