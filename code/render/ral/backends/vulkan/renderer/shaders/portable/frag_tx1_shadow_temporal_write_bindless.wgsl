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
@id(7) override discard_mode: i32 = 0i;
override override_type_3_7: bool = (discard_mode == 1i);
override override_type_3_8: bool = (discard_mode == 2i);
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
var<private> frag_color0In_1: vec4<f32>;
var<private> out_color: vec4<f32>;

fn wiredTemporalFinite2_u0028_vf2_u003b(value: ptr<function, vec2<f32>>) -> bool {
    var phi_97_: bool;

    let _e87 = (*value);
    let _e88 = (*value);
    let _e90 = all((_e87 == _e88));
    phi_97_ = _e90;
    if _e90 {
        let _e91 = (*value);
        phi_97_ = all((abs(_e91) <= vec2<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e96 = phi_97_;
    return _e96;
}

fn wiredTemporalFinite4_u0028_vf4_u003b(value_1: ptr<function, vec4<f32>>) -> bool {
    var phi_82_: bool;

    let _e87 = (*value_1);
    let _e88 = (*value_1);
    let _e90 = all((_e87 == _e88));
    phi_82_ = _e90;
    if _e90 {
        let _e91 = (*value_1);
        phi_82_ = all((abs(_e91) <= vec4<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e96 = phi_82_;
    return _e96;
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
    let _e97 = temporalOutcome_1;
    let _e98 = (_e97 != 1u);
    phi_120_ = _e98;
    if !(_e98) {
        let _e100 = temporalCurrentClip_1;
        param = _e100;
        let _e101 = wiredTemporalFinite4_u0028_vf4_u003b((&param));
        phi_120_ = !(_e101);
    }
    let _e104 = phi_120_;
    phi_129_ = _e104;
    if !(_e104) {
        let _e106 = temporalPreviousClip_1;
        param_1 = _e106;
        let _e107 = wiredTemporalFinite4_u0028_vf4_u003b((&param_1));
        phi_129_ = !(_e107);
    }
    let _e110 = phi_129_;
    phi_139_ = _e110;
    if !(_e110) {
        let _e113 = temporalCurrentClip_1[3u];
        phi_139_ = (_e113 <= 0.000001f);
    }
    let _e116 = phi_139_;
    phi_146_ = _e116;
    if !(_e116) {
        let _e119 = temporalPreviousClip_1[3u];
        phi_146_ = (_e119 <= 0.000001f);
    }
    let _e122 = phi_146_;
    if _e122 {
        return;
    }
    let _e123 = temporalCurrentClip_1;
    let _e126 = temporalCurrentClip_1[3u];
    currentNdc = (_e123.xy / vec2(_e126));
    let _e129 = temporalPreviousClip_1;
    let _e132 = temporalPreviousClip_1[3u];
    previousNdc = (_e129.xy / vec2(_e132));
    let _e135 = currentNdc;
    param_2 = _e135;
    let _e136 = wiredTemporalFinite2_u0028_vf2_u003b((&param_2));
    let _e137 = !(_e136);
    phi_175_ = _e137;
    if !(_e137) {
        let _e139 = previousNdc;
        param_3 = _e139;
        let _e140 = wiredTemporalFinite2_u0028_vf2_u003b((&param_3));
        phi_175_ = !(_e140);
    }
    let _e143 = phi_175_;
    if _e143 {
        return;
    }
    let _e144 = currentNdc;
    currentUv = ((_e144 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e147 = previousNdc;
    previousUv = ((_e147 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e150 = currentUv;
    let _e151 = previousUv;
    velocity = (_e150 - _e151);
    let _e153 = velocity;
    param_4 = _e153;
    let _e154 = wiredTemporalFinite2_u0028_vf2_u003b((&param_4));
    if !(_e154) {
        return;
    }
    let _e156 = velocity;
    out_temporal_velocity = _e156;
    let _e157 = (*coverageConfidence);
    out_temporal_validity = clamp(_e157, 0f, 1f);
    return;
}

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e89 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e89 + 0.5f));
    let _e94 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e96 = fogType;
    let _e99 = fogType;
    return (((_e94 > 0.5f) && (_e96 >= 1i)) && (_e99 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e89 = wired_advanced_fog_enabled_u0028_();
    if !(_e89) {
        return 0f;
    }
    let _e92 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e92, 0.000001f));
    let _e97 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e97 + 0.5f));
    let _e100 = fogType_1;
    if (_e100 == 1i) {
        let _e104 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e104 <= 0f) {
            return 0f;
        }
        let _e106 = viewDepth;
        let _e109 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e106 / _e109), 0f, 1f);
    }
    let _e114 = unnamed.advancedFogColorDensity[3u];
    let _e116 = viewDepth;
    opticalDepth = (max(_e114, 0f) * _e116);
    let _e118 = fogType_1;
    if (_e118 == 2i) {
        let _e120 = opticalDepth;
        return clamp((1f - exp(-(_e120))), 0f, 1f);
    }
    let _e125 = opticalDepth;
    let _e126 = opticalDepth;
    return clamp((1f - exp(-((_e125 * _e126)))), 0f, 1f);
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

    let _e96 = (*c);
    let _e99 = unnamed.cascadeMVP[_e96];
    let _e100 = (*worldPos);
    sc4_ = (_e99 * vec4<f32>(_e100.x, _e100.y, _e100.z, 1f));
    let _e106 = sc4_;
    let _e109 = sc4_[3u];
    sc = (_e106.xyz / vec3(_e109));
    let _e112 = sc;
    let _e116 = ((_e112.xy * 0.5f) + vec2(0.5f));
    sc[0u] = _e116.x;
    sc[1u] = _e116.y;
    let _e122 = sc[0u];
    let _e123 = (_e122 < 0f);
    phi_470_ = _e123;
    if !(_e123) {
        let _e126 = sc[0u];
        phi_470_ = (_e126 > 1f);
    }
    let _e129 = phi_470_;
    phi_477_ = _e129;
    if !(_e129) {
        let _e132 = sc[1u];
        phi_477_ = (_e132 < 0f);
    }
    let _e135 = phi_477_;
    phi_484_ = _e135;
    if !(_e135) {
        let _e138 = sc[1u];
        phi_484_ = (_e138 > 1f);
    }
    let _e141 = phi_484_;
    phi_491_ = _e141;
    if !(_e141) {
        let _e144 = sc[2u];
        phi_491_ = (_e144 > 1f);
    }
    let _e147 = phi_491_;
    if _e147 {
        return 1f;
    }
    let _e148 = (*c);
    layer = f32(_e148);
    let _e150 = textureDimensions(shadowMap, 0i);
    texelSize = (vec2(1f) / vec2<f32>(vec2<i32>(_e150).xy));
    let _e157 = sc[2u];
    currentDepth = (_e157 - shadow_bias);
    shadow = 0f;
    if override_type_3_ {
        let _e159 = currentDepth;
        let _e160 = sc;
        let _e161 = _e160.xy;
        let _e162 = layer;
        let _e165 = vec3<f32>(_e161.x, _e161.y, _e162);
        let _e171 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e165.x, _e165.y), i32(_e165.z));
        shadow = step(_e159, _e171.x);
    } else {
        if override_type_3_1 {
            let _e174 = currentDepth;
            let _e175 = sc;
            let _e176 = _e175.xy;
            let _e177 = layer;
            let _e180 = vec3<f32>(_e176.x, _e176.y, _e177);
            let _e186 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e180.x, _e180.y), i32(_e180.z));
            let _e189 = shadow;
            shadow = (_e189 + step(_e174, _e186.x));
            let _e191 = currentDepth;
            let _e192 = sc;
            let _e195 = texelSize[0u];
            let _e197 = (_e192.xy + vec2<f32>(_e195, 0f));
            let _e198 = layer;
            let _e201 = vec3<f32>(_e197.x, _e197.y, _e198);
            let _e207 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e201.x, _e201.y), i32(_e201.z));
            let _e210 = shadow;
            shadow = (_e210 + step(_e191, _e207.x));
            let _e212 = currentDepth;
            let _e213 = sc;
            let _e216 = texelSize[0u];
            let _e218 = (_e213.xy - vec2<f32>(_e216, 0f));
            let _e219 = layer;
            let _e222 = vec3<f32>(_e218.x, _e218.y, _e219);
            let _e228 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e222.x, _e222.y), i32(_e222.z));
            let _e231 = shadow;
            shadow = (_e231 + step(_e212, _e228.x));
            let _e233 = currentDepth;
            let _e234 = sc;
            let _e237 = texelSize[1u];
            let _e239 = (_e234.xy + vec2<f32>(0f, _e237));
            let _e240 = layer;
            let _e243 = vec3<f32>(_e239.x, _e239.y, _e240);
            let _e249 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e243.x, _e243.y), i32(_e243.z));
            let _e252 = shadow;
            shadow = (_e252 + step(_e233, _e249.x));
            let _e254 = currentDepth;
            let _e255 = sc;
            let _e258 = texelSize[1u];
            let _e260 = (_e255.xy - vec2<f32>(0f, _e258));
            let _e261 = layer;
            let _e264 = vec3<f32>(_e260.x, _e260.y, _e261);
            let _e270 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e264.x, _e264.y), i32(_e264.z));
            let _e273 = shadow;
            shadow = (_e273 + step(_e254, _e270.x));
            let _e275 = shadow;
            shadow = (_e275 / 5f);
        } else {
            x = -1i;
            loop {
                let _e277 = x;
                if (_e277 <= 1i) {
                    y = -1i;
                    loop {
                        let _e279 = y;
                        if (_e279 <= 1i) {
                            let _e281 = currentDepth;
                            let _e282 = sc;
                            let _e284 = x;
                            let _e286 = y;
                            let _e289 = texelSize;
                            let _e291 = (_e282.xy + (vec2<f32>(f32(_e284), f32(_e286)) * _e289));
                            let _e292 = layer;
                            let _e295 = vec3<f32>(_e291.x, _e291.y, _e292);
                            let _e301 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e295.x, _e295.y), i32(_e295.z));
                            let _e304 = shadow;
                            shadow = (_e304 + step(_e281, _e301.x));
                            continue;
                        } else {
                            break;
                        }
                        continuing {
                            let _e306 = y;
                            y = (_e306 + 1i);
                        }
                    }
                    continue;
                } else {
                    break;
                }
                continuing {
                    let _e308 = x;
                    x = (_e308 + 1i);
                }
            }
            let _e310 = shadow;
            shadow = (_e310 / 9f);
        }
    }
    let _e312 = shadow;
    return _e312;
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

    let _e103 = unnamed.cascadeSplits;
    let _e104 = (*viewDepth_1);
    cmp = step(_e103, vec4(_e104));
    let _e108 = cmp[0u];
    let _e110 = cmp[1u];
    let _e113 = cmp[2u];
    let _e116 = cmp[3u];
    cascade = min(i32((((_e108 + _e110) + _e113) + _e116)), 3i);
    let _e120 = cascade;
    (*outCascade) = _e120;
    let _e121 = cascade;
    if (_e121 == 0i) {
        local = 0f;
    } else {
        let _e123 = cascade;
        let _e128 = unnamed.cascadeSplits[max((_e123 - 1i), 0i)];
        local = _e128;
    }
    let _e129 = local;
    prevSplit = _e129;
    let _e130 = cascade;
    let _e133 = unnamed.cascadeSplits[_e130];
    farSplit = _e133;
    let _e134 = farSplit;
    let _e135 = prevSplit;
    blendRange = max((0.1f * (_e134 - _e135)), 1f);
    let _e139 = farSplit;
    let _e140 = (*viewDepth_1);
    let _e142 = blendRange;
    blendT = clamp(((_e139 - _e140) / _e142), 0f, 1f);
    let _e145 = cascade;
    param_5 = _e145;
    let _e146 = (*worldPos_1);
    param_6 = _e146;
    let _e147 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param_5), (&param_6));
    s0_ = _e147;
    let _e148 = cascade;
    param_7 = min((_e148 + 1i), 3i);
    let _e151 = (*worldPos_1);
    param_8 = _e151;
    let _e152 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param_7), (&param_8));
    s1_ = _e152;
    let _e153 = s1_;
    let _e154 = s0_;
    let _e155 = blendT;
    return mix(_e153, _e154, _e155);
}

