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
override override_type_3_3: bool = (lightmap_slot == 2i);
@id(8) override identity_color: f32 = 1f;
@id(9) override identity_alpha: f32 = 1f;
@id(6) override tex_mode: i32 = 0i;
override override_type_3_4: bool = (tex_mode == 1i);
override override_type_3_5: bool = (tex_mode == 2i);
override override_type_3_6: bool = (lightmap_slot != 0i);
@id(7) override discard_mode: i32 = 0i;
override override_type_3_7: bool = (discard_mode == 1i);
override override_type_3_8: bool = (discard_mode == 2i);
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
var<private> frag_tex_coord1_1: vec2<f32>;
var<private> out_color: vec4<f32>;

fn wiredTemporalFinite2_u0028_vf2_u003b(value: ptr<function, vec2<f32>>) -> bool {
    var phi_98_: bool;

    let _e89 = (*value);
    let _e90 = (*value);
    let _e92 = all((_e89 == _e90));
    phi_98_ = _e92;
    if _e92 {
        let _e93 = (*value);
        phi_98_ = all((abs(_e93) <= vec2<f32>(0x1.fffffep+127f, 0x1.fffffep+127f)));
    }
    let _e98 = phi_98_;
    return _e98;
}

fn wiredTemporalFinite4_u0028_vf4_u003b(value_1: ptr<function, vec4<f32>>) -> bool {
    var phi_83_: bool;

    let _e89 = (*value_1);
    let _e90 = (*value_1);
    let _e92 = all((_e89 == _e90));
    phi_83_ = _e92;
    if _e92 {
        let _e93 = (*value_1);
        phi_83_ = all((abs(_e93) <= vec4<f32>(0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f)));
    }
    let _e98 = phi_83_;
    return _e98;
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
    let _e100 = temporalOutcome_1;
    let _e101 = (_e100 != 1u);
    phi_122_ = _e101;
    if !(_e101) {
        let _e103 = temporalCurrentClip_1;
        param = _e103;
        let _e104 = wiredTemporalFinite4_u0028_vf4_u003b((&param));
        phi_122_ = !(_e104);
    }
    let _e107 = phi_122_;
    phi_131_ = _e107;
    if !(_e107) {
        let _e109 = temporalPreviousClip_1;
        param_1 = _e109;
        let _e110 = wiredTemporalFinite4_u0028_vf4_u003b((&param_1));
        phi_131_ = !(_e110);
    }
    let _e113 = phi_131_;
    phi_141_ = _e113;
    if !(_e113) {
        let _e116 = temporalCurrentClip_1[3u];
        phi_141_ = (_e116 <= 0.000001f);
    }
    let _e119 = phi_141_;
    phi_148_ = _e119;
    if !(_e119) {
        let _e122 = temporalPreviousClip_1[3u];
        phi_148_ = (_e122 <= 0.000001f);
    }
    let _e125 = phi_148_;
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
    phi_177_ = _e140;
    if !(_e140) {
        let _e142 = previousNdc;
        param_3 = _e142;
        let _e143 = wiredTemporalFinite2_u0028_vf2_u003b((&param_3));
        phi_177_ = !(_e143);
    }
    let _e146 = phi_177_;
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
    out_temporal_validity = vec4<f32>(clamp(_e160, 0f, 1f), 0f, 0f, 1f);
    return;
}

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e91 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e91 + 0.5f));
    let _e96 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e98 = fogType;
    let _e101 = fogType;
    return (((_e96 > 0.5f) && (_e98 >= 1i)) && (_e101 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e91 = wired_advanced_fog_enabled_u0028_();
    if !(_e91) {
        return 0f;
    }
    let _e94 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e94, 0.000001f));
    let _e99 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e99 + 0.5f));
    let _e102 = fogType_1;
    if (_e102 == 1i) {
        let _e106 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e106 <= 0f) {
            return 0f;
        }
        let _e108 = viewDepth;
        let _e111 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e108 / _e111), 0f, 1f);
    }
    let _e116 = unnamed.advancedFogColorDensity[3u];
    let _e118 = viewDepth;
    opticalDepth = (max(_e116, 0f) * _e118);
    let _e120 = fogType_1;
    if (_e120 == 2i) {
        let _e122 = opticalDepth;
        return clamp((1f - exp(-(_e122))), 0f, 1f);
    }
    let _e127 = opticalDepth;
    let _e128 = opticalDepth;
    return clamp((1f - exp(-((_e127 * _e128)))), 0f, 1f);
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

    let _e98 = (*c);
    let _e101 = unnamed.cascadeMVP[_e98];
    let _e102 = (*worldPos);
    sc4_ = (_e101 * vec4<f32>(_e102.x, _e102.y, _e102.z, 1f));
    let _e108 = sc4_;
    let _e111 = sc4_[3u];
    sc = (_e108.xyz / vec3(_e111));
    let _e114 = sc;
    let _e118 = ((_e114.xy * 0.5f) + vec2(0.5f));
    sc[0u] = _e118.x;
    sc[1u] = _e118.y;
    let _e124 = sc[0u];
    let _e125 = (_e124 < 0f);
    phi_473_ = _e125;
    if !(_e125) {
        let _e128 = sc[0u];
        phi_473_ = (_e128 > 1f);
    }
    let _e131 = phi_473_;
    phi_480_ = _e131;
    if !(_e131) {
        let _e134 = sc[1u];
        phi_480_ = (_e134 < 0f);
    }
    let _e137 = phi_480_;
    phi_487_ = _e137;
    if !(_e137) {
        let _e140 = sc[1u];
        phi_487_ = (_e140 > 1f);
    }
    let _e143 = phi_487_;
    phi_494_ = _e143;
    if !(_e143) {
        let _e146 = sc[2u];
        phi_494_ = (_e146 > 1f);
    }
    let _e149 = phi_494_;
    if _e149 {
        return 1f;
    }
    let _e150 = (*c);
    layer = f32(_e150);
    let _e152 = textureDimensions(shadowMap, 0i);
    texelSize = (vec2(1f) / vec2<f32>(vec2<i32>(_e152).xy));
    let _e159 = sc[2u];
    currentDepth = (_e159 - shadow_bias);
    shadow = 0f;
    if override_type_3_ {
        let _e161 = currentDepth;
        let _e162 = sc;
        let _e163 = _e162.xy;
        let _e164 = layer;
        let _e167 = vec3<f32>(_e163.x, _e163.y, _e164);
        let _e173 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e167.x, _e167.y), i32(_e167.z));
        shadow = step(_e161, _e173.x);
    } else {
        if override_type_3_1 {
            let _e176 = currentDepth;
            let _e177 = sc;
            let _e178 = _e177.xy;
            let _e179 = layer;
            let _e182 = vec3<f32>(_e178.x, _e178.y, _e179);
            let _e188 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e182.x, _e182.y), i32(_e182.z));
            let _e191 = shadow;
            shadow = (_e191 + step(_e176, _e188.x));
            let _e193 = currentDepth;
            let _e194 = sc;
            let _e197 = texelSize[0u];
            let _e199 = (_e194.xy + vec2<f32>(_e197, 0f));
            let _e200 = layer;
            let _e203 = vec3<f32>(_e199.x, _e199.y, _e200);
            let _e209 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e203.x, _e203.y), i32(_e203.z));
            let _e212 = shadow;
            shadow = (_e212 + step(_e193, _e209.x));
            let _e214 = currentDepth;
            let _e215 = sc;
            let _e218 = texelSize[0u];
            let _e220 = (_e215.xy - vec2<f32>(_e218, 0f));
            let _e221 = layer;
            let _e224 = vec3<f32>(_e220.x, _e220.y, _e221);
            let _e230 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e224.x, _e224.y), i32(_e224.z));
            let _e233 = shadow;
            shadow = (_e233 + step(_e214, _e230.x));
            let _e235 = currentDepth;
            let _e236 = sc;
            let _e239 = texelSize[1u];
            let _e241 = (_e236.xy + vec2<f32>(0f, _e239));
            let _e242 = layer;
            let _e245 = vec3<f32>(_e241.x, _e241.y, _e242);
            let _e251 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e245.x, _e245.y), i32(_e245.z));
            let _e254 = shadow;
            shadow = (_e254 + step(_e235, _e251.x));
            let _e256 = currentDepth;
            let _e257 = sc;
            let _e260 = texelSize[1u];
            let _e262 = (_e257.xy - vec2<f32>(0f, _e260));
            let _e263 = layer;
            let _e266 = vec3<f32>(_e262.x, _e262.y, _e263);
            let _e272 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e266.x, _e266.y), i32(_e266.z));
            let _e275 = shadow;
            shadow = (_e275 + step(_e256, _e272.x));
            let _e277 = shadow;
            shadow = (_e277 / 5f);
        } else {
            x = -1i;
            loop {
                let _e279 = x;
                if (_e279 <= 1i) {
                    y = -1i;
                    loop {
                        let _e281 = y;
                        if (_e281 <= 1i) {
                            let _e283 = currentDepth;
                            let _e284 = sc;
                            let _e286 = x;
                            let _e288 = y;
                            let _e291 = texelSize;
                            let _e293 = (_e284.xy + (vec2<f32>(f32(_e286), f32(_e288)) * _e291));
                            let _e294 = layer;
                            let _e297 = vec3<f32>(_e293.x, _e293.y, _e294);
                            let _e303 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e297.x, _e297.y), i32(_e297.z));
                            let _e306 = shadow;
                            shadow = (_e306 + step(_e283, _e303.x));
                            continue;
                        } else {
                            break;
                        }
                        continuing {
                            let _e308 = y;
                            y = (_e308 + 1i);
                        }
                    }
                    continue;
                } else {
                    break;
                }
                continuing {
                    let _e310 = x;
                    x = (_e310 + 1i);
                }
            }
            let _e312 = shadow;
            shadow = (_e312 / 9f);
        }
    }
    let _e314 = shadow;
    return _e314;
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

    let _e105 = unnamed.cascadeSplits;
    let _e106 = (*viewDepth_1);
    cmp = step(_e105, vec4(_e106));
    let _e110 = cmp[0u];
    let _e112 = cmp[1u];
    let _e115 = cmp[2u];
    let _e118 = cmp[3u];
    cascade = min(i32((((_e110 + _e112) + _e115) + _e118)), 3i);
    let _e122 = cascade;
    (*outCascade) = _e122;
    let _e123 = cascade;
    if (_e123 == 0i) {
        local = 0f;
    } else {
        let _e125 = cascade;
        let _e130 = unnamed.cascadeSplits[max((_e125 - 1i), 0i)];
        local = _e130;
    }
    let _e131 = local;
    prevSplit = _e131;
    let _e132 = cascade;
    let _e135 = unnamed.cascadeSplits[_e132];
    farSplit = _e135;
    let _e136 = farSplit;
    let _e137 = prevSplit;
    blendRange = max((0.1f * (_e136 - _e137)), 1f);
    let _e141 = farSplit;
    let _e142 = (*viewDepth_1);
    let _e144 = blendRange;
    blendT = clamp(((_e141 - _e142) / _e144), 0f, 1f);
    let _e147 = cascade;
    param_5 = _e147;
    let _e148 = (*worldPos_1);
    param_6 = _e148;
    let _e149 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param_5), (&param_6));
    s0_ = _e149;
    let _e150 = cascade;
    param_7 = min((_e150 + 1i), 3i);
    let _e153 = (*worldPos_1);
    param_8 = _e153;
    let _e154 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param_7), (&param_8));
    s1_ = _e154;
    let _e155 = s1_;
    let _e156 = s0_;
    let _e157 = blendT;
    return mix(_e155, _e156, _e157);
}

