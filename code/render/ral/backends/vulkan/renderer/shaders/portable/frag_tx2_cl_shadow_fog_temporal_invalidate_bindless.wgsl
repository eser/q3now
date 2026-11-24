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
override override_type_6_: bool = (shadow_pcf <= 1i);
override override_type_6_1: bool = (shadow_pcf <= 5i);
override override_type_6_2: bool = (lightmap_slot == 1i);
override override_type_6_3: bool = (lightmap_slot == 2i);
override override_type_6_4: bool = (lightmap_slot == 3i);
@id(6) override tex_mode: i32 = 0i;
override override_type_6_5: bool = (tex_mode == 1i);
override override_type_6_6: bool = (tex_mode == 2i);
override override_type_6_7: bool = (override_type_6_5 || override_type_6_6);
override override_type_6_8: bool = (tex_mode == 3i);
override override_type_6_9: bool = (tex_mode == 4i);
override override_type_6_10: bool = (tex_mode == 5i);
override override_type_6_11: bool = (tex_mode == 6i);
override override_type_6_12: bool = (tex_mode == 7i);
override override_type_6_13: bool = (lightmap_slot != 0i);
@id(10) override acff: i32 = 0i;
override override_type_6_14: bool = (acff == 1i);
override override_type_6_15: bool = (acff == 2i);
override override_type_6_16: bool = (acff == 3i);
override override_type_6_17: bool = (acff == 1i);
override override_type_6_18: bool = (acff == 2i);
override override_type_6_19: bool = (acff == 3i);
@id(7) override discard_mode: i32 = 0i;
override override_type_6_20: bool = (discard_mode == 1i);
override override_type_6_21: bool = (discard_mode == 2i);
@id(3) override alpha_to_coverage: i32 = 0i;

var<private> out_temporal_velocity: vec2<f32>;
var<private> out_temporal_validity: vec4<f32>;
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
var<private> temporalCurrentClip_1: vec4<f32>;
var<private> temporalPreviousClip_1: vec4<f32>;
var<private> temporalOutcome_1: u32;

