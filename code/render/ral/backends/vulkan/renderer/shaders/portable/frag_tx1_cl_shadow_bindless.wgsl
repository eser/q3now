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
    emissionRadiance: vec4<f32>,
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
override override_type_3_6: bool = (override_type_3_4 || override_type_3_5);
override override_type_3_7: bool = (tex_mode == 3i);
override override_type_3_8: bool = (tex_mode == 4i);
override override_type_3_9: bool = (tex_mode == 5i);
override override_type_3_10: bool = (tex_mode == 6i);
override override_type_3_11: bool = (tex_mode == 7i);
override override_type_3_12: bool = (lightmap_slot != 0i);
@id(7) override discard_mode: i32 = 0i;
override override_type_3_13: bool = (discard_mode == 1i);
override override_type_3_14: bool = (discard_mode == 2i);
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
var<private> frag_color1In_1: vec4<f32>;
var<private> out_color: vec4<f32>;

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e87 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e87 + 0.5f));
    let _e92 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e94 = fogType;
    let _e97 = fogType;
    return (((_e92 > 0.5f) && (_e94 >= 1i)) && (_e97 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e87 = wired_advanced_fog_enabled_u0028_();
    if !(_e87) {
        return 0f;
    }
    let _e90 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e90, 0.000001f));
    let _e95 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e95 + 0.5f));
    let _e98 = fogType_1;
    if (_e98 == 1i) {
        let _e102 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e102 <= 0f) {
            return 0f;
        }
        let _e104 = viewDepth;
        let _e107 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e104 / _e107), 0f, 1f);
    }
    let _e112 = unnamed.advancedFogColorDensity[3u];
    let _e114 = viewDepth;
    opticalDepth = (max(_e112, 0f) * _e114);
    let _e116 = fogType_1;
    if (_e116 == 2i) {
        let _e118 = opticalDepth;
        return clamp((1f - exp(-(_e118))), 0f, 1f);
    }
    let _e123 = opticalDepth;
    let _e124 = opticalDepth;
    return clamp((1f - exp(-((_e123 * _e124)))), 0f, 1f);
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
    var phi_331_: bool;
    var phi_338_: bool;
    var phi_345_: bool;
    var phi_352_: bool;

    let _e94 = (*c);
    let _e97 = unnamed.cascadeMVP[_e94];
    let _e98 = (*worldPos);
    sc4_ = (_e97 * vec4<f32>(_e98.x, _e98.y, _e98.z, 1f));
    let _e104 = sc4_;
    let _e107 = sc4_[3u];
    sc = (_e104.xyz / vec3(_e107));
    let _e110 = sc;
    let _e114 = ((_e110.xy * 0.5f) + vec2(0.5f));
    sc[0u] = _e114.x;
    sc[1u] = _e114.y;
    let _e120 = sc[0u];
    let _e121 = (_e120 < 0f);
    phi_331_ = _e121;
    if !(_e121) {
        let _e124 = sc[0u];
        phi_331_ = (_e124 > 1f);
    }
    let _e127 = phi_331_;
    phi_338_ = _e127;
    if !(_e127) {
        let _e130 = sc[1u];
        phi_338_ = (_e130 < 0f);
    }
    let _e133 = phi_338_;
    phi_345_ = _e133;
    if !(_e133) {
        let _e136 = sc[1u];
        phi_345_ = (_e136 > 1f);
    }
    let _e139 = phi_345_;
    phi_352_ = _e139;
    if !(_e139) {
        let _e142 = sc[2u];
        phi_352_ = (_e142 > 1f);
    }
    let _e145 = phi_352_;
    if _e145 {
        return 1f;
    }
    let _e146 = (*c);
    layer = f32(_e146);
    let _e148 = textureDimensions(shadowMap, 0i);
    texelSize = (vec2(1f) / vec2<f32>(vec2<i32>(_e148).xy));
    let _e155 = sc[2u];
    currentDepth = (_e155 - shadow_bias);
    shadow = 0f;
    if override_type_3_ {
        let _e157 = currentDepth;
        let _e158 = sc;
        let _e159 = _e158.xy;
        let _e160 = layer;
        let _e163 = vec3<f32>(_e159.x, _e159.y, _e160);
        let _e169 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e163.x, _e163.y), i32(_e163.z));
        shadow = step(_e157, _e169.x);
    } else {
        if override_type_3_1 {
            let _e172 = currentDepth;
            let _e173 = sc;
            let _e174 = _e173.xy;
            let _e175 = layer;
            let _e178 = vec3<f32>(_e174.x, _e174.y, _e175);
            let _e184 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e178.x, _e178.y), i32(_e178.z));
            let _e187 = shadow;
            shadow = (_e187 + step(_e172, _e184.x));
            let _e189 = currentDepth;
            let _e190 = sc;
            let _e193 = texelSize[0u];
            let _e195 = (_e190.xy + vec2<f32>(_e193, 0f));
            let _e196 = layer;
            let _e199 = vec3<f32>(_e195.x, _e195.y, _e196);
            let _e205 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e199.x, _e199.y), i32(_e199.z));
            let _e208 = shadow;
            shadow = (_e208 + step(_e189, _e205.x));
            let _e210 = currentDepth;
            let _e211 = sc;
            let _e214 = texelSize[0u];
            let _e216 = (_e211.xy - vec2<f32>(_e214, 0f));
            let _e217 = layer;
            let _e220 = vec3<f32>(_e216.x, _e216.y, _e217);
            let _e226 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e220.x, _e220.y), i32(_e220.z));
            let _e229 = shadow;
            shadow = (_e229 + step(_e210, _e226.x));
            let _e231 = currentDepth;
            let _e232 = sc;
            let _e235 = texelSize[1u];
            let _e237 = (_e232.xy + vec2<f32>(0f, _e235));
            let _e238 = layer;
            let _e241 = vec3<f32>(_e237.x, _e237.y, _e238);
            let _e247 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e241.x, _e241.y), i32(_e241.z));
            let _e250 = shadow;
            shadow = (_e250 + step(_e231, _e247.x));
            let _e252 = currentDepth;
            let _e253 = sc;
            let _e256 = texelSize[1u];
            let _e258 = (_e253.xy - vec2<f32>(0f, _e256));
            let _e259 = layer;
            let _e262 = vec3<f32>(_e258.x, _e258.y, _e259);
            let _e268 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e262.x, _e262.y), i32(_e262.z));
            let _e271 = shadow;
            shadow = (_e271 + step(_e252, _e268.x));
            let _e273 = shadow;
            shadow = (_e273 / 5f);
        } else {
            x = -1i;
            loop {
                let _e275 = x;
                if (_e275 <= 1i) {
                    y = -1i;
                    loop {
                        let _e277 = y;
                        if (_e277 <= 1i) {
                            let _e279 = currentDepth;
                            let _e280 = sc;
                            let _e282 = x;
                            let _e284 = y;
                            let _e287 = texelSize;
                            let _e289 = (_e280.xy + (vec2<f32>(f32(_e282), f32(_e284)) * _e287));
                            let _e290 = layer;
                            let _e293 = vec3<f32>(_e289.x, _e289.y, _e290);
                            let _e299 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e293.x, _e293.y), i32(_e293.z));
                            let _e302 = shadow;
                            shadow = (_e302 + step(_e279, _e299.x));
                            continue;
                        } else {
                            break;
                        }
                        continuing {
                            let _e304 = y;
                            y = (_e304 + 1i);
                        }
                    }
                    continue;
                } else {
                    break;
                }
                continuing {
                    let _e306 = x;
                    x = (_e306 + 1i);
                }
            }
            let _e308 = shadow;
            shadow = (_e308 / 9f);
        }
    }
    let _e310 = shadow;
    return _e310;
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

    let _e101 = unnamed.cascadeSplits;
    let _e102 = (*viewDepth_1);
    cmp = step(_e101, vec4(_e102));
    let _e106 = cmp[0u];
    let _e108 = cmp[1u];
    let _e111 = cmp[2u];
    let _e114 = cmp[3u];
    cascade = min(i32((((_e106 + _e108) + _e111) + _e114)), 3i);
    let _e118 = cascade;
    (*outCascade) = _e118;
    let _e119 = cascade;
    if (_e119 == 0i) {
        local = 0f;
    } else {
        let _e121 = cascade;
        let _e126 = unnamed.cascadeSplits[max((_e121 - 1i), 0i)];
        local = _e126;
    }
    let _e127 = local;
    prevSplit = _e127;
    let _e128 = cascade;
    let _e131 = unnamed.cascadeSplits[_e128];
    farSplit = _e131;
    let _e132 = farSplit;
    let _e133 = prevSplit;
    blendRange = max((0.1f * (_e132 - _e133)), 1f);
    let _e137 = farSplit;
    let _e138 = (*viewDepth_1);
    let _e140 = blendRange;
    blendT = clamp(((_e137 - _e138) / _e140), 0f, 1f);
    let _e143 = cascade;
    param = _e143;
    let _e144 = (*worldPos_1);
    param_1 = _e144;
    let _e145 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param), (&param_1));
    s0_ = _e145;
    let _e146 = cascade;
    param_2 = min((_e146 + 1i), 3i);
    let _e149 = (*worldPos_1);
    param_3 = _e149;
    let _e150 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param_2), (&param_3));
    s1_ = _e150;
    let _e151 = s1_;
    let _e152 = s0_;
    let _e153 = blendT;
    return mix(_e151, _e152, _e153);
}

