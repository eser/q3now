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
@id(7) override discard_mode: i32 = 0i;
override override_type_11_3: bool = (discard_mode == 1i);
override override_type_11_4: bool = (discard_mode == 2i);
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

    let _e66 = (*c);
    let _e69 = unnamed.cascadeMVP[_e66];
    let _e70 = (*worldPos);
    sc4_ = (_e69 * vec4<f32>(_e70.x, _e70.y, _e70.z, 1f));
    let _e76 = sc4_;
    let _e79 = sc4_[3u];
    sc = (_e76.xyz / vec3(_e79));
    let _e82 = sc;
    let _e86 = ((_e82.xy * 0.5f) + vec2(0.5f));
    sc[0u] = _e86.x;
    sc[1u] = _e86.y;
    let _e92 = sc[0u];
    let _e93 = (_e92 < 0f);
    phi_218_ = _e93;
    if !(_e93) {
        let _e96 = sc[0u];
        phi_218_ = (_e96 > 1f);
    }
    let _e99 = phi_218_;
    phi_225_ = _e99;
    if !(_e99) {
        let _e102 = sc[1u];
        phi_225_ = (_e102 < 0f);
    }
    let _e105 = phi_225_;
    phi_232_ = _e105;
    if !(_e105) {
        let _e108 = sc[1u];
        phi_232_ = (_e108 > 1f);
    }
    let _e111 = phi_232_;
    phi_239_ = _e111;
    if !(_e111) {
        let _e114 = sc[2u];
        phi_239_ = (_e114 > 1f);
    }
    let _e117 = phi_239_;
    if _e117 {
        return 1f;
    }
    let _e118 = (*c);
    layer = f32(_e118);
    let _e120 = textureDimensions(shadowMap, 0i);
    texelSize = (vec2(1f) / vec2<f32>(vec2<i32>(_e120).xy));
    let _e127 = sc[2u];
    currentDepth = (_e127 - shadow_bias);
    shadow = 0f;
    if override_type_11_ {
        let _e129 = currentDepth;
        let _e130 = sc;
        let _e131 = _e130.xy;
        let _e132 = layer;
        let _e135 = vec3<f32>(_e131.x, _e131.y, _e132);
        let _e141 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e135.x, _e135.y), i32(_e135.z));
        shadow = step(_e129, _e141.x);
    } else {
        if override_type_11_1 {
            let _e144 = currentDepth;
            let _e145 = sc;
            let _e146 = _e145.xy;
            let _e147 = layer;
            let _e150 = vec3<f32>(_e146.x, _e146.y, _e147);
            let _e156 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e150.x, _e150.y), i32(_e150.z));
            let _e159 = shadow;
            shadow = (_e159 + step(_e144, _e156.x));
            let _e161 = currentDepth;
            let _e162 = sc;
            let _e165 = texelSize[0u];
            let _e167 = (_e162.xy + vec2<f32>(_e165, 0f));
            let _e168 = layer;
            let _e171 = vec3<f32>(_e167.x, _e167.y, _e168);
            let _e177 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e171.x, _e171.y), i32(_e171.z));
            let _e180 = shadow;
            shadow = (_e180 + step(_e161, _e177.x));
            let _e182 = currentDepth;
            let _e183 = sc;
            let _e186 = texelSize[0u];
            let _e188 = (_e183.xy - vec2<f32>(_e186, 0f));
            let _e189 = layer;
            let _e192 = vec3<f32>(_e188.x, _e188.y, _e189);
            let _e198 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e192.x, _e192.y), i32(_e192.z));
            let _e201 = shadow;
            shadow = (_e201 + step(_e182, _e198.x));
            let _e203 = currentDepth;
            let _e204 = sc;
            let _e207 = texelSize[1u];
            let _e209 = (_e204.xy + vec2<f32>(0f, _e207));
            let _e210 = layer;
            let _e213 = vec3<f32>(_e209.x, _e209.y, _e210);
            let _e219 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e213.x, _e213.y), i32(_e213.z));
            let _e222 = shadow;
            shadow = (_e222 + step(_e203, _e219.x));
            let _e224 = currentDepth;
            let _e225 = sc;
            let _e228 = texelSize[1u];
            let _e230 = (_e225.xy - vec2<f32>(0f, _e228));
            let _e231 = layer;
            let _e234 = vec3<f32>(_e230.x, _e230.y, _e231);
            let _e240 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e234.x, _e234.y), i32(_e234.z));
            let _e243 = shadow;
            shadow = (_e243 + step(_e224, _e240.x));
            let _e245 = shadow;
            shadow = (_e245 / 5f);
        } else {
            x = -1i;
            loop {
                let _e247 = x;
                if (_e247 <= 1i) {
                    y = -1i;
                    loop {
                        let _e249 = y;
                        if (_e249 <= 1i) {
                            let _e251 = currentDepth;
                            let _e252 = sc;
                            let _e254 = x;
                            let _e256 = y;
                            let _e259 = texelSize;
                            let _e261 = (_e252.xy + (vec2<f32>(f32(_e254), f32(_e256)) * _e259));
                            let _e262 = layer;
                            let _e265 = vec3<f32>(_e261.x, _e261.y, _e262);
                            let _e271 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e265.x, _e265.y), i32(_e265.z));
                            let _e274 = shadow;
                            shadow = (_e274 + step(_e251, _e271.x));
                            continue;
                        } else {
                            break;
                        }
                        continuing {
                            let _e276 = y;
                            y = (_e276 + 1i);
                        }
                    }
                    continue;
                } else {
                    break;
                }
                continuing {
                    let _e278 = x;
                    x = (_e278 + 1i);
                }
            }
            let _e280 = shadow;
            shadow = (_e280 / 9f);
        }
    }
    let _e282 = shadow;
    return _e282;
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

    let _e73 = unnamed.cascadeSplits;
    let _e74 = (*viewDepth);
    cmp = step(_e73, vec4(_e74));
    let _e78 = cmp[0u];
    let _e80 = cmp[1u];
    let _e83 = cmp[2u];
    let _e86 = cmp[3u];
    cascade = min(i32((((_e78 + _e80) + _e83) + _e86)), 3i);
    let _e90 = cascade;
    (*outCascade) = _e90;
    let _e91 = cascade;
    if (_e91 == 0i) {
        local = 0f;
    } else {
        let _e93 = cascade;
        let _e98 = unnamed.cascadeSplits[max((_e93 - 1i), 0i)];
        local = _e98;
    }
    let _e99 = local;
    prevSplit = _e99;
    let _e100 = cascade;
    let _e103 = unnamed.cascadeSplits[_e100];
    farSplit = _e103;
    let _e104 = farSplit;
    let _e105 = prevSplit;
    blendRange = max((0.1f * (_e104 - _e105)), 1f);
    let _e109 = farSplit;
    let _e110 = (*viewDepth);
    let _e112 = blendRange;
    blendT = clamp(((_e109 - _e110) / _e112), 0f, 1f);
    let _e115 = cascade;
    param = _e115;
    let _e116 = (*worldPos_1);
    param_1 = _e116;
    let _e117 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param), (&param_1));
    s0_ = _e117;
    let _e118 = cascade;
    param_2 = min((_e118 + 1i), 3i);
    let _e121 = (*worldPos_1);
    param_3 = _e121;
    let _e122 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param_2), (&param_3));
    s1_ = _e122;
    let _e123 = s1_;
    let _e124 = s0_;
    let _e125 = blendT;
    return mix(_e123, _e124, _e125);
}