fn wiredTemporalWriteAux_u0028_f1_u003b_vf4_u003b(coverageConfidence: ptr<function, f32>, sceneColor: ptr<function, vec4<f32>>) {
    out_temporal_velocity = vec2<f32>(0f, 0f);
    out_temporal_validity = vec4<f32>(0f, 0f, 0f, 0f);
    let _e107 = (*sceneColor)[0u];
    let _e109 = (*sceneColor)[1u];
    let _e111 = (*sceneColor)[2u];
    let _e116 = (*sceneColor)[3u];
    out_temporal_validity = vec4<f32>(clamp(max(_e107, max(_e109, _e111)), 0f, 1f), 0f, 0f, clamp(_e116, 0f, 1f));
    return;
}

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e107 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e107 + 0.5f));
    let _e112 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e114 = fogType;
    let _e117 = fogType;
    return (((_e112 > 0.5f) && (_e114 >= 1i)) && (_e117 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e107 = wired_advanced_fog_enabled_u0028_();
    if !(_e107) {
        return 0f;
    }
    let _e110 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e110, 0.000001f));
    let _e115 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e115 + 0.5f));
    let _e118 = fogType_1;
    if (_e118 == 1i) {
        let _e122 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e122 <= 0f) {
            return 0f;
        }
        let _e124 = viewDepth;
        let _e127 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e124 / _e127), 0f, 1f);
    }
    let _e132 = unnamed.advancedFogColorDensity[3u];
    let _e134 = viewDepth;
    opticalDepth = (max(_e132, 0f) * _e134);
    let _e136 = fogType_1;
    if (_e136 == 2i) {
        let _e138 = opticalDepth;
        return clamp((1f - exp(-(_e138))), 0f, 1f);
    }
    let _e143 = opticalDepth;
    let _e144 = opticalDepth;
    return clamp((1f - exp(-((_e143 * _e144)))), 0f, 1f);
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
    var phi_355_: bool;
    var phi_362_: bool;
    var phi_369_: bool;
    var phi_376_: bool;

    let _e114 = (*c);
    let _e117 = unnamed.cascadeMVP[_e114];
    let _e118 = (*worldPos);
    sc4_ = (_e117 * vec4<f32>(_e118.x, _e118.y, _e118.z, 1f));
    let _e124 = sc4_;
    let _e127 = sc4_[3u];
    sc = (_e124.xyz / vec3(_e127));
    let _e130 = sc;
    let _e134 = ((_e130.xy * 0.5f) + vec2(0.5f));
    sc[0u] = _e134.x;
    sc[1u] = _e134.y;
    let _e140 = sc[0u];
    let _e141 = (_e140 < 0f);
    phi_355_ = _e141;
    if !(_e141) {
        let _e144 = sc[0u];
        phi_355_ = (_e144 > 1f);
    }
    let _e147 = phi_355_;
    phi_362_ = _e147;
    if !(_e147) {
        let _e150 = sc[1u];
        phi_362_ = (_e150 < 0f);
    }
    let _e153 = phi_362_;
    phi_369_ = _e153;
    if !(_e153) {
        let _e156 = sc[1u];
        phi_369_ = (_e156 > 1f);
    }
    let _e159 = phi_369_;
    phi_376_ = _e159;
    if !(_e159) {
        let _e162 = sc[2u];
        phi_376_ = (_e162 > 1f);
    }
    let _e165 = phi_376_;
    if _e165 {
        return 1f;
    }
    let _e166 = (*c);
    layer = f32(_e166);
    let _e168 = textureDimensions(shadowMap, 0i);
    texelSize = (vec2(1f) / vec2<f32>(vec2<i32>(_e168).xy));
    let _e175 = sc[2u];
    currentDepth = (_e175 - shadow_bias);
    shadow = 0f;
    if override_type_6_ {
        let _e177 = currentDepth;
        let _e178 = sc;
        let _e179 = _e178.xy;
        let _e180 = layer;
        let _e183 = vec3<f32>(_e179.x, _e179.y, _e180);
        let _e189 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e183.x, _e183.y), i32(_e183.z));
        shadow = step(_e177, _e189.x);
    } else {
        if override_type_6_1 {
            let _e192 = currentDepth;
            let _e193 = sc;
            let _e194 = _e193.xy;
            let _e195 = layer;
            let _e198 = vec3<f32>(_e194.x, _e194.y, _e195);
            let _e204 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e198.x, _e198.y), i32(_e198.z));
            let _e207 = shadow;
            shadow = (_e207 + step(_e192, _e204.x));
            let _e209 = currentDepth;
            let _e210 = sc;
            let _e213 = texelSize[0u];
            let _e215 = (_e210.xy + vec2<f32>(_e213, 0f));
            let _e216 = layer;
            let _e219 = vec3<f32>(_e215.x, _e215.y, _e216);
            let _e225 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e219.x, _e219.y), i32(_e219.z));
            let _e228 = shadow;
            shadow = (_e228 + step(_e209, _e225.x));
            let _e230 = currentDepth;
            let _e231 = sc;
            let _e234 = texelSize[0u];
            let _e236 = (_e231.xy - vec2<f32>(_e234, 0f));
            let _e237 = layer;
            let _e240 = vec3<f32>(_e236.x, _e236.y, _e237);
            let _e246 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e240.x, _e240.y), i32(_e240.z));
            let _e249 = shadow;
            shadow = (_e249 + step(_e230, _e246.x));
            let _e251 = currentDepth;
            let _e252 = sc;
            let _e255 = texelSize[1u];
            let _e257 = (_e252.xy + vec2<f32>(0f, _e255));
            let _e258 = layer;
            let _e261 = vec3<f32>(_e257.x, _e257.y, _e258);
            let _e267 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e261.x, _e261.y), i32(_e261.z));
            let _e270 = shadow;
            shadow = (_e270 + step(_e251, _e267.x));
            let _e272 = currentDepth;
            let _e273 = sc;
            let _e276 = texelSize[1u];
            let _e278 = (_e273.xy - vec2<f32>(0f, _e276));
            let _e279 = layer;
            let _e282 = vec3<f32>(_e278.x, _e278.y, _e279);
            let _e288 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e282.x, _e282.y), i32(_e282.z));
            let _e291 = shadow;
            shadow = (_e291 + step(_e272, _e288.x));
            let _e293 = shadow;
            shadow = (_e293 / 5f);
        } else {
            x = -1i;
            loop {
                let _e295 = x;
                if (_e295 <= 1i) {
                    y = -1i;
                    loop {
                        let _e297 = y;
                        if (_e297 <= 1i) {
                            let _e299 = currentDepth;
                            let _e300 = sc;
                            let _e302 = x;
                            let _e304 = y;
                            let _e307 = texelSize;
                            let _e309 = (_e300.xy + (vec2<f32>(f32(_e302), f32(_e304)) * _e307));
                            let _e310 = layer;
                            let _e313 = vec3<f32>(_e309.x, _e309.y, _e310);
                            let _e319 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e313.x, _e313.y), i32(_e313.z));
                            let _e322 = shadow;
                            shadow = (_e322 + step(_e299, _e319.x));
                            continue;
                        } else {
                            break;
                        }
                        continuing {
                            let _e324 = y;
                            y = (_e324 + 1i);
                        }
                    }
                    continue;
                } else {
                    break;
                }
                continuing {
                    let _e326 = x;
                    x = (_e326 + 1i);
                }
            }
            let _e328 = shadow;
            shadow = (_e328 / 9f);
        }
    }
    let _e330 = shadow;
    return _e330;
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
    var param: i32;
    var param_1: vec3<f32>;
    var s1_: f32;
    var param_2: i32;
    var param_3: vec3<f32>;

    let _e121 = unnamed.cascadeSplits;
    let _e122 = (*viewDepth_1);
    cmp = step(_e121, vec4(_e122));
    let _e126 = cmp[0u];
    let _e128 = cmp[1u];
    let _e131 = cmp[2u];
    let _e134 = cmp[3u];
    cascade = min(i32((((_e126 + _e128) + _e131) + _e134)), 3i);
    let _e138 = cascade;
    (*outCascade) = _e138;
    let _e139 = cascade;
    if (_e139 == 0i) {
        local = 0f;
    } else {
        let _e141 = cascade;
        let _e146 = unnamed.cascadeSplits[max((_e141 - 1i), 0i)];
        local = _e146;
    }
    let _e147 = local;
    prevSplit = _e147;
    let _e148 = cascade;
    let _e151 = unnamed.cascadeSplits[_e148];
    farSplit = _e151;
    let _e152 = farSplit;
    let _e153 = prevSplit;
    blendRange = max((0.1f * (_e152 - _e153)), 1f);
    let _e157 = farSplit;
    let _e158 = (*viewDepth_1);
    let _e160 = blendRange;
    blendT = clamp(((_e157 - _e158) / _e160), 0f, 1f);
    let _e163 = cascade;
    param = _e163;
    let _e164 = (*worldPos_1);
    param_1 = _e164;
    let _e165 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param), (&param_1));
    s0_ = _e165;
    let _e166 = cascade;
    param_2 = min((_e166 + 1i), 3i);
    let _e169 = (*worldPos_1);
    param_3 = _e169;
    let _e170 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param_2), (&param_3));
    s1_ = _e170;
    let _e171 = s1_;
    let _e172 = s0_;
    let _e173 = blendT;
    return mix(_e171, _e172, _e173);
}

