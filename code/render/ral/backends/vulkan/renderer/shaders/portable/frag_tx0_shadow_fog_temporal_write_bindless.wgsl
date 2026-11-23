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
var<private> frag_color0In_1: vec4<f32>;
var<private> out_color: vec4<f32>;

fn wiredTemporalFinite2_u0028_vf2_u003b(value: ptr<function, vec2<f32>>) -> bool {
    var phi_97_: bool;

    let _e91 = (*value);
    let _e92 = (*value);
    let _e94 = all((_e91 == _e92));
    phi_97_ = _e94;
    if _e94 {
        let _e95 = (*value);
        phi_97_ = all((abs(_e95) <= vec2<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e100 = phi_97_;
    return _e100;
}

fn wiredTemporalFinite4_u0028_vf4_u003b(value_1: ptr<function, vec4<f32>>) -> bool {
    var phi_82_: bool;

    let _e91 = (*value_1);
    let _e92 = (*value_1);
    let _e94 = all((_e91 == _e92));
    phi_82_ = _e94;
    if _e94 {
        let _e95 = (*value_1);
        phi_82_ = all((abs(_e95) <= vec4<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e100 = phi_82_;
    return _e100;
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
    let _e101 = temporalOutcome_1;
    let _e102 = (_e101 != 1u);
    phi_120_ = _e102;
    if !(_e102) {
        let _e104 = temporalCurrentClip_1;
        param = _e104;
        let _e105 = wiredTemporalFinite4_u0028_vf4_u003b((&param));
        phi_120_ = !(_e105);
    }
    let _e108 = phi_120_;
    phi_129_ = _e108;
    if !(_e108) {
        let _e110 = temporalPreviousClip_1;
        param_1 = _e110;
        let _e111 = wiredTemporalFinite4_u0028_vf4_u003b((&param_1));
        phi_129_ = !(_e111);
    }
    let _e114 = phi_129_;
    phi_139_ = _e114;
    if !(_e114) {
        let _e117 = temporalCurrentClip_1[3u];
        phi_139_ = (_e117 <= 0.000001f);
    }
    let _e120 = phi_139_;
    phi_146_ = _e120;
    if !(_e120) {
        let _e123 = temporalPreviousClip_1[3u];
        phi_146_ = (_e123 <= 0.000001f);
    }
    let _e126 = phi_146_;
    if _e126 {
        return;
    }
    let _e127 = temporalCurrentClip_1;
    let _e130 = temporalCurrentClip_1[3u];
    currentNdc = (_e127.xy / vec2(_e130));
    let _e133 = temporalPreviousClip_1;
    let _e136 = temporalPreviousClip_1[3u];
    previousNdc = (_e133.xy / vec2(_e136));
    let _e139 = currentNdc;
    param_2 = _e139;
    let _e140 = wiredTemporalFinite2_u0028_vf2_u003b((&param_2));
    let _e141 = !(_e140);
    phi_175_ = _e141;
    if !(_e141) {
        let _e143 = previousNdc;
        param_3 = _e143;
        let _e144 = wiredTemporalFinite2_u0028_vf2_u003b((&param_3));
        phi_175_ = !(_e144);
    }
    let _e147 = phi_175_;
    if _e147 {
        return;
    }
    let _e148 = currentNdc;
    currentUv = ((_e148 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e151 = previousNdc;
    previousUv = ((_e151 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e154 = currentUv;
    let _e155 = previousUv;
    velocity = (_e154 - _e155);
    let _e157 = velocity;
    param_4 = _e157;
    let _e158 = wiredTemporalFinite2_u0028_vf2_u003b((&param_4));
    if !(_e158) {
        return;
    }
    let _e160 = velocity;
    out_temporal_velocity = _e160;
    let _e161 = (*coverageConfidence);
    out_temporal_validity = clamp(_e161, 0f, 1f);
    return;
}

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e93 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e93 + 0.5f));
    let _e98 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e100 = fogType;
    let _e103 = fogType;
    return (((_e98 > 0.5f) && (_e100 >= 1i)) && (_e103 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e93 = wired_advanced_fog_enabled_u0028_();
    if !(_e93) {
        return 0f;
    }
    let _e96 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e96, 0.000001f));
    let _e101 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e101 + 0.5f));
    let _e104 = fogType_1;
    if (_e104 == 1i) {
        let _e108 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e108 <= 0f) {
            return 0f;
        }
        let _e110 = viewDepth;
        let _e113 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e110 / _e113), 0f, 1f);
    }
    let _e118 = unnamed.advancedFogColorDensity[3u];
    let _e120 = viewDepth;
    opticalDepth = (max(_e118, 0f) * _e120);
    let _e122 = fogType_1;
    if (_e122 == 2i) {
        let _e124 = opticalDepth;
        return clamp((1f - exp(-(_e124))), 0f, 1f);
    }
    let _e129 = opticalDepth;
    let _e130 = opticalDepth;
    return clamp((1f - exp(-((_e129 * _e130)))), 0f, 1f);
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

    let _e100 = (*c);
    let _e103 = unnamed.cascadeMVP[_e100];
    let _e104 = (*worldPos);
    sc4_ = (_e103 * vec4<f32>(_e104.x, _e104.y, _e104.z, 1f));
    let _e110 = sc4_;
    let _e113 = sc4_[3u];
    sc = (_e110.xyz / vec3(_e113));
    let _e116 = sc;
    let _e120 = ((_e116.xy * 0.5f) + vec2(0.5f));
    sc[0u] = _e120.x;
    sc[1u] = _e120.y;
    let _e126 = sc[0u];
    let _e127 = (_e126 < 0f);
    phi_470_ = _e127;
    if !(_e127) {
        let _e130 = sc[0u];
        phi_470_ = (_e130 > 1f);
    }
    let _e133 = phi_470_;
    phi_477_ = _e133;
    if !(_e133) {
        let _e136 = sc[1u];
        phi_477_ = (_e136 < 0f);
    }
    let _e139 = phi_477_;
    phi_484_ = _e139;
    if !(_e139) {
        let _e142 = sc[1u];
        phi_484_ = (_e142 > 1f);
    }
    let _e145 = phi_484_;
    phi_491_ = _e145;
    if !(_e145) {
        let _e148 = sc[2u];
        phi_491_ = (_e148 > 1f);
    }
    let _e151 = phi_491_;
    if _e151 {
        return 1f;
    }
    let _e152 = (*c);
    layer = f32(_e152);
    let _e154 = textureDimensions(shadowMap, 0i);
    texelSize = (vec2(1f) / vec2<f32>(vec2<i32>(_e154).xy));
    let _e161 = sc[2u];
    currentDepth = (_e161 - shadow_bias);
    shadow = 0f;
    if override_type_3_ {
        let _e163 = currentDepth;
        let _e164 = sc;
        let _e165 = _e164.xy;
        let _e166 = layer;
        let _e169 = vec3<f32>(_e165.x, _e165.y, _e166);
        let _e175 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e169.x, _e169.y), i32(_e169.z));
        shadow = step(_e163, _e175.x);
    } else {
        if override_type_3_1 {
            let _e178 = currentDepth;
            let _e179 = sc;
            let _e180 = _e179.xy;
            let _e181 = layer;
            let _e184 = vec3<f32>(_e180.x, _e180.y, _e181);
            let _e190 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e184.x, _e184.y), i32(_e184.z));
            let _e193 = shadow;
            shadow = (_e193 + step(_e178, _e190.x));
            let _e195 = currentDepth;
            let _e196 = sc;
            let _e199 = texelSize[0u];
            let _e201 = (_e196.xy + vec2<f32>(_e199, 0f));
            let _e202 = layer;
            let _e205 = vec3<f32>(_e201.x, _e201.y, _e202);
            let _e211 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e205.x, _e205.y), i32(_e205.z));
            let _e214 = shadow;
            shadow = (_e214 + step(_e195, _e211.x));
            let _e216 = currentDepth;
            let _e217 = sc;
            let _e220 = texelSize[0u];
            let _e222 = (_e217.xy - vec2<f32>(_e220, 0f));
            let _e223 = layer;
            let _e226 = vec3<f32>(_e222.x, _e222.y, _e223);
            let _e232 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e226.x, _e226.y), i32(_e226.z));
            let _e235 = shadow;
            shadow = (_e235 + step(_e216, _e232.x));
            let _e237 = currentDepth;
            let _e238 = sc;
            let _e241 = texelSize[1u];
            let _e243 = (_e238.xy + vec2<f32>(0f, _e241));
            let _e244 = layer;
            let _e247 = vec3<f32>(_e243.x, _e243.y, _e244);
            let _e253 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e247.x, _e247.y), i32(_e247.z));
            let _e256 = shadow;
            shadow = (_e256 + step(_e237, _e253.x));
            let _e258 = currentDepth;
            let _e259 = sc;
            let _e262 = texelSize[1u];
            let _e264 = (_e259.xy - vec2<f32>(0f, _e262));
            let _e265 = layer;
            let _e268 = vec3<f32>(_e264.x, _e264.y, _e265);
            let _e274 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e268.x, _e268.y), i32(_e268.z));
            let _e277 = shadow;
            shadow = (_e277 + step(_e258, _e274.x));
            let _e279 = shadow;
            shadow = (_e279 / 5f);
        } else {
            x = -1i;
            loop {
                let _e281 = x;
                if (_e281 <= 1i) {
                    y = -1i;
                    loop {
                        let _e283 = y;
                        if (_e283 <= 1i) {
                            let _e285 = currentDepth;
                            let _e286 = sc;
                            let _e288 = x;
                            let _e290 = y;
                            let _e293 = texelSize;
                            let _e295 = (_e286.xy + (vec2<f32>(f32(_e288), f32(_e290)) * _e293));
                            let _e296 = layer;
                            let _e299 = vec3<f32>(_e295.x, _e295.y, _e296);
                            let _e305 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e299.x, _e299.y), i32(_e299.z));
                            let _e308 = shadow;
                            shadow = (_e308 + step(_e285, _e305.x));
                            continue;
                        } else {
                            break;
                        }
                        continuing {
                            let _e310 = y;
                            y = (_e310 + 1i);
                        }
                    }
                    continue;
                } else {
                    break;
                }
                continuing {
                    let _e312 = x;
                    x = (_e312 + 1i);
                }
            }
            let _e314 = shadow;
            shadow = (_e314 / 9f);
        }
    }
    let _e316 = shadow;
    return _e316;
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

    let _e107 = unnamed.cascadeSplits;
    let _e108 = (*viewDepth_1);
    cmp = step(_e107, vec4(_e108));
    let _e112 = cmp[0u];
    let _e114 = cmp[1u];
    let _e117 = cmp[2u];
    let _e120 = cmp[3u];
    cascade = min(i32((((_e112 + _e114) + _e117) + _e120)), 3i);
    let _e124 = cascade;
    (*outCascade) = _e124;
    let _e125 = cascade;
    if (_e125 == 0i) {
        local = 0f;
    } else {
        let _e127 = cascade;
        let _e132 = unnamed.cascadeSplits[max((_e127 - 1i), 0i)];
        local = _e132;
    }
    let _e133 = local;
    prevSplit = _e133;
    let _e134 = cascade;
    let _e137 = unnamed.cascadeSplits[_e134];
    farSplit = _e137;
    let _e138 = farSplit;
    let _e139 = prevSplit;
    blendRange = max((0.1f * (_e138 - _e139)), 1f);
    let _e143 = farSplit;
    let _e144 = (*viewDepth_1);
    let _e146 = blendRange;
    blendT = clamp(((_e143 - _e144) / _e146), 0f, 1f);
    let _e149 = cascade;
    param_5 = _e149;
    let _e150 = (*worldPos_1);
    param_6 = _e150;
    let _e151 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param_5), (&param_6));
    s0_ = _e151;
    let _e152 = cascade;
    param_7 = min((_e152 + 1i), 3i);
    let _e155 = (*worldPos_1);
    param_8 = _e155;
    let _e156 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param_7), (&param_8));
    s1_ = _e156;
    let _e157 = s1_;
    let _e158 = s0_;
    let _e159 = blendT;
    return mix(_e157, _e158, _e159);
}

