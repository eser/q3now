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
@id(8) override identity_color: f32 = 1f;
@id(9) override identity_alpha: f32 = 1f;
@id(6) override tex_mode: i32 = 0i;
override override_type_3_4: bool = (tex_mode == 1i);
override override_type_3_5: bool = (tex_mode == 2i);
override override_type_3_6: bool = (lightmap_slot != 0i);
@id(10) override acff: i32 = 0i;
override override_type_3_7: bool = (acff == 1i);
override override_type_3_8: bool = (acff == 2i);
override override_type_3_9: bool = (acff == 3i);
override override_type_3_10: bool = (acff == 1i);
override override_type_3_11: bool = (acff == 2i);
override override_type_3_12: bool = (acff == 3i);
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
var<private> fog_tex_coord_1: vec2<f32>;
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
    var fog: vec4<f32>;
    var sampledColor0_: vec4<f32>;
    var param_13: u32;
    var param_14: vec2<f32>;
    var param_15: i32;
    var sampledColor1_: vec4<f32>;
    var param_16: u32;
    var param_17: vec2<f32>;
    var param_18: i32;
    var frag_color: vec4<f32>;
    var color0_: vec4<f32>;
    var color1_: vec4<f32>;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var color1_2: vec4<f32>;
    var param_19: vec3<f32>;
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var fogAmount: f32;

    let _e109 = unnamed.packed_indices[0i][3u];
    let _e115 = unnamed.packed_indices[0i][3u];
    let _e120 = fog_tex_coord_1;
    let _e121 = textureSample(wired_bindless_images[(_e109 & 4095u)], wired_bindless_samplers[((_e115 >> bitcast<u32>(12i)) & 255u)], _e120);
    fog = _e121;
    param_13 = 0u;
    let _e122 = frag_tex_coord0_1;
    param_14 = _e122;
    param_15 = 0i;
    let _e123 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_13), (&param_14), (&param_15));
    sampledColor0_ = _e123;
    param_16 = 1u;
    let _e124 = frag_tex_coord1_1;
    param_17 = _e124;
    param_18 = 1i;
    let _e125 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_16), (&param_17), (&param_18));
    sampledColor1_ = _e125;
    frag_color[0u] = identity_color;
    frag_color[1u] = identity_color;
    frag_color[2u] = identity_color;
    frag_color[3u] = identity_alpha;
    let _e130 = sampledColor0_;
    let _e131 = frag_color;
    color0_ = (_e130 * _e131);
    if override_type_3_4 {
        let _e133 = sampledColor1_;
        color1_ = _e133;
        let _e134 = color0_;
        let _e136 = color1_;
        let _e138 = (_e134.xyz + _e136.xyz);
        let _e140 = color0_[3u];
        let _e142 = color1_[3u];
        base = vec4<f32>(_e138.x, _e138.y, _e138.z, (_e140 * _e142));
    } else {
        if override_type_3_5 {
            let _e148 = sampledColor1_;
            let _e149 = frag_color;
            color1_1 = (_e148 * _e149);
            let _e151 = color0_;
            let _e153 = color1_1;
            let _e155 = (_e151.xyz + _e153.xyz);
            let _e157 = color0_[3u];
            let _e159 = color1_1[3u];
            base = vec4<f32>(_e155.x, _e155.y, _e155.z, (_e157 * _e159));
        } else {
            let _e165 = sampledColor1_;
            let _e166 = frag_color;
            color1_2 = (_e165 * _e166);
            let _e168 = color0_;
            let _e170 = color1_2;
            let _e172 = (_e168.xyz * _e170.xyz);
            base[0u] = _e172.x;
            base[1u] = _e172.y;
            base[2u] = _e172.z;
            let _e180 = color0_[3u];
            let _e182 = color1_2[3u];
            base[3u] = (_e180 * _e182);
        }
    }
    let _e185 = base;
    param_19 = _e185.xyz;
    let _e187 = wired_apply_sun_shadow_operand_u0028_vf3_u003b((&param_19));
    base[0u] = _e187.x;
    base[1u] = _e187.y;
    base[2u] = _e187.z;
    if override_type_3_6 {
        let _e196 = unnamed.worldLightParams[1u];
        wetness = clamp(_e196, 0f, 1f);
        let _e200 = unnamed.worldLightParams[2u];
        frost = clamp(_e200, 0f, 1f);
        let _e202 = base;
        luminance = dot(_e202.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e205 = wetness;
        let _e207 = base;
        let _e209 = (_e207.xyz * mix(1f, 0.82f, _e205));
        base[0u] = _e209.x;
        base[1u] = _e209.y;
        base[2u] = _e209.z;
        let _e216 = base;
        let _e218 = luminance;
        let _e220 = luminance;
        let _e222 = luminance;
        let _e224 = frost;
        let _e227 = mix(_e216.xyz, vec3<f32>((_e218 * 0.88f), (_e220 * 0.94f), _e222), vec3((_e224 * 0.55f)));
        base[0u] = _e227.x;
        base[1u] = _e227.y;
        base[2u] = _e227.z;
    }
    let _e236 = unnamed.materialFactors[0u];
    if (_e236 >= 0f) {
        let _e239 = unnamed.materialBaseColor;
        let _e240 = base;
        base = (_e240 * _e239);
    }
    let _e242 = color0_;
    let _e245 = unnamed.emissionRadiance;
    let _e248 = base;
    let _e250 = (_e248.xyz + (_e242.xyz * _e245.xyz));
    base[0u] = _e250.x;
    base[1u] = _e250.y;
    base[2u] = _e250.z;
    let _e257 = wired_advanced_fog_enabled_u0028_();
    if _e257 {
        let _e258 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e258;
        if override_type_3_7 {
            let _e259 = fogAmount;
            let _e261 = base;
            let _e263 = (_e261.xyz * (1f - _e259));
            base[0u] = _e263.x;
            base[1u] = _e263.y;
            base[2u] = _e263.z;
        } else {
            if override_type_3_8 {
                let _e270 = fogAmount;
                let _e272 = base;
                base = (_e272 * (1f - _e270));
            } else {
                if override_type_3_9 {
                    let _e274 = fogAmount;
                    let _e277 = base[3u];
                    base[3u] = (_e277 * (1f - _e274));
                } else {
                    let _e280 = base;
                    let _e283 = unnamed.advancedFogColorDensity;
                    let _e285 = fogAmount;
                    let _e287 = mix(_e280.xyz, _e283.xyz, vec3(_e285));
                    base[0u] = _e287.x;
                    base[1u] = _e287.y;
                    base[2u] = _e287.z;
                }
            }
        }
    } else {
        if override_type_3_10 {
            let _e294 = base;
            let _e297 = fog[3u];
            let _e299 = (_e294.xyz * (1f - _e297));
            base[0u] = _e299.x;
            base[1u] = _e299.y;
            base[2u] = _e299.z;
        } else {
            if override_type_3_11 {
                let _e306 = base;
                let _e308 = fog[3u];
                base = (_e306 * (1f - _e308));
            } else {
                if override_type_3_12 {
                    let _e312 = base[3u];
                    let _e314 = fog[3u];
                    base[3u] = (_e312 * (1f - _e314));
                } else {
                    let _e318 = base;
                    let _e319 = fog;
                    let _e321 = unnamed.fogColor;
                    let _e324 = fog[3u];
                    base = mix(_e318, (_e319 * _e321), vec4(_e324));
                }
            }
        }
    }
    if override_type_3_13 {
        let _e328 = base[3u];
        if (_e328 == 0f) {
            discard;
        }
    } else {
        if override_type_3_14 {
            let _e330 = base;
            let _e332 = base;
            if (dot(_e330.xyz, _e332.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e336 = base;
    out_color = _e336;
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(7) shadowData: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(4) fog_tex_coord: vec2<f32>) -> @location(0) vec4<f32> {
    gl_FragCoord_1 = gl_FragCoord;
    shadowData_1 = shadowData;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    fog_tex_coord_1 = fog_tex_coord;
    main_1();
    let _e11 = out_color;
    return _e11;
}
