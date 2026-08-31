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

    let _e73 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e73 + 0.5f));
    let _e78 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e80 = fogType;
    let _e83 = fogType;
    return (((_e78 > 0.5f) && (_e80 >= 1i)) && (_e83 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e73 = wired_advanced_fog_enabled_u0028_();
    if !(_e73) {
        return 0f;
    }
    let _e76 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e76, 0.000001f));
    let _e81 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e81 + 0.5f));
    let _e84 = fogType_1;
    if (_e84 == 1i) {
        let _e88 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e88 <= 0f) {
            return 0f;
        }
        let _e90 = viewDepth;
        let _e93 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e90 / _e93), 0f, 1f);
    }
    let _e98 = unnamed.advancedFogColorDensity[3u];
    let _e100 = viewDepth;
    opticalDepth = (max(_e98, 0f) * _e100);
    let _e102 = fogType_1;
    if (_e102 == 2i) {
        let _e104 = opticalDepth;
        return clamp((1f - exp(-(_e104))), 0f, 1f);
    }
    let _e109 = opticalDepth;
    let _e110 = opticalDepth;
    return clamp((1f - exp(-((_e109 * _e110)))), 0f, 1f);
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

    let _e80 = (*c);
    let _e83 = unnamed.cascadeMVP[_e80];
    let _e84 = (*worldPos);
    sc4_ = (_e83 * vec4<f32>(_e84.x, _e84.y, _e84.z, 1f));
    let _e90 = sc4_;
    let _e93 = sc4_[3u];
    sc = (_e90.xyz / vec3(_e93));
    let _e96 = sc;
    let _e100 = ((_e96.xy * 0.5f) + vec2(0.5f));
    sc[0u] = _e100.x;
    sc[1u] = _e100.y;
    let _e106 = sc[0u];
    let _e107 = (_e106 < 0f);
    phi_331_ = _e107;
    if !(_e107) {
        let _e110 = sc[0u];
        phi_331_ = (_e110 > 1f);
    }
    let _e113 = phi_331_;
    phi_338_ = _e113;
    if !(_e113) {
        let _e116 = sc[1u];
        phi_338_ = (_e116 < 0f);
    }
    let _e119 = phi_338_;
    phi_345_ = _e119;
    if !(_e119) {
        let _e122 = sc[1u];
        phi_345_ = (_e122 > 1f);
    }
    let _e125 = phi_345_;
    phi_352_ = _e125;
    if !(_e125) {
        let _e128 = sc[2u];
        phi_352_ = (_e128 > 1f);
    }
    let _e131 = phi_352_;
    if _e131 {
        return 1f;
    }
    let _e132 = (*c);
    layer = f32(_e132);
    let _e134 = textureDimensions(shadowMap, 0i);
    texelSize = (vec2(1f) / vec2<f32>(vec2<i32>(_e134).xy));
    let _e141 = sc[2u];
    currentDepth = (_e141 - shadow_bias);
    shadow = 0f;
    if override_type_3_ {
        let _e143 = currentDepth;
        let _e144 = sc;
        let _e145 = _e144.xy;
        let _e146 = layer;
        let _e149 = vec3<f32>(_e145.x, _e145.y, _e146);
        let _e155 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e149.x, _e149.y), i32(_e149.z));
        shadow = step(_e143, _e155.x);
    } else {
        if override_type_3_1 {
            let _e158 = currentDepth;
            let _e159 = sc;
            let _e160 = _e159.xy;
            let _e161 = layer;
            let _e164 = vec3<f32>(_e160.x, _e160.y, _e161);
            let _e170 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e164.x, _e164.y), i32(_e164.z));
            let _e173 = shadow;
            shadow = (_e173 + step(_e158, _e170.x));
            let _e175 = currentDepth;
            let _e176 = sc;
            let _e179 = texelSize[0u];
            let _e181 = (_e176.xy + vec2<f32>(_e179, 0f));
            let _e182 = layer;
            let _e185 = vec3<f32>(_e181.x, _e181.y, _e182);
            let _e191 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e185.x, _e185.y), i32(_e185.z));
            let _e194 = shadow;
            shadow = (_e194 + step(_e175, _e191.x));
            let _e196 = currentDepth;
            let _e197 = sc;
            let _e200 = texelSize[0u];
            let _e202 = (_e197.xy - vec2<f32>(_e200, 0f));
            let _e203 = layer;
            let _e206 = vec3<f32>(_e202.x, _e202.y, _e203);
            let _e212 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e206.x, _e206.y), i32(_e206.z));
            let _e215 = shadow;
            shadow = (_e215 + step(_e196, _e212.x));
            let _e217 = currentDepth;
            let _e218 = sc;
            let _e221 = texelSize[1u];
            let _e223 = (_e218.xy + vec2<f32>(0f, _e221));
            let _e224 = layer;
            let _e227 = vec3<f32>(_e223.x, _e223.y, _e224);
            let _e233 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e227.x, _e227.y), i32(_e227.z));
            let _e236 = shadow;
            shadow = (_e236 + step(_e217, _e233.x));
            let _e238 = currentDepth;
            let _e239 = sc;
            let _e242 = texelSize[1u];
            let _e244 = (_e239.xy - vec2<f32>(0f, _e242));
            let _e245 = layer;
            let _e248 = vec3<f32>(_e244.x, _e244.y, _e245);
            let _e254 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e248.x, _e248.y), i32(_e248.z));
            let _e257 = shadow;
            shadow = (_e257 + step(_e238, _e254.x));
            let _e259 = shadow;
            shadow = (_e259 / 5f);
        } else {
            x = -1i;
            loop {
                let _e261 = x;
                if (_e261 <= 1i) {
                    y = -1i;
                    loop {
                        let _e263 = y;
                        if (_e263 <= 1i) {
                            let _e265 = currentDepth;
                            let _e266 = sc;
                            let _e268 = x;
                            let _e270 = y;
                            let _e273 = texelSize;
                            let _e275 = (_e266.xy + (vec2<f32>(f32(_e268), f32(_e270)) * _e273));
                            let _e276 = layer;
                            let _e279 = vec3<f32>(_e275.x, _e275.y, _e276);
                            let _e285 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e279.x, _e279.y), i32(_e279.z));
                            let _e288 = shadow;
                            shadow = (_e288 + step(_e265, _e285.x));
                            continue;
                        } else {
                            break;
                        }
                        continuing {
                            let _e290 = y;
                            y = (_e290 + 1i);
                        }
                    }
                    continue;
                } else {
                    break;
                }
                continuing {
                    let _e292 = x;
                    x = (_e292 + 1i);
                }
            }
            let _e294 = shadow;
            shadow = (_e294 / 9f);
        }
    }
    let _e296 = shadow;
    return _e296;
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

    let _e87 = unnamed.cascadeSplits;
    let _e88 = (*viewDepth_1);
    cmp = step(_e87, vec4(_e88));
    let _e92 = cmp[0u];
    let _e94 = cmp[1u];
    let _e97 = cmp[2u];
    let _e100 = cmp[3u];
    cascade = min(i32((((_e92 + _e94) + _e97) + _e100)), 3i);
    let _e104 = cascade;
    (*outCascade) = _e104;
    let _e105 = cascade;
    if (_e105 == 0i) {
        local = 0f;
    } else {
        let _e107 = cascade;
        let _e112 = unnamed.cascadeSplits[max((_e107 - 1i), 0i)];
        local = _e112;
    }
    let _e113 = local;
    prevSplit = _e113;
    let _e114 = cascade;
    let _e117 = unnamed.cascadeSplits[_e114];
    farSplit = _e117;
    let _e118 = farSplit;
    let _e119 = prevSplit;
    blendRange = max((0.1f * (_e118 - _e119)), 1f);
    let _e123 = farSplit;
    let _e124 = (*viewDepth_1);
    let _e126 = blendRange;
    blendT = clamp(((_e123 - _e124) / _e126), 0f, 1f);
    let _e129 = cascade;
    param = _e129;
    let _e130 = (*worldPos_1);
    param_1 = _e130;
    let _e131 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param), (&param_1));
    s0_ = _e131;
    let _e132 = cascade;
    param_2 = min((_e132 + 1i), 3i);
    let _e135 = (*worldPos_1);
    param_3 = _e135;
    let _e136 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param_2), (&param_3));
    s1_ = _e136;
    let _e137 = s1_;
    let _e138 = s0_;
    let _e139 = blendT;
    return mix(_e137, _e138, _e139);
}

