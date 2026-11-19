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
@id(7) override discard_mode: i32 = 0i;
override override_type_3_13: bool = (discard_mode == 1i);
override override_type_3_14: bool = (discard_mode == 2i);
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
var<private> frag_color0In_1: vec4<f32>;
var<private> frag_color1In_1: vec4<f32>;
var<private> out_color: vec4<f32>;

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e89 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e89 + 0.5f));
    let _e94 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e96 = fogType;
    let _e99 = fogType;
    return (((_e94 > 0.5f) && (_e96 >= 1i)) && (_e99 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e89 = wired_advanced_fog_enabled_u0028_();
    if !(_e89) {
        return 0f;
    }
    let _e92 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e92, 0.000001f));
    let _e97 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e97 + 0.5f));
    let _e100 = fogType_1;
    if (_e100 == 1i) {
        let _e104 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e104 <= 0f) {
            return 0f;
        }
        let _e106 = viewDepth;
        let _e109 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e106 / _e109), 0f, 1f);
    }
    let _e114 = unnamed.advancedFogColorDensity[3u];
    let _e116 = viewDepth;
    opticalDepth = (max(_e114, 0f) * _e116);
    let _e118 = fogType_1;
    if (_e118 == 2i) {
        let _e120 = opticalDepth;
        return clamp((1f - exp(-(_e120))), 0f, 1f);
    }
    let _e125 = opticalDepth;
    let _e126 = opticalDepth;
    return clamp((1f - exp(-((_e125 * _e126)))), 0f, 1f);
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

    let _e96 = (*c);
    let _e99 = unnamed.cascadeMVP[_e96];
    let _e100 = (*worldPos);
    sc4_ = (_e99 * vec4<f32>(_e100.x, _e100.y, _e100.z, 1f));
    let _e106 = sc4_;
    let _e109 = sc4_[3u];
    sc = (_e106.xyz / vec3(_e109));
    let _e112 = sc;
    let _e116 = ((_e112.xy * 0.5f) + vec2(0.5f));
    sc[0u] = _e116.x;
    sc[1u] = _e116.y;
    let _e122 = sc[0u];
    let _e123 = (_e122 < 0f);
    phi_331_ = _e123;
    if !(_e123) {
        let _e126 = sc[0u];
        phi_331_ = (_e126 > 1f);
    }
    let _e129 = phi_331_;
    phi_338_ = _e129;
    if !(_e129) {
        let _e132 = sc[1u];
        phi_338_ = (_e132 < 0f);
    }
    let _e135 = phi_338_;
    phi_345_ = _e135;
    if !(_e135) {
        let _e138 = sc[1u];
        phi_345_ = (_e138 > 1f);
    }
    let _e141 = phi_345_;
    phi_352_ = _e141;
    if !(_e141) {
        let _e144 = sc[2u];
        phi_352_ = (_e144 > 1f);
    }
    let _e147 = phi_352_;
    if _e147 {
        return 1f;
    }
    let _e148 = (*c);
    layer = f32(_e148);
    let _e150 = textureDimensions(shadowMap, 0i);
    texelSize = (vec2(1f) / vec2<f32>(vec2<i32>(_e150).xy));
    let _e157 = sc[2u];
    currentDepth = (_e157 - shadow_bias);
    shadow = 0f;
    if override_type_3_ {
        let _e159 = currentDepth;
        let _e160 = sc;
        let _e161 = _e160.xy;
        let _e162 = layer;
        let _e165 = vec3<f32>(_e161.x, _e161.y, _e162);
        let _e171 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e165.x, _e165.y), i32(_e165.z));
        shadow = step(_e159, _e171.x);
    } else {
        if override_type_3_1 {
            let _e174 = currentDepth;
            let _e175 = sc;
            let _e176 = _e175.xy;
            let _e177 = layer;
            let _e180 = vec3<f32>(_e176.x, _e176.y, _e177);
            let _e186 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e180.x, _e180.y), i32(_e180.z));
            let _e189 = shadow;
            shadow = (_e189 + step(_e174, _e186.x));
            let _e191 = currentDepth;
            let _e192 = sc;
            let _e195 = texelSize[0u];
            let _e197 = (_e192.xy + vec2<f32>(_e195, 0f));
            let _e198 = layer;
            let _e201 = vec3<f32>(_e197.x, _e197.y, _e198);
            let _e207 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e201.x, _e201.y), i32(_e201.z));
            let _e210 = shadow;
            shadow = (_e210 + step(_e191, _e207.x));
            let _e212 = currentDepth;
            let _e213 = sc;
            let _e216 = texelSize[0u];
            let _e218 = (_e213.xy - vec2<f32>(_e216, 0f));
            let _e219 = layer;
            let _e222 = vec3<f32>(_e218.x, _e218.y, _e219);
            let _e228 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e222.x, _e222.y), i32(_e222.z));
            let _e231 = shadow;
            shadow = (_e231 + step(_e212, _e228.x));
            let _e233 = currentDepth;
            let _e234 = sc;
            let _e237 = texelSize[1u];
            let _e239 = (_e234.xy + vec2<f32>(0f, _e237));
            let _e240 = layer;
            let _e243 = vec3<f32>(_e239.x, _e239.y, _e240);
            let _e249 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e243.x, _e243.y), i32(_e243.z));
            let _e252 = shadow;
            shadow = (_e252 + step(_e233, _e249.x));
            let _e254 = currentDepth;
            let _e255 = sc;
            let _e258 = texelSize[1u];
            let _e260 = (_e255.xy - vec2<f32>(0f, _e258));
            let _e261 = layer;
            let _e264 = vec3<f32>(_e260.x, _e260.y, _e261);
            let _e270 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e264.x, _e264.y), i32(_e264.z));
            let _e273 = shadow;
            shadow = (_e273 + step(_e254, _e270.x));
            let _e275 = shadow;
            shadow = (_e275 / 5f);
        } else {
            x = -1i;
            loop {
                let _e277 = x;
                if (_e277 <= 1i) {
                    y = -1i;
                    loop {
                        let _e279 = y;
                        if (_e279 <= 1i) {
                            let _e281 = currentDepth;
                            let _e282 = sc;
                            let _e284 = x;
                            let _e286 = y;
                            let _e289 = texelSize;
                            let _e291 = (_e282.xy + (vec2<f32>(f32(_e284), f32(_e286)) * _e289));
                            let _e292 = layer;
                            let _e295 = vec3<f32>(_e291.x, _e291.y, _e292);
                            let _e301 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e295.x, _e295.y), i32(_e295.z));
                            let _e304 = shadow;
                            shadow = (_e304 + step(_e281, _e301.x));
                            continue;
                        } else {
                            break;
                        }
                        continuing {
                            let _e306 = y;
                            y = (_e306 + 1i);
                        }
                    }
                    continue;
                } else {
                    break;
                }
                continuing {
                    let _e308 = x;
                    x = (_e308 + 1i);
                }
            }
            let _e310 = shadow;
            shadow = (_e310 / 9f);
        }
    }
    let _e312 = shadow;
    return _e312;
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

    let _e103 = unnamed.cascadeSplits;
    let _e104 = (*viewDepth_1);
    cmp = step(_e103, vec4(_e104));
    let _e108 = cmp[0u];
    let _e110 = cmp[1u];
    let _e113 = cmp[2u];
    let _e116 = cmp[3u];
    cascade = min(i32((((_e108 + _e110) + _e113) + _e116)), 3i);
    let _e120 = cascade;
    (*outCascade) = _e120;
    let _e121 = cascade;
    if (_e121 == 0i) {
        local = 0f;
    } else {
        let _e123 = cascade;
        let _e128 = unnamed.cascadeSplits[max((_e123 - 1i), 0i)];
        local = _e128;
    }
    let _e129 = local;
    prevSplit = _e129;
    let _e130 = cascade;
    let _e133 = unnamed.cascadeSplits[_e130];
    farSplit = _e133;
    let _e134 = farSplit;
    let _e135 = prevSplit;
    blendRange = max((0.1f * (_e134 - _e135)), 1f);
    let _e139 = farSplit;
    let _e140 = (*viewDepth_1);
    let _e142 = blendRange;
    blendT = clamp(((_e139 - _e140) / _e142), 0f, 1f);
    let _e145 = cascade;
    param = _e145;
    let _e146 = (*worldPos_1);
    param_1 = _e146;
    let _e147 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param), (&param_1));
    s0_ = _e147;
    let _e148 = cascade;
    param_2 = min((_e148 + 1i), 3i);
    let _e151 = (*worldPos_1);
    param_3 = _e151;
    let _e152 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param_2), (&param_3));
    s1_ = _e152;
    let _e153 = s1_;
    let _e154 = s0_;
    let _e155 = blendT;
    return mix(_e153, _e154, _e155);
}

