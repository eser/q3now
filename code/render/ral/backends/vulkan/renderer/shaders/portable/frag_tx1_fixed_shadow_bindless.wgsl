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
@id(7) override discard_mode: i32 = 0i;
override override_type_3_7: bool = (discard_mode == 1i);
override override_type_3_8: bool = (discard_mode == 2i);
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
var<private> out_color: vec4<f32>;

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e80 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e80 + 0.5f));
    let _e85 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e87 = fogType;
    let _e90 = fogType;
    return (((_e85 > 0.5f) && (_e87 >= 1i)) && (_e90 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e80 = wired_advanced_fog_enabled_u0028_();
    if !(_e80) {
        return 0f;
    }
    let _e83 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e83, 0.000001f));
    let _e88 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e88 + 0.5f));
    let _e91 = fogType_1;
    if (_e91 == 1i) {
        let _e95 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e95 <= 0f) {
            return 0f;
        }
        let _e97 = viewDepth;
        let _e100 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e97 / _e100), 0f, 1f);
    }
    let _e105 = unnamed.advancedFogColorDensity[3u];
    let _e107 = viewDepth;
    opticalDepth = (max(_e105, 0f) * _e107);
    let _e109 = fogType_1;
    if (_e109 == 2i) {
        let _e111 = opticalDepth;
        return clamp((1f - exp(-(_e111))), 0f, 1f);
    }
    let _e116 = opticalDepth;
    let _e117 = opticalDepth;
    return clamp((1f - exp(-((_e116 * _e117)))), 0f, 1f);
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

    let _e87 = (*c);
    let _e90 = unnamed.cascadeMVP[_e87];
    let _e91 = (*worldPos);
    sc4_ = (_e90 * vec4<f32>(_e91.x, _e91.y, _e91.z, 1f));
    let _e97 = sc4_;
    let _e100 = sc4_[3u];
    sc = (_e97.xyz / vec3(_e100));
    let _e103 = sc;
    let _e107 = ((_e103.xy * 0.5f) + vec2(0.5f));
    sc[0u] = _e107.x;
    sc[1u] = _e107.y;
    let _e113 = sc[0u];
    let _e114 = (_e113 < 0f);
    phi_331_ = _e114;
    if !(_e114) {
        let _e117 = sc[0u];
        phi_331_ = (_e117 > 1f);
    }
    let _e120 = phi_331_;
    phi_338_ = _e120;
    if !(_e120) {
        let _e123 = sc[1u];
        phi_338_ = (_e123 < 0f);
    }
    let _e126 = phi_338_;
    phi_345_ = _e126;
    if !(_e126) {
        let _e129 = sc[1u];
        phi_345_ = (_e129 > 1f);
    }
    let _e132 = phi_345_;
    phi_352_ = _e132;
    if !(_e132) {
        let _e135 = sc[2u];
        phi_352_ = (_e135 > 1f);
    }
    let _e138 = phi_352_;
    if _e138 {
        return 1f;
    }
    let _e139 = (*c);
    layer = f32(_e139);
    let _e141 = textureDimensions(shadowMap, 0i);
    texelSize = (vec2(1f) / vec2<f32>(vec2<i32>(_e141).xy));
    let _e148 = sc[2u];
    currentDepth = (_e148 - shadow_bias);
    shadow = 0f;
    if override_type_3_ {
        let _e150 = currentDepth;
        let _e151 = sc;
        let _e152 = _e151.xy;
        let _e153 = layer;
        let _e156 = vec3<f32>(_e152.x, _e152.y, _e153);
        let _e162 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e156.x, _e156.y), i32(_e156.z));
        shadow = step(_e150, _e162.x);
    } else {
        if override_type_3_1 {
            let _e165 = currentDepth;
            let _e166 = sc;
            let _e167 = _e166.xy;
            let _e168 = layer;
            let _e171 = vec3<f32>(_e167.x, _e167.y, _e168);
            let _e177 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e171.x, _e171.y), i32(_e171.z));
            let _e180 = shadow;
            shadow = (_e180 + step(_e165, _e177.x));
            let _e182 = currentDepth;
            let _e183 = sc;
            let _e186 = texelSize[0u];
            let _e188 = (_e183.xy + vec2<f32>(_e186, 0f));
            let _e189 = layer;
            let _e192 = vec3<f32>(_e188.x, _e188.y, _e189);
            let _e198 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e192.x, _e192.y), i32(_e192.z));
            let _e201 = shadow;
            shadow = (_e201 + step(_e182, _e198.x));
            let _e203 = currentDepth;
            let _e204 = sc;
            let _e207 = texelSize[0u];
            let _e209 = (_e204.xy - vec2<f32>(_e207, 0f));
            let _e210 = layer;
            let _e213 = vec3<f32>(_e209.x, _e209.y, _e210);
            let _e219 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e213.x, _e213.y), i32(_e213.z));
            let _e222 = shadow;
            shadow = (_e222 + step(_e203, _e219.x));
            let _e224 = currentDepth;
            let _e225 = sc;
            let _e228 = texelSize[1u];
            let _e230 = (_e225.xy + vec2<f32>(0f, _e228));
            let _e231 = layer;
            let _e234 = vec3<f32>(_e230.x, _e230.y, _e231);
            let _e240 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e234.x, _e234.y), i32(_e234.z));
            let _e243 = shadow;
            shadow = (_e243 + step(_e224, _e240.x));
            let _e245 = currentDepth;
            let _e246 = sc;
            let _e249 = texelSize[1u];
            let _e251 = (_e246.xy - vec2<f32>(0f, _e249));
            let _e252 = layer;
            let _e255 = vec3<f32>(_e251.x, _e251.y, _e252);
            let _e261 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e255.x, _e255.y), i32(_e255.z));
            let _e264 = shadow;
            shadow = (_e264 + step(_e245, _e261.x));
            let _e266 = shadow;
            shadow = (_e266 / 5f);
        } else {
            x = -1i;
            loop {
                let _e268 = x;
                if (_e268 <= 1i) {
                    y = -1i;
                    loop {
                        let _e270 = y;
                        if (_e270 <= 1i) {
                            let _e272 = currentDepth;
                            let _e273 = sc;
                            let _e275 = x;
                            let _e277 = y;
                            let _e280 = texelSize;
                            let _e282 = (_e273.xy + (vec2<f32>(f32(_e275), f32(_e277)) * _e280));
                            let _e283 = layer;
                            let _e286 = vec3<f32>(_e282.x, _e282.y, _e283);
                            let _e292 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e286.x, _e286.y), i32(_e286.z));
                            let _e295 = shadow;
                            shadow = (_e295 + step(_e272, _e292.x));
                            continue;
                        } else {
                            break;
                        }
                        continuing {
                            let _e297 = y;
                            y = (_e297 + 1i);
                        }
                    }
                    continue;
                } else {
                    break;
                }
                continuing {
                    let _e299 = x;
                    x = (_e299 + 1i);
                }
            }
            let _e301 = shadow;
            shadow = (_e301 / 9f);
        }
    }
    let _e303 = shadow;
    return _e303;
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

    let _e94 = unnamed.cascadeSplits;
    let _e95 = (*viewDepth_1);
    cmp = step(_e94, vec4(_e95));
    let _e99 = cmp[0u];
    let _e101 = cmp[1u];
    let _e104 = cmp[2u];
    let _e107 = cmp[3u];
    cascade = min(i32((((_e99 + _e101) + _e104) + _e107)), 3i);
    let _e111 = cascade;
    (*outCascade) = _e111;
    let _e112 = cascade;
    if (_e112 == 0i) {
        local = 0f;
    } else {
        let _e114 = cascade;
        let _e119 = unnamed.cascadeSplits[max((_e114 - 1i), 0i)];
        local = _e119;
    }
    let _e120 = local;
    prevSplit = _e120;
    let _e121 = cascade;
    let _e124 = unnamed.cascadeSplits[_e121];
    farSplit = _e124;
    let _e125 = farSplit;
    let _e126 = prevSplit;
    blendRange = max((0.1f * (_e125 - _e126)), 1f);
    let _e130 = farSplit;
    let _e131 = (*viewDepth_1);
    let _e133 = blendRange;
    blendT = clamp(((_e130 - _e131) / _e133), 0f, 1f);
    let _e136 = cascade;
    param = _e136;
    let _e137 = (*worldPos_1);
    param_1 = _e137;
    let _e138 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param), (&param_1));
    s0_ = _e138;
    let _e139 = cascade;
    param_2 = min((_e139 + 1i), 3i);
    let _e142 = (*worldPos_1);
    param_3 = _e142;
    let _e143 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param_2), (&param_3));
    s1_ = _e143;
    let _e144 = s1_;
    let _e145 = s0_;
    let _e146 = blendT;
    return mix(_e144, _e145, _e146);
}

