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
override override_type_3_3: bool = (lightmap_slot != 0i);
@id(10) override acff: i32 = 0i;
override override_type_3_4: bool = (acff == 1i);
override override_type_3_5: bool = (acff == 2i);
override override_type_3_6: bool = (acff == 3i);
override override_type_3_7: bool = (acff == 1i);
override override_type_3_8: bool = (acff == 2i);
override override_type_3_9: bool = (acff == 3i);
@id(7) override discard_mode: i32 = 0i;
override override_type_3_10: bool = (discard_mode == 1i);
override override_type_3_11: bool = (discard_mode == 2i);
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
var<private> fog_tex_coord_1: vec2<f32>;
var<private> frag_color0In_1: vec4<f32>;
var<private> out_color: vec4<f32>;

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e83 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e83 + 0.5f));
    let _e88 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e90 = fogType;
    let _e93 = fogType;
    return (((_e88 > 0.5f) && (_e90 >= 1i)) && (_e93 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e83 = wired_advanced_fog_enabled_u0028_();
    if !(_e83) {
        return 0f;
    }
    let _e86 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e86, 0.000001f));
    let _e91 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e91 + 0.5f));
    let _e94 = fogType_1;
    if (_e94 == 1i) {
        let _e98 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e98 <= 0f) {
            return 0f;
        }
        let _e100 = viewDepth;
        let _e103 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e100 / _e103), 0f, 1f);
    }
    let _e108 = unnamed.advancedFogColorDensity[3u];
    let _e110 = viewDepth;
    opticalDepth = (max(_e108, 0f) * _e110);
    let _e112 = fogType_1;
    if (_e112 == 2i) {
        let _e114 = opticalDepth;
        return clamp((1f - exp(-(_e114))), 0f, 1f);
    }
    let _e119 = opticalDepth;
    let _e120 = opticalDepth;
    return clamp((1f - exp(-((_e119 * _e120)))), 0f, 1f);
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

    let _e90 = (*c);
    let _e93 = unnamed.cascadeMVP[_e90];
    let _e94 = (*worldPos);
    sc4_ = (_e93 * vec4<f32>(_e94.x, _e94.y, _e94.z, 1f));
    let _e100 = sc4_;
    let _e103 = sc4_[3u];
    sc = (_e100.xyz / vec3(_e103));
    let _e106 = sc;
    let _e110 = ((_e106.xy * 0.5f) + vec2(0.5f));
    sc[0u] = _e110.x;
    sc[1u] = _e110.y;
    let _e116 = sc[0u];
    let _e117 = (_e116 < 0f);
    phi_331_ = _e117;
    if !(_e117) {
        let _e120 = sc[0u];
        phi_331_ = (_e120 > 1f);
    }
    let _e123 = phi_331_;
    phi_338_ = _e123;
    if !(_e123) {
        let _e126 = sc[1u];
        phi_338_ = (_e126 < 0f);
    }
    let _e129 = phi_338_;
    phi_345_ = _e129;
    if !(_e129) {
        let _e132 = sc[1u];
        phi_345_ = (_e132 > 1f);
    }
    let _e135 = phi_345_;
    phi_352_ = _e135;
    if !(_e135) {
        let _e138 = sc[2u];
        phi_352_ = (_e138 > 1f);
    }
    let _e141 = phi_352_;
    if _e141 {
        return 1f;
    }
    let _e142 = (*c);
    layer = f32(_e142);
    let _e144 = textureDimensions(shadowMap, 0i);
    texelSize = (vec2(1f) / vec2<f32>(vec2<i32>(_e144).xy));
    let _e151 = sc[2u];
    currentDepth = (_e151 - shadow_bias);
    shadow = 0f;
    if override_type_3_ {
        let _e153 = currentDepth;
        let _e154 = sc;
        let _e155 = _e154.xy;
        let _e156 = layer;
        let _e159 = vec3<f32>(_e155.x, _e155.y, _e156);
        let _e165 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e159.x, _e159.y), i32(_e159.z));
        shadow = step(_e153, _e165.x);
    } else {
        if override_type_3_1 {
            let _e168 = currentDepth;
            let _e169 = sc;
            let _e170 = _e169.xy;
            let _e171 = layer;
            let _e174 = vec3<f32>(_e170.x, _e170.y, _e171);
            let _e180 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e174.x, _e174.y), i32(_e174.z));
            let _e183 = shadow;
            shadow = (_e183 + step(_e168, _e180.x));
            let _e185 = currentDepth;
            let _e186 = sc;
            let _e189 = texelSize[0u];
            let _e191 = (_e186.xy + vec2<f32>(_e189, 0f));
            let _e192 = layer;
            let _e195 = vec3<f32>(_e191.x, _e191.y, _e192);
            let _e201 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e195.x, _e195.y), i32(_e195.z));
            let _e204 = shadow;
            shadow = (_e204 + step(_e185, _e201.x));
            let _e206 = currentDepth;
            let _e207 = sc;
            let _e210 = texelSize[0u];
            let _e212 = (_e207.xy - vec2<f32>(_e210, 0f));
            let _e213 = layer;
            let _e216 = vec3<f32>(_e212.x, _e212.y, _e213);
            let _e222 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e216.x, _e216.y), i32(_e216.z));
            let _e225 = shadow;
            shadow = (_e225 + step(_e206, _e222.x));
            let _e227 = currentDepth;
            let _e228 = sc;
            let _e231 = texelSize[1u];
            let _e233 = (_e228.xy + vec2<f32>(0f, _e231));
            let _e234 = layer;
            let _e237 = vec3<f32>(_e233.x, _e233.y, _e234);
            let _e243 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e237.x, _e237.y), i32(_e237.z));
            let _e246 = shadow;
            shadow = (_e246 + step(_e227, _e243.x));
            let _e248 = currentDepth;
            let _e249 = sc;
            let _e252 = texelSize[1u];
            let _e254 = (_e249.xy - vec2<f32>(0f, _e252));
            let _e255 = layer;
            let _e258 = vec3<f32>(_e254.x, _e254.y, _e255);
            let _e264 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e258.x, _e258.y), i32(_e258.z));
            let _e267 = shadow;
            shadow = (_e267 + step(_e248, _e264.x));
            let _e269 = shadow;
            shadow = (_e269 / 5f);
        } else {
            x = -1i;
            loop {
                let _e271 = x;
                if (_e271 <= 1i) {
                    y = -1i;
                    loop {
                        let _e273 = y;
                        if (_e273 <= 1i) {
                            let _e275 = currentDepth;
                            let _e276 = sc;
                            let _e278 = x;
                            let _e280 = y;
                            let _e283 = texelSize;
                            let _e285 = (_e276.xy + (vec2<f32>(f32(_e278), f32(_e280)) * _e283));
                            let _e286 = layer;
                            let _e289 = vec3<f32>(_e285.x, _e285.y, _e286);
                            let _e295 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e289.x, _e289.y), i32(_e289.z));
                            let _e298 = shadow;
                            shadow = (_e298 + step(_e275, _e295.x));
                            continue;
                        } else {
                            break;
                        }
                        continuing {
                            let _e300 = y;
                            y = (_e300 + 1i);
                        }
                    }
                    continue;
                } else {
                    break;
                }
                continuing {
                    let _e302 = x;
                    x = (_e302 + 1i);
                }
            }
            let _e304 = shadow;
            shadow = (_e304 / 9f);
        }
    }
    let _e306 = shadow;
    return _e306;
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

    let _e97 = unnamed.cascadeSplits;
    let _e98 = (*viewDepth_1);
    cmp = step(_e97, vec4(_e98));
    let _e102 = cmp[0u];
    let _e104 = cmp[1u];
    let _e107 = cmp[2u];
    let _e110 = cmp[3u];
    cascade = min(i32((((_e102 + _e104) + _e107) + _e110)), 3i);
    let _e114 = cascade;
    (*outCascade) = _e114;
    let _e115 = cascade;
    if (_e115 == 0i) {
        local = 0f;
    } else {
        let _e117 = cascade;
        let _e122 = unnamed.cascadeSplits[max((_e117 - 1i), 0i)];
        local = _e122;
    }
    let _e123 = local;
    prevSplit = _e123;
    let _e124 = cascade;
    let _e127 = unnamed.cascadeSplits[_e124];
    farSplit = _e127;
    let _e128 = farSplit;
    let _e129 = prevSplit;
    blendRange = max((0.1f * (_e128 - _e129)), 1f);
    let _e133 = farSplit;
    let _e134 = (*viewDepth_1);
    let _e136 = blendRange;
    blendT = clamp(((_e133 - _e134) / _e136), 0f, 1f);
    let _e139 = cascade;
    param = _e139;
    let _e140 = (*worldPos_1);
    param_1 = _e140;
    let _e141 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param), (&param_1));
    s0_ = _e141;
    let _e142 = cascade;
    param_2 = min((_e142 + 1i), 3i);
    let _e145 = (*worldPos_1);
    param_3 = _e145;
    let _e146 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param_2), (&param_3));
    s1_ = _e146;
    let _e147 = s1_;
    let _e148 = s0_;
    let _e149 = blendT;
    return mix(_e147, _e148, _e149);
}