fn wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b(rgb: ptr<function, vec3<f32>>, lm_uv: ptr<function, vec2<f32>>) -> vec3<f32> {
    var sun_mask: f32;
    var shadow_1: f32;
    var ignoredCascade: i32;
    var param_4: vec3<f32>;
    var param_5: f32;
    var param_6: i32;

    let _e97 = unnamed.packed_indices[1i][3u];
    let _e103 = unnamed.packed_indices[1i][3u];
    let _e108 = (*lm_uv);
    let _e109 = textureSample(wired_bindless_images[(_e97 & 4095u)], wired_bindless_samplers[((_e103 >> bitcast<u32>(12i)) & 255u)], _e108);
    sun_mask = _e109.x;
    let _e111 = sun_mask;
    if (_e111 > 0.001f) {
        let _e113 = shadowData_1;
        param_4 = _e113.xyz;
        let _e116 = shadowData_1[3u];
        param_5 = _e116;
        let _e117 = sampleShadow_u0028_vf3_u003b_f1_u003b_i1_u003b((&param_4), (&param_5), (&param_6));
        let _e118 = param_6;
        ignoredCascade = _e118;
        shadow_1 = _e117;
        let _e119 = shadow_1;
        let _e120 = sun_mask;
        let _e122 = (*rgb);
        (*rgb) = (_e122 * mix(1f, _e119, _e120));
    }
    let _e124 = (*rgb);
    return _e124;
}