fn wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b(rgb: ptr<function, vec3<f32>>, lm_uv: ptr<function, vec2<f32>>) -> vec3<f32> {
    var sun_mask: f32;
    var shadow_1: f32;
    var ignoredCascade: i32;
    var param_4: vec3<f32>;
    var param_5: f32;
    var param_6: i32;

    let _e88 = unnamed.packed_indices[1i][3u];
    let _e94 = unnamed.packed_indices[1i][3u];
    let _e99 = (*lm_uv);
    let _e100 = textureSample(wired_bindless_images[(_e88 & 4095u)], wired_bindless_samplers[((_e94 >> bitcast<u32>(12i)) & 255u)], _e99);
    sun_mask = _e100.x;
    let _e102 = sun_mask;
    if (_e102 > 0.001f) {
        let _e104 = shadowData_1;
        param_4 = _e104.xyz;
        let _e107 = shadowData_1[3u];
        param_5 = _e107;
        let _e108 = sampleShadow_u0028_vf3_u003b_f1_u003b_i1_u003b((&param_4), (&param_5), (&param_6));
        let _e109 = param_6;
        ignoredCascade = _e109;
        shadow_1 = _e108;
        let _e110 = shadow_1;
        let _e111 = sun_mask;
        let _e113 = (*rgb);
        (*rgb) = (_e113 * mix(1f, _e110, _e111));
    }
    let _e115 = (*rgb);
    return _e115;
}

