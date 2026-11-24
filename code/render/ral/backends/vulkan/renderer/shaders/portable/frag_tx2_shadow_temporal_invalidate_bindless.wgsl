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
override override_type_6_7: bool = (lightmap_slot != 0i);
@id(7) override discard_mode: i32 = 0i;
override override_type_6_8: bool = (discard_mode == 1i);
override override_type_6_9: bool = (discard_mode == 2i);
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
var<private> frag_color0In_1: vec4<f32>;
var<private> out_color: vec4<f32>;
var<private> temporalCurrentClip_1: vec4<f32>;
var<private> temporalPreviousClip_1: vec4<f32>;
var<private> temporalOutcome_1: u32;

fn wiredTemporalWriteAux_u0028_f1_u003b_vf4_u003b(coverageConfidence: ptr<function, f32>, sceneColor: ptr<function, vec4<f32>>) {
    out_temporal_velocity = vec2<f32>(0f, 0f);
    out_temporal_validity = vec4<f32>(0f, 0f, 0f, 0f);
    let _e88 = (*sceneColor)[0u];
    let _e90 = (*sceneColor)[1u];
    let _e92 = (*sceneColor)[2u];
    let _e97 = (*sceneColor)[3u];
    out_temporal_validity = vec4<f32>(clamp(max(_e88, max(_e90, _e92)), 0f, 1f), 0f, 0f, clamp(_e97, 0f, 1f));
    return;
}

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e88 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e88 + 0.5f));
    let _e93 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e95 = fogType;
    let _e98 = fogType;
    return (((_e93 > 0.5f) && (_e95 >= 1i)) && (_e98 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e88 = wired_advanced_fog_enabled_u0028_();
    if !(_e88) {
        return 0f;
    }
    let _e91 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e91, 0.000001f));
    let _e96 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e96 + 0.5f));
    let _e99 = fogType_1;
    if (_e99 == 1i) {
        let _e103 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e103 <= 0f) {
            return 0f;
        }
        let _e105 = viewDepth;
        let _e108 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e105 / _e108), 0f, 1f);
    }
    let _e113 = unnamed.advancedFogColorDensity[3u];
    let _e115 = viewDepth;
    opticalDepth = (max(_e113, 0f) * _e115);
    let _e117 = fogType_1;
    if (_e117 == 2i) {
        let _e119 = opticalDepth;
        return clamp((1f - exp(-(_e119))), 0f, 1f);
    }
    let _e124 = opticalDepth;
    let _e125 = opticalDepth;
    return clamp((1f - exp(-((_e124 * _e125)))), 0f, 1f);
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

    let _e95 = (*c);
    let _e98 = unnamed.cascadeMVP[_e95];
    let _e99 = (*worldPos);
    sc4_ = (_e98 * vec4<f32>(_e99.x, _e99.y, _e99.z, 1f));
    let _e105 = sc4_;
    let _e108 = sc4_[3u];
    sc = (_e105.xyz / vec3(_e108));
    let _e111 = sc;
    let _e115 = ((_e111.xy * 0.5f) + vec2(0.5f));
    sc[0u] = _e115.x;
    sc[1u] = _e115.y;
    let _e121 = sc[0u];
    let _e122 = (_e121 < 0f);
    phi_355_ = _e122;
    if !(_e122) {
        let _e125 = sc[0u];
        phi_355_ = (_e125 > 1f);
    }
    let _e128 = phi_355_;
    phi_362_ = _e128;
    if !(_e128) {
        let _e131 = sc[1u];
        phi_362_ = (_e131 < 0f);
    }
    let _e134 = phi_362_;
    phi_369_ = _e134;
    if !(_e134) {
        let _e137 = sc[1u];
        phi_369_ = (_e137 > 1f);
    }
    let _e140 = phi_369_;
    phi_376_ = _e140;
    if !(_e140) {
        let _e143 = sc[2u];
        phi_376_ = (_e143 > 1f);
    }
    let _e146 = phi_376_;
    if _e146 {
        return 1f;
    }
    let _e147 = (*c);
    layer = f32(_e147);
    let _e149 = textureDimensions(shadowMap, 0i);
    texelSize = (vec2(1f) / vec2<f32>(vec2<i32>(_e149).xy));
    let _e156 = sc[2u];
    currentDepth = (_e156 - shadow_bias);
    shadow = 0f;
    if override_type_6_ {
        let _e158 = currentDepth;
        let _e159 = sc;
        let _e160 = _e159.xy;
        let _e161 = layer;
        let _e164 = vec3<f32>(_e160.x, _e160.y, _e161);
        let _e170 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e164.x, _e164.y), i32(_e164.z));
        shadow = step(_e158, _e170.x);
    } else {
        if override_type_6_1 {
            let _e173 = currentDepth;
            let _e174 = sc;
            let _e175 = _e174.xy;
            let _e176 = layer;
            let _e179 = vec3<f32>(_e175.x, _e175.y, _e176);
            let _e185 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e179.x, _e179.y), i32(_e179.z));
            let _e188 = shadow;
            shadow = (_e188 + step(_e173, _e185.x));
            let _e190 = currentDepth;
            let _e191 = sc;
            let _e194 = texelSize[0u];
            let _e196 = (_e191.xy + vec2<f32>(_e194, 0f));
            let _e197 = layer;
            let _e200 = vec3<f32>(_e196.x, _e196.y, _e197);
            let _e206 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e200.x, _e200.y), i32(_e200.z));
            let _e209 = shadow;
            shadow = (_e209 + step(_e190, _e206.x));
            let _e211 = currentDepth;
            let _e212 = sc;
            let _e215 = texelSize[0u];
            let _e217 = (_e212.xy - vec2<f32>(_e215, 0f));
            let _e218 = layer;
            let _e221 = vec3<f32>(_e217.x, _e217.y, _e218);
            let _e227 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e221.x, _e221.y), i32(_e221.z));
            let _e230 = shadow;
            shadow = (_e230 + step(_e211, _e227.x));
            let _e232 = currentDepth;
            let _e233 = sc;
            let _e236 = texelSize[1u];
            let _e238 = (_e233.xy + vec2<f32>(0f, _e236));
            let _e239 = layer;
            let _e242 = vec3<f32>(_e238.x, _e238.y, _e239);
            let _e248 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e242.x, _e242.y), i32(_e242.z));
            let _e251 = shadow;
            shadow = (_e251 + step(_e232, _e248.x));
            let _e253 = currentDepth;
            let _e254 = sc;
            let _e257 = texelSize[1u];
            let _e259 = (_e254.xy - vec2<f32>(0f, _e257));
            let _e260 = layer;
            let _e263 = vec3<f32>(_e259.x, _e259.y, _e260);
            let _e269 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e263.x, _e263.y), i32(_e263.z));
            let _e272 = shadow;
            shadow = (_e272 + step(_e253, _e269.x));
            let _e274 = shadow;
            shadow = (_e274 / 5f);
        } else {
            x = -1i;
            loop {
                let _e276 = x;
                if (_e276 <= 1i) {
                    y = -1i;
                    loop {
                        let _e278 = y;
                        if (_e278 <= 1i) {
                            let _e280 = currentDepth;
                            let _e281 = sc;
                            let _e283 = x;
                            let _e285 = y;
                            let _e288 = texelSize;
                            let _e290 = (_e281.xy + (vec2<f32>(f32(_e283), f32(_e285)) * _e288));
                            let _e291 = layer;
                            let _e294 = vec3<f32>(_e290.x, _e290.y, _e291);
                            let _e300 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e294.x, _e294.y), i32(_e294.z));
                            let _e303 = shadow;
                            shadow = (_e303 + step(_e280, _e300.x));
                            continue;
                        } else {
                            break;
                        }
                        continuing {
                            let _e305 = y;
                            y = (_e305 + 1i);
                        }
                    }
                    continue;
                } else {
                    break;
                }
                continuing {
                    let _e307 = x;
                    x = (_e307 + 1i);
                }
            }
            let _e309 = shadow;
            shadow = (_e309 / 9f);
        }
    }
    let _e311 = shadow;
    return _e311;
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

    let _e102 = unnamed.cascadeSplits;
    let _e103 = (*viewDepth_1);
    cmp = step(_e102, vec4(_e103));
    let _e107 = cmp[0u];
    let _e109 = cmp[1u];
    let _e112 = cmp[2u];
    let _e115 = cmp[3u];
    cascade = min(i32((((_e107 + _e109) + _e112) + _e115)), 3i);
    let _e119 = cascade;
    (*outCascade) = _e119;
    let _e120 = cascade;
    if (_e120 == 0i) {
        local = 0f;
    } else {
        let _e122 = cascade;
        let _e127 = unnamed.cascadeSplits[max((_e122 - 1i), 0i)];
        local = _e127;
    }
    let _e128 = local;
    prevSplit = _e128;
    let _e129 = cascade;
    let _e132 = unnamed.cascadeSplits[_e129];
    farSplit = _e132;
    let _e133 = farSplit;
    let _e134 = prevSplit;
    blendRange = max((0.1f * (_e133 - _e134)), 1f);
    let _e138 = farSplit;
    let _e139 = (*viewDepth_1);
    let _e141 = blendRange;
    blendT = clamp(((_e138 - _e139) / _e141), 0f, 1f);
    let _e144 = cascade;
    param = _e144;
    let _e145 = (*worldPos_1);
    param_1 = _e145;
    let _e146 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param), (&param_1));
    s0_ = _e146;
    let _e147 = cascade;
    param_2 = min((_e147 + 1i), 3i);
    let _e150 = (*worldPos_1);
    param_3 = _e150;
    let _e151 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param_2), (&param_3));
    s1_ = _e151;
    let _e152 = s1_;
    let _e153 = s0_;
    let _e154 = blendT;
    return mix(_e152, _e153, _e154);
}