fn wired_apply_sun_shadow_operand_u0028_vf3_u003b(rgb_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var param_7: vec3<f32>;
    var param_8: vec2<f32>;
    var param_9: vec3<f32>;
    var param_10: vec2<f32>;

    if override_type_3_2 {
        let _e91 = (*rgb_1);
        param_7 = _e91;
        let _e92 = frag_tex_coord0_1;
        param_8 = _e92;
        let _e93 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_7), (&param_8));
        return _e93;
    }
    if override_type_3_3 {
        let _e94 = (*rgb_1);
        param_9 = _e94;
        let _e95 = frag_tex_coord1_1;
        param_10 = _e95;
        let _e96 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_9), (&param_10));
        return _e96;
    }
    let _e97 = (*rgb_1);
    return _e97;
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb_2: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e89 = (*rgb_2);
    let _e92 = unnamed.worldLightParams[0u];
    boosted = (_e89 * _e92);
    let _e95 = boosted[0u];
    let _e97 = boosted[1u];
    let _e99 = boosted[2u];
    peak = max(_e95, max(_e97, _e99));
    let _e102 = peak;
    if (_e102 > 1f) {
        let _e104 = peak;
        let _e105 = boosted;
        boosted = (_e105 / vec3(_e104));
    }
    let _e108 = boosted;
    return _e108;
}

fn sRGBToLinear_u0028_vf3_u003b(c_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e90 = (*c_1);
    (*c_1) = max(_e90, vec3<f32>(0f, 0f, 0f));
    let _e92 = (*c_1);
    cutoff = (_e92 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e94 = (*c_1);
    lo = (_e94 / vec3(12.92f));
    let _e97 = (*c_1);
    hi = pow(((_e97 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e102 = hi;
    let _e103 = lo;
    let _e104 = cutoff;
    return mix(_e102, _e103, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e104));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_2: vec4<f32>;
    var param_11: vec3<f32>;
    var param_12: vec3<f32>;

    let _e92 = (*role);
    let _e94 = (*role);
    let _e99 = unnamed.packed_indices[(_e92 / 4u)][(_e94 % 4u)];
    let _e102 = (*role);
    let _e104 = (*role);
    let _e109 = unnamed.packed_indices[(_e102 / 4u)][(_e104 % 4u)];
    let _e114 = (*uv);
    let _e115 = textureSample(wired_bindless_images[(_e99 & 4095u)], wired_bindless_samplers[((_e109 >> bitcast<u32>(12i)) & 255u)], _e114);
    c_2 = _e115;
    let _e116 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e116))) == 0i) {
        let _e121 = c_2;
        param_11 = _e121.xyz;
        let _e123 = sRGBToLinear_u0028_vf3_u003b((&param_11));
        c_2[0u] = _e123.x;
        c_2[1u] = _e123.y;
        c_2[2u] = _e123.z;
    }
    let _e130 = (*slot);
    if (lightmap_slot == (_e130 + 1i)) {
        let _e133 = c_2;
        param_12 = _e133.xyz;
        let _e135 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_12));
        c_2[0u] = _e135.x;
        c_2[1u] = _e135.y;
        c_2[2u] = _e135.z;
    }
    let _e142 = c_2;
    return _e142;
}

