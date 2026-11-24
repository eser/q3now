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
    @location(2) member_1: vec4<f32>,
    @location(0) member_2: vec4<f32>,
}

@id(4) override tex_domain: i32 = 0i;
@id(26) override lightmap_slot: i32 = 0i;
@id(29) override shadow_bias: f32 = 0.005f;
@id(28) override shadow_pcf: i32 = 5i;
override override_type_3_: bool = (shadow_pcf <= 1i);
override override_type_3_1: bool = (shadow_pcf <= 5i);
override override_type_3_2: bool = (lightmap_slot == 1i);
@id(8) override identity_color: f32 = 1f;
@id(9) override identity_alpha: f32 = 1f;
override override_type_3_3: bool = (lightmap_slot != 0i);
@id(10) override acff: i32 = 0i;
override override_type_3_4: bool = (acff == 1i);
override override_type_3_5: bool = (acff == 2i);
override override_type_3_6: bool = (acff == 3i);
override override_type_3_7: bool = (acff == 1i);
override override_type_3_8: bool = (acff == 2i);
override override_type_3_9: bool = (acff == 3i);
@id(7) override discard_mode: i32 = 0i;
override override_type_3_10: bool = (discard_mode == 1i);
override override_type_3_11: bool = (discard_mode == 2i);
@id(3) override alpha_to_coverage: i32 = 0i;

var<private> out_temporal_velocity: vec2<f32>;
var<private> out_temporal_validity: vec4<f32>;
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
var<private> fog_tex_coord_1: vec2<f32>;
var<private> out_color: vec4<f32>;

fn wiredTemporalFinite2_u0028_vf2_u003b(value: ptr<function, vec2<f32>>) -> bool {
    var phi_98_: bool;

    let _e93 = (*value);
    let _e94 = (*value);
    let _e96 = all((_e93 == _e94));
    phi_98_ = _e96;
    if _e96 {
        let _e97 = (*value);
        phi_98_ = all((abs(_e97) <= vec2<f32>(0x1.fffffep+127f, 0x1.fffffep+127f)));
    }
    let _e102 = phi_98_;
    return _e102;
}

fn wiredTemporalFinite4_u0028_vf4_u003b(value_1: ptr<function, vec4<f32>>) -> bool {
    var phi_83_: bool;

    let _e93 = (*value_1);
    let _e94 = (*value_1);
    let _e96 = all((_e93 == _e94));
    phi_83_ = _e96;
    if _e96 {
        let _e97 = (*value_1);
        phi_83_ = all((abs(_e97) <= vec4<f32>(0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f)));
    }
    let _e102 = phi_83_;
    return _e102;
}