fn wired_apply_sun_shadow_operand_u0028_vf3_u003b(rgb_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var param_7: vec3<f32>;
    var param_8: vec2<f32>;
    var param_9: vec3<f32>;
    var param_10: vec2<f32>;

    if override_type_3_2 {
        let _e82 = (*rgb_1);
        param_7 = _e82;
        let _e83 = frag_tex_coord0_1;
        param_8 = _e83;
        let _e84 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_7), (&param_8));
        return _e84;
    }
    if override_type_3_3 {
        let _e85 = (*rgb_1);
        param_9 = _e85;
        let _e86 = frag_tex_coord1_1;
        param_10 = _e86;
        let _e87 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_9), (&param_10));
        return _e87;
    }
    let _e88 = (*rgb_1);
    return _e88;
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb_2: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e80 = (*rgb_2);
    let _e83 = unnamed.worldLightParams[0u];
    boosted = (_e80 * _e83);
    let _e86 = boosted[0u];
    let _e88 = boosted[1u];
    let _e90 = boosted[2u];
    peak = max(_e86, max(_e88, _e90));
    let _e93 = peak;
    if (_e93 > 1f) {
        let _e95 = peak;
        let _e96 = boosted;
        boosted = (_e96 / vec3(_e95));
    }
    let _e99 = boosted;
    return _e99;
}

fn sRGBToLinear_u0028_vf3_u003b(c_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e81 = (*c_1);
    (*c_1) = max(_e81, vec3<f32>(0f, 0f, 0f));
    let _e83 = (*c_1);
    cutoff = (_e83 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e85 = (*c_1);
    lo = (_e85 / vec3(12.92f));
    let _e88 = (*c_1);
    hi = pow(((_e88 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e93 = hi;
    let _e94 = lo;
    let _e95 = cutoff;
    return mix(_e93, _e94, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e95));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_2: vec4<f32>;
    var param_11: vec3<f32>;
    var param_12: vec3<f32>;

    let _e83 = (*role);
    let _e85 = (*role);
    let _e90 = unnamed.packed_indices[(_e83 / 4u)][(_e85 % 4u)];
    let _e93 = (*role);
    let _e95 = (*role);
    let _e100 = unnamed.packed_indices[(_e93 / 4u)][(_e95 % 4u)];
    let _e105 = (*uv);
    let _e106 = textureSample(wired_bindless_images[(_e90 & 4095u)], wired_bindless_samplers[((_e100 >> bitcast<u32>(12i)) & 255u)], _e105);
    c_2 = _e106;
    let _e107 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e107))) == 0i) {
        let _e112 = c_2;
        param_11 = _e112.xyz;
        let _e114 = sRGBToLinear_u0028_vf3_u003b((&param_11));
        c_2[0u] = _e114.x;
        c_2[1u] = _e114.y;
        c_2[2u] = _e114.z;
    }
    let _e121 = (*slot);
    if (lightmap_slot == (_e121 + 1i)) {
        let _e124 = c_2;
        param_12 = _e124.xyz;
        let _e126 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_12));
        c_2[0u] = _e126.x;
        c_2[1u] = _e126.y;
        c_2[2u] = _e126.z;
    }
    let _e133 = c_2;
    return _e133;
}

