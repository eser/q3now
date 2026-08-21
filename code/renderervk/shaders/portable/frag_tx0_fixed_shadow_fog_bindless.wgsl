enable wgpu_binding_array;

struct UBO {
    eyePos: vec4<f32>,
    lightPos: vec4<f32>,
    lightColor: vec4<f32>,
    lightVector: vec4<f32>,
    fogDistanceVector: vec4<f32>,
    fogDepthVector: vec4<f32>,
    fogEyeT: vec4<f32>,
    fogColor: vec4<f32>,
    _pad_to_cascadeMVP: array<vec4<f32>, 1>,
    cascadeMVP: array<mat4x4<f32>, 4>,
    cascadeSplits: vec4<f32>,
    _pad_to_packed_indices: array<vec4<f32>, 8>,
    packed_indices: array<vec4<u32>, 3>,
    worldLightParams: vec4<f32>,
}

@id(4) override tex_domain: i32 = 0i;
@id(26) override lightmap_slot: i32 = 0i;
@id(29) override shadow_bias: f32 = 0.005f;
@id(28) override shadow_pcf: i32 = 5i;
override override_type_11_: bool = (shadow_pcf <= 1i);
override override_type_11_1: bool = (shadow_pcf <= 5i);
override override_type_11_2: bool = (lightmap_slot == 1i);
@id(8) override identity_color: f32 = 1f;
@id(9) override identity_alpha: f32 = 1f;
@id(10) override acff: i32 = 0i;
override override_type_11_3: bool = (acff == 1i);
override override_type_11_4: bool = (acff == 2i);
override override_type_11_5: bool = (acff == 3i);
@id(7) override discard_mode: i32 = 0i;
override override_type_11_6: bool = (discard_mode == 1i);
override override_type_11_7: bool = (discard_mode == 2i);
@id(3) override alpha_to_coverage: i32 = 0i;

@group(1) @binding(0) 
var wired_bindless_images: binding_array<texture_2d<f32>>;
@group(0) @binding(0) 
var<uniform> unnamed: UBO;
@group(1) @binding(1) 
var wired_bindless_samplers: binding_array<sampler>;
@group(2) @binding(0) 
var shadowMap: texture_2d_array<f32>;
@group(2) @binding(32) 
var shadowMap_sampler: sampler;
var<private> shadowData_1: vec4<f32>;
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> fog_tex_coord_1: vec2<f32>;
var<private> out_color: vec4<f32>;

