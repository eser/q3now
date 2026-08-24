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
    advancedFogColorDensity: vec4<f32>,
    advancedFogTypeFarEnabled: vec4<f32>,
}

@id(4) override tex_domain: i32 = 0i;
@id(26) override lightmap_slot: i32 = 0i;
@id(29) override shadow_bias: f32 = 0.005f;
@id(28) override shadow_pcf: i32 = 5i;
override override_type_3_: bool = (shadow_pcf <= 1i);
override override_type_3_1: bool = (shadow_pcf <= 5i);
override override_type_3_2: bool = (lightmap_slot == 1i);
override override_type_3_3: bool = (lightmap_slot == 2i);
@id(8) override identity_color: f32 = 1f;
@id(9) override identity_alpha: f32 = 1f;
@id(6) override tex_mode: i32 = 0i;
override override_type_3_4: bool = (tex_mode == 1i);
override override_type_3_5: bool = (tex_mode == 2i);
@id(7) override discard_mode: i32 = 0i;
override override_type_3_6: bool = (discard_mode == 1i);
override override_type_3_7: bool = (discard_mode == 2i);
@id(3) override alpha_to_coverage: i32 = 0i;

@group(0) @binding(0)
var<uniform> unnamed: UBO;
var<private> gl_FragCoord_1: vec4<f32>;
@group(1) @binding(0)
var wired_bindless_images: binding_array<texture_2d<f32>>;
@group(1) @binding(1)
var wired_bindless_samplers: binding_array<sampler>;
@group(2) @binding(0)
var shadowMap: texture_2d_array<f32>;
@group(2) @binding(32)
var shadowMap_sampler: sampler;
var<private> shadowData_1: vec4<f32>;
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> frag_tex_coord1_1: vec2<f32>;
var<private> out_color: vec4<f32>;

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e68 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e68 + 0.5f));
    let _e73 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e75 = fogType;
    let _e78 = fogType;
    return (((_e73 > 0.5f) && (_e75 >= 1i)) && (_e78 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e68 = wired_advanced_fog_enabled_u0028_();
    if !(_e68) {
        return 0f;
    }
    let _e71 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e71, 0.000001f));
    let _e76 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e76 + 0.5f));
    let _e79 = fogType_1;
    if (_e79 == 1i) {
        let _e83 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e83 <= 0f) {
            return 0f;
        }
        let _e85 = viewDepth;
        let _e88 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e85 / _e88), 0f, 1f);
    }
    let _e93 = unnamed.advancedFogColorDensity[3u];
    let _e95 = viewDepth;
    opticalDepth = (max(_e93, 0f) * _e95);
    let _e97 = fogType_1;
    if (_e97 == 2i) {
        let _e99 = opticalDepth;
        return clamp((1f - exp(-(_e99))), 0f, 1f);
    }
    let _e104 = opticalDepth;
    let _e105 = opticalDepth;
    return clamp((1f - exp(-((_e104 * _e105)))), 0f, 1f);
}

