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

struct FragmentOutput {
    @location(1) member: vec2<f32>,
    @location(2) member_1: f32,
    @location(0) member_2: vec4<f32>,
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
@id(10) override acff: i32 = 0i;
override override_type_3_14: bool = (acff == 1i);
override override_type_3_15: bool = (acff == 2i);
override override_type_3_16: bool = (acff == 3i);
override override_type_3_17: bool = (acff == 1i);
override override_type_3_18: bool = (acff == 2i);
override override_type_3_19: bool = (acff == 3i);
@id(7) override discard_mode: i32 = 0i;
override override_type_3_20: bool = (discard_mode == 1i);
override override_type_3_21: bool = (discard_mode == 2i);
@id(3) override alpha_to_coverage: i32 = 0i;

var<private> out_temporal_velocity: vec2<f32>;
var<private> out_temporal_validity: f32;
var<private> temporalOutcome_1: u32;
var<private> temporalCurrentClip_1: vec4<f32>;
var<private> temporalPreviousClip_1: vec4<f32>;
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

fn wiredTemporalFinite2_u0028_vf2_u003b(value: ptr<function, vec2<f32>>) -> bool {
    var phi_97_: bool;

    let _e108 = (*value);
    let _e109 = (*value);
    let _e111 = all((_e108 == _e109));
    phi_97_ = _e111;
    if _e111 {
        let _e112 = (*value);
        phi_97_ = all((abs(_e112) <= vec2<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e117 = phi_97_;
    return _e117;
}

fn wiredTemporalFinite4_u0028_vf4_u003b(value_1: ptr<function, vec4<f32>>) -> bool {
    var phi_82_: bool;

    let _e108 = (*value_1);
    let _e109 = (*value_1);
    let _e111 = all((_e108 == _e109));
    phi_82_ = _e111;
    if _e111 {
        let _e112 = (*value_1);
        phi_82_ = all((abs(_e112) <= vec4<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e117 = phi_82_;
    return _e117;
}

fn wiredTemporalWriteAux_u0028_f1_u003b(coverageConfidence: ptr<function, f32>) {
    var param: vec4<f32>;
    var param_1: vec4<f32>;
    var currentNdc: vec2<f32>;
    var previousNdc: vec2<f32>;
    var param_2: vec2<f32>;
    var param_3: vec2<f32>;
    var currentUv: vec2<f32>;
    var previousUv: vec2<f32>;
    var velocity: vec2<f32>;
    var param_4: vec2<f32>;
    var phi_120_: bool;
    var phi_129_: bool;
    var phi_139_: bool;
    var phi_146_: bool;
    var phi_175_: bool;

    out_temporal_velocity = vec2<f32>(0f, 0f);
    out_temporal_validity = 0f;
    let _e118 = temporalOutcome_1;
    let _e119 = (_e118 != 1u);
    phi_120_ = _e119;
    if !(_e119) {
        let _e121 = temporalCurrentClip_1;
        param = _e121;
        let _e122 = wiredTemporalFinite4_u0028_vf4_u003b((&param));
        phi_120_ = !(_e122);
    }
    let _e125 = phi_120_;
    phi_129_ = _e125;
    if !(_e125) {
        let _e127 = temporalPreviousClip_1;
        param_1 = _e127;
        let _e128 = wiredTemporalFinite4_u0028_vf4_u003b((&param_1));
        phi_129_ = !(_e128);
    }
    let _e131 = phi_129_;
    phi_139_ = _e131;
    if !(_e131) {
        let _e134 = temporalCurrentClip_1[3u];
        phi_139_ = (_e134 <= 0.000001f);
    }
    let _e137 = phi_139_;
    phi_146_ = _e137;
    if !(_e137) {
        let _e140 = temporalPreviousClip_1[3u];
        phi_146_ = (_e140 <= 0.000001f);
    }
    let _e143 = phi_146_;
    if _e143 {
        return;
    }
    let _e144 = temporalCurrentClip_1;
    let _e147 = temporalCurrentClip_1[3u];
    currentNdc = (_e144.xy / vec2(_e147));
    let _e150 = temporalPreviousClip_1;
    let _e153 = temporalPreviousClip_1[3u];
    previousNdc = (_e150.xy / vec2(_e153));
    let _e156 = currentNdc;
    param_2 = _e156;
    let _e157 = wiredTemporalFinite2_u0028_vf2_u003b((&param_2));
    let _e158 = !(_e157);
    phi_175_ = _e158;
    if !(_e158) {
        let _e160 = previousNdc;
        param_3 = _e160;
        let _e161 = wiredTemporalFinite2_u0028_vf2_u003b((&param_3));
        phi_175_ = !(_e161);
    }
    let _e164 = phi_175_;
    if _e164 {
        return;
    }
    let _e165 = currentNdc;
    currentUv = ((_e165 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e168 = previousNdc;
    previousUv = ((_e168 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e171 = currentUv;
    let _e172 = previousUv;
    velocity = (_e171 - _e172);
    let _e174 = velocity;
    param_4 = _e174;
    let _e175 = wiredTemporalFinite2_u0028_vf2_u003b((&param_4));
    if !(_e175) {
        return;
    }
    let _e177 = velocity;
    out_temporal_velocity = _e177;
    let _e178 = (*coverageConfidence);
    out_temporal_validity = clamp(_e178, 0f, 1f);
    return;
}

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e110 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e110 + 0.5f));
    let _e115 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e117 = fogType;
    let _e120 = fogType;
    return (((_e115 > 0.5f) && (_e117 >= 1i)) && (_e120 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e110 = wired_advanced_fog_enabled_u0028_();
    if !(_e110) {
        return 0f;
    }
    let _e113 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e113, 0.000001f));
    let _e118 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e118 + 0.5f));
    let _e121 = fogType_1;
    if (_e121 == 1i) {
        let _e125 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e125 <= 0f) {
            return 0f;
        }
        let _e127 = viewDepth;
        let _e130 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e127 / _e130), 0f, 1f);
    }
    let _e135 = unnamed.advancedFogColorDensity[3u];
    let _e137 = viewDepth;
    opticalDepth = (max(_e135, 0f) * _e137);
    let _e139 = fogType_1;
    if (_e139 == 2i) {
        let _e141 = opticalDepth;
        return clamp((1f - exp(-(_e141))), 0f, 1f);
    }
    let _e146 = opticalDepth;
    let _e147 = opticalDepth;
    return clamp((1f - exp(-((_e146 * _e147)))), 0f, 1f);
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
    var phi_470_: bool;
    var phi_477_: bool;
    var phi_484_: bool;
    var phi_491_: bool;

    let _e117 = (*c);
    let _e120 = unnamed.cascadeMVP[_e117];
    let _e121 = (*worldPos);
    sc4_ = (_e120 * vec4<f32>(_e121.x, _e121.y, _e121.z, 1f));
    let _e127 = sc4_;
    let _e130 = sc4_[3u];
    sc = (_e127.xyz / vec3(_e130));
    let _e133 = sc;
    let _e137 = ((_e133.xy * 0.5f) + vec2(0.5f));
    sc[0u] = _e137.x;
    sc[1u] = _e137.y;
    let _e143 = sc[0u];
    let _e144 = (_e143 < 0f);
    phi_470_ = _e144;
    if !(_e144) {
        let _e147 = sc[0u];
        phi_470_ = (_e147 > 1f);
    }
    let _e150 = phi_470_;
    phi_477_ = _e150;
    if !(_e150) {
        let _e153 = sc[1u];
        phi_477_ = (_e153 < 0f);
    }
    let _e156 = phi_477_;
    phi_484_ = _e156;
    if !(_e156) {
        let _e159 = sc[1u];
        phi_484_ = (_e159 > 1f);
    }
    let _e162 = phi_484_;
    phi_491_ = _e162;
    if !(_e162) {
        let _e165 = sc[2u];
        phi_491_ = (_e165 > 1f);
    }
    let _e168 = phi_491_;
    if _e168 {
        return 1f;
    }
    let _e169 = (*c);
    layer = f32(_e169);
    let _e171 = textureDimensions(shadowMap, 0i);
    texelSize = (vec2(1f) / vec2<f32>(vec2<i32>(_e171).xy));
    let _e178 = sc[2u];
    currentDepth = (_e178 - shadow_bias);
    shadow = 0f;
    if override_type_3_ {
        let _e180 = currentDepth;
        let _e181 = sc;
        let _e182 = _e181.xy;
        let _e183 = layer;
        let _e186 = vec3<f32>(_e182.x, _e182.y, _e183);
        let _e192 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e186.x, _e186.y), i32(_e186.z));
        shadow = step(_e180, _e192.x);
    } else {
        if override_type_3_1 {
            let _e195 = currentDepth;
            let _e196 = sc;
            let _e197 = _e196.xy;
            let _e198 = layer;
            let _e201 = vec3<f32>(_e197.x, _e197.y, _e198);
            let _e207 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e201.x, _e201.y), i32(_e201.z));
            let _e210 = shadow;
            shadow = (_e210 + step(_e195, _e207.x));
            let _e212 = currentDepth;
            let _e213 = sc;
            let _e216 = texelSize[0u];
            let _e218 = (_e213.xy + vec2<f32>(_e216, 0f));
            let _e219 = layer;
            let _e222 = vec3<f32>(_e218.x, _e218.y, _e219);
            let _e228 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e222.x, _e222.y), i32(_e222.z));
            let _e231 = shadow;
            shadow = (_e231 + step(_e212, _e228.x));
            let _e233 = currentDepth;
            let _e234 = sc;
            let _e237 = texelSize[0u];
            let _e239 = (_e234.xy - vec2<f32>(_e237, 0f));
            let _e240 = layer;
            let _e243 = vec3<f32>(_e239.x, _e239.y, _e240);
            let _e249 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e243.x, _e243.y), i32(_e243.z));
            let _e252 = shadow;
            shadow = (_e252 + step(_e233, _e249.x));
            let _e254 = currentDepth;
            let _e255 = sc;
            let _e258 = texelSize[1u];
            let _e260 = (_e255.xy + vec2<f32>(0f, _e258));
            let _e261 = layer;
            let _e264 = vec3<f32>(_e260.x, _e260.y, _e261);
            let _e270 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e264.x, _e264.y), i32(_e264.z));
            let _e273 = shadow;
            shadow = (_e273 + step(_e254, _e270.x));
            let _e275 = currentDepth;
            let _e276 = sc;
            let _e279 = texelSize[1u];
            let _e281 = (_e276.xy - vec2<f32>(0f, _e279));
            let _e282 = layer;
            let _e285 = vec3<f32>(_e281.x, _e281.y, _e282);
            let _e291 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e285.x, _e285.y), i32(_e285.z));
            let _e294 = shadow;
            shadow = (_e294 + step(_e275, _e291.x));
            let _e296 = shadow;
            shadow = (_e296 / 5f);
        } else {
            x = -1i;
            loop {
                let _e298 = x;
                if (_e298 <= 1i) {
                    y = -1i;
                    loop {
                        let _e300 = y;
                        if (_e300 <= 1i) {
                            let _e302 = currentDepth;
                            let _e303 = sc;
                            let _e305 = x;
                            let _e307 = y;
                            let _e310 = texelSize;
                            let _e312 = (_e303.xy + (vec2<f32>(f32(_e305), f32(_e307)) * _e310));
                            let _e313 = layer;
                            let _e316 = vec3<f32>(_e312.x, _e312.y, _e313);
                            let _e322 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e316.x, _e316.y), i32(_e316.z));
                            let _e325 = shadow;
                            shadow = (_e325 + step(_e302, _e322.x));
                            continue;
                        } else {
                            break;
                        }
                        continuing {
                            let _e327 = y;
                            y = (_e327 + 1i);
                        }
                    }
                    continue;
                } else {
                    break;
                }
                continuing {
                    let _e329 = x;
                    x = (_e329 + 1i);
                }
            }
            let _e331 = shadow;
            shadow = (_e331 / 9f);
        }
    }
    let _e333 = shadow;
    return _e333;
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
    var param_5: i32;
    var param_6: vec3<f32>;
    var s1_: f32;
    var param_7: i32;
    var param_8: vec3<f32>;

    let _e124 = unnamed.cascadeSplits;
    let _e125 = (*viewDepth_1);
    cmp = step(_e124, vec4(_e125));
    let _e129 = cmp[0u];
    let _e131 = cmp[1u];
    let _e134 = cmp[2u];
    let _e137 = cmp[3u];
    cascade = min(i32((((_e129 + _e131) + _e134) + _e137)), 3i);
    let _e141 = cascade;
    (*outCascade) = _e141;
    let _e142 = cascade;
    if (_e142 == 0i) {
        local = 0f;
    } else {
        let _e144 = cascade;
        let _e149 = unnamed.cascadeSplits[max((_e144 - 1i), 0i)];
        local = _e149;
    }
    let _e150 = local;
    prevSplit = _e150;
    let _e151 = cascade;
    let _e154 = unnamed.cascadeSplits[_e151];
    farSplit = _e154;
    let _e155 = farSplit;
    let _e156 = prevSplit;
    blendRange = max((0.1f * (_e155 - _e156)), 1f);
    let _e160 = farSplit;
    let _e161 = (*viewDepth_1);
    let _e163 = blendRange;
    blendT = clamp(((_e160 - _e161) / _e163), 0f, 1f);
    let _e166 = cascade;
    param_5 = _e166;
    let _e167 = (*worldPos_1);
    param_6 = _e167;
    let _e168 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param_5), (&param_6));
    s0_ = _e168;
    let _e169 = cascade;
    param_7 = min((_e169 + 1i), 3i);
    let _e172 = (*worldPos_1);
    param_8 = _e172;
    let _e173 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param_7), (&param_8));
    s1_ = _e173;
    let _e174 = s1_;
    let _e175 = s0_;
    let _e176 = blendT;
    return mix(_e174, _e175, _e176);
}

