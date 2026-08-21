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

    let _e64 = (*c);
    let _e67 = unnamed.cascadeMVP[_e64];
    let _e68 = (*worldPos);
    sc4_ = (_e67 * vec4<f32>(_e68.x, _e68.y, _e68.z, 1f));
    let _e74 = sc4_;
    let _e77 = sc4_[3u];
    sc = (_e74.xyz / vec3(_e77));
    let _e80 = sc;
    let _e84 = ((_e80.xy * 0.5f) + vec2(0.5f));
    sc[0u] = _e84.x;
    sc[1u] = _e84.y;
    let _e90 = sc[0u];
    let _e91 = (_e90 < 0f);
    phi_218_ = _e91;
    if !(_e91) {
        let _e94 = sc[0u];
        phi_218_ = (_e94 > 1f);
    }
    let _e97 = phi_218_;
    phi_225_ = _e97;
    if !(_e97) {
        let _e100 = sc[1u];
        phi_225_ = (_e100 < 0f);
    }
    let _e103 = phi_225_;
    phi_232_ = _e103;
    if !(_e103) {
        let _e106 = sc[1u];
        phi_232_ = (_e106 > 1f);
    }
    let _e109 = phi_232_;
    phi_239_ = _e109;
    if !(_e109) {
        let _e112 = sc[2u];
        phi_239_ = (_e112 > 1f);
    }
    let _e115 = phi_239_;
    if _e115 {
        return 1f;
    }
    let _e116 = (*c);
    layer = f32(_e116);
    let _e118 = textureDimensions(shadowMap, 0i);
    texelSize = (vec2(1f) / vec2<f32>(vec2<i32>(_e118).xy));
    let _e125 = sc[2u];
    currentDepth = (_e125 - shadow_bias);
    shadow = 0f;
    if override_type_11_ {
        let _e127 = currentDepth;
        let _e128 = sc;
        let _e129 = _e128.xy;
        let _e130 = layer;
        let _e133 = vec3<f32>(_e129.x, _e129.y, _e130);
        let _e139 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e133.x, _e133.y), i32(_e133.z));
        shadow = step(_e127, _e139.x);
    } else {
        if override_type_11_1 {
            let _e142 = currentDepth;
            let _e143 = sc;
            let _e144 = _e143.xy;
            let _e145 = layer;
            let _e148 = vec3<f32>(_e144.x, _e144.y, _e145);
            let _e154 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e148.x, _e148.y), i32(_e148.z));
            let _e157 = shadow;
            shadow = (_e157 + step(_e142, _e154.x));
            let _e159 = currentDepth;
            let _e160 = sc;
            let _e163 = texelSize[0u];
            let _e165 = (_e160.xy + vec2<f32>(_e163, 0f));
            let _e166 = layer;
            let _e169 = vec3<f32>(_e165.x, _e165.y, _e166);
            let _e175 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e169.x, _e169.y), i32(_e169.z));
            let _e178 = shadow;
            shadow = (_e178 + step(_e159, _e175.x));
            let _e180 = currentDepth;
            let _e181 = sc;
            let _e184 = texelSize[0u];
            let _e186 = (_e181.xy - vec2<f32>(_e184, 0f));
            let _e187 = layer;
            let _e190 = vec3<f32>(_e186.x, _e186.y, _e187);
            let _e196 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e190.x, _e190.y), i32(_e190.z));
            let _e199 = shadow;
            shadow = (_e199 + step(_e180, _e196.x));
            let _e201 = currentDepth;
            let _e202 = sc;
            let _e205 = texelSize[1u];
            let _e207 = (_e202.xy + vec2<f32>(0f, _e205));
            let _e208 = layer;
            let _e211 = vec3<f32>(_e207.x, _e207.y, _e208);
            let _e217 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e211.x, _e211.y), i32(_e211.z));
            let _e220 = shadow;
            shadow = (_e220 + step(_e201, _e217.x));
            let _e222 = currentDepth;
            let _e223 = sc;
            let _e226 = texelSize[1u];
            let _e228 = (_e223.xy - vec2<f32>(0f, _e226));
            let _e229 = layer;
            let _e232 = vec3<f32>(_e228.x, _e228.y, _e229);
            let _e238 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e232.x, _e232.y), i32(_e232.z));
            let _e241 = shadow;
            shadow = (_e241 + step(_e222, _e238.x));
            let _e243 = shadow;
            shadow = (_e243 / 5f);
        } else {
            x = -1i;
            loop {
                let _e245 = x;
                if (_e245 <= 1i) {
                    y = -1i;
                    loop {
                        let _e247 = y;
                        if (_e247 <= 1i) {
                            let _e249 = currentDepth;
                            let _e250 = sc;
                            let _e252 = x;
                            let _e254 = y;
                            let _e257 = texelSize;
                            let _e259 = (_e250.xy + (vec2<f32>(f32(_e252), f32(_e254)) * _e257));
                            let _e260 = layer;
                            let _e263 = vec3<f32>(_e259.x, _e259.y, _e260);
                            let _e269 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e263.x, _e263.y), i32(_e263.z));
                            let _e272 = shadow;
                            shadow = (_e272 + step(_e249, _e269.x));
                            continue;
                        } else {
                            break;
                        }
                        continuing {
                            let _e274 = y;
                            y = (_e274 + 1i);
                        }
                    }
                    continue;
                } else {
                    break;
                }
                continuing {
                    let _e276 = x;
                    x = (_e276 + 1i);
                }
            }
            let _e278 = shadow;
            shadow = (_e278 / 9f);
        }
    }
    let _e280 = shadow;
    return _e280;
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

    let _e71 = unnamed.cascadeSplits;
    let _e72 = (*viewDepth);
    cmp = step(_e71, vec4(_e72));
    let _e76 = cmp[0u];
    let _e78 = cmp[1u];
    let _e81 = cmp[2u];
    let _e84 = cmp[3u];
    cascade = min(i32((((_e76 + _e78) + _e81) + _e84)), 3i);
    let _e88 = cascade;
    (*outCascade) = _e88;
    let _e89 = cascade;
    if (_e89 == 0i) {
        local = 0f;
    } else {
        let _e91 = cascade;
        let _e96 = unnamed.cascadeSplits[max((_e91 - 1i), 0i)];
        local = _e96;
    }
    let _e97 = local;
    prevSplit = _e97;
    let _e98 = cascade;
    let _e101 = unnamed.cascadeSplits[_e98];
    farSplit = _e101;
    let _e102 = farSplit;
    let _e103 = prevSplit;
    blendRange = max((0.1f * (_e102 - _e103)), 1f);
    let _e107 = farSplit;
    let _e108 = (*viewDepth);
    let _e110 = blendRange;
    blendT = clamp(((_e107 - _e108) / _e110), 0f, 1f);
    let _e113 = cascade;
    param = _e113;
    let _e114 = (*worldPos_1);
    param_1 = _e114;
    let _e115 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param), (&param_1));
    s0_ = _e115;
    let _e116 = cascade;
    param_2 = min((_e116 + 1i), 3i);
    let _e119 = (*worldPos_1);
    param_3 = _e119;
    let _e120 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param_2), (&param_3));
    s1_ = _e120;
    let _e121 = s1_;
    let _e122 = s0_;
    let _e123 = blendT;
    return mix(_e121, _e122, _e123);
}

