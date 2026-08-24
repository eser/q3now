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
override override_type_3_4: bool = (lightmap_slot == 3i);
@id(6) override tex_mode: i32 = 0i;
override override_type_3_5: bool = (tex_mode == 1i);
override override_type_3_6: bool = (tex_mode == 2i);
override override_type_3_7: bool = (override_type_3_5 || override_type_3_6);
override override_type_3_8: bool = (tex_mode == 3i);
override override_type_3_9: bool = (tex_mode == 4i);
override override_type_3_10: bool = (tex_mode == 5i);
override override_type_3_11: bool = (tex_mode == 6i);
override override_type_3_12: bool = (tex_mode == 7i);
@id(10) override acff: i32 = 0i;
override override_type_3_13: bool = (acff == 1i);
override override_type_3_14: bool = (acff == 2i);
override override_type_3_15: bool = (acff == 3i);
override override_type_3_16: bool = (acff == 1i);
override override_type_3_17: bool = (acff == 2i);
override override_type_3_18: bool = (acff == 3i);
@id(7) override discard_mode: i32 = 0i;
override override_type_3_19: bool = (discard_mode == 1i);
override override_type_3_20: bool = (discard_mode == 2i);
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
var<private> frag_tex_coord2_1: vec2<f32>;
var<private> fog_tex_coord_1: vec2<f32>;
var<private> frag_color0In_1: vec4<f32>;
var<private> frag_color1In_1: vec4<f32>;
var<private> frag_color2In_1: vec4<f32>;
var<private> out_color: vec4<f32>;

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e88 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e88 + 0.5f));
    let _e93 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e95 = fogType;
    let _e98 = fogType;
    return (((_e93 > 0.5f) && (_e95 >= 1i)) && (_e98 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e88 = wired_advanced_fog_enabled_u0028_();
    if !(_e88) {
        return 0f;
    }
    let _e91 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e91, 0.000001f));
    let _e96 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e96 + 0.5f));
    let _e99 = fogType_1;
    if (_e99 == 1i) {
        let _e103 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e103 <= 0f) {
            return 0f;
        }
        let _e105 = viewDepth;
        let _e108 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e105 / _e108), 0f, 1f);
    }
    let _e113 = unnamed.advancedFogColorDensity[3u];
    let _e115 = viewDepth;
    opticalDepth = (max(_e113, 0f) * _e115);
    let _e117 = fogType_1;
    if (_e117 == 2i) {
        let _e119 = opticalDepth;
        return clamp((1f - exp(-(_e119))), 0f, 1f);
    }
    let _e124 = opticalDepth;
    let _e125 = opticalDepth;
    return clamp((1f - exp(-((_e124 * _e125)))), 0f, 1f);
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

    let _e95 = (*c);
    let _e98 = unnamed.cascadeMVP[_e95];
    let _e99 = (*worldPos);
    sc4_ = (_e98 * vec4<f32>(_e99.x, _e99.y, _e99.z, 1f));
    let _e105 = sc4_;
    let _e108 = sc4_[3u];
    sc = (_e105.xyz / vec3(_e108));
    let _e111 = sc;
    let _e115 = ((_e111.xy * 0.5f) + vec2(0.5f));
    sc[0u] = _e115.x;
    sc[1u] = _e115.y;
    let _e121 = sc[0u];
    let _e122 = (_e121 < 0f);
    phi_304_ = _e122;
    if !(_e122) {
        let _e125 = sc[0u];
        phi_304_ = (_e125 > 1f);
    }
    let _e128 = phi_304_;
    phi_311_ = _e128;
    if !(_e128) {
        let _e131 = sc[1u];
        phi_311_ = (_e131 < 0f);
    }
    let _e134 = phi_311_;
    phi_318_ = _e134;
    if !(_e134) {
        let _e137 = sc[1u];
        phi_318_ = (_e137 > 1f);
    }
    let _e140 = phi_318_;
    phi_325_ = _e140;
    if !(_e140) {
        let _e143 = sc[2u];
        phi_325_ = (_e143 > 1f);
    }
    let _e146 = phi_325_;
    if _e146 {
        return 1f;
    }
    let _e147 = (*c);
    layer = f32(_e147);
    let _e149 = textureDimensions(shadowMap, 0i);
    texelSize = (vec2(1f) / vec2<f32>(vec2<i32>(_e149).xy));
    let _e156 = sc[2u];
    currentDepth = (_e156 - shadow_bias);
    shadow = 0f;
    if override_type_3_ {
        let _e158 = currentDepth;
        let _e159 = sc;
        let _e160 = _e159.xy;
        let _e161 = layer;
        let _e164 = vec3<f32>(_e160.x, _e160.y, _e161);
        let _e170 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e164.x, _e164.y), i32(_e164.z));
        shadow = step(_e158, _e170.x);
    } else {
        if override_type_3_1 {
            let _e173 = currentDepth;
            let _e174 = sc;
            let _e175 = _e174.xy;
            let _e176 = layer;
            let _e179 = vec3<f32>(_e175.x, _e175.y, _e176);
            let _e185 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e179.x, _e179.y), i32(_e179.z));
            let _e188 = shadow;
            shadow = (_e188 + step(_e173, _e185.x));
            let _e190 = currentDepth;
            let _e191 = sc;
            let _e194 = texelSize[0u];
            let _e196 = (_e191.xy + vec2<f32>(_e194, 0f));
            let _e197 = layer;
            let _e200 = vec3<f32>(_e196.x, _e196.y, _e197);
            let _e206 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e200.x, _e200.y), i32(_e200.z));
            let _e209 = shadow;
            shadow = (_e209 + step(_e190, _e206.x));
            let _e211 = currentDepth;
            let _e212 = sc;
            let _e215 = texelSize[0u];
            let _e217 = (_e212.xy - vec2<f32>(_e215, 0f));
            let _e218 = layer;
            let _e221 = vec3<f32>(_e217.x, _e217.y, _e218);
            let _e227 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e221.x, _e221.y), i32(_e221.z));
            let _e230 = shadow;
            shadow = (_e230 + step(_e211, _e227.x));
            let _e232 = currentDepth;
            let _e233 = sc;
            let _e236 = texelSize[1u];
            let _e238 = (_e233.xy + vec2<f32>(0f, _e236));
            let _e239 = layer;
            let _e242 = vec3<f32>(_e238.x, _e238.y, _e239);
            let _e248 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e242.x, _e242.y), i32(_e242.z));
            let _e251 = shadow;
            shadow = (_e251 + step(_e232, _e248.x));
            let _e253 = currentDepth;
            let _e254 = sc;
            let _e257 = texelSize[1u];
            let _e259 = (_e254.xy - vec2<f32>(0f, _e257));
            let _e260 = layer;
            let _e263 = vec3<f32>(_e259.x, _e259.y, _e260);
            let _e269 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e263.x, _e263.y), i32(_e263.z));
            let _e272 = shadow;
            shadow = (_e272 + step(_e253, _e269.x));
            let _e274 = shadow;
            shadow = (_e274 / 5f);
        } else {
            x = -1i;
            loop {
                let _e276 = x;
                if (_e276 <= 1i) {
                    y = -1i;
                    loop {
                        let _e278 = y;
                        if (_e278 <= 1i) {
                            let _e280 = currentDepth;
                            let _e281 = sc;
                            let _e283 = x;
                            let _e285 = y;
                            let _e288 = texelSize;
                            let _e290 = (_e281.xy + (vec2<f32>(f32(_e283), f32(_e285)) * _e288));
                            let _e291 = layer;
                            let _e294 = vec3<f32>(_e290.x, _e290.y, _e291);
                            let _e300 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e294.x, _e294.y), i32(_e294.z));
                            let _e303 = shadow;
                            shadow = (_e303 + step(_e280, _e300.x));
                            continue;
                        } else {
                            break;
                        }
                        continuing {
                            let _e305 = y;
                            y = (_e305 + 1i);
                        }
                    }
                    continue;
                } else {
                    break;
                }
                continuing {
                    let _e307 = x;
                    x = (_e307 + 1i);
                }
            }
            let _e309 = shadow;
            shadow = (_e309 / 9f);
        }
    }
    let _e311 = shadow;
    return _e311;
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

    let _e102 = unnamed.cascadeSplits;
    let _e103 = (*viewDepth_1);
    cmp = step(_e102, vec4(_e103));
    let _e107 = cmp[0u];
    let _e109 = cmp[1u];
    let _e112 = cmp[2u];
    let _e115 = cmp[3u];
    cascade = min(i32((((_e107 + _e109) + _e112) + _e115)), 3i);
    let _e119 = cascade;
    (*outCascade) = _e119;
    let _e120 = cascade;
    if (_e120 == 0i) {
        local = 0f;
    } else {
        let _e122 = cascade;
        let _e127 = unnamed.cascadeSplits[max((_e122 - 1i), 0i)];
        local = _e127;
    }
    let _e128 = local;
    prevSplit = _e128;
    let _e129 = cascade;
    let _e132 = unnamed.cascadeSplits[_e129];
    farSplit = _e132;
    let _e133 = farSplit;
    let _e134 = prevSplit;
    blendRange = max((0.1f * (_e133 - _e134)), 1f);
    let _e138 = farSplit;
    let _e139 = (*viewDepth_1);
    let _e141 = blendRange;
    blendT = clamp(((_e138 - _e139) / _e141), 0f, 1f);
    let _e144 = cascade;
    param = _e144;
    let _e145 = (*worldPos_1);
    param_1 = _e145;
    let _e146 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param), (&param_1));
    s0_ = _e146;
    let _e147 = cascade;
    param_2 = min((_e147 + 1i), 3i);
    let _e150 = (*worldPos_1);
    param_3 = _e150;
    let _e151 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param_2), (&param_3));
    s1_ = _e151;
    let _e152 = s1_;
    let _e153 = s0_;
    let _e154 = blendT;
    return mix(_e152, _e153, _e154);
}