fn sampleCascade_u0028_i1_u003b_vf3_u003b(c: ptr<function, i32>, worldPos: ptr<function, vec3<f32>>) -> f32 {
    var sc4_: vec4<f32>;
    var sc: vec3<f32>;
    var layer: f32;
    var texelSize: vec2<f32>;
    var currentDepth: f32;
    var shadow: f32;
    var x: i32;
    var y: i32;
    var phi_218_: bool;
    var phi_225_: bool;
    var phi_232_: bool;
    var phi_239_: bool;

    let _e72 = (*c);
    let _e75 = unnamed.cascadeMVP[_e72];
    let _e76 = (*worldPos);
    sc4_ = (_e75 * vec4<f32>(_e76.x, _e76.y, _e76.z, 1f));
    let _e82 = sc4_;
    let _e85 = sc4_[3u];
    sc = (_e82.xyz / vec3(_e85));
    let _e88 = sc;
    let _e92 = ((_e88.xy * 0.5f) + vec2(0.5f));
    sc[0u] = _e92.x;
    sc[1u] = _e92.y;
    let _e98 = sc[0u];
    let _e99 = (_e98 < 0f);
    phi_218_ = _e99;
    if !(_e99) {
        let _e102 = sc[0u];
        phi_218_ = (_e102 > 1f);
    }
    let _e105 = phi_218_;
    phi_225_ = _e105;
    if !(_e105) {
        let _e108 = sc[1u];
        phi_225_ = (_e108 < 0f);
    }
    let _e111 = phi_225_;
    phi_232_ = _e111;
    if !(_e111) {
        let _e114 = sc[1u];
        phi_232_ = (_e114 > 1f);
    }
    let _e117 = phi_232_;
    phi_239_ = _e117;
    if !(_e117) {
        let _e120 = sc[2u];
        phi_239_ = (_e120 > 1f);
    }
    let _e123 = phi_239_;
    if _e123 {
        return 1f;
    }
    let _e124 = (*c);
    layer = f32(_e124);
    let _e126 = textureDimensions(shadowMap, 0i);
    texelSize = (vec2(1f) / vec2<f32>(vec2<i32>(_e126).xy));
    let _e133 = sc[2u];
    currentDepth = (_e133 - shadow_bias);
    shadow = 0f;
    if override_type_11_ {
        let _e135 = currentDepth;
        let _e136 = sc;
        let _e137 = _e136.xy;
        let _e138 = layer;
        let _e141 = vec3<f32>(_e137.x, _e137.y, _e138);
        let _e147 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e141.x, _e141.y), i32(_e141.z));
        shadow = step(_e135, _e147.x);
    } else {
        if override_type_11_1 {
            let _e150 = currentDepth;
            let _e151 = sc;
            let _e152 = _e151.xy;
            let _e153 = layer;
            let _e156 = vec3<f32>(_e152.x, _e152.y, _e153);
            let _e162 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e156.x, _e156.y), i32(_e156.z));
            let _e165 = shadow;
            shadow = (_e165 + step(_e150, _e162.x));
            let _e167 = currentDepth;
            let _e168 = sc;
            let _e171 = texelSize[0u];
            let _e173 = (_e168.xy + vec2<f32>(_e171, 0f));
            let _e174 = layer;
            let _e177 = vec3<f32>(_e173.x, _e173.y, _e174);
            let _e183 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e177.x, _e177.y), i32(_e177.z));
            let _e186 = shadow;
            shadow = (_e186 + step(_e167, _e183.x));
            let _e188 = currentDepth;
            let _e189 = sc;
            let _e192 = texelSize[0u];
            let _e194 = (_e189.xy - vec2<f32>(_e192, 0f));
            let _e195 = layer;
            let _e198 = vec3<f32>(_e194.x, _e194.y, _e195);
            let _e204 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e198.x, _e198.y), i32(_e198.z));
            let _e207 = shadow;
            shadow = (_e207 + step(_e188, _e204.x));
            let _e209 = currentDepth;
            let _e210 = sc;
            let _e213 = texelSize[1u];
            let _e215 = (_e210.xy + vec2<f32>(0f, _e213));
            let _e216 = layer;
            let _e219 = vec3<f32>(_e215.x, _e215.y, _e216);
            let _e225 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e219.x, _e219.y), i32(_e219.z));
            let _e228 = shadow;
            shadow = (_e228 + step(_e209, _e225.x));
            let _e230 = currentDepth;
            let _e231 = sc;
            let _e234 = texelSize[1u];
            let _e236 = (_e231.xy - vec2<f32>(0f, _e234));
            let _e237 = layer;
            let _e240 = vec3<f32>(_e236.x, _e236.y, _e237);
            let _e246 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e240.x, _e240.y), i32(_e240.z));
            let _e249 = shadow;
            shadow = (_e249 + step(_e230, _e246.x));
            let _e251 = shadow;
            shadow = (_e251 / 5f);
        } else {
            x = -1i;
            loop {
                let _e253 = x;
                if (_e253 <= 1i) {
                    y = -1i;
                    loop {
                        let _e255 = y;
                        if (_e255 <= 1i) {
                            let _e257 = currentDepth;
                            let _e258 = sc;
                            let _e260 = x;
                            let _e262 = y;
                            let _e265 = texelSize;
                            let _e267 = (_e258.xy + (vec2<f32>(f32(_e260), f32(_e262)) * _e265));
                            let _e268 = layer;
                            let _e271 = vec3<f32>(_e267.x, _e267.y, _e268);
                            let _e277 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e271.x, _e271.y), i32(_e271.z));
                            let _e280 = shadow;
                            shadow = (_e280 + step(_e257, _e277.x));
                            continue;
                        } else {
                            break;
                        }
                        continuing {
                            let _e282 = y;
                            y = (_e282 + 1i);
                        }
                    }
                    continue;
                } else {
                    break;
                }
                continuing {
                    let _e284 = x;
                    x = (_e284 + 1i);
                }
            }
            let _e286 = shadow;
            shadow = (_e286 / 9f);
        }
    }
    let _e288 = shadow;
    return _e288;
}