fn wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b(rgb: ptr<function, vec3<f32>>, lm_uv: ptr<function, vec2<f32>>) -> vec3<f32> {
    var sun_mask: f32;
    var shadow_1: f32;
    var ignoredCascade: i32;
    var param_4: vec3<f32>;
    var param_5: f32;
    var param_6: i32;

    let _e81 = unnamed.packed_indices[1i][3u];
    let _e87 = unnamed.packed_indices[1i][3u];
    let _e92 = (*lm_uv);
    let _e93 = textureSample(wired_bindless_images[(_e81 & 4095u)], wired_bindless_samplers[((_e87 >> bitcast<u32>(12i)) & 255u)], _e92);
    sun_mask = _e93.x;
    let _e95 = sun_mask;
    if (_e95 > 0.001f) {
        let _e97 = shadowData_1;
        param_4 = _e97.xyz;
        let _e100 = shadowData_1[3u];
        param_5 = _e100;
        let _e101 = sampleShadow_u0028_vf3_u003b_f1_u003b_i1_u003b((&param_4), (&param_5), (&param_6));
        let _e102 = param_6;
        ignoredCascade = _e102;
        shadow_1 = _e101;
        let _e103 = shadow_1;
        let _e104 = sun_mask;
        let _e106 = (*rgb);
        (*rgb) = (_e106 * mix(1f, _e103, _e104));
    }
    let _e108 = (*rgb);
    return _e108;
}