fn wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b(rgb: ptr<function, vec3<f32>>, lm_uv: ptr<function, vec2<f32>>) -> vec3<f32> {
    var sun_mask: f32;
    var shadow_1: f32;
    var ignoredCascade: i32;
    var param_4: vec3<f32>;
    var param_5: f32;
    var param_6: i32;

    let _e67 = unnamed.packed_indices[1i][3u];
    let _e73 = unnamed.packed_indices[1i][3u];
    let _e78 = (*lm_uv);
    let _e79 = textureSample(wired_bindless_images[(_e67 & 4095u)], wired_bindless_samplers[((_e73 >> bitcast<u32>(12i)) & 255u)], _e78);
    sun_mask = _e79.x;
    let _e81 = sun_mask;
    if (_e81 > 0.001f) {
        let _e83 = shadowData_1;
        param_4 = _e83.xyz;
        let _e86 = shadowData_1[3u];
        param_5 = _e86;
        let _e87 = sampleShadow_u0028_vf3_u003b_f1_u003b_i1_u003b((&param_4), (&param_5), (&param_6));
        let _e88 = param_6;
        ignoredCascade = _e88;
        shadow_1 = _e87;
        let _e89 = shadow_1;
        let _e90 = sun_mask;
        let _e92 = (*rgb);
        (*rgb) = (_e92 * mix(1f, _e89, _e90));
    }
    let _e94 = (*rgb);
    return _e94;
}