fn wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b(rgb: ptr<function, vec3<f32>>, lm_uv: ptr<function, vec2<f32>>) -> vec3<f32> {
    var sun_mask: f32;
    var shadow_1: f32;
    var ignoredCascade: i32;
    var param_9: vec3<f32>;
    var param_10: f32;
    var param_11: i32;

    let _e97 = unnamed.packed_indices[1i][3u];
    let _e103 = unnamed.packed_indices[1i][3u];
    let _e108 = (*lm_uv);
    let _e109 = textureSample(wired_bindless_images[(_e97 & 4095u)], wired_bindless_samplers[((_e103 >> bitcast<u32>(12i)) & 255u)], _e108);
    sun_mask = _e109.x;
    let _e111 = sun_mask;
    if (_e111 > 0.001f) {
        let _e113 = shadowData_1;
        param_9 = _e113.xyz;
        let _e116 = shadowData_1[3u];
        param_10 = _e116;
        let _e117 = sampleShadow_u0028_vf3_u003b_f1_u003b_i1_u003b((&param_9), (&param_10), (&param_11));
        let _e118 = param_11;
        ignoredCascade = _e118;
        shadow_1 = _e117;
        let _e119 = shadow_1;
        let _e120 = sun_mask;
        let _e122 = (*rgb);
        (*rgb) = (_e122 * mix(1f, _e119, _e120));
    }
    let _e124 = (*rgb);
    return _e124;
}

