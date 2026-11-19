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
    materialBaseColor: vec4<f32>,
    materialFactors: vec4<f32>,
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

    let _e92 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e92 + 0.5f));
    let _e97 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e99 = fogType;
    let _e102 = fogType;
    return (((_e97 > 0.5f) && (_e99 >= 1i)) && (_e102 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e92 = wired_advanced_fog_enabled_u0028_();
    if !(_e92) {
        return 0f;
    }
    let _e95 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e95, 0.000001f));
    let _e100 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e100 + 0.5f));
    let _e103 = fogType_1;
    if (_e103 == 1i) {
        let _e107 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e107 <= 0f) {
            return 0f;
        }
        let _e109 = viewDepth;
        let _e112 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e109 / _e112), 0f, 1f);
    }
    let _e117 = unnamed.advancedFogColorDensity[3u];
    let _e119 = viewDepth;
    opticalDepth = (max(_e117, 0f) * _e119);
    let _e121 = fogType_1;
    if (_e121 == 2i) {
        let _e123 = opticalDepth;
        return clamp((1f - exp(-(_e123))), 0f, 1f);
    }
    let _e128 = opticalDepth;
    let _e129 = opticalDepth;
    return clamp((1f - exp(-((_e128 * _e129)))), 0f, 1f);
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

    let _e99 = (*c);
    let _e102 = unnamed.cascadeMVP[_e99];
    let _e103 = (*worldPos);
    sc4_ = (_e102 * vec4<f32>(_e103.x, _e103.y, _e103.z, 1f));
    let _e109 = sc4_;
    let _e112 = sc4_[3u];
    sc = (_e109.xyz / vec3(_e112));
    let _e115 = sc;
    let _e119 = ((_e115.xy * 0.5f) + vec2(0.5f));
    sc[0u] = _e119.x;
    sc[1u] = _e119.y;
    let _e125 = sc[0u];
    let _e126 = (_e125 < 0f);
    phi_331_ = _e126;
    if !(_e126) {
        let _e129 = sc[0u];
        phi_331_ = (_e129 > 1f);
    }
    let _e132 = phi_331_;
    phi_338_ = _e132;
    if !(_e132) {
        let _e135 = sc[1u];
        phi_338_ = (_e135 < 0f);
    }
    let _e138 = phi_338_;
    phi_345_ = _e138;
    if !(_e138) {
        let _e141 = sc[1u];
        phi_345_ = (_e141 > 1f);
    }
    let _e144 = phi_345_;
    phi_352_ = _e144;
    if !(_e144) {
        let _e147 = sc[2u];
        phi_352_ = (_e147 > 1f);
    }
    let _e150 = phi_352_;
    if _e150 {
        return 1f;
    }
    let _e151 = (*c);
    layer = f32(_e151);
    let _e153 = textureDimensions(shadowMap, 0i);
    texelSize = (vec2(1f) / vec2<f32>(vec2<i32>(_e153).xy));
    let _e160 = sc[2u];
    currentDepth = (_e160 - shadow_bias);
    shadow = 0f;
    if override_type_3_ {
        let _e162 = currentDepth;
        let _e163 = sc;
        let _e164 = _e163.xy;
        let _e165 = layer;
        let _e168 = vec3<f32>(_e164.x, _e164.y, _e165);
        let _e174 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e168.x, _e168.y), i32(_e168.z));
        shadow = step(_e162, _e174.x);
    } else {
        if override_type_3_1 {
            let _e177 = currentDepth;
            let _e178 = sc;
            let _e179 = _e178.xy;
            let _e180 = layer;
            let _e183 = vec3<f32>(_e179.x, _e179.y, _e180);
            let _e189 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e183.x, _e183.y), i32(_e183.z));
            let _e192 = shadow;
            shadow = (_e192 + step(_e177, _e189.x));
            let _e194 = currentDepth;
            let _e195 = sc;
            let _e198 = texelSize[0u];
            let _e200 = (_e195.xy + vec2<f32>(_e198, 0f));
            let _e201 = layer;
            let _e204 = vec3<f32>(_e200.x, _e200.y, _e201);
            let _e210 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e204.x, _e204.y), i32(_e204.z));
            let _e213 = shadow;
            shadow = (_e213 + step(_e194, _e210.x));
            let _e215 = currentDepth;
            let _e216 = sc;
            let _e219 = texelSize[0u];
            let _e221 = (_e216.xy - vec2<f32>(_e219, 0f));
            let _e222 = layer;
            let _e225 = vec3<f32>(_e221.x, _e221.y, _e222);
            let _e231 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e225.x, _e225.y), i32(_e225.z));
            let _e234 = shadow;
            shadow = (_e234 + step(_e215, _e231.x));
            let _e236 = currentDepth;
            let _e237 = sc;
            let _e240 = texelSize[1u];
            let _e242 = (_e237.xy + vec2<f32>(0f, _e240));
            let _e243 = layer;
            let _e246 = vec3<f32>(_e242.x, _e242.y, _e243);
            let _e252 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e246.x, _e246.y), i32(_e246.z));
            let _e255 = shadow;
            shadow = (_e255 + step(_e236, _e252.x));
            let _e257 = currentDepth;
            let _e258 = sc;
            let _e261 = texelSize[1u];
            let _e263 = (_e258.xy - vec2<f32>(0f, _e261));
            let _e264 = layer;
            let _e267 = vec3<f32>(_e263.x, _e263.y, _e264);
            let _e273 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e267.x, _e267.y), i32(_e267.z));
            let _e276 = shadow;
            shadow = (_e276 + step(_e257, _e273.x));
            let _e278 = shadow;
            shadow = (_e278 / 5f);
        } else {
            x = -1i;
            loop {
                let _e280 = x;
                if (_e280 <= 1i) {
                    y = -1i;
                    loop {
                        let _e282 = y;
                        if (_e282 <= 1i) {
                            let _e284 = currentDepth;
                            let _e285 = sc;
                            let _e287 = x;
                            let _e289 = y;
                            let _e292 = texelSize;
                            let _e294 = (_e285.xy + (vec2<f32>(f32(_e287), f32(_e289)) * _e292));
                            let _e295 = layer;
                            let _e298 = vec3<f32>(_e294.x, _e294.y, _e295);
                            let _e304 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e298.x, _e298.y), i32(_e298.z));
                            let _e307 = shadow;
                            shadow = (_e307 + step(_e284, _e304.x));
                            continue;
                        } else {
                            break;
                        }
                        continuing {
                            let _e309 = y;
                            y = (_e309 + 1i);
                        }
                    }
                    continue;
                } else {
                    break;
                }
                continuing {
                    let _e311 = x;
                    x = (_e311 + 1i);
                }
            }
            let _e313 = shadow;
            shadow = (_e313 / 9f);
        }
    }
    let _e315 = shadow;
    return _e315;
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

    let _e106 = unnamed.cascadeSplits;
    let _e107 = (*viewDepth_1);
    cmp = step(_e106, vec4(_e107));
    let _e111 = cmp[0u];
    let _e113 = cmp[1u];
    let _e116 = cmp[2u];
    let _e119 = cmp[3u];
    cascade = min(i32((((_e111 + _e113) + _e116) + _e119)), 3i);
    let _e123 = cascade;
    (*outCascade) = _e123;
    let _e124 = cascade;
    if (_e124 == 0i) {
        local = 0f;
    } else {
        let _e126 = cascade;
        let _e131 = unnamed.cascadeSplits[max((_e126 - 1i), 0i)];
        local = _e131;
    }
    let _e132 = local;
    prevSplit = _e132;
    let _e133 = cascade;
    let _e136 = unnamed.cascadeSplits[_e133];
    farSplit = _e136;
    let _e137 = farSplit;
    let _e138 = prevSplit;
    blendRange = max((0.1f * (_e137 - _e138)), 1f);
    let _e142 = farSplit;
    let _e143 = (*viewDepth_1);
    let _e145 = blendRange;
    blendT = clamp(((_e142 - _e143) / _e145), 0f, 1f);
    let _e148 = cascade;
    param = _e148;
    let _e149 = (*worldPos_1);
    param_1 = _e149;
    let _e150 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param), (&param_1));
    s0_ = _e150;
    let _e151 = cascade;
    param_2 = min((_e151 + 1i), 3i);
    let _e154 = (*worldPos_1);
    param_3 = _e154;
    let _e155 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param_2), (&param_3));
    s1_ = _e155;
    let _e156 = s1_;
    let _e157 = s0_;
    let _e158 = blendT;
    return mix(_e156, _e157, _e158);
}

