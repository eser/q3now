uniform sampler2D u_DiffuseMap;
uniform vec4      u_Color;

varying vec2      var_Tex1;
varying vec4      var_Color;

float median(float r, float g, float b) {
    return max(min(r, g), min(max(r, g), b));
}

void main()
{
    vec3 msd = texture2D(u_DiffuseMap, var_Tex1).rgb;
    float sd = median(msd.r, msd.g, msd.b);

    // u_Color.xyz = (distanceRange, atlasWidth, atlasHeight) repurposed as MSDF params
    vec2 unitRange = vec2(u_Color.x) / vec2(u_Color.y, u_Color.z);
    vec2 screenTexSize = vec2(1.0) / fwidth(var_Tex1);
    float screenPxRange = max(0.5 * dot(unitRange, screenTexSize), 1.0);

    float screenPxDistance = screenPxRange * (sd - 0.5);
    float opacity = clamp(screenPxDistance + 0.5, 0.0, 1.0);

    gl_FragColor = vec4(var_Color.rgb, var_Color.a * opacity);
}
