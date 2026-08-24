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
var<private> frag_color0In_1: vec4<f32>;
var<private> out_color: vec4<f32>;

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e67 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e67 + 0.5f));
    let _e72 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e74 = fogType;
    let _e77 = fogType;
    return (((_e72 > 0.5f) && (_e74 >= 1i)) && (_e77 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e67 = wired_advanced_fog_enabled_u0028_();
    if !(_e67) {
        return 0f;
    }
    let _e70 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e70, 0.000001f));
    let _e75 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e75 + 0.5f));
    let _e78 = fogType_1;
    if (_e78 == 1i) {
        let _e82 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e82 <= 0f) {
            return 0f;
        }
        let _e84 = viewDepth;
        let _e87 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e84 / _e87), 0f, 1f);
    }
    let _e92 = unnamed.advancedFogColorDensity[3u];
    let _e94 = viewDepth;
    opticalDepth = (max(_e92, 0f) * _e94);
    let _e96 = fogType_1;
    if (_e96 == 2i) {
        let _e98 = opticalDepth;
        return clamp((1f - exp(-(_e98))), 0f, 1f);
    }
    let _e103 = opticalDepth;
    let _e104 = opticalDepth;
    return clamp((1f - exp(-((_e103 * _e104)))), 0f, 1f);
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

    let _e74 = (*c);
    let _e77 = unnamed.cascadeMVP[_e74];
    let _e78 = (*worldPos);
    sc4_ = (_e77 * vec4<f32>(_e78.x, _e78.y, _e78.z, 1f));
    let _e84 = sc4_;
    let _e87 = sc4_[3u];
    sc = (_e84.xyz / vec3(_e87));
    let _e90 = sc;
    let _e94 = ((_e90.xy * 0.5f) + vec2(0.5f));
    sc[0u] = _e94.x;
    sc[1u] = _e94.y;
    let _e100 = sc[0u];
    let _e101 = (_e100 < 0f);
    phi_304_ = _e101;
    if !(_e101) {
        let _e104 = sc[0u];
        phi_304_ = (_e104 > 1f);
    }
    let _e107 = phi_304_;
    phi_311_ = _e107;
    if !(_e107) {
        let _e110 = sc[1u];
        phi_311_ = (_e110 < 0f);
    }
    let _e113 = phi_311_;
    phi_318_ = _e113;
    if !(_e113) {
        let _e116 = sc[1u];
        phi_318_ = (_e116 > 1f);
    }
    let _e119 = phi_318_;
    phi_325_ = _e119;
    if !(_e119) {
        let _e122 = sc[2u];
        phi_325_ = (_e122 > 1f);
    }
    let _e125 = phi_325_;
    if _e125 {
        return 1f;
    }
    let _e126 = (*c);
    layer = f32(_e126);
    let _e128 = textureDimensions(shadowMap, 0i);
    texelSize = (vec2(1f) / vec2<f32>(vec2<i32>(_e128).xy));
    let _e135 = sc[2u];
    currentDepth = (_e135 - shadow_bias);
    shadow = 0f;
    if override_type_3_ {
        let _e137 = currentDepth;
        let _e138 = sc;
        let _e139 = _e138.xy;
        let _e140 = layer;
        let _e143 = vec3<f32>(_e139.x, _e139.y, _e140);
        let _e149 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e143.x, _e143.y), i32(_e143.z));
        shadow = step(_e137, _e149.x);
    } else {
        if override_type_3_1 {
            let _e152 = currentDepth;
            let _e153 = sc;
            let _e154 = _e153.xy;
            let _e155 = layer;
            let _e158 = vec3<f32>(_e154.x, _e154.y, _e155);
            let _e164 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e158.x, _e158.y), i32(_e158.z));
            let _e167 = shadow;
            shadow = (_e167 + step(_e152, _e164.x));
            let _e169 = currentDepth;
            let _e170 = sc;
            let _e173 = texelSize[0u];
            let _e175 = (_e170.xy + vec2<f32>(_e173, 0f));
            let _e176 = layer;
            let _e179 = vec3<f32>(_e175.x, _e175.y, _e176);
            let _e185 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e179.x, _e179.y), i32(_e179.z));
            let _e188 = shadow;
            shadow = (_e188 + step(_e169, _e185.x));
            let _e190 = currentDepth;
            let _e191 = sc;
            let _e194 = texelSize[0u];
            let _e196 = (_e191.xy - vec2<f32>(_e194, 0f));
            let _e197 = layer;
            let _e200 = vec3<f32>(_e196.x, _e196.y, _e197);
            let _e206 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e200.x, _e200.y), i32(_e200.z));
            let _e209 = shadow;
            shadow = (_e209 + step(_e190, _e206.x));
            let _e211 = currentDepth;
            let _e212 = sc;
            let _e215 = texelSize[1u];
            let _e217 = (_e212.xy + vec2<f32>(0f, _e215));
            let _e218 = layer;
            let _e221 = vec3<f32>(_e217.x, _e217.y, _e218);
            let _e227 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e221.x, _e221.y), i32(_e221.z));
            let _e230 = shadow;
            shadow = (_e230 + step(_e211, _e227.x));
            let _e232 = currentDepth;
            let _e233 = sc;
            let _e236 = texelSize[1u];
            let _e238 = (_e233.xy - vec2<f32>(0f, _e236));
            let _e239 = layer;
            let _e242 = vec3<f32>(_e238.x, _e238.y, _e239);
            let _e248 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e242.x, _e242.y), i32(_e242.z));
            let _e251 = shadow;
            shadow = (_e251 + step(_e232, _e248.x));
            let _e253 = shadow;
            shadow = (_e253 / 5f);
        } else {
            x = -1i;
            loop {
                let _e255 = x;
                if (_e255 <= 1i) {
                    y = -1i;
                    loop {
                        let _e257 = y;
                        if (_e257 <= 1i) {
                            let _e259 = currentDepth;
                            let _e260 = sc;
                            let _e262 = x;
                            let _e264 = y;
                            let _e267 = texelSize;
                            let _e269 = (_e260.xy + (vec2<f32>(f32(_e262), f32(_e264)) * _e267));
                            let _e270 = layer;
                            let _e273 = vec3<f32>(_e269.x, _e269.y, _e270);
                            let _e279 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e273.x, _e273.y), i32(_e273.z));
                            let _e282 = shadow;
                            shadow = (_e282 + step(_e259, _e279.x));
                            continue;
                        } else {
                            break;
                        }
                        continuing {
                            let _e284 = y;
                            y = (_e284 + 1i);
                        }
                    }
                    continue;
                } else {
                    break;
                }
                continuing {
                    let _e286 = x;
                    x = (_e286 + 1i);
                }
            }
            let _e288 = shadow;
            shadow = (_e288 / 9f);
        }
    }
    let _e290 = shadow;
    return _e290;
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

    let _e81 = unnamed.cascadeSplits;
    let _e82 = (*viewDepth_1);
    cmp = step(_e81, vec4(_e82));
    let _e86 = cmp[0u];
    let _e88 = cmp[1u];
    let _e91 = cmp[2u];
    let _e94 = cmp[3u];
    cascade = min(i32((((_e86 + _e88) + _e91) + _e94)), 3i);
    let _e98 = cascade;
    (*outCascade) = _e98;
    let _e99 = cascade;
    if (_e99 == 0i) {
        local = 0f;
    } else {
        let _e101 = cascade;
        let _e106 = unnamed.cascadeSplits[max((_e101 - 1i), 0i)];
        local = _e106;
    }
    let _e107 = local;
    prevSplit = _e107;
    let _e108 = cascade;
    let _e111 = unnamed.cascadeSplits[_e108];
    farSplit = _e111;
    let _e112 = farSplit;
    let _e113 = prevSplit;
    blendRange = max((0.1f * (_e112 - _e113)), 1f);
    let _e117 = farSplit;
    let _e118 = (*viewDepth_1);
    let _e120 = blendRange;
    blendT = clamp(((_e117 - _e118) / _e120), 0f, 1f);
    let _e123 = cascade;
    param = _e123;
    let _e124 = (*worldPos_1);
    param_1 = _e124;
    let _e125 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param), (&param_1));
    s0_ = _e125;
    let _e126 = cascade;
    param_2 = min((_e126 + 1i), 3i);
    let _e129 = (*worldPos_1);
    param_3 = _e129;
    let _e130 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param_2), (&param_3));
    s1_ = _e130;
    let _e131 = s1_;
    let _e132 = s0_;
    let _e133 = blendT;
    return mix(_e131, _e132, _e133);
}

