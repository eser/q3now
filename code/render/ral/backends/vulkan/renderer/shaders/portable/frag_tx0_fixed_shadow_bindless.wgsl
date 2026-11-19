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
@id(8) override identity_color: f32 = 1f;
@id(9) override identity_alpha: f32 = 1f;
override override_type_3_3: bool = (lightmap_slot != 0i);
@id(7) override discard_mode: i32 = 0i;
override override_type_3_4: bool = (discard_mode == 1i);
override override_type_3_5: bool = (discard_mode == 2i);
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
var<private> out_color: vec4<f32>;

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e75 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e75 + 0.5f));
    let _e80 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e82 = fogType;
    let _e85 = fogType;
    return (((_e80 > 0.5f) && (_e82 >= 1i)) && (_e85 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e75 = wired_advanced_fog_enabled_u0028_();
    if !(_e75) {
        return 0f;
    }
    let _e78 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e78, 0.000001f));
    let _e83 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e83 + 0.5f));
    let _e86 = fogType_1;
    if (_e86 == 1i) {
        let _e90 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e90 <= 0f) {
            return 0f;
        }
        let _e92 = viewDepth;
        let _e95 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e92 / _e95), 0f, 1f);
    }
    let _e100 = unnamed.advancedFogColorDensity[3u];
    let _e102 = viewDepth;
    opticalDepth = (max(_e100, 0f) * _e102);
    let _e104 = fogType_1;
    if (_e104 == 2i) {
        let _e106 = opticalDepth;
        return clamp((1f - exp(-(_e106))), 0f, 1f);
    }
    let _e111 = opticalDepth;
    let _e112 = opticalDepth;
    return clamp((1f - exp(-((_e111 * _e112)))), 0f, 1f);
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

    let _e82 = (*c);
    let _e85 = unnamed.cascadeMVP[_e82];
    let _e86 = (*worldPos);
    sc4_ = (_e85 * vec4<f32>(_e86.x, _e86.y, _e86.z, 1f));
    let _e92 = sc4_;
    let _e95 = sc4_[3u];
    sc = (_e92.xyz / vec3(_e95));
    let _e98 = sc;
    let _e102 = ((_e98.xy * 0.5f) + vec2(0.5f));
    sc[0u] = _e102.x;
    sc[1u] = _e102.y;
    let _e108 = sc[0u];
    let _e109 = (_e108 < 0f);
    phi_331_ = _e109;
    if !(_e109) {
        let _e112 = sc[0u];
        phi_331_ = (_e112 > 1f);
    }
    let _e115 = phi_331_;
    phi_338_ = _e115;
    if !(_e115) {
        let _e118 = sc[1u];
        phi_338_ = (_e118 < 0f);
    }
    let _e121 = phi_338_;
    phi_345_ = _e121;
    if !(_e121) {
        let _e124 = sc[1u];
        phi_345_ = (_e124 > 1f);
    }
    let _e127 = phi_345_;
    phi_352_ = _e127;
    if !(_e127) {
        let _e130 = sc[2u];
        phi_352_ = (_e130 > 1f);
    }
    let _e133 = phi_352_;
    if _e133 {
        return 1f;
    }
    let _e134 = (*c);
    layer = f32(_e134);
    let _e136 = textureDimensions(shadowMap, 0i);
    texelSize = (vec2(1f) / vec2<f32>(vec2<i32>(_e136).xy));
    let _e143 = sc[2u];
    currentDepth = (_e143 - shadow_bias);
    shadow = 0f;
    if override_type_3_ {
        let _e145 = currentDepth;
        let _e146 = sc;
        let _e147 = _e146.xy;
        let _e148 = layer;
        let _e151 = vec3<f32>(_e147.x, _e147.y, _e148);
        let _e157 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e151.x, _e151.y), i32(_e151.z));
        shadow = step(_e145, _e157.x);
    } else {
        if override_type_3_1 {
            let _e160 = currentDepth;
            let _e161 = sc;
            let _e162 = _e161.xy;
            let _e163 = layer;
            let _e166 = vec3<f32>(_e162.x, _e162.y, _e163);
            let _e172 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e166.x, _e166.y), i32(_e166.z));
            let _e175 = shadow;
            shadow = (_e175 + step(_e160, _e172.x));
            let _e177 = currentDepth;
            let _e178 = sc;
            let _e181 = texelSize[0u];
            let _e183 = (_e178.xy + vec2<f32>(_e181, 0f));
            let _e184 = layer;
            let _e187 = vec3<f32>(_e183.x, _e183.y, _e184);
            let _e193 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e187.x, _e187.y), i32(_e187.z));
            let _e196 = shadow;
            shadow = (_e196 + step(_e177, _e193.x));
            let _e198 = currentDepth;
            let _e199 = sc;
            let _e202 = texelSize[0u];
            let _e204 = (_e199.xy - vec2<f32>(_e202, 0f));
            let _e205 = layer;
            let _e208 = vec3<f32>(_e204.x, _e204.y, _e205);
            let _e214 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e208.x, _e208.y), i32(_e208.z));
            let _e217 = shadow;
            shadow = (_e217 + step(_e198, _e214.x));
            let _e219 = currentDepth;
            let _e220 = sc;
            let _e223 = texelSize[1u];
            let _e225 = (_e220.xy + vec2<f32>(0f, _e223));
            let _e226 = layer;
            let _e229 = vec3<f32>(_e225.x, _e225.y, _e226);
            let _e235 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e229.x, _e229.y), i32(_e229.z));
            let _e238 = shadow;
            shadow = (_e238 + step(_e219, _e235.x));
            let _e240 = currentDepth;
            let _e241 = sc;
            let _e244 = texelSize[1u];
            let _e246 = (_e241.xy - vec2<f32>(0f, _e244));
            let _e247 = layer;
            let _e250 = vec3<f32>(_e246.x, _e246.y, _e247);
            let _e256 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e250.x, _e250.y), i32(_e250.z));
            let _e259 = shadow;
            shadow = (_e259 + step(_e240, _e256.x));
            let _e261 = shadow;
            shadow = (_e261 / 5f);
        } else {
            x = -1i;
            loop {
                let _e263 = x;
                if (_e263 <= 1i) {
                    y = -1i;
                    loop {
                        let _e265 = y;
                        if (_e265 <= 1i) {
                            let _e267 = currentDepth;
                            let _e268 = sc;
                            let _e270 = x;
                            let _e272 = y;
                            let _e275 = texelSize;
                            let _e277 = (_e268.xy + (vec2<f32>(f32(_e270), f32(_e272)) * _e275));
                            let _e278 = layer;
                            let _e281 = vec3<f32>(_e277.x, _e277.y, _e278);
                            let _e287 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e281.x, _e281.y), i32(_e281.z));
                            let _e290 = shadow;
                            shadow = (_e290 + step(_e267, _e287.x));
                            continue;
                        } else {
                            break;
                        }
                        continuing {
                            let _e292 = y;
                            y = (_e292 + 1i);
                        }
                    }
                    continue;
                } else {
                    break;
                }
                continuing {
                    let _e294 = x;
                    x = (_e294 + 1i);
                }
            }
            let _e296 = shadow;
            shadow = (_e296 / 9f);
        }
    }
    let _e298 = shadow;
    return _e298;
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

    let _e89 = unnamed.cascadeSplits;
    let _e90 = (*viewDepth_1);
    cmp = step(_e89, vec4(_e90));
    let _e94 = cmp[0u];
    let _e96 = cmp[1u];
    let _e99 = cmp[2u];
    let _e102 = cmp[3u];
    cascade = min(i32((((_e94 + _e96) + _e99) + _e102)), 3i);
    let _e106 = cascade;
    (*outCascade) = _e106;
    let _e107 = cascade;
    if (_e107 == 0i) {
        local = 0f;
    } else {
        let _e109 = cascade;
        let _e114 = unnamed.cascadeSplits[max((_e109 - 1i), 0i)];
        local = _e114;
    }
    let _e115 = local;
    prevSplit = _e115;
    let _e116 = cascade;
    let _e119 = unnamed.cascadeSplits[_e116];
    farSplit = _e119;
    let _e120 = farSplit;
    let _e121 = prevSplit;
    blendRange = max((0.1f * (_e120 - _e121)), 1f);
    let _e125 = farSplit;
    let _e126 = (*viewDepth_1);
    let _e128 = blendRange;
    blendT = clamp(((_e125 - _e126) / _e128), 0f, 1f);
    let _e131 = cascade;
    param = _e131;
    let _e132 = (*worldPos_1);
    param_1 = _e132;
    let _e133 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param), (&param_1));
    s0_ = _e133;
    let _e134 = cascade;
    param_2 = min((_e134 + 1i), 3i);
    let _e137 = (*worldPos_1);
    param_3 = _e137;
    let _e138 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param_2), (&param_3));
    s1_ = _e138;
    let _e139 = s1_;
    let _e140 = s0_;
    let _e141 = blendT;
    return mix(_e139, _e140, _e141);
}

