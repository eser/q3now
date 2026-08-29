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
override override_type_3_13: bool = (lightmap_slot != 0i);
@id(7) override discard_mode: i32 = 0i;
override override_type_3_14: bool = (discard_mode == 1i);
override override_type_3_15: bool = (discard_mode == 2i);
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
var<private> frag_color0In_1: vec4<f32>;
var<private> frag_color1In_1: vec4<f32>;
var<private> frag_color2In_1: vec4<f32>;
var<private> out_color: vec4<f32>;

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e90 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e90 + 0.5f));
    let _e95 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e97 = fogType;
    let _e100 = fogType;
    return (((_e95 > 0.5f) && (_e97 >= 1i)) && (_e100 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e90 = wired_advanced_fog_enabled_u0028_();
    if !(_e90) {
        return 0f;
    }
    let _e93 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e93, 0.000001f));
    let _e98 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e98 + 0.5f));
    let _e101 = fogType_1;
    if (_e101 == 1i) {
        let _e105 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e105 <= 0f) {
            return 0f;
        }
        let _e107 = viewDepth;
        let _e110 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e107 / _e110), 0f, 1f);
    }
    let _e115 = unnamed.advancedFogColorDensity[3u];
    let _e117 = viewDepth;
    opticalDepth = (max(_e115, 0f) * _e117);
    let _e119 = fogType_1;
    if (_e119 == 2i) {
        let _e121 = opticalDepth;
        return clamp((1f - exp(-(_e121))), 0f, 1f);
    }
    let _e126 = opticalDepth;
    let _e127 = opticalDepth;
    return clamp((1f - exp(-((_e126 * _e127)))), 0f, 1f);
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

    let _e97 = (*c);
    let _e100 = unnamed.cascadeMVP[_e97];
    let _e101 = (*worldPos);
    sc4_ = (_e100 * vec4<f32>(_e101.x, _e101.y, _e101.z, 1f));
    let _e107 = sc4_;
    let _e110 = sc4_[3u];
    sc = (_e107.xyz / vec3(_e110));
    let _e113 = sc;
    let _e117 = ((_e113.xy * 0.5f) + vec2(0.5f));
    sc[0u] = _e117.x;
    sc[1u] = _e117.y;
    let _e123 = sc[0u];
    let _e124 = (_e123 < 0f);
    phi_304_ = _e124;
    if !(_e124) {
        let _e127 = sc[0u];
        phi_304_ = (_e127 > 1f);
    }
    let _e130 = phi_304_;
    phi_311_ = _e130;
    if !(_e130) {
        let _e133 = sc[1u];
        phi_311_ = (_e133 < 0f);
    }
    let _e136 = phi_311_;
    phi_318_ = _e136;
    if !(_e136) {
        let _e139 = sc[1u];
        phi_318_ = (_e139 > 1f);
    }
    let _e142 = phi_318_;
    phi_325_ = _e142;
    if !(_e142) {
        let _e145 = sc[2u];
        phi_325_ = (_e145 > 1f);
    }
    let _e148 = phi_325_;
    if _e148 {
        return 1f;
    }
    let _e149 = (*c);
    layer = f32(_e149);
    let _e151 = textureDimensions(shadowMap, 0i);
    texelSize = (vec2(1f) / vec2<f32>(vec2<i32>(_e151).xy));
    let _e158 = sc[2u];
    currentDepth = (_e158 - shadow_bias);
    shadow = 0f;
    if override_type_3_ {
        let _e160 = currentDepth;
        let _e161 = sc;
        let _e162 = _e161.xy;
        let _e163 = layer;
        let _e166 = vec3<f32>(_e162.x, _e162.y, _e163);
        let _e172 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e166.x, _e166.y), i32(_e166.z));
        shadow = step(_e160, _e172.x);
    } else {
        if override_type_3_1 {
            let _e175 = currentDepth;
            let _e176 = sc;
            let _e177 = _e176.xy;
            let _e178 = layer;
            let _e181 = vec3<f32>(_e177.x, _e177.y, _e178);
            let _e187 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e181.x, _e181.y), i32(_e181.z));
            let _e190 = shadow;
            shadow = (_e190 + step(_e175, _e187.x));
            let _e192 = currentDepth;
            let _e193 = sc;
            let _e196 = texelSize[0u];
            let _e198 = (_e193.xy + vec2<f32>(_e196, 0f));
            let _e199 = layer;
            let _e202 = vec3<f32>(_e198.x, _e198.y, _e199);
            let _e208 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e202.x, _e202.y), i32(_e202.z));
            let _e211 = shadow;
            shadow = (_e211 + step(_e192, _e208.x));
            let _e213 = currentDepth;
            let _e214 = sc;
            let _e217 = texelSize[0u];
            let _e219 = (_e214.xy - vec2<f32>(_e217, 0f));
            let _e220 = layer;
            let _e223 = vec3<f32>(_e219.x, _e219.y, _e220);
            let _e229 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e223.x, _e223.y), i32(_e223.z));
            let _e232 = shadow;
            shadow = (_e232 + step(_e213, _e229.x));
            let _e234 = currentDepth;
            let _e235 = sc;
            let _e238 = texelSize[1u];
            let _e240 = (_e235.xy + vec2<f32>(0f, _e238));
            let _e241 = layer;
            let _e244 = vec3<f32>(_e240.x, _e240.y, _e241);
            let _e250 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e244.x, _e244.y), i32(_e244.z));
            let _e253 = shadow;
            shadow = (_e253 + step(_e234, _e250.x));
            let _e255 = currentDepth;
            let _e256 = sc;
            let _e259 = texelSize[1u];
            let _e261 = (_e256.xy - vec2<f32>(0f, _e259));
            let _e262 = layer;
            let _e265 = vec3<f32>(_e261.x, _e261.y, _e262);
            let _e271 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e265.x, _e265.y), i32(_e265.z));
            let _e274 = shadow;
            shadow = (_e274 + step(_e255, _e271.x));
            let _e276 = shadow;
            shadow = (_e276 / 5f);
        } else {
            x = -1i;
            loop {
                let _e278 = x;
                if (_e278 <= 1i) {
                    y = -1i;
                    loop {
                        let _e280 = y;
                        if (_e280 <= 1i) {
                            let _e282 = currentDepth;
                            let _e283 = sc;
                            let _e285 = x;
                            let _e287 = y;
                            let _e290 = texelSize;
                            let _e292 = (_e283.xy + (vec2<f32>(f32(_e285), f32(_e287)) * _e290));
                            let _e293 = layer;
                            let _e296 = vec3<f32>(_e292.x, _e292.y, _e293);
                            let _e302 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e296.x, _e296.y), i32(_e296.z));
                            let _e305 = shadow;
                            shadow = (_e305 + step(_e282, _e302.x));
                            continue;
                        } else {
                            break;
                        }
                        continuing {
                            let _e307 = y;
                            y = (_e307 + 1i);
                        }
                    }
                    continue;
                } else {
                    break;
                }
                continuing {
                    let _e309 = x;
                    x = (_e309 + 1i);
                }
            }
            let _e311 = shadow;
            shadow = (_e311 / 9f);
        }
    }
    let _e313 = shadow;
    return _e313;
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

    let _e104 = unnamed.cascadeSplits;
    let _e105 = (*viewDepth_1);
    cmp = step(_e104, vec4(_e105));
    let _e109 = cmp[0u];
    let _e111 = cmp[1u];
    let _e114 = cmp[2u];
    let _e117 = cmp[3u];
    cascade = min(i32((((_e109 + _e111) + _e114) + _e117)), 3i);
    let _e121 = cascade;
    (*outCascade) = _e121;
    let _e122 = cascade;
    if (_e122 == 0i) {
        local = 0f;
    } else {
        let _e124 = cascade;
        let _e129 = unnamed.cascadeSplits[max((_e124 - 1i), 0i)];
        local = _e129;
    }
    let _e130 = local;
    prevSplit = _e130;
    let _e131 = cascade;
    let _e134 = unnamed.cascadeSplits[_e131];
    farSplit = _e134;
    let _e135 = farSplit;
    let _e136 = prevSplit;
    blendRange = max((0.1f * (_e135 - _e136)), 1f);
    let _e140 = farSplit;
    let _e141 = (*viewDepth_1);
    let _e143 = blendRange;
    blendT = clamp(((_e140 - _e141) / _e143), 0f, 1f);
    let _e146 = cascade;
    param = _e146;
    let _e147 = (*worldPos_1);
    param_1 = _e147;
    let _e148 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param), (&param_1));
    s0_ = _e148;
    let _e149 = cascade;
    param_2 = min((_e149 + 1i), 3i);
    let _e152 = (*worldPos_1);
    param_3 = _e152;
    let _e153 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param_2), (&param_3));
    s1_ = _e153;
    let _e154 = s1_;
    let _e155 = s0_;
    let _e156 = blendT;
    return mix(_e154, _e155, _e156);
}