fn wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b(rgb: ptr<function, vec3<f32>>, lm_uv: ptr<function, vec2<f32>>) -> vec3<f32> {
    var sun_mask: f32;
    var shadow_1: f32;
    var ignoredCascade: i32;
    var param_4: vec3<f32>;
    var param_5: f32;
    var param_6: i32;

    let _e96 = unnamed.packed_indices[1i][3u];
    let _e102 = unnamed.packed_indices[1i][3u];
    let _e107 = (*lm_uv);
    let _e108 = textureSample(wired_bindless_images[(_e96 & 4095u)], wired_bindless_samplers[((_e102 >> bitcast<u32>(12i)) & 255u)], _e107);
    sun_mask = _e108.x;
    let _e110 = sun_mask;
    if (_e110 > 0.001f) {
        let _e112 = shadowData_1;
        param_4 = _e112.xyz;
        let _e115 = shadowData_1[3u];
        param_5 = _e115;
        let _e116 = sampleShadow_u0028_vf3_u003b_f1_u003b_i1_u003b((&param_4), (&param_5), (&param_6));
        let _e117 = param_6;
        ignoredCascade = _e117;
        shadow_1 = _e116;
        let _e118 = shadow_1;
        let _e119 = sun_mask;
        let _e121 = (*rgb);
        (*rgb) = (_e121 * mix(1f, _e118, _e119));
    }
    let _e123 = (*rgb);
    return _e123;
}