fn wiredTemporalWriteAux_u0028_f1_u003b_vf4_u003b(coverageConfidence: ptr<function, f32>, sceneColor: ptr<function, vec4<f32>>) {
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
    var phi_122_: bool;
    var phi_131_: bool;
    var phi_141_: bool;
    var phi_148_: bool;
    var phi_177_: bool;

    out_temporal_velocity = vec2<f32>(0f, 0f);
    out_temporal_validity = vec4<f32>(0f, 0f, 0f, 0f);
    let _e104 = temporalOutcome_1;
    let _e105 = (_e104 != 1u);
    phi_122_ = _e105;
    if !(_e105) {
        let _e107 = temporalCurrentClip_1;
        param = _e107;
        let _e108 = wiredTemporalFinite4_u0028_vf4_u003b((&param));
        phi_122_ = !(_e108);
    }
    let _e111 = phi_122_;
    phi_131_ = _e111;
    if !(_e111) {
        let _e113 = temporalPreviousClip_1;
        param_1 = _e113;
        let _e114 = wiredTemporalFinite4_u0028_vf4_u003b((&param_1));
        phi_131_ = !(_e114);
    }
    let _e117 = phi_131_;
    phi_141_ = _e117;
    if !(_e117) {
        let _e120 = temporalCurrentClip_1[3u];
        phi_141_ = (_e120 <= 0.000001f);
    }
    let _e123 = phi_141_;
    phi_148_ = _e123;
    if !(_e123) {
        let _e126 = temporalPreviousClip_1[3u];
        phi_148_ = (_e126 <= 0.000001f);
    }
    let _e129 = phi_148_;
    if _e129 {
        return;
    }
    let _e130 = temporalCurrentClip_1;
    let _e133 = temporalCurrentClip_1[3u];
    currentNdc = (_e130.xy / vec2(_e133));
    let _e136 = temporalPreviousClip_1;
    let _e139 = temporalPreviousClip_1[3u];
    previousNdc = (_e136.xy / vec2(_e139));
    let _e142 = currentNdc;
    param_2 = _e142;
    let _e143 = wiredTemporalFinite2_u0028_vf2_u003b((&param_2));
    let _e144 = !(_e143);
    phi_177_ = _e144;
    if !(_e144) {
        let _e146 = previousNdc;
        param_3 = _e146;
        let _e147 = wiredTemporalFinite2_u0028_vf2_u003b((&param_3));
        phi_177_ = !(_e147);
    }
    let _e150 = phi_177_;
    if _e150 {
        return;
    }
    let _e151 = currentNdc;
    currentUv = ((_e151 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e154 = previousNdc;
    previousUv = ((_e154 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e157 = currentUv;
    let _e158 = previousUv;
    velocity = (_e157 - _e158);
    let _e160 = velocity;
    param_4 = _e160;
    let _e161 = wiredTemporalFinite2_u0028_vf2_u003b((&param_4));
    if !(_e161) {
        return;
    }
    let _e163 = velocity;
    out_temporal_velocity = _e163;
    let _e164 = (*coverageConfidence);
    out_temporal_validity = vec4<f32>(clamp(_e164, 0f, 1f), 0f, 0f, 1f);
    return;
}

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e95 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e95 + 0.5f));
    let _e100 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e102 = fogType;
    let _e105 = fogType;
    return (((_e100 > 0.5f) && (_e102 >= 1i)) && (_e105 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e95 = wired_advanced_fog_enabled_u0028_();
    if !(_e95) {
        return 0f;
    }
    let _e98 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e98, 0.000001f));
    let _e103 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e103 + 0.5f));
    let _e106 = fogType_1;
    if (_e106 == 1i) {
        let _e110 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e110 <= 0f) {
            return 0f;
        }
        let _e112 = viewDepth;
        let _e115 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e112 / _e115), 0f, 1f);
    }
    let _e120 = unnamed.advancedFogColorDensity[3u];
    let _e122 = viewDepth;
    opticalDepth = (max(_e120, 0f) * _e122);
    let _e124 = fogType_1;
    if (_e124 == 2i) {
        let _e126 = opticalDepth;
        return clamp((1f - exp(-(_e126))), 0f, 1f);
    }
    let _e131 = opticalDepth;
    let _e132 = opticalDepth;
    return clamp((1f - exp(-((_e131 * _e132)))), 0f, 1f);
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
    var phi_473_: bool;
    var phi_480_: bool;
    var phi_487_: bool;
    var phi_494_: bool;

    let _e102 = (*c);
    let _e105 = unnamed.cascadeMVP[_e102];
    let _e106 = (*worldPos);
    sc4_ = (_e105 * vec4<f32>(_e106.x, _e106.y, _e106.z, 1f));
    let _e112 = sc4_;
    let _e115 = sc4_[3u];
    sc = (_e112.xyz / vec3(_e115));
    let _e118 = sc;
    let _e122 = ((_e118.xy * 0.5f) + vec2(0.5f));
    sc[0u] = _e122.x;
    sc[1u] = _e122.y;
    let _e128 = sc[0u];
    let _e129 = (_e128 < 0f);
    phi_473_ = _e129;
    if !(_e129) {
        let _e132 = sc[0u];
        phi_473_ = (_e132 > 1f);
    }
    let _e135 = phi_473_;
    phi_480_ = _e135;
    if !(_e135) {
        let _e138 = sc[1u];
        phi_480_ = (_e138 < 0f);
    }
    let _e141 = phi_480_;
    phi_487_ = _e141;
    if !(_e141) {
        let _e144 = sc[1u];
        phi_487_ = (_e144 > 1f);
    }
    let _e147 = phi_487_;
    phi_494_ = _e147;
    if !(_e147) {
        let _e150 = sc[2u];
        phi_494_ = (_e150 > 1f);
    }
    let _e153 = phi_494_;
    if _e153 {
        return 1f;
    }
    let _e154 = (*c);
    layer = f32(_e154);
    let _e156 = textureDimensions(shadowMap, 0i);
    texelSize = (vec2(1f) / vec2<f32>(vec2<i32>(_e156).xy));
    let _e163 = sc[2u];
    currentDepth = (_e163 - shadow_bias);
    shadow = 0f;
    if override_type_3_ {
        let _e165 = currentDepth;
        let _e166 = sc;
        let _e167 = _e166.xy;
        let _e168 = layer;
        let _e171 = vec3<f32>(_e167.x, _e167.y, _e168);
        let _e177 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e171.x, _e171.y), i32(_e171.z));
        shadow = step(_e165, _e177.x);
    } else {
        if override_type_3_1 {
            let _e180 = currentDepth;
            let _e181 = sc;
            let _e182 = _e181.xy;
            let _e183 = layer;
            let _e186 = vec3<f32>(_e182.x, _e182.y, _e183);
            let _e192 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e186.x, _e186.y), i32(_e186.z));
            let _e195 = shadow;
            shadow = (_e195 + step(_e180, _e192.x));
            let _e197 = currentDepth;
            let _e198 = sc;
            let _e201 = texelSize[0u];
            let _e203 = (_e198.xy + vec2<f32>(_e201, 0f));
            let _e204 = layer;
            let _e207 = vec3<f32>(_e203.x, _e203.y, _e204);
            let _e213 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e207.x, _e207.y), i32(_e207.z));
            let _e216 = shadow;
            shadow = (_e216 + step(_e197, _e213.x));
            let _e218 = currentDepth;
            let _e219 = sc;
            let _e222 = texelSize[0u];
            let _e224 = (_e219.xy - vec2<f32>(_e222, 0f));
            let _e225 = layer;
            let _e228 = vec3<f32>(_e224.x, _e224.y, _e225);
            let _e234 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e228.x, _e228.y), i32(_e228.z));
            let _e237 = shadow;
            shadow = (_e237 + step(_e218, _e234.x));
            let _e239 = currentDepth;
            let _e240 = sc;
            let _e243 = texelSize[1u];
            let _e245 = (_e240.xy + vec2<f32>(0f, _e243));
            let _e246 = layer;
            let _e249 = vec3<f32>(_e245.x, _e245.y, _e246);
            let _e255 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e249.x, _e249.y), i32(_e249.z));
            let _e258 = shadow;
            shadow = (_e258 + step(_e239, _e255.x));
            let _e260 = currentDepth;
            let _e261 = sc;
            let _e264 = texelSize[1u];
            let _e266 = (_e261.xy - vec2<f32>(0f, _e264));
            let _e267 = layer;
            let _e270 = vec3<f32>(_e266.x, _e266.y, _e267);
            let _e276 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e270.x, _e270.y), i32(_e270.z));
            let _e279 = shadow;
            shadow = (_e279 + step(_e260, _e276.x));
            let _e281 = shadow;
            shadow = (_e281 / 5f);
        } else {
            x = -1i;
            loop {
                let _e283 = x;
                if (_e283 <= 1i) {
                    y = -1i;
                    loop {
                        let _e285 = y;
                        if (_e285 <= 1i) {
                            let _e287 = currentDepth;
                            let _e288 = sc;
                            let _e290 = x;
                            let _e292 = y;
                            let _e295 = texelSize;
                            let _e297 = (_e288.xy + (vec2<f32>(f32(_e290), f32(_e292)) * _e295));
                            let _e298 = layer;
                            let _e301 = vec3<f32>(_e297.x, _e297.y, _e298);
                            let _e307 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e301.x, _e301.y), i32(_e301.z));
                            let _e310 = shadow;
                            shadow = (_e310 + step(_e287, _e307.x));
                            continue;
                        } else {
                            break;
                        }
                        continuing {
                            let _e312 = y;
                            y = (_e312 + 1i);
                        }
                    }
                    continue;
                } else {
                    break;
                }
                continuing {
                    let _e314 = x;
                    x = (_e314 + 1i);
                }
            }
            let _e316 = shadow;
            shadow = (_e316 / 9f);
        }
    }
    let _e318 = shadow;
    return _e318;
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

    let _e109 = unnamed.cascadeSplits;
    let _e110 = (*viewDepth_1);
    cmp = step(_e109, vec4(_e110));
    let _e114 = cmp[0u];
    let _e116 = cmp[1u];
    let _e119 = cmp[2u];
    let _e122 = cmp[3u];
    cascade = min(i32((((_e114 + _e116) + _e119) + _e122)), 3i);
    let _e126 = cascade;
    (*outCascade) = _e126;
    let _e127 = cascade;
    if (_e127 == 0i) {
        local = 0f;
    } else {
        let _e129 = cascade;
        let _e134 = unnamed.cascadeSplits[max((_e129 - 1i), 0i)];
        local = _e134;
    }
    let _e135 = local;
    prevSplit = _e135;
    let _e136 = cascade;
    let _e139 = unnamed.cascadeSplits[_e136];
    farSplit = _e139;
    let _e140 = farSplit;
    let _e141 = prevSplit;
    blendRange = max((0.1f * (_e140 - _e141)), 1f);
    let _e145 = farSplit;
    let _e146 = (*viewDepth_1);
    let _e148 = blendRange;
    blendT = clamp(((_e145 - _e146) / _e148), 0f, 1f);
    let _e151 = cascade;
    param_5 = _e151;
    let _e152 = (*worldPos_1);
    param_6 = _e152;
    let _e153 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param_5), (&param_6));
    s0_ = _e153;
    let _e154 = cascade;
    param_7 = min((_e154 + 1i), 3i);
    let _e157 = (*worldPos_1);
    param_8 = _e157;
    let _e158 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param_7), (&param_8));
    s1_ = _e158;
    let _e159 = s1_;
    let _e160 = s0_;
    let _e161 = blendT;
    return mix(_e159, _e160, _e161);
}