fn wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b(rgb: ptr<function, vec3<f32>>, lm_uv: ptr<function, vec2<f32>>) -> vec3<f32> {
    var sun_mask: f32;
    var shadow_1: f32;
    var ignoredCascade: i32;
    var param_4: vec3<f32>;
    var param_5: f32;
    var param_6: i32;

    let _e100 = unnamed.packed_indices[1i][3u];
    let _e106 = unnamed.packed_indices[1i][3u];
    let _e111 = (*lm_uv);
    let _e112 = textureSample(wired_bindless_images[(_e100 & 4095u)], wired_bindless_samplers[((_e106 >> bitcast<u32>(12i)) & 255u)], _e111);
    sun_mask = _e112.x;
    let _e114 = sun_mask;
    if (_e114 > 0.001f) {
        let _e116 = shadowData_1;
        param_4 = _e116.xyz;
        let _e119 = shadowData_1[3u];
        param_5 = _e119;
        let _e120 = sampleShadow_u0028_vf3_u003b_f1_u003b_i1_u003b((&param_4), (&param_5), (&param_6));
        let _e121 = param_6;
        ignoredCascade = _e121;
        shadow_1 = _e120;
        let _e122 = shadow_1;
        let _e123 = sun_mask;
        let _e125 = (*rgb);
        (*rgb) = (_e125 * mix(1f, _e122, _e123));
    }
    let _e127 = (*rgb);
    return _e127;
}