fn wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b(rgb: ptr<function, vec3<f32>>, lm_uv: ptr<function, vec2<f32>>) -> vec3<f32> {
    var sun_mask: f32;
    var shadow_1: f32;
    var ignoredCascade: i32;
    var param_4: vec3<f32>;
    var param_5: f32;
    var param_6: i32;

    let _e98 = unnamed.packed_indices[1i][3u];
    let _e104 = unnamed.packed_indices[1i][3u];
    let _e109 = (*lm_uv);
    let _e110 = textureSample(wired_bindless_images[(_e98 & 4095u)], wired_bindless_samplers[((_e104 >> bitcast<u32>(12i)) & 255u)], _e109);
    sun_mask = _e110.x;
    let _e112 = sun_mask;
    if (_e112 > 0.001f) {
        let _e114 = shadowData_1;
        param_4 = _e114.xyz;
        let _e117 = shadowData_1[3u];
        param_5 = _e117;
        let _e118 = sampleShadow_u0028_vf3_u003b_f1_u003b_i1_u003b((&param_4), (&param_5), (&param_6));
        let _e119 = param_6;
        ignoredCascade = _e119;
        shadow_1 = _e118;
        let _e120 = shadow_1;
        let _e121 = sun_mask;
        let _e123 = (*rgb);
        (*rgb) = (_e123 * mix(1f, _e120, _e121));
    }
    let _e125 = (*rgb);
    return _e125;
}

