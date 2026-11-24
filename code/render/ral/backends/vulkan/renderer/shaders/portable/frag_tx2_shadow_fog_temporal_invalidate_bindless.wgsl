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
@id(10) override acff: i32 = 0i;
override override_type_6_8: bool = (acff == 1i);
override override_type_6_9: bool = (acff == 2i);
override override_type_6_10: bool = (acff == 3i);
override override_type_6_11: bool = (acff == 1i);
override override_type_6_12: bool = (acff == 2i);
override override_type_6_13: bool = (acff == 3i);
@id(7) override discard_mode: i32 = 0i;
override override_type_6_14: bool = (discard_mode == 1i);
override override_type_6_15: bool = (discard_mode == 2i);
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
var<private> out_color: vec4<f32>;
var<private> temporalCurrentClip_1: vec4<f32>;
var<private> temporalPreviousClip_1: vec4<f32>;
var<private> temporalOutcome_1: u32;

fn wiredTemporalWriteAux_u0028_f1_u003b_vf4_u003b(coverageConfidence: ptr<function, f32>, sceneColor: ptr<function, vec4<f32>>) {
    out_temporal_velocity = vec2<f32>(0f, 0f);
    out_temporal_validity = vec4<f32>(0f, 0f, 0f, 0f);
    let _e97 = (*sceneColor)[0u];
    let _e99 = (*sceneColor)[1u];
    let _e101 = (*sceneColor)[2u];
    let _e106 = (*sceneColor)[3u];
    out_temporal_validity = vec4<f32>(clamp(max(_e97, max(_e99, _e101)), 0f, 1f), 0f, 0f, clamp(_e106, 0f, 1f));
    return;
}

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e97 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e97 + 0.5f));
    let _e102 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e104 = fogType;
    let _e107 = fogType;
    return (((_e102 > 0.5f) && (_e104 >= 1i)) && (_e107 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e97 = wired_advanced_fog_enabled_u0028_();
    if !(_e97) {
        return 0f;
    }
    let _e100 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e100, 0.000001f));
    let _e105 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e105 + 0.5f));
    let _e108 = fogType_1;
    if (_e108 == 1i) {
        let _e112 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e112 <= 0f) {
            return 0f;
        }
        let _e114 = viewDepth;
        let _e117 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e114 / _e117), 0f, 1f);
    }
    let _e122 = unnamed.advancedFogColorDensity[3u];
    let _e124 = viewDepth;
    opticalDepth = (max(_e122, 0f) * _e124);
    let _e126 = fogType_1;
    if (_e126 == 2i) {
        let _e128 = opticalDepth;
        return clamp((1f - exp(-(_e128))), 0f, 1f);
    }
    let _e133 = opticalDepth;
    let _e134 = opticalDepth;
    return clamp((1f - exp(-((_e133 * _e134)))), 0f, 1f);
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

    let _e104 = (*c);
    let _e107 = unnamed.cascadeMVP[_e104];
    let _e108 = (*worldPos);
    sc4_ = (_e107 * vec4<f32>(_e108.x, _e108.y, _e108.z, 1f));
    let _e114 = sc4_;
    let _e117 = sc4_[3u];
    sc = (_e114.xyz / vec3(_e117));
    let _e120 = sc;
    let _e124 = ((_e120.xy * 0.5f) + vec2(0.5f));
    sc[0u] = _e124.x;
    sc[1u] = _e124.y;
    let _e130 = sc[0u];
    let _e131 = (_e130 < 0f);
    phi_355_ = _e131;
    if !(_e131) {
        let _e134 = sc[0u];
        phi_355_ = (_e134 > 1f);
    }
    let _e137 = phi_355_;
    phi_362_ = _e137;
    if !(_e137) {
        let _e140 = sc[1u];
        phi_362_ = (_e140 < 0f);
    }
    let _e143 = phi_362_;
    phi_369_ = _e143;
    if !(_e143) {
        let _e146 = sc[1u];
        phi_369_ = (_e146 > 1f);
    }
    let _e149 = phi_369_;
    phi_376_ = _e149;
    if !(_e149) {
        let _e152 = sc[2u];
        phi_376_ = (_e152 > 1f);
    }
    let _e155 = phi_376_;
    if _e155 {
        return 1f;
    }
    let _e156 = (*c);
    layer = f32(_e156);
    let _e158 = textureDimensions(shadowMap, 0i);
    texelSize = (vec2(1f) / vec2<f32>(vec2<i32>(_e158).xy));
    let _e165 = sc[2u];
    currentDepth = (_e165 - shadow_bias);
    shadow = 0f;
    if override_type_6_ {
        let _e167 = currentDepth;
        let _e168 = sc;
        let _e169 = _e168.xy;
        let _e170 = layer;
        let _e173 = vec3<f32>(_e169.x, _e169.y, _e170);
        let _e179 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e173.x, _e173.y), i32(_e173.z));
        shadow = step(_e167, _e179.x);
    } else {
        if override_type_6_1 {
            let _e182 = currentDepth;
            let _e183 = sc;
            let _e184 = _e183.xy;
            let _e185 = layer;
            let _e188 = vec3<f32>(_e184.x, _e184.y, _e185);
            let _e194 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e188.x, _e188.y), i32(_e188.z));
            let _e197 = shadow;
            shadow = (_e197 + step(_e182, _e194.x));
            let _e199 = currentDepth;
            let _e200 = sc;
            let _e203 = texelSize[0u];
            let _e205 = (_e200.xy + vec2<f32>(_e203, 0f));
            let _e206 = layer;
            let _e209 = vec3<f32>(_e205.x, _e205.y, _e206);
            let _e215 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e209.x, _e209.y), i32(_e209.z));
            let _e218 = shadow;
            shadow = (_e218 + step(_e199, _e215.x));
            let _e220 = currentDepth;
            let _e221 = sc;
            let _e224 = texelSize[0u];
            let _e226 = (_e221.xy - vec2<f32>(_e224, 0f));
            let _e227 = layer;
            let _e230 = vec3<f32>(_e226.x, _e226.y, _e227);
            let _e236 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e230.x, _e230.y), i32(_e230.z));
            let _e239 = shadow;
            shadow = (_e239 + step(_e220, _e236.x));
            let _e241 = currentDepth;
            let _e242 = sc;
            let _e245 = texelSize[1u];
            let _e247 = (_e242.xy + vec2<f32>(0f, _e245));
            let _e248 = layer;
            let _e251 = vec3<f32>(_e247.x, _e247.y, _e248);
            let _e257 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e251.x, _e251.y), i32(_e251.z));
            let _e260 = shadow;
            shadow = (_e260 + step(_e241, _e257.x));
            let _e262 = currentDepth;
            let _e263 = sc;
            let _e266 = texelSize[1u];
            let _e268 = (_e263.xy - vec2<f32>(0f, _e266));
            let _e269 = layer;
            let _e272 = vec3<f32>(_e268.x, _e268.y, _e269);
            let _e278 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e272.x, _e272.y), i32(_e272.z));
            let _e281 = shadow;
            shadow = (_e281 + step(_e262, _e278.x));
            let _e283 = shadow;
            shadow = (_e283 / 5f);
        } else {
            x = -1i;
            loop {
                let _e285 = x;
                if (_e285 <= 1i) {
                    y = -1i;
                    loop {
                        let _e287 = y;
                        if (_e287 <= 1i) {
                            let _e289 = currentDepth;
                            let _e290 = sc;
                            let _e292 = x;
                            let _e294 = y;
                            let _e297 = texelSize;
                            let _e299 = (_e290.xy + (vec2<f32>(f32(_e292), f32(_e294)) * _e297));
                            let _e300 = layer;
                            let _e303 = vec3<f32>(_e299.x, _e299.y, _e300);
                            let _e309 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e303.x, _e303.y), i32(_e303.z));
                            let _e312 = shadow;
                            shadow = (_e312 + step(_e289, _e309.x));
                            continue;
                        } else {
                            break;
                        }
                        continuing {
                            let _e314 = y;
                            y = (_e314 + 1i);
                        }
                    }
                    continue;
                } else {
                    break;
                }
                continuing {
                    let _e316 = x;
                    x = (_e316 + 1i);
                }
            }
            let _e318 = shadow;
            shadow = (_e318 / 9f);
        }
    }
    let _e320 = shadow;
    return _e320;
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

    let _e111 = unnamed.cascadeSplits;
    let _e112 = (*viewDepth_1);
    cmp = step(_e111, vec4(_e112));
    let _e116 = cmp[0u];
    let _e118 = cmp[1u];
    let _e121 = cmp[2u];
    let _e124 = cmp[3u];
    cascade = min(i32((((_e116 + _e118) + _e121) + _e124)), 3i);
    let _e128 = cascade;
    (*outCascade) = _e128;
    let _e129 = cascade;
    if (_e129 == 0i) {
        local = 0f;
    } else {
        let _e131 = cascade;
        let _e136 = unnamed.cascadeSplits[max((_e131 - 1i), 0i)];
        local = _e136;
    }
    let _e137 = local;
    prevSplit = _e137;
    let _e138 = cascade;
    let _e141 = unnamed.cascadeSplits[_e138];
    farSplit = _e141;
    let _e142 = farSplit;
    let _e143 = prevSplit;
    blendRange = max((0.1f * (_e142 - _e143)), 1f);
    let _e147 = farSplit;
    let _e148 = (*viewDepth_1);
    let _e150 = blendRange;
    blendT = clamp(((_e147 - _e148) / _e150), 0f, 1f);
    let _e153 = cascade;
    param = _e153;
    let _e154 = (*worldPos_1);
    param_1 = _e154;
    let _e155 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param), (&param_1));
    s0_ = _e155;
    let _e156 = cascade;
    param_2 = min((_e156 + 1i), 3i);
    let _e159 = (*worldPos_1);
    param_3 = _e159;
    let _e160 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param_2), (&param_3));
    s1_ = _e160;
    let _e161 = s1_;
    let _e162 = s0_;
    let _e163 = blendT;
    return mix(_e161, _e162, _e163);
}

