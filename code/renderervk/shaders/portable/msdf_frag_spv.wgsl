enable wgpu_binding_array;

struct MsdfUBO {
    mvp: mat4x4<f32>,
    outlineWidth: f32,
    glowWidth: f32,
    shadowOffset: vec2<f32>,
    outlineColor: vec4<f32>,
    glowColor: vec4<f32>,
    shadowColor: vec4<f32>,
    bindless_packed_slot: u32,
}

@id(0) override msdf_distance_range: f32 = 8f;

var<private> frag_color0_1: vec4<f32>;
@group(1) @binding(0) 
var wired_bindless_images: binding_array<texture_2d<f32>>;
@group(3) @binding(0) 
var<uniform> pc: MsdfUBO;
@group(1) @binding(1) 
var wired_bindless_samplers: binding_array<sampler>;
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> out_color: vec4<f32>;

fn median_u0028_f1_u003b_f1_u003b_f1_u003b(r: ptr<function, f32>, g: ptr<function, f32>, b: ptr<function, f32>) -> f32 {
    let _e39 = (*r);
    let _e40 = (*g);
    let _e42 = (*r);
    let _e43 = (*g);
    let _e45 = (*b);
    return max(min(_e39, _e40), min(max(_e42, _e43), _e45));
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e40 = (*c);
    (*c) = max(_e40, vec3<f32>(0f, 0f, 0f));
    let _e42 = (*c);
    cutoff = (_e42 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e44 = (*c);
    lo = (_e44 / vec3(12.92f));
    let _e47 = (*c);
    hi = pow(((_e47 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e52 = hi;
    let _e53 = lo;
    let _e54 = cutoff;
    return mix(_e52, _e53, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e54));
}

fn main_1() {
    var fillColor: vec3<f32>;
    var param: vec3<f32>;
    var msd: vec3<f32>;
    var sd: f32;
    var param_1: f32;
    var param_2: f32;
    var param_3: f32;
    var atlasSize: vec2<f32>;
    var unitRange: vec2<f32>;
    var screenTexSize: vec2<f32>;
    var screenPxRange: f32;
    var fillDist: f32;
    var outlineDist: f32;
    var glowDist: f32;
    var fillAlpha: f32;
    var outlineAlpha: f32;
    var glowAlpha: f32;
    var shadowUV: vec2<f32>;
    var shadowMsd: vec3<f32>;
    var shadowSd: f32;
    var param_4: f32;
    var param_5: f32;
    var param_6: f32;
    var shadowAlpha: f32;
    var fill: vec4<f32>;
    var outline: vec4<f32>;
    var glow: vec4<f32>;
    var shadow: vec4<f32>;
    var col: vec3<f32>;
    var a: f32;

    let _e66 = frag_color0_1;
    param = _e66.xyz;
    let _e68 = sRGBToLinear_u0028_vf3_u003b((&param));
    fillColor = _e68;
    let _e70 = pc.bindless_packed_slot;
    let _e74 = pc.bindless_packed_slot;
    let _e79 = frag_tex_coord0_1;
    let _e80 = textureSample(wired_bindless_images[(_e70 & 4095u)], wired_bindless_samplers[((_e74 >> bitcast<u32>(12i)) & 255u)], _e79);
    msd = _e80.xyz;
    let _e83 = msd[0u];
    param_1 = _e83;
    let _e85 = msd[1u];
    param_2 = _e85;
    let _e87 = msd[2u];
    param_3 = _e87;
    let _e88 = median_u0028_f1_u003b_f1_u003b_f1_u003b((&param_1), (&param_2), (&param_3));
    sd = _e88;
    let _e90 = pc.bindless_packed_slot;
    let _e94 = pc.bindless_packed_slot;
    let _e99 = textureDimensions(wired_bindless_images[(_e90 & 4095u)], 0i);
    atlasSize = vec2<f32>(vec2<i32>(_e99));
    let _e103 = atlasSize[0u];
    unitRange[0u] = (msdf_distance_range / _e103);
    let _e107 = atlasSize[1u];
    unitRange[1u] = (msdf_distance_range / _e107);
    let _e110 = frag_tex_coord0_1;
    let _e111 = fwidth(_e110);
    screenTexSize = (vec2<f32>(1f, 1f) / _e111);
    let _e113 = unitRange;
    let _e114 = screenTexSize;
    screenPxRange = max((0.5f * dot(_e113, _e114)), 1f);
    let _e118 = screenPxRange;
    let _e119 = sd;
    fillDist = (_e118 * (_e119 - 0.5f));
    let _e122 = screenPxRange;
    let _e123 = sd;
    let _e126 = pc.outlineWidth;
    outlineDist = (_e122 * ((_e123 - 0.5f) + _e126));
    let _e129 = screenPxRange;
    let _e130 = sd;
    let _e133 = pc.outlineWidth;
    let _e136 = pc.glowWidth;
    glowDist = (_e129 * (((_e130 - 0.5f) + _e133) + _e136));
    let _e139 = fillDist;
    fillAlpha = clamp((_e139 + 0.5f), 0f, 1f);
    let _e142 = outlineDist;
    outlineAlpha = clamp((_e142 + 0.5f), 0f, 1f);
    let _e145 = glowDist;
    glowAlpha = clamp((_e145 + 0.5f), 0f, 1f);
    let _e148 = frag_tex_coord0_1;
    let _e150 = pc.shadowOffset;
    let _e152 = pc.bindless_packed_slot;
    let _e156 = pc.bindless_packed_slot;
    let _e161 = textureDimensions(wired_bindless_images[(_e152 & 4095u)], 0i);
    shadowUV = (_e148 - (_e150 / vec2<f32>(vec2<i32>(_e161))));
    let _e167 = pc.bindless_packed_slot;
    let _e171 = pc.bindless_packed_slot;
    let _e176 = shadowUV;
    let _e177 = textureSample(wired_bindless_images[(_e167 & 4095u)], wired_bindless_samplers[((_e171 >> bitcast<u32>(12i)) & 255u)], _e176);
    shadowMsd = _e177.xyz;
    let _e180 = shadowMsd[0u];
    param_4 = _e180;
    let _e182 = shadowMsd[1u];
    param_5 = _e182;
    let _e184 = shadowMsd[2u];
    param_6 = _e184;
    let _e185 = median_u0028_f1_u003b_f1_u003b_f1_u003b((&param_4), (&param_5), (&param_6));
    shadowSd = _e185;
    let _e186 = screenPxRange;
    let _e187 = shadowSd;
    let _e194 = pc.shadowColor[3u];
    shadowAlpha = (clamp(((_e186 * (_e187 - 0.5f)) + 0.5f), 0f, 1f) * _e194);
    let _e196 = fillColor;
    let _e198 = frag_color0_1[3u];
    let _e199 = fillAlpha;
    fill = vec4<f32>(_e196.x, _e196.y, _e196.z, (_e198 * _e199));
    let _e206 = pc.outlineColor;
    let _e207 = _e206.xyz;
    let _e210 = pc.outlineColor[3u];
    let _e211 = outlineAlpha;
    outline = vec4<f32>(_e207.x, _e207.y, _e207.z, (_e210 * _e211));
    let _e218 = pc.glowColor;
    let _e219 = _e218.xyz;
    let _e222 = pc.glowColor[3u];
    let _e223 = glowAlpha;
    let _e225 = outlineAlpha;
    glow = vec4<f32>(_e219.x, _e219.y, _e219.z, ((_e222 * _e223) * (1f - _e225)));
    let _e233 = pc.shadowColor;
    let _e234 = _e233.xyz;
    let _e235 = shadowAlpha;
    shadow = vec4<f32>(_e234.x, _e234.y, _e234.z, _e235);
    let _e240 = shadow;
    let _e243 = shadow[3u];
    col = (_e240.xyz * _e243);
    let _e246 = shadow[3u];
    a = _e246;
    let _e247 = col;
    let _e249 = glow[3u];
    let _e252 = glow;
    let _e255 = glow[3u];
    col = ((_e247 * (1f - _e249)) + (_e252.xyz * _e255));
    let _e258 = a;
    let _e260 = glow[3u];
    let _e264 = glow[3u];
    a = ((_e258 * (1f - _e260)) + _e264);
    let _e266 = col;
    let _e268 = outline[3u];
    let _e271 = outline;
    let _e274 = outline[3u];
    col = ((_e266 * (1f - _e268)) + (_e271.xyz * _e274));
    let _e277 = a;
    let _e279 = outline[3u];
    let _e283 = outline[3u];
    a = ((_e277 * (1f - _e279)) + _e283);
    let _e285 = col;
    let _e287 = fill[3u];
    let _e290 = fill;
    let _e293 = fill[3u];
    col = ((_e285 * (1f - _e287)) + (_e290.xyz * _e293));
    let _e296 = a;
    let _e298 = fill[3u];
    let _e302 = fill[3u];
    a = ((_e296 * (1f - _e298)) + _e302);
    let _e304 = col;
    let _e305 = a;
    out_color = vec4<f32>(_e304.x, _e304.y, _e304.z, _e305);
    return;
}

@fragment 
fn main(@location(0) frag_color0_: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>) -> @location(0) vec4<f32> {
    frag_color0_1 = frag_color0_;
    frag_tex_coord0_1 = frag_tex_coord0_;
    main_1();
    let _e5 = out_color;
    return _e5;
}
