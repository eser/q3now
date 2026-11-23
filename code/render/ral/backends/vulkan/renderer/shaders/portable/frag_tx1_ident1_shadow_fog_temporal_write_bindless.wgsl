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
override override_type_3_6: bool = (lightmap_slot != 0i);
@id(10) override acff: i32 = 0i;
override override_type_3_7: bool = (acff == 1i);
override override_type_3_8: bool = (acff == 2i);
override override_type_3_9: bool = (acff == 3i);
override override_type_3_10: bool = (acff == 1i);
override override_type_3_11: bool = (acff == 2i);
override override_type_3_12: bool = (acff == 3i);
@id(7) override discard_mode: i32 = 0i;
override override_type_3_13: bool = (discard_mode == 1i);
override override_type_3_14: bool = (discard_mode == 2i);
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
var<private> out_color: vec4<f32>;

fn wiredTemporalFinite2_u0028_vf2_u003b(value: ptr<function, vec2<f32>>) -> bool {
    var phi_97_: bool;

    let _e95 = (*value);
    let _e96 = (*value);
    let _e98 = all((_e95 == _e96));
    phi_97_ = _e98;
    if _e98 {
        let _e99 = (*value);
        phi_97_ = all((abs(_e99) <= vec2<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e104 = phi_97_;
    return _e104;
}

fn wiredTemporalFinite4_u0028_vf4_u003b(value_1: ptr<function, vec4<f32>>) -> bool {
    var phi_82_: bool;

    let _e95 = (*value_1);
    let _e96 = (*value_1);
    let _e98 = all((_e95 == _e96));
    phi_82_ = _e98;
    if _e98 {
        let _e99 = (*value_1);
        phi_82_ = all((abs(_e99) <= vec4<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e104 = phi_82_;
    return _e104;
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
    let _e105 = temporalOutcome_1;
    let _e106 = (_e105 != 1u);
    phi_120_ = _e106;
    if !(_e106) {
        let _e108 = temporalCurrentClip_1;
        param = _e108;
        let _e109 = wiredTemporalFinite4_u0028_vf4_u003b((&param));
        phi_120_ = !(_e109);
    }
    let _e112 = phi_120_;
    phi_129_ = _e112;
    if !(_e112) {
        let _e114 = temporalPreviousClip_1;
        param_1 = _e114;
        let _e115 = wiredTemporalFinite4_u0028_vf4_u003b((&param_1));
        phi_129_ = !(_e115);
    }
    let _e118 = phi_129_;
    phi_139_ = _e118;
    if !(_e118) {
        let _e121 = temporalCurrentClip_1[3u];
        phi_139_ = (_e121 <= 0.000001f);
    }
    let _e124 = phi_139_;
    phi_146_ = _e124;
    if !(_e124) {
        let _e127 = temporalPreviousClip_1[3u];
        phi_146_ = (_e127 <= 0.000001f);
    }
    let _e130 = phi_146_;
    if _e130 {
        return;
    }
    let _e131 = temporalCurrentClip_1;
    let _e134 = temporalCurrentClip_1[3u];
    currentNdc = (_e131.xy / vec2(_e134));
    let _e137 = temporalPreviousClip_1;
    let _e140 = temporalPreviousClip_1[3u];
    previousNdc = (_e137.xy / vec2(_e140));
    let _e143 = currentNdc;
    param_2 = _e143;
    let _e144 = wiredTemporalFinite2_u0028_vf2_u003b((&param_2));
    let _e145 = !(_e144);
    phi_175_ = _e145;
    if !(_e145) {
        let _e147 = previousNdc;
        param_3 = _e147;
        let _e148 = wiredTemporalFinite2_u0028_vf2_u003b((&param_3));
        phi_175_ = !(_e148);
    }
    let _e151 = phi_175_;
    if _e151 {
        return;
    }
    let _e152 = currentNdc;
    currentUv = ((_e152 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e155 = previousNdc;
    previousUv = ((_e155 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e158 = currentUv;
    let _e159 = previousUv;
    velocity = (_e158 - _e159);
    let _e161 = velocity;
    param_4 = _e161;
    let _e162 = wiredTemporalFinite2_u0028_vf2_u003b((&param_4));
    if !(_e162) {
        return;
    }
    let _e164 = velocity;
    out_temporal_velocity = _e164;
    let _e165 = (*coverageConfidence);
    out_temporal_validity = clamp(_e165, 0f, 1f);
    return;
}

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e97 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e97 + 0.5f));
    let _e102 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e104 = fogType;
    let _e107 = fogType;
    return (((_e102 > 0.5f) && (_e104 >= 1i)) && (_e107 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e97 = wired_advanced_fog_enabled_u0028_();
    if !(_e97) {
        return 0f;
    }
    let _e100 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e100, 0.000001f));
    let _e105 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e105 + 0.5f));
    let _e108 = fogType_1;
    if (_e108 == 1i) {
        let _e112 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e112 <= 0f) {
            return 0f;
        }
        let _e114 = viewDepth;
        let _e117 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e114 / _e117), 0f, 1f);
    }
    let _e122 = unnamed.advancedFogColorDensity[3u];
    let _e124 = viewDepth;
    opticalDepth = (max(_e122, 0f) * _e124);
    let _e126 = fogType_1;
    if (_e126 == 2i) {
        let _e128 = opticalDepth;
        return clamp((1f - exp(-(_e128))), 0f, 1f);
    }
    let _e133 = opticalDepth;
    let _e134 = opticalDepth;
    return clamp((1f - exp(-((_e133 * _e134)))), 0f, 1f);
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

    let _e104 = (*c);
    let _e107 = unnamed.cascadeMVP[_e104];
    let _e108 = (*worldPos);
    sc4_ = (_e107 * vec4<f32>(_e108.x, _e108.y, _e108.z, 1f));
    let _e114 = sc4_;
    let _e117 = sc4_[3u];
    sc = (_e114.xyz / vec3(_e117));
    let _e120 = sc;
    let _e124 = ((_e120.xy * 0.5f) + vec2(0.5f));
    sc[0u] = _e124.x;
    sc[1u] = _e124.y;
    let _e130 = sc[0u];
    let _e131 = (_e130 < 0f);
    phi_470_ = _e131;
    if !(_e131) {
        let _e134 = sc[0u];
        phi_470_ = (_e134 > 1f);
    }
    let _e137 = phi_470_;
    phi_477_ = _e137;
    if !(_e137) {
        let _e140 = sc[1u];
        phi_477_ = (_e140 < 0f);
    }
    let _e143 = phi_477_;
    phi_484_ = _e143;
    if !(_e143) {
        let _e146 = sc[1u];
        phi_484_ = (_e146 > 1f);
    }
    let _e149 = phi_484_;
    phi_491_ = _e149;
    if !(_e149) {
        let _e152 = sc[2u];
        phi_491_ = (_e152 > 1f);
    }
    let _e155 = phi_491_;
    if _e155 {
        return 1f;
    }
    let _e156 = (*c);
    layer = f32(_e156);
    let _e158 = textureDimensions(shadowMap, 0i);
    texelSize = (vec2(1f) / vec2<f32>(vec2<i32>(_e158).xy));
    let _e165 = sc[2u];
    currentDepth = (_e165 - shadow_bias);
    shadow = 0f;
    if override_type_3_ {
        let _e167 = currentDepth;
        let _e168 = sc;
        let _e169 = _e168.xy;
        let _e170 = layer;
        let _e173 = vec3<f32>(_e169.x, _e169.y, _e170);
        let _e179 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e173.x, _e173.y), i32(_e173.z));
        shadow = step(_e167, _e179.x);
    } else {
        if override_type_3_1 {
            let _e182 = currentDepth;
            let _e183 = sc;
            let _e184 = _e183.xy;
            let _e185 = layer;
            let _e188 = vec3<f32>(_e184.x, _e184.y, _e185);
            let _e194 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e188.x, _e188.y), i32(_e188.z));
            let _e197 = shadow;
            shadow = (_e197 + step(_e182, _e194.x));
            let _e199 = currentDepth;
            let _e200 = sc;
            let _e203 = texelSize[0u];
            let _e205 = (_e200.xy + vec2<f32>(_e203, 0f));
            let _e206 = layer;
            let _e209 = vec3<f32>(_e205.x, _e205.y, _e206);
            let _e215 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e209.x, _e209.y), i32(_e209.z));
            let _e218 = shadow;
            shadow = (_e218 + step(_e199, _e215.x));
            let _e220 = currentDepth;
            let _e221 = sc;
            let _e224 = texelSize[0u];
            let _e226 = (_e221.xy - vec2<f32>(_e224, 0f));
            let _e227 = layer;
            let _e230 = vec3<f32>(_e226.x, _e226.y, _e227);
            let _e236 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e230.x, _e230.y), i32(_e230.z));
            let _e239 = shadow;
            shadow = (_e239 + step(_e220, _e236.x));
            let _e241 = currentDepth;
            let _e242 = sc;
            let _e245 = texelSize[1u];
            let _e247 = (_e242.xy + vec2<f32>(0f, _e245));
            let _e248 = layer;
            let _e251 = vec3<f32>(_e247.x, _e247.y, _e248);
            let _e257 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e251.x, _e251.y), i32(_e251.z));
            let _e260 = shadow;
            shadow = (_e260 + step(_e241, _e257.x));
            let _e262 = currentDepth;
            let _e263 = sc;
            let _e266 = texelSize[1u];
            let _e268 = (_e263.xy - vec2<f32>(0f, _e266));
            let _e269 = layer;
            let _e272 = vec3<f32>(_e268.x, _e268.y, _e269);
            let _e278 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e272.x, _e272.y), i32(_e272.z));
            let _e281 = shadow;
            shadow = (_e281 + step(_e262, _e278.x));
            let _e283 = shadow;
            shadow = (_e283 / 5f);
        } else {
            x = -1i;
            loop {
                let _e285 = x;
                if (_e285 <= 1i) {
                    y = -1i;
                    loop {
                        let _e287 = y;
                        if (_e287 <= 1i) {
                            let _e289 = currentDepth;
                            let _e290 = sc;
                            let _e292 = x;
                            let _e294 = y;
                            let _e297 = texelSize;
                            let _e299 = (_e290.xy + (vec2<f32>(f32(_e292), f32(_e294)) * _e297));
                            let _e300 = layer;
                            let _e303 = vec3<f32>(_e299.x, _e299.y, _e300);
                            let _e309 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e303.x, _e303.y), i32(_e303.z));
                            let _e312 = shadow;
                            shadow = (_e312 + step(_e289, _e309.x));
                            continue;
                        } else {
                            break;
                        }
                        continuing {
                            let _e314 = y;
                            y = (_e314 + 1i);
                        }
                    }
                    continue;
                } else {
                    break;
                }
                continuing {
                    let _e316 = x;
                    x = (_e316 + 1i);
                }
            }
            let _e318 = shadow;
            shadow = (_e318 / 9f);
        }
    }
    let _e320 = shadow;
    return _e320;
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

    let _e111 = unnamed.cascadeSplits;
    let _e112 = (*viewDepth_1);
    cmp = step(_e111, vec4(_e112));
    let _e116 = cmp[0u];
    let _e118 = cmp[1u];
    let _e121 = cmp[2u];
    let _e124 = cmp[3u];
    cascade = min(i32((((_e116 + _e118) + _e121) + _e124)), 3i);
    let _e128 = cascade;
    (*outCascade) = _e128;
    let _e129 = cascade;
    if (_e129 == 0i) {
        local = 0f;
    } else {
        let _e131 = cascade;
        let _e136 = unnamed.cascadeSplits[max((_e131 - 1i), 0i)];
        local = _e136;
    }
    let _e137 = local;
    prevSplit = _e137;
    let _e138 = cascade;
    let _e141 = unnamed.cascadeSplits[_e138];
    farSplit = _e141;
    let _e142 = farSplit;
    let _e143 = prevSplit;
    blendRange = max((0.1f * (_e142 - _e143)), 1f);
    let _e147 = farSplit;
    let _e148 = (*viewDepth_1);
    let _e150 = blendRange;
    blendT = clamp(((_e147 - _e148) / _e150), 0f, 1f);
    let _e153 = cascade;
    param_5 = _e153;
    let _e154 = (*worldPos_1);
    param_6 = _e154;
    let _e155 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param_5), (&param_6));
    s0_ = _e155;
    let _e156 = cascade;
    param_7 = min((_e156 + 1i), 3i);
    let _e159 = (*worldPos_1);
    param_8 = _e159;
    let _e160 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param_7), (&param_8));
    s1_ = _e160;
    let _e161 = s1_;
    let _e162 = s0_;
    let _e163 = blendT;
    return mix(_e161, _e162, _e163);
}