fn wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b(rgb: ptr<function, vec3<f32>>, lm_uv: ptr<function, vec2<f32>>) -> vec3<f32> {
    var sun_mask: f32;
    var shadow_1: f32;
    var ignoredCascade: i32;
    var param_4: vec3<f32>;
    var param_5: f32;
    var param_6: i32;

    let _e105 = unnamed.packed_indices[1i][3u];
    let _e111 = unnamed.packed_indices[1i][3u];
    let _e116 = (*lm_uv);
    let _e117 = textureSample(wired_bindless_images[(_e105 & 4095u)], wired_bindless_samplers[((_e111 >> bitcast<u32>(12i)) & 255u)], _e116);
    sun_mask = _e117.x;
    let _e119 = sun_mask;
    if (_e119 > 0.001f) {
        let _e121 = shadowData_1;
        param_4 = _e121.xyz;
        let _e124 = shadowData_1[3u];
        param_5 = _e124;
        let _e125 = sampleShadow_u0028_vf3_u003b_f1_u003b_i1_u003b((&param_4), (&param_5), (&param_6));
        let _e126 = param_6;
        ignoredCascade = _e126;
        shadow_1 = _e125;
        let _e127 = shadow_1;
        let _e128 = sun_mask;
        let _e130 = (*rgb);
        (*rgb) = (_e130 * mix(1f, _e127, _e128));
    }
    let _e132 = (*rgb);
    return _e132;
}