fn wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b(rgb: ptr<function, vec3<f32>>, lm_uv: ptr<function, vec2<f32>>) -> vec3<f32> {
    var sun_mask: f32;
    var shadow_1: f32;
    var ignoredCascade: i32;
    var param_9: vec3<f32>;
    var param_10: f32;
    var param_11: i32;

    let _e101 = unnamed.packed_indices[1i][3u];
    let _e107 = unnamed.packed_indices[1i][3u];
    let _e112 = (*lm_uv);
    let _e113 = textureSample(wired_bindless_images[(_e101 & 4095u)], wired_bindless_samplers[((_e107 >> bitcast<u32>(12i)) & 255u)], _e112);
    sun_mask = _e113.x;
    let _e115 = sun_mask;
    if (_e115 > 0.001f) {
        let _e117 = shadowData_1;
        param_9 = _e117.xyz;
        let _e120 = shadowData_1[3u];
        param_10 = _e120;
        let _e121 = sampleShadow_u0028_vf3_u003b_f1_u003b_i1_u003b((&param_9), (&param_10), (&param_11));
        let _e122 = param_11;
        ignoredCascade = _e122;
        shadow_1 = _e121;
        let _e123 = shadow_1;
        let _e124 = sun_mask;
        let _e126 = (*rgb);
        (*rgb) = (_e126 * mix(1f, _e123, _e124));
    }
    let _e128 = (*rgb);
    return _e128;
}