fn main_1() {
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

    param_13 = 0u;
    let _e96 = frag_tex_coord0_1;
    param_14 = _e96;
    param_15 = 0i;
    let _e97 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_13), (&param_14), (&param_15));
    sampledColor0_ = _e97;
    param_16 = 1u;
    let _e98 = frag_tex_coord1_1;
    param_17 = _e98;
    param_18 = 1i;
    let _e99 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_16), (&param_17), (&param_18));
    sampledColor1_ = _e99;
    frag_color[0u] = identity_color;
    frag_color[1u] = identity_color;
    frag_color[2u] = identity_color;
    frag_color[3u] = identity_alpha;
    let _e104 = sampledColor0_;
    let _e105 = frag_color;
    color0_ = (_e104 * _e105);
    if override_type_3_4 {
        let _e107 = sampledColor1_;
        color1_ = _e107;
        let _e108 = color0_;
        let _e110 = color1_;
        let _e112 = (_e108.xyz + _e110.xyz);
        let _e114 = color0_[3u];
        let _e116 = color1_[3u];
        base = vec4<f32>(_e112.x, _e112.y, _e112.z, (_e114 * _e116));
    } else {
        if override_type_3_5 {
            let _e122 = sampledColor1_;
            let _e123 = frag_color;
            color1_1 = (_e122 * _e123);
            let _e125 = color0_;
            let _e127 = color1_1;
            let _e129 = (_e125.xyz + _e127.xyz);
            let _e131 = color0_[3u];
            let _e133 = color1_1[3u];
            base = vec4<f32>(_e129.x, _e129.y, _e129.z, (_e131 * _e133));
        } else {
            let _e139 = sampledColor1_;
            let _e140 = frag_color;
            color1_2 = (_e139 * _e140);
            let _e142 = color0_;
            let _e144 = color1_2;
            let _e146 = (_e142.xyz * _e144.xyz);
            base[0u] = _e146.x;
            base[1u] = _e146.y;
            base[2u] = _e146.z;
            let _e154 = color0_[3u];
            let _e156 = color1_2[3u];
            base[3u] = (_e154 * _e156);
        }
    }
    let _e159 = base;
    param_19 = _e159.xyz;
    let _e161 = wired_apply_sun_shadow_operand_u0028_vf3_u003b((&param_19));
    base[0u] = _e161.x;
    base[1u] = _e161.y;
    base[2u] = _e161.z;
    if override_type_3_6 {
        let _e170 = unnamed.worldLightParams[1u];
        wetness = clamp(_e170, 0f, 1f);
        let _e174 = unnamed.worldLightParams[2u];
        frost = clamp(_e174, 0f, 1f);
        let _e176 = base;
        luminance = dot(_e176.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e179 = wetness;
        let _e181 = base;
        let _e183 = (_e181.xyz * mix(1f, 0.82f, _e179));
        base[0u] = _e183.x;
        base[1u] = _e183.y;
        base[2u] = _e183.z;
        let _e190 = base;
        let _e192 = luminance;
        let _e194 = luminance;
        let _e196 = luminance;
        let _e198 = frost;
        let _e201 = mix(_e190.xyz, vec3<f32>((_e192 * 0.88f), (_e194 * 0.94f), _e196), vec3((_e198 * 0.55f)));
        base[0u] = _e201.x;
        base[1u] = _e201.y;
        base[2u] = _e201.z;
    }
    let _e210 = unnamed.materialFactors[0u];
    if (_e210 >= 0f) {
        let _e213 = unnamed.materialBaseColor;
        let _e214 = base;
        base = (_e214 * _e213);
    }
    let _e216 = color0_;
    let _e219 = unnamed.emissionRadiance;
    let _e222 = base;
    let _e224 = (_e222.xyz + (_e216.xyz * _e219.xyz));
    base[0u] = _e224.x;
    base[1u] = _e224.y;
    base[2u] = _e224.z;
    let _e231 = wired_advanced_fog_enabled_u0028_();
    if _e231 {
        let _e232 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e232;
        let _e233 = base;
        let _e236 = unnamed.advancedFogColorDensity;
        let _e238 = fogAmount;
        let _e240 = mix(_e233.xyz, _e236.xyz, vec3(_e238));
        base[0u] = _e240.x;
        base[1u] = _e240.y;
        base[2u] = _e240.z;
    }
    if override_type_3_7 {
        let _e248 = base[3u];
        if (_e248 == 0f) {
            discard;
        }
    } else {
        if override_type_3_8 {
            let _e250 = base;
            let _e252 = base;
            if (dot(_e250.xyz, _e252.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e256 = base;
    out_color = _e256;
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(7) shadowData: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>) -> @location(0) vec4<f32> {
    gl_FragCoord_1 = gl_FragCoord;
    shadowData_1 = shadowData;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    main_1();
    let _e9 = out_color;
    return _e9;
}
