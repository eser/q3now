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
var<private> frag_color0In_1: vec4<f32>;
var<private> out_color: vec4<f32>;

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e72 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e72 + 0.5f));
    let _e77 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e79 = fogType;
    let _e82 = fogType;
    return (((_e77 > 0.5f) && (_e79 >= 1i)) && (_e82 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e72 = wired_advanced_fog_enabled_u0028_();
    if !(_e72) {
        return 0f;
    }
    let _e75 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e75, 0.000001f));
    let _e80 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e80 + 0.5f));
    let _e83 = fogType_1;
    if (_e83 == 1i) {
        let _e87 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e87 <= 0f) {
            return 0f;
        }
        let _e89 = viewDepth;
        let _e92 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e89 / _e92), 0f, 1f);
    }
    let _e97 = unnamed.advancedFogColorDensity[3u];
    let _e99 = viewDepth;
    opticalDepth = (max(_e97, 0f) * _e99);
    let _e101 = fogType_1;
    if (_e101 == 2i) {
        let _e103 = opticalDepth;
        return clamp((1f - exp(-(_e103))), 0f, 1f);
    }
    let _e108 = opticalDepth;
    let _e109 = opticalDepth;
    return clamp((1f - exp(-((_e108 * _e109)))), 0f, 1f);
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

    let _e79 = (*c);
    let _e82 = unnamed.cascadeMVP[_e79];
    let _e83 = (*worldPos);
    sc4_ = (_e82 * vec4<f32>(_e83.x, _e83.y, _e83.z, 1f));
    let _e89 = sc4_;
    let _e92 = sc4_[3u];
    sc = (_e89.xyz / vec3(_e92));
    let _e95 = sc;
    let _e99 = ((_e95.xy * 0.5f) + vec2(0.5f));
    sc[0u] = _e99.x;
    sc[1u] = _e99.y;
    let _e105 = sc[0u];
    let _e106 = (_e105 < 0f);
    phi_331_ = _e106;
    if !(_e106) {
        let _e109 = sc[0u];
        phi_331_ = (_e109 > 1f);
    }
    let _e112 = phi_331_;
    phi_338_ = _e112;
    if !(_e112) {
        let _e115 = sc[1u];
        phi_338_ = (_e115 < 0f);
    }
    let _e118 = phi_338_;
    phi_345_ = _e118;
    if !(_e118) {
        let _e121 = sc[1u];
        phi_345_ = (_e121 > 1f);
    }
    let _e124 = phi_345_;
    phi_352_ = _e124;
    if !(_e124) {
        let _e127 = sc[2u];
        phi_352_ = (_e127 > 1f);
    }
    let _e130 = phi_352_;
    if _e130 {
        return 1f;
    }
    let _e131 = (*c);
    layer = f32(_e131);
    let _e133 = textureDimensions(shadowMap, 0i);
    texelSize = (vec2(1f) / vec2<f32>(vec2<i32>(_e133).xy));
    let _e140 = sc[2u];
    currentDepth = (_e140 - shadow_bias);
    shadow = 0f;
    if override_type_3_ {
        let _e142 = currentDepth;
        let _e143 = sc;
        let _e144 = _e143.xy;
        let _e145 = layer;
        let _e148 = vec3<f32>(_e144.x, _e144.y, _e145);
        let _e154 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e148.x, _e148.y), i32(_e148.z));
        shadow = step(_e142, _e154.x);
    } else {
        if override_type_3_1 {
            let _e157 = currentDepth;
            let _e158 = sc;
            let _e159 = _e158.xy;
            let _e160 = layer;
            let _e163 = vec3<f32>(_e159.x, _e159.y, _e160);
            let _e169 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e163.x, _e163.y), i32(_e163.z));
            let _e172 = shadow;
            shadow = (_e172 + step(_e157, _e169.x));
            let _e174 = currentDepth;
            let _e175 = sc;
            let _e178 = texelSize[0u];
            let _e180 = (_e175.xy + vec2<f32>(_e178, 0f));
            let _e181 = layer;
            let _e184 = vec3<f32>(_e180.x, _e180.y, _e181);
            let _e190 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e184.x, _e184.y), i32(_e184.z));
            let _e193 = shadow;
            shadow = (_e193 + step(_e174, _e190.x));
            let _e195 = currentDepth;
            let _e196 = sc;
            let _e199 = texelSize[0u];
            let _e201 = (_e196.xy - vec2<f32>(_e199, 0f));
            let _e202 = layer;
            let _e205 = vec3<f32>(_e201.x, _e201.y, _e202);
            let _e211 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e205.x, _e205.y), i32(_e205.z));
            let _e214 = shadow;
            shadow = (_e214 + step(_e195, _e211.x));
            let _e216 = currentDepth;
            let _e217 = sc;
            let _e220 = texelSize[1u];
            let _e222 = (_e217.xy + vec2<f32>(0f, _e220));
            let _e223 = layer;
            let _e226 = vec3<f32>(_e222.x, _e222.y, _e223);
            let _e232 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e226.x, _e226.y), i32(_e226.z));
            let _e235 = shadow;
            shadow = (_e235 + step(_e216, _e232.x));
            let _e237 = currentDepth;
            let _e238 = sc;
            let _e241 = texelSize[1u];
            let _e243 = (_e238.xy - vec2<f32>(0f, _e241));
            let _e244 = layer;
            let _e247 = vec3<f32>(_e243.x, _e243.y, _e244);
            let _e253 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e247.x, _e247.y), i32(_e247.z));
            let _e256 = shadow;
            shadow = (_e256 + step(_e237, _e253.x));
            let _e258 = shadow;
            shadow = (_e258 / 5f);
        } else {
            x = -1i;
            loop {
                let _e260 = x;
                if (_e260 <= 1i) {
                    y = -1i;
                    loop {
                        let _e262 = y;
                        if (_e262 <= 1i) {
                            let _e264 = currentDepth;
                            let _e265 = sc;
                            let _e267 = x;
                            let _e269 = y;
                            let _e272 = texelSize;
                            let _e274 = (_e265.xy + (vec2<f32>(f32(_e267), f32(_e269)) * _e272));
                            let _e275 = layer;
                            let _e278 = vec3<f32>(_e274.x, _e274.y, _e275);
                            let _e284 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e278.x, _e278.y), i32(_e278.z));
                            let _e287 = shadow;
                            shadow = (_e287 + step(_e264, _e284.x));
                            continue;
                        } else {
                            break;
                        }
                        continuing {
                            let _e289 = y;
                            y = (_e289 + 1i);
                        }
                    }
                    continue;
                } else {
                    break;
                }
                continuing {
                    let _e291 = x;
                    x = (_e291 + 1i);
                }
            }
            let _e293 = shadow;
            shadow = (_e293 / 9f);
        }
    }
    let _e295 = shadow;
    return _e295;
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

    let _e86 = unnamed.cascadeSplits;
    let _e87 = (*viewDepth_1);
    cmp = step(_e86, vec4(_e87));
    let _e91 = cmp[0u];
    let _e93 = cmp[1u];
    let _e96 = cmp[2u];
    let _e99 = cmp[3u];
    cascade = min(i32((((_e91 + _e93) + _e96) + _e99)), 3i);
    let _e103 = cascade;
    (*outCascade) = _e103;
    let _e104 = cascade;
    if (_e104 == 0i) {
        local = 0f;
    } else {
        let _e106 = cascade;
        let _e111 = unnamed.cascadeSplits[max((_e106 - 1i), 0i)];
        local = _e111;
    }
    let _e112 = local;
    prevSplit = _e112;
    let _e113 = cascade;
    let _e116 = unnamed.cascadeSplits[_e113];
    farSplit = _e116;
    let _e117 = farSplit;
    let _e118 = prevSplit;
    blendRange = max((0.1f * (_e117 - _e118)), 1f);
    let _e122 = farSplit;
    let _e123 = (*viewDepth_1);
    let _e125 = blendRange;
    blendT = clamp(((_e122 - _e123) / _e125), 0f, 1f);
    let _e128 = cascade;
    param = _e128;
    let _e129 = (*worldPos_1);
    param_1 = _e129;
    let _e130 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param), (&param_1));
    s0_ = _e130;
    let _e131 = cascade;
    param_2 = min((_e131 + 1i), 3i);
    let _e134 = (*worldPos_1);
    param_3 = _e134;
    let _e135 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param_2), (&param_3));
    s1_ = _e135;
    let _e136 = s1_;
    let _e137 = s0_;
    let _e138 = blendT;
    return mix(_e136, _e137, _e138);
}