fn wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b(rgb: ptr<function, vec3<f32>>, lm_uv: ptr<function, vec2<f32>>) -> vec3<f32> {
    var sun_mask: f32;
    var shadow_1: f32;
    var ignoredCascade: i32;
    var param_4: vec3<f32>;
    var param_5: f32;
    var param_6: i32;

    let _e91 = unnamed.packed_indices[1i][3u];
    let _e97 = unnamed.packed_indices[1i][3u];
    let _e102 = (*lm_uv);
    let _e103 = textureSample(wired_bindless_images[(_e91 & 4095u)], wired_bindless_samplers[((_e97 >> bitcast<u32>(12i)) & 255u)], _e102);
    sun_mask = _e103.x;
    let _e105 = sun_mask;
    if (_e105 > 0.001f) {
        let _e107 = shadowData_1;
        param_4 = _e107.xyz;
        let _e110 = shadowData_1[3u];
        param_5 = _e110;
        let _e111 = sampleShadow_u0028_vf3_u003b_f1_u003b_i1_u003b((&param_4), (&param_5), (&param_6));
        let _e112 = param_6;
        ignoredCascade = _e112;
        shadow_1 = _e111;
        let _e113 = shadow_1;
        let _e114 = sun_mask;
        let _e116 = (*rgb);
        (*rgb) = (_e116 * mix(1f, _e113, _e114));
    }
    let _e118 = (*rgb);
    return _e118;
}

