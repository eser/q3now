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
var<private> fog_tex_coord_1: vec2<f32>;
var<private> frag_color0In_1: vec4<f32>;
var<private> frag_color1In_1: vec4<f32>;
var<private> out_color: vec4<f32>;

fn wiredTemporalFinite2_u0028_vf2_u003b(value: ptr<function, vec2<f32>>) -> bool {
    var phi_97_: bool;

    let _e105 = (*value);
    let _e106 = (*value);
    let _e108 = all((_e105 == _e106));
    phi_97_ = _e108;
    if _e108 {
        let _e109 = (*value);
        phi_97_ = all((abs(_e109) <= vec2<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e114 = phi_97_;
    return _e114;
}

fn wiredTemporalFinite4_u0028_vf4_u003b(value_1: ptr<function, vec4<f32>>) -> bool {
    var phi_82_: bool;

    let _e105 = (*value_1);
    let _e106 = (*value_1);
    let _e108 = all((_e105 == _e106));
    phi_82_ = _e108;
    if _e108 {
        let _e109 = (*value_1);
        phi_82_ = all((abs(_e109) <= vec4<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e114 = phi_82_;
    return _e114;
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
    let _e115 = temporalOutcome_1;
    let _e116 = (_e115 != 1u);
    phi_120_ = _e116;
    if !(_e116) {
        let _e118 = temporalCurrentClip_1;
        param = _e118;
        let _e119 = wiredTemporalFinite4_u0028_vf4_u003b((&param));
        phi_120_ = !(_e119);
    }
    let _e122 = phi_120_;
    phi_129_ = _e122;
    if !(_e122) {
        let _e124 = temporalPreviousClip_1;
        param_1 = _e124;
        let _e125 = wiredTemporalFinite4_u0028_vf4_u003b((&param_1));
        phi_129_ = !(_e125);
    }
    let _e128 = phi_129_;
    phi_139_ = _e128;
    if !(_e128) {
        let _e131 = temporalCurrentClip_1[3u];
        phi_139_ = (_e131 <= 0.000001f);
    }
    let _e134 = phi_139_;
    phi_146_ = _e134;
    if !(_e134) {
        let _e137 = temporalPreviousClip_1[3u];
        phi_146_ = (_e137 <= 0.000001f);
    }
    let _e140 = phi_146_;
    if _e140 {
        return;
    }
    let _e141 = temporalCurrentClip_1;
    let _e144 = temporalCurrentClip_1[3u];
    currentNdc = (_e141.xy / vec2(_e144));
    let _e147 = temporalPreviousClip_1;
    let _e150 = temporalPreviousClip_1[3u];
    previousNdc = (_e147.xy / vec2(_e150));
    let _e153 = currentNdc;
    param_2 = _e153;
    let _e154 = wiredTemporalFinite2_u0028_vf2_u003b((&param_2));
    let _e155 = !(_e154);
    phi_175_ = _e155;
    if !(_e155) {
        let _e157 = previousNdc;
        param_3 = _e157;
        let _e158 = wiredTemporalFinite2_u0028_vf2_u003b((&param_3));
        phi_175_ = !(_e158);
    }
    let _e161 = phi_175_;
    if _e161 {
        return;
    }
    let _e162 = currentNdc;
    currentUv = ((_e162 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e165 = previousNdc;
    previousUv = ((_e165 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e168 = currentUv;
    let _e169 = previousUv;
    velocity = (_e168 - _e169);
    let _e171 = velocity;
    param_4 = _e171;
    let _e172 = wiredTemporalFinite2_u0028_vf2_u003b((&param_4));
    if !(_e172) {
        return;
    }
    let _e174 = velocity;
    out_temporal_velocity = _e174;
    let _e175 = (*coverageConfidence);
    out_temporal_validity = clamp(_e175, 0f, 1f);
    return;
}

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e107 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e107 + 0.5f));
    let _e112 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e114 = fogType;
    let _e117 = fogType;
    return (((_e112 > 0.5f) && (_e114 >= 1i)) && (_e117 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e107 = wired_advanced_fog_enabled_u0028_();
    if !(_e107) {
        return 0f;
    }
    let _e110 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e110, 0.000001f));
    let _e115 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e115 + 0.5f));
    let _e118 = fogType_1;
    if (_e118 == 1i) {
        let _e122 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e122 <= 0f) {
            return 0f;
        }
        let _e124 = viewDepth;
        let _e127 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e124 / _e127), 0f, 1f);
    }
    let _e132 = unnamed.advancedFogColorDensity[3u];
    let _e134 = viewDepth;
    opticalDepth = (max(_e132, 0f) * _e134);
    let _e136 = fogType_1;
    if (_e136 == 2i) {
        let _e138 = opticalDepth;
        return clamp((1f - exp(-(_e138))), 0f, 1f);
    }
    let _e143 = opticalDepth;
    let _e144 = opticalDepth;
    return clamp((1f - exp(-((_e143 * _e144)))), 0f, 1f);
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

    let _e114 = (*c);
    let _e117 = unnamed.cascadeMVP[_e114];
    let _e118 = (*worldPos);
    sc4_ = (_e117 * vec4<f32>(_e118.x, _e118.y, _e118.z, 1f));
    let _e124 = sc4_;
    let _e127 = sc4_[3u];
    sc = (_e124.xyz / vec3(_e127));
    let _e130 = sc;
    let _e134 = ((_e130.xy * 0.5f) + vec2(0.5f));
    sc[0u] = _e134.x;
    sc[1u] = _e134.y;
    let _e140 = sc[0u];
    let _e141 = (_e140 < 0f);
    phi_470_ = _e141;
    if !(_e141) {
        let _e144 = sc[0u];
        phi_470_ = (_e144 > 1f);
    }
    let _e147 = phi_470_;
    phi_477_ = _e147;
    if !(_e147) {
        let _e150 = sc[1u];
        phi_477_ = (_e150 < 0f);
    }
    let _e153 = phi_477_;
    phi_484_ = _e153;
    if !(_e153) {
        let _e156 = sc[1u];
        phi_484_ = (_e156 > 1f);
    }
    let _e159 = phi_484_;
    phi_491_ = _e159;
    if !(_e159) {
        let _e162 = sc[2u];
        phi_491_ = (_e162 > 1f);
    }
    let _e165 = phi_491_;
    if _e165 {
        return 1f;
    }
    let _e166 = (*c);
    layer = f32(_e166);
    let _e168 = textureDimensions(shadowMap, 0i);
    texelSize = (vec2(1f) / vec2<f32>(vec2<i32>(_e168).xy));
    let _e175 = sc[2u];
    currentDepth = (_e175 - shadow_bias);
    shadow = 0f;
    if override_type_3_ {
        let _e177 = currentDepth;
        let _e178 = sc;
        let _e179 = _e178.xy;
        let _e180 = layer;
        let _e183 = vec3<f32>(_e179.x, _e179.y, _e180);
        let _e189 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e183.x, _e183.y), i32(_e183.z));
        shadow = step(_e177, _e189.x);
    } else {
        if override_type_3_1 {
            let _e192 = currentDepth;
            let _e193 = sc;
            let _e194 = _e193.xy;
            let _e195 = layer;
            let _e198 = vec3<f32>(_e194.x, _e194.y, _e195);
            let _e204 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e198.x, _e198.y), i32(_e198.z));
            let _e207 = shadow;
            shadow = (_e207 + step(_e192, _e204.x));
            let _e209 = currentDepth;
            let _e210 = sc;
            let _e213 = texelSize[0u];
            let _e215 = (_e210.xy + vec2<f32>(_e213, 0f));
            let _e216 = layer;
            let _e219 = vec3<f32>(_e215.x, _e215.y, _e216);
            let _e225 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e219.x, _e219.y), i32(_e219.z));
            let _e228 = shadow;
            shadow = (_e228 + step(_e209, _e225.x));
            let _e230 = currentDepth;
            let _e231 = sc;
            let _e234 = texelSize[0u];
            let _e236 = (_e231.xy - vec2<f32>(_e234, 0f));
            let _e237 = layer;
            let _e240 = vec3<f32>(_e236.x, _e236.y, _e237);
            let _e246 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e240.x, _e240.y), i32(_e240.z));
            let _e249 = shadow;
            shadow = (_e249 + step(_e230, _e246.x));
            let _e251 = currentDepth;
            let _e252 = sc;
            let _e255 = texelSize[1u];
            let _e257 = (_e252.xy + vec2<f32>(0f, _e255));
            let _e258 = layer;
            let _e261 = vec3<f32>(_e257.x, _e257.y, _e258);
            let _e267 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e261.x, _e261.y), i32(_e261.z));
            let _e270 = shadow;
            shadow = (_e270 + step(_e251, _e267.x));
            let _e272 = currentDepth;
            let _e273 = sc;
            let _e276 = texelSize[1u];
            let _e278 = (_e273.xy - vec2<f32>(0f, _e276));
            let _e279 = layer;
            let _e282 = vec3<f32>(_e278.x, _e278.y, _e279);
            let _e288 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e282.x, _e282.y), i32(_e282.z));
            let _e291 = shadow;
            shadow = (_e291 + step(_e272, _e288.x));
            let _e293 = shadow;
            shadow = (_e293 / 5f);
        } else {
            x = -1i;
            loop {
                let _e295 = x;
                if (_e295 <= 1i) {
                    y = -1i;
                    loop {
                        let _e297 = y;
                        if (_e297 <= 1i) {
                            let _e299 = currentDepth;
                            let _e300 = sc;
                            let _e302 = x;
                            let _e304 = y;
                            let _e307 = texelSize;
                            let _e309 = (_e300.xy + (vec2<f32>(f32(_e302), f32(_e304)) * _e307));
                            let _e310 = layer;
                            let _e313 = vec3<f32>(_e309.x, _e309.y, _e310);
                            let _e319 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e313.x, _e313.y), i32(_e313.z));
                            let _e322 = shadow;
                            shadow = (_e322 + step(_e299, _e319.x));
                            continue;
                        } else {
                            break;
                        }
                        continuing {
                            let _e324 = y;
                            y = (_e324 + 1i);
                        }
                    }
                    continue;
                } else {
                    break;
                }
                continuing {
                    let _e326 = x;
                    x = (_e326 + 1i);
                }
            }
            let _e328 = shadow;
            shadow = (_e328 / 9f);
        }
    }
    let _e330 = shadow;
    return _e330;
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

    let _e121 = unnamed.cascadeSplits;
    let _e122 = (*viewDepth_1);
    cmp = step(_e121, vec4(_e122));
    let _e126 = cmp[0u];
    let _e128 = cmp[1u];
    let _e131 = cmp[2u];
    let _e134 = cmp[3u];
    cascade = min(i32((((_e126 + _e128) + _e131) + _e134)), 3i);
    let _e138 = cascade;
    (*outCascade) = _e138;
    let _e139 = cascade;
    if (_e139 == 0i) {
        local = 0f;
    } else {
        let _e141 = cascade;
        let _e146 = unnamed.cascadeSplits[max((_e141 - 1i), 0i)];
        local = _e146;
    }
    let _e147 = local;
    prevSplit = _e147;
    let _e148 = cascade;
    let _e151 = unnamed.cascadeSplits[_e148];
    farSplit = _e151;
    let _e152 = farSplit;
    let _e153 = prevSplit;
    blendRange = max((0.1f * (_e152 - _e153)), 1f);
    let _e157 = farSplit;
    let _e158 = (*viewDepth_1);
    let _e160 = blendRange;
    blendT = clamp(((_e157 - _e158) / _e160), 0f, 1f);
    let _e163 = cascade;
    param_5 = _e163;
    let _e164 = (*worldPos_1);
    param_6 = _e164;
    let _e165 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param_5), (&param_6));
    s0_ = _e165;
    let _e166 = cascade;
    param_7 = min((_e166 + 1i), 3i);
    let _e169 = (*worldPos_1);
    param_8 = _e169;
    let _e170 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param_7), (&param_8));
    s1_ = _e170;
    let _e171 = s1_;
    let _e172 = s0_;
    let _e173 = blendT;
    return mix(_e171, _e172, _e173);
}