fn wired_apply_sun_shadow_operand_u0028_vf3_u003b(rgb_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var param_7: vec3<f32>;
    var param_8: vec2<f32>;
    var param_9: vec3<f32>;
    var param_10: vec2<f32>;
    var param_11: vec3<f32>;
    var param_12: vec2<f32>;

    if override_type_6_2 {
        let _e101 = (*rgb_1);
        param_7 = _e101;
        let _e102 = frag_tex_coord0_1;
        param_8 = _e102;
        let _e103 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_7), (&param_8));
        return _e103;
    }
    if override_type_6_3 {
        let _e104 = (*rgb_1);
        param_9 = _e104;
        let _e105 = frag_tex_coord1_1;
        param_10 = _e105;
        let _e106 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_9), (&param_10));
        return _e106;
    }
    if override_type_6_4 {
        let _e107 = (*rgb_1);
        param_11 = _e107;
        let _e108 = frag_tex_coord2_1;
        param_12 = _e108;
        let _e109 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_11), (&param_12));
        return _e109;
    }
    let _e110 = (*rgb_1);
    return _e110;
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb_2: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e97 = (*rgb_2);
    let _e100 = unnamed.worldLightParams[0u];
    boosted = (_e97 * _e100);
    let _e103 = boosted[0u];
    let _e105 = boosted[1u];
    let _e107 = boosted[2u];
    peak = max(_e103, max(_e105, _e107));
    let _e110 = peak;
    if (_e110 > 1f) {
        let _e112 = peak;
        let _e113 = boosted;
        boosted = (_e113 / vec3(_e112));
    }
    let _e116 = boosted;
    return _e116;
}