fn wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b(rgb: ptr<function, vec3<f32>>, lm_uv: ptr<function, vec2<f32>>) -> vec3<f32> {
    var sun_mask: f32;
    var shadow_1: f32;
    var ignoredCascade: i32;
    var param_4: vec3<f32>;
    var param_5: f32;
    var param_6: i32;

    let _e115 = unnamed.packed_indices[1i][3u];
    let _e121 = unnamed.packed_indices[1i][3u];
    let _e126 = (*lm_uv);
    let _e127 = textureSample(wired_bindless_images[(_e115 & 4095u)], wired_bindless_samplers[((_e121 >> bitcast<u32>(12i)) & 255u)], _e126);
    sun_mask = _e127.x;
    let _e129 = sun_mask;
    if (_e129 > 0.001f) {
        let _e131 = shadowData_1;
        param_4 = _e131.xyz;
        let _e134 = shadowData_1[3u];
        param_5 = _e134;
        let _e135 = sampleShadow_u0028_vf3_u003b_f1_u003b_i1_u003b((&param_4), (&param_5), (&param_6));
        let _e136 = param_6;
        ignoredCascade = _e136;
        shadow_1 = _e135;
        let _e137 = shadow_1;
        let _e138 = sun_mask;
        let _e140 = (*rgb);
        (*rgb) = (_e140 * mix(1f, _e137, _e138));
    }
    let _e142 = (*rgb);
    return _e142;
}

