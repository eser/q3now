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
var<private> frag_tex_coord2_1: vec2<f32>;
var<private> frag_color0In_1: vec4<f32>;
var<private> frag_color1In_1: vec4<f32>;
var<private> frag_color2In_1: vec4<f32>;
var<private> out_color: vec4<f32>;

fn wiredTemporalFinite2_u0028_vf2_u003b(value: ptr<function, vec2<f32>>) -> bool {
    var phi_98_: bool;

    let _e101 = (*value);
    let _e102 = (*value);
    let _e104 = all((_e101 == _e102));
    phi_98_ = _e104;
    if _e104 {
        let _e105 = (*value);
        phi_98_ = all((abs(_e105) <= vec2<f32>(0x1.fffffep+127f, 0x1.fffffep+127f)));
    }
    let _e110 = phi_98_;
    return _e110;
}

fn wiredTemporalFinite4_u0028_vf4_u003b(value_1: ptr<function, vec4<f32>>) -> bool {
    var phi_83_: bool;

    let _e101 = (*value_1);
    let _e102 = (*value_1);
    let _e104 = all((_e101 == _e102));
    phi_83_ = _e104;
    if _e104 {
        let _e105 = (*value_1);
        phi_83_ = all((abs(_e105) <= vec4<f32>(0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f)));
    }
    let _e110 = phi_83_;
    return _e110;
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
    let _e112 = temporalOutcome_1;
    let _e113 = (_e112 != 1u);
    phi_122_ = _e113;
    if !(_e113) {
        let _e115 = temporalCurrentClip_1;
        param = _e115;
        let _e116 = wiredTemporalFinite4_u0028_vf4_u003b((&param));
        phi_122_ = !(_e116);
    }
    let _e119 = phi_122_;
    phi_131_ = _e119;
    if !(_e119) {
        let _e121 = temporalPreviousClip_1;
        param_1 = _e121;
        let _e122 = wiredTemporalFinite4_u0028_vf4_u003b((&param_1));
        phi_131_ = !(_e122);
    }
    let _e125 = phi_131_;
    phi_141_ = _e125;
    if !(_e125) {
        let _e128 = temporalCurrentClip_1[3u];
        phi_141_ = (_e128 <= 0.000001f);
    }
    let _e131 = phi_141_;
    phi_148_ = _e131;
    if !(_e131) {
        let _e134 = temporalPreviousClip_1[3u];
        phi_148_ = (_e134 <= 0.000001f);
    }
    let _e137 = phi_148_;
    if _e137 {
        return;
    }
    let _e138 = temporalCurrentClip_1;
    let _e141 = temporalCurrentClip_1[3u];
    currentNdc = (_e138.xy / vec2(_e141));
    let _e144 = temporalPreviousClip_1;
    let _e147 = temporalPreviousClip_1[3u];
    previousNdc = (_e144.xy / vec2(_e147));
    let _e150 = currentNdc;
    param_2 = _e150;
    let _e151 = wiredTemporalFinite2_u0028_vf2_u003b((&param_2));
    let _e152 = !(_e151);
    phi_177_ = _e152;
    if !(_e152) {
        let _e154 = previousNdc;
        param_3 = _e154;
        let _e155 = wiredTemporalFinite2_u0028_vf2_u003b((&param_3));
        phi_177_ = !(_e155);
    }
    let _e158 = phi_177_;
    if _e158 {
        return;
    }
    let _e159 = currentNdc;
    currentUv = ((_e159 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e162 = previousNdc;
    previousUv = ((_e162 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e165 = currentUv;
    let _e166 = previousUv;
    velocity = (_e165 - _e166);
    let _e168 = velocity;
    param_4 = _e168;
    let _e169 = wiredTemporalFinite2_u0028_vf2_u003b((&param_4));
    if !(_e169) {
        return;
    }
    let _e171 = velocity;
    out_temporal_velocity = _e171;
    let _e172 = (*coverageConfidence);
    out_temporal_validity = vec4<f32>(clamp(_e172, 0f, 1f), 0f, 0f, 1f);
    return;
}

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e103 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e103 + 0.5f));
    let _e108 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e110 = fogType;
    let _e113 = fogType;
    return (((_e108 > 0.5f) && (_e110 >= 1i)) && (_e113 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e103 = wired_advanced_fog_enabled_u0028_();
    if !(_e103) {
        return 0f;
    }
    let _e106 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e106, 0.000001f));
    let _e111 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e111 + 0.5f));
    let _e114 = fogType_1;
    if (_e114 == 1i) {
        let _e118 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e118 <= 0f) {
            return 0f;
        }
        let _e120 = viewDepth;
        let _e123 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e120 / _e123), 0f, 1f);
    }
    let _e128 = unnamed.advancedFogColorDensity[3u];
    let _e130 = viewDepth;
    opticalDepth = (max(_e128, 0f) * _e130);
    let _e132 = fogType_1;
    if (_e132 == 2i) {
        let _e134 = opticalDepth;
        return clamp((1f - exp(-(_e134))), 0f, 1f);
    }
    let _e139 = opticalDepth;
    let _e140 = opticalDepth;
    return clamp((1f - exp(-((_e139 * _e140)))), 0f, 1f);
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

    let _e110 = (*c);
    let _e113 = unnamed.cascadeMVP[_e110];
    let _e114 = (*worldPos);
    sc4_ = (_e113 * vec4<f32>(_e114.x, _e114.y, _e114.z, 1f));
    let _e120 = sc4_;
    let _e123 = sc4_[3u];
    sc = (_e120.xyz / vec3(_e123));
    let _e126 = sc;
    let _e130 = ((_e126.xy * 0.5f) + vec2(0.5f));
    sc[0u] = _e130.x;
    sc[1u] = _e130.y;
    let _e136 = sc[0u];
    let _e137 = (_e136 < 0f);
    phi_473_ = _e137;
    if !(_e137) {
        let _e140 = sc[0u];
        phi_473_ = (_e140 > 1f);
    }
    let _e143 = phi_473_;
    phi_480_ = _e143;
    if !(_e143) {
        let _e146 = sc[1u];
        phi_480_ = (_e146 < 0f);
    }
    let _e149 = phi_480_;
    phi_487_ = _e149;
    if !(_e149) {
        let _e152 = sc[1u];
        phi_487_ = (_e152 > 1f);
    }
    let _e155 = phi_487_;
    phi_494_ = _e155;
    if !(_e155) {
        let _e158 = sc[2u];
        phi_494_ = (_e158 > 1f);
    }
    let _e161 = phi_494_;
    if _e161 {
        return 1f;
    }
    let _e162 = (*c);
    layer = f32(_e162);
    let _e164 = textureDimensions(shadowMap, 0i);
    texelSize = (vec2(1f) / vec2<f32>(vec2<i32>(_e164).xy));
    let _e171 = sc[2u];
    currentDepth = (_e171 - shadow_bias);
    shadow = 0f;
    if override_type_3_ {
        let _e173 = currentDepth;
        let _e174 = sc;
        let _e175 = _e174.xy;
        let _e176 = layer;
        let _e179 = vec3<f32>(_e175.x, _e175.y, _e176);
        let _e185 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e179.x, _e179.y), i32(_e179.z));
        shadow = step(_e173, _e185.x);
    } else {
        if override_type_3_1 {
            let _e188 = currentDepth;
            let _e189 = sc;
            let _e190 = _e189.xy;
            let _e191 = layer;
            let _e194 = vec3<f32>(_e190.x, _e190.y, _e191);
            let _e200 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e194.x, _e194.y), i32(_e194.z));
            let _e203 = shadow;
            shadow = (_e203 + step(_e188, _e200.x));
            let _e205 = currentDepth;
            let _e206 = sc;
            let _e209 = texelSize[0u];
            let _e211 = (_e206.xy + vec2<f32>(_e209, 0f));
            let _e212 = layer;
            let _e215 = vec3<f32>(_e211.x, _e211.y, _e212);
            let _e221 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e215.x, _e215.y), i32(_e215.z));
            let _e224 = shadow;
            shadow = (_e224 + step(_e205, _e221.x));
            let _e226 = currentDepth;
            let _e227 = sc;
            let _e230 = texelSize[0u];
            let _e232 = (_e227.xy - vec2<f32>(_e230, 0f));
            let _e233 = layer;
            let _e236 = vec3<f32>(_e232.x, _e232.y, _e233);
            let _e242 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e236.x, _e236.y), i32(_e236.z));
            let _e245 = shadow;
            shadow = (_e245 + step(_e226, _e242.x));
            let _e247 = currentDepth;
            let _e248 = sc;
            let _e251 = texelSize[1u];
            let _e253 = (_e248.xy + vec2<f32>(0f, _e251));
            let _e254 = layer;
            let _e257 = vec3<f32>(_e253.x, _e253.y, _e254);
            let _e263 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e257.x, _e257.y), i32(_e257.z));
            let _e266 = shadow;
            shadow = (_e266 + step(_e247, _e263.x));
            let _e268 = currentDepth;
            let _e269 = sc;
            let _e272 = texelSize[1u];
            let _e274 = (_e269.xy - vec2<f32>(0f, _e272));
            let _e275 = layer;
            let _e278 = vec3<f32>(_e274.x, _e274.y, _e275);
            let _e284 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e278.x, _e278.y), i32(_e278.z));
            let _e287 = shadow;
            shadow = (_e287 + step(_e268, _e284.x));
            let _e289 = shadow;
            shadow = (_e289 / 5f);
        } else {
            x = -1i;
            loop {
                let _e291 = x;
                if (_e291 <= 1i) {
                    y = -1i;
                    loop {
                        let _e293 = y;
                        if (_e293 <= 1i) {
                            let _e295 = currentDepth;
                            let _e296 = sc;
                            let _e298 = x;
                            let _e300 = y;
                            let _e303 = texelSize;
                            let _e305 = (_e296.xy + (vec2<f32>(f32(_e298), f32(_e300)) * _e303));
                            let _e306 = layer;
                            let _e309 = vec3<f32>(_e305.x, _e305.y, _e306);
                            let _e315 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e309.x, _e309.y), i32(_e309.z));
                            let _e318 = shadow;
                            shadow = (_e318 + step(_e295, _e315.x));
                            continue;
                        } else {
                            break;
                        }
                        continuing {
                            let _e320 = y;
                            y = (_e320 + 1i);
                        }
                    }
                    continue;
                } else {
                    break;
                }
                continuing {
                    let _e322 = x;
                    x = (_e322 + 1i);
                }
            }
            let _e324 = shadow;
            shadow = (_e324 / 9f);
        }
    }
    let _e326 = shadow;
    return _e326;
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

    let _e117 = unnamed.cascadeSplits;
    let _e118 = (*viewDepth_1);
    cmp = step(_e117, vec4(_e118));
    let _e122 = cmp[0u];
    let _e124 = cmp[1u];
    let _e127 = cmp[2u];
    let _e130 = cmp[3u];
    cascade = min(i32((((_e122 + _e124) + _e127) + _e130)), 3i);
    let _e134 = cascade;
    (*outCascade) = _e134;
    let _e135 = cascade;
    if (_e135 == 0i) {
        local = 0f;
    } else {
        let _e137 = cascade;
        let _e142 = unnamed.cascadeSplits[max((_e137 - 1i), 0i)];
        local = _e142;
    }
    let _e143 = local;
    prevSplit = _e143;
    let _e144 = cascade;
    let _e147 = unnamed.cascadeSplits[_e144];
    farSplit = _e147;
    let _e148 = farSplit;
    let _e149 = prevSplit;
    blendRange = max((0.1f * (_e148 - _e149)), 1f);
    let _e153 = farSplit;
    let _e154 = (*viewDepth_1);
    let _e156 = blendRange;
    blendT = clamp(((_e153 - _e154) / _e156), 0f, 1f);
    let _e159 = cascade;
    param_5 = _e159;
    let _e160 = (*worldPos_1);
    param_6 = _e160;
    let _e161 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param_5), (&param_6));
    s0_ = _e161;
    let _e162 = cascade;
    param_7 = min((_e162 + 1i), 3i);
    let _e165 = (*worldPos_1);
    param_8 = _e165;
    let _e166 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param_7), (&param_8));
    s1_ = _e166;
    let _e167 = s1_;
    let _e168 = s0_;
    let _e169 = blendT;
    return mix(_e167, _e168, _e169);
}