fn wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b(rgb: ptr<function, vec3<f32>>, lm_uv: ptr<function, vec2<f32>>) -> vec3<f32> {
    var sun_mask: f32;
    var shadow_1: f32;
    var ignoredCascade: i32;
    var param_9: vec3<f32>;
    var param_10: f32;
    var param_11: i32;

    let _e118 = unnamed.packed_indices[1i][3u];
    let _e124 = unnamed.packed_indices[1i][3u];
    let _e129 = (*lm_uv);
    let _e130 = textureSample(wired_bindless_images[(_e118 & 4095u)], wired_bindless_samplers[((_e124 >> bitcast<u32>(12i)) & 255u)], _e129);
    sun_mask = _e130.x;
    let _e132 = sun_mask;
    if (_e132 > 0.001f) {
        let _e134 = shadowData_1;
        param_9 = _e134.xyz;
        let _e137 = shadowData_1[3u];
        param_10 = _e137;
        let _e138 = sampleShadow_u0028_vf3_u003b_f1_u003b_i1_u003b((&param_9), (&param_10), (&param_11));
        let _e139 = param_11;
        ignoredCascade = _e139;
        shadow_1 = _e138;
        let _e140 = shadow_1;
        let _e141 = sun_mask;
        let _e143 = (*rgb);
        (*rgb) = (_e143 * mix(1f, _e140, _e141));
    }
    let _e145 = (*rgb);
    return _e145;
}

