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
    var phi_331_: bool;
    var phi_338_: bool;
    var phi_345_: bool;
    var phi_352_: bool;

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
    phi_331_ = _e131;
    if !(_e131) {
        let _e134 = sc[0u];
        phi_331_ = (_e134 > 1f);
    }
    let _e137 = phi_331_;
    phi_338_ = _e137;
    if !(_e137) {
        let _e140 = sc[1u];
        phi_338_ = (_e140 < 0f);
    }
    let _e143 = phi_338_;
    phi_345_ = _e143;
    if !(_e143) {
        let _e146 = sc[1u];
        phi_345_ = (_e146 > 1f);
    }
    let _e149 = phi_345_;
    phi_352_ = _e149;
    if !(_e149) {
        let _e152 = sc[2u];
        phi_352_ = (_e152 > 1f);
    }
    let _e155 = phi_352_;
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
    if override_type_3_ {
        let _e167 = currentDepth;
        let _e168 = sc;
        let _e169 = _e168.xy;
        let _e170 = layer;
        let _e173 = vec3<f32>(_e169.x, _e169.y, _e170);
        let _e179 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e173.x, _e173.y), i32(_e173.z));
        shadow = step(_e167, _e179.x);
    } else {
        if override_type_3_1 {
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

    if override_type_3_2 {
        let _e99 = (*rgb_1);
        param_7 = _e99;
        let _e100 = frag_tex_coord0_1;
        param_8 = _e100;
        let _e101 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_7), (&param_8));
        return _e101;
    }
    if override_type_3_3 {
        let _e102 = (*rgb_1);
        param_9 = _e102;
        let _e103 = frag_tex_coord1_1;
        param_10 = _e103;
        let _e104 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_9), (&param_10));
        return _e104;
    }
    let _e105 = (*rgb_1);
    return _e105;
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
    var param_11: vec3<f32>;
    var param_12: vec3<f32>;

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
        param_11 = _e129.xyz;
        let _e131 = sRGBToLinear_u0028_vf3_u003b((&param_11));
        c_2[0u] = _e131.x;
        c_2[1u] = _e131.y;
        c_2[2u] = _e131.z;
    }
    let _e138 = (*slot);
    if (lightmap_slot == (_e138 + 1i)) {
        let _e141 = c_2;
        param_12 = _e141.xyz;
        let _e143 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_12));
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
    var param_13: vec3<f32>;
    var frag_color1_: vec4<f32>;
    var param_14: vec3<f32>;
    var sampledColor0_: vec4<f32>;
    var param_15: u32;
    var param_16: vec2<f32>;
    var param_17: i32;
    var sampledColor1_: vec4<f32>;
    var param_18: u32;
    var param_19: vec2<f32>;
    var param_20: i32;
    var color0_: vec4<f32>;
    var color1_: vec4<f32>;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var color1_2: vec4<f32>;
    var color1_3: vec4<f32>;
    var color1_4: vec4<f32>;
    var color1_5: vec4<f32>;
    var color1_6: vec4<f32>;
    var param_21: vec3<f32>;
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var fogAmount: f32;

    let _e124 = unnamed.packed_indices[0i][3u];
    let _e130 = unnamed.packed_indices[0i][3u];
    let _e135 = fog_tex_coord_1;
    let _e136 = textureSample(wired_bindless_images[(_e124 & 4095u)], wired_bindless_samplers[((_e130 >> bitcast<u32>(12i)) & 255u)], _e135);
    fog = _e136;
    let _e137 = frag_color0In_1;
    param_13 = _e137.xyz;
    let _e139 = sRGBToLinear_u0028_vf3_u003b((&param_13));
    let _e141 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e139.x, _e139.y, _e139.z, _e141);
    let _e146 = frag_color1In_1;
    param_14 = _e146.xyz;
    let _e148 = sRGBToLinear_u0028_vf3_u003b((&param_14));
    let _e150 = frag_color1In_1[3u];
    frag_color1_ = vec4<f32>(_e148.x, _e148.y, _e148.z, _e150);
    param_15 = 0u;
    let _e155 = frag_tex_coord0_1;
    param_16 = _e155;
    param_17 = 0i;
    let _e156 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_15), (&param_16), (&param_17));
    sampledColor0_ = _e156;
    param_18 = 1u;
    let _e157 = frag_tex_coord1_1;
    param_19 = _e157;
    param_20 = 1i;
    let _e158 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_18), (&param_19), (&param_20));
    sampledColor1_ = _e158;
    let _e159 = sampledColor0_;
    let _e160 = frag_color0_;
    color0_ = (_e159 * _e160);
    if override_type_3_6 {
        let _e162 = sampledColor1_;
        let _e163 = frag_color1_;
        color1_ = (_e162 * _e163);
        let _e165 = color0_;
        let _e167 = color1_;
        let _e169 = (_e165.xyz + _e167.xyz);
        let _e171 = color0_[3u];
        let _e173 = color1_[3u];
        base = vec4<f32>(_e169.x, _e169.y, _e169.z, (_e171 * _e173));
    } else {
        if override_type_3_7 {
            let _e179 = sampledColor1_;
            let _e180 = frag_color1_;
            color1_1 = (_e179 * _e180);
            let _e183 = color0_[3u];
            let _e184 = color0_;
            color0_ = (_e184 * _e183);
            let _e187 = color1_1[3u];
            let _e188 = color1_1;
            color1_1 = (_e188 * _e187);
            let _e190 = color0_;
            let _e192 = color1_1;
            let _e194 = (_e190.xyz + _e192.xyz);
            let _e196 = color0_[3u];
            let _e198 = color1_1[3u];
            base = vec4<f32>(_e194.x, _e194.y, _e194.z, (_e196 * _e198));
        } else {
            if override_type_3_8 {
                let _e204 = sampledColor1_;
                let _e205 = frag_color1_;
                color1_2 = (_e204 * _e205);
                let _e208 = color0_[3u];
                let _e210 = color0_;
                color0_ = (_e210 * (1f - _e208));
                let _e213 = color1_2[3u];
                let _e215 = color1_2;
                color1_2 = (_e215 * (1f - _e213));
                let _e217 = color0_;
                let _e219 = color1_2;
                let _e221 = (_e217.xyz + _e219.xyz);
                let _e223 = color0_[3u];
                let _e225 = color1_2[3u];
                base = vec4<f32>(_e221.x, _e221.y, _e221.z, (_e223 * _e225));
            } else {
                if override_type_3_9 {
                    let _e231 = sampledColor1_;
                    let _e232 = frag_color1_;
                    color1_3 = (_e231 * _e232);
                    let _e234 = color0_;
                    let _e235 = color1_3;
                    let _e237 = color1_3[3u];
                    base = mix(_e234, _e235, vec4(_e237));
                } else {
                    if override_type_3_10 {
                        let _e240 = sampledColor1_;
                        let _e241 = frag_color1_;
                        color1_4 = (_e240 * _e241);
                        let _e243 = color1_4;
                        let _e244 = color0_;
                        let _e246 = color1_4[3u];
                        base = mix(_e243, _e244, vec4(_e246));
                    } else {
                        if override_type_3_11 {
                            let _e249 = sampledColor1_;
                            let _e250 = frag_color1_;
                            color1_5 = (_e249 * _e250);
                            let _e252 = color1_5;
                            let _e254 = color1_5[3u];
                            let _e257 = color0_;
                            base = ((_e252 + vec4(_e254)) * _e257);
                        } else {
                            let _e259 = sampledColor1_;
                            let _e260 = frag_color1_;
                            color1_6 = (_e259 * _e260);
                            let _e262 = color0_;
                            let _e264 = color1_6;
                            let _e266 = (_e262.xyz * _e264.xyz);
                            base[0u] = _e266.x;
                            base[1u] = _e266.y;
                            base[2u] = _e266.z;
                            let _e274 = color0_[3u];
                            let _e276 = color1_6[3u];
                            base[3u] = (_e274 * _e276);
                        }
                    }
                }
            }
        }
    }
    let _e279 = base;
    param_21 = _e279.xyz;
    let _e281 = wired_apply_sun_shadow_operand_u0028_vf3_u003b((&param_21));
    base[0u] = _e281.x;
    base[1u] = _e281.y;
    base[2u] = _e281.z;
    if override_type_3_12 {
        let _e290 = unnamed.worldLightParams[1u];
        wetness = clamp(_e290, 0f, 1f);
        let _e294 = unnamed.worldLightParams[2u];
        frost = clamp(_e294, 0f, 1f);
        let _e296 = base;
        luminance = dot(_e296.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e299 = wetness;
        let _e301 = base;
        let _e303 = (_e301.xyz * mix(1f, 0.82f, _e299));
        base[0u] = _e303.x;
        base[1u] = _e303.y;
        base[2u] = _e303.z;
        let _e310 = base;
        let _e312 = luminance;
        let _e314 = luminance;
        let _e316 = luminance;
        let _e318 = frost;
        let _e321 = mix(_e310.xyz, vec3<f32>((_e312 * 0.88f), (_e314 * 0.94f), _e316), vec3((_e318 * 0.55f)));
        base[0u] = _e321.x;
        base[1u] = _e321.y;
        base[2u] = _e321.z;
    }
    let _e330 = unnamed.materialFactors[0u];
    if (_e330 >= 0f) {
        let _e333 = unnamed.materialBaseColor;
        let _e334 = base;
        base = (_e334 * _e333);
    }
    let _e336 = color0_;
    let _e339 = unnamed.emissionRadiance;
    let _e342 = base;
    let _e344 = (_e342.xyz + (_e336.xyz * _e339.xyz));
    base[0u] = _e344.x;
    base[1u] = _e344.y;
    base[2u] = _e344.z;
    let _e351 = wired_advanced_fog_enabled_u0028_();
    if _e351 {
        let _e352 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e352;
        if override_type_3_13 {
            let _e353 = fogAmount;
            let _e355 = base;
            let _e357 = (_e355.xyz * (1f - _e353));
            base[0u] = _e357.x;
            base[1u] = _e357.y;
            base[2u] = _e357.z;
        } else {
            if override_type_3_14 {
                let _e364 = fogAmount;
                let _e366 = base;
                base = (_e366 * (1f - _e364));
            } else {
                if override_type_3_15 {
                    let _e368 = fogAmount;
                    let _e371 = base[3u];
                    base[3u] = (_e371 * (1f - _e368));
                } else {
                    let _e374 = base;
                    let _e377 = unnamed.advancedFogColorDensity;
                    let _e379 = fogAmount;
                    let _e381 = mix(_e374.xyz, _e377.xyz, vec3(_e379));
                    base[0u] = _e381.x;
                    base[1u] = _e381.y;
                    base[2u] = _e381.z;
                }
            }
        }
    } else {
        if override_type_3_16 {
            let _e388 = base;
            let _e391 = fog[3u];
            let _e393 = (_e388.xyz * (1f - _e391));
            base[0u] = _e393.x;
            base[1u] = _e393.y;
            base[2u] = _e393.z;
        } else {
            if override_type_3_17 {
                let _e400 = base;
                let _e402 = fog[3u];
                base = (_e400 * (1f - _e402));
            } else {
                if override_type_3_18 {
                    let _e406 = base[3u];
                    let _e408 = fog[3u];
                    base[3u] = (_e406 * (1f - _e408));
                } else {
                    let _e412 = base;
                    let _e413 = fog;
                    let _e415 = unnamed.fogColor;
                    let _e418 = fog[3u];
                    base = mix(_e412, (_e413 * _e415), vec4(_e418));
                }
            }
        }
    }
    if override_type_3_19 {
        let _e422 = base[3u];
        if (_e422 == 0f) {
            discard;
        }
    } else {
        if override_type_3_20 {
            let _e424 = base;
            let _e426 = base;
            if (dot(_e424.xyz, _e426.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e430 = base;
    out_color = _e430;
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(7) shadowData: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(4) fog_tex_coord: vec2<f32>, @location(0) frag_color0In: vec4<f32>, @location(5) frag_color1In: vec4<f32>) -> @location(0) vec4<f32> {
    gl_FragCoord_1 = gl_FragCoord;
    shadowData_1 = shadowData;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    fog_tex_coord_1 = fog_tex_coord;
    frag_color0In_1 = frag_color0In;
    frag_color1In_1 = frag_color1In;
    main_1();
    let _e15 = out_color;
    return _e15;
}