fn wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b(rgb: ptr<function, vec3<f32>>, lm_uv: ptr<function, vec2<f32>>) -> vec3<f32> {
    var sun_mask: f32;
    var shadow_1: f32;
    var ignoredCascade: i32;
    var param_4: vec3<f32>;
    var param_5: f32;
    var param_6: i32;

    let _e96 = unnamed.packed_indices[1i][3u];
    let _e102 = unnamed.packed_indices[1i][3u];
    let _e107 = (*lm_uv);
    let _e108 = textureSample(wired_bindless_images[(_e96 & 4095u)], wired_bindless_samplers[((_e102 >> bitcast<u32>(12i)) & 255u)], _e107);
    sun_mask = _e108.x;
    let _e110 = sun_mask;
    if (_e110 > 0.001f) {
        let _e112 = shadowData_1;
        param_4 = _e112.xyz;
        let _e115 = shadowData_1[3u];
        param_5 = _e115;
        let _e116 = sampleShadow_u0028_vf3_u003b_f1_u003b_i1_u003b((&param_4), (&param_5), (&param_6));
        let _e117 = param_6;
        ignoredCascade = _e117;
        shadow_1 = _e116;
        let _e118 = shadow_1;
        let _e119 = sun_mask;
        let _e121 = (*rgb);
        (*rgb) = (_e121 * mix(1f, _e118, _e119));
    }
    let _e123 = (*rgb);
    return _e123;
}

