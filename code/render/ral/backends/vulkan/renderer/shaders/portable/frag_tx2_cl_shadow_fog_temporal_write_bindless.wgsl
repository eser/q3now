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
var<private> fog_tex_coord_1: vec2<f32>;
var<private> frag_color0In_1: vec4<f32>;
var<private> frag_color1In_1: vec4<f32>;
var<private> frag_color2In_1: vec4<f32>;
var<private> out_color: vec4<f32>;

fn wiredTemporalFinite2_u0028_vf2_u003b(value: ptr<function, vec2<f32>>) -> bool {
    var phi_98_: bool;

    let _e109 = (*value);
    let _e110 = (*value);
    let _e112 = all((_e109 == _e110));
    phi_98_ = _e112;
    if _e112 {
        let _e113 = (*value);
        phi_98_ = all((abs(_e113) <= vec2<f32>(0x1.fffffep+127f, 0x1.fffffep+127f)));
    }
    let _e118 = phi_98_;
    return _e118;
}

fn wiredTemporalFinite4_u0028_vf4_u003b(value_1: ptr<function, vec4<f32>>) -> bool {
    var phi_83_: bool;

    let _e109 = (*value_1);
    let _e110 = (*value_1);
    let _e112 = all((_e109 == _e110));
    phi_83_ = _e112;
    if _e112 {
        let _e113 = (*value_1);
        phi_83_ = all((abs(_e113) <= vec4<f32>(0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f)));
    }
    let _e118 = phi_83_;
    return _e118;
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
    let _e120 = temporalOutcome_1;
    let _e121 = (_e120 != 1u);
    phi_122_ = _e121;
    if !(_e121) {
        let _e123 = temporalCurrentClip_1;
        param = _e123;
        let _e124 = wiredTemporalFinite4_u0028_vf4_u003b((&param));
        phi_122_ = !(_e124);
    }
    let _e127 = phi_122_;
    phi_131_ = _e127;
    if !(_e127) {
        let _e129 = temporalPreviousClip_1;
        param_1 = _e129;
        let _e130 = wiredTemporalFinite4_u0028_vf4_u003b((&param_1));
        phi_131_ = !(_e130);
    }
    let _e133 = phi_131_;
    phi_141_ = _e133;
    if !(_e133) {
        let _e136 = temporalCurrentClip_1[3u];
        phi_141_ = (_e136 <= 0.000001f);
    }
    let _e139 = phi_141_;
    phi_148_ = _e139;
    if !(_e139) {
        let _e142 = temporalPreviousClip_1[3u];
        phi_148_ = (_e142 <= 0.000001f);
    }
    let _e145 = phi_148_;
    if _e145 {
        return;
    }
    let _e146 = temporalCurrentClip_1;
    let _e149 = temporalCurrentClip_1[3u];
    currentNdc = (_e146.xy / vec2(_e149));
    let _e152 = temporalPreviousClip_1;
    let _e155 = temporalPreviousClip_1[3u];
    previousNdc = (_e152.xy / vec2(_e155));
    let _e158 = currentNdc;
    param_2 = _e158;
    let _e159 = wiredTemporalFinite2_u0028_vf2_u003b((&param_2));
    let _e160 = !(_e159);
    phi_177_ = _e160;
    if !(_e160) {
        let _e162 = previousNdc;
        param_3 = _e162;
        let _e163 = wiredTemporalFinite2_u0028_vf2_u003b((&param_3));
        phi_177_ = !(_e163);
    }
    let _e166 = phi_177_;
    if _e166 {
        return;
    }
    let _e167 = currentNdc;
    currentUv = ((_e167 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e170 = previousNdc;
    previousUv = ((_e170 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e173 = currentUv;
    let _e174 = previousUv;
    velocity = (_e173 - _e174);
    let _e176 = velocity;
    param_4 = _e176;
    let _e177 = wiredTemporalFinite2_u0028_vf2_u003b((&param_4));
    if !(_e177) {
        return;
    }
    let _e179 = velocity;
    out_temporal_velocity = _e179;
    let _e180 = (*coverageConfidence);
    out_temporal_validity = vec4<f32>(clamp(_e180, 0f, 1f), 0f, 0f, 1f);
    return;
}

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e111 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e111 + 0.5f));
    let _e116 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e118 = fogType;
    let _e121 = fogType;
    return (((_e116 > 0.5f) && (_e118 >= 1i)) && (_e121 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e111 = wired_advanced_fog_enabled_u0028_();
    if !(_e111) {
        return 0f;
    }
    let _e114 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e114, 0.000001f));
    let _e119 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e119 + 0.5f));
    let _e122 = fogType_1;
    if (_e122 == 1i) {
        let _e126 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e126 <= 0f) {
            return 0f;
        }
        let _e128 = viewDepth;
        let _e131 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e128 / _e131), 0f, 1f);
    }
    let _e136 = unnamed.advancedFogColorDensity[3u];
    let _e138 = viewDepth;
    opticalDepth = (max(_e136, 0f) * _e138);
    let _e140 = fogType_1;
    if (_e140 == 2i) {
        let _e142 = opticalDepth;
        return clamp((1f - exp(-(_e142))), 0f, 1f);
    }
    let _e147 = opticalDepth;
    let _e148 = opticalDepth;
    return clamp((1f - exp(-((_e147 * _e148)))), 0f, 1f);
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

    let _e118 = (*c);
    let _e121 = unnamed.cascadeMVP[_e118];
    let _e122 = (*worldPos);
    sc4_ = (_e121 * vec4<f32>(_e122.x, _e122.y, _e122.z, 1f));
    let _e128 = sc4_;
    let _e131 = sc4_[3u];
    sc = (_e128.xyz / vec3(_e131));
    let _e134 = sc;
    let _e138 = ((_e134.xy * 0.5f) + vec2(0.5f));
    sc[0u] = _e138.x;
    sc[1u] = _e138.y;
    let _e144 = sc[0u];
    let _e145 = (_e144 < 0f);
    phi_473_ = _e145;
    if !(_e145) {
        let _e148 = sc[0u];
        phi_473_ = (_e148 > 1f);
    }
    let _e151 = phi_473_;
    phi_480_ = _e151;
    if !(_e151) {
        let _e154 = sc[1u];
        phi_480_ = (_e154 < 0f);
    }
    let _e157 = phi_480_;
    phi_487_ = _e157;
    if !(_e157) {
        let _e160 = sc[1u];
        phi_487_ = (_e160 > 1f);
    }
    let _e163 = phi_487_;
    phi_494_ = _e163;
    if !(_e163) {
        let _e166 = sc[2u];
        phi_494_ = (_e166 > 1f);
    }
    let _e169 = phi_494_;
    if _e169 {
        return 1f;
    }
    let _e170 = (*c);
    layer = f32(_e170);
    let _e172 = textureDimensions(shadowMap, 0i);
    texelSize = (vec2(1f) / vec2<f32>(vec2<i32>(_e172).xy));
    let _e179 = sc[2u];
    currentDepth = (_e179 - shadow_bias);
    shadow = 0f;
    if override_type_3_ {
        let _e181 = currentDepth;
        let _e182 = sc;
        let _e183 = _e182.xy;
        let _e184 = layer;
        let _e187 = vec3<f32>(_e183.x, _e183.y, _e184);
        let _e193 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e187.x, _e187.y), i32(_e187.z));
        shadow = step(_e181, _e193.x);
    } else {
        if override_type_3_1 {
            let _e196 = currentDepth;
            let _e197 = sc;
            let _e198 = _e197.xy;
            let _e199 = layer;
            let _e202 = vec3<f32>(_e198.x, _e198.y, _e199);
            let _e208 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e202.x, _e202.y), i32(_e202.z));
            let _e211 = shadow;
            shadow = (_e211 + step(_e196, _e208.x));
            let _e213 = currentDepth;
            let _e214 = sc;
            let _e217 = texelSize[0u];
            let _e219 = (_e214.xy + vec2<f32>(_e217, 0f));
            let _e220 = layer;
            let _e223 = vec3<f32>(_e219.x, _e219.y, _e220);
            let _e229 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e223.x, _e223.y), i32(_e223.z));
            let _e232 = shadow;
            shadow = (_e232 + step(_e213, _e229.x));
            let _e234 = currentDepth;
            let _e235 = sc;
            let _e238 = texelSize[0u];
            let _e240 = (_e235.xy - vec2<f32>(_e238, 0f));
            let _e241 = layer;
            let _e244 = vec3<f32>(_e240.x, _e240.y, _e241);
            let _e250 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e244.x, _e244.y), i32(_e244.z));
            let _e253 = shadow;
            shadow = (_e253 + step(_e234, _e250.x));
            let _e255 = currentDepth;
            let _e256 = sc;
            let _e259 = texelSize[1u];
            let _e261 = (_e256.xy + vec2<f32>(0f, _e259));
            let _e262 = layer;
            let _e265 = vec3<f32>(_e261.x, _e261.y, _e262);
            let _e271 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e265.x, _e265.y), i32(_e265.z));
            let _e274 = shadow;
            shadow = (_e274 + step(_e255, _e271.x));
            let _e276 = currentDepth;
            let _e277 = sc;
            let _e280 = texelSize[1u];
            let _e282 = (_e277.xy - vec2<f32>(0f, _e280));
            let _e283 = layer;
            let _e286 = vec3<f32>(_e282.x, _e282.y, _e283);
            let _e292 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e286.x, _e286.y), i32(_e286.z));
            let _e295 = shadow;
            shadow = (_e295 + step(_e276, _e292.x));
            let _e297 = shadow;
            shadow = (_e297 / 5f);
        } else {
            x = -1i;
            loop {
                let _e299 = x;
                if (_e299 <= 1i) {
                    y = -1i;
                    loop {
                        let _e301 = y;
                        if (_e301 <= 1i) {
                            let _e303 = currentDepth;
                            let _e304 = sc;
                            let _e306 = x;
                            let _e308 = y;
                            let _e311 = texelSize;
                            let _e313 = (_e304.xy + (vec2<f32>(f32(_e306), f32(_e308)) * _e311));
                            let _e314 = layer;
                            let _e317 = vec3<f32>(_e313.x, _e313.y, _e314);
                            let _e323 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e317.x, _e317.y), i32(_e317.z));
                            let _e326 = shadow;
                            shadow = (_e326 + step(_e303, _e323.x));
                            continue;
                        } else {
                            break;
                        }
                        continuing {
                            let _e328 = y;
                            y = (_e328 + 1i);
                        }
                    }
                    continue;
                } else {
                    break;
                }
                continuing {
                    let _e330 = x;
                    x = (_e330 + 1i);
                }
            }
            let _e332 = shadow;
            shadow = (_e332 / 9f);
        }
    }
    let _e334 = shadow;
    return _e334;
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

    let _e125 = unnamed.cascadeSplits;
    let _e126 = (*viewDepth_1);
    cmp = step(_e125, vec4(_e126));
    let _e130 = cmp[0u];
    let _e132 = cmp[1u];
    let _e135 = cmp[2u];
    let _e138 = cmp[3u];
    cascade = min(i32((((_e130 + _e132) + _e135) + _e138)), 3i);
    let _e142 = cascade;
    (*outCascade) = _e142;
    let _e143 = cascade;
    if (_e143 == 0i) {
        local = 0f;
    } else {
        let _e145 = cascade;
        let _e150 = unnamed.cascadeSplits[max((_e145 - 1i), 0i)];
        local = _e150;
    }
    let _e151 = local;
    prevSplit = _e151;
    let _e152 = cascade;
    let _e155 = unnamed.cascadeSplits[_e152];
    farSplit = _e155;
    let _e156 = farSplit;
    let _e157 = prevSplit;
    blendRange = max((0.1f * (_e156 - _e157)), 1f);
    let _e161 = farSplit;
    let _e162 = (*viewDepth_1);
    let _e164 = blendRange;
    blendT = clamp(((_e161 - _e162) / _e164), 0f, 1f);
    let _e167 = cascade;
    param_5 = _e167;
    let _e168 = (*worldPos_1);
    param_6 = _e168;
    let _e169 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param_5), (&param_6));
    s0_ = _e169;
    let _e170 = cascade;
    param_7 = min((_e170 + 1i), 3i);
    let _e173 = (*worldPos_1);
    param_8 = _e173;
    let _e174 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param_7), (&param_8));
    s1_ = _e174;
    let _e175 = s1_;
    let _e176 = s0_;
    let _e177 = blendT;
    return mix(_e175, _e176, _e177);
}