fn wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b(rgb: ptr<function, vec3<f32>>, lm_uv: ptr<function, vec2<f32>>) -> vec3<f32> {
    var sun_mask: f32;
    var shadow_1: f32;
    var ignoredCascade: i32;
    var param_9: vec3<f32>;
    var param_10: f32;
    var param_11: i32;

    let _e103 = unnamed.packed_indices[1i][3u];
    let _e109 = unnamed.packed_indices[1i][3u];
    let _e114 = (*lm_uv);
    let _e115 = textureSample(wired_bindless_images[(_e103 & 4095u)], wired_bindless_samplers[((_e109 >> bitcast<u32>(12i)) & 255u)], _e114);
    sun_mask = _e115.x;
    let _e117 = sun_mask;
    if (_e117 > 0.001f) {
        let _e119 = shadowData_1;
        param_9 = _e119.xyz;
        let _e122 = shadowData_1[3u];
        param_10 = _e122;
        let _e123 = sampleShadow_u0028_vf3_u003b_f1_u003b_i1_u003b((&param_9), (&param_10), (&param_11));
        let _e124 = param_11;
        ignoredCascade = _e124;
        shadow_1 = _e123;
        let _e125 = shadow_1;
        let _e126 = sun_mask;
        let _e128 = (*rgb);
        (*rgb) = (_e128 * mix(1f, _e125, _e126));
    }
    let _e130 = (*rgb);
    return _e130;
}