fn wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b(rgb: ptr<function, vec3<f32>>, lm_uv: ptr<function, vec2<f32>>) -> vec3<f32> {
    var sun_mask: f32;
    var shadow_1: f32;
    var ignoredCascade: i32;
    var param_9: vec3<f32>;
    var param_10: f32;
    var param_11: i32;

    let _e105 = unnamed.packed_indices[1i][3u];
    let _e111 = unnamed.packed_indices[1i][3u];
    let _e116 = (*lm_uv);
    let _e117 = textureSample(wired_bindless_images[(_e105 & 4095u)], wired_bindless_samplers[((_e111 >> bitcast<u32>(12i)) & 255u)], _e116);
    sun_mask = _e117.x;
    let _e119 = sun_mask;
    if (_e119 > 0.001f) {
        let _e121 = shadowData_1;
        param_9 = _e121.xyz;
        let _e124 = shadowData_1[3u];
        param_10 = _e124;
        let _e125 = sampleShadow_u0028_vf3_u003b_f1_u003b_i1_u003b((&param_9), (&param_10), (&param_11));
        let _e126 = param_11;
        ignoredCascade = _e126;
        shadow_1 = _e125;
        let _e127 = shadow_1;
        let _e128 = sun_mask;
        let _e130 = (*rgb);
        (*rgb) = (_e130 * mix(1f, _e127, _e128));
    }
    let _e132 = (*rgb);
    return _e132;
}