fn wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b(rgb: ptr<function, vec3<f32>>, lm_uv: ptr<function, vec2<f32>>) -> vec3<f32> {
    var sun_mask: f32;
    var shadow_1: f32;
    var ignoredCascade: i32;
    var param_9: vec3<f32>;
    var param_10: f32;
    var param_11: i32;

    let _e119 = unnamed.packed_indices[1i][3u];
    let _e125 = unnamed.packed_indices[1i][3u];
    let _e130 = (*lm_uv);
    let _e131 = textureSample(wired_bindless_images[(_e119 & 4095u)], wired_bindless_samplers[((_e125 >> bitcast<u32>(12i)) & 255u)], _e130);
    sun_mask = _e131.x;
    let _e133 = sun_mask;
    if (_e133 > 0.001f) {
        let _e135 = shadowData_1;
        param_9 = _e135.xyz;
        let _e138 = shadowData_1[3u];
        param_10 = _e138;
        let _e139 = sampleShadow_u0028_vf3_u003b_f1_u003b_i1_u003b((&param_9), (&param_10), (&param_11));
        let _e140 = param_11;
        ignoredCascade = _e140;
        shadow_1 = _e139;
        let _e141 = shadow_1;
        let _e142 = sun_mask;
        let _e144 = (*rgb);
        (*rgb) = (_e144 * mix(1f, _e141, _e142));
    }
    let _e146 = (*rgb);
    return _e146;
}