fn wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b(rgb: ptr<function, vec3<f32>>, lm_uv: ptr<function, vec2<f32>>) -> vec3<f32> {
    var sun_mask: f32;
    var shadow_1: f32;
    var ignoredCascade: i32;
    var param_9: vec3<f32>;
    var param_10: f32;
    var param_11: i32;

    let _e115 = unnamed.packed_indices[1i][3u];
    let _e121 = unnamed.packed_indices[1i][3u];
    let _e126 = (*lm_uv);
    let _e127 = textureSample(wired_bindless_images[(_e115 & 4095u)], wired_bindless_samplers[((_e121 >> bitcast<u32>(12i)) & 255u)], _e126);
    sun_mask = _e127.x;
    let _e129 = sun_mask;
    if (_e129 > 0.001f) {
        let _e131 = shadowData_1;
        param_9 = _e131.xyz;
        let _e134 = shadowData_1[3u];
        param_10 = _e134;
        let _e135 = sampleShadow_u0028_vf3_u003b_f1_u003b_i1_u003b((&param_9), (&param_10), (&param_11));
        let _e136 = param_11;
        ignoredCascade = _e136;
        shadow_1 = _e135;
        let _e137 = shadow_1;
        let _e138 = sun_mask;
        let _e140 = (*rgb);
        (*rgb) = (_e140 * mix(1f, _e137, _e138));
    }
    let _e142 = (*rgb);
    return _e142;
}