fn wired_apply_sun_shadow_operand_u0028_vf3_u003b(rgb_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var param_12: vec3<f32>;
    var param_13: vec2<f32>;
    var param_14: vec3<f32>;
    var param_15: vec2<f32>;

    if override_type_3_2 {
        let _e99 = (*rgb_1);
        param_12 = _e99;
        let _e100 = frag_tex_coord0_1;
        param_13 = _e100;
        let _e101 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_12), (&param_13));
        return _e101;
    }
    if override_type_3_3 {
        let _e102 = (*rgb_1);
        param_14 = _e102;
        let _e103 = frag_tex_coord1_1;
        param_15 = _e103;
        let _e104 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_14), (&param_15));
        return _e104;
    }
    let _e105 = (*rgb_1);
    return _e105;
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb_2: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e97 = (*rgb_2);
    let _e100 = unnamed.worldLightParams[0u];
    boosted = (_e97 * _e100);
    let _e103 = boosted[0u];
    let _e105 = boosted[1u];
    let _e107 = boosted[2u];
    peak = max(_e103, max(_e105, _e107));
    let _e110 = peak;
    if (_e110 > 1f) {
        let _e112 = peak;
        let _e113 = boosted;
        boosted = (_e113 / vec3(_e112));
    }
    let _e116 = boosted;
    return _e116;
}