fn wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b(rgb: ptr<function, vec3<f32>>, lm_uv: ptr<function, vec2<f32>>) -> vec3<f32> {
    var sun_mask: f32;
    var shadow_1: f32;
    var ignoredCascade: i32;
    var param_4: vec3<f32>;
    var param_5: f32;
    var param_6: i32;

    let _e80 = unnamed.packed_indices[1i][3u];
    let _e86 = unnamed.packed_indices[1i][3u];
    let _e91 = (*lm_uv);
    let _e92 = textureSample(wired_bindless_images[(_e80 & 4095u)], wired_bindless_samplers[((_e86 >> bitcast<u32>(12i)) & 255u)], _e91);
    sun_mask = _e92.x;
    let _e94 = sun_mask;
    if (_e94 > 0.001f) {
        let _e96 = shadowData_1;
        param_4 = _e96.xyz;
        let _e99 = shadowData_1[3u];
        param_5 = _e99;
        let _e100 = sampleShadow_u0028_vf3_u003b_f1_u003b_i1_u003b((&param_4), (&param_5), (&param_6));
        let _e101 = param_6;
        ignoredCascade = _e101;
        shadow_1 = _e100;
        let _e102 = shadow_1;
        let _e103 = sun_mask;
        let _e105 = (*rgb);
        (*rgb) = (_e105 * mix(1f, _e102, _e103));
    }
    let _e107 = (*rgb);
    return _e107;
}