fn wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b(rgb: ptr<function, vec3<f32>>, lm_uv: ptr<function, vec2<f32>>) -> vec3<f32> {
    var sun_mask: f32;
    var shadow_1: f32;
    var ignoredCascade: i32;
    var param_9: vec3<f32>;
    var param_10: f32;
    var param_11: i32;

    let _e111 = unnamed.packed_indices[1i][3u];
    let _e117 = unnamed.packed_indices[1i][3u];
    let _e122 = (*lm_uv);
    let _e123 = textureSample(wired_bindless_images[(_e111 & 4095u)], wired_bindless_samplers[((_e117 >> bitcast<u32>(12i)) & 255u)], _e122);
    sun_mask = _e123.x;
    let _e125 = sun_mask;
    if (_e125 > 0.001f) {
        let _e127 = shadowData_1;
        param_9 = _e127.xyz;
        let _e130 = shadowData_1[3u];
        param_10 = _e130;
        let _e131 = sampleShadow_u0028_vf3_u003b_f1_u003b_i1_u003b((&param_9), (&param_10), (&param_11));
        let _e132 = param_11;
        ignoredCascade = _e132;
        shadow_1 = _e131;
        let _e133 = shadow_1;
        let _e134 = sun_mask;
        let _e136 = (*rgb);
        (*rgb) = (_e136 * mix(1f, _e133, _e134));
    }
    let _e138 = (*rgb);
    return _e138;
}