fn wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b(rgb: ptr<function, vec3<f32>>, lm_uv: ptr<function, vec2<f32>>) -> vec3<f32> {
    var sun_mask: f32;
    var shadow_1: f32;
    var ignoredCascade: i32;
    var param_4: vec3<f32>;
    var param_5: f32;
    var param_6: i32;

    let _e83 = unnamed.packed_indices[1i][3u];
    let _e89 = unnamed.packed_indices[1i][3u];
    let _e94 = (*lm_uv);
    let _e95 = textureSample(wired_bindless_images[(_e83 & 4095u)], wired_bindless_samplers[((_e89 >> bitcast<u32>(12i)) & 255u)], _e94);
    sun_mask = _e95.x;
    let _e97 = sun_mask;
    if (_e97 > 0.001f) {
        let _e99 = shadowData_1;
        param_4 = _e99.xyz;
        let _e102 = shadowData_1[3u];
        param_5 = _e102;
        let _e103 = sampleShadow_u0028_vf3_u003b_f1_u003b_i1_u003b((&param_4), (&param_5), (&param_6));
        let _e104 = param_6;
        ignoredCascade = _e104;
        shadow_1 = _e103;
        let _e105 = shadow_1;
        let _e106 = sun_mask;
        let _e108 = (*rgb);
        (*rgb) = (_e108 * mix(1f, _e105, _e106));
    }
    let _e110 = (*rgb);
    return _e110;
}