fn wired_apply_sun_shadow_operand_u0028_vf3_u003b(rgb_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var param_7: vec3<f32>;
    var param_8: vec2<f32>;
    var param_9: vec3<f32>;
    var param_10: vec2<f32>;
    var param_11: vec3<f32>;
    var param_12: vec2<f32>;

    if override_type_6_2 {
        let _e92 = (*rgb_1);
        param_7 = _e92;
        let _e93 = frag_tex_coord0_1;
        param_8 = _e93;
        let _e94 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_7), (&param_8));
        return _e94;
    }
    if override_type_6_3 {
        let _e95 = (*rgb_1);
        param_9 = _e95;
        let _e96 = frag_tex_coord1_1;
        param_10 = _e96;
        let _e97 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_9), (&param_10));
        return _e97;
    }
    if override_type_6_4 {
        let _e98 = (*rgb_1);
        param_11 = _e98;
        let _e99 = frag_tex_coord2_1;
        param_12 = _e99;
        let _e100 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_11), (&param_12));
        return _e100;
    }
    let _e101 = (*rgb_1);
    return _e101;
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb_2: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e88 = (*rgb_2);
    let _e91 = unnamed.worldLightParams[0u];
    boosted = (_e88 * _e91);
    let _e94 = boosted[0u];
    let _e96 = boosted[1u];
    let _e98 = boosted[2u];
    peak = max(_e94, max(_e96, _e98));
    let _e101 = peak;
    if (_e101 > 1f) {
        let _e103 = peak;
        let _e104 = boosted;
        boosted = (_e104 / vec3(_e103));
    }
    let _e107 = boosted;
    return _e107;
}