fn wired_apply_sun_shadow_operand_u0028_vf3_u003b(rgb_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var param_7: vec3<f32>;
    var param_8: vec2<f32>;
    var param_9: vec3<f32>;
    var param_10: vec2<f32>;
    var param_11: vec3<f32>;
    var param_12: vec2<f32>;

    if override_type_3_2 {
        let _e96 = (*rgb_1);
        param_7 = _e96;
        let _e97 = frag_tex_coord0_1;
        param_8 = _e97;
        let _e98 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_7), (&param_8));
        return _e98;
    }
    if override_type_3_3 {
        let _e99 = (*rgb_1);
        param_9 = _e99;
        let _e100 = frag_tex_coord1_1;
        param_10 = _e100;
        let _e101 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_9), (&param_10));
        return _e101;
    }
    if override_type_3_4 {
        let _e102 = (*rgb_1);
        param_11 = _e102;
        let _e103 = frag_tex_coord2_1;
        param_12 = _e103;
        let _e104 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_11), (&param_12));
        return _e104;
    }
    let _e105 = (*rgb_1);
    return _e105;
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb_2: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e92 = (*rgb_2);
    let _e95 = unnamed.worldLightParams[0u];
    boosted = (_e92 * _e95);
    let _e98 = boosted[0u];
    let _e100 = boosted[1u];
    let _e102 = boosted[2u];
    peak = max(_e98, max(_e100, _e102));
    let _e105 = peak;
    if (_e105 > 1f) {
        let _e107 = peak;
        let _e108 = boosted;
        boosted = (_e108 / vec3(_e107));
    }
    let _e111 = boosted;
    return _e111;
}