fn wired_apply_sun_shadow_operand_u0028_vf3_u003b(rgb_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var param_7: vec3<f32>;
    var param_8: vec2<f32>;

    if override_type_3_2 {
        let _e75 = (*rgb_1);
        param_7 = _e75;
        let _e76 = frag_tex_coord0_1;
        param_8 = _e76;
        let _e77 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_7), (&param_8));
        return _e77;
    }
    let _e78 = (*rgb_1);
    return _e78;
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb_2: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e75 = (*rgb_2);
    let _e78 = unnamed.worldLightParams[0u];
    boosted = (_e75 * _e78);
    let _e81 = boosted[0u];
    let _e83 = boosted[1u];
    let _e85 = boosted[2u];
    peak = max(_e81, max(_e83, _e85));
    let _e88 = peak;
    if (_e88 > 1f) {
        let _e90 = peak;
        let _e91 = boosted;
        boosted = (_e91 / vec3(_e90));
    }
    let _e94 = boosted;
    return _e94;
}

fn sRGBToLinear_u0028_vf3_u003b(c_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e76 = (*c_1);
    (*c_1) = max(_e76, vec3<f32>(0f, 0f, 0f));
    let _e78 = (*c_1);
    cutoff = (_e78 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e80 = (*c_1);
    lo = (_e80 / vec3(12.92f));
    let _e83 = (*c_1);
    hi = pow(((_e83 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e88 = hi;
    let _e89 = lo;
    let _e90 = cutoff;
    return mix(_e88, _e89, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e90));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_2: vec4<f32>;
    var param_9: vec3<f32>;
    var param_10: vec3<f32>;

    let _e78 = (*role);
    let _e80 = (*role);
    let _e85 = unnamed.packed_indices[(_e78 / 4u)][(_e80 % 4u)];
    let _e88 = (*role);
    let _e90 = (*role);
    let _e95 = unnamed.packed_indices[(_e88 / 4u)][(_e90 % 4u)];
    let _e100 = (*uv);
    let _e101 = textureSample(wired_bindless_images[(_e85 & 4095u)], wired_bindless_samplers[((_e95 >> bitcast<u32>(12i)) & 255u)], _e100);
    c_2 = _e101;
    let _e102 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e102))) == 0i) {
        let _e107 = c_2;
        param_9 = _e107.xyz;
        let _e109 = sRGBToLinear_u0028_vf3_u003b((&param_9));
        c_2[0u] = _e109.x;
        c_2[1u] = _e109.y;
        c_2[2u] = _e109.z;
    }
    let _e116 = (*slot);
    if (lightmap_slot == (_e116 + 1i)) {
        let _e119 = c_2;
        param_10 = _e119.xyz;
        let _e121 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_10));
        c_2[0u] = _e121.x;
        c_2[1u] = _e121.y;
        c_2[2u] = _e121.z;
    }
    let _e128 = c_2;
    return _e128;
}