fn wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b(rgb: ptr<function, vec3<f32>>, lm_uv: ptr<function, vec2<f32>>) -> vec3<f32> {
    var sun_mask: f32;
    var shadow_1: f32;
    var ignoredCascade: i32;
    var param_4: vec3<f32>;
    var param_5: f32;
    var param_6: i32;

    let _e75 = unnamed.packed_indices[1i][3u];
    let _e81 = unnamed.packed_indices[1i][3u];
    let _e86 = (*lm_uv);
    let _e87 = textureSample(wired_bindless_images[(_e75 & 4095u)], wired_bindless_samplers[((_e81 >> bitcast<u32>(12i)) & 255u)], _e86);
    sun_mask = _e87.x;
    let _e89 = sun_mask;
    if (_e89 > 0.001f) {
        let _e91 = shadowData_1;
        param_4 = _e91.xyz;
        let _e94 = shadowData_1[3u];
        param_5 = _e94;
        let _e95 = sampleShadow_u0028_vf3_u003b_f1_u003b_i1_u003b((&param_4), (&param_5), (&param_6));
        let _e96 = param_6;
        ignoredCascade = _e96;
        shadow_1 = _e95;
        let _e97 = shadow_1;
        let _e98 = sun_mask;
        let _e100 = (*rgb);
        (*rgb) = (_e100 * mix(1f, _e97, _e98));
    }
    let _e102 = (*rgb);
    return _e102;
}