fn sRGBToLinear_u0028_vf3_u003b(c_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e93 = (*c_1);
    (*c_1) = max(_e93, vec3<f32>(0f, 0f, 0f));
    let _e95 = (*c_1);
    cutoff = (_e95 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e97 = (*c_1);
    lo = (_e97 / vec3(12.92f));
    let _e100 = (*c_1);
    hi = pow(((_e100 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e105 = hi;
    let _e106 = lo;
    let _e107 = cutoff;
    return mix(_e105, _e106, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e107));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_2: vec4<f32>;
    var param_13: vec3<f32>;
    var param_14: vec3<f32>;

    let _e95 = (*role);
    let _e97 = (*role);
    let _e102 = unnamed.packed_indices[(_e95 / 4u)][(_e97 % 4u)];
    let _e105 = (*role);
    let _e107 = (*role);
    let _e112 = unnamed.packed_indices[(_e105 / 4u)][(_e107 % 4u)];
    let _e117 = (*uv);
    let _e118 = textureSample(wired_bindless_images[(_e102 & 4095u)], wired_bindless_samplers[((_e112 >> bitcast<u32>(12i)) & 255u)], _e117);
    c_2 = _e118;
    let _e119 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e119))) == 0i) {
        let _e124 = c_2;
        param_13 = _e124.xyz;
        let _e126 = sRGBToLinear_u0028_vf3_u003b((&param_13));
        c_2[0u] = _e126.x;
        c_2[1u] = _e126.y;
        c_2[2u] = _e126.z;
    }
    let _e133 = (*slot);
    if (lightmap_slot == (_e133 + 1i)) {
        let _e136 = c_2;
        param_14 = _e136.xyz;
        let _e138 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_14));
        c_2[0u] = _e138.x;
        c_2[1u] = _e138.y;
        c_2[2u] = _e138.z;
    }
    let _e145 = c_2;
    return _e145;
}