fn wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b(rgb: ptr<function, vec3<f32>>, lm_uv: ptr<function, vec2<f32>>) -> vec3<f32> {
    var sun_mask: f32;
    var shadow_1: f32;
    var ignoredCascade: i32;
    var param_4: vec3<f32>;
    var param_5: f32;
    var param_6: i32;

    let _e95 = unnamed.packed_indices[1i][3u];
    let _e101 = unnamed.packed_indices[1i][3u];
    let _e106 = (*lm_uv);
    let _e107 = textureSample(wired_bindless_images[(_e95 & 4095u)], wired_bindless_samplers[((_e101 >> bitcast<u32>(12i)) & 255u)], _e106);
    sun_mask = _e107.x;
    let _e109 = sun_mask;
    if (_e109 > 0.001f) {
        let _e111 = shadowData_1;
        param_4 = _e111.xyz;
        let _e114 = shadowData_1[3u];
        param_5 = _e114;
        let _e115 = sampleShadow_u0028_vf3_u003b_f1_u003b_i1_u003b((&param_4), (&param_5), (&param_6));
        let _e116 = param_6;
        ignoredCascade = _e116;
        shadow_1 = _e115;
        let _e117 = shadow_1;
        let _e118 = sun_mask;
        let _e120 = (*rgb);
        (*rgb) = (_e120 * mix(1f, _e117, _e118));
    }
    let _e122 = (*rgb);
    return _e122;
}