fn sRGBToLinear_u0028_vf3_u003b(c_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e89 = (*c_1);
    (*c_1) = max(_e89, vec3<f32>(0f, 0f, 0f));
    let _e91 = (*c_1);
    cutoff = (_e91 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e93 = (*c_1);
    lo = (_e93 / vec3(12.92f));
    let _e96 = (*c_1);
    hi = pow(((_e96 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e101 = hi;
    let _e102 = lo;
    let _e103 = cutoff;
    return mix(_e101, _e102, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e103));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_2: vec4<f32>;
    var param_13: vec3<f32>;
    var param_14: vec3<f32>;

    let _e91 = (*role);
    let _e93 = (*role);
    let _e98 = unnamed.packed_indices[(_e91 / 4u)][(_e93 % 4u)];
    let _e101 = (*role);
    let _e103 = (*role);
    let _e108 = unnamed.packed_indices[(_e101 / 4u)][(_e103 % 4u)];
    let _e113 = (*uv);
    let _e114 = textureSample(wired_bindless_images[(_e98 & 4095u)], wired_bindless_samplers[((_e108 >> bitcast<u32>(12i)) & 255u)], _e113);
    c_2 = _e114;
    let _e115 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e115))) == 0i) {
        let _e120 = c_2;
        param_13 = _e120.xyz;
        let _e122 = sRGBToLinear_u0028_vf3_u003b((&param_13));
        c_2[0u] = _e122.x;
        c_2[1u] = _e122.y;
        c_2[2u] = _e122.z;
    }
    let _e129 = (*slot);
    if (lightmap_slot == (_e129 + 1i)) {
        let _e132 = c_2;
        param_14 = _e132.xyz;
        let _e134 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_14));
        c_2[0u] = _e134.x;
        c_2[1u] = _e134.y;
        c_2[2u] = _e134.z;
    }
    let _e141 = c_2;
    return _e141;
}