fn wired_apply_sun_shadow_operand_u0028_vf3_u003b(rgb_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var param_7: vec3<f32>;
    var param_8: vec2<f32>;

    if override_type_11_2 {
        let _e59 = (*rgb_1);
        param_7 = _e59;
        let _e60 = frag_tex_coord0_1;
        param_8 = _e60;
        let _e61 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_7), (&param_8));
        return _e61;
    }
    let _e62 = (*rgb_1);
    return _e62;
}

fn sRGBToLinear_u0028_vf3_u003b(c_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e60 = (*c_1);
    (*c_1) = max(_e60, vec3<f32>(0f, 0f, 0f));
    let _e62 = (*c_1);
    cutoff = (_e62 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e64 = (*c_1);
    lo = (_e64 / vec3(12.92f));
    let _e67 = (*c_1);
    hi = pow(((_e67 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e72 = hi;
    let _e73 = lo;
    let _e74 = cutoff;
    return mix(_e72, _e73, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e74));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_2: vec4<f32>;
    var param_9: vec3<f32>;

    let _e61 = (*role);
    let _e63 = (*role);
    let _e68 = unnamed.packed_indices[(_e61 / 4u)][(_e63 % 4u)];
    let _e71 = (*role);
    let _e73 = (*role);
    let _e78 = unnamed.packed_indices[(_e71 / 4u)][(_e73 % 4u)];
    let _e83 = (*uv);
    let _e84 = textureSample(wired_bindless_images[(_e68 & 4095u)], wired_bindless_samplers[((_e78 >> bitcast<u32>(12i)) & 255u)], _e83);
    c_2 = _e84;
    let _e85 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e85))) == 0i) {
        let _e90 = c_2;
        param_9 = _e90.xyz;
        let _e92 = sRGBToLinear_u0028_vf3_u003b((&param_9));
        c_2[0u] = _e92.x;
        c_2[1u] = _e92.y;
        c_2[2u] = _e92.z;
    }
    let _e99 = (*slot);
    if (lightmap_slot == (_e99 + 1i)) {
        let _e104 = unnamed.worldLightParams[0u];
        let _e105 = c_2;
        let _e107 = (_e105.xyz * _e104);
        c_2[0u] = _e107.x;
        c_2[1u] = _e107.y;
        c_2[2u] = _e107.z;
    }
    let _e114 = c_2;
    return _e114;
}

fn main_1() {
    var frag_color: vec4<f32>;
    var color0_: vec4<f32>;
    var param_10: u32;
    var param_11: vec2<f32>;
    var param_12: i32;
    var base: vec4<f32>;
    var param_13: vec3<f32>;

    frag_color[0u] = identity_color;
    frag_color[1u] = identity_color;
    frag_color[2u] = identity_color;
    frag_color[3u] = identity_alpha;
    param_10 = 0u;
    let _e67 = frag_tex_coord0_1;
    param_11 = _e67;
    param_12 = 0i;
    let _e68 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_10), (&param_11), (&param_12));
    let _e69 = frag_color;
    color0_ = (_e68 * _e69);
    let _e71 = color0_;
    base = _e71;
    let _e72 = color0_;
    base = _e72;
    let _e73 = base;
    param_13 = _e73.xyz;
    let _e75 = wired_apply_sun_shadow_operand_u0028_vf3_u003b((&param_13));
    base[0u] = _e75.x;
    base[1u] = _e75.y;
    base[2u] = _e75.z;
    if override_type_11_3 {
        let _e83 = base[3u];
        if (_e83 == 0f) {
            discard;
        }
    } else {
        if override_type_11_4 {
            let _e85 = base;
            let _e87 = base;
            if (dot(_e85.xyz, _e87.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e91 = base;
    out_color = _e91;
    return;
}

@fragment 
fn main(@location(7) shadowData: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>) -> @location(0) vec4<f32> {
    shadowData_1 = shadowData;
    frag_tex_coord0_1 = frag_tex_coord0_;
    main_1();
    let _e5 = out_color;
    return _e5;
}