fn wired_apply_sun_shadow_operand_u0028_vf3_u003b(rgb_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var param_7: vec3<f32>;
    var param_8: vec2<f32>;
    var param_9: vec3<f32>;
    var param_10: vec2<f32>;

    if override_type_3_2 {
        let _e89 = (*rgb_1);
        param_7 = _e89;
        let _e90 = frag_tex_coord0_1;
        param_8 = _e90;
        let _e91 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_7), (&param_8));
        return _e91;
    }
    if override_type_3_3 {
        let _e92 = (*rgb_1);
        param_9 = _e92;
        let _e93 = frag_tex_coord1_1;
        param_10 = _e93;
        let _e94 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_9), (&param_10));
        return _e94;
    }
    let _e95 = (*rgb_1);
    return _e95;
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb_2: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e87 = (*rgb_2);
    let _e90 = unnamed.worldLightParams[0u];
    boosted = (_e87 * _e90);
    let _e93 = boosted[0u];
    let _e95 = boosted[1u];
    let _e97 = boosted[2u];
    peak = max(_e93, max(_e95, _e97));
    let _e100 = peak;
    if (_e100 > 1f) {
        let _e102 = peak;
        let _e103 = boosted;
        boosted = (_e103 / vec3(_e102));
    }
    let _e106 = boosted;
    return _e106;
}