fn sRGBToLinear_u0028_vf3_u003b(c_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e98 = (*c_1);
    (*c_1) = max(_e98, vec3<f32>(0f, 0f, 0f));
    let _e100 = (*c_1);
    cutoff = (_e100 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e102 = (*c_1);
    lo = (_e102 / vec3(12.92f));
    let _e105 = (*c_1);
    hi = pow(((_e105 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e110 = hi;
    let _e111 = lo;
    let _e112 = cutoff;
    return mix(_e110, _e111, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e112));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_2: vec4<f32>;
    var param_16: vec3<f32>;
    var param_17: vec3<f32>;

    let _e100 = (*role);
    let _e102 = (*role);
    let _e107 = unnamed.packed_indices[(_e100 / 4u)][(_e102 % 4u)];
    let _e110 = (*role);
    let _e112 = (*role);
    let _e117 = unnamed.packed_indices[(_e110 / 4u)][(_e112 % 4u)];
    let _e122 = (*uv);
    let _e123 = textureSample(wired_bindless_images[(_e107 & 4095u)], wired_bindless_samplers[((_e117 >> bitcast<u32>(12i)) & 255u)], _e122);
    c_2 = _e123;
    let _e124 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e124))) == 0i) {
        let _e129 = c_2;
        param_16 = _e129.xyz;
        let _e131 = sRGBToLinear_u0028_vf3_u003b((&param_16));
        c_2[0u] = _e131.x;
        c_2[1u] = _e131.y;
        c_2[2u] = _e131.z;
    }
    let _e138 = (*slot);
    if (lightmap_slot == (_e138 + 1i)) {
        let _e141 = c_2;
        param_17 = _e141.xyz;
        let _e143 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_17));
        c_2[0u] = _e143.x;
        c_2[1u] = _e143.y;
        c_2[2u] = _e143.z;
    }
    let _e150 = c_2;
    return _e150;
}