fn wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b(rgb: ptr<function, vec3<f32>>, lm_uv: ptr<function, vec2<f32>>) -> vec3<f32> {
    var sun_mask: f32;
    var shadow_1: f32;
    var ignoredCascade: i32;
    var param_4: vec3<f32>;
    var param_5: f32;
    var param_6: i32;

    let _e65 = unnamed.packed_indices[1i][3u];
    let _e71 = unnamed.packed_indices[1i][3u];
    let _e76 = (*lm_uv);
    let _e77 = textureSample(wired_bindless_images[(_e65 & 4095u)], wired_bindless_samplers[((_e71 >> bitcast<u32>(12i)) & 255u)], _e76);
    sun_mask = _e77.x;
    let _e79 = sun_mask;
    if (_e79 > 0.001f) {
        let _e81 = shadowData_1;
        param_4 = _e81.xyz;
        let _e84 = shadowData_1[3u];
        param_5 = _e84;
        let _e85 = sampleShadow_u0028_vf3_u003b_f1_u003b_i1_u003b((&param_4), (&param_5), (&param_6));
        let _e86 = param_6;
        ignoredCascade = _e86;
        shadow_1 = _e85;
        let _e87 = shadow_1;
        let _e88 = sun_mask;
        let _e90 = (*rgb);
        (*rgb) = (_e90 * mix(1f, _e87, _e88));
    }
    let _e92 = (*rgb);
    return _e92;
}