fn sRGBToLinear_u0028_vf3_u003b(c_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e88 = (*c_1);
    (*c_1) = max(_e88, vec3<f32>(0f, 0f, 0f));
    let _e90 = (*c_1);
    cutoff = (_e90 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e92 = (*c_1);
    lo = (_e92 / vec3(12.92f));
    let _e95 = (*c_1);
    hi = pow(((_e95 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e100 = hi;
    let _e101 = lo;
    let _e102 = cutoff;
    return mix(_e100, _e101, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e102));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_2: vec4<f32>;
    var param_11: vec3<f32>;
    var param_12: vec3<f32>;

    let _e90 = (*role);
    let _e92 = (*role);
    let _e97 = unnamed.packed_indices[(_e90 / 4u)][(_e92 % 4u)];
    let _e100 = (*role);
    let _e102 = (*role);
    let _e107 = unnamed.packed_indices[(_e100 / 4u)][(_e102 % 4u)];
    let _e112 = (*uv);
    let _e113 = textureSample(wired_bindless_images[(_e97 & 4095u)], wired_bindless_samplers[((_e107 >> bitcast<u32>(12i)) & 255u)], _e112);
    c_2 = _e113;
    let _e114 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e114))) == 0i) {
        let _e119 = c_2;
        param_11 = _e119.xyz;
        let _e121 = sRGBToLinear_u0028_vf3_u003b((&param_11));
        c_2[0u] = _e121.x;
        c_2[1u] = _e121.y;
        c_2[2u] = _e121.z;
    }
    let _e128 = (*slot);
    if (lightmap_slot == (_e128 + 1i)) {
        let _e131 = c_2;
        param_12 = _e131.xyz;
        let _e133 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_12));
        c_2[0u] = _e133.x;
        c_2[1u] = _e133.y;
        c_2[2u] = _e133.z;
    }
    let _e140 = c_2;
    return _e140;
}