fn sampleCascade_u0028_i1_u003b_vf3_u003b(c: ptr<function, i32>, worldPos: ptr<function, vec3<f32>>) -> f32 {
    var sc4_: vec4<f32>;
    var sc: vec3<f32>;
    var layer: f32;
    var texelSize: vec2<f32>;
    var currentDepth: f32;
    var shadow: f32;
    var x: i32;
    var y: i32;
    var phi_304_: bool;
    var phi_311_: bool;
    var phi_318_: bool;
    var phi_325_: bool;

    let _e75 = (*c);
    let _e78 = unnamed.cascadeMVP[_e75];
    let _e79 = (*worldPos);
    sc4_ = (_e78 * vec4<f32>(_e79.x, _e79.y, _e79.z, 1f));
    let _e85 = sc4_;
    let _e88 = sc4_[3u];
    sc = (_e85.xyz / vec3(_e88));
    let _e91 = sc;
    let _e95 = ((_e91.xy * 0.5f) + vec2(0.5f));
    sc[0u] = _e95.x;
    sc[1u] = _e95.y;
    let _e101 = sc[0u];
    let _e102 = (_e101 < 0f);
    phi_304_ = _e102;
    if !(_e102) {
        let _e105 = sc[0u];
        phi_304_ = (_e105 > 1f);
    }
    let _e108 = phi_304_;
    phi_311_ = _e108;
    if !(_e108) {
        let _e111 = sc[1u];
        phi_311_ = (_e111 < 0f);
    }
    let _e114 = phi_311_;
    phi_318_ = _e114;
    if !(_e114) {
        let _e117 = sc[1u];
        phi_318_ = (_e117 > 1f);
    }
    let _e120 = phi_318_;
    phi_325_ = _e120;
    if !(_e120) {
        let _e123 = sc[2u];
        phi_325_ = (_e123 > 1f);
    }
    let _e126 = phi_325_;
    if _e126 {
        return 1f;
    }
    let _e127 = (*c);
    layer = f32(_e127);
    let _e129 = textureDimensions(shadowMap, 0i);
    texelSize = (vec2(1f) / vec2<f32>(vec2<i32>(_e129).xy));
    let _e136 = sc[2u];
    currentDepth = (_e136 - shadow_bias);
    shadow = 0f;
    if override_type_3_ {
        let _e138 = currentDepth;
        let _e139 = sc;
        let _e140 = _e139.xy;
        let _e141 = layer;
        let _e144 = vec3<f32>(_e140.x, _e140.y, _e141);
        let _e150 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e144.x, _e144.y), i32(_e144.z));
        shadow = step(_e138, _e150.x);
    } else {
        if override_type_3_1 {
            let _e153 = currentDepth;
            let _e154 = sc;
            let _e155 = _e154.xy;
            let _e156 = layer;
            let _e159 = vec3<f32>(_e155.x, _e155.y, _e156);
            let _e165 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e159.x, _e159.y), i32(_e159.z));
            let _e168 = shadow;
            shadow = (_e168 + step(_e153, _e165.x));
            let _e170 = currentDepth;
            let _e171 = sc;
            let _e174 = texelSize[0u];
            let _e176 = (_e171.xy + vec2<f32>(_e174, 0f));
            let _e177 = layer;
            let _e180 = vec3<f32>(_e176.x, _e176.y, _e177);
            let _e186 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e180.x, _e180.y), i32(_e180.z));
            let _e189 = shadow;
            shadow = (_e189 + step(_e170, _e186.x));
            let _e191 = currentDepth;
            let _e192 = sc;
            let _e195 = texelSize[0u];
            let _e197 = (_e192.xy - vec2<f32>(_e195, 0f));
            let _e198 = layer;
            let _e201 = vec3<f32>(_e197.x, _e197.y, _e198);
            let _e207 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e201.x, _e201.y), i32(_e201.z));
            let _e210 = shadow;
            shadow = (_e210 + step(_e191, _e207.x));
            let _e212 = currentDepth;
            let _e213 = sc;
            let _e216 = texelSize[1u];
            let _e218 = (_e213.xy + vec2<f32>(0f, _e216));
            let _e219 = layer;
            let _e222 = vec3<f32>(_e218.x, _e218.y, _e219);
            let _e228 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e222.x, _e222.y), i32(_e222.z));
            let _e231 = shadow;
            shadow = (_e231 + step(_e212, _e228.x));
            let _e233 = currentDepth;
            let _e234 = sc;
            let _e237 = texelSize[1u];
            let _e239 = (_e234.xy - vec2<f32>(0f, _e237));
            let _e240 = layer;
            let _e243 = vec3<f32>(_e239.x, _e239.y, _e240);
            let _e249 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e243.x, _e243.y), i32(_e243.z));
            let _e252 = shadow;
            shadow = (_e252 + step(_e233, _e249.x));
            let _e254 = shadow;
            shadow = (_e254 / 5f);
        } else {
            x = -1i;
            loop {
                let _e256 = x;
                if (_e256 <= 1i) {
                    y = -1i;
                    loop {
                        let _e258 = y;
                        if (_e258 <= 1i) {
                            let _e260 = currentDepth;
                            let _e261 = sc;
                            let _e263 = x;
                            let _e265 = y;
                            let _e268 = texelSize;
                            let _e270 = (_e261.xy + (vec2<f32>(f32(_e263), f32(_e265)) * _e268));
                            let _e271 = layer;
                            let _e274 = vec3<f32>(_e270.x, _e270.y, _e271);
                            let _e280 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e274.x, _e274.y), i32(_e274.z));
                            let _e283 = shadow;
                            shadow = (_e283 + step(_e260, _e280.x));
                            continue;
                        } else {
                            break;
                        }
                        continuing {
                            let _e285 = y;
                            y = (_e285 + 1i);
                        }
                    }
                    continue;
                } else {
                    break;
                }
                continuing {
                    let _e287 = x;
                    x = (_e287 + 1i);
                }
            }
            let _e289 = shadow;
            shadow = (_e289 / 9f);
        }
    }
    let _e291 = shadow;
    return _e291;
}