fn wired_apply_sun_shadow_operand_u0028_vf3_u003b(rgb_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var param_12: vec3<f32>;
    var param_13: vec2<f32>;
    var param_14: vec3<f32>;
    var param_15: vec2<f32>;
    var param_16: vec3<f32>;
    var param_17: vec2<f32>;

    if override_type_3_2 {
        let _e114 = (*rgb_1);
        param_12 = _e114;
        let _e115 = frag_tex_coord0_1;
        param_13 = _e115;
        let _e116 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_12), (&param_13));
        return _e116;
    }
    if override_type_3_3 {
        let _e117 = (*rgb_1);
        param_14 = _e117;
        let _e118 = frag_tex_coord1_1;
        param_15 = _e118;
        let _e119 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_14), (&param_15));
        return _e119;
    }
    if override_type_3_4 {
        let _e120 = (*rgb_1);
        param_16 = _e120;
        let _e121 = frag_tex_coord2_1;
        param_17 = _e121;
        let _e122 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_16), (&param_17));
        return _e122;
    }
    let _e123 = (*rgb_1);
    return _e123;
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb_2: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e110 = (*rgb_2);
    let _e113 = unnamed.worldLightParams[0u];
    boosted = (_e110 * _e113);
    let _e116 = boosted[0u];
    let _e118 = boosted[1u];
    let _e120 = boosted[2u];
    peak = max(_e116, max(_e118, _e120));
    let _e123 = peak;
    if (_e123 > 1f) {
        let _e125 = peak;
        let _e126 = boosted;
        boosted = (_e126 / vec3(_e125));
    }
    let _e129 = boosted;
    return _e129;
}