fn main_1() {
    var frag_color0_: vec4<f32>;
    var param_15: vec3<f32>;
    var frag_color1_: vec4<f32>;
    var param_16: vec3<f32>;
    var frag_color2_: vec4<f32>;
    var param_17: vec3<f32>;
    var sampledColor0_: vec4<f32>;
    var param_18: u32;
    var param_19: vec2<f32>;
    var param_20: i32;
    var sampledColor1_: vec4<f32>;
    var param_21: u32;
    var param_22: vec2<f32>;
    var param_23: i32;
    var sampledColor2_: vec4<f32>;
    var param_24: u32;
    var param_25: vec2<f32>;
    var param_26: i32;
    var color0_: vec4<f32>;
    var color1_: vec4<f32>;
    var color2_: vec4<f32>;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var color2_1: vec4<f32>;
    var color1_2: vec4<f32>;
    var color2_2: vec4<f32>;
    var color1_3: vec4<f32>;
    var color2_3: vec4<f32>;
    var color1_4: vec4<f32>;
    var color2_4: vec4<f32>;
    var color1_5: vec4<f32>;
    var color2_5: vec4<f32>;
    var color1_6: vec4<f32>;
    var color2_6: vec4<f32>;
    var param_27: vec3<f32>;
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var fogAmount: f32;

    let _e128 = frag_color0In_1;
    param_15 = _e128.xyz;
    let _e130 = sRGBToLinear_u0028_vf3_u003b((&param_15));
    let _e132 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e130.x, _e130.y, _e130.z, _e132);
    let _e137 = frag_color1In_1;
    param_16 = _e137.xyz;
    let _e139 = sRGBToLinear_u0028_vf3_u003b((&param_16));
    let _e141 = frag_color1In_1[3u];
    frag_color1_ = vec4<f32>(_e139.x, _e139.y, _e139.z, _e141);
    let _e146 = frag_color2In_1;
    param_17 = _e146.xyz;
    let _e148 = sRGBToLinear_u0028_vf3_u003b((&param_17));
    let _e150 = frag_color2In_1[3u];
    frag_color2_ = vec4<f32>(_e148.x, _e148.y, _e148.z, _e150);
    param_18 = 0u;
    let _e155 = frag_tex_coord0_1;
    param_19 = _e155;
    param_20 = 0i;
    let _e156 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_18), (&param_19), (&param_20));
    sampledColor0_ = _e156;
    param_21 = 1u;
    let _e157 = frag_tex_coord1_1;
    param_22 = _e157;
    param_23 = 1i;
    let _e158 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_21), (&param_22), (&param_23));
    sampledColor1_ = _e158;
    param_24 = 2u;
    let _e159 = frag_tex_coord2_1;
    param_25 = _e159;
    param_26 = 2i;
    let _e160 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_24), (&param_25), (&param_26));
    sampledColor2_ = _e160;
    let _e161 = sampledColor0_;
    let _e162 = frag_color0_;
    color0_ = (_e161 * _e162);
    if override_type_3_7 {
        let _e164 = sampledColor1_;
        let _e165 = frag_color1_;
        color1_ = (_e164 * _e165);
        let _e167 = sampledColor2_;
        let _e168 = frag_color2_;
        color2_ = (_e167 * _e168);
        let _e170 = color0_;
        let _e172 = color1_;
        let _e175 = color2_;
        let _e177 = ((_e170.xyz + _e172.xyz) + _e175.xyz);
        let _e179 = color0_[3u];
        let _e181 = color1_[3u];
        let _e184 = color2_[3u];
        base = vec4<f32>(_e177.x, _e177.y, _e177.z, ((_e179 * _e181) * _e184));
    } else {
        if override_type_3_8 {
            let _e190 = sampledColor1_;
            let _e191 = frag_color1_;
            color1_1 = (_e190 * _e191);
            let _e193 = sampledColor2_;
            let _e194 = frag_color2_;
            color2_1 = (_e193 * _e194);
            let _e197 = color0_[3u];
            let _e198 = color0_;
            color0_ = (_e198 * _e197);
            let _e201 = color1_1[3u];
            let _e202 = color1_1;
            color1_1 = (_e202 * _e201);
            let _e205 = color2_1[3u];
            let _e206 = color2_1;
            color2_1 = (_e206 * _e205);
            let _e208 = color0_;
            let _e210 = color1_1;
            let _e213 = color2_1;
            let _e215 = ((_e208.xyz + _e210.xyz) + _e213.xyz);
            let _e217 = color0_[3u];
            let _e219 = color1_1[3u];
            let _e222 = color2_1[3u];
            base = vec4<f32>(_e215.x, _e215.y, _e215.z, ((_e217 * _e219) * _e222));
        } else {
            if override_type_3_9 {
                let _e228 = sampledColor1_;
                let _e229 = frag_color1_;
                color1_2 = (_e228 * _e229);
                let _e231 = sampledColor2_;
                let _e232 = frag_color2_;
                color2_2 = (_e231 * _e232);
                let _e235 = color0_[3u];
                let _e237 = color0_;
                color0_ = (_e237 * (1f - _e235));
                let _e240 = color1_2[3u];
                let _e242 = color1_2;
                color1_2 = (_e242 * (1f - _e240));
                let _e245 = color2_2[3u];
                let _e247 = color2_2;
                color2_2 = (_e247 * (1f - _e245));
                let _e249 = color0_;
                let _e251 = color1_2;
                let _e254 = color2_2;
                let _e256 = ((_e249.xyz + _e251.xyz) + _e254.xyz);
                let _e258 = color0_[3u];
                let _e260 = color1_2[3u];
                let _e263 = color2_2[3u];
                base = vec4<f32>(_e256.x, _e256.y, _e256.z, ((_e258 * _e260) * _e263));
            } else {
                if override_type_3_10 {
                    let _e269 = sampledColor1_;
                    let _e270 = frag_color1_;
                    color1_3 = (_e269 * _e270);
                    let _e272 = sampledColor2_;
                    let _e273 = frag_color2_;
                    color2_3 = (_e272 * _e273);
                    let _e275 = color0_;
                    let _e276 = color1_3;
                    let _e278 = color1_3[3u];
                    let _e281 = color2_3;
                    let _e283 = color2_3[3u];
                    base = mix(mix(_e275, _e276, vec4(_e278)), _e281, vec4(_e283));
                } else {
                    if override_type_3_11 {
                        let _e286 = sampledColor1_;
                        let _e287 = frag_color1_;
                        color1_4 = (_e286 * _e287);
                        let _e289 = sampledColor2_;
                        let _e290 = frag_color2_;
                        color2_4 = (_e289 * _e290);
                        let _e292 = color2_4;
                        let _e293 = color1_4;
                        let _e294 = color0_;
                        let _e296 = color1_4[3u];
                        let _e300 = color2_4[3u];
                        base = mix(_e292, mix(_e293, _e294, vec4(_e296)), vec4(_e300));
                    } else {
                        if override_type_3_12 {
                            let _e303 = sampledColor1_;
                            let _e304 = frag_color1_;
                            color1_5 = (_e303 * _e304);
                            let _e306 = sampledColor2_;
                            let _e307 = frag_color2_;
                            color2_5 = (_e306 * _e307);
                            let _e309 = color2_5;
                            let _e311 = color2_5[3u];
                            let _e314 = color1_5;
                            let _e316 = color1_5[3u];
                            let _e320 = color0_;
                            base = (((_e309 + vec4(_e311)) * (_e314 + vec4(_e316))) * _e320);
                        } else {
                            let _e322 = sampledColor1_;
                            let _e323 = frag_color1_;
                            color1_6 = (_e322 * _e323);
                            let _e325 = sampledColor2_;
                            let _e326 = frag_color2_;
                            color2_6 = (_e325 * _e326);
                            let _e328 = color0_;
                            let _e330 = color1_6;
                            let _e333 = color2_6;
                            let _e335 = ((_e328.xyz * _e330.xyz) * _e333.xyz);
                            base[0u] = _e335.x;
                            base[1u] = _e335.y;
                            base[2u] = _e335.z;
                            let _e343 = color0_[3u];
                            let _e345 = color1_6[3u];
                            let _e348 = color2_6[3u];
                            base[3u] = ((_e343 * _e345) * _e348);
                        }
                    }
                }
            }
        }
    }
    let _e351 = base;
    param_27 = _e351.xyz;
    let _e353 = wired_apply_sun_shadow_operand_u0028_vf3_u003b((&param_27));
    base[0u] = _e353.x;
    base[1u] = _e353.y;
    base[2u] = _e353.z;
    if override_type_3_13 {
        let _e362 = unnamed.worldLightParams[1u];
        wetness = clamp(_e362, 0f, 1f);
        let _e366 = unnamed.worldLightParams[2u];
        frost = clamp(_e366, 0f, 1f);
        let _e368 = base;
        luminance = dot(_e368.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e371 = wetness;
        let _e373 = base;
        let _e375 = (_e373.xyz * mix(1f, 0.82f, _e371));
        base[0u] = _e375.x;
        base[1u] = _e375.y;
        base[2u] = _e375.z;
        let _e382 = base;
        let _e384 = luminance;
        let _e386 = luminance;
        let _e388 = luminance;
        let _e390 = frost;
        let _e393 = mix(_e382.xyz, vec3<f32>((_e384 * 0.88f), (_e386 * 0.94f), _e388), vec3((_e390 * 0.55f)));
        base[0u] = _e393.x;
        base[1u] = _e393.y;
        base[2u] = _e393.z;
    }
    let _e402 = unnamed.materialFactors[0u];
    if (_e402 >= 0f) {
        let _e405 = unnamed.materialBaseColor;
        let _e406 = base;
        base = (_e406 * _e405);
    }
    let _e408 = color0_;
    let _e411 = unnamed.emissionRadiance;
    let _e414 = base;
    let _e416 = (_e414.xyz + (_e408.xyz * _e411.xyz));
    base[0u] = _e416.x;
    base[1u] = _e416.y;
    base[2u] = _e416.z;
    let _e423 = wired_advanced_fog_enabled_u0028_();
    if _e423 {
        let _e424 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e424;
        let _e425 = base;
        let _e428 = unnamed.advancedFogColorDensity;
        let _e430 = fogAmount;
        let _e432 = mix(_e425.xyz, _e428.xyz, vec3(_e430));
        base[0u] = _e432.x;
        base[1u] = _e432.y;
        base[2u] = _e432.z;
    }
    if override_type_3_14 {
        let _e440 = base[3u];
        if (_e440 == 0f) {
            discard;
        }
    } else {
        if override_type_3_15 {
            let _e442 = base;
            let _e444 = base;
            if (dot(_e442.xyz, _e444.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e448 = base;
    out_color = _e448;
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