fn main_1() {
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

    let _e112 = frag_color0In_1;
    param_13 = _e112.xyz;
    let _e114 = sRGBToLinear_u0028_vf3_u003b((&param_13));
    let _e116 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e114.x, _e114.y, _e114.z, _e116);
    let _e121 = frag_color1In_1;
    param_14 = _e121.xyz;
    let _e123 = sRGBToLinear_u0028_vf3_u003b((&param_14));
    let _e125 = frag_color1In_1[3u];
    frag_color1_ = vec4<f32>(_e123.x, _e123.y, _e123.z, _e125);
    param_15 = 0u;
    let _e130 = frag_tex_coord0_1;
    param_16 = _e130;
    param_17 = 0i;
    let _e131 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_15), (&param_16), (&param_17));
    sampledColor0_ = _e131;
    param_18 = 1u;
    let _e132 = frag_tex_coord1_1;
    param_19 = _e132;
    param_20 = 1i;
    let _e133 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_18), (&param_19), (&param_20));
    sampledColor1_ = _e133;
    let _e134 = sampledColor0_;
    let _e135 = frag_color0_;
    color0_ = (_e134 * _e135);
    if override_type_3_6 {
        let _e137 = sampledColor1_;
        let _e138 = frag_color1_;
        color1_ = (_e137 * _e138);
        let _e140 = color0_;
        let _e142 = color1_;
        let _e144 = (_e140.xyz + _e142.xyz);
        let _e146 = color0_[3u];
        let _e148 = color1_[3u];
        base = vec4<f32>(_e144.x, _e144.y, _e144.z, (_e146 * _e148));
    } else {
        if override_type_3_7 {
            let _e154 = sampledColor1_;
            let _e155 = frag_color1_;
            color1_1 = (_e154 * _e155);
            let _e158 = color0_[3u];
            let _e159 = color0_;
            color0_ = (_e159 * _e158);
            let _e162 = color1_1[3u];
            let _e163 = color1_1;
            color1_1 = (_e163 * _e162);
            let _e165 = color0_;
            let _e167 = color1_1;
            let _e169 = (_e165.xyz + _e167.xyz);
            let _e171 = color0_[3u];
            let _e173 = color1_1[3u];
            base = vec4<f32>(_e169.x, _e169.y, _e169.z, (_e171 * _e173));
        } else {
            if override_type_3_8 {
                let _e179 = sampledColor1_;
                let _e180 = frag_color1_;
                color1_2 = (_e179 * _e180);
                let _e183 = color0_[3u];
                let _e185 = color0_;
                color0_ = (_e185 * (1f - _e183));
                let _e188 = color1_2[3u];
                let _e190 = color1_2;
                color1_2 = (_e190 * (1f - _e188));
                let _e192 = color0_;
                let _e194 = color1_2;
                let _e196 = (_e192.xyz + _e194.xyz);
                let _e198 = color0_[3u];
                let _e200 = color1_2[3u];
                base = vec4<f32>(_e196.x, _e196.y, _e196.z, (_e198 * _e200));
            } else {
                if override_type_3_9 {
                    let _e206 = sampledColor1_;
                    let _e207 = frag_color1_;
                    color1_3 = (_e206 * _e207);
                    let _e209 = color0_;
                    let _e210 = color1_3;
                    let _e212 = color1_3[3u];
                    base = mix(_e209, _e210, vec4(_e212));
                } else {
                    if override_type_3_10 {
                        let _e215 = sampledColor1_;
                        let _e216 = frag_color1_;
                        color1_4 = (_e215 * _e216);
                        let _e218 = color1_4;
                        let _e219 = color0_;
                        let _e221 = color1_4[3u];
                        base = mix(_e218, _e219, vec4(_e221));
                    } else {
                        if override_type_3_11 {
                            let _e224 = sampledColor1_;
                            let _e225 = frag_color1_;
                            color1_5 = (_e224 * _e225);
                            let _e227 = color1_5;
                            let _e229 = color1_5[3u];
                            let _e232 = color0_;
                            base = ((_e227 + vec4(_e229)) * _e232);
                        } else {
                            let _e234 = sampledColor1_;
                            let _e235 = frag_color1_;
                            color1_6 = (_e234 * _e235);
                            let _e237 = color0_;
                            let _e239 = color1_6;
                            let _e241 = (_e237.xyz * _e239.xyz);
                            base[0u] = _e241.x;
                            base[1u] = _e241.y;
                            base[2u] = _e241.z;
                            let _e249 = color0_[3u];
                            let _e251 = color1_6[3u];
                            base[3u] = (_e249 * _e251);
                        }
                    }
                }
            }
        }
    }
    let _e254 = base;
    param_21 = _e254.xyz;
    let _e256 = wired_apply_sun_shadow_operand_u0028_vf3_u003b((&param_21));
    base[0u] = _e256.x;
    base[1u] = _e256.y;
    base[2u] = _e256.z;
    if override_type_3_12 {
        let _e265 = unnamed.worldLightParams[1u];
        wetness = clamp(_e265, 0f, 1f);
        let _e269 = unnamed.worldLightParams[2u];
        frost = clamp(_e269, 0f, 1f);
        let _e271 = base;
        luminance = dot(_e271.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e274 = wetness;
        let _e276 = base;
        let _e278 = (_e276.xyz * mix(1f, 0.82f, _e274));
        base[0u] = _e278.x;
        base[1u] = _e278.y;
        base[2u] = _e278.z;
        let _e285 = base;
        let _e287 = luminance;
        let _e289 = luminance;
        let _e291 = luminance;
        let _e293 = frost;
        let _e296 = mix(_e285.xyz, vec3<f32>((_e287 * 0.88f), (_e289 * 0.94f), _e291), vec3((_e293 * 0.55f)));
        base[0u] = _e296.x;
        base[1u] = _e296.y;
        base[2u] = _e296.z;
    }
    let _e305 = unnamed.materialFactors[0u];
    if (_e305 >= 0f) {
        let _e308 = unnamed.materialBaseColor;
        let _e309 = base;
        base = (_e309 * _e308);
    }
    let _e311 = color0_;
    let _e314 = unnamed.emissionRadiance;
    let _e317 = base;
    let _e319 = (_e317.xyz + (_e311.xyz * _e314.xyz));
    base[0u] = _e319.x;
    base[1u] = _e319.y;
    base[2u] = _e319.z;
    let _e326 = wired_advanced_fog_enabled_u0028_();
    if _e326 {
        let _e327 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e327;
        let _e328 = base;
        let _e331 = unnamed.advancedFogColorDensity;
        let _e333 = fogAmount;
        let _e335 = mix(_e328.xyz, _e331.xyz, vec3(_e333));
        base[0u] = _e335.x;
        base[1u] = _e335.y;
        base[2u] = _e335.z;
    }
    if override_type_3_13 {
        let _e343 = base[3u];
        if (_e343 == 0f) {
            discard;
        }
    } else {
        if override_type_3_14 {
            let _e345 = base;
            let _e347 = base;
            if (dot(_e345.xyz, _e347.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e351 = base;
    out_color = _e351;
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(7) shadowData: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(0) frag_color0In: vec4<f32>, @location(5) frag_color1In: vec4<f32>) -> @location(0) vec4<f32> {
    gl_FragCoord_1 = gl_FragCoord;
    shadowData_1 = shadowData;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    frag_color0In_1 = frag_color0In;
    frag_color1In_1 = frag_color1In;
    main_1();
    let _e13 = out_color;
    return _e13;
}