fn wired_apply_sun_shadow_operand_u0028_vf3_u003b(rgb_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var param_12: vec3<f32>;
    var param_13: vec2<f32>;
    var param_14: vec3<f32>;
    var param_15: vec2<f32>;
    var param_16: vec3<f32>;
    var param_17: vec2<f32>;

    if override_type_3_2 {
        let _e107 = (*rgb_1);
        param_12 = _e107;
        let _e108 = frag_tex_coord0_1;
        param_13 = _e108;
        let _e109 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_12), (&param_13));
        return _e109;
    }
    if override_type_3_3 {
        let _e110 = (*rgb_1);
        param_14 = _e110;
        let _e111 = frag_tex_coord1_1;
        param_15 = _e111;
        let _e112 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_14), (&param_15));
        return _e112;
    }
    if override_type_3_4 {
        let _e113 = (*rgb_1);
        param_16 = _e113;
        let _e114 = frag_tex_coord2_1;
        param_17 = _e114;
        let _e115 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_16), (&param_17));
        return _e115;
    }
    let _e116 = (*rgb_1);
    return _e116;
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb_2: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e103 = (*rgb_2);
    let _e106 = unnamed.worldLightParams[0u];
    boosted = (_e103 * _e106);
    let _e109 = boosted[0u];
    let _e111 = boosted[1u];
    let _e113 = boosted[2u];
    peak = max(_e109, max(_e111, _e113));
    let _e116 = peak;
    if (_e116 > 1f) {
        let _e118 = peak;
        let _e119 = boosted;
        boosted = (_e119 / vec3(_e118));
    }
    let _e122 = boosted;
    return _e122;
}