fn wired_apply_sun_shadow_operand_u0028_vf3_u003b(rgb_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var param_7: vec3<f32>;
    var param_8: vec2<f32>;
    var param_9: vec3<f32>;
    var param_10: vec2<f32>;
    var param_11: vec3<f32>;
    var param_12: vec2<f32>;

    if override_type_6_2 {
        let _e111 = (*rgb_1);
        param_7 = _e111;
        let _e112 = frag_tex_coord0_1;
        param_8 = _e112;
        let _e113 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_7), (&param_8));
        return _e113;
    }
    if override_type_6_3 {
        let _e114 = (*rgb_1);
        param_9 = _e114;
        let _e115 = frag_tex_coord1_1;
        param_10 = _e115;
        let _e116 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_9), (&param_10));
        return _e116;
    }
    if override_type_6_4 {
        let _e117 = (*rgb_1);
        param_11 = _e117;
        let _e118 = frag_tex_coord2_1;
        param_12 = _e118;
        let _e119 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_11), (&param_12));
        return _e119;
    }
    let _e120 = (*rgb_1);
    return _e120;
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb_2: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e107 = (*rgb_2);
    let _e110 = unnamed.worldLightParams[0u];
    boosted = (_e107 * _e110);
    let _e113 = boosted[0u];
    let _e115 = boosted[1u];
    let _e117 = boosted[2u];
    peak = max(_e113, max(_e115, _e117));
    let _e120 = peak;
    if (_e120 > 1f) {
        let _e122 = peak;
        let _e123 = boosted;
        boosted = (_e123 / vec3(_e122));
    }
    let _e126 = boosted;
    return _e126;
}

fn sRGBToLinear_u0028_vf3_u003b(c_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e108 = (*c_1);
    (*c_1) = max(_e108, vec3<f32>(0f, 0f, 0f));
    let _e110 = (*c_1);
    cutoff = (_e110 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e112 = (*c_1);
    lo = (_e112 / vec3(12.92f));
    let _e115 = (*c_1);
    hi = pow(((_e115 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e120 = hi;
    let _e121 = lo;
    let _e122 = cutoff;
    return mix(_e120, _e121, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e122));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_2: vec4<f32>;
    var param_13: vec3<f32>;
    var param_14: vec3<f32>;

    let _e110 = (*role);
    let _e112 = (*role);
    let _e117 = unnamed.packed_indices[(_e110 / 4u)][(_e112 % 4u)];
    let _e120 = (*role);
    let _e122 = (*role);
    let _e127 = unnamed.packed_indices[(_e120 / 4u)][(_e122 % 4u)];
    let _e132 = (*uv);
    let _e133 = textureSample(wired_bindless_images[(_e117 & 4095u)], wired_bindless_samplers[((_e127 >> bitcast<u32>(12i)) & 255u)], _e132);
    c_2 = _e133;
    let _e134 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e134))) == 0i) {
        let _e139 = c_2;
        param_13 = _e139.xyz;
        let _e141 = sRGBToLinear_u0028_vf3_u003b((&param_13));
        c_2[0u] = _e141.x;
        c_2[1u] = _e141.y;
        c_2[2u] = _e141.z;
    }
    let _e148 = (*slot);
    if (lightmap_slot == (_e148 + 1i)) {
        let _e151 = c_2;
        param_14 = _e151.xyz;
        let _e153 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_14));
        c_2[0u] = _e153.x;
        c_2[1u] = _e153.y;
        c_2[2u] = _e153.z;
    }
    let _e160 = c_2;
    return _e160;
}