fn wired_apply_sun_shadow_operand_u0028_vf3_u003b(rgb_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var param_12: vec3<f32>;
    var param_13: vec2<f32>;

    if override_type_3_2 {
        let _e93 = (*rgb_1);
        param_12 = _e93;
        let _e94 = frag_tex_coord0_1;
        param_13 = _e94;
        let _e95 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_12), (&param_13));
        return _e95;
    }
    let _e96 = (*rgb_1);
    return _e96;
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb_2: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e93 = (*rgb_2);
    let _e96 = unnamed.worldLightParams[0u];
    boosted = (_e93 * _e96);
    let _e99 = boosted[0u];
    let _e101 = boosted[1u];
    let _e103 = boosted[2u];
    peak = max(_e99, max(_e101, _e103));
    let _e106 = peak;
    if (_e106 > 1f) {
        let _e108 = peak;
        let _e109 = boosted;
        boosted = (_e109 / vec3(_e108));
    }
    let _e112 = boosted;
    return _e112;
}

fn sRGBToLinear_u0028_vf3_u003b(c_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e94 = (*c_1);
    (*c_1) = max(_e94, vec3<f32>(0f, 0f, 0f));
    let _e96 = (*c_1);
    cutoff = (_e96 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e98 = (*c_1);
    lo = (_e98 / vec3(12.92f));
    let _e101 = (*c_1);
    hi = pow(((_e101 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e106 = hi;
    let _e107 = lo;
    let _e108 = cutoff;
    return mix(_e106, _e107, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e108));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_2: vec4<f32>;
    var param_14: vec3<f32>;
    var param_15: vec3<f32>;

    let _e96 = (*role);
    let _e98 = (*role);
    let _e103 = unnamed.packed_indices[(_e96 / 4u)][(_e98 % 4u)];
    let _e106 = (*role);
    let _e108 = (*role);
    let _e113 = unnamed.packed_indices[(_e106 / 4u)][(_e108 % 4u)];
    let _e118 = (*uv);
    let _e119 = textureSample(wired_bindless_images[(_e103 & 4095u)], wired_bindless_samplers[((_e113 >> bitcast<u32>(12i)) & 255u)], _e118);
    c_2 = _e119;
    let _e120 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e120))) == 0i) {
        let _e125 = c_2;
        param_14 = _e125.xyz;
        let _e127 = sRGBToLinear_u0028_vf3_u003b((&param_14));
        c_2[0u] = _e127.x;
        c_2[1u] = _e127.y;
        c_2[2u] = _e127.z;
    }
    let _e134 = (*slot);
    if (lightmap_slot == (_e134 + 1i)) {
        let _e137 = c_2;
        param_15 = _e137.xyz;
        let _e139 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_15));
        c_2[0u] = _e139.x;
        c_2[1u] = _e139.y;
        c_2[2u] = _e139.z;
    }
    let _e146 = c_2;
    return _e146;
}