fn wired_apply_sun_shadow_operand_u0028_vf3_u003b(rgb_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var param_12: vec3<f32>;
    var param_13: vec2<f32>;
    var param_14: vec3<f32>;
    var param_15: vec2<f32>;

    if override_type_3_2 {
        let _e109 = (*rgb_1);
        param_12 = _e109;
        let _e110 = frag_tex_coord0_1;
        param_13 = _e110;
        let _e111 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_12), (&param_13));
        return _e111;
    }
    if override_type_3_3 {
        let _e112 = (*rgb_1);
        param_14 = _e112;
        let _e113 = frag_tex_coord1_1;
        param_15 = _e113;
        let _e114 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_14), (&param_15));
        return _e114;
    }
    let _e115 = (*rgb_1);
    return _e115;
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb_2: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e107 = (*rgb_2);
    let _e110 = unnamed.worldLightParams[0u];
    boosted = (_e107 * _e110);
    let _e113 = boosted[0u];
    let _e115 = boosted[1u];
    let _e117 = boosted[2u];
    peak = max(_e113, max(_e115, _e117));
    let _e120 = peak;
    if (_e120 > 1f) {
        let _e122 = peak;
        let _e123 = boosted;
        boosted = (_e123 / vec3(_e122));
    }
    let _e126 = boosted;
    return _e126;
}