fn main_1() {
    var fog: vec4<f32>;
    var frag_color0_: vec4<f32>;
    var param_15: vec3<f32>;
    var frag_color1_: vec4<f32>;
    var param_16: vec3<f32>;
    var frag_color2_: vec4<f32>;
    var param_17: vec3<f32>;
    var sampledColor0_: vec4<f32>;
    var param_18: u32;
    var param_19: vec2<f32>;
    var param_20: i32;
    var sampledColor1_: vec4<f32>;
    var param_21: u32;
    var param_22: vec2<f32>;
    var param_23: i32;
    var sampledColor2_: vec4<f32>;
    var param_24: u32;
    var param_25: vec2<f32>;
    var param_26: i32;
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
    var param_27: vec3<f32>;
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var fogAmount: f32;
    var param_28: f32;
    var param_29: vec4<f32>;

    let _e149 = unnamed.packed_indices[0i][3u];
    let _e155 = unnamed.packed_indices[0i][3u];
    let _e160 = fog_tex_coord_1;
    let _e161 = textureSample(wired_bindless_images[(_e149 & 4095u)], wired_bindless_samplers[((_e155 >> bitcast<u32>(12i)) & 255u)], _e160);
    fog = _e161;
    let _e162 = frag_color0In_1;
    param_15 = _e162.xyz;
    let _e164 = sRGBToLinear_u0028_vf3_u003b((&param_15));
    let _e166 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e164.x, _e164.y, _e164.z, _e166);
    let _e171 = frag_color1In_1;
    param_16 = _e171.xyz;
    let _e173 = sRGBToLinear_u0028_vf3_u003b((&param_16));
    let _e175 = frag_color1In_1[3u];
    frag_color1_ = vec4<f32>(_e173.x, _e173.y, _e173.z, _e175);
    let _e180 = frag_color2In_1;
    param_17 = _e180.xyz;
    let _e182 = sRGBToLinear_u0028_vf3_u003b((&param_17));
    let _e184 = frag_color2In_1[3u];
    frag_color2_ = vec4<f32>(_e182.x, _e182.y, _e182.z, _e184);
    param_18 = 0u;
    let _e189 = frag_tex_coord0_1;
    param_19 = _e189;
    param_20 = 0i;
    let _e190 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_18), (&param_19), (&param_20));
    sampledColor0_ = _e190;
    param_21 = 1u;
    let _e191 = frag_tex_coord1_1;
    param_22 = _e191;
    param_23 = 1i;
    let _e192 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_21), (&param_22), (&param_23));
    sampledColor1_ = _e192;
    param_24 = 2u;
    let _e193 = frag_tex_coord2_1;
    param_25 = _e193;
    param_26 = 2i;
    let _e194 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_24), (&param_25), (&param_26));
    sampledColor2_ = _e194;
    let _e195 = sampledColor0_;
    let _e196 = frag_color0_;
    color0_ = (_e195 * _e196);
    if override_type_6_7 {
        let _e198 = sampledColor1_;
        let _e199 = frag_color1_;
        color1_ = (_e198 * _e199);
        let _e201 = sampledColor2_;
        let _e202 = frag_color2_;
        color2_ = (_e201 * _e202);
        let _e204 = color0_;
        let _e206 = color1_;
        let _e209 = color2_;
        let _e211 = ((_e204.xyz + _e206.xyz) + _e209.xyz);
        let _e213 = color0_[3u];
        let _e215 = color1_[3u];
        let _e218 = color2_[3u];
        base = vec4<f32>(_e211.x, _e211.y, _e211.z, ((_e213 * _e215) * _e218));
    } else {
        if override_type_6_8 {
            let _e224 = sampledColor1_;
            let _e225 = frag_color1_;
            color1_1 = (_e224 * _e225);
            let _e227 = sampledColor2_;
            let _e228 = frag_color2_;
            color2_1 = (_e227 * _e228);
            let _e231 = color0_[3u];
            let _e232 = color0_;
            color0_ = (_e232 * _e231);
            let _e235 = color1_1[3u];
            let _e236 = color1_1;
            color1_1 = (_e236 * _e235);
            let _e239 = color2_1[3u];
            let _e240 = color2_1;
            color2_1 = (_e240 * _e239);
            let _e242 = color0_;
            let _e244 = color1_1;
            let _e247 = color2_1;
            let _e249 = ((_e242.xyz + _e244.xyz) + _e247.xyz);
            let _e251 = color0_[3u];
            let _e253 = color1_1[3u];
            let _e256 = color2_1[3u];
            base = vec4<f32>(_e249.x, _e249.y, _e249.z, ((_e251 * _e253) * _e256));
        } else {
            if override_type_6_9 {
                let _e262 = sampledColor1_;
                let _e263 = frag_color1_;
                color1_2 = (_e262 * _e263);
                let _e265 = sampledColor2_;
                let _e266 = frag_color2_;
                color2_2 = (_e265 * _e266);
                let _e269 = color0_[3u];
                let _e271 = color0_;
                color0_ = (_e271 * (1f - _e269));
                let _e274 = color1_2[3u];
                let _e276 = color1_2;
                color1_2 = (_e276 * (1f - _e274));
                let _e279 = color2_2[3u];
                let _e281 = color2_2;
                color2_2 = (_e281 * (1f - _e279));
                let _e283 = color0_;
                let _e285 = color1_2;
                let _e288 = color2_2;
                let _e290 = ((_e283.xyz + _e285.xyz) + _e288.xyz);
                let _e292 = color0_[3u];
                let _e294 = color1_2[3u];
                let _e297 = color2_2[3u];
                base = vec4<f32>(_e290.x, _e290.y, _e290.z, ((_e292 * _e294) * _e297));
            } else {
                if override_type_6_10 {
                    let _e303 = sampledColor1_;
                    let _e304 = frag_color1_;
                    color1_3 = (_e303 * _e304);
                    let _e306 = sampledColor2_;
                    let _e307 = frag_color2_;
                    color2_3 = (_e306 * _e307);
                    let _e309 = color0_;
                    let _e310 = color1_3;
                    let _e312 = color1_3[3u];
                    let _e315 = color2_3;
                    let _e317 = color2_3[3u];
                    base = mix(mix(_e309, _e310, vec4(_e312)), _e315, vec4(_e317));
                } else {
                    if override_type_6_11 {
                        let _e320 = sampledColor1_;
                        let _e321 = frag_color1_;
                        color1_4 = (_e320 * _e321);
                        let _e323 = sampledColor2_;
                        let _e324 = frag_color2_;
                        color2_4 = (_e323 * _e324);
                        let _e326 = color2_4;
                        let _e327 = color1_4;
                        let _e328 = color0_;
                        let _e330 = color1_4[3u];
                        let _e334 = color2_4[3u];
                        base = mix(_e326, mix(_e327, _e328, vec4(_e330)), vec4(_e334));
                    } else {
                        if override_type_6_12 {
                            let _e337 = sampledColor1_;
                            let _e338 = frag_color1_;
                            color1_5 = (_e337 * _e338);
                            let _e340 = sampledColor2_;
                            let _e341 = frag_color2_;
                            color2_5 = (_e340 * _e341);
                            let _e343 = color2_5;
                            let _e345 = color2_5[3u];
                            let _e348 = color1_5;
                            let _e350 = color1_5[3u];
                            let _e354 = color0_;
                            base = (((_e343 + vec4(_e345)) * (_e348 + vec4(_e350))) * _e354);
                        } else {
                            let _e356 = sampledColor1_;
                            let _e357 = frag_color1_;
                            color1_6 = (_e356 * _e357);
                            let _e359 = sampledColor2_;
                            let _e360 = frag_color2_;
                            color2_6 = (_e359 * _e360);
                            let _e362 = color0_;
                            let _e364 = color1_6;
                            let _e367 = color2_6;
                            let _e369 = ((_e362.xyz * _e364.xyz) * _e367.xyz);
                            base[0u] = _e369.x;
                            base[1u] = _e369.y;
                            base[2u] = _e369.z;
                            let _e377 = color0_[3u];
                            let _e379 = color1_6[3u];
                            let _e382 = color2_6[3u];
                            base[3u] = ((_e377 * _e379) * _e382);
                        }
                    }
                }
            }
        }
    }
    let _e385 = base;
    param_27 = _e385.xyz;
    let _e387 = wired_apply_sun_shadow_operand_u0028_vf3_u003b((&param_27));
    base[0u] = _e387.x;
    base[1u] = _e387.y;
    base[2u] = _e387.z;
    if override_type_6_13 {
        let _e396 = unnamed.worldLightParams[1u];
        wetness = clamp(_e396, 0f, 1f);
        let _e400 = unnamed.worldLightParams[2u];
        frost = clamp(_e400, 0f, 1f);
        let _e402 = base;
        luminance = dot(_e402.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e405 = wetness;
        let _e407 = base;
        let _e409 = (_e407.xyz * mix(1f, 0.82f, _e405));
        base[0u] = _e409.x;
        base[1u] = _e409.y;
        base[2u] = _e409.z;
        let _e416 = base;
        let _e418 = luminance;
        let _e420 = luminance;
        let _e422 = luminance;
        let _e424 = frost;
        let _e427 = mix(_e416.xyz, vec3<f32>((_e418 * 0.88f), (_e420 * 0.94f), _e422), vec3((_e424 * 0.55f)));
        base[0u] = _e427.x;
        base[1u] = _e427.y;
        base[2u] = _e427.z;
    }
    let _e436 = unnamed.materialFactors[0u];
    if (_e436 >= 0f) {
        let _e439 = unnamed.materialBaseColor;
        let _e440 = base;
        base = (_e440 * _e439);
    }
    let _e442 = color0_;
    let _e445 = unnamed.emissionRadiance;
    let _e448 = base;
    let _e450 = (_e448.xyz + (_e442.xyz * _e445.xyz));
    base[0u] = _e450.x;
    base[1u] = _e450.y;
    base[2u] = _e450.z;
    let _e457 = wired_advanced_fog_enabled_u0028_();
    if _e457 {
        let _e458 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e458;
        if override_type_6_14 {
            let _e459 = fogAmount;
            let _e461 = base;
            let _e463 = (_e461.xyz * (1f - _e459));
            base[0u] = _e463.x;
            base[1u] = _e463.y;
            base[2u] = _e463.z;
        } else {
            if override_type_6_15 {
                let _e470 = fogAmount;
                let _e472 = base;
                base = (_e472 * (1f - _e470));
            } else {
                if override_type_6_16 {
                    let _e474 = fogAmount;
                    let _e477 = base[3u];
                    base[3u] = (_e477 * (1f - _e474));
                } else {
                    let _e480 = base;
                    let _e483 = unnamed.advancedFogColorDensity;
                    let _e485 = fogAmount;
                    let _e487 = mix(_e480.xyz, _e483.xyz, vec3(_e485));
                    base[0u] = _e487.x;
                    base[1u] = _e487.y;
                    base[2u] = _e487.z;
                }
            }
        }
    } else {
        if override_type_6_17 {
            let _e494 = base;
            let _e497 = fog[3u];
            let _e499 = (_e494.xyz * (1f - _e497));
            base[0u] = _e499.x;
            base[1u] = _e499.y;
            base[2u] = _e499.z;
        } else {
            if override_type_6_18 {
                let _e506 = base;
                let _e508 = fog[3u];
                base = (_e506 * (1f - _e508));
            } else {
                if override_type_6_19 {
                    let _e512 = base[3u];
                    let _e514 = fog[3u];
                    base[3u] = (_e512 * (1f - _e514));
                } else {
                    let _e518 = base;
                    let _e519 = fog;
                    let _e521 = unnamed.fogColor;
                    let _e524 = fog[3u];
                    base = mix(_e518, (_e519 * _e521), vec4(_e524));
                }
            }
        }
    }
    if override_type_6_20 {
        let _e528 = base[3u];
        if (_e528 == 0f) {
            discard;
        }
    } else {
        if override_type_6_21 {
            let _e530 = base;
            let _e532 = base;
            if (dot(_e530.xyz, _e532.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e536 = base;
    out_color = _e536;
    param_28 = 1f;
    let _e537 = base;
    param_29 = _e537;
    wiredTemporalWriteAux_u0028_f1_u003b_vf4_u003b((&param_28), (&param_29));
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(7) shadowData: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(3) frag_tex_coord2_: vec2<f32>, @location(4) fog_tex_coord: vec2<f32>, @location(0) frag_color0In: vec4<f32>, @location(5) frag_color1In: vec4<f32>, @location(6) frag_color2In: vec4<f32>, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @location(12) @interpolate(flat) temporalOutcome: u32) -> FragmentOutput {
    gl_FragCoord_1 = gl_FragCoord;
    shadowData_1 = shadowData;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    frag_tex_coord2_1 = frag_tex_coord2_;
    fog_tex_coord_1 = fog_tex_coord;
    frag_color0In_1 = frag_color0In;
    frag_color1In_1 = frag_color1In;
    frag_color2In_1 = frag_color2In;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    temporalOutcome_1 = temporalOutcome;
    main_1();
    let _e27 = out_temporal_velocity;
    let _e28 = out_temporal_validity;
    let _e29 = out_color;
    return FragmentOutput(_e27, _e28, _e29);
}