fn main_1() {
    var frag_color0_: vec4<f32>;
    var param_15: vec3<f32>;
    var sampledColor0_: vec4<f32>;
    var param_16: u32;
    var param_17: vec2<f32>;
    var param_18: i32;
    var sampledColor1_: vec4<f32>;
    var param_19: u32;
    var param_20: vec2<f32>;
    var param_21: i32;
    var sampledColor2_: vec4<f32>;
    var param_22: u32;
    var param_23: vec2<f32>;
    var param_24: i32;
    var color0_: vec4<f32>;
    var color1_: vec4<f32>;
    var color2_: vec4<f32>;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var color2_1: vec4<f32>;
    var color1_2: vec4<f32>;
    var color2_2: vec4<f32>;
    var param_25: vec3<f32>;
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var fogAmount: f32;
    var param_26: f32;
    var param_27: vec4<f32>;

    let _e114 = frag_color0In_1;
    param_15 = _e114.xyz;
    let _e116 = sRGBToLinear_u0028_vf3_u003b((&param_15));
    let _e118 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e116.x, _e116.y, _e116.z, _e118);
    param_16 = 0u;
    let _e123 = frag_tex_coord0_1;
    param_17 = _e123;
    param_18 = 0i;
    let _e124 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_16), (&param_17), (&param_18));
    sampledColor0_ = _e124;
    param_19 = 1u;
    let _e125 = frag_tex_coord1_1;
    param_20 = _e125;
    param_21 = 1i;
    let _e126 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_19), (&param_20), (&param_21));
    sampledColor1_ = _e126;
    param_22 = 2u;
    let _e127 = frag_tex_coord2_1;
    param_23 = _e127;
    param_24 = 2i;
    let _e128 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_22), (&param_23), (&param_24));
    sampledColor2_ = _e128;
    let _e129 = sampledColor0_;
    let _e130 = frag_color0_;
    color0_ = (_e129 * _e130);
    if override_type_6_5 {
        let _e132 = sampledColor1_;
        color1_ = _e132;
        let _e133 = sampledColor2_;
        color2_ = _e133;
        let _e134 = color0_;
        let _e136 = color1_;
        let _e139 = color2_;
        let _e141 = ((_e134.xyz + _e136.xyz) + _e139.xyz);
        let _e143 = color0_[3u];
        let _e145 = color1_[3u];
        let _e148 = color2_[3u];
        base = vec4<f32>(_e141.x, _e141.y, _e141.z, ((_e143 * _e145) * _e148));
    } else {
        if override_type_6_6 {
            let _e154 = sampledColor1_;
            let _e155 = frag_color0_;
            color1_1 = (_e154 * _e155);
            let _e157 = sampledColor2_;
            let _e158 = frag_color0_;
            color2_1 = (_e157 * _e158);
            let _e160 = color0_;
            let _e162 = color1_1;
            let _e165 = color2_1;
            let _e167 = ((_e160.xyz + _e162.xyz) + _e165.xyz);
            let _e169 = color0_[3u];
            let _e171 = color1_1[3u];
            let _e174 = color2_1[3u];
            base = vec4<f32>(_e167.x, _e167.y, _e167.z, ((_e169 * _e171) * _e174));
        } else {
            let _e180 = sampledColor1_;
            color1_2 = _e180;
            let _e181 = sampledColor2_;
            color2_2 = _e181;
            let _e182 = color0_;
            let _e184 = color1_2;
            let _e187 = color2_2;
            let _e189 = ((_e182.xyz * _e184.xyz) * _e187.xyz);
            base[0u] = _e189.x;
            base[1u] = _e189.y;
            base[2u] = _e189.z;
            let _e197 = color0_[3u];
            let _e199 = color1_2[3u];
            let _e202 = color2_2[3u];
            base[3u] = ((_e197 * _e199) * _e202);
        }
    }
    let _e205 = base;
    param_25 = _e205.xyz;
    let _e207 = wired_apply_sun_shadow_operand_u0028_vf3_u003b((&param_25));
    base[0u] = _e207.x;
    base[1u] = _e207.y;
    base[2u] = _e207.z;
    if override_type_6_7 {
        let _e216 = unnamed.worldLightParams[1u];
        wetness = clamp(_e216, 0f, 1f);
        let _e220 = unnamed.worldLightParams[2u];
        frost = clamp(_e220, 0f, 1f);
        let _e222 = base;
        luminance = dot(_e222.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e225 = wetness;
        let _e227 = base;
        let _e229 = (_e227.xyz * mix(1f, 0.82f, _e225));
        base[0u] = _e229.x;
        base[1u] = _e229.y;
        base[2u] = _e229.z;
        let _e236 = base;
        let _e238 = luminance;
        let _e240 = luminance;
        let _e242 = luminance;
        let _e244 = frost;
        let _e247 = mix(_e236.xyz, vec3<f32>((_e238 * 0.88f), (_e240 * 0.94f), _e242), vec3((_e244 * 0.55f)));
        base[0u] = _e247.x;
        base[1u] = _e247.y;
        base[2u] = _e247.z;
    }
    let _e256 = unnamed.materialFactors[0u];
    if (_e256 >= 0f) {
        let _e259 = unnamed.materialBaseColor;
        let _e260 = base;
        base = (_e260 * _e259);
    }
    let _e262 = color0_;
    let _e265 = unnamed.emissionRadiance;
    let _e268 = base;
    let _e270 = (_e268.xyz + (_e262.xyz * _e265.xyz));
    base[0u] = _e270.x;
    base[1u] = _e270.y;
    base[2u] = _e270.z;
    let _e277 = wired_advanced_fog_enabled_u0028_();
    if _e277 {
        let _e278 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e278;
        let _e279 = base;
        let _e282 = unnamed.advancedFogColorDensity;
        let _e284 = fogAmount;
        let _e286 = mix(_e279.xyz, _e282.xyz, vec3(_e284));
        base[0u] = _e286.x;
        base[1u] = _e286.y;
        base[2u] = _e286.z;
    }
    if override_type_6_8 {
        let _e294 = base[3u];
        if (_e294 == 0f) {
            discard;
        }
    } else {
        if override_type_6_9 {
            let _e296 = base;
            let _e298 = base;
            if (dot(_e296.xyz, _e298.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e302 = base;
    out_color = _e302;
    param_26 = 1f;
    let _e303 = base;
    param_27 = _e303;
    wiredTemporalWriteAux_u0028_f1_u003b_vf4_u003b((&param_26), (&param_27));
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(7) shadowData: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(3) frag_tex_coord2_: vec2<f32>, @location(0) frag_color0In: vec4<f32>, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @location(12) @interpolate(flat) temporalOutcome: u32) -> FragmentOutput {
    gl_FragCoord_1 = gl_FragCoord;
    shadowData_1 = shadowData;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    frag_tex_coord2_1 = frag_tex_coord2_;
    frag_color0In_1 = frag_color0In;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    temporalOutcome_1 = temporalOutcome;
    main_1();
    let _e21 = out_temporal_velocity;
    let _e22 = out_temporal_validity;
    let _e23 = out_color;
    return FragmentOutput(_e21, _e22, _e23);
}