fn wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b(rgb: ptr<function, vec3<f32>>, lm_uv: ptr<function, vec2<f32>>) -> vec3<f32> {
    var sun_mask: f32;
    var shadow_1: f32;
    var ignoredCascade: i32;
    var param_9: vec3<f32>;
    var param_10: f32;
    var param_11: i32;

    let _e99 = unnamed.packed_indices[1i][3u];
    let _e105 = unnamed.packed_indices[1i][3u];
    let _e110 = (*lm_uv);
    let _e111 = textureSample(wired_bindless_images[(_e99 & 4095u)], wired_bindless_samplers[((_e105 >> bitcast<u32>(12i)) & 255u)], _e110);
    sun_mask = _e111.x;
    let _e113 = sun_mask;
    if (_e113 > 0.001f) {
        let _e115 = shadowData_1;
        param_9 = _e115.xyz;
        let _e118 = shadowData_1[3u];
        param_10 = _e118;
        let _e119 = sampleShadow_u0028_vf3_u003b_f1_u003b_i1_u003b((&param_9), (&param_10), (&param_11));
        let _e120 = param_11;
        ignoredCascade = _e120;
        shadow_1 = _e119;
        let _e121 = shadow_1;
        let _e122 = sun_mask;
        let _e124 = (*rgb);
        (*rgb) = (_e124 * mix(1f, _e121, _e122));
    }
    let _e126 = (*rgb);
    return _e126;
}

