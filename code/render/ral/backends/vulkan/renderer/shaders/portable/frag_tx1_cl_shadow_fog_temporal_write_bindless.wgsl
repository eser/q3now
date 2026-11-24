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
var<private> fog_tex_coord_1: vec2<f32>;
var<private> frag_color0In_1: vec4<f32>;
var<private> frag_color1In_1: vec4<f32>;
var<private> out_color: vec4<f32>;

fn wiredTemporalFinite2_u0028_vf2_u003b(value: ptr<function, vec2<f32>>) -> bool {
    var phi_98_: bool;

    let _e106 = (*value);
    let _e107 = (*value);
    let _e109 = all((_e106 == _e107));
    phi_98_ = _e109;
    if _e109 {
        let _e110 = (*value);
        phi_98_ = all((abs(_e110) <= vec2<f32>(0x1.fffffep+127f, 0x1.fffffep+127f)));
    }
    let _e115 = phi_98_;
    return _e115;
}

fn wiredTemporalFinite4_u0028_vf4_u003b(value_1: ptr<function, vec4<f32>>) -> bool {
    var phi_83_: bool;

    let _e106 = (*value_1);
    let _e107 = (*value_1);
    let _e109 = all((_e106 == _e107));
    phi_83_ = _e109;
    if _e109 {
        let _e110 = (*value_1);
        phi_83_ = all((abs(_e110) <= vec4<f32>(0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f)));
    }
    let _e115 = phi_83_;
    return _e115;
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
    let _e117 = temporalOutcome_1;
    let _e118 = (_e117 != 1u);
    phi_122_ = _e118;
    if !(_e118) {
        let _e120 = temporalCurrentClip_1;
        param = _e120;
        let _e121 = wiredTemporalFinite4_u0028_vf4_u003b((&param));
        phi_122_ = !(_e121);
    }
    let _e124 = phi_122_;
    phi_131_ = _e124;
    if !(_e124) {
        let _e126 = temporalPreviousClip_1;
        param_1 = _e126;
        let _e127 = wiredTemporalFinite4_u0028_vf4_u003b((&param_1));
        phi_131_ = !(_e127);
    }
    let _e130 = phi_131_;
    phi_141_ = _e130;
    if !(_e130) {
        let _e133 = temporalCurrentClip_1[3u];
        phi_141_ = (_e133 <= 0.000001f);
    }
    let _e136 = phi_141_;
    phi_148_ = _e136;
    if !(_e136) {
        let _e139 = temporalPreviousClip_1[3u];
        phi_148_ = (_e139 <= 0.000001f);
    }
    let _e142 = phi_148_;
    if _e142 {
        return;
    }
    let _e143 = temporalCurrentClip_1;
    let _e146 = temporalCurrentClip_1[3u];
    currentNdc = (_e143.xy / vec2(_e146));
    let _e149 = temporalPreviousClip_1;
    let _e152 = temporalPreviousClip_1[3u];
    previousNdc = (_e149.xy / vec2(_e152));
    let _e155 = currentNdc;
    param_2 = _e155;
    let _e156 = wiredTemporalFinite2_u0028_vf2_u003b((&param_2));
    let _e157 = !(_e156);
    phi_177_ = _e157;
    if !(_e157) {
        let _e159 = previousNdc;
        param_3 = _e159;
        let _e160 = wiredTemporalFinite2_u0028_vf2_u003b((&param_3));
        phi_177_ = !(_e160);
    }
    let _e163 = phi_177_;
    if _e163 {
        return;
    }
    let _e164 = currentNdc;
    currentUv = ((_e164 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e167 = previousNdc;
    previousUv = ((_e167 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e170 = currentUv;
    let _e171 = previousUv;
    velocity = (_e170 - _e171);
    let _e173 = velocity;
    param_4 = _e173;
    let _e174 = wiredTemporalFinite2_u0028_vf2_u003b((&param_4));
    if !(_e174) {
        return;
    }
    let _e176 = velocity;
    out_temporal_velocity = _e176;
    let _e177 = (*coverageConfidence);
    out_temporal_validity = vec4<f32>(clamp(_e177, 0f, 1f), 0f, 0f, 1f);
    return;
}

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e108 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e108 + 0.5f));
    let _e113 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e115 = fogType;
    let _e118 = fogType;
    return (((_e113 > 0.5f) && (_e115 >= 1i)) && (_e118 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e108 = wired_advanced_fog_enabled_u0028_();
    if !(_e108) {
        return 0f;
    }
    let _e111 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e111, 0.000001f));
    let _e116 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e116 + 0.5f));
    let _e119 = fogType_1;
    if (_e119 == 1i) {
        let _e123 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e123 <= 0f) {
            return 0f;
        }
        let _e125 = viewDepth;
        let _e128 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e125 / _e128), 0f, 1f);
    }
    let _e133 = unnamed.advancedFogColorDensity[3u];
    let _e135 = viewDepth;
    opticalDepth = (max(_e133, 0f) * _e135);
    let _e137 = fogType_1;
    if (_e137 == 2i) {
        let _e139 = opticalDepth;
        return clamp((1f - exp(-(_e139))), 0f, 1f);
    }
    let _e144 = opticalDepth;
    let _e145 = opticalDepth;
    return clamp((1f - exp(-((_e144 * _e145)))), 0f, 1f);
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

    let _e115 = (*c);
    let _e118 = unnamed.cascadeMVP[_e115];
    let _e119 = (*worldPos);
    sc4_ = (_e118 * vec4<f32>(_e119.x, _e119.y, _e119.z, 1f));
    let _e125 = sc4_;
    let _e128 = sc4_[3u];
    sc = (_e125.xyz / vec3(_e128));
    let _e131 = sc;
    let _e135 = ((_e131.xy * 0.5f) + vec2(0.5f));
    sc[0u] = _e135.x;
    sc[1u] = _e135.y;
    let _e141 = sc[0u];
    let _e142 = (_e141 < 0f);
    phi_473_ = _e142;
    if !(_e142) {
        let _e145 = sc[0u];
        phi_473_ = (_e145 > 1f);
    }
    let _e148 = phi_473_;
    phi_480_ = _e148;
    if !(_e148) {
        let _e151 = sc[1u];
        phi_480_ = (_e151 < 0f);
    }
    let _e154 = phi_480_;
    phi_487_ = _e154;
    if !(_e154) {
        let _e157 = sc[1u];
        phi_487_ = (_e157 > 1f);
    }
    let _e160 = phi_487_;
    phi_494_ = _e160;
    if !(_e160) {
        let _e163 = sc[2u];
        phi_494_ = (_e163 > 1f);
    }
    let _e166 = phi_494_;
    if _e166 {
        return 1f;
    }
    let _e167 = (*c);
    layer = f32(_e167);
    let _e169 = textureDimensions(shadowMap, 0i);
    texelSize = (vec2(1f) / vec2<f32>(vec2<i32>(_e169).xy));
    let _e176 = sc[2u];
    currentDepth = (_e176 - shadow_bias);
    shadow = 0f;
    if override_type_3_ {
        let _e178 = currentDepth;
        let _e179 = sc;
        let _e180 = _e179.xy;
        let _e181 = layer;
        let _e184 = vec3<f32>(_e180.x, _e180.y, _e181);
        let _e190 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e184.x, _e184.y), i32(_e184.z));
        shadow = step(_e178, _e190.x);
    } else {
        if override_type_3_1 {
            let _e193 = currentDepth;
            let _e194 = sc;
            let _e195 = _e194.xy;
            let _e196 = layer;
            let _e199 = vec3<f32>(_e195.x, _e195.y, _e196);
            let _e205 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e199.x, _e199.y), i32(_e199.z));
            let _e208 = shadow;
            shadow = (_e208 + step(_e193, _e205.x));
            let _e210 = currentDepth;
            let _e211 = sc;
            let _e214 = texelSize[0u];
            let _e216 = (_e211.xy + vec2<f32>(_e214, 0f));
            let _e217 = layer;
            let _e220 = vec3<f32>(_e216.x, _e216.y, _e217);
            let _e226 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e220.x, _e220.y), i32(_e220.z));
            let _e229 = shadow;
            shadow = (_e229 + step(_e210, _e226.x));
            let _e231 = currentDepth;
            let _e232 = sc;
            let _e235 = texelSize[0u];
            let _e237 = (_e232.xy - vec2<f32>(_e235, 0f));
            let _e238 = layer;
            let _e241 = vec3<f32>(_e237.x, _e237.y, _e238);
            let _e247 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e241.x, _e241.y), i32(_e241.z));
            let _e250 = shadow;
            shadow = (_e250 + step(_e231, _e247.x));
            let _e252 = currentDepth;
            let _e253 = sc;
            let _e256 = texelSize[1u];
            let _e258 = (_e253.xy + vec2<f32>(0f, _e256));
            let _e259 = layer;
            let _e262 = vec3<f32>(_e258.x, _e258.y, _e259);
            let _e268 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e262.x, _e262.y), i32(_e262.z));
            let _e271 = shadow;
            shadow = (_e271 + step(_e252, _e268.x));
            let _e273 = currentDepth;
            let _e274 = sc;
            let _e277 = texelSize[1u];
            let _e279 = (_e274.xy - vec2<f32>(0f, _e277));
            let _e280 = layer;
            let _e283 = vec3<f32>(_e279.x, _e279.y, _e280);
            let _e289 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e283.x, _e283.y), i32(_e283.z));
            let _e292 = shadow;
            shadow = (_e292 + step(_e273, _e289.x));
            let _e294 = shadow;
            shadow = (_e294 / 5f);
        } else {
            x = -1i;
            loop {
                let _e296 = x;
                if (_e296 <= 1i) {
                    y = -1i;
                    loop {
                        let _e298 = y;
                        if (_e298 <= 1i) {
                            let _e300 = currentDepth;
                            let _e301 = sc;
                            let _e303 = x;
                            let _e305 = y;
                            let _e308 = texelSize;
                            let _e310 = (_e301.xy + (vec2<f32>(f32(_e303), f32(_e305)) * _e308));
                            let _e311 = layer;
                            let _e314 = vec3<f32>(_e310.x, _e310.y, _e311);
                            let _e320 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e314.x, _e314.y), i32(_e314.z));
                            let _e323 = shadow;
                            shadow = (_e323 + step(_e300, _e320.x));
                            continue;
                        } else {
                            break;
                        }
                        continuing {
                            let _e325 = y;
                            y = (_e325 + 1i);
                        }
                    }
                    continue;
                } else {
                    break;
                }
                continuing {
                    let _e327 = x;
                    x = (_e327 + 1i);
                }
            }
            let _e329 = shadow;
            shadow = (_e329 / 9f);
        }
    }
    let _e331 = shadow;
    return _e331;
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

    let _e122 = unnamed.cascadeSplits;
    let _e123 = (*viewDepth_1);
    cmp = step(_e122, vec4(_e123));
    let _e127 = cmp[0u];
    let _e129 = cmp[1u];
    let _e132 = cmp[2u];
    let _e135 = cmp[3u];
    cascade = min(i32((((_e127 + _e129) + _e132) + _e135)), 3i);
    let _e139 = cascade;
    (*outCascade) = _e139;
    let _e140 = cascade;
    if (_e140 == 0i) {
        local = 0f;
    } else {
        let _e142 = cascade;
        let _e147 = unnamed.cascadeSplits[max((_e142 - 1i), 0i)];
        local = _e147;
    }
    let _e148 = local;
    prevSplit = _e148;
    let _e149 = cascade;
    let _e152 = unnamed.cascadeSplits[_e149];
    farSplit = _e152;
    let _e153 = farSplit;
    let _e154 = prevSplit;
    blendRange = max((0.1f * (_e153 - _e154)), 1f);
    let _e158 = farSplit;
    let _e159 = (*viewDepth_1);
    let _e161 = blendRange;
    blendT = clamp(((_e158 - _e159) / _e161), 0f, 1f);
    let _e164 = cascade;
    param_5 = _e164;
    let _e165 = (*worldPos_1);
    param_6 = _e165;
    let _e166 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param_5), (&param_6));
    s0_ = _e166;
    let _e167 = cascade;
    param_7 = min((_e167 + 1i), 3i);
    let _e170 = (*worldPos_1);
    param_8 = _e170;
    let _e171 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param_7), (&param_8));
    s1_ = _e171;
    let _e172 = s1_;
    let _e173 = s0_;
    let _e174 = blendT;
    return mix(_e172, _e173, _e174);
}