fn wired_apply_sun_shadow_operand_u0028_vf3_u003b(rgb_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var param_7: vec3<f32>;
    var param_8: vec2<f32>;

    if override_type_3_2 {
        let _e73 = (*rgb_1);
        param_7 = _e73;
        let _e74 = frag_tex_coord0_1;
        param_8 = _e74;
        let _e75 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_7), (&param_8));
        return _e75;
    }
    let _e76 = (*rgb_1);
    return _e76;
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb_2: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e73 = (*rgb_2);
    let _e76 = unnamed.worldLightParams[0u];
    boosted = (_e73 * _e76);
    let _e79 = boosted[0u];
    let _e81 = boosted[1u];
    let _e83 = boosted[2u];
    peak = max(_e79, max(_e81, _e83));
    let _e86 = peak;
    if (_e86 > 1f) {
        let _e88 = peak;
        let _e89 = boosted;
        boosted = (_e89 / vec3(_e88));
    }
    let _e92 = boosted;
    return _e92;
}

fn sRGBToLinear_u0028_vf3_u003b(c_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e74 = (*c_1);
    (*c_1) = max(_e74, vec3<f32>(0f, 0f, 0f));
    let _e76 = (*c_1);
    cutoff = (_e76 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e78 = (*c_1);
    lo = (_e78 / vec3(12.92f));
    let _e81 = (*c_1);
    hi = pow(((_e81 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e86 = hi;
    let _e87 = lo;
    let _e88 = cutoff;
    return mix(_e86, _e87, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e88));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_2: vec4<f32>;
    var param_9: vec3<f32>;
    var param_10: vec3<f32>;

    let _e76 = (*role);
    let _e78 = (*role);
    let _e83 = unnamed.packed_indices[(_e76 / 4u)][(_e78 % 4u)];
    let _e86 = (*role);
    let _e88 = (*role);
    let _e93 = unnamed.packed_indices[(_e86 / 4u)][(_e88 % 4u)];
    let _e98 = (*uv);
    let _e99 = textureSample(wired_bindless_images[(_e83 & 4095u)], wired_bindless_samplers[((_e93 >> bitcast<u32>(12i)) & 255u)], _e98);
    c_2 = _e99;
    let _e100 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e100))) == 0i) {
        let _e105 = c_2;
        param_9 = _e105.xyz;
        let _e107 = sRGBToLinear_u0028_vf3_u003b((&param_9));
        c_2[0u] = _e107.x;
        c_2[1u] = _e107.y;
        c_2[2u] = _e107.z;
    }
    let _e114 = (*slot);
    if (lightmap_slot == (_e114 + 1i)) {
        let _e117 = c_2;
        param_10 = _e117.xyz;
        let _e119 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_10));
        c_2[0u] = _e119.x;
        c_2[1u] = _e119.y;
        c_2[2u] = _e119.z;
    }
    let _e126 = c_2;
    return _e126;
}