fn sampleShadow_u0028_vf3_u003b_f1_u003b_i1_u003b(worldPos_1: ptr<function, vec3<f32>>, viewDepth_1: ptr<function, f32>, outCascade: ptr<function, i32>) -> f32 {
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

    let _e82 = unnamed.cascadeSplits;
    let _e83 = (*viewDepth_1);
    cmp = step(_e82, vec4(_e83));
    let _e87 = cmp[0u];
    let _e89 = cmp[1u];
    let _e92 = cmp[2u];
    let _e95 = cmp[3u];
    cascade = min(i32((((_e87 + _e89) + _e92) + _e95)), 3i);
    let _e99 = cascade;
    (*outCascade) = _e99;
    let _e100 = cascade;
    if (_e100 == 0i) {
        local = 0f;
    } else {
        let _e102 = cascade;
        let _e107 = unnamed.cascadeSplits[max((_e102 - 1i), 0i)];
        local = _e107;
    }
    let _e108 = local;
    prevSplit = _e108;
    let _e109 = cascade;
    let _e112 = unnamed.cascadeSplits[_e109];
    farSplit = _e112;
    let _e113 = farSplit;
    let _e114 = prevSplit;
    blendRange = max((0.1f * (_e113 - _e114)), 1f);
    let _e118 = farSplit;
    let _e119 = (*viewDepth_1);
    let _e121 = blendRange;
    blendT = clamp(((_e118 - _e119) / _e121), 0f, 1f);
    let _e124 = cascade;
    param = _e124;
    let _e125 = (*worldPos_1);
    param_1 = _e125;
    let _e126 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param), (&param_1));
    s0_ = _e126;
    let _e127 = cascade;
    param_2 = min((_e127 + 1i), 3i);
    let _e130 = (*worldPos_1);
    param_3 = _e130;
    let _e131 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param_2), (&param_3));
    s1_ = _e131;
    let _e132 = s1_;
    let _e133 = s0_;
    let _e134 = blendT;
    return mix(_e132, _e133, _e134);
}

fn wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b(rgb: ptr<function, vec3<f32>>, lm_uv: ptr<function, vec2<f32>>) -> vec3<f32> {
    var sun_mask: f32;
    var shadow_1: f32;
    var ignoredCascade: i32;
    var param_4: vec3<f32>;
    var param_5: f32;
    var param_6: i32;

    let _e76 = unnamed.packed_indices[1i][3u];
    let _e82 = unnamed.packed_indices[1i][3u];
    let _e87 = (*lm_uv);
    let _e88 = textureSample(wired_bindless_images[(_e76 & 4095u)], wired_bindless_samplers[((_e82 >> bitcast<u32>(12i)) & 255u)], _e87);
    sun_mask = _e88.x;
    let _e90 = sun_mask;
    if (_e90 > 0.001f) {
        let _e92 = shadowData_1;
        param_4 = _e92.xyz;
        let _e95 = shadowData_1[3u];
        param_5 = _e95;
        let _e96 = sampleShadow_u0028_vf3_u003b_f1_u003b_i1_u003b((&param_4), (&param_5), (&param_6));
        let _e97 = param_6;
        ignoredCascade = _e97;
        shadow_1 = _e96;
        let _e98 = shadow_1;
        let _e99 = sun_mask;
        let _e101 = (*rgb);
        (*rgb) = (_e101 * mix(1f, _e98, _e99));
    }
    let _e103 = (*rgb);
    return _e103;
}

fn wired_apply_sun_shadow_operand_u0028_vf3_u003b(rgb_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var param_7: vec3<f32>;
    var param_8: vec2<f32>;
    var param_9: vec3<f32>;
    var param_10: vec2<f32>;

    if override_type_3_2 {
        let _e70 = (*rgb_1);
        param_7 = _e70;
        let _e71 = frag_tex_coord0_1;
        param_8 = _e71;
        let _e72 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_7), (&param_8));
        return _e72;
    }
    if override_type_3_3 {
        let _e73 = (*rgb_1);
        param_9 = _e73;
        let _e74 = frag_tex_coord1_1;
        param_10 = _e74;
        let _e75 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_9), (&param_10));
        return _e75;
    }
    let _e76 = (*rgb_1);
    return _e76;
}

fn sRGBToLinear_u0028_vf3_u003b(c_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e69 = (*c_1);
    (*c_1) = max(_e69, vec3<f32>(0f, 0f, 0f));
    let _e71 = (*c_1);
    cutoff = (_e71 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e73 = (*c_1);
    lo = (_e73 / vec3(12.92f));
    let _e76 = (*c_1);
    hi = pow(((_e76 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e81 = hi;
    let _e82 = lo;
    let _e83 = cutoff;
    return mix(_e81, _e82, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e83));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_2: vec4<f32>;
    var param_11: vec3<f32>;

    let _e70 = (*role);
    let _e72 = (*role);
    let _e77 = unnamed.packed_indices[(_e70 / 4u)][(_e72 % 4u)];
    let _e80 = (*role);
    let _e82 = (*role);
    let _e87 = unnamed.packed_indices[(_e80 / 4u)][(_e82 % 4u)];
    let _e92 = (*uv);
    let _e93 = textureSample(wired_bindless_images[(_e77 & 4095u)], wired_bindless_samplers[((_e87 >> bitcast<u32>(12i)) & 255u)], _e92);
    c_2 = _e93;
    let _e94 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e94))) == 0i) {
        let _e99 = c_2;
        param_11 = _e99.xyz;
        let _e101 = sRGBToLinear_u0028_vf3_u003b((&param_11));
        c_2[0u] = _e101.x;
        c_2[1u] = _e101.y;
        c_2[2u] = _e101.z;
    }
    let _e108 = (*slot);
    if (lightmap_slot == (_e108 + 1i)) {
        let _e113 = unnamed.worldLightParams[0u];
        let _e114 = c_2;
        let _e116 = (_e114.xyz * _e113);
        c_2[0u] = _e116.x;
        c_2[1u] = _e116.y;
        c_2[2u] = _e116.z;
    }
    let _e123 = c_2;
    return _e123;
}