fn wired_apply_sun_shadow_operand_u0028_vf3_u003b(rgb_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var param_12: vec3<f32>;
    var param_13: vec2<f32>;
    var param_14: vec3<f32>;
    var param_15: vec2<f32>;
    var param_16: vec3<f32>;
    var param_17: vec2<f32>;

    if override_type_3_2 {
        let _e115 = (*rgb_1);
        param_12 = _e115;
        let _e116 = frag_tex_coord0_1;
        param_13 = _e116;
        let _e117 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_12), (&param_13));
        return _e117;
    }
    if override_type_3_3 {
        let _e118 = (*rgb_1);
        param_14 = _e118;
        let _e119 = frag_tex_coord1_1;
        param_15 = _e119;
        let _e120 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_14), (&param_15));
        return _e120;
    }
    if override_type_3_4 {
        let _e121 = (*rgb_1);
        param_16 = _e121;
        let _e122 = frag_tex_coord2_1;
        param_17 = _e122;
        let _e123 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_16), (&param_17));
        return _e123;
    }
    let _e124 = (*rgb_1);
    return _e124;
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb_2: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e111 = (*rgb_2);
    let _e114 = unnamed.worldLightParams[0u];
    boosted = (_e111 * _e114);
    let _e117 = boosted[0u];
    let _e119 = boosted[1u];
    let _e121 = boosted[2u];
    peak = max(_e117, max(_e119, _e121));
    let _e124 = peak;
    if (_e124 > 1f) {
        let _e126 = peak;
        let _e127 = boosted;
        boosted = (_e127 / vec3(_e126));
    }
    let _e130 = boosted;
    return _e130;
}

