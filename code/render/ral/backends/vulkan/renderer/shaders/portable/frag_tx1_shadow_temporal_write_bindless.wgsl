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
var<private> frag_color0In_1: vec4<f32>;
var<private> out_color: vec4<f32>;

fn wiredTemporalFinite2_u0028_vf2_u003b(value: ptr<function, vec2<f32>>) -> bool {
    var phi_98_: bool;

    let _e88 = (*value);
    let _e89 = (*value);
    let _e91 = all((_e88 == _e89));
    phi_98_ = _e91;
    if _e91 {
        let _e92 = (*value);
        phi_98_ = all((abs(_e92) <= vec2<f32>(0x1.fffffep+127f, 0x1.fffffep+127f)));
    }
    let _e97 = phi_98_;
    return _e97;
}

fn wiredTemporalFinite4_u0028_vf4_u003b(value_1: ptr<function, vec4<f32>>) -> bool {
    var phi_83_: bool;

    let _e88 = (*value_1);
    let _e89 = (*value_1);
    let _e91 = all((_e88 == _e89));
    phi_83_ = _e91;
    if _e91 {
        let _e92 = (*value_1);
        phi_83_ = all((abs(_e92) <= vec4<f32>(0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f)));
    }
    let _e97 = phi_83_;
    return _e97;
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
    let _e99 = temporalOutcome_1;
    let _e100 = (_e99 != 1u);
    phi_122_ = _e100;
    if !(_e100) {
        let _e102 = temporalCurrentClip_1;
        param = _e102;
        let _e103 = wiredTemporalFinite4_u0028_vf4_u003b((&param));
        phi_122_ = !(_e103);
    }
    let _e106 = phi_122_;
    phi_131_ = _e106;
    if !(_e106) {
        let _e108 = temporalPreviousClip_1;
        param_1 = _e108;
        let _e109 = wiredTemporalFinite4_u0028_vf4_u003b((&param_1));
        phi_131_ = !(_e109);
    }
    let _e112 = phi_131_;
    phi_141_ = _e112;
    if !(_e112) {
        let _e115 = temporalCurrentClip_1[3u];
        phi_141_ = (_e115 <= 0.000001f);
    }
    let _e118 = phi_141_;
    phi_148_ = _e118;
    if !(_e118) {
        let _e121 = temporalPreviousClip_1[3u];
        phi_148_ = (_e121 <= 0.000001f);
    }
    let _e124 = phi_148_;
    if _e124 {
        return;
    }
    let _e125 = temporalCurrentClip_1;
    let _e128 = temporalCurrentClip_1[3u];
    currentNdc = (_e125.xy / vec2(_e128));
    let _e131 = temporalPreviousClip_1;
    let _e134 = temporalPreviousClip_1[3u];
    previousNdc = (_e131.xy / vec2(_e134));
    let _e137 = currentNdc;
    param_2 = _e137;
    let _e138 = wiredTemporalFinite2_u0028_vf2_u003b((&param_2));
    let _e139 = !(_e138);
    phi_177_ = _e139;
    if !(_e139) {
        let _e141 = previousNdc;
        param_3 = _e141;
        let _e142 = wiredTemporalFinite2_u0028_vf2_u003b((&param_3));
        phi_177_ = !(_e142);
    }
    let _e145 = phi_177_;
    if _e145 {
        return;
    }
    let _e146 = currentNdc;
    currentUv = ((_e146 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e149 = previousNdc;
    previousUv = ((_e149 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e152 = currentUv;
    let _e153 = previousUv;
    velocity = (_e152 - _e153);
    let _e155 = velocity;
    param_4 = _e155;
    let _e156 = wiredTemporalFinite2_u0028_vf2_u003b((&param_4));
    if !(_e156) {
        return;
    }
    let _e158 = velocity;
    out_temporal_velocity = _e158;
    let _e159 = (*coverageConfidence);
    out_temporal_validity = vec4<f32>(clamp(_e159, 0f, 1f), 0f, 0f, 1f);
    return;
}

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
    var phi_473_: bool;
    var phi_480_: bool;
    var phi_487_: bool;
    var phi_494_: bool;

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
    phi_473_ = _e124;
    if !(_e124) {
        let _e127 = sc[0u];
        phi_473_ = (_e127 > 1f);
    }
    let _e130 = phi_473_;
    phi_480_ = _e130;
    if !(_e130) {
        let _e133 = sc[1u];
        phi_480_ = (_e133 < 0f);
    }
    let _e136 = phi_480_;
    phi_487_ = _e136;
    if !(_e136) {
        let _e139 = sc[1u];
        phi_487_ = (_e139 > 1f);
    }
    let _e142 = phi_487_;
    phi_494_ = _e142;
    if !(_e142) {
        let _e145 = sc[2u];
        phi_494_ = (_e145 > 1f);
    }
    let _e148 = phi_494_;
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
    var param_5: i32;
    var param_6: vec3<f32>;
    var s1_: f32;
    var param_7: i32;
    var param_8: vec3<f32>;

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
    param_5 = _e146;
    let _e147 = (*worldPos_1);
    param_6 = _e147;
    let _e148 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param_5), (&param_6));
    s0_ = _e148;
    let _e149 = cascade;
    param_7 = min((_e149 + 1i), 3i);
    let _e152 = (*worldPos_1);
    param_8 = _e152;
    let _e153 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param_7), (&param_8));
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
    var param_9: vec3<f32>;
    var param_10: f32;
    var param_11: i32;

    let _e98 = unnamed.packed_indices[1i][3u];
    let _e104 = unnamed.packed_indices[1i][3u];
    let _e109 = (*lm_uv);
    let _e110 = textureSample(wired_bindless_images[(_e98 & 4095u)], wired_bindless_samplers[((_e104 >> bitcast<u32>(12i)) & 255u)], _e109);
    sun_mask = _e110.x;
    let _e112 = sun_mask;
    if (_e112 > 0.001f) {
        let _e114 = shadowData_1;
        param_9 = _e114.xyz;
        let _e117 = shadowData_1[3u];
        param_10 = _e117;
        let _e118 = sampleShadow_u0028_vf3_u003b_f1_u003b_i1_u003b((&param_9), (&param_10), (&param_11));
        let _e119 = param_11;
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
    var param_12: vec3<f32>;
    var param_13: vec2<f32>;
    var param_14: vec3<f32>;
    var param_15: vec2<f32>;

    if override_type_3_2 {
        let _e92 = (*rgb_1);
        param_12 = _e92;
        let _e93 = frag_tex_coord0_1;
        param_13 = _e93;
        let _e94 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_12), (&param_13));
        return _e94;
    }
    if override_type_3_3 {
        let _e95 = (*rgb_1);
        param_14 = _e95;
        let _e96 = frag_tex_coord1_1;
        param_15 = _e96;
        let _e97 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_14), (&param_15));
        return _e97;
    }
    let _e98 = (*rgb_1);
    return _e98;
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb_2: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e90 = (*rgb_2);
    let _e93 = unnamed.worldLightParams[0u];
    boosted = (_e90 * _e93);
    let _e96 = boosted[0u];
    let _e98 = boosted[1u];
    let _e100 = boosted[2u];
    peak = max(_e96, max(_e98, _e100));
    let _e103 = peak;
    if (_e103 > 1f) {
        let _e105 = peak;
        let _e106 = boosted;
        boosted = (_e106 / vec3(_e105));
    }
    let _e109 = boosted;
    return _e109;
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
    var param_16: vec3<f32>;
    var param_17: vec3<f32>;

    let _e93 = (*role);
    let _e95 = (*role);
    let _e100 = unnamed.packed_indices[(_e93 / 4u)][(_e95 % 4u)];
    let _e103 = (*role);
    let _e105 = (*role);
    let _e110 = unnamed.packed_indices[(_e103 / 4u)][(_e105 % 4u)];
    let _e115 = (*uv);
    let _e116 = textureSample(wired_bindless_images[(_e100 & 4095u)], wired_bindless_samplers[((_e110 >> bitcast<u32>(12i)) & 255u)], _e115);
    c_2 = _e116;
    let _e117 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e117))) == 0i) {
        let _e122 = c_2;
        param_16 = _e122.xyz;
        let _e124 = sRGBToLinear_u0028_vf3_u003b((&param_16));
        c_2[0u] = _e124.x;
        c_2[1u] = _e124.y;
        c_2[2u] = _e124.z;
    }
    let _e131 = (*slot);
    if (lightmap_slot == (_e131 + 1i)) {
        let _e134 = c_2;
        param_17 = _e134.xyz;
        let _e136 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_17));
        c_2[0u] = _e136.x;
        c_2[1u] = _e136.y;
        c_2[2u] = _e136.z;
    }
    let _e143 = c_2;
    return _e143;
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
    var param_27: vec4<f32>;

    let _e109 = frag_color0In_1;
    param_18 = _e109.xyz;
    let _e111 = sRGBToLinear_u0028_vf3_u003b((&param_18));
    let _e113 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e111.x, _e111.y, _e111.z, _e113);
    param_19 = 0u;
    let _e118 = frag_tex_coord0_1;
    param_20 = _e118;
    param_21 = 0i;
    let _e119 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_19), (&param_20), (&param_21));
    sampledColor0_ = _e119;
    param_22 = 1u;
    let _e120 = frag_tex_coord1_1;
    param_23 = _e120;
    param_24 = 1i;
    let _e121 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_22), (&param_23), (&param_24));
    sampledColor1_ = _e121;
    let _e122 = sampledColor0_;
    let _e123 = frag_color0_;
    color0_ = (_e122 * _e123);
    if override_type_3_4 {
        let _e125 = sampledColor1_;
        color1_ = _e125;
        let _e126 = color0_;
        let _e128 = color1_;
        let _e130 = (_e126.xyz + _e128.xyz);
        let _e132 = color0_[3u];
        let _e134 = color1_[3u];
        base = vec4<f32>(_e130.x, _e130.y, _e130.z, (_e132 * _e134));
    } else {
        if override_type_3_5 {
            let _e140 = sampledColor1_;
            let _e141 = frag_color0_;
            color1_1 = (_e140 * _e141);
            let _e143 = color0_;
            let _e145 = color1_1;
            let _e147 = (_e143.xyz + _e145.xyz);
            let _e149 = color0_[3u];
            let _e151 = color1_1[3u];
            base = vec4<f32>(_e147.x, _e147.y, _e147.z, (_e149 * _e151));
        } else {
            let _e157 = sampledColor1_;
            color1_2 = _e157;
            let _e158 = color0_;
            let _e160 = color1_2;
            let _e162 = (_e158.xyz * _e160.xyz);
            base[0u] = _e162.x;
            base[1u] = _e162.y;
            base[2u] = _e162.z;
            let _e170 = color0_[3u];
            let _e172 = color1_2[3u];
            base[3u] = (_e170 * _e172);
        }
    }
    let _e175 = base;
    param_25 = _e175.xyz;
    let _e177 = wired_apply_sun_shadow_operand_u0028_vf3_u003b((&param_25));
    base[0u] = _e177.x;
    base[1u] = _e177.y;
    base[2u] = _e177.z;
    if override_type_3_6 {
        let _e186 = unnamed.worldLightParams[1u];
        wetness = clamp(_e186, 0f, 1f);
        let _e190 = unnamed.worldLightParams[2u];
        frost = clamp(_e190, 0f, 1f);
        let _e192 = base;
        luminance = dot(_e192.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e195 = wetness;
        let _e197 = base;
        let _e199 = (_e197.xyz * mix(1f, 0.82f, _e195));
        base[0u] = _e199.x;
        base[1u] = _e199.y;
        base[2u] = _e199.z;
        let _e206 = base;
        let _e208 = luminance;
        let _e210 = luminance;
        let _e212 = luminance;
        let _e214 = frost;
        let _e217 = mix(_e206.xyz, vec3<f32>((_e208 * 0.88f), (_e210 * 0.94f), _e212), vec3((_e214 * 0.55f)));
        base[0u] = _e217.x;
        base[1u] = _e217.y;
        base[2u] = _e217.z;
    }
    let _e226 = unnamed.materialFactors[0u];
    if (_e226 >= 0f) {
        let _e229 = unnamed.materialBaseColor;
        let _e230 = base;
        base = (_e230 * _e229);
    }
    let _e232 = color0_;
    let _e235 = unnamed.emissionRadiance;
    let _e238 = base;
    let _e240 = (_e238.xyz + (_e232.xyz * _e235.xyz));
    base[0u] = _e240.x;
    base[1u] = _e240.y;
    base[2u] = _e240.z;
    let _e247 = wired_advanced_fog_enabled_u0028_();
    if _e247 {
        let _e248 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e248;
        let _e249 = base;
        let _e252 = unnamed.advancedFogColorDensity;
        let _e254 = fogAmount;
        let _e256 = mix(_e249.xyz, _e252.xyz, vec3(_e254));
        base[0u] = _e256.x;
        base[1u] = _e256.y;
        base[2u] = _e256.z;
    }
    if override_type_3_7 {
        let _e264 = base[3u];
        if (_e264 == 0f) {
            discard;
        }
    } else {
        if override_type_3_8 {
            let _e266 = base;
            let _e268 = base;
            if (dot(_e266.xyz, _e268.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e272 = base;
    out_color = _e272;
    param_26 = 1f;
    let _e273 = base;
    param_27 = _e273;
    wiredTemporalWriteAux_u0028_f1_u003b_vf4_u003b((&param_26), (&param_27));
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