fn wired_apply_sun_shadow_operand_u0028_vf3_u003b(rgb_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var param_7: vec3<f32>;
    var param_8: vec2<f32>;
    var param_9: vec3<f32>;
    var param_10: vec2<f32>;

    if override_type_3_2 {
        let _e69 = (*rgb_1);
        param_7 = _e69;
        let _e70 = frag_tex_coord0_1;
        param_8 = _e70;
        let _e71 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_7), (&param_8));
        return _e71;
    }
    if override_type_3_3 {
        let _e72 = (*rgb_1);
        param_9 = _e72;
        let _e73 = frag_tex_coord1_1;
        param_10 = _e73;
        let _e74 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_9), (&param_10));
        return _e74;
    }
    let _e75 = (*rgb_1);
    return _e75;
}

fn sRGBToLinear_u0028_vf3_u003b(c_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e68 = (*c_1);
    (*c_1) = max(_e68, vec3<f32>(0f, 0f, 0f));
    let _e70 = (*c_1);
    cutoff = (_e70 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e72 = (*c_1);
    lo = (_e72 / vec3(12.92f));
    let _e75 = (*c_1);
    hi = pow(((_e75 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e80 = hi;
    let _e81 = lo;
    let _e82 = cutoff;
    return mix(_e80, _e81, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e82));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_2: vec4<f32>;
    var param_11: vec3<f32>;

    let _e69 = (*role);
    let _e71 = (*role);
    let _e76 = unnamed.packed_indices[(_e69 / 4u)][(_e71 % 4u)];
    let _e79 = (*role);
    let _e81 = (*role);
    let _e86 = unnamed.packed_indices[(_e79 / 4u)][(_e81 % 4u)];
    let _e91 = (*uv);
    let _e92 = textureSample(wired_bindless_images[(_e76 & 4095u)], wired_bindless_samplers[((_e86 >> bitcast<u32>(12i)) & 255u)], _e91);
    c_2 = _e92;
    let _e93 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e93))) == 0i) {
        let _e98 = c_2;
        param_11 = _e98.xyz;
        let _e100 = sRGBToLinear_u0028_vf3_u003b((&param_11));
        c_2[0u] = _e100.x;
        c_2[1u] = _e100.y;
        c_2[2u] = _e100.z;
    }
    let _e107 = (*slot);
    if (lightmap_slot == (_e107 + 1i)) {
        let _e112 = unnamed.worldLightParams[0u];
        let _e113 = c_2;
        let _e115 = (_e113.xyz * _e112);
        c_2[0u] = _e115.x;
        c_2[1u] = _e115.y;
        c_2[2u] = _e115.z;
    }
    let _e122 = c_2;
    return _e122;
}