fn wired_apply_sun_shadow_operand_u0028_vf3_u003b(rgb_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var param_12: vec3<f32>;
    var param_13: vec2<f32>;
    var param_14: vec3<f32>;
    var param_15: vec2<f32>;

    if override_type_3_2 {
        let _e93 = (*rgb_1);
        param_12 = _e93;
        let _e94 = frag_tex_coord0_1;
        param_13 = _e94;
        let _e95 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_12), (&param_13));
        return _e95;
    }
    if override_type_3_3 {
        let _e96 = (*rgb_1);
        param_14 = _e96;
        let _e97 = frag_tex_coord1_1;
        param_15 = _e97;
        let _e98 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_14), (&param_15));
        return _e98;
    }
    let _e99 = (*rgb_1);
    return _e99;
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb_2: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e91 = (*rgb_2);
    let _e94 = unnamed.worldLightParams[0u];
    boosted = (_e91 * _e94);
    let _e97 = boosted[0u];
    let _e99 = boosted[1u];
    let _e101 = boosted[2u];
    peak = max(_e97, max(_e99, _e101));
    let _e104 = peak;
    if (_e104 > 1f) {
        let _e106 = peak;
        let _e107 = boosted;
        boosted = (_e107 / vec3(_e106));
    }
    let _e110 = boosted;
    return _e110;
}