fn sampleShadow_u0028_vf3_u003b_f1_u003b_i1_u003b(worldPos_1: ptr<function, vec3<f32>>, viewDepth: ptr<function, f32>, outCascade: ptr<function, i32>) -> f32 {
    var cmp: vec4<f32>;
    var cascade: i32;
    var prevSplit: f32;
    var local: f32;
    var farSplit: f32;
    var blendRange: f32;
    var blendT: f32;
    var s0_: f32;
    var param: i32;
    var param_1: vec3<f32>;
    var s1_: f32;
    var param_2: i32;
    var param_3: vec3<f32>;

    let _e79 = unnamed.cascadeSplits;
    let _e80 = (*viewDepth);
    cmp = step(_e79, vec4(_e80));
    let _e84 = cmp[0u];
    let _e86 = cmp[1u];
    let _e89 = cmp[2u];
    let _e92 = cmp[3u];
    cascade = min(i32((((_e84 + _e86) + _e89) + _e92)), 3i);
    let _e96 = cascade;
    (*outCascade) = _e96;
    let _e97 = cascade;
    if (_e97 == 0i) {
        local = 0f;
    } else {
        let _e99 = cascade;
        let _e104 = unnamed.cascadeSplits[max((_e99 - 1i), 0i)];
        local = _e104;
    }
    let _e105 = local;
    prevSplit = _e105;
    let _e106 = cascade;
    let _e109 = unnamed.cascadeSplits[_e106];
    farSplit = _e109;
    let _e110 = farSplit;
    let _e111 = prevSplit;
    blendRange = max((0.1f * (_e110 - _e111)), 1f);
    let _e115 = farSplit;
    let _e116 = (*viewDepth);
    let _e118 = blendRange;
    blendT = clamp(((_e115 - _e116) / _e118), 0f, 1f);
    let _e121 = cascade;
    param = _e121;
    let _e122 = (*worldPos_1);
    param_1 = _e122;
    let _e123 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param), (&param_1));
    s0_ = _e123;
    let _e124 = cascade;
    param_2 = min((_e124 + 1i), 3i);
    let _e127 = (*worldPos_1);
    param_3 = _e127;
    let _e128 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param_2), (&param_3));
    s1_ = _e128;
    let _e129 = s1_;
    let _e130 = s0_;
    let _e131 = blendT;
    return mix(_e129, _e130, _e131);
}

fn wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b(rgb: ptr<function, vec3<f32>>, lm_uv: ptr<function, vec2<f32>>) -> vec3<f32> {
    var sun_mask: f32;
    var shadow_1: f32;
    var ignoredCascade: i32;
    var param_4: vec3<f32>;
    var param_5: f32;
    var param_6: i32;

    let _e73 = unnamed.packed_indices[1i][3u];
    let _e79 = unnamed.packed_indices[1i][3u];
    let _e84 = (*lm_uv);
    let _e85 = textureSample(wired_bindless_images[(_e73 & 4095u)], wired_bindless_samplers[((_e79 >> bitcast<u32>(12i)) & 255u)], _e84);
    sun_mask = _e85.x;
    let _e87 = sun_mask;
    if (_e87 > 0.001f) {
        let _e89 = shadowData_1;
        param_4 = _e89.xyz;
        let _e92 = shadowData_1[3u];
        param_5 = _e92;
        let _e93 = sampleShadow_u0028_vf3_u003b_f1_u003b_i1_u003b((&param_4), (&param_5), (&param_6));
        let _e94 = param_6;
        ignoredCascade = _e94;
        shadow_1 = _e93;
        let _e95 = shadow_1;
        let _e96 = sun_mask;
        let _e98 = (*rgb);
        (*rgb) = (_e98 * mix(1f, _e95, _e96));
    }
    let _e100 = (*rgb);
    return _e100;
}

fn wired_apply_sun_shadow_operand_u0028_vf3_u003b(rgb_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var param_7: vec3<f32>;
    var param_8: vec2<f32>;

    if override_type_11_2 {
        let _e65 = (*rgb_1);
        param_7 = _e65;
        let _e66 = frag_tex_coord0_1;
        param_8 = _e66;
        let _e67 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_7), (&param_8));
        return _e67;
    }
    let _e68 = (*rgb_1);
    return _e68;
}