fn sRGBToLinear_u0028_vf3_u003b(c_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e112 = (*c_1);
    (*c_1) = max(_e112, vec3<f32>(0f, 0f, 0f));
    let _e114 = (*c_1);
    cutoff = (_e114 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e116 = (*c_1);
    lo = (_e116 / vec3(12.92f));
    let _e119 = (*c_1);
    hi = pow(((_e119 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e124 = hi;
    let _e125 = lo;
    let _e126 = cutoff;
    return mix(_e124, _e125, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e126));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_2: vec4<f32>;
    var param_18: vec3<f32>;
    var param_19: vec3<f32>;

    let _e114 = (*role);
    let _e116 = (*role);
    let _e121 = unnamed.packed_indices[(_e114 / 4u)][(_e116 % 4u)];
    let _e124 = (*role);
    let _e126 = (*role);
    let _e131 = unnamed.packed_indices[(_e124 / 4u)][(_e126 % 4u)];
    let _e136 = (*uv);
    let _e137 = textureSample(wired_bindless_images[(_e121 & 4095u)], wired_bindless_samplers[((_e131 >> bitcast<u32>(12i)) & 255u)], _e136);
    c_2 = _e137;
    let _e138 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e138))) == 0i) {
        let _e143 = c_2;
        param_18 = _e143.xyz;
        let _e145 = sRGBToLinear_u0028_vf3_u003b((&param_18));
        c_2[0u] = _e145.x;
        c_2[1u] = _e145.y;
        c_2[2u] = _e145.z;
    }
    let _e152 = (*slot);
    if (lightmap_slot == (_e152 + 1i)) {
        let _e155 = c_2;
        param_19 = _e155.xyz;
        let _e157 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_19));
        c_2[0u] = _e157.x;
        c_2[1u] = _e157.y;
        c_2[2u] = _e157.z;
    }
    let _e164 = c_2;
    return _e164;
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
    var param_34: vec4<f32>;

    let _e153 = unnamed.packed_indices[0i][3u];
    let _e159 = unnamed.packed_indices[0i][3u];
    let _e164 = fog_tex_coord_1;
    let _e165 = textureSample(wired_bindless_images[(_e153 & 4095u)], wired_bindless_samplers[((_e159 >> bitcast<u32>(12i)) & 255u)], _e164);
    fog = _e165;
    let _e166 = frag_color0In_1;
    param_20 = _e166.xyz;
    let _e168 = sRGBToLinear_u0028_vf3_u003b((&param_20));
    let _e170 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e168.x, _e168.y, _e168.z, _e170);
    let _e175 = frag_color1In_1;
    param_21 = _e175.xyz;
    let _e177 = sRGBToLinear_u0028_vf3_u003b((&param_21));
    let _e179 = frag_color1In_1[3u];
    frag_color1_ = vec4<f32>(_e177.x, _e177.y, _e177.z, _e179);
    let _e184 = frag_color2In_1;
    param_22 = _e184.xyz;
    let _e186 = sRGBToLinear_u0028_vf3_u003b((&param_22));
    let _e188 = frag_color2In_1[3u];
    frag_color2_ = vec4<f32>(_e186.x, _e186.y, _e186.z, _e188);
    param_23 = 0u;
    let _e193 = frag_tex_coord0_1;
    param_24 = _e193;
    param_25 = 0i;
    let _e194 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_23), (&param_24), (&param_25));
    sampledColor0_ = _e194;
    param_26 = 1u;
    let _e195 = frag_tex_coord1_1;
    param_27 = _e195;
    param_28 = 1i;
    let _e196 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_26), (&param_27), (&param_28));
    sampledColor1_ = _e196;
    param_29 = 2u;
    let _e197 = frag_tex_coord2_1;
    param_30 = _e197;
    param_31 = 2i;
    let _e198 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_29), (&param_30), (&param_31));
    sampledColor2_ = _e198;
    let _e199 = sampledColor0_;
    let _e200 = frag_color0_;
    color0_ = (_e199 * _e200);
    if override_type_3_7 {
        let _e202 = sampledColor1_;
        let _e203 = frag_color1_;
        color1_ = (_e202 * _e203);
        let _e205 = sampledColor2_;
        let _e206 = frag_color2_;
        color2_ = (_e205 * _e206);
        let _e208 = color0_;
        let _e210 = color1_;
        let _e213 = color2_;
        let _e215 = ((_e208.xyz + _e210.xyz) + _e213.xyz);
        let _e217 = color0_[3u];
        let _e219 = color1_[3u];
        let _e222 = color2_[3u];
        base = vec4<f32>(_e215.x, _e215.y, _e215.z, ((_e217 * _e219) * _e222));
    } else {
        if override_type_3_8 {
            let _e228 = sampledColor1_;
            let _e229 = frag_color1_;
            color1_1 = (_e228 * _e229);
            let _e231 = sampledColor2_;
            let _e232 = frag_color2_;
            color2_1 = (_e231 * _e232);
            let _e235 = color0_[3u];
            let _e236 = color0_;
            color0_ = (_e236 * _e235);
            let _e239 = color1_1[3u];
            let _e240 = color1_1;
            color1_1 = (_e240 * _e239);
            let _e243 = color2_1[3u];
            let _e244 = color2_1;
            color2_1 = (_e244 * _e243);
            let _e246 = color0_;
            let _e248 = color1_1;
            let _e251 = color2_1;
            let _e253 = ((_e246.xyz + _e248.xyz) + _e251.xyz);
            let _e255 = color0_[3u];
            let _e257 = color1_1[3u];
            let _e260 = color2_1[3u];
            base = vec4<f32>(_e253.x, _e253.y, _e253.z, ((_e255 * _e257) * _e260));
        } else {
            if override_type_3_9 {
                let _e266 = sampledColor1_;
                let _e267 = frag_color1_;
                color1_2 = (_e266 * _e267);
                let _e269 = sampledColor2_;
                let _e270 = frag_color2_;
                color2_2 = (_e269 * _e270);
                let _e273 = color0_[3u];
                let _e275 = color0_;
                color0_ = (_e275 * (1f - _e273));
                let _e278 = color1_2[3u];
                let _e280 = color1_2;
                color1_2 = (_e280 * (1f - _e278));
                let _e283 = color2_2[3u];
                let _e285 = color2_2;
                color2_2 = (_e285 * (1f - _e283));
                let _e287 = color0_;
                let _e289 = color1_2;
                let _e292 = color2_2;
                let _e294 = ((_e287.xyz + _e289.xyz) + _e292.xyz);
                let _e296 = color0_[3u];
                let _e298 = color1_2[3u];
                let _e301 = color2_2[3u];
                base = vec4<f32>(_e294.x, _e294.y, _e294.z, ((_e296 * _e298) * _e301));
            } else {
                if override_type_3_10 {
                    let _e307 = sampledColor1_;
                    let _e308 = frag_color1_;
                    color1_3 = (_e307 * _e308);
                    let _e310 = sampledColor2_;
                    let _e311 = frag_color2_;
                    color2_3 = (_e310 * _e311);
                    let _e313 = color0_;
                    let _e314 = color1_3;
                    let _e316 = color1_3[3u];
                    let _e319 = color2_3;
                    let _e321 = color2_3[3u];
                    base = mix(mix(_e313, _e314, vec4(_e316)), _e319, vec4(_e321));
                } else {
                    if override_type_3_11 {
                        let _e324 = sampledColor1_;
                        let _e325 = frag_color1_;
                        color1_4 = (_e324 * _e325);
                        let _e327 = sampledColor2_;
                        let _e328 = frag_color2_;
                        color2_4 = (_e327 * _e328);
                        let _e330 = color2_4;
                        let _e331 = color1_4;
                        let _e332 = color0_;
                        let _e334 = color1_4[3u];
                        let _e338 = color2_4[3u];
                        base = mix(_e330, mix(_e331, _e332, vec4(_e334)), vec4(_e338));
                    } else {
                        if override_type_3_12 {
                            let _e341 = sampledColor1_;
                            let _e342 = frag_color1_;
                            color1_5 = (_e341 * _e342);
                            let _e344 = sampledColor2_;
                            let _e345 = frag_color2_;
                            color2_5 = (_e344 * _e345);
                            let _e347 = color2_5;
                            let _e349 = color2_5[3u];
                            let _e352 = color1_5;
                            let _e354 = color1_5[3u];
                            let _e358 = color0_;
                            base = (((_e347 + vec4(_e349)) * (_e352 + vec4(_e354))) * _e358);
                        } else {
                            let _e360 = sampledColor1_;
                            let _e361 = frag_color1_;
                            color1_6 = (_e360 * _e361);
                            let _e363 = sampledColor2_;
                            let _e364 = frag_color2_;
                            color2_6 = (_e363 * _e364);
                            let _e366 = color0_;
                            let _e368 = color1_6;
                            let _e371 = color2_6;
                            let _e373 = ((_e366.xyz * _e368.xyz) * _e371.xyz);
                            base[0u] = _e373.x;
                            base[1u] = _e373.y;
                            base[2u] = _e373.z;
                            let _e381 = color0_[3u];
                            let _e383 = color1_6[3u];
                            let _e386 = color2_6[3u];
                            base[3u] = ((_e381 * _e383) * _e386);
                        }
                    }
                }
            }
        }
    }
    let _e389 = base;
    param_32 = _e389.xyz;
    let _e391 = wired_apply_sun_shadow_operand_u0028_vf3_u003b((&param_32));
    base[0u] = _e391.x;
    base[1u] = _e391.y;
    base[2u] = _e391.z;
    if override_type_3_13 {
        let _e400 = unnamed.worldLightParams[1u];
        wetness = clamp(_e400, 0f, 1f);
        let _e404 = unnamed.worldLightParams[2u];
        frost = clamp(_e404, 0f, 1f);
        let _e406 = base;
        luminance = dot(_e406.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e409 = wetness;
        let _e411 = base;
        let _e413 = (_e411.xyz * mix(1f, 0.82f, _e409));
        base[0u] = _e413.x;
        base[1u] = _e413.y;
        base[2u] = _e413.z;
        let _e420 = base;
        let _e422 = luminance;
        let _e424 = luminance;
        let _e426 = luminance;
        let _e428 = frost;
        let _e431 = mix(_e420.xyz, vec3<f32>((_e422 * 0.88f), (_e424 * 0.94f), _e426), vec3((_e428 * 0.55f)));
        base[0u] = _e431.x;
        base[1u] = _e431.y;
        base[2u] = _e431.z;
    }
    let _e440 = unnamed.materialFactors[0u];
    if (_e440 >= 0f) {
        let _e443 = unnamed.materialBaseColor;
        let _e444 = base;
        base = (_e444 * _e443);
    }
    let _e446 = color0_;
    let _e449 = unnamed.emissionRadiance;
    let _e452 = base;
    let _e454 = (_e452.xyz + (_e446.xyz * _e449.xyz));
    base[0u] = _e454.x;
    base[1u] = _e454.y;
    base[2u] = _e454.z;
    let _e461 = wired_advanced_fog_enabled_u0028_();
    if _e461 {
        let _e462 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e462;
        if override_type_3_14 {
            let _e463 = fogAmount;
            let _e465 = base;
            let _e467 = (_e465.xyz * (1f - _e463));
            base[0u] = _e467.x;
            base[1u] = _e467.y;
            base[2u] = _e467.z;
        } else {
            if override_type_3_15 {
                let _e474 = fogAmount;
                let _e476 = base;
                base = (_e476 * (1f - _e474));
            } else {
                if override_type_3_16 {
                    let _e478 = fogAmount;
                    let _e481 = base[3u];
                    base[3u] = (_e481 * (1f - _e478));
                } else {
                    let _e484 = base;
                    let _e487 = unnamed.advancedFogColorDensity;
                    let _e489 = fogAmount;
                    let _e491 = mix(_e484.xyz, _e487.xyz, vec3(_e489));
                    base[0u] = _e491.x;
                    base[1u] = _e491.y;
                    base[2u] = _e491.z;
                }
            }
        }
    } else {
        if override_type_3_17 {
            let _e498 = base;
            let _e501 = fog[3u];
            let _e503 = (_e498.xyz * (1f - _e501));
            base[0u] = _e503.x;
            base[1u] = _e503.y;
            base[2u] = _e503.z;
        } else {
            if override_type_3_18 {
                let _e510 = base;
                let _e512 = fog[3u];
                base = (_e510 * (1f - _e512));
            } else {
                if override_type_3_19 {
                    let _e516 = base[3u];
                    let _e518 = fog[3u];
                    base[3u] = (_e516 * (1f - _e518));
                } else {
                    let _e522 = base;
                    let _e523 = fog;
                    let _e525 = unnamed.fogColor;
                    let _e528 = fog[3u];
                    base = mix(_e522, (_e523 * _e525), vec4(_e528));
                }
            }
        }
    }
    if override_type_3_20 {
        let _e532 = base[3u];
        if (_e532 == 0f) {
            discard;
        }
    } else {
        if override_type_3_21 {
            let _e534 = base;
            let _e536 = base;
            if (dot(_e534.xyz, _e536.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e540 = base;
    out_color = _e540;
    param_33 = 1f;
    let _e541 = base;
    param_34 = _e541;
    wiredTemporalWriteAux_u0028_f1_u003b_vf4_u003b((&param_33), (&param_34));
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