fn wired_apply_sun_shadow_operand_u0028_vf3_u003b(rgb_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var param_7: vec3<f32>;
    var param_8: vec2<f32>;

    if override_type_3_2 {
        let _e72 = (*rgb_1);
        param_7 = _e72;
        let _e73 = frag_tex_coord0_1;
        param_8 = _e73;
        let _e74 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_7), (&param_8));
        return _e74;
    }
    let _e75 = (*rgb_1);
    return _e75;
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb_2: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e72 = (*rgb_2);
    let _e75 = unnamed.worldLightParams[0u];
    boosted = (_e72 * _e75);
    let _e78 = boosted[0u];
    let _e80 = boosted[1u];
    let _e82 = boosted[2u];
    peak = max(_e78, max(_e80, _e82));
    let _e85 = peak;
    if (_e85 > 1f) {
        let _e87 = peak;
        let _e88 = boosted;
        boosted = (_e88 / vec3(_e87));
    }
    let _e91 = boosted;
    return _e91;
}

fn sRGBToLinear_u0028_vf3_u003b(c_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e73 = (*c_1);
    (*c_1) = max(_e73, vec3<f32>(0f, 0f, 0f));
    let _e75 = (*c_1);
    cutoff = (_e75 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e77 = (*c_1);
    lo = (_e77 / vec3(12.92f));
    let _e80 = (*c_1);
    hi = pow(((_e80 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e85 = hi;
    let _e86 = lo;
    let _e87 = cutoff;
    return mix(_e85, _e86, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e87));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_2: vec4<f32>;
    var param_9: vec3<f32>;
    var param_10: vec3<f32>;

    let _e75 = (*role);
    let _e77 = (*role);
    let _e82 = unnamed.packed_indices[(_e75 / 4u)][(_e77 % 4u)];
    let _e85 = (*role);
    let _e87 = (*role);
    let _e92 = unnamed.packed_indices[(_e85 / 4u)][(_e87 % 4u)];
    let _e97 = (*uv);
    let _e98 = textureSample(wired_bindless_images[(_e82 & 4095u)], wired_bindless_samplers[((_e92 >> bitcast<u32>(12i)) & 255u)], _e97);
    c_2 = _e98;
    let _e99 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e99))) == 0i) {
        let _e104 = c_2;
        param_9 = _e104.xyz;
        let _e106 = sRGBToLinear_u0028_vf3_u003b((&param_9));
        c_2[0u] = _e106.x;
        c_2[1u] = _e106.y;
        c_2[2u] = _e106.z;
    }
    let _e113 = (*slot);
    if (lightmap_slot == (_e113 + 1i)) {
        let _e116 = c_2;
        param_10 = _e116.xyz;
        let _e118 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_10));
        c_2[0u] = _e118.x;
        c_2[1u] = _e118.y;
        c_2[2u] = _e118.z;
    }
    let _e125 = c_2;
    return _e125;
}