fn sRGBToLinear_u0028_vf3_u003b(c_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e98 = (*c_1);
    (*c_1) = max(_e98, vec3<f32>(0f, 0f, 0f));
    let _e100 = (*c_1);
    cutoff = (_e100 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e102 = (*c_1);
    lo = (_e102 / vec3(12.92f));
    let _e105 = (*c_1);
    hi = pow(((_e105 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e110 = hi;
    let _e111 = lo;
    let _e112 = cutoff;
    return mix(_e110, _e111, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e112));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_2: vec4<f32>;
    var param_13: vec3<f32>;
    var param_14: vec3<f32>;

    let _e100 = (*role);
    let _e102 = (*role);
    let _e107 = unnamed.packed_indices[(_e100 / 4u)][(_e102 % 4u)];
    let _e110 = (*role);
    let _e112 = (*role);
    let _e117 = unnamed.packed_indices[(_e110 / 4u)][(_e112 % 4u)];
    let _e122 = (*uv);
    let _e123 = textureSample(wired_bindless_images[(_e107 & 4095u)], wired_bindless_samplers[((_e117 >> bitcast<u32>(12i)) & 255u)], _e122);
    c_2 = _e123;
    let _e124 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e124))) == 0i) {
        let _e129 = c_2;
        param_13 = _e129.xyz;
        let _e131 = sRGBToLinear_u0028_vf3_u003b((&param_13));
        c_2[0u] = _e131.x;
        c_2[1u] = _e131.y;
        c_2[2u] = _e131.z;
    }
    let _e138 = (*slot);
    if (lightmap_slot == (_e138 + 1i)) {
        let _e141 = c_2;
        param_14 = _e141.xyz;
        let _e143 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_14));
        c_2[0u] = _e143.x;
        c_2[1u] = _e143.y;
        c_2[2u] = _e143.z;
    }
    let _e150 = c_2;
    return _e150;
}