fn wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b(rgb: ptr<function, vec3<f32>>, lm_uv: ptr<function, vec2<f32>>) -> vec3<f32> {
    var sun_mask: f32;
    var shadow_1: f32;
    var ignoredCascade: i32;
    var param_9: vec3<f32>;
    var param_10: f32;
    var param_11: i32;

    let _e116 = unnamed.packed_indices[1i][3u];
    let _e122 = unnamed.packed_indices[1i][3u];
    let _e127 = (*lm_uv);
    let _e128 = textureSample(wired_bindless_images[(_e116 & 4095u)], wired_bindless_samplers[((_e122 >> bitcast<u32>(12i)) & 255u)], _e127);
    sun_mask = _e128.x;
    let _e130 = sun_mask;
    if (_e130 > 0.001f) {
        let _e132 = shadowData_1;
        param_9 = _e132.xyz;
        let _e135 = shadowData_1[3u];
        param_10 = _e135;
        let _e136 = sampleShadow_u0028_vf3_u003b_f1_u003b_i1_u003b((&param_9), (&param_10), (&param_11));
        let _e137 = param_11;
        ignoredCascade = _e137;
        shadow_1 = _e136;
        let _e138 = shadow_1;
        let _e139 = sun_mask;
        let _e141 = (*rgb);
        (*rgb) = (_e141 * mix(1f, _e138, _e139));
    }
    let _e143 = (*rgb);
    return _e143;
}