fn main_1() {
    var sampledColor0_: vec4<f32>;
    var param_11: u32;
    var param_12: vec2<f32>;
    var param_13: i32;
    var frag_color: vec4<f32>;
    var color0_: vec4<f32>;
    var base: vec4<f32>;
    var param_14: vec3<f32>;
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var fogAmount: f32;

    param_11 = 0u;
    let _e84 = frag_tex_coord0_1;
    param_12 = _e84;
    param_13 = 0i;
    let _e85 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_11), (&param_12), (&param_13));
    sampledColor0_ = _e85;
    frag_color[0u] = identity_color;
    frag_color[1u] = identity_color;
    frag_color[2u] = identity_color;
    frag_color[3u] = identity_alpha;
    let _e90 = sampledColor0_;
    let _e91 = frag_color;
    color0_ = (_e90 * _e91);
    let _e93 = color0_;
    base = _e93;
    let _e94 = color0_;
    base = _e94;
    let _e95 = base;
    param_14 = _e95.xyz;
    let _e97 = wired_apply_sun_shadow_operand_u0028_vf3_u003b((&param_14));
    base[0u] = _e97.x;
    base[1u] = _e97.y;
    base[2u] = _e97.z;
    if override_type_3_3 {
        let _e106 = unnamed.worldLightParams[1u];
        wetness = clamp(_e106, 0f, 1f);
        let _e110 = unnamed.worldLightParams[2u];
        frost = clamp(_e110, 0f, 1f);
        let _e112 = base;
        luminance = dot(_e112.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e115 = wetness;
        let _e117 = base;
        let _e119 = (_e117.xyz * mix(1f, 0.82f, _e115));
        base[0u] = _e119.x;
        base[1u] = _e119.y;
        base[2u] = _e119.z;
        let _e126 = base;
        let _e128 = luminance;
        let _e130 = luminance;
        let _e132 = luminance;
        let _e134 = frost;
        let _e137 = mix(_e126.xyz, vec3<f32>((_e128 * 0.88f), (_e130 * 0.94f), _e132), vec3((_e134 * 0.55f)));
        base[0u] = _e137.x;
        base[1u] = _e137.y;
        base[2u] = _e137.z;
    }
    let _e146 = unnamed.materialFactors[0u];
    if (_e146 >= 0f) {
        let _e149 = unnamed.materialBaseColor;
        let _e150 = base;
        base = (_e150 * _e149);
    }
    let _e152 = color0_;
    let _e155 = unnamed.emissionRadiance;
    let _e158 = base;
    let _e160 = (_e158.xyz + (_e152.xyz * _e155.xyz));
    base[0u] = _e160.x;
    base[1u] = _e160.y;
    base[2u] = _e160.z;
    let _e167 = wired_advanced_fog_enabled_u0028_();
    if _e167 {
        let _e168 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e168;
        let _e169 = base;
        let _e172 = unnamed.advancedFogColorDensity;
        let _e174 = fogAmount;
        let _e176 = mix(_e169.xyz, _e172.xyz, vec3(_e174));
        base[0u] = _e176.x;
        base[1u] = _e176.y;
        base[2u] = _e176.z;
    }
    if override_type_3_4 {
        let _e184 = base[3u];
        if (_e184 == 0f) {
            discard;
        }
    } else {
        if override_type_3_5 {
            let _e186 = base;
            let _e188 = base;
            if (dot(_e186.xyz, _e188.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e192 = base;
    out_color = _e192;
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(7) shadowData: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>) -> @location(0) vec4<f32> {
    gl_FragCoord_1 = gl_FragCoord;
    shadowData_1 = shadowData;
    frag_tex_coord0_1 = frag_tex_coord0_;
    main_1();
    let _e7 = out_color;
    return _e7;
}