fn main_1() {
    var fog: vec4<f32>;
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

    let _e127 = unnamed.packed_indices[0i][3u];
    let _e133 = unnamed.packed_indices[0i][3u];
    let _e138 = fog_tex_coord_1;
    let _e139 = textureSample(wired_bindless_images[(_e127 & 4095u)], wired_bindless_samplers[((_e133 >> bitcast<u32>(12i)) & 255u)], _e138);
    fog = _e139;
    let _e140 = frag_color0In_1;
    param_15 = _e140.xyz;
    let _e142 = sRGBToLinear_u0028_vf3_u003b((&param_15));
    let _e144 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e142.x, _e142.y, _e142.z, _e144);
    param_16 = 0u;
    let _e149 = frag_tex_coord0_1;
    param_17 = _e149;
    param_18 = 0i;
    let _e150 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_16), (&param_17), (&param_18));
    sampledColor0_ = _e150;
    param_19 = 1u;
    let _e151 = frag_tex_coord1_1;
    param_20 = _e151;
    param_21 = 1i;
    let _e152 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_19), (&param_20), (&param_21));
    sampledColor1_ = _e152;
    param_22 = 2u;
    let _e153 = frag_tex_coord2_1;
    param_23 = _e153;
    param_24 = 2i;
    let _e154 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_22), (&param_23), (&param_24));
    sampledColor2_ = _e154;
    let _e155 = sampledColor0_;
    let _e156 = frag_color0_;
    color0_ = (_e155 * _e156);
    if override_type_6_5 {
        let _e158 = sampledColor1_;
        color1_ = _e158;
        let _e159 = sampledColor2_;
        color2_ = _e159;
        let _e160 = color0_;
        let _e162 = color1_;
        let _e165 = color2_;
        let _e167 = ((_e160.xyz + _e162.xyz) + _e165.xyz);
        let _e169 = color0_[3u];
        let _e171 = color1_[3u];
        let _e174 = color2_[3u];
        base = vec4<f32>(_e167.x, _e167.y, _e167.z, ((_e169 * _e171) * _e174));
    } else {
        if override_type_6_6 {
            let _e180 = sampledColor1_;
            let _e181 = frag_color0_;
            color1_1 = (_e180 * _e181);
            let _e183 = sampledColor2_;
            let _e184 = frag_color0_;
            color2_1 = (_e183 * _e184);
            let _e186 = color0_;
            let _e188 = color1_1;
            let _e191 = color2_1;
            let _e193 = ((_e186.xyz + _e188.xyz) + _e191.xyz);
            let _e195 = color0_[3u];
            let _e197 = color1_1[3u];
            let _e200 = color2_1[3u];
            base = vec4<f32>(_e193.x, _e193.y, _e193.z, ((_e195 * _e197) * _e200));
        } else {
            let _e206 = sampledColor1_;
            color1_2 = _e206;
            let _e207 = sampledColor2_;
            color2_2 = _e207;
            let _e208 = color0_;
            let _e210 = color1_2;
            let _e213 = color2_2;
            let _e215 = ((_e208.xyz * _e210.xyz) * _e213.xyz);
            base[0u] = _e215.x;
            base[1u] = _e215.y;
            base[2u] = _e215.z;
            let _e223 = color0_[3u];
            let _e225 = color1_2[3u];
            let _e228 = color2_2[3u];
            base[3u] = ((_e223 * _e225) * _e228);
        }
    }
    let _e231 = base;
    param_25 = _e231.xyz;
    let _e233 = wired_apply_sun_shadow_operand_u0028_vf3_u003b((&param_25));
    base[0u] = _e233.x;
    base[1u] = _e233.y;
    base[2u] = _e233.z;
    if override_type_6_7 {
        let _e242 = unnamed.worldLightParams[1u];
        wetness = clamp(_e242, 0f, 1f);
        let _e246 = unnamed.worldLightParams[2u];
        frost = clamp(_e246, 0f, 1f);
        let _e248 = base;
        luminance = dot(_e248.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e251 = wetness;
        let _e253 = base;
        let _e255 = (_e253.xyz * mix(1f, 0.82f, _e251));
        base[0u] = _e255.x;
        base[1u] = _e255.y;
        base[2u] = _e255.z;
        let _e262 = base;
        let _e264 = luminance;
        let _e266 = luminance;
        let _e268 = luminance;
        let _e270 = frost;
        let _e273 = mix(_e262.xyz, vec3<f32>((_e264 * 0.88f), (_e266 * 0.94f), _e268), vec3((_e270 * 0.55f)));
        base[0u] = _e273.x;
        base[1u] = _e273.y;
        base[2u] = _e273.z;
    }
    let _e282 = unnamed.materialFactors[0u];
    if (_e282 >= 0f) {
        let _e285 = unnamed.materialBaseColor;
        let _e286 = base;
        base = (_e286 * _e285);
    }
    let _e288 = color0_;
    let _e291 = unnamed.emissionRadiance;
    let _e294 = base;
    let _e296 = (_e294.xyz + (_e288.xyz * _e291.xyz));
    base[0u] = _e296.x;
    base[1u] = _e296.y;
    base[2u] = _e296.z;
    let _e303 = wired_advanced_fog_enabled_u0028_();
    if _e303 {
        let _e304 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e304;
        if override_type_6_8 {
            let _e305 = fogAmount;
            let _e307 = base;
            let _e309 = (_e307.xyz * (1f - _e305));
            base[0u] = _e309.x;
            base[1u] = _e309.y;
            base[2u] = _e309.z;
        } else {
            if override_type_6_9 {
                let _e316 = fogAmount;
                let _e318 = base;
                base = (_e318 * (1f - _e316));
            } else {
                if override_type_6_10 {
                    let _e320 = fogAmount;
                    let _e323 = base[3u];
                    base[3u] = (_e323 * (1f - _e320));
                } else {
                    let _e326 = base;
                    let _e329 = unnamed.advancedFogColorDensity;
                    let _e331 = fogAmount;
                    let _e333 = mix(_e326.xyz, _e329.xyz, vec3(_e331));
                    base[0u] = _e333.x;
                    base[1u] = _e333.y;
                    base[2u] = _e333.z;
                }
            }
        }
    } else {
        if override_type_6_11 {
            let _e340 = base;
            let _e343 = fog[3u];
            let _e345 = (_e340.xyz * (1f - _e343));
            base[0u] = _e345.x;
            base[1u] = _e345.y;
            base[2u] = _e345.z;
        } else {
            if override_type_6_12 {
                let _e352 = base;
                let _e354 = fog[3u];
                base = (_e352 * (1f - _e354));
            } else {
                if override_type_6_13 {
                    let _e358 = base[3u];
                    let _e360 = fog[3u];
                    base[3u] = (_e358 * (1f - _e360));
                } else {
                    let _e364 = base;
                    let _e365 = fog;
                    let _e367 = unnamed.fogColor;
                    let _e370 = fog[3u];
                    base = mix(_e364, (_e365 * _e367), vec4(_e370));
                }
            }
        }
    }
    if override_type_6_14 {
        let _e374 = base[3u];
        if (_e374 == 0f) {
            discard;
        }
    } else {
        if override_type_6_15 {
            let _e376 = base;
            let _e378 = base;
            if (dot(_e376.xyz, _e378.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e382 = base;
    out_color = _e382;
    param_26 = 1f;
    let _e383 = base;
    param_27 = _e383;
    wiredTemporalWriteAux_u0028_f1_u003b_vf4_u003b((&param_26), (&param_27));
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(7) shadowData: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(3) frag_tex_coord2_: vec2<f32>, @location(4) fog_tex_coord: vec2<f32>, @location(0) frag_color0In: vec4<f32>, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @location(12) @interpolate(flat) temporalOutcome: u32) -> FragmentOutput {
    gl_FragCoord_1 = gl_FragCoord;
    shadowData_1 = shadowData;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    frag_tex_coord2_1 = frag_tex_coord2_;
    fog_tex_coord_1 = fog_tex_coord;
    frag_color0In_1 = frag_color0In;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    temporalOutcome_1 = temporalOutcome;
    main_1();
    let _e23 = out_temporal_velocity;
    let _e24 = out_temporal_validity;
    let _e25 = out_color;
    return FragmentOutput(_e23, _e24, _e25);
}