fn sRGBToLinear_u0028_vf3_u003b(c_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e108 = (*c_1);
    (*c_1) = max(_e108, vec3<f32>(0f, 0f, 0f));
    let _e110 = (*c_1);
    cutoff = (_e110 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e112 = (*c_1);
    lo = (_e112 / vec3(12.92f));
    let _e115 = (*c_1);
    hi = pow(((_e115 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e120 = hi;
    let _e121 = lo;
    let _e122 = cutoff;
    return mix(_e120, _e121, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e122));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_2: vec4<f32>;
    var param_16: vec3<f32>;
    var param_17: vec3<f32>;

    let _e110 = (*role);
    let _e112 = (*role);
    let _e117 = unnamed.packed_indices[(_e110 / 4u)][(_e112 % 4u)];
    let _e120 = (*role);
    let _e122 = (*role);
    let _e127 = unnamed.packed_indices[(_e120 / 4u)][(_e122 % 4u)];
    let _e132 = (*uv);
    let _e133 = textureSample(wired_bindless_images[(_e117 & 4095u)], wired_bindless_samplers[((_e127 >> bitcast<u32>(12i)) & 255u)], _e132);
    c_2 = _e133;
    let _e134 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e134))) == 0i) {
        let _e139 = c_2;
        param_16 = _e139.xyz;
        let _e141 = sRGBToLinear_u0028_vf3_u003b((&param_16));
        c_2[0u] = _e141.x;
        c_2[1u] = _e141.y;
        c_2[2u] = _e141.z;
    }
    let _e148 = (*slot);
    if (lightmap_slot == (_e148 + 1i)) {
        let _e151 = c_2;
        param_17 = _e151.xyz;
        let _e153 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_17));
        c_2[0u] = _e153.x;
        c_2[1u] = _e153.y;
        c_2[2u] = _e153.z;
    }
    let _e160 = c_2;
    return _e160;
}