fn main_1() {
    var fog: vec4<f32>;
    var frag_color0_: vec4<f32>;
    var param_16: vec3<f32>;
    var sampledColor0_: vec4<f32>;
    var param_17: u32;
    var param_18: vec2<f32>;
    var param_19: i32;
    var color0_: vec4<f32>;
    var base: vec4<f32>;
    var param_20: vec3<f32>;
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var fogAmount: f32;
    var param_21: f32;

    let _e108 = unnamed.packed_indices[0i][3u];
    let _e114 = unnamed.packed_indices[0i][3u];
    let _e119 = fog_tex_coord_1;
    let _e120 = textureSample(wired_bindless_images[(_e108 & 4095u)], wired_bindless_samplers[((_e114 >> bitcast<u32>(12i)) & 255u)], _e119);
    fog = _e120;
    let _e121 = frag_color0In_1;
    param_16 = _e121.xyz;
    let _e123 = sRGBToLinear_u0028_vf3_u003b((&param_16));
    let _e125 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e123.x, _e123.y, _e123.z, _e125);
    param_17 = 0u;
    let _e130 = frag_tex_coord0_1;
    param_18 = _e130;
    param_19 = 0i;
    let _e131 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_17), (&param_18), (&param_19));
    sampledColor0_ = _e131;
    let _e132 = sampledColor0_;
    let _e133 = frag_color0_;
    color0_ = (_e132 * _e133);
    let _e135 = color0_;
    base = _e135;
    let _e136 = color0_;
    base = _e136;
    let _e137 = base;
    param_20 = _e137.xyz;
    let _e139 = wired_apply_sun_shadow_operand_u0028_vf3_u003b((&param_20));
    base[0u] = _e139.x;
    base[1u] = _e139.y;
    base[2u] = _e139.z;
    if override_type_3_3 {
        let _e148 = unnamed.worldLightParams[1u];
        wetness = clamp(_e148, 0f, 1f);
        let _e152 = unnamed.worldLightParams[2u];
        frost = clamp(_e152, 0f, 1f);
        let _e154 = base;
        luminance = dot(_e154.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e157 = wetness;
        let _e159 = base;
        let _e161 = (_e159.xyz * mix(1f, 0.82f, _e157));
        base[0u] = _e161.x;
        base[1u] = _e161.y;
        base[2u] = _e161.z;
        let _e168 = base;
        let _e170 = luminance;
        let _e172 = luminance;
        let _e174 = luminance;
        let _e176 = frost;
        let _e179 = mix(_e168.xyz, vec3<f32>((_e170 * 0.88f), (_e172 * 0.94f), _e174), vec3((_e176 * 0.55f)));
        base[0u] = _e179.x;
        base[1u] = _e179.y;
        base[2u] = _e179.z;
    }
    let _e188 = unnamed.materialFactors[0u];
    if (_e188 >= 0f) {
        let _e191 = unnamed.materialBaseColor;
        let _e192 = base;
        base = (_e192 * _e191);
    }
    let _e194 = color0_;
    let _e197 = unnamed.emissionRadiance;
    let _e200 = base;
    let _e202 = (_e200.xyz + (_e194.xyz * _e197.xyz));
    base[0u] = _e202.x;
    base[1u] = _e202.y;
    base[2u] = _e202.z;
    let _e209 = wired_advanced_fog_enabled_u0028_();
    if _e209 {
        let _e210 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e210;
        if override_type_3_4 {
            let _e211 = fogAmount;
            let _e213 = base;
            let _e215 = (_e213.xyz * (1f - _e211));
            base[0u] = _e215.x;
            base[1u] = _e215.y;
            base[2u] = _e215.z;
        } else {
            if override_type_3_5 {
                let _e222 = fogAmount;
                let _e224 = base;
                base = (_e224 * (1f - _e222));
            } else {
                if override_type_3_6 {
                    let _e226 = fogAmount;
                    let _e229 = base[3u];
                    base[3u] = (_e229 * (1f - _e226));
                } else {
                    let _e232 = base;
                    let _e235 = unnamed.advancedFogColorDensity;
                    let _e237 = fogAmount;
                    let _e239 = mix(_e232.xyz, _e235.xyz, vec3(_e237));
                    base[0u] = _e239.x;
                    base[1u] = _e239.y;
                    base[2u] = _e239.z;
                }
            }
        }
    } else {
        if override_type_3_7 {
            let _e246 = base;
            let _e249 = fog[3u];
            let _e251 = (_e246.xyz * (1f - _e249));
            base[0u] = _e251.x;
            base[1u] = _e251.y;
            base[2u] = _e251.z;
        } else {
            if override_type_3_8 {
                let _e258 = base;
                let _e260 = fog[3u];
                base = (_e258 * (1f - _e260));
            } else {
                if override_type_3_9 {
                    let _e264 = base[3u];
                    let _e266 = fog[3u];
                    base[3u] = (_e264 * (1f - _e266));
                } else {
                    let _e270 = base;
                    let _e271 = fog;
                    let _e273 = unnamed.fogColor;
                    let _e276 = fog[3u];
                    base = mix(_e270, (_e271 * _e273), vec4(_e276));
                }
            }
        }
    }
    if override_type_3_10 {
        let _e280 = base[3u];
        if (_e280 == 0f) {
            discard;
        }
    } else {
        if override_type_3_11 {
            let _e282 = base;
            let _e284 = base;
            if (dot(_e282.xyz, _e284.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e288 = base;
    out_color = _e288;
    param_21 = 1f;
    wiredTemporalWriteAux_u0028_f1_u003b((&param_21));
    return;
}

@fragment
fn main(@location(12) @interpolate(flat) temporalOutcome: u32, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @builtin(position) gl_FragCoord: vec4<f32>, @location(7) shadowData: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(4) fog_tex_coord: vec2<f32>, @location(0) frag_color0In: vec4<f32>) -> FragmentOutput {
    temporalOutcome_1 = temporalOutcome;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    gl_FragCoord_1 = gl_FragCoord;
    shadowData_1 = shadowData;
    frag_tex_coord0_1 = frag_tex_coord0_;
    fog_tex_coord_1 = fog_tex_coord;
    frag_color0In_1 = frag_color0In;
    main_1();
    let _e19 = out_temporal_velocity;
    let _e20 = out_temporal_validity;
    let _e21 = out_color;
    return FragmentOutput(_e19, _e20, _e21);
}