fn sRGBToLinear_u0028_vf3_u003b(c_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e111 = (*c_1);
    (*c_1) = max(_e111, vec3<f32>(0f, 0f, 0f));
    let _e113 = (*c_1);
    cutoff = (_e113 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e115 = (*c_1);
    lo = (_e115 / vec3(12.92f));
    let _e118 = (*c_1);
    hi = pow(((_e118 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e123 = hi;
    let _e124 = lo;
    let _e125 = cutoff;
    return mix(_e123, _e124, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e125));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_2: vec4<f32>;
    var param_18: vec3<f32>;
    var param_19: vec3<f32>;

    let _e113 = (*role);
    let _e115 = (*role);
    let _e120 = unnamed.packed_indices[(_e113 / 4u)][(_e115 % 4u)];
    let _e123 = (*role);
    let _e125 = (*role);
    let _e130 = unnamed.packed_indices[(_e123 / 4u)][(_e125 % 4u)];
    let _e135 = (*uv);
    let _e136 = textureSample(wired_bindless_images[(_e120 & 4095u)], wired_bindless_samplers[((_e130 >> bitcast<u32>(12i)) & 255u)], _e135);
    c_2 = _e136;
    let _e137 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e137))) == 0i) {
        let _e142 = c_2;
        param_18 = _e142.xyz;
        let _e144 = sRGBToLinear_u0028_vf3_u003b((&param_18));
        c_2[0u] = _e144.x;
        c_2[1u] = _e144.y;
        c_2[2u] = _e144.z;
    }
    let _e151 = (*slot);
    if (lightmap_slot == (_e151 + 1i)) {
        let _e154 = c_2;
        param_19 = _e154.xyz;
        let _e156 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_19));
        c_2[0u] = _e156.x;
        c_2[1u] = _e156.y;
        c_2[2u] = _e156.z;
    }
    let _e163 = c_2;
    return _e163;
}

