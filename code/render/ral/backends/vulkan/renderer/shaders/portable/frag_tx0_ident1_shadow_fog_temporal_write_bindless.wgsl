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
var<private> fog_tex_coord_1: vec2<f32>;
var<private> out_color: vec4<f32>;

fn wiredTemporalFinite2_u0028_vf2_u003b(value: ptr<function, vec2<f32>>) -> bool {
    var phi_97_: bool;

    let _e90 = (*value);
    let _e91 = (*value);
    let _e93 = all((_e90 == _e91));
    phi_97_ = _e93;
    if _e93 {
        let _e94 = (*value);
        phi_97_ = all((abs(_e94) <= vec2<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e99 = phi_97_;
    return _e99;
}

fn wiredTemporalFinite4_u0028_vf4_u003b(value_1: ptr<function, vec4<f32>>) -> bool {
    var phi_82_: bool;

    let _e90 = (*value_1);
    let _e91 = (*value_1);
    let _e93 = all((_e90 == _e91));
    phi_82_ = _e93;
    if _e93 {
        let _e94 = (*value_1);
        phi_82_ = all((abs(_e94) <= vec4<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e99 = phi_82_;
    return _e99;
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
    let _e100 = temporalOutcome_1;
    let _e101 = (_e100 != 1u);
    phi_120_ = _e101;
    if !(_e101) {
        let _e103 = temporalCurrentClip_1;
        param = _e103;
        let _e104 = wiredTemporalFinite4_u0028_vf4_u003b((&param));
        phi_120_ = !(_e104);
    }
    let _e107 = phi_120_;
    phi_129_ = _e107;
    if !(_e107) {
        let _e109 = temporalPreviousClip_1;
        param_1 = _e109;
        let _e110 = wiredTemporalFinite4_u0028_vf4_u003b((&param_1));
        phi_129_ = !(_e110);
    }
    let _e113 = phi_129_;
    phi_139_ = _e113;
    if !(_e113) {
        let _e116 = temporalCurrentClip_1[3u];
        phi_139_ = (_e116 <= 0.000001f);
    }
    let _e119 = phi_139_;
    phi_146_ = _e119;
    if !(_e119) {
        let _e122 = temporalPreviousClip_1[3u];
        phi_146_ = (_e122 <= 0.000001f);
    }
    let _e125 = phi_146_;
    if _e125 {
        return;
    }
    let _e126 = temporalCurrentClip_1;
    let _e129 = temporalCurrentClip_1[3u];
    currentNdc = (_e126.xy / vec2(_e129));
    let _e132 = temporalPreviousClip_1;
    let _e135 = temporalPreviousClip_1[3u];
    previousNdc = (_e132.xy / vec2(_e135));
    let _e138 = currentNdc;
    param_2 = _e138;
    let _e139 = wiredTemporalFinite2_u0028_vf2_u003b((&param_2));
    let _e140 = !(_e139);
    phi_175_ = _e140;
    if !(_e140) {
        let _e142 = previousNdc;
        param_3 = _e142;
        let _e143 = wiredTemporalFinite2_u0028_vf2_u003b((&param_3));
        phi_175_ = !(_e143);
    }
    let _e146 = phi_175_;
    if _e146 {
        return;
    }
    let _e147 = currentNdc;
    currentUv = ((_e147 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e150 = previousNdc;
    previousUv = ((_e150 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e153 = currentUv;
    let _e154 = previousUv;
    velocity = (_e153 - _e154);
    let _e156 = velocity;
    param_4 = _e156;
    let _e157 = wiredTemporalFinite2_u0028_vf2_u003b((&param_4));
    if !(_e157) {
        return;
    }
    let _e159 = velocity;
    out_temporal_velocity = _e159;
    let _e160 = (*coverageConfidence);
    out_temporal_validity = clamp(_e160, 0f, 1f);
    return;
}

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
    var phi_470_: bool;
    var phi_477_: bool;
    var phi_484_: bool;
    var phi_491_: bool;

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
    phi_470_ = _e126;
    if !(_e126) {
        let _e129 = sc[0u];
        phi_470_ = (_e129 > 1f);
    }
    let _e132 = phi_470_;
    phi_477_ = _e132;
    if !(_e132) {
        let _e135 = sc[1u];
        phi_477_ = (_e135 < 0f);
    }
    let _e138 = phi_477_;
    phi_484_ = _e138;
    if !(_e138) {
        let _e141 = sc[1u];
        phi_484_ = (_e141 > 1f);
    }
    let _e144 = phi_484_;
    phi_491_ = _e144;
    if !(_e144) {
        let _e147 = sc[2u];
        phi_491_ = (_e147 > 1f);
    }
    let _e150 = phi_491_;
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
    var param_5: i32;
    var param_6: vec3<f32>;
    var s1_: f32;
    var param_7: i32;
    var param_8: vec3<f32>;

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
    param_5 = _e148;
    let _e149 = (*worldPos_1);
    param_6 = _e149;
    let _e150 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param_5), (&param_6));
    s0_ = _e150;
    let _e151 = cascade;
    param_7 = min((_e151 + 1i), 3i);
    let _e154 = (*worldPos_1);
    param_8 = _e154;
    let _e155 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param_7), (&param_8));
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
    var param_9: vec3<f32>;
    var param_10: f32;
    var param_11: i32;

    let _e100 = unnamed.packed_indices[1i][3u];
    let _e106 = unnamed.packed_indices[1i][3u];
    let _e111 = (*lm_uv);
    let _e112 = textureSample(wired_bindless_images[(_e100 & 4095u)], wired_bindless_samplers[((_e106 >> bitcast<u32>(12i)) & 255u)], _e111);
    sun_mask = _e112.x;
    let _e114 = sun_mask;
    if (_e114 > 0.001f) {
        let _e116 = shadowData_1;
        param_9 = _e116.xyz;
        let _e119 = shadowData_1[3u];
        param_10 = _e119;
        let _e120 = sampleShadow_u0028_vf3_u003b_f1_u003b_i1_u003b((&param_9), (&param_10), (&param_11));
        let _e121 = param_11;
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
    var param_12: vec3<f32>;
    var param_13: vec2<f32>;

    if override_type_3_2 {
        let _e92 = (*rgb_1);
        param_12 = _e92;
        let _e93 = frag_tex_coord0_1;
        param_13 = _e93;
        let _e94 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_12), (&param_13));
        return _e94;
    }
    let _e95 = (*rgb_1);
    return _e95;
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
    var param_14: vec3<f32>;
    var param_15: vec3<f32>;

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
        param_14 = _e124.xyz;
        let _e126 = sRGBToLinear_u0028_vf3_u003b((&param_14));
        c_2[0u] = _e126.x;
        c_2[1u] = _e126.y;
        c_2[2u] = _e126.z;
    }
    let _e133 = (*slot);
    if (lightmap_slot == (_e133 + 1i)) {
        let _e136 = c_2;
        param_15 = _e136.xyz;
        let _e138 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_15));
        c_2[0u] = _e138.x;
        c_2[1u] = _e138.y;
        c_2[2u] = _e138.z;
    }
    let _e145 = c_2;
    return _e145;
}

fn main_1() {
    var fog: vec4<f32>;
    var sampledColor0_: vec4<f32>;
    var param_16: u32;
    var param_17: vec2<f32>;
    var param_18: i32;
    var color0_: vec4<f32>;
    var base: vec4<f32>;
    var param_19: vec3<f32>;
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var fogAmount: f32;
    var param_20: f32;

    let _e105 = unnamed.packed_indices[0i][3u];
    let _e111 = unnamed.packed_indices[0i][3u];
    let _e116 = fog_tex_coord_1;
    let _e117 = textureSample(wired_bindless_images[(_e105 & 4095u)], wired_bindless_samplers[((_e111 >> bitcast<u32>(12i)) & 255u)], _e116);
    fog = _e117;
    param_16 = 0u;
    let _e118 = frag_tex_coord0_1;
    param_17 = _e118;
    param_18 = 0i;
    let _e119 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_16), (&param_17), (&param_18));
    sampledColor0_ = _e119;
    let _e120 = sampledColor0_;
    color0_ = _e120;
    let _e121 = color0_;
    base = _e121;
    let _e122 = color0_;
    base = _e122;
    let _e123 = base;
    param_19 = _e123.xyz;
    let _e125 = wired_apply_sun_shadow_operand_u0028_vf3_u003b((&param_19));
    base[0u] = _e125.x;
    base[1u] = _e125.y;
    base[2u] = _e125.z;
    if override_type_3_3 {
        let _e134 = unnamed.worldLightParams[1u];
        wetness = clamp(_e134, 0f, 1f);
        let _e138 = unnamed.worldLightParams[2u];
        frost = clamp(_e138, 0f, 1f);
        let _e140 = base;
        luminance = dot(_e140.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e143 = wetness;
        let _e145 = base;
        let _e147 = (_e145.xyz * mix(1f, 0.82f, _e143));
        base[0u] = _e147.x;
        base[1u] = _e147.y;
        base[2u] = _e147.z;
        let _e154 = base;
        let _e156 = luminance;
        let _e158 = luminance;
        let _e160 = luminance;
        let _e162 = frost;
        let _e165 = mix(_e154.xyz, vec3<f32>((_e156 * 0.88f), (_e158 * 0.94f), _e160), vec3((_e162 * 0.55f)));
        base[0u] = _e165.x;
        base[1u] = _e165.y;
        base[2u] = _e165.z;
    }
    let _e174 = unnamed.materialFactors[0u];
    if (_e174 >= 0f) {
        let _e177 = unnamed.materialBaseColor;
        let _e178 = base;
        base = (_e178 * _e177);
    }
    let _e180 = color0_;
    let _e183 = unnamed.emissionRadiance;
    let _e186 = base;
    let _e188 = (_e186.xyz + (_e180.xyz * _e183.xyz));
    base[0u] = _e188.x;
    base[1u] = _e188.y;
    base[2u] = _e188.z;
    let _e195 = wired_advanced_fog_enabled_u0028_();
    if _e195 {
        let _e196 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e196;
        if override_type_3_4 {
            let _e197 = fogAmount;
            let _e199 = base;
            let _e201 = (_e199.xyz * (1f - _e197));
            base[0u] = _e201.x;
            base[1u] = _e201.y;
            base[2u] = _e201.z;
        } else {
            if override_type_3_5 {
                let _e208 = fogAmount;
                let _e210 = base;
                base = (_e210 * (1f - _e208));
            } else {
                if override_type_3_6 {
                    let _e212 = fogAmount;
                    let _e215 = base[3u];
                    base[3u] = (_e215 * (1f - _e212));
                } else {
                    let _e218 = base;
                    let _e221 = unnamed.advancedFogColorDensity;
                    let _e223 = fogAmount;
                    let _e225 = mix(_e218.xyz, _e221.xyz, vec3(_e223));
                    base[0u] = _e225.x;
                    base[1u] = _e225.y;
                    base[2u] = _e225.z;
                }
            }
        }
    } else {
        if override_type_3_7 {
            let _e232 = base;
            let _e235 = fog[3u];
            let _e237 = (_e232.xyz * (1f - _e235));
            base[0u] = _e237.x;
            base[1u] = _e237.y;
            base[2u] = _e237.z;
        } else {
            if override_type_3_8 {
                let _e244 = base;
                let _e246 = fog[3u];
                base = (_e244 * (1f - _e246));
            } else {
                if override_type_3_9 {
                    let _e250 = base[3u];
                    let _e252 = fog[3u];
                    base[3u] = (_e250 * (1f - _e252));
                } else {
                    let _e256 = base;
                    let _e257 = fog;
                    let _e259 = unnamed.fogColor;
                    let _e262 = fog[3u];
                    base = mix(_e256, (_e257 * _e259), vec4(_e262));
                }
            }
        }
    }
    if override_type_3_10 {
        let _e266 = base[3u];
        if (_e266 == 0f) {
            discard;
        }
    } else {
        if override_type_3_11 {
            let _e268 = base;
            let _e270 = base;
            if (dot(_e268.xyz, _e270.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e274 = base;
    out_color = _e274;
    param_20 = 1f;
    wiredTemporalWriteAux_u0028_f1_u003b((&param_20));
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