fn main_1() {
    var frag_color0_: vec4<f32>;
    var param_13: vec3<f32>;
    var frag_color1_: vec4<f32>;
    var param_14: vec3<f32>;
    var color0_: vec4<f32>;
    var param_15: u32;
    var param_16: vec2<f32>;
    var param_17: i32;
    var color1_: vec4<f32>;
    var param_18: u32;
    var param_19: vec2<f32>;
    var param_20: i32;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var param_21: u32;
    var param_22: vec2<f32>;
    var param_23: i32;
    var color1_2: vec4<f32>;
    var param_24: u32;
    var param_25: vec2<f32>;
    var param_26: i32;
    var color1_3: vec4<f32>;
    var param_27: u32;
    var param_28: vec2<f32>;
    var param_29: i32;
    var color1_4: vec4<f32>;
    var param_30: u32;
    var param_31: vec2<f32>;
    var param_32: i32;
    var color1_5: vec4<f32>;
    var param_33: u32;
    var param_34: vec2<f32>;
    var param_35: i32;
    var color1_6: vec4<f32>;
    var param_36: u32;
    var param_37: vec2<f32>;
    var param_38: i32;
    var param_39: vec3<f32>;
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var fogAmount: f32;

    let _e126 = frag_color0In_1;
    param_13 = _e126.xyz;
    let _e128 = sRGBToLinear_u0028_vf3_u003b((&param_13));
    let _e130 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e128.x, _e128.y, _e128.z, _e130);
    let _e135 = frag_color1In_1;
    param_14 = _e135.xyz;
    let _e137 = sRGBToLinear_u0028_vf3_u003b((&param_14));
    let _e139 = frag_color1In_1[3u];
    frag_color1_ = vec4<f32>(_e137.x, _e137.y, _e137.z, _e139);
    param_15 = 0u;
    let _e144 = frag_tex_coord0_1;
    param_16 = _e144;
    param_17 = 0i;
    let _e145 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_15), (&param_16), (&param_17));
    let _e146 = frag_color0_;
    color0_ = (_e145 * _e146);
    if override_type_3_6 {
        param_18 = 1u;
        let _e148 = frag_tex_coord1_1;
        param_19 = _e148;
        param_20 = 1i;
        let _e149 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_18), (&param_19), (&param_20));
        let _e150 = frag_color1_;
        color1_ = (_e149 * _e150);
        let _e152 = color0_;
        let _e154 = color1_;
        let _e156 = (_e152.xyz + _e154.xyz);
        let _e158 = color0_[3u];
        let _e160 = color1_[3u];
        base = vec4<f32>(_e156.x, _e156.y, _e156.z, (_e158 * _e160));
    } else {
        if override_type_3_7 {
            param_21 = 1u;
            let _e166 = frag_tex_coord1_1;
            param_22 = _e166;
            param_23 = 1i;
            let _e167 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_21), (&param_22), (&param_23));
            let _e168 = frag_color1_;
            color1_1 = (_e167 * _e168);
            let _e171 = color0_[3u];
            let _e172 = color0_;
            color0_ = (_e172 * _e171);
            let _e175 = color1_1[3u];
            let _e176 = color1_1;
            color1_1 = (_e176 * _e175);
            let _e178 = color0_;
            let _e180 = color1_1;
            let _e182 = (_e178.xyz + _e180.xyz);
            let _e184 = color0_[3u];
            let _e186 = color1_1[3u];
            base = vec4<f32>(_e182.x, _e182.y, _e182.z, (_e184 * _e186));
        } else {
            if override_type_3_8 {
                param_24 = 1u;
                let _e192 = frag_tex_coord1_1;
                param_25 = _e192;
                param_26 = 1i;
                let _e193 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_24), (&param_25), (&param_26));
                let _e194 = frag_color1_;
                color1_2 = (_e193 * _e194);
                let _e197 = color0_[3u];
                let _e199 = color0_;
                color0_ = (_e199 * (1f - _e197));
                let _e202 = color1_2[3u];
                let _e204 = color1_2;
                color1_2 = (_e204 * (1f - _e202));
                let _e206 = color0_;
                let _e208 = color1_2;
                let _e210 = (_e206.xyz + _e208.xyz);
                let _e212 = color0_[3u];
                let _e214 = color1_2[3u];
                base = vec4<f32>(_e210.x, _e210.y, _e210.z, (_e212 * _e214));
            } else {
                if override_type_3_9 {
                    param_27 = 1u;
                    let _e220 = frag_tex_coord1_1;
                    param_28 = _e220;
                    param_29 = 1i;
                    let _e221 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_27), (&param_28), (&param_29));
                    let _e222 = frag_color1_;
                    color1_3 = (_e221 * _e222);
                    let _e224 = color0_;
                    let _e225 = color1_3;
                    let _e227 = color1_3[3u];
                    base = mix(_e224, _e225, vec4(_e227));
                } else {
                    if override_type_3_10 {
                        param_30 = 1u;
                        let _e230 = frag_tex_coord1_1;
                        param_31 = _e230;
                        param_32 = 1i;
                        let _e231 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_30), (&param_31), (&param_32));
                        let _e232 = frag_color1_;
                        color1_4 = (_e231 * _e232);
                        let _e234 = color1_4;
                        let _e235 = color0_;
                        let _e237 = color1_4[3u];
                        base = mix(_e234, _e235, vec4(_e237));
                    } else {
                        if override_type_3_11 {
                            param_33 = 1u;
                            let _e240 = frag_tex_coord1_1;
                            param_34 = _e240;
                            param_35 = 1i;
                            let _e241 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_33), (&param_34), (&param_35));
                            let _e242 = frag_color1_;
                            color1_5 = (_e241 * _e242);
                            let _e244 = color1_5;
                            let _e246 = color1_5[3u];
                            let _e249 = color0_;
                            base = ((_e244 + vec4(_e246)) * _e249);
                        } else {
                            param_36 = 1u;
                            let _e251 = frag_tex_coord1_1;
                            param_37 = _e251;
                            param_38 = 1i;
                            let _e252 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_36), (&param_37), (&param_38));
                            let _e253 = frag_color1_;
                            color1_6 = (_e252 * _e253);
                            let _e255 = color0_;
                            let _e257 = color1_6;
                            let _e259 = (_e255.xyz * _e257.xyz);
                            base[0u] = _e259.x;
                            base[1u] = _e259.y;
                            base[2u] = _e259.z;
                            let _e267 = color0_[3u];
                            let _e269 = color1_6[3u];
                            base[3u] = (_e267 * _e269);
                        }
                    }
                }
            }
        }
    }
    let _e272 = base;
    param_39 = _e272.xyz;
    let _e274 = wired_apply_sun_shadow_operand_u0028_vf3_u003b((&param_39));
    base[0u] = _e274.x;
    base[1u] = _e274.y;
    base[2u] = _e274.z;
    if override_type_3_12 {
        let _e283 = unnamed.worldLightParams[1u];
        wetness = clamp(_e283, 0f, 1f);
        let _e287 = unnamed.worldLightParams[2u];
        frost = clamp(_e287, 0f, 1f);
        let _e289 = base;
        luminance = dot(_e289.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e292 = wetness;
        let _e294 = base;
        let _e296 = (_e294.xyz * mix(1f, 0.82f, _e292));
        base[0u] = _e296.x;
        base[1u] = _e296.y;
        base[2u] = _e296.z;
        let _e303 = base;
        let _e305 = luminance;
        let _e307 = luminance;
        let _e309 = luminance;
        let _e311 = frost;
        let _e314 = mix(_e303.xyz, vec3<f32>((_e305 * 0.88f), (_e307 * 0.94f), _e309), vec3((_e311 * 0.55f)));
        base[0u] = _e314.x;
        base[1u] = _e314.y;
        base[2u] = _e314.z;
    }
    let _e321 = color0_;
    let _e324 = unnamed.emissionRadiance;
    let _e327 = base;
    let _e329 = (_e327.xyz + (_e321.xyz * _e324.xyz));
    base[0u] = _e329.x;
    base[1u] = _e329.y;
    base[2u] = _e329.z;
    let _e336 = wired_advanced_fog_enabled_u0028_();
    if _e336 {
        let _e337 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e337;
        let _e338 = base;
        let _e341 = unnamed.advancedFogColorDensity;
        let _e343 = fogAmount;
        let _e345 = mix(_e338.xyz, _e341.xyz, vec3(_e343));
        base[0u] = _e345.x;
        base[1u] = _e345.y;
        base[2u] = _e345.z;
    }
    if override_type_3_13 {
        let _e353 = base[3u];
        if (_e353 == 0f) {
            discard;
        }
    } else {
        if override_type_3_14 {
            let _e355 = base;
            let _e357 = base;
            if (dot(_e355.xyz, _e357.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e361 = base;
    out_color = _e361;
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(7) shadowData: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(0) frag_color0In: vec4<f32>, @location(5) frag_color1In: vec4<f32>) -> @location(0) vec4<f32> {
    gl_FragCoord_1 = gl_FragCoord;
    shadowData_1 = shadowData;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    frag_color0In_1 = frag_color0In;
    frag_color1In_1 = frag_color1In;
    main_1();
    let _e13 = out_color;
    return _e13;
}