fn wired_apply_sun_shadow_operand_u0028_vf3_u003b(rgb_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var param_12: vec3<f32>;
    var param_13: vec2<f32>;
    var param_14: vec3<f32>;
    var param_15: vec2<f32>;

    if override_type_3_2 {
        let _e110 = (*rgb_1);
        param_12 = _e110;
        let _e111 = frag_tex_coord0_1;
        param_13 = _e111;
        let _e112 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_12), (&param_13));
        return _e112;
    }
    if override_type_3_3 {
        let _e113 = (*rgb_1);
        param_14 = _e113;
        let _e114 = frag_tex_coord1_1;
        param_15 = _e114;
        let _e115 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_14), (&param_15));
        return _e115;
    }
    let _e116 = (*rgb_1);
    return _e116;
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb_2: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e108 = (*rgb_2);
    let _e111 = unnamed.worldLightParams[0u];
    boosted = (_e108 * _e111);
    let _e114 = boosted[0u];
    let _e116 = boosted[1u];
    let _e118 = boosted[2u];
    peak = max(_e114, max(_e116, _e118));
    let _e121 = peak;
    if (_e121 > 1f) {
        let _e123 = peak;
        let _e124 = boosted;
        boosted = (_e124 / vec3(_e123));
    }
    let _e127 = boosted;
    return _e127;
}