fn wired_apply_sun_shadow_operand_u0028_vf3_u003b(rgb_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var param_7: vec3<f32>;
    var param_8: vec2<f32>;

    if override_type_3_2 {
        let _e83 = (*rgb_1);
        param_7 = _e83;
        let _e84 = frag_tex_coord0_1;
        param_8 = _e84;
        let _e85 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_7), (&param_8));
        return _e85;
    }
    let _e86 = (*rgb_1);
    return _e86;
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb_2: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e83 = (*rgb_2);
    let _e86 = unnamed.worldLightParams[0u];
    boosted = (_e83 * _e86);
    let _e89 = boosted[0u];
    let _e91 = boosted[1u];
    let _e93 = boosted[2u];
    peak = max(_e89, max(_e91, _e93));
    let _e96 = peak;
    if (_e96 > 1f) {
        let _e98 = peak;
        let _e99 = boosted;
        boosted = (_e99 / vec3(_e98));
    }
    let _e102 = boosted;
    return _e102;
}

fn sRGBToLinear_u0028_vf3_u003b(c_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e84 = (*c_1);
    (*c_1) = max(_e84, vec3<f32>(0f, 0f, 0f));
    let _e86 = (*c_1);
    cutoff = (_e86 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e88 = (*c_1);
    lo = (_e88 / vec3(12.92f));
    let _e91 = (*c_1);
    hi = pow(((_e91 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e96 = hi;
    let _e97 = lo;
    let _e98 = cutoff;
    return mix(_e96, _e97, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e98));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_2: vec4<f32>;
    var param_9: vec3<f32>;
    var param_10: vec3<f32>;

    let _e86 = (*role);
    let _e88 = (*role);
    let _e93 = unnamed.packed_indices[(_e86 / 4u)][(_e88 % 4u)];
    let _e96 = (*role);
    let _e98 = (*role);
    let _e103 = unnamed.packed_indices[(_e96 / 4u)][(_e98 % 4u)];
    let _e108 = (*uv);
    let _e109 = textureSample(wired_bindless_images[(_e93 & 4095u)], wired_bindless_samplers[((_e103 >> bitcast<u32>(12i)) & 255u)], _e108);
    c_2 = _e109;
    let _e110 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e110))) == 0i) {
        let _e115 = c_2;
        param_9 = _e115.xyz;
        let _e117 = sRGBToLinear_u0028_vf3_u003b((&param_9));
        c_2[0u] = _e117.x;
        c_2[1u] = _e117.y;
        c_2[2u] = _e117.z;
    }
    let _e124 = (*slot);
    if (lightmap_slot == (_e124 + 1i)) {
        let _e127 = c_2;
        param_10 = _e127.xyz;
        let _e129 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_10));
        c_2[0u] = _e129.x;
        c_2[1u] = _e129.y;
        c_2[2u] = _e129.z;
    }
    let _e136 = c_2;
    return _e136;
}