fn main_1() {
    var fog: vec4<f32>;
    var sampledColor0_: vec4<f32>;
    var param_18: u32;
    var param_19: vec2<f32>;
    var param_20: i32;
    var sampledColor1_: vec4<f32>;
    var param_21: u32;
    var param_22: vec2<f32>;
    var param_23: i32;
    var color0_: vec4<f32>;
    var color1_: vec4<f32>;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var color1_2: vec4<f32>;
    var param_24: vec3<f32>;
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var fogAmount: f32;
    var param_25: f32;

    let _e117 = unnamed.packed_indices[0i][3u];
    let _e123 = unnamed.packed_indices[0i][3u];
    let _e128 = fog_tex_coord_1;
    let _e129 = textureSample(wired_bindless_images[(_e117 & 4095u)], wired_bindless_samplers[((_e123 >> bitcast<u32>(12i)) & 255u)], _e128);
    fog = _e129;
    param_18 = 0u;
    let _e130 = frag_tex_coord0_1;
    param_19 = _e130;
    param_20 = 0i;
    let _e131 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_18), (&param_19), (&param_20));
    sampledColor0_ = _e131;
    param_21 = 1u;
    let _e132 = frag_tex_coord1_1;
    param_22 = _e132;
    param_23 = 1i;
    let _e133 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_21), (&param_22), (&param_23));
    sampledColor1_ = _e133;
    let _e134 = sampledColor0_;
    color0_ = _e134;
    if override_type_3_4 {
        let _e135 = sampledColor1_;
        color1_ = _e135;
        let _e136 = color0_;
        let _e138 = color1_;
        let _e140 = (_e136.xyz + _e138.xyz);
        let _e142 = color0_[3u];
        let _e144 = color1_[3u];
        base = vec4<f32>(_e140.x, _e140.y, _e140.z, (_e142 * _e144));
    } else {
        if override_type_3_5 {
            let _e150 = sampledColor1_;
            color1_1 = _e150;
            let _e151 = color0_;
            let _e153 = color1_1;
            let _e155 = (_e151.xyz + _e153.xyz);
            let _e157 = color0_[3u];
            let _e159 = color1_1[3u];
            base = vec4<f32>(_e155.x, _e155.y, _e155.z, (_e157 * _e159));
        } else {
            let _e165 = sampledColor1_;
            color1_2 = _e165;
            let _e166 = color0_;
            let _e168 = color1_2;
            let _e170 = (_e166.xyz * _e168.xyz);
            base[0u] = _e170.x;
            base[1u] = _e170.y;
            base[2u] = _e170.z;
            let _e178 = color0_[3u];
            let _e180 = color1_2[3u];
            base[3u] = (_e178 * _e180);
        }
    }
    let _e183 = base;
    param_24 = _e183.xyz;
    let _e185 = wired_apply_sun_shadow_operand_u0028_vf3_u003b((&param_24));
    base[0u] = _e185.x;
    base[1u] = _e185.y;
    base[2u] = _e185.z;
    if override_type_3_6 {
        let _e194 = unnamed.worldLightParams[1u];
        wetness = clamp(_e194, 0f, 1f);
        let _e198 = unnamed.worldLightParams[2u];
        frost = clamp(_e198, 0f, 1f);
        let _e200 = base;
        luminance = dot(_e200.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e203 = wetness;
        let _e205 = base;
        let _e207 = (_e205.xyz * mix(1f, 0.82f, _e203));
        base[0u] = _e207.x;
        base[1u] = _e207.y;
        base[2u] = _e207.z;
        let _e214 = base;
        let _e216 = luminance;
        let _e218 = luminance;
        let _e220 = luminance;
        let _e222 = frost;
        let _e225 = mix(_e214.xyz, vec3<f32>((_e216 * 0.88f), (_e218 * 0.94f), _e220), vec3((_e222 * 0.55f)));
        base[0u] = _e225.x;
        base[1u] = _e225.y;
        base[2u] = _e225.z;
    }
    let _e234 = unnamed.materialFactors[0u];
    if (_e234 >= 0f) {
        let _e237 = unnamed.materialBaseColor;
        let _e238 = base;
        base = (_e238 * _e237);
    }
    let _e240 = color0_;
    let _e243 = unnamed.emissionRadiance;
    let _e246 = base;
    let _e248 = (_e246.xyz + (_e240.xyz * _e243.xyz));
    base[0u] = _e248.x;
    base[1u] = _e248.y;
    base[2u] = _e248.z;
    let _e255 = wired_advanced_fog_enabled_u0028_();
    if _e255 {
        let _e256 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e256;
        if override_type_3_7 {
            let _e257 = fogAmount;
            let _e259 = base;
            let _e261 = (_e259.xyz * (1f - _e257));
            base[0u] = _e261.x;
            base[1u] = _e261.y;
            base[2u] = _e261.z;
        } else {
            if override_type_3_8 {
                let _e268 = fogAmount;
                let _e270 = base;
                base = (_e270 * (1f - _e268));
            } else {
                if override_type_3_9 {
                    let _e272 = fogAmount;
                    let _e275 = base[3u];
                    base[3u] = (_e275 * (1f - _e272));
                } else {
                    let _e278 = base;
                    let _e281 = unnamed.advancedFogColorDensity;
                    let _e283 = fogAmount;
                    let _e285 = mix(_e278.xyz, _e281.xyz, vec3(_e283));
                    base[0u] = _e285.x;
                    base[1u] = _e285.y;
                    base[2u] = _e285.z;
                }
            }
        }
    } else {
        if override_type_3_10 {
            let _e292 = base;
            let _e295 = fog[3u];
            let _e297 = (_e292.xyz * (1f - _e295));
            base[0u] = _e297.x;
            base[1u] = _e297.y;
            base[2u] = _e297.z;
        } else {
            if override_type_3_11 {
                let _e304 = base;
                let _e306 = fog[3u];
                base = (_e304 * (1f - _e306));
            } else {
                if override_type_3_12 {
                    let _e310 = base[3u];
                    let _e312 = fog[3u];
                    base[3u] = (_e310 * (1f - _e312));
                } else {
                    let _e316 = base;
                    let _e317 = fog;
                    let _e319 = unnamed.fogColor;
                    let _e322 = fog[3u];
                    base = mix(_e316, (_e317 * _e319), vec4(_e322));
                }
            }
        }
    }
    if override_type_3_13 {
        let _e326 = base[3u];
        if (_e326 == 0f) {
            discard;
        }
    } else {
        if override_type_3_14 {
            let _e328 = base;
            let _e330 = base;
            if (dot(_e328.xyz, _e330.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e334 = base;
    out_color = _e334;
    param_25 = 1f;
    wiredTemporalWriteAux_u0028_f1_u003b((&param_25));
    return;
}

@fragment
fn main(@location(12) @interpolate(flat) temporalOutcome: u32, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @builtin(position) gl_FragCoord: vec4<f32>, @location(7) shadowData: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(4) fog_tex_coord: vec2<f32>) -> FragmentOutput {
    temporalOutcome_1 = temporalOutcome;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    gl_FragCoord_1 = gl_FragCoord;
    shadowData_1 = shadowData;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    fog_tex_coord_1 = fog_tex_coord;
    main_1();
    let _e19 = out_temporal_velocity;
    let _e20 = out_temporal_validity;
    let _e21 = out_color;
    return FragmentOutput(_e19, _e20, _e21);
}