fn main_1() {
    var fog: vec4<f32>;
    var frag_color0_: vec4<f32>;
    var param_20: vec3<f32>;
    var frag_color1_: vec4<f32>;
    var param_21: vec3<f32>;
    var frag_color2_: vec4<f32>;
    var param_22: vec3<f32>;
    var sampledColor0_: vec4<f32>;
    var param_23: u32;
    var param_24: vec2<f32>;
    var param_25: i32;
    var sampledColor1_: vec4<f32>;
    var param_26: u32;
    var param_27: vec2<f32>;
    var param_28: i32;
    var sampledColor2_: vec4<f32>;
    var param_29: u32;
    var param_30: vec2<f32>;
    var param_31: i32;
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
    var param_32: vec3<f32>;
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var fogAmount: f32;
    var param_33: f32;

    let _e151 = unnamed.packed_indices[0i][3u];
    let _e157 = unnamed.packed_indices[0i][3u];
    let _e162 = fog_tex_coord_1;
    let _e163 = textureSample(wired_bindless_images[(_e151 & 4095u)], wired_bindless_samplers[((_e157 >> bitcast<u32>(12i)) & 255u)], _e162);
    fog = _e163;
    let _e164 = frag_color0In_1;
    param_20 = _e164.xyz;
    let _e166 = sRGBToLinear_u0028_vf3_u003b((&param_20));
    let _e168 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e166.x, _e166.y, _e166.z, _e168);
    let _e173 = frag_color1In_1;
    param_21 = _e173.xyz;
    let _e175 = sRGBToLinear_u0028_vf3_u003b((&param_21));
    let _e177 = frag_color1In_1[3u];
    frag_color1_ = vec4<f32>(_e175.x, _e175.y, _e175.z, _e177);
    let _e182 = frag_color2In_1;
    param_22 = _e182.xyz;
    let _e184 = sRGBToLinear_u0028_vf3_u003b((&param_22));
    let _e186 = frag_color2In_1[3u];
    frag_color2_ = vec4<f32>(_e184.x, _e184.y, _e184.z, _e186);
    param_23 = 0u;
    let _e191 = frag_tex_coord0_1;
    param_24 = _e191;
    param_25 = 0i;
    let _e192 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_23), (&param_24), (&param_25));
    sampledColor0_ = _e192;
    param_26 = 1u;
    let _e193 = frag_tex_coord1_1;
    param_27 = _e193;
    param_28 = 1i;
    let _e194 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_26), (&param_27), (&param_28));
    sampledColor1_ = _e194;
    param_29 = 2u;
    let _e195 = frag_tex_coord2_1;
    param_30 = _e195;
    param_31 = 2i;
    let _e196 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_29), (&param_30), (&param_31));
    sampledColor2_ = _e196;
    let _e197 = sampledColor0_;
    let _e198 = frag_color0_;
    color0_ = (_e197 * _e198);
    if override_type_3_7 {
        let _e200 = sampledColor1_;
        let _e201 = frag_color1_;
        color1_ = (_e200 * _e201);
        let _e203 = sampledColor2_;
        let _e204 = frag_color2_;
        color2_ = (_e203 * _e204);
        let _e206 = color0_;
        let _e208 = color1_;
        let _e211 = color2_;
        let _e213 = ((_e206.xyz + _e208.xyz) + _e211.xyz);
        let _e215 = color0_[3u];
        let _e217 = color1_[3u];
        let _e220 = color2_[3u];
        base = vec4<f32>(_e213.x, _e213.y, _e213.z, ((_e215 * _e217) * _e220));
    } else {
        if override_type_3_8 {
            let _e226 = sampledColor1_;
            let _e227 = frag_color1_;
            color1_1 = (_e226 * _e227);
            let _e229 = sampledColor2_;
            let _e230 = frag_color2_;
            color2_1 = (_e229 * _e230);
            let _e233 = color0_[3u];
            let _e234 = color0_;
            color0_ = (_e234 * _e233);
            let _e237 = color1_1[3u];
            let _e238 = color1_1;
            color1_1 = (_e238 * _e237);
            let _e241 = color2_1[3u];
            let _e242 = color2_1;
            color2_1 = (_e242 * _e241);
            let _e244 = color0_;
            let _e246 = color1_1;
            let _e249 = color2_1;
            let _e251 = ((_e244.xyz + _e246.xyz) + _e249.xyz);
            let _e253 = color0_[3u];
            let _e255 = color1_1[3u];
            let _e258 = color2_1[3u];
            base = vec4<f32>(_e251.x, _e251.y, _e251.z, ((_e253 * _e255) * _e258));
        } else {
            if override_type_3_9 {
                let _e264 = sampledColor1_;
                let _e265 = frag_color1_;
                color1_2 = (_e264 * _e265);
                let _e267 = sampledColor2_;
                let _e268 = frag_color2_;
                color2_2 = (_e267 * _e268);
                let _e271 = color0_[3u];
                let _e273 = color0_;
                color0_ = (_e273 * (1f - _e271));
                let _e276 = color1_2[3u];
                let _e278 = color1_2;
                color1_2 = (_e278 * (1f - _e276));
                let _e281 = color2_2[3u];
                let _e283 = color2_2;
                color2_2 = (_e283 * (1f - _e281));
                let _e285 = color0_;
                let _e287 = color1_2;
                let _e290 = color2_2;
                let _e292 = ((_e285.xyz + _e287.xyz) + _e290.xyz);
                let _e294 = color0_[3u];
                let _e296 = color1_2[3u];
                let _e299 = color2_2[3u];
                base = vec4<f32>(_e292.x, _e292.y, _e292.z, ((_e294 * _e296) * _e299));
            } else {
                if override_type_3_10 {
                    let _e305 = sampledColor1_;
                    let _e306 = frag_color1_;
                    color1_3 = (_e305 * _e306);
                    let _e308 = sampledColor2_;
                    let _e309 = frag_color2_;
                    color2_3 = (_e308 * _e309);
                    let _e311 = color0_;
                    let _e312 = color1_3;
                    let _e314 = color1_3[3u];
                    let _e317 = color2_3;
                    let _e319 = color2_3[3u];
                    base = mix(mix(_e311, _e312, vec4(_e314)), _e317, vec4(_e319));
                } else {
                    if override_type_3_11 {
                        let _e322 = sampledColor1_;
                        let _e323 = frag_color1_;
                        color1_4 = (_e322 * _e323);
                        let _e325 = sampledColor2_;
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
                            let _e339 = sampledColor1_;
                            let _e340 = frag_color1_;
                            color1_5 = (_e339 * _e340);
                            let _e342 = sampledColor2_;
                            let _e343 = frag_color2_;
                            color2_5 = (_e342 * _e343);
                            let _e345 = color2_5;
                            let _e347 = color2_5[3u];
                            let _e350 = color1_5;
                            let _e352 = color1_5[3u];
                            let _e356 = color0_;
                            base = (((_e345 + vec4(_e347)) * (_e350 + vec4(_e352))) * _e356);
                        } else {
                            let _e358 = sampledColor1_;
                            let _e359 = frag_color1_;
                            color1_6 = (_e358 * _e359);
                            let _e361 = sampledColor2_;
                            let _e362 = frag_color2_;
                            color2_6 = (_e361 * _e362);
                            let _e364 = color0_;
                            let _e366 = color1_6;
                            let _e369 = color2_6;
                            let _e371 = ((_e364.xyz * _e366.xyz) * _e369.xyz);
                            base[0u] = _e371.x;
                            base[1u] = _e371.y;
                            base[2u] = _e371.z;
                            let _e379 = color0_[3u];
                            let _e381 = color1_6[3u];
                            let _e384 = color2_6[3u];
                            base[3u] = ((_e379 * _e381) * _e384);
                        }
                    }
                }
            }
        }
    }
    let _e387 = base;
    param_32 = _e387.xyz;
    let _e389 = wired_apply_sun_shadow_operand_u0028_vf3_u003b((&param_32));
    base[0u] = _e389.x;
    base[1u] = _e389.y;
    base[2u] = _e389.z;
    if override_type_3_13 {
        let _e398 = unnamed.worldLightParams[1u];
        wetness = clamp(_e398, 0f, 1f);
        let _e402 = unnamed.worldLightParams[2u];
        frost = clamp(_e402, 0f, 1f);
        let _e404 = base;
        luminance = dot(_e404.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e407 = wetness;
        let _e409 = base;
        let _e411 = (_e409.xyz * mix(1f, 0.82f, _e407));
        base[0u] = _e411.x;
        base[1u] = _e411.y;
        base[2u] = _e411.z;
        let _e418 = base;
        let _e420 = luminance;
        let _e422 = luminance;
        let _e424 = luminance;
        let _e426 = frost;
        let _e429 = mix(_e418.xyz, vec3<f32>((_e420 * 0.88f), (_e422 * 0.94f), _e424), vec3((_e426 * 0.55f)));
        base[0u] = _e429.x;
        base[1u] = _e429.y;
        base[2u] = _e429.z;
    }
    let _e438 = unnamed.materialFactors[0u];
    if (_e438 >= 0f) {
        let _e441 = unnamed.materialBaseColor;
        let _e442 = base;
        base = (_e442 * _e441);
    }
    let _e444 = color0_;
    let _e447 = unnamed.emissionRadiance;
    let _e450 = base;
    let _e452 = (_e450.xyz + (_e444.xyz * _e447.xyz));
    base[0u] = _e452.x;
    base[1u] = _e452.y;
    base[2u] = _e452.z;
    let _e459 = wired_advanced_fog_enabled_u0028_();
    if _e459 {
        let _e460 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e460;
        if override_type_3_14 {
            let _e461 = fogAmount;
            let _e463 = base;
            let _e465 = (_e463.xyz * (1f - _e461));
            base[0u] = _e465.x;
            base[1u] = _e465.y;
            base[2u] = _e465.z;
        } else {
            if override_type_3_15 {
                let _e472 = fogAmount;
                let _e474 = base;
                base = (_e474 * (1f - _e472));
            } else {
                if override_type_3_16 {
                    let _e476 = fogAmount;
                    let _e479 = base[3u];
                    base[3u] = (_e479 * (1f - _e476));
                } else {
                    let _e482 = base;
                    let _e485 = unnamed.advancedFogColorDensity;
                    let _e487 = fogAmount;
                    let _e489 = mix(_e482.xyz, _e485.xyz, vec3(_e487));
                    base[0u] = _e489.x;
                    base[1u] = _e489.y;
                    base[2u] = _e489.z;
                }
            }
        }
    } else {
        if override_type_3_17 {
            let _e496 = base;
            let _e499 = fog[3u];
            let _e501 = (_e496.xyz * (1f - _e499));
            base[0u] = _e501.x;
            base[1u] = _e501.y;
            base[2u] = _e501.z;
        } else {
            if override_type_3_18 {
                let _e508 = base;
                let _e510 = fog[3u];
                base = (_e508 * (1f - _e510));
            } else {
                if override_type_3_19 {
                    let _e514 = base[3u];
                    let _e516 = fog[3u];
                    base[3u] = (_e514 * (1f - _e516));
                } else {
                    let _e520 = base;
                    let _e521 = fog;
                    let _e523 = unnamed.fogColor;
                    let _e526 = fog[3u];
                    base = mix(_e520, (_e521 * _e523), vec4(_e526));
                }
            }
        }
    }
    if override_type_3_20 {
        let _e530 = base[3u];
        if (_e530 == 0f) {
            discard;
        }
    } else {
        if override_type_3_21 {
            let _e532 = base;
            let _e534 = base;
            if (dot(_e532.xyz, _e534.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e538 = base;
    out_color = _e538;
    param_33 = 1f;
    wiredTemporalWriteAux_u0028_f1_u003b((&param_33));
    return;
}

@fragment
fn main(@location(12) @interpolate(flat) temporalOutcome: u32, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @builtin(position) gl_FragCoord: vec4<f32>, @location(7) shadowData: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(3) frag_tex_coord2_: vec2<f32>, @location(4) fog_tex_coord: vec2<f32>, @location(0) frag_color0In: vec4<f32>, @location(5) frag_color1In: vec4<f32>, @location(6) frag_color2In: vec4<f32>) -> FragmentOutput {
    temporalOutcome_1 = temporalOutcome;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
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
    let _e27 = out_temporal_velocity;
    let _e28 = out_temporal_validity;
    let _e29 = out_color;
    return FragmentOutput(_e27, _e28, _e29);
}