fn main_1() {
    var frag_color0_: vec4<f32>;
    var param_12: vec3<f32>;
    var color0_: vec4<f32>;
    var param_13: u32;
    var param_14: vec2<f32>;
    var param_15: i32;
    var color1_: vec4<f32>;
    var param_16: u32;
    var param_17: vec2<f32>;
    var param_18: i32;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var param_19: u32;
    var param_20: vec2<f32>;
    var param_21: i32;
    var color1_2: vec4<f32>;
    var param_22: u32;
    var param_23: vec2<f32>;
    var param_24: i32;
    var param_25: vec3<f32>;
    var fogAmount: f32;

    let _e85 = frag_color0In_1;
    param_12 = _e85.xyz;
    let _e87 = sRGBToLinear_u0028_vf3_u003b((&param_12));
    let _e89 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e87.x, _e87.y, _e87.z, _e89);
    param_13 = 0u;
    let _e94 = frag_tex_coord0_1;
    param_14 = _e94;
    param_15 = 0i;
    let _e95 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_13), (&param_14), (&param_15));
    let _e96 = frag_color0_;
    color0_ = (_e95 * _e96);
    if override_type_3_4 {
        param_16 = 1u;
        let _e98 = frag_tex_coord1_1;
        param_17 = _e98;
        param_18 = 1i;
        let _e99 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_16), (&param_17), (&param_18));
        color1_ = _e99;
        let _e100 = color0_;
        let _e102 = color1_;
        let _e104 = (_e100.xyz + _e102.xyz);
        let _e106 = color0_[3u];
        let _e108 = color1_[3u];
        base = vec4<f32>(_e104.x, _e104.y, _e104.z, (_e106 * _e108));
    } else {
        if override_type_3_5 {
            param_19 = 1u;
            let _e114 = frag_tex_coord1_1;
            param_20 = _e114;
            param_21 = 1i;
            let _e115 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_19), (&param_20), (&param_21));
            let _e116 = frag_color0_;
            color1_1 = (_e115 * _e116);
            let _e118 = color0_;
            let _e120 = color1_1;
            let _e122 = (_e118.xyz + _e120.xyz);
            let _e124 = color0_[3u];
            let _e126 = color1_1[3u];
            base = vec4<f32>(_e122.x, _e122.y, _e122.z, (_e124 * _e126));
        } else {
            param_22 = 1u;
            let _e132 = frag_tex_coord1_1;
            param_23 = _e132;
            param_24 = 1i;
            let _e133 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_22), (&param_23), (&param_24));
            color1_2 = _e133;
            let _e134 = color0_;
            let _e136 = color1_2;
            let _e138 = (_e134.xyz * _e136.xyz);
            base[0u] = _e138.x;
            base[1u] = _e138.y;
            base[2u] = _e138.z;
            let _e146 = color0_[3u];
            let _e148 = color1_2[3u];
            base[3u] = (_e146 * _e148);
        }
    }
    let _e151 = base;
    param_25 = _e151.xyz;
    let _e153 = wired_apply_sun_shadow_operand_u0028_vf3_u003b((&param_25));
    base[0u] = _e153.x;
    base[1u] = _e153.y;
    base[2u] = _e153.z;
    let _e160 = wired_advanced_fog_enabled_u0028_();
    if _e160 {
        let _e161 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e161;
        let _e162 = base;
        let _e165 = unnamed.advancedFogColorDensity;
        let _e167 = fogAmount;
        let _e169 = mix(_e162.xyz, _e165.xyz, vec3(_e167));
        base[0u] = _e169.x;
        base[1u] = _e169.y;
        base[2u] = _e169.z;
    }
    if override_type_3_6 {
        let _e177 = base[3u];
        if (_e177 == 0f) {
            discard;
        }
    } else {
        if override_type_3_7 {
            let _e179 = base;
            let _e181 = base;
            if (dot(_e179.xyz, _e181.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e185 = base;
    out_color = _e185;
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(7) shadowData: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(0) frag_color0In: vec4<f32>) -> @location(0) vec4<f32> {
    gl_FragCoord_1 = gl_FragCoord;
    shadowData_1 = shadowData;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    frag_color0In_1 = frag_color0In;
    main_1();
    let _e11 = out_color;
    return _e11;
}