fn sRGBToLinear_u0028_vf3_u003b(c_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e104 = (*c_1);
    (*c_1) = max(_e104, vec3<f32>(0f, 0f, 0f));
    let _e106 = (*c_1);
    cutoff = (_e106 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e108 = (*c_1);
    lo = (_e108 / vec3(12.92f));
    let _e111 = (*c_1);
    hi = pow(((_e111 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e116 = hi;
    let _e117 = lo;
    let _e118 = cutoff;
    return mix(_e116, _e117, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e118));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_2: vec4<f32>;
    var param_18: vec3<f32>;
    var param_19: vec3<f32>;

    let _e106 = (*role);
    let _e108 = (*role);
    let _e113 = unnamed.packed_indices[(_e106 / 4u)][(_e108 % 4u)];
    let _e116 = (*role);
    let _e118 = (*role);
    let _e123 = unnamed.packed_indices[(_e116 / 4u)][(_e118 % 4u)];
    let _e128 = (*uv);
    let _e129 = textureSample(wired_bindless_images[(_e113 & 4095u)], wired_bindless_samplers[((_e123 >> bitcast<u32>(12i)) & 255u)], _e128);
    c_2 = _e129;
    let _e130 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e130))) == 0i) {
        let _e135 = c_2;
        param_18 = _e135.xyz;
        let _e137 = sRGBToLinear_u0028_vf3_u003b((&param_18));
        c_2[0u] = _e137.x;
        c_2[1u] = _e137.y;
        c_2[2u] = _e137.z;
    }
    let _e144 = (*slot);
    if (lightmap_slot == (_e144 + 1i)) {
        let _e147 = c_2;
        param_19 = _e147.xyz;
        let _e149 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_19));
        c_2[0u] = _e149.x;
        c_2[1u] = _e149.y;
        c_2[2u] = _e149.z;
    }
    let _e156 = c_2;
    return _e156;
}