fn sRGBToLinear_u0028_vf3_u003b(c_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e109 = (*c_1);
    (*c_1) = max(_e109, vec3<f32>(0f, 0f, 0f));
    let _e111 = (*c_1);
    cutoff = (_e111 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e113 = (*c_1);
    lo = (_e113 / vec3(12.92f));
    let _e116 = (*c_1);
    hi = pow(((_e116 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e121 = hi;
    let _e122 = lo;
    let _e123 = cutoff;
    return mix(_e121, _e122, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e123));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_2: vec4<f32>;
    var param_16: vec3<f32>;
    var param_17: vec3<f32>;

    let _e111 = (*role);
    let _e113 = (*role);
    let _e118 = unnamed.packed_indices[(_e111 / 4u)][(_e113 % 4u)];
    let _e121 = (*role);
    let _e123 = (*role);
    let _e128 = unnamed.packed_indices[(_e121 / 4u)][(_e123 % 4u)];
    let _e133 = (*uv);
    let _e134 = textureSample(wired_bindless_images[(_e118 & 4095u)], wired_bindless_samplers[((_e128 >> bitcast<u32>(12i)) & 255u)], _e133);
    c_2 = _e134;
    let _e135 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e135))) == 0i) {
        let _e140 = c_2;
        param_16 = _e140.xyz;
        let _e142 = sRGBToLinear_u0028_vf3_u003b((&param_16));
        c_2[0u] = _e142.x;
        c_2[1u] = _e142.y;
        c_2[2u] = _e142.z;
    }
    let _e149 = (*slot);
    if (lightmap_slot == (_e149 + 1i)) {
        let _e152 = c_2;
        param_17 = _e152.xyz;
        let _e154 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_17));
        c_2[0u] = _e154.x;
        c_2[1u] = _e154.y;
        c_2[2u] = _e154.z;
    }
    let _e161 = c_2;
    return _e161;
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
    var param_28: vec4<f32>;

    let _e137 = unnamed.packed_indices[0i][3u];
    let _e143 = unnamed.packed_indices[0i][3u];
    let _e148 = fog_tex_coord_1;
    let _e149 = textureSample(wired_bindless_images[(_e137 & 4095u)], wired_bindless_samplers[((_e143 >> bitcast<u32>(12i)) & 255u)], _e148);
    fog = _e149;
    let _e150 = frag_color0In_1;
    param_18 = _e150.xyz;
    let _e152 = sRGBToLinear_u0028_vf3_u003b((&param_18));
    let _e154 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e152.x, _e152.y, _e152.z, _e154);
    let _e159 = frag_color1In_1;
    param_19 = _e159.xyz;
    let _e161 = sRGBToLinear_u0028_vf3_u003b((&param_19));
    let _e163 = frag_color1In_1[3u];
    frag_color1_ = vec4<f32>(_e161.x, _e161.y, _e161.z, _e163);
    param_20 = 0u;
    let _e168 = frag_tex_coord0_1;
    param_21 = _e168;
    param_22 = 0i;
    let _e169 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_20), (&param_21), (&param_22));
    sampledColor0_ = _e169;
    param_23 = 1u;
    let _e170 = frag_tex_coord1_1;
    param_24 = _e170;
    param_25 = 1i;
    let _e171 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_23), (&param_24), (&param_25));
    sampledColor1_ = _e171;
    let _e172 = sampledColor0_;
    let _e173 = frag_color0_;
    color0_ = (_e172 * _e173);
    if override_type_3_6 {
        let _e175 = sampledColor1_;
        let _e176 = frag_color1_;
        color1_ = (_e175 * _e176);
        let _e178 = color0_;
        let _e180 = color1_;
        let _e182 = (_e178.xyz + _e180.xyz);
        let _e184 = color0_[3u];
        let _e186 = color1_[3u];
        base = vec4<f32>(_e182.x, _e182.y, _e182.z, (_e184 * _e186));
    } else {
        if override_type_3_7 {
            let _e192 = sampledColor1_;
            let _e193 = frag_color1_;
            color1_1 = (_e192 * _e193);
            let _e196 = color0_[3u];
            let _e197 = color0_;
            color0_ = (_e197 * _e196);
            let _e200 = color1_1[3u];
            let _e201 = color1_1;
            color1_1 = (_e201 * _e200);
            let _e203 = color0_;
            let _e205 = color1_1;
            let _e207 = (_e203.xyz + _e205.xyz);
            let _e209 = color0_[3u];
            let _e211 = color1_1[3u];
            base = vec4<f32>(_e207.x, _e207.y, _e207.z, (_e209 * _e211));
        } else {
            if override_type_3_8 {
                let _e217 = sampledColor1_;
                let _e218 = frag_color1_;
                color1_2 = (_e217 * _e218);
                let _e221 = color0_[3u];
                let _e223 = color0_;
                color0_ = (_e223 * (1f - _e221));
                let _e226 = color1_2[3u];
                let _e228 = color1_2;
                color1_2 = (_e228 * (1f - _e226));
                let _e230 = color0_;
                let _e232 = color1_2;
                let _e234 = (_e230.xyz + _e232.xyz);
                let _e236 = color0_[3u];
                let _e238 = color1_2[3u];
                base = vec4<f32>(_e234.x, _e234.y, _e234.z, (_e236 * _e238));
            } else {
                if override_type_3_9 {
                    let _e244 = sampledColor1_;
                    let _e245 = frag_color1_;
                    color1_3 = (_e244 * _e245);
                    let _e247 = color0_;
                    let _e248 = color1_3;
                    let _e250 = color1_3[3u];
                    base = mix(_e247, _e248, vec4(_e250));
                } else {
                    if override_type_3_10 {
                        let _e253 = sampledColor1_;
                        let _e254 = frag_color1_;
                        color1_4 = (_e253 * _e254);
                        let _e256 = color1_4;
                        let _e257 = color0_;
                        let _e259 = color1_4[3u];
                        base = mix(_e256, _e257, vec4(_e259));
                    } else {
                        if override_type_3_11 {
                            let _e262 = sampledColor1_;
                            let _e263 = frag_color1_;
                            color1_5 = (_e262 * _e263);
                            let _e265 = color1_5;
                            let _e267 = color1_5[3u];
                            let _e270 = color0_;
                            base = ((_e265 + vec4(_e267)) * _e270);
                        } else {
                            let _e272 = sampledColor1_;
                            let _e273 = frag_color1_;
                            color1_6 = (_e272 * _e273);
                            let _e275 = color0_;
                            let _e277 = color1_6;
                            let _e279 = (_e275.xyz * _e277.xyz);
                            base[0u] = _e279.x;
                            base[1u] = _e279.y;
                            base[2u] = _e279.z;
                            let _e287 = color0_[3u];
                            let _e289 = color1_6[3u];
                            base[3u] = (_e287 * _e289);
                        }
                    }
                }
            }
        }
    }
    let _e292 = base;
    param_26 = _e292.xyz;
    let _e294 = wired_apply_sun_shadow_operand_u0028_vf3_u003b((&param_26));
    base[0u] = _e294.x;
    base[1u] = _e294.y;
    base[2u] = _e294.z;
    if override_type_3_12 {
        let _e303 = unnamed.worldLightParams[1u];
        wetness = clamp(_e303, 0f, 1f);
        let _e307 = unnamed.worldLightParams[2u];
        frost = clamp(_e307, 0f, 1f);
        let _e309 = base;
        luminance = dot(_e309.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e312 = wetness;
        let _e314 = base;
        let _e316 = (_e314.xyz * mix(1f, 0.82f, _e312));
        base[0u] = _e316.x;
        base[1u] = _e316.y;
        base[2u] = _e316.z;
        let _e323 = base;
        let _e325 = luminance;
        let _e327 = luminance;
        let _e329 = luminance;
        let _e331 = frost;
        let _e334 = mix(_e323.xyz, vec3<f32>((_e325 * 0.88f), (_e327 * 0.94f), _e329), vec3((_e331 * 0.55f)));
        base[0u] = _e334.x;
        base[1u] = _e334.y;
        base[2u] = _e334.z;
    }
    let _e343 = unnamed.materialFactors[0u];
    if (_e343 >= 0f) {
        let _e346 = unnamed.materialBaseColor;
        let _e347 = base;
        base = (_e347 * _e346);
    }
    let _e349 = color0_;
    let _e352 = unnamed.emissionRadiance;
    let _e355 = base;
    let _e357 = (_e355.xyz + (_e349.xyz * _e352.xyz));
    base[0u] = _e357.x;
    base[1u] = _e357.y;
    base[2u] = _e357.z;
    let _e364 = wired_advanced_fog_enabled_u0028_();
    if _e364 {
        let _e365 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e365;
        if override_type_3_13 {
            let _e366 = fogAmount;
            let _e368 = base;
            let _e370 = (_e368.xyz * (1f - _e366));
            base[0u] = _e370.x;
            base[1u] = _e370.y;
            base[2u] = _e370.z;
        } else {
            if override_type_3_14 {
                let _e377 = fogAmount;
                let _e379 = base;
                base = (_e379 * (1f - _e377));
            } else {
                if override_type_3_15 {
                    let _e381 = fogAmount;
                    let _e384 = base[3u];
                    base[3u] = (_e384 * (1f - _e381));
                } else {
                    let _e387 = base;
                    let _e390 = unnamed.advancedFogColorDensity;
                    let _e392 = fogAmount;
                    let _e394 = mix(_e387.xyz, _e390.xyz, vec3(_e392));
                    base[0u] = _e394.x;
                    base[1u] = _e394.y;
                    base[2u] = _e394.z;
                }
            }
        }
    } else {
        if override_type_3_16 {
            let _e401 = base;
            let _e404 = fog[3u];
            let _e406 = (_e401.xyz * (1f - _e404));
            base[0u] = _e406.x;
            base[1u] = _e406.y;
            base[2u] = _e406.z;
        } else {
            if override_type_3_17 {
                let _e413 = base;
                let _e415 = fog[3u];
                base = (_e413 * (1f - _e415));
            } else {
                if override_type_3_18 {
                    let _e419 = base[3u];
                    let _e421 = fog[3u];
                    base[3u] = (_e419 * (1f - _e421));
                } else {
                    let _e425 = base;
                    let _e426 = fog;
                    let _e428 = unnamed.fogColor;
                    let _e431 = fog[3u];
                    base = mix(_e425, (_e426 * _e428), vec4(_e431));
                }
            }
        }
    }
    if override_type_3_19 {
        let _e435 = base[3u];
        if (_e435 == 0f) {
            discard;
        }
    } else {
        if override_type_3_20 {
            let _e437 = base;
            let _e439 = base;
            if (dot(_e437.xyz, _e439.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e443 = base;
    out_color = _e443;
    param_27 = 1f;
    let _e444 = base;
    param_28 = _e444;
    wiredTemporalWriteAux_u0028_f1_u003b_vf4_u003b((&param_27), (&param_28));
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