fn main_1() {
    var fog: vec4<f32>;
    var frag_color0_: vec4<f32>;
    var param_18: vec3<f32>;
    var frag_color1_: vec4<f32>;
    var param_19: vec3<f32>;
    var sampledColor0_: vec4<f32>;
    var param_20: u32;
    var param_21: vec2<f32>;
    var param_22: i32;
    var sampledColor1_: vec4<f32>;
    var param_23: u32;
    var param_24: vec2<f32>;
    var param_25: i32;
    var color0_: vec4<f32>;
    var color1_: vec4<f32>;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var color1_2: vec4<f32>;
    var color1_3: vec4<f32>;
    var color1_4: vec4<f32>;
    var color1_5: vec4<f32>;
    var color1_6: vec4<f32>;
    var param_26: vec3<f32>;
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var fogAmount: f32;
    var param_27: f32;

    let _e135 = unnamed.packed_indices[0i][3u];
    let _e141 = unnamed.packed_indices[0i][3u];
    let _e146 = fog_tex_coord_1;
    let _e147 = textureSample(wired_bindless_images[(_e135 & 4095u)], wired_bindless_samplers[((_e141 >> bitcast<u32>(12i)) & 255u)], _e146);
    fog = _e147;
    let _e148 = frag_color0In_1;
    param_18 = _e148.xyz;
    let _e150 = sRGBToLinear_u0028_vf3_u003b((&param_18));
    let _e152 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e150.x, _e150.y, _e150.z, _e152);
    let _e157 = frag_color1In_1;
    param_19 = _e157.xyz;
    let _e159 = sRGBToLinear_u0028_vf3_u003b((&param_19));
    let _e161 = frag_color1In_1[3u];
    frag_color1_ = vec4<f32>(_e159.x, _e159.y, _e159.z, _e161);
    param_20 = 0u;
    let _e166 = frag_tex_coord0_1;
    param_21 = _e166;
    param_22 = 0i;
    let _e167 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_20), (&param_21), (&param_22));
    sampledColor0_ = _e167;
    param_23 = 1u;
    let _e168 = frag_tex_coord1_1;
    param_24 = _e168;
    param_25 = 1i;
    let _e169 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_23), (&param_24), (&param_25));
    sampledColor1_ = _e169;
    let _e170 = sampledColor0_;
    let _e171 = frag_color0_;
    color0_ = (_e170 * _e171);
    if override_type_3_6 {
        let _e173 = sampledColor1_;
        let _e174 = frag_color1_;
        color1_ = (_e173 * _e174);
        let _e176 = color0_;
        let _e178 = color1_;
        let _e180 = (_e176.xyz + _e178.xyz);
        let _e182 = color0_[3u];
        let _e184 = color1_[3u];
        base = vec4<f32>(_e180.x, _e180.y, _e180.z, (_e182 * _e184));
    } else {
        if override_type_3_7 {
            let _e190 = sampledColor1_;
            let _e191 = frag_color1_;
            color1_1 = (_e190 * _e191);
            let _e194 = color0_[3u];
            let _e195 = color0_;
            color0_ = (_e195 * _e194);
            let _e198 = color1_1[3u];
            let _e199 = color1_1;
            color1_1 = (_e199 * _e198);
            let _e201 = color0_;
            let _e203 = color1_1;
            let _e205 = (_e201.xyz + _e203.xyz);
            let _e207 = color0_[3u];
            let _e209 = color1_1[3u];
            base = vec4<f32>(_e205.x, _e205.y, _e205.z, (_e207 * _e209));
        } else {
            if override_type_3_8 {
                let _e215 = sampledColor1_;
                let _e216 = frag_color1_;
                color1_2 = (_e215 * _e216);
                let _e219 = color0_[3u];
                let _e221 = color0_;
                color0_ = (_e221 * (1f - _e219));
                let _e224 = color1_2[3u];
                let _e226 = color1_2;
                color1_2 = (_e226 * (1f - _e224));
                let _e228 = color0_;
                let _e230 = color1_2;
                let _e232 = (_e228.xyz + _e230.xyz);
                let _e234 = color0_[3u];
                let _e236 = color1_2[3u];
                base = vec4<f32>(_e232.x, _e232.y, _e232.z, (_e234 * _e236));
            } else {
                if override_type_3_9 {
                    let _e242 = sampledColor1_;
                    let _e243 = frag_color1_;
                    color1_3 = (_e242 * _e243);
                    let _e245 = color0_;
                    let _e246 = color1_3;
                    let _e248 = color1_3[3u];
                    base = mix(_e245, _e246, vec4(_e248));
                } else {
                    if override_type_3_10 {
                        let _e251 = sampledColor1_;
                        let _e252 = frag_color1_;
                        color1_4 = (_e251 * _e252);
                        let _e254 = color1_4;
                        let _e255 = color0_;
                        let _e257 = color1_4[3u];
                        base = mix(_e254, _e255, vec4(_e257));
                    } else {
                        if override_type_3_11 {
                            let _e260 = sampledColor1_;
                            let _e261 = frag_color1_;
                            color1_5 = (_e260 * _e261);
                            let _e263 = color1_5;
                            let _e265 = color1_5[3u];
                            let _e268 = color0_;
                            base = ((_e263 + vec4(_e265)) * _e268);
                        } else {
                            let _e270 = sampledColor1_;
                            let _e271 = frag_color1_;
                            color1_6 = (_e270 * _e271);
                            let _e273 = color0_;
                            let _e275 = color1_6;
                            let _e277 = (_e273.xyz * _e275.xyz);
                            base[0u] = _e277.x;
                            base[1u] = _e277.y;
                            base[2u] = _e277.z;
                            let _e285 = color0_[3u];
                            let _e287 = color1_6[3u];
                            base[3u] = (_e285 * _e287);
                        }
                    }
                }
            }
        }
    }
    let _e290 = base;
    param_26 = _e290.xyz;
    let _e292 = wired_apply_sun_shadow_operand_u0028_vf3_u003b((&param_26));
    base[0u] = _e292.x;
    base[1u] = _e292.y;
    base[2u] = _e292.z;
    if override_type_3_12 {
        let _e301 = unnamed.worldLightParams[1u];
        wetness = clamp(_e301, 0f, 1f);
        let _e305 = unnamed.worldLightParams[2u];
        frost = clamp(_e305, 0f, 1f);
        let _e307 = base;
        luminance = dot(_e307.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e310 = wetness;
        let _e312 = base;
        let _e314 = (_e312.xyz * mix(1f, 0.82f, _e310));
        base[0u] = _e314.x;
        base[1u] = _e314.y;
        base[2u] = _e314.z;
        let _e321 = base;
        let _e323 = luminance;
        let _e325 = luminance;
        let _e327 = luminance;
        let _e329 = frost;
        let _e332 = mix(_e321.xyz, vec3<f32>((_e323 * 0.88f), (_e325 * 0.94f), _e327), vec3((_e329 * 0.55f)));
        base[0u] = _e332.x;
        base[1u] = _e332.y;
        base[2u] = _e332.z;
    }
    let _e341 = unnamed.materialFactors[0u];
    if (_e341 >= 0f) {
        let _e344 = unnamed.materialBaseColor;
        let _e345 = base;
        base = (_e345 * _e344);
    }
    let _e347 = color0_;
    let _e350 = unnamed.emissionRadiance;
    let _e353 = base;
    let _e355 = (_e353.xyz + (_e347.xyz * _e350.xyz));
    base[0u] = _e355.x;
    base[1u] = _e355.y;
    base[2u] = _e355.z;
    let _e362 = wired_advanced_fog_enabled_u0028_();
    if _e362 {
        let _e363 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e363;
        if override_type_3_13 {
            let _e364 = fogAmount;
            let _e366 = base;
            let _e368 = (_e366.xyz * (1f - _e364));
            base[0u] = _e368.x;
            base[1u] = _e368.y;
            base[2u] = _e368.z;
        } else {
            if override_type_3_14 {
                let _e375 = fogAmount;
                let _e377 = base;
                base = (_e377 * (1f - _e375));
            } else {
                if override_type_3_15 {
                    let _e379 = fogAmount;
                    let _e382 = base[3u];
                    base[3u] = (_e382 * (1f - _e379));
                } else {
                    let _e385 = base;
                    let _e388 = unnamed.advancedFogColorDensity;
                    let _e390 = fogAmount;
                    let _e392 = mix(_e385.xyz, _e388.xyz, vec3(_e390));
                    base[0u] = _e392.x;
                    base[1u] = _e392.y;
                    base[2u] = _e392.z;
                }
            }
        }
    } else {
        if override_type_3_16 {
            let _e399 = base;
            let _e402 = fog[3u];
            let _e404 = (_e399.xyz * (1f - _e402));
            base[0u] = _e404.x;
            base[1u] = _e404.y;
            base[2u] = _e404.z;
        } else {
            if override_type_3_17 {
                let _e411 = base;
                let _e413 = fog[3u];
                base = (_e411 * (1f - _e413));
            } else {
                if override_type_3_18 {
                    let _e417 = base[3u];
                    let _e419 = fog[3u];
                    base[3u] = (_e417 * (1f - _e419));
                } else {
                    let _e423 = base;
                    let _e424 = fog;
                    let _e426 = unnamed.fogColor;
                    let _e429 = fog[3u];
                    base = mix(_e423, (_e424 * _e426), vec4(_e429));
                }
            }
        }
    }
    if override_type_3_19 {
        let _e433 = base[3u];
        if (_e433 == 0f) {
            discard;
        }
    } else {
        if override_type_3_20 {
            let _e435 = base;
            let _e437 = base;
            if (dot(_e435.xyz, _e437.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e441 = base;
    out_color = _e441;
    param_27 = 1f;
    wiredTemporalWriteAux_u0028_f1_u003b((&param_27));
    return;
}

@fragment
fn main(@location(12) @interpolate(flat) temporalOutcome: u32, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @builtin(position) gl_FragCoord: vec4<f32>, @location(7) shadowData: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(4) fog_tex_coord: vec2<f32>, @location(0) frag_color0In: vec4<f32>, @location(5) frag_color1In: vec4<f32>) -> FragmentOutput {
    temporalOutcome_1 = temporalOutcome;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    gl_FragCoord_1 = gl_FragCoord;
    shadowData_1 = shadowData;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    fog_tex_coord_1 = fog_tex_coord;
    frag_color0In_1 = frag_color0In;
    frag_color1In_1 = frag_color1In;
    main_1();
    let _e23 = out_temporal_velocity;
    let _e24 = out_temporal_validity;
    let _e25 = out_color;
    return FragmentOutput(_e23, _e24, _e25);
}