fn sRGBToLinear_u0028_vf3_u003b(c_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e66 = (*c_1);
    (*c_1) = max(_e66, vec3<f32>(0f, 0f, 0f));
    let _e68 = (*c_1);
    cutoff = (_e68 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e70 = (*c_1);
    lo = (_e70 / vec3(12.92f));
    let _e73 = (*c_1);
    hi = pow(((_e73 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e78 = hi;
    let _e79 = lo;
    let _e80 = cutoff;
    return mix(_e78, _e79, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e80));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_2: vec4<f32>;
    var param_9: vec3<f32>;

    let _e67 = (*role);
    let _e69 = (*role);
    let _e74 = unnamed.packed_indices[(_e67 / 4u)][(_e69 % 4u)];
    let _e77 = (*role);
    let _e79 = (*role);
    let _e84 = unnamed.packed_indices[(_e77 / 4u)][(_e79 % 4u)];
    let _e89 = (*uv);
    let _e90 = textureSample(wired_bindless_images[(_e74 & 4095u)], wired_bindless_samplers[((_e84 >> bitcast<u32>(12i)) & 255u)], _e89);
    c_2 = _e90;
    let _e91 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e91))) == 0i) {
        let _e96 = c_2;
        param_9 = _e96.xyz;
        let _e98 = sRGBToLinear_u0028_vf3_u003b((&param_9));
        c_2[0u] = _e98.x;
        c_2[1u] = _e98.y;
        c_2[2u] = _e98.z;
    }
    let _e105 = (*slot);
    if (lightmap_slot == (_e105 + 1i)) {
        let _e110 = unnamed.worldLightParams[0u];
        let _e111 = c_2;
        let _e113 = (_e111.xyz * _e110);
        c_2[0u] = _e113.x;
        c_2[1u] = _e113.y;
        c_2[2u] = _e113.z;
    }
    let _e120 = c_2;
    return _e120;
}

fn main_1() {
    var fog: vec4<f32>;
    var frag_color: vec4<f32>;
    var color0_: vec4<f32>;
    var param_10: u32;
    var param_11: vec2<f32>;
    var param_12: i32;
    var base: vec4<f32>;
    var param_13: vec3<f32>;

    let _e73 = unnamed.packed_indices[0i][3u];
    let _e79 = unnamed.packed_indices[0i][3u];
    let _e84 = fog_tex_coord_1;
    let _e85 = textureSample(wired_bindless_images[(_e73 & 4095u)], wired_bindless_samplers[((_e79 >> bitcast<u32>(12i)) & 255u)], _e84);
    fog = _e85;
    frag_color[0u] = identity_color;
    frag_color[1u] = identity_color;
    frag_color[2u] = identity_color;
    frag_color[3u] = identity_alpha;
    param_10 = 0u;
    let _e90 = frag_tex_coord0_1;
    param_11 = _e90;
    param_12 = 0i;
    let _e91 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_10), (&param_11), (&param_12));
    let _e92 = frag_color;
    color0_ = (_e91 * _e92);
    let _e94 = color0_;
    base = _e94;
    let _e95 = color0_;
    base = _e95;
    let _e96 = base;
    param_13 = _e96.xyz;
    let _e98 = wired_apply_sun_shadow_operand_u0028_vf3_u003b((&param_13));
    base[0u] = _e98.x;
    base[1u] = _e98.y;
    base[2u] = _e98.z;
    if override_type_11_3 {
        let _e105 = base;
        let _e108 = fog[3u];
        let _e110 = (_e105.xyz * (1f - _e108));
        base[0u] = _e110.x;
        base[1u] = _e110.y;
        base[2u] = _e110.z;
    } else {
        if override_type_11_4 {
            let _e117 = base;
            let _e119 = fog[3u];
            base = (_e117 * (1f - _e119));
        } else {
            if override_type_11_5 {
                let _e123 = base[3u];
                let _e125 = fog[3u];
                base[3u] = (_e123 * (1f - _e125));
            } else {
                let _e129 = base;
                let _e130 = fog;
                let _e132 = unnamed.fogColor;
                let _e135 = fog[3u];
                base = mix(_e129, (_e130 * _e132), vec4(_e135));
            }
        }
    }
    if override_type_11_6 {
        let _e139 = base[3u];
        if (_e139 == 0f) {
            discard;
        }
    } else {
        if override_type_11_7 {
            let _e141 = base;
            let _e143 = base;
            if (dot(_e141.xyz, _e143.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e147 = base;
    out_color = _e147;
    return;
}

@fragment 
fn main(@location(7) shadowData: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(4) fog_tex_coord: vec2<f32>) -> @location(0) vec4<f32> {
    shadowData_1 = shadowData;
    frag_tex_coord0_1 = frag_tex_coord0_;
    fog_tex_coord_1 = fog_tex_coord;
    main_1();
    let _e7 = out_color;
    return _e7;
}