fn main_1() {
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
    var param_34: vec4<f32>;

    let _e141 = frag_color0In_1;
    param_20 = _e141.xyz;
    let _e143 = sRGBToLinear_u0028_vf3_u003b((&param_20));
    let _e145 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e143.x, _e143.y, _e143.z, _e145);
    let _e150 = frag_color1In_1;
    param_21 = _e150.xyz;
    let _e152 = sRGBToLinear_u0028_vf3_u003b((&param_21));
    let _e154 = frag_color1In_1[3u];
    frag_color1_ = vec4<f32>(_e152.x, _e152.y, _e152.z, _e154);
    let _e159 = frag_color2In_1;
    param_22 = _e159.xyz;
    let _e161 = sRGBToLinear_u0028_vf3_u003b((&param_22));
    let _e163 = frag_color2In_1[3u];
    frag_color2_ = vec4<f32>(_e161.x, _e161.y, _e161.z, _e163);
    param_23 = 0u;
    let _e168 = frag_tex_coord0_1;
    param_24 = _e168;
    param_25 = 0i;
    let _e169 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_23), (&param_24), (&param_25));
    sampledColor0_ = _e169;
    param_26 = 1u;
    let _e170 = frag_tex_coord1_1;
    param_27 = _e170;
    param_28 = 1i;
    let _e171 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_26), (&param_27), (&param_28));
    sampledColor1_ = _e171;
    param_29 = 2u;
    let _e172 = frag_tex_coord2_1;
    param_30 = _e172;
    param_31 = 2i;
    let _e173 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_29), (&param_30), (&param_31));
    sampledColor2_ = _e173;
    let _e174 = sampledColor0_;
    let _e175 = frag_color0_;
    color0_ = (_e174 * _e175);
    if override_type_3_7 {
        let _e177 = sampledColor1_;
        let _e178 = frag_color1_;
        color1_ = (_e177 * _e178);
        let _e180 = sampledColor2_;
        let _e181 = frag_color2_;
        color2_ = (_e180 * _e181);
        let _e183 = color0_;
        let _e185 = color1_;
        let _e188 = color2_;
        let _e190 = ((_e183.xyz + _e185.xyz) + _e188.xyz);
        let _e192 = color0_[3u];
        let _e194 = color1_[3u];
        let _e197 = color2_[3u];
        base = vec4<f32>(_e190.x, _e190.y, _e190.z, ((_e192 * _e194) * _e197));
    } else {
        if override_type_3_8 {
            let _e203 = sampledColor1_;
            let _e204 = frag_color1_;
            color1_1 = (_e203 * _e204);
            let _e206 = sampledColor2_;
            let _e207 = frag_color2_;
            color2_1 = (_e206 * _e207);
            let _e210 = color0_[3u];
            let _e211 = color0_;
            color0_ = (_e211 * _e210);
            let _e214 = color1_1[3u];
            let _e215 = color1_1;
            color1_1 = (_e215 * _e214);
            let _e218 = color2_1[3u];
            let _e219 = color2_1;
            color2_1 = (_e219 * _e218);
            let _e221 = color0_;
            let _e223 = color1_1;
            let _e226 = color2_1;
            let _e228 = ((_e221.xyz + _e223.xyz) + _e226.xyz);
            let _e230 = color0_[3u];
            let _e232 = color1_1[3u];
            let _e235 = color2_1[3u];
            base = vec4<f32>(_e228.x, _e228.y, _e228.z, ((_e230 * _e232) * _e235));
        } else {
            if override_type_3_9 {
                let _e241 = sampledColor1_;
                let _e242 = frag_color1_;
                color1_2 = (_e241 * _e242);
                let _e244 = sampledColor2_;
                let _e245 = frag_color2_;
                color2_2 = (_e244 * _e245);
                let _e248 = color0_[3u];
                let _e250 = color0_;
                color0_ = (_e250 * (1f - _e248));
                let _e253 = color1_2[3u];
                let _e255 = color1_2;
                color1_2 = (_e255 * (1f - _e253));
                let _e258 = color2_2[3u];
                let _e260 = color2_2;
                color2_2 = (_e260 * (1f - _e258));
                let _e262 = color0_;
                let _e264 = color1_2;
                let _e267 = color2_2;
                let _e269 = ((_e262.xyz + _e264.xyz) + _e267.xyz);
                let _e271 = color0_[3u];
                let _e273 = color1_2[3u];
                let _e276 = color2_2[3u];
                base = vec4<f32>(_e269.x, _e269.y, _e269.z, ((_e271 * _e273) * _e276));
            } else {
                if override_type_3_10 {
                    let _e282 = sampledColor1_;
                    let _e283 = frag_color1_;
                    color1_3 = (_e282 * _e283);
                    let _e285 = sampledColor2_;
                    let _e286 = frag_color2_;
                    color2_3 = (_e285 * _e286);
                    let _e288 = color0_;
                    let _e289 = color1_3;
                    let _e291 = color1_3[3u];
                    let _e294 = color2_3;
                    let _e296 = color2_3[3u];
                    base = mix(mix(_e288, _e289, vec4(_e291)), _e294, vec4(_e296));
                } else {
                    if override_type_3_11 {
                        let _e299 = sampledColor1_;
                        let _e300 = frag_color1_;
                        color1_4 = (_e299 * _e300);
                        let _e302 = sampledColor2_;
                        let _e303 = frag_color2_;
                        color2_4 = (_e302 * _e303);
                        let _e305 = color2_4;
                        let _e306 = color1_4;
                        let _e307 = color0_;
                        let _e309 = color1_4[3u];
                        let _e313 = color2_4[3u];
                        base = mix(_e305, mix(_e306, _e307, vec4(_e309)), vec4(_e313));
                    } else {
                        if override_type_3_12 {
                            let _e316 = sampledColor1_;
                            let _e317 = frag_color1_;
                            color1_5 = (_e316 * _e317);
                            let _e319 = sampledColor2_;
                            let _e320 = frag_color2_;
                            color2_5 = (_e319 * _e320);
                            let _e322 = color2_5;
                            let _e324 = color2_5[3u];
                            let _e327 = color1_5;
                            let _e329 = color1_5[3u];
                            let _e333 = color0_;
                            base = (((_e322 + vec4(_e324)) * (_e327 + vec4(_e329))) * _e333);
                        } else {
                            let _e335 = sampledColor1_;
                            let _e336 = frag_color1_;
                            color1_6 = (_e335 * _e336);
                            let _e338 = sampledColor2_;
                            let _e339 = frag_color2_;
                            color2_6 = (_e338 * _e339);
                            let _e341 = color0_;
                            let _e343 = color1_6;
                            let _e346 = color2_6;
                            let _e348 = ((_e341.xyz * _e343.xyz) * _e346.xyz);
                            base[0u] = _e348.x;
                            base[1u] = _e348.y;
                            base[2u] = _e348.z;
                            let _e356 = color0_[3u];
                            let _e358 = color1_6[3u];
                            let _e361 = color2_6[3u];
                            base[3u] = ((_e356 * _e358) * _e361);
                        }
                    }
                }
            }
        }
    }
    let _e364 = base;
    param_32 = _e364.xyz;
    let _e366 = wired_apply_sun_shadow_operand_u0028_vf3_u003b((&param_32));
    base[0u] = _e366.x;
    base[1u] = _e366.y;
    base[2u] = _e366.z;
    if override_type_3_13 {
        let _e375 = unnamed.worldLightParams[1u];
        wetness = clamp(_e375, 0f, 1f);
        let _e379 = unnamed.worldLightParams[2u];
        frost = clamp(_e379, 0f, 1f);
        let _e381 = base;
        luminance = dot(_e381.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e384 = wetness;
        let _e386 = base;
        let _e388 = (_e386.xyz * mix(1f, 0.82f, _e384));
        base[0u] = _e388.x;
        base[1u] = _e388.y;
        base[2u] = _e388.z;
        let _e395 = base;
        let _e397 = luminance;
        let _e399 = luminance;
        let _e401 = luminance;
        let _e403 = frost;
        let _e406 = mix(_e395.xyz, vec3<f32>((_e397 * 0.88f), (_e399 * 0.94f), _e401), vec3((_e403 * 0.55f)));
        base[0u] = _e406.x;
        base[1u] = _e406.y;
        base[2u] = _e406.z;
    }
    let _e415 = unnamed.materialFactors[0u];
    if (_e415 >= 0f) {
        let _e418 = unnamed.materialBaseColor;
        let _e419 = base;
        base = (_e419 * _e418);
    }
    let _e421 = color0_;
    let _e424 = unnamed.emissionRadiance;
    let _e427 = base;
    let _e429 = (_e427.xyz + (_e421.xyz * _e424.xyz));
    base[0u] = _e429.x;
    base[1u] = _e429.y;
    base[2u] = _e429.z;
    let _e436 = wired_advanced_fog_enabled_u0028_();
    if _e436 {
        let _e437 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e437;
        let _e438 = base;
        let _e441 = unnamed.advancedFogColorDensity;
        let _e443 = fogAmount;
        let _e445 = mix(_e438.xyz, _e441.xyz, vec3(_e443));
        base[0u] = _e445.x;
        base[1u] = _e445.y;
        base[2u] = _e445.z;
    }
    if override_type_3_14 {
        let _e453 = base[3u];
        if (_e453 == 0f) {
            discard;
        }
    } else {
        if override_type_3_15 {
            let _e455 = base;
            let _e457 = base;
            if (dot(_e455.xyz, _e457.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e461 = base;
    out_color = _e461;
    param_33 = 1f;
    let _e462 = base;
    param_34 = _e462;
    wiredTemporalWriteAux_u0028_f1_u003b_vf4_u003b((&param_33), (&param_34));
    return;
}

@fragment
fn main(@location(12) @interpolate(flat) temporalOutcome: u32, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @builtin(position) gl_FragCoord: vec4<f32>, @location(7) shadowData: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(3) frag_tex_coord2_: vec2<f32>, @location(0) frag_color0In: vec4<f32>, @location(5) frag_color1In: vec4<f32>, @location(6) frag_color2In: vec4<f32>) -> FragmentOutput {
    temporalOutcome_1 = temporalOutcome;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    gl_FragCoord_1 = gl_FragCoord;
    shadowData_1 = shadowData;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    frag_tex_coord2_1 = frag_tex_coord2_;
    frag_color0In_1 = frag_color0In;
    frag_color1In_1 = frag_color1In;
    frag_color2In_1 = frag_color2In;
    main_1();
    let _e25 = out_temporal_velocity;
    let _e26 = out_temporal_validity;
    let _e27 = out_color;
    return FragmentOutput(_e25, _e26, _e27);
}