fn sRGBToLinear_u0028_vf3_u003b(c_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e92 = (*c_1);
    (*c_1) = max(_e92, vec3<f32>(0f, 0f, 0f));
    let _e94 = (*c_1);
    cutoff = (_e94 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e96 = (*c_1);
    lo = (_e96 / vec3(12.92f));
    let _e99 = (*c_1);
    hi = pow(((_e99 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e104 = hi;
    let _e105 = lo;
    let _e106 = cutoff;
    return mix(_e104, _e105, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e106));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_2: vec4<f32>;
    var param_16: vec3<f32>;
    var param_17: vec3<f32>;

    let _e94 = (*role);
    let _e96 = (*role);
    let _e101 = unnamed.packed_indices[(_e94 / 4u)][(_e96 % 4u)];
    let _e104 = (*role);
    let _e106 = (*role);
    let _e111 = unnamed.packed_indices[(_e104 / 4u)][(_e106 % 4u)];
    let _e116 = (*uv);
    let _e117 = textureSample(wired_bindless_images[(_e101 & 4095u)], wired_bindless_samplers[((_e111 >> bitcast<u32>(12i)) & 255u)], _e116);
    c_2 = _e117;
    let _e118 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e118))) == 0i) {
        let _e123 = c_2;
        param_16 = _e123.xyz;
        let _e125 = sRGBToLinear_u0028_vf3_u003b((&param_16));
        c_2[0u] = _e125.x;
        c_2[1u] = _e125.y;
        c_2[2u] = _e125.z;
    }
    let _e132 = (*slot);
    if (lightmap_slot == (_e132 + 1i)) {
        let _e135 = c_2;
        param_17 = _e135.xyz;
        let _e137 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_17));
        c_2[0u] = _e137.x;
        c_2[1u] = _e137.y;
        c_2[2u] = _e137.z;
    }
    let _e144 = c_2;
    return _e144;
}