fn wired_apply_sun_shadow_operand_u0028_vf3_u003b(rgb_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var param_12: vec3<f32>;
    var param_13: vec2<f32>;
    var param_14: vec3<f32>;
    var param_15: vec2<f32>;

    if override_type_3_2 {
        let _e91 = (*rgb_1);
        param_12 = _e91;
        let _e92 = frag_tex_coord0_1;
        param_13 = _e92;
        let _e93 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_12), (&param_13));
        return _e93;
    }
    if override_type_3_3 {
        let _e94 = (*rgb_1);
        param_14 = _e94;
        let _e95 = frag_tex_coord1_1;
        param_15 = _e95;
        let _e96 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_14), (&param_15));
        return _e96;
    }
    let _e97 = (*rgb_1);
    return _e97;
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb_2: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e89 = (*rgb_2);
    let _e92 = unnamed.worldLightParams[0u];
    boosted = (_e89 * _e92);
    let _e95 = boosted[0u];
    let _e97 = boosted[1u];
    let _e99 = boosted[2u];
    peak = max(_e95, max(_e97, _e99));
    let _e102 = peak;
    if (_e102 > 1f) {
        let _e104 = peak;
        let _e105 = boosted;
        boosted = (_e105 / vec3(_e104));
    }
    let _e108 = boosted;
    return _e108;
}