fn wired_apply_sun_shadow_operand_u0028_vf3_u003b(rgb_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var param_12: vec3<f32>;
    var param_13: vec2<f32>;

    if override_type_3_2 {
        let _e95 = (*rgb_1);
        param_12 = _e95;
        let _e96 = frag_tex_coord0_1;
        param_13 = _e96;
        let _e97 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_12), (&param_13));
        return _e97;
    }
    let _e98 = (*rgb_1);
    return _e98;
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb_2: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e95 = (*rgb_2);
    let _e98 = unnamed.worldLightParams[0u];
    boosted = (_e95 * _e98);
    let _e101 = boosted[0u];
    let _e103 = boosted[1u];
    let _e105 = boosted[2u];
    peak = max(_e101, max(_e103, _e105));
    let _e108 = peak;
    if (_e108 > 1f) {
        let _e110 = peak;
        let _e111 = boosted;
        boosted = (_e111 / vec3(_e110));
    }
    let _e114 = boosted;
    return _e114;
}

fn sRGBToLinear_u0028_vf3_u003b(c_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e96 = (*c_1);
    (*c_1) = max(_e96, vec3<f32>(0f, 0f, 0f));
    let _e98 = (*c_1);
    cutoff = (_e98 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e100 = (*c_1);
    lo = (_e100 / vec3(12.92f));
    let _e103 = (*c_1);
    hi = pow(((_e103 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e108 = hi;
    let _e109 = lo;
    let _e110 = cutoff;
    return mix(_e108, _e109, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e110));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_2: vec4<f32>;
    var param_14: vec3<f32>;
    var param_15: vec3<f32>;

    let _e98 = (*role);
    let _e100 = (*role);
    let _e105 = unnamed.packed_indices[(_e98 / 4u)][(_e100 % 4u)];
    let _e108 = (*role);
    let _e110 = (*role);
    let _e115 = unnamed.packed_indices[(_e108 / 4u)][(_e110 % 4u)];
    let _e120 = (*uv);
    let _e121 = textureSample(wired_bindless_images[(_e105 & 4095u)], wired_bindless_samplers[((_e115 >> bitcast<u32>(12i)) & 255u)], _e120);
    c_2 = _e121;
    let _e122 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e122))) == 0i) {
        let _e127 = c_2;
        param_14 = _e127.xyz;
        let _e129 = sRGBToLinear_u0028_vf3_u003b((&param_14));
        c_2[0u] = _e129.x;
        c_2[1u] = _e129.y;
        c_2[2u] = _e129.z;
    }
    let _e136 = (*slot);
    if (lightmap_slot == (_e136 + 1i)) {
        let _e139 = c_2;
        param_15 = _e139.xyz;
        let _e141 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_15));
        c_2[0u] = _e141.x;
        c_2[1u] = _e141.y;
        c_2[2u] = _e141.z;
    }
    let _e148 = c_2;
    return _e148;
}