fn main_1() {
    var frag_color: vec4<f32>;
    var color0_: vec4<f32>;
    var param_11: u32;
    var param_12: vec2<f32>;
    var param_13: i32;
    var base: vec4<f32>;
    var param_14: vec3<f32>;
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var fogAmount: f32;

    frag_color[0u] = identity_color;
    frag_color[1u] = identity_color;
    frag_color[2u] = identity_color;
    frag_color[3u] = identity_alpha;
    param_11 = 0u;
    let _e85 = frag_tex_coord0_1;
    param_12 = _e85;
    param_13 = 0i;
    let _e86 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_11), (&param_12), (&param_13));
    let _e87 = frag_color;
    color0_ = (_e86 * _e87);
    let _e89 = color0_;
    base = _e89;
    let _e90 = color0_;
    base = _e90;
    let _e91 = base;
    param_14 = _e91.xyz;
    let _e93 = wired_apply_sun_shadow_operand_u0028_vf3_u003b((&param_14));
    base[0u] = _e93.x;
    base[1u] = _e93.y;
    base[2u] = _e93.z;
    if override_type_3_3 {
        let _e102 = unnamed.worldLightParams[1u];
        wetness = clamp(_e102, 0f, 1f);
        let _e106 = unnamed.worldLightParams[2u];
        frost = clamp(_e106, 0f, 1f);
        let _e108 = base;
        luminance = dot(_e108.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e111 = wetness;
        let _e113 = base;
        let _e115 = (_e113.xyz * mix(1f, 0.82f, _e111));
        base[0u] = _e115.x;
        base[1u] = _e115.y;
        base[2u] = _e115.z;
        let _e122 = base;
        let _e124 = luminance;
        let _e126 = luminance;
        let _e128 = luminance;
        let _e130 = frost;
        let _e133 = mix(_e122.xyz, vec3<f32>((_e124 * 0.88f), (_e126 * 0.94f), _e128), vec3((_e130 * 0.55f)));
        base[0u] = _e133.x;
        base[1u] = _e133.y;
        base[2u] = _e133.z;
    }
    let _e140 = color0_;
    let _e143 = unnamed.emissionRadiance;
    let _e146 = base;
    let _e148 = (_e146.xyz + (_e140.xyz * _e143.xyz));
    base[0u] = _e148.x;
    base[1u] = _e148.y;
    base[2u] = _e148.z;
    let _e155 = wired_advanced_fog_enabled_u0028_();
    if _e155 {
        let _e156 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e156;
        let _e157 = base;
        let _e160 = unnamed.advancedFogColorDensity;
        let _e162 = fogAmount;
        let _e164 = mix(_e157.xyz, _e160.xyz, vec3(_e162));
        base[0u] = _e164.x;
        base[1u] = _e164.y;
        base[2u] = _e164.z;
    }
    if override_type_3_4 {
        let _e172 = base[3u];
        if (_e172 == 0f) {
            discard;
        }
    } else {
        if override_type_3_5 {
            let _e174 = base;
            let _e176 = base;
            if (dot(_e174.xyz, _e176.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e180 = base;
    out_color = _e180;
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