fn main_1() {
    var sampledColor0_: vec4<f32>;
    var param_18: u32;
    var param_19: vec2<f32>;
    var param_20: i32;
    var sampledColor1_: vec4<f32>;
    var param_21: u32;
    var param_22: vec2<f32>;
    var param_23: i32;
    var frag_color: vec4<f32>;
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
    var param_26: vec4<f32>;

    param_18 = 0u;
    let _e109 = frag_tex_coord0_1;
    param_19 = _e109;
    param_20 = 0i;
    let _e110 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_18), (&param_19), (&param_20));
    sampledColor0_ = _e110;
    param_21 = 1u;
    let _e111 = frag_tex_coord1_1;
    param_22 = _e111;
    param_23 = 1i;
    let _e112 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_21), (&param_22), (&param_23));
    sampledColor1_ = _e112;
    frag_color[0u] = identity_color;
    frag_color[1u] = identity_color;
    frag_color[2u] = identity_color;
    frag_color[3u] = identity_alpha;
    let _e117 = sampledColor0_;
    let _e118 = frag_color;
    color0_ = (_e117 * _e118);
    if override_type_3_4 {
        let _e120 = sampledColor1_;
        color1_ = _e120;
        let _e121 = color0_;
        let _e123 = color1_;
        let _e125 = (_e121.xyz + _e123.xyz);
        let _e127 = color0_[3u];
        let _e129 = color1_[3u];
        base = vec4<f32>(_e125.x, _e125.y, _e125.z, (_e127 * _e129));
    } else {
        if override_type_3_5 {
            let _e135 = sampledColor1_;
            let _e136 = frag_color;
            color1_1 = (_e135 * _e136);
            let _e138 = color0_;
            let _e140 = color1_1;
            let _e142 = (_e138.xyz + _e140.xyz);
            let _e144 = color0_[3u];
            let _e146 = color1_1[3u];
            base = vec4<f32>(_e142.x, _e142.y, _e142.z, (_e144 * _e146));
        } else {
            let _e152 = sampledColor1_;
            let _e153 = frag_color;
            color1_2 = (_e152 * _e153);
            let _e155 = color0_;
            let _e157 = color1_2;
            let _e159 = (_e155.xyz * _e157.xyz);
            base[0u] = _e159.x;
            base[1u] = _e159.y;
            base[2u] = _e159.z;
            let _e167 = color0_[3u];
            let _e169 = color1_2[3u];
            base[3u] = (_e167 * _e169);
        }
    }
    let _e172 = base;
    param_24 = _e172.xyz;
    let _e174 = wired_apply_sun_shadow_operand_u0028_vf3_u003b((&param_24));
    base[0u] = _e174.x;
    base[1u] = _e174.y;
    base[2u] = _e174.z;
    if override_type_3_6 {
        let _e183 = unnamed.worldLightParams[1u];
        wetness = clamp(_e183, 0f, 1f);
        let _e187 = unnamed.worldLightParams[2u];
        frost = clamp(_e187, 0f, 1f);
        let _e189 = base;
        luminance = dot(_e189.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e192 = wetness;
        let _e194 = base;
        let _e196 = (_e194.xyz * mix(1f, 0.82f, _e192));
        base[0u] = _e196.x;
        base[1u] = _e196.y;
        base[2u] = _e196.z;
        let _e203 = base;
        let _e205 = luminance;
        let _e207 = luminance;
        let _e209 = luminance;
        let _e211 = frost;
        let _e214 = mix(_e203.xyz, vec3<f32>((_e205 * 0.88f), (_e207 * 0.94f), _e209), vec3((_e211 * 0.55f)));
        base[0u] = _e214.x;
        base[1u] = _e214.y;
        base[2u] = _e214.z;
    }
    let _e223 = unnamed.materialFactors[0u];
    if (_e223 >= 0f) {
        let _e226 = unnamed.materialBaseColor;
        let _e227 = base;
        base = (_e227 * _e226);
    }
    let _e229 = color0_;
    let _e232 = unnamed.emissionRadiance;
    let _e235 = base;
    let _e237 = (_e235.xyz + (_e229.xyz * _e232.xyz));
    base[0u] = _e237.x;
    base[1u] = _e237.y;
    base[2u] = _e237.z;
    let _e244 = wired_advanced_fog_enabled_u0028_();
    if _e244 {
        let _e245 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e245;
        let _e246 = base;
        let _e249 = unnamed.advancedFogColorDensity;
        let _e251 = fogAmount;
        let _e253 = mix(_e246.xyz, _e249.xyz, vec3(_e251));
        base[0u] = _e253.x;
        base[1u] = _e253.y;
        base[2u] = _e253.z;
    }
    if override_type_3_7 {
        let _e261 = base[3u];
        if (_e261 == 0f) {
            discard;
        }
    } else {
        if override_type_3_8 {
            let _e263 = base;
            let _e265 = base;
            if (dot(_e263.xyz, _e265.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e269 = base;
    out_color = _e269;
    param_25 = 1f;
    let _e270 = base;
    param_26 = _e270;
    wiredTemporalWriteAux_u0028_f1_u003b_vf4_u003b((&param_25), (&param_26));
    return;
}

@fragment
fn main(@location(12) @interpolate(flat) temporalOutcome: u32, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @builtin(position) gl_FragCoord: vec4<f32>, @location(7) shadowData: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>) -> FragmentOutput {
    temporalOutcome_1 = temporalOutcome;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    gl_FragCoord_1 = gl_FragCoord;
    shadowData_1 = shadowData;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    main_1();
    let _e17 = out_temporal_velocity;
    let _e18 = out_temporal_validity;
    let _e19 = out_color;
    return FragmentOutput(_e17, _e18, _e19);
}