fn main_1() {
    var fog: vec4<f32>;
    var frag_color0_: vec4<f32>;
    var param_11: vec3<f32>;
    var sampledColor0_: vec4<f32>;
    var param_12: u32;
    var param_13: vec2<f32>;
    var param_14: i32;
    var color0_: vec4<f32>;
    var base: vec4<f32>;
    var param_15: vec3<f32>;
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var fogAmount: f32;

    let _e97 = unnamed.packed_indices[0i][3u];
    let _e103 = unnamed.packed_indices[0i][3u];
    let _e108 = fog_tex_coord_1;
    let _e109 = textureSample(wired_bindless_images[(_e97 & 4095u)], wired_bindless_samplers[((_e103 >> bitcast<u32>(12i)) & 255u)], _e108);
    fog = _e109;
    let _e110 = frag_color0In_1;
    param_11 = _e110.xyz;
    let _e112 = sRGBToLinear_u0028_vf3_u003b((&param_11));
    let _e114 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e112.x, _e112.y, _e112.z, _e114);
    param_12 = 0u;
    let _e119 = frag_tex_coord0_1;
    param_13 = _e119;
    param_14 = 0i;
    let _e120 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_12), (&param_13), (&param_14));
    sampledColor0_ = _e120;
    let _e121 = sampledColor0_;
    let _e122 = frag_color0_;
    color0_ = (_e121 * _e122);
    let _e124 = color0_;
    base = _e124;
    let _e125 = color0_;
    base = _e125;
    let _e126 = base;
    param_15 = _e126.xyz;
    let _e128 = wired_apply_sun_shadow_operand_u0028_vf3_u003b((&param_15));
    base[0u] = _e128.x;
    base[1u] = _e128.y;
    base[2u] = _e128.z;
    if override_type_3_3 {
        let _e137 = unnamed.worldLightParams[1u];
        wetness = clamp(_e137, 0f, 1f);
        let _e141 = unnamed.worldLightParams[2u];
        frost = clamp(_e141, 0f, 1f);
        let _e143 = base;
        luminance = dot(_e143.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e146 = wetness;
        let _e148 = base;
        let _e150 = (_e148.xyz * mix(1f, 0.82f, _e146));
        base[0u] = _e150.x;
        base[1u] = _e150.y;
        base[2u] = _e150.z;
        let _e157 = base;
        let _e159 = luminance;
        let _e161 = luminance;
        let _e163 = luminance;
        let _e165 = frost;
        let _e168 = mix(_e157.xyz, vec3<f32>((_e159 * 0.88f), (_e161 * 0.94f), _e163), vec3((_e165 * 0.55f)));
        base[0u] = _e168.x;
        base[1u] = _e168.y;
        base[2u] = _e168.z;
    }
    let _e177 = unnamed.materialFactors[0u];
    if (_e177 >= 0f) {
        let _e180 = unnamed.materialBaseColor;
        let _e181 = base;
        base = (_e181 * _e180);
    }
    let _e183 = color0_;
    let _e186 = unnamed.emissionRadiance;
    let _e189 = base;
    let _e191 = (_e189.xyz + (_e183.xyz * _e186.xyz));
    base[0u] = _e191.x;
    base[1u] = _e191.y;
    base[2u] = _e191.z;
    let _e198 = wired_advanced_fog_enabled_u0028_();
    if _e198 {
        let _e199 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e199;
        if override_type_3_4 {
            let _e200 = fogAmount;
            let _e202 = base;
            let _e204 = (_e202.xyz * (1f - _e200));
            base[0u] = _e204.x;
            base[1u] = _e204.y;
            base[2u] = _e204.z;
        } else {
            if override_type_3_5 {
                let _e211 = fogAmount;
                let _e213 = base;
                base = (_e213 * (1f - _e211));
            } else {
                if override_type_3_6 {
                    let _e215 = fogAmount;
                    let _e218 = base[3u];
                    base[3u] = (_e218 * (1f - _e215));
                } else {
                    let _e221 = base;
                    let _e224 = unnamed.advancedFogColorDensity;
                    let _e226 = fogAmount;
                    let _e228 = mix(_e221.xyz, _e224.xyz, vec3(_e226));
                    base[0u] = _e228.x;
                    base[1u] = _e228.y;
                    base[2u] = _e228.z;
                }
            }
        }
    } else {
        if override_type_3_7 {
            let _e235 = base;
            let _e238 = fog[3u];
            let _e240 = (_e235.xyz * (1f - _e238));
            base[0u] = _e240.x;
            base[1u] = _e240.y;
            base[2u] = _e240.z;
        } else {
            if override_type_3_8 {
                let _e247 = base;
                let _e249 = fog[3u];
                base = (_e247 * (1f - _e249));
            } else {
                if override_type_3_9 {
                    let _e253 = base[3u];
                    let _e255 = fog[3u];
                    base[3u] = (_e253 * (1f - _e255));
                } else {
                    let _e259 = base;
                    let _e260 = fog;
                    let _e262 = unnamed.fogColor;
                    let _e265 = fog[3u];
                    base = mix(_e259, (_e260 * _e262), vec4(_e265));
                }
            }
        }
    }
    if override_type_3_10 {
        let _e269 = base[3u];
        if (_e269 == 0f) {
            discard;
        }
    } else {
        if override_type_3_11 {
            let _e271 = base;
            let _e273 = base;
            if (dot(_e271.xyz, _e273.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e277 = base;
    out_color = _e277;
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(7) shadowData: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(4) fog_tex_coord: vec2<f32>, @location(0) frag_color0In: vec4<f32>) -> @location(0) vec4<f32> {
    gl_FragCoord_1 = gl_FragCoord;
    shadowData_1 = shadowData;
    frag_tex_coord0_1 = frag_tex_coord0_;
    fog_tex_coord_1 = fog_tex_coord;
    frag_color0In_1 = frag_color0In;
    main_1();
    let _e11 = out_color;
    return _e11;
}