fn sRGBToLinear_u0028_vf3_u003b(c_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e90 = (*c_1);
    (*c_1) = max(_e90, vec3<f32>(0f, 0f, 0f));
    let _e92 = (*c_1);
    cutoff = (_e92 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e94 = (*c_1);
    lo = (_e94 / vec3(12.92f));
    let _e97 = (*c_1);
    hi = pow(((_e97 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e102 = hi;
    let _e103 = lo;
    let _e104 = cutoff;
    return mix(_e102, _e103, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e104));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_2: vec4<f32>;
    var param_16: vec3<f32>;
    var param_17: vec3<f32>;

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
        param_16 = _e121.xyz;
        let _e123 = sRGBToLinear_u0028_vf3_u003b((&param_16));
        c_2[0u] = _e123.x;
        c_2[1u] = _e123.y;
        c_2[2u] = _e123.z;
    }
    let _e130 = (*slot);
    if (lightmap_slot == (_e130 + 1i)) {
        let _e133 = c_2;
        param_17 = _e133.xyz;
        let _e135 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_17));
        c_2[0u] = _e135.x;
        c_2[1u] = _e135.y;
        c_2[2u] = _e135.z;
    }
    let _e142 = c_2;
    return _e142;
}

fn main_1() {
    var frag_color0_: vec4<f32>;
    var param_18: vec3<f32>;
    var sampledColor0_: vec4<f32>;
    var param_19: u32;
    var param_20: vec2<f32>;
    var param_21: i32;
    var sampledColor1_: vec4<f32>;
    var param_22: u32;
    var param_23: vec2<f32>;
    var param_24: i32;
    var color0_: vec4<f32>;
    var color1_: vec4<f32>;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var color1_2: vec4<f32>;
    var param_25: vec3<f32>;
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var fogAmount: f32;
    var param_26: f32;

    let _e107 = frag_color0In_1;
    param_18 = _e107.xyz;
    let _e109 = sRGBToLinear_u0028_vf3_u003b((&param_18));
    let _e111 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e109.x, _e109.y, _e109.z, _e111);
    param_19 = 0u;
    let _e116 = frag_tex_coord0_1;
    param_20 = _e116;
    param_21 = 0i;
    let _e117 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_19), (&param_20), (&param_21));
    sampledColor0_ = _e117;
    param_22 = 1u;
    let _e118 = frag_tex_coord1_1;
    param_23 = _e118;
    param_24 = 1i;
    let _e119 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_22), (&param_23), (&param_24));
    sampledColor1_ = _e119;
    let _e120 = sampledColor0_;
    let _e121 = frag_color0_;
    color0_ = (_e120 * _e121);
    if override_type_3_4 {
        let _e123 = sampledColor1_;
        color1_ = _e123;
        let _e124 = color0_;
        let _e126 = color1_;
        let _e128 = (_e124.xyz + _e126.xyz);
        let _e130 = color0_[3u];
        let _e132 = color1_[3u];
        base = vec4<f32>(_e128.x, _e128.y, _e128.z, (_e130 * _e132));
    } else {
        if override_type_3_5 {
            let _e138 = sampledColor1_;
            let _e139 = frag_color0_;
            color1_1 = (_e138 * _e139);
            let _e141 = color0_;
            let _e143 = color1_1;
            let _e145 = (_e141.xyz + _e143.xyz);
            let _e147 = color0_[3u];
            let _e149 = color1_1[3u];
            base = vec4<f32>(_e145.x, _e145.y, _e145.z, (_e147 * _e149));
        } else {
            let _e155 = sampledColor1_;
            color1_2 = _e155;
            let _e156 = color0_;
            let _e158 = color1_2;
            let _e160 = (_e156.xyz * _e158.xyz);
            base[0u] = _e160.x;
            base[1u] = _e160.y;
            base[2u] = _e160.z;
            let _e168 = color0_[3u];
            let _e170 = color1_2[3u];
            base[3u] = (_e168 * _e170);
        }
    }
    let _e173 = base;
    param_25 = _e173.xyz;
    let _e175 = wired_apply_sun_shadow_operand_u0028_vf3_u003b((&param_25));
    base[0u] = _e175.x;
    base[1u] = _e175.y;
    base[2u] = _e175.z;
    if override_type_3_6 {
        let _e184 = unnamed.worldLightParams[1u];
        wetness = clamp(_e184, 0f, 1f);
        let _e188 = unnamed.worldLightParams[2u];
        frost = clamp(_e188, 0f, 1f);
        let _e190 = base;
        luminance = dot(_e190.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e193 = wetness;
        let _e195 = base;
        let _e197 = (_e195.xyz * mix(1f, 0.82f, _e193));
        base[0u] = _e197.x;
        base[1u] = _e197.y;
        base[2u] = _e197.z;
        let _e204 = base;
        let _e206 = luminance;
        let _e208 = luminance;
        let _e210 = luminance;
        let _e212 = frost;
        let _e215 = mix(_e204.xyz, vec3<f32>((_e206 * 0.88f), (_e208 * 0.94f), _e210), vec3((_e212 * 0.55f)));
        base[0u] = _e215.x;
        base[1u] = _e215.y;
        base[2u] = _e215.z;
    }
    let _e224 = unnamed.materialFactors[0u];
    if (_e224 >= 0f) {
        let _e227 = unnamed.materialBaseColor;
        let _e228 = base;
        base = (_e228 * _e227);
    }
    let _e230 = color0_;
    let _e233 = unnamed.emissionRadiance;
    let _e236 = base;
    let _e238 = (_e236.xyz + (_e230.xyz * _e233.xyz));
    base[0u] = _e238.x;
    base[1u] = _e238.y;
    base[2u] = _e238.z;
    let _e245 = wired_advanced_fog_enabled_u0028_();
    if _e245 {
        let _e246 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e246;
        let _e247 = base;
        let _e250 = unnamed.advancedFogColorDensity;
        let _e252 = fogAmount;
        let _e254 = mix(_e247.xyz, _e250.xyz, vec3(_e252));
        base[0u] = _e254.x;
        base[1u] = _e254.y;
        base[2u] = _e254.z;
    }
    if override_type_3_7 {
        let _e262 = base[3u];
        if (_e262 == 0f) {
            discard;
        }
    } else {
        if override_type_3_8 {
            let _e264 = base;
            let _e266 = base;
            if (dot(_e264.xyz, _e266.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e270 = base;
    out_color = _e270;
    param_26 = 1f;
    wiredTemporalWriteAux_u0028_f1_u003b((&param_26));
    return;
}

@fragment
fn main(@location(12) @interpolate(flat) temporalOutcome: u32, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @builtin(position) gl_FragCoord: vec4<f32>, @location(7) shadowData: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(0) frag_color0In: vec4<f32>) -> FragmentOutput {
    temporalOutcome_1 = temporalOutcome;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    gl_FragCoord_1 = gl_FragCoord;
    shadowData_1 = shadowData;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    frag_color0In_1 = frag_color0In;
    main_1();
    let _e19 = out_temporal_velocity;
    let _e20 = out_temporal_validity;
    let _e21 = out_color;
    return FragmentOutput(_e19, _e20, _e21);
}