fn main_1() {
    var frag_color0_: vec4<f32>;
    var param_11: vec3<f32>;
    var color0_: vec4<f32>;
    var param_12: u32;
    var param_13: vec2<f32>;
    var param_14: i32;
    var base: vec4<f32>;
    var param_15: vec3<f32>;
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var fogAmount: f32;

    let _e81 = frag_color0In_1;
    param_11 = _e81.xyz;
    let _e83 = sRGBToLinear_u0028_vf3_u003b((&param_11));
    let _e85 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e83.x, _e83.y, _e83.z, _e85);
    param_12 = 0u;
    let _e90 = frag_tex_coord0_1;
    param_13 = _e90;
    param_14 = 0i;
    let _e91 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_12), (&param_13), (&param_14));
    let _e92 = frag_color0_;
    color0_ = (_e91 * _e92);
    let _e94 = color0_;
    base = _e94;
    let _e95 = color0_;
    base = _e95;
    let _e96 = base;
    param_15 = _e96.xyz;
    let _e98 = wired_apply_sun_shadow_operand_u0028_vf3_u003b((&param_15));
    base[0u] = _e98.x;
    base[1u] = _e98.y;
    base[2u] = _e98.z;
    if override_type_3_3 {
        let _e107 = unnamed.worldLightParams[1u];
        wetness = clamp(_e107, 0f, 1f);
        let _e111 = unnamed.worldLightParams[2u];
        frost = clamp(_e111, 0f, 1f);
        let _e113 = base;
        luminance = dot(_e113.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e116 = wetness;
        let _e118 = base;
        let _e120 = (_e118.xyz * mix(1f, 0.82f, _e116));
        base[0u] = _e120.x;
        base[1u] = _e120.y;
        base[2u] = _e120.z;
        let _e127 = base;
        let _e129 = luminance;
        let _e131 = luminance;
        let _e133 = luminance;
        let _e135 = frost;
        let _e138 = mix(_e127.xyz, vec3<f32>((_e129 * 0.88f), (_e131 * 0.94f), _e133), vec3((_e135 * 0.55f)));
        base[0u] = _e138.x;
        base[1u] = _e138.y;
        base[2u] = _e138.z;
    }
    let _e145 = color0_;
    let _e148 = unnamed.emissionRadiance;
    let _e151 = base;
    let _e153 = (_e151.xyz + (_e145.xyz * _e148.xyz));
    base[0u] = _e153.x;
    base[1u] = _e153.y;
    base[2u] = _e153.z;
    let _e160 = wired_advanced_fog_enabled_u0028_();
    if _e160 {
        let _e161 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e161;
        let _e162 = base;
        let _e165 = unnamed.advancedFogColorDensity;
        let _e167 = fogAmount;
        let _e169 = mix(_e162.xyz, _e165.xyz, vec3(_e167));
        base[0u] = _e169.x;
        base[1u] = _e169.y;
        base[2u] = _e169.z;
    }
    if override_type_3_4 {
        let _e177 = base[3u];
        if (_e177 == 0f) {
            discard;
        }
    } else {
        if override_type_3_5 {
            let _e179 = base;
            let _e181 = base;
            if (dot(_e179.xyz, _e181.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e185 = base;
    out_color = _e185;
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(7) shadowData: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(0) frag_color0In: vec4<f32>) -> @location(0) vec4<f32> {
    gl_FragCoord_1 = gl_FragCoord;
    shadowData_1 = shadowData;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_color0In_1 = frag_color0In;
    main_1();
    let _e9 = out_color;
    return _e9;
}