fn wired_apply_sun_shadow_operand_u0028_vf3_u003b(rgb_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var param_7: vec3<f32>;
    var param_8: vec2<f32>;
    var param_9: vec3<f32>;
    var param_10: vec2<f32>;
    var param_11: vec3<f32>;
    var param_12: vec2<f32>;

    if override_type_3_2 {
        let _e92 = (*rgb_1);
        param_7 = _e92;
        let _e93 = frag_tex_coord0_1;
        param_8 = _e93;
        let _e94 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_7), (&param_8));
        return _e94;
    }
    if override_type_3_3 {
        let _e95 = (*rgb_1);
        param_9 = _e95;
        let _e96 = frag_tex_coord1_1;
        param_10 = _e96;
        let _e97 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_9), (&param_10));
        return _e97;
    }
    if override_type_3_4 {
        let _e98 = (*rgb_1);
        param_11 = _e98;
        let _e99 = frag_tex_coord2_1;
        param_12 = _e99;
        let _e100 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_11), (&param_12));
        return _e100;
    }
    let _e101 = (*rgb_1);
    return _e101;
}

fn sRGBToLinear_u0028_vf3_u003b(c_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e89 = (*c_1);
    (*c_1) = max(_e89, vec3<f32>(0f, 0f, 0f));
    let _e91 = (*c_1);
    cutoff = (_e91 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e93 = (*c_1);
    lo = (_e93 / vec3(12.92f));
    let _e96 = (*c_1);
    hi = pow(((_e96 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e101 = hi;
    let _e102 = lo;
    let _e103 = cutoff;
    return mix(_e101, _e102, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e103));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_2: vec4<f32>;
    var param_13: vec3<f32>;

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
        param_13 = _e119.xyz;
        let _e121 = sRGBToLinear_u0028_vf3_u003b((&param_13));
        c_2[0u] = _e121.x;
        c_2[1u] = _e121.y;
        c_2[2u] = _e121.z;
    }
    let _e128 = (*slot);
    if (lightmap_slot == (_e128 + 1i)) {
        let _e133 = unnamed.worldLightParams[0u];
        let _e134 = c_2;
        let _e136 = (_e134.xyz * _e133);
        c_2[0u] = _e136.x;
        c_2[1u] = _e136.y;
        c_2[2u] = _e136.z;
    }
    let _e143 = c_2;
    return _e143;
}

fn main_1() {
    var fog: vec4<f32>;
    var frag_color0_: vec4<f32>;
    var param_14: vec3<f32>;
    var frag_color1_: vec4<f32>;
    var param_15: vec3<f32>;
    var frag_color2_: vec4<f32>;
    var param_16: vec3<f32>;
    var color0_: vec4<f32>;
    var param_17: u32;
    var param_18: vec2<f32>;
    var param_19: i32;
    var color1_: vec4<f32>;
    var param_20: u32;
    var param_21: vec2<f32>;
    var param_22: i32;
    var color2_: vec4<f32>;
    var param_23: u32;
    var param_24: vec2<f32>;
    var param_25: i32;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var param_26: u32;
    var param_27: vec2<f32>;
    var param_28: i32;
    var color2_1: vec4<f32>;
    var param_29: u32;
    var param_30: vec2<f32>;
    var param_31: i32;
    var color1_2: vec4<f32>;
    var param_32: u32;
    var param_33: vec2<f32>;
    var param_34: i32;
    var color2_2: vec4<f32>;
    var param_35: u32;
    var param_36: vec2<f32>;
    var param_37: i32;
    var color1_3: vec4<f32>;
    var param_38: u32;
    var param_39: vec2<f32>;
    var param_40: i32;
    var color2_3: vec4<f32>;
    var param_41: u32;
    var param_42: vec2<f32>;
    var param_43: i32;
    var color1_4: vec4<f32>;
    var param_44: u32;
    var param_45: vec2<f32>;
    var param_46: i32;
    var color2_4: vec4<f32>;
    var param_47: u32;
    var param_48: vec2<f32>;
    var param_49: i32;
    var color1_5: vec4<f32>;
    var param_50: u32;
    var param_51: vec2<f32>;
    var param_52: i32;
    var color2_5: vec4<f32>;
    var param_53: u32;
    var param_54: vec2<f32>;
    var param_55: i32;
    var color1_6: vec4<f32>;
    var param_56: u32;
    var param_57: vec2<f32>;
    var param_58: i32;
    var color2_6: vec4<f32>;
    var param_59: u32;
    var param_60: vec2<f32>;
    var param_61: i32;
    var param_62: vec3<f32>;
    var fogAmount: f32;

    let _e158 = unnamed.packed_indices[0i][3u];
    let _e164 = unnamed.packed_indices[0i][3u];
    let _e169 = fog_tex_coord_1;
    let _e170 = textureSample(wired_bindless_images[(_e158 & 4095u)], wired_bindless_samplers[((_e164 >> bitcast<u32>(12i)) & 255u)], _e169);
    fog = _e170;
    let _e171 = frag_color0In_1;
    param_14 = _e171.xyz;
    let _e173 = sRGBToLinear_u0028_vf3_u003b((&param_14));
    let _e175 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e173.x, _e173.y, _e173.z, _e175);
    let _e180 = frag_color1In_1;
    param_15 = _e180.xyz;
    let _e182 = sRGBToLinear_u0028_vf3_u003b((&param_15));
    let _e184 = frag_color1In_1[3u];
    frag_color1_ = vec4<f32>(_e182.x, _e182.y, _e182.z, _e184);
    let _e189 = frag_color2In_1;
    param_16 = _e189.xyz;
    let _e191 = sRGBToLinear_u0028_vf3_u003b((&param_16));
    let _e193 = frag_color2In_1[3u];
    frag_color2_ = vec4<f32>(_e191.x, _e191.y, _e191.z, _e193);
    param_17 = 0u;
    let _e198 = frag_tex_coord0_1;
    param_18 = _e198;
    param_19 = 0i;
    let _e199 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_17), (&param_18), (&param_19));
    let _e200 = frag_color0_;
    color0_ = (_e199 * _e200);
    if override_type_3_7 {
        param_20 = 1u;
        let _e202 = frag_tex_coord1_1;
        param_21 = _e202;
        param_22 = 1i;
        let _e203 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_20), (&param_21), (&param_22));
        let _e204 = frag_color1_;
        color1_ = (_e203 * _e204);
        param_23 = 2u;
        let _e206 = frag_tex_coord2_1;
        param_24 = _e206;
        param_25 = 2i;
        let _e207 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_23), (&param_24), (&param_25));
        let _e208 = frag_color2_;
        color2_ = (_e207 * _e208);
        let _e210 = color0_;
        let _e212 = color1_;
        let _e215 = color2_;
        let _e217 = ((_e210.xyz + _e212.xyz) + _e215.xyz);
        let _e219 = color0_[3u];
        let _e221 = color1_[3u];
        let _e224 = color2_[3u];
        base = vec4<f32>(_e217.x, _e217.y, _e217.z, ((_e219 * _e221) * _e224));
    } else {
        if override_type_3_8 {
            param_26 = 1u;
            let _e230 = frag_tex_coord1_1;
            param_27 = _e230;
            param_28 = 1i;
            let _e231 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_26), (&param_27), (&param_28));
            let _e232 = frag_color1_;
            color1_1 = (_e231 * _e232);
            param_29 = 2u;
            let _e234 = frag_tex_coord2_1;
            param_30 = _e234;
            param_31 = 2i;
            let _e235 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_29), (&param_30), (&param_31));
            let _e236 = frag_color2_;
            color2_1 = (_e235 * _e236);
            let _e239 = color0_[3u];
            let _e240 = color0_;
            color0_ = (_e240 * _e239);
            let _e243 = color1_1[3u];
            let _e244 = color1_1;
            color1_1 = (_e244 * _e243);
            let _e247 = color2_1[3u];
            let _e248 = color2_1;
            color2_1 = (_e248 * _e247);
            let _e250 = color0_;
            let _e252 = color1_1;
            let _e255 = color2_1;
            let _e257 = ((_e250.xyz + _e252.xyz) + _e255.xyz);
            let _e259 = color0_[3u];
            let _e261 = color1_1[3u];
            let _e264 = color2_1[3u];
            base = vec4<f32>(_e257.x, _e257.y, _e257.z, ((_e259 * _e261) * _e264));
        } else {
            if override_type_3_9 {
                param_32 = 1u;
                let _e270 = frag_tex_coord1_1;
                param_33 = _e270;
                param_34 = 1i;
                let _e271 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_32), (&param_33), (&param_34));
                let _e272 = frag_color1_;
                color1_2 = (_e271 * _e272);
                param_35 = 2u;
                let _e274 = frag_tex_coord2_1;
                param_36 = _e274;
                param_37 = 2i;
                let _e275 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_35), (&param_36), (&param_37));
                let _e276 = frag_color2_;
                color2_2 = (_e275 * _e276);
                let _e279 = color0_[3u];
                let _e281 = color0_;
                color0_ = (_e281 * (1f - _e279));
                let _e284 = color1_2[3u];
                let _e286 = color1_2;
                color1_2 = (_e286 * (1f - _e284));
                let _e289 = color2_2[3u];
                let _e291 = color2_2;
                color2_2 = (_e291 * (1f - _e289));
                let _e293 = color0_;
                let _e295 = color1_2;
                let _e298 = color2_2;
                let _e300 = ((_e293.xyz + _e295.xyz) + _e298.xyz);
                let _e302 = color0_[3u];
                let _e304 = color1_2[3u];
                let _e307 = color2_2[3u];
                base = vec4<f32>(_e300.x, _e300.y, _e300.z, ((_e302 * _e304) * _e307));
            } else {
                if override_type_3_10 {
                    param_38 = 1u;
                    let _e313 = frag_tex_coord1_1;
                    param_39 = _e313;
                    param_40 = 1i;
                    let _e314 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_38), (&param_39), (&param_40));
                    let _e315 = frag_color1_;
                    color1_3 = (_e314 * _e315);
                    param_41 = 2u;
                    let _e317 = frag_tex_coord2_1;
                    param_42 = _e317;
                    param_43 = 2i;
                    let _e318 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_41), (&param_42), (&param_43));
                    let _e319 = frag_color2_;
                    color2_3 = (_e318 * _e319);
                    let _e321 = color0_;
                    let _e322 = color1_3;
                    let _e324 = color1_3[3u];
                    let _e327 = color2_3;
                    let _e329 = color2_3[3u];
                    base = mix(mix(_e321, _e322, vec4(_e324)), _e327, vec4(_e329));
                } else {
                    if override_type_3_11 {
                        param_44 = 1u;
                        let _e332 = frag_tex_coord1_1;
                        param_45 = _e332;
                        param_46 = 1i;
                        let _e333 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_44), (&param_45), (&param_46));
                        let _e334 = frag_color1_;
                        color1_4 = (_e333 * _e334);
                        param_47 = 2u;
                        let _e336 = frag_tex_coord2_1;
                        param_48 = _e336;
                        param_49 = 2i;
                        let _e337 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_47), (&param_48), (&param_49));
                        let _e338 = frag_color2_;
                        color2_4 = (_e337 * _e338);
                        let _e340 = color2_4;
                        let _e341 = color1_4;
                        let _e342 = color0_;
                        let _e344 = color1_4[3u];
                        let _e348 = color2_4[3u];
                        base = mix(_e340, mix(_e341, _e342, vec4(_e344)), vec4(_e348));
                    } else {
                        if override_type_3_12 {
                            param_50 = 1u;
                            let _e351 = frag_tex_coord1_1;
                            param_51 = _e351;
                            param_52 = 1i;
                            let _e352 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_50), (&param_51), (&param_52));
                            let _e353 = frag_color1_;
                            color1_5 = (_e352 * _e353);
                            param_53 = 2u;
                            let _e355 = frag_tex_coord2_1;
                            param_54 = _e355;
                            param_55 = 2i;
                            let _e356 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_53), (&param_54), (&param_55));
                            let _e357 = frag_color2_;
                            color2_5 = (_e356 * _e357);
                            let _e359 = color2_5;
                            let _e361 = color2_5[3u];
                            let _e364 = color1_5;
                            let _e366 = color1_5[3u];
                            let _e370 = color0_;
                            base = (((_e359 + vec4(_e361)) * (_e364 + vec4(_e366))) * _e370);
                        } else {
                            param_56 = 1u;
                            let _e372 = frag_tex_coord1_1;
                            param_57 = _e372;
                            param_58 = 1i;
                            let _e373 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_56), (&param_57), (&param_58));
                            let _e374 = frag_color1_;
                            color1_6 = (_e373 * _e374);
                            param_59 = 2u;
                            let _e376 = frag_tex_coord2_1;
                            param_60 = _e376;
                            param_61 = 2i;
                            let _e377 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_59), (&param_60), (&param_61));
                            let _e378 = frag_color2_;
                            color2_6 = (_e377 * _e378);
                            let _e380 = color0_;
                            let _e382 = color1_6;
                            let _e385 = color2_6;
                            let _e387 = ((_e380.xyz * _e382.xyz) * _e385.xyz);
                            base[0u] = _e387.x;
                            base[1u] = _e387.y;
                            base[2u] = _e387.z;
                            let _e395 = color0_[3u];
                            let _e397 = color1_6[3u];
                            let _e400 = color2_6[3u];
                            base[3u] = ((_e395 * _e397) * _e400);
                        }
                    }
                }
            }
        }
    }
    let _e403 = base;
    param_62 = _e403.xyz;
    let _e405 = wired_apply_sun_shadow_operand_u0028_vf3_u003b((&param_62));
    base[0u] = _e405.x;
    base[1u] = _e405.y;
    base[2u] = _e405.z;
    let _e412 = wired_advanced_fog_enabled_u0028_();
    if _e412 {
        let _e413 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e413;
        if override_type_3_13 {
            let _e414 = fogAmount;
            let _e416 = base;
            let _e418 = (_e416.xyz * (1f - _e414));
            base[0u] = _e418.x;
            base[1u] = _e418.y;
            base[2u] = _e418.z;
        } else {
            if override_type_3_14 {
                let _e425 = fogAmount;
                let _e427 = base;
                base = (_e427 * (1f - _e425));
            } else {
                if override_type_3_15 {
                    let _e429 = fogAmount;
                    let _e432 = base[3u];
                    base[3u] = (_e432 * (1f - _e429));
                } else {
                    let _e435 = base;
                    let _e438 = unnamed.advancedFogColorDensity;
                    let _e440 = fogAmount;
                    let _e442 = mix(_e435.xyz, _e438.xyz, vec3(_e440));
                    base[0u] = _e442.x;
                    base[1u] = _e442.y;
                    base[2u] = _e442.z;
                }
            }
        }
    } else {
        if override_type_3_16 {
            let _e449 = base;
            let _e452 = fog[3u];
            let _e454 = (_e449.xyz * (1f - _e452));
            base[0u] = _e454.x;
            base[1u] = _e454.y;
            base[2u] = _e454.z;
        } else {
            if override_type_3_17 {
                let _e461 = base;
                let _e463 = fog[3u];
                base = (_e461 * (1f - _e463));
            } else {
                if override_type_3_18 {
                    let _e467 = base[3u];
                    let _e469 = fog[3u];
                    base[3u] = (_e467 * (1f - _e469));
                } else {
                    let _e473 = base;
                    let _e474 = fog;
                    let _e476 = unnamed.fogColor;
                    let _e479 = fog[3u];
                    base = mix(_e473, (_e474 * _e476), vec4(_e479));
                }
            }
        }
    }
    if override_type_3_19 {
        let _e483 = base[3u];
        if (_e483 == 0f) {
            discard;
        }
    } else {
        if override_type_3_20 {
            let _e485 = base;
            let _e487 = base;
            if (dot(_e485.xyz, _e487.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e491 = base;
    out_color = _e491;
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(7) shadowData: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(3) frag_tex_coord2_: vec2<f32>, @location(4) fog_tex_coord: vec2<f32>, @location(0) frag_color0In: vec4<f32>, @location(5) frag_color1In: vec4<f32>, @location(6) frag_color2In: vec4<f32>) -> @location(0) vec4<f32> {
    gl_FragCoord_1 = gl_FragCoord;
    shadowData_1 = shadowData;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    frag_tex_coord2_1 = frag_tex_coord2_;
    fog_tex_coord_1 = fog_tex_coord;
    frag_color0In_1 = frag_color0In;
    frag_color1In_1 = frag_color1In;
    frag_color2In_1 = frag_color2In;
    main_1();
    let _e19 = out_color;
    return _e19;
}