fn wired_apply_sun_shadow_operand_u0028_vf3_u003b(rgb_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var param_7: vec3<f32>;
    var param_8: vec2<f32>;
    var param_9: vec3<f32>;
    var param_10: vec2<f32>;
    var param_11: vec3<f32>;
    var param_12: vec2<f32>;

    if override_type_3_2 {
        let _e94 = (*rgb_1);
        param_7 = _e94;
        let _e95 = frag_tex_coord0_1;
        param_8 = _e95;
        let _e96 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_7), (&param_8));
        return _e96;
    }
    if override_type_3_3 {
        let _e97 = (*rgb_1);
        param_9 = _e97;
        let _e98 = frag_tex_coord1_1;
        param_10 = _e98;
        let _e99 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_9), (&param_10));
        return _e99;
    }
    if override_type_3_4 {
        let _e100 = (*rgb_1);
        param_11 = _e100;
        let _e101 = frag_tex_coord2_1;
        param_12 = _e101;
        let _e102 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_11), (&param_12));
        return _e102;
    }
    let _e103 = (*rgb_1);
    return _e103;
}

fn sRGBToLinear_u0028_vf3_u003b(c_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e91 = (*c_1);
    (*c_1) = max(_e91, vec3<f32>(0f, 0f, 0f));
    let _e93 = (*c_1);
    cutoff = (_e93 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e95 = (*c_1);
    lo = (_e95 / vec3(12.92f));
    let _e98 = (*c_1);
    hi = pow(((_e98 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e103 = hi;
    let _e104 = lo;
    let _e105 = cutoff;
    return mix(_e103, _e104, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e105));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_2: vec4<f32>;
    var param_13: vec3<f32>;

    let _e92 = (*role);
    let _e94 = (*role);
    let _e99 = unnamed.packed_indices[(_e92 / 4u)][(_e94 % 4u)];
    let _e102 = (*role);
    let _e104 = (*role);
    let _e109 = unnamed.packed_indices[(_e102 / 4u)][(_e104 % 4u)];
    let _e114 = (*uv);
    let _e115 = textureSample(wired_bindless_images[(_e99 & 4095u)], wired_bindless_samplers[((_e109 >> bitcast<u32>(12i)) & 255u)], _e114);
    c_2 = _e115;
    let _e116 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e116))) == 0i) {
        let _e121 = c_2;
        param_13 = _e121.xyz;
        let _e123 = sRGBToLinear_u0028_vf3_u003b((&param_13));
        c_2[0u] = _e123.x;
        c_2[1u] = _e123.y;
        c_2[2u] = _e123.z;
    }
    let _e130 = (*slot);
    if (lightmap_slot == (_e130 + 1i)) {
        let _e135 = unnamed.worldLightParams[0u];
        let _e136 = c_2;
        let _e138 = (_e136.xyz * _e135);
        c_2[0u] = _e138.x;
        c_2[1u] = _e138.y;
        c_2[2u] = _e138.z;
    }
    let _e145 = c_2;
    return _e145;
}