fn main_1() {
    var frag_color: vec4<f32>;
    var color0_: vec4<f32>;
    var param_12: u32;
    var param_13: vec2<f32>;
    var param_14: i32;
    var color1_: vec4<f32>;
    var param_15: u32;
    var param_16: vec2<f32>;
    var param_17: i32;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var param_18: u32;
    var param_19: vec2<f32>;
    var param_20: i32;
    var color1_2: vec4<f32>;
    var param_21: u32;
    var param_22: vec2<f32>;
    var param_23: i32;
    var param_24: vec3<f32>;
    var fogAmount: f32;

    frag_color[0u] = identity_color;
    frag_color[1u] = identity_color;
    frag_color[2u] = identity_color;
    frag_color[3u] = identity_alpha;
    param_12 = 0u;
    let _e89 = frag_tex_coord0_1;
    param_13 = _e89;
    param_14 = 0i;
    let _e90 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_12), (&param_13), (&param_14));
    let _e91 = frag_color;
    color0_ = (_e90 * _e91);
    if override_type_3_4 {
        param_15 = 1u;
        let _e93 = frag_tex_coord1_1;
        param_16 = _e93;
        param_17 = 1i;
        let _e94 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_15), (&param_16), (&param_17));
        color1_ = _e94;
        let _e95 = color0_;
        let _e97 = color1_;
        let _e99 = (_e95.xyz + _e97.xyz);
        let _e101 = color0_[3u];
        let _e103 = color1_[3u];
        base = vec4<f32>(_e99.x, _e99.y, _e99.z, (_e101 * _e103));
    } else {
        if override_type_3_5 {
            param_18 = 1u;
            let _e109 = frag_tex_coord1_1;
            param_19 = _e109;
            param_20 = 1i;
            let _e110 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_18), (&param_19), (&param_20));
            let _e111 = frag_color;
            color1_1 = (_e110 * _e111);
            let _e113 = color0_;
            let _e115 = color1_1;
            let _e117 = (_e113.xyz + _e115.xyz);
            let _e119 = color0_[3u];
            let _e121 = color1_1[3u];
            base = vec4<f32>(_e117.x, _e117.y, _e117.z, (_e119 * _e121));
        } else {
            param_21 = 1u;
            let _e127 = frag_tex_coord1_1;
            param_22 = _e127;
            param_23 = 1i;
            let _e128 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_21), (&param_22), (&param_23));
            let _e129 = frag_color;
            color1_2 = (_e128 * _e129);
            let _e131 = color0_;
            let _e133 = color1_2;
            let _e135 = (_e131.xyz * _e133.xyz);
            base[0u] = _e135.x;
            base[1u] = _e135.y;
            base[2u] = _e135.z;
            let _e143 = color0_[3u];
            let _e145 = color1_2[3u];
            base[3u] = (_e143 * _e145);
        }
    }
    let _e148 = base;
    param_24 = _e148.xyz;
    let _e150 = wired_apply_sun_shadow_operand_u0028_vf3_u003b((&param_24));
    base[0u] = _e150.x;
    base[1u] = _e150.y;
    base[2u] = _e150.z;
    let _e157 = wired_advanced_fog_enabled_u0028_();
    if _e157 {
        let _e158 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e158;
        let _e159 = base;
        let _e162 = unnamed.advancedFogColorDensity;
        let _e164 = fogAmount;
        let _e166 = mix(_e159.xyz, _e162.xyz, vec3(_e164));
        base[0u] = _e166.x;
        base[1u] = _e166.y;
        base[2u] = _e166.z;
    }
    if override_type_3_6 {
        let _e174 = base[3u];
        if (_e174 == 0f) {
            discard;
        }
    } else {
        if override_type_3_7 {
            let _e176 = base;
            let _e178 = base;
            if (dot(_e176.xyz, _e178.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e182 = base;
    out_color = _e182;
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(7) shadowData: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>) -> @location(0) vec4<f32> {
    gl_FragCoord_1 = gl_FragCoord;
    shadowData_1 = shadowData;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    main_1();
    let _e9 = out_color;
    return _e9;
}