fn main_1() {
    var fog: vec4<f32>;
    var sampledColor0_: vec4<f32>;
    var param_16: u32;
    var param_17: vec2<f32>;
    var param_18: i32;
    var frag_color: vec4<f32>;
    var color0_: vec4<f32>;
    var base: vec4<f32>;
    var param_19: vec3<f32>;
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var fogAmount: f32;
    var param_20: f32;
    var param_21: vec4<f32>;

    let _e110 = unnamed.packed_indices[0i][3u];
    let _e116 = unnamed.packed_indices[0i][3u];
    let _e121 = fog_tex_coord_1;
    let _e122 = textureSample(wired_bindless_images[(_e110 & 4095u)], wired_bindless_samplers[((_e116 >> bitcast<u32>(12i)) & 255u)], _e121);
    fog = _e122;
    param_16 = 0u;
    let _e123 = frag_tex_coord0_1;
    param_17 = _e123;
    param_18 = 0i;
    let _e124 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_16), (&param_17), (&param_18));
    sampledColor0_ = _e124;
    frag_color[0u] = identity_color;
    frag_color[1u] = identity_color;
    frag_color[2u] = identity_color;
    frag_color[3u] = identity_alpha;
    let _e129 = sampledColor0_;
    let _e130 = frag_color;
    color0_ = (_e129 * _e130);
    let _e132 = color0_;
    base = _e132;
    let _e133 = color0_;
    base = _e133;
    let _e134 = base;
    param_19 = _e134.xyz;
    let _e136 = wired_apply_sun_shadow_operand_u0028_vf3_u003b((&param_19));
    base[0u] = _e136.x;
    base[1u] = _e136.y;
    base[2u] = _e136.z;
    if override_type_3_3 {
        let _e145 = unnamed.worldLightParams[1u];
        wetness = clamp(_e145, 0f, 1f);
        let _e149 = unnamed.worldLightParams[2u];
        frost = clamp(_e149, 0f, 1f);
        let _e151 = base;
        luminance = dot(_e151.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e154 = wetness;
        let _e156 = base;
        let _e158 = (_e156.xyz * mix(1f, 0.82f, _e154));
        base[0u] = _e158.x;
        base[1u] = _e158.y;
        base[2u] = _e158.z;
        let _e165 = base;
        let _e167 = luminance;
        let _e169 = luminance;
        let _e171 = luminance;
        let _e173 = frost;
        let _e176 = mix(_e165.xyz, vec3<f32>((_e167 * 0.88f), (_e169 * 0.94f), _e171), vec3((_e173 * 0.55f)));
        base[0u] = _e176.x;
        base[1u] = _e176.y;
        base[2u] = _e176.z;
    }
    let _e185 = unnamed.materialFactors[0u];
    if (_e185 >= 0f) {
        let _e188 = unnamed.materialBaseColor;
        let _e189 = base;
        base = (_e189 * _e188);
    }
    let _e191 = color0_;
    let _e194 = unnamed.emissionRadiance;
    let _e197 = base;
    let _e199 = (_e197.xyz + (_e191.xyz * _e194.xyz));
    base[0u] = _e199.x;
    base[1u] = _e199.y;
    base[2u] = _e199.z;
    let _e206 = wired_advanced_fog_enabled_u0028_();
    if _e206 {
        let _e207 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e207;
        if override_type_3_4 {
            let _e208 = fogAmount;
            let _e210 = base;
            let _e212 = (_e210.xyz * (1f - _e208));
            base[0u] = _e212.x;
            base[1u] = _e212.y;
            base[2u] = _e212.z;
        } else {
            if override_type_3_5 {
                let _e219 = fogAmount;
                let _e221 = base;
                base = (_e221 * (1f - _e219));
            } else {
                if override_type_3_6 {
                    let _e223 = fogAmount;
                    let _e226 = base[3u];
                    base[3u] = (_e226 * (1f - _e223));
                } else {
                    let _e229 = base;
                    let _e232 = unnamed.advancedFogColorDensity;
                    let _e234 = fogAmount;
                    let _e236 = mix(_e229.xyz, _e232.xyz, vec3(_e234));
                    base[0u] = _e236.x;
                    base[1u] = _e236.y;
                    base[2u] = _e236.z;
                }
            }
        }
    } else {
        if override_type_3_7 {
            let _e243 = base;
            let _e246 = fog[3u];
            let _e248 = (_e243.xyz * (1f - _e246));
            base[0u] = _e248.x;
            base[1u] = _e248.y;
            base[2u] = _e248.z;
        } else {
            if override_type_3_8 {
                let _e255 = base;
                let _e257 = fog[3u];
                base = (_e255 * (1f - _e257));
            } else {
                if override_type_3_9 {
                    let _e261 = base[3u];
                    let _e263 = fog[3u];
                    base[3u] = (_e261 * (1f - _e263));
                } else {
                    let _e267 = base;
                    let _e268 = fog;
                    let _e270 = unnamed.fogColor;
                    let _e273 = fog[3u];
                    base = mix(_e267, (_e268 * _e270), vec4(_e273));
                }
            }
        }
    }
    if override_type_3_10 {
        let _e277 = base[3u];
        if (_e277 == 0f) {
            discard;
        }
    } else {
        if override_type_3_11 {
            let _e279 = base;
            let _e281 = base;
            if (dot(_e279.xyz, _e281.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e285 = base;
    out_color = _e285;
    param_20 = 1f;
    let _e286 = base;
    param_21 = _e286;
    wiredTemporalWriteAux_u0028_f1_u003b_vf4_u003b((&param_20), (&param_21));
    return;
}

@fragment
fn main(@location(12) @interpolate(flat) temporalOutcome: u32, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @builtin(position) gl_FragCoord: vec4<f32>, @location(7) shadowData: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(4) fog_tex_coord: vec2<f32>) -> FragmentOutput {
    temporalOutcome_1 = temporalOutcome;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    gl_FragCoord_1 = gl_FragCoord;
    shadowData_1 = shadowData;
    frag_tex_coord0_1 = frag_tex_coord0_;
    fog_tex_coord_1 = fog_tex_coord;
    main_1();
    let _e17 = out_temporal_velocity;
    let _e18 = out_temporal_validity;
    let _e19 = out_color;
    return FragmentOutput(_e17, _e18, _e19);
}