fn wired_apply_sun_shadow_operand_u0028_vf3_u003b(rgb_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var param_7: vec3<f32>;
    var param_8: vec2<f32>;

    if override_type_11_2 {
        let _e57 = (*rgb_1);
        param_7 = _e57;
        let _e58 = frag_tex_coord0_1;
        param_8 = _e58;
        let _e59 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_7), (&param_8));
        return _e59;
    }
    let _e60 = (*rgb_1);
    return _e60;
}

fn sRGBToLinear_u0028_vf3_u003b(c_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e58 = (*c_1);
    (*c_1) = max(_e58, vec3<f32>(0f, 0f, 0f));
    let _e60 = (*c_1);
    cutoff = (_e60 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e62 = (*c_1);
    lo = (_e62 / vec3(12.92f));
    let _e65 = (*c_1);
    hi = pow(((_e65 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e70 = hi;
    let _e71 = lo;
    let _e72 = cutoff;
    return mix(_e70, _e71, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e72));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_2: vec4<f32>;
    var param_9: vec3<f32>;

    let _e59 = (*role);
    let _e61 = (*role);
    let _e66 = unnamed.packed_indices[(_e59 / 4u)][(_e61 % 4u)];
    let _e69 = (*role);
    let _e71 = (*role);
    let _e76 = unnamed.packed_indices[(_e69 / 4u)][(_e71 % 4u)];
    let _e81 = (*uv);
    let _e82 = textureSample(wired_bindless_images[(_e66 & 4095u)], wired_bindless_samplers[((_e76 >> bitcast<u32>(12i)) & 255u)], _e81);
    c_2 = _e82;
    let _e83 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e83))) == 0i) {
        let _e88 = c_2;
        param_9 = _e88.xyz;
        let _e90 = sRGBToLinear_u0028_vf3_u003b((&param_9));
        c_2[0u] = _e90.x;
        c_2[1u] = _e90.y;
        c_2[2u] = _e90.z;
    }
    let _e97 = (*slot);
    if (lightmap_slot == (_e97 + 1i)) {
        let _e102 = unnamed.worldLightParams[0u];
        let _e103 = c_2;
        let _e105 = (_e103.xyz * _e102);
        c_2[0u] = _e105.x;
        c_2[1u] = _e105.y;
        c_2[2u] = _e105.z;
    }
    let _e112 = c_2;
    return _e112;
}

fn main_1() {
    var color0_: vec4<f32>;
    var param_10: u32;
    var param_11: vec2<f32>;
    var param_12: i32;
    var base: vec4<f32>;
    var param_13: vec3<f32>;

    param_10 = 0u;
    let _e60 = frag_tex_coord0_1;
    param_11 = _e60;
    param_12 = 0i;
    let _e61 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_10), (&param_11), (&param_12));
    color0_ = _e61;
    let _e62 = color0_;
    base = _e62;
    let _e63 = color0_;
    base = _e63;
    let _e64 = base;
    param_13 = _e64.xyz;
    let _e66 = wired_apply_sun_shadow_operand_u0028_vf3_u003b((&param_13));
    base[0u] = _e66.x;
    base[1u] = _e66.y;
    base[2u] = _e66.z;
    if override_type_11_3 {
        let _e74 = base[3u];
        if (_e74 == 0f) {
            discard;
        }
    } else {
        if override_type_11_4 {
            let _e76 = base;
            let _e78 = base;
            if (dot(_e76.xyz, _e78.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e82 = base;
    out_color = _e82;
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