fn main_1() {
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
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var fogAmount: f32;

    let _e159 = frag_color0In_1;
    param_14 = _e159.xyz;
    let _e161 = sRGBToLinear_u0028_vf3_u003b((&param_14));
    let _e163 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e161.x, _e161.y, _e161.z, _e163);
    let _e168 = frag_color1In_1;
    param_15 = _e168.xyz;
    let _e170 = sRGBToLinear_u0028_vf3_u003b((&param_15));
    let _e172 = frag_color1In_1[3u];
    frag_color1_ = vec4<f32>(_e170.x, _e170.y, _e170.z, _e172);
    let _e177 = frag_color2In_1;
    param_16 = _e177.xyz;
    let _e179 = sRGBToLinear_u0028_vf3_u003b((&param_16));
    let _e181 = frag_color2In_1[3u];
    frag_color2_ = vec4<f32>(_e179.x, _e179.y, _e179.z, _e181);
    param_17 = 0u;
    let _e186 = frag_tex_coord0_1;
    param_18 = _e186;
    param_19 = 0i;
    let _e187 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_17), (&param_18), (&param_19));
    let _e188 = frag_color0_;
    color0_ = (_e187 * _e188);
    if override_type_3_7 {
        param_20 = 1u;
        let _e190 = frag_tex_coord1_1;
        param_21 = _e190;
        param_22 = 1i;
        let _e191 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_20), (&param_21), (&param_22));
        let _e192 = frag_color1_;
        color1_ = (_e191 * _e192);
        param_23 = 2u;
        let _e194 = frag_tex_coord2_1;
        param_24 = _e194;
        param_25 = 2i;
        let _e195 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_23), (&param_24), (&param_25));
        let _e196 = frag_color2_;
        color2_ = (_e195 * _e196);
        let _e198 = color0_;
        let _e200 = color1_;
        let _e203 = color2_;
        let _e205 = ((_e198.xyz + _e200.xyz) + _e203.xyz);
        let _e207 = color0_[3u];
        let _e209 = color1_[3u];
        let _e212 = color2_[3u];
        base = vec4<f32>(_e205.x, _e205.y, _e205.z, ((_e207 * _e209) * _e212));
    } else {
        if override_type_3_8 {
            param_26 = 1u;
            let _e218 = frag_tex_coord1_1;
            param_27 = _e218;
            param_28 = 1i;
            let _e219 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_26), (&param_27), (&param_28));
            let _e220 = frag_color1_;
            color1_1 = (_e219 * _e220);
            param_29 = 2u;
            let _e222 = frag_tex_coord2_1;
            param_30 = _e222;
            param_31 = 2i;
            let _e223 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_29), (&param_30), (&param_31));
            let _e224 = frag_color2_;
            color2_1 = (_e223 * _e224);
            let _e227 = color0_[3u];
            let _e228 = color0_;
            color0_ = (_e228 * _e227);
            let _e231 = color1_1[3u];
            let _e232 = color1_1;
            color1_1 = (_e232 * _e231);
            let _e235 = color2_1[3u];
            let _e236 = color2_1;
            color2_1 = (_e236 * _e235);
            let _e238 = color0_;
            let _e240 = color1_1;
            let _e243 = color2_1;
            let _e245 = ((_e238.xyz + _e240.xyz) + _e243.xyz);
            let _e247 = color0_[3u];
            let _e249 = color1_1[3u];
            let _e252 = color2_1[3u];
            base = vec4<f32>(_e245.x, _e245.y, _e245.z, ((_e247 * _e249) * _e252));
        } else {
            if override_type_3_9 {
                param_32 = 1u;
                let _e258 = frag_tex_coord1_1;
                param_33 = _e258;
                param_34 = 1i;
                let _e259 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_32), (&param_33), (&param_34));
                let _e260 = frag_color1_;
                color1_2 = (_e259 * _e260);
                param_35 = 2u;
                let _e262 = frag_tex_coord2_1;
                param_36 = _e262;
                param_37 = 2i;
                let _e263 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_35), (&param_36), (&param_37));
                let _e264 = frag_color2_;
                color2_2 = (_e263 * _e264);
                let _e267 = color0_[3u];
                let _e269 = color0_;
                color0_ = (_e269 * (1f - _e267));
                let _e272 = color1_2[3u];
                let _e274 = color1_2;
                color1_2 = (_e274 * (1f - _e272));
                let _e277 = color2_2[3u];
                let _e279 = color2_2;
                color2_2 = (_e279 * (1f - _e277));
                let _e281 = color0_;
                let _e283 = color1_2;
                let _e286 = color2_2;
                let _e288 = ((_e281.xyz + _e283.xyz) + _e286.xyz);
                let _e290 = color0_[3u];
                let _e292 = color1_2[3u];
                let _e295 = color2_2[3u];
                base = vec4<f32>(_e288.x, _e288.y, _e288.z, ((_e290 * _e292) * _e295));
            } else {
                if override_type_3_10 {
                    param_38 = 1u;
                    let _e301 = frag_tex_coord1_1;
                    param_39 = _e301;
                    param_40 = 1i;
                    let _e302 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_38), (&param_39), (&param_40));
                    let _e303 = frag_color1_;
                    color1_3 = (_e302 * _e303);
                    param_41 = 2u;
                    let _e305 = frag_tex_coord2_1;
                    param_42 = _e305;
                    param_43 = 2i;
                    let _e306 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_41), (&param_42), (&param_43));
                    let _e307 = frag_color2_;
                    color2_3 = (_e306 * _e307);
                    let _e309 = color0_;
                    let _e310 = color1_3;
                    let _e312 = color1_3[3u];
                    let _e315 = color2_3;
                    let _e317 = color2_3[3u];
                    base = mix(mix(_e309, _e310, vec4(_e312)), _e315, vec4(_e317));
                } else {
                    if override_type_3_11 {
                        param_44 = 1u;
                        let _e320 = frag_tex_coord1_1;
                        param_45 = _e320;
                        param_46 = 1i;
                        let _e321 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_44), (&param_45), (&param_46));
                        let _e322 = frag_color1_;
                        color1_4 = (_e321 * _e322);
                        param_47 = 2u;
                        let _e324 = frag_tex_coord2_1;
                        param_48 = _e324;
                        param_49 = 2i;
                        let _e325 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_47), (&param_48), (&param_49));
                        let _e326 = frag_color2_;
                        color2_4 = (_e325 * _e326);
                        let _e328 = color2_4;
                        let _e329 = color1_4;
                        let _e330 = color0_;
                        let _e332 = color1_4[3u];
                        let _e336 = color2_4[3u];
                        base = mix(_e328, mix(_e329, _e330, vec4(_e332)), vec4(_e336));
                    } else {
                        if override_type_3_12 {
                            param_50 = 1u;
                            let _e339 = frag_tex_coord1_1;
                            param_51 = _e339;
                            param_52 = 1i;
                            let _e340 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_50), (&param_51), (&param_52));
                            let _e341 = frag_color1_;
                            color1_5 = (_e340 * _e341);
                            param_53 = 2u;
                            let _e343 = frag_tex_coord2_1;
                            param_54 = _e343;
                            param_55 = 2i;
                            let _e344 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_53), (&param_54), (&param_55));
                            let _e345 = frag_color2_;
                            color2_5 = (_e344 * _e345);
                            let _e347 = color2_5;
                            let _e349 = color2_5[3u];
                            let _e352 = color1_5;
                            let _e354 = color1_5[3u];
                            let _e358 = color0_;
                            base = (((_e347 + vec4(_e349)) * (_e352 + vec4(_e354))) * _e358);
                        } else {
                            param_56 = 1u;
                            let _e360 = frag_tex_coord1_1;
                            param_57 = _e360;
                            param_58 = 1i;
                            let _e361 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_56), (&param_57), (&param_58));
                            let _e362 = frag_color1_;
                            color1_6 = (_e361 * _e362);
                            param_59 = 2u;
                            let _e364 = frag_tex_coord2_1;
                            param_60 = _e364;
                            param_61 = 2i;
                            let _e365 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_59), (&param_60), (&param_61));
                            let _e366 = frag_color2_;
                            color2_6 = (_e365 * _e366);
                            let _e368 = color0_;
                            let _e370 = color1_6;
                            let _e373 = color2_6;
                            let _e375 = ((_e368.xyz * _e370.xyz) * _e373.xyz);
                            base[0u] = _e375.x;
                            base[1u] = _e375.y;
                            base[2u] = _e375.z;
                            let _e383 = color0_[3u];
                            let _e385 = color1_6[3u];
                            let _e388 = color2_6[3u];
                            base[3u] = ((_e383 * _e385) * _e388);
                        }
                    }
                }
            }
        }
    }
    let _e391 = base;
    param_62 = _e391.xyz;
    let _e393 = wired_apply_sun_shadow_operand_u0028_vf3_u003b((&param_62));
    base[0u] = _e393.x;
    base[1u] = _e393.y;
    base[2u] = _e393.z;
    if override_type_3_13 {
        let _e402 = unnamed.worldLightParams[1u];
        wetness = clamp(_e402, 0f, 1f);
        let _e406 = unnamed.worldLightParams[2u];
        frost = clamp(_e406, 0f, 1f);
        let _e408 = base;
        luminance = dot(_e408.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e411 = wetness;
        let _e413 = base;
        let _e415 = (_e413.xyz * mix(1f, 0.82f, _e411));
        base[0u] = _e415.x;
        base[1u] = _e415.y;
        base[2u] = _e415.z;
        let _e422 = base;
        let _e424 = luminance;
        let _e426 = luminance;
        let _e428 = luminance;
        let _e430 = frost;
        let _e433 = mix(_e422.xyz, vec3<f32>((_e424 * 0.88f), (_e426 * 0.94f), _e428), vec3((_e430 * 0.55f)));
        base[0u] = _e433.x;
        base[1u] = _e433.y;
        base[2u] = _e433.z;
    }
    let _e440 = color0_;
    let _e443 = unnamed.emissionRadiance;
    let _e446 = base;
    let _e448 = (_e446.xyz + (_e440.xyz * _e443.xyz));
    base[0u] = _e448.x;
    base[1u] = _e448.y;
    base[2u] = _e448.z;
    let _e455 = wired_advanced_fog_enabled_u0028_();
    if _e455 {
        let _e456 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e456;
        let _e457 = base;
        let _e460 = unnamed.advancedFogColorDensity;
        let _e462 = fogAmount;
        let _e464 = mix(_e457.xyz, _e460.xyz, vec3(_e462));
        base[0u] = _e464.x;
        base[1u] = _e464.y;
        base[2u] = _e464.z;
    }
    if override_type_3_14 {
        let _e472 = base[3u];
        if (_e472 == 0f) {
            discard;
        }
    } else {
        if override_type_3_15 {
            let _e474 = base;
            let _e476 = base;
            if (dot(_e474.xyz, _e476.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e480 = base;
    out_color = _e480;
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(7) shadowData: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(3) frag_tex_coord2_: vec2<f32>, @location(0) frag_color0In: vec4<f32>, @location(5) frag_color1In: vec4<f32>, @location(6) frag_color2In: vec4<f32>) -> @location(0) vec4<f32> {
    gl_FragCoord_1 = gl_FragCoord;
    shadowData_1 = shadowData;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    frag_tex_coord2_1 = frag_tex_coord2_;
    frag_color0In_1 = frag_color0In;
    frag_color1In_1 = frag_color1In;
    frag_color2In_1 = frag_color2In;
    main_1();
    let _e17 = out_color;
    return _e17;
}
